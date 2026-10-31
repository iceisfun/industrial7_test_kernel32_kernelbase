/* gap24: calendar leap helpers, CALDATETIME validation, local file/system time conversion. */
#include "t.h"
#define PK(name) ((name##_pfn)t_proc_k32(#name))
#define PB(name) ((name##_pfn)t_proc_kb(#name))

UDECL(BOOL, IsCalendarLeapYear, (DWORD cal, UINT year, UINT era));
UDECL(BOOL, IsCalendarLeapMonth, (DWORD cal, UINT month, UINT year, UINT era));
UDECL(BOOL, IsCalendarLeapDay, (DWORD cal, UINT day, UINT month, UINT year, UINT era));
UDECL(BOOL, IsValidCalDateTime, (const void *dt, ULONGLONG v));

static const DWORD greg_cals[] = {1, 2, 9, 10, 11, 12};   /* CAL_GREGORIAN + localized variants */

TEST(gap24_leapyear_gregorian_table) {
    IsCalendarLeapYear_pfn f = PK(IsCalendarLeapYear); REQUIRE(f);
    static const struct { UINT y; int leap; } t[] = {
        {1,0},{4,1},{100,0},{400,1},{1600,1},{1700,0},{1800,0},{1900,0},{1996,1},{1999,0},{2000,1},
        {2001,0},{2004,1},{2020,1},{2021,0},{2023,0},{2024,1},{2100,0},{2400,1},{9996,1},{9999,0}};
    for (size_t c = 0; c < sizeof greg_cals / sizeof *greg_cals; c++)
        for (size_t i = 0; i < sizeof t / sizeof *t; i++) {
            SetLastError(0xdeadbeef);
            BOOL r = f(greg_cals[c], t[i].y, 1);
            CHECKF((r != 0) == t[i].leap, "cal %lu year %u -> %d", greg_cals[c], t[i].y, r);
            EXPECT_ERR(0);
        }
}
TEST(gap24_leapyear_range_and_era_errors) {
    IsCalendarLeapYear_pfn f = PK(IsCalendarLeapYear); REQUIRE(f);
    UINT bad[] = {0, 10000, 10001, 0x7fffffff, 0xffffffff};
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        SetLastError(0); CHECKF(!f(1, bad[i], 1), "year %u", bad[i]); EXPECT_ERR(288);
    }
    UINT eras[] = {0, 2, 3, 0xffffffff};
    for (size_t i = 0; i < sizeof eras / sizeof *eras; i++) {
        SetLastError(0); CHECKF(!f(1, 2024, eras[i]), "era %u", eras[i]); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(gap24_leapyear_bad_calendar_ids) {
    IsCalendarLeapYear_pfn f = PK(IsCalendarLeapYear); REQUIRE(f);
    DWORD ids[] = {0, 13, 14, 15, 20, 21, 24, 100, 0xffff, 0x80000000, 0xffffffff};
    for (size_t i = 0; i < sizeof ids / sizeof *ids; i++) {
        SetLastError(0); CHECKF(!f(ids[i], 2024, 1), "cal %lu", ids[i]); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(gap24_leapyear_other_calendars_note) {
    IsCalendarLeapYear_pfn f = PK(IsCalendarLeapYear); REQUIRE(f);
    DWORD ids[] = {3, 4, 5, 6, 7, 8, 22, 23};
    for (size_t i = 0; i < sizeof ids / sizeof *ids; i++) {
        SetLastError(0); BOOL r = f(ids[i], 1445, 1);
        t_note("IsCalendarLeapYear(cal %lu, 1445, 1)=%d err %lu", ids[i], r, GetLastError());
        CHECK(r == 0 || r == 1);
    }
}
TEST(gap24_leapmonth_gregorian) {
    IsCalendarLeapMonth_pfn f = PK(IsCalendarLeapMonth); REQUIRE(f);
    for (size_t c = 0; c < sizeof greg_cals / sizeof *greg_cals; c++)
        for (UINT m = 1; m <= 12; m++) {
            SetLastError(0xdeadbeef);
            CHECKF(!f(greg_cals[c], m, 2024, 1), "cal %lu month %u never leap", greg_cals[c], m);
            EXPECT_ERR(0);
        }
    UINT badm[] = {0, 13, 14, 100, 0xffffffff};
    for (size_t i = 0; i < sizeof badm / sizeof *badm; i++) {
        SetLastError(0); CHECKF(!f(1, badm[i], 2024, 1), "month %u", badm[i]); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
    SetLastError(0); CHECK(!f(1, 1, 0, 1)); EXPECT_ERR(288);
    SetLastError(0); CHECK(!f(1, 1, 10000, 1)); EXPECT_ERR(288);
    SetLastError(0); CHECK(!f(1, 1, 2024, 2)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SetLastError(0); CHECK(!f(0, 1, 2024, 1)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SetLastError(0); CHECK(!f(77, 1, 2024, 1)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap24_leapday_gregorian) {
    IsCalendarLeapDay_pfn f = PK(IsCalendarLeapDay); REQUIRE(f);
    /* args: calendar, day, month, year, era */
    SetLastError(0xdeadbeef); CHECK(f(1, 29, 2, 2024, 1)); EXPECT_ERR(0);
    SetLastError(0xdeadbeef); CHECK(f(1, 29, 2, 2000, 1)); EXPECT_ERR(0);
    SetLastError(0xdeadbeef); CHECK(f(1, 29, 2, 1600, 1)); EXPECT_ERR(0);
    SetLastError(0xdeadbeef); CHECK(!f(1, 28, 2, 2024, 1)); EXPECT_ERR(0);
    SetLastError(0xdeadbeef); CHECK(!f(1, 28, 2, 2023, 1)); EXPECT_ERR(0);
    SetLastError(0xdeadbeef); CHECK(!f(1, 1, 3, 2024, 1)); EXPECT_ERR(0);
    SetLastError(0xdeadbeef); CHECK(!f(1, 31, 12, 2024, 1)); EXPECT_ERR(0);
    for (UINT m = 1; m <= 12; m++) { if (m == 2) continue;
        SetLastError(0xdeadbeef); CHECKF(!f(1, 29, m, 2024, 1), "29/%u", m); EXPECT_ERR(m == 2 ? 0 : 0); }
    for (size_t c = 0; c < sizeof greg_cals / sizeof *greg_cals; c++)
        CHECKF(f(greg_cals[c], 29, 2, 2024, 1), "cal %lu", greg_cals[c]);
}
TEST(gap24_leapday_invalid_dates) {
    IsCalendarLeapDay_pfn f = PK(IsCalendarLeapDay); REQUIRE(f);
    struct { DWORD cal, d, m, y, e; DWORD err; } t[] = {
        {1, 29, 2, 2023, 1, ERROR_INVALID_PARAMETER},   /* day past end of month */
        {1, 29, 2, 1900, 1, ERROR_INVALID_PARAMETER},
        {1, 29, 2, 2100, 1, ERROR_INVALID_PARAMETER},
        {1, 30, 2, 2024, 1, ERROR_INVALID_PARAMETER},
        {1, 32, 1, 2024, 1, ERROR_INVALID_PARAMETER},
        {1, 31, 4, 2024, 1, ERROR_INVALID_PARAMETER},
        {1, 0, 1, 2024, 1, ERROR_INVALID_PARAMETER},    /* day 0 */
        {1, 1, 0, 2024, 1, ERROR_INVALID_PARAMETER},    /* month 0 */
        {1, 1, 13, 2024, 1, ERROR_INVALID_PARAMETER},
        {1, 1, 1, 0, 1, 288},
        {1, 1, 1, 10000, 1, 288},
        {1, 1, 1, 2024, 0, ERROR_INVALID_PARAMETER},
        {1, 1, 1, 2024, 2, ERROR_INVALID_PARAMETER},
        {0, 1, 1, 2024, 1, ERROR_INVALID_PARAMETER},
        {13, 1, 1, 2024, 1, ERROR_INVALID_PARAMETER},
        {99, 1, 1, 2024, 1, ERROR_INVALID_PARAMETER},
        {0xffffffff, 1, 1, 2024, 1, ERROR_INVALID_PARAMETER},
    };
    for (size_t i = 0; i < sizeof t / sizeof *t; i++) {
        SetLastError(0xdeadbeef);
        BOOL r = f(t[i].cal, t[i].d, t[i].m, t[i].y, t[i].e);
        CHECKF(!r, "row %zu returned %d", i, r);
        CHECKF(GetLastError() == t[i].err || (t[i].d == 0 && r == 0), "row %zu err %lu want %lu", i, GetLastError(), t[i].err);
    }
}
TEST(gap24_leapday_all_days_consistent_with_leapyear) {
    IsCalendarLeapDay_pfn d = PK(IsCalendarLeapDay); IsCalendarLeapYear_pfn y = PK(IsCalendarLeapYear);
    REQUIRE(d); REQUIRE(y);
    for (UINT yr = 1; yr <= 9999; yr += 7) {
        BOOL leap = y(1, yr, 1);
        BOOL ld = d(1, 29, 2, yr, 1);
        CHECKF(ld == leap, "year %u", yr);
        if (!leap) { SetLastError(0); CHECKF(!d(1, 29, 2, yr, 1), "y%u", yr); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
    }
}

TEST(gap24_validcaldatetime_null_args) {
    IsValidCalDateTime_pfn f = PK(IsValidCalDateTime); REQUIRE(f);
    BYTE buf[64]; memset(buf, 0, sizeof buf);
    SetLastError(0); CHECK(!f(NULL, 1)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SetLastError(0); CHECK(!f(NULL, 0)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SetLastError(0); CHECK(!f(buf, 0)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap24_validcaldatetime_bad_calendar) {
    IsValidCalDateTime_pfn f = PK(IsValidCalDateTime); REQUIRE(f);
    DWORD ids[] = {13, 14, 20, 21, 24, 100, 0x1000, 0xffffffff};
    for (size_t i = 0; i < sizeof ids / sizeof *ids; i++) {
        DWORD buf[16]; memset(buf, 0, sizeof buf); buf[0] = ids[i];
        SetLastError(0); CHECKF(!f(buf, 1), "cal %lu", ids[i]); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}

/* ---------- LocalFileTime <-> LocalSystemTime ---------- */
typedef BOOL (WINAPI *LFT2LST_t)(const TIME_ZONE_INFORMATION *, const FILETIME *, SYSTEMTIME *);
typedef BOOL (WINAPI *LST2LFT_t)(const TIME_ZONE_INFORMATION *, const SYSTEMTIME *, FILETIME *);

static void utc_tz(TIME_ZONE_INFORMATION *tz) { memset(tz, 0, sizeof *tz); }

TEST(gap24_localfiletime_utc_zone_matches_filetimetosystemtime) {
    LFT2LST_t f = (LFT2LST_t)t_proc("LocalFileTimeToLocalSystemTime"); REQUIRE(f);
    TIME_ZONE_INFORMATION tz; utc_tz(&tz);
    SYSTEMTIME in[] = {
        {1970,1,4,1,0,0,0,0},{2000,2,2,29,23,59,59,999},{2024,12,2,31,23,59,59,0},{1601,1,1,1,0,0,0,0},
        {2038,1,2,19,3,14,7,0},{2100,3,1,1,12,0,0,500},{1999,12,5,31,23,59,59,999},{2016,12,6,31,23,59,59,0}};
    for (size_t i = 0; i < sizeof in / sizeof *in; i++) {
        FILETIME ft; SYSTEMTIME want, got; memset(&got, 0xAA, sizeof got);
        if (!SystemTimeToFileTime(&in[i], &ft)) { t_note("scaffold failed row %zu", i); continue; }
        FileTimeToSystemTime(&ft, &want);
        BOOL r = f(&tz, &ft, &got);
        CHECKF(r, "row %zu failed err %lu", i, GetLastError());
        CHECKF(memcmp(&want, &got, sizeof want) == 0, "row %zu: got %04u-%02u-%02u %02u:%02u:%02u.%03u", i, got.wYear, got.wMonth, got.wDay, got.wHour, got.wMinute, got.wSecond, got.wMilliseconds);
    }
}
TEST(gap24_localsystemtime_utc_zone_roundtrip) {
    LFT2LST_t g = (LFT2LST_t)t_proc("LocalFileTimeToLocalSystemTime");
    LST2LFT_t f = (LST2LFT_t)t_proc("LocalSystemTimeToLocalFileTime"); REQUIRE(f); REQUIRE(g);
    TIME_ZONE_INFORMATION tz; utc_tz(&tz);
    SYSTEMTIME in[] = {{2001,9,0,9,1,46,40,0},{2024,2,4,29,12,34,56,789},{1980,1,2,1,0,0,0,1},{2262,4,0,11,23,47,16,854}};
    for (size_t i = 0; i < sizeof in / sizeof *in; i++) {
        FILETIME a, b; SYSTEMTIME back; SystemTimeToFileTime(&in[i], &a);
        BOOL r = f(&tz, &in[i], &b);
        CHECKF(r, "row %zu err %lu", i, GetLastError());
        CHECKF(a.dwLowDateTime == b.dwLowDateTime && a.dwHighDateTime == b.dwHighDateTime, "row %zu ft mismatch", i);
        CHECK(g(&tz, &b, &back));
        CHECKF(back.wYear == in[i].wYear && back.wMonth == in[i].wMonth && back.wDay == in[i].wDay &&
               back.wHour == in[i].wHour && back.wMinute == in[i].wMinute && back.wSecond == in[i].wSecond &&
               back.wMilliseconds == in[i].wMilliseconds, "round trip row %zu", i);
    }
}
TEST(gap24_localsystemtime_leap_second_clamped) {
    LST2LFT_t f = (LST2LFT_t)t_proc("LocalSystemTimeToLocalFileTime"); REQUIRE(f);
    TIME_ZONE_INFORMATION tz; utc_tz(&tz);
    SYSTEMTIME s59 = {2016,12,0,31,23,59,59,0}, s60 = s59; s60.wSecond = 60;
    FILETIME a, b;
    CHECK(f(&tz, &s59, &a));
    BOOL r = f(&tz, &s60, &b);
    CHECKF(r, "second=60 accepted, err %lu", GetLastError());
    CHECK(a.dwLowDateTime == b.dwLowDateTime && a.dwHighDateTime == b.dwHighDateTime);
}
TEST(gap24_localsystemtime_invalid_fields_fail) {
    LST2LFT_t f = (LST2LFT_t)t_proc("LocalSystemTimeToLocalFileTime"); REQUIRE(f);
    TIME_ZONE_INFORMATION tz; utc_tz(&tz);
    SYSTEMTIME bad[] = {
        {2024,13,0,1,0,0,0,0},{2024,0,0,1,0,0,0,0},{2024,1,0,0,0,0,0,0},{2024,2,0,30,0,0,0,0},
        {2024,1,0,1,24,0,0,0},{2024,1,0,1,0,60,0,0},{2024,1,0,1,0,0,61,0},{2024,1,0,1,0,0,0,1000},
        {1600,1,0,1,0,0,0,0},{30828,1,0,1,0,0,0,0}};
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        FILETIME ft = {1, 2};
        SetLastError(0);
        BOOL r = f(&tz, &bad[i], &ft);
        if (r) t_note("row %zu unexpectedly accepted", i);
        CHECKF(!r, "row %zu must fail", i);
        if (!r) t_note("row %zu err %lu", i, GetLastError());
    }
}
TEST(gap24_localfiletime_bias_applied) {
    LFT2LST_t g = (LFT2LST_t)t_proc("LocalFileTimeToLocalSystemTime");
    LST2LFT_t f = (LST2LFT_t)t_proc("LocalSystemTimeToLocalFileTime"); REQUIRE(f); REQUIRE(g);
    TIME_ZONE_INFORMATION tz; utc_tz(&tz); tz.Bias = 120;  /* UTC-2, no DST */
    SYSTEMTIME s = {2024,6,0,15,12,0,0,0}; FILETIME a, b;
    SystemTimeToFileTime(&s, &a);
    CHECK(f(&tz, &s, &b));
    ULONGLONG ua = ((ULONGLONG)a.dwHighDateTime << 32) | a.dwLowDateTime, ub = ((ULONGLONG)b.dwHighDateTime << 32) | b.dwLowDateTime;
    LONGLONG diff = (LONGLONG)(ub - ua) / 10000000LL;
    t_note("bias 120: LocalSystemTimeToLocalFileTime delta = %lld s", diff);
    CHECK(diff == 0 || diff == 7200 || diff == -7200);
    SYSTEMTIME out; CHECK(g(&tz, &a, &out));
    t_note("bias 120: LocalFileTimeToLocalSystemTime hour %u", out.wHour);
}
TEST(gap24_localfiletime_dst_selection_note) {
    LFT2LST_t g = (LFT2LST_t)t_proc("LocalFileTimeToLocalSystemTime"); REQUIRE(g);
    TIME_ZONE_INFORMATION tz; memset(&tz, 0, sizeof tz);
    tz.Bias = 300; tz.StandardBias = 0; tz.DaylightBias = -60;
    tz.StandardDate = (SYSTEMTIME){0,11,0,1,2,0,0,0};
    tz.DaylightDate = (SYSTEMTIME){0,3,0,2,2,0,0,0};
    SYSTEMTIME summer = {2024,7,0,4,12,0,0,0}, winter = {2024,1,0,4,12,0,0,0};
    FILETIME fs, fw; SYSTEMTIME os, ow;
    SystemTimeToFileTime(&summer, &fs); SystemTimeToFileTime(&winter, &fw);
    BOOL r1 = g(&tz, &fs, &os), r2 = g(&tz, &fw, &ow);
    CHECK(r1); CHECK(r2);
    t_note("DST zone: summer hour %u winter hour %u", os.wHour, ow.wHour);
    if (r1 && r2) CHECK(os.wDay == 4 && ow.wDay == 4);
}
TEST(gap24_localfiletime_repeat_deterministic) {
    LFT2LST_t f = (LFT2LST_t)t_proc("LocalFileTimeToLocalSystemTime"); REQUIRE(f);
    TIME_ZONE_INFORMATION tz; utc_tz(&tz);
    FILETIME ft = {0xd53e8000, 0x019db1de};  /* 1970-01-01 */
    SYSTEMTIME first, nx;
    CHECK(f(&tz, &ft, &first));
    for (int i = 0; i < 200; i++) { CHECK(f(&tz, &ft, &nx)); CHECK(!memcmp(&first, &nx, sizeof nx)); }
    CHECK(first.wYear == 1970 && first.wMonth == 1 && first.wDay == 1 && first.wDayOfWeek == 4);
}
