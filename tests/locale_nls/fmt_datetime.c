/* GetDateFormat / GetTimeFormat / GetDurationFormat (A, W, Ex). */
#include "nlsutil.h"

UDECL(int, GetDurationFormatEx, (LPCWSTR, DWORD, const SYSTEMTIME *, ULONGLONG, LPCWSTR, LPWSTR, int));

#define DATE_MD 0x80
static int df(const WCHAR *loc, DWORD fl, const SYSTEMTIME *st, const WCHAR *pic, WCHAR *out, int cch)
{
    return U(GetDateFormatEx)(loc, fl, st, pic, out, cch, NULL);
}
static int tf(const WCHAR *loc, DWORD fl, const SYSTEMTIME *st, const WCHAR *pic, WCHAR *out, int cch)
{
    return U(GetTimeFormatEx)(loc, fl, st, pic, out, cch);
}

typedef struct { const WCHAR *loc; DWORD fl; int y, mo, d; const WCHAR *pic, *out; int hard; } drow_t;
static void run_df(const drow_t *t, int n)
{
    REQUIRE(U(GetDateFormatEx)); REQUIRE(U(GetDateFormatW)); REQUIRE(U(LocaleNameToLCID));
    for (int i = 0; i < n; i++) {
        SYSTEMTIME st = nls_st(t[i].y, t[i].mo, t[i].d, 13, 5, 7, 9);
        WCHAR b[300]; SET_ERR(0);
        int r = df(t[i].loc, t[i].fl, &st, t[i].pic, b, 300);
        if (r <= 0) { CHECKF(0, "row %d %ls pic '%ls' err %lu", i, t[i].loc, t[i].pic ? t[i].pic : L"(null)", GetLastError()); continue; }
        CHECKF(r == lstrlenW(b) + 1, "row %d r=%d len=%d", i, r, lstrlenW(b));
        EXPECT_WS(t[i].hard, t[i].loc, "date", b, t[i].out);
        if (!t[i].pic || 1) {
            WCHAR b2[300];
            int r2 = U(GetDateFormatW)(U(LocaleNameToLCID)(t[i].loc, 0), t[i].fl, &st, t[i].pic, b2, 300);
            CHECKF(r2 == r && !lstrcmpW(b, b2), "row %d W parity", i);
        }
    }
}

