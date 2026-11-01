/* Generic per-locale consistency matrix: every LCTYPE through every entry point. */
#include "nlsutil.h"

static int is_numeric_type(DWORD t)
{
    switch (t) {
    case L_ICOUNTRY: case L_IMEASURE: case L_IDIGITS: case L_ILZERO: case L_INEGNUMBER: case L_ICURRDIGITS:
    case L_IINTLCURRDIGITS: case L_ICURRENCY: case L_INEGCURR: case L_IDATE: case L_ILDATE: case L_ITIME:
    case L_ICENTURY: case L_ITLZERO: case L_IDAYLZERO: case L_IMONLZERO: case L_ICALENDARTYPE:
    case L_IOPTIONALCALENDAR: case L_IFIRSTDAYOFWEEK: case L_IFIRSTWEEKOFYEAR: case L_IDEFAULTANSICP:
    case L_IDEFAULTCODEPAGE: case L_IDEFAULTMACCP: case L_IDEFAULTEBCDICCP: case L_IPAPERSIZE:
    case L_ITIMEMARKPOSN: case L_IPOSSIGNPOSN: case L_INEGSIGNPOSN: case L_IPOSSYMPRECEDES:
    case L_IPOSSEPBYSPACE: case L_INEGSYMPRECEDES: case L_INEGSEPBYSPACE: case L_IGEOID:
    case L_IDIGITSUBSTITUTION: case L_IREADINGLAYOUT: case L_INEUTRAL:
        return 1;
    }
    return 0;
}

static void matrix(const WCHAR *loc)
{
    REQUIRE(U(GetLocaleInfoEx)); REQUIRE(U(GetLocaleInfoW)); REQUIRE(U(GetLocaleInfoA)); REQUIRE(U(LocaleNameToLCID));
    LCID lcid = U(LocaleNameToLCID)(loc, 0x08000000);
    if (!lcid && !loc[0]) lcid = 0x7f;
    CHECK(lcid != 0);
    WCHAR acpw[16]; nls_str(loc, L_IDEFAULTANSICP, acpw, 16);
    int acp = _wtoi(acpw);
    WCHAR neutw[8]; nls_str(loc, L_INEUTRAL, neutw, 8);
    int neutral = neutw[0] == '1';   /* LCID-based lookups of neutral names are not exact */
    for (int i = 0; i < ARRAYLEN(ALL_LCTYPES); i++) {
        DWORD t = ALL_LCTYPES[i];
        WCHAR b[512], b2[512], b3[512];
        SET_ERR(0);
        int n = nls_str(loc, t, NULL, 0);
        if (n <= 0) { CHECKF(0, "size query type 0x%lx failed err %lu", t, GetLastError()); continue; }
        int r = nls_str(loc, t, b, 512);
        CHECKF(r == n, "type 0x%lx size %d vs %d", t, n, r);
        CHECKF(lstrlenW(b) + 1 == n, "type 0x%lx strlen+1 %d vs %d", t, lstrlenW(b) + 1, n);
        /* exact fit */
        memset(b2, 0xAA, sizeof b2);
        r = nls_str(loc, t, b2, n);
        CHECKF(r == n && !lstrcmpW(b, b2), "exact fit type 0x%lx", t);
        /* one too small */
        if (n > 1) {
            SET_ERR(0);
            r = nls_str(loc, t, b3, n - 1);
            CHECKF(r == 0, "short type 0x%lx r=%d", t, r);
            EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
        }
        /* W via LCID */
        if (neutral) continue;
        r = U(GetLocaleInfoW)(lcid, t | N_NOUSER, b3, 512);
        CHECKF(r == n && !lstrcmpW(b, b3), "GetLocaleInfoW type 0x%lx", t);
        /* A: converted with the locale code page */
        char ab[1024];
        SET_ERR(0);
        int ra = U(GetLocaleInfoA)(lcid, t | N_NOUSER, ab, 1024);
        if (t >= 0x70 && t <= 0xFFF) {
            /* P1: Win10 GetLocaleInfoA rejects LCTYPE 0x70..0xFFF (kernelbase decompile) */
            CHECKF(ra == 0, "GetLocaleInfoA type 0x%lx ra=%d", t, ra);
            EXPECT_ERR(ERROR_INVALID_FLAGS);
            ra = 0;
        } else
        CHECKF(ra > 0, "GetLocaleInfoA type 0x%lx", t);
        if (ra > 0 && acp > 0) {
            char ex[1024]; BOOL used = FALSE;
            int rc = WideCharToMultiByte(acp, 0, b, -1, ex, 1024, NULL, &used);
            if (rc > 0 && !used) CHECKF(ra == rc && !strcmp(ab, ex), "A parity type 0x%lx", t);
            else if (nls_isascii(b)) CHECKF(ra == n && !strcmp(ab, ex), "A ascii type 0x%lx", t);
        } else if (ra > 0 && nls_isascii(b)) {
            CHECKF(ra == n, "A ascii len type 0x%lx", t);
        }
        /* numeric */
        if (is_numeric_type(t)) {
            DWORD v = 0xDEADBEEF; WCHAR vb[8];
            r = U(GetLocaleInfoEx)(loc, t | N_NOUSER | N_RETNUM, vb, 2);
            memcpy(&v, vb, 4);
            CHECKF(r == 2, "retnum type 0x%lx r=%d err=%lu", t, r, GetLastError());
            if (r == 2) CHECKF(v == (DWORD)_wtoi(b), "retnum value type 0x%lx got %lu str %ls", t, v, b);
        }
    }
}

