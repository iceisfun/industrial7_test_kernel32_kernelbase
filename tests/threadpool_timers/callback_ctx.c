/* Callback-instance helpers used from inside threadpool callbacks. */
#include "tp.h"

#define NEED_INST() REQUIRE(U(SetEventWhenCallbackReturns)); REQUIRE(U(ReleaseSemaphoreWhenCallbackReturns)); \
    REQUIRE(U(ReleaseMutexWhenCallbackReturns)); REQUIRE(U(LeaveCriticalSectionWhenCallbackReturns)); \
    REQUIRE(U(FreeLibraryWhenCallbackReturns)); REQUIRE(U(CallbackMayRunLong)); \
    REQUIRE(U(DisassociateCurrentThreadFromCallback))

typedef struct {
    HANDLE target;          /* event / semaphore / mutex to be signalled on return */
    HANDLE inside_ev;       /* signalled by callback once registration is done */
    HANDLE gate;            /* callback waits on it before returning */
    volatile LONG inside_state;   /* result of a probe performed inside the callback */
    volatile LONG done;
    CRITICAL_SECTION *cs;
    HMODULE lib;
    LONG crel;
    volatile LONG mayrunlong, mayrunlong_err;
} icb;

/* ---- SetEventWhenCallbackReturns ---- */
static VOID NTAPI icb_set_event(PTP_CALLBACK_INSTANCE i, PVOID p) {
    icb *c = (icb *)p;
    void (WINAPI *f)(PTP_CALLBACK_INSTANCE, HANDLE) = (void *)t_proc("SetEventWhenCallbackReturns");
    f(i, c->target);
    c->inside_state = WaitForSingleObject(c->target, 0);     /* must still be non-signalled here */
    InterlockedIncrement(&c->done);
    SetEvent(c->inside_ev);
    if (c->gate) WaitForSingleObject(c->gate, 20000);
}
TEST(tp_cbctx_set_event_after_return_simple) {
    NEED_INST(); REQUIRE(U(TrySubmitThreadpoolCallback));
    icb c; memset(&c, 0, sizeof c);
    c.target = tp_ev(TRUE); c.inside_ev = tp_ev(FALSE); c.gate = tp_ev(TRUE);
    CHECK(U(TrySubmitThreadpoolCallback)(icb_set_event, &c, NULL));
    CHECK(tp_signaled(c.inside_ev, TP_WAIT_MS));
    CHECK(tp_not_signaled(c.target, 100));          /* callback still running: event not yet set */
    EXPECT_EQ(c.inside_state, WAIT_TIMEOUT);
    SetEvent(c.gate);
    CHECK(tp_signaled(c.target, TP_WAIT_MS));       /* set once the callback returned */
    CloseHandle(c.target); CloseHandle(c.inside_ev); CloseHandle(c.gate);
}
static VOID NTAPI icb_set_event_work(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_WORK w) { icb_set_event(i, p); }
static VOID NTAPI icb_set_event_timer(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_TIMER t) { icb_set_event(i, p); }
static VOID NTAPI icb_set_event_wait(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_WAIT w, TP_WAIT_RESULT r) { icb_set_event(i, p); }
TEST(tp_cbctx_set_event_after_return_work) {
    NEED_INST(); REQUIRE(U(CreateThreadpoolWork)); REQUIRE(U(SubmitThreadpoolWork));
    REQUIRE(U(WaitForThreadpoolWorkCallbacks)); REQUIRE(U(CloseThreadpoolWork));
    icb c; memset(&c, 0, sizeof c);
    c.target = tp_ev(TRUE); c.inside_ev = tp_ev(FALSE);
    PTP_WORK w = U(CreateThreadpoolWork)(icb_set_event_work, &c, NULL);
    U(SubmitThreadpoolWork)(w);
    CHECK(tp_signaled(c.target, TP_WAIT_MS));
    EXPECT_EQ(c.inside_state, WAIT_TIMEOUT);
    U(WaitForThreadpoolWorkCallbacks)(w, FALSE);
    U(CloseThreadpoolWork)(w);
    CloseHandle(c.target); CloseHandle(c.inside_ev);
}
TEST(tp_cbctx_set_event_after_return_timer) {
    NEED_INST(); REQUIRE(U(CreateThreadpoolTimer)); REQUIRE(U(SetThreadpoolTimer));
    REQUIRE(U(WaitForThreadpoolTimerCallbacks)); REQUIRE(U(CloseThreadpoolTimer));
    icb c; memset(&c, 0, sizeof c);
    c.target = tp_ev(TRUE); c.inside_ev = tp_ev(FALSE);
    PTP_TIMER t = U(CreateThreadpoolTimer)(icb_set_event_timer, &c, NULL);
    FILETIME ft; tp_rel_ft(&ft, 10);
    U(SetThreadpoolTimer)(t, &ft, 0, 0);
    CHECK(tp_signaled(c.target, TP_WAIT_MS));
    EXPECT_EQ(c.inside_state, WAIT_TIMEOUT);
    U(WaitForThreadpoolTimerCallbacks)(t, FALSE);
    U(CloseThreadpoolTimer)(t);
    CloseHandle(c.target); CloseHandle(c.inside_ev);
}
TEST(tp_cbctx_set_event_after_return_wait) {
    NEED_INST(); REQUIRE(U(CreateThreadpoolWait)); REQUIRE(U(SetThreadpoolWait));
    REQUIRE(U(WaitForThreadpoolWaitCallbacks)); REQUIRE(U(CloseThreadpoolWait));
    icb c; memset(&c, 0, sizeof c);
    c.target = tp_ev(TRUE); c.inside_ev = tp_ev(FALSE);
    HANDLE trig = tp_ev(FALSE);
    PTP_WAIT w = U(CreateThreadpoolWait)(icb_set_event_wait, &c, NULL);
    U(SetThreadpoolWait)(w, trig, NULL);
    SetEvent(trig);
    CHECK(tp_signaled(c.target, TP_WAIT_MS));
    EXPECT_EQ(c.inside_state, WAIT_TIMEOUT);
    U(WaitForThreadpoolWaitCallbacks)(w, FALSE);
    U(CloseThreadpoolWait)(w);
    CloseHandle(c.target); CloseHandle(c.inside_ev); CloseHandle(trig);
}
TEST(tp_cbctx_set_event_auto_reset_event) {
    NEED_INST(); REQUIRE(U(TrySubmitThreadpoolCallback));
    icb c; memset(&c, 0, sizeof c);
    c.target = tp_ev(FALSE); c.inside_ev = tp_ev(FALSE);
    CHECK(U(TrySubmitThreadpoolCallback)(icb_set_event, &c, NULL));
    CHECK(tp_signaled(c.target, TP_WAIT_MS));       /* the wake consumes the auto-reset event */
    CHECK(tp_not_signaled(c.target, 50));
    CloseHandle(c.target); CloseHandle(c.inside_ev);
}

