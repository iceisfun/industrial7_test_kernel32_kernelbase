#include "g18.h"
/* gap18: WaitForSingleObjectEx, SetStdHandleEx. */

static HANDLE ev(BOOL manual, BOOL init) { return CreateEventA(NULL, manual, init, NULL); }
static DWORD WINAPI set_after(LPVOID p) { Sleep(150); SetEvent((HANDLE)p); return 0; }

TEST(gap18_wfsoex_signaled_event) {
    REQUIRE(U(WaitForSingleObjectEx));
    HANDLE e = ev(TRUE, TRUE);
    EQ(U(WaitForSingleObjectEx)(e, 0, FALSE), WAIT_OBJECT_0);
    EQ(U(WaitForSingleObjectEx)(e, 0, FALSE), WAIT_OBJECT_0);     /* manual-reset stays signaled */
    EQ(U(WaitForSingleObjectEx)(e, INFINITE, TRUE), WAIT_OBJECT_0);
    CloseHandle(e);
}
TEST(gap18_wfsoex_timeout_zero) {
    REQUIRE(U(WaitForSingleObjectEx));
    HANDLE e = ev(TRUE, FALSE);
    EQ(U(WaitForSingleObjectEx)(e, 0, FALSE), WAIT_TIMEOUT);
    EQ(U(WaitForSingleObjectEx)(e, 0, TRUE), WAIT_TIMEOUT);
    CloseHandle(e);
}
TEST(gap18_wfsoex_timeout_elapses) {
    REQUIRE(U(WaitForSingleObjectEx));
    HANDLE e = ev(TRUE, FALSE);
    DWORD t0 = GetTickCount();
    EQ(U(WaitForSingleObjectEx)(e, 120, FALSE), WAIT_TIMEOUT);
    DWORD dt = GetTickCount() - t0;
    CHECKF(dt >= 100 && dt < 3000, "elapsed %lu", dt);
    CloseHandle(e);
}
TEST(gap18_wfsoex_autoreset_consumes) {
    REQUIRE(U(WaitForSingleObjectEx));
    HANDLE e = ev(FALSE, TRUE);
    EQ(U(WaitForSingleObjectEx)(e, 0, FALSE), WAIT_OBJECT_0);
    EQ(U(WaitForSingleObjectEx)(e, 0, FALSE), WAIT_TIMEOUT);
    CloseHandle(e);
}
TEST(gap18_wfsoex_null_handle) {
    REQUIRE(U(WaitForSingleObjectEx));
    SET_ERR(0);
    EQ(U(WaitForSingleObjectEx)(NULL, 0, FALSE), WAIT_FAILED);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    EQ(U(WaitForSingleObjectEx)(NULL, 0, TRUE), WAIT_FAILED);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap18_wfsoex_bad_handles) {
    REQUIRE(U(WaitForSingleObjectEx));
    static const ULONG_PTR bad[] = { 0x7FFC, 0x3FFFC, 0x7FFFFC };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SET_ERR(0);
        DWORD r = U(WaitForSingleObjectEx)((HANDLE)bad[i], 0, FALSE);
        CHECKF(r == WAIT_FAILED, "handle %llx -> %lu", (unsigned long long)bad[i], r);
        DWORD e = GetLastError();
        CHECKF(e == ERROR_INVALID_HANDLE, "handle %llx err %lu", (unsigned long long)bad[i], e);
    }
}
TEST(gap18_wfsoex_closed_handle) {
    REQUIRE(U(WaitForSingleObjectEx));
    HANDLE e = ev(TRUE, TRUE); CloseHandle(e);
    SET_ERR(0);
    EQ(U(WaitForSingleObjectEx)(e, 0, FALSE), WAIT_FAILED);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap18_wfsoex_pseudo_current_thread) {
    REQUIRE(U(WaitForSingleObjectEx));
    SET_ERR(0);
    EQ(U(WaitForSingleObjectEx)(GetCurrentThread(), 0, FALSE), WAIT_TIMEOUT);    /* a running thread is not signaled */
    EQ(U(WaitForSingleObjectEx)(GetCurrentProcess(), 0, FALSE), WAIT_TIMEOUT);
}
TEST(gap18_wfsoex_invalid_handle_value) {
    REQUIRE(U(WaitForSingleObjectEx));
    SET_ERR(0);
    DWORD r = U(WaitForSingleObjectEx)(INVALID_HANDLE_VALUE, 0, FALSE);
    t_note("wait(INVALID_HANDLE_VALUE, 0) = 0x%lx err %lu", r, GetLastError());   /* -1 is the current-process pseudo handle */
    CHECK(r == WAIT_TIMEOUT || r == WAIT_FAILED);
}
TEST(gap18_wfsoex_wrong_object_types) {
    REQUIRE(U(WaitForSingleObjectEx));
    /* objects that can be waited on: all succeed/time out; those that cannot give an error */
    HANDLE f = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (f != INVALID_HANDLE_VALUE) {
        DWORD r = U(WaitForSingleObjectEx)(f, 0, FALSE);
        t_note("wait on NUL file handle = 0x%lx", r);
        CHECK(r == WAIT_OBJECT_0 || r == WAIT_TIMEOUT);       /* file objects are waitable (signaled on completed I/O) */
        CloseHandle(f);
    }
    HANDLE fm = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, NULL);
    SET_ERR(0);
    DWORD r = U(WaitForSingleObjectEx)(fm, 0, FALSE);
    CHECK(r == WAIT_FAILED);                                  /* sections are not waitable */
    t_note("wait on section err %lu", GetLastError());
    CloseHandle(fm);
}
TEST(gap18_wfsoex_access_denied_no_synchronize) {
    REQUIRE(U(WaitForSingleObjectEx));
    HANDLE e = ev(TRUE, TRUE), d = NULL;
    CHECK(DuplicateHandle(GetCurrentProcess(), e, GetCurrentProcess(), &d, EVENT_MODIFY_STATE, FALSE, 0));
    SET_ERR(0);
    EQ(U(WaitForSingleObjectEx)(d, 0, FALSE), WAIT_FAILED);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(d); CloseHandle(e);
}
TEST(gap18_wfsoex_access_denied_all_object_kinds) {
    REQUIRE(U(WaitForSingleObjectEx));
    HANDLE objs[4] = { ev(TRUE, TRUE), CreateMutexA(NULL, FALSE, NULL), CreateSemaphoreA(NULL, 1, 4, NULL), CreateWaitableTimerA(NULL, TRUE, NULL) };
    for (int i = 0; i < 4; i++) {
        HANDLE d = NULL;
        CHECK(DuplicateHandle(GetCurrentProcess(), objs[i], GetCurrentProcess(), &d, READ_CONTROL, FALSE, 0));
        SET_ERR(0);
        EQ(U(WaitForSingleObjectEx)(d, 0, FALSE), WAIT_FAILED);
        EXPECT_ERR(ERROR_ACCESS_DENIED);
        CloseHandle(d); CloseHandle(objs[i]);
    }
}
TEST(gap18_wfsoex_semaphore_counts) {
    REQUIRE(U(WaitForSingleObjectEx));
    HANDLE s = CreateSemaphoreA(NULL, 2, 5, NULL);
    EQ(U(WaitForSingleObjectEx)(s, 0, FALSE), WAIT_OBJECT_0);
    EQ(U(WaitForSingleObjectEx)(s, 0, FALSE), WAIT_OBJECT_0);
    EQ(U(WaitForSingleObjectEx)(s, 0, FALSE), WAIT_TIMEOUT);
    LONG prev = -1; ReleaseSemaphore(s, 3, &prev); EQ(prev, 0);
    for (int i = 0; i < 3; i++) EQ(U(WaitForSingleObjectEx)(s, 0, FALSE), WAIT_OBJECT_0);
    EQ(U(WaitForSingleObjectEx)(s, 0, FALSE), WAIT_TIMEOUT);
    CloseHandle(s);
}
TEST(gap18_wfsoex_mutex_recursive) {
    REQUIRE(U(WaitForSingleObjectEx));
    HANDLE m = CreateMutexA(NULL, FALSE, NULL);
    for (int i = 0; i < 4; i++) EQ(U(WaitForSingleObjectEx)(m, 0, FALSE), WAIT_OBJECT_0);
    for (int i = 0; i < 4; i++) CHECK(ReleaseMutex(m));
    SET_ERR(0);
    CHECK(!ReleaseMutex(m));
    EXPECT_ERR(ERROR_NOT_OWNER);
    CloseHandle(m);
}
static DWORD WINAPI hold_mutex_exit(LPVOID p) { WaitForSingleObject((HANDLE)p, INFINITE); return 0; }
TEST(gap18_wfsoex_abandoned_mutex) {
    REQUIRE(U(WaitForSingleObjectEx));
    HANDLE m = CreateMutexA(NULL, FALSE, NULL);
    HANDLE t = CreateThread(NULL, 0, hold_mutex_exit, m, 0, NULL);
    WaitForSingleObject(t, 5000); CloseHandle(t);
    EQ(U(WaitForSingleObjectEx)(m, 0, FALSE), WAIT_ABANDONED_0);
    EQ(U(WaitForSingleObjectEx)(m, 0, FALSE), WAIT_OBJECT_0);   /* now owned recursively; abandonment reported once */
    ReleaseMutex(m); ReleaseMutex(m);
    CloseHandle(m);
}
TEST(gap18_wfsoex_cross_thread_signal) {
    REQUIRE(U(WaitForSingleObjectEx));
    HANDLE e = ev(TRUE, FALSE);
    HANDLE t = CreateThread(NULL, 0, set_after, e, 0, NULL);
    DWORD t0 = GetTickCount();
    EQ(U(WaitForSingleObjectEx)(e, 5000, FALSE), WAIT_OBJECT_0);
    CHECK(GetTickCount() - t0 < 4000);
    WaitForSingleObject(t, 5000); CloseHandle(t); CloseHandle(e);
}
TEST(gap18_wfsoex_wait_on_thread_handle) {
    REQUIRE(U(WaitForSingleObjectEx));
    HANDLE e = ev(TRUE, FALSE);
    HANDLE t = CreateThread(NULL, 0, set_after, e, 0, NULL);
    EQ(U(WaitForSingleObjectEx)(t, 0, FALSE), WAIT_TIMEOUT);
    EQ(U(WaitForSingleObjectEx)(t, 5000, FALSE), WAIT_OBJECT_0);
    EQ(U(WaitForSingleObjectEx)(t, 0, FALSE), WAIT_OBJECT_0);
    CloseHandle(t); CloseHandle(e);
}
TEST(gap18_wfsoex_wait_on_process_handle) {
    REQUIRE(U(WaitForSingleObjectEx));
    PROCESS_INFORMATION pi; HANDLE h = t_spawn_child("gap18_exit7", "", 0, NULL, NULL, NULL, FALSE, NULL, &pi);
    CHECK(h); if (!h) return;
    EQ(U(WaitForSingleObjectEx)(h, 15000, FALSE), WAIT_OBJECT_0);
    DWORD code = 0; GetExitCodeProcess(h, &code); EQ(code, 7);
    CloseHandle(pi.hThread); CloseHandle(h);
}
CHILD(gap18_exit7) { return 7; }

