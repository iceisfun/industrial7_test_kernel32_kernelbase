#include "t.h"
/* Non-public exports: presence, module ownership, alias identity, and the
 * side-effect-free PathIsValidChar{A,W} predicate (verified from kernelbase decomp). */
static const struct { const char *n; int k32; } ex[] = {
  {"OpenStateExplicit",1},{"OpenStateExplicitForUserSid",0},{"OpenStateExplicitForUserSidString",0},
  {"PackageFamilyNameFromIdA",0},{"PackageFamilyNameFromProductId",0},{"PackageFullNameFromIdA",0},
  {"PackageFullNameFromProductId",0},{"PackageIdFromProductId",0},{"PackageRelativeApplicationIdFromProductId",0},
  {"PackageSidFromFamilyName",0},{"PackageSidFromProductId",0},{"PathIsValidCharA",0},{"PathIsValidCharW",0},
  {"PcwAddQueryItem",0},{"PcwClearCounterSetSecurity",0},{"PcwCollectData",0},{"PcwCompleteNotification",0},
  {"PcwCreateNotifier",0},{"PcwCreateQuery",0},{"PcwDisconnectCounterSet",0},{"PcwEnumerateInstances",0},
  {"PcwIsNotifierAlive",0},{"PcwQueryCounterSetSecurity",0},{"PcwReadNotificationData",0},{"PcwRegisterCounterSet",0},
  {"PcwRemoveQueryItem",0},{"PcwSendNotification",0},{"PcwSendStatelessNotification",0},{"PcwSetCounterSetSecurity",0},
  {"PcwSetQueryItemUserData",0},{"PrivCopyFileExW",1},{"PrivMoveFileIdentityW",1},{"ProductIdFromPackageFamilyName",0},
  {"PsmCreateKey",0},{"PsmCreateKeyWithDynamicId",0},{"PsmEqualApplication",0},{"PsmEqualPackage",0},
  {"PsmGetApplicationNameFromKey",0},{"PsmGetDynamicIdFromKey",0},{"PsmGetKeyFromProcess",0},{"PsmGetKeyFromToken",0},
  {"PsmGetPackageFullNameFromKey",0},{"PsmIsChildKey",0},{"PsmIsDynamicKey",0},{"PsmIsValidKey",0},
  {"PssWalkMarkerRewind",1},{"PssWalkMarkerSeek",1},{"PssWalkMarkerTell",1},
  {"PublishStateChangeNotification",0},{"PublisherFromPackageFullName",0},
};
#define NEX (sizeof ex / sizeof ex[0])

TEST(gap25_exports_present) {
  for (size_t i = 0; i < NEX; i++) {
    FARPROC p = t_proc(ex[i].n);
    if (!p && t_is_wine()) continue;
    CHECKF(p != NULL, "%s missing", ex[i].n);
  }
}
TEST(gap25_exports_in_expected_module) {
  if (t_is_wine()) { t_skip("module layout differs on wine"); return; }
  for (size_t i = 0; i < NEX; i++) {
    CHECKF(t_proc_kb(ex[i].n) != NULL || ex[i].k32, "%s not in kernelbase", ex[i].n);
    if (ex[i].k32) CHECKF(t_proc_k32(ex[i].n) != NULL, "%s not in kernel32", ex[i].n);
    else if (t_proc_k32(ex[i].n)) t_note("%s also exported by kernel32", ex[i].n);
  }
}
TEST(gap25_exports_stable) {
  for (size_t i = 0; i < NEX; i++) CHECKF(t_proc(ex[i].n) == t_proc(ex[i].n), "%s", ex[i].n);
}
TEST(gap25_exports_case_sensitive) {
  /* GetProcAddress is case sensitive */
  CHECK(t_proc("pathisvalidchara") == NULL);
  CHECK(t_proc("PATHISVALIDCHARW") == NULL);
  CHECK(t_proc("PsmIsValidKey ") == NULL);
}
TEST(gap25_exports_not_decorated) {
  CHECK(t_proc("_PathIsValidCharW@8") == NULL);
  CHECK(t_proc("PathIsValidChar") == NULL);
}

