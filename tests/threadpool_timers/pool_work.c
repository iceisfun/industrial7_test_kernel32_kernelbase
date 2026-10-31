/* Work objects, TrySubmitThreadpoolCallback and cleanup groups. */
#include "tp.h"

static PTP_POOL mkpool(DWORD max) {
    PTP_POOL p = U(CreateThreadpool)(NULL);
    if (p && max) U(SetThreadpoolThreadMaximum)(p, max);
    return p;
}
static void env_pool(TP_CALLBACK_ENVIRON *e, PTP_POOL p) { InitializeThreadpoolEnvironment(e); SetThreadpoolCallbackPool(e, p); }

#define NEED_WORK() REQUIRE(U(CreateThreadpoolWork)); REQUIRE(U(SubmitThreadpoolWork)); \
    REQUIRE(U(WaitForThreadpoolWorkCallbacks)); REQUIRE(U(CloseThreadpoolWork))
#define NEED_POOL() REQUIRE(U(CreateThreadpool)); REQUIRE(U(CloseThreadpool)); REQUIRE(U(SetThreadpoolThreadMaximum))
#define NEED_CG() REQUIRE(U(CreateThreadpoolCleanupGroup)); REQUIRE(U(CloseThreadpoolCleanupGroupMembers)); \
    REQUIRE(U(CloseThreadpoolCleanupGroup))

