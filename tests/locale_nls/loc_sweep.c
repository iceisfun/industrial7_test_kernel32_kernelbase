/* Sweeps over every supported locale and multi-threaded consistency checks. */
#include "nlsutil.h"

#define MAXL 2048
static WCHAR g_names[MAXL][86]; static DWORD g_flags[MAXL]; static int g_nn;
static BOOL CALLBACK collect(LPWSTR s, DWORD fl, LPARAM lp)
{
    if (g_nn < MAXL) { lstrcpynW(g_names[g_nn], s, 86); g_flags[g_nn] = fl; g_nn++; }
    return TRUE;
}
static int collect_all(DWORD flags)
{
    g_nn = 0;
    return U(EnumSystemLocalesEx)(collect, flags, 0, NULL) ? g_nn : -1;
}

TEST(locsweep_every_name_reports_itself) {
    REQUIRE(U(EnumSystemLocalesEx)); REQUIRE(U(GetLocaleInfoEx));
    int n = collect_all(LOCALE_ALL);
    CHECKF(n > 200, "n=%d", n);
    int bad = 0;
    for (int i = 0; i < n; i++) {
        WCHAR b[100];
        int r = nls_str(g_names[i], L_SNAME, b, 100);
        if (r <= 0 || lstrcmpiW(b, g_names[i])) { if (++bad < 5) t_note("name %ls -> SNAME '%ls'", g_names[i], r > 0 ? b : L"(fail)"); }
    }
    CHECKF(bad == 0, "bad=%d/%d", bad, n);
}
TEST(locsweep_every_specific_locale_roundtrips_lcid) {
    REQUIRE(U(EnumSystemLocalesEx)); REQUIRE(U(LocaleNameToLCID)); REQUIRE(U(LCIDToLocaleName));
    int n = collect_all(LOCALE_SPECIFICDATA);
    CHECK(n > 100);
    int bad = 0;
    for (int i = 0; i < n; i++) {
        LCID l = U(LocaleNameToLCID)(g_names[i], 0);
        WCHAR b[100];
        if (l == 0) { bad++; continue; }
        if (l == 0x1000) continue;     /* locales without an assigned LCID */
        if (U(LCIDToLocaleName)(l, b, 100, 0) <= 0) { bad++; if (bad < 5) t_note("lcid 0x%lx of %ls has no name", l, g_names[i]); }
    }
    CHECKF(bad == 0, "bad=%d/%d", bad, n);
}
TEST(locsweep_numeric_fields_parse) {
    REQUIRE(U(EnumSystemLocalesEx)); REQUIRE(U(GetLocaleInfoEx));
    int n = collect_all(LOCALE_SPECIFICDATA);
    int bad = 0;
    static const DWORD nums[] = { L_ICOUNTRY, L_IMEASURE, L_IDIGITS, L_ILZERO, L_INEGNUMBER, L_ICURRDIGITS, L_ICURRENCY, L_INEGCURR, L_IDATE, L_ILDATE, L_ITIME, L_ICALENDARTYPE, L_IFIRSTDAYOFWEEK, L_IFIRSTWEEKOFYEAR, L_IGEOID };
    for (int i = 0; i < n && i < 300; i++)
        for (int k = 0; k < ARRAYLEN(nums); k++) {
            int ok; DWORD v = nls_num(g_names[i], nums[k], &ok);
            WCHAR b[32]; int r = nls_str(g_names[i], nums[k], b, 32);
            if (!ok || r <= 1 || v != (DWORD)_wtoi(b)) { if (++bad < 5) t_note("%ls type 0x%lx ok=%d v=%lu str=%ls", g_names[i], nums[k], ok, v, r > 0 ? b : L"?"); }
        }
    CHECKF(bad == 0, "bad=%d", bad);
}
TEST(locsweep_iso_names_shape) {
    REQUIRE(U(EnumSystemLocalesEx)); REQUIRE(U(GetLocaleInfoEx));
    int n = collect_all(LOCALE_SPECIFICDATA);
    int bad = 0;
    for (int i = 0; i < n; i++) {
        WCHAR l2[16], c2[16];
        int a = nls_str(g_names[i], L_SISO639LANGNAME, l2, 16), b = nls_str(g_names[i], L_SISO3166CTRYNAME, c2, 16);
        if (a < 3 || a > 4 || b < 3 || b > 4) { if (++bad < 5) t_note("%ls lang '%ls' ctry '%ls'", g_names[i], a > 0 ? l2 : L"?", b > 0 ? c2 : L"?"); }
    }
    CHECKF(bad == 0, "bad=%d/%d", bad, n);
}
TEST(locsweep_sname_prefix_is_iso_lang) {
    REQUIRE(U(EnumSystemLocalesEx)); REQUIRE(U(GetLocaleInfoEx));
    int n = collect_all(LOCALE_SPECIFICDATA);
    int bad = 0;
    for (int i = 0; i < n; i++) {
        WCHAR l2[16]; nls_str(g_names[i], L_SISO639LANGNAME, l2, 16);
        size_t ll = wcslen(l2);
        if (wcsncmp(g_names[i], l2, ll) || (g_names[i][ll] != '-' && g_names[i][ll] != 0 && g_names[i][ll] != '_')) { if (++bad < 5) t_note("%ls vs iso lang %ls", g_names[i], l2); }
    }
    CHECKF(bad <= n / 50, "bad=%d/%d", bad, n);
}
TEST(locsweep_codepages_are_known) {
    REQUIRE(U(EnumSystemLocalesEx)); REQUIRE(U(GetLocaleInfoEx)); REQUIRE(U(IsValidCodePage));
    int n = collect_all(LOCALE_SPECIFICDATA);
    int bad = 0;
    for (int i = 0; i < n; i++) {
        WCHAR b[16];
        if (nls_str(g_names[i], L_IDEFAULTANSICP, b, 16) <= 0) { bad++; continue; }
        int acp = _wtoi(b);
        if (acp != 0 && acp != 1 && !U(IsValidCodePage)(acp)) { if (++bad < 5) t_note("%ls acp %d invalid", g_names[i], acp); }
        if (nls_str(g_names[i], L_IDEFAULTCODEPAGE, b, 16) <= 0) { bad++; continue; }
        int oem = _wtoi(b);
        if (oem != 0 && oem != 1 && !U(IsValidCodePage)(oem)) { if (++bad < 5) t_note("%ls oem %d invalid", g_names[i], oem); }
    }
    CHECKF(bad == 0, "bad=%d/%d", bad, n);
}
TEST(locsweep_formats_work_for_every_specific_locale) {
    REQUIRE(U(EnumSystemLocalesEx)); REQUIRE(U(GetDateFormatEx)); REQUIRE(U(GetTimeFormatEx)); REQUIRE(U(GetNumberFormatEx)); REQUIRE(U(GetCurrencyFormatEx));
    int n = collect_all(LOCALE_SPECIFICDATA);
    SYSTEMTIME st = nls_st(2024, 3, 5, 14, 7, 9, 0);
    int bad = 0;
    for (int i = 0; i < n; i++) {
        WCHAR b[400];
        if (U(GetDateFormatEx)(g_names[i], N_NOUSER | DATE_LONGDATE, &st, NULL, b, 400, NULL) <= 0) { if (++bad < 5) t_note("%ls long date err %lu", g_names[i], GetLastError()); continue; }
        if (U(GetDateFormatEx)(g_names[i], N_NOUSER | DATE_SHORTDATE, &st, NULL, b, 400, NULL) <= 0) { bad++; continue; }
        if (U(GetTimeFormatEx)(g_names[i], N_NOUSER, &st, NULL, b, 400) <= 0) { bad++; continue; }
        if (U(GetNumberFormatEx)(g_names[i], N_NOUSER, L"-1234567.891", NULL, b, 400) <= 0) { bad++; continue; }
        if (U(GetCurrencyFormatEx)(g_names[i], N_NOUSER, L"-1234567.891", NULL, b, 400) <= 0) { bad++; continue; }
    }
    CHECKF(bad == 0, "bad=%d/%d", bad, n);
}
TEST(locsweep_calendar_default_supported) {
    REQUIRE(U(EnumSystemLocalesEx)); REQUIRE(UN(GetCalendarInfoEx));
    int n = collect_all(LOCALE_SPECIFICDATA);
    int bad = 0;
    for (int i = 0; i < n; i++) {
        int ok; DWORD cal = nls_num(g_names[i], L_ICALENDARTYPE, &ok);
        WCHAR b[100];
        if (!ok || cal == 0) { bad++; continue; }
        if (UN(GetCalendarInfoEx)(g_names[i], cal, NULL, CAL_SMONTHNAME1 | N_NOUSER, b, 100, NULL) <= 0) { if (++bad < 5) t_note("%ls default calendar %lu month name err %lu", g_names[i], cal, GetLastError()); }
    }
    CHECKF(bad == 0, "bad=%d/%d", bad, n);
}
TEST(locsweep_neutral_names_have_no_country_data) {
    REQUIRE(U(EnumSystemLocalesEx)); REQUIRE(U(GetLocaleInfoEx));
    int n = collect_all(LOCALE_NEUTRALDATA);
    CHECK(n > 20);
    int bad = 0;
    for (int i = 0; i < n; i++) {
        if (!g_names[i][0]) continue;   /* P1: Win10 LOCALE_NEUTRALDATA also enumerates the invariant name "" (INEUTRAL 0) */
        int ok; DWORD v = nls_num(g_names[i], L_INEUTRAL, &ok);
        if (!ok || v != 1) { if (++bad < 5) t_note("neutral %ls INEUTRAL=%lu ok=%d", g_names[i], v, ok); }
    }
    CHECKF(bad == 0, "bad=%d/%d", bad, n);
}

