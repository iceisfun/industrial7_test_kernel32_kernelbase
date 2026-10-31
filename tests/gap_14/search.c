/* Str search / scan / compare exports. */
#include "t.h"
#include <shlwapi.h>

/* ---------- StrStr family ---------- */
static const struct { const char *hay; const char *needle; int cs; int ci; } strrows[] = {
    /* expected offsets, -1 for NULL */
    {"hello world", "world", 6, 6}, {"hello world", "WORLD", -1, 6}, {"hello world", "hello", 0, 0},
    {"hello world", "o", 4, 4}, {"hello world", "O", -1, 4}, {"hello world", "lo w", 3, 3},
    {"hello world", "xyz", -1, -1}, {"hello world", "hello world", 0, 0}, {"hello world", "hello worlds", -1, -1},
    {"aaab", "aab", 1, 1}, {"aaab", "AAB", -1, 1}, {"abcabc", "abc", 0, 0}, {"abcabc", "cab", 2, 2},
    {"abc", "d", -1, -1}, {"abc", "c", 2, 2}, {"abc", "bcd", -1, -1}, {"", "a", -1, -1},
    {"ab", "abc", -1, -1}, {"mississippi", "issip", 4, 4}, {"mississippi", "ISSIP", -1, 4},
    {"mississippi", "ssi", 2, 2}, {"x", "x", 0, 0}, {"X", "x", -1, 0},
};
#define NSTR (sizeof strrows / sizeof strrows[0])

static void wide(wchar_t *o, const char *s) { while ((*o++ = (unsigned char)*s++)) ; }

