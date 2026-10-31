/* Shared static helpers for the locale_nls area. */
#ifndef NLSUTIL_H
#define NLSUTIL_H
#include "t.h"

#ifndef ARRAYLEN
#define ARRAYLEN(a) ((int)(sizeof(a) / sizeof((a)[0])))
#endif

#define N_NOUSER   0x80000000u   /* LOCALE_NOUSEROVERRIDE */
#define N_RETNUM   0x20000000u   /* LOCALE_RETURN_NUMBER */
#define N_USECP    0x40000000u   /* LOCALE_USE_CP_ACP */

/* LCTYPE values (numeric so the area does not depend on header coverage). */
enum {
    L_ILANGUAGE = 0x01, L_SLANGUAGE = 0x02, L_SABBREVLANGNAME = 0x03, L_SNATIVELANGNAME = 0x04,
    L_ICOUNTRY = 0x05, L_SCOUNTRY = 0x06, L_SABBREVCTRYNAME = 0x07, L_SNATIVECTRYNAME = 0x08,
    L_IDEFAULTLANGUAGE = 0x09, L_IDEFAULTCOUNTRY = 0x0A, L_IDEFAULTCODEPAGE = 0x0B,
    L_SLIST = 0x0C, L_IMEASURE = 0x0D, L_SDECIMAL = 0x0E, L_STHOUSAND = 0x0F, L_SGROUPING = 0x10,
    L_IDIGITS = 0x11, L_ILZERO = 0x12, L_SNATIVEDIGITS = 0x13, L_SCURRENCY = 0x14, L_SINTLSYMBOL = 0x15,
    L_SMONDECIMALSEP = 0x16, L_SMONTHOUSANDSEP = 0x17, L_SMONGROUPING = 0x18, L_ICURRDIGITS = 0x19,
    L_IINTLCURRDIGITS = 0x1A, L_ICURRENCY = 0x1B, L_INEGCURR = 0x1C, L_SDATE = 0x1D, L_STIME = 0x1E,
    L_SSHORTDATE = 0x1F, L_SLONGDATE = 0x20, L_IDATE = 0x21, L_ILDATE = 0x22, L_ITIME = 0x23,
    L_ICENTURY = 0x24, L_ITLZERO = 0x25, L_IDAYLZERO = 0x26, L_IMONLZERO = 0x27, L_S1159 = 0x28,
    L_S2359 = 0x29, L_SDAYNAME1 = 0x2A, L_SABBREVDAYNAME1 = 0x31, L_SMONTHNAME1 = 0x38,
    L_SABBREVMONTHNAME1 = 0x44, L_SPOSITIVESIGN = 0x50, L_SNEGATIVESIGN = 0x51, L_IPOSSIGNPOSN = 0x52,
    L_INEGSIGNPOSN = 0x53, L_IPOSSYMPRECEDES = 0x54, L_IPOSSEPBYSPACE = 0x55, L_INEGSYMPRECEDES = 0x56,
    L_INEGSEPBYSPACE = 0x57, L_FONTSIGNATURE = 0x58, L_SISO639LANGNAME = 0x59, L_SISO3166CTRYNAME = 0x5A,
    L_IGEOID = 0x5B, L_SNAME = 0x5C, L_SDURATION = 0x5D, L_SKEYBOARDSTOINSTALL = 0x5E,
    L_SSHORTESTDAYNAME1 = 0x60, L_SISO639LANGNAME2 = 0x67, L_SISO3166CTRYNAME2 = 0x68, L_SNAN = 0x69,
    L_SPOSINFINITY = 0x6A, L_SNEGINFINITY = 0x6B, L_SSCRIPTS = 0x6C, L_SPARENT = 0x6D,
    L_SCONSOLEFALLBACKNAME = 0x6E, L_SLOCALIZEDLANGNAME = 0x6F, L_IREADINGLAYOUT = 0x70, L_INEUTRAL = 0x71,
    L_SENGDISPLAYNAME = 0x72, L_SNATDISPLAYNAME = 0x73, L_SSHORTTIME = 0x79, L_SOPENTYPELANGTAG = 0x7A,
    L_SSORTLOCALE = 0x7B, L_SRELATIVELONGDATE = 0x7C, L_ICONSTRUCTEDLOCALE = 0x7D,
    L_SSHORTESTAM = 0x7E, L_SSHORTESTPM = 0x7F,
    L_SENGLANGNAME = 0x1001, L_SENGCOUNTRY = 0x1002, L_STIMEFORMAT = 0x1003, L_IDEFAULTANSICP = 0x1004,
    L_ITIMEMARKPOSN = 0x1005, L_SYEARMONTH = 0x1006, L_SENGCURRNAME = 0x1007, L_SNATIVECURRNAME = 0x1008,
    L_ICALENDARTYPE = 0x1009, L_IPAPERSIZE = 0x100A, L_IOPTIONALCALENDAR = 0x100B,
    L_IFIRSTDAYOFWEEK = 0x100C, L_IFIRSTWEEKOFYEAR = 0x100D, L_SMONTHNAME13 = 0x100E,
    L_SABBREVMONTHNAME13 = 0x100F, L_INEGNUMBER = 0x1010, L_IDEFAULTMACCP = 0x1011,
    L_IDEFAULTEBCDICCP = 0x1012, L_SSORTNAME = 0x1013, L_IDIGITSUBSTITUTION = 0x1014
};

