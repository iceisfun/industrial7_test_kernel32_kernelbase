/* Time zone: current zone queries, dynamic zones, and exact DST arithmetic with synthetic zones. */
#include "nlsutil.h"

/* ---- calendar helpers (independent oracle) ---- */
static long long days_from_civil(int y, int m, int d)
{
    y -= m <= 2;
    long long era = (y >= 0 ? y : y - 399) / 400;
    long long yoe = y - era * 400;
    long long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}
static void civil_from_days(long long z, int *y, int *m, int *d)
{
    z += 719468;
    long long era = (z >= 0 ? z : z - 146096) / 146097;
    long long doe = z - era * 146097;
    long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long long yy = yoe + era * 400;
    long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long long mp = (5 * doy + 2) / 153;
    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y = (int)(yy + (*m <= 2));
}
static long long st_to_secs(const SYSTEMTIME *s)
{
    return days_from_civil(s->wYear, s->wMonth, s->wDay) * 86400 + s->wHour * 3600 + s->wMinute * 60 + s->wSecond;
}
static SYSTEMTIME secs_to_st(long long t)
{
    long long d = t >= 0 ? t / 86400 : -((-t + 86399) / 86400);
    long long r = t - d * 86400;
    int y, m, dd; civil_from_days(d, &y, &m, &dd);
    return nls_st(y, m, dd, (int)(r / 3600), (int)(r / 60 % 60), (int)(r % 60), 0);
}
static int dim(int y, int m)
{
    static const int t[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) return 29;
    return t[m - 1];
}
/* nth (1..4) or last (5) occurrence of weekday dow in month */
static int nth_dow(int y, int m, int dow, int n)
{
    SYSTEMTIME f = nls_st(y, m, 1, 0, 0, 0, 0);
    int first = 1 + ((dow - f.wDayOfWeek) + 7) % 7;
    int d = first + (n >= 5 ? 4 : n - 1) * 7;
    while (d > dim(y, m)) d -= 7;
    return d;
}

typedef struct { int month, n, hour, minute; } rule_t;    /* relative: nth Sunday */
typedef struct { const char *name; int bias, stdbias, dstbias; rule_t std, dst; } zone_t;

static TIME_ZONE_INFORMATION mk_tzi(const zone_t *z)
{
    TIME_ZONE_INFORMATION t; memset(&t, 0, sizeof t);
    t.Bias = z->bias; t.StandardBias = z->stdbias; t.DaylightBias = z->dstbias;
    wsprintfW(t.StandardName, L"%hs Std", z->name); wsprintfW(t.DaylightName, L"%hs Dst", z->name);
    if (z->std.month) { t.StandardDate.wMonth = z->std.month; t.StandardDate.wDay = z->std.n; t.StandardDate.wHour = z->std.hour; t.StandardDate.wMinute = z->std.minute; }
    if (z->dst.month) { t.DaylightDate.wMonth = z->dst.month; t.DaylightDate.wDay = z->dst.n; t.DaylightDate.wHour = z->dst.hour; t.DaylightDate.wMinute = z->dst.minute; }
    return t;
}
/* UTC seconds of the transitions of year y */
static long long dst_start_utc(const zone_t *z, int y)   /* wall time is standard time */
{
    int d = nth_dow(y, z->dst.month, 0, z->dst.n);
    SYSTEMTIME s = nls_st(y, z->dst.month, d, z->dst.hour, z->dst.minute, 0, 0);
    return st_to_secs(&s) + (z->bias + z->stdbias) * 60;
}
static long long dst_end_utc(const zone_t *z, int y)     /* wall time is daylight time */
{
    int d = nth_dow(y, z->std.month, 0, z->std.n);
    SYSTEMTIME s = nls_st(y, z->std.month, d, z->std.hour, z->std.minute, 0, 0);
    return st_to_secs(&s) + (z->bias + z->dstbias) * 60;
}
static int in_dst(const zone_t *z, long long t)
{
    if (!z->dst.month) return 0;
    SYSTEMTIME s = secs_to_st(t);
    int y = s.wYear;
    long long S = dst_start_utc(z, y), E = dst_end_utc(z, y);
    if (S < E) return t >= S && t < E;
    return t >= S || t < E;
}
static long long oracle_local(const zone_t *z, long long t)
{
    return t - (z->bias + (in_dst(z, t) ? z->dstbias : z->stdbias)) * 60;
}

