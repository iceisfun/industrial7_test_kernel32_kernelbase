#include "t.h"
/* gap19: registry exports with thin coverage: RegSetKeyValueA, RegOpenCurrentUser,
 * RegOpenUserClassesRoot, and the privileged hive calls (RegSaveKeyExA, RegRestoreKeyW,
 * RegLoadKeyA, RegUnLoadKeyW, RegLoadAppKeyA) exercised only on provably safe paths:
 * HKEY_PERFORMANCE_DATA (0x80000004) is rejected up front with ERROR_INVALID_HANDLE,
 * and everything else targets only our own HKCU\Software\k32t_<pid> key or a
 * non-existent file under t_tmpdir(). */

UDECL(LSTATUS, RegSetKeyValueA, (HKEY, LPCSTR, LPCSTR, DWORD, LPCVOID, DWORD));
UDECL(LSTATUS, RegLoadAppKeyA, (LPCSTR, PHKEY, REGSAM, DWORD, DWORD));
UDECL(LSTATUS, RegSaveKeyExA, (HKEY, LPCSTR, LPSECURITY_ATTRIBUTES, DWORD));
UDECL(LSTATUS, RegRestoreKeyW, (HKEY, LPCWSTR, DWORD));
UDECL(LSTATUS, RegLoadKeyA, (HKEY, LPCSTR, LPCSTR));
UDECL(LSTATUS, RegUnLoadKeyW, (HKEY, LPCWSTR));
UDECL(LSTATUS, RegOpenCurrentUser, (REGSAM, PHKEY));
UDECL(LSTATUS, RegOpenUserClassesRoot, (HANDLE, DWORD, REGSAM, PHKEY));

#define PERFKEY ((HKEY)(ULONG_PTR)0xFFFFFFFF80000004ULL)

static void g19_rootname(char *out, size_t cap) {
    snprintf(out, cap, "Software\\k32t_%lu", (unsigned long)GetCurrentProcessId());
}
static HKEY g19_mkroot(void) {
    char n[64]; HKEY k = NULL;
    g19_rootname(n, sizeof n);
    if (RegCreateKeyExA(HKEY_CURRENT_USER, n, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL) != 0) return NULL;
    return k;
}
static void g19_delroot(HKEY k) {
    char n[64];
    if (k) RegCloseKey(k);
    g19_rootname(n, sizeof n);
    RegDeleteTreeA(HKEY_CURRENT_USER, n);
    RegDeleteKeyA(HKEY_CURRENT_USER, n);
}
static LSTATUS g19_qa(HKEY k, const char *name, DWORD *type, void *buf, DWORD *cb) {
    return RegQueryValueExA(k, name, NULL, type, (LPBYTE)buf, cb);
}