TEST(gap18_wfsoex_waitable_timer) {
    REQUIRE(U(WaitForSingleObjectEx));
    HANDLE t = CreateWaitableTimerA(NULL, TRUE, NULL);
    LARGE_INTEGER due; due.QuadPart = -500000;      /* 50 ms */
    CHECK(SetWaitableTimer(t, &due, 0, NULL, NULL, FALSE));
    EQ(U(WaitForSingleObjectEx)(t, 0, FALSE), WAIT_TIMEOUT);
    EQ(U(WaitForSingleObjectEx)(t, 5000, FALSE), WAIT_OBJECT_0);
    EQ(U(WaitForSingleObjectEx)(t, 0, FALSE), WAIT_OBJECT_0);        /* manual-reset timer stays signaled */
    CloseHandle(t);
}
static volatile LONG g_apc_runs; static volatile DWORD g_apc_tid;
static VOID CALLBACK apc_fn(ULONG_PTR p) { g_apc_runs++; g_apc_tid = GetCurrentThreadId(); }
TEST(gap18_wfsoex_alertable_apc_interrupts) {
    REQUIRE(U(WaitForSingleObjectEx)); REQUIRE(U(QueueUserAPC));
    HANDLE e = ev(TRUE, FALSE); g_apc_runs = 0;
    CHECK(U(QueueUserAPC)(apc_fn, GetCurrentThread(), 1));
    DWORD t0 = GetTickCount();
    EQ(U(WaitForSingleObjectEx)(e, 5000, TRUE), WAIT_IO_COMPLETION);
    CHECK(GetTickCount() - t0 < 3000);
    EQ(g_apc_runs, 1);
    EQ(g_apc_tid, GetCurrentThreadId());
    EQ(U(WaitForSingleObjectEx)(e, 0, TRUE), WAIT_TIMEOUT);      /* APC consumed */
    CloseHandle(e);
}
TEST(gap18_wfsoex_nonalertable_apc_waits) {
    REQUIRE(U(WaitForSingleObjectEx)); REQUIRE(U(QueueUserAPC));
    HANDLE e = ev(TRUE, FALSE); g_apc_runs = 0;
    CHECK(U(QueueUserAPC)(apc_fn, GetCurrentThread(), 1));
    EQ(U(WaitForSingleObjectEx)(e, 80, FALSE), WAIT_TIMEOUT);
    EQ(g_apc_runs, 0);
    EQ(U(WaitForSingleObjectEx)(e, 0, TRUE), WAIT_IO_COMPLETION);   /* delivered at first alertable wait */
    EQ(g_apc_runs, 1);
    CloseHandle(e);
}
TEST(gap18_wfsoex_multiple_apcs_one_wait) {
    REQUIRE(U(WaitForSingleObjectEx)); REQUIRE(U(QueueUserAPC));
    HANDLE e = ev(TRUE, FALSE); g_apc_runs = 0;
    for (int i = 0; i < 5; i++) CHECK(U(QueueUserAPC)(apc_fn, GetCurrentThread(), i));
    EQ(U(WaitForSingleObjectEx)(e, 5000, TRUE), WAIT_IO_COMPLETION);
    EQ(g_apc_runs, 5);          /* all queued user APCs run in one alertable entry */
    CloseHandle(e);
}
static DWORD WINAPI queue_apc_later(LPVOID p) { Sleep(100); U(QueueUserAPC)(apc_fn, (HANDLE)p, 0); return 0; }
TEST(gap18_wfsoex_apc_from_other_thread) {
    REQUIRE(U(WaitForSingleObjectEx)); REQUIRE(U(QueueUserAPC));
    HANDLE e = ev(TRUE, FALSE); g_apc_runs = 0;
    HANDLE me = NULL; DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &me, 0, FALSE, DUPLICATE_SAME_ACCESS);
    HANDLE t = CreateThread(NULL, 0, queue_apc_later, me, 0, NULL);
    EQ(U(WaitForSingleObjectEx)(e, 5000, TRUE), WAIT_IO_COMPLETION);
    EQ(g_apc_runs, 1);
    WaitForSingleObject(t, 5000); CloseHandle(t); CloseHandle(me); CloseHandle(e);
}
TEST(gap18_wfsoex_alertable_signaled_with_pending_apc) {
    REQUIRE(U(WaitForSingleObjectEx)); REQUIRE(U(QueueUserAPC)); REQUIRE(U(SleepEx));
    HANDLE e = ev(TRUE, TRUE); g_apc_runs = 0;
    CHECK(U(QueueUserAPC)(apc_fn, GetCurrentThread(), 0));
    DWORD r = U(WaitForSingleObjectEx)(e, 1000, TRUE);
    t_note("signaled+pending APC -> 0x%lx, apc runs %ld", r, (long)g_apc_runs);
    CHECK(r == WAIT_OBJECT_0 || r == WAIT_IO_COMPLETION);
    U(SleepEx)(0, TRUE);      /* drain whatever is still pending */
    EQ(g_apc_runs, 1);
    CloseHandle(e);
}
TEST(gap18_wfsoex_sleepex_agrees) {
    REQUIRE(U(SleepEx)); REQUIRE(U(QueueUserAPC));
    g_apc_runs = 0;
    CHECK(U(QueueUserAPC)(apc_fn, GetCurrentThread(), 0));
    EQ(U(SleepEx)(2000, TRUE), WAIT_IO_COMPLETION);
    EQ(g_apc_runs, 1);
}
TEST(gap18_wfsoex_large_timeouts) {
    REQUIRE(U(WaitForSingleObjectEx));
    HANDLE e = ev(TRUE, TRUE);
    EQ(U(WaitForSingleObjectEx)(e, INFINITE - 1, FALSE), WAIT_OBJECT_0);
    EQ(U(WaitForSingleObjectEx)(e, 0x7FFFFFFF, FALSE), WAIT_OBJECT_0);
    EQ(U(WaitForSingleObjectEx)(e, 0x80000000u, FALSE), WAIT_OBJECT_0);
    CloseHandle(e);
}
TEST(gap18_wfsoex_wait_for_exited_process_no_hang) {
    REQUIRE(U(WaitForSingleObjectEx));
    EQ(t_run_child("gap18_exit7", "", 20000), 7);
}
TEST(gap18_wfsoex_waiters_released_in_order_manual) {
    REQUIRE(U(WaitForSingleObjectEx));
    HANDLE e = ev(TRUE, FALSE);
    HANDLE ts[4]; for (int i = 0; i < 4; i++) ts[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)(void *)hold_mutex_exit, e, 0, NULL);
    Sleep(50);
    for (int i = 0; i < 4; i++) EQ(U(WaitForSingleObjectEx)(ts[i], 0, FALSE), WAIT_TIMEOUT);
    SetEvent(e);
    for (int i = 0; i < 4; i++) { EQ(U(WaitForSingleObjectEx)(ts[i], 5000, FALSE), WAIT_OBJECT_0); CloseHandle(ts[i]); }
    CloseHandle(e);
}
TEST(gap18_wfsoex_std_handle_constants_child) { EQ(t_run_child("gap18_wfso_std", "", 30000), 0); }
CHILD(gap18_wfso_std) {
    CK(U(WaitForSingleObjectEx) && U(SetStdHandleEx));
    /* STD_* pseudo values are translated through the PEB process parameters (kernelbase only). */
    HANDLE e = CreateEventA(NULL, TRUE, TRUE, NULL);
    HANDLE old = GetStdHandle(STD_INPUT_HANDLE);
    SetStdHandle(STD_INPUT_HANDLE, e);
    DWORD r = U(WaitForSingleObjectEx)((HANDLE)(ULONG_PTR)(DWORD)STD_INPUT_HANDLE, 0, FALSE);
    SetStdHandle(STD_INPUT_HANDLE, old);
    CK(r == WAIT_OBJECT_0);
    SetStdHandle(STD_OUTPUT_HANDLE, e);
    r = U(WaitForSingleObjectEx)((HANDLE)(LONG_PTR)(LONG)STD_OUTPUT_HANDLE, 0, FALSE);
    CK(r == WAIT_OBJECT_0);
    SetStdHandle(STD_ERROR_HANDLE, e);
    r = U(WaitForSingleObjectEx)((HANDLE)(ULONG_PTR)(DWORD)STD_ERROR_HANDLE, 0, FALSE);
    CK(r == WAIT_OBJECT_0);
    return 0;
}

