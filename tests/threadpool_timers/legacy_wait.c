/* RegisterWaitForSingleObject(Ex) / UnregisterWait(Ex). */
#include "tp.h"

typedef BOOL (WINAPI *RWFSO_t)(PHANDLE, HANDLE, WAITORTIMERCALLBACK, PVOID, ULONG, ULONG);

/* Callback that records the TimerOrWaitFired value of the most recent call. */
static VOID CALLBACK rw_slow_cb(PVOID p, BOOLEAN fired) {
    tp_ctx *c = (tp_ctx *)p;
    c->val = fired;
    InterlockedIncrement(&c->n);
    SetEvent(c->ev2);                     /* "started" */
    WaitForSingleObject(c->gate, 20000);  /* hold the callback open */
    Sleep(c->tid);                        /* extra delay, ms stored in tid */
    InterlockedIncrement(&c->n2);         /* "finished" */
    if (c->ev) SetEvent(c->ev);
}

TEST(tp_rw_signaled_fires_with_false) {
    REQUIRE(U(RegisterWaitForSingleObject)); REQUIRE(U(UnregisterWait));
    tp_ctx c; tp_ctx_init(&c);
    HANDLE ev = tp_ev(FALSE), h = (HANDLE)0x55;
    SET_ERR(0);
    BOOL r = U(RegisterWaitForSingleObject)(&h, ev, tp_wot_count, &c, INFINITE, WT_EXECUTEONLYONCE);
    CHECK(r == TRUE);
    CHECK(h != NULL && h != (HANDLE)0x55);
    CHECK(tp_not_signaled(c.ev, 100));
    SetEvent(ev);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    EXPECT_EQ(c.val, 0);                  /* TimerOrWaitFired == FALSE: object signaled */
    EXPECT_EQ(c.n, 1);
    CHECK(U(UnregisterWaitEx)(h, INVALID_HANDLE_VALUE) || GetLastError() == ERROR_IO_PENDING);
    CloseHandle(ev); tp_ctx_free(&c);
}

TEST(tp_rw_timeout_fires_with_true) {
    REQUIRE(U(RegisterWaitForSingleObject));
    tp_ctx c; tp_ctx_init(&c);
    HANDLE ev = tp_ev(FALSE), h = NULL;
    ULONGLONG t0 = tp_now();
    CHECK(U(RegisterWaitForSingleObject)(&h, ev, tp_wot_count, &c, 100, WT_EXECUTEONLYONCE));
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    ULONGLONG dt = tp_now() - t0;
    EXPECT_EQ(c.val, 1);                  /* TimerOrWaitFired == TRUE: timed out */
    CHECK(dt >= 80);
    if (dt > 600) t_note("100ms wait timeout took %llu ms", dt);
    U(UnregisterWaitEx)(h, INVALID_HANDLE_VALUE);
    CloseHandle(ev); tp_ctx_free(&c);
}

TEST(tp_rw_onlyonce_fires_once_with_manual_event) {
    REQUIRE(U(RegisterWaitForSingleObject));
    tp_ctx c; tp_ctx_init(&c);
    HANDLE ev = tp_ev(TRUE), h = NULL;
    SetEvent(ev);
    CHECK(U(RegisterWaitForSingleObject)(&h, ev, tp_wot_count, &c, INFINITE, WT_EXECUTEONLYONCE));
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    Sleep(300);
    EXPECT_EQ(c.n, 1);
    U(UnregisterWaitEx)(h, INVALID_HANDLE_VALUE);
    CloseHandle(ev); tp_ctx_free(&c);
}

TEST(tp_rw_manual_event_repeats_without_onlyonce) {
    REQUIRE(U(RegisterWaitForSingleObject));
    tp_ctx c; tp_ctx_init(&c); c.ev = NULL;
    HANDLE ev = tp_ev(TRUE), h = NULL;
    SetEvent(ev);
    CHECK(U(RegisterWaitForSingleObject)(&h, ev, tp_wot_count, &c, INFINITE, WT_EXECUTEDEFAULT));
    CHECK(tp_wait_count(&c.n, 3, 5000));
    EXPECT_EQ(c.val, 0);
    ResetEvent(ev);
    U(UnregisterWaitEx)(h, INVALID_HANDLE_VALUE);
    LONG after = c.n;
    Sleep(100);
    EXPECT_EQ(c.n, after);                /* nothing after the blocking unregister */
    CloseHandle(ev); tp_ctx_free(&c);
}

