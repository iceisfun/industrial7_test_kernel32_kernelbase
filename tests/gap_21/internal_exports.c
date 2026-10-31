#include "t.h"
static const struct { const char *n; int k32, kb; } ex[] = {
  {"CloseConsoleHandle", 1, 0},
  {"CloseProfileUserMapping", 1, 0},
  {"CloseState", 1, 1},
  {"CloseStateAtom", 0, 1},
  {"CloseStateChangeNotification", 0, 1},
  {"CloseStateContainer", 0, 1},
  {"CloseStateLock", 0, 1},
  {"CommitStateAtom", 0, 1},
  {"CompareCalendarDates", 1, 0},
  {"ConvertCalDateTimeToSystemTime", 1, 0},
  {"ConvertNLSDayOfWeekToWin32DayOfWeek", 1, 0},
  {"ConvertSystemTimeToCalDateTime", 1, 0},
  {"CopyLZFile", 1, 0},
  {"CopyMemoryNonTemporal", 0, 1},
  {"CreateAppContainerToken", 0, 1},
  {"CreatePseudoConsoleAsUser", 0, 1},
  {"CreateStateAtom", 0, 1},
  {"CreateStateChangeNotification", 0, 1},
  {"CreateStateContainer", 0, 1},
  {"CreateStateLock", 0, 1},
  {"CreateStateSubcontainer", 0, 1},
  {"DelayLoadFailureHook", 1, 1},
  {"DelayLoadFailureHookLookup", 0, 1},
  {"DeleteStateAtomValue", 0, 1},
  {"DeleteStateContainer", 0, 1},
  {"DeleteStateContainerValue", 0, 1},
  {"DosPathToSessionPathA", 1, 0},
  {"DosPathToSessionPathW", 1, 0},
  {"DuplicateConsoleHandle", 1, 0},
  {"DuplicateStateContainerHandle", 0, 1},
  {"EnumerateExtensionNames", 0, 1},
  {"EnumerateLocalComputerNamesA", 1, 0},
  {"EnumerateLocalComputerNamesW", 1, 0},
  {"EnumerateStateAtomValues", 0, 1},
  {"EnumerateStateContainerItems", 0, 1},
  {"ExitVDM", 1, 0},
  {"ExtensionProgIdExists", 0, 1},
  {"GetAlternatePackageRoots", 0, 1},
  {"GetAppContainerAce", 1, 1},
  {"GetAppDataFolder", 0, 1},
  {"GetAppModelVersion", 0, 1},
  {"GetCPFileNameFromRegistry", 0, 1},
  {"GetCPHashNode", 0, 1},
  {"GetCalendar", 0, 1},
  {"GetCalendarDateFormat", 1, 0},
  {"GetCalendarDateFormatEx", 1, 0},
  {"GetCalendarDaysInMonth", 1, 0},
  {"GetCalendarDifferenceInDays", 1, 0},
  {"GetCalendarMonthsInYear", 1, 0},
  {"GetCalendarSupportedDateRange", 1, 0},
};
#define NEX (sizeof ex / sizeof ex[0])
TEST(gap21_export_presence) {
    for (size_t i = 0; i < NEX; i++) {
        FARPROC a = t_proc_k32(ex[i].n), b = t_proc_kb(ex[i].n);
        if (ex[i].k32) { if (!a && t_is_wine()) t_note("wine lacks kernel32 %s", ex[i].n); else CHECKF(a != NULL, "kernel32 export %s", ex[i].n); }
        else if (a) t_note("%s also resolves via kernel32", ex[i].n);
        if (ex[i].kb) { if (!b && t_is_wine()) t_note("wine lacks kernelbase %s", ex[i].n); else CHECKF(b != NULL, "kernelbase export %s", ex[i].n); }
        else if (b) t_note("%s also resolves via kernelbase", ex[i].n);
        CHECKF(ex[i].k32 || ex[i].kb, "%s listed", ex[i].n);
    }
}
TEST(gap21_resolve_via_generic) {
    for (size_t i = 0; i < NEX; i++) { if (!t_proc(ex[i].n) && t_is_wine()) continue; CHECKF(t_proc(ex[i].n) != NULL, "t_proc %s", ex[i].n); }
}
TEST(gap21_resolve_stable) {
    for (size_t i = 0; i < NEX; i++) CHECKF(t_proc(ex[i].n) == t_proc(ex[i].n), "%s", ex[i].n);
}
TEST(gap21_forwarder_consistency) {
    /* an export present in both modules resolves to a function in one of them */
    for (size_t i = 0; i < NEX; i++) {
        FARPROC a = t_proc_k32(ex[i].n);
        if (!a) continue;
        HMODULE m = NULL;
        CHECKF(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)a, &m) && m != NULL, "%s module", ex[i].n);
    }
}
TEST(gap21_nls_dow_conversion) {
    int (WINAPI *f)(DWORD) = (int (WINAPI *)(DWORD))t_proc_k32("ConvertNLSDayOfWeekToWin32DayOfWeek");
    REQUIRE(f);
    /* NLS: 0=Monday..6=Sunday; Win32: 0=Sunday..6=Saturday */
    for (DWORD i = 0; i < 6; i++) EXPECT_EQ(f(i), (int)(i + 1));
    EXPECT_EQ(f(6), 0);
    SET_ERR(0);
    EXPECT_EQ(f(7), -1); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    EXPECT_EQ(f(0xFFFFFFFF), -1); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap21_close_profile_user_mapping) {
    BOOL (WINAPI *f)(void) = (BOOL (WINAPI *)(void))t_proc_k32("CloseProfileUserMapping");
    REQUIRE(f);
    if (t_is_wine()) { t_skip("wine stub differs (Windows returns TRUE)"); return; }
    CHECK(f() == TRUE);
    CHECK(f() == TRUE);
}
TEST(gap21_copymemory_nontemporal) {
    typedef void (WINAPI *fn_t)(void *, const void *, SIZE_T);
    fn_t f = (fn_t)t_proc_kb("CopyMemoryNonTemporal");
    REQUIRE(f);
    static const SIZE_T sizes[] = {0,1,2,7,8,9,15,16,17,31,63,64,77,78,79,80,127,128,255,256,1000,4096,4099};
    static unsigned char src[4200], dst[4200];
    for (size_t i = 0; i < sizeof src; i++) src[i] = (unsigned char)(i * 7 + 3);
    for (size_t s = 0; s < sizeof sizes / sizeof *sizes; s++)
        for (int off = 0; off < 3; off++) {
            memset(dst, 0xCC, sizeof dst);
            f(dst + off, src + off, sizes[s]);
            int ok = memcmp(dst + off, src + off, sizes[s]) == 0;
            for (SIZE_T j = 0; j < (SIZE_T)off; j++) ok &= dst[j] == 0xCC;
            ok &= dst[off + sizes[s]] == 0xCC;
            CHECKF(ok, "size %zu off %d", (size_t)sizes[s], off);
        }
}
TEST(gap21_getappmodelversion_null) {
    typedef LONG (WINAPI *fn_t)(ULONG64 *);
    fn_t f = (fn_t)t_proc_kb("GetAppModelVersion");
    REQUIRE(f);
    CHECK(f(NULL) == ERROR_INVALID_PARAMETER);
}