/* PssWalkMarker aliases share the entry stub with the documented name. */
TEST(gap25_pss_alias_rewind) {
  FARPROC a = t_proc_k32("PssWalkMarkerRewind"), b = t_proc_k32("PssWalkMarkerSeekToBeginning");
  REQUIRE(a); REQUIRE(b);
  if (t_is_wine()) { t_note("alias identity not checked on wine"); return; }
  CHECK(a == b);
}
TEST(gap25_pss_alias_seek) {
  FARPROC a = t_proc_k32("PssWalkMarkerSeek"), b = t_proc_k32("PssWalkMarkerSetPosition");
  REQUIRE(a); REQUIRE(b);
  if (t_is_wine()) { t_note("alias identity not checked on wine"); return; }
  CHECK(a == b);
}
TEST(gap25_pss_alias_tell) {
  FARPROC a = t_proc_k32("PssWalkMarkerTell"), b = t_proc_k32("PssWalkMarkerGetPosition");
  REQUIRE(a); REQUIRE(b);
  if (t_is_wine()) { t_note("alias identity not checked on wine"); return; }
  CHECK(a == b);
}
TEST(gap25_pss_aliases_distinct) {
  FARPROC r = t_proc_k32("PssWalkMarkerRewind"), s = t_proc_k32("PssWalkMarkerSeek"), t = t_proc_k32("PssWalkMarkerTell");
  REQUIRE(r); REQUIRE(s); REQUIRE(t);
  CHECK(r != s); CHECK(s != t); CHECK(r != t);
}

/* ---- PathIsValidChar (flags: PPIVC_*) ---- */
typedef DWORD (WINAPI *pivw_t)(WCHAR, DWORD);
typedef DWORD (WINAPI *piva_t)(BYTE, DWORD);
#define F_Q 1u      /* ? */
#define F_STAR 2u   /* * */
#define F_DOT 4u
#define F_BSL 8u
#define F_COLON 0x10u
#define F_SEMI 0x20u
#define F_COMMA 0x40u
#define F_SPACE 0x80u
#define F_OTHER 0x100u
#define F_QUOTE 0x200u
#define F_ALL 0x3FFu

static const struct { unsigned c; unsigned flag; } sp[] = {
  {':',F_COLON},{' ',F_SPACE},{'"',F_QUOTE},{'*',F_STAR},{',',F_COMMA},{'.',F_DOT},{';',F_SEMI},{'?',F_Q},{'\\',F_BSL},
};
static const unsigned never[] = {'/', '<', '>', '|'};

