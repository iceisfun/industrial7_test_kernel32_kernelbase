/* QueueUserWorkItem and export-level checks for the threadpool/timer API family. */
#include "tp.h"

/* ---- export presence / forwarding ---- */
static const char *const k32_kb_both[] = {
    "QueueUserWorkItem", "RegisterWaitForSingleObjectEx", "UnregisterWaitEx", "CreateTimerQueue",
    "DeleteTimerQueueEx", "CreateTimerQueueTimer", "ChangeTimerQueueTimer", "DeleteTimerQueueTimer",
    "CreateThreadpool", "CloseThreadpool", "SetThreadpoolThreadMaximum", "SetThreadpoolThreadMinimum",
    "SetThreadpoolStackInformation", "QueryThreadpoolStackInformation", "CreateThreadpoolCleanupGroup",
    "CloseThreadpoolCleanupGroupMembers", "CloseThreadpoolCleanupGroup", "CreateThreadpoolWork",
    "SubmitThreadpoolWork", "WaitForThreadpoolWorkCallbacks", "CloseThreadpoolWork",
    "TrySubmitThreadpoolCallback", "CreateThreadpoolTimer", "SetThreadpoolTimer", "SetThreadpoolTimerEx",
    "IsThreadpoolTimerSet", "WaitForThreadpoolTimerCallbacks", "CloseThreadpoolTimer", "CreateThreadpoolWait",
    "SetThreadpoolWait", "SetThreadpoolWaitEx", "WaitForThreadpoolWaitCallbacks", "CloseThreadpoolWait",
    "CreateThreadpoolIo", "StartThreadpoolIo", "CancelThreadpoolIo", "WaitForThreadpoolIoCallbacks",
    "CloseThreadpoolIo", "SetEventWhenCallbackReturns", "ReleaseSemaphoreWhenCallbackReturns",
    "ReleaseMutexWhenCallbackReturns", "LeaveCriticalSectionWhenCallbackReturns",
    "FreeLibraryWhenCallbackReturns", "CallbackMayRunLong", "DisassociateCurrentThreadFromCallback",
};
static const char *const k32_only[] = {
    "RegisterWaitForSingleObject", "UnregisterWait", "DeleteTimerQueue", "CancelTimerQueueTimer",
    "SetTimerQueueTimer",
};

TEST(tp_exports_kernel32_present) {
    for (size_t i = 0; i < sizeof k32_kb_both / sizeof *k32_kb_both; i++) {
        FARPROC p = t_proc_k32(k32_kb_both[i]);
        if (!p && t_is_wine()) { t_note("wine lacks %s", k32_kb_both[i]); continue; }
        CHECKF(p != NULL, "%s missing from kernel32", k32_kb_both[i]);
    }
    for (size_t i = 0; i < sizeof k32_only / sizeof *k32_only; i++) {
        FARPROC p = t_proc_k32(k32_only[i]);
        CHECKF(p != NULL, "%s missing from kernel32", k32_only[i]);
    }
}
TEST(tp_exports_kernelbase_present) {
    for (size_t i = 0; i < sizeof k32_kb_both / sizeof *k32_kb_both; i++) {
        FARPROC p = t_proc_kb(k32_kb_both[i]);
        if (!p && t_is_wine()) { t_note("wine lacks %s", k32_kb_both[i]); continue; }
        CHECKF(p != NULL, "%s missing from kernelbase", k32_kb_both[i]);
    }
}
TEST(tp_exports_kernel32_only_not_in_kernelbase) {
    for (size_t i = 0; i < sizeof k32_only / sizeof *k32_only; i++) {
        CHECKF(t_proc_kb(k32_only[i]) == NULL, "%s unexpectedly exported by kernelbase", k32_only[i]);
    }
}
TEST(tp_exports_oobe_present) {
    if (t_is_wine()) { t_skip("OOBE wait APIs not provided by wine"); return; }
    CHECK(t_proc_k32("RegisterWaitUntilOOBECompleted") != NULL);
    CHECK(t_proc_k32("UnregisterWaitUntilOOBECompleted") != NULL);
}

