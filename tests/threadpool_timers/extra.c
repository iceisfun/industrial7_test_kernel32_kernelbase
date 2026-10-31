/* Timing drift and miscellaneous behavioural checks. Timing assertions are deliberately generous. */
#include "tp.h"

typedef struct { volatile LONG n; ULONGLONG t[16]; HANDLE ev; } stamp;
static VOID NTAPI st_timer_cb(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_TIMER t) {
    stamp *s = (stamp *)p;
    LONG k = InterlockedIncrement(&s->n) - 1;
    if (k < 16) s->t[k] = GetTickCount64();
    if (k == 9) SetEvent(s->ev);
}
static VOID CALLBACK st_wot_cb(PVOID p, BOOLEAN f) {
    stamp *s = (stamp *)p;
    LONG k = InterlockedIncrement(&s->n) - 1;
    if (k < 16) s->t[k] = GetTickCount64();
    if (k == 9) SetEvent(s->ev);
}

TEST(tp_extra_threadpool_timer_drift_bounded) {
    REQUIRE(U(CreateThreadpoolTimer)); REQUIRE(U(SetThreadpoolTimer));
    REQUIRE(U(WaitForThreadpoolTimerCallbacks)); REQUIRE(U(CloseThreadpoolTimer));
    stamp s; memset(&s, 0, sizeof s); s.ev = tp_ev(FALSE);
    PTP_TIMER t = U(CreateThreadpoolTimer)(st_timer_cb, &s, NULL);
    FILETIME ft; tp_rel_ft(&ft, 100);
    ULONGLONG t0 = tp_now();
    U(SetThreadpoolTimer)(t, &ft, 100, 0);
    CHECK(tp_signaled(s.ev, TP_WAIT_MS));
    ULONGLONG dt = s.t[9] - t0;
    U(SetThreadpoolTimer)(t, NULL, 0, 0);
    U(WaitForThreadpoolTimerCallbacks)(t, TRUE);
    t_note("10th tick of a 100ms periodic timer after %llu ms (ideal 1000)", dt);
    CHECK(dt >= 800 && dt <= 3000);
    for (int i = 1; i < 10; i++) CHECKF(s.t[i] >= s.t[i - 1], "tick %d out of order", i);
    U(CloseThreadpoolTimer)(t);
    CloseHandle(s.ev);
}
TEST(tp_extra_timer_queue_drift_bounded) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(CreateTimerQueueTimer)); REQUIRE(U(DeleteTimerQueueEx));
    stamp s; memset(&s, 0, sizeof s); s.ev = tp_ev(FALSE);
    HANDLE q = U(CreateTimerQueue)(), t = NULL;
    ULONGLONG t0 = tp_now();
    CHECK(U(CreateTimerQueueTimer)(&t, q, st_wot_cb, &s, 100, 100, 0));
    CHECK(tp_signaled(s.ev, TP_WAIT_MS));
    ULONGLONG dt = s.t[9] - t0;
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
    t_note("10th tick of a 100ms timer-queue timer after %llu ms (ideal 1000)", dt);
    CHECK(dt >= 800 && dt <= 3000);
    CloseHandle(s.ev);
}
TEST(tp_extra_timer_queue_in_timer_thread_flag_short_callbacks) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(CreateTimerQueueTimer)); REQUIRE(U(DeleteTimerQueueEx));
    tp_ctx c; tp_ctx_init(&c); c.ev = NULL;
    HANDLE q = U(CreateTimerQueue)(), t = NULL;
    CHECK(U(CreateTimerQueueTimer)(&t, q, tp_wot_count, &c, 10, 20, WT_EXECUTEINTIMERTHREAD));
    CHECK(tp_wait_count(&c.n, 10, 5000));
    CHECK(c.tid != GetCurrentThreadId());
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
    tp_ctx_free(&c);
}
TEST(tp_extra_registered_wait_timeout_accuracy) {
    REQUIRE(U(RegisterWaitForSingleObject)); REQUIRE(U(UnregisterWaitEx));
    if (t_is_wine()) { t_skip("wine does not re-arm timed-out registered waits"); return; }
    stamp s; memset(&s, 0, sizeof s); s.ev = tp_ev(FALSE);
    HANDLE ev = tp_ev(FALSE), h = NULL;
    ULONGLONG t0 = tp_now();
    CHECK(U(RegisterWaitForSingleObject)(&h, ev, st_wot_cb, &s, 120, WT_EXECUTEDEFAULT));
    CHECK(tp_signaled(s.ev, TP_WAIT_MS));
    ULONGLONG dt = s.t[9] - t0;
    CHECK(U(UnregisterWaitEx)(h, INVALID_HANDLE_VALUE));
    t_note("10th repeated timeout of a 120ms registered wait after %llu ms (ideal 1200)", dt);
    CHECK(dt >= 1000 && dt <= 4000);
    CloseHandle(ev); CloseHandle(s.ev);
}
TEST(tp_extra_work_and_timer_share_default_pool) {
    REQUIRE(U(CreateThreadpoolWork)); REQUIRE(U(SubmitThreadpoolWork)); REQUIRE(U(WaitForThreadpoolWorkCallbacks));
    REQUIRE(U(CloseThreadpoolWork)); REQUIRE(U(CreateThreadpoolTimer)); REQUIRE(U(SetThreadpoolTimer));
    REQUIRE(U(WaitForThreadpoolTimerCallbacks)); REQUIRE(U(CloseThreadpoolTimer));
    tp_ctx a, b; tp_ctx_init(&a); tp_ctx_init(&b);
    PTP_WORK w = U(CreateThreadpoolWork)(tp_work_count, &a, NULL);
    PTP_TIMER t = U(CreateThreadpoolTimer)(tp_timer_count, &b, NULL);
    FILETIME ft; tp_rel_ft(&ft, 10);
    U(SubmitThreadpoolWork)(w);
    U(SetThreadpoolTimer)(t, &ft, 0, 0);
    CHECK(tp_signaled(a.ev, TP_WAIT_MS));
    CHECK(tp_signaled(b.ev, TP_WAIT_MS));
    U(WaitForThreadpoolWorkCallbacks)(w, FALSE); U(WaitForThreadpoolTimerCallbacks)(t, FALSE);
    U(CloseThreadpoolWork)(w); U(CloseThreadpoolTimer)(t);
    tp_ctx_free(&a); tp_ctx_free(&b);
}
TEST(tp_extra_qwi_and_trysubmit_share_default_pool_threads) {
    REQUIRE(U(QueueUserWorkItem)); REQUIRE(U(TrySubmitThreadpoolCallback));
    tp_ctx a, b; tp_ctx_init(&a); tp_ctx_init(&b);
    for (int i = 0; i < 20; i++) {
        CHECK(U(QueueUserWorkItem)(tp_wi_count, &a, 0));
        CHECK(tp_signaled(a.ev, TP_WAIT_MS));
        CHECK(U(TrySubmitThreadpoolCallback)(tp_cb_count, &b, NULL));
        CHECK(tp_signaled(b.ev, TP_WAIT_MS));
    }
    EXPECT_EQ(a.n, 20); EXPECT_EQ(b.n, 20);
    tp_ctx_free(&a); tp_ctx_free(&b);
}
TEST(tp_extra_sleepex_alertable_not_woken_by_work_completion) {
    REQUIRE(U(TrySubmitThreadpoolCallback));
    volatile LONG n = 0;
    for (int i = 0; i < 20; i++) U(TrySubmitThreadpoolCallback)(tp_simple_bump, (PVOID)&n, NULL);
    DWORD r = SleepEx(200, TRUE);
    EXPECT_EQ(r, 0);                                /* thread pool completions never queue user APCs here */
    CHECK(tp_wait_count(&n, 20, TP_WAIT_MS));
}