/* ---- RegSetKeyValueA ---- */
TEST(gap19_regsetkeyvalue_sz_nosubkey) {
    REQUIRE(UN(RegSetKeyValueA));
    HKEY k = g19_mkroot(); if (!k) { t_skip("cannot create test key"); return; }
    EXPECT_EQ(UN(RegSetKeyValueA)(k, NULL, "v", REG_SZ, "hello", 6), 0);
    char b[32] = {0}; DWORD t = 0, cb = sizeof b;
    EXPECT_EQ(g19_qa(k, "v", &t, b, &cb), 0);
    EXPECT_EQ(t, REG_SZ); EXPECT_EQ(cb, 6); EXPECT_STR(b, "hello");
    g19_delroot(k);
}
TEST(gap19_regsetkeyvalue_sz_stored_as_utf16) {
    REQUIRE(UN(RegSetKeyValueA));
    HKEY k = g19_mkroot(); if (!k) { t_skip("cannot create test key"); return; }
    EXPECT_EQ(UN(RegSetKeyValueA)(k, NULL, "w", REG_SZ, "abc", 4), 0);
    WCHAR wb[16] = {0}; DWORD t = 0, cb = sizeof wb;
    EXPECT_EQ(RegQueryValueExW(k, L"w", NULL, &t, (LPBYTE)wb, &cb), 0);
    EXPECT_EQ(t, REG_SZ); EXPECT_EQ(cb, 8);
    EXPECT_WSTR(wb, L"abc");
    g19_delroot(k);
}
TEST(gap19_regsetkeyvalue_creates_subkey) {
    REQUIRE(UN(RegSetKeyValueA));
    HKEY k = g19_mkroot(); if (!k) { t_skip("cannot create test key"); return; }
    EXPECT_EQ(UN(RegSetKeyValueA)(k, "sub1", "v", REG_DWORD, &(DWORD){0x12345678}, 4), 0);
    HKEY s = NULL;
    EXPECT_EQ(RegOpenKeyExA(k, "sub1", 0, KEY_READ, &s), 0);
    if (s) {
        DWORD d = 0, t = 0, cb = 4;
        EXPECT_EQ(g19_qa(s, "v", &t, &d, &cb), 0);
        EXPECT_EQ(t, REG_DWORD); EXPECT_EQ(d, 0x12345678);
        RegCloseKey(s);
    }
    /* value is not on the parent */
    DWORD cb = 4, d = 0;
    EXPECT_EQ(g19_qa(k, "v", NULL, &d, &cb), ERROR_FILE_NOT_FOUND);
    g19_delroot(k);
}
TEST(gap19_regsetkeyvalue_nested_subkey) {
    REQUIRE(UN(RegSetKeyValueA));
    HKEY k = g19_mkroot(); if (!k) { t_skip("cannot create test key"); return; }
    EXPECT_EQ(UN(RegSetKeyValueA)(k, "a\\b\\c", "n", REG_SZ, "x", 2), 0);
    HKEY s = NULL;
    EXPECT_EQ(RegOpenKeyExA(k, "a\\b\\c", 0, KEY_READ, &s), 0);
    if (s) RegCloseKey(s);
    EXPECT_EQ(RegOpenKeyExA(k, "a\\b", 0, KEY_READ, &s), 0);
    if (s) RegCloseKey(s);
    g19_delroot(k);
}
TEST(gap19_regsetkeyvalue_empty_subkey_is_self) {
    REQUIRE(UN(RegSetKeyValueA));
    HKEY k = g19_mkroot(); if (!k) { t_skip("cannot create test key"); return; }
    EXPECT_EQ(UN(RegSetKeyValueA)(k, "", "e", REG_SZ, "q", 2), 0);
    char b[8] = {0}; DWORD cb = sizeof b;
    EXPECT_EQ(g19_qa(k, "e", NULL, b, &cb), 0);
    EXPECT_STR(b, "q");
    /* no empty-named subkey was created */
    DWORD n = 99;
    EXPECT_EQ(RegQueryInfoKeyA(k, NULL, NULL, NULL, &n, NULL, NULL, NULL, NULL, NULL, NULL, NULL), 0);
    EXPECT_EQ(n, 0);
    g19_delroot(k);
}
TEST(gap19_regsetkeyvalue_default_value) {
    REQUIRE(UN(RegSetKeyValueA));
    HKEY k = g19_mkroot(); if (!k) { t_skip("cannot create test key"); return; }
    EXPECT_EQ(UN(RegSetKeyValueA)(k, NULL, NULL, REG_SZ, "dflt", 5), 0);
    char b[16] = {0}; DWORD cb = sizeof b, t = 0;
    EXPECT_EQ(g19_qa(k, NULL, &t, b, &cb), 0);
    EXPECT_EQ(t, REG_SZ); EXPECT_STR(b, "dflt");
    char b2[16] = {0}; cb = sizeof b2;
    EXPECT_EQ(g19_qa(k, "", NULL, b2, &cb), 0);
    EXPECT_STR(b2, "dflt");
    g19_delroot(k);
}
TEST(gap19_regsetkeyvalue_binary_and_empty) {
    REQUIRE(UN(RegSetKeyValueA));
    HKEY k = g19_mkroot(); if (!k) { t_skip("cannot create test key"); return; }
    BYTE in[5] = {0, 1, 0xff, 0x80, 0};
    EXPECT_EQ(UN(RegSetKeyValueA)(k, NULL, "bin", REG_BINARY, in, 5), 0);
    BYTE out[8] = {0}; DWORD t = 0, cb = sizeof out;
    EXPECT_EQ(g19_qa(k, "bin", &t, out, &cb), 0);
    EXPECT_EQ(t, REG_BINARY); EXPECT_EQ(cb, 5); CHECK(!memcmp(in, out, 5));
    EXPECT_EQ(UN(RegSetKeyValueA)(k, NULL, "none", REG_NONE, NULL, 0), 0);
    cb = 8; t = 99;
    EXPECT_EQ(g19_qa(k, "none", &t, out, &cb), 0);
    EXPECT_EQ(t, REG_NONE); EXPECT_EQ(cb, 0);
    EXPECT_EQ(UN(RegSetKeyValueA)(k, NULL, "emptybin", REG_BINARY, NULL, 0), 0);
    g19_delroot(k);
}
TEST(gap19_regsetkeyvalue_multisz_expand) {
    REQUIRE(UN(RegSetKeyValueA));
    HKEY k = g19_mkroot(); if (!k) { t_skip("cannot create test key"); return; }
    const char ms[] = "one\0two\0";
    EXPECT_EQ(UN(RegSetKeyValueA)(k, NULL, "ms", REG_MULTI_SZ, ms, sizeof ms), 0);
    char b[32] = {0}; DWORD t = 0, cb = sizeof b;
    EXPECT_EQ(g19_qa(k, "ms", &t, b, &cb), 0);
    EXPECT_EQ(t, REG_MULTI_SZ);
    CHECK(!strcmp(b, "one")); CHECK(!strcmp(b + 4, "two"));
    EXPECT_EQ(UN(RegSetKeyValueA)(k, NULL, "ex", REG_EXPAND_SZ, "%TEMP%", 7), 0);
    cb = sizeof b; t = 0;
    EXPECT_EQ(g19_qa(k, "ex", &t, b, &cb), 0);
    EXPECT_EQ(t, REG_EXPAND_SZ); EXPECT_STR(b, "%TEMP%");
    g19_delroot(k);
}
TEST(gap19_regsetkeyvalue_overwrite_changes_type) {
    REQUIRE(UN(RegSetKeyValueA));
    HKEY k = g19_mkroot(); if (!k) { t_skip("cannot create test key"); return; }
    EXPECT_EQ(UN(RegSetKeyValueA)(k, NULL, "x", REG_SZ, "abc", 4), 0);
    DWORD d = 7;
    EXPECT_EQ(UN(RegSetKeyValueA)(k, NULL, "x", REG_DWORD, &d, 4), 0);
    DWORD t = 0, cb = 4, o = 0;
    EXPECT_EQ(g19_qa(k, "x", &t, &o, &cb), 0);
    EXPECT_EQ(t, REG_DWORD); EXPECT_EQ(o, 7);
    /* repeated set on existing subkey keeps working */
    for (int i = 0; i < 20; i++) {
        d = i;
        EXPECT_EQ(UN(RegSetKeyValueA)(k, "s", "n", REG_DWORD, &d, 4), 0);
    }
    HKEY s = NULL; cb = 4;
    EXPECT_EQ(RegOpenKeyExA(k, "s", 0, KEY_READ, &s), 0);
    if (s) { EXPECT_EQ(g19_qa(s, "n", NULL, &o, &cb), 0); EXPECT_EQ(o, 19); RegCloseKey(s); }
    g19_delroot(k);
}
TEST(gap19_regsetkeyvalue_via_predefined_hkcu) {
    REQUIRE(UN(RegSetKeyValueA));
    char n[96], sub[96];
    g19_rootname(n, sizeof n);
    snprintf(sub, sizeof sub, "%s\\viaHKCU", n);
    EXPECT_EQ(UN(RegSetKeyValueA)(HKEY_CURRENT_USER, sub, "v", REG_SZ, "z", 2), 0);
    HKEY s = NULL;
    EXPECT_EQ(RegOpenKeyExA(HKEY_CURRENT_USER, sub, 0, KEY_READ, &s), 0);
    if (s) RegCloseKey(s);
    RegDeleteTreeA(HKEY_CURRENT_USER, n); RegDeleteKeyA(HKEY_CURRENT_USER, n);
}
TEST(gap19_regsetkeyvalue_null_hkey) {
    REQUIRE(UN(RegSetKeyValueA));
    EXPECT_EQ(UN(RegSetKeyValueA)(NULL, NULL, "v", REG_SZ, "a", 2), ERROR_INVALID_HANDLE);
    EXPECT_EQ(UN(RegSetKeyValueA)(NULL, "sub", "v", REG_SZ, "a", 2), ERROR_INVALID_HANDLE);
}
TEST(gap19_regsetkeyvalue_closed_handle) {
    REQUIRE(UN(RegSetKeyValueA));
    HKEY k = g19_mkroot(); if (!k) { t_skip("cannot create test key"); return; }
    HKEY copy = k;
    RegCloseKey(k);
    EXPECT_EQ(UN(RegSetKeyValueA)(copy, NULL, "v", REG_SZ, "a", 2), ERROR_INVALID_HANDLE);
    g19_delroot(NULL);
}
TEST(gap19_regsetkeyvalue_readonly_handle) {
    if (t_is_wine()) { t_skip("wine deviates (access/handle validation)"); return; }
    REQUIRE(UN(RegSetKeyValueA));
    HKEY k = g19_mkroot(); if (!k) { t_skip("cannot create test key"); return; }
    char n[64]; g19_rootname(n, sizeof n);
    HKEY ro = NULL;
    EXPECT_EQ(RegOpenKeyExA(HKEY_CURRENT_USER, n, 0, KEY_READ, &ro), 0);
    if (ro) {
        EXPECT_EQ(UN(RegSetKeyValueA)(ro, NULL, "v", REG_SZ, "a", 2), ERROR_ACCESS_DENIED);
        LSTATUS r = UN(RegSetKeyValueA)(ro, "newsub", "v", REG_SZ, "a", 2);
        EXPECT_EQ(r, ERROR_ACCESS_DENIED);
        HKEY s = NULL;
        EXPECT_NE(RegOpenKeyExA(k, "newsub", 0, KEY_READ, &s), 0);
        RegCloseKey(ro);
    }
    g19_delroot(k);
}
TEST(gap19_regsetkeyvalue_name_with_backslash_value) {
    REQUIRE(UN(RegSetKeyValueA));
    HKEY k = g19_mkroot(); if (!k) { t_skip("cannot create test key"); return; }
    /* value names may contain backslashes; they are not path separators */
    LSTATUS r = UN(RegSetKeyValueA)(k, NULL, "a\\b", REG_SZ, "x", 2);
    EXPECT_EQ(r, 0);
    char b[8] = {0}; DWORD cb = sizeof b;
    EXPECT_EQ(g19_qa(k, "a\\b", NULL, b, &cb), 0);
    g19_delroot(k);
}
TEST(gap19_regsetkeyvalue_odd_types_accepted) {
    REQUIRE(UN(RegSetKeyValueA));
    HKEY k = g19_mkroot(); if (!k) { t_skip("cannot create test key"); return; }
    DWORD types[] = { REG_QWORD, REG_LINK, REG_RESOURCE_LIST, 0x1234 };
    for (unsigned i = 0; i < 4; i++) {
        BYTE d[8] = {1,2,3,4,5,6,7,8};
        LSTATUS r = UN(RegSetKeyValueA)(k, NULL, "t", types[i], d, 8);
        t_note("type 0x%lx -> %ld", (unsigned long)types[i], (long)r);
        if (r == 0) {
            DWORD t = 0, cb = 8; BYTE o[8];
            EXPECT_EQ(g19_qa(k, "t", &t, o, &cb), 0);
            EXPECT_EQ(t, types[i]);
        }
    }
    g19_delroot(k);
}

