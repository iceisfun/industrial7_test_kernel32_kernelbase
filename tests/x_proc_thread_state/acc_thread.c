/* Access-mask enforcement on thread handles: every API with a handle opened by OpenThread with exactly
 * one access right. The target is an own worker thread parked in a bounded, non-alertable wait. */
#include "xpt.h"

typedef struct { HANDLE full; DWORD tid; DWORD_PTR mask; GROUP_AFFINITY ga; PROCESSOR_NUMBER ideal; } tctx;
typedef int (*top)(HANDLE h, tctx *c);

static volatile LONG apc_runs;
static void CALLBACK apc_count(ULONG_PTR p) { InterlockedIncrement(&apc_runs); }

static int op_times(HANDLE h, tctx *c) { FILETIME a, b, d, e; return XB(U(GetThreadTimes)(h, &a, &b, &d, &e)); }
static int op_cycle(HANDLE h, tctx *c) { ULONG64 u; return XB(U(QueryThreadCycleTime)(h, &u)); }
static int op_exitcode(HANDLE h, tctx *c) { DWORD x; return XB(U(GetExitCodeThread)(h, &x)); }
static int op_getprio(HANDLE h, tctx *c) { return XB(U(GetThreadPriority)(h) != THREAD_PRIORITY_ERROR_RETURN); }
static int op_setprio(HANDLE h, tctx *c) { return XB(U(SetThreadPriority)(h, THREAD_PRIORITY_NORMAL)); }
static int op_getboost(HANDLE h, tctx *c) { BOOL b; return XB(U(GetThreadPriorityBoost)(h, &b)); }
static int op_setboost(HANDLE h, tctx *c) { return XB(U(SetThreadPriorityBoost)(h, FALSE)); }
static int op_setaff(HANDLE h, tctx *c) { return XB(U(SetThreadAffinityMask)(h, c->mask)); }
static int op_getgaff(HANDLE h, tctx *c) { GROUP_AFFINITY g; return XB(U(GetThreadGroupAffinity)(h, &g)); }
static int op_setgaff(HANDLE h, tctx *c) { return XB(U(SetThreadGroupAffinity)(h, &c->ga, NULL)); }
static int op_getideal(HANDLE h, tctx *c) { PROCESSOR_NUMBER p; return XB(U(GetThreadIdealProcessorEx)(h, &p)); }
static int op_setideal(HANDLE h, tctx *c) { PROCESSOR_NUMBER p = c->ideal; return XB(U(SetThreadIdealProcessorEx)(h, &p, NULL)); }
static int op_iopend(HANDLE h, tctx *c) { BOOL b; return XB(U(GetThreadIOPendingFlag)(h, &b)); }
static int op_getmem(HANDLE h, tctx *c) { MEMORY_PRIORITY_INFORMATION m; return XB(U(GetThreadInformation)(h, ThreadMemoryPriority, &m, sizeof m)); }
static int op_setmem(HANDLE h, tctx *c) { MEMORY_PRIORITY_INFORMATION m = { MEMORY_PRIORITY_NORMAL }; return XB(U(SetThreadInformation)(h, ThreadMemoryPriority, &m, sizeof m)); }
static int op_setthr(HANDLE h, tctx *c) { THREAD_POWER_THROTTLING_STATE s = { THREAD_POWER_THROTTLING_CURRENT_VERSION, 0, 0 }; return XB(U(SetThreadInformation)(h, ThreadPowerThrottling, &s, sizeof s)); }
static int op_getdesc(HANDLE h, tctx *c) {
    PWSTR p = NULL; HRESULT hr = UN(GetThreadDescription)(h, &p);
    if (SUCCEEDED(hr)) { LocalFree(p); xpt_err = 0; return 1; }
    xpt_err = (DWORD)hr; return 0;
}
static int op_setdesc(HANDLE h, tctx *c) {
    HRESULT hr = UN(SetThreadDescription)(h, L"xpt-acc");
    xpt_err = SUCCEEDED(hr) ? 0 : (DWORD)hr; return SUCCEEDED(hr);
}
static int op_tid(HANDLE h, tctx *c) { return XB(U(GetThreadId)(h)); }
static int op_pidof(HANDLE h, tctx *c) { return XB(U(GetProcessIdOfThread)(h)); }
static int op_suspend(HANDLE h, tctx *c) {
    int ok = XB(U(SuspendThread)(h) != (DWORD)-1);
    if (ok) ResumeThread(c->full);
    return ok;
}
static int op_resume(HANDLE h, tctx *c) {
    SuspendThread(c->full);
    int ok = XB(U(ResumeThread)(h) != (DWORD)-1);
    if (!ok) ResumeThread(c->full);
    return ok;
}
static int op_getctx(HANDLE h, tctx *c) {
    CONTEXT cx; memset(&cx, 0, sizeof cx); cx.ContextFlags = CONTEXT_CONTROL;
    SuspendThread(c->full);
    int ok = XB(U(GetThreadContext)(h, &cx));
    ResumeThread(c->full);
    return ok;
}
static int op_setctx(HANDLE h, tctx *c) {
    CONTEXT cx; memset(&cx, 0, sizeof cx); cx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    SuspendThread(c->full);
    int ok = 0;
    if (GetThreadContext(c->full, &cx)) ok = XB(U(SetThreadContext)(h, &cx));
    else xpt_err = 0xFFFFFFFF;
    ResumeThread(c->full);
    return ok;
}
static int op_apc(HANDLE h, tctx *c) { return XB(U(QueueUserAPC)(apc_count, h, 0)); }
static int op_wait(HANDLE h, tctx *c) { return XB(U(WaitForSingleObject)(h, 0) != WAIT_FAILED); }