/* Every string/number LCTYPE we exercise generically (FONTSIGNATURE excluded). */
static const DWORD ALL_LCTYPES[] = {
    1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,
    0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,
    0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,
    0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x59,0x5A,0x5B,0x5C,0x5D,0x5E,
    0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,0x70,0x71,0x72,0x73,
    0x1001,0x1002,0x1003,0x1004,0x1005,0x1006,0x1007,0x1008,0x1009,0x100A,0x100B,0x100C,0x100D,0x100E,0x100F,
    0x1010,0x1011,0x1012,0x1013,0x1014
};

/* The fixed set of locales used for deterministic tests. */
static const WCHAR *const FIXED_LOCALES[] = {
    L"en-US", L"de-DE", L"fr-FR", L"ja-JP", L"zh-CN", L"ar-SA", L"tr-TR",
    L"he-IL", L"th-TH", L"hi-IN", L"ko-KR", L"ru-RU", L"pt-BR"
};
static const DWORD FIXED_LCIDS[] = {
    0x0409, 0x0407, 0x040C, 0x0411, 0x0804, 0x0401, 0x041F, 0x040D, 0x041E, 0x0439, 0x0412, 0x0419, 0x0416
};

typedef BOOL (CALLBACK *K_GEONAMEPROC)(PWSTR, LPARAM);
UDECL(BOOL, EnumSystemGeoNames, (GEOCLASS, K_GEONAMEPROC, LPARAM));
static int nls_isascii(const WCHAR *s) { for (; *s; s++) if (*s > 0x7f) return 0; return 1; }

/* GetLocaleInfoEx wrapper. Returns the API result; out is always NUL-terminated on failure. */
static int nls_gli(const WCHAR *loc, DWORD type, WCHAR *out, int cch)
{
    int r = U(GetLocaleInfoEx)(loc, type, out, cch);
    if (r <= 0 && out && cch > 0) out[0] = 0;
    return r;
}
/* Fetch a string for loc/type (always with NOUSEROVERRIDE). Returns length incl. NUL or 0. */
static int nls_str(const WCHAR *loc, DWORD type, WCHAR *out, int cch)
{
    return nls_gli(loc, type | N_NOUSER, out, cch);
}
static DWORD nls_num(const WCHAR *loc, DWORD type, int *ok)
{
    DWORD v = 0xDEADBEEF;
    int r = U(GetLocaleInfoEx)(loc, type | N_NOUSER | N_RETNUM, (WCHAR *)&v, 2);
    if (ok) *ok = (r == 2);
    return v;
}
static void nls_dump(const WCHAR *w, char *out, size_t cap)
{
    size_t n = 0;
    for (; *w && n + 8 < cap; w++) {
        if (*w < 0x80 && *w >= 0x20) out[n++] = (char)*w;
        else n += (size_t)snprintf(out + n, cap - n, "\\u%04x", *w);
    }
    out[n] = 0;
}
/* Compare wide strings; on mismatch fail (hard) or note (soft). */
static void nls_expect(int hard, const char *file, int line, const WCHAR *loc, const char *what, const WCHAR *got, const WCHAR *want)
{
    if (!lstrcmpW(got, want)) { t_check(1, file, line, what, NULL); return; }
    char g[256], w[256], l[64];
    nls_dump(got, g, sizeof g); nls_dump(want, w, sizeof w);
    WideCharToMultiByte(CP_ACP, 0, loc, -1, l, sizeof l, NULL, NULL);
    if (hard) t_check(0, file, line, what, "%s: got \"%s\" want \"%s\"", l, g, w);
    else t_note("%s %s: got \"%s\" expected \"%s\"", l, what, g, w);
}
#define EXPECT_WS(hard, loc, what, got, want) nls_expect(hard, __FILE__, __LINE__, loc, what, got, want)

static BOOL nls_have(void) { return U(GetLocaleInfoEx) != NULL; }

/* Wide <-> narrow helpers */
static void nls_w2a(const WCHAR *w, char *a, int cap) { WideCharToMultiByte(CP_ACP, 0, w, -1, a, cap, NULL, NULL); }
static void nls_a2w(const char *a, WCHAR *w, int cap) { MultiByteToWideChar(CP_ACP, 0, a, -1, w, cap); }

/* SYSTEMTIME builder */
static SYSTEMTIME nls_st(int y, int mo, int d, int h, int mi, int s, int ms)
{
    SYSTEMTIME st; memset(&st, 0, sizeof st);
    st.wYear = (WORD)y; st.wMonth = (WORD)mo; st.wDay = (WORD)d;
    st.wHour = (WORD)h; st.wMinute = (WORD)mi; st.wSecond = (WORD)s; st.wMilliseconds = (WORD)ms;
    /* day of week via days-from-civil */
    int yy = y - (mo <= 2), era = (yy >= 0 ? yy : yy - 399) / 400, yoe = yy - era * 400;
    int doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + d - 1, doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    long days = (long)era * 146097 + doe - 719468;
    st.wDayOfWeek = (WORD)(((days % 7) + 11) % 7);   /* 1970-01-01 was Thursday (4) */
    return st;
}
static int nls_cmp_st(const SYSTEMTIME *a, const SYSTEMTIME *b)
{
    return a->wYear == b->wYear && a->wMonth == b->wMonth && a->wDay == b->wDay && a->wHour == b->wHour &&
           a->wMinute == b->wMinute && a->wSecond == b->wSecond && a->wMilliseconds == b->wMilliseconds;
}
#endif
