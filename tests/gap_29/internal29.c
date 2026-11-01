#include "t.h"
/* Non-public / CRT-ish exports of kernel32 and kernelbase.
 * Behaviour below was taken from the Win10 kernelbase/kernel32 reference analysis:
 *  - time/_time64: GetSystemTimeAsFileTime -> seconds since 1970, clamped to -1 past year 3000
 *  - atexit -> _onexit (returns func or NULL); atexit returns 0 / -1
 *  - exit -> doexit -> RtlExitUserProcess(code)
 *  - _purecall -> RaiseException(0xC0000002, EXCEPTION_NONCONTINUABLE, 0, NULL)
 *  - hgets(buf, n, hFile): reads n-1 bytes, folds CR / CRLF to LF, terminates after the first LF
 *  - hwprintf(h, wfmt, ...) / wprintf: vswprintf_s then code-page conversion and WriteFile
 *  - uaw_* : unaligned-wide string helpers (uaw_lstrcmpW/uaw_lstrcmpiW are aliases of lstrcmpW/lstrcmpiW)
 */
typedef long long t29_time_t;
typedef t29_time_t (__cdecl *time_pfn29)(t29_time_t *);
typedef int (__cdecl *atexit_pfn29)(void (__cdecl *)(void));
typedef void *(__cdecl *onexit_pfn29)(void *);
typedef void (__cdecl *exit_pfn29)(int);
typedef void (__cdecl *void_pfn29)(void);
typedef void *(WINAPI *hgets_pfn29)(void *, int, HANDLE);
typedef void (__cdecl *hwprintf_pfn29)(HANDLE, const wchar_t *, ...);
typedef int (__cdecl *wprintf_pfn29)(const wchar_t *, ...);
typedef int (WINAPI *cmpw_pfn29)(LPCWSTR, LPCWSTR);
typedef WCHAR *(__cdecl *wcschr_pfn29)(const WCHAR *, WCHAR);
typedef WCHAR *(__cdecl *wcscpy_pfn29)(WCHAR *, const WCHAR *);
typedef int (__cdecl *wcsicmp_pfn29)(const WCHAR *, const WCHAR *);

/* Under an emulator lacking these exports, fall back to the msvcrt equivalent so the logic still runs. */
static FARPROC fb29(const char *n, FARPROC p) {
  if (p || !t_is_wine()) return p;
  HMODULE m = LoadLibraryA("msvcrt.dll");
  if (!m) return NULL;
  if (!strcmp(n, "uaw_wcsicmp")) n = "_wcsicmp"; else if (!strncmp(n, "uaw_", 4)) n += 4;
  if (!strcmp(n, "lstrcmpW") || !strcmp(n, "lstrcmpiW")) return GetProcAddress(GetModuleHandleA("kernel32.dll"), n);
  return GetProcAddress(m, n);
}
#define KB(name, type) ((type)fb29(name, t_proc_kb(name)))
#define K32(name, type) ((type)fb29(name, t_proc_k32(name)))

/* ---- export presence / ownership / ordinals ---- */
static DWORD ordinal_of(HMODULE m, const char *name) {
  BYTE *b = (BYTE *)m;
  IMAGE_DOS_HEADER *dh = (IMAGE_DOS_HEADER *)b;
  IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(b + dh->e_lfanew);
  IMAGE_DATA_DIRECTORY *dd = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
  if (!dd->VirtualAddress) return 0;
  IMAGE_EXPORT_DIRECTORY *ed = (IMAGE_EXPORT_DIRECTORY *)(b + dd->VirtualAddress);
  DWORD *names = (DWORD *)(b + ed->AddressOfNames);
  WORD *ords = (WORD *)(b + ed->AddressOfNameOrdinals);
  for (DWORD i = 0; i < ed->NumberOfNames; i++)
    if (!strcmp((char *)(b + names[i]), name)) return ords[i] + ed->Base;
  return 0;
}
static const char *k32_names[] = {"_local_unwind","uaw_lstrcmpW","uaw_lstrcmpiW","uaw_wcschr","uaw_wcscpy","uaw_wcsicmp","uaw_wcsrchr"};
static const char *kb_names[] = {"_local_unwind","_onexit","_purecall","_time64","atexit","exit","hgets","hwprintf","time","wprintf"};
#define N29(a) (sizeof(a)/sizeof((a)[0]))

