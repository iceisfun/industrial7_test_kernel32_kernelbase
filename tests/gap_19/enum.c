#include "t.h"
#undef RT_VERSION
#undef RT_RCDATA
#undef RT_MANIFEST
#undef RT_STRING
#undef RT_MESSAGETABLE
#undef RT_ICON
#undef RT_DIALOG
#define RT_VERSION MAKEINTRESOURCEW(16)
#define RT_RCDATA MAKEINTRESOURCEW(10)
#define RT_MANIFEST MAKEINTRESOURCEW(24)
#define RT_STRING MAKEINTRESOURCEW(6)
#define RT_MESSAGETABLE MAKEINTRESOURCEW(11)
#define RT_ICON MAKEINTRESOURCEW(3)
#define RT_DIALOG MAKEINTRESOURCEW(5)
/* gap19: EnumResourceNamesExW, EnumResourceLanguagesExW, EnumCalendarInfoExA,
 * EnumDateFormatsExW, GetStringTypeExA. */

typedef BOOL (CALLBACK *g19_namecb)(HMODULE, LPCWSTR, LPWSTR, LONG_PTR);
typedef BOOL (CALLBACK *g19_langcb)(HMODULE, LPCWSTR, LPCWSTR, WORD, LONG_PTR);
UDECL(BOOL, EnumResourceNamesExW, (HMODULE, LPCWSTR, g19_namecb, LONG_PTR, DWORD, LANGID));
UDECL(BOOL, EnumResourceLanguagesExW, (HMODULE, LPCWSTR, LPCWSTR, g19_langcb, LONG_PTR, DWORD, LANGID));
typedef BOOL (CALLBACK *g19_calcb)(LPSTR, CALID);
typedef BOOL (CALLBACK *g19_datecb)(LPWSTR, CALID);
UDECL(BOOL, EnumCalendarInfoExA, (g19_calcb, LCID, CALID, CALTYPE));
UDECL(BOOL, EnumDateFormatsExW, (g19_datecb, LCID, DWORD));
UDECL(BOOL, GetStringTypeExA, (LCID, DWORD, LPCSTR, int, LPWORD));

static HMODULE g19_k32(void) { return GetModuleHandleW(L"kernel32.dll"); }

struct g19_res { int count; BOOL stop_after_first; HMODULE hmod; LPCWSTR type; LPWSTR first; int hmod_ok, type_ok; };
static BOOL CALLBACK g19_name(HMODULE h, LPCWSTR type, LPWSTR name, LONG_PTR lp) {
    struct g19_res *r = (struct g19_res *)lp;
    if (r->count == 0) r->first = name;
    r->count++;
    if (h == r->hmod) r->hmod_ok++;
    if (type == r->type) r->type_ok++;
    return r->stop_after_first ? FALSE : TRUE;
}
struct g19_lang { int count; WORD first; WORD all[16]; BOOL stop; };
static BOOL CALLBACK g19_langf(HMODULE h, LPCWSTR t, LPCWSTR n, WORD lang, LONG_PTR lp) {
    struct g19_lang *r = (struct g19_lang *)lp;
    if (r->count < 16) r->all[r->count] = lang;
    if (r->count == 0) r->first = lang;
    r->count++;
    return r->stop ? FALSE : TRUE;
}