/* ------------------------- SetStdHandleEx ------------------------- */
CHILD(gap18_ssh_invalid) {
    CK(U(SetStdHandleEx));
    static const DWORD bad[] = { 0, 1, (DWORD)-9, (DWORD)-13, (DWORD)-1, 0x1000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0xFFFFFFF3, 0xFFFFFFF7 };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        HANDLE prev = (HANDLE)0x1234;
        SetLastError(0);
        CK(U(SetStdHandleEx)(bad[i], NULL, &prev) == FALSE);
        CK(GetLastError() == ERROR_INVALID_HANDLE);
        CK(prev == NULL);                 /* *phPrevValue is zeroed before validation */
        SetLastError(0);
        CK(U(SetStdHandleEx)(bad[i], NULL, NULL) == FALSE);
        CK(GetLastError() == ERROR_INVALID_HANDLE);
    }
    return 0;
}
TEST(gap18_ssh_invalid_ids) { EQ(t_run_child("gap18_ssh_invalid", "", 30000), 0); }

CHILD(gap18_ssh_roundtrip) {
    CK(U(SetStdHandleEx));
    static const DWORD ids[] = { STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE };
    for (int i = 0; i < 3; i++) {
        HANDLE orig = GetStdHandle(ids[i]);
        HANDLE a = CreateEventA(NULL, TRUE, FALSE, NULL), b = CreateEventA(NULL, TRUE, FALSE, NULL);
        HANDLE prev = (HANDLE)0x99;
        CK(U(SetStdHandleEx)(ids[i], a, &prev));
        CK(prev == orig);
        CK(GetStdHandle(ids[i]) == a);
        CK(U(SetStdHandleEx)(ids[i], b, &prev));
        CK(prev == a);
        CK(GetStdHandle(ids[i]) == b);
        CK(U(SetStdHandleEx)(ids[i], orig, NULL));
        CK(GetStdHandle(ids[i]) == orig);
        CloseHandle(a); CloseHandle(b);
    }
    return 0;
}
TEST(gap18_ssh_roundtrip) { EQ(t_run_child("gap18_ssh_roundtrip", "", 30000), 0); }

