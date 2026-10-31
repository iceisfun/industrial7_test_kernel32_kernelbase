/* Timer queue timers: periodic/oneshot, change, delete, cancel, default queue, ordering. */
#include "tp.h"

static VOID CALLBACK tq2_slow_cb(PVOID p, BOOLEAN fired) {
    tp_ctx *c = (tp_ctx *)p;
    if (InterlockedIncrement(&c->n) > 1) return;
    SetEvent(c->ev2);
    WaitForSingleObject(c->gate, 20000);
    InterlockedIncrement(&c->n2);
    if (c->ev) SetEvent(c->ev);
}
typedef struct { tp_ctx *c; LONG id; } tq_tag;
static VOID CALLBACK tq2_tag_cb(PVOID p, BOOLEAN fired) {
    tq_tag *t = (tq_tag *)p;
    LONG i = InterlockedIncrement(&t->c->seq) - 1;
    if (i < 64) t->c->order[i] = t->id;
    InterlockedIncrement(&t->c->n);
}
static VOID CALLBACK tq2_bump_cb(PVOID p, BOOLEAN fired) { InterlockedIncrement((volatile LONG *)p); }

/* ---- default queue (NULL TimerQueue) ---- */
static volatile LONG g_nd_n;
static VOID CALLBACK nd_cb(PVOID p, BOOLEAN fired) { InterlockedIncrement(&g_nd_n); }
CHILD(tp_tq_nodefault) {
    BOOL (WINAPI *del)(HANDLE, HANDLE, HANDLE) = (void *)t_proc("DeleteTimerQueueTimer");
    BOOL (WINAPI *chg)(HANDLE, HANDLE, ULONG, ULONG) = (void *)t_proc("ChangeTimerQueueTimer");
    BOOL (WINAPI *can)(HANDLE, HANDLE) = (void *)t_proc("CancelTimerQueueTimer");
    BOOL (WINAPI *mk)(PHANDLE, HANDLE, WAITORTIMERCALLBACK, PVOID, DWORD, DWORD, ULONG) = (void *)t_proc("CreateTimerQueueTimer");
    if (!del || !chg || !can || !mk) return 90;
    /* No default queue exists yet in a fresh process: NULL queue -> ERROR_INVALID_PARAMETER. */
    SetLastError(0); if (del(NULL, NULL, NULL) || GetLastError() != ERROR_INVALID_PARAMETER) return 1;
    SetLastError(0); if (chg(NULL, NULL, 0, 0) || GetLastError() != ERROR_INVALID_PARAMETER) return 2;
    SetLastError(0); if (can(NULL, NULL) || GetLastError() != ERROR_INVALID_PARAMETER) return 3;
    SetLastError(0); if (del(NULL, (HANDLE)0x1000, INVALID_HANDLE_VALUE) || GetLastError() != ERROR_INVALID_PARAMETER) return 4;
    /* Creating a timer on the NULL queue brings the default queue to life. */
    HANDLE t = NULL;
    if (!mk(&t, NULL, nd_cb, NULL, 20, 0, 0) || !t) return 5;
    for (int i = 0; i < 200 && !g_nd_n; i++) Sleep(10);
    if (g_nd_n != 1) return 6;
    if (!del(NULL, t, INVALID_HANDLE_VALUE)) return 7;
    /* With a default queue present, a NULL timer is rejected by the runtime, not the wrapper. */
    SetLastError(0); if (del(NULL, NULL, NULL) || GetLastError() != ERROR_INVALID_PARAMETER) return 8;
    return 0;
}
TEST(tp_tq_null_queue_without_default) {
    REQUIRE(U(DeleteTimerQueueTimer)); REQUIRE(U(CreateTimerQueueTimer));
    if (t_is_wine()) { t_skip("wine faults on NULL timer arguments"); return; }
    EXPECT_EQ(t_run_child("tp_tq_nodefault", "", 25000), 0);
}