static const zone_t ZONES[] = {
    { "US Pacific",  480, 0, -60, { 11, 1, 2, 0 }, { 3, 2, 2, 0 } },
    { "US Eastern",  300, 0, -60, { 11, 1, 2, 0 }, { 3, 2, 2, 0 } },
    { "US Mountain", 420, 0, -60, { 11, 1, 2, 0 }, { 3, 2, 2, 0 } },
    { "W Europe",    -60, 0, -60, { 10, 5, 3, 0 }, { 3, 5, 2, 0 } },
    { "UK",            0, 0, -60, { 10, 5, 2, 0 }, { 3, 5, 1, 0 } },
    { "AUS Eastern", -600, 0, -60, { 4, 1, 3, 0 }, { 10, 1, 2, 0 } },
    { "New Zealand", -720, 0, -60, { 4, 1, 3, 0 }, { 9, 5, 2, 0 } },
    { "Lord Howe",  -630, 0, -30, { 4, 1, 2, 0 }, { 10, 1, 2, 0 } },
    { "Chile",       240, 0, -60, { 4, 1, 24 > 23 ? 0 : 0, 0 }, { 9, 1, 0, 0 } },
    { "Half Hour Rule", 300, 0, -60, { 11, 1, 2, 30 }, { 3, 2, 2, 30 } },
    { "Odd Bias",   -345, 0, -60, { 10, 4, 4, 0 }, { 4, 2, 3, 0 } },
    { "Neg DST",       0, 0, 60,  { 3, 5, 2, 0 }, { 10, 5, 3, 0 } },
};
#define NZONE ARRAYLEN(ZONES)

static void check_zone_year(const zone_t *z, int y)
{
    TIME_ZONE_INFORMATION tzi = mk_tzi(z);
    long long S = dst_start_utc(z, y), E = dst_end_utc(z, y);
    long long pts[] = { S - 1, S, S + 1, E - 1, E, E + 1, S - 86400 * 20, S + 86400 * 20, E - 86400 * 20, E + 86400 * 20,
                        st_to_secs(&(SYSTEMTIME){ y, 1, 0, 1, 0, 0, 0, 0 }), st_to_secs(&(SYSTEMTIME){ y, 7, 0, 1, 12, 0, 0, 0 }) };
    for (int i = 0; i < ARRAYLEN(pts); i++) {
        SYSTEMTIME u = secs_to_st(pts[i]), l, want = secs_to_st(oracle_local(z, pts[i]));
        memset(&l, 0, sizeof l);
        SET_ERR(0);
        BOOL r = U(SystemTimeToTzSpecificLocalTime)(&tzi, &u, &l);
        if (!r) { CHECKF(0, "%s %d pt %d: fail err %lu", z->name, y, i, GetLastError()); continue; }
        CHECKF(nls_cmp_st(&l, &want), "%s utc %04d-%02d-%02d %02d:%02d:%02d -> %04d-%02d-%02d %02d:%02d:%02d want %04d-%02d-%02d %02d:%02d:%02d",
               z->name, u.wYear, u.wMonth, u.wDay, u.wHour, u.wMinute, u.wSecond, l.wYear, l.wMonth, l.wDay, l.wHour, l.wMinute, l.wSecond,
               want.wYear, want.wMonth, want.wDay, want.wHour, want.wMinute, want.wSecond);
    }
}

#define ZONE_TEST(idx, nm) \
TEST(tzcalc_utc_to_local_##nm) { \
    REQUIRE(U(SystemTimeToTzSpecificLocalTime)); \
    for (int y = 1970; y <= 2100; y++) check_zone_year(&ZONES[idx], y); \
}
ZONE_TEST(0, us_pacific) ZONE_TEST(1, us_eastern) ZONE_TEST(2, us_mountain) ZONE_TEST(3, w_europe) ZONE_TEST(4, uk)
ZONE_TEST(5, aus_eastern) ZONE_TEST(6, new_zealand) ZONE_TEST(7, lord_howe) ZONE_TEST(8, chile) ZONE_TEST(9, half_hour_rule)
ZONE_TEST(10, odd_bias) ZONE_TEST(11, negative_dst)