/* ---- QueueUserWorkItem ---- */
TEST(tp_qwi_basic) {
    REQUIRE(U(QueueUserWorkItem));
    tp_ctx c; tp_ctx_init(&c);
    SET_ERR(0);
    BOOL r = U(QueueUserWorkItem)(tp_wi_count, &c, WT_EXECUTEDEFAULT);
    CHECK(r == TRUE);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    EXPECT_EQ(c.n, 1);
    tp_ctx_free(&c);
}

TEST(tp_qwi_runs_on_other_thread) {
    REQUIRE(U(QueueUserWorkItem));
    tp_ctx c; tp_ctx_init(&c);
    CHECK(U(QueueUserWorkItem)(tp_wi_count, &c, 0));
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    CHECK(c.tid != 0);
    CHECK(c.tid != GetCurrentThreadId());
    tp_ctx_free(&c);
}

static DWORD WINAPI wi_store_ctx(LPVOID p) {
    tp_ctx *c = (tp_ctx *)((PVOID *)p)[0];
    c->seen_ctx = p;
    InterlockedIncrement(&c->n);
    SetEvent(c->ev);
    return 0;
}
TEST(tp_qwi_context_identity) {
    REQUIRE(U(QueueUserWorkItem));
    tp_ctx c; tp_ctx_init(&c);
    PVOID box[2] = { &c, 0 };
    CHECK(U(QueueUserWorkItem)(wi_store_ctx, box, 0));
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    CHECK(c.seen_ctx == (PVOID)box);
    tp_ctx_free(&c);
}

static volatile ULONG_PTR g_wi_seen;
static HANDLE g_wi_ev;
static DWORD WINAPI wi_record(LPVOID p) { g_wi_seen = (ULONG_PTR)p; SetEvent(g_wi_ev); return 0; }
TEST(tp_qwi_context_values) {
    REQUIRE(U(QueueUserWorkItem));
    static const ULONG_PTR vals[] = { 0, 1, 0x1234, (ULONG_PTR)-1, (ULONG_PTR)0x8000000000000000ULL, 0xdeadbeef };
    g_wi_ev = tp_ev(FALSE);
    for (size_t i = 0; i < sizeof vals / sizeof *vals; i++) {
        g_wi_seen = 0x77;
        CHECK(U(QueueUserWorkItem)(wi_record, (PVOID)vals[i], 0));
        CHECK(tp_signaled(g_wi_ev, TP_WAIT_MS));
        CHECKF(g_wi_seen == vals[i], "i=%zu got %llx", i, (unsigned long long)g_wi_seen);
    }
    CloseHandle(g_wi_ev);
}

TEST(tp_qwi_flags_table) {
    REQUIRE(U(QueueUserWorkItem));
    static const ULONG flags[] = {
        WT_EXECUTEDEFAULT, WT_EXECUTEINIOTHREAD, WT_EXECUTEINUITHREAD, WT_EXECUTEINWAITTHREAD,
        WT_EXECUTELONGFUNCTION, WT_EXECUTEINPERSISTENTTHREAD, WT_TRANSFER_IMPERSONATION,
        WT_EXECUTELONGFUNCTION | WT_TRANSFER_IMPERSONATION,
        WT_EXECUTEINIOTHREAD | WT_EXECUTELONGFUNCTION,
        WT_EXECUTEINPERSISTENTTHREAD | WT_EXECUTELONGFUNCTION,
    };
    for (size_t i = 0; i < sizeof flags / sizeof *flags; i++) {
        tp_ctx c; tp_ctx_init(&c);
        SET_ERR(0);
        BOOL r = U(QueueUserWorkItem)(tp_wi_count, &c, flags[i]);
        CHECKF(r, "flags 0x%lx failed err=%lu", flags[i], GetLastError());
        CHECKF(tp_signaled(c.ev, TP_WAIT_MS), "flags 0x%lx never ran", flags[i]);
        tp_ctx_free(&c);
    }
}