TEST(tp_tq_default_queue_timer) {
    REQUIRE(U(CreateTimerQueueTimer)); REQUIRE(U(DeleteTimerQueueTimer));
    tp_ctx c; tp_ctx_init(&c);
    HANDLE t = NULL;
    SET_ERR(0);
    CHECK(U(CreateTimerQueueTimer)(&t, NULL, tp_wot_count, &c, 30, 0, 0) == TRUE);
    CHECK(t != NULL);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    EXPECT_EQ(c.val, 1);
    CHECK(U(DeleteTimerQueueTimer)(NULL, t, INVALID_HANDLE_VALUE) == TRUE);
    tp_ctx_free(&c);
}
TEST(tp_tq_default_queue_set_timer_queue_timer) {
    if (t_is_wine()) { t_skip("wine: SetTimerQueueTimer unimplemented"); return; }
    REQUIRE(U(SetTimerQueueTimer)); REQUIRE(U(DeleteTimerQueueTimer));
    tp_ctx c; tp_ctx_init(&c);
    HANDLE t = U(SetTimerQueueTimer)(NULL, tp_wot_count, &c, 30, 0, FALSE);
    CHECK(t != NULL);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    CHECK(U(DeleteTimerQueueTimer)(NULL, t, INVALID_HANDLE_VALUE));
    t = U(SetTimerQueueTimer)(NULL, tp_wot_count, &c, 30, 0, TRUE);
    CHECK(t != NULL);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    CHECK(U(DeleteTimerQueueTimer)(NULL, t, INVALID_HANDLE_VALUE));
    tp_ctx_free(&c);
}

/* ---- one shot / periodic ---- */
TEST(tp_tq_periodic_accuracy_bounds) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(CreateTimerQueueTimer)); REQUIRE(U(DeleteTimerQueueEx));
    tp_ctx c; tp_ctx_init(&c); c.ev = NULL;
    HANDLE q = U(CreateTimerQueue)(), t = NULL;
    CHECK(U(CreateTimerQueueTimer)(&t, q, tp_wot_count, &c, 50, 50, 0));
    Sleep(1000);
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
    LONG n = c.n;
    CHECKF(n >= 10 && n <= 25, "periodic 50ms timer fired %ld times in 1s", (long)n);
    t_note("50ms periodic timer fired %ld times in 1000ms", (long)n);
    EXPECT_EQ(c.val, 1);
    Sleep(150);
    EXPECT_EQ(c.n, n);                           /* silent after the queue is gone */
    tp_ctx_free(&c);
}

TEST(tp_tq_due_time_respected) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(CreateTimerQueueTimer)); REQUIRE(U(DeleteTimerQueueEx));
    tp_ctx c; tp_ctx_init(&c);
    HANDLE q = U(CreateTimerQueue)(), t = NULL;
    ULONGLONG t0 = tp_now();
    CHECK(U(CreateTimerQueueTimer)(&t, q, tp_wot_count, &c, 300, 0, 0));
    CHECK(tp_not_signaled(c.ev, 150));
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    ULONGLONG dt = tp_now() - t0;
    CHECK(dt >= 270);
    t_note("300ms one-shot fired after %llu ms", dt);
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
    tp_ctx_free(&c);
}

TEST(tp_tq_zero_due_fires_promptly) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(CreateTimerQueueTimer)); REQUIRE(U(DeleteTimerQueueEx));
    tp_ctx c; tp_ctx_init(&c);
    HANDLE q = U(CreateTimerQueue)(), t = NULL;
    CHECK(U(CreateTimerQueueTimer)(&t, q, tp_wot_count, &c, 0, 0, 0));
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    Sleep(100);
    EXPECT_EQ(c.n, 1);
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
    tp_ctx_free(&c);
}

