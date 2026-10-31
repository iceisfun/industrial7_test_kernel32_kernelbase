/* GetPrivateProfileSectionA/W and GetPrivateProfileSectionNamesA/W */
#include "ap.h"

#define SECFILE "[section1]\r\nname1=val1\r\nname2=\r\nname3\r\nname4=val4\r\n[section2]\r\n[section3]\r\n=val5\r\n"

static DWORD gsec(const char *s, char *b, DWORD n, const char *f) { return U(GetPrivateProfileSectionA)(s, b, n, f); }
static DWORD gnames(char *b, DWORD n, const char *f) { return U(GetPrivateProfileSectionNamesA)(b, n, f); }

TEST(profile_section_basic) {
    REQUIRE(U(GetPrivateProfileSectionA));
    char path[MAX_PATH]; ap_ini(path, "sec.ini", SECFILE); char b[200], f[200];
    memset(b, 'z', sizeof b);
    SET_ERR(0xdeadbeef);
    DWORD r = gsec("section1", b, sizeof b, path);
    EXPECT_EQ(r, 35);
    EXPECT_ERR(0);
    ap_flat(b, sizeof b, f, sizeof f);
    EXPECT_STR(f, "name1=val1|name2=|name3|name4=val4");
    EXPECT_EQ(b[r - 1], 0); EXPECT_EQ(b[r], 0);
    ap_rm(path);
}

TEST(profile_section_empty_and_value_only) {
    REQUIRE(U(GetPrivateProfileSectionA));
    char path[MAX_PATH]; ap_ini(path, "sec2.ini", SECFILE); char b[64], f[64];
    memset(b, 'z', sizeof b);
    SET_ERR(0xdeadbeef);
    EXPECT_EQ(gsec("section2", b, sizeof b, path), 0);
    EXPECT_ERR(0);
    CHECK(b[0] == 0 && b[1] == 0);
    memset(b, 'z', sizeof b);
    DWORD r = gsec("section3", b, sizeof b, path);
    EXPECT_EQ(r, 6);
    ap_flat(b, sizeof b, f, sizeof f);
    EXPECT_STR(f, "=val5");
    EXPECT_EQ(b[5], 0); EXPECT_EQ(b[6], 0);
    ap_rm(path);
}

TEST(profile_section_missing_section_and_file) {
    REQUIRE(U(GetPrivateProfileSectionA));
    char path[MAX_PATH]; ap_ini(path, "sec3.ini", SECFILE); char b[32];
    memset(b, 'z', sizeof b);
    SET_ERR(0xdeadbeef);
    EXPECT_EQ(gsec("nosuch", b, sizeof b, path), 0);
    t_note("missing section: err=%lu b0=%d b1=%d", GetLastError(), b[0], b[1]);
    CHECK(b[0] == 0 && b[1] == 0);
    char gone[MAX_PATH]; t_tmpfile(gone, sizeof gone, "sec_gone.ini"); ap_rm(gone);
    memset(b, 'z', sizeof b);
    SET_ERR(0xdeadbeef);
    EXPECT_EQ(gsec("section1", b, sizeof b, gone), 0);
    t_note("missing file: err=%lu", GetLastError());
    CHECK(b[0] == 0);
    ap_rm(path);
}

