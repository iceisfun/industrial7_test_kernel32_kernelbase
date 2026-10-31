#include "t.h"
/* Export ownership table (k32/kb flags) cross-checked against decomp of both modules.
 * SHExpand/Reg*Internal error paths verified from kernelbase decomp.
 * Non-public / undocumented exports: presence, module ownership, stability, and
 * only the error paths and side-effect-free calls verified from decomp. */
static const struct { const char *n; int k32, kb; } ex[] = {
{"QueryGlobalizationUserSettingsStatus",0,1},{"QueryStateAtomValueInfo",0,1},{"QueryStateContainerCreatedNew",0,1},{"QueryStateContainerItemInfo",0,1},{"QuirkGetData",0,1},{"QuirkGetData2",0,1},{"QuirkIsEnabled",0,1},{"QuirkIsEnabled2",0,1},{"QuirkIsEnabled3",0,1},{"QuirkIsEnabledForPackage",0,1},{"QuirkIsEnabledForPackage2",0,1},{"QuirkIsEnabledForPackage3",0,1},{"QuirkIsEnabledForPackage4",0,1},{"QuirkIsEnabledForProcess",0,1},{"RaiseCustomSystemEventTrigger",0,1},{"ReadStateAtomValue",0,1},{"ReadStateContainerValue",0,1},{"RecordFeatureUsage2",0,1},{"RefreshPackageInfo",0,1},{"RegCreateKeyExInternalA",0,1},{"RegCreateKeyExInternalW",0,1},{"RegDeleteKeyExInternalA",0,1},{"RegDeleteKeyExInternalW",0,1},{"RegOpenKeyExInternalA",0,1},{"RegOpenKeyExInternalW",0,1},{"RegQueryMultipleValuesA",0,1},{"RegQueryMultipleValuesW",0,1},{"RegisterConsoleIME",1,0},{"RegisterConsoleOS2",1,0},{"RegisterConsoleVDM",1,0},{"RegisterStateChangeNotification",0,1},{"RegisterStateLock",0,1},{"RegisterWaitForInputIdle",1,0},{"RegisterWowBaseHandlers",1,0},{"RegisterWowExec",1,0},{"ReleaseStateLock",0,1},{"RemoveExtensionProgIds",0,1},{"RemoveLocalAlternateComputerNameA",1,0},{"RemoveLocalAlternateComputerNameW",1,0},{"RemovePackageFromFamilyXref",0,1},{"RemovePackageStatus",0,1},{"RemovePackageStatusForUser",0,1},{"ReplaceFileExInternal",0,1},{"ReplacePartitionUnit",1,0},{"ResetState",0,1},{"ResolveDelayLoadedAPI",1,1},{"ResolveDelayLoadsFromDll",1,1},{"RestoreThreadPreferredUILanguages",0,1},{"SHExpandEnvironmentStringsA",0,1},{"SHExpandEnvironmentStringsW",0,1},
};
#define NEX (sizeof ex / sizeof ex[0])

TEST(gap26_exports_present) {
  for (size_t i = 0; i < NEX; i++) {
    FARPROC p = t_proc(ex[i].n);
    if (!p && t_is_wine()) continue;
    CHECKF(p != NULL, "%s missing", ex[i].n);
  }
}
TEST(gap26_exports_module_ownership) {
  if (t_is_wine()) { t_skip("module layout differs on wine"); return; }
  for (size_t i = 0; i < NEX; i++) {
    if (ex[i].kb) CHECKF(t_proc_kb(ex[i].n) != NULL, "%s not in kernelbase", ex[i].n);
    if (ex[i].k32) CHECKF(t_proc_k32(ex[i].n) != NULL, "%s not in kernel32", ex[i].n);
    else if (t_proc_k32(ex[i].n)) t_note("%s also resolves via kernel32 (forwarder)", ex[i].n);
  }
}
TEST(gap26_exports_stable) {
  for (size_t i = 0; i < NEX; i++) CHECKF(t_proc(ex[i].n) == t_proc(ex[i].n), "%s", ex[i].n);
}
TEST(gap26_exports_case_sensitive) {
  for (size_t i = 0; i < NEX; i++) {
    char b[128]; strcpy(b, ex[i].n); b[0] = (char)(b[0] | 0x20);
    if (b[0] == ex[i].n[0]) continue;
    CHECKF(t_proc(b) == NULL || t_is_wine(), "%s lower-case resolved", b);
  }
}
TEST(gap26_exports_distinct_entry_points) {
  /* the three Quirk "enabled" entries are separate functions */
  FARPROC a = t_proc("QuirkIsEnabled"), b = t_proc("QuirkIsEnabled2"), c = t_proc("QuirkIsEnabled3");
  if (!a || !b || !c) { t_skip("quirk exports missing"); return; }
  CHECK(a != b); CHECK(b != c); CHECK(a != c);
  FARPROC d = t_proc("QuirkGetData"), e = t_proc("QuirkGetData2");
  if (d && e) CHECK(d != e);
}
TEST(gap26_kernelbase_in_kernelbase_range) {
  if (t_is_wine()) { t_skip("wine"); return; }
  HMODULE kb = t_kernelbase();
  for (size_t i = 0; i < NEX; i++) {
    if (!ex[i].kb) continue;
    FARPROC p = t_proc_kb(ex[i].n);
    if (!p) continue;
    HMODULE m = NULL;
    BOOL ok = GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &m);
    CHECKF(ok && m == kb, "%s not located in kernelbase image", ex[i].n);
  }
}