/* ---- work objects ---- */
TEST(tp_work_basic) {
    NEED_WORK();
    tp_ctx c; tp_ctx_init(&c);
    SET_ERR(0);
    PTP_WORK w = U(CreateThreadpoolWork)(tp_work_count, &c, NULL);
    CHECK(w != NULL);
    U(SubmitThreadpoolWork)(w);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    U(WaitForThreadpoolWorkCallbacks)(w, FALSE);
    EXPECT_EQ(c.n, 1);
    CHECK(c.seen_obj == (PVOID)w);               /* the callback receives the work object itself */
    CHECK(c.seen_inst != NULL);
    CHECK(c.tid != GetCurrentThreadId());
    U(CloseThreadpoolWork)(w);
    tp_ctx_free(&c);
}
TEST(tp_work_objects_distinct) {
    NEED_WORK();
    tp_ctx c; tp_ctx_init(&c);
    PTP_WORK a = U(CreateThreadpoolWork)(tp_work_count, &c, NULL), b = U(CreateThreadpoolWork)(tp_work_count, &c, NULL);
    CHECK(a && b && a != b);
    U(CloseThreadpoolWork)(a); U(CloseThreadpoolWork)(b);
    tp_ctx_free(&c);
}
TEST(tp_work_each_submit_is_one_callback) {
    NEED_WORK();
    tp_ctx c; tp_ctx_init(&c); c.ev = NULL;
    PTP_WORK w = U(CreateThreadpoolWork)(tp_work_count, &c, NULL);
    for (int i = 0; i < 25; i++) U(SubmitThreadpoolWork)(w);
    U(WaitForThreadpoolWorkCallbacks)(w, FALSE);
    EXPECT_EQ(c.n, 25);
    U(CloseThreadpoolWork)(w);
    tp_ctx_free(&c);
}
static volatile PVOID wk_got; static HANDLE wk_gev;
static VOID NTAPI wk_ctx_cb(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_WORK w) { wk_got = p; SetEvent(wk_gev); }
TEST(tp_work_context_identity) {
    NEED_WORK();
    wk_gev = tp_ev(FALSE);
    PVOID ctxs[] = { NULL, (PVOID)1, (PVOID)0x123456789abcULL, (PVOID)(LONG_PTR)-1 };
    for (int i = 0; i < 4; i++) {
        wk_got = (PVOID)0x77;
        PTP_WORK w = U(CreateThreadpoolWork)(wk_ctx_cb, ctxs[i], NULL);
        CHECK(w != NULL);
        U(SubmitThreadpoolWork)(w);
        CHECK(tp_signaled(wk_gev, TP_WAIT_MS));
        CHECKF(wk_got == ctxs[i], "i=%d got %p", i, wk_got);
        U(WaitForThreadpoolWorkCallbacks)(w, FALSE);
        U(CloseThreadpoolWork)(w);
    }
    CloseHandle(wk_gev);
}
TEST(tp_work_wait_idle_returns_immediately) {
    NEED_WORK();
    tp_ctx c; tp_ctx_init(&c);
    PTP_WORK w = U(CreateThreadpoolWork)(tp_work_count, &c, NULL);
    ULONGLONG t0 = tp_now();
    U(WaitForThreadpoolWorkCallbacks)(w, FALSE);
    U(WaitForThreadpoolWorkCallbacks)(w, TRUE);
    CHECK(tp_now() - t0 < 1000);
    EXPECT_EQ(c.n, 0);
    U(CloseThreadpoolWork)(w);
    tp_ctx_free(&c);
}
TEST(tp_work_wait_twice_after_run) {
    NEED_WORK();
    tp_ctx c; tp_ctx_init(&c);
    PTP_WORK w = U(CreateThreadpoolWork)(tp_work_count, &c, NULL);
    U(SubmitThreadpoolWork)(w);
    U(WaitForThreadpoolWorkCallbacks)(w, FALSE);
    U(WaitForThreadpoolWorkCallbacks)(w, FALSE);
    U(WaitForThreadpoolWorkCallbacks)(w, TRUE);
    EXPECT_EQ(c.n, 1);
    U(CloseThreadpoolWork)(w);
    tp_ctx_free(&c);
}
TEST(tp_work_create_close_loop) {
    NEED_WORK();
    tp_ctx c; tp_ctx_init(&c);
    for (int i = 0; i < 1000; i++) {
        PTP_WORK w = U(CreateThreadpoolWork)(tp_work_count, &c, NULL);
        if (!w) { CHECK(w != NULL); break; }
        U(CloseThreadpoolWork)(w);
    }
    EXPECT_EQ(c.n, 0);
    tp_ctx_free(&c);
}
TEST(tp_work_wait_blocks_until_callback_done) {
    NEED_WORK();
    tp_ctx c; tp_ctx_init(&c);
    PTP_WORK w = U(CreateThreadpoolWork)(tp_work_gate, &c, NULL);
    U(SubmitThreadpoolWork)(w);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));         /* callback running, parked on the gate */
    tp_delay_arg da; HANDLE th = tp_delayed_set(&da, c.gate, 250);
    ULONGLONG t0 = tp_now();
    U(WaitForThreadpoolWorkCallbacks)(w, FALSE);
    ULONGLONG dt = tp_now() - t0;
    EXPECT_EQ(c.n2, 1);
    CHECK(dt >= 150);
    tp_join(th);
    U(CloseThreadpoolWork)(w);
    tp_ctx_free(&c);
}
TEST(tp_work_wait_cancel_pending_keeps_running_drops_queued) {
    NEED_WORK(); NEED_POOL();
    PTP_POOL p = mkpool(1);
    TP_CALLBACK_ENVIRON e; env_pool(&e, p);
    tp_ctx c; tp_ctx_init(&c);
    PTP_WORK w = U(CreateThreadpoolWork)(tp_work_gate, &c, &e);
    for (int i = 0; i < 6; i++) U(SubmitThreadpoolWork)(w);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));         /* first one is running; five are queued behind it */
    tp_delay_arg da; HANDLE th = tp_delayed_set(&da, c.gate, 250);
    U(WaitForThreadpoolWorkCallbacks)(w, TRUE);
    EXPECT_EQ(c.n2, 1);                           /* the running one completed ... */
    Sleep(150);
    EXPECT_EQ(c.n, 1);                            /* ... and the queued ones never started */
    tp_join(th);
    U(CloseThreadpoolWork)(w);
    U(CloseThreadpool)(p);
    tp_ctx_free(&c);
}
TEST(tp_work_wait_without_cancel_runs_all) {
    NEED_WORK(); NEED_POOL();
    PTP_POOL p = mkpool(1);
    TP_CALLBACK_ENVIRON e; env_pool(&e, p);
    tp_ctx c; tp_ctx_init(&c);
    PTP_WORK w = U(CreateThreadpoolWork)(tp_work_gate, &c, &e);
    for (int i = 0; i < 6; i++) U(SubmitThreadpoolWork)(w);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    SetEvent(c.gate);
    U(WaitForThreadpoolWorkCallbacks)(w, FALSE);
    EXPECT_EQ(c.n, 6);
    EXPECT_EQ(c.n2, 6);
    U(CloseThreadpoolWork)(w);
    U(CloseThreadpool)(p);
    tp_ctx_free(&c);
}
TEST(tp_work_close_with_pending_submit_still_runs) {
    NEED_WORK();
    tp_ctx c; tp_ctx_init(&c);
    PTP_WORK w = U(CreateThreadpoolWork)(tp_work_count, &c, NULL);
    U(SubmitThreadpoolWork)(w);
    U(CloseThreadpoolWork)(w);                    /* the queued callback keeps the object alive */
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    EXPECT_EQ(c.n, 1);
    tp_ctx_free(&c);
}
static VOID NTAPI wk_resubmit(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_WORK w) {
    tp_ctx *c = (tp_ctx *)p;
    void (WINAPI *sub)(PTP_WORK) = (void *)t_proc("SubmitThreadpoolWork");
    if (InterlockedIncrement(&c->n) < 20) sub(w); else SetEvent(c->ev);
}
TEST(tp_work_resubmit_from_own_callback) {
    NEED_WORK();
    tp_ctx c; tp_ctx_init(&c);
    PTP_WORK w = U(CreateThreadpoolWork)(wk_resubmit, &c, NULL);
    U(SubmitThreadpoolWork)(w);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    U(WaitForThreadpoolWorkCallbacks)(w, FALSE);
    EXPECT_EQ(c.n, 20);
    U(CloseThreadpoolWork)(w);
    tp_ctx_free(&c);
}
static VOID NTAPI wk_nested_submit(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_WORK w) {
    tp_ctx *c = (tp_ctx *)p;
    BOOL (WINAPI *ts)(PTP_SIMPLE_CALLBACK, PVOID, PTP_CALLBACK_ENVIRON) = (void *)t_proc("TrySubmitThreadpoolCallback");
    InterlockedIncrement(&c->n);
    if (!ts(tp_cb_count, c, NULL)) InterlockedIncrement(&c->n2);
}
TEST(tp_work_nested_trysubmit_from_callback) {
    NEED_WORK(); REQUIRE(U(TrySubmitThreadpoolCallback));
    tp_ctx c; tp_ctx_init(&c); c.ev = NULL;
    PTP_WORK w = U(CreateThreadpoolWork)(wk_nested_submit, &c, NULL);
    for (int i = 0; i < 10; i++) U(SubmitThreadpoolWork)(w);
    U(WaitForThreadpoolWorkCallbacks)(w, FALSE);
    CHECK(tp_wait_count(&c.n, 20, TP_WAIT_MS));   /* 10 outer + 10 inner */
    EXPECT_EQ(c.n2, 0);
    U(CloseThreadpoolWork)(w);
    tp_ctx_free(&c);
}
static VOID NTAPI wk_prio(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_WORK w) {
    tp_ctx *c = (tp_ctx *)p; c->val = GetThreadPriority(GetCurrentThread()); SetEvent(c->ev);
}
TEST(tp_work_pool_thread_has_normal_priority) {
    NEED_WORK();
    tp_ctx c; tp_ctx_init(&c); c.val = 99;
    PTP_WORK w = U(CreateThreadpoolWork)(wk_prio, &c, NULL);
    U(SubmitThreadpoolWork)(w);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    EXPECT_EQ(c.val, THREAD_PRIORITY_NORMAL);
    U(WaitForThreadpoolWorkCallbacks)(w, FALSE);
    U(CloseThreadpoolWork)(w);
    tp_ctx_free(&c);
}
typedef struct { HANDLE mine, other; volatile LONG ok; } rdv;
static VOID NTAPI wk_rdv(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_WORK w) {
    rdv *r = (rdv *)p;
    SetEvent(r->mine);
    if (WaitForSingleObject(r->other, 10000) == WAIT_OBJECT_0) InterlockedIncrement(&r->ok);
}
TEST(tp_work_two_objects_run_concurrently) {
    NEED_WORK();
    rdv a, b;
    a.mine = b.other = tp_ev(FALSE); a.other = b.mine = tp_ev(FALSE); a.ok = b.ok = 0;
    PTP_WORK wa = U(CreateThreadpoolWork)(wk_rdv, &a, NULL), wb = U(CreateThreadpoolWork)(wk_rdv, &b, NULL);
    U(SubmitThreadpoolWork)(wa); U(SubmitThreadpoolWork)(wb);
    U(WaitForThreadpoolWorkCallbacks)(wa, FALSE); U(WaitForThreadpoolWorkCallbacks)(wb, FALSE);
    EXPECT_EQ(a.ok, 1);                           /* each saw the other: they overlapped in time */
    EXPECT_EQ(b.ok, 1);
    U(CloseThreadpoolWork)(wa); U(CloseThreadpoolWork)(wb);
    CloseHandle(a.mine); CloseHandle(a.other);
}
TEST(tp_work_null_env_same_as_default_env) {
    NEED_WORK();
    TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e);
    tp_ctx c1, c2; tp_ctx_init(&c1); tp_ctx_init(&c2);
    PTP_WORK a = U(CreateThreadpoolWork)(tp_work_count, &c1, NULL), b = U(CreateThreadpoolWork)(tp_work_count, &c2, &e);
    CHECK(a && b);
    U(SubmitThreadpoolWork)(a); U(SubmitThreadpoolWork)(b);
    CHECK(tp_signaled(c1.ev, TP_WAIT_MS)); CHECK(tp_signaled(c2.ev, TP_WAIT_MS));
    U(WaitForThreadpoolWorkCallbacks)(a, FALSE); U(WaitForThreadpoolWorkCallbacks)(b, FALSE);
    U(CloseThreadpoolWork)(a); U(CloseThreadpoolWork)(b);
    DestroyThreadpoolEnvironment(&e);
    tp_ctx_free(&c1); tp_ctx_free(&c2);
}
TEST(tp_work_private_pool_runs_off_main_thread) {
    NEED_WORK(); NEED_POOL();
    PTP_POOL p = mkpool(2);
    TP_CALLBACK_ENVIRON e; env_pool(&e, p);
    tp_ctx c; tp_ctx_init(&c);
    PTP_WORK w = U(CreateThreadpoolWork)(tp_work_count, &c, &e);
    U(SubmitThreadpoolWork)(w);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    CHECK(c.tid != 0 && c.tid != GetCurrentThreadId());
    U(WaitForThreadpoolWorkCallbacks)(w, FALSE);
    U(CloseThreadpoolWork)(w);
    U(CloseThreadpool)(p);
    tp_ctx_free(&c);
}
static VOID NTAPI wk_slow_inc(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_WORK w) { Sleep(30); InterlockedIncrement((volatile LONG *)p); }
TEST(tp_work_many_objects_one_pool) {
    NEED_WORK(); NEED_POOL();
    PTP_POOL p = mkpool(4);
    TP_CALLBACK_ENVIRON e; env_pool(&e, p);
    volatile LONG n = 0;
    PTP_WORK ws[50];
    for (int i = 0; i < 50; i++) { ws[i] = U(CreateThreadpoolWork)(tp_work_bump, (PVOID)&n, &e); CHECK(ws[i] != NULL); }
    for (int i = 0; i < 50; i++) U(SubmitThreadpoolWork)(ws[i]);
    for (int i = 0; i < 50; i++) U(WaitForThreadpoolWorkCallbacks)(ws[i], FALSE);
    EXPECT_EQ(n, 50);
    for (int i = 0; i < 50; i++) U(CloseThreadpoolWork)(ws[i]);
    U(CloseThreadpool)(p);
}