TEST(profile_section_invalid_parameters) {
    REQUIRE(U(GetPrivateProfileSectionA));
    char path[MAX_PATH]; ap_ini(path, "sec4.ini", SECFILE); char b[32];
    SET_ERR(0xdeadbeef);
    EXPECT_EQ(U(GetPrivateProfileSectionA)(NULL, NULL, 0, NULL), 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0xdeadbeef);
    EXPECT_EQ(U(GetPrivateProfileSectionA)(NULL, NULL, 0, path), 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0xdeadbeef);
    EXPECT_EQ(U(GetPrivateProfileSectionA)("section1", NULL, 0, path), 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0xdeadbeef);
    EXPECT_EQ(U(GetPrivateProfileSectionA)(NULL, b, sizeof b, path), 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    ap_rm(path);
}

TEST(profile_section_invalid_parameters_w) {
    REQUIRE(U(GetPrivateProfileSectionW));
    char path[MAX_PATH]; ap_ini(path, "sec4w.ini", SECFILE); WCHAR wp[MAX_PATH], b[16]; ap_wpath(wp, path);
    SET_ERR(0xdeadbeef);
    EXPECT_EQ(U(GetPrivateProfileSectionW)(NULL, b, 16, wp), 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0xdeadbeef);
    EXPECT_EQ(U(GetPrivateProfileSectionW)(L"section1", NULL, 0, wp), 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    ap_rm(path);
}

/* Section content is "name1=val1\0name2=\0name3\0name4=val4\0" = 35 chars; needs nSize >= 37 */
static DWORD gsec_cb(char *b, DWORD n) { extern char g_sec_path[]; return gsec("section1", b, n, g_sec_path); }
char g_sec_path[MAX_PATH];
TEST(profile_section_truncation_model) {
    REQUIRE(U(GetPrivateProfileSectionA));
    ap_ini(g_sec_path, "sec5.ini", SECFILE);
    const char data[] = "name1=val1\0name2=\0name3\0name4=val4\0";
    size_t T = sizeof data - 1;
    EXPECT_EQ(T, 35);
    for (DWORD n = 0; n <= T + 3; n++) {
        if (t_is_wine() && n < 2) continue;    /* wine faults on nSize 0/1 */
        char b[100]; memset(b, 'z', sizeof b);
        DWORD r = gsec_cb(b, n);
        DWORD wantr = n >= T + 2 ? (DWORD)T : (n >= 2 ? n - 2 : 0);
        CHECKF(r == wantr, "nSize %lu: ret %lu want %lu", n, r, wantr);
        if (n == 0) { CHECK(b[0] == 'z'); continue; }
        DWORD keep = wantr;
        CHECKF(!memcmp(b, data, keep), "nSize %lu prefix", n);
        CHECKF(b[keep] == 0, "nSize %lu term", n);
        if (!t_is_wine() && keep < T && n >= 2) CHECKF(b[keep + 1] == 0, "nSize %lu double NUL", n);
        CHECKF(n > T + 2 || b[n] == 'z', "nSize %lu overrun", n);
    }
    ap_rm(g_sec_path);
}

TEST(profile_section_overflow_wine_example) {
    REQUIRE(U(GetPrivateProfileSectionA));
    char path[MAX_PATH]; ap_ini(path, "sec6.ini", SECFILE); char b[100], f[100];
    memset(b, 'z', sizeof b);
    EXPECT_EQ(gsec("section1", b, 24, path), 22);
    ap_flat(b, 24, f, sizeof f);
    EXPECT_STR(f, "name1=val1|name2=|name");
    EXPECT_EQ(b[22], 0);
    ap_rm(path);
}

TEST(profile_section_size_one_and_zero) {
    REQUIRE(U(GetPrivateProfileSectionA));
    char path[MAX_PATH]; ap_ini(path, "sec7.ini", SECFILE); char b[8];
    if (!t_is_wine()) {   /* wine faults on nSize 0/1 */
    memset(b, 'z', sizeof b); EXPECT_EQ(gsec("section1", b, 0, path), 0); EXPECT_EQ(b[0], 'z');
    memset(b, 'z', sizeof b); EXPECT_EQ(gsec("section1", b, 1, path), 0); EXPECT_EQ(b[0], 0); EXPECT_EQ(b[1], 'z');
    }
    memset(b, 'z', sizeof b); EXPECT_EQ(gsec("section1", b, 2, path), 0); EXPECT_EQ(b[0], 0); EXPECT_EQ(b[2], 'z');
    ap_rm(path);
}

TEST(profile_section_case_insensitive_and_content) {
    REQUIRE(U(GetPrivateProfileSectionA));
    char path[MAX_PATH]; ap_ini(path, "sec8.ini", "[MixedCase]\r\nKeyA=1\r\nkeyB=2\r\n"); char b[64], f[64];
    EXPECT_EQ(gsec("MIXEDCASE", b, sizeof b, path), 14);
    ap_flat(b, sizeof b, f, sizeof f);
    EXPECT_STR(f, "KeyA=1|keyB=2");        /* original case of keys preserved */
    ap_rm(path);
}

TEST(profile_section_whitespace_and_comments_note) {
    REQUIRE(U(GetPrivateProfileSectionA));
    char path[MAX_PATH]; char b[256], f[256];
    ap_ini(path, "sec9.ini", "[s]\r\n; comment\r\n  a = 1  \r\n\r\nb=\"q\"\r\n;c=3\r\nd = x y \r\n");
    DWORD r = gsec("s", b, sizeof b, path);
    ap_flat(b, sizeof b, f, sizeof f);
    t_note("section with comments/spaces (%lu): %s", r, f);
    CHECK(strstr(f, "b=") != NULL);
    ap_rm(path);
}

TEST(profile_section_duplicate_keys_kept) {
    REQUIRE(U(GetPrivateProfileSectionA));
    char path[MAX_PATH]; ap_ini(path, "sec10.ini", "[s]\r\na=1\r\na=2\r\nb=3\r\n"); char b[64], f[64];
    gsec("s", b, sizeof b, path);
    ap_flat(b, sizeof b, f, sizeof f);
    EXPECT_STR(f, "a=1|a=2|b=3");
    ap_rm(path);
}

TEST(profile_section_w_matches_a) {
    REQUIRE(U(GetPrivateProfileSectionW)); REQUIRE(U(GetPrivateProfileSectionA));
    char path[MAX_PATH]; ap_ini(path, "sec11.ini", SECFILE); WCHAR wp[MAX_PATH], wb[200]; ap_wpath(wp, path);
    char ab[200], f1[200], f2[200];
    DWORD ra = gsec("section1", ab, sizeof ab, path);
    for (int i = 0; i < 200; i++) wb[i] = L'z';
    DWORD rw = U(GetPrivateProfileSectionW)(L"section1", wb, 200, wp);
    EXPECT_EQ(ra, rw);
    ap_flat(ab, 200, f1, 200); ap_flatw(wb, 200, f2, 200);
    EXPECT_STR(f1, f2);
    EXPECT_EQ(wb[rw], 0); EXPECT_EQ(wb[rw - 1], 0);
    /* wide truncation */
    for (int i = 0; i < 200; i++) wb[i] = L'z';
    EXPECT_EQ(U(GetPrivateProfileSectionW)(L"section1", wb, 24, wp), 22);
    EXPECT_EQ(wb[22], 0); if (!t_is_wine()) EXPECT_EQ(wb[23], 0);
    EXPECT_EQ(wb[24], L'z');
    ap_rm(path);
}

TEST(profile_section_names_basic) {
    REQUIRE(U(GetPrivateProfileSectionNamesA));
    char path[MAX_PATH]; ap_ini(path, "names.ini", "[ section1 ]\r\n[section2]\r\n[section3]\r\n"); char b[100], f[100];
    memset(b, 0x0c, sizeof b);
    EXPECT_EQ(gnames(b, 29, path), 27);
    EXPECT_EQ(b[26], 0); EXPECT_EQ(b[27], 0);
    ap_flat(b, 29, f, sizeof f);
    EXPECT_STR(f, "section1|section2|section3");
    ap_rm(path);
}

static DWORD names_cb(char *b, DWORD n) { return gnames(b, n, g_sec_path); }
TEST(profile_section_names_truncation_model) {
    REQUIRE(U(GetPrivateProfileSectionNamesA));
    ap_ini(g_sec_path, "names2.ini", "[ section1 ]\r\n[section2]\r\n[section3]\r\n");
    const char data[] = "section1\0section2\0section3\0";
    size_t T = sizeof data - 1;
    EXPECT_EQ(T, 27);
    for (DWORD n = 0; n <= T + 3; n++) {
        char b[100]; memset(b, 0x0c, sizeof b);
        DWORD r = names_cb(b, n);
        DWORD wantr = n >= T + 2 ? (DWORD)T : (n >= 2 ? n - 2 : 0);
        if (n == 1) wantr = 0;
        CHECKF(r == wantr, "nSize %lu: ret %lu want %lu", n, r, wantr);
        if (n == 0) { CHECK(b[0] == 0x0c); continue; }
        if (!t_is_wine() || n > 2) CHECKF(!memcmp(b, data, wantr), "nSize %lu prefix", n);
        CHECKF(b[wantr] == 0, "nSize %lu term", n);
        if (!t_is_wine() && n >= 2 && wantr < T) CHECKF(b[wantr + 1] == 0, "nSize %lu double NUL", n);
    }
    ap_rm(g_sec_path);
}

TEST(profile_section_names_missing_file_and_empty) {
    REQUIRE(U(GetPrivateProfileSectionNamesA));
    char path[MAX_PATH]; t_tmpfile(path, sizeof path, "names_none.ini"); ap_rm(path); char b[16];
    memset(b, 0x0c, sizeof b);
    EXPECT_EQ(gnames(b, 10, path), 0);
    EXPECT_EQ(b[0], 0);
    EXPECT_NE(b[1], 0);                    /* only a single NUL is written */
    ap_ini(path, "names_empty.ini", "");
    ap_put(path, "", 0);
    memset(b, 0x0c, sizeof b);
    EXPECT_EQ(gnames(b, 10, path), 0);
    EXPECT_EQ(b[0], 0);
    ap_ini(path, "names_nosec.ini", "a=1\r\nb=2\r\n");
    memset(b, 0x0c, sizeof b);
    EXPECT_EQ(gnames(b, 10, path), 0);
    EXPECT_EQ(b[0], 0);
    ap_rm(path);
}

TEST(profile_section_names_order_and_dups) {
    REQUIRE(U(GetPrivateProfileSectionNamesA));
    char path[MAX_PATH]; ap_ini(path, "names3.ini", "[zeta]\r\na=1\r\n[Alpha]\r\n[mid]\r\n[ALPHA]\r\n"); char b[100], f[100];
    gnames(b, sizeof b, path);
    ap_flat(b, sizeof b, f, sizeof f);
    t_note("section name order: %s", f);
    CHECK(!strncmp(f, "zeta|Alpha|mid", 14));
    ap_rm(path);
}

TEST(profile_section_names_w) {
    REQUIRE(U(GetPrivateProfileSectionNamesW));
    char path[MAX_PATH]; ap_ini(path, "namesw.ini", "[ section1 ]\r\n[section2]\r\n[section3]\r\n"); WCHAR wp[MAX_PATH], b[100]; ap_wpath(wp, path);
    for (int i = 0; i < 100; i++) b[i] = 0xcccc;
    EXPECT_EQ(U(GetPrivateProfileSectionNamesW)(b, 29, wp), 27);
    EXPECT_EQ(b[26], 0); EXPECT_EQ(b[27], 0);
    for (int i = 0; i < 100; i++) b[i] = 0xcccc;
    EXPECT_EQ(U(GetPrivateProfileSectionNamesW)(b, 28, wp), 26);
    EXPECT_EQ(b[26], 0);
    for (int i = 0; i < 100; i++) b[i] = 0xcccc;
    EXPECT_EQ(U(GetPrivateProfileSectionNamesW)(b, 27, wp), 25);
    EXPECT_EQ(b[25], 0); if (!t_is_wine()) EXPECT_EQ(b[26], 0);
    char gone[MAX_PATH]; t_tmpfile(gone, sizeof gone, "namesw_gone.ini"); ap_rm(gone); WCHAR wg[MAX_PATH]; ap_wpath(wg, gone);
    for (int i = 0; i < 100; i++) b[i] = 0xcccc;
    EXPECT_EQ(U(GetPrivateProfileSectionNamesW)(b, 10, wg), 0);
    EXPECT_EQ(b[0], 0); EXPECT_NE(b[1], 0);
    ap_rm(path);
}

TEST(profile_section_names_null_file_is_winini) {
    REQUIRE(U(GetPrivateProfileSectionNamesA));
    char b[8192]; memset(b, 0x0c, sizeof b);
    DWORD r = U(GetPrivateProfileSectionNamesA)(b, sizeof b, NULL);
    t_note("section names of win.ini: %lu chars", r);
    CHECK(r < sizeof b);
    CHECK(b[r] == 0);
}

TEST(profile_section_names_many_sections) {
    REQUIRE(U(GetPrivateProfileSectionNamesA));
    static char c[60000]; int n = 0;
    for (int i = 0; i < 2000; i++) n += snprintf(c + n, 40, "[sec%04d]\r\nk=%d\r\n", i, i);
    char path[MAX_PATH]; ap_ini(path, "names_many.ini", NULL); ap_put(path, c, n);
    static char b[40000];
    DWORD r = gnames(b, sizeof b, path);
    EXPECT_EQ(r, 2000 * 8);
    EXPECT_EQ(b[r], 0);
    EXPECT_STR(b, "sec0000");
    EXPECT_STR(b + 1999 * 8, "sec1999");
    /* small buffer: truncated */
    r = gnames(b, 100, path);
    EXPECT_EQ(r, 98);
    ap_rm(path);
}

TEST(profile_section_get_large_section) {
    REQUIRE(U(GetPrivateProfileSectionA));
    static char c[60000]; int n = snprintf(c, sizeof c, "[big]\r\n");
    for (int i = 0; i < 3000; i++) n += snprintf(c + n, 40, "key%04d=%d\r\n", i, i);
    char path[MAX_PATH]; ap_ini(path, "sec_large.ini", NULL); ap_put(path, c, n);
    static char b[60000];
    DWORD r = gsec("big", b, sizeof b, path);
    DWORD want = 0; for (int i = 0; i < 3000; i++) want += (DWORD)snprintf(NULL, 0, "key%04d=%d", i, i) + 1;
    EXPECT_EQ(r, want);
    EXPECT_STR(b, "key0000=0");
    ap_rm(path);
}
