/* GetPrivateProfileIntA/W */
#include "ap.h"

typedef struct { const char *val; int def; unsigned want; int hard; } int_case;
static const int_case ic[] = {
    { "0", 99, 0, 1 }, { "1", 99, 1, 1 }, { "42", 99, 42, 1 }, { "007", 99, 7, 1 },
    { "-1", 1, 0xFFFFFFFFu, 1 }, { "-1", -1, 0xFFFFFFFFu, 1 }, { "-0", 5, 0, 1 }, { "-12345", 0, (unsigned)-12345, 1 },
    { "+1", -1, 1, 1 }, { "+42", 0, 42, 1 },
    { "2147483647", 0, 2147483647u, 1 }, { "2147483648", 0, 2147483648u, 1 }, { "-2147483648", 0, 0x80000000u, 1 },
    { "4294967295", 0, 4294967295u, 1 }, { "4294967296", -1, 0, 1 }, { "4294967296", 1, 0, 1 },
    { "4294967297", -1, 1, 1 }, { "4294967297", 1, 1, 1 }, { "-4294967297", -1, 0xFFFFFFFFu, 1 }, { "-4294967297", 1, 0xFFFFFFFFu, 1 },
    { "42A94967297", -1, 42, 1 }, { "42A94967297", 1, 42, 1 }, { "B4294967297", -1, 0, 1 }, { "B4294967297", 1, 0, 1 },
    { "12abc", 5, 12, 1 }, { "abc", 5, 0, 1 }, { "abc", -1, 0, 1 }, { "1.9", 5, 1, 1 }, { "3,4", 5, 3, 1 },
    { " 12", 5, 12, 1 }, { "12 ", 5, 12, 1 }, { "   -7   ", 5, (unsigned)-7, 1 }, { "1 2", 5, 1, 1 },
    { "\"55\"", 5, 55, 1 }, { "\" 55 \"", 5, 55, 0 },
    { "- 5", 3, 0, 0 }, { "--5", 3, 0, 0 }, { "+-5", 3, 0, 0 }, { ".5", 3, 0, 1 }, { "e5", 3, 0, 1 },
    { "0x10", 3, 0, 0 }, { "0X1f", 3, 0, 0 }, { "0b11", 3, 0, 0 }, { "0o17", 3, 0, 0 }, { "010", 3, 10, 0 }, { "1e3", 3, 1, 1 },
    { "99999999999999999999", 0, 0, 0 },
};

TEST(profile_int_value_table_a) {
    REQUIRE(U(GetPrivateProfileIntA)); REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH];
    for (unsigned i = 0; i < sizeof ic / sizeof *ic; i++) {
        char c[128]; snprintf(c, sizeof c, "[s]\r\nk=%s\r\n", ic[i].val);
        ap_ini(path, "int_a.ini", c);
        UINT r = U(GetPrivateProfileIntA)("s", "k", ic[i].def, path);
        if (ic[i].hard) CHECKF(r == ic[i].want, "val \"%s\" def %d: got %u want %u", ic[i].val, ic[i].def, r, ic[i].want);
        else if (r != ic[i].want) t_note("(soft) val \"%s\" def %d: got %u (expected %u)", ic[i].val, ic[i].def, r, ic[i].want);
    }
    ap_rm(path);
}

TEST(profile_int_value_table_w) {
    REQUIRE(U(GetPrivateProfileIntW));
    char path[MAX_PATH]; WCHAR wp[MAX_PATH];
    for (unsigned i = 0; i < sizeof ic / sizeof *ic; i++) {
        char c[128]; snprintf(c, sizeof c, "[s]\r\nk=%s\r\n", ic[i].val);
        ap_ini(path, "int_w.ini", c); ap_wpath(wp, path);
        UINT r = U(GetPrivateProfileIntW)(L"s", L"k", ic[i].def, wp);
        if (ic[i].hard) CHECKF(r == ic[i].want, "val \"%s\" def %d: got %u want %u", ic[i].val, ic[i].def, r, ic[i].want);
    }
    ap_rm(path);
}

TEST(profile_int_a_w_agree) {
    REQUIRE(U(GetPrivateProfileIntA)); REQUIRE(U(GetPrivateProfileIntW));
    char path[MAX_PATH]; WCHAR wp[MAX_PATH];
    for (unsigned i = 0; i < sizeof ic / sizeof *ic; i++) {
        char c[128]; snprintf(c, sizeof c, "[s]\r\nk=%s\r\n", ic[i].val);
        ap_ini(path, "int_aw.ini", c); ap_wpath(wp, path);
        UINT a = U(GetPrivateProfileIntA)("s", "k", 7, path), w = U(GetPrivateProfileIntW)(L"s", L"k", 7, wp);
        CHECKF(a == w, "val \"%s\": A=%u W=%u", ic[i].val, a, w);
    }
    ap_rm(path);
}