TEST(tp_qwi_1000_all_run) {
    REQUIRE(U(QueueUserWorkItem));
    tp_ctx c; tp_ctx_init(&c);
    c.ev = NULL;
    for (int i = 0; i < 1000; i++) CHECK(U(QueueUserWorkItem)(tp_wi_count, &c, 0));
    CHECK(tp_wait_count(&c.n, 1000, 20000));
    Sleep(50);
    EXPECT_EQ(c.n, 1000);   /* none ran twice */
    tp_ctx_free(&c);
}

TEST(tp_qwi_1000_long_function) {
    REQUIRE(U(QueueUserWorkItem));
    tp_ctx c; tp_ctx_init(&c);
    c.ev = NULL;
    for (int i = 0; i < 1000; i++) CHECK(U(QueueUserWorkItem)(tp_wi_count, &c, WT_EXECUTELONGFUNCTION));
    CHECK(tp_wait_count(&c.n, 1000, 20000));
    EXPECT_EQ(c.n, 1000);
    tp_ctx_free(&c);
}

TEST(tp_qwi_normal_thread_priority) {
    REQUIRE(U(QueueUserWorkItem));
    static volatile int prio; static HANDLE ev;
    ev = tp_ev(FALSE); prio = 99;
    DWORD WINAPI f(LPVOID p) { prio = GetThreadPriority(GetCurrentThread()); SetEvent(ev); return 0; }
    CHECK(U(QueueUserWorkItem)(f, NULL, 0));
    CHECK(tp_signaled(ev, TP_WAIT_MS));
    CHECK(prio == THREAD_PRIORITY_NORMAL);
    CloseHandle(ev);
}

static DWORD WINAPI wi_nested(LPVOID p) {
    tp_ctx *c = (tp_ctx *)p;
    LONG n = InterlockedIncrement(&c->n);
    if (n < 20) {
        BOOL (WINAPI *q)(LPTHREAD_START_ROUTINE, PVOID, ULONG) = (void *)t_proc("QueueUserWorkItem");
        if (!q(wi_nested, c, 0)) InterlockedIncrement(&c->n2);
    } else SetEvent(c->ev);
    return 0;
}
TEST(tp_qwi_nested_queue_from_callback) {
    REQUIRE(U(QueueUserWorkItem));
    tp_ctx c; tp_ctx_init(&c);
    CHECK(U(QueueUserWorkItem)(wi_nested, &c, 0));
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    EXPECT_EQ(c.n, 20);
    EXPECT_EQ(c.n2, 0);
    tp_ctx_free(&c);
}

static DWORD WINAPI wi_hold(LPVOID p) {
    tp_ctx *c = (tp_ctx *)p;
    tp_enter(c);
    InterlockedIncrement(&c->n);
    WaitForSingleObject(c->gate, 20000);
    tp_leave(c);
    InterlockedIncrement(&c->n2);
    return 0;
}
TEST(tp_qwi_long_functions_run_concurrently) {
    REQUIRE(U(QueueUserWorkItem));
    tp_ctx c; tp_ctx_init(&c);
    enum { N = 6 };
    for (int i = 0; i < N; i++) CHECK(U(QueueUserWorkItem)(wi_hold, &c, WT_EXECUTELONGFUNCTION));
    /* A blocking long function must not starve its siblings: at least 2 run at once. */
    ULONGLONG end = tp_now() + 8000;
    while (c.peak < 2 && tp_now() < end) Sleep(5);
    CHECK(c.peak >= 2);
    t_note("peak concurrency of %d long work items: %ld", N, (long)c.peak);
    /* An unrelated short item still gets a thread while the long ones are parked. */
    tp_ctx d; tp_ctx_init(&d);
    CHECK(U(QueueUserWorkItem)(tp_wi_count, &d, 0));
    CHECK(tp_signaled(d.ev, TP_WAIT_MS));
    tp_ctx_free(&d);
    SetEvent(c.gate);
    CHECK(tp_wait_count(&c.n2, N, 10000));
    tp_ctx_free(&c);
}