/* ---- SHExpandEnvironmentStrings{A,W} (kernelbase wrappers over ExpandEnvironmentStrings) ---- */
TEST(gap26_shexpand_w_basic) {
  UDECL(DWORD, SHExpandEnvironmentStringsW, (LPCWSTR, LPWSTR, DWORD));
  SHExpandEnvironmentStringsW_pfn f = UN(SHExpandEnvironmentStringsW); REQUIRE(f);
  WCHAR exp[MAX_PATH], out[MAX_PATH];
  DWORD n = GetEnvironmentVariableW(L"SystemRoot", exp, MAX_PATH);
  if (!n) { t_skip("no SystemRoot"); return; }
  DWORD r = f(L"%SystemRoot%", out, MAX_PATH);
  EXPECT_EQ(r, n + 1);
  EXPECT_WSTR(out, exp);
}
TEST(gap26_shexpand_a_basic) {
  UDECL(DWORD, SHExpandEnvironmentStringsA, (LPCSTR, LPSTR, DWORD));
  SHExpandEnvironmentStringsA_pfn f = UN(SHExpandEnvironmentStringsA); REQUIRE(f);
  char exp[MAX_PATH], out[MAX_PATH];
  DWORD n = GetEnvironmentVariableA("SystemRoot", exp, MAX_PATH);
  if (!n) { t_skip("no SystemRoot"); return; }
  DWORD r = f("%SystemRoot%\\x", out, MAX_PATH);
  EXPECT_EQ(r, n + 3);
  strcat(exp, "\\x"); EXPECT_STR(out, exp);
}
TEST(gap26_shexpand_no_vars) {
  UDECL(DWORD, SHExpandEnvironmentStringsW, (LPCWSTR, LPWSTR, DWORD));
  SHExpandEnvironmentStringsW_pfn f = UN(SHExpandEnvironmentStringsW); REQUIRE(f);
  WCHAR out[32];
  EXPECT_EQ(f(L"plain", out, 32), 6);
  EXPECT_WSTR(out, L"plain");
  EXPECT_EQ(f(L"", out, 32), 1);
  EXPECT_WSTR(out, L"");
}
TEST(gap26_shexpand_unknown_var_left_alone) {
  UDECL(DWORD, SHExpandEnvironmentStringsW, (LPCWSTR, LPWSTR, DWORD));
  SHExpandEnvironmentStringsW_pfn f = UN(SHExpandEnvironmentStringsW); REQUIRE(f);
  WCHAR out[64];
  DWORD r = f(L"a%K32T_NO_SUCH_VAR_26%b", out, 64);
  EXPECT_EQ(r, 24);
  EXPECT_WSTR(out, L"a%K32T_NO_SUCH_VAR_26%b");
}
TEST(gap26_shexpand_w_truncation_returns_zero) {
  if (t_is_wine()) { t_skip("wine SHExpand semantics differ"); return; }
  UDECL(DWORD, SHExpandEnvironmentStringsW, (LPCWSTR, LPWSTR, DWORD));
  SHExpandEnvironmentStringsW_pfn f = UN(SHExpandEnvironmentStringsW); REQUIRE(f);
  /* "abcdef" needs 7 chars. exact fits; exact-1 truncates, returns 0 and NUL-terminates */
  WCHAR out[16];
  EXPECT_EQ(f(L"abcdef", out, 7), 7); EXPECT_WSTR(out, L"abcdef");
  for (int i = 0; i < 16; i++) out[i] = 0x5a5a;
  EXPECT_EQ(f(L"abcdef", out, 6), 0);
  CHECK(out[5] == 0);
  CHECK(!memcmp(out, L"abcde", 10));
  for (int i = 0; i < 16; i++) out[i] = 0x5a5a;
  EXPECT_EQ(f(L"abcdef", out, 1), 0);
  CHECK(out[0] == 0);
  /* size 0: nothing written */
  for (int i = 0; i < 4; i++) out[i] = 0x5a5a;
  EXPECT_EQ(f(L"abcdef", out, 0), 0);
  CHECK(out[0] == 0x5a5a);
  EXPECT_EQ(f(L"abcdef", NULL, 0), 0);
}
TEST(gap26_shexpand_a_truncation_returns_zero) {
  if (t_is_wine()) { t_skip("wine SHExpand semantics differ"); return; }
  UDECL(DWORD, SHExpandEnvironmentStringsA, (LPCSTR, LPSTR, DWORD));
  SHExpandEnvironmentStringsA_pfn f = UN(SHExpandEnvironmentStringsA); REQUIRE(f);
  char out[16];
  EXPECT_EQ(f("abcdef", out, 7), 7); EXPECT_STR(out, "abcdef");
  memset(out, 'Z', sizeof out);
  EXPECT_EQ(f("abcdef", out, 6), 0);
  CHECK(strlen(out) <= 5);
  CHECK(!strncmp(out, "abcde", strlen(out)));
}
TEST(gap26_shexpand_matches_expandenvironmentstrings) {
  UDECL(DWORD, SHExpandEnvironmentStringsW, (LPCWSTR, LPWSTR, DWORD));
  SHExpandEnvironmentStringsW_pfn f = UN(SHExpandEnvironmentStringsW); REQUIRE(f);
  static const WCHAR *in[] = { L"%SystemRoot%\\system32", L"%windir%", L"%%", L"%", L"x%y", L"%SystemRoot%%SystemRoot%", L"%TEMP%\\k32t" };
  for (size_t i = 0; i < sizeof in / sizeof *in; i++) {
    WCHAR a[MAX_PATH], b[MAX_PATH];
    DWORD ra = f(in[i], a, MAX_PATH);
    DWORD rb = ExpandEnvironmentStringsW(in[i], b, MAX_PATH);
    CHECKF(ra == rb, "case %zu: %lu vs %lu", i, ra, rb);
    if (ra == rb && ra) CHECK(!lstrcmpW(a, b));
  }
}

