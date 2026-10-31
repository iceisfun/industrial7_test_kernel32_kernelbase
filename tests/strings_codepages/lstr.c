/* lstrlen / lstrcpy / lstrcpyn / lstrcat / lstrcmp / lstrcmpi */
#include "sc.h"

TEST(strcp_lstrlenA_basic) {
    REQUIRE(U(lstrlenA));
    static const struct { const char *s; int n; } v[] = {
        {"", 0}, {"a", 1}, {"ab", 2}, {"hello", 5}, {"hello world", 11}, {"\x80\xff", 2},
        {"a\0b", 1}, {"\0abc", 0}, {"0123456789012345678901234567890123456789012345678901234567890123", 64},
        {"tab\there", 8}, {"\xe9\xe8", 2},
    };
    for (int i = 0; i < ARRAY_N(v); i++) EXPECT_EQ(U(lstrlenA)(v[i].s), v[i].n);
}
TEST(strcp_lstrlenA_null) {
    REQUIRE(U(lstrlenA));
    if (t_is_wine()) { t_skip("emulator faults visibly on NULL"); return; }
    EXPECT_EQ(U(lstrlenA)(NULL), 0);
}
TEST(strcp_lstrlenW_basic) {
    REQUIRE(U(lstrlenW));
    static const WCHAR e[] = W_(0), a[] = W_('a', 0), h[] = W_('h', 'e', 'l', 'l', 'o', 0), u[] = W_(0x20ac, 0x4e2d, 0xd83d, 0xde00, 0),
        z[] = W_('a', 0, 'b', 0), hi[] = W_(0xffff, 0xfffe, 0);
    EXPECT_EQ(U(lstrlenW)(e), 0);
    EXPECT_EQ(U(lstrlenW)(a), 1);
    EXPECT_EQ(U(lstrlenW)(h), 5);
    EXPECT_EQ(U(lstrlenW)(u), 4);
    EXPECT_EQ(U(lstrlenW)(z), 1);
    EXPECT_EQ(U(lstrlenW)(hi), 2);
}
TEST(strcp_lstrlenW_null) {
    REQUIRE(U(lstrlenW));
    if (t_is_wine()) { t_skip("emulator faults visibly on NULL"); return; }
    EXPECT_EQ(U(lstrlenW)(NULL), 0);
}
TEST(strcp_lstrlen_long) {
    REQUIRE(U(lstrlenA)); REQUIRE(U(lstrlenW));
    static char a[70000]; static WCHAR w[70000];
    for (int n = 1; n <= 65536; n *= 2) {
        memset(a, 'x', n); a[n] = 0;
        for (int i = 0; i < n; i++) w[i] = 'y';
        w[n] = 0;
        EXPECT_EQ(U(lstrlenA)(a), n);
        EXPECT_EQ(U(lstrlenW)(w), n);
    }
}
TEST(strcp_lstrlen_aliases_unalignedW) {
    /* uaw_lstrlenW is an exported alias for lstrlenW. */
    FARPROC p = t_proc_k32("uaw_lstrlenW");
    if (!p) { t_note("uaw_lstrlenW not exported"); return; }
    static const WCHAR s[] = W_('a', 'b', 'c', 0);
    EXPECT_EQ(((int (WINAPI *)(const WCHAR *))p)(s), 3);
}

TEST(strcp_lstrcpyA_basic) {
    REQUIRE(U(lstrcpyA));
    char buf[32];
    memset(buf, 'Z', sizeof buf);
    LPSTR r = U(lstrcpyA)(buf, "abc");
    EXPECT_EQ(r, buf);
    CHECK(!memcmp(buf, "abc\0ZZZ", 7));
    r = U(lstrcpyA)(buf, "");
    EXPECT_EQ(r, buf);
    CHECK(buf[0] == 0 && buf[1] == 'b');
    r = U(lstrcpyA)(buf, "\x80\xe9\xff");
    CHECK(!memcmp(buf, "\x80\xe9\xff\0", 4));
}
TEST(strcp_lstrcpyW_basic) {
    REQUIRE(U(lstrcpyW));
    WCHAR buf[16]; for (int i = 0; i < 16; i++) buf[i] = 0x5a5a;
    static const WCHAR s[] = W_(0x20ac, 'x', 0), e[] = W_(0);
    LPWSTR r = U(lstrcpyW)(buf, s);
    EXPECT_EQ(r, buf);
    CHECK(buf[0] == 0x20ac && buf[1] == 'x' && buf[2] == 0 && buf[3] == 0x5a5a);
    r = U(lstrcpyW)(buf, e);
    CHECK(buf[0] == 0 && buf[1] == 'x');
}
TEST(strcp_lstrcpyA_exactfit) {
    REQUIRE(U(lstrcpyA));
    char buf[8] = "XXXXXXX";
    U(lstrcpyA)(buf, "1234567");
    CHECK(!memcmp(buf, "1234567", 8));
}

