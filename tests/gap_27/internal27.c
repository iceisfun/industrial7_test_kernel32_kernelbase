#include "t.h"
/* Non-public / internal exports: presence, module ownership, export-table
 * shape (ordinals, forwarders) and the side-effect-free helpers whose
 * behaviour was verified from the kernelbase / kernel32 reference analysis
 * (SHTruncateString, StrCpyNX{A,W}, StrChrA_MB, TermsrvAppInstallMode).
 * Everything with system-wide side effects is only resolved, never called. */

static const struct { const char *n; int k32, kb; } ex[] = {
{"SHLoadIndirectStringInternal",0,1}, {"SHTruncateString",0,1}, {"SaveAlternatePackageRootPath",0,1}, {"SaveStateRootFolderPath",0,1}, {"SetClientDynamicTimeZoneInformation",0,1}, {"SetClientTimeZoneInformation",0,1}, {"SetComPlusPackageInstallStatus",1,0}, {"SetComputerNameEx2W",1,1}, {"SetConsoleCursor",1,0}, {"SetConsoleHardwareState",1,0}, {"SetConsoleIcon",1,0}, {"SetConsoleKeyShortcuts",1,0}, {"SetConsoleLocalEUDC",1,0}, {"SetConsoleNlsMode",1,0}, {"SetConsoleOS2OemFormat",1,0}, {"SetConsolePalette",1,0}, {"SetExtensionProperty",0,1}, {"SetIsDeveloperModeEnabled",0,1}, {"SetIsSideloadingEnabled",0,1}, {"SetLocalPrimaryComputerNameA",1,0}, {"SetLocalPrimaryComputerNameW",1,0}, {"SetProcessGroupAffinity",0,1}, {"SetProcessValidCallTargetsForMappedView",0,1}, {"SetProtocolProperty",0,1}, {"SetRoamingLastObservedChangeTime",0,1}, {"SetStateVersion",0,1}, {"SetSystemTimeAdjustmentPrecise",0,1}, {"SetTermsrvAppInstallMode",1,0}, {"SetThreadPreferredUILanguages2",0,1}, {"SetVDMCurrentDirectories",1,0}, {"ShowConsoleCursor",1,0}, {"SortCloseHandle",1,0}, {"SortGetHandle",1,0}, {"SpecialMBToWC",0,1}, {"StrChrA_MB",0,1}, {"StrCpyNXA",0,1}, {"StrCpyNXW",0,1}, {"SubscribeStateChangeNotification",0,1}, {"TerminateProcessOnMemoryExhaustion",0,1}, {"TermsrvAppInstallMode",1,0}, {"TermsrvConvertSysRootToUserDir",1,0}, {"TermsrvCreateRegEntry",1,0}, {"TermsrvDeleteKey",1,0}, {"TermsrvDeleteValue",1,0}, {"TermsrvGetPreSetValue",1,0}, {"TermsrvGetWindowsDirectoryA",1,0}, {"TermsrvGetWindowsDirectoryW",1,0}, {"TermsrvOpenRegEntry",1,0}, {"TermsrvOpenUserClasses",1,0}, {"TermsrvRestoreKey",1,0},
};
#define NEX (sizeof ex / sizeof ex[0])

/* ---- PE export directory walker ---- */
typedef struct { DWORD ordinal; DWORD rva; int fwd; char fwdname[160]; int found; } expinfo;
static int pe_export(HMODULE m, const char *name, expinfo *o) {
  memset(o, 0, sizeof *o);
  BYTE *b = (BYTE *)m;
  IMAGE_DOS_HEADER *dh = (IMAGE_DOS_HEADER *)b;
  IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(b + dh->e_lfanew);
  IMAGE_DATA_DIRECTORY dd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
  if (!dd.VirtualAddress) return 0;
  IMAGE_EXPORT_DIRECTORY *ed = (IMAGE_EXPORT_DIRECTORY *)(b + dd.VirtualAddress);
  DWORD *names = (DWORD *)(b + ed->AddressOfNames), *funcs = (DWORD *)(b + ed->AddressOfFunctions);
  WORD *ords = (WORD *)(b + ed->AddressOfNameOrdinals);
  for (DWORD i = 0; i < ed->NumberOfNames; i++) {
    if (strcmp((const char *)(b + names[i]), name)) continue;
    DWORD rva = funcs[ords[i]];
    o->found = 1; o->ordinal = ords[i] + ed->Base; o->rva = rva;
    if (rva >= dd.VirtualAddress && rva < dd.VirtualAddress + dd.Size) {
      o->fwd = 1; strncpy(o->fwdname, (const char *)(b + rva), sizeof o->fwdname - 1);
    }
    return 1;
  }
  return 0;
}