/* ---- ReleaseSemaphoreWhenCallbackReturns ---- */
static VOID NTAPI icb_release_sem(PTP_CALLBACK_INSTANCE i, PVOID p) {
    icb *c = (icb *)p;
    void (WINAPI *f)(PTP_CALLBACK_INSTANCE, HANDLE, DWORD) = (void *)t_proc("ReleaseSemaphoreWhenCallbackReturns");
    f(i, c->target, c->crel);
    c->inside_state = WaitForSingleObject(c->target, 0);
    SetEvent(c->inside_ev);
    if (c->gate) WaitForSingleObject(c->gate, 20000);
}
TEST(tp_cbctx_release_semaphore_after_return) {
    NEED_INST(); REQUIRE(U(TrySubmitThreadpoolCallback));
    icb c; memset(&c, 0, sizeof c);
    c.target = tp_sem(0, 10); c.inside_ev = tp_ev(FALSE); c.gate = tp_ev(TRUE); c.crel = 3;
    CHECK(U(TrySubmitThreadpoolCallback)(icb_release_sem, &c, NULL));
    CHECK(tp_signaled(c.inside_ev, TP_WAIT_MS));
    EXPECT_EQ(c.inside_state, WAIT_TIMEOUT);        /* not released while the callback runs */
    CHECK(tp_not_signaled(c.target, 100));
    SetEvent(c.gate);
    for (int k = 0; k < 3; k++) CHECKF(tp_signaled(c.target, TP_WAIT_MS), "count %d", k);
    CHECK(tp_not_signaled(c.target, 100));          /* exactly three */
    CloseHandle(c.target); CloseHandle(c.inside_ev); CloseHandle(c.gate);
}
TEST(tp_cbctx_release_semaphore_single_count) {
    NEED_INST(); REQUIRE(U(TrySubmitThreadpoolCallback));
    icb c; memset(&c, 0, sizeof c);
    c.target = tp_sem(0, 1); c.inside_ev = tp_ev(FALSE); c.crel = 1;
    CHECK(U(TrySubmitThreadpoolCallback)(icb_release_sem, &c, NULL));
    CHECK(tp_signaled(c.target, TP_WAIT_MS));
    CHECK(tp_not_signaled(c.target, 50));
    CloseHandle(c.target); CloseHandle(c.inside_ev);
}