/* lstrcpyn: copies at most n-1 chars and always NUL-terminates when n>0. */
TEST(strcp_lstrcpynA_table) {
    REQUIRE(U(lstrcpynA));
    static const struct { const char *src; int n; const char *exp; } v[] = {
        {"abcdef", 1, ""}, {"abcdef", 2, "a"}, {"abcdef", 3, "ab"}, {"abcdef", 5, "abcd"}, {"abcdef", 6, "abcde"},
        {"abcdef", 7, "abcdef"}, {"abcdef", 8, "abcdef"}, {"abcdef", 100, "abcdef"},
        {"", 1, ""}, {"", 2, ""}, {"", 10, ""}, {"a", 1, ""}, {"a", 2, "a"}, {"a", 3, "a"},
        {"\x80\xe9", 3, "\x80\xe9"}, {"\x80\xe9", 2, "\x80"},
    };
    for (int i = 0; i < ARRAY_N(v); i++) {
        char buf[128]; memset(buf, 'Q', sizeof buf);
        LPSTR r = U(lstrcpynA)(buf, v[i].src, v[i].n);
        EXPECT_EQ(r, buf);
        size_t el = strlen(v[i].exp);
        CHECKF(!memcmp(buf, v[i].exp, el + 1), "vec %d n=%d", i, v[i].n);
        /* nothing after the terminator is touched */
        CHECKF(buf[el + 1] == 'Q', "vec %d spill", i);
    }
}
TEST(strcp_lstrcpynA_zero) {
    REQUIRE(U(lstrcpynA));
    char buf[4] = "QQQ";
    LPSTR r = U(lstrcpynA)(buf, "abc", 0);
    EXPECT_EQ(r, buf);
    CHECK(!memcmp(buf, "QQQ", 4));
}
TEST(strcp_lstrcpynA_negative) {
    REQUIRE(U(lstrcpynA));
    char buf[4] = "QQQ";
    /* iMaxLength < 0 treated as huge unsigned count by the loop: only test with a short source, never overruns. */
    U(lstrcpynA)(buf, "ab", -1);
    CHECK(!memcmp(buf, "ab\0", 4));
}
TEST(strcp_lstrcpynA_truncate_no_overrun) {
    REQUIRE(U(lstrcpynA));
    char buf[10]; memset(buf, 'Q', sizeof buf);
    U(lstrcpynA)(buf + 2, "0123456789", 4);
    CHECK(buf[0] == 'Q' && buf[1] == 'Q');
    CHECK(!memcmp(buf + 2, "012\0", 4));
    CHECK(buf[6] == 'Q' && buf[7] == 'Q');
}
TEST(strcp_lstrcpynW_table) {
    REQUIRE(U(lstrcpynW));
    static const WCHAR src[] = W_('a', 'b', 'c', 'd', 'e', 'f', 0), em[] = W_(0), wide[] = W_(0x20ac, 0x4e2d, 0);
    for (int n = 0; n <= 9; n++) {
        WCHAR buf[16]; for (int i = 0; i < 16; i++) buf[i] = 0x5151;
        LPWSTR r = U(lstrcpynW)(buf, src, n);
        EXPECT_EQ(r, buf);
        int copied = n == 0 ? 0 : (n - 1 < 6 ? n - 1 : 6);
        for (int i = 0; i < copied; i++) CHECKF(buf[i] == src[i], "n=%d i=%d", n, i);
        if (n) CHECKF(buf[copied] == 0, "n=%d term", n);
        else CHECK(buf[0] == 0x5151);
        CHECKF(buf[(n ? copied + 1 : 1)] == 0x5151, "n=%d spill", n);
    }
    WCHAR b2[4] = W_(1, 2, 3, 4);
    U(lstrcpynW)(b2, em, 1); CHECK(b2[0] == 0 && b2[1] == 2);
    U(lstrcpynW)(b2, wide, 2); CHECK(b2[0] == 0x20ac && b2[1] == 0 && b2[2] == 3);
    U(lstrcpynW)(b2, wide, 3); CHECK(b2[0] == 0x20ac && b2[1] == 0x4e2d && b2[2] == 0);
}
TEST(strcp_lstrcpynW_one) {
    REQUIRE(U(lstrcpynW));
    WCHAR b[3] = W_(7, 7, 7); static const WCHAR s[] = W_('x', 0);
    U(lstrcpynW)(b, s, 1);
    CHECK(b[0] == 0 && b[1] == 7);
}

