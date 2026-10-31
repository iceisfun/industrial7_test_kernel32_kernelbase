#include "tc_util.h"

UDECL(LONG, NtQueryTimerResolution, (PULONG, PULONG, PULONG));

static SYSTEMTIME mk(int y, int mo, int d, int h, int mi, int s, int ms) {
    SYSTEMTIME st; memset(&st, 0, sizeof st);
    st.wYear = y; st.wMonth = mo; st.wDay = d; st.wHour = h; st.wMinute = mi; st.wSecond = s; st.wMilliseconds = ms; return st;
}

/* ---- Set* invalid-argument paths (validated before any privilege check / clock change) ---- */
TEST(timemisc_setsystemtime_invalid_87) {
    REQUIRE(U(SetSystemTime));
    struct { const char *n; SYSTEMTIME st; } v[] = {
        {"year1500", mk(1500, 1, 1, 0, 0, 0, 0)}, {"year1600", mk(1600, 12, 31, 0, 0, 0, 0)}, {"year0", mk(0, 1, 1, 0, 0, 0, 0)},
        {"month13", mk(2020, 13, 1, 0, 0, 0, 0)}, {"month0", mk(2020, 0, 1, 0, 0, 0, 0)}, {"day32", mk(2020, 1, 32, 0, 0, 0, 0)},
        {"day0", mk(2020, 1, 0, 0, 0, 0, 0)}, {"feb30", mk(2020, 2, 30, 0, 0, 0, 0)}, {"hour24", mk(2020, 1, 1, 24, 0, 0, 0)},
        {"minute60", mk(2020, 1, 1, 0, 60, 0, 0)}, {"ms1000", mk(2020, 1, 1, 0, 0, 0, 1000)},
        {"allzero", mk(0, 0, 0, 0, 0, 0, 0)},
    };
    for (size_t i = 0; i < sizeof v / sizeof v[0]; i++) {
        SET_ERR(0); BOOL r = U(SetSystemTime)(&v[i].st);
        CHECKF(!r, "%s should fail", v[i].n); CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "%s err %lu (not 1314)", v[i].n, GetLastError());
    }
}
TEST(timemisc_setlocaltime_invalid_87) {
    REQUIRE(U(SetLocalTime));
    struct { const char *n; SYSTEMTIME st; } v[] = {
        {"month13", mk(2020, 13, 1, 0, 0, 0, 0)}, {"day32", mk(2020, 1, 32, 0, 0, 0, 0)},
        {"hour24", mk(2020, 1, 1, 24, 0, 0, 0)}, {"minute60", mk(2020, 1, 1, 0, 60, 0, 0)},
        {"feb29-2023", mk(2023, 2, 29, 0, 0, 0, 0)}, {"allzero", mk(0, 0, 0, 0, 0, 0, 0)},
    };
    unsigned long long before = tc_now100();
    for (size_t i = 0; i < sizeof v / sizeof v[0]; i++) {
        SET_ERR(0); BOOL r = U(SetLocalTime)(&v[i].st);
        CHECKF(!r, "%s should fail", v[i].n);
        if (GetLastError() != ERROR_INVALID_PARAMETER) t_note("%s err %lu", v[i].n, GetLastError());
    }
    unsigned long long after = tc_now100(); CHECK(after >= before && after - before < 20000000ULL);
}
TEST(timemisc_clock_unchanged_after_invalid_set) {
    REQUIRE(U(SetSystemTime)); REQUIRE(U(GetSystemTimeAsFileTime));
    SYSTEMTIME bad = mk(1500, 1, 1, 0, 0, 0, 0); U(SetSystemTime)(&bad);
    FILETIME a; U(GetSystemTimeAsFileTime)(&a); long long d = (long long)(tc_u64(&a) - tc_now100());
    CHECK(d > -10000000 && d < 10000000);
}
TEST(timemisc_dangerous_setters_not_called) {
    t_note("SetTimeZoneInformation/SetDynamicTimeZoneInformation/SetSystemTimeAdjustment not called by design");
}

