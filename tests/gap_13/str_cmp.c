#include "gap13.h"
/* StrCmp*: ordinal "C" variants, ignore-case ASCII-fold variants, sort-based variants. */
#define SGN(x) ((x) < 0 ? -1 : (x) > 0 ? 1 : 0)

/* ---------- StrCmpCA / StrCmpCW: exact byte/word difference ---------- */
TEST(gap13_strcmpca_values) {
    REQUIRE(U(StrCmpCA));
    EXPECT_EQ(U(StrCmpCA)("abc", "abc"), 0);
    EXPECT_EQ(U(StrCmpCA)("", ""), 0);
    EXPECT_EQ(U(StrCmpCA)("abc", "abd"), -1);
    EXPECT_EQ(U(StrCmpCA)("abd", "abc"), 1);
    EXPECT_EQ(U(StrCmpCA)("a", "c"), -2);
    EXPECT_EQ(U(StrCmpCA)("abc", "ab"), 'c');
    EXPECT_EQ(U(StrCmpCA)("ab", "abc"), -'c');
    EXPECT_EQ(U(StrCmpCA)("A", "a"), -32);
    EXPECT_EQ(U(StrCmpCA)("a", "A"), 32);
    EXPECT_EQ(U(StrCmpCA)("", "a"), -'a');
}
TEST(gap13_strcmpca_highbytes_unsigned) {
    REQUIRE(U(StrCmpCA));
    EXPECT_EQ(U(StrCmpCA)("\xff", "a"), 0xff - 'a');
    EXPECT_EQ(U(StrCmpCA)("a", "\xff"), 'a' - 0xff);
    EXPECT_EQ(U(StrCmpCA)("\x80", "\x7f"), 1);
}
TEST(gap13_strcmpca_sign_vs_crt) {
    REQUIRE(U(StrCmpCA));
    const char *v[] = { "", "a", "A", "ab", "abc", "abd", "b", "Zebra", "zebra", "0", "9" };
    for (int i = 0; i < 11; i++)
        for (int j = 0; j < 11; j++)
            CHECKF(SGN(U(StrCmpCA)(v[i], v[j])) == SGN(strcmp(v[i], v[j])), "%d,%d", i, j);
}
TEST(gap13_strcmpcw_values) {
    REQUIRE(U(StrCmpCW));
    EXPECT_EQ(U(StrCmpCW)(L"abc", L"abc"), 0);
    EXPECT_EQ(U(StrCmpCW)(L"", L""), 0);
    EXPECT_EQ(U(StrCmpCW)(L"abc", L"abd"), -1);
    EXPECT_EQ(U(StrCmpCW)(L"abd", L"abc"), 1);
    EXPECT_EQ(U(StrCmpCW)(L"abc", L"ab"), 'c');
    EXPECT_EQ(U(StrCmpCW)(L"ab", L"abc"), -'c');
    EXPECT_EQ(U(StrCmpCW)(L"A", L"a"), -32);
}
TEST(gap13_strcmpcw_unsigned_words) {
    REQUIRE(U(StrCmpCW));
    const WCHAR a[] = { 0xffff, 0 }, b[] = { 0x0001, 0 }, c[] = { 0x8000, 0 }, d[] = { 0x7fff, 0 };
    EXPECT_EQ(U(StrCmpCW)(a, b), 0xfffe);
    EXPECT_EQ(U(StrCmpCW)(b, a), -0xfffe);
    EXPECT_EQ(U(StrCmpCW)(c, d), 1);
    EXPECT_EQ(U(StrCmpCW)(d, c), -1);
}
TEST(gap13_strcmpcw_ordinal_not_linguistic) {
    REQUIRE(U(StrCmpCW));
    /* ordinal: uppercase sorts before lowercase, unlike linguistic compare */
    CHECK(U(StrCmpCW)(L"Z", L"a") < 0);
    CHECK(U(StrCmpCW)(L"a", L"Z") > 0);
    CHECK(U(StrCmpCW)(L"-", L"a") < 0);
}
TEST(gap13_strcmpca_ab_parity) {
    REQUIRE(U(StrCmpCA)); REQUIRE(U(StrCmpCW));
    const char *a[] = { "abc", "abd", "ABC", "" , "abcd" };
    const WCHAR *w[] = { L"abc", L"abd", L"ABC", L"", L"abcd" };
    for (int i = 0; i < 5; i++) for (int j = 0; j < 5; j++)
        CHECKF(U(StrCmpCA)(a[i], a[j]) == U(StrCmpCW)(w[i], w[j]), "%d,%d", i, j);
}

