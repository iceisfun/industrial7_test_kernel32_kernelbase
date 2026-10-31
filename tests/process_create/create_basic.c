/* CreateProcessA/W: basic behaviour, PROCESS_INFORMATION, failure modes, legacy launch APIs. */
#include "pc.h"

static void pc_tmpw(WCHAR *out, size_t cap, const char *leaf) { char p[MAX_PATH * 2]; t_tmpfile(p, sizeof p, leaf); pc_widen(out, cap, p); }
static void pc_writefile(const char *leaf, const void *data, DWORD n) {
    char p[MAX_PATH * 2]; DWORD w; HANDLE h;
    t_tmpfile(p, sizeof p, leaf);
    h = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) { if (n) WriteFile(h, data, n, &w, NULL); CloseHandle(h); }
}

/* ---------- success paths ---------- */
TEST(pcreate_w_basic_exit_code) {
    REQUIRE(UK(CreateProcessW));
    pc_opts o; memset(&o, 0, sizeof o); o.child = "pc_exit77";
    EQV(pc_run(&o, PC_TIMEOUT), 77);
}
TEST(pcreate_a_basic_exit_code) {
    REQUIRE(UK(CreateProcessA));
    pc_opts o; memset(&o, 0, sizeof o); o.child = "pc_exit77"; o.ansi = 1;
    EQV(pc_run(&o, PC_TIMEOUT), 77);
}
TEST(pcreate_a_with_application_name) {
    REQUIRE(UK(CreateProcessA));
    pc_opts o; memset(&o, 0, sizeof o); o.child = "pc_exit77"; o.ansi = 1; o.app = pc_selfw();
    EQV(pc_run(&o, PC_TIMEOUT), 77);
}
TEST(pcreate_w_with_application_name) {
    REQUIRE(UK(CreateProcessW));
    pc_opts o; memset(&o, 0, sizeof o); o.child = "pc_exit77"; o.app = pc_selfw();
    EQV(pc_run(&o, PC_TIMEOUT), 77);
}
TEST(pcreate_pi_fields_valid) {
    REQUIRE(UK(CreateProcessW));
    PROCESS_INFORMATION pi;
    CHECK(pc_suspended("pc_exit77", "", 0, &pi));
    CHECK(pi.hProcess != NULL && pi.hProcess != INVALID_HANDLE_VALUE);
    CHECK(pi.hThread != NULL && pi.hThread != INVALID_HANDLE_VALUE);
    CHECK(pi.dwProcessId != 0 && pi.dwThreadId != 0);
    CHECK(pi.dwProcessId != GetCurrentProcessId());
    CHECK((pi.dwProcessId & 3) == 0);
    CHECK((pi.dwThreadId & 3) == 0);
    CHECK(((ULONG_PTR)pi.hProcess & 3) == 0 && ((ULONG_PTR)pi.hThread & 3) == 0);
    CHECK(pi.hProcess != pi.hThread);
    CHECK(U(GetProcessId)(pi.hProcess) == pi.dwProcessId);
    CHECK(U(GetThreadId)(pi.hThread) == pi.dwThreadId);
    CHECK(U(GetProcessIdOfThread)(pi.hThread) == pi.dwProcessId);
    pc_kill(&pi);
}
TEST(pcreate_pi_ids_unique_across_spawns) {
    REQUIRE(UK(CreateProcessW));
    PROCESS_INFORMATION p[3]; int i, j;
    for (i = 0; i < 3; i++) CHECK(pc_suspended("pc_exit77", "", 0, &p[i]));
    for (i = 0; i < 3; i++) for (j = i + 1; j < 3; j++) { CHECK(p[i].dwProcessId != p[j].dwProcessId); CHECK(p[i].dwThreadId != p[j].dwThreadId); }
    for (i = 0; i < 3; i++) pc_kill(&p[i]);
}
TEST(pcreate_pi_process_handle_full_access) {
    REQUIRE(UK(CreateProcessW)); UNT(NtQueryObject);
    NtQueryObject_pfn q = UN(NtQueryObject); PROCESS_INFORMATION pi; PC_OBI b;
    REQUIRE(q);
    CHECK(pc_suspended("pc_exit77", "", 0, &pi));
    memset(&b, 0, sizeof b);
    CHECK(q(pi.hProcess, 0, &b, sizeof b, NULL) >= 0);
    CHECKF((b.GrantedAccess & PROCESS_ALL_ACCESS) == PROCESS_ALL_ACCESS, "granted 0x%lx", b.GrantedAccess);
    memset(&b, 0, sizeof b);
    CHECK(q(pi.hThread, 0, &b, sizeof b, NULL) >= 0);
    CHECKF((b.GrantedAccess & THREAD_ALL_ACCESS) == THREAD_ALL_ACCESS, "granted 0x%lx", b.GrantedAccess);
    pc_kill(&pi);
}
TEST(pcreate_pi_handles_not_inheritable_by_default) {
    REQUIRE(UK(CreateProcessW));
    PROCESS_INFORMATION pi; DWORD fl = 0xFF;
    CHECK(pc_suspended("pc_exit77", "", 0, &pi));
    CHECK(GetHandleInformation(pi.hProcess, &fl)); CHECK(!(fl & HANDLE_FLAG_INHERIT));
    fl = 0xFF; CHECK(GetHandleInformation(pi.hThread, &fl)); CHECK(!(fl & HANDLE_FLAG_INHERIT));
    pc_kill(&pi);
}
TEST(pcreate_pi_handles_inheritable_via_security_attributes) {
    REQUIRE(UK(CreateProcessW));
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE }; pc_opts o; PROCESS_INFORMATION pi; DWORD fl = 0;
    memset(&o, 0, sizeof o); o.child = "pc_exit77"; o.flags = CREATE_SUSPENDED; o.psa = &sa; o.tsa = NULL;
    CHECK(pc_launch(&o, &pi));
    CHECK(GetHandleInformation(pi.hProcess, &fl)); CHECK(fl & HANDLE_FLAG_INHERIT);
    fl = 0xFF; CHECK(GetHandleInformation(pi.hThread, &fl)); CHECK(!(fl & HANDLE_FLAG_INHERIT));
    pc_kill(&pi);
    o.psa = NULL; o.tsa = &sa;
    CHECK(pc_launch(&o, &pi));
    fl = 0xFF; CHECK(GetHandleInformation(pi.hProcess, &fl)); CHECK(!(fl & HANDLE_FLAG_INHERIT));
    fl = 0; CHECK(GetHandleInformation(pi.hThread, &fl)); CHECK(fl & HANDLE_FLAG_INHERIT);
    pc_kill(&pi);
}
TEST(pcreate_initial_thread_priority_normal) {
    REQUIRE(UK(CreateProcessW));
    PROCESS_INFORMATION pi;
    CHECK(pc_suspended("pc_exit77", "", IDLE_PRIORITY_CLASS, &pi));
    EQV(U(GetThreadPriority)(pi.hThread), THREAD_PRIORITY_NORMAL);
    pc_kill(&pi);
}
TEST(pcreate_wait_signals_after_exit_and_stays_signaled) {
    REQUIRE(UK(CreateProcessW));
    PROCESS_INFORMATION pi; DWORD code = 0;
    CHECK(pc_suspended("pc_exit77", "", 0, &pi));
    EQV(WaitForSingleObject(pi.hProcess, 150), WAIT_TIMEOUT);
    EQV(ResumeThread(pi.hThread), 1);
    EQV(WaitForSingleObject(pi.hProcess, PC_TIMEOUT), WAIT_OBJECT_0);
    EQV(WaitForSingleObject(pi.hProcess, 0), WAIT_OBJECT_0);
    EQV(WaitForSingleObject(pi.hThread, 5000), WAIT_OBJECT_0);
    CHECK(GetExitCodeProcess(pi.hProcess, &code)); EQV(code, 77);
    CHECK(GetExitCodeThread(pi.hThread, &code)); t_note("main thread exit code %lu", code);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
}

