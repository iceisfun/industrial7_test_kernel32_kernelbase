/* GetNumberFormat / GetCurrencyFormat (A, W, Ex). */
#include "nlsutil.h"

static int nf(const WCHAR *loc, DWORD fl, const WCHAR *v, const NUMBERFMTW *f, WCHAR *out, int cch)
{
    return U(GetNumberFormatEx)(loc, fl, v, f, out, cch);
}
static int cf(const WCHAR *loc, DWORD fl, const WCHAR *v, const CURRENCYFMTW *f, WCHAR *out, int cch)
{
    return U(GetCurrencyFormatEx)(loc, fl, v, f, out, cch);
}

typedef struct { const WCHAR *loc, *in, *out; int hard; } nfrow_t;

static void run_nf(const nfrow_t *t, int n)
{
    REQUIRE(U(GetNumberFormatEx)); REQUIRE(U(GetNumberFormatW)); REQUIRE(U(LocaleNameToLCID));
    for (int i = 0; i < n; i++) {
        WCHAR b[200];
        SET_ERR(0);
        int r = nf(t[i].loc, N_NOUSER, t[i].in, NULL, b, 200);
        if (r <= 0) { CHECKF(0, "%ls '%ls' err %lu", t[i].loc, t[i].in, GetLastError()); continue; }
        CHECKF(r == lstrlenW(b) + 1, "%ls '%ls' r=%d len=%d", t[i].loc, t[i].in, r, lstrlenW(b));
        EXPECT_WS(t[i].hard, t[i].loc, "format", b, t[i].out);
        WCHAR b2[200];
        int r2 = U(GetNumberFormatW)(U(LocaleNameToLCID)(t[i].loc, 0), N_NOUSER, t[i].in, NULL, b2, 200);
        CHECKF(r2 == r && !lstrcmpW(b, b2), "W parity %ls", t[i].in);
    }
}

