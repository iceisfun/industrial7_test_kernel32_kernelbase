#include "g18.h"
/* gap18: CreateConsoleScreenBuffer (child processes only) and the Comm* family on non-comm handles.
 * No real serial port is ever opened or reconfigured. */

/* ------------------------- CreateConsoleScreenBuffer ------------------------- */
static DWORD g18_run(const char *name, DWORD flags) {
    PROCESS_INFORMATION pi; HANDLE h = t_spawn_child(name, "", flags, NULL, NULL, NULL, FALSE, NULL, &pi);
    if (!h) return 0xDEAD0000;
    DWORD c = g18_wait_exit(h, 30000);
    CloseHandle(pi.hThread); CloseHandle(h);
    return c;
}

CHILD(gap18_ccsb_invalid) {
    CK(U(CreateConsoleScreenBuffer));
    SECURITY_ATTRIBUTES sa; HANDLE h;
    /* nLength must be sizeof(SECURITY_ATTRIBUTES); rejected before the console server is contacted */
    static const DWORD lens[] = { 0, 1, 8, 16, 23, 25, 32, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof lens / sizeof lens[0]; i++) {
        memset(&sa, 0, sizeof sa); sa.nLength = lens[i];
        SetLastError(0);
        h = U(CreateConsoleScreenBuffer)(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CONSOLE_TEXTMODE_BUFFER, NULL);
        CK(h == INVALID_HANDLE_VALUE);
        CK(GetLastError() == ERROR_INVALID_PARAMETER);
    }
    static const DWORD badflags[] = { 0, 4, 0x80, 0x100, 0x80000000, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof badflags / sizeof badflags[0]; i++) {
        SetLastError(0);
        h = U(CreateConsoleScreenBuffer)(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, badflags[i], NULL);
        CK(h == INVALID_HANDLE_VALUE);
        CK(GetLastError() != 0);
    }
    return 0;
}
TEST(gap18_ccsb_invalid_params) {
    REQUIRE(U(CreateConsoleScreenBuffer));
    if (t_is_wine()) { t_skip("console children skipped under wine"); return; }
    EQ(g18_run("gap18_ccsb_invalid", CREATE_NEW_CONSOLE), 0);
}