TEST(profile_int_missing_returns_default) {
    REQUIRE(U(GetPrivateProfileIntA)); REQUIRE(U(GetPrivateProfileIntW));
    char path[MAX_PATH]; ap_ini(path, "int_miss.ini", "[s]\r\nk=5\r\n"); WCHAR wp[MAX_PATH]; ap_wpath(wp, path);
    EXPECT_EQ(U(GetPrivateProfileIntA)("s", "nokey", 70, path), 70);
    EXPECT_EQ(U(GetPrivateProfileIntA)("nosec", "k", 71, path), 71);
    EXPECT_EQ(U(GetPrivateProfileIntA)("s", "nokey", -1, path), 0xFFFFFFFFu);
    EXPECT_EQ(U(GetPrivateProfileIntW)(L"s", L"nokey", -2, wp), 0xFFFFFFFEu);
    EXPECT_EQ(U(GetPrivateProfileIntW)(L"s", L"nokey", 0x7FFFFFFF, wp), 0x7FFFFFFFu);
    EXPECT_EQ(U(GetPrivateProfileIntW)(L"s", L"nokey", (int)0x80000000, wp), 0x80000000u);
    char gone[MAX_PATH]; t_tmpfile(gone, sizeof gone, "int_gone.ini"); ap_rm(gone);
    EXPECT_EQ(U(GetPrivateProfileIntA)("s", "k", 33, gone), 33);
    ap_rm(path);
}

TEST(profile_int_empty_value_returns_default) {
    REQUIRE(U(GetPrivateProfileIntA));
    char path[MAX_PATH]; ap_ini(path, "int_empty.ini", "[s]\r\nk=\r\nq=\"\"\r\nsp=   \r\n");
    EXPECT_EQ(U(GetPrivateProfileIntA)("s", "k", 17, path), 17);
    EXPECT_EQ(U(GetPrivateProfileIntA)("s", "sp", 18, path), 18);
    t_note("empty quoted value with default 19 -> %u", U(GetPrivateProfileIntA)("s", "q", 19, path));
    ap_rm(path);
}

TEST(profile_int_key_without_equals_is_default) {
    REQUIRE(U(GetPrivateProfileIntA));
    char path[MAX_PATH]; ap_ini(path, "int_noeq.ini", "[s]\r\nk\r\nj=3\r\n");
    EXPECT_EQ(U(GetPrivateProfileIntA)("s", "k", 17, path), 17);
    EXPECT_EQ(U(GetPrivateProfileIntA)("s", "j", 17, path), 3);
    ap_rm(path);
}

TEST(profile_int_case_insensitive_names) {
    REQUIRE(U(GetPrivateProfileIntA));
    char path[MAX_PATH]; ap_ini(path, "int_case.ini", "[Sec]\r\nKey=12\r\n");
    EXPECT_EQ(U(GetPrivateProfileIntA)("sEC", "kEY", 0, path), 12);
    ap_rm(path);
}

TEST(profile_int_null_names_with_default) {
    REQUIRE(U(GetPrivateProfileIntA));
    char path[MAX_PATH]; t_tmpfile(path, sizeof path, "int_nulls_nofile.ini"); ap_rm(path);
    EXPECT_EQ(U(GetPrivateProfileIntA)(NULL, NULL, 70, path), 70);
    EXPECT_EQ(U(GetPrivateProfileIntA)(NULL, "k", -1, path), 0xFFFFFFFFu);
    EXPECT_EQ(U(GetPrivateProfileIntA)("s", NULL, 1, path), 1);
    ap_ini(path, "int_nulls.ini", "[s]\r\nk=9\r\n");
    t_note("int(NULL,\"k\") with file -> %u", U(GetPrivateProfileIntA)(NULL, "k", 5, path));
    t_note("int(\"s\",NULL) with file -> %u", U(GetPrivateProfileIntA)("s", NULL, 5, path));
    ap_rm(path);
}

TEST(profile_int_value_longer_than_255_is_truncated) {
    REQUIRE(U(GetPrivateProfileIntA));
    char path[MAX_PATH]; static char c[1024]; int n = snprintf(c, sizeof c, "[s]\r\nk=");
    n += snprintf(c + n, 16, "12");
    for (int i = 0; i < 400; i++) c[n++] = ' ';
    n += snprintf(c + n, 16, "99\r\n");
    ap_ini(path, "int_long.ini", NULL); ap_put(path, c, n);
    EXPECT_EQ(U(GetPrivateProfileIntA)("s", "k", 0, path), 12);
    ap_rm(path);
}

TEST(profile_int_roundtrip_via_write) {
    REQUIRE(U(GetPrivateProfileIntA)); REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; t_tmpfile(path, sizeof path, "int_rt.ini"); ap_rm(path);
    int vals[] = { 0, 1, -1, 12345, -12345, 2147483647, (int)0x80000000 };
    for (unsigned i = 0; i < 7; i++) {
        char v[16]; snprintf(v, sizeof v, "%d", vals[i]);
        CHECK(ap_wps("s", "k", v, path));
        EXPECT_EQ(U(GetPrivateProfileIntA)("s", "k", 55, path), (unsigned)vals[i]);
    }
    ap_rm(path);
}

TEST(profile_int_readonly_does_not_modify_file) {
    REQUIRE(U(GetPrivateProfileIntA));
    char path[MAX_PATH]; ap_ini(path, "int_ro.ini", "[s]\r\nk=1\r\n");
    char before[128], after[128];
    long n1 = ap_get(path, before, sizeof before);
    U(GetPrivateProfileIntA)("s", "k", 0, path);
    U(GetPrivateProfileIntA)("s", "missing", 0, path);
    long n2 = ap_get(path, after, sizeof after);
    EXPECT_EQ(n1, n2);
    CHECK(!strcmp(before, after));
    ap_rm(path);
}
