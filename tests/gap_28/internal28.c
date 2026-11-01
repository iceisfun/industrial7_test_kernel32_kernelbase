#include "t.h"
/* Non-public / internal exports: presence, module ownership (from the Win10 export
 * tables) and the few calls whose reference analysis shows trivial, side-effect-free behaviour. */
static const struct { const char *n; int k32, kb; } ex[] = {
  {"TermsrvSetKeySecurity",1,0},
  {"TermsrvSetValueKey",1,0},
  {"TermsrvSyncUserIniFileExt",1,0},
  {"TestClose",0,1},
  {"TestControlReporting",0,1},
  {"TestCreate",0,1},
  {"TestOpen",0,1},
  {"TestQueryData",0,1},
  {"TestReport",0,1},
  {"TestUnlockData",0,1},
  {"UTRegister",1,0},
  {"UTUnRegister",1,0},
  {"UnregisterConsoleIME",1,0},
  {"UnregisterStateChangeNotification",0,1},
  {"UnregisterStateLock",0,1},
  {"UpdateCalendarDayOfWeek",1,0},
  {"UpdatePackageStatus",0,1},
  {"UpdatePackageStatusForUser",0,1},
  {"UpdatePackageStatusForUserSid",0,1},
  {"VDMConsoleOperation",1,0},
  {"VDMOperationStarted",1,0},
  {"VerifyPackageIdA",0,1},
  {"VerifyPackagePublisher",0,1},
  {"VerifyPackagePublisherA",0,1},
  {"WTSGetServiceSessionId",0,1},
  {"WTSIsServerContainer",0,1},
  {"WerSetMaxProcessHoldMilliseconds",0,1},
  {"WerUnregisterCustomMetadata",1,1},
  {"WerpGetDebugger",1,0},
  {"WerpInitiateRemoteRecovery",1,0},
  {"WerpLaunchAeDebug",1,0},
  {"WriteConsoleInputVDMA",1,0},
  {"WriteConsoleInputVDMW",1,0},
  {"WriteStateAtomValue",0,1},
  {"WriteStateContainerValue",0,1},
  {"_AddMUIStringToCache",0,1},
  {"_GetMUIStringFromCache",0,1},
  {"_OpenMuiStringCache",0,1},
  {"__C_specific_handler",1,1},
  {"__chkstk",1,1},
  {"__dllonexit3",0,1},
  {"__misaligned_access",1,1},
  {"__wgetmainargs",0,1},
  {"_amsg_exit",0,1},
  {"_c_exit",0,1},
  {"_cexit",0,1},
  {"_exit",0,1},
  {"_initterm",0,1},
  {"_initterm_e",0,1},
  {"_invalid_parameter",0,1}
};
#define NEX (sizeof ex / sizeof ex[0])

TEST(gap28_exports_present) {
  for (size_t i = 0; i < NEX; i++) {
    FARPROC p = t_proc(ex[i].n);
    if (!p && t_is_wine()) continue;
    CHECKF(p != NULL, "%s missing", ex[i].n);
  }
}
TEST(gap28_exports_in_kernel32_as_listed) {
  if (t_is_wine()) { t_skip("module layout differs on wine"); return; }
  for (size_t i = 0; i < NEX; i++)
    if (ex[i].k32) CHECKF(t_proc_k32(ex[i].n) != NULL, "%s not in kernel32", ex[i].n);
}
TEST(gap28_exports_in_kernelbase_as_listed) {
  if (t_is_wine()) { t_skip("module layout differs on wine"); return; }
  for (size_t i = 0; i < NEX; i++)
    if (ex[i].kb) CHECKF(t_proc_kb(ex[i].n) != NULL, "%s not in kernelbase", ex[i].n);
}
TEST(gap28_exports_kernelbase_only_not_in_kernel32) {
  if (t_is_wine()) { t_skip("module layout differs on wine"); return; }
  for (size_t i = 0; i < NEX; i++)
    if (!ex[i].k32 && ex[i].kb && t_proc_k32(ex[i].n)) t_note("%s also reachable via kernel32", ex[i].n);
}
TEST(gap28_exports_stable) {
  for (size_t i = 0; i < NEX; i++) CHECKF(t_proc(ex[i].n) == t_proc(ex[i].n), "%s", ex[i].n);
}
TEST(gap28_exports_case_sensitive) {
  CHECK(t_proc("updatecalendardayofweek") == NULL);
  CHECK(t_proc("WTSISSERVERCONTAINER") == NULL);
  CHECK(t_proc("_INITTERM") == NULL);
}
TEST(gap28_exports_not_decorated) {
  CHECK(t_proc("_initterm@8") == NULL);
  CHECK(t_proc("UpdateCalendarDayOfWeek@8") == NULL);
  CHECK(t_proc("__initterm") == NULL);
}
/* __C_specific_handler, __chkstk and __misaligned_access are export forwarders
 * ("NTDLL.<name>") in both kernel32 and kernelbase on Win10, so GetProcAddress
 * resolves them into ntdll's image. */
