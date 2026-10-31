/* Additional cross-checks: A/W/Ex parity, repeat stability, well-known en-US data. */
#include "nlsutil.h"

static const DWORD PARITY_TYPES[] = { L_SENGLANGNAME, L_SENGCOUNTRY, L_SISO639LANGNAME, L_SISO3166CTRYNAME, L_SDECIMAL,
    L_STHOUSAND, L_SGROUPING, L_SLIST, L_SDATE, L_STIME, L_S1159, L_S2359, L_SPOSITIVESIGN, L_SNEGATIVESIGN,
    L_IMEASURE, L_SINTLSYMBOL, L_SDAYNAME1, L_SMONTHNAME1, L_SABBREVMONTHNAME1 + 1, L_SNAME, L_SPARENT };

TEST(xtra_ex_vs_w_vs_a_parity_ascii_fields) {
    REQUIRE(U(GetLocaleInfoEx)); REQUIRE(U(GetLocaleInfoW)); REQUIRE(U(GetLocaleInfoA));
    for (int i = 0; i < ARRAYLEN(FIXED_LOCALES); i++) {
        for (int j = 0; j < ARRAYLEN(PARITY_TYPES); j++) {
            DWORD ty = PARITY_TYPES[j] | N_NOUSER;
            WCHAR e[128], w[128]; char a[256], a2[256];
            int re = U(GetLocaleInfoEx)(FIXED_LOCALES[i], ty, e, 128);
            int rw = U(GetLocaleInfoW)(FIXED_LCIDS[i], ty, w, 128);
            CHECKF(re == rw, "%d/0x%lx re=%d rw=%d", i, ty, re, rw);
            if (re > 0 && rw > 0) CHECKF(!lstrcmpW(e, w), "%d/0x%lx Ex/W text", i, ty);
            int ra = U(GetLocaleInfoA)(FIXED_LCIDS[i], ty, a, 256);
            if (rw > 0 && nls_isascii(w)) {
                CHECKF(ra == rw, "%d/0x%lx ra=%d rw=%d", i, ty, ra, rw);
                nls_w2a(w, a2, 256);
                CHECKF(!strcmp(a, a2), "%d/0x%lx A/W text", i, ty);
            }
        }
    }
}
TEST(xtra_enus_well_known_values) {
    REQUIRE(U(GetLocaleInfoEx));
    WCHAR b[128];
    struct { DWORD ty; const WCHAR *v; } t[] = {
        { L_SNAME, L"en-US" }, { L_SISO639LANGNAME, L"en" }, { L_SISO3166CTRYNAME, L"US" }, { L_SDECIMAL, L"." },
        { L_STHOUSAND, L"," }, { L_SGROUPING, L"3;0" }, { L_SLIST, L"," }, { L_SDATE, L"/" }, { L_STIME, L":" },
        { L_S1159, L"AM" }, { L_S2359, L"PM" }, { L_SDAYNAME1, L"Monday" }, { L_SDAYNAME1 + 6, L"Sunday" },
        { L_SMONTHNAME1, L"January" }, { L_SMONTHNAME1 + 11, L"December" }, { L_SABBREVMONTHNAME1, L"Jan" },
        { L_SABBREVDAYNAME1, L"Mon" }, { L_SPOSITIVESIGN, L"" }, { L_SNEGATIVESIGN, L"-" }, { L_SINTLSYMBOL, L"USD" },
        { L_SENGLANGNAME, L"English" }, { L_SENGCOUNTRY, L"United States" }, { L_SSHORTDATE, L"M/d/yyyy" },
        { L_SLONGDATE, L"dddd, MMMM d, yyyy" }, { L_STIMEFORMAT, L"h:mm:ss tt" },
    };
    for (int i = 0; i < ARRAYLEN(t); i++) {
        int r = nls_str(L"en-US", t[i].ty, b, 128);
        CHECKF(r > 0, "type 0x%lx r=%d", t[i].ty, r);
        if (r > 0) EXPECT_WS(1, L"en-US", "en-US value", b, t[i].v);
    }
}
TEST(xtra_enus_numeric_fields) {
    REQUIRE(U(GetLocaleInfoEx));
    struct { DWORD ty; DWORD v; } t[] = {
        { L_ILANGUAGE, 0x409 }, { L_ICOUNTRY, 1 }, { L_IMEASURE, 1 }, { L_IDEFAULTANSICP, 1252 }, { L_IDEFAULTCODEPAGE, 437 },
        { L_IGEOID, 244 }, { L_IPAPERSIZE, 1 }, { L_IDIGITS, 2 }, { L_ILZERO, 1 }, { L_IDATE, 0 }, { L_ITIME, 0 },
        { L_ITLZERO, 0 }, { L_IFIRSTDAYOFWEEK, 6 }, { L_INEGNUMBER, 1 }, { L_ICURRDIGITS, 2 },
    };
    for (int i = 0; i < ARRAYLEN(t); i++) {
        int ok; DWORD v = nls_num(L"en-US", t[i].ty, &ok);
        CHECKF(ok, "type 0x%lx", t[i].ty);
        if (ok) CHECKF(v == t[i].v, "type 0x%lx got %lu want %lu", t[i].ty, v, t[i].v);
    }
}
TEST(xtra_size_query_equals_fill_for_every_type) {
    REQUIRE(U(GetLocaleInfoEx));
    for (int i = 0; i < ARRAYLEN(ALL_LCTYPES); i++) {
        WCHAR b[512];
        int q = U(GetLocaleInfoEx)(L"en-US", ALL_LCTYPES[i] | N_NOUSER, NULL, 0);
        int f = U(GetLocaleInfoEx)(L"en-US", ALL_LCTYPES[i] | N_NOUSER, b, 512);
        CHECKF(q == f, "type 0x%lx q=%d f=%d", ALL_LCTYPES[i], q, f);
        if (f > 0) CHECKF(b[f - 1] == 0, "type 0x%lx NUL", ALL_LCTYPES[i]);
    }
}
TEST(xtra_truncation_never_writes_past_cch) {
    REQUIRE(U(GetLocaleInfoEx));
    for (int cch = 1; cch < 9; cch++) {
        WCHAR b[16]; for (int k = 0; k < 16; k++) b[k] = 0x7777;
        SET_ERR(0);
        int r = U(GetLocaleInfoEx)(L"en-US", L_SENGLANGNAME | N_NOUSER, b, cch);
        if (cch < 8) { CHECKF(r == 0, "cch=%d r=%d", cch, r); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER); }
        else CHECKF(r == 8, "cch=%d r=%d", cch, r);
        for (int k = cch; k < 16; k++) CHECKF(b[k] == 0x7777, "cch=%d overwrote [%d]", cch, k);
    }
}
TEST(xtra_enus_calendar_basics) {
    REQUIRE(U(GetCalendarInfoEx)); REQUIRE(U(GetCalendarInfoW));
    WCHAR b[64]; DWORD n = 0;
    int r = U(GetCalendarInfoW)(0x409, 1, CAL_SMONTHNAME1 | 0, b, 64, NULL);
    CHECK(r == 8); if (r == 8) CHECK(!lstrcmpW(b, L"January"));
    r = U(GetCalendarInfoW)(0x409, 1, CAL_SMONTHNAME12, b, 64, NULL);
    CHECK(r == 9); if (r == 9) CHECK(!lstrcmpW(b, L"December"));
    r = U(GetCalendarInfoW)(0x409, 1, CAL_ICALINTVALUE | CAL_RETURN_NUMBER, NULL, 0, &n);
    CHECK(r == 2); CHECK(n == 1);
    r = U(GetCalendarInfoW)(0x409, 1, CAL_SDAYNAME1, b, 64, NULL);
    CHECK(r == 7); if (r == 7) CHECK(!lstrcmpW(b, L"Monday"));
    SET_ERR(0); r = U(GetCalendarInfoW)(0x409, 1, CAL_SMONTHNAME1, b, 3, NULL);
    CHECK(r == 0); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
}
TEST(xtra_number_format_known_vectors) {
    REQUIRE(U(GetNumberFormatEx));
    static const struct { const WCHAR *in, *out; } v[] = {
        { L"0", L"0.00" }, { L"1", L"1.00" }, { L"-1", L"-1.00" }, { L"999", L"999.00" }, { L"1234567.891", L"1,234,567.89" },
        { L"0.005", L"0.01" }, { L"0.004", L"0.00" }, { L"-0.5", L"-0.50" }, { L"12345", L"12,345.00" },
    };
    for (int i = 0; i < ARRAYLEN(v); i++) {
        WCHAR b[64]; SET_ERR(0);
        int r = U(GetNumberFormatEx)(L"en-US", 0, v[i].in, NULL, b, 64);
        CHECKF(r == lstrlenW(v[i].out) + 1, "in=%ls r=%d", v[i].in, r);
        if (r > 0) CHECKF(!lstrcmpW(b, v[i].out), "in=%ls got=%ls", v[i].in, b);
    }
}
TEST(xtra_currency_format_known_vectors) {
    REQUIRE(U(GetCurrencyFormatEx));
    static const struct { const WCHAR *in, *out; } v[] = {
        { L"0", L"$0.00" }, { L"1234.5", L"$1,234.50" }, { L"-5", L"($5.00)" }, { L"1000000", L"$1,000,000.00" },
    };
    for (int i = 0; i < ARRAYLEN(v); i++) {
        WCHAR b[64]; SET_ERR(0);
        int r = U(GetCurrencyFormatEx)(L"en-US", 0, v[i].in, NULL, b, 64);
        CHECKF(r == lstrlenW(v[i].out) + 1, "in=%ls r=%d", v[i].in, r);
        if (r > 0) CHECKF(!lstrcmpW(b, v[i].out), "in=%ls got=%ls", v[i].in, b);
    }
}
TEST(xtra_convertdefaultlocale_known) {
    REQUIRE(U(ConvertDefaultLocale));
    CHECK(U(ConvertDefaultLocale)(0x0409) == 0x0409);
    CHECK(U(ConvertDefaultLocale)(0x0407) == 0x0407);
    CHECK(U(ConvertDefaultLocale)(0x0411) == 0x0411);
}
TEST(xtra_geo_nation_known_values) {
    REQUIRE(U(GetGeoInfoW));
    WCHAR b[64];
    int r = U(GetGeoInfoW)(244, GEO_ISO2, b, 64, 0);
    CHECK(r == 3); if (r == 3) CHECK(!lstrcmpW(b, L"US"));
    r = U(GetGeoInfoW)(244, GEO_ISO3, b, 64, 0);
    CHECK(r == 4); if (r == 4) CHECK(!lstrcmpW(b, L"USA"));
    r = U(GetGeoInfoW)(94, GEO_ISO2, b, 64, 0);
    CHECK(r == 3); if (r == 3) CHECK(!lstrcmpW(b, L"DE"));
    r = U(GetGeoInfoW)(84, GEO_ISO2, b, 64, 0);
    CHECK(r == 3); if (r == 3) CHECK(!lstrcmpW(b, L"FR"));
    r = U(GetGeoInfoW)(122, GEO_ISO2, b, 64, 0);
    CHECK(r == 3); if (r == 3) CHECK(!lstrcmpW(b, L"JP"));
}
TEST(xtra_repeat_calls_identical) {
    REQUIRE(U(GetLocaleInfoEx)); REQUIRE(U(GetDateFormatEx));
    SYSTEMTIME st = nls_st(2024, 7, 4, 13, 5, 9, 0);
    WCHAR first[128], cur[128];
    CHECK(U(GetDateFormatEx)(L"de-DE", 0, &st, L"dddd, d. MMMM yyyy", first, 128, NULL) > 0);
    for (int i = 0; i < 200; i++) {
        CHECK(U(GetDateFormatEx)(L"de-DE", 0, &st, L"dddd, d. MMMM yyyy", cur, 128, NULL) > 0);
        if (lstrcmpW(first, cur)) { CHECKF(0, "iteration %d differs", i); break; }
    }
    EXPECT_WS(1, L"de-DE", "de date", first, L"Donnerstag, 4. Juli 2024");
}
TEST(xtra_time_format_known_vectors) {
    REQUIRE(U(GetTimeFormatEx));
    SYSTEMTIME st = nls_st(2024, 1, 2, 0, 5, 9, 0);
    WCHAR b[64];
    int r = U(GetTimeFormatEx)(L"en-US", 0, &st, L"h:mm:ss tt", b, 64);
    CHECK(r == 12); if (r > 0) CHECK(!lstrcmpW(b, L"12:05:09 AM"));
    st.wHour = 12;
    r = U(GetTimeFormatEx)(L"en-US", 0, &st, L"h:mm tt", b, 64);
    if (r > 0) CHECK(!lstrcmpW(b, L"12:05 PM"));
    st.wHour = 23;
    r = U(GetTimeFormatEx)(L"en-US", 0, &st, L"HH:mm:ss", b, 64);
    CHECK(r == 9); if (r > 0) CHECK(!lstrcmpW(b, L"23:05:09"));
    r = U(GetTimeFormatEx)(L"en-US", 0, &st, L"hh 'o''clock'", b, 64);
    if (r > 0) CHECK(!lstrcmpW(b, L"11 o'clock"));
}
