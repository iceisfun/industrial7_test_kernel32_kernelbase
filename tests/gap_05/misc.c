/* GetPrivateProfileStructA, GetFirmwareEnvironmentVariableExA, GetGeoInfoEx, GetPackageInfo2,
 * GetGamingDeviceModelInformation, GetIntegratedDisplaySize, GetProcessorSystemCycleTime, GetNextUmsListItem */
#include "g5.h"

/* ======================= GetPrivateProfileStructA ======================= */
typedef BOOL (WINAPI *gps_t)(LPCSTR, LPCSTR, LPVOID, UINT, LPCSTR);
typedef BOOL (WINAPI *wps_t)(LPCSTR, LPCSTR, LPVOID, UINT, LPCSTR);
#define GPS ((gps_t)g5_proc("GetPrivateProfileStructA"))
#define WPS ((wps_t)g5_proc("WritePrivateProfileStructA"))

static void ini_path(char *out, size_t cap, const char *leaf) { t_tmpfile(out, cap, leaf); }
/* Write a raw "key=value" line using the (scaffolding) string writer. */
static void rawkey(const char *file, const char *sec, const char *key, const char *val) {
    WritePrivateProfileStringA(sec, key, val, file);
}
static void hexline(char *out, const BYTE *d, int n, int csum_delta) {
    BYTE sum = 0; char *o = out;
    for (int i = 0; i < n; i++) { o += sprintf(o, "%02X", d[i]); sum += d[i]; }
    sprintf(o, "%02X", (BYTE)(sum + csum_delta));
}

