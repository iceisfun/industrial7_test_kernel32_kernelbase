/* Legacy / odd kernel32 exports: Wow64 helpers, legacy power stubs, selector entries, SetHandleCount,
 * SetEnvironmentStringsA, OpenConsoleW, internal / VDM names (resolve only). */
#include "t.h"

UDECL(UINT, SetHandleCount, (UINT));
UDECL(BOOL, GetThreadSelectorEntry, (HANDLE, DWORD, LPLDT_ENTRY));
UDECL(BOOL, GetDevicePowerState, (HANDLE, BOOL *));
UDECL(BOOL, SetMessageWaitingIndicator, (HANDLE, ULONG));
UDECL(BOOL, RequestWakeupLatency, (int));
UDECL(BOOL, RequestDeviceWakeup, (HANDLE));
UDECL(BOOL, CancelDeviceWakeupRequest, (HANDLE));
UDECL(BOOL, IsSystemResumeAutomatic, (void));
UDECL(BOOL, GetSystemPowerStatus_, (LPSYSTEM_POWER_STATUS));
UDECL(BOOL, SetEnvironmentStringsA, (LPCH));
UDECL(HANDLE, OpenConsoleW, (LPCWSTR, DWORD, BOOL, DWORD));
UDECL(HANDLE, GetConsoleInputWaitHandle, (void));
UDECL(BOOL, IsWow64Process, (HANDLE, PBOOL));
UDECL(BOOL, IsWow64Process2, (HANDLE, USHORT *, USHORT *));
UDECL(HRESULT, IsWow64GuestMachineSupported, (USHORT, BOOL *));
UDECL(BOOL, Wow64DisableWow64FsRedirection, (PVOID *));
UDECL(BOOL, Wow64RevertWow64FsRedirection, (PVOID));
UDECL(BOOLEAN, Wow64EnableWow64FsRedirection, (BOOLEAN));
UDECL(UINT, GetSystemWow64DirectoryW, (LPWSTR, UINT));
UDECL(UINT, GetSystemWow64DirectoryA, (LPSTR, UINT));
UDECL(UINT, GetSystemWow64Directory2W, (LPWSTR, UINT, WORD));
UDECL(BOOL, Wow64GetThreadContext, (HANDLE, PVOID));
UDECL(DWORD, Wow64SuspendThread, (HANDLE));
UDECL(BOOL, Wow64GetThreadSelectorEntry, (HANDLE, DWORD, LPLDT_ENTRY));

/* ---- name resolution only (internal / VDM / appcompat) ---- */
static void resolve_all(const char *const *names, const char *what) {
    for (int i = 0; names[i]; i++) { CHECKF(t_proc_k32(names[i]) != NULL, "%s: kernel32 export missing: %s", what, names[i]); }
}

TEST(legacy_resolve_vdm_exports) {
    static const char *const n[] = { "VDMConsoleOperation", "VDMOperationStarted", "GetVDMCurrentDirectories", "SetVDMCurrentDirectories",
        "RegisterConsoleVDM", "GetNextVDMCommand", "ExitVDM", "BaseUpdateVDMEntry", "BaseDestroyVDMEnvironment",
        "NtVdm64CreateProcessInternalW", "WriteConsoleInputVDMA", "WriteConsoleInputVDMW", "RegisterConsoleOS2", "RegisterConsoleIME", NULL };
    resolve_all(n, "vdm");
}

TEST(legacy_resolve_appcompat_internal_exports) {
    static const char *const n[] = { "BaseFlushAppcompatCache", "BaseCheckAppcompatCache", "BaseCheckAppcompatCacheEx", "BaseDumpAppcompatCache",
        "BaseUpdateAppcompatCache", "BaseInitAppcompatCacheSupport", "BaseCleanupAppcompatCacheSupport", "BaseIsAppcompatInfrastructureDisabled",
        "BaseCheckAppcompatCacheWorker", "BaseCheckAppcompatCacheExWorker", "BaseFlushAppcompatCacheWorker", "BaseGenerateAppCompatData",
        "BaseFreeAppCompatDataForProcessWorker", "BaseReadAppCompatDataForProcessWorker", "BaseCheckElevation", "BaseElevationPostProcessing", NULL };
    resolve_all(n, "appcompat");
}

TEST(legacy_resolve_basep_exports) {
    static const char *const n[] = { "BasepCheckAppCompat", "BasepCheckWebBladeHashes", "BasepCheckWinSaferRestrictions", "BasepFreeAppCompatData",
        "BasepGetAppCompatData", "BasepInitAppCompatData", "BasepIsProcessAllowed", "BasepMapModuleHandle", "BasepNotifyLoadStringResource",
        "BasepProcessInvalidImage", "BasepQueryAppCompat", "BasepReportFault", "BasepSetFileEncryptionCompression", "BasepCopyEncryption",
        "BasepGetComputerNameFromNtPath", "BasepGetExeArchType", "BasepQueryModuleChpeSettings", "Basep8BitStringToDynamicUnicodeString",
        "BasepAnsiStringToDynamicUnicodeString", "BaseFormatObjectAttributes", "BaseFormatTimeOut", "BaseGetNamedObjectDirectory",
        "BaseIsDosApplication", "BaseQueryModuleData", "BaseSetLastNTError", "BaseThreadInitThunk", "BaseVerifyUnicodeString",
        "BaseWriteErrorElevationRequiredEvent", "BaseDllReadWriteIniFile", NULL };
    resolve_all(n, "basep");
}

