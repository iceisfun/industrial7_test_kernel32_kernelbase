/* Pipes across processes: redirected stdio, inherited / duplicated pipe handles, named pipe peers in a child. */
#include "pmc.h"

static HANDLE open_nul(void) {
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    return CreateFileA("NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
}
/* inheritable pipe; the parent's end is made non-inheritable */
static BOOL mkpipe(HANDLE *parent_end, HANDLE *child_end, BOOL parent_reads) {
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE r, w;
    if (!U(CreatePipe)(&r, &w, &sa, 0)) return FALSE;
    if (parent_reads) { *parent_end = r; *child_end = w; } else { *parent_end = w; *child_end = r; }
    SetHandleInformation(*parent_end, HANDLE_FLAG_INHERIT, 0);
    return TRUE;
}
/* read until EOF or deadline; returns bytes (cap), *eof set when broken pipe seen */
static size_t read_all(HANDLE h, unsigned char *buf, size_t cap, DWORD ms, BOOL *eof) {
    size_t got = 0; DWORD t0 = GetTickCount(); *eof = FALSE;
    while (GetTickCount() - t0 < ms && got < cap) {
        DWORD av = 0;
        if (!PeekNamedPipe(h, NULL, 0, NULL, &av, NULL)) { *eof = (GetLastError() == ERROR_BROKEN_PIPE); break; }
        if (!av) { Sleep(5); continue; }
        DWORD n = 0;
        if (!ReadFile(h, buf + got, (DWORD)(cap - got < av ? cap - got : av), &n, NULL)) { *eof = (GetLastError() == ERROR_BROKEN_PIPE); break; }
        got += n;
    }
    return got;
}
static void *mymemmem(const void *h, size_t hl, const void *n, size_t nl) { for (size_t i = 0; i + nl <= hl; i++) if (!memcmp((const char *)h + i, n, nl)) return (void *)((const char *)h + i); return NULL; }
static DWORD wait_child(HANDLE p, DWORD ms) {
    DWORD code = 0xDEAD0001;
    if (WaitForSingleObject(p, ms) == WAIT_OBJECT_0) GetExitCodeProcess(p, &code); else TerminateProcess(p, 1);
    return code;
}

CHILD(pmc_upper_stdin) {
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE), out = GetStdHandle(STD_OUTPUT_HANDLE);
    char b[256]; DWORD n; int total = 0;
    while (ReadFile(in, b, sizeof b, &n, NULL) && n) {
        for (DWORD i = 0; i < n; i++) if (b[i] >= 'a' && b[i] <= 'z') b[i] -= 32;
        WriteFile(out, b, n, &n, NULL); total += n;
    }
    return total;
}
CHILD(pmc_out_err) {
    DWORD n;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), "OUT-DATA", 8, &n, NULL);
    WriteFile(GetStdHandle(STD_ERROR_HANDLE), "ERR-DATA", 8, &n, NULL);
    return 3;
}
CHILD(pmc_stdtypes) {
    int r = 10;
    if (GetFileType(GetStdHandle(STD_INPUT_HANDLE)) == FILE_TYPE_PIPE) r += 1;
    if (GetFileType(GetStdHandle(STD_OUTPUT_HANDLE)) == FILE_TYPE_PIPE) r += 2;
    if (GetFileType(GetStdHandle(STD_ERROR_HANDLE)) == FILE_TYPE_PIPE) r += 4;
    return r;
}
CHILD(pmc_flood) {
    size_t total = (size_t)strtoul(argv[1], NULL, 10); HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    unsigned char *b = malloc(32768); size_t pos = 0; DWORD n;
    while (pos < total) { size_t c = total - pos > 32768 ? 32768 : total - pos; pmc_fill(b, c, (DWORD)(pos >> 15)); if (!WriteFile(out, b, (DWORD)c, &n, NULL)) return 2; pos += c; }
    return 0;
}
CHILD(pmc_write_handle_arg) {
    HANDLE h = (HANDLE)(ULONG_PTR)_strtoui64(argv[1], NULL, 10); DWORD n;
    if (!WriteFile(h, "via-handle", 10, &n, NULL)) return 2;
    return 0;
}
CHILD(pmc_close_stdout_early) {
    CloseHandle(GetStdHandle(STD_OUTPUT_HANDLE));
    Sleep(500);
    return 0;
}
CHILD(pmc_dup_target) {
    /* stdin carries a decimal handle value duplicated into this process */
    char b[64] = {0}; DWORD n = 0;
    if (!ReadFile(GetStdHandle(STD_INPUT_HANDLE), b, sizeof b - 1, &n, NULL) || !n) return 2;
    HANDLE h = (HANDLE)(ULONG_PTR)_strtoui64(b, NULL, 10);
    if (!WriteFile(h, "dup-ok", 6, &n, NULL)) return 3;
    return 0;
}
CHILD(pmc_named_client) {
    const char *nm = argv[1]; HANDLE c = INVALID_HANDLE_VALUE; DWORD n; char b[64];
    for (int i = 0; i < 100; i++) {
        c = CreateFileA(nm, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (c != INVALID_HANDLE_VALUE) break;
        if (GetLastError() == ERROR_PIPE_BUSY) WaitNamedPipeA(nm, 500); else Sleep(50);
    }
    if (c == INVALID_HANDLE_VALUE) return 2;
    DWORD mode = PIPE_READMODE_MESSAGE; SetNamedPipeHandleState(c, &mode, NULL, NULL);
    ULONG spid = 0; GetNamedPipeServerProcessId(c, &spid);
    char msg[64]; int l = snprintf(msg, sizeof msg, "hello-from-%lu-srv%lu", GetCurrentProcessId(), (unsigned long)spid);
    if (!WriteFile(c, msg, l, &n, NULL)) return 3;
    if (!ReadFile(c, b, sizeof b, &n, NULL)) return 4;
    if (n != 5 || memcmp(b, "reply", 5)) return 5;
    CloseHandle(c);
    return 0;
}
CHILD(pmc_named_server_for_parent) {
    /* child is the server; parent connects as client and checks server pid */
    const char *nm = argv[1]; DWORD n; char b[32];
    HANDLE s = CreateNamedPipeA(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512, 0, NULL);
    if (s == INVALID_HANDLE_VALUE) return 2;
    HANDLE ev = OpenEventA(EVENT_MODIFY_STATE, FALSE, argv[2]);
    if (ev) { SetEvent(ev); CloseHandle(ev); }
    if (!ConnectNamedPipe(s, NULL) && GetLastError() != ERROR_PIPE_CONNECTED) return 3;
    if (!ReadFile(s, b, sizeof b, &n, NULL)) return 4;
    WriteFile(s, "ack", 3, &n, NULL);
    FlushFileBuffers(s);
    return 0;
}

TEST(pmc_child_stdio_roundtrip) {
    REQUIRE(U(CreatePipe));
    HANDLE inP, inC, outP, outC, errC = open_nul();
    CHECK(mkpipe(&inP, &inC, FALSE)); CHECK(mkpipe(&outP, &outC, TRUE));
    PROCESS_INFORMATION pi;
    HANDLE p = t_spawn_child("pmc_upper_stdin", "", 0, inC, outC, errC, TRUE, NULL, &pi);
    CHECK(p != NULL);
    CloseHandle(inC); CloseHandle(outC); CloseHandle(errC);
    if (!p) { CloseHandle(inP); CloseHandle(outP); return; }
    DWORD n; unsigned char buf[256]; BOOL eof;
    CHECK(WriteFile(inP, "hello world", 11, &n, NULL)); EXPECT_EQ(n, 11);
    CloseHandle(inP);
    size_t got = read_all(outP, buf, sizeof buf, 15000, &eof);
    EXPECT_EQ(got, 11); CHECK(!memcmp(buf, "HELLO WORLD", 11)); CHECK(eof);
    EXPECT_EQ(wait_child(p, 15000), 11);
    CloseHandle(pi.hThread); CloseHandle(p); CloseHandle(outP);
}
TEST(pmc_child_stdout_stderr_separate) {
    REQUIRE(U(CreatePipe));
    HANDLE oP, oC, eP, eC, inC = open_nul();
    CHECK(mkpipe(&oP, &oC, TRUE)); CHECK(mkpipe(&eP, &eC, TRUE));
    PROCESS_INFORMATION pi;
    HANDLE p = t_spawn_child("pmc_out_err", "", 0, inC, oC, eC, TRUE, NULL, &pi);
    CHECK(p != NULL);
    CloseHandle(inC); CloseHandle(oC); CloseHandle(eC);
    if (!p) { CloseHandle(oP); CloseHandle(eP); return; }
    unsigned char b1[64] = {0}, b2[64] = {0}; BOOL e1, e2;
    size_t g1 = read_all(oP, b1, 63, 15000, &e1), g2 = read_all(eP, b2, 63, 15000, &e2);
    EXPECT_EQ(g1, 8); EXPECT_EQ(g2, 8); CHECK(!memcmp(b1, "OUT-DATA", 8)); CHECK(!memcmp(b2, "ERR-DATA", 8));
    EXPECT_EQ(wait_child(p, 15000), 3);
    CloseHandle(pi.hThread); CloseHandle(p); CloseHandle(oP); CloseHandle(eP);
}
TEST(pmc_child_stderr_merged_into_stdout) {
    REQUIRE(U(CreatePipe));
    HANDLE oP, oC, inC = open_nul();
    CHECK(mkpipe(&oP, &oC, TRUE));
    PROCESS_INFORMATION pi;
    HANDLE p = t_spawn_child("pmc_out_err", "", 0, inC, oC, oC, TRUE, NULL, &pi);
    CHECK(p != NULL);
    CloseHandle(inC); CloseHandle(oC);
    if (!p) { CloseHandle(oP); return; }
    unsigned char b[64] = {0}; BOOL eof;
    size_t g = read_all(oP, b, 63, 15000, &eof);
    EXPECT_EQ(g, 16); CHECK(eof);
    CHECK(mymemmem(b, g, "OUT-DATA", 8) != NULL); CHECK(mymemmem(b, g, "ERR-DATA", 8) != NULL);
    wait_child(p, 15000);
    CloseHandle(pi.hThread); CloseHandle(p); CloseHandle(oP);
}
TEST(pmc_child_std_handles_are_pipes) {
    REQUIRE(U(CreatePipe));
    HANDLE iP, iC, oP, oC, eP, eC;
    CHECK(mkpipe(&iP, &iC, FALSE)); CHECK(mkpipe(&oP, &oC, TRUE)); CHECK(mkpipe(&eP, &eC, TRUE));
    PROCESS_INFORMATION pi;
    HANDLE p = t_spawn_child("pmc_stdtypes", "", 0, iC, oC, eC, TRUE, NULL, &pi);
    CHECK(p != NULL);
    CloseHandle(iC); CloseHandle(oC); CloseHandle(eC);
    if (p) { EXPECT_EQ(wait_child(p, 15000), 17); CloseHandle(pi.hThread); CloseHandle(p); }
    CloseHandle(iP); CloseHandle(oP); CloseHandle(eP);
}
TEST(pmc_child_mixed_std_types) {
    REQUIRE(U(CreatePipe));
    HANDLE iN = open_nul(), oP, oC, eN = open_nul();
    CHECK(mkpipe(&oP, &oC, TRUE));
    PROCESS_INFORMATION pi;
    HANDLE p = t_spawn_child("pmc_stdtypes", "", 0, iN, oC, eN, TRUE, NULL, &pi);
    CHECK(p != NULL);
    CloseHandle(iN); CloseHandle(oC); CloseHandle(eN);
    if (p) { EXPECT_EQ(wait_child(p, 15000), 12); CloseHandle(pi.hThread); CloseHandle(p); }
    CloseHandle(oP);
}
TEST(pmc_child_large_output_checksum) {
    REQUIRE(U(CreatePipe));
    HANDLE oP, oC, inC = open_nul(), eC = open_nul();
    CHECK(mkpipe(&oP, &oC, TRUE));
    PROCESS_INFORMATION pi;
    size_t total = 4u << 20; char arg[32]; snprintf(arg, sizeof arg, "%lu", (unsigned long)total);
    HANDLE p = t_spawn_child("pmc_flood", arg, 0, inC, oC, eC, TRUE, NULL, &pi);
    CHECK(p != NULL);
    CloseHandle(inC); CloseHandle(oC); CloseHandle(eC);
    if (!p) { CloseHandle(oP); return; }
    unsigned char *buf = malloc(total), *ref = malloc(total); BOOL eof;
    size_t got = read_all(oP, buf, total, 25000, &eof);
    for (size_t pos = 0; pos < total; pos += 32768) { size_t c = total - pos > 32768 ? 32768 : total - pos; pmc_fill(ref + pos, c, (DWORD)(pos >> 15)); }
    EXPECT_EQ(got, total);
    CHECK(got == total && pmc_sum(buf, got, 0) == pmc_sum(ref, total, 0));
    EXPECT_EQ(wait_child(p, 20000), 0);
    free(buf); free(ref);
    CloseHandle(pi.hThread); CloseHandle(p); CloseHandle(oP);
}
TEST(pmc_child_inherited_handle_by_value) {
    REQUIRE(U(CreatePipe));
    HANDLE rP, wC, inC = open_nul(), eC = open_nul(), oC = open_nul();
    CHECK(mkpipe(&rP, &wC, TRUE));
    PROCESS_INFORMATION pi; char arg[32]; snprintf(arg, sizeof arg, "%llu", (unsigned long long)(ULONG_PTR)wC);
    HANDLE p = t_spawn_child("pmc_write_handle_arg", arg, 0, inC, oC, eC, TRUE, NULL, &pi);
    CHECK(p != NULL);
    CloseHandle(inC); CloseHandle(oC); CloseHandle(eC); CloseHandle(wC);
    if (!p) { CloseHandle(rP); return; }
    unsigned char b[32] = {0}; BOOL eof;
    size_t g = read_all(rP, b, 31, 15000, &eof);
    EXPECT_EQ(g, 10); CHECK(!memcmp(b, "via-handle", 10)); CHECK(eof);
    EXPECT_EQ(wait_child(p, 15000), 0);
    CloseHandle(pi.hThread); CloseHandle(p); CloseHandle(rP);
}
TEST(pmc_child_eof_when_child_exits) {
    REQUIRE(U(CreatePipe));
    HANDLE oP, oC, inC = open_nul(), eC = open_nul();
    CHECK(mkpipe(&oP, &oC, TRUE));
    PROCESS_INFORMATION pi;
    HANDLE p = t_spawn_child("pmc_close_stdout_early", "", 0, inC, oC, eC, TRUE, NULL, &pi);
    CHECK(p != NULL);
    CloseHandle(inC); CloseHandle(oC); CloseHandle(eC);
    if (!p) { CloseHandle(oP); return; }
    unsigned char b[8]; BOOL eof;
    size_t g = read_all(oP, b, 8, 10000, &eof);
    EXPECT_EQ(g, 0); CHECK(eof);   /* EOF seen although the child is still alive (it closed its copy) */
    wait_child(p, 15000);
    CloseHandle(pi.hThread); CloseHandle(p); CloseHandle(oP);
}
TEST(pmc_child_write_after_child_exit) {
    REQUIRE(U(CreatePipe));
    HANDLE iP, iC, oC = open_nul(), eC = open_nul();
    CHECK(mkpipe(&iP, &iC, FALSE));
    PROCESS_INFORMATION pi;
    HANDLE p = t_spawn_child("pmc_stdtypes", "", 0, iC, oC, eC, TRUE, NULL, &pi);
    CHECK(p != NULL);
    CloseHandle(iC); CloseHandle(oC); CloseHandle(eC);
    if (!p) { CloseHandle(iP); return; }
    wait_child(p, 15000);
    DWORD n = 5;
    SET_ERR(0);
    CHECK(!WriteFile(iP, "x", 1, &n, NULL));
    CHECK_ERR_ONE_OF2(ERROR_BROKEN_PIPE, ERROR_NO_DATA);
    CloseHandle(pi.hThread); CloseHandle(p); CloseHandle(iP);
}
TEST(pmc_child_not_inherited_without_flag) {
    REQUIRE(U(CreatePipe));
    HANDLE rP, wP;
    CHECK(U(CreatePipe)(&rP, &wP, NULL, 0));   /* non-inheritable */
    HANDLE inC = open_nul(), eC = open_nul(), oC = open_nul();
    PROCESS_INFORMATION pi; char arg[32]; snprintf(arg, sizeof arg, "%llu", (unsigned long long)(ULONG_PTR)wP);
    HANDLE p = t_spawn_child("pmc_write_handle_arg", arg, 0, inC, oC, eC, TRUE, NULL, &pi);
    CHECK(p != NULL);
    CloseHandle(inC); CloseHandle(oC); CloseHandle(eC);
    if (p) { DWORD code = wait_child(p, 15000); CHECK(code != 0xDEAD0001); DWORD av = 7; CHECK(PeekNamedPipe(rP, NULL, 0, NULL, &av, NULL)); EXPECT_EQ(av, 0); CloseHandle(pi.hThread); CloseHandle(p); }
    CloseHandle(rP); CloseHandle(wP);
}
TEST(pmc_child_duplicate_handle_into_child) {
    REQUIRE(U(CreatePipe));
    HANDLE iP, iC, oC = open_nul(), eC = open_nul(), rP, wP;
    CHECK(mkpipe(&iP, &iC, FALSE));
    CHECK(U(CreatePipe)(&rP, &wP, NULL, 0));
    PROCESS_INFORMATION pi;
    HANDLE p = t_spawn_child("pmc_dup_target", "", 0, iC, oC, eC, TRUE, NULL, &pi);
    CHECK(p != NULL);
    CloseHandle(iC); CloseHandle(oC); CloseHandle(eC);
    if (!p) { CloseHandle(iP); CloseHandle(rP); CloseHandle(wP); return; }
    HANDLE remote = NULL;
    CHECK(DuplicateHandle(GetCurrentProcess(), wP, p, &remote, 0, FALSE, DUPLICATE_SAME_ACCESS));
    CloseHandle(wP);
    char s[32]; int l = snprintf(s, sizeof s, "%llu", (unsigned long long)(ULONG_PTR)remote); DWORD n;
    CHECK(WriteFile(iP, s, l, &n, NULL));
    unsigned char b[16] = {0}; BOOL eof;
    size_t g = read_all(rP, b, 15, 15000, &eof);
    EXPECT_EQ(g, 6); CHECK(!memcmp(b, "dup-ok", 6));
    EXPECT_EQ(wait_child(p, 15000), 0);
    CloseHandle(pi.hThread); CloseHandle(p); CloseHandle(iP); CloseHandle(rP);
}
TEST(pmc_child_named_pipe_client_pid) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(GetNamedPipeClientProcessId)); REQUIRE(U(GetNamedPipeClientSessionId));
    char nm[128]; pmc_pipename(nm, sizeof nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 1, 512, 512);
    CHECK(s != INVALID_HANDLE_VALUE);
    OVERLAPPED ov = {0}; ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    BOOL ok = ConnectNamedPipe(s, &ov);
    CHECK(!ok); DWORD e = GetLastError(); CHECK(e == ERROR_IO_PENDING || e == ERROR_PIPE_CONNECTED);
    PROCESS_INFORMATION pi; char extra[160]; snprintf(extra, sizeof extra, "\"%s\"", nm);
    HANDLE p = t_spawn_child("pmc_named_client", extra, 0, NULL, NULL, NULL, FALSE, NULL, &pi);
    CHECK(p != NULL);
    if (p) {
        CHECK(WaitForSingleObject(ov.hEvent, 20000) == WAIT_OBJECT_0 || e == ERROR_PIPE_CONNECTED);
        ULONG cpid = 0, csess = 0xffff; DWORD mysess = 0xfffe;
        CHECK(U(GetNamedPipeClientProcessId)(s, &cpid)); EXPECT_EQ(cpid, pi.dwProcessId);
        CHECK(cpid != GetCurrentProcessId());
        CHECK(U(GetNamedPipeClientSessionId)(s, &csess));
        ProcessIdToSessionId(GetCurrentProcessId(), &mysess); EXPECT_EQ(csess, mysess);
        char b[128] = {0}; DWORD n = 0;
        OVERLAPPED rv = {0}; rv.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
        BOOL r = ReadFile(s, b, 127, &n, &rv);
        if (!r && GetLastError() == ERROR_IO_PENDING) r = GetOverlappedResult(s, &rv, &n, TRUE);
        CHECK(r);
        char want[64]; snprintf(want, sizeof want, "hello-from-%lu-srv%lu", (unsigned long)pi.dwProcessId, GetCurrentProcessId());
        CHECK(n == strlen(want) && !memcmp(b, want, n));
        WriteFile(s, "reply", 5, &n, NULL);
        FlushFileBuffers(s);
        EXPECT_EQ(wait_child(p, 20000), 0);
        CloseHandle(rv.hEvent);
        CloseHandle(pi.hThread); CloseHandle(p);
    }
    CloseHandle(ov.hEvent); CloseHandle(s);
}
TEST(pmc_child_named_pipe_server_pid) {
    REQUIRE(U(GetNamedPipeServerProcessId)); REQUIRE(U(GetNamedPipeServerSessionId));
    char nm[128]; pmc_pipename(nm, sizeof nm);
    char evn[64]; snprintf(evn, sizeof evn, "k32t_pmc_ev_%lu", GetCurrentProcessId());
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, evn);
    PROCESS_INFORMATION pi; char extra[256]; snprintf(extra, sizeof extra, "\"%s\" %s", nm, evn);
    HANDLE p = t_spawn_child("pmc_named_server_for_parent", extra, 0, NULL, NULL, NULL, FALSE, NULL, &pi);
    CHECK(p != NULL);
    if (p) {
        CHECK(WaitForSingleObject(ev, 20000) == WAIT_OBJECT_0);
        HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
        CHECK(c != INVALID_HANDLE_VALUE);
        if (c != INVALID_HANDLE_VALUE) {
            ULONG spid = 0, ssess = 0xffff; DWORD mysess = 0xfffe, n; char b[8];
            CHECK(U(GetNamedPipeServerProcessId)(c, &spid)); EXPECT_EQ(spid, pi.dwProcessId);
            CHECK(U(GetNamedPipeServerSessionId)(c, &ssess));
            ProcessIdToSessionId(GetCurrentProcessId(), &mysess); EXPECT_EQ(ssess, mysess);
            CHECK(WriteFile(c, "hi", 2, &n, NULL));
            CHECK(ReadFile(c, b, 8, &n, NULL)); EXPECT_EQ(n, 3);
            CloseHandle(c);
        }
        wait_child(p, 20000);
        CloseHandle(pi.hThread); CloseHandle(p);
    }
    CloseHandle(ev);
}
TEST(pmc_child_wait_named_pipe_cross_process) {
    REQUIRE(U(WaitNamedPipeA));
    char nm[128]; pmc_pipename(nm, sizeof nm);
    char evn[64]; snprintf(evn, sizeof evn, "k32t_pmc_ev2_%lu", GetCurrentProcessId());
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, evn);
    PROCESS_INFORMATION pi; char extra[256]; snprintf(extra, sizeof extra, "\"%s\" %s", nm, evn);
    HANDLE p = t_spawn_child("pmc_named_server_for_parent", extra, 0, NULL, NULL, NULL, FALSE, NULL, &pi);
    CHECK(p != NULL);
    if (p) {
        CHECK(WaitForSingleObject(ev, 20000) == WAIT_OBJECT_0);
        CHECK(U(WaitNamedPipeA)(nm, 5000));
        HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
        CHECK(c != INVALID_HANDLE_VALUE);
        if (c != INVALID_HANDLE_VALUE) {
            DWORD n; char b[8];
            /* instance is now connected and the single slot is taken: waiting must time out */
            SET_ERR(0); CHECK(!U(WaitNamedPipeA)(nm, 100)); EXPECT_ERR(ERROR_SEM_TIMEOUT);
            WriteFile(c, "x", 1, &n, NULL); ReadFile(c, b, 8, &n, NULL);
            CloseHandle(c);
        }
        wait_child(p, 20000);
        CloseHandle(pi.hThread); CloseHandle(p);
    }
    CloseHandle(ev);
}
TEST(pmc_child_server_gone_after_child_exit) {
    REQUIRE(U(WaitNamedPipeA));
    char nm[128]; pmc_pipename(nm, sizeof nm);
    char evn[64]; snprintf(evn, sizeof evn, "k32t_pmc_ev3_%lu", GetCurrentProcessId());
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, evn);
    PROCESS_INFORMATION pi; char extra[256]; snprintf(extra, sizeof extra, "\"%s\" %s", nm, evn);
    HANDLE p = t_spawn_child("pmc_named_server_for_parent", extra, 0, NULL, NULL, NULL, FALSE, NULL, &pi);
    CHECK(p != NULL);
    if (p) {
        CHECK(WaitForSingleObject(ev, 20000) == WAIT_OBJECT_0);
        HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
        if (c != INVALID_HANDLE_VALUE) { DWORD n; char b[8]; WriteFile(c, "x", 1, &n, NULL); ReadFile(c, b, 8, &n, NULL); CloseHandle(c); }
        wait_child(p, 20000);
        CloseHandle(pi.hThread); CloseHandle(p);
        SET_ERR(0); CHECK(!U(WaitNamedPipeA)(nm, 10)); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
        SET_ERR(0); HANDLE c2 = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
        CHECK(c2 == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    }
    CloseHandle(ev);
}