TEST(tzcalc_us_pacific_2024_explicit) {
    REQUIRE(U(SystemTimeToTzSpecificLocalTime));
    zone_t z = ZONES[0]; TIME_ZONE_INFORMATION t = mk_tzi(&z);
    static const struct { int uy, um, ud, uh, umi, us; int ly, lm, ld, lh, lmi, ls; } v[] = {
        { 2024, 1, 15, 12, 0, 0,   2024, 1, 15, 4, 0, 0 },
        { 2024, 7, 15, 12, 0, 0,   2024, 7, 15, 5, 0, 0 },
        { 2024, 3, 10, 9, 59, 59,  2024, 3, 10, 1, 59, 59 },
        { 2024, 3, 10, 10, 0, 0,   2024, 3, 10, 3, 0, 0 },
        { 2024, 3, 10, 10, 0, 1,   2024, 3, 10, 3, 0, 1 },
        { 2024, 11, 3, 8, 59, 59,  2024, 11, 3, 1, 59, 59 },
        { 2024, 11, 3, 9, 0, 0,    2024, 11, 3, 1, 0, 0 },
        { 2024, 11, 3, 9, 59, 59,  2024, 11, 3, 1, 59, 59 },
        { 2024, 11, 3, 10, 0, 0,   2024, 11, 3, 2, 0, 0 },
        { 2024, 1, 1, 0, 0, 0,     2023, 12, 31, 16, 0, 0 },
        { 2025, 1, 1, 7, 59, 59,   2024, 12, 31, 23, 59, 59 },
        { 2025, 1, 1, 8, 0, 0,     2025, 1, 1, 0, 0, 0 },
        { 2024, 3, 1, 0, 0, 0,     2024, 2, 29, 16, 0, 0 },
        { 2023, 3, 1, 0, 0, 0,     2023, 2, 28, 16, 0, 0 },
        { 2024, 3, 31, 23, 59, 59, 2024, 3, 31, 16, 59, 59 },
        { 2005, 3, 13, 10, 0, 0,   2005, 3, 13, 3, 0, 0 },
        { 2007, 3, 11, 10, 0, 0,   2007, 3, 11, 3, 0, 0 },
        { 2007, 11, 4, 9, 0, 0,    2007, 11, 4, 1, 0, 0 },
        { 2000, 2, 29, 6, 30, 15,  2000, 2, 28, 22, 30, 15 },
        { 1970, 1, 1, 0, 0, 0,     1969, 12, 31, 16, 0, 0 },
    };
    for (int i = 0; i < ARRAYLEN(v); i++) {
        SYSTEMTIME u = nls_st(v[i].uy, v[i].um, v[i].ud, v[i].uh, v[i].umi, v[i].us, 123), l;
        SYSTEMTIME w = nls_st(v[i].ly, v[i].lm, v[i].ld, v[i].lh, v[i].lmi, v[i].ls, 123);
        memset(&l, 0, sizeof l);
        CHECKF(U(SystemTimeToTzSpecificLocalTime)(&t, &u, &l) && nls_cmp_st(&l, &w), "row %d got %04d-%02d-%02d %02d:%02d:%02d.%03d", i, l.wYear, l.wMonth, l.wDay, l.wHour, l.wMinute, l.wSecond, l.wMilliseconds);
        if (nls_cmp_st(&l, &w)) { CHECKF(l.wDayOfWeek == w.wDayOfWeek, "row %d dow", i); }
    }
}
TEST(tzcalc_utc_to_local_fills_dayofweek) {
    REQUIRE(U(SystemTimeToTzSpecificLocalTime));
    TIME_ZONE_INFORMATION t = mk_tzi(&ZONES[0]);
    SYSTEMTIME u = nls_st(2024, 3, 11, 3, 0, 0, 0), l; u.wDayOfWeek = 6;   /* wrong weekday on input is ignored */
    memset(&l, 0xFF, sizeof l);
    CHECK(U(SystemTimeToTzSpecificLocalTime)(&t, &u, &l));
    CHECKF(l.wYear == 2024 && l.wMonth == 3 && l.wDay == 10 && l.wHour == 20, "got %d-%d-%d %d", l.wYear, l.wMonth, l.wDay, l.wHour);
    CHECKF(l.wDayOfWeek == 0, "dow %d", l.wDayOfWeek);
}
TEST(tzcalc_milliseconds_preserved) {
    REQUIRE(U(SystemTimeToTzSpecificLocalTime));
    TIME_ZONE_INFORMATION t = mk_tzi(&ZONES[0]);
    for (int ms = 0; ms < 1000; ms += 111) {
        SYSTEMTIME u = nls_st(2024, 6, 1, 12, 0, 0, ms), l;
        CHECK(U(SystemTimeToTzSpecificLocalTime)(&t, &u, &l)); CHECK(l.wMilliseconds == ms); CHECK(l.wHour == 5);
    }
}
TEST(tzcalc_in_place_conversion) {
    REQUIRE(U(SystemTimeToTzSpecificLocalTime)); REQUIRE(U(TzSpecificLocalTimeToSystemTime));
    TIME_ZONE_INFORMATION t = mk_tzi(&ZONES[0]);
    SYSTEMTIME s = nls_st(2024, 7, 4, 12, 0, 0, 0);
    CHECK(U(SystemTimeToTzSpecificLocalTime)(&t, &s, &s));
    CHECK(s.wHour == 5 && s.wDay == 4);
    CHECK(U(TzSpecificLocalTimeToSystemTime)(&t, &s, &s));
    CHECK(s.wHour == 12 && s.wDay == 4);
}
TEST(tzcalc_fixed_bias_zones) {
    REQUIRE(U(SystemTimeToTzSpecificLocalTime));
    static const struct { int bias; int dh, dm; } z[] = { { 0, 0, 0 }, { -330, 5, 30 }, { -345, 5, 45 }, { 480, -8, 0 }, { -840, 14, 0 }, { 720, -12, 0 }, { -60, 1, 0 }, { 210, -3, -30 }, { 1, 0, -1 }, { -1, 0, 1 } };
    for (int i = 0; i < ARRAYLEN(z); i++) {
        TIME_ZONE_INFORMATION t; memset(&t, 0, sizeof t); t.Bias = z[i].bias;
        SYSTEMTIME u = nls_st(2024, 6, 15, 12, 0, 0, 0), l;
        CHECKF(U(SystemTimeToTzSpecificLocalTime)(&t, &u, &l), "bias %d", z[i].bias);
        long long want = st_to_secs(&u) - z[i].bias * 60LL;
        SYSTEMTIME w = secs_to_st(want);
        CHECKF(nls_cmp_st(&l, &w), "bias %d got %02d:%02d want %02d:%02d", z[i].bias, l.wHour, l.wMinute, w.wHour, w.wMinute);
    }
}
TEST(tzcalc_fixed_bias_day_rollover) {
    REQUIRE(U(SystemTimeToTzSpecificLocalTime));
    TIME_ZONE_INFORMATION t; memset(&t, 0, sizeof t); t.Bias = -840;   /* UTC+14 */
    SYSTEMTIME u = nls_st(2024, 12, 31, 12, 0, 0, 0), l;
    CHECK(U(SystemTimeToTzSpecificLocalTime)(&t, &u, &l));
    CHECK(l.wYear == 2025 && l.wMonth == 1 && l.wDay == 1 && l.wHour == 2);
    t.Bias = 720;
    u = nls_st(2024, 1, 1, 6, 0, 0, 0);
    CHECK(U(SystemTimeToTzSpecificLocalTime)(&t, &u, &l));
    CHECK(l.wYear == 2023 && l.wMonth == 12 && l.wDay == 31 && l.wHour == 18);
    u = nls_st(2024, 3, 1, 0, 0, 0, 0); t.Bias = 1;
    CHECK(U(SystemTimeToTzSpecificLocalTime)(&t, &u, &l));
    CHECK(l.wMonth == 2 && l.wDay == 29 && l.wHour == 23 && l.wMinute == 59);
}
TEST(tzcalc_utc_zone) {
    REQUIRE(U(SystemTimeToTzSpecificLocalTime));
    TIME_ZONE_INFORMATION t; memset(&t, 0, sizeof t);
    SYSTEMTIME u = nls_st(2024, 6, 15, 12, 34, 56, 789), l;
    CHECK(U(SystemTimeToTzSpecificLocalTime)(&t, &u, &l)); CHECK(nls_cmp_st(&u, &l));
}
TEST(tzcalc_year_extremes) {
    REQUIRE(U(SystemTimeToTzSpecificLocalTime));
    TIME_ZONE_INFORMATION t = mk_tzi(&ZONES[0]);
    SYSTEMTIME u = nls_st(1601, 1, 2, 12, 0, 0, 0), l; SET_ERR(0);
    CHECKF(U(SystemTimeToTzSpecificLocalTime)(&t, &u, &l), "err %lu", GetLastError());
    if (l.wYear == 1601) CHECK(l.wHour == 4);
    u = nls_st(30827, 6, 15, 12, 0, 0, 0);
    CHECKF(U(SystemTimeToTzSpecificLocalTime)(&t, &u, &l), "err %lu", GetLastError());
    u = nls_st(1601, 1, 1, 0, 0, 0, 0); SET_ERR(0);
    BOOL r = U(SystemTimeToTzSpecificLocalTime)(&t, &u, &l);
    t_note("UTC 1601-01-01 00:00 in UTC-8 (would underflow): r=%d err=%lu", r, r ? 0 : GetLastError());
}
TEST(tzcalc_invalid_systemtime) {
    REQUIRE(U(SystemTimeToTzSpecificLocalTime));
    TIME_ZONE_INFORMATION t = mk_tzi(&ZONES[0]);
    static const struct { int y, mo, d, h, mi, s; } bad[] = { { 2024, 0, 1, 0, 0, 0 }, { 2024, 13, 1, 0, 0, 0 }, { 2024, 1, 0, 0, 0, 0 }, { 2024, 2, 30, 0, 0, 0 },
        { 2024, 1, 1, 24, 0, 0 }, { 2024, 1, 1, 0, 60, 0 }, { 2024, 1, 1, 0, 0, 60 }, { 1600, 1, 1, 0, 0, 0 }, { 30828, 1, 1, 0, 0, 0 }, { 2023, 2, 29, 0, 0, 0 } };
    for (int i = 0; i < ARRAYLEN(bad); i++) {
        SYSTEMTIME u = nls_st(bad[i].y, bad[i].mo, bad[i].d, bad[i].h, bad[i].mi, bad[i].s, 0), l; SET_ERR(0);
        BOOL r = U(SystemTimeToTzSpecificLocalTime)(&t, &u, &l);
        CHECKF(!r, "row %d", i);
        if (!r) t_note("invalid utc row %d err=%lu", i, GetLastError());
    }
    SYSTEMTIME u = nls_st(2024, 1, 1, 0, 0, 0, 1000), l; SET_ERR(0);
    CHECK(!U(SystemTimeToTzSpecificLocalTime)(&t, &u, &l));
}
TEST(tzcalc_null_tzi_uses_current_zone) {
    REQUIRE(U(SystemTimeToTzSpecificLocalTime)); REQUIRE(U(GetTimeZoneInformation));
    TIME_ZONE_INFORMATION cur; DWORD id = U(GetTimeZoneInformation)(&cur);
    CHECK(id != TIME_ZONE_ID_INVALID);
    SYSTEMTIME u = nls_st(2024, 1, 15, 12, 0, 0, 0), a, b;
    CHECK(U(SystemTimeToTzSpecificLocalTime)(NULL, &u, &a));
    CHECK(U(SystemTimeToTzSpecificLocalTime)(&cur, &u, &b));
    CHECKF(nls_cmp_st(&a, &b), "NULL tzi differs from explicit current zone: %02d:%02d vs %02d:%02d", a.wHour, a.wMinute, b.wHour, b.wMinute);
    SYSTEMTIME u2 = nls_st(2024, 7, 15, 12, 0, 0, 0);
    CHECK(U(SystemTimeToTzSpecificLocalTime)(NULL, &u2, &a));
    CHECK(U(SystemTimeToTzSpecificLocalTime)(&cur, &u2, &b));
    CHECK(nls_cmp_st(&a, &b));
}

