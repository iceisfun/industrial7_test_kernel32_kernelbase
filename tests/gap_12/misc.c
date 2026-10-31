/* gap_12: SetStdHandle, SetFileApisToANSI/OEM, SetEnvironmentStringsW, SetLocale*, SetProcess* policies,
 * firmware variables, job IO rate control, protected policies and the deliberately-not-called dangerous exports. */
#include "gc.h"

/* ---------------- export presence ---------------- */
static const struct { const char *n; int k32, kb; } k_exp[] = {
    { "SetConsoleScreenBufferInfoEx", 1, 1 }, { "SetConsoleScreenBufferSize", 1, 1 }, { "SetConsoleTextAttribute", 1, 1 },
    { "SetConsoleTitleA", 1, 1 }, { "SetConsoleTitleW", 1, 1 }, { "SetConsoleWindowInfo", 1, 1 }, { "SetCurrentConsoleFontEx", 1, 1 },
    { "SetDynamicTimeZoneInformation", 1, 1 }, { "SetEnvironmentStringsW", 1, 1 }, { "SetFileApisToANSI", 1, 1 }, { "SetFileApisToOEM", 1, 1 },
    { "SetFileSecurityW", 0, 1 }, { "SetFirmwareEnvironmentVariableA", 1, 0 }, { "SetFirmwareEnvironmentVariableExA", 1, 0 },
    { "SetFirmwareEnvironmentVariableExW", 1, 0 }, { "SetFirmwareEnvironmentVariableW", 1, 0 }, { "SetIoRateControlInformationJobObject", 1, 0 },
    { "SetKernelObjectSecurity", 0, 1 }, { "SetLocaleInfoA", 1, 0 }, { "SetLocaleInfoW", 1, 1 }, { "SetPrivateObjectSecurity", 0, 1 },
    { "SetPrivateObjectSecurityEx", 0, 1 }, { "SetProcessAffinityUpdateMode", 1, 1 }, { "SetProcessDEPPolicy", 1, 0 },
    { "SetProcessDynamicEnforcedCetCompatibleRanges", 1, 1 }, { "SetProtectedPolicy", 1, 1 }, { "SetSecurityAccessMask", 0, 1 },
    { "SetSecurityDescriptorControl", 0, 1 }, { "SetSecurityDescriptorDacl", 0, 1 }, { "SetSecurityDescriptorGroup", 0, 1 },
    { "SetSecurityDescriptorOwner", 0, 1 }, { "SetSecurityDescriptorRMControl", 0, 1 }, { "SetSecurityDescriptorSacl", 0, 1 },
    { "SetStdHandle", 1, 1 }, { "SetSystemPowerState", 1, 0 },
};
TEST(gap12_exports_in_expected_modules) {
    for (unsigned i = 0; i < sizeof k_exp / sizeof k_exp[0]; i++) {
        FARPROC k = t_proc_k32(k_exp[i].n), b = t_proc_kb(k_exp[i].n);
        if (t_is_wine() && ((k_exp[i].k32 && !k) || (k_exp[i].kb && !b))) { t_note("emulator lacks %s", k_exp[i].n); continue; }
        if (k_exp[i].k32) CHECKF(k != NULL, "kernel32 must export %s", k_exp[i].n);
        if (k_exp[i].kb)  CHECKF(b != NULL, "kernelbase must export %s", k_exp[i].n);
        CHECKF(U(SetStdHandle) != NULL, "resolution");
    }
}
TEST(gap12_exports_resolve_via_generic_lookup) {
    for (unsigned i = 0; i < sizeof k_exp / sizeof k_exp[0]; i++) {
        FARPROC p = t_proc(k_exp[i].n);
        if (t_is_wine() && !p) { t_note("emulator lacks %s", k_exp[i].n); continue; }
        CHECKF(p != NULL, "%s", k_exp[i].n);
    }
}
TEST(gap12_dangerous_exports_present_not_called) {
    /* SetSystemPowerState would suspend or hibernate the host; SetDynamicTimeZoneInformation (like the other
     * time-zone setters) has no argument check that fails before the system call. Presence is all that is checked. */
    CHECK(t_is_wine() || U(SetSystemPowerState) != NULL);
    CHECK(t_is_wine() || U(SetDynamicTimeZoneInformation) != NULL);
}

