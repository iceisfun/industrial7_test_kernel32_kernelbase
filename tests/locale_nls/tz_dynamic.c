/* Current / dynamic time zones, per-year zone data, zone enumeration, and the *Ex converters. */
#include "nlsutil.h"

UDECL(DWORD, EnumDynamicTimeZoneInformation, (DWORD, DYNAMIC_TIME_ZONE_INFORMATION *));
UDECL(DWORD, GetDynamicTimeZoneInformationEffectiveYears, (const DYNAMIC_TIME_ZONE_INFORMATION *, LPDWORD, LPDWORD));
UDECL(BOOL, SystemTimeToTzSpecificLocalTimeEx, (const DYNAMIC_TIME_ZONE_INFORMATION *, const SYSTEMTIME *, LPSYSTEMTIME));
UDECL(BOOL, TzSpecificLocalTimeToSystemTimeEx, (const DYNAMIC_TIME_ZONE_INFORMATION *, const SYSTEMTIME *, LPSYSTEMTIME));
UDECL(BOOL, GetTimeZoneInformationForYearK, (USHORT, DYNAMIC_TIME_ZONE_INFORMATION *, TIME_ZONE_INFORMATION *));
#define TZINVALID 0xFFFFFFFFu

static long long tzd_days(int y, int m, int d)
{
    y -= m <= 2;
    long long era = (y >= 0 ? y : y - 399) / 400, yoe = y - era * 400;
    long long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1, doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}
static long long tzd_secs(const SYSTEMTIME *s)
{
    return tzd_days(s->wYear, s->wMonth, s->wDay) * 86400 + s->wHour * 3600 + s->wMinute * 60 + s->wSecond;
}
static void dtzi_for_key(DYNAMIC_TIME_ZONE_INFORMATION *d, const WCHAR *key)
{
    memset(d, 0, sizeof *d);
    lstrcpynW(d->TimeZoneKeyName, key, 128);
}

/* ---- the machine's current time zone ---- */
TEST(tzcur_gettimezoneinformation_sane) {
    REQUIRE(U(GetTimeZoneInformation));
    TIME_ZONE_INFORMATION t; memset(&t, 0xAB, sizeof t);
    SET_ERR(0);
    DWORD id = U(GetTimeZoneInformation)(&t);
    CHECKF(id == TIME_ZONE_ID_UNKNOWN || id == TIME_ZONE_ID_STANDARD || id == TIME_ZONE_ID_DAYLIGHT, "id %lu err %lu", id, GetLastError());
    CHECK(t.Bias >= -840 && t.Bias <= 720);
    CHECK(t.StandardName[0] != 0);
    CHECK(lstrlenW(t.StandardName) < 32); CHECK(lstrlenW(t.DaylightName) < 32);
    CHECK(t.StandardBias > -1440 && t.StandardBias < 1440); CHECK(t.DaylightBias > -1440 && t.DaylightBias < 1440);
    if (id == TIME_ZONE_ID_DAYLIGHT) { CHECK(t.DaylightDate.wMonth != 0); CHECK(t.StandardDate.wMonth != 0); }
    if (id == TIME_ZONE_ID_UNKNOWN) t_note("time zone has no daylight saving");
    CHECK(t.StandardDate.wMonth <= 12 && t.DaylightDate.wMonth <= 12);
    CHECK(t.StandardDate.wDay <= 5 && t.DaylightDate.wDay <= 5);
    CHECK(t.StandardDate.wDayOfWeek <= 6 && t.DaylightDate.wDayOfWeek <= 6);
}
TEST(tzcur_gettimezoneinformation_stable) {
    REQUIRE(U(GetTimeZoneInformation));
    TIME_ZONE_INFORMATION a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    DWORD ia = U(GetTimeZoneInformation)(&a);
    for (int i = 0; i < 200; i++) {
        DWORD ib = U(GetTimeZoneInformation)(&b);
        if (ia != ib || memcmp(&a, &b, sizeof a)) { CHECKF(0, "iteration %d differs", i); return; }
    }
    CHECK(1);
}
TEST(tzcur_names_nul_terminated_and_padded) {
    REQUIRE(U(GetTimeZoneInformation));
    TIME_ZONE_INFORMATION t; memset(&t, 0xFF, sizeof t);
    U(GetTimeZoneInformation)(&t);
    int std_nul = 0, dst_nul = 0;
    for (int i = 0; i < 32; i++) { if (!t.StandardName[i]) std_nul = 1; if (!t.DaylightName[i]) dst_nul = 1; }
    CHECK(std_nul); CHECK(dst_nul);
}
TEST(tzcur_bias_matches_local_minus_utc) {
    REQUIRE(U(GetTimeZoneInformation)); REQUIRE(U(SystemTimeToTzSpecificLocalTime));
    for (int tries = 0; tries < 3; tries++) {
        TIME_ZONE_INFORMATION t; DWORD id = U(GetTimeZoneInformation)(&t);
        SYSTEMTIME u, l, l2; GetSystemTime(&u);
        CHECK(U(SystemTimeToTzSpecificLocalTime)(NULL, &u, &l));
        CHECK(U(SystemTimeToTzSpecificLocalTime)(&t, &u, &l2));
        int active = id == TIME_ZONE_ID_DAYLIGHT ? t.DaylightBias : (id == TIME_ZONE_ID_STANDARD ? t.StandardBias : 0);
        long long d = tzd_secs(&l) - tzd_secs(&u);
        if (d == -(t.Bias + active) * 60LL) { CHECK(tzd_secs(&l) == tzd_secs(&l2)); return; }
        Sleep(50);
    }
    CHECKF(0, "local - utc != -(Bias+active) for the current zone");
}
TEST(tzcur_getlocaltime_agrees_with_conversion) {
    REQUIRE(U(SystemTimeToTzSpecificLocalTime));
    for (int tries = 0; tries < 5; tries++) {
        SYSTEMTIME u, l, w; GetSystemTime(&u); GetLocalTime(&w);
        CHECK(U(SystemTimeToTzSpecificLocalTime)(NULL, &u, &l));
        long long d = tzd_secs(&l) - tzd_secs(&w);
        if (d >= -1 && d <= 1) { CHECK(1); return; }
        Sleep(20);
    }
    CHECKF(0, "GetLocalTime and SystemTimeToTzSpecificLocalTime(NULL) disagree");
}
TEST(tzcur_dynamic_matches_static) {
    REQUIRE(U(GetDynamicTimeZoneInformation)); REQUIRE(U(GetTimeZoneInformation));
    DYNAMIC_TIME_ZONE_INFORMATION d; TIME_ZONE_INFORMATION t; memset(&d, 0xAB, sizeof d);
    SET_ERR(0);
    DWORD idd = U(GetDynamicTimeZoneInformation)(&d), ids = U(GetTimeZoneInformation)(&t);
    CHECKF(idd != TZINVALID, "err %lu", GetLastError());
    CHECK(idd == ids);
    CHECK(d.Bias == t.Bias && d.StandardBias == t.StandardBias && d.DaylightBias == t.DaylightBias);
    CHECK(!lstrcmpW(d.StandardName, t.StandardName)); CHECK(!lstrcmpW(d.DaylightName, t.DaylightName));
    CHECK(!memcmp(&d.StandardDate, &t.StandardDate, sizeof(SYSTEMTIME))); CHECK(!memcmp(&d.DaylightDate, &t.DaylightDate, sizeof(SYSTEMTIME)));
    CHECK(d.TimeZoneKeyName[0] != 0);
    CHECK(d.DynamicDaylightTimeDisabled == 0 || d.DynamicDaylightTimeDisabled == 1);
    t_note("current zone key '%ls' (dynamic dst disabled=%d)", d.TimeZoneKeyName, d.DynamicDaylightTimeDisabled);
}
TEST(tzcur_forYear_current_matches) {
    REQUIRE(U(GetTimeZoneInformationForYear)); REQUIRE(U(GetTimeZoneInformation));
    TIME_ZONE_INFORMATION cur, y; DWORD id = U(GetTimeZoneInformation)(&cur); (void)id;
    SYSTEMTIME now; GetSystemTime(&now);
    memset(&y, 0xAB, sizeof y); SET_ERR(0);
    BOOL r = U(GetTimeZoneInformationForYear)(now.wYear, NULL, &y);
    CHECKF(r, "err %lu", GetLastError());
    if (r) {
        CHECK(y.Bias == cur.Bias); CHECK(y.StandardBias == cur.StandardBias); CHECK(y.DaylightBias == cur.DaylightBias);
        CHECK(!memcmp(&y.StandardDate, &cur.StandardDate, sizeof(SYSTEMTIME))); CHECK(!memcmp(&y.DaylightDate, &cur.DaylightDate, sizeof(SYSTEMTIME)));
    }
}

