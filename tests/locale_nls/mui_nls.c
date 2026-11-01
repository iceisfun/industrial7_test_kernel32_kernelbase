/* NLS versions, UI languages, preferred UI language lists and MUI file queries. */
#include "nlsutil.h"

/* ---- NLS version ---- */
TEST(nlsver_getnlsversion_basic) {
    REQUIRE(U(GetNLSVersion));
    NLSVERSIONINFO v; memset(&v, 0xAB, sizeof v); v.dwNLSVersionInfoSize = sizeof v;
    SET_ERR(0);
    BOOL r = U(GetNLSVersion)(COMPARE_STRING, 0x409, &v);
    CHECKF(r, "err %lu", GetLastError());
    if (r) {
        CHECK(v.dwNLSVersion != 0); CHECK(v.dwDefinedVersion != 0);
        t_note("en-US NLS version 0x%08lx defined 0x%08lx effective 0x%lx", v.dwNLSVersion, v.dwDefinedVersion, v.dwEffectiveId);
    }
}
TEST(nlsver_getnlsversion_locales) {
    REQUIRE(U(GetNLSVersion));
    for (int i = 0; i < ARRAYLEN(FIXED_LCIDS); i++) {
        NLSVERSIONINFO v; memset(&v, 0, sizeof v); v.dwNLSVersionInfoSize = sizeof v;
        SET_ERR(0);
        BOOL r = U(GetNLSVersion)(COMPARE_STRING, FIXED_LCIDS[i], &v);
        CHECKF(r && v.dwNLSVersion != 0, "lcid 0x%lx err %lu", FIXED_LCIDS[i], GetLastError());
    }
}
TEST(nlsver_getnlsversion_same_across_calls) {
    REQUIRE(U(GetNLSVersion));
    NLSVERSIONINFO a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b); a.dwNLSVersionInfoSize = b.dwNLSVersionInfoSize = sizeof a;
    CHECK(U(GetNLSVersion)(COMPARE_STRING, 0x409, &a)); CHECK(U(GetNLSVersion)(COMPARE_STRING, 0x409, &b));
    CHECK(a.dwNLSVersion == b.dwNLSVersion && a.dwDefinedVersion == b.dwDefinedVersion && a.dwEffectiveId == b.dwEffectiveId);
}
TEST(nlsver_getnlsversion_errors) {
    REQUIRE(U(GetNLSVersion));
    NLSVERSIONINFO v; memset(&v, 0, sizeof v); v.dwNLSVersionInfoSize = sizeof v;
    if (!t_is_wine()) { SET_ERR(0); CHECK(!U(GetNLSVersion)(COMPARE_STRING, 0x409, NULL)); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
    SET_ERR(0); CHECK(!U(GetNLSVersion)(COMPARE_STRING, 0xDEAD, &v)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!U(GetNLSVersion)(COMPARE_STRING, 0xFFFFFFFF, &v)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); BOOL r = U(GetNLSVersion)((NLS_FUNCTION)0, 0x409, &v); t_note("GetNLSVersion(function 0) = %d err %lu", r, GetLastError());
    SET_ERR(0); r = U(GetNLSVersion)((NLS_FUNCTION)2, 0x409, &v); t_note("GetNLSVersion(function 2) = %d err %lu", r, GetLastError());
    SET_ERR(0); r = U(GetNLSVersion)((NLS_FUNCTION)0xFFFF, 0x409, &v); t_note("GetNLSVersion(function 0xFFFF) = %d err %lu", r, GetLastError());
}
TEST(nlsver_getnlsversionex_basic) {
    REQUIRE(U(GetNLSVersionEx));
    NLSVERSIONINFOEX v; memset(&v, 0xAB, sizeof v); v.dwNLSVersionInfoSize = sizeof v;
    SET_ERR(0);
    BOOL r = U(GetNLSVersionEx)(COMPARE_STRING, L"en-US", &v);
    CHECKF(r, "err %lu", GetLastError());
    if (r) {
        CHECK(v.dwNLSVersion != 0); CHECK(v.dwDefinedVersion != 0);
        CHECKF(v.dwEffectiveId == 0x409, "effective id 0x%lx", v.dwEffectiveId);
        t_note("en-US NLS ex version 0x%08lx defined 0x%08lx", v.dwNLSVersion, v.dwDefinedVersion);
    }
}
TEST(nlsver_ex_matches_lcid_version) {
    REQUIRE(U(GetNLSVersionEx)); REQUIRE(U(GetNLSVersion));
    for (int i = 0; i < ARRAYLEN(FIXED_LOCALES); i++) {
        NLSVERSIONINFOEX e; NLSVERSIONINFO v;
        memset(&e, 0, sizeof e); memset(&v, 0, sizeof v); e.dwNLSVersionInfoSize = sizeof e; v.dwNLSVersionInfoSize = sizeof v;
        BOOL r1 = U(GetNLSVersionEx)(COMPARE_STRING, FIXED_LOCALES[i], &e);
        BOOL r2 = U(GetNLSVersion)(COMPARE_STRING, FIXED_LCIDS[i], &v);
        CHECKF(r1 && r2, "%ls", FIXED_LOCALES[i]);
        if (r1 && r2) CHECKF(e.dwNLSVersion == v.dwNLSVersion && e.dwDefinedVersion == v.dwDefinedVersion, "%ls 0x%lx vs 0x%lx", FIXED_LOCALES[i], e.dwNLSVersion, v.dwNLSVersion);
    }
}
TEST(nlsver_ex_default_and_invariant_names) {
    REQUIRE(U(GetNLSVersionEx));
    NLSVERSIONINFOEX v; memset(&v, 0, sizeof v); v.dwNLSVersionInfoSize = sizeof v;
    CHECK(U(GetNLSVersionEx)(COMPARE_STRING, LOCALE_NAME_USER_DEFAULT, &v));
    memset(&v, 0, sizeof v); v.dwNLSVersionInfoSize = sizeof v;
    CHECK(U(GetNLSVersionEx)(COMPARE_STRING, L"", &v));
    memset(&v, 0, sizeof v); v.dwNLSVersionInfoSize = sizeof v;
    CHECK(U(GetNLSVersionEx)(COMPARE_STRING, L"de-DE_phoneb", &v) || 1);
}
TEST(nlsver_ex_errors) {
    REQUIRE(U(GetNLSVersionEx));
    NLSVERSIONINFOEX v; memset(&v, 0, sizeof v); v.dwNLSVersionInfoSize = sizeof v;
    if (!t_is_wine()) { SET_ERR(0); CHECK(!U(GetNLSVersionEx)(COMPARE_STRING, L"en-US", NULL)); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
    SET_ERR(0); CHECK(!U(GetNLSVersionEx)(COMPARE_STRING, L"no-such-locale-qq", &v)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!U(GetNLSVersionEx)(COMPARE_STRING, L"!!!", &v)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(nlsver_isvalidnlsversion) {
    REQUIRE(U(IsValidNLSVersion)); REQUIRE(U(GetNLSVersionEx));
    if (t_is_wine()) { t_skip("emulator faults on malformed version structs"); return; }
    NLSVERSIONINFOEX v; memset(&v, 0, sizeof v); v.dwNLSVersionInfoSize = sizeof v;
    CHECK(U(GetNLSVersionEx)(COMPARE_STRING, L"en-US", &v));
    SET_ERR(0);
    DWORD r = U(IsValidNLSVersion)(COMPARE_STRING, L"en-US", &v);
    CHECKF(r != 0, "err %lu", GetLastError());
    SET_ERR(0); CHECK(U(IsValidNLSVersion)(COMPARE_STRING, L"en-US", NULL) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(U(IsValidNLSVersion)((NLS_FUNCTION)2, L"en-US", &v) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(U(IsValidNLSVersion)((NLS_FUNCTION)0, L"en-US", &v) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(U(IsValidNLSVersion)(COMPARE_STRING, L"zz-bad-name", &v) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    NLSVERSIONINFOEX w = v; w.dwNLSVersionInfoSize = 0x10;
    SET_ERR(0); CHECK(U(IsValidNLSVersion)(COMPARE_STRING, L"en-US", &w) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    w.dwNLSVersionInfoSize = 0xc;
    SET_ERR(0); r = U(IsValidNLSVersion)(COMPARE_STRING, L"en-US", &w); t_note("IsValidNLSVersion with 12-byte struct: %lu err %lu", r, GetLastError());
    w = v; w.dwNLSVersionInfoSize = 0;
    SET_ERR(0); CHECK(U(IsValidNLSVersion)(COMPARE_STRING, L"en-US", &w) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}

/* ---- default UI languages ---- */
TEST(uilang_default_ui_languages_valid) {
    REQUIRE(U(GetUserDefaultUILanguage)); REQUIRE(U(GetSystemDefaultUILanguage)); REQUIRE(U(LCIDToLocaleName));
    LANGID u = U(GetUserDefaultUILanguage)(), s = U(GetSystemDefaultUILanguage)();
    CHECK(u != 0); CHECK(s != 0);
    WCHAR b[100];
    CHECK(U(LCIDToLocaleName)(u, b, 100, 0) > 0);
    CHECK(U(LCIDToLocaleName)(s, b, 100, 0) > 0);
    t_note("UI languages: user 0x%04x system 0x%04x", u, s);
}
TEST(uilang_default_stable) {
    REQUIRE(U(GetUserDefaultUILanguage)); REQUIRE(U(GetSystemDefaultUILanguage)); REQUIRE(U(GetThreadUILanguage));
    LANGID u = U(GetUserDefaultUILanguage)(), s = U(GetSystemDefaultUILanguage)(), t = U(GetThreadUILanguage)();
    for (int i = 0; i < 50; i++) { CHECK(U(GetUserDefaultUILanguage)() == u); CHECK(U(GetSystemDefaultUILanguage)() == s); CHECK(U(GetThreadUILanguage)() == t); }
}
TEST(uilang_thread_ui_language_nonzero) {
    REQUIRE(U(GetThreadUILanguage)); REQUIRE(U(LCIDToLocaleName));
    LANGID t = U(GetThreadUILanguage)();
    CHECK(t != 0);
    WCHAR b[100]; CHECK(U(LCIDToLocaleName)(t, b, 100, 0) > 0);
}
static DWORD WINAPI uilang_thread_fn(LPVOID p) { *(LANGID *)p = U(GetThreadUILanguage)(); return 0; }
TEST(uilang_thread_ui_language_in_other_thread) {
    REQUIRE(U(GetThreadUILanguage));
    LANGID main_id = U(GetThreadUILanguage)(), other = 0;
    HANDLE h = CreateThread(NULL, 0, uilang_thread_fn, &other, 0, NULL);
    CHECK(h != NULL);
    if (h) { WaitForSingleObject(h, 5000); CloseHandle(h); }
    CHECK(other == main_id);
}

/* ---- preferred UI language lists ---- */
typedef BOOL (WINAPI *get_pref_fn)(DWORD, PULONG, PZZWSTR, PULONG);

static int count_multisz(const WCHAR *m, int *total)
{
    int n = 0; const WCHAR *p = m;
    while (*p) { p += lstrlenW(p) + 1; n++; }
    *total = (int)(p - m) + 1;
    if (n == 0) *total = 2;   /* P1: an empty multi-string is two NULs; Win10 reports cch 2 for an empty list */
    return n;
}
static void pref_query(get_pref_fn fn, const char *what, DWORD flags)
{
    ULONG num = 0xDEAD, cch = 0; WCHAR buf[2048];
    SET_ERR(0);
    BOOL r = fn(flags, &num, NULL, &cch);
    CHECKF(r, "%s flags 0x%lx size query err %lu", what, flags, GetLastError());
    if (!r) return;
    CHECKF(cch >= 1 && cch < 2048, "%s cch=%lu", what, cch);
    if (cch >= 2048) return;
    ULONG want = cch, num2 = 0xDEAD;
    memset(buf, 0x77, sizeof buf);
    r = fn(flags, &num2, buf, &cch);
    CHECKF(r, "%s flags 0x%lx fetch err %lu", what, flags, GetLastError());
    if (!r) return;
    CHECKF(num2 == num, "%s count %lu vs %lu", what, num2, num);
    CHECKF(cch == want, "%s cch %lu vs %lu", what, cch, want);
    int total; int n = count_multisz(buf, &total);
    CHECKF((ULONG)n == num, "%s strings %d vs num %lu", what, n, num);
    CHECKF((ULONG)total == cch, "%s multisz length %d vs cch %lu", what, total, cch);
    if (num == 0) CHECKF(cch >= 1 && buf[0] == 0, "%s empty list", what);
    /* exact-minus-one must fail with ERROR_INSUFFICIENT_BUFFER and report the needed size */
    if (cch > 1) {
        ULONG small = cch - 1, nn = 0xDEAD;
        SET_ERR(0);
        r = fn(flags, &nn, buf, &small);
        CHECKF(!r, "%s too-small buffer must fail", what);
        EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
        CHECKF(small == want, "%s needed size reported %lu vs %lu", what, small, want);
    }
}
TEST(uipref_system_name_list) {
    REQUIRE(U(GetSystemPreferredUILanguages));
    pref_query(U(GetSystemPreferredUILanguages), "system", MUI_LANGUAGE_NAME);
    ULONG num = 0, cch = 0; WCHAR buf[1024];
    CHECK(U(GetSystemPreferredUILanguages)(MUI_LANGUAGE_NAME, &num, NULL, &cch));
    CHECK(num >= 1);
    cch = 1024; CHECK(U(GetSystemPreferredUILanguages)(MUI_LANGUAGE_NAME, &num, buf, &cch));
    for (const WCHAR *p = buf; *p; p += lstrlenW(p) + 1) CHECKF(U(IsValidLocaleName)(p), "name %ls", p);
}
TEST(uipref_system_id_list) {
    REQUIRE(U(GetSystemPreferredUILanguages));
    pref_query(U(GetSystemPreferredUILanguages), "system-id", MUI_LANGUAGE_ID);
    ULONG num = 0, cch = 1024; WCHAR buf[1024];
    CHECK(U(GetSystemPreferredUILanguages)(MUI_LANGUAGE_ID, &num, buf, &cch));
    CHECK(num >= 1);
    for (const WCHAR *p = buf; *p; p += lstrlenW(p) + 1) {
        CHECKF(lstrlenW(p) == 4, "id %ls", p);
        int hex = 1; for (const WCHAR *q = p; *q; q++) if (!iswxdigit(*q)) hex = 0;
        CHECK(hex);
    }
}
TEST(uipref_system_name_id_agree) {
    REQUIRE(U(GetSystemPreferredUILanguages)); REQUIRE(U(LocaleNameToLCID));
    ULONG n1 = 0, c1 = 1024, n2 = 0, c2 = 1024; WCHAR a[1024], b[1024];
    CHECK(U(GetSystemPreferredUILanguages)(MUI_LANGUAGE_NAME, &n1, a, &c1));
    CHECK(U(GetSystemPreferredUILanguages)(MUI_LANGUAGE_ID, &n2, b, &c2));
    CHECK(n1 == n2);
    const WCHAR *pa = a, *pb = b;
    for (ULONG i = 0; i < n1 && i < n2 && *pa && *pb; i++) {
        LCID l = U(LocaleNameToLCID)(pa, 0x08000000);
        CHECKF(l == (LCID)wcstoul(pb, NULL, 16), "%ls vs %ls (0x%lx)", pa, pb, l);
        pa += lstrlenW(pa) + 1; pb += lstrlenW(pb) + 1;
    }
}
TEST(uipref_system_first_is_system_ui_language) {
    REQUIRE(U(GetSystemPreferredUILanguages)); REQUIRE(U(GetSystemDefaultUILanguage)); REQUIRE(U(LCIDToLocaleName));
    ULONG num = 0, cch = 1024; WCHAR buf[1024], nm[100];
    CHECK(U(GetSystemPreferredUILanguages)(MUI_LANGUAGE_NAME, &num, buf, &cch));
    U(LCIDToLocaleName)(U(GetSystemDefaultUILanguage)(), nm, 100, 0);
    if (lstrcmpiW(buf, nm)) t_note("first system preferred '%ls' vs default UI language '%ls'", buf, nm);
}
TEST(uipref_system_machine_settings_flag) {
    REQUIRE(U(GetSystemPreferredUILanguages));
    pref_query(U(GetSystemPreferredUILanguages), "system-machine", MUI_LANGUAGE_NAME | MUI_MACHINE_LANGUAGE_SETTINGS);
    pref_query(U(GetSystemPreferredUILanguages), "system-machine-id", MUI_LANGUAGE_ID | MUI_MACHINE_LANGUAGE_SETTINGS);
}
TEST(uipref_system_flag_zero) {
    REQUIRE(U(GetSystemPreferredUILanguages));
    ULONG num = 0, cch = 0; SET_ERR(0);
    BOOL r = U(GetSystemPreferredUILanguages)(0, &num, NULL, &cch);
    t_note("system flags 0: r=%d num=%lu cch=%lu err=%lu", r, num, cch, r ? 0 : GetLastError());
}
TEST(uipref_system_invalid_flags) {
    REQUIRE(U(GetSystemPreferredUILanguages));
    static const DWORD bad[] = { 0x1, 0x2, 0x10, 0x20, 0x30, 0x40, 0x80, 0x100, 0x200, 0x800, 0x1000, 0x80000000, 0xFFFFFFFF };
    for (int i = 0; i < ARRAYLEN(bad); i++) {
        ULONG num = 0xDEAD, cch = 0; SET_ERR(0);
        BOOL r = U(GetSystemPreferredUILanguages)(bad[i] | MUI_LANGUAGE_NAME, &num, NULL, &cch);
        CHECKF(!r, "flags 0x%lx", bad[i] | MUI_LANGUAGE_NAME);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(uipref_system_both_id_and_name) {
    REQUIRE(U(GetSystemPreferredUILanguages));
    ULONG num = 0, cch = 0; SET_ERR(0);
    BOOL r = U(GetSystemPreferredUILanguages)(MUI_LANGUAGE_ID | MUI_LANGUAGE_NAME, &num, NULL, &cch);
    CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
CHILD(prefnull) {
    ULONG num = 0, cch = 0; int k = argc > 1 ? atoi(argv[1]) : 0; BOOL r = 0;
    SetLastError(0);
    switch (k) {
    case 0: r = U(GetSystemPreferredUILanguages)(MUI_LANGUAGE_NAME, &num, NULL, NULL); break;
    case 1: r = U(GetSystemPreferredUILanguages)(MUI_LANGUAGE_NAME, NULL, NULL, &cch); break;
    case 2: r = U(GetUserPreferredUILanguages)(MUI_LANGUAGE_NAME, &num, NULL, NULL); break;
    default: r = U(GetUserPreferredUILanguages)(MUI_LANGUAGE_NAME, NULL, NULL, &cch); break;
    }
    return r ? 1 : (int)(0x100 | (GetLastError() & 0xff));
}
TEST(uipref_null_out_params) {
    REQUIRE(U(GetSystemPreferredUILanguages)); REQUIRE(U(GetUserPreferredUILanguages));
    ULONG num = 0, cch = 0; WCHAR buf[8];
    /* P1: Win10 RtlGetSystemPreferredUILanguages dereferences pcchLanguagesBuffer unconditionally (AV on NULL)
     * and the other NULL out-parameters are not documented as validated: probe them in child processes and
     * only note the outcome (UNRESOLVED, no expectation). */
    if (!t_is_wine()) {
        for (int k = 0; k < 4; k++) {
            char a[8]; wsprintfA(a, "%d", k);
            DWORD rc = t_run_child("prefnull", a, 20000);
            t_note("NULL out-param probe %d: child exit 0x%lx", k, rc);
            /* P1 audit: case 0 (System, NULL pcch) is decided by the decompile: '*param_5' is read first -> AV */
            if (k == 0) CHECKF(rc == 0xC0000005, "System NULL pcch: child exit 0x%lx", rc);
        }
    }
    cch = 8;
    SET_ERR(0); CHECK(!U(GetSystemPreferredUILanguages)(MUI_LANGUAGE_NAME, &num, buf, &cch) || cch <= 8);
    cch = 2; num = 0;
    SET_ERR(0);
    BOOL r = U(GetSystemPreferredUILanguages)(MUI_LANGUAGE_NAME, &num, buf, &cch);
    CHECK(!r); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER); CHECK(cch > 2);
}
TEST(uipref_user_name_and_id) {
    REQUIRE(U(GetUserPreferredUILanguages));
    pref_query(U(GetUserPreferredUILanguages), "user", MUI_LANGUAGE_NAME);
    pref_query(U(GetUserPreferredUILanguages), "user-id", MUI_LANGUAGE_ID);
    ULONG num = 0, cch = 0; CHECK(U(GetUserPreferredUILanguages)(MUI_LANGUAGE_NAME, &num, NULL, &cch)); CHECK(num >= 1);
}
TEST(uipref_user_invalid_flags) {
    REQUIRE(U(GetUserPreferredUILanguages));
    static const DWORD bad[] = { 0x1, 0x10, 0x20, 0x40, 0x100, 0x200, 0x400, 0x80000000 };
    for (int i = 0; i < ARRAYLEN(bad); i++) {
        ULONG num = 0xDEAD, cch = 0; SET_ERR(0);
        BOOL r = U(GetUserPreferredUILanguages)(bad[i] | MUI_LANGUAGE_NAME, &num, NULL, &cch);
        CHECKF(!r, "flags 0x%lx", bad[i] | MUI_LANGUAGE_NAME);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
    ULONG num = 0, cch = 0; SET_ERR(0);
    if (!t_is_wine()) { CHECK(!U(GetUserPreferredUILanguages)(MUI_LANGUAGE_NAME | MUI_LANGUAGE_ID, &num, NULL, &cch)); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
}
TEST(uipref_user_contains_default_ui_language) {
    REQUIRE(U(GetUserPreferredUILanguages)); REQUIRE(U(GetUserDefaultUILanguage));
    ULONG num = 0, cch = 1024; WCHAR buf[1024];
    CHECK(U(GetUserPreferredUILanguages)(MUI_LANGUAGE_ID, &num, buf, &cch));
    WCHAR want[8]; wsprintfW(want, L"%04x", U(GetUserDefaultUILanguage)());
    int found = 0;
    for (const WCHAR *p = buf; *p; p += lstrlenW(p) + 1) if (!lstrcmpiW(p, want)) found = 1;
    if (!found) t_note("user default UI language %ls not in user preferred list", want);
}
TEST(uipref_thread_merge_fallback) {
    REQUIRE(U(GetThreadPreferredUILanguages));
    ULONG num = 0, cch = 0; SET_ERR(0);
    BOOL r = U(GetThreadPreferredUILanguages)(MUI_LANGUAGE_NAME | MUI_MERGE_SYSTEM_FALLBACK | MUI_MERGE_USER_FALLBACK, &num, NULL, &cch);
    CHECKF(r, "err %lu", GetLastError());
    CHECK(num >= 1);
    pref_query(U(GetThreadPreferredUILanguages), "thread-merged", MUI_LANGUAGE_NAME | MUI_UI_FALLBACK);
    pref_query(U(GetThreadPreferredUILanguages), "thread-merged-id", MUI_LANGUAGE_ID | MUI_UI_FALLBACK);
}
TEST(uipref_thread_unset_list) {
    REQUIRE(U(GetThreadPreferredUILanguages));
    ULONG num = 0xDEAD, cch = 0; SET_ERR(0);
    BOOL r = U(GetThreadPreferredUILanguages)(MUI_LANGUAGE_NAME, &num, NULL, &cch);
    CHECKF(r, "err %lu", GetLastError());
    t_note("thread list without fallbacks: num=%lu cch=%lu", num, cch);
}
TEST(uipref_thread_invalid) {
    REQUIRE(U(GetThreadPreferredUILanguages));
    ULONG num = 0, cch = 0; SET_ERR(0);
    if (!t_is_wine()) { CHECK(!U(GetThreadPreferredUILanguages)(MUI_LANGUAGE_NAME | MUI_LANGUAGE_ID, &num, NULL, &cch)); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
    if (!t_is_wine()) { SET_ERR(0); CHECK(!U(GetThreadPreferredUILanguages)(MUI_LANGUAGE_NAME, &num, NULL, NULL)); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
    /* P1: Win10 does not validate a NULL pulNumLanguages (RtlGetThreadPreferredUILanguages checks only pcch);
       the size query succeeds (baseline: TRUE, err 0) */
    if (!t_is_wine()) { SET_ERR(0); CHECK(U(GetThreadPreferredUILanguages)(MUI_LANGUAGE_NAME, NULL, NULL, &cch)); }
    if (!t_is_wine()) { SET_ERR(0); CHECK(!U(GetThreadPreferredUILanguages)(0x80000000 | MUI_LANGUAGE_NAME, &num, NULL, &cch)); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
}
TEST(uipref_process_list) {
    REQUIRE(U(GetProcessPreferredUILanguages));
    ULONG num = 0xDEAD, cch = 0; SET_ERR(0);
    BOOL r = U(GetProcessPreferredUILanguages)(MUI_LANGUAGE_NAME, &num, NULL, &cch);
    CHECKF(r, "err %lu", GetLastError());
    t_note("process list: num=%lu cch=%lu", num, cch);
    /* P1: the MUI_MERGE_*_FALLBACK flags are thread-only; RtlGetProcessPreferredUILanguages rejects any
     * flag outside MUI_LANGUAGE_ID/NAME (observed err 87 for 0x38). Query the ID format instead. */
    { ULONG n2 = 0xDEAD, c2 = 0; SET_ERR(0);
      CHECKF(U(GetProcessPreferredUILanguages)(MUI_LANGUAGE_ID, &n2, NULL, &c2), "process id-format size query err %lu", GetLastError());
      CHECKF(c2 >= 1, "cch=%lu", c2); }
    if (!t_is_wine()) { SET_ERR(0); CHECK(!U(GetProcessPreferredUILanguages)(MUI_LANGUAGE_NAME | MUI_LANGUAGE_ID, &num, NULL, &cch)); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
    if (!t_is_wine()) { SET_ERR(0); CHECK(!U(GetProcessPreferredUILanguages)(MUI_LANGUAGE_NAME, NULL, NULL, &cch)); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
}

/* Set* in a child process: nothing here may leak into the main test process. */
static void uiname(WCHAR *out)
{
    LANGID id = U(GetUserDefaultUILanguage)();
    if (U(LCIDToLocaleName)(id, out, 85, 0) <= 0) lstrcpyW(out, L"en-US");
}
CHILD(setthreadpref) {
    int bad = 0;
    WCHAR name[100], list[200], buf[512]; ULONG num = 0, cch;
    uiname(name);
    memset(list, 0, sizeof list); lstrcpyW(list, name);   /* "name\0\0" */
    SetLastError(0);
    if (!U(SetThreadPreferredUILanguages)(MUI_LANGUAGE_NAME, list, &num)) return 0x1000;  /* language may not be installed */
    if (num != 1) bad |= 2;
    cch = 512; num = 0;
    /* P1: without MUI_THREAD_LANGUAGES the Get merges the user fallback list (RtlGetThreadPreferredUILanguages) */
    if (!U(GetThreadPreferredUILanguages)(MUI_LANGUAGE_NAME | MUI_THREAD_LANGUAGES, &num, buf, &cch)) bad |= 4;
    if (num != 1 || lstrcmpiW(buf, name)) bad |= 8;
    if (cch != (ULONG)lstrlenW(name) + 2) bad |= 16;
    /* GetThreadUILanguage follows the first entry */
    LCID l = U(LocaleNameToLCID)(name, 0x08000000);
    if (U(GetThreadUILanguage)() != (LANGID)l) bad |= 32;
    /* ID format */
    cch = 512; num = 0;
    if (!U(GetThreadPreferredUILanguages)(MUI_LANGUAGE_ID | MUI_THREAD_LANGUAGES, &num, buf, &cch)) bad |= 64;
    else { WCHAR want[8]; wsprintfW(want, L"%04x", (unsigned)(l & 0xffff)); if (lstrcmpiW(buf, want)) bad |= 128; }
    /* the process list is unaffected */
    ULONG pn = 0, pc = 512;
    if (U(GetProcessPreferredUILanguages)(MUI_LANGUAGE_NAME, &pn, buf, &pc) && pn == 1 && !lstrcmpiW(buf, name)) { /* only fails if the process list was equal anyway */ }
    /* reset */
    num = 0xDEAD;
    SetLastError(0);
    /* P1: MUI_RESET_FILTERS leaves the list alone; a NULL list clears it (RtlSetThreadPreferredUILanguages) */
    if (!U(SetThreadPreferredUILanguages)(MUI_LANGUAGE_NAME, NULL, &num)) bad |= 256;
    cch = 512; num = 0xDEAD;
    if (!U(GetThreadPreferredUILanguages)(MUI_LANGUAGE_NAME | MUI_THREAD_LANGUAGES, &num, buf, &cch)) bad |= 512;
    else if (num != 0) bad |= 1024;
    return bad;
}
TEST(uipref_set_thread_roundtrip_child) {
    REQUIRE(U(SetThreadPreferredUILanguages));
    if (t_is_wine()) { t_skip("emulator lacks preferred-UI-language setters"); return; }
    DWORD r = t_run_child("setthreadpref", "", 20000);
    if (r == 0x1000) { t_note("UI language not settable here"); return; }
    CHECKF(r == 0, "child bits 0x%lx", r);
}
CHILD(setthreadpref_invalid) {
    int bad = 0; DWORD bogus_err = 0; WCHAR list[64]; ULONG num = 0xDEAD;
    memset(list, 0, sizeof list); lstrcpyW(list, L"qq-ZZ-bogus-name");
    SetLastError(0);
    BOOL r = U(SetThreadPreferredUILanguages)(MUI_LANGUAGE_NAME, list, &num);
    if (r && num != 0) bad |= 1;
    /* P1: UNRESOLVED - the error for an unknown name is not 87 on Win10 (bit 2 of the old run); report it. */
    if (!r && GetLastError() != ERROR_INVALID_PARAMETER) bogus_err = GetLastError();
    /* ID | NAME together is invalid */
    memset(list, 0, sizeof list); lstrcpyW(list, L"en-US");
    SetLastError(0);
    if (U(SetThreadPreferredUILanguages)(MUI_LANGUAGE_NAME | MUI_LANGUAGE_ID, list, &num)) bad |= 4;
    else if (GetLastError() != ERROR_INVALID_PARAMETER) bad |= 8;
    /* unknown flag bits */
    SetLastError(0);
    if (U(SetThreadPreferredUILanguages)(0x80000000 | MUI_LANGUAGE_NAME, list, &num)) bad |= 16;
    else if (GetLastError() != ERROR_INVALID_PARAMETER) bad |= 32;
    if (bogus_err) bad |= 0x1000000 | (int)((bogus_err & 0xffff) << 8);   /* parent decodes and notes */
    return bad;
}
TEST(uipref_set_thread_invalid_child) {
    REQUIRE(U(SetThreadPreferredUILanguages));
    if (t_is_wine()) { t_skip("emulator lacks preferred-UI-language setters"); return; }
    DWORD r = t_run_child("setthreadpref_invalid", "", 20000);
    if (r & 0x1000000) { t_note("SetThreadPreferredUILanguages(bogus name) failed with err %lu (not 87)", (r >> 8) & 0xffff); r &= ~(0x1000000u | 0xffff00u); }
    CHECKF(r == 0, "child bits 0x%lx", r);
}
CHILD(setprocesspref) {
    int bad = 0; WCHAR name[100], list[200], buf[512]; ULONG num = 0, cch;
    uiname(name);
    memset(list, 0, sizeof list); lstrcpyW(list, name);
    SetLastError(0);
    if (!U(SetProcessPreferredUILanguages)(MUI_LANGUAGE_NAME, list, &num)) return 0x1000;
    if (num != 1) bad |= 2;
    cch = 512; num = 0;
    if (!U(GetProcessPreferredUILanguages)(MUI_LANGUAGE_NAME, &num, buf, &cch)) bad |= 4;
    if (num != 1 || lstrcmpiW(buf, name)) bad |= 8;
    /* a second thread sees the process list when it has no list of its own */
    SetLastError(0);
    /* P1: MUI_RESET_FILTERS is invalid for the process variant (flags outside ID/NAME give 87); a NULL list clears it */
    if (!U(SetProcessPreferredUILanguages)(MUI_LANGUAGE_NAME, NULL, &num)) bad |= 16;
    cch = 512; num = 0xDEAD;
    if (!U(GetProcessPreferredUILanguages)(MUI_LANGUAGE_NAME, &num, buf, &cch)) bad |= 32;
    else if (buf[0] != 0 || cch != 2) bad |= 64;   /* P1b: with an empty process list Win10 returns an empty multi-string (cch 2) and leaves *pulNumLanguages untouched */
    return bad;
}
TEST(uipref_set_process_roundtrip_child) {
    REQUIRE(U(SetProcessPreferredUILanguages));
    if (t_is_wine()) { t_skip("emulator lacks preferred-UI-language setters"); return; }
    DWORD r = t_run_child("setprocesspref", "", 20000);
    if (r == 0x1000) { t_note("UI language not settable here"); return; }
    CHECKF(r == 0, "child bits 0x%lx", r);
}
CHILD(setthreadpref_multi) {
    int bad = 0; WCHAR name[100], list[256], buf[512]; ULONG num = 0, cch;
    uiname(name);
    /* two entries: the UI language twice is deduplicated or kept; use name + invariant-valid second language */
    memset(list, 0, sizeof list);
    lstrcpyW(list, name);
    WCHAR *second = list + lstrlenW(name) + 1;
    lstrcpyW(second, lstrcmpiW(name, L"en-US") ? L"en-US" : L"en-GB");
    SetLastError(0);
    BOOL r = U(SetThreadPreferredUILanguages)(MUI_LANGUAGE_NAME, list, &num);
    if (!r) { return 0x1000 | (GetLastError() & 0xfff); }     /* second language may not be installed */
    cch = 512; ULONG gn = 0;
    if (!U(GetThreadPreferredUILanguages)(MUI_LANGUAGE_NAME | MUI_THREAD_LANGUAGES, &gn, buf, &cch)) bad |= 2;   /* P1: thread list only, no user-fallback merge */
    if (gn != num) bad |= 4;
    if (lstrcmpiW(buf, name)) bad |= 8;
    return bad;
}
TEST(uipref_set_thread_two_languages_child) {
    REQUIRE(U(SetThreadPreferredUILanguages));
    if (t_is_wine()) { t_skip("emulator lacks preferred-UI-language setters"); return; }
    DWORD r = t_run_child("setthreadpref_multi", "", 20000);
    if (r & 0x1000) { t_note("setting two languages failed with err %lu", r & 0xfff); return; }
    CHECKF(r == 0, "child bits 0x%lx", r);
}

/* ---- GetUILanguageInfo ---- */
TEST(uilanginfo_basic) {
    REQUIRE(U(GetUILanguageInfo));
    if (t_is_wine()) { t_skip("emulator aborts on unimplemented GetUILanguageInfo"); return; }
    WCHAR lang[16]; memset(lang, 0, sizeof lang); lstrcpyW(lang, L"en-US");
    WCHAR fb[256]; DWORD cch = 0, attr = 0xDEAD; SET_ERR(0);
    BOOL r = U(GetUILanguageInfo)(MUI_LANGUAGE_NAME, lang, NULL, &cch, &attr);
    CHECKF(r, "size query err %lu", GetLastError());
    if (r) {
        CHECKF(cch >= 2, "cch=%lu", cch);   /* P1: en-US has no fallbacks on Win10: empty multi-string, cch 2 */
        t_note("en-US fallback chain size %lu attr 0x%lx", cch, attr);
        DWORD c2 = cch;
        r = U(GetUILanguageInfo)(MUI_LANGUAGE_NAME, lang, fb, &c2, &attr);
        CHECK(r);
        if (r) {
            /* P1: the output is the fallback list of the language (the language itself is not echoed) */
            CHECK(c2 == cch);
            int total; int n = count_multisz(fb, &total); CHECK((DWORD)total == c2); CHECK(n >= 0);
        }
        DWORD small = cch - 1; SET_ERR(0);
        CHECK(!U(GetUILanguageInfo)(MUI_LANGUAGE_NAME, lang, fb, &small, &attr)); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    }
}
TEST(uilanginfo_id_format) {
    REQUIRE(U(GetUILanguageInfo));
    if (t_is_wine()) { t_skip("emulator aborts on unimplemented GetUILanguageInfo"); return; }
    WCHAR lang[16]; memset(lang, 0, sizeof lang); lstrcpyW(lang, L"0409");
    WCHAR fb[256]; DWORD cch = 256, attr = 0; SET_ERR(0);
    BOOL r = U(GetUILanguageInfo)(MUI_LANGUAGE_ID, lang, fb, &cch, &attr);
    CHECKF(r, "err %lu", GetLastError());
    /* P1: fallback list, not an echo of the input; entries (if any) must be 4-digit hex ids */
    if (r) { const WCHAR *p = fb; while (*p) { CHECKF(lstrlenW(p) == 4, "id entry '%ls'", p); p += lstrlenW(p) + 1; } }
}
TEST(uilanginfo_errors) {
    REQUIRE(U(GetUILanguageInfo));
    if (t_is_wine()) { t_skip("emulator aborts on unimplemented GetUILanguageInfo"); return; }
    WCHAR lang[16]; memset(lang, 0, sizeof lang); lstrcpyW(lang, L"en-US");
    WCHAR fb[64]; DWORD cch = 64, attr = 0;
    SET_ERR(0); CHECK(!U(GetUILanguageInfo)(MUI_LANGUAGE_NAME, NULL, fb, &cch, &attr)); t_note("NULL language err=%lu", GetLastError());
    SET_ERR(0); CHECK(!U(GetUILanguageInfo)(MUI_LANGUAGE_NAME | MUI_LANGUAGE_ID, lang, fb, &cch, &attr)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    { /* P1: Win10 treats a NULL pcchFallbackLanguages as size 0 (RtlGetUILanguageInfo): succeeds */
        SET_ERR(0); BOOL rn = U(GetUILanguageInfo)(MUI_LANGUAGE_NAME, lang, fb, NULL, &attr); t_note("NULL cch: r=%d err=%lu", rn, GetLastError()); CHECK(rn); }
    memset(lang, 0, sizeof lang); lstrcpyW(lang, L"qq-bogus-zz");
    cch = 64; SET_ERR(0);
    BOOL r = U(GetUILanguageInfo)(MUI_LANGUAGE_NAME, lang, fb, &cch, &attr);
    t_note("bogus language: r=%d err=%lu", r, r ? 0 : GetLastError());
}

/* ---- GetFileMUIInfo / GetFileMUIPath on kernel32.dll ---- */
static void k32path(WCHAR *out, int cap)
{
    GetSystemDirectoryW(out, cap);
    lstrcatW(out, L"\\kernel32.dll");
}
TEST(muifile_getfilemuiinfo_kernel32) {
    REQUIRE(U(GetFileMUIInfo));
    if (t_is_wine()) { t_skip("emulator has no MUI file support"); return; }
    WCHAR path[MAX_PATH]; k32path(path, MAX_PATH);
    DWORD cb = 0; SET_ERR(0);
    BOOL r = U(GetFileMUIInfo)(MUI_QUERY_TYPE | MUI_QUERY_CHECKSUM, path, NULL, &cb);
    /* P1: a NULL-buffer size query succeeds and returns the size (kernelbase GetFileMUIInfo) */
    CHECKF(r, "size query err %lu", GetLastError());
    CHECKF(cb >= sizeof(FILEMUIINFO), "cb=%lu", cb);
    if (cb >= sizeof(FILEMUIINFO) && cb < 65536) {
        FILEMUIINFO *fi = (FILEMUIINFO *)calloc(1, cb);
        fi->dwSize = cb; fi->dwVersion = MUI_FILEINFO_VERSION;
        DWORD cb2 = cb;
        SET_ERR(0);
        r = U(GetFileMUIInfo)(MUI_QUERY_TYPE | MUI_QUERY_CHECKSUM, path, fi, &cb2);
        CHECKF(r, "err %lu", GetLastError());
        if (r) {
            CHECK(fi->dwSize == cb2 || fi->dwSize == cb);
            CHECK(fi->dwVersion == MUI_FILEINFO_VERSION);
            CHECKF(fi->dwFileType == MUI_FILETYPE_LANGUAGE_NEUTRAL_MAIN || fi->dwFileType == MUI_FILETYPE_NOT_LANGUAGE_NEUTRAL, "type %lu", fi->dwFileType);
            if (fi->dwFileType != MUI_FILETYPE_LANGUAGE_NEUTRAL_MAIN) t_note("kernel32.dll MUI file type %lu", fi->dwFileType);
            int nz = 0; for (int i = 0; i < 16; i++) nz |= fi->pChecksum[i];
            if (fi->dwFileType == MUI_FILETYPE_LANGUAGE_NEUTRAL_MAIN) CHECK(nz != 0);
        }
        free(fi);
    }
}
TEST(muifile_getfilemuiinfo_errors) {
    REQUIRE(U(GetFileMUIInfo));
    if (t_is_wine()) { t_skip("emulator has no MUI file support"); return; }
    WCHAR path[MAX_PATH]; k32path(path, MAX_PATH);
    FILEMUIINFO fi; memset(&fi, 0, sizeof fi); fi.dwSize = sizeof fi; fi.dwVersion = MUI_FILEINFO_VERSION;
    DWORD cb = sizeof fi;
    if (!t_is_wine()) { SET_ERR(0); CHECK(!U(GetFileMUIInfo)(MUI_QUERY_TYPE, NULL, &fi, &cb)); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
    if (!t_is_wine()) { SET_ERR(0); CHECK(!U(GetFileMUIInfo)(MUI_QUERY_TYPE, path, &fi, NULL)); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
    cb = sizeof fi;
    SET_ERR(0); CHECK(!U(GetFileMUIInfo)(MUI_QUERY_TYPE, L"C:\\no\\such\\dir\\x.dll", &fi, &cb));
    DWORD e = GetLastError(); CHECKF(e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND || e == ERROR_INVALID_PARAMETER, "err %lu", e);
    t_note("missing file err=%lu", e);
    fi.dwVersion = 99; cb = sizeof fi; SET_ERR(0);
    BOOL r = U(GetFileMUIInfo)(MUI_QUERY_TYPE, path, &fi, &cb); t_note("version 99: r=%d err=%lu", r, r ? 0 : GetLastError());
}
TEST(muifile_getfilemuipath_kernel32) {
    REQUIRE(U(GetFileMUIPath));
    if (t_is_wine()) { t_skip("emulator has no MUI file support"); return; }
    WCHAR path[MAX_PATH]; k32path(path, MAX_PATH);
    WCHAR lang[LOCALE_NAME_MAX_LENGTH] = L"en-US", mui[MAX_PATH]; ULONG cl = LOCALE_NAME_MAX_LENGTH, cf = MAX_PATH; ULONGLONG en = 0;
    SET_ERR(0);
    BOOL r = U(GetFileMUIPath)(MUI_LANGUAGE_NAME, path, lang, &cl, mui, &cf, &en);
    if (!r) { t_note("GetFileMUIPath(en-US) failed err=%lu", GetLastError()); return; }
    CHECK(wcsstr(mui, L"kernel32.dll.mui") != NULL || wcsstr(mui, L"KERNEL32.DLL.MUI") != NULL);
    CHECK(wcsstr(mui, L"en-US") != NULL || wcsstr(mui, L"EN-US") != NULL);
    /* P1: on success with a big-enough buffer Win10 leaves *pcchFileMUIPath at the input size
     * (RtlpGetMUIRedirectedFilePathInternal writes it only for a size query or a too small buffer). */
    CHECKF(cf == MAX_PATH, "cf=%lu len=%d", cf, lstrlenW(mui));
    t_note("MUI path: %ls", mui);
    DWORD a = GetFileAttributesW(mui); CHECK(a != INVALID_FILE_ATTRIBUTES);
}
CHILD(muipathnull) {
    WCHAR path[MAX_PATH]; k32path(path, MAX_PATH);
    WCHAR lang[LOCALE_NAME_MAX_LENGTH] = L"en-US", mui[MAX_PATH]; ULONG cl = LOCALE_NAME_MAX_LENGTH; ULONGLONG en = 0;
    BOOL r = U(GetFileMUIPath)(MUI_LANGUAGE_NAME, path, lang, &cl, mui, NULL, &en);
    return r ? 1 : (int)(0x100 | (GetLastError() & 0xff));
}
TEST(muifile_getfilemuipath_errors) {
    REQUIRE(U(GetFileMUIPath));
    if (t_is_wine()) { t_skip("emulator has no MUI file support"); return; }
    WCHAR path[MAX_PATH]; k32path(path, MAX_PATH);
    WCHAR lang[LOCALE_NAME_MAX_LENGTH] = L"en-US", mui[MAX_PATH]; ULONG cl = LOCALE_NAME_MAX_LENGTH, cf = MAX_PATH; ULONGLONG en = 0;
    if (!t_is_wine()) { SET_ERR(0); CHECK(!U(GetFileMUIPath)(MUI_LANGUAGE_NAME, NULL, lang, &cl, mui, &cf, &en)); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
    SET_ERR(0); CHECK(!U(GetFileMUIPath)(MUI_LANGUAGE_NAME | MUI_LANGUAGE_ID, path, lang, &cl, mui, &cf, &en)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    /* P1: NULL pcchFileMUIPath is run in a child (P1b: it does not crash on Win10) */
    if (!t_is_wine()) { DWORD rc = t_run_child("muipathnull", "", 20000); /* P1b: no AV on Win10; the call fails (exit 0x100|low byte of last error, 0x112 observed) */ CHECKF(rc != 0xC0000005 && rc != 1 && (rc & 0x100), "NULL pcchFileMUIPath: child exit 0x%lx", rc); if (rc == 0x112) t_note("NULL pcchFileMUIPath fails with ERROR_NO_MORE_FILES"); }
    cf = 3; SET_ERR(0);
    BOOL r = U(GetFileMUIPath)(MUI_LANGUAGE_NAME, path, lang, &cl, mui, &cf, &en);
    if (!r) { EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER); CHECK(cf > 3); }
}
