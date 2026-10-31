#include "tc_util.h"

UDECL(VOID, GetSystemTimeAdjustmentPrecise, (PDWORD64, PDWORD64, PBOOL));
UDECL(BOOL, QueryIdleProcessorCycleTimeEx, (USHORT, PULONG, PULONG64));

static unsigned long long qpc(void) { LARGE_INTEGER l; U(QueryPerformanceCounter)(&l); return (unsigned long long)l.QuadPart; }
static unsigned long long qpf(void) { LARGE_INTEGER l; U(QueryPerformanceFrequency)(&l); return (unsigned long long)l.QuadPart; }
static void spin_ms(DWORD ms) { ULONGLONG t0 = GetTickCount64(); while (GetTickCount64() - t0 < ms) { volatile int x = 0; for (int i = 0; i < 1000; i++) x += i; } }

/* ---- system time getters ---- */
TEST(clocks_systemtime_fields_valid) {
    REQUIRE(U(GetSystemTime)); REQUIRE(U(GetLocalTime));
    SYSTEMTIME s, l; U(GetSystemTime)(&s); U(GetLocalTime)(&l);
    SYSTEMTIME *p[2] = {&s, &l};
    for (int i = 0; i < 2; i++) {
        SYSTEMTIME *t = p[i];
        CHECK(t->wMonth >= 1 && t->wMonth <= 12); CHECK(t->wDay >= 1 && t->wDay <= tc_dim(t->wYear, t->wMonth));
        CHECK(t->wHour < 24 && t->wMinute < 60 && t->wSecond < 60 && t->wMilliseconds < 1000);
        CHECK(t->wDayOfWeek == tc_dow(t->wYear, t->wMonth, t->wDay));
        if (t->wYear < 2020 || t->wYear > 2100) t_note("implausible year %u", t->wYear);
    }
}
TEST(clocks_systemtime_vs_filetime_consistent) {
    REQUIRE(U(GetSystemTime)); REQUIRE(U(GetSystemTimeAsFileTime)); REQUIRE(U(SystemTimeToFileTime));
    FILETIME a, c; SYSTEMTIME s;
    U(GetSystemTimeAsFileTime)(&a); U(GetSystemTime)(&s);
    CHECK(U(SystemTimeToFileTime)(&s, &c));
    long long d = (long long)tc_u64(&c) - (long long)tc_u64(&a);
    CHECKF(d > -500000 && d < 500000, "diff %lld (50ms)", d);
}
TEST(clocks_filetime_variants_consistent) {
    REQUIRE(U(GetSystemTimeAsFileTime)); REQUIRE(U(GetSystemTimePreciseAsFileTime));
    FILETIME a, p, a2;
    U(GetSystemTimeAsFileTime)(&a); U(GetSystemTimePreciseAsFileTime)(&p); U(GetSystemTimeAsFileTime)(&a2);
    long long d = (long long)tc_u64(&p) - (long long)tc_u64(&a);
    CHECKF(d > -500000 && d < 500000, "precise-coarse %lld", d);
    CHECK(tc_u64(&a2) >= tc_u64(&a));
    CHECKF(tc_u64(&p) + 500000 >= tc_u64(&a), "precise not far behind coarse");
}
TEST(clocks_filetime_monotonic_100k) {
    REQUIRE(U(GetSystemTimeAsFileTime)); REQUIRE(U(GetSystemTimePreciseAsFileTime));
    unsigned long long prev = 0, prevp = 0; int back = 0, backp = 0;
    for (int i = 0; i < 100000; i++) {
        FILETIME f, p; U(GetSystemTimeAsFileTime)(&f); U(GetSystemTimePreciseAsFileTime)(&p);
        if (tc_u64(&f) < prev) back++; if (tc_u64(&p) < prevp) backp++;
        prev = tc_u64(&f); prevp = tc_u64(&p);
    }
    if (back || backp) t_note("clock stepped backwards: coarse %d precise %d (clock adjust?)", back, backp);
    CHECK(back == 0 && backp == 0);
}
TEST(clocks_precise_finer_resolution) {
    REQUIRE(U(GetSystemTimePreciseAsFileTime)); REQUIRE(U(GetSystemTimeAsFileTime));
    /* precise values advance in many distinct steps in 5ms; coarse only by tick (>= ~0.5ms) */
    unsigned long long t0 = GetTickCount64(); int dp = 0, dc = 0; unsigned long long lp = 0, lc = 0;
    while (GetTickCount64() - t0 < 20) {
        FILETIME p, c; U(GetSystemTimePreciseAsFileTime)(&p); U(GetSystemTimeAsFileTime)(&c);
        if (tc_u64(&p) != lp) { dp++; lp = tc_u64(&p); } if (tc_u64(&c) != lc) { dc++; lc = tc_u64(&c); }
    }
    t_note("distinct values in 20ms: precise %d coarse %d", dp, dc);
    CHECK(dp >= dc);
    CHECK(dp > 100);
}
TEST(clocks_systemtime_precise_min_step) {
    REQUIRE(U(GetSystemTimePreciseAsFileTime));
    FILETIME a, b; U(GetSystemTimePreciseAsFileTime)(&a);
    int n = 0; do { U(GetSystemTimePreciseAsFileTime)(&b); n++; } while (tc_u64(&b) == tc_u64(&a) && n < 1000000);
    unsigned long long step = tc_u64(&b) - tc_u64(&a);
    t_note("smallest observed step %llu x100ns", step);
    CHECK(step < 100000); /* < 10ms */
}
TEST(clocks_local_vs_system_bias) {
    REQUIRE(U(GetSystemTime)); REQUIRE(U(GetLocalTime));
    SYSTEMTIME s, l; U(GetSystemTime)(&s); U(GetLocalTime)(&l);
    FILETIME fs, fl; SystemTimeToFileTime(&s, &fs); SystemTimeToFileTime(&l, &fl);
    long long d = ((long long)tc_u64(&fl) - (long long)tc_u64(&fs)) / 10000000LL;
    CHECK(d > -15 * 3600 && d < 15 * 3600);
    CHECK(d % 900 == 0 || d % 900 == 1 || d % 900 == -1 || d % 900 == 899 || d % 900 == -899);
}
TEST(clocks_systemtime_advances) {
    REQUIRE(U(GetSystemTime)); REQUIRE(U(GetSystemTimeAsFileTime));
    FILETIME a, b; U(GetSystemTimeAsFileTime)(&a); Sleep(120); U(GetSystemTimeAsFileTime)(&b);
    long long ms = ((long long)tc_u64(&b) - (long long)tc_u64(&a)) / 10000;
    CHECKF(ms >= 90 && ms < 3000, "advance %lld ms over Sleep(120)", ms);
}
TEST(clocks_usershareddata_systemtime) {
    REQUIRE(U(GetSystemTimeAsFileTime));
    /* the shared page KUSER_SHARED_DATA is mapped at 0x7ffe0000; SystemTime at +0x14 (KSYSTEM_TIME) */
    volatile unsigned *k = (volatile unsigned *)0x7ffe0014; FILETIME f;
    unsigned lo, hi, hi2;
    do { hi = k[1]; lo = k[0]; hi2 = k[2]; } while (hi != hi2);
    U(GetSystemTimeAsFileTime)(&f);
    long long d = (long long)tc_u64(&f) - (long long)(((unsigned long long)hi << 32) | lo);
    CHECKF(d > -1000000 && d < 1000000, "shared data diff %lld", d);
}