/* ---- TrySubmitThreadpoolCallback ---- */
TEST(tp_trysubmit_basic) {
    REQUIRE(U(TrySubmitThreadpoolCallback));
    tp_ctx c; tp_ctx_init(&c);
    SET_ERR(0);
    BOOL r = U(TrySubmitThreadpoolCallback)(tp_cb_count, &c, NULL);
    CHECK(r == TRUE);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    EXPECT_EQ(c.n, 1);
    CHECK(c.seen_inst != NULL);
    CHECK(c.tid != GetCurrentThreadId());
    tp_ctx_free(&c);
}
static volatile PVOID ts_got; static HANDLE ts_gev;
static VOID NTAPI ts_ctx_cb(PTP_CALLBACK_INSTANCE i, PVOID p) { ts_got = p; SetEvent(ts_gev); }
TEST(tp_trysubmit_context_values) {
    REQUIRE(U(TrySubmitThreadpoolCallback));
    ts_gev = tp_ev(FALSE);
    PVOID ctxs[] = { NULL, (PVOID)1, (PVOID)0x123456789abcULL, (PVOID)(LONG_PTR)-1 };
    for (int i = 0; i < 4; i++) {
        ts_got = (PVOID)0x77;
        CHECK(U(TrySubmitThreadpoolCallback)(ts_ctx_cb, ctxs[i], NULL));
        CHECK(tp_signaled(ts_gev, TP_WAIT_MS));
        CHECKF(ts_got == ctxs[i], "i=%d got %p", i, ts_got);
    }
    CloseHandle(ts_gev);
}
TEST(tp_trysubmit_with_default_env) {
    REQUIRE(U(TrySubmitThreadpoolCallback));
    TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e);
    tp_ctx c; tp_ctx_init(&c);
    CHECK(U(TrySubmitThreadpoolCallback)(tp_cb_count, &c, &e));
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    DestroyThreadpoolEnvironment(&e);
    tp_ctx_free(&c);
}
TEST(tp_trysubmit_1000_all_run) {
    REQUIRE(U(TrySubmitThreadpoolCallback));
    volatile LONG n = 0;
    for (int i = 0; i < 1000; i++) CHECK(U(TrySubmitThreadpoolCallback)(tp_simple_bump, (PVOID)&n, NULL));
    CHECK(tp_wait_count(&n, 1000, 20000));
    Sleep(50);
    EXPECT_EQ(n, 1000);
}
TEST(tp_trysubmit_private_pool) {
    REQUIRE(U(TrySubmitThreadpoolCallback)); NEED_POOL();
    PTP_POOL p = mkpool(2);
    TP_CALLBACK_ENVIRON e; env_pool(&e, p);
    tp_ctx c; tp_ctx_init(&c);
    CHECK(U(TrySubmitThreadpoolCallback)(tp_cb_count, &c, &e));
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    CHECK(c.tid != GetCurrentThreadId());
    U(CloseThreadpool)(p);
    tp_ctx_free(&c);
}
TEST(tp_trysubmit_sequential_100) {
    REQUIRE(U(TrySubmitThreadpoolCallback));
    tp_ctx c; tp_ctx_init(&c);
    for (int i = 0; i < 100; i++) {
        CHECK(U(TrySubmitThreadpoolCallback)(tp_cb_count, &c, NULL));
        CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    }
    EXPECT_EQ(c.n, 100);
    tp_ctx_free(&c);
}
TEST(tp_trysubmit_does_not_run_inline) {
    REQUIRE(U(TrySubmitThreadpoolCallback));
    tp_ctx c; tp_ctx_init(&c);
    CHECK(U(TrySubmitThreadpoolCallback)(tp_cb_gate, &c, NULL));
    /* If the callback ran inline on this thread it would deadlock on the gate; it didn't. */
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    SetEvent(c.gate);
    CHECK(tp_wait_count(&c.n2, 1, TP_WAIT_MS));
    tp_ctx_free(&c);
}