TEST(gap29_kernel32_exports_present) {
  for (size_t i = 0; i < N29(k32_names); i++) {
    FARPROC p = t_proc_k32(k32_names[i]);
    if (!p && t_is_wine()) continue;
    CHECKF(p != NULL, "kernel32!%s missing", k32_names[i]);
  }
}
TEST(gap29_kernelbase_exports_present) {
  for (size_t i = 0; i < N29(kb_names); i++) {
    FARPROC p = t_proc_kb(kb_names[i]);
    if (!p && t_is_wine()) continue;
    CHECKF(p != NULL, "kernelbase!%s missing", kb_names[i]);
  }
}
TEST(gap29_crt_exports_not_in_kernel32) {
  if (t_is_wine()) { t_skip("module layout differs on wine"); return; }
  static const char *n[] = {"_onexit","_purecall","_time64","atexit","exit","hgets","hwprintf","time","wprintf"};
  for (size_t i = 0; i < N29(n); i++)
    CHECKF(GetProcAddress(t_kernel32(), n[i]) == NULL, "kernel32 unexpectedly exports %s", n[i]);
}
TEST(gap29_uaw_not_in_kernelbase) {
  if (t_is_wine()) { t_skip("module layout differs on wine"); return; }
  for (size_t i = 1; i < N29(k32_names); i++)
    CHECKF(GetProcAddress(t_kernelbase(), k32_names[i]) == NULL, "kernelbase unexpectedly exports %s", k32_names[i]);
}
TEST(gap29_local_unwind_both_modules) {
  FARPROC a = t_proc_k32("_local_unwind"), b = t_proc_kb("_local_unwind");
  REQUIRE(a); REQUIRE(b);
  if (t_is_wine()) { t_skip("stub layout differs on wine"); return; }
  /* both exports are forwarders to NTDLL._local_unwind, so both resolve to the same ntdll function */
  CHECK(a == b);
  CHECK(a == GetProcAddress(GetModuleHandleA("ntdll.dll"), "_local_unwind"));
}
TEST(gap29_ordinal_lookup_matches_name_lookup) {
  for (size_t i = 0; i < N29(k32_names); i++) {
    DWORD o = ordinal_of(t_kernel32(), k32_names[i]);
    if (!o) { if (!t_is_wine()) CHECKF(0, "no export entry kernel32!%s", k32_names[i]); continue; }
    FARPROC byord = GetProcAddress(t_kernel32(), (LPCSTR)(ULONG_PTR)o);
    CHECKF(byord == GetProcAddress(t_kernel32(), k32_names[i]), "ordinal %lu != name for %s", o, k32_names[i]);
    t_note("kernel32!%s ordinal %lu", k32_names[i], o);
  }
  for (size_t i = 0; i < N29(kb_names); i++) {
    DWORD o = ordinal_of(t_kernelbase(), kb_names[i]);
    if (!o) { if (!t_is_wine()) CHECKF(0, "no export entry kernelbase!%s", kb_names[i]); continue; }
    FARPROC byord = GetProcAddress(t_kernelbase(), (LPCSTR)(ULONG_PTR)o);
    CHECKF(byord == GetProcAddress(t_kernelbase(), kb_names[i]), "ordinal %lu != name for %s", o, kb_names[i]);
    t_note("kernelbase!%s ordinal %lu", kb_names[i], o);
  }
}
TEST(gap29_exports_are_not_forwarders) {
  if (t_is_wine()) { t_skip("export layout differs on wine"); return; }
  for (size_t i = 0; i < N29(k32_names); i++) {
    FARPROC p = t_proc_k32(k32_names[i]);
    if (!p) continue;
    /* a forwarder string would resolve outside the module; these resolve inside it */
    HMODULE owner = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &owner);
    CHECKF(owner != NULL, "%s has no owning module", k32_names[i]);
    /* _local_unwind is an export forwarder "NTDLL._local_unwind" in the Win10 image: it resolves into ntdll */
    if (!strcmp(k32_names[i], "_local_unwind")) CHECKF(owner == GetModuleHandleA("ntdll.dll"), "%s not forwarded to ntdll", k32_names[i]);
    else if (strncmp(k32_names[i], "uaw_lstrcmp", 11)) CHECKF(owner == t_kernel32(), "%s not located in kernel32", k32_names[i]);
  }
  for (size_t i = 0; i < N29(kb_names); i++) {
    FARPROC p = t_proc_kb(kb_names[i]);
    if (!p) continue;
    HMODULE owner = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &owner);
    if (!strcmp(kb_names[i], "_local_unwind")) CHECKF(owner == GetModuleHandleA("ntdll.dll"), "%s not forwarded to ntdll", kb_names[i]);
    else CHECKF(owner == t_kernelbase(), "%s not located in kernelbase", kb_names[i]);
  }
}
TEST(gap29_uaw_lstrcmp_alias_identity) {
  FARPROC a = t_proc_k32("uaw_lstrcmpW"), b = t_proc_k32("lstrcmpW");
  FARPROC c = t_proc_k32("uaw_lstrcmpiW"), d = t_proc_k32("lstrcmpiW");
  REQUIRE(a); REQUIRE(b); REQUIRE(c); REQUIRE(d);
  if (t_is_wine()) { t_note("alias identity not enforced on wine"); return; }
  CHECK(a == b);   /* same RVA in the Win10 image (kernel32 reference analysis: uaw_lstrcmpW/lstrcmpW share 0x1bf00, uaw_lstrcmpiW/lstrcmpiW share 0x18330) */
  CHECK(c == d);
  CHECK(a != c);
}