/* ---------- StrCmpICA / StrCmpICW: ASCII-only fold, exact difference ---------- */
TEST(gap13_strcmpica_values) {
    REQUIRE(U(StrCmpICA));
    EXPECT_EQ(U(StrCmpICA)("abc", "ABC"), 0);
    EXPECT_EQ(U(StrCmpICA)("ABC", "abc"), 0);
    EXPECT_EQ(U(StrCmpICA)("", ""), 0);
    EXPECT_EQ(U(StrCmpICA)("a", "B"), -1);
    EXPECT_EQ(U(StrCmpICA)("B", "a"), 1);
    EXPECT_EQ(U(StrCmpICA)("abc", "AB"), 'c');
    EXPECT_EQ(U(StrCmpICA)("AB", "abc"), -'c');
    EXPECT_EQ(U(StrCmpICA)("a", "c"), -2);
}
TEST(gap13_strcmpica_nonalpha_fold_edges) {
    REQUIRE(U(StrCmpICA));
    /* '[' (0x5b) and '_' (0x5f) are between 'Z' and 'a': not folded */
    EXPECT_EQ(U(StrCmpICA)("[", "{"), '[' - '{');
    EXPECT_EQ(U(StrCmpICA)("_", "\x7f"), '_' - 0x7f);
    EXPECT_EQ(U(StrCmpICA)("@", "`"), '@' - '`');           /* 0x40 vs 0x60 */
    EXPECT_EQ(U(StrCmpICA)("Z", "z"), 0);
    EXPECT_EQ(U(StrCmpICA)("A", "a"), 0);
    /* mixed: '_' sorts below 'a' once 'A' -> 'a', but 'B'->'b' ... */
    EXPECT_EQ(U(StrCmpICA)("_", "a"), '_' - 'a');
    EXPECT_EQ(U(StrCmpICA)("_", "A"), '_' - 'a');           /* folded to lower */
}
TEST(gap13_strcmpica_all_pairs) {
    REQUIRE(U(StrCmpICA));
    for (int c = 'A'; c <= 'Z'; c++) {
        char u[2] = { (char)c, 0 }, l[2] = { (char)(c + 32), 0 };
        CHECKF(U(StrCmpICA)(u, l) == 0, "%c", c);
        CHECKF(U(StrCmpICA)(l, u) == 0, "%c", c);
    }
}
TEST(gap13_strcmpicw_values) {
    REQUIRE(U(StrCmpICW));
    EXPECT_EQ(U(StrCmpICW)(L"abc", L"ABC"), 0);
    EXPECT_EQ(U(StrCmpICW)(L"", L""), 0);
    EXPECT_EQ(U(StrCmpICW)(L"a", L"B"), -1);
    EXPECT_EQ(U(StrCmpICW)(L"B", L"a"), 1);
    EXPECT_EQ(U(StrCmpICW)(L"abc", L"AB"), 'c');
    EXPECT_EQ(U(StrCmpICW)(L"AB", L"abc"), -'c');
    EXPECT_EQ(U(StrCmpICW)(L"_", L"A"), '_' - 'a');
}
TEST(gap13_strcmpicw_no_unicode_fold) {
    REQUIRE(U(StrCmpICW));
    /* only A-Z folded: U+00C9 vs U+00E9 differ by 0x20 */
    EXPECT_EQ(U(StrCmpICW)(L"\x00c9", L"\x00e9"), -0x20);
    EXPECT_EQ(U(StrCmpICW)(L"\x00e9", L"\x00c9"), 0x20);
    EXPECT_EQ(U(StrCmpICW)(L"\x0391", L"\x03b1"), -0x20);
}
TEST(gap13_strcmpicw_high_words) {
    REQUIRE(U(StrCmpICW));
    const WCHAR a[] = { 0xffff, 0 }, b[] = { 0x0041, 0 };
    EXPECT_EQ(U(StrCmpICW)(a, b), 0xffff - 'a');
}
TEST(gap13_strcmpic_ab_parity) {
    REQUIRE(U(StrCmpICA)); REQUIRE(U(StrCmpICW));
    const char *a[] = { "abc", "ABD", "Abc", "", "abcd", "_x", "[y" };
    const WCHAR *w[] = { L"abc", L"ABD", L"Abc", L"", L"abcd", L"_x", L"[y" };
    for (int i = 0; i < 7; i++) for (int j = 0; j < 7; j++)
        CHECKF(U(StrCmpICA)(a[i], a[j]) == U(StrCmpICW)(w[i], w[j]), "%d,%d", i, j);
}