TEST(gap27_exports_resolve) {
  for (size_t i = 0; i < NEX; i++) {
    FARPROC p = t_proc(ex[i].n);
    if (!p && t_is_wine()) continue;
    CHECKF(p != NULL, "%s missing", ex[i].n);
  }
}
TEST(gap27_exports_in_expected_module) {
  if (t_is_wine()) { t_skip("module layout differs on wine"); return; }
  for (size_t i = 0; i < NEX; i++) {
    if (ex[i].k32) CHECKF(t_proc_k32(ex[i].n) != NULL, "%s not in kernel32", ex[i].n);
    if (ex[i].kb)  CHECKF(t_proc_kb(ex[i].n) != NULL, "%s not in kernelbase", ex[i].n);
    if (!ex[i].k32 && t_proc_k32(ex[i].n)) t_note("%s also resolvable via kernel32", ex[i].n);
    if (!ex[i].kb && t_proc_kb(ex[i].n)) t_note("%s also resolvable via kernelbase", ex[i].n);
  }
}
TEST(gap27_exports_stable) {
  for (size_t i = 0; i < NEX; i++) CHECKF(t_proc(ex[i].n) == t_proc(ex[i].n), "%s", ex[i].n);
}
TEST(gap27_exports_case_and_decoration) {
  CHECK(t_proc("shtruncatestring") == NULL);
  CHECK(t_proc("STRCPYNXA") == NULL);
  CHECK(t_proc("StrCpyNXA ") == NULL);
  CHECK(t_proc("_StrCpyNXA@12") == NULL);
  CHECK(t_proc("StrCpyNX") == NULL);
  CHECK(t_proc("Termsrv") == NULL);
}
TEST(gap27_exports_unique_names) {
  for (size_t i = 0; i < NEX; i++)
    for (size_t j = i + 1; j < NEX; j++) CHECKF(strcmp(ex[i].n, ex[j].n), "dup %s", ex[i].n);
}
TEST(gap27_export_table_entries) {
  if (t_is_wine()) { t_skip("export layout differs on wine"); return; }
  for (size_t i = 0; i < NEX; i++) {
    expinfo e;
    if (ex[i].k32) {
      CHECKF(pe_export(t_kernel32(), ex[i].n, &e), "%s not in kernel32 export table", ex[i].n);
      CHECKF(e.ordinal >= 1 && e.ordinal < 4000, "%s ordinal %lu", ex[i].n, (unsigned long)e.ordinal);
      if (e.fwd) t_note("kernel32!%s forwards to %s", ex[i].n, e.fwdname);
    }
    if (ex[i].kb) {
      CHECKF(pe_export(t_kernelbase(), ex[i].n, &e), "%s not in kernelbase export table", ex[i].n);
      CHECKF(e.ordinal >= 1 && e.ordinal < 4000, "%s ordinal %lu", ex[i].n, (unsigned long)e.ordinal);
      if (e.fwd) t_note("kernelbase!%s forwards to %s", ex[i].n, e.fwdname);
    }
  }
}
TEST(gap27_not_forwarders_when_implemented) {
  if (t_is_wine()) { t_skip("export layout differs on wine"); return; }
  /* reference analysis shows real code bodies for these */
  static const char *kb[] = {"SHTruncateString","StrCpyNXA","StrCpyNXW","StrChrA_MB","SetProcessGroupAffinity","SetStateVersion"};
  static const char *k3[] = {"TermsrvAppInstallMode","SortGetHandle","SortCloseHandle","TermsrvGetWindowsDirectoryA","TermsrvGetWindowsDirectoryW"};
  expinfo e;
  for (size_t i = 0; i < sizeof kb / sizeof *kb; i++)
    if (pe_export(t_kernelbase(), kb[i], &e)) CHECKF(!e.fwd, "kernelbase!%s is a forwarder to %s", kb[i], e.fwdname);
  for (size_t i = 0; i < sizeof k3 / sizeof *k3; i++)
    if (pe_export(t_kernel32(), k3[i], &e)) CHECKF(!e.fwd, "kernel32!%s is a forwarder to %s", k3[i], e.fwdname);
}
TEST(gap27_sort_close_handle_alias) {
  if (t_is_wine()) { t_skip("alias layout differs on wine"); return; }
  FARPROC a = t_proc_k32("SortCloseHandle"), b = t_proc_k32("BaseFreeAppCompatDataForProcessWorker");
  REQUIRE(a);
  if (!b) { t_note("BaseFreeAppCompatDataForProcessWorker not exported"); return; }
  CHECK(a == b);   /* reference analysis: same RVA carries both names */
}
TEST(gap27_distinct_entry_points) {
  FARPROC a = t_proc("StrCpyNXA"), b = t_proc("StrCpyNXW"), c = t_proc("SHTruncateString"), d = t_proc("StrChrA_MB");
  REQUIRE(a); REQUIRE(b); REQUIRE(c); REQUIRE(d);
  CHECK(a != b); CHECK(a != c); CHECK(a != d); CHECK(b != c); CHECK(c != d);
}
TEST(gap27_exports_within_image) {
  if (t_is_wine()) { t_skip("image layout differs on wine"); return; }
  for (size_t i = 0; i < NEX; i++) {
    FARPROC p = t_proc_kb(ex[i].n); HMODULE m = t_kernelbase();
    if (!p) { p = t_proc_k32(ex[i].n); m = t_kernel32(); }
    if (!p) continue;
    HMODULE owner = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &owner);
    CHECKF(owner == m || owner == t_ntdll(), "%s lives in unexpected module", ex[i].n);
  }
}