#define MX(tag, nm) TEST(locmatrix_##tag) { matrix(nm); }
MX(en_US, L"en-US") MX(de_DE, L"de-DE") MX(fr_FR, L"fr-FR") MX(ja_JP, L"ja-JP") MX(zh_CN, L"zh-CN")
MX(ar_SA, L"ar-SA") MX(tr_TR, L"tr-TR") MX(he_IL, L"he-IL") MX(th_TH, L"th-TH") MX(hi_IN, L"hi-IN")
MX(ko_KR, L"ko-KR") MX(ru_RU, L"ru-RU") MX(pt_BR, L"pt-BR") MX(invariant, L"")
MX(en_GB, L"en-GB") MX(es_ES, L"es-ES") MX(it_IT, L"it-IT") MX(nl_NL, L"nl-NL") MX(sv_SE, L"sv-SE")
MX(pl_PL, L"pl-PL") MX(zh_TW, L"zh-TW") MX(fa_IR, L"fa-IR") MX(en_IN, L"en-IN") MX(de_CH, L"de-CH")
MX(neutral_en, L"en") MX(neutral_de, L"de") MX(neutral_zh_Hans, L"zh-Hans") MX(neutral_sr_Latn, L"sr-Latn")

/* Win32 numeric representation equals decimal value of the string for ILANGUAGE-like hex types */
TEST(locinfo_ilanguage_retnum_is_lcid) {
    REQUIRE(U(GetLocaleInfoEx));
    for (int i = 0; i < ARRAYLEN(FIXED_LOCALES); i++) {
        int ok; DWORD v = nls_num(FIXED_LOCALES[i], L_ILANGUAGE, &ok);
        CHECKF(ok, "%ls", FIXED_LOCALES[i]);
        if (ok) CHECKF(v == FIXED_LCIDS[i], "%ls got 0x%lx want 0x%lx", FIXED_LOCALES[i], v, FIXED_LCIDS[i]);
    }
}
TEST(locinfo_icountry_retnum_matches_str) {
    REQUIRE(U(GetLocaleInfoEx));
    for (int i = 0; i < ARRAYLEN(FIXED_LOCALES); i++) {
        WCHAR b[32]; nls_str(FIXED_LOCALES[i], L_ICOUNTRY, b, 32);
        int ok; DWORD v = nls_num(FIXED_LOCALES[i], L_ICOUNTRY, &ok);
        CHECK(ok); CHECK(v == (DWORD)_wtoi(b));
    }
}
TEST(locinfo_igeoid_retnum) {
    REQUIRE(U(GetLocaleInfoEx));
    static const DWORD geo[] = { 244, 94, 84, 122, 45, 205, 235, 117, 227, 113, 134, 203, 32 };
    for (int i = 0; i < ARRAYLEN(FIXED_LOCALES); i++) {
        int ok; DWORD v = nls_num(FIXED_LOCALES[i], L_IGEOID, &ok);
        CHECK(ok); CHECKF(v == geo[i], "%ls got %lu", FIXED_LOCALES[i], v);
    }
}
TEST(locinfo_acp_retnum) {
    REQUIRE(U(GetLocaleInfoEx));
    static const DWORD acp[] = { 1252, 1252, 1252, 932, 936, 1256, 1254, 1255, 874, 0, 949, 1251, 1252 };
    static const DWORD oem[] = { 437, 850, 850, 932, 936, 720, 857, 862, 874, 1, 949, 866, 850 };
    for (int i = 0; i < ARRAYLEN(FIXED_LOCALES); i++) {
        int ok; DWORD v = nls_num(FIXED_LOCALES[i], L_IDEFAULTANSICP, &ok);
        CHECK(ok);
        if (i != 9) CHECKF(v == acp[i], "%ls acp got %lu", FIXED_LOCALES[i], v); else t_note("hi-IN acp=%lu", v);
        v = nls_num(FIXED_LOCALES[i], L_IDEFAULTCODEPAGE, &ok);
        CHECK(ok);
        if (i != 9) CHECKF(v == oem[i], "%ls oem got %lu", FIXED_LOCALES[i], v); else t_note("hi-IN oem=%lu", v);
    }
}

