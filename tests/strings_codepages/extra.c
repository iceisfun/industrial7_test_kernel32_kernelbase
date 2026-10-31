/* Additional edge cases: round trips, buffer semantics, A/W parity. */
#include "sc.h"

TEST(xtra_utf8_roundtrip_bmp_sample) {
    REQUIRE(U(MultiByteToWideChar)); REQUIRE(U(WideCharToMultiByte));
    /* every 61st BMP code point outside the surrogate range round-trips through UTF-8 */
    int n = 0;
    for (unsigned c = 1; c < 0x10000; c += 61) {
        if (c >= 0xD800 && c <= 0xDFFF) continue;
        WCHAR w = (WCHAR)c, back = 0; char u[8];
        int l = U(WideCharToMultiByte)(CP_UTF8, 0, &w, 1, u, sizeof u, NULL, NULL);
        int el = c < 0x80 ? 1 : c < 0x800 ? 2 : 3;
        CHECKF(l == el, "U+%04X utf8 len %d want %d", c, l, el);
        int r = U(MultiByteToWideChar)(CP_UTF8, 0, u, l, &back, 1);
        CHECKF(r == 1 && back == w, "U+%04X roundtrip r=%d back=%04X", c, r, back);
        n++;
    }
    CHECK(n > 900);
}
TEST(xtra_utf8_astral_roundtrip) {
    REQUIRE(U(MultiByteToWideChar)); REQUIRE(U(WideCharToMultiByte));
    static const WCHAR w[] = {0xD83D, 0xDE00, 0xD800, 0xDC00, 0xDBFF, 0xDFFF};
    char u[16];
    int l = U(WideCharToMultiByte)(CP_UTF8, 0, w, 6, u, sizeof u, NULL, NULL);
    EXPECT_EQ(l, 12);
    WCHAR b[8] = {0};
    EXPECT_EQ(U(MultiByteToWideChar)(CP_UTF8, 0, u, l, b, 8), 6);
    CHECK(sc_weq(b, w, 6));
    /* truncating inside a 4-byte sequence must fail with 122, not emit half a character */
    SET_ERR(0);
    EXPECT_EQ(U(WideCharToMultiByte)(CP_UTF8, 0, w, 2, u, 3, NULL, NULL), 0);
    EXPECT_ERR(E_INSUFFICIENT_BUFFER);
}
TEST(xtra_mb2wc_embedded_nul_counts) {
    REQUIRE(U(MultiByteToWideChar));
    WCHAR w[8];
    static const char s[] = "a\0b";
    EXPECT_EQ(U(MultiByteToWideChar)(CP_ACP, 0, s, 3, w, 8), 3);
    CHECK(w[0] == 'a' && w[1] == 0 && w[2] == 'b');
    EXPECT_EQ(U(MultiByteToWideChar)(CP_ACP, 0, s, -1, w, 8), 2);
    EXPECT_EQ(U(MultiByteToWideChar)(CP_UTF8, 0, s, 4, NULL, 0), 4);
}
TEST(xtra_wc2mb_embedded_nul_counts) {
    REQUIRE(U(WideCharToMultiByte));
    static const WCHAR s[] = {'a', 0, 'b', 0};
    char o[8];
    EXPECT_EQ(U(WideCharToMultiByte)(CP_ACP, 0, s, 4, o, 8, NULL, NULL), 4);
    EXPECT_EQ(U(WideCharToMultiByte)(CP_ACP, 0, s, -1, o, 8, NULL, NULL), 2);
    EXPECT_EQ(U(WideCharToMultiByte)(CP_UTF8, 0, s, -1, NULL, 0, NULL, NULL), 2);
}
TEST(xtra_lstrcpyn_semantics) {
    REQUIRE(U(lstrcpynA)); REQUIRE(U(lstrcpynW));
    char a[8]; WCHAR w[8];
    memset(a, 'x', sizeof a);
    CHECK(U(lstrcpynA)(a, "abcdef", 4) == a);
    CHECK(!memcmp(a, "abc\0xxxx", 8));
    memset(a, 'x', sizeof a);
    U(lstrcpynA)(a, "ab", 8);
    CHECK(!memcmp(a, "ab\0xxxxx", 8) || !memcmp(a, "ab\0\0\0\0\0\0", 8));
    memset(a, 'x', sizeof a);
    U(lstrcpynA)(a, "abc", 1);
    EXPECT_EQ(a[0], 0); EXPECT_EQ(a[1], 'x');
    for (int i = 0; i < 8; i++) w[i] = 0x7777;
    U(lstrcpynW)(w, L"abcdef", 3);
    CHECK(w[0] == 'a' && w[1] == 'b' && w[2] == 0 && w[3] == 0x7777);
    for (int i = 0; i < 8; i++) w[i] = 0x7777;
    U(lstrcpynW)(w, L"abc", 1);
    CHECK(w[0] == 0 && w[1] == 0x7777);
}
TEST(xtra_lstrcat_lstrcpy_returns) {
    REQUIRE(U(lstrcatA)); REQUIRE(U(lstrcpyA)); REQUIRE(U(lstrcatW)); REQUIRE(U(lstrcpyW));
    char a[16]; WCHAR w[16];
    CHECK(U(lstrcpyA)(a, "foo") == a);
    CHECK(U(lstrcatA)(a, "bar") == a);
    CHECK(!strcmp(a, "foobar"));
    CHECK(U(lstrcpyW)(w, L"foo") == w);
    CHECK(U(lstrcatW)(w, L"bar") == w);
    CHECK(!lstrcmpW(w, L"foobar"));
    U(lstrcpyA)(a, ""); U(lstrcatA)(a, "");
    EXPECT_EQ(a[0], 0);
}
TEST(xtra_lstrlen_embedded_and_long) {
    REQUIRE(U(lstrlenA)); REQUIRE(U(lstrlenW));
    EXPECT_EQ(U(lstrlenA)("ab\0cd"), 2);
    EXPECT_EQ(U(lstrlenW)(L"ab\0cd"), 2);
    EXPECT_EQ(U(lstrlenA)(""), 0);
    static char big[20001]; memset(big, 'a', 20000);
    EXPECT_EQ(U(lstrlenA)(big), 20000);
}
TEST(xtra_lstrcmp_ordering_consistency) {
    REQUIRE(U(lstrcmpA)); REQUIRE(U(lstrcmpiA)); REQUIRE(U(lstrcmpW)); REQUIRE(U(lstrcmpiW));
    static const char *s[] = {"", "a", "A", "b", "B", "ab", "abc", "Abc", "z", "0", "9", "10"};
    for (int i = 0; i < ARRAY_N(s); i++) for (int j = 0; j < ARRAY_N(s); j++) {
        int r1 = U(lstrcmpA)(s[i], s[j]), r2 = U(lstrcmpA)(s[j], s[i]);
        CHECKF((r1 < 0) == (r2 > 0) && (r1 == 0) == (r2 == 0), "antisymmetry %s %s", s[i], s[j]);
        WCHAR wi[8], wj[8]; MultiByteToWideChar(CP_ACP, 0, s[i], -1, wi, 8); MultiByteToWideChar(CP_ACP, 0, s[j], -1, wj, 8);
        int w1 = U(lstrcmpW)(wi, wj);
        CHECKF((r1 < 0) == (w1 < 0) && (r1 == 0) == (w1 == 0), "A/W parity %s %s", s[i], s[j]);
        int i1 = U(lstrcmpiA)(s[i], s[j]), i2 = U(lstrcmpiW)(wi, wj);
        CHECKF((i1 < 0) == (i2 < 0) && (i1 == 0) == (i2 == 0), "i A/W parity %s %s", s[i], s[j]);
    }
    EXPECT_EQ(U(lstrcmpiA)("abc", "ABC"), 0);
    EXPECT_EQ(U(lstrcmpiW)(L"abc", L"ABC"), 0);
    CHECK(U(lstrcmpA)("a", "A") < 0);
}
TEST(xtra_compare_string_ordinal_table) {
    REQUIRE(U(CompareStringOrdinal));
    static const struct { const WCHAR *a, *b; BOOL ic; int r; } v[] = {
        {L"abc", L"abc", 0, 2}, {L"abc", L"ABC", 0, 3}, {L"abc", L"ABC", 1, 2}, {L"a", L"b", 0, 1}, {L"b", L"a", 0, 3},
        {L"ab", L"abc", 0, 1}, {L"abc", L"ab", 0, 3}, {L"", L"", 0, 2}, {L"", L"a", 0, 1}, {L"a", L"", 0, 3},
        {L"\xe9", L"\xc9", 1, 2}, {L"\xe9", L"\xc9", 0, 3}, {L"\x3b1", L"\x391", 1, 2}, {L"\x430", L"\x410", 1, 2},
    };
    for (int i = 0; i < ARRAY_N(v); i++) {
        int r = U(CompareStringOrdinal)(v[i].a, -1, v[i].b, -1, v[i].ic);
        CHECKF(r == v[i].r, "vec %d got %d want %d", i, r, v[i].r);
    }
}
TEST(xtra_comparestringw_flag_matrix_ascii) {
    REQUIRE(U(CompareStringW));
    static const DWORD fl[] = {0, NORM_IGNORECASE, NORM_IGNORENONSPACE, NORM_IGNORESYMBOLS, SORT_STRINGSORT, NORM_IGNOREWIDTH, NORM_IGNOREKANATYPE};
    for (int i = 0; i < ARRAY_N(fl); i++) {
        SET_ERR(0);
        EXPECT_EQ(U(CompareStringW)(LOCALE_INVARIANT, fl[i], L"abc", -1, L"abc", -1), CSTR_EQUAL);
        EXPECT_EQ(U(CompareStringW)(LOCALE_INVARIANT, fl[i], L"abc", 3, L"abd", 3), CSTR_LESS_THAN);
        EXPECT_EQ(U(CompareStringW)(LOCALE_INVARIANT, fl[i], L"abd", 3, L"abc", 3), CSTR_GREATER_THAN);
        EXPECT_EQ(U(CompareStringW)(LOCALE_INVARIANT, fl[i], L"abcX", 3, L"abc", -1), CSTR_EQUAL);
    }
    SET_ERR(0);
    EXPECT_EQ(U(CompareStringW)(LOCALE_INVARIANT, 0, NULL, -1, L"a", -1), 0);
    EXPECT_ERR(E_INVALID_PARAMETER);
}
TEST(xtra_getcpinfo_acp_oem_consistent) {
    REQUIRE(U(GetCPInfo)); REQUIRE(U(GetACP)); REQUIRE(U(GetOEMCP)); REQUIRE(U(IsValidCodePage));
    CPINFO ci;
    CHECK(U(GetCPInfo)(U(GetACP)(), &ci));
    CHECK(ci.MaxCharSize >= 1 && ci.MaxCharSize <= 4);
    CHECK(U(IsValidCodePage)(U(GetACP)()));
    CHECK(U(IsValidCodePage)(U(GetOEMCP)()));
    CHECK(U(IsValidCodePage)(CP_UTF8));
    CHECK(!U(IsValidCodePage)(0xFFFF));
    CHECK(!U(IsValidCodePage)(0));
    for (UINT cp = CP_ACP; cp <= CP_THREAD_ACP; cp++) {
        SET_ERR(0);
        BOOL ok = U(GetCPInfo)(cp, &ci);
        CHECKF(ok || GetLastError() == E_INVALID_PARAMETER, "cp %u ok=%d err=%lu", cp, ok, GetLastError());
    }
    SET_ERR(0);
    EXPECT_EQ(U(GetCPInfo)(1252, NULL), 0);
    EXPECT_ERR(E_INVALID_PARAMETER);
}
TEST(xtra_isdbcsleadbyte_sbcs_cp) {
    REQUIRE(U(IsDBCSLeadByteEx));
    for (int b = 0; b < 256; b++) CHECKF(!U(IsDBCSLeadByteEx)(1252, (BYTE)b), "1252 lead %02X", b);
    for (int b = 0; b < 256; b++) CHECKF(!U(IsDBCSLeadByteEx)(CP_UTF8, (BYTE)b), "utf8 lead %02X", b);
    CHECK(U(IsDBCSLeadByteEx)(932, 0x81));
    CHECK(U(IsDBCSLeadByteEx)(932, 0x9F));
    CHECK(U(IsDBCSLeadByteEx)(932, 0xE0));
    CHECK(!U(IsDBCSLeadByteEx)(932, 0x41));
    CHECK(!U(IsDBCSLeadByteEx)(932, 0xA1));
    CHECK(U(IsDBCSLeadByteEx)(936, 0x81));
    CHECK(U(IsDBCSLeadByteEx)(949, 0x81));
    CHECK(U(IsDBCSLeadByteEx)(950, 0x81));
}
TEST(xtra_lcmap_case_roundtrip_ascii) {
    REQUIRE(U(LCMapStringW));
    WCHAR s[27 * 2], u[64], l[64];
    int n = 0; for (WCHAR c = 'a'; c <= 'z'; c++) s[n++] = c; s[n] = 0;
    EXPECT_EQ(U(LCMapStringW)(LOCALE_INVARIANT, LCMAP_UPPERCASE, s, 26, u, 64), 26);
    EXPECT_EQ(U(LCMapStringW)(LOCALE_INVARIANT, LCMAP_LOWERCASE, u, 26, l, 64), 26);
    for (int i = 0; i < 26; i++) CHECKF(u[i] == 'A' + i && l[i] == 'a' + i, "i=%d", i);
    EXPECT_EQ(U(LCMapStringW)(LOCALE_INVARIANT, LCMAP_UPPERCASE, s, -1, NULL, 0), 27);
    SET_ERR(0);
    EXPECT_EQ(U(LCMapStringW)(LOCALE_INVARIANT, LCMAP_UPPERCASE, s, 26, u, 10), 0);
    EXPECT_ERR(E_INSUFFICIENT_BUFFER);
}
TEST(xtra_getstringtypew_ascii_classes) {
    REQUIRE(U(GetStringTypeW));
    WORD t[128]; WCHAR s[128];
    for (int i = 0; i < 128; i++) s[i] = (WCHAR)i;
    SET_ERR(0);
    CHECK(U(GetStringTypeW)(CT_CTYPE1, s, 128, t));
    for (int i = 0; i < 128; i++) {
        BOOL up = i >= 'A' && i <= 'Z', lo = i >= 'a' && i <= 'z', dg = i >= '0' && i <= '9';
        CHECKF(!!(t[i] & C1_UPPER) == up, "upper %d", i);
        CHECKF(!!(t[i] & C1_LOWER) == lo, "lower %d", i);
        CHECKF(!!(t[i] & C1_DIGIT) == dg, "digit %d", i);
        CHECKF(!!(t[i] & C1_ALPHA) == (up || lo), "alpha %d", i);
    }
    CHECK(t[' '] & C1_SPACE); CHECK(t['\t'] & C1_SPACE); CHECK(t['\n'] & C1_SPACE);
    CHECK(t[0] & C1_CNTRL); CHECK(t[0x7f] & C1_CNTRL);
    CHECK(t['!'] & C1_PUNCT); CHECK(t['A'] & C1_XDIGIT); CHECK(!(t['G'] & C1_XDIGIT));
}
TEST(xtra_foldstring_invalid_params) {
    REQUIRE(U(FoldStringW));
    WCHAR o[8];
    SET_ERR(0); EXPECT_EQ(U(FoldStringW)(MAP_PRECOMPOSED, NULL, -1, o, 8), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(FoldStringW)(MAP_PRECOMPOSED, L"a", 0, o, 8), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(FoldStringW)(MAP_PRECOMPOSED, L"a", -1, NULL, 8), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(FoldStringW)(MAP_PRECOMPOSED, L"a", -1, o, -1), 0); EXPECT_ERR(E_INVALID_PARAMETER);
}
TEST(xtra_muldiv_symmetry_large) {
    REQUIRE(U(MulDiv));
    int n = 0;
    for (int a = -300; a <= 300; a += 7) for (int b = -50; b <= 50; b += 3) for (int d = -9; d <= 9; d++) {
        if (!d) { EXPECT_EQ(U(MulDiv)(a, b, 0), -1); continue; }
        int r = U(MulDiv)(a, b, d), r2 = U(MulDiv)(-a, b, d);
        CHECKF(r == -r2, "odd symmetry %d %d %d: %d %d", a, b, d, r, r2);
        n++;
    }
    CHECK(n > 1000);
}