TEST(numfmt_enus_basic) {
    static const nfrow_t t[] = {
        { L"en-US", L"0", L"0.00", 1 }, { L"en-US", L"1", L"1.00", 1 }, { L"en-US", L"12", L"12.00", 1 },
        { L"en-US", L"123", L"123.00", 1 }, { L"en-US", L"1234", L"1,234.00", 1 }, { L"en-US", L"12345", L"12,345.00", 1 },
        { L"en-US", L"123456", L"123,456.00", 1 }, { L"en-US", L"1234567", L"1,234,567.00", 1 },
        { L"en-US", L"1234567.891", L"1,234,567.89", 1 }, { L"en-US", L"-1234.5", L"-1,234.50", 1 },
        { L"en-US", L"-1", L"-1.00", 1 }, { L"en-US", L"-0.5", L"-0.50", 1 }, { L"en-US", L"0.5", L"0.50", 1 },
        { L"en-US", L"0.05", L"0.05", 1 }, { L"en-US", L"0.005", L"0.01", 1 }, { L"en-US", L"0.004", L"0.00", 1 },
        { L"en-US", L"1.005", L"1.01", 1 }, { L"en-US", L"1.004", L"1.00", 1 }, { L"en-US", L"0.995", L"1.00", 1 },
        { L"en-US", L"9.999", L"10.00", 1 }, { L"en-US", L"99.995", L"100.00", 1 }, { L"en-US", L"999.999", L"1,000.00", 1 },
        { L"en-US", L"1000", L"1,000.00", 1 }, { L"en-US", L"999999.995", L"1,000,000.00", 1 },
        { L"en-US", L"1.1", L"1.10", 1 }, { L"en-US", L"1.10", L"1.10", 1 }, { L"en-US", L"1.123456789", L"1.12", 1 },
        { L"en-US", L"-1234567.891", L"-1,234,567.89", 1 }, { L"en-US", L"-999.999", L"-1,000.00", 1 },
        { L"en-US", L"00001", L"1.00", 1 }, { L"en-US", L"000.5", L"0.50", 1 }, { L"en-US", L"1.", L"1.00", 1 },
        { L"en-US", L"123456789012345678901234567890", L"123,456,789,012,345,678,901,234,567,890.00", 1 },
        { L"en-US", L"0.0", L"0.00", 1 }, { L"en-US", L"0.00000000", L"0.00", 1 },
        { L"en-US", L"100", L"100.00", 1 }, { L"en-US", L"10", L"10.00", 1 }, { L"en-US", L"-100000", L"-100,000.00", 1 },
        { L"en-US", L".5", L"0.50", 0 }, { L"en-US", L"-.5", L"-0.50", 0 }, { L"en-US", L"-0", L"0.00", 0 },
        { L"en-US", L"-0.001", L"0.00", 0 }, { L"en-US", L"-0.00", L"0.00", 0 },
    };
    run_nf(t, ARRAYLEN(t));
}
TEST(numfmt_other_locales) {
    static const nfrow_t t[] = {
        { L"de-DE", L"1234567.891", L"1.234.567,89", 1 }, { L"de-DE", L"-1234.5", L"-1.234,50", 1 }, { L"de-DE", L"0.5", L"0,50", 1 },
        { L"de-DE", L"999", L"999,00", 1 }, { L"de-DE", L"1000", L"1.000,00", 1 },
        { L"ja-JP", L"1234567.891", L"1,234,567.89", 1 }, { L"zh-CN", L"1234567.891", L"1,234,567.89", 1 },
        { L"ko-KR", L"1234567.891", L"1,234,567.89", 1 }, { L"th-TH", L"1234567.891", L"1,234,567.89", 1 },
        { L"he-IL", L"1234567.891", L"1,234,567.89", 1 },
        { L"tr-TR", L"1234567.891", L"1.234.567,89", 1 }, { L"pt-BR", L"1234567.891", L"1.234.567,89", 1 },
        { L"fr-FR", L"1234567.891", L"1 234 567,89", 0 }, { L"ru-RU", L"1234567.891", L"1 234 567,89", 0 },
        { L"hi-IN", L"1234567.891", L"12,34,567.89", 0 }, { L"ar-SA", L"1234567.891", L"1,234,567.89", 0 },
        { L"", L"1234567.891", L"1,234,567.89", 1 }, { L"", L"-5", L"-5.00", 1 },
        { L"en-GB", L"1234567.891", L"1,234,567.89", 1 }, { L"de-CH", L"1234567.891", L"1’234’567.89", 0 },
        { L"es-ES", L"1234567.891", L"1.234.567,89", 1 }, { L"it-IT", L"1234567.891", L"1.234.567,89", 1 },
        { L"es-ES", L"1234.5", L"1234,50", 0 },    /* es-ES has min-grouping 2 digits in newer CLDR */
    };
    run_nf(t, ARRAYLEN(t));
}
typedef struct { DWORD nd, lz, grp; const WCHAR *dec, *thou; DWORD neg; const WCHAR *in, *out; } nfc_t;
TEST(numfmt_custom_table) {
    REQUIRE(U(GetNumberFormatEx));
    static const nfc_t t[] = {
        { 2, 1, 3, L".", L",", 1, L"1234567.891", L"1,234,567.89" },
        { 0, 1, 3, L"", L",", 1, L"1234567.891", L"1,234,568" },
        { 0, 1, 3, L".", L",", 1, L"0.4", L"0" },
        { 0, 1, 3, L".", L",", 1, L"0.5", L"1" },
        { 0, 1, 3, L".", L",", 1, L"-0.6", L"-1" },
        { 1, 1, 3, L".", L",", 1, L"1234.56", L"1,234.6" },
        { 1, 1, 3, L".", L",", 1, L"1234.54", L"1,234.5" },
        { 3, 1, 3, L".", L",", 1, L"1234.5", L"1,234.500" },
        { 9, 1, 3, L".", L",", 1, L"1.5", L"1.500000000" },
        { 9, 1, 3, L".", L",", 1, L"1.1234567891", L"1.123456789" },
        { 2, 0, 3, L".", L",", 1, L"0.5", L".50" },
        { 2, 1, 3, L".", L",", 1, L"0.5", L"0.50" },
        { 2, 0, 3, L".", L",", 1, L"1.5", L"1.50" },
        { 2, 0, 3, L".", L",", 1, L"-0.5", L"-.50" },
        { 2, 1, 0, L".", L",", 1, L"1234567.891", L"1234567.89" },
        { 2, 1, 1, L".", L",", 1, L"1234.5", L"1,2,3,4.50" },
        { 2, 1, 2, L".", L",", 1, L"1234567.891", L"1,23,45,67.89" },
        { 2, 1, 3, L".", L",", 1, L"1234567.891", L"1,234,567.89" },
        { 2, 1, 4, L".", L",", 1, L"123456789.1", L"1,2345,6789.10" },
        { 2, 1, 32, L".", L",", 1, L"1234567.891", L"12,34,567.89" },
        { 2, 1, 32, L".", L",", 1, L"123456789.1", L"12,34,56,789.10" },
        { 2, 1, 3, L",", L".", 1, L"1234567.891", L"1.234.567,89" },
        { 2, 1, 3, L"|", L"'", 1, L"1234567.891", L"1'234'567|89" },
        { 2, 1, 3, L"dec", L"th", 1, L"1234.5", L"1th234dec50" },
        { 2, 1, 3, L".", L"", 1, L"1234567.891", L"1234567.89" },
        { 2, 1, 3, L".", L"  ", 1, L"1234567.891", L"1  234  567.89" },
        { 2, 1, 3, L".", L",", 0, L"-1.5", L"(1.50)" },
        { 2, 1, 3, L".", L",", 1, L"-1.5", L"-1.50" },
        { 2, 1, 3, L".", L",", 2, L"-1.5", L"- 1.50" },
        { 2, 1, 3, L".", L",", 3, L"-1.5", L"1.50-" },
        { 2, 1, 3, L".", L",", 4, L"-1.5", L"1.50 -" },
        { 2, 1, 3, L".", L",", 0, L"1.5", L"1.50" },
        { 2, 1, 3, L".", L",", 3, L"1.5", L"1.50" },
        { 2, 1, 3, L".", L",", 0, L"-1234567", L"(1,234,567.00)" },
        { 0, 1, 3, L"", L"", 1, L"42", L"42" },
    };
    for (int i = 0; i < ARRAYLEN(t); i++) {
        NUMBERFMTW f = { t[i].nd, t[i].lz, t[i].grp, (LPWSTR)t[i].dec, (LPWSTR)t[i].thou, t[i].neg };
        WCHAR b[200]; SET_ERR(0);
        int r = nf(L"en-US", 0, t[i].in, &f, b, 200);
        if (r <= 0) { CHECKF(0, "row %d '%ls' err %lu", i, t[i].in, GetLastError()); continue; }
        CHECKF(!lstrcmpW(b, t[i].out), "row %d '%ls' got '%ls' want '%ls'", i, t[i].in, b, t[i].out);
        CHECK(r == lstrlenW(b) + 1);
    }
}
TEST(numfmt_custom_ignores_locale) {
    REQUIRE(U(GetNumberFormatEx));
    NUMBERFMTW f = { 2, 1, 3, L".", L",", 1 };
    static const WCHAR *locs[] = { L"en-US", L"de-DE", L"ja-JP", L"ar-SA", L"" };
    for (int i = 0; i < ARRAYLEN(locs); i++) {
        WCHAR b[100]; int r = nf(locs[i], 0, L"-1234.5", &f, b, 100);
        CHECKF(r > 0 && !lstrcmpW(b, L"-1,234.50"), "%ls", locs[i]);
    }
}
TEST(numfmt_invalid_numberfmt) {
    REQUIRE(U(GetNumberFormatEx));
    if (t_is_wine()) { t_skip("emulator does not validate NUMBERFMT (and overflows on some)"); return; }
    WCHAR b[100];
    struct { NUMBERFMTW f; const char *why; } t[] = {
        { { 10, 1, 3, L".", L",", 1 }, "NumDigits 10" },
        { { 2, 2, 3, L".", L",", 1 }, "LeadingZero 2" },
        { { 2, 1, 10000, L".", L",", 1 }, "Grouping 10000" },
        { { 2, 1, 3, L".", L",", 5 }, "NegativeOrder 5" },
        { { 2, 1, 3, NULL, L",", 1 }, "NULL decimal" },
        { { 2, 1, 3, L".", NULL, 1 }, "NULL thousand" },
        { { 2, 1, 3, L"", L",", 1 }, "empty decimal with digits" },
        { { 2, 1, 3, L"abcde", L",", 1 }, "decimal sep too long" },
        { { 2, 1, 3, L".", L"abcde", 1 }, "thousand sep too long" },
        { { 2, 1, 3, L"abcd", L",", 1 }, "decimal sep 4 chars" },
        { { 2, 1, 3, L"1", L",", 1 }, "digit in decimal sep" },
        { { 0xFFFFFFFF, 1, 3, L".", L",", 1 }, "NumDigits -1" },
    };
    for (int i = 0; i < ARRAYLEN(t); i++) {
        if (t[i].f.lpDecimalSep == NULL || t[i].f.lpThousandSep == NULL) { if (t_is_wine()) continue; }
        SET_ERR(0);
        int r = nf(L"en-US", 0, L"1.5", &t[i].f, b, 100);
        CHECKF(r == 0, "%s r=%d", t[i].why, r);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(numfmt_numfmt_boundaries_valid) {
    REQUIRE(U(GetNumberFormatEx));
    WCHAR b[100];
    NUMBERFMTW f1 = { 9, 1, 9999, L".", L",", 4 };
    CHECK(nf(L"en-US", 0, L"1.5", &f1, b, 100) > 0);
    NUMBERFMTW f2 = { 0, 0, 0, L"", L"", 0 };
    CHECK(nf(L"en-US", 0, L"1.5", &f2, b, 100) > 0);
    NUMBERFMTW f3 = { 2, 1, 3, L"abc", L"xyz", 1 };   /* IsValidSeparatorString: len < 4, no digits */
    SET_ERR(0);
    int r = nf(L"en-US", 0, L"1234.5", &f3, b, 100);
    CHECK(r > 0);
}
TEST(numfmt_flags_with_format) {
    REQUIRE(U(GetNumberFormatEx));
    NUMBERFMTW f = { 2, 1, 3, L".", L",", 1 };
    WCHAR b[100];
    SET_ERR(0); CHECK(nf(L"en-US", N_NOUSER, L"1", &f, b, 100) == 0); EXPECT_ERR(ERROR_INVALID_FLAGS);
    SET_ERR(0); CHECK(nf(L"en-US", 1, L"1", NULL, b, 100) == 0); EXPECT_ERR(ERROR_INVALID_FLAGS);
    CHECK(nf(L"en-US", 0x40000000, L"1", NULL, b, 100) > 0);   /* worker mask is 0x3fffffff: LOCALE_USE_CP_ACP is not rejected */
    SET_ERR(0); CHECK(nf(L"en-US", 0x20000000, L"1", NULL, b, 100) == 0); EXPECT_ERR(ERROR_INVALID_FLAGS);
    SET_ERR(0); CHECK(nf(L"en-US", 0x10, L"1", NULL, b, 100) == 0); EXPECT_ERR(ERROR_INVALID_FLAGS);
    CHECK(nf(L"en-US", N_NOUSER, L"1", NULL, b, 100) > 0);
    CHECK(nf(L"en-US", 0, L"1", NULL, b, 100) > 0);
}
TEST(numfmt_invalid_strings) {
    REQUIRE(U(GetNumberFormatEx));
    static const WCHAR *bad[] = { L"abc", L"1.2.3", L"1e5", L"1,000", L" 1", L"--1", L"1-", L"1a", L"a1", L"0x10", L"1/2", L"\x0661" };
    WCHAR b[100];
    for (int i = 0; i < ARRAYLEN(bad); i++) {
        SET_ERR(0);
        int r = nf(L"en-US", N_NOUSER, bad[i], NULL, b, 100);
        CHECKF(r == 0, "'%ls' r=%d", bad[i], r);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(numfmt_questionable_strings) {
    REQUIRE(U(GetNumberFormatEx));
    static const WCHAR *q[] = { L"", L"+1", L"1 ", L".", L"-.", L"1.2 ", L"+", L" ", L"-" };
    WCHAR b[100];
    for (int i = 0; i < ARRAYLEN(q); i++) {
        SET_ERR(0);
        int r = nf(L"en-US", N_NOUSER, q[i], NULL, b, 100);
        t_note("GetNumberFormatEx('%ls') = %d (%ls) err=%lu", q[i], r, r > 0 ? b : L"", r ? 0 : GetLastError());
    }
}
TEST(numfmt_null_args) {
    REQUIRE(U(GetNumberFormatEx));
    WCHAR b[100];
    SET_ERR(0); CHECK(nf(L"en-US", 0, NULL, NULL, b, 100) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(nf(L"en-US", 0, L"1", NULL, NULL, 5) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(nf(L"en-US", 0, L"1", NULL, b, -1) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(nf(L"bad-locale-name", 0, L"1", NULL, b, 100) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    WCHAR same[100] = L"123";
    SET_ERR(0); CHECK(nf(L"en-US", 0, same, NULL, same, 100) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);   /* in/out buffers must differ */
}
TEST(numfmt_size_query_and_truncation) {
    REQUIRE(U(GetNumberFormatEx));
    WCHAR b[100];
    SET_ERR(0);
    int n = nf(L"en-US", N_NOUSER, L"1234567.891", NULL, NULL, 0);
    CHECKF(n == 13, "n=%d", n);   /* "1,234,567.89" + NUL */
    CHECK(nf(L"en-US", N_NOUSER, L"1234567.891", NULL, b, 13) == 13);
    for (int cch = 1; cch < 13; cch++) {
        for (int k = 0; k < 20; k++) b[k] = 0x7777;
        SET_ERR(0);
        int r = nf(L"en-US", N_NOUSER, L"1234567.891", NULL, b, cch);
        CHECKF(r == 0, "cch=%d r=%d", cch, r);
        EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    }
    n = nf(L"en-US", N_NOUSER, L"0", NULL, NULL, 0);
    CHECK(n == 5);
}
TEST(numfmt_a_variant) {
    REQUIRE(U(GetNumberFormatA));
    char b[100];
    SET_ERR(0);
    int r = U(GetNumberFormatA)(0x409, N_NOUSER, "1234567.891", NULL, b, 100);
    CHECK(r == 13); CHECK(!strcmp(b, "1,234,567.89"));
    CHECK(U(GetNumberFormatA)(0x409, N_NOUSER, "1234567.891", NULL, NULL, 0) == 13);
    r = U(GetNumberFormatA)(0x407, N_NOUSER, "-1234.5", NULL, b, 100);
    CHECK(r == 10); CHECK(!strcmp(b, "-1.234,50"));
    SET_ERR(0); CHECK(U(GetNumberFormatA)(0x409, N_NOUSER, "abc", NULL, b, 100) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(U(GetNumberFormatA)(0x409, N_NOUSER, NULL, NULL, b, 100) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(U(GetNumberFormatA)(0x409, N_NOUSER, "1", NULL, b, 1) == 0); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    SET_ERR(0); CHECK(U(GetNumberFormatA)(0x409, 0x1, "1", NULL, b, 100) == 0); EXPECT_ERR(ERROR_INVALID_FLAGS);
}
TEST(numfmt_a_custom) {
    REQUIRE(U(GetNumberFormatA));
    NUMBERFMTA f = { 3, 1, 3, (LPSTR)",", (LPSTR)".", 1 };
    char b[100];
    int r = U(GetNumberFormatA)(0x409, 0, "-1234567.8912", &f, b, 100);
    CHECK(r == 15); CHECKF(!strcmp(b, "-1.234.567,891"), "got %s", b);
    NUMBERFMTA g = { 2, 1, 3, (LPSTR)".", (LPSTR)",", 1 };
    SET_ERR(0); CHECK(U(GetNumberFormatA)(0x409, N_NOUSER, "1", &g, b, 100) == 0); EXPECT_ERR(ERROR_INVALID_FLAGS);
}
TEST(numfmt_w_lcid_variants) {
    REQUIRE(U(GetNumberFormatW));
    WCHAR b[100];
    CHECK(U(GetNumberFormatW)(0x409, N_NOUSER, L"1234.5", NULL, b, 100) == 9); CHECK(!lstrcmpW(b, L"1,234.50"));
    CHECK(U(GetNumberFormatW)(0x407, N_NOUSER, L"1234.5", NULL, b, 100) == 9); CHECK(!lstrcmpW(b, L"1.234,50"));
    CHECK(U(GetNumberFormatW)(LOCALE_INVARIANT, N_NOUSER, L"1234.5", NULL, b, 100) == 9); CHECK(!lstrcmpW(b, L"1,234.50"));
    SET_ERR(0); CHECK(U(GetNumberFormatW)(0xDEAD, N_NOUSER, L"1", NULL, b, 100) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    int r = U(GetNumberFormatW)(LOCALE_SYSTEM_DEFAULT, N_NOUSER, L"1", NULL, b, 100); CHECK(r > 0);
    r = U(GetNumberFormatW)(LOCALE_USER_DEFAULT, 0, L"1", NULL, b, 100); CHECK(r > 0);
}
TEST(numfmt_locale_info_consistency) {
    REQUIRE(U(GetNumberFormatEx)); REQUIRE(U(GetLocaleInfoEx));
    /* The default format must equal the format built from the locale's own number settings. */
    for (int i = 0; i < ARRAYLEN(FIXED_LOCALES); i++) {
        WCHAR dec[8], thou[8], b1[100], b2[100]; DWORD nd, lz, ng, grp = 0;
        int ok;
        nls_str(FIXED_LOCALES[i], L_SDECIMAL, dec, 8); nls_str(FIXED_LOCALES[i], L_STHOUSAND, thou, 8);
        nd = nls_num(FIXED_LOCALES[i], L_IDIGITS, &ok); lz = nls_num(FIXED_LOCALES[i], L_ILZERO, &ok); ng = nls_num(FIXED_LOCALES[i], L_INEGNUMBER, &ok);
        WCHAR gs[16]; nls_str(FIXED_LOCALES[i], L_SGROUPING, gs, 16);
        /* "3;0" -> 3, "3;2;0" -> 32, "0" -> 0 */
        for (WCHAR *p = gs; *p && *p != '0'; p++) if (*p >= '1' && *p <= '9') grp = grp * 10 + (*p - '0');
        if (!wcscmp(gs, L"3;2;0")) grp = 32;
        NUMBERFMTW f = { nd, lz, grp, dec, thou, ng };
        int r1 = nf(FIXED_LOCALES[i], N_NOUSER, L"-9876543.2109", NULL, b1, 100);
        int r2 = nf(FIXED_LOCALES[i], 0, L"-9876543.2109", &f, b2, 100);
        CHECKF(r1 > 0 && r2 > 0 && !lstrcmpW(b1, b2), "%ls: '%ls' vs '%ls'", FIXED_LOCALES[i], b1, b2);
    }
}
TEST(numfmt_repeat_stability) {
    REQUIRE(U(GetNumberFormatEx));
    WCHAR b[100];
    for (int i = 0; i < 1000; i++) {
        int r = nf(L"en-US", N_NOUSER, L"1234.5678", NULL, b, 100);
        if (r != 9 || lstrcmpW(b, L"1,234.57")) { CHECKF(0, "iter %d", i); break; }
    }
}
TEST(numfmt_long_input) {
    REQUIRE(U(GetNumberFormatEx));
    WCHAR in[300], b[600];
    for (int i = 0; i < 200; i++) in[i] = '7';
    in[200] = 0;
    SET_ERR(0);
    int r = nf(L"en-US", N_NOUSER, in, NULL, b, 600);
    t_note("200-digit number: r=%d err=%lu", r, r ? 0 : GetLastError());
    if (r > 0) CHECK(r > 200);
}

/* ---- currency ---- */
typedef struct { const WCHAR *loc, *in, *out; int hard; } cfrow_t;
static void run_cf(const cfrow_t *t, int n)
{
    REQUIRE(U(GetCurrencyFormatEx)); REQUIRE(U(GetCurrencyFormatW)); REQUIRE(U(LocaleNameToLCID));
    for (int i = 0; i < n; i++) {
        WCHAR b[200]; SET_ERR(0);
        int r = cf(t[i].loc, N_NOUSER, t[i].in, NULL, b, 200);
        if (r <= 0) { CHECKF(0, "%ls '%ls' err %lu", t[i].loc, t[i].in, GetLastError()); continue; }
        CHECK(r == lstrlenW(b) + 1);
        EXPECT_WS(t[i].hard, t[i].loc, "format", b, t[i].out);
        WCHAR b2[200];
        int r2 = U(GetCurrencyFormatW)(U(LocaleNameToLCID)(t[i].loc, 0), N_NOUSER, t[i].in, NULL, b2, 200);
        CHECKF(r2 == r && !lstrcmpW(b, b2), "W parity %ls", t[i].in);
    }
}
TEST(curfmt_enus_basic) {
    static const cfrow_t t[] = {
        { L"en-US", L"0", L"$0.00", 1 }, { L"en-US", L"1", L"$1.00", 1 }, { L"en-US", L"1234567.891", L"$1,234,567.89", 1 },
        { L"en-US", L"-1234.5", L"($1,234.50)", 1 }, { L"en-US", L"0.5", L"$0.50", 1 }, { L"en-US", L"0.005", L"$0.01", 1 },
        { L"en-US", L"0.004", L"$0.00", 1 }, { L"en-US", L"999.995", L"$1,000.00", 1 }, { L"en-US", L"12", L"$12.00", 1 },
        { L"en-US", L"-0.5", L"($0.50)", 1 }, { L"en-US", L"1000000", L"$1,000,000.00", 1 },
        { L"en-US", L"-1", L"($1.00)", 1 }, { L"en-US", L"123456789012", L"$123,456,789,012.00", 1 },
        { L"de-DE", L"1234.5", L"1.234,50 €", 0 }, { L"de-DE", L"-1234.5", L"-1.234,50 €", 0 },
        { L"fr-FR", L"1234.5", L"1 234,50 €", 0 },
        { L"ja-JP", L"1234.5", L"¥1,235", 0 }, { L"ja-JP", L"-1234.5", L"-¥1,235", 0 },
        { L"en-GB", L"1234.5", L"£1,234.50", 1 }, { L"en-GB", L"-1234.5", L"-£1,234.50", 1 },
        { L"ko-KR", L"1234.5", L"₩1,235", 0 }, { L"zh-CN", L"1234.5", L"¥1,234.50", 0 },
        { L"ru-RU", L"1234.5", L"1 234,50 ₽", 0 }, { L"pt-BR", L"1234.5", L"R$ 1.234,50", 0 },
        { L"tr-TR", L"1234.5", L"₺1.234,50", 0 }, { L"hi-IN", L"1234567.5", L"₹12,34,567.50", 0 },
        { L"", L"1234.5", L"¤1,234.50", 1 }, { L"", L"-1234.5", L"(¤1,234.50)", 0 },
    };
    run_cf(t, ARRAYLEN(t));
}
TEST(curfmt_neg_order_table) {
    REQUIRE(U(GetCurrencyFormatEx));
    static const WCHAR *neg[16] = {
        L"($1.50)", L"-$1.50", L"$-1.50", L"$1.50-", L"(1.50$)", L"-1.50$", L"1.50-$", L"1.50$-",
        L"-1.50 $", L"-$ 1.50", L"1.50 $-", L"$ 1.50-", L"$ -1.50", L"1.50- $", L"($ 1.50)", L"(1.50 $)"
    };
    for (DWORD o = 0; o < 16; o++) {
        CURRENCYFMTW f = { 2, 1, 3, L".", L",", o, 0, L"$" };
        WCHAR b[100]; SET_ERR(0);
        int r = cf(L"en-US", 0, L"-1.5", &f, b, 100);
        CHECKF(r > 0 && !lstrcmpW(b, neg[o]), "neg order %lu got '%ls' want '%ls'", o, b, neg[o]);
    }
}
TEST(curfmt_pos_order_table) {
    REQUIRE(U(GetCurrencyFormatEx));
    static const WCHAR *pos[4] = { L"$1.50", L"1.50$", L"$ 1.50", L"1.50 $" };
    for (DWORD o = 0; o < 4; o++) {
        CURRENCYFMTW f = { 2, 1, 3, L".", L",", 0, o, L"$" };
        WCHAR b[100];
        int r = cf(L"en-US", 0, L"1.5", &f, b, 100);
        CHECKF(r > 0 && !lstrcmpW(b, pos[o]), "pos order %lu got '%ls'", o, b);
    }
}
TEST(curfmt_custom_misc) {
    REQUIRE(U(GetCurrencyFormatEx));
    WCHAR b[100];
    CURRENCYFMTW f1 = { 0, 1, 3, L"", L",", 1, 0, L"EUR" };
    CHECK(cf(L"en-US", 0, L"1234567.5", &f1, b, 100) > 0); CHECKF(!lstrcmpW(b, L"EUR1,234,568"), "got %ls", b);
    CURRENCYFMTW f2 = { 3, 0, 2, L",", L".", 1, 3, L"kr" };
    CHECK(cf(L"en-US", 0, L"1234567.8915", &f2, b, 100) > 0); CHECKF(!lstrcmpW(b, L"1.23.45.67,892 kr"), "got %ls", b);
    CURRENCYFMTW f3 = { 2, 0, 3, L".", L",", 1, 0, L"X" };
    CHECK(cf(L"en-US", 0, L"0.25", &f3, b, 100) > 0); CHECKF(!lstrcmpW(b, L"X.25"), "got %ls", b);
    CURRENCYFMTW f4 = { 2, 1, 3, L".", L",", 1, 0, L"" };
    SET_ERR(0);
    int r = cf(L"en-US", 0, L"2.5", &f4, b, 100);
    t_note("empty currency symbol: r=%d '%ls' err=%lu", r, r > 0 ? b : L"", r ? 0 : GetLastError());
    CURRENCYFMTW f5 = { 2, 1, 32, L".", L",", 1, 0, L"$" };
    CHECK(cf(L"en-US", 0, L"1234567.5", &f5, b, 100) > 0); CHECKF(!lstrcmpW(b, L"$12,34,567.50"), "got %ls", b);
}
TEST(curfmt_invalid_currencyfmt) {
    REQUIRE(U(GetCurrencyFormatEx));
    if (t_is_wine()) { t_skip("emulator does not validate CURRENCYFMT"); return; }
    WCHAR b[100];
    struct { CURRENCYFMTW f; const char *why; } t[] = {
        { { 10, 1, 3, L".", L",", 0, 0, L"$" }, "NumDigits 10" },
        { { 2, 2, 3, L".", L",", 0, 0, L"$" }, "LeadingZero 2" },
        { { 2, 1, 10000, L".", L",", 0, 0, L"$" }, "Grouping 10000" },
        { { 2, 1, 3, L".", L",", 16, 0, L"$" }, "NegativeOrder 16" },
        { { 2, 1, 3, L".", L",", 0, 4, L"$" }, "PositiveOrder 4" },
        { { 2, 1, 3, NULL, L",", 0, 0, L"$" }, "NULL decimal" },
        { { 2, 1, 3, L".", NULL, 0, 0, L"$" }, "NULL thousand" },
        { { 2, 1, 3, L".", L",", 0, 0, NULL }, "NULL symbol" },
        { { 2, 1, 3, L".", L",", 0, 0, L"1234567890123" }, "symbol with digits" },
        { { 2, 1, 3, L".", L",", 0, 0, L"abcdefghijklm" }, "symbol 13 chars" },
    };
    for (int i = 0; i < ARRAYLEN(t); i++) {
        if (!t[i].f.lpDecimalSep || !t[i].f.lpThousandSep || !t[i].f.lpCurrencySymbol) { if (t_is_wine()) continue; }
        SET_ERR(0);
        int r = cf(L"en-US", 0, L"1.5", &t[i].f, b, 100);
        CHECKF(r == 0, "%s r=%d", t[i].why, r);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(curfmt_flags_and_args) {
    REQUIRE(U(GetCurrencyFormatEx));
    CURRENCYFMTW f = { 2, 1, 3, L".", L",", 0, 0, L"$" };
    WCHAR b[100];
    SET_ERR(0); CHECK(cf(L"en-US", N_NOUSER, L"1", &f, b, 100) == 0); EXPECT_ERR(ERROR_INVALID_FLAGS);
    SET_ERR(0); CHECK(cf(L"en-US", 2, L"1", NULL, b, 100) == 0); EXPECT_ERR(ERROR_INVALID_FLAGS);
    SET_ERR(0); CHECK(cf(L"en-US", 0, NULL, NULL, b, 100) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(cf(L"en-US", 0, L"abc", NULL, b, 100) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(cf(L"en-US", 0, L"1.2.3", NULL, b, 100) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(cf(L"en-US", 0, L"1", NULL, NULL, 4) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(cf(L"en-US", 0, L"1", NULL, b, -3) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(cf(L"nope-NOPE-nope", 0, L"1", NULL, b, 100) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(curfmt_size_query_and_truncation) {
    REQUIRE(U(GetCurrencyFormatEx));
    WCHAR b[100];
    int n = cf(L"en-US", N_NOUSER, L"1234567.891", NULL, NULL, 0);
    CHECKF(n == 14, "n=%d", n);    /* "$1,234,567.89" */
    for (int cch = 1; cch < n; cch++) {
        SET_ERR(0); int r = cf(L"en-US", N_NOUSER, L"1234567.891", NULL, b, cch);
        CHECKF(r == 0, "cch=%d", cch); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    }
    CHECK(cf(L"en-US", N_NOUSER, L"1234567.891", NULL, b, n) == n);
}
TEST(curfmt_a_variant) {
    REQUIRE(U(GetCurrencyFormatA));
    char b[100];
    int r = U(GetCurrencyFormatA)(0x409, N_NOUSER, "1234.5", NULL, b, 100);
    CHECK(r == 10); CHECKF(!strcmp(b, "$1,234.50"), "got %s", b);
    CHECK(U(GetCurrencyFormatA)(0x409, N_NOUSER, "1234.5", NULL, NULL, 0) == 10);
    r = U(GetCurrencyFormatA)(0x409, N_NOUSER, "-1234.5", NULL, b, 100);
    CHECK(r == 12); CHECK(!strcmp(b, "($1,234.50)"));
    CURRENCYFMTA f = { 2, 1, 3, (LPSTR)".", (LPSTR)",", 1, 0, (LPSTR)"USD " };
    r = U(GetCurrencyFormatA)(0x409, 0, "-5", &f, b, 100);
    CHECK(r > 0); CHECKF(!strcmp(b, "-USD 5.00"), "got %s", b);
    SET_ERR(0); CHECK(U(GetCurrencyFormatA)(0x409, N_NOUSER, "x", NULL, b, 100) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(curfmt_w_lcid_variants) {
    REQUIRE(U(GetCurrencyFormatW));
    WCHAR b[100];
    CHECK(U(GetCurrencyFormatW)(0x409, N_NOUSER, L"1.5", NULL, b, 100) == 6); CHECK(!lstrcmpW(b, L"$1.50"));
    CHECK(U(GetCurrencyFormatW)(LOCALE_INVARIANT, N_NOUSER, L"1.5", NULL, b, 100) == 6); CHECK(!lstrcmpW(b, L"¤1.50"));
    SET_ERR(0); CHECK(U(GetCurrencyFormatW)(0xDEAD, N_NOUSER, L"1", NULL, b, 100) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(curfmt_locale_info_consistency) {
    REQUIRE(U(GetCurrencyFormatEx)); REQUIRE(U(GetLocaleInfoEx));
    for (int i = 0; i < ARRAYLEN(FIXED_LOCALES); i++) {
        WCHAR dec[8], thou[8], sym[16], b1[100], b2[100], gs[16]; int ok; DWORD grp = 0;
        nls_str(FIXED_LOCALES[i], L_SMONDECIMALSEP, dec, 8); nls_str(FIXED_LOCALES[i], L_SMONTHOUSANDSEP, thou, 8);
        nls_str(FIXED_LOCALES[i], L_SCURRENCY, sym, 16); nls_str(FIXED_LOCALES[i], L_SMONGROUPING, gs, 16);
        for (WCHAR *p = gs; *p && *p != '0'; p++) if (*p >= '1' && *p <= '9') grp = grp * 10 + (*p - '0');
        if (!wcscmp(gs, L"3;2;0")) grp = 32;
        CURRENCYFMTW f = { nls_num(FIXED_LOCALES[i], L_ICURRDIGITS, &ok), nls_num(FIXED_LOCALES[i], L_ILZERO, &ok), grp, dec, thou,
                           nls_num(FIXED_LOCALES[i], L_INEGCURR, &ok), nls_num(FIXED_LOCALES[i], L_ICURRENCY, &ok), sym };
        int r1 = cf(FIXED_LOCALES[i], N_NOUSER, L"-9876543.2109", NULL, b1, 100);
        int r2 = cf(FIXED_LOCALES[i], 0, L"-9876543.2109", &f, b2, 100);
        CHECKF(r1 > 0 && r2 > 0 && !lstrcmpW(b1, b2), "%ls: '%ls' vs '%ls'", FIXED_LOCALES[i], b1, b2);
    }
}