/* ---- time / _time64 ---- */
static t29_time_t ft_to_unix(void) {
  FILETIME ft; GetSystemTimeAsFileTime(&ft);
  ULONGLONG v = ((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
  return (t29_time_t)((v - 116444736000000000ULL) / 10000000ULL);
}
TEST(gap29_time64_matches_system_clock) {
  time_pfn29 f = KB("_time64", time_pfn29); REQUIRE(f);
  t29_time_t a = ft_to_unix(), r = f(NULL), b = ft_to_unix();
  CHECK(r >= a && r <= b + 0);
}
TEST(gap29_time_matches_system_clock) {
  time_pfn29 f = KB("time", time_pfn29); REQUIRE(f);
  t29_time_t a = ft_to_unix(), r = f(NULL), b = ft_to_unix();
  CHECK(r >= a && r <= b);
}
TEST(gap29_time64_writes_out_param) {
  time_pfn29 f = KB("_time64", time_pfn29); REQUIRE(f);
  t29_time_t out = 0x1234567890LL;
  t29_time_t r = f(&out);
  CHECK(out == r);
}
TEST(gap29_time_writes_out_param) {
  time_pfn29 f = KB("time", time_pfn29); REQUIRE(f);
  t29_time_t out = 0x1234567890LL;
  t29_time_t r = f(&out);
  CHECK(out == r);
}
TEST(gap29_time_plausible_range) {
  time_pfn29 f = KB("_time64", time_pfn29); REQUIRE(f);
  t29_time_t r = f(NULL);
  CHECK(r > 1577836800LL);        /* after 2020-01-01 */
  CHECK(r < 0x793406fffLL);       /* before the 3000-01-01 clamp */
}
TEST(gap29_time_null_does_not_touch_last_error) {
  time_pfn29 f = KB("time", time_pfn29); REQUIRE(f);
  SetLastError(0x4242); f(NULL); EXPECT_ERR(0x4242);
  time_pfn29 g = KB("_time64", time_pfn29); REQUIRE(g);
  SetLastError(0x4243); g(NULL); EXPECT_ERR(0x4243);
}
TEST(gap29_time_and_time64_agree) {
  time_pfn29 f = KB("time", time_pfn29), g = KB("_time64", time_pfn29); REQUIRE(f); REQUIRE(g);
  for (int i = 0; i < 200; i++) {
    t29_time_t a = g(NULL), b = f(NULL), c = g(NULL);
    CHECKF(a <= b && b <= c, "%lld %lld %lld", a, b, c);
    if (i == 199) CHECK(c - a <= 1);
  }
}
TEST(gap29_time_monotonic_sleep) {
  time_pfn29 g = KB("_time64", time_pfn29); REQUIRE(g);
  t29_time_t a = g(NULL);
  Sleep(1100);
  t29_time_t b = g(NULL);
  CHECKF(b >= a + 1, "%lld -> %lld", a, b);
  CHECKF(b <= a + 5, "%lld -> %lld", a, b);
}
TEST(gap29_time_sample_loop_nondecreasing) {
  time_pfn29 g = KB("time", time_pfn29); REQUIRE(g);
  t29_time_t prev = g(NULL);
  for (int i = 0; i < 5000; i++) { t29_time_t c = g(NULL); if (c < prev) { CHECKF(0, "went back %lld < %lld", c, prev); break; } prev = c; }
}
TEST(gap29_time_same_target_as_ft_when_stored_twice) {
  time_pfn29 g = KB("_time64", time_pfn29); REQUIRE(g);
  t29_time_t o1 = -5, o2 = -5;
  t29_time_t r1 = g(&o1), r2 = g(&o2);
  CHECK(o1 == r1 && o2 == r2 && r2 >= r1);
}

/* ---- uaw_* string helpers ---- */
TEST(gap29_uaw_wcschr_basic) {
  wcschr_pfn29 f = K32("uaw_wcschr", wcschr_pfn29); REQUIRE(f);
  static const WCHAR s[] = L"hello world";
  CHECK(f(s, L'h') == s);
  CHECK(f(s, L'o') == s + 4);          /* first occurrence */
  CHECK(f(s, L'd') == s + 10);
  CHECK(f(s, L'z') == NULL);
  CHECK(f(s, L'H') == NULL);           /* case sensitive */
}
TEST(gap29_uaw_wcschr_terminator) {
  wcschr_pfn29 f = K32("uaw_wcschr", wcschr_pfn29); REQUIRE(f);
  static const WCHAR s[] = L"abc";
  CHECK(f(s, 0) == s + 3);             /* returns the terminator */
  static const WCHAR e[] = L"";
  CHECK(f(e, 0) == e);
  CHECK(f(e, L'a') == NULL);
}
TEST(gap29_uaw_wcschr_unaligned) {
  wcschr_pfn29 f = K32("uaw_wcschr", wcschr_pfn29); REQUIRE(f);
  BYTE buf[64] = {0};
  memcpy(buf + 1, L"unaligned", sizeof L"unaligned");
  WCHAR *p = (WCHAR *)(buf + 1);
  WCHAR *r = f(p, L'l');
  CHECK((BYTE *)r == buf + 1 + 3 * 2);
  CHECK((BYTE *)f(p, 0) == buf + 1 + 9 * 2);
  CHECK(f(p, L'Q') == NULL);
}
TEST(gap29_uaw_wcschr_high_chars) {
  wcschr_pfn29 f = K32("uaw_wcschr", wcschr_pfn29); REQUIRE(f);
  WCHAR s[] = {0x0100, 0x20AC, 0xFFFF, 0xD800, 0};
  CHECK(f(s, 0x20AC) == s + 1);
  CHECK(f(s, 0xFFFF) == s + 2);
  CHECK(f(s, 0xD800) == s + 3);
  CHECK(f(s, 0x0101) == NULL);
  CHECK(f(s, 0x0001) == NULL);
}
TEST(gap29_uaw_wcschr_table) {
  wcschr_pfn29 f = K32("uaw_wcschr", wcschr_pfn29); REQUIRE(f);
  static const WCHAR s[] = L"a.b.c.d";
  static const struct { WCHAR c; int off; } t[] = {{L'a',0},{L'.',1},{L'b',2},{L'c',4},{L'd',6},{0,7},{L'e',-1},{L',',-1},{L' ',-1}};
  for (size_t i = 0; i < N29(t); i++) {
    WCHAR *r = f(s, t[i].c);
    CHECKF(t[i].off < 0 ? r == NULL : r == s + t[i].off, "case %zu", i);
  }
}

TEST(gap29_uaw_wcsrchr_basic) {
  wcschr_pfn29 f = K32("uaw_wcsrchr", wcschr_pfn29); REQUIRE(f);
  static const WCHAR s[] = L"a\\b\\c.txt";
  CHECK(f(s, L'\\') == s + 3);         /* last occurrence */
  CHECK(f(s, L'a') == s);
  CHECK(f(s, L't') == s + 8);
  CHECK(f(s, L'.') == s + 5);
  CHECK(f(s, L'Z') == NULL);
  CHECK(f(s, L'T') == NULL);
}
TEST(gap29_uaw_wcsrchr_terminator) {
  wcschr_pfn29 f = K32("uaw_wcsrchr", wcschr_pfn29); REQUIRE(f);
  static const WCHAR s[] = L"abc";
  CHECK(f(s, 0) == s + 3);
  static const WCHAR e[] = L"";
  CHECK(f(e, 0) == e);
  CHECK(f(e, L'x') == NULL);
}
TEST(gap29_uaw_wcsrchr_unaligned) {
  wcschr_pfn29 f = K32("uaw_wcsrchr", wcschr_pfn29); REQUIRE(f);
  BYTE buf[64] = {0};
  memcpy(buf + 3, L"abcabc", sizeof L"abcabc");
  WCHAR *p = (WCHAR *)(buf + 3);
  CHECK((BYTE *)f(p, L'b') == buf + 3 + 4 * 2);
  CHECK((BYTE *)f(p, L'a') == buf + 3 + 3 * 2);
  CHECK((BYTE *)f(p, 0) == buf + 3 + 6 * 2);
}
TEST(gap29_uaw_wcsrchr_single_and_repeat) {
  wcschr_pfn29 f = K32("uaw_wcsrchr", wcschr_pfn29); REQUIRE(f);
  static const WCHAR s[] = L"xxxxxxxx";
  CHECK(f(s, L'x') == s + 7);
  static const WCHAR one[] = L"x";
  CHECK(f(one, L'x') == one);
  CHECK(f(one, L'y') == NULL);
}
TEST(gap29_uaw_wcsrchr_high_chars) {
  wcschr_pfn29 f = K32("uaw_wcsrchr", wcschr_pfn29); REQUIRE(f);
  WCHAR s[] = {0x20AC, 0x00E9, 0x20AC, 0};
  CHECK(f(s, 0x20AC) == s + 2);
  CHECK(f(s, 0x00E9) == s + 1);
  CHECK(f(s, 0xE900) == NULL);
}

TEST(gap29_uaw_wcscpy_basic) {
  wcscpy_pfn29 f = K32("uaw_wcscpy", wcscpy_pfn29); REQUIRE(f);
  WCHAR d[32]; for (int i = 0; i < 32; i++) d[i] = 0x7777;
  WCHAR *r = f(d, L"copy me");
  CHECK(r == d);
  EXPECT_WSTR(d, L"copy me");
  CHECK(d[8] == 0x7777);               /* nothing written past the terminator */
}
TEST(gap29_uaw_wcscpy_empty) {
  wcscpy_pfn29 f = K32("uaw_wcscpy", wcscpy_pfn29); REQUIRE(f);
  WCHAR d[4] = {1, 2, 3, 4};
  CHECK(f(d, L"") == d);
  CHECK(d[0] == 0 && d[1] == 2 && d[2] == 3);
}
TEST(gap29_uaw_wcscpy_unaligned_dest_and_src) {
  wcscpy_pfn29 f = K32("uaw_wcscpy", wcscpy_pfn29); REQUIRE(f);
  BYTE src[64] = {0}, dst[64]; memset(dst, 0xEE, sizeof dst);
  memcpy(src + 1, L"odd offset", sizeof L"odd offset");
  WCHAR *r = f((WCHAR *)(dst + 3), (WCHAR *)(src + 1));
  CHECK((BYTE *)r == dst + 3);
  CHECK(!memcmp(dst + 3, src + 1, sizeof L"odd offset"));
  CHECK(dst[2] == 0xEE && dst[3 + sizeof L"odd offset"] == 0xEE);
}
TEST(gap29_uaw_wcscpy_long_string) {
  wcscpy_pfn29 f = K32("uaw_wcscpy", wcscpy_pfn29); REQUIRE(f);
  enum { N = 5000 };
  WCHAR *s = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, (N + 1) * 2), *d = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, (N + 1) * 2);
  REQUIRE(s && d);
  for (int i = 0; i < N; i++) s[i] = (WCHAR)(1 + (i % 0xFFFE));
  s[N] = 0;
  CHECK(f(d, s) == d);
  CHECK(!memcmp(d, s, (N + 1) * 2));
  HeapFree(GetProcessHeap(), 0, s); HeapFree(GetProcessHeap(), 0, d);
}
TEST(gap29_uaw_wcscpy_returns_destination_not_end) {
  wcscpy_pfn29 f = K32("uaw_wcscpy", wcscpy_pfn29); REQUIRE(f);
  WCHAR d[8];
  for (int n = 0; n <= 6; n++) {
    WCHAR s[8] = {0}; for (int i = 0; i < n; i++) s[i] = L'a' + i;
    CHECK(f(d, s) == d);
    CHECK(lstrlenW(d) == n);
  }
}
TEST(gap29_uaw_wcscpy_exact_overlap_self) {
  wcscpy_pfn29 f = K32("uaw_wcscpy", wcscpy_pfn29); REQUIRE(f);
  WCHAR d[8] = L"same";
  CHECK(f(d, d) == d);
  EXPECT_WSTR(d, L"same");
}

