/* Exited thread whose handle is still open: which queries still answer, which setters are refused. */
#include "xpt.h"

static volatile LONG zapc_runs;
static void CALLBACK zapc(ULONG_PTR p) { InterlockedIncrement(&zapc_runs); }

#define DEAD(h, code) HANDLE h = xpt_dead_thread(code, NULL); CHECK(h != NULL); if (!h) return

TEST(xpt_zt_signaled_and_exit_code) {
    DEAD(h, 0x1234);
    EQ(U(WaitForSingleObject)(h, 0), WAIT_OBJECT_0);
    EQ(U(WaitForSingleObject)(h, 0), WAIT_OBJECT_0);   /* stays signaled */
    DWORD c = 0; SUCCEEDS(U(GetExitCodeThread)(h, &c)); EQ(c, 0x1234);
    CloseHandle(h);
}
TEST(xpt_zt_exitthread_code) {
    DWORD tid;
    HANDLE h = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleA("kernel32.dll"), "ExitThread"), (LPVOID)0xBEEF, 0, &tid);
    CHECK(h != NULL); if (!h) return;
    EQ(WaitForSingleObject(h, 10000), WAIT_OBJECT_0);
    DWORD c = 0; SUCCEEDS(U(GetExitCodeThread)(h, &c)); EQ(c, 0xBEEF);
    CloseHandle(h);
}
TEST(xpt_zt_exit_code_still_active_value) {
    /* A thread that returns STILL_ACTIVE is indistinguishable through GetExitCodeThread, but signaled. */
    DEAD(h, STILL_ACTIVE);
    DWORD c = 0; SUCCEEDS(U(GetExitCodeThread)(h, &c)); EQ(c, STILL_ACTIVE);
    EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
    CloseHandle(h);
}
TEST(xpt_zt_times_exit_time_set_and_frozen) {
    DEAD(h, 1);
    FILETIME cr, ex, k, u, cr2, ex2, k2, u2;
    SUCCEEDS(U(GetThreadTimes)(h, &cr, &ex, &k, &u));
    ULONGLONG c = ((ULONGLONG)cr.dwHighDateTime << 32) | cr.dwLowDateTime, e = ((ULONGLONG)ex.dwHighDateTime << 32) | ex.dwLowDateTime;
    NE(c, 0); NE(e, 0);
    CHECKF(e >= c, "exit %llu < creation %llu", e, c);
    Sleep(30);
    SUCCEEDS(U(GetThreadTimes)(h, &cr2, &ex2, &k2, &u2));
    CHECK(!memcmp(&cr, &cr2, sizeof cr) && !memcmp(&ex, &ex2, sizeof ex));
    CHECK(!memcmp(&k, &k2, sizeof k) && !memcmp(&u, &u2, sizeof u));
    CloseHandle(h);
}
TEST(xpt_zt_live_thread_exit_time_zero) {
    FILETIME cr, ex, k, u;
    SUCCEEDS(U(GetThreadTimes)(GetCurrentThread(), &cr, &ex, &k, &u));
    EQ(ex.dwLowDateTime | ex.dwHighDateTime, 0);
}
TEST(xpt_zt_cycle_time_frozen) {
    DEAD(h, 1);
    ULONG64 a = 0, b = 1;
    SUCCEEDS(U(QueryThreadCycleTime)(h, &a));
    Sleep(20);
    SUCCEEDS(U(QueryThreadCycleTime)(h, &b));
    EQ(a, b);
    CloseHandle(h);
}
TEST(xpt_zt_priority_kept) {
    HANDLE h = xpt_dead_thread_prepared(2, THREAD_PRIORITY_BELOW_NORMAL, NULL);
    CHECK(h != NULL); if (!h) return;
    SetLastError(0);
    EQ(U(GetThreadPriority)(h), THREAD_PRIORITY_BELOW_NORMAL);
    CloseHandle(h);
}
TEST(xpt_zt_priority_boost_query) {
    DEAD(h, 1);
    BOOL b = 7;
    SUCCEEDS(U(GetThreadPriorityBoost)(h, &b));
    EQ(b, FALSE);
    CloseHandle(h);
}
TEST(xpt_zt_group_affinity_query) {
    DEAD(h, 1);
    GROUP_AFFINITY g, me; memset(&g, 0xCC, sizeof g);
    SUCCEEDS(U(GetThreadGroupAffinity)(h, &g));
    GetThreadGroupAffinity(GetCurrentThread(), &me);
    EQ(g.Group, me.Group);
    EQ(g.Mask, me.Mask);
    CloseHandle(h);
}
TEST(xpt_zt_ideal_processor_query) {
    DEAD(h, 1);
    PROCESSOR_NUMBER p; memset(&p, 0xCC, sizeof p);
    SUCCEEDS(U(GetThreadIdealProcessorEx)(h, &p));
    EQ(p.Group, 0);
    SYSTEM_INFO si; GetSystemInfo(&si);
    CHECKF(p.Number < si.dwNumberOfProcessors, "ideal %u of %lu", p.Number, si.dwNumberOfProcessors);
    CloseHandle(h);
}
TEST(xpt_zt_description_kept) {
    REQUIRE(UN(GetThreadDescription));
    HANDLE h = xpt_dead_thread_prepared(3, THREAD_PRIORITY_NORMAL, NULL);
    CHECK(h != NULL); if (!h) return;
    PWSTR d = NULL;
    HRESULT hr = UN(GetThreadDescription)(h, &d);
    EQ(hr, XPT_HR_NT_OK);
    if (SUCCEEDED(hr) && d) { EXPECT_WSTR(d, L"xpt-dead"); LocalFree(d); }
    CloseHandle(h);
}
TEST(xpt_zt_description_empty_when_never_set) {
    REQUIRE(UN(GetThreadDescription));
    DEAD(h, 1);
    PWSTR d = NULL;
    HRESULT hr = UN(GetThreadDescription)(h, &d);
    EQ(hr, XPT_HR_NT_OK);
    CHECK(d != NULL);
    if (d) { EQ(d[0], 0); LocalFree(d); }
    CloseHandle(h);
}
TEST(xpt_zt_ids) {
    DWORD tid = 0;
    HANDLE h = xpt_dead_thread(1, &tid);
    CHECK(h != NULL); if (!h) return;
    EQ(U(GetThreadId)(h), tid);
    EQ(U(GetProcessIdOfThread)(h), GetCurrentProcessId());
    CloseHandle(h);
}
TEST(xpt_zt_io_pending_flag) {
    DEAD(h, 1);
    BOOL b = 7;
    SUCCEEDS(U(GetThreadIOPendingFlag)(h, &b));
    EQ(b, FALSE);
    CloseHandle(h);
}
TEST(xpt_zt_getthreadinformation_memprio) {
    DEAD(h, 1);
    MEMORY_PRIORITY_INFORMATION m = { 0xCC };
    SUCCEEDS(U(GetThreadInformation)(h, ThreadMemoryPriority, &m, sizeof m));
    EQ(m.MemoryPriority, MEMORY_PRIORITY_NORMAL);
    CloseHandle(h);
}
TEST(xpt_zt_getthreadcontext) {
    DEAD(h, 1);
    CONTEXT cx; memset(&cx, 0, sizeof cx); cx.ContextFlags = CONTEXT_CONTROL;
    FAILS(U(GetThreadContext)(h, &cx), ERROR_GEN_FAILURE);   /* STATUS_UNSUCCESSFUL */
    CloseHandle(h);
}
TEST(xpt_zt_suspendthread) {
    DEAD(h, 1);
    SetLastError(0);
    EQ(U(SuspendThread)(h), (DWORD)-1);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(h);
}
/* ResumeThread on an exited thread is not refused: it reports a previous suspend count of 0
 * and leaves the last error alone. */