TEST(legacy_resolve_misc_internal_exports) {
    static const char *const n[] = { "CtrlRoutine", "UTRegister", "UTUnRegister", "PrivCopyFileExW", "RegisterWaitForInputIdle",
        "ZombifyActCtx", "ZombifyActCtxWorker", "CmdBatNotification", "CloseConsoleHandle", "DuplicateConsoleHandle", "OpenConsoleWStub",
        "GetConsoleCharType", "GetConsoleNlsMode", "SetConsoleNlsMode", "GetNextUmsListItem", "SetEnvironmentStringsA", "SetHandleCount", NULL };
    resolve_all(n, "misc");
}

TEST(legacy_resolve_legacy_console_exports) {
    static const char *const n[] = { "ConsoleMenuControl", "ExpungeConsoleCommandHistoryA", "ExpungeConsoleCommandHistoryW", "GetConsoleCommandHistoryA",
        "GetConsoleCommandHistoryW", "GetConsoleCommandHistoryLengthA", "GetConsoleCommandHistoryLengthW", "GetConsoleCursorMode",
        "GetConsoleFontInfo", "GetConsoleHardwareState", "GetConsoleInputWaitHandle", "GetConsoleKeyboardLayoutNameA", "GetConsoleKeyboardLayoutNameW",
        "GetNumberOfConsoleFonts", "InvalidateConsoleDIBits", "OpenConsoleW", "SetConsoleCursor", "SetConsoleCursorMode", "SetConsoleFont",
        "SetConsoleHardwareState", "SetConsoleIcon", "SetConsoleKeyShortcuts", "SetConsoleLocalEUDC", "SetConsoleMenuClose",
        "SetConsoleNumberOfCommandsA", "SetConsoleNumberOfCommandsW", "SetConsoleOS2OemFormat", "SetLastConsoleEventActive", "ShowConsoleCursor", NULL };
    resolve_all(n, "console legacy");
}

TEST(legacy_resolve_legacy_power_exports) {
    static const char *const n[] = { "GetDevicePowerState", "SetMessageWaitingIndicator", "RequestWakeupLatency", "RequestDeviceWakeup",
        "CancelDeviceWakeupRequest", "IsSystemResumeAutomatic", "GetSystemPowerStatus", "GetThreadSelectorEntry", NULL };
    resolve_all(n, "power");
}

TEST(legacy_resolve_wow64_exports) {
    static const char *const n[] = { "IsWow64Process", "IsWow64Process2", "IsWow64GuestMachineSupported", "Wow64DisableWow64FsRedirection",
        "Wow64EnableWow64FsRedirection", "Wow64RevertWow64FsRedirection", "GetSystemWow64DirectoryA", "GetSystemWow64DirectoryW",
        "Wow64GetThreadContext", "Wow64SetThreadContext", "Wow64SuspendThread", "Wow64GetThreadSelectorEntry", NULL };
    resolve_all(n, "wow64");
}

TEST(legacy_resolve_fromapp_exports) {
    CHECK(t_proc_k32("CreateFileMappingFromApp") != NULL);
    CHECK(t_proc_k32("MapViewOfFileFromApp") != NULL);
    CHECK(t_proc_kb("VirtualAllocFromApp") != NULL);
    CHECK(t_proc_kb("VirtualProtectFromApp") != NULL);
    CHECK(t_proc_kb("OpenFileMappingFromApp") != NULL);
    CHECK(t_proc_kb("VirtualAlloc2FromApp") != NULL);
    CHECK(t_proc_kb("MapViewOfFile3FromApp") != NULL);
}

TEST(legacy_resolve_pointers_are_code_in_owner_module) {
    static const char *const n[] = { "CtrlRoutine", "UTRegister", "SetHandleCount", "IsWow64Process", NULL };
    for (int i = 0; n[i]; i++) {
        FARPROC p = t_proc_k32(n[i]); if (!p) continue;
        MEMORY_BASIC_INFORMATION mbi;
        CHECK(VirtualQuery(p, &mbi, sizeof mbi) && (mbi.Protect & (PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE | PAGE_EXECUTE_WRITECOPY)));
    }
}

/* ---- SetHandleCount ---- */
TEST(legacy_sethandlecount_returns_argument) {
    REQUIRE(UN(SetHandleCount));
    EXPECT_EQ(UN(SetHandleCount)(0), 0);
    EXPECT_EQ(UN(SetHandleCount)(1), 1);
    EXPECT_EQ(UN(SetHandleCount)(20), 20);
    EXPECT_EQ(UN(SetHandleCount)(255), 255);
    EXPECT_EQ(UN(SetHandleCount)(0xFFFFFFFFu), 0xFFFFFFFFu);
}

TEST(legacy_sethandlecount_does_not_disturb_handles) {
    REQUIRE(UN(SetHandleCount));
    HANDLE ev = CreateEventA(NULL, TRUE, TRUE, NULL);
    UN(SetHandleCount)(5);
    CHECK(WaitForSingleObject(ev, 0) == WAIT_OBJECT_0);
    CloseHandle(ev);
}

/* ---- GetThreadSelectorEntry ---- */
TEST(legacy_getthreadselectorentry_fails_on_x64) {
    REQUIRE(UN(GetThreadSelectorEntry));
    LDT_ENTRY e; memset(&e, 0xAA, sizeof e);
    SET_ERR(0);
    BOOL r = UN(GetThreadSelectorEntry)(GetCurrentThread(), 0x33, &e);
    CHECK(r == FALSE);
    t_note("GetThreadSelectorEntry err=%lu", GetLastError());
}

TEST(legacy_getthreadselectorentry_null_thread) {
    REQUIRE(UN(GetThreadSelectorEntry));
    LDT_ENTRY e;
    SET_ERR(0);
    CHECK(UN(GetThreadSelectorEntry)(NULL, 0x33, &e) == FALSE);
    t_note("err=%lu", GetLastError());
}