TEST(tp_tq_zero_due_periodic) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(CreateTimerQueueTimer)); REQUIRE(U(DeleteTimerQueueEx));
    tp_ctx c; tp_ctx_init(&c); c.ev = NULL;
    HANDLE q = U(CreateTimerQueue)(), t = NULL;
    CHECK(U(CreateTimerQueueTimer)(&t, q, tp_wot_count, &c, 0, 100, 0));
    CHECK(tp_wait_count(&c.n, 3, 5000));
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
    tp_ctx_free(&c);
}

TEST(tp_tq_flags_table) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(CreateTimerQueueTimer)); REQUIRE(U(DeleteTimerQueueEx));
    static const ULONG flags[] = {
        WT_EXECUTEDEFAULT, WT_EXECUTEINIOTHREAD, WT_EXECUTEINTIMERTHREAD, WT_EXECUTELONGFUNCTION,
        WT_EXECUTEINPERSISTENTTHREAD, WT_EXECUTEONLYONCE, WT_TRANSFER_IMPERSONATION,
        WT_EXECUTEINTIMERTHREAD | WT_EXECUTEONLYONCE, WT_EXECUTELONGFUNCTION | WT_EXECUTEONLYONCE,
    };
    HANDLE q = U(CreateTimerQueue)();
    for (size_t i = 0; i < sizeof flags / sizeof *flags; i++) {
        tp_ctx c; tp_ctx_init(&c);
        HANDLE t = NULL;
        SET_ERR(0);
        BOOL r = U(CreateTimerQueueTimer)(&t, q, tp_wot_count, &c, 20, 0, flags[i]);
        CHECKF(r, "flags 0x%lx err %lu", flags[i], GetLastError());
        if (r) {
            CHECKF(tp_signaled(c.ev, TP_WAIT_MS), "flags 0x%lx never fired", flags[i]);
            EXPECT_EQ(c.val, 1);
        }
        tp_ctx_free(&c);
    }
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
}

TEST(tp_tq_onlyonce_flag_with_period) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(CreateTimerQueueTimer)); REQUIRE(U(DeleteTimerQueueEx));
    tp_ctx c; tp_ctx_init(&c); c.ev = NULL;
    HANDLE q = U(CreateTimerQueue)(), t = NULL;
    CHECK(U(CreateTimerQueueTimer)(&t, q, tp_wot_count, &c, 20, 40, WT_EXECUTEONLYONCE));
    Sleep(400);
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
    CHECK(c.n >= 1);
    t_note("WT_EXECUTEONLYONCE with period 40: fired %ld times in 400ms", (long)c.n);
    tp_ctx_free(&c);
}

