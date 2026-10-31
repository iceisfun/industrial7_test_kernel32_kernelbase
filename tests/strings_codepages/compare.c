/* CompareStringA/W/Ex/Ordinal, FindNLSString(Ex), FindStringOrdinal */
#include "sc.h"
#include <limits.h>

#define LT 1
#define EQ 2
#define GT 3
#define INV L""
#define ENUS L"en-US"

typedef struct { const WCHAR *loc; DWORD fl; const WCHAR *a, *b; int r; } cmpv_t;

static void run_cmpex(const cmpv_t *v, int n, const char *tag) {
    if (!U(CompareStringEx)) { t_skip("missing export: CompareStringEx"); return; }
    for (int i = 0; i < n; i++) {
        SET_ERR(0);
        int r = U(CompareStringEx)(v[i].loc, v[i].fl, v[i].a, -1, v[i].b, -1, NULL, NULL, 0);
        CHECKF(r == v[i].r, "%s vec %d fl=%lx got %d want %d (err %lu)", tag, i, v[i].fl, r, v[i].r, GetLastError());
        if (r != v[i].r) continue;
        /* antisymmetry */
        int rev = U(CompareStringEx)(v[i].loc, v[i].fl, v[i].b, -1, v[i].a, -1, NULL, NULL, 0);
        int wantrev = v[i].r == LT ? GT : v[i].r == GT ? LT : v[i].r;
        CHECKF(rev == wantrev, "%s vec %d reversed got %d want %d", tag, i, rev, wantrev);
        /* explicit lengths give the same answer */
        int la = lstrlenW(v[i].a), lb = lstrlenW(v[i].b);
        int e = U(CompareStringEx)(v[i].loc, v[i].fl, v[i].a, la, v[i].b, lb, NULL, NULL, 0);
        CHECKF(e == v[i].r, "%s vec %d explicit len got %d", tag, i, e);
        /* the LCID entry point agrees for the invariant / en-US locale */
        if (U(CompareStringW)) {
            LCID lc = v[i].loc[0] == 0 ? LOCALE_INVARIANT : 0x0409;
            if (v[i].loc[0] == 0 || !lstrcmpW(v[i].loc, L"en-US")) {
                int w = U(CompareStringW)(lc, v[i].fl, v[i].a, -1, v[i].b, -1);
                CHECKF(w == v[i].r, "%s vec %d CompareStringW got %d", tag, i, w);
            }
        }
    }
}
#define CMPTEST(name, ...) \
    static const cmpv_t name##_v[] = { __VA_ARGS__ }; \
    TEST(cmp_##name) { run_cmpex(name##_v, ARRAY_N(name##_v), #name); }

/* ---- basic ordering ---- */
CMPTEST(basic_invariant,
    {INV, 0, L"", L"", EQ}, {INV, 0, L"a", L"a", EQ}, {INV, 0, L"abc", L"abc", EQ}, {INV, 0, L"a", L"b", LT}, {INV, 0, L"b", L"a", GT},
    {INV, 0, L"abc", L"abd", LT}, {INV, 0, L"ab", L"abc", LT}, {INV, 0, L"abc", L"ab", GT}, {INV, 0, L"", L"a", LT}, {INV, 0, L"a", L"", GT},
    {INV, 0, L"a", L"A", LT}, {INV, 0, L"A", L"a", GT}, {INV, 0, L"a", L"B", LT}, {INV, 0, L"A", L"b", LT}, {INV, 0, L"B", L"a", GT},
    {INV, 0, L"apple", L"Apple", LT}, {INV, 0, L"Apple", L"apple", GT}, {INV, 0, L"apple", L"apples", LT}, {INV, 0, L"1", L"2", LT}, {INV, 0, L"10", L"9", LT},
    {INV, 0, L"9", L"a", LT}, {INV, 0, L"z", L"zz", LT}, {INV, 0, L"abc", L"ABC", LT}, {INV, 0, L"ABC", L"abd", LT}, {INV, 0, L"Zebra", L"apple", GT},
    {INV, 0, L"hello", L"hello", EQ}, {INV, 0, L"hello world", L"hello world", EQ}, {INV, 0, L"hello world", L"hello  world", GT}, {INV, 0, L"a b", L"ab", LT},
    {INV, 0, L"x", L"y", LT}, {INV, 0, L"mm", L"mn", LT}, {INV, 0, L"The", L"the", GT}, {INV, 0, L"the", L"The", LT})
CMPTEST(basic_enus,
    {ENUS, 0, L"", L"", EQ}, {ENUS, 0, L"a", L"a", EQ}, {ENUS, 0, L"a", L"b", LT}, {ENUS, 0, L"b", L"a", GT}, {ENUS, 0, L"a", L"A", LT}, {ENUS, 0, L"A", L"a", GT},
    {ENUS, 0, L"abc", L"abd", LT}, {ENUS, 0, L"ab", L"abc", LT}, {ENUS, 0, L"apple", L"banana", LT}, {ENUS, 0, L"Banana", L"apple", GT}, {ENUS, 0, L"z", L"Z", LT},
    {ENUS, NORM_IGNORECASE, L"a", L"A", EQ}, {ENUS, NORM_IGNORECASE, L"abc", L"ABC", EQ}, {ENUS, NORM_IGNORECASE, L"aBc", L"AbC", EQ}, {ENUS, NORM_IGNORECASE, L"a", L"B", LT},
    {ENUS, NORM_IGNORECASE, L"B", L"a", GT}, {ENUS, NORM_IGNORECASE, L"abc", L"abd", LT}, {ENUS, NORM_IGNORECASE, L"Z", L"a", GT}, {ENUS, NORM_IGNORECASE, L"", L"", EQ})
CMPTEST(ignorecase_unicode,
    {INV, NORM_IGNORECASE, L"\x00e9", L"\x00c9", EQ}, {INV, NORM_IGNORECASE, L"\x03b1", L"\x0391", EQ}, {INV, NORM_IGNORECASE, L"\x0430", L"\x0410", EQ},
    {INV, NORM_IGNORECASE, L"\x044f", L"\x042f", EQ}, {INV, NORM_IGNORECASE, L"\x00fc", L"\x00dc", EQ}, {INV, NORM_IGNORECASE, L"\x00f1", L"\x00d1", EQ},
    {INV, NORM_IGNORECASE, L"\x0101", L"\x0100", EQ}, {INV, NORM_IGNORECASE, L"\x03c3", L"\x03a3", EQ}, {INV, NORM_IGNORECASE, L"\x0451", L"\x0401", EQ},
    {INV, NORM_IGNORECASE, L"\x00e9", L"\x00c8", LT}, {INV, NORM_IGNORECASE, L"\x00e9", L"e", GT}, {INV, NORM_IGNORECASE, L"\xff41", L"\xff21", EQ},
    {INV, NORM_IGNORECASE, L"\x2170", L"\x2160", EQ}, {INV, NORM_IGNORECASE, L"\x1e9e", L"\x00df", EQ}, {INV, NORM_IGNORECASE, L"\x24d0", L"\x24b6", EQ},
    {INV, NORM_IGNORECASE, L"\x03c3", L"\x03a3", EQ})
CMPTEST(diacritics,
    {INV, 0, L"e", L"\x00e9", LT}, {INV, 0, L"\x00e9", L"e", GT}, {INV, 0, L"resume", L"r\x00e9" L"sum\x00e9", LT}, {INV, NORM_IGNORENONSPACE, L"e", L"\x00e9", EQ},
    {INV, NORM_IGNORENONSPACE, L"resume", L"r\x00e9" L"sum\x00e9", EQ}, {INV, NORM_IGNORENONSPACE, L"a", L"\x00e0", EQ}, {INV, NORM_IGNORENONSPACE, L"u", L"\x00fc", EQ},
    {INV, NORM_IGNORENONSPACE, L"n", L"\x00f1", EQ}, {INV, NORM_IGNORENONSPACE, L"a", L"b", LT}, {INV, NORM_IGNORENONSPACE, L"\x00e9", L"f", LT},
    {INV, NORM_IGNORENONSPACE | NORM_IGNORECASE, L"E", L"\x00e9", EQ}, {INV, NORM_IGNORENONSPACE, L"E", L"\x00e9", GT}, {INV, NORM_IGNORENONSPACE, L"e", L"e\x0301", EQ},
    {INV, 0, L"e\x0301", L"\x00e9", EQ}, {INV, 0, L"a\x0308", L"\x00e4", EQ}, {INV, 0, L"\x00e9", L"e\x0301", EQ}, {INV, 0, L"e", L"e\x0301", LT},
    {INV, 0, L"o\x0302\x0323", L"o\x0323\x0302", EQ}, {INV, LINGUISTIC_IGNOREDIACRITIC, L"e", L"\x00e9", EQ}, {INV, 0, L"\x1e09", L"c\x0327\x0301", EQ})
CMPTEST(symbols_wordsort,
    {INV, NORM_IGNORESYMBOLS, L"co-op", L"coop", EQ}, {INV, NORM_IGNORESYMBOLS, L"a b", L"ab", EQ}, {INV, NORM_IGNORESYMBOLS, L"a.b", L"ab", EQ}, {INV, NORM_IGNORESYMBOLS, L"a$b", L"ab", EQ},
    {INV, NORM_IGNORESYMBOLS, L"a", L"b", LT}, {INV, NORM_IGNORESYMBOLS, L"a-", L"b", LT}, {INV, NORM_IGNORESYMBOLS, L"!!", L"", EQ}, {INV, NORM_IGNORESYMBOLS | NORM_IGNORECASE, L"A-B", L"ab", EQ},
    {INV, NORM_IGNORESYMBOLS, L"it's", L"its", EQ}, {INV, NORM_IGNORESYMBOLS, L"a+b", L"ab", EQ},
    {INV, 0, L"co-op", L"con", GT}, {INV, 0, L"coop", L"con", GT}, {INV, SORT_STRINGSORT, L"co-op", L"con", LT}, {INV, SORT_STRINGSORT, L"coop", L"con", GT},
    {INV, SORT_STRINGSORT, L"a-b", L"ab", LT}, {INV, SORT_STRINGSORT, L"it's", L"its", LT}, {INV, SORT_STRINGSORT, L"a b", L"ab", LT}, {INV, SORT_STRINGSORT, L"a", L"a", EQ},
    {INV, SORT_STRINGSORT, L"abc", L"abd", LT}, {INV, SORT_STRINGSORT, L"a-b", L"a-c", LT})
CMPTEST(width_and_kana,
    {INV, 0, L"A", L"\xff21", LT}, {INV, NORM_IGNOREWIDTH, L"A", L"\xff21", EQ}, {INV, NORM_IGNOREWIDTH, L"a", L"\xff41", EQ}, {INV, NORM_IGNOREWIDTH, L"1", L"\xff11", EQ},
    {INV, NORM_IGNOREWIDTH, L"\x30a2", L"\xff71", EQ}, {INV, NORM_IGNOREWIDTH, L"\xff71", L"\x30a2", EQ}, {INV, NORM_IGNOREWIDTH, L"A", L"B", LT},
    {INV, NORM_IGNOREWIDTH | NORM_IGNORECASE, L"A", L"\xff41", EQ}, {INV, NORM_IGNOREWIDTH, L"A", L"\xff22", LT},
    {INV, NORM_IGNOREKANATYPE, L"\x3042", L"\x30a2", EQ}, {INV, NORM_IGNOREKANATYPE, L"\x3044", L"\x30a4", EQ},
    {INV, NORM_IGNOREKANATYPE, L"\x3042", L"\x3044", LT}, {INV, NORM_IGNOREKANATYPE | NORM_IGNOREWIDTH, L"\x3042", L"\xff71", EQ}, {INV, NORM_IGNOREKANATYPE, L"\x3053\x3093", L"\x30b3\x30f3", EQ},
    {INV, 0, L"\x3042", L"\x3042", EQ}, {INV, NORM_IGNOREKANATYPE, L"a", L"A", LT})
CMPTEST(digits_as_numbers,
    {ENUS, 0, L"a2", L"a10", GT}, {ENUS, SORT_DIGITSASNUMBERS, L"a2", L"a10", LT}, {ENUS, SORT_DIGITSASNUMBERS, L"a10", L"a2", GT}, {ENUS, SORT_DIGITSASNUMBERS, L"10", L"9", GT},
    {ENUS, SORT_DIGITSASNUMBERS, L"9", L"10", LT}, {ENUS, SORT_DIGITSASNUMBERS, L"a1", L"a1", EQ}, {ENUS, SORT_DIGITSASNUMBERS, L"file2.txt", L"file10.txt", LT},
    {ENUS, SORT_DIGITSASNUMBERS, L"file100", L"file20", GT}, {ENUS, SORT_DIGITSASNUMBERS, L"1", L"2", LT}, {ENUS, SORT_DIGITSASNUMBERS, L"a", L"b", LT},
    {ENUS, 0, L"10", L"9", LT}, {ENUS, 0, L"100", L"20", LT}, {ENUS, SORT_DIGITSASNUMBERS, L"x99", L"x100", LT}, {ENUS, SORT_DIGITSASNUMBERS, L"x100", L"x99", GT})
CMPTEST(swedish_collation,
    {L"sv-SE", 0, L"z", L"\x00e5", LT}, {L"sv-SE", 0, L"\x00e5", L"\x00e4", LT}, {L"sv-SE", 0, L"\x00e4", L"\x00f6", LT}, {L"sv-SE", 0, L"a", L"\x00e5", LT},
    {ENUS, 0, L"\x00e5", L"z", LT}, {ENUS, 0, L"\x00e4", L"z", LT}, {ENUS, 0, L"\x00f6", L"z", LT}, {L"sv-SE", 0, L"y", L"z", LT}, {L"sv-SE", 0, L"Z", L"\x00c5", LT})
CMPTEST(locale_names,
    {L"de-DE", 0, L"a", L"b", LT}, {L"fr-FR", 0, L"a", L"b", LT}, {L"ja-JP", 0, L"a", L"b", LT}, {L"zh-CN", 0, L"a", L"b", LT}, {L"tr-TR", 0, L"a", L"b", LT},
    {L"en-GB", 0, L"a", L"b", LT}, {L"ru-RU", 0, L"\x0430", L"\x0431", LT}, {L"ar-SA", 0, L"\x0627", L"\x0628", LT}, {L"he-IL", 0, L"\x05d0", L"\x05d1", LT},
    {L"en", 0, L"a", L"b", LT}, {L"de", 0, L"a", L"b", LT}, {L"fr-CA", 0, L"a", L"b", LT}, {L"pt-BR", 0, L"a", L"b", LT}, {L"es-ES", 0, L"a", L"b", LT},
    {L"zh-Hans", 0, L"a", L"b", LT}, {L"zh-Hant-TW", 0, L"a", L"b", LT}, {L"EN-us", 0, L"a", L"b", LT}, {L"en_US", 0, L"a", L"b", LT}, {L"el-GR", 0, L"\x03b1", L"\x03b2", LT},
    {L"ko-KR", 0, L"\xac00", L"\xac01", LT}, {L"hi-IN", 0, L"\x0905", L"\x0906", LT})
CMPTEST(tr_casing,
    {L"tr-TR", NORM_IGNORECASE | NORM_LINGUISTIC_CASING, L"I", L"\x0131", EQ}, {L"tr-TR", NORM_IGNORECASE | NORM_LINGUISTIC_CASING, L"i", L"\x0130", EQ},
    {L"tr-TR", NORM_IGNORECASE | NORM_LINGUISTIC_CASING, L"i", L"I", GT}, {L"tr-TR", NORM_IGNORECASE | NORM_LINGUISTIC_CASING, L"\x0131", L"i", LT},
    {L"tr-TR", NORM_IGNORECASE | NORM_LINGUISTIC_CASING, L"a", L"A", EQ}, {L"tr-TR", NORM_IGNORECASE | NORM_LINGUISTIC_CASING, L"I", L"i", LT})
CMPTEST(embedded_misc,
    {INV, 0, L"abc", L"abc ", LT}, {INV, 0, L"abc ", L"abc", GT}, {INV, 0, L"\t", L"", GT}, {INV, 0, L"a\x00a0" L"b", L"a b", GT},
    {INV, NORM_IGNORECASE, L"STRASSE", L"strasse", EQ},
    {INV, 0, L"\x4e2d", L"\x4e2d", EQ}, {INV, 0, L"\x4e2d", L"\x6587", LT}, {INV, 0, L"a", L"\x4e2d", LT}, {INV, 0, L"1", L"a", LT})

/* ---- count / length semantics ---- */
TEST(cmp_ex_counts) {
    REQUIRE(U(CompareStringEx));
    static const WCHAR s1[] = W_('a', 'b', 'c', 'd', 0), s2[] = W_('a', 'b', 'c', 'e', 0);
    EXPECT_EQ(U(CompareStringEx)(INV, 0, s1, 3, s2, 3, NULL, NULL, 0), EQ);
    EXPECT_EQ(U(CompareStringEx)(INV, 0, s1, 4, s2, 4, NULL, NULL, 0), LT);
    EXPECT_EQ(U(CompareStringEx)(INV, 0, s1, -1, s2, -1, NULL, NULL, 0), LT);
    EXPECT_EQ(U(CompareStringEx)(INV, 0, s1, 4, s2, -1, NULL, NULL, 0), LT);
    EXPECT_EQ(U(CompareStringEx)(INV, 0, s1, -1, s2, 3, NULL, NULL, 0), GT);
    EXPECT_EQ(U(CompareStringEx)(INV, 0, s1, 0, s2, 0, NULL, NULL, 0), EQ);
    EXPECT_EQ(U(CompareStringEx)(INV, 0, s1, 1, s2, 0, NULL, NULL, 0), GT);
    EXPECT_EQ(U(CompareStringEx)(INV, 0, s1, 0, s2, 1, NULL, NULL, 0), LT);
    EXPECT_EQ(U(CompareStringEx)(INV, 0, s1, 1, s2, 1, NULL, NULL, 0), EQ);
    EXPECT_EQ(U(CompareStringEx)(INV, 0, s1, 2, s2, 3, NULL, NULL, 0), LT);
    EXPECT_EQ(U(CompareStringEx)(INV, 0, s1, 3, s2, 2, NULL, NULL, 0), GT);
}
TEST(cmp_ex_embedded_nuls) {
    REQUIRE(U(CompareStringEx));
    static const WCHAR a[] = W_('a', 0, 'b'), b[] = W_('a', 0, 'c'), c[] = W_('a');
    EXPECT_EQ(U(CompareStringEx)(INV, 0, a, 3, b, 3, NULL, NULL, 0), LT);
    EXPECT_EQ(U(CompareStringEx)(INV, 0, a, 3, a, 3, NULL, NULL, 0), EQ);
    EXPECT_EQ(U(CompareStringEx)(INV, 0, a, 3, c, 1, NULL, NULL, 0), GT);
    EXPECT_EQ(U(CompareStringEx)(INV, 0, c, 1, a, 3, NULL, NULL, 0), LT);
    EXPECT_EQ(U(CompareStringEx)(INV, 0, a, -1, b, -1, NULL, NULL, 0), EQ);   /* both stop at the first NUL */
    EXPECT_EQ(U(CompareStringEx)(INV, 0, a, -1, c, 1, NULL, NULL, 0), EQ);
    t_note("CompareStringEx(\"a\\0\",2,\"a\",1) = %d (NUL ignorable?)", U(CompareStringEx)(INV, 0, a, 2, c, 1, NULL, NULL, 0));
}
TEST(cmp_w_counts_and_lcids) {
    REQUIRE(U(CompareStringW));
    static const LCID l[] = {LOCALE_INVARIANT, 0x0409, LOCALE_USER_DEFAULT, LOCALE_SYSTEM_DEFAULT, 0x0407, 0x040c, 0x0411, 0x0804, 0x041f, LOCALE_NEUTRAL, 0x0c00, 0x0809, 0x0419};
    for (int i = 0; i < ARRAY_N(l); i++) {
        SET_ERR(0);
        int r = U(CompareStringW)(l[i], 0, L"abc", -1, L"abd", -1);
        CHECKF(r == LT, "lcid %lx r=%d err=%lu", l[i], r, GetLastError());
        r = U(CompareStringW)(l[i], 0, L"abc", 3, L"abc", -1); CHECKF(r == EQ, "lcid %lx eq r=%d", l[i], r);
        r = U(CompareStringW)(l[i], 0, L"abd", -1, L"abc", -1); CHECKF(r == GT, "lcid %lx gt r=%d", l[i], r);
        r = U(CompareStringW)(l[i], NORM_IGNORECASE, L"ABC", -1, L"abc", -1); CHECKF(r == EQ, "lcid %lx ic r=%d", l[i], r);
    }
}
TEST(cmp_w_invalid_lcid) {
    REQUIRE(U(CompareStringW));
    static const LCID bad[] = {0xdeadbeef, 0xffff, 0x7fffffff, 0xffffffff};
    for (int i = 0; i < ARRAY_N(bad); i++) {
        SET_ERR(0);
        int r = U(CompareStringW)(bad[i], 0, L"a", -1, L"a", -1);
        CHECKF(r == 0 && GetLastError() == E_INVALID_PARAMETER, "lcid %lx r=%d err=%lu", bad[i], r, GetLastError());
    }
}
TEST(cmp_ex_invalid_locale_name) {
    REQUIRE(U(CompareStringEx));
    static const WCHAR *bad[] = {L"xx-INVALID-yy", L"zzzz", L"en-US-garbage-more", L"1234", L"!", L"en US", L"this-is-way-too-long-for-a-locale-name-0123456789"};
    for (int i = 0; i < ARRAY_N(bad); i++) {
        SET_ERR(0);
        int r = U(CompareStringEx)(bad[i], 0, L"a", -1, L"a", -1, NULL, NULL, 0);
        CHECKF(r == 0 && GetLastError() == E_INVALID_PARAMETER, "name %d r=%d err=%lu", i, r, GetLastError());
    }
}
TEST(cmp_w_null_strings) {
    REQUIRE(U(CompareStringW));
    SET_ERR(0); EXPECT_EQ(U(CompareStringW)(LOCALE_INVARIANT, 0, NULL, -1, L"a", -1), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(CompareStringW)(LOCALE_INVARIANT, 0, L"a", -1, NULL, -1), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(CompareStringW)(LOCALE_INVARIANT, 0, NULL, 1, L"a", 1), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(CompareStringW)(LOCALE_INVARIANT, 0, L"a", 1, NULL, 1), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); int r = U(CompareStringW)(LOCALE_INVARIANT, 0, NULL, 0, NULL, 0);
    t_note("CompareStringW(NULL,0,NULL,0) = %d err=%lu", r, GetLastError());
    SET_ERR(0); r = U(CompareStringW)(LOCALE_INVARIANT, 0, NULL, 0, L"a", 1);
    t_note("CompareStringW(NULL,0,\"a\",1) = %d err=%lu", r, GetLastError());
}
TEST(cmp_ex_null_strings) {
    REQUIRE(U(CompareStringEx));
    SET_ERR(0); EXPECT_EQ(U(CompareStringEx)(INV, 0, NULL, -1, L"a", -1, NULL, NULL, 0), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(CompareStringEx)(INV, 0, L"a", -1, NULL, -1, NULL, NULL, 0), 0); EXPECT_ERR(E_INVALID_PARAMETER);
}
TEST(cmp_w_invalid_flags) {
    REQUIRE(U(CompareStringW));
    SET_ERR(0);
    int r = U(CompareStringW)(LOCALE_INVARIANT, 0xffffffff, L"a", -1, L"a", -1);
    CHECK(r == 0); EXPECT_ERR(E_INVALID_FLAGS);
    SET_ERR(0);
    r = U(CompareStringW)(LOCALE_INVARIANT, 0x80000000, L"a", -1, L"a", -1);
    t_note("flags 0x80000000 -> r=%d err=%lu", r, GetLastError());
    static const DWORD odd[] = {0x40, 0x80, 0x100, 0x200, 0x400, 0x800, 0x2000, 0x4000, 0x8000, 0x40000, 0x80000, 0x100000, 0x1000000};
    for (int i = 0; i < ARRAY_N(odd); i++) { SET_ERR(0); r = U(CompareStringW)(LOCALE_INVARIANT, odd[i], L"a", -1, L"a", -1); t_note("flags %lx -> r=%d err=%lu", odd[i], r, r ? 0 : GetLastError()); }
}
TEST(cmp_valid_flag_combinations) {
    REQUIRE(U(CompareStringEx));
    static const DWORD f[] = {0, NORM_IGNORECASE, NORM_IGNORENONSPACE, NORM_IGNORESYMBOLS, NORM_IGNOREWIDTH, NORM_IGNOREKANATYPE, SORT_STRINGSORT, SORT_DIGITSASNUMBERS,
        NORM_LINGUISTIC_CASING, LINGUISTIC_IGNORECASE, LINGUISTIC_IGNOREDIACRITIC, NORM_IGNORECASE | NORM_IGNORENONSPACE | NORM_IGNORESYMBOLS | NORM_IGNOREWIDTH | NORM_IGNOREKANATYPE,
        NORM_IGNORECASE | SORT_STRINGSORT, SORT_STRINGSORT | NORM_IGNORESYMBOLS};
    for (int i = 0; i < ARRAY_N(f); i++) {
        SET_ERR(0);
        int r = U(CompareStringEx)(ENUS, f[i], L"abc", -1, L"abc", -1, NULL, NULL, 0);
        CHECKF(r == EQ, "flags %lx r=%d err=%lu", f[i], r, GetLastError());
    }
}
TEST(cmp_flag_monotonic_equalities) {
    REQUIRE(U(CompareStringEx));
    /* adding ignore flags can only turn LT/GT into EQ, never EQ into LT/GT */
    static const WCHAR *s[] = {L"abc", L"ABC", L"abc ", L"a-bc", L"\x00e0" L"bc", L"\xff41" L"bc", L"\x3042", L"\x30a2", L"a1", L"a10", L"a2"};
    static const DWORD f[] = {NORM_IGNORECASE, NORM_IGNORENONSPACE, NORM_IGNORESYMBOLS, NORM_IGNOREWIDTH, NORM_IGNOREKANATYPE};
    for (int i = 0; i < ARRAY_N(s); i++) for (int j = 0; j < ARRAY_N(s); j++) for (int k = 0; k < ARRAY_N(f); k++) {
        int base = U(CompareStringEx)(INV, 0, s[i], -1, s[j], -1, NULL, NULL, 0);
        int with = U(CompareStringEx)(INV, f[k], s[i], -1, s[j], -1, NULL, NULL, 0);
        if (base == EQ) CHECKF(with == EQ, "i=%d j=%d f=%lx lost equality", i, j, f[k]);
        CHECKF(with != 0, "i=%d j=%d f=%lx failed", i, j, f[k]);
    }
}
TEST(cmp_transitivity_small_set) {
    REQUIRE(U(CompareStringEx));
    static const WCHAR *s[] = {L"", L"a", L"A", L"b", L"B", L"ab", L"Ab", L"aB", L"AB", L"abc", L"a b", L"a-b", L"1", L"10", L"2", L"\x00e0", L"\x00e1", L"z", L"Z", L"\x00e5"};
    int n = ARRAY_N(s);
    for (int i = 0; i < n; i++) for (int j = 0; j < n; j++) for (int k = 0; k < n; k++) {
        int ij = U(CompareStringEx)(ENUS, 0, s[i], -1, s[j], -1, NULL, NULL, 0), jk = U(CompareStringEx)(ENUS, 0, s[j], -1, s[k], -1, NULL, NULL, 0), ik = U(CompareStringEx)(ENUS, 0, s[i], -1, s[k], -1, NULL, NULL, 0);
        if (ij == LT && jk == LT) CHECKF(ik == LT, "lt %d %d %d", i, j, k);
        if (ij == EQ && jk == EQ) CHECKF(ik == EQ, "eq %d %d %d", i, j, k);
        if (ij == GT && jk == GT) CHECKF(ik == GT, "gt %d %d %d", i, j, k);
    }
}
TEST(cmp_ex_version_info) {
    REQUIRE(U(CompareStringEx));
    NLSVERSIONINFO v; memset(&v, 0, sizeof v); v.dwNLSVersionInfoSize = 3;
    SET_ERR(0);
    int r = U(CompareStringEx)(ENUS, 0, L"a", -1, L"a", -1, &v, NULL, 0);
    CHECK(r == 0); EXPECT_ERR(E_INSUFFICIENT_BUFFER);
    SET_ERR(0);
    r = U(CompareStringEx)(NULL, 0, L"a", -1, L"a", -1, &v, NULL, 0);
    CHECK(r == 0);
    t_note("version info size 3 / NULL locale -> err=%lu", GetLastError());
}
TEST(cmp_ex_null_localename) {
    REQUIRE(U(CompareStringEx));
    SET_ERR(0);
    int r = U(CompareStringEx)(NULL, 0, L"a", -1, L"b", -1, NULL, NULL, 0);
    t_note("CompareStringEx(NULL locale) -> %d err=%lu", r, r ? 0 : GetLastError());
    r = U(CompareStringEx)(LOCALE_NAME_USER_DEFAULT, 0, L"a", -1, L"b", -1, NULL, NULL, 0);
    EXPECT_EQ(r, LT);
    r = U(CompareStringEx)(LOCALE_NAME_SYSTEM_DEFAULT, 0, L"a", -1, L"b", -1, NULL, NULL, 0);
    EXPECT_EQ(r, LT);
    r = U(CompareStringEx)(LOCALE_NAME_INVARIANT, 0, L"a", -1, L"b", -1, NULL, NULL, 0);
    EXPECT_EQ(r, LT);
}

/* ---- CompareStringA ---- */
TEST(cmp_a_basic) {
    REQUIRE(U(CompareStringA));
    static const struct { const char *a, *b; DWORD fl; int r; } v[] = {
        {"", "", 0, EQ}, {"a", "a", 0, EQ}, {"a", "b", 0, LT}, {"b", "a", 0, GT}, {"a", "A", 0, LT}, {"A", "a", 0, GT}, {"a", "A", NORM_IGNORECASE, EQ},
        {"abc", "ABC", NORM_IGNORECASE, EQ}, {"abc", "abd", 0, LT}, {"ab", "abc", 0, LT}, {"co-op", "con", 0, GT}, {"co-op", "con", SORT_STRINGSORT, LT},
        {"co-op", "coop", NORM_IGNORESYMBOLS, EQ}, {"a2", "a10", 0, GT}, {"hello", "HELLO", NORM_IGNORECASE, EQ},
    };
    static const LCID locs[] = {LOCALE_INVARIANT, 0x0409, LOCALE_USER_DEFAULT};
    for (int l = 0; l < 3; l++) for (int i = 0; i < ARRAY_N(v); i++) {
        if (locs[l] == LOCALE_USER_DEFAULT && (v[i].fl & SORT_DIGITSASNUMBERS)) continue;
        SET_ERR(0);
        int r = U(CompareStringA)(locs[l], v[i].fl, v[i].a, -1, v[i].b, -1);
        if (locs[l] == LOCALE_USER_DEFAULT && r != v[i].r) { t_note("user default locale vec %d got %d", i, r); continue; }
        CHECKF(r == v[i].r, "lcid %lx vec %d [%s][%s] got %d want %d (err %lu)", locs[l], i, v[i].a, v[i].b, r, v[i].r, GetLastError());
        r = U(CompareStringA)(locs[l], v[i].fl, v[i].a, (int)strlen(v[i].a), v[i].b, (int)strlen(v[i].b));
        if (locs[l] != LOCALE_USER_DEFAULT) CHECKF(r == v[i].r, "lcid %lx vec %d explicit got %d", locs[l], i, r);
    }
}
TEST(cmp_a_errors) {
    REQUIRE(U(CompareStringA));
    SET_ERR(0); EXPECT_EQ(U(CompareStringA)(LOCALE_INVARIANT, 0, NULL, -1, "a", -1), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(CompareStringA)(LOCALE_INVARIANT, 0, "a", -1, NULL, -1), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(CompareStringA)(0xdeadbeef, 0, "a", -1, "a", -1), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(CompareStringA)(LOCALE_INVARIANT, 0xffffffff, "a", -1, "a", -1), 0); EXPECT_ERR(E_INVALID_FLAGS);
}
TEST(cmp_a_acp_chars) {
    REQUIRE(U(CompareStringA)); REQUIRE(U(GetACP));
    if (U(GetACP)() != 1252) { t_note("ACP %u: skipping 1252 byte vectors", U(GetACP)()); return; }
    EXPECT_EQ(U(CompareStringA)(LOCALE_INVARIANT, 0, "\xe9", -1, "e", -1), GT);
    EXPECT_EQ(U(CompareStringA)(LOCALE_INVARIANT, NORM_IGNORENONSPACE, "\xe9", -1, "e", -1), EQ);
    EXPECT_EQ(U(CompareStringA)(LOCALE_INVARIANT, NORM_IGNORECASE, "\xe9", -1, "\xc9", -1), EQ);
    EXPECT_EQ(U(CompareStringA)(LOCALE_INVARIANT, 0, "\xe9", -1, "\xc9", -1), LT);
    EXPECT_EQ(U(CompareStringA)(LOCALE_INVARIANT, 0, "\x80", -1, "\x80", -1), EQ);
    EXPECT_EQ(U(CompareStringA)(0x0409, 0, "caf\xe9", -1, "cafe", -1), GT);
}
TEST(cmp_a_embedded_nul_and_counts) {
    REQUIRE(U(CompareStringA));
    EXPECT_EQ(U(CompareStringA)(LOCALE_INVARIANT, 0, "a\0b", 3, "a\0c", 3), LT);
    EXPECT_EQ(U(CompareStringA)(LOCALE_INVARIANT, 0, "a\0b", -1, "a\0c", -1), EQ);
    EXPECT_EQ(U(CompareStringA)(LOCALE_INVARIANT, 0, "abc", 2, "abd", 2), EQ);
    EXPECT_EQ(U(CompareStringA)(LOCALE_INVARIANT, 0, "abc", 0, "", 0), EQ);
    EXPECT_EQ(U(CompareStringA)(LOCALE_INVARIANT, 0, "a", 1, "", 0), GT);
}

/* ---- CompareStringOrdinal ---- */
TEST(cmpord_basic) {
    REQUIRE(U(CompareStringOrdinal));
    static const struct { const WCHAR *a, *b; BOOL ic; int r; } v[] = {
        {L"", L"", 0, EQ}, {L"", L"", 1, EQ}, {L"a", L"a", 0, EQ}, {L"a", L"A", 0, GT}, {L"A", L"a", 0, LT}, {L"a", L"A", 1, EQ}, {L"A", L"a", 1, EQ},
        {L"abc", L"abd", 0, LT}, {L"abd", L"abc", 0, GT}, {L"ab", L"abc", 0, LT}, {L"abc", L"ab", 0, GT}, {L"", L"a", 0, LT}, {L"a", L"", 0, GT},
        {L"abc", L"ABD", 1, LT}, {L"ABD", L"abc", 1, GT}, {L"Zebra", L"apple", 0, LT}, {L"Zebra", L"apple", 1, GT}, {L"z", L"A", 1, GT}, {L"Z", L"a", 1, GT},
        {L"\x00e9", L"\x00c9", 1, EQ}, {L"\x00e9", L"\x00c9", 0, GT}, {L"\x00e9", L"e", 0, GT}, {L"\x00e9", L"e", 1, GT}, {L"\x0430", L"\x0410", 1, EQ},
        {L"\x044f", L"\x042f", 1, EQ}, {L"\xff41", L"\xff21", 1, EQ}, {L"\x03c3", L"\x03a3", 1, EQ},
        {L"\x00df", L"SS", 1, GT}, {L"\x00df", L"ss", 1, GT}, {L"\x0101", L"\x0100", 1, EQ}, {L"\x2170", L"\x2160", 1, EQ},
        {L"\x4e2d", L"\x4e2d", 0, EQ}, {L"\x4e2d", L"\x6587", 0, LT}, {L"\xfffe", L"\xffff", 0, LT}, {L"\x0000", L"", 0, EQ}, {L"1", L"2", 0, LT}, {L"10", L"9", 0, LT},
        {L"a b", L"ab", 0, LT}, {L"a-b", L"ab", 0, LT}, {L"abc ", L"abc", 0, GT}, {L"\x00e9", L"e\x0301", 0, GT}, {L"e\x0301", L"\x00e9", 0, LT},
        {L"\xd800", L"\xd800", 0, EQ}, {L"\xd800", L"\xdc00", 0, LT}, {L"\xd83d\xde00", L"\xd83d\xde00", 0, EQ}, {L"\xd83d\xde00", L"\xffff", 0, LT},
    };
    for (int i = 0; i < ARRAY_N(v); i++) {
        SET_ERR(0);
        int r = U(CompareStringOrdinal)(v[i].a, -1, v[i].b, -1, v[i].ic);
        CHECKF(r == v[i].r, "vec %d ic=%d got %d want %d", i, v[i].ic, r, v[i].r);
        int la = lstrlenW(v[i].a), lb = lstrlenW(v[i].b);
        r = U(CompareStringOrdinal)(v[i].a, la, v[i].b, lb, v[i].ic);
        CHECKF(r == v[i].r, "vec %d explicit got %d want %d", i, r, v[i].r);
        r = U(CompareStringOrdinal)(v[i].a, la, v[i].b, -1, v[i].ic);
        CHECKF(r == v[i].r, "vec %d mixed1 got %d", i, r);
        r = U(CompareStringOrdinal)(v[i].a, -1, v[i].b, lb, v[i].ic);
        CHECKF(r == v[i].r, "vec %d mixed2 got %d", i, r);
        int rev = U(CompareStringOrdinal)(v[i].b, -1, v[i].a, -1, v[i].ic);
        CHECKF(rev == (v[i].r == LT ? GT : v[i].r == GT ? LT : EQ), "vec %d reversed %d", i, rev);
    }
}
TEST(cmpord_params) {
    REQUIRE(U(CompareStringOrdinal));
    SET_ERR(0); EXPECT_EQ(U(CompareStringOrdinal)(NULL, -1, L"a", -1, FALSE), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(CompareStringOrdinal)(L"a", -1, NULL, -1, FALSE), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(CompareStringOrdinal)(NULL, 0, NULL, 0, FALSE), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(CompareStringOrdinal)(NULL, 1, L"a", 1, TRUE), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(CompareStringOrdinal)(L"a", -1, L"a", -1, 2), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(CompareStringOrdinal)(L"a", -1, L"a", -1, -1), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(CompareStringOrdinal)(L"a", -1, L"a", -1, 0x100), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(CompareStringOrdinal)(L"a", -2, L"a", -2, FALSE), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(CompareStringOrdinal)(L"a", -2, L"a", -1, FALSE), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(CompareStringOrdinal)(L"a", -1, L"a", -5, FALSE), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(CompareStringOrdinal)(L"a", INT_MIN, L"a", INT_MIN, FALSE), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(CompareStringOrdinal)(L"a", 0, L"", 0, FALSE), EQ);
    SET_ERR(0); EXPECT_EQ(U(CompareStringOrdinal)(L"a", 0, L"b", 0, TRUE), EQ);
}
TEST(cmpord_counts) {
    REQUIRE(U(CompareStringOrdinal));
    EXPECT_EQ(U(CompareStringOrdinal)(L"abc", 2, L"abd", 2, FALSE), EQ);
    EXPECT_EQ(U(CompareStringOrdinal)(L"abc", 3, L"abd", 3, FALSE), LT);
    EXPECT_EQ(U(CompareStringOrdinal)(L"abc", 1, L"ABD", 1, TRUE), EQ);
    EXPECT_EQ(U(CompareStringOrdinal)(L"abc", 2, L"ab", -1, FALSE), EQ);
    EXPECT_EQ(U(CompareStringOrdinal)(L"abc", -1, L"ab", 2, FALSE), GT);
    EXPECT_EQ(U(CompareStringOrdinal)(L"abc", 0, L"abc", -1, FALSE), LT);
    EXPECT_EQ(U(CompareStringOrdinal)(L"abc", -1, L"abc", 0, FALSE), GT);
    static const WCHAR a[] = W_('a', 0, 'b'), b[] = W_('a', 0, 'c');
    EXPECT_EQ(U(CompareStringOrdinal)(a, 3, b, 3, FALSE), LT);
    EXPECT_EQ(U(CompareStringOrdinal)(a, 3, a, 3, FALSE), EQ);
    EXPECT_EQ(U(CompareStringOrdinal)(a, -1, b, -1, FALSE), EQ);
    EXPECT_EQ(U(CompareStringOrdinal)(a, 3, b, -1, FALSE), GT);
}
TEST(cmpord_no_lasterror_clobber_needed) {
    REQUIRE(U(CompareStringOrdinal));
    SET_ERR(0x4321);
    int r = U(CompareStringOrdinal)(L"a", -1, L"b", -1, FALSE);
    EXPECT_EQ(r, LT);
    t_note("LastError after success: %lu", GetLastError());
}
TEST(cmpord_upcase_table_all_bmp_symmetric) {
    REQUIRE(U(CompareStringOrdinal));
    /* case insensitive result must be EQ whenever both orderings agree, and never fail */
    int fails = 0;
    for (unsigned c = 1; c < 0x10000; c += 7) {
        if (c >= 0xD800 && c < 0xE000) continue;
        WCHAR a[2] = {(WCHAR)c, 0}, b[2] = {(WCHAR)(c ^ 1), 0};
        int r1 = U(CompareStringOrdinal)(a, 1, b, 1, TRUE), r2 = U(CompareStringOrdinal)(b, 1, a, 1, TRUE);
        if (!r1 || !r2) fails++;
        else if (r1 == EQ && r2 != EQ) fails++;
        else if (r1 == LT && r2 != GT) fails++;
        else if (r1 == GT && r2 != LT) fails++;
    }
    EXPECT_EQ(fails, 0);
}
TEST(cmpord_ascii_case_table) {
    REQUIRE(U(CompareStringOrdinal));
    for (int c = 1; c < 128; c++) {
        WCHAR a[2] = {(WCHAR)c, 0};
        int lower_upper = (c >= 'a' && c <= 'z') ? c - 32 : (c >= 'A' && c <= 'Z') ? c + 32 : c;
        WCHAR b[2] = {(WCHAR)lower_upper, 0};
        CHECKF(U(CompareStringOrdinal)(a, 1, b, 1, TRUE) == EQ, "c=%d", c);
        CHECKF(U(CompareStringOrdinal)(a, 1, b, 1, FALSE) == (c == lower_upper ? EQ : (c < lower_upper ? LT : GT)), "c=%d cs", c);
    }
}
TEST(cmpord_large_equal) {
    REQUIRE(U(CompareStringOrdinal));
    enum { N = 100000 };
    WCHAR *a = HeapAlloc(GetProcessHeap(), 0, (N + 1) * 2), *b = HeapAlloc(GetProcessHeap(), 0, (N + 1) * 2);
    for (int i = 0; i < N; i++) a[i] = b[i] = (WCHAR)('a' + i % 26);
    a[N] = b[N] = 0;
    EXPECT_EQ(U(CompareStringOrdinal)(a, N, b, N, FALSE), EQ);
    EXPECT_EQ(U(CompareStringOrdinal)(a, -1, b, -1, TRUE), EQ);
    b[N - 1] = 'z' + 0; a[N - 1] = 'a';
    EXPECT_EQ(U(CompareStringOrdinal)(a, N, b, N, FALSE), LT);
    HeapFree(GetProcessHeap(), 0, a); HeapFree(GetProcessHeap(), 0, b);
}

/* ---- FindStringOrdinal ---- */
#define FIND_STARTSWITH_ 0x00100000
#define FIND_ENDSWITH_ 0x00200000
#define FIND_FROMSTART_ 0x00400000
#define FIND_FROMEND_ 0x00800000
TEST(findord_table) {
    REQUIRE(U(FindStringOrdinal));
    static const struct { DWORD fl; const WCHAR *src; const WCHAR *val; BOOL ic; int r; } v[] = {
        {FIND_FROMSTART_, L"hello world", L"o", 0, 4}, {FIND_FROMEND_, L"hello world", L"o", 0, 7}, {FIND_FROMSTART_, L"hello world", L"world", 0, 6}, {FIND_FROMEND_, L"hello world", L"world", 0, 6},
        {FIND_FROMSTART_, L"hello world", L"hello", 0, 0}, {FIND_STARTSWITH_, L"hello world", L"hello", 0, 0}, {FIND_STARTSWITH_, L"hello world", L"world", 0, -1}, {FIND_STARTSWITH_, L"hello world", L"h", 0, 0},
        {FIND_ENDSWITH_, L"hello world", L"world", 0, 6}, {FIND_ENDSWITH_, L"hello world", L"hello", 0, -1}, {FIND_ENDSWITH_, L"hello world", L"d", 0, 10}, {FIND_ENDSWITH_, L"hello world", L"hello world", 0, 0},
        {FIND_FROMSTART_, L"hello world", L"xyz", 0, -1}, {FIND_FROMEND_, L"hello world", L"xyz", 0, -1}, {FIND_FROMSTART_, L"hello", L"hello world", 0, -1}, {FIND_FROMSTART_, L"abc", L"abc", 0, 0},
        {FIND_FROMSTART_, L"Hello", L"hello", 0, -1}, {FIND_FROMSTART_, L"Hello", L"hello", 1, 0}, {FIND_FROMEND_, L"aAaA", L"a", 1, 3}, {FIND_FROMEND_, L"aAaA", L"a", 0, 2}, {FIND_FROMSTART_, L"aAaA", L"A", 0, 1},
        {FIND_STARTSWITH_, L"HELLO", L"hel", 1, 0}, {FIND_STARTSWITH_, L"HELLO", L"hel", 0, -1}, {FIND_ENDSWITH_, L"HELLO", L"llo", 1, 2}, {FIND_ENDSWITH_, L"HELLO", L"llo", 0, -1},
        {FIND_FROMSTART_, L"aaaa", L"aa", 0, 0}, {FIND_FROMEND_, L"aaaa", L"aa", 0, 2}, {FIND_FROMEND_, L"abab", L"ab", 0, 2}, {FIND_FROMSTART_, L"abab", L"ba", 0, 1}, {FIND_FROMEND_, L"abab", L"ba", 0, 1},
        {FIND_FROMSTART_, L"\x00e9" L"t\x00e9", L"\x00c9", 1, 0}, {FIND_FROMEND_, L"\x00e9" L"t\x00e9", L"\x00c9", 1, 2}, {FIND_FROMSTART_, L"\x00e9" L"t\x00e9", L"e", 1, -1},
        {0, L"hello world", L"o", 0, 4}, {0, L"hello world", L"xyz", 0, -1},
        /* empty search value */
        {FIND_FROMSTART_, L"abc", L"", 0, 0}, {FIND_FROMEND_, L"abc", L"", 0, 3}, {FIND_STARTSWITH_, L"abc", L"", 0, 0}, {FIND_ENDSWITH_, L"abc", L"", 0, 3},
        {FIND_FROMSTART_, L"", L"", 0, 0}, {FIND_FROMEND_, L"", L"", 0, 0}, {FIND_FROMSTART_, L"", L"a", 0, -1}, {FIND_STARTSWITH_, L"", L"a", 0, -1}, {FIND_ENDSWITH_, L"", L"a", 0, -1},
    };
    for (int i = 0; i < ARRAY_N(v); i++) {
        SET_ERR(0x1234);
        int r = U(FindStringOrdinal)(v[i].fl, v[i].src, -1, v[i].val, -1, v[i].ic);
        CHECKF(r == v[i].r, "vec %d got %d want %d (err %lu)", i, r, v[i].r, GetLastError());
        CHECKF(GetLastError() == 0, "vec %d lasterror %lu (expect cleared)", i, GetLastError());
        r = U(FindStringOrdinal)(v[i].fl, v[i].src, lstrlenW(v[i].src), v[i].val, lstrlenW(v[i].val), v[i].ic);
        CHECKF(r == v[i].r, "vec %d explicit got %d", i, r);
    }
}
TEST(findord_params) {
    REQUIRE(U(FindStringOrdinal));
    SET_ERR(0); EXPECT_EQ(U(FindStringOrdinal)(FIND_FROMSTART_, NULL, -1, L"a", -1, FALSE), -1); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(FindStringOrdinal)(FIND_FROMSTART_, L"a", -1, NULL, -1, FALSE), -1); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(FindStringOrdinal)(FIND_FROMSTART_, L"a", -1, L"a", -1, 2), -1); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(FindStringOrdinal)(FIND_FROMSTART_, L"a", -2, L"a", -1, FALSE), -1); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(FindStringOrdinal)(FIND_FROMSTART_, L"a", -1, L"a", -2, FALSE), -1); EXPECT_ERR(E_INVALID_PARAMETER);
    /* FindStringOrdinal: bad flags (multiple/foreign bits) fail 1004; bad pointers/ignorecase/cch fail 87 */
    SET_ERR(0); EXPECT_EQ(U(FindStringOrdinal)(FIND_FROMSTART_ | FIND_FROMEND_, L"a", -1, L"a", -1, FALSE), -1); EXPECT_ERR(E_INVALID_FLAGS);
    SET_ERR(0); EXPECT_EQ(U(FindStringOrdinal)(FIND_STARTSWITH_ | FIND_ENDSWITH_, L"a", -1, L"a", -1, FALSE), -1); EXPECT_ERR(E_INVALID_FLAGS);
    SET_ERR(0); EXPECT_EQ(U(FindStringOrdinal)(FIND_FROMSTART_ | 1, L"a", -1, L"a", -1, FALSE), -1); EXPECT_ERR(E_INVALID_FLAGS);
    SET_ERR(0); EXPECT_EQ(U(FindStringOrdinal)(0x80000000, L"a", -1, L"a", -1, FALSE), -1); EXPECT_ERR(E_INVALID_FLAGS);
    SET_ERR(0); EXPECT_EQ(U(FindStringOrdinal)(0x1000000, L"a", -1, L"a", -1, FALSE), -1); EXPECT_ERR(E_INVALID_FLAGS);
    SET_ERR(0); EXPECT_EQ(U(FindStringOrdinal)(1, L"a", -1, L"a", -1, FALSE), -1); EXPECT_ERR(E_INVALID_FLAGS);
    SET_ERR(0); EXPECT_EQ(U(FindStringOrdinal)(FIND_FROMSTART_, L"a", 0, L"a", 1, FALSE), -1); EXPECT_ERR(0);
    SET_ERR(0); EXPECT_EQ(U(FindStringOrdinal)(FIND_FROMSTART_, L"abc", 2, L"abc", 3, FALSE), -1); EXPECT_ERR(0);
    SET_ERR(0); EXPECT_EQ(U(FindStringOrdinal)(FIND_FROMSTART_, L"abc", 2, L"bc", 2, FALSE), -1); EXPECT_ERR(0);
    EXPECT_EQ(U(FindStringOrdinal)(FIND_FROMSTART_, L"abc", 2, L"b", 1, FALSE), 1);
    EXPECT_EQ(U(FindStringOrdinal)(FIND_FROMEND_, L"abcabc", 5, L"c", 1, FALSE), 2);
    EXPECT_EQ(U(FindStringOrdinal)(FIND_ENDSWITH_, L"abcabc", 5, L"ab", 2, FALSE), 3);
}
TEST(findord_embedded_nul) {
    REQUIRE(U(FindStringOrdinal));
    static const WCHAR s[] = W_('a', 0, 'b', 0, 'c'), v1[] = W_('b', 0), v2[] = W_(0, 'c');
    EXPECT_EQ(U(FindStringOrdinal)(FIND_FROMSTART_, s, 5, v1, 2, FALSE), 2);
    EXPECT_EQ(U(FindStringOrdinal)(FIND_FROMSTART_, s, 5, v2, 2, FALSE), 3);
    EXPECT_EQ(U(FindStringOrdinal)(FIND_ENDSWITH_, s, 5, v2, 2, FALSE), 3);
    EXPECT_EQ(U(FindStringOrdinal)(FIND_FROMSTART_, s, -1, v1, 2, FALSE), -1);   /* source stops at first NUL */
}

