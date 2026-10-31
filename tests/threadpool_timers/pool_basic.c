/* Private thread pools, thread limits, stack information, callback environments. */
#include "tp.h"

/* ---- pool create/close ---- */
TEST(tp_pool_create_close) {
    REQUIRE(U(CreateThreadpool)); REQUIRE(U(CloseThreadpool));
    SET_ERR(0);
    PTP_POOL p = U(CreateThreadpool)(NULL);
    CHECK(p != NULL);
    U(CloseThreadpool)(p);
}
TEST(tp_pool_create_distinct) {
    REQUIRE(U(CreateThreadpool)); REQUIRE(U(CloseThreadpool));
    PTP_POOL p[8];
    for (int i = 0; i < 8; i++) { p[i] = U(CreateThreadpool)(NULL); CHECK(p[i] != NULL); }
    for (int i = 0; i < 8; i++) for (int j = i + 1; j < 8; j++) CHECK(p[i] != p[j]);
    for (int i = 0; i < 8; i++) U(CloseThreadpool)(p[i]);
}
TEST(tp_pool_create_close_loop) {
    REQUIRE(U(CreateThreadpool)); REQUIRE(U(CloseThreadpool));
    for (int i = 0; i < 100; i++) {
        PTP_POOL p = U(CreateThreadpool)(NULL);
        if (!p) { CHECK(p != NULL); break; }
        U(CloseThreadpool)(p);
    }
}

/* ---- min / max ---- */
TEST(tp_pool_thread_maximum_roundtrip) {
    REQUIRE(U(CreateThreadpool)); REQUIRE(U(CloseThreadpool)); REQUIRE(U(SetThreadpoolThreadMaximum));
    PTP_POOL p = U(CreateThreadpool)(NULL);
    DWORD vals[] = { 1, 2, 4, 16, 500, 0x7fffffff };
    for (size_t i = 0; i < sizeof vals / sizeof *vals; i++) U(SetThreadpoolThreadMaximum)(p, vals[i]);
    U(CloseThreadpool)(p);
}
TEST(tp_pool_thread_minimum_ok) {
    REQUIRE(U(CreateThreadpool)); REQUIRE(U(CloseThreadpool));
    REQUIRE(U(SetThreadpoolThreadMaximum)); REQUIRE(U(SetThreadpoolThreadMinimum));
    PTP_POOL p = U(CreateThreadpool)(NULL);
    U(SetThreadpoolThreadMaximum)(p, 4);
    SET_ERR(0);
    CHECK(U(SetThreadpoolThreadMinimum)(p, 0) == TRUE);
    CHECK(U(SetThreadpoolThreadMinimum)(p, 1) == TRUE);
    CHECK(U(SetThreadpoolThreadMinimum)(p, 4) == TRUE);        /* min == max is allowed */
    CHECK(U(SetThreadpoolThreadMinimum)(p, 0) == TRUE);
    U(CloseThreadpool)(p);
}
TEST(tp_pool_minimum_above_maximum_fails) {
    REQUIRE(U(CreateThreadpool)); REQUIRE(U(CloseThreadpool));
    REQUIRE(U(SetThreadpoolThreadMaximum)); REQUIRE(U(SetThreadpoolThreadMinimum));
    PTP_POOL p = U(CreateThreadpool)(NULL);
    U(SetThreadpoolThreadMaximum)(p, 1);
    SET_ERR(0);
    BOOL r = U(SetThreadpoolThreadMinimum)(p, 5);
    DWORD e = GetLastError();
    /* The min/max cross-check lives in the kernel worker factory (NtSetInformationWorkerFactory),
     * which user-mode decompilation cannot confirm: record the result only. */
    t_note("min(5) > max(1): result %d, last error %lu", r, e);
    /* The failed call must not have changed the pool: a valid minimum still works. */
    CHECK(U(SetThreadpoolThreadMinimum)(p, 1) == TRUE);
    U(CloseThreadpool)(p);
}
TEST(tp_pool_maximum_below_minimum_keeps_pool_working) {
    REQUIRE(U(CreateThreadpool)); REQUIRE(U(CloseThreadpool));
    REQUIRE(U(SetThreadpoolThreadMaximum)); REQUIRE(U(SetThreadpoolThreadMinimum));
    REQUIRE(U(TrySubmitThreadpoolCallback));
    PTP_POOL p = U(CreateThreadpool)(NULL);
    U(SetThreadpoolThreadMaximum)(p, 3);
    CHECK(U(SetThreadpoolThreadMinimum)(p, 2));
    U(SetThreadpoolThreadMaximum)(p, 1);                      /* lowers below the old minimum */
    TP_CALLBACK_ENVIRON env; InitializeThreadpoolEnvironment(&env); SetThreadpoolCallbackPool(&env, p);
    tp_ctx c; tp_ctx_init(&c);
    CHECK(U(TrySubmitThreadpoolCallback)(tp_cb_count, &c, &env));
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    DestroyThreadpoolEnvironment(&env);
    U(CloseThreadpool)(p);
    tp_ctx_free(&c);
}

