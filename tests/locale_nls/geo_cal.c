/* Geo information and calendar APIs. */
#include "nlsutil.h"

static int geo(GEOID id, GEOTYPE t, WCHAR *out, int cch) { return U(GetGeoInfoW)(id, t, out, cch, 0); }

typedef struct { GEOID id; const WCHAR *iso2, *iso3, *un, *name; int friendly_hard; } georow_t;
static const georow_t geos[] = {
    { 244, L"US", L"USA", L"840", L"United States", 1 }, { 94, L"DE", L"DEU", L"276", L"Germany", 1 },
    { 84, L"FR", L"FRA", L"250", L"France", 1 }, { 122, L"JP", L"JPN", L"392", L"Japan", 1 },
    { 45, L"CN", L"CHN", L"156", L"China", 0 }, { 32, L"BR", L"BRA", L"076", L"Brazil", 1 },
    { 203, L"RU", L"RUS", L"643", L"Russia", 0 }, { 113, L"IN", L"IND", L"356", L"India", 1 },
    { 134, L"KR", L"KOR", L"410", L"Korea", 0 }, { 235, L"TR", L"TUR", L"792", L"Turkey", 0 },
    { 205, L"SA", L"SAU", L"682", L"Saudi Arabia", 1 }, { 117, L"IL", L"ISR", L"376", L"Israel", 1 },
    { 227, L"TH", L"THA", L"764", L"Thailand", 1 }, { 242, L"GB", L"GBR", L"826", L"United Kingdom", 1 },
    { 39, L"CA", L"CAN", L"124", L"Canada", 1 }, { 12, L"AU", L"AUS", L"036", L"Australia", 1 },
    { 77, L"ES", L"ESP", L"724", L"Spain", 1 }, { 118, L"IT", L"ITA", L"380", L"Italy", 1 },
    { 176, L"NL", L"NLD", L"528", L"Netherlands", 1 }, { 221, L"SE", L"SWE", L"752", L"Sweden", 1 },
    { 191, L"PL", L"POL", L"616", L"Poland", 1 }, { 166, L"MX", L"MEX", L"484", L"Mexico", 1 },
};
TEST(geo_iso_codes_table) {
    REQUIRE(U(GetGeoInfoW));
    for (int i = 0; i < ARRAYLEN(geos); i++) {
        WCHAR b[100];
        SET_ERR(0);
        int r = geo(geos[i].id, GEO_ISO2, b, 100); CHECKF(r == 3 && !lstrcmpW(b, geos[i].iso2), "%ld iso2 got '%ls' r=%d", (long)geos[i].id, b, r);
        r = geo(geos[i].id, GEO_ISO3, b, 100); CHECKF(r == 4 && !lstrcmpW(b, geos[i].iso3), "%ld iso3 got '%ls'", (long)geos[i].id, b);
        r = geo(geos[i].id, GEO_ISO_UN_NUMBER, b, 100); CHECKF(r == 4 && !lstrcmpW(b, geos[i].un), "%ld un got '%ls'", (long)geos[i].id, b);
        r = geo(geos[i].id, GEO_NATION, b, 100);
        WCHAR want[16]; wsprintfW(want, L"%ld", (long)geos[i].id);
        CHECKF(r > 1 && !lstrcmpW(b, want), "%ld nation got '%ls'", (long)geos[i].id, b);
    }
}
TEST(geo_friendly_names_table) {
    REQUIRE(U(GetGeoInfoW));
    for (int i = 0; i < ARRAYLEN(geos); i++) {
        WCHAR b[200]; SET_ERR(0);
        int r = geo(geos[i].id, GEO_FRIENDLYNAME, b, 200);
        CHECKF(r > 1, "%ld", (long)geos[i].id);
        if (r > 1) EXPECT_WS(geos[i].friendly_hard, L"geo", "friendly", b, geos[i].name);
    }
}
TEST(geo_us_all_types) {
    REQUIRE(U(GetGeoInfoW));
    WCHAR b[300];
    for (GEOTYPE t = 1; t <= 16; t++) {
        SET_ERR(0);
        int r = geo(244, t, b, 300);
        if (r <= 0) t_note("GetGeoInfoW(244, type %lu) failed err=%lu", (unsigned long)t, GetLastError());
        else { CHECK(r == lstrlenW(b) + 1); }
    }
    int r = geo(244, GEO_LATITUDE, b, 300); CHECK(r > 1); if (r > 1) CHECK(_wtof(b) > 20.0 && _wtof(b) < 60.0);
    r = geo(244, GEO_LONGITUDE, b, 300); CHECK(r > 1); if (r > 1) CHECK(_wtof(b) < -50.0 && _wtof(b) > -180.0);
    r = geo(244, GEO_DIALINGCODE, b, 300); if (r > 1) CHECKF(!lstrcmpW(b, L"1"), "dialing %ls", b);
    r = geo(244, GEO_CURRENCYCODE, b, 300); if (r > 1) CHECKF(!lstrcmpW(b, L"USD"), "currency %ls", b);
    r = geo(244, GEO_CURRENCYSYMBOL, b, 300); if (r > 1) CHECKF(!lstrcmpW(b, L"$"), "symbol %ls", b);
    r = geo(244, GEO_PARENT, b, 300); if (r > 1) t_note("US parent geo = %ls", b);
    r = geo(244, GEO_RFC1766, b, 300); if (r > 1) t_note("US rfc1766 = %ls", b);
    r = geo(244, GEO_LCID, b, 300); if (r > 1) t_note("US lcid = %ls", b);
    r = geo(244, GEO_OFFICIALNAME, b, 300); if (r > 1) t_note("US official = %ls", b);
}
TEST(geo_buffer_semantics) {
    REQUIRE(U(GetGeoInfoW));
    if (t_is_wine()) { t_skip("emulator faults on size-query/NULL combinations"); return; }
    WCHAR b[100];
    SET_ERR(0);
    CHECK(geo(244, GEO_ISO2, NULL, 0) == 3);
    CHECK(geo(244, GEO_ISO3, NULL, 0) == 4);
    CHECK(geo(244, GEO_ISO2, b, 3) == 3);
    for (int cch = 1; cch < 3; cch++) {
        SET_ERR(0); CHECKF(geo(244, GEO_ISO2, b, cch) == 0, "cch=%d", cch); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    }
    if (!t_is_wine()) { SET_ERR(0); CHECK(geo(244, GEO_ISO2, NULL, 5) == 0); }
    SET_ERR(0); CHECK(geo(244, GEO_ISO2, b, -1) == 0);
    int n = geo(244, GEO_FRIENDLYNAME, NULL, 0);
    CHECK(n > 1);
    SET_ERR(0); CHECK(geo(244, GEO_FRIENDLYNAME, b, n - 1) == 0); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(geo(244, GEO_FRIENDLYNAME, b, n) == n);
}
TEST(geo_invalid_ids) {
    REQUIRE(U(GetGeoInfoW));
    static const GEOID bad[] = { 0, 1, -1, 99999999, 0x7fffffff, 10000, 5 };
    WCHAR b[100];
    for (int i = 0; i < ARRAYLEN(bad); i++) {
        SET_ERR(0);
        int r = geo(bad[i], GEO_ISO2, b, 100);
        if (r != 0) { t_note("geo id %ld accepted (%ls)", (long)bad[i], b); continue; }
        if (bad[i] != -1) t_note("geo id %ld err=%lu", (long)bad[i], GetLastError());
    }
    SET_ERR(0); CHECK(geo(0, GEO_ISO2, b, 100) == 0);
    SET_ERR(0); CHECK(geo(-1, GEO_ISO2, b, 100) == 0);
    SET_ERR(0); CHECK(geo(99999999, GEO_ISO2, b, 100) == 0);
}
TEST(geo_invalid_type) {
    REQUIRE(U(GetGeoInfoW));
    static const GEOTYPE bad[] = { 0, 17, 100, 0xFFFF, 0xFFFFFFFF };
    WCHAR b[100];
    for (int i = 0; i < ARRAYLEN(bad); i++) {
        SET_ERR(0);
        int r = geo(244, bad[i], b, 100);
        CHECKF(r == 0, "type %lu r=%d", (unsigned long)bad[i], r);
        t_note("GetGeoInfoW invalid type %lu err=%lu", (unsigned long)bad[i], GetLastError());
    }
}
TEST(geo_langid_param) {
    REQUIRE(U(GetGeoInfoW));
    WCHAR b[200], c[200];
    int r1 = U(GetGeoInfoW)(244, GEO_FRIENDLYNAME, b, 200, 0x409);
    int r2 = U(GetGeoInfoW)(244, GEO_FRIENDLYNAME, c, 200, 0);
    CHECK(r1 > 1);
    if (r2 <= 1) t_note("GetGeoInfoW with LangId 0 failed: err=%lu", GetLastError());
    int r3 = U(GetGeoInfoW)(94, GEO_FRIENDLYNAME, b, 200, 0x407);
    CHECK(r3 > 1);
    if (r3 > 1 && lstrcmpW(b, L"Deutschland")) t_note("DE friendly name with de-DE langid = %ls", b);
    SET_ERR(0);
    int r4 = U(GetGeoInfoW)(244, GEO_ISO2, b, 200, 0xDEAD);
    CHECKF(r4 == 0, "invalid langid r=%d", r4);
}
TEST(geo_a_variant) {
    REQUIRE(U(GetGeoInfoA));
    char b[100];
    int r = U(GetGeoInfoA)(244, GEO_ISO2, b, 100, 0); CHECK(r == 3); CHECK(!strcmp(b, "US"));
    r = U(GetGeoInfoA)(244, GEO_ISO3, b, 100, 0); CHECK(r == 4); CHECK(!strcmp(b, "USA"));
    r = U(GetGeoInfoA)(244, GEO_FRIENDLYNAME, b, 100, 0); CHECK(r == 14); CHECK(!strcmp(b, "United States"));
    CHECK(U(GetGeoInfoA)(244, GEO_ISO2, NULL, 0, 0) == 3);
    SET_ERR(0); CHECK(U(GetGeoInfoA)(244, GEO_ISO2, b, 2, 0) == 0); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    SET_ERR(0); CHECK(U(GetGeoInfoA)(0, GEO_ISO2, b, 100, 0) == 0);
}
TEST(geo_ex_by_region_name) {
    REQUIRE(UN(GetGeoInfoExK));
    WCHAR b[100]; SET_ERR(0);
    int r = UN(GetGeoInfoExK)(L"US", GEO_ISO3, b, 100);
    CHECKF(r == 4 && !lstrcmpW(b, L"USA"), "r=%d err=%lu '%ls'", r, GetLastError(), b);
    r = UN(GetGeoInfoExK)(L"DE", GEO_NATION, b, 100); CHECK(r > 1); if (r > 1) CHECK(!lstrcmpW(b, L"94"));
    r = UN(GetGeoInfoExK)(L"JP", GEO_ISO_UN_NUMBER, b, 100); CHECK(r == 4); CHECK(!lstrcmpW(b, L"392"));
    r = UN(GetGeoInfoExK)(L"us", GEO_ISO2, b, 100); t_note("lowercase region name: r=%d '%ls'", r, b);
    CHECK(UN(GetGeoInfoExK)(L"US", GEO_ISO2, NULL, 0) == 3);
    SET_ERR(0); CHECK(UN(GetGeoInfoExK)(NULL, GEO_ISO2, b, 100) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(UN(GetGeoInfoExK)(L"ZZZZZ", GEO_ISO2, b, 100) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(UN(GetGeoInfoExK)(L"", GEO_ISO2, b, 100) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(UN(GetGeoInfoExK)(L"US", GEO_ISO2, b, -1) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    if (!t_is_wine()) { SET_ERR(0); CHECK(UN(GetGeoInfoExK)(L"US", GEO_ISO2, NULL, 5) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
    SET_ERR(0); CHECK(UN(GetGeoInfoExK)(L"US", GEO_ISO2, b, 2) == 0); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
}
TEST(geo_user_geoid) {
    REQUIRE(U(GetUserGeoID)); REQUIRE(U(GetGeoInfoW));
    GEOID n = U(GetUserGeoID)(GEOCLASS_NATION);
    CHECKF(n > 0, "nation geoid %ld", (long)n);
    WCHAR b[100];
    if (n > 0) CHECK(geo(n, GEO_ISO2, b, 100) == 3);
    GEOID r = U(GetUserGeoID)(GEOCLASS_REGION);
    t_note("user nation geo=%ld region geo=%ld", (long)n, (long)r);
    for (int i = 0; i < 20; i++) CHECK(U(GetUserGeoID)(GEOCLASS_NATION) == n);
}
TEST(geo_user_geoid_invalid_class) {
    REQUIRE(U(GetUserGeoID));
    CHECK(U(GetUserGeoID)(0) == -1);
    CHECK(U(GetUserGeoID)(1) == -1);
    CHECK(U(GetUserGeoID)(15) == -1);
    CHECK(U(GetUserGeoID)(17) == -1);
    CHECK(U(GetUserGeoID)(0xFFFFFFFF) == -1);
}
TEST(geo_user_default_geoname) {
    REQUIRE(U(GetUserDefaultGeoName)); REQUIRE(U(GetUserGeoID));
    WCHAR b[100]; SET_ERR(0);
    int r = U(GetUserDefaultGeoName)(b, 100);
    CHECKF(r > 1, "r=%d err=%lu", r, GetLastError());
    if (r > 1) {
        CHECK(r == lstrlenW(b) + 1);
        CHECK(lstrlenW(b) == 2 || lstrlenW(b) == 3);
        if (UN(GetGeoInfoExK)) {
            WCHAR n[32];
            int rn = UN(GetGeoInfoExK)(b, GEO_NATION, n, 32);
            if (rn > 1 && U(GetUserGeoID)(GEOCLASS_NATION) > 0) {
                int ok = _wtoi(n) == (int)U(GetUserGeoID)(GEOCLASS_NATION);
                if (!ok) t_note("geo name %ls nation=%ls vs GetUserGeoID %ld", b, n, (long)U(GetUserGeoID)(GEOCLASS_NATION));
            }
        }
    }
    SET_ERR(0); CHECK(U(GetUserDefaultGeoName)(b, -1) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    if (!t_is_wine()) { SET_ERR(0); CHECK(U(GetUserDefaultGeoName)(NULL, 5) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
    SET_ERR(0); r = U(GetUserDefaultGeoName)(NULL, 0); t_note("GetUserDefaultGeoName(NULL,0)=%d", r);
}

/* ---- calendar ---- */
#define CALN(x) ((x) | N_NOUSER)
static int ci(const WCHAR *loc, CALID cal, CALTYPE t, WCHAR *out, int cch, DWORD *val)
{
    return UN(GetCalendarInfoEx)(loc, cal, NULL, t, out, cch, val);
}
typedef struct { const WCHAR *loc; CALID cal; CALTYPE t; const WCHAR *val; int hard; } calrow_t;
static void run_cal(const calrow_t *t, int n)
{
    REQUIRE(UN(GetCalendarInfoEx));
    for (int i = 0; i < n; i++) {
        WCHAR b[300]; SET_ERR(0);
        int r = ci(t[i].loc, t[i].cal, CALN(t[i].t), b, 300, NULL);
        if (r <= 0) { if (t[i].hard) CHECKF(0, "row %d %ls cal %lu type %lu err %lu", i, t[i].loc, (unsigned long)t[i].cal, (unsigned long)t[i].t, GetLastError()); else t_note("row %d failed err %lu", i, GetLastError()); continue; }
        CHECKF(r == lstrlenW(b) + 1, "row %d r=%d len=%d", i, r, lstrlenW(b));
        EXPECT_WS(t[i].hard, t[i].loc, "cal", b, t[i].val);
    }
}
TEST(calinfo_gregorian_enus_names) {
    static const calrow_t t[] = {
        { L"en-US", 1, CAL_SMONTHNAME1, L"January", 1 }, { L"en-US", 1, CAL_SMONTHNAME2, L"February", 1 }, { L"en-US", 1, CAL_SMONTHNAME3, L"March", 1 },
        { L"en-US", 1, CAL_SMONTHNAME4, L"April", 1 }, { L"en-US", 1, CAL_SMONTHNAME5, L"May", 1 }, { L"en-US", 1, CAL_SMONTHNAME6, L"June", 1 },
        { L"en-US", 1, CAL_SMONTHNAME7, L"July", 1 }, { L"en-US", 1, CAL_SMONTHNAME8, L"August", 1 }, { L"en-US", 1, CAL_SMONTHNAME9, L"September", 1 },
        { L"en-US", 1, CAL_SMONTHNAME10, L"October", 1 }, { L"en-US", 1, CAL_SMONTHNAME11, L"November", 1 }, { L"en-US", 1, CAL_SMONTHNAME12, L"December", 1 },
        { L"en-US", 1, CAL_SMONTHNAME13, L"", 1 },
        { L"en-US", 1, CAL_SABBREVMONTHNAME1, L"Jan", 1 }, { L"en-US", 1, CAL_SABBREVMONTHNAME2, L"Feb", 1 }, { L"en-US", 1, CAL_SABBREVMONTHNAME3, L"Mar", 1 },
        { L"en-US", 1, CAL_SABBREVMONTHNAME4, L"Apr", 1 }, { L"en-US", 1, CAL_SABBREVMONTHNAME5, L"May", 1 }, { L"en-US", 1, CAL_SABBREVMONTHNAME6, L"Jun", 1 },
        { L"en-US", 1, CAL_SABBREVMONTHNAME7, L"Jul", 1 }, { L"en-US", 1, CAL_SABBREVMONTHNAME8, L"Aug", 1 }, { L"en-US", 1, CAL_SABBREVMONTHNAME9, L"Sep", 1 },
        { L"en-US", 1, CAL_SABBREVMONTHNAME10, L"Oct", 1 }, { L"en-US", 1, CAL_SABBREVMONTHNAME11, L"Nov", 1 }, { L"en-US", 1, CAL_SABBREVMONTHNAME12, L"Dec", 1 },
        { L"en-US", 1, CAL_SABBREVMONTHNAME13, L"", 1 },
        { L"en-US", 1, CAL_SDAYNAME1, L"Monday", 1 }, { L"en-US", 1, CAL_SDAYNAME2, L"Tuesday", 1 }, { L"en-US", 1, CAL_SDAYNAME3, L"Wednesday", 1 },
        { L"en-US", 1, CAL_SDAYNAME4, L"Thursday", 1 }, { L"en-US", 1, CAL_SDAYNAME5, L"Friday", 1 }, { L"en-US", 1, CAL_SDAYNAME6, L"Saturday", 1 },
        { L"en-US", 1, CAL_SDAYNAME7, L"Sunday", 1 },
        { L"en-US", 1, CAL_SABBREVDAYNAME1, L"Mon", 1 }, { L"en-US", 1, CAL_SABBREVDAYNAME2, L"Tue", 1 }, { L"en-US", 1, CAL_SABBREVDAYNAME3, L"Wed", 1 },
        { L"en-US", 1, CAL_SABBREVDAYNAME4, L"Thu", 1 }, { L"en-US", 1, CAL_SABBREVDAYNAME5, L"Fri", 1 }, { L"en-US", 1, CAL_SABBREVDAYNAME6, L"Sat", 1 },
        { L"en-US", 1, CAL_SABBREVDAYNAME7, L"Sun", 1 },
        { L"en-US", 1, CAL_SSHORTESTDAYNAME1, L"Mo", 0 }, { L"en-US", 1, CAL_SSHORTESTDAYNAME7, L"Su", 0 },
        { L"en-US", 1, CAL_SERASTRING, L"A.D.", 0 }, { L"en-US", 1, CAL_SABBREVERASTRING, L"AD", 0 },
        { L"en-US", 1, CAL_SCALNAME, L"Gregorian Calendar", 0 },
        { L"en-US", 1, CAL_SSHORTDATE, L"M/d/yyyy", 1 }, { L"en-US", 1, CAL_SLONGDATE, L"dddd, MMMM d, yyyy", 1 },
        { L"en-US", 1, CAL_SYEARMONTH, L"MMMM yyyy", 1 }, { L"en-US", 1, CAL_SMONTHDAY, L"MMMM d", 1 },
        { L"en-US", 1, CAL_ITWODIGITYEARMAX, L"2029", 1 },
        { L"de-DE", 1, CAL_SMONTHNAME3, L"März", 1 }, { L"de-DE", 1, CAL_SDAYNAME2, L"Dienstag", 1 }, { L"fr-FR", 1, CAL_SMONTHNAME2, L"février", 1 },
        { L"fr-FR", 1, CAL_SDAYNAME1, L"lundi", 1 }, { L"ja-JP", 1, CAL_SDAYNAME1, L"月曜日", 1 }, { L"ja-JP", 1, CAL_SMONTHNAME1, L"1月", 1 },
        { L"zh-CN", 1, CAL_SDAYNAME7, L"星期日", 1 }, { L"ru-RU", 1, CAL_SMONTHNAME1, L"январь", 1 },
        { L"tr-TR", 1, CAL_SDAYNAME1, L"Pazartesi", 1 }, { L"pt-BR", 1, CAL_SMONTHNAME3, L"março", 1 }, { L"ko-KR", 1, CAL_SDAYNAME1, L"월요일", 1 },
        { L"", 1, CAL_SMONTHNAME1, L"January", 1 }, { L"", 1, CAL_SDAYNAME7, L"Sunday", 1 }, { L"", 1, CAL_SSHORTDATE, L"MM/dd/yyyy", 1 },
    };
    run_cal(t, ARRAYLEN(t));
}
TEST(calinfo_names_match_localeinfo) {
    REQUIRE(UN(GetCalendarInfoEx)); REQUIRE(U(GetLocaleInfoEx));
    for (int i = 0; i < ARRAYLEN(FIXED_LOCALES); i++) {
        for (int k = 0; k < 12; k++) {
            WCHAR a[100], b[100];
            int ra = ci(FIXED_LOCALES[i], 1, CALN(CAL_SMONTHNAME1 + k), a, 100, NULL);
            int rb = nls_str(FIXED_LOCALES[i], L_SMONTHNAME1 + k, b, 100);
            if (ra > 0 && rb > 0 && wcscmp(a, b)) {
                /* the default calendar's names are the locale names; a different default calendar legitimately differs */
                DWORD dc = 0; ci(FIXED_LOCALES[i], 1, CALN(CAL_ICALINTVALUE | N_RETNUM), NULL, 0, &dc);
                WCHAR ct[8]; nls_str(FIXED_LOCALES[i], L_ICALENDARTYPE, ct, 8);
                if (_wtoi(ct) == 1) CHECKF(0, "%ls month %d: cal '%ls' locale '%ls'", FIXED_LOCALES[i], k + 1, a, b);
            }
        }
        for (int k = 0; k < 7; k++) {
            WCHAR a[100], b[100];
            int ra = ci(FIXED_LOCALES[i], 1, CALN(CAL_SDAYNAME1 + k), a, 100, NULL);
            int rb = nls_str(FIXED_LOCALES[i], L_SDAYNAME1 + k, b, 100);
            CHECKF(ra > 0 && rb > 0 && !lstrcmpW(a, b), "%ls day %d: '%ls' '%ls'", FIXED_LOCALES[i], k + 1, a, b);
        }
    }
}
TEST(calinfo_return_number) {
    REQUIRE(UN(GetCalendarInfoEx));
    DWORD v = 0xDEADBEEF; SET_ERR(0);
    int r = ci(L"en-US", 1, CAL_ICALINTVALUE | CAL_RETURN_NUMBER, NULL, 0, &v);
    CHECKF(r > 0 && v == 1, "r=%d v=%lu err=%lu", r, v, GetLastError());
    v = 0; r = ci(L"en-US", 1, CAL_ITWODIGITYEARMAX | CAL_RETURN_NUMBER, NULL, 0, &v);
    CHECKF(r > 0 && v == 2029, "r=%d v=%lu", r, v);
    static const struct { const WCHAR *loc; CALID cal; } cals[] = {
        { L"en-US", 2 }, { L"ja-JP", 3 }, { L"zh-TW", 4 }, { L"ko-KR", 5 }, { L"ar-SA", 6 }, { L"th-TH", 7 }, { L"he-IL", 8 },
        { L"ar-SA", 23 }, { L"fa-IR", 22 }, { L"fr-FR", 9 }, { L"ar-SA", 10 },
    };
    for (int i = 0; i < ARRAYLEN(cals); i++) {
        v = 0xDEADBEEF; SET_ERR(0);
        r = ci(cals[i].loc, cals[i].cal, CAL_ICALINTVALUE | CAL_RETURN_NUMBER, NULL, 0, &v);
        if (r > 0) CHECKF(v == cals[i].cal, "%ls cal %lu -> %lu", cals[i].loc, (unsigned long)cals[i].cal, v);
        else t_note("%ls calendar %lu unsupported: err=%lu", cals[i].loc, (unsigned long)cals[i].cal, GetLastError());
    }
}
TEST(calinfo_return_number_null_value) {
    REQUIRE(UN(GetCalendarInfoEx));
    SET_ERR(0);
    int r = ci(L"en-US", 1, CAL_ICALINTVALUE | CAL_RETURN_NUMBER, NULL, 0, NULL);
    t_note("RETURN_NUMBER with NULL lpValue: r=%d err=%lu", r, r ? 0 : GetLastError());
    DWORD v = 0; WCHAR b[16]; SET_ERR(0);
    r = ci(L"en-US", 1, CAL_SMONTHNAME1 | CAL_RETURN_NUMBER, NULL, 0, &v);
    CHECK(r == 0);
    t_note("RETURN_NUMBER on a string type: err=%lu", GetLastError());
}
TEST(calinfo_japanese_calendar) {
    REQUIRE(UN(GetCalendarInfoEx));
    WCHAR b[100]; DWORD v = 0; SET_ERR(0);
    int r = ci(L"ja-JP", CAL_JAPAN, CALN(CAL_ICALINTVALUE | CAL_RETURN_NUMBER), NULL, 0, &v);
    CHECKF(r > 0 && v == 3, "r=%d v=%lu err=%lu", r, v, GetLastError());
    r = ci(L"ja-JP", CAL_JAPAN, CALN(CAL_SERASTRING), b, 100, NULL);
    CHECK(r > 1); if (r > 1) t_note("ja-JP Japanese calendar CAL_SERASTRING = %ls", b);
    r = ci(L"ja-JP", CAL_JAPAN, CALN(CAL_SCALNAME), b, 100, NULL);
    CHECK(r > 1); if (r > 1) t_note("ja-JP Japanese calendar name = %ls", b);
    r = ci(L"ja-JP", CAL_JAPAN, CALN(CAL_SMONTHNAME1), b, 100, NULL);
    CHECK(r > 1); if (r > 1) EXPECT_WS(0, L"ja-JP", "month1", b, L"1月");
    r = ci(L"ja-JP", CAL_JAPAN, CALN(CAL_IYEAROFFSETRANGE), b, 100, NULL);
    CHECK(r > 1); if (r > 1) t_note("ja-JP year offset range = %ls", b);
    r = ci(L"ja-JP", CAL_JAPAN, CALN(CAL_SJAPANESEERAFIRSTYEAR), b, 100, NULL);
    if (r > 1) t_note("CAL_SJAPANESEERAFIRSTYEAR = %ls", b);
}
TEST(calinfo_other_calendars_names) {
    static const calrow_t t[] = {
        { L"th-TH", 7, CAL_SERASTRING, L"พ.ศ.", 0 }, { L"th-TH", 7, CAL_SCALNAME, L"พุทธศักราช", 0 },
        { L"ar-SA", 6, CAL_SMONTHNAME1, L"محرم", 0 }, { L"ar-SA", 23, CAL_SMONTHNAME1, L"محرم", 0 },
        { L"zh-TW", 4, CAL_SERASTRING, L"民國", 0 }, { L"ko-KR", 5, CAL_SERASTRING, L"단기", 0 },
        { L"he-IL", 8, CAL_SMONTHNAME1, L"תשרי", 0 }, { L"fa-IR", 22, CAL_SMONTHNAME1, L"فروردین", 0 },
        { L"en-US", 2, CAL_SMONTHNAME1, L"January", 0 }, { L"en-US", 2, CAL_SDAYNAME1, L"Monday", 0 },
        { L"fr-FR", 9, CAL_SMONTHNAME1, L"janvier", 0 }, { L"fr-FR", 12, CAL_SMONTHNAME1, L"janvier", 0 },
        { L"en-US", 11, CAL_SMONTHNAME1, L"January", 0 },
    };
    run_cal(t, ARRAYLEN(t));
}
TEST(calinfo_calendar_month13) {
    REQUIRE(UN(GetCalendarInfoEx));
    WCHAR b[100];
    SET_ERR(0);
    int r = ci(L"he-IL", 8, CALN(CAL_SMONTHNAME13), b, 100, NULL);
    if (r > 1) t_note("Hebrew 13th month = %ls", b); else t_note("Hebrew month 13 r=%d err=%lu", r, GetLastError());
    r = ci(L"en-US", 1, CALN(CAL_SMONTHNAME13), b, 100, NULL);
    CHECK(r == 1); CHECK(b[0] == 0);
}
TEST(calinfo_buffer_semantics) {
    REQUIRE(UN(GetCalendarInfoEx));
    WCHAR b[100];
    SET_ERR(0);
    CHECK(ci(L"en-US", 1, CALN(CAL_SMONTHNAME3), NULL, 0, NULL) == 6);
    CHECK(ci(L"en-US", 1, CALN(CAL_SMONTHNAME3), b, 6, NULL) == 6);
    for (int cch = 1; cch < 6; cch++) {
        SET_ERR(0); CHECKF(ci(L"en-US", 1, CALN(CAL_SMONTHNAME3), b, cch, NULL) == 0, "cch=%d", cch); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    }
    if (!t_is_wine()) { SET_ERR(0); CHECK(ci(L"en-US", 1, CALN(CAL_SMONTHNAME3), NULL, 5, NULL) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
    SET_ERR(0); CHECK(ci(L"en-US", 1, CALN(CAL_SMONTHNAME3), b, -1, NULL) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(ci(L"not-a-locale-x", 1, CALN(CAL_SMONTHNAME3), b, 100, NULL) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CHECK(ci(L"en-US", 1, CALN(CAL_SMONTHNAME13), NULL, 0, NULL) == 1);
}
TEST(calinfo_invalid_calendar_and_type) {
    REQUIRE(UN(GetCalendarInfoEx));
    WCHAR b[100];
    static const CALID badcal[] = { 0, 13, 16, 17, 18, 19, 24, 99, 1000, 0xFFFF, 0xFFFFFFFF };
    for (int i = 0; i < ARRAYLEN(badcal); i++) {
        SET_ERR(0);
        int r = ci(L"en-US", badcal[i], CALN(CAL_SMONTHNAME1), b, 100, NULL);
        if (r) t_note("calendar %lu accepted for en-US", (unsigned long)badcal[i]);
        else t_note("calendar %lu rejected err=%lu", (unsigned long)badcal[i], GetLastError());
    }
    SET_ERR(0); CHECK(ci(L"en-US", 99, CALN(CAL_SMONTHNAME1), b, 100, NULL) == 0);
    SET_ERR(0); CHECK(ci(L"en-US", 1000, CALN(CAL_SMONTHNAME1), b, 100, NULL) == 0);
    static const CALTYPE badtype[] = { 0, 0x7fff, 0xFFFF, 0x1000, 0x40 };
    for (int i = 0; i < ARRAYLEN(badtype); i++) {
        SET_ERR(0);
        int r = ci(L"en-US", 1, CALN(badtype[i]), b, 100, NULL);
        CHECKF(r == 0, "type 0x%lx r=%d", (unsigned long)badtype[i], r);
        t_note("CalType 0x%lx err=%lu", (unsigned long)badtype[i], GetLastError());
    }
}
TEST(calinfo_w_a_variants) {
    REQUIRE(U(GetCalendarInfoW)); REQUIRE(U(GetCalendarInfoA));
    WCHAR w[100]; char a[100]; DWORD v = 0;
    int r = U(GetCalendarInfoW)(0x409, 1, CAL_SMONTHNAME3 | N_NOUSER, w, 100, NULL);
    CHECK(r == 6); CHECK(!lstrcmpW(w, L"March"));
    r = U(GetCalendarInfoA)(0x409, 1, CAL_SMONTHNAME3 | N_NOUSER, a, 100, NULL);
    CHECK(r == 6); CHECK(!strcmp(a, "March"));
    CHECK(U(GetCalendarInfoA)(0x409, 1, CAL_SMONTHNAME3 | N_NOUSER, NULL, 0, NULL) == 6);
    r = U(GetCalendarInfoW)(0x409, 1, CAL_ICALINTVALUE | CAL_RETURN_NUMBER | N_NOUSER, NULL, 0, &v);
    CHECK(r > 0); CHECK(v == 1);
    v = 0; r = U(GetCalendarInfoA)(0x409, 1, CAL_ITWODIGITYEARMAX | CAL_RETURN_NUMBER | N_NOUSER, NULL, 0, &v);
    CHECK(r > 0); CHECK(v == 2029);
    r = U(GetCalendarInfoW)(0x407, 1, CAL_SMONTHNAME3 | N_NOUSER, w, 100, NULL);
    CHECK(r == 5); CHECK(!lstrcmpW(w, L"März"));
    SET_ERR(0); CHECK(U(GetCalendarInfoW)(0xDEAD, 1, CAL_SMONTHNAME3, w, 100, NULL) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    if (!t_is_wine()) { SET_ERR(0); CHECK(U(GetCalendarInfoW)(0x409, 1, CAL_SMONTHNAME3, NULL, 5, NULL) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
    SET_ERR(0); CHECK(U(GetCalendarInfoA)(0x409, 1, CAL_SMONTHNAME3, a, 3, NULL) == 0); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    r = U(GetCalendarInfoA)(0x411, 1, CAL_SDAYNAME1 | N_NOUSER, a, 100, NULL);
    CHECKF(r == 7, "ja day name r=%d", r);
}
TEST(calinfo_all_types_all_locales_present) {
    REQUIRE(UN(GetCalendarInfoEx));
    for (int i = 0; i < ARRAYLEN(FIXED_LOCALES); i++) {
        for (CALTYPE t = CAL_ICALINTVALUE; t <= CAL_SMONTHDAY; t++) {
            if (t == CAL_SERASTRING + 100) continue;
            WCHAR b[300]; SET_ERR(0);
            int r = ci(FIXED_LOCALES[i], 1, CALN(t), b, 300, NULL);
            if (r <= 0) { CHECKF(t == CAL_IYEAROFFSETRANGE || t == CAL_SERASTRING, "%ls cal 1 type %lu err %lu", FIXED_LOCALES[i], (unsigned long)t, GetLastError()); }
        }
    }
}

/* ---- calendar enumeration ---- */
static WCHAR g_cs[256][96]; static CALID g_cid[256]; static int g_cn, g_cstop;
static void c_reset(int stop) { g_cn = 0; g_cstop = stop; }
static void c_add(const WCHAR *s, CALID c) { if (g_cn < 256) { lstrcpynW(g_cs[g_cn], s, 96); g_cid[g_cn] = c; } g_cn++; }
static int c_find(const WCHAR *s) { for (int i = 0; i < g_cn && i < 256; i++) if (!lstrcmpW(g_cs[i], s)) return i; return -1; }
static BOOL c_cont(void) { return !(g_cstop && g_cn >= g_cstop); }
static BOOL CALLBACK ccb_w(LPWSTR s) { c_add(s, 0); return c_cont(); }
static BOOL CALLBACK ccb_a(LPSTR s) { WCHAR w[96]; nls_a2w(s, w, 96); c_add(w, 0); return c_cont(); }
static BOOL CALLBACK ccb_exw(LPWSTR s, CALID c) { c_add(s, c); return c_cont(); }
static BOOL CALLBACK ccb_exa(LPSTR s, CALID c) { WCHAR w[96]; nls_a2w(s, w, 96); c_add(w, c); return c_cont(); }
static LPARAM g_lp;
static BOOL CALLBACK ccb_exex(LPWSTR s, CALID c, LPWSTR res, LPARAM lp) { g_lp = lp; c_add(s, c); return c_cont(); }
static BOOL CALLBACK tcb_w(LPWSTR s) { c_add(s, 0); return c_cont(); }
static BOOL CALLBACK tcb_ex(LPWSTR s, LPARAM lp) { g_lp = lp; c_add(s, 0); return c_cont(); }
static BOOL CALLBACK dcb_exex(LPWSTR s, CALID c, LPARAM lp) { g_lp = lp; c_add(s, c); return c_cont(); }

TEST(calenum_calinfo_w_short_date) {
    REQUIRE(U(EnumCalendarInfoW));
    c_reset(0); SET_ERR(0);
    BOOL r = U(EnumCalendarInfoW)(ccb_w, 0x409, 1, CAL_SSHORTDATE);
    CHECK(r); CHECK(g_cn >= 1);
    CHECK(c_find(L"M/d/yyyy") >= 0);
    for (int i = 0; i < g_cn && i < 256; i++) CHECK(g_cs[i][0] != 0);
}
TEST(calenum_calinfo_w_month_names) {
    REQUIRE(U(EnumCalendarInfoW));
    c_reset(0);
    CHECK(U(EnumCalendarInfoW)(ccb_w, 0x409, 1, CAL_SMONTHNAME1));
    CHECK(c_find(L"January") >= 0);
    c_reset(0);
    CHECK(U(EnumCalendarInfoW)(ccb_w, 0x409, 1, CAL_SDAYNAME1));
    CHECK(g_cn == 1 || g_cn > 0); CHECK(c_find(L"Monday") >= 0);
    c_reset(0);
    CHECK(U(EnumCalendarInfoW)(ccb_w, 0x407, 1, CAL_SMONTHNAME3));
    CHECK(c_find(L"März") >= 0);
}
TEST(calenum_calinfo_all_calendars_values) {
    REQUIRE(U(EnumCalendarInfoW));
    c_reset(0); SET_ERR(0);
    BOOL r = U(EnumCalendarInfoW)(ccb_w, 0x409, ENUM_ALL_CALENDARS, CAL_ICALINTVALUE);
    CHECKF(r, "err %lu", GetLastError());
    CHECK(c_find(L"1") >= 0);
    c_reset(0);
    r = U(EnumCalendarInfoW)(ccb_w, 0x411, ENUM_ALL_CALENDARS, CAL_ICALINTVALUE);
    CHECK(r); CHECK(c_find(L"1") >= 0); if (c_find(L"3") < 0) t_note("ja-JP does not list calendar 3");
    c_reset(0);
    r = U(EnumCalendarInfoW)(ccb_w, 0x0401, ENUM_ALL_CALENDARS, CAL_ICALINTVALUE);
    CHECK(r); t_note("ar-SA calendars: %d (hijri listed: %d)", g_cn, c_find(L"6") >= 0);
}
TEST(calenum_calinfo_a_matches_w) {
    REQUIRE(U(EnumCalendarInfoA)); REQUIRE(U(EnumCalendarInfoW));
    c_reset(0); U(EnumCalendarInfoW)(ccb_w, 0x409, 1, CAL_SSHORTDATE); int nw = g_cn;
    c_reset(0); CHECK(U(EnumCalendarInfoA)(ccb_a, 0x409, 1, CAL_SSHORTDATE));
    CHECK(g_cn == nw); CHECK(c_find(L"M/d/yyyy") >= 0);
}
TEST(calenum_calinfo_ex_reports_calendar) {
    REQUIRE(U(EnumCalendarInfoExW));
    c_reset(0); SET_ERR(0);
    BOOL r = U(EnumCalendarInfoExW)(ccb_exw, 0x409, 1, CAL_SSHORTDATE);
    CHECK(r); CHECK(g_cn >= 1);
    for (int i = 0; i < g_cn && i < 256; i++) CHECKF(g_cid[i] == 1, "calid %lu", (unsigned long)g_cid[i]);
    c_reset(0);
    r = U(EnumCalendarInfoExW)(ccb_exw, 0x411, ENUM_ALL_CALENDARS, CAL_SMONTHNAME1);
    CHECK(r);
    int seen1 = 0; for (int i = 0; i < g_cn && i < 256; i++) seen1 |= g_cid[i] == 1;
    CHECK(seen1);
    REQUIRE(U(EnumCalendarInfoExA));
    c_reset(0); CHECK(U(EnumCalendarInfoExA)(ccb_exa, 0x409, 1, CAL_SSHORTDATE)); CHECK(g_cn >= 1);
}
TEST(calenum_calinfo_exex) {
    REQUIRE(U(EnumCalendarInfoExEx)); 
    c_reset(0); g_lp = 0; SET_ERR(0);
    BOOL r = U(EnumCalendarInfoExEx)(ccb_exex, L"en-US", 1, NULL, CAL_SSHORTDATE, 0x5151);
    CHECKF(r, "err %lu", GetLastError());
    CHECK(g_cn >= 1); CHECK(g_lp == 0x5151); CHECK(c_find(L"M/d/yyyy") >= 0);
    c_reset(0);
    r = U(EnumCalendarInfoExEx)(ccb_exex, L"de-DE", 1, NULL, CAL_SMONTHNAME1, 0);
    CHECK(r); CHECK(c_find(L"Januar") >= 0);
    c_reset(0);
    r = U(EnumCalendarInfoExEx)(ccb_exex, L"en-US", ENUM_ALL_CALENDARS, NULL, CAL_ICALINTVALUE, 0);
    CHECK(r); CHECK(c_find(L"1") >= 0);
}
TEST(calenum_early_stop) {
    REQUIRE(U(EnumCalendarInfoW)); REQUIRE(U(EnumCalendarInfoExEx));
    c_reset(1);
    U(EnumCalendarInfoW)(ccb_w, 0x409, 1, CAL_SSHORTDATE);
    CHECK(g_cn == 1);
    c_reset(1);
    U(EnumCalendarInfoExEx)(ccb_exex, L"en-US", 1, NULL, CAL_SSHORTDATE, 0);
    CHECK(g_cn == 1);
    c_reset(1);
    U(EnumCalendarInfoExW)(ccb_exw, 0x409, 1, CAL_SSHORTDATE);
    CHECK(g_cn == 1);
}
TEST(calenum_errors) {
    REQUIRE(U(EnumCalendarInfoW));
    SET_ERR(0);
    if (!t_is_wine()) { CHECK(!U(EnumCalendarInfoW)(NULL, 0x409, 1, CAL_SSHORTDATE)); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
    c_reset(0); SET_ERR(0);
    BOOL r = U(EnumCalendarInfoW)(ccb_w, 0x409, 99, CAL_SSHORTDATE);
    CHECK(!r || g_cn == 0); t_note("bad calendar: r=%d err=%lu", r, GetLastError());
    c_reset(0); SET_ERR(0);
    r = U(EnumCalendarInfoW)(ccb_w, 0x409, 1, 0x7fff);
    t_note("bad caltype: err=%lu", GetLastError());
    c_reset(0); SET_ERR(0);
    r = U(EnumCalendarInfoW)(ccb_w, 0xDEAD, 1, CAL_SSHORTDATE);
    CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(calenum_date_formats) {
    REQUIRE(U(EnumDateFormatsW));
    c_reset(0); SET_ERR(0);
    BOOL r = U(EnumDateFormatsW)(dcb_exex ? (DATEFMT_ENUMPROCW)ccb_w : NULL, 0x409, DATE_SHORTDATE);
    CHECK(r); CHECK(c_find(L"M/d/yyyy") >= 0);
    c_reset(0);
    CHECK(U(EnumDateFormatsW)(ccb_w, 0x409, DATE_LONGDATE)); CHECK(c_find(L"dddd, MMMM d, yyyy") >= 0);
    c_reset(0);
    CHECK(U(EnumDateFormatsW)(ccb_w, 0x409, DATE_YEARMONTH)); CHECK(c_find(L"MMMM yyyy") >= 0);
    c_reset(0);
    CHECK(U(EnumDateFormatsW)(ccb_w, 0x407, DATE_SHORTDATE)); CHECK(g_cn >= 1);
    c_reset(1); U(EnumDateFormatsW)(ccb_w, 0x409, DATE_SHORTDATE); CHECK(g_cn == 1);
    c_reset(0); SET_ERR(0);
    CHECK(!U(EnumDateFormatsW)(ccb_w, 0x409, 0x4000)); t_note("bad date flags err=%lu", GetLastError());
    REQUIRE(U(EnumDateFormatsA));
    c_reset(0); CHECK(U(EnumDateFormatsA)(ccb_a, 0x409, DATE_SHORTDATE)); CHECK(c_find(L"M/d/yyyy") >= 0);
}
TEST(calenum_date_formats_exex) {
    REQUIRE(UN(EnumDateFormatsExEx));
    c_reset(0); g_lp = 0; SET_ERR(0);
    BOOL r = UN(EnumDateFormatsExEx)(dcb_exex, L"en-US", DATE_SHORTDATE, 0x77);
    CHECKF(r, "err %lu", GetLastError());
    CHECK(g_lp == 0x77); CHECK(c_find(L"M/d/yyyy") >= 0);
    for (int i = 0; i < g_cn && i < 256; i++) CHECKF(g_cid[i] == 1 || g_cid[i] == 0, "calid %lu", (unsigned long)g_cid[i]);
    c_reset(1); UN(EnumDateFormatsExEx)(dcb_exex, L"en-US", DATE_LONGDATE, 0); CHECK(g_cn == 1);
    REQUIRE(U(EnumDateFormatsExW));
    c_reset(0); CHECK(U(EnumDateFormatsExW)(ccb_exw, 0x409, DATE_SHORTDATE)); CHECK(c_find(L"M/d/yyyy") >= 0);
    REQUIRE(U(EnumDateFormatsExA));
    c_reset(0); CHECK(U(EnumDateFormatsExA)(ccb_exa, 0x409, DATE_SHORTDATE)); if (c_find(L"M/d/yyyy") < 0) t_note("EnumDateFormatsExA: short date list lacks M/d/yyyy (n=%d)", g_cn);
}
TEST(calenum_time_formats) {
    REQUIRE(U(EnumTimeFormatsW));
    c_reset(0); SET_ERR(0);
    BOOL r = U(EnumTimeFormatsW)(tcb_w, 0x409, 0);
    CHECK(r); CHECK(c_find(L"h:mm:ss tt") >= 0);
    c_reset(0); CHECK(U(EnumTimeFormatsW)(tcb_w, 0x407, 0)); CHECK(c_find(L"HH:mm:ss") >= 0);
    c_reset(1); U(EnumTimeFormatsW)(tcb_w, 0x409, 0); CHECK(g_cn == 1);
    c_reset(0); SET_ERR(0);
    r = U(EnumTimeFormatsW)(tcb_w, 0x409, 0x4000);
    CHECK(!r); t_note("bad time flags err=%lu", GetLastError());
    c_reset(0); SET_ERR(0);
    r = U(EnumTimeFormatsW)(tcb_w, 0x409, TIME_NOSECONDS);
    t_note("EnumTimeFormatsW(TIME_NOSECONDS) r=%d n=%d", r, g_cn);
    REQUIRE(U(EnumTimeFormatsA));
    c_reset(0); CHECK(U(EnumTimeFormatsA)(ccb_a, 0x409, 0)); CHECK(c_find(L"h:mm:ss tt") >= 0);
}
TEST(calenum_time_formats_ex) {
    REQUIRE(UN(EnumTimeFormatsEx));
    c_reset(0); g_lp = 0; SET_ERR(0);
    BOOL r = UN(EnumTimeFormatsEx)(tcb_ex, L"en-US", 0, 0x99);
    CHECKF(r, "err %lu", GetLastError());
    CHECK(g_lp == 0x99); CHECK(c_find(L"h:mm:ss tt") >= 0);
    c_reset(0); CHECK(UN(EnumTimeFormatsEx)(tcb_ex, L"", 0, 0)); CHECK(c_find(L"HH:mm:ss") >= 0);
    c_reset(0); SET_ERR(0); CHECK(!UN(EnumTimeFormatsEx)(tcb_ex, L"no-such-locale-xx", 0, 0)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    c_reset(1); UN(EnumTimeFormatsEx)(tcb_ex, L"en-US", 0, 0); CHECK(g_cn == 1);
}