/* ---- tick count ---- */
TEST(clocks_tick_monotonic) {
    REQUIRE(U(GetTickCount)); REQUIRE(U(GetTickCount64));
    ULONGLONG p = 0; int bad = 0; DWORD pd = 0;
    for (int i = 0; i < 100000; i++) {
        ULONGLONG t = U(GetTickCount64)(); DWORD d = U(GetTickCount)();
        if (t < p) bad++; p = t;
        if (i && (int)(d - pd) < 0) bad++; pd = d;
    }
    EXPECT_EQ(bad, 0);
}
TEST(clocks_tick_low_dword_relation) {
    REQUIRE(U(GetTickCount)); REQUIRE(U(GetTickCount64));
    for (int i = 0; i < 100; i++) {
        ULONGLONG a = U(GetTickCount64)(); DWORD d = U(GetTickCount)(); ULONGLONG b = U(GetTickCount64)();
        DWORD lo_a = (DWORD)a, lo_b = (DWORD)b;
        CHECKF((DWORD)(d - lo_a) <= (DWORD)(lo_b - lo_a) || (DWORD)(d - lo_a) < 100, "tick %u in [%u,%u]", d, lo_a, lo_b);
    }
}
TEST(clocks_tick_resolution_and_granularity) {
    REQUIRE(U(GetTickCount64));
    ULONGLONG a = U(GetTickCount64)(), b; int steps = 0; ULONGLONG min = ~0ULL, last = a;
    ULONGLONG t0 = a;
    while ((b = U(GetTickCount64)()) - t0 < 200) { if (b != last) { if (b - last < min) min = b - last; last = b; steps++; } }
    t_note("tick steps in 200ms: %d min step %llu ms", steps, min);
    CHECK(steps >= 5); CHECK(min >= 1);
}
TEST(clocks_tick_uptime_plausible) {
    REQUIRE(U(GetTickCount64));
    ULONGLONG t = U(GetTickCount64)();
    CHECK(t > 0); CHECK(t < 3650ULL * 86400 * 1000);
}
TEST(clocks_tick_vs_interrupt_time) {
    REQUIRE(U(GetTickCount64)); 
    ULONGLONG ub = 0; REQUIRE(U(QueryUnbiasedInterruptTime));
    ULONGLONG t = U(GetTickCount64)(); CHECK(U(QueryUnbiasedInterruptTime)(&ub)); ULONGLONG t2 = U(GetTickCount64)();
    /* GetTickCount64 counts interrupt time including sleep time in Win10 */
    ULONGLONG ms = ub / 10000;
    t_note("tick %llu unbiased ms %llu", t, ms);
    CHECK(ms <= t2 + 50);
}
TEST(clocks_sleep_100ms_range) {
    REQUIRE(U(Sleep)); REQUIRE(U(GetTickCount64));
    for (int i = 0; i < 3; i++) {
        ULONGLONG a = qpc(); ULONGLONG t0 = GetTickCount64(); U(Sleep)(100); ULONGLONG t1 = GetTickCount64(); ULONGLONG b = qpc();
        long long ms = (long long)((b - a) * 1000 / qpf());
        CHECKF(ms >= 90 && ms < 1000, "Sleep(100) took %lld ms", ms);
        CHECK(t1 - t0 >= 90);
    }
}
TEST(clocks_sleep_zero_and_one) {
    REQUIRE(U(Sleep));
    unsigned long long f = qpf(), a = qpc();
    for (int i = 0; i < 100; i++) U(Sleep)(0);
    long long ms0 = (long long)((qpc() - a) * 1000 / f);
    CHECKF(ms0 < 2000, "100x Sleep(0) %lld ms", ms0);
    a = qpc(); for (int i = 0; i < 20; i++) U(Sleep)(1);
    long long ms1 = (long long)((qpc() - a) * 1000 / f);
    CHECKF(ms1 >= 15 && ms1 < 2000, "20x Sleep(1) %lld ms", ms1);
}
TEST(clocks_sleep_15_and_50) {
    REQUIRE(U(Sleep));
    int ms_list[] = {15, 50, 250};
    for (int i = 0; i < 3; i++) {
        unsigned long long a = qpc(); U(Sleep)(ms_list[i]); long long ms = (long long)((qpc() - a) * 1000 / qpf());
        CHECKF(ms >= ms_list[i] - 5 && ms < ms_list[i] + 1000, "Sleep(%d) took %lld", ms_list[i], ms);
    }
}
TEST(clocks_sleepex_basic) {
    REQUIRE(U(SleepEx));
    unsigned long long a = qpc(); DWORD r = U(SleepEx)(60, FALSE); long long ms = (long long)((qpc() - a) * 1000 / qpf());
    EXPECT_EQ(r, 0); CHECKF(ms >= 55 && ms < 1000, "ms %lld", ms);
    a = qpc(); r = U(SleepEx)(0, TRUE); EXPECT_EQ(r, 0);
    r = U(SleepEx)(0, FALSE); EXPECT_EQ(r, 0);
}
static VOID CALLBACK apc_fn(ULONG_PTR p) { (*(volatile int *)p)++; }
TEST(clocks_sleepex_alertable_apc) {
    REQUIRE(U(SleepEx)); REQUIRE(U(QueueUserAPC));
    volatile int hit = 0;
    CHECK(U(QueueUserAPC)(apc_fn, GetCurrentThread(), (ULONG_PTR)&hit));
    DWORD r = U(SleepEx)(2000, TRUE);
    EXPECT_EQ(r, WAIT_IO_COMPLETION); EXPECT_EQ(hit, 1);
    /* non-alertable sleep doesn't run APC */
    CHECK(U(QueueUserAPC)(apc_fn, GetCurrentThread(), (ULONG_PTR)&hit));
    r = U(SleepEx)(20, FALSE); EXPECT_EQ(r, 0); EXPECT_EQ(hit, 1);
    r = U(SleepEx)(0, TRUE); EXPECT_EQ(r, WAIT_IO_COMPLETION); EXPECT_EQ(hit, 2);
}
TEST(clocks_sleep_infinite_apc_wakes) {
    REQUIRE(U(SleepEx)); REQUIRE(U(QueueUserAPC));
    /* SleepEx(INFINITE, TRUE) in a thread is woken by APC */
    extern DWORD WINAPI tc_alert_sleeper(LPVOID);
    HANDLE h = CreateThread(NULL, 0, tc_alert_sleeper, NULL, 0, NULL);
    CHECK(h != NULL); if (!h) return;
    Sleep(50);
    DWORD w = WaitForSingleObject(h, 100); EXPECT_EQ(w, WAIT_TIMEOUT);
    volatile int hit = 0; CHECK(U(QueueUserAPC)(apc_fn, h, (ULONG_PTR)&hit));
    w = WaitForSingleObject(h, 3000); EXPECT_EQ(w, WAIT_OBJECT_0);
    DWORD code = 0; GetExitCodeThread(h, &code); EXPECT_EQ(code, WAIT_IO_COMPLETION);
    EXPECT_EQ(hit, 1);
    CloseHandle(h);
}
DWORD WINAPI tc_alert_sleeper(LPVOID p) { return SleepEx(INFINITE, TRUE); }

