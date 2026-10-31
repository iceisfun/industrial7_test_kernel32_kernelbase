#include "ho.h"

/* ---- CloseHandle ---- */
TEST(ho_close_valid_event) {
    REQUIRE(U(CloseHandle));
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL);
    CHECK(h != NULL);
    SET_ERR(0);
    EXPECT_EQ(U(CloseHandle)(h), TRUE);
    EXPECT_EQ(ho_live(h), 0);
}
TEST(ho_close_success_keeps_last_error) {
    REQUIRE(U(CloseHandle));
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0x1234);
    EXPECT_EQ(U(CloseHandle)(h), TRUE);
    EXPECT_ERR(0x1234);
}
TEST(ho_close_double_close) {
    REQUIRE(U(CloseHandle));
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL);
    EXPECT_EQ(U(CloseHandle)(h), TRUE);
    SET_ERR(0);
    EXPECT_EQ(U(CloseHandle)(h), FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    EXPECT_EQ(U(CloseHandle)(h), FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(ho_close_null) {
    REQUIRE(U(CloseHandle));
    SET_ERR(0);
    EXPECT_EQ(U(CloseHandle)(NULL), FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(ho_close_invalid_values) {
    REQUIRE(U(CloseHandle));
    ULONG_PTR vals[] = { 0x4, 0x7F0, 0x12344, 0xFFFC, 0x7FFFFFFC, 0x100000, 0x3FFFFC };
    for (unsigned i = 0; i < sizeof vals / sizeof *vals; i++) {
        if (ho_live((HANDLE)vals[i])) continue; /* happens to be a real handle: skip */
        SET_ERR(0);
        BOOL r = U(CloseHandle)((HANDLE)vals[i]);
        CHECKF(r == FALSE, "val=%llx", (unsigned long long)vals[i]);
        EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
}
TEST(ho_close_invalid_handle_value_and_pseudo) {
    REQUIRE(U(CloseHandle)); REQUIRE(U(GetProcessId)); REQUIRE(U(GetThreadId));
    /* Pseudo handles: behaviour recorded, and the pseudo handles must keep working afterwards. */
    SET_ERR(0);
    BOOL r1 = U(CloseHandle)(INVALID_HANDLE_VALUE); DWORD e1 = GetLastError();
    BOOL r2 = U(CloseHandle)(GetCurrentThread()); DWORD e2 = GetLastError();
    t_note("CloseHandle(-1)=%d err=%lu, CloseHandle(-2)=%d err=%lu", r1, e1, r2, e2);
    EXPECT_EQ(U(GetProcessId)(GetCurrentProcess()), GetCurrentProcessId());
    EXPECT_EQ(U(GetThreadId)(GetCurrentThread()), GetCurrentThreadId());
}
TEST(ho_close_does_not_close_dup) {
    REQUIRE(U(CloseHandle)); REQUIRE(U(DuplicateHandle));
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL), d = NULL;
    CHECK(DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS));
    EXPECT_EQ(U(CloseHandle)(h), TRUE);
    CHECK(SetEvent(d));
    EXPECT_EQ(WaitForSingleObject(d, 0), WAIT_OBJECT_0);
    EXPECT_EQ(U(CloseHandle)(d), TRUE);
}
TEST(ho_close_many_types) {
    REQUIRE(U(CloseHandle));
    HANDLE hs[10]; int n = 0; char p[MAX_PATH];
    t_tmpfile(p, sizeof p, "close_types.bin");
    hs[n++] = CreateEventA(NULL, FALSE, FALSE, NULL);
    hs[n++] = CreateMutexA(NULL, FALSE, NULL);
    hs[n++] = CreateSemaphoreA(NULL, 0, 1, NULL);
    hs[n++] = CreateWaitableTimerA(NULL, FALSE, NULL);
    hs[n++] = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    hs[n++] = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, NULL);
    hs[n++] = CreateJobObjectA(NULL, NULL);
    hs[n++] = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    { HANDLE r, w; CreatePipe(&r, &w, NULL, 0); hs[n++] = r; hs[n++] = w; }
    for (int i = 0; i < n; i++) {
        CHECKF(hs[i] != NULL && hs[i] != INVALID_HANDLE_VALUE, "i=%d", i);
        EXPECT_EQ(U(CloseHandle)(hs[i]), TRUE);
        SET_ERR(0);
        EXPECT_EQ(U(CloseHandle)(hs[i]), FALSE);
        EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
}
TEST(ho_close_file_deletes_on_close) {
    REQUIRE(U(CloseHandle));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "doc.bin");
    HANDLE h = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    CHECK(GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES);
    EXPECT_EQ(U(CloseHandle)(h), TRUE);
    CHECK(GetFileAttributesA(p) == INVALID_FILE_ATTRIBUTES);
}
TEST(ho_close_share_released) {
    REQUIRE(U(CloseHandle));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "share.bin");
    HANDLE h = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    HANDLE h2 = CreateFileA(p, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h2 == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_SHARING_VIOLATION);
    U(CloseHandle)(h);
    h2 = CreateFileA(p, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h2 != INVALID_HANDLE_VALUE);
    CloseHandle(h2);
    DeleteFileA(p);
}
TEST(ho_close_thread_handle_thread_runs) {
    REQUIRE(U(CloseHandle)); REQUIRE(U(CreateThread));
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE ev_done = CreateEventA(NULL, TRUE, FALSE, NULL);
    struct { HANDLE a, b; } ctx = { ev, ev_done };
    DWORD WINAPI worker(LPVOID p) { struct { HANDLE a, b; } *c = p; WaitForSingleObject(c->a, 5000); SetEvent(c->b); return 0; }
    HANDLE t = U(CreateThread)(NULL, 0, worker, &ctx, 0, NULL);
    CHECK(t != NULL);
    EXPECT_EQ(U(CloseHandle)(t), TRUE); /* closing the thread handle does not stop the thread */
    SetEvent(ev);
    EXPECT_EQ(WaitForSingleObject(ev_done, 5000), WAIT_OBJECT_0);
    CloseHandle(ev); CloseHandle(ev_done);
}
TEST(ho_close_process_handle_after_exit) {
    REQUIRE(U(CloseHandle));
    PROCESS_INFORMATION pi; HANDLE h = t_spawn_child("ho_echo", "5", 0, NULL, NULL, NULL, FALSE, NULL, &pi);
    CHECK(h != NULL); if (!h) return;
    EXPECT_EQ(WaitForSingleObject(h, 20000), WAIT_OBJECT_0);
    DWORD code = 0; CHECK(GetExitCodeProcess(h, &code)); EXPECT_EQ(code, 5);
    EXPECT_EQ(U(CloseHandle)(pi.hThread), TRUE);
    EXPECT_EQ(U(CloseHandle)(h), TRUE);
    EXPECT_EQ(U(CloseHandle)(h), FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
CHILD(ho_echo) { return argc > 1 ? atoi(argv[1]) : 0; }

TEST(ho_close_handle_low_bits_alias) {
    REQUIRE(U(CloseHandle)); REQUIRE(U(GetHandleInformation));
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL);
    for (int i = 1; i <= 3; i++) {
        DWORD f = 0xFF;
        CHECKF(U(GetHandleInformation)((HANDLE)((ULONG_PTR)h + i), &f), "low bits %d", i);
        SetEvent((HANDLE)((ULONG_PTR)h + i));
    }
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
    EXPECT_EQ(U(CloseHandle)((HANDLE)((ULONG_PTR)h + 2)), TRUE); /* closes the same slot */
    EXPECT_EQ(ho_live(h), 0);
}
TEST(ho_close_reuse_after_close) {
    REQUIRE(U(CloseHandle));
    /* a freed handle value is reused for the next allocation (most-recently freed slot first) */
    HANDLE a = CreateEventA(NULL, TRUE, FALSE, NULL);
    U(CloseHandle)(a);
    HANDLE b = CreateEventA(NULL, TRUE, FALSE, NULL);
    t_note("old=%p new=%p", a, b);
    CHECK(b != NULL);
    CHECK(ho_live(b));
    CloseHandle(b);
}
TEST(ho_close_stale_handle_after_reuse_is_new_object) {
    REQUIRE(U(CloseHandle));
    HANDLE a = CreateEventA(NULL, TRUE, TRUE, NULL);
    U(CloseHandle)(a);
    HANDLE b = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (a == b) { EXPECT_EQ(WaitForSingleObject(a, 0), WAIT_TIMEOUT); } /* the stale value now means the new, unsignaled event */
    CloseHandle(b);
}

/* ---- protected handle / debugger behaviour (child) ---- */
static volatile LONG g_veh_hits; static volatile DWORD g_veh_code;
static LONG CALLBACK ho_veh(PEXCEPTION_POINTERS ep) {
    DWORD c = ep->ExceptionRecord->ExceptionCode;
    if (c == 0xC0000008 || c == 0xC0000235) { g_veh_code = c; InterlockedIncrement(&g_veh_hits); return EXCEPTION_CONTINUE_EXECUTION; }
    return EXCEPTION_CONTINUE_SEARCH;
}
/* exit code: bit0 CloseHandle result, bits 8..15 low byte of last error, bit 1 child VEH saw exception */
CHILD(ho_closeinv) {
    AddVectoredExceptionHandler(1, ho_veh);
    SetLastError(0);
    BOOL r = CloseHandle((HANDLE)0x12344);
    DWORD e = GetLastError();
    return (r ? 1 : 0) | (g_veh_hits ? 2 : 0) | ((e & 0xFF) << 8);
}
CHILD(ho_closeprot) {
    AddVectoredExceptionHandler(1, ho_veh);
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!SetHandleInformation(h, HANDLE_FLAG_PROTECT_FROM_CLOSE, HANDLE_FLAG_PROTECT_FROM_CLOSE)) return 0xF0;
    SetLastError(0);
    BOOL r = CloseHandle(h);
    DWORD e = GetLastError();
    DWORD fl = 0; GetHandleInformation(h, &fl);
    int still = (fl & HANDLE_FLAG_PROTECT_FROM_CLOSE) != 0;
    SetHandleInformation(h, HANDLE_FLAG_PROTECT_FROM_CLOSE, 0);
    BOOL r2 = CloseHandle(h);
    return (r ? 1 : 0) | (g_veh_hits ? 2 : 0) | (still ? 4 : 0) | (r2 ? 8 : 0) | ((e & 0xFF) << 8);
}
TEST(ho_close_invalid_child_no_debugger) {
    REQUIRE(U(CloseHandle));
    DWORD c = t_run_child("ho_closeinv", NULL, 20000);
    EXPECT_EQ(c & 3, 0);          /* FALSE, no exception without a debugger */
    EXPECT_EQ((c >> 8) & 0xFF, ERROR_INVALID_HANDLE);
}
TEST(ho_close_protected_child_no_debugger) {
    REQUIRE(U(CloseHandle)); REQUIRE(U(SetHandleInformation));
    DWORD c = t_run_child("ho_closeprot", NULL, 20000);
    EXPECT_EQ(c & 1, 0);          /* CloseHandle(protected) fails */
    EXPECT_EQ(c & 2, 0);          /* no exception without a debugger */
    EXPECT_EQ(c & 4, 4);          /* the protection flag survives the failed close */
    EXPECT_EQ(c & 8, 8);          /* after clearing the flag, close works */
    DWORD e = (c >> 8) & 0xFF;
    ho_RtlNtStatusToDosError_pfn m = (ho_RtlNtStatusToDosError_pfn)t_proc_nt("RtlNtStatusToDosError");
    if (m) { DWORD want = m((LONG)0xC0000235) & 0xFF; EXPECT_EQ(e, want); }
    t_note("protected close last error = %lu", e);
}
/* run a child under our own debugger; counts exception events by code */
static int ho_debug_run(const char *child, DWORD *exitcode, int *n_invalid, int *n_notclosable) {
    PROCESS_INFORMATION pi; *n_invalid = *n_notclosable = 0; *exitcode = 0xDEAD0000;
    HANDLE h = t_spawn_child(child, NULL, DEBUG_ONLY_THIS_PROCESS, NULL, NULL, NULL, FALSE, NULL, &pi);
    if (!h) return 0;
    DWORD start = GetTickCount(); int done = 0;
    while (!done && GetTickCount() - start < 25000) {
        DEBUG_EVENT ev;
        if (!WaitForDebugEvent(&ev, 500)) continue;
        DWORD cont = DBG_CONTINUE;
        switch (ev.dwDebugEventCode) {
        case CREATE_PROCESS_DEBUG_EVENT: if (ev.u.CreateProcessInfo.hFile) CloseHandle(ev.u.CreateProcessInfo.hFile); break;
        case LOAD_DLL_DEBUG_EVENT: if (ev.u.LoadDll.hFile) CloseHandle(ev.u.LoadDll.hFile); break;
        case EXCEPTION_DEBUG_EVENT: {
            DWORD c = ev.u.Exception.ExceptionRecord.ExceptionCode;
            if (c == 0xC0000008 && ev.u.Exception.dwFirstChance) (*n_invalid)++;
            if (c == 0xC0000235 && ev.u.Exception.dwFirstChance) (*n_notclosable)++;
            if (c != 0x80000003 && c != 0x4000001f) cont = DBG_EXCEPTION_NOT_HANDLED;
            break; }
        case EXIT_PROCESS_DEBUG_EVENT: done = 1; break;
        }
        ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, cont);
    }
    if (!done) { TerminateProcess(h, 1); }
    WaitForSingleObject(h, 5000);
    GetExitCodeProcess(h, exitcode);
    CloseHandle(pi.hThread); CloseHandle(h);
    return done;
}
TEST(ho_close_invalid_child_under_debugger) {
    REQUIRE(U(CloseHandle));
    if (t_is_wine()) { t_skip("debug events unreliable under emulator"); return; }
    DWORD code; int ni, nn;
    CHECK(ho_debug_run("ho_closeinv", &code, &ni, &nn));
    CHECKF(ni >= 1, "EXCEPTION_INVALID_HANDLE events=%d", ni);
    EXPECT_EQ(code & 1, 0);
    EXPECT_EQ((code >> 8) & 0xFF, ERROR_INVALID_HANDLE);
}
TEST(ho_close_protected_child_under_debugger) {
    REQUIRE(U(CloseHandle));
    if (t_is_wine()) { t_skip("debug events unreliable under emulator"); return; }
    DWORD code; int ni, nn;
    CHECK(ho_debug_run("ho_closeprot", &code, &ni, &nn));
    t_note("protected close under debugger: invalid=%d notclosable=%d exit=0x%lx", ni, nn, code);
    EXPECT_EQ(code & 1, 0);
    EXPECT_EQ(code & 8, 8);
}
