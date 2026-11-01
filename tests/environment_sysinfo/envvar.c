/* GetEnvironmentVariable / SetEnvironmentVariable (A and W). */
#include "t.h"
#include "k.h"

#define ENOTFOUND 203
#define SENTINEL_ERR 0x5A5A5A

static void ev_del(const char *n) { U(SetEnvironmentVariableA)(n, NULL); }

static void ev_wfill(WCHAR *w, size_t n, WCHAR c) { for (size_t i = 0; i < n; i++) w[i] = c; w[n] = 0; }
static void ev_afill(char *a, size_t n, char c) { memset(a, c, n); a[n] = 0; }

/* ---------- basic set/get ---------- */
TEST(envvar_w_set_get_basic) {
    REQUIRE(U(SetEnvironmentVariableW)); REQUIRE(U(GetEnvironmentVariableW));
    WCHAR buf[64];
    CHECK(U(SetEnvironmentVariableW)(L"K32T_EV_W1", L"hello"));
    memset(buf, 0xCC, sizeof buf);
    DWORD n = U(GetEnvironmentVariableW)(L"K32T_EV_W1", buf, 64);
    EXPECT_EQ(n, 5);
    EXPECT_WSTR(buf, L"hello");
    U(SetEnvironmentVariableW)(L"K32T_EV_W1", NULL);
}
TEST(envvar_a_set_get_basic) {
    REQUIRE(U(SetEnvironmentVariableA)); REQUIRE(U(GetEnvironmentVariableA));
    char buf[64];
    CHECK(U(SetEnvironmentVariableA)("K32T_EV_A1", "hello"));
    memset(buf, 0xCC, sizeof buf);
    DWORD n = U(GetEnvironmentVariableA)("K32T_EV_A1", buf, 64);
    EXPECT_EQ(n, 5);
    EXPECT_STR(buf, "hello");
    ev_del("K32T_EV_A1");
}
TEST(envvar_set_returns_true_and_overwrites) {
    REQUIRE(U(SetEnvironmentVariableA)); REQUIRE(U(GetEnvironmentVariableA));
    char buf[64];
    CHECK(U(SetEnvironmentVariableA)("K32T_EV_OW", "one"));
    CHECK(U(SetEnvironmentVariableA)("K32T_EV_OW", "second"));
    EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_EV_OW", buf, 64), 6);
    EXPECT_STR(buf, "second");
    CHECK(U(SetEnvironmentVariableA)("K32T_EV_OW", "x"));
    EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_EV_OW", buf, 64), 1);
    EXPECT_STR(buf, "x");
    ev_del("K32T_EV_OW");
}
TEST(envvar_w_overwrite_longer_then_shorter) {
    REQUIRE(U(SetEnvironmentVariableW)); REQUIRE(U(GetEnvironmentVariableW));
    WCHAR big[600], buf[700];
    ev_wfill(big, 500, L'q');
    CHECK(U(SetEnvironmentVariableW)(L"K32T_EV_LS", big));
    EXPECT_EQ(U(GetEnvironmentVariableW)(L"K32T_EV_LS", buf, 700), 500);
    CHECK(U(SetEnvironmentVariableW)(L"K32T_EV_LS", L"s"));
    EXPECT_EQ(U(GetEnvironmentVariableW)(L"K32T_EV_LS", buf, 700), 1);
    EXPECT_WSTR(buf, L"s");
    U(SetEnvironmentVariableW)(L"K32T_EV_LS", NULL);
}
TEST(envvar_repeated_get_is_stable) {
    REQUIRE(U(SetEnvironmentVariableA)); REQUIRE(U(GetEnvironmentVariableA));
    char b1[32], b2[32];
    U(SetEnvironmentVariableA)("K32T_EV_REP", "stable");
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_EV_REP", b1, 32), 6);
        EXPECT_STR(b1, "stable");
    }
    ev_del("K32T_EV_REP");
}
TEST(envvar_setget_many_vars) {
    REQUIRE(U(SetEnvironmentVariableA)); REQUIRE(U(GetEnvironmentVariableA));
    char name[40], val[40], buf[40];
    for (int i = 0; i < 200; i++) {
        snprintf(name, sizeof name, "K32T_EV_MANY_%03d", i);
        snprintf(val, sizeof val, "value-%d", i * 7);
        CHECK(U(SetEnvironmentVariableA)(name, val));
    }
    for (int i = 0; i < 200; i++) {
        snprintf(name, sizeof name, "K32T_EV_MANY_%03d", i);
        snprintf(val, sizeof val, "value-%d", i * 7);
        DWORD n = U(GetEnvironmentVariableA)(name, buf, sizeof buf);
        EXPECT_EQ(n, strlen(val));
        EXPECT_STR(buf, val);
    }
    for (int i = 0; i < 200; i++) { snprintf(name, sizeof name, "K32T_EV_MANY_%03d", i); CHECK(U(SetEnvironmentVariableA)(name, NULL)); }
    for (int i = 0; i < 200; i += 17) {
        snprintf(name, sizeof name, "K32T_EV_MANY_%03d", i);
        SET_ERR(0);
        EXPECT_EQ(U(GetEnvironmentVariableA)(name, buf, sizeof buf), 0);
        EXPECT_ERR(ENOTFOUND);
    }
}