/* ---------- StrCmpIW: linguistic, ignore case, returns -1/0/1 ---------- */
TEST(gap13_strcmpiw_basic) {
    REQUIRE(U(StrCmpIW));
    EXPECT_EQ(U(StrCmpIW)(L"abc", L"abc"), 0);
    EXPECT_EQ(U(StrCmpIW)(L"abc", L"ABC"), 0);
    EXPECT_EQ(U(StrCmpIW)(L"", L""), 0);
    EXPECT_EQ(U(StrCmpIW)(L"abc", L"abd"), -1);
    EXPECT_EQ(U(StrCmpIW)(L"abd", L"ABC"), 1);
    EXPECT_EQ(U(StrCmpIW)(L"ab", L"abc"), -1);
    EXPECT_EQ(U(StrCmpIW)(L"abc", L"ab"), 1);
    EXPECT_EQ(U(StrCmpIW)(L"", L"a"), -1);
    EXPECT_EQ(U(StrCmpIW)(L"a", L""), 1);
}
TEST(gap13_strcmpiw_range_is_sign_only) {
    REQUIRE(U(StrCmpIW));
    const WCHAR *v[] = { L"a", L"b", L"zzz", L"A", L"", L"10", L"9" };
    for (int i = 0; i < 7; i++) for (int j = 0; j < 7; j++) {
        int r = U(StrCmpIW)(v[i], v[j]);
        CHECKF(r >= -1 && r <= 1, "r=%d", r);
    }
}
TEST(gap13_strcmpiw_antisymmetric) {
    REQUIRE(U(StrCmpIW));
    const WCHAR *v[] = { L"apple", L"Banana", L"cherry", L"APPLE", L"apples", L"" };
    for (int i = 0; i < 6; i++) for (int j = 0; j < 6; j++)
        CHECKF(U(StrCmpIW)(v[i], v[j]) == -U(StrCmpIW)(v[j], v[i]), "%d,%d", i, j);
}
TEST(gap13_strcmpiw_linguistic_accent) {
    REQUIRE(U(StrCmpIW));
    /* e-acute sorts between e and f in linguistic order, not after 'z' */
    CHECK(U(StrCmpIW)(L"\x00e9", L"f") < 0);
    CHECK(U(StrCmpIW)(L"e", L"\x00e9") < 0);
    EXPECT_EQ(U(StrCmpIW)(L"\x00c9", L"\x00e9"), 0);       /* ignore case, non-ASCII */
}
TEST(gap13_strcmpiw_vs_ordinal_c) {
    REQUIRE(U(StrCmpIW)); REQUIRE(U(StrCmpCW));
    /* ordinal says 'Z' < 'a'; linguistic ignore-case says 'a' < 'Z' */
    CHECK(U(StrCmpCW)(L"Z", L"a") < 0);
    CHECK(U(StrCmpIW)(L"Z", L"a") > 0);
}
TEST(gap13_strcmpiw_no_error_set) {
    REQUIRE(U(StrCmpIW));
    SetLastError(0x1234);
    (void)U(StrCmpIW)(L"a", L"b");
    DWORD e = GetLastError();
    if (e != 0x1234) t_note("StrCmpIW changed last error to %lu", e);
}