/* ---- ChangeTimerQueueTimer ---- */
TEST(tp_tq_change_pulls_in_due_time) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(CreateTimerQueueTimer)); REQUIRE(U(ChangeTimerQueueTimer));
    REQUIRE(U(DeleteTimerQueueEx));
    tp_ctx c; tp_ctx_init(&c);
    HANDLE q = U(CreateTimerQueue)(), t = NULL;
    CHECK(U(CreateTimerQueueTimer)(&t, q, tp_wot_count, &c, 60000, 0, 0));
    CHECK(tp_not_signaled(c.ev, 100));
    SET_ERR(0);
    CHECK(U(ChangeTimerQueueTimer)(q, t, 30, 0) == TRUE);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
    tp_ctx_free(&c);
}
TEST(tp_tq_change_pushes_out_due_time) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(CreateTimerQueueTimer)); REQUIRE(U(ChangeTimerQueueTimer));
    REQUIRE(U(DeleteTimerQueueEx));
    tp_ctx c; tp_ctx_init(&c);
    HANDLE q = U(CreateTimerQueue)(), t = NULL;
    CHECK(U(CreateTimerQueueTimer)(&t, q, tp_wot_count, &c, 150, 0, 0));
    CHECK(U(ChangeTimerQueueTimer)(q, t, 60000, 0));
    Sleep(400);
    EXPECT_EQ(c.n, 0);
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
    tp_ctx_free(&c);
}
TEST(tp_tq_change_adds_period) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(CreateTimerQueueTimer)); REQUIRE(U(ChangeTimerQueueTimer));
    REQUIRE(U(DeleteTimerQueueEx));
    tp_ctx c; tp_ctx_init(&c); c.ev = NULL;
    HANDLE q = U(CreateTimerQueue)(), t = NULL;
    CHECK(U(CreateTimerQueueTimer)(&t, q, tp_wot_count, &c, 10, 0, 0));
    CHECK(tp_wait_count(&c.n, 1, 3000));
    Sleep(150);
    EXPECT_EQ(c.n, 1);                              /* one-shot fired once */
    CHECK(U(ChangeTimerQueueTimer)(q, t, 10, 40));
    t_note("re-arming an expired one-shot with a period: n=%ld after 300ms", (long)(Sleep(300), c.n));
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
    tp_ctx_free(&c);
}
TEST(tp_tq_change_expired_oneshot_is_not_updated) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(CreateTimerQueueTimer)); REQUIRE(U(ChangeTimerQueueTimer));
    REQUIRE(U(DeleteTimerQueueEx));
    tp_ctx c; tp_ctx_init(&c);
    HANDLE q = U(CreateTimerQueue)(), t = NULL;
    CHECK(U(CreateTimerQueueTimer)(&t, q, tp_wot_count, &c, 10, 0, 0));
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    Sleep(100);
    SET_ERR(0);
    CHECK(U(ChangeTimerQueueTimer)(q, t, 10, 0) == TRUE);
    Sleep(300);
    EXPECT_EQ(c.n, 1);                              /* documented: expired one-shot is not re-armed */
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
    tp_ctx_free(&c);
}
TEST(tp_tq_change_periodic_to_oneshot) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(CreateTimerQueueTimer)); REQUIRE(U(ChangeTimerQueueTimer));
    REQUIRE(U(DeleteTimerQueueEx));
    tp_ctx c; tp_ctx_init(&c); c.ev = NULL;
    HANDLE q = U(CreateTimerQueue)(), t = NULL;
    CHECK(U(CreateTimerQueueTimer)(&t, q, tp_wot_count, &c, 20, 30, 0));
    CHECK(tp_wait_count(&c.n, 3, 5000));
    CHECK(U(ChangeTimerQueueTimer)(q, t, 20, 0));
    Sleep(200);
    LONG n = c.n;
    Sleep(300);
    EXPECT_EQ(c.n, n);                              /* stops after the converted one-shot */
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
    tp_ctx_free(&c);
}
TEST(tp_tq_change_invalid_timer) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(ChangeTimerQueueTimer)); REQUIRE(U(DeleteTimerQueueEx));
    if (t_is_wine()) { t_skip("wine faults on a NULL timer"); return; }
    HANDLE q = U(CreateTimerQueue)();
    SET_ERR(0);
    CHECK(U(ChangeTimerQueueTimer)(q, NULL, 10, 0) == FALSE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
}
TEST(tp_tq_delete_timer_invalid_timer) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(DeleteTimerQueueTimer)); REQUIRE(U(DeleteTimerQueueEx));
    HANDLE q = U(CreateTimerQueue)();
    HANDLE comps[] = { NULL, INVALID_HANDLE_VALUE };
    for (int i = 0; i < 2; i++) {
        SET_ERR(0);
        CHECK(U(DeleteTimerQueueTimer)(q, NULL, comps[i]) == FALSE);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
}
TEST(tp_tq_cancel_timer_invalid_timer) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(CancelTimerQueueTimer)); REQUIRE(U(DeleteTimerQueueEx));
    HANDLE q = U(CreateTimerQueue)();
    SET_ERR(0);
    CHECK(U(CancelTimerQueueTimer)(q, NULL) == FALSE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
}

