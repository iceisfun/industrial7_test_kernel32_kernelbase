/* LCMapString*, GetStringType*, FoldString* */
#include "sc.h"
#include <limits.h>

#define INV L""
#ifndef LCMAP_TITLECASE
#define LCMAP_TITLECASE 0x300
#endif
#ifndef LCMAP_SORTKEY
#define LCMAP_SORTKEY 0x400
#endif

typedef struct { const WCHAR *loc; DWORD fl; const WCHAR *src; const WCHAR *dst; } lcv_t;

static void run_lc(const lcv_t *v, int n, const char *tag) {
    if (!U(LCMapStringEx)) { t_skip("missing export: LCMapStringEx"); return; }
    for (int i = 0; i < n; i++) {
        WCHAR out[64]; int sl = lstrlenW(v[i].src), dl = lstrlenW(v[i].dst);
        SET_ERR(0);
        int q = U(LCMapStringEx)(v[i].loc, v[i].fl, v[i].src, sl, NULL, 0, NULL, NULL, 0);
        CHECKF(q == dl, "%s vec %d size query got %d want %d (err %lu)", tag, i, q, dl, GetLastError());
        for (int k = 0; k < 64; k++) out[k] = 0x7777;
        int r = U(LCMapStringEx)(v[i].loc, v[i].fl, v[i].src, sl, out, 64, NULL, NULL, 0);
        CHECKF(r == dl, "%s vec %d got %d want %d", tag, i, r, dl);
        if (r == dl) {
            char g[256], e[256]; sc_wdump(g, sizeof g, out, r); sc_wdump(e, sizeof e, v[i].dst, dl);
            CHECKF(sc_weq(out, v[i].dst, dl), "%s vec %d content got [%s] want [%s]", tag, i, g, e);
            CHECKF(out[dl] == 0x7777, "%s vec %d wrote past result", tag, i);
        }
        /* cch -1 includes the terminator */
        r = U(LCMapStringEx)(v[i].loc, v[i].fl, v[i].src, -1, out, 64, NULL, NULL, 0);
        CHECKF(r == dl + 1, "%s vec %d -1 got %d want %d", tag, i, r, dl + 1);
        if (r == dl + 1) CHECKF(out[dl] == 0 && sc_weq(out, v[i].dst, dl), "%s vec %d -1 content", tag, i);
        /* short destination */
        if (dl >= 1) {
            SET_ERR(0);
            r = U(LCMapStringEx)(v[i].loc, v[i].fl, v[i].src, sl, out, dl - 1 > 0 ? dl - 1 : 0, NULL, NULL, 0);
            if (dl - 1 > 0) { CHECKF(r == 0, "%s vec %d short got %d", tag, i, r); EXPECT_ERR(E_INSUFFICIENT_BUFFER); }
        }
    }
}
#define LCTEST(name, ...) \
    static const lcv_t name##_v[] = { __VA_ARGS__ }; \
    TEST(lcmap_##name) { run_lc(name##_v, ARRAY_N(name##_v), #name); }

LCTEST(lower,
    {INV, LCMAP_LOWERCASE, L"ABC", L"abc"}, {INV, LCMAP_LOWERCASE, L"abc", L"abc"}, {INV, LCMAP_LOWERCASE, L"Hello World 123", L"hello world 123"}, {INV, LCMAP_LOWERCASE, L"\x00c9\x00c0\x00dc", L"\x00e9\x00e0\x00fc"},
    {INV, LCMAP_LOWERCASE, L"\x0391\x0392\x03a3", L"\x03b1\x03b2\x03c3"}, {INV, LCMAP_LOWERCASE, L"\x0410\x042f", L"\x0430\x044f"}, {INV, LCMAP_LOWERCASE, L"\xff21\xff3a", L"\xff41\xff5a"},
    {INV, LCMAP_LOWERCASE, L"\x2160\x216b", L"\x2170\x217b"}, {INV, LCMAP_LOWERCASE, L"\x0100\x017d", L"\x0101\x017e"}, {INV, LCMAP_LOWERCASE, L"\x00df", L"\x00df"}, {INV, LCMAP_LOWERCASE, L"\x4e2d\x6587", L"\x4e2d\x6587"},
    {INV, LCMAP_LOWERCASE, L"[\\]^_`{|}~", L"[\\]^_`{|}~"}, {INV, LCMAP_LOWERCASE, L"@AZ[", L"@az["}, {INV, LCMAP_LOWERCASE, L"\x24b6", L"\x24d0"}, {L"en-US", LCMAP_LOWERCASE, L"ABC", L"abc"})
LCTEST(upper,
    {INV, LCMAP_UPPERCASE, L"abc", L"ABC"}, {INV, LCMAP_UPPERCASE, L"ABC", L"ABC"}, {INV, LCMAP_UPPERCASE, L"Hello World 123", L"HELLO WORLD 123"}, {INV, LCMAP_UPPERCASE, L"\x00e9\x00e0\x00fc", L"\x00c9\x00c0\x00dc"},
    {INV, LCMAP_UPPERCASE, L"\x03b1\x03b2\x03c3", L"\x0391\x0392\x03a3"}, {INV, LCMAP_UPPERCASE, L"\x0430\x044f", L"\x0410\x042f"}, {INV, LCMAP_UPPERCASE, L"\xff41\xff5a", L"\xff21\xff3a"},
    {INV, LCMAP_UPPERCASE, L"\x00ff", L"\x0178"}, {INV, LCMAP_UPPERCASE, L"\x00df", L"\x00df"},
    {INV, LCMAP_UPPERCASE, L"\x4e2d", L"\x4e2d"}, {INV, LCMAP_UPPERCASE, L"@az[", L"@AZ["}, {INV, LCMAP_UPPERCASE, L"\x2170", L"\x2160"}, {L"en-US", LCMAP_UPPERCASE, L"abc", L"ABC"})
LCTEST(kana_width,
    {INV, LCMAP_HIRAGANA, L"\x30a2\x30a4\x30a6", L"\x3042\x3044\x3046"}, {INV, LCMAP_KATAKANA, L"\x3042\x3044\x3046", L"\x30a2\x30a4\x30a6"}, {INV, LCMAP_HIRAGANA, L"\x3042", L"\x3042"}, {INV, LCMAP_KATAKANA, L"\x30a2", L"\x30a2"},
    {INV, LCMAP_HIRAGANA, L"abc\x30ab", L"abc\x304b"}, {INV, LCMAP_KATAKANA, L"abc\x304b", L"abc\x30ab"}, {INV, LCMAP_HALFWIDTH, L"\xff21\xff22\xff11", L"AB1"}, {INV, LCMAP_FULLWIDTH, L"AB1", L"\xff21\xff22\xff11"},
    {INV, LCMAP_HALFWIDTH, L"\x30a2", L"\xff71"}, {INV, LCMAP_FULLWIDTH, L"\xff71", L"\x30a2"}, {INV, LCMAP_FULLWIDTH, L"\xff76\xff9e", L"\x30ac"}, {INV, LCMAP_FULLWIDTH, L"\xff8a\xff9f", L"\x30d1"},
    {INV, LCMAP_HALFWIDTH, L"\x30ac", L"\xff76\xff9e"}, {INV, LCMAP_FULLWIDTH, L" ", L"\x3000"}, {INV, LCMAP_HALFWIDTH, L"\x3000", L" "}, {INV, LCMAP_FULLWIDTH, L"!", L"\xff01"}, {INV, LCMAP_HALFWIDTH, L"\xff01", L"!"},
    {INV, LCMAP_HIRAGANA | LCMAP_FULLWIDTH, L"\xff71", L"\x3042"}, {INV, LCMAP_KATAKANA | LCMAP_HALFWIDTH, L"\x3042", L"\xff71"}, {INV, LCMAP_UPPERCASE | LCMAP_FULLWIDTH, L"ab", L"\xff21\xff22"},
    {INV, LCMAP_LOWERCASE | LCMAP_HALFWIDTH, L"\xff21", L"a"}, {INV, LCMAP_FULLWIDTH, L"\x4e2d", L"\x4e2d"}, {INV, LCMAP_FULLWIDTH, L"\xff21", L"\xff21"}, {INV, LCMAP_HALFWIDTH, L"A", L"A"})
LCTEST(chinese,
    {L"zh-CN", LCMAP_SIMPLIFIED_CHINESE, L"\x6f22\x5b78\x570b", L"\x6c49\x5b66\x56fd"}, {L"zh-CN", LCMAP_TRADITIONAL_CHINESE, L"\x6c49\x5b66\x56fd", L"\x6f22\x5b78\x570b"},
    {L"zh-TW", LCMAP_SIMPLIFIED_CHINESE, L"\x6f22\x5b78\x570b", L"\x6c49\x5b66\x56fd"}, {L"zh-TW", LCMAP_TRADITIONAL_CHINESE, L"\x6c49\x5b66\x56fd", L"\x6f22\x5b78\x570b"},
    {L"zh-CN", LCMAP_SIMPLIFIED_CHINESE, L"\x4e2d\x6587", L"\x4e2d\x6587"}, {L"zh-CN", LCMAP_TRADITIONAL_CHINESE, L"abc\x4e2d", L"abc\x4e2d"})
LCTEST(linguistic_casing,
    {L"tr-TR", LCMAP_LOWERCASE | LCMAP_LINGUISTIC_CASING, L"I", L"\x0131"}, {L"tr-TR", LCMAP_LOWERCASE | LCMAP_LINGUISTIC_CASING, L"\x0130", L"i"}, {L"tr-TR", LCMAP_UPPERCASE | LCMAP_LINGUISTIC_CASING, L"i", L"\x0130"},
    {L"tr-TR", LCMAP_UPPERCASE | LCMAP_LINGUISTIC_CASING, L"\x0131", L"I"}, {L"tr-TR", LCMAP_LOWERCASE | LCMAP_LINGUISTIC_CASING, L"TITLE", L"t\x0131tle"}, {L"tr-TR", LCMAP_UPPERCASE | LCMAP_LINGUISTIC_CASING, L"title", L"T\x0130TLE"},
    {L"az-Latn-AZ", LCMAP_LOWERCASE | LCMAP_LINGUISTIC_CASING, L"I", L"\x0131"}, {L"az-Latn-AZ", LCMAP_UPPERCASE | LCMAP_LINGUISTIC_CASING, L"i", L"\x0130"},
    {L"en-US", LCMAP_LOWERCASE | LCMAP_LINGUISTIC_CASING, L"I", L"i"}, {L"en-US", LCMAP_UPPERCASE | LCMAP_LINGUISTIC_CASING, L"i", L"I"}, {L"tr-TR", LCMAP_LOWERCASE | LCMAP_LINGUISTIC_CASING, L"abc", L"abc"},
    {L"tr-TR", LCMAP_UPPERCASE | LCMAP_LINGUISTIC_CASING, L"abc\x0131", L"ABCI"})
TEST(lcmap_titlecase_note) {
    REQUIRE(U(LCMapStringEx));
    WCHAR o[32]; SET_ERR(0);
    int r = U(LCMapStringEx)(INV, 0x300, L"hello world", -1, o, 32, NULL, NULL, 0);
    t_note("LCMAP_TITLECASE(hello world) -> %d err=%lu [%04X %04X %04X %04X %04X %04X]", r, r ? 0 : GetLastError(), o[0], o[1], o[2], o[3], o[4], o[5]);
    CHECK(r == 12 || r == 0);
    SET_ERR(0);
    r = U(LCMapStringEx)(INV, LCMAP_UPPERCASE, L"\x00b5\x0131\x017f", -1, o, 32, NULL, NULL, 0);
    t_note("upper(micro, dotless i, long s) -> %d: %04X %04X %04X", r, o[0], o[1], o[2]);
}
TEST(lcmap_byterev) {
    REQUIRE(U(LCMapStringEx));
    WCHAR out[8];
    static const WCHAR in[] = W_(0x3450, 0x4822, 0x0061, 0x6100), exp[] = W_(0x5034, 0x2248, 0x6100, 0x0061);
    SET_ERR(0);
    int r = U(LCMapStringEx)(INV, LCMAP_BYTEREV, in, 4, out, 8, NULL, NULL, 0);
    t_note("LCMAP_BYTEREV alone -> %d err=%lu", r, r ? 0 : GetLastError());
    if (r == 4) CHECK_WEQ(out, exp, 4);
    r = U(LCMapStringEx)(INV, LCMAP_BYTEREV | LCMAP_LOWERCASE, L"AB", 2, out, 8, NULL, NULL, 0);
    t_note("LCMAP_BYTEREV|LOWERCASE -> %d: %04X %04X", r, out[0], out[1]);
}

/* ---- parameter validation ---- */
TEST(lcmap_params) {
    REQUIRE(U(LCMapStringEx)); REQUIRE(U(LCMapStringW));
    WCHAR out[8];
    if (!t_is_wine()) {
        SET_ERR(0); EXPECT_EQ(U(LCMapStringEx)(INV, LCMAP_UPPERCASE, NULL, 1, out, 8, NULL, NULL, 0), 0); EXPECT_ERR(E_INVALID_PARAMETER);
        SET_ERR(0); EXPECT_EQ(U(LCMapStringEx)(INV, LCMAP_UPPERCASE, NULL, -1, NULL, 0, NULL, NULL, 0), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    }
    SET_ERR(0); EXPECT_EQ(U(LCMapStringEx)(INV, LCMAP_UPPERCASE, L"a", 0, out, 8, NULL, NULL, 0), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(LCMapStringEx)(INV, LCMAP_UPPERCASE, L"a", 1, out, -1, NULL, NULL, 0), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    if (!t_is_wine()) { SET_ERR(0); EXPECT_EQ(U(LCMapStringEx)(INV, LCMAP_UPPERCASE, L"a", 1, NULL, 1, NULL, NULL, 0), 0); EXPECT_ERR(E_INVALID_PARAMETER); }
    SET_ERR(0); EXPECT_EQ(U(LCMapStringEx)(L"xx-INVALID-yy", LCMAP_UPPERCASE, L"a", 1, out, 8, NULL, NULL, 0), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(LCMapStringW)(0xdeadbeef, LCMAP_UPPERCASE, L"a", 1, out, 8), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    if (!t_is_wine()) { SET_ERR(0); EXPECT_EQ(U(LCMapStringW)(LOCALE_INVARIANT, LCMAP_UPPERCASE, NULL, 1, out, 8), 0); EXPECT_ERR(E_INVALID_PARAMETER); }
    SET_ERR(0); EXPECT_EQ(U(LCMapStringW)(LOCALE_INVARIANT, LCMAP_UPPERCASE, L"a", 0, out, 8), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(LCMapStringW)(LOCALE_INVARIANT, LCMAP_UPPERCASE, L"a", 1, out, 8), 1); EXPECT_EQ(out[0], 'A');
    SET_ERR(0); EXPECT_EQ(U(LCMapStringW)(LOCALE_INVARIANT, LCMAP_UPPERCASE, L"a", 1, NULL, 0), 1);
}
TEST(lcmap_invalid_flags) {
    REQUIRE(U(LCMapStringEx));
    WCHAR out[8];
    static const DWORD bad[] = {0, LCMAP_LOWERCASE | LCMAP_SIMPLIFIED_CHINESE | LCMAP_TRADITIONAL_CHINESE, LCMAP_HIRAGANA | LCMAP_KATAKANA, LCMAP_HALFWIDTH | LCMAP_FULLWIDTH,
        LCMAP_SIMPLIFIED_CHINESE | LCMAP_TRADITIONAL_CHINESE, 0x80000000, 0xffffffff, LCMAP_SORTKEY | LCMAP_UPPERCASE, LCMAP_SORTKEY | LCMAP_LOWERCASE, LCMAP_SORTKEY | LCMAP_HALFWIDTH};
    for (int i = 0; i < ARRAY_N(bad); i++) {
        SET_ERR(0);
        int r = U(LCMapStringEx)(INV, bad[i], L"a", 1, out, 8, NULL, NULL, 0);
        CHECKF(r == 0 && GetLastError() == E_INVALID_FLAGS, "flags %lx r=%d err=%lu", bad[i], r, GetLastError());
    }
}
TEST(lcmap_buffer_semantics) {
    REQUIRE(U(LCMapStringW));
    WCHAR out[8]; for (int i = 0; i < 8; i++) out[i] = 0x5555;
    SET_ERR(0);
    EXPECT_EQ(U(LCMapStringW)(LOCALE_INVARIANT, LCMAP_UPPERCASE, L"abcd", 4, out, 3), 0); EXPECT_ERR(E_INSUFFICIENT_BUFFER);
    SET_ERR(0);
    EXPECT_EQ(U(LCMapStringW)(LOCALE_INVARIANT, LCMAP_UPPERCASE, L"abcd", -1, out, 4), 0); EXPECT_ERR(E_INSUFFICIENT_BUFFER);
    EXPECT_EQ(U(LCMapStringW)(LOCALE_INVARIANT, LCMAP_UPPERCASE, L"abcd", -1, out, 5), 5);
    EXPECT_EQ(U(LCMapStringW)(LOCALE_INVARIANT, LCMAP_UPPERCASE, L"abcd", 4, out, 4), 4);
    EXPECT_EQ(U(LCMapStringW)(LOCALE_INVARIANT, LCMAP_UPPERCASE, L"abcd", 4, NULL, 0), 4);
    EXPECT_EQ(U(LCMapStringW)(LOCALE_INVARIANT, LCMAP_UPPERCASE, L"abcd", -1, NULL, 0), 5);
    EXPECT_EQ(U(LCMapStringW)(LOCALE_INVARIANT, LCMAP_UPPERCASE, L"abcdef", 2, out, 8), 2);
    CHECK(out[0] == 'A' && out[1] == 'B');
}
TEST(lcmap_in_place_case_mapping) {
    REQUIRE(U(LCMapStringW));
    WCHAR b[8] = W_('a', 'b', 'c', 0);
    EXPECT_EQ(U(LCMapStringW)(LOCALE_INVARIANT, LCMAP_UPPERCASE, b, 4, b, 8), 4);
    CHECK(b[0] == 'A' && b[1] == 'B' && b[2] == 'C' && b[3] == 0);
    EXPECT_EQ(U(LCMapStringW)(LOCALE_INVARIANT, LCMAP_LOWERCASE, b, -1, b, 8), 4);
    CHECK(b[0] == 'a' && b[2] == 'c');
}
TEST(lcmap_embedded_nul_preserved) {
    REQUIRE(U(LCMapStringW));
    WCHAR out[8];
    static const WCHAR in[] = W_('a', 0, 'b');
    EXPECT_EQ(U(LCMapStringW)(LOCALE_INVARIANT, LCMAP_UPPERCASE, in, 3, out, 8), 3);
    CHECK(out[0] == 'A' && out[1] == 0 && out[2] == 'B');
    EXPECT_EQ(U(LCMapStringW)(LOCALE_INVARIANT, LCMAP_UPPERCASE, in, -1, out, 8), 2);
}
TEST(lcmap_lcid_locales) {
    REQUIRE(U(LCMapStringW));
    static const LCID l[] = {LOCALE_INVARIANT, 0x0409, LOCALE_USER_DEFAULT, LOCALE_SYSTEM_DEFAULT, 0x0407, 0x040c, 0x0411, 0x0804, LOCALE_NEUTRAL, 0x0c00};
    for (int i = 0; i < ARRAY_N(l); i++) {
        WCHAR o[8];
        SET_ERR(0);
        int r = U(LCMapStringW)(l[i], LCMAP_UPPERCASE, L"abc", -1, o, 8);
        CHECKF(r == 4 && o[0] == 'A' && o[2] == 'C', "lcid %lx r=%d err=%lu", l[i], r, GetLastError());
        r = U(LCMapStringW)(l[i], LCMAP_LOWERCASE, L"ABC", 3, o, 8);
        CHECKF(r == 3 && o[0] == 'a', "lcid %lx lower r=%d", l[i], r);
    }
}
TEST(lcmap_string_a) {
    REQUIRE(U(LCMapStringA));
    char o[16];
    EXPECT_EQ(U(LCMapStringA)(LOCALE_INVARIANT, LCMAP_UPPERCASE, "hello", -1, o, 16), 6); EXPECT_STR(o, "HELLO");
    EXPECT_EQ(U(LCMapStringA)(LOCALE_INVARIANT, LCMAP_LOWERCASE, "HeLLo", 5, o, 16), 5); CHECK(!memcmp(o, "hello", 5));
    EXPECT_EQ(U(LCMapStringA)(LOCALE_INVARIANT, LCMAP_UPPERCASE, "hello", 5, NULL, 0), 5);
    SET_ERR(0); EXPECT_EQ(U(LCMapStringA)(LOCALE_INVARIANT, LCMAP_UPPERCASE, "hello", 5, o, 3), 0); EXPECT_ERR(E_INSUFFICIENT_BUFFER);
    SET_ERR(0); EXPECT_EQ(U(LCMapStringA)(LOCALE_INVARIANT, LCMAP_UPPERCASE, NULL, 5, o, 16), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(LCMapStringA)(LOCALE_INVARIANT, 0, "a", 1, o, 16), 0); EXPECT_ERR(E_INVALID_FLAGS);
    SET_ERR(0); EXPECT_EQ(U(LCMapStringA)(0xdeadbeef, LCMAP_UPPERCASE, "a", 1, o, 16), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    if (U(GetACP) && U(GetACP)() == 1252) {
        EXPECT_EQ(U(LCMapStringA)(LOCALE_INVARIANT, LCMAP_UPPERCASE, "\xe9\xe0", 2, o, 16), 2); CHECK(!memcmp(o, "\xc9\xc0", 2));
        EXPECT_EQ(U(LCMapStringA)(LOCALE_INVARIANT, LCMAP_LOWERCASE, "\xc9\xc0", 2, o, 16), 2); CHECK(!memcmp(o, "\xe9\xe0", 2));
        EXPECT_EQ(U(LCMapStringA)(LOCALE_INVARIANT, LCMAP_UPPERCASE, "\xff", 1, o, 16), 1);
        t_note("LCMapStringA upper of 0xFF -> %02X", (unsigned char)o[0]);
        EXPECT_EQ(U(LCMapStringA)(LOCALE_INVARIANT, LCMAP_UPPERCASE, "\xdf", 1, o, 16), 1); EXPECT_EQ((unsigned char)o[0], 0xDF);
    }
}
TEST(lcmap_sortkey_basic) {
    REQUIRE(U(LCMapStringW));
    BYTE k1[256], k2[256];
    int n1 = U(LCMapStringW)(LOCALE_INVARIANT, LCMAP_SORTKEY, L"abc", -1, NULL, 0);
    CHECK(n1 > 0);
    int n = U(LCMapStringW)(LOCALE_INVARIANT, LCMAP_SORTKEY, L"abc", -1, (LPWSTR)k1, sizeof k1);
    EXPECT_EQ(n, n1);
    int n2 = U(LCMapStringW)(LOCALE_INVARIANT, LCMAP_SORTKEY, L"abc", 3, (LPWSTR)k2, sizeof k2);
    EXPECT_EQ(n2, n1);
    CHECK(n2 == n1 && !memcmp(k1, k2, n1));
    SET_ERR(0);
    EXPECT_EQ(U(LCMapStringW)(LOCALE_INVARIANT, LCMAP_SORTKEY, L"abc", 3, (LPWSTR)k2, n1 - 1), 0); EXPECT_ERR(E_INSUFFICIENT_BUFFER);
    /* key of an empty string is still produced */
    int ne = U(LCMapStringW)(LOCALE_INVARIANT, LCMAP_SORTKEY, L"", 1, NULL, 0);
    CHECK(ne > 0);
    t_note("sortkey(abc) len=%d first bytes %02X %02X %02X %02X", n1, k1[0], k1[1], k1[2], k1[3]);
}
static int key_cmp(LCID lc, DWORD fl, const WCHAR *a, const WCHAR *b, int *ok) {
    BYTE ka[512], kb[512];
    int na = U(LCMapStringW)(lc, LCMAP_SORTKEY | fl, a, -1, (LPWSTR)ka, sizeof ka), nb = U(LCMapStringW)(lc, LCMAP_SORTKEY | fl, b, -1, (LPWSTR)kb, sizeof kb);
    *ok = na > 0 && nb > 0;
    if (!*ok) return 0;
    int m = na < nb ? na : nb, c = memcmp(ka, kb, m);
    if (c == 0) c = na - nb;
    return c < 0 ? 1 : c > 0 ? 3 : 2;
}
TEST(lcmap_sortkey_matches_comparestring) {
    REQUIRE(U(LCMapStringW)); REQUIRE(U(CompareStringW));
    static const WCHAR *s[] = {L"a", L"A", L"b", L"B", L"ab", L"Ab", L"aB", L"abc", L"abd", L"ABC", L"a b", L"ab ", L"1", L"2", L"10", L"z", L"Z", L"zz", L"hello", L"Hello", L"HELLO", L"help", L"", L"\x00e9", L"e", L"f", L"\x00c9", L"co-op", L"coop", L"con", L"it's", L"its", L"\x4e2d", L"\x3042"};
    static const DWORD f[] = {0, NORM_IGNORECASE, NORM_IGNORENONSPACE, NORM_IGNORECASE | NORM_IGNORENONSPACE, SORT_STRINGSORT, NORM_IGNORESYMBOLS, NORM_IGNOREWIDTH, NORM_IGNOREKANATYPE};
    int mism = 0, total = 0;
    for (int k = 0; k < ARRAY_N(f); k++) for (int i = 0; i < ARRAY_N(s); i++) for (int j = 0; j < ARRAY_N(s); j++) {
        int ok; int kc = key_cmp(LOCALE_INVARIANT, f[k], s[i], s[j], &ok);
        CHECKF(ok, "key generation failed flags %lx i=%d j=%d err=%lu", f[k], i, j, GetLastError());
        if (!ok) continue;
        int cc = U(CompareStringW)(LOCALE_INVARIANT, f[k], s[i], -1, s[j], -1);
        total++;
        if (kc != cc) { mism++; if (mism <= 5) t_note("flags %lx i=%d j=%d key=%d cmp=%d", f[k], i, j, kc, cc); }
    }
    CHECKF(mism == 0, "%d of %d sort-key orders disagree with CompareString", mism, total);
}
TEST(lcmap_sortkey_ignorecase_equal) {
    REQUIRE(U(LCMapStringW));
    int ok;
    EXPECT_EQ(key_cmp(LOCALE_INVARIANT, NORM_IGNORECASE, L"Hello", L"hELLO", &ok), 2); CHECK(ok);
    EXPECT_EQ(key_cmp(LOCALE_INVARIANT, 0, L"Hello", L"hELLO", &ok), 3); CHECK(ok);   /* H > h */
    EXPECT_EQ(key_cmp(LOCALE_INVARIANT, NORM_IGNORENONSPACE, L"e", L"\x00e9", &ok), 2); CHECK(ok);
    EXPECT_EQ(key_cmp(LOCALE_INVARIANT, NORM_IGNOREWIDTH, L"A", L"\xff21", &ok), 2); CHECK(ok);
}
TEST(lcmap_sortkey_ascii_order) {
    REQUIRE(U(LCMapStringW));
    /* strictly increasing for a sorted ASCII alphabet (case-insensitively) */
    static const WCHAR *s[] = {L"a", L"b", L"c", L"d", L"e", L"f", L"g", L"h", L"i", L"j", L"k", L"l", L"m", L"n", L"o", L"p", L"q", L"r", L"s", L"t", L"u", L"v", L"w", L"x", L"y", L"z"};
    for (int i = 0; i + 1 < ARRAY_N(s); i++) { int ok; CHECKF(key_cmp(LOCALE_INVARIANT, NORM_IGNORECASE, s[i], s[i + 1], &ok) == 1 && ok, "i=%d", i); }
    static const WCHAR *d[] = {L"0", L"1", L"2", L"3", L"4", L"5", L"6", L"7", L"8", L"9"};
    for (int i = 0; i + 1 < ARRAY_N(d); i++) { int ok; CHECKF(key_cmp(LOCALE_INVARIANT, 0, d[i], d[i + 1], &ok) == 1 && ok, "digit i=%d", i); }
    int ok; CHECK(key_cmp(LOCALE_INVARIANT, 0, L"9", L"a", &ok) == 1 && ok);
}
TEST(lcmap_sortkey_a) {
    REQUIRE(U(LCMapStringA));
    BYTE k[256];
    int n = U(LCMapStringA)(LOCALE_INVARIANT, LCMAP_SORTKEY, "abc", -1, NULL, 0);
    CHECK(n > 0);
    EXPECT_EQ(U(LCMapStringA)(LOCALE_INVARIANT, LCMAP_SORTKEY, "abc", -1, (LPSTR)k, sizeof k), n);
    BYTE kw[256];
    int nw = U(LCMapStringW)(LOCALE_INVARIANT, LCMAP_SORTKEY, L"abc", -1, (LPWSTR)kw, sizeof kw);
    CHECK(nw == n && !memcmp(k, kw, n));
}
TEST(lcmap_sortkey_swedish) {
    REQUIRE(U(LCMapStringEx));
    BYTE ka[128], kb[128];
    int na = U(LCMapStringEx)(L"sv-SE", LCMAP_SORTKEY, L"z", -1, (LPWSTR)ka, sizeof ka, NULL, NULL, 0), nb = U(LCMapStringEx)(L"sv-SE", LCMAP_SORTKEY, L"\x00e5", -1, (LPWSTR)kb, sizeof kb, NULL, NULL, 0);
    CHECK(na > 0 && nb > 0);
    if (na > 0 && nb > 0) CHECK(memcmp(ka, kb, na < nb ? na : nb) < 0);
}

/* ---- GetStringType ---- */
static WORD ct(DWORD type, WCHAR c) { WORD r = 0xFFFF; U(GetStringTypeW)(type, &c, 1, &r); return r; }
TEST(strtype_ctype1_ascii_table) {
    REQUIRE(U(GetStringTypeW));
    int bad = 0;
    for (int c = 0; c < 128; c++) {
        WORD want;
        if (c >= 'A' && c <= 'F') want = C1_UPPER | C1_XDIGIT | C1_ALPHA;
        else if (c >= 'G' && c <= 'Z') want = C1_UPPER | C1_ALPHA;
        else if (c >= 'a' && c <= 'f') want = C1_LOWER | C1_XDIGIT | C1_ALPHA;
        else if (c >= 'g' && c <= 'z') want = C1_LOWER | C1_ALPHA;
        else if (c >= '0' && c <= '9') want = C1_DIGIT | C1_XDIGIT;
        else if (c == ' ') want = C1_SPACE | C1_BLANK;
        else if (c == '\t') want = C1_CNTRL | C1_SPACE | C1_BLANK;
        else if (c >= 0x0A && c <= 0x0D) want = C1_CNTRL | C1_SPACE;
        else if (c < 0x20 || c == 0x7f) want = C1_CNTRL;
        else want = C1_PUNCT;
        WORD got = ct(CT_CTYPE1, (WCHAR)c) & 0x1FF;
        if (got != want) { bad++; if (bad < 8) t_note("ctype1[%02X] got %03X want %03X", c, got, want); }
        CHECKF(got == want, "ctype1[%02X] got %03X want %03X", c, got, want);
    }
}
TEST(strtype_ctype1_unicode) {
    REQUIRE(U(GetStringTypeW));
    struct { WCHAR c; WORD must; WORD mustnot; } v[] = {
        {0x00e9, C1_LOWER | C1_ALPHA, C1_UPPER | C1_DIGIT}, {0x00c9, C1_UPPER | C1_ALPHA, C1_LOWER}, {0x03b1, C1_LOWER | C1_ALPHA, C1_UPPER}, {0x0391, C1_UPPER | C1_ALPHA, C1_LOWER},
        {0x0430, C1_LOWER | C1_ALPHA, C1_UPPER}, {0x0410, C1_UPPER | C1_ALPHA, C1_LOWER}, {0x4e2d, C1_ALPHA, C1_UPPER | C1_LOWER | C1_DIGIT}, {0x3042, C1_ALPHA, C1_UPPER | C1_LOWER},
        {0x00a0, C1_SPACE, C1_ALPHA}, {0x3000, C1_SPACE, C1_ALPHA}, {0x2003, C1_SPACE, C1_ALPHA}, {0x0660, C1_DIGIT, C1_ALPHA}, {0x0966, C1_DIGIT, C1_ALPHA}, {0xff10, C1_DIGIT, C1_ALPHA},
        {0xff21, C1_UPPER | C1_ALPHA, C1_LOWER}, {0xff41, C1_LOWER | C1_ALPHA, C1_UPPER}, {0x00b2, C1_DIGIT, C1_ALPHA}, {0x00a1, C1_PUNCT, C1_ALPHA}, {0x2014, C1_PUNCT, C1_ALPHA},
        {0x0085, C1_CNTRL, C1_ALPHA}, {0x0080, C1_CNTRL, C1_ALPHA}, {0x009f, C1_CNTRL, C1_ALPHA}, {0x00df, C1_LOWER | C1_ALPHA, C1_UPPER}, {0x0131, C1_LOWER | C1_ALPHA, C1_UPPER},
        {0x0301, 0, C1_UPPER | C1_LOWER | C1_DIGIT}, {0x05d0, C1_ALPHA, C1_UPPER | C1_LOWER}, {0x0627, C1_ALPHA, C1_UPPER | C1_LOWER},
    };
    for (int i = 0; i < ARRAY_N(v); i++) {
        WORD g = ct(CT_CTYPE1, v[i].c);
        CHECKF((g & v[i].must) == v[i].must && !(g & v[i].mustnot), "U+%04X ctype1 %03X must %03X mustnot %03X", v[i].c, g, v[i].must, v[i].mustnot);
    }
}
TEST(strtype_ctype2_bidi) {
    REQUIRE(U(GetStringTypeW));
    static const struct { WCHAR c; WORD t; } v[] = {
        {'a', C2_LEFTTORIGHT}, {'Z', C2_LEFTTORIGHT}, {0x00e9, C2_LEFTTORIGHT}, {0x4e2d, C2_LEFTTORIGHT}, {0x0430, C2_LEFTTORIGHT}, {0x05d0, C2_RIGHTTOLEFT}, {0x0627, C2_RIGHTTOLEFT},
        {'0', C2_EUROPENUMBER}, {'9', C2_EUROPENUMBER}, {0x0660, C2_ARABICNUMBER}, {0x0669, C2_ARABICNUMBER}, {' ', C2_WHITESPACE}, {'+', C2_EUROPESEPARATOR}, {'-', C2_EUROPESEPARATOR},
        {'#', C2_EUROPETERMINATOR}, {'$', C2_EUROPETERMINATOR}, {'%', C2_EUROPETERMINATOR}, {',', C2_COMMONSEPARATOR}, {'.', C2_COMMONSEPARATOR}, {'/', C2_COMMONSEPARATOR}, {':', C2_COMMONSEPARATOR},
        {'\t', C2_SEGMENTSEPARATOR}, {'\n', C2_BLOCKSEPARATOR}, {'\r', C2_BLOCKSEPARATOR}, {'!', C2_OTHERNEUTRAL}, {'"', C2_OTHERNEUTRAL}, {'(', C2_OTHERNEUTRAL}, {'@', C2_OTHERNEUTRAL}, {'[', C2_OTHERNEUTRAL}, {'~', C2_OTHERNEUTRAL},
    };
    for (int i = 0; i < ARRAY_N(v); i++) {
        WORD g = ct(CT_CTYPE2, v[i].c);
        CHECKF(g == v[i].t, "U+%04X ctype2 got %X want %X", v[i].c, g, v[i].t);
    }
}
TEST(strtype_ctype3) {
    REQUIRE(U(GetStringTypeW));
    static const struct { WCHAR c; WORD must; WORD mustnot; } v[] = {
        {0x0301, C3_NONSPACING | C3_DIACRITIC, C3_ALPHA}, {0x0300, C3_NONSPACING, 0}, {0x3042, C3_HIRAGANA, C3_KATAKANA}, {0x30a2, C3_KATAKANA, C3_HIRAGANA}, {0xff71, C3_KATAKANA | C3_HALFWIDTH, C3_HIRAGANA},
        {0xff21, C3_FULLWIDTH, C3_HALFWIDTH}, {0x4e2d, C3_IDEOGRAPH, C3_KATAKANA | C3_HIRAGANA}, {'$', C3_SYMBOL, C3_ALPHA}, {'a', C3_ALPHA, C3_NONSPACING | C3_SYMBOL}, {'Z', C3_ALPHA, C3_NONSPACING},
        {0x0640, C3_KASHIDA, 0}, {0x00e9, C3_ALPHA, 0}, {0x0e31, C3_NONSPACING | C3_VOWELMARK, 0}, {'+', C3_SYMBOL, C3_ALPHA}, {'=', C3_SYMBOL, C3_ALPHA}, {'<', C3_SYMBOL, C3_ALPHA},
        {0x00a9, C3_SYMBOL, C3_ALPHA}, {0xff71, C3_ALPHA, 0}, {'5', 0, C3_NONSPACING | C3_SYMBOL},
    };
    for (int i = 0; i < ARRAY_N(v); i++) {
        WORD g = ct(CT_CTYPE3, v[i].c);
        CHECKF((g & v[i].must) == v[i].must && !(g & v[i].mustnot), "U+%04X ctype3 %04X must %04X mustnot %04X", v[i].c, g, v[i].must, v[i].mustnot);
    }
}
TEST(strtype_w_cch_semantics) {
    REQUIRE(U(GetStringTypeW));
    WORD out[8]; for (int i = 0; i < 8; i++) out[i] = 0xAAAA;
    EXPECT_EQ(U(GetStringTypeW)(CT_CTYPE1, L"aB1", 3, out), TRUE);
    CHECK((out[0] & 0x1FF) == (C1_LOWER | C1_XDIGIT | C1_ALPHA) && (out[1] & 0x1FF) == (C1_UPPER | C1_XDIGIT | C1_ALPHA) && (out[2] & 0x1FF) == (C1_DIGIT | C1_XDIGIT));
    CHECK(out[3] == 0xAAAA);
    for (int i = 0; i < 8; i++) out[i] = 0xAAAA;
    EXPECT_EQ(U(GetStringTypeW)(CT_CTYPE1, L"aB1", -1, out), TRUE);
    CHECK(out[3] != 0xAAAA && (out[3] & 0x1FF) == C1_CNTRL);
    CHECK(out[4] == 0xAAAA);
    for (int i = 0; i < 8; i++) out[i] = 0xAAAA;
    static const WCHAR emb[] = W_('a', 0, 'b');
    EXPECT_EQ(U(GetStringTypeW)(CT_CTYPE1, emb, 3, out), TRUE);
    CHECK((out[1] & 0x1FF) == C1_CNTRL);
    EXPECT_EQ(U(GetStringTypeW)(CT_CTYPE1, L"x", 1, out), TRUE);
}
TEST(strtype_w_errors) {
    REQUIRE(U(GetStringTypeW));
    WORD out[8];
    if (!t_is_wine()) { SET_ERR(0); EXPECT_EQ(U(GetStringTypeW)(CT_CTYPE1, NULL, 1, out), 0); EXPECT_ERR(E_INVALID_PARAMETER); }
    SET_ERR(0); EXPECT_EQ(U(GetStringTypeW)(CT_CTYPE1, L"a", 0, out), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    if (!t_is_wine()) { SET_ERR(0); EXPECT_EQ(U(GetStringTypeW)(CT_CTYPE1, L"a", 1, NULL), 0); EXPECT_ERR(E_INVALID_PARAMETER); }
    WCHAR same[4] = W_('a', 'b', 0, 0);
    SET_ERR(0); EXPECT_EQ(U(GetStringTypeW)(CT_CTYPE1, same, 2, (LPWORD)same), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    static const DWORD bad[] = {0, 3, 5, 7, 8, 0x10, 0xffffffff, 0x80000000};
    for (int i = 0; i < ARRAY_N(bad); i++) { SET_ERR(0); int r = U(GetStringTypeW)(bad[i], L"a", 1, out); CHECKF(r == 0 && GetLastError() == E_INVALID_FLAGS, "type %lx r=%d err=%lu", bad[i], r, GetLastError()); }
}
TEST(strtype_exw_matches_w) {
    REQUIRE(U(GetStringTypeExW)); REQUIRE(U(GetStringTypeW));
    static const LCID l[] = {LOCALE_INVARIANT, 0x0409, LOCALE_USER_DEFAULT, LOCALE_SYSTEM_DEFAULT, 0x0407, 0x0411};
    static const WCHAR s[] = W_('A', 'z', '5', ' ', '!', 0x00e9, 0x4e2d, 0x0301, 0);
    for (int i = 0; i < ARRAY_N(l); i++) for (DWORD t = 1; t <= 4; t = t == 2 ? 4 : t + 1) {
        WORD a[16], b[16]; memset(a, 0xAB, sizeof a); memset(b, 0xCD, sizeof b);
        SET_ERR(0);
        BOOL ra = U(GetStringTypeExW)(l[i], t, s, 8, a), rb = U(GetStringTypeW)(t, s, 8, b);
        CHECKF(ra && rb, "lcid %lx type %lu err=%lu", l[i], t, GetLastError());
        if (ra && rb) CHECKF(!memcmp(a, b, 16), "lcid %lx type %lu differ", l[i], t);
    }
    WORD o[4];
    SET_ERR(0); int rr = U(GetStringTypeExW)(0xdeadbeef, CT_CTYPE1, L"a", 1, o);
    t_note("GetStringTypeExW(invalid lcid) = %d err=%lu", rr, GetLastError());
    SET_ERR(0); EXPECT_EQ(U(GetStringTypeExW)(LOCALE_INVARIANT, 9, L"a", 1, o), 0); EXPECT_ERR(E_INVALID_FLAGS);
    if (!t_is_wine()) { SET_ERR(0); EXPECT_EQ(U(GetStringTypeExW)(LOCALE_INVARIANT, CT_CTYPE1, NULL, 1, o), 0); EXPECT_ERR(E_INVALID_PARAMETER); }
    SET_ERR(0); EXPECT_EQ(U(GetStringTypeExW)(LOCALE_INVARIANT, CT_CTYPE1, L"a", 0, o), 0); EXPECT_ERR(E_INVALID_PARAMETER);
}
TEST(strtype_a_variants) {
    REQUIRE(U(GetStringTypeA)); REQUIRE(U(GetStringTypeExA));
    WORD a[8], b[8];
    for (int i = 0; i < 8; i++) a[i] = b[i] = 0xEEEE;
    EXPECT_EQ(U(GetStringTypeA)(LOCALE_INVARIANT, CT_CTYPE1, "aB1 !", 5, a), TRUE);
    CHECK((a[0] & 0x1FF) == (C1_LOWER | C1_XDIGIT | C1_ALPHA) && (a[1] & 0x1FF) == (C1_UPPER | C1_XDIGIT | C1_ALPHA) && (a[2] & 0x1FF) == (C1_DIGIT | C1_XDIGIT));
    CHECK((a[3] & 0x1FF) == (C1_SPACE | C1_BLANK) && (a[4] & 0x1FF) == C1_PUNCT);
    CHECK(a[5] == 0xEEEE);
    EXPECT_EQ(U(GetStringTypeExA)(LOCALE_INVARIANT, CT_CTYPE1, "aB1 !", 5, b), TRUE);
    CHECK(!memcmp(a, b, 10));
    EXPECT_EQ(U(GetStringTypeA)(LOCALE_INVARIANT, CT_CTYPE1, "ab", -1, a), TRUE);
    CHECK((a[2] & 0x1FF) == C1_CNTRL);
    EXPECT_EQ(U(GetStringTypeA)(LOCALE_INVARIANT, CT_CTYPE2, "a1", 2, a), TRUE);
    CHECK(a[0] == C2_LEFTTORIGHT && a[1] == C2_EUROPENUMBER);
    EXPECT_EQ(U(GetStringTypeA)(LOCALE_INVARIANT, CT_CTYPE3, "a$", 2, a), TRUE);
    CHECK((a[0] & C3_ALPHA) && (a[1] & C3_SYMBOL));
    if (!t_is_wine()) {
        SET_ERR(0); EXPECT_EQ(U(GetStringTypeA)(LOCALE_INVARIANT, CT_CTYPE1, NULL, 1, a), 0); EXPECT_ERR(E_INVALID_PARAMETER);
        SET_ERR(0); EXPECT_EQ(U(GetStringTypeA)(LOCALE_INVARIANT, CT_CTYPE1, "a", 0, a), 0); EXPECT_ERR(E_INVALID_PARAMETER);
        SET_ERR(0); EXPECT_EQ(U(GetStringTypeA)(LOCALE_INVARIANT, CT_CTYPE1, "a", 1, NULL), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    }
    SET_ERR(0); int r9 = U(GetStringTypeA)(LOCALE_INVARIANT, 9, "a", 1, a);
    t_note("GetStringTypeA(type 9) = %d err=%lu", r9, GetLastError());
    CHECK(r9 == 0);
    if (U(GetACP) && U(GetACP)() == 1252) {
        EXPECT_EQ(U(GetStringTypeA)(0x0409, CT_CTYPE1, "\xe9\xc9\xa0", 3, a), TRUE);
        CHECK((a[0] & (C1_LOWER | C1_ALPHA)) == (C1_LOWER | C1_ALPHA) && (a[1] & (C1_UPPER | C1_ALPHA)) == (C1_UPPER | C1_ALPHA) && (a[2] & C1_SPACE));
    }
    DWORD dbcs = 0;
}
TEST(strtype_a_dbcs_pairs) {
    REQUIRE(U(GetStringTypeA));
    if (!U(IsValidCodePage) || !U(IsValidCodePage)(932)) { t_skip("cp932 absent"); return; }
    /* LCID with Japanese ANSI code page: one result per character, not per byte */
    WORD w[8]; memset(w, 0xFF, sizeof w);
    BOOL r = U(GetStringTypeA)(0x0411, CT_CTYPE1, "a\x82\xa0" "b", 4, w);
    t_note("GetStringTypeA(ja-JP, 4 bytes) = %d out[0..3]=%04X %04X %04X %04X", r, w[0], w[1], w[2], w[3]);
    CHECK(r);
}

/* ---- FoldString ---- */
typedef struct { DWORD fl; const WCHAR *src; const WCHAR *dst; } fsv_t;
static void run_fold(const fsv_t *v, int n, const char *tag) {
    if (!U(FoldStringW)) { t_skip("missing export: FoldStringW"); return; }
    for (int i = 0; i < n; i++) {
        WCHAR out[64]; int sl = lstrlenW(v[i].src), dl = lstrlenW(v[i].dst);
        SET_ERR(0);
        int q = U(FoldStringW)(v[i].fl, v[i].src, sl, NULL, 0);
        CHECKF(q == dl, "%s vec %d query got %d want %d (err %lu)", tag, i, q, dl, GetLastError());
        for (int k = 0; k < 64; k++) out[k] = 0x7777;
        int r = U(FoldStringW)(v[i].fl, v[i].src, sl, out, 64);
        CHECKF(r == dl, "%s vec %d got %d want %d", tag, i, r, dl);
        if (r == dl) { char g[256], e[256]; sc_wdump(g, sizeof g, out, r); sc_wdump(e, sizeof e, v[i].dst, dl); CHECKF(sc_weq(out, v[i].dst, dl) && out[dl] == 0x7777, "%s vec %d content got [%s] want [%s]", tag, i, g, e); }
        r = U(FoldStringW)(v[i].fl, v[i].src, -1, out, 64);
        CHECKF(r == dl + 1 && out[dl] == 0, "%s vec %d -1 got %d", tag, i, r);
        if (dl >= 2) { SET_ERR(0); r = U(FoldStringW)(v[i].fl, v[i].src, sl, out, dl - 1); CHECKF(r == 0, "%s vec %d short got %d", tag, i, r); EXPECT_ERR(E_INSUFFICIENT_BUFFER); }
    }
}
#define FSTEST(name, ...) \
    static const fsv_t name##_v[] = { __VA_ARGS__ }; \
    TEST(fold_##name) { run_fold(name##_v, ARRAY_N(name##_v), #name); }

FSTEST(czone,
    {MAP_FOLDCZONE, L"\xff21\xff22\xff23", L"ABC"}, {MAP_FOLDCZONE, L"\xff41\xff5a", L"az"}, {MAP_FOLDCZONE, L"\xff10\xff19", L"09"}, {MAP_FOLDCZONE, L"ABC", L"ABC"}, {MAP_FOLDCZONE, L"\xff01", L"!"},
    {MAP_FOLDCZONE, L"x\xff21y", L"xAy"}, {MAP_FOLDCZONE, L"\x4e2d", L"\x4e2d"}, {MAP_FOLDCZONE, L"\x2460", L"1"}, {MAP_FOLDCZONE, L"\x2163", L"IV"}, {MAP_FOLDCZONE, L"\x00a0", L" "},
    {MAP_FOLDCZONE, L"\xff71", L"\x30a2"}, {MAP_FOLDCZONE, L"\xfb01", L"fi"}, {MAP_FOLDCZONE, L"\xfb03", L"ffi"}, {MAP_FOLDCZONE, L"\xff76\xff9e", L"\x30ac"}, {MAP_FOLDCZONE, L"e\x0301", L"\x00e9"})
FSTEST(digits,
    {MAP_FOLDDIGITS, L"\x0660\x0661\x0669", L"019"}, {MAP_FOLDDIGITS, L"\x0966\x0969", L"03"}, {MAP_FOLDDIGITS, L"\x00b2\x00b3\x00b9", L"231"},
    {MAP_FOLDDIGITS, L"123", L"123"}, {MAP_FOLDDIGITS, L"abc", L"abc"}, {MAP_FOLDDIGITS, L"\xff11", L"1"}, {MAP_FOLDDIGITS, L"\x06f0\x06f9", L"09"}, {MAP_FOLDDIGITS, L"\x09e6\x09ef", L"09"},
    {MAP_FOLDDIGITS, L"\x0e50\x0e59", L"09"}, {MAP_FOLDDIGITS, L"a\x0662" L"b", L"a2b"})
FSTEST(ligatures,
    {MAP_EXPAND_LIGATURES, L"\x00c6", L"AE"}, {MAP_EXPAND_LIGATURES, L"\x00e6", L"ae"}, {MAP_EXPAND_LIGATURES, L"\x0152", L"OE"}, {MAP_EXPAND_LIGATURES, L"\x0153", L"oe"},
    {MAP_EXPAND_LIGATURES, L"\xfb01", L"fi"}, {MAP_EXPAND_LIGATURES, L"\xfb02", L"fl"}, {MAP_EXPAND_LIGATURES, L"\xfb00", L"ff"}, {MAP_EXPAND_LIGATURES, L"\xfb03", L"ffi"}, {MAP_EXPAND_LIGATURES, L"\xfb04", L"ffl"},
    {MAP_EXPAND_LIGATURES, L"\x0132", L"IJ"}, {MAP_EXPAND_LIGATURES, L"\x0133", L"ij"}, {MAP_EXPAND_LIGATURES, L"abc", L"abc"}, {MAP_EXPAND_LIGATURES, L"a\x00c6" L"b", L"aAEb"},
    {MAP_EXPAND_LIGATURES, L"\x00c6\x00c6", L"AEAE"}, {MAP_EXPAND_LIGATURES, L"\x00df", L"ss"}, {MAP_EXPAND_LIGATURES, L"\xfb06", L"st"}, {MAP_EXPAND_LIGATURES, L"\x4e2d", L"\x4e2d"})
FSTEST(composition,
    {MAP_PRECOMPOSED, L"e\x0301", L"\x00e9"}, {MAP_PRECOMPOSED, L"E\x0301", L"\x00c9"}, {MAP_PRECOMPOSED, L"a\x0308", L"\x00e4"}, {MAP_PRECOMPOSED, L"n\x0303", L"\x00f1"}, {MAP_PRECOMPOSED, L"A\x030a", L"\x00c5"},
    {MAP_PRECOMPOSED, L"c\x0327", L"\x00e7"}, {MAP_PRECOMPOSED, L"abc", L"abc"}, {MAP_PRECOMPOSED, L"\x00e9", L"\x00e9"}, {MAP_PRECOMPOSED, L"xe\x0301y", L"x\x00e9y"}, {MAP_PRECOMPOSED, L"o\x0302", L"\x00f4"},
    {MAP_PRECOMPOSED, L"u\x0308", L"\x00fc"}, {MAP_PRECOMPOSED, L"e\x0301" L"e\x0300", L"\x00e9\x00e8"},
    {MAP_COMPOSITE, L"\x00e9", L"e\x0301"}, {MAP_COMPOSITE, L"\x00c9", L"E\x0301"}, {MAP_COMPOSITE, L"\x00e4", L"a\x0308"}, {MAP_COMPOSITE, L"\x00f1", L"n\x0303"}, {MAP_COMPOSITE, L"\x00c5", L"A\x030a"},
    {MAP_COMPOSITE, L"abc", L"abc"}, {MAP_COMPOSITE, L"x\x00e9y", L"xe\x0301y"}, {MAP_COMPOSITE, L"e\x0301", L"e\x0301"}, {MAP_COMPOSITE, L"\x00e7", L"c\x0327"}, {MAP_COMPOSITE, L"\x00fc\x00f6", L"u\x0308o\x0308"})

TEST(fold_params) {
    REQUIRE(U(FoldStringW));
    WCHAR out[16];
    SET_ERR(0); EXPECT_EQ(U(FoldStringW)(MAP_FOLDCZONE, NULL, 1, out, 16), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(FoldStringW)(MAP_FOLDCZONE, L"a", 0, out, 16), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(FoldStringW)(MAP_FOLDCZONE, L"a", 1, out, -1), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(FoldStringW)(MAP_FOLDCZONE, L"a", 1, NULL, 1), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    WCHAR same[8] = W_('a', 'b', 0);
    SET_ERR(0); EXPECT_EQ(U(FoldStringW)(MAP_FOLDCZONE, same, 2, same, 8), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    static const DWORD bad[] = {0, MAP_PRECOMPOSED | MAP_COMPOSITE, MAP_PRECOMPOSED | MAP_EXPAND_LIGATURES, MAP_COMPOSITE | MAP_EXPAND_LIGATURES, 0x100, 0x1000, 0x80000000, 0xffffffff, 0x4, 0x8};
    for (int i = 0; i < ARRAY_N(bad); i++) { SET_ERR(0); int r = U(FoldStringW)(bad[i], L"a", 1, out, 16); CHECKF(r == 0 && GetLastError() == E_INVALID_FLAGS, "flags %lx r=%d err=%lu", bad[i], r, GetLastError()); }
}
TEST(fold_flag_combinations) {
    REQUIRE(U(FoldStringW));
    WCHAR out[16];
    int r = U(FoldStringW)(MAP_FOLDCZONE | MAP_FOLDDIGITS, L"\xff21\x0661", 2, out, 16);
    EXPECT_EQ(r, 2); if (r == 2) { CHECK(out[0] == 'A' && out[1] == '1'); }
    r = U(FoldStringW)(MAP_PRECOMPOSED | MAP_FOLDCZONE, L"e\x0301\xff21", 3, out, 16);
    t_note("PRECOMPOSED|FOLDCZONE -> %d chars", r);
    r = U(FoldStringW)(MAP_EXPAND_LIGATURES | MAP_FOLDDIGITS, L"\x00c6\x0662", 2, out, 16);
    t_note("EXPAND_LIGATURES|FOLDDIGITS -> %d chars", r);
    CHECK(r > 0);
}
TEST(fold_a_variants) {
    REQUIRE(U(FoldStringA));
    char o[16];
    SET_ERR(0);
    EXPECT_EQ(U(FoldStringA)(MAP_FOLDCZONE, "abc", -1, o, 16), 4); EXPECT_STR(o, "abc");
    EXPECT_EQ(U(FoldStringA)(MAP_FOLDCZONE, "abc", 3, NULL, 0), 3);
    SET_ERR(0); EXPECT_EQ(U(FoldStringA)(MAP_FOLDCZONE, NULL, 3, o, 16), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(FoldStringA)(0, "abc", 3, o, 16), 0); EXPECT_ERR(E_INVALID_FLAGS);
    SET_ERR(0); EXPECT_EQ(U(FoldStringA)(MAP_FOLDCZONE, "abc", 3, o, 2), 0); EXPECT_ERR(E_INSUFFICIENT_BUFFER);
    SET_ERR(0); EXPECT_EQ(U(FoldStringA)(MAP_FOLDCZONE, "abc", 0, o, 16), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    if (U(GetACP) && U(GetACP)() == 1252) {
        EXPECT_EQ(U(FoldStringA)(MAP_EXPAND_LIGATURES, "\xc6", 1, o, 16), 2); CHECK(!memcmp(o, "AE", 2));
        EXPECT_EQ(U(FoldStringA)(MAP_EXPAND_LIGATURES, "\xe6", 1, o, 16), 2); CHECK(!memcmp(o, "ae", 2));
        EXPECT_EQ(U(FoldStringA)(MAP_EXPAND_LIGATURES, "\x8c", 1, o, 16), 2); CHECK(!memcmp(o, "OE", 2));
        EXPECT_EQ(U(FoldStringA)(MAP_FOLDDIGITS, "\xb2\xb3\xb9", 3, o, 16), 3); CHECK(!memcmp(o, "231", 3));
        int r = U(FoldStringA)(MAP_COMPOSITE, "\xe9", 1, o, 16);
        t_note("FoldStringA(MAP_COMPOSITE, 0xE9) -> %d bytes: %02X %02X", r, (unsigned char)o[0], r > 1 ? (unsigned char)o[1] : 0);
    }
}

TEST(fold_valid_flag_set) {
    REQUIRE(U(FoldStringW));
    /* The only accepted flag combinations; everything else is ERROR_INVALID_FLAGS. */
    static const DWORD good[] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x80, 0x90, 0xa0, 0xb0, 0xc0, 0xd0, 0x2000, 0x2010, 0x2080, 0x2090};
    WCHAR out[16];
    int nvalid = 0;
    for (DWORD f = 0; f < 0x2100; f++) {
        int isgood = 0; for (int i = 0; i < ARRAY_N(good); i++) isgood |= (good[i] == f);
        SET_ERR(0);
        int r = U(FoldStringW)(f, L"abc", 3, out, 16);
        if (isgood) { CHECKF(r == 3, "flags %lx r=%d err=%lu", f, r, GetLastError()); nvalid++; }
        else CHECKF(r == 0 && GetLastError() == E_INVALID_FLAGS, "flags %lx should be invalid: r=%d err=%lu", f, r, GetLastError());
    }
    EXPECT_EQ(nvalid, ARRAY_N(good));
    static const DWORD badhi[] = {0x4000, 0x8000, 0x10000, 0x100000, 0x80000000, 0xffffffff, 0x2020, 0x2040, 0x2060};
    for (int i = 0; i < ARRAY_N(badhi); i++) { SET_ERR(0); int r = U(FoldStringW)(badhi[i], L"abc", 3, out, 16); CHECKF(r == 0 && GetLastError() == E_INVALID_FLAGS, "flags %lx r=%d err=%lu", badhi[i], r, GetLastError()); }
}
TEST(fold_czone_is_compat_composition) {
    REQUIRE(U(FoldStringW));
    WCHAR a[16], b[16];
    /* MAP_FOLDCZONE behaves as NFKC, MAP_PRECOMPOSED as NFC, MAP_COMPOSITE as NFD, 0x50 as NFKD */
    static const WCHAR *s[] = {L"e\x0301", L"\x00e9", L"\xfb01", L"\xff21", L"\x2460", L"\xac00", L"\x1100\x1161", L"A\x030a"};
    if (!U(NormalizeString)) return;
    for (int i = 0; i < ARRAY_N(s); i++) {
        int n1 = U(FoldStringW)(MAP_FOLDCZONE, s[i], -1, a, 16), n2 = U(NormalizeString)(5, s[i], -1, b, 16);
        CHECKF(n1 == n2 && n1 > 0 && sc_weq(a, b, n1), "czone vs NFKC vec %d (%d/%d)", i, n1, n2);
        n1 = U(FoldStringW)(MAP_PRECOMPOSED, s[i], -1, a, 16); n2 = U(NormalizeString)(1, s[i], -1, b, 16);
        CHECKF(n1 == n2 && n1 > 0 && sc_weq(a, b, n1), "precomposed vs NFC vec %d (%d/%d)", i, n1, n2);
        n1 = U(FoldStringW)(MAP_COMPOSITE, s[i], -1, a, 16); n2 = U(NormalizeString)(2, s[i], -1, b, 16);
        CHECKF(n1 == n2 && n1 > 0 && sc_weq(a, b, n1), "composite vs NFD vec %d (%d/%d)", i, n1, n2);
    }
}
