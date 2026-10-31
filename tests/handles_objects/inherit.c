#include "ho.h"

/* Child: argv[1..3] = hex handle values A, B, C (0 = none). Reports which are live in this process.
   exit: bit0 A live, bit1 B live, bit2 C live, bit3 A has HANDLE_FLAG_INHERIT, bit4 A is a signalable event and SetEvent worked */
CHILD(ho_inh_probe) {
    HANDLE a = argc > 1 ? (HANDLE)ho_parse(argv[1]) : NULL;
    HANDLE b = argc > 2 ? (HANDLE)ho_parse(argv[2]) : NULL;
    HANDLE c = argc > 3 ? (HANDLE)ho_parse(argv[3]) : NULL;
    DWORD f = 0, rc = 0;
    if (a && GetHandleInformation(a, &f)) { rc |= 1; if (f & HANDLE_FLAG_INHERIT) rc |= 8; if (SetEvent(a)) rc |= 16; }
    if (b && GetHandleInformation(b, &f)) rc |= 2;
    if (c && GetHandleInformation(c, &f)) rc |= 4;
    return rc;
}
static void hexargs(char *out, size_t cap, HANDLE a, HANDLE b, HANDLE c) {
    snprintf(out, cap, "%llx %llx %llx", (unsigned long long)(ULONG_PTR)a, (unsigned long long)(ULONG_PTR)b, (unsigned long long)(ULONG_PTR)c);
}