/* ---- DeleteTimerQueueTimer / CancelTimerQueueTimer ---- */
TEST(tp_tq_delete_timer_idle_variants) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(CreateTimerQueueTimer)); REQUIRE(U(DeleteTimerQueueTimer));
    REQUIRE(U(DeleteTimerQueueEx));
    HANDLE q = U(CreateTimerQueue)();
    for (int v = 0; v < 3; v++) {
        tp_ctx c; tp_ctx_init(&c);
        HANDLE t = NULL, comp = tp_ev(TRUE);
        CHECK(U(CreateTimerQueueTimer)(&t, q, tp_wot_count, &c, 60000, 0, 0));
        HANDLE ce = v == 0 ? NULL : v == 1 ? INVALID_HANDLE_VALUE : comp;
        SET_ERR(0);
        BOOL dr = U(DeleteTimerQueueTimer)(q, t, ce);
        if (t_is_wine() && !dr && GetLastError() == ERROR_IO_PENDING) t_note("wine reports ERROR_IO_PENDING for an idle delete (variant %d)", v);
        else CHECKF(dr == TRUE, "variant %d err %lu", v, GetLastError());
        EXPECT_EQ(c.n, 0);
        CloseHandle(comp); tp_ctx_free(&c);
    }
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
}
TEST(tp_tq_delete_timer_stops_periodic) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(CreateTimerQueueTimer)); REQUIRE(U(DeleteTimerQueueTimer));
    REQUIRE(U(DeleteTimerQueueEx));
    tp_ctx c; tp_ctx_init(&c); c.ev = NULL;
    HANDLE q = U(CreateTimerQueue)(), t = NULL;
    CHECK(U(CreateTimerQueueTimer)(&t, q, tp_wot_count, &c, 10, 20, 0));
    CHECK(tp_wait_count(&c.n, 3, 5000));
    CHECK(U(DeleteTimerQueueTimer)(q, t, INVALID_HANDLE_VALUE));
    LONG n = c.n;
    Sleep(200);
    EXPECT_EQ(c.n, n);
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
    tp_ctx_free(&c);
}
TEST(tp_tq_delete_timer_blocking_waits_for_callback) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(CreateTimerQueueTimer)); REQUIRE(U(DeleteTimerQueueTimer));
    REQUIRE(U(DeleteTimerQueueEx));
    tp_ctx c; tp_ctx_init(&c);
    HANDLE q = U(CreateTimerQueue)(), t = NULL;
    CHECK(U(CreateTimerQueueTimer)(&t, q, tq2_slow_cb, &c, 10, 0, 0));
    CHECK(tp_signaled(c.ev2, TP_WAIT_MS));
    tp_delay_arg da; HANDLE th = tp_delayed_set(&da, c.gate, 250);
    ULONGLONG t0 = tp_now();
    BOOL r = U(DeleteTimerQueueTimer)(q, t, INVALID_HANDLE_VALUE);
    ULONGLONG dt = tp_now() - t0;
    CHECK(r == TRUE);
    EXPECT_EQ(c.n2, 1);
    CHECK(dt >= 150);
    tp_join(th);
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
    tp_ctx_free(&c);
}
TEST(tp_tq_delete_timer_pending_io_pending) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(CreateTimerQueueTimer)); REQUIRE(U(DeleteTimerQueueTimer));
    REQUIRE(U(DeleteTimerQueueEx));
    tp_ctx c; tp_ctx_init(&c);
    HANDLE q = U(CreateTimerQueue)(), t = NULL;
    CHECK(U(CreateTimerQueueTimer)(&t, q, tq2_slow_cb, &c, 10, 0, 0));
    CHECK(tp_signaled(c.ev2, TP_WAIT_MS));
    SET_ERR(0);
    CHECK(U(DeleteTimerQueueTimer)(q, t, NULL) == FALSE);
    EXPECT_ERR(ERROR_IO_PENDING);
    EXPECT_EQ(c.n2, 0);
    SetEvent(c.gate);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
    tp_ctx_free(&c);
}
TEST(tp_tq_delete_timer_pending_with_event) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(CreateTimerQueueTimer)); REQUIRE(U(DeleteTimerQueueTimer));
    REQUIRE(U(DeleteTimerQueueEx));
    tp_ctx c; tp_ctx_init(&c);
    HANDLE q = U(CreateTimerQueue)(), t = NULL, comp = tp_ev(TRUE);
    CHECK(U(CreateTimerQueueTimer)(&t, q, tq2_slow_cb, &c, 10, 0, 0));
    CHECK(tp_signaled(c.ev2, TP_WAIT_MS));
    SET_ERR(0);
    CHECK(U(DeleteTimerQueueTimer)(q, t, comp) == FALSE);
    EXPECT_ERR(ERROR_IO_PENDING);
    CHECK(tp_not_signaled(comp, 100));
    SetEvent(c.gate);
    CHECK(tp_signaled(comp, TP_WAIT_MS));
    EXPECT_EQ(c.n2, 1);
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
    CloseHandle(comp); tp_ctx_free(&c);
}
TEST(tp_tq_cancel_timer_before_fire) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(CreateTimerQueueTimer)); REQUIRE(U(CancelTimerQueueTimer));
    REQUIRE(U(DeleteTimerQueueEx));
    tp_ctx c; tp_ctx_init(&c);
    HANDLE q = U(CreateTimerQueue)(), t = NULL;
    CHECK(U(CreateTimerQueueTimer)(&t, q, tp_wot_count, &c, 200, 0, 0));
    SET_ERR(0);
    BOOL cr = U(CancelTimerQueueTimer)(q, t);
    if (t_is_wine() && !cr && GetLastError() == ERROR_IO_PENDING) t_note("wine reports ERROR_IO_PENDING for an idle cancel");
    else CHECK(cr == TRUE);
    Sleep(400);
    EXPECT_EQ(c.n, 0);
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
    tp_ctx_free(&c);
}
TEST(tp_tq_cancel_timer_while_running) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(CreateTimerQueueTimer)); REQUIRE(U(CancelTimerQueueTimer));
    REQUIRE(U(DeleteTimerQueueEx));
    tp_ctx c; tp_ctx_init(&c);
    HANDLE q = U(CreateTimerQueue)(), t = NULL;
    CHECK(U(CreateTimerQueueTimer)(&t, q, tq2_slow_cb, &c, 10, 0, 0));
    CHECK(tp_signaled(c.ev2, TP_WAIT_MS));
    SET_ERR(0);
    CHECK(U(CancelTimerQueueTimer)(q, t) == FALSE);   /* non-blocking delete with callback outstanding */
    EXPECT_ERR(ERROR_IO_PENDING);
    SetEvent(c.gate);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
    tp_ctx_free(&c);
}

