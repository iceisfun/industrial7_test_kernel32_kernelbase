/* Threadpool timer objects. */
#include "tp.h"

#define NEED_TIMER() REQUIRE(U(CreateThreadpoolTimer)); REQUIRE(U(SetThreadpoolTimer)); \
    REQUIRE(U(WaitForThreadpoolTimerCallbacks)); REQUIRE(U(CloseThreadpoolTimer)); REQUIRE(U(IsThreadpoolTimerSet))

static void tm_cancel_close(PTP_TIMER t) {
    U(SetThreadpoolTimer)(t, NULL, 0, 0);
    U(WaitForThreadpoolTimerCallbacks)(t, TRUE);
    U(CloseThreadpoolTimer)(t);
}

TEST(tp_timer_create_close) {
    NEED_TIMER();
    tp_ctx c; tp_ctx_init(&c);
    SET_ERR(0);
    PTP_TIMER t = U(CreateThreadpoolTimer)(tp_timer_count, &c, NULL);
    CHECK(t != NULL);
    U(CloseThreadpoolTimer)(t);
    tp_ctx_free(&c);
}
TEST(tp_timer_create_close_loop) {
    NEED_TIMER();
    tp_ctx c; tp_ctx_init(&c);
    for (int i = 0; i < 1000; i++) {
        PTP_TIMER t = U(CreateThreadpoolTimer)(tp_timer_count, &c, NULL);
        if (!t) { CHECK(t != NULL); break; }
        U(CloseThreadpoolTimer)(t);
    }
    EXPECT_EQ(c.n, 0);
    tp_ctx_free(&c);
}
TEST(tp_timer_relative_due_fires_once) {
    NEED_TIMER();
    tp_ctx c; tp_ctx_init(&c);
    PTP_TIMER t = U(CreateThreadpoolTimer)(tp_timer_count, &c, NULL);
    FILETIME ft; tp_rel_ft(&ft, 100);
    ULONGLONG t0 = tp_now();
    U(SetThreadpoolTimer)(t, &ft, 0, 0);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    ULONGLONG dt = tp_now() - t0;
    CHECK(dt >= 80);
    t_note("100ms relative timer fired after %llu ms", dt);
    CHECK(c.seen_obj == (PVOID)t);               /* the callback receives the timer object */
    CHECK(c.seen_inst != NULL);
    CHECK(c.tid != GetCurrentThreadId());
    Sleep(300);
    EXPECT_EQ(c.n, 1);                           /* one-shot: no repeat */
    U(WaitForThreadpoolTimerCallbacks)(t, FALSE);
    U(CloseThreadpoolTimer)(t);
    tp_ctx_free(&c);
}
static volatile PVOID tm_got; static HANDLE tm_gev;
static VOID NTAPI tm_ctx_cb(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_TIMER t) { tm_got = p; SetEvent(tm_gev); }
TEST(tp_timer_context_identity) {
    NEED_TIMER();
    tm_gev = tp_ev(FALSE);
    PVOID ctxs[] = { NULL, (PVOID)1, (PVOID)0x123456789abcULL, (PVOID)(LONG_PTR)-1 };
    for (int i = 0; i < 4; i++) {
        tm_got = (PVOID)0x77;
        PTP_TIMER t = U(CreateThreadpoolTimer)(tm_ctx_cb, ctxs[i], NULL);
        FILETIME ft; tp_rel_ft(&ft, 1);
        U(SetThreadpoolTimer)(t, &ft, 0, 0);
        CHECK(tp_signaled(tm_gev, TP_WAIT_MS));
        CHECKF(tm_got == ctxs[i], "i=%d got %p", i, tm_got);
        U(WaitForThreadpoolTimerCallbacks)(t, FALSE);
        U(CloseThreadpoolTimer)(t);
    }
    CloseHandle(tm_gev);
}
TEST(tp_timer_zero_due_time_fires_immediately) {
    NEED_TIMER();
    tp_ctx c; tp_ctx_init(&c);
    PTP_TIMER t = U(CreateThreadpoolTimer)(tp_timer_count, &c, NULL);
    FILETIME ft = { 0, 0 };
    U(SetThreadpoolTimer)(t, &ft, 0, 0);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    U(WaitForThreadpoolTimerCallbacks)(t, FALSE);
    U(CloseThreadpoolTimer)(t);
    tp_ctx_free(&c);
}
TEST(tp_timer_absolute_due_time) {
    NEED_TIMER();
    tp_ctx c; tp_ctx_init(&c);
    PTP_TIMER t = U(CreateThreadpoolTimer)(tp_timer_count, &c, NULL);
    FILETIME ft; tp_abs_ft(&ft, 150);
    ULONGLONG t0 = tp_now();
    U(SetThreadpoolTimer)(t, &ft, 0, 0);
    CHECK(tp_not_signaled(c.ev, 50));
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    CHECK(tp_now() - t0 >= 100);
    U(WaitForThreadpoolTimerCallbacks)(t, FALSE);
    U(CloseThreadpoolTimer)(t);
    tp_ctx_free(&c);
}
TEST(tp_timer_absolute_time_in_the_past_fires) {
    NEED_TIMER();
    tp_ctx c; tp_ctx_init(&c);
    PTP_TIMER t = U(CreateThreadpoolTimer)(tp_timer_count, &c, NULL);
    FILETIME ft; tp_abs_ft(&ft, -5000);
    U(SetThreadpoolTimer)(t, &ft, 0, 0);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    U(WaitForThreadpoolTimerCallbacks)(t, FALSE);
    U(CloseThreadpoolTimer)(t);
    tp_ctx_free(&c);
}
TEST(tp_timer_period_accuracy_bounds) {
    NEED_TIMER();
    tp_ctx c; tp_ctx_init(&c); c.ev = NULL;
    PTP_TIMER t = U(CreateThreadpoolTimer)(tp_timer_count, &c, NULL);
    FILETIME ft; tp_rel_ft(&ft, 50);
    U(SetThreadpoolTimer)(t, &ft, 50, 0);
    Sleep(1000);
    tm_cancel_close(t);
    LONG n = c.n;
    CHECKF(n >= 10 && n <= 25, "50ms periodic timer fired %ld times in 1s", (long)n);
    t_note("50ms periodic threadpool timer fired %ld times in 1000ms", (long)n);
    tp_ctx_free(&c);
}
TEST(tp_timer_period_continues_until_cancelled) {
    NEED_TIMER();
    tp_ctx c; tp_ctx_init(&c); c.ev = NULL;
    PTP_TIMER t = U(CreateThreadpoolTimer)(tp_timer_count, &c, NULL);
    FILETIME ft; tp_rel_ft(&ft, 10);
    U(SetThreadpoolTimer)(t, &ft, 20, 0);
    CHECK(tp_wait_count(&c.n, 5, 5000));
    CHECK(U(IsThreadpoolTimerSet)(t));            /* a periodic timer stays set */
    U(SetThreadpoolTimer)(t, NULL, 0, 0);
    U(WaitForThreadpoolTimerCallbacks)(t, TRUE);
    LONG n = c.n;
    Sleep(200);
    EXPECT_EQ(c.n, n);
    CHECK(!U(IsThreadpoolTimerSet)(t));
    U(CloseThreadpoolTimer)(t);
    tp_ctx_free(&c);
}
TEST(tp_timer_null_due_cancels_pending) {
    NEED_TIMER();
    tp_ctx c; tp_ctx_init(&c);
    PTP_TIMER t = U(CreateThreadpoolTimer)(tp_timer_count, &c, NULL);
    FILETIME ft; tp_rel_ft(&ft, 200);
    U(SetThreadpoolTimer)(t, &ft, 0, 0);
    U(SetThreadpoolTimer)(t, NULL, 0, 0);
    Sleep(500);
    EXPECT_EQ(c.n, 0);
    U(CloseThreadpoolTimer)(t);
    tp_ctx_free(&c);
}
TEST(tp_timer_is_set_state_machine) {
    NEED_TIMER();
    tp_ctx c; tp_ctx_init(&c);
    PTP_TIMER t = U(CreateThreadpoolTimer)(tp_timer_count, &c, NULL);
    CHECK(U(IsThreadpoolTimerSet)(t) == FALSE);   /* fresh timer */
    FILETIME ft; tp_rel_ft(&ft, 60000);
    U(SetThreadpoolTimer)(t, &ft, 0, 0);
    CHECK(U(IsThreadpoolTimerSet)(t) == TRUE);
    U(SetThreadpoolTimer)(t, NULL, 0, 0);
    CHECK(U(IsThreadpoolTimerSet)(t) == FALSE);   /* cancelled */
    tp_rel_ft(&ft, 20);
    U(SetThreadpoolTimer)(t, &ft, 0, 0);
    CHECK(U(IsThreadpoolTimerSet)(t) == TRUE);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    U(WaitForThreadpoolTimerCallbacks)(t, FALSE);
    t_note("IsThreadpoolTimerSet after a one-shot fired: %d", U(IsThreadpoolTimerSet)(t));
    U(CloseThreadpoolTimer)(t);
    tp_ctx_free(&c);
}
TEST(tp_timer_reset_replaces_previous) {
    NEED_TIMER();
    tp_ctx c; tp_ctx_init(&c);
    PTP_TIMER t = U(CreateThreadpoolTimer)(tp_timer_count, &c, NULL);
    FILETIME fartime, soon; tp_rel_ft(&fartime, 60000); tp_rel_ft(&soon, 50);
    U(SetThreadpoolTimer)(t, &fartime, 0, 0);
    U(SetThreadpoolTimer)(t, &soon, 0, 0);        /* replaces, does not add */
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    Sleep(200);
    EXPECT_EQ(c.n, 1);
    U(WaitForThreadpoolTimerCallbacks)(t, FALSE);
    U(CloseThreadpoolTimer)(t);
    tp_ctx_free(&c);
}
TEST(tp_timer_reset_pushes_out) {
    NEED_TIMER();
    tp_ctx c; tp_ctx_init(&c);
    PTP_TIMER t = U(CreateThreadpoolTimer)(tp_timer_count, &c, NULL);
    FILETIME a, b; tp_rel_ft(&a, 150); tp_rel_ft(&b, 60000);
    U(SetThreadpoolTimer)(t, &a, 0, 0);
    U(SetThreadpoolTimer)(t, &b, 0, 0);
    Sleep(400);
    EXPECT_EQ(c.n, 0);
    tm_cancel_close(t);
    tp_ctx_free(&c);
}
TEST(tp_timer_window_length_does_not_fire_early) {
    NEED_TIMER();
    tp_ctx c; tp_ctx_init(&c);
    PTP_TIMER t = U(CreateThreadpoolTimer)(tp_timer_count, &c, NULL);
    FILETIME ft; tp_rel_ft(&ft, 100);
    ULONGLONG t0 = tp_now();
    U(SetThreadpoolTimer)(t, &ft, 0, 300);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    ULONGLONG dt = tp_now() - t0;
    CHECK(dt >= 80);
    t_note("100ms timer with 300ms window fired after %llu ms", dt);
    U(WaitForThreadpoolTimerCallbacks)(t, FALSE);
    U(CloseThreadpoolTimer)(t);
    tp_ctx_free(&c);
}
TEST(tp_timer_window_coalesces_two_timers) {
    NEED_TIMER();
    tp_ctx a, b; tp_ctx_init(&a); tp_ctx_init(&b);
    PTP_TIMER ta = U(CreateThreadpoolTimer)(tp_timer_count, &a, NULL), tb = U(CreateThreadpoolTimer)(tp_timer_count, &b, NULL);
    FILETIME fa, fb; tp_rel_ft(&fa, 100); tp_rel_ft(&fb, 160);
    U(SetThreadpoolTimer)(ta, &fa, 0, 400);
    U(SetThreadpoolTimer)(tb, &fb, 0, 400);
    CHECK(tp_signaled(a.ev, TP_WAIT_MS));
    CHECK(tp_signaled(b.ev, TP_WAIT_MS));
    EXPECT_EQ(a.n, 1);
    EXPECT_EQ(b.n, 1);
    U(WaitForThreadpoolTimerCallbacks)(ta, FALSE); U(WaitForThreadpoolTimerCallbacks)(tb, FALSE);
    U(CloseThreadpoolTimer)(ta); U(CloseThreadpoolTimer)(tb);
    tp_ctx_free(&a); tp_ctx_free(&b);
}
TEST(tp_timer_set_ex_basic) {
    NEED_TIMER(); REQUIRE(U(SetThreadpoolTimerEx));
    tp_ctx c; tp_ctx_init(&c);
    PTP_TIMER t = U(CreateThreadpoolTimer)(tp_timer_count, &c, NULL);
    FILETIME ft; tp_rel_ft(&ft, 60000);
    BOOL r1 = U(SetThreadpoolTimerEx)(t, &ft, 0, 0);
    tp_rel_ft(&ft, 30);
    BOOL r2 = U(SetThreadpoolTimerEx)(t, &ft, 0, 0);
    t_note("SetThreadpoolTimerEx returned %d then %d (second call replaced a pending timer)", r1, r2);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    U(WaitForThreadpoolTimerCallbacks)(t, FALSE);
    BOOL r3 = U(SetThreadpoolTimerEx)(t, NULL, 0, 0);
    t_note("SetThreadpoolTimerEx(NULL) on an idle timer returned %d", r3);
    U(CloseThreadpoolTimer)(t);
    tp_ctx_free(&c);
}
TEST(tp_timer_set_ex_cancel_prevents_callback) {
    NEED_TIMER(); REQUIRE(U(SetThreadpoolTimerEx));
    tp_ctx c; tp_ctx_init(&c);
    PTP_TIMER t = U(CreateThreadpoolTimer)(tp_timer_count, &c, NULL);
    FILETIME ft; tp_rel_ft(&ft, 200);
    U(SetThreadpoolTimerEx)(t, &ft, 0, 0);
    U(SetThreadpoolTimerEx)(t, NULL, 0, 0);
    Sleep(450);
    EXPECT_EQ(c.n, 0);
    U(CloseThreadpoolTimer)(t);
    tp_ctx_free(&c);
}
static VOID NTAPI tm_slow_cb(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_TIMER t) {
    tp_ctx *c = (tp_ctx *)p;
    InterlockedIncrement(&c->n);
    SetEvent(c->ev2);
    Sleep(200);
    InterlockedIncrement(&c->n2);
}
TEST(tp_timer_wait_callbacks_blocks_for_running) {
    NEED_TIMER();
    tp_ctx c; tp_ctx_init(&c);
    PTP_TIMER t = U(CreateThreadpoolTimer)(tm_slow_cb, &c, NULL);
    FILETIME ft; tp_rel_ft(&ft, 10);
    U(SetThreadpoolTimer)(t, &ft, 0, 0);
    CHECK(tp_signaled(c.ev2, TP_WAIT_MS));        /* callback is running */
    U(WaitForThreadpoolTimerCallbacks)(t, FALSE);
    EXPECT_EQ(c.n2, 1);                           /* returned only after it finished */
    U(CloseThreadpoolTimer)(t);
    tp_ctx_free(&c);
}
TEST(tp_timer_wait_callbacks_idle_returns) {
    NEED_TIMER();
    tp_ctx c; tp_ctx_init(&c);
    PTP_TIMER t = U(CreateThreadpoolTimer)(tp_timer_count, &c, NULL);
    ULONGLONG t0 = tp_now();
    U(WaitForThreadpoolTimerCallbacks)(t, FALSE);
    U(WaitForThreadpoolTimerCallbacks)(t, TRUE);
    CHECK(tp_now() - t0 < 1000);
    U(CloseThreadpoolTimer)(t);
    tp_ctx_free(&c);
}
TEST(tp_timer_wait_cancel_drops_queued_callbacks) {
    NEED_TIMER(); REQUIRE(U(CreateThreadpool)); REQUIRE(U(CloseThreadpool)); REQUIRE(U(SetThreadpoolThreadMaximum));
    REQUIRE(U(TrySubmitThreadpoolCallback));
    PTP_POOL p = U(CreateThreadpool)(NULL);
    U(SetThreadpoolThreadMaximum)(p, 1);
    TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e); SetThreadpoolCallbackPool(&e, p);
    tp_ctx blocker, c; tp_ctx_init(&blocker); tp_ctx_init(&c); c.ev = NULL;
    CHECK(U(TrySubmitThreadpoolCallback)(tp_cb_gate, &blocker, &e));
    CHECK(tp_signaled(blocker.ev, TP_WAIT_MS));   /* the only pool thread is parked */
    PTP_TIMER t = U(CreateThreadpoolTimer)(tp_timer_count, &c, &e);
    FILETIME ft; tp_rel_ft(&ft, 10);
    U(SetThreadpoolTimer)(t, &ft, 0, 0);
    Sleep(200);                                   /* expired; its callback is queued behind the blocker */
    EXPECT_EQ(c.n, 0);
    U(WaitForThreadpoolTimerCallbacks)(t, TRUE);  /* cancel the queued callback */
    SetEvent(blocker.gate);
    CHECK(tp_wait_count(&blocker.n2, 1, TP_WAIT_MS));
    Sleep(150);
    EXPECT_EQ(c.n, 0);
    U(CloseThreadpoolTimer)(t);
    U(CloseThreadpool)(p);
    tp_ctx_free(&blocker); tp_ctx_free(&c);
}
TEST(tp_timer_wait_no_cancel_runs_queued_callback) {
    NEED_TIMER(); REQUIRE(U(CreateThreadpool)); REQUIRE(U(CloseThreadpool)); REQUIRE(U(SetThreadpoolThreadMaximum));
    REQUIRE(U(TrySubmitThreadpoolCallback));
    PTP_POOL p = U(CreateThreadpool)(NULL);
    U(SetThreadpoolThreadMaximum)(p, 1);
    TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e); SetThreadpoolCallbackPool(&e, p);
    tp_ctx blocker, c; tp_ctx_init(&blocker); tp_ctx_init(&c); c.ev = NULL;
    CHECK(U(TrySubmitThreadpoolCallback)(tp_cb_gate, &blocker, &e));
    CHECK(tp_signaled(blocker.ev, TP_WAIT_MS));
    PTP_TIMER t = U(CreateThreadpoolTimer)(tp_timer_count, &c, &e);
    FILETIME ft; tp_rel_ft(&ft, 10);
    U(SetThreadpoolTimer)(t, &ft, 0, 0);
    Sleep(150);
    tp_delay_arg da; HANDLE th = tp_delayed_set(&da, blocker.gate, 100);
    U(WaitForThreadpoolTimerCallbacks)(t, FALSE);
    EXPECT_EQ(c.n, 1);                            /* waited for the queued callback to run */
    tp_join(th);
    U(CloseThreadpoolTimer)(t);
    U(CloseThreadpool)(p);
    tp_ctx_free(&blocker); tp_ctx_free(&c);
}
TEST(tp_timer_close_while_set_never_fires) {
    NEED_TIMER();
    tp_ctx c; tp_ctx_init(&c);
    PTP_TIMER t = U(CreateThreadpoolTimer)(tp_timer_count, &c, NULL);
    FILETIME ft; tp_rel_ft(&ft, 200);
    U(SetThreadpoolTimer)(t, &ft, 0, 0);
    U(CloseThreadpoolTimer)(t);
    Sleep(450);
    EXPECT_EQ(c.n, 0);
    tp_ctx_free(&c);
}
typedef struct { tp_ctx *c; LONG id; } tm_tag;
static VOID NTAPI tm_tag_cb(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_TIMER t) {
    tm_tag *g = (tm_tag *)p;
    LONG k = InterlockedIncrement(&g->c->seq) - 1;
    if (k < 64) g->c->order[k] = g->id;
    InterlockedIncrement(&g->c->n);
}
TEST(tp_timer_callback_order_follows_due_time) {
    NEED_TIMER();
    tp_ctx c; tp_ctx_init(&c);
    tm_tag tags[3] = { { &c, 1 }, { &c, 2 }, { &c, 3 } };
    LONG dues[3] = { 300, 60, 180 };
    PTP_TIMER t[3];
    for (int i = 0; i < 3; i++) {
        t[i] = U(CreateThreadpoolTimer)(tm_tag_cb, &tags[i], NULL);
        FILETIME ft; tp_rel_ft(&ft, dues[i]);
        U(SetThreadpoolTimer)(t[i], &ft, 0, 0);
    }
    CHECK(tp_wait_count(&c.n, 3, TP_WAIT_MS));
    EXPECT_EQ(c.order[0], 2);
    EXPECT_EQ(c.order[1], 3);
    EXPECT_EQ(c.order[2], 1);
    for (int i = 0; i < 3; i++) { U(WaitForThreadpoolTimerCallbacks)(t[i], FALSE); U(CloseThreadpoolTimer)(t[i]); }
    tp_ctx_free(&c);
}
static VOID NTAPI tm_prio_cb(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_TIMER t) {
    tp_ctx *c = (tp_ctx *)p; c->val = GetThreadPriority(GetCurrentThread()); SetEvent(c->ev);
}
TEST(tp_timer_callback_thread_normal_priority) {
    NEED_TIMER();
    tp_ctx c; tp_ctx_init(&c); c.val = 99;
    PTP_TIMER t = U(CreateThreadpoolTimer)(tm_prio_cb, &c, NULL);
    FILETIME ft; tp_rel_ft(&ft, 5);
    U(SetThreadpoolTimer)(t, &ft, 0, 0);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    EXPECT_EQ(c.val, THREAD_PRIORITY_NORMAL);
    U(WaitForThreadpoolTimerCallbacks)(t, FALSE);
    U(CloseThreadpoolTimer)(t);
    tp_ctx_free(&c);
}
static VOID NTAPI tm_bump_cb(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_TIMER t) { InterlockedIncrement((volatile LONG *)p); }
TEST(tp_timer_many_timers_all_fire) {
    NEED_TIMER();
    enum { N = 200 };
    static PTP_TIMER ts[N];
    volatile LONG n = 0;
    for (int i = 0; i < N; i++) {
        ts[i] = U(CreateThreadpoolTimer)(tm_bump_cb, (PVOID)&n, NULL);
        CHECK(ts[i] != NULL);
        FILETIME ft; tp_rel_ft(&ft, 10 + (i % 20) * 5);
        U(SetThreadpoolTimer)(ts[i], &ft, 0, 0);
    }
    CHECK(tp_wait_count(&n, N, 20000));
    Sleep(100);
    EXPECT_EQ(n, N);
    for (int i = 0; i < N; i++) { U(WaitForThreadpoolTimerCallbacks)(ts[i], FALSE); U(CloseThreadpoolTimer)(ts[i]); }
}
TEST(tp_timer_set_cancel_loop) {
    NEED_TIMER();
    tp_ctx c; tp_ctx_init(&c);
    PTP_TIMER t = U(CreateThreadpoolTimer)(tp_timer_count, &c, NULL);
    FILETIME ft; tp_rel_ft(&ft, 60000);
    for (int i = 0; i < 5000; i++) {
        U(SetThreadpoolTimer)(t, &ft, 0, 0);
        U(SetThreadpoolTimer)(t, NULL, 0, 0);
    }
    EXPECT_EQ(c.n, 0);
    CHECK(!U(IsThreadpoolTimerSet)(t));
    U(CloseThreadpoolTimer)(t);
    tp_ctx_free(&c);
}
TEST(tp_timer_period_change_on_reset) {
    NEED_TIMER();
    tp_ctx c; tp_ctx_init(&c); c.ev = NULL;
    PTP_TIMER t = U(CreateThreadpoolTimer)(tp_timer_count, &c, NULL);
    FILETIME ft; tp_rel_ft(&ft, 10);
    U(SetThreadpoolTimer)(t, &ft, 30, 0);
    CHECK(tp_wait_count(&c.n, 4, 5000));
    tp_rel_ft(&ft, 10);
    U(SetThreadpoolTimer)(t, &ft, 0, 0);          /* converted to one-shot */
    Sleep(150);
    U(WaitForThreadpoolTimerCallbacks)(t, FALSE);
    LONG n = c.n;
    Sleep(250);
    EXPECT_EQ(c.n, n);
    U(CloseThreadpoolTimer)(t);
    tp_ctx_free(&c);
}
static VOID NTAPI tm_self_cancel_cb(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_TIMER t) {
    tp_ctx *c = (tp_ctx *)p;
    void (WINAPI *st)(PTP_TIMER, PFILETIME, DWORD, DWORD) = (void *)t_proc("SetThreadpoolTimer");
    if (InterlockedIncrement(&c->n) == 3) { st(t, NULL, 0, 0); SetEvent(c->ev); }
}
TEST(tp_timer_cancel_from_own_callback) {
    NEED_TIMER();
    tp_ctx c; tp_ctx_init(&c);
    PTP_TIMER t = U(CreateThreadpoolTimer)(tm_self_cancel_cb, &c, NULL);
    FILETIME ft; tp_rel_ft(&ft, 5);
    U(SetThreadpoolTimer)(t, &ft, 15, 0);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    U(WaitForThreadpoolTimerCallbacks)(t, TRUE);
    LONG n = c.n;
    Sleep(150);
    EXPECT_EQ(c.n, n);
    CHECK(n >= 3);
    U(CloseThreadpoolTimer)(t);
    tp_ctx_free(&c);
}
TEST(tp_timer_private_pool_runs_off_main_thread) {
    NEED_TIMER(); REQUIRE(U(CreateThreadpool)); REQUIRE(U(CloseThreadpool));
    PTP_POOL p = U(CreateThreadpool)(NULL);
    TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e); SetThreadpoolCallbackPool(&e, p);
    tp_ctx c; tp_ctx_init(&c);
    PTP_TIMER t = U(CreateThreadpoolTimer)(tp_timer_count, &c, &e);
    FILETIME ft; tp_rel_ft(&ft, 5);
    U(SetThreadpoolTimer)(t, &ft, 0, 0);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    CHECK(c.tid != GetCurrentThreadId());
    U(WaitForThreadpoolTimerCallbacks)(t, FALSE);
    U(CloseThreadpoolTimer)(t);
    U(CloseThreadpool)(p);
    tp_ctx_free(&c);
}