/* ---------- parameter validation ---------- */
TEST(pcreate_null_app_and_cmdline_fails_87) {
    REQUIRE(UK(CreateProcessW)); REQUIRE(UK(CreateProcessA));
    if (t_is_wine()) { t_skip("crashes under wine"); return; }
    STARTUPINFOW si; STARTUPINFOA sa; PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si); si.cb = sizeof si; memset(&sa, 0, sizeof sa); sa.cb = sizeof sa;
    SET_ERR(0); CHECK(!UK(CreateProcessW)(NULL, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!UK(CreateProcessA)(NULL, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &sa, &pi)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(pcreate_null_startupinfo_or_pi_fails_87) {
    REQUIRE(UK(CreateProcessW));
    if (t_is_wine()) { t_skip("crashes under wine"); return; }
    WCHAR cmd[MAX_PATH * 2]; STARTUPINFOW si; PROCESS_INFORMATION pi;
    wsprintfW(cmd, L"\"%s\" --child pc_exit77", pc_selfw());
    memset(&si, 0, sizeof si); si.cb = sizeof si;
    SET_ERR(0); CHECK(!UK(CreateProcessW)(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, NULL, &pi)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!UK(CreateProcessW)(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, NULL)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(pcreate_detached_and_new_console_conflict_87) {
    REQUIRE(UK(CreateProcessW));
    pc_opts o; PROCESS_INFORMATION pi;
    memset(&o, 0, sizeof o); o.child = "pc_exit77"; o.flags = DETACHED_PROCESS | CREATE_NEW_CONSOLE;
    SET_ERR(0); CHECK(!pc_launch(&o, &pi)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(pcreate_shared_and_separate_wow_vdm_conflict_87) {
    REQUIRE(UK(CreateProcessW));
    pc_opts o; PROCESS_INFORMATION pi;
    memset(&o, 0, sizeof o); o.child = "pc_exit77"; o.flags = CREATE_SHARED_WOW_VDM | CREATE_SEPARATE_WOW_VDM;
    SET_ERR(0); CHECK(!pc_launch(&o, &pi)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(pcreate_empty_command_line_fails) {
    REQUIRE(UK(CreateProcessW));
    pc_opts o; PROCESS_INFORMATION pi;
    memset(&o, 0, sizeof o); o.raw_cmd = L"";
    SET_ERR(0); CHECK(!pc_launch(&o, &pi)); t_note("empty command line error %lu", GetLastError());
    o.raw_cmd = L"   ";
    SET_ERR(0); CHECK(!pc_launch(&o, &pi)); t_note("blank command line error %lu", GetLastError());
    o.raw_cmd = L"\"\"";
    SET_ERR(0); CHECK(!pc_launch(&o, &pi)); t_note("empty quoted command line error %lu", GetLastError());
}
TEST(pcreate_unknown_creation_flag_bits) {
    REQUIRE(UK(CreateProcessW));
    pc_opts o; PROCESS_INFORMATION pi;
    memset(&o, 0, sizeof o); o.child = "pc_exit77"; o.flags = CREATE_SUSPENDED | 0x00200000;
    if (pc_launch(&o, &pi)) { t_note("flag 0x200000 accepted"); pc_kill(&pi); } else t_note("flag 0x200000 rejected err %lu", GetLastError());
    o.flags = CREATE_SUSPENDED | CREATE_PROTECTED_PROCESS;
    SET_ERR(0);
    if (pc_launch(&o, &pi)) { t_note("CREATE_PROTECTED_PROCESS accepted"); pc_kill(&pi); } else t_note("CREATE_PROTECTED_PROCESS rejected err %lu", GetLastError());
}

/* ---------- failure modes ---------- */
TEST(pcreate_nonexistent_file_application_name) {
    REQUIRE(UK(CreateProcessW)); REQUIRE(UK(CreateProcessA));
    pc_opts o; PROCESS_INFORMATION pi; WCHAR p[MAX_PATH * 2];
    pc_tmpw(p, MAX_PATH * 2, "no_such_program.exe");
    memset(&o, 0, sizeof o); o.app = p; o.raw_cmd = L"x --child pc_exit77";
    SET_ERR(0); CHECK(!pc_launch(&o, &pi)); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    o.ansi = 1;
    SET_ERR(0); CHECK(!pc_launch(&o, &pi)); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(pcreate_nonexistent_file_in_command_line) {
    REQUIRE(UK(CreateProcessW)); REQUIRE(UK(CreateProcessA));
    pc_opts o; PROCESS_INFORMATION pi; WCHAR cmd[MAX_PATH * 2], p[MAX_PATH * 2];
    pc_tmpw(p, MAX_PATH * 2, "no_such_program.exe");
    wsprintfW(cmd, L"\"%s\" a b c", p);
    memset(&o, 0, sizeof o); o.raw_cmd = cmd;
    SET_ERR(0); CHECK(!pc_launch(&o, &pi)); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    o.ansi = 1;
    SET_ERR(0); CHECK(!pc_launch(&o, &pi)); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(pcreate_bare_name_not_found_2) {
    REQUIRE(UK(CreateProcessW));
    pc_opts o; PROCESS_INFORMATION pi;
    memset(&o, 0, sizeof o); o.raw_cmd = L"k32t_no_such_program_zz arg";
    SET_ERR(0); CHECK(!pc_launch(&o, &pi)); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(pcreate_missing_directory_path) {
    REQUIRE(UK(CreateProcessW));
    pc_opts o; PROCESS_INFORMATION pi; WCHAR p[MAX_PATH * 2];
    pc_tmpw(p, MAX_PATH * 2, "no_such_dir\\sub\\x.exe");
    memset(&o, 0, sizeof o); o.app = p; o.raw_cmd = L"x --child pc_exit77";
    SET_ERR(0); CHECK(!pc_launch(&o, &pi)); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    o.app = NULL; o.raw_cmd = p;
    SET_ERR(0); CHECK(!pc_launch(&o, &pi)); { DWORD e = GetLastError(); CHECKF(e == ERROR_PATH_NOT_FOUND || e == ERROR_FILE_NOT_FOUND, "err %lu", e); }
}
TEST(pcreate_application_name_is_directory) {
    REQUIRE(UK(CreateProcessW));
    pc_opts o; PROCESS_INFORMATION pi; WCHAR p[MAX_PATH * 2];
    pc_tmpw(p, MAX_PATH * 2, "");
    memset(&o, 0, sizeof o); o.app = p; o.raw_cmd = L"x --child pc_exit77";
    SET_ERR(0); CHECK(!pc_launch(&o, &pi)); t_note("directory as image error %lu", GetLastError());
    { DWORD e = GetLastError(); CHECKF(e == ERROR_ACCESS_DENIED || e == ERROR_BAD_EXE_FORMAT || e == ERROR_FILE_NOT_FOUND, "err %lu", e); }
}
TEST(pcreate_text_file_is_bad_exe_format) {
    REQUIRE(UK(CreateProcessW));
    static const char t[] = "this is not a PE image at all, just some text\r\n";
    pc_opts o; PROCESS_INFORMATION pi; WCHAR p[MAX_PATH * 2];
    pc_writefile("text.exe", t, sizeof t - 1); pc_tmpw(p, MAX_PATH * 2, "text.exe");
    memset(&o, 0, sizeof o); o.app = p; o.raw_cmd = L"x --child pc_exit77";
    SET_ERR(0); CHECK(!pc_launch(&o, &pi)); EXPECT_ERR(ERROR_BAD_EXE_FORMAT);
    o.app = NULL; o.raw_cmd = p;
    SET_ERR(0); CHECK(!pc_launch(&o, &pi)); EXPECT_ERR(ERROR_BAD_EXE_FORMAT);
}
TEST(pcreate_text_file_other_extension_is_bad_exe_format) {
    REQUIRE(UK(CreateProcessW));
    pc_opts o; PROCESS_INFORMATION pi; WCHAR p[MAX_PATH * 2];
    pc_writefile("notes.txt", "hello world, this is text", 25); pc_tmpw(p, MAX_PATH * 2, "notes.txt");
    memset(&o, 0, sizeof o); o.app = p; o.raw_cmd = L"x --child pc_exit77";
    SET_ERR(0); CHECK(!pc_launch(&o, &pi)); EXPECT_ERR(ERROR_BAD_EXE_FORMAT);
}
TEST(pcreate_empty_file_is_bad_exe_format) {
    REQUIRE(UK(CreateProcessW));
    pc_opts o; PROCESS_INFORMATION pi; WCHAR p[MAX_PATH * 2];
    pc_writefile("empty.exe", NULL, 0); pc_tmpw(p, MAX_PATH * 2, "empty.exe");
    memset(&o, 0, sizeof o); o.app = p; o.raw_cmd = L"x --child pc_exit77";
    SET_ERR(0); CHECK(!pc_launch(&o, &pi)); EXPECT_ERR(ERROR_BAD_EXE_FORMAT);
}
TEST(pcreate_mz_only_is_bad_exe_format) {
    REQUIRE(UK(CreateProcessW));
    pc_opts o; PROCESS_INFORMATION pi; WCHAR p[MAX_PATH * 2];
    pc_writefile("mz.exe", "MZ", 2); pc_tmpw(p, MAX_PATH * 2, "mz.exe");
    memset(&o, 0, sizeof o); o.app = p; o.raw_cmd = L"x --child pc_exit77";
    SET_ERR(0); CHECK(!pc_launch(&o, &pi)); { DWORD e = GetLastError(); CHECKF(e == ERROR_BAD_EXE_FORMAT, "err %lu", e); }
}
TEST(pcreate_truncated_pe_is_bad_exe_format) {
    REQUIRE(UK(CreateProcessW));
    pc_opts o; PROCESS_INFORMATION pi; WCHAR p[MAX_PATH * 2]; BYTE buf[4096]; DWORD n = 0; HANDLE h;
    h = CreateFileA(t_self_path(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE); if (h == INVALID_HANDLE_VALUE) return;
    ReadFile(h, buf, sizeof buf, &n, NULL); CloseHandle(h);
    pc_writefile("trunc.exe", buf, 512); pc_tmpw(p, MAX_PATH * 2, "trunc.exe");
    memset(&o, 0, sizeof o); o.app = p; o.raw_cmd = L"x --child pc_exit77";
    SET_ERR(0); CHECK(!pc_launch(&o, &pi)); { DWORD e = GetLastError(); CHECKF(e == ERROR_BAD_EXE_FORMAT, "err %lu", e); }
}
TEST(pcreate_dll_image_is_not_executable) {
    REQUIRE(UK(CreateProcessW));
    pc_opts o; PROCESS_INFORMATION pi; WCHAR p[MAX_PATH * 2]; char sys[MAX_PATH], src[MAX_PATH * 2], dst[MAX_PATH * 2];
    GetSystemDirectoryA(sys, sizeof sys);
    snprintf(src, sizeof src, "%s\\version.dll", sys); t_tmpfile(dst, sizeof dst, "dllasexe.exe");
    if (!CopyFileA(src, dst, FALSE)) { t_skip("cannot copy version.dll"); return; }
    pc_widen(p, MAX_PATH * 2, dst);
    memset(&o, 0, sizeof o); o.app = p; o.raw_cmd = L"x --child pc_exit77";
    SET_ERR(0); CHECK(!pc_launch(&o, &pi)); EXPECT_ERR(ERROR_BAD_EXE_FORMAT);
}
TEST(pcreate_relative_name_with_application_name_uses_cwd_only) {
    REQUIRE(UK(CreateProcessW));
    pc_opts o; PROCESS_INFORMATION pi;
    if (GetFileAttributesW(L"cmd.exe") != INVALID_FILE_ATTRIBUTES) { t_skip("cmd.exe in cwd"); return; }
    memset(&o, 0, sizeof o); o.app = L"cmd.exe"; o.raw_cmd = L"cmd.exe /c exit 5";
    SET_ERR(0); CHECK(!pc_launch(&o, &pi)); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(pcreate_system_program_found_by_search) {
    REQUIRE(UK(CreateProcessW));
    pc_opts o; memset(&o, 0, sizeof o); o.raw_cmd = L"cmd /c exit 5";
    o.flags = CREATE_NO_WINDOW;
    EQV(pc_run(&o, PC_TIMEOUT), 5);
    o.raw_cmd = L"cmd.exe /c exit 6";
    EQV(pc_run(&o, PC_TIMEOUT), 6);
    o.raw_cmd = L"CMD.EXE /C exit 7";
    EQV(pc_run(&o, PC_TIMEOUT), 7);
}
TEST(pcreate_system_program_via_application_name) {
    REQUIRE(UK(CreateProcessW));
    pc_opts o; WCHAR p[MAX_PATH], cmd[MAX_PATH * 2];
    GetSystemDirectoryW(p, MAX_PATH); lstrcatW(p, L"\\cmd.exe");
    wsprintfW(cmd, L"cmd /c exit 9");
    memset(&o, 0, sizeof o); o.app = p; o.raw_cmd = cmd; o.flags = CREATE_NO_WINDOW;
    EQV(pc_run(&o, PC_TIMEOUT), 9);
}

/* ---------- access denied / suspended-never-runs ---------- */
TEST(pcreate_suspended_then_terminated_never_runs) {
    REQUIRE(UK(CreateProcessW));
    char f[MAX_PATH * 2], args[MAX_PATH * 2 + 4]; PROCESS_INFORMATION pi; DWORD code = 0;
    t_tmpfile(f, sizeof f, "marker_never.txt"); snprintf(args, sizeof args, "\"%s\"", f);
    CHECK(pc_suspended("pc_touch", args, 0, &pi));
    EQV(WaitForSingleObject(pi.hProcess, 300), WAIT_TIMEOUT);
    CHECK(GetExitCodeProcess(pi.hProcess, &code)); EQV(code, STILL_ACTIVE);
    CHECK(U(TerminateProcess)(pi.hProcess, 0x1234));
    EQV(WaitForSingleObject(pi.hProcess, 5000), WAIT_OBJECT_0);
    CHECK(GetExitCodeProcess(pi.hProcess, &code)); EQV(code, 0x1234);
    CHECK(GetFileAttributesA(f) == INVALID_FILE_ATTRIBUTES);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
}
TEST(pcreate_suspended_resume_runs_child) {
    REQUIRE(UK(CreateProcessW));
    char f[MAX_PATH * 2], args[MAX_PATH * 2 + 4]; PROCESS_INFORMATION pi; int i;
    t_tmpfile(f, sizeof f, "marker_runs.txt"); snprintf(args, sizeof args, "\"%s\"", f);
    CHECK(pc_suspended("pc_touch", args, 0, &pi));
    Sleep(200);
    CHECK(GetFileAttributesA(f) == INVALID_FILE_ATTRIBUTES);
    CHECK(ResumeThread(pi.hThread) == 1);
    CHECK(WaitForSingleObject(pi.hProcess, PC_TIMEOUT) == WAIT_OBJECT_0);
    for (i = 0; i < 20 && GetFileAttributesA(f) == INVALID_FILE_ATTRIBUTES; i++) Sleep(50);
    CHECK(GetFileAttributesA(f) != INVALID_FILE_ATTRIBUTES);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
}

/* ---------- legacy launch APIs ---------- */
UDECL(BOOL, CreateProcessInternalW, (HANDLE, LPCWSTR, LPWSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD, LPVOID, LPCWSTR, LPSTARTUPINFOW, LPPROCESS_INFORMATION, PHANDLE));
UDECL(BOOL, CreateProcessInternalA, (HANDLE, LPCSTR, LPSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD, LPVOID, LPCSTR, LPSTARTUPINFOA, LPPROCESS_INFORMATION, PHANDLE));
TEST(pcreate_internal_w_null_token) {
    CreateProcessInternalW_pfn f = UN(CreateProcessInternalW); REQUIRE(f);
    if (t_is_wine()) { t_skip("crashes under wine"); return; }
    WCHAR cmd[MAX_PATH * 2]; STARTUPINFOW si; PROCESS_INFORMATION pi; DWORD code = 0;
    wsprintfW(cmd, L"\"%s\" --child pc_exit77", pc_selfw());
    memset(&si, 0, sizeof si); si.cb = sizeof si;
    CHECK(f(NULL, NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi, NULL));
    CHECK(WaitForSingleObject(pi.hProcess, PC_TIMEOUT) == WAIT_OBJECT_0);
    GetExitCodeProcess(pi.hProcess, &code); EQV(code, 77);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    SET_ERR(0); CHECK(!f(NULL, NULL, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi, NULL)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(pcreate_internal_a_null_token) {
    CreateProcessInternalA_pfn f = UN(CreateProcessInternalA); REQUIRE(f);
    char cmd[MAX_PATH * 2]; STARTUPINFOA si; PROCESS_INFORMATION pi; DWORD code = 0;
    snprintf(cmd, sizeof cmd, "\"%s\" --child pc_exit77", t_self_path());
    memset(&si, 0, sizeof si); si.cb = sizeof si;
    CHECK(f(NULL, NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi, NULL));
    CHECK(WaitForSingleObject(pi.hProcess, PC_TIMEOUT) == WAIT_OBJECT_0);
    GetExitCodeProcess(pi.hProcess, &code); EQV(code, 77);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
}
TEST(pcreate_as_user_null_token_observed) {
    REQUIRE(UK(CreateProcessAsUserW));
    WCHAR cmd[MAX_PATH * 2]; STARTUPINFOW si; PROCESS_INFORMATION pi;
    wsprintfW(cmd, L"\"%s\" --child pc_exit77", pc_selfw());
    memset(&si, 0, sizeof si); si.cb = sizeof si;
    SET_ERR(0);
    if (UK(CreateProcessAsUserW)(NULL, NULL, cmd, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi)) { t_note("CreateProcessAsUserW(NULL token) succeeded"); pc_kill(&pi); }
    else t_note("CreateProcessAsUserW(NULL token) failed err %lu", GetLastError());
}
TEST(pcreate_winexec_success_returns_33) {
    REQUIRE(U(WinExec));
    char f[MAX_PATH * 2], cmd[MAX_PATH * 4]; int i; UINT r;
    t_tmpfile(f, sizeof f, "winexec_marker.txt");
    snprintf(cmd, sizeof cmd, "\"%s\" --child pc_touch \"%s\"", t_self_path(), f);
    r = U(WinExec)(cmd, SW_HIDE); EQV(r, 33);
    for (i = 0; i < 200 && GetFileAttributesA(f) == INVALID_FILE_ATTRIBUTES; i++) Sleep(50);
    CHECK(GetFileAttributesA(f) != INVALID_FILE_ATTRIBUTES);
}
TEST(pcreate_winexec_error_returns) {
    REQUIRE(U(WinExec));
    char p[MAX_PATH * 2];
    EQV(U(WinExec)("k32t_no_such_program_zz.exe", SW_HIDE), 2);
    t_tmpfile(p, sizeof p, "no_such_dir\\x.exe"); { UINT r = U(WinExec)(p, SW_HIDE); CHECKF(r == 2 || r == 3, "got %u", r); }
    pc_writefile("winexec_bad.exe", "not an exe at all", 17); t_tmpfile(p, sizeof p, "winexec_bad.exe");
    EQV(U(WinExec)(p, SW_HIDE), 11);
}
TEST(pcreate_loadmodule_not_found) {
    REQUIRE(U(LoadModule));
    struct { LPVOID env; LPSTR cmd; WORD *show; DWORD rsv; } pb; WORD show[2] = { 2, SW_HIDE };
    pb.env = NULL; pb.cmd = (LPSTR)"\0"; pb.show = show; pb.rsv = 0;
    EQV(U(LoadModule)("k32t_no_such_module_zz", &pb), 2);
}
TEST(pcreate_loadmodule_runs_program) {
    REQUIRE(U(LoadModule));
    char f[MAX_PATH * 2], self[MAX_PATH * 2]; int i; DWORD r; WORD show[2] = { 2, SW_HIDE };
    struct { LPVOID env; LPSTR cmd; WORD *show; DWORD rsv; } pb;
    char pas[MAX_PATH * 2 + 40];
    t_tmpfile(f, sizeof f, "loadmodule_marker.txt");
    strcpy(self, t_self_path());
    if (strchr(self, ' ') || strchr(f, ' ')) { t_skip("path contains spaces"); return; }
    snprintf(pas + 1, sizeof pas - 1, "--child pc_touch %s", f); pas[0] = (char)strlen(pas + 1);
    pb.env = NULL; pb.cmd = pas; pb.show = show; pb.rsv = 0;
    r = U(LoadModule)(self, &pb); EQV(r, 33);
    for (i = 0; i < 200 && GetFileAttributesA(f) == INVALID_FILE_ATTRIBUTES; i++) Sleep(50);
    CHECK(GetFileAttributesA(f) != INVALID_FILE_ATTRIBUTES);
}