/* ---- FindNLSString / FindNLSStringEx ---- */
TEST(findnls_ex_basic) {
    REQUIRE(U(FindNLSStringEx));
    int found;
    SET_ERR(0);
    int r = U(FindNLSStringEx)(INV, FIND_FROMSTART, L"hello world", -1, L"world", -1, &found, NULL, NULL, 0);
    EXPECT_EQ(r, 6); EXPECT_EQ(found, 5);
    r = U(FindNLSStringEx)(INV, FIND_FROMEND, L"hello hello", -1, L"hello", -1, &found, NULL, NULL, 0);
    EXPECT_EQ(r, 6); EXPECT_EQ(found, 5);
    r = U(FindNLSStringEx)(INV, FIND_FROMSTART, L"hello hello", -1, L"hello", -1, &found, NULL, NULL, 0);
    EXPECT_EQ(r, 0);
    r = U(FindNLSStringEx)(INV, FIND_STARTSWITH, L"hello world", -1, L"hello", -1, &found, NULL, NULL, 0);
    EXPECT_EQ(r, 0); EXPECT_EQ(found, 5);
    r = U(FindNLSStringEx)(INV, FIND_STARTSWITH, L"hello world", -1, L"world", -1, &found, NULL, NULL, 0);
    EXPECT_EQ(r, -1);
    r = U(FindNLSStringEx)(INV, FIND_ENDSWITH, L"hello world", -1, L"world", -1, &found, NULL, NULL, 0);
    EXPECT_EQ(r, 6); EXPECT_EQ(found, 5);
    r = U(FindNLSStringEx)(INV, FIND_ENDSWITH, L"hello world", -1, L"hello", -1, &found, NULL, NULL, 0);
    EXPECT_EQ(r, -1);
    r = U(FindNLSStringEx)(INV, FIND_FROMSTART, L"hello", -1, L"xyz", -1, &found, NULL, NULL, 0);
    EXPECT_EQ(r, -1);
    r = U(FindNLSStringEx)(INV, FIND_FROMSTART, L"hello", -1, L"HELLO", -1, &found, NULL, NULL, 0);
    EXPECT_EQ(r, -1);
    r = U(FindNLSStringEx)(INV, FIND_FROMSTART | NORM_IGNORECASE, L"hello", -1, L"HELLO", -1, &found, NULL, NULL, 0);
    EXPECT_EQ(r, 0); EXPECT_EQ(found, 5);
    r = U(FindNLSStringEx)(INV, FIND_FROMEND | NORM_IGNORECASE, L"aAaA", -1, L"a", -1, &found, NULL, NULL, 0);
    EXPECT_EQ(r, 3);
    r = U(FindNLSStringEx)(INV, FIND_FROMSTART, L"hello", 5, L"ll", 2, NULL, NULL, NULL, 0);
    EXPECT_EQ(r, 2);
}
TEST(findnls_ex_canonical_equivalence) {
    REQUIRE(U(FindNLSStringEx));
    int found = -1;
    int r = U(FindNLSStringEx)(INV, FIND_FROMSTART, L"xa\x0308" L"y", -1, L"\x00e4", -1, &found, NULL, NULL, 0);
    EXPECT_EQ(r, 1); EXPECT_EQ(found, 2);
    found = -1;
    r = U(FindNLSStringEx)(INV, FIND_FROMSTART, L"x\x00e4" L"y", -1, L"a\x0308", -1, &found, NULL, NULL, 0);
    EXPECT_EQ(r, 1); EXPECT_EQ(found, 1);
    found = -1;
    r = U(FindNLSStringEx)(INV, FIND_FROMSTART | NORM_IGNORENONSPACE, L"caf\x00e9", -1, L"cafe", -1, &found, NULL, NULL, 0);
    EXPECT_EQ(r, 0); EXPECT_EQ(found, 4);
    r = U(FindNLSStringEx)(INV, FIND_FROMSTART, L"caf\x00e9", -1, L"cafe", -1, &found, NULL, NULL, 0);
    EXPECT_EQ(r, -1);
    r = U(FindNLSStringEx)(INV, FIND_FROMSTART | NORM_IGNOREWIDTH, L"x\xff21" L"y", -1, L"A", -1, &found, NULL, NULL, 0);
    EXPECT_EQ(r, 1);
}
TEST(findnls_ex_errors) {
    REQUIRE(U(FindNLSStringEx));
    int found;
    SET_ERR(0); EXPECT_EQ(U(FindNLSStringEx)(INV, FIND_FROMSTART, NULL, -1, L"a", -1, &found, NULL, NULL, 0), -1); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(FindNLSStringEx)(INV, FIND_FROMSTART, L"a", -1, NULL, -1, &found, NULL, NULL, 0), -1); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(FindNLSStringEx)(INV, FIND_FROMSTART, L"a", 0, L"a", -1, &found, NULL, NULL, 0), -1); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(FindNLSStringEx)(INV, FIND_FROMSTART, L"a", -1, L"a", 0, &found, NULL, NULL, 0), -1); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); { int rr = (U(FindNLSStringEx)(INV, 0, L"a", -1, L"a", -1, &found, NULL, NULL, 0));  t_note("FindNLSStringEx flag-combo -> %d err=%lu (validated in the sort callee, not decompile-verified)", rr, GetLastError()); }
    SET_ERR(0); { int rr = (U(FindNLSStringEx)(INV, FIND_FROMSTART | FIND_FROMEND, L"a", -1, L"a", -1, &found, NULL, NULL, 0));  t_note("FindNLSStringEx flag-combo -> %d err=%lu (validated in the sort callee, not decompile-verified)", rr, GetLastError()); }
    SET_ERR(0); { int rr = (U(FindNLSStringEx)(INV, FIND_STARTSWITH | FIND_ENDSWITH, L"a", -1, L"a", -1, &found, NULL, NULL, 0));  t_note("FindNLSStringEx flag-combo -> %d err=%lu (validated in the sort callee, not decompile-verified)", rr, GetLastError()); }
    SET_ERR(0); EXPECT_EQ(U(FindNLSStringEx)(L"xx-INVALID-yy", FIND_FROMSTART, L"a", -1, L"a", -1, &found, NULL, NULL, 0), -1); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(FindNLSStringEx)(INV, FIND_FROMSTART, L"a", -1, L"a", -1, &found, NULL, NULL, 0), 0); EXPECT_ERR(0);
    SET_ERR(0); EXPECT_EQ(U(FindNLSStringEx)(INV, FIND_FROMSTART, L"a", -1, L"b", -1, &found, NULL, NULL, 0), -1);
    t_note("not-found last error = %lu", GetLastError());
}
TEST(findnls_lcid_basic) {
    REQUIRE(U(FindNLSString));
    int found = 0;
    EXPECT_EQ(U(FindNLSString)(LOCALE_INVARIANT, FIND_FROMSTART, L"hello world", -1, L"o w", -1, &found), 4);
    EXPECT_EQ(found, 3);
    EXPECT_EQ(U(FindNLSString)(0x0409, FIND_FROMEND | NORM_IGNORECASE, L"abcABC", -1, L"b", -1, &found), 4);
    EXPECT_EQ(found, 1);
    SET_ERR(0);
    EXPECT_EQ(U(FindNLSString)(0xdeadbeef, FIND_FROMSTART, L"a", -1, L"a", -1, &found), -1);
    EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0);
    EXPECT_EQ(U(FindNLSString)(LOCALE_INVARIANT, FIND_FROMSTART, L"abc", -1, L"", 0, &found), -1);
    EXPECT_ERR(E_INVALID_PARAMETER);
}
TEST(findnls_pcchfound_null_ok) {
    REQUIRE(U(FindNLSString)); REQUIRE(U(FindNLSStringEx));
    EXPECT_EQ(U(FindNLSString)(LOCALE_INVARIANT, FIND_FROMSTART, L"abc", -1, L"b", -1, NULL), 1);
    EXPECT_EQ(U(FindNLSStringEx)(INV, FIND_FROMSTART, L"abc", -1, L"c", -1, NULL, NULL, NULL, 0), 2);
}
TEST(cmpord_final_sigma_note) {
    REQUIRE(U(CompareStringOrdinal));
    t_note("ordinal ignore-case: final sigma vs sigma = %d, vs capital = %d", U(CompareStringOrdinal)(L"\x03c2", -1, L"\x03c3", -1, TRUE), U(CompareStringOrdinal)(L"\x03c2", -1, L"\x03a3", -1, TRUE));
    t_note("hiragana vs katakana (default) = %d", U(CompareStringEx)(L"", 0, L"\x3042", -1, L"\x30a2", -1, NULL, NULL, 0));
    t_note("sharp s vs ss (ignorecase) = %d", U(CompareStringEx)(L"", NORM_IGNORECASE, L"\x00df", -1, L"ss", -1, NULL, NULL, 0));
}