/* ---- QPC ---- */
TEST(clocks_qpf_constant_nonzero) {
    REQUIRE(U(QueryPerformanceFrequency));
    LARGE_INTEGER a, b; SET_ERR(0);
    CHECK(U(QueryPerformanceFrequency)(&a)); CHECK(a.QuadPart > 0);
    for (int i = 0; i < 1000; i++) { CHECK(U(QueryPerformanceFrequency)(&b)); if (b.QuadPart != a.QuadPart) { CHECK(0); break; } }
    if (a.QuadPart != 10000000) t_note("QPC frequency %lld (Win10 usually 10000000)", a.QuadPart);
    CHECK(a.QuadPart >= 1000000);
}
TEST(clocks_qpc_monotonic_100k) {
    REQUIRE(U(QueryPerformanceCounter));
    LARGE_INTEGER p, c; U(QueryPerformanceCounter)(&p); int bad = 0;
    for (int i = 0; i < 100000; i++) { CHECK(U(QueryPerformanceCounter)(&c)); if (c.QuadPart < p.QuadPart) bad++; p = c; }
    EXPECT_EQ(bad, 0);
}
TEST(clocks_qpc_strictly_advances) {
    REQUIRE(U(QueryPerformanceCounter));
    LARGE_INTEGER a, b; U(QueryPerformanceCounter)(&a); int n = 0;
    do { U(QueryPerformanceCounter)(&b); n++; } while (b.QuadPart == a.QuadPart && n < 10000000);
    CHECK(b.QuadPart > a.QuadPart);
}
TEST(clocks_qpc_sleep_50) {
    REQUIRE(U(QueryPerformanceCounter)); REQUIRE(U(QueryPerformanceFrequency));
    unsigned long long f = qpf(), a = qpc(); Sleep(50); unsigned long long d = qpc() - a;
    CHECKF(d >= f * 45 / 1000 && d < f * 1000 / 1000, "delta %llu freq %llu", d, f);
}
TEST(clocks_qpc_vs_tickcount64) {
    REQUIRE(U(QueryPerformanceCounter)); REQUIRE(U(GetTickCount64));
    unsigned long long f = qpf(), a = qpc(); ULONGLONG t = GetTickCount64();
    Sleep(200);
    unsigned long long b = qpc(); ULONGLONG t2 = GetTickCount64();
    long long qms = (long long)((b - a) * 1000 / f), tms = (long long)(t2 - t);
    CHECKF(qms - tms < 60 && tms - qms < 60, "qpc %lld tick %lld", qms, tms);
}
TEST(clocks_qpc_vs_filetime) {
    REQUIRE(U(QueryPerformanceCounter)); REQUIRE(U(GetSystemTimePreciseAsFileTime));
    unsigned long long f = qpf(), a = qpc(); FILETIME x, y; U(GetSystemTimePreciseAsFileTime)(&x);
    Sleep(150);
    unsigned long long b = qpc(); U(GetSystemTimePreciseAsFileTime)(&y);
    long long qms = (long long)((b - a) * 1000 / f), fms = (long long)((tc_u64(&y) - tc_u64(&x)) / 10000);
    CHECKF(qms - fms < 30 && fms - qms < 30, "qpc %lld ft %lld", qms, fms);
}
TEST(clocks_qpc_no_overflow_calc) {
    REQUIRE(U(QueryPerformanceCounter));
    CHECK(qpc() > 0);
    unsigned long long f = qpf(); CHECK(qpc() / f < 10ULL * 365 * 86400);
}
TEST(clocks_qpc_threads_monotonic_across_cpus) {
    REQUIRE(U(QueryPerformanceCounter));
    /* values read on different threads in sequence must not go backwards */
    unsigned long long a = qpc();
    extern DWORD WINAPI tc_qpc_thread(LPVOID);
    HANDLE h = CreateThread(NULL, 0, tc_qpc_thread, NULL, 0, NULL);
    if (!h) { CHECK(0); return; }
    WaitForSingleObject(h, 5000); DWORD code; GetExitCodeThread(h, &code); CloseHandle(h);
    unsigned long long b = qpc(); CHECK(b >= a);
}
DWORD WINAPI tc_qpc_thread(LPVOID p) { unsigned long long q = qpc(); (void)q; return 0; }