/* ---------------- SetStdHandle ---------------- */
GT(stdh_invalid_ids) {
    GREQ(U(SetStdHandle)); GREQ(U(GetStdHandle));
    static const DWORD ids[] = { 0, 1, 2, 3, 0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFFFFD, 0xFFFFFFFC, 0xFFFFFFF9, 0xFFFFFFF8, 0xFFFFFFF7, 0xFFFFFFF3, 0xFFFFFFF0, 0x80000000, 0x7FFFFFFF, 10, 11, 12, 0xFFFFFFF5 + 0x100 };
    HANDLE out0 = U(GetStdHandle)(STD_OUTPUT_HANDLE), in0 = U(GetStdHandle)(STD_INPUT_HANDLE), err0 = U(GetStdHandle)(STD_ERROR_HANDLE);
    for (unsigned i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        SetLastError(0xBAD);
        BOOL r = U(SetStdHandle)(ids[i], (HANDLE)(ULONG_PTR)0x1234);
        if (r != 0) gfail(__FILE__, __LINE__, "SetStdHandle invalid id rejected", "id 0x%lx returned %d", ids[i], r); else g_cpass++;
        if (!r) { DWORD e = GetLastError(); if (e == ERROR_INVALID_HANDLE) g_cpass++; else gfail(__FILE__, __LINE__, "err==6", "id 0x%lx err=%lu", ids[i], e); }
    }
    GEQ(U(GetStdHandle)(STD_OUTPUT_HANDLE), out0);
    GEQ(U(GetStdHandle)(STD_INPUT_HANDLE), in0);
    GEQ(U(GetStdHandle)(STD_ERROR_HANDLE), err0);
}
GT(stdh_set_get_each) {
    GREQ(U(SetStdHandle)); GREQ(U(GetStdHandle));
    static const DWORD ids[] = { STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE };
    static const ULONG_PTR vals[] = { 0x1234, 0, (ULONG_PTR)-1, 0x7FFFFFFC, 0xFFFFFFFFFFFFFFF0ull, 8 };
    for (unsigned i = 0; i < 3; i++)
        for (unsigned v = 0; v < 6; v++) {
            SetLastError(0xBAD);
            BOOL r = U(SetStdHandle)(ids[i], (HANDLE)vals[v]);
            GEQ(r, 1);
            GERR(0xBAD);   /* success leaves the last error alone */
            GEQ(U(GetStdHandle)(ids[i]), vals[v]);
        }
}
GT(stdh_independent_slots) {
    GREQ(U(SetStdHandle)); GREQ(U(GetStdHandle));
    GC(U(SetStdHandle)(STD_INPUT_HANDLE, (HANDLE)(ULONG_PTR)0x100));
    GC(U(SetStdHandle)(STD_OUTPUT_HANDLE, (HANDLE)(ULONG_PTR)0x200));
    GC(U(SetStdHandle)(STD_ERROR_HANDLE, (HANDLE)(ULONG_PTR)0x300));
    GEQ(U(GetStdHandle)(STD_INPUT_HANDLE), 0x100);
    GEQ(U(GetStdHandle)(STD_OUTPUT_HANDLE), 0x200);
    GEQ(U(GetStdHandle)(STD_ERROR_HANDLE), 0x300);
    GC(U(SetStdHandle)(STD_OUTPUT_HANDLE, (HANDLE)(ULONG_PTR)0x201));
    GEQ(U(GetStdHandle)(STD_INPUT_HANDLE), 0x100);
    GEQ(U(GetStdHandle)(STD_ERROR_HANDLE), 0x300);
}
GT(stdh_no_validation_closed_handle) {
    GREQ(U(SetStdHandle)); GREQ(U(GetStdHandle));
    HANDLE e = CreateEventW(NULL, TRUE, FALSE, NULL); CloseHandle(e);
    SetLastError(0xBAD);
    GEQ(U(SetStdHandle)(STD_OUTPUT_HANDLE, e), 1);
    GEQ(U(GetStdHandle)(STD_OUTPUT_HANDLE), e);
    /* the value is stored as is, even though it is not a handle any more */
    DWORD n = 0;
    SetLastError(0);
    BOOL w = WriteFile(U(GetStdHandle)(STD_OUTPUT_HANDLE), "x", 1, &n, NULL);
    GEQ(w, 0);
    GERR(ERROR_INVALID_HANDLE);
}
GT(stdh_no_ownership) {
    GREQ(U(SetStdHandle));
    HANDLE e = CreateEventW(NULL, TRUE, FALSE, NULL);
    GC(U(SetStdHandle)(STD_ERROR_HANDLE, e));
    GC(CloseHandle(e));   /* still ours */
    SetLastError(0);
    GC(!CloseHandle(e));
    GERR(ERROR_INVALID_HANDLE);
}
GT(stdh_write_through_redirected) {
    GREQ(U(SetStdHandle)); GREQ(U(GetStdHandle));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "stdout_redirect.txt");
    HANDLE f = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, NULL);
    GC(f != INVALID_HANDLE_VALUE);
    GC(U(SetStdHandle)(STD_OUTPUT_HANDLE, f));
    DWORD n = 0;
    GC(WriteFile(U(GetStdHandle)(STD_OUTPUT_HANDLE), "hello std", 9, &n, NULL));
    GEQ(n, 9);
    char buf[32] = {0};
    SetFilePointer(f, 0, NULL, FILE_BEGIN);
    GC(ReadFile(f, buf, 31, &n, NULL));
    GSTR(buf, "hello std");
    /* GetFileType through the std handle sees the file */
    GEQ(GetFileType(U(GetStdHandle)(STD_OUTPUT_HANDLE)), FILE_TYPE_DISK);
    CloseHandle(f);
}
GT(stdh_write_through_pipe) {
    GREQ(U(SetStdHandle)); GREQ(U(GetStdHandle));
    HANDLE r, w; GC(CreatePipe(&r, &w, NULL, 0));
    GC(U(SetStdHandle)(STD_ERROR_HANDLE, w));
    DWORD n = 0; char buf[16] = {0};
    GC(WriteFile(U(GetStdHandle)(STD_ERROR_HANDLE), "pipe!", 5, &n, NULL));
    GC(ReadFile(r, buf, 5, &n, NULL));
    GSTR(buf, "pipe!");
    GEQ(GetFileType(U(GetStdHandle)(STD_ERROR_HANDLE)), FILE_TYPE_PIPE);
    CloseHandle(r); CloseHandle(w);
}
static DWORD WINAPI stdh_thread(LPVOID p) {
    HANDLE (WINAPI *g)(DWORD) = (HANDLE (WINAPI *)(DWORD))t_proc("GetStdHandle");
    *(HANDLE *)p = g(STD_OUTPUT_HANDLE);
    return 0;
}
GT(stdh_visible_to_other_threads) {
    GREQ(U(SetStdHandle));
    GC(U(SetStdHandle)(STD_OUTPUT_HANDLE, (HANDLE)(ULONG_PTR)0xABC0));
    HANDLE seen = NULL, t = CreateThread(NULL, 0, stdh_thread, &seen, 0, NULL);
    GC(t != NULL);
    WaitForSingleObject(t, 5000); CloseHandle(t);
    GEQ(seen, 0xABC0);
}
GT(stdh_repeat_many) {
    GREQ(U(SetStdHandle)); GREQ(U(GetStdHandle));
    int bad = 0;
    for (int i = 0; i < 20000; i++) {
        DWORD id = (i % 3 == 0) ? STD_INPUT_HANDLE : (i % 3 == 1) ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE;
        HANDLE h = (HANDLE)(ULONG_PTR)(0x1000 + i * 4);
        if (!U(SetStdHandle)(id, h) || U(GetStdHandle)(id) != h) bad++;
    }
    GEQ(bad, 0);
}
GT(stdh_getstdhandle_invalid_id) {
    GREQ(U(GetStdHandle));
    SetLastError(0xBAD);
    GEQ(U(GetStdHandle)(0), INVALID_HANDLE_VALUE);
    GERR(ERROR_INVALID_HANDLE);
    SetLastError(0xBAD);
    GEQ(U(GetStdHandle)(0xFFFFFFF3), INVALID_HANDLE_VALUE);
    GERR(ERROR_INVALID_HANDLE);
}
GT(stdh_detached_initial_null_then_set) {
    GREQ(U(SetStdHandle)); GREQ(U(GetStdHandle));
    /* a detached process without redirection has no standard handles */
    HANDLE o = U(GetStdHandle)(STD_OUTPUT_HANDLE);
    gnote("initial stdout in detached child: %p", o);
    GC(U(SetStdHandle)(STD_OUTPUT_HANDLE, INVALID_HANDLE_VALUE));
    GEQ(U(GetStdHandle)(STD_OUTPUT_HANDLE), INVALID_HANDLE_VALUE);
}