/* ---- SHTruncateString(BYTE *buf, int cb): ASCII only so ACP lead bytes are irrelevant ---- */
typedef int (WINAPI *shtrunc_t)(BYTE *, int);
TEST(gap27_shtruncatestring_null) {
  shtrunc_t f = (shtrunc_t)t_proc_kb("SHTruncateString"); REQUIRE(f);
  CHECK(f(NULL, 10) == 0);
  CHECK(f(NULL, 0) == 0);
  CHECK(f(NULL, -1) == 0);
}
TEST(gap27_shtruncatestring_nonpositive_size) {
  shtrunc_t f = (shtrunc_t)t_proc_kb("SHTruncateString"); REQUIRE(f);
  if (t_is_wine()) { t_skip("wine SHTruncateString differs"); return; }
  BYTE b[8] = "abcdefg";
  CHECK(f(b, 0) == 0);  CHECK(!memcmp(b, "abcdefg", 8));
  CHECK(f(b, -5) == 0); CHECK(!memcmp(b, "abcdefg", 8));
  CHECK(f(b, 0x80000000) == 0); CHECK(!memcmp(b, "abcdefg", 8));
}
TEST(gap27_shtruncatestring_size_one) {
  shtrunc_t f = (shtrunc_t)t_proc_kb("SHTruncateString"); REQUIRE(f);
  if (t_is_wine()) { t_skip("wine SHTruncateString differs"); return; }
  BYTE b[4] = "abc";
  CHECK(f(b, 1) == 0);
  CHECK(b[0] == 0 && b[1] == 'b' && b[2] == 'c');
}
TEST(gap27_shtruncatestring_sizes) {
  shtrunc_t f = (shtrunc_t)t_proc_kb("SHTruncateString"); REQUIRE(f);
  if (t_is_wine()) { t_skip("wine SHTruncateString differs"); return; }
  for (int cb = 1; cb <= 16; cb++) {
    BYTE b[32]; memset(b, 'x', sizeof b);
    int r = f(b, cb);
    CHECKF(r == cb - 1, "cb=%d r=%d", cb, r);
    CHECKF(b[cb - 1] == 0, "cb=%d no terminator", cb);
    for (int k = 0; k < cb - 1; k++) CHECKF(b[k] == 'x', "cb=%d byte %d clobbered", cb, k);
    CHECKF(b[cb] == 'x', "cb=%d wrote past terminator", cb);
  }
}
TEST(gap27_shtruncatestring_in_place) {
  shtrunc_t f = (shtrunc_t)t_proc_kb("SHTruncateString"); REQUIRE(f);
  if (t_is_wine()) { t_skip("wine SHTruncateString differs"); return; }
  BYTE b[16] = "hello world";
  CHECK(f(b, 6) == 5);
  CHECK(!strcmp((char *)b, "hello"));
  CHECK(f(b, 16) == 15);   /* writes terminator at index 15 */
  CHECK(b[15] == 0);
}