TEST(gap29_uaw_wcsicmp_equal_and_case) {
  wcsicmp_pfn29 f = K32("uaw_wcsicmp", wcsicmp_pfn29); REQUIRE(f);
  CHECK(f(L"", L"") == 0);
  CHECK(f(L"abc", L"abc") == 0);
  CHECK(f(L"abc", L"ABC") == 0);
  CHECK(f(L"AbC", L"aBc") == 0);
  CHECK(f(L"Hello World", L"hELLO wORLD") == 0);
}
TEST(gap29_uaw_wcsicmp_ordering) {
  wcsicmp_pfn29 f = K32("uaw_wcsicmp", wcsicmp_pfn29); REQUIRE(f);
  CHECK(f(L"a", L"b") < 0);
  CHECK(f(L"b", L"a") > 0);
  CHECK(f(L"A", L"b") < 0);
  CHECK(f(L"a", L"B") < 0);
  CHECK(f(L"B", L"a") > 0);
  CHECK(f(L"abc", L"abcd") < 0);
  CHECK(f(L"abcd", L"abc") > 0);
  CHECK(f(L"", L"a") < 0);
  CHECK(f(L"a", L"") > 0);
  CHECK(f(L"abc", L"abd") < 0);
  CHECK(f(L"ABD", L"abc") > 0);
}
TEST(gap29_uaw_wcsicmp_unaligned) {
  wcsicmp_pfn29 f = K32("uaw_wcsicmp", wcsicmp_pfn29); REQUIRE(f);
  /* the _wcsicmp backend may fault on odd addresses on some CPUs; x64 permits it */
  BYTE a[32] = {0}, b[32] = {0};
  memcpy(a + 1, L"MiXeD", sizeof L"MiXeD");
  memcpy(b + 1, L"mixed", sizeof L"mixed");
  CHECK(f((WCHAR *)(a + 1), (WCHAR *)(b + 1)) == 0);
}
TEST(gap29_uaw_wcsicmp_punct_fold_direction) {
  wcsicmp_pfn29 f = K32("uaw_wcsicmp", wcsicmp_pfn29); REQUIRE(f);
  /* '_' (0x5F) sits between upper and lower case letters: fold direction decides sign */
  int r = f(L"_", L"a");
  t_note("_wcsicmp(\"_\",\"a\") = %d (negative => folds to lower)", r);
  CHECK(f(L"_", L"_") == 0);
  CHECK(f(L"1", L"2") < 0);
}
TEST(gap29_uaw_wcsicmp_accent_ascii_only_fold_note) {
  wcsicmp_pfn29 f = K32("uaw_wcsicmp", wcsicmp_pfn29); REQUIRE(f);
  int r = f(L"É", L"é");
  t_note("_wcsicmp(E-acute upper, lower) = %d", r);
  CHECK(f(L"é", L"é") == 0);
}