TEST(strcp_lstrcatA_basic) {
    REQUIRE(U(lstrcatA));
    char buf[32] = "foo";
    LPSTR r = U(lstrcatA)(buf, "bar");
    EXPECT_EQ(r, buf);
    EXPECT_STR(buf, "foobar");
    U(lstrcatA)(buf, "");
    EXPECT_STR(buf, "foobar");
    buf[0] = 0;
    U(lstrcatA)(buf, "x");
    EXPECT_STR(buf, "x");
    U(lstrcatA)(buf, "\xe9\xff");
    CHECK(!memcmp(buf, "x\xe9\xff\0", 4));
    char b2[8] = "a\0QQQQQ";
    U(lstrcatA)(b2, "bc");
    CHECK(!memcmp(b2, "abc\0QQQ", 7));
}
TEST(strcp_lstrcatW_basic) {
    REQUIRE(U(lstrcatW));
    WCHAR buf[16] = W_('f', 'o', 'o', 0);
    static const WCHAR bar[] = W_('b', 'a', 'r', 0), e[] = W_(0), w[] = W_(0x20ac, 0);
    LPWSTR r = U(lstrcatW)(buf, bar);
    EXPECT_EQ(r, buf);
    EXPECT_EQ(U(lstrlenW) ? U(lstrlenW)(buf) : 6, 6);
    CHECK(buf[3] == 'b' && buf[5] == 'r' && buf[6] == 0);
    U(lstrcatW)(buf, e); CHECK(buf[6] == 0);
    U(lstrcatW)(buf, w); CHECK(buf[6] == 0x20ac && buf[7] == 0);
}
TEST(strcp_lstrcat_repeated) {
    REQUIRE(U(lstrcatA)); REQUIRE(U(lstrlenA));
    char buf[256] = "";
    for (int i = 0; i < 50; i++) { U(lstrcatA)(buf, "ab"); EXPECT_EQ(U(lstrlenA)(buf), 2 * (i + 1)); }
}

