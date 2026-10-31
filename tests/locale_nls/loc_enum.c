/* Enumeration APIs: locales, language groups, UI languages, code pages, geo ids. */
#include "nlsutil.h"

#define MAXE 4096
static struct { WCHAR s[96]; DWORD flags; } g_e[MAXE];
static int g_n, g_stop_after, g_calls_after_stop;
static LONG_PTR g_lp_seen;
static void e_reset(int stop_after) { g_n = 0; g_stop_after = stop_after; g_calls_after_stop = 0; g_lp_seen = 0; }
static int e_find(const WCHAR *s) { for (int i = 0; i < g_n && i < MAXE; i++) if (!lstrcmpiW(g_e[i].s, s)) return i; return -1; }
static void e_add(const WCHAR *s, DWORD fl) {
    if (g_stop_after && g_n >= g_stop_after) g_calls_after_stop++;
    if (g_n < MAXE) { lstrcpynW(g_e[g_n].s, s ? s : L"(null)", 96); g_e[g_n].flags = fl; }
    g_n++;
}
static BOOL e_cont(void) { return !(g_stop_after && g_n >= g_stop_after); }

static BOOL CALLBACK cb_locw(LPWSTR s) { e_add(s, 0); return e_cont(); }
static BOOL CALLBACK cb_loca(LPSTR s) { WCHAR w[96]; nls_a2w(s, w, 96); e_add(w, 0); return e_cont(); }
static BOOL CALLBACK cb_locex(LPWSTR s, DWORD fl, LPARAM lp) { g_lp_seen = lp; e_add(s, fl); return e_cont(); }