TEST(tp_rw_periodic_timeout_without_onlyonce) {
    REQUIRE(U(RegisterWaitForSingleObject));
    tp_ctx c; tp_ctx_init(&c); c.ev = NULL;
    HANDLE ev = tp_ev(FALSE), h = NULL;
    CHECK(U(RegisterWaitForSingleObject)(&h, ev, tp_wot_count, &c, 40, WT_EXECUTEDEFAULT));
    Sleep(1000);
    U(UnregisterWaitEx)(h, INVALID_HANDLE_VALUE);
    if (t_is_wine() && c.n < 5) t_note("wine does not re-arm timed-out waits");
    else CHECK(c.n >= 5);
    CHECK(c.n <= 30);
    EXPECT_EQ(c.val, 1);
    t_note("40ms re-arming wait timeout fired %ld times in 1s", (long)c.n);
    CloseHandle(ev); tp_ctx_free(&c);
}

TEST(tp_rw_signal_sequence_counts) {
    REQUIRE(U(RegisterWaitForSingleObject));
    tp_ctx c; tp_ctx_init(&c); c.ev = NULL;
    HANDLE ev = tp_ev(FALSE), h = NULL;
    CHECK(U(RegisterWaitForSingleObject)(&h, ev, tp_wot_count, &c, INFINITE, WT_EXECUTEDEFAULT));
    for (int i = 1; i <= 5; i++) {
        SetEvent(ev);
        CHECKF(tp_wait_count(&c.n, i, 3000), "signal %d", i);
        EXPECT_EQ(c.val, 0);
    }
    Sleep(100);
    EXPECT_EQ(c.n, 5);
    U(UnregisterWaitEx)(h, INVALID_HANDLE_VALUE);
    CloseHandle(ev); tp_ctx_free(&c);
}

TEST(tp_rw_infinite_does_not_fire) {
    REQUIRE(U(RegisterWaitForSingleObject));
    tp_ctx c; tp_ctx_init(&c);
    HANDLE ev = tp_ev(FALSE), h = NULL;
    CHECK(U(RegisterWaitForSingleObject)(&h, ev, tp_wot_count, &c, INFINITE, WT_EXECUTEONLYONCE));
    Sleep(250);
    EXPECT_EQ(c.n, 0);
    CHECK(U(UnregisterWaitEx)(h, INVALID_HANDLE_VALUE));
    EXPECT_EQ(c.n, 0);
    CloseHandle(ev); tp_ctx_free(&c);
}

TEST(tp_rw_zero_timeout_fires_immediately) {
    REQUIRE(U(RegisterWaitForSingleObject));
    tp_ctx c; tp_ctx_init(&c);
    HANDLE ev = tp_ev(FALSE), h = NULL;
    CHECK(U(RegisterWaitForSingleObject)(&h, ev, tp_wot_count, &c, 0, WT_EXECUTEONLYONCE));
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    EXPECT_EQ(c.val, 1);
    U(UnregisterWaitEx)(h, INVALID_HANDLE_VALUE);
    CloseHandle(ev); tp_ctx_free(&c);
}

static volatile PVOID got; static HANDLE gev;
static VOID CALLBACK rw_ctx_f(PVOID p, BOOLEAN b) { got = p; SetEvent(gev); }
TEST(tp_rw_context_identity) {
    REQUIRE(U(RegisterWaitForSingleObject));
    gev = tp_ev(FALSE);
    HANDLE ev = tp_ev(TRUE), h = NULL;
    SetEvent(ev);
    CHECK(U(RegisterWaitForSingleObject)(&h, ev, rw_ctx_f, (PVOID)0xABCDEF12345ULL, INFINITE, WT_EXECUTEONLYONCE));
    CHECK(tp_signaled(gev, TP_WAIT_MS));
    CHECK(got == (PVOID)0xABCDEF12345ULL);
    U(UnregisterWaitEx)(h, INVALID_HANDLE_VALUE);
    CloseHandle(ev); CloseHandle(gev);
}