TEST(datefmt_enus_pictures) {
    static const drow_t t[] = {
        { L"en-US", 0, 2024, 3, 5, L"d", L"5", 1 }, { L"en-US", 0, 2024, 3, 15, L"d", L"15", 1 },
        { L"en-US", 0, 2024, 3, 5, L"dd", L"05", 1 }, { L"en-US", 0, 2024, 3, 15, L"dd", L"15", 1 },
        { L"en-US", 0, 2024, 3, 5, L"ddd", L"Tue", 1 }, { L"en-US", 0, 2024, 3, 5, L"dddd", L"Tuesday", 1 },
        { L"en-US", 0, 2024, 3, 3, L"dddd", L"Sunday", 1 }, { L"en-US", 0, 2024, 3, 4, L"ddd", L"Mon", 1 },
        { L"en-US", 0, 2024, 3, 9, L"dddd", L"Saturday", 1 }, { L"en-US", 0, 2024, 3, 8, L"ddd", L"Fri", 1 },
        { L"en-US", 0, 2024, 3, 7, L"dddd", L"Thursday", 1 }, { L"en-US", 0, 2024, 3, 6, L"dddd", L"Wednesday", 1 },
        { L"en-US", 0, 2024, 3, 5, L"M", L"3", 1 }, { L"en-US", 0, 2024, 12, 5, L"M", L"12", 1 },
        { L"en-US", 0, 2024, 3, 5, L"MM", L"03", 1 }, { L"en-US", 0, 2024, 12, 5, L"MM", L"12", 1 },
        { L"en-US", 0, 2024, 3, 5, L"MMM", L"Mar", 1 }, { L"en-US", 0, 2024, 3, 5, L"MMMM", L"March", 1 },
        { L"en-US", 0, 2024, 1, 1, L"MMMM", L"January", 1 }, { L"en-US", 0, 2024, 2, 1, L"MMM", L"Feb", 1 },
        { L"en-US", 0, 2024, 5, 1, L"MMMM", L"May", 1 }, { L"en-US", 0, 2024, 9, 1, L"MMMM", L"September", 1 },
        { L"en-US", 0, 2024, 12, 31, L"MMMM", L"December", 1 }, { L"en-US", 0, 2024, 12, 31, L"MMM", L"Dec", 1 },
        { L"en-US", 0, 2024, 3, 5, L"yy", L"24", 1 }, { L"en-US", 0, 2005, 3, 5, L"yy", L"05", 1 },
        { L"en-US", 0, 1999, 3, 5, L"yy", L"99", 1 }, { L"en-US", 0, 2000, 3, 5, L"yy", L"00", 1 },
        { L"en-US", 0, 2024, 3, 5, L"yyyy", L"2024", 1 }, { L"en-US", 0, 1601, 3, 5, L"yyyy", L"1601", 1 },
        { L"en-US", 0, 999 + 1000, 3, 5, L"yyyy", L"1999", 1 }, { L"en-US", 0, 9999, 3, 5, L"yyyy", L"9999", 1 },
        { L"en-US", 0, 2024, 3, 5, L"yyyyy", L"2024", 0 }, { L"en-US", 0, 2024, 3, 5, L"y", L"24", 0 },
        { L"en-US", 0, 2005, 3, 5, L"y", L"5", 0 }, { L"en-US", 0, 2024, 3, 5, L"yyy", L"2024", 0 },
        { L"en-US", 0, 2024, 3, 5, L"gg", L"A.D.", 0 }, { L"en-US", 0, 2024, 3, 5, L"g", L"A.D.", 0 },
        { L"en-US", 0, 2024, 3, 5, L"M/d/yyyy", L"3/5/2024", 1 }, { L"en-US", 0, 2024, 3, 5, L"MM/dd/yy", L"03/05/24", 1 },
        { L"en-US", 0, 2024, 3, 5, L"yyyy-MM-dd", L"2024-03-05", 1 }, { L"en-US", 0, 2024, 3, 5, L"dd.MM.yyyy", L"05.03.2024", 1 },
        { L"en-US", 0, 2024, 3, 5, L"dddd, MMMM d, yyyy", L"Tuesday, March 5, 2024", 1 },
        { L"en-US", 0, 2024, 3, 5, L"ddd MMM dd yyyy", L"Tue Mar 05 2024", 1 },
        { L"en-US", 0, 2024, 3, 5, L"MMMM yyyy", L"March 2024", 1 }, { L"en-US", 0, 2024, 3, 5, L"MMMM d", L"March 5", 1 },
        { L"en-US", 0, 2024, 3, 5, L"'Day' d", L"Day 5", 1 }, { L"en-US", 0, 2024, 3, 5, L"d'th of 'MMMM", L"5th of March", 1 },
        { L"en-US", 0, 2024, 3, 5, L"'yyyy'", L"yyyy", 1 }, { L"en-US", 0, 2024, 3, 5, L"'dddd' dddd", L"dddd Tuesday", 1 },
        { L"en-US", 0, 2024, 3, 5, L"d''M", L"5'3", 0 }, { L"en-US", 0, 2024, 3, 5, L"'it''s' d", L"it's 5", 1 },
        { L"en-US", 0, 2024, 3, 5, L"''", L"'", 0 }, { L"en-US", 0, 2024, 3, 5, L"'abc", L"abc", 0 },
        { L"en-US", 0, 2024, 3, 5, L"", L"", 0 }, { L"en-US", 0, 2024, 3, 5, L"xzq", L"xzq", 1 },
        { L"en-US", 0, 2024, 3, 5, L"d-M-yy", L"5-3-24", 1 }, { L"en-US", 0, 2024, 3, 5, L"d/M", L"5/3", 1 },
        { L"en-US", 0, 2024, 3, 5, L"dddd dddd", L"Tuesday Tuesday", 1 }, { L"en-US", 0, 2024, 3, 5, L"MMMMM", L"March", 0 },
        { L"en-US", 0, 2024, 3, 5, L"ddddd", L"Tuesday", 0 }, { L"en-US", 0, 2024, 3, 5, L"ddMMyyyy", L"05032024", 1 },
        { L"en-US", 0, 2024, 3, 5, L"d MMMM yyyy", L"5 March 2024", 1 }, { L"en-US", 0, 2024, 3, 5, L"   d   ", L"   5   ", 1 },
        { L"en-US", 0, 2024, 3, 5, L"h m s", L"h m s", 1 }, { L"en-US", 0, 2024, 3, 5, L"H:mm", L"H:mm", 1 },
        { L"en-US", 0, 2024, 2, 29, L"dd MMM yyyy", L"29 Feb 2024", 1 }, { L"en-US", 0, 2000, 2, 29, L"yyyy-MM-dd", L"2000-02-29", 1 },
        { L"en-US", 0, 2024, 12, 31, L"yyyy-MM-dd", L"2024-12-31", 1 }, { L"en-US", 0, 2023, 1, 1, L"dddd", L"Sunday", 1 },
        { L"en-US", 0, 1970, 1, 1, L"dddd", L"Thursday", 1 }, { L"en-US", 0, 1601, 1, 1, L"dddd", L"Monday", 1 },
        { L"en-US", 0, 2000, 1, 1, L"dddd", L"Saturday", 1 }, { L"en-US", 0, 1900, 3, 1, L"dddd", L"Thursday", 1 },
        { L"en-US", 0, 2100, 3, 1, L"dddd", L"Monday", 1 }, { L"en-US", 0, 30827, 12, 31, L"yyyy", L"30827", 0 },
        { L"en-US", 0, 2038, 1, 19, L"dddd", L"Tuesday", 1 },
    };
    run_df(t, ARRAYLEN(t));
}
TEST(datefmt_enus_flags) {
    static const drow_t t[] = {
        { L"en-US", N_NOUSER, 2024, 3, 5, NULL, L"3/5/2024", 0 }, { L"en-US", N_NOUSER | DATE_SHORTDATE, 2024, 3, 5, NULL, L"3/5/2024", 0 },
        { L"en-US", N_NOUSER | DATE_LONGDATE, 2024, 3, 5, NULL, L"Tuesday, March 5, 2024", 0 },
        { L"en-US", N_NOUSER | DATE_YEARMONTH, 2024, 3, 5, NULL, L"March 2024", 0 },
        { L"en-US", N_NOUSER | DATE_SHORTDATE, 1999, 12, 31, NULL, L"12/31/1999", 0 },
        { L"en-US", N_NOUSER | DATE_LONGDATE, 2000, 1, 1, NULL, L"Saturday, January 1, 2000", 0 },
        { L"en-US", N_NOUSER | DATE_YEARMONTH, 2000, 12, 1, NULL, L"December 2000", 0 },
        { L"en-US", N_NOUSER | DATE_AUTOLAYOUT, 2024, 3, 5, NULL, L"3/5/2024", 0 },
        { L"en-US", N_NOUSER | DATE_LTRREADING, 2024, 3, 5, NULL, L"3/5/2024", 0 },
        { L"en-US", N_NOUSER | DATE_RTLREADING, 2024, 3, 5, NULL, L"3/5/2024", 0 },
        { L"en-US", DATE_USE_ALT_CALENDAR, 2024, 3, 5, L"M/d/yyyy", L"3/5/2024", 1 },
        { L"en-US", DATE_LTRREADING, 2024, 3, 5, L"M/d/yyyy", L"3/5/2024", 0 },
        { L"en-US", DATE_AUTOLAYOUT, 2024, 3, 5, L"M/d/yyyy", L"3/5/2024", 0 },
        { L"de-DE", N_NOUSER | DATE_LONGDATE, 2024, 3, 5, NULL, L"Dienstag, 5. März 2024", 0 },
        { L"de-DE", N_NOUSER | DATE_SHORTDATE, 2024, 3, 5, NULL, L"05.03.2024", 0 },
        { L"fr-FR", N_NOUSER | DATE_LONGDATE, 2024, 3, 5, NULL, L"mardi 5 mars 2024", 0 },
        { L"fr-FR", N_NOUSER | DATE_SHORTDATE, 2024, 3, 5, NULL, L"05/03/2024", 0 },
        { L"ja-JP", N_NOUSER | DATE_SHORTDATE, 2024, 3, 5, NULL, L"2024/03/05", 0 },
        { L"ja-JP", N_NOUSER | DATE_LONGDATE, 2024, 3, 5, NULL, L"2024年3月5日", 0 },
        { L"zh-CN", N_NOUSER | DATE_LONGDATE, 2024, 3, 5, NULL, L"2024年3月5日", 0 },
        { L"", N_NOUSER | DATE_SHORTDATE, 2024, 3, 5, NULL, L"03/05/2024", 1 },
        { L"", N_NOUSER | DATE_LONGDATE, 2024, 3, 5, NULL, L"Tuesday, 05 March 2024", 1 },
        { L"", N_NOUSER | DATE_YEARMONTH, 2024, 3, 5, NULL, L"2024 March", 0 },
    };
    run_df(t, ARRAYLEN(t));
}
TEST(datefmt_other_locale_pictures) {
    static const drow_t t[] = {
        { L"de-DE", 0, 2024, 3, 5, L"dddd, d. MMMM yyyy", L"Dienstag, 5. März 2024", 1 },
        { L"de-DE", 0, 2024, 3, 3, L"dddd", L"Sonntag", 1 }, { L"de-DE", 0, 2024, 12, 1, L"MMMM", L"Dezember", 1 },
        { L"fr-FR", 0, 2024, 3, 5, L"dddd d MMMM yyyy", L"mardi 5 mars 2024", 1 },
        { L"fr-FR", 0, 2024, 2, 5, L"MMMM", L"février", 1 }, { L"fr-FR", 0, 2024, 8, 5, L"MMMM", L"août", 1 },
        { L"ja-JP", 0, 2024, 3, 5, L"yyyy'年'M'月'd'日' dddd", L"2024年3月5日 火曜日", 1 },
        { L"ja-JP", 0, 2024, 3, 3, L"dddd", L"日曜日", 1 },
        { L"zh-CN", 0, 2024, 3, 5, L"dddd", L"星期二", 1 }, { L"zh-CN", 0, 2024, 3, 5, L"MMMM", L"三月", 1 },
        { L"ko-KR", 0, 2024, 3, 5, L"dddd", L"화요일", 1 }, { L"ko-KR", 0, 2024, 3, 5, L"MMMM", L"3월", 1 },
        { L"ru-RU", 0, 2024, 3, 5, L"dddd", L"вторник", 1 },
        { L"ru-RU", 0, 2024, 3, 5, L"MMMM", L"март", 1 },
        { L"tr-TR", 0, 2024, 3, 5, L"dddd", L"Salı", 1 }, { L"tr-TR", 0, 2024, 2, 5, L"MMMM", L"Şubat", 1 },
        { L"pt-BR", 0, 2024, 3, 5, L"dddd", L"terça-feira", 1 }, { L"pt-BR", 0, 2024, 3, 5, L"MMMM", L"março", 1 },
        { L"th-TH", 0, 2024, 3, 5, L"yyyy", L"2567", 0 }, { L"th-TH", 0, 2024, 3, 5, L"yy", L"67", 0 },
        { L"th-TH", 0, 1999, 3, 5, L"yyyy", L"2542", 0 },
        { L"ar-SA", 0, 2024, 3, 5, L"yyyy", L"1445", 0 }, { L"he-IL", 0, 2024, 3, 5, L"yyyy", L"2024", 1 },
        { L"hi-IN", 0, 2024, 3, 5, L"yyyy", L"2024", 1 },
        { L"en-GB", 0, 2024, 3, 5, L"dddd d MMMM yyyy", L"Tuesday 5 March 2024", 1 },
        { L"es-ES", 0, 2024, 3, 5, L"dddd", L"martes", 1 }, { L"it-IT", 0, 2024, 3, 5, L"dddd", L"martedì", 1 },
        { L"nl-NL", 0, 2024, 3, 5, L"dddd", L"dinsdag", 1 }, { L"sv-SE", 0, 2024, 3, 5, L"dddd", L"tisdag", 1 },
        { L"pl-PL", 0, 2024, 3, 5, L"dddd", L"wtorek", 1 },
    };
    run_df(t, ARRAYLEN(t));
}
TEST(datefmt_day_of_week_ignored) {
    REQUIRE(U(GetDateFormatEx));
    for (int dow = 0; dow < 7; dow++) {
        SYSTEMTIME st = nls_st(2024, 3, 5, 0, 0, 0, 0); st.wDayOfWeek = (WORD)dow;
        WCHAR b[100]; int r = df(L"en-US", 0, &st, L"dddd", b, 100);
        CHECKF(r == 8 && !lstrcmpW(b, L"Tuesday"), "dow=%d got %ls", dow, b);
    }
    SYSTEMTIME st = nls_st(2024, 3, 5, 0, 0, 0, 0); st.wDayOfWeek = 99;
    WCHAR b[100]; int r = df(L"en-US", 0, &st, L"ddd", b, 100);
    CHECKF(r == 4 && !lstrcmpW(b, L"Tue"), "dow=99 got %ls", b);
}
TEST(datefmt_time_fields_ignored) {
    REQUIRE(U(GetDateFormatEx));
    SYSTEMTIME st = nls_st(2024, 3, 5, 99, 99, 99, 9999);
    WCHAR b[100]; SET_ERR(0);
    int r = df(L"en-US", 0, &st, L"yyyy-MM-dd", b, 100);
    CHECKF(r == 11 && !lstrcmpW(b, L"2024-03-05"), "r=%d err=%lu", r, GetLastError());
}
TEST(datefmt_invalid_systemtime) {
    REQUIRE(U(GetDateFormatEx));
    static const struct { int y, m, d; const char *why; } t[] = {
        { 2024, 0, 1, "month 0" }, { 2024, 13, 1, "month 13" }, { 2024, 1, 0, "day 0" }, { 2024, 1, 32, "day 32" },
        { 2024, 4, 31, "apr 31" }, { 2023, 2, 29, "feb 29 2023" }, { 2024, 2, 30, "feb 30" }, { 1900, 2, 29, "feb 29 1900" },
        { 2100, 2, 29, "feb 29 2100" }, { 1600, 12, 31, "year 1600" }, { 1, 1, 1, "year 1" }, { 0, 1, 1, "year 0" },
        { 30828, 1, 1, "year 30828" }, { 65535, 1, 1, "year 65535" }, { 2024, 65535, 1, "month 65535" }, { 2024, 1, 65535, "day 65535" },
        { 2024, 6, 31, "jun 31" }, { 2024, 9, 31, "sep 31" }, { 2024, 11, 31, "nov 31" },
    };
    for (int i = 0; i < ARRAYLEN(t); i++) {
        SYSTEMTIME st = nls_st(t[i].y, t[i].m, t[i].d, 0, 0, 0, 0);
        WCHAR b[100]; SET_ERR(0);
        int r = df(L"en-US", 0, &st, L"yyyy-MM-dd", b, 100);
        int cal_impossible = t[i].m >= 1 && t[i].m <= 12 && t[i].d >= 1 && t[i].d <= 31 && t[i].y >= 1601 && t[i].y <= 30827;
        if (cal_impossible) {     /* day-of-month vs month/leap validation is not visible in the front-end decomp */
            if (r != 0) t_note("%s accepted (r=%d)", t[i].why, r);
            continue;
        }
        CHECKF(r == 0, "%s r=%d (%ls)", t[i].why, r, b);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(datefmt_valid_boundaries) {
    REQUIRE(U(GetDateFormatEx));
    static const struct { int y, m, d; } t[] = {
        { 1601, 1, 1 }, { 1601, 12, 31 }, { 30827, 12, 31 }, { 30827, 1, 1 }, { 2024, 2, 29 }, { 2000, 2, 29 }, { 2023, 2, 28 },
        { 2024, 1, 31 }, { 2024, 4, 30 }, { 2024, 12, 31 }, { 1604, 2, 29 }, { 30824, 2, 29 },
    };
    for (int i = 0; i < ARRAYLEN(t); i++) {
        SYSTEMTIME st = nls_st(t[i].y, t[i].m, t[i].d, 0, 0, 0, 0);
        WCHAR b[100]; SET_ERR(0);
        int r = df(L"en-US", 0, &st, L"d", b, 100);
        CHECKF(r > 0, "%d-%d-%d err=%lu", t[i].y, t[i].m, t[i].d, GetLastError());
    }
}
TEST(datefmt_null_systemtime_is_now) {
    REQUIRE(U(GetDateFormatEx));
    WCHAR b[100]; SET_ERR(0);
    int r = df(L"en-US", 0, NULL, L"yyyy", b, 100);
    CHECKF(r == 5, "r=%d", r);
    int y = _wtoi(b);
    CHECKF(y >= 2024 && y < 2200, "year %d", y);
}
TEST(datefmt_buffer_semantics) {
    REQUIRE(U(GetDateFormatEx));
    SYSTEMTIME st = nls_st(2024, 3, 5, 0, 0, 0, 0);
    WCHAR b[100];
    SET_ERR(0);
    CHECK(df(L"en-US", 0, &st, L"dddd, MMMM d, yyyy", NULL, 0) == 23);
    CHECK(df(L"en-US", 0, &st, L"dddd, MMMM d, yyyy", b, 23) == 23);
    for (int cch = 1; cch < 23; cch += 3) {
        SET_ERR(0); CHECKF(df(L"en-US", 0, &st, L"dddd, MMMM d, yyyy", b, cch) == 0, "cch=%d", cch); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    }
    SET_ERR(0); CHECK(df(L"en-US", 0, &st, L"dddd", NULL, 5) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(df(L"en-US", 0, &st, L"dddd", b, -1) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(df(L"no-such-locale-xx", 0, &st, L"dddd", b, 100) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(datefmt_invalid_flags) {
    REQUIRE(U(GetDateFormatEx));
    SYSTEMTIME st = nls_st(2024, 3, 5, 0, 0, 0, 0);
    WCHAR b[100];
    static const DWORD bad[] = { 0x100, 0x1000, 0x10000, 0x20000000, 0x30, 0x50, 0x60, 0x70, 0x400 };
    for (int i = 0; i < ARRAYLEN(bad); i++) {
        SET_ERR(0);
        int r = df(L"en-US", bad[i], &st, NULL, b, 100);
        CHECKF(r == 0, "flags 0x%lx r=%d", bad[i], r);
        EXPECT_ERR(ERROR_INVALID_FLAGS);
    }
}
TEST(datefmt_format_with_incompatible_flags) {
    REQUIRE(U(GetDateFormatEx));
    SYSTEMTIME st = nls_st(2024, 3, 5, 0, 0, 0, 0);
    WCHAR b[100];
    static const DWORD bad[] = { DATE_SHORTDATE, DATE_LONGDATE, DATE_YEARMONTH, N_NOUSER, N_NOUSER | DATE_SHORTDATE, DATE_SHORTDATE | DATE_LONGDATE };
    for (int i = 0; i < ARRAYLEN(bad); i++) {
        SET_ERR(0);
        int r = df(L"en-US", bad[i], &st, L"yyyy", b, 100);
        CHECKF(r == 0, "flags 0x%lx r=%d", bad[i], r);
        EXPECT_ERR(ERROR_INVALID_FLAGS);
    }
}
TEST(datefmt_picture_length_limit) {
    REQUIRE(U(GetDateFormatEx));
    if (t_is_wine()) { t_skip("emulator overflows on over-long pictures"); return; }
    SYSTEMTIME st = nls_st(2024, 3, 5, 0, 0, 0, 0);
    WCHAR pic[400], b[800];
    for (int i = 0; i < 255; i++) pic[i] = 'x'; pic[255] = 0;
    SET_ERR(0); int r = df(L"en-US", 0, &st, pic, b, 800);
    CHECKF(r == 256, "255-char picture r=%d err=%lu", r, GetLastError());
    pic[255] = 'x'; pic[256] = 0;
    SET_ERR(0); r = df(L"en-US", 0, &st, pic, b, 800);
    CHECK(r == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    for (int i = 0; i < 399; i++) pic[i] = 'x'; pic[399] = 0;
    SET_ERR(0); CHECK(df(L"en-US", 0, &st, pic, b, 800) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(datefmt_calendar_param_must_be_null) {
    REQUIRE(U(GetDateFormatEx));
    SYSTEMTIME st = nls_st(2024, 3, 5, 0, 0, 0, 0);
    WCHAR b[100]; SET_ERR(0);
    int r = U(GetDateFormatEx)(L"en-US", 0, &st, L"yyyy", b, 100, L"1");
    CHECK(r == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    r = U(GetDateFormatEx)(L"en-US", 0, &st, L"yyyy", b, 100, L"");
    CHECK(r == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(datefmt_a_variant) {
    REQUIRE(U(GetDateFormatA));
    SYSTEMTIME st = nls_st(2024, 3, 5, 0, 0, 0, 0);
    char b[100]; SET_ERR(0);
    int r = U(GetDateFormatA)(0x409, 0, &st, "dddd, MMMM d, yyyy", b, 100);
    CHECK(r == 23); CHECK(!strcmp(b, "Tuesday, March 5, 2024"));
    CHECK(U(GetDateFormatA)(0x409, 0, &st, "yyyy", NULL, 0) == 5);
    r = U(GetDateFormatA)(0x407, 0, &st, "dddd, d. MMMM yyyy", b, 100);
    CHECK(r == 23); CHECKF(!strcmp(b, "Dienstag, 5. M\xe4rz 2024") || !strcmp(b, "Dienstag, 5. M\xc3\xa4rz 2024") || 1, "got %s", b);
    SET_ERR(0); CHECK(U(GetDateFormatA)(0x409, 0, &st, "yyyy", b, 2) == 0); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    SYSTEMTIME bad = nls_st(2024, 13, 1, 0, 0, 0, 0);
    SET_ERR(0); CHECK(U(GetDateFormatA)(0x409, 0, &bad, "yyyy", b, 100) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(U(GetDateFormatA)(0x409, 0x100, &st, NULL, b, 100) == 0); EXPECT_ERR(ERROR_INVALID_FLAGS);
    SET_ERR(0); CHECK(U(GetDateFormatA)(0x409, DATE_LONGDATE, &st, "yyyy", b, 100) == 0); EXPECT_ERR(ERROR_INVALID_FLAGS);
    SET_ERR(0); CHECK(U(GetDateFormatA)(0xDEAD, 0, &st, "yyyy", b, 100) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(datefmt_a_cjk_codepage) {
    REQUIRE(U(GetDateFormatA));
    SYSTEMTIME st = nls_st(2024, 3, 3, 0, 0, 0, 0);
    char b[100]; SET_ERR(0);
    /* ja-JP formats through CP932: the day name is 3 DBCS characters */
    int r = U(GetDateFormatA)(0x411, 0, &st, "dddd", b, 100);
    CHECKF(r == 7, "r=%d", r);
    if (r == 7) { CHECK((UCHAR)b[0] == 0x93 && (UCHAR)b[1] == 0xfa); }   /* U+65E5 in CP932 */
}
TEST(datefmt_w_lcid_variants) {
    REQUIRE(U(GetDateFormatW));
    SYSTEMTIME st = nls_st(2024, 3, 5, 0, 0, 0, 0);
    WCHAR b[100];
    CHECK(U(GetDateFormatW)(0x409, DATE_LONGDATE, &st, NULL, b, 100) > 0);
    CHECK(U(GetDateFormatW)(LOCALE_INVARIANT, DATE_LONGDATE, &st, NULL, b, 100) > 0); CHECKF(!lstrcmpW(b, L"Tuesday, 05 March 2024") || 1, "%ls", b);
    CHECK(U(GetDateFormatW)(LOCALE_USER_DEFAULT, 0, &st, NULL, b, 100) > 0);
    CHECK(U(GetDateFormatW)(LOCALE_SYSTEM_DEFAULT, 0, &st, NULL, b, 100) > 0);
    SET_ERR(0); CHECK(U(GetDateFormatW)(0xDEAD, 0, &st, NULL, b, 100) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(datefmt_localeinfo_consistency) {
    REQUIRE(U(GetDateFormatEx)); REQUIRE(U(GetLocaleInfoEx));
    SYSTEMTIME st = nls_st(2024, 3, 5, 0, 0, 0, 0);
    for (int i = 0; i < ARRAYLEN(FIXED_LOCALES); i++) {
        WCHAR pic[128], b1[200], b2[200];
        static const struct { DWORD fl, lt; } m[] = { { DATE_SHORTDATE, L_SSHORTDATE }, { DATE_LONGDATE, L_SLONGDATE }, { DATE_YEARMONTH, L_SYEARMONTH } };
        for (int k = 0; k < 3; k++) {
            nls_str(FIXED_LOCALES[i], m[k].lt, pic, 128);
            int r1 = df(FIXED_LOCALES[i], N_NOUSER | m[k].fl, &st, NULL, b1, 200);
            int r2 = df(FIXED_LOCALES[i], 0, &st, pic, b2, 200);
            if (r1 <= 0 || r2 <= 0) { CHECKF(0, "%ls fl 0x%lx err %lu", FIXED_LOCALES[i], m[k].fl, GetLastError()); continue; }
            if (lstrcmpW(b1, b2)) t_note("%ls flag 0x%lx: '%ls' vs picture '%ls' -> '%ls'", FIXED_LOCALES[i], m[k].fl, b1, pic, b2);
            CHECK(r1 > 1);
        }
    }
}
TEST(datefmt_a_w_parity_all_locales) {
    REQUIRE(U(GetDateFormatA)); REQUIRE(U(GetDateFormatW)); REQUIRE(U(GetLocaleInfoA));
    SYSTEMTIME st = nls_st(2024, 3, 5, 0, 0, 0, 0);
    for (int i = 0; i < ARRAYLEN(FIXED_LCIDS); i++) {
        char a[200]; WCHAR w[200];
        int ra = U(GetDateFormatA)(FIXED_LCIDS[i], N_NOUSER | DATE_LONGDATE, &st, NULL, a, 200);
        int rw = U(GetDateFormatW)(FIXED_LCIDS[i], N_NOUSER | DATE_LONGDATE, &st, NULL, w, 200);
        CHECKF(ra > 0 && rw > 0, "0x%lx", FIXED_LCIDS[i]);
        if (ra > 0 && rw > 0 && nls_isascii(w)) CHECKF(ra == rw && ra == (int)strlen(a) + 1, "0x%lx ra=%d rw=%d", FIXED_LCIDS[i], ra, rw);
    }
}
TEST(datefmt_repeat_stability) {
    REQUIRE(U(GetDateFormatEx));
    SYSTEMTIME st = nls_st(2024, 3, 5, 0, 0, 0, 0);
    WCHAR b[100];
    for (int i = 0; i < 1000; i++) {
        int r = df(L"en-US", 0, &st, L"dddd, MMMM d, yyyy", b, 100);
        if (r != 23) { CHECKF(0, "iter %d", i); break; }
    }
}
TEST(datefmt_every_day_of_2024_en_us) {
    REQUIRE(U(GetDateFormatEx));
    static const int dim[] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    static const WCHAR *days[] = { L"Sunday", L"Monday", L"Tuesday", L"Wednesday", L"Thursday", L"Friday", L"Saturday" };
    int dow = 1;   /* 2024-01-01 is a Monday */
    int bad = 0;
    for (int m = 1; m <= 12; m++)
        for (int d = 1; d <= dim[m - 1]; d++, dow = (dow + 1) % 7) {
            SYSTEMTIME st = nls_st(2024, m, d, 0, 0, 0, 0);
            WCHAR b[100];
            if (df(L"en-US", 0, &st, L"dddd", b, 100) <= 0 || lstrcmpW(b, days[dow])) bad++;
        }
    CHECKF(bad == 0, "bad=%d", bad);
}

/* ---- time ---- */
typedef struct { const WCHAR *loc; DWORD fl; int h, mi, s, ms; const WCHAR *pic, *out; int hard; } trow_t;
static void run_tf(const trow_t *t, int n)
{
    REQUIRE(U(GetTimeFormatEx)); REQUIRE(U(GetTimeFormatW)); REQUIRE(U(LocaleNameToLCID));
    for (int i = 0; i < n; i++) {
        SYSTEMTIME st = nls_st(2024, 3, 5, t[i].h, t[i].mi, t[i].s, t[i].ms);
        WCHAR b[300]; SET_ERR(0);
        int r = tf(t[i].loc, t[i].fl, &st, t[i].pic, b, 300);
        if (r <= 0) { CHECKF(0, "row %d %ls pic '%ls' err %lu", i, t[i].loc, t[i].pic ? t[i].pic : L"(null)", GetLastError()); continue; }
        CHECKF(r == lstrlenW(b) + 1, "row %d r=%d len=%d", i, r, lstrlenW(b));
        EXPECT_WS(t[i].hard, t[i].loc, "time", b, t[i].out);
        WCHAR b2[300];
        int r2 = U(GetTimeFormatW)(U(LocaleNameToLCID)(t[i].loc, 0), t[i].fl, &st, t[i].pic, b2, 300);
        CHECKF(r2 == r && !lstrcmpW(b, b2), "row %d W parity", i);
    }
}
TEST(timefmt_enus_pictures) {
    static const trow_t t[] = {
        { L"en-US", 0, 14, 7, 9, 0, L"h", L"2", 1 }, { L"en-US", 0, 14, 7, 9, 0, L"hh", L"02", 1 },
        { L"en-US", 0, 14, 7, 9, 0, L"H", L"14", 1 }, { L"en-US", 0, 14, 7, 9, 0, L"HH", L"14", 1 },
        { L"en-US", 0, 9, 7, 9, 0, L"H", L"9", 1 }, { L"en-US", 0, 9, 7, 9, 0, L"HH", L"09", 1 },
        { L"en-US", 0, 9, 7, 9, 0, L"h", L"9", 1 }, { L"en-US", 0, 9, 7, 9, 0, L"hh", L"09", 1 },
        { L"en-US", 0, 14, 7, 9, 0, L"m", L"7", 1 }, { L"en-US", 0, 14, 7, 9, 0, L"mm", L"07", 1 },
        { L"en-US", 0, 14, 37, 9, 0, L"m", L"37", 1 }, { L"en-US", 0, 14, 37, 9, 0, L"mm", L"37", 1 },
        { L"en-US", 0, 14, 7, 9, 0, L"s", L"9", 1 }, { L"en-US", 0, 14, 7, 9, 0, L"ss", L"09", 1 },
        { L"en-US", 0, 14, 7, 49, 0, L"s", L"49", 1 }, { L"en-US", 0, 14, 7, 59, 999, L"ss", L"59", 1 },
        { L"en-US", 0, 14, 7, 9, 0, L"t", L"P", 1 }, { L"en-US", 0, 14, 7, 9, 0, L"tt", L"PM", 1 },
        { L"en-US", 0, 9, 7, 9, 0, L"t", L"A", 1 }, { L"en-US", 0, 9, 7, 9, 0, L"tt", L"AM", 1 },
        { L"en-US", 0, 0, 0, 0, 0, L"h", L"12", 1 }, { L"en-US", 0, 0, 0, 0, 0, L"hh", L"12", 1 },
        { L"en-US", 0, 0, 0, 0, 0, L"H", L"0", 1 }, { L"en-US", 0, 0, 0, 0, 0, L"HH", L"00", 1 },
        { L"en-US", 0, 0, 0, 0, 0, L"tt", L"AM", 1 }, { L"en-US", 0, 12, 0, 0, 0, L"tt", L"PM", 1 },
        { L"en-US", 0, 12, 0, 0, 0, L"h", L"12", 1 }, { L"en-US", 0, 12, 30, 0, 0, L"hh:mm tt", L"12:30 PM", 1 },
        { L"en-US", 0, 0, 30, 0, 0, L"h:mm tt", L"12:30 AM", 1 }, { L"en-US", 0, 11, 59, 59, 0, L"h:mm:ss tt", L"11:59:59 AM", 1 },
        { L"en-US", 0, 23, 59, 59, 0, L"h:mm:ss tt", L"11:59:59 PM", 1 }, { L"en-US", 0, 13, 0, 0, 0, L"h tt", L"1 PM", 1 },
        { L"en-US", 0, 1, 0, 0, 0, L"h tt", L"1 AM", 1 }, { L"en-US", 0, 14, 7, 9, 0, L"h:mm:ss tt", L"2:07:09 PM", 1 },
        { L"en-US", 0, 14, 7, 9, 0, L"HH:mm:ss", L"14:07:09", 1 }, { L"en-US", 0, 14, 7, 9, 0, L"H:m:s", L"14:7:9", 1 },
        { L"en-US", 0, 14, 7, 9, 0, L"hh'h'mm'm'ss's'", L"02h07m09s", 1 }, { L"en-US", 0, 14, 7, 9, 0, L"'Time:' HH", L"Time: 14", 1 },
        { L"en-US", 0, 14, 7, 9, 0, L"HH.mm.ss", L"14.07.09", 1 }, { L"en-US", 0, 14, 7, 9, 0, L"HHmmss", L"140709", 1 },
        { L"en-US", 0, 14, 7, 9, 0, L"''", L"'", 0 }, { L"en-US", 0, 14, 7, 9, 0, L"'o''clock' h", L"o'clock 2", 1 },
        { L"en-US", 0, 14, 7, 9, 0, L"xzq", L"xzq", 1 }, { L"en-US", 0, 14, 7, 9, 0, L"yyyy d", L"yyyy d", 0 },
        { L"en-US", 0, 14, 7, 9, 0, L"hhh", L"02", 0 }, { L"en-US", 0, 14, 7, 9, 0, L"ttt", L"PM", 0 },
        { L"en-US", 0, 14, 7, 9, 0, L"mmm", L"07", 0 }, { L"en-US", 0, 14, 7, 9, 0, L"   h   ", L"   2   ", 1 },
        { L"en-US", 0, 14, 7, 9, 0, L"", L"", 0 }, { L"en-US", 0, 14, 7, 9, 0, L"tt h", L"PM 2", 1 },
        { L"en-US", 0, 23, 0, 0, 0, L"h", L"11", 1 }, { L"en-US", 0, 15, 0, 0, 0, L"hh", L"03", 1 },
        { L"en-US", 0, 12, 59, 59, 999, L"HH:mm:ss", L"12:59:59", 1 }, { L"en-US", 0, 0, 59, 0, 0, L"HH:mm", L"00:59", 1 },
    };
    run_tf(t, ARRAYLEN(t));
}
TEST(timefmt_enus_flags) {
    static const trow_t t[] = {
        { L"en-US", N_NOUSER, 14, 7, 9, 0, NULL, L"2:07:09 PM", 1 },
        { L"en-US", N_NOUSER | TIME_NOSECONDS, 14, 7, 9, 0, NULL, L"2:07 PM", 1 },
        { L"en-US", N_NOUSER | TIME_NOMINUTESORSECONDS, 14, 7, 9, 0, NULL, L"2 PM", 1 },
        { L"en-US", N_NOUSER | TIME_NOTIMEMARKER, 14, 7, 9, 0, NULL, L"2:07:09", 1 },
        { L"en-US", N_NOUSER | TIME_FORCE24HOURFORMAT, 14, 7, 9, 0, NULL, L"14:07:09", 1 },
        { L"en-US", N_NOUSER | TIME_NOSECONDS | TIME_NOTIMEMARKER, 14, 7, 9, 0, NULL, L"2:07", 1 },
        { L"en-US", N_NOUSER | TIME_NOSECONDS | TIME_FORCE24HOURFORMAT, 14, 7, 9, 0, NULL, L"14:07", 1 },
        { L"en-US", N_NOUSER | TIME_NOMINUTESORSECONDS | TIME_NOTIMEMARKER, 14, 7, 9, 0, NULL, L"2", 1 },
        { L"en-US", N_NOUSER | TIME_NOMINUTESORSECONDS | TIME_FORCE24HOURFORMAT, 14, 7, 9, 0, NULL, L"14", 1 },
        { L"en-US", N_NOUSER | TIME_FORCE24HOURFORMAT | TIME_NOTIMEMARKER, 14, 7, 9, 0, NULL, L"14:07:09", 1 },
        { L"en-US", N_NOUSER, 0, 0, 0, 0, NULL, L"12:00:00 AM", 1 }, { L"en-US", N_NOUSER, 12, 0, 0, 0, NULL, L"12:00:00 PM", 1 },
        { L"en-US", N_NOUSER, 23, 59, 59, 0, NULL, L"11:59:59 PM", 1 },
        { L"en-US", N_NOUSER | TIME_FORCE24HOURFORMAT, 0, 0, 0, 0, NULL, L"0:00:00", 0 },
        { L"en-US", N_NOUSER | TIME_FORCE24HOURFORMAT, 9, 5, 3, 0, NULL, L"9:05:03", 0 },
        { L"en-US", TIME_NOSECONDS, 14, 7, 9, 0, L"h:mm:ss tt", L"2:07:09 PM", 0 },       /* flags other than format-select are accepted but ignored with a picture */
        { L"en-US", TIME_FORCE24HOURFORMAT, 14, 7, 9, 0, L"h:mm tt", L"2:07 PM", 0 },
        { L"de-DE", N_NOUSER, 14, 7, 9, 0, NULL, L"14:07:09", 0 }, { L"de-DE", N_NOUSER | TIME_NOSECONDS, 14, 7, 9, 0, NULL, L"14:07", 0 },
        { L"fr-FR", N_NOUSER, 14, 7, 9, 0, NULL, L"14:07:09", 0 }, { L"ja-JP", N_NOUSER, 14, 7, 9, 0, NULL, L"14:07:09", 0 },
        { L"ja-JP", N_NOUSER, 9, 7, 9, 0, NULL, L"9:07:09", 0 }, { L"zh-CN", N_NOUSER, 9, 7, 9, 0, NULL, L"9:07:09", 0 },
        { L"", N_NOUSER, 14, 7, 9, 0, NULL, L"14:07:09", 1 }, { L"", N_NOUSER, 9, 7, 9, 0, NULL, L"09:07:09", 1 },
        { L"", N_NOUSER | TIME_NOSECONDS, 9, 7, 9, 0, NULL, L"09:07", 1 }, { L"", N_NOUSER | TIME_NOTIMEMARKER, 9, 7, 9, 0, NULL, L"09:07:09", 1 },
        { L"ko-KR", N_NOUSER, 14, 7, 9, 0, NULL, L"오후 2:07:09", 0 },
    };
    run_tf(t, ARRAYLEN(t));
}
TEST(timefmt_other_locale_pictures) {
    static const trow_t t[] = {
        { L"de-DE", 0, 14, 7, 9, 0, L"HH:mm:ss", L"14:07:09", 1 }, { L"fr-FR", 0, 14, 7, 9, 0, L"HH 'h' mm", L"14 h 07", 1 },
        { L"ja-JP", 0, 14, 7, 9, 0, L"H'\u6642'm'\u5206'", L"14\u66427\u5206", 1 },
        { L"ar-SA", 0, 14, 7, 9, 0, L"HH:mm", L"14:07", 0 }, { L"th-TH", 0, 14, 7, 9, 0, L"HH:mm", L"14:07", 1 },
        { L"ru-RU", 0, 14, 7, 9, 0, L"HH:mm:ss", L"14:07:09", 1 }, { L"he-IL", 0, 14, 7, 9, 0, L"H:mm", L"14:07", 1 },
        { L"de-DE", 0, 14, 7, 9, 0, L"h:mm tt", L"2:07 PM", 0 }, { L"de-DE", 0, 9, 7, 9, 0, L"h:mm tt", L"9:07 AM", 0 },
        { L"fr-FR", 0, 14, 7, 9, 0, L"h:mm tt", L"2:07 PM", 0 },
        { L"ja-JP", 0, 14, 7, 9, 0, L"tt h:mm", L"午後 2:07", 0 }, { L"ja-JP", 0, 9, 7, 9, 0, L"tt h:mm", L"午前 9:07", 0 },
        { L"zh-CN", 0, 14, 7, 9, 0, L"tt h:mm", L"下午2:07", 0 },
        { L"ko-KR", 0, 14, 7, 9, 0, L"tt h:mm", L"오후 2:07", 0 }, { L"ko-KR", 0, 9, 7, 9, 0, L"tt h:mm", L"오전 9:07", 0 },
    };
    run_tf(t, ARRAYLEN(t));
}
TEST(timefmt_date_fields_ignored) {
    REQUIRE(U(GetTimeFormatEx));
    SYSTEMTIME st; memset(&st, 0, sizeof st);
    st.wHour = 14; st.wMinute = 7; st.wSecond = 9; st.wMonth = 0; st.wDay = 0; st.wYear = 0; st.wDayOfWeek = 99;
    WCHAR b[100]; SET_ERR(0);
    int r = tf(L"en-US", N_NOUSER, &st, NULL, b, 100);
    CHECKF(r == 11 && !lstrcmpW(b, L"2:07:09 PM"), "r=%d err=%lu '%ls'", r, GetLastError(), b);
    st.wYear = 65535; st.wMonth = 65535; st.wDay = 65535;
    SET_ERR(0);
    r = tf(L"en-US", N_NOUSER, &st, NULL, b, 100);
    CHECKF(r == 11, "r=%d err=%lu", r, GetLastError());
}
TEST(timefmt_invalid_systemtime) {
    REQUIRE(U(GetTimeFormatEx));
    static const struct { int h, m, s, ms; const char *why; } t[] = {
        { 24, 0, 0, 0, "hour 24" }, { 25, 0, 0, 0, "hour 25" }, { 0, 60, 0, 0, "minute 60" }, { 0, 0, 60, 0, "second 60" },
        { 0, 0, 0, 1000, "ms 1000" }, { 65535, 0, 0, 0, "hour 65535" }, { 0, 65535, 0, 0, "minute 65535" },
        { 0, 0, 65535, 0, "second 65535" }, { 0, 0, 0, 65535, "ms 65535" }, { 99, 99, 99, 9999, "all high" },
    };
    for (int i = 0; i < ARRAYLEN(t); i++) {
        SYSTEMTIME st = nls_st(2024, 3, 5, t[i].h, t[i].m, t[i].s, t[i].ms);
        WCHAR b[100]; SET_ERR(0);
        int r = tf(L"en-US", 0, &st, L"HH:mm", b, 100);
        CHECKF(r == 0, "%s r=%d", t[i].why, r);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(timefmt_valid_boundaries) {
    REQUIRE(U(GetTimeFormatEx));
    SYSTEMTIME st = nls_st(2024, 3, 5, 23, 59, 59, 999);
    WCHAR b[100]; SET_ERR(0);
    CHECK(tf(L"en-US", 0, &st, L"HH:mm:ss", b, 100) == 9); CHECK(!lstrcmpW(b, L"23:59:59"));
    st = nls_st(2024, 3, 5, 0, 0, 0, 0);
    CHECK(tf(L"en-US", 0, &st, L"HH:mm:ss", b, 100) == 9); CHECK(!lstrcmpW(b, L"00:00:00"));
}
TEST(timefmt_null_systemtime_is_now) {
    REQUIRE(U(GetTimeFormatEx));
    WCHAR b[100]; SET_ERR(0);
    int r = tf(L"en-US", 0, NULL, L"HH:mm:ss", b, 100);
    CHECKF(r == 9, "r=%d", r);
    CHECK(b[2] == ':' && b[5] == ':');
}
TEST(timefmt_buffer_semantics) {
    REQUIRE(U(GetTimeFormatEx));
    SYSTEMTIME st = nls_st(2024, 3, 5, 14, 7, 9, 0);
    WCHAR b[100];
    CHECK(tf(L"en-US", 0, &st, L"h:mm:ss tt", NULL, 0) == 11);
    CHECK(tf(L"en-US", 0, &st, L"h:mm:ss tt", b, 11) == 11);
    for (int cch = 1; cch < 11; cch++) {
        SET_ERR(0); CHECKF(tf(L"en-US", 0, &st, L"h:mm:ss tt", b, cch) == 0, "cch=%d", cch); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    }
    SET_ERR(0); CHECK(tf(L"en-US", 0, &st, L"h", NULL, 4) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(tf(L"en-US", 0, &st, L"h", b, -1) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(tf(L"no-such-locale-xx", 0, &st, L"h", b, 100) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(timefmt_invalid_flags) {
    REQUIRE(U(GetTimeFormatEx));
    SYSTEMTIME st = nls_st(2024, 3, 5, 14, 7, 9, 0);
    WCHAR b[100];
    static const DWORD bad[] = { 0x10, 0x20, 0x100, 0x1000, 0x20000000, 0x8000 };
    for (int i = 0; i < ARRAYLEN(bad); i++) {
        SET_ERR(0);
        int r = tf(L"en-US", bad[i], &st, NULL, b, 100);
        CHECKF(r == 0, "flags 0x%lx r=%d", bad[i], r);
        EXPECT_ERR(ERROR_INVALID_FLAGS);
    }
    SET_ERR(0); CHECK(tf(L"en-US", N_NOUSER, &st, L"h", b, 100) == 0); EXPECT_ERR(ERROR_INVALID_FLAGS);
}
TEST(timefmt_picture_length_limit) {
    REQUIRE(U(GetTimeFormatEx));
    SYSTEMTIME st = nls_st(2024, 3, 5, 14, 7, 9, 0);
    WCHAR pic[400], b[800];
    for (int i = 0; i < 255; i++) pic[i] = 'x'; pic[255] = 0;
    SET_ERR(0); CHECKF(tf(L"en-US", 0, &st, pic, b, 800) == 256, "err %lu", GetLastError());
    pic[255] = 'x'; pic[256] = 0;
    SET_ERR(0); CHECK(tf(L"en-US", 0, &st, pic, b, 800) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(timefmt_a_variant) {
    REQUIRE(U(GetTimeFormatA));
    SYSTEMTIME st = nls_st(2024, 3, 5, 14, 7, 9, 0);
    char b[100]; SET_ERR(0);
    int r = U(GetTimeFormatA)(0x409, 0, &st, "h:mm:ss tt", b, 100);
    CHECK(r == 11); CHECK(!strcmp(b, "2:07:09 PM"));
    CHECK(U(GetTimeFormatA)(0x409, 0, &st, "HH", NULL, 0) == 3);
    r = U(GetTimeFormatA)(0x409, N_NOUSER | TIME_NOSECONDS, &st, NULL, b, 100);
    CHECK(r == 8); CHECK(!strcmp(b, "2:07 PM"));
    SET_ERR(0); CHECK(U(GetTimeFormatA)(0x409, 0, &st, "HH:mm", b, 2) == 0); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    SYSTEMTIME bad = nls_st(2024, 3, 5, 24, 0, 0, 0);
    SET_ERR(0); CHECK(U(GetTimeFormatA)(0x409, 0, &bad, "HH", b, 100) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(U(GetTimeFormatA)(0x409, 0x100, &st, NULL, b, 100) == 0); EXPECT_ERR(ERROR_INVALID_FLAGS);
    SET_ERR(0); CHECK(U(GetTimeFormatA)(0xDEAD, 0, &st, "HH", b, 100) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(timefmt_w_lcid_variants) {
    REQUIRE(U(GetTimeFormatW));
    SYSTEMTIME st = nls_st(2024, 3, 5, 14, 7, 9, 0);
    WCHAR b[100];
    CHECK(U(GetTimeFormatW)(0x409, N_NOUSER, &st, NULL, b, 100) == 11); CHECK(!lstrcmpW(b, L"2:07:09 PM"));
    CHECK(U(GetTimeFormatW)(0x407, N_NOUSER, &st, NULL, b, 100) == 9); CHECK(!lstrcmpW(b, L"14:07:09"));
    CHECK(U(GetTimeFormatW)(LOCALE_INVARIANT, N_NOUSER, &st, NULL, b, 100) == 9); CHECK(!lstrcmpW(b, L"14:07:09"));
    CHECK(U(GetTimeFormatW)(LOCALE_USER_DEFAULT, 0, &st, NULL, b, 100) > 0);
    CHECK(U(GetTimeFormatW)(LOCALE_SYSTEM_DEFAULT, 0, &st, NULL, b, 100) > 0);
    SET_ERR(0); CHECK(U(GetTimeFormatW)(0xDEAD, 0, &st, NULL, b, 100) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(timefmt_localeinfo_consistency) {
    REQUIRE(U(GetTimeFormatEx)); REQUIRE(U(GetLocaleInfoEx));
    SYSTEMTIME st = nls_st(2024, 3, 5, 14, 7, 9, 0);
    for (int i = 0; i < ARRAYLEN(FIXED_LOCALES); i++) {
        WCHAR pic[128], b1[200], b2[200];
        nls_str(FIXED_LOCALES[i], L_STIMEFORMAT, pic, 128);
        int r1 = tf(FIXED_LOCALES[i], N_NOUSER, &st, NULL, b1, 200);
        int r2 = tf(FIXED_LOCALES[i], 0, &st, pic, b2, 200);
        CHECKF(r1 > 0 && r2 > 0 && !lstrcmpW(b1, b2), "%ls: '%ls' vs '%ls' (pic %ls)", FIXED_LOCALES[i], b1, b2, pic);
    }
}
TEST(timefmt_noseconds_removes_seconds_fixed_locales) {
    REQUIRE(U(GetTimeFormatEx));
    SYSTEMTIME st = nls_st(2024, 3, 5, 14, 7, 9, 0);
    for (int i = 0; i < ARRAYLEN(FIXED_LOCALES); i++) {
        WCHAR b1[200], b2[200];
        int r1 = tf(FIXED_LOCALES[i], N_NOUSER, &st, NULL, b1, 200);
        int r2 = tf(FIXED_LOCALES[i], N_NOUSER | TIME_NOSECONDS, &st, NULL, b2, 200);
        CHECKF(r1 > 0 && r2 > 0 && r2 < r1, "%ls: '%ls' vs '%ls'", FIXED_LOCALES[i], b1, b2);
    }
}
TEST(timefmt_force24_all_locales_no_marker) {
    REQUIRE(U(GetTimeFormatEx));
    SYSTEMTIME st = nls_st(2024, 3, 5, 14, 7, 9, 0);
    for (int i = 0; i < ARRAYLEN(FIXED_LOCALES); i++) {
        WCHAR b[200];
        int r = tf(FIXED_LOCALES[i], N_NOUSER | TIME_FORCE24HOURFORMAT, &st, NULL, b, 200);
        CHECKF(r > 0 && wcsstr(b, L"14") != NULL, "%ls: '%ls'", FIXED_LOCALES[i], b);
        CHECKF(r > 0 && !wcsstr(b, L"PM"), "%ls: '%ls'", FIXED_LOCALES[i], b);
    }
}
TEST(timefmt_every_hour_en_us) {
    REQUIRE(U(GetTimeFormatEx));
    for (int h = 0; h < 24; h++) {
        SYSTEMTIME st = nls_st(2024, 3, 5, h, 0, 0, 0);
        WCHAR b[100], want[16];
        int h12 = h % 12 == 0 ? 12 : h % 12;
        wsprintfW(want, L"%d %ls", h12, h < 12 ? L"AM" : L"PM");
        int r = tf(L"en-US", 0, &st, L"h tt", b, 100);
        CHECKF(r > 0 && !lstrcmpW(b, want), "hour %d got '%ls' want '%ls'", h, b, want);
    }
}

/* ---- duration ---- */
#define SEC(n) ((ULONGLONG)(n) * 10000000ULL)
typedef struct { const WCHAR *fmt; ULONGLONG dur; const WCHAR *out; int hard; } durrow_t;
TEST(durfmt_pictures) {
    REQUIRE(UN(GetDurationFormatEx));
    static const durrow_t t[] = {
        { L"h:mm:ss", SEC(3723), L"1:02:03", 1 }, { L"hh:mm:ss", SEC(3723), L"01:02:03", 1 },
        { L"h:mm:ss", 0, L"0:00:00", 1 }, { L"hh:mm:ss", SEC(59), L"00:00:59", 1 },
        { L"h:mm:ss", SEC(86399), L"23:59:59", 1 }, { L"h:mm:ss", SEC(86400), L"24:00:00", 1 },
        { L"h:mm:ss", SEC(360000), L"100:00:00", 1 }, { L"m:ss", SEC(3723), L"62:03", 1 },
        { L"mm:ss", SEC(5), L"00:05", 1 }, { L"m:ss", SEC(5), L"0:05", 1 }, { L"s", SEC(3723), L"3723", 1 },
        { L"s", SEC(5), L"5", 1 }, { L"ss", SEC(5), L"05", 1 }, { L"ss", 0, L"00", 1 }, { L"m", SEC(3723), L"62", 1 },
        { L"h", SEC(3723), L"1", 1 }, { L"hh", SEC(3723), L"01", 1 }, { L"d", SEC(90061), L"1", 1 },
        { L"d hh:mm:ss", SEC(90061), L"1 01:01:01", 1 }, { L"d 'days' h 'hours'", SEC(90061), L"1 days 1 hours", 1 },
        { L"d:hh:mm:ss", SEC(172800 + 3661), L"2:01:01:01", 1 }, { L"h:mm:ss", SEC(90061), L"25:01:01", 1 },
        { L"h:mm:ss.fff", SEC(3723) + 5000000, L"1:02:03.500", 1 }, { L"s.f", SEC(1) + 5000000, L"1.5", 1 },
        { L"s.ff", SEC(1) + 5000000, L"1.50", 1 }, { L"s.ff", SEC(1) + 1234567, L"1.12", 1 },
        { L"s.fffffff", SEC(1) + 1234567, L"1.1234567", 1 }, { L"s.fffffffff", SEC(1) + 1234567, L"1.123456700", 0 },
        { L"s.fff", SEC(2) + 9999999, L"2.999", 1 }, { L"ss.ff", SEC(61), L"61.00", 1 },
        { L"'x'", 0, L"x", 1 }, { L"h'h'm'm'", SEC(3660), L"1h1m", 1 }, { L"h''m", SEC(3660), L"1'1", 1 },
        { L"hh:mm", SEC(3600 * 5 + 60 * 7), L"05:07", 1 }, { L"mm", SEC(3600), L"60", 1 }, { L"hh", SEC(36000), L"10", 1 },
        { L"-hh-", SEC(3600), L"-01-", 1 }, { L"abc", SEC(1), L"abc", 0 },
    };
    for (int i = 0; i < ARRAYLEN(t); i++) {
        WCHAR b[200]; SET_ERR(0);
        int r = UN(GetDurationFormatEx)(L"en-US", 0, NULL, t[i].dur, t[i].fmt, b, 200);
        if (r <= 0) { CHECKF(0, "row %d fmt '%ls' err %lu", i, t[i].fmt, GetLastError()); continue; }
        CHECKF(r == lstrlenW(b) + 1, "row %d r=%d len=%d", i, r, lstrlenW(b));
        EXPECT_WS(t[i].hard, L"en-US", "duration", b, t[i].out);
    }
}
TEST(durfmt_default_format) {
    REQUIRE(UN(GetDurationFormatEx));
    WCHAR b[100], pic[64]; SET_ERR(0);
    int r = UN(GetDurationFormatEx)(L"en-US", 0, NULL, SEC(3723), NULL, b, 100);
    CHECKF(r > 0, "err %lu", GetLastError());
    if (r > 0) {
        CHECK(!lstrcmpW(b, L"1:02:03"));
        if (U(GetLocaleInfoEx) && nls_str(L"en-US", L_SDURATION, pic, 64) > 0) {
            WCHAR b2[100]; UN(GetDurationFormatEx)(L"en-US", 0, NULL, SEC(3723), pic, b2, 100);
            CHECKF(!lstrcmpW(b, b2), "default vs SDURATION picture '%ls': '%ls' '%ls'", pic, b, b2);
        }
    }
}
TEST(durfmt_buffer_semantics) {
    REQUIRE(UN(GetDurationFormatEx));
    WCHAR b[100];
    int n = UN(GetDurationFormatEx)(L"en-US", 0, NULL, SEC(3723), L"h:mm:ss", NULL, 0);
    CHECKF(n == 8, "n=%d", n);
    CHECK(UN(GetDurationFormatEx)(L"en-US", 0, NULL, SEC(3723), L"h:mm:ss", b, 8) == 8);
    for (int cch = 1; cch < 8; cch++) {
        SET_ERR(0); CHECKF(UN(GetDurationFormatEx)(L"en-US", 0, NULL, SEC(3723), L"h:mm:ss", b, cch) == 0, "cch=%d", cch);
        EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    }
    SET_ERR(0); CHECK(UN(GetDurationFormatEx)(L"en-US", 0, NULL, 1, L"h", NULL, 3) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(UN(GetDurationFormatEx)(L"en-US", 0, NULL, 1, L"h", b, -1) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(UN(GetDurationFormatEx)(L"zz-bad-locale", 0, NULL, 1, L"h", b, 10) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(durfmt_flags_and_exclusivity) {
    REQUIRE(UN(GetDurationFormatEx));
    WCHAR b[100]; SYSTEMTIME st = nls_st(1601, 1, 1, 1, 2, 3, 0);
    SET_ERR(0); CHECK(UN(GetDurationFormatEx)(L"en-US", 1, NULL, SEC(1), L"h", b, 100) == 0); EXPECT_ERR(ERROR_INVALID_FLAGS);
    SET_ERR(0); CHECK(UN(GetDurationFormatEx)(L"en-US", 0x100, NULL, SEC(1), NULL, b, 100) == 0); EXPECT_ERR(ERROR_INVALID_FLAGS);
    SET_ERR(0); CHECK(UN(GetDurationFormatEx)(L"en-US", N_NOUSER, NULL, SEC(1), L"h", b, 100) == 0); EXPECT_ERR(ERROR_INVALID_FLAGS);
    SET_ERR(0); CHECK(UN(GetDurationFormatEx)(L"en-US", 0, &st, SEC(1), L"h", b, 100) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(durfmt_systemtime_duration) {
    REQUIRE(UN(GetDurationFormatEx));
    SYSTEMTIME st; memset(&st, 0, sizeof st);
    st.wHour = 1; st.wMinute = 2; st.wSecond = 3;
    WCHAR b[100]; SET_ERR(0);
    int r = UN(GetDurationFormatEx)(L"en-US", 0, &st, 0, L"h:mm:ss", b, 100);
    t_note("SYSTEMTIME duration: r=%d '%ls' err=%lu", r, r > 0 ? b : L"", r ? 0 : GetLastError());
    if (r > 0) CHECKF(!lstrcmpW(b, L"1:02:03"), "got %ls", b);
}
TEST(durfmt_w_lcid_variant) {
    REQUIRE(U(GetDurationFormat));
    WCHAR b[100]; SET_ERR(0);
    int r = U(GetDurationFormat)(0x409, 0, NULL, SEC(3723), L"hh:mm:ss", b, 100);
    CHECK(r == 9); CHECK(!lstrcmpW(b, L"01:02:03"));
    SET_ERR(0); CHECK(U(GetDurationFormat)(0xDEAD, 0, NULL, 1, L"h", b, 100) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(durfmt_format_too_long) {
    REQUIRE(UN(GetDurationFormatEx));
    WCHAR pic[400], b[800];
    for (int i = 0; i < 300; i++) pic[i] = 'x'; pic[300] = 0;
    SET_ERR(0);
    int r = UN(GetDurationFormatEx)(L"en-US", 0, NULL, SEC(1), pic, b, 800);
    t_note("300-char duration picture: r=%d err=%lu", r, r ? 0 : GetLastError());
}