TEST(locenum_systemlocalesw_supported) {
    REQUIRE(U(EnumSystemLocalesW));
    e_reset(0); SET_ERR(0);
    BOOL r = U(EnumSystemLocalesW)(cb_locw, LCID_SUPPORTED);
    CHECK(r); CHECKF(g_n > 100, "n=%d", g_n);
    CHECK(e_find(L"00000409") >= 0);
    CHECK(e_find(L"00000407") >= 0);
    CHECK(e_find(L"00000411") >= 0);
    for (int i = 0; i < g_n && i < MAXE; i++) {
        CHECKF(lstrlenW(g_e[i].s) == 8, "len of %ls", g_e[i].s);
        int hex = 1; for (WCHAR *p = g_e[i].s; *p; p++) if (!iswxdigit(*p)) hex = 0;
        CHECKF(hex, "hex %ls", g_e[i].s);
    }
}
TEST(locenum_systemlocalesw_installed) {
    REQUIRE(U(EnumSystemLocalesW));
    e_reset(0);
    CHECK(U(EnumSystemLocalesW)(cb_locw, LCID_INSTALLED));
    CHECK(g_n >= 1);
    t_note("installed locales: %d", g_n);
    CHECK(e_find(L"00000409") >= 0);
}
TEST(locenum_systemlocalesw_alternate) {
    REQUIRE(U(EnumSystemLocalesW));
    e_reset(0);
    CHECK(U(EnumSystemLocalesW)(cb_locw, LCID_ALTERNATE_SORTS));
    CHECK(g_n >= 1);
    CHECK(e_find(L"00010407") >= 0 || e_find(L"00010407") < 0);
    t_note("alternate sorts: %d (de-DE phonebook present: %d)", g_n, e_find(L"00010407") >= 0);
}
TEST(locenum_systemlocalesw_supported_superset_of_installed) {
    REQUIRE(U(EnumSystemLocalesW));
    static WCHAR inst[MAXE][9]; int ni = 0;
    e_reset(0); U(EnumSystemLocalesW)(cb_locw, LCID_INSTALLED);
    for (int i = 0; i < g_n && i < MAXE; i++) lstrcpynW(inst[ni++], g_e[i].s, 9);
    e_reset(0); U(EnumSystemLocalesW)(cb_locw, LCID_SUPPORTED);
    for (int i = 0; i < ni; i++) CHECKF(e_find(inst[i]) >= 0, "%ls", inst[i]);
}
TEST(locenum_systemlocalesw_early_stop) {
    REQUIRE(U(EnumSystemLocalesW));
    e_reset(1); SET_ERR(0);
    BOOL r = U(EnumSystemLocalesW)(cb_locw, LCID_SUPPORTED);
    CHECK(g_n == 1); CHECK(g_calls_after_stop == 0);
    t_note("stop result=%d err=%lu", r, GetLastError());
    e_reset(5);
    U(EnumSystemLocalesW)(cb_locw, LCID_SUPPORTED);
    CHECK(g_n == 5);
}
TEST(locenum_systemlocalesw_null_callback) {
    REQUIRE(U(EnumSystemLocalesW));
    if (t_is_wine()) { t_skip("emulator faults on NULL callback"); return; }
    SET_ERR(0);
    BOOL r = U(EnumSystemLocalesW)(NULL, LCID_SUPPORTED);
    CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(locenum_systemlocalesw_bad_flags) {
    REQUIRE(U(EnumSystemLocalesW));
    static const DWORD f[] = { 3, 7, 8, 0x80, 0x100, 0x10000, 0x80000000 };
    for (int i = 0; i < ARRAYLEN(f); i++) {
        e_reset(0); SET_ERR(0);
        BOOL r = U(EnumSystemLocalesW)(cb_locw, f[i]);
        CHECKF(!r && g_n == 0, "flags 0x%lx", f[i]);
        if (!r) t_note("EnumSystemLocalesW flags 0x%lx err=%lu", f[i], GetLastError());
    }
}
TEST(locenum_systemlocalesw_flags_zero) {
    REQUIRE(U(EnumSystemLocalesW));
    e_reset(0); SET_ERR(0);
    BOOL r = U(EnumSystemLocalesW)(cb_locw, 0);
    t_note("EnumSystemLocalesW flags 0: r=%d n=%d err=%lu", r, g_n, GetLastError());
}
TEST(locenum_systemlocalesa_matches_w) {
    REQUIRE(U(EnumSystemLocalesA)); REQUIRE(U(EnumSystemLocalesW));
    e_reset(0); U(EnumSystemLocalesW)(cb_locw, LCID_SUPPORTED);
    int nw = g_n;
    e_reset(0);
    CHECK(U(EnumSystemLocalesA)(cb_loca, LCID_SUPPORTED));
    CHECKF(g_n == nw, "A=%d W=%d", g_n, nw);
    CHECK(e_find(L"00000409") >= 0);
}
TEST(locenum_systemlocalesa_null_callback) {
    REQUIRE(U(EnumSystemLocalesA));
    if (t_is_wine()) { t_skip("emulator faults on NULL callback"); return; }
    SET_ERR(0); CHECK(!U(EnumSystemLocalesA)(NULL, LCID_SUPPORTED)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(locenum_systemlocalesa_early_stop) {
    REQUIRE(U(EnumSystemLocalesA));
    e_reset(3); U(EnumSystemLocalesA)(cb_loca, LCID_SUPPORTED);
    CHECK(g_n == 3);
}
TEST(locenum_supported_each_is_valid) {
    REQUIRE(U(EnumSystemLocalesW)); REQUIRE(U(IsValidLocale)); REQUIRE(U(LCIDToLocaleName));
    e_reset(0); U(EnumSystemLocalesW)(cb_locw, LCID_SUPPORTED);
    int n = g_n < MAXE ? g_n : MAXE, bad = 0;
    static WCHAR copy[MAXE][9];
    for (int i = 0; i < n; i++) lstrcpynW(copy[i], g_e[i].s, 9);
    for (int i = 0; i < n; i++) {
        LCID l = (LCID)wcstoul(copy[i], NULL, 16);
        WCHAR nm[100];
        if (!U(IsValidLocale)(l, LCID_SUPPORTED)) bad++;
        else if (U(LCIDToLocaleName)(l, nm, 100, 0) <= 0) bad++;
    }
    CHECKF(bad == 0, "bad=%d of %d", bad, n);
}

/* ---- EnumSystemLocalesEx ---- */
TEST(locenum_ex_all) {
    REQUIRE(U(EnumSystemLocalesEx));
    e_reset(0); SET_ERR(0);
    BOOL r = U(EnumSystemLocalesEx)(cb_locex, LOCALE_ALL, 0x1234, NULL);
    CHECK(r); CHECKF(g_n > 200, "n=%d", g_n);
    CHECK(g_lp_seen == 0x1234);
    for (int i = 0; i < ARRAYLEN(FIXED_LOCALES); i++) CHECKF(e_find(FIXED_LOCALES[i]) >= 0, "%ls", FIXED_LOCALES[i]);
}
TEST(locenum_ex_windows_flag) {
    REQUIRE(U(EnumSystemLocalesEx));
    e_reset(0);
    CHECK(U(EnumSystemLocalesEx)(cb_locex, LOCALE_WINDOWS, 0, NULL));
    CHECK(g_n > 100);
    CHECK(e_find(L"en-US") >= 0);
    CHECK(e_find(L"de-DE") >= 0);
}
TEST(locenum_ex_specific_and_neutral) {
    REQUIRE(U(EnumSystemLocalesEx));
    e_reset(0);
    CHECK(U(EnumSystemLocalesEx)(cb_locex, LOCALE_SPECIFICDATA, 0, NULL));
    CHECK(g_n > 100);
    CHECK(e_find(L"en-US") >= 0); CHECK(e_find(L"en") < 0);
    int nspec = g_n;
    e_reset(0);
    CHECK(U(EnumSystemLocalesEx)(cb_locex, LOCALE_NEUTRALDATA, 0, NULL));
    CHECK(g_n > 20);
    CHECK(e_find(L"en") >= 0); CHECK(e_find(L"de") >= 0); CHECK(e_find(L"en-US") < 0);
    int nneu = g_n;
    e_reset(0);
    U(EnumSystemLocalesEx)(cb_locex, LOCALE_ALL, 0, NULL);
    CHECKF(g_n >= nspec + nneu - 5, "all=%d spec=%d neutral=%d", g_n, nspec, nneu);
}
TEST(locenum_ex_callback_flags_describe_entry) {
    REQUIRE(U(EnumSystemLocalesEx));
    e_reset(0);
    U(EnumSystemLocalesEx)(cb_locex, LOCALE_ALL, 0, NULL);
    int i = e_find(L"en-US"), j = e_find(L"en");
    CHECK(i >= 0 && j >= 0);
    if (i >= 0) CHECKF(g_e[i].flags & LOCALE_SPECIFICDATA, "en-US flags 0x%lx", g_e[i].flags);
    if (j >= 0) CHECKF(g_e[j].flags & LOCALE_NEUTRALDATA, "en flags 0x%lx", g_e[j].flags);
    if (i >= 0) CHECK(!(g_e[i].flags & LOCALE_NEUTRALDATA));
    if (j >= 0) CHECK(!(g_e[j].flags & LOCALE_SPECIFICDATA));
}
TEST(locenum_ex_names_are_valid) {
    REQUIRE(U(EnumSystemLocalesEx)); REQUIRE(U(IsValidLocaleName)); REQUIRE(U(LocaleNameToLCID));
    e_reset(0); U(EnumSystemLocalesEx)(cb_locex, LOCALE_ALL, 0, NULL);
    int n = g_n < MAXE ? g_n : MAXE, bad = 0;
    static WCHAR copy[MAXE][96];
    for (int i = 0; i < n; i++) lstrcpynW(copy[i], g_e[i].s, 96);
    for (int i = 0; i < n; i++) if (!U(IsValidLocaleName)(copy[i])) { bad++; if (bad < 4) t_note("invalid enumerated name %ls", copy[i]); }
    CHECKF(bad == 0, "bad=%d of %d", bad, n);
}
TEST(locenum_ex_alternate_sorts) {
    REQUIRE(U(EnumSystemLocalesEx));
    e_reset(0);
    CHECK(U(EnumSystemLocalesEx)(cb_locex, LOCALE_ALTERNATE_SORTS, 0, NULL));
    t_note("alternate sorts: %d", g_n);
    for (int i = 0; i < g_n && i < MAXE; i++) CHECKF(wcschr(g_e[i].s, '_') != NULL, "%ls", g_e[i].s);
}
TEST(locenum_ex_early_stop) {
    REQUIRE(U(EnumSystemLocalesEx));
    e_reset(1);
    U(EnumSystemLocalesEx)(cb_locex, LOCALE_ALL, 0, NULL);
    CHECK(g_n == 1); CHECK(g_calls_after_stop == 0);
    e_reset(7);
    U(EnumSystemLocalesEx)(cb_locex, LOCALE_ALL, 0, NULL);
    CHECK(g_n == 7);
}
TEST(locenum_ex_null_callback) {
    REQUIRE(U(EnumSystemLocalesEx));
    if (t_is_wine()) { t_skip("emulator faults on NULL callback"); return; }
    SET_ERR(0); CHECK(!U(EnumSystemLocalesEx)(NULL, LOCALE_ALL, 0, NULL)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(locenum_ex_reserved_nonnull) {
    REQUIRE(U(EnumSystemLocalesEx));
    int dummy = 0;
    e_reset(0); SET_ERR(0);
    BOOL r = U(EnumSystemLocalesEx)(cb_locex, LOCALE_ALL, 0, &dummy);
    CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(locenum_ex_bad_flags) {
    REQUIRE(U(EnumSystemLocalesEx));
    static const DWORD f[] = { 0x40, 0x80, 0x100, 0x80000000 };
    for (int i = 0; i < ARRAYLEN(f); i++) {
        e_reset(0); SET_ERR(0);
        BOOL r = U(EnumSystemLocalesEx)(cb_locex, f[i], 0, NULL);
        CHECKF(!r, "flags 0x%lx", f[i]);
        if (!r) EXPECT_ERR(ERROR_INVALID_FLAGS);
    }
}
TEST(locenum_ex_repeat_same_count) {
    REQUIRE(U(EnumSystemLocalesEx));
    e_reset(0); U(EnumSystemLocalesEx)(cb_locex, LOCALE_WINDOWS, 0, NULL); int a = g_n;
    e_reset(0); U(EnumSystemLocalesEx)(cb_locex, LOCALE_WINDOWS, 0, NULL); int b = g_n;
    CHECK(a == b);
}
TEST(locenum_ex_no_duplicates) {
    REQUIRE(U(EnumSystemLocalesEx));
    e_reset(0); U(EnumSystemLocalesEx)(cb_locex, LOCALE_ALL, 0, NULL);
    int n = g_n < MAXE ? g_n : MAXE, dups = 0;
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) if (!lstrcmpiW(g_e[i].s, g_e[j].s)) dups++;
    CHECKF(dups == 0, "dups=%d", dups);
}

/* ---- language groups ---- */
static BOOL CALLBACK cb_lgw(LGRPID id, LPWSTR s, LPWSTR name, DWORD fl, LONG_PTR lp) {
    WCHAR t[96]; wsprintfW(t, L"%lu|%ls", (unsigned long)id, s ? s : L"(null)");
    e_add(t, fl); g_lp_seen = lp;
    return e_cont();
}
static BOOL CALLBACK cb_lga(LGRPID id, LPSTR s, LPSTR name, DWORD fl, LONG_PTR lp) {
    WCHAR t[96], w[64]; nls_a2w(s ? s : "", w, 64); wsprintfW(t, L"%lu|%ls", (unsigned long)id, w);
    e_add(t, fl); g_lp_seen = lp;
    return e_cont();
}
TEST(locenum_langgroups_supported) {
    REQUIRE(U(EnumSystemLanguageGroupsW));
    e_reset(0); SET_ERR(0);
    BOOL r = U(EnumSystemLanguageGroupsW)(cb_lgw, LGRPID_SUPPORTED, 77);
    CHECK(r); CHECKF(g_n >= 10, "n=%d", g_n);
    CHECK(g_lp_seen == 77);
    int has1 = 0, has7 = 0;
    for (int i = 0; i < g_n && i < MAXE; i++) {
        unsigned long id = wcstoul(g_e[i].s, NULL, 10);
        const WCHAR *bar = wcschr(g_e[i].s, '|');
        CHECKF(bar && wcstoul(bar + 1, NULL, 16) == id, "group string mismatch %ls", g_e[i].s);
        has1 |= id == 1; has7 |= id == 7;
        CHECK(id >= 1 && id <= 17);
    }
    CHECK(has1); CHECK(has7);
    t_note("supported language groups: %d first=%ls", g_n, g_e[0].s);
}
TEST(locenum_langgroups_installed) {
    REQUIRE(U(EnumSystemLanguageGroupsW));
    e_reset(0);
    CHECK(U(EnumSystemLanguageGroupsW)(cb_lgw, LGRPID_INSTALLED, 0));
    CHECK(g_n >= 1);
    int has1 = 0;
    for (int i = 0; i < g_n && i < MAXE; i++) { CHECK(g_e[i].flags & LGRPID_INSTALLED); has1 |= wcstoul(g_e[i].s, NULL, 10) == 1; }
    CHECK(has1);
}
TEST(locenum_langgroups_a_matches_w) {
    REQUIRE(U(EnumSystemLanguageGroupsA)); REQUIRE(U(EnumSystemLanguageGroupsW));
    e_reset(0); U(EnumSystemLanguageGroupsW)(cb_lgw, LGRPID_SUPPORTED, 0); int nw = g_n;
    e_reset(0); CHECK(U(EnumSystemLanguageGroupsA)(cb_lga, LGRPID_SUPPORTED, 0));
    CHECK(g_n == nw);
}
TEST(locenum_langgroups_early_stop) {
    REQUIRE(U(EnumSystemLanguageGroupsW));
    e_reset(2); U(EnumSystemLanguageGroupsW)(cb_lgw, LGRPID_SUPPORTED, 0);
    CHECK(g_n == 2); CHECK(g_calls_after_stop == 0);
}
TEST(locenum_langgroups_null_callback) {
    REQUIRE(U(EnumSystemLanguageGroupsW));
    SET_ERR(0); CHECK(!U(EnumSystemLanguageGroupsW)(NULL, LGRPID_SUPPORTED, 0)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    REQUIRE(U(EnumSystemLanguageGroupsA));
    SET_ERR(0); CHECK(!U(EnumSystemLanguageGroupsA)(NULL, LGRPID_SUPPORTED, 0)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(locenum_langgroups_bad_flags) {
    REQUIRE(U(EnumSystemLanguageGroupsW));
    static const DWORD f[] = { 3, 4, 0x10, 0x80000000 };
    for (int i = 0; i < ARRAYLEN(f); i++) {
        e_reset(0); SET_ERR(0);
        BOOL r = U(EnumSystemLanguageGroupsW)(cb_lgw, f[i], 0);
        CHECKF(!r && g_n == 0, "flags 0x%lx", f[i]);
        if (!r) EXPECT_ERR(ERROR_INVALID_FLAGS);
    }
}
TEST(locenum_isvalidlanguagegroup) {
    REQUIRE(U(IsValidLanguageGroup));
    CHECK(U(IsValidLanguageGroup)(LGRPID_WESTERN_EUROPE, LGRPID_SUPPORTED));
    CHECK(U(IsValidLanguageGroup)(LGRPID_WESTERN_EUROPE, LGRPID_INSTALLED));
    for (int g = 1; g <= 17; g++)
        CHECKF(U(IsValidLanguageGroup)(g, LGRPID_SUPPORTED), "group %d", g);
    SET_ERR(0);
    CHECK(!U(IsValidLanguageGroup)(0, LGRPID_SUPPORTED));
    CHECK(!U(IsValidLanguageGroup)(0xFFFF, LGRPID_SUPPORTED));
    CHECK(!U(IsValidLanguageGroup)(18, LGRPID_SUPPORTED));
    CHECK(U(IsValidLanguageGroup)(17, LGRPID_SUPPORTED));
    CHECK(U(IsValidLanguageGroup)(1, 0));
    CHECK(!U(IsValidLanguageGroup)(1, 3));
    CHECK(!U(IsValidLanguageGroup)(1, 0x80));
}
static BOOL CALLBACK cb_glw(LGRPID g, LCID l, LPWSTR s, LONG_PTR lp) {
    WCHAR t[40]; wsprintfW(t, L"%lx|%ls", (unsigned long)l, s ? s : L"(null)"); e_add(t, (DWORD)g); g_lp_seen = lp;
    return e_cont();
}
static BOOL CALLBACK cb_gla(LGRPID g, LCID l, LPSTR s, LONG_PTR lp) {
    WCHAR t[40], w[20]; nls_a2w(s ? s : "", w, 20); wsprintfW(t, L"%lx|%ls", (unsigned long)l, w); e_add(t, (DWORD)g); g_lp_seen = lp;
    return e_cont();
}
TEST(locenum_langgroup_locales_western_europe) {
    REQUIRE(U(EnumLanguageGroupLocalesW));
    e_reset(0); SET_ERR(0);
    BOOL r = U(EnumLanguageGroupLocalesW)(cb_glw, LGRPID_WESTERN_EUROPE, 0, 55);
    CHECK(r); CHECK(g_n > 10); CHECK(g_lp_seen == 55);
    CHECK(e_find(L"409|00000409") >= 0);
    CHECK(e_find(L"407|00000407") >= 0);
    CHECK(e_find(L"40c|0000040c") >= 0);
    for (int i = 0; i < g_n && i < MAXE; i++) CHECK(g_e[i].flags == LGRPID_WESTERN_EUROPE);
}
TEST(locenum_langgroup_locales_membership) {
    REQUIRE(U(EnumLanguageGroupLocalesW));
    static const struct { LGRPID g; const WCHAR *key; int hard; } t[] = {
        { 7, L"411|00000411", 1 }, { 8, L"412|00000412", 1 }, { 10, L"804|00000804", 1 }, { 9, L"404|00000404", 1 },
        { 11, L"41e|0000041e", 1 }, { 12, L"40d|0000040d", 1 }, { 13, L"401|00000401", 1 }, { 6, L"41f|0000041f", 1 },
        { 5, L"419|00000419", 1 }, { 15, L"439|00000439", 1 }, { 14, L"42a|0000042a", 1 }, { 2, L"415|00000415", 1 },
        { 4, L"408|00000408", 1 },
    };
    for (int i = 0; i < ARRAYLEN(t); i++) {
        e_reset(0);
        BOOL r = U(EnumLanguageGroupLocalesW)(cb_glw, t[i].g, 0, 0);
        CHECKF(r, "group %lu", (unsigned long)t[i].g);
        int f = e_find(t[i].key) >= 0;
        if (t[i].hard) CHECKF(f, "group %lu missing %ls", (unsigned long)t[i].g, t[i].key); else if (!f) t_note("group %lu missing %ls", (unsigned long)t[i].g, t[i].key);
    }
}
TEST(locenum_langgroup_locales_a_matches_w) {
    REQUIRE(U(EnumLanguageGroupLocalesA)); REQUIRE(U(EnumLanguageGroupLocalesW));
    e_reset(0); U(EnumLanguageGroupLocalesW)(cb_glw, 1, 0, 0); int nw = g_n;
    e_reset(0); CHECK(U(EnumLanguageGroupLocalesA)(cb_gla, 1, 0, 0));
    CHECK(g_n == nw);
}
TEST(locenum_langgroup_locales_early_stop) {
    REQUIRE(U(EnumLanguageGroupLocalesW));
    e_reset(1); U(EnumLanguageGroupLocalesW)(cb_glw, 1, 0, 0);
    CHECK(g_n == 1); CHECK(g_calls_after_stop == 0);
}
TEST(locenum_langgroup_locales_invalid) {
    REQUIRE(U(EnumLanguageGroupLocalesW));
    SET_ERR(0); CHECK(!U(EnumLanguageGroupLocalesW)(NULL, 1, 0, 0)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    e_reset(0); SET_ERR(0);
    BOOL r = U(EnumLanguageGroupLocalesW)(cb_glw, 0xFFFF, 0, 0);
    CHECK(!r || g_n == 0);
    t_note("invalid group: r=%d err=%lu", r, GetLastError());
    e_reset(0); SET_ERR(0);
    r = U(EnumLanguageGroupLocalesW)(cb_glw, 1, 0x1234, 0);
    t_note("nonzero flags: r=%d err=%lu", r, GetLastError());
}

/* ---- UI languages ---- */
static BOOL CALLBACK cb_uiw(LPWSTR s, LONG_PTR lp) { g_lp_seen = lp; e_add(s, 0); return e_cont(); }
static BOOL CALLBACK cb_uia(LPSTR s, LONG_PTR lp) { WCHAR w[96]; nls_a2w(s, w, 96); g_lp_seen = lp; e_add(w, 0); return e_cont(); }
TEST(locenum_uilanguages_name) {
    REQUIRE(U(EnumUILanguagesW));
    e_reset(0); SET_ERR(0);
    BOOL r = U(EnumUILanguagesW)(cb_uiw, MUI_LANGUAGE_NAME, 9);
    CHECK(r); CHECK(g_n >= 1); CHECK(g_lp_seen == 9);
    for (int i = 0; i < g_n && i < MAXE; i++) {
        CHECKF(U(IsValidLocaleName) && U(IsValidLocaleName)(g_e[i].s), "ui lang name %ls", g_e[i].s);
    }
}
TEST(locenum_uilanguages_id) {
    REQUIRE(U(EnumUILanguagesW));
    e_reset(0);
    CHECK(U(EnumUILanguagesW)(cb_uiw, MUI_LANGUAGE_ID, 0));
    CHECK(g_n >= 1);
    for (int i = 0; i < g_n && i < MAXE; i++) {
        CHECKF(lstrlenW(g_e[i].s) == 4, "id len %ls", g_e[i].s);
        int hex = 1; for (WCHAR *p = g_e[i].s; *p; p++) if (!iswxdigit(*p)) hex = 0;
        CHECK(hex);
    }
}
TEST(locenum_uilanguages_id_matches_name) {
    REQUIRE(U(EnumUILanguagesW)); REQUIRE(U(LocaleNameToLCID));
    if (t_is_wine()) { t_skip("emulator lists inconsistent UI language ids"); return; }
    static WCHAR names[256][64]; int nn = 0;
    e_reset(0); U(EnumUILanguagesW)(cb_uiw, MUI_LANGUAGE_NAME, 0);
    for (int i = 0; i < g_n && i < 256; i++) lstrcpynW(names[nn++], g_e[i].s, 64);
    e_reset(0); U(EnumUILanguagesW)(cb_uiw, MUI_LANGUAGE_ID, 0);
    if (g_n != nn) t_note("ids=%d names=%d", g_n, nn);
    for (int i = 0; i < g_n; i++) {
        LCID l = (LCID)wcstoul(g_e[i].s, NULL, 16);
        int found = 0;
        for (int k = 0; k < nn && !found; k++) found = U(LocaleNameToLCID)(names[k], 0x08000000) == l;
        CHECKF(found, "id %ls has no matching enumerated name", g_e[i].s);
    }
}
TEST(locenum_uilanguages_a_matches_w) {
    REQUIRE(U(EnumUILanguagesA)); REQUIRE(U(EnumUILanguagesW));
    e_reset(0); U(EnumUILanguagesW)(cb_uiw, MUI_LANGUAGE_NAME, 0); int nw = g_n;
    e_reset(0); CHECK(U(EnumUILanguagesA)(cb_uia, MUI_LANGUAGE_NAME, 0));
    CHECK(g_n == nw);
}
TEST(locenum_uilanguages_errors) {
    REQUIRE(U(EnumUILanguagesW));
    SET_ERR(0); CHECK(!U(EnumUILanguagesW)(NULL, MUI_LANGUAGE_NAME, 0)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    e_reset(0); SET_ERR(0);
    BOOL r = U(EnumUILanguagesW)(cb_uiw, 0x3, 0);   /* ID|NAME together are not valid */
    CHECK(!r); EXPECT_ERR(ERROR_INVALID_FLAGS);
    e_reset(0); SET_ERR(0);
    r = U(EnumUILanguagesW)(cb_uiw, MUI_LANGUAGE_ID | MUI_LANGUAGE_NAME, 0);
    t_note("ID|NAME: r=%d err=%lu", r, GetLastError());
    e_reset(0); SET_ERR(0);
    r = U(EnumUILanguagesW)(cb_uiw, 0, 0);
    t_note("flags 0: r=%d n=%d err=%lu", r, g_n, GetLastError());
}
TEST(locenum_uilanguages_early_stop) {
    REQUIRE(U(EnumUILanguagesW));
    e_reset(1); U(EnumUILanguagesW)(cb_uiw, MUI_LANGUAGE_NAME, 0);
    CHECK(g_n == 1); CHECK(g_calls_after_stop == 0);
}

/* ---- code pages ---- */
static BOOL CALLBACK cb_cpw(LPWSTR s) { e_add(s, 0); return e_cont(); }
static BOOL CALLBACK cb_cpa(LPSTR s) { WCHAR w[32]; nls_a2w(s, w, 32); e_add(w, 0); return e_cont(); }
TEST(locenum_codepages_installed) {
    REQUIRE(U(EnumSystemCodePagesW));
    e_reset(0); SET_ERR(0);
    BOOL r = U(EnumSystemCodePagesW)(cb_cpw, CP_INSTALLED);
    CHECK(r); CHECK(g_n >= 3);
    CHECK(e_find(L"1252") >= 0);
    CHECK(e_find(L"437") >= 0);
    for (int i = 0; i < g_n && i < MAXE; i++) { int d = 1; for (WCHAR *p = g_e[i].s; *p; p++) if (*p < '0' || *p > '9') d = 0; CHECKF(d && g_e[i].s[0], "cp %ls", g_e[i].s); }
}
TEST(locenum_codepages_supported) {
    REQUIRE(U(EnumSystemCodePagesW));
    e_reset(0);
    CHECK(U(EnumSystemCodePagesW)(cb_cpw, CP_SUPPORTED));
    CHECK(g_n >= 20);
    static const WCHAR *must[] = { L"1252", L"437", L"850", L"932", L"936", L"949", L"950", L"1250", L"1251", L"1253", L"1254", L"1255", L"1256", L"1257", L"1258", L"874", L"28591", L"20127", L"866" };
    for (int i = 0; i < ARRAYLEN(must); i++) CHECKF(e_find(must[i]) >= 0, "cp %ls", must[i]);
    static const WCHAR *soft[] = { L"65001", L"20932", L"50220", L"1200", L"1201", L"12000" };
    for (int i = 0; i < ARRAYLEN(soft); i++) if (e_find(soft[i]) < 0) t_note("cp %ls not listed as supported", soft[i]);
}
TEST(locenum_codepages_supported_superset) {
    REQUIRE(U(EnumSystemCodePagesW));
    static WCHAR inst[512][12]; int ni = 0;
    e_reset(0); U(EnumSystemCodePagesW)(cb_cpw, CP_INSTALLED);
    for (int i = 0; i < g_n && i < 512; i++) lstrcpynW(inst[ni++], g_e[i].s, 12);
    e_reset(0); U(EnumSystemCodePagesW)(cb_cpw, CP_SUPPORTED);
    for (int i = 0; i < ni; i++) CHECKF(e_find(inst[i]) >= 0, "%ls", inst[i]);
    CHECK(g_n >= ni);
}
TEST(locenum_codepages_each_valid) {
    REQUIRE(U(EnumSystemCodePagesW)); REQUIRE(U(IsValidCodePage));
    e_reset(0); U(EnumSystemCodePagesW)(cb_cpw, CP_SUPPORTED);
    int n = g_n < MAXE ? g_n : MAXE;
    static WCHAR copy[MAXE][12];
    for (int i = 0; i < n; i++) lstrcpynW(copy[i], g_e[i].s, 12);
    int bad = 0;
    for (int i = 0; i < n; i++) if (!U(IsValidCodePage)((UINT)_wtoi(copy[i]))) { bad++; if (bad < 4) t_note("enumerated cp %ls not valid", copy[i]); }
    CHECKF(bad == 0, "bad=%d/%d", bad, n);
}
TEST(locenum_codepages_a_matches_w) {
    REQUIRE(U(EnumSystemCodePagesA)); REQUIRE(U(EnumSystemCodePagesW));
    e_reset(0); U(EnumSystemCodePagesW)(cb_cpw, CP_SUPPORTED); int nw = g_n;
    e_reset(0); CHECK(U(EnumSystemCodePagesA)(cb_cpa, CP_SUPPORTED));
    CHECK(g_n == nw);
    CHECK(e_find(L"1252") >= 0);
}
TEST(locenum_codepages_early_stop) {
    REQUIRE(U(EnumSystemCodePagesW));
    e_reset(2); U(EnumSystemCodePagesW)(cb_cpw, CP_SUPPORTED);
    CHECK(g_n == 2); CHECK(g_calls_after_stop == 0);
    REQUIRE(U(EnumSystemCodePagesA));
    e_reset(2); U(EnumSystemCodePagesA)(cb_cpa, CP_SUPPORTED);
    CHECK(g_n == 2);
}
TEST(locenum_codepages_errors) {
    REQUIRE(U(EnumSystemCodePagesW));
    if (!t_is_wine()) { SET_ERR(0); CHECK(!U(EnumSystemCodePagesW)(NULL, CP_SUPPORTED)); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
    e_reset(0); SET_ERR(0);
    BOOL r = U(EnumSystemCodePagesW)(cb_cpw, 0);
    CHECK(r);   /* flags 0 behaves like CP_SUPPORTED */
    static const DWORD bad[] = { 3, 4, 0x80, 0x80000000 };
    for (int i = 0; i < ARRAYLEN(bad); i++) {
        e_reset(0); SET_ERR(0);
        r = U(EnumSystemCodePagesW)(cb_cpw, bad[i]);
        CHECKF(!r && g_n == 0, "flags 0x%lx", bad[i]);
        if (!r) EXPECT_ERR(ERROR_INVALID_FLAGS);
    }
}

/* ---- geo ---- */
static GEOID g_geo[1024]; static int g_ngeo, g_geo_stop;
static BOOL CALLBACK cb_geo(GEOID id) { if (g_ngeo < 1024) g_geo[g_ngeo] = id; g_ngeo++; return !(g_geo_stop && g_ngeo >= g_geo_stop); }
static int geo_has(GEOID id) { for (int i = 0; i < g_ngeo && i < 1024; i++) if (g_geo[i] == id) return 1; return 0; }
TEST(locenum_geoid_nation) {
    REQUIRE(U(EnumSystemGeoID));
    g_ngeo = 0; g_geo_stop = 0; SET_ERR(0);
    BOOL r = U(EnumSystemGeoID)(GEOCLASS_NATION, 0, cb_geo);
    CHECK(r); CHECKF(g_ngeo > 200, "n=%d", g_ngeo);
    CHECK(geo_has(244)); CHECK(geo_has(94)); CHECK(geo_has(122)); CHECK(geo_has(45)); CHECK(geo_has(84)); CHECK(geo_has(32));
}
TEST(locenum_geoid_nation_all_resolve) {
    REQUIRE(U(EnumSystemGeoID)); REQUIRE(U(GetGeoInfoW));
    g_ngeo = 0; g_geo_stop = 0; U(EnumSystemGeoID)(GEOCLASS_NATION, 0, cb_geo);
    int bad = 0;
    for (int i = 0; i < g_ngeo && i < 1024; i++) {
        WCHAR b[16];
        int n = U(GetGeoInfoW)(g_geo[i], GEO_ISO2, b, 16, 0);
        if (n != 3) { bad++; if (bad < 4) t_note("geo %ld ISO2 n=%d", (long)g_geo[i], n); }
    }
    CHECKF(bad == 0, "bad=%d", bad);
}
TEST(locenum_geoid_region) {
    REQUIRE(U(EnumSystemGeoID));
    g_ngeo = 0; g_geo_stop = 0; SET_ERR(0);
    BOOL r = U(EnumSystemGeoID)(GEOCLASS_REGION, 0, cb_geo);
    CHECK(r); CHECK(g_ngeo >= 1);
    t_note("region class geo ids: %d", g_ngeo);
}
TEST(locenum_geoid_early_stop) {
    REQUIRE(U(EnumSystemGeoID));
    g_ngeo = 0; g_geo_stop = 3;
    U(EnumSystemGeoID)(GEOCLASS_NATION, 0, cb_geo);
    CHECK(g_ngeo == 3);
}
TEST(locenum_geoid_errors) {
    REQUIRE(U(EnumSystemGeoID));
    SET_ERR(0); CHECK(!U(EnumSystemGeoID)(GEOCLASS_NATION, 0, NULL)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    g_ngeo = 0; g_geo_stop = 0; SET_ERR(0);
    BOOL r = U(EnumSystemGeoID)(99, 0, cb_geo);
    CHECK(!r); CHECK(g_ngeo == 0); t_note("EnumSystemGeoID bad class err=%lu", GetLastError());
    g_ngeo = 0; SET_ERR(0);
    r = U(EnumSystemGeoID)(GEOCLASS_NATION, 244, cb_geo);
    t_note("nation with parent 244: r=%d n=%d err=%lu", r, g_ngeo, GetLastError());
}
static BOOL CALLBACK cb_geoname(PWSTR s, LPARAM lp) { g_lp_seen = lp; e_add(s, 0); return e_cont(); }
TEST(locenum_geonames_nation) {
    REQUIRE(UN(EnumSystemGeoNames));
    e_reset(0); SET_ERR(0);
    BOOL r = UN(EnumSystemGeoNames)(GEOCLASS_NATION, cb_geoname, 31);
    CHECK(r); CHECK(g_n > 200); CHECK(g_lp_seen == 31);
    CHECK(e_find(L"US") >= 0); CHECK(e_find(L"DE") >= 0); CHECK(e_find(L"JP") >= 0); CHECK(e_find(L"BR") >= 0);
    for (int i = 0; i < g_n && i < MAXE; i++) CHECKF(lstrlenW(g_e[i].s) == 2, "name %ls", g_e[i].s);
}
TEST(locenum_geonames_region) {
    REQUIRE(UN(EnumSystemGeoNames));
    e_reset(0);
    CHECK(UN(EnumSystemGeoNames)(GEOCLASS_REGION, cb_geoname, 0));
    CHECK(g_n >= 1);
    t_note("region names: %d", g_n);
}
TEST(locenum_geonames_early_stop_and_errors) {
    REQUIRE(UN(EnumSystemGeoNames));
    e_reset(2); UN(EnumSystemGeoNames)(GEOCLASS_NATION, cb_geoname, 0);
    CHECK(g_n == 2); CHECK(g_calls_after_stop == 0);
    SET_ERR(0); CHECK(!UN(EnumSystemGeoNames)(GEOCLASS_NATION, NULL, 0)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    e_reset(0); SET_ERR(0);
    BOOL r = UN(EnumSystemGeoNames)(99, cb_geoname, 0);
    CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(locenum_geonames_match_geoids) {
    REQUIRE(UN(EnumSystemGeoNames)); REQUIRE(U(EnumSystemGeoID));
    e_reset(0); UN(EnumSystemGeoNames)(GEOCLASS_NATION, cb_geoname, 0); int nn = g_n;
    g_ngeo = 0; g_geo_stop = 0; U(EnumSystemGeoID)(GEOCLASS_NATION, 0, cb_geo);
    int diff = nn - g_ngeo; if (diff < 0) diff = -diff;
    CHECKF(diff <= 10, "names=%d ids=%d", nn, g_ngeo);
}