TEST(xpt_zt_resumethread) {
    DEAD(h, 1);
    SetLastError(0xBEEF);
    EQ(U(ResumeThread)(h), 0);
    EXPECT_ERR(0xBEEF);
    EQ(U(ResumeThread)(h), 0);
    CloseHandle(h);
}
TEST(xpt_zt_setthreadcontext) {
    DEAD(h, 1);
    CONTEXT cx; memset(&cx, 0, sizeof cx); cx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    FAILS(U(SetThreadContext)(h, &cx), ERROR_GEN_FAILURE);
    CloseHandle(h);
}
/* The thread setters are not refused on an exited thread: the new value is stored and reads back. */
TEST(xpt_zt_setthreadpriority) {
    DEAD(h, 1);
    SUCCEEDS(U(SetThreadPriority)(h, THREAD_PRIORITY_ABOVE_NORMAL));
    EQ(GetThreadPriority(h), THREAD_PRIORITY_ABOVE_NORMAL);
    SUCCEEDS(U(SetThreadPriority)(h, THREAD_PRIORITY_TIME_CRITICAL));
    EQ(GetThreadPriority(h), THREAD_PRIORITY_TIME_CRITICAL);
    FAILS(U(SetThreadPriority)(h, 99), ERROR_INVALID_PARAMETER);   /* argument validation still applies */
    CloseHandle(h);
}
TEST(xpt_zt_setthreadpriorityboost) {
    DEAD(h, 1);
    SUCCEEDS(U(SetThreadPriorityBoost)(h, TRUE));
    BOOL b = 7; GetThreadPriorityBoost(h, &b); EQ(b, TRUE);
    CloseHandle(h);
}
TEST(xpt_zt_setthreadaffinitymask) {
    DEAD(h, 1);
    DWORD_PTR pm, sm; GetProcessAffinityMask(GetCurrentProcess(), &pm, &sm);
    DWORD_PTR low = pm & (~pm + 1);
    EQ(U(SetThreadAffinityMask)(h, low), pm);   /* returns the previous mask */
    EQ(U(SetThreadAffinityMask)(h, pm), low);
    CloseHandle(h);
}
TEST(xpt_zt_setthreadgroupaffinity) {
    DEAD(h, 1);
    GROUP_AFFINITY g; memset(&g, 0, sizeof g); GetThreadGroupAffinity(h, &g);
    GROUP_AFFINITY prev; memset(&prev, 0xCC, sizeof prev);
    SUCCEEDS(U(SetThreadGroupAffinity)(h, &g, &prev));
    EQ(prev.Mask, g.Mask); EQ(prev.Group, g.Group);
    CloseHandle(h);
}
TEST(xpt_zt_setthreadidealprocessorex) {
    DEAD(h, 1);
    SYSTEM_INFO si; GetSystemInfo(&si);
    PROCESSOR_NUMBER p = { 0, (BYTE)(si.dwNumberOfProcessors > 1 ? 1 : 0), 0 }, prev, now;
    SUCCEEDS(U(SetThreadIdealProcessorEx)(h, &p, &prev));
    SUCCEEDS(GetThreadIdealProcessorEx(h, &now));
    EQ(now.Number, p.Number);
    CloseHandle(h);
}
TEST(xpt_zt_setthreaddescription) {
    REQUIRE(UN(SetThreadDescription)); REQUIRE(UN(GetThreadDescription));
    HANDLE h = xpt_dead_thread_prepared(3, THREAD_PRIORITY_NORMAL, NULL);
    CHECK(h != NULL); if (!h) return;
    EQ(UN(SetThreadDescription)(h, L"later"), XPT_HR_NT_OK);
    PWSTR d = NULL;
    if (SUCCEEDED(UN(GetThreadDescription)(h, &d)) && d) { EXPECT_WSTR(d, L"later"); LocalFree(d); }
    CloseHandle(h);
}
TEST(xpt_zt_setthreadinformation_memprio) {
    DEAD(h, 1);
    MEMORY_PRIORITY_INFORMATION m = { MEMORY_PRIORITY_LOW };
    SUCCEEDS(U(SetThreadInformation)(h, ThreadMemoryPriority, &m, sizeof m));
    m.MemoryPriority = 0xCC;
    SUCCEEDS(GetThreadInformation(h, ThreadMemoryPriority, &m, sizeof m));
    EQ(m.MemoryPriority, MEMORY_PRIORITY_LOW);
    CloseHandle(h);
}
TEST(xpt_zt_setthreadinformation_powerthrottling) {
    DEAD(h, 1);
    THREAD_POWER_THROTTLING_STATE s = { THREAD_POWER_THROTTLING_CURRENT_VERSION, 0, 0 };
    SUCCEEDS(U(SetThreadInformation)(h, ThreadPowerThrottling, &s, sizeof s));
    CloseHandle(h);
}
TEST(xpt_zt_queueuserapc) {
    DEAD(h, 1);
    LONG before = zapc_runs;
    FAILS(U(QueueUserAPC)(zapc, h, 0), ERROR_GEN_FAILURE);
    SleepEx(0, TRUE);
    EQ(zapc_runs, before);
    CloseHandle(h);
}
/* An APC queued to a thread that exits without ever waiting alertably is discarded, never run. */
static DWORD WINAPI nonalert(LPVOID p) {
    HANDLE *ev = p;
    SetEvent(ev[0]);
    WaitForSingleObject(ev[1], 10000);
    return 0x60;
}
TEST(xpt_zt_pending_apc_discarded_at_exit) {
    HANDLE ev[2] = { CreateEventA(NULL, TRUE, FALSE, NULL), CreateEventA(NULL, TRUE, FALSE, NULL) };
    DWORD tid; HANDLE th = CreateThread(NULL, 0, nonalert, ev, 0, &tid);
    CHECK(th != NULL); if (!th) { CloseHandle(ev[0]); CloseHandle(ev[1]); return; }
    EQ(WaitForSingleObject(ev[0], 5000), WAIT_OBJECT_0);
    Sleep(20);
    LONG before = zapc_runs;
    SUCCEEDS(U(QueueUserAPC)(zapc, th, 1));
    SUCCEEDS(U(QueueUserAPC)(zapc, th, 2));
    Sleep(30);
    EQ(zapc_runs, before);
    SetEvent(ev[1]);
    EQ(WaitForSingleObject(th, 5000), WAIT_OBJECT_0);
    DWORD c = 0; GetExitCodeThread(th, &c); EQ(c, 0x60);
    EQ(zapc_runs, before);
    CloseHandle(th); CloseHandle(ev[0]); CloseHandle(ev[1]);
}
/* TerminateThread on an exited thread reports success but keeps the original exit code. */
TEST(xpt_zt_terminate_again) {
    DEAD(h, 0x1111);
    SUCCEEDS(U(TerminateThread)(h, 0x2222));
    DWORD c = 0; GetExitCodeThread(h, &c); EQ(c, 0x1111);
    CloseHandle(h);
}
TEST(xpt_zt_duplicate_handle_still_works) {
    DEAD(h, 0x4444);
    HANDLE d = NULL;
    SUCCEEDS(U(DuplicateHandle)(GetCurrentProcess(), h, GetCurrentProcess(), &d, THREAD_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, 0));
    if (d) {
        EQ(WaitForSingleObject(d, 0), WAIT_OBJECT_0);
        DWORD c = 0; SUCCEEDS(U(GetExitCodeThread)(d, &c)); EQ(c, 0x4444);
        EQ(xpt_access(d), THREAD_QUERY_LIMITED_INFORMATION | SYNCHRONIZE);
        CloseHandle(d);
    }
    CloseHandle(h);
}
/* The query matrix on an exited thread through a QUERY_LIMITED handle opened by id. */
TEST(xpt_zt_limited_handle_queries) {
    DWORD tid = 0;
    HANDLE h = xpt_dead_thread(0x55, &tid);
    CHECK(h != NULL); if (!h) return;
    HANDLE q = U(OpenThread)(THREAD_QUERY_LIMITED_INFORMATION, FALSE, tid);
    CHECKF(q != NULL, "err=%lu", GetLastError());
    if (q) {
        DWORD c = 0; SUCCEEDS(U(GetExitCodeThread)(q, &c)); EQ(c, 0x55);
        FILETIME a, b, d, e; SUCCEEDS(U(GetThreadTimes)(q, &a, &b, &d, &e));
        NE(b.dwLowDateTime | b.dwHighDateTime, 0);
        ULONG64 cy; SUCCEEDS(U(QueryThreadCycleTime)(q, &cy));
        EQ(U(GetThreadId)(q), tid);
        EQ(U(GetProcessIdOfThread)(q), GetCurrentProcessId());
        EQ(U(GetThreadPriority)(q), THREAD_PRIORITY_NORMAL);
        GROUP_AFFINITY g; SUCCEEDS(U(GetThreadGroupAffinity)(q, &g));
        SetLastError(0); EQ(U(WaitForSingleObject)(q, 0), WAIT_FAILED); EXPECT_ERR(ERROR_ACCESS_DENIED);
        CloseHandle(q);
    }
    CloseHandle(h);
}
/* Kernel and user time of a thread that only returned are tiny but valid. */
TEST(xpt_zt_cpu_times_small) {
    DEAD(h, 1);
    FILETIME a, b, k, u;
    SUCCEEDS(U(GetThreadTimes)(h, &a, &b, &k, &u));
    ULONGLONG kt = ((ULONGLONG)k.dwHighDateTime << 32) | k.dwLowDateTime, ut = ((ULONGLONG)u.dwHighDateTime << 32) | u.dwLowDateTime;
    CHECKF(kt + ut < 10000000ull, "k=%llu u=%llu", kt, ut);   /* well under 1 s */
    CloseHandle(h);
}