static int gap28_is_ntdll_fwd(const char *n) {
  return !strcmp(n, "__C_specific_handler") || !strcmp(n, "__chkstk") || !strcmp(n, "__misaligned_access");
}
static int gap28_in_image(FARPROC p, HMODULE h, size_t span) {
  return (char *)p > (char *)h && (char *)p < (char *)h + span;
}
TEST(gap28_exports_inside_module_image) {
  if (t_is_wine()) { t_skip("wine builtin layout"); return; }
  HMODULE hk = t_kernelbase(), h32 = t_kernel32();
  for (size_t i = 0; i < NEX; i++) {
    if (!ex[i].kb) continue;
    FARPROC p = t_proc_kb(ex[i].n);
    if (!p) continue;
    if (gap28_is_ntdll_fwd(ex[i].n)) {
      CHECKF(p == t_proc_nt(ex[i].n), "%s (forwarder) does not resolve to the ntdll export", ex[i].n);
      continue;
    }
    CHECKF(gap28_in_image(p, hk, 0x1000000), "%s outside kernelbase image", ex[i].n);
  }
  for (size_t i = 0; i < NEX; i++) {
    if (!ex[i].k32) continue;
    FARPROC p = t_proc_k32(ex[i].n);
    if (!p) continue;
    if (gap28_is_ntdll_fwd(ex[i].n)) {
      CHECKF(p == t_proc_nt(ex[i].n), "%s (forwarder) does not resolve to the ntdll export", ex[i].n);
      continue;
    }
    int in32 = gap28_in_image(p, h32, 0x400000);
    int inkb = gap28_in_image(p, hk, 0x1000000);
    CHECKF(in32 || inkb, "%s in neither image", ex[i].n);
  }
}

/* ---- UpdateCalendarDayOfWeek (kernel32): NULL and unknown calendar id paths ---- */
UDECL(BOOL, UpdateCalendarDayOfWeek, (void *));
TEST(gap28_updatecal_null) {
  REQUIRE(UN(UpdateCalendarDayOfWeek));
  SET_ERR(0);
  int r = UN(UpdateCalendarDayOfWeek)(NULL);
  EXPECT_EQ(r, 0);
  EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap28_updatecal_unknown_calid) {
  REQUIRE(UN(UpdateCalendarDayOfWeek));
  /* CalId values 9..12 other than the handled ones fall to the default error. CalId 0 too. */
  static const UINT ids[] = { 0, 0x18, 0x19, 0xff, 0x1000, 0xffffffffu };
  for (size_t i = 0; i < sizeof ids / sizeof ids[0]; i++) {
    UINT buf[16] = { ids[i], 1, 2020, 1, 1 };
    SET_ERR(0);
    int r = UN(UpdateCalendarDayOfWeek)(buf);
    DWORD e = GetLastError();   /* capture first: t_is_wine() does a failing GetProcAddress (sets 127) */
    if (t_is_wine()) { t_note("calid 0x%x -> %d", ids[i], r); continue; }
    EXPECT_EQ(r, 0);
    CHECKF(e == ERROR_INVALID_PARAMETER, "calid 0x%x GetLastError got %lu want 87", ids[i], e);
  }
}
TEST(gap28_updatecal_gregorian_observed) {
  REQUIRE(UN(UpdateCalendarDayOfWeek));
  UINT buf[16] = { 1, 1, 2020, 2, 29 };  /* CAL_GREGORIAN, 2020-02-29 (Saturday) */
  SET_ERR(0);
  int r = UN(UpdateCalendarDayOfWeek)(buf);
  t_note("gregorian 2020-02-29: ret=%d dow=%u (expect 6) err=%lu", r, buf[5], GetLastError());
}

