/* gap7: IsChar{Alpha,Lower,Upper,Space}{A,W} */
#include "t.h"
UDECL(BOOL, IsCharSpaceA, (CHAR));
UDECL(BOOL, IsCharSpaceW, (WCHAR));

static int asc_lower(int c) { return c >= 'a' && c <= 'z'; }
static int asc_upper(int c) { return c >= 'A' && c <= 'Z'; }
static int asc_alpha(int c) { return asc_lower(c) || asc_upper(c); }
static int asc_space(int c) { return (c >= 9 && c <= 13) || c == ' '; }

TEST(gap7_ischar_lowerA_ascii) {
    REQUIRE(U(IsCharLowerA));
    int bad = 0;
    for (int c = 0; c < 128; c++) {
        BOOL r = U(IsCharLowerA)((CHAR)c);
        if ((r != 0) != asc_lower(c)) { bad++; t_note("IsCharLowerA(0x%02x)=%d", c, r); }
    }
    CHECKF(bad == 0, "%d mismatches", bad);
    CHECK(U(IsCharLowerA)('a'));
    CHECK(U(IsCharLowerA)('z'));
    CHECK(!U(IsCharLowerA)('A'));
    CHECK(!U(IsCharLowerA)('0'));
    CHECK(!U(IsCharLowerA)(0));
}

TEST(gap7_ischar_upperA_ascii) {
    REQUIRE(U(IsCharUpperA));
    int bad = 0;
    for (int c = 0; c < 128; c++) {
        BOOL r = U(IsCharUpperA)((CHAR)c);
        if ((r != 0) != asc_upper(c)) { bad++; t_note("IsCharUpperA(0x%02x)=%d", c, r); }
    }
    CHECKF(bad == 0, "%d mismatches", bad);
    CHECK(U(IsCharUpperA)('A'));
    CHECK(U(IsCharUpperA)('Z'));
    CHECK(!U(IsCharUpperA)('a'));
    CHECK(!U(IsCharUpperA)(' '));
}

TEST(gap7_ischar_spaceA_ascii) {
    REQUIRE(UN(IsCharSpaceA));
    int bad = 0;
    for (int c = 0; c < 128; c++) {
        if (c >= 0x1c && c <= 0x1f) continue;      /* separators: classification not asserted */
        BOOL r = UN(IsCharSpaceA)((CHAR)c);
        if ((r != 0) != asc_space(c)) { bad++; t_note("IsCharSpaceA(0x%02x)=%d", c, r); }
    }
    CHECKF(bad == 0, "%d mismatches", bad);
    CHECK(UN(IsCharSpaceA)(' '));
    CHECK(UN(IsCharSpaceA)('\t'));
    CHECK(UN(IsCharSpaceA)('\n'));
    CHECK(!UN(IsCharSpaceA)('x'));
    CHECK(!UN(IsCharSpaceA)('_'));
    CHECK(!UN(IsCharSpaceA)(0));
    for (int c = 0x1c; c <= 0x1f; c++) t_note("IsCharSpaceA(0x%02x)=%d", c, UN(IsCharSpaceA)((CHAR)c));
}

TEST(gap7_ischar_lowerW_ascii) {
    REQUIRE(U(IsCharLowerW));
    int bad = 0;
    for (int c = 0; c < 128; c++)
        if ((U(IsCharLowerW)((WCHAR)c) != 0) != asc_lower(c)) bad++;
    CHECKF(bad == 0, "%d mismatches", bad);
}

TEST(gap7_ischar_upperW_ascii) {
    REQUIRE(U(IsCharUpperW));
    int bad = 0;
    for (int c = 0; c < 128; c++)
        if ((U(IsCharUpperW)((WCHAR)c) != 0) != asc_upper(c)) bad++;
    CHECKF(bad == 0, "%d mismatches", bad);
}

TEST(gap7_ischar_alphaW_ascii) {
    REQUIRE(U(IsCharAlphaW));
    int bad = 0;
    for (int c = 0; c < 128; c++)
        if ((U(IsCharAlphaW)((WCHAR)c) != 0) != asc_alpha(c)) { bad++; t_note("IsCharAlphaW(0x%02x)", c); }
    CHECKF(bad == 0, "%d mismatches", bad);
}

TEST(gap7_ischar_spaceW_ascii) {
    REQUIRE(UN(IsCharSpaceW));
    int bad = 0;
    for (int c = 0; c < 128; c++) {
        if (c >= 0x1c && c <= 0x1f) continue;
        if ((UN(IsCharSpaceW)((WCHAR)c) != 0) != asc_space(c)) { bad++; t_note("IsCharSpaceW(0x%02x)", c); }
    }
    CHECKF(bad == 0, "%d mismatches", bad);
}