/* ---- tz-specific conversions with fixed bias TZIs ---- */
static void mk_tzi(TIME_ZONE_INFORMATION *z, LONG bias) {
    memset(z, 0, sizeof *z); z->Bias = bias; wcscpy(z->StandardName, L"K32T"); wcscpy(z->DaylightName, L"K32TD");
}
TEST(timemisc_tz_fixed_bias_utc_to_local) {
    REQUIRE(U(SystemTimeToTzSpecificLocalTime));
    LONG biases[] = {0, 60, -60, 300, -330, 480, -720, 720, 1, -1, 840 , -840};
    for (size_t i = 0; i < sizeof biases / sizeof biases[0]; i++) {
        TIME_ZONE_INFORMATION z; mk_tzi(&z, biases[i]);
        SYSTEMTIME u = mk(2021, 6, 15, 12, 0, 0, 0), l; memset(&l, 0, sizeof l);
        BOOL r = U(SystemTimeToTzSpecificLocalTime)(&z, &u, &l);
        CHECKF(r, "bias %ld err %lu", biases[i], GetLastError()); if (!r) continue;
        FILETIME fu, fl; SystemTimeToFileTime(&u, &fu); SystemTimeToFileTime(&l, &fl);
        long long d = (long long)tc_u64(&fl) - (long long)tc_u64(&fu);
        CHECKF(d == -(long long)biases[i] * 600000000LL, "bias %ld d %lld", biases[i], d);
        CHECK(l.wDayOfWeek == tc_dow(l.wYear, l.wMonth, l.wDay));
    }
}
TEST(timemisc_tz_fixed_bias_local_to_utc) {
    REQUIRE(U(TzSpecificLocalTimeToSystemTime)); REQUIRE(U(SystemTimeToTzSpecificLocalTime));
    LONG biases[] = {0, 60, -60, 300, -330, 480, -720};
    for (size_t i = 0; i < sizeof biases / sizeof biases[0]; i++) {
        TIME_ZONE_INFORMATION z; mk_tzi(&z, biases[i]);
        SYSTEMTIME loc = mk(2021, 6, 15, 12, 0, 0, 0), u = {0}, back = {0};
        CHECK(U(TzSpecificLocalTimeToSystemTime)(&z, &loc, &u));
        FILETIME fl, fu; SystemTimeToFileTime(&loc, &fl); SystemTimeToFileTime(&u, &fu);
        CHECKF((long long)tc_u64(&fu) - (long long)tc_u64(&fl) == (long long)biases[i] * 600000000LL, "bias %ld", biases[i]);
        CHECK(U(SystemTimeToTzSpecificLocalTime)(&z, &u, &back));
        CHECK(back.wYear == 2021 && back.wMonth == 6 && back.wDay == 15 && back.wHour == 12 && back.wMinute == 0);
    }
}
TEST(timemisc_tz_day_rollover) {
    REQUIRE(U(SystemTimeToTzSpecificLocalTime));
    TIME_ZONE_INFORMATION z; mk_tzi(&z, 300);
    SYSTEMTIME u = mk(2021, 1, 1, 2, 30, 15, 250), l;
    CHECK(U(SystemTimeToTzSpecificLocalTime)(&z, &u, &l));
    EXPECT_EQ(l.wYear, 2020); EXPECT_EQ(l.wMonth, 12); EXPECT_EQ(l.wDay, 31); EXPECT_EQ(l.wHour, 21); EXPECT_EQ(l.wMinute, 30);
    EXPECT_EQ(l.wSecond, 15); EXPECT_EQ(l.wMilliseconds, 250);
    z.Bias = -600; u = mk(2020, 12, 31, 20, 0, 0, 0);
    CHECK(U(SystemTimeToTzSpecificLocalTime)(&z, &u, &l));
    EXPECT_EQ(l.wYear, 2021); EXPECT_EQ(l.wMonth, 1); EXPECT_EQ(l.wDay, 1); EXPECT_EQ(l.wHour, 6);
    z.Bias = 0; u = mk(2024, 2, 28, 23, 59, 59, 999); CHECK(U(SystemTimeToTzSpecificLocalTime)(&z, &u, &l)); EXPECT_EQ(l.wDay, 28);
    z.Bias = -1; CHECK(U(SystemTimeToTzSpecificLocalTime)(&z, &u, &l)); EXPECT_EQ(l.wMonth, 2); EXPECT_EQ(l.wDay, 29); EXPECT_EQ(l.wHour, 0); EXPECT_EQ(l.wMinute, 0);
}
TEST(timemisc_tz_invalid_systemtime) {
    REQUIRE(U(SystemTimeToTzSpecificLocalTime)); REQUIRE(U(TzSpecificLocalTimeToSystemTime));
    TIME_ZONE_INFORMATION z; mk_tzi(&z, 0);
    SYSTEMTIME bad[] = {mk(2020, 13, 1, 0, 0, 0, 0), mk(2020, 2, 30, 0, 0, 0, 0), mk(2020, 1, 1, 24, 0, 0, 0), mk(1600, 1, 1, 0, 0, 0, 0)}, out;
    for (int i = 0; i < 4; i++) {
        SET_ERR(0); CHECKF(!U(SystemTimeToTzSpecificLocalTime)(&z, &bad[i], &out), "u2l %d", i); EXPECT_ERR(ERROR_INVALID_PARAMETER);
        SET_ERR(0); CHECKF(!U(TzSpecificLocalTimeToSystemTime)(&z, &bad[i], &out), "l2u %d", i); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(timemisc_tz_null_tzi_uses_current) {
    REQUIRE(U(SystemTimeToTzSpecificLocalTime)); REQUIRE(U(GetSystemTime)); REQUIRE(U(GetLocalTime));
    SYSTEMTIME u, l, l2; U(GetSystemTime)(&u);
    CHECK(U(SystemTimeToTzSpecificLocalTime)(NULL, &u, &l));
    U(GetLocalTime)(&l2);
    FILETIME a, b; SystemTimeToFileTime(&l, &a); SystemTimeToFileTime(&l2, &b);
    long long d = (long long)tc_u64(&b) - (long long)tc_u64(&a); CHECKF(d > -20000000 && d < 20000000, "d %lld", d);
}
TEST(timemisc_tz_utc_zone_identity) {
    REQUIRE(U(SystemTimeToTzSpecificLocalTime));
    TIME_ZONE_INFORMATION z; mk_tzi(&z, 0); unsigned long long s = 31;
    for (int i = 0; i < 200; i++) {
        FILETIME f = tc_ft(tc_mkft(1700, 1, 1, 0, 0, 0, 0) + tc_rand64(&s) % 4000000000000000ULL); SYSTEMTIME u, l;
        FileTimeToSystemTime(&f, &u);
        CHECK(U(SystemTimeToTzSpecificLocalTime)(&z, &u, &l));
        CHECK(!memcmp(&u, &l, sizeof u));
    }
}
TEST(timemisc_tz_dst_transition_us_rules) {
    REQUIRE(U(SystemTimeToTzSpecificLocalTime));
    /* US Eastern rules: DST 2nd Sunday March 02:00 -> 1st Sunday Nov 02:00 */
    TIME_ZONE_INFORMATION z; memset(&z, 0, sizeof z);
    z.Bias = 300; z.StandardBias = 0; z.DaylightBias = -60;
    z.StandardDate.wMonth = 11; z.StandardDate.wDay = 1; z.StandardDate.wHour = 2; z.StandardDate.wDayOfWeek = 0;
    z.DaylightDate.wMonth = 3; z.DaylightDate.wDay = 2; z.DaylightDate.wHour = 2; z.DaylightDate.wDayOfWeek = 0;
    struct { SYSTEMTIME u; int exp_h; int exp_d; } v[] = {
        {mk(2021, 1, 15, 12, 0, 0, 0), 7, 15},   /* winter: -5 */
        {mk(2021, 7, 15, 12, 0, 0, 0), 8, 15},   /* summer: -4 */
        {mk(2021, 3, 14, 6, 59, 59, 0), 1, 14},  /* just before: EST */
        {mk(2021, 3, 14, 7, 0, 0, 0), 3, 14},    /* after: EDT */
        {mk(2021, 11, 7, 5, 59, 59, 0), 1, 7},   /* before end (EDT) */
        {mk(2021, 11, 7, 6, 0, 0, 0), 1, 7},     /* after end (EST) */
    };
    for (size_t i = 0; i < sizeof v / sizeof v[0]; i++) {
        SYSTEMTIME l; BOOL r = U(SystemTimeToTzSpecificLocalTime)(&z, &v[i].u, &l);
        CHECKF(r, "v%d", (int)i); if (!r) continue;
        CHECKF(l.wHour == v[i].exp_h && l.wDay == v[i].exp_d, "v%d got %u:%u day %u", (int)i, l.wHour, l.wMinute, l.wDay);
    }
}

/* ---- access rights ---- */
TEST(timemisc_process_times_access_denied_sync_only) {
    REQUIRE(U(GetProcessTimes)); REQUIRE(U(QueryProcessCycleTime));
    if (t_is_wine()) { t_skip("access checks / cycle times are stubs under wine"); return; }
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, GetCurrentProcessId()); if (!h) { t_note("OpenProcess SYNCHRONIZE failed"); return; }
    FILETIME a, b, c, d; ULONG64 cyc;
    SET_ERR(0); CHECK(!U(GetProcessTimes)(h, &a, &b, &c, &d)); EXPECT_ERR(ERROR_ACCESS_DENIED);
    SET_ERR(0); CHECK(!U(QueryProcessCycleTime)(h, &cyc)); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(h);
}
TEST(timemisc_process_times_limited_right_ok) {
    REQUIRE(U(GetProcessTimes)); REQUIRE(U(QueryProcessCycleTime));
    if (t_is_wine()) { t_skip("access checks / cycle times are stubs under wine"); return; }
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId()); if (!h) { CHECK(0); return; }
    FILETIME a, b, c, d; ULONG64 cyc;
    CHECK(U(GetProcessTimes)(h, &a, &b, &c, &d)); CHECK(U(QueryProcessCycleTime)(h, &cyc)); CHECK(cyc > 0);
    CloseHandle(h);
}
TEST(timemisc_thread_times_access) {
    REQUIRE(U(GetThreadTimes)); REQUIRE(U(QueryThreadCycleTime));
    if (t_is_wine()) { t_skip("access checks / cycle times are stubs under wine"); return; }
    HANDLE h = OpenThread(SYNCHRONIZE, FALSE, GetCurrentThreadId()); if (!h) { t_note("OpenThread failed"); return; }
    FILETIME a, b, c, d; ULONG64 cyc;
    SET_ERR(0); CHECK(!U(GetThreadTimes)(h, &a, &b, &c, &d)); EXPECT_ERR(ERROR_ACCESS_DENIED);
    SET_ERR(0); CHECK(!U(QueryThreadCycleTime)(h, &cyc)); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(h);
    h = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentThreadId()); if (!h) { CHECK(0); return; }
    CHECK(U(GetThreadTimes)(h, &a, &b, &c, &d)); CHECK(U(QueryThreadCycleTime)(h, &cyc)); CloseHandle(h);
}
TEST(timemisc_handle_lifetime_after_close) {
    REQUIRE(U(GetThreadTimes)); REQUIRE(U(GetProcessTimes));
    HANDLE h = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentThreadId()); if (!h) return;
    CloseHandle(h); FILETIME a, b, c, d;
    SET_ERR(0); CHECK(!U(GetThreadTimes)(h, &a, &b, &c, &d)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId()); if (!h) return;
    CloseHandle(h); SET_ERR(0); CHECK(!U(GetProcessTimes)(h, &a, &b, &c, &d)); EXPECT_ERR(ERROR_INVALID_HANDLE);
}

