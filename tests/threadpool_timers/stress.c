/* Stress and concurrency tests for the threadpool family. All counts are bounded. */
#include "tp.h"

TEST(tp_stress_5000_work_submissions_one_object) {
    REQUIRE(U(CreateThreadpoolWork)); REQUIRE(U(SubmitThreadpoolWork));
    REQUIRE(U(WaitForThreadpoolWorkCallbacks)); REQUIRE(U(CloseThreadpoolWork));
    volatile LONG n = 0;
    PTP_WORK w = U(CreateThreadpoolWork)(tp_work_bump, (PVOID)&n, NULL);
    CHECK(w != NULL);
    for (int i = 0; i < 5000; i++) U(SubmitThreadpoolWork)(w);
    U(WaitForThreadpoolWorkCallbacks)(w, FALSE);
    EXPECT_EQ(n, 5000);
    U(CloseThreadpoolWork)(w);
}
TEST(tp_stress_5000_trysubmit_atomic_counter) {
    REQUIRE(U(TrySubmitThreadpoolCallback));
    volatile LONG n = 0;
    for (int i = 0; i < 5000; i++) if (!U(TrySubmitThreadpoolCallback)(tp_simple_bump, (PVOID)&n, NULL)) { CHECK(0); break; }
    CHECK(tp_wait_count(&n, 5000, 30000));
    Sleep(50);
    EXPECT_EQ(n, 5000);
}
TEST(tp_stress_5000_distinct_work_objects) {
    REQUIRE(U(CreateThreadpoolWork)); REQUIRE(U(SubmitThreadpoolWork));
    REQUIRE(U(WaitForThreadpoolWorkCallbacks)); REQUIRE(U(CloseThreadpoolWork));
    volatile LONG n = 0;
    for (int i = 0; i < 5000; i++) {
        PTP_WORK w = U(CreateThreadpoolWork)(tp_work_bump, (PVOID)&n, NULL);
        if (!w) { CHECK(w != NULL); break; }
        U(SubmitThreadpoolWork)(w);
        U(CloseThreadpoolWork)(w);                 /* closed while queued: callback must still run once */
    }
    CHECK(tp_wait_count(&n, 5000, 30000));
    Sleep(50);
    EXPECT_EQ(n, 5000);
}
TEST(tp_stress_10000_queue_user_work_items) {
    REQUIRE(U(QueueUserWorkItem));
    tp_ctx c; tp_ctx_init(&c); c.ev = NULL;
    for (int i = 0; i < 10000; i++) if (!U(QueueUserWorkItem)(tp_wi_count, &c, 0)) { CHECK(0); break; }
    CHECK(tp_wait_count(&c.n, 10000, 40000));
    Sleep(50);
    EXPECT_EQ(c.n, 10000);
    tp_ctx_free(&c);
}