/* ---------- StrCmpLogicalW ---------- */
TEST(gap13_strcmplogical_digits_as_numbers) {
    REQUIRE(U(StrCmpLogicalW));
    EXPECT_EQ(U(StrCmpLogicalW)(L"a2", L"a10"), -1);
    EXPECT_EQ(U(StrCmpLogicalW)(L"a10", L"a2"), 1);
    EXPECT_EQ(U(StrCmpLogicalW)(L"file9.txt", L"file10.txt"), -1);
    EXPECT_EQ(U(StrCmpLogicalW)(L"1", L"2"), -1);
    EXPECT_EQ(U(StrCmpLogicalW)(L"100", L"20"), 1);
    EXPECT_EQ(U(StrCmpLogicalW)(L"x1y", L"x1y"), 0);
}
TEST(gap13_strcmplogical_ignores_case) {
    REQUIRE(U(StrCmpLogicalW));
    EXPECT_EQ(U(StrCmpLogicalW)(L"ABC", L"abc"), 0);
    EXPECT_EQ(U(StrCmpLogicalW)(L"File2", L"file2"), 0);
    EXPECT_EQ(U(StrCmpLogicalW)(L"File2", L"file10"), -1);
}
TEST(gap13_strcmplogical_empty) {
    REQUIRE(U(StrCmpLogicalW));
    EXPECT_EQ(U(StrCmpLogicalW)(L"", L""), 0);
    EXPECT_EQ(U(StrCmpLogicalW)(L"", L"a"), -1);
    EXPECT_EQ(U(StrCmpLogicalW)(L"a", L""), 1);
}
TEST(gap13_strcmplogical_leading_zeros) {
    REQUIRE(U(StrCmpLogicalW));
    /* numeric value equal; result must be consistent in both directions */
    int a = U(StrCmpLogicalW)(L"a01", L"a1");
    int b = U(StrCmpLogicalW)(L"a1", L"a01");
    CHECK(a == -b);
    CHECK(a >= -1 && a <= 1);
    EXPECT_EQ(U(StrCmpLogicalW)(L"a001", L"a2"), -1);
}
TEST(gap13_strcmplogical_sign_only) {
    REQUIRE(U(StrCmpLogicalW));
    const WCHAR *v[] = { L"a1", L"a2", L"a10", L"b1", L"B1", L"", L"1a", L"a" };
    for (int i = 0; i < 8; i++) for (int j = 0; j < 8; j++) {
        int r = U(StrCmpLogicalW)(v[i], v[j]);
        CHECKF(r >= -1 && r <= 1, "r=%d", r);
        CHECKF(r == -U(StrCmpLogicalW)(v[j], v[i]), "asym %d,%d", i, j);
    }
}
TEST(gap13_strcmplogical_sort_order) {
    REQUIRE(U(StrCmpLogicalW));
    const WCHAR *v[] = { L"img1", L"img2", L"img9", L"img10", L"img11", L"img100" };
    for (int i = 0; i + 1 < 6; i++)
        CHECKF(U(StrCmpLogicalW)(v[i], v[i + 1]) == -1, "i=%d", i);
}
TEST(gap13_strcmplogical_long_numbers) {
    REQUIRE(U(StrCmpLogicalW));
    int a = U(StrCmpLogicalW)(L"n12345678901234567890", L"n2");
    int b = U(StrCmpLogicalW)(L"n2", L"n12345678901234567890");
    if (a != 1 || b != -1) t_note("long digit runs: %d %d", a, b);
    CHECK(a == -b);
}

