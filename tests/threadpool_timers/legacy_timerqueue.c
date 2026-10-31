/* Timer queues: CreateTimerQueue*, ChangeTimerQueueTimer, DeleteTimerQueue*, SetTimerQueueTimer. */
#include "tp.h"

/* Holds the callback open: signals ev2 (started), waits gate, sleeps tid ms, counts n2, signals ev. */
static VOID CALLBACK tq_slow_cb(PVOID p, BOOLEAN fired) {
    tp_ctx *c = (tp_ctx *)p;
    if (InterlockedIncrement(&c->n) > 1) return;     /* only the first invocation blocks */
    SetEvent(c->ev2);
    WaitForSingleObject(c->gate, 20000);
    if (c->tid) Sleep(c->tid);
    InterlockedIncrement(&c->n2);
    if (c->ev) SetEvent(c->ev);
}
static VOID CALLBACK tq_order_cb(PVOID p, BOOLEAN fired) {
    tp_ctx *c = (tp_ctx *)p; (void)c;
}
typedef struct { tp_ctx *c; LONG id; } tq_tag;
static VOID CALLBACK tq_tag_cb(PVOID p, BOOLEAN fired) {
    tq_tag *t = (tq_tag *)p;
    LONG i = InterlockedIncrement(&t->c->seq) - 1;
    if (i < 64) t->c->order[i] = t->id;
    InterlockedIncrement(&t->c->n);
}

TEST(tp_tq_create_delete) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(DeleteTimerQueue));
    SET_ERR(0);
    HANDLE q = U(CreateTimerQueue)();
    CHECK(q != NULL);
    SET_ERR(0);
    BOOL r = U(DeleteTimerQueue)(q);
    if (t_is_wine() && !r) t_note("wine DeleteTimerQueue on an idle queue failed, err %lu", GetLastError());
    else CHECK(r == TRUE);
}
TEST(tp_tq_queues_distinct) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(DeleteTimerQueueEx));
    HANDLE q[8];
    for (int i = 0; i < 8; i++) { q[i] = U(CreateTimerQueue)(); CHECK(q[i] != NULL); }
    for (int i = 0; i < 8; i++) for (int j = i + 1; j < 8; j++) CHECK(q[i] != q[j]);
    for (int i = 0; i < 8; i++) CHECK(U(DeleteTimerQueueEx)(q[i], INVALID_HANDLE_VALUE));
}
TEST(tp_tq_create_delete_loop) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(DeleteTimerQueueEx));
    for (int i = 0; i < 300; i++) {
        HANDLE q = U(CreateTimerQueue)();
        if (!q) { CHECK(q != NULL); break; }
        if (!U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE)) { CHECK(0); break; }
    }
}
TEST(tp_tq_delete_null_queue) {
    REQUIRE(U(DeleteTimerQueue)); REQUIRE(U(DeleteTimerQueueEx));
    SET_ERR(0);
    CHECK(U(DeleteTimerQueue)(NULL) == FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE comps[] = { NULL, INVALID_HANDLE_VALUE };
    for (int i = 0; i < 2; i++) {
        SET_ERR(0);
        CHECK(U(DeleteTimerQueueEx)(NULL, comps[i]) == FALSE);
        EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
}

TEST(tp_tq_delete_ex_completion_event) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(DeleteTimerQueueEx));
    HANDLE q = U(CreateTimerQueue)(), comp = tp_ev(TRUE);
    SET_ERR(0);
    BOOL r = U(DeleteTimerQueueEx)(q, comp);
    CHECK(r == TRUE || GetLastError() == ERROR_IO_PENDING);
    if (!tp_signaled(comp, 3000)) t_note("DeleteTimerQueueEx completion event not signalled for an idle queue");
    CloseHandle(comp);
}

TEST(tp_tq_delete_ex_blocking_waits_for_callback) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(DeleteTimerQueueEx)); REQUIRE(U(CreateTimerQueueTimer));
    tp_ctx c; tp_ctx_init(&c);
    HANDLE q = U(CreateTimerQueue)(), t = NULL;
    CHECK(U(CreateTimerQueueTimer)(&t, q, tq_slow_cb, &c, 10, 0, WT_EXECUTEDEFAULT));
    CHECK(tp_signaled(c.ev2, TP_WAIT_MS));
    tp_delay_arg da; HANDLE th = tp_delayed_set(&da, c.gate, 250);
    ULONGLONG t0 = tp_now();
    BOOL r = U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE);
    ULONGLONG dt = tp_now() - t0;
    CHECK(r == TRUE);
    EXPECT_EQ(c.n2, 1);                           /* the callback finished before the call returned */
    CHECK(dt >= 150);
    tp_join(th);
    tp_ctx_free(&c);
}

