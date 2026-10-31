#include "tc_util.h"

static BOOL s2f(const SYSTEMTIME *st, FILETIME *ft) { return U(SystemTimeToFileTime)(st, ft); }
static BOOL f2s(const FILETIME *ft, SYSTEMTIME *st) { return U(FileTimeToSystemTime)(ft, st); }
static SYSTEMTIME mkst(int y, int mo, int d, int h, int mi, int s, int ms) {
    SYSTEMTIME st; memset(&st, 0, sizeof st);
    st.wYear = y; st.wMonth = mo; st.wDay = d; st.wHour = h; st.wMinute = mi; st.wSecond = s; st.wMilliseconds = ms; return st;
}

TEST(timeconv_epoch_1601_is_zero) {
    REQUIRE(U(SystemTimeToFileTime));
    SYSTEMTIME st = mkst(1601, 1, 1, 0, 0, 0, 0); FILETIME ft = tc_ft(~0ULL);
    SET_ERR(0);
    CHECK(s2f(&st, &ft));
    EXPECT_EQ(tc_u64(&ft), 0);
}
TEST(timeconv_epoch_1970) {
    REQUIRE(U(SystemTimeToFileTime));
    SYSTEMTIME st = mkst(1970, 1, 1, 0, 0, 0, 0); FILETIME ft;
    CHECK(s2f(&st, &ft));
    EXPECT_EQ(tc_u64(&ft), 116444736000000000ULL);
}
TEST(timeconv_known_vectors_s2f) {
    REQUIRE(U(SystemTimeToFileTime));
    struct { int y, mo, d, h, mi, s, ms; unsigned long long ft; } v[] = {
        {2000, 1, 1, 0, 0, 0, 0, 125911584000000000ULL},
        {2038, 1, 19, 3, 14, 7, 0, 137919572470000000ULL},
        {1601, 1, 1, 0, 0, 1, 0, 10000000ULL},
        {1601, 1, 1, 0, 0, 0, 1, 10000ULL},
        {1601, 1, 1, 0, 0, 0, 999, 9990000ULL},
        {1601, 1, 1, 0, 1, 0, 0, 600000000ULL},
        {1601, 1, 1, 1, 0, 0, 0, 36000000000ULL},
        {1601, 1, 2, 0, 0, 0, 0, 864000000000ULL},
        {1601, 2, 1, 0, 0, 0, 0, 31ULL * 864000000000ULL},
        {1980, 1, 1, 0, 0, 0, 0, 119600064000000000ULL},
        {1969, 12, 31, 23, 59, 59, 999, 116444736000000000ULL - 10000ULL},
    };
    for (size_t i = 0; i < sizeof v / sizeof v[0]; i++) {
        SYSTEMTIME st = mkst(v[i].y, v[i].mo, v[i].d, v[i].h, v[i].mi, v[i].s, v[i].ms); FILETIME ft = {0};
        BOOL r = s2f(&st, &ft);
        CHECKF(r, "vector %d", (int)i);
        CHECKF(tc_u64(&ft) == v[i].ft, "vector %d got %llu want %llu", (int)i, tc_u64(&ft), v[i].ft);
        CHECKF(tc_mkft(v[i].y, v[i].mo, v[i].d, v[i].h, v[i].mi, v[i].s, v[i].ms) == v[i].ft, "helper vector %d", (int)i);
    }
}
TEST(timeconv_known_vectors_f2s) {
    REQUIRE(U(FileTimeToSystemTime));
    struct { unsigned long long ft; int y, mo, d, dow, h, mi, s, ms; } v[] = {
        {0, 1601, 1, 1, 1, 0, 0, 0, 0},
        {116444736000000000ULL, 1970, 1, 1, 4, 0, 0, 0, 0},
        {125911584000000000ULL, 2000, 1, 1, 6, 0, 0, 0, 0},
        {137919572470000000ULL, 2038, 1, 19, 2, 3, 14, 7, 0},
        {9999ULL, 1601, 1, 1, 1, 0, 0, 0, 0},
        {10000ULL, 1601, 1, 1, 1, 0, 0, 0, 1},
        {9999999ULL, 1601, 1, 1, 1, 0, 0, 0, 999},
        {10000000ULL, 1601, 1, 1, 1, 0, 0, 1, 0},
        {116444736000000000ULL - 1, 1969, 12, 31, 3, 23, 59, 59, 999},
    };
    for (size_t i = 0; i < sizeof v / sizeof v[0]; i++) {
        FILETIME ft = tc_ft(v[i].ft); SYSTEMTIME st; memset(&st, 0xAA, sizeof st);
        BOOL r = f2s(&ft, &st);
        CHECKF(r, "vector %d", (int)i);
        CHECKF(st.wYear == v[i].y && st.wMonth == v[i].mo && st.wDay == v[i].d, "vector %d date %u-%u-%u", (int)i, st.wYear, st.wMonth, st.wDay);
        CHECKF(st.wDayOfWeek == v[i].dow, "vector %d dow %u want %d", (int)i, st.wDayOfWeek, v[i].dow);
        CHECKF(st.wHour == v[i].h && st.wMinute == v[i].mi && st.wSecond == v[i].s && st.wMilliseconds == v[i].ms, "vector %d time", (int)i);
    }
}
TEST(timeconv_max_filetime) {
    REQUIRE(U(FileTimeToSystemTime)); REQUIRE(U(SystemTimeToFileTime));
    FILETIME ft = tc_ft(TC_MAXFT); SYSTEMTIME st;
    CHECK(f2s(&ft, &st));
    EXPECT_EQ(st.wYear, 30828); EXPECT_EQ(st.wMonth, 9); EXPECT_EQ(st.wDay, 14);
    EXPECT_EQ(st.wHour, 2); EXPECT_EQ(st.wMinute, 48); EXPECT_EQ(st.wSecond, 5); EXPECT_EQ(st.wMilliseconds, 477);
    EXPECT_EQ(st.wDayOfWeek, tc_dow(30828, 9, 14));
    /* converting the truncated value back gives max minus the 0.4775807 -> .477 remainder */
    /* RtlTimeFieldsToTime accepts years 1601..30827 only (Year-1601 < 0x722b) */
    FILETIME back = tc_ft(77);
    SET_ERR(0); CHECK(!s2f(&st, &back)); EXPECT_ERR(ERROR_INVALID_PARAMETER); EXPECT_EQ(tc_u64(&back), 77);
}
TEST(timeconv_negative_filetime_fails_87) {
    REQUIRE(U(FileTimeToSystemTime));
    unsigned long long vals[] = {0x8000000000000000ULL, 0x8000000000000001ULL, 0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFF00000000ULL, 0x9000000000000000ULL};
    for (int i = 0; i < 5; i++) {
        FILETIME ft = tc_ft(vals[i]); SYSTEMTIME st; memset(&st, 0x55, sizeof st);
        SET_ERR(0);
        BOOL r = f2s(&ft, &st);
        CHECKF(!r, "val %d", i); EXPECT_ERR(ERROR_INVALID_PARAMETER);
        CHECKF(st.wYear == 0x5555, "output untouched on failure %d", i);
    }
}
TEST(timeconv_filetime_boundary_near_max) {
    REQUIRE(U(FileTimeToSystemTime));
    SYSTEMTIME st; FILETIME ft = tc_ft(TC_MAXFT - 1);
    CHECK(f2s(&ft, &st)); EXPECT_EQ(st.wYear, 30828);
    ft = tc_ft(0x7FFFFFFF00000000ULL); CHECK(f2s(&ft, &st)); EXPECT_EQ(st.wYear, 30828);
    ft = tc_ft(1ULL << 62); CHECK(f2s(&ft, &st)); t_note("1<<62 => year %u", st.wYear);
    CHECK(st.wYear > 14000 && st.wYear < 20000);
}
TEST(timeconv_s2f_invalid_fields_87) {
    REQUIRE(U(SystemTimeToFileTime));
    struct { const char *n; int a[8]; } v[] = {
        {"month0", {2020, 0, 0, 1, 0, 0, 0, 0}},
        {"month13", {2020, 0, 13, 1, 0, 0, 0, 0}},
        {"month65535", {2020, 0, 0xFFFF, 1, 0, 0, 0, 0}},
        {"day0", {2020, 0, 1, 0, 0, 0, 0, 0}},
        {"day32", {2020, 0, 1, 32, 0, 0, 0, 0}},
        {"jan32", {2020, 0, 1, 32, 12, 0, 0, 0}},
        {"apr31", {2020, 0, 4, 31, 0, 0, 0, 0}},
        {"jun31", {2020, 0, 6, 31, 0, 0, 0, 0}},
        {"sep31", {2020, 0, 9, 31, 0, 0, 0, 0}},
        {"nov31", {2020, 0, 11, 31, 0, 0, 0, 0}},
        {"feb30", {2020, 0, 2, 30, 0, 0, 0, 0}},
        {"feb29-2021", {2021, 0, 2, 29, 0, 0, 0, 0}},
        {"feb29-1900", {1900, 0, 2, 29, 0, 0, 0, 0}},
        {"feb29-2100", {2100, 0, 2, 29, 0, 0, 0, 0}},
        {"feb29-1700", {1700, 0, 2, 29, 0, 0, 0, 0}},
        {"hour24", {2020, 0, 1, 1, 24, 0, 0, 0}},
        {"hour65535", {2020, 0, 1, 1, 0xFFFF, 0, 0, 0}},
        {"minute60", {2020, 0, 1, 1, 0, 60, 0, 0}},
        {"second61", {2020, 0, 1, 1, 0, 0, 61, 0}},
        {"ms1000", {2020, 0, 1, 1, 0, 0, 0, 1000}},
        {"ms65535", {2020, 0, 1, 1, 0, 0, 0, 0xFFFF}},
        {"year1600", {1600, 0, 12, 31, 23, 59, 59, 999}},
        {"year0", {0, 0, 1, 1, 0, 0, 0, 0}},
        {"year1", {1, 0, 1, 1, 0, 0, 0, 0}},
        {"year1600jan1", {1600, 0, 1, 1, 0, 0, 0, 0}},
        {"all-zero", {0, 0, 0, 0, 0, 0, 0, 0}},
    };
    for (size_t i = 0; i < sizeof v / sizeof v[0]; i++) {
        FILETIME ft = tc_ft(0x1234567812345678ULL);
        SYSTEMTIME vs = mkst(v[i].a[0], v[i].a[2], v[i].a[3], v[i].a[4], v[i].a[5], v[i].a[6], v[i].a[7]);
        SET_ERR(0);
        BOOL r = s2f(&vs, &ft);
        CHECKF(!r, "%s should fail", v[i].n);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "%s err %lu", v[i].n, GetLastError());
        CHECKF(tc_u64(&ft) == 0x1234567812345678ULL, "%s output untouched", v[i].n);
    }
}
TEST(timeconv_s2f_second60_note) {
    REQUIRE(U(SystemTimeToFileTime));
    /* seconds==60 is only meaningful for leap-second aware processes on a real leap-second date */
    SYSTEMTIME st = mkst(2021, 3, 3, 0, 0, 60, 0); FILETIME ft = {0}; SET_ERR(0);
    BOOL r = s2f(&st, &ft);
    t_note("second=60 on a non-leap date -> %d err %lu", r, r ? 0 : GetLastError());
    st = mkst(2021, 3, 3, 0, 0, 61, 0); SET_ERR(0); CHECK(!s2f(&st, &ft)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(timeconv_s2f_dayofweek_ignored) {
    REQUIRE(U(SystemTimeToFileTime));
    for (int dow = 0; dow < 7; dow++) {
        SYSTEMTIME st = mkst(2021, 6, 15, 12, 30, 45, 123); st.wDayOfWeek = dow; FILETIME ft;
        CHECK(s2f(&st, &ft)); EXPECT_EQ(tc_u64(&ft), tc_mkft(2021, 6, 15, 12, 30, 45, 123));
    }
    SYSTEMTIME st = mkst(2021, 6, 15, 0, 0, 0, 0); st.wDayOfWeek = 0xFFFF; FILETIME ft;
    CHECK(s2f(&st, &ft));
}
TEST(timeconv_f2s_dayofweek_computed) {
    REQUIRE(U(FileTimeToSystemTime));
    for (int d = 0; d < 14; d++) {
        FILETIME ft = tc_ft(tc_mkft(2021, 1, 1 + d, 0, 0, 0, 0)); SYSTEMTIME st;
        CHECK(f2s(&ft, &st));
        EXPECT_EQ(st.wDayOfWeek, tc_dow(2021, 1, 1 + d));
    }
    FILETIME ft = tc_ft(tc_mkft(2021, 1, 1, 0, 0, 0, 0)); SYSTEMTIME st; f2s(&ft, &st);
    EXPECT_EQ(st.wDayOfWeek, 5); /* Friday */
}
TEST(timeconv_leap_year_rules) {
    REQUIRE(U(SystemTimeToFileTime)); REQUIRE(U(FileTimeToSystemTime));
    int years[] = {1604, 1700, 1800, 1900, 1996, 2000, 2004, 2019, 2020, 2023, 2024, 2100, 2200, 2400, 3000, 4000, 9999, 10000, 12000, 30000};
    for (size_t i = 0; i < sizeof years / sizeof years[0]; i++) {
        int y = years[i]; SYSTEMTIME st = mkst(y, 2, 29, 0, 0, 0, 0); FILETIME ft;
        SET_ERR(0);
        BOOL r = s2f(&st, &ft);
        CHECKF(r == tc_leap(y), "year %d feb29 valid=%d", y, tc_leap(y));
        if (!r) EXPECT_ERR(87);
        if (r) {
            SYSTEMTIME rt; CHECK(f2s(&ft, &rt));
            CHECKF(rt.wMonth == 2 && rt.wDay == 29, "year %d roundtrip", y);
            EXPECT_EQ(tc_u64(&ft), tc_mkft(y, 2, 29, 0, 0, 0, 0));
        }
        st = mkst(y, 2, 28, 23, 59, 59, 999); r = s2f(&st, &ft); CHECKF(r, "feb28 %d", y);
        if (r) {
            unsigned long long next = tc_u64(&ft) + 10000ULL; FILETIME nf = tc_ft(next); SYSTEMTIME ns; CHECK(f2s(&nf, &ns));
            CHECKF(tc_leap(y) ? (ns.wMonth == 2 && ns.wDay == 29) : (ns.wMonth == 3 && ns.wDay == 1), "year %d day after feb28", y);
        }
    }
}
TEST(timeconv_days_in_month_table) {
    REQUIRE(U(SystemTimeToFileTime));
    int ys[] = {1601, 1900, 2000, 2021, 2024, 2100};
    for (size_t k = 0; k < 6; k++) for (int m = 1; m <= 12; m++) {
        int dim = tc_dim(ys[k], m); FILETIME ft;
        SYSTEMTIME a = mkst(ys[k], m, dim, 23, 59, 59, 999), b = mkst(ys[k], m, dim + 1, 0, 0, 0, 0);
        CHECKF(s2f(&a, &ft), "%d-%d-%d valid", ys[k], m, dim);
        SET_ERR(0);
        CHECKF(!s2f(&b, &ft), "%d-%d-%d invalid", ys[k], m, dim + 1);
        EXPECT_ERR(87);
    }
}
TEST(timeconv_every_day_of_year_sequential) {
    REQUIRE(U(SystemTimeToFileTime)); REQUIRE(U(FileTimeToSystemTime));
    int years[] = {1999, 2000, 2024, 2100};
    for (size_t k = 0; k < 4; k++) {
        int y = years[k]; unsigned long long prev = 0; int count = 0;
        for (int m = 1; m <= 12; m++) for (int d = 1; d <= tc_dim(y, m); d++) {
            SYSTEMTIME st = mkst(y, m, d, 0, 0, 0, 0), rt; FILETIME ft;
            if (!s2f(&st, &ft)) { CHECKF(0, "%d-%d-%d", y, m, d); continue; }
            unsigned long long v = tc_u64(&ft);
            if (count) EXPECT_EQ(v - prev, 864000000000ULL);
            prev = v; count++;
            f2s(&ft, &rt);
            CHECK(rt.wYear == y && rt.wMonth == m && rt.wDay == d);
            CHECK(rt.wDayOfWeek == tc_dow(y, m, d));
        }
        EXPECT_EQ(count, tc_leap(y) ? 366 : 365);
    }
}
TEST(timeconv_millisecond_sweep) {
    REQUIRE(U(SystemTimeToFileTime)); REQUIRE(U(FileTimeToSystemTime));
    for (int ms = 0; ms < 1000; ms += 7) {
        SYSTEMTIME st = mkst(2010, 5, 5, 5, 5, 5, ms), rt; FILETIME ft;
        CHECK(s2f(&st, &ft)); EXPECT_EQ(tc_u64(&ft), tc_mkft(2010, 5, 5, 5, 5, 5, ms));
        CHECK(f2s(&ft, &rt)); EXPECT_EQ(rt.wMilliseconds, ms);
    }
    /* sub-ms ticks truncate */
    FILETIME ft = tc_ft(tc_mkft(2010, 1, 1, 0, 0, 0, 5) + 9999); SYSTEMTIME rt; CHECK(f2s(&ft, &rt)); EXPECT_EQ(rt.wMilliseconds, 5);
    ft = tc_ft(tc_mkft(2010, 1, 1, 0, 0, 0, 5) + 10000); CHECK(f2s(&ft, &rt)); EXPECT_EQ(rt.wMilliseconds, 6);
}
TEST(timeconv_field_extremes_valid) {
    REQUIRE(U(SystemTimeToFileTime));
    SYSTEMTIME st = mkst(2020, 12, 31, 23, 59, 59, 999); FILETIME ft;
    CHECK(s2f(&st, &ft)); EXPECT_EQ(tc_u64(&ft), tc_mkft(2020, 12, 31, 23, 59, 59, 999));
    st = mkst(1601, 1, 1, 0, 0, 0, 0); CHECK(s2f(&st, &ft));
    st = mkst(1601, 12, 31, 23, 59, 59, 999); CHECK(s2f(&st, &ft)); EXPECT_EQ(tc_u64(&ft), 365ULL * 864000000000ULL - 10000ULL);
    st = mkst(30827, 12, 31, 23, 59, 59, 999); CHECK(s2f(&st, &ft)); EXPECT_EQ(tc_u64(&ft), tc_mkft(30827, 12, 31, 23, 59, 59, 999));
}
TEST(timeconv_year_30828_s2f_edge) {
    REQUIRE(U(SystemTimeToFileTime));
    SYSTEMTIME st = mkst(30828, 9, 14, 2, 48, 5, 477); FILETIME ft = {0};
    SET_ERR(0); BOOL r = s2f(&st, &ft);
    CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    st = mkst(30828, 1, 1, 0, 0, 0, 0); SET_ERR(0); r = s2f(&st, &ft); CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    st = mkst(65535, 12, 31, 23, 59, 59, 999); SET_ERR(0); r = s2f(&st, &ft); CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    st = mkst(30827, 1, 1, 0, 0, 0, 0); CHECK(s2f(&st, &ft)); CHECK((long long)tc_u64(&ft) > 0);
}
TEST(timeconv_null_filetime_out_not_tested_in_range) {
    REQUIRE(U(SystemTimeToFileTime));
    /* output is written only on success: invalid input leaves buffer intact */
    SYSTEMTIME st = mkst(2020, 13, 1, 0, 0, 0, 0); FILETIME ft = tc_ft(77);
    CHECK(!s2f(&st, &ft)); EXPECT_EQ(tc_u64(&ft), 77);
}

/* ---- Local file time ---- */
TEST(timeconv_local_roundtrip) {
    REQUIRE(U(FileTimeToLocalFileTime)); REQUIRE(U(LocalFileTimeToFileTime));
    unsigned long long vals[] = {TC_EPOCH, 125911584000000000ULL, 132000000000000000ULL, 200000000000000000ULL, 1ULL << 60};
    for (int i = 0; i < 5; i++) {
        FILETIME ft = tc_ft(vals[i]), lf = {0}, back = {0};
        CHECK(U(FileTimeToLocalFileTime)(&ft, &lf));
        CHECK(U(LocalFileTimeToFileTime)(&lf, &back));
        EXPECT_EQ(tc_u64(&back), vals[i]);
        long long diff = (long long)tc_u64(&lf) - (long long)vals[i];
        CHECKF(diff % 600000000LL == 0 || diff % 18000000000LL == 0 || diff % 9000000000LL == 0, "diff %lld not a minute multiple", diff);
        CHECKF(diff >= -14LL * 36000000000LL && diff <= 14LL * 36000000000LL, "bias plausible %lld", diff);
    }
}
TEST(timeconv_local_bias_is_constant) {
    REQUIRE(U(FileTimeToLocalFileTime)); REQUIRE(U(LocalFileTimeToFileTime));
    FILETIME a = tc_ft(TC_EPOCH), b = tc_ft(TC_EPOCH + 123456789ULL), la, lb;
    CHECK(U(FileTimeToLocalFileTime)(&a, &la)); CHECK(U(FileTimeToLocalFileTime)(&b, &lb));
    EXPECT_EQ(tc_u64(&lb) - tc_u64(&la), 123456789ULL);
    /* applying the conversion twice gives twice the bias */
    FILETIME l2; CHECK(U(FileTimeToLocalFileTime)(&la, &l2));
    EXPECT_EQ(tc_u64(&l2) - tc_u64(&la), tc_u64(&la) - tc_u64(&a));
}
TEST(timeconv_local_in_place) {
    REQUIRE(U(FileTimeToLocalFileTime)); REQUIRE(U(LocalFileTimeToFileTime));
    FILETIME ft = tc_ft(TC_EPOCH), ref; CHECK(U(FileTimeToLocalFileTime)(&ft, &ref));
    CHECK(U(FileTimeToLocalFileTime)(&ft, &ft)); EXPECT_EQ(tc_u64(&ft), tc_u64(&ref));
    CHECK(U(LocalFileTimeToFileTime)(&ft, &ft)); EXPECT_EQ(tc_u64(&ft), TC_EPOCH);
}
TEST(timeconv_local_matches_localtime) {
    REQUIRE(U(FileTimeToLocalFileTime)); REQUIRE(U(GetLocalTime)); REQUIRE(U(GetSystemTime));
    for (int tries = 0; tries < 5; tries++) {
        SYSTEMTIME sys, loc; U(GetSystemTime)(&sys); U(GetLocalTime)(&loc);
        FILETIME fs, fl, lf; s2f(&sys, &fs); s2f(&loc, &fl);
        U(FileTimeToLocalFileTime)(&fs, &lf);
        long long d = (long long)tc_u64(&fl) - (long long)tc_u64(&lf);
        if (d < 0) d = -d;
        if (d < 5000000LL || tries == 4) { CHECKF(d < 5000000LL || d % 36000000000LL == 0, "local differs by %lld", d); if (d >= 5000000LL) t_note("differs by DST-ish %lld", d); break; }
    }
}
TEST(timeconv_local_no_validation_negative) {
    REQUIRE(U(FileTimeToLocalFileTime)); REQUIRE(U(LocalFileTimeToFileTime));
    /* the decomp does plain subtraction and always returns TRUE */
    FILETIME ft = tc_ft(0x8000000000000000ULL), out = {0};
    SET_ERR(0);
    CHECK(U(FileTimeToLocalFileTime)(&ft, &out));
    ft = tc_ft(0); CHECK(U(FileTimeToLocalFileTime)(&ft, &out));
    CHECK(U(LocalFileTimeToFileTime)(&ft, &out));
    ft = tc_ft(~0ULL); CHECK(U(LocalFileTimeToFileTime)(&ft, &out));
    ft = tc_ft(TC_MAXFT); CHECK(U(LocalFileTimeToFileTime)(&ft, &out));
}
TEST(timeconv_local_wraps_at_edges) {
    REQUIRE(U(FileTimeToLocalFileTime)); REQUIRE(U(LocalFileTimeToFileTime));
    FILETIME zero = tc_ft(0), lz, back;
    CHECK(U(FileTimeToLocalFileTime)(&zero, &lz));
    CHECK(U(LocalFileTimeToFileTime)(&lz, &back)); EXPECT_EQ(tc_u64(&back), 0);
    FILETIME mx = tc_ft(TC_MAXFT), lm; CHECK(U(FileTimeToLocalFileTime)(&mx, &lm));
    CHECK(U(LocalFileTimeToFileTime)(&lm, &back)); EXPECT_EQ(tc_u64(&back), TC_MAXFT);
}

/* ---- DOS date time ---- */
static WORD dd(int y, int m, int d) { return (WORD)(((y - 1980) << 9) | (m << 5) | d); }
static WORD dt(int h, int mi, int s) { return (WORD)((h << 11) | (mi << 5) | (s / 2)); }

TEST(timeconv_dos_to_ft_vectors) {
    REQUIRE(U(DosDateTimeToFileTime));
    struct { int y, mo, d, h, mi, s; } v[] = {
        {1980, 1, 1, 0, 0, 0}, {1980, 1, 1, 0, 0, 2}, {1999, 12, 31, 23, 59, 58}, {2000, 2, 29, 12, 0, 0},
        {2024, 2, 29, 1, 2, 4}, {2107, 12, 31, 23, 59, 58}, {2020, 6, 15, 13, 45, 30}, {1980, 2, 29, 0, 0, 0},
    };
    for (size_t i = 0; i < sizeof v / sizeof v[0]; i++) {
        FILETIME ft = {0};
        BOOL r = U(DosDateTimeToFileTime)(dd(v[i].y, v[i].mo, v[i].d), dt(v[i].h, v[i].mi, v[i].s), &ft);
        CHECKF(r, "vector %d err %lu", (int)i, GetLastError());
        CHECKF(tc_u64(&ft) == tc_mkft(v[i].y, v[i].mo, v[i].d, v[i].h, v[i].mi, v[i].s, 0), "vector %d got %llu", (int)i, tc_u64(&ft));
    }
}
TEST(timeconv_dos_raw_constants) {
    REQUIRE(U(DosDateTimeToFileTime));
    FILETIME ft;
    CHECK(U(DosDateTimeToFileTime)(0x0021, 0x0000, &ft)); EXPECT_EQ(tc_u64(&ft), 119600064000000000ULL);
    CHECK(U(DosDateTimeToFileTime)(0xFF9F, 0xBF7D, &ft)); EXPECT_EQ(tc_u64(&ft), tc_mkft(2107, 12, 31, 23, 59, 58, 0));
    CHECK(U(DosDateTimeToFileTime)(0x5A6F, 0x6C3D, &ft));
}
TEST(timeconv_dos_invalid_fields) {
    REQUIRE(U(DosDateTimeToFileTime));
    struct { const char *n; WORD d, t; } v[] = {
        {"zero-date", 0x0000, 0}, {"month0", (WORD)((20 << 9) | (0 << 5) | 1), 0}, {"day0", (WORD)((20 << 9) | (1 << 5) | 0), 0},
        {"month13", (WORD)((20 << 9) | (13 << 5) | 1), 0}, {"month15", (WORD)((20 << 9) | (15 << 5) | 1), 0},
        {"feb30", dd(2020, 2, 30), 0}, {"feb29-2021", dd(2021, 2, 29), 0}, {"feb29-2100", dd(2100, 2, 29), 0},
        {"apr31", dd(2020, 4, 31), 0}, {"hour24", dd(2020, 1, 1), (WORD)(24 << 11)}, {"hour31", dd(2020, 1, 1), (WORD)(31 << 11)},
        {"minute60", dd(2020, 1, 1), (WORD)(60 << 5)}, {"minute63", dd(2020, 1, 1), (WORD)(63 << 5)},
        {"sec30x2", dd(2020, 1, 1), 30}, {"sec31x2", dd(2020, 1, 1), 31},
    };
    for (size_t i = 0; i < sizeof v / sizeof v[0]; i++) {
        FILETIME ft = tc_ft(99); SET_ERR(0);
        BOOL r = U(DosDateTimeToFileTime)(v[i].d, v[i].t, &ft);
        CHECKF(!r, "%s should fail", v[i].n);
        if (GetLastError() != ERROR_INVALID_PARAMETER) t_note("%s err %lu (expected 87)", v[i].n, GetLastError());
        CHECKF(tc_u64(&ft) == 99, "%s untouched", v[i].n);
    }
}
TEST(timeconv_dos_roundtrip_exhaustive_days) {
    REQUIRE(U(DosDateTimeToFileTime)); REQUIRE(U(FileTimeToDosDateTime));
    int n = 0;
    for (int y = 1980; y <= 2107; y += 3) for (int m = 1; m <= 12; m++) for (int d = 1; d <= tc_dim(y, m); d += 5) {
        FILETIME ft; WORD fd = 0, ftm = 0;
        WORD wd = dd(y, m, d), wt = dt((y + d) % 24, (m * 7 + d) % 60, ((y + m) % 30) * 2);
        if (!U(DosDateTimeToFileTime)(wd, wt, &ft)) { CHECKF(0, "dos->ft %d-%d-%d", y, m, d); continue; }
        if (!U(FileTimeToDosDateTime)(&ft, &fd, &ftm)) { CHECKF(0, "ft->dos %d-%d-%d", y, m, d); continue; }
        CHECKF(fd == wd && ftm == wt, "%d-%d-%d: %04x %04x vs %04x %04x", y, m, d, fd, ftm, wd, wt);
        n++;
    }
    CHECK(n > 1000);
}
TEST(timeconv_ft_to_dos_rounding) {
    REQUIRE(U(FileTimeToDosDateTime));
    /* implementation adds 19999999 ticks first: results round UP to the next even second */
    unsigned long long base = tc_mkft(2020, 6, 15, 10, 20, 30, 0);
    struct { long long delta; int sec; } v[] = {{0, 30}, {1, 32}, {10000000LL, 32}, {10000001LL, 32}, {19999999LL, 32}, {20000000LL, 32}, {20000001LL, 34}, {-1, 30}, {-10000000LL, 30}, {-10000001LL, 30}, {-20000000LL, 28}};
    for (size_t i = 0; i < sizeof v / sizeof v[0]; i++) {
        FILETIME ft = tc_ft(base + v[i].delta); WORD fd = 0, ftm = 0;
        BOOL r = U(FileTimeToDosDateTime)(&ft, &fd, &ftm);
        CHECKF(r, "delta %lld", v[i].delta);
        CHECKF((ftm & 0x1f) * 2 == v[i].sec, "delta %lld sec %d want %d", v[i].delta, (ftm & 0x1f) * 2, v[i].sec);
        CHECKF(fd == dd(2020, 6, 15) && (ftm >> 11) == 10 && ((ftm >> 5) & 63) == 20, "delta %lld rest", v[i].delta);
    }
}
TEST(timeconv_ft_to_dos_range) {
    REQUIRE(U(FileTimeToDosDateTime));
    WORD fd, ft_;
    FILETIME f = tc_ft(tc_mkft(1980, 1, 1, 0, 0, 0, 0)); SET_ERR(0);
    CHECK(U(FileTimeToDosDateTime)(&f, &fd, &ft_)); EXPECT_EQ(fd, 0x21); EXPECT_EQ(ft_, 0);
    f = tc_ft(tc_mkft(1979, 12, 31, 23, 59, 58, 0)); SET_ERR(0);
    CHECK(!U(FileTimeToDosDateTime)(&f, &fd, &ft_)); t_note("out-of-range dos err %lu", GetLastError());
    f = tc_ft(tc_mkft(2107, 12, 31, 23, 59, 58, 0)); CHECK(U(FileTimeToDosDateTime)(&f, &fd, &ft_)); EXPECT_EQ(fd, 0xFF9F); EXPECT_EQ(ft_, 0xBF7D);
    f = tc_ft(tc_mkft(2108, 1, 1, 0, 0, 0, 0)); SET_ERR(0);
    CHECK(!U(FileTimeToDosDateTime)(&f, &fd, &ft_)); t_note("out-of-range dos err %lu", GetLastError());
    f = tc_ft(0); SET_ERR(0); CHECK(!U(FileTimeToDosDateTime)(&f, &fd, &ft_)); t_note("out-of-range dos err %lu", GetLastError());
    f = tc_ft(TC_EPOCH); CHECK(!U(FileTimeToDosDateTime)(&f, &fd, &ft_)); /* 1970 < 1980 */
    f = tc_ft(TC_MAXFT - 40000000ULL); SET_ERR(0); CHECK(!U(FileTimeToDosDateTime)(&f, &fd, &ft_));
}
TEST(timeconv_ft_to_dos_negative_87) {
    REQUIRE(U(FileTimeToDosDateTime));
    WORD fd = 0x1111, ftm = 0x2222;
    FILETIME f = tc_ft(0x8000000000000000ULL); SET_ERR(0);
    CHECK(!U(FileTimeToDosDateTime)(&f, &fd, &ftm)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    f = tc_ft(~0ULL); SET_ERR(0);
    CHECK(!U(FileTimeToDosDateTime)(&f, &fd, &ftm)); t_note("ft=-1 err %lu", GetLastError());
    EXPECT_EQ(fd, 0x1111); EXPECT_EQ(ftm, 0x2222);
}
TEST(timeconv_ft_to_dos_max_rounding_no_overflow) {
    REQUIRE(U(FileTimeToDosDateTime));
    WORD fd, ftm; FILETIME f = tc_ft(TC_MAXFT); SET_ERR(0);
    BOOL r = U(FileTimeToDosDateTime)(&f, &fd, &ftm);
    CHECK(!r); /* adding 19999999 overflows to negative -> 87; either way out of range */
    t_note("max ft dos err %lu", GetLastError());
}

/* ---- CompareFileTime ---- */
TEST(timeconv_compare_basic) {
    REQUIRE(U(CompareFileTime));
    FILETIME a = tc_ft(5), b = tc_ft(6), c = tc_ft(5);
    EXPECT_EQ(U(CompareFileTime)(&a, &b), -1); EXPECT_EQ(U(CompareFileTime)(&b, &a), 1);
    EXPECT_EQ(U(CompareFileTime)(&a, &c), 0); EXPECT_EQ(U(CompareFileTime)(&a, &a), 0);
}
TEST(timeconv_compare_high_low_dword) {
    REQUIRE(U(CompareFileTime));
    FILETIME a = tc_ft(0x00000001FFFFFFFFULL), b = tc_ft(0x0000000200000000ULL);
    EXPECT_EQ(U(CompareFileTime)(&a, &b), -1); EXPECT_EQ(U(CompareFileTime)(&b, &a), 1);
    FILETIME c = tc_ft(0x0000000100000001ULL), d = tc_ft(0x00000001FFFFFFFFULL);
    EXPECT_EQ(U(CompareFileTime)(&c, &d), -1);
    FILETIME e = tc_ft(0x0000000100000000ULL), f = tc_ft(0x00000000FFFFFFFFULL);
    EXPECT_EQ(U(CompareFileTime)(&e, &f), 1);
}
TEST(timeconv_compare_unsigned_semantics) {
    REQUIRE(U(CompareFileTime));
    /* the implementation compares as unsigned 64-bit: "negative" values sort above positives */
    FILETIME big = tc_ft(0x8000000000000000ULL), small = tc_ft(1), mx = tc_ft(TC_MAXFT), all = tc_ft(~0ULL);
    EXPECT_EQ(U(CompareFileTime)(&big, &small), 1); EXPECT_EQ(U(CompareFileTime)(&small, &big), -1);
    EXPECT_EQ(U(CompareFileTime)(&big, &mx), 1); EXPECT_EQ(U(CompareFileTime)(&all, &big), 1);
    EXPECT_EQ(U(CompareFileTime)(&all, &all), 0);
    FILETIME zero = tc_ft(0); EXPECT_EQ(U(CompareFileTime)(&zero, &all), -1);
}
TEST(timeconv_compare_return_values_exact) {
    REQUIRE(U(CompareFileTime));
    unsigned long long s = 99; int bad = 0;
    for (int i = 0; i < 20000; i++) {
        FILETIME a = tc_ft(tc_rand64(&s)), b = tc_ft(i % 5 == 0 ? tc_u64(&a) : tc_rand64(&s));
        unsigned long long x = tc_u64(&a), y = tc_u64(&b);
        LONG exp = x < y ? -1 : (x > y ? 1 : 0);
        if (U(CompareFileTime)(&a, &b) != exp) bad++;
    }
    EXPECT_EQ(bad, 0);
}
TEST(timeconv_compare_sort_consistency) {
    REQUIRE(U(CompareFileTime));
    FILETIME v[50]; unsigned long long s = 5;
    for (int i = 0; i < 50; i++) v[i] = tc_ft(tc_rand64(&s) >> 3);
    for (int i = 0; i < 50; i++) for (int j = 0; j < 50; j++) {
        LONG ab = U(CompareFileTime)(&v[i], &v[j]), ba = U(CompareFileTime)(&v[j], &v[i]);
        CHECK(ab == -ba);
    }
}

/* ---- property tests ---- */
TEST(timeconv_property_roundtrip_random) {
    REQUIRE(U(FileTimeToSystemTime)); REQUIRE(U(SystemTimeToFileTime));
    unsigned long long s = 0x1234; int bad = 0, dowbad = 0, fieldbad = 0;
    for (int i = 0; i < 100000; i++) {
        unsigned long long v = tc_rand64(&s) % (TC_MAXFT);
        if (i % 3 == 0) v %= 3000000000000000ULL * 10 / 3; /* bias toward plausible years */
        FILETIME ft = tc_ft(v), back; SYSTEMTIME st;
        if (!f2s(&ft, &st)) { bad++; continue; }
        if (st.wMonth < 1 || st.wMonth > 12 || st.wDay < 1 || st.wDay > tc_dim(st.wYear, st.wMonth) || st.wHour > 23 || st.wMinute > 59 || st.wSecond > 59 || st.wMilliseconds > 999) fieldbad++;
        if (st.wDayOfWeek != tc_dow(st.wYear, st.wMonth, st.wDay)) dowbad++;
        if (st.wYear >= 30828) continue; /* SystemTimeToFileTime rejects year 30828 */
        if (!s2f(&st, &back)) { bad++; continue; }
        if (tc_u64(&back) != v - v % 10000) bad++;
        if (tc_u64(&back) != tc_mkft(st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds)) bad++;
    }
    EXPECT_EQ(bad, 0); EXPECT_EQ(dowbad, 0); EXPECT_EQ(fieldbad, 0);
}
TEST(timeconv_property_monotonic_mapping) {
    REQUIRE(U(FileTimeToSystemTime));
    unsigned long long s = 77; int bad = 0;
    for (int i = 0; i < 20000; i++) {
        unsigned long long a = tc_rand64(&s) % (1ULL << 60), b = a + (tc_rand64(&s) % 100000000000ULL);
        FILETIME fa = tc_ft(a), fb = tc_ft(b); SYSTEMTIME sa, sb; f2s(&fa, &sa); f2s(&fb, &sb);
        unsigned long long ka = tc_mkft(sa.wYear, sa.wMonth, sa.wDay, sa.wHour, sa.wMinute, sa.wSecond, sa.wMilliseconds);
        unsigned long long kb = tc_mkft(sb.wYear, sb.wMonth, sb.wDay, sb.wHour, sb.wMinute, sb.wSecond, sb.wMilliseconds);
        if (ka > kb) bad++;
    }
    EXPECT_EQ(bad, 0);
}
TEST(timeconv_property_consecutive_ms) {
    REQUIRE(U(FileTimeToSystemTime)); REQUIRE(U(SystemTimeToFileTime));
    /* walk across midnight / month / year boundaries ms by ms */
    unsigned long long pts[] = {tc_mkft(1999, 12, 31, 23, 59, 59, 995), tc_mkft(2000, 2, 28, 23, 59, 59, 995), tc_mkft(2100, 2, 28, 23, 59, 59, 995), tc_mkft(2024, 12, 31, 23, 59, 59, 995)};
    for (int p = 0; p < 4; p++) {
        unsigned long long prev = 0;
        for (int i = 0; i < 12; i++) {
            FILETIME ft = tc_ft(pts[p] + (unsigned long long)i * 10000ULL), b; SYSTEMTIME st;
            CHECK(f2s(&ft, &st)); CHECK(s2f(&st, &b));
            EXPECT_EQ(tc_u64(&b), tc_u64(&ft));
            if (i) EXPECT_EQ(tc_u64(&b) - prev, 10000);
            prev = tc_u64(&b);
        }
    }
}
TEST(timeconv_property_year_table_start) {
    REQUIRE(U(SystemTimeToFileTime)); REQUIRE(U(FileTimeToSystemTime));
    for (int y = 1601; y <= 2600; y++) {
        SYSTEMTIME st = mkst(y, 1, 1, 0, 0, 0, 0), rt; FILETIME ft;
        if (!s2f(&st, &ft)) { CHECKF(0, "year %d", y); continue; }
        CHECKF(tc_u64(&ft) == tc_mkft(y, 1, 1, 0, 0, 0, 0), "year %d", y);
        f2s(&ft, &rt); CHECKF(rt.wYear == y && rt.wDayOfWeek == tc_dow(y, 1, 1), "year %d dow", y);
    }
}
TEST(timeconv_dow_zeller_self_check) {
    CHECK(tc_dow(1601, 1, 1) == 1); CHECK(tc_dow(1970, 1, 1) == 4); CHECK(tc_dow(2000, 1, 1) == 6); CHECK(tc_dow(2024, 2, 29) == 4);
    CHECK(tc_dow(2100, 3, 1) == 1); CHECK(tc_dow(1900, 1, 1) == 1);
}