/* ---------- not found ---------- */
TEST(envvar_w_get_missing) {
    REQUIRE(U(GetEnvironmentVariableW));
    WCHAR buf[8] = L"abc";
    SET_ERR(SENTINEL_ERR);
    EXPECT_EQ(U(GetEnvironmentVariableW)(L"K32T_EV_DOES_NOT_EXIST", buf, 8), 0);
    EXPECT_ERR(ENOTFOUND);
}
TEST(envvar_a_get_missing) {
    REQUIRE(U(GetEnvironmentVariableA));
    char buf[8] = "abc";
    SET_ERR(SENTINEL_ERR);
    EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_EV_DOES_NOT_EXIST", buf, 8), 0);
    EXPECT_ERR(ENOTFOUND);
}
TEST(envvar_get_missing_null_buffer) {
    REQUIRE(U(GetEnvironmentVariableW)); REQUIRE(U(GetEnvironmentVariableA));
    SET_ERR(SENTINEL_ERR);
    EXPECT_EQ(U(GetEnvironmentVariableW)(L"K32T_EV_DOES_NOT_EXIST", NULL, 0), 0);
    EXPECT_ERR(ENOTFOUND);
    SET_ERR(SENTINEL_ERR);
    EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_EV_DOES_NOT_EXIST", NULL, 0), 0);
    EXPECT_ERR(ENOTFOUND);
}
TEST(envvar_w_get_empty_name) {
    REQUIRE(U(GetEnvironmentVariableW));
    WCHAR buf[8];
    SET_ERR(SENTINEL_ERR);
    EXPECT_EQ(U(GetEnvironmentVariableW)(L"", buf, 8), 0);
    EXPECT_ERR(ENOTFOUND);
}
TEST(envvar_a_get_empty_name) {
    REQUIRE(U(GetEnvironmentVariableA));
    char buf[8];
    SET_ERR(SENTINEL_ERR);
    EXPECT_EQ(U(GetEnvironmentVariableA)("", buf, 8), 0);
    DWORD e = GetLastError();
    CHECKF(e == ENOTFOUND, "last error %lu", e);
}
TEST(envvar_w_get_null_name) {
    REQUIRE(U(GetEnvironmentVariableW));
    WCHAR buf[8];
    SET_ERR(SENTINEL_ERR);
    EXPECT_EQ(U(GetEnvironmentVariableW)(NULL, buf, 8), 0);
    DWORD e = GetLastError();
    CHECKF(e == ENOTFOUND || e == ERROR_INVALID_PARAMETER, "last error %lu", e);
}
TEST(envvar_get_prefix_of_existing_not_matched) {
    REQUIRE(U(SetEnvironmentVariableA)); REQUIRE(U(GetEnvironmentVariableA));
    char buf[32];
    U(SetEnvironmentVariableA)("K32T_EV_PREFIXLONG", "v");
    SET_ERR(0);
    EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_EV_PREFIX", buf, 32), 0);
    EXPECT_ERR(ENOTFOUND);
    SET_ERR(0);
    EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_EV_PREFIXLONGER", buf, 32), 0);
    EXPECT_ERR(ENOTFOUND);
    SET_ERR(0);
    EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_EV_PREFIXLON", buf, 32), 0);
    EXPECT_ERR(ENOTFOUND);
    EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_EV_PREFIXLONG", buf, 32), 1);
    ev_del("K32T_EV_PREFIXLONG");
}
TEST(envvar_get_name_with_trailing_space_distinct) {
    REQUIRE(U(SetEnvironmentVariableA)); REQUIRE(U(GetEnvironmentVariableA));
    char buf[32];
    U(SetEnvironmentVariableA)("K32T_EV_SP", "a");
    SET_ERR(0);
    EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_EV_SP ", buf, 32), 0);
    EXPECT_ERR(ENOTFOUND);
    EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_EV_SP", buf, 32), 1);
    ev_del("K32T_EV_SP");
}