TEST(gap19_enumresnames_kernel32_version) {
    REQUIRE(UN(EnumResourceNamesExW));
    struct g19_res r = { .hmod = g19_k32(), .type = RT_VERSION };
    SET_ERR(0);
    BOOL ok = UN(EnumResourceNamesExW)(g19_k32(), RT_VERSION, g19_name, (LONG_PTR)&r, 0, 0);
    EXPECT_EQ(ok, TRUE);
    CHECK(r.count >= 1);
    EXPECT_EQ(r.hmod_ok, r.count);
    EXPECT_EQ(r.type_ok, r.count);
    CHECK(IS_INTRESOURCE(r.first));
    EXPECT_EQ((ULONG_PTR)r.first, 1);   /* VS_VERSION_INFO is resource 1 */
}
TEST(gap19_enumresnames_stop_early) {
    REQUIRE(UN(EnumResourceNamesExW));
    struct g19_res r = { .hmod = g19_k32(), .type = RT_VERSION, .stop_after_first = TRUE };
    SET_ERR(0x1234);
    BOOL ok = UN(EnumResourceNamesExW)(g19_k32(), RT_VERSION, g19_name, (LONG_PTR)&r, 0, 0);
    EXPECT_EQ(r.count, 1);
    t_note("callback FALSE -> ret %d err %lu", ok, (unsigned long)GetLastError());
}
TEST(gap19_enumresnames_no_resources_in_exe) {
    REQUIRE(UN(EnumResourceNamesExW));
    struct g19_res r = { 0 };
    SET_ERR(0);
    BOOL ok = UN(EnumResourceNamesExW)(GetModuleHandleW(NULL), RT_RCDATA, g19_name, (LONG_PTR)&r, 0, 0);
    DWORD e = GetLastError();
    EXPECT_EQ(ok, FALSE);
    EXPECT_EQ(r.count, 0);
    t_note("no rsrc dir / type -> err %lu (1812 data, 1813 type)", (unsigned long)e);
    CHECK(e == ERROR_RESOURCE_DATA_NOT_FOUND || e == ERROR_RESOURCE_TYPE_NOT_FOUND || e != 0);
}
TEST(gap19_enumresnames_type_not_found) {
    REQUIRE(UN(EnumResourceNamesExW));
    struct g19_res r = { 0 };
    SET_ERR(0);
    BOOL ok = UN(EnumResourceNamesExW)(g19_k32(), MAKEINTRESOURCEW(0x7771), g19_name, (LONG_PTR)&r, 0, 0);
    EXPECT_EQ(ok, FALSE);
    EXPECT_EQ(r.count, 0);
    EXPECT_ERR(ERROR_RESOURCE_TYPE_NOT_FOUND);
}
TEST(gap19_enumresnames_string_type_not_found) {
    REQUIRE(UN(EnumResourceNamesExW));
    struct g19_res r = { 0 };
    SET_ERR(0);
    BOOL ok = UN(EnumResourceNamesExW)(g19_k32(), L"K32T_NOSUCHTYPE", g19_name, (LONG_PTR)&r, 0, 0);
    EXPECT_EQ(ok, FALSE);
    EXPECT_EQ(r.count, 0);
    EXPECT_ERR(ERROR_RESOURCE_TYPE_NOT_FOUND);
}
TEST(gap19_enumresnames_datafile_module) {
    REQUIRE(UN(EnumResourceNamesExW));
    WCHAR p[MAX_PATH]; GetSystemDirectoryW(p, MAX_PATH); lstrcatW(p, L"\\kernel32.dll");
    HMODULE d = LoadLibraryExW(p, NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (!d) { t_skip("datafile load failed"); return; }
    struct g19_res a = { .hmod = d, .type = RT_VERSION }, b = { .hmod = g19_k32(), .type = RT_VERSION };
    EXPECT_EQ(UN(EnumResourceNamesExW)(d, RT_VERSION, g19_name, (LONG_PTR)&a, 0, 0), TRUE);
    EXPECT_EQ(UN(EnumResourceNamesExW)(g19_k32(), RT_VERSION, g19_name, (LONG_PTR)&b, 0, 0), TRUE);
    EXPECT_EQ(a.count, b.count);
    EXPECT_EQ(a.hmod_ok, a.count);
    FreeLibrary(d);
}
TEST(gap19_enumresnames_image_resource_module) {
    REQUIRE(UN(EnumResourceNamesExW));
    WCHAR p[MAX_PATH]; GetSystemDirectoryW(p, MAX_PATH); lstrcatW(p, L"\\kernel32.dll");
    HMODULE d = LoadLibraryExW(p, NULL, LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    if (!d) { t_skip("image-resource load failed"); return; }
    struct g19_res a = { .hmod = d, .type = RT_VERSION };
    EXPECT_EQ(UN(EnumResourceNamesExW)(d, RT_VERSION, g19_name, (LONG_PTR)&a, 0, 0), TRUE);
    CHECK(a.count >= 1);
    FreeLibrary(d);
}
TEST(gap19_enumresnames_flags_ln_mui) {
    REQUIRE(UN(EnumResourceNamesExW));
    DWORD flags[] = { 0, RESOURCE_ENUM_LN, RESOURCE_ENUM_MUI, RESOURCE_ENUM_LN | RESOURCE_ENUM_MUI,
                      RESOURCE_ENUM_MUI_SYSTEM, RESOURCE_ENUM_VALIDATE };
    for (unsigned i = 0; i < 6; i++) {
        struct g19_res r = { .hmod = g19_k32(), .type = RT_VERSION };
        SET_ERR(0);
        BOOL ok = UN(EnumResourceNamesExW)(g19_k32(), RT_VERSION, g19_name, (LONG_PTR)&r, flags[i], 0);
        t_note("flags 0x%lx -> %d count %d err %lu", (unsigned long)flags[i], ok, r.count, (unsigned long)GetLastError());
        if (flags[i] == RESOURCE_ENUM_LN) { EXPECT_EQ(ok, TRUE); CHECK(r.count >= 1); }
    }
}
TEST(gap19_enumresnames_invalid_flags) {
    REQUIRE(UN(EnumResourceNamesExW));
    DWORD flags[] = { 0x80000000, 0xffffffff, 0x100 };
    for (unsigned i = 0; i < 3; i++) {
        struct g19_res r = { 0 };
        SET_ERR(0);
        BOOL ok = UN(EnumResourceNamesExW)(g19_k32(), RT_VERSION, g19_name, (LONG_PTR)&r, flags[i], 0);
        t_note("flags 0x%lx -> %d count %d err %lu", (unsigned long)flags[i], ok, r.count, (unsigned long)GetLastError());
    }
}
TEST(gap19_enumresnames_repeatable) {
    REQUIRE(UN(EnumResourceNamesExW));
    int first = -1;
    for (int i = 0; i < 20; i++) {
        struct g19_res r = { .hmod = g19_k32(), .type = RT_VERSION };
        UN(EnumResourceNamesExW)(g19_k32(), RT_VERSION, g19_name, (LONG_PTR)&r, 0, 0);
        if (first < 0) first = r.count; else EXPECT_EQ(r.count, first);
    }
}
TEST(gap19_enumresnames_langid_param) {
    REQUIRE(UN(EnumResourceNamesExW));
    struct g19_res r = { .hmod = g19_k32(), .type = RT_VERSION };
    BOOL ok = UN(EnumResourceNamesExW)(g19_k32(), RT_VERSION, g19_name, (LONG_PTR)&r, RESOURCE_ENUM_MUI, 0x0409);
    t_note("MUI with langid 0409 -> %d count %d", ok, r.count);
}
TEST(gap19_enumresnames_other_types) {
    REQUIRE(UN(EnumResourceNamesExW));
    LPCWSTR types[] = { RT_MANIFEST, RT_STRING, RT_MESSAGETABLE, RT_ICON, RT_DIALOG };
    for (unsigned i = 0; i < 5; i++) {
        struct g19_res r = { .hmod = g19_k32(), .type = types[i] };
        SET_ERR(0);
        BOOL ok = UN(EnumResourceNamesExW)(g19_k32(), types[i], g19_name, (LONG_PTR)&r, 0, 0);
        t_note("kernel32 type %p -> %d count %d err %lu", types[i], ok, r.count, (unsigned long)GetLastError());
        if (ok) CHECK(r.count >= 1); else EXPECT_EQ(r.count, 0);
    }
}

TEST(gap19_enumreslangs_kernel32_version) {
    REQUIRE(UN(EnumResourceLanguagesExW));
    struct g19_lang r = { 0 };
    SET_ERR(0);
    BOOL ok = UN(EnumResourceLanguagesExW)(g19_k32(), RT_VERSION, MAKEINTRESOURCEW(1), g19_langf, (LONG_PTR)&r, 0, 0);
    EXPECT_EQ(ok, TRUE);
    CHECK(r.count >= 1);
    t_note("kernel32 RT_VERSION/1: %d language(s), first 0x%x", r.count, r.first);
}
TEST(gap19_enumreslangs_stop_early) {
    REQUIRE(UN(EnumResourceLanguagesExW));
    struct g19_lang r = { .stop = TRUE };
    BOOL ok = UN(EnumResourceLanguagesExW)(g19_k32(), RT_VERSION, MAKEINTRESOURCEW(1), g19_langf, (LONG_PTR)&r, 0, 0);
    EXPECT_EQ(r.count, 1);
    t_note("callback FALSE -> %d", ok);
}
TEST(gap19_enumreslangs_name_not_found) {
    REQUIRE(UN(EnumResourceLanguagesExW));
    struct g19_lang r = { 0 };
    SET_ERR(0);
    BOOL ok = UN(EnumResourceLanguagesExW)(g19_k32(), RT_VERSION, MAKEINTRESOURCEW(0x7777), g19_langf, (LONG_PTR)&r, 0, 0);
    EXPECT_EQ(ok, FALSE);
    EXPECT_EQ(r.count, 0);
    EXPECT_ERR(ERROR_RESOURCE_NAME_NOT_FOUND);
}
TEST(gap19_enumreslangs_type_not_found) {
    REQUIRE(UN(EnumResourceLanguagesExW));
    struct g19_lang r = { 0 };
    SET_ERR(0);
    BOOL ok = UN(EnumResourceLanguagesExW)(g19_k32(), MAKEINTRESOURCEW(0x7771), MAKEINTRESOURCEW(1), g19_langf, (LONG_PTR)&r, 0, 0);
    EXPECT_EQ(ok, FALSE);
    EXPECT_EQ(r.count, 0);
    EXPECT_ERR(ERROR_RESOURCE_TYPE_NOT_FOUND);
}
TEST(gap19_enumreslangs_string_name_not_found) {
    REQUIRE(UN(EnumResourceLanguagesExW));
    struct g19_lang r = { 0 };
    SET_ERR(0);
    BOOL ok = UN(EnumResourceLanguagesExW)(g19_k32(), RT_VERSION, L"K32T_NONAME", g19_langf, (LONG_PTR)&r, 0, 0);
    EXPECT_EQ(ok, FALSE);
    EXPECT_EQ(r.count, 0);
    EXPECT_ERR(ERROR_RESOURCE_NAME_NOT_FOUND);
}
TEST(gap19_enumreslangs_exe_without_resources) {
    REQUIRE(UN(EnumResourceLanguagesExW));
    struct g19_lang r = { 0 };
    SET_ERR(0);
    BOOL ok = UN(EnumResourceLanguagesExW)(GetModuleHandleW(NULL), RT_RCDATA, MAKEINTRESOURCEW(1), g19_langf, (LONG_PTR)&r, 0, 0);
    EXPECT_EQ(ok, FALSE);
    EXPECT_EQ(r.count, 0);
    t_note("err %lu", (unsigned long)GetLastError());
}
TEST(gap19_enumreslangs_flags_matrix) {
    REQUIRE(UN(EnumResourceLanguagesExW));
    DWORD flags[] = { 0, RESOURCE_ENUM_LN, RESOURCE_ENUM_MUI, RESOURCE_ENUM_LN | RESOURCE_ENUM_MUI, RESOURCE_ENUM_VALIDATE, 0x80000000 };
    for (unsigned i = 0; i < 6; i++) {
        struct g19_lang r = { 0 };
        SET_ERR(0);
        BOOL ok = UN(EnumResourceLanguagesExW)(g19_k32(), RT_VERSION, MAKEINTRESOURCEW(1), g19_langf, (LONG_PTR)&r, flags[i], 0);
        t_note("flags 0x%lx -> %d count %d err %lu", (unsigned long)flags[i], ok, r.count, (unsigned long)GetLastError());
        if (flags[i] == RESOURCE_ENUM_LN) EXPECT_EQ(ok, TRUE);
    }
}
TEST(gap19_enumreslangs_datafile_matches) {
    REQUIRE(UN(EnumResourceLanguagesExW));
    WCHAR p[MAX_PATH]; GetSystemDirectoryW(p, MAX_PATH); lstrcatW(p, L"\\kernel32.dll");
    HMODULE d = LoadLibraryExW(p, NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (!d) { t_skip("datafile load failed"); return; }
    struct g19_lang a = {0}, b = {0};
    UN(EnumResourceLanguagesExW)(d, RT_VERSION, MAKEINTRESOURCEW(1), g19_langf, (LONG_PTR)&a, 0, 0);
    UN(EnumResourceLanguagesExW)(g19_k32(), RT_VERSION, MAKEINTRESOURCEW(1), g19_langf, (LONG_PTR)&b, 0, 0);
    EXPECT_EQ(a.count, b.count);
    EXPECT_EQ(a.first, b.first);
    FreeLibrary(d);
}

/* ---- EnumCalendarInfoExA ---- */
struct g19_cal;
static struct g19_cal *g19_curcal;
struct g19_cal { int count; char first[64]; CALID firstid; CALID ids[32]; BOOL stop; };
static BOOL CALLBACK g19_calf(LPSTR s, CALID id) {
    struct g19_cal *c = g19_curcal;
    if (c->count == 0) { lstrcpynA(c->first, s, sizeof c->first); c->firstid = id; }
    if (c->count < 32) c->ids[c->count] = id;
    c->count++;
    return !c->stop;
}
static BOOL g19_runcal(struct g19_cal *c, LCID l, CALID cal, CALTYPE t) {
    memset(c, 0, sizeof *c);
    g19_curcal = c;
    return UN(EnumCalendarInfoExA)(g19_calf, l, cal, t);
}
TEST(gap19_enumcalex_month1_gregorian_enus) {
    REQUIRE(UN(EnumCalendarInfoExA));
    struct g19_cal c;
    SET_ERR(0);
    EXPECT_EQ(g19_runcal(&c, 0x0409, CAL_GREGORIAN, CAL_SMONTHNAME1), TRUE);
    EXPECT_EQ(c.count, 1);
    EXPECT_STR(c.first, "January");
    EXPECT_EQ(c.firstid, CAL_GREGORIAN);
}
TEST(gap19_enumcalex_all_twelve_months_enus) {
    REQUIRE(UN(EnumCalendarInfoExA));
    static const char *names[12] = { "January","February","March","April","May","June","July","August","September","October","November","December" };
    for (int i = 0; i < 12; i++) {
        struct g19_cal c;
        EXPECT_EQ(g19_runcal(&c, 0x0409, CAL_GREGORIAN, CAL_SMONTHNAME1 + i), TRUE);
        EXPECT_EQ(c.count, 1);
        EXPECT_STR(c.first, names[i]);
    }
}
TEST(gap19_enumcalex_daynames_enus) {
    REQUIRE(UN(EnumCalendarInfoExA));
    static const char *names[7] = { "Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Sunday" };
    for (int i = 0; i < 7; i++) {
        struct g19_cal c;
        EXPECT_EQ(g19_runcal(&c, 0x0409, CAL_GREGORIAN, CAL_SDAYNAME1 + i), TRUE);
        EXPECT_STR(c.first, names[i]);
    }
    struct g19_cal c;
    EXPECT_EQ(g19_runcal(&c, 0x0409, CAL_GREGORIAN, CAL_SABBREVDAYNAME1), TRUE);
    EXPECT_STR(c.first, "Mon");
    EXPECT_EQ(g19_runcal(&c, 0x0409, CAL_GREGORIAN, CAL_SABBREVMONTHNAME1), TRUE);
    EXPECT_STR(c.first, "Jan");
}
TEST(gap19_enumcalex_stop_early_all_calendars) {
    REQUIRE(UN(EnumCalendarInfoExA));
    struct g19_cal c;
    BOOL ok = g19_runcal(&c, 0x0409, ENUM_ALL_CALENDARS, CAL_SMONTHNAME1);
    EXPECT_EQ(ok, TRUE);
    CHECK(c.count >= 1);
    int total = c.count;
    memset(&c, 0, sizeof c); c.stop = TRUE; g19_curcal = &c;
    UN(EnumCalendarInfoExA)(g19_calf, 0x0409, ENUM_ALL_CALENDARS, CAL_SMONTHNAME1);
    EXPECT_EQ(c.count, 1);
    t_note("ENUM_ALL_CALENDARS en-US month1: %d callbacks", total);
}
TEST(gap19_enumcalex_invalid_caltype) {
    REQUIRE(UN(EnumCalendarInfoExA));
    CALTYPE bad[] = { 0x38, 0x39, 0x100, 0xffff, 0x10000001, 0x1fffffff };
    for (unsigned i = 0; i < 6; i++) {
        struct g19_cal c;
        SET_ERR(0);
        EXPECT_EQ(g19_runcal(&c, 0x0409, CAL_GREGORIAN, bad[i]), FALSE);
        EXPECT_ERR(ERROR_INVALID_FLAGS);
        EXPECT_EQ(c.count, 0);
    }
}
TEST(gap19_enumcalex_user_default_locale) {
    REQUIRE(UN(EnumCalendarInfoExA));
    LCID ids[] = { LOCALE_USER_DEFAULT, LOCALE_SYSTEM_DEFAULT, 0x0409, 0x0407, 0x040c };
    for (unsigned i = 0; i < 5; i++) {
        struct g19_cal c;
        SET_ERR(0);
        BOOL ok = g19_runcal(&c, ids[i], ENUM_ALL_CALENDARS, CAL_SCALNAME);
        t_note("locale 0x%lx CAL_SCALNAME -> %d count %d first '%s'", (unsigned long)ids[i], ok, c.count, c.first);
        CHECK(ok);
        CHECK(c.count >= 1);
    }
}
TEST(gap19_enumcalex_invalid_locale) {
    REQUIRE(UN(EnumCalendarInfoExA));
    struct g19_cal c;
    SET_ERR(0);
    BOOL ok = g19_runcal(&c, 0x0000ffff, CAL_GREGORIAN, CAL_SMONTHNAME1);
    t_note("bogus locale -> %d err %lu count %d", ok, (unsigned long)GetLastError(), c.count);
    if (!ok) EXPECT_EQ(c.count, 0);
}
TEST(gap19_enumcalex_german_month) {
    REQUIRE(UN(EnumCalendarInfoExA));
    struct g19_cal c;
    EXPECT_EQ(g19_runcal(&c, 0x0407, CAL_GREGORIAN, CAL_SMONTHNAME3), TRUE);
    CHECK(c.count >= 1);
    EXPECT_STR(c.first, "M\xe4rz");   /* ANSI CP1252 on the test host; note: depends on ACP */
}
TEST(gap19_enumcalex_return_number) {
    REQUIRE(UN(EnumCalendarInfoExA));
    struct g19_cal c;
    SET_ERR(0);
    BOOL ok = g19_runcal(&c, 0x0409, CAL_GREGORIAN, CAL_ITWODIGITYEARMAX);
    t_note("CAL_ITWODIGITYEARMAX -> %d count %d '%s'", ok, c.count, c.first);
    CHECK(ok);
    int yr = atoi(c.first);
    CHECK(yr >= 1930 && yr <= 2099);
}

/* ---- EnumDateFormatsExW ---- */
struct g19_dt { int count; BOOL stop; WCHAR all[16][48]; CALID ids[16]; };
static struct g19_dt *g19_curdt;
static BOOL CALLBACK g19_dtf(LPWSTR s, CALID id) {
    struct g19_dt *d = g19_curdt;
    if (d->count < 16) { lstrcpynW(d->all[d->count], s, 48); d->ids[d->count] = id; }
    d->count++;
    return !d->stop;
}
static BOOL g19_rundt(struct g19_dt *d, LCID l, DWORD f) { memset(d, 0, sizeof *d); g19_curdt = d; return UN(EnumDateFormatsExW)(g19_dtf, l, f); }
static BOOL g19_hasdt(struct g19_dt *d, const WCHAR *w) {
    for (int i = 0; i < d->count && i < 16; i++) if (!lstrcmpW(d->all[i], w)) return TRUE;
    return FALSE;
}
TEST(gap19_enumdatefmtex_short_enus) {
    REQUIRE(UN(EnumDateFormatsExW));
    struct g19_dt d;
    SET_ERR(0);
    EXPECT_EQ(g19_rundt(&d, 0x0409, DATE_SHORTDATE), TRUE);
    CHECK(d.count >= 1);
    CHECK(g19_hasdt(&d, L"M/d/yyyy"));
    if (!g19_hasdt(&d, L"M/d/yy")) t_note("M/d/yy not in en-US short list");
    for (int i = 0; i < d.count && i < 16; i++) EXPECT_EQ(d.ids[i], CAL_GREGORIAN);
}
TEST(gap19_enumdatefmtex_long_enus) {
    REQUIRE(UN(EnumDateFormatsExW));
    struct g19_dt d;
    EXPECT_EQ(g19_rundt(&d, 0x0409, DATE_LONGDATE), TRUE);
    CHECK(d.count >= 1);
    CHECK(g19_hasdt(&d, L"dddd, MMMM d, yyyy"));
}
TEST(gap19_enumdatefmtex_yearmonth_enus) {
    REQUIRE(UN(EnumDateFormatsExW));
    struct g19_dt d;
    EXPECT_EQ(g19_rundt(&d, 0x0409, DATE_YEARMONTH), TRUE);
    CHECK(d.count >= 1);
    CHECK(g19_hasdt(&d, L"MMMM yyyy"));
}
TEST(gap19_enumdatefmtex_flag_zero_is_short) {
    REQUIRE(UN(EnumDateFormatsExW));
    struct g19_dt a, b;
    EXPECT_EQ(g19_rundt(&a, 0x0409, 0), TRUE);
    EXPECT_EQ(g19_rundt(&b, 0x0409, DATE_SHORTDATE), TRUE);
    EXPECT_EQ(a.count, b.count);
    for (int i = 0; i < a.count && i < 16; i++) EXPECT_WSTR(a.all[i], b.all[i]);
}
TEST(gap19_enumdatefmtex_stop_early) {
    REQUIRE(UN(EnumDateFormatsExW));
    struct g19_dt d; memset(&d, 0, sizeof d); d.stop = TRUE; g19_curdt = &d;
    BOOL ok = UN(EnumDateFormatsExW)(g19_dtf, 0x0409, DATE_SHORTDATE);
    EXPECT_EQ(d.count, 1);
    t_note("callback FALSE -> %d", ok);
}
TEST(gap19_enumdatefmtex_invalid_flags) {
    REQUIRE(UN(EnumDateFormatsExW));
    DWORD bad[] = { 3, 4, 5, 6, 7, 0x10, 0x20, 0x81, 0x100, 0xff };
    for (unsigned i = 0; i < 10; i++) {
        struct g19_dt d;
        SET_ERR(0);
        BOOL ok = g19_rundt(&d, 0x0409, bad[i]);
        if (bad[i] == 3 || bad[i] == 4 || bad[i] == 5 || bad[i] == 0x10 || bad[i] == 0x100 || bad[i] == 0xff) {
            EXPECT_EQ(ok, FALSE);
            EXPECT_ERR(ERROR_INVALID_FLAGS);
            EXPECT_EQ(d.count, 0);
        } else t_note("flags 0x%lx -> %d err %lu", (unsigned long)bad[i], ok, (unsigned long)GetLastError());
    }
}
TEST(gap19_enumdatefmtex_null_callback) {
    REQUIRE(UN(EnumDateFormatsExW));
    SET_ERR(0);
    BOOL ok = UN(EnumDateFormatsExW)(NULL, 0x0409, DATE_SHORTDATE);
    EXPECT_EQ(ok, FALSE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap19_enumdatefmtex_locales) {
    REQUIRE(UN(EnumDateFormatsExW));
    LCID ls[] = { LOCALE_USER_DEFAULT, LOCALE_SYSTEM_DEFAULT, 0x0407, 0x040c, 0x0411, 0x0809 };
    for (unsigned i = 0; i < 6; i++) {
        struct g19_dt d;
        BOOL ok = g19_rundt(&d, ls[i], DATE_SHORTDATE);
        CHECK(ok);
        CHECK(d.count >= 1);
    }
    struct g19_dt d;
    EXPECT_EQ(g19_rundt(&d, 0x0407, DATE_SHORTDATE), TRUE);
    CHECK(g19_hasdt(&d, L"dd.MM.yyyy"));
    EXPECT_EQ(g19_rundt(&d, 0x0809, DATE_SHORTDATE), TRUE);
    CHECK(g19_hasdt(&d, L"dd/MM/yyyy"));
}
TEST(gap19_enumdatefmtex_invalid_locale) {
    REQUIRE(UN(EnumDateFormatsExW));
    struct g19_dt d;
    SET_ERR(0);
    BOOL ok = g19_rundt(&d, 0x0000ffff, DATE_SHORTDATE);
    t_note("bogus locale -> %d err %lu count %d", ok, (unsigned long)GetLastError(), d.count);
    if (!ok) EXPECT_EQ(d.count, 0);
}
TEST(gap19_enumdatefmtex_autolayout_flag) {
    REQUIRE(UN(EnumDateFormatsExW));
    struct g19_dt d;
    SET_ERR(0);
    BOOL ok = g19_rundt(&d, 0x0409, 0x80);
    t_note("DATE_AUTOLAYOUT -> %d count %d err %lu", ok, d.count, (unsigned long)GetLastError());
}
TEST(gap19_enumdatefmtex_repeatable) {
    REQUIRE(UN(EnumDateFormatsExW));
    struct g19_dt a;
    g19_rundt(&a, 0x0409, DATE_LONGDATE);
    for (int i = 0; i < 50; i++) {
        struct g19_dt b;
        EXPECT_EQ(g19_rundt(&b, 0x0409, DATE_LONGDATE), TRUE);
        EXPECT_EQ(a.count, b.count);
    }
}

/* ---- GetStringTypeExA ---- */
static WORD g19_ct(DWORD info, const char *s, int n, int idx, BOOL *ok) {
    WORD out[64]; memset(out, 0xee, sizeof out);
    BOOL r = UN(GetStringTypeExA)(0x0409, info, s, n, out);
    if (ok) *ok = r;
    return out[idx];
}
TEST(gap19_gsteA_ctype1_basic) {
    REQUIRE(UN(GetStringTypeExA));
    BOOL ok;
    WORD w = g19_ct(CT_CTYPE1, "A", 1, 0, &ok);
    EXPECT_EQ(ok, TRUE);
    CHECK((w & C1_UPPER) && (w & C1_ALPHA) && !(w & C1_LOWER));
    w = g19_ct(CT_CTYPE1, "a", 1, 0, &ok);
    CHECK((w & C1_LOWER) && (w & C1_ALPHA) && !(w & C1_UPPER));
    w = g19_ct(CT_CTYPE1, "5", 1, 0, &ok);
    CHECK((w & C1_DIGIT) && !(w & C1_ALPHA));
    w = g19_ct(CT_CTYPE1, " ", 1, 0, &ok);
    CHECK((w & C1_SPACE) && (w & C1_BLANK));
    w = g19_ct(CT_CTYPE1, ".", 1, 0, &ok);
    CHECK((w & C1_PUNCT));
    w = g19_ct(CT_CTYPE1, "F", 1, 0, &ok);
    CHECK((w & C1_XDIGIT) && (w & C1_UPPER));
    w = g19_ct(CT_CTYPE1, "g", 1, 0, &ok);
    CHECK(!(w & C1_XDIGIT));
    w = g19_ct(CT_CTYPE1, "\t", 1, 0, &ok);
    CHECK((w & C1_CNTRL) && (w & C1_SPACE));
    w = g19_ct(CT_CTYPE1, "\x01", 1, 0, &ok);
    CHECK((w & C1_CNTRL) && !(w & C1_SPACE));
}
TEST(gap19_gsteA_ctype1_string_each_char) {
    REQUIRE(UN(GetStringTypeExA));
    WORD out[8] = {0};
    EXPECT_EQ(UN(GetStringTypeExA)(0x0409, CT_CTYPE1, "aB1 ", 4, out), TRUE);
    CHECK(out[0] & C1_LOWER); CHECK(out[1] & C1_UPPER); CHECK(out[2] & C1_DIGIT); CHECK(out[3] & C1_SPACE);
}
TEST(gap19_gsteA_cch_minus1_includes_terminator) {
    REQUIRE(UN(GetStringTypeExA));
    WORD out[8]; memset(out, 0x77, sizeof out);
    EXPECT_EQ(UN(GetStringTypeExA)(0x0409, CT_CTYPE1, "ab", -1, out), TRUE);
    CHECK(out[0] & C1_ALPHA); CHECK(out[1] & C1_ALPHA);
    EXPECT_NE(out[2], 0x7777);       /* terminator classified too */
    EXPECT_EQ(out[3], 0x7777);       /* nothing beyond */
}
TEST(gap19_gsteA_cch_exact_does_not_touch_beyond) {
    REQUIRE(UN(GetStringTypeExA));
    WORD out[8]; memset(out, 0x77, sizeof out);
    EXPECT_EQ(UN(GetStringTypeExA)(0x0409, CT_CTYPE1, "abcd", 2, out), TRUE);
    EXPECT_NE(out[1], 0x7777);
    EXPECT_EQ(out[2], 0x7777);
}
TEST(gap19_gsteA_ctype2) {
    REQUIRE(UN(GetStringTypeExA));
    BOOL ok;
    WORD w = g19_ct(CT_CTYPE2, "A", 1, 0, &ok);
    EXPECT_EQ(ok, TRUE);
    EXPECT_EQ(w, C2_LEFTTORIGHT);
    w = g19_ct(CT_CTYPE2, " ", 1, 0, &ok);
    EXPECT_EQ(w, C2_WHITESPACE);
    w = g19_ct(CT_CTYPE2, "1", 1, 0, &ok);
    EXPECT_EQ(w, C2_EUROPENUMBER);
    w = g19_ct(CT_CTYPE2, "+", 1, 0, &ok);
    t_note("CTYPE2 of '+' = %u", w);
}
TEST(gap19_gsteA_ctype3) {
    REQUIRE(UN(GetStringTypeExA));
    BOOL ok;
    WORD w = g19_ct(CT_CTYPE3, "A", 1, 0, &ok);
    EXPECT_EQ(ok, TRUE);
    CHECK(w & C3_ALPHA);
    w = g19_ct(CT_CTYPE3, "1", 1, 0, &ok);
    CHECK(!(w & C3_ALPHA));
}
TEST(gap19_gsteA_invalid_info_type) {
    REQUIRE(UN(GetStringTypeExA));
    WORD out[4];
    DWORD bad[] = { 0, 3, 5, 8, 0x10, 0xffffffff };
    for (unsigned i = 0; i < 6; i++) {
        SET_ERR(0);
        EXPECT_EQ(UN(GetStringTypeExA)(0x0409, bad[i], "a", 1, out), FALSE);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(gap19_gsteA_null_params) {
    if (t_is_wine()) { t_skip("wine faults on NULL output"); return; }
    REQUIRE(UN(GetStringTypeExA));
    WORD out[4];
    SET_ERR(0);
    SET_ERR(0);
    EXPECT_EQ(UN(GetStringTypeExA)(0x0409, CT_CTYPE1, "a", 1, NULL), FALSE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap19_gsteA_bad_count) {
    REQUIRE(UN(GetStringTypeExA));
    WORD out[4];
    SET_ERR(0);
    BOOL r0 = UN(GetStringTypeExA)(0x0409, CT_CTYPE1, "a", 0, out);
    t_note("cchSrc 0 -> %d err %lu", r0, (unsigned long)GetLastError());
    /* any negative count means "NUL-terminated" in the W worker (decomp) */
    WORD o2[8];
    SET_ERR(0);
    BOOL rm = UN(GetStringTypeExA)(0x0409, CT_CTYPE1, "a", -2, o2);
    t_note("cchSrc -2 -> %d", rm);
}
TEST(gap19_gsteA_locale_variants) {
    REQUIRE(UN(GetStringTypeExA));
    LCID ls[] = { LOCALE_USER_DEFAULT, LOCALE_SYSTEM_DEFAULT, 0x0409, 0x0407, 0x0411, 0x0000 };
    for (unsigned i = 0; i < 6; i++) {
        WORD out[2] = {0};
        BOOL ok = UN(GetStringTypeExA)(ls[i], CT_CTYPE1, "Z", 1, out);
        t_note("locale 0x%lx -> %d type 0x%x", (unsigned long)ls[i], ok, out[0]);
        if (ok) CHECK(out[0] & C1_UPPER);
    }
}
TEST(gap19_gsteA_high_bytes_latin1) {
    REQUIRE(UN(GetStringTypeExA));
    /* CP1252 0xE9 e-acute / 0xC9 E-acute; only valid when ACP is 1252 */
    if (GetACP() != 1252) { t_skip("ACP != 1252"); return; }
    WORD out[2] = {0};
    EXPECT_EQ(UN(GetStringTypeExA)(0x0409, CT_CTYPE1, "\xe9\xc9", 2, out), TRUE);
    CHECK(out[0] & C1_LOWER); CHECK(out[1] & C1_UPPER);
}
TEST(gap19_gsteA_same_as_gstA) {
    REQUIRE(UN(GetStringTypeExA));
    typedef BOOL (WINAPI *gsta_t)(LCID, DWORD, LPCSTR, int, LPWORD);
    gsta_t a = (gsta_t)t_proc_k32("GetStringTypeA");
    if (!a) { t_skip("no GetStringTypeA"); return; }
    WORD o1[16], o2[16];
    EXPECT_EQ(UN(GetStringTypeExA)(0x0409, CT_CTYPE1, "Hello, 42!", 10, o1), TRUE);
    EXPECT_EQ(a(0x0409, CT_CTYPE1, "Hello, 42!", 10, o2) , TRUE);
}