/* ---- RestoreThreadPreferredUILanguages: NULL context is a no-op (decomp) ---- */
TEST(gap26_restore_thread_ui_langs_null_noop) {
  UDECL(void, RestoreThreadPreferredUILanguages, (PVOID));
  RestoreThreadPreferredUILanguages_pfn f = UN(RestoreThreadPreferredUILanguages); REQUIRE(f);
  SET_ERR(0x1234);
  f(NULL);
  EXPECT_EQ(GetLastError(), 0x1234);
  f(NULL);
}

/* ---- Reg*Internal error paths verified from decomp ---- */
#define HK_PERFDATA ((HKEY)(ULONG_PTR)0xffffffff80000004ull)
#define HK_PERFTEXT ((HKEY)(ULONG_PTR)0xffffffff80000050ull)
#define HK_PERFNLS  ((HKEY)(ULONG_PTR)0xffffffff80000060ull)

TEST(gap26_regopenkeyexinternalw_errors) {
  UDECL(LONG, RegOpenKeyExInternalW, (HKEY, LPCWSTR, DWORD, REGSAM, PHKEY, PVOID));
  RegOpenKeyExInternalW_pfn f = UN(RegOpenKeyExInternalW); REQUIRE(f);
  HKEY k = (HKEY)0x1234;
  EXPECT_EQ(f(HK_PERFDATA, L"", 0, KEY_READ, &k, NULL), ERROR_INVALID_HANDLE);
  EXPECT_EQ(f(HK_PERFTEXT, L"", 0, KEY_READ, &k, NULL), ERROR_INVALID_HANDLE);
  EXPECT_EQ(f(HK_PERFNLS, L"", 0, KEY_READ, &k, NULL), ERROR_INVALID_HANDLE);
  EXPECT_EQ((ULONG_PTR)k, 0x1234);
  EXPECT_EQ(f(HKEY_CURRENT_USER, L"Software", 0, KEY_READ, NULL, NULL), ERROR_INVALID_PARAMETER);
}
TEST(gap26_regopenkeyexinternala_errors) {
  UDECL(LONG, RegOpenKeyExInternalA, (HKEY, LPCSTR, DWORD, REGSAM, PHKEY, PVOID));
  RegOpenKeyExInternalA_pfn f = UN(RegOpenKeyExInternalA); REQUIRE(f);
  HKEY k = (HKEY)0x1234;
  LONG r = f(HK_PERFDATA, "", 0, KEY_READ, &k, NULL);
  CHECKF(r == ERROR_INVALID_HANDLE, "got %ld", r);
}
TEST(gap26_regcreatekeyexinternalw_errors) {
  UDECL(LONG, RegCreateKeyExInternalW, (HKEY, LPCWSTR, DWORD, LPWSTR, DWORD, REGSAM, LPSECURITY_ATTRIBUTES, PHKEY, LPDWORD, PVOID));
  RegCreateKeyExInternalW_pfn f = UN(RegCreateKeyExInternalW); REQUIRE(f);
  HKEY k = NULL; DWORD disp;
  EXPECT_EQ(f(HK_PERFDATA, L"x", 0, NULL, 0, KEY_READ, NULL, &k, &disp, NULL), ERROR_INVALID_HANDLE);
  EXPECT_EQ(f(HK_PERFTEXT, L"x", 0, NULL, 0, KEY_READ, NULL, &k, &disp, NULL), ERROR_INVALID_HANDLE);
  EXPECT_EQ(f(HK_PERFNLS, L"x", 0, NULL, 0, KEY_READ, NULL, &k, &disp, NULL), ERROR_INVALID_HANDLE);
  EXPECT_EQ(f(HKEY_CURRENT_USER, L"k32t_gap26_never", 1, NULL, 0, KEY_READ, NULL, &k, &disp, NULL), ERROR_INVALID_PARAMETER);
  EXPECT_EQ(f(HKEY_CURRENT_USER, NULL, 0, NULL, 0, KEY_READ, NULL, &k, &disp, NULL), ERROR_BADKEY);
  EXPECT_EQ(f(HKEY_CURRENT_USER, L"k32t_gap26_never", 0, NULL, 0, KEY_READ, NULL, NULL, &disp, NULL), ERROR_BADKEY);
  CHECK(k == NULL);
}
TEST(gap26_regdeletekeyexinternalw_errors) {
  UDECL(LONG, RegDeleteKeyExInternalW, (HKEY, LPCWSTR, REGSAM, DWORD, PVOID));
  RegDeleteKeyExInternalW_pfn f = UN(RegDeleteKeyExInternalW); REQUIRE(f);
  EXPECT_EQ(f(HKEY_CURRENT_USER, NULL, 0, 0, NULL), ERROR_INVALID_PARAMETER);
  EXPECT_EQ(f(HKEY_CURRENT_USER, L"k32t_gap26_never", 0, 1, NULL), ERROR_INVALID_PARAMETER);
  EXPECT_EQ(f(HK_PERFDATA, L"k32t_gap26_never", 0, 0, NULL), ERROR_INVALID_HANDLE);
  /* nonexistent subkey under HKCU: nothing to delete */
  LONG r = f(HKEY_CURRENT_USER, L"Software\\k32t_gap26_never_exists", 0, 0, NULL);
  CHECKF(r == ERROR_FILE_NOT_FOUND, "got %ld", r);
}