typedef struct { volatile LONG *n; int count; HANDLE go; } submitter;
static DWORD WINAPI submit_thread(LPVOID p) {
    submitter *s = (submitter *)p;
    BOOL (WINAPI *ts)(PTP_SIMPLE_CALLBACK, PVOID, PTP_CALLBACK_ENVIRON) = (void *)t_proc("TrySubmitThreadpoolCallback");
    WaitForSingleObject(s->go, 10000);
    for (int i = 0; i < s->count; i++) if (!ts(tp_simple_bump, (PVOID)s->n, NULL)) return 1;
    return 0;
}
TEST(tp_stress_concurrent_submitters) {
    REQUIRE(U(TrySubmitThreadpoolCallback));
    volatile LONG n = 0;
    HANDLE go = tp_ev(TRUE);
    submitter s = { &n, 1000, go };
    HANDLE th[4];
    for (int i = 0; i < 4; i++) th[i] = CreateThread(NULL, 0, submit_thread, &s, 0, NULL);
    SetEvent(go);
    for (int i = 0; i < 4; i++) {
        WaitForSingleObject(th[i], 30000);
        DWORD code = 99; GetExitCodeThread(th[i], &code);
        EXPECT_EQ(code, 0);
        CloseHandle(th[i]);
    }
    CHECK(tp_wait_count(&n, 4000, 30000));
    Sleep(50);
    EXPECT_EQ(n, 4000);
    CloseHandle(go);
}
typedef struct { PTP_WORK w; int count; HANDLE go; } wsub;
static DWORD WINAPI work_submit_thread(LPVOID p) {
    wsub *s = (wsub *)p;
    void (WINAPI *sub)(PTP_WORK) = (void *)t_proc("SubmitThreadpoolWork");
    WaitForSingleObject(s->go, 10000);
    for (int i = 0; i < s->count; i++) sub(s->w);
    return 0;
}
TEST(tp_stress_concurrent_submit_same_work_object) {
    REQUIRE(U(CreateThreadpoolWork)); REQUIRE(U(SubmitThreadpoolWork));
    REQUIRE(U(WaitForThreadpoolWorkCallbacks)); REQUIRE(U(CloseThreadpoolWork));
    volatile LONG n = 0;
    PTP_WORK w = U(CreateThreadpoolWork)(tp_work_bump, (PVOID)&n, NULL);
    HANDLE go = tp_ev(TRUE);
    wsub s = { w, 800, go };
    HANDLE th[4];
    for (int i = 0; i < 4; i++) th[i] = CreateThread(NULL, 0, work_submit_thread, &s, 0, NULL);
    SetEvent(go);
    for (int i = 0; i < 4; i++) { WaitForSingleObject(th[i], 30000); CloseHandle(th[i]); }
    U(WaitForThreadpoolWorkCallbacks)(w, FALSE);
    EXPECT_EQ(n, 3200);
    U(CloseThreadpoolWork)(w);
    CloseHandle(go);
}
typedef struct { PTP_TIMER t; int count; HANDLE go; int off; } tsub;
static DWORD WINAPI timer_hammer_thread(LPVOID p) {
    tsub *s = (tsub *)p;
    void (WINAPI *st)(PTP_TIMER, PFILETIME, DWORD, DWORD) = (void *)t_proc("SetThreadpoolTimer");
    WaitForSingleObject(s->go, 10000);
    for (int i = 0; i < s->count; i++) {
        FILETIME ft; tp_rel_ft(&ft, 30000 + s->off);
        st(s->t, &ft, 0, 0);
        if (i % 3 == 0) st(s->t, NULL, 0, 0);
    }
    return 0;
}
TEST(tp_stress_concurrent_set_cancel_same_timer) {
    REQUIRE(U(CreateThreadpoolTimer)); REQUIRE(U(SetThreadpoolTimer)); REQUIRE(U(WaitForThreadpoolTimerCallbacks));
    REQUIRE(U(CloseThreadpoolTimer)); REQUIRE(U(IsThreadpoolTimerSet));
    tp_ctx c; tp_ctx_init(&c);
    PTP_TIMER t = U(CreateThreadpoolTimer)(tp_timer_count, &c, NULL);
    HANDLE go = tp_ev(TRUE);
    tsub s[4];
    HANDLE th[4];
    for (int i = 0; i < 4; i++) { s[i].t = t; s[i].count = 1000; s[i].go = go; s[i].off = i; th[i] = CreateThread(NULL, 0, timer_hammer_thread, &s[i], 0, NULL); }
    SetEvent(go);
    for (int i = 0; i < 4; i++) { WaitForSingleObject(th[i], 30000); CloseHandle(th[i]); }
    U(SetThreadpoolTimer)(t, NULL, 0, 0);
    U(WaitForThreadpoolTimerCallbacks)(t, TRUE);
    CHECK(!U(IsThreadpoolTimerSet)(t));
    EXPECT_EQ(c.n, 0);                              /* 30 s timers never expired */
    U(CloseThreadpoolTimer)(t);
    CloseHandle(go); tp_ctx_free(&c);
}
TEST(tp_stress_pools_with_work_create_close) {
    REQUIRE(U(CreateThreadpool)); REQUIRE(U(CloseThreadpool)); REQUIRE(U(SetThreadpoolThreadMaximum));
    REQUIRE(U(CreateThreadpoolWork)); REQUIRE(U(SubmitThreadpoolWork));
    REQUIRE(U(WaitForThreadpoolWorkCallbacks)); REQUIRE(U(CloseThreadpoolWork));
    for (int r = 0; r < 40; r++) {
        PTP_POOL p = U(CreateThreadpool)(NULL);
        if (!p) { CHECK(p != NULL); break; }
        U(SetThreadpoolThreadMaximum)(p, 3);
        TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e); SetThreadpoolCallbackPool(&e, p);
        volatile LONG n = 0;
        PTP_WORK w = U(CreateThreadpoolWork)(tp_work_bump, (PVOID)&n, &e);
        for (int i = 0; i < 20; i++) U(SubmitThreadpoolWork)(w);
        U(WaitForThreadpoolWorkCallbacks)(w, FALSE);
        if (n != 20) { CHECKF(0, "round %d: n=%ld", r, (long)n); }
        U(CloseThreadpoolWork)(w);
        U(CloseThreadpool)(p);
    }
}
TEST(tp_stress_mixed_object_kinds) {
    REQUIRE(U(CreateThreadpoolWork)); REQUIRE(U(SubmitThreadpoolWork)); REQUIRE(U(WaitForThreadpoolWorkCallbacks));
    REQUIRE(U(CloseThreadpoolWork)); REQUIRE(U(CreateThreadpoolTimer)); REQUIRE(U(SetThreadpoolTimer));
    REQUIRE(U(WaitForThreadpoolTimerCallbacks)); REQUIRE(U(CloseThreadpoolTimer)); REQUIRE(U(CreateThreadpoolWait));
    REQUIRE(U(SetThreadpoolWait)); REQUIRE(U(WaitForThreadpoolWaitCallbacks)); REQUIRE(U(CloseThreadpoolWait));
    enum { N = 60 };
    static PTP_WORK ws[N]; static PTP_TIMER ts[N]; static PTP_WAIT wa[N]; static HANDLE evs[N];
    tp_ctx cw, ct, cwa; tp_ctx_init(&cw); tp_ctx_init(&ct); tp_ctx_init(&cwa);
    cw.ev = ct.ev = cwa.ev = NULL;
    for (int i = 0; i < N; i++) {
        ws[i] = U(CreateThreadpoolWork)(tp_work_count, &cw, NULL);
        ts[i] = U(CreateThreadpoolTimer)(tp_timer_count, &ct, NULL);
        evs[i] = tp_ev(FALSE);
        wa[i] = U(CreateThreadpoolWait)(tp_wait_cbcount, &cwa, NULL);
    }
    for (int i = 0; i < N; i++) {
        FILETIME ft; tp_rel_ft(&ft, 5 + (i % 10) * 3);
        U(SetThreadpoolTimer)(ts[i], &ft, 0, 0);
        U(SetThreadpoolWait)(wa[i], evs[i], NULL);
        U(SubmitThreadpoolWork)(ws[i]);
        SetEvent(evs[i]);
    }
    CHECK(tp_wait_count(&cw.n, N, 20000));
    CHECK(tp_wait_count(&ct.n, N, 20000));
    CHECK(tp_wait_count(&cwa.n, N, 20000));
    Sleep(50);
    EXPECT_EQ(cw.n, N); EXPECT_EQ(ct.n, N); EXPECT_EQ(cwa.n, N);
    for (int i = 0; i < N; i++) {
        U(WaitForThreadpoolWorkCallbacks)(ws[i], FALSE); U(CloseThreadpoolWork)(ws[i]);
        U(WaitForThreadpoolTimerCallbacks)(ts[i], FALSE); U(CloseThreadpoolTimer)(ts[i]);
        U(WaitForThreadpoolWaitCallbacks)(wa[i], FALSE); U(CloseThreadpoolWait)(wa[i]);
        CloseHandle(evs[i]);
    }
    tp_ctx_free(&cw); tp_ctx_free(&ct); tp_ctx_free(&cwa);
}
TEST(tp_stress_default_pool_still_healthy) {
    /* After everything above the default pool must still service a trivial request promptly. */
    REQUIRE(U(TrySubmitThreadpoolCallback));
    tp_ctx c; tp_ctx_init(&c);
    ULONGLONG t0 = tp_now();
    CHECK(U(TrySubmitThreadpoolCallback)(tp_cb_count, &c, NULL));
    CHECK(tp_signaled(c.ev, TP_WAIT_MS));
    CHECK(tp_now() - t0 < 4000);
    tp_ctx_free(&c);
}
TEST(tp_stress_timer_queue_and_threadpool_timers_together) {
    REQUIRE(U(CreateTimerQueue)); REQUIRE(U(CreateTimerQueueTimer)); REQUIRE(U(DeleteTimerQueueEx));
    REQUIRE(U(CreateThreadpoolTimer)); REQUIRE(U(SetThreadpoolTimer)); REQUIRE(U(WaitForThreadpoolTimerCallbacks));
    REQUIRE(U(CloseThreadpoolTimer));
    tp_ctx a, b; tp_ctx_init(&a); tp_ctx_init(&b); a.ev = b.ev = NULL;
    HANDLE q = U(CreateTimerQueue)(), t = NULL;
    CHECK(U(CreateTimerQueueTimer)(&t, q, tp_wot_count, &a, 10, 20, 0));
    PTP_TIMER tt = U(CreateThreadpoolTimer)(tp_timer_count, &b, NULL);
    FILETIME ft; tp_rel_ft(&ft, 10);
    U(SetThreadpoolTimer)(tt, &ft, 20, 0);
    Sleep(400);
    CHECK(U(DeleteTimerQueueEx)(q, INVALID_HANDLE_VALUE));
    U(SetThreadpoolTimer)(tt, NULL, 0, 0);
    U(WaitForThreadpoolTimerCallbacks)(tt, TRUE);
    CHECK(a.n >= 5);
    CHECK(b.n >= 5);
    U(CloseThreadpoolTimer)(tt);
    tp_ctx_free(&a); tp_ctx_free(&b);
}