/* ---------- StrCmpNA ---------- */
TEST(gap13_strcmpna_basic) {
    REQUIRE(U(StrCmpNA));
    EXPECT_EQ(U(StrCmpNA)("abcdef", "abcxyz", 3), 0);
    CHECK(U(StrCmpNA)("abcdef", "abcxyz", 4) < 0);
    CHECK(U(StrCmpNA)("abcxyz", "abcdef", 4) > 0);
    EXPECT_EQ(U(StrCmpNA)("abc", "abc", 3), 0);
    EXPECT_EQ(U(StrCmpNA)("abc", "abc", 10), 0);
    EXPECT_EQ(U(StrCmpNA)("", "", 5), 0);
    EXPECT_EQ(U(StrCmpNA)("a", "b", 0), 0);
}
TEST(gap13_strcmpna_length_diff) {
    REQUIRE(U(StrCmpNA));
    CHECK(U(StrCmpNA)("ab", "abc", 3) < 0);
    CHECK(U(StrCmpNA)("abc", "ab", 3) > 0);
    EXPECT_EQ(U(StrCmpNA)("ab", "abc", 2), 0);
    CHECK(U(StrCmpNA)("", "a", 1) < 0);
    CHECK(U(StrCmpNA)("a", "", 1) > 0);
}
TEST(gap13_strcmpna_null_and_negative) {
    REQUIRE(U(StrCmpNA));
    EXPECT_EQ(U(StrCmpNA)(NULL, "abc", 3), 0);
    EXPECT_EQ(U(StrCmpNA)("abc", NULL, 3), 0);
    EXPECT_EQ(U(StrCmpNA)(NULL, NULL, 3), 0);
    EXPECT_EQ(U(StrCmpNA)("abc", "xyz", -1), 0);        /* end pointer < start: no compare */
    EXPECT_EQ(U(StrCmpNA)("abc", "xyz", 0), 0);
}
TEST(gap13_strcmpna_sign_only_range) {
    REQUIRE(U(StrCmpNA));
    int r = U(StrCmpNA)("abc", "abd", 3);
    CHECK(r == -1);
    r = U(StrCmpNA)("abd", "abc", 3);
    CHECK(r == 1);
}
TEST(gap13_strcmpna_case) {
    REQUIRE(U(StrCmpNA));
    /* differing bytes are resolved by lstrcmp (linguistic, case-sensitive) */
    int r = U(StrCmpNA)("ABC", "abc", 3);
    if (r == 0) t_note("StrCmpNA treats case as equal");
    CHECK(U(StrCmpNA)("abc", "abc", 3) == 0);
}