/* ---------------- SetFileApisToANSI / SetFileApisToOEM ---------------- */
GT(fileapis_state_flag) {
    GREQ(U(SetFileApisToOEM)); GREQ(U(SetFileApisToANSI)); GREQ(U(AreFileApisANSI));
    GEQ(U(AreFileApisANSI)(), TRUE);
    U(SetFileApisToOEM)();
    GEQ(U(AreFileApisANSI)(), FALSE);
    U(SetFileApisToOEM)();
    GEQ(U(AreFileApisANSI)(), FALSE);
    U(SetFileApisToANSI)();
    GEQ(U(AreFileApisANSI)(), TRUE);
    U(SetFileApisToANSI)();
    GEQ(U(AreFileApisANSI)(), TRUE);
}
GT(fileapis_toggle_many) {
    GREQ(U(SetFileApisToOEM)); GREQ(U(SetFileApisToANSI)); GREQ(U(AreFileApisANSI));
    for (int i = 0; i < 1000; i++) {
        if (i & 1) U(SetFileApisToANSI)(); else U(SetFileApisToOEM)();
        if ((U(AreFileApisANSI)() != 0) != ((i & 1) != 0)) { GCF(0, "iteration %d", i); return; }
    }
    GC(1);
    U(SetFileApisToANSI)();
}
GT(fileapis_does_not_change_codepages) {
    GREQ(U(SetFileApisToOEM)); GREQ(U(SetFileApisToANSI));
    UINT acp = GetACP(), oem = GetOEMCP();
    U(SetFileApisToOEM)();
    GEQ(GetACP(), acp); GEQ(GetOEMCP(), oem);
    WCHAR w[8]; int n = MultiByteToWideChar(CP_ACP, 0, "\xE9", 1, w, 8);
    GEQ(n, 1);
    U(SetFileApisToANSI)();
    WCHAR w2[8]; int n2 = MultiByteToWideChar(CP_ACP, 0, "\xE9", 1, w2, 8);
    GEQ(n, n2); GEQ(w[0], w2[0]);
}
GT(fileapis_ascii_names_unaffected) {
    GREQ(U(SetFileApisToOEM)); GREQ(U(SetFileApisToANSI));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "fa_ascii.txt");
    HANDLE h = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); GC(h != INVALID_HANDLE_VALUE); CloseHandle(h);
    U(SetFileApisToOEM)();
    GC(GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES);
    U(SetFileApisToANSI)();
    GC(GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES);
}
GT(fileapis_names_follow_mode) {
    GREQ(U(SetFileApisToOEM)); GREQ(U(SetFileApisToANSI)); GREQ(U(AreFileApisANSI));
    UINT acp = GetACP(), oem = GetOEMCP();
    static const char nm[] = "fa_\xE9\xFC.bin";
    if (IsDBCSLeadByteEx(acp, 0xE9) || IsDBCSLeadByteEx(oem, 0xE9) || IsDBCSLeadByteEx(acp, 0xFC) || IsDBCSLeadByteEx(oem, 0xFC) || acp == CP_UTF8 || oem == CP_UTF8) { gline("SKIP DBCS/UTF-8 code page"); g_cskip = 1; return; }
    WCHAR wa[64], wo[64];
    MultiByteToWideChar(CP_ACP, 0, nm, -1, wa, 64);
    MultiByteToWideChar(CP_OEMCP, 0, nm, -1, wo, 64);
    if (!lstrcmpW(wa, wo)) { gline("SKIP ANSI and OEM code pages decode the test name identically"); g_cskip = 1; return; }
    char dir[MAX_PATH], full[MAX_PATH]; WCHAR wdir[MAX_PATH], fa[MAX_PATH], fo[MAX_PATH];
    t_tmpfile(dir, sizeof dir, "");
    MultiByteToWideChar(CP_ACP, 0, dir, -1, wdir, MAX_PATH);
    snprintf(full, sizeof full, "%s%s", dir, nm);
    swprintf(fa, MAX_PATH, L"%ls%ls", wdir, wa);
    swprintf(fo, MAX_PATH, L"%ls%ls", wdir, wo);
    /* ANSI mode creates the name decoded with the ANSI page */
    HANDLE h = CreateFileA(full, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
    GC(h != INVALID_HANDLE_VALUE); if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    GC(GetFileAttributesW(fa) != INVALID_FILE_ATTRIBUTES);
    GEQ(GetFileAttributesW(fo), INVALID_FILE_ATTRIBUTES);
    U(SetFileApisToOEM)();
    GEQ(U(AreFileApisANSI)(), FALSE);
    /* in OEM mode the same bytes name a different (non-existent) file */
    SetLastError(0xBAD);
    GEQ(GetFileAttributesA(full), INVALID_FILE_ATTRIBUTES);
    GERR(ERROR_FILE_NOT_FOUND);
    h = CreateFileA(full, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
    GC(h != INVALID_HANDLE_VALUE); if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    GC(GetFileAttributesW(fo) != INVALID_FILE_ATTRIBUTES);
    GC(GetFileAttributesA(full) != INVALID_FILE_ATTRIBUTES);
    /* directory enumeration converts back with the OEM page */
    char pat[MAX_PATH]; snprintf(pat, sizeof pat, "%sfa_*", dir);
    WIN32_FIND_DATAA fd; int seen = 0;
    HANDLE f = FindFirstFileA(pat, &fd);
    if (f != INVALID_HANDLE_VALUE) { do { if (!strcmp(fd.cFileName, nm)) seen++; } while (FindNextFileA(f, &fd)); FindClose(f); }
    GCF(seen == 1, "OEM-mode enumeration found the name %d times", seen);
    U(SetFileApisToANSI)();
    GEQ(U(AreFileApisANSI)(), TRUE);
    GC(GetFileAttributesA(full) != INVALID_FILE_ATTRIBUTES);
    seen = 0;
    f = FindFirstFileA(pat, &fd);
    if (f != INVALID_HANDLE_VALUE) { do { if (!strcmp(fd.cFileName, nm)) seen++; } while (FindNextFileA(f, &fd)); FindClose(f); }
    GCF(seen == 1, "ANSI-mode enumeration found the name %d times", seen);
    /* both files exist under their W names */
    GC(DeleteFileW(fa)); GC(DeleteFileW(fo));
}
GT(fileapis_createdirectory_follows_mode) {
    GREQ(U(SetFileApisToOEM)); GREQ(U(SetFileApisToANSI));
    UINT acp = GetACP(), oem = GetOEMCP();
    static const char nm[] = "fd_\xE9\xFC";
    if (IsDBCSLeadByteEx(acp, 0xE9) || IsDBCSLeadByteEx(oem, 0xE9) || acp == CP_UTF8 || oem == CP_UTF8) { gline("SKIP DBCS/UTF-8 code page"); g_cskip = 1; return; }
    WCHAR wo[64]; MultiByteToWideChar(CP_OEMCP, 0, nm, -1, wo, 64);
    WCHAR wa[64]; MultiByteToWideChar(CP_ACP, 0, nm, -1, wa, 64);
    if (!lstrcmpW(wa, wo)) { gline("SKIP code pages equivalent"); g_cskip = 1; return; }
    char dir[MAX_PATH], full[MAX_PATH]; WCHAR wdir[MAX_PATH], fo[MAX_PATH];
    t_tmpfile(dir, sizeof dir, ""); snprintf(full, sizeof full, "%s%s", dir, nm);
    MultiByteToWideChar(CP_ACP, 0, dir, -1, wdir, MAX_PATH); swprintf(fo, MAX_PATH, L"%ls%ls", wdir, wo);
    U(SetFileApisToOEM)();
    GC(CreateDirectoryA(full, NULL));
    U(SetFileApisToANSI)();
    GC((GetFileAttributesW(fo) & FILE_ATTRIBUTE_DIRECTORY) != 0 && GetFileAttributesW(fo) != INVALID_FILE_ATTRIBUTES);
    GC(RemoveDirectoryW(fo));
}
GT(fileapis_unicode_apis_unaffected) {
    GREQ(U(SetFileApisToOEM)); GREQ(U(SetFileApisToANSI));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; t_tmpfile(p, sizeof p, "fa_w\xE9.bin");
    int n = MultiByteToWideChar(CP_ACP, 0, p, -1, w, MAX_PATH); (void)n;
    HANDLE h = CreateFileW(w, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL); GC(h != INVALID_HANDLE_VALUE); CloseHandle(h);
    U(SetFileApisToOEM)();
    GC(GetFileAttributesW(w) != INVALID_FILE_ATTRIBUTES);
    GC(DeleteFileW(w));
    U(SetFileApisToANSI)();
}
static DWORD WINAPI fa_thread(LPVOID p) { BOOL (WINAPI *f)(void) = (BOOL (WINAPI *)(void))t_proc("AreFileApisANSI"); *(BOOL *)p = f(); return 0; }
GT(fileapis_threads_share_mode) {
    GREQ(U(SetFileApisToOEM)); GREQ(U(SetFileApisToANSI)); GREQ(U(AreFileApisANSI));
    U(SetFileApisToOEM)();
    BOOL seen = -1;
    HANDLE t = CreateThread(NULL, 0, fa_thread, &seen, 0, NULL);
    WaitForSingleObject(t, 5000); CloseHandle(t);
    GEQ(seen, FALSE);
    U(SetFileApisToANSI)();
}

/* ---------------- SetEnvironmentStringsW ---------------- */
static WCHAR *snapshot_env(size_t *wchars) {
    WCHAR *e = GetEnvironmentStringsW(), *p = e; size_t n;
    while (*p) p += lstrlenW(p) + 1;
    n = (size_t)(p - e) + 1;
    WCHAR *c = (WCHAR *)malloc(n * sizeof(WCHAR)); memcpy(c, e, n * sizeof(WCHAR));
    FreeEnvironmentStringsW(e);
    *wchars = n;
    return c;
}
GT(envstr_replace_block) {
    GREQ(U(SetEnvironmentStringsW));
    size_t on; WCHAR *orig = snapshot_env(&on);
    WCHAR blk[] = L"K32T_A=1\0K32T_B=two words\0\0";
    SetLastError(0xBAD);
    BOOL r = U(SetEnvironmentStringsW)(blk);
    GEQ(r, 1);
    WCHAR v[64];
    GEQ(GetEnvironmentVariableW(L"K32T_A", v, 64), 1); GWSTR(v, L"1");
    GEQ(GetEnvironmentVariableW(L"K32T_B", v, 64), 9); GWSTR(v, L"two words");
    SetLastError(0xBAD);
    GEQ(GetEnvironmentVariableW(L"PATH", v, 64), 0);
    GERR(ERROR_ENVVAR_NOT_FOUND);
    /* restore */
    r = U(SetEnvironmentStringsW)(orig);
    GEQ(r, 1);
    GC(GetEnvironmentVariableW(L"PATH", v, 64) != 0 || GetEnvironmentVariableW(L"Path", v, 64) != 0);
    SetLastError(0xBAD);
    GEQ(GetEnvironmentVariableW(L"K32T_A", v, 64), 0);
    GERR(ERROR_ENVVAR_NOT_FOUND);
    free(orig);
}
GT(envstr_block_is_copied) {
    GREQ(U(SetEnvironmentStringsW));
    size_t on; WCHAR *orig = snapshot_env(&on);
    WCHAR blk[] = L"K32T_COPY=value\0\0";
    GC(U(SetEnvironmentStringsW)(blk));
    memset(blk, 0, sizeof blk);
    WCHAR v[32];
    GEQ(GetEnvironmentVariableW(L"K32T_COPY", v, 32), 5); GWSTR(v, L"value");
    GC(U(SetEnvironmentStringsW)(orig));
    free(orig);
}
GT(envstr_get_environment_strings_matches) {
    GREQ(U(SetEnvironmentStringsW));
    size_t on; WCHAR *orig = snapshot_env(&on);
    static const WCHAR blk[] = L"K32T_X=1\0K32T_Y=2\0K32T_Z=3\0\0";
    GC(U(SetEnvironmentStringsW)((LPWCH)blk));
    size_t n; WCHAR *now = snapshot_env(&n);
    GEQ(n, sizeof blk / sizeof(WCHAR) - 1);
    GC(n == sizeof blk / sizeof(WCHAR) - 1 && !memcmp(now, blk, (n) * sizeof(WCHAR)));
    free(now);
    GC(U(SetEnvironmentStringsW)(orig));
    free(orig);
}
GT(envstr_invalid_blocks) {
    GREQ(U(SetEnvironmentStringsW));
    GWINESKIP("emulator does not validate environment blocks");
    size_t on; WCHAR *orig = snapshot_env(&on);
    static const struct { const WCHAR *blk; const char *what; } bad[] = {
        { L"\0", "empty block" },
        { L"NOEQUALS\0", "no equals sign" },
        { L"=\0", "lone equals" },
        { L"=X\0", "leading equals only" },
        { L"A=1\0BAD\0", "second entry without equals" },
    };
    WCHAR v[16];
    for (unsigned i = 0; i < 5; i++) {
        SetLastError(0xBAD);
        BOOL r = U(SetEnvironmentStringsW)((LPWCH)bad[i].blk);
        if (r) { gfail(__FILE__, __LINE__, "block rejected", "%s accepted", bad[i].what); U(SetEnvironmentStringsW)(orig); continue; }
        g_cpass++;
        GERR(ERROR_INVALID_PARAMETER);
        GC(GetEnvironmentVariableW(L"Path", v, 16) != 0 || GetEnvironmentVariableW(L"PATH", v, 16) != 0);   /* environment untouched */
    }
    free(orig);
}
GT(envstr_accepted_forms) {
    GREQ(U(SetEnvironmentStringsW));
    size_t on; WCHAR *orig = snapshot_env(&on);
    WCHAR v[64];
    static const WCHAR empty_value[] = L"K32T_EMPTY=\0\0";
    GC(U(SetEnvironmentStringsW)((LPWCH)empty_value));
    GEQ(GetEnvironmentVariableW(L"K32T_EMPTY", v, 64), 0);
    static const WCHAR hidden[] = L"=Q:=Q:\\dir\0K32T_H=1\0\0";
    if (!t_is_wine()) {
        GC(U(SetEnvironmentStringsW)((LPWCH)hidden));
        GEQ(GetEnvironmentVariableW(L"K32T_H", v, 64), 1);
    }
    static const WCHAR eq_in_value[] = L"K32T_E=a=b=c\0\0";
    GC(U(SetEnvironmentStringsW)((LPWCH)eq_in_value));
    GEQ(GetEnvironmentVariableW(L"K32T_E", v, 64), 5); GWSTR(v, L"a=b=c");
    static const WCHAR lowercase[] = L"k32t_case=low\0\0";
    GC(U(SetEnvironmentStringsW)((LPWCH)lowercase));
    GEQ(GetEnvironmentVariableW(L"K32T_CASE", v, 64), 3);
    static const WCHAR spaces[] = L"K32T SP=a b\0\0";
    GC(U(SetEnvironmentStringsW)((LPWCH)spaces));
    GEQ(GetEnvironmentVariableW(L"K32T SP", v, 64), 3);
    GC(U(SetEnvironmentStringsW)(orig));
    free(orig);
}
GT(envstr_duplicates_allowed) {
    GREQ(U(SetEnvironmentStringsW));
    size_t on; WCHAR *orig = snapshot_env(&on);
    static const WCHAR dup[] = L"K32T_D=first\0K32T_D=second\0\0";
    SetLastError(0xBAD);
    BOOL r = U(SetEnvironmentStringsW)((LPWCH)dup);
    GEQ(r, 1);
    WCHAR v[32]; GetEnvironmentVariableW(L"K32T_D", v, 32);
    gnote("duplicate name resolves to \"%ls\"", v);
    GC(U(SetEnvironmentStringsW)(orig));
    free(orig);
}
GT(envstr_limits) {
    GREQ(U(SetEnvironmentStringsW));
    size_t on; WCHAR *orig = snapshot_env(&on);
    /* name length 0x8000 or value length 0x8000 is rejected; 0x7fff is accepted */
    size_t cap = 0x8000 + 0x8000 + 16;
    WCHAR *b = (WCHAR *)calloc(cap, sizeof(WCHAR));
    for (int variant = 0; variant < 4; variant++) {
        size_t nl = (variant == 0) ? 0x8000 : (variant == 1) ? 0x7fff : 4;
        size_t vl = (variant == 2) ? 0x8000 : (variant == 3) ? 0x7fff : 1;
        size_t k = 0;
        for (size_t i = 0; i < nl; i++) b[k++] = L'N';
        b[k++] = L'=';
        for (size_t i = 0; i < vl; i++) b[k++] = L'v';
        b[k++] = 0; b[k++] = 0;
        SetLastError(0xBAD);
        BOOL r = U(SetEnvironmentStringsW)(b);
        BOOL want = (variant == 1 || variant == 3);
        GCF(r == want || (t_is_wine() && variant == 2), "variant %d (name %zu, value %zu) ret=%d", variant, nl, vl, r);
        if (!r && !t_is_wine()) GERR(ERROR_INVALID_PARAMETER);
    }
    free(b);
    GC(U(SetEnvironmentStringsW)(orig));
    free(orig);
}
GT(envstr_many_variables) {
    GREQ(U(SetEnvironmentStringsW));
    size_t on; WCHAR *orig = snapshot_env(&on);
    WCHAR *b = (WCHAR *)calloc(1000 * 24 + 8, sizeof(WCHAR)); size_t k = 0;
    for (int i = 0; i < 1000; i++) k += swprintf(b + k, 24, L"K32T_V%04d=%d", i, i) + 1;
    b[k] = 0;
    GC(U(SetEnvironmentStringsW)(b));
    WCHAR v[16];
    GEQ(GetEnvironmentVariableW(L"K32T_V0999", v, 16), 3); GWSTR(v, L"999");
    GEQ(GetEnvironmentVariableW(L"K32T_V0000", v, 16), 1);
    free(b);
    GC(U(SetEnvironmentStringsW)(orig));
    free(orig);
}
GT(envstr_then_setenvironmentvariable) {
    GREQ(U(SetEnvironmentStringsW));
    size_t on; WCHAR *orig = snapshot_env(&on);
    static const WCHAR blk[] = L"K32T_BASE=1\0\0";
    GC(U(SetEnvironmentStringsW)((LPWCH)blk));
    GC(SetEnvironmentVariableW(L"K32T_ADDED", L"2"));
    WCHAR v[8];
    GEQ(GetEnvironmentVariableW(L"K32T_BASE", v, 8), 1);
    GEQ(GetEnvironmentVariableW(L"K32T_ADDED", v, 8), 1);
    GC(SetEnvironmentVariableW(L"K32T_BASE", NULL));
    SetLastError(0xBAD);
    GEQ(GetEnvironmentVariableW(L"K32T_BASE", v, 8), 0);
    GERR(ERROR_ENVVAR_NOT_FOUND);
    GC(U(SetEnvironmentStringsW)(orig));
    free(orig);
}
GT(envstr_expand_uses_new_env) {
    GREQ(U(SetEnvironmentStringsW));
    size_t on; WCHAR *orig = snapshot_env(&on);
    static const WCHAR blk[] = L"K32T_EXP=expanded\0\0";
    GC(U(SetEnvironmentStringsW)((LPWCH)blk));
    WCHAR out[64];
    DWORD n = ExpandEnvironmentStringsW(L"<%K32T_EXP%>", out, 64);
    GEQ(n, 11); GWSTR(out, L"<expanded>");
    GC(U(SetEnvironmentStringsW)(orig));
    free(orig);
}
static DWORD WINAPI env_thread(LPVOID p) { WCHAR v[8]; *(DWORD *)p = GetEnvironmentVariableW(L"K32T_THR", v, 8); return 0; }
GT(envstr_cross_thread) {
    GREQ(U(SetEnvironmentStringsW));
    size_t on; WCHAR *orig = snapshot_env(&on);
    static const WCHAR blk[] = L"K32T_THR=abc\0\0";
    GC(U(SetEnvironmentStringsW)((LPWCH)blk));
    DWORD seen = 99; HANDLE t = CreateThread(NULL, 0, env_thread, &seen, 0, NULL);
    WaitForSingleObject(t, 5000); CloseHandle(t);
    GEQ(seen, 3);
    GC(U(SetEnvironmentStringsW)(orig));
    free(orig);
}
CHILD(gap12_envprobe) {
    WCHAR v[16]; DWORD n = GetEnvironmentVariableW(L"K32T_INH", v, 16);
    return n == 3 && !lstrcmpW(v, L"yes") ? 0 : 11;
}
GT(envstr_inherited_by_child) {
    GREQ(U(SetEnvironmentStringsW));
    size_t on; WCHAR *orig = snapshot_env(&on);
    if (on > 30000) { gline("SKIP environment too large"); free(orig); g_cskip = 1; return; }
    WCHAR *blk = (WCHAR *)malloc((on + 64) * sizeof(WCHAR)); size_t k = 0;
    /* new environment = original + marker (SystemRoot is needed for the child to start) */
    memcpy(blk, orig, (on - 1) * sizeof(WCHAR)); k = on - 1;
    k += swprintf(blk + k, 32, L"K32T_INH=yes") + 1; blk[k] = 0;
    GC(U(SetEnvironmentStringsW)(blk));
    DWORD code = t_run_child("gap12_envprobe", "", 15000);
    GEQ(code, 0);
    GC(U(SetEnvironmentStringsW)(orig));
    free(orig); free(blk);
}

/* ---------------- SetLocaleInfoA/W (error paths that cannot change anything) ---------------- */
static const LCID k_badloc[] = { 0xDEADBEEF, 0xFFFFFFFF, 0x7FFFFFFF, 0x0000FFFF };
static const LCTYPE k_lct[] = { LOCALE_SDECIMAL, LOCALE_SSHORTDATE, LOCALE_SLONGDATE, LOCALE_STIMEFORMAT, LOCALE_ILANGUAGE, 0, 0x7FFFFFFF, LOCALE_USE_CP_ACP | LOCALE_SDECIMAL, LOCALE_NOUSEROVERRIDE | LOCALE_STHOUSAND, 0x1000, 0x70, 0xFFF };
TEST(gap12_setlocaleinfow_invalid_locale) {
    REQUIRE(U(SetLocaleInfoW));
    if (t_is_wine()) { t_skip("emulator validates locales differently"); return; }
    for (unsigned i = 0; i < sizeof k_badloc / sizeof k_badloc[0]; i++)
        for (unsigned j = 0; j < sizeof k_lct / sizeof k_lct[0]; j++) {
            SET_ERR(0xBAD);
            BOOL r = U(SetLocaleInfoW)(k_badloc[i], k_lct[j], L"x");
            CHECKF(r == FALSE, "locale 0x%lx type 0x%lx", k_badloc[i], k_lct[j]);
            if (!r) { DWORD e = GetLastError(); CHECKF(e == ERROR_INVALID_PARAMETER, "locale 0x%lx type 0x%lx err=%lu", k_badloc[i], k_lct[j], e); }
        }
}
TEST(gap12_setlocaleinfow_null_data) {
    REQUIRE(U(SetLocaleInfoW));
    if (t_is_wine()) { t_skip("emulator validates locales differently"); return; }
    static const LCID loc[] = { LOCALE_USER_DEFAULT, LOCALE_SYSTEM_DEFAULT, LOCALE_INVARIANT, 0x409, 0xDEADBEEF };
    for (unsigned i = 0; i < 5; i++)
        for (unsigned j = 0; j < sizeof k_lct / sizeof k_lct[0]; j++) {
            SET_ERR(0xBAD);
            BOOL r = U(SetLocaleInfoW)(loc[i], k_lct[j], NULL);
            CHECKF(r == FALSE, "locale 0x%lx type 0x%lx", loc[i], k_lct[j]);
            if (!r) { DWORD e = GetLastError(); CHECKF(e == ERROR_INVALID_PARAMETER, "locale 0x%lx type 0x%lx err=%lu", loc[i], k_lct[j], e); }
        }
}
TEST(gap12_setlocaleinfoa_invalid_locale) {
    REQUIRE(U(SetLocaleInfoA));
    if (t_is_wine()) { t_skip("emulator validates locales differently"); return; }
    for (unsigned i = 0; i < sizeof k_badloc / sizeof k_badloc[0]; i++)
        for (unsigned j = 0; j < sizeof k_lct / sizeof k_lct[0]; j++) {
            SET_ERR(0xBAD);
            BOOL r = U(SetLocaleInfoA)(k_badloc[i], k_lct[j], "x");
            CHECKF(r == FALSE, "locale 0x%lx type 0x%lx", k_badloc[i], k_lct[j]);
            if (!r) { DWORD e = GetLastError(); CHECKF(e == ERROR_INVALID_PARAMETER, "locale 0x%lx type 0x%lx err=%lu", k_badloc[i], k_lct[j], e); }
        }
}
TEST(gap12_setlocaleinfoa_a_w_parity_invalid) {
    REQUIRE(U(SetLocaleInfoA));
    if (t_is_wine()) { t_skip("emulator validates locales differently"); return; } REQUIRE(U(SetLocaleInfoW));
    SET_ERR(0xBAD); BOOL a = U(SetLocaleInfoA)(0xDEADBEEF, LOCALE_SDECIMAL, "."); DWORD ea = GetLastError();
    SET_ERR(0xBAD); BOOL w = U(SetLocaleInfoW)(0xDEADBEEF, LOCALE_SDECIMAL, L"."); DWORD ew = GetLastError();
    EXPECT_EQ(a, w); EXPECT_EQ(ea, ew); EXPECT_EQ(ea, ERROR_INVALID_PARAMETER);
}
TEST(gap12_setlocaleinfoa_out_of_range_type) {
    REQUIRE(U(SetLocaleInfoA));
    if (t_is_wine()) { t_skip("emulator validates locales differently"); return; }
    /* an LCTYPE in 0x70..0xFFF is rejected as invalid flags before the wide routine is reached */
    static const LCTYPE t[] = { 0x70, 0x100, 0x800, 0xFFF };
    for (unsigned i = 0; i < 4; i++) {
        SET_ERR(0xBAD);
        BOOL r = U(SetLocaleInfoA)(0x409, t[i], "x");
        t_note("SetLocaleInfoA(0x409, 0x%lx): ret=%d err=%lu", t[i], r, GetLastError());
        CHECK(r == FALSE);
    }
}
TEST(gap12_setlocaleinfo_repeat_invalid) {
    REQUIRE(U(SetLocaleInfoW));
    if (t_is_wine()) { t_skip("emulator validates locales differently"); return; }
    int bad = 0;
    for (int i = 0; i < 2000; i++) { SET_ERR(0); if (U(SetLocaleInfoW)(0xDEADBEEF, LOCALE_SDECIMAL, L".") || GetLastError() != ERROR_INVALID_PARAMETER) bad++; }
    EXPECT_EQ(bad, 0);
}

/* ---------------- SetProcessAffinityUpdateMode ---------------- */
TEST(gap12_affinity_update_mode_invalid_flags) {
    REQUIRE(U(SetProcessAffinityUpdateMode));
    if (t_is_wine()) { t_skip("emulator does not implement SetProcessAffinityUpdateMode"); return; }
    static const DWORD f[] = { 2, 3, 4, 0x10, 0x100, 0x80000000, 0xFFFFFFFF, 0xFFFFFFFE };
    for (unsigned i = 0; i < sizeof f / sizeof f[0]; i++) {
        SET_ERR(0xBAD);
        EXPECT_EQ(U(SetProcessAffinityUpdateMode)(GetCurrentProcess(), f[i]), FALSE);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(gap12_affinity_update_mode_invalid_flags_precede_handle) {
    REQUIRE(U(SetProcessAffinityUpdateMode));
    if (t_is_wine()) { t_skip("emulator does not implement SetProcessAffinityUpdateMode"); return; }
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetProcessAffinityUpdateMode)(NULL, 2), FALSE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetProcessAffinityUpdateMode)((HANDLE)(ULONG_PTR)0x7777, 0xFFFFFFFF), FALSE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap12_affinity_update_mode_bad_handles) {
    REQUIRE(U(SetProcessAffinityUpdateMode));
    if (t_is_wine()) { t_skip("emulator does not implement SetProcessAffinityUpdateMode"); return; }
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetProcessAffinityUpdateMode)(NULL, 0), FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE e = CreateEventW(NULL, TRUE, FALSE, NULL); CloseHandle(e);
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetProcessAffinityUpdateMode)(e, 0), FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    e = CreateEventW(NULL, TRUE, FALSE, NULL);
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetProcessAffinityUpdateMode)(e, 0), FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(e);
}
TEST(gap12_affinity_update_mode_thread_handle_wrong_type) {
    REQUIRE(U(SetProcessAffinityUpdateMode));
    if (t_is_wine()) { t_skip("emulator does not implement SetProcessAffinityUpdateMode"); return; }
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetProcessAffinityUpdateMode)(GetCurrentThread(), 0), FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap12_affinity_update_mode_query_only_handle) {
    REQUIRE(U(SetProcessAffinityUpdateMode));
    if (t_is_wine()) { t_skip("emulator does not implement SetProcessAffinityUpdateMode"); return; }
    HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    if (!p) { t_skip("OpenProcess"); return; }
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetProcessAffinityUpdateMode)(p, 0), FALSE);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(p);
}
GT(affinity_update_mode_valid) {
    GREQ(U(SetProcessAffinityUpdateMode));
    GWINESKIP("not implemented by the emulator");
    SetLastError(0xBAD);
    GEQ(U(SetProcessAffinityUpdateMode)(GetCurrentProcess(), 0), TRUE);
    GEQ(U(SetProcessAffinityUpdateMode)(GetCurrentProcess(), PROCESS_AFFINITY_ENABLE_AUTO_UPDATE), TRUE);
    GEQ(U(SetProcessAffinityUpdateMode)(GetCurrentProcess(), 0), TRUE);
    HANDLE p = OpenProcess(PROCESS_SET_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    if (p) { GEQ(U(SetProcessAffinityUpdateMode)(p, 1), TRUE); GEQ(U(SetProcessAffinityUpdateMode)(p, 0), TRUE); CloseHandle(p); }
    for (int i = 0; i < 1000; i++) if (!U(SetProcessAffinityUpdateMode)(GetCurrentProcess(), i & 1)) { GCF(0, "iteration %d", i); return; }
}

/* ---------------- SetProcessDEPPolicy ---------------- */
TEST(gap12_dep_policy_not_supported_on_x64) {
    REQUIRE(U(SetProcessDEPPolicy));
    if (t_is_wine()) { t_skip("emulator semantics differ"); return; }
    static const DWORD f[] = { 0, 1, 2, 3, 4, 0x80, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof f / sizeof f[0]; i++) {
        SET_ERR(0xBAD);
        EXPECT_EQ(U(SetProcessDEPPolicy)(f[i]), FALSE);
        EXPECT_ERR(ERROR_NOT_SUPPORTED);
    }
}
TEST(gap12_dep_policy_does_not_change_state) {
    REQUIRE(U(SetProcessDEPPolicy));
    DWORD before = 0; BOOL perm = 0;
    if (!GetProcessDEPPolicy(GetCurrentProcess(), &before, &perm)) { t_skip("GetProcessDEPPolicy"); return; }
    U(SetProcessDEPPolicy)(PROCESS_DEP_ENABLE | PROCESS_DEP_DISABLE_ATL_THUNK_EMULATION);
    DWORD after = 0xFF; BOOL perm2 = 0;
    CHECK(GetProcessDEPPolicy(GetCurrentProcess(), &after, &perm2));
    EXPECT_EQ(before, after); EXPECT_EQ(perm, perm2);
}

/* ---------------- SetProcessDynamicEnforcedCetCompatibleRanges ---------------- */
UDECL(BOOL, SetProcessDynamicEnforcedCetCompatibleRanges, (HANDLE, WORD, PVOID));
GT(cet_ranges_empty) {
    SetProcessDynamicEnforcedCetCompatibleRanges_pfn f = UN(SetProcessDynamicEnforcedCetCompatibleRanges);
    GREQ(f);
    SetLastError(0xBAD);
    BOOL r = f(GetCurrentProcess(), 0, NULL);
    gnote("0 ranges: ret=%d err=%lu", r, GetLastError());
    SetLastError(0xBAD);
    r = f(NULL, 0, NULL);
    gnote("NULL process, 0 ranges: ret=%d err=%lu", r, GetLastError());
    SetLastError(0xBAD);
    r = f(GetCurrentProcess(), 1, NULL);
    gnote("1 range, NULL array: ret=%d err=%lu", r, GetLastError());
    GC(1);
}

/* ---------------- SetProtectedPolicy ---------------- */
UDECL(BOOL, SetProtectedPolicy, (const GUID *, ULONG_PTR, PULONG_PTR));
UDECL(BOOL, QueryProtectedPolicy, (const GUID *, PULONG_PTR));
static const GUID k_pg1 = { 0x6b32a001, 0x1111, 0x4222, { 0x93, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xa1 } };
static const GUID k_pg2 = { 0x6b32a002, 0x1111, 0x4222, { 0x93, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xa2 } };
GT(protected_policy_set_and_old_value) {
    SetProtectedPolicy_pfn f = UN(SetProtectedPolicy);
    GREQ(f); GWINESKIP("emulator stubs protected policies");
    ULONG_PTR old = 0x55;
    SetLastError(0xBAD);
    GEQ(f(&k_pg1, 5, &old), TRUE);
    GEQ(old, 0);                       /* first registration: previous value 0 */
    GEQ(f(&k_pg1, 7, &old), TRUE);
    GEQ(old, 5);
    GEQ(f(&k_pg1, 9, NULL), TRUE);     /* old value is optional */
    GEQ(f(&k_pg1, 0, &old), TRUE);
    GEQ(old, 9);
    GEQ(f(&k_pg1, ~(ULONG_PTR)0, &old), TRUE);
    GEQ(old, 0);
    GEQ(f(&k_pg1, 1, &old), TRUE);
    GEQ(old, ~(ULONG_PTR)0);
}
GT(protected_policy_independent_guids) {
    SetProtectedPolicy_pfn f = UN(SetProtectedPolicy);
    GREQ(f); GWINESKIP("emulator stubs protected policies");
    ULONG_PTR o1 = 1, o2 = 1;
    GC(f(&k_pg1, 100, &o1));
    GC(f(&k_pg2, 200, &o2));
    GEQ(o2, 0);
    GC(f(&k_pg1, 101, &o1)); GEQ(o1, 100);
    GC(f(&k_pg2, 201, &o2)); GEQ(o2, 200);
}
GT(protected_policy_query_matches) {
    SetProtectedPolicy_pfn f = UN(SetProtectedPolicy); QueryProtectedPolicy_pfn q = UN(QueryProtectedPolicy);
    GREQ(f); GREQ(q); GWINESKIP("emulator stubs protected policies");
    GC(f(&k_pg1, 0x1234, NULL));
    ULONG_PTR v = 0;
    SetLastError(0xBAD);
    GEQ(q(&k_pg1, &v), TRUE);
    GEQ(v, 0x1234);
    GC(f(&k_pg1, 0x4321, NULL));
    GEQ(q(&k_pg1, &v), TRUE); GEQ(v, 0x4321);
}
GT(protected_policy_query_unknown) {
    QueryProtectedPolicy_pfn q = UN(QueryProtectedPolicy);
    GREQ(q); GWINESKIP("emulator stubs protected policies");
    static const GUID unk = { 0x6b32afff, 0x9999, 0x4888, { 1, 2, 3, 4, 5, 6, 7, 8 } };
    ULONG_PTR v = 0x77;
    SetLastError(0xBAD);
    BOOL r = q(&unk, &v);
    gnote("unknown policy: ret=%d err=%lu value=0x%llx", r, GetLastError(), (unsigned long long)v);
    GC(1);
}
GT(protected_policy_many_guids) {
    SetProtectedPolicy_pfn f = UN(SetProtectedPolicy); QueryProtectedPolicy_pfn q = UN(QueryProtectedPolicy);
    GREQ(f); GWINESKIP("emulator stubs protected policies");
    /* the table grows by doubling from 16 entries: cross several growth steps */
    int bad = 0;
    for (int i = 0; i < 200; i++) {
        GUID g = { 0x6b32b000u + i, 0x2222, 0x4333, { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, (BYTE)i } };
        ULONG_PTR old = 0x99;
        if (!f(&g, (ULONG_PTR)i + 1000, &old) || old != 0) bad++;
    }
    GEQ(bad, 0);
    bad = 0;
    for (int i = 0; i < 200; i++) {
        GUID g = { 0x6b32b000u + i, 0x2222, 0x4333, { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, (BYTE)i } };
        ULONG_PTR old = 0;
        if (!f(&g, 1, &old) || old != (ULONG_PTR)i + 1000) bad++;
        if (q) { ULONG_PTR v = 0; if (!q(&g, &v) || v != 1) bad++; }
    }
    GEQ(bad, 0);
}

/* ---------------- SetFirmwareEnvironmentVariable{A,W,ExA,ExW}: argument-validation paths only ---------------- */
static int priv_enabled(const char *name) {
    HANDLE t; BYTE buf[2048]; DWORD n = 0; LUID l; int on = 0;
    if (!LookupPrivilegeValueA(NULL, name, &l)) return 1;     /* unknown: treat as unsafe */
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &t)) return 1;
    if (GetTokenInformation(t, TokenPrivileges, buf, sizeof buf, &n)) {
        TOKEN_PRIVILEGES *tp = (TOKEN_PRIVILEGES *)buf;
        for (DWORD i = 0; i < tp->PrivilegeCount; i++)
            if (tp->Privileges[i].Luid.LowPart == l.LowPart && tp->Privileges[i].Luid.HighPart == l.HighPart && (tp->Privileges[i].Attributes & SE_PRIVILEGE_ENABLED)) on = 1;
    } else on = 1;
    CloseHandle(t);
    return on;
}
TEST(gap12_firmware_null_name_or_guid_w) {
    REQUIRE(U(SetFirmwareEnvironmentVariableW)); REQUIRE(U(SetFirmwareEnvironmentVariableExW));
    if (t_is_wine()) { t_skip("emulator semantics differ"); return; }
    static const WCHAR *g = L"{00000000-0000-0000-0000-000000000000}";
    SET_ERR(0xBAD); EXPECT_EQ(U(SetFirmwareEnvironmentVariableW)(NULL, g, NULL, 0), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0xBAD); EXPECT_EQ(U(SetFirmwareEnvironmentVariableW)(L"k32t", NULL, NULL, 0), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0xBAD); EXPECT_EQ(U(SetFirmwareEnvironmentVariableW)(NULL, NULL, NULL, 0), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0xBAD); EXPECT_EQ(U(SetFirmwareEnvironmentVariableExW)(NULL, g, NULL, 0, 0), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0xBAD); EXPECT_EQ(U(SetFirmwareEnvironmentVariableExW)(L"k32t", NULL, NULL, 0, 7), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0xBAD); EXPECT_EQ(U(SetFirmwareEnvironmentVariableExW)(NULL, NULL, NULL, 0, 0xFFFFFFFF), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap12_firmware_null_name_or_guid_a) {
    REQUIRE(U(SetFirmwareEnvironmentVariableA)); REQUIRE(U(SetFirmwareEnvironmentVariableExA));
    if (t_is_wine()) { t_skip("emulator semantics differ"); return; }
    static const char *g = "{00000000-0000-0000-0000-000000000000}";
    SET_ERR(0xBAD); EXPECT_EQ(U(SetFirmwareEnvironmentVariableA)(NULL, g, NULL, 0), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0xBAD); EXPECT_EQ(U(SetFirmwareEnvironmentVariableA)("k32t", NULL, NULL, 0), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0xBAD); EXPECT_EQ(U(SetFirmwareEnvironmentVariableA)(NULL, NULL, NULL, 0), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0xBAD); EXPECT_EQ(U(SetFirmwareEnvironmentVariableExA)(NULL, g, NULL, 0, 0), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0xBAD); EXPECT_EQ(U(SetFirmwareEnvironmentVariableExA)("k32t", NULL, NULL, 0, 3), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0xBAD); EXPECT_EQ(U(SetFirmwareEnvironmentVariableExA)(NULL, NULL, NULL, 0, 0), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
static const WCHAR *k_badguid[] = {
    L"", L"x", L"{}", L"{00000000-0000-0000-0000-000000000000", L"00000000-0000-0000-0000-000000000000}", L"00000000-0000-0000-0000-000000000000",
    L"{0000000-0000-0000-0000-000000000000}", L"{GGGGGGGG-0000-0000-0000-000000000000}", L"{00000000-0000-0000-0000-0000000000000}",
    L"{00000000_0000_0000_0000_000000000000}", L"{00000000-0000-0000-0000-00000000000}", L" {00000000-0000-0000-0000-000000000000}",
    L"{00000000-0000-0000-0000-000000000000} ", L"VenHw(1234)", L"not a guid at all",
};
TEST(gap12_firmware_bad_guid_string_w) {
    REQUIRE(U(SetFirmwareEnvironmentVariableW)); REQUIRE(U(SetFirmwareEnvironmentVariableExW));
    if (t_is_wine()) { t_skip("emulator semantics differ"); return; }
    for (unsigned i = 0; i < sizeof k_badguid / sizeof k_badguid[0]; i++) {
        SET_ERR(0xBAD);
        BOOL r = U(SetFirmwareEnvironmentVariableW)(L"k32t_nonexistent", k_badguid[i], NULL, 0);
        CHECKF(r == FALSE, "guid \"%ls\"", k_badguid[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        SET_ERR(0xBAD);
        r = U(SetFirmwareEnvironmentVariableExW)(L"k32t_nonexistent", k_badguid[i], NULL, 0, 0);
        CHECKF(r == FALSE, "Ex guid \"%ls\"", k_badguid[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(gap12_firmware_bad_guid_string_a) {
    REQUIRE(U(SetFirmwareEnvironmentVariableA)); REQUIRE(U(SetFirmwareEnvironmentVariableExA));
    if (t_is_wine()) { t_skip("emulator semantics differ"); return; }
    for (unsigned i = 0; i < sizeof k_badguid / sizeof k_badguid[0]; i++) {
        char g[96]; WideCharToMultiByte(CP_ACP, 0, k_badguid[i], -1, g, sizeof g, NULL, NULL);
        SET_ERR(0xBAD);
        BOOL r = U(SetFirmwareEnvironmentVariableA)("k32t_nonexistent", g, NULL, 0);
        CHECKF(r == FALSE, "guid \"%s\"", g);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        SET_ERR(0xBAD);
        r = U(SetFirmwareEnvironmentVariableExA)("k32t_nonexistent", g, NULL, 0, 1);
        CHECKF(r == FALSE, "Ex guid \"%s\"", g);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(gap12_firmware_empty_name_bad_guid) {
    REQUIRE(U(SetFirmwareEnvironmentVariableW));
    if (t_is_wine()) { t_skip("emulator semantics differ"); return; }
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetFirmwareEnvironmentVariableW)(L"", L"{bad", NULL, 0), FALSE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap12_firmware_without_privilege) {
    REQUIRE(U(SetFirmwareEnvironmentVariableW)); REQUIRE(U(SetFirmwareEnvironmentVariableA));
    if (t_is_wine()) { t_skip("emulator semantics differ"); return; }
    if (priv_enabled("SeSystemEnvironmentPrivilege")) { t_skip("SeSystemEnvironmentPrivilege is enabled in this token; not touching firmware"); return; }
    /* deleting a variable that does not exist (value NULL, size 0) under a throw-away GUID: the privilege check
     * rejects the request before the firmware is consulted */
    SET_ERR(0xBAD);
    BOOL r = U(SetFirmwareEnvironmentVariableW)(L"k32t_nonexistent", L"{6b32c0de-0000-4000-8000-000000000001}", NULL, 0);
    CHECK(r == FALSE);
    EXPECT_ERR(ERROR_PRIVILEGE_NOT_HELD);
    SET_ERR(0xBAD);
    r = U(SetFirmwareEnvironmentVariableA)("k32t_nonexistent", "{6b32c0de-0000-4000-8000-000000000001}", NULL, 0);
    CHECK(r == FALSE);
    EXPECT_ERR(ERROR_PRIVILEGE_NOT_HELD);
}

/* ---------------- SetIoRateControlInformationJobObject ---------------- */
typedef struct { LONG64 MaxIops, MaxBandwidth, ReservationIops; PCWSTR VolumeName; ULONG BaseIoSize; ULONG ControlFlags; } IORATE;
UDECL(DWORD, SetIoRateControlInformationJobObject, (HANDLE, IORATE *));
TEST(gap12_iorate_null_handle) {
    SetIoRateControlInformationJobObject_pfn f = UN(SetIoRateControlInformationJobObject);
    REQUIRE(f);
    IORATE ir; memset(&ir, 0, sizeof ir);
    SET_ERR(0xBAD);
    EXPECT_EQ(f(NULL, &ir), 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap12_iorate_closed_handle) {
    SetIoRateControlInformationJobObject_pfn f = UN(SetIoRateControlInformationJobObject);
    REQUIRE(f);
    HANDLE j = CreateJobObjectW(NULL, NULL); CloseHandle(j);
    IORATE ir; memset(&ir, 0, sizeof ir);
    SET_ERR(0xBAD);
    EXPECT_EQ(f(j, &ir), 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap12_iorate_wrong_object_type) {
    SetIoRateControlInformationJobObject_pfn f = UN(SetIoRateControlInformationJobObject);
    REQUIRE(f);
    HANDLE e = CreateEventW(NULL, TRUE, FALSE, NULL);
    IORATE ir; memset(&ir, 0, sizeof ir);
    SET_ERR(0xBAD);
    EXPECT_EQ(f(e, &ir), 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(e);
}
TEST(gap12_iorate_process_handle_wrong_type) {
    SetIoRateControlInformationJobObject_pfn f = UN(SetIoRateControlInformationJobObject);
    REQUIRE(f);
    IORATE ir; memset(&ir, 0, sizeof ir);
    SET_ERR(0xBAD);
    EXPECT_EQ(f(GetCurrentProcess(), &ir), 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap12_iorate_no_set_attributes_access) {
    SetIoRateControlInformationJobObject_pfn f = UN(SetIoRateControlInformationJobObject);
    REQUIRE(f);
    HANDLE j = CreateJobObjectW(NULL, NULL), q = NULL;
    CHECK(DuplicateHandle(GetCurrentProcess(), j, GetCurrentProcess(), &q, JOB_OBJECT_QUERY, FALSE, 0));
    IORATE ir; memset(&ir, 0, sizeof ir);
    SET_ERR(0xBAD);
    DWORD r = f(q, &ir);
    t_note("query-only job handle: ret=%lu err=%lu", r, GetLastError());
    CHECK(r == 0);
    CloseHandle(q); CloseHandle(j);
}
TEST(gap12_iorate_empty_job_zero_flags) {
    SetIoRateControlInformationJobObject_pfn f = UN(SetIoRateControlInformationJobObject);
    REQUIRE(f);
    HANDLE j = CreateJobObjectW(NULL, NULL);   /* private, no processes: nothing can be throttled */
    IORATE ir; memset(&ir, 0, sizeof ir);
    SET_ERR(0xBAD);
    DWORD r = f(j, &ir);
    t_note("all-zero control information on an empty job: ret=%lu err=%lu", r, GetLastError());
    CloseHandle(j);
}
TEST(gap12_iorate_volume_name_too_long) {
    SetIoRateControlInformationJobObject_pfn f = UN(SetIoRateControlInformationJobObject);
    REQUIRE(f);
    /* a VolumeName without a terminator within 0x7fff characters fails before the system call */
    HANDLE j = CreateJobObjectW(NULL, NULL);
    WCHAR *s = (WCHAR *)VirtualAlloc(NULL, 0x10000 * sizeof(WCHAR), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!s) { CloseHandle(j); t_skip("VirtualAlloc"); return; }
    for (int i = 0; i < 0x10000; i++) s[i] = L'a';
    s[0xFFFF] = 0;
    IORATE ir; memset(&ir, 0, sizeof ir); ir.VolumeName = s; ir.ControlFlags = 1;
    SET_ERR(0xBAD);
    DWORD r = f(j, &ir);
    EXPECT_EQ(r, 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    VirtualFree(s, 0, MEM_RELEASE);
    CloseHandle(j);
}