/* ---- StrCpyNXA / StrCpyNXW(dst, src, cch): returns pointer to written terminator ---- */
typedef char *(WINAPI *cpya_t)(char *, const char *, int);
typedef WCHAR *(WINAPI *cpyw_t)(WCHAR *, const WCHAR *, int);
TEST(gap27_strcpynxa_basic) {
  cpya_t f = (cpya_t)t_proc_kb("StrCpyNXA"); REQUIRE(f);
  char d[16]; memset(d, 'Z', sizeof d);
  char *r = f(d, "abc", 16);
  CHECK(r == d + 3); CHECK(!strcmp(d, "abc")); CHECK(d[4] == 'Z');
}
TEST(gap27_strcpynxa_truncation) {
  cpya_t f = (cpya_t)t_proc_kb("StrCpyNXA"); REQUIRE(f);
  for (int n = 1; n <= 8; n++) {
    char d[16]; memset(d, 'Z', sizeof d);
    char *r = f(d, "abcdef", n);   /* length 6 */
    int want = n - 1 < 6 ? n - 1 : 6;
    CHECKF(r == d + want, "n=%d ret off %d want %d", n, (int)(r - d), want);
    CHECKF(d[want] == 0, "n=%d terminator", n);
    CHECKF(!strncmp(d, "abcdef", want), "n=%d content", n);
    CHECKF(d[want + 1] == 'Z', "n=%d overran", n);
  }
}
TEST(gap27_strcpynxa_exact_fit) {
  cpya_t f = (cpya_t)t_proc_kb("StrCpyNXA"); REQUIRE(f);
  char d[8]; memset(d, 'Z', sizeof d);
  char *r = f(d, "abcd", 5);    /* exactly fits with terminator */
  CHECK(r == d + 4); CHECK(!strcmp(d, "abcd")); CHECK(d[5] == 'Z');
  memset(d, 'Z', sizeof d);
  r = f(d, "abcd", 4);          /* one short */
  CHECK(r == d + 3); CHECK(!strcmp(d, "abc"));
}
TEST(gap27_strcpynxa_nonpositive_count) {
  cpya_t f = (cpya_t)t_proc_kb("StrCpyNXA"); REQUIRE(f);
  char d[4] = "QQQ";
  CHECK(f(d, "abc", 0) == d);  CHECK(!strcmp(d, "QQQ"));
  CHECK(f(d, "abc", -1) == d); CHECK(!strcmp(d, "QQQ"));
  CHECK(f(d, "abc", (int)0x80000000) == d); CHECK(!strcmp(d, "QQQ"));
}
TEST(gap27_strcpynxa_count_one) {
  cpya_t f = (cpya_t)t_proc_kb("StrCpyNXA"); REQUIRE(f);
  char d[4] = "QQQ";
  CHECK(f(d, "abc", 1) == d);
  CHECK(d[0] == 0 && d[1] == 'Q');
}
TEST(gap27_strcpynxa_null_src) {
  cpya_t f = (cpya_t)t_proc_kb("StrCpyNXA"); REQUIRE(f);
  if (t_is_wine()) { t_skip("wine NULL src handling differs"); return; }
  char d[4] = "QQQ";
  CHECK(f(d, NULL, 4) == d);
  CHECK(d[0] == 0 && d[1] == 'Q');
  char e[4] = "QQQ";
  CHECK(f(e, NULL, 0) == e); CHECK(!strcmp(e, "QQQ"));   /* count checked first */
}
TEST(gap27_strcpynxa_empty_src) {
  cpya_t f = (cpya_t)t_proc_kb("StrCpyNXA"); REQUIRE(f);
  char d[4] = "QQQ";
  CHECK(f(d, "", 4) == d); CHECK(d[0] == 0 && d[1] == 'Q');
}
TEST(gap27_strcpynxa_chaining) {
  cpya_t f = (cpya_t)t_proc_kb("StrCpyNXA"); REQUIRE(f);
  char d[16]; memset(d, 0, sizeof d);
  char *p = f(d, "ab", 16);
  p = f(p, "cd", (int)(sizeof d - (p - d)));
  p = f(p, "efghijkl", (int)(sizeof d - (p - d)));
  CHECK(p == d + 12);
  CHECK(!strcmp(d, "abcdefghijkl"));
}
TEST(gap27_strcpynxw_basic) {
  cpyw_t f = (cpyw_t)t_proc_kb("StrCpyNXW"); REQUIRE(f);
  WCHAR d[16]; for (int i = 0; i < 16; i++) d[i] = L'Z';
  WCHAR *r = f(d, L"abc", 16);
  CHECK(r == d + 3); CHECK(!lstrcmpW(d, L"abc")); CHECK(d[4] == L'Z');
}
TEST(gap27_strcpynxw_truncation) {
  cpyw_t f = (cpyw_t)t_proc_kb("StrCpyNXW"); REQUIRE(f);
  for (int n = 1; n <= 8; n++) {
    WCHAR d[16]; for (int i = 0; i < 16; i++) d[i] = L'Z';
    WCHAR *r = f(d, L"abcdef", n);
    int want = n - 1 < 6 ? n - 1 : 6;
    CHECKF(r == d + want, "n=%d ret off %d want %d", n, (int)(r - d), want);
    CHECKF(d[want] == 0, "n=%d terminator", n);
    CHECKF(!wcsncmp(d, L"abcdef", want), "n=%d content", n);
    CHECKF(d[want + 1] == L'Z', "n=%d overran", n);
  }
}
TEST(gap27_strcpynxw_nonpositive_count) {
  cpyw_t f = (cpyw_t)t_proc_kb("StrCpyNXW"); REQUIRE(f);
  WCHAR d[4] = {L'Q', L'Q', L'Q', 0};
  CHECK(f(d, L"abc", 0) == d);  CHECK(d[0] == L'Q');
  CHECK(f(d, L"abc", -7) == d); CHECK(d[0] == L'Q');
}
TEST(gap27_strcpynxw_null_src_and_one) {
  cpyw_t f = (cpyw_t)t_proc_kb("StrCpyNXW"); REQUIRE(f);
  if (t_is_wine()) { t_skip("wine NULL src handling differs"); return; }
  WCHAR d[4] = {L'Q', L'Q', L'Q', 0};
  CHECK(f(d, NULL, 4) == d); CHECK(d[0] == 0 && d[1] == L'Q');
  WCHAR e[4] = {L'Q', L'Q', L'Q', 0};
  CHECK(f(e, L"abc", 1) == e); CHECK(e[0] == 0 && e[1] == L'Q');
}
TEST(gap27_strcpynxw_non_ascii) {
  cpyw_t f = (cpyw_t)t_proc_kb("StrCpyNXW"); REQUIRE(f);
  static const WCHAR s[] = {0x00e9, 0x4e2d, 0xd83d, 0xde00, 0xffff, 0};
  WCHAR d[8] = {0};
  WCHAR *r = f(d, s, 8);
  CHECK(r == d + 5); CHECK(!memcmp(d, s, sizeof s));
}

