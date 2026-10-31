#include "so.h"

static HANDLE tm_create(int w, SECURITY_ATTRIBUTES *sa, BOOL manual, const char *name) {
    WCHAR wn[300];
    if (w) { if (name) so_wide(name, wn, 300); return U(CreateWaitableTimerW)(sa, manual, name ? wn : NULL); }
    return U(CreateWaitableTimerA)(sa, manual, name);
}
static HANDLE tm_createex(int w, SECURITY_ATTRIBUTES *sa, const char *name, DWORD fl, DWORD acc) {
    WCHAR wn[300];
    if (w) { if (name) so_wide(name, wn, 300); return U(CreateWaitableTimerExW)(sa, name ? wn : NULL, fl, acc); }
    return U(CreateWaitableTimerExA)(sa, name, fl, acc);
}
static HANDLE tm_open(int w, DWORD acc, BOOL inh, const char *name) {
    WCHAR wn[300];
    if (w) { if (name) so_wide(name, wn, 300); return U(OpenWaitableTimerW)(acc, inh, name ? wn : NULL); }
    return U(OpenWaitableTimerA)(acc, inh, name);
}
#define NEED_TM() do { REQUIRE(U(CreateWaitableTimerA)); REQUIRE(U(CreateWaitableTimerW)); REQUIRE(U(CreateWaitableTimerExA)); REQUIRE(U(CreateWaitableTimerExW)); \
    REQUIRE(U(OpenWaitableTimerA)); REQUIRE(U(OpenWaitableTimerW)); REQUIRE(U(SetWaitableTimer)); REQUIRE(U(CancelWaitableTimer)); } while (0)

static LARGE_INTEGER rel_ms(LONGLONG ms) { LARGE_INTEGER l; l.QuadPart = -ms * 10000; return l; }
static LARGE_INTEGER abs_in_ms(LONGLONG ms) {
    FILETIME ft; GetSystemTimeAsFileTime(&ft);
    LARGE_INTEGER l; l.LowPart = ft.dwLowDateTime; l.HighPart = ft.dwHighDateTime; l.QuadPart += ms * 10000; return l;
}