#define STRTEST(name, fn, field) \
TEST(name##_a) { REQUIRE(U(fn##A)); \
    for (size_t i = 0; i < NSTR; i++) { char *r = U(fn##A)(strrows[i].hay, strrows[i].needle); \
        int want = strrows[i].field; \
        CHECKF(want < 0 ? r == NULL : r == strrows[i].hay + want, "row %zu \"%s\" in \"%s\"", i, strrows[i].needle, strrows[i].hay); } } \
TEST(name##_w) { REQUIRE(U(fn##W)); \
    for (size_t i = 0; i < NSTR; i++) { wchar_t h[64], n[64]; wide(h, strrows[i].hay); wide(n, strrows[i].needle); \
        wchar_t *r = U(fn##W)(h, n); int want = strrows[i].field; \
        CHECKF(want < 0 ? r == NULL : r == h + want, "row %zu \"%s\" in \"%s\"", i, strrows[i].needle, strrows[i].hay); } }

STRTEST(gap14_strstr, StrStr, cs)
STRTEST(gap14_strstri, StrStrI, ci)

TEST(gap14_strstr_null_args) {
    REQUIRE(U(StrStrA)); REQUIRE(U(StrStrW)); REQUIRE(U(StrStrIA)); REQUIRE(U(StrStrIW));
    CHECK(U(StrStrA)(NULL, "a") == NULL); CHECK(U(StrStrA)("a", NULL) == NULL); CHECK(U(StrStrA)(NULL, NULL) == NULL);
    CHECK(U(StrStrW)(NULL, L"a") == NULL); CHECK(U(StrStrW)(L"a", NULL) == NULL); CHECK(U(StrStrW)(NULL, NULL) == NULL);
    CHECK(U(StrStrIA)(NULL, "a") == NULL); CHECK(U(StrStrIA)("a", NULL) == NULL);
    CHECK(U(StrStrIW)(NULL, L"a") == NULL); CHECK(U(StrStrIW)(L"a", NULL) == NULL);
}

TEST(gap14_strstr_empty_needle_note) {
    REQUIRE(U(StrStrA)); REQUIRE(U(StrStrW)); REQUIRE(U(StrStrIA)); REQUIRE(U(StrStrIW));
    const char *h = "abc"; const wchar_t *hw = L"abc";
    t_note("StrStrA(abc,\"\")=%p (base %p)", (void *)U(StrStrA)(h, ""), (const void *)h);
    t_note("StrStrW(abc,\"\")=%p", (void *)U(StrStrW)(hw, L""));
    t_note("StrStrIA(abc,\"\")=%p", (void *)U(StrStrIA)(h, ""));
    t_note("StrStrIW(abc,\"\")=%p", (void *)U(StrStrIW)(hw, L""));
}

TEST(gap14_strstr_returns_first_of_many) {
    REQUIRE(U(StrStrW)); REQUIRE(U(StrStrIW));
    const wchar_t *h = L"xAbxabxAB";
    CHECK(U(StrStrW)(h, L"ab") == h + 4);
    CHECK(U(StrStrIW)(h, L"ab") == h + 1);
    CHECK(U(StrStrW)(h, L"AB") == h + 7);
}

TEST(gap14_strstri_nonascii_case) {
    REQUIRE(U(StrStrIW));
    const wchar_t *h = L"xx\x00e9\x00c9yy";   /* e-acute, E-acute */
    wchar_t *r = U(StrStrIW)(h, L"\x00c9");
    CHECK(r == h + 2);
    CHECK(U(StrStrIW)(h, L"\x00e9\x00e9") == h + 2);
}

/* ---------- StrStrN / StrStrNI ---------- */
static const struct { const wchar_t *hay; const wchar_t *needle; UINT max; int cs; int ci; } nrows[] = {
    {L"abcdef", L"cd", 6, 2, 2}, {L"abcdef", L"cd", 4, 2, 2}, {L"abcdef", L"cd", 3, 2, 2},
    {L"abcdef", L"cd", 2, -1, -1}, {L"abcdef", L"cd", 0, -1, -1}, {L"abcdef", L"cd", 1, -1, -1},
    {L"abcdef", L"CD", 6, -1, 2}, {L"abcdef", L"ab", 2, 0, 0}, {L"abcdef", L"ab", 1, -1, -1},
    {L"abcdef", L"ef", 6, 4, 4}, {L"abcdef", L"ef", 5, 4, 4}, {L"abcdef", L"abcdef", 6, 0, 0},
    {L"abcdef", L"abcdefg", 7, -1, -1}, {L"abcdef", L"abcdef", 5, -1, -1}, {L"abcdef", L"x", 6, -1, -1},
    {L"aXbXc", L"x", 5, -1, 1}, {L"abc", L"c", 3, 2, 2}, {L"abc", L"c", 2, -1, -1},
    {L"ababab", L"bab", 5, 1, 1}, {L"ababab", L"bab", 4, 1, 1}, {L"ababab", L"bab", 6, 1, 1},
    {L"abcdef", L"cd", 100, 2, 2},  /* max beyond string end: stops at NUL */
};
#define NNR (sizeof nrows / sizeof nrows[0])

TEST(gap14_strstrnw_table) {
    REQUIRE(U(StrStrNW));
    for (size_t i = 0; i < NNR; i++) {
        const wchar_t *r = U(StrStrNW)(nrows[i].hay, nrows[i].needle, nrows[i].max);
        CHECKF(nrows[i].cs < 0 ? r == NULL : r == nrows[i].hay + nrows[i].cs, "row %zu max %u got %p", i, nrows[i].max, (void *)r);
    }
}
TEST(gap14_strstrniw_table) {
    REQUIRE(U(StrStrNIW));
    for (size_t i = 0; i < NNR; i++) {
        const wchar_t *r = U(StrStrNIW)(nrows[i].hay, nrows[i].needle, nrows[i].max);
        CHECKF(nrows[i].ci < 0 ? r == NULL : r == nrows[i].hay + nrows[i].ci, "row %zu max %u got %p", i, nrows[i].max, (void *)r);
    }
}
TEST(gap14_strstrn_null_args) {
    REQUIRE(U(StrStrNW)); REQUIRE(U(StrStrNIW));
    CHECK(U(StrStrNW)(NULL, L"a", 5) == NULL); CHECK(U(StrStrNW)(L"a", NULL, 5) == NULL);
    CHECK(U(StrStrNIW)(NULL, L"a", 5) == NULL); CHECK(U(StrStrNIW)(L"a", NULL, 5) == NULL);
    CHECK(U(StrStrNW)(NULL, NULL, 0) == NULL); CHECK(U(StrStrNIW)(NULL, NULL, 0) == NULL);
}
TEST(gap14_strstrn_window_semantics) {
    REQUIRE(U(StrStrNW)); REQUIRE(U(StrStrNIW));
    /* the match START must lie in the first cchMax chars and cchMax >= needle length;
       the compare itself may extend past the window */
    wchar_t buf[8] = L"abcXYZ";
    CHECK(U(StrStrNW)(buf, L"cXY", 2) == NULL);      /* 'c' not in window */
    CHECK(U(StrStrNW)(buf, L"cXY", 3) == buf + 2);
    CHECK(U(StrStrNW)(buf, L"cXY", 4) == buf + 2);
    CHECK(U(StrStrNIW)(buf, L"cxy", 2) == NULL);
    CHECK(U(StrStrNIW)(buf, L"cxy", 3) == buf + 2);
    CHECK(U(StrStrNW)(buf, L"abcX", 3) == NULL);     /* window shorter than needle */
    CHECK(U(StrStrNW)(buf, L"abcX", 4) == buf);
}

/* ---------- StrRChr / StrRChrI ---------- */
TEST(gap14_strrchra_basic) {
    REQUIRE(U(StrRChrA));
    const char *s = "abcabcabc";
    CHECK(U(StrRChrA)(s, NULL, 'a') == s + 6);
    CHECK(U(StrRChrA)(s, NULL, 'c') == s + 8);
    CHECK(U(StrRChrA)(s, NULL, 'z') == NULL);
    CHECK(U(StrRChrA)(s, NULL, 'A') == NULL);
    CHECK(U(StrRChrA)(s, NULL, 0) == NULL);             /* terminator is outside [start,end) */
    CHECK(U(StrRChrA)(s, s + 6, 'a') == s + 3);         /* end exclusive */
    CHECK(U(StrRChrA)(s, s + 7, 'a') == s + 6);
    CHECK(U(StrRChrA)(s, s, 'a') == NULL);              /* empty range */
    CHECK(U(StrRChrA)(s, s + 1, 'a') == s);
    CHECK(U(StrRChrA)(s + 5, s, 'a') == NULL);          /* end before start */
    CHECK(U(StrRChrA)("", NULL, 'a') == NULL);
}
TEST(gap14_strrchrw_basic) {
    REQUIRE(U(StrRChrW));
    const wchar_t *s = L"abcabcabc";
    CHECK(U(StrRChrW)(s, NULL, L'a') == s + 6);
    CHECK(U(StrRChrW)(s, NULL, L'c') == s + 8);
    CHECK(U(StrRChrW)(s, NULL, L'z') == NULL);
    CHECK(U(StrRChrW)(s, NULL, L'A') == NULL);
    CHECK(U(StrRChrW)(s, NULL, 0) == NULL);
    CHECK(U(StrRChrW)(s, s + 6, L'a') == s + 3);
    CHECK(U(StrRChrW)(s, s + 7, L'a') == s + 6);
    CHECK(U(StrRChrW)(s, s, L'a') == NULL);
    CHECK(U(StrRChrW)(s + 5, s, L'a') == NULL);
    CHECK(U(StrRChrW)(L"", NULL, L'a') == NULL);
    CHECK(U(StrRChrW)(L"a\x0416" L"b", NULL, 0x0416) != NULL);
    CHECK(U(StrRChrW)(L"a\x0416" L"b", NULL, 0x0016) == NULL);
}
TEST(gap14_strrchria_basic) {
    REQUIRE(U(StrRChrIA));
    const char *s = "abCabcAbc";
    CHECK(U(StrRChrIA)(s, NULL, 'a') == s + 6);
    CHECK(U(StrRChrIA)(s, NULL, 'A') == s + 6);
    CHECK(U(StrRChrIA)(s, NULL, 'c') == s + 8);
    CHECK(U(StrRChrIA)(s, NULL, 'C') == s + 8);
    CHECK(U(StrRChrIA)(s, NULL, 'z') == NULL);
    CHECK(U(StrRChrIA)(s, s + 6, 'A') == s + 3);
    CHECK(U(StrRChrIA)(s, s + 3, 'c') == s + 2);
    CHECK(U(StrRChrIA)(s, s, 'a') == NULL);
    CHECK(U(StrRChrIA)(s, NULL, 0) == NULL);
}
TEST(gap14_strrchriw_basic) {
    REQUIRE(U(StrRChrIW));
    const wchar_t *s = L"abCabcAbc";
    CHECK(U(StrRChrIW)(s, NULL, L'a') == s + 6);
    CHECK(U(StrRChrIW)(s, NULL, L'A') == s + 6);
    CHECK(U(StrRChrIW)(s, NULL, L'C') == s + 8);
    CHECK(U(StrRChrIW)(s, NULL, L'z') == NULL);
    CHECK(U(StrRChrIW)(s, s + 6, L'A') == s + 3);
    CHECK(U(StrRChrIW)(s, s, L'a') == NULL);
    CHECK(U(StrRChrIW)(s, NULL, 0) == NULL);
    const wchar_t *u = L"\x00e9x\x00c9y";
    CHECK(U(StrRChrIW)(u, NULL, 0x00e9) == u + 2);
    CHECK(U(StrRChrIW)(u, NULL, 0x00c9) == u + 2);
}
TEST(gap14_strrchr_null_start) {
    REQUIRE(U(StrRChrW)); REQUIRE(U(StrRChrA)); REQUIRE(U(StrRChrIW)); REQUIRE(U(StrRChrIA));
    CHECK(U(StrRChrW)(NULL, NULL, L'a') == NULL);
    CHECK(U(StrRChrA)(NULL, NULL, 'a') == NULL);
    CHECK(U(StrRChrIW)(NULL, NULL, L'a') == NULL);
    CHECK(U(StrRChrIA)(NULL, NULL, 'a') == NULL);
}
TEST(gap14_strrchr_ab_parity) {
    REQUIRE(U(StrRChrA)); REQUIRE(U(StrRChrW)); REQUIRE(U(StrRChrIA)); REQUIRE(U(StrRChrIW));
    const char *a = "path/to\\file.txt"; wchar_t w[32]; wide(w, a);
    const char cs[] = "/\\.tTxz";
    for (size_t i = 0; i < sizeof cs - 1; i++) {
        char *ra = U(StrRChrA)(a, NULL, cs[i]); wchar_t *rw = U(StrRChrW)(w, NULL, cs[i]);
        CHECKF((ra == NULL) == (rw == NULL) && (!ra || ra - a == rw - w), "c=%c", cs[i]);
        ra = U(StrRChrIA)(a, NULL, cs[i]); rw = U(StrRChrIW)(w, NULL, cs[i]);
        CHECKF((ra == NULL) == (rw == NULL) && (!ra || ra - a == rw - w), "I c=%c", cs[i]);
    }
}

/* ---------- StrRStrI ---------- */
TEST(gap14_strrstria_basic) {
    REQUIRE(U(StrRStrIA));
    const char *s = "abcABCabc";
    CHECK(U(StrRStrIA)(s, NULL, "abc") == s + 6);
    CHECK(U(StrRStrIA)(s, NULL, "ABC") == s + 6);
    CHECK(U(StrRStrIA)(s, NULL, "cab") == s + 5);
    CHECK(U(StrRStrIA)(s, NULL, "xyz") == NULL);
    CHECK(U(StrRStrIA)(s, NULL, "") == NULL);
    CHECK(U(StrRStrIA)(s, s + 6, "abc") == s + 3);      /* match starts must lie before lpLast */
    CHECK(U(StrRStrIA)(s, s + 7, "abc") == s + 6);
    CHECK(U(StrRStrIA)(s, s, "abc") == NULL);
    CHECK(U(StrRStrIA)(s, s + 1, "abc") == s);
    CHECK(U(StrRStrIA)(s, NULL, "abcabcabcd") == NULL);
    CHECK(U(StrRStrIA)(s, NULL, "c") == s + 8);
    CHECK(U(StrRStrIA)(NULL, NULL, "a") == NULL);
    CHECK(U(StrRStrIA)(s, NULL, NULL) == NULL);
}
TEST(gap14_strrstriw_basic) {
    REQUIRE(U(StrRStrIW));
    const wchar_t *s = L"abcABCabc";
    CHECK(U(StrRStrIW)(s, NULL, L"abc") == s + 6);
    CHECK(U(StrRStrIW)(s, NULL, L"ABC") == s + 6);
    CHECK(U(StrRStrIW)(s, NULL, L"cab") == s + 5);
    CHECK(U(StrRStrIW)(s, NULL, L"xyz") == NULL);
    CHECK(U(StrRStrIW)(s, NULL, L"") == NULL);
    CHECK(U(StrRStrIW)(s, s + 6, L"abc") == s + 3);
    CHECK(U(StrRStrIW)(s, s + 7, L"abc") == s + 6);
    CHECK(U(StrRStrIW)(s, s, L"abc") == NULL);
    CHECK(U(StrRStrIW)(s, NULL, L"abcabcabcd") == NULL);
    CHECK(U(StrRStrIW)(NULL, NULL, L"a") == NULL);
    CHECK(U(StrRStrIW)(s, NULL, NULL) == NULL);
    CHECK(U(StrRStrIW)(L"", NULL, L"a") == NULL);
    const wchar_t *u = L"\x00e9z\x00c9z";
    CHECK(U(StrRStrIW)(u, NULL, L"\x00e9z") == u + 2);
}

/* ---------- StrSpn / StrPBrk ---------- */
static const struct { const char *s; const char *set; int n; int pbrk; } sprows[] = {
    {"abcdef", "abc", 3, 0}, {"abcdef", "cba", 3, 0}, {"abcdef", "xyz", 0, -1}, {"abcdef", "", 0, -1},
    {"", "abc", 0, -1}, {"", "", 0, -1}, {"aaaa", "a", 4, 0}, {"aaaab", "a", 4, 0}, {"baaa", "a", 0, 1},
    {"abcdef", "abcdef", 6, 0}, {"abcdef", "fedcba", 6, 0}, {"abcdef", "ABC", 0, -1},
    {"12345abc", "0123456789", 5, 0}, {"  \t x", " \t", 4, 0}, {"abc", "c", 0, 2}, {"abc", "bc", 0, 1},
    {"xyzabc", "abc", 0, 3}, {"abcdef", "f", 0, 5}, {"abcdef", "af", 1, 0},
};
#define NSP (sizeof sprows / sizeof sprows[0])

TEST(gap14_strspna_table) {
    REQUIRE(U(StrSpnA));
    for (size_t i = 0; i < NSP; i++) EXPECT_EQ(U(StrSpnA)(sprows[i].s, sprows[i].set), sprows[i].n);
}
TEST(gap14_strspnw_table) {
    REQUIRE(U(StrSpnW));
    for (size_t i = 0; i < NSP; i++) {
        wchar_t s[32], set[32]; wide(s, sprows[i].s); wide(set, sprows[i].set);
        EXPECT_EQ(U(StrSpnW)(s, set), sprows[i].n);
    }
}
TEST(gap14_strspn_null) {
    REQUIRE(U(StrSpnA)); REQUIRE(U(StrSpnW));
    CHECK(U(StrSpnA)(NULL, "a") == 0); CHECK(U(StrSpnA)("a", NULL) == 0); CHECK(U(StrSpnA)(NULL, NULL) == 0);
    CHECK(U(StrSpnW)(NULL, L"a") == 0); CHECK(U(StrSpnW)(L"a", NULL) == 0); CHECK(U(StrSpnW)(NULL, NULL) == 0);
}
TEST(gap14_strpbrka_table) {
    REQUIRE(U(StrPBrkA));
    for (size_t i = 0; i < NSP; i++) {
        char *r = U(StrPBrkA)(sprows[i].s, sprows[i].set);
        CHECKF(sprows[i].pbrk < 0 ? r == NULL : r == sprows[i].s + sprows[i].pbrk, "row %zu", i);
    }
}
TEST(gap14_strpbrkw_table) {
    REQUIRE(U(StrPBrkW));
    for (size_t i = 0; i < NSP; i++) {
        wchar_t s[32], set[32]; wide(s, sprows[i].s); wide(set, sprows[i].set);
        wchar_t *r = U(StrPBrkW)(s, set);
        CHECKF(sprows[i].pbrk < 0 ? r == NULL : r == s + sprows[i].pbrk, "row %zu", i);
    }
}
TEST(gap14_strpbrk_null) {
    REQUIRE(U(StrPBrkA)); REQUIRE(U(StrPBrkW));
    CHECK(U(StrPBrkA)(NULL, "a") == NULL); CHECK(U(StrPBrkA)("a", NULL) == NULL);
    CHECK(U(StrPBrkW)(NULL, L"a") == NULL); CHECK(U(StrPBrkW)(L"a", NULL) == NULL);
    CHECK(U(StrPBrkW)(NULL, NULL) == NULL);
}
TEST(gap14_strspn_unicode) {
    REQUIRE(U(StrSpnW)); REQUIRE(U(StrPBrkW));
    const wchar_t *s = L"\x0416\x0416\x0417z";
    CHECK(U(StrSpnW)(s, L"\x0416") == 2);
    CHECK(U(StrSpnW)(s, L"\x0416\x0417") == 3);
    CHECK(U(StrPBrkW)(s, L"z") == s + 3);
    CHECK(U(StrPBrkW)(s, L"\x0417") == s + 2);
    CHECK(U(StrSpnW)(L"\x0116", L"\x0016") == 0);   /* full 16-bit compare */
}

/* ---------- StrCmpW / StrCmpNW ---------- */
static int sgn(int v) { return v < 0 ? -1 : v > 0 ? 1 : 0; }

TEST(gap14_strcmpw_basic) {
    REQUIRE(U(StrCmpW));
    EXPECT_EQ(U(StrCmpW)(L"abc", L"abc"), 0);
    EXPECT_EQ(U(StrCmpW)(L"", L""), 0);
    EXPECT_EQ(sgn(U(StrCmpW)(L"abc", L"abd")), -1);
    EXPECT_EQ(sgn(U(StrCmpW)(L"abd", L"abc")), 1);
    EXPECT_EQ(sgn(U(StrCmpW)(L"ab", L"abc")), -1);
    EXPECT_EQ(sgn(U(StrCmpW)(L"abc", L"ab")), 1);
    EXPECT_EQ(sgn(U(StrCmpW)(L"", L"a")), -1);
    EXPECT_EQ(sgn(U(StrCmpW)(L"a", L"")), 1);
    EXPECT_EQ(sgn(U(StrCmpW)(L"a", L"b")), -1);
    EXPECT_EQ(sgn(U(StrCmpW)(L"apple", L"banana")), -1);
    EXPECT_EQ(sgn(U(StrCmpW)(L"abc", L"ABC")) != 0, 1);   /* case sensitive */
}
TEST(gap14_strcmpw_result_range) {
    REQUIRE(U(StrCmpW));
    const wchar_t *v[] = { L"a", L"b", L"A", L"", L"abc", L"zz", L"\x00e9" };
    for (size_t i = 0; i < 7; i++) for (size_t j = 0; j < 7; j++) {
        int r = U(StrCmpW)(v[i], v[j]);
        CHECKF(r >= -1 && r <= 1, "%zu,%zu -> %d", i, j, r);
        CHECKF(sgn(r) == -sgn(U(StrCmpW)(v[j], v[i])), "antisym %zu,%zu", i, j);
        if (i == j) CHECK(r == 0);
    }
}
TEST(gap14_strcmpw_case_order_note) {
    REQUIRE(U(StrCmpW));
    t_note("StrCmpW(a,A)=%d StrCmpW(A,a)=%d", U(StrCmpW)(L"a", L"A"), U(StrCmpW)(L"A", L"a"));
    t_note("StrCmpW(a,B)=%d", U(StrCmpW)(L"a", L"B"));
}
TEST(gap14_strcmpnw_basic) {
    REQUIRE(U(StrCmpNW));
    EXPECT_EQ(U(StrCmpNW)(L"abcdef", L"abcxyz", 3), 0);
    EXPECT_EQ(sgn(U(StrCmpNW)(L"abcdef", L"abcxyz", 4)), -1);
    EXPECT_EQ(sgn(U(StrCmpNW)(L"abcxyz", L"abcdef", 4)), 1);
    EXPECT_EQ(U(StrCmpNW)(L"abc", L"abc", 10), 0);
    EXPECT_EQ(U(StrCmpNW)(L"abc", L"abc", 3), 0);
    EXPECT_EQ(U(StrCmpNW)(L"abc", L"xyz", 0), 0);
    EXPECT_EQ(sgn(U(StrCmpNW)(L"abc", L"xyz", 1)), -1);
    EXPECT_EQ(sgn(U(StrCmpNW)(L"b", L"a", 1)), 1);
    EXPECT_EQ(sgn(U(StrCmpNW)(L"ab", L"abc", 3)), -1);
    EXPECT_EQ(U(StrCmpNW)(L"", L"", 5), 0);
    EXPECT_EQ(U(StrCmpNW)(L"abc", L"abc", -1), 0);
    EXPECT_EQ(sgn(U(StrCmpNW)(L"abc", L"abd", -1)), -1);
}
TEST(gap14_strcmpnw_case_sensitive) {
    REQUIRE(U(StrCmpNW)); REQUIRE(U(StrCmpW));
    CHECK(U(StrCmpNW)(L"ABC", L"abc", 3) != 0);
    EXPECT_EQ(U(StrCmpNW)(L"ABC", L"ABC", 3), 0);
    EXPECT_EQ(U(StrCmpNW)(L"abcX", L"abcY", 3), 0);
}

/* ---------- StrIsIntlEqual ---------- */
TEST(gap14_strisintlequala_basic) {
    REQUIRE(U(StrIsIntlEqualA));
    CHECK(U(StrIsIntlEqualA)(TRUE, "abc", "abc", 3));
    CHECK(!U(StrIsIntlEqualA)(TRUE, "abc", "ABC", 3));
    CHECK(U(StrIsIntlEqualA)(FALSE, "abc", "ABC", 3));
    CHECK(U(StrIsIntlEqualA)(FALSE, "AbC", "aBc", -1));
    CHECK(U(StrIsIntlEqualA)(TRUE, "abc", "abc", -1));
    CHECK(!U(StrIsIntlEqualA)(TRUE, "abc", "abd", 3));
    CHECK(U(StrIsIntlEqualA)(TRUE, "abc", "abd", 2));
    CHECK(U(StrIsIntlEqualA)(FALSE, "abcX", "ABCY", 3));
    CHECK(!U(StrIsIntlEqualA)(FALSE, "abcX", "ABCY", 4));
    CHECK(U(StrIsIntlEqualA)(TRUE, "xyz", "abc", 0));
    CHECK(U(StrIsIntlEqualA)(TRUE, "", "", -1));
    CHECK(!U(StrIsIntlEqualA)(TRUE, "a", "", -1));
    CHECK(U(StrIsIntlEqualA)(2, "abc", "abc", 3));       /* any nonzero = case sensitive */
    CHECK(!U(StrIsIntlEqualA)(-1, "abc", "ABC", 3));
}
TEST(gap14_strisintlequalw_basic) {
    REQUIRE(U(StrIsIntlEqualW));
    CHECK(U(StrIsIntlEqualW)(TRUE, L"abc", L"abc", 3));
    CHECK(!U(StrIsIntlEqualW)(TRUE, L"abc", L"ABC", 3));
    CHECK(U(StrIsIntlEqualW)(FALSE, L"abc", L"ABC", 3));
    CHECK(U(StrIsIntlEqualW)(FALSE, L"AbC", L"aBc", -1));
    CHECK(U(StrIsIntlEqualW)(TRUE, L"abc", L"abc", -1));
    CHECK(!U(StrIsIntlEqualW)(TRUE, L"abc", L"abd", 3));
    CHECK(U(StrIsIntlEqualW)(TRUE, L"abc", L"abd", 2));
    CHECK(U(StrIsIntlEqualW)(FALSE, L"abcX", L"ABCY", 3));
    CHECK(!U(StrIsIntlEqualW)(FALSE, L"abcX", L"ABCY", 4));
    CHECK(U(StrIsIntlEqualW)(TRUE, L"xyz", L"abc", 0));
    CHECK(U(StrIsIntlEqualW)(TRUE, L"", L"", -1));
    CHECK(!U(StrIsIntlEqualW)(TRUE, L"a", L"", -1));
    CHECK(U(StrIsIntlEqualW)(FALSE, L"\x00e9", L"\x00c9", 1));
    CHECK(!U(StrIsIntlEqualW)(TRUE, L"\x00e9", L"\x00c9", 1));
    CHECK(U(StrIsIntlEqualW)(FALSE, L"\x0436", L"\x0416", -1));
}
TEST(gap14_strisintlequal_null) {
    REQUIRE(U(StrIsIntlEqualA)); REQUIRE(U(StrIsIntlEqualW));
    SET_ERR(0);
    BOOL r = U(StrIsIntlEqualW)(TRUE, NULL, L"a", -1);
    t_note("StrIsIntlEqualW(NULL,a)=%d err=%lu", (int)r, GetLastError());
    CHECK(!r);
    r = U(StrIsIntlEqualA)(TRUE, "a", NULL, -1);
    CHECK(!r);
}