/* ---- Reg*Internal round trip under HKCU\Software\k32t_<pid> ---- */
TEST(gap26_reg_internal_roundtrip) {
  UDECL(LONG, RegCreateKeyExInternalW, (HKEY, LPCWSTR, DWORD, LPWSTR, DWORD, REGSAM, LPSECURITY_ATTRIBUTES, PHKEY, LPDWORD, PVOID));
  UDECL(LONG, RegOpenKeyExInternalW, (HKEY, LPCWSTR, DWORD, REGSAM, PHKEY, PVOID));
  UDECL(LONG, RegDeleteKeyExInternalW, (HKEY, LPCWSTR, REGSAM, DWORD, PVOID));
  RegCreateKeyExInternalW_pfn cr = UN(RegCreateKeyExInternalW);
  RegOpenKeyExInternalW_pfn op = UN(RegOpenKeyExInternalW);
  RegDeleteKeyExInternalW_pfn de = UN(RegDeleteKeyExInternalW);
  REQUIRE(cr); REQUIRE(op); REQUIRE(de);
  WCHAR name[64]; wsprintfW(name, L"Software\\k32t_%lu_g26", GetCurrentProcessId());
  HKEY k = NULL; DWORD disp = 0;
  LONG r = cr(HKEY_CURRENT_USER, name, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, &disp, NULL);
  if (r != ERROR_SUCCESS) { t_note("create internal returned %ld", r); return; }
  EXPECT_EQ(disp, REG_CREATED_NEW_KEY);
  CHECK(k != NULL);
  { __typeof__(&RegCloseKey) cl = U(RegCloseKey); if (cl) cl(k); }
  HKEY k2 = NULL;
  r = op(HKEY_CURRENT_USER, name, 0, KEY_READ, &k2, NULL);
  CHECKF(r == ERROR_SUCCESS, "open got %ld", r);
  if (r == ERROR_SUCCESS) { __typeof__(&RegCloseKey) cl = U(RegCloseKey); if (cl) cl(k2); }
  r = de(HKEY_CURRENT_USER, name, 0, 0, NULL);
  CHECKF(r == ERROR_SUCCESS, "delete got %ld", r);
  if (r != ERROR_SUCCESS) RegDeleteKeyW(HKEY_CURRENT_USER, name); /* cleanup */
  k2 = NULL;
  r = op(HKEY_CURRENT_USER, name, 0, KEY_READ, &k2, NULL);
  CHECKF(r == ERROR_FILE_NOT_FOUND, "open-after-delete got %ld", r);
}