/* ---- interrupt time ---- */
TEST(clocks_interrupt_time_basic) {
    REQUIRE(U(QueryInterruptTime)); REQUIRE(U(QueryInterruptTimePrecise));
    ULONGLONG a = 0, p = 0; U(QueryInterruptTime)(&a); U(QueryInterruptTimePrecise)(&p);
    CHECK(a > 0); CHECK(p >= a || a - p < 200000);
    ULONGLONG t = GetTickCount64() * 10000;
    long long d = (long long)(p - t); if (d < 0) d = -d;
    CHECKF(d < 1000000, "interrupt vs tick %lld (100ms)", d);
}
TEST(clocks_interrupt_time_monotonic_100k) {
    REQUIRE(U(QueryInterruptTime)); REQUIRE(U(QueryInterruptTimePrecise));
    ULONGLONG pa = 0, pp = 0; int b1 = 0, b2 = 0;
    for (int i = 0; i < 100000; i++) {
        ULONGLONG a, p; U(QueryInterruptTime)(&a); U(QueryInterruptTimePrecise)(&p);
        if (a < pa) b1++; if (p < pp) b2++; pa = a; pp = p;
    }
    EXPECT_EQ(b1, 0); EXPECT_EQ(b2, 0);
}
TEST(clocks_interrupt_time_sleep_delta) {
    REQUIRE(U(QueryInterruptTimePrecise));
    ULONGLONG a, b; U(QueryInterruptTimePrecise)(&a); Sleep(100); U(QueryInterruptTimePrecise)(&b);
    long long ms = (long long)((b - a) / 10000);
    CHECKF(ms >= 90 && ms < 1000, "ms %lld", ms);
}
TEST(clocks_unbiased_le_biased) {
    REQUIRE(U(QueryInterruptTimePrecise)); REQUIRE(U(QueryUnbiasedInterruptTimePrecise)); REQUIRE(U(QueryUnbiasedInterruptTime)); REQUIRE(U(QueryInterruptTime));
    for (int i = 0; i < 100; i++) {
        ULONGLONG ub = 0, ubp = 0, bi = 0, bip = 0;
        CHECK(U(QueryUnbiasedInterruptTime)(&ub)); U(QueryUnbiasedInterruptTimePrecise)(&ubp);
        U(QueryInterruptTime)(&bi); U(QueryInterruptTimePrecise)(&bip);
        CHECK(ub > 0); CHECK(ubp > 0);
        CHECKF(ubp <= bip + 100000, "unbiased %llu > biased %llu", ubp, bip);
        CHECKF(ub <= bi + 200000, "unbiased(c) %llu > biased(c) %llu", ub, bi);
    }
}
TEST(clocks_unbiased_monotonic_and_sleep) {
    REQUIRE(U(QueryUnbiasedInterruptTime)); REQUIRE(U(QueryUnbiasedInterruptTimePrecise));
    ULONGLONG a, b, p, q; SET_ERR(0);
    CHECK(U(QueryUnbiasedInterruptTime)(&a)); U(QueryUnbiasedInterruptTimePrecise)(&p);
    Sleep(100);
    CHECK(U(QueryUnbiasedInterruptTime)(&b)); U(QueryUnbiasedInterruptTimePrecise)(&q);
    CHECK(b > a); CHECK(q > p);
    long long ms = (long long)((q - p) / 10000); CHECKF(ms >= 90 && ms < 1000, "ms %lld", ms);
}
TEST(clocks_unbiased_null_outparam_note) {
    REQUIRE(U(QueryUnbiasedInterruptTime));
    t_note("QueryUnbiasedInterruptTime(NULL) is not exercised: documented as invalid pointer (fault) on some builds");
}