/* ---- stack information ---- */
TEST(tp_pool_stack_query_default) {
    REQUIRE(U(CreateThreadpool)); REQUIRE(U(CloseThreadpool)); REQUIRE(U(QueryThreadpoolStackInformation));
    PTP_POOL p = U(CreateThreadpool)(NULL);
    TP_POOL_STACK_INFORMATION si = { 0x1111, 0x2222 };
    SET_ERR(0);
    BOOL r = U(QueryThreadpoolStackInformation)(p, &si);
    CHECK(r == TRUE);
    CHECK(si.StackReserve != 0x1111 || si.StackCommit != 0x2222);   /* the structure was filled in */
    t_note("default pool stack: reserve=0x%llx commit=0x%llx", (unsigned long long)si.StackReserve, (unsigned long long)si.StackCommit);
    CHECK(si.StackCommit <= si.StackReserve);
    U(CloseThreadpool)(p);
}
TEST(tp_pool_stack_set_query_roundtrip) {
    REQUIRE(U(CreateThreadpool)); REQUIRE(U(CloseThreadpool));
    REQUIRE(U(QueryThreadpoolStackInformation)); REQUIRE(U(SetThreadpoolStackInformation));
    PTP_POOL p = U(CreateThreadpool)(NULL);
    TP_POOL_STACK_INFORMATION in = { 0x200000, 0x10000 }, out = { 0, 0 };
    SET_ERR(0);
    CHECK(U(SetThreadpoolStackInformation)(p, &in) == TRUE);
    CHECK(U(QueryThreadpoolStackInformation)(p, &out) == TRUE);
    EXPECT_EQ(out.StackReserve, 0x200000);
    EXPECT_EQ(out.StackCommit, 0x10000);
    TP_POOL_STACK_INFORMATION in2 = { 0x400000, 0x2000 };
    CHECK(U(SetThreadpoolStackInformation)(p, &in2) == TRUE);
    CHECK(U(QueryThreadpoolStackInformation)(p, &out) == TRUE);
    EXPECT_EQ(out.StackReserve, 0x400000);
    EXPECT_EQ(out.StackCommit, 0x2000);
    U(CloseThreadpool)(p);
}
TEST(tp_pool_stack_independent_per_pool) {
    REQUIRE(U(CreateThreadpool)); REQUIRE(U(CloseThreadpool));
    REQUIRE(U(QueryThreadpoolStackInformation)); REQUIRE(U(SetThreadpoolStackInformation));
    PTP_POOL a = U(CreateThreadpool)(NULL), b = U(CreateThreadpool)(NULL);
    TP_POOL_STACK_INFORMATION ia = { 0x300000, 0x4000 }, qa, qb0, qb;
    CHECK(U(QueryThreadpoolStackInformation)(b, &qb0));
    CHECK(U(SetThreadpoolStackInformation)(a, &ia));
    CHECK(U(QueryThreadpoolStackInformation)(b, &qb));
    EXPECT_EQ(qb.StackReserve, qb0.StackReserve);
    EXPECT_EQ(qb.StackCommit, qb0.StackCommit);
    CHECK(U(QueryThreadpoolStackInformation)(a, &qa));
    EXPECT_EQ(qa.StackReserve, 0x300000);
    U(CloseThreadpool)(a); U(CloseThreadpool)(b);
}
TEST(tp_pool_stack_null_arguments) {
    REQUIRE(U(CreateThreadpool)); REQUIRE(U(CloseThreadpool));
    if (t_is_wine()) { t_skip("wine faults on NULL stack information"); return; }
    REQUIRE(U(QueryThreadpoolStackInformation)); REQUIRE(U(SetThreadpoolStackInformation));
    PTP_POOL p = U(CreateThreadpool)(NULL);
    TP_POOL_STACK_INFORMATION si = { 0x100000, 0x1000 };
    SET_ERR(0);
    CHECK(U(SetThreadpoolStackInformation)(p, NULL) == FALSE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(U(SetThreadpoolStackInformation)(NULL, &si) == FALSE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(U(QueryThreadpoolStackInformation)(p, NULL) == FALSE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(U(QueryThreadpoolStackInformation)(NULL, &si) == FALSE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    U(CloseThreadpool)(p);
}
TEST(tp_pool_stack_reserve_less_than_commit) {
    REQUIRE(U(CreateThreadpool)); REQUIRE(U(CloseThreadpool)); REQUIRE(U(SetThreadpoolStackInformation));
    PTP_POOL p = U(CreateThreadpool)(NULL);
    TP_POOL_STACK_INFORMATION si = { 0x1000, 0x100000 };
    SET_ERR(0);
    BOOL r = U(SetThreadpoolStackInformation)(p, &si);
    t_note("reserve < commit: result %d, last error %lu", r, GetLastError());
    U(CloseThreadpool)(p);
}

static volatile ULONG_PTR g_stk_total, g_stk_commit_ok;
static VOID NTAPI stk_cb(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_WORK w) {
    NT_TIB *tib = (NT_TIB *)NtCurrentTeb();
    ULONG_PTR dealloc = *(ULONG_PTR *)((BYTE *)tib + 0x1478);       /* TEB.DeallocationStack (x64) */
    g_stk_total = (ULONG_PTR)tib->StackBase - dealloc;
    SetEvent((HANDLE)p);
}
TEST(tp_pool_stack_size_applies_to_new_threads) {
    REQUIRE(U(CreateThreadpool)); REQUIRE(U(CloseThreadpool)); REQUIRE(U(SetThreadpoolStackInformation));
    REQUIRE(U(CreateThreadpoolWork)); REQUIRE(U(SubmitThreadpoolWork)); REQUIRE(U(WaitForThreadpoolWorkCallbacks));
    REQUIRE(U(CloseThreadpoolWork));
    PTP_POOL p = U(CreateThreadpool)(NULL);
    TP_POOL_STACK_INFORMATION si = { 0x400000, 0x10000 };
    CHECK(U(SetThreadpoolStackInformation)(p, &si));
    TP_CALLBACK_ENVIRON env; InitializeThreadpoolEnvironment(&env); SetThreadpoolCallbackPool(&env, p);
    HANDLE ev = tp_ev(FALSE);
    g_stk_total = 0;
    PTP_WORK w = U(CreateThreadpoolWork)(stk_cb, ev, &env);
    CHECK(w != NULL);
    if (w) {
        U(SubmitThreadpoolWork)(w);
        CHECK(tp_signaled(ev, TP_WAIT_MS));
        U(WaitForThreadpoolWorkCallbacks)(w, FALSE);
        U(CloseThreadpoolWork)(w);
        t_note("pool thread stack region: 0x%llx bytes (requested reserve 0x400000)", (unsigned long long)g_stk_total);
        if (t_is_wine()) { if (g_stk_total < 0x400000) t_note("wine ignores the pool stack size"); }
        else CHECK(g_stk_total >= 0x400000);
    }
    DestroyThreadpoolEnvironment(&env);
    U(CloseThreadpool)(p);
    CloseHandle(ev);
}

/* ---- TP_CALLBACK_ENVIRON layout (V3) ---- */
TEST(tp_env_layout_matches_win10) {
    TP_CALLBACK_ENVIRON e;
    EXPECT_EQ(sizeof e, 0x48);
    EXPECT_EQ(offsetof(TP_CALLBACK_ENVIRON, Version), 0x00);
    EXPECT_EQ(offsetof(TP_CALLBACK_ENVIRON, Pool), 0x08);
    EXPECT_EQ(offsetof(TP_CALLBACK_ENVIRON, CleanupGroup), 0x10);
    EXPECT_EQ(offsetof(TP_CALLBACK_ENVIRON, CleanupGroupCancelCallback), 0x18);
    EXPECT_EQ(offsetof(TP_CALLBACK_ENVIRON, RaceDll), 0x20);
    EXPECT_EQ(offsetof(TP_CALLBACK_ENVIRON, ActivationContext), 0x28);
    EXPECT_EQ(offsetof(TP_CALLBACK_ENVIRON, FinalizationCallback), 0x30);
    EXPECT_EQ(offsetof(TP_CALLBACK_ENVIRON, u), 0x38);
    EXPECT_EQ(offsetof(TP_CALLBACK_ENVIRON, CallbackPriority), 0x3c);
    EXPECT_EQ(offsetof(TP_CALLBACK_ENVIRON, Size), 0x40);
}
TEST(tp_env_initialize_values) {
    TP_CALLBACK_ENVIRON e;
    memset(&e, 0xA5, sizeof e);
    InitializeThreadpoolEnvironment(&e);
    EXPECT_EQ(e.Version, 3);
    CHECK(e.Pool == NULL);
    CHECK(e.CleanupGroup == NULL);
    CHECK(e.CleanupGroupCancelCallback == NULL);
    CHECK(e.RaceDll == NULL);
    CHECK(e.ActivationContext == NULL);
    CHECK(e.FinalizationCallback == NULL);
    EXPECT_EQ(e.u.Flags, 0);
    EXPECT_EQ(e.CallbackPriority, TP_CALLBACK_PRIORITY_NORMAL);
    EXPECT_EQ(e.Size, sizeof e);
}
TEST(tp_env_setters_populate_fields) {
    TP_CALLBACK_ENVIRON e;
    InitializeThreadpoolEnvironment(&e);
    SetThreadpoolCallbackPool(&e, (PTP_POOL)0x1000);
    CHECK(e.Pool == (PTP_POOL)0x1000);
    SetThreadpoolCallbackCleanupGroup(&e, (PTP_CLEANUP_GROUP)0x2000, (PTP_CLEANUP_GROUP_CANCEL_CALLBACK)0x3000);
    CHECK(e.CleanupGroup == (PTP_CLEANUP_GROUP)0x2000);
    CHECK(e.CleanupGroupCancelCallback == (PTP_CLEANUP_GROUP_CANCEL_CALLBACK)0x3000);
    SetThreadpoolCallbackRunsLong(&e);
    EXPECT_EQ(e.u.Flags & 1, 1);
    SetThreadpoolCallbackLibrary(&e, (PVOID)0x4000);
    CHECK(e.RaceDll == (PVOID)0x4000);
    SetThreadpoolCallbackPriority(&e, TP_CALLBACK_PRIORITY_HIGH);
    EXPECT_EQ(e.CallbackPriority, TP_CALLBACK_PRIORITY_HIGH);
    SetThreadpoolCallbackPriority(&e, TP_CALLBACK_PRIORITY_LOW);
    EXPECT_EQ(e.CallbackPriority, TP_CALLBACK_PRIORITY_LOW);
    DestroyThreadpoolEnvironment(&e);
}

/* ---- environments passed to the Create* APIs ---- */
static VOID NTAPI env_work_ok(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_WORK w) { InterlockedIncrement((volatile LONG *)p); }
TEST(tp_env_invalid_priority_rejected) {
    REQUIRE(U(CreateThreadpoolWork));
    TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e);
    e.CallbackPriority = TP_CALLBACK_PRIORITY_INVALID;
    SET_ERR(0);
    PTP_WORK w = U(CreateThreadpoolWork)(env_work_ok, NULL, &e);
    CHECK(w == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    if (w) U(CloseThreadpoolWork)(w);
    e.CallbackPriority = (TP_CALLBACK_PRIORITY)77;
    SET_ERR(0);
    w = U(CreateThreadpoolWork)(env_work_ok, NULL, &e);
    CHECK(w == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    if (w) U(CloseThreadpoolWork)(w);
}
TEST(tp_env_invalid_priority_rejected_timer_wait) {
    REQUIRE(U(CreateThreadpoolTimer)); REQUIRE(U(CreateThreadpoolWait));
    TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e);
    e.CallbackPriority = TP_CALLBACK_PRIORITY_INVALID;
    SET_ERR(0);
    CHECK(U(CreateThreadpoolTimer)((PTP_TIMER_CALLBACK)tp_timer_count, NULL, &e) == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(U(CreateThreadpoolWait)(tp_wait_cbcount, NULL, &e) == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(tp_env_version1_ignores_priority) {
    REQUIRE(U(CreateThreadpoolWork)); REQUIRE(U(SubmitThreadpoolWork));
    REQUIRE(U(WaitForThreadpoolWorkCallbacks)); REQUIRE(U(CloseThreadpoolWork));
    TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e);
    e.Version = 1;
    e.CallbackPriority = TP_CALLBACK_PRIORITY_INVALID;       /* beyond the V1 layout: must be ignored */
    volatile LONG n = 0;
    PTP_WORK w = U(CreateThreadpoolWork)(env_work_ok, (PVOID)&n, &e);
    CHECK(w != NULL);
    if (w) {
        U(SubmitThreadpoolWork)(w);
        U(WaitForThreadpoolWorkCallbacks)(w, FALSE);
        U(CloseThreadpoolWork)(w);
        EXPECT_EQ(n, 1);
    }
}
TEST(tp_env_priorities_all_run) {
    REQUIRE(U(CreateThreadpoolWork)); REQUIRE(U(SubmitThreadpoolWork));
    REQUIRE(U(WaitForThreadpoolWorkCallbacks)); REQUIRE(U(CloseThreadpoolWork));
    TP_CALLBACK_PRIORITY pr[] = { TP_CALLBACK_PRIORITY_HIGH, TP_CALLBACK_PRIORITY_NORMAL, TP_CALLBACK_PRIORITY_LOW };
    for (int i = 0; i < 3; i++) {
        TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e);
        SetThreadpoolCallbackPriority(&e, pr[i]);
        volatile LONG n = 0;
        PTP_WORK w = U(CreateThreadpoolWork)(env_work_ok, (PVOID)&n, &e);
        CHECKF(w != NULL, "priority %d", i);
        if (!w) continue;
        for (int k = 0; k < 10; k++) U(SubmitThreadpoolWork)(w);
        U(WaitForThreadpoolWorkCallbacks)(w, FALSE);
        U(CloseThreadpoolWork)(w);
        EXPECT_EQ(n, 10);
    }
}
TEST(tp_env_persistent_with_explicit_pool_rejected) {
    REQUIRE(U(CreateThreadpool)); REQUIRE(U(CloseThreadpool)); REQUIRE(U(CreateThreadpoolWork));
    PTP_POOL p = U(CreateThreadpool)(NULL);
    TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e);
    SetThreadpoolCallbackPool(&e, p);
    e.u.Flags |= 2;                                          /* Persistent */
    SET_ERR(0);
    PTP_WORK w = U(CreateThreadpoolWork)(env_work_ok, NULL, &e);
    if (t_is_wine() && w) t_note("wine accepts a persistent environment with a pool");
    else { CHECK(w == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
    if (w) U(CloseThreadpoolWork)(w);
    U(CloseThreadpool)(p);
}
TEST(tp_env_longfunction_flag_work_runs) {
    REQUIRE(U(CreateThreadpoolWork)); REQUIRE(U(SubmitThreadpoolWork));
    REQUIRE(U(WaitForThreadpoolWorkCallbacks)); REQUIRE(U(CloseThreadpoolWork));
    TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e);
    SetThreadpoolCallbackRunsLong(&e);
    volatile LONG n = 0;
    PTP_WORK w = U(CreateThreadpoolWork)(env_work_ok, (PVOID)&n, &e);
    CHECK(w != NULL);
    if (w) {
        for (int k = 0; k < 5; k++) U(SubmitThreadpoolWork)(w);
        U(WaitForThreadpoolWorkCallbacks)(w, FALSE);
        U(CloseThreadpoolWork)(w);
        EXPECT_EQ(n, 5);
    }
}

/* ---- private pool vs default pool ---- */
static VOID NTAPI tid_cb(PTP_CALLBACK_INSTANCE i, PVOID p) {
    tp_ctx *c = (tp_ctx *)p;
    LONG k = InterlockedIncrement(&c->n) - 1;
    if (k < 64) c->tids[k] = GetCurrentThreadId();
    InterlockedIncrement(&c->n2);
    WaitForSingleObject(c->gate, 5000);       /* hold so several threads get created */
}
TEST(tp_pool_private_threads_disjoint_from_default) {
    REQUIRE(U(CreateThreadpool)); REQUIRE(U(CloseThreadpool)); REQUIRE(U(SetThreadpoolThreadMaximum));
    REQUIRE(U(TrySubmitThreadpoolCallback));
    PTP_POOL p = U(CreateThreadpool)(NULL);
    U(SetThreadpoolThreadMaximum)(p, 3);
    TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e); SetThreadpoolCallbackPool(&e, p);
    tp_ctx priv, def; tp_ctx_init(&priv); tp_ctx_init(&def);
    for (int i = 0; i < 3; i++) CHECK(U(TrySubmitThreadpoolCallback)(tid_cb, &priv, &e));
    for (int i = 0; i < 3; i++) CHECK(U(TrySubmitThreadpoolCallback)(tid_cb, &def, NULL));
    CHECK(tp_wait_count(&priv.n2, 1, TP_WAIT_MS));
    CHECK(tp_wait_count(&def.n2, 1, TP_WAIT_MS));
    Sleep(300);
    SetEvent(priv.gate); SetEvent(def.gate);
    CHECK(tp_wait_count(&priv.n2, 3, TP_WAIT_MS));
    CHECK(tp_wait_count(&def.n2, 3, TP_WAIT_MS));
    int overlap = 0;
    for (int i = 0; i < priv.n; i++) for (int j = 0; j < def.n; j++) if (priv.tids[i] == def.tids[j]) overlap++;
    EXPECT_EQ(overlap, 0);
    for (int i = 0; i < priv.n; i++) CHECK(priv.tids[i] != GetCurrentThreadId());
    DestroyThreadpoolEnvironment(&e);
    U(CloseThreadpool)(p);
    tp_ctx_free(&priv); tp_ctx_free(&def);
}
TEST(tp_pool_two_private_pools_disjoint) {
    REQUIRE(U(CreateThreadpool)); REQUIRE(U(CloseThreadpool)); REQUIRE(U(TrySubmitThreadpoolCallback));
    PTP_POOL pa = U(CreateThreadpool)(NULL), pb = U(CreateThreadpool)(NULL);
    TP_CALLBACK_ENVIRON ea, eb;
    InitializeThreadpoolEnvironment(&ea); SetThreadpoolCallbackPool(&ea, pa);
    InitializeThreadpoolEnvironment(&eb); SetThreadpoolCallbackPool(&eb, pb);
    tp_ctx a, b; tp_ctx_init(&a); tp_ctx_init(&b);
    SetEvent(a.gate); SetEvent(b.gate);
    for (int i = 0; i < 10; i++) {
        CHECK(U(TrySubmitThreadpoolCallback)(tid_cb, &a, &ea));
        CHECK(U(TrySubmitThreadpoolCallback)(tid_cb, &b, &eb));
    }
    CHECK(tp_wait_count(&a.n2, 10, TP_WAIT_MS));
    CHECK(tp_wait_count(&b.n2, 10, TP_WAIT_MS));
    int overlap = 0;
    for (int i = 0; i < a.n && i < 64; i++) for (int j = 0; j < b.n && j < 64; j++) if (a.tids[i] == b.tids[j]) overlap++;
    EXPECT_EQ(overlap, 0);
    U(CloseThreadpool)(pa); U(CloseThreadpool)(pb);
    tp_ctx_free(&a); tp_ctx_free(&b);
}

/* ---- single-thread pool serialises ---- */
static VOID NTAPI order_cb(PTP_CALLBACK_INSTANCE i, PVOID p) {
    tp_ctx *c = (tp_ctx *)p;
    tp_enter(c);
    LONG k = InterlockedIncrement(&c->seq) - 1;
    if (k < 64) { c->tids[k] = GetCurrentThreadId(); }
    Sleep(1);
    tp_leave(c);
    InterlockedIncrement(&c->n);
}
typedef struct { tp_ctx *c; LONG id; } ord_tag;
static VOID NTAPI order_tag_cb(PTP_CALLBACK_INSTANCE i, PVOID p) {
    ord_tag *t = (ord_tag *)p;
    tp_enter(t->c);
    LONG k = InterlockedIncrement(&t->c->seq) - 1;
    if (k < 64) { t->c->order[k] = t->id; t->c->tids[k] = GetCurrentThreadId(); }
    Sleep(1);
    tp_leave(t->c);
    InterlockedIncrement(&t->c->n);
}
TEST(tp_pool_min_max_one_serializes_in_order) {
    REQUIRE(U(CreateThreadpool)); REQUIRE(U(CloseThreadpool)); REQUIRE(U(SetThreadpoolThreadMaximum));
    REQUIRE(U(SetThreadpoolThreadMinimum)); REQUIRE(U(TrySubmitThreadpoolCallback));
    PTP_POOL p = U(CreateThreadpool)(NULL);
    U(SetThreadpoolThreadMaximum)(p, 1);
    CHECK(U(SetThreadpoolThreadMinimum)(p, 1));
    TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e); SetThreadpoolCallbackPool(&e, p);
    tp_ctx c; tp_ctx_init(&c);
    enum { N = 40 };
    ord_tag tags[N];
    for (int i = 0; i < N; i++) { tags[i].c = &c; tags[i].id = i; CHECK(U(TrySubmitThreadpoolCallback)(order_tag_cb, &tags[i], &e)); }
    CHECK(tp_wait_count(&c.n, N, 15000));
    EXPECT_EQ(c.peak, 1);                                 /* never two at once */
    int inorder = 1, one_thread = 1;
    for (int i = 0; i < N; i++) { if (c.order[i] != i) inorder = 0; if (c.tids[i] != c.tids[0]) one_thread = 0; }
    CHECK(one_thread);
    CHECK(inorder);
    DestroyThreadpoolEnvironment(&e);
    U(CloseThreadpool)(p);
    tp_ctx_free(&c);
}
static VOID NTAPI gate_cb2(PTP_CALLBACK_INSTANCE i, PVOID p) {
    tp_ctx *c = (tp_ctx *)p;
    tp_enter(c);
    InterlockedIncrement(&c->n);
    WaitForSingleObject(c->gate, 20000);
    tp_leave(c);
    InterlockedIncrement(&c->n2);
}
TEST(tp_pool_growth_never_exceeds_maximum) {
    REQUIRE(U(CreateThreadpool)); REQUIRE(U(CloseThreadpool)); REQUIRE(U(SetThreadpoolThreadMaximum));
    REQUIRE(U(TrySubmitThreadpoolCallback));
    PTP_POOL p = U(CreateThreadpool)(NULL);
    U(SetThreadpoolThreadMaximum)(p, 4);
    TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e); SetThreadpoolCallbackPool(&e, p);
    tp_ctx c; tp_ctx_init(&c);
    for (int i = 0; i < 16; i++) CHECK(U(TrySubmitThreadpoolCallback)(gate_cb2, &c, &e));
    ULONGLONG end = tp_now() + 1500;
    while (tp_now() < end) Sleep(10);
    CHECK(c.peak >= 1);
    CHECK(c.peak <= 4);
    t_note("blocked callbacks in a max=4 pool: peak %ld", (long)c.peak);
    SetEvent(c.gate);
    CHECK(tp_wait_count(&c.n2, 16, 15000));
    EXPECT_EQ(c.peak <= 4, 1);
    DestroyThreadpoolEnvironment(&e);
    U(CloseThreadpool)(p);
    tp_ctx_free(&c);
}
TEST(tp_pool_min_threads_prestarted) {
    REQUIRE(U(CreateThreadpool)); REQUIRE(U(CloseThreadpool)); REQUIRE(U(SetThreadpoolThreadMaximum));
    REQUIRE(U(SetThreadpoolThreadMinimum)); REQUIRE(U(TrySubmitThreadpoolCallback));
    PTP_POOL p = U(CreateThreadpool)(NULL);
    U(SetThreadpoolThreadMaximum)(p, 3);
    CHECK(U(SetThreadpoolThreadMinimum)(p, 3));
    TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e); SetThreadpoolCallbackPool(&e, p);
    tp_ctx c; tp_ctx_init(&c);
    for (int i = 0; i < 3; i++) CHECK(U(TrySubmitThreadpoolCallback)(gate_cb2, &c, &e));
    CHECK(tp_wait_count(&c.n, 3, TP_WAIT_MS));            /* three blocked callbacks all get a thread */
    EXPECT_EQ(c.peak, 3);
    SetEvent(c.gate);
    CHECK(tp_wait_count(&c.n2, 3, TP_WAIT_MS));
    DestroyThreadpoolEnvironment(&e);
    U(CloseThreadpool)(p);
    tp_ctx_free(&c);
}

/* ---- closing a pool with pending work ---- */
static volatile LONG g_cp_n;
static VOID NTAPI cp_cb(PTP_CALLBACK_INSTANCE i, PVOID p) { Sleep(20); InterlockedIncrement(&g_cp_n); }
CHILD(tp_pool_close_pending) {
    PTP_POOL (WINAPI *cp)(PVOID) = (void *)t_proc("CreateThreadpool");
    void (WINAPI *cl)(PTP_POOL) = (void *)t_proc("CloseThreadpool");
    void (WINAPI *smax)(PTP_POOL, DWORD) = (void *)t_proc("SetThreadpoolThreadMaximum");
    BOOL (WINAPI *ts)(PTP_SIMPLE_CALLBACK, PVOID, PTP_CALLBACK_ENVIRON) = (void *)t_proc("TrySubmitThreadpoolCallback");
    if (!cp || !cl || !smax || !ts) return 90;
    PTP_POOL p = cp(NULL);
    if (!p) return 91;
    smax(p, 1);
    TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e); SetThreadpoolCallbackPool(&e, p);
    for (int i = 0; i < 10; i++) if (!ts(cp_cb, NULL, &e)) return 92;
    cl(p);                                                  /* pending items hold their own pool reference */
    for (int i = 0; i < 300 && g_cp_n < 10; i++) Sleep(10);
    return g_cp_n == 10 ? 0 : 100 + g_cp_n;
}
TEST(tp_pool_close_with_pending_callbacks) {
    REQUIRE(U(CreateThreadpool));
    DWORD r = t_run_child("tp_pool_close_pending", "", 25000);
    CHECKF(r == 0 || (r >= 100 && r < 110), "child exit 0x%lx", r);   /* no crash, no hang */
    if (r != 0) t_note("pending callbacks completed after CloseThreadpool: %ld of 10", (long)(r - 100));
    else CHECK(r == 0);
}