/* ---- cleanup groups ---- */
TEST(tp_cg_create_close) {
    NEED_CG();
    SET_ERR(0);
    PTP_CLEANUP_GROUP g = U(CreateThreadpoolCleanupGroup)();
    CHECK(g != NULL);
    U(CloseThreadpoolCleanupGroup)(g);
}
TEST(tp_cg_distinct_and_loop) {
    NEED_CG();
    PTP_CLEANUP_GROUP a = U(CreateThreadpoolCleanupGroup)(), b = U(CreateThreadpoolCleanupGroup)();
    CHECK(a && b && a != b);
    U(CloseThreadpoolCleanupGroup)(a); U(CloseThreadpoolCleanupGroup)(b);
    for (int i = 0; i < 500; i++) {
        PTP_CLEANUP_GROUP g = U(CreateThreadpoolCleanupGroup)();
        if (!g) { CHECK(g != NULL); break; }
        U(CloseThreadpoolCleanupGroup)(g);
    }
}
TEST(tp_cg_close_members_empty_group) {
    NEED_CG();
    PTP_CLEANUP_GROUP g = U(CreateThreadpoolCleanupGroup)();
    U(CloseThreadpoolCleanupGroupMembers)(g, FALSE, NULL);
    U(CloseThreadpoolCleanupGroupMembers)(g, TRUE, NULL);
    U(CloseThreadpoolCleanupGroupMembers)(g, TRUE, (PVOID)0x1234);
    U(CloseThreadpoolCleanupGroup)(g);
}
TEST(tp_cg_close_members_waits_for_callbacks) {
    NEED_WORK(); NEED_CG();
    PTP_CLEANUP_GROUP g = U(CreateThreadpoolCleanupGroup)();
    TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e);
    SetThreadpoolCallbackCleanupGroup(&e, g, NULL);
    tp_ctx c; tp_ctx_init(&c); c.ev = NULL;
    PTP_WORK w1 = U(CreateThreadpoolWork)(tp_work_gate, &c, &e), w2 = U(CreateThreadpoolWork)(tp_work_gate, &c, &e);
    CHECK(w1 && w2);
    U(SubmitThreadpoolWork)(w1); U(SubmitThreadpoolWork)(w2);
    CHECK(tp_wait_count(&c.n, 2, TP_WAIT_MS));
    tp_delay_arg da; HANDLE th = tp_delayed_set(&da, c.gate, 250);
    ULONGLONG t0 = tp_now();
    U(CloseThreadpoolCleanupGroupMembers)(g, FALSE, NULL);
    CHECK(tp_now() - t0 >= 150);
    EXPECT_EQ(c.n2, 2);                           /* both callbacks finished; members are now closed */
    tp_join(th);
    U(CloseThreadpoolCleanupGroup)(g);
    DestroyThreadpoolEnvironment(&e);
    tp_ctx_free(&c);
}