TEST(tp_rw_invalid_object_handles) {
    REQUIRE(U(RegisterWaitForSingleObject));
    if (t_is_wine()) { t_skip("wine accepts invalid wait handles and later calls them"); return; }
    /* Per RtlRegisterWait: handle values 0, -1 and -2 are rejected with STATUS_INVALID_PARAMETER. */
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(LONG_PTR)-2 };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        tp_ctx c; tp_ctx_init(&c);
        HANDLE h = (HANDLE)0x1234;
        SET_ERR(0);
        BOOL r = U(RegisterWaitForSingleObject)(&h, bad[i], tp_wot_count, &c, 100, 0);
        CHECKF(r == FALSE, "handle %p", bad[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        CHECK(h == NULL);                 /* *phNewWaitObject is zeroed before validation */
        tp_ctx_free(&c);
    }
}

TEST(tp_rw_null_callback) {
    REQUIRE(U(RegisterWaitForSingleObject));
    if (t_is_wine()) { t_skip("wine accepts a NULL wait callback and later calls it"); return; }
    HANDLE ev = tp_ev(FALSE), h = (HANDLE)0x1234;
    SET_ERR(0);
    CHECK(U(RegisterWaitForSingleObject)(&h, ev, NULL, NULL, 100, 0) == FALSE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CHECK(h == NULL);
    CloseHandle(ev);
}

TEST(tp_rw_handles_distinct) {
    REQUIRE(U(RegisterWaitForSingleObject));
    tp_ctx c; tp_ctx_init(&c); c.ev = NULL;
    HANDLE ev = tp_ev(FALSE), h1 = NULL, h2 = NULL;
    CHECK(U(RegisterWaitForSingleObject)(&h1, ev, tp_wot_count, &c, INFINITE, WT_EXECUTEONLYONCE));
    CHECK(U(RegisterWaitForSingleObject)(&h2, ev, tp_wot_count, &c, INFINITE, WT_EXECUTEONLYONCE));
    CHECK(h1 && h2 && h1 != h2);
    CHECK(U(UnregisterWaitEx)(h1, INVALID_HANDLE_VALUE));
    CHECK(U(UnregisterWaitEx)(h2, INVALID_HANDLE_VALUE));
    CloseHandle(ev); tp_ctx_free(&c);
}

TEST(tp_rw_two_waits_one_event_both_fire) {
    REQUIRE(U(RegisterWaitForSingleObject));
    tp_ctx a, b; tp_ctx_init(&a); tp_ctx_init(&b);
    HANDLE ev = tp_ev(TRUE), h1 = NULL, h2 = NULL;
    CHECK(U(RegisterWaitForSingleObject)(&h1, ev, tp_wot_count, &a, INFINITE, WT_EXECUTEONLYONCE));
    CHECK(U(RegisterWaitForSingleObject)(&h2, ev, tp_wot_count, &b, INFINITE, WT_EXECUTEONLYONCE));
    SetEvent(ev);
    CHECK(tp_signaled(a.ev, TP_WAIT_MS));
    CHECK(tp_signaled(b.ev, TP_WAIT_MS));
    U(UnregisterWaitEx)(h1, INVALID_HANDLE_VALUE); U(UnregisterWaitEx)(h2, INVALID_HANDLE_VALUE);
    CloseHandle(ev); tp_ctx_free(&a); tp_ctx_free(&b);
}

TEST(tp_rw_semaphore_object) {
    REQUIRE(U(RegisterWaitForSingleObject));
    tp_ctx c; tp_ctx_init(&c); c.ev = NULL;
    HANDLE sem = tp_sem(0, 10), h = NULL;
    CHECK(U(RegisterWaitForSingleObject)(&h, sem, tp_wot_count, &c, INFINITE, WT_EXECUTEDEFAULT));
    ReleaseSemaphore(sem, 3, NULL);
    CHECK(tp_wait_count(&c.n, 3, 5000));  /* each fire consumes one count */
    Sleep(100);
    EXPECT_EQ(c.n, 3);
    EXPECT_EQ(WaitForSingleObject(sem, 0), WAIT_TIMEOUT);
    U(UnregisterWaitEx)(h, INVALID_HANDLE_VALUE);
    CloseHandle(sem); tp_ctx_free(&c);
}

static DWORD WINAPI rw_thread_body(LPVOID p) { WaitForSingleObject((HANDLE)p, 5000); return 0; }
TEST(tp_rw_thread_handle_object) {
    REQUIRE(U(RegisterWaitForSingleObject));
    tp_ctx c; tp_ctx_init(&c);
    HANDLE go = tp_ev(FALSE), h = NULL;
    HANDLE th = CreateThread(NULL, 0, rw_thread_body, go, 0, NULL);
    CHECK(U(RegisterWaitForSingleObject)(&h, th, tp_wot_count, &c, INFINITE, WT_EXECUTEONLYONCE));
    CHECK(tp_not_signaled(c.ev, 100));
    SetEvent(go);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    EXPECT_EQ(c.val, 0);
    U(UnregisterWaitEx)(h, INVALID_HANDLE_VALUE);
    WaitForSingleObject(th, 5000); CloseHandle(th); CloseHandle(go); tp_ctx_free(&c);
}

TEST(tp_rw_flag_variants_fire) {
    REQUIRE(U(RegisterWaitForSingleObject));
    static const ULONG flags[] = {
        WT_EXECUTEDEFAULT, WT_EXECUTEINIOTHREAD, WT_EXECUTEINUITHREAD, WT_EXECUTEINWAITTHREAD,
        WT_EXECUTELONGFUNCTION, WT_EXECUTEINPERSISTENTTHREAD, WT_TRANSFER_IMPERSONATION,
        WT_EXECUTEINWAITTHREAD | WT_EXECUTEONLYONCE, WT_EXECUTELONGFUNCTION | WT_EXECUTEONLYONCE,
    };
    for (size_t i = 0; i < sizeof flags / sizeof *flags; i++) {
        tp_ctx c; tp_ctx_init(&c);
        HANDLE ev = tp_ev(FALSE), h = NULL;
        BOOL r = U(RegisterWaitForSingleObject)(&h, ev, tp_wot_count, &c, 30, flags[i] | WT_EXECUTEONLYONCE);
        CHECKF(r, "flags 0x%lx err %lu", flags[i], GetLastError());
        if (r) {
            CHECKF(tp_signaled(c.ev, TP_WAIT_MS), "flags 0x%lx timeout cb", flags[i]);
            EXPECT_EQ(c.val, 1);
            U(UnregisterWaitEx)(h, INVALID_HANDLE_VALUE);
        }
        CloseHandle(ev); tp_ctx_free(&c);
    }
}

TEST(tp_rw_executeinwaitthread_runs) {
    REQUIRE(U(RegisterWaitForSingleObject));
    tp_ctx c; tp_ctx_init(&c);
    HANDLE ev = tp_ev(FALSE), h = NULL;
    SetEvent(ev);
    CHECK(U(RegisterWaitForSingleObject)(&h, ev, tp_wot_count, &c, INFINITE, WT_EXECUTEINWAITTHREAD | WT_EXECUTEONLYONCE));
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    CHECK(c.tid != GetCurrentThreadId());
    U(UnregisterWaitEx)(h, INVALID_HANDLE_VALUE);
    CloseHandle(ev); tp_ctx_free(&c);
}

/* ---- UnregisterWait / UnregisterWaitEx ---- */
TEST(tp_unregisterwait_null_handle) {
    REQUIRE(U(UnregisterWait));
    SET_ERR(0);
    CHECK(U(UnregisterWait)(NULL) == FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(tp_unregisterwaitex_null_handle) {
    REQUIRE(U(UnregisterWaitEx));
    HANDLE comps[] = { NULL, INVALID_HANDLE_VALUE };
    for (size_t i = 0; i < 2; i++) {
        SET_ERR(0);
        CHECK(U(UnregisterWaitEx)(NULL, comps[i]) == FALSE);
        EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
    HANDLE ev = tp_ev(FALSE);
    SET_ERR(0);
    CHECK(U(UnregisterWaitEx)(NULL, ev) == FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
}

TEST(tp_unregisterwait_idle_succeeds) {
    REQUIRE(U(RegisterWaitForSingleObject)); REQUIRE(U(UnregisterWait));
    tp_ctx c; tp_ctx_init(&c);
    HANDLE ev = tp_ev(FALSE), h = NULL;
    CHECK(U(RegisterWaitForSingleObject)(&h, ev, tp_wot_count, &c, INFINITE, WT_EXECUTEDEFAULT));
    SET_ERR(0);
    BOOL r = U(UnregisterWait)(h);
    CHECK(r == TRUE);
    SetEvent(ev);
    Sleep(150);
    EXPECT_EQ(c.n, 0);                    /* no callbacks after UnregisterWait */
    CloseHandle(ev); tp_ctx_free(&c);
}

TEST(tp_unregisterwaitex_idle_completion_variants) {
    REQUIRE(U(RegisterWaitForSingleObject)); REQUIRE(U(UnregisterWaitEx));
    for (int v = 0; v < 3; v++) {
        tp_ctx c; tp_ctx_init(&c);
        HANDLE ev = tp_ev(FALSE), h = NULL, comp = tp_ev(FALSE);
        CHECK(U(RegisterWaitForSingleObject)(&h, ev, tp_wot_count, &c, INFINITE, WT_EXECUTEONLYONCE));
        HANDLE ce = v == 0 ? NULL : v == 1 ? INVALID_HANDLE_VALUE : comp;
        SET_ERR(0);
        CHECKF(U(UnregisterWaitEx)(h, ce) == TRUE, "variant %d err %lu", v, GetLastError());
        CloseHandle(ev); CloseHandle(comp); tp_ctx_free(&c);
    }
}

TEST(tp_unregisterwait_pending_returns_io_pending) {
    REQUIRE(U(RegisterWaitForSingleObject)); REQUIRE(U(UnregisterWait));
    tp_ctx c; tp_ctx_init(&c); c.tid = 0;
    HANDLE ev = tp_ev(FALSE), h = NULL;
    SetEvent(ev);
    CHECK(U(RegisterWaitForSingleObject)(&h, ev, rw_slow_cb, &c, INFINITE, WT_EXECUTEONLYONCE));
    CHECK(tp_signaled(c.ev2, TP_WAIT_MS));        /* callback is running and parked on the gate */
    SET_ERR(0);
    BOOL r = U(UnregisterWait)(h);
    DWORD e = GetLastError();
    CHECK(r == FALSE);
    CHECK(e == ERROR_IO_PENDING);                 /* 997: callback still outstanding */
    EXPECT_EQ(c.n2, 0);
    SetEvent(c.gate);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    EXPECT_EQ(c.n2, 1);
    CloseHandle(ev); tp_ctx_free(&c);
}

TEST(tp_unregisterwaitex_null_completion_pending) {
    REQUIRE(U(RegisterWaitForSingleObject)); REQUIRE(U(UnregisterWaitEx));
    tp_ctx c; tp_ctx_init(&c); c.tid = 0;
    HANDLE ev = tp_ev(FALSE), h = NULL;
    SetEvent(ev);
    CHECK(U(RegisterWaitForSingleObject)(&h, ev, rw_slow_cb, &c, INFINITE, WT_EXECUTEONLYONCE));
    CHECK(tp_signaled(c.ev2, TP_WAIT_MS));
    SET_ERR(0);
    CHECK(U(UnregisterWaitEx)(h, NULL) == FALSE);
    EXPECT_ERR(ERROR_IO_PENDING);
    SetEvent(c.gate);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    CloseHandle(ev); tp_ctx_free(&c);
}

TEST(tp_unregisterwaitex_blocking_waits_for_callback) {
    REQUIRE(U(RegisterWaitForSingleObject)); REQUIRE(U(UnregisterWaitEx));
    tp_ctx c; tp_ctx_init(&c); c.tid = 0;
    HANDLE ev = tp_ev(FALSE), h = NULL;
    SetEvent(ev);
    CHECK(U(RegisterWaitForSingleObject)(&h, ev, rw_slow_cb, &c, INFINITE, WT_EXECUTEONLYONCE));
    CHECK(tp_signaled(c.ev2, TP_WAIT_MS));
    tp_delay_arg da; HANDLE th = tp_delayed_set(&da, c.gate, 250);
    ULONGLONG t0 = tp_now();
    SET_ERR(0);
    BOOL r = U(UnregisterWaitEx)(h, INVALID_HANDLE_VALUE);
    ULONGLONG dt = tp_now() - t0;
    CHECK(r == TRUE);
    CHECK(c.n2 == 1);                              /* callback fully finished before return */
    CHECK(dt >= 150);
    tp_join(th);
    CloseHandle(ev); tp_ctx_free(&c);
}

TEST(tp_unregisterwaitex_event_signalled_after_callback) {
    REQUIRE(U(RegisterWaitForSingleObject)); REQUIRE(U(UnregisterWaitEx));
    tp_ctx c; tp_ctx_init(&c); c.tid = 0;
    HANDLE ev = tp_ev(FALSE), h = NULL, comp = tp_ev(TRUE);
    SetEvent(ev);
    CHECK(U(RegisterWaitForSingleObject)(&h, ev, rw_slow_cb, &c, INFINITE, WT_EXECUTEONLYONCE));
    CHECK(tp_signaled(c.ev2, TP_WAIT_MS));
    SET_ERR(0);
    BOOL r = U(UnregisterWaitEx)(h, comp);
    DWORD e = GetLastError();
    CHECK(r == FALSE);
    CHECK(e == ERROR_IO_PENDING);
    CHECK(tp_not_signaled(comp, 100));             /* callback still parked */
    SetEvent(c.gate);
    CHECK(tp_signaled(comp, TP_WAIT_MS));          /* completion event fires after callback is done */
    EXPECT_EQ(c.n2, 1);
    CloseHandle(ev); CloseHandle(comp); tp_ctx_free(&c);
}

TEST(tp_unregisterwaitex_cancels_future_callbacks) {
    REQUIRE(U(RegisterWaitForSingleObject)); REQUIRE(U(UnregisterWaitEx));
    tp_ctx c; tp_ctx_init(&c); c.ev = NULL;
    HANDLE ev = tp_ev(FALSE), h = NULL;
    CHECK(U(RegisterWaitForSingleObject)(&h, ev, tp_wot_count, &c, 30, WT_EXECUTEDEFAULT));
    Sleep(200);
    CHECK(U(UnregisterWaitEx)(h, INVALID_HANDLE_VALUE));
    LONG n = c.n;
    CHECK(n >= 1);
    Sleep(250);
    EXPECT_EQ(c.n, n);
    CloseHandle(ev); tp_ctx_free(&c);
}

TEST(tp_unregisterwait_does_not_close_event) {
    REQUIRE(U(RegisterWaitForSingleObject)); REQUIRE(U(UnregisterWaitEx));
    tp_ctx c; tp_ctx_init(&c);
    HANDLE ev = tp_ev(FALSE), h = NULL;
    CHECK(U(RegisterWaitForSingleObject)(&h, ev, tp_wot_count, &c, INFINITE, WT_EXECUTEONLYONCE));
    CHECK(U(UnregisterWaitEx)(h, INVALID_HANDLE_VALUE));
    CHECK(SetEvent(ev));                           /* still a valid handle */
    CHECK(tp_signaled(ev, 0));
    CloseHandle(ev); tp_ctx_free(&c);
}

TEST(tp_rw_event_after_close_does_not_fire_after_unregister) {
    REQUIRE(U(RegisterWaitForSingleObject)); REQUIRE(U(UnregisterWaitEx));
    tp_ctx c; tp_ctx_init(&c);
    HANDLE ev = tp_ev(FALSE), h = NULL;
    CHECK(U(RegisterWaitForSingleObject)(&h, ev, tp_wot_count, &c, INFINITE, WT_EXECUTEONLYONCE));
    CHECK(U(UnregisterWaitEx)(h, INVALID_HANDLE_VALUE));
    CloseHandle(ev);                               /* closing after unregistering is safe */
    Sleep(50);
    EXPECT_EQ(c.n, 0);
    tp_ctx_free(&c);
}

/* ---- RegisterWaitForSingleObjectEx ---- */
TEST(tp_rwex_basic) {
    RegisterWaitForSingleObjectEx_pfn f = UN(RegisterWaitForSingleObjectEx);
    REQUIRE(f); REQUIRE(U(UnregisterWaitEx));
    tp_ctx c; tp_ctx_init(&c);
    HANDLE ev = tp_ev(FALSE);
    HANDLE h = f(ev, tp_wot_count, &c, INFINITE, WT_EXECUTEONLYONCE);
    CHECK(h != NULL);
    SetEvent(ev);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    EXPECT_EQ(c.val, 0);
    CHECK(U(UnregisterWaitEx)(h, INVALID_HANDLE_VALUE));
    CloseHandle(ev); tp_ctx_free(&c);
}
TEST(tp_rwex_invalid) {
    RegisterWaitForSingleObjectEx_pfn f = UN(RegisterWaitForSingleObjectEx);
    REQUIRE(f);
    if (t_is_wine()) { t_skip("wine accepts invalid wait arguments"); return; }
    tp_ctx c; tp_ctx_init(&c);
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE };
    for (int i = 0; i < 2; i++) {
        SET_ERR(0);
        HANDLE h = f(bad[i], tp_wot_count, &c, 100, 0);
        CHECK(h == NULL);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
    HANDLE ev = tp_ev(FALSE);
    SET_ERR(0);
    CHECK(f(ev, NULL, &c, 100, 0) == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CloseHandle(ev); tp_ctx_free(&c);
}
TEST(tp_rwex_timeout) {
    RegisterWaitForSingleObjectEx_pfn f = UN(RegisterWaitForSingleObjectEx);
    REQUIRE(f); REQUIRE(U(UnregisterWaitEx));
    tp_ctx c; tp_ctx_init(&c);
    HANDLE ev = tp_ev(FALSE);
    HANDLE h = f(ev, tp_wot_count, &c, 50, WT_EXECUTEONLYONCE);
    CHECK(h != NULL);
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    EXPECT_EQ(c.val, 1);
    U(UnregisterWaitEx)(h, INVALID_HANDLE_VALUE);
    CloseHandle(ev); tp_ctx_free(&c);
}

TEST(tp_rw_many_waits_all_fire) {
    REQUIRE(U(RegisterWaitForSingleObject)); REQUIRE(U(UnregisterWaitEx));
    enum { N = 100 };
    static HANDLE evs[N], hs[N];
    tp_ctx c; tp_ctx_init(&c); c.ev = NULL;
    for (int i = 0; i < N; i++) {
        evs[i] = tp_ev(FALSE); hs[i] = NULL;
        CHECK(U(RegisterWaitForSingleObject)(&hs[i], evs[i], tp_wot_count, &c, INFINITE, WT_EXECUTEONLYONCE));
    }
    for (int i = 0; i < N; i++) SetEvent(evs[i]);
    CHECK(tp_wait_count(&c.n, N, 15000));
    Sleep(50);
    EXPECT_EQ(c.n, N);
    for (int i = 0; i < N; i++) { CHECK(U(UnregisterWaitEx)(hs[i], INVALID_HANDLE_VALUE)); CloseHandle(evs[i]); }
    tp_ctx_free(&c);
}

TEST(tp_rw_register_unregister_loop) {
    REQUIRE(U(RegisterWaitForSingleObject)); REQUIRE(U(UnregisterWait));
    tp_ctx c; tp_ctx_init(&c); c.ev = NULL;
    HANDLE ev = tp_ev(FALSE);
    for (int i = 0; i < 500; i++) {
        HANDLE h = NULL;
        if (!U(RegisterWaitForSingleObject)(&h, ev, tp_wot_count, &c, INFINITE, WT_EXECUTEONLYONCE)) { CHECK(0); break; }
        if (!U(UnregisterWait)(h)) { CHECKF(0, "unregister iteration %d err %lu", i, GetLastError()); break; }
    }
    EXPECT_EQ(c.n, 0);
    CloseHandle(ev); tp_ctx_free(&c);
}

/* A callback that unregisters its own (WT_EXECUTEONLYONCE) wait must not deadlock. */
static HANDLE g_selfh; static HANDLE g_selfev; static volatile LONG g_selfres;
static VOID CALLBACK rw_self_unreg(PVOID p, BOOLEAN b) {
    BOOL (WINAPI *u)(HANDLE) = (void *)t_proc("UnregisterWait");
    g_selfres = u(g_selfh) ? 1 : (LONG)(0x10000 | GetLastError());
    SetEvent(g_selfev);
}
TEST(tp_unregisterwait_from_own_callback) {
    REQUIRE(U(RegisterWaitForSingleObject)); REQUIRE(U(UnregisterWait));
    HANDLE ev = tp_ev(FALSE);
    g_selfev = tp_ev(FALSE); g_selfres = -1; g_selfh = NULL;
    CHECK(U(RegisterWaitForSingleObject)(&g_selfh, ev, rw_self_unreg, NULL, INFINITE, WT_EXECUTEONLYONCE | WT_EXECUTEINWAITTHREAD));
    SetEvent(ev);
    CHECK(tp_signaled(g_selfev, TP_WAIT_MS));
    /* Result is either success or ERROR_IO_PENDING (own callback outstanding); never a hang. */
    CHECKF(g_selfres == 1 || g_selfres == (0x10000 | ERROR_IO_PENDING), "res 0x%lx", (long)g_selfres);
    t_note("self unregister result 0x%lx", (long)g_selfres);
    Sleep(50);
    CloseHandle(ev); CloseHandle(g_selfev);
}