/* ---- ReleaseMutexWhenCallbackReturns ---- */
static VOID NTAPI icb_release_mutex(PTP_CALLBACK_INSTANCE i, PVOID p) {
    icb *c = (icb *)p;
    void (WINAPI *f)(PTP_CALLBACK_INSTANCE, HANDLE) = (void *)t_proc("ReleaseMutexWhenCallbackReturns");
    c->inside_state = WaitForSingleObject(c->target, 5000);   /* take ownership on the pool thread */
    f(i, c->target);
    SetEvent(c->inside_ev);
    if (c->gate) WaitForSingleObject(c->gate, 20000);
}
TEST(tp_cbctx_release_mutex_after_return) {
    NEED_INST(); REQUIRE(U(TrySubmitThreadpoolCallback));
    icb c; memset(&c, 0, sizeof c);
    c.target = CreateMutexA(NULL, FALSE, NULL); c.inside_ev = tp_ev(FALSE); c.gate = tp_ev(TRUE);
    CHECK(U(TrySubmitThreadpoolCallback)(icb_release_mutex, &c, NULL));
    CHECK(tp_signaled(c.inside_ev, TP_WAIT_MS));
    EXPECT_EQ(c.inside_state, WAIT_OBJECT_0);
    CHECK(WaitForSingleObject(c.target, 100) == WAIT_TIMEOUT);    /* owned by the callback thread */
    SetEvent(c.gate);
    DWORD r = WaitForSingleObject(c.target, TP_WAIT_MS);
    CHECK(r == WAIT_OBJECT_0);                                    /* released, not abandoned */
    if (r == WAIT_OBJECT_0) ReleaseMutex(c.target);
    CloseHandle(c.target); CloseHandle(c.inside_ev); CloseHandle(c.gate);
}

/* ---- LeaveCriticalSectionWhenCallbackReturns ---- */
static VOID NTAPI icb_leave_cs(PTP_CALLBACK_INSTANCE i, PVOID p) {
    icb *c = (icb *)p;
    void (WINAPI *f)(PTP_CALLBACK_INSTANCE, PCRITICAL_SECTION) = (void *)t_proc("LeaveCriticalSectionWhenCallbackReturns");
    EnterCriticalSection(c->cs);
    f(i, c->cs);
    SetEvent(c->inside_ev);
    if (c->gate) WaitForSingleObject(c->gate, 20000);
}
TEST(tp_cbctx_leave_critical_section_after_return) {
    NEED_INST(); REQUIRE(U(TrySubmitThreadpoolCallback));
    CRITICAL_SECTION cs; InitializeCriticalSection(&cs);
    icb c; memset(&c, 0, sizeof c);
    c.cs = &cs; c.inside_ev = tp_ev(FALSE); c.gate = tp_ev(TRUE);
    CHECK(U(TrySubmitThreadpoolCallback)(icb_leave_cs, &c, NULL));
    CHECK(tp_signaled(c.inside_ev, TP_WAIT_MS));
    CHECK(TryEnterCriticalSection(&cs) == FALSE);   /* held by the callback */
    SetEvent(c.gate);
    ULONGLONG end = tp_now() + TP_WAIT_MS; BOOL got = FALSE;
    while (tp_now() < end) { if (TryEnterCriticalSection(&cs)) { got = TRUE; break; } Sleep(5); }
    CHECK(got);                                     /* the pool left the section on the callback's behalf */
    if (got) LeaveCriticalSection(&cs);
    DeleteCriticalSection(&cs);
    CloseHandle(c.inside_ev); CloseHandle(c.gate);
}