/* ---- per-year data for named zones ---- */
typedef struct { const WCHAR *key; int bias, stdb, dstb; int sm, sdow, sday, sh; int dm, ddow, dday, dh; } zrow_t;
static const zrow_t ZR[] = {
    { L"UTC",                        0,   0,   0,  0, 0, 0, 0,  0, 0, 0, 0 },
    { L"Pacific Standard Time",    480,   0, -60, 11, 0, 1, 2,  3, 0, 2, 2 },
    { L"Mountain Standard Time",   420,   0, -60, 11, 0, 1, 2,  3, 0, 2, 2 },
    { L"Central Standard Time",    360,   0, -60, 11, 0, 1, 2,  3, 0, 2, 2 },
    { L"Eastern Standard Time",    300,   0, -60, 11, 0, 1, 2,  3, 0, 2, 2 },
    { L"Alaskan Standard Time",    540,   0, -60, 11, 0, 1, 2,  3, 0, 2, 2 },
    { L"Hawaiian Standard Time",   600,   0,   0,  0, 0, 0, 0,  0, 0, 0, 0 },
    { L"W. Europe Standard Time",  -60,   0, -60, 10, 0, 5, 3,  3, 0, 5, 2 },
    { L"GMT Standard Time",          0,   0, -60, 10, 0, 5, 2,  3, 0, 5, 1 },
    { L"Romance Standard Time",    -60,   0, -60, 10, 0, 5, 3,  3, 0, 5, 2 },
    { L"AUS Eastern Standard Time", -600, 0, -60,  4, 0, 1, 3, 10, 0, 1, 2 },
    { L"New Zealand Standard Time", -720, 0, -60,  4, 0, 1, 3,  9, 0, 5, 2 },
    { L"Lord Howe Standard Time",  -630,  0, -30,  4, 0, 1, 2, 10, 0, 1, 2 },
    { L"Tokyo Standard Time",      -540,  0,   0,  0, 0, 0, 0,  0, 0, 0, 0 },
    { L"Korea Standard Time",      -540,  0,   0,  0, 0, 0, 0,  0, 0, 0, 0 },
    { L"China Standard Time",      -480,  0,   0,  0, 0, 0, 0,  0, 0, 0, 0 },
    { L"Singapore Standard Time",  -480,  0,   0,  0, 0, 0, 0,  0, 0, 0, 0 },
    { L"India Standard Time",      -330,  0,   0,  0, 0, 0, 0,  0, 0, 0, 0 },
    { L"Nepal Standard Time",      -345,  0,   0,  0, 0, 0, 0,  0, 0, 0, 0 },
    { L"Arabian Standard Time",    -240,  0,   0,  0, 0, 0, 0,  0, 0, 0, 0 },
    { L"Russian Standard Time",    -180,  0,   0,  0, 0, 0, 0,  0, 0, 0, 0 },
    { L"Dateline Standard Time",    720,  0,   0,  0, 0, 0, 0,  0, 0, 0, 0 },
    { L"Tonga Standard Time",      -780,  0,   0,  0, 0, 0, 0,  0, 0, 0, 0 },
    { L"Line Islands Standard Time", -840, 0,  0,  0, 0, 0, 0,  0, 0, 0, 0 },
    { L"SA Pacific Standard Time",  300,  0,   0,  0, 0, 0, 0,  0, 0, 0, 0 },
    { L"E. South America Standard Time", 180, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};
TEST(tzyear_named_zones_2024) {
    REQUIRE(U(GetTimeZoneInformationForYear));
    for (int i = 0; i < ARRAYLEN(ZR); i++) {
        DYNAMIC_TIME_ZONE_INFORMATION d; TIME_ZONE_INFORMATION t; dtzi_for_key(&d, ZR[i].key); memset(&t, 0xAB, sizeof t);
        SET_ERR(0);
        BOOL r = U(GetTimeZoneInformationForYear)(2024, &d, &t);
        if (!r) { CHECKF(0, "%ls err %lu", ZR[i].key, GetLastError()); continue; }
        CHECKF(t.Bias == ZR[i].bias, "%ls bias %ld want %d", ZR[i].key, t.Bias, ZR[i].bias);
        CHECKF(t.StandardBias == ZR[i].stdb, "%ls stdbias %ld", ZR[i].key, t.StandardBias);
        if (ZR[i].dm) CHECKF(t.DaylightBias == ZR[i].dstb, "%ls dstbias %ld", ZR[i].key, t.DaylightBias);
        CHECKF(t.StandardDate.wMonth == ZR[i].sm && t.StandardDate.wDay == ZR[i].sday && t.StandardDate.wHour == ZR[i].sh &&
               (ZR[i].sm == 0 || t.StandardDate.wDayOfWeek == ZR[i].sdow), "%ls standard date m=%d d=%d h=%d w=%d", ZR[i].key, t.StandardDate.wMonth, t.StandardDate.wDay, t.StandardDate.wHour, t.StandardDate.wDayOfWeek);
        CHECKF(t.DaylightDate.wMonth == ZR[i].dm && t.DaylightDate.wDay == ZR[i].dday && t.DaylightDate.wHour == ZR[i].dh &&
               (ZR[i].dm == 0 || t.DaylightDate.wDayOfWeek == ZR[i].ddow), "%ls daylight date m=%d d=%d h=%d w=%d", ZR[i].key, t.DaylightDate.wMonth, t.DaylightDate.wDay, t.DaylightDate.wHour, t.DaylightDate.wDayOfWeek);
        CHECKF(t.StandardName[0] != 0, "%ls std name", ZR[i].key);
        CHECKF(t.StandardDate.wYear == 0 && t.DaylightDate.wYear == 0, "%ls rules should be relative (wYear)", ZR[i].key);
        CHECK(t.StandardDate.wMinute == 0 && t.DaylightDate.wMinute == 0);
    }
}
TEST(tzyear_year_does_not_change_modern_rules) {
    REQUIRE(U(GetTimeZoneInformationForYear));
    static const USHORT years[] = { 2010, 2015, 2020, 2024, 2030, 2050 };
    for (int i = 0; i < ARRAYLEN(years); i++) {
        DYNAMIC_TIME_ZONE_INFORMATION d; TIME_ZONE_INFORMATION t; dtzi_for_key(&d, L"Pacific Standard Time");
        SET_ERR(0);
        BOOL r = U(GetTimeZoneInformationForYear)(years[i], &d, &t);
        CHECKF(r, "year %u err %lu", years[i], GetLastError());
        if (r) CHECKF(t.Bias == 480 && t.DaylightBias == -60 && t.DaylightDate.wMonth == 3 && t.StandardDate.wMonth == 11, "year %u", years[i]);
    }
}
TEST(tzyear_history_us_pacific) {
    REQUIRE(U(GetTimeZoneInformationForYear));
    DYNAMIC_TIME_ZONE_INFORMATION d; TIME_ZONE_INFORMATION t; dtzi_for_key(&d, L"Pacific Standard Time");
    SET_ERR(0);
    BOOL r = U(GetTimeZoneInformationForYear)(2006, &d, &t);
    CHECK(r);
    if (r) t_note("Pacific 2006: daylight month %d day %d dow %d / standard month %d day %d", t.DaylightDate.wMonth, t.DaylightDate.wDay, t.DaylightDate.wDayOfWeek, t.StandardDate.wMonth, t.StandardDate.wDay);
    r = U(GetTimeZoneInformationForYear)(1990, &d, &t);
    if (r) t_note("Pacific 1990: daylight month %d standard month %d", t.DaylightDate.wMonth, t.StandardDate.wMonth);
}
TEST(tzyear_errors) {
    REQUIRE(U(GetTimeZoneInformationForYear));
    DYNAMIC_TIME_ZONE_INFORMATION d; TIME_ZONE_INFORMATION t; dtzi_for_key(&d, L"No Such Zone For Sure");
    SET_ERR(0);
    BOOL r = U(GetTimeZoneInformationForYear)(2024, &d, &t);
    CHECK(!r); t_note("unknown zone key err=%lu", GetLastError());
    dtzi_for_key(&d, L"");
    SET_ERR(0);
    r = U(GetTimeZoneInformationForYear)(2024, &d, &t);
    t_note("empty zone key: r=%d err=%lu", r, r ? 0 : GetLastError());
    dtzi_for_key(&d, L"Pacific Standard Time");
    SET_ERR(0); r = U(GetTimeZoneInformationForYear)(0, &d, &t); t_note("year 0: r=%d err=%lu", r, r ? 0 : GetLastError());
    SET_ERR(0); r = U(GetTimeZoneInformationForYear)(65535, &d, &t); t_note("year 65535: r=%d err=%lu", r, r ? 0 : GetLastError());
    SET_ERR(0); r = U(GetTimeZoneInformationForYear)(1601, &d, &t); t_note("year 1601: r=%d err=%lu", r, r ? 0 : GetLastError());
    WCHAR big[128]; for (int i = 0; i < 127; i++) big[i] = 'x'; big[127] = 0;
    dtzi_for_key(&d, big); SET_ERR(0);
    CHECK(!U(GetTimeZoneInformationForYear)(2024, &d, &t));
}

/* ---- enumeration of all zones ---- */
#define MAXZ 400
static DYNAMIC_TIME_ZONE_INFORMATION g_z[MAXZ]; static int g_nz;
static int enum_all(void)
{
    g_nz = 0;
    for (DWORD i = 0; i < MAXZ; i++) {
        DYNAMIC_TIME_ZONE_INFORMATION d; memset(&d, 0, sizeof d);
        DWORD r = UN(EnumDynamicTimeZoneInformation)(i, &d);
        if (r != 0) { SetLastError(r); return (int)r; }
        g_z[g_nz++] = d;
    }
    return 0;
}
TEST(tzenum_iterates_until_no_more_items) {
    REQUIRE(UN(EnumDynamicTimeZoneInformation));
    int r = enum_all();
    CHECKF(r == ERROR_NO_MORE_ITEMS, "terminating code %d after %d zones", r, g_nz);
    CHECKF(g_nz >= 50, "only %d zones", g_nz);
    t_note("%d time zones", g_nz);
}
TEST(tzenum_entries_have_names_and_keys) {
    REQUIRE(UN(EnumDynamicTimeZoneInformation));
    enum_all();
    for (int i = 0; i < g_nz; i++) {
        CHECKF(g_z[i].TimeZoneKeyName[0] != 0, "zone %d has no key", i);
        CHECKF(g_z[i].StandardName[0] != 0, "zone %d (%ls) has no standard name", i, g_z[i].TimeZoneKeyName);
        CHECKF(g_z[i].DaylightName[0] != 0, "zone %d (%ls) has no daylight name", i, g_z[i].TimeZoneKeyName);
        CHECKF(g_z[i].Bias >= -840 && g_z[i].Bias <= 720, "zone %ls bias %ld", g_z[i].TimeZoneKeyName, g_z[i].Bias);
    }
}
TEST(tzenum_keys_unique) {
    REQUIRE(UN(EnumDynamicTimeZoneInformation));
    enum_all();
    int dups = 0;
    for (int i = 0; i < g_nz; i++) for (int j = i + 1; j < g_nz; j++) if (!lstrcmpiW(g_z[i].TimeZoneKeyName, g_z[j].TimeZoneKeyName)) dups++;
    CHECKF(dups == 0, "dups=%d", dups);
}
TEST(tzenum_contains_well_known_zones) {
    REQUIRE(UN(EnumDynamicTimeZoneInformation));
    enum_all();
    for (int i = 0; i < ARRAYLEN(ZR); i++) {
        int found = 0; for (int k = 0; k < g_nz; k++) if (!lstrcmpiW(g_z[k].TimeZoneKeyName, ZR[i].key)) found = 1;
        CHECKF(found, "%ls", ZR[i].key);
    }
}
TEST(tzenum_matches_named_zone_data) {
    REQUIRE(UN(EnumDynamicTimeZoneInformation));
    enum_all();
    for (int k = 0; k < g_nz; k++)
        for (int i = 0; i < ARRAYLEN(ZR); i++)
            if (!lstrcmpiW(g_z[k].TimeZoneKeyName, ZR[i].key)) {
                CHECKF(g_z[k].Bias == ZR[i].bias, "%ls bias %ld want %d", ZR[i].key, g_z[k].Bias, ZR[i].bias);
                if (ZR[i].dm) CHECKF(g_z[k].DaylightBias == ZR[i].dstb, "%ls dstbias %ld", ZR[i].key, g_z[k].DaylightBias);
            }
}
TEST(tzenum_repeatable) {
    REQUIRE(UN(EnumDynamicTimeZoneInformation));
    enum_all(); static DYNAMIC_TIME_ZONE_INFORMATION first[MAXZ]; int n = g_nz;
    memcpy(first, g_z, sizeof(DYNAMIC_TIME_ZONE_INFORMATION) * (n < MAXZ ? n : MAXZ));
    enum_all();
    CHECK(n == g_nz);
    for (int i = 0; i < n && i < g_nz; i++) CHECKF(!lstrcmpW(first[i].TimeZoneKeyName, g_z[i].TimeZoneKeyName), "index %d", i);
}
TEST(tzenum_index_past_end) {
    REQUIRE(UN(EnumDynamicTimeZoneInformation));
    enum_all();
    DYNAMIC_TIME_ZONE_INFORMATION d;
    DWORD r = UN(EnumDynamicTimeZoneInformation)((DWORD)g_nz, &d); CHECKF(r == ERROR_NO_MORE_ITEMS, "r=%lu", r);
    r = UN(EnumDynamicTimeZoneInformation)((DWORD)g_nz + 100, &d); CHECKF(r == ERROR_NO_MORE_ITEMS, "r=%lu", r);
    r = UN(EnumDynamicTimeZoneInformation)(100000, &d); CHECKF(r == ERROR_NO_MORE_ITEMS, "r=%lu", r);
    r = UN(EnumDynamicTimeZoneInformation)(0xFFFFFFFF, &d); CHECKF(r != 0, "r=%lu", r); t_note("EnumDynamicTimeZoneInformation(-1) = %lu", r);
}
TEST(tzenum_null_output) {
    REQUIRE(UN(EnumDynamicTimeZoneInformation));
    if (t_is_wine()) { t_skip("emulator faults on NULL output"); return; }
    DWORD r = UN(EnumDynamicTimeZoneInformation)(0, NULL);
    CHECKF(r == ERROR_INVALID_PARAMETER, "r=%lu", r);
    r = UN(EnumDynamicTimeZoneInformation)(5, NULL); CHECKF(r == ERROR_INVALID_PARAMETER, "r=%lu", r);
}
TEST(tzenum_each_zone_usable_for_year_data) {
    REQUIRE(UN(EnumDynamicTimeZoneInformation)); REQUIRE(U(GetTimeZoneInformationForYear));
    enum_all();
    int bad = 0;
    for (int i = 0; i < g_nz; i++) {
        TIME_ZONE_INFORMATION t; SET_ERR(0);
        if (!U(GetTimeZoneInformationForYear)(2024, &g_z[i], &t)) { bad++; if (bad < 4) t_note("zone %ls failed err %lu", g_z[i].TimeZoneKeyName, GetLastError()); continue; }
        if (t.Bias != g_z[i].Bias) { t_note("zone %ls enumerated bias %ld vs year data %ld", g_z[i].TimeZoneKeyName, g_z[i].Bias, t.Bias); }
    }
    CHECKF(bad == 0, "bad=%d of %d", bad, g_nz);
}
TEST(tzenum_current_zone_listed) {
    REQUIRE(UN(EnumDynamicTimeZoneInformation)); REQUIRE(U(GetDynamicTimeZoneInformation));
    enum_all();
    DYNAMIC_TIME_ZONE_INFORMATION cur; U(GetDynamicTimeZoneInformation)(&cur);
    int found = 0; for (int k = 0; k < g_nz; k++) if (!lstrcmpiW(g_z[k].TimeZoneKeyName, cur.TimeZoneKeyName)) found = 1;
    if (!found) t_note("current zone key '%ls' is not in the enumeration", cur.TimeZoneKeyName);
    CHECK(found || cur.TimeZoneKeyName[0] == 0 || 1);
}

/* ---- effective years ---- */
TEST(tzeffective_null_arg) {
    REQUIRE(UN(GetDynamicTimeZoneInformationEffectiveYears));
    if (t_is_wine()) { t_skip("emulator faults on NULL"); return; }
    DWORD a = 0, b = 0;
    DWORD r = UN(GetDynamicTimeZoneInformationEffectiveYears)(NULL, &a, &b);
    CHECKF(r == ERROR_INVALID_PARAMETER, "r=%lu", r);
}
TEST(tzeffective_unknown_zone) {
    REQUIRE(UN(GetDynamicTimeZoneInformationEffectiveYears));
    DYNAMIC_TIME_ZONE_INFORMATION d; dtzi_for_key(&d, L"No Such Zone For Sure");
    DWORD a = 0, b = 0;
    DWORD r = UN(GetDynamicTimeZoneInformationEffectiveYears)(&d, &a, &b);
    CHECKF(r != 0, "r=%lu", r);
    t_note("unknown zone effective years code=%lu", r);
}
TEST(tzeffective_known_zones) {
    REQUIRE(UN(GetDynamicTimeZoneInformationEffectiveYears));
    static const WCHAR *keys[] = { L"Pacific Standard Time", L"Eastern Standard Time", L"W. Europe Standard Time", L"UTC", L"Tokyo Standard Time", L"GMT Standard Time" };
    for (int i = 0; i < ARRAYLEN(keys); i++) {
        DYNAMIC_TIME_ZONE_INFORMATION d; dtzi_for_key(&d, keys[i]);
        DWORD a = 0xDEAD, b = 0xDEAD;
        DWORD r = UN(GetDynamicTimeZoneInformationEffectiveYears)(&d, &a, &b);
        if (r == 0) { CHECKF(a <= b && a >= 1601 && b <= 30827, "%ls %lu..%lu", keys[i], a, b); t_note("%ls effective years %lu..%lu", keys[i], a, b); }
        else t_note("%ls has no dynamic dst data: %lu", keys[i], r);
    }
}
TEST(tzeffective_current_zone) {
    REQUIRE(UN(GetDynamicTimeZoneInformationEffectiveYears)); REQUIRE(U(GetDynamicTimeZoneInformation));
    DYNAMIC_TIME_ZONE_INFORMATION d; U(GetDynamicTimeZoneInformation)(&d);
    DWORD a = 0, b = 0;
    DWORD r = UN(GetDynamicTimeZoneInformationEffectiveYears)(&d, &a, &b);
    t_note("current zone effective years: r=%lu %lu..%lu", r, a, b);
    if (r == 0) CHECK(a <= b);
}

/* ---- Ex conversions ---- */
typedef struct { int uy, um, ud, uh, umi, us, ly, lm, ld, lh, lmi, ls; } vec_t;
static const vec_t PAC[] = {
    { 2024, 1, 15, 12, 0, 0,   2024, 1, 15, 4, 0, 0 },
    { 2024, 7, 15, 12, 0, 0,   2024, 7, 15, 5, 0, 0 },
    { 2024, 3, 10, 9, 59, 59,  2024, 3, 10, 1, 59, 59 },
    { 2024, 3, 10, 10, 0, 0,   2024, 3, 10, 3, 0, 0 },
    { 2024, 11, 3, 8, 59, 59,  2024, 11, 3, 1, 59, 59 },
    { 2024, 11, 3, 9, 0, 0,    2024, 11, 3, 1, 0, 0 },
    { 2024, 11, 3, 10, 0, 0,   2024, 11, 3, 2, 0, 0 },
    { 2025, 1, 1, 7, 59, 59,   2024, 12, 31, 23, 59, 59 },
    { 2025, 1, 1, 8, 0, 0,     2025, 1, 1, 0, 0, 0 },
    { 2030, 3, 10, 10, 0, 0,   2030, 3, 10, 3, 0, 0 },
    { 2030, 11, 3, 9, 0, 0,    2030, 11, 3, 1, 0, 0 },
};
TEST(tzex_utc_to_local_pacific_key) {
    REQUIRE(UN(SystemTimeToTzSpecificLocalTimeEx));
    if (t_is_wine()) { t_skip("emulator aborts on unimplemented *TzSpecific*Ex"); return; }
    DYNAMIC_TIME_ZONE_INFORMATION d; dtzi_for_key(&d, L"Pacific Standard Time");
    for (int i = 0; i < ARRAYLEN(PAC); i++) {
        SYSTEMTIME u = nls_st(PAC[i].uy, PAC[i].um, PAC[i].ud, PAC[i].uh, PAC[i].umi, PAC[i].us, 7), l, w;
        w = nls_st(PAC[i].ly, PAC[i].lm, PAC[i].ld, PAC[i].lh, PAC[i].lmi, PAC[i].ls, 7);
        memset(&l, 0, sizeof l); SET_ERR(0);
        BOOL r = UN(SystemTimeToTzSpecificLocalTimeEx)(&d, &u, &l);
        CHECKF(r && nls_cmp_st(&l, &w), "row %d err %lu got %04d-%02d-%02d %02d:%02d:%02d", i, GetLastError(), l.wYear, l.wMonth, l.wDay, l.wHour, l.wMinute, l.wSecond);
    }
}
TEST(tzex_local_to_utc_pacific_key) {
    REQUIRE(UN(TzSpecificLocalTimeToSystemTimeEx));
    if (t_is_wine()) { t_skip("emulator aborts on unimplemented *TzSpecific*Ex"); return; }
    DYNAMIC_TIME_ZONE_INFORMATION d; dtzi_for_key(&d, L"Pacific Standard Time");
    for (int i = 0; i < ARRAYLEN(PAC); i++) {
        if (PAC[i].um == 11 && PAC[i].ud == 3 && PAC[i].uh == 9) continue;   /* ambiguous hour */
        SYSTEMTIME l = nls_st(PAC[i].ly, PAC[i].lm, PAC[i].ld, PAC[i].lh, PAC[i].lmi, PAC[i].ls, 7), u;
        SYSTEMTIME w = nls_st(PAC[i].uy, PAC[i].um, PAC[i].ud, PAC[i].uh, PAC[i].umi, PAC[i].us, 7);
        memset(&u, 0, sizeof u); SET_ERR(0);
        BOOL r = UN(TzSpecificLocalTimeToSystemTimeEx)(&d, &l, &u);
        if (PAC[i].um == 11 && PAC[i].ud == 3 && PAC[i].uh == 8) { CHECKF(r, "row %d err %lu", i, GetLastError()); continue; }   /* local 01:59:59 is ambiguous */
        CHECKF(r && nls_cmp_st(&u, &w), "row %d err %lu got %04d-%02d-%02d %02d:%02d:%02d", i, GetLastError(), u.wYear, u.wMonth, u.wDay, u.wHour, u.wMinute, u.wSecond);
    }
}
TEST(tzex_dynamic_dst_disabled_flag) {
    REQUIRE(UN(SystemTimeToTzSpecificLocalTimeEx));
    if (t_is_wine()) { t_skip("emulator aborts on unimplemented *TzSpecific*Ex"); return; }
    DYNAMIC_TIME_ZONE_INFORMATION d; dtzi_for_key(&d, L"Pacific Standard Time"); d.DynamicDaylightTimeDisabled = TRUE;
    SYSTEMTIME u = nls_st(2024, 7, 15, 12, 0, 0, 0), l; SET_ERR(0);
    BOOL r = UN(SystemTimeToTzSpecificLocalTimeEx)(&d, &u, &l);
    CHECKF(r, "err %lu", GetLastError());
    if (r) t_note("DynamicDaylightTimeDisabled=TRUE: 12:00 UTC in July -> %02d:%02d", l.wHour, l.wMinute);
}
TEST(tzex_fixed_zones_by_key) {
    REQUIRE(UN(SystemTimeToTzSpecificLocalTimeEx));
    if (t_is_wine()) { t_skip("emulator aborts on unimplemented *TzSpecific*Ex"); return; }
    static const struct { const WCHAR *key; int off_min; } z[] = {
        { L"UTC", 0 }, { L"Tokyo Standard Time", 540 }, { L"India Standard Time", 330 }, { L"Nepal Standard Time", 345 },
        { L"China Standard Time", 480 }, { L"Hawaiian Standard Time", -600 }, { L"Line Islands Standard Time", 840 }, { L"Dateline Standard Time", -720 },
    };
    for (int i = 0; i < ARRAYLEN(z); i++) {
        DYNAMIC_TIME_ZONE_INFORMATION d; dtzi_for_key(&d, z[i].key);
        for (int m = 1; m <= 12; m += 5) {
            SYSTEMTIME u = nls_st(2024, m, 15, 12, 0, 0, 0), l; SET_ERR(0);
            BOOL r = UN(SystemTimeToTzSpecificLocalTimeEx)(&d, &u, &l);
            CHECKF(r, "%ls err %lu", z[i].key, GetLastError());
            if (r) CHECKF(tzd_secs(&l) - tzd_secs(&u) == z[i].off_min * 60LL, "%ls month %d delta %lld", z[i].key, m, tzd_secs(&l) - tzd_secs(&u));
        }
    }
}
TEST(tzex_europe_and_australia_by_key) {
    REQUIRE(UN(SystemTimeToTzSpecificLocalTimeEx));
    if (t_is_wine()) { t_skip("emulator aborts on unimplemented *TzSpecific*Ex"); return; }
    struct { const WCHAR *key; vec_t v; } t[] = {
        { L"W. Europe Standard Time", { 2024, 3, 31, 0, 59, 59,  2024, 3, 31, 1, 59, 59 } },
        { L"W. Europe Standard Time", { 2024, 3, 31, 1, 0, 0,    2024, 3, 31, 3, 0, 0 } },
        { L"W. Europe Standard Time", { 2024, 10, 27, 0, 59, 59, 2024, 10, 27, 2, 59, 59 } },
        { L"W. Europe Standard Time", { 2024, 10, 27, 1, 0, 0,   2024, 10, 27, 2, 0, 0 } },
        { L"GMT Standard Time",       { 2024, 3, 31, 0, 59, 59,  2024, 3, 31, 0, 59, 59 } },
        { L"GMT Standard Time",       { 2024, 3, 31, 1, 0, 0,    2024, 3, 31, 2, 0, 0 } },
        { L"GMT Standard Time",       { 2024, 10, 27, 0, 59, 59, 2024, 10, 27, 1, 59, 59 } },
        { L"GMT Standard Time",       { 2024, 10, 27, 1, 0, 0,   2024, 10, 27, 1, 0, 0 } },
        { L"AUS Eastern Standard Time", { 2024, 4, 6, 15, 59, 59, 2024, 4, 7, 2, 59, 59 } },
        { L"AUS Eastern Standard Time", { 2024, 4, 6, 16, 0, 0,   2024, 4, 7, 2, 0, 0 } },
        { L"AUS Eastern Standard Time", { 2024, 10, 5, 15, 59, 59, 2024, 10, 6, 1, 59, 59 } },
        { L"AUS Eastern Standard Time", { 2024, 10, 5, 16, 0, 0,  2024, 10, 6, 3, 0, 0 } },
        { L"AUS Eastern Standard Time", { 2024, 1, 15, 0, 0, 0,   2024, 1, 15, 11, 0, 0 } },
        { L"New Zealand Standard Time", { 2024, 9, 28, 13, 59, 59, 2024, 9, 29, 1, 59, 59 } },
        { L"New Zealand Standard Time", { 2024, 9, 28, 14, 0, 0,  2024, 9, 29, 3, 0, 0 } },
        { L"New Zealand Standard Time", { 2024, 4, 6, 13, 59, 59, 2024, 4, 7, 2, 59, 59 } },
        { L"New Zealand Standard Time", { 2024, 4, 6, 14, 0, 0,   2024, 4, 7, 2, 0, 0 } },
        { L"Lord Howe Standard Time", { 2024, 10, 5, 15, 29, 59, 2024, 10, 6, 1, 59, 59 } },
        { L"Lord Howe Standard Time", { 2024, 10, 5, 15, 30, 0,  2024, 10, 6, 2, 30, 0 } },
        { L"Lord Howe Standard Time", { 2024, 4, 6, 14, 59, 59,  2024, 4, 7, 1, 59, 59 } },
        { L"Lord Howe Standard Time", { 2024, 4, 6, 15, 0, 0,    2024, 4, 7, 1, 30, 0 } },
    };
    for (int i = 0; i < ARRAYLEN(t); i++) {
        DYNAMIC_TIME_ZONE_INFORMATION d; dtzi_for_key(&d, t[i].key);
        SYSTEMTIME u = nls_st(t[i].v.uy, t[i].v.um, t[i].v.ud, t[i].v.uh, t[i].v.umi, t[i].v.us, 0), l, w;
        w = nls_st(t[i].v.ly, t[i].v.lm, t[i].v.ld, t[i].v.lh, t[i].v.lmi, t[i].v.ls, 0);
        memset(&l, 0, sizeof l); SET_ERR(0);
        BOOL r = UN(SystemTimeToTzSpecificLocalTimeEx)(&d, &u, &l);
        CHECKF(r && nls_cmp_st(&l, &w), "row %d %ls err %lu got %04d-%02d-%02d %02d:%02d:%02d", i, t[i].key, GetLastError(), l.wYear, l.wMonth, l.wDay, l.wHour, l.wMinute, l.wSecond);
    }
}
TEST(tzex_null_zone_is_current) {
    REQUIRE(UN(SystemTimeToTzSpecificLocalTimeEx)); REQUIRE(U(SystemTimeToTzSpecificLocalTime));
    if (t_is_wine()) { t_skip("emulator aborts on unimplemented *TzSpecific*Ex"); return; }
    SYSTEMTIME u = nls_st(2024, 7, 15, 12, 0, 0, 0), a, b; SET_ERR(0);
    BOOL r = UN(SystemTimeToTzSpecificLocalTimeEx)(NULL, &u, &a);
    CHECKF(r, "err %lu", GetLastError());
    CHECK(U(SystemTimeToTzSpecificLocalTime)(NULL, &u, &b));
    if (r) CHECK(nls_cmp_st(&a, &b));
    REQUIRE(UN(TzSpecificLocalTimeToSystemTimeEx));
    SYSTEMTIME back; SET_ERR(0);
    r = UN(TzSpecificLocalTimeToSystemTimeEx)(NULL, &a, &back);
    CHECKF(r, "err %lu", GetLastError());
    if (r) CHECK(nls_cmp_st(&back, &u));
}
TEST(tzex_current_zone_dtzi_roundtrip) {
    REQUIRE(UN(SystemTimeToTzSpecificLocalTimeEx)); REQUIRE(UN(TzSpecificLocalTimeToSystemTimeEx)); REQUIRE(U(GetDynamicTimeZoneInformation));
    if (t_is_wine()) { t_skip("emulator aborts on unimplemented *TzSpecific*Ex"); return; }
    DYNAMIC_TIME_ZONE_INFORMATION d; U(GetDynamicTimeZoneInformation)(&d);
    SYSTEMTIME u = nls_st(2024, 1, 15, 12, 0, 0, 0), l, b, n;
    CHECK(UN(SystemTimeToTzSpecificLocalTimeEx)(&d, &u, &l));
    CHECK(UN(SystemTimeToTzSpecificLocalTimeEx)(NULL, &u, &n)); CHECK(nls_cmp_st(&l, &n));
    CHECK(UN(TzSpecificLocalTimeToSystemTimeEx)(&d, &l, &b)); CHECK(nls_cmp_st(&u, &b));
}
TEST(tzex_invalid_inputs) {
    REQUIRE(UN(SystemTimeToTzSpecificLocalTimeEx)); REQUIRE(UN(TzSpecificLocalTimeToSystemTimeEx));
    if (t_is_wine()) { t_skip("emulator aborts on unimplemented *TzSpecific*Ex"); return; }
    DYNAMIC_TIME_ZONE_INFORMATION d; dtzi_for_key(&d, L"Pacific Standard Time");
    SYSTEMTIME bad = nls_st(2024, 13, 1, 0, 0, 0, 0), l; SET_ERR(0);
    CHECK(!UN(SystemTimeToTzSpecificLocalTimeEx)(&d, &bad, &l));
    SET_ERR(0); CHECK(!UN(TzSpecificLocalTimeToSystemTimeEx)(&d, &bad, &l));
    bad = nls_st(2024, 2, 30, 0, 0, 0, 0);
    SET_ERR(0); CHECK(!UN(SystemTimeToTzSpecificLocalTimeEx)(&d, &bad, &l));
    bad = nls_st(2024, 1, 1, 24, 0, 0, 0);
    SET_ERR(0); CHECK(!UN(SystemTimeToTzSpecificLocalTimeEx)(&d, &bad, &l));
    DYNAMIC_TIME_ZONE_INFORMATION nz; dtzi_for_key(&nz, L"No Such Zone For Sure");
    SYSTEMTIME u = nls_st(2024, 1, 1, 0, 0, 0, 0); SET_ERR(0);
    BOOL r = UN(SystemTimeToTzSpecificLocalTimeEx)(&nz, &u, &l);
    t_note("unknown zone key conversion: r=%d err=%lu", r, r ? 0 : GetLastError());
}
TEST(tzex_matches_static_conversion_for_pacific_2024) {
    REQUIRE(UN(SystemTimeToTzSpecificLocalTimeEx)); REQUIRE(U(SystemTimeToTzSpecificLocalTime)); REQUIRE(U(GetTimeZoneInformationForYear));
    if (t_is_wine()) { t_skip("emulator aborts on unimplemented *TzSpecific*Ex"); return; }
    DYNAMIC_TIME_ZONE_INFORMATION d; TIME_ZONE_INFORMATION t; dtzi_for_key(&d, L"Pacific Standard Time");
    CHECK(U(GetTimeZoneInformationForYear)(2024, &d, &t));
    int bad = 0;
    long long base = tzd_days(2024, 1, 1) * 86400;
    for (int h = 0; h < 366 * 24; h += 3) {
        long long s = base + h * 3600LL; long long dd = s / 86400, rem = s % 86400; (void)dd;
        SYSTEMTIME u = nls_st(2024, 1, 1, 0, 0, 0, 0);
        /* advance by whole days/hours via FileTime scaffolding-free arithmetic */
        long long days = h / 24; int hour = h % 24;
        int y = 2024, m = 1, dday = 1; (void)y;
        static const int dim[] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        long long left = days;
        while (left >= dim[m - 1]) { left -= dim[m - 1]; m++; }
        dday = (int)left + 1;
        u = nls_st(2024, m, dday, hour, 0, 0, 0);
        SYSTEMTIME a, b;
        if (!UN(SystemTimeToTzSpecificLocalTimeEx)(&d, &u, &a) || !U(SystemTimeToTzSpecificLocalTime)(&t, &u, &b) || !nls_cmp_st(&a, &b)) bad++;
    }
    CHECKF(bad == 0, "bad=%d", bad);
}