/* ---- multiple queues / many timers ---- */
TEST(tp_tq_queues_are_independent) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(CreateTimerQueueTimer)); REQUIRE(U(DeleteTimerQueueEx));
    tp_ctx a, b; tp_ctx_init(&a); tp_ctx_init(&b); a.ev = b.ev = NULL;
    HANDLE qa = U(CreateTimerQueue)(), qb = U(CreateTimerQueue)(), ta = NULL, tb = NULL;
    CHECK(U(CreateTimerQueueTimer)(&ta, qa, tp_wot_count, &a, 20, 30, 0));
    CHECK(U(CreateTimerQueueTimer)(&tb, qb, tp_wot_count, &b, 20, 30, 0));
    CHECK(tp_wait_count(&a.n, 3, 5000)); CHECK(tp_wait_count(&b.n, 3, 5000));
    CHECK(U(DeleteTimerQueueEx)(qa, INVALID_HANDLE_VALUE));
    LONG an = a.n, bn = b.n;
    Sleep(250);
    EXPECT_EQ(a.n, an);
    CHECK(b.n > bn);                                  /* the other queue keeps running */
    CHECK(U(DeleteTimerQueueEx)(qb, INVALID_HANDLE_VALUE));
    tp_ctx_free(&a); tp_ctx_free(&b);
}
TEST(tp_tq_ordering_by_due_time) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(CreateTimerQueueTimer)); REQUIRE(U(DeleteTimerQueueEx));
    tp_ctx c; tp_ctx_init(&c);
    tq_tag tags[3] = { { &c, 1 }, { &c, 2 }, { &c, 3 } };
    HANDLE q = U(CreateTimerQueue)(), t[3];
    CHECK(U(CreateTimerQueueTimer)(&t[0], q, tq2_tag_cb, &tags[0], 300, 0, 0));
    CHECK(U(CreateTimerQueueTimer)(&t[1], q, tq2_tag_cb, &tags[1], 60, 0, 0));
    CHECK(U(CreateTimerQueueTimer)(&t[2], q, tq2_tag_cb, &tags[2], 180, 0, 0));
    CHECK(tp_wait_count(&c.n, 3, 5000));
    EXPECT_EQ(c.order[0], 2);
    EXPECT_EQ(c.order[1], 3);
    EXPECT_EQ(c.order[2], 1);
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
    tp_ctx_free(&c);
}
TEST(tp_tq_many_oneshots_all_fire) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(CreateTimerQueueTimer)); REQUIRE(U(DeleteTimerQueueEx));
    enum { N = 200 };
    volatile LONG n = 0;
    HANDLE q = U(CreateTimerQueue)();
    for (int i = 0; i < N; i++) {
        HANDLE t;
        if (!U(CreateTimerQueueTimer)(&t, q, tq2_bump_cb, (PVOID)&n, 10 + (i % 20) * 5, 0, 0)) { CHECK(0); break; }
    }
    CHECK(tp_wait_count(&n, N, 20000));
    Sleep(100);
    EXPECT_EQ(n, N);
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
}
TEST(tp_tq_huge_due_times_create_and_delete) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(CreateTimerQueueTimer)); REQUIRE(U(DeleteTimerQueueTimer));
    REQUIRE(U(DeleteTimerQueueEx));
    HANDLE q = U(CreateTimerQueue)();
    DWORD dues[] = { 0x7fffffff, 0xfffffffe, 0xffffffff };
    for (int i = 0; i < 3; i++) {
        tp_ctx c; tp_ctx_init(&c);
        HANDLE t = NULL;
        SET_ERR(0);
        BOOL r = U(CreateTimerQueueTimer)(&t, q, tp_wot_count, &c, dues[i], 0, 0);
        CHECKF(r, "due 0x%lx err %lu", dues[i], GetLastError());
        if (r) {
            Sleep(20);
            EXPECT_EQ(c.n, 0);
            CHECK(U(DeleteTimerQueueTimer)(q, t, INVALID_HANDLE_VALUE));
        }
        tp_ctx_free(&c);
    }
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
}
TEST(tp_tq_set_timer_queue_timer_wrapper) {
    if (t_is_wine()) { t_skip("wine: SetTimerQueueTimer unimplemented"); return; }
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(SetTimerQueueTimer)); REQUIRE(U(DeleteTimerQueueEx));
    tp_ctx c; tp_ctx_init(&c);
    HANDLE q = U(CreateTimerQueue)();
    HANDLE t = U(SetTimerQueueTimer)(q, tp_wot_count, &c, 20, 0, FALSE);
    CHECK(t != NULL);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    EXPECT_EQ(c.val, 1);
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
    tp_ctx_free(&c);
}

