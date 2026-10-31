/* GetLocaleInfo{A,W,Ex} argument validation and buffer semantics. */
#include "nlsutil.h"

TEST(locerr_ex_invalid_lctype) {
    REQUIRE(U(GetLocaleInfoEx));
    static const DWORD bad[] = { 0xFFFF, 0x2FFF, 0x0FFF, 0x7777, 0x1FFF, 0x3000, 0xFF00 };
    WCHAR b[100];
    for (int i = 0; i < ARRAYLEN(bad); i++) {
        SET_ERR(0);
        int r = U(GetLocaleInfoEx)(L"en-US", bad[i] | N_NOUSER, b, 100);
        CHECKF(r == 0, "type 0x%lx r=%d", bad[i], r);
        EXPECT_ERR(ERROR_INVALID_FLAGS);
    }
}
TEST(locerr_ex_lctype_zero) {
    REQUIRE(U(GetLocaleInfoEx));
    WCHAR b[100]; SET_ERR(0);
    int r = U(GetLocaleInfoEx)(L"en-US", 0, b, 100);
    t_note("GetLocaleInfoEx(type 0) r=%d err=%lu", r, GetLastError());
    CHECK(r == 0);
}
TEST(locerr_w_invalid_lctype) {
    REQUIRE(U(GetLocaleInfoW));
    WCHAR b[100];
    SET_ERR(0); CHECK(U(GetLocaleInfoW)(0x409, 0xFFFF, b, 100) == 0); EXPECT_ERR(ERROR_INVALID_FLAGS);
}
TEST(locerr_a_invalid_lctype) {
    REQUIRE(U(GetLocaleInfoA));
    char b[100];
    SET_ERR(0); CHECK(U(GetLocaleInfoA)(0x409, 0xFFFF, b, 100) == 0); EXPECT_ERR(ERROR_INVALID_FLAGS);
}
TEST(locerr_ex_null_buffer_nonzero_size) {
    REQUIRE(U(GetLocaleInfoEx));
    if (t_is_wine()) { t_skip("emulator faults on NULL buffer"); return; }
    SET_ERR(0); CHECK(U(GetLocaleInfoEx)(L"en-US", LOCALE_SNAME, NULL, 10) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(U(GetLocaleInfoEx)(L"en-US", LOCALE_SNAME, NULL, 1) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(locerr_ex_negative_size) {
    REQUIRE(U(GetLocaleInfoEx));
    WCHAR b[10];
    SET_ERR(0); CHECK(U(GetLocaleInfoEx)(L"en-US", LOCALE_SNAME, b, -1) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(U(GetLocaleInfoEx)(L"en-US", LOCALE_SNAME, b, INT_MIN) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(U(GetLocaleInfoEx)(L"en-US", LOCALE_SNAME, NULL, -1) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(locerr_ex_bad_names) {
    REQUIRE(U(GetLocaleInfoEx));
    static const WCHAR *bad[] = { L"!!!", L"xx", L"not a locale", L"12345", L"en US", L"en-US-" , L"zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz" };
    WCHAR b[100];
    for (int i = 0; i < ARRAYLEN(bad); i++) {
        SET_ERR(0);
        int r = U(GetLocaleInfoEx)(bad[i], LOCALE_SNAME, b, 100);
        if (r) { t_note("GetLocaleInfoEx(%ls) succeeded: %ls", bad[i], b); continue; }
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(locerr_ex_overlong_name) {
    REQUIRE(U(GetLocaleInfoEx));
    WCHAR name[300]; for (int i = 0; i < 299; i++) name[i] = 'a'; name[299] = 0;
    WCHAR b[100]; SET_ERR(0);
    int r = U(GetLocaleInfoEx)(name, LOCALE_SNAME, b, 100);
    CHECK(r == 0);
}
TEST(locerr_ex_case_and_underscore) {
    REQUIRE(U(GetLocaleInfoEx));
    static const WCHAR *vars[] = { L"EN-US", L"en-us", L"En-uS", L"en_US", L"EN_us" };
    for (int i = 0; i < ARRAYLEN(vars); i++) {
        WCHAR b[100]; int r = U(GetLocaleInfoEx)(vars[i], LOCALE_SNAME | N_NOUSER, b, 100);
        CHECKF(r == 6 && !lstrcmpW(b, L"en-US"), "%ls -> %ls (%d)", vars[i], b, r);
    }
}
TEST(locerr_ex_size_query_for_all_fixed) {
    REQUIRE(U(GetLocaleInfoEx));
    for (int i = 0; i < ARRAYLEN(FIXED_LOCALES); i++) {
        int n = U(GetLocaleInfoEx)(FIXED_LOCALES[i], LOCALE_SNAME, NULL, 0);
        CHECKF(n == lstrlenW(FIXED_LOCALES[i]) + 1, "%ls n=%d", FIXED_LOCALES[i], n);
    }
}
TEST(locerr_ex_size_query_ignores_buffer) {
    REQUIRE(U(GetLocaleInfoEx));
    WCHAR b[4] = { 0x1234, 0x1234, 0x1234, 0x1234 };
    int n = U(GetLocaleInfoEx)(L"en-US", LOCALE_SENGLISHDISPLAYNAME | N_NOUSER, b, 0);
    CHECK(n > 4);
    CHECK(b[0] == 0x1234 && b[3] == 0x1234);   /* nothing written for a size query */
}
TEST(locerr_ex_truncation_does_not_overflow) {
    REQUIRE(U(GetLocaleInfoEx));
    for (int cch = 1; cch < 6; cch++) {
        WCHAR b[16]; for (int k = 0; k < 16; k++) b[k] = 0x7777;
        SET_ERR(0);
        int r = U(GetLocaleInfoEx)(L"en-US", LOCALE_SNAME, b, cch);
        CHECKF(r == 0, "cch=%d r=%d", cch, r);
        EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
        CHECKF(b[cch] == 0x7777 && b[15] == 0x7777, "overflow at cch=%d", cch);
    }
}
TEST(locerr_ex_exact_size_writes_nul) {
    REQUIRE(U(GetLocaleInfoEx));
    WCHAR b[16]; for (int k = 0; k < 16; k++) b[k] = 0x7777;
    int r = U(GetLocaleInfoEx)(L"en-US", LOCALE_SNAME, b, 6);
    CHECK(r == 6); CHECK(b[5] == 0); CHECK(b[6] == 0x7777);
}
TEST(locerr_ex_empty_string_results) {
    REQUIRE(U(GetLocaleInfoEx));
    WCHAR b[8] = { 1, 1, 1, 1 };
    SET_ERR(0);
    CHECK(U(GetLocaleInfoEx)(L"en-US", LOCALE_SPOSITIVESIGN | N_NOUSER, NULL, 0) == 1);
    CHECK(U(GetLocaleInfoEx)(L"en-US", LOCALE_SPOSITIVESIGN | N_NOUSER, b, 8) == 1);
    CHECK(b[0] == 0);
    CHECK(U(GetLocaleInfoEx)(L"en-US", LOCALE_SPOSITIVESIGN | N_NOUSER, b, 1) == 1);
    CHECK(U(GetLocaleInfoEx)(L"en-US", LOCALE_SMONTHNAME13 | N_NOUSER, b, 1) == 1);
}
TEST(locerr_retnum_sizes_w) {
    REQUIRE(U(GetLocaleInfoEx));
    DWORD v = 0xDEADBEEF; WCHAR *p = (WCHAR *)&v;
    SET_ERR(0);
    CHECK(U(GetLocaleInfoEx)(L"en-US", LOCALE_ICOUNTRY | N_NOUSER | N_RETNUM, NULL, 0) == 2);
    CHECK(U(GetLocaleInfoEx)(L"en-US", LOCALE_ICOUNTRY | N_NOUSER | N_RETNUM, p, 2) == 2);
    CHECK(v == 1);
    v = 0xDEADBEEF;
    WCHAR big[8] = { 0xAAAA, 0xAAAA, 0xAAAA };
    CHECK(U(GetLocaleInfoEx)(L"en-US", LOCALE_ICOUNTRY | N_NOUSER | N_RETNUM, big, 8) == 2);
    CHECK(*(DWORD *)big == 1);
    CHECK(big[2] == 0xAAAA);
}
TEST(locerr_retnum_small_buffer_w) {
    REQUIRE(U(GetLocaleInfoEx));
    WCHAR b[2] = { 0x1111, 0x2222 };
    SET_ERR(0);
    int r = U(GetLocaleInfoEx)(L"en-US", LOCALE_ICOUNTRY | N_NOUSER | N_RETNUM, b, 1);
    CHECK(r == 0);
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
}
TEST(locerr_retnum_a_sizes) {
    REQUIRE(U(GetLocaleInfoA));
    DWORD v = 0xDEADBEEF; SET_ERR(0);
    int n = U(GetLocaleInfoA)(0x409, LOCALE_ICOUNTRY | N_NOUSER | N_RETNUM, NULL, 0);
    t_note("GetLocaleInfoA RETURN_NUMBER size query = %d", n);
    n = U(GetLocaleInfoA)(0x409, LOCALE_ICOUNTRY | N_NOUSER | N_RETNUM, (char *)&v, 4);
    CHECK(n == 4); CHECK(v == 1);
}
TEST(locerr_retnum_on_string_type) {
    REQUIRE(U(GetLocaleInfoEx));
    DWORD v = 0; SET_ERR(0);
    int r = U(GetLocaleInfoEx)(L"en-US", LOCALE_SDECIMAL | N_NOUSER | N_RETNUM, (WCHAR *)&v, 2);
    CHECK(r == 0);
    t_note("RETURN_NUMBER on SDECIMAL: err=%lu", GetLastError());
}
TEST(locerr_w_null_buffer_nonzero) {
    REQUIRE(U(GetLocaleInfoW));
    SET_ERR(0); CHECK(U(GetLocaleInfoW)(0x409, LOCALE_SNAME, NULL, 5) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(U(GetLocaleInfoW)(0x409, LOCALE_SNAME, NULL, -1) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(U(GetLocaleInfoW)(0x409, LOCALE_SNAME, NULL, 0) == 6);
}
TEST(locerr_a_null_buffer_nonzero) {
    REQUIRE(U(GetLocaleInfoA));
    SET_ERR(0); CHECK(U(GetLocaleInfoA)(0x409, LOCALE_SNAME, NULL, 5) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(U(GetLocaleInfoA)(0x409, LOCALE_SNAME, NULL, -1) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(U(GetLocaleInfoA)(0x409, LOCALE_SNAME, NULL, 0) == 6);
}
TEST(locerr_a_truncation) {
    REQUIRE(U(GetLocaleInfoA));
    char b[16];
    for (int cch = 1; cch < 6; cch++) {
        memset(b, 0x77, sizeof b); SET_ERR(0);
        int r = U(GetLocaleInfoA)(0x409, LOCALE_SNAME, b, cch);
        CHECKF(r == 0, "cch=%d r=%d", cch, r);
        EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
        CHECK(b[cch] == 0x77);
    }
    memset(b, 0x77, sizeof b);
    CHECK(U(GetLocaleInfoA)(0x409, LOCALE_SNAME, b, 6) == 6);
    CHECK(!strcmp(b, "en-US"));
}
TEST(locerr_a_size_query_dbcs) {
    REQUIRE(U(GetLocaleInfoA)); REQUIRE(U(GetLocaleInfoW));
    /* ja-JP day name: 3 wide chars + NUL; in CP932 it is 6 bytes + NUL. */
    char b[64]; WCHAR w[64];
    int na = U(GetLocaleInfoA)(0x411, LOCALE_SDAYNAME1 | N_NOUSER, NULL, 0);
    int nw = U(GetLocaleInfoW)(0x411, LOCALE_SDAYNAME1 | N_NOUSER, NULL, 0);
    CHECKF(nw == 4, "nw=%d", nw);
    CHECKF(na == 7, "na=%d", na);
    CHECK(U(GetLocaleInfoA)(0x411, LOCALE_SDAYNAME1 | N_NOUSER, b, 64) == 7);
    CHECK((UCHAR)b[0] == 0x8c && (UCHAR)b[1] == 0x8e);   /* U+6708 in CP932 */
    CHECK(U(GetLocaleInfoW)(0x411, LOCALE_SDAYNAME1 | N_NOUSER, w, 64) == 4);
    CHECK(w[0] == 0x6708);
}
TEST(locerr_a_use_cp_acp) {
    REQUIRE(U(GetLocaleInfoA));
    char b[128], c[128];
    /* USE_CP_ACP converts with the system ANSI code page rather than the locale's. */
    int r = U(GetLocaleInfoA)(0x409, LOCALE_SENGLANGUAGE | N_USECP | N_NOUSER, b, 128);
    int r2 = U(GetLocaleInfoA)(0x409, LOCALE_SENGLANGUAGE | N_NOUSER, c, 128);
    CHECK(r == r2); CHECK(!strcmp(b, c)); CHECK(!strcmp(b, "English"));
    /* German month name with umlaut: system ACP vs 1252 locale ACP. */
    r = U(GetLocaleInfoA)(0x407, LOCALE_SMONTHNAME3 | N_USECP | N_NOUSER, b, 128);
    CHECK(r > 0);
    WCHAR w[32]; U(GetLocaleInfoW)(0x407, LOCALE_SMONTHNAME3 | N_NOUSER, w, 32);
    char ex[64]; WideCharToMultiByte(CP_ACP, 0, w, -1, ex, 64, NULL, NULL);
    CHECK(!strcmp(b, ex));
}
TEST(locerr_nouseroverride_equal_for_fixed) {
    REQUIRE(U(GetLocaleInfoEx));
    WCHAR u[100], d[100];
    WCHAR def[100]; U(GetUserDefaultLocaleName) ? U(GetUserDefaultLocaleName)(def, 100) : 0;
    static const DWORD t[] = { LOCALE_SDECIMAL, LOCALE_STHOUSAND, LOCALE_SSHORTDATE, LOCALE_SLONGDATE, LOCALE_STIMEFORMAT,
                               LOCALE_SDATE, LOCALE_STIME, LOCALE_SGROUPING, LOCALE_IDIGITS, LOCALE_ICURRENCY };
    for (int i = 0; i < ARRAYLEN(FIXED_LOCALES); i++) {
        if (!lstrcmpiW(FIXED_LOCALES[i], def)) continue;   /* the user's own locale may carry overrides */
        for (int k = 0; k < ARRAYLEN(t); k++) {
            int a = U(GetLocaleInfoEx)(FIXED_LOCALES[i], t[k], u, 100);
            int b = U(GetLocaleInfoEx)(FIXED_LOCALES[i], t[k] | N_NOUSER, d, 100);
            CHECKF(a == b && !lstrcmpW(u, d), "%ls type 0x%lx", FIXED_LOCALES[i], t[k]);
        }
    }
}
TEST(locerr_repeat_stability) {
    REQUIRE(U(GetLocaleInfoEx));
    WCHAR b[100];
    for (int i = 0; i < 2000; i++) {
        int r = U(GetLocaleInfoEx)(L"de-DE", LOCALE_SMONTHNAME3 | N_NOUSER, b, 100);
        if (r != 5 || b[1] != 0xe4) { CHECKF(0, "iter %d r=%d", i, r); break; }
    }
    CHECK(1);
}
TEST(locerr_parent_names) {
    REQUIRE(U(GetLocaleInfoEx));
    WCHAR b[100];
    CHECK(nls_str(L"en", L_SPARENT, b, 100) >= 1);
    CHECK(b[0] == 0);              /* neutral locales have no parent below the invariant */
    nls_str(L"en-US", L_SPARENT, b, 100); CHECK(!lstrcmpW(b, L"en"));
    nls_str(L"de-DE", L_SPARENT, b, 100); CHECK(!lstrcmpW(b, L"de"));
    nls_str(L"fr-CA", L_SPARENT, b, 100); CHECK(!lstrcmpW(b, L"fr"));
    int r = nls_str(L"zh-Hans", L_SPARENT, b, 100);
    if (r > 0 && lstrcmpW(b, L"zh")) t_note("zh-Hans parent=%ls", b);
}
TEST(locerr_neutral_locale_info) {
    REQUIRE(U(GetLocaleInfoEx));
    WCHAR b[100];
    nls_str(L"en", L_SNAME, b, 100); CHECK(!lstrcmpW(b, L"en"));
    nls_str(L"de", L_SNAME, b, 100); CHECK(!lstrcmpW(b, L"de"));
    nls_str(L"en", L_SISO639LANGNAME, b, 100); CHECK(!lstrcmpW(b, L"en"));
    int ok; DWORD v = nls_num(L"en", L_INEUTRAL, &ok); CHECK(ok); CHECK(v == 1);
    v = nls_num(L"en-US", L_INEUTRAL, &ok); CHECK(ok); CHECK(v == 0);
    v = nls_num(L"", L_INEUTRAL, &ok); CHECK(ok);
    nls_str(L"en", L_ILANGUAGE, b, 100); if (lstrcmpiW(b, L"0009")) t_note("en ILANGUAGE=%ls", b);
    nls_str(L"de", L_ILANGUAGE, b, 100); if (lstrcmpiW(b, L"0007")) t_note("de ILANGUAGE=%ls", b);
}
TEST(locerr_reading_layout) {
    REQUIRE(U(GetLocaleInfoEx));
    int ok;
    DWORD v = nls_num(L"en-US", L_IREADINGLAYOUT, &ok); CHECK(ok); CHECK(v == 0);   /* left to right */
    v = nls_num(L"ar-SA", L_IREADINGLAYOUT, &ok); CHECK(ok); CHECK(v == 1);           /* right to left */
    v = nls_num(L"he-IL", L_IREADINGLAYOUT, &ok); CHECK(ok); CHECK(v == 1);
    v = nls_num(L"ja-JP", L_IREADINGLAYOUT, &ok); CHECK(ok);
    v = nls_num(L"de-DE", L_IREADINGLAYOUT, &ok); CHECK(ok); CHECK(v == 0);
}
TEST(locerr_fontsignature) {
    REQUIRE(U(GetLocaleInfoEx));
    WCHAR b[32]; SET_ERR(0);
    int n = U(GetLocaleInfoEx)(L"en-US", 0x58, NULL, 0);
    CHECKF(n == 16, "n=%d", n);                      /* LOCALE_FONTSIGNATURE is a 16-WCHAR binary blob */
    n = U(GetLocaleInfoEx)(L"en-US", 0x58, b, 32);
    CHECK(n == 16);
    SET_ERR(0); n = U(GetLocaleInfoEx)(L"en-US", 0x58, b, 15);
    CHECK(n == 0); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
}
TEST(locerr_display_names_nonempty) {
    REQUIRE(U(GetLocaleInfoEx));
    for (int i = 0; i < ARRAYLEN(FIXED_LOCALES); i++) {
        WCHAR b[200];
        CHECKF(nls_str(FIXED_LOCALES[i], L_SENGDISPLAYNAME, b, 200) > 2, "%ls", FIXED_LOCALES[i]);
        CHECKF(nls_str(FIXED_LOCALES[i], L_SNATDISPLAYNAME, b, 200) > 2, "%ls", FIXED_LOCALES[i]);
        CHECKF(nls_str(FIXED_LOCALES[i], L_SENGLANGNAME, b, 200) > 2, "%ls", FIXED_LOCALES[i]);
        CHECKF(nls_str(FIXED_LOCALES[i], L_SENGCOUNTRY, b, 200) > 2, "%ls", FIXED_LOCALES[i]);
    }
    WCHAR b[200];
    nls_str(L"de-DE", L_SENGDISPLAYNAME, b, 200); EXPECT_WS(0, L"de-DE", "engdisplay", b, L"German (Germany)");
    nls_str(L"fr-FR", L_SENGDISPLAYNAME, b, 200); EXPECT_WS(0, L"fr-FR", "engdisplay", b, L"French (France)");
    nls_str(L"ja-JP", L_SENGDISPLAYNAME, b, 200); EXPECT_WS(0, L"ja-JP", "engdisplay", b, L"Japanese (Japan)");
}
TEST(locerr_sortname_present) {
    REQUIRE(U(GetLocaleInfoEx));
    WCHAR b[100];
    for (int i = 0; i < ARRAYLEN(FIXED_LOCALES); i++) CHECK(nls_str(FIXED_LOCALES[i], L_SSORTNAME, b, 100) > 1);
}
TEST(locerr_scripts_present) {
    REQUIRE(U(GetLocaleInfoEx));
    WCHAR b[100];
    for (int i = 0; i < ARRAYLEN(FIXED_LOCALES); i++) {
        int n = nls_str(FIXED_LOCALES[i], L_SSCRIPTS, b, 100);
        CHECKF(n > 1 && b[n - 2] == ';', "%ls scripts %ls", FIXED_LOCALES[i], b);
    }
}
TEST(locerr_keyboards_to_install) {
    REQUIRE(U(GetLocaleInfoEx));
    WCHAR b[100];
    int n = nls_str(L"en-US", L_SKEYBOARDSTOINSTALL, b, 100);
    CHECK(n > 1);
    if (n > 1) CHECK(wcsstr(b, L"00000409") != NULL);
    n = nls_str(L"de-DE", L_SKEYBOARDSTOINSTALL, b, 100);
    if (n > 1 && !wcsstr(b, L"00000407")) t_note("de-DE keyboards: %ls", b);
}
TEST(locerr_opentype_tag) {
    REQUIRE(U(GetLocaleInfoEx));
    WCHAR b[100];
    int n = nls_str(L"en-US", 0x7A, b, 100);
    CHECK(n > 1);
}
TEST(locerr_sduration_pattern) {
    REQUIRE(U(GetLocaleInfoEx));
    WCHAR b[100];
    for (int i = 0; i < ARRAYLEN(FIXED_LOCALES); i++) {
        CHECKF(nls_str(FIXED_LOCALES[i], L_SDURATION, b, 100) > 1, "%ls", FIXED_LOCALES[i]);
    }
}