/* ---- FreeLibraryWhenCallbackReturns ---- */
static VOID NTAPI icb_free_lib(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_WORK w) {
    icb *c = (icb *)p;
    void (WINAPI *f)(PTP_CALLBACK_INSTANCE, HMODULE) = (void *)t_proc("FreeLibraryWhenCallbackReturns");
    f(i, c->lib);
    c->inside_state = GetModuleHandleA("cabinet.dll") != NULL;     /* still loaded while running */
    SetEvent(c->inside_ev);
}
TEST(tp_cbctx_free_library_after_return) {
    NEED_INST(); REQUIRE(U(CreateThreadpoolWork)); REQUIRE(U(SubmitThreadpoolWork));
    REQUIRE(U(WaitForThreadpoolWorkCallbacks)); REQUIRE(U(CloseThreadpoolWork));
    const char *lib = "cabinet.dll";
    if (GetModuleHandleA(lib)) { t_skip("%s already loaded in this process", lib); return; }
    HMODULE h1 = LoadLibraryA(lib), h2 = LoadLibraryA(lib);        /* reference count 2 */
    if (!h1 || !h2) { t_skip("cannot load %s", lib); return; }
    icb c; memset(&c, 0, sizeof c);
    c.lib = h1; c.inside_ev = tp_ev(FALSE);
    PTP_WORK w = U(CreateThreadpoolWork)(icb_free_lib, &c, NULL);
    U(SubmitThreadpoolWork)(w);
    CHECK(tp_signaled(c.inside_ev, TP_WAIT_MS));
    EXPECT_EQ(c.inside_state, 1);
    U(WaitForThreadpoolWorkCallbacks)(w, FALSE);
    U(CloseThreadpoolWork)(w);
    CHECK(GetModuleHandleA(lib) != NULL);           /* the callback dropped one reference; we still hold one */
    FreeLibrary(h2);
    CHECK(GetModuleHandleA(lib) == NULL);           /* so the last release unloads it: the pool freed exactly one */
    CloseHandle(c.inside_ev);
}