/* ---------- buffer size semantics ---------- */
TEST(envvar_w_buffer_boundaries) {
    REQUIRE(U(SetEnvironmentVariableW)); REQUIRE(U(GetEnvironmentVariableW));
    static const int lens[] = { 1, 2, 3, 7, 8, 15, 16, 31, 100, 255, 256, 1000 };
    WCHAR val[1100], buf[1200];
    for (unsigned i = 0; i < sizeof lens / sizeof lens[0]; i++) {
        int L = lens[i];
        ev_wfill(val, L, L'a' + (L % 20));
        CHECK(U(SetEnvironmentVariableW)(L"K32T_EV_BB", val));
        /* nSize == 0 : required size incl NUL */
        EXPECT_EQ(U(GetEnvironmentVariableW)(L"K32T_EV_BB", buf, 0), L + 1);
        EXPECT_EQ(U(GetEnvironmentVariableW)(L"K32T_EV_BB", NULL, 0), L + 1);
        /* nSize == L : too small by one */
        for (int j = 0; j < 1200; j++) buf[j] = 0x7777;
        EXPECT_EQ(U(GetEnvironmentVariableW)(L"K32T_EV_BB", buf, L), L + 1);
        /* nSize == L+1 : exact fit, returns L */
        for (int j = 0; j < 1200; j++) buf[j] = 0x7777;
        EXPECT_EQ(U(GetEnvironmentVariableW)(L"K32T_EV_BB", buf, L + 1), L);
        EXPECT_WSTR(buf, val);
        EXPECT_EQ(buf[L + 1], 0x7777);   /* nothing written past the NUL */
        /* nSize larger */
        EXPECT_EQ(U(GetEnvironmentVariableW)(L"K32T_EV_BB", buf, 1200), L);
    }
    U(SetEnvironmentVariableW)(L"K32T_EV_BB", NULL);
}
TEST(envvar_a_buffer_boundaries) {
    REQUIRE(U(SetEnvironmentVariableA)); REQUIRE(U(GetEnvironmentVariableA));
    static const int lens[] = { 1, 2, 3, 7, 8, 15, 16, 31, 100, 255, 256, 1000 };
    char val[1100], buf[1200];
    for (unsigned i = 0; i < sizeof lens / sizeof lens[0]; i++) {
        int L = lens[i];
        ev_afill(val, L, 'a' + (L % 20));
        CHECK(U(SetEnvironmentVariableA)("K32T_EV_BBA", val));
        EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_EV_BBA", buf, 0), L + 1);
        EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_EV_BBA", NULL, 0), L + 1);
        EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_EV_BBA", buf, L), L + 1);
        memset(buf, 0x77, sizeof buf);
        EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_EV_BBA", buf, L + 1), L);
        EXPECT_STR(buf, val);
        EXPECT_EQ((unsigned char)buf[L + 1], 0x77);
        EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_EV_BBA", buf, 1200), L);
    }
    ev_del("K32T_EV_BBA");
}
TEST(envvar_w_truncated_get_first_char_nul) {
    /* when the buffer is too small and non-empty, the first character is zeroed */
    REQUIRE(U(SetEnvironmentVariableW)); REQUIRE(U(GetEnvironmentVariableW));
    WCHAR buf[8];
    U(SetEnvironmentVariableW)(L"K32T_EV_TR", L"abcdefghijk");
    for (int i = 0; i < 8; i++) buf[i] = 0x4141;
    EXPECT_EQ(U(GetEnvironmentVariableW)(L"K32T_EV_TR", buf, 5), 12);
    EXPECT_EQ(buf[0], 0);
    EXPECT_EQ(buf[5], 0x4141);
    U(SetEnvironmentVariableW)(L"K32T_EV_TR", NULL);
}
TEST(envvar_w_size_one_buffer) {
    REQUIRE(U(SetEnvironmentVariableW)); REQUIRE(U(GetEnvironmentVariableW));
    WCHAR buf[2] = { 0x4141, 0x4141 };
    U(SetEnvironmentVariableW)(L"K32T_EV_S1", L"x");
    EXPECT_EQ(U(GetEnvironmentVariableW)(L"K32T_EV_S1", buf, 1), 2);
    U(SetEnvironmentVariableW)(L"K32T_EV_S1", L"");
    buf[0] = 0x4141;
    EXPECT_EQ(U(GetEnvironmentVariableW)(L"K32T_EV_S1", buf, 1), 0);
    EXPECT_EQ(buf[0], 0);
    U(SetEnvironmentVariableW)(L"K32T_EV_S1", NULL);
}
TEST(envvar_huge_nsize_with_small_buffer) {
    /* nSize is only an upper bound for the actual write; short values are safe with a lying size */
    REQUIRE(U(SetEnvironmentVariableA)); REQUIRE(U(GetEnvironmentVariableA)); REQUIRE(U(GetEnvironmentVariableW));
    char b[64]; WCHAR w[64];
    U(SetEnvironmentVariableA)("K32T_EV_HUGE", "abc");
    memset(b, 0, sizeof b); memset(w, 0, sizeof w);
    EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_EV_HUGE", b, 0xFFFFFFFF), 3);
    EXPECT_STR(b, "abc");
    EXPECT_EQ(U(GetEnvironmentVariableW)(L"K32T_EV_HUGE", w, 0xFFFFFFFF), 3);
    EXPECT_WSTR(w, L"abc");
    ev_del("K32T_EV_HUGE");
}

/* ---------- empty values ---------- */
TEST(envvar_w_empty_value_exists) {
    REQUIRE(U(SetEnvironmentVariableW)); REQUIRE(U(GetEnvironmentVariableW));
    WCHAR buf[8]; buf[0] = 0x4141;
    CHECK(U(SetEnvironmentVariableW)(L"K32T_EV_EMPTY", L""));
    SET_ERR(SENTINEL_ERR);
    EXPECT_EQ(U(GetEnvironmentVariableW)(L"K32T_EV_EMPTY", buf, 8), 0);
    EXPECT_EQ(buf[0], 0);
    DWORD e = GetLastError();
    if (e == ENOTFOUND) t_note("empty value reported as ENVVAR_NOT_FOUND (unexpected)");
    CHECKF(e != ENOTFOUND, "last error %lu", e);
    /* size query for an empty value */
    EXPECT_EQ(U(GetEnvironmentVariableW)(L"K32T_EV_EMPTY", NULL, 0), 1);
    U(SetEnvironmentVariableW)(L"K32T_EV_EMPTY", NULL);
}
TEST(envvar_a_empty_value_exists) {
    REQUIRE(U(SetEnvironmentVariableA)); REQUIRE(U(GetEnvironmentVariableA));
    char buf[8]; buf[0] = 'x';
    CHECK(U(SetEnvironmentVariableA)("K32T_EV_EMPTYA", ""));
    SET_ERR(SENTINEL_ERR);
    EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_EV_EMPTYA", buf, 8), 0);
    EXPECT_EQ(buf[0], 0);
    DWORD e = GetLastError();
    CHECKF(e != ENOTFOUND, "last error %lu", e);
    ev_del("K32T_EV_EMPTYA");
}
TEST(envvar_empty_value_then_delete) {
    REQUIRE(U(SetEnvironmentVariableA)); REQUIRE(U(GetEnvironmentVariableA));
    char buf[8];
    U(SetEnvironmentVariableA)("K32T_EV_ED", "");
    CHECK(U(SetEnvironmentVariableA)("K32T_EV_ED", NULL));
    SET_ERR(0);
    EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_EV_ED", buf, 8), 0);
    EXPECT_ERR(ENOTFOUND);
}
TEST(envvar_set_value_to_empty_keeps_variable) {
    REQUIRE(U(SetEnvironmentVariableA)); REQUIRE(U(GetEnvironmentVariableA));
    char buf[8];
    U(SetEnvironmentVariableA)("K32T_EV_KEEP", "abc");
    CHECK(U(SetEnvironmentVariableA)("K32T_EV_KEEP", ""));
    SET_ERR(SENTINEL_ERR);
    EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_EV_KEEP", buf, 8), 0);
    CHECK(GetLastError() != ENOTFOUND);
    ev_del("K32T_EV_KEEP");
}