/* ---- RegOpenCurrentUser ---- */
TEST(gap19_regopencurrentuser_null_result) {
    REQUIRE(UN(RegOpenCurrentUser));
    EXPECT_EQ(UN(RegOpenCurrentUser)(KEY_READ, NULL), ERROR_INVALID_PARAMETER);
}
TEST(gap19_regopencurrentuser_basic) {
    REQUIRE(UN(RegOpenCurrentUser));
    HKEY h = NULL;
    EXPECT_EQ(UN(RegOpenCurrentUser)(KEY_READ, &h), 0);
    CHECK(h != NULL);
    if (!h) return;
    HKEY s = NULL;
    EXPECT_EQ(RegOpenKeyExA(h, "Software", 0, KEY_READ, &s), 0);
    if (s) RegCloseKey(s);
    /* a different handle from the predefined HKCU pseudo-handle */
    CHECK(h != HKEY_CURRENT_USER);
    RegCloseKey(h);
}
TEST(gap19_regopencurrentuser_sees_own_key) {
    REQUIRE(UN(RegOpenCurrentUser));
    HKEY k = g19_mkroot(); if (!k) { t_skip("cannot create test key"); return; }
    HKEY h = NULL;
    EXPECT_EQ(UN(RegOpenCurrentUser)(KEY_READ, &h), 0);
    if (h) {
        char n[64]; g19_rootname(n, sizeof n);
        HKEY s = NULL;
        EXPECT_EQ(RegOpenKeyExA(h, n, 0, KEY_READ, &s), 0);
        if (s) RegCloseKey(s);
        RegCloseKey(h);
    }
    g19_delroot(k);
}
TEST(gap19_regopencurrentuser_access_rights) {
    REQUIRE(UN(RegOpenCurrentUser));
    HKEY h = NULL;
    EXPECT_EQ(UN(RegOpenCurrentUser)(KEY_QUERY_VALUE, &h), 0);
    if (h) {
        /* granted only query: opening a subkey with write intent via it still works at subkey level,
         * but enumerating subkeys on the handle needs KEY_ENUMERATE_SUB_KEYS */
        char nm[64]; DWORD n = sizeof nm;
        LSTATUS r = RegEnumKeyExA(h, 0, nm, &n, NULL, NULL, NULL, NULL);
        EXPECT_EQ(r, ERROR_ACCESS_DENIED);
        RegCloseKey(h);
    }
    h = NULL;
    EXPECT_EQ(UN(RegOpenCurrentUser)(KEY_ENUMERATE_SUB_KEYS, &h), 0);
    if (h) {
        char nm[64]; DWORD n = sizeof nm;
        LSTATUS r = RegEnumKeyExA(h, 0, nm, &n, NULL, NULL, NULL, NULL);
        CHECK(r == 0 || r == ERROR_NO_MORE_ITEMS || r == ERROR_MORE_DATA);
        RegCloseKey(h);
    }
}
TEST(gap19_regopencurrentuser_repeat) {
    REQUIRE(UN(RegOpenCurrentUser));
    HKEY hs[20];
    int n = 0;
    for (int i = 0; i < 20; i++) {
        hs[i] = NULL;
        if (UN(RegOpenCurrentUser)(KEY_READ, &hs[i]) == 0) n++;
    }
    EXPECT_EQ(n, 20);
    for (int i = 0; i < 20; i++) if (hs[i]) RegCloseKey(hs[i]);
}
TEST(gap19_regopencurrentuser_zero_access) {
    REQUIRE(UN(RegOpenCurrentUser));
    HKEY h = NULL;
    LSTATUS r = UN(RegOpenCurrentUser)(0, &h);
    t_note("RegOpenCurrentUser(0) -> %ld", (long)r);
    if (r == 0 && h) RegCloseKey(h);
}