/* ---- StrChrA_MB(str, wchar, flag) ---- */
typedef const char *(WINAPI *chrmb_t)(const char *, WORD, int);
TEST(gap27_strchra_mb_single_byte_mode) {
  chrmb_t f = (chrmb_t)t_proc_kb("StrChrA_MB"); REQUIRE(f);
  const char *s = "hello world";
  CHECK(f(s, 'h', 0) == s);
  CHECK(f(s, 'o', 0) == s + 4);
  CHECK(f(s, 'd', 0) == s + 10);
  CHECK(f(s, 'z', 0) == NULL);
}
TEST(gap27_strchra_mb_terminator_not_matched) {
  chrmb_t f = (chrmb_t)t_proc_kb("StrChrA_MB"); REQUIRE(f);
  CHECK(f("abc", 0, 0) == NULL);
  CHECK(f("", 'a', 0) == NULL);
  CHECK(f("", 0, 0) == NULL);
  CHECK(f("", 'a', 1) == NULL);
}
TEST(gap27_strchra_mb_low_byte_only) {
  chrmb_t f = (chrmb_t)t_proc_kb("StrChrA_MB"); REQUIRE(f);
  const char *s = "abc";
  CHECK(f(s, 0x0162, 0) == s + 1);   /* flag 0 compares only the low byte 'b' */
}
TEST(gap27_strchra_mb_checked_mode_ascii) {
  chrmb_t f = (chrmb_t)t_proc_kb("StrChrA_MB"); REQUIRE(f);
  const char *s = "hello world";
  CHECK(f(s, 'h', 1) == s);
  CHECK(f(s, 'w', 1) == s + 6);
  CHECK(f(s, 'z', 1) == NULL);
  CHECK(f(s, 'l', 7) == s + 2);      /* any nonzero flag selects the DBCS-aware path */
}
TEST(gap27_strchra_mb_first_match) {
  chrmb_t f = (chrmb_t)t_proc_kb("StrChrA_MB"); REQUIRE(f);
  const char *s = "aXaXa";
  CHECK(f(s, 'X', 0) == s + 1);
  CHECK(f(s, 'X', 1) == s + 1);
  CHECK(f(s, 'a', 0) == s);
}
TEST(gap27_strchra_mb_case_sensitive) {
  chrmb_t f = (chrmb_t)t_proc_kb("StrChrA_MB"); REQUIRE(f);
  CHECK(f("abc", 'B', 0) == NULL);
  CHECK(f("abc", 'B', 1) == NULL);
}