/* ---------- deletion ---------- */
TEST(envvar_delete_existing) {
    REQUIRE(U(SetEnvironmentVariableW)); REQUIRE(U(GetEnvironmentVariableW));
    WCHAR buf[8];
    U(SetEnvironmentVariableW)(L"K32T_EV_DEL", L"1");
    CHECK(U(SetEnvironmentVariableW)(L"K32T_EV_DEL", NULL));
    SET_ERR(0);
    EXPECT_EQ(U(GetEnvironmentVariableW)(L"K32T_EV_DEL", buf, 8), 0);
    EXPECT_ERR(ENOTFOUND);
}
TEST(envvar_delete_missing_w) {
    REQUIRE(U(SetEnvironmentVariableW));
    SET_ERR(SENTINEL_ERR);
    /* RtlSetEnvironmentVar reference analysis shows no not-found status path; result not asserted */
    BOOL r = U(SetEnvironmentVariableW)(L"K32T_EV_NEVER_SET", NULL);
    t_note("delete missing W: ret=%d err=%lu", (int)r, GetLastError());
}
TEST(envvar_delete_missing_a) {
    REQUIRE(U(SetEnvironmentVariableA));
    SET_ERR(SENTINEL_ERR);
    BOOL r = U(SetEnvironmentVariableA)("K32T_EV_NEVER_SET", NULL);
    t_note("delete missing A: ret=%d err=%lu", (int)r, GetLastError());
}
TEST(envvar_delete_twice) {
    REQUIRE(U(SetEnvironmentVariableA));
    U(SetEnvironmentVariableA)("K32T_EV_DEL2", "v");
    CHECK(U(SetEnvironmentVariableA)("K32T_EV_DEL2", NULL));
    SET_ERR(0);
    BOOL r = U(SetEnvironmentVariableA)("K32T_EV_DEL2", NULL);
    t_note("second delete: ret=%d err=%lu", (int)r, GetLastError());
}
TEST(envvar_delete_case_insensitive) {
    REQUIRE(U(SetEnvironmentVariableA)); REQUIRE(U(GetEnvironmentVariableA));
    char buf[8];
    U(SetEnvironmentVariableA)("K32T_EV_DELCASE", "v");
    CHECK(U(SetEnvironmentVariableA)("k32t_ev_delcase", NULL));
    SET_ERR(0);
    EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_EV_DELCASE", buf, 8), 0);
    EXPECT_ERR(ENOTFOUND);
}
TEST(envvar_recreate_after_delete) {
    REQUIRE(U(SetEnvironmentVariableA)); REQUIRE(U(GetEnvironmentVariableA));
    char buf[16];
    for (int i = 0; i < 20; i++) {
        U(SetEnvironmentVariableA)("K32T_EV_CYC", "abc");
        EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_EV_CYC", buf, 16), 3);
        CHECK(U(SetEnvironmentVariableA)("K32T_EV_CYC", NULL));
    }
}

/* ---------- case insensitivity ---------- */
TEST(envvar_name_case_insensitive_ascii) {
    REQUIRE(U(SetEnvironmentVariableA)); REQUIRE(U(GetEnvironmentVariableA));
    char buf[16];
    U(SetEnvironmentVariableA)("K32T_EV_CaSe", "v1");
    EXPECT_EQ(U(GetEnvironmentVariableA)("k32t_ev_case", buf, 16), 2);
    EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_EV_CASE", buf, 16), 2);
    EXPECT_STR(buf, "v1");
    /* setting with a different case replaces, does not create a second variable */
    U(SetEnvironmentVariableA)("k32t_ev_CASE", "v22");
    EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_EV_CaSe", buf, 16), 3);
    EXPECT_STR(buf, "v22");
    ev_del("K32T_EV_CASE");
    SET_ERR(0);
    EXPECT_EQ(U(GetEnvironmentVariableA)("k32t_ev_case", buf, 16), 0);
    EXPECT_ERR(ENOTFOUND);
}
TEST(envvar_value_case_preserved) {
    REQUIRE(U(SetEnvironmentVariableA)); REQUIRE(U(GetEnvironmentVariableA));
    char buf[16];
    U(SetEnvironmentVariableA)("K32T_EV_VC", "MiXeD");
    U(GetEnvironmentVariableA)("k32t_ev_vc", buf, 16);
    EXPECT_STR(buf, "MiXeD");
    ev_del("K32T_EV_VC");
}
TEST(envvar_w_name_case_insensitive_latin1) {
    REQUIRE(U(SetEnvironmentVariableW)); REQUIRE(U(GetEnvironmentVariableW));
    WCHAR buf[16];
    CHECK(U(SetEnvironmentVariableW)(L"K32T_EV_\x00C4\x00D6", L"u"));
    EXPECT_EQ(U(GetEnvironmentVariableW)(L"K32T_EV_\x00E4\x00F6", buf, 16), 1);
    EXPECT_EQ(U(GetEnvironmentVariableW)(L"k32t_ev_\x00C4\x00F6", buf, 16), 1);
    U(SetEnvironmentVariableW)(L"k32t_EV_\x00E4\x00E4", NULL);
    U(SetEnvironmentVariableW)(L"K32T_EV_\x00C4\x00D6", NULL);
}
TEST(envvar_w_name_case_insensitive_greek_cyrillic) {
    REQUIRE(U(SetEnvironmentVariableW)); REQUIRE(U(GetEnvironmentVariableW));
    WCHAR buf[16];
    CHECK(U(SetEnvironmentVariableW)(L"K32T_EV_\x03A3\x0414", L"g"));
    EXPECT_EQ(U(GetEnvironmentVariableW)(L"K32T_EV_\x03C3\x0434", buf, 16), 1);
    U(SetEnvironmentVariableW)(L"K32T_EV_\x03A3\x0414", NULL);
}