TEST(gap5_gps_exports) {
    CHECK(t_proc_k32("GetPrivateProfileStructA") != NULL);
    CHECK(t_proc_k32("GetPrivateProfileStructW") != NULL);
    CHECK(t_proc_k32("WritePrivateProfileStructA") != NULL);
}
TEST(gap5_gps_raw_hex_format) {
    REQUIRE(GPS);
    char f[MAX_PATH]; ini_path(f, sizeof f, "gap5_gps1.ini");
    BYTE d[] = { 0x01, 0x23, 0xAB, 0xCD, 0xEF, 0x00, 0xFF };
    char line[64]; hexline(line, d, sizeof d, 0);
    rawkey(f, "S", "K", line);
    BYTE out[16]; memset(out, 0xCC, sizeof out);
    SET_ERR(0);
    CHECK(GPS("S", "K", out, sizeof d, f));
    CHECK(!memcmp(out, d, sizeof d));
    EXPECT_EQ(out[sizeof d], 0xCC);                 /* checksum byte is not stored */
    /* lower-case hex is accepted */
    char low[64]; strcpy(low, line); for (char *c = low; *c; c++) *c = tolower(*c);
    rawkey(f, "S", "L", low);
    memset(out, 0xCC, sizeof out);
    CHECK(GPS("S", "L", out, sizeof d, f));
    CHECK(!memcmp(out, d, sizeof d));
    DeleteFileA(f);
}
TEST(gap5_gps_checksum_error) {
    WINDOWS_ONLY();
    REQUIRE(GPS);
    char f[MAX_PATH]; ini_path(f, sizeof f, "gap5_gps2.ini");
    BYTE d[] = { 1, 2, 3, 4 };
    char line[64];
    for (int delta = 1; delta <= 255; delta += 17) {
        hexline(line, d, sizeof d, delta);
        rawkey(f, "S", "K", line);
        BYTE out[8];
        SET_ERR(0);
        CHECKF(!GPS("S", "K", out, sizeof d, f), "delta %d", delta);
        EXPECT_ERR(ERROR_INVALID_DATA);
    }
    hexline(line, d, sizeof d, 0);
    rawkey(f, "S", "K", line);
    BYTE out[8]; CHECK(GPS("S", "K", out, sizeof d, f));
    DeleteFileA(f);
}
TEST(gap5_gps_length_errors) {
    WINDOWS_ONLY();
    REQUIRE(GPS);
    char f[MAX_PATH]; ini_path(f, sizeof f, "gap5_gps3.ini");
    BYTE d[] = { 0x10, 0x20, 0x30, 0x40 }, out[16];
    char line[64]; hexline(line, d, sizeof d, 0);
    rawkey(f, "S", "K", line);
    for (UINT n = 0; n <= 8; n++) {
        if (n == sizeof d) continue;
        SET_ERR(0);
        CHECKF(!GPS("S", "K", out, n, f), "uSize %u", n);
        EXPECT_ERR(ERROR_BAD_LENGTH);
    }
    /* trailing / missing characters */
    char longer[80]; snprintf(longer, sizeof longer, "%s00", line);
    rawkey(f, "S", "K2", longer);
    SET_ERR(0); CHECK(!GPS("S", "K2", out, sizeof d, f)); EXPECT_ERR(ERROR_BAD_LENGTH);
    line[strlen(line) - 1] = 0;
    rawkey(f, "S", "K3", line);
    SET_ERR(0); CHECK(!GPS("S", "K3", out, sizeof d, f)); EXPECT_ERR(ERROR_BAD_LENGTH);
    rawkey(f, "S", "K4", "");
    SET_ERR(0); CHECK(!GPS("S", "K4", out, sizeof d, f)); EXPECT_ERR(ERROR_BAD_LENGTH);
    DeleteFileA(f);
}
TEST(gap5_gps_missing_things) {
    WINDOWS_ONLY();
    REQUIRE(GPS);
    char f[MAX_PATH]; ini_path(f, sizeof f, "gap5_gps4.ini");
    BYTE out[8];
    SET_ERR(0); CHECK(!GPS("S", "K", out, 4, f)); EXPECT_ERR(ERROR_BAD_LENGTH);     /* no such file */
    rawkey(f, "S", "K", "00");
    SET_ERR(0); CHECK(!GPS("S", "Nokey", out, 4, f)); EXPECT_ERR(ERROR_BAD_LENGTH);
    SET_ERR(0); CHECK(!GPS("Nosection", "K", out, 4, f)); EXPECT_ERR(ERROR_BAD_LENGTH);
    DeleteFileA(f);
}
TEST(gap5_gps_zero_size_struct) {
    WINDOWS_ONLY();
    REQUIRE(GPS);
    char f[MAX_PATH]; ini_path(f, sizeof f, "gap5_gps5.ini");
    rawkey(f, "S", "K", "00");                  /* zero data bytes + checksum 0 */
    BYTE out[4] = { 0xCC };
    SET_ERR(0);
    CHECK(GPS("S", "K", out, 0, f));
    EXPECT_EQ(out[0], 0xCC);
    rawkey(f, "S", "K1", "01");                 /* wrong checksum for empty data */
    SET_ERR(0); CHECK(!GPS("S", "K1", out, 0, f)); EXPECT_ERR(ERROR_INVALID_DATA);
    DeleteFileA(f);
}
TEST(gap5_gps_huge_size_invalid_parameter) {
    WINDOWS_ONLY();
    REQUIRE(GPS);
    char f[MAX_PATH]; ini_path(f, sizeof f, "gap5_gps6.ini");
    BYTE out[4];
    UINT bad[] = { 0x7ffffffb, 0x7ffffffc, 0x80000000, 0xFFFFFFF0, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SET_ERR(0);
        CHECKF(!GPS("S", "K", out, bad[i], f), "size 0x%x", bad[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
    DeleteFileA(f);
}
TEST(gap5_gps_sizes_around_stack_threshold) {
    WINDOWS_ONLY();
    REQUIRE(GPS); REQUIRE(WPS);
    /* uSize*2+10 <= 0x100 uses the stack buffer (uSize <= 123); above that the heap */
    char f[MAX_PATH]; ini_path(f, sizeof f, "gap5_gps7.ini");
    static const UINT sizes[] = { 1, 2, 3, 100, 122, 123, 124, 125, 126, 200, 500, 1000, 2000 };
    for (unsigned i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        UINT n = sizes[i];
        BYTE *src = malloc(n), *dst = malloc(n + 4);
        for (UINT k = 0; k < n; k++) src[k] = (BYTE)(k * 7 + 3);
        memset(dst, 0xCC, n + 4);
        BOOL w = WPS("S", "K", src, n, f);
        if (!w) { CHECKF(0, "write size %u err %lu", n, GetLastError()); free(src); free(dst); continue; }
        SET_ERR(0);
        BOOL r = GPS("S", "K", dst, n, f);
        CHECKF(r, "size %u err %lu", n, GetLastError());
        if (r) { CHECKF(!memcmp(src, dst, n), "size %u data", n); CHECKF(dst[n] == 0xCC, "overrun size %u", n); }
        SET_ERR(0);
        CHECKF(!GPS("S", "K", dst, n + 1, f), "size %u+1", n);
        EXPECT_ERR(ERROR_BAD_LENGTH);
        free(src); free(dst);
    }
    DeleteFileA(f);
}
TEST(gap5_gps_roundtrip_struct) {
    REQUIRE(GPS); REQUIRE(WPS);
    struct S { int a; short b; char c[10]; double d; } s = { -5, 77, "hello", 3.25 }, o;
    char f[MAX_PATH]; ini_path(f, sizeof f, "gap5_gps8.ini");
    CHECK(WPS("Sect", "Key", &s, sizeof s, f));
    memset(&o, 0, sizeof o);
    CHECK(GPS("Sect", "Key", &o, sizeof s, f));
    CHECK(!memcmp(&s, &o, sizeof s));
    /* section/key names are case-insensitive */
    memset(&o, 0, sizeof o);
    CHECK(GPS("SECT", "kEY", &o, sizeof s, f));
    CHECK(!memcmp(&s, &o, sizeof s));
    /* the file holds a checksum-terminated hex string */
    char buf[256];
    GetPrivateProfileStringA("Sect", "Key", "", buf, sizeof buf, f);
    EXPECT_EQ(strlen(buf), sizeof s * 2 + 2);
    for (char *c = buf; *c; c++) CHECKF(isxdigit((unsigned char)*c), "non-hex char %c", *c);
    /* overwrite then read again */
    s.a = 12345;
    CHECK(WPS("Sect", "Key", &s, sizeof s, f));
    CHECK(GPS("Sect", "Key", &o, sizeof s, f)); EXPECT_EQ(o.a, 12345);
    DeleteFileA(f);
}
TEST(gap5_gps_other_keys_not_disturbed) {
    WINDOWS_ONLY();
    REQUIRE(GPS); REQUIRE(WPS);
    char f[MAX_PATH]; ini_path(f, sizeof f, "gap5_gps9.ini");
    BYTE a[3] = { 1, 2, 3 }, b[5] = { 9, 8, 7, 6, 5 }, o[8];
    CHECK(WPS("A", "x", a, 3, f));
    CHECK(WPS("B", "x", b, 5, f));
    CHECK(WPS("A", "y", b, 5, f));
    CHECK(GPS("A", "x", o, 3, f) && !memcmp(o, a, 3));
    CHECK(GPS("B", "x", o, 5, f) && !memcmp(o, b, 5));
    CHECK(GPS("A", "y", o, 5, f) && !memcmp(o, b, 5));
    SET_ERR(0); CHECK(!GPS("A", "x", o, 5, f)); EXPECT_ERR(ERROR_BAD_LENGTH);
    DeleteFileA(f);
}
TEST(gap5_gps_value_with_whitespace_and_quotes) {
    REQUIRE(GPS);
    char f[MAX_PATH]; ini_path(f, sizeof f, "gap5_gps10.ini");
    BYTE d[] = { 0xAA, 0x55 }, out[4];
    char line[32]; hexline(line, d, 2, 0);
    char padded[64]; snprintf(padded, sizeof padded, "  %s  ", line);
    /* write the raw file text so leading/trailing blanks reach the parser */
    HANDLE h = CreateFileA(f, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    char txt[200]; int n = snprintf(txt, sizeof txt, "[S]\r\nK =  %s  \r\nQ=\"%s\"\r\n", line, line);
    DWORD w; WriteFile(h, txt, n, &w, NULL); CloseHandle(h);
    SET_ERR(0);
    CHECK(GPS("S", "K", out, 2, f));           /* surrounding blanks are trimmed by the string reader */
    CHECK(out[0] == 0xAA && out[1] == 0x55);
    SET_ERR(0);
    BOOL q = GPS("S", "Q", out, 2, f);        /* matching quotes are stripped by GetPrivateProfileString */
    t_note("quoted value: ret=%d err=%lu", q, GetLastError());
    DeleteFileA(f);
}
TEST(gap5_gps_untouched_on_length_error) {
    WINDOWS_ONLY();
    REQUIRE(GPS);
    char f[MAX_PATH]; ini_path(f, sizeof f, "gap5_gps11.ini");
    rawkey(f, "S", "K", "0102");
    BYTE out[4] = { 0xCC, 0xCC, 0xCC, 0xCC };
    SET_ERR(0);
    CHECK(!GPS("S", "K", out, 2, f)); EXPECT_ERR(ERROR_BAD_LENGTH);     /* "0102" has 1 data byte + checksum */
    EXPECT_EQ(out[0], 0xCC);
    CHECK(GPS("S", "K", out, 1, f) == (0x01 == 0x02));                   /* data 01, checksum 02 -> mismatch */
    DeleteFileA(f);
}

/* ======================= GetFirmwareEnvironmentVariableExA ======================= */
typedef DWORD (WINAPI *fwA_t)(LPCSTR, LPCSTR, PVOID, DWORD, PDWORD);
#define FWA ((fwA_t)t_proc_k32("GetFirmwareEnvironmentVariableExA"))
TEST(gap5_fw_null_arguments) {
    REQUIRE(FWA);
    BYTE buf[16]; DWORD attr = 0xCC;
    const char *guid = "{00000000-0000-0000-0000-000000000000}";
    SET_ERR(0); EXPECT_EQ(FWA(NULL, guid, buf, sizeof buf, &attr), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(FWA("k32t_none", NULL, buf, sizeof buf, &attr), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(FWA(NULL, NULL, NULL, 0, NULL), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    EXPECT_EQ(attr, 0xCC);
}
TEST(gap5_fw_nonexistent_variable_read_only) {
    REQUIRE(FWA);
    /* A read of a variable that does not exist. Without SeSystemEnvironmentPrivilege (the normal case) the call
     * fails before touching firmware; on legacy-BIOS systems it reports not-supported. Never succeeds. */
    BYTE buf[16]; DWORD attr = 0;
    SET_ERR(0);
    DWORD r = FWA("K32T_NO_SUCH_VAR", "{00000000-0000-0000-0000-000000000000}", buf, sizeof buf, &attr);
    EXPECT_EQ(r, 0);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_PRIVILEGE_NOT_HELD || e == ERROR_INVALID_FUNCTION || e == ERROR_ENVVAR_NOT_FOUND || e == ERROR_NOACCESS ||
           e == ERROR_INVALID_PARAMETER || e == ERROR_NOT_SUPPORTED, "unexpected error %lu", e);
    t_note("firmware read error=%lu", e);
}
TEST(gap5_fw_malformed_guid) {
    REQUIRE(FWA);
    BYTE buf[8]; DWORD attr = 0;
    static const char *bad[] = { "", "not-a-guid", "{}", "{00000000-0000-0000-0000-00000000000}", "00000000-0000-0000-0000-000000000000" };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SET_ERR(0);
        CHECKF(FWA("K32T_NO_SUCH_VAR", bad[i], buf, sizeof buf, &attr) == 0, "guid '%s'", bad[i]);
        t_note("guid '%s' -> err %lu", bad[i], GetLastError());
    }
}

/* ======================= GetGeoInfoEx ======================= */
typedef int (WINAPI *geoex_t)(PWSTR, DWORD, PWSTR, int);
#define GEOEX ((geoex_t)g5_proc("GetGeoInfoEx"))
#define GEO_ISO2_T 4
#define GEO_ISO3_T 5
#define GEO_FRIENDLY_T 8
#define GEO_OFFICIAL_T 9
#define GEO_TZ_T 10
#define GEO_LANG_T 11
#define GEO_UNNUM_T 12
#define GEO_PARENT_T 13
#define GEO_DIAL_T 14
#define GEO_CURCODE_T 15
#define GEO_CURSYM_T 16
#define GEO_NAME_T 17
#define GEO_ID_T 18

static int geo(const WCHAR *region, DWORD type, WCHAR *buf, int cch) {
    return GEOEX((PWSTR)region, type, buf, cch);
}
static void geostr(const WCHAR *region, DWORD type, char *out, size_t cap) {
    WCHAR w[128] = { 0 };
    int n = geo(region, type, w, 128);
    if (n <= 0) { snprintf(out, cap, "<fail %lu>", GetLastError()); return; }
    g5_w2a(w, out, (int)cap);
}
TEST(gap5_geo_exports) {
    CHECK(t_proc_k32("GetGeoInfoEx") != NULL);
    CHECK(t_proc_kb("GetGeoInfoEx") != NULL || t_is_wine());
}
TEST(gap5_geo_known_regions) {
    WINDOWS_ONLY();
    REQUIRE(GEOEX);
    static const struct { const WCHAR *name; const char *iso3; const char *id; const char *un; } t[] = {
        { L"US", "USA", "244", "840" }, { L"DE", "DEU", "94", "276" }, { L"JP", "JPN", "122", "392" },
        { L"GB", "GBR", "242", "826" }, { L"FR", "FRA", "84", "250" }, { L"CA", "CAN", "39", "124" },
        { L"AU", "AUS", "12", "036" }, { L"CN", "CHN", "45", "156" }, { L"IN", "IND", "113", "356" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        char a[64];
        SET_ERR(0);
        WCHAR w[32] = { 0 };
        int n = geo(t[i].name, GEO_ISO2_T, w, 32);
        CHECKF(n == 3, "region %u: iso2 ret %d err %lu", i, n, GetLastError());
        CHECK(!lstrcmpW(w, t[i].name));
        geostr(t[i].name, GEO_ISO3_T, a, sizeof a);      CHECKF(!strcmp(a, t[i].iso3), "%s", a);
        geostr(t[i].name, GEO_ID_T, a, sizeof a);        CHECKF(!strcmp(a, t[i].id), "%s want %s", a, t[i].id);
        geostr(t[i].name, GEO_UNNUM_T, a, sizeof a);     CHECKF(!strcmp(a, t[i].un) || !strcmp(a, t[i].un + (t[i].un[0] == '0')), "%s want %s", a, t[i].un);
        geostr(t[i].name, GEO_NAME_T, a, sizeof a);
        WCHAR wn[16]; geo(t[i].name, GEO_NAME_T, wn, 16); CHECK(!lstrcmpW(wn, t[i].name));
    }
}
TEST(gap5_geo_region_name_case_insensitive) {
    WINDOWS_ONLY();
    REQUIRE(GEOEX);
    WCHAR a[16] = { 0 }, b[16] = { 0 };
    CHECK(geo(L"us", GEO_ISO3_T, a, 16) == 4);
    CHECK(geo(L"US", GEO_ISO3_T, b, 16) == 4);
    CHECK(!lstrcmpW(a, b));
    CHECK(geo(L"Us", GEO_ISO3_T, a, 16) == 4);
    /* the canonical name is returned for GEO_NAME regardless of the input spelling */
    CHECK(geo(L"us", GEO_NAME_T, a, 16) == 3); CHECK(!lstrcmpW(a, L"US"));
}
TEST(gap5_geo_size_query_and_truncation) {
    WINDOWS_ONLY();
    REQUIRE(GEOEX);
    SET_ERR(0);
    EXPECT_EQ(geo(L"US", GEO_ISO2_T, NULL, 0), 3);              /* chars incl. NUL */
    EXPECT_EQ(geo(L"US", GEO_ISO3_T, NULL, 0), 4);
    EXPECT_EQ(geo(L"US", GEO_ID_T, NULL, 0), 4);
    WCHAR w[8];
    for (int i = 0; i < 8; i++) w[i] = 0x7777;
    EXPECT_EQ(geo(L"US", GEO_ISO3_T, w, 0), 4);                 /* cch==0 with a buffer is still a size query */
    EXPECT_EQ(w[0], 0x7777);
    for (int cch = 1; cch < 4; cch++) {
        for (int i = 0; i < 8; i++) w[i] = 0x7777;
        SET_ERR(0);
        CHECKF(geo(L"US", GEO_ISO3_T, w, cch) == 0, "cch %d", cch);
        EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
        EXPECT_EQ(w[0], 0x7777);                               /* nothing copied */
    }
    for (int i = 0; i < 8; i++) w[i] = 0x7777;
    SET_ERR(0);
    EXPECT_EQ(geo(L"US", GEO_ISO3_T, w, 4), 4);
    EXPECT_EQ(w[3], 0); EXPECT_EQ(w[4], 0x7777); CHECK(!lstrcmpW(w, L"USA"));
    for (int i = 0; i < 8; i++) w[i] = 0x7777;
    EXPECT_EQ(geo(L"US", GEO_ISO3_T, w, 8), 4);
    EXPECT_EQ(w[4], 0x7777);
}
TEST(gap5_geo_invalid_parameters) {
    WINDOWS_ONLY();
    REQUIRE(GEOEX);
    WCHAR w[16];
    SET_ERR(0); EXPECT_EQ(geo(NULL, GEO_ISO2_T, w, 16), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(geo(L"US", GEO_ISO2_T, w, -1), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(geo(L"US", GEO_ISO2_T, w, 0x80000000), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(geo(L"US", GEO_ISO2_T, NULL, 1), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(geo(L"US", GEO_ISO2_T, NULL, 16), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(geo(L"", GEO_ISO2_T, w, 16), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(geo(L"k32t", GEO_ISO2_T, w, 16), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(geo(L"USA1", GEO_ISO2_T, w, 16), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(geo(L" US", GEO_ISO2_T, w, 16), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(geo(L"en-US", GEO_ISO2_T, w, 16), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap5_geo_invalid_geotype) {
    REQUIRE(GEOEX);
    WCHAR w[32];
    static const DWORD bad[] = { 0, 1, 6, 7, 0x13, 0x14, 0x100, 0x7fffffff, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SET_ERR(0);
        CHECKF(geo(L"US", bad[i], w, 32) == 0, "type 0x%lx", bad[i]);
        EXPECT_ERR(ERROR_INVALID_FLAGS);
    }
    /* the size-query form is equally rejected */
    SET_ERR(0); EXPECT_EQ(geo(L"US", 0x13, NULL, 0), 0); EXPECT_ERR(ERROR_INVALID_FLAGS);
}
TEST(gap5_geo_all_supported_types_observed) {
    REQUIRE(GEOEX);
    char a[128];
    for (DWORD t = 2; t <= 18; t++) {
        WCHAR w[128] = { 0 };
        SET_ERR(0);
        int n = geo(L"US", t, w, 128);
        g5_w2a(w, a, sizeof a);
        t_note("US type %lu -> ret %d '%s' err=%lu", t, n, a, GetLastError());
        if (n > 0) CHECKF(lstrlenW(w) == n - 1, "type %lu length", t);
    }
}
TEST(gap5_geo_latitude_longitude_numeric) {
    WINDOWS_ONLY();
    REQUIRE(GEOEX);
    WCHAR w[32] = { 0 };
    int n = geo(L"US", 2, w, 32);
    CHECK(n > 1);
    if (n > 1) { double lat = _wtof(w); CHECK(lat > 20 && lat < 75); }
    n = geo(L"US", 3, w, 32);
    CHECK(n > 1);
    if (n > 1) { double lon = _wtof(w); CHECK(lon < -60 && lon > -180); }
    n = geo(L"JP", 3, w, 32);
    if (n > 1) CHECK(_wtof(w) > 100);
    n = geo(L"DE", 2, w, 32);
    if (n > 1) CHECK(_wtof(w) > 45 && _wtof(w) < 56);
}
TEST(gap5_geo_names_and_parent) {
    WINDOWS_ONLY();
    REQUIRE(GEOEX);
    char a[128];
    geostr(L"US", GEO_FRIENDLY_T, a, sizeof a); CHECKF(strstr(a, "United States") != NULL, "%s", a);
    geostr(L"DE", GEO_FRIENDLY_T, a, sizeof a); CHECKF(strstr(a, "Germany") != NULL, "%s", a);
    geostr(L"JP", GEO_OFFICIAL_T, a, sizeof a); CHECKF(strstr(a, "Japan") != NULL, "%s", a);
    geostr(L"US", GEO_PARENT_T, a, sizeof a);   t_note("US parent: %s", a);
    geostr(L"US", GEO_CURCODE_T, a, sizeof a);  t_note("US currency code: %s", a);
    geostr(L"US", GEO_DIAL_T, a, sizeof a);     t_note("US dialing code: %s", a);
}
TEST(gap5_geo_world_region) {
    REQUIRE(GEOEX);
    char a[64];
    SET_ERR(0);
    int n = geo(L"001", GEO_NAME_T, NULL, 0);
    t_note("region 001: name size %d err=%lu", n, GetLastError());
    if (n > 0) { geostr(L"001", GEO_ID_T, a, sizeof a); t_note("001 id: %s", a); }
}
TEST(gap5_geo_repeated_calls_stable) {
    REQUIRE(GEOEX);
    WCHAR first[32] = { 0 }; geo(L"FR", GEO_ISO3_T, first, 32);
    for (int i = 0; i < 1000; i++) {
        WCHAR w[32] = { 0 };
        if (geo(L"FR", GEO_ISO3_T, w, 32) != 4 || lstrcmpW(w, first)) { CHECK(0); break; }
    }
}

/* ======================= GetPackageInfo2 ======================= */
typedef LONG (WINAPI *pi2_t)(void *, UINT32, int, UINT32 *, BYTE *, UINT32 *);
#define PI2 ((pi2_t)g5_proc("GetPackageInfo2"))
TEST(gap5_pkg_exports) {
    CHECK(t_proc_kb("GetPackageInfo2") != NULL || t_is_wine());
    CHECK(t_proc_kb("GetPackageInfo") != NULL || t_is_wine());
}
TEST(gap5_pkg_null_reference_all_path_types) {
    REQUIRE(PI2);
    UINT32 len = 0, cnt = 0; BYTE buf[64];
    for (int pt = 0; pt <= 5; pt++) {
        SET_ERR(0);
        LONG r = PI2(NULL, 0, pt, &len, buf, &cnt);
        CHECKF(r == ERROR_INVALID_PARAMETER, "path type %d -> %ld", pt, r);
    }
}
TEST(gap5_pkg_invalid_path_type) {
    REQUIRE(PI2);
    UINT32 len = 0, cnt = 0; BYTE buf[64];
    static const int bad[] = { 6, 7, 8, 0xF, 0x12, 0x20, 100, -1, 0x7fffffff };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        LONG r = PI2(NULL, 0, bad[i], &len, buf, &cnt);
        CHECKF(r == ERROR_INVALID_PARAMETER, "path type %d -> %ld", bad[i], r);
    }
    /* the two reserved types report "not supported" */
    EXPECT_EQ(PI2(NULL, 0, 0x10, &len, buf, &cnt), ERROR_NOT_SUPPORTED);
    EXPECT_EQ(PI2(NULL, 0, 0x11, &len, buf, &cnt), ERROR_NOT_SUPPORTED);
}
TEST(gap5_pkg_flags_do_not_rescue_null_ref) {
    REQUIRE(PI2);
    UINT32 len = 0, cnt = 0;
    static const UINT32 fl[] = { 0, 1, 0x10, 0x20, 0x40, 0x80, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof fl / sizeof *fl; i++)
        CHECKF(PI2(NULL, fl[i], 0, &len, NULL, &cnt) == ERROR_INVALID_PARAMETER, "flags 0x%x", fl[i]);
}
TEST(gap5_pkg_invalid_ref_null_buffer_len) {
    REQUIRE(PI2);
    UINT32 cnt = 0;
    EXPECT_EQ(PI2(NULL, 0, 0, NULL, NULL, &cnt), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(PI2(NULL, 0, 0, NULL, NULL, NULL), ERROR_INVALID_PARAMETER);
}
TEST(gap5_pkg_returns_plain_win32_codes) {
    REQUIRE(PI2);
    /* PackageInfo2 strips the HRESULT_FROM_WIN32 wrapper: the result is a bare error number */
    UINT32 len = 0;
    LONG r = PI2(NULL, 0, 0, &len, NULL, NULL);
    CHECK(r > 0 && r < 0x10000);
}

/* ======================= GetGamingDeviceModelInformation ======================= */
typedef HRESULT (WINAPI *gdm_t)(DWORD *);
#define GDM ((gdm_t)g5_proc("GetGamingDeviceModelInformation"))
TEST(gap5_gaming_basic) {
    REQUIRE(GDM);
    DWORD info[2] = { 0xCCCCCCCC, 0xCCCCCCCC };
    HRESULT hr = GDM(info);
    EXPECT_EQ(hr, S_OK);
    t_note("gaming device: vendor=0x%lx model=0x%lx", info[0], info[1]);
    if (!t_is_wine()) { EXPECT_EQ(info[0], 0); EXPECT_EQ(info[1], 0); }   /* only non-zero on Xbox device families */
}
TEST(gap5_gaming_idempotent_and_cached) {
    REQUIRE(GDM);
    DWORD a[2] = { 1, 1 }, b[2] = { 2, 2 };
    EXPECT_EQ(GDM(a), S_OK);
    for (int i = 0; i < 100; i++) { EXPECT_EQ(GDM(b), S_OK); if (memcmp(a, b, 8)) { CHECK(0); break; } }
}
TEST(gap5_gaming_writes_eight_bytes) {
    REQUIRE(GDM);
    BYTE buf[24]; memset(buf, 0xAB, sizeof buf);
    EXPECT_EQ(GDM((DWORD *)(buf + 8)), S_OK);
    for (int i = 0; i < 8; i++) CHECKF(buf[i] == 0xAB && buf[16 + i] == 0xAB, "guard %d", i);
}

/* ======================= GetIntegratedDisplaySize ======================= */
typedef HRESULT (WINAPI *ids_t)(double *);
#define IDS ((ids_t)g5_proc("GetIntegratedDisplaySize"))
TEST(gap5_ids_null_pointer) {
    WINDOWS_ONLY();
    REQUIRE(IDS);
    EXPECT_EQ(IDS(NULL), E_INVALIDARG);
}
TEST(gap5_ids_observed_result) {
    WINDOWS_ONLY();
    REQUIRE(IDS);
    double d = -1.0;
    HRESULT hr = IDS(&d);
    t_note("GetIntegratedDisplaySize hr=0x%08lx size=%f", hr, d);
    if (SUCCEEDED(hr)) CHECK(d > 3.0 && d < 100.0);          /* inches, diagonal */
    else CHECK(d == -1.0);                                    /* untouched on failure (EXPECT_EQ would cast the double to an integer) */
    if (!SUCCEEDED(hr)) CHECK(hr < 0);
}
TEST(gap5_ids_repeatable) {
    REQUIRE(IDS);
    double a = -1, b = -1;
    HRESULT h1 = IDS(&a), h2 = IDS(&b);
    EXPECT_EQ(h1, h2);
    if (SUCCEEDED(h1)) CHECK(a == b);
}

/* ======================= GetProcessorSystemCycleTime ======================= */
typedef BOOL (WINAPI *gcyc_t)(USHORT, PSYSTEM_PROCESSOR_CYCLE_TIME_INFORMATION, PDWORD);
#define GCYC ((gcyc_t)g5_proc("GetProcessorSystemCycleTime"))
TEST(gap5_cyc_exports) {
    CHECK(t_proc_k32("GetProcessorSystemCycleTime") != NULL || t_is_wine());
    CHECK(t_proc_kb("GetProcessorSystemCycleTime") != NULL || t_is_wine());
}
TEST(gap5_cyc_null_returned_length) {
    REQUIRE(GCYC);
    SYSTEM_PROCESSOR_CYCLE_TIME_INFORMATION buf[4];
    SET_ERR(0); EXPECT_EQ(GCYC(0, buf, NULL), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(GCYC(0, NULL, NULL), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(GCYC(5, buf, NULL), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap5_cyc_size_query) {
    REQUIRE(GCYC);
    DWORD len = 0; SET_ERR(0);
    BOOL r0 = GCYC(0, NULL, &len);      /* single evaluation: the call updates len */
    EXPECT_EQ(r0, 0);
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    DWORD ncpu = GetActiveProcessorCount(0);
    CHECK(len % 8 == 0);
    if (len != ncpu * 8) t_note("required length %lu, active processors in group 0: %lu", len, ncpu);
    CHECK(len >= 8);
    SYSTEM_PROCESSOR_CYCLE_TIME_INFORMATION one;
    len = sizeof one - 1; SET_ERR(0);
    BOOL r1 = GCYC(0, &one, &len); DWORD e1 = GetLastError();
    EXPECT_EQ(r1, 0); EXPECT_EQ(e1, ERROR_INSUFFICIENT_BUFFER);
    len = 0; SET_ERR(0);
    BOOL r2 = GCYC(0, &one, &len); DWORD e2 = GetLastError();
    EXPECT_EQ(r2, 0); EXPECT_EQ(e2, ERROR_INSUFFICIENT_BUFFER);
}
TEST(gap5_cyc_fetch) {
    REQUIRE(GCYC);
    DWORD need = 0;
    GCYC(0, NULL, &need);
    CHECK(need >= 8);
    DWORD n = need / 8;
    SYSTEM_PROCESSOR_CYCLE_TIME_INFORMATION *b = calloc(n + 4, sizeof *b);
    DWORD len = need; SET_ERR(0);
    CHECK(GCYC(0, b, &len));
    EXPECT_EQ(len, need);
    int nz = 0; for (DWORD i = 0; i < n; i++) if (b[i].CycleTime) nz++;
    CHECK(nz > 0);
    /* bigger buffer than needed: the kernel's length rule for this class is not in the decompilation, so observe only */
    len = need + 64; memset(b, 0, (n + 4) * sizeof *b);
    SET_ERR(0);
    BOOL r = GCYC(0, b, &len);
    t_note("oversized buffer: ret=%d len=%lu (need %lu) err=%lu", r, len, need, GetLastError());
    /* one entry short */
    /* call once: GCYC updates len, and EXPECT_EQ evaluates its argument twice */
    if (n > 1) { len = need - 8; SET_ERR(0); BOOL r1 = GCYC(0, b, &len); DWORD e1 = GetLastError(); EXPECT_EQ(r1, 0); EXPECT_EQ(e1, ERROR_INSUFFICIENT_BUFFER); }
    free(b);
}
TEST(gap5_cyc_monotonic) {
    REQUIRE(GCYC);
    DWORD need = 0; GCYC(0, NULL, &need);
    DWORD n = need / 8; if (!n) { t_skip("no data"); return; }
    SYSTEM_PROCESSOR_CYCLE_TIME_INFORMATION *a = calloc(n, 8), *b = calloc(n, 8);
    DWORD l1 = need, l2 = need;
    CHECK(GCYC(0, a, &l1));
    DWORD t0 = GetTickCount(); volatile unsigned long long x = 0;
    while (GetTickCount() - t0 < 50) for (int i = 0; i < 1000; i++) x += i;   /* burn some cycles */
    CHECK(GCYC(0, b, &l2));
    unsigned long long sa = 0, sb = 0;
    for (DWORD i = 0; i < n; i++) { CHECKF(b[i].CycleTime >= a[i].CycleTime, "cpu %lu went backwards", i); sa += a[i].CycleTime; sb += b[i].CycleTime; }
    if (sb == sa) t_note("no system cycles accumulated over 50ms");
    free(a); free(b);
}
TEST(gap5_cyc_invalid_group) {
    REQUIRE(GCYC);
    WORD groups = GetActiveProcessorGroupCount();
    SYSTEM_PROCESSOR_CYCLE_TIME_INFORMATION buf[8];
    static const USHORT bad[] = { 0xFFFF, 0x8000, 64, 100 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        if (bad[i] < groups) continue;
        DWORD len = sizeof buf; SET_ERR(0);
        CHECKF(!GCYC(bad[i], buf, &len), "group %u", bad[i]);
        t_note("group %u -> err %lu len %lu", bad[i], GetLastError(), len);
    }
    if (groups == 1) {
        DWORD len = sizeof buf; SET_ERR(0);
        CHECK(!GCYC(1, buf, &len));
        t_note("group 1 on single-group system -> err %lu", GetLastError());
    }
}

/* ======================= GetNextUmsListItem ======================= */
TEST(gap5_ums_export_only) {
    /* User-mode scheduling needs a scheduler thread context; a bogus UMS context pointer is dereferenced by
     * the callee, so no call is made here. Only the export is verified. */
    CHECK(t_proc_k32("GetNextUmsListItem") != NULL);
    CHECK(t_proc_k32("QueryUmsThreadInformation") != NULL);
    CHECK(t_proc_k32("DequeueUmsCompletionListItems") != NULL);
}