/* ---- concurrent use ---- */
typedef struct { int iters; int bad; int which; } thr_t;
static DWORD WINAPI worker(LPVOID p)
{
    thr_t *t = (thr_t *)p;
    SYSTEMTIME st = nls_st(2024, 3, 5, 14, 7, 9, 0);
    for (int i = 0; i < t->iters; i++) {
        WCHAR b[200]; int k = (i + t->which) % ARRAYLEN(FIXED_LOCALES);
        int r = U(GetLocaleInfoEx)(FIXED_LOCALES[k], LOCALE_SNAME | N_NOUSER, b, 200);
        if (r != lstrlenW(FIXED_LOCALES[k]) + 1 || lstrcmpW(b, FIXED_LOCALES[k])) t->bad++;
        if (U(GetDateFormatEx)(L"en-US", 0, &st, L"dddd, MMMM d, yyyy", b, 200, NULL) != 23 || lstrcmpW(b, L"Tuesday, March 5, 2024")) t->bad++;
        if (U(GetNumberFormatEx)(L"de-DE", N_NOUSER, L"1234567.891", NULL, b, 200) != 13 || lstrcmpW(b, L"1.234.567,89")) t->bad++;
        if (U(LocaleNameToLCID)(FIXED_LOCALES[k], 0) != FIXED_LCIDS[k]) t->bad++;
        if (U(GetTimeFormatEx)(L"en-US", 0, &st, L"h:mm:ss tt", b, 200) != 11 || lstrcmpW(b, L"2:07:09 PM")) t->bad++;
    }
    return 0;
}
TEST(locsweep_concurrent_queries) {
    REQUIRE(U(GetLocaleInfoEx)); REQUIRE(U(GetDateFormatEx)); REQUIRE(U(GetNumberFormatEx)); REQUIRE(U(LocaleNameToLCID)); REQUIRE(U(GetTimeFormatEx));
    enum { NT = 8 };
    HANDLE h[NT]; thr_t t[NT];
    for (int i = 0; i < NT; i++) { t[i].iters = 1500; t[i].bad = 0; t[i].which = i; h[i] = CreateThread(NULL, 0, worker, &t[i], 0, NULL); CHECK(h[i] != NULL); }
    for (int i = 0; i < NT; i++) if (h[i]) { DWORD w = WaitForSingleObject(h[i], 60000); CHECK(w == WAIT_OBJECT_0); CloseHandle(h[i]); }
    for (int i = 0; i < NT; i++) CHECKF(t[i].bad == 0, "thread %d bad=%d", i, t[i].bad);
}
static DWORD WINAPI worker_tz(LPVOID p)
{
    thr_t *t = (thr_t *)p;
    TIME_ZONE_INFORMATION z; memset(&z, 0, sizeof z);
    z.Bias = 480; z.DaylightBias = -60;
    z.StandardDate.wMonth = 11; z.StandardDate.wDay = 1; z.StandardDate.wHour = 2;
    z.DaylightDate.wMonth = 3; z.DaylightDate.wDay = 2; z.DaylightDate.wHour = 2;
    for (int i = 0; i < t->iters; i++) {
        SYSTEMTIME u = nls_st(2024, 7, 15, 12, 0, 0, 0), l;
        if (!U(SystemTimeToTzSpecificLocalTime)(&z, &u, &l) || l.wHour != 5) t->bad++;
        u = nls_st(2024, 1, 15, 12, 0, 0, 0);
        if (!U(SystemTimeToTzSpecificLocalTime)(&z, &u, &l) || l.wHour != 4) t->bad++;
    }
    return 0;
}
TEST(locsweep_concurrent_timezone_conversion) {
    REQUIRE(U(SystemTimeToTzSpecificLocalTime));
    enum { NT = 6 };
    HANDLE h[NT]; thr_t t[NT];
    for (int i = 0; i < NT; i++) { t[i].iters = 3000; t[i].bad = 0; t[i].which = i; h[i] = CreateThread(NULL, 0, worker_tz, &t[i], 0, NULL); }
    for (int i = 0; i < NT; i++) if (h[i]) { WaitForSingleObject(h[i], 60000); CloseHandle(h[i]); }
    for (int i = 0; i < NT; i++) CHECKF(t[i].bad == 0, "thread %d bad=%d", i, t[i].bad);
}
static DWORD WINAPI worker_enum(LPVOID p)
{
    thr_t *t = (thr_t *)p;
    for (int i = 0; i < t->iters; i++) {
        WCHAR b[100]; DWORD d = U(GetThreadLocale)();
        if (!d) t->bad++;
        if (U(GetUserDefaultLocaleName)(b, 100) <= 0) t->bad++;
        if (U(GetSystemDefaultLocaleName)(b, 100) <= 0) t->bad++;
        if (U(GetUserDefaultLCID)() == 0) t->bad++;
    }
    return 0;
}
TEST(locsweep_concurrent_default_queries) {
    REQUIRE(U(GetThreadLocale)); REQUIRE(U(GetUserDefaultLocaleName)); REQUIRE(U(GetSystemDefaultLocaleName)); REQUIRE(U(GetUserDefaultLCID));
    enum { NT = 8 };
    HANDLE h[NT]; thr_t t[NT];
    for (int i = 0; i < NT; i++) { t[i].iters = 2000; t[i].bad = 0; t[i].which = i; h[i] = CreateThread(NULL, 0, worker_enum, &t[i], 0, NULL); }
    for (int i = 0; i < NT; i++) if (h[i]) { WaitForSingleObject(h[i], 60000); CloseHandle(h[i]); }
    for (int i = 0; i < NT; i++) CHECKF(t[i].bad == 0, "thread %d bad=%d", i, t[i].bad);
}