/* ---- local -> UTC ---- */
static void check_inverse(const zone_t *z, int y)
{
    TIME_ZONE_INFORMATION tzi = mk_tzi(z);
    long long S = dst_start_utc(z, y), E = dst_end_utc(z, y);
    long long pts[] = { S - 1, S, S + 1, E - 1, E, E + 1, S - 86400 * 20, E + 86400 * 20, S + 3600 * 2, E - 3600 * 2, E + 3600 * 2, S - 3600 * 2,
                        S + 1800, E - 1800, S - 1800, E + 1800, S + 3600, E - 3600 };
    for (int i = 0; i < ARRAYLEN(pts); i++) {
        /* take an instant, compute its local wall time with the oracle, then invert */
        long long loc = oracle_local(z, pts[i]);
        SYSTEMTIME l = secs_to_st(loc), u; memset(&u, 0, sizeof u);
        long long c1 = loc + (z->bias + z->stdbias) * 60, c2 = loc + (z->bias + z->dstbias) * 60;
        int v1 = !in_dst(z, c1) || !z->dst.month;
        int v2 = in_dst(z, c2) && z->dst.month;
        SET_ERR(0);
        BOOL r = U(TzSpecificLocalTimeToSystemTime)(&tzi, &l, &u);
        if (!r) { CHECKF(0, "%s local %04d-%02d-%02d %02d:%02d:%02d fail err %lu", z->name, l.wYear, l.wMonth, l.wDay, l.wHour, l.wMinute, l.wSecond, GetLastError()); continue; }
        long long got = st_to_secs(&u);
        if (v1 + v2 == 1) {
            long long want = v1 ? c1 : c2;
            CHECKF(got == want, "%s local %04d-%02d-%02d %02d:%02d:%02d -> utc %04d-%02d-%02d %02d:%02d:%02d (delta %lld s)", z->name,
                   l.wYear, l.wMonth, l.wDay, l.wHour, l.wMinute, l.wSecond, u.wYear, u.wMonth, u.wDay, u.wHour, u.wMinute, u.wSecond, got - want);
        } else {
            CHECKF(got == c1 || got == c2, "%s ambiguous/gap local result %lld not in {%lld,%lld}", z->name, got, c1, c2);
        }
    }
}
#define INV_TEST(idx, nm) \
TEST(tzcalc_local_to_utc_##nm) { \
    REQUIRE(U(TzSpecificLocalTimeToSystemTime)); \
    for (int y = 1980; y <= 2060; y++) check_inverse(&ZONES[idx], y); \
}
INV_TEST(0, us_pacific) INV_TEST(1, us_eastern) INV_TEST(3, w_europe) INV_TEST(4, uk) INV_TEST(5, aus_eastern) INV_TEST(6, new_zealand)
INV_TEST(7, lord_howe) INV_TEST(9, half_hour_rule) INV_TEST(10, odd_bias) INV_TEST(11, negative_dst)