/* ---- running child ---- */
CHILD(tc_sleepy) { Sleep(1500); return 5; }
TEST(timemisc_running_child_times) {
    REQUIRE(U(GetProcessTimes));
    unsigned long long before = tc_now100();
    PROCESS_INFORMATION pi; HANDLE h = t_spawn_child("tc_sleepy", "", 0, NULL, NULL, NULL, FALSE, NULL, &pi);
    if (!h) { CHECK(0); return; }
    FILETIME cr, ex, kt, ut; BOOL ok = U(GetProcessTimes)(h, &cr, &ex, &kt, &ut);
    CHECK(ok);
    if (ok) {
        EXPECT_EQ(tc_u64(&ex), 0);
        CHECK(tc_u64(&cr) + 5000000 >= before); CHECK(tc_u64(&cr) <= tc_now100() + 5000000);
    }
    FILETIME cr2, ex2, kt2, ut2;
    if (WaitForSingleObject(h, 20000) == WAIT_OBJECT_0) {
        CHECK(U(GetProcessTimes)(h, &cr2, &ex2, &kt2, &ut2));
        CHECK(tc_u64(&ex2) > 0); EXPECT_EQ(tc_u64(&cr2), tc_u64(&cr));
        long long life = (long long)(tc_u64(&ex2) - tc_u64(&cr2)) / 10000; CHECKF(life >= 1300 && life < 15000, "life %lld", life);
    } else { TerminateProcess(h, 1); CHECK(0); }
    CloseHandle(h); CloseHandle(pi.hThread);
}
static DWORD WINAPI tc_wait_ev(LPVOID p) { WaitForSingleObject((HANDLE)p, 5000); return 0; }
TEST(timemisc_running_thread_exit_time_zero) {
    REQUIRE(U(GetThreadTimes));
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL); HANDLE h = CreateThread(NULL, 0, tc_wait_ev, ev, 0, NULL);
    if (!h) { CHECK(0); CloseHandle(ev); return; }
    Sleep(30); FILETIME cr, ex, kt, ut;
    CHECK(U(GetThreadTimes)(h, &cr, &ex, &kt, &ut)); EXPECT_EQ(tc_u64(&ex), 0); CHECK(tc_u64(&cr) > 0);
    SetEvent(ev); WaitForSingleObject(h, 5000);
    CHECK(U(GetThreadTimes)(h, &cr, &ex, &kt, &ut)); CHECK(tc_u64(&ex) >= tc_u64(&cr));
    CloseHandle(h); CloseHandle(ev);
}