static volatile LONG cg_cancel_calls, cg_cancel_bad_cleanup;
static PVOID cg_expect_cleanup;
static VOID NTAPI cg_cancel_cb(PVOID obj, PVOID cleanup) {
    InterlockedIncrement(&cg_cancel_calls);
    if (cleanup != cg_expect_cleanup) InterlockedIncrement(&cg_cancel_bad_cleanup);
}
TEST(tp_cg_cancel_pending_runs_cancel_callback) {
    NEED_WORK(); NEED_CG(); NEED_POOL();
    PTP_POOL p = mkpool(1);
    PTP_CLEANUP_GROUP g = U(CreateThreadpoolCleanupGroup)();
    TP_CALLBACK_ENVIRON e; env_pool(&e, p);
    SetThreadpoolCallbackCleanupGroup(&e, g, cg_cancel_cb);
    tp_ctx c; tp_ctx_init(&c);
    cg_cancel_calls = cg_cancel_bad_cleanup = 0;
    cg_expect_cleanup = (PVOID)0xC1EA9;
    PTP_WORK w = U(CreateThreadpoolWork)(tp_work_gate, &c, &e);
    for (int i = 0; i < 5; i++) U(SubmitThreadpoolWork)(w);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    tp_delay_arg da; HANDLE th = tp_delayed_set(&da, c.gate, 250);
    U(CloseThreadpoolCleanupGroupMembers)(g, TRUE, cg_expect_cleanup);
    EXPECT_EQ(c.n, 1);                            /* queued callbacks were cancelled */
    EXPECT_EQ(c.n2, 1);                           /* the running one was waited for */
    EXPECT_EQ(cg_cancel_bad_cleanup, 0);          /* cancel callback always receives our cleanup context */
    t_note("cleanup-group cancel callback invoked %ld time(s)", (long)cg_cancel_calls);
    CHECK(cg_cancel_calls <= 1);                  /* at most one call per cancelled member object */
    tp_join(th);
    U(CloseThreadpoolCleanupGroup)(g);
    U(CloseThreadpool)(p);
    tp_ctx_free(&c);
}
TEST(tp_cg_cancel_idle_members_invokes_no_callback) {
    NEED_WORK(); NEED_CG();
    PTP_CLEANUP_GROUP g = U(CreateThreadpoolCleanupGroup)();
    TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e);
    SetThreadpoolCallbackCleanupGroup(&e, g, cg_cancel_cb);
    cg_cancel_calls = cg_cancel_bad_cleanup = 0; cg_expect_cleanup = NULL;
    tp_ctx c; tp_ctx_init(&c);
    PTP_WORK w = U(CreateThreadpoolWork)(tp_work_count, &c, &e);
    CHECK(w != NULL);
    U(CloseThreadpoolCleanupGroupMembers)(g, TRUE, NULL);
    t_note("idle member cancel callbacks: %ld", (long)cg_cancel_calls);
    EXPECT_EQ(c.n, 0);
    U(CloseThreadpoolCleanupGroup)(g);
    tp_ctx_free(&c);
}
TEST(tp_cg_group_reusable_after_close_members) {
    NEED_WORK(); NEED_CG();
    PTP_CLEANUP_GROUP g = U(CreateThreadpoolCleanupGroup)();
    TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e);
    SetThreadpoolCallbackCleanupGroup(&e, g, NULL);
    for (int round = 0; round < 3; round++) {
        volatile LONG n = 0;
        PTP_WORK w = U(CreateThreadpoolWork)(tp_work_bump, (PVOID)&n, &e);
        CHECKF(w != NULL, "round %d", round);
        U(SubmitThreadpoolWork)(w);
        U(CloseThreadpoolCleanupGroupMembers)(g, FALSE, NULL);
        EXPECT_EQ(n, 1);
    }
    U(CloseThreadpoolCleanupGroup)(g);
}
TEST(tp_cg_member_closed_explicitly_before_group) {
    NEED_WORK(); NEED_CG();
    PTP_CLEANUP_GROUP g = U(CreateThreadpoolCleanupGroup)();
    TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e);
    SetThreadpoolCallbackCleanupGroup(&e, g, NULL);
    volatile LONG n = 0;
    PTP_WORK w1 = U(CreateThreadpoolWork)(tp_work_bump, (PVOID)&n, &e);
    PTP_WORK w2 = U(CreateThreadpoolWork)(tp_work_bump, (PVOID)&n, &e);
    U(SubmitThreadpoolWork)(w1);
    U(WaitForThreadpoolWorkCallbacks)(w1, FALSE);
    U(CloseThreadpoolWork)(w1);                   /* removes it from the group */
    U(CloseThreadpoolCleanupGroupMembers)(g, FALSE, NULL);   /* releases w2 */
    EXPECT_EQ(n, 1);
    U(CloseThreadpoolCleanupGroup)(g);
}
TEST(tp_cg_trysubmit_members_waited) {
    REQUIRE(U(TrySubmitThreadpoolCallback)); NEED_CG();
    PTP_CLEANUP_GROUP g = U(CreateThreadpoolCleanupGroup)();
    TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e);
    SetThreadpoolCallbackCleanupGroup(&e, g, NULL);
    volatile LONG n = 0;
    for (int i = 0; i < 100; i++) CHECK(U(TrySubmitThreadpoolCallback)(tp_simple_bump, (PVOID)&n, &e));
    U(CloseThreadpoolCleanupGroupMembers)(g, FALSE, NULL);
    EXPECT_EQ(n, 100);                            /* every member callback has completed */
    U(CloseThreadpoolCleanupGroup)(g);
}
TEST(tp_cg_two_groups_isolated) {
    NEED_WORK(); NEED_CG(); NEED_POOL();
    PTP_POOL p = mkpool(1);
    PTP_CLEANUP_GROUP ga = U(CreateThreadpoolCleanupGroup)(), gb = U(CreateThreadpoolCleanupGroup)();
    TP_CALLBACK_ENVIRON ea, eb; env_pool(&ea, p); env_pool(&eb, p);
    SetThreadpoolCallbackCleanupGroup(&ea, ga, NULL); SetThreadpoolCallbackCleanupGroup(&eb, gb, NULL);
    tp_ctx blocker, a, b; tp_ctx_init(&blocker); tp_ctx_init(&a); tp_ctx_init(&b);
    a.ev = b.ev = NULL;
    PTP_WORK wk = U(CreateThreadpoolWork)(tp_work_gate, &blocker, &ea);
    PTP_WORK wa = U(CreateThreadpoolWork)(tp_work_count, &a, &ea), wb = U(CreateThreadpoolWork)(tp_work_count, &b, &eb);
    U(SubmitThreadpoolWork)(wk);
    CHECK(tp_signaled(blocker.ev, TP_WAIT_MS));   /* the only pool thread is parked */
    U(SubmitThreadpoolWork)(wa); U(SubmitThreadpoolWork)(wb);
    tp_delay_arg da; HANDLE th = tp_delayed_set(&da, blocker.gate, 200);
    U(CloseThreadpoolCleanupGroupMembers)(ga, TRUE, NULL);   /* cancels A's queued item only */
    EXPECT_EQ(a.n, 0);
    U(CloseThreadpoolCleanupGroupMembers)(gb, FALSE, NULL);
    EXPECT_EQ(b.n, 1);
    tp_join(th);
    U(CloseThreadpoolCleanupGroup)(ga); U(CloseThreadpoolCleanupGroup)(gb);
    U(CloseThreadpool)(p);
    tp_ctx_free(&blocker); tp_ctx_free(&a); tp_ctx_free(&b);
}
TEST(tp_cg_timer_members_cancelled) {
    REQUIRE(U(CreateThreadpoolTimer)); REQUIRE(U(SetThreadpoolTimer)); NEED_CG();
    PTP_CLEANUP_GROUP g = U(CreateThreadpoolCleanupGroup)();
    TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e);
    SetThreadpoolCallbackCleanupGroup(&e, g, NULL);
    tp_ctx c; tp_ctx_init(&c);
    PTP_TIMER t = U(CreateThreadpoolTimer)(tp_timer_count, &c, &e);
    CHECK(t != NULL);
    FILETIME ft; tp_rel_ft(&ft, 300);
    U(SetThreadpoolTimer)(t, &ft, 0, 0);
    U(CloseThreadpoolCleanupGroupMembers)(g, TRUE, NULL);
    Sleep(500);
    EXPECT_EQ(c.n, 0);
    U(CloseThreadpoolCleanupGroup)(g);
    tp_ctx_free(&c);
}
TEST(tp_cg_wait_members_closed) {
    REQUIRE(U(CreateThreadpoolWait)); REQUIRE(U(SetThreadpoolWait)); NEED_CG();
    PTP_CLEANUP_GROUP g = U(CreateThreadpoolCleanupGroup)();
    TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e);
    SetThreadpoolCallbackCleanupGroup(&e, g, NULL);
    tp_ctx c; tp_ctx_init(&c);
    HANDLE ev = tp_ev(FALSE);
    PTP_WAIT w = U(CreateThreadpoolWait)(tp_wait_cbcount, &c, &e);
    CHECK(w != NULL);
    U(SetThreadpoolWait)(w, ev, NULL);
    U(CloseThreadpoolCleanupGroupMembers)(g, TRUE, NULL);
    SetEvent(ev);
    Sleep(200);
    EXPECT_EQ(c.n, 0);
    U(CloseThreadpoolCleanupGroup)(g);
    CloseHandle(ev); tp_ctx_free(&c);
}
TEST(tp_cg_create_close_with_many_members) {
    NEED_WORK(); NEED_CG();
    PTP_CLEANUP_GROUP g = U(CreateThreadpoolCleanupGroup)();
    TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e);
    SetThreadpoolCallbackCleanupGroup(&e, g, NULL);
    volatile LONG n = 0;
    for (int i = 0; i < 300; i++) {
        PTP_WORK w = U(CreateThreadpoolWork)(tp_work_bump, (PVOID)&n, &e);
        if (!w) { CHECK(w != NULL); break; }
        if (i % 2) U(SubmitThreadpoolWork)(w);
    }
    U(CloseThreadpoolCleanupGroupMembers)(g, FALSE, NULL);
    EXPECT_EQ(n, 150);
    U(CloseThreadpoolCleanupGroup)(g);
}
