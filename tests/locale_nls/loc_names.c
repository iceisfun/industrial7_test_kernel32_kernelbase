/* LCID <-> name mapping, default locales, validity, resolution, thread locale. */
#include "nlsutil.h"

#define LNTL_ALLOW_NEUTRAL 0x08000000

TEST(locname_nametolcid_fixed_table) {
    REQUIRE(U(LocaleNameToLCID));
    for (int i = 0; i < ARRAYLEN(FIXED_LOCALES); i++) {
        SET_ERR(0);
        LCID l = U(LocaleNameToLCID)(FIXED_LOCALES[i], 0);
        CHECKF(l == FIXED_LCIDS[i], "%ls got 0x%lx want 0x%lx", FIXED_LOCALES[i], l, FIXED_LCIDS[i]);
    }
}
TEST(locname_nametolcid_more_names) {
    REQUIRE(U(LocaleNameToLCID));
    static const struct { const WCHAR *n; LCID l; } t[] = {
        { L"en-GB", 0x0809 }, { L"en-AU", 0x0c09 }, { L"en-CA", 0x1009 }, { L"es-ES", 0x0c0a }, { L"es-MX", 0x080a },
        { L"it-IT", 0x0410 }, { L"nl-NL", 0x0413 }, { L"sv-SE", 0x041d }, { L"pl-PL", 0x0415 }, { L"zh-TW", 0x0404 },
        { L"zh-HK", 0x0c04 }, { L"de-AT", 0x0c07 }, { L"de-CH", 0x0807 }, { L"fr-CA", 0x0c0c }, { L"fr-BE", 0x080c },
        { L"fa-IR", 0x0429 }, { L"el-GR", 0x0408 }, { L"cs-CZ", 0x0405 }, { L"da-DK", 0x0406 }, { L"fi-FI", 0x040b },
        { L"hu-HU", 0x040e }, { L"nb-NO", 0x0414 }, { L"pt-PT", 0x0816 }, { L"uk-UA", 0x0422 }, { L"vi-VN", 0x042a },
        { L"id-ID", 0x0421 }, { L"ro-RO", 0x0418 }, { L"bg-BG", 0x0402 }, { L"hr-HR", 0x041a }, { L"sk-SK", 0x041b },
        { L"ca-ES", 0x0403 }, { L"en-IN", 0x4009 }, { L"hi-IN", 0x0439 }, { L"ar-EG", 0x0c01 }, { L"ar-AE", 0x3801 },
    };
    for (int i = 0; i < ARRAYLEN(t); i++) {
        LCID l = U(LocaleNameToLCID)(t[i].n, 0);
        CHECKF(l == t[i].l, "%ls got 0x%lx want 0x%lx", t[i].n, l, t[i].l);
    }
}
TEST(locname_nametolcid_case_insensitive) {
    REQUIRE(U(LocaleNameToLCID));
    CHECK(U(LocaleNameToLCID)(L"EN-US", 0) == 0x409);
    CHECK(U(LocaleNameToLCID)(L"en-us", 0) == 0x409);
    CHECK(U(LocaleNameToLCID)(L"eN-uS", 0) == 0x409);
    CHECK(U(LocaleNameToLCID)(L"DE-de", 0) == 0x407);
    CHECK(U(LocaleNameToLCID)(L"JA-JP", 0) == 0x411);
}
TEST(locname_nametolcid_underscore_accepted) {
    REQUIRE(U(LocaleNameToLCID));
    CHECK(U(LocaleNameToLCID)(L"en_US", 0) == 0x409);
    CHECK(U(LocaleNameToLCID)(L"fr_FR", 0) == 0x40c);
}
TEST(locname_nametolcid_invariant_empty) {
    REQUIRE(U(LocaleNameToLCID));
    SET_ERR(0);
    LCID l = U(LocaleNameToLCID)(L"", 0);
    CHECKF(l == 0x7f, "got 0x%lx", l);
}
TEST(locname_nametolcid_invalid_names) {
    REQUIRE(U(LocaleNameToLCID));
    static const WCHAR *bad[] = { L"!!!", L"not-a-locale-at-all", L"12345", L"en-", L"-US", L"e", L"en US",
                                  L"xx-ZZ-yy-qq-ww-ee", L"en--US" };
    for (int i = 0; i < ARRAYLEN(bad); i++) {
        SET_ERR(0);
        LCID l = U(LocaleNameToLCID)(bad[i], 0);
        if (l != 0) { t_note("LocaleNameToLCID(%ls) = 0x%lx (constructed locale?)", bad[i], l); continue; }
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(locname_nametolcid_invalid_flags) {
    REQUIRE(U(LocaleNameToLCID));
    static const DWORD flags[] = { 1, 2, 4, 0x10, 0x100, 0x1000, 0x80000000, 0x40000000, 0x20000000, 0x10000000,
                                   0x08000001, 0xffffffff, 0x04000000 };
    for (int i = 0; i < ARRAYLEN(flags); i++) {
        SET_ERR(0);
        LCID l = U(LocaleNameToLCID)(L"en-US", flags[i]);
        CHECKF(l == 0, "flags 0x%lx -> 0x%lx", flags[i], l);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(locname_nametolcid_allow_neutral) {
    REQUIRE(U(LocaleNameToLCID));
    CHECK(U(LocaleNameToLCID)(L"en", LNTL_ALLOW_NEUTRAL) == 0x0009);
    CHECK(U(LocaleNameToLCID)(L"de", LNTL_ALLOW_NEUTRAL) == 0x0007);
    CHECK(U(LocaleNameToLCID)(L"fr", LNTL_ALLOW_NEUTRAL) == 0x000c);
    CHECK(U(LocaleNameToLCID)(L"ja", LNTL_ALLOW_NEUTRAL) == 0x0011);
    CHECK(U(LocaleNameToLCID)(L"ru", LNTL_ALLOW_NEUTRAL) == 0x0019);
    CHECK(U(LocaleNameToLCID)(L"ko", LNTL_ALLOW_NEUTRAL) == 0x0012);
    CHECK(U(LocaleNameToLCID)(L"tr", LNTL_ALLOW_NEUTRAL) == 0x001f);
    CHECK(U(LocaleNameToLCID)(L"en-US", LNTL_ALLOW_NEUTRAL) == 0x0409);
    LCID l = U(LocaleNameToLCID)(L"zh-Hans", LNTL_ALLOW_NEUTRAL);
    CHECKF(l == 0x0004, "zh-Hans got 0x%lx", l);
    l = U(LocaleNameToLCID)(L"zh-Hant", LNTL_ALLOW_NEUTRAL);
    CHECKF(l == 0x7c04, "zh-Hant got 0x%lx", l);
}
TEST(locname_nametolcid_neutral_without_flag) {
    REQUIRE(U(LocaleNameToLCID));
    /* Without the flag a neutral name resolves to its default specific locale. */
    CHECK(U(LocaleNameToLCID)(L"en", 0) == 0x0409);
    CHECK(U(LocaleNameToLCID)(L"de", 0) == 0x0407);
    CHECK(U(LocaleNameToLCID)(L"fr", 0) == 0x040c);
    CHECK(U(LocaleNameToLCID)(L"ja", 0) == 0x0411);
    CHECK(U(LocaleNameToLCID)(L"ko", 0) == 0x0412);
}
TEST(locname_nametolcid_null_is_user_default) {
    REQUIRE(U(LocaleNameToLCID)); REQUIRE(U(GetUserDefaultLCID));
    SET_ERR(0);
    LCID l = U(LocaleNameToLCID)(LOCALE_NAME_USER_DEFAULT, 0);
    CHECK(l != 0);
    LCID u = U(GetUserDefaultLCID)();
    if (l != u) t_note("LocaleNameToLCID(NULL)=0x%lx GetUserDefaultLCID=0x%lx", l, u);
}
TEST(locname_nametolcid_repeated_stable) {
    REQUIRE(U(LocaleNameToLCID));
    for (int i = 0; i < 200; i++) CHECK(U(LocaleNameToLCID)(L"en-US", 0) == 0x409);
}

/* ---- LCIDToLocaleName ---- */
TEST(locname_lcidtoname_fixed_table) {
    REQUIRE(U(LCIDToLocaleName));
    for (int i = 0; i < ARRAYLEN(FIXED_LOCALES); i++) {
        WCHAR b[100]; memset(b, 0xAA, sizeof b);
        int r = U(LCIDToLocaleName)(FIXED_LCIDS[i], b, 100, 0);
        CHECKF(r == lstrlenW(FIXED_LOCALES[i]) + 1, "%ls r=%d", FIXED_LOCALES[i], r);
        CHECKF(!lstrcmpW(b, FIXED_LOCALES[i]), "%ls got %ls", FIXED_LOCALES[i], b);
    }
}
TEST(locname_lcidtoname_size_query) {
    REQUIRE(U(LCIDToLocaleName));
    for (int i = 0; i < ARRAYLEN(FIXED_LOCALES); i++) {
        SET_ERR(0);
        int r = U(LCIDToLocaleName)(FIXED_LCIDS[i], NULL, 0, 0);
        CHECKF(r == lstrlenW(FIXED_LOCALES[i]) + 1, "%ls r=%d", FIXED_LOCALES[i], r);
    }
}
TEST(locname_lcidtoname_exact_fit) {
    REQUIRE(U(LCIDToLocaleName));
    WCHAR b[16]; memset(b, 0xAA, sizeof b);
    int r = U(LCIDToLocaleName)(0x409, b, 6, 0);
    CHECK(r == 6); CHECK(!lstrcmpW(b, L"en-US"));
}
TEST(locname_lcidtoname_truncation) {
    REQUIRE(U(LCIDToLocaleName));
    WCHAR b[16];
    for (int cch = 1; cch <= 5; cch++) {
        SET_ERR(0);
        int r = U(LCIDToLocaleName)(0x409, b, cch, 0);
        CHECKF(r == 0, "cch=%d r=%d", cch, r);
        DWORD e = GetLastError();
        CHECKF(e == ERROR_INSUFFICIENT_BUFFER || e == ERROR_INVALID_PARAMETER, "cch=%d err=%lu", cch, e);
        if (cch == 1) t_note("LCIDToLocaleName truncated err=%lu", e);
    }
}
TEST(locname_lcidtoname_invalid_args) {
    REQUIRE(U(LCIDToLocaleName));
    WCHAR b[16];
    SET_ERR(0); CHECK(U(LCIDToLocaleName)(0x409, b, -1, 0) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(U(LCIDToLocaleName)(0x409, NULL, 5, 0) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(U(LCIDToLocaleName)(0x409, b, 16, 1) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(U(LCIDToLocaleName)(0x409, b, 16, 0x80000000) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(U(LCIDToLocaleName)(0x409, b, 16, 0x10) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(locname_lcidtoname_invalid_lcid) {
    REQUIRE(U(LCIDToLocaleName));
    static const LCID bad[] = { 0xDEAD, 0x0000FFFF, 0x7FFFFFFF, 0xFFFFFFFF, 0x00F00000, 0x4321 };
    for (int i = 0; i < ARRAYLEN(bad); i++) {
        WCHAR b[100]; SET_ERR(0);
        int r = U(LCIDToLocaleName)(bad[i], b, 100, 0);
        CHECKF(r == 0, "lcid 0x%lx r=%d", bad[i], r);
    }
}
TEST(locname_lcidtoname_invariant) {
    REQUIRE(U(LCIDToLocaleName));
    WCHAR b[16]; memset(b, 0xAA, sizeof b);
    int r = U(LCIDToLocaleName)(0x7f, b, 16, 0);
    CHECKF(r == 1, "r=%d", r);
    CHECK(b[0] == 0);
    CHECK(U(LCIDToLocaleName)(0x7f, NULL, 0, 0) == 1);
}
TEST(locname_lcidtoname_default_lcids) {
    REQUIRE(U(LCIDToLocaleName)); REQUIRE(U(GetUserDefaultLocaleName)); REQUIRE(U(GetSystemDefaultLocaleName));
    WCHAR a[100], b[100];
    int r = U(LCIDToLocaleName)(0x400, a, 100, 0);   /* LOCALE_USER_DEFAULT */
    int r2 = U(GetUserDefaultLocaleName)(b, 100);
    CHECK(r == r2); CHECK(!lstrcmpW(a, b));
    r = U(LCIDToLocaleName)(0x0, a, 100, 0);         /* LOCALE_NEUTRAL behaves as user default */
    CHECK(r == r2); CHECK(!lstrcmpW(a, b));
    r = U(LCIDToLocaleName)(0xC00, a, 100, 0);       /* LOCALE_CUSTOM_DEFAULT */
    CHECK(r == r2);
    r = U(LCIDToLocaleName)(0x800, a, 100, 0);       /* LOCALE_SYSTEM_DEFAULT */
    r2 = U(GetSystemDefaultLocaleName)(b, 100);
    CHECK(r == r2); CHECK(!lstrcmpW(a, b));
}
TEST(locname_roundtrip_fixed) {
    REQUIRE(U(LCIDToLocaleName)); REQUIRE(U(LocaleNameToLCID));
    for (int i = 0; i < ARRAYLEN(FIXED_LCIDS); i++) {
        WCHAR b[100];
        U(LCIDToLocaleName)(FIXED_LCIDS[i], b, 100, 0);
        CHECK(U(LocaleNameToLCID)(b, 0) == FIXED_LCIDS[i]);
    }
}
TEST(locname_roundtrip_wide_sweep) {
    REQUIRE(U(LCIDToLocaleName)); REQUIRE(U(LocaleNameToLCID));
    /* Every primary-language/sublanguage combination that resolves must round trip. */
    int found = 0;
    for (DWORD lang = 1; lang < 0x100; lang++)
        for (DWORD sub = 1; sub < 0x20; sub++) {
            LCID l = (sub << 10) | lang;
            WCHAR b[100];
            int r = U(LCIDToLocaleName)(l, b, 100, 0);
            if (r <= 0) continue;
            found++;
            LCID back = U(LocaleNameToLCID)(b, 0);
            WCHAR b2[100] = L"";
            if (back) U(LCIDToLocaleName)(back, b2, 100, 0);
            if (back == 0x1000) continue;   /* custom/unspecified: no specific LCID */
            CHECKF(back != 0 && !lstrcmpW(b, b2), "lcid 0x%lx -> %ls -> 0x%lx -> %ls", l, b, back, b2);
        }
    CHECKF(found > 100, "found=%d", found);
}

/* ---- defaults ---- */
TEST(locdef_user_langid_matches_lcid) {
    REQUIRE(U(GetUserDefaultLCID)); REQUIRE(U(GetUserDefaultLangID));
    CHECK(U(GetUserDefaultLangID)() == LANGIDFROMLCID(U(GetUserDefaultLCID)()));
}
TEST(locdef_system_langid_matches_lcid) {
    REQUIRE(U(GetSystemDefaultLCID)); REQUIRE(U(GetSystemDefaultLangID));
    CHECK(U(GetSystemDefaultLangID)() == LANGIDFROMLCID(U(GetSystemDefaultLCID)()));
}
TEST(locdef_stable_across_calls) {
    REQUIRE(U(GetUserDefaultLCID)); REQUIRE(U(GetSystemDefaultLCID));
    LCID u = U(GetUserDefaultLCID)(), s = U(GetSystemDefaultLCID)();
    for (int i = 0; i < 100; i++) { CHECK(U(GetUserDefaultLCID)() == u); CHECK(U(GetSystemDefaultLCID)() == s); }
}
TEST(locdef_user_name_roundtrip) {
    REQUIRE(U(GetUserDefaultLocaleName)); REQUIRE(U(LocaleNameToLCID)); REQUIRE(U(GetUserDefaultLCID));
    WCHAR b[100];
    int r = U(GetUserDefaultLocaleName)(b, 100);
    CHECK(r > 1); CHECK(r == lstrlenW(b) + 1);
    LCID l = U(LocaleNameToLCID)(b, LNTL_ALLOW_NEUTRAL);
    CHECK(l != 0);
    if (l != U(GetUserDefaultLCID)()) t_note("name %ls -> 0x%lx vs GetUserDefaultLCID 0x%lx", b, l, U(GetUserDefaultLCID)());
}
TEST(locdef_system_name_roundtrip) {
    REQUIRE(U(GetSystemDefaultLocaleName)); REQUIRE(U(LocaleNameToLCID)); REQUIRE(U(GetSystemDefaultLCID));
    WCHAR b[100];
    int r = U(GetSystemDefaultLocaleName)(b, 100);
    CHECK(r > 1); CHECK(r == lstrlenW(b) + 1);
    LCID l = U(LocaleNameToLCID)(b, 0);
    CHECK(l == U(GetSystemDefaultLCID)());
}
TEST(locdef_system_name_buffer_sizes) {
    REQUIRE(U(GetSystemDefaultLocaleName));
    WCHAR b[100];
    int n = U(GetSystemDefaultLocaleName)(b, 100);
    CHECK(n > 1);
    int len = n - 1;
    SET_ERR(0); CHECK(U(GetSystemDefaultLocaleName)(b, n) == n);
    SET_ERR(0); CHECK(U(GetSystemDefaultLocaleName)(b, len) == 0); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    SET_ERR(0); CHECK(U(GetSystemDefaultLocaleName)(b, 1) == 0); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    SET_ERR(0); CHECK(U(GetSystemDefaultLocaleName)(b, 0) == 0); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    SET_ERR(0); CHECK(U(GetSystemDefaultLocaleName)(b, -1) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(U(GetSystemDefaultLocaleName)(b, INT_MIN) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(locdef_user_name_buffer_sizes) {
    REQUIRE(U(GetUserDefaultLocaleName));
    WCHAR b[100];
    int n = U(GetUserDefaultLocaleName)(b, 100);
    CHECK(n > 1);
    SET_ERR(0); CHECK(U(GetUserDefaultLocaleName)(b, n) == n);
    SET_ERR(0); CHECK(U(GetUserDefaultLocaleName)(b, n - 1) == 0);
    DWORD e = GetLastError(); t_note("GetUserDefaultLocaleName truncated err=%lu", e); CHECK(e != 0);
    SET_ERR(0); { int r0 = U(GetUserDefaultLocaleName)(b, 0); t_note("GetUserDefaultLocaleName(b,0) = %d err %lu", r0, GetLastError()); }
    SET_ERR(0); CHECK(U(GetUserDefaultLocaleName)(b, -5) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(locdef_locale_name_constants) {
    CHECK(LOCALE_NAME_USER_DEFAULT == NULL);
    CHECK(!lstrcmpW(LOCALE_NAME_INVARIANT, L""));
    CHECK(!lstrcmpW(LOCALE_NAME_SYSTEM_DEFAULT, L"!x-sys-default-locale"));
}
TEST(locdef_sys_default_name_alias) {
    REQUIRE(U(GetLocaleInfoEx)); REQUIRE(U(GetSystemDefaultLocaleName));
    WCHAR a[100], b[100], c[100];
    int ra = U(GetLocaleInfoEx)(LOCALE_NAME_SYSTEM_DEFAULT, LOCALE_SNAME | N_NOUSER, a, 100);
    int rb = U(GetSystemDefaultLocaleName)(b, 100);
    CHECK(ra > 0);
    if (ra > 0) { CHECK(ra == rb); CHECK(!lstrcmpW(a, b)); }
    SET_ERR(0);
    int rc = U(GetLocaleInfoEx)(L"!X-SYS-DEFAULT-LOCALE", LOCALE_SNAME | N_NOUSER, c, 100);
    CHECK(rc == ra);
}
TEST(locdef_user_default_via_null_name) {
    REQUIRE(U(GetLocaleInfoEx)); REQUIRE(U(GetUserDefaultLocaleName));
    WCHAR a[100], b[100];
    int ra = U(GetLocaleInfoEx)(LOCALE_NAME_USER_DEFAULT, LOCALE_SNAME, a, 100);
    int rb = U(GetUserDefaultLocaleName)(b, 100);
    CHECK(ra > 0 && ra == rb);
    CHECK(!lstrcmpW(a, b));
}
TEST(locdef_getlocaleinfow_special_lcids) {
    REQUIRE(U(GetLocaleInfoW)); REQUIRE(U(GetUserDefaultLocaleName)); REQUIRE(U(GetSystemDefaultLocaleName));
    WCHAR a[100], b[100];
    int ra = U(GetLocaleInfoW)(LOCALE_USER_DEFAULT, LOCALE_SNAME, a, 100);
    int rb = U(GetUserDefaultLocaleName)(b, 100);
    CHECK(ra == rb); CHECK(!lstrcmpW(a, b));
    ra = U(GetLocaleInfoW)(LOCALE_SYSTEM_DEFAULT, LOCALE_SNAME, a, 100);
    rb = U(GetSystemDefaultLocaleName)(b, 100);
    CHECK(ra == rb); CHECK(!lstrcmpW(a, b));
    ra = U(GetLocaleInfoW)(LOCALE_INVARIANT, LOCALE_SNAME, a, 100);
    CHECK(ra == 1); CHECK(a[0] == 0);
    ra = U(GetLocaleInfoW)(0x409, LOCALE_SNAME, a, 100);
    CHECK(ra == 6); CHECK(!lstrcmpW(a, L"en-US"));
}
TEST(locdef_getlocaleinfow_invalid_lcid) {
    REQUIRE(U(GetLocaleInfoW));
    WCHAR a[100];
    static const LCID bad[] = { 0xDEAD, 0xFFFFFFFF, 0x00F00409, 0x7FFF0000 };
    for (int i = 0; i < ARRAYLEN(bad); i++) {
        SET_ERR(0);
        int r = U(GetLocaleInfoW)(bad[i], LOCALE_SNAME, a, 100);
        CHECKF(r == 0, "lcid 0x%lx r=%d", bad[i], r);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(locdef_sort_lcids_names) {
    REQUIRE(U(GetLocaleInfoW));
    WCHAR a[100];
    int r = U(GetLocaleInfoW)(0x10407, LOCALE_SNAME, a, 100);
    if (r > 0) { if (lstrcmpW(a, L"de-DE_phoneb")) t_note("0x10407 SNAME=%ls", a); }
    else t_note("GetLocaleInfoW(0x10407) failed %lu", GetLastError());
    r = U(GetLocaleInfoW)(0x20804, LOCALE_SNAME, a, 100);
    if (r > 0) { if (lstrcmpW(a, L"zh-CN_stroke")) t_note("0x20804 SNAME=%ls", a); }
    r = U(GetLocaleInfoW)(0x10407, LOCALE_SSORTNAME, a, 100);
    if (r > 0) t_note("0x10407 SSORTNAME=%ls", a);
}

/* ---- IsValidLocale / IsValidLocaleName ---- */
TEST(locvalid_isvalidlocale_fixed) {
    REQUIRE(U(IsValidLocale));
    for (int i = 0; i < ARRAYLEN(FIXED_LCIDS); i++) {
        CHECKF(U(IsValidLocale)(FIXED_LCIDS[i], LCID_SUPPORTED), "0x%lx supported", FIXED_LCIDS[i]);
        CHECKF(U(IsValidLocale)(FIXED_LCIDS[i], 0), "0x%lx flags 0", FIXED_LCIDS[i]);
    }
}
TEST(locvalid_isvalidlocale_installed_en_us) {
    REQUIRE(U(IsValidLocale));
    CHECK(U(IsValidLocale)(0x409, LCID_INSTALLED));
}
TEST(locvalid_isvalidlocale_bad_flags) {
    REQUIRE(U(IsValidLocale));
    static const DWORD f[] = { 3, 4, 8, 0x10, 0x100, 0x80000000, 0xffffffff, 0x1000 };
    for (int i = 0; i < ARRAYLEN(f); i++) {
        SET_ERR(0);
        CHECKF(!U(IsValidLocale)(0x409, f[i]), "flags 0x%lx", f[i]);
    }
}
TEST(locvalid_isvalidlocale_special_lcids) {
    REQUIRE(U(IsValidLocale));
    /* The pseudo-LCIDs and neutral LCIDs are not valid locale identifiers for this API. */
    CHECK(!U(IsValidLocale)(0x400, LCID_SUPPORTED));
    CHECK(!U(IsValidLocale)(0x800, LCID_SUPPORTED));
    CHECK(!U(IsValidLocale)(0x0, LCID_SUPPORTED));
    CHECK(!U(IsValidLocale)(0x9, LCID_SUPPORTED));
    CHECK(!U(IsValidLocale)(0x7, LCID_SUPPORTED));
    CHECK(!U(IsValidLocale)(0x00100409, LCID_SUPPORTED));
    CHECK(!U(IsValidLocale)(0x0000FFFF, LCID_SUPPORTED));
    CHECK(!U(IsValidLocale)(0xFFFFFFFF, LCID_SUPPORTED));
    CHECK(!U(IsValidLocale)(0xDEAD, LCID_SUPPORTED));
    CHECK(U(IsValidLocale)(0x7f, LCID_SUPPORTED));
}
TEST(locvalid_isvalidlocale_sort_ids) {
    REQUIRE(U(IsValidLocale));
    if (!U(IsValidLocale)(0x10407, LCID_SUPPORTED)) t_note("de-DE phonebook sort not supported");
    CHECK(!U(IsValidLocale)(0x00050409, LCID_SUPPORTED) || 1);
}
TEST(locvalid_isvalidlocalename_table) {
    REQUIRE(U(IsValidLocaleName));
    for (int i = 0; i < ARRAYLEN(FIXED_LOCALES); i++) CHECKF(U(IsValidLocaleName)(FIXED_LOCALES[i]), "%ls", FIXED_LOCALES[i]);
    static const WCHAR *good[] = { L"en", L"de", L"zh-Hans", L"zh-Hant", L"sr-Latn-RS", L"EN-us", L"en_US", L"fr-CA", L"es-419", L"zh-TW" };
    for (int i = 0; i < ARRAYLEN(good); i++) CHECKF(U(IsValidLocaleName)(good[i]), "%ls", good[i]);
}
TEST(locvalid_isvalidlocalename_rejects) {
    REQUIRE(U(IsValidLocaleName));
    CHECK(!U(IsValidLocaleName)(NULL));
    CHECK(!U(IsValidLocaleName)(LOCALE_NAME_SYSTEM_DEFAULT));
    CHECK(!U(IsValidLocaleName)(L"!x-sys-default-locale"));
    static const WCHAR *bad[] = { L"!!!", L"not-a-locale", L"12345", L"-", L"en US", L"e" };
    for (int i = 0; i < ARRAYLEN(bad); i++) {
        if (U(IsValidLocaleName)(bad[i])) t_note("IsValidLocaleName(%ls) is TRUE", bad[i]);
    }
}
TEST(locvalid_isvalidlocalename_invariant) {
    REQUIRE(U(IsValidLocaleName));
    if (!U(IsValidLocaleName)(L"")) t_note("IsValidLocaleName(\"\") is FALSE");
}

/* ---- ConvertDefaultLocale ---- */
TEST(locconv_specific_unchanged) {
    REQUIRE(U(ConvertDefaultLocale));
    for (int i = 0; i < ARRAYLEN(FIXED_LCIDS); i++) CHECKF(U(ConvertDefaultLocale)(FIXED_LCIDS[i]) == FIXED_LCIDS[i], "0x%lx", FIXED_LCIDS[i]);
}
TEST(locconv_user_default) {
    REQUIRE(U(ConvertDefaultLocale)); REQUIRE(U(GetUserDefaultLCID));
    LCID c = U(ConvertDefaultLocale)(LOCALE_USER_DEFAULT);
    CHECK(c != 0 && c != LOCALE_USER_DEFAULT);
    if (c != U(GetUserDefaultLCID)()) t_note("ConvertDefaultLocale(USER)=0x%lx user=0x%lx", c, U(GetUserDefaultLCID)());
}
TEST(locconv_system_default) {
    REQUIRE(U(ConvertDefaultLocale)); REQUIRE(U(GetSystemDefaultLCID));
    CHECK(U(ConvertDefaultLocale)(LOCALE_SYSTEM_DEFAULT) == U(GetSystemDefaultLCID)());
}
TEST(locconv_neutral_to_default_specific) {
    REQUIRE(U(ConvertDefaultLocale));
    CHECK(U(ConvertDefaultLocale)(0x0009) == 0x0409);
    CHECK(U(ConvertDefaultLocale)(0x0007) == 0x0407);
    CHECK(U(ConvertDefaultLocale)(0x000c) == 0x040c);
    CHECK(U(ConvertDefaultLocale)(0x0011) == 0x0411);
    CHECK(U(ConvertDefaultLocale)(0x0012) == 0x0412);
    CHECK(U(ConvertDefaultLocale)(0x0019) == 0x0419);
}
TEST(locconv_invalid_returned_unchanged) {
    REQUIRE(U(ConvertDefaultLocale));
    static const LCID v[] = { 0xDEAD, 0xFFFFFFFF, 0x00F00000 };
    for (int i = 0; i < ARRAYLEN(v); i++) CHECKF(U(ConvertDefaultLocale)(v[i]) == v[i], "0x%lx -> 0x%lx", v[i], U(ConvertDefaultLocale)(v[i]));
}
TEST(locconv_idempotent) {
    REQUIRE(U(ConvertDefaultLocale));
    LCID in[] = { 0x400, 0x800, 0x9, 0x7f, 0x409, 0x0 };
    for (int i = 0; i < ARRAYLEN(in); i++) { LCID a = U(ConvertDefaultLocale)(in[i]); CHECK(U(ConvertDefaultLocale)(a) == a); }
}

/* ---- ResolveLocaleName ---- */
TEST(locresolve_neutral_to_specific) {
    REQUIRE(U(ResolveLocaleName));
    static const struct { const WCHAR *in, *out; } t[] = {
        { L"en", L"en-US" }, { L"de", L"de-DE" }, { L"fr", L"fr-FR" }, { L"ja", L"ja-JP" }, { L"ko", L"ko-KR" },
        { L"ru", L"ru-RU" }, { L"tr", L"tr-TR" }, { L"he", L"he-IL" }, { L"th", L"th-TH" }, { L"hi", L"hi-IN" },
        { L"pt", L"pt-BR" }, { L"it", L"it-IT" }, { L"es", L"es-ES" },
    };
    for (int i = 0; i < ARRAYLEN(t); i++) {
        WCHAR b[100]; SET_ERR(0);
        int r = U(ResolveLocaleName)(t[i].in, b, 100);
        CHECKF(r > 0, "%ls err %lu", t[i].in, GetLastError());
        if (r > 0) { CHECKF(!lstrcmpW(b, t[i].out), "%ls -> %ls (want %ls)", t[i].in, b, t[i].out); CHECK(r == lstrlenW(b) + 1); }
    }
}
TEST(locresolve_specific_unchanged) {
    REQUIRE(U(ResolveLocaleName));
    for (int i = 0; i < ARRAYLEN(FIXED_LOCALES); i++) {
        WCHAR b[100];
        int r = U(ResolveLocaleName)(FIXED_LOCALES[i], b, 100);
        CHECK(r > 0);
        CHECKF(!lstrcmpW(b, FIXED_LOCALES[i]), "%ls -> %ls", FIXED_LOCALES[i], b);
    }
}
TEST(locresolve_extra_subtags_dropped) {
    REQUIRE(U(ResolveLocaleName));
    WCHAR b[100];
    int r = U(ResolveLocaleName)(L"en-US-x-private", b, 100);
    if (r > 0) CHECKF(!lstrcmpW(b, L"en-US"), "got %ls", b); else t_note("en-US-x-private failed %lu", GetLastError());
    r = U(ResolveLocaleName)(L"fr-FR-u-co-phonebk", b, 100);
    if (r > 0) CHECKF(!lstrcmpW(b, L"fr-FR"), "got %ls", b); else t_note("fr-FR-u-co failed %lu", GetLastError());
}
TEST(locresolve_size_query) {
    REQUIRE(U(ResolveLocaleName));
    SET_ERR(0);
    int r = U(ResolveLocaleName)(L"en", NULL, 0);
    CHECKF(r == 6, "r=%d err=%lu", r, GetLastError());
}
TEST(locresolve_truncation) {
    REQUIRE(U(ResolveLocaleName));
    WCHAR b[100];
    SET_ERR(0);
    int r = U(ResolveLocaleName)(L"en-US", b, 5);
    CHECK(r == 0); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    SET_ERR(0);
    r = U(ResolveLocaleName)(L"en-US", b, 6);
    CHECK(r == 6); CHECK(!lstrcmpW(b, L"en-US"));
}
TEST(locresolve_invalid) {
    REQUIRE(U(ResolveLocaleName));
    WCHAR b[100];
    SET_ERR(0);
    int r = U(ResolveLocaleName)(NULL, b, 100);   /* NULL resolves the user default locale */
    CHECK(r > 1);
    SET_ERR(0);
    r = U(ResolveLocaleName)(L"!!!", b, 100);
    t_note("ResolveLocaleName(\"!!!\") r=%d err=%lu b=%ls", r, GetLastError(), r > 0 ? b : L"");
    SET_ERR(0);
}

/* ---- thread locale ---- */
TEST(locthread_default_is_valid) {
    REQUIRE(U(GetThreadLocale));
    LCID l = U(GetThreadLocale)();
    CHECK(l != 0);
    WCHAR b[100]; REQUIRE(U(LCIDToLocaleName));
    CHECK(U(LCIDToLocaleName)(l, b, 100, 0) > 0);
}
TEST(locthread_default_matches_user) {
    REQUIRE(U(GetThreadLocale)); REQUIRE(U(GetUserDefaultLCID));
    LCID t = U(GetThreadLocale)(), u = U(GetUserDefaultLCID)();
    if (t != u) t_note("thread locale 0x%lx user 0x%lx", t, u);
}
CHILD(setthreadlocale) {
    LCID orig = U(GetThreadLocale)();
    int bad = 0;
    if (!U(SetThreadLocale)(0x407)) bad |= 1;
    if (U(GetThreadLocale)() != 0x407) bad |= 2;
    if (!U(SetThreadLocale)(0x411)) bad |= 4;
    if (U(GetThreadLocale)() != 0x411) bad |= 8;
    SetLastError(0);
    if (U(SetThreadLocale)(0xDEAD)) bad |= 16;
    else if (GetLastError() != ERROR_INVALID_PARAMETER) bad |= 32;
    if (U(GetThreadLocale)() != 0x411) bad |= 64;       /* failed call must not change anything */
    if (!U(SetThreadLocale)(0x409)) bad |= 128;
    if (U(GetThreadLocale)() != 0x409) bad |= 256;
    /* other threads are unaffected: verified by the other child */
    U(SetThreadLocale)(orig);
    return bad;
}
TEST(locthread_setthreadlocale_child) {
    REQUIRE(U(SetThreadLocale));
    EXPECT_EQ(t_run_child("setthreadlocale", "", 20000), 0);
}
static DWORD WINAPI tl_thread(LPVOID p) { *(LCID *)p = U(GetThreadLocale)(); return 0; }
CHILD(threadlocale_per_thread) {
    LCID mine = 0, other = 0;
    U(SetThreadLocale)(0x407);
    HANDLE h = CreateThread(NULL, 0, tl_thread, &other, 0, NULL);
    WaitForSingleObject(h, 5000); CloseHandle(h);
    mine = U(GetThreadLocale)();
    if (mine != 0x407) return 1;
    /* new thread starts with a default that is not the one set on the creating thread (unless defaults agree) */
    return other != 0 ? 0 : 2;
}
TEST(locthread_setthreadlocale_is_per_thread) {
    REQUIRE(U(SetThreadLocale));
    EXPECT_EQ(t_run_child("threadlocale_per_thread", "", 20000), 0);
}
CHILD(threadlocale_pseudo) {
    int bad = 0;
    LCID user = U(GetUserDefaultLCID)(), sys = U(GetSystemDefaultLCID)();
    if (!U(SetThreadLocale)(0x800)) bad |= 1;
    LCID l = U(GetThreadLocale)();
    if (l != sys) bad |= 2;
    if (!U(SetThreadLocale)(0x400)) bad |= 4;
    l = U(GetThreadLocale)();
    if (l == 0x400 || l == 0) bad |= 8;
    (void)user;
    if (!U(SetThreadLocale)(0x7f)) bad |= 16;
    if (U(GetThreadLocale)() != 0x7f) bad |= 32;
    return bad;
}
TEST(locthread_setthreadlocale_pseudo_lcids) {
    REQUIRE(U(SetThreadLocale)); REQUIRE(U(GetSystemDefaultLCID));
    DWORD r = t_run_child("threadlocale_pseudo", "", 20000);
    CHECKF(r == 0, "bits 0x%lx", r);
}
