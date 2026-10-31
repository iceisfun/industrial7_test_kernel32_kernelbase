/* gap_22: non-public / internal exports (calendar week, package context/property,
 * package status, LZ expanded name, NLS helpers). Only argument-validation paths and
 * side-effect-free calls verified from decompilation are exercised. */
#include "t.h"

static const struct { const char *n; int k32, kb; } ex[] = {
  {"GetCalendarWeekNumber", 1, 0},
  {"GetConsoleHistoryInfo", 1, 1},
  {"GetCurrentPackageApplicationContext", 0, 1},
  {"GetCurrentPackageApplicationResourcesContext", 0, 1},
  {"GetCurrentPackageContext", 0, 1},
  {"GetCurrentPackageGlobalizationContext", 0, 1},
  {"GetCurrentPackageInfo3", 0, 1},
  {"GetCurrentPackageResourcesContext", 0, 1},
  {"GetCurrentPackageSecurityContext", 0, 1},
  {"GetCurrentTargetPlatformContext", 0, 1},
  {"GetDateFormatAWorker", 1, 0},
  {"GetDateFormatWWorker", 1, 0},
  {"GetEffectivePackageStatusForUser", 0, 1},
  {"GetEffectivePackageStatusForUserSid", 0, 1},
  {"GetEightBitStringToUnicodeSizeRoutine", 0, 1},
  {"GetEightBitStringToUnicodeStringRoutine", 0, 1},
  {"GetEraNameCountedString", 1, 1},
  {"GetExpandedNameA", 1, 0},
  {"GetExpandedNameW", 1, 0},
  {"GetExtensionApplicationUserModelId", 0, 1},
  {"GetExtensionProgIds", 0, 1},
  {"GetExtensionProperty", 0, 1},
  {"GetExtensionProperty2", 0, 1},
  {"GetFallbackDisplayName", 0, 1},
  {"GetFileVersionInfoByHandle", 0, 1},
  {"GetHivePath", 0, 1},
  {"GetLocaleInfoHelper", 0, 1},
  {"GetNamedLocaleHashNode", 0, 1},
  {"GetNextVDMCommand", 1, 0},
  {"GetPackageApplicationContext", 0, 1},
  {"GetPackageApplicationProperty", 0, 1},
  {"GetPackageApplicationPropertyString", 0, 1},
  {"GetPackageApplicationResourcesContext", 0, 1},
  {"GetPackageContext", 0, 1},
  {"GetPackageFamilyNameFromProgId", 0, 1},
  {"GetPackageGlobalizationContext", 0, 1},
  {"GetPackageGlobalizationProperty", 0, 1},
  {"GetPackageInfo3", 0, 1},
  {"GetPackageInstallTime", 0, 1},
  {"GetPackageOSMaxVersionTested", 0, 1},
  {"GetPackagePathOnVolume", 0, 1},
  {"GetPackageProperty", 0, 1},
  {"GetPackagePropertyString", 0, 1},
  {"GetPackageResourcesContext", 0, 1},
  {"GetPackageResourcesProperty", 0, 1},
  {"GetPackageSecurityContext", 0, 1},
  {"GetPackageSecurityProperty", 0, 1},
  {"GetPackageStatus", 0, 1},
  {"GetPackageStatusForUser", 0, 1},
  {"GetPackageStatusForUserSid", 0, 1},
};
#define NEX (sizeof ex / sizeof ex[0])

static FARPROC kb_or_k32(const char *n) { FARPROC p = t_proc_kb(n); return p ? p : t_proc_k32(n); }

#define ERR_NO_PACKAGE 15700
typedef LONG (WINAPI *ctx3_t)(UINT, void *, void **);
typedef LONG (WINAPI *ctx4_t)(void *, UINT, void *, void **);
typedef LONG (WINAPI *prop_t)(void *, UINT, UINT *, void *);
typedef LONG (WINAPI *prop5_t)(void *, UINT, UINT *, void *, UINT *);