static int tctx_init(tctx *c, xpt_tw *w) {
    memset(c, 0, sizeof *c);
    if (!xpt_tw_start(w)) { xpt_tw_stop(w); return 0; }
    c->full = w->h; c->tid = w->tid;
    DWORD_PTR sm; GetProcessAffinityMask(GetCurrentProcess(), &c->mask, &sm);
    GetThreadGroupAffinity(w->h, &c->ga);
    GetThreadIdealProcessorEx(w->h, &c->ideal);
    return 1;
}

/* Runs op once per single right; 'allowed' is the set of single rights that must succeed, the others
 * must fail with 'denied' (ERROR_ACCESS_DENIED, or an HRESULT for the description APIs). */
static void tmatrix(const char *api, top fn, DWORD allowed, DWORD denied) {
    xpt_tw w; tctx c;
    if (!tctx_init(&c, &w)) { CHECK(!"worker thread"); return; }
    for (int i = 0; i < XPT_NTR; i++) {
        DWORD r = xpt_trights[i].r;
        HANDLE h = U(OpenThread)(r, FALSE, c.tid);
        CHECKF(h != NULL, "%s: OpenThread(%s) err=%lu", api, xpt_trights[i].n, GetLastError());
        if (!h) continue;
        xpt_err = 0xFFFFFFFF;
        int ok = fn(h, &c);
        int want = (allowed & r) != 0;
        CHECKF(ok == want, "%s with %s: ok=%d want %d err=0x%lx", api, xpt_trights[i].n, ok, want, xpt_err);
        if (!ok && !want) CHECKF(xpt_err == denied, "%s with %s: err=0x%lx want 0x%lx", api, xpt_trights[i].n, xpt_err, denied);
        CloseHandle(h);
    }
    xpt_tw_stop(&w);
}
#define TM(name, fn, allowed) TEST(xpt_acc_thread_##name) { tmatrix(#name, fn, allowed, ERROR_ACCESS_DENIED); }

TM(getthreadtimes, op_times, TQ)
TM(querythreadcycletime, op_cycle, TQ)
TM(getexitcodethread, op_exitcode, TQ)
TM(getthreadpriority, op_getprio, TQ)
TM(setthreadpriority, op_setprio, TS)
TM(getthreadpriorityboost, op_getboost, TQ)
TM(setthreadpriorityboost, op_setboost, TS)
TM(setthreadaffinitymask, op_setaff, 0)
TM(getthreadgroupaffinity, op_getgaff, TQ)
TM(setthreadgroupaffinity, op_setgaff, THREAD_SET_INFORMATION)
TM(getthreadidealprocessorex, op_getideal, TQ)
TM(setthreadidealprocessorex, op_setideal, THREAD_SET_INFORMATION)
TM(getthreadiopendingflag, op_iopend, THREAD_QUERY_INFORMATION)
TM(getthreadinformation_memprio, op_getmem, TQ)
TM(setthreadinformation_memprio, op_setmem, THREAD_SET_INFORMATION)
TM(setthreadinformation_powerthrottling, op_setthr, THREAD_SET_INFORMATION)
TM(getthreadid, op_tid, TQ)
TM(getprocessidofthread, op_pidof, TQ)
TM(suspendthread, op_suspend, THREAD_SUSPEND_RESUME)
TM(resumethread, op_resume, THREAD_SUSPEND_RESUME)
TM(getthreadcontext, op_getctx, THREAD_GET_CONTEXT)
TM(setthreadcontext, op_setctx, THREAD_SET_CONTEXT)
TM(queueuserapc, op_apc, THREAD_SET_CONTEXT)
TM(wait, op_wait, SYNCHRONIZE)
TEST(xpt_acc_thread_getthreaddescription) { REQUIRE(UN(GetThreadDescription)); tmatrix("getthreaddescription", op_getdesc, TQ, (DWORD)XPT_HR_DENIED); }
TEST(xpt_acc_thread_setthreaddescription) { REQUIRE(UN(SetThreadDescription)); tmatrix("setthreaddescription", op_setdesc, TS, (DWORD)XPT_HR_DENIED); }