TEST(ho_inh_matrix_event_inherit_flag_vs_param) {
    REQUIRE(U(CreateProcessA));
    ho_pad(300);
    HANDLE inh = ho_mkevent(TRUE, TRUE, FALSE), non = ho_mkevent(FALSE, TRUE, FALSE);
    char args[100]; hexargs(args, sizeof args, inh, non, NULL);
    /* bInheritHandles = TRUE: only the inheritable one arrives, at the same value */
    DWORD c = ho_run_inherit("ho_inh_probe", args, 0);
    EXPECT_EQ(c & 1, 1);
    EXPECT_EQ(c & 2, 0);
    EXPECT_EQ(c & 8, 8);        /* stays inheritable in the child */
    EXPECT_EQ(c & 16, 16);
    EXPECT_EQ(WaitForSingleObject(inh, 0), WAIT_OBJECT_0);   /* the child signaled our event */
    EXPECT_EQ(WaitForSingleObject(non, 0), WAIT_TIMEOUT);
    ResetEvent(inh);
    /* bInheritHandles = FALSE: nothing arrives */
    c = t_run_child("ho_inh_probe", args, 20000);
    EXPECT_EQ(c & 3, 0);
    EXPECT_EQ(WaitForSingleObject(inh, 0), WAIT_TIMEOUT);
    CloseHandle(inh); CloseHandle(non);
    ho_unpad();
}
TEST(ho_inh_set_after_create_via_sethandleinformation) {
    REQUIRE(U(CreateProcessA)); REQUIRE(U(SetHandleInformation));
    ho_pad(300);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    char args[100]; hexargs(args, sizeof args, e, NULL, NULL);
    DWORD c = ho_run_inherit("ho_inh_probe", args, 0);
    EXPECT_EQ(c & 1, 0);
    U(SetHandleInformation)(e, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    c = ho_run_inherit("ho_inh_probe", args, 0);
    EXPECT_EQ(c & 1, 1);
    EXPECT_EQ(WaitForSingleObject(e, 0), WAIT_OBJECT_0);
    ResetEvent(e);
    U(SetHandleInformation)(e, HANDLE_FLAG_INHERIT, 0);
    c = ho_run_inherit("ho_inh_probe", args, 0);
    EXPECT_EQ(c & 1, 0);
    EXPECT_EQ(WaitForSingleObject(e, 0), WAIT_TIMEOUT);
    CloseHandle(e);
    ho_unpad();
}
TEST(ho_inh_duplicate_with_inherit_true) {
    REQUIRE(U(CreateProcessA)); REQUIRE(U(DuplicateHandle));
    ho_pad(300);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL), d = NULL, d2 = NULL;
    U(DuplicateHandle)(GetCurrentProcess(), e, GetCurrentProcess(), &d, 0, TRUE, DUPLICATE_SAME_ACCESS);
    U(DuplicateHandle)(GetCurrentProcess(), e, GetCurrentProcess(), &d2, 0, FALSE, DUPLICATE_SAME_ACCESS);
    char args[100]; hexargs(args, sizeof args, d, d2, e);
    DWORD c = ho_run_inherit("ho_inh_probe", args, 0);
    EXPECT_EQ(c & 7, 1);
    EXPECT_EQ(WaitForSingleObject(e, 0), WAIT_OBJECT_0);
    CloseHandle(e); CloseHandle(d); CloseHandle(d2);
    ho_unpad();
}
TEST(ho_inh_same_value_in_child_with_many_handles) {
    REQUIRE(U(CreateProcessA));
    ho_pad(500);
    HANDLE hs[5];
    for (int i = 0; i < 5; i++) hs[i] = ho_mkevent(TRUE, TRUE, FALSE);
    char args[100]; hexargs(args, sizeof args, hs[4], hs[0], hs[2]);
    DWORD c = ho_run_inherit("ho_inh_probe", args, 0);
    EXPECT_EQ(c & 7, 7);
    EXPECT_EQ(WaitForSingleObject(hs[4], 0), WAIT_OBJECT_0);
    for (int i = 0; i < 5; i++) CloseHandle(hs[i]);
    ho_unpad();
}
TEST(ho_inh_parent_closed_copy_does_not_affect_child) {
    REQUIRE(U(CreateProcessA));
    HANDLE e = ho_mkevent(TRUE, TRUE, FALSE), keep = NULL;
    DuplicateHandle(GetCurrentProcess(), e, GetCurrentProcess(), &keep, 0, FALSE, DUPLICATE_SAME_ACCESS);
    char args[100]; snprintf(args, sizeof args, "%llx", (unsigned long long)(ULONG_PTR)e);
    PROCESS_INFORMATION pi;
    HANDLE hp = t_spawn_child("ho_inh_waitev", args, 0, NULL, NULL, NULL, TRUE, NULL, &pi);
    CHECK(hp != NULL);
    CloseHandle(e);                 /* child has its own handle to the object */
    Sleep(100);
    SetEvent(keep);
    if (hp) {
        EXPECT_EQ(WaitForSingleObject(hp, 15000), WAIT_OBJECT_0);
        DWORD code = 99; GetExitCodeProcess(hp, &code); EXPECT_EQ(code, 0);
        CloseHandle(pi.hThread); CloseHandle(hp);
    }
    CloseHandle(keep);
}
CHILD(ho_inh_waitev) {
    HANDLE e = (HANDLE)ho_parse(argv[1]);
    return WaitForSingleObject(e, 10000) == WAIT_OBJECT_0 ? 0 : 3;
}
TEST(ho_inh_pipe_child_writes_to_inherited_end) {
    REQUIRE(U(CreateProcessA));
    HANDLE r, w; SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    CreatePipe(&r, &w, &sa, 0);
    SetHandleInformation(r, HANDLE_FLAG_INHERIT, 0);
    char args[40]; snprintf(args, sizeof args, "%llx", (unsigned long long)(ULONG_PTR)w);
    PROCESS_INFORMATION pi;
    HANDLE hp = t_spawn_child("ho_inh_pipewrite", args, 0, NULL, NULL, NULL, TRUE, NULL, &pi);
    CloseHandle(w);
    CHECK(hp != NULL);
    char buf[32] = { 0 }; DWORD n = 0;
    BOOL ok = ReadFile(r, buf, 5, &n, NULL);
    CHECK(ok); EXPECT_EQ(n, 5); CHECK(!memcmp(buf, "hello", 5));
    ok = ReadFile(r, buf, 1, &n, NULL);     /* child exits -> all write ends closed -> EOF */
    EXPECT_EQ(ok, FALSE); EXPECT_ERR(ERROR_BROKEN_PIPE);
    if (hp) { WaitForSingleObject(hp, 10000); DWORD code = 9; GetExitCodeProcess(hp, &code); EXPECT_EQ(code, 0); CloseHandle(pi.hThread); CloseHandle(hp); }
    CloseHandle(r);
}
CHILD(ho_inh_pipewrite) {
    HANDLE w = (HANDLE)ho_parse(argv[1]); DWORD n = 0;
    return (WriteFile(w, "hello", 5, &n, NULL) && n == 5) ? 0 : 4;
}
TEST(ho_inh_file_shared_position) {
    REQUIRE(U(CreateProcessA));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "inhfile.bin");
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE f = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS, 0, NULL);
    DWORD n; WriteFile(f, "AAAA", 4, &n, NULL);
    char args[40]; snprintf(args, sizeof args, "%llx", (unsigned long long)(ULONG_PTR)f);
    DWORD c = ho_run_inherit("ho_inh_filewrite", args, 0);
    EXPECT_EQ(c, 0);
    EXPECT_EQ(SetFilePointer(f, 0, NULL, FILE_CURRENT), 8);   /* the child's write advanced our (shared) position */
    SetFilePointer(f, 0, NULL, FILE_BEGIN);
    char buf[16] = { 0 }; ReadFile(f, buf, 8, &n, NULL);
    CHECK(!memcmp(buf, "AAAABBBB", 8));
    CloseHandle(f); DeleteFileA(p);
}
CHILD(ho_inh_filewrite) {
    HANDLE f = (HANDLE)ho_parse(argv[1]); DWORD n = 0;
    return (WriteFile(f, "BBBB", 4, &n, NULL) && n == 4) ? 0 : 4;
}
TEST(ho_inh_semaphore_and_mutex) {
    REQUIRE(U(CreateProcessA));
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE s = CreateSemaphoreA(&sa, 0, 10, NULL), m = CreateMutexA(&sa, TRUE, NULL);
    char args[100]; hexargs(args, sizeof args, s, m, NULL);
    DWORD c = ho_run_inherit("ho_inh_semmtx", args, 0);
    EXPECT_EQ(c, 0);
    EXPECT_EQ(WaitForSingleObject(s, 0), WAIT_OBJECT_0);
    EXPECT_EQ(WaitForSingleObject(s, 0), WAIT_OBJECT_0);
    EXPECT_EQ(WaitForSingleObject(s, 0), WAIT_TIMEOUT);
    ReleaseMutex(m); CloseHandle(m); CloseHandle(s);
}
CHILD(ho_inh_semmtx) {
    HANDLE s = (HANDLE)ho_parse(argv[1]), m = (HANDLE)ho_parse(argv[2]);
    LONG prev = -1;
    if (!ReleaseSemaphore(s, 2, &prev) || prev != 0) return 1;
    /* the mutex is owned by the parent's thread: we cannot get it, and cannot release it */
    if (WaitForSingleObject(m, 0) != WAIT_TIMEOUT) return 2;
    if (ReleaseMutex(m) || GetLastError() != ERROR_NOT_OWNER) return 3;
    return 0;
}
TEST(ho_inh_mapping_shared_view) {
    REQUIRE(U(CreateProcessA));
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE m = CreateFileMappingA(INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0, 4096, NULL);
    char *v = MapViewOfFile(m, FILE_MAP_WRITE, 0, 0, 0);
    strcpy(v, "from parent");
    char args[40]; snprintf(args, sizeof args, "%llx", (unsigned long long)(ULONG_PTR)m);
    DWORD c = ho_run_inherit("ho_inh_map", args, 0);
    EXPECT_EQ(c, 0);
    EXPECT_STR(v + 64, "from child");
    UnmapViewOfFile(v); CloseHandle(m);
}
CHILD(ho_inh_map) {
    HANDLE m = (HANDLE)ho_parse(argv[1]);
    char *v = MapViewOfFile(m, FILE_MAP_WRITE, 0, 0, 0);
    if (!v) return 1;
    if (strcmp(v, "from parent")) return 2;
    strcpy(v + 64, "from child");
    return 0;
}
TEST(ho_inh_stdout_redirect_to_pipe) {
    REQUIRE(U(CreateProcessA));
    HANDLE r, w; SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    CreatePipe(&r, &w, &sa, 0); SetHandleInformation(r, HANDLE_FLAG_INHERIT, 0);
    PROCESS_INFORMATION pi;
    HANDLE hp = t_spawn_child("ho_inh_say", NULL, 0, NULL, w, w, TRUE, NULL, &pi);
    CloseHandle(w); CHECK(hp != NULL);
    char buf[64] = { 0 }; DWORD n = 0, tot = 0;
    while (tot < 20 && ReadFile(r, buf + tot, 20 - tot, &n, NULL) && n) tot += n;
    CHECK(tot >= 7); CHECK(!strncmp(buf, "to-out\n", 7));
    if (hp) { WaitForSingleObject(hp, 10000); CloseHandle(pi.hThread); CloseHandle(hp); }
    CloseHandle(r);
}
CHILD(ho_inh_say) {
    DWORD n; WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), "to-out\n", 7, &n, NULL);
    return 0;
}
TEST(ho_inh_std_handles_flag_in_child) {
    REQUIRE(U(CreateProcessA));
    HANDLE r, w; SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    CreatePipe(&r, &w, &sa, 0); SetHandleInformation(r, HANDLE_FLAG_INHERIT, 0);
    PROCESS_INFORMATION pi;
    HANDLE hp = t_spawn_child("ho_inh_stdinfo", NULL, 0, NULL, w, w, TRUE, NULL, &pi);
    CloseHandle(w); CHECK(hp != NULL);
    if (hp) {
        WaitForSingleObject(hp, 10000); DWORD code = 99; GetExitCodeProcess(hp, &code);
        EXPECT_EQ(code & 1, 1);     /* stdout valid */
        EXPECT_EQ(code & 2, 2);     /* stdout is the pipe */
        EXPECT_EQ(code & 4, 4);     /* GetStdHandle(STD_OUTPUT) == handle that is flagged inheritable */
        CloseHandle(pi.hThread); CloseHandle(hp);
    }
    CloseHandle(r);
}
CHILD(ho_inh_stdinfo) {
    HANDLE o = GetStdHandle(STD_OUTPUT_HANDLE); DWORD f = 0, rc = 0;
    if (o && o != INVALID_HANDLE_VALUE) rc |= 1;
    if (GetFileType(o) == FILE_TYPE_PIPE) rc |= 2;
    if (GetHandleInformation(o, &f) && (f & HANDLE_FLAG_INHERIT)) rc |= 4;
    return rc;
}