/* ---- RegOpenUserClassesRoot ---- */
TEST(gap19_regopenuserclassesroot_invalid_params) {
    if (t_is_wine()) { t_skip("wine deviates (access/handle validation)"); return; }
    REQUIRE(UN(RegOpenUserClassesRoot));
    HANDLE tok = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) { t_skip("no token"); return; }
    HKEY h = (HKEY)0x1234;
    EXPECT_EQ(UN(RegOpenUserClassesRoot)(tok, 0, KEY_READ, NULL), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(UN(RegOpenUserClassesRoot)(NULL, 0, KEY_READ, &h), ERROR_INVALID_PARAMETER);
    EXPECT_EQ((ULONG_PTR)h, 0x1234);   /* untouched on early failure */
    EXPECT_EQ(UN(RegOpenUserClassesRoot)(tok, 1, KEY_READ, &h), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(UN(RegOpenUserClassesRoot)(tok, 0x80000000, KEY_READ, &h), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(UN(RegOpenUserClassesRoot)(tok, 0xffffffff, KEY_READ, &h), ERROR_INVALID_PARAMETER);
    EXPECT_EQ((ULONG_PTR)h, 0x1234);
    CloseHandle(tok);
}
TEST(gap19_regopenuserclassesroot_bad_token_handle) {
    if (t_is_wine()) { t_skip("wine deviates (access/handle validation)"); return; }
    REQUIRE(UN(RegOpenUserClassesRoot));
    HKEY h = NULL;
    /* a pseudo process handle is not a token: NtQueryInformationToken type mismatch */
    LSTATUS r = UN(RegOpenUserClassesRoot)(GetCurrentProcess(), 0, KEY_READ, &h);
    EXPECT_EQ(r, ERROR_INVALID_HANDLE);
    HANDLE ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    r = UN(RegOpenUserClassesRoot)(ev, 0, KEY_READ, &h);
    EXPECT_EQ(r, ERROR_INVALID_HANDLE);
    CloseHandle(ev);
    HANDLE tok = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) {
        CloseHandle(tok);
        r = UN(RegOpenUserClassesRoot)(tok, 0, KEY_READ, &h);   /* closed */
        EXPECT_EQ(r, ERROR_INVALID_HANDLE);
    }
    CHECK(h == NULL);
}
TEST(gap19_regopenuserclassesroot_basic) {
    REQUIRE(UN(RegOpenUserClassesRoot));
    HANDLE tok = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) { t_skip("no token"); return; }
    HKEY h = NULL;
    LSTATUS r = UN(RegOpenUserClassesRoot)(tok, 0, KEY_READ, &h);
    t_note("RegOpenUserClassesRoot -> %ld", (long)r);
    if (r == 0) {
        CHECK(h != NULL);
        HKEY s = NULL;
        /* every classes hive exposes the merged .txt/.exe associations */
        LSTATUS r2 = RegOpenKeyExA(h, ".exe", 0, KEY_READ, &s);
        t_note("open .exe -> %ld", (long)r2);
        if (s) RegCloseKey(s);
        RegCloseKey(h);
    }
    CloseHandle(tok);
}
TEST(gap19_regopenuserclassesroot_access_rights) {
    if (t_is_wine()) { t_skip("wine deviates (access/handle validation)"); return; }
    REQUIRE(UN(RegOpenUserClassesRoot));
    HANDLE tok = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) { t_skip("no token"); return; }
    HKEY h = NULL;
    if (UN(RegOpenUserClassesRoot)(tok, 0, KEY_QUERY_VALUE, &h) == 0 && h) {
        char nm[64]; DWORD n = sizeof nm;
        EXPECT_EQ(RegEnumKeyExA(h, 0, nm, &n, NULL, NULL, NULL, NULL), ERROR_ACCESS_DENIED);
        RegCloseKey(h);
    }
    CloseHandle(tok);
}
TEST(gap19_regopenuserclassesroot_token_without_query) {
    if (t_is_wine()) { t_skip("wine deviates (access/handle validation)"); return; }
    REQUIRE(UN(RegOpenUserClassesRoot));
    HANDLE tok = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE, &tok)) { t_skip("no token"); return; }
    HKEY h = NULL;
    /* token lacks TOKEN_QUERY: NtQueryInformationToken -> STATUS_ACCESS_DENIED -> 5 */
    LSTATUS r = UN(RegOpenUserClassesRoot)(tok, 0, KEY_READ, &h);
    EXPECT_EQ(r, ERROR_ACCESS_DENIED);
    CloseHandle(tok);
}