/* TerminateThread: every right but TERMINATE is refused, then a TERMINATE-only handle ends the thread. */
TEST(xpt_acc_thread_terminatethread) {
    xpt_tw w; if (!xpt_tw_start(&w)) { CHECK(!"worker"); xpt_tw_stop(&w); return; }
    for (int i = 0; i < XPT_NTR; i++) {
        if (xpt_trights[i].r == THREAD_TERMINATE) continue;
        HANDLE h = U(OpenThread)(xpt_trights[i].r, FALSE, w.tid);
        if (!h) { CHECK(h != NULL); continue; }
        SetLastError(0);
        BOOL r = U(TerminateThread)(h, 0x99);
        DWORD e = GetLastError();
        CHECKF(!r && e == ERROR_ACCESS_DENIED, "TerminateThread with %s: r=%d err=%lu", xpt_trights[i].n, r, e);
        CloseHandle(h);
    }
    EQ(WaitForSingleObject(w.h, 0), WAIT_TIMEOUT);
    HANDLE t = U(OpenThread)(THREAD_TERMINATE, FALSE, w.tid);
    CHECK(t != NULL);
    if (t) { SUCCEEDS(U(TerminateThread)(t, 0x99)); CloseHandle(t); }
    EQ(WaitForSingleObject(w.h, 5000), WAIT_OBJECT_0);
    DWORD code = 0; GetExitCodeThread(w.h, &code); EQ(code, 0x99);
    xpt_tw_stop(&w);
}

/* A QueueUserAPC that succeeded with a SET_CONTEXT-only handle runs at the next alertable wait. */
static DWORD WINAPI alert_wait(LPVOID p) {
    HANDLE *ev = p;
    SetEvent(ev[0]);
    DWORD r;
    int n = 0;
    do { r = WaitForSingleObjectEx(ev[1], 10000, TRUE); if (r == WAIT_IO_COMPLETION) n++; } while (r == WAIT_IO_COMPLETION && n < 4);
    return n;
}
TEST(xpt_acc_thread_apc_set_context_handle_delivers) {
    HANDLE ev[2] = { CreateEventA(NULL, TRUE, FALSE, NULL), CreateEventA(NULL, TRUE, FALSE, NULL) };
    DWORD tid; HANDLE th = CreateThread(NULL, 0, alert_wait, ev, CREATE_SUSPENDED, &tid);
    CHECK(th != NULL); if (!th) { CloseHandle(ev[0]); CloseHandle(ev[1]); return; }
    HANDLE h = U(OpenThread)(THREAD_SET_CONTEXT, FALSE, tid);
    CHECK(h != NULL);
    ResumeThread(th);
    EQ(WaitForSingleObject(ev[0], 5000), WAIT_OBJECT_0);
    Sleep(20);
    LONG before = apc_runs;
    if (h) { SUCCEEDS(U(QueueUserAPC)(apc_count, h, 1)); CloseHandle(h); }
    Sleep(50);
    SetEvent(ev[1]);
    EQ(WaitForSingleObject(th, 5000), WAIT_OBJECT_0);
    DWORD code = 0; GetExitCodeThread(th, &code);
    EQ(code, 1);
    EQ(apc_runs - before, 1);
    CloseHandle(th); CloseHandle(ev[0]); CloseHandle(ev[1]);
}

/* OpenThread with no access at all is refused; a zero-access duplicate exists and is refused by everything. */
TEST(xpt_acc_thread_zero_access_handle) {
    xpt_tw w; if (!xpt_tw_start(&w)) { CHECK(!"worker"); xpt_tw_stop(&w); return; }
    FAILS(U(OpenThread)(0, FALSE, w.tid), ERROR_ACCESS_DENIED);
    HANDLE h = NULL;
    SUCCEEDS(U(DuplicateHandle)(GetCurrentProcess(), w.h, GetCurrentProcess(), &h, 0, FALSE, 0));
    if (h) {
        EQ(xpt_access(h), 0);
        DWORD x = 0x1234;
        FAILS(U(GetExitCodeThread)(h, &x), ERROR_ACCESS_DENIED);
        EQ(x, 0x1234);
        FAILS(U(GetThreadId)(h), ERROR_ACCESS_DENIED);
        SetLastError(0); EQ(U(GetThreadPriority)(h), THREAD_PRIORITY_ERROR_RETURN); EXPECT_ERR(ERROR_ACCESS_DENIED);
        SetLastError(0); EQ(U(WaitForSingleObject)(h, 0), WAIT_FAILED); EXPECT_ERR(ERROR_ACCESS_DENIED);
        CloseHandle(h);
    }
    xpt_tw_stop(&w);
}