/* ---------- StrCmpNCA / StrCmpNCW ---------- */
TEST(gap13_strcmpnca_values) {
    REQUIRE(U(StrCmpNCA));
    EXPECT_EQ(U(StrCmpNCA)("abcdef", "abcxyz", 3), 0);
    EXPECT_EQ(U(StrCmpNCA)("abcdef", "abcxyz", 4), 'd' - 'x');
    EXPECT_EQ(U(StrCmpNCA)("abc", "abc", 10), 0);
    EXPECT_EQ(U(StrCmpNCA)("a", "b", 0), 0);
    EXPECT_EQ(U(StrCmpNCA)("", "", 1), 0);
    EXPECT_EQ(U(StrCmpNCA)("A", "a", 1), -32);
    EXPECT_EQ(U(StrCmpNCA)("ab", "abc", 3), -'c');
    EXPECT_EQ(U(StrCmpNCA)("abc", "ab", 3), 'c');
    EXPECT_EQ(U(StrCmpNCA)("abc", "ab", 2), 0);
    EXPECT_EQ(U(StrCmpNCA)("abc", "abd", 1), 0);
}
TEST(gap13_strcmpnca_zero_no_deref) {
    REQUIRE(U(StrCmpNCA));
    EXPECT_EQ(U(StrCmpNCA)(NULL, NULL, 0), 0);
    EXPECT_EQ(U(StrCmpNCA)(NULL, "a", 0), 0);
}
TEST(gap13_strcmpnca_negative_is_unbounded) {
    REQUIRE(U(StrCmpNCA));
    EXPECT_EQ(U(StrCmpNCA)("abc", "abc", -1), 0);
    EXPECT_EQ(U(StrCmpNCA)("abcd", "abce", -1), 'd' - 'e');
    EXPECT_EQ(U(StrCmpNCA)("ab", "abc", -5), -'c');
}
TEST(gap13_strcmpnca_highbytes) {
    REQUIRE(U(StrCmpNCA));
    EXPECT_EQ(U(StrCmpNCA)("\xff", "a", 1), 0xff - 'a');
    EXPECT_EQ(U(StrCmpNCA)("a", "\xff", 1), 'a' - 0xff);
}
TEST(gap13_strcmpncw_values) {
    REQUIRE(U(StrCmpNCW));
    EXPECT_EQ(U(StrCmpNCW)(L"abcdef", L"abcxyz", 3), 0);
    EXPECT_EQ(U(StrCmpNCW)(L"abcdef", L"abcxyz", 4), 'd' - 'x');
    EXPECT_EQ(U(StrCmpNCW)(L"abc", L"abc", 10), 0);
    EXPECT_EQ(U(StrCmpNCW)(L"a", L"b", 0), 0);
    EXPECT_EQ(U(StrCmpNCW)(L"A", L"a", 1), -32);
    EXPECT_EQ(U(StrCmpNCW)(L"ab", L"abc", 3), -'c');
    EXPECT_EQ(U(StrCmpNCW)(L"abc", L"ab", 2), 0);
    EXPECT_EQ(U(StrCmpNCW)(NULL, NULL, 0), 0);
}
TEST(gap13_strcmpncw_negative_and_high) {
    REQUIRE(U(StrCmpNCW));
    EXPECT_EQ(U(StrCmpNCW)(L"abc", L"abc", -1), 0);
    EXPECT_EQ(U(StrCmpNCW)(L"abcd", L"abce", -1), 'd' - 'e');
    const WCHAR a[] = { 0xffff, 0 }, b[] = { 0x1, 0 };
    EXPECT_EQ(U(StrCmpNCW)(a, b, 1), 0xfffe);
}
TEST(gap13_strcmpnc_ab_parity) {
    REQUIRE(U(StrCmpNCA)); REQUIRE(U(StrCmpNCW));
    for (int n = 0; n <= 6; n++) {
        EXPECT_EQ(U(StrCmpNCA)("abcdef", "abcxyz", n), U(StrCmpNCW)(L"abcdef", L"abcxyz", n));
        EXPECT_EQ(U(StrCmpNCA)("Hello", "help", n), U(StrCmpNCW)(L"Hello", L"help", n));
    }
}

/* ---------- StrCmpNIA ---------- */
TEST(gap13_strcmpnia_basic) {
    REQUIRE(U(StrCmpNIA));
    EXPECT_EQ(U(StrCmpNIA)("ABCdef", "abcXYZ", 3), 0);
    EXPECT_EQ(U(StrCmpNIA)("ABCdef", "abcXYZ", 4), -1);
    EXPECT_EQ(U(StrCmpNIA)("abcxyz", "ABCdef", 4), 1);
    EXPECT_EQ(U(StrCmpNIA)("abc", "ABC", 10), 0);
    EXPECT_EQ(U(StrCmpNIA)("", "", 4), 0);
    EXPECT_EQ(U(StrCmpNIA)("ab", "abc", 3), -1);
    EXPECT_EQ(U(StrCmpNIA)("abc", "ab", 3), 1);
    EXPECT_EQ(U(StrCmpNIA)("ab", "abc", 2), 0);
}
TEST(gap13_strcmpnia_zero) {
    REQUIRE(U(StrCmpNIA));
    EXPECT_EQ(U(StrCmpNIA)("a", "b", 0), 0);
    int r = U(StrCmpNIA)(NULL, NULL, 0);
    if (r != 0) t_note("StrCmpNIA(NULL,NULL,0) = %d", r);
}
TEST(gap13_strcmpnia_negative) {
    REQUIRE(U(StrCmpNIA));
    int r = U(StrCmpNIA)("abc", "xyz", -1);
    t_note("StrCmpNIA negative count returns %d", r);
    CHECK(r >= -1 && r <= 1);
}
TEST(gap13_strcmpnia_range) {
    REQUIRE(U(StrCmpNIA));
    const char *v[] = { "a", "B", "c", "A", "ab", "" };
    for (int i = 0; i < 6; i++) for (int j = 0; j < 6; j++) {
        int r = U(StrCmpNIA)(v[i], v[j], 5);
        CHECKF(r >= -1 && r <= 1, "r=%d", r);
        CHECKF(r == -U(StrCmpNIA)(v[j], v[i], 5), "asym %d,%d", i, j);
    }
}