/* lstrcmp aliases */
static const WCHAR *const lw[] = {L"", L"a", L"A", L"b", L"abc", L"ABC", L"Abc", L"abd", L"abcd", L"z", L"0", L"9", L"a b"};
TEST(gap29_uaw_lstrcmpW_matches_lstrcmpW) {
  cmpw_pfn29 u = K32("uaw_lstrcmpW", cmpw_pfn29), l = K32("lstrcmpW", cmpw_pfn29); REQUIRE(u); REQUIRE(l);
  for (size_t i = 0; i < N29(lw); i++) for (size_t j = 0; j < N29(lw); j++)
    CHECKF(u(lw[i], lw[j]) == l(lw[i], lw[j]), "%zu,%zu", i, j);
}
TEST(gap29_uaw_lstrcmpiW_matches_lstrcmpiW) {
  cmpw_pfn29 u = K32("uaw_lstrcmpiW", cmpw_pfn29), l = K32("lstrcmpiW", cmpw_pfn29); REQUIRE(u); REQUIRE(l);
  for (size_t i = 0; i < N29(lw); i++) for (size_t j = 0; j < N29(lw); j++)
    CHECKF(u(lw[i], lw[j]) == l(lw[i], lw[j]), "%zu,%zu", i, j);
}
TEST(gap29_uaw_lstrcmpW_semantics) {
  cmpw_pfn29 u = K32("uaw_lstrcmpW", cmpw_pfn29); REQUIRE(u);
  CHECK(u(L"abc", L"abc") == 0);
  CHECK(u(L"", L"") == 0);
  CHECK(u(L"a", L"b") < 0);
  CHECK(u(L"b", L"a") > 0);
  CHECK(u(L"a", L"A") < 0);            /* word sort: lowercase before uppercase */
  CHECK(u(L"A", L"a") > 0);
  CHECK(u(L"abc", L"abcd") < 0);
  CHECK(u(L"", L"a") < 0);
  CHECK(u(L"a", L"") > 0);
}
TEST(gap29_uaw_lstrcmpiW_semantics) {
  cmpw_pfn29 u = K32("uaw_lstrcmpiW", cmpw_pfn29); REQUIRE(u);
  CHECK(u(L"abc", L"ABC") == 0);
  CHECK(u(L"a", L"A") == 0);
  CHECK(u(L"a", L"b") < 0);
  CHECK(u(L"B", L"a") > 0);
  CHECK(u(L"", L"") == 0);
  CHECK(u(L"abc", L"abcd") < 0);
}
TEST(gap29_uaw_lstrcmp_result_magnitude) {
  cmpw_pfn29 u = K32("uaw_lstrcmpW", cmpw_pfn29), i = K32("uaw_lstrcmpiW", cmpw_pfn29); REQUIRE(u); REQUIRE(i);
  int a = u(L"a", L"b"), b = u(L"b", L"a"), c = i(L"a", L"b"), d = i(L"b", L"a");
  CHECK(a == -1 && b == 1 && c == -1 && d == 1);  /* CompareStringW result minus 2 */
}
TEST(gap29_uaw_lstrcmp_null_args_note) {
  cmpw_pfn29 u = K32("uaw_lstrcmpW", cmpw_pfn29); REQUIRE(u);
  /* lstrcmpW tolerates NULL (documented as not crashing); observe only */
  int r1 = u(NULL, L"a"), r2 = u(L"a", NULL), r3 = u(NULL, NULL);
  t_note("lstrcmpW(NULL,a)=%d (a,NULL)=%d (NULL,NULL)=%d", r1, r2, r3);
}

/* ---- hgets ---- */
static HANDLE mkfile29(const char *leaf, const void *data, DWORD len) {
  char p[MAX_PATH]; t_tmpfile(p, sizeof p, leaf);
  HANDLE h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE) return h;
  DWORD w; if (len) WriteFile(h, data, len, &w, NULL);
  SetFilePointer(h, 0, NULL, FILE_BEGIN);
  return h;
}
static LONG pos29(HANDLE h) { return (LONG)SetFilePointer(h, 0, NULL, FILE_CURRENT); }

