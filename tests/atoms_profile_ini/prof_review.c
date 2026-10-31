/* Additional edge cases added during review. */
#include "ap.h"


TEST(profile_review_section_roundtrip_order) {
    REQUIRE(U(WritePrivateProfileSectionA)); REQUIRE(U(GetPrivateProfileSectionA));
    char path[MAX_PATH]; ap_ini(path, "rv_sec.ini", NULL);
    CHECK(U(WritePrivateProfileSectionA)("S", "a=1\0b=2\0c=3\0", path));
    char b[64]; memset(b, 'z', sizeof b);
    DWORD r = U(GetPrivateProfileSectionA)("s", b, sizeof b, path);
    EXPECT_EQ(r, 12);
    char flat[64]; ap_flat(b, r + 1, flat, sizeof flat);
    EXPECT_STR(flat, "a=1|b=2|c=3");
    CHECK(b[12] == 0 && b[11] == 0);
    ap_rm(path);
}

TEST(profile_review_string_nsize_zero) {
    REQUIRE(U(GetPrivateProfileStringA)); REQUIRE(U(GetPrivateProfileStringW));
    char path[MAX_PATH]; ap_ini(path, "rv_n0.ini", "[s]\r\nk=value\r\n"); WCHAR wp[MAX_PATH]; ap_wpath(wp, path);
    char b[4] = {'q','q','q','q'}; WCHAR w[4] = {L'q',L'q',L'q',L'q'};
    EXPECT_EQ(U(GetPrivateProfileStringA)("s", "k", "d", b, 0, path), 0);
    CHECK(b[0] == 'q');
    EXPECT_EQ(U(GetPrivateProfileStringW)(L"s", L"k", L"d", w, 0, wp), 0);
    CHECK(w[0] == L'q');
    ap_rm(path);
}

TEST(profile_review_string_truncation_sizes) {
    REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH]; ap_ini(path, "rv_tr.ini", "[s]\r\nk=value\r\n");
    char b[16];
    for (int n = 2; n <= 5; n++) {
        memset(b, 'z', sizeof b);
        DWORD r = U(GetPrivateProfileStringA)("s", "k", "d", b, n, path);
        CHECKF(r == (DWORD)(n - 1), "nSize %d returned %lu", n, r);
        CHECKF(b[n - 1] == 0 && !strncmp(b, "value", n - 1), "nSize %d content [%s]", n, b);
    }
    memset(b, 'z', sizeof b);
    EXPECT_EQ(U(GetPrivateProfileStringA)("s", "k", "d", b, 6, path), 5);
    EXPECT_STR(b, "value");
    ap_rm(path);
}

TEST(profile_review_w_key_case_and_whitespace) {
    REQUIRE(U(GetPrivateProfileStringW));
    char path[MAX_PATH]; ap_ini(path, "rv_cw.ini", "[Sect]\r\n  Key Name  =  v 1  \r\n"); WCHAR wp[MAX_PATH]; ap_wpath(wp, path);
    WCHAR w[32];
    DWORD r = U(GetPrivateProfileStringW)(L"SECT", L"key name", L"d", w, 32, wp);
    EXPECT_EQ(r, 3);
    CHECK(!wcscmp(w, L"v 1"));
    ap_rm(path);
}

TEST(profile_review_int_missing_file_default) {
    REQUIRE(U(GetPrivateProfileIntW));
    char path[MAX_PATH]; ap_ini(path, "rv_nf.ini", NULL); WCHAR wp[MAX_PATH]; ap_wpath(wp, path);
    EXPECT_EQ(U(GetPrivateProfileIntW)(L"s", L"k", 321, wp), 321);
    CHECK(!ap_exists(path));
}

TEST(profile_review_read_does_not_create_file) {
    REQUIRE(U(GetPrivateProfileStringA)); REQUIRE(U(GetPrivateProfileSectionNamesA));
    char path[MAX_PATH]; ap_ini(path, "rv_nc.ini", NULL);
    char b[16];
    U(GetPrivateProfileStringA)("s", "k", "d", b, sizeof b, path);
    U(GetPrivateProfileSectionNamesA)(b, sizeof b, path);
    CHECK(!ap_exists(path));
}

TEST(profile_review_delete_section_then_names) {
    REQUIRE(U(WritePrivateProfileStringA)); REQUIRE(U(GetPrivateProfileSectionNamesA));
    char path[MAX_PATH]; ap_ini(path, "rv_dn.ini", NULL);
    CHECK(ap_wps("one", "k", "v", path)); CHECK(ap_wps("two", "k", "v", path));
    CHECK(ap_wps("one", NULL, NULL, path));
    char b[32]; DWORD r = U(GetPrivateProfileSectionNamesA)(b, sizeof b, path);
    EXPECT_EQ(r, 4);   /* "two" + NUL; the count excludes only the final terminator */
    EXPECT_STR(b, "two");
    ap_rm(path);
}

TEST(profile_review_write_w_file_is_ansi_ascii) {
    REQUIRE(U(WritePrivateProfileStringW));
    char path[MAX_PATH]; ap_ini(path, "rv_wa.ini", NULL); WCHAR wp[MAX_PATH]; ap_wpath(wp, path);
    CHECK(U(WritePrivateProfileStringW)(L"s", L"k", L"abc", wp));
    char b[64]; long n = ap_get(path, b, sizeof b);
    EXPECT_EQ(n, 12);
    CHECK(!memcmp(b, "[s]\r\nk=abc\r\n", n));
    ap_rm(path);
}
