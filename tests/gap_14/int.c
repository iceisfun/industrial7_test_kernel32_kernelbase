/* StrToInt family (kernelbase shlwapi-compatible exports). */
#include "t.h"
#include <shlwapi.h>

typedef struct { const char *a; const wchar_t *w; DWORD flags; BOOL ret; LONGLONG val; } introw;
#define ROW(s, f, r, v) { s, L##s, f, r, v }

static const introw rows[] = {
    ROW("0", 0, 1, 0), ROW("1", 0, 1, 1), ROW("123", 0, 1, 123), ROW("-123", 0, 1, -123),
    ROW("+123", 0, 1, 123), ROW("  42", 0, 1, 42), ROW("\t42", 0, 1, 42), ROW("\n42", 0, 1, 42),
    ROW(" \t\n 7", 0, 1, 7), ROW("\r42", 0, 0, 0), ROW("\v42", 0, 0, 0),
    ROW("12abc", 0, 1, 12), ROW("12 34", 0, 1, 12), ROW("abc", 0, 0, 0), ROW("", 0, 0, 0),
    ROW("-", 0, 0, 0), ROW("+", 0, 0, 0), ROW("--5", 0, 0, 0), ROW("+-5", 0, 0, 0), ROW("-+5", 0, 0, 0),
    ROW("- 5", 0, 0, 0), ROW("   ", 0, 0, 0), ROW("-0", 0, 1, 0), ROW("007", 0, 1, 7),
    ROW("2147483647", 0, 1, 2147483647LL), ROW("2147483648", 0, 1, 2147483648LL),
    ROW("-2147483648", 0, 1, -2147483648LL), ROW("-2147483649", 0, 1, -2147483649LL),
    ROW("4294967295", 0, 1, 4294967295LL), ROW("4294967296", 0, 1, 4294967296LL),
    ROW("9223372036854775807", 0, 1, 0x7fffffffffffffffLL),
    ROW("-9223372036854775807", 0, 1, -0x7fffffffffffffffLL),
    ROW("9223372036854775808", 0, 1, (LONGLONG)0x8000000000000000ULL),
    ROW("-9223372036854775808", 0, 1, (LONGLONG)0x8000000000000000ULL),
    ROW("18446744073709551615", 0, 1, -1), ROW("18446744073709551616", 0, 1, 0),
    ROW("0x10", 0, 1, 0), ROW("0x", 0, 1, 0), ROW("1.5", 0, 1, 1), ROW("1e3", 0, 1, 1),
    /* STIF_SUPPORT_HEX */
    ROW("0x10", 1, 1, 16), ROW("0X10", 1, 1, 16), ROW("0xff", 1, 1, 255), ROW("0xFF", 1, 1, 255),
    ROW("0xAbCd", 1, 1, 0xabcd), ROW("0x0", 1, 1, 0), ROW("0x1Fzz", 1, 1, 31),
    ROW("0x", 1, 0, 0), ROW("0xg", 1, 0, 0), ROW("0x ", 1, 0, 0), ROW("  0x20", 1, 1, 32),
    ROW("0xFFFFFFFF", 1, 1, 0xffffffffLL), ROW("0xFFFFFFFFFFFFFFFF", 1, 1, -1),
    ROW("0x7FFFFFFFFFFFFFFF", 1, 1, 0x7fffffffffffffffLL),
    ROW("0x10000000000000000", 1, 1, 0),
    ROW("123", 1, 1, 123), ROW("0", 1, 1, 0), ROW("0abc", 1, 1, 0), ROW("x10", 1, 0, 0),
    ROW("-5", 1, 1, -5), ROW("+0x10", 1, 1, 16),
};
#define NROWS (sizeof rows / sizeof rows[0])

#define SENT 0x5a5a5a5a5a5a5a5aLL

TEST(gap14_strtoint64exw_table) {
    REQUIRE(U(StrToInt64ExW));
    for (size_t i = 0; i < NROWS; i++) {
        LONGLONG v = SENT;
        BOOL r = U(StrToInt64ExW)(rows[i].w, rows[i].flags, &v);
        CHECKF(!!r == !!rows[i].ret, "row %zu \"%s\" flags %lu ret %d want %d", i, rows[i].a, rows[i].flags, (int)r, (int)rows[i].ret);
        CHECKF(v == rows[i].val, "row %zu \"%s\" flags %lu val %lld want %lld", i, rows[i].a, rows[i].flags, v, rows[i].val);
    }
}

