/* Threads terminated while suspended: never-run threads created suspended, and running threads parked
 * in a wait that were suspended (once or several times) before TerminateThread. */
#include "xpt.h"

static volatile LONG sapc_runs;
static void CALLBACK sapc(ULONG_PTR p) { InterlockedIncrement(&sapc_runs); }
static volatile LONG never_ran;
static DWORD WINAPI mark_ran(LPVOID p) { InterlockedIncrement(&never_ran); return 0x10; }

/* Never-run suspended thread, terminated; returns the handle (full access). */
static HANDLE term_never_run(DWORD code, DWORD *tid) {
    HANDLE h = CreateThread(NULL, 0, mark_ran, NULL, CREATE_SUSPENDED, tid);
    if (!h) return NULL;
    if (!U(TerminateThread)(h, code) || WaitForSingleObject(h, 5000) != WAIT_OBJECT_0) { ResumeThread(h); WaitForSingleObject(h, 5000); CloseHandle(h); return NULL; }
    return h;
}
/* Running worker parked in a wait, suspended n times, then terminated. Event in *go. */
static HANDLE term_parked(int n, DWORD code, xpt_tw *w) {
    if (!xpt_tw_start(w)) { xpt_tw_stop(w); return NULL; }
    for (int i = 0; i < n; i++) SuspendThread(w->h);
    if (!U(TerminateThread)(w->h, code)) return NULL;
    return w->h;
}

TEST(xpt_st_never_run_terminate_signals) {
    LONG before = never_ran;
    DWORD tid;
    HANDLE h = CreateThread(NULL, 0, mark_ran, NULL, CREATE_SUSPENDED, &tid);
    CHECK(h != NULL); if (!h) return;
    EQ(WaitForSingleObject(h, 0), WAIT_TIMEOUT);
    SUCCEEDS(U(TerminateThread)(h, 0x7001));
    EQ(WaitForSingleObject(h, 5000), WAIT_OBJECT_0);
    DWORD c = 0; SUCCEEDS(U(GetExitCodeThread)(h, &c)); EQ(c, 0x7001);
    EQ(never_ran, before);
    CloseHandle(h);
}
TEST(xpt_st_never_run_suspend_resume_after) {
    HANDLE h = term_never_run(0x7002, NULL);
    CHECK(h != NULL); if (!h) return;
    SetLastError(0); EQ(U(SuspendThread)(h), (DWORD)-1); EXPECT_ERR(ERROR_ACCESS_DENIED);
    EQ(U(ResumeThread)(h), 0);
    DWORD c = 0; GetExitCodeThread(h, &c); EQ(c, 0x7002);
    CloseHandle(h);
}
TEST(xpt_st_never_run_times) {
    HANDLE h = term_never_run(0x7003, NULL);
    CHECK(h != NULL); if (!h) return;
    FILETIME cr, ex, k, u;
    SUCCEEDS(U(GetThreadTimes)(h, &cr, &ex, &k, &u));
    ULONGLONG c = ((ULONGLONG)cr.dwHighDateTime << 32) | cr.dwLowDateTime, e = ((ULONGLONG)ex.dwHighDateTime << 32) | ex.dwLowDateTime;
    NE(c, 0); NE(e, 0); CHECK(e >= c);
    EQ(u.dwLowDateTime | u.dwHighDateTime, 0);   /* never executed user code */
    CloseHandle(h);
}
TEST(xpt_st_never_run_query_matrix) {
    DWORD tid = 0;
    HANDLE h = term_never_run(0x7004, &tid);
    CHECK(h != NULL); if (!h) return;
    EQ(U(GetThreadId)(h), tid);
    EQ(U(GetProcessIdOfThread)(h), GetCurrentProcessId());
    EQ(U(GetThreadPriority)(h), THREAD_PRIORITY_NORMAL);
    ULONG64 cy; SUCCEEDS(U(QueryThreadCycleTime)(h, &cy));
    GROUP_AFFINITY g; SUCCEEDS(U(GetThreadGroupAffinity)(h, &g));
    BOOL b; SUCCEEDS(U(GetThreadPriorityBoost)(h, &b));
    BOOL io = 7; SUCCEEDS(U(GetThreadIOPendingFlag)(h, &io)); EQ(io, FALSE);
    if (UN(GetThreadDescription)) { PWSTR d = NULL; EQ(UN(GetThreadDescription)(h, &d), XPT_HR_NT_OK); if (d) { EQ(d[0], 0); LocalFree(d); } }
    CloseHandle(h);
}
/* Setters on a terminated never-run thread behave as on any exited thread: stored, not refused;
 * the context is unavailable. */