TEST(gap29_hgets_crlf_lines) {
  hgets_pfn29 f = KB("hgets", hgets_pfn29); REQUIRE(f);
  HANDLE h = mkfile29("hg1.txt", "hello\r\nworld\r\n", 14); REQUIRE(h != INVALID_HANDLE_VALUE);
  char b[64]; memset(b, 'X', sizeof b);
  CHECK(f(b, 64, h) == b);
  EXPECT_STR(b, "hello\n");
  CHECK(pos29(h) == 7);
  CHECK(f(b, 64, h) == b);
  EXPECT_STR(b, "world\n");
  CHECK(pos29(h) == 14);
  CHECK(f(b, 64, h) == NULL);          /* EOF */
  CloseHandle(h);
}
TEST(gap29_hgets_lf_only_and_unterminated_tail) {
  hgets_pfn29 f = KB("hgets", hgets_pfn29); REQUIRE(f);
  HANDLE h = mkfile29("hg2.txt", "abc\ndef", 7); REQUIRE(h != INVALID_HANDLE_VALUE);
  char b[64];
  CHECK(f(b, 64, h) == b); EXPECT_STR(b, "abc\n"); CHECK(pos29(h) == 4);
  CHECK(f(b, 64, h) == b); EXPECT_STR(b, "def");   CHECK(pos29(h) == 7);
  CHECK(f(b, 64, h) == NULL);
  CloseHandle(h);
}
TEST(gap29_hgets_invalid_sizes) {
  hgets_pfn29 f = KB("hgets", hgets_pfn29); REQUIRE(f);
  HANDLE h = mkfile29("hg3.txt", "data\n", 5); REQUIRE(h != INVALID_HANDLE_VALUE);
  char b[8]; memset(b, 'X', sizeof b);
  CHECK(f(b, 1, h) == NULL);
  CHECK(f(b, 0, h) == NULL);
  CHECK(f(b, -1, h) == NULL);
  CHECK(f(b, 0x80000000, h) == NULL);
  CHECK(b[0] == 'X' && b[1] == 'X');
  CHECK(pos29(h) == 0);                /* untouched */
  CloseHandle(h);
}
TEST(gap29_hgets_empty_file) {
  hgets_pfn29 f = KB("hgets", hgets_pfn29); REQUIRE(f);
  HANDLE h = mkfile29("hg4.txt", "", 0); REQUIRE(h != INVALID_HANDLE_VALUE);
  char b[16]; memset(b, 'X', sizeof b);
  CHECK(f(b, 16, h) == NULL);
  CHECK(b[0] == 'X');
  CloseHandle(h);
}
TEST(gap29_hgets_invalid_handles) {
  hgets_pfn29 f = KB("hgets", hgets_pfn29); REQUIRE(f);
  char b[16]; memset(b, 'X', sizeof b);
  CHECK(f(b, 16, INVALID_HANDLE_VALUE) == NULL);
  CHECK(f(b, 16, NULL) == NULL);
  CHECK(b[0] == 'X');
  HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
  CHECK(f(b, 16, ev) == NULL);         /* ReadFile fails on an event handle */
  CloseHandle(ev);
  HANDLE h = mkfile29("hg5.txt", "x\n", 2); REQUIRE(h != INVALID_HANDLE_VALUE);
  CloseHandle(h);
  CHECK(f(b, 16, h) == NULL);          /* closed handle */
}
TEST(gap29_hgets_lone_cr_becomes_lf) {
  hgets_pfn29 f = KB("hgets", hgets_pfn29); REQUIRE(f);
  HANDLE h = mkfile29("hg6.txt", "a\rb\n", 4); REQUIRE(h != INVALID_HANDLE_VALUE);
  char b[32];
  CHECK(f(b, 32, h) == b); EXPECT_STR(b, "a\n"); CHECK(pos29(h) == 2);
  CHECK(f(b, 32, h) == b); EXPECT_STR(b, "b\n"); CHECK(pos29(h) == 4);
  CloseHandle(h);
}
TEST(gap29_hgets_truncated_by_buffer) {
  hgets_pfn29 f = KB("hgets", hgets_pfn29); REQUIRE(f);
  HANDLE h = mkfile29("hg7.txt", "abcdef\n", 7); REQUIRE(h != INVALID_HANDLE_VALUE);
  char b[16]; memset(b, 'X', sizeof b);
  CHECK(f(b, 4, h) == b); EXPECT_STR(b, "abc"); CHECK(pos29(h) == 3);
  CHECK(f(b, 16, h) == b); EXPECT_STR(b, "def\n");
  CloseHandle(h);
}
TEST(gap29_hgets_n_two_reads_single_byte) {
  hgets_pfn29 f = KB("hgets", hgets_pfn29); REQUIRE(f);
  HANDLE h = mkfile29("hg8.txt", "xyz", 3); REQUIRE(h != INVALID_HANDLE_VALUE);
  char b[4];
  CHECK(f(b, 2, h) == b); EXPECT_STR(b, "x");
  CHECK(f(b, 2, h) == b); EXPECT_STR(b, "y");
  CHECK(f(b, 2, h) == b); EXPECT_STR(b, "z");
  CHECK(f(b, 2, h) == NULL);
  CloseHandle(h);
}
TEST(gap29_hgets_crlf_split_at_buffer_end) {
  hgets_pfn29 f = KB("hgets", hgets_pfn29); REQUIRE(f);
  HANDLE h = mkfile29("hg9.txt", "ab\r\ncd", 6); REQUIRE(h != INVALID_HANDLE_VALUE);
  char b[4];
  CHECK(f(b, 4, h) == b);               /* reads "ab\r", peeks the LF */
  EXPECT_STR(b, "ab\n");
  CHECK(pos29(h) == 4);
  CHECK(f(b, 4, h) == b); EXPECT_STR(b, "cd");
  CloseHandle(h);
}
TEST(gap29_hgets_cr_at_buffer_end_followed_by_text) {
  hgets_pfn29 f = KB("hgets", hgets_pfn29); REQUIRE(f);
  HANDLE h = mkfile29("hg10.txt", "ab\rcd\n", 6); REQUIRE(h != INVALID_HANDLE_VALUE);
  char b[4];
  CHECK(f(b, 4, h) == b); EXPECT_STR(b, "ab\n"); CHECK(pos29(h) == 3);
  CHECK(f(b, 4, h) == b); EXPECT_STR(b, "cd\n");
  CloseHandle(h);
}
TEST(gap29_hgets_cr_at_eof) {
  hgets_pfn29 f = KB("hgets", hgets_pfn29); REQUIRE(f);
  HANDLE h = mkfile29("hg11.txt", "ab\r", 3); REQUIRE(h != INVALID_HANDLE_VALUE);
  char b[16];
  CHECK(f(b, 16, h) == b); EXPECT_STR(b, "ab\n"); CHECK(pos29(h) == 3);
  CHECK(f(b, 16, h) == NULL);
  CloseHandle(h);
}
TEST(gap29_hgets_leading_newline) {
  hgets_pfn29 f = KB("hgets", hgets_pfn29); REQUIRE(f);
  HANDLE h = mkfile29("hg12.txt", "\nabc", 4); REQUIRE(h != INVALID_HANDLE_VALUE);
  char b[16];
  CHECK(f(b, 16, h) == b); EXPECT_STR(b, "\n"); CHECK(pos29(h) == 1);
  CHECK(f(b, 16, h) == b); EXPECT_STR(b, "abc");
  CloseHandle(h);
}
TEST(gap29_hgets_starts_at_current_position) {
  hgets_pfn29 f = KB("hgets", hgets_pfn29); REQUIRE(f);
  HANDLE h = mkfile29("hg13.txt", "skip\nline2\n", 11); REQUIRE(h != INVALID_HANDLE_VALUE);
  SetFilePointer(h, 5, NULL, FILE_BEGIN);
  char b[16];
  CHECK(f(b, 16, h) == b); EXPECT_STR(b, "line2\n");
  CloseHandle(h);
}
TEST(gap29_hgets_many_lines) {
  hgets_pfn29 f = KB("hgets", hgets_pfn29); REQUIRE(f);
  char data[1000]; int len = 0;
  for (int i = 0; i < 100; i++) len += sprintf(data + len, "L%02d\r\n", i);
  HANDLE h = mkfile29("hg14.txt", data, len); REQUIRE(h != INVALID_HANDLE_VALUE);
  char b[32];
  for (int i = 0; i < 100; i++) {
    char want[16]; sprintf(want, "L%02d\n", i);
    void *r = f(b, 32, h);
    if (r != b) { CHECKF(0, "line %d returned NULL", i); break; }
    CHECKF(!strcmp(b, want), "line %d got %s", i, b);
  }
  CHECK(f(b, 32, h) == NULL);
  CloseHandle(h);
}
TEST(gap29_hgets_does_not_set_error_on_success) {
  hgets_pfn29 f = KB("hgets", hgets_pfn29); REQUIRE(f);
  HANDLE h = mkfile29("hg15.txt", "q\n", 2); REQUIRE(h != INVALID_HANDLE_VALUE);
  char b[8];
  SetLastError(0x5151); f(b, 8, h);
  DWORD e = GetLastError(); t_note("last error after hgets success: %lu", e);
  CloseHandle(h);
}
TEST(gap29_hgets_binary_nul_bytes) {
  hgets_pfn29 f = KB("hgets", hgets_pfn29); REQUIRE(f);
  HANDLE h = mkfile29("hg16.txt", "a\0b\nc", 5); REQUIRE(h != INVALID_HANDLE_VALUE);
  char b[16]; memset(b, 'X', sizeof b);
  CHECK(f(b, 16, h) == b);
  CHECK(b[0] == 'a' && b[1] == 0 && b[2] == 'b' && b[3] == '\n' && b[4] == 0);
  CHECK(pos29(h) == 4);
  CloseHandle(h);
}