/* ---------- StrCmpNIW ---------- */
TEST(gap13_strcmpniw_basic) {
    REQUIRE(U(StrCmpNIW));
    EXPECT_EQ(U(StrCmpNIW)(L"ABCdef", L"abcXYZ", 3), 0);
    EXPECT_EQ(U(StrCmpNIW)(L"ABCdef", L"abcXYZ", 4), -1);
    EXPECT_EQ(U(StrCmpNIW)(L"abcxyz", L"ABCdef", 4), 1);
    EXPECT_EQ(U(StrCmpNIW)(L"abc", L"ABC", 3), 0);
    EXPECT_EQ(U(StrCmpNIW)(L"a", L"b", 1), -1);
    EXPECT_EQ(U(StrCmpNIW)(L"b", L"a", 1), 1);
    EXPECT_EQ(U(StrCmpNIW)(L"abc", L"abd", 2), 0);
}
TEST(gap13_strcmpniw_zero_count) {
    REQUIRE(U(StrCmpNIW));
    int r = U(StrCmpNIW)(L"a", L"b", 0);
    t_note("StrCmpNIW count 0 returns %d", r);
    CHECK(r >= -1 && r <= 1);
}
TEST(gap13_strcmpniw_shorter_string) {
    REQUIRE(U(StrCmpNIW));
    EXPECT_EQ(U(StrCmpNIW)(L"ab", L"abc", 3), -1);
    EXPECT_EQ(U(StrCmpNIW)(L"abc", L"ab", 3), 1);
    EXPECT_EQ(U(StrCmpNIW)(L"ab", L"abc", 2), 0);
}
TEST(gap13_strcmpniw_range) {
    REQUIRE(U(StrCmpNIW));
    const WCHAR *v[] = { L"a", L"B", L"c", L"A", L"ab", L"x1" };
    for (int i = 0; i < 6; i++) for (int j = 0; j < 6; j++) {
        int r = U(StrCmpNIW)(v[i], v[j], 4);
        CHECKF(r >= -1 && r <= 1, "r=%d", r);
        CHECKF(r == -U(StrCmpNIW)(v[j], v[i], 4), "asym %d,%d", i, j);
    }
}
TEST(gap13_strcmpni_ab_parity) {
    REQUIRE(U(StrCmpNIA)); REQUIRE(U(StrCmpNIW));
    const char *a[] = { "Alpha", "alpHA", "Beta", "alp" };
    const WCHAR *w[] = { L"Alpha", L"alpHA", L"Beta", L"alp" };
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) for (int n = 1; n <= 3; n++)
        CHECKF(U(StrCmpNIA)(a[i], a[j], n) == U(StrCmpNIW)(w[i], w[j], n), "%d,%d,%d", i, j, n);
}

