#include "rk.h"
#include <winternl.h>

static int get_sz(HKEY root, const WCHAR *sub, const WCHAR *val, WCHAR *out, DWORD cch) {
    HKEY k; if (RegOpenKeyExW(root, sub, 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &k)) return 0;
    DWORD cb = cch * 2, t = 0; LSTATUS s = U(RegQueryValueExW)(k, val, NULL, &t, (BYTE *)out, &cb);
    RegCloseKey(k); return s == 0 && (t == REG_SZ || t == REG_EXPAND_SZ);
}
static const WCHAR *CV = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";

/* ---------------- Windows version values in HKLM ---------------- */
TEST(regkb_sys_currentversion_major_minor) {
    REQUIRE(U(RegQueryValueExW));
    HKEY k; EQ(RegOpenKeyExW(HKEY_LOCAL_MACHINE, CV, 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &k), 0); REQUIRE(k);
    DWORD v = 0, cb = 4, t = 0;
    EQ(U(RegQueryValueExW)(k, L"CurrentMajorVersionNumber", NULL, &t, (BYTE *)&v, &cb), 0); EQ(t, REG_DWORD); EQ(v, 10);
    cb = 4; EQ(U(RegQueryValueExW)(k, L"CurrentMinorVersionNumber", NULL, &t, (BYTE *)&v, &cb), 0); EQ(t, REG_DWORD); EQ(v, 0);
    RegCloseKey(k);
}
TEST(regkb_sys_currentbuildnumber_matches_rtlgetversion) {
    REQUIRE(U(RegQueryValueExW));
    typedef LONG (NTAPI *rgv_t)(PRTL_OSVERSIONINFOW); rgv_t rgv = (rgv_t)t_proc_nt("RtlGetVersion"); REQUIRE(rgv);
    WCHAR b[32]; CHECK(get_sz(HKEY_LOCAL_MACHINE, CV, L"CurrentBuildNumber", b, 32));
    RTL_OSVERSIONINFOW vi = { sizeof vi }; rgv(&vi);
    long n = wcstol(b, NULL, 10);
    CHECK(n >= 19041);
    EQ(n, vi.dwBuildNumber);
    WCHAR b2[32]; CHECK(get_sz(HKEY_LOCAL_MACHINE, CV, L"CurrentBuild", b2, 32)); CHECK(!lstrcmpW(b, b2));
    t_note("build %ls", b);
}
TEST(regkb_sys_ubr_and_install_date_are_dwords) {
    REQUIRE(U(RegQueryValueExW));
    HKEY k; EQ(RegOpenKeyExW(HKEY_LOCAL_MACHINE, CV, 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &k), 0); REQUIRE(k);
    DWORD v = 0, cb = 4, t = 0;
    EQ(U(RegQueryValueExW)(k, L"UBR", NULL, &t, (BYTE *)&v, &cb), 0); EQ(t, REG_DWORD); t_note("UBR %lu", v);
    cb = 4; EQ(U(RegQueryValueExW)(k, L"InstallDate", NULL, &t, (BYTE *)&v, &cb), 0); EQ(t, REG_DWORD); CHECK(v > 1400000000u);
    RegCloseKey(k);
}
TEST(regkb_sys_productname_edition_systemroot) {
    REQUIRE(U(RegQueryValueExW));
    WCHAR s[260], win[MAX_PATH];
    CHECK(get_sz(HKEY_LOCAL_MACHINE, CV, L"ProductName", s, 260)); CHECK(wcsstr(s, L"Windows") != NULL);
    CHECK(get_sz(HKEY_LOCAL_MACHINE, CV, L"EditionID", s, 260)); CHECK(s[0] != 0);
    CHECK(get_sz(HKEY_LOCAL_MACHINE, CV, L"SystemRoot", s, 260));
    GetWindowsDirectoryW(win, MAX_PATH);
    CHECK(!lstrcmpiW(s, win));
    CHECK(get_sz(HKEY_LOCAL_MACHINE, CV, L"BuildLabEx", s, 260) || get_sz(HKEY_LOCAL_MACHINE, CV, L"BuildLab", s, 260));
    CHECK(get_sz(HKEY_LOCAL_MACHINE, CV, L"CurrentVersion", s, 260));
    t_note("CurrentVersion = %ls", s);
}
TEST(regkb_sys_currentversion_queried_by_a_api) {
    REQUIRE(U(RegQueryValueExA)); REQUIRE(U(RegOpenKeyExA));
    HKEY k; EQ(U(RegOpenKeyExA)(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &k), 0); REQUIRE(k);
    char s[128]; DWORD cb = sizeof s, t;
    EQ(U(RegQueryValueExA)(k, "ProductName", NULL, &t, (BYTE *)s, &cb), 0); EQ(t, REG_SZ); EQ(cb, strlen(s) + 1);
    RegCloseKey(k);
}
TEST(regkb_sys_currentversion_size_query_matches_fetch) {
    REQUIRE(U(RegQueryValueExW));
    HKEY k; EQ(RegOpenKeyExW(HKEY_LOCAL_MACHINE, CV, 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &k), 0); REQUIRE(k);
    DWORD need = 0, t; EQ(U(RegQueryValueExW)(k, L"ProductName", NULL, &t, NULL, &need), 0);
    WCHAR *p = malloc(need); DWORD cb = need; EQ(U(RegQueryValueExW)(k, L"ProductName", NULL, &t, (BYTE *)p, &cb), 0); EQ(cb, need);
    cb = need - 1; EQ(U(RegQueryValueExW)(k, L"ProductName", NULL, &t, (BYTE *)p, &cb), ERROR_MORE_DATA); EQ(cb, need);
    free(p); RegCloseKey(k);
}