TEST(tp_qwi_distinct_calls_distinct_runs) {
    REQUIRE(U(QueueUserWorkItem));
    tp_ctx a, b; tp_ctx_init(&a); tp_ctx_init(&b);
    CHECK(U(QueueUserWorkItem)(tp_wi_count, &a, 0));
    CHECK(U(QueueUserWorkItem)(tp_wi_count, &b, 0));
    CHECK(tp_signaled(a.ev, TP_WAIT_MS));
    CHECK(tp_signaled(b.ev, TP_WAIT_MS));
    EXPECT_EQ(a.n, 1);
    EXPECT_EQ(b.n, 1);
    tp_ctx_free(&a); tp_ctx_free(&b);
}

TEST(tp_qwi_return_value_is_bool_true) {
    REQUIRE(U(QueueUserWorkItem));
    tp_ctx c; tp_ctx_init(&c);
    BOOL r = U(QueueUserWorkItem)(tp_wi_count, &c, 0);
    EXPECT_EQ(r, 1);   /* exactly TRUE, not merely non-zero */
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    tp_ctx_free(&c);
}

TEST(tp_qwi_preserves_last_error_on_success) {
    REQUIRE(U(QueueUserWorkItem));
    tp_ctx c; tp_ctx_init(&c);
    SET_ERR(0x1234);
    CHECK(U(QueueUserWorkItem)(tp_wi_count, &c, 0));
    DWORD e = GetLastError();
    if (e != 0x1234) t_note("QueueUserWorkItem success changed last error to %lu", e);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    tp_ctx_free(&c);
}

static DWORD WINAPI wi_sleepy(LPVOID p) { Sleep(5); InterlockedIncrement((volatile LONG *)p); return 0; }
CHILD(tp_qwi_exit_pending) {
    BOOL (WINAPI *q)(LPTHREAD_START_ROUTINE, PVOID, ULONG) = (void *)t_proc("QueueUserWorkItem");
    if (!q) return 90;
    static volatile LONG n;
    for (int i = 0; i < 100; i++) if (!q(wi_sleepy, (PVOID)&n, 0)) return 91;
    return 5;   /* returning from main with items still queued must not hang */
}
TEST(tp_qwi_process_exit_with_pending_items) {
    REQUIRE(U(QueueUserWorkItem));
    EXPECT_EQ(t_run_child("tp_qwi_exit_pending", "", 20000), 5);
}

static DWORD WINAPI wi_record_tid(LPVOID p) {
    tp_ctx *c = (tp_ctx *)p;
    LONG i = InterlockedIncrement(&c->n) - 1;
    if (i < 64) c->tids[i] = GetCurrentThreadId();
    return 0;
}
TEST(tp_qwi_pool_threads_reused) {
    REQUIRE(U(QueueUserWorkItem));
    tp_ctx c; tp_ctx_init(&c);
    c.ev = NULL;
    for (int i = 0; i < 64; i++) { CHECK(U(QueueUserWorkItem)(wi_record_tid, &c, 0)); }
    CHECK(tp_wait_count(&c.n, 64, 10000));
    int distinct = 0;
    for (int i = 0; i < 64; i++) {
        int dup = 0;
        for (int j = 0; j < i; j++) if (c.tids[j] == c.tids[i]) dup = 1;
        if (!dup) distinct++;
    }
    CHECK(distinct >= 1);
    CHECK(distinct < 64);       /* 64 trivial items must not need 64 threads */
    t_note("64 trivial work items ran on %d distinct threads", distinct);
    tp_ctx_free(&c);
}

TEST(tp_qwi_sequential_waits_100) {
    REQUIRE(U(QueueUserWorkItem));
    tp_ctx c; tp_ctx_init(&c);
    for (int i = 0; i < 100; i++) {
        CHECK(U(QueueUserWorkItem)(tp_wi_count, &c, 0));
        CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    }
    EXPECT_EQ(c.n, 100);
    tp_ctx_free(&c);
}