/* ---- WTSIsServerContainer / WTSGetServiceSessionId consistency ---- */
TEST(gap28_wts_service_session_consistent) {
  REQUIRE(t_proc("WTSGetServiceSessionId")); REQUIRE(t_proc_nt("RtlGetCurrentServiceSessionId"));
  typedef ULONG (WINAPI *fn_t)(void);
  fn_t a = (fn_t)t_proc("WTSGetServiceSessionId");
  fn_t b = (fn_t)t_proc_nt("RtlGetCurrentServiceSessionId");
  EXPECT_EQ(a(), b());
  EXPECT_EQ(a(), a());
}
TEST(gap28_wts_isservercontainer_matches_session) {
  typedef ULONG (WINAPI *sfn_t)(void);
  typedef BOOL (WINAPI *bfn_t)(void);
  FARPROC s = t_proc("WTSGetServiceSessionId"), c = t_proc("WTSIsServerContainer");
  REQUIRE(s); REQUIRE(c);
  BYTE r = (BYTE)((bfn_t)c)();
  BYTE exp = ((sfn_t)s)() != 0;
  EXPECT_EQ(r, exp);
  /* A normal (non-container) desktop host reports session id 0 */
  t_note("service session=%lu isContainer=%u", ((sfn_t)s)(), r);
}

/* ---- VerifyPackageIdA(NULL) -> ERROR_INVALID_PARAMETER returned directly ---- */
UDECL(LONG, VerifyPackageIdA, (void *));
TEST(gap28_verifypackageida_null) {
  REQUIRE(UN(VerifyPackageIdA));
  SET_ERR(0);
  LONG r = UN(VerifyPackageIdA)(NULL);
  EXPECT_EQ(r, ERROR_INVALID_PARAMETER);
}

/* ---- UnregisterStateLock(NULL, 0): E_HANDLE-class failure mapped to last error 6 ---- */
UDECL(BOOL, UnregisterStateLock, (void *, DWORD));
TEST(gap28_unregisterstatelock_null) {
  REQUIRE(UN(UnregisterStateLock));
  SET_ERR(0);
  int r = UN(UnregisterStateLock)(NULL, 0);
  EXPECT_EQ(r, 0);
  EXPECT_ERR(ERROR_INVALID_HANDLE);
}

/* ---- _initterm / _initterm_e (CRT table walkers) ---- */
static int g_calls, g_order[8];
static void ft_a(void) { g_order[g_calls++] = 1; }
static void ft_b(void) { g_order[g_calls++] = 2; }
static void ft_c(void) { g_order[g_calls++] = 3; }
static int fe_ok(void) { g_order[g_calls++] = 1; return 0; }
static int fe_fail(void) { g_order[g_calls++] = 2; return 77; }
static int fe_never(void) { g_order[g_calls++] = 3; return 0; }
typedef void (__cdecl *initterm_t)(void *, void *);
typedef int (__cdecl *initterm_e_t)(void *, void *);