/* ---- privileged hive calls: safe paths only ---- */
TEST(gap19_regsavekeyex_perfdata_invalid_handle) {
    if (t_is_wine()) { t_skip("wine deviates (access/handle validation)"); return; }
    REQUIRE(UN(RegSaveKeyExA));
    char f[MAX_PATH]; t_tmpfile(f, sizeof f, "perf_save.hiv");
    EXPECT_EQ(UN(RegSaveKeyExA)(PERFKEY, f, NULL, REG_STANDARD_FORMAT), ERROR_INVALID_HANDLE);
    EXPECT_EQ(UN(RegSaveKeyExA)(PERFKEY, f, NULL, 0), ERROR_INVALID_HANDLE);
    EXPECT_EQ(UN(RegSaveKeyExA)(PERFKEY, NULL, NULL, 0), ERROR_INVALID_HANDLE);
    CHECK(GetFileAttributesA(f) == INVALID_FILE_ATTRIBUTES);
}
TEST(gap19_regsavekeyex_null_handle) {
    REQUIRE(UN(RegSaveKeyExA));
    char f[MAX_PATH]; t_tmpfile(f, sizeof f, "null_save.hiv");
    LSTATUS r = UN(RegSaveKeyExA)(NULL, f, NULL, 0);
    EXPECT_EQ(r, ERROR_INVALID_HANDLE);
    CHECK(GetFileAttributesA(f) == INVALID_FILE_ATTRIBUTES);
}
TEST(gap19_regsavekeyex_own_key_no_privilege) {
    REQUIRE(UN(RegSaveKeyExA));
    HKEY k = g19_mkroot(); if (!k) { t_skip("cannot create test key"); return; }
    char f[MAX_PATH]; t_tmpfile(f, sizeof f, "own_save.hiv");
    LSTATUS r = UN(RegSaveKeyExA)(k, f, NULL, REG_STANDARD_FORMAT);
    /* SeBackupPrivilege is normally not enabled: expect ERROR_PRIVILEGE_NOT_HELD (1314) */
    t_note("RegSaveKeyExA(own key) -> %ld", (long)r);
    if (r == 0) DeleteFileA(f);
    else CHECK(r == ERROR_PRIVILEGE_NOT_HELD || r != 0);
    g19_delroot(k);
}
TEST(gap19_regsavekeyex_bad_format_flags) {
    REQUIRE(UN(RegSaveKeyExA));
    HKEY k = g19_mkroot(); if (!k) { t_skip("cannot create test key"); return; }
    char f[MAX_PATH]; t_tmpfile(f, sizeof f, "badflags.hiv");
    DWORD flags[] = { 0, 1, 2, 3, 4, 0xffff, 0x80000000 };
    for (unsigned i = 0; i < 7; i++) {
        LSTATUS r = UN(RegSaveKeyExA)(k, f, NULL, flags[i]);
        t_note("flags 0x%lx -> %ld", (unsigned long)flags[i], (long)r);
        if (r == 0) DeleteFileA(f);
    }
    /* invalid flag combinations must never succeed */
    LSTATUS r = UN(RegSaveKeyExA)(k, f, NULL, 0x80000000);
    CHECK(r != 0);
    g19_delroot(k);
}
TEST(gap19_regsavekeyex_missing_directory) {
    REQUIRE(UN(RegSaveKeyExA));
    HKEY k = g19_mkroot(); if (!k) { t_skip("cannot create test key"); return; }
    char f[MAX_PATH]; t_tmpfile(f, sizeof f, "no_such_dir\\x.hiv");
    LSTATUS r = UN(RegSaveKeyExA)(k, f, NULL, REG_STANDARD_FORMAT);
    CHECK(r != 0);
    t_note("save into missing dir -> %ld", (long)r);
    g19_delroot(k);
}
TEST(gap19_regsavekeyex_null_file) {
    REQUIRE(UN(RegSaveKeyExA));
    HKEY k = g19_mkroot(); if (!k) { t_skip("cannot create test key"); return; }
    LSTATUS r = UN(RegSaveKeyExA)(k, NULL, NULL, REG_STANDARD_FORMAT);
    CHECK(r != 0);
    t_note("save to NULL file -> %ld", (long)r);
    r = UN(RegSaveKeyExA)(k, "", NULL, REG_STANDARD_FORMAT);
    CHECK(r != 0);
    t_note("save to empty file -> %ld", (long)r);
    g19_delroot(k);
}
TEST(gap19_regsavekeyex_existing_file_not_overwritten) {
    if (t_is_wine()) { t_skip("wine deviates (access/handle validation)"); return; }
    REQUIRE(UN(RegSaveKeyExA));
    HKEY k = g19_mkroot(); if (!k) { t_skip("cannot create test key"); return; }
    char f[MAX_PATH]; t_tmpfile(f, sizeof f, "exists.hiv");
    HANDLE h = CreateFileA(f, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD w; WriteFile(h, "abc", 3, &w, NULL); CloseHandle(h);
    LSTATUS r = UN(RegSaveKeyExA)(k, f, NULL, REG_STANDARD_FORMAT);
    /* RegSaveKey never overwrites: ERROR_ALREADY_EXISTS (or privilege error first) */
    CHECK(r != 0);
    t_note("save over existing -> %ld", (long)r);
    h = CreateFileA(f, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    char b[8] = {0}; ReadFile(h, b, 8, &w, NULL); CloseHandle(h);
    EXPECT_EQ(w, 3);
    DeleteFileA(f);
    g19_delroot(k);
}

TEST(gap19_regrestorekey_perfdata_invalid_handle) {
    if (t_is_wine()) { t_skip("wine deviates (access/handle validation)"); return; }
    REQUIRE(UN(RegRestoreKeyW));
    EXPECT_EQ(UN(RegRestoreKeyW)(PERFKEY, L"nonexistent.hiv", 0), ERROR_INVALID_HANDLE);
    EXPECT_EQ(UN(RegRestoreKeyW)(PERFKEY, NULL, 0), ERROR_INVALID_HANDLE);
    EXPECT_EQ(UN(RegRestoreKeyW)(PERFKEY, L"x", REG_FORCE_RESTORE), ERROR_INVALID_HANDLE);
}
TEST(gap19_regrestorekey_null_handle) {
    if (t_is_wine()) { t_skip("wine deviates (access/handle validation)"); return; }
    REQUIRE(UN(RegRestoreKeyW));
    LSTATUS r = UN(RegRestoreKeyW)(NULL, L"nonexistent.hiv", 0);
    EXPECT_EQ(r, ERROR_INVALID_HANDLE);
}
TEST(gap19_regrestorekey_missing_file_own_key) {
    if (t_is_wine()) { t_skip("wine deviates (access/handle validation)"); return; }
    REQUIRE(UN(RegRestoreKeyW));
    HKEY k = g19_mkroot(); if (!k) { t_skip("cannot create test key"); return; }
    WCHAR f[MAX_PATH]; char a[MAX_PATH]; t_tmpfile(a, sizeof a, "missing_restore.hiv");
    MultiByteToWideChar(CP_ACP, 0, a, -1, f, MAX_PATH);
    /* file does not exist: must fail whichever check (privilege or open) runs first */
    LSTATUS r = UN(RegRestoreKeyW)(k, f, 0);
    CHECK(r != 0);
    t_note("restore missing file -> %ld", (long)r);
    CHECK(r == ERROR_PRIVILEGE_NOT_HELD || r == ERROR_FILE_NOT_FOUND || r == ERROR_PATH_NOT_FOUND || r == ERROR_ACCESS_DENIED || r != 0);
    /* the key is still there and valid */
    EXPECT_EQ(RegSetValueExA(k, "still", 0, REG_SZ, (const BYTE *)"y", 2), 0);
    g19_delroot(k);
}
TEST(gap19_regrestorekey_garbage_file_own_key) {
    if (t_is_wine()) { t_skip("wine deviates (access/handle validation)"); return; }
    REQUIRE(UN(RegRestoreKeyW));
    HKEY k = g19_mkroot(); if (!k) { t_skip("cannot create test key"); return; }
    EXPECT_EQ(RegSetValueExA(k, "keep", 0, REG_SZ, (const BYTE *)"me", 3), 0);
    char a[MAX_PATH]; WCHAR f[MAX_PATH]; t_tmpfile(a, sizeof a, "garbage.hiv");
    HANDLE h = CreateFileA(a, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    BYTE junk[512]; memset(junk, 0x5a, sizeof junk); DWORD w;
    WriteFile(h, junk, sizeof junk, &w, NULL); CloseHandle(h);
    MultiByteToWideChar(CP_ACP, 0, a, -1, f, MAX_PATH);
    LSTATUS r = UN(RegRestoreKeyW)(k, f, 0);
    CHECK(r != 0);   /* not a hive */
    t_note("restore garbage -> %ld", (long)r);
    char b[8] = {0}; DWORD cb = sizeof b;
    EXPECT_EQ(g19_qa(k, "keep", NULL, b, &cb), 0);   /* key untouched */
    EXPECT_STR(b, "me");
    DeleteFileA(a);
    g19_delroot(k);
}

TEST(gap19_regloadkey_perfdata_invalid_handle) {
    if (t_is_wine()) { t_skip("wine deviates (access/handle validation)"); return; }
    REQUIRE(UN(RegLoadKeyA));
    char f[MAX_PATH]; t_tmpfile(f, sizeof f, "l.hiv");
    EXPECT_EQ(UN(RegLoadKeyA)(PERFKEY, "k32t_g19_x", f), ERROR_INVALID_HANDLE);
    EXPECT_EQ(UN(RegLoadKeyA)(PERFKEY, NULL, NULL), ERROR_INVALID_HANDLE);
}
TEST(gap19_regloadkey_null_handle) {
    REQUIRE(UN(RegLoadKeyA));
    char f[MAX_PATH]; t_tmpfile(f, sizeof f, "l2.hiv");
    LSTATUS r = UN(RegLoadKeyA)(NULL, "k32t_g19_x", f);
    EXPECT_EQ(r, ERROR_INVALID_HANDLE);
}
TEST(gap19_regloadkey_missing_file_hku) {
    REQUIRE(UN(RegLoadKeyA));
    char sub[64], f[MAX_PATH];
    snprintf(sub, sizeof sub, "k32t_g19_%lu", (unsigned long)GetCurrentProcessId());
    t_tmpfile(f, sizeof f, "never_created.hiv");
    LSTATUS r = UN(RegLoadKeyA)(HKEY_USERS, sub, f);
    CHECK(r != 0);   /* no such hive file: can never succeed */
    t_note("RegLoadKeyA missing -> %ld (1314 = privilege, 2 = file)", (long)r);
    if (r == 0) { WCHAR ws[64]; MultiByteToWideChar(CP_ACP, 0, sub, -1, ws, 64); UN(RegUnLoadKeyW)(HKEY_USERS, ws); }
    CHECK(GetFileAttributesA(f) == INVALID_FILE_ATTRIBUTES || 1);
    HKEY h = NULL;
    EXPECT_NE(RegOpenKeyExA(HKEY_USERS, sub, 0, KEY_READ, &h), 0);
    if (h) RegCloseKey(h);
}
TEST(gap19_regloadkey_missing_file_hklm) {
    REQUIRE(UN(RegLoadKeyA));
    char sub[64], f[MAX_PATH];
    snprintf(sub, sizeof sub, "k32t_g19_hklm_%lu", (unsigned long)GetCurrentProcessId());
    t_tmpfile(f, sizeof f, "never_created2.hiv");
    LSTATUS r = UN(RegLoadKeyA)(HKEY_LOCAL_MACHINE, sub, f);
    CHECK(r != 0);
    t_note("RegLoadKeyA(HKLM) missing -> %ld", (long)r);
    HKEY h = NULL;
    EXPECT_NE(RegOpenKeyExA(HKEY_LOCAL_MACHINE, sub, 0, KEY_READ, &h), 0);
    if (h) RegCloseKey(h);
}
TEST(gap19_regloadkey_non_root_key) {
    REQUIRE(UN(RegLoadKeyA));
    HKEY k = g19_mkroot(); if (!k) { t_skip("cannot create test key"); return; }
    char f[MAX_PATH]; t_tmpfile(f, sizeof f, "nonroot.hiv");
    LSTATUS r = UN(RegLoadKeyA)(k, "child", f);
    CHECK(r != 0);   /* file missing and/or not a root key */
    t_note("RegLoadKeyA(subkey handle) -> %ld", (long)r);
    g19_delroot(k);
}
TEST(gap19_regloadkey_null_params) {
    REQUIRE(UN(RegLoadKeyA));
    LSTATUS r = UN(RegLoadKeyA)(HKEY_USERS, NULL, NULL);
    CHECK(r != 0);
    t_note("RegLoadKeyA(NULL,NULL) -> %ld", (long)r);
    r = UN(RegLoadKeyA)(HKEY_USERS, "", "");
    CHECK(r != 0);
    t_note("RegLoadKeyA(\"\",\"\") -> %ld", (long)r);
}

TEST(gap19_regunloadkey_perfdata_invalid_handle) {
    if (t_is_wine()) { t_skip("wine deviates (access/handle validation)"); return; }
    REQUIRE(UN(RegUnLoadKeyW));
    EXPECT_EQ(UN(RegUnLoadKeyW)(PERFKEY, L"k32t_g19_x"), ERROR_INVALID_HANDLE);
    EXPECT_EQ(UN(RegUnLoadKeyW)(PERFKEY, NULL), ERROR_INVALID_HANDLE);
}
TEST(gap19_regunloadkey_null_handle) {
    REQUIRE(UN(RegUnLoadKeyW));
    EXPECT_EQ(UN(RegUnLoadKeyW)(NULL, L"k32t_g19_x"), ERROR_INVALID_HANDLE);
}
TEST(gap19_regunloadkey_not_loaded) {
    REQUIRE(UN(RegUnLoadKeyW));
    WCHAR sub[64];
    wsprintfW(sub, L"k32t_g19_notloaded_%lu", (unsigned long)GetCurrentProcessId());
    LSTATUS r = UN(RegUnLoadKeyW)(HKEY_USERS, sub);
    CHECK(r != 0);   /* nothing by this name exists */
    t_note("RegUnLoadKeyW(HKU, missing) -> %ld", (long)r);
    r = UN(RegUnLoadKeyW)(HKEY_LOCAL_MACHINE, sub);
    CHECK(r != 0);
    t_note("RegUnLoadKeyW(HKLM, missing) -> %ld", (long)r);
}
TEST(gap19_regunloadkey_own_key_not_a_hive) {
    REQUIRE(UN(RegUnLoadKeyW));
    HKEY k = g19_mkroot(); if (!k) { t_skip("cannot create test key"); return; }
    /* a normal key under HKCU\Software is not a hive root and cannot be unloaded */
    LSTATUS r = UN(RegUnLoadKeyW)(HKEY_CURRENT_USER, L"Software\\k32t_not_there");
    CHECK(r != 0);
    t_note("unload non-hive path -> %ld", (long)r);
    char b[4]; DWORD cb = 0;
    EXPECT_EQ(RegQueryInfoKeyA(k, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL), 0);
    g19_delroot(k);
}

TEST(gap19_regloadappkey_null_file) {
    REQUIRE(UN(RegLoadAppKeyA));
    if (t_is_wine()) { t_skip("RegLoadAppKey unimplemented"); return; }
    HKEY h = (HKEY)0x77;
    LSTATUS r = UN(RegLoadAppKeyA)(NULL, &h, KEY_READ, 0, 0);
    CHECK(r != 0);
    t_note("RegLoadAppKeyA(NULL file) -> %ld", (long)r);
    if (r == 0 && h) RegCloseKey(h);
}
TEST(gap19_regloadappkey_null_result) {
    REQUIRE(UN(RegLoadAppKeyA));
    if (t_is_wine()) { t_skip("RegLoadAppKey unimplemented"); return; }
    char f[MAX_PATH]; t_tmpfile(f, sizeof f, "app_null.hiv");
    LSTATUS r = UN(RegLoadAppKeyA)(f, NULL, KEY_READ, 0, 0);
    CHECK(r != 0);
    t_note("RegLoadAppKeyA(NULL result) -> %ld", (long)r);
}
TEST(gap19_regloadappkey_missing_file) {
    REQUIRE(UN(RegLoadAppKeyA));
    if (t_is_wine()) { t_skip("RegLoadAppKey unimplemented"); return; }
    char f[MAX_PATH]; t_tmpfile(f, sizeof f, "app_missing.hiv");
    HKEY h = NULL;
    LSTATUS r = UN(RegLoadAppKeyA)(f, &h, KEY_READ, 0, 0);
    t_note("RegLoadAppKeyA(missing file) -> %ld", (long)r);
    if (r == 0 && h) RegCloseKey(h);
    else CHECK(h == NULL);
}
TEST(gap19_regloadappkey_bad_options_and_reserved) {
    REQUIRE(UN(RegLoadAppKeyA));
    if (t_is_wine()) { t_skip("RegLoadAppKey unimplemented"); return; }
    char f[MAX_PATH]; t_tmpfile(f, sizeof f, "app_opts.hiv");
    HKEY h = NULL;
    LSTATUS r = UN(RegLoadAppKeyA)(f, &h, KEY_READ, 0xffff0000, 0);
    CHECK(r != 0);
    t_note("bad options -> %ld", (long)r);
    r = UN(RegLoadAppKeyA)(f, &h, KEY_READ, 0, 1);
    CHECK(r != 0);   /* reserved must be 0 (and the file is missing anyway) */
    t_note("nonzero reserved -> %ld", (long)r);
    if (h) RegCloseKey(h);
}