struct wc_case { WCHAR ch; int lower, upper, alpha; const char *what; };
static const struct wc_case wc_hard[] = {
    { 0x00E9, 1, 0, 1, "e acute" },
    { 0x00C9, 0, 1, 1, "E acute" },
    { 0x00FC, 1, 0, 1, "u diaeresis" },
    { 0x00DC, 0, 1, 1, "U diaeresis" },
    { 0x00DF, 1, 0, 1, "sharp s" },
    { 0x03B1, 1, 0, 1, "alpha" },
    { 0x0391, 0, 1, 1, "Alpha" },
    { 0x0430, 1, 0, 1, "cyrillic a" },
    { 0x0410, 0, 1, 1, "cyrillic A" },
    { 0x0101, 1, 0, 1, "a macron" },
    { 0x0100, 0, 1, 1, "A macron" },
    { 0x0031, 0, 0, 0, "digit one" },
    { 0x0020, 0, 0, 0, "space" },
    { 0x00D7, 0, 0, 0, "multiplication sign" },
    { 0x00F7, 0, 0, 0, "division sign" },
    { 0x2022, 0, 0, 0, "bullet" },
    { 0x20AC, 0, 0, 0, "euro" },
    { 0xFFFF, 0, 0, 0, "noncharacter FFFF" },
    { 0xFFFE, 0, 0, 0, "noncharacter FFFE" },
    { 0xD800, 0, 0, 0, "high surrogate" },
    { 0xDC00, 0, 0, 0, "low surrogate" },
    { 0x0000, 0, 0, 0, "NUL" },
};

TEST(gap7_ischar_W_unicode_table) {
    REQUIRE(U(IsCharLowerW)); REQUIRE(U(IsCharUpperW)); REQUIRE(U(IsCharAlphaW));
    for (size_t i = 0; i < sizeof wc_hard / sizeof wc_hard[0]; i++) {
        const struct wc_case *c = &wc_hard[i];
        CHECKF((U(IsCharLowerW)(c->ch) != 0) == c->lower, "%s U+%04X lower", c->what, c->ch);
        CHECKF((U(IsCharUpperW)(c->ch) != 0) == c->upper, "%s U+%04X upper", c->what, c->ch);
        CHECKF((U(IsCharAlphaW)(c->ch) != 0) == c->alpha, "%s U+%04X alpha", c->what, c->ch);
    }
}

TEST(gap7_ischar_alphaW_ideograph_kana) {
    REQUIRE(U(IsCharAlphaW));
    /* ideographs are alphabetic (C1_ALPHA); kana carry C3_KATAKANA/C3_HIRAGANA and are excluded by the implementation */
    t_note("IsCharAlphaW(U+4E2D)=%d", U(IsCharAlphaW)(0x4E2D));
    t_note("IsCharAlphaW(hiragana A)=%d katakana A=%d", U(IsCharAlphaW)(0x3042), U(IsCharAlphaW)(0x30A2));
    CHECK(!U(IsCharLowerW)(0x4E2D));
    CHECK(!U(IsCharUpperW)(0x4E2D));
}

TEST(gap7_ischar_spaceW_unicode) {
    REQUIRE(UN(IsCharSpaceW));
    static const WCHAR yes[] = { 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x0020, 0x2003, 0x3000 };
    static const WCHAR no[]  = { 0x0000, 0x0021, 0x0030, 0x0041, 0x0061, 0x007F, 0x00E9, 0x4E2D, 0xFFFF, 0xD800 };
    for (size_t i = 0; i < sizeof yes / sizeof yes[0]; i++) CHECKF(UN(IsCharSpaceW)(yes[i]), "U+%04X", yes[i]);
    for (size_t i = 0; i < sizeof no / sizeof no[0]; i++) CHECKF(!UN(IsCharSpaceW)(no[i]), "U+%04X", no[i]);
    static const WCHAR maybe[] = { 0x0085, 0x00A0, 0x1680, 0x2000, 0x2001, 0x2002, 0x2004, 0x2005, 0x2006,
        0x2007, 0x2008, 0x2009, 0x200A, 0x200B, 0x2028, 0x2029, 0x202F, 0x205F, 0xFEFF, 0x180E };
    for (size_t i = 0; i < sizeof maybe / sizeof maybe[0]; i++) t_note("IsCharSpaceW(U+%04X)=%d", maybe[i], UN(IsCharSpaceW)(maybe[i]));
}