TEST(gap22_export_presence) {
    for (size_t i = 0; i < NEX; i++) {
        FARPROC a = t_proc_k32(ex[i].n), b = t_proc_kb(ex[i].n);
        if (ex[i].k32) { if (!a && t_is_wine()) t_note("wine lacks kernel32 %s", ex[i].n); else CHECKF(a != NULL, "kernel32 export %s", ex[i].n); }
        else if (a) t_note("%s also resolves via kernel32", ex[i].n);
        if (ex[i].kb) { if (!b && t_is_wine()) t_note("wine lacks kernelbase %s", ex[i].n); else CHECKF(b != NULL, "kernelbase export %s", ex[i].n); }
        else if (b) t_note("%s also resolves via kernelbase", ex[i].n);
        CHECKF(ex[i].k32 || ex[i].kb, "%s listed", ex[i].n);
    }
}
TEST(gap22_resolve_generic_and_stable) {
    for (size_t i = 0; i < NEX; i++) {
        FARPROC a = t_proc(ex[i].n);
        if (!a && t_is_wine()) continue;
        CHECKF(a != NULL, "t_proc %s", ex[i].n);
        CHECKF(a == t_proc(ex[i].n), "stable %s", ex[i].n);
    }
}
TEST(gap22_address_in_module) {
    for (size_t i = 0; i < NEX; i++) {
        FARPROC a = t_proc(ex[i].n);
        if (!a) continue;
        HMODULE m = NULL;
        CHECKF(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)a, &m) && m != NULL, "%s module", ex[i].n);
    }
}
TEST(gap22_k32_forwards_to_kernelbase_when_both) {
    /* kernel32 forwarders resolve to the kernelbase implementation */
    for (size_t i = 0; i < NEX; i++) {
        if (!(ex[i].k32 && ex[i].kb)) continue;
        FARPROC a = t_proc_k32(ex[i].n), b = t_proc_kb(ex[i].n);
        if (!a || !b) continue;
        if (a != b) t_note("%s: kernel32 and kernelbase addresses differ (wrapper, not forwarder)", ex[i].n);
    }
}
TEST(gap22_no_name_aliasing) {
    for (size_t i = 0; i < NEX; i++)
        for (size_t j = i + 1; j < NEX; j++) {
            FARPROC a = t_proc(ex[i].n), b = t_proc(ex[j].n);
            if (a && b && a == b) t_note("%s and %s share an address (identical code folding)", ex[i].n, ex[j].n);
        }
}