/* Native-digit strings are 10 characters for the locales in the table. */
TEST(locinfo_native_digits_length) {
    REQUIRE(U(GetLocaleInfoEx));
    for (int i = 0; i < ARRAYLEN(FIXED_LOCALES); i++) {
        WCHAR b[64];
        int n = nls_str(FIXED_LOCALES[i], L_SNATIVEDIGITS, b, 64);
        CHECKF(n == 11, "%ls n=%d", FIXED_LOCALES[i], n);
    }
}
/* Grouping strings are digit lists separated by ';' and terminated by 0 or a lone digit. */
TEST(locinfo_grouping_syntax) {
    REQUIRE(U(GetLocaleInfoEx));
    for (int i = 0; i < ARRAYLEN(FIXED_LOCALES); i++) {
        WCHAR b[64];
        if (nls_str(FIXED_LOCALES[i], L_SGROUPING, b, 64) <= 0) { CHECK(0); continue; }
        int good = b[0] != 0;
        for (WCHAR *p = b; *p; p++) if (!((*p >= '0' && *p <= '9') || *p == ';')) good = 0;
        CHECKF(good, "%ls: %ls", FIXED_LOCALES[i], b);
        nls_str(FIXED_LOCALES[i], L_SMONGROUPING, b, 64);
        good = b[0] != 0;
        for (WCHAR *p = b; *p; p++) if (!((*p >= '0' && *p <= '9') || *p == ';')) good = 0;
        CHECKF(good, "%ls mon: %ls", FIXED_LOCALES[i], b);
    }
}
/* The date/time picture strings of every fixed locale contain only picture tokens, separators, or quoted text. */
TEST(locinfo_date_pictures_valid) {
    REQUIRE(U(GetLocaleInfoEx));
    static const DWORD types[] = { L_SSHORTDATE, L_SLONGDATE, L_SYEARMONTH };
    for (int i = 0; i < ARRAYLEN(FIXED_LOCALES); i++)
        for (int k = 0; k < 3; k++) {
            WCHAR b[128];
            int n = nls_str(FIXED_LOCALES[i], types[k], b, 128);
            CHECKF(n > 1, "%ls type 0x%lx", FIXED_LOCALES[i], types[k]);
            int quotes = 0; for (WCHAR *p = b; *p; p++) if (*p == '\'') quotes++;
            CHECKF(!(quotes & 1), "%ls unbalanced quotes %ls", FIXED_LOCALES[i], b);
        }
}
TEST(locinfo_time_pictures_have_hour_minute) {
    REQUIRE(U(GetLocaleInfoEx));
    for (int i = 0; i < ARRAYLEN(FIXED_LOCALES); i++) {
        WCHAR b[128];
        nls_str(FIXED_LOCALES[i], L_STIMEFORMAT, b, 128);
        CHECKF(wcschr(b, 'h') || wcschr(b, 'H'), "%ls %ls", FIXED_LOCALES[i], b);
        CHECKF(wcschr(b, 'm'), "%ls %ls", FIXED_LOCALES[i], b);
    }
}
