#include "so.h"

static volatile LONG sm_hits;
static VOID CALLBACK sm_apc(ULONG_PTR p) { InterlockedIncrement(&sm_hits); }
static HANDLE sm_self(void) {
    HANDLE h = NULL;
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &h, 0, FALSE, DUPLICATE_SAME_ACCESS);
    return h;
}

TEST(sleepmisc_sleep_zero_and_short) {
    REQUIRE(U(Sleep));
    U(Sleep)(0);
    DWORD t0 = GetTickCount();
    U(Sleep)(60);
    DWORD dt = GetTickCount() - t0;
    CHECKF(dt >= 40 && dt < 3000, "elapsed %lu", dt);
}
TEST(sleepmisc_sleepex_returns_zero_on_timeout) {
    REQUIRE(U(SleepEx));
    EXPECT_EQ(U(SleepEx)(20, FALSE), 0);
    EXPECT_EQ(U(SleepEx)(20, TRUE), 0);
    EXPECT_EQ(U(SleepEx)(0, TRUE), 0);
}
TEST(sleepmisc_sleepex_nonalertable_defers_apc) {
    REQUIRE(U(SleepEx)); REQUIRE(U(QueueUserAPC));
    HANDLE me = sm_self(); CHECK(me != NULL); if (!me) return;
    sm_hits = 0;
    EXPECT_NE(U(QueueUserAPC)(sm_apc, me, 0), 0);
    EXPECT_EQ(U(SleepEx)(30, FALSE), 0);
    EXPECT_EQ(sm_hits, 0);
    EXPECT_EQ(U(SleepEx)(0, TRUE), WAIT_IO_COMPLETION);       /* delivered now */
    EXPECT_EQ(sm_hits, 1);
    EXPECT_EQ(U(SleepEx)(0, TRUE), 0);                        /* queue drained */
    CloseHandle(me);
}
TEST(sleepmisc_sleepex_alertable_apc_wakes_early) {
    REQUIRE(U(SleepEx)); REQUIRE(U(QueueUserAPC));
    HANDLE me = sm_self(); CHECK(me != NULL); if (!me) return;
    sm_hits = 0;
    EXPECT_NE(U(QueueUserAPC)(sm_apc, me, 0), 0);
    DWORD t0 = GetTickCount();
    EXPECT_EQ(U(SleepEx)(5000, TRUE), WAIT_IO_COMPLETION);
    CHECK(GetTickCount() - t0 < 2000);
    EXPECT_EQ(sm_hits, 1);
    CloseHandle(me);
}
TEST(sleepmisc_sleepex_multiple_apcs_run_in_one_call) {
    REQUIRE(U(SleepEx)); REQUIRE(U(QueueUserAPC));
    HANDLE me = sm_self(); CHECK(me != NULL); if (!me) return;
    sm_hits = 0;
    for (int i = 0; i < 5; i++) EXPECT_NE(U(QueueUserAPC)(sm_apc, me, 0), 0);
    EXPECT_EQ(U(SleepEx)(5000, TRUE), WAIT_IO_COMPLETION);
    EXPECT_EQ(sm_hits, 5);
    CloseHandle(me);
}
static volatile LONG sm_st;
static DWORD WINAPI sm_sleeper(LPVOID p) {
    InterlockedExchange(&sm_st, 1);
    DWORD r = U(SleepEx)(10000, TRUE);
    InterlockedExchange(&sm_st, 2);
    return r;
}
TEST(sleepmisc_apc_from_other_thread_interrupts_sleepex) {
    REQUIRE(U(SleepEx)); REQUIRE(U(QueueUserAPC));
    sm_st = 0; sm_hits = 0;
    DWORD id; HANDLE t = CreateThread(NULL, 0, sm_sleeper, NULL, 0, &id);
    CHECK(t != NULL); if (!t) return;
    so_spin_until(&sm_st, 1, 3000); Sleep(100);
    EXPECT_NE(U(QueueUserAPC)(sm_apc, t, 0), 0);
    EXPECT_EQ(so_spin_until(&sm_st, 2, 3000), 1);
    DWORD code = 0; WaitForSingleObject(t, 3000); GetExitCodeThread(t, &code);
    EXPECT_EQ(code, WAIT_IO_COMPLETION); EXPECT_EQ(sm_hits, 1);
    CloseHandle(t);
}
TEST(sleepmisc_queue_apc_invalid_handles) {
    REQUIRE(U(QueueUserAPC));
    SET_ERR(0); EXPECT_EQ(U(QueueUserAPC)(sm_apc, NULL, 0), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(U(QueueUserAPC)(sm_apc, (HANDLE)(ULONG_PTR)0x7777ABCC, 0), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0); EXPECT_EQ(U(QueueUserAPC)(sm_apc, e, 0), 0);
    DWORD err = GetLastError(); CHECKF(err == ERROR_INVALID_HANDLE, "err %lu", err);
    CloseHandle(e);
}
TEST(sleepmisc_switch_to_thread_returns_bool) {
    REQUIRE(U(SwitchToThread));
    for (int i = 0; i < 20; i++) { BOOL r = U(SwitchToThread)(); CHECK(r == TRUE || r == FALSE); }
}
TEST(sleepmisc_wait_huge_finite_timeout_signaled_returns_immediately) {
    REQUIRE(U(WaitForSingleObject)); REQUIRE(U(WaitForMultipleObjects));
    HANDLE e = CreateEventA(NULL, TRUE, TRUE, NULL);
    DWORD t0 = GetTickCount();
    EXPECT_EQ(U(WaitForSingleObject)(e, 0xFFFFFFFE), WAIT_OBJECT_0);
    EXPECT_EQ(U(WaitForMultipleObjects)(1, &e, TRUE, 0xFFFFFFFE), WAIT_OBJECT_0);
    EXPECT_EQ(U(WaitForSingleObject)(e, INFINITE), WAIT_OBJECT_0);
    CHECK(GetTickCount() - t0 < 2000);
    CloseHandle(e);
}
TEST(sleepmisc_wait_pseudo_handles) {
    REQUIRE(U(WaitForSingleObject));
    /* the current-thread pseudo handle is a thread object that is not finished: zero timeout times out */
    EXPECT_EQ(U(WaitForSingleObject)(GetCurrentThread(), 0), WAIT_TIMEOUT);
    EXPECT_EQ(U(WaitForSingleObject)(GetCurrentProcess(), 0), WAIT_TIMEOUT);
}
TEST(sleepmisc_wait_on_file_handle_signals_when_idle) {
    REQUIRE(U(WaitForSingleObject));
    char p[MAX_PATH]; snprintf(p, sizeof p, "%s\\sm_file.tmp", t_tmpdir());
    HANDLE f = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    CHECK(f != INVALID_HANDLE_VALUE); if (f == INVALID_HANDLE_VALUE) return;
    /* synchronous file objects are signaled for waits only transiently; the wait must not fail with invalid handle */
    SET_ERR(0);
    DWORD r = U(WaitForSingleObject)(f, 0);
    CHECKF(r == WAIT_OBJECT_0 || r == WAIT_TIMEOUT, "r=0x%lx", r);
    CloseHandle(f);
}
TEST(sleepmisc_manual_event_many_waiters_bounded) {
    REQUIRE(U(WaitForSingleObject)); REQUIRE(U(SetEvent));
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE th[16]; DWORD id;
    static volatile LONG done; done = 0;
    DWORD WINAPI w(LPVOID p) { DWORD r = U(WaitForSingleObject)((HANDLE)p, 5000); if (r == WAIT_OBJECT_0) InterlockedIncrement(&done); return r; }
    for (int i = 0; i < 16; i++) th[i] = CreateThread(NULL, 0, w, e, 0, &id);
    Sleep(150);
    EXPECT_EQ(done, 0);
    U(SetEvent)(e);
    for (int i = 0; i < 16; i++) if (th[i]) { WaitForSingleObject(th[i], 5000); CloseHandle(th[i]); }
    EXPECT_EQ(done, 16);
    CloseHandle(e);
}
TEST(sleepmisc_auto_event_many_waiters_one_at_a_time) {
    REQUIRE(U(WaitForSingleObject)); REQUIRE(U(SetEvent));
    HANDLE e = CreateEventA(NULL, FALSE, FALSE, NULL);
    HANDLE th[8]; DWORD id;
    static volatile LONG done; done = 0;
    DWORD WINAPI w(LPVOID p) { DWORD r = U(WaitForSingleObject)((HANDLE)p, 8000); if (r == WAIT_OBJECT_0) InterlockedIncrement(&done); return r; }
    for (int i = 0; i < 8; i++) th[i] = CreateThread(NULL, 0, w, e, 0, &id);
    Sleep(150);
    for (int i = 1; i <= 8; i++) {
        U(SetEvent)(e);
        so_spin_until(&done, i, 3000);
        EXPECT_EQ(done, i);
    }
    for (int i = 0; i < 8; i++) if (th[i]) { WaitForSingleObject(th[i], 5000); CloseHandle(th[i]); }
    CloseHandle(e);
}