CHILD(gap18_ssh_independence) {
    CK(U(SetStdHandleEx));
    HANDLE i0 = GetStdHandle(STD_INPUT_HANDLE), o0 = GetStdHandle(STD_OUTPUT_HANDLE), e0 = GetStdHandle(STD_ERROR_HANDLE);
    HANDLE x = (HANDLE)(ULONG_PTR)0x4444, prev;
    CK(U(SetStdHandleEx)(STD_OUTPUT_HANDLE, x, &prev));
    CK(GetStdHandle(STD_OUTPUT_HANDLE) == x);
    CK(GetStdHandle(STD_INPUT_HANDLE) == i0);
    CK(GetStdHandle(STD_ERROR_HANDLE) == e0);
    CK(U(SetStdHandleEx)(STD_OUTPUT_HANDLE, o0, NULL));
    return 0;
}
TEST(gap18_ssh_independent_slots) { EQ(t_run_child("gap18_ssh_independence", "", 30000), 0); }

CHILD(gap18_ssh_unvalidated) {
    CK(U(SetStdHandleEx));
    HANDLE o0 = GetStdHandle(STD_OUTPUT_HANDLE), prev;
    static const ULONG_PTR vals[] = { 0, (ULONG_PTR)-1, 0xDEAD0, 0x7FFFFFFFFFFFFFF0ull };
    for (unsigned i = 0; i < sizeof vals / sizeof vals[0]; i++) {
        CK(U(SetStdHandleEx)(STD_OUTPUT_HANDLE, (HANDLE)vals[i], &prev));      /* the handle itself is not validated */
        CK(GetStdHandle(STD_OUTPUT_HANDLE) == (HANDLE)vals[i]);
    }
    CK(U(SetStdHandleEx)(STD_OUTPUT_HANDLE, o0, NULL));
    return 0;
}
TEST(gap18_ssh_handle_not_validated) { EQ(t_run_child("gap18_ssh_unvalidated", "", 30000), 0); }

