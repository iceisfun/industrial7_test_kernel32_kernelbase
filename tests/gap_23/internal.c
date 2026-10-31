#include "t.h"
/* Non-public / internal exports: presence, stability, module ownership, and the few trivially safe calls. */
static const struct { const char *n; int k32, kb; } ex[] = {
  {"GetPackageTargetPlatformProperty", 0, 1},
  {"GetPackageVolumeSisPath", 0, 1},
  {"GetPersistedFileLocationW", 0, 1},
  {"GetPersistedRegistryLocationW", 0, 1},
  {"GetPersistedRegistryValueW", 0, 1},
  {"GetProcAddressForCaller", 0, 1},
  {"GetProtocolAumid", 0, 1},
  {"GetProtocolProperty", 0, 1},
  {"GetPtrCalData", 0, 1},
  {"GetPtrCalDataArray", 0, 1},
  {"GetPublisherCacheFolder", 0, 1},
  {"GetPublisherRootFolder", 0, 1},
  {"GetRegistryExtensionFlags", 0, 1},
  {"GetRegistryValueWithFallbackW", 0, 1},
  {"GetRoamingLastObservedChangeTime", 0, 1},
  {"GetSecureSystemAppDataFolder", 0, 1},
  {"GetSerializedAtomBytes", 0, 1},
  {"GetSharedLocalFolder", 0, 1},
  {"GetStateContainerDepth", 0, 1},
  {"GetStateFolder", 1, 1},
  {"GetStateRootFolder", 0, 1},
  {"GetStateRootFolderBase", 0, 1},
  {"GetStateSettingsFolder", 0, 1},
  {"GetStateVersion", 0, 1},
  {"GetStringTableEntry", 0, 1},
  {"GetSystemAppDataFolder", 0, 1},
  {"GetSystemAppDataKey", 1, 1},
  {"GetSystemLeapSecondInformation", 0, 1},
  {"GetSystemMetadataPath", 0, 1},
  {"GetSystemMetadataPathForPackage", 0, 1},
  {"GetSystemMetadataPathForPackageFamily", 0, 1},
  {"GetSystemStateRootFolder", 0, 1},
  {"GetTargetPlatformContext", 0, 1},
  {"GetUnicodeStringToEightBitSizeRoutine", 0, 1},
  {"GetUnicodeStringToEightBitStringRoutine", 0, 1},
  {"GetUserInfo", 0, 1},
  {"GetUserInfoWord", 0, 1},
  {"GetUserOverrideString", 0, 1},
  {"GetUserOverrideWord", 0, 1},
  {"GetVDMCurrentDirectories", 1, 0},
  {"IncrementPackageStatusVersion", 0, 1},
  {"InternalLcidToName", 0, 1},
  {"Internal_EnumCalendarInfo", 0, 1},
  {"Internal_EnumDateFormats", 0, 1},
  {"Internal_EnumLanguageGroupLocales", 0, 1},
  {"Internal_EnumSystemCodePages", 0, 1},
  {"Internal_EnumSystemLanguageGroups", 0, 1},
  {"Internal_EnumSystemLocales", 0, 1},
  {"Internal_EnumTimeFormats", 0, 1},
  {"Internal_EnumUILanguages", 0, 1},
};
#define NEX (sizeof ex / sizeof ex[0])
TEST(gap23_export_presence) {
    for (size_t i = 0; i < NEX; i++) {
        FARPROC a = t_proc_k32(ex[i].n), b = t_proc_kb(ex[i].n);
        if (ex[i].k32) { if (!a && t_is_wine()) t_note("wine lacks kernel32 %s", ex[i].n); else CHECKF(a != NULL, "kernel32 export %s", ex[i].n); }
        else if (a) t_note("%s also resolves via kernel32", ex[i].n);
        if (ex[i].kb) { if (!b && t_is_wine()) t_note("wine lacks kernelbase %s", ex[i].n); else CHECKF(b != NULL, "kernelbase export %s", ex[i].n); }
        else if (b) t_note("%s also resolves via kernelbase", ex[i].n);
    }
}
TEST(gap23_resolve_via_generic) {
    for (size_t i = 0; i < NEX; i++) { if (!t_proc(ex[i].n) && t_is_wine()) continue; CHECKF(t_proc(ex[i].n) != NULL, "t_proc %s", ex[i].n); }
}
TEST(gap23_resolve_stable) {
    for (size_t i = 0; i < NEX; i++) CHECKF(t_proc(ex[i].n) == t_proc(ex[i].n), "%s", ex[i].n);
}
TEST(gap23_module_ownership) {
    for (size_t i = 0; i < NEX; i++) {
        FARPROC p = t_proc(ex[i].n);
        if (!p) continue;
        HMODULE m = NULL;
        CHECKF(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &m) && m != NULL, "%s module", ex[i].n);
        if (m && !t_is_wine()) CHECKF(m == t_kernelbase() || m == t_kernel32() || m == t_ntdll(), "%s owner", ex[i].n);
    }
}
TEST(gap23_kernelbase_only_not_in_kernel32) {
    /* internal names listed kb-only are not exported by kernel32 on Win10 */
    for (size_t i = 0; i < NEX; i++)
        if (!ex[i].k32 && t_proc_k32(ex[i].n)) t_note("%s unexpectedly exported by kernel32", ex[i].n);
}
TEST(gap23_leap_second_info) {
    typedef BOOL (WINAPI *fn_t)(DWORD *, DWORD *);
    fn_t f = (fn_t)t_proc_kb("GetSystemLeapSecondInformation");
    REQUIRE(f);
    DWORD en = 0xCCCCCCCC, fl = 0xCCCCCCCC;
    SET_ERR(0);
    BOOL r = f(&en, &fl);
    if (!r) { t_note("leap second query failed, err %lu", GetLastError()); return; }
    CHECK(en <= 1);
    t_note("leap second enabled=%lu flags=0x%lx", en, fl);
    DWORD en2 = 0xCCCCCCCC, fl2 = 0xCCCCCCCC;
    CHECK(f(&en2, &fl2));
    CHECK(en2 == en);
}
TEST(gap23_internal_lcid_to_name) {
    typedef const WCHAR *(WINAPI *fn_t)(DWORD);
    fn_t f = (fn_t)t_proc_kb("InternalLcidToName");
    REQUIRE(f);
    if (t_is_wine()) { t_skip("wine differs"); return; }
    const WCHAR *n = f(0x0409);
    CHECK(n != NULL);
    if (n) { CHECK(!lstrcmpiW(n, L"en-US")); }
    /* repeated call yields identical record */
    CHECK(f(0x0409) == n);
    /* invalid locale id */
    CHECK(f(0xFFFFFFFF) == NULL);
}
TEST(gap23_unicode_to_eightbit_routines) {
    typedef void *(WINAPI *fn_t)(void);
    fn_t a = (fn_t)t_proc_kb("GetUnicodeStringToEightBitSizeRoutine");
    fn_t b = (fn_t)t_proc_kb("GetUnicodeStringToEightBitStringRoutine");
    REQUIRE(a); REQUIRE(b);
    /* both just return a global function pointer; stable across calls */
    CHECK(a() == a());
    CHECK(b() == b());
    t_note("size routine %p string routine %p", a(), b());
}