TEST(gap28_initterm_runs_in_order) {
  initterm_t f = (initterm_t)t_proc_kb("_initterm"); REQUIRE(f);
  void *tab[3] = { (void *)ft_a, (void *)ft_b, (void *)ft_c };
  g_calls = 0;
  f(tab, tab + 3);
  EXPECT_EQ(g_calls, 3);
  EXPECT_EQ(g_order[0], 1); EXPECT_EQ(g_order[1], 2); EXPECT_EQ(g_order[2], 3);
}
TEST(gap28_initterm_skips_null_entries) {
  initterm_t f = (initterm_t)t_proc_kb("_initterm"); REQUIRE(f);
  void *tab[5] = { NULL, (void *)ft_a, NULL, NULL, (void *)ft_b };
  g_calls = 0;
  f(tab, tab + 5);
  EXPECT_EQ(g_calls, 2);
  EXPECT_EQ(g_order[0], 1); EXPECT_EQ(g_order[1], 2);
}
TEST(gap28_initterm_empty_range) {
  initterm_t f = (initterm_t)t_proc_kb("_initterm"); REQUIRE(f);
  void *tab[2] = { (void *)ft_a, (void *)ft_b };
  g_calls = 0;
  f(tab, tab);
  EXPECT_EQ(g_calls, 0);
}
TEST(gap28_initterm_reversed_range_is_empty) {
  initterm_t f = (initterm_t)t_proc_kb("_initterm"); REQUIRE(f);
  void *tab[2] = { (void *)ft_a, (void *)ft_b };
  g_calls = 0;
  f(tab + 2, tab);
  EXPECT_EQ(g_calls, 0);
}
TEST(gap28_initterm_end_is_exclusive) {
  initterm_t f = (initterm_t)t_proc_kb("_initterm"); REQUIRE(f);
  void *tab[3] = { (void *)ft_a, (void *)ft_b, (void *)ft_c };
  g_calls = 0;
  f(tab, tab + 2);
  EXPECT_EQ(g_calls, 2);
  g_calls = 0;
  f(tab + 1, tab + 2);
  EXPECT_EQ(g_calls, 1);
  EXPECT_EQ(g_order[0], 2);
}
TEST(gap28_initterm_all_null) {
  initterm_t f = (initterm_t)t_proc_kb("_initterm"); REQUIRE(f);
  void *tab[4] = { 0, 0, 0, 0 };
  g_calls = 0;
  f(tab, tab + 4);
  EXPECT_EQ(g_calls, 0);
}
TEST(gap28_initterm_repeatable) {
  initterm_t f = (initterm_t)t_proc_kb("_initterm"); REQUIRE(f);
  void *tab[1] = { (void *)ft_a };
  g_calls = 0;
  for (int i = 0; i < 5; i++) f(tab, tab + 1);
  EXPECT_EQ(g_calls, 5);
}
TEST(gap28_initterm_e_all_ok) {
  initterm_e_t f = (initterm_e_t)t_proc_kb("_initterm_e"); REQUIRE(f);
  void *tab[3] = { (void *)fe_ok, NULL, (void *)fe_ok };
  g_calls = 0;
  int r = f(tab, tab + 3);
  EXPECT_EQ(g_calls, 2);
  EXPECT_EQ(r, 0);
}
TEST(gap28_initterm_e_stops_on_error) {
  initterm_e_t f = (initterm_e_t)t_proc_kb("_initterm_e"); REQUIRE(f);
  void *tab[3] = { (void *)fe_ok, (void *)fe_fail, (void *)fe_never };
  g_calls = 0;
  int r = f(tab, tab + 3);
  EXPECT_EQ(g_calls, 2);
  EXPECT_EQ(g_order[0], 1); EXPECT_EQ(g_order[1], 2);
  EXPECT_EQ(r, 77);
}
TEST(gap28_initterm_e_empty_and_reversed) {
  initterm_e_t f = (initterm_e_t)t_proc_kb("_initterm_e"); REQUIRE(f);
  void *tab[2] = { (void *)fe_fail, (void *)fe_fail };
  g_calls = 0;
  EXPECT_EQ(f(tab, tab), 0);
  EXPECT_EQ(f(tab + 2, tab), 0);
  EXPECT_EQ(g_calls, 0);
}
TEST(gap28_initterm_e_first_fails) {
  initterm_e_t f = (initterm_e_t)t_proc_kb("_initterm_e"); REQUIRE(f);
  void *tab[2] = { (void *)fe_fail, (void *)fe_ok };
  g_calls = 0;
  int r = f(tab, tab + 2);   /* EXPECT_EQ evaluates its operands twice: call exactly once */
  EXPECT_EQ(r, 77);
  EXPECT_EQ(g_calls, 1);
}
TEST(gap28_initterm_e_null_then_fail) {
  initterm_e_t f = (initterm_e_t)t_proc_kb("_initterm_e"); REQUIRE(f);
  void *tab[3] = { NULL, (void *)fe_fail, (void *)fe_ok };
  g_calls = 0;
  int r = f(tab, tab + 3);   /* EXPECT_EQ evaluates its operands twice: call exactly once */
  EXPECT_EQ(r, 77);
  EXPECT_EQ(g_calls, 1);
}