/* ---- GetCalendarWeekNumber: invalid arguments fail before the date is read ---- */
TEST(gap22_calweek_invalid_params) {
    typedef BOOL (WINAPI *fn_t)(void *, UINT, UINT, int *);
    fn_t f = (fn_t)t_proc_k32("GetCalendarWeekNumber");
    REQUIRE(f);
    unsigned char dummy[64] = {0};
    int out = 0x1234;
    static const struct { UINT a, b; } bad[] = { {7,0}, {8,0}, {0xFFFFFFFF,0}, {0,3}, {0,4}, {0,0xFFFFFFFF}, {7,3}, {100,100} };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        out = 0x1234; SET_ERR(0);
        CHECKF(f(dummy, bad[i].a, bad[i].b, &out) == FALSE, "case %zu", i);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        EXPECT_EQ(out, 0x1234);
    }
    SET_ERR(0);
    CHECK(f(dummy, 0, 0, NULL) == FALSE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(f(NULL, 7, 0, &out) == FALSE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(f(dummy, 0, 3, NULL) == FALSE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}

/* ---- Current package contexts: validation and "no package" in an unpackaged process ---- */
static const char *const cur_ctx3[] = {
    "GetCurrentPackageApplicationContext", "GetCurrentPackageApplicationResourcesContext",
    "GetCurrentPackageContext", "GetCurrentPackageGlobalizationContext", "GetCurrentPackageResourcesContext" };
TEST(gap22_curpkg_ctx_null_out) {
    for (size_t i = 0; i < sizeof cur_ctx3 / sizeof *cur_ctx3; i++) {
        ctx3_t f = (ctx3_t)kb_or_k32(cur_ctx3[i]);
        if (!f) { if (!t_is_wine()) CHECKF(0, "missing %s", cur_ctx3[i]); continue; }
        if (t_is_wine()) continue;
        for (UINT idx = 0; idx < 3; idx++) {
            EXPECT_EQ(f(idx, NULL, NULL), ERROR_INVALID_PARAMETER);
        }
    }
}
TEST(gap22_curpkg_ctx_unpackaged) {
    if (t_is_wine()) { t_skip("package graph not implemented"); return; }
    for (size_t i = 0; i < sizeof cur_ctx3 / sizeof *cur_ctx3; i++) {
        ctx3_t f = (ctx3_t)kb_or_k32(cur_ctx3[i]);
        REQUIRE(f);
        void *ctx = (void *)(ULONG_PTR)0x1111;
        LONG r = f(0, NULL, &ctx);
        CHECKF(r == ERR_NO_PACKAGE, "%s -> %ld", cur_ctx3[i], (long)r);
        EXPECT_EQ(ctx, 0x1111);   /* untouched on failure */
    }
}
TEST(gap22_curpkg_security_and_target_ctx) {
    typedef LONG (WINAPI *fn_t)(void *, void **);
    static const char *const names[] = { "GetCurrentPackageSecurityContext", "GetCurrentTargetPlatformContext" };
    if (t_is_wine()) { t_skip("package graph not implemented"); return; }
    for (size_t i = 0; i < 2; i++) {
        fn_t f = (fn_t)kb_or_k32(names[i]);
        REQUIRE(f);
        EXPECT_EQ(f(NULL, NULL), ERROR_INVALID_PARAMETER);
        void *ctx = (void *)(ULONG_PTR)0x2222;
        LONG r = f(NULL, &ctx);
        CHECKF(r == ERR_NO_PACKAGE, "%s -> %ld", names[i], (long)r);
        EXPECT_EQ(ctx, 0x2222);
    }
}
TEST(gap22_curpkg_info3_validation) {
    typedef LONG (WINAPI *fn_t)(UINT, int, UINT *, void *, UINT *);
    fn_t f = (fn_t)kb_or_k32("GetCurrentPackageInfo3");
    REQUIRE(f);
    if (t_is_wine()) { t_skip("package graph not implemented"); return; }
    UINT len = 0, cnt = 0;
    EXPECT_EQ(f(1, 1, NULL, NULL, NULL), 0x80070057);
    EXPECT_EQ(f(1, 1, NULL, NULL, &cnt), 0x80070057);
    len = 16;   /* non-zero length requires a buffer */
    EXPECT_EQ(f(1, 1, &len, NULL, &cnt), 0x80070057);
    EXPECT_EQ(len, 16);
    len = 0;
    LONG r = f(1, 1, &len, NULL, &cnt);
    t_note("GetCurrentPackageInfo3 unpackaged -> 0x%lx (decomp: 0x80073d54)", (unsigned long)r);
    CHECK(r != 0);
}

/* ---- Package-context based functions: NULL context / NULL out validation ---- */
TEST(gap22_pkg_ctx_getters_null) {
    static const char *const names[] = { "GetPackageApplicationContext", "GetPackageApplicationResourcesContext",
        "GetPackageContext", "GetPackageGlobalizationContext", "GetPackageResourcesContext" };
    unsigned char dummy[64] = {0};
    void *out = NULL;
    if (t_is_wine()) { t_skip("package graph not implemented"); return; }
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) {
        ctx4_t f = (ctx4_t)kb_or_k32(names[i]);
        REQUIRE(f);
        EXPECT_EQ(f(NULL, 0, NULL, &out), ERROR_INVALID_PARAMETER);
        EXPECT_EQ(f(NULL, 0, NULL, NULL), ERROR_INVALID_PARAMETER);
        EXPECT_EQ(f(dummy, 0, NULL, NULL), ERROR_INVALID_PARAMETER);   /* out is checked before the context is read */
        EXPECT_EQ(f(NULL, 5, NULL, &out), ERROR_INVALID_PARAMETER);
        EXPECT_EQ(out, 0);
    }
}
TEST(gap22_pkg_security_ctx_null) {
    typedef LONG (WINAPI *fn_t)(void *, void *, void **);
    fn_t f = (fn_t)kb_or_k32("GetPackageSecurityContext");
    REQUIRE(f);
    if (t_is_wine()) { t_skip("package graph not implemented"); return; }
    unsigned char dummy[128] = {0};
    void *out = NULL;
    EXPECT_EQ(f(NULL, NULL, &out), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(f(dummy, NULL, NULL), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(f(NULL, NULL, NULL), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(out, 0);
}
TEST(gap22_pkg_osmaxversion_null) {
    typedef LONG (WINAPI *fn_t)(void *, ULONGLONG *);
    fn_t f = (fn_t)kb_or_k32("GetPackageOSMaxVersionTested");
    REQUIRE(f);
    if (t_is_wine()) { t_skip("not implemented"); return; }
    unsigned char dummy[64] = {0};
    ULONGLONG v = 0xABCD;
    EXPECT_EQ(f(NULL, &v), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(f(NULL, NULL), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(f(dummy, NULL), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(v, 0xABCD);
}

/* ---- Property getters: parameter validation happens before the context is dereferenced ---- */
static const char *const prop4[] = { "GetPackageProperty", "GetPackageApplicationProperty",
    "GetPackageSecurityProperty", "GetPackageGlobalizationProperty" };
TEST(gap22_pkg_property_null_ctx) {
    unsigned char dummy[64] = {0};
    char buf[64];
    if (t_is_wine()) { t_skip("package graph not implemented"); return; }
    for (size_t i = 0; i < sizeof prop4 / sizeof *prop4; i++) {
        prop_t f = (prop_t)kb_or_k32(prop4[i]);
        REQUIRE(f);
        UINT len = 8;
        for (UINT id = 0; id < 6; id++) {
            len = 8;
            CHECKF(f(NULL, id, &len, buf) == ERROR_INVALID_PARAMETER, "%s id %u", prop4[i], id);
            CHECKF(len == 8, "%s len untouched", prop4[i]);
        }
        EXPECT_EQ(f(NULL, 1, NULL, buf), ERROR_INVALID_PARAMETER);
        EXPECT_EQ(f(NULL, 1, NULL, NULL), ERROR_INVALID_PARAMETER);
        EXPECT_EQ(f(dummy, 1, NULL, buf), ERROR_INVALID_PARAMETER);
        len = 8;
        CHECKF(f(dummy, 1, &len, NULL) == ERROR_INVALID_PARAMETER, "%s nonzero len needs buffer", prop4[i]);
        CHECKF(len == 8, "%s len after", prop4[i]);
    }
}
TEST(gap22_pkg_resources_property_null) {
    prop5_t f = (prop5_t)kb_or_k32("GetPackageResourcesProperty");
    REQUIRE(f);
    if (t_is_wine()) { t_skip("package graph not implemented"); return; }
    unsigned char dummy[64] = {0};
    char buf[16]; UINT len = 4, extra = 0;
    EXPECT_EQ(f(NULL, 1, &len, buf, &extra), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(f(NULL, 1, NULL, buf, NULL), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(f(dummy, 1, NULL, buf, NULL), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(f(dummy, 1, &len, NULL, NULL), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(len, 4);
}
TEST(gap22_pkg_property_string_validation) {
    prop_t f = (prop_t)kb_or_k32("GetPackagePropertyString");
    REQUIRE(f);
    if (t_is_wine()) { t_skip("package graph not implemented"); return; }
    unsigned char dummy[64] = {0};
    WCHAR buf[8]; UINT len;
    /* valid ids: bits of 0x7df2 -> 1,4..14 ; invalid: 0,2,3,15+ */
    static const UINT bad_ids[] = { 0, 2, 3, 15, 16, 100, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof bad_ids / sizeof *bad_ids; i++) {
        len = 8;
        CHECKF(f(dummy, bad_ids[i], &len, buf) == ERROR_INVALID_PARAMETER, "id %u", bad_ids[i]);
        CHECKF(len == 8, "len untouched id %u", bad_ids[i]);
    }
    len = 0x80000000u;
    EXPECT_EQ(f(dummy, 1, &len, buf), ERROR_INVALID_PARAMETER);
    len = 0xFFFFFFFFu;
    EXPECT_EQ(f(dummy, 1, &len, buf), ERROR_INVALID_PARAMETER);
    len = 8;
    EXPECT_EQ(f(dummy, 1, &len, NULL), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(f(dummy, 1, NULL, buf), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(f(NULL, 1, &len, buf), ERROR_INVALID_PARAMETER);
}
TEST(gap22_pkg_app_property_string_validation) {
    prop_t f = (prop_t)kb_or_k32("GetPackageApplicationPropertyString");
    REQUIRE(f);
    if (t_is_wine()) { t_skip("package graph not implemented"); return; }
    unsigned char dummy[64] = {0};
    WCHAR buf[8]; UINT len;
    /* valid ids: bits of 0x1557e -> 1..6,8,10,12,14,16 ; invalid: 0,7,9,11,13,15,17+ */
    static const UINT bad_ids[] = { 0, 7, 9, 11, 13, 15, 17, 18, 0x20, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof bad_ids / sizeof *bad_ids; i++) {
        len = 8;
        CHECKF(f(dummy, bad_ids[i], &len, buf) == ERROR_INVALID_PARAMETER, "id %u", bad_ids[i]);
        CHECKF(len == 8, "len untouched id %u", bad_ids[i]);
    }
    len = 0x80000000u;
    EXPECT_EQ(f(dummy, 1, &len, buf), ERROR_INVALID_PARAMETER);
    len = 8;
    EXPECT_EQ(f(dummy, 1, &len, NULL), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(f(dummy, 1, NULL, buf), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(f(NULL, 1, &len, buf), ERROR_INVALID_PARAMETER);
}
TEST(gap22_pkg_info3_validation) {
    typedef LONG (WINAPI *fn_t)(void *, UINT, int, UINT *, void *, UINT *);
    fn_t f = (fn_t)kb_or_k32("GetPackageInfo3");
    REQUIRE(f);
    if (t_is_wine()) { t_skip("package graph not implemented"); return; }
    unsigned char dummy[64] = {0};
    unsigned char buf[16]; UINT len = 16, cnt = 0;
    for (int type = 0; type <= 5; type++) {
        len = 16;
        CHECKF(f(NULL, 0, type, &len, buf, &cnt) == ERROR_INVALID_PARAMETER, "null ctx type %d", type);
        CHECKF(f(dummy, 0, type, NULL, buf, &cnt) == ERROR_INVALID_PARAMETER, "null len type %d", type);
        len = 16;
        CHECKF(f(dummy, 0, type, &len, NULL, &cnt) == ERROR_INVALID_PARAMETER, "null buf type %d", type);
        len = 16;
        CHECKF(f(dummy, 0, type, &len, buf, NULL) == ERROR_INVALID_PARAMETER, "null count type %d", type);
    }
}

/* ---- Package status APIs ---- */
TEST(gap22_pkg_status_validation) {
    typedef LONG (WINAPI *fn_t)(const WCHAR *, UINT *);
    fn_t f = (fn_t)kb_or_k32("GetPackageStatus");
    REQUIRE(f);
    if (t_is_wine()) { t_skip("not implemented"); return; }
    UINT st = 0x5555;
    EXPECT_EQ(f(NULL, &st), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(f(L"", &st), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(f(L"k32t.nonexistent_1.0.0.0_x64__0000000000000", NULL), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(f(NULL, NULL), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(st, 0x5555);
}
TEST(gap22_pkg_status_unknown_package) {
    typedef LONG (WINAPI *fn_t)(const WCHAR *, UINT *);
    fn_t f = (fn_t)kb_or_k32("GetPackageStatus");
    REQUIRE(f);
    if (t_is_wine()) { t_skip("not implemented"); return; }
    UINT st = 0x5555;
    LONG r = f(L"k32t.nonexistent_1.0.0.0_x64__0000000000000", &st);
    t_note("GetPackageStatus(unknown) -> %ld", (long)r);
    CHECK(r != 0);
    EXPECT_EQ(st, 0x5555);
    r = f(L"a", &st);   /* too short / malformed full name */
    CHECK(r != 0);
}
TEST(gap22_pkg_status_overlong_name) {
    typedef LONG (WINAPI *fn_t)(const WCHAR *, UINT *);
    fn_t f = (fn_t)kb_or_k32("GetPackageStatus");
    REQUIRE(f);
    if (t_is_wine()) { t_skip("not implemented"); return; }
    static WCHAR big[1024];
    for (int i = 0; i < 1023; i++) big[i] = L'a';
    UINT st = 0x5555;
    LONG r = f(big, &st);
    CHECK(r != 0);
    EXPECT_EQ(st, 0x5555);
}
TEST(gap22_pkg_status_for_user_validation) {
    typedef LONG (WINAPI *fn_t)(void *, const WCHAR *, UINT *);
    static const char *const names[] = { "GetPackageStatusForUser", "GetPackageStatusForUserSid",
        "GetEffectivePackageStatusForUser", "GetEffectivePackageStatusForUserSid" };
    if (t_is_wine()) { t_skip("not implemented"); return; }
    for (size_t i = 0; i < 4; i++) {
        fn_t f = (fn_t)kb_or_k32(names[i]);
        REQUIRE(f);
        UINT st = 0x7777;
        CHECKF(f(NULL, NULL, &st) == ERROR_INVALID_PARAMETER, "%s null name", names[i]);
        CHECKF(f(NULL, L"k32t.nonexistent", NULL) == ERROR_INVALID_PARAMETER, "%s null out", names[i]);
        CHECKF(f(NULL, NULL, NULL) == ERROR_INVALID_PARAMETER, "%s all null", names[i]);
        if (i < 2) CHECKF(f(NULL, L"", &st) == ERROR_INVALID_PARAMETER, "%s empty name", names[i]);
        CHECKF(st == 0x7777, "%s out untouched", names[i]);
    }
}
TEST(gap22_pkg_status_effective_unknown) {
    typedef LONG (WINAPI *fn_t)(void *, const WCHAR *, UINT *);
    fn_t f = (fn_t)kb_or_k32("GetEffectivePackageStatusForUserSid");
    REQUIRE(f);
    if (t_is_wine()) { t_skip("not implemented"); return; }
    UINT st = 0x7777;
    LONG r = f(NULL, L"k32t.nonexistent_1.0.0.0_x64__0000000000000", &st);
    CHECK(r != 0);   /* package-level status lookup fails first */
    EXPECT_EQ(st, 0x7777);
}
TEST(gap22_pkg_install_time_validation) {
    typedef LONG (WINAPI *fn_t)(const WCHAR *, FILETIME *);
    fn_t f = (fn_t)kb_or_k32("GetPackageInstallTime");
    REQUIRE(f);
    if (t_is_wine()) { t_skip("not implemented"); return; }
    FILETIME ft = { 0x1234, 0x5678 };
    EXPECT_EQ(f(NULL, &ft), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(f(L"k32t.nonexistent_1.0.0.0_x64__0000000000000", NULL), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(f(NULL, NULL), ERROR_INVALID_PARAMETER);
    LONG r = f(L"k32t.nonexistent_1.0.0.0_x64__0000000000000", &ft);
    CHECK(r != 0);
    EXPECT_EQ(ft.dwLowDateTime, 0x1234);
    EXPECT_EQ(ft.dwHighDateTime, 0x5678);
}
TEST(gap22_pkg_path_on_volume_validation) {
    typedef LONG (WINAPI *fn_t)(const WCHAR *, const WCHAR *, UINT *, WCHAR *);
    fn_t f = (fn_t)kb_or_k32("GetPackagePathOnVolume");
    REQUIRE(f);
    if (t_is_wine()) { t_skip("not implemented"); return; }
    WCHAR buf[300]; UINT len = 300;
    EXPECT_EQ(f(L"C:\\", NULL, &len, buf), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(f(L"C:\\", L"bogus", &len, buf), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(f(L"C:\\", L"", &len, buf), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(f(L"C:\\", L"k32t.nonexistent_1.0.0.0_x64__0000000000000", NULL, buf), ERROR_INVALID_PARAMETER);
    len = 300;
    EXPECT_EQ(f(L"C:\\", L"k32t.nonexistent_1.0.0.0_x64__0000000000000", &len, NULL), ERROR_INVALID_PARAMETER);
}
TEST(gap22_pkg_family_from_progid_validation) {
    typedef ULONG (WINAPI *fn_t)(const WCHAR *, UINT *, WCHAR *);
    fn_t f = (fn_t)kb_or_k32("GetPackageFamilyNameFromProgId");
    REQUIRE(f);
    if (t_is_wine()) { t_skip("not implemented"); return; }
    WCHAR buf[64]; UINT len = 64;
    EXPECT_EQ(f(NULL, &len, buf), 0x80070057);
    EXPECT_EQ(f(L"", &len, buf), 0x80070057);
    EXPECT_EQ(f(L"k32t.progid", NULL, buf), 0x80070057);
    len = 0;
    EXPECT_EQ(f(L"k32t.progid", &len, buf), 0x80070057);
    len = 64;
    EXPECT_EQ(f(L"k32t.progid", &len, NULL), 0x80070057);
    EXPECT_EQ(len, 64);
}

/* ---- GetExpandedName (LZ expand name) ---- */
TEST(gap22_expandedname_plain_file) {
    typedef INT (WINAPI *fa_t)(LPSTR, LPSTR);
    fa_t f = (fa_t)t_proc_k32("GetExpandedNameA");
    REQUIRE(f);
    if (t_is_wine()) { t_skip("LZ expand differs"); return; }
    char src[MAX_PATH], out[MAX_PATH];
    GetSystemDirectoryA(src, MAX_PATH);
    lstrcatA(src, "\\kernel32.dll");
    memset(out, 'x', sizeof out);
    EXPECT_EQ(f(src, out), 1);
    EXPECT_STR(out, src);
}
TEST(gap22_expandedname_nonexistent) {
    typedef INT (WINAPI *fa_t)(LPSTR, LPSTR);
    typedef INT (WINAPI *fw_t)(LPWSTR, LPWSTR);
    fa_t fa = (fa_t)t_proc_k32("GetExpandedNameA");
    fw_t fw = (fw_t)t_proc_k32("GetExpandedNameW");
    REQUIRE(fa); REQUIRE(fw);
    if (t_is_wine()) { t_skip("LZ expand differs"); return; }
    char out[MAX_PATH]; WCHAR outw[MAX_PATH];
    char missing[MAX_PATH];
    lstrcpyA(missing, "C:\\k32t_gap22_no_such_dir\\none.dat");
    EXPECT_EQ(fa(missing, out), -7);
    WCHAR wmiss[] = L"C:\\k32t_gap22_no_such_dir\\none.dat";
    EXPECT_EQ(fw(wmiss, outw), -7);
}
TEST(gap22_expandedname_w_plain_file) {
    typedef INT (WINAPI *fw_t)(LPWSTR, LPWSTR);
    fw_t fw = (fw_t)t_proc_k32("GetExpandedNameW");
    REQUIRE(fw);
    if (t_is_wine()) { t_skip("LZ expand differs"); return; }
    WCHAR src[MAX_PATH], out[MAX_PATH];
    GetSystemDirectoryW(src, MAX_PATH);
    lstrcatW(src, L"\\kernel32.dll");
    memset(out, 0, sizeof out);
    INT r = fw(src, out);
    t_note("GetExpandedNameW(plain) -> %d (decomp returns the copy status, 0)", r);
    CHECK(r != -1 && r != -7);
    EXPECT_WSTR(out, src);
}

/* ---- DateFormat workers: thin wrappers over GetDateFormatW / GetDateFormatEx ---- */
TEST(gap22_dateformat_a_worker) {
    typedef int (WINAPI *fn_t)(UINT *, LCID, DWORD, const SYSTEMTIME *, LPCSTR, LPSTR, int);
    fn_t f = (fn_t)t_proc_k32("GetDateFormatAWorker");
    REQUIRE(f);
    if (t_is_wine()) { t_skip("worker not implemented"); return; }
    SYSTEMTIME st = { 2021, 3, 4, 4, 0, 0, 0, 0 };
    UINT cp = CP_ACP;
    char buf[64]; memset(buf, 'x', sizeof buf);
    int r = f(&cp, LOCALE_INVARIANT, 0, &st, "yyyy-MM-dd", buf, sizeof buf);
    EXPECT_EQ(r, 11);
    EXPECT_STR(buf, "2021-03-04");
    /* size query */
    EXPECT_EQ(f(&cp, LOCALE_INVARIANT, 0, &st, "yyyy-MM-dd", NULL, 0), 11);
    /* truncation */
    SET_ERR(0);
    r = f(&cp, LOCALE_INVARIANT, 0, &st, "yyyy-MM-dd", buf, 5);
    EXPECT_EQ(r, 0);
    /* day-of-week and month names for the invariant locale */
    r = f(&cp, LOCALE_INVARIANT, 0, &st, "dddd, MMMM d", buf, sizeof buf);
    CHECK(r > 0);
    EXPECT_STR(buf, "Thursday, March 4");
    /* UTF-8 target code page */
    UINT u8 = CP_UTF8;
    r = f(&u8, LOCALE_INVARIANT, 0, &st, "yy/M/d", buf, sizeof buf);
    EXPECT_STR(buf, "21/3/4");
    EXPECT_EQ(r, 7);
}
TEST(gap22_dateformat_a_worker_matches_api) {
    typedef int (WINAPI *fn_t)(UINT *, LCID, DWORD, const SYSTEMTIME *, LPCSTR, LPSTR, int);
    fn_t f = (fn_t)t_proc_k32("GetDateFormatAWorker");
    REQUIRE(f); REQUIRE(U(GetDateFormatA));
    if (t_is_wine()) { t_skip("worker not implemented"); return; }
    SYSTEMTIME st = { 1999, 12, 5, 31, 0, 0, 0, 0 };
    UINT cp = CP_ACP;
    char a[64], b[64];
    static const LCID lc[] = { LOCALE_INVARIANT, 0x0409, 0x0407, 0x040c };
    for (size_t i = 0; i < 4; i++) {
        memset(a, 0, sizeof a); memset(b, 0, sizeof b);
        int ra = f(&cp, lc[i], DATE_SHORTDATE, &st, NULL, a, sizeof a);
        int rb = U(GetDateFormatA)(lc[i], DATE_SHORTDATE, &st, NULL, b, sizeof b);
        CHECKF(ra == rb, "locale %lx ret %d vs %d", (unsigned long)lc[i], ra, rb);
        CHECKF(!strcmp(a, b), "locale %lx text", (unsigned long)lc[i]);
    }
}
TEST(gap22_dateformat_w_worker) {
    typedef int (WINAPI *fn_t)(const WCHAR *, DWORD, const SYSTEMTIME *, const WCHAR *, WCHAR *, int, UINT);
    fn_t f = (fn_t)t_proc_k32("GetDateFormatWWorker");
    REQUIRE(f); REQUIRE(U(GetDateFormatEx));
    if (t_is_wine()) { t_skip("worker not implemented"); return; }
    SYSTEMTIME st = { 2021, 3, 4, 4, 0, 0, 0, 0 };
    WCHAR a[64] = {0}, b[64] = {0};
    int ra = f(L"en-US", 0, &st, L"yyyy-MM-dd", a, 64, 0);
    int rb = U(GetDateFormatEx)(L"en-US", 0, &st, L"yyyy-MM-dd", b, 64, NULL);
    t_note("GetDateFormatWWorker ret %d, GetDateFormatEx ret %d", ra, rb);
    CHECK(ra == rb);
    CHECK(!lstrcmpW(a, b));
}

/* ---- Eight-bit-string routine getters: return stored function pointers ---- */
TEST(gap22_eightbit_routines) {
    typedef void *(WINAPI *fn_t)(void);
    fn_t fs = (fn_t)kb_or_k32("GetEightBitStringToUnicodeSizeRoutine");
    fn_t ft = (fn_t)kb_or_k32("GetEightBitStringToUnicodeStringRoutine");
    REQUIRE(fs); REQUIRE(ft);
    if (t_is_wine()) { t_skip("not implemented"); return; }
    void *a = fs(), *b = ft();
    CHECK(a == fs());
    CHECK(b == ft());
    t_note("size routine %p string routine %p", a, b);
    if (a && b) CHECK(a != b);
}

/* ---- NLS helpers ---- */
TEST(gap22_named_locale_hash_node) {
    typedef void *(WINAPI *fn_t)(const WCHAR *, SHORT);
    fn_t f = (fn_t)kb_or_k32("GetNamedLocaleHashNode");
    REQUIRE(f);
    if (t_is_wine()) { t_skip("not implemented"); return; }
    void *inv = f(L"", 0);
    CHECK(inv != NULL);
    CHECK(f(L"", 0) == inv);
    void *us = f(L"en-US", 0);
    t_note("en-US node %p, invariant node %p", us, inv);
    if (us) { CHECK(f(L"en-US", 0) == us); CHECK(us != inv); CHECK(f(L"EN-us", 0) == us); }
}
TEST(gap22_getlocaleinfo_helper_present) {
    CHECK(t_proc_kb("GetLocaleInfoHelper") != NULL || t_is_wine());
    CHECK(t_proc_kb("GetEraNameCountedString") != NULL || t_is_wine());
    CHECK(t_proc_kb("GetFallbackDisplayName") != NULL || t_is_wine());
}

/* ---- Console history info: only in a detached child (no console) ---- */
CHILD(gap22_conhist) {
    typedef BOOL (WINAPI *fn_t)(CONSOLE_HISTORY_INFO *);
    fn_t f = (fn_t)t_proc_k32("GetConsoleHistoryInfo");
    if (!f) return 2;
    CONSOLE_HISTORY_INFO ci;
    memset(&ci, 0, sizeof ci);
    ci.cbSize = 1;   /* wrong size */
    SetLastError(0);
    BOOL r = f(&ci);
    DWORD e = GetLastError();
    printf("badsize ret=%d err=%lu\n", r, (unsigned long)e);
    ci.cbSize = sizeof ci;
    SetLastError(0);
    r = f(&ci);
    e = GetLastError();
    printf("noconsole ret=%d err=%lu\n", r, (unsigned long)e);
    return 0;
}
TEST(gap22_console_history_info_detached) {
    REQUIRE(t_proc_k32("GetConsoleHistoryInfo"));
    if (t_is_wine()) { t_skip("console under wine"); return; }
    DWORD rc = t_run_child("gap22_conhist", "", 20000);
    EXPECT_EQ(rc, 0);
}