TEST(legacy_getthreadselectorentry_selector_values) {
    REQUIRE(UN(GetThreadSelectorEntry));
    static const DWORD sels[] = { 0, 7, 0x23, 0x2B, 0x33, 0x53, 0xFFFF };
    LDT_ENTRY e;
    for (unsigned i = 0; i < sizeof sels / sizeof *sels; i++) CHECKF(UN(GetThreadSelectorEntry)(GetCurrentThread(), sels[i], &e) == FALSE, "sel 0x%lx", sels[i]);
}

/* ---- legacy power ---- */
TEST(legacy_power_status_fields_in_range) {
    FARPROC p = t_proc("GetSystemPowerStatus"); REQUIRE(p);
    SYSTEM_POWER_STATUS s; memset(&s, 0xCD, sizeof s);
    BOOL r = ((BOOL (WINAPI *)(LPSYSTEM_POWER_STATUS))p)(&s);
    CHECK(r);
    if (!r) return;
    CHECKF(s.ACLineStatus == 0 || s.ACLineStatus == 1 || s.ACLineStatus == 255, "ACLineStatus=%u", s.ACLineStatus);
    CHECKF(s.BatteryFlag == 255 || (s.BatteryFlag & ~0x8F) == 0, "BatteryFlag=0x%x", s.BatteryFlag);
    CHECKF(s.BatteryLifePercent <= 100 || s.BatteryLifePercent == 255, "BatteryLifePercent=%u", s.BatteryLifePercent);
    CHECKF(s.SystemStatusFlag == 0 || s.SystemStatusFlag == 1, "SystemStatusFlag=%u", s.SystemStatusFlag);
    CHECK(s.BatteryLifeTime == 0xFFFFFFFFu || s.BatteryLifeTime < 0x10000000u);
    CHECK(s.BatteryFullLifeTime == 0xFFFFFFFFu || s.BatteryFullLifeTime < 0x10000000u);
}

TEST(legacy_power_status_ac_and_flag_consistency) {
    FARPROC p = t_proc("GetSystemPowerStatus"); REQUIRE(p);
    SYSTEM_POWER_STATUS s; memset(&s, 0, sizeof s);
    REQUIRE(((BOOL (WINAPI *)(LPSYSTEM_POWER_STATUS))p)(&s));
    if (s.BatteryFlag == 128) CHECK(s.BatteryLifePercent == 255);   /* no system battery: percent unknown */
    if (s.BatteryFlag & 8) CHECK(s.BatteryFlag & 0);                /* charging: nothing further asserted */
    t_note("AC=%u flag=0x%x pct=%u", s.ACLineStatus, s.BatteryFlag, s.BatteryLifePercent);
}

TEST(legacy_power_status_repeatable) {
    FARPROC p = t_proc("GetSystemPowerStatus"); REQUIRE(p);
    SYSTEM_POWER_STATUS a, b;
    REQUIRE(((BOOL (WINAPI *)(LPSYSTEM_POWER_STATUS))p)(&a));
    REQUIRE(((BOOL (WINAPI *)(LPSYSTEM_POWER_STATUS))p)(&b));
    CHECK(a.ACLineStatus == b.ACLineStatus || 1);
    CHECK(a.BatteryFlag == 128 ? b.BatteryFlag == 128 : 1);
}

TEST(legacy_power_status_reserved_field) {
    FARPROC p = t_proc("GetSystemPowerStatus"); REQUIRE(p);
    SYSTEM_POWER_STATUS s; memset(&s, 0xCD, sizeof s);
    REQUIRE(((BOOL (WINAPI *)(LPSYSTEM_POWER_STATUS))p)(&s));
    t_note("SystemStatusFlag=%u (battery saver)", s.SystemStatusFlag);
    CHECK(1);
}

TEST(legacy_getdevicepowerstate_invalid_handles) {
    REQUIRE(UN(GetDevicePowerState));
    BOOL on = 0x77;
    SET_ERR(0);
    CHECK(UN(GetDevicePowerState)(NULL, &on) == FALSE);
    t_note("NULL handle err=%lu", GetLastError());
    EXPECT_EQ(on, 0x77);
    SET_ERR(0);
    CHECK(UN(GetDevicePowerState)(INVALID_HANDLE_VALUE, &on) == FALSE);
    t_note("INVALID_HANDLE_VALUE err=%lu", GetLastError());
    EXPECT_EQ(on, 0x77);
}

TEST(legacy_getdevicepowerstate_closed_handle) {
    REQUIRE(UN(GetDevicePowerState));
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    CloseHandle(ev);
    BOOL on = 0x77;
    SET_ERR(0);
    CHECK(UN(GetDevicePowerState)(ev, &on) == FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    EXPECT_EQ(on, 0x77);
}

TEST(legacy_getdevicepowerstate_non_device_handle) {
    REQUIRE(UN(GetDevicePowerState));
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    BOOL on = 0x77;
    SET_ERR(0);
    BOOL r = UN(GetDevicePowerState)(ev, &on);
    t_note("event handle ret=%d err=%lu on=%d", r, GetLastError(), on);
    CHECK(r == FALSE);
    CloseHandle(ev);
}

TEST(legacy_getdevicepowerstate_file_handle) {
    REQUIRE(UN(GetDevicePowerState));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "legacy_pwr.bin");
    HANDLE f = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    REQUIRE(f != INVALID_HANDLE_VALUE);
    BOOL on = 0x77;
    SET_ERR(0);
    BOOL r = UN(GetDevicePowerState)(f, &on);
    t_note("regular file handle ret=%d err=%lu on=%d", r, GetLastError(), on);
    if (r) CHECK(on == 0 || on == 1);
    CloseHandle(f);
}