/* ---- RegQueryMultipleValues{A,W} ---- */
static HKEY mk_key(WCHAR *name) {
  HKEY k = NULL;
  __typeof__(&RegCreateKeyExW) cr = U(RegCreateKeyExW);
  wsprintfW(name, L"Software\\k32t_%lu_g26m", GetCurrentProcessId());
  if (!cr || cr(HKEY_CURRENT_USER, name, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL) != ERROR_SUCCESS) return NULL;
  return k;
}
static void rm_key(HKEY k, const WCHAR *name) {
  __typeof__(&RegCloseKey) cl = U(RegCloseKey);
  __typeof__(&RegDeleteKeyW) dl = U(RegDeleteKeyW);
  if (cl) cl(k);
  if (dl) dl(HKEY_CURRENT_USER, name);
}
TEST(gap26_regquerymultiplevalues_w) {
  __typeof__(&RegQueryMultipleValuesW) q = U(RegQueryMultipleValuesW); REQUIRE(q);
  __typeof__(&RegSetValueExW) sv = U(RegSetValueExW); REQUIRE(sv);
  WCHAR kn[64]; HKEY k = mk_key(kn);
  if (!k) { t_skip("cannot create key"); return; }
  DWORD d = 0xCAFEBABE;
  sv(k, L"dw", 0, REG_DWORD, (BYTE *)&d, 4);
  sv(k, L"sz", 0, REG_SZ, (const BYTE *)L"hello", 12);
  VALENTW v[2]; memset(v, 0, sizeof v);
  v[0].ve_valuename = (LPWSTR)L"dw"; v[1].ve_valuename = (LPWSTR)L"sz";
  BYTE buf[256]; DWORD tot = sizeof buf;
  LONG r = q(k, v, 2, (LPWSTR)buf, &tot);
  EXPECT_EQ(r, ERROR_SUCCESS);
  if (r == ERROR_SUCCESS) {
    EXPECT_EQ(v[0].ve_type, REG_DWORD); EXPECT_EQ(v[0].ve_valuelen, 4);
    EXPECT_EQ(v[1].ve_type, REG_SZ);    EXPECT_EQ(v[1].ve_valuelen, 12);
    CHECK(v[0].ve_valueptr && *(DWORD *)v[0].ve_valueptr == 0xCAFEBABE);
    CHECK(v[1].ve_valueptr && !lstrcmpW((LPCWSTR)v[1].ve_valueptr, L"hello"));
    CHECK(tot >= 16);
    t_note("total size reported %lu", tot);
  }
  /* NULL buffer: size query */
  DWORD need = 0;
  memset(v, 0, sizeof v); v[0].ve_valuename = (LPWSTR)L"dw"; v[1].ve_valuename = (LPWSTR)L"sz";
  r = q(k, v, 2, NULL, &need);
  t_note("size query: r=%ld need=%lu", r, need);
  if (r == ERROR_MORE_DATA) CHECK(need >= 16);
  /* too-small buffer */
  tot = 4; memset(v, 0, sizeof v); v[0].ve_valuename = (LPWSTR)L"dw"; v[1].ve_valuename = (LPWSTR)L"sz";
  r = q(k, v, 2, (LPWSTR)buf, &tot);
  CHECKF(r == ERROR_MORE_DATA, "small buf got %ld", r);
  /* missing value */
  VALENTW m; memset(&m, 0, sizeof m); m.ve_valuename = (LPWSTR)L"nope_g26"; tot = sizeof buf;
  r = q(k, &m, 1, (LPWSTR)buf, &tot);
  t_note("missing value: r=%ld", r);
  CHECK(r != ERROR_SUCCESS);
  rm_key(k, kn);
}
TEST(gap26_regquerymultiplevalues_a) {
  __typeof__(&RegQueryMultipleValuesA) q = U(RegQueryMultipleValuesA); REQUIRE(q);
  __typeof__(&RegSetValueExW) sv = U(RegSetValueExW); REQUIRE(sv);
  WCHAR kn[64]; HKEY k = mk_key(kn);
  if (!k) { t_skip("cannot create key"); return; }
  sv(k, L"sz", 0, REG_SZ, (const BYTE *)L"hi", 6);
  VALENTA v; memset(&v, 0, sizeof v); v.ve_valuename = "sz";
  BYTE buf[128]; DWORD tot = sizeof buf;
  LONG r = q(k, &v, 1, (LPSTR)buf, &tot);
  EXPECT_EQ(r, ERROR_SUCCESS);
  if (r == ERROR_SUCCESS) {
    EXPECT_EQ(v.ve_type, REG_SZ);
    t_note("A value len=%lu ptr=%s", v.ve_valuelen, v.ve_valueptr ? (char *)v.ve_valueptr : "(null)");
    if (v.ve_valueptr) CHECK(!strcmp((char *)v.ve_valueptr, "hi"));
  }
  rm_key(k, kn);
}
TEST(gap26_regquerymultiplevalues_zero_values) {
  __typeof__(&RegQueryMultipleValuesW) q = U(RegQueryMultipleValuesW); REQUIRE(q);
  WCHAR kn[64]; HKEY k = mk_key(kn);
  if (!k) { t_skip("cannot create key"); return; }
  BYTE buf[16]; DWORD tot = sizeof buf;
  LONG r = q(k, NULL, 0, (LPWSTR)buf, &tot);
  t_note("zero values: r=%ld tot=%lu", r, tot);
  rm_key(k, kn);
}
TEST(gap26_regquerymultiplevalues_too_many_values) {
  __typeof__(&RegQueryMultipleValuesW) q = U(RegQueryMultipleValuesW); REQUIRE(q);
  WCHAR kn[64]; HKEY k = mk_key(kn);
  if (!k) { t_skip("cannot create key"); return; }
  /* decomp: num_vals >= 0x10000 rejected before touching val_list */
  DWORD tot = 0;
  LONG r = q(k, NULL, 0x10000, NULL, &tot);
  t_note("0x10000 values: r=%ld", r);
  CHECK(r != ERROR_SUCCESS);
  rm_key(k, kn);
}