CHILD(gap18_ccsb_basic) {
    CK(U(CreateConsoleScreenBuffer));
    HANDLE orig = CreateFileA("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CK(orig != INVALID_HANDLE_VALUE);
    SetLastError(0);
    HANDLE b = U(CreateConsoleScreenBuffer)(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    CK(b != INVALID_HANDLE_VALUE && b != NULL);
    CK(b != orig);
    CK(GetFileType(b) == FILE_TYPE_CHAR);
    DWORD mode = 0; CK(GetConsoleMode(b, &mode));
    CONSOLE_SCREEN_BUFFER_INFO i0, i1;
    CK(GetConsoleScreenBufferInfo(orig, &i0));
    CK(GetConsoleScreenBufferInfo(b, &i1));
    CK(i1.dwSize.X > 0 && i1.dwSize.Y > 0);
    CK(i1.dwCursorPosition.X == 0 && i1.dwCursorPosition.Y == 0);
    CK(i1.srWindow.Right - i1.srWindow.Left + 1 <= i1.dwSize.X);
    /* writes to the new buffer do not show up in the active one */
    DWORD n = 0; COORD z = { 0, 0 };
    CK(WriteConsoleOutputCharacterA(b, "NEWBUF", 6, z, &n) && n == 6);
    char got[8] = {0}; CK(ReadConsoleOutputCharacterA(b, got, 6, z, &n) && n == 6 && !memcmp(got, "NEWBUF", 6));
    CK(WriteConsoleOutputCharacterA(orig, "OLDBUF", 6, z, &n) && n == 6);
    memset(got, 0, sizeof got);
    CK(ReadConsoleOutputCharacterA(b, got, 6, z, &n) && !memcmp(got, "NEWBUF", 6));
    CK(ReadConsoleOutputCharacterA(orig, got, 6, z, &n) && !memcmp(got, "OLDBUF", 6));
    /* the std handles are unaffected */
    CK(GetStdHandle(STD_OUTPUT_HANDLE) != b);
    /* closing works and the handle value becomes invalid */
    CK(CloseHandle(b));
    SetLastError(0);
    CK(!GetConsoleMode(b, &mode));
    CK(GetLastError() == ERROR_INVALID_HANDLE);
    CloseHandle(orig);
    return 0;
}
TEST(gap18_ccsb_basic_new_console) {
    REQUIRE(U(CreateConsoleScreenBuffer));
    if (t_is_wine()) { t_skip("console children skipped under wine"); return; }
    EQ(g18_run("gap18_ccsb_basic", CREATE_NEW_CONSOLE), 0);
}

CHILD(gap18_ccsb_inherit) {
    CK(U(CreateConsoleScreenBuffer));
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE inh = U(CreateConsoleScreenBuffer)(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CONSOLE_TEXTMODE_BUFFER, NULL);
    CK(inh != INVALID_HANDLE_VALUE);
    DWORD fl = 0; CK(GetHandleInformation(inh, &fl)); CK(fl & HANDLE_FLAG_INHERIT);
    SECURITY_ATTRIBUTES sa2 = { sizeof sa2, NULL, FALSE };
    HANDLE ninh = U(CreateConsoleScreenBuffer)(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa2, CONSOLE_TEXTMODE_BUFFER, NULL);
    CK(ninh != INVALID_HANDLE_VALUE);
    CK(GetHandleInformation(ninh, &fl)); CK(!(fl & HANDLE_FLAG_INHERIT));
    HANDLE plain = U(CreateConsoleScreenBuffer)(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    CK(plain != INVALID_HANDLE_VALUE);
    CK(GetHandleInformation(plain, &fl)); CK(!(fl & HANDLE_FLAG_INHERIT));
    CloseHandle(inh); CloseHandle(ninh); CloseHandle(plain);
    return 0;
}
TEST(gap18_ccsb_inherit_flag) {
    REQUIRE(U(CreateConsoleScreenBuffer));
    if (t_is_wine()) { t_skip("console children skipped under wine"); return; }
    EQ(g18_run("gap18_ccsb_inherit", CREATE_NEW_CONSOLE), 0);
}

CHILD(gap18_ccsb_access) {
    CK(U(CreateConsoleScreenBuffer));
    HANDLE w = U(CreateConsoleScreenBuffer)(GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    CK(w != INVALID_HANDLE_VALUE);
    DWORD n; COORD z = { 0, 0 };
    CK(WriteConsoleOutputCharacterA(w, "ab", 2, z, &n) && n == 2);
    char b[4];
    SetLastError(0);
    BOOL rd = ReadConsoleOutputCharacterA(w, b, 2, z, &n);
    DWORD e = GetLastError();
    CK(!rd);           /* reading needs GENERIC_READ */
    CK(e == ERROR_ACCESS_DENIED || e == ERROR_INVALID_HANDLE);
    CloseHandle(w);
    HANDLE r = U(CreateConsoleScreenBuffer)(GENERIC_READ, FILE_SHARE_READ, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    CK(r != INVALID_HANDLE_VALUE);
    SetLastError(0);
    BOOL wr = WriteConsoleOutputCharacterA(r, "ab", 2, z, &n);
    CK(!wr);
    CloseHandle(r);
    return 0;
}
TEST(gap18_ccsb_access_rights) {
    REQUIRE(U(CreateConsoleScreenBuffer));
    if (t_is_wine()) { t_skip("console children skipped under wine"); return; }
    EQ(g18_run("gap18_ccsb_access", CREATE_NEW_CONSOLE), 0);
}

CHILD(gap18_ccsb_many) {
    CK(U(CreateConsoleScreenBuffer));
    HANDLE hs[40];
    for (int i = 0; i < 40; i++) {
        hs[i] = U(CreateConsoleScreenBuffer)(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
        CK(hs[i] != INVALID_HANDLE_VALUE);
        for (int j = 0; j < i; j++) CK(hs[i] != hs[j]);
    }
    /* each buffer is independent */
    for (int i = 0; i < 40; i++) { DWORD n; COORD z = { 0, 0 }; char c = 'A' + (i % 26); CK(WriteConsoleOutputCharacterA(hs[i], &c, 1, z, &n)); }
    for (int i = 0; i < 40; i++) { DWORD n; COORD z = { 0, 0 }; char c = 0; CK(ReadConsoleOutputCharacterA(hs[i], &c, 1, z, &n) && c == 'A' + (i % 26)); }
    for (int i = 0; i < 40; i++) CK(CloseHandle(hs[i]));
    return 0;
}
TEST(gap18_ccsb_many_buffers) {
    REQUIRE(U(CreateConsoleScreenBuffer));
    if (t_is_wine()) { t_skip("console children skipped under wine"); return; }
    EQ(g18_run("gap18_ccsb_many", CREATE_NEW_CONSOLE), 0);
}

CHILD(gap18_ccsb_active) {
    CK(U(CreateConsoleScreenBuffer));
    HANDLE orig = CreateFileA("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CK(orig != INVALID_HANDLE_VALUE);
    HANDLE b = U(CreateConsoleScreenBuffer)(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    CK(b != INVALID_HANDLE_VALUE);
    CK(SetConsoleActiveScreenBuffer(b));
    /* a freshly opened CONOUT$ now refers to the active buffer */
    HANDLE cur = CreateFileA("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CK(cur != INVALID_HANDLE_VALUE);
    DWORD n; COORD z = { 0, 0 };
    CK(WriteConsoleOutputCharacterA(b, "ACTIVE", 6, z, &n));
    char g[8] = {0}; CK(ReadConsoleOutputCharacterA(cur, g, 6, z, &n) && !memcmp(g, "ACTIVE", 6));
    CK(SetConsoleActiveScreenBuffer(orig));
    CloseHandle(cur);
    CK(CloseHandle(b));
    CloseHandle(orig);
    return 0;
}
TEST(gap18_ccsb_becomes_active_buffer) {
    REQUIRE(U(CreateConsoleScreenBuffer));
    if (t_is_wine()) { t_skip("console children skipped under wine"); return; }
    EQ(g18_run("gap18_ccsb_active", CREATE_NEW_CONSOLE), 0);
}

CHILD(gap18_ccsb_noconsole) {
    CK(U(CreateConsoleScreenBuffer));
    SetLastError(0);
    HANDLE h = U(CreateConsoleScreenBuffer)(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    if (h != INVALID_HANDLE_VALUE) { CloseHandle(h); return 2; }   /* cannot create a buffer without a console */
    return GetLastError() ? 0 : 3;
}
TEST(gap18_ccsb_detached_process_fails) {
    REQUIRE(U(CreateConsoleScreenBuffer));
    if (t_is_wine()) { t_skip("console children skipped under wine"); return; }
    EQ(g18_run("gap18_ccsb_noconsole", DETACHED_PROCESS), 0);
}

CHILD(gap18_ccsb_sharemodes) {
    CK(U(CreateConsoleScreenBuffer));
    static const DWORD sh[] = { 0, FILE_SHARE_READ, FILE_SHARE_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_SHARE_DELETE, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof sh / sizeof sh[0]; i++) {
        SetLastError(0);
        HANDLE h = U(CreateConsoleScreenBuffer)(GENERIC_READ | GENERIC_WRITE, sh[i], NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
        if (h == INVALID_HANDLE_VALUE) { if (sh[i] != 0xFFFFFFFF) return 10 + i; continue; }
        CloseHandle(h);
    }
    return 0;
}
TEST(gap18_ccsb_share_modes) {
    REQUIRE(U(CreateConsoleScreenBuffer));
    if (t_is_wine()) { t_skip("console children skipped under wine"); return; }
    EQ(g18_run("gap18_ccsb_sharemodes", CREATE_NEW_CONSOLE), 0);
}

/* ------------------------- Comm family on non-comm handles ------------------------- */
typedef struct { const char *name; HANDLE h; int must_fail; } cm_h;

static void cm_probe(const cm_h *c) {
    DCB dcb; memset(&dcb, 0, sizeof dcb); dcb.DCBlength = sizeof dcb; dcb.BaudRate = CBR_9600; dcb.ByteSize = 8;
    COMMTIMEOUTS to; memset(&to, 0, sizeof to);
    COMMCONFIG cc; memset(&cc, 0, sizeof cc); cc.dwSize = sizeof cc; cc.dcb = dcb;
    DWORD m = 0xFEEDF00D, ms = 0xFEEDF00D;
    struct { const char *n; BOOL r; DWORD e; } res[7];
    SET_ERR(0); res[0].r = U(SetCommState)(c->h, &dcb);                        res[0].e = GetLastError(); res[0].n = "SetCommState";
    SET_ERR(0); res[1].r = U(SetCommTimeouts)(c->h, &to);                      res[1].e = GetLastError(); res[1].n = "SetCommTimeouts";
    SET_ERR(0); res[2].r = U(SetCommConfig)(c->h, &cc, sizeof cc);             res[2].e = GetLastError(); res[2].n = "SetCommConfig";
    SET_ERR(0); res[3].r = U(GetCommMask)(c->h, &m);                           res[3].e = GetLastError(); res[3].n = "GetCommMask";
    SET_ERR(0); res[4].r = U(GetCommModemStatus)(c->h, &ms);                   res[4].e = GetLastError(); res[4].n = "GetCommModemStatus";
    SET_ERR(0); res[5].r = U(SetCommBreak)(c->h);                              res[5].e = GetLastError(); res[5].n = "SetCommBreak";
    SET_ERR(0); res[6].r = U(ClearCommBreak)(c->h);                            res[6].e = GetLastError(); res[6].n = "ClearCommBreak";
    for (int i = 0; i < 7; i++) {
        if (c->must_fail) {
            CHECKF(!res[i].r, "%s succeeded on %s", res[i].n, c->name);
            CHECKF(res[i].e != 0, "%s on %s: no last-error", res[i].n, c->name);
        } else t_note("%s on %s -> %d err %lu", res[i].n, c->name, res[i].r, res[i].e);
    }
    if (c->must_fail) {
        CHECKF(m == 0xFEEDF00D || m == 0, "GetCommMask output touched on failure: 0x%lx (%s)", m, c->name);
        CHECKF(ms == 0xFEEDF00D || ms == 0, "GetCommModemStatus output touched on failure: 0x%lx (%s)", ms, c->name);
    }
}
#define NEED_COMM() do { REQUIRE(U(SetCommState)); REQUIRE(U(SetCommTimeouts)); REQUIRE(U(SetCommConfig)); REQUIRE(U(GetCommMask)); \
    REQUIRE(U(GetCommModemStatus)); REQUIRE(U(SetCommBreak)); REQUIRE(U(ClearCommBreak)); } while (0)

TEST(gap18_comm_null_handle) {
    NEED_COMM();
    cm_h c = { "NULL", NULL, 1 }; cm_probe(&c);
    DWORD m; SET_ERR(0); CHECK(!U(GetCommMask)(NULL, &m)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(GetCommModemStatus)(NULL, &m)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(SetCommBreak)(NULL)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(ClearCommBreak)(NULL)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    COMMTIMEOUTS to = {0}; SET_ERR(0); CHECK(!U(SetCommTimeouts)(NULL, &to)); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap18_comm_invalid_handle_value) {
    NEED_COMM();
    cm_h c = { "INVALID_HANDLE_VALUE", INVALID_HANDLE_VALUE, 1 }; cm_probe(&c);
}
TEST(gap18_comm_closed_handle) {
    NEED_COMM();
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL); CloseHandle(e);
    cm_h c = { "closed handle", e, 1 }; cm_probe(&c);
    DWORD m; SET_ERR(0); CHECK(!U(GetCommMask)(e, &m)); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap18_comm_event_handle) {
    NEED_COMM();
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    cm_h c = { "event", e, 1 }; cm_probe(&c);
    DWORD m; SET_ERR(0); CHECK(!U(GetCommMask)(e, &m)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(e);
}
TEST(gap18_comm_regular_file_handle) {
    NEED_COMM();
    char p[MAX_PATH]; g18_path(p, MAX_PATH, "comm_file.bin"); g18_put(p, "x", 1);
    HANDLE f = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(f != INVALID_HANDLE_VALUE);
    cm_h c = { "disk file", f, 1 }; cm_probe(&c);
    DWORD m; SET_ERR(0); CHECK(!U(GetCommMask)(f, &m));
    t_note("GetCommMask(file) err %lu", GetLastError());
    CloseHandle(f); g18_rm(p);
}
TEST(gap18_comm_directory_handle) {
    NEED_COMM();
    HANDLE d = CreateFileA(t_tmpdir(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    CHECK(d != INVALID_HANDLE_VALUE);
    cm_h c = { "directory", d, 1 }; cm_probe(&c);
    CloseHandle(d);
}
TEST(gap18_comm_mutex_semaphore_thread_process_handles) {
    NEED_COMM();
    HANDLE hs[3] = { CreateMutexA(NULL, FALSE, NULL), CreateSemaphoreA(NULL, 1, 1, NULL), NULL };
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &hs[2], 0, FALSE, DUPLICATE_SAME_ACCESS);
    static const char *names[] = { "mutex", "semaphore", "thread" };
    for (int i = 0; i < 3; i++) { cm_h c = { names[i], hs[i], 1 }; cm_probe(&c); CloseHandle(hs[i]); }
    cm_h pp = { "process pseudo-handle", GetCurrentProcess(), 1 }; cm_probe(&pp);
}
TEST(gap18_comm_pipe_and_nul) {
    NEED_COMM();
    HANDLE r, w; CHECK(CreatePipe(&r, &w, NULL, 0));
    cm_h c1 = { "anonymous pipe read end", r, 0 }; cm_probe(&c1);
    cm_h c2 = { "anonymous pipe write end", w, 0 }; cm_probe(&c2);
    CloseHandle(r); CloseHandle(w);
    HANDLE n = CreateFileA("NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (n != INVALID_HANDLE_VALUE) { cm_h c3 = { "NUL device", n, 0 }; cm_probe(&c3); CloseHandle(n); }
}
TEST(gap18_comm_error_consistency_on_file) {
    NEED_COMM();
    char p[MAX_PATH]; g18_path(p, MAX_PATH, "comm_cons.bin"); g18_put(p, "x", 1);
    HANDLE f = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    DWORD m, e1, e2, e3;
    SET_ERR(0); U(GetCommMask)(f, &m); e1 = GetLastError();
    SET_ERR(0); U(GetCommModemStatus)(f, &m); e2 = GetLastError();
    SET_ERR(0); U(SetCommBreak)(f); e3 = GetLastError();
    EQ(e1, e2);      /* both are plain IOCTLs to the same driver, same NTSTATUS mapping */
    t_note("SetCommBreak err %lu", e3);
    t_note("comm IOCTL on disk file -> err %lu", e1);
    CloseHandle(f); g18_rm(p);
}
TEST(gap18_comm_setcommconfig_provider_offset) {
    NEED_COMM();
    COMMCONFIG cc; memset(&cc, 0, sizeof cc); cc.dwSize = sizeof cc; cc.dcb.DCBlength = sizeof(DCB);
    cc.dwProviderOffset = 0x100;          /* takes the provider-settings IOCTL path */
    cc.dwProviderSize = 4;
    SET_ERR(0);
    CHECK(!U(SetCommConfig)(NULL, &cc, sizeof cc));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    CHECK(!U(SetCommConfig)(e, &cc, sizeof cc));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(e);
}
TEST(gap18_comm_dcb_length_ignored_on_bad_handle) {
    NEED_COMM();
    DCB dcb; memset(&dcb, 0, sizeof dcb);   /* DCBlength deliberately 0 */
    SET_ERR(0);
    CHECK(!U(SetCommState)(NULL, &dcb));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap18_comm_kernel32_exports_present) {
    NEED_COMM();
    REQUIRE(UK(SetCommState)); REQUIRE(UK(SetCommTimeouts)); REQUIRE(UK(SetCommConfig));
    REQUIRE(UK(GetCommMask)); REQUIRE(UK(GetCommModemStatus)); REQUIRE(UK(SetCommBreak)); REQUIRE(UK(ClearCommBreak));
    DWORD m; SET_ERR(0);
    CHECK(!UK(GetCommMask)(NULL, &m)); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap18_comm_repeat_failures_no_leak) {
    NEED_COMM();
    /* every call creates/closes an internal event; hammering must not exhaust handles */
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    DWORD m;
    for (int i = 0; i < 3000; i++) { U(GetCommMask)(e, &m); U(SetCommBreak)(e); }
    HANDLE probe = CreateEventA(NULL, TRUE, FALSE, NULL);
    CHECK(probe != NULL);
    CloseHandle(probe); CloseHandle(e);
}