/* ---- lstrcmp family ---- */
static int sgn(int x) { return x < 0 ? -1 : x > 0 ? 1 : 0; }
TEST(strcp_lstrcmpA_basic) {
    REQUIRE(U(lstrcmpA));
    static const struct { const char *a, *b; int r; } v[] = {
        {"", "", 0}, {"a", "a", 0}, {"abc", "abc", 0}, {"a", "b", -1}, {"b", "a", 1}, {"abc", "abd", -1}, {"abd", "abc", 1},
        {"ab", "abc", -1}, {"abc", "ab", 1}, {"", "a", -1}, {"a", "", 1},
        {"a", "A", -1}, {"A", "a", 1}, {"a", "B", -1}, {"B", "a", 1}, {"A", "b", -1}, {"abc", "ABC", -1},
        {"apple", "banana", -1}, {"Zebra", "apple", 1}, {"1", "2", -1}, {"10", "2", -1}, {"2", "10", 1},
        {"a1", "a2", -1}, {"A", "B", -1},
    };
    for (int i = 0; i < ARRAY_N(v); i++) CHECKF(sgn(U(lstrcmpA)(v[i].a, v[i].b)) == v[i].r, "vec %d [%s] [%s] got %d", i, v[i].a, v[i].b, U(lstrcmpA)(v[i].a, v[i].b));
}
TEST(strcp_lstrcmpA_return_values_are_unit) {
    REQUIRE(U(lstrcmpA));
    int r1 = U(lstrcmpA)("a", "z"), r2 = U(lstrcmpA)("z", "a"), r3 = U(lstrcmpA)("q", "q");
    EXPECT_EQ(r1, -1); EXPECT_EQ(r2, 1); EXPECT_EQ(r3, 0);
}
TEST(strcp_lstrcmpA_null) {
    REQUIRE(U(lstrcmpA));
    EXPECT_EQ(U(lstrcmpA)(NULL, NULL), 0);
    EXPECT_EQ(U(lstrcmpA)(NULL, "a"), -1);
    EXPECT_EQ(U(lstrcmpA)("a", NULL), 1);
    EXPECT_EQ(U(lstrcmpA)(NULL, ""), -1);
    EXPECT_EQ(U(lstrcmpA)("", NULL), 1);
}
TEST(strcp_lstrcmpW_null) {
    REQUIRE(U(lstrcmpW));
    static const WCHAR a[] = W_('a', 0), e[] = W_(0);
    EXPECT_EQ(U(lstrcmpW)(NULL, NULL), 0);
    EXPECT_EQ(U(lstrcmpW)(NULL, a), -1);
    EXPECT_EQ(U(lstrcmpW)(a, NULL), 1);
    EXPECT_EQ(U(lstrcmpW)(NULL, e), -1);
    EXPECT_EQ(U(lstrcmpW)(e, NULL), 1);
}
TEST(strcp_lstrcmpiA_null) {
    REQUIRE(U(lstrcmpiA));
    EXPECT_EQ(U(lstrcmpiA)(NULL, NULL), 0);
    EXPECT_EQ(U(lstrcmpiA)(NULL, "a"), -1);
    EXPECT_EQ(U(lstrcmpiA)("a", NULL), 1);
}
TEST(strcp_lstrcmpiW_null) {
    REQUIRE(U(lstrcmpiW));
    static const WCHAR a[] = W_('a', 0);
    EXPECT_EQ(U(lstrcmpiW)(NULL, NULL), 0);
    EXPECT_EQ(U(lstrcmpiW)(NULL, a), -1);
    EXPECT_EQ(U(lstrcmpiW)(a, NULL), 1);
}
TEST(strcp_lstrcmpiA_table) {
    REQUIRE(U(lstrcmpiA));
    static const struct { const char *a, *b; int r; } v[] = {
        {"", "", 0}, {"a", "A", 0}, {"A", "a", 0}, {"abc", "ABC", 0}, {"AbC", "aBc", 0}, {"abc", "abd", -1}, {"ABD", "abc", 1},
        {"a", "B", -1}, {"B", "a", 1}, {"hello", "HELLO", 0}, {"hello", "HELLO!", -1}, {"Z", "a", 1}, {"z", "A", 1},
        {"file1", "FILE2", -1}, {"File.TXT", "file.txt", 0}, {"x", "", 1}, {"", "x", -1},
        {"\xe9", "\xc9", 0},   /* e-acute vs E-acute in CP1252 */
    };
    for (int i = 0; i < ARRAY_N(v); i++) {
        int r = sgn(U(lstrcmpiA)(v[i].a, v[i].b));
        if (i == ARRAY_N(v) - 1) { if (GetACP() == 1252) CHECKF(r == v[i].r, "got %d", r); else t_note("ACP %u: e-acute case fold", GetACP()); continue; }
        CHECKF(r == v[i].r, "vec %d [%s] [%s] got %d", i, v[i].a, v[i].b, r);
    }
}
TEST(strcp_lstrcmpW_table) {
    REQUIRE(U(lstrcmpW));
    static const struct { WCHAR a[8], b[8]; int r; } v[] = {
        {W_(0), W_(0), 0}, {W_('a', 0), W_('a', 0), 0}, {W_('a', 0), W_('b', 0), -1}, {W_('b', 0), W_('a', 0), 1},
        {W_('a', 0), W_('A', 0), -1}, {W_('A', 0), W_('a', 0), 1}, {W_('a', 'b', 0), W_('a', 0), 1},
        {W_(0), W_('a', 0), -1}, {W_('a', 0), W_(0), 1},
        {W_(0xe9, 0), W_('e', 0), 1}, {W_('e', 0), W_(0xe9, 0), -1}, {W_(0xe9, 0), W_('f', 0), -1},
        {W_('a', 0), W_(0x4e2d, 0), -1}, {W_(0x4e2d, 0), W_('a', 0), 1},
        {W_('1', 0), W_('2', 0), -1}, {W_('9', 0), W_('a', 0), -1},
    };
    for (int i = 0; i < ARRAY_N(v); i++) { int r = sgn(U(lstrcmpW)(v[i].a, v[i].b)); CHECKF(r == v[i].r, "vec %d got %d", i, r); }
}
TEST(strcp_lstrcmpiW_table) {
    REQUIRE(U(lstrcmpiW));
    static const struct { WCHAR a[8], b[8]; int r; } v[] = {
        {W_(0), W_(0), 0}, {W_('a', 0), W_('A', 0), 0}, {W_('A', 'b', 'C', 0), W_('a', 'B', 'c', 0), 0},
        {W_(0xe9, 0), W_(0xc9, 0), 0}, {W_(0x3b1, 0), W_(0x391, 0), 0}, {W_(0x430, 0), W_(0x410, 0), 0},
        {W_('a', 0), W_('b', 0), -1}, {W_('B', 0), W_('a', 0), 1}, {W_('a', 'b', 0), W_('A', 0), 1},
        {W_(0x4e2d, 0), W_(0x4e2d, 0), 0}, {W_(0xff21, 0), W_(0xff41, 0), 0}, {W_('z', 0), W_('A', 0), 1},
    };
    for (int i = 0; i < ARRAY_N(v); i++) { int r = sgn(U(lstrcmpiW)(v[i].a, v[i].b)); CHECKF(r == v[i].r, "vec %d got %d", i, r); }
}
TEST(strcp_lstrcmp_word_sort_hyphen) {
    /* lstrcmp uses word sort: hyphen / apostrophe are skipped for the primary comparison. */
    REQUIRE(U(lstrcmpA));
    EXPECT_EQ(sgn(U(lstrcmpA)("co-op", "con")), 1);
    EXPECT_EQ(sgn(U(lstrcmpA)("coop", "con")), 1);
    EXPECT_EQ(sgn(U(lstrcmpA)("con", "co-op")), -1);
    /* unequal: the hyphenated form still differs from the plain one */
    CHECK(U(lstrcmpA)("co-op", "coop") != 0);
    CHECK(U(lstrcmpA)("it's", "its") != 0);
    t_note("lstrcmpA(coop,co-op)=%d lstrcmpA(its,it's)=%d", U(lstrcmpA)("coop", "co-op"), U(lstrcmpA)("its", "it's"));
}
TEST(strcp_lstrcmp_symmetry) {
    REQUIRE(U(lstrcmpA)); REQUIRE(U(lstrcmpiA)); REQUIRE(U(lstrcmpW)); REQUIRE(U(lstrcmpiW));
    static const char *s[] = {"", "a", "A", "b", "B", "ab", "Ab", "aB", "abc", "a-b", "a'b", "a b", "1", "10", "2", "z", "Z"};
    for (int i = 0; i < ARRAY_N(s); i++) for (int j = 0; j < ARRAY_N(s); j++) {
        CHECKF(sgn(U(lstrcmpA)(s[i], s[j])) == -sgn(U(lstrcmpA)(s[j], s[i])), "A %d %d", i, j);
        CHECKF(sgn(U(lstrcmpiA)(s[i], s[j])) == -sgn(U(lstrcmpiA)(s[j], s[i])), "iA %d %d", i, j);
        WCHAR a[8], b[8]; for (int k = 0; k < 8; k++) { a[k] = (unsigned char)s[i][k]; b[k] = (unsigned char)s[j][k]; if (!s[i][k]) { for (; k < 8; k++) a[k] = 0; break; } }
        for (int k = 0; k < 8; k++) { b[k] = (unsigned char)s[j][k]; if (!s[j][k]) { for (; k < 8; k++) b[k] = 0; break; } }
        CHECKF(sgn(U(lstrcmpW)(a, b)) == sgn(U(lstrcmpA)(s[i], s[j])), "W/A parity %d %d", i, j);
        CHECKF(sgn(U(lstrcmpiW)(a, b)) == sgn(U(lstrcmpiA)(s[i], s[j])), "iW/iA parity %d %d", i, j);
    }
}
TEST(strcp_lstrcmpi_ascii_fold_all) {
    REQUIRE(U(lstrcmpiA)); REQUIRE(U(lstrcmpiW));
    for (int c = 'a'; c <= 'z'; c++) {
        char x[2] = {(char)c, 0}, y[2] = {(char)(c - 32), 0};
        EXPECT_EQ(U(lstrcmpiA)(x, y), 0);
        WCHAR xw[2] = {(WCHAR)c, 0}, yw[2] = {(WCHAR)(c - 32), 0};
        EXPECT_EQ(U(lstrcmpiW)(xw, yw), 0);
    }
}