/* ---------- invalid names ---------- */
TEST(envvar_set_name_with_equals_w) {
    REQUIRE(U(SetEnvironmentVariableW));
    static const WCHAR *names[] = { L"K32T_EV=X", L"K32T=", L"A=B=C", L"K32T_EV_X=" };
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++) {
        SET_ERR(0);
        EXPECT_EQ(U(SetEnvironmentVariableW)(names[i], L"v"), 0);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(envvar_set_name_with_equals_a) {
    REQUIRE(U(SetEnvironmentVariableA));
    static const char *names[] = { "K32T_EV=X", "K32T=", "A=B=C", "K32T_EV_X=" };
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++) {
        SET_ERR(0);
        EXPECT_EQ(U(SetEnvironmentVariableA)(names[i], "v"), 0);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(envvar_set_name_with_equals_delete) {
    REQUIRE(U(SetEnvironmentVariableA));
    SET_ERR(0);
    EXPECT_EQ(U(SetEnvironmentVariableA)("K32T=EQ", NULL), 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(envvar_set_empty_name) {
    REQUIRE(U(SetEnvironmentVariableA)); REQUIRE(U(SetEnvironmentVariableW));
    SET_ERR(0);
    EXPECT_EQ(U(SetEnvironmentVariableA)("", "v"), 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    EXPECT_EQ(U(SetEnvironmentVariableW)(L"", L"v"), 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    EXPECT_EQ(U(SetEnvironmentVariableW)(L"", NULL), 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(envvar_leading_equals_name_allowed) {
    REQUIRE(U(SetEnvironmentVariableW)); REQUIRE(U(GetEnvironmentVariableW));
    WCHAR buf[16];
    CHECK(U(SetEnvironmentVariableW)(L"=K32T_EV_HIDDEN", L"hid"));
    EXPECT_EQ(U(GetEnvironmentVariableW)(L"=K32T_EV_HIDDEN", buf, 16), 3);
    EXPECT_WSTR(buf, L"hid");
    CHECK(U(SetEnvironmentVariableW)(L"=K32T_EV_HIDDEN", NULL));
    SET_ERR(0);
    EXPECT_EQ(U(GetEnvironmentVariableW)(L"=K32T_EV_HIDDEN", buf, 16), 0);
    EXPECT_ERR(ENOTFOUND);
}
TEST(envvar_leading_equals_with_second_equals_rejected) {
    REQUIRE(U(SetEnvironmentVariableW));
    SET_ERR(0);
    EXPECT_EQ(U(SetEnvironmentVariableW)(L"=K32T=EV", L"x"), 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(envvar_single_equals_name) {
    REQUIRE(U(SetEnvironmentVariableW)); REQUIRE(U(GetEnvironmentVariableW));
    WCHAR buf[8];
    /* "=" alone: first char may be '=' (index 0 is not scanned) */
    BOOL r = U(SetEnvironmentVariableW)(L"=", L"x");
    t_note("SetEnvironmentVariableW(\"=\", \"x\") -> %d (err %lu)", r, GetLastError());
    if (r) { U(GetEnvironmentVariableW)(L"=", buf, 8); U(SetEnvironmentVariableW)(L"=", NULL); }
}
TEST(envvar_name_with_dollar_space_paren) {
    REQUIRE(U(SetEnvironmentVariableA)); REQUIRE(U(GetEnvironmentVariableA));
    static const char *names[] = { "K32T EV SP", "K32T_EV(x86)", "K32T_EV$", "K32T_EV;1", "K32T_EV%P", "K32T_EV.dot", "K32T-EV-dash", "K32T_EV{b}" };
    char buf[16];
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++) {
        CHECKF(U(SetEnvironmentVariableA)(names[i], "val"), "name %s", names[i]);
        EXPECT_EQ(U(GetEnvironmentVariableA)(names[i], buf, 16), 3);
        EXPECT_STR(buf, "val");
        CHECK(U(SetEnvironmentVariableA)(names[i], NULL));
    }
}
TEST(envvar_long_name_w) {
    REQUIRE(U(SetEnvironmentVariableW)); REQUIRE(U(GetEnvironmentVariableW));
    WCHAR name[1100], buf[8];
    wcscpy(name, L"K32T_EV_LONGNAME_");
    size_t l = wcslen(name);
    for (size_t i = l; i < 1000; i++) name[i] = L'N';
    name[1000] = 0;
    BOOL r = U(SetEnvironmentVariableW)(name, L"v");
    CHECK(r);
    if (r) {
        EXPECT_EQ(U(GetEnvironmentVariableW)(name, buf, 8), 1);
        name[999] = L'M';
        SET_ERR(0);
        EXPECT_EQ(U(GetEnvironmentVariableW)(name, buf, 8), 0);
        EXPECT_ERR(ENOTFOUND);
        name[999] = L'N';
        CHECK(U(SetEnvironmentVariableW)(name, NULL));
    }
}

/* ---------- value contents ---------- */
TEST(envvar_value_with_equals_and_percent) {
    REQUIRE(U(SetEnvironmentVariableA)); REQUIRE(U(GetEnvironmentVariableA));
    char buf[64];
    CHECK(U(SetEnvironmentVariableA)("K32T_EV_VEQ", "a=b=c%PATH%d"));
    EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_EV_VEQ", buf, 64), 12);
    EXPECT_STR(buf, "a=b=c%PATH%d");
    ev_del("K32T_EV_VEQ");
}
TEST(envvar_value_with_whitespace_preserved) {
    REQUIRE(U(SetEnvironmentVariableA)); REQUIRE(U(GetEnvironmentVariableA));
    char buf[64];
    CHECK(U(SetEnvironmentVariableA)("K32T_EV_WS", "  lead and trail \t "));
    EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_EV_WS", buf, 64), 19);
    EXPECT_STR(buf, "  lead and trail \t ");
    ev_del("K32T_EV_WS");
}
TEST(envvar_value_with_newlines) {
    REQUIRE(U(SetEnvironmentVariableA)); REQUIRE(U(GetEnvironmentVariableA));
    char buf[64];
    CHECK(U(SetEnvironmentVariableA)("K32T_EV_NL", "l1\r\nl2\n"));
    EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_EV_NL", buf, 64), 7);
    EXPECT_STR(buf, "l1\r\nl2\n");
    ev_del("K32T_EV_NL");
}
TEST(envvar_value_with_control_chars_w) {
    REQUIRE(U(SetEnvironmentVariableW)); REQUIRE(U(GetEnvironmentVariableW));
    WCHAR buf[16];
    CHECK(U(SetEnvironmentVariableW)(L"K32T_EV_CTL", L"\x01\x02\x1f\x7f"));
    EXPECT_EQ(U(GetEnvironmentVariableW)(L"K32T_EV_CTL", buf, 16), 4);
    EXPECT_EQ(buf[0], 1); EXPECT_EQ(buf[3], 0x7f);
    U(SetEnvironmentVariableW)(L"K32T_EV_CTL", NULL);
}
TEST(envvar_w_unicode_value_roundtrip) {
    REQUIRE(U(SetEnvironmentVariableW)); REQUIRE(U(GetEnvironmentVariableW));
    static const WCHAR v[] = { 0x0416, 0x4E2D, 0x6587, 0xD83D, 0xDE00, 0x00E9, 0 };
    WCHAR buf[16];
    CHECK(U(SetEnvironmentVariableW)(L"K32T_EV_UNI", v));
    EXPECT_EQ(U(GetEnvironmentVariableW)(L"K32T_EV_UNI", buf, 16), 6);
    EXPECT_WSTR(buf, v);
    U(SetEnvironmentVariableW)(L"K32T_EV_UNI", NULL);
}
TEST(envvar_w_value_32766_chars) {
    REQUIRE(U(SetEnvironmentVariableW)); REQUIRE(U(GetEnvironmentVariableW));
    static WCHAR val[32780], buf[32780];
    ev_wfill(val, 32766, L'z');
    CHECK(U(SetEnvironmentVariableW)(L"K32T_EV_BIG", val));
    EXPECT_EQ(U(GetEnvironmentVariableW)(L"K32T_EV_BIG", NULL, 0), 32767);
    EXPECT_EQ(U(GetEnvironmentVariableW)(L"K32T_EV_BIG", buf, 32767), 32766);
    CHECK(!wcscmp(buf, val));
    U(SetEnvironmentVariableW)(L"K32T_EV_BIG", NULL);
}
TEST(envvar_w_value_over_32767_note) {
    REQUIRE(U(SetEnvironmentVariableW)); REQUIRE(U(GetEnvironmentVariableW));
    static WCHAR val[40010];
    ev_wfill(val, 32767, L'z');
    SET_ERR(0);
    BOOL r = U(SetEnvironmentVariableW)(L"K32T_EV_BIG2", val);
    t_note("32767-char value: set=%d err=%lu (documented limit is 32767 incl. NUL)", r, GetLastError());
    if (r) U(SetEnvironmentVariableW)(L"K32T_EV_BIG2", NULL);
    ev_wfill(val, 40000, L'z');
    SET_ERR(0);
    r = U(SetEnvironmentVariableW)(L"K32T_EV_BIG3", val);
    t_note("40000-char value: set=%d err=%lu", r, GetLastError());
    if (r) U(SetEnvironmentVariableW)(L"K32T_EV_BIG3", NULL);
}
TEST(envvar_a_value_over_ushort_range) {
    /* an ANSI string longer than 65535 bytes cannot be converted to a UNICODE_STRING */
    REQUIRE(U(SetEnvironmentVariableA));
    char *v = (char *)malloc(70010);
    if (!v) { t_skip("alloc"); return; }
    memset(v, 'y', 70000); v[70000] = 0;
    SET_ERR(0);
    BOOL r = U(SetEnvironmentVariableA)("K32T_EV_A70K", v);
    CHECK(!r);
    DWORD e = GetLastError();
    CHECKF(e != 0, "last error %lu", e);
    t_note("70000-byte A value: set=%d err=%lu", r, e);
    if (r) ev_del("K32T_EV_A70K");
    free(v);
}

/* ---------- A / W parity ---------- */
TEST(envvar_ascii_parity_aw) {
    REQUIRE(U(SetEnvironmentVariableA)); REQUIRE(U(GetEnvironmentVariableW)); REQUIRE(U(SetEnvironmentVariableW)); REQUIRE(U(GetEnvironmentVariableA));
    WCHAR w[32]; char a[32];
    U(SetEnvironmentVariableA)("K32T_EV_PAR", "via-a");
    EXPECT_EQ(U(GetEnvironmentVariableW)(L"K32T_EV_PAR", w, 32), 5);
    EXPECT_WSTR(w, L"via-a");
    U(SetEnvironmentVariableW)(L"K32T_EV_PAR", L"via-w");
    EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_EV_PAR", a, 32), 5);
    EXPECT_STR(a, "via-w");
    ev_del("K32T_EV_PAR");
}
TEST(envvar_a_high_bytes_convert_via_acp) {
    REQUIRE(U(SetEnvironmentVariableA)); REQUIRE(U(GetEnvironmentVariableW));
    char va[8] = { 'c', 'a', 'f', (char)0xE9, 0 };
    WCHAR expect[8], got[8];
    int n = MultiByteToWideChar(CP_ACP, 0, va, -1, expect, 8);
    CHECK(n > 0);
    CHECK(U(SetEnvironmentVariableA)("K32T_EV_HB", va));
    DWORD r = U(GetEnvironmentVariableW)(L"K32T_EV_HB", got, 8);
    EXPECT_EQ(r, (DWORD)(n - 1));
    CHECK(!wcscmp(got, expect));
    ev_del("K32T_EV_HB");
}
TEST(envvar_a_name_high_bytes_match_w_name) {
    REQUIRE(U(SetEnvironmentVariableA)); REQUIRE(U(GetEnvironmentVariableW));
    char na[16] = { 'K', '3', '2', 'T', '_', 'H', (char)0xE9, 0 };
    WCHAR nw[16], buf[8];
    MultiByteToWideChar(CP_ACP, 0, na, -1, nw, 16);
    CHECK(U(SetEnvironmentVariableA)(na, "x"));
    EXPECT_EQ(U(GetEnvironmentVariableW)(nw, buf, 8), 1);
    U(SetEnvironmentVariableW)(nw, NULL);
}
TEST(envvar_w_to_a_unrepresentable_uses_default_char) {
    REQUIRE(U(SetEnvironmentVariableW)); REQUIRE(U(GetEnvironmentVariableA));
    /* a CJK character is not representable in single-byte ANSI code pages */
    CHECK(U(SetEnvironmentVariableW)(L"K32T_EV_CJK", L"\x4E2D"));
    char buf[8];
    DWORD n = U(GetEnvironmentVariableA)("K32T_EV_CJK", buf, 8);
    CHECK(n >= 1 && n <= 3);
    t_note("ANSI length of U+4E2D under ACP %u is %lu", GetACP(), n);
    U(SetEnvironmentVariableW)(L"K32T_EV_CJK", NULL);
}
TEST(envvar_a_required_size_counts_ansi_bytes) {
    REQUIRE(U(SetEnvironmentVariableW)); REQUIRE(U(GetEnvironmentVariableA));
    CHECK(U(SetEnvironmentVariableW)(L"K32T_EV_RQ", L"\x00E9\x00E9\x00E9"));
    char tmp[16]; char buf[16];
    int want = WideCharToMultiByte(CP_ACP, 0, L"\x00E9\x00E9\x00E9", 3, tmp, 16, NULL, NULL);
    DWORD n = U(GetEnvironmentVariableA)("K32T_EV_RQ", buf, 0);
    EXPECT_EQ(n, (DWORD)want + 1);
    n = U(GetEnvironmentVariableA)("K32T_EV_RQ", buf, 16);
    EXPECT_EQ(n, (DWORD)want);
    U(SetEnvironmentVariableW)(L"K32T_EV_RQ", NULL);
}

/* ---------- predefined variables ---------- */
static DWORD ev_geta(const char *n, char *b, DWORD sz) { return U(GetEnvironmentVariableA)(n, b, sz); }
TEST(envvar_predefined_systemroot_windir) {
    REQUIRE(U(GetEnvironmentVariableA)); REQUIRE(U(GetWindowsDirectoryA));
    char sr[MAX_PATH], wd[MAX_PATH], w2[MAX_PATH];
    CHECK(ev_geta("SystemRoot", sr, MAX_PATH) > 2);
    CHECK(ev_geta("windir", wd, MAX_PATH) > 2);
    CHECK(U(GetWindowsDirectoryA)(w2, MAX_PATH) > 2);
    CHECK(!_stricmp(sr, wd));
    CHECK(!_stricmp(sr, w2));
    CHECK(sr[1] == ':' && sr[2] == '\\');
}
TEST(envvar_predefined_os_and_arch) {
    REQUIRE(U(GetEnvironmentVariableA));
    char b[64];
    if (ev_geta("OS", b, 64)) EXPECT_STR(b, "Windows_NT"); else CHECK(0);
    if (ev_geta("PROCESSOR_ARCHITECTURE", b, 64)) {
        CHECKF(!strcmp(b, "AMD64"), "arch=%s", b);
    } else CHECK(0);
    CHECK(ev_geta("PROCESSOR_IDENTIFIER", b, 64) > 0);
    CHECK(ev_geta("PROCESSOR_LEVEL", b, 64) > 0);
    CHECK(ev_geta("PROCESSOR_REVISION", b, 64) > 0);
}
TEST(envvar_predefined_number_of_processors) {
    REQUIRE(U(GetEnvironmentVariableA)); REQUIRE(U(GetActiveProcessorCount));
    char b[32];
    CHECK(ev_geta("NUMBER_OF_PROCESSORS", b, 32) > 0);
    int n = atoi(b);
    CHECK(n >= 1);
    DWORD total = U(GetActiveProcessorCount)(ALL_PROCESSOR_GROUPS);
    CHECKF((DWORD)n == total, "env %d api %lu", n, total);
}
TEST(envvar_predefined_path_pathext) {
    REQUIRE(U(GetEnvironmentVariableA));
    static char b[32768];
    CHECK(ev_geta("PATH", b, sizeof b) > 3);
    CHECK(ev_geta("PATHEXT", b, sizeof b) > 3);
    CHECK(strstr(b, ".EXE") || strstr(b, ".exe"));
    CHECK(ev_geta("ComSpec", b, sizeof b) > 3);
    CHECK(strstr(b, "cmd.exe") || strstr(b, "CMD.EXE") || strstr(b, "Cmd.exe"));
}
TEST(envvar_predefined_temp_tmp) {
    REQUIRE(U(GetEnvironmentVariableA));
    char b[MAX_PATH];
    CHECK(ev_geta("TEMP", b, MAX_PATH) > 2 || ev_geta("TMP", b, MAX_PATH) > 2);
}
TEST(envvar_predefined_systemdrive) {
    REQUIRE(U(GetEnvironmentVariableA));
    char b[16], sr[MAX_PATH];
    EXPECT_EQ(ev_geta("SystemDrive", b, 16), 2);
    ev_geta("SystemRoot", sr, MAX_PATH);
    CHECK(b[1] == ':' && toupper(b[0]) == toupper(sr[0]));
}
TEST(envvar_predefined_programfiles_x86_entry) {
    REQUIRE(U(GetEnvironmentVariableA));
    char b[MAX_PATH];
    DWORD n = ev_geta("ProgramFiles", b, MAX_PATH);
    CHECK(n > 3);
    n = ev_geta("ProgramFiles(x86)", b, MAX_PATH);
    t_note("ProgramFiles(x86) length %lu (present on 64-bit Windows)", n);
}
TEST(envvar_predefined_computername_matches_api) {
    REQUIRE(U(GetEnvironmentVariableA)); REQUIRE(U(GetComputerNameA));
    char env[64], api[64]; DWORD sz = 64;
    CHECK(ev_geta("COMPUTERNAME", env, 64) > 0);
    CHECK(U(GetComputerNameA)(api, &sz));
    CHECK(!_stricmp(env, api));
}
TEST(envvar_predefined_username_userprofile) {
    REQUIRE(U(GetEnvironmentVariableA));
    char b[MAX_PATH];
    t_note("USERNAME len %lu, USERPROFILE len %lu, APPDATA len %lu",
           ev_geta("USERNAME", b, MAX_PATH), ev_geta("USERPROFILE", b, MAX_PATH), ev_geta("APPDATA", b, MAX_PATH));
}

/* ---------- ntdll cross-check ---------- */
UDECL(NTSTATUS, RtlQueryEnvironmentVariable_U, (PVOID, PUNICODE_STRING, PUNICODE_STRING));
UDECL(NTSTATUS, RtlSetEnvironmentVariable, (PVOID *, PUNICODE_STRING, PUNICODE_STRING));
TEST(envvar_ntdll_query_matches_kernel32) {
    REQUIRE(UN(RtlQueryEnvironmentVariable_U)); REQUIRE(U(SetEnvironmentVariableW));
    U(SetEnvironmentVariableW)(L"K32T_EV_NT", L"ntval");
    UNICODE_STRING name, val; WCHAR vb[32];
    RtlInitUnicodeString(&name, L"k32t_ev_nt");
    val.Buffer = vb; val.Length = 0; val.MaximumLength = sizeof vb;
    NTSTATUS st = UN(RtlQueryEnvironmentVariable_U)(NULL, &name, &val);
    EXPECT_EQ(st, 0);
    EXPECT_EQ(val.Length, 10);
    CHECK(!wcsncmp(vb, L"ntval", 5));
    /* small buffer -> STATUS_BUFFER_TOO_SMALL (0xC0000023) with Length = needed bytes */
    val.MaximumLength = 4; val.Length = 0;
    st = UN(RtlQueryEnvironmentVariable_U)(NULL, &name, &val);
    EXPECT_EQ((DWORD)st, 0xC0000023);
    /* missing -> STATUS_VARIABLE_NOT_FOUND */
    RtlInitUnicodeString(&name, L"K32T_EV_NT_MISSING");
    val.MaximumLength = sizeof vb;
    st = UN(RtlQueryEnvironmentVariable_U)(NULL, &name, &val);
    EXPECT_EQ((DWORD)st, 0xC0000100);
    U(SetEnvironmentVariableW)(L"K32T_EV_NT", NULL);
}
TEST(envvar_ntdll_set_matches_kernel32_get) {
    REQUIRE(UN(RtlSetEnvironmentVariable)); REQUIRE(U(GetEnvironmentVariableW));
    UNICODE_STRING name, val; WCHAR buf[16];
    RtlInitUnicodeString(&name, L"K32T_EV_NT2"); RtlInitUnicodeString(&val, L"setbynt");
    EXPECT_EQ(UN(RtlSetEnvironmentVariable)(NULL, &name, &val), 0);
    EXPECT_EQ(U(GetEnvironmentVariableW)(L"K32T_EV_NT2", buf, 16), 7);
    EXPECT_EQ(UN(RtlSetEnvironmentVariable)(NULL, &name, NULL), 0);
    SET_ERR(0);
    EXPECT_EQ(U(GetEnvironmentVariableW)(L"K32T_EV_NT2", buf, 16), 0);
    EXPECT_ERR(ENOTFOUND);
    /* deleting a missing variable -> STATUS_VARIABLE_NOT_FOUND */
    t_note("RtlSetEnvironmentVariable delete missing -> 0x%lx", (DWORD)UN(RtlSetEnvironmentVariable)(NULL, &name, NULL));
}
TEST(envvar_ntdll_status_to_dos_error_mapping) {
    REQUIRE(UNT(RtlNtStatusToDosError));
    EXPECT_EQ(UNT(RtlNtStatusToDosError)(0xC0000100), ENOTFOUND);
    EXPECT_EQ(UNT(RtlNtStatusToDosError)(0xC000000D), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(UNT(RtlNtStatusToDosError)(0xC0000023), ERROR_INSUFFICIENT_BUFFER);
}