/* ---- _invalid_parameter: only logs via DbgPrint and returns (reference analysis verified) ---- */
TEST(gap28_invalid_parameter_returns) {
  typedef void (__cdecl *fn_t)(void);
  fn_t f = (fn_t)t_proc_kb("_invalid_parameter"); REQUIRE(f);
  f();
  f();
  CHECK(1);
}

/* ---- _exit / _c_exit / _cexit / _amsg_exit in child processes ---- */
CHILD(gap28_exit_code) {
  typedef void (__cdecl *fn_t)(int);
  fn_t f = (fn_t)t_proc_kb("_exit");
  if (!f) return 90;
  f(argc > 1 ? atoi(argv[1]) : 0);
  return 91;  /* not reached */
}
TEST(gap28_exit_terminates_with_code) {
  REQUIRE(t_proc_kb("_exit"));
  EXPECT_EQ(t_run_child("gap28_exit_code", "37", 20000), 37);
  EXPECT_EQ(t_run_child("gap28_exit_code", "0", 20000), 0);
  EXPECT_EQ(t_run_child("gap28_exit_code", "255", 20000), 255);
}
CHILD(gap28_c_exit_returns) {
  typedef void (__cdecl *fn_t)(void);
  fn_t f = (fn_t)t_proc_kb("_c_exit");
  if (!f) return 90;
  f();   /* performs cleanup without terminating the process */
  return 55;
}
TEST(gap28_c_exit_does_not_terminate) {
  REQUIRE(t_proc_kb("_c_exit"));
  EXPECT_EQ(t_run_child("gap28_c_exit_returns", "", 20000), 55);
}
CHILD(gap28_cexit_returns) {
  typedef void (__cdecl *fn_t)(void);
  fn_t f = (fn_t)t_proc_kb("_cexit");
  if (!f) return 90;
  f();
  return 56;
}
TEST(gap28_cexit_does_not_terminate) {
  REQUIRE(t_proc_kb("_cexit"));
  EXPECT_EQ(t_run_child("gap28_cexit_returns", "", 20000), 56);
}

/* Presence-only items with side effects (not called): Wer*, Termsrv*, VDM*, Test*, UT*,
 * State* writers, MUI cache, __wgetmainargs (reparses the command line into CRT globals),
 * __chkstk/__misaligned_access (custom register conventions), __C_specific_handler. */
TEST(gap28_c_specific_handler_k32_kb_both) {
  if (t_is_wine()) { t_skip("module layout differs on wine"); return; }
  CHECK(t_proc_k32("__C_specific_handler") != NULL);
  CHECK(t_proc_kb("__C_specific_handler") != NULL);
  CHECK(t_proc_k32("__chkstk") != NULL);
  CHECK(t_proc_kb("__chkstk") != NULL);
}