/* ---- alertable wait interplay ---- */
static void CALLBACK tq_apc_fn(ULONG_PTR p) { InterlockedIncrement((volatile LONG *)p); }
typedef struct { HANDLE th; volatile LONG *cnt; } tq_apc_arg;
static VOID CALLBACK tq_apc_cb(PVOID p, BOOLEAN fired) {
    tq_apc_arg *a = (tq_apc_arg *)p;
    BOOL (WINAPI *qa)(PAPCFUNC, HANDLE, ULONG_PTR) = (void *)t_proc("QueueUserAPC");
    qa(tq_apc_fn, a->th, (ULONG_PTR)a->cnt);
}
TEST(tp_tq_alertable_sleep_interrupted_only_by_apc) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(CreateTimerQueueTimer)); REQUIRE(U(DeleteTimerQueueEx));
    REQUIRE(U(QueueUserAPC));
    /* Pool callbacks themselves never wake an alertable wait of another thread. */
    tp_ctx c; tp_ctx_init(&c); c.ev = NULL;
    HANDLE q = U(CreateTimerQueue)(), t = NULL;
    CHECK(U(CreateTimerQueueTimer)(&t, q, tp_wot_count, &c, 10, 20, 0));
    ULONGLONG t0 = tp_now();
    DWORD r = SleepEx(300, TRUE);
    ULONGLONG dt = tp_now() - t0;
    EXPECT_EQ(r, 0);
    CHECK(dt >= 270);
    CHECK(c.n >= 3);
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
    tp_ctx_free(&c);

    /* A timer callback that queues an APC to this thread does. */
    volatile LONG cnt = 0;
    tq_apc_arg a;
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &a.th, 0, FALSE, DUPLICATE_SAME_ACCESS);
    a.cnt = &cnt;
    q = U(CreateTimerQueue)();
    CHECK(U(CreateTimerQueueTimer)(&t, q, tq_apc_cb, &a, 50, 0, 0));
    r = SleepEx(5000, TRUE);
    EXPECT_EQ(r, WAIT_IO_COMPLETION);
    EXPECT_EQ(cnt, 1);
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
    CloseHandle(a.th);
}
TEST(tp_tq_wait_for_object_ex_alertable_with_timer_apc) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(CreateTimerQueueTimer)); REQUIRE(U(DeleteTimerQueueEx));
    volatile LONG cnt = 0;
    tq_apc_arg a;
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &a.th, 0, FALSE, DUPLICATE_SAME_ACCESS);
    a.cnt = &cnt;
    HANDLE ev = tp_ev(FALSE), t = NULL, q = U(CreateTimerQueue)();
    CHECK(U(CreateTimerQueueTimer)(&t, q, tq_apc_cb, &a, 40, 0, 0));
    DWORD r = WaitForSingleObjectEx(ev, 5000, TRUE);
    EXPECT_EQ(r, WAIT_IO_COMPLETION);
    EXPECT_EQ(cnt, 1);
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
    CloseHandle(a.th); CloseHandle(ev);
}