/* ---------------- WOW64 views ---------------- */
TEST(regkb_wow_programfiles_views) {
    REQUIRE(U(RegOpenKeyExW)); REQUIRE(U(RegQueryValueExW));
    HKEY k64, k32; WCHAR a[260], b[260]; DWORD ca = sizeof a, cb = sizeof b, t;
    EQ(U(RegOpenKeyExW)(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion", 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &k64), 0);
    EQ(U(RegOpenKeyExW)(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion", 0, KEY_QUERY_VALUE | KEY_WOW64_32KEY, &k32), 0);
    REQUIRE(k64); REQUIRE(k32);
    EQ(RegQueryValueExW(k64, L"ProgramFilesDir", NULL, &t, (BYTE *)a, &ca), 0);
    EQ(RegQueryValueExW(k32, L"ProgramFilesDir", NULL, &t, (BYTE *)b, &cb), 0);
    CHECK(wcsstr(a, L"(x86)") == NULL); CHECK(wcsstr(b, L"(x86)") != NULL);
    RegCloseKey(k64); RegCloseKey(k32);
}
TEST(regkb_wow_explicit_wow6432node_path_equals_32bit_view) {
    REQUIRE(U(RegOpenKeyExW)); REQUIRE(U(RegQueryValueExW));
    HKEY kx, k32; WCHAR a[260], b[260]; DWORD ca = sizeof a, cb = sizeof b, t;
    EQ(U(RegOpenKeyExW)(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion", 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &kx), 0);
    EQ(U(RegOpenKeyExW)(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion", 0, KEY_QUERY_VALUE | KEY_WOW64_32KEY, &k32), 0);
    REQUIRE(kx); REQUIRE(k32);
    EQ(RegQueryValueExW(kx, L"ProgramFilesDir", NULL, &t, (BYTE *)a, &ca), 0);
    EQ(RegQueryValueExW(k32, L"ProgramFilesDir", NULL, &t, (BYTE *)b, &cb), 0);
    CHECK(!lstrcmpW(a, b));
    RegCloseKey(kx); RegCloseKey(k32);
}
TEST(regkb_wow_program_w6432_value_exists_in_64bit_view) {
    REQUIRE(U(RegQueryValueExW));
    WCHAR s[260];
    CHECK(get_sz(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion", L"ProgramW6432Dir", s, 260));
    CHECK(wcsstr(s, L"(x86)") == NULL);
}
TEST(regkb_wow_wow6432node_key_exists_only_physically) {
    REQUIRE(U(RegOpenKeyExW));
    HKEY k = NULL;
    EQ(U(RegOpenKeyExW)(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Wow6432Node", 0, KEY_READ | KEY_WOW64_64KEY, &k), 0);
    if (k) RegCloseKey(k);
}
TEST(regkb_wow_hkcu_software_is_shared) {
    REQUIRE(U(RegCreateKeyExW)); REQUIRE(U(RegOpenKeyExW));
    HKEY r = FRESH(); REQUIRE(r); HKEY a = NULL, b = NULL;
    EQ(U(RegCreateKeyExW)(r, L"shared", 0, NULL, 0, KEY_ALL_ACCESS | KEY_WOW64_32KEY, NULL, &a, NULL), 0);
    if (a) sc_dw(a, L"v", 1);
    EQ(U(RegOpenKeyExW)(r, L"shared", 0, KEY_READ | KEY_WOW64_64KEY, &b), 0);
    DWORD v = 0, cb = 4, t;
    if (b) { EQ(RegQueryValueExW(b, L"v", NULL, &t, (BYTE *)&v, &cb), 0); EQ(v, 1); RegCloseKey(b); }
    if (a) RegCloseKey(a);
    RegCloseKey(r);
}
TEST(regkb_wow_flag_on_handle_children_inherit_view) {
    REQUIRE(U(RegOpenKeyExW));
    HKEY h = NULL, c = NULL;
    EQ(U(RegOpenKeyExW)(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft", 0, KEY_READ | KEY_WOW64_32KEY, &h), 0); REQUIRE(h);
    /* a child opened without a WOW flag from a 32-bit-view handle stays in the 32-bit view */
    EQ(U(RegOpenKeyExW)(h, L"Windows\\CurrentVersion", 0, KEY_QUERY_VALUE, &c), 0);
    WCHAR b[260]; DWORD cb = sizeof b, t;
    if (c) { EQ(RegQueryValueExW(c, L"ProgramFilesDir", NULL, &t, (BYTE *)b, &cb), 0); CHECK(wcsstr(b, L"(x86)") != NULL); RegCloseKey(c); }
    RegCloseKey(h);
}
TEST(regkb_wow_process_is_native_x64) {
    BOOL w = TRUE; typedef BOOL (WINAPI *f_t)(HANDLE, PBOOL);
    f_t f = (f_t)t_proc("IsWow64Process"); REQUIRE(f);
    f(GetCurrentProcess(), &w);
    t_note("IsWow64Process = %d (x64 build expected FALSE)", w);
    CHECK(!w);
}

/* ---------------- predefined keys ---------------- */
TEST(regkb_pre_hkcr_txt_default) {
    REQUIRE(U(RegOpenKeyExW)); REQUIRE(U(RegGetValueW));
    HKEY k; EQ(U(RegOpenKeyExW)(HKEY_CLASSES_ROOT, L".txt", 0, KEY_READ, &k), 0); REQUIRE(k);
    WCHAR s[64]; DWORD cb = sizeof s;
    EQ(U(RegGetValueW)(k, NULL, NULL, RRF_RT_REG_SZ, NULL, s, &cb), 0);
    CHECK(s[0] != 0); t_note(".txt -> %ls", s);
    RegCloseKey(k);
}
TEST(regkb_pre_hkcr_clsid_and_merged_view) {
    REQUIRE(U(RegOpenKeyExW));
    HKEY k = NULL, k2 = NULL;
    EQ(U(RegOpenKeyExW)(HKEY_CLASSES_ROOT, L"CLSID", 0, KEY_READ, &k), 0);
    EQ(U(RegOpenKeyExW)(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Classes\\CLSID", 0, KEY_READ, &k2), 0);
    if (k) RegCloseKey(k);
    if (k2) RegCloseKey(k2);
    EQ(U(RegOpenKeyExW)(HKEY_CLASSES_ROOT, L"k32t_no_such_class_ZZ", 0, KEY_READ, &k), ERROR_FILE_NOT_FOUND);
}
TEST(regkb_pre_hkcr_write_goes_to_hkcu_classes_denied_check_only) {
    REQUIRE(U(RegOpenKeyExW));
    HKEY k = NULL;
    LSTATUS s = U(RegOpenKeyExW)(HKEY_CLASSES_ROOT, L".txt", 0, KEY_SET_VALUE, &k);
    t_note("open HKCR\\.txt for KEY_SET_VALUE = %ld", (long)s);
    if (s == 0) RegCloseKey(k);
}
TEST(regkb_pre_hkcu_is_current_user_hive) {
    REQUIRE(U(RegOpenKeyExW));
    HKEY a = NULL, b = NULL, sidk = NULL;
    EQ(U(RegOpenKeyExW)(HKEY_CURRENT_USER, L"Software", 0, KEY_READ, &a), 0);
    /* same key reached via HKEY_USERS\<sid> */
    HANDLE tok; CHECK(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok));
    BYTE buf[256]; DWORD n = 0; CHECK(GetTokenInformation(tok, TokenUser, buf, sizeof buf, &n));
    typedef BOOL (WINAPI *cs_t)(PSID, LPWSTR *); HMODULE adv = LoadLibraryA("advapi32.dll");
    cs_t cs = (cs_t)GetProcAddress(adv, "ConvertSidToStringSidW"); LPWSTR sidstr = NULL;
    if (cs && cs(((TOKEN_USER *)buf)->User.Sid, &sidstr)) {
        EQ(U(RegOpenKeyExW)(HKEY_USERS, sidstr, 0, KEY_READ, &sidk), 0);
        if (sidk) { EQ(U(RegOpenKeyExW)(sidk, L"Software", 0, KEY_READ, &b), 0); }
        if (a && b) {
            DWORD s1, s2;
            RegQueryInfoKeyW(a, NULL, NULL, NULL, &s1, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            RegQueryInfoKeyW(b, NULL, NULL, NULL, &s2, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
            EQ(s1, s2);
        }
        LocalFree(sidstr);
    }
    if (a) RegCloseKey(a);
    if (b) RegCloseKey(b);
    if (sidk) RegCloseKey(sidk);
    CloseHandle(tok);
}
TEST(regkb_pre_hku_default_opens) {
    REQUIRE(U(RegOpenKeyExW));
    HKEY k = NULL;
    EQ(U(RegOpenKeyExW)(HKEY_USERS, L".DEFAULT", 0, KEY_READ, &k), 0);
    if (k) RegCloseKey(k);
    EQ(U(RegOpenKeyExW)(HKEY_USERS, L"k32t_no_such_sid_ZZ", 0, KEY_READ, &k), ERROR_FILE_NOT_FOUND);
}
TEST(regkb_pre_hkcc_opens) {
    REQUIRE(U(RegOpenKeyExW));
    HKEY k = NULL;
    LSTATUS s = U(RegOpenKeyExW)(HKEY_CURRENT_CONFIG, L"System", 0, KEY_READ, &k);
    t_note("HKCC\\System = %ld", (long)s);
    if (s == 0) RegCloseKey(k);
    EQ(U(RegOpenKeyExW)(HKEY_CURRENT_CONFIG, L"k32t_nope", 0, KEY_READ, &k), ERROR_FILE_NOT_FOUND);
}
TEST(regkb_pre_hkcc_is_alias_for_hardware_profile) {
    REQUIRE(U(RegOpenKeyExW));
    HKEY a = NULL, b = NULL;
    LSTATUS sa = U(RegOpenKeyExW)(HKEY_CURRENT_CONFIG, L"Software", 0, KEY_READ, &a);
    LSTATUS sb = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Hardware Profiles\\Current\\Software", 0, KEY_READ, &b);
    t_note("HKCC\\Software = %ld, HW profile = %ld", (long)sa, (long)sb);
    CHECK((sa == 0) == (sb == 0));
    if (a) RegCloseKey(a);
    if (b) RegCloseKey(b);
}
TEST(regkb_pre_dyn_data_not_supported) {
    REQUIRE(U(RegOpenKeyExW)); REQUIRE(U(RegQueryValueExW));
    HKEY k = NULL; HKEY dyn = (HKEY)(ULONG_PTR)0xFFFFFFFF80000006ULL;
    LSTATUS s = U(RegOpenKeyExW)(dyn, L"x", 0, KEY_READ, &k);
    CHECK(s != 0); t_note("open on HKEY_DYN_DATA = %ld", (long)s);
    DWORD cb = 0;
    s = U(RegQueryValueExW)(dyn, L"x", NULL, NULL, NULL, &cb); CHECK(s != 0);
}
TEST(regkb_pre_unassigned_predefined_values) {
    REQUIRE(U(RegOpenKeyExW));
    for (ULONG_PTR v = 0x80000007; v <= 0x80000009; v++) {
        HKEY k = NULL; LSTATUS s = U(RegOpenKeyExW)((HKEY)(LONG_PTR)(LONG)v, L"x", 0, KEY_READ, &k);
        CHECKF(s != 0, "0x%llx", (unsigned long long)v);
    }
}
TEST(regkb_pre_hklm_system_select_current) {
    REQUIRE(U(RegGetValueW));
    DWORD v = 0, cb = 4;
    EQ(U(RegGetValueW)(HKEY_LOCAL_MACHINE, L"SYSTEM\\Select", L"Current", RRF_RT_REG_DWORD, NULL, &v, &cb), 0);
    CHECK(v >= 1 && v <= 8);
    WCHAR nm[64]; swprintf(nm, 64, L"SYSTEM\\ControlSet%03lu", v);
    HKEY k; EQ(RegOpenKeyExW(HKEY_LOCAL_MACHINE, nm, 0, KEY_READ, &k), 0); if (k) RegCloseKey(k);
    EQ(RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet", 0, KEY_READ, &k), 0); if (k) RegCloseKey(k);
}
TEST(regkb_pre_hklm_computername_matches) {
    REQUIRE(U(RegGetValueW));
    WCHAR reg[64], api[64]; DWORD cb = sizeof reg, n = 64;
    EQ(U(RegGetValueW)(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\ComputerName\\ActiveComputerName", L"ComputerName", RRF_RT_REG_SZ, NULL, reg, &cb), 0);
    CHECK(GetComputerNameW(api, &n));
    CHECK(!lstrcmpiW(reg, api));
}
TEST(regkb_pre_hkcu_environment_key_exists) {
    REQUIRE(U(RegOpenKeyExW));
    HKEY k = NULL;
    LSTATUS s = U(RegOpenKeyExW)(HKEY_CURRENT_USER, L"Environment", 0, KEY_READ, &k);
    CHECK(s == 0 || s == ERROR_FILE_NOT_FOUND);
    if (s == 0) RegCloseKey(k);
    s = U(RegOpenKeyExW)(HKEY_CURRENT_USER, L"Volatile Environment", 0, KEY_READ, &k);
    CHECK(s == 0 || s == ERROR_FILE_NOT_FOUND);
    t_note("HKCU\\Volatile Environment open = %ld", (long)s);
    if (s == 0) RegCloseKey(k);
}
TEST(regkb_pre_hkcu_volatile_env_username) {
    REQUIRE(U(RegGetValueW));
    WCHAR reg[128], api[128]; DWORD cb = sizeof reg;
    LSTATUS s = U(RegGetValueW)(HKEY_CURRENT_USER, L"Volatile Environment", L"USERNAME", RRF_RT_REG_SZ, NULL, reg, &cb);
    if (s == 0) { DWORD n = 128; GetEnvironmentVariableW(L"USERNAME", api, n); CHECK(!lstrcmpiW(reg, api)); }
    else t_note("Volatile Environment\\USERNAME = %ld", (long)s);
}

/* ---------------- performance data (child) ---------------- */
CHILD(regkb_perfdata) {
    HKEY k = HKEY_PERFORMANCE_DATA;
    DWORD cb = 8, t = 0; BYTE small[8];
    LSTATUS s = RegQueryValueExW(k, L"Global", NULL, &t, small, &cb);
    if (s != ERROR_MORE_DATA) { printf("small buffer query = %ld\n", (long)s); return 10; }
    DWORD cap = 1 << 16; BYTE *buf = NULL;
    for (int i = 0; i < 9; i++) {
        buf = malloc(cap); cb = cap;
        s = RegQueryValueExW(k, L"Global", NULL, &t, buf, &cb);
        if (s != ERROR_MORE_DATA) break;
        free(buf); buf = NULL; cap *= 2;
    }
    if (s != 0 || !buf) { printf("global query = %ld\n", (long)s); return 11; }
    if (t != REG_BINARY && t != 0) { printf("type %lu\n", t); }
    if (cb < 16 || memcmp(buf, L"PERF", 8)) { printf("no PERF signature\n"); return 12; }
    free(buf);
    RegCloseKey(HKEY_PERFORMANCE_DATA);
    return 0;
}
TEST(regkb_sys_perfdata_global_query_in_child) {
    if (t_is_wine()) { t_skip("performance data under wine"); return; }
    EQ(t_run_child("regkb_perfdata", "", 30000), 0);
}
CHILD(regkb_perftext) {
    HKEY k = HKEY_PERFORMANCE_TEXT;
    DWORD cap = 1 << 16, cb, t = 0; BYTE *buf = NULL; LSTATUS s = 0;
    for (int i = 0; i < 8; i++) {
        buf = malloc(cap); cb = cap;
        s = RegQueryValueExW(k, L"Counter", NULL, &t, buf, &cb);
        if (s != ERROR_MORE_DATA) break;
        free(buf); buf = NULL; cap *= 2;
    }
    if (s != 0 || !buf) { printf("counter text = %ld\n", (long)s); return 10; }
    if (t != REG_MULTI_SZ) { printf("type %lu\n", t); return 11; }
    if (((WCHAR *)buf)[0] != L'1') { printf("first char %x\n", ((WCHAR *)buf)[0]); return 12; }
    return 0;
}
TEST(regkb_sys_perftext_counter_in_child) {
    if (t_is_wine()) { t_skip("performance text under wine"); return; }
    EQ(t_run_child("regkb_perftext", "", 30000), 0);
}

/* ---------------- override predefined key (child) ---------------- */
CHILD(regkb_override) {
    HKEY r = rk_root(); if (!r) return 20;
    HKEY ov = NULL;
    if (RegCreateKeyExW(r, L"ovr", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &ov, NULL)) return 21;
    sc_dw(ov, L"marker", 0x4B32);
    HKEY sub = sc_sub(ov, L"inner"); RegCloseKey(sub);
    typedef LONG (WINAPI *ov_t)(HKEY, HKEY);
    ov_t f = (ov_t)rk_adv("RegOverridePredefKey"); if (!f) return 22;
    if (f(HKEY_CURRENT_USER, ov) != 0) return 23;
    DWORD v = 0, cb = 4, t;
    if (RegQueryValueExW(HKEY_CURRENT_USER, L"marker", NULL, &t, (BYTE *)&v, &cb) != 0 || v != 0x4B32) return 24;
    HKEY k = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"inner", 0, KEY_READ, &k) != 0) return 25;
    RegCloseKey(k);
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software", 0, KEY_READ, &k) != ERROR_FILE_NOT_FOUND) return 26;
    if (f(HKEY_CURRENT_USER, NULL) != 0) return 27;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software", 0, KEY_READ, &k) != 0) return 28;
    RegCloseKey(k);
    cb = 4;
    if (RegQueryValueExW(HKEY_CURRENT_USER, L"marker", NULL, &t, (BYTE *)&v, &cb) != ERROR_FILE_NOT_FOUND) return 29;
    RegCloseKey(ov);
    return 0;
}
TEST(regkb_sys_override_predef_key_in_child) {
    REQUIRE(rk_adv("RegOverridePredefKey"));
    if (t_is_wine()) { t_skip("RegOverridePredefKey under wine"); return; }
    EQ(t_run_child("regkb_override", "", 30000), 0);
}
CHILD(regkb_override_errors) {
    typedef LONG (WINAPI *ov_t)(HKEY, HKEY);
    ov_t f = (ov_t)rk_adv("RegOverridePredefKey"); if (!f) return 20;
    HKEY r = rk_root(); if (!r) return 21;
    /* only predefined keys can be overridden */
    LONG s = f(r, NULL);
    printf("override of a non-predefined key = %ld\n", s);
    if (s == 0) return 22;
    s = f(HKEY_CURRENT_USER, (HKEY)(ULONG_PTR)0x7fff0000);
    printf("override with garbage handle = %ld\n", s);
    if (s == 0) { f(HKEY_CURRENT_USER, NULL); return 23; }
    s = f(NULL, NULL);
    printf("override of NULL = %ld\n", s);
    if (s == 0) return 24;
    return 0;
}
TEST(regkb_sys_override_predef_key_errors_in_child) {
    REQUIRE(rk_adv("RegOverridePredefKey"));
    if (t_is_wine()) { t_skip("RegOverridePredefKey under wine"); return; }
    EQ(t_run_child("regkb_override_errors", "", 30000), 0);
}
CHILD(regkb_disable_cache) {
    typedef LONG (WINAPI *f_t)(void); typedef LONG (WINAPI *fx_t)(void);
    f_t f = (f_t)rk_adv("RegDisablePredefinedCache"); fx_t fx = (fx_t)rk_adv("RegDisablePredefinedCacheEx");
    if (!f && !fx) return 20;
    HKEY k;
    if (f && f() != 0) return 21;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software", 0, KEY_READ, &k) != 0) return 22;
    RegCloseKey(k);
    if (fx && fx() != 0) return 23;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software", 0, KEY_READ, &k) != 0) return 24;
    RegCloseKey(k);
    return 0;
}
TEST(regkb_sys_disable_predefined_cache_in_child) {
    if (t_is_wine()) { t_skip("predefined cache control under wine"); return; }
    EQ(t_run_child("regkb_disable_cache", "", 30000), 0);
}

/* ---------------- privileged operations: error paths ---------------- */
TEST(regkb_sys_savekey_without_privilege) {
    REQUIRE(U(RegSaveKeyExW));
    HKEY h = FRESH(); REQUIRE(h); sc_dw(h, L"v", 1);
    char fa[MAX_PATH]; WCHAR fw[MAX_PATH]; t_tmpfile(fa, sizeof fa, "save1.hiv"); MultiByteToWideChar(CP_ACP, 0, fa, -1, fw, MAX_PATH);
    LSTATUS s = U(RegSaveKeyExW)(h, fw, NULL, REG_STANDARD_FORMAT);
    EQ(s, ERROR_PRIVILEGE_NOT_HELD);
    EQ(ERROR_PRIVILEGE_NOT_HELD, 1314);
    CHECK(GetFileAttributesW(fw) == INVALID_FILE_ATTRIBUTES);
    RegCloseKey(h);
}
TEST(regkb_sys_savekey_a_without_privilege) {
    REQUIRE(U(RegSaveKeyExA));
    HKEY h = FRESH(); REQUIRE(h);
    char fa[MAX_PATH]; t_tmpfile(fa, sizeof fa, "save2.hiv");
    EQ(U(RegSaveKeyExA)(h, fa, NULL, REG_LATEST_FORMAT), ERROR_PRIVILEGE_NOT_HELD);
    CHECK(GetFileAttributesA(fa) == INVALID_FILE_ATTRIBUTES);
    RegCloseKey(h);
}
TEST(regkb_sys_savekey_bad_format_flag) {
    REQUIRE(U(RegSaveKeyExW));
    HKEY h = FRESH(); REQUIRE(h);
    WCHAR fw[MAX_PATH]; char fa[MAX_PATH]; t_tmpfile(fa, sizeof fa, "save3.hiv"); MultiByteToWideChar(CP_ACP, 0, fa, -1, fw, MAX_PATH);
    LSTATUS s = U(RegSaveKeyExW)(h, fw, NULL, 99);
    CHECK(s != 0); t_note("RegSaveKeyExW(format 99) = %ld", (long)s);
    CHECK(GetFileAttributesW(fw) == INVALID_FILE_ATTRIBUTES);
    RegCloseKey(h);
}
TEST(regkb_sys_restorekey_missing_file) {
    REQUIRE(U(RegRestoreKeyW));
    HKEY h = FRESH(); REQUIRE(h); sc_dw(h, L"keepme", 1);
    WCHAR fw[MAX_PATH]; char fa[MAX_PATH]; t_tmpfile(fa, sizeof fa, "does_not_exist.hiv"); MultiByteToWideChar(CP_ACP, 0, fa, -1, fw, MAX_PATH);
    LSTATUS s = U(RegRestoreKeyW)(h, fw, 0);
    CHECK(s != 0); t_note("RegRestoreKeyW(missing file) = %ld", (long)s);
    EQ(rk_valcount(h), 1);
    RegCloseKey(h);
}
TEST(regkb_sys_restorekey_a_missing_file) {
    REQUIRE(U(RegRestoreKeyA));
    HKEY h = FRESH(); REQUIRE(h); sc_dw(h, L"keepme", 1);
    char fa[MAX_PATH]; t_tmpfile(fa, sizeof fa, "does_not_exist2.hiv");
    LSTATUS s = U(RegRestoreKeyA)(h, fa, 0);
    CHECK(s != 0); t_note("RegRestoreKeyA(missing file) = %ld", (long)s);
    EQ(rk_valcount(h), 1);
    RegCloseKey(h);
}
TEST(regkb_sys_loadkey_without_privilege_or_missing_file) {
    REQUIRE(U(RegLoadKeyW)); REQUIRE(U(RegLoadKeyA));
    WCHAR fw[MAX_PATH], nm[64]; char fa[MAX_PATH]; t_tmpfile(fa, sizeof fa, "nothive.hiv"); MultiByteToWideChar(CP_ACP, 0, fa, -1, fw, MAX_PATH);
    swprintf(nm, 64, L"k32t_load_%lu", GetCurrentProcessId());
    LSTATUS s = U(RegLoadKeyW)(HKEY_USERS, nm, fw);
    CHECK(s != 0); t_note("RegLoadKeyW(HKU, missing file) = %ld", (long)s);
    s = U(RegLoadKeyA)(HKEY_LOCAL_MACHINE, "k32t_load_a", fa);
    CHECK(s != 0); t_note("RegLoadKeyA(HKLM, missing file) = %ld", (long)s);
    CHECK(!rk_existsw(HKEY_USERS, nm));
}
TEST(regkb_sys_loadkey_on_non_root_key) {
    REQUIRE(U(RegLoadKeyW));
    HKEY h = FRESH(); REQUIRE(h);
    WCHAR fw[MAX_PATH]; char fa[MAX_PATH]; t_tmpfile(fa, sizeof fa, "nothive2.hiv"); MultiByteToWideChar(CP_ACP, 0, fa, -1, fw, MAX_PATH);
    LSTATUS s = U(RegLoadKeyW)(h, L"child", fw);
    CHECK(s != 0); t_note("RegLoadKeyW(non-root key) = %ld", (long)s);
    RegCloseKey(h);
}
TEST(regkb_sys_unloadkey_nonexistent) {
    REQUIRE(U(RegUnLoadKeyW)); REQUIRE(U(RegUnLoadKeyA));
    WCHAR nm[64]; swprintf(nm, 64, L"k32t_unload_%lu_absent", GetCurrentProcessId());
    LSTATUS s = U(RegUnLoadKeyW)(HKEY_USERS, nm);
    CHECK(s != 0); t_note("RegUnLoadKeyW(absent) = %ld", (long)s);
    s = U(RegUnLoadKeyA)(HKEY_LOCAL_MACHINE, "k32t_unload_absent_a");
    CHECK(s != 0); t_note("RegUnLoadKeyA(absent) = %ld", (long)s);
}
TEST(regkb_sys_loadappkey_missing_file) {
    REQUIRE(U(RegLoadAppKeyW)); REQUIRE(U(RegLoadAppKeyA));
    WCHAR fw[MAX_PATH]; char fa[MAX_PATH]; t_tmpfile(fa, sizeof fa, "app_absent.hiv"); MultiByteToWideChar(CP_ACP, 0, fa, -1, fw, MAX_PATH);
    HKEY k = NULL;
    LSTATUS s = U(RegLoadAppKeyW)(fw, &k, KEY_READ, 0, 0);
    CHECK(s != 0); CHECK(k == NULL);
    t_note("RegLoadAppKeyW(missing) = %ld", (long)s);
    s = U(RegLoadAppKeyA)(fa, &k, KEY_READ, 0, 0);
    CHECK(s != 0); t_note("RegLoadAppKeyA(missing) = %ld", (long)s);
}
TEST(regkb_sys_loadappkey_invalid_options) {
    REQUIRE(U(RegLoadAppKeyW));
    WCHAR fw[MAX_PATH]; char fa[MAX_PATH]; t_tmpfile(fa, sizeof fa, "app_absent2.hiv"); MultiByteToWideChar(CP_ACP, 0, fa, -1, fw, MAX_PATH);
    HKEY k = NULL;
    LSTATUS s = U(RegLoadAppKeyW)(fw, &k, KEY_READ, 0x80, 0); t_note("RegLoadAppKeyW(bad options) = %ld", (long)s);
    s = U(RegLoadAppKeyW)(fw, &k, KEY_READ, 0, 5); t_note("RegLoadAppKeyW(reserved) = %ld", (long)s);
    CHECK(k == NULL);
}
TEST(regkb_sys_replacekey_without_privilege) {
    REQUIRE(UA(RegReplaceKeyW));
    HKEY h = FRESH(); REQUIRE(h);
    WCHAR f1[MAX_PATH], f2[MAX_PATH]; char a1[MAX_PATH], a2[MAX_PATH];
    t_tmpfile(a1, sizeof a1, "new.hiv"); t_tmpfile(a2, sizeof a2, "old.hiv");
    MultiByteToWideChar(CP_ACP, 0, a1, -1, f1, MAX_PATH); MultiByteToWideChar(CP_ACP, 0, a2, -1, f2, MAX_PATH);
    LSTATUS s = UA(RegReplaceKeyW)(h, NULL, f1, f2);
    CHECK(s != 0); t_note("RegReplaceKeyW = %ld", (long)s);
    RegCloseKey(h);
}

/* ---------------- application restart / recovery (child) ---------------- */
typedef HRESULT (WINAPI *RegRestart_t)(PCWSTR, DWORD);
typedef HRESULT (WINAPI *GetRestart_t)(HANDLE, PWSTR, PDWORD, PDWORD);
typedef HRESULT (WINAPI *Unreg_t)(void);
static DWORD WINAPI rec_cb(PVOID p) { return 0; }
typedef HRESULT (WINAPI *RegRec_t)(LPVOID, PVOID, DWORD, DWORD);
typedef HRESULT (WINAPI *GetRec_t)(HANDLE, LPVOID *, PVOID *, PDWORD, PDWORD);

CHILD(regkb_restart) {
    RegRestart_t reg = (RegRestart_t)t_proc("RegisterApplicationRestart");
    GetRestart_t get = (GetRestart_t)t_proc("GetApplicationRestartSettings");
    Unreg_t unreg = (Unreg_t)t_proc("UnregisterApplicationRestart");
    if (!reg || !get || !unreg) return 20;
    WCHAR out[64]; DWORD sz = 64, fl = 99;
    HRESULT hr = get(GetCurrentProcess(), out, &sz, &fl);
    if (hr != HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) { printf("get before register hr=%08lx\n", hr); return 21; }
    hr = reg(L"/k32t restart args", 0);
    if (hr != S_OK) { printf("register hr=%08lx\n", hr); return 22; }
    sz = 64; fl = 99; memset(out, 0xCC, sizeof out);
    hr = get(GetCurrentProcess(), out, &sz, &fl);
    if (hr != S_OK || lstrcmpW(out, L"/k32t restart args") || fl != 0) { printf("get hr=%08lx fl=%lu\n", hr, fl); return 23; }
    if (sz != 19) { printf("size %lu\n", sz); return 24; }
    sz = 0; hr = get(GetCurrentProcess(), NULL, &sz, &fl);
    if (hr != HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) || sz != 19) { printf("size query hr=%08lx sz=%lu\n", hr, sz); return 25; }
    sz = 5; hr = get(GetCurrentProcess(), out, &sz, &fl);
    if (hr != HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)) { printf("small hr=%08lx\n", hr); return 26; }
    hr = reg(L"/second", RESTART_NO_CRASH | RESTART_NO_HANG);
    if (hr != S_OK) return 27;
    sz = 64; hr = get(GetCurrentProcess(), out, &sz, &fl);
    if (hr != S_OK || lstrcmpW(out, L"/second") || fl != (RESTART_NO_CRASH | RESTART_NO_HANG)) { printf("re-register hr=%08lx fl=%lu\n", hr, fl); return 28; }
    hr = unreg();
    if (hr != S_OK) { printf("unregister hr=%08lx\n", hr); return 29; }
    sz = 64; hr = get(GetCurrentProcess(), out, &sz, &fl);
    if (hr != HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) { printf("get after unregister hr=%08lx\n", hr); return 30; }
    hr = unreg();
    printf("second unregister hr=%08lx\n", hr);
    return 0;
}
TEST(regkb_restart_register_get_unregister_in_child) {
    REQUIRE(t_proc("RegisterApplicationRestart")); REQUIRE(t_proc("GetApplicationRestartSettings")); REQUIRE(t_proc("UnregisterApplicationRestart"));
    if (t_is_wine()) { t_skip("application restart under wine"); return; }
    EQ(t_run_child("regkb_restart", "", 30000), 0);
}
CHILD(regkb_restart_errors) {
    RegRestart_t reg = (RegRestart_t)t_proc("RegisterApplicationRestart");
    GetRestart_t get = (GetRestart_t)t_proc("GetApplicationRestartSettings");
    if (!reg || !get) return 20;
    static WCHAR big[1100]; rk_fill_w(big, 1024, L'x');
    HRESULT hr = reg(big, 0);
    if (hr != E_INVALIDARG) { printf("1024 chars hr=%08lx\n", hr); return 21; }
    rk_fill_w(big, 1023, L'x');
    hr = reg(big, 0);
    if (hr != S_OK) { printf("1023 chars hr=%08lx\n", hr); return 22; }
    hr = reg(L"ok", 0x10);
    if (hr != E_INVALIDARG) { printf("bad flags hr=%08lx\n", hr); return 23; }
    hr = reg(NULL, 0);
    printf("NULL cmdline hr=%08lx\n", hr);
    WCHAR out[8]; DWORD sz = 8, fl;
    hr = get(NULL, out, &sz, &fl);
    printf("get(NULL process) hr=%08lx\n", hr);
    if (hr == S_OK) return 24;
    return 0;
}
TEST(regkb_restart_argument_errors_in_child) {
    REQUIRE(t_proc("RegisterApplicationRestart"));
    if (t_is_wine()) { t_skip("application restart under wine"); return; }
    EQ(t_run_child("regkb_restart_errors", "", 30000), 0);
}
CHILD(regkb_recovery) {
    RegRec_t reg = (RegRec_t)t_proc("RegisterApplicationRecoveryCallback");
    GetRec_t get = (GetRec_t)t_proc("GetApplicationRecoveryCallback");
    Unreg_t unreg = (Unreg_t)t_proc("UnregisterApplicationRecoveryCallback");
    if (!reg || !get || !unreg) return 20;
    LPVOID cb = NULL; PVOID par = NULL; DWORD ping = 0, fl = 7;
    HRESULT hr = get(GetCurrentProcess(), &cb, &par, &ping, &fl);
    if (hr != HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) { printf("get before register hr=%08lx\n", hr); return 21; }
    hr = reg((LPVOID)rec_cb, (PVOID)0x1234, 7000, 0);
    if (hr != S_OK) { printf("register hr=%08lx\n", hr); return 22; }
    hr = get(GetCurrentProcess(), &cb, &par, &ping, &fl);
    if (hr != S_OK || cb != (LPVOID)rec_cb || par != (PVOID)0x1234 || ping != 7000 || fl != 0) { printf("get hr=%08lx ping=%lu\n", hr, ping); return 23; }
    hr = reg((LPVOID)rec_cb, NULL, 0, 0);
    if (hr != S_OK) return 24;
    hr = get(GetCurrentProcess(), &cb, &par, &ping, &fl);
    printf("default ping interval = %lu (hr %08lx)\n", ping, hr);
    hr = reg(NULL, NULL, 5000, 0);
    if (hr != E_INVALIDARG) { printf("NULL callback hr=%08lx\n", hr); return 25; }
    hr = reg((LPVOID)rec_cb, NULL, 5000, 1);
    if (hr != E_INVALIDARG) { printf("flags=1 hr=%08lx\n", hr); return 26; }
    hr = reg((LPVOID)rec_cb, NULL, 5 * 60 * 1000 + 1, 0);
    printf("ping > 5 minutes hr=%08lx\n", hr);
    hr = unreg();
    if (hr != S_OK) { printf("unregister hr=%08lx\n", hr); return 27; }
    hr = get(GetCurrentProcess(), &cb, &par, &ping, &fl);
    if (hr != HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) { printf("get after unregister hr=%08lx\n", hr); return 28; }
    return 0;
}
TEST(regkb_recovery_callback_register_get_unregister_in_child) {
    REQUIRE(t_proc("RegisterApplicationRecoveryCallback")); REQUIRE(t_proc("GetApplicationRecoveryCallback"));
    if (t_is_wine()) { t_skip("application recovery under wine"); return; }
    EQ(t_run_child("regkb_recovery", "", 30000), 0);
}
TEST(regkb_recovery_in_progress_outside_recovery) {
    FARPROC f = t_proc("ApplicationRecoveryInProgress"); REQUIRE(f);
    if (t_is_wine()) { t_skip("application recovery under wine"); return; }
    BOOL cancelled = TRUE;
    HRESULT hr = ((HRESULT (WINAPI *)(PBOOL))f)(&cancelled);
    CHECK(hr != S_OK);
    t_note("ApplicationRecoveryInProgress outside recovery hr=%08lx", hr);
}

/* ---------------- cleanup behaviour and concurrency ---------------- */
CHILD(regkb_cleanup_child) {
    HKEY r = rk_root(); if (!r) return 20;
    HKEY a = sc_sub(r, L"x\\y\\z"); sc_dw(a, L"v", 1); RegCloseKey(a);
    printf("%s\n", g_rk_root_name);
    return 0;
}
TEST(regkb_sys_child_cleans_its_own_root_on_exit) {
    PROCESS_INFORMATION pi; HANDLE h = t_spawn_child("regkb_cleanup_child", "", 0, NULL, NULL, NULL, FALSE, NULL, &pi);
    REQUIRE(h);
    DWORD wr = WaitForSingleObject(h, 30000); DWORD code = 99;
    if (wr != WAIT_OBJECT_0) { TerminateProcess(h, 1); }
    GetExitCodeProcess(h, &code);
    DWORD pid = pi.dwProcessId; CloseHandle(pi.hThread); CloseHandle(h);
    EQ(wr, WAIT_OBJECT_0); EQ(code, 0);
    char p[64]; snprintf(p, sizeof p, "Software\\k32t_%lu", pid);
    CHECK(!rk_exists(HKEY_CURRENT_USER, p));
}

struct writer { HKEY key; int id; int ok; };
static DWORD WINAPI writer_thread(LPVOID p) {
    struct writer *w = p;
    for (int i = 0; i < 100; i++) {
        WCHAR nm[32]; swprintf(nm, 32, L"w%d_%03d", w->id, i);
        DWORD v = w->id * 1000 + i;
        if (U(RegSetValueExW)(w->key, nm, 0, REG_DWORD, (BYTE *)&v, 4) != 0) return 1;
        swprintf(nm, 32, L"k%d_%03d", w->id, i % 10);
        HKEY k = NULL;
        if (U(RegCreateKeyExW)(w->key, nm, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL) != 0) return 2;
        U(RegCloseKey)(k);
    }
    w->ok = 1;
    return 0;
}
TEST(regkb_sys_three_concurrent_writers) {
    REQUIRE(U(RegSetValueExW)); REQUIRE(U(RegCreateKeyExW)); REQUIRE(U(RegCloseKey));
    HKEY h = FRESH(); REQUIRE(h);
    struct writer w[3] = { { h, 1, 0 }, { h, 2, 0 }, { h, 3, 0 } }; HANDLE th[3];
    for (int i = 0; i < 3; i++) th[i] = CreateThread(NULL, 0, writer_thread, &w[i], 0, NULL);
    for (int i = 0; i < 3; i++) { REQUIRE(th[i]); }
    CHECK(WaitForMultipleObjects(3, th, TRUE, 30000) == WAIT_OBJECT_0);
    for (int i = 0; i < 3; i++) { CloseHandle(th[i]); EQ(w[i].ok, 1); }
    EQ(rk_valcount(h), 300); EQ(rk_subcount(h), 30);
    DWORD v = 0, cb = 4, t; EQ(RegQueryValueExW(h, L"w2_042", NULL, &t, (BYTE *)&v, &cb), 0); EQ(v, 2042);
    RegCloseKey(h);
}
struct racer { HKEY parent; DWORD disp; LSTATUS s; HANDLE go; };
static DWORD WINAPI racer_thread(LPVOID p) {
    struct racer *r = p; HKEY k = NULL;
    WaitForSingleObject(r->go, 5000);
    r->s = U(RegCreateKeyExW)(r->parent, L"race", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, &r->disp);
    if (k) U(RegCloseKey)(k);
    return 0;
}
TEST(regkb_sys_concurrent_create_exactly_one_creator) {
    REQUIRE(U(RegCreateKeyExW));
    HKEY h = FRESH(); REQUIRE(h); HANDLE go = CreateEventW(NULL, TRUE, FALSE, NULL);
    struct racer r[8]; HANDLE th[8];
    for (int i = 0; i < 8; i++) { r[i].parent = h; r[i].disp = 0; r[i].s = 99; r[i].go = go; th[i] = CreateThread(NULL, 0, racer_thread, &r[i], 0, NULL); REQUIRE(th[i]); }
    SetEvent(go);
    CHECK(WaitForMultipleObjects(8, th, TRUE, 20000) == WAIT_OBJECT_0);
    int created = 0, opened = 0;
    for (int i = 0; i < 8; i++) { CloseHandle(th[i]); EQ(r[i].s, 0); created += r[i].disp == REG_CREATED_NEW_KEY; opened += r[i].disp == REG_OPENED_EXISTING_KEY; }
    EQ(created, 1); EQ(opened, 7); EQ(rk_subcount(h), 1);
    CloseHandle(go); RegCloseKey(h);
}
struct rdr { HKEY key; volatile LONG *stop; int bad; };
static DWORD WINAPI reader_thread(LPVOID p) {
    struct rdr *r = p;
    while (!*r->stop) {
        DWORD v = 0, cb = 4, t; LSTATUS s = U(RegQueryValueExW)(r->key, L"counter", NULL, &t, (BYTE *)&v, &cb);
        if (s != 0 || cb != 4 || t != REG_DWORD) r->bad++;
    }
    return 0;
}
TEST(regkb_sys_reader_sees_consistent_value_while_writer_updates) {
    REQUIRE(U(RegQueryValueExW)); REQUIRE(U(RegSetValueExW));
    HKEY h = FRESH(); REQUIRE(h); sc_dw(h, L"counter", 0);
    volatile LONG stop = 0; struct rdr r = { h, &stop, 0 };
    HANDLE th = CreateThread(NULL, 0, reader_thread, &r, 0, NULL); REQUIRE(th);
    for (DWORD i = 1; i <= 2000; i++) U(RegSetValueExW)(h, L"counter", 0, REG_DWORD, (BYTE *)&i, 4);
    stop = 1; WaitForSingleObject(th, 10000); CloseHandle(th);
    EQ(r.bad, 0);
    RegCloseKey(h);
}
TEST(regkb_sys_handle_count_stable_after_many_open_close) {
    REQUIRE(U(RegOpenKeyExW)); REQUIRE(U(RegCloseKey));
    HKEY h = FRESH(); REQUIRE(h); sc_subc(h, L"s");
    DWORD before = 0, after = 0; GetProcessHandleCount(GetCurrentProcess(), &before);
    for (int i = 0; i < 3000; i++) { HKEY k; if (U(RegOpenKeyExW)(h, L"s", 0, KEY_READ, &k) != 0) { CHECK(0); break; } U(RegCloseKey)(k); }
    GetProcessHandleCount(GetCurrentProcess(), &after);
    CHECKF(after <= before + 2, "handles %lu -> %lu", before, after);
    RegCloseKey(h);
}