/* ---- CallbackMayRunLong ---- */
static VOID NTAPI icb_maylong(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_WORK w) {
    icb *c = (icb *)p;
    BOOL (WINAPI *f)(PTP_CALLBACK_INSTANCE) = (void *)t_proc("CallbackMayRunLong");
    SetLastError(0);
    c->mayrunlong = f(i);
    c->mayrunlong_err = GetLastError();
    SetEvent(c->inside_ev);
}
TEST(tp_cbctx_may_run_long_returns_bool) {
    NEED_INST(); REQUIRE(U(CreateThreadpoolWork)); REQUIRE(U(SubmitThreadpoolWork));
    REQUIRE(U(WaitForThreadpoolWorkCallbacks)); REQUIRE(U(CloseThreadpoolWork));
    icb c; memset(&c, 0, sizeof c);
    c.inside_ev = tp_ev(FALSE); c.mayrunlong = 77;
    PTP_WORK w = U(CreateThreadpoolWork)(icb_maylong, &c, NULL);
    U(SubmitThreadpoolWork)(w);
    CHECK(tp_signaled(c.inside_ev, TP_WAIT_MS));
    U(WaitForThreadpoolWorkCallbacks)(w, FALSE);
    CHECK(c.mayrunlong == 0 || c.mayrunlong == 1);
    t_note("CallbackMayRunLong in the default pool: %ld (last error %ld)", (long)c.mayrunlong, (long)c.mayrunlong_err);
    U(CloseThreadpoolWork)(w);
    CloseHandle(c.inside_ev);
}
TEST(tp_cbctx_may_run_long_private_pool) {
    NEED_INST(); REQUIRE(U(CreateThreadpoolWork)); REQUIRE(U(SubmitThreadpoolWork));
    REQUIRE(U(WaitForThreadpoolWorkCallbacks)); REQUIRE(U(CloseThreadpoolWork));
    REQUIRE(U(CreateThreadpool)); REQUIRE(U(CloseThreadpool)); REQUIRE(U(SetThreadpoolThreadMaximum));
    PTP_POOL p = U(CreateThreadpool)(NULL);
    U(SetThreadpoolThreadMaximum)(p, 1);
    TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e); SetThreadpoolCallbackPool(&e, p);
    icb c; memset(&c, 0, sizeof c);
    c.inside_ev = tp_ev(FALSE); c.mayrunlong = 77;
    PTP_WORK w = U(CreateThreadpoolWork)(icb_maylong, &c, &e);
    U(SubmitThreadpoolWork)(w);
    CHECK(tp_signaled(c.inside_ev, TP_WAIT_MS));
    U(WaitForThreadpoolWorkCallbacks)(w, FALSE);
    CHECK(c.mayrunlong == 0 || c.mayrunlong == 1);
    t_note("CallbackMayRunLong in a max=1 pool: %ld (last error %ld)", (long)c.mayrunlong, (long)c.mayrunlong_err);
    U(CloseThreadpoolWork)(w);
    U(CloseThreadpool)(p);
    CloseHandle(c.inside_ev);
}
TEST(tp_cbctx_may_run_long_longfunction_env) {
    NEED_INST(); REQUIRE(U(CreateThreadpoolWork)); REQUIRE(U(SubmitThreadpoolWork));
    REQUIRE(U(WaitForThreadpoolWorkCallbacks)); REQUIRE(U(CloseThreadpoolWork));
    TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e); SetThreadpoolCallbackRunsLong(&e);
    icb c; memset(&c, 0, sizeof c);
    c.inside_ev = tp_ev(FALSE); c.mayrunlong = 77;
    PTP_WORK w = U(CreateThreadpoolWork)(icb_maylong, &c, &e);
    U(SubmitThreadpoolWork)(w);
    CHECK(tp_signaled(c.inside_ev, TP_WAIT_MS));
    U(WaitForThreadpoolWorkCallbacks)(w, FALSE);
    CHECK(c.mayrunlong == 0 || c.mayrunlong == 1);
    t_note("CallbackMayRunLong with a long-function environment: %ld (last error %ld)", (long)c.mayrunlong, (long)c.mayrunlong_err);
    U(CloseThreadpoolWork)(w);
    CloseHandle(c.inside_ev);
}

/* ---- DisassociateCurrentThreadFromCallback ---- */
static VOID NTAPI icb_disassoc(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_WORK w) {
    icb *c = (icb *)p;
    void (WINAPI *f)(PTP_CALLBACK_INSTANCE) = (void *)t_proc("DisassociateCurrentThreadFromCallback");
    f(i);
    SetEvent(c->inside_ev);
    WaitForSingleObject(c->gate, 20000);
    InterlockedIncrement(&c->done);
}
TEST(tp_cbctx_disassociate_lets_wait_return_early) {
    NEED_INST(); REQUIRE(U(CreateThreadpoolWork)); REQUIRE(U(SubmitThreadpoolWork));
    REQUIRE(U(WaitForThreadpoolWorkCallbacks)); REQUIRE(U(CloseThreadpoolWork));
    icb c; memset(&c, 0, sizeof c);
    c.inside_ev = tp_ev(FALSE); c.gate = tp_ev(TRUE);
    PTP_WORK w = U(CreateThreadpoolWork)(icb_disassoc, &c, NULL);
    U(SubmitThreadpoolWork)(w);
    CHECK(tp_signaled(c.inside_ev, TP_WAIT_MS));
    ULONGLONG t0 = tp_now();
    U(WaitForThreadpoolWorkCallbacks)(w, FALSE);   /* callback is still blocked, but disassociated */
    CHECK(tp_now() - t0 < 3000);
    EXPECT_EQ(c.done, 0);
    SetEvent(c.gate);
    CHECK(tp_wait_count(&c.done, 1, TP_WAIT_MS));
    Sleep(50);
    U(CloseThreadpoolWork)(w);
    CloseHandle(c.inside_ev); CloseHandle(c.gate);
}