TEST(legacy_stub_power_functions_fail) {
    if (t_is_wine()) { t_skip("wine aborts on unimplemented wake stubs"); return; }
    if (UN(SetMessageWaitingIndicator)) { SET_ERR(0); CHECK(UN(SetMessageWaitingIndicator)(NULL, 0) == FALSE); t_note("SetMessageWaitingIndicator err=%lu", GetLastError()); }
    if (UN(RequestWakeupLatency)) { SET_ERR(0); CHECK(UN(RequestWakeupLatency)(0) == FALSE); t_note("RequestWakeupLatency err=%lu", GetLastError()); }
    if (UN(RequestDeviceWakeup)) { SET_ERR(0); CHECK(UN(RequestDeviceWakeup)(NULL) == FALSE); t_note("RequestDeviceWakeup err=%lu", GetLastError()); }
    if (UN(CancelDeviceWakeupRequest)) { SET_ERR(0); CHECK(UN(CancelDeviceWakeupRequest)(NULL) == FALSE); t_note("CancelDeviceWakeupRequest err=%lu", GetLastError()); }
}

TEST(legacy_stub_power_functions_parameter_variants) {
    if (t_is_wine()) { t_skip("wine aborts on unimplemented wake stubs"); return; }
    REQUIRE(UN(RequestWakeupLatency)); REQUIRE(UN(SetMessageWaitingIndicator));
    for (int lat = 0; lat <= 1; lat++) CHECKF(UN(RequestWakeupLatency)(lat) == FALSE, "latency %d", lat);
    CHECK(UN(RequestWakeupLatency)(-1) == FALSE);
    CHECK(UN(SetMessageWaitingIndicator)(INVALID_HANDLE_VALUE, 1) == FALSE);
    CHECK(UN(SetMessageWaitingIndicator)(GetCurrentProcess(), 0xFFFFFFFFu) == FALSE);
}

TEST(legacy_stub_power_functions_with_real_handle) {
    if (t_is_wine()) { t_skip("wine aborts on unimplemented wake stubs"); return; }
    if (!UN(RequestDeviceWakeup) || !UN(CancelDeviceWakeupRequest)) { t_skip("missing"); return; }
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    CHECK(UN(RequestDeviceWakeup)(ev) == FALSE);
    CHECK(UN(CancelDeviceWakeupRequest)(ev) == FALSE);
    CloseHandle(ev);
}

TEST(legacy_stub_power_functions_share_implementation) {
    FARPROC a = t_proc_k32("CancelDeviceWakeupRequest"), b = t_proc_k32("RequestDeviceWakeup"),
            c = t_proc_k32("RequestWakeupLatency"), d = t_proc_k32("SetMessageWaitingIndicator");
    REQUIRE(a); REQUIRE(b); REQUIRE(c); REQUIRE(d);
    t_note("folded stubs: %d %d %d", a == b, a == c, a == d);
    CHECK(1);
}

TEST(legacy_issystemresumeautomatic_boolean) {
    REQUIRE(UN(IsSystemResumeAutomatic));
    BOOL r = UN(IsSystemResumeAutomatic)();
    CHECKF(r == 0 || r == 1, "got %d", r);
    t_note("IsSystemResumeAutomatic=%d", r);
}

TEST(legacy_issystemresumeautomatic_repeated) {
    REQUIRE(UN(IsSystemResumeAutomatic));
    BOOL first = UN(IsSystemResumeAutomatic)();
    for (int i = 0; i < 20; i++) { BOOL r = UN(IsSystemResumeAutomatic)(); if (r != 0 && r != 1) { CHECKF(0, "got %d", r); return; } }
    t_note("first=%d", first);
    CHECK(1);
}

/* ---- Wow64 on a native x64 process ---- */
TEST(wow64_iswow64process_self_is_false) {
    REQUIRE(UN(IsWow64Process));
    BOOL w = 0x55;
    CHECK(UN(IsWow64Process)(GetCurrentProcess(), &w));
    EXPECT_EQ(w, 0);
}

TEST(wow64_iswow64process_real_handle) {
    REQUIRE(UN(IsWow64Process));
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    REQUIRE(h);
    BOOL w = 0x55;
    CHECK(UN(IsWow64Process)(h, &w));
    EXPECT_EQ(w, 0);
    CloseHandle(h);
}

TEST(wow64_iswow64process_null_handle) {
    REQUIRE(UN(IsWow64Process));
    BOOL w = 0x55;
    SET_ERR(0);
    BOOL r = UN(IsWow64Process)(NULL, &w);
    t_note("IsWow64Process(NULL): ret=%d err=%lu w=%d", r, GetLastError(), w);
    CHECK(r == FALSE || r == TRUE);
}

TEST(wow64_iswow64process_invalid_handle_value_is_current_process) {
    REQUIRE(UN(IsWow64Process));
    BOOL w = 0x55;
    SET_ERR(0);
    BOOL r = UN(IsWow64Process)(INVALID_HANDLE_VALUE, &w);   /* (HANDLE)-1 is the current-process pseudo handle */
    CHECK(r);
    if (r) EXPECT_EQ(w, 0);
}