TEST(gap14_strtoint64exa_table) {
    REQUIRE(U(StrToInt64ExA));
    for (size_t i = 0; i < NROWS; i++) {
        LONGLONG v = SENT;
        BOOL r = U(StrToInt64ExA)(rows[i].a, rows[i].flags, &v);
        CHECKF(!!r == !!rows[i].ret, "row %zu \"%s\" flags %lu ret %d want %d", i, rows[i].a, rows[i].flags, (int)r, (int)rows[i].ret);
        CHECKF(v == rows[i].val, "row %zu \"%s\" flags %lu val %lld want %lld", i, rows[i].a, rows[i].flags, v, rows[i].val);
    }
}

TEST(gap14_strtoint64ex_ab_parity) {
    REQUIRE(U(StrToInt64ExA)); REQUIRE(U(StrToInt64ExW));
    for (size_t i = 0; i < NROWS; i++) {
        LONGLONG va = 1, vw = 2;
        BOOL ra = U(StrToInt64ExA)(rows[i].a, rows[i].flags, &va);
        BOOL rw = U(StrToInt64ExW)(rows[i].w, rows[i].flags, &vw);
        CHECKF(!!ra == !!rw && va == vw, "row %zu \"%s\" A/W differ", i, rows[i].a);
    }
}

TEST(gap14_strtoint64ex_null_string) {
    REQUIRE(U(StrToInt64ExA)); REQUIRE(U(StrToInt64ExW));
    LONGLONG v = SENT;
    CHECK(!U(StrToInt64ExW)(NULL, 0, &v)); CHECK(v == SENT);
    v = SENT;
    CHECK(!U(StrToInt64ExA)(NULL, 0, &v)); CHECK(v == SENT);
    v = SENT;
    CHECK(!U(StrToInt64ExW)(NULL, 1, &v)); CHECK(v == SENT);
    CHECK(!U(StrToInt64ExW)(NULL, 0, NULL));
    CHECK(!U(StrToInt64ExA)(NULL, 0, NULL));
}

TEST(gap14_strtoint64ex_null_out) {
    REQUIRE(U(StrToInt64ExA)); REQUIRE(U(StrToInt64ExW));
    CHECK(U(StrToInt64ExW)(L"55", 0, NULL));
    CHECK(U(StrToInt64ExA)("55", 0, NULL));
    CHECK(U(StrToInt64ExW)(L"0x55", 1, NULL));
    CHECK(!U(StrToInt64ExW)(L"zz", 0, NULL));
    CHECK(!U(StrToInt64ExA)("", 0, NULL));
}

TEST(gap14_strtoint64ex_unknown_flags) {
    REQUIRE(U(StrToInt64ExW));
    /* only bit 0 (STIF_SUPPORT_HEX) is consulted */
    for (DWORD f = 0; f < 64; f++) {
        LONGLONG v = SENT;
        BOOL r = U(StrToInt64ExW)(L"0x10", f, &v);
        CHECKF(r, "flags %lu", f);
        CHECKF(v == ((f & 1) ? 16 : 0), "flags %lu val %lld", f, v);
    }
    LONGLONG v = SENT;
    CHECK(U(StrToInt64ExW)(L"0x10", 0xfffffffe, &v)); CHECK(v == 0);
    CHECK(U(StrToInt64ExW)(L"0x10", 0xffffffff, &v)); CHECK(v == 16);
}

TEST(gap14_strtoint64ex_hex_sign_ignored) {
    REQUIRE(U(StrToInt64ExW));
    /* reference analysis: hex branch clears the negative flag */
    LONGLONG v = SENT;
    CHECK(U(StrToInt64ExW)(L"-0x10", 1, &v)); CHECK(v == 16);
    v = SENT;
    CHECK(U(StrToInt64ExW)(L"-0x10", 0, &v)); CHECK(v == 0);
}