/* ---- callback environment properties ---- */
static VOID NTAPI icb_props(PTP_CALLBACK_INSTANCE i, PVOID p) {
    icb *c = (icb *)p;
    c->inside_state = GetThreadPriority(GetCurrentThread());
    c->mayrunlong = *(volatile ULONG_PTR *)((BYTE *)NtCurrentTeb() + 0x1758) != 0;   /* TEB.ReservedForOle */
    c->mayrunlong_err = IsThreadAFiber();
    SetEvent(c->inside_ev);
}
TEST(tp_cbctx_pool_thread_properties) {
    REQUIRE(U(TrySubmitThreadpoolCallback));
    icb c; memset(&c, 0, sizeof c);
    c.inside_ev = tp_ev(FALSE);
    CHECK(U(TrySubmitThreadpoolCallback)(icb_props, &c, NULL));
    CHECK(tp_signaled(c.inside_ev, TP_WAIT_MS));
    EXPECT_EQ(c.inside_state, THREAD_PRIORITY_NORMAL);
    t_note("pool thread has COM state: %ld, is fiber: %ld", (long)c.mayrunlong, (long)c.mayrunlong_err);
    CloseHandle(c.inside_ev);
}

/* ---- priorities ---- */
typedef struct { tp_ctx *c; LONG id; } pr_tag;
static VOID NTAPI pr_cb(PTP_CALLBACK_INSTANCE i, PVOID p) {
    pr_tag *t = (pr_tag *)p;
    LONG k = InterlockedIncrement(&t->c->seq) - 1;
    if (k < 64) t->c->order[k] = t->id;
    InterlockedIncrement(&t->c->n);
}
TEST(tp_cbctx_priority_orders_queued_callbacks) {
    NEED_INST(); REQUIRE(U(TrySubmitThreadpoolCallback));
    REQUIRE(U(CreateThreadpool)); REQUIRE(U(CloseThreadpool)); REQUIRE(U(SetThreadpoolThreadMaximum));
    PTP_POOL p = U(CreateThreadpool)(NULL);
    U(SetThreadpoolThreadMaximum)(p, 1);
    TP_CALLBACK_ENVIRON eb, el, en, eh;
    InitializeThreadpoolEnvironment(&eb); SetThreadpoolCallbackPool(&eb, p);
    el = en = eh = eb;
    SetThreadpoolCallbackPriority(&el, TP_CALLBACK_PRIORITY_LOW);
    SetThreadpoolCallbackPriority(&en, TP_CALLBACK_PRIORITY_NORMAL);
    SetThreadpoolCallbackPriority(&eh, TP_CALLBACK_PRIORITY_HIGH);
    tp_ctx blocker, c; tp_ctx_init(&blocker); tp_ctx_init(&c);
    CHECK(U(TrySubmitThreadpoolCallback)(tp_cb_gate, &blocker, &eb));
    CHECK(tp_signaled(blocker.ev, TP_WAIT_MS));
    pr_tag lo = { &c, 1 }, no = { &c, 2 }, hi = { &c, 3 };
    CHECK(U(TrySubmitThreadpoolCallback)(pr_cb, &lo, &el));
    CHECK(U(TrySubmitThreadpoolCallback)(pr_cb, &no, &en));
    CHECK(U(TrySubmitThreadpoolCallback)(pr_cb, &hi, &eh));
    Sleep(100);
    SetEvent(blocker.gate);
    CHECK(tp_wait_count(&c.n, 3, TP_WAIT_MS));
    t_note("execution order of low(1), normal(2), high(3): %ld %ld %ld", (long)c.order[0], (long)c.order[1], (long)c.order[2]);
    EXPECT_EQ(c.order[0], 3);                       /* high first */
    EXPECT_EQ(c.order[2], 1);                       /* low last */
    U(CloseThreadpool)(p);
    tp_ctx_free(&blocker); tp_ctx_free(&c);
}