/* ---------- StrCmpNICA / StrCmpNICW ---------- */
TEST(gap13_strcmpnica_values) {
    REQUIRE(U(StrCmpNICA));
    EXPECT_EQ(U(StrCmpNICA)("ABCdef", "abcXYZ", 3), 0);
    EXPECT_EQ(U(StrCmpNICA)("ABCdef", "abcXYZ", 4), 'd' - 'x');
    EXPECT_EQ(U(StrCmpNICA)("abc", "ABC", 10), 0);
    EXPECT_EQ(U(StrCmpNICA)("a", "B", 1), -1);
    EXPECT_EQ(U(StrCmpNICA)("a", "b", 0), 0);
    EXPECT_EQ(U(StrCmpNICA)("ab", "abc", 3), -'c');
    EXPECT_EQ(U(StrCmpNICA)("abc", "ab", 2), 0);
    EXPECT_EQ(U(StrCmpNICA)("_", "A", 1), '_' - 'a');
}
TEST(gap13_strcmpnica_negative) {
    REQUIRE(U(StrCmpNICA));
    EXPECT_EQ(U(StrCmpNICA)("ABC", "abc", -1), 0);
    EXPECT_EQ(U(StrCmpNICA)("ABD", "abc", -1), 'd' - 'c');
}
TEST(gap13_strcmpnicw_values) {
    REQUIRE(U(StrCmpNICW));
    EXPECT_EQ(U(StrCmpNICW)(L"ABCdef", L"abcXYZ", 3), 0);
    EXPECT_EQ(U(StrCmpNICW)(L"ABCdef", L"abcXYZ", 4), 'd' - 'x');
    EXPECT_EQ(U(StrCmpNICW)(L"abc", L"ABC", 10), 0);
    EXPECT_EQ(U(StrCmpNICW)(L"a", L"B", 1), -1);
    EXPECT_EQ(U(StrCmpNICW)(L"a", L"b", 0), 0);
    EXPECT_EQ(U(StrCmpNICW)(L"ab", L"abc", 3), -'c');
    EXPECT_EQ(U(StrCmpNICW)(L"abc", L"ab", 2), 0);
    EXPECT_EQ(U(StrCmpNICW)(L"\x00c9", L"\x00e9", 1), -0x20);   /* no non-ASCII fold */
}
TEST(gap13_strcmpnicw_negative_and_high) {
    REQUIRE(U(StrCmpNICW));
    EXPECT_EQ(U(StrCmpNICW)(L"ABC", L"abc", -1), 0);
    const WCHAR a[] = { 0xffff, 0 }, b[] = { 0x41, 0 };
    EXPECT_EQ(U(StrCmpNICW)(a, b, 1), 0xffff - 'a');
}
TEST(gap13_strcmpnic_ab_parity) {
    REQUIRE(U(StrCmpNICA)); REQUIRE(U(StrCmpNICW));
    const char *a[] = { "Alpha", "alpHA", "Beta", "alp", "_x" };
    const WCHAR *w[] = { L"Alpha", L"alpHA", L"Beta", L"alp", L"_x" };
    for (int i = 0; i < 5; i++) for (int j = 0; j < 5; j++) for (int n = 0; n <= 6; n++)
        CHECKF(U(StrCmpNICA)(a[i], a[j], n) == U(StrCmpNICW)(w[i], w[j], n), "%d,%d,%d", i, j, n);
}
TEST(gap13_str_family_cross_consistency) {
    REQUIRE(U(StrCmpNCA)); REQUIRE(U(StrCmpNICA)); REQUIRE(U(StrCmpCA)); REQUIRE(U(StrCmpICA));
    /* with a large count the N variants equal the unbounded ones */
    const char *v[] = { "abc", "ABD", "ab", "", "zz" };
    for (int i = 0; i < 5; i++) for (int j = 0; j < 5; j++) {
        CHECKF(U(StrCmpNCA)(v[i], v[j], 100) == U(StrCmpCA)(v[i], v[j]), "nc %d,%d", i, j);
        CHECKF(U(StrCmpNICA)(v[i], v[j], 100) == U(StrCmpICA)(v[i], v[j]), "nic %d,%d", i, j);
    }
}