/* ---- cycle times ---- */
TEST(clocks_thread_cycle_time) {
    REQUIRE(U(QueryThreadCycleTime));
    if (t_is_wine()) { t_skip("cycle-time queries are stubs under wine"); return; }
    ULONG64 a = 0, b = 0, c = 0;
    SET_ERR(0);
    CHECK(U(QueryThreadCycleTime)(GetCurrentThread(), &a));
    spin_ms(30);
    CHECK(U(QueryThreadCycleTime)(GetCurrentThread(), &b));
    CHECK(b > a);
    CHECK(U(QueryThreadCycleTime)(GetCurrentThread(), &c)); CHECK(c >= b);
    CHECKF(b - a > 1000000ULL, "30ms busy loop only %llu cycles", b - a);
}
TEST(clocks_thread_cycle_time_sleep_small) {
    REQUIRE(U(QueryThreadCycleTime));
    if (t_is_wine()) { t_skip("cycle-time queries are stubs under wine"); return; }
    ULONG64 a, b; U(QueryThreadCycleTime)(GetCurrentThread(), &a); Sleep(100); U(QueryThreadCycleTime)(GetCurrentThread(), &b);
    CHECK(b >= a); CHECKF(b - a < 200000000ULL, "sleeping consumed %llu cycles", b - a);
}
TEST(clocks_thread_cycle_time_invalid_handles) {
    REQUIRE(U(QueryThreadCycleTime));
    if (t_is_wine()) { t_skip("cycle-time queries are stubs under wine"); return; }
    ULONG64 v = 123;
    SET_ERR(0); CHECK(!U(QueryThreadCycleTime)(NULL, &v)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(QueryThreadCycleTime)(INVALID_HANDLE_VALUE, &v)); t_note("INVALID_HANDLE_VALUE err %lu", GetLastError());
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0); CHECK(!U(QueryThreadCycleTime)(ev, &v)); t_note("event handle err %lu", GetLastError());
    CloseHandle(ev);
    HANDLE h = (HANDLE)(ULONG_PTR)0x7FFFFF8; SET_ERR(0); CHECK(!U(QueryThreadCycleTime)(h, &v)); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(clocks_process_cycle_time) {
    REQUIRE(U(QueryProcessCycleTime));
    if (t_is_wine()) { t_skip("cycle-time queries are stubs under wine"); return; }
    ULONG64 a = 0, b = 0; SET_ERR(0);
    CHECK(U(QueryProcessCycleTime)(GetCurrentProcess(), &a));
    spin_ms(30);
    CHECK(U(QueryProcessCycleTime)(GetCurrentProcess(), &b));
    CHECK(b > a); CHECK(b - a > 1000000ULL);
    ULONG64 c; ULONG64 t; U(QueryThreadCycleTime)(GetCurrentThread(), &t); CHECK(U(QueryProcessCycleTime)((HANDLE)(LONG_PTR)-1, &c));
    CHECKF(c >= t || t - c < 1000000, "process %llu >= thread %llu", c, t);
}
TEST(clocks_process_cycle_time_invalid) {
    REQUIRE(U(QueryProcessCycleTime));
    if (t_is_wine()) { t_skip("cycle-time queries are stubs under wine"); return; }
    ULONG64 v;
    SET_ERR(0); CHECK(!U(QueryProcessCycleTime)(NULL, &v)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE h = (HANDLE)(ULONG_PTR)0x7FFFFF8; SET_ERR(0); CHECK(!U(QueryProcessCycleTime)(h, &v)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0); CHECK(!U(QueryProcessCycleTime)(ev, &v)); t_note("event handle err %lu", GetLastError()); CloseHandle(ev);
}
TEST(clocks_process_cycle_time_child) {
    REQUIRE(U(QueryProcessCycleTime));
    if (t_is_wine()) { t_skip("cycle-time queries are stubs under wine"); return; }
    PROCESS_INFORMATION pi; HANDLE h = t_spawn_child("tc_spin", "", 0, NULL, NULL, NULL, FALSE, NULL, &pi);
    if (!h) { CHECK(0); return; }
    ULONG64 c = 0; WaitForSingleObject(h, 20000);
    SET_ERR(0); BOOL ok = U(QueryProcessCycleTime)(h, &c);
    CHECKF(ok, "err %lu", GetLastError()); if (ok) CHECK(c > 1000000ULL);
    CloseHandle(h); CloseHandle(pi.hThread);
}
CHILD(tc_spin) { spin_ms(50); return 0; }

TEST(clocks_idle_cycle_time) {
    REQUIRE(U(QueryIdleProcessorCycleTime));
    SYSTEM_INFO si; GetSystemInfo(&si); ULONG n = si.dwNumberOfProcessors;
    ULONG64 buf[256]; memset(buf, 0, sizeof buf);
    ULONG len = sizeof buf; SET_ERR(0);
    BOOL r = U(QueryIdleProcessorCycleTime)(&len, buf);
    if (!r) { t_note("QueryIdleProcessorCycleTime err %lu", GetLastError()); return; }
    CHECKF(len == n * 8 || len % 8 == 0, "len %lu for %lu cpus", len, n);
    CHECK(len >= n * 8 || n > 32);
    ULONG64 buf2[256]; ULONG len2 = len; spin_ms(20); CHECK(U(QueryIdleProcessorCycleTime)(&len2, buf2));
    for (ULONG i = 0; i < len / 8 && i < 256; i++) CHECKF(buf2[i] >= buf[i], "cpu %lu idle went back", i);
}
TEST(clocks_idle_cycle_time_small_buffer) {
    REQUIRE(U(QueryIdleProcessorCycleTime));
    if (t_is_wine()) { t_skip("cycle-time queries are stubs under wine"); return; }
    ULONG64 buf[4]; ULONG len = 0; SET_ERR(0);
    BOOL r = U(QueryIdleProcessorCycleTime)(&len, buf);
    t_note("len=0: ret %d err %lu len %lu", r, GetLastError(), len);
    SYSTEM_INFO si; GetSystemInfo(&si);
    if (si.dwNumberOfProcessors > 1) {
        len = 8; SET_ERR(0); r = U(QueryIdleProcessorCycleTime)(&len, buf);
        CHECK(!r); EXPECT_ERR(ERROR_BAD_LENGTH); CHECKF(len == si.dwNumberOfProcessors * 8, "needed %lu", len);
    }
}
TEST(clocks_idle_cycle_time_null_query_size) {
    REQUIRE(U(QueryIdleProcessorCycleTime));
    ULONG len = 0; SET_ERR(0);
    BOOL r = U(QueryIdleProcessorCycleTime)(&len, NULL);
    /* decomp: failure with NULL buffer is reported as success */
    CHECK(r); t_note("size query returned len %lu", len);
    SYSTEM_INFO si; GetSystemInfo(&si); CHECK(len == 0 || len == si.dwNumberOfProcessors * 8);
}
TEST(clocks_idle_cycle_time_ex) {
    REQUIRE(UN(QueryIdleProcessorCycleTimeEx));
    ULONG len = 0; ULONG64 total = 0; SET_ERR(0);
    BOOL r = UN(QueryIdleProcessorCycleTimeEx)(0, &len, NULL);
    t_note("Ex(group0, NULL) ret %d len %lu err %lu", r, len, GetLastError());
    ULONG64 buf[256]; len = sizeof buf; r = UN(QueryIdleProcessorCycleTimeEx)(0, &len, buf);
    if (r) { CHECK(len >= 8); CHECK(len % 8 == 0); } else t_note("Ex err %lu", GetLastError());
    ULONG l2 = sizeof buf; SET_ERR(0);
    r = UN(QueryIdleProcessorCycleTimeEx)(0xFFFF, &l2, buf);
    t_note("bad group ret %d err %lu", r, GetLastError());
}

/* ---- GetSystemTimes ---- */
TEST(clocks_system_times_relations) {
    REQUIRE(U(GetSystemTimes));
    FILETIME i, k, u; SET_ERR(0);
    CHECK(U(GetSystemTimes)(&i, &k, &u));
    /* kernel time includes idle time */
    CHECK(tc_u64(&k) >= tc_u64(&i));
    CHECK(tc_u64(&k) > 0);
    FILETIME i2, k2, u2; spin_ms(50);
    CHECK(U(GetSystemTimes)(&i2, &k2, &u2));
    CHECK(tc_u64(&i2) >= tc_u64(&i)); CHECK(tc_u64(&k2) >= tc_u64(&k)); CHECK(tc_u64(&u2) >= tc_u64(&u));
    unsigned long long total1 = tc_u64(&k) + tc_u64(&u), total2 = tc_u64(&k2) + tc_u64(&u2);
    CHECK(total2 > total1);
}
TEST(clocks_system_times_null_params) {
    REQUIRE(U(GetSystemTimes));
    FILETIME i, k, u;
    SET_ERR(0); CHECK(U(GetSystemTimes)(&i, NULL, NULL)); CHECK(tc_u64(&i) > 0);
    CHECK(U(GetSystemTimes)(NULL, &k, NULL)); CHECK(U(GetSystemTimes)(NULL, NULL, &u));
    CHECK(U(GetSystemTimes)(NULL, NULL, NULL));
    CHECK(U(GetSystemTimes)(NULL, &k, &u));
}
TEST(clocks_system_times_vs_wallclock) {
    REQUIRE(U(GetSystemTimes));
    FILETIME i, k, u, i2, k2, u2; SYSTEM_INFO si; GetSystemInfo(&si);
    U(GetSystemTimes)(&i, &k, &u); unsigned long long q0 = qpc(); Sleep(300); U(GetSystemTimes)(&i2, &k2, &u2); unsigned long long q1 = qpc();
    unsigned long long wall100 = (q1 - q0) * 10000000ULL / qpf();
    unsigned long long tot = (tc_u64(&k2) - tc_u64(&k)) + (tc_u64(&u2) - tc_u64(&u));
    t_note("cpu total %llu vs wall*ncpu %llu", tot, wall100 * si.dwNumberOfProcessors);
    CHECK(tot <= wall100 * si.dwNumberOfProcessors * 2 + 3000000);
}

/* ---- adjustment ---- */
TEST(clocks_system_time_adjustment_query) {
    REQUIRE(U(GetSystemTimeAdjustment));
    DWORD adj = 0, inc = 0; BOOL dis = 99; SET_ERR(0);
    CHECK(U(GetSystemTimeAdjustment)(&adj, &inc, &dis));
    CHECK(dis == TRUE || dis == FALSE);
    CHECK(inc >= 10000 && inc <= 1000000); /* between 1ms and 100ms */
    if (dis) EXPECT_EQ(adj, inc);
    CHECKF(adj > 0, "adj %lu", adj);
    t_note("adj %lu inc %lu disabled %d", adj, inc, dis);
    DWORD adj2, inc2; BOOL dis2; CHECK(U(GetSystemTimeAdjustment)(&adj2, &inc2, &dis2)); CHECK(inc2 == inc);
}
TEST(clocks_system_time_adjustment_vs_tick) {
    REQUIRE(U(GetSystemTimeAdjustment));
    DWORD adj, inc; BOOL dis; U(GetSystemTimeAdjustment)(&adj, &inc, &dis);
    /* clock tick increment is the coarse GetSystemTimeAsFileTime step; typical 156250 */
    FILETIME a, b; U(GetSystemTimeAsFileTime)(&a); unsigned long long min = ~0ULL; int n = 0;
    ULONGLONG t0 = GetTickCount64();
    while (GetTickCount64() - t0 < 300) { U(GetSystemTimeAsFileTime)(&b); if (tc_u64(&b) != tc_u64(&a)) { unsigned long long d = tc_u64(&b) - tc_u64(&a); if (d < min) min = d; a = b; n++; } }
    t_note("observed min step %llu, increment %lu, steps %d", min, inc, n);
    CHECK(n > 3);
    CHECK(min >= 1);
    if (min < inc / 2) t_note("clock tick finer than increment/2: timer resolution raised by another process?");
}
TEST(clocks_system_time_adjustment_precise) {
    REQUIRE(UN(GetSystemTimeAdjustmentPrecise));
    DWORD64 adj = 0, inc = 0; BOOL dis = 99; SET_ERR(0);
    UN(GetSystemTimeAdjustmentPrecise)(&adj, &inc, &dis);
    CHECK(dis == TRUE || dis == FALSE); CHECK(adj > 0); CHECK(inc > 0);
    t_note("precise adj %llu inc %llu disabled %d", adj, inc, dis);
    if (U(GetSystemTimeAdjustment)) {
        DWORD a2, i2; BOOL d2; U(GetSystemTimeAdjustment)(&a2, &i2, &d2);
        /* precise values carry 2^-16 extra bits */
        CHECK(d2 == dis || 1);
        CHECK((adj >> 16) <= 2 * (unsigned long long)a2 + 1000 && (adj >> 16) + 1000 >= a2 / 2);
    }
}
TEST(clocks_set_system_time_adjustment_never) {
    t_note("SetSystemTimeAdjustment(Precise) deliberately not called");
}

/* ---- process/thread times ---- */
TEST(clocks_process_times_basic) {
    REQUIRE(U(GetProcessTimes));
    FILETIME cr, ex, kt, ut; SET_ERR(0);
    CHECK(U(GetProcessTimes)(GetCurrentProcess(), &cr, &ex, &kt, &ut));
    unsigned long long now = tc_now100();
    CHECK(tc_u64(&cr) <= now); CHECK(tc_u64(&cr) + 3600ULL * 10000000 > now);
    EXPECT_EQ(tc_u64(&ex), 0);
    spin_ms(100);
    FILETIME cr2, ex2, kt2, ut2; CHECK(U(GetProcessTimes)(GetCurrentProcess(), &cr2, &ex2, &kt2, &ut2));
    EXPECT_EQ(tc_u64(&cr2), tc_u64(&cr));
    CHECK(tc_u64(&ut2) + tc_u64(&kt2) >= tc_u64(&ut) + tc_u64(&kt));
    CHECKF(tc_u64(&ut2) + tc_u64(&kt2) - tc_u64(&ut) - tc_u64(&kt) > 300000, "cpu advance after 100ms busy");
    CHECK(tc_u64(&ut2) >= tc_u64(&ut)); CHECK(tc_u64(&kt2) >= tc_u64(&kt));
}
TEST(clocks_process_times_pseudo_handle) {
    REQUIRE(U(GetProcessTimes));
    FILETIME a[4], b[4];
    CHECK(U(GetProcessTimes)(GetCurrentProcess(), &a[0], &a[1], &a[2], &a[3]));
    CHECK(U(GetProcessTimes)((HANDLE)(LONG_PTR)-1, &b[0], &b[1], &b[2], &b[3]));
    EXPECT_EQ(tc_u64(&a[0]), tc_u64(&b[0]));
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    if (h) { CHECK(U(GetProcessTimes)(h, &b[0], &b[1], &b[2], &b[3])); EXPECT_EQ(tc_u64(&a[0]), tc_u64(&b[0])); CloseHandle(h); }
}
TEST(clocks_process_times_invalid) {
    REQUIRE(U(GetProcessTimes));
    FILETIME a, b, c, d;
    SET_ERR(0); CHECK(!U(GetProcessTimes)(NULL, &a, &b, &c, &d)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(GetProcessTimes)((HANDLE)(ULONG_PTR)0x7FFFFF8, &a, &b, &c, &d)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0); CHECK(!U(GetProcessTimes)(ev, &a, &b, &c, &d)); t_note("event err %lu", GetLastError()); CloseHandle(ev);
}
TEST(clocks_process_times_exited_child) {
    REQUIRE(U(GetProcessTimes));
    unsigned long long before = tc_now100();
    PROCESS_INFORMATION pi; HANDLE h = t_spawn_child("tc_spin", "", 0, NULL, NULL, NULL, FALSE, NULL, &pi);
    if (!h) { CHECK(0); return; }
    DWORD w = WaitForSingleObject(h, 20000); EXPECT_EQ(w, WAIT_OBJECT_0);
    FILETIME cr, ex, kt, ut; SET_ERR(0);
    BOOL ok = U(GetProcessTimes)(h, &cr, &ex, &kt, &ut);
    CHECK(ok);
    if (ok) {
        CHECK(tc_u64(&ex) > tc_u64(&cr)); CHECK(tc_u64(&cr) + 5000000 >= before);
        CHECK(tc_u64(&ex) <= tc_now100() + 5000000);
        long long life = (long long)(tc_u64(&ex) - tc_u64(&cr)) / 10000; CHECKF(life >= 40, "lifetime %lld ms", life);
        CHECK(tc_u64(&ut) + tc_u64(&kt) > 0);
    }
    CloseHandle(h); CloseHandle(pi.hThread);
}
static DWORD WINAPI busy_thread(LPVOID p) { spin_ms(60); return 0; }
TEST(clocks_thread_times_basic) {
    REQUIRE(U(GetThreadTimes));
    FILETIME cr, ex, kt, ut; SET_ERR(0);
    CHECK(U(GetThreadTimes)(GetCurrentThread(), &cr, &ex, &kt, &ut));
    unsigned long long now = tc_now100(); CHECK(tc_u64(&cr) <= now); CHECK(tc_u64(&cr) + 3600ULL * 10000000 > now);
    EXPECT_EQ(tc_u64(&ex), 0);
    spin_ms(100);
    FILETIME cr2, ex2, kt2, ut2; CHECK(U(GetThreadTimes)(GetCurrentThread(), &cr2, &ex2, &kt2, &ut2));
    EXPECT_EQ(tc_u64(&cr), tc_u64(&cr2));
    unsigned long long d = tc_u64(&kt2) + tc_u64(&ut2) - tc_u64(&kt) - tc_u64(&ut);
    CHECKF(d > 300000 && d < 5000000000ULL, "thread cpu advance %llu", d);
}
TEST(clocks_thread_times_exited_thread) {
    REQUIRE(U(GetThreadTimes));
    HANDLE h = CreateThread(NULL, 0, busy_thread, NULL, 0, NULL); if (!h) { CHECK(0); return; }
    WaitForSingleObject(h, 10000);
    FILETIME cr, ex, kt, ut; CHECK(U(GetThreadTimes)(h, &cr, &ex, &kt, &ut));
    CHECK(tc_u64(&ex) >= tc_u64(&cr)); CHECK(tc_u64(&ex) > 0);
    if (!t_is_wine()) CHECK(tc_u64(&kt) + tc_u64(&ut) > 0);
    FILETIME cr2, ex2, kt2, ut2; CHECK(U(GetThreadTimes)(h, &cr2, &ex2, &kt2, &ut2));
    EXPECT_EQ(tc_u64(&ex), tc_u64(&ex2)); EXPECT_EQ(tc_u64(&ut), tc_u64(&ut2));
    CloseHandle(h);
}
TEST(clocks_thread_times_invalid) {
    REQUIRE(U(GetThreadTimes));
    FILETIME a, b, c, d;
    SET_ERR(0); CHECK(!U(GetThreadTimes)(NULL, &a, &b, &c, &d)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(GetThreadTimes)((HANDLE)(ULONG_PTR)0x7FFFFF8, &a, &b, &c, &d)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0); CHECK(!U(GetThreadTimes)(ev, &a, &b, &c, &d)); t_note("event err %lu", GetLastError()); CloseHandle(ev);
}
TEST(clocks_thread_times_pseudo_handle) {
    REQUIRE(U(GetThreadTimes));
    FILETIME a[4], b[4];
    CHECK(U(GetThreadTimes)(GetCurrentThread(), &a[0], &a[1], &a[2], &a[3]));
    CHECK(U(GetThreadTimes)((HANDLE)(LONG_PTR)-2, &b[0], &b[1], &b[2], &b[3]));
    EXPECT_EQ(tc_u64(&a[0]), tc_u64(&b[0]));
    HANDLE real; CHECK(DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &real, 0, FALSE, DUPLICATE_SAME_ACCESS));
    CHECK(U(GetThreadTimes)(real, &b[0], &b[1], &b[2], &b[3])); EXPECT_EQ(tc_u64(&a[0]), tc_u64(&b[0])); CloseHandle(real);
}
TEST(clocks_process_vs_thread_creation) {
    REQUIRE(U(GetProcessTimes)); REQUIRE(U(GetThreadTimes));
    FILETIME p[4], t[4];
    U(GetProcessTimes)(GetCurrentProcess(), &p[0], &p[1], &p[2], &p[3]); U(GetThreadTimes)(GetCurrentThread(), &t[0], &t[1], &t[2], &t[3]);
    if (t_is_wine() && tc_u64(&t[0]) < tc_u64(&p[0])) { t_skip("wine reports main thread created before process"); return; }
    CHECK(tc_u64(&t[0]) >= tc_u64(&p[0]));
}
TEST(clocks_process_times_cpu_vs_cycle) {
    REQUIRE(U(GetProcessTimes)); REQUIRE(U(QueryProcessCycleTime));
    if (t_is_wine()) { t_skip("cycle-time queries are stubs under wine"); return; }
    FILETIME cr, ex, k1, u1, k2, u2; ULONG64 c1, c2;
    U(GetProcessTimes)(GetCurrentProcess(), &cr, &ex, &k1, &u1); U(QueryProcessCycleTime)(GetCurrentProcess(), &c1);
    spin_ms(200);
    U(GetProcessTimes)(GetCurrentProcess(), &cr, &ex, &k2, &u2); U(QueryProcessCycleTime)(GetCurrentProcess(), &c2);
    unsigned long long cpu100 = tc_u64(&k2) + tc_u64(&u2) - tc_u64(&k1) - tc_u64(&u1), cyc = c2 - c1;
    if (cpu100 == 0) { t_note("cpu time did not advance (coarse accounting)"); return; }
    CHECK(cyc > 0);
    double ghz = (double)cyc / ((double)cpu100 * 100.0);
    t_note("implied clock %.2f GHz", ghz);
    CHECK(ghz > 0.2 && ghz < 8.0);
}