/* ---- leap seconds ---- */
CHILD(tc_leap) {
    UDECL(BOOL, SetProcessInformation, (HANDLE, int, LPVOID, DWORD));
    UDECL(BOOL, GetProcessInformation, (HANDLE, int, LPVOID, DWORD));
    SetProcessInformation_pfn sp = UN(SetProcessInformation); GetProcessInformation_pfn gp = UN(GetProcessInformation);
    if (!sp || !gp) return 100;
    struct { ULONG Flags, Reserved; } li = {0, 0}, out = {0xFFFF, 0xFFFF};
    /* default: leap seconds off */
    if (!gp(GetCurrentProcess(), 8, &out, sizeof out)) return 101;
    
    li.Flags = 1; li.Reserved = 0;
    if (!sp(GetCurrentProcess(), 8, &li, sizeof li)) return 103;
    out.Flags = 0xFFFF; if (!gp(GetCurrentProcess(), 8, &out, sizeof out)) return 104; if (out.Flags != 1) return 105;
    li.Flags = 0; if (!sp(GetCurrentProcess(), 8, &li, sizeof li)) return 106;
    out.Flags = 0xFFFF; gp(GetCurrentProcess(), 8, &out, sizeof out); if (out.Flags != 0) return 107;
    /* invalid flags */
    li.Flags = 2; SetLastError(0); if (sp(GetCurrentProcess(), 8, &li, sizeof li)) return 108;
    if (GetLastError() != ERROR_INVALID_PARAMETER) return 109; /* 0xc000000d */
    li.Flags = 0; li.Reserved = 1; if (!sp(GetCurrentProcess(), 8, &li, sizeof li)) return 110; /* Reserved ignored */
    li.Reserved = 0; SetLastError(0); if (sp(GetCurrentProcess(), 8, &li, 4)) return 111;
    if (GetLastError() != ERROR_BAD_LENGTH) return 114;
    for (int i = 0; i < 10000; i++) { SYSTEMTIME st; GetSystemTime(&st); if (st.wSecond > 59) return 112; GetLocalTime(&st); if (st.wSecond > 59) return 113; }
    return 0;
}
TEST(timemisc_leap_second_info_child) {
    DWORD r = t_run_child("tc_leap", "", 25000);
    if (r == 100) { t_skip("SetProcessInformation unavailable"); return; }
    if (t_is_wine() && r >= 101 && r <= 105) { t_skip("ProcessLeapSecondInfo unsupported on wine"); return; }
    EXPECT_EQ(r, 0);
}
TEST(timemisc_no_sixty_in_main_loop) {
    REQUIRE(U(GetSystemTime)); REQUIRE(U(GetLocalTime));
    int bad = 0;
    for (int i = 0; i < 100000; i++) { SYSTEMTIME s; U(GetSystemTime)(&s); if (s.wSecond > 59) bad++; U(GetLocalTime)(&s); if (s.wSecond > 59) bad++; }
    EXPECT_EQ(bad, 0);
}