TEST(gap7_ischar_W_exhaustive_boolean) {
    REQUIRE(U(IsCharLowerW)); REQUIRE(U(IsCharUpperW)); REQUIRE(U(IsCharAlphaW)); REQUIRE(UN(IsCharSpaceW));
    int nonbool = 0, both = 0, lowers = 0, uppers = 0, alphas = 0, spaces = 0, lower_not_alpha = 0;
    for (unsigned c = 0; c < 0x10000; c++) {
        BOOL l = U(IsCharLowerW)((WCHAR)c), u = U(IsCharUpperW)((WCHAR)c);
        BOOL a = U(IsCharAlphaW)((WCHAR)c), s = UN(IsCharSpaceW)((WCHAR)c);
        if (l > 1 || u > 1 || a > 1 || s > 1) nonbool++;
        if (l && u) both++;
        if (l) lowers++;
        if (u) uppers++;
        if (a) alphas++;
        if (s) spaces++;
        if (l && !a) lower_not_alpha++;
    }
    CHECKF(nonbool == 0, "%d non-boolean results", nonbool);
    CHECKF(lowers > 500, "lowers=%d", lowers);
    CHECKF(uppers > 500, "uppers=%d", uppers);
    CHECKF(alphas > 5000, "alphas=%d", alphas);
    CHECKF(spaces >= 6 && spaces < 64, "spaces=%d", spaces);
    t_note("both=%d lower_not_alpha=%d lowers=%d uppers=%d alphas=%d spaces=%d", both, lower_not_alpha, lowers, uppers, alphas, spaces);
}

TEST(gap7_ischar_A_parity_with_W_high_bytes) {
    REQUIRE(U(IsCharLowerA)); REQUIRE(U(IsCharUpperA)); REQUIRE(UN(IsCharSpaceA));
    REQUIRE(U(IsCharLowerW)); REQUIRE(U(IsCharUpperW)); REQUIRE(UN(IsCharSpaceW));
    int bad = 0, checked = 0;
    for (int c = 0x80; c < 0x100; c++) {
        if (IsDBCSLeadByte((BYTE)c)) continue;
        char ch = (char)c; WCHAR wc = 0;
        if (MultiByteToWideChar(CP_ACP, 0, &ch, 1, &wc, 1) != 1) continue;
        checked++;
        if ((U(IsCharLowerA)(ch) != 0) != (U(IsCharLowerW)(wc) != 0)) { bad++; t_note("lower parity 0x%02x", c); }
        if ((U(IsCharUpperA)(ch) != 0) != (U(IsCharUpperW)(wc) != 0)) { bad++; t_note("upper parity 0x%02x", c); }
        if ((UN(IsCharSpaceA)(ch) != 0) != (UN(IsCharSpaceW)(wc) != 0)) { bad++; t_note("space parity 0x%02x", c); }
    }
    CHECKF(bad == 0, "%d parity mismatches over %d bytes", bad, checked);
}

TEST(gap7_ischar_A_lead_byte_not_case) {
    REQUIRE(U(IsCharLowerA)); REQUIRE(U(IsCharUpperA));
    /* a DBCS lead byte is never reported as lower/upper case */
    for (int c = 0x80; c < 0x100; c++) {
        if (!IsDBCSLeadByte((BYTE)c)) continue;
        CHECKF(!U(IsCharLowerA)((CHAR)c), "lead 0x%02x lower", c);
        CHECKF(!U(IsCharUpperA)((CHAR)c), "lead 0x%02x upper", c);
    }
}

TEST(gap7_ischar_repeat_stable) {
    REQUIRE(U(IsCharAlphaW)); REQUIRE(U(IsCharLowerA));
    for (int i = 0; i < 1000; i++) {
        if (!U(IsCharAlphaW)(L'Q') || U(IsCharAlphaW)(L'7') || !U(IsCharLowerA)('q')) { CHECKF(0, "iteration %d", i); break; }
    }
}

TEST(gap7_ischar_does_not_set_error) {
    REQUIRE(U(IsCharAlphaW)); REQUIRE(UN(IsCharSpaceW));
    SET_ERR(0x1234);
    UN(IsCharSpaceW)(L' ');
    EXPECT_ERR(0x1234);   /* table lookup only */
    SET_ERR(0x1234);
    U(IsCharAlphaW)(L'a');
    DWORD e = GetLastError();
    if (e != 0x1234) t_note("IsCharAlphaW altered last error to %lu", e);
}
