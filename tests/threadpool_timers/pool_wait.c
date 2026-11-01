/* Threadpool wait objects. */
#include "tp.h"

#define NEED_WAIT() REQUIRE(U(CreateThreadpoolWait)); REQUIRE(U(SetThreadpoolWait)); \
    REQUIRE(U(WaitForThreadpoolWaitCallbacks)); REQUIRE(U(CloseThreadpoolWait))

static void wt_cancel_close(PTP_WAIT w) {
    U(SetThreadpoolWait)(w, NULL, NULL);
    U(WaitForThreadpoolWaitCallbacks)(w, TRUE);
    U(CloseThreadpoolWait)(w);
}

TEST(tp_wait_create_close) {
    NEED_WAIT();
    tp_ctx c; tp_ctx_init(&c);
    SET_ERR(0);
    PTP_WAIT w = U(CreateThreadpoolWait)(tp_wait_cbcount, &c, NULL);
    CHECK(w != NULL);
    U(CloseThreadpoolWait)(w);
    tp_ctx_free(&c);
}
TEST(tp_wait_create_close_loop) {
    NEED_WAIT();
    tp_ctx c; tp_ctx_init(&c);
    for (int i = 0; i < 1000; i++) {
        PTP_WAIT w = U(CreateThreadpoolWait)(tp_wait_cbcount, &c, NULL);
        if (!w) { CHECK(w != NULL); break; }
        U(CloseThreadpoolWait)(w);
    }
    EXPECT_EQ(c.n, 0);
    tp_ctx_free(&c);
}
TEST(tp_wait_signaled_result_wait_object_0) {
    NEED_WAIT();
    tp_ctx c; tp_ctx_init(&c);
    HANDLE ev = tp_ev(FALSE);
    PTP_WAIT w = U(CreateThreadpoolWait)(tp_wait_cbcount, &c, NULL);
    U(SetThreadpoolWait)(w, ev, NULL);
    CHECK(tp_not_signaled(c.ev, 100));
    SetEvent(ev);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    EXPECT_EQ(c.res, WAIT_OBJECT_0);
    CHECK(c.seen_obj == (PVOID)w);                /* callback receives the wait object */
    CHECK(c.seen_inst != NULL);
    CHECK(c.tid != GetCurrentThreadId());
    U(WaitForThreadpoolWaitCallbacks)(w, FALSE);
    U(CloseThreadpoolWait)(w);
    CloseHandle(ev); tp_ctx_free(&c);
}
TEST(tp_wait_timeout_result_wait_timeout) {
    NEED_WAIT();
    tp_ctx c; tp_ctx_init(&c);
    HANDLE ev = tp_ev(FALSE);
    PTP_WAIT w = U(CreateThreadpoolWait)(tp_wait_cbcount, &c, NULL);
    FILETIME ft; tp_rel_ft(&ft, 100);
    ULONGLONG t0 = tp_now();
    U(SetThreadpoolWait)(w, ev, &ft);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    CHECK(tp_now() - t0 >= 80);
    EXPECT_EQ(c.res, WAIT_TIMEOUT);
    U(WaitForThreadpoolWaitCallbacks)(w, FALSE);
    U(CloseThreadpoolWait)(w);
    CloseHandle(ev); tp_ctx_free(&c);
}
TEST(tp_wait_signal_before_timeout_wins) {
    NEED_WAIT();
    tp_ctx c; tp_ctx_init(&c);
    HANDLE ev = tp_ev(FALSE);
    PTP_WAIT w = U(CreateThreadpoolWait)(tp_wait_cbcount, &c, NULL);
    FILETIME ft; tp_rel_ft(&ft, 10000);
    U(SetThreadpoolWait)(w, ev, &ft);
    SetEvent(ev);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    EXPECT_EQ(c.res, WAIT_OBJECT_0);
    U(WaitForThreadpoolWaitCallbacks)(w, FALSE);
    U(CloseThreadpoolWait)(w);
    CloseHandle(ev); tp_ctx_free(&c);
}
TEST(tp_wait_absolute_timeout) {
    NEED_WAIT();
    tp_ctx c; tp_ctx_init(&c);
    HANDLE ev = tp_ev(FALSE);
    PTP_WAIT w = U(CreateThreadpoolWait)(tp_wait_cbcount, &c, NULL);
    FILETIME ft; tp_abs_ft(&ft, 150);
    ULONGLONG t0 = tp_now();
    U(SetThreadpoolWait)(w, ev, &ft);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    CHECK(tp_now() - t0 >= 100);
    EXPECT_EQ(c.res, WAIT_TIMEOUT);
    U(WaitForThreadpoolWaitCallbacks)(w, FALSE);
    U(CloseThreadpoolWait)(w);
    CloseHandle(ev); tp_ctx_free(&c);
}
TEST(tp_wait_zero_timeout_is_immediate) {
    NEED_WAIT();
    tp_ctx c; tp_ctx_init(&c);
    HANDLE ev = tp_ev(FALSE);
    PTP_WAIT w = U(CreateThreadpoolWait)(tp_wait_cbcount, &c, NULL);
    FILETIME ft = { 0, 0 };
    U(SetThreadpoolWait)(w, ev, &ft);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    EXPECT_EQ(c.res, WAIT_TIMEOUT);
    U(WaitForThreadpoolWaitCallbacks)(w, FALSE);
    U(CloseThreadpoolWait)(w);
    CloseHandle(ev); tp_ctx_free(&c);
}
TEST(tp_wait_null_timeout_is_infinite) {
    NEED_WAIT();
    tp_ctx c; tp_ctx_init(&c);
    HANDLE ev = tp_ev(FALSE);
    PTP_WAIT w = U(CreateThreadpoolWait)(tp_wait_cbcount, &c, NULL);
    U(SetThreadpoolWait)(w, ev, NULL);
    Sleep(300);
    EXPECT_EQ(c.n, 0);
    wt_cancel_close(w);
    CloseHandle(ev); tp_ctx_free(&c);
}
TEST(tp_wait_is_one_shot_until_rearmed) {
    NEED_WAIT();
    tp_ctx c; tp_ctx_init(&c);
    HANDLE ev = tp_ev(FALSE);
    PTP_WAIT w = U(CreateThreadpoolWait)(tp_wait_cbcount, &c, NULL);
    U(SetThreadpoolWait)(w, ev, NULL);
    SetEvent(ev);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    U(WaitForThreadpoolWaitCallbacks)(w, FALSE);
    SetEvent(ev);                                  /* not re-armed: nothing waits on ev */
    Sleep(200);
    EXPECT_EQ(c.n, 1);
    U(SetThreadpoolWait)(w, ev, NULL);             /* the still-signalled auto-reset event is consumed now */
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    U(WaitForThreadpoolWaitCallbacks)(w, FALSE);
    EXPECT_EQ(c.n, 2);
    U(CloseThreadpoolWait)(w);
    CloseHandle(ev); tp_ctx_free(&c);
}
TEST(tp_wait_manual_reset_event_already_signaled) {
    NEED_WAIT();
    tp_ctx c; tp_ctx_init(&c);
    HANDLE ev = tp_ev(TRUE);
    SetEvent(ev);
    PTP_WAIT w = U(CreateThreadpoolWait)(tp_wait_cbcount, &c, NULL);
    U(SetThreadpoolWait)(w, ev, NULL);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    EXPECT_EQ(c.res, WAIT_OBJECT_0);
    Sleep(150);
    EXPECT_EQ(c.n, 1);                             /* still one-shot though the event stays signalled */
    U(WaitForThreadpoolWaitCallbacks)(w, FALSE);
    U(CloseThreadpoolWait)(w);
    CloseHandle(ev); tp_ctx_free(&c);
}
TEST(tp_wait_null_handle_cancels) {
    NEED_WAIT();
    tp_ctx c; tp_ctx_init(&c);
    HANDLE ev = tp_ev(FALSE);
    PTP_WAIT w = U(CreateThreadpoolWait)(tp_wait_cbcount, &c, NULL);
    U(SetThreadpoolWait)(w, ev, NULL);
    U(SetThreadpoolWait)(w, NULL, NULL);
    SetEvent(ev);
    Sleep(250);
    EXPECT_EQ(c.n, 0);
    CHECK(tp_signaled(ev, 0));                     /* nobody consumed the event */
    U(CloseThreadpoolWait)(w);
    CloseHandle(ev); tp_ctx_free(&c);
}
TEST(tp_wait_rearm_with_other_handle_replaces) {
    NEED_WAIT();
    tp_ctx c; tp_ctx_init(&c);
    HANDLE e1 = tp_ev(FALSE), e2 = tp_ev(FALSE);
    PTP_WAIT w = U(CreateThreadpoolWait)(tp_wait_cbcount, &c, NULL);
    U(SetThreadpoolWait)(w, e1, NULL);
    U(SetThreadpoolWait)(w, e2, NULL);
    SetEvent(e1);
    Sleep(200);
    EXPECT_EQ(c.n, 0);                             /* e1 is no longer being waited on */
    SetEvent(e2);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    U(WaitForThreadpoolWaitCallbacks)(w, FALSE);
    EXPECT_EQ(c.n, 1);
    U(CloseThreadpoolWait)(w);
    CloseHandle(e1); CloseHandle(e2); tp_ctx_free(&c);
}
static volatile PVOID wt_got; static HANDLE wt_gev;
static VOID NTAPI wt_ctx_cb(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_WAIT w, TP_WAIT_RESULT r) { wt_got = p; SetEvent(wt_gev); }
TEST(tp_wait_context_identity) {
    NEED_WAIT();
    wt_gev = tp_ev(FALSE);
    PVOID ctxs[] = { NULL, (PVOID)1, (PVOID)0x123456789abcULL, (PVOID)(LONG_PTR)-1 };
    HANDLE ev = tp_ev(TRUE);
    SetEvent(ev);
    for (int i = 0; i < 4; i++) {
        wt_got = (PVOID)0x77;
        PTP_WAIT w = U(CreateThreadpoolWait)(wt_ctx_cb, ctxs[i], NULL);
        U(SetThreadpoolWait)(w, ev, NULL);
        CHECK(tp_signaled(wt_gev, TP_WAIT_MS));
        CHECKF(wt_got == ctxs[i], "i=%d got %p", i, wt_got);
        U(WaitForThreadpoolWaitCallbacks)(w, FALSE);
        U(CloseThreadpoolWait)(w);
    }
    CloseHandle(ev); CloseHandle(wt_gev);
}
TEST(tp_wait_semaphore_consumed) {
    NEED_WAIT();
    tp_ctx c; tp_ctx_init(&c);
    HANDLE sem = tp_sem(0, 5);
    PTP_WAIT w = U(CreateThreadpoolWait)(tp_wait_cbcount, &c, NULL);
    U(SetThreadpoolWait)(w, sem, NULL);
    ReleaseSemaphore(sem, 2, NULL);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    U(WaitForThreadpoolWaitCallbacks)(w, FALSE);
    { DWORD wr_ = WaitForSingleObject(sem, 0); EXPECT_EQ(wr_, WAIT_OBJECT_0); }        /* exactly one count was taken */
    { DWORD wr_ = WaitForSingleObject(sem, 0); EXPECT_EQ(wr_, WAIT_TIMEOUT); }
    U(CloseThreadpoolWait)(w);
    CloseHandle(sem); tp_ctx_free(&c);
}
static DWORD WINAPI wt_thread_body(LPVOID p) { WaitForSingleObject((HANDLE)p, 5000); return 0; }
TEST(tp_wait_on_thread_handle) {
    NEED_WAIT();
    tp_ctx c; tp_ctx_init(&c);
    HANDLE go = tp_ev(FALSE);
    HANDLE th = CreateThread(NULL, 0, wt_thread_body, go, 0, NULL);
    PTP_WAIT w = U(CreateThreadpoolWait)(tp_wait_cbcount, &c, NULL);
    U(SetThreadpoolWait)(w, th, NULL);
    Sleep(100);
    EXPECT_EQ(c.n, 0);
    SetEvent(go);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    EXPECT_EQ(c.res, WAIT_OBJECT_0);
    U(WaitForThreadpoolWaitCallbacks)(w, FALSE);
    U(CloseThreadpoolWait)(w);
    WaitForSingleObject(th, 5000); CloseHandle(th); CloseHandle(go); tp_ctx_free(&c);
}
static HANDLE g_abandon_mutex;
static volatile LONG g_veh_hits, g_veh_armed; static volatile ULONG_PTR g_veh_status;
/* The harness VEH longjmps out of the test on any 0xCxxxxxxx exception, so code after the raising call
   never runs: the status has to be printed here, before the harness handler sees the exception. */
