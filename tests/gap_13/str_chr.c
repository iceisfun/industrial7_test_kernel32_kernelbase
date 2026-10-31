#include "gap13.h"
/* StrChr*, StrCSpn*: shlwapi-style string search forwarded from kernelbase. */
#define GAP_SGN(x) ((x) < 0 ? -1 : (x) > 0 ? 1 : 0)

TEST(gap13_strchra_basic) {
    REQUIRE(U(StrChrA));
    const char *s = "Hello, World";
    EXPECT_EQ(U(StrChrA)(s, 'H'), s);
    EXPECT_EQ(U(StrChrA)(s, 'o'), s + 4);
    EXPECT_EQ(U(StrChrA)(s, 'd'), s + 11);
    EXPECT_EQ(U(StrChrA)(s, 'h'), NULL);            /* case sensitive */
    EXPECT_EQ(U(StrChrA)(s, 'z'), NULL);
    EXPECT_EQ(U(StrChrA)(s, ','), s + 5);
    EXPECT_EQ(U(StrChrA)(s, ' '), s + 6);
}
TEST(gap13_strchra_edges) {
    REQUIRE(U(StrChrA));
    EXPECT_EQ(U(StrChrA)(NULL, 'a'), NULL);
    EXPECT_EQ(U(StrChrA)("", 'a'), NULL);
    const char *s = "abc";
    EXPECT_EQ(U(StrChrA)(s, 0), NULL);              /* terminator is never matched */
    EXPECT_EQ(U(StrChrA)("", 0), NULL);
    char one[2] = { 'x', 0 };
    EXPECT_EQ(U(StrChrA)(one, 'x'), one);
    /* first of repeated chars */
    const char *r = "aXbXc";
    EXPECT_EQ(U(StrChrA)(r, 'X'), r + 1);
}
TEST(gap13_strchra_no_side_effects) {
    REQUIRE(U(StrChrA));
    char buf[] = "abcabc";
    char *p = U(StrChrA)(buf, 'c');
    CHECK(p == buf + 2);
    EXPECT_STR(buf, "abcabc");
}
TEST(gap13_strchra_every_ascii) {
    REQUIRE(U(StrChrA));
    char s[128];
    for (int i = 1; i < 128; i++) s[i - 1] = (char)i;
    s[127] = 0;
    for (int i = 1; i < 128; i++) {
        char *p = U(StrChrA)(s, (WORD)i);
        CHECKF(p == s + (i - 1), "ch %d", i);
    }
}
TEST(gap13_strchia_basic) {
    REQUIRE(U(StrChrIA));
    const char *s = "Hello, World";
    EXPECT_EQ(U(StrChrIA)(s, 'h'), s);
    EXPECT_EQ(U(StrChrIA)(s, 'H'), s);
    EXPECT_EQ(U(StrChrIA)(s, 'O'), s + 4);
    EXPECT_EQ(U(StrChrIA)(s, 'w'), s + 7);
    EXPECT_EQ(U(StrChrIA)(s, 'D'), s + 11);
    EXPECT_EQ(U(StrChrIA)(s, 'z'), NULL);
    EXPECT_EQ(U(StrChrIA)(s, ','), s + 5);
}
TEST(gap13_strchia_edges) {
    REQUIRE(U(StrChrIA));
    EXPECT_EQ(U(StrChrIA)(NULL, 'a'), NULL);
    EXPECT_EQ(U(StrChrIA)("", 'a'), NULL);
    EXPECT_EQ(U(StrChrIA)("abc", 0), NULL);
}
TEST(gap13_strchia_alpha_all) {
    REQUIRE(U(StrChrIA));
    const char *s = "abcdefghijklmnopqrstuvwxyz";
    for (int i = 0; i < 26; i++) {
        CHECKF(U(StrChrIA)(s, (WORD)('A' + i)) == s + i, "upper %d", i);
        CHECKF(U(StrChrIA)(s, (WORD)('a' + i)) == s + i, "lower %d", i);
    }
    const char *u = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    for (int i = 0; i < 26; i++)
        CHECKF(U(StrChrIA)(u, (WORD)('a' + i)) == u + i, "u2l %d", i);
}
TEST(gap13_strchrw_basic) {
    REQUIRE(U(StrChrW));
    const WCHAR *s = L"Hello, World";
    EXPECT_EQ(U(StrChrW)(s, L'H'), s);
    EXPECT_EQ(U(StrChrW)(s, L'o'), s + 4);
    EXPECT_EQ(U(StrChrW)(s, L'd'), s + 11);
    EXPECT_EQ(U(StrChrW)(s, L'h'), NULL);
    EXPECT_EQ(U(StrChrW)(s, L'W'), s + 7);
}
TEST(gap13_strchrw_edges) {
    REQUIRE(U(StrChrW));
    EXPECT_EQ(U(StrChrW)(NULL, L'a'), NULL);
    EXPECT_EQ(U(StrChrW)(L"", L'a'), NULL);
    EXPECT_EQ(U(StrChrW)(L"abc", 0), NULL);
    EXPECT_EQ(U(StrChrW)(L"", 0), NULL);
}
TEST(gap13_strchrw_unicode) {
    REQUIRE(U(StrChrW));
    const WCHAR s[] = { 0x00e9, 0x4e2d, 0xd83d, 0xde00, 0xffff, 0 };
    for (int i = 0; i < 5; i++)
        CHECKF(U(StrChrW)(s, s[i]) == s + i, "i=%d", i);
    EXPECT_EQ(U(StrChrW)(s, 0x00c9), NULL);          /* exact match, no folding */
    EXPECT_EQ(U(StrChrW)(s, 0x0100), NULL);
}
TEST(gap13_strchrw_high_bits_distinct) {
    REQUIRE(U(StrChrW));
    /* a match value whose low byte equals a char must not match */
    const WCHAR s[] = { L'a', L'b', 0 };
    EXPECT_EQ(U(StrChrW)(s, 0x0161), NULL);
    EXPECT_EQ(U(StrChrW)(s, 0x0162), NULL);
}
TEST(gap13_strchriw_basic) {
    REQUIRE(U(StrChrIW));
    const WCHAR *s = L"Hello, World";
    EXPECT_EQ(U(StrChrIW)(s, L'h'), s);
    EXPECT_EQ(U(StrChrIW)(s, L'O'), s + 4);
    EXPECT_EQ(U(StrChrIW)(s, L'D'), s + 11);
    EXPECT_EQ(U(StrChrIW)(s, L'z'), NULL);
    EXPECT_EQ(U(StrChrIW)(NULL, L'a'), NULL);
    EXPECT_EQ(U(StrChrIW)(L"", L'a'), NULL);
}
TEST(gap13_strchriw_alpha_all) {
    REQUIRE(U(StrChrIW));
    const WCHAR *s = L"abcdefghijklmnopqrstuvwxyz";
    for (int i = 0; i < 26; i++) {
        CHECKF(U(StrChrIW)(s, (WCHAR)(L'A' + i)) == s + i, "upper %d", i);
        CHECKF(U(StrChrIW)(s, (WCHAR)(L'a' + i)) == s + i, "lower %d", i);
    }
}
TEST(gap13_strchriw_nonascii_fold) {
    REQUIRE(U(StrChrIW));
    /* ChrCmpIW goes through the invariant-ish sort compare with ignore-case */
    const WCHAR s[] = { 0x00e9, L'x', 0 };
    WCHAR *p = U(StrChrIW)(s, 0x00c9);
    if (p != s) t_note("StrChrIW does not fold U+00C9/U+00E9 (got %p)", (void *)p);
    CHECK(U(StrChrIW)(s, L'x') == s + 1);
}
TEST(gap13_strchriw_terminator) {
    REQUIRE(U(StrChrIW));
    EXPECT_EQ(U(StrChrIW)(L"abc", 0), NULL);
}
TEST(gap13_strchrnw_basic) {
    REQUIRE(U(StrChrNW));
    const WCHAR *s = L"abcdef";
    EXPECT_EQ(U(StrChrNW)(s, L'a', 6), s);
    EXPECT_EQ(U(StrChrNW)(s, L'f', 6), s + 5);
    EXPECT_EQ(U(StrChrNW)(s, L'f', 5), NULL);
    EXPECT_EQ(U(StrChrNW)(s, L'e', 5), s + 4);
    EXPECT_EQ(U(StrChrNW)(s, L'e', 4), NULL);
    EXPECT_EQ(U(StrChrNW)(s, L'a', 1), s);
    EXPECT_EQ(U(StrChrNW)(s, L'b', 1), NULL);
}
TEST(gap13_strchrnw_zero_and_null) {
    REQUIRE(U(StrChrNW));
    EXPECT_EQ(U(StrChrNW)(L"abc", L'a', 0), NULL);
    EXPECT_EQ(U(StrChrNW)(NULL, L'a', 5), NULL);
    EXPECT_EQ(U(StrChrNW)(NULL, L'a', 0), NULL);
    EXPECT_EQ(U(StrChrNW)(L"", L'a', 5), NULL);
}
TEST(gap13_strchrnw_stops_at_nul) {
    REQUIRE(U(StrChrNW));
    /* count larger than string: stop at terminator, do not read past it */
    const WCHAR buf[] = { L'a', L'b', 0, L'c', 0 };
    EXPECT_EQ(U(StrChrNW)(buf, L'c', 5), NULL);
    EXPECT_EQ(U(StrChrNW)(buf, L'b', 100), buf + 1);
    EXPECT_EQ(U(StrChrNW)(buf, 0, 5), NULL);
}
TEST(gap13_strchrnw_case_sensitive) {
    REQUIRE(U(StrChrNW));
    EXPECT_EQ(U(StrChrNW)(L"ABC", L'a', 3), NULL);
    { const WCHAR *u = L"ABC"; EXPECT_EQ(U(StrChrNW)(u, L'A', 3), u); }
}
TEST(gap13_strchrnw_unbounded_huge) {
    REQUIRE(U(StrChrNW));
    const WCHAR *s = L"xyz";
    EXPECT_EQ(U(StrChrNW)(s, L'z', 0x7fffffff), s + 2);
}
TEST(gap13_strchrniw_basic) {
    REQUIRE(U(StrChrNIW));
    const WCHAR *s = L"AbCdEf";
    EXPECT_EQ(U(StrChrNIW)(s, L'a', 6), s);
    EXPECT_EQ(U(StrChrNIW)(s, L'F', 6), s + 5);
    EXPECT_EQ(U(StrChrNIW)(s, L'f', 5), NULL);
    EXPECT_EQ(U(StrChrNIW)(s, L'E', 5), s + 4);
    EXPECT_EQ(U(StrChrNIW)(s, L'e', 4), NULL);
    EXPECT_EQ(U(StrChrNIW)(s, L'c', 3), s + 2);
    EXPECT_EQ(U(StrChrNIW)(s, L'c', 2), NULL);
}
TEST(gap13_strchrniw_zero_and_null) {
    REQUIRE(U(StrChrNIW));
    EXPECT_EQ(U(StrChrNIW)(L"abc", L'a', 0), NULL);
    EXPECT_EQ(U(StrChrNIW)(NULL, L'a', 3), NULL);
    EXPECT_EQ(U(StrChrNIW)(L"", L'a', 3), NULL);
    EXPECT_EQ(U(StrChrNIW)(L"abc", 0, 10), NULL);
}
TEST(gap13_strchrniw_stops_at_nul) {
    REQUIRE(U(StrChrNIW));
    const WCHAR buf[] = { L'a', 0, L'B', 0 };
    EXPECT_EQ(U(StrChrNIW)(buf, L'b', 4), NULL);
    EXPECT_EQ(U(StrChrNIW)(buf, L'A', 4), buf);
}
TEST(gap13_strcspna_basic) {
    REQUIRE(U(StrCSpnA));
    EXPECT_EQ(U(StrCSpnA)("hello world", " "), 5);
    EXPECT_EQ(U(StrCSpnA)("hello world", "xyz"), 11);
    EXPECT_EQ(U(StrCSpnA)("hello world", "h"), 0);
    EXPECT_EQ(U(StrCSpnA)("hello world", "od"), 4);
    EXPECT_EQ(U(StrCSpnA)("hello world", "H"), 11);       /* case sensitive */
    EXPECT_EQ(U(StrCSpnA)("abc", "cba"), 0);
    EXPECT_EQ(U(StrCSpnA)("abc", "c"), 2);
}
TEST(gap13_strcspna_empty_null) {
    REQUIRE(U(StrCSpnA));
    EXPECT_EQ(U(StrCSpnA)("", "abc"), 0);
    EXPECT_EQ(U(StrCSpnA)("abc", ""), 3);
    EXPECT_EQ(U(StrCSpnA)("", ""), 0);
    EXPECT_EQ(U(StrCSpnA)(NULL, "abc"), 0);
    EXPECT_EQ(U(StrCSpnA)("abc", NULL), 0);
    EXPECT_EQ(U(StrCSpnA)(NULL, NULL), 0);
}
TEST(gap13_strcspna_matches_crt_ascii) {
    REQUIRE(U(StrCSpnA));
    const char *strs[] = { "abcdef", "a,b;c", "   x", "path\\to\\file.txt", "zzz" };
    const char *sets[] = { ",;", "\\.", " ", "xyz", "q" };
    for (int i = 0; i < 5; i++)
        for (int j = 0; j < 5; j++)
            CHECKF(U(StrCSpnA)(strs[i], sets[j]) == (int)strcspn(strs[i], sets[j]), "%d,%d", i, j);
}
TEST(gap13_strcspnia_basic) {
    REQUIRE(U(StrCSpnIA));
    EXPECT_EQ(U(StrCSpnIA)("hello world", "H"), 0);
    EXPECT_EQ(U(StrCSpnIA)("hello world", "W"), 6);
    EXPECT_EQ(U(StrCSpnIA)("HELLO WORLD", "o"), 4);
    EXPECT_EQ(U(StrCSpnIA)("hello", "xyz"), 5);
    EXPECT_EQ(U(StrCSpnIA)("", "a"), 0);
    EXPECT_EQ(U(StrCSpnIA)("abc", ""), 3);
    EXPECT_EQ(U(StrCSpnIA)(NULL, "a"), 0);
    EXPECT_EQ(U(StrCSpnIA)("abc", NULL), 0);
}
TEST(gap13_strcspnw_basic) {
    REQUIRE(U(StrCSpnW));
    EXPECT_EQ(U(StrCSpnW)(L"hello world", L" "), 5);
    EXPECT_EQ(U(StrCSpnW)(L"hello world", L"xyz"), 11);
    EXPECT_EQ(U(StrCSpnW)(L"hello world", L"h"), 0);
    EXPECT_EQ(U(StrCSpnW)(L"hello world", L"H"), 11);
    EXPECT_EQ(U(StrCSpnW)(L"hello world", L"od"), 4);
    EXPECT_EQ(U(StrCSpnW)(L"", L"abc"), 0);
    EXPECT_EQ(U(StrCSpnW)(L"abc", L""), 3);
    EXPECT_EQ(U(StrCSpnW)(NULL, L"abc"), 0);
    EXPECT_EQ(U(StrCSpnW)(L"abc", NULL), 0);
    EXPECT_EQ(U(StrCSpnW)(NULL, NULL), 0);
}
TEST(gap13_strcspnw_unicode) {
    REQUIRE(U(StrCSpnW));
    const WCHAR s[] = { 0x4e2d, 0x6587, L'a', 0x00e9, 0 };
    const WCHAR set1[] = { 0x00e9, 0 };
    const WCHAR set2[] = { L'a', 0 };
    const WCHAR set3[] = { 0x6587, 0 };
    EXPECT_EQ(U(StrCSpnW)(s, set1), 3);
    EXPECT_EQ(U(StrCSpnW)(s, set2), 2);
    EXPECT_EQ(U(StrCSpnW)(s, set3), 1);
}
TEST(gap13_strcspnw_matches_wcscspn) {
    REQUIRE(U(StrCSpnW));
    const WCHAR *strs[] = { L"abcdef", L"a,b;c", L"   x", L"path\\to\\file.txt" };
    const WCHAR *sets[] = { L",;", L"\\.", L" ", L"xyz" };
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            int exp = 0;
            while (strs[i][exp] && !wcschr(sets[j], strs[i][exp])) exp++;
            CHECKF(U(StrCSpnW)(strs[i], sets[j]) == exp, "%d,%d", i, j);
        }
}
TEST(gap13_strcspniw_basic) {
    REQUIRE(U(StrCSpnIW));
    EXPECT_EQ(U(StrCSpnIW)(L"hello world", L"H"), 0);
    EXPECT_EQ(U(StrCSpnIW)(L"hello world", L"W"), 6);
    EXPECT_EQ(U(StrCSpnIW)(L"HELLO", L"xyz"), 5);
    EXPECT_EQ(U(StrCSpnIW)(L"HELLO", L"l"), 2);
    EXPECT_EQ(U(StrCSpnIW)(L"", L"a"), 0);
    EXPECT_EQ(U(StrCSpnIW)(L"abc", L""), 3);
    EXPECT_EQ(U(StrCSpnIW)(NULL, L"a"), 0);
    EXPECT_EQ(U(StrCSpnIW)(L"abc", NULL), 0);
}
TEST(gap13_strcspn_ab_parity) {
    REQUIRE(U(StrCSpnA)); REQUIRE(U(StrCSpnW)); REQUIRE(U(StrCSpnIA)); REQUIRE(U(StrCSpnIW));
    const char *as = "Alpha.Beta-Gamma"; const WCHAR *ws = L"Alpha.Beta-Gamma";
    const char *aset = ".-"; const WCHAR *wset = L".-";
    EXPECT_EQ(U(StrCSpnA)(as, aset), U(StrCSpnW)(ws, wset));
    EXPECT_EQ(U(StrCSpnIA)(as, "b"), U(StrCSpnIW)(ws, L"b"));
    EXPECT_EQ(U(StrCSpnIA)(as, "G"), U(StrCSpnIW)(ws, L"G"));
}