/* ---- TermsrvAppInstallMode: reads a flag from BaseStaticServerData ---- */
TEST(gap27_termsrv_appinstallmode_readonly) {
  UDECL(BYTE, TermsrvAppInstallMode, (void));
  TermsrvAppInstallMode_pfn f = UN(TermsrvAppInstallMode); REQUIRE(f);
  if (t_is_wine()) { t_skip("stub on wine"); return; }
  BYTE a = f(), b = f();
  CHECK(a <= 1);
  CHECK(a == b);
  if (a) t_note("TS install mode active on this host");
}
TEST(gap27_termsrv_appinstallmode_preserves_lasterror) {
  UDECL(BYTE, TermsrvAppInstallMode, (void));
  TermsrvAppInstallMode_pfn f = UN(TermsrvAppInstallMode); REQUIRE(f);
  if (t_is_wine()) { t_skip("stub on wine"); return; }
  SET_ERR(0xBEEF);
  f();
  EXPECT_ERR(0xBEEF);
}

/* ---- SetProcessGroupAffinity: only the NULL-handle failure path (cannot change anything) ---- */
TEST(gap27_setprocessgroupaffinity_null_handle) {
  typedef BOOL (WINAPI *spga_t)(HANDLE, const GROUP_AFFINITY *, GROUP_AFFINITY *);
  spga_t f = (spga_t)t_proc_kb("SetProcessGroupAffinity"); REQUIRE(f);
  if (t_is_wine()) { t_skip("not implemented on wine"); return; }
  GROUP_AFFINITY ga; memset(&ga, 0, sizeof ga); ga.Mask = 1; ga.Group = 0;
  SET_ERR(0);
  BOOL r = f(NULL, &ga, NULL);
  CHECK(r == FALSE);
  DWORD e = GetLastError();
  CHECKF(e != 0, "expected an error code");
  t_note("SetProcessGroupAffinity(NULL) error %lu", (unsigned long)e);
}
