#include "g16.h"
#define WEQ(call, want) do { DWORD r__ = (call); EXPECT_EQ(r__, want); } while (0)
#define NEED REQUIRE(UK(WaitForMultipleObjectsEx))
#define WMO UK(WaitForMultipleObjectsEx)
static HANDLE ev(BOOL manual, BOOL init) { return CreateEventA(NULL, manual, init, NULL); }
static HANDLE closed_h(void) { HANDLE e = ev(1, 0); CloseHandle(e); return e; }
static DWORD WINAPI sig_thread(LPVOID p) { Sleep(80); SetEvent((HANDLE)p); return 0; }
static DWORD WINAPI mutex_thread(LPVOID p) { HANDLE m = (HANDLE)p; WaitForSingleObject(m, 2000); return 0; }
static int g_apc;
static VOID CALLBACK apc_fn(ULONG_PTR x) { g_apc += (int)x; }

TEST(gap16_wmo_count_zero_invalid) {
    NEED; HANDLE h[1] = { ev(1, 1) };
    SET_ERR(0); WEQ(WMO(0, h, FALSE, 0, FALSE), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); WEQ(WMO(0, h, TRUE, 0, FALSE), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); WEQ(WMO(0, h, FALSE, 0, TRUE), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); WEQ(WMO(0, NULL, FALSE, 0, FALSE), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CloseHandle(h[0]);
}
TEST(gap16_wmo_count_over_max_invalid) {
    NEED; HANDLE h[70]; for (int i = 0; i < 70; i++) h[i] = ev(1, 1);
    static const DWORD n[] = { 65, 66, 70, 128, 0x7fffffff, 0xffffffffu };
    for (int i = 0; i < 6; i++) { SET_ERR(0); DWORD r = WMO(n[i], h, FALSE, 0, FALSE); CHECK(r == WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
    SET_ERR(0); WEQ(WMO(65, h, TRUE, 0, TRUE), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    for (int i = 0; i < 70; i++) CloseHandle(h[i]);
}
TEST(gap16_wmo_max_count_64) {
    NEED; HANDLE h[64]; for (int i = 0; i < 64; i++) h[i] = ev(1, 0);
    SET_ERR(0); WEQ(WMO(64, h, FALSE, 0, FALSE), WAIT_TIMEOUT);
    WEQ(WMO(64, h, TRUE, 0, FALSE), WAIT_TIMEOUT);
    SetEvent(h[63]); WEQ(WMO(64, h, FALSE, 0, FALSE), WAIT_OBJECT_0 + 63);
    WEQ(WMO(64, h, TRUE, 0, FALSE), WAIT_TIMEOUT);
    for (int i = 0; i < 64; i++) SetEvent(h[i]);
    WEQ(WMO(64, h, TRUE, 0, FALSE), WAIT_OBJECT_0); WEQ(WMO(64, h, FALSE, 0, FALSE), WAIT_OBJECT_0);
    for (int i = 0; i < 64; i++) CloseHandle(h[i]);
}
TEST(gap16_wmo_index_reporting) {
    NEED; HANDLE h[5]; for (int i = 0; i < 5; i++) h[i] = ev(1, 0);
    for (int i = 0; i < 5; i++) { SetEvent(h[i]); WEQ(WMO(5, h, FALSE, 0, FALSE), WAIT_OBJECT_0 + i); ResetEvent(h[i]); }
    SetEvent(h[1]); SetEvent(h[3]); WEQ(WMO(5, h, FALSE, 0, FALSE), WAIT_OBJECT_0 + 1);
    ResetEvent(h[1]); WEQ(WMO(5, h, FALSE, 0, FALSE), WAIT_OBJECT_0 + 3);
    for (int i = 0; i < 5; i++) CloseHandle(h[i]);
}
TEST(gap16_wmo_single_handle_and_wait_all_equivalence) {
    NEED; HANDLE e = ev(1, 0);
    WEQ(WMO(1, &e, FALSE, 0, FALSE), WAIT_TIMEOUT); WEQ(WMO(1, &e, TRUE, 0, FALSE), WAIT_TIMEOUT);
    SetEvent(e); WEQ(WMO(1, &e, FALSE, 0, FALSE), WAIT_OBJECT_0); WEQ(WMO(1, &e, TRUE, 0, FALSE), WAIT_OBJECT_0);
    CloseHandle(e);
}
TEST(gap16_wmo_autoreset_consumed_once_each) {
    NEED; HANDLE h[2] = { ev(0, 1), ev(0, 1) };
    WEQ(WMO(2, h, FALSE, 0, FALSE), WAIT_OBJECT_0); WEQ(WMO(2, h, FALSE, 0, FALSE), WAIT_OBJECT_0 + 1); WEQ(WMO(2, h, FALSE, 0, FALSE), WAIT_TIMEOUT);
    CloseHandle(h[0]); CloseHandle(h[1]);
}
TEST(gap16_wmo_manual_reset_not_consumed) {
    NEED; HANDLE h[2] = { ev(1, 1), ev(1, 1) };
    for (int i = 0; i < 5; i++) { WEQ(WMO(2, h, TRUE, 0, FALSE), WAIT_OBJECT_0); WEQ(WMO(2, h, FALSE, 0, FALSE), WAIT_OBJECT_0); }
    CloseHandle(h[0]); CloseHandle(h[1]);
}
TEST(gap16_wmo_waitall_is_atomic_no_partial_consume) {
    NEED; HANDLE h[2] = { ev(0, 1), ev(0, 0) };
    WEQ(WMO(2, h, TRUE, 0, FALSE), WAIT_TIMEOUT);
    WEQ(WaitForSingleObject(h[0], 0), WAIT_OBJECT_0);   /* h[0] was not consumed by the failed wait-all */
    SetEvent(h[0]); SetEvent(h[1]);
    WEQ(WMO(2, h, TRUE, 0, FALSE), WAIT_OBJECT_0);
    WEQ(WaitForSingleObject(h[0], 0), WAIT_TIMEOUT); WEQ(WaitForSingleObject(h[1], 0), WAIT_TIMEOUT);   /* both consumed */
    CloseHandle(h[0]); CloseHandle(h[1]);
}
TEST(gap16_wmo_timeout_elapsed) {
    NEED; HANDLE h[2] = { ev(1, 0), ev(1, 0) };
    ULONGLONG t0 = GetTickCount64(); DWORD r = WMO(2, h, FALSE, 150, FALSE); ULONGLONG dt = GetTickCount64() - t0;
    EXPECT_EQ(r, WAIT_TIMEOUT); CHECKF(dt >= 120 && dt < 2500, "dt %llu", dt);
    t0 = GetTickCount64(); r = WMO(2, h, TRUE, 100, FALSE); dt = GetTickCount64() - t0;
    EXPECT_EQ(r, WAIT_TIMEOUT); CHECKF(dt >= 70 && dt < 2500, "dt %llu", dt);
    CloseHandle(h[0]); CloseHandle(h[1]);
}
TEST(gap16_wmo_infinite_with_signaled_returns) {
    NEED; HANDLE h[2] = { ev(1, 0), ev(1, 1) };
    WEQ(WMO(2, h, FALSE, INFINITE, FALSE), WAIT_OBJECT_0 + 1);
    SetEvent(h[0]); WEQ(WMO(2, h, TRUE, INFINITE, FALSE), WAIT_OBJECT_0);
    CloseHandle(h[0]); CloseHandle(h[1]);
}
TEST(gap16_wmo_signal_from_other_thread) {
    NEED; HANDLE h[3] = { ev(1, 0), ev(1, 0), ev(1, 0) };
    HANDLE t = CreateThread(NULL, 0, sig_thread, h[1], 0, NULL); REQUIRE(t);
    ULONGLONG t0 = GetTickCount64(); DWORD r = WMO(3, h, FALSE, 5000, FALSE);
    EXPECT_EQ(r, WAIT_OBJECT_0 + 1); CHECK(GetTickCount64() - t0 < 4000);
    WaitForSingleObject(t, 3000); CloseHandle(t);
    CloseHandle(h[0]); CloseHandle(h[1]); CloseHandle(h[2]);
}
TEST(gap16_wmo_waitall_completes_when_last_signaled) {
    NEED; HANDLE h[2] = { ev(1, 1), ev(1, 0) };
    HANDLE t = CreateThread(NULL, 0, sig_thread, h[1], 0, NULL); REQUIRE(t);
    WEQ(WMO(2, h, TRUE, 5000, FALSE), WAIT_OBJECT_0);
    WaitForSingleObject(t, 3000); CloseHandle(t); CloseHandle(h[0]); CloseHandle(h[1]);
}
TEST(gap16_wmo_closed_handle) {
    NEED; HANDLE h[3] = { ev(1, 1), closed_h(), ev(1, 1) }; HANDLE bad = h[1];
    SET_ERR(0); WEQ(WMO(3, h, FALSE, 0, FALSE), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); WEQ(WMO(3, h, TRUE, 0, FALSE), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); WEQ(WMO(1, &bad, FALSE, 0, FALSE), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(h[0]); CloseHandle(h[2]);
}
TEST(gap16_wmo_null_handle_entry) {
    NEED; HANDLE h[2] = { ev(1, 1), NULL };
    SET_ERR(0); WEQ(WMO(2, h, FALSE, 0, FALSE), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); WEQ(WMO(2, h, TRUE, 0, FALSE), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE n = NULL; SET_ERR(0); WEQ(WMO(1, &n, FALSE, 0, FALSE), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(h[0]);
}
TEST(gap16_wmo_non_waitable_object) {
    NEED; HANDLE sec = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, NULL); REQUIRE(sec);
    HANDLE h[2] = { ev(1, 1), sec };
    SET_ERR(0); WEQ(WMO(1, &sec, FALSE, 0, FALSE), WAIT_FAILED); EXPECT_ERR2(ERROR_INVALID_HANDLE, ERROR_ACCESS_DENIED);
    SET_ERR(0); { DWORD r = WMO(2, h, FALSE, 0, FALSE); t_note("signaled + section in array -> %lu err %lu", r, GetLastError()); }
    CloseHandle(sec); CloseHandle(h[0]);
}
TEST(gap16_wmo_semaphore_counts) {
    NEED; HANDLE s = CreateSemaphoreA(NULL, 2, 5, NULL), e = ev(1, 0); HANDLE h[2] = { s, e };
    WEQ(WMO(2, h, FALSE, 0, FALSE), WAIT_OBJECT_0); WEQ(WMO(2, h, FALSE, 0, FALSE), WAIT_OBJECT_0); WEQ(WMO(2, h, FALSE, 0, FALSE), WAIT_TIMEOUT);
    LONG prev = -1; ReleaseSemaphore(s, 1, &prev); EXPECT_EQ(prev, 0);
    SetEvent(e); WEQ(WMO(2, h, TRUE, 0, FALSE), WAIT_OBJECT_0);
    ReleaseSemaphore(s, 1, &prev); EXPECT_EQ(prev, 0);
    CloseHandle(s); CloseHandle(e);
}
TEST(gap16_wmo_mutex_recursive) {
    NEED; HANDLE m = CreateMutexA(NULL, FALSE, NULL), e = ev(1, 0); HANDLE h[2] = { e, m };
    WEQ(WMO(2, h, FALSE, 0, FALSE), WAIT_OBJECT_0 + 1); WEQ(WMO(2, h, FALSE, 0, FALSE), WAIT_OBJECT_0 + 1);
    CHECK(ReleaseMutex(m)); CHECK(ReleaseMutex(m)); SET_ERR(0); CHECK(!ReleaseMutex(m)); EXPECT_ERR(ERROR_NOT_OWNER);
    CloseHandle(m); CloseHandle(e);
}
TEST(gap16_wmo_abandoned_mutex) {
    NEED; HANDLE m = CreateMutexA(NULL, FALSE, NULL), e = ev(1, 0); HANDLE h[2] = { e, m };
    HANDLE t = CreateThread(NULL, 0, mutex_thread, m, 0, NULL); REQUIRE(t); WaitForSingleObject(t, 3000); CloseHandle(t);
    WEQ(WMO(2, h, FALSE, 0, FALSE), WAIT_ABANDONED_0 + 1);
    CHECK(ReleaseMutex(m));
    HANDLE m2 = CreateMutexA(NULL, FALSE, NULL); HANDLE h2[2] = { e, m2 }; t = CreateThread(NULL, 0, mutex_thread, m2, 0, NULL); WaitForSingleObject(t, 3000); CloseHandle(t);
    SetEvent(e); DWORD r = WMO(2, h2, TRUE, 0, FALSE); CHECKF(r == WAIT_ABANDONED_0 || r == WAIT_ABANDONED_0 + 1, "r %lu", r);
    ReleaseMutex(m2); CloseHandle(m); CloseHandle(m2); CloseHandle(e);
}
TEST(gap16_wmo_thread_and_process_handles) {
    NEED; HANDLE ready = ev(1, 0); HANDLE t = CreateThread(NULL, 0, sig_thread, ready, 0, NULL); REQUIRE(t);
    HANDLE h[3] = { ev(1, 0), t, GetCurrentProcess() };
    DWORD r = WMO(2, h, FALSE, 5000, FALSE); EXPECT_EQ(r, WAIT_OBJECT_0 + 1);
    WEQ(WMO(2, h, TRUE, 0, FALSE), WAIT_TIMEOUT);
    WEQ(WaitForSingleObject(ready, 0), WAIT_OBJECT_0);
    CloseHandle(t); CloseHandle(ready); CloseHandle(h[0]);
}
TEST(gap16_wmo_duplicate_handles_waitany) {
    NEED; HANDLE e = ev(1, 1), f = ev(1, 0); HANDLE h[3] = { f, e, e };
    WEQ(WMO(3, h, FALSE, 0, FALSE), WAIT_OBJECT_0 + 1);
    CloseHandle(e); CloseHandle(f);
}
TEST(gap16_wmo_duplicate_handles_waitall) {
    NEED; HANDLE e = ev(1, 1); HANDLE h[2] = { e, e }; SET_ERR(0);
    DWORD r = WMO(2, h, TRUE, 0, FALSE); t_note("duplicate handles in wait-all -> %lu err %lu", r, GetLastError());
    CloseHandle(e);
}
TEST(gap16_wmo_duplicate_distinct_handle_values_same_object) {
    NEED; HANDLE e = ev(0, 1), d = NULL; DuplicateHandle(GetCurrentProcess(), e, GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS); REQUIRE(d);
    HANDLE h[2] = { e, d }; SET_ERR(0);
    DWORD r = WMO(2, h, TRUE, 0, FALSE); t_note("two handles, same object, wait-all -> %lu err %lu", r, GetLastError());
    CloseHandle(e); CloseHandle(d);
}
TEST(gap16_wmo_alertable_apc_interrupts) {
    NEED; HANDLE h[2] = { ev(1, 0), ev(1, 0) }; g_apc = 0;
    CHECK(QueueUserAPC(apc_fn, GetCurrentThread(), 5));
    DWORD r = WMO(2, h, FALSE, 3000, TRUE); EXPECT_EQ(r, WAIT_IO_COMPLETION); EXPECT_EQ(g_apc, 5);
    CHECK(QueueUserAPC(apc_fn, GetCurrentThread(), 7));
    r = WMO(2, h, TRUE, 3000, TRUE); EXPECT_EQ(r, WAIT_IO_COMPLETION); EXPECT_EQ(g_apc, 12);
    WEQ(WMO(2, h, FALSE, 20, TRUE), WAIT_TIMEOUT);
    CloseHandle(h[0]); CloseHandle(h[1]);
}
TEST(gap16_wmo_non_alertable_leaves_apc_queued) {
    NEED; HANDLE h[1] = { ev(1, 0) }; g_apc = 0;
    CHECK(QueueUserAPC(apc_fn, GetCurrentThread(), 3));
    WEQ(WMO(1, h, FALSE, 30, FALSE), WAIT_TIMEOUT); EXPECT_EQ(g_apc, 0);
    WEQ(WMO(1, h, FALSE, 3000, TRUE), WAIT_IO_COMPLETION); EXPECT_EQ(g_apc, 3);
    CloseHandle(h[0]);
}
TEST(gap16_wmo_alertable_signaled_object_vs_apc) {
    NEED; HANDLE h[1] = { ev(1, 1) }; g_apc = 0;
    CHECK(QueueUserAPC(apc_fn, GetCurrentThread(), 1));
    DWORD r = WMO(1, h, FALSE, 0, TRUE); t_note("signaled + pending APC -> %lu apc=%d", r, g_apc);
    CHECK(r == WAIT_OBJECT_0 || r == WAIT_IO_COMPLETION);
    SleepEx(0, TRUE); EXPECT_EQ(g_apc, 1);
    CloseHandle(h[0]);
}
TEST(gap16_wmo_alertable_invalid_still_fails) {
    NEED; HANDLE h[1] = { closed_h() }; SET_ERR(0);
    WEQ(WMO(1, h, FALSE, 0, TRUE), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); WEQ(WMO(0, h, FALSE, 0, TRUE), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap16_wmo_bool_values_nonzero) {
    NEED; HANDLE h[2] = { ev(1, 1), ev(1, 0) };
    WEQ(WMO(2, h, 1, 0, 0), WAIT_TIMEOUT); WEQ(WMO(2, h, 0, 0, 0), WAIT_OBJECT_0); WEQ(WMO(2, h, 0, 0, 1), WAIT_OBJECT_0);
    SET_ERR(0); DWORD r2 = WMO(2, h, 2, 0, 0); t_note("bWaitAll=2 -> %lu err %lu", r2, GetLastError());
    SET_ERR(0); r2 = WMO(2, h, 0, 0, 2); t_note("bAlertable=2 -> %lu err %lu", r2, GetLastError());
    CloseHandle(h[0]); CloseHandle(h[1]);
}
TEST(gap16_wmo_large_timeouts_zero_and_one) {
    NEED; HANDLE e = ev(1, 0);
    WEQ(WMO(1, &e, FALSE, 0, FALSE), WAIT_TIMEOUT); WEQ(WMO(1, &e, FALSE, 1, FALSE), WAIT_TIMEOUT);
    WEQ(WMO(1, &e, FALSE, 0xFFFFFFFEu - 0xFFFFFFFEu + 10, FALSE), WAIT_TIMEOUT);
    CloseHandle(e);
}
TEST(gap16_wmo_repeated_calls_stress) {
    NEED; HANDLE h[3] = { ev(0, 0), ev(0, 0), ev(0, 0) }; int ok = 1;
    for (int i = 0; i < 3000; i++) { int k = i % 3; SetEvent(h[k]); if (WMO(3, h, FALSE, 0, FALSE) != WAIT_OBJECT_0 + k) ok = 0; }
    CHECK(ok); for (int i = 0; i < 3; i++) CloseHandle(h[i]);
}
TEST(gap16_wmo_timer_handle) {
    NEED; HANDLE tm = CreateWaitableTimerA(NULL, TRUE, NULL), e = ev(1, 0); REQUIRE(tm); HANDLE h[2] = { e, tm };
    LARGE_INTEGER due; due.QuadPart = -500000;   /* 50 ms */
    CHECK(SetWaitableTimer(tm, &due, 0, NULL, NULL, FALSE));
    WEQ(WMO(2, h, FALSE, 3000, FALSE), WAIT_OBJECT_0 + 1);
    WEQ(WMO(2, h, FALSE, 0, FALSE), WAIT_OBJECT_0 + 1);   /* manual-reset timer stays signaled */
    CloseHandle(tm); CloseHandle(e);
}
TEST(gap16_wmo_file_handle_signaled_state) {
    NEED; char p[MAX_PATH]; t_tmpfile(p, sizeof p, "g16_wmo.bin");
    HANDLE f = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); REQUIRE(f != INVALID_HANDLE_VALUE);
    DWORD r = WMO(1, &f, FALSE, 0, FALSE); t_note("sync file handle wait -> %lu", r);
    CHECK(r == WAIT_OBJECT_0 || r == WAIT_TIMEOUT); CloseHandle(f); DeleteFileA(p);
}