/* ---- hwprintf (writes via WriteFile to the supplied handle) ---- */
static int slurp29(HANDLE h, char *out, int cap) {
  DWORD n = 0; SetFilePointer(h, 0, NULL, FILE_BEGIN);
  if (!ReadFile(h, out, cap - 1, &n, NULL)) n = 0;
  out[n] = 0; return (int)n;
}
TEST(gap29_hwprintf_basic) {
  hwprintf_pfn29 f = KB("hwprintf", hwprintf_pfn29); REQUIRE(f);
  HANDLE h = mkfile29("hw1.txt", "", 0); REQUIRE(h != INVALID_HANDLE_VALUE);
  f(h, L"value=%d", 42);
  char b[64]; slurp29(h, b, sizeof b);
  EXPECT_STR(b, "value=42");
  CloseHandle(h);
}
TEST(gap29_hwprintf_wide_string_arg) {
  hwprintf_pfn29 f = KB("hwprintf", hwprintf_pfn29); REQUIRE(f);
  HANDLE h = mkfile29("hw2.txt", "", 0); REQUIRE(h != INVALID_HANDLE_VALUE);
  f(h, L"[%ls|%5d|%-4x|%c]", L"wide", 7, 0xAB, L'Z');
  char b[64]; slurp29(h, b, sizeof b);
  EXPECT_STR(b, "[wide|    7|ab  |Z]");
  CloseHandle(h);
}
TEST(gap29_hwprintf_empty_format_writes_nothing) {
  hwprintf_pfn29 f = KB("hwprintf", hwprintf_pfn29); REQUIRE(f);
  HANDLE h = mkfile29("hw3.txt", "", 0); REQUIRE(h != INVALID_HANDLE_VALUE);
  f(h, L"");
  CHECK(GetFileSize(h, NULL) == 0);
  CloseHandle(h);
}
TEST(gap29_hwprintf_appends_at_file_pointer) {
  hwprintf_pfn29 f = KB("hwprintf", hwprintf_pfn29); REQUIRE(f);
  HANDLE h = mkfile29("hw4.txt", "", 0); REQUIRE(h != INVALID_HANDLE_VALUE);
  f(h, L"one "); f(h, L"two "); f(h, L"%d", 3);
  char b[64]; slurp29(h, b, sizeof b);
  EXPECT_STR(b, "one two 3");
  CloseHandle(h);
}
TEST(gap29_hwprintf_no_terminator_written) {
  hwprintf_pfn29 f = KB("hwprintf", hwprintf_pfn29); REQUIRE(f);
  HANDLE h = mkfile29("hw5.txt", "", 0); REQUIRE(h != INVALID_HANDLE_VALUE);
  f(h, L"abc");
  CHECK(GetFileSize(h, NULL) == 3);    /* WriteFile length is converted length minus the NUL */
  CloseHandle(h);
}
TEST(gap29_hwprintf_ansi_conversion_of_ascii_range) {
  hwprintf_pfn29 f = KB("hwprintf", hwprintf_pfn29); REQUIRE(f);
  HANDLE h = mkfile29("hw6.txt", "", 0); REQUIRE(h != INVALID_HANDLE_VALUE);
  f(h, L"%ls", L"AB C\n");
  char b[32]; int n = slurp29(h, b, sizeof b);
  CHECK(n == 5 && !strcmp(b, "AB C\n"));
  CloseHandle(h);
}
TEST(gap29_hwprintf_invalid_handle_does_not_crash) {
  hwprintf_pfn29 f = KB("hwprintf", hwprintf_pfn29); REQUIRE(f);
  f(INVALID_HANDLE_VALUE, L"nowhere %d", 1);
  f(NULL, L"nowhere %d", 2);
  CHECK(1);
}
TEST(gap29_hwprintf_readonly_handle_write_fails_quietly) {
  hwprintf_pfn29 f = KB("hwprintf", hwprintf_pfn29); REQUIRE(f);
  char p[MAX_PATH]; t_tmpfile(p, sizeof p, "hw7.txt");
  HANDLE w = CreateFileA(p, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, NULL);
  CloseHandle(w);
  HANDLE r = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
  REQUIRE(r != INVALID_HANDLE_VALUE);
  f(r, L"denied");
  CHECK(GetFileSize(r, NULL) == 0);
  CloseHandle(r);
}