TEST(xpt_st_never_run_set_matrix) {
    HANDLE h = term_never_run(0x7005, NULL);
    CHECK(h != NULL); if (!h) return;
    SUCCEEDS(U(SetThreadPriority)(h, THREAD_PRIORITY_LOWEST));
    EQ(GetThreadPriority(h), THREAD_PRIORITY_LOWEST);
    DWORD_PTR pm, sm; GetProcessAffinityMask(GetCurrentProcess(), &pm, &sm);
    EQ(U(SetThreadAffinityMask)(h, pm), pm);
    PROCESSOR_NUMBER p = { 0, 0, 0 };
    SUCCEEDS(U(SetThreadIdealProcessorEx)(h, &p, NULL));
    CONTEXT cx; memset(&cx, 0, sizeof cx); cx.ContextFlags = CONTEXT_CONTROL;
    FAILS(U(GetThreadContext)(h, &cx), ERROR_GEN_FAILURE);
    if (UN(SetThreadDescription)) EQ(UN(SetThreadDescription)(h, L"x"), XPT_HR_NT_OK);
    CloseHandle(h);
}
TEST(xpt_st_never_run_apc_queued_before_terminate) {
    LONG before = sapc_runs, ran = never_ran;
    HANDLE h = CreateThread(NULL, 0, mark_ran, NULL, CREATE_SUSPENDED, NULL);
    CHECK(h != NULL); if (!h) return;
    SUCCEEDS(U(QueueUserAPC)(sapc, h, 1));
    SUCCEEDS(U(TerminateThread)(h, 0x7006));
    EQ(WaitForSingleObject(h, 5000), WAIT_OBJECT_0);
    EQ(sapc_runs, before);
    EQ(never_ran, ran);
    FAILS(U(QueueUserAPC)(sapc, h, 2), ERROR_GEN_FAILURE);
    CloseHandle(h);
}
TEST(xpt_st_terminate_twice_keeps_first_code) {
    HANDLE h = term_never_run(0x7007, NULL);
    CHECK(h != NULL); if (!h) return;
    SUCCEEDS(U(TerminateThread)(h, 0x7008));
    DWORD c = 0; GetExitCodeThread(h, &c); EQ(c, 0x7007);
    CloseHandle(h);
}
TEST(xpt_st_parked_suspended_once_terminate) {
    xpt_tw w; HANDLE h = term_parked(1, 0x7101, &w);
    CHECK(h != NULL); if (!h) { xpt_tw_stop(&w); return; }
    EQ(WaitForSingleObject(h, 5000), WAIT_OBJECT_0);
    DWORD c = 0; GetExitCodeThread(h, &c); EQ(c, 0x7101);
    EQ(U(ResumeThread)(h), 0);   /* the suspend count is gone with the thread */
    xpt_tw_stop(&w);
}
TEST(xpt_st_parked_suspended_thrice_terminate) {
    xpt_tw w; HANDLE h = term_parked(3, 0x7102, &w);
    CHECK(h != NULL); if (!h) { xpt_tw_stop(&w); return; }
    EQ(WaitForSingleObject(h, 5000), WAIT_OBJECT_0);
    DWORD c = 0; GetExitCodeThread(h, &c); EQ(c, 0x7102);
    SetLastError(0); EQ(U(SuspendThread)(h), (DWORD)-1); EXPECT_ERR(ERROR_ACCESS_DENIED);
    xpt_tw_stop(&w);
}
TEST(xpt_st_suspend_count_reported_before_terminate) {
    xpt_tw w; if (!xpt_tw_start(&w)) { CHECK(!"worker"); xpt_tw_stop(&w); return; }
    EQ(U(SuspendThread)(w.h), 0);
    EQ(U(SuspendThread)(w.h), 1);
    EQ(U(SuspendThread)(w.h), 2);
    EQ(U(ResumeThread)(w.h), 3);
    EQ(WaitForSingleObject(w.h, 0), WAIT_TIMEOUT);
    SUCCEEDS(U(TerminateThread)(w.h, 0x7103));
    EQ(WaitForSingleObject(w.h, 5000), WAIT_OBJECT_0);
    DWORD c = 0; GetExitCodeThread(w.h, &c); EQ(c, 0x7103);
    xpt_tw_stop(&w);
}
TEST(xpt_st_parked_query_after_terminate) {
    xpt_tw w; HANDLE h = term_parked(1, 0x7104, &w);
    CHECK(h != NULL); if (!h) { xpt_tw_stop(&w); return; }
    EQ(WaitForSingleObject(h, 5000), WAIT_OBJECT_0);
    FILETIME cr, ex, k, u; SUCCEEDS(U(GetThreadTimes)(h, &cr, &ex, &k, &u));
    NE(ex.dwLowDateTime | ex.dwHighDateTime, 0);
    EQ(U(GetThreadId)(h), w.tid);
    EQ(U(GetThreadPriority)(h), THREAD_PRIORITY_NORMAL);
    BOOL io = 7; SUCCEEDS(U(GetThreadIOPendingFlag)(h, &io)); EQ(io, FALSE);
    xpt_tw_stop(&w);
}
TEST(xpt_st_openthread_by_id_after_terminate) {
    DWORD tid = 0;
    HANDLE h = term_never_run(0x7009, &tid);
    CHECK(h != NULL); if (!h) return;
    HANDLE o = U(OpenThread)(THREAD_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, tid);
    CHECKF(o != NULL, "err=%lu", GetLastError());
    if (o) {
        EQ(WaitForSingleObject(o, 0), WAIT_OBJECT_0);
        DWORD c = 0; SUCCEEDS(U(GetExitCodeThread)(o, &c)); EQ(c, 0x7009);
        CloseHandle(o);
    }
    CloseHandle(h);
    EQ(xpt_poll_open_gone(1, tid), ERROR_INVALID_PARAMETER);
}