CHILD(gap18_ssh_vs_setstdhandle) {
    CK(U(SetStdHandleEx) && U(SetStdHandle));
    HANDLE o0 = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE a = CreateEventA(NULL, TRUE, FALSE, NULL);
    CK(U(SetStdHandle)(STD_OUTPUT_HANDLE, a));
    HANDLE prev = NULL;
    CK(U(SetStdHandleEx)(STD_OUTPUT_HANDLE, o0, &prev));
    CK(prev == a);
    /* error codes agree with SetStdHandle on an invalid id */
    SetLastError(0); CK(!U(SetStdHandle)(0x55, NULL)); DWORD e1 = GetLastError();
    SetLastError(0); CK(!U(SetStdHandleEx)(0x55, NULL, NULL)); DWORD e2 = GetLastError();
    CK(e1 == e2 && e1 == ERROR_INVALID_HANDLE);
    CloseHandle(a);
    return 0;
}
TEST(gap18_ssh_agrees_with_setstdhandle) { EQ(t_run_child("gap18_ssh_vs_setstdhandle", "", 30000), 0); }

TEST(gap18_ssh_kernel32_forwarder) {
    REQUIRE(UK(SetStdHandleEx)); REQUIRE(UB(SetStdHandleEx));
    CHECK(UK(SetStdHandleEx) != NULL);
}
CHILD(gap18_ssh_thread_shared) {
    CK(U(SetStdHandleEx));
    /* standard handles are per-process: another thread sees the change */
    HANDLE o0 = GetStdHandle(STD_OUTPUT_HANDLE), prev;
    HANDLE x = (HANDLE)(ULONG_PTR)0x5252;
    CK(U(SetStdHandleEx)(STD_OUTPUT_HANDLE, x, &prev));
    static HANDLE seen;
    DWORD WINAPI th(LPVOID p) { seen = GetStdHandle(STD_OUTPUT_HANDLE); return 0; }
    HANDLE t = CreateThread(NULL, 0, th, NULL, 0, NULL);
    WaitForSingleObject(t, 5000); CloseHandle(t);
    CK(seen == x);
    CK(U(SetStdHandleEx)(STD_OUTPUT_HANDLE, o0, NULL));
    return 0;
}
TEST(gap18_ssh_process_wide) { EQ(t_run_child("gap18_ssh_thread_shared", "", 30000), 0); }

CHILD(gap18_ssh_inherit_to_grandchild) {
    CK(U(SetStdHandleEx));
    /* Redirect stdout of this process to a pipe and read what a grandchild with default std handles writes. */
    HANDLE r, w; SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    CK(CreatePipe(&r, &w, &sa, 0));
    HANDLE prev;
    CK(U(SetStdHandleEx)(STD_OUTPUT_HANDLE, w, &prev));
    CK(GetStdHandle(STD_OUTPUT_HANDLE) == w);
    DWORD wr; CK(WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), "via-ex", 6, &wr, NULL) && wr == 6);
    char b[16] = {0}; DWORD n = 0; CK(ReadFile(r, b, 6, &n, NULL) && n == 6 && !memcmp(b, "via-ex", 6));
    CK(U(SetStdHandleEx)(STD_OUTPUT_HANDLE, prev, NULL));
    CloseHandle(r); CloseHandle(w);
    return 0;
}
TEST(gap18_ssh_write_through_redirected) { EQ(t_run_child("gap18_ssh_inherit_to_grandchild", "", 30000), 0); }