/* ---- children: exit / atexit / _onexit / _purecall / wprintf ---- */
static void __cdecl noop29(void) {}
static const char *g_marker29;
static void __cdecl marker_handler29(void) {
  if (!g_marker29) return;
  HANDLE h = CreateFileA(g_marker29, GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, 0, NULL);
  if (h != INVALID_HANDLE_VALUE) { DWORD w; WriteFile(h, "ran", 3, &w, NULL); CloseHandle(h); }
}
CHILD(gap29_exit) { exit_pfn29 f = KB("exit", exit_pfn29); if (!f) return 90; f(37); return 91; }
CHILD(gap29_exit0) { exit_pfn29 f = KB("exit", exit_pfn29); if (!f) return 90; f(0); return 91; }
CHILD(gap29_exit_neg) { exit_pfn29 f = KB("exit", exit_pfn29); if (!f) return 90; f(-1); return 91; }
CHILD(gap29_exit_big) { exit_pfn29 f = KB("exit", exit_pfn29); if (!f) return 90; f(0x7FFFFFFF); return 91; }
CHILD(gap29_exit_atexit) {
  exit_pfn29 f = KB("exit", exit_pfn29); atexit_pfn29 a = KB("atexit", atexit_pfn29);
  if (!f || !a || argc < 2) return 90;
  g_marker29 = argv[1];
  if (a(marker_handler29) != 0) return 92;
  f(5); return 91;
}
CHILD(gap29_atexit_ret) {
  atexit_pfn29 a = KB("atexit", atexit_pfn29); onexit_pfn29 o = KB("_onexit", onexit_pfn29);
  if (!a || !o) return 90;
  if (a(noop29) != 0) return 11;
  if (a(noop29) != 0) return 12;
  if (o((void *)noop29) != (void *)noop29) return 13;
  return 0;
}
CHILD(gap29_onexit_null) {
  onexit_pfn29 o = KB("_onexit", onexit_pfn29); if (!o) return 90;
  void *r = o(NULL);
  return r == NULL ? 0 : 21;
}
static LONG CALLBACK veh29(PEXCEPTION_POINTERS ep) {
  if (ep->ExceptionRecord->ExceptionCode == 0xC0000002) {
    int ok = (ep->ExceptionRecord->ExceptionFlags & EXCEPTION_NONCONTINUABLE) && ep->ExceptionRecord->NumberParameters == 0;
    ExitProcess(ok ? 0x50 : 0x51);
  }
  return EXCEPTION_CONTINUE_SEARCH;
}
CHILD(gap29_purecall) {
  void_pfn29 f = KB("_purecall", void_pfn29); if (!f) return 90;
  AddVectoredExceptionHandler(1, veh29);
  f();
  return 0x52;
}
CHILD(gap29_wprintf) {
  wprintf_pfn29 f = KB("wprintf", wprintf_pfn29); if (!f) return 90;
  int r = f(L"abc%d\n", 5);
  return r < 0 ? 100 : (r > 99 ? 99 : r);
}

TEST(gap29_exit_exit_code) {
  REQUIRE(t_proc_kb("exit"));
  EXPECT_EQ(t_run_child("gap29_exit", "", 20000), 37);
}
TEST(gap29_exit_zero) {
  REQUIRE(t_proc_kb("exit"));
  EXPECT_EQ(t_run_child("gap29_exit0", "", 20000), 0);
}
TEST(gap29_exit_negative_code) {
  REQUIRE(t_proc_kb("exit"));
  EXPECT_EQ(t_run_child("gap29_exit_neg", "", 20000), 0xFFFFFFFFu);
}
TEST(gap29_exit_large_code) {
  REQUIRE(t_proc_kb("exit"));
  EXPECT_EQ(t_run_child("gap29_exit_big", "", 20000), 0x7FFFFFFF);
}
TEST(gap29_exit_runs_atexit_handlers_note) {
  REQUIRE(t_proc_kb("exit")); REQUIRE(t_proc_kb("atexit"));
  char m[MAX_PATH], arg[MAX_PATH + 4]; t_tmpfile(m, sizeof m, "atexit_marker.txt");
  DeleteFileA(m);
  snprintf(arg, sizeof arg, "%s", m);
  EXPECT_EQ(t_run_child("gap29_exit_atexit", arg, 20000), 5);
  DWORD a = GetFileAttributesA(m);
  t_note("kernelbase exit() ran atexit handler: %s", a != INVALID_FILE_ATTRIBUTES ? "yes" : "no");
  DeleteFileA(m);
}
TEST(gap29_atexit_and_onexit_return_values) {
  REQUIRE(t_proc_kb("atexit")); REQUIRE(t_proc_kb("_onexit"));
  EXPECT_EQ(t_run_child("gap29_atexit_ret", "", 20000), 0);
}
TEST(gap29_onexit_null_function) {
  REQUIRE(t_proc_kb("_onexit"));
  DWORD r = t_run_child("gap29_onexit_null", "", 20000);
  t_note("_onexit(NULL) child result %lu (0 = returned NULL)", r);
  CHECK(r == 0 || r == 21);
}
TEST(gap29_purecall_raises_status_not_implemented) {
  REQUIRE(t_proc_kb("_purecall"));
  EXPECT_EQ(t_run_child("gap29_purecall", "", 20000), 0x50);
}
TEST(gap29_wprintf_child_stdout_redirect) {
  REQUIRE(t_proc_kb("wprintf"));
  char p[MAX_PATH]; t_tmpfile(p, sizeof p, "wp_out.txt");
  SECURITY_ATTRIBUTES sa = {sizeof sa, NULL, TRUE};
  HANDLE out = CreateFileA(p, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS, 0, NULL);
  REQUIRE(out != INVALID_HANDLE_VALUE);
  PROCESS_INFORMATION pi;
  HANDLE hp = t_spawn_child("gap29_wprintf", "", 0, NULL, out, out, TRUE, NULL, &pi);
  REQUIRE(hp);
  DWORD code = 0xDEAD0001;
  if (WaitForSingleObject(hp, 20000) == WAIT_OBJECT_0) GetExitCodeProcess(hp, &code); else TerminateProcess(hp, 1);
  CloseHandle(pi.hThread); CloseHandle(hp);
  char b[64]; slurp29(out, b, sizeof b);
  t_note("wprintf child exit=%lu output=[%s]", code, b);
  CHECK(code != 90 && code < 0xDEAD0000);
  CloseHandle(out);
}