static LONG CALLBACK wt_veh(PEXCEPTION_POINTERS ep) {
    if (g_veh_armed && ep->ExceptionRecord->ExceptionCode == 0xc000070a && ep->ExceptionRecord->NumberParameters > 0) {
        g_veh_armed = 0;
        g_veh_status = ep->ExceptionRecord->ExceptionInformation[0]; InterlockedIncrement(&g_veh_hits);
        t_note("SetThreadpoolWait on abandoned mutex raised 0xc000070a, NTSTATUS %08lx", (unsigned long)g_veh_status);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
static DWORD WINAPI wt_abandoner(LPVOID p) { g_abandon_mutex = CreateMutexA(NULL, TRUE, NULL); return 0; }
TEST(tp_wait_abandoned_mutex_result) {
    NEED_WAIT();
    tp_ctx c; tp_ctx_init(&c);
    HANDLE th = CreateThread(NULL, 0, wt_abandoner, NULL, 0, NULL);
    WaitForSingleObject(th, 5000); CloseHandle(th);   /* the owner exited holding the mutex */
    PTP_WAIT w = U(CreateThreadpoolWait)(tp_wait_cbcount, &c, NULL);
    /* diagnostic only: record the NTSTATUS carried by the handle exception, if any (p1b) */
    g_veh_armed = 1;
    PVOID veh = AddVectoredExceptionHandler(1, wt_veh);
    U(SetThreadpoolWait)(w, g_abandon_mutex, NULL);
    g_veh_armed = 0;   /* not reached if the exception fires (harness longjmp); the handler disarms itself then */
    if (veh) RemoveVectoredExceptionHandler(veh);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    if (t_is_wine() && c.res == WAIT_OBJECT_0) t_note("wine reports WAIT_OBJECT_0 for an abandoned mutex");
    else EXPECT_EQ(c.res, WAIT_ABANDONED_0);
    U(WaitForThreadpoolWaitCallbacks)(w, FALSE);
    U(CloseThreadpoolWait)(w);
    CloseHandle(g_abandon_mutex); tp_ctx_free(&c);
}
static VOID NTAPI wt_slow_cb(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_WAIT w, TP_WAIT_RESULT r) {
    tp_ctx *c = (tp_ctx *)p;
    InterlockedIncrement(&c->n);
    SetEvent(c->ev2);
    WaitForSingleObject(c->gate, 20000);
    InterlockedIncrement(&c->n2);
}
TEST(tp_wait_callbacks_wait_blocks_for_running) {
    NEED_WAIT();
    tp_ctx c; tp_ctx_init(&c);
    HANDLE ev = tp_ev(FALSE);
    PTP_WAIT w = U(CreateThreadpoolWait)(wt_slow_cb, &c, NULL);
    U(SetThreadpoolWait)(w, ev, NULL);
    SetEvent(ev);
    CHECK(tp_signaled(c.ev2, TP_WAIT_MS));
    tp_delay_arg da; HANDLE th = tp_delayed_set(&da, c.gate, 250);
    ULONGLONG t0 = tp_now();
    U(WaitForThreadpoolWaitCallbacks)(w, FALSE);
    CHECK(tp_now() - t0 >= 150);
    EXPECT_EQ(c.n2, 1);
    tp_join(th);
    U(CloseThreadpoolWait)(w);
    CloseHandle(ev); tp_ctx_free(&c);
}
TEST(tp_wait_callbacks_wait_idle_returns) {
    NEED_WAIT();
    tp_ctx c; tp_ctx_init(&c);
    PTP_WAIT w = U(CreateThreadpoolWait)(tp_wait_cbcount, &c, NULL);
    ULONGLONG t0 = tp_now();
    U(WaitForThreadpoolWaitCallbacks)(w, FALSE);
    U(WaitForThreadpoolWaitCallbacks)(w, TRUE);
    CHECK(tp_now() - t0 < 1000);
    U(CloseThreadpoolWait)(w);
    tp_ctx_free(&c);
}
TEST(tp_wait_wait_callbacks_cancel_drops_queued) {
    NEED_WAIT(); REQUIRE(U(CreateThreadpool)); REQUIRE(U(CloseThreadpool)); REQUIRE(U(SetThreadpoolThreadMaximum));
    REQUIRE(U(TrySubmitThreadpoolCallback));
    PTP_POOL p = U(CreateThreadpool)(NULL);
    U(SetThreadpoolThreadMaximum)(p, 1);
    TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e); SetThreadpoolCallbackPool(&e, p);
    tp_ctx blocker, c; tp_ctx_init(&blocker); tp_ctx_init(&c);
    CHECK(U(TrySubmitThreadpoolCallback)(tp_cb_gate, &blocker, &e));
    CHECK(tp_signaled(blocker.ev, TP_WAIT_MS));
    HANDLE ev = tp_ev(FALSE);
    PTP_WAIT w = U(CreateThreadpoolWait)(tp_wait_cbcount, &c, &e);
    U(SetThreadpoolWait)(w, ev, NULL);
    SetEvent(ev);
    Sleep(200);                                    /* satisfied; callback is queued behind the blocker */
    EXPECT_EQ(c.n, 0);
    U(WaitForThreadpoolWaitCallbacks)(w, TRUE);
    SetEvent(blocker.gate);
    CHECK(tp_wait_count(&blocker.n2, 1, TP_WAIT_MS));
    Sleep(150);
    EXPECT_EQ(c.n, 0);
    U(CloseThreadpoolWait)(w);
    U(CloseThreadpool)(p);
    CloseHandle(ev); tp_ctx_free(&blocker); tp_ctx_free(&c);
}
TEST(tp_wait_close_while_armed_never_fires) {
    NEED_WAIT();
    tp_ctx c; tp_ctx_init(&c);
    HANDLE ev = tp_ev(FALSE);
    PTP_WAIT w = U(CreateThreadpoolWait)(tp_wait_cbcount, &c, NULL);
    U(SetThreadpoolWait)(w, ev, NULL);
    U(CloseThreadpoolWait)(w);
    SetEvent(ev);
    Sleep(250);
    EXPECT_EQ(c.n, 0);
    CloseHandle(ev); tp_ctx_free(&c);
}
TEST(tp_wait_set_ex_basic) {
    NEED_WAIT(); REQUIRE(U(SetThreadpoolWaitEx));
    tp_ctx c; tp_ctx_init(&c);
    HANDLE ev = tp_ev(FALSE);
    PTP_WAIT w = U(CreateThreadpoolWait)(tp_wait_cbcount, &c, NULL);
    BOOL r1 = U(SetThreadpoolWaitEx)(w, ev, NULL, NULL);
    SetEvent(ev);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    EXPECT_EQ(c.res, WAIT_OBJECT_0);
    U(WaitForThreadpoolWaitCallbacks)(w, FALSE);
    BOOL r2 = U(SetThreadpoolWaitEx)(w, NULL, NULL, NULL);
    t_note("SetThreadpoolWaitEx returned %d (arm) and %d (cancel idle)", r1, r2);
    U(CloseThreadpoolWait)(w);
    CloseHandle(ev); tp_ctx_free(&c);
}
TEST(tp_wait_many_objects) {
    NEED_WAIT();
    enum { N = 100 };
    static PTP_WAIT ws[N]; static HANDLE evs[N];
    tp_ctx c; tp_ctx_init(&c); c.ev = NULL;
    for (int i = 0; i < N; i++) {
        evs[i] = tp_ev(FALSE);
        ws[i] = U(CreateThreadpoolWait)(tp_wait_cbcount, &c, NULL);
        CHECK(ws[i] != NULL);
        U(SetThreadpoolWait)(ws[i], evs[i], NULL);
    }
    for (int i = 0; i < N; i++) SetEvent(evs[i]);
    CHECK(tp_wait_count(&c.n, N, 20000));
    Sleep(50);
    EXPECT_EQ(c.n, N);
    for (int i = 0; i < N; i++) { U(WaitForThreadpoolWaitCallbacks)(ws[i], FALSE); U(CloseThreadpoolWait)(ws[i]); CloseHandle(evs[i]); }
    tp_ctx_free(&c);
}
static HANDLE rl_ev, rl_done; static volatile LONG rl_cnt;
static VOID NTAPI wt_rearm_cb(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_WAIT w, TP_WAIT_RESULT r) {
    void (WINAPI *sw)(PTP_WAIT, HANDLE, PFILETIME) = (void *)t_proc("SetThreadpoolWait");
    LONG n = InterlockedIncrement(&rl_cnt);
    if (n < 10) { sw(w, rl_ev, NULL); SetEvent(rl_ev); }
    else SetEvent(rl_done);
}
TEST(tp_wait_rearm_from_callback_loop) {
    NEED_WAIT();
    rl_ev = tp_ev(FALSE); rl_done = tp_ev(FALSE); rl_cnt = 0;
    PTP_WAIT w = U(CreateThreadpoolWait)(wt_rearm_cb, NULL, NULL);
    U(SetThreadpoolWait)(w, rl_ev, NULL);
    SetEvent(rl_ev);
    CHECK(tp_signaled(rl_done, TP_WAIT_MS));
    U(WaitForThreadpoolWaitCallbacks)(w, FALSE);
    EXPECT_EQ(rl_cnt, 10);
    U(CloseThreadpoolWait)(w);
    CloseHandle(rl_ev); CloseHandle(rl_done);
}
TEST(tp_wait_private_pool_runs_off_main_thread) {
    NEED_WAIT(); REQUIRE(U(CreateThreadpool)); REQUIRE(U(CloseThreadpool));
    PTP_POOL p = U(CreateThreadpool)(NULL);
    TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e); SetThreadpoolCallbackPool(&e, p);
    tp_ctx c; tp_ctx_init(&c);
    HANDLE ev = tp_ev(FALSE);
    PTP_WAIT w = U(CreateThreadpoolWait)(tp_wait_cbcount, &c, &e);
    U(SetThreadpoolWait)(w, ev, NULL);
    SetEvent(ev);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    CHECK(c.tid != GetCurrentThreadId());
    U(WaitForThreadpoolWaitCallbacks)(w, FALSE);
    U(CloseThreadpoolWait)(w);
    U(CloseThreadpool)(p);
    CloseHandle(ev); tp_ctx_free(&c);
}