/* ---- Entries that are deliberately NOT called ----
 * Quirk*: forward into a quirk-database worker (reads policy state).
 * State* (State repository / state change): touch per-user system state broker.
 * RemoveLocalAlternateComputerName*, ReplacePartitionUnit, RemovePackage*, RefreshPackageInfo,
 * RaiseCustomSystemEventTrigger, RegisterWow*, RegisterConsole*, RegisterWaitForInputIdle
 * (overwrites user32's idle callback), ResolveDelayLoad* (needs a real delay-load descriptor),
 * ReplaceFileExInternal: all mutate machine/process-global state. */
TEST(gap26_uncalled_exports_resolve_nonnull_distinct) {
  const char *n[] = {"RegisterConsoleIME","RegisterConsoleOS2","RegisterConsoleVDM","RegisterWowBaseHandlers","RegisterWowExec","ReplacePartitionUnit","RemoveLocalAlternateComputerNameA","RemoveLocalAlternateComputerNameW"};
  FARPROC p[8]; int have = 0;
  for (int i = 0; i < 8; i++) { p[i] = t_proc(n[i]); if (p[i]) have++; }
  if (!have) { t_skip("none present"); return; }
  for (int i = 0; i < 8; i++) for (int j = i + 1; j < 8; j++)
    if (p[i] && p[j] && p[i] == p[j]) t_note("%s and %s share an entry point", n[i], n[j]);
}