/* A callback may delete its own timer without blocking (non-blocking variant). */
static HANDLE g_selfq, g_selft; static volatile LONG g_self_res; static HANDLE g_self_ev;
static VOID CALLBACK tq_self_del(PVOID p, BOOLEAN fired) {
    BOOL (WINAPI *d)(HANDLE, HANDLE, HANDLE) = (void *)t_proc("DeleteTimerQueueTimer");
    if (d(g_selfq, g_selft, NULL)) g_self_res = 1;
    else g_self_res = 0x10000 | GetLastError();
    SetEvent(g_self_ev);
}
TEST(tp_tq_delete_timer_from_own_callback) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(CreateTimerQueueTimer)); REQUIRE(U(DeleteTimerQueueEx));
    g_self_ev = tp_ev(FALSE); g_self_res = -1;
    g_selfq = U(CreateTimerQueue)();
    CHECK(U(CreateTimerQueueTimer)(&g_selft, g_selfq, tq_self_del, NULL, 20, 25, 0));
    CHECK(tp_signaled(g_self_ev, TP_WAIT_MS));
    /* The callback is outstanding while it deletes itself: either TRUE or ERROR_IO_PENDING. */
    CHECKF(g_self_res == 1 || g_self_res == (0x10000 | ERROR_IO_PENDING), "res 0x%lx", (long)g_self_res);
    t_note("self delete result 0x%lx", (long)g_self_res);
    CHECK(U(DeleteTimerQueueEx)(g_selfq, INVALID_HANDLE_VALUE));
    CloseHandle(g_self_ev);
}