TEST(timer_create_unsignaled_manual_auto) {
    NEED_TM();
    for (int w = 0; w < 2; w++)
    for (int m = 0; m < 2; m++) {
        SET_ERR(77);
        HANDLE h = tm_create(w, NULL, m, NULL);
        CHECK(h != NULL); if (!h) continue;
        EXPECT_ERR(0);
        EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_TIMEOUT);
        CloseHandle(h);
    }
}
TEST(timer_set_relative_fires_manual_stays_signaled) {
    NEED_TM();
    HANDLE h = U(CreateWaitableTimerA)(NULL, TRUE, NULL);
    CHECK(h != NULL); if (!h) return;
    LARGE_INTEGER d = rel_ms(40);
    EXPECT_EQ(U(SetWaitableTimer)(h, &d, 0, NULL, NULL, FALSE), TRUE);
    EXPECT_EQ(WaitForSingleObject(h, 3000), WAIT_OBJECT_0);
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
    CloseHandle(h);
}
TEST(timer_set_relative_fires_auto_resets) {
    NEED_TM();
    HANDLE h = U(CreateWaitableTimerW)(NULL, FALSE, NULL);
    CHECK(h != NULL); if (!h) return;
    LARGE_INTEGER d = rel_ms(40);
    EXPECT_EQ(U(SetWaitableTimer)(h, &d, 0, NULL, NULL, FALSE), TRUE);
    EXPECT_EQ(WaitForSingleObject(h, 3000), WAIT_OBJECT_0);
    EXPECT_EQ(WaitForSingleObject(h, 100), WAIT_TIMEOUT);
    CloseHandle(h);
}
TEST(timer_relative_elapsed_time_sane) {
    NEED_TM();
    HANDLE h = U(CreateWaitableTimerA)(NULL, TRUE, NULL);
    CHECK(h != NULL); if (!h) return;
    LARGE_INTEGER d = rel_ms(200);
    DWORD t0 = GetTickCount();
    U(SetWaitableTimer)(h, &d, 0, NULL, NULL, FALSE);
    EXPECT_EQ(WaitForSingleObject(h, 4000), WAIT_OBJECT_0);
    DWORD dt = GetTickCount() - t0;
    CHECKF(dt >= 150 && dt < 3000, "elapsed %lu ms", dt);
    CloseHandle(h);
}
TEST(timer_not_signaled_before_due) {
    NEED_TM();
    HANDLE h = U(CreateWaitableTimerA)(NULL, TRUE, NULL);
    CHECK(h != NULL); if (!h) return;
    LARGE_INTEGER d = rel_ms(60000);
    EXPECT_EQ(U(SetWaitableTimer)(h, &d, 0, NULL, NULL, FALSE), TRUE);
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_TIMEOUT);
    EXPECT_EQ(WaitForSingleObject(h, 100), WAIT_TIMEOUT);
    EXPECT_EQ(U(CancelWaitableTimer)(h), TRUE);
    CloseHandle(h);
}
TEST(timer_zero_due_fires_immediately) {
    NEED_TM();
    HANDLE h = U(CreateWaitableTimerA)(NULL, TRUE, NULL);
    CHECK(h != NULL); if (!h) return;
    LARGE_INTEGER d; d.QuadPart = 0;
    EXPECT_EQ(U(SetWaitableTimer)(h, &d, 0, NULL, NULL, FALSE), TRUE);
    EXPECT_EQ(WaitForSingleObject(h, 2000), WAIT_OBJECT_0);
    CloseHandle(h);
}
TEST(timer_absolute_due_time) {
    NEED_TM();
    HANDLE h = U(CreateWaitableTimerA)(NULL, TRUE, NULL);
    CHECK(h != NULL); if (!h) return;
    LARGE_INTEGER d = abs_in_ms(150);
    EXPECT_EQ(U(SetWaitableTimer)(h, &d, 0, NULL, NULL, FALSE), TRUE);
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_TIMEOUT);
    EXPECT_EQ(WaitForSingleObject(h, 4000), WAIT_OBJECT_0);
    CloseHandle(h);
}
TEST(timer_absolute_in_past_fires_immediately) {
    NEED_TM();
    HANDLE h = U(CreateWaitableTimerA)(NULL, TRUE, NULL);
    CHECK(h != NULL); if (!h) return;
    LARGE_INTEGER d = abs_in_ms(-60000);
    EXPECT_EQ(U(SetWaitableTimer)(h, &d, 0, NULL, NULL, FALSE), TRUE);
    EXPECT_EQ(WaitForSingleObject(h, 2000), WAIT_OBJECT_0);
    d.QuadPart = 1;   /* long before any real time */
    EXPECT_EQ(U(SetWaitableTimer)(h, &d, 0, NULL, NULL, FALSE), TRUE);
    EXPECT_EQ(WaitForSingleObject(h, 2000), WAIT_OBJECT_0);
    CloseHandle(h);
}
TEST(timer_set_resets_signaled_state) {
    NEED_TM();
    HANDLE h = U(CreateWaitableTimerA)(NULL, TRUE, NULL);
    CHECK(h != NULL); if (!h) return;
    LARGE_INTEGER d = rel_ms(10);
    U(SetWaitableTimer)(h, &d, 0, NULL, NULL, FALSE);
    EXPECT_EQ(WaitForSingleObject(h, 3000), WAIT_OBJECT_0);
    d = rel_ms(60000);
    EXPECT_EQ(U(SetWaitableTimer)(h, &d, 0, NULL, NULL, FALSE), TRUE);
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_TIMEOUT);   /* Set clears the signaled state */
    U(CancelWaitableTimer)(h);
    CloseHandle(h);
}
TEST(timer_cancel_keeps_signaled_state) {
    NEED_TM();
    HANDLE h = U(CreateWaitableTimerA)(NULL, TRUE, NULL);
    CHECK(h != NULL); if (!h) return;
    LARGE_INTEGER d = rel_ms(10);
    U(SetWaitableTimer)(h, &d, 0, NULL, NULL, FALSE);
    EXPECT_EQ(WaitForSingleObject(h, 3000), WAIT_OBJECT_0);
    EXPECT_EQ(U(CancelWaitableTimer)(h), TRUE);
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
    CloseHandle(h);
}
TEST(timer_cancel_prevents_firing) {
    NEED_TM();
    HANDLE h = U(CreateWaitableTimerA)(NULL, TRUE, NULL);
    CHECK(h != NULL); if (!h) return;
    LARGE_INTEGER d = rel_ms(150);
    U(SetWaitableTimer)(h, &d, 0, NULL, NULL, FALSE);
    EXPECT_EQ(U(CancelWaitableTimer)(h), TRUE);
    EXPECT_EQ(WaitForSingleObject(h, 500), WAIT_TIMEOUT);
    CloseHandle(h);
}
TEST(timer_cancel_unset_and_twice) {
    NEED_TM();
    HANDLE h = U(CreateWaitableTimerW)(NULL, FALSE, NULL);
    CHECK(h != NULL); if (!h) return;
    EXPECT_EQ(U(CancelWaitableTimer)(h), TRUE);
    EXPECT_EQ(U(CancelWaitableTimer)(h), TRUE);
    CloseHandle(h);
}
TEST(timer_reset_replaces_previous_due_time) {
    NEED_TM();
    HANDLE h = U(CreateWaitableTimerA)(NULL, TRUE, NULL);
    CHECK(h != NULL); if (!h) return;
    LARGE_INTEGER d = rel_ms(60000);
    U(SetWaitableTimer)(h, &d, 0, NULL, NULL, FALSE);
    d = rel_ms(50);
    EXPECT_EQ(U(SetWaitableTimer)(h, &d, 0, NULL, NULL, FALSE), TRUE);
    EXPECT_EQ(WaitForSingleObject(h, 3000), WAIT_OBJECT_0);
    CloseHandle(h);
    h = U(CreateWaitableTimerA)(NULL, TRUE, NULL);
    d = rel_ms(100);
    U(SetWaitableTimer)(h, &d, 0, NULL, NULL, FALSE);
    d = rel_ms(60000);
    U(SetWaitableTimer)(h, &d, 0, NULL, NULL, FALSE);
    EXPECT_EQ(WaitForSingleObject(h, 400), WAIT_TIMEOUT);   /* first due time was discarded */
    U(CancelWaitableTimer)(h);
    CloseHandle(h);
}
TEST(timer_periodic_auto_reset_fires_repeatedly) {
    NEED_TM();
    HANDLE h = U(CreateWaitableTimerA)(NULL, FALSE, NULL);
    CHECK(h != NULL); if (!h) return;
    LARGE_INTEGER d = rel_ms(20);
    EXPECT_EQ(U(SetWaitableTimer)(h, &d, 30, NULL, NULL, FALSE), TRUE);
    int n = 0;
    for (int i = 0; i < 5; i++) if (WaitForSingleObject(h, 2000) == WAIT_OBJECT_0) n++;
    EXPECT_EQ(n, 5);
    U(CancelWaitableTimer)(h);
    Sleep(100);
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_TIMEOUT);
    EXPECT_EQ(WaitForSingleObject(h, 150), WAIT_TIMEOUT);   /* periodic stopped by cancel */
    CloseHandle(h);
}
TEST(timer_periodic_manual_reset_stays_signaled) {
    NEED_TM();
    HANDLE h = U(CreateWaitableTimerA)(NULL, TRUE, NULL);
    CHECK(h != NULL); if (!h) return;
    LARGE_INTEGER d = rel_ms(20);
    U(SetWaitableTimer)(h, &d, 25, NULL, NULL, FALSE);
    EXPECT_EQ(WaitForSingleObject(h, 3000), WAIT_OBJECT_0);
    Sleep(120);
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
    U(CancelWaitableTimer)(h);
    CloseHandle(h);
}
TEST(timer_period_accepts_large_values) {
    NEED_TM();
    HANDLE h = U(CreateWaitableTimerA)(NULL, TRUE, NULL);
    CHECK(h != NULL); if (!h) return;
    LARGE_INTEGER d = rel_ms(60000);
    EXPECT_EQ(U(SetWaitableTimer)(h, &d, LONG_MAX, NULL, NULL, FALSE), TRUE);
    EXPECT_EQ(U(SetWaitableTimer)(h, &d, 1, NULL, NULL, FALSE), TRUE);
    U(CancelWaitableTimer)(h);
    CloseHandle(h);
}
TEST(timer_invalid_period_negative) {
    NEED_TM();
    HANDLE h = U(CreateWaitableTimerA)(NULL, TRUE, NULL);
    CHECK(h != NULL); if (!h) return;
    LARGE_INTEGER d = rel_ms(60000);
    LONG per[] = { -1, -1000, LONG_MIN };
    for (int i = 0; i < 3; i++) { SET_ERR(0); EXPECT_EQ(U(SetWaitableTimer)(h, &d, per[i], NULL, NULL, FALSE), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
    CloseHandle(h);
}
TEST(timer_null_due_time) {
    NEED_TM();
    HANDLE h = U(CreateWaitableTimerA)(NULL, TRUE, NULL);
    CHECK(h != NULL); if (!h) return;
    SET_ERR(0); EXPECT_EQ(U(SetWaitableTimer)(h, NULL, 0, NULL, NULL, FALSE), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    REQUIRE(U(SetWaitableTimerEx));
    SET_ERR(0); EXPECT_EQ(U(SetWaitableTimerEx)(h, NULL, 0, NULL, NULL, NULL, 0), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CloseHandle(h);
}
TEST(timer_set_cancel_invalid_handles) {
    NEED_TM();
    LARGE_INTEGER d = rel_ms(1000);
    HANDLE bad[] = { NULL, (HANDLE)(ULONG_PTR)0x7777ABCC, (HANDLE)(LONG_PTR)-2, INVALID_HANDLE_VALUE };
    for (int i = 0; i < 4; i++) {
        SET_ERR(0); EXPECT_EQ(U(SetWaitableTimer)(bad[i], &d, 0, NULL, NULL, FALSE), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
        SET_ERR(0); EXPECT_EQ(U(CancelWaitableTimer)(bad[i]), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
    HANDLE h = U(CreateWaitableTimerA)(NULL, TRUE, NULL); CloseHandle(h);
    SET_ERR(0); EXPECT_EQ(U(SetWaitableTimer)(h, &d, 0, NULL, NULL, FALSE), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(U(CancelWaitableTimer)(h), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(timer_set_cancel_wrong_type) {
    NEED_TM();
    LARGE_INTEGER d = rel_ms(1000);
    HANDLE m = CreateMutexA(NULL, FALSE, NULL), s = CreateSemaphoreA(NULL, 0, 1, NULL);
    HANDLE o[] = { m, s, GetCurrentProcess(), GetCurrentThread() };
    for (int i = 0; i < 4; i++) {
        SET_ERR(0); EXPECT_EQ(U(SetWaitableTimer)(o[i], &d, 0, NULL, NULL, FALSE), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
        SET_ERR(0); EXPECT_EQ(U(CancelWaitableTimer)(o[i]), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
    CloseHandle(m); CloseHandle(s);
}
/* APC completion */
static volatile LONG g_apc_count; static volatile LPVOID g_apc_arg; static volatile DWORD g_apc_lo, g_apc_hi, g_apc_tid;
static VOID CALLBACK apc_cb(LPVOID arg, DWORD lo, DWORD hi) {
    InterlockedIncrement(&g_apc_count); g_apc_arg = arg; g_apc_lo = lo; g_apc_hi = hi; g_apc_tid = GetCurrentThreadId();
}
TEST(timer_apc_delivered_in_alertable_sleep) {
    NEED_TM();
    HANDLE h = U(CreateWaitableTimerA)(NULL, FALSE, NULL);
    CHECK(h != NULL); if (!h) return;
    g_apc_count = 0; g_apc_arg = NULL; g_apc_lo = g_apc_hi = 0;
    LARGE_INTEGER d = rel_ms(40);
    EXPECT_EQ(U(SetWaitableTimer)(h, &d, 0, apc_cb, (LPVOID)0x1234, FALSE), TRUE);
    EXPECT_EQ(SleepEx(4000, TRUE), WAIT_IO_COMPLETION);
    EXPECT_EQ(g_apc_count, 1);
    EXPECT_EQ((ULONG_PTR)g_apc_arg, 0x1234);
    EXPECT_EQ(g_apc_tid, GetCurrentThreadId());
    CHECK(g_apc_lo != 0 || g_apc_hi != 0);   /* timer expiry FILETIME */
    CloseHandle(h);
}
TEST(timer_apc_not_delivered_when_not_alertable) {
    NEED_TM();
    HANDLE h = U(CreateWaitableTimerA)(NULL, TRUE, NULL);
    CHECK(h != NULL); if (!h) return;
    g_apc_count = 0;
    LARGE_INTEGER d = rel_ms(20);
    U(SetWaitableTimer)(h, &d, 0, apc_cb, NULL, FALSE);
    Sleep(250);
    EXPECT_EQ(g_apc_count, 0);
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);      /* timer object signaled meanwhile */
    EXPECT_EQ(g_apc_count, 0);
    EXPECT_EQ(SleepEx(0, TRUE), WAIT_IO_COMPLETION);
    EXPECT_EQ(g_apc_count, 1);
    CloseHandle(h);
}
TEST(timer_apc_delivered_in_waitforsingleobjectex) {
    NEED_TM();
    HANDLE h = U(CreateWaitableTimerA)(NULL, TRUE, NULL), never = CreateEventA(NULL, TRUE, FALSE, NULL);
    CHECK(h && never); if (!h || !never) return;
    g_apc_count = 0;
    LARGE_INTEGER d = rel_ms(30);
    U(SetWaitableTimer)(h, &d, 0, apc_cb, NULL, FALSE);
    EXPECT_EQ(WaitForSingleObjectEx(never, 4000, TRUE), WAIT_IO_COMPLETION);
    EXPECT_EQ(g_apc_count, 1);
    CloseHandle(h); CloseHandle(never);
}
TEST(timer_apc_periodic_repeats) {
    NEED_TM();
    HANDLE h = U(CreateWaitableTimerA)(NULL, FALSE, NULL);
    CHECK(h != NULL); if (!h) return;
    g_apc_count = 0;
    LARGE_INTEGER d = rel_ms(20);
    U(SetWaitableTimer)(h, &d, 20, apc_cb, NULL, FALSE);
    DWORD t0 = GetTickCount();
    while (g_apc_count < 4 && GetTickCount() - t0 < 5000) SleepEx(200, TRUE);
    CHECK(g_apc_count >= 4);
    U(CancelWaitableTimer)(h);
    LONG c = g_apc_count;
    SleepEx(150, TRUE);
    EXPECT_EQ(g_apc_count, c);
    CloseHandle(h);
}
TEST(timer_cancel_discards_pending_apc) {
    NEED_TM();
    HANDLE h = U(CreateWaitableTimerA)(NULL, FALSE, NULL);
    CHECK(h != NULL); if (!h) return;
    g_apc_count = 0;
    LARGE_INTEGER d = rel_ms(100);
    U(SetWaitableTimer)(h, &d, 0, apc_cb, NULL, FALSE);
    EXPECT_EQ(U(CancelWaitableTimer)(h), TRUE);
    SleepEx(400, TRUE);
    EXPECT_EQ(g_apc_count, 0);
    CloseHandle(h);
}
TEST(timer_apc_reset_replaces_routine) {
    NEED_TM();
    HANDLE h = U(CreateWaitableTimerA)(NULL, FALSE, NULL);
    CHECK(h != NULL); if (!h) return;
    g_apc_count = 0;
    LARGE_INTEGER d = rel_ms(60000);
    U(SetWaitableTimer)(h, &d, 0, apc_cb, (LPVOID)1, FALSE);
    d = rel_ms(40);
    U(SetWaitableTimer)(h, &d, 0, apc_cb, (LPVOID)2, FALSE);
    EXPECT_EQ(SleepEx(4000, TRUE), WAIT_IO_COMPLETION);
    EXPECT_EQ(g_apc_count, 1);
    EXPECT_EQ((ULONG_PTR)g_apc_arg, 2);
    CloseHandle(h);
}
TEST(timer_closing_handle_cancels_timer_and_apc) {
    NEED_TM();
    HANDLE h = U(CreateWaitableTimerA)(NULL, FALSE, NULL);
    CHECK(h != NULL); if (!h) return;
    g_apc_count = 0;
    LARGE_INTEGER d = rel_ms(100);
    U(SetWaitableTimer)(h, &d, 0, apc_cb, NULL, FALSE);
    CloseHandle(h);
    SleepEx(400, TRUE);
    EXPECT_EQ(g_apc_count, 0);
}
static DWORD WINAPI tm_apc_thread(LPVOID p) {
    HANDLE h = (HANDLE)p;
    LARGE_INTEGER d = rel_ms(30);
    U(SetWaitableTimer)(h, &d, 0, apc_cb, (LPVOID)0x77, FALSE);
    SleepEx(3000, TRUE);
    return 0;
}
TEST(timer_apc_runs_on_setting_thread) {
    NEED_TM();
    HANDLE h = U(CreateWaitableTimerA)(NULL, FALSE, NULL);
    CHECK(h != NULL); if (!h) return;
    g_apc_count = 0; g_apc_tid = 0;
    so_thr t = so_start(tm_apc_thread, h);
    DWORD tid = GetThreadId(t.h);
    so_join(t, 6000);
    EXPECT_EQ(g_apc_count, 1);
    EXPECT_EQ(g_apc_tid, tid);
    CHECK(g_apc_tid != GetCurrentThreadId());
    CloseHandle(h);
}
/* SetWaitableTimerEx */
TEST(timer_ex_basic_and_tolerable_delay) {
    NEED_TM(); REQUIRE(U(SetWaitableTimerEx));
    HANDLE h = U(CreateWaitableTimerA)(NULL, TRUE, NULL);
    CHECK(h != NULL); if (!h) return;
    ULONG delays[] = { 0, 10, 100, 1000 };
    for (int i = 0; i < 4; i++) {
        LARGE_INTEGER d = rel_ms(30);
        EXPECT_EQ(U(SetWaitableTimerEx)(h, &d, 0, NULL, NULL, NULL, delays[i]), TRUE);
        EXPECT_EQ(WaitForSingleObject(h, 6000), WAIT_OBJECT_0);
    }
    CloseHandle(h);
}
TEST(timer_ex_with_reason_context) {
    NEED_TM(); REQUIRE(U(SetWaitableTimerEx));
    HANDLE h = U(CreateWaitableTimerA)(NULL, TRUE, NULL);
    CHECK(h != NULL); if (!h) return;
    REASON_CONTEXT rc; memset(&rc, 0, sizeof rc);
    rc.Version = POWER_REQUEST_CONTEXT_VERSION;
    rc.Flags = POWER_REQUEST_CONTEXT_SIMPLE_STRING;
    rc.Reason.SimpleReasonString = L"k32t timer";
    LARGE_INTEGER d = rel_ms(30);
    SET_ERR(0);
    BOOL r = U(SetWaitableTimerEx)(h, &d, 0, NULL, NULL, &rc, 0);
    CHECK(r);
    if (r) EXPECT_EQ(WaitForSingleObject(h, 6000), WAIT_OBJECT_0);
    CloseHandle(h);
}
TEST(timer_ex_reason_context_invalid) {
    NEED_TM(); REQUIRE(U(SetWaitableTimerEx));
    HANDLE h = U(CreateWaitableTimerA)(NULL, TRUE, NULL);
    CHECK(h != NULL); if (!h) return;
    REASON_CONTEXT rc; memset(&rc, 0, sizeof rc);
    rc.Version = 0xFFFF; rc.Flags = 0xFFFFFFFF;
    LARGE_INTEGER d = rel_ms(60000);
    SET_ERR(0);
    BOOL r = U(SetWaitableTimerEx)(h, &d, 0, NULL, NULL, &rc, 0);
    DWORD e = GetLastError();
    t_note("SetWaitableTimerEx bad REASON_CONTEXT -> %d err %lu", r, e);
    (void)0;
    U(CancelWaitableTimer)(h);
    CloseHandle(h);
}
TEST(timer_ex_apc_and_period) {
    NEED_TM(); REQUIRE(U(SetWaitableTimerEx));
    HANDLE h = U(CreateWaitableTimerA)(NULL, FALSE, NULL);
    CHECK(h != NULL); if (!h) return;
    g_apc_count = 0;
    LARGE_INTEGER d = rel_ms(20);
    EXPECT_EQ(U(SetWaitableTimerEx)(h, &d, 25, apc_cb, (LPVOID)9, NULL, 0), TRUE);
    DWORD t0 = GetTickCount();
    while (g_apc_count < 3 && GetTickCount() - t0 < 5000) SleepEx(100, TRUE);
    CHECK(g_apc_count >= 3);
    EXPECT_EQ((ULONG_PTR)g_apc_arg, 9);
    U(CancelWaitableTimer)(h);
    CloseHandle(h);
}
TEST(timer_ex_invalid_period_and_handle) {
    NEED_TM(); REQUIRE(U(SetWaitableTimerEx));
    HANDLE h = U(CreateWaitableTimerA)(NULL, TRUE, NULL);
    CHECK(h != NULL); if (!h) return;
    LARGE_INTEGER d = rel_ms(60000);
    SET_ERR(0); EXPECT_EQ(U(SetWaitableTimerEx)(h, &d, -5, NULL, NULL, NULL, 0), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(SetWaitableTimerEx)(NULL, &d, 0, NULL, NULL, NULL, 0), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0); EXPECT_EQ(U(SetWaitableTimerEx)(e, &d, 0, NULL, NULL, NULL, 0), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(e); CloseHandle(h);
}
TEST(timer_resume_flag_accepted) {
    NEED_TM();
    HANDLE h = U(CreateWaitableTimerA)(NULL, TRUE, NULL);
    CHECK(h != NULL); if (!h) return;
    LARGE_INTEGER d = rel_ms(30);
    SET_ERR(0);
    BOOL r = U(SetWaitableTimer)(h, &d, 0, NULL, NULL, TRUE);
    CHECK(r);
    t_note("SetWaitableTimer(fResume=TRUE) -> %d, GetLastError %lu (50 = resume not supported)", r, GetLastError());
    if (r) EXPECT_EQ(WaitForSingleObject(h, 4000), WAIT_OBJECT_0);
    CloseHandle(h);
}
/* Ex creation */
TEST(timer_ex_create_flags) {
    NEED_TM();
    EXPECT_EQ(CREATE_WAITABLE_TIMER_MANUAL_RESET, 1);
    EXPECT_EQ(CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, 2);
    EXPECT_EQ(TIMER_MODIFY_STATE, 2); EXPECT_EQ(TIMER_QUERY_STATE, 1); EXPECT_EQ(TIMER_ALL_ACCESS, 0x1F0003);
    for (int w = 0; w < 2; w++) {
        HANDLE a = tm_createex(w, NULL, NULL, 0, TIMER_ALL_ACCESS), b = tm_createex(w, NULL, NULL, CREATE_WAITABLE_TIMER_MANUAL_RESET, TIMER_ALL_ACCESS);
        CHECK(a && b);
        LARGE_INTEGER d = rel_ms(10);
        if (a) { U(SetWaitableTimer)(a, &d, 0, NULL, NULL, FALSE); EXPECT_EQ(WaitForSingleObject(a, 3000), WAIT_OBJECT_0); EXPECT_EQ(WaitForSingleObject(a, 0), WAIT_TIMEOUT); CloseHandle(a); }
        if (b) { U(SetWaitableTimer)(b, &d, 0, NULL, NULL, FALSE); EXPECT_EQ(WaitForSingleObject(b, 3000), WAIT_OBJECT_0); EXPECT_EQ(WaitForSingleObject(b, 0), WAIT_OBJECT_0); CloseHandle(b); }
    }
}
TEST(timer_ex_high_resolution) {
    NEED_TM();
    for (int m = 0; m < 2; m++) {
        SET_ERR(0);
        HANDLE h = U(CreateWaitableTimerExW)(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION | (m ? CREATE_WAITABLE_TIMER_MANUAL_RESET : 0), TIMER_ALL_ACCESS);
        if (!h) { if (t_is_wine()) { t_skip("high resolution timers unsupported"); return; } CHECKF(0, "create failed err %lu", GetLastError()); return; }
        LARGE_INTEGER d = rel_ms(5);
        EXPECT_EQ(U(SetWaitableTimer)(h, &d, 0, NULL, NULL, FALSE), TRUE);
        EXPECT_EQ(WaitForSingleObject(h, 3000), WAIT_OBJECT_0);
        EXPECT_EQ(WaitForSingleObject(h, 0), m ? WAIT_OBJECT_0 : WAIT_TIMEOUT);
        CloseHandle(h);
    }
}
TEST(timer_ex_invalid_flags) {
    NEED_TM();
    DWORD bad[] = { 4, 8, 0x10, 0x100, 0x80000000, 0xFFFFFFFC, 0xFFFFFFFF };
    for (int w = 0; w < 2; w++)
    for (int i = 0; i < 7; i++) {
        SET_ERR(0); HANDLE h = tm_createex(w, NULL, NULL, bad[i], TIMER_ALL_ACCESS);
        CHECKF(h == NULL, "flags 0x%lx", bad[i]); EXPECT_ERR(ERROR_INVALID_PARAMETER); if (h) CloseHandle(h);
    }
}
TEST(timer_ex_access_modify_only) {
    NEED_TM();
    HANDLE h = U(CreateWaitableTimerExA)(NULL, NULL, CREATE_WAITABLE_TIMER_MANUAL_RESET, TIMER_MODIFY_STATE);
    CHECK(h != NULL); if (!h) return;
    LARGE_INTEGER d = rel_ms(10);
    EXPECT_EQ(U(SetWaitableTimer)(h, &d, 0, NULL, NULL, FALSE), TRUE);
    EXPECT_EQ(U(CancelWaitableTimer)(h), TRUE);
    SET_ERR(0); EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_FAILED); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(h);
}
TEST(timer_ex_access_synchronize_only) {
    NEED_TM();
    HANDLE h = U(CreateWaitableTimerExW)(NULL, NULL, CREATE_WAITABLE_TIMER_MANUAL_RESET, SYNCHRONIZE);
    CHECK(h != NULL); if (!h) return;
    LARGE_INTEGER d = rel_ms(10);
    SET_ERR(0); EXPECT_EQ(U(SetWaitableTimer)(h, &d, 0, NULL, NULL, FALSE), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    SET_ERR(0); EXPECT_EQ(U(CancelWaitableTimer)(h), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_TIMEOUT);
    CloseHandle(h);
}
TEST(timer_named_create_open) {
    NEED_TM();
    for (int w = 0; w < 2; w++) {
        char n[80]; so_name(n, sizeof n, "tmn");
        SET_ERR(0); HANDLE a = tm_create(w, NULL, TRUE, n); CHECK(a != NULL); EXPECT_ERR(0);
        SET_ERR(0); HANDLE b = tm_create(w, NULL, FALSE, n); CHECK(b != NULL); EXPECT_ERR(ERROR_ALREADY_EXISTS);
        SET_ERR(0); HANDLE c = tm_createex(w, NULL, n, 0, TIMER_ALL_ACCESS); CHECK(c != NULL); EXPECT_ERR(ERROR_ALREADY_EXISTS);
        SET_ERR(0); HANDLE d = tm_open(w, TIMER_ALL_ACCESS, FALSE, n); CHECK(d != NULL);
        if (a && b && c && d) {
            LARGE_INTEGER due = rel_ms(20);
            U(SetWaitableTimer)(d, &due, 0, NULL, NULL, FALSE);
            EXPECT_EQ(WaitForSingleObject(a, 3000), WAIT_OBJECT_0);
            EXPECT_EQ(WaitForSingleObject(b, 0), WAIT_OBJECT_0);   /* manual-reset kind from first create persists */
            EXPECT_EQ(WaitForSingleObject(c, 0), WAIT_OBJECT_0);
        }
        if (a) CloseHandle(a); if (b) CloseHandle(b); if (c) CloseHandle(c); if (d) CloseHandle(d);
    }
}
TEST(timer_open_errors) {
    NEED_TM();
    for (int w = 0; w < 2; w++) {
        char n[80]; so_name(n, sizeof n, "tmopen");
        SET_ERR(0); HANDLE h = tm_open(w, SYNCHRONIZE, FALSE, n); CHECK(h == NULL); EXPECT_ERR(ERROR_FILE_NOT_FOUND); if (h) CloseHandle(h);
        SET_ERR(0); h = tm_open(w, SYNCHRONIZE, FALSE, NULL); CHECK(h == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER); if (h) CloseHandle(h);
    }
    char n[80]; so_name(n, sizeof n, "tmconf");
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, n);
    SET_ERR(0); HANDLE h = U(OpenWaitableTimerA)(SYNCHRONIZE, FALSE, n); CHECK(h == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE); if (h) CloseHandle(h);
    SET_ERR(0); h = U(CreateWaitableTimerA)(NULL, TRUE, n); CHECK(h == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE); if (h) CloseHandle(h);
    if (e) CloseHandle(e);
}
TEST(timer_open_access_and_inherit) {
    NEED_TM();
    char n[80]; so_name(n, sizeof n, "tmacc");
    HANDLE c = U(CreateWaitableTimerA)(NULL, TRUE, n);
    CHECK(c != NULL); if (!c) return;
    HANDLE sy = U(OpenWaitableTimerA)(SYNCHRONIZE, TRUE, n), ms = U(OpenWaitableTimerA)(TIMER_MODIFY_STATE, FALSE, n);
    CHECK(sy && ms);
    if (sy && ms) {
        EXPECT_EQ(so_inheritable(sy), 1); EXPECT_EQ(so_inheritable(ms), 0);
        LARGE_INTEGER d = rel_ms(10);
        SET_ERR(0); EXPECT_EQ(U(SetWaitableTimer)(sy, &d, 0, NULL, NULL, FALSE), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
        EXPECT_EQ(U(SetWaitableTimer)(ms, &d, 0, NULL, NULL, FALSE), TRUE);
        EXPECT_EQ(WaitForSingleObject(sy, 3000), WAIT_OBJECT_0);
    }
    if (sy) CloseHandle(sy); if (ms) CloseHandle(ms); CloseHandle(c);
}
TEST(timer_open_denied_by_empty_dacl) {
    NEED_TM();
    char n[80]; so_name(n, sizeof n, "tmdeny");
    so_denysa d; SECURITY_ATTRIBUTES *sa = so_deny(&d, FALSE);
    HANDLE c = U(CreateWaitableTimerA)(sa, TRUE, n);
    CHECK(c != NULL); if (!c) return;
    SET_ERR(0); HANDLE o = U(OpenWaitableTimerA)(SYNCHRONIZE, FALSE, n); CHECK(o == NULL); EXPECT_ERR(ERROR_ACCESS_DENIED);
    if (o) CloseHandle(o);
    CloseHandle(c);
}
TEST(timer_sa_inherit) {
    NEED_TM();
    SECURITY_ATTRIBUTES yes = { sizeof yes, NULL, TRUE };
    for (int w = 0; w < 2; w++) {
        HANDLE a = tm_create(w, &yes, TRUE, NULL), b = tm_create(w, NULL, TRUE, NULL), c = tm_createex(w, &yes, NULL, 0, TIMER_ALL_ACCESS);
        CHECK(a && b && c);
        if (a) { EXPECT_EQ(so_inheritable(a), 1); CloseHandle(a); }
        if (b) { EXPECT_EQ(so_inheritable(b), 0); CloseHandle(b); }
        if (c) { EXPECT_EQ(so_inheritable(c), 1); CloseHandle(c); }
    }
}
TEST(timer_wakes_blocked_thread) {
    NEED_TM();
    static HANDLE h; static volatile LONG state;
    h = U(CreateWaitableTimerA)(NULL, TRUE, NULL); state = 0;
    CHECK(h != NULL); if (!h) return;
    DWORD WINAPI waiter(LPVOID p) { InterlockedExchange(&state, 1); DWORD r = WaitForSingleObject(h, 5000); InterlockedExchange(&state, r == WAIT_OBJECT_0 ? 2 : 3); return 0; }
    so_thr t = so_start(waiter, NULL);
    so_spin_until(&state, 1, 3000);
    LARGE_INTEGER d = rel_ms(80);
    U(SetWaitableTimer)(h, &d, 0, NULL, NULL, FALSE);
    so_join(t, 7000);
    EXPECT_EQ(state, 2);
    CloseHandle(h);
}
TEST(timer_duplicate_handle) {
    NEED_TM();
    HANDLE h = U(CreateWaitableTimerA)(NULL, TRUE, NULL), d = NULL;
    CHECK(h != NULL); if (!h) return;
    CHECK(DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS));
    if (d) {
        LARGE_INTEGER due = rel_ms(20);
        EXPECT_EQ(U(SetWaitableTimer)(d, &due, 0, NULL, NULL, FALSE), TRUE);
        CloseHandle(h);
        EXPECT_EQ(WaitForSingleObject(d, 3000), WAIT_OBJECT_0);
        CloseHandle(d);
    } else CloseHandle(h);
}
TEST(timer_many_timers) {
    NEED_TM();
    HANDLE hs[64]; LARGE_INTEGER d = rel_ms(30);
    for (int i = 0; i < 64; i++) { hs[i] = U(CreateWaitableTimerA)(NULL, TRUE, NULL); if (hs[i]) U(SetWaitableTimer)(hs[i], &d, 0, NULL, NULL, FALSE); }
    int ok = 0;
    for (int i = 0; i < 64; i++) if (hs[i] && WaitForSingleObject(hs[i], 5000) == WAIT_OBJECT_0) ok++;
    EXPECT_EQ(ok, 64);
    for (int i = 0; i < 64; i++) if (hs[i]) CloseHandle(hs[i]);
}
TEST(timer_name_backslash_and_prefix) {
    NEED_TM();
    char n[100]; so_name(n, sizeof n, "x"); strcat(n, "\\y");
    SET_ERR(0); HANDLE h = U(CreateWaitableTimerA)(NULL, TRUE, n);
    CHECK(h == NULL); DWORD e = GetLastError();
    CHECKF(e == ERROR_PATH_NOT_FOUND || e == ERROR_INVALID_NAME || e == ERROR_FILE_NOT_FOUND, "err %lu", e);
    if (h) CloseHandle(h);
    char base[80], l[100]; so_name(base, sizeof base, "tmpfx"); snprintf(l, sizeof l, "Local\\%s", base);
    HANDLE a = U(CreateWaitableTimerA)(NULL, TRUE, l);
    SET_ERR(0); HANDLE b = U(CreateWaitableTimerA)(NULL, TRUE, base);
    CHECK(a && b); EXPECT_ERR(ERROR_ALREADY_EXISTS);
    if (a) CloseHandle(a); if (b) CloseHandle(b);
}
