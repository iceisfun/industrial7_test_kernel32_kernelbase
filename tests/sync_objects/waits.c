#include "so.h"

#define NEED_W() do { REQUIRE(U(WaitForSingleObject)); REQUIRE(U(WaitForSingleObjectEx)); REQUIRE(U(WaitForMultipleObjects)); REQUIRE(U(WaitForMultipleObjectsEx)); } while (0)
#define WS U(WaitForSingleObject)
#define WSE U(WaitForSingleObjectEx)
#define WM U(WaitForMultipleObjects)
#define WME U(WaitForMultipleObjectsEx)

TEST(wait_single_signaled_and_timeout) {
    NEED_W();
    HANDLE m = CreateEventA(NULL, TRUE, TRUE, NULL), u = CreateEventA(NULL, TRUE, FALSE, NULL);
    EXPECT_EQ(WS(m, 0), WAIT_OBJECT_0);
    EXPECT_EQ(WS(m, 1), WAIT_OBJECT_0);
    EXPECT_EQ(WS(m, INFINITE), WAIT_OBJECT_0);
    EXPECT_EQ(WS(m, 0xFFFFFFFE), WAIT_OBJECT_0);
    EXPECT_EQ(WS(u, 0), WAIT_TIMEOUT);
    EXPECT_EQ(WS(u, 1), WAIT_TIMEOUT);
    DWORD t0 = GetTickCount();
    EXPECT_EQ(WS(u, 100), WAIT_TIMEOUT);
    DWORD dt = GetTickCount() - t0;
    CHECKF(dt >= 80 && dt < 3000, "elapsed %lu", dt);
    CloseHandle(m); CloseHandle(u);
}
TEST(wait_single_auto_reset_consumed) {
    NEED_W();
    HANDLE a = CreateEventA(NULL, FALSE, TRUE, NULL);
    EXPECT_EQ(WS(a, 0), WAIT_OBJECT_0);
    EXPECT_EQ(WS(a, 0), WAIT_TIMEOUT);
    EXPECT_EQ(WSE(a, 0, FALSE), WAIT_TIMEOUT);
    SetEvent(a);
    EXPECT_EQ(WSE(a, 0, TRUE), WAIT_OBJECT_0);
    EXPECT_EQ(WSE(a, 0, TRUE), WAIT_TIMEOUT);
    CloseHandle(a);
}
TEST(wait_single_invalid_handles) {
    NEED_W();
    HANDLE bad[] = { NULL, (HANDLE)(ULONG_PTR)0x7777ABD0, (HANDLE)(ULONG_PTR)0x123454, (HANDLE)(ULONG_PTR)0x7777ABD4 };
    DWORD to[] = { 0, 10, INFINITE };
    for (int i = 0; i < 4; i++)
    for (int j = 0; j < 3; j++) {
        SET_ERR(0); EXPECT_EQ(WS(bad[i], to[j]), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE);
        SET_ERR(0); EXPECT_EQ(WSE(bad[i], to[j], FALSE), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE);
        SET_ERR(0); EXPECT_EQ(WSE(bad[i], to[j], TRUE), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
}
TEST(wait_single_closed_handle) {
    NEED_W();
    HANDLE h = CreateEventA(NULL, TRUE, TRUE, NULL);
    CloseHandle(h);
    SET_ERR(0); EXPECT_EQ(WS(h, 0), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(WS(h, INFINITE), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(wait_single_current_process_pseudo_handle_times_out) {
    NEED_W();
    /* -1 is a valid pseudo handle for the (still running) current process */
    SET_ERR(0);
    EXPECT_EQ(WS(GetCurrentProcess(), 0), WAIT_TIMEOUT);
    EXPECT_EQ(WS(GetCurrentProcess(), 20), WAIT_TIMEOUT);
    EXPECT_EQ(WS(GetCurrentThread(), 20), WAIT_TIMEOUT);
}
static DWORD WINAPI w_exit_now(LPVOID p) { return 42; }
TEST(wait_single_thread_handle) {
    NEED_W();
    so_thr t = so_start(w_exit_now, NULL);
    EXPECT_EQ(WS(t.h, 5000), WAIT_OBJECT_0);
    EXPECT_EQ(WS(t.h, 0), WAIT_OBJECT_0);
    DWORD c = 0; GetExitCodeThread(t.h, &c); EXPECT_EQ(c, 42);
    CloseHandle(t.h);
}
static DWORD WINAPI w_sleep_300(LPVOID p) { Sleep(300); return 0; }
TEST(wait_single_thread_running_then_done) {
    NEED_W();
    so_thr t = so_start(w_sleep_300, NULL);
    EXPECT_EQ(WS(t.h, 0), WAIT_TIMEOUT);
    EXPECT_EQ(WS(t.h, 20), WAIT_TIMEOUT);
    EXPECT_EQ(WS(t.h, 5000), WAIT_OBJECT_0);
    CloseHandle(t.h);
}
TEST(wait_single_infinite_released_by_other_thread) {
    NEED_W();
    HANDLE e = CreateEventA(NULL, FALSE, FALSE, NULL);
    DWORD WINAPI setter(LPVOID p) { Sleep(100); SetEvent((HANDLE)p); return 0; }
    so_thr t = so_start(setter, e);
    EXPECT_EQ(WS(e, INFINITE), WAIT_OBJECT_0);
    so_join(t, 3000);
    CloseHandle(e);
}
static VOID CALLBACK noop_apc(ULONG_PTR p) { InterlockedIncrement((volatile LONG *)p); }
TEST(wait_single_ex_alertable_apc_returns_io_completion) {
    NEED_W();
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    volatile LONG hit = 0;
    CHECK(QueueUserAPC(noop_apc, GetCurrentThread(), (ULONG_PTR)&hit));
    DWORD t0 = GetTickCount();
    EXPECT_EQ(WSE(e, 5000, TRUE), WAIT_IO_COMPLETION);
    CHECK(GetTickCount() - t0 < 2000);
    EXPECT_EQ(hit, 1);
    EXPECT_EQ(WSE(e, 10, TRUE), WAIT_TIMEOUT);
    CloseHandle(e);
}
TEST(wait_single_ex_nonalertable_does_not_run_apc) {
    NEED_W();
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    volatile LONG hit = 0;
    QueueUserAPC(noop_apc, GetCurrentThread(), (ULONG_PTR)&hit);
    EXPECT_EQ(WSE(e, 50, FALSE), WAIT_TIMEOUT);
    EXPECT_EQ(WS(e, 50), WAIT_TIMEOUT);
    EXPECT_EQ(hit, 0);
    EXPECT_EQ(WSE(e, 0, TRUE), WAIT_IO_COMPLETION);   /* delivered at the first alertable point */
    EXPECT_EQ(hit, 1);
    CloseHandle(e);
}
TEST(wait_single_ex_zero_timeout_alertable_with_pending_apc) {
    NEED_W();
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    volatile LONG hit = 0;
    QueueUserAPC(noop_apc, GetCurrentThread(), (ULONG_PTR)&hit);
    EXPECT_EQ(WSE(e, 0, TRUE), WAIT_IO_COMPLETION);
    EXPECT_EQ(hit, 1);
    CloseHandle(e);
}
TEST(wait_single_ex_multiple_apcs_all_run) {
    NEED_W();
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    volatile LONG hit = 0;
    for (int i = 0; i < 5; i++) QueueUserAPC(noop_apc, GetCurrentThread(), (ULONG_PTR)&hit);
    EXPECT_EQ(WSE(e, 1000, TRUE), WAIT_IO_COMPLETION);
    EXPECT_EQ(hit, 5);   /* one alertable wait drains the whole APC queue */
    CloseHandle(e);
}
TEST(wait_single_ex_signaled_object_vs_pending_apc) {
    NEED_W();
    HANDLE e = CreateEventA(NULL, TRUE, TRUE, NULL);
    volatile LONG hit = 0;
    QueueUserAPC(noop_apc, GetCurrentThread(), (ULONG_PTR)&hit);
    DWORD r = WSE(e, 1000, TRUE);
    t_note("signaled object + pending APC -> 0x%lx, apc ran %ld", r, (long)hit);
    CHECK(r == WAIT_OBJECT_0 || r == WAIT_IO_COMPLETION);
    SleepEx(0, TRUE);
    EXPECT_EQ(hit, 1);
    CloseHandle(e);
}
TEST(wait_single_ex_apc_from_other_thread) {
    NEED_W();
    static HANDLE main_thread; static volatile LONG hit;
    hit = 0;
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &main_thread, 0, FALSE, DUPLICATE_SAME_ACCESS);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    DWORD WINAPI q(LPVOID p) { Sleep(100); QueueUserAPC(noop_apc, main_thread, (ULONG_PTR)&hit); return 0; }
    so_thr t = so_start(q, NULL);
    EXPECT_EQ(WSE(e, 5000, TRUE), WAIT_IO_COMPLETION);
    EXPECT_EQ(hit, 1);
    so_join(t, 3000);
    CloseHandle(main_thread); CloseHandle(e);
}
/* ---- multiple ---- */
TEST(waitmulti_count_zero_and_too_large) {
    NEED_W();
    HANDLE h[65]; for (int i = 0; i < 65; i++) h[i] = CreateEventA(NULL, TRUE, TRUE, NULL);
    SET_ERR(0); EXPECT_EQ(WM(0, h, FALSE, 0), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(WM(0, h, TRUE, 0), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(WM(0, NULL, FALSE, 0), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(WM(65, h, FALSE, 0), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(WM(65, h, TRUE, 0), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(WM(0xFFFFFFFF, h, FALSE, 0), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(WM(128, h, FALSE, 0), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(WME(0, h, FALSE, 0, TRUE), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(WME(65, h, TRUE, 0, TRUE), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    for (int i = 0; i < 65; i++) CloseHandle(h[i]);
}
TEST(waitmulti_exactly_64_any) {
    NEED_W();
    HANDLE h[64];
    for (int i = 0; i < 64; i++) h[i] = CreateEventA(NULL, TRUE, FALSE, NULL);
    EXPECT_EQ(WM(64, h, FALSE, 0), WAIT_TIMEOUT);
    SetEvent(h[63]);
    EXPECT_EQ(WM(64, h, FALSE, 0), WAIT_OBJECT_0 + 63);
    SetEvent(h[17]);
    EXPECT_EQ(WM(64, h, FALSE, 0), WAIT_OBJECT_0 + 17);   /* lowest index wins */
    for (int i = 0; i < 64; i++) SetEvent(h[i]);
    EXPECT_EQ(WM(64, h, TRUE, 0), WAIT_OBJECT_0);
    ResetEvent(h[40]);
    EXPECT_EQ(WM(64, h, TRUE, 0), WAIT_TIMEOUT);
    for (int i = 0; i < 64; i++) CloseHandle(h[i]);
}
TEST(waitmulti_any_index_results) {
    NEED_W();
    HANDLE h[5];
    for (int i = 0; i < 5; i++) h[i] = CreateEventA(NULL, TRUE, FALSE, NULL);
    for (int k = 0; k < 5; k++) {
        SetEvent(h[k]);
        EXPECT_EQ(WM(5, h, FALSE, 0), WAIT_OBJECT_0 + k);
        EXPECT_EQ(WME(5, h, FALSE, 0, FALSE), WAIT_OBJECT_0 + k);
        ResetEvent(h[k]);
    }
    EXPECT_EQ(WM(5, h, FALSE, 10), WAIT_TIMEOUT);
    for (int i = 0; i < 5; i++) CloseHandle(h[i]);
}
TEST(waitmulti_count_one_behaves_like_single) {
    NEED_W();
    HANDLE a = CreateEventA(NULL, FALSE, TRUE, NULL);
    EXPECT_EQ(WM(1, &a, FALSE, 0), WAIT_OBJECT_0);
    EXPECT_EQ(WM(1, &a, FALSE, 0), WAIT_TIMEOUT);
    SetEvent(a);
    EXPECT_EQ(WM(1, &a, TRUE, 0), WAIT_OBJECT_0);
    EXPECT_EQ(WM(1, &a, TRUE, 0), WAIT_TIMEOUT);
    CloseHandle(a);
}
TEST(waitmulti_any_consumes_only_returned_auto_reset) {
    NEED_W();
    HANDLE h[3];
    for (int i = 0; i < 3; i++) h[i] = CreateEventA(NULL, FALSE, TRUE, NULL);
    EXPECT_EQ(WM(3, h, FALSE, 0), WAIT_OBJECT_0);
    EXPECT_EQ(WM(3, h, FALSE, 0), WAIT_OBJECT_0 + 1);
    EXPECT_EQ(WM(3, h, FALSE, 0), WAIT_OBJECT_0 + 2);
    EXPECT_EQ(WM(3, h, FALSE, 0), WAIT_TIMEOUT);
    for (int i = 0; i < 3; i++) CloseHandle(h[i]);
}
TEST(waitmulti_all_consumes_all_auto_reset) {
    NEED_W();
    HANDLE h[3];
    for (int i = 0; i < 3; i++) h[i] = CreateEventA(NULL, FALSE, TRUE, NULL);
    EXPECT_EQ(WM(3, h, TRUE, 0), WAIT_OBJECT_0);
    for (int i = 0; i < 3; i++) EXPECT_EQ(WS(h[i], 0), WAIT_TIMEOUT);
    for (int i = 0; i < 3; i++) CloseHandle(h[i]);
}
TEST(waitmulti_all_partial_consumes_nothing) {
    NEED_W();
    HANDLE h[3];
    h[0] = CreateEventA(NULL, FALSE, TRUE, NULL); h[1] = CreateEventA(NULL, FALSE, TRUE, NULL); h[2] = CreateEventA(NULL, FALSE, FALSE, NULL);
    EXPECT_EQ(WM(3, h, TRUE, 0), WAIT_TIMEOUT);
    EXPECT_EQ(WM(3, h, TRUE, 20), WAIT_TIMEOUT);
    EXPECT_EQ(WS(h[0], 0), WAIT_OBJECT_0);   /* still signaled */
    EXPECT_EQ(WS(h[1], 0), WAIT_OBJECT_0);
    for (int i = 0; i < 3; i++) CloseHandle(h[i]);
}
TEST(waitmulti_all_becomes_satisfied_while_waiting) {
    NEED_W();
    static HANDLE h[3];
    for (int i = 0; i < 3; i++) h[i] = CreateEventA(NULL, FALSE, FALSE, NULL);
    DWORD WINAPI setter(LPVOID p) { for (int i = 0; i < 3; i++) { Sleep(60); SetEvent(h[i]); } return 0; }
    so_thr t = so_start(setter, NULL);
    EXPECT_EQ(WM(3, h, TRUE, 5000), WAIT_OBJECT_0);
    so_join(t, 3000);
    for (int i = 0; i < 3; i++) { EXPECT_EQ(WS(h[i], 0), WAIT_TIMEOUT); CloseHandle(h[i]); }
}
TEST(waitmulti_any_blocking_returns_index) {
    NEED_W();
    static HANDLE h[4];
    for (int i = 0; i < 4; i++) h[i] = CreateEventA(NULL, TRUE, FALSE, NULL);
    DWORD WINAPI setter(LPVOID p) { Sleep(100); SetEvent(h[2]); return 0; }
    so_thr t = so_start(setter, NULL);
    EXPECT_EQ(WM(4, h, FALSE, 5000), WAIT_OBJECT_0 + 2);
    so_join(t, 3000);
    for (int i = 0; i < 4; i++) CloseHandle(h[i]);
}
TEST(waitmulti_invalid_handle_in_array) {
    NEED_W();
    HANDLE ok = CreateEventA(NULL, TRUE, TRUE, NULL);
    HANDLE a[3] = { ok, NULL, ok }, b[2] = { ok, (HANDLE)(ULONG_PTR)0x7777ABD4 }, c[2] = { (HANDLE)(ULONG_PTR)0x123454, ok };
    for (int k = 0; k < 2; k++) {
        SET_ERR(0); EXPECT_EQ(WM(3, a, k, 0), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE);
        SET_ERR(0); EXPECT_EQ(WM(2, b, k, 0), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE);
        SET_ERR(0); EXPECT_EQ(WM(2, c, k, 0), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE);
        SET_ERR(0); EXPECT_EQ(WME(2, c, k, 100, TRUE), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
    CloseHandle(ok);
}
TEST(waitmulti_closed_handle_in_array) {
    NEED_W();
    HANDLE a = CreateEventA(NULL, TRUE, TRUE, NULL), b = CreateEventA(NULL, TRUE, TRUE, NULL);
    CloseHandle(b);
    HANDLE arr[2] = { a, b };
    SET_ERR(0); EXPECT_EQ(WM(2, arr, FALSE, 0), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(WM(2, arr, TRUE, 0), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(a);
}
TEST(waitmulti_wait_all_duplicate_handle_values) {
    NEED_W();
    HANDLE a = CreateEventA(NULL, TRUE, TRUE, NULL);
    HANDLE arr[2] = { a, a };
    /* docs: the array may not contain multiple copies of the same handle; kernel behaviour not verified from decomp -> accept both */
    SET_ERR(0); DWORD r2 = WM(2, arr, TRUE, 0);
    t_note("wait-all dup handle value -> 0x%lx err %lu", r2, r2 == WAIT_FAILED ? GetLastError() : 0);
    CHECK(r2 == WAIT_FAILED || r2 == WAIT_OBJECT_0);
    if (r2 == WAIT_FAILED) EXPECT_ERR(ERROR_INVALID_PARAMETER);
    HANDLE arr3[3] = { a, CreateEventA(NULL, TRUE, TRUE, NULL), a };
    SET_ERR(0); r2 = WM(3, arr3, TRUE, 0);
    CHECK(r2 == WAIT_FAILED || r2 == WAIT_OBJECT_0);
    SET_ERR(0); r2 = WME(3, arr3, TRUE, 0, FALSE);
    CHECK(r2 == WAIT_FAILED || r2 == WAIT_OBJECT_0);
    /* wait-any with duplicates is fine */
    EXPECT_EQ(WM(2, arr, FALSE, 0), WAIT_OBJECT_0);
    EXPECT_EQ(WM(3, arr3, FALSE, 0), WAIT_OBJECT_0);
    CloseHandle(arr3[1]); CloseHandle(a);
}
TEST(waitmulti_wait_all_two_handles_same_object) {
    NEED_W();
    HANDLE a = CreateEventA(NULL, TRUE, TRUE, NULL), d = NULL;
    DuplicateHandle(GetCurrentProcess(), a, GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS);
    HANDLE arr[2] = { a, d };
    SET_ERR(0);
    DWORD r = WM(2, arr, TRUE, 0);
    t_note("wait-all on two handles to one object -> 0x%lx err %lu", r, r == WAIT_FAILED ? GetLastError() : 0);
    CHECK(r == WAIT_OBJECT_0 || r == WAIT_FAILED);
    EXPECT_EQ(WM(2, arr, FALSE, 0), WAIT_OBJECT_0);
    CloseHandle(a); if (d) CloseHandle(d);
}
TEST(waitmulti_mixed_object_types_all) {
    NEED_W();
    HANDLE ev = CreateEventA(NULL, FALSE, TRUE, NULL), mx = CreateMutexA(NULL, FALSE, NULL), se = CreateSemaphoreA(NULL, 2, 2, NULL), tm = CreateWaitableTimerA(NULL, TRUE, NULL);
    LARGE_INTEGER d; d.QuadPart = -10000 * 20; SetWaitableTimer(tm, &d, 0, NULL, NULL, FALSE);
    Sleep(150);
    so_thr th = so_start(w_exit_now, NULL);
    WaitForSingleObject(th.h, 3000);
    HANDLE arr[5] = { ev, mx, se, tm, th.h };
    EXPECT_EQ(WM(5, arr, TRUE, 1000), WAIT_OBJECT_0);
    /* event consumed, mutex owned (recursion 1), semaphore decremented */
    EXPECT_EQ(WS(ev, 0), WAIT_TIMEOUT);
    EXPECT_EQ(ReleaseMutex(mx), TRUE);
    SET_ERR(0); EXPECT_EQ(ReleaseMutex(mx), FALSE); EXPECT_ERR(ERROR_NOT_OWNER);
    LONG prev = -1; ReleaseSemaphore(se, 1, &prev); EXPECT_EQ(prev, 1);
    CloseHandle(ev); CloseHandle(mx); CloseHandle(se); CloseHandle(tm); CloseHandle(th.h);
}
TEST(waitmulti_abandoned_mutex_any_index) {
    NEED_W();
    static HANDLE m[3];
    m[0] = CreateEventA(NULL, TRUE, FALSE, NULL); m[1] = CreateMutexA(NULL, FALSE, NULL); m[2] = CreateMutexA(NULL, FALSE, NULL);
    DWORD WINAPI own(LPVOID p) { WaitForSingleObject(m[1], 1000); return 0; }
    so_join(so_start(own, NULL), 5000);
    EXPECT_EQ(WM(3, m, FALSE, 1000), WAIT_ABANDONED_0 + 1);
    ReleaseMutex(m[1]);
    EXPECT_EQ(WM(3, m, FALSE, 1000), WAIT_OBJECT_0 + 1);   /* flag consumed: plain success now */
    ReleaseMutex(m[1]);
    for (int i = 0; i < 3; i++) CloseHandle(m[i]);
}
TEST(waitmulti_abandoned_mutex_wait_all) {
    NEED_W();
    static HANDLE m[3];
    m[0] = CreateEventA(NULL, TRUE, TRUE, NULL); m[1] = CreateMutexA(NULL, FALSE, NULL); m[2] = CreateMutexA(NULL, FALSE, NULL);
    DWORD WINAPI own(LPVOID p) { WaitForSingleObject(m[2], 1000); return 0; }
    so_join(so_start(own, NULL), 5000);
    DWORD r = WM(3, m, TRUE, 1000);
    CHECKF(r >= WAIT_ABANDONED_0 && r < WAIT_ABANDONED_0 + 3, "r=0x%lx", r);
    t_note("wait-all abandoned index -> 0x%lx", r);
    ReleaseMutex(m[1]); ReleaseMutex(m[2]);
    for (int i = 0; i < 3; i++) CloseHandle(m[i]);
}
TEST(waitmulti_signaled_wins_over_earlier_unsignaled) {
    NEED_W();
    HANDLE h[3] = { CreateEventA(NULL, TRUE, FALSE, NULL), CreateEventA(NULL, TRUE, FALSE, NULL), CreateEventA(NULL, TRUE, TRUE, NULL) };
    EXPECT_EQ(WME(3, h, FALSE, 0, FALSE), WAIT_OBJECT_0 + 2);
    for (int i = 0; i < 3; i++) CloseHandle(h[i]);
}
TEST(waitmulti_ex_alertable_apc) {
    NEED_W();
    HANDLE h[2] = { CreateEventA(NULL, TRUE, FALSE, NULL), CreateEventA(NULL, TRUE, FALSE, NULL) };
    volatile LONG hit = 0;
    QueueUserAPC(noop_apc, GetCurrentThread(), (ULONG_PTR)&hit);
    EXPECT_EQ(WME(2, h, FALSE, 5000, TRUE), WAIT_IO_COMPLETION);
    EXPECT_EQ(hit, 1);
    QueueUserAPC(noop_apc, GetCurrentThread(), (ULONG_PTR)&hit);
    EXPECT_EQ(WME(2, h, TRUE, 5000, TRUE), WAIT_IO_COMPLETION);
    EXPECT_EQ(hit, 2);
    QueueUserAPC(noop_apc, GetCurrentThread(), (ULONG_PTR)&hit);
    EXPECT_EQ(WME(2, h, TRUE, 30, FALSE), WAIT_TIMEOUT);
    EXPECT_EQ(hit, 2);
    SleepEx(0, TRUE);
    for (int i = 0; i < 2; i++) CloseHandle(h[i]);
}
TEST(waitmulti_timeouts) {
    NEED_W();
    HANDLE h[2] = { CreateEventA(NULL, TRUE, FALSE, NULL), CreateEventA(NULL, TRUE, FALSE, NULL) };
    EXPECT_EQ(WM(2, h, FALSE, 0), WAIT_TIMEOUT);
    EXPECT_EQ(WM(2, h, TRUE, 0), WAIT_TIMEOUT);
    DWORD t0 = GetTickCount();
    EXPECT_EQ(WM(2, h, FALSE, 120), WAIT_TIMEOUT);
    EXPECT_EQ(WM(2, h, TRUE, 120), WAIT_TIMEOUT);
    DWORD dt = GetTickCount() - t0;
    CHECKF(dt >= 200 && dt < 4000, "elapsed %lu", dt);
    for (int i = 0; i < 2; i++) CloseHandle(h[i]);
}
TEST(waitmulti_does_not_modify_handle_array) {
    NEED_W();
    HANDLE h[3] = { CreateEventA(NULL, TRUE, TRUE, NULL), CreateEventA(NULL, TRUE, TRUE, NULL), CreateEventA(NULL, TRUE, TRUE, NULL) };
    HANDLE copy[3]; memcpy(copy, h, sizeof h);
    WM(3, h, TRUE, 0); WM(3, h, FALSE, 0);
    CHECK(!memcmp(copy, h, sizeof h));
    for (int i = 0; i < 3; i++) CloseHandle(h[i]);
}
TEST(waitmulti_threads_as_handles) {
    NEED_W();
    so_thr t[3]; HANDLE h[3];
    for (int i = 0; i < 3; i++) { t[i] = so_start(w_sleep_300, NULL); h[i] = t[i].h; }
    EXPECT_EQ(WM(3, h, TRUE, 0), WAIT_TIMEOUT);
    EXPECT_EQ(WM(3, h, TRUE, 5000), WAIT_OBJECT_0);
    EXPECT_EQ(WM(3, h, FALSE, 0), WAIT_OBJECT_0);
    for (int i = 0; i < 3; i++) CloseHandle(h[i]);
}
/* ---- SignalObjectAndWait ---- */
#define NEED_SAW() do { REQUIRE(U(SignalObjectAndWait)); } while (0)
#define SAW U(SignalObjectAndWait)
TEST(saw_event_to_event) {
    NEED_SAW();
    HANDLE a = CreateEventA(NULL, TRUE, FALSE, NULL), b = CreateEventA(NULL, TRUE, TRUE, NULL);
    EXPECT_EQ(SAW(a, b, 1000, FALSE), WAIT_OBJECT_0);
    EXPECT_EQ(WS(a, 0), WAIT_OBJECT_0);   /* a was signaled */
    CloseHandle(a); CloseHandle(b);
}
TEST(saw_timeout_still_signals) {
    NEED_SAW();
    HANDLE a = CreateEventA(NULL, TRUE, FALSE, NULL), b = CreateEventA(NULL, TRUE, FALSE, NULL);
    EXPECT_EQ(SAW(a, b, 50, FALSE), WAIT_TIMEOUT);
    EXPECT_EQ(WS(a, 0), WAIT_OBJECT_0);
    EXPECT_EQ(SAW(a, b, 0, FALSE), WAIT_TIMEOUT);
    CloseHandle(a); CloseHandle(b);
}
TEST(saw_same_object_event) {
    NEED_SAW();
    HANDLE a = CreateEventA(NULL, FALSE, FALSE, NULL);
    EXPECT_EQ(SAW(a, a, 1000, FALSE), WAIT_OBJECT_0);   /* signal then wait consumes the signal atomically */
    EXPECT_EQ(WS(a, 0), WAIT_TIMEOUT);
    CloseHandle(a);
}
TEST(saw_mutex_release_and_wait) {
    NEED_SAW();
    HANDLE m = CreateMutexA(NULL, TRUE, NULL), e = CreateEventA(NULL, TRUE, TRUE, NULL);
    EXPECT_EQ(SAW(m, e, 1000, FALSE), WAIT_OBJECT_0);
    SET_ERR(0); EXPECT_EQ(ReleaseMutex(m), FALSE); EXPECT_ERR(ERROR_NOT_OWNER);   /* released by SAW */
    CloseHandle(m); CloseHandle(e);
}
TEST(saw_mutex_not_owned) {
    NEED_SAW();
    HANDLE m = CreateMutexA(NULL, FALSE, NULL), e = CreateEventA(NULL, TRUE, TRUE, NULL);
    SET_ERR(0); EXPECT_EQ(SAW(m, e, 1000, FALSE), WAIT_FAILED); EXPECT_ERR(ERROR_NOT_OWNER);
    CloseHandle(m); CloseHandle(e);
}
TEST(saw_semaphore_release_and_wait) {
    NEED_SAW();
    HANDLE s = CreateSemaphoreA(NULL, 0, 3, NULL), e = CreateEventA(NULL, TRUE, TRUE, NULL);
    EXPECT_EQ(SAW(s, e, 1000, FALSE), WAIT_OBJECT_0);
    EXPECT_EQ(WS(s, 0), WAIT_OBJECT_0);   /* released one */
    EXPECT_EQ(WS(s, 0), WAIT_TIMEOUT);
    CloseHandle(s); CloseHandle(e);
}
TEST(saw_semaphore_at_max) {
    NEED_SAW();
    HANDLE s = CreateSemaphoreA(NULL, 1, 1, NULL), e = CreateEventA(NULL, TRUE, TRUE, NULL);
    SET_ERR(0); EXPECT_EQ(SAW(s, e, 1000, FALSE), WAIT_FAILED); EXPECT_ERR(ERROR_TOO_MANY_POSTS);
    CloseHandle(s); CloseHandle(e);
}
TEST(saw_invalid_handles) {
    NEED_SAW();
    HANDLE e = CreateEventA(NULL, TRUE, TRUE, NULL);
    SET_ERR(0); EXPECT_EQ(SAW(NULL, e, 0, FALSE), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(SAW(e, NULL, 0, FALSE), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(SAW(NULL, NULL, 0, FALSE), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(SAW((HANDLE)(ULONG_PTR)0x7777ABD8, e, 0, FALSE), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(SAW(e, (HANDLE)(ULONG_PTR)0x7777ABD8, 0, FALSE), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE x = CreateEventA(NULL, TRUE, TRUE, NULL); CloseHandle(x);
    SET_ERR(0); EXPECT_EQ(SAW(x, e, 0, TRUE), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(e);
}
TEST(saw_wrong_type_to_signal) {
    NEED_SAW();
    HANDLE e = CreateEventA(NULL, TRUE, TRUE, NULL), t = CreateWaitableTimerA(NULL, TRUE, NULL);
    HANDLE bad[] = { t, GetCurrentProcess(), GetCurrentThread() };
    for (int i = 0; i < 3; i++) { SET_ERR(0); EXPECT_EQ(SAW(bad[i], e, 0, FALSE), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE); }
    CloseHandle(e); CloseHandle(t);
}
TEST(saw_wait_on_unsignalable_types) {
    NEED_SAW();
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    /* waiting on a process/thread is valid; the process never signals */
    EXPECT_EQ(SAW(e, GetCurrentProcess(), 20, FALSE), WAIT_TIMEOUT);
    EXPECT_EQ(WS(e, 0), WAIT_OBJECT_0);
    CloseHandle(e);
}
TEST(saw_access_rights) {
    NEED_SAW();
    HANDLE sy = CreateEventExA(NULL, NULL, CREATE_EVENT_MANUAL_RESET | CREATE_EVENT_INITIAL_SET, SYNCHRONIZE);
    HANDLE ms = CreateEventExA(NULL, NULL, CREATE_EVENT_MANUAL_RESET, EVENT_MODIFY_STATE);
    HANDLE all = CreateEventA(NULL, TRUE, TRUE, NULL);
    if (!sy || !ms) { t_skip("CreateEventExA failed"); return; }
    SET_ERR(0); EXPECT_EQ(SAW(sy, all, 0, FALSE), WAIT_FAILED); EXPECT_ERR(ERROR_ACCESS_DENIED);   /* no MODIFY_STATE */
    SET_ERR(0); EXPECT_EQ(SAW(all, ms, 0, FALSE), WAIT_FAILED); EXPECT_ERR(ERROR_ACCESS_DENIED);   /* no SYNCHRONIZE */
    EXPECT_EQ(SAW(ms, sy, 0, FALSE), WAIT_OBJECT_0);
    CloseHandle(sy); CloseHandle(ms); CloseHandle(all);
}
TEST(saw_alertable_apc) {
    NEED_SAW();
    HANDLE a = CreateEventA(NULL, TRUE, FALSE, NULL), b = CreateEventA(NULL, TRUE, FALSE, NULL);
    volatile LONG hit = 0;
    QueueUserAPC(noop_apc, GetCurrentThread(), (ULONG_PTR)&hit);
    EXPECT_EQ(SAW(a, b, 5000, TRUE), WAIT_IO_COMPLETION);
    EXPECT_EQ(hit, 1);
    EXPECT_EQ(WS(a, 0), WAIT_OBJECT_0);
    CloseHandle(a); CloseHandle(b);
}
TEST(saw_ping_pong_between_threads) {
    NEED_SAW();
    static HANDLE ping, pong; static volatile LONG n;
    ping = CreateEventA(NULL, FALSE, FALSE, NULL); pong = CreateEventA(NULL, FALSE, FALSE, NULL); n = 0;
    DWORD WINAPI peer(LPVOID p) {
        for (int i = 0; i < 100; i++) {
            if (WaitForSingleObject(ping, 5000) != WAIT_OBJECT_0) return 1;
            InterlockedIncrement(&n);
            if (U(SignalObjectAndWait)(pong, ping, 0, FALSE) == WAIT_FAILED) return 2;   /* signal pong, then wait (0 -> timeout) */
        }
        return 0;
    }
    so_thr t = so_start(peer, NULL);
    int ok = 0;
    for (int i = 0; i < 100; i++) {
        SetEvent(ping);
        if (WS(pong, 5000) == WAIT_OBJECT_0) ok++;
    }
    so_join(t, 10000);
    EXPECT_EQ(ok, 100); EXPECT_EQ(n, 100);
    CloseHandle(ping); CloseHandle(pong);
}
TEST(saw_wakes_waiter_on_signal_object) {
    NEED_SAW();
    static HANDLE a, b; static volatile LONG st;
    a = CreateEventA(NULL, TRUE, FALSE, NULL); b = CreateEventA(NULL, TRUE, FALSE, NULL); st = 0;
    DWORD WINAPI waiter(LPVOID p) { InterlockedExchange(&st, 1); DWORD r = WaitForSingleObject(a, 5000); InterlockedExchange(&st, r == WAIT_OBJECT_0 ? 2 : 3); return 0; }
    so_thr t = so_start(waiter, NULL);
    so_spin_until(&st, 1, 3000); Sleep(100);
    EXPECT_EQ(SAW(a, b, 50, FALSE), WAIT_TIMEOUT);
    so_join(t, 5000);
    EXPECT_EQ(st, 2);
    CloseHandle(a); CloseHandle(b);
}
TEST(wait_constants) {
    EXPECT_EQ(STATUS_WAIT_0, 0); EXPECT_EQ(WAIT_ABANDONED_0, 0x80); EXPECT_EQ(WAIT_ABANDONED, 0x80);
    EXPECT_EQ(MAXIMUM_WAIT_OBJECTS, 64);
    EXPECT_EQ(WAIT_TIMEOUT, 0x102); EXPECT_EQ(WAIT_IO_COMPLETION, 0xC0);
    EXPECT_EQ(WAIT_OBJECT_0 + 63, 63);
}
TEST(wait_process_exit_signals_process_handle) {
    NEED_W();
    PROCESS_INFORMATION pi;
    HANDLE hp = t_spawn_child("so_exit7", "", 0, NULL, NULL, NULL, FALSE, NULL, &pi);
    CHECK(hp != NULL); if (!hp) return;
    EXPECT_EQ(WS(hp, 20000), WAIT_OBJECT_0);
    DWORD code = 0; GetExitCodeProcess(hp, &code); EXPECT_EQ(code, 7);
    EXPECT_EQ(WS(hp, 0), WAIT_OBJECT_0);
    HANDLE both[2] = { pi.hThread, hp };
    EXPECT_EQ(WM(2, both, TRUE, 0), WAIT_OBJECT_0);
    CloseHandle(pi.hThread); CloseHandle(hp);
}
CHILD(so_exit7) { return 7; }