TEST(gap14_strtoint64ex_unicode_digits) {
    REQUIRE(U(StrToInt64ExW));
    LONGLONG v = SENT;
    CHECK(!U(StrToInt64ExW)(L"\x0663\x0662", 0, &v)); CHECK(v == 0);   /* Arabic-Indic digits not accepted */
    v = SENT;
    CHECK(!U(StrToInt64ExW)(L"\xff11\xff12", 0, &v)); CHECK(v == 0);   /* fullwidth digits not accepted */
    v = SENT;
    CHECK(U(StrToInt64ExW)(L"12\xff13", 0, &v)); CHECK(v == 12);
    v = SENT;
    CHECK(!U(StrToInt64ExW)(L"\x00a0" L"5", 0, &v)); /* NBSP is not whitespace here */
}

TEST(gap14_strtoint64ex_long_digit_string) {
    REQUIRE(U(StrToInt64ExA)); REQUIRE(U(StrToInt64ExW));
    char a[600]; wchar_t w[600];
    for (int i = 0; i < 590; i++) { a[i] = '0'; w[i] = L'0'; }
    a[590] = '9'; w[590] = L'9'; a[591] = 0; w[591] = 0;
    LONGLONG v = SENT;
    CHECK(U(StrToInt64ExW)(w, 0, &v)); CHECK(v == 9);
    v = SENT;
    BOOL r = U(StrToInt64ExA)(a, 0, &v);
    if (r) CHECK(v == 9); else t_note("StrToInt64ExA rejected 591-char string");
}

static const struct { const char *a; const wchar_t *w; DWORD f; BOOL ret; int val; } irows[] = {
    {"0", L"0", 0, 1, 0}, {"-1", L"-1", 0, 1, -1}, {"+7", L"+7", 0, 1, 7}, {"  9", L"  9", 0, 1, 9},
    {"2147483647", L"2147483647", 0, 1, 2147483647}, {"2147483648", L"2147483648", 0, 1, (int)0x80000000},
    {"-2147483648", L"-2147483648", 0, 1, (int)0x80000000}, {"4294967295", L"4294967295", 0, 1, -1},
    {"4294967296", L"4294967296", 0, 1, 0}, {"4294967297", L"4294967297", 0, 1, 1},
    {"abc", L"abc", 0, 0, 0}, {"", L"", 0, 0, 0}, {"-", L"-", 0, 0, 0},
    {"0xFF", L"0xFF", 1, 1, 255}, {"0xFFFFFFFF", L"0xFFFFFFFF", 1, 1, -1},
    {"0x100000000", L"0x100000000", 1, 1, 0}, {"0x", L"0x", 1, 0, 0}, {"0x80000000", L"0x80000000", 1, 1, (int)0x80000000},
    {"0xFF", L"0xFF", 0, 1, 0}, {"12xyz", L"12xyz", 0, 1, 12},
};
#define NIROWS (sizeof irows / sizeof irows[0])

TEST(gap14_strtointexw_table) {
    REQUIRE(U(StrToIntExW));
    for (size_t i = 0; i < NIROWS; i++) {
        int v = 0x1234567;
        BOOL r = U(StrToIntExW)(irows[i].w, irows[i].f, &v);
        CHECKF(!!r == !!irows[i].ret, "row %zu \"%s\" ret %d", i, irows[i].a, (int)r);
        CHECKF(v == irows[i].val, "row %zu \"%s\" val %d want %d", i, irows[i].a, v, irows[i].val);
    }
}

TEST(gap14_strtointexa_table) {
    REQUIRE(U(StrToIntExA));
    for (size_t i = 0; i < NIROWS; i++) {
        int v = 0x1234567;
        BOOL r = U(StrToIntExA)(irows[i].a, irows[i].f, &v);
        CHECKF(!!r == !!irows[i].ret, "row %zu \"%s\" ret %d", i, irows[i].a, (int)r);
        CHECKF(v == irows[i].val, "row %zu \"%s\" val %d want %d", i, irows[i].a, v, irows[i].val);
    }
}

TEST(gap14_strtointex_null_string) {
    REQUIRE(U(StrToIntExW)); REQUIRE(U(StrToIntExA));
    int v = 77;
    CHECK(!U(StrToIntExW)(NULL, 0, &v)); CHECK(v == 0);   /* W writes 0 on failure */
    v = 77;
    CHECK(!U(StrToIntExA)(NULL, 0, &v));
    CHECK(v == 0);
}