TEST(tp_tq_delete_ex_null_event_with_running_callback) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(DeleteTimerQueueEx)); REQUIRE(U(CreateTimerQueueTimer));
    tp_ctx c; tp_ctx_init(&c);
    HANDLE q = U(CreateTimerQueue)(), t = NULL;
    CHECK(U(CreateTimerQueueTimer)(&t, q, tq_slow_cb, &c, 10, 0, WT_EXECUTEDEFAULT));
    CHECK(tp_signaled(c.ev2, TP_WAIT_MS));
    SET_ERR(0);
    CHECK(U(DeleteTimerQueueEx)(q, NULL) == FALSE);
    EXPECT_ERR(ERROR_IO_PENDING);
    EXPECT_EQ(c.n2, 0);
    SetEvent(c.gate);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    tp_ctx_free(&c);
}

TEST(tp_tq_delete_ex_event_with_running_callback) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(DeleteTimerQueueEx)); REQUIRE(U(CreateTimerQueueTimer));
    tp_ctx c; tp_ctx_init(&c);
    HANDLE q = U(CreateTimerQueue)(), t = NULL, comp = tp_ev(TRUE);
    CHECK(U(CreateTimerQueueTimer)(&t, q, tq_slow_cb, &c, 10, 0, WT_EXECUTEDEFAULT));
    CHECK(tp_signaled(c.ev2, TP_WAIT_MS));
    SET_ERR(0);
    CHECK(U(DeleteTimerQueueEx)(q, comp) == FALSE);
    EXPECT_ERR(ERROR_IO_PENDING);
    if (t_is_wine()) t_note("wine signals the completion event early");
    else CHECK(tp_not_signaled(comp, 100));
    SetEvent(c.gate);
    CHECK(tp_signaled(comp, TP_WAIT_MS));
    CHECK(tp_wait_count(&c.n2, 1, TP_WAIT_MS));
    CloseHandle(comp); tp_ctx_free(&c);
}

TEST(tp_tq_delete_queue_cancels_pending_timers) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(DeleteTimerQueueEx)); REQUIRE(U(CreateTimerQueueTimer));
    tp_ctx c; tp_ctx_init(&c);
    HANDLE q = U(CreateTimerQueue)();
    HANDLE t[5];
    for (int i = 0; i < 5; i++) CHECK(U(CreateTimerQueueTimer)(&t[i], q, tp_wot_count, &c, 300 + i, 0, 0));
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
    Sleep(500);
    EXPECT_EQ(c.n, 0);
    tp_ctx_free(&c);
}

TEST(tp_tq_oneshot_fires_once_with_true) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(DeleteTimerQueueEx)); REQUIRE(U(CreateTimerQueueTimer));
    tp_ctx c; tp_ctx_init(&c);
    HANDLE q = U(CreateTimerQueue)(), t = NULL;
    ULONGLONG t0 = tp_now();
    SET_ERR(0);
    CHECK(U(CreateTimerQueueTimer)(&t, q, tp_wot_count, &c, 100, 0, WT_EXECUTEDEFAULT) == TRUE);
    CHECK(t != NULL);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    ULONGLONG dt = tp_now() - t0;
    CHECK(dt >= 80);
    EXPECT_EQ(c.val, 1);                          /* TimerOrWaitFired is always TRUE for timers */
    Sleep(300);
    EXPECT_EQ(c.n, 1);
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
    tp_ctx_free(&c);
}

TEST(tp_tq_timer_handle_nonnull_and_distinct) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(DeleteTimerQueueEx)); REQUIRE(U(CreateTimerQueueTimer));
    tp_ctx c; tp_ctx_init(&c);
    HANDLE q = U(CreateTimerQueue)(), t1 = (HANDLE)5, t2 = (HANDLE)5;
    CHECK(U(CreateTimerQueueTimer)(&t1, q, tp_wot_count, &c, 60000, 0, 0));
    CHECK(U(CreateTimerQueueTimer)(&t2, q, tp_wot_count, &c, 60000, 0, 0));
    CHECK(t1 != NULL && t1 != (HANDLE)5);
    CHECK(t2 != NULL && t1 != t2);
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
    EXPECT_EQ(c.n, 0);
    tp_ctx_free(&c);
}

static volatile PVOID tq_got; static HANDLE tq_gev;
static VOID CALLBACK tq_ctx_cb(PVOID p, BOOLEAN fired) { tq_got = p; SetEvent(tq_gev); }
TEST(tp_tq_context_identity) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(DeleteTimerQueueEx)); REQUIRE(U(CreateTimerQueueTimer));
    PVOID ctxs[] = { (PVOID)0, (PVOID)1, (PVOID)0x123456789abcULL, (PVOID)(LONG_PTR)-1 };
    tq_gev = tp_ev(FALSE);
    HANDLE q = U(CreateTimerQueue)();
    for (int i = 0; i < 4; i++) {
        HANDLE t = NULL;
        tq_got = (PVOID)0x77;
        CHECK(U(CreateTimerQueueTimer)(&t, q, tq_ctx_cb, ctxs[i], 1, 0, 0));
        CHECK(tp_signaled(tq_gev, TP_WAIT_MS));
        CHECKF(tq_got == ctxs[i], "i=%d got %p", i, tq_got);
    }
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
    CloseHandle(tq_gev);
}