/* ---- PROC_THREAD_ATTRIBUTE_HANDLE_LIST ---- */
static HANDLE spawn_list(const char *child, const char *args, HANDLE *list, int n, BOOL inherit, DWORD *err, PROCESS_INFORMATION *pi) {
    SIZE_T sz = 0; STARTUPINFOEXA si; char cmd[2048];
    U(InitializeProcThreadAttributeList)(NULL, 1, 0, &sz);
    void *buf = HeapAlloc(GetProcessHeap(), 0, sz);
    memset(&si, 0, sizeof si); si.StartupInfo.cb = sizeof si; si.lpAttributeList = buf;
    if (!U(InitializeProcThreadAttributeList)(buf, 1, 0, &sz)) { HeapFree(GetProcessHeap(), 0, buf); *err = GetLastError(); return NULL; }
    if (!U(UpdateProcThreadAttribute)(buf, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, list, n * sizeof(HANDLE), NULL, NULL)) {
        *err = GetLastError(); U(DeleteProcThreadAttributeList)(buf); HeapFree(GetProcessHeap(), 0, buf); return NULL;
    }
    snprintf(cmd, sizeof cmd, "\"%s\" --child %s %s", t_self_path(), child, args ? args : "");
    memset(pi, 0, sizeof *pi);
    SET_ERR(0);
    BOOL ok = U(CreateProcessA)(NULL, cmd, NULL, NULL, inherit, EXTENDED_STARTUPINFO_PRESENT | DETACHED_PROCESS, NULL, NULL, &si.StartupInfo, pi);
    *err = GetLastError();
    U(DeleteProcThreadAttributeList)(buf); HeapFree(GetProcessHeap(), 0, buf);
    return ok ? pi->hProcess : NULL;
}
#define NEED_LIST() do { REQUIRE(U(CreateProcessA)); REQUIRE(U(InitializeProcThreadAttributeList)); REQUIRE(U(UpdateProcThreadAttribute)); REQUIRE(U(DeleteProcThreadAttributeList)); } while (0)
static DWORD finish(HANDLE hp, PROCESS_INFORMATION *pi) {
    DWORD code = 0xDEAD0000;
    if (WaitForSingleObject(hp, 20000) == WAIT_OBJECT_0) GetExitCodeProcess(hp, &code); else TerminateProcess(hp, 1);
    CloseHandle(pi->hThread); CloseHandle(hp);
    return code;
}
TEST(ho_inh_handlelist_restricts_to_list) {
    NEED_LIST();
    ho_pad(300);
    HANDLE a = ho_mkevent(TRUE, TRUE, FALSE), b = ho_mkevent(TRUE, TRUE, FALSE), c = ho_mkevent(TRUE, TRUE, FALSE);
    HANDLE list[2] = { a, c }; DWORD err; PROCESS_INFORMATION pi;
    char args[100]; hexargs(args, sizeof args, a, b, c);
    HANDLE hp = spawn_list("ho_inh_probe", args, list, 2, TRUE, &err, &pi);
    CHECKF(hp != NULL, "err=%lu", err);
    if (hp) {
        DWORD code = finish(hp, &pi);
        EXPECT_EQ(code & 7, 5);     /* A and C arrive; B (inheritable but not listed) does not */
        EXPECT_EQ(code & 8, 8);
    }
    CloseHandle(a); CloseHandle(b); CloseHandle(c);
    ho_unpad();
}
TEST(ho_inh_handlelist_single) {
    NEED_LIST();
    ho_pad(300);
    HANDLE a = ho_mkevent(TRUE, TRUE, FALSE), b = ho_mkevent(TRUE, TRUE, FALSE);
    HANDLE list[1] = { b }; DWORD err; PROCESS_INFORMATION pi;
    char args[100]; hexargs(args, sizeof args, b, a, NULL);
    HANDLE hp = spawn_list("ho_inh_probe", args, list, 1, TRUE, &err, &pi);
    CHECKF(hp != NULL, "err=%lu", err);
    if (hp) { DWORD code = finish(hp, &pi); EXPECT_EQ(code & 3, 1); }
    EXPECT_EQ(WaitForSingleObject(b, 0), WAIT_OBJECT_0);
    CloseHandle(a); CloseHandle(b);
    ho_unpad();
}
TEST(ho_inh_handlelist_value_unchanged) {
    NEED_LIST();
    ho_pad(450);
    HANDLE e = ho_mkevent(TRUE, TRUE, FALSE);
    HANDLE list[1] = { e }; DWORD err; PROCESS_INFORMATION pi;
    char args[100]; hexargs(args, sizeof args, e, NULL, NULL);
    HANDLE hp = spawn_list("ho_inh_probe", args, list, 1, TRUE, &err, &pi);
    CHECK(hp != NULL);
    if (hp) { DWORD code = finish(hp, &pi); EXPECT_EQ(code & 1, 1); }
    CloseHandle(e);
    ho_unpad();
}
TEST(ho_inh_handlelist_non_inheritable_entry_fails) {
    NEED_LIST();
    HANDLE a = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE list[1] = { a }; DWORD err = 0; PROCESS_INFORMATION pi;
    HANDLE hp = spawn_list("ho_inh_probe", NULL, list, 1, TRUE, &err, &pi);
    CHECK(hp == NULL);
    t_note("CreateProcess with non-inheritable handle in list: err=%lu", err);
    if (hp) finish(hp, &pi);
    CloseHandle(a);
}
TEST(ho_inh_handlelist_requires_inherit_true) {
    NEED_LIST();
    HANDLE a = ho_mkevent(TRUE, TRUE, FALSE);
    HANDLE list[1] = { a }; DWORD err = 0; PROCESS_INFORMATION pi;
    HANDLE hp = spawn_list("ho_inh_probe", NULL, list, 1, FALSE, &err, &pi);
    CHECK(hp == NULL);
    EXPECT_EQ(err, ERROR_INVALID_PARAMETER);
    if (hp) finish(hp, &pi);
    CloseHandle(a);
}
TEST(ho_inh_handlelist_invalid_entry_fails) {
    NEED_LIST();
    HANDLE list[1] = { (HANDLE)0x7FFFFFC }; DWORD err = 0; PROCESS_INFORMATION pi;
    HANDLE hp = spawn_list("ho_inh_probe", NULL, list, 1, TRUE, &err, &pi);
    CHECK(hp == NULL);
    t_note("CreateProcess with invalid handle in list: err=%lu", err);
    if (hp) finish(hp, &pi);
}
TEST(ho_inh_handlelist_closed_entry_fails) {
    NEED_LIST();
    HANDLE a = ho_mkevent(TRUE, TRUE, FALSE);
    CloseHandle(a);
    HANDLE list[1] = { a }; DWORD err = 0; PROCESS_INFORMATION pi;
    HANDLE hp = spawn_list("ho_inh_probe", NULL, list, 1, TRUE, &err, &pi);
    CHECK(hp == NULL);
    t_note("CreateProcess with closed handle in list: err=%lu", err);
    if (hp) finish(hp, &pi);
}
TEST(ho_inh_handlelist_pipe_pair) {
    NEED_LIST();
    HANDLE r, w; SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    CreatePipe(&r, &w, &sa, 0);
    HANDLE list[1] = { w }; DWORD err; PROCESS_INFORMATION pi;
    char args[40]; snprintf(args, sizeof args, "%llx", (unsigned long long)(ULONG_PTR)w);
    HANDLE hp = spawn_list("ho_inh_pipewrite", args, list, 1, TRUE, &err, &pi);
    CHECKF(hp != NULL, "err=%lu", err);
    CloseHandle(w);
    char buf[8] = { 0 }; DWORD n = 0;
    /* r was inheritable but not listed, so the child never held the read end; only the child's write end matters for EOF */
    if (hp) {
        ReadFile(r, buf, 5, &n, NULL); EXPECT_EQ(n, 5);
        DWORD code = finish(hp, &pi); EXPECT_EQ(code, 0);
    }
    CloseHandle(r);
}
TEST(ho_inh_handlelist_attribute_validation) {
    NEED_LIST();
    SIZE_T sz = 0;
    SET_ERR(0);
    BOOL r = U(InitializeProcThreadAttributeList)(NULL, 1, 0, &sz);
    EXPECT_EQ(r, FALSE); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(sz > 0);
    void *buf = HeapAlloc(GetProcessHeap(), 0, sz);
    EXPECT_EQ(U(InitializeProcThreadAttributeList)(buf, 1, 0, &sz), TRUE);
    HANDLE e = ho_mkevent(TRUE, TRUE, FALSE);
    HANDLE list[1] = { e };
    SET_ERR(0);
    EXPECT_EQ(U(UpdateProcThreadAttribute)(buf, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, list, sizeof(HANDLE), NULL, NULL), TRUE);
    /* a second update of a different type exceeds the declared count of 1 */
    SET_ERR(0);
    BOOL r2 = U(UpdateProcThreadAttribute)(buf, 0, PROC_THREAD_ATTRIBUTE_PARENT_PROCESS, list, sizeof(HANDLE), NULL, NULL);
    t_note("second attribute beyond count: r=%d err=%lu", r2, GetLastError());
    U(DeleteProcThreadAttributeList)(buf);
    HeapFree(GetProcessHeap(), 0, buf);
    CloseHandle(e);
}
TEST(ho_inh_handlelist_cb_not_multiple_of_handle) {
    NEED_LIST();
    SIZE_T sz = 0;
    U(InitializeProcThreadAttributeList)(NULL, 1, 0, &sz);
    void *buf = HeapAlloc(GetProcessHeap(), 0, sz);
    U(InitializeProcThreadAttributeList)(buf, 1, 0, &sz);
    HANDLE e = ho_mkevent(TRUE, TRUE, FALSE);
    HANDLE list[2] = { e, e };
    SET_ERR(0);
    BOOL r = U(UpdateProcThreadAttribute)(buf, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, list, 12, NULL, NULL);
    t_note("handle list with cbSize=12: r=%d err=%lu", r, GetLastError());
    U(DeleteProcThreadAttributeList)(buf);
    HeapFree(GetProcessHeap(), 0, buf);
    CloseHandle(e);
}