TEST(tzcalc_local_to_utc_us_pacific_2024_explicit) {
    REQUIRE(U(TzSpecificLocalTimeToSystemTime));
    TIME_ZONE_INFORMATION t = mk_tzi(&ZONES[0]);
    static const struct { int ly, lm, ld, lh, lmi, ls; int uy, um, ud, uh, umi, us; } v[] = {
        { 2024, 1, 15, 4, 0, 0,   2024, 1, 15, 12, 0, 0 },
        { 2024, 7, 15, 5, 0, 0,   2024, 7, 15, 12, 0, 0 },
        { 2024, 3, 10, 1, 59, 59, 2024, 3, 10, 9, 59, 59 },
        { 2024, 3, 10, 3, 0, 0,   2024, 3, 10, 10, 0, 0 },
        { 2024, 3, 10, 3, 0, 1,   2024, 3, 10, 10, 0, 1 },
        { 2024, 11, 3, 0, 59, 59, 2024, 11, 3, 7, 59, 59 },
        { 2024, 11, 3, 2, 0, 0,   2024, 11, 3, 10, 0, 0 },
        { 2024, 11, 3, 2, 0, 1,   2024, 11, 3, 10, 0, 1 },
        { 2024, 12, 31, 23, 59, 59, 2025, 1, 1, 7, 59, 59 },
        { 2024, 1, 1, 0, 0, 0,    2024, 1, 1, 8, 0, 0 },
        { 2023, 12, 31, 16, 0, 0, 2024, 1, 1, 0, 0, 0 },
        { 2024, 2, 29, 16, 0, 0,  2024, 3, 1, 0, 0, 0 },
        { 2024, 3, 9, 18, 0, 0,   2024, 3, 10, 2, 0, 0 },
        { 2024, 3, 10, 4, 0, 0,   2024, 3, 10, 11, 0, 0 },
        { 2024, 11, 3, 3, 0, 0,   2024, 11, 3, 11, 0, 0 },
    };
    for (int i = 0; i < ARRAYLEN(v); i++) {
        SYSTEMTIME l = nls_st(v[i].ly, v[i].lm, v[i].ld, v[i].lh, v[i].lmi, v[i].ls, 321), u;
        SYSTEMTIME w = nls_st(v[i].uy, v[i].um, v[i].ud, v[i].uh, v[i].umi, v[i].us, 321);
        memset(&u, 0, sizeof u);
        CHECKF(U(TzSpecificLocalTimeToSystemTime)(&t, &l, &u) && nls_cmp_st(&u, &w), "row %d got %04d-%02d-%02d %02d:%02d:%02d.%03d", i, u.wYear, u.wMonth, u.wDay, u.wHour, u.wMinute, u.wSecond, u.wMilliseconds);
    }
}
TEST(tzcalc_local_to_utc_gap_and_overlap_us_pacific) {
    REQUIRE(U(TzSpecificLocalTimeToSystemTime));
    TIME_ZONE_INFORMATION t = mk_tzi(&ZONES[0]);
    /* 2024-03-10 02:00-02:59:59 does not exist; 2024-11-03 01:00-01:59:59 happens twice. */
    static const int gap[][3] = { { 2, 0, 0 }, { 2, 30, 0 }, { 2, 59, 59 } };
    for (int i = 0; i < 3; i++) {
        SYSTEMTIME l = nls_st(2024, 3, 10, gap[i][0], gap[i][1], gap[i][2], 0), u; SET_ERR(0);
        BOOL r = U(TzSpecificLocalTimeToSystemTime)(&t, &l, &u);
        CHECKF(r, "gap %d err %lu", i, GetLastError());
        if (r) {
            long long loc = st_to_secs(&l), got = st_to_secs(&u);
            CHECKF(got == loc + 8 * 3600 || got == loc + 7 * 3600, "gap row %d result off", i);
            t_note("gap local 02:%02d:%02d -> utc %02d:%02d:%02d", gap[i][1], gap[i][2], u.wHour, u.wMinute, u.wSecond);
        }
    }
    static const int ov[][3] = { { 1, 0, 0 }, { 1, 30, 0 }, { 1, 59, 59 } };
    for (int i = 0; i < 3; i++) {
        SYSTEMTIME l = nls_st(2024, 11, 3, ov[i][0], ov[i][1], ov[i][2], 0), u; SET_ERR(0);
        BOOL r = U(TzSpecificLocalTimeToSystemTime)(&t, &l, &u);
        CHECKF(r, "overlap %d err %lu", i, GetLastError());
        if (r) {
            long long loc = st_to_secs(&l), got = st_to_secs(&u);
            CHECKF(got == loc + 7 * 3600 || got == loc + 8 * 3600, "overlap row %d result off", i);
            t_note("overlap local 01:%02d:%02d -> utc %02d:%02d:%02d", ov[i][1], ov[i][2], u.wHour, u.wMinute, u.wSecond);
        }
    }
}
TEST(tzcalc_local_to_utc_invalid_systemtime) {
    REQUIRE(U(TzSpecificLocalTimeToSystemTime));
    TIME_ZONE_INFORMATION t = mk_tzi(&ZONES[0]);
    static const struct { int y, mo, d, h, mi, s; } bad[] = { { 2024, 0, 1, 0, 0, 0 }, { 2024, 13, 1, 0, 0, 0 }, { 2024, 1, 0, 0, 0, 0 }, { 2024, 2, 30, 0, 0, 0 },
        { 2024, 1, 1, 24, 0, 0 }, { 2024, 1, 1, 0, 60, 0 }, { 1600, 1, 1, 0, 0, 0 }, { 30828, 1, 1, 0, 0, 0 } };
    for (int i = 0; i < ARRAYLEN(bad); i++) {
        SYSTEMTIME l = nls_st(bad[i].y, bad[i].mo, bad[i].d, bad[i].h, bad[i].mi, bad[i].s, 0), u; SET_ERR(0);
        CHECKF(!U(TzSpecificLocalTimeToSystemTime)(&t, &l, &u), "row %d", i);
    }
}
TEST(tzcalc_local_to_utc_null_tzi) {
    REQUIRE(U(TzSpecificLocalTimeToSystemTime)); REQUIRE(U(SystemTimeToTzSpecificLocalTime));
    SYSTEMTIME u = nls_st(2024, 1, 15, 12, 0, 0, 0), l, back;
    CHECK(U(SystemTimeToTzSpecificLocalTime)(NULL, &u, &l));
    SET_ERR(0);
    BOOL r = U(TzSpecificLocalTimeToSystemTime)(NULL, &l, &back);
    CHECKF(r, "err %lu", GetLastError());
    if (r) CHECK(nls_cmp_st(&u, &back));
}
TEST(tzcalc_roundtrip_every_hour_2024_pacific) {
    REQUIRE(U(SystemTimeToTzSpecificLocalTime)); REQUIRE(U(TzSpecificLocalTimeToSystemTime));
    TIME_ZONE_INFORMATION t = mk_tzi(&ZONES[0]);
    long long base = days_from_civil(2024, 1, 1) * 86400; int bad = 0;
    for (int h = 0; h < 366 * 24; h++) {
        SYSTEMTIME u = secs_to_st(base + h * 3600LL), l, b;
        if (!U(SystemTimeToTzSpecificLocalTime)(&t, &u, &l)) { bad++; continue; }
        if (!U(TzSpecificLocalTimeToSystemTime)(&t, &l, &b)) { bad++; continue; }
        long long d = st_to_secs(&b) - st_to_secs(&u);
        if (d != 0 && !(u.wMonth == 11 && u.wDay == 3 && (d == 3600 || d == -3600))) bad++;
    }
    CHECKF(bad == 0, "bad=%d", bad);
}
TEST(tzcalc_roundtrip_every_hour_2024_sydney) {
    REQUIRE(U(SystemTimeToTzSpecificLocalTime)); REQUIRE(U(TzSpecificLocalTimeToSystemTime));
    TIME_ZONE_INFORMATION t = mk_tzi(&ZONES[5]);
    long long base = days_from_civil(2024, 1, 1) * 86400; int bad = 0;
    for (int h = 0; h < 366 * 24; h++) {
        SYSTEMTIME u = secs_to_st(base + h * 3600LL), l, b;
        if (!U(SystemTimeToTzSpecificLocalTime)(&t, &u, &l)) { bad++; continue; }
        if (!U(TzSpecificLocalTimeToSystemTime)(&t, &l, &b)) { bad++; continue; }
        long long d = st_to_secs(&b) - st_to_secs(&u);
        if (d != 0 && !((u.wMonth == 4 || u.wMonth == 10) && (d == 3600 || d == -3600))) bad++;
    }
    CHECKF(bad == 0, "bad=%d", bad);
}
TEST(tzcalc_absolute_dst_dates) {
    REQUIRE(U(SystemTimeToTzSpecificLocalTime));
    TIME_ZONE_INFORMATION t; memset(&t, 0, sizeof t);
    t.Bias = 480; t.DaylightBias = -60;
    t.DaylightDate = (SYSTEMTIME){ 2024, 4, 0, 7, 2, 0, 0, 0 };    /* absolute: 2024-04-07 02:00 */
    t.StandardDate = (SYSTEMTIME){ 2024, 10, 0, 6, 2, 0, 0, 0 };   /* absolute: 2024-10-06 02:00 */
    static const struct { SYSTEMTIME u; int lh; } v[] = {
        { { 2024, 1, 0, 15, 12 }, 4 }, { { 2024, 4, 0, 15, 12 }, 5 }, { { 2024, 7, 0, 15, 12 }, 5 }, { { 2024, 9, 0, 15, 12 }, 5 }, { { 2024, 11, 0, 15, 12 }, 4 },
        { { 2024, 4, 0, 7, 9, 59, 59 }, 1 }, { { 2024, 4, 0, 7, 10, 0, 0 }, 3 }, { { 2024, 10, 0, 6, 8, 59, 59 }, 1 }, { { 2024, 10, 0, 6, 9, 0, 0 }, 1 },
    };
    for (int i = 0; i < ARRAYLEN(v); i++) {
        SYSTEMTIME u = v[i].u; u.wDayOfWeek = 0; u.wDay = u.wDay ? u.wDay : 1;
        SYSTEMTIME l;
        BOOL r = U(SystemTimeToTzSpecificLocalTime)(&t, &u, &l);
        CHECKF(r, "row %d err %lu", i, GetLastError());
        if (r && i < 5) CHECKF(l.wHour == v[i].lh, "row %d hour %d want %d", i, l.wHour, v[i].lh);
        if (r && i >= 5) t_note("absolute rule row %d: local %02d:%02d:%02d", i, l.wHour, l.wMinute, l.wSecond);
    }
}
TEST(tzcalc_ambiguous_rules_dst_only_one_date) {
    REQUIRE(U(SystemTimeToTzSpecificLocalTime));
    /* StandardDate set but DaylightDate unset: no transitions, plain bias */
    TIME_ZONE_INFORMATION t; memset(&t, 0, sizeof t);
    t.Bias = 300; t.DaylightBias = -60; t.StandardDate.wMonth = 11; t.StandardDate.wDay = 1; t.StandardDate.wHour = 2;
    SYSTEMTIME u = nls_st(2024, 7, 15, 12, 0, 0, 0), l; SET_ERR(0);
    BOOL r = U(SystemTimeToTzSpecificLocalTime)(&t, &u, &l);
    t_note("only StandardDate set: r=%d hour=%d err=%lu", r, r ? l.wHour : -1, r ? 0 : GetLastError());
    if (r) CHECK(l.wHour == 7);
}
TEST(tzcalc_invalid_tzi_rules) {
    REQUIRE(U(SystemTimeToTzSpecificLocalTime));
    TIME_ZONE_INFORMATION t = mk_tzi(&ZONES[0]);
    SYSTEMTIME u = nls_st(2024, 7, 15, 12, 0, 0, 0), l;
    t.DaylightDate.wMonth = 13; SET_ERR(0);
    BOOL r = U(SystemTimeToTzSpecificLocalTime)(&t, &u, &l);
    t_note("DaylightDate month 13: r=%d err=%lu", r, r ? 0 : GetLastError());
    t = mk_tzi(&ZONES[0]); t.StandardDate.wDay = 6; SET_ERR(0);
    r = U(SystemTimeToTzSpecificLocalTime)(&t, &u, &l);
    t_note("StandardDate day 6: r=%d err=%lu", r, r ? 0 : GetLastError());
    t = mk_tzi(&ZONES[0]); t.DaylightDate.wHour = 25; SET_ERR(0);
    r = U(SystemTimeToTzSpecificLocalTime)(&t, &u, &l);
    t_note("DaylightDate hour 25: r=%d err=%lu", r, r ? 0 : GetLastError());
    t = mk_tzi(&ZONES[0]); t.DaylightDate.wDayOfWeek = 9; SET_ERR(0);
    r = U(SystemTimeToTzSpecificLocalTime)(&t, &u, &l);
    t_note("DaylightDate dow 9: r=%d err=%lu", r, r ? 0 : GetLastError());
    t = mk_tzi(&ZONES[0]); t.Bias = 0x7fffffff; SET_ERR(0);
    r = U(SystemTimeToTzSpecificLocalTime)(&t, &u, &l);
    t_note("huge bias: r=%d err=%lu", r, r ? 0 : GetLastError());
}
TEST(tzcalc_weekday_rule_with_dayofweek) {
    REQUIRE(U(SystemTimeToTzSpecificLocalTime));
    /* DST starts on the first Saturday of March (wDayOfWeek=6), ends on the last Friday of October */
    TIME_ZONE_INFORMATION t; memset(&t, 0, sizeof t);
    t.Bias = 0; t.DaylightBias = -60;
    t.DaylightDate.wMonth = 3; t.DaylightDate.wDayOfWeek = 6; t.DaylightDate.wDay = 1; t.DaylightDate.wHour = 0;
    t.StandardDate.wMonth = 10; t.StandardDate.wDayOfWeek = 5; t.StandardDate.wDay = 5; t.StandardDate.wHour = 1;
    /* 2024: first Saturday of March = Mar 2; last Friday of Oct = Oct 25 */
    SYSTEMTIME u, l;
    u = nls_st(2024, 3, 1, 23, 59, 59, 0); CHECK(U(SystemTimeToTzSpecificLocalTime)(&t, &u, &l)); CHECK(l.wHour == 23);
    u = nls_st(2024, 3, 2, 0, 0, 0, 0);    CHECK(U(SystemTimeToTzSpecificLocalTime)(&t, &u, &l)); CHECKF(l.wHour == 1 && l.wDay == 2, "got %d:%d day %d", l.wHour, l.wMinute, l.wDay);
    /* DST ends at 01:00 wall time on the last Friday: 2024-10-25 00:00 UTC */
    u = nls_st(2024, 10, 24, 23, 59, 59, 0); CHECK(U(SystemTimeToTzSpecificLocalTime)(&t, &u, &l)); CHECKF(l.wHour == 0 && l.wMinute == 59 && l.wDay == 25, "got %d:%d day %d", l.wHour, l.wMinute, l.wDay);
    u = nls_st(2024, 10, 25, 0, 0, 0, 0); CHECK(U(SystemTimeToTzSpecificLocalTime)(&t, &u, &l)); CHECKF(l.wHour == 0 && l.wMinute == 0 && l.wDay == 25, "got %d:%d day %d", l.wHour, l.wMinute, l.wDay);
    u = nls_st(2024, 10, 26, 0, 0, 0, 0); CHECK(U(SystemTimeToTzSpecificLocalTime)(&t, &u, &l)); CHECKF(l.wHour == 0 && l.wDay == 26, "got %d:%d day %d", l.wHour, l.wMinute, l.wDay);
}