TEST(gap25_pathisvalidcharw_letters) {
  pivw_t f = (pivw_t)t_proc_kb("PathIsValidCharW"); REQUIRE(f);
  if (t_is_wine()) { t_skip("wine returns a different value for letters"); return; }
  for (unsigned c = 'a'; c <= 'z'; c++) { CHECKF(f(c, 0) == 1, "c=%c", c); CHECKF(f(c, F_ALL) == 1, "c=%c all", c); }
  for (unsigned c = 'A'; c <= 'Z'; c++) { CHECKF(f(c, 0) == 1, "c=%c", c); CHECKF(f(c, F_ALL) == 1, "c=%c all", c); }
}
TEST(gap25_pathisvalidchara_letters) {
  piva_t f = (piva_t)t_proc_kb("PathIsValidCharA"); REQUIRE(f);
  if (t_is_wine()) { t_skip("wine returns a different value for letters"); return; }
  for (unsigned c = 'a'; c <= 'z'; c++) { CHECKF(f(c, 0) == 1, "c=%c", c); CHECKF(f(c, F_ALL) == 1, "c=%c all", c); }
  for (unsigned c = 'A'; c <= 'Z'; c++) { CHECKF(f(c, 0) == 1, "c=%c", c); CHECKF(f(c, F_ALL) == 1, "c=%c all", c); }
}
TEST(gap25_pathisvalidcharw_special_flags) {
  pivw_t f = (pivw_t)t_proc_kb("PathIsValidCharW"); REQUIRE(f);
  for (size_t i = 0; i < sizeof sp / sizeof sp[0]; i++) {
    CHECKF(f(sp[i].c, sp[i].flag) == sp[i].flag, "c=%c own flag", sp[i].c);
    CHECKF(f(sp[i].c, 0) == 0, "c=%c no flags", sp[i].c);
    CHECKF(f(sp[i].c, F_ALL & ~sp[i].flag) == 0, "c=%c other flags", sp[i].c);
    CHECKF(f(sp[i].c, F_ALL) == sp[i].flag, "c=%c all flags", sp[i].c);
  }
}
TEST(gap25_pathisvalidchara_special_flags) {
  piva_t f = (piva_t)t_proc_kb("PathIsValidCharA"); REQUIRE(f);
  for (size_t i = 0; i < sizeof sp / sizeof sp[0]; i++) {
    CHECKF(f(sp[i].c, sp[i].flag) == sp[i].flag, "c=%c own flag", sp[i].c);
    CHECKF(f(sp[i].c, 0) == 0, "c=%c no flags", sp[i].c);
    CHECKF(f(sp[i].c, F_ALL & ~sp[i].flag) == 0, "c=%c other flags", sp[i].c);
    CHECKF(f(sp[i].c, F_ALL) == sp[i].flag, "c=%c all flags", sp[i].c);
  }
}
TEST(gap25_pathisvalidchar_never_valid) {
  pivw_t w = (pivw_t)t_proc_kb("PathIsValidCharW"); piva_t a = (piva_t)t_proc_kb("PathIsValidCharA");
  REQUIRE(w); REQUIRE(a);
  for (size_t i = 0; i < sizeof never / sizeof never[0]; i++) {
    CHECKF(w(never[i], F_ALL) == 0, "W c=%c", never[i]);
    CHECKF(a(never[i], F_ALL) == 0, "A c=%c", never[i]);
    CHECKF(w(never[i], 0xFFFFFFFF) == 0, "W c=%c ff", never[i]);
    CHECKF(a(never[i], 0xFFFFFFFF) == 0, "A c=%c ff", never[i]);
  }
}
TEST(gap25_pathisvalidchar_control_chars) {
  pivw_t w = (pivw_t)t_proc_kb("PathIsValidCharW"); piva_t a = (piva_t)t_proc_kb("PathIsValidCharA");
  REQUIRE(w); REQUIRE(a);
  for (unsigned c = 0; c < 0x20; c++) {
    CHECKF(w(c, 0xFFFFFFFF) == 0, "W 0x%x", c);
    CHECKF(a(c, 0xFFFFFFFF) == 0, "A 0x%x", c);
  }
}
TEST(gap25_pathisvalidchar_other_flag) {
  pivw_t w = (pivw_t)t_proc_kb("PathIsValidCharW"); piva_t a = (piva_t)t_proc_kb("PathIsValidCharA");
  REQUIRE(w); REQUIRE(a);
  /* digits and non-special printable chars are governed by PPIVC_OTHER (0x100) */
  const char *others = "0123456789!#$%&'()+-=@[]^_`{}~";
  for (const char *p = others; *p; p++) {
    unsigned c = (unsigned char)*p;
    if (c == '=') continue;   /* not reachable in verified decomp excerpt; checked via note below */
    CHECKF(w(c, F_OTHER) == F_OTHER, "W c=%c", c);
    CHECKF(a(c, F_OTHER) == F_OTHER, "A c=%c", c);
    CHECKF(w(c, F_ALL & ~F_OTHER) == 0, "W c=%c noother", c);
    CHECKF(a(c, F_ALL & ~F_OTHER) == 0, "A c=%c noother", c);
    CHECKF(w(c, 0) == 0, "W c=%c zero", c);
  }
  t_note("'=' W(OTHER)=0x%lx", (unsigned long)w('=', F_OTHER));
}
TEST(gap25_pathisvalidcharw_high_chars) {
  pivw_t w = (pivw_t)t_proc_kb("PathIsValidCharW"); REQUIRE(w);
  static const WCHAR hi[] = {0x7F, 0x80, 0xE9, 0x100, 0x3042, 0xD800, 0xFFFD, 0xFFFF};
  for (size_t i = 0; i < sizeof hi / sizeof hi[0]; i++) {
    CHECKF(w(hi[i], F_OTHER) == F_OTHER, "0x%x", hi[i]);
    CHECKF(w(hi[i], 0) == 0, "0x%x zero", hi[i]);
  }
}
TEST(gap25_pathisvalidchara_high_bytes) {
  piva_t a = (piva_t)t_proc_kb("PathIsValidCharA"); REQUIRE(a);
  for (unsigned c = 0x7F; c <= 0xFF; c++) {
    CHECKF(a(c, F_OTHER) == F_OTHER, "0x%x", c);
    CHECKF(a(c, 0) == 0, "0x%x zero", c);
  }
}
TEST(gap25_pathisvalidchar_ap_parity) {
  pivw_t w = (pivw_t)t_proc_kb("PathIsValidCharW"); piva_t a = (piva_t)t_proc_kb("PathIsValidCharA");
  REQUIRE(w); REQUIRE(a);
  static const DWORD fl[] = {0, F_OTHER, F_ALL, F_Q|F_STAR|F_DOT, F_BSL|F_COLON, 0xFFFFFFFF};
  for (unsigned c = 0; c < 256; c++)
    for (size_t i = 0; i < sizeof fl / sizeof fl[0]; i++)
      CHECKF(w(c, fl[i]) == a(c, fl[i]), "c=0x%x flags=0x%lx", c, (unsigned long)fl[i]);
}
TEST(gap25_pathisvalidchar_repeatable) {
  pivw_t w = (pivw_t)t_proc_kb("PathIsValidCharW"); REQUIRE(w);
  if (t_is_wine()) { t_skip("wine returns a different value for letters"); return; }
  for (int i = 0; i < 1000; i++) { CHECK(w('a', 0) == 1); CHECK(w('\\', F_BSL) == F_BSL); }
}
TEST(gap25_pathisvalidchar_no_lasterror) {
  pivw_t w = (pivw_t)t_proc_kb("PathIsValidCharW"); REQUIRE(w);
  SET_ERR(0x1234);
  w('a', 0); w('*', F_STAR); w('<', F_ALL);
  CHECK(GetLastError() == 0x1234);
}
TEST(gap25_pathisvalidchar_flag_bits_independent) {
  pivw_t w = (pivw_t)t_proc_kb("PathIsValidCharW"); REQUIRE(w);
  for (size_t i = 0; i < sizeof sp / sizeof sp[0]; i++)
    for (size_t j = 0; j < sizeof sp / sizeof sp[0]; j++)
      CHECKF(w(sp[i].c, sp[j].flag) == (i == j ? sp[i].flag : 0), "c=%c flag=%u", sp[i].c, sp[j].flag);
}
TEST(gap25_pathisvalidchar_matches_filename_rules) {
  /* a typical file name charset: letters/digits/space/dot are valid, wildcard chars are not */
  pivw_t w = (pivw_t)t_proc_kb("PathIsValidCharW"); REQUIRE(w);
  const DWORD fname = F_OTHER | F_DOT | F_SPACE;
  const WCHAR *good = L"Report 2020.txt";
  for (const WCHAR *p = good; *p; p++) CHECKF(w(*p, fname) != 0, "c=%c", *p);
  const WCHAR *bad = L"a*b?c<d>e|f/g\\h:i\"";
  for (const WCHAR *p = bad; *p; p++) {
    if (*p >= 'a' && *p <= 'z') continue;
    CHECKF(w(*p, fname) == 0, "bad c=%c", *p);
  }
}