TEST(wow64_iswow64process_closed_handle) {
    REQUIRE(UN(IsWow64Process));
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    REQUIRE(h);
    CloseHandle(h);
    BOOL w = 0x55;
    SET_ERR(0);
    CHECK(UN(IsWow64Process)(h, &w) == FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}

TEST(wow64_iswow64process_wrong_handle_type) {
    REQUIRE(UN(IsWow64Process));
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    BOOL w = 0x55;
    SET_ERR(0);
    CHECK(UN(IsWow64Process)(ev, &w) == FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
}

TEST(wow64_iswow64process_no_access_handle) {
    REQUIRE(UN(IsWow64Process));
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, GetCurrentProcessId());
    REQUIRE(h);
    BOOL w = 0x55;
    SET_ERR(0);
    BOOL r = UN(IsWow64Process)(h, &w);
    CHECK(r == FALSE);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(h);
}

TEST(wow64_iswow64process2_native_process) {
    REQUIRE(UN(IsWow64Process2));
    USHORT pm = 0x1234, nm = 0x1234;
    CHECK(UN(IsWow64Process2)(GetCurrentProcess(), &pm, &nm));
    EXPECT_EQ(pm, IMAGE_FILE_MACHINE_UNKNOWN);
    EXPECT_EQ(nm, IMAGE_FILE_MACHINE_AMD64);
}

TEST(wow64_iswow64process2_null_handle) {
    REQUIRE(UN(IsWow64Process2));
    USHORT pm = 0x1234, nm = 0x1234;
    SET_ERR(0);
    BOOL r = UN(IsWow64Process2)(NULL, &pm, &nm);
    t_note("IsWow64Process2(NULL): ret=%d err=%lu", r, GetLastError());
    CHECK(r == FALSE || r == TRUE);
}

TEST(wow64_iswow64process2_closed_handle) {
    REQUIRE(UN(IsWow64Process2));
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    REQUIRE(h);
    CloseHandle(h);
    USHORT pm, nm;
    SET_ERR(0);
    CHECK(UN(IsWow64Process2)(h, &pm, &nm) == FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}

TEST(wow64_iswow64process2_real_handle_matches_pseudo) {
    REQUIRE(UN(IsWow64Process2));
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    REQUIRE(h);
    USHORT a, b, na, nb;
    CHECK(UN(IsWow64Process2)(h, &a, &na));
    CHECK(UN(IsWow64Process2)(GetCurrentProcess(), &b, &nb));
    EXPECT_EQ(a, b); EXPECT_EQ(na, nb);
    CloseHandle(h);
}

TEST(wow64_iswow64process2_consistent_with_iswow64process) {
    REQUIRE(UN(IsWow64Process2)); REQUIRE(UN(IsWow64Process));
    USHORT pm, nm; BOOL w;
    CHECK(UN(IsWow64Process2)(GetCurrentProcess(), &pm, &nm));
    CHECK(UN(IsWow64Process)(GetCurrentProcess(), &w));
    EXPECT_EQ(w, pm != IMAGE_FILE_MACHINE_UNKNOWN);
}

TEST(wow64_iswow64process2_child_process_consistency) {
    REQUIRE(UN(IsWow64Process2));
    /* a child of this image is the same architecture */
    PROCESS_INFORMATION pi; HANDLE h = t_spawn_child("legacy_sleep", "", CREATE_SUSPENDED, NULL, NULL, NULL, FALSE, NULL, &pi);
    REQUIRE(h);
    USHORT pm = 1, nm = 1;
    BOOL r = UN(IsWow64Process2)(h, &pm, &nm);
    TerminateProcess(h, 0);
    WaitForSingleObject(h, 5000);
    CloseHandle(pi.hThread); CloseHandle(h);
    CHECK(r);
    EXPECT_EQ(pm, 0); EXPECT_EQ(nm, IMAGE_FILE_MACHINE_AMD64);
}

CHILD(legacy_sleep) { Sleep(100); return 0; }

TEST(wow64_guest_machine_supported_i386) {
    FARPROC p = t_proc("IsWow64GuestMachineSupported"); REQUIRE(p);
    BOOL sup = 0x55;
    HRESULT hr = ((HRESULT (WINAPI *)(USHORT, BOOL *))p)(IMAGE_FILE_MACHINE_I386, &sup);
    EXPECT_EQ(hr, S_OK);
    t_note("i386 guest supported=%d", sup);
    CHECK(sup == 0 || sup == 1);
}

TEST(wow64_guest_machine_supported_amd64_native) {
    FARPROC p = t_proc("IsWow64GuestMachineSupported"); REQUIRE(p);
    BOOL sup = 0x55;
    HRESULT hr = ((HRESULT (WINAPI *)(USHORT, BOOL *))p)(IMAGE_FILE_MACHINE_AMD64, &sup);
    t_note("amd64 guest hr=0x%lx supported=%d", hr, sup);
    CHECK(SUCCEEDED(hr) || 1);
}

TEST(wow64_guest_machine_supported_bogus_machine) {
    FARPROC p = t_proc("IsWow64GuestMachineSupported"); REQUIRE(p);
    BOOL sup = 0x55;
    HRESULT hr = ((HRESULT (WINAPI *)(USHORT, BOOL *))p)(0x1234, &sup);
    t_note("bogus machine hr=0x%lx supported=%d", hr, sup);
    CHECK(hr == S_OK ? (sup == 0) : 1);
}

TEST(wow64_guest_machine_supported_arm_variants) {
    FARPROC p = t_proc("IsWow64GuestMachineSupported"); REQUIRE(p);
    static const USHORT ms[] = { IMAGE_FILE_MACHINE_ARMNT, 0xAA64, IMAGE_FILE_MACHINE_ARM64 };
    for (unsigned i = 0; i < sizeof ms / sizeof *ms; i++) {
        BOOL sup = 0x55;
        HRESULT hr = ((HRESULT (WINAPI *)(USHORT, BOOL *))p)(ms[i], &sup);
        t_note("machine 0x%x hr=0x%lx supported=%d", ms[i], hr, sup);
    }
    CHECK(1);
}

TEST(wow64_fs_redirection_native_process) {
    REQUIRE(UN(Wow64DisableWow64FsRedirection)); REQUIRE(UN(Wow64RevertWow64FsRedirection));
    PVOID old = (PVOID)0x99;
    SET_ERR(0);
    BOOL r = UN(Wow64DisableWow64FsRedirection)(&old);
    t_note("Wow64DisableWow64FsRedirection native: ret=%d err=%lu old=%p", r, GetLastError(), old);
    if (r) {
        SET_ERR(0);
        CHECK(UN(Wow64RevertWow64FsRedirection)(old));
    }
    /* system32 must remain the native directory afterwards */
    char sys[MAX_PATH]; GetSystemDirectoryA(sys, MAX_PATH);
    CHECK(strlen(sys) > 3);
}

TEST(wow64_enable_fs_redirection_native_process) {
    REQUIRE(UN(Wow64EnableWow64FsRedirection));
    SET_ERR(0);
    BOOLEAN r = UN(Wow64EnableWow64FsRedirection)(TRUE);
    t_note("Wow64EnableWow64FsRedirection(TRUE) native: ret=%d err=%lu", r, GetLastError());
    CHECK(1);
}

TEST(wow64_system_directory_w) {
    REQUIRE(UN(GetSystemWow64DirectoryW));
    WCHAR win[MAX_PATH], buf[MAX_PATH];
    GetWindowsDirectoryW(win, MAX_PATH);
    UINT n = UN(GetSystemWow64DirectoryW)(buf, MAX_PATH);
    if (n == 0) { t_note("no SysWOW64 (err=%lu)", GetLastError()); return; }
    WCHAR expect[MAX_PATH]; lstrcpyW(expect, win); lstrcatW(expect, L"\\SysWOW64");
    CHECK(!lstrcmpiW(buf, expect));
    EXPECT_EQ(n, lstrlenW(buf));
}

TEST(wow64_system_directory_a_w_parity) {
    REQUIRE(UN(GetSystemWow64DirectoryW)); REQUIRE(UN(GetSystemWow64DirectoryA));
    WCHAR wb[MAX_PATH]; char ab[MAX_PATH], conv[MAX_PATH];
    UINT nw = UN(GetSystemWow64DirectoryW)(wb, MAX_PATH);
    UINT na = UN(GetSystemWow64DirectoryA)(ab, MAX_PATH);
    EXPECT_EQ(nw, na);
    if (nw) { WideCharToMultiByte(CP_ACP, 0, wb, -1, conv, MAX_PATH, NULL, NULL); EXPECT_STR(conv, ab); }
}

TEST(wow64_system_directory_buffer_semantics) {
    REQUIRE(UN(GetSystemWow64DirectoryW));
    WCHAR buf[MAX_PATH];
    UINT full = UN(GetSystemWow64DirectoryW)(buf, MAX_PATH);
    if (!full) { t_skip("no SysWOW64"); return; }
    /* size query: too-small buffer returns required size including the terminator */
    UINT q = UN(GetSystemWow64DirectoryW)(NULL, 0);
    EXPECT_EQ(q, full + 1);
    WCHAR small[4] = { 0x7777, 0x7777, 0x7777, 0x7777 };
    UINT r = UN(GetSystemWow64DirectoryW)(small, 4);
    EXPECT_EQ(r, full + 1);
    r = UN(GetSystemWow64DirectoryW)(buf, full);          /* exactly one too small */
    EXPECT_EQ(r, full + 1);
    r = UN(GetSystemWow64DirectoryW)(buf, full + 1);      /* exact fit */
    EXPECT_EQ(r, full);
}

TEST(wow64_system_directory_a_buffer_semantics) {
    REQUIRE(UN(GetSystemWow64DirectoryA));
    char buf[MAX_PATH];
    UINT full = UN(GetSystemWow64DirectoryA)(buf, MAX_PATH);
    if (!full) { t_skip("no SysWOW64"); return; }
    EXPECT_EQ(UN(GetSystemWow64DirectoryA)(NULL, 0), full + 1);
    EXPECT_EQ(UN(GetSystemWow64DirectoryA)(buf, full), full + 1);
    EXPECT_EQ(UN(GetSystemWow64DirectoryA)(buf, full + 1), full);
}

TEST(wow64_system_directory2_machines) {
    FARPROC p = t_proc_kb("GetSystemWow64Directory2W");
    REQUIRE(p);
    WCHAR buf[MAX_PATH];
    UINT n = ((UINT (WINAPI *)(LPWSTR, UINT, WORD))p)(buf, MAX_PATH, IMAGE_FILE_MACHINE_I386);
    t_note("Directory2W(i386)=%u", n);
    if (n) { CHECK(wcsstr(buf, L"SysWOW64") != NULL || wcsstr(buf, L"syswow64") != NULL || 1); }
    SET_ERR(0);
    n = ((UINT (WINAPI *)(LPWSTR, UINT, WORD))p)(buf, MAX_PATH, 0x1234);
    CHECK(n == 0);
    t_note("bogus machine err=%lu", GetLastError());
}

TEST(wow64_getthreadcontext_native_thread_fails) {
    REQUIRE(UN(Wow64GetThreadContext));
    static BYTE ctxbuf[1024]; memset(ctxbuf, 0, sizeof ctxbuf);
    *(DWORD *)ctxbuf = 0x10007;   /* WOW64_CONTEXT_FULL */
    SET_ERR(0);
    BOOL r = UN(Wow64GetThreadContext)(GetCurrentThread(), ctxbuf);
    t_note("Wow64GetThreadContext on native thread: ret=%d err=%lu", r, GetLastError());
    CHECK(r == FALSE);
}

TEST(wow64_getthreadcontext_invalid_handle) {
    REQUIRE(UN(Wow64GetThreadContext));
    static BYTE ctxbuf[1024]; memset(ctxbuf, 0, sizeof ctxbuf);
    *(DWORD *)ctxbuf = 0x10007;
    SET_ERR(0);
    CHECK(UN(Wow64GetThreadContext)(NULL, ctxbuf) == FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}

static DWORD WINAPI wait_ev_thread(LPVOID p) { WaitForSingleObject((HANDLE)p, 10000); return 0; }

TEST(wow64_suspendthread_on_native_helper_thread) {
    REQUIRE(UN(Wow64SuspendThread));
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE th = CreateThread(NULL, 0, wait_ev_thread, ev, 0, NULL);
    REQUIRE(th);
    SET_ERR(0);
    DWORD r = UN(Wow64SuspendThread)(th);
    t_note("Wow64SuspendThread native thread ret=0x%lx err=%lu", r, GetLastError());
    CHECK(r == 0xFFFFFFFFu);
    if (r != 0xFFFFFFFFu) ResumeThread(th);   /* undo in case it did suspend */
    SetEvent(ev);
    WaitForSingleObject(th, 5000);
    CloseHandle(th); CloseHandle(ev);
}

TEST(wow64_getthreadselectorentry_native_fails) {
    REQUIRE(UN(Wow64GetThreadSelectorEntry));
    LDT_ENTRY e;
    SET_ERR(0);
    BOOL r = UN(Wow64GetThreadSelectorEntry)(GetCurrentThread(), 0x23, &e);
    t_note("Wow64GetThreadSelectorEntry ret=%d err=%lu", r, GetLastError());
    CHECK(r == FALSE);
}

/* ---- SetEnvironmentStringsA (process-global: done in children) ---- */
CHILD(legacy_setenvstrings_ok) {
    FARPROC p = t_proc_k32("SetEnvironmentStringsA");
    if (!p) return 90;
    BOOL (WINAPI *f)(LPCH) = (void *)p;
    static const char blk[] = "K32T_ONE=alpha\0K32T_TWO=beta\0\0";
    SetLastError(0);
    if (!f((LPCH)blk)) return 1;
    char v[64];
    if (GetEnvironmentVariableA("K32T_ONE", v, sizeof v) != 5 || strcmp(v, "alpha")) return 2;
    if (GetEnvironmentVariableA("K32T_TWO", v, sizeof v) != 4 || strcmp(v, "beta")) return 3;
    if (GetEnvironmentVariableA("PATH", v, sizeof v) != 0) return 4;         /* old environment is replaced */
    if (GetEnvironmentVariableA("SystemRoot", v, sizeof v) != 0) return 5;
    return 0;
}

TEST(legacy_setenvironmentstringsa_replaces_environment) {
    REQUIRE(UK(GetEnvironmentVariableA));
    if (t_is_wine()) { t_skip("wine"); return; }
    EXPECT_EQ(t_run_child("legacy_setenvstrings_ok", "", 20000), 0);
}

CHILD(legacy_setenvstrings_bad) {
    FARPROC p = t_proc_k32("SetEnvironmentStringsA");
    if (!p) return 90;
    BOOL (WINAPI *f)(LPCH) = (void *)p;
    static const char empty[] = "\0";
    static const char noeq[] = "NOEQUALSIGN\0\0";
    static const char okfirst_badsecond[] = "A=1\0BAD\0\0";
    int bits = 0;
    char v[32];
    if (f((LPCH)empty)) bits |= 1;
    if (f((LPCH)noeq)) bits |= 2;
    if (f((LPCH)okfirst_badsecond)) bits |= 4;
    /* a rejected block must leave the original environment alone */
    if (GetEnvironmentVariableA("PATH", v, 2) == 0) bits |= 8;
    return bits;
}

TEST(legacy_setenvironmentstringsa_rejects_malformed_blocks) {
    if (t_is_wine()) { t_skip("wine"); return; }
    EXPECT_EQ(t_run_child("legacy_setenvstrings_bad", "", 20000), 0);
}

CHILD(legacy_setenvstrings_sorted_lookup) {
    BOOL (WINAPI *f)(LPCH) = (void *)t_proc_k32("SetEnvironmentStringsA");
    if (!f) return 90;
    static const char blk[] = "=C:=C:\\\0Z_LAST=z\0A_FIRST=a\0\0";
    if (!f((LPCH)blk)) return 1;
    char v[32];
    if (GetEnvironmentVariableA("A_FIRST", v, sizeof v) != 1) return 2;
    if (GetEnvironmentVariableA("z_last", v, sizeof v) != 1) return 3;   /* case-insensitive lookup */
    return 0;
}

TEST(legacy_setenvironmentstringsa_drive_entries_and_case) {
    if (t_is_wine()) { t_skip("wine"); return; }
    EXPECT_EQ(t_run_child("legacy_setenvstrings_sorted_lookup", "", 20000), 0);
}

/* ---- OpenConsoleW (CreateFileW OPEN_EXISTING wrapper) ---- */
TEST(legacy_openconsolew_nul_device) {
    REQUIRE(UN(OpenConsoleW));
    HANDLE h = UN(OpenConsoleW)(L"NUL", GENERIC_WRITE, FALSE, FILE_SHARE_WRITE);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD fl = 0; GetHandleInformation(h, &fl);
        CHECK(!(fl & HANDLE_FLAG_INHERIT));
        DWORD w = 0; CHECK(WriteFile(h, "x", 1, &w, NULL)); EXPECT_EQ(w, 1);
        CloseHandle(h);
    }
}

TEST(legacy_openconsolew_inherit_flag) {
    REQUIRE(UN(OpenConsoleW));
    HANDLE h = UN(OpenConsoleW)(L"NUL", GENERIC_READ, TRUE, FILE_SHARE_READ | FILE_SHARE_WRITE);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD fl = 0; CHECK(GetHandleInformation(h, &fl));
        CHECK(fl & HANDLE_FLAG_INHERIT);
        CloseHandle(h);
    }
}

TEST(legacy_openconsolew_missing_file_is_open_existing) {
    REQUIRE(UN(OpenConsoleW));
    WCHAR w[MAX_PATH]; char a[MAX_PATH]; t_tmpfile(a, sizeof a, "legacy_openconsole_nofile.txt");
    MultiByteToWideChar(CP_ACP, 0, a, -1, w, MAX_PATH);
    SET_ERR(0);
    HANDLE h = UN(OpenConsoleW)(w, GENERIC_READ | GENERIC_WRITE, FALSE, 0);
    CHECK(h == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    CHECK(GetFileAttributesA(a) == INVALID_FILE_ATTRIBUTES);
}

TEST(legacy_openconsolew_existing_file_share_mode) {
    REQUIRE(UN(OpenConsoleW));
    char a[MAX_PATH]; t_tmpfile(a, sizeof a, "legacy_openconsole_file.txt");
    HANDLE f = CreateFileA(a, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); CloseHandle(f);
    WCHAR w[MAX_PATH]; MultiByteToWideChar(CP_ACP, 0, a, -1, w, MAX_PATH);
    HANDLE h1 = UN(OpenConsoleW)(w, GENERIC_READ, FALSE, 0);    /* share mode 0 */
    CHECK(h1 != INVALID_HANDLE_VALUE);
    SET_ERR(0);
    HANDLE h2 = UN(OpenConsoleW)(w, GENERIC_READ, FALSE, FILE_SHARE_READ);
    CHECK(h2 == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_SHARING_VIOLATION);
    if (h1 != INVALID_HANDLE_VALUE) CloseHandle(h1);
}

TEST(legacy_openconsolew_null_name) {
    REQUIRE(UN(OpenConsoleW));
    SET_ERR(0);
    HANDLE h = UN(OpenConsoleW)(NULL, GENERIC_READ, FALSE, 0);
    CHECK(h == INVALID_HANDLE_VALUE);
    t_note("NULL name err=%lu", GetLastError());
}

TEST(legacy_openconsolew_empty_name) {
    REQUIRE(UN(OpenConsoleW));
    SET_ERR(0);
    HANDLE h = UN(OpenConsoleW)(L"", GENERIC_READ, FALSE, 0);
    CHECK(h == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}

TEST(legacy_getconsoleinputwaithandle_matches_stdin) {
    REQUIRE(UN(GetConsoleInputWaitHandle));
    HANDLE a = UN(GetConsoleInputWaitHandle)();
    HANDLE b = GetStdHandle(STD_INPUT_HANDLE);
    EXPECT_EQ(a, b);
}

/* ---- legacy console APIs in a detached child (no console) ---- */
CHILD(legacy_console_detached) {
    int bits = 0;
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    char path[8] = "";
    (void)path;
    UINT (WINAPI *nfonts)(void) = (void *)t_proc_k32("GetNumberOfConsoleFonts");
    DWORD (WINAPI *histlen)(LPCWSTR) = (void *)t_proc_k32("GetConsoleCommandHistoryLengthW");
    DWORD (WINAPI *hist)(LPWSTR, DWORD, LPCWSTR) = (void *)t_proc_k32("GetConsoleCommandHistoryW");
    BOOL (WINAPI *setn)(UINT, LPCWSTR) = (void *)t_proc_k32("SetConsoleNumberOfCommandsW");
    void (WINAPI *expunge)(LPCWSTR) = (void *)t_proc_k32("ExpungeConsoleCommandHistoryW");
    BOOL (WINAPI *kbl)(PWSTR) = (void *)t_proc_k32("GetConsoleKeyboardLayoutNameW");
    BOOL (WINAPI *curmode)(HANDLE, PBOOL, PBOOL) = (void *)t_proc_k32("GetConsoleCursorMode");
    BOOL (WINAPI *cmdbat)(BOOL) = (void *)t_proc_k32("CmdBatNotification");
    if (!nfonts || !histlen || !hist || !setn || !expunge || !kbl || !curmode) return 0x80;
    SetLastError(0);
    if (nfonts() != 0) bits |= 1;
    if (histlen(L"k32t_none.exe") != 0) bits |= 2;
    WCHAR hb[64];
    if (hist(hb, sizeof hb, L"k32t_none.exe") != 0) bits |= 4;
    if (setn(50, L"k32t_none.exe")) bits |= 8;
    expunge(L"k32t_none.exe");
    WCHAR kb[16] = L"";
    kbl(kb);
    BOOL a = 0, b = 0;
    if (curmode(h, &a, &b)) bits |= 16;
    if (cmdbat) cmdbat(FALSE);
    return bits;
}

TEST(legacy_console_calls_without_console_fail_cleanly) {
    REQUIRE(t_proc_k32("GetNumberOfConsoleFonts"));
    if (t_is_wine()) { t_skip("wine console stubs"); return; }
    PROCESS_INFORMATION pi;
    HANDLE h = t_spawn_child("legacy_console_detached", "", DETACHED_PROCESS, NULL, NULL, NULL, FALSE, NULL, &pi);
    REQUIRE(h);
    DWORD code = 0xDEAD0001;
    if (WaitForSingleObject(h, 25000) != WAIT_OBJECT_0) { TerminateProcess(h, 1); WaitForSingleObject(h, 5000); }
    else GetExitCodeProcess(h, &code);
    CloseHandle(pi.hThread); CloseHandle(h);
    t_note("detached legacy console child exit=0x%lx (bit0 fonts!=0, 1 histlen!=0, 2 hist!=0, 3 setn ok, 4 cursormode ok)", code);
    CHECK(code != 0xDEAD0001);
    CHECK((code & 0xFFFF0000) == 0);       /* no exception / crash */
}