TEST(gap14_strtointex_failure_zeroes_out) {
    REQUIRE(U(StrToIntExW));
    int v = 99;
    CHECK(!U(StrToIntExW)(L"xyz", 0, &v)); CHECK(v == 0);
    v = 99;
    CHECK(!U(StrToIntExW)(L"0x", 1, &v)); CHECK(v == 0);
}

static const struct { const char *a; const wchar_t *w; int val; } srows[] = {
    {"0", L"0", 0}, {"1", L"1", 1}, {"-1", L"-1", -1}, {"123", L"123", 123}, {"-123", L"-123", -123},
    {"+5", L"+5", 0}, {" 5", L" 5", 0}, {"\t5", L"\t5", 0}, {"5 ", L"5 ", 5}, {"12abc", L"12abc", 12},
    {"abc", L"abc", 0}, {"", L"", 0}, {"-", L"-", 0}, {"--5", L"--5", 0}, {"-+5", L"-+5", 0},
    {"2147483647", L"2147483647", 2147483647}, {"2147483648", L"2147483648", (int)0x80000000},
    {"-2147483648", L"-2147483648", (int)0x80000000}, {"-2147483649", L"-2147483649", 2147483647},
    {"4294967295", L"4294967295", -1}, {"4294967296", L"4294967296", 0}, {"4294967297", L"4294967297", 1},
    {"0x10", L"0x10", 0}, {"007", L"007", 7}, {"1.9", L"1.9", 1}, {"1,000", L"1,000", 1},
    {"9999999999", L"9999999999", (int)(9999999999ULL & 0xffffffffu)},
};
#define NSROWS (sizeof srows / sizeof srows[0])

TEST(gap14_strtointw_table) {
    REQUIRE(U(StrToIntW));
    for (size_t i = 0; i < NSROWS; i++)
        EXPECT_EQ((int)U(StrToIntW)(srows[i].w), srows[i].val);
}

TEST(gap14_strtointa_table) {
    REQUIRE(U(StrToIntA));
    for (size_t i = 0; i < NSROWS; i++)
        EXPECT_EQ((int)U(StrToIntA)(srows[i].a), srows[i].val);
}

TEST(gap14_strtoint_null) {
    REQUIRE(U(StrToIntA)); REQUIRE(U(StrToIntW));
    CHECK(U(StrToIntA)(NULL) == 0);
    CHECK(U(StrToIntW)(NULL) == 0);
}

TEST(gap14_strtoint_vs_ex_parity) {
    REQUIRE(U(StrToIntA)); REQUIRE(U(StrToIntW)); REQUIRE(U(StrToIntExW));
    /* for strings starting with a digit or '-', StrToInt and StrToIntEx(default) agree */
    const wchar_t *s[] = { L"0", L"15", L"-15", L"2147483648", L"7zz", L"-9" };
    for (size_t i = 0; i < sizeof s / sizeof s[0]; i++) {
        int v = 0;
        U(StrToIntExW)(s[i], 0, &v);
        CHECKF(v == U(StrToIntW)(s[i]), "i=%zu", i);
    }
}

TEST(gap14_strtointexa_long_string) {
    REQUIRE(U(StrToIntExA));
    char a[400];
    memset(a, '0', 380); a[380] = '4'; a[381] = '2'; a[382] = 0;
    int v = -1;
    BOOL r = U(StrToIntExA)(a, 0, &v);
    if (r) CHECK(v == 42); else t_note("StrToIntExA rejected 382-char input");
}

TEST(gap14_strtoint_a_highbit_terminates) {
    REQUIRE(U(StrToIntA));
    char s[] = { '1', '2', (char)0xe9, '3', 0 };
    CHECK(U(StrToIntA)(s) == 12);
}

TEST(gap14_strtoint_repeat) {
    REQUIRE(U(StrToIntW)); REQUIRE(U(StrToInt64ExW));
    for (int i = -500; i <= 500; i++) {
        wchar_t b[16]; wsprintfW(b, L"%d", i);
        CHECK(U(StrToIntW)(b) == i);
        LONGLONG v = 0;
        CHECK(U(StrToInt64ExW)(b, 0, &v) && v == i);
    }
}