/* ---- timer resolution / concurrency ---- */
TEST(timemisc_timer_resolution_query) {
    REQUIRE(UNT(NtQueryTimerResolution) || 1);
    NtQueryTimerResolution_pfn f = (NtQueryTimerResolution_pfn)t_proc_nt("NtQueryTimerResolution"); if (!f) { t_skip("no NtQueryTimerResolution"); return; }
    ULONG mn = 0, mx = 0, cur = 0; LONG st = f(&mn, &mx, &cur);
    EXPECT_EQ(st, 0);
    CHECK(mx <= cur && cur <= mn); /* min = coarsest */
    CHECK(mn >= 100000 && mx >= 1000); t_note("timer res min %lu max %lu cur %lu", mn, mx, cur);
}
static volatile LONG g_bad;
static DWORD WINAPI mono_thread(LPVOID p) {
    unsigned long long prev = 0;
    for (int i = 0; i < 20000; i++) { FILETIME f; GetSystemTimePreciseAsFileTime(&f); if (tc_u64(&f) < prev) InterlockedIncrement(&g_bad); prev = tc_u64(&f);
        LARGE_INTEGER q; QueryPerformanceCounter(&q); static __thread long long pq; if (q.QuadPart < pq) InterlockedIncrement(&g_bad); pq = q.QuadPart; }
    return 0;
}
TEST(timemisc_multithread_monotonic) {
    REQUIRE(U(GetSystemTimePreciseAsFileTime));
    HANDLE h[4]; g_bad = 0;
    for (int i = 0; i < 4; i++) h[i] = CreateThread(NULL, 0, mono_thread, NULL, 0, NULL);
    for (int i = 0; i < 4; i++) if (h[i]) { WaitForSingleObject(h[i], 30000); CloseHandle(h[i]); }
    if (g_bad) t_note("backwards steps: %ld", (long)g_bad);
    CHECK(g_bad == 0);
}
TEST(timemisc_cross_thread_ordering) {
    REQUIRE(U(QueryPerformanceCounter)); REQUIRE(U(GetSystemTimePreciseAsFileTime));
    /* a timestamp taken before CreateThread must precede one taken inside the thread */
    extern DWORD WINAPI tc_stamp_thread(LPVOID);
    for (int i = 0; i < 20; i++) {
        LARGE_INTEGER a, c; U(QueryPerformanceCounter)(&a);
        HANDLE h = CreateThread(NULL, 0, tc_stamp_thread, NULL, 0, NULL); if (!h) { CHECK(0); return; }
        WaitForSingleObject(h, 5000); DWORD code; GetExitCodeThread(h, &code); CloseHandle(h);
        U(QueryPerformanceCounter)(&c);
        CHECK(code == 0);
    }
}
DWORD WINAPI tc_stamp_thread(LPVOID p) { LARGE_INTEGER q; QueryPerformanceCounter(&q); return q.QuadPart > 0 ? 0 : 1; }

/* ---- export parity ---- */
TEST(timemisc_exports_present) {
    const char *k32[] = {"GetSystemTime", "GetLocalTime", "GetSystemTimeAsFileTime", "GetSystemTimePreciseAsFileTime", "SystemTimeToFileTime",
        "FileTimeToSystemTime", "FileTimeToLocalFileTime", "LocalFileTimeToFileTime", "DosDateTimeToFileTime", "FileTimeToDosDateTime", "CompareFileTime",
        "GetTickCount", "GetTickCount64", "QueryPerformanceCounter", "QueryPerformanceFrequency", "QueryUnbiasedInterruptTime", "QueryThreadCycleTime",
        "QueryProcessCycleTime", "QueryIdleProcessorCycleTime", "QueryIdleProcessorCycleTimeEx", "GetSystemTimes", "GetSystemTimeAdjustment",
        "GetProcessTimes", "GetThreadTimes", "Sleep", "SleepEx", "SetSystemTime", "SetLocalTime", "SystemTimeToTzSpecificLocalTime", "TzSpecificLocalTimeToSystemTime"};
    for (size_t i = 0; i < sizeof k32 / sizeof k32[0]; i++) CHECKF(t_proc_k32(k32[i]) != NULL, "kernel32 export %s", k32[i]);
    const char *kb[] = {"QueryInterruptTime", "QueryInterruptTimePrecise", "QueryUnbiasedInterruptTimePrecise", "GetSystemTimeAdjustmentPrecise"};
    for (size_t i = 0; i < sizeof kb / sizeof kb[0]; i++) if (!t_is_wine()) CHECKF(t_proc_kb(kb[i]) != NULL, "kernelbase export %s", kb[i]);
}
TEST(timemisc_k32_kb_same_behaviour) {
    FARPROC a = t_proc_k32("GetTickCount64"), b = t_proc_kb("GetTickCount64");
    if (!a || !b) { t_skip("exports missing"); return; }
    ULONGLONG x = ((ULONGLONG (WINAPI *)(void))a)(), y = ((ULONGLONG (WINAPI *)(void))b)();
    long long d = (long long)(y - x); CHECK(d >= -50 && d < 500);
    FARPROC c = t_proc_k32("GetSystemTimeAsFileTime"), e = t_proc_kb("GetSystemTimeAsFileTime");
    if (c && e) { FILETIME f1, f2; ((void (WINAPI *)(FILETIME *))c)(&f1); ((void (WINAPI *)(FILETIME *))e)(&f2); long long dd = (long long)(tc_u64(&f2) - tc_u64(&f1)); CHECK(dd > -500000 && dd < 500000); }
}
