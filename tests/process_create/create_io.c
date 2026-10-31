/* CreateProcess: handle inheritance, standard handle redirection, STARTUPINFO propagation. */
#include "pc.h"

/* Make a handle whose value is high enough that it cannot coincide with a handle the child opens on its own. */
static HANDLE junk[800]; static int njunk;
static HANDLE high_event(BOOL inheritable, BOOL manual) {
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, inheritable }; HANDLE h = NULL;
    njunk = 0;
    do { h = CreateEventW(&sa, manual, FALSE, NULL); junk[njunk++] = h; } while (h && (ULONG_PTR)h < 0x400 && njunk < 790);
    return h;
}
static void free_junk(void) { while (njunk > 0) { HANDLE h = junk[--njunk]; if (h) CloseHandle(h); } }
static void u8(const WCHAR *w, char *out, size_t cap) { if (!WideCharToMultiByte(CP_UTF8, 0, w, -1, out, (int)cap, NULL, NULL)) out[0] = 0; }

TEST(pcinh_inheritable_event_same_value_and_usable) {
    REQUIRE(UK(CreateProcessW));
    HANDLE ev = high_event(TRUE, TRUE); pc_opts o; char out[4096], a[64], key[32]; DWORD code;
    CHECK(ev != NULL); if (!ev) { free_junk(); return; }
    snprintf(a, sizeof a, "%llx:s", (unsigned long long)(ULONG_PTR)ev);
    memset(&o, 0, sizeof o); o.child = "pc_handles"; o.args = a;
    CHECK(pc_capture(&o, out, sizeof out, NULL, 0, &code, PC_TIMEOUT)); EQV(code, 0);
    snprintf(key, sizeof key, "h%llx", (unsigned long long)(ULONG_PTR)ev); EQV(pc_num(out, key, -1), 1);
    snprintf(key, sizeof key, "i%llx", (unsigned long long)(ULONG_PTR)ev); EQV(pc_num(out, key, -1), 1);
    EQV(WaitForSingleObject(ev, 0), WAIT_OBJECT_0);   /* child signalled it through its copy */
    free_junk();
}
TEST(pcinh_non_inheritable_event_absent_in_child) {
    REQUIRE(UK(CreateProcessW));
    HANDLE ev = high_event(FALSE, TRUE); pc_opts o; char out[4096], a[64], key[32]; DWORD code;
    CHECK(ev != NULL); if (!ev) { free_junk(); return; }
    snprintf(a, sizeof a, "%llx:s", (unsigned long long)(ULONG_PTR)ev);
    memset(&o, 0, sizeof o); o.child = "pc_handles"; o.args = a;
    CHECK(pc_capture(&o, out, sizeof out, NULL, 0, &code, PC_TIMEOUT)); EQV(code, 0);
    snprintf(key, sizeof key, "h%llx", (unsigned long long)(ULONG_PTR)ev); EQV(pc_num(out, key, -1), 0);
    snprintf(key, sizeof key, "e%llx", (unsigned long long)(ULONG_PTR)ev); EQV(pc_num(out, key, -1), ERROR_INVALID_HANDLE);
    EQV(WaitForSingleObject(ev, 0), WAIT_TIMEOUT);
    free_junk();
}
TEST(pcinh_bInheritHandles_false_passes_nothing) {
    REQUIRE(UK(CreateProcessW));
    HANDLE ev = high_event(TRUE, TRUE); pc_opts o; char a[64]; DWORD code;
    CHECK(ev != NULL); if (!ev) { free_junk(); return; }
    snprintf(a, sizeof a, "%llx", (unsigned long long)(ULONG_PTR)ev);
    memset(&o, 0, sizeof o); o.child = "pc_handles_x"; o.args = a; o.inherit = FALSE;
    code = pc_run(&o, PC_TIMEOUT); EQV(code, 0);
    o.inherit = TRUE; code = pc_run(&o, PC_TIMEOUT); EQV(code, 1);
    free_junk();
}
TEST(pcinh_inherit_flag_cleared_after_creation_blocks_inheritance) {
    REQUIRE(UK(CreateProcessW));
    HANDLE ev = high_event(TRUE, TRUE); pc_opts o; char a[64]; DWORD code;
    CHECK(ev != NULL); if (!ev) { free_junk(); return; }
    snprintf(a, sizeof a, "%llx", (unsigned long long)(ULONG_PTR)ev);
    CHECK(SetHandleInformation(ev, HANDLE_FLAG_INHERIT, 0));
    memset(&o, 0, sizeof o); o.child = "pc_handles_x"; o.args = a; o.inherit = TRUE;
    code = pc_run(&o, PC_TIMEOUT); EQV(code, 0);
    CHECK(SetHandleInformation(ev, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT));
    code = pc_run(&o, PC_TIMEOUT); EQV(code, 1);
    free_junk();
}
TEST(pcinh_many_handles_inherit_with_same_values) {
    REQUIRE(UK(CreateProcessW));
    HANDLE h[6]; pc_opts o; char a[512] = ""; int i; DWORD code;
    for (i = 0; i < 6; i++) {
        SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, i % 2 == 0 }; h[i] = CreateEventW(&sa, TRUE, FALSE, NULL);
        sprintf(a + strlen(a), "%llx ", (unsigned long long)(ULONG_PTR)h[i]);
    }
    memset(&o, 0, sizeof o); o.child = "pc_handles_x"; o.args = a; o.inherit = TRUE;
    code = pc_run(&o, PC_TIMEOUT); EQV(code, 0x15);   /* bits 0,2,4 */
    for (i = 0; i < 6; i++) CloseHandle(h[i]);
}
TEST(pcinh_file_handle_shares_file_pointer) {
    REQUIRE(UK(CreateProcessW));
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE }; char f[MAX_PATH * 2], a[MAX_PATH * 2], buf[32] = ""; HANDLE h; DWORD w, code, r = 0; pc_opts o;
    t_tmpfile(f, sizeof f, "inh_file.txt");
    h = CreateFileA(f, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE); if (h == INVALID_HANDLE_VALUE) return;
    WriteFile(h, "AAA", 3, &w, NULL);
    snprintf(a, sizeof a, "%llx BBB", (unsigned long long)(ULONG_PTR)h);
    memset(&o, 0, sizeof o); o.child = "pc_filewrite"; o.args = a; o.inherit = TRUE;
    code = pc_run(&o, PC_TIMEOUT); EQV(code, 0);
    WriteFile(h, "CCC", 3, &w, NULL);
    SetFilePointer(h, 0, NULL, FILE_BEGIN); ReadFile(h, buf, 31, &r, NULL); buf[r] = 0;
    EXPECT_STR(buf, "AAABBBCCC");
    CloseHandle(h);
}
TEST(pcinh_read_only_handle_cannot_be_written_by_child) {
    REQUIRE(UK(CreateProcessW));
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE }; char f[MAX_PATH * 2], a[MAX_PATH * 2]; HANDLE h; DWORD code; pc_opts o;
    t_tmpfile(f, sizeof f, "inh_ro.txt");
    h = CreateFileA(f, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, NULL); CloseHandle(h);
    h = CreateFileA(f, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE); if (h == INVALID_HANDLE_VALUE) return;
    snprintf(a, sizeof a, "%llx XXX", (unsigned long long)(ULONG_PTR)h);
    memset(&o, 0, sizeof o); o.child = "pc_filewrite"; o.args = a; o.inherit = TRUE;
    code = pc_run(&o, PC_TIMEOUT); EQV(code, 40);
    CloseHandle(h);
}
TEST(pcinh_handle_closed_in_parent_after_launch_stays_open_in_child) {
    REQUIRE(UK(CreateProcessW));
    HANDLE ev = high_event(TRUE, TRUE); pc_opts o; PROCESS_INFORMATION pi; char a[64]; DWORD code = 0;
    CHECK(ev != NULL); if (!ev) { free_junk(); return; }
    snprintf(a, sizeof a, "%llx", (unsigned long long)(ULONG_PTR)ev);
    memset(&o, 0, sizeof o); o.child = "pc_handles_x"; o.args = a; o.inherit = TRUE; o.flags = CREATE_SUSPENDED;
    CHECK(pc_launch(&o, &pi));
    CloseHandle(ev); junk[njunk - 1] = NULL;           /* child already owns its copy */
    free_junk();
    ResumeThread(pi.hThread);
    WaitForSingleObject(pi.hProcess, PC_TIMEOUT); GetExitCodeProcess(pi.hProcess, &code); EQV(code, 1);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
}

/* ---------- STARTF_USESTDHANDLES ---------- */
TEST(pcio_stdout_and_stderr_through_separate_pipes) {
    REQUIRE(UK(CreateProcessW));
    pc_opts o; char out[4096], err[4096]; DWORD code;
    memset(&o, 0, sizeof o); o.child = "pc_stdio"; o.args = "noread"; o.flags = CREATE_NO_WINDOW;
    CHECK(pc_capture(&o, out, sizeof out, err, sizeof err, &code, PC_TIMEOUT)); EQV(code, 0);
    CHECK(pc_streq(out, "out", "hello-stdout")); CHECK(strstr(err, "ERR=hello-stderr") != NULL);
    CHECK(strstr(out, "ERR=") == NULL);
    EQV(pc_num(out, "sameouterr", -1), 0);
}
TEST(pcio_stdout_and_stderr_same_pipe) {
    REQUIRE(UK(CreateProcessW));
    pc_opts o; char out[4096]; DWORD code;
    memset(&o, 0, sizeof o); o.child = "pc_stdio"; o.args = "noread"; o.flags = CREATE_NO_WINDOW;
    CHECK(pc_capture(&o, out, sizeof out, NULL, 0, &code, PC_TIMEOUT)); EQV(code, 0);
    CHECK(pc_streq(out, "out", "hello-stdout")); CHECK(strstr(out, "ERR=hello-stderr") != NULL);
    EQV(pc_num(out, "sameouterr", -1), 1);
}
TEST(pcio_stdin_pipe_feeds_child) {
    REQUIRE(UK(CreateProcessW));
    pc_opts o; char out[4096]; DWORD code;
    memset(&o, 0, sizeof o); o.child = "pc_stdio"; o.flags = CREATE_NO_WINDOW; o.in_text = "ping pong 123";
    CHECK(pc_capture(&o, out, sizeof out, NULL, 0, &code, PC_TIMEOUT)); EQV(code, 0);
    CHECK(pc_streq(out, "in", "PING PONG 123"));
}
TEST(pcio_stdin_large_text) {
    REQUIRE(UK(CreateProcessW));
    static char big[1000]; pc_opts o; char out[4096]; DWORD code; int i;
    for (i = 0; i < 900; i++) big[i] = (char)('a' + i % 26);
    big[900] = 0;
    memset(&o, 0, sizeof o); o.child = "pc_stdio"; o.flags = CREATE_NO_WINDOW; o.in_text = big;
    CHECK(pc_capture(&o, out, sizeof out, NULL, 0, &code, PC_TIMEOUT)); EQV(code, 0);
    CHECK(strncmp(pc_val(out, "in") ? pc_val(out, "in") : "", "ABCDEFGHIJKLMNOPQRSTUVWXYZABC", 29) == 0);
}
TEST(pcio_stdout_to_file) {
    REQUIRE(UK(CreateProcessW));
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE }; char f[MAX_PATH * 2], buf[512] = ""; HANDLE h; DWORD r = 0, code; pc_opts o;
    t_tmpfile(f, sizeof f, "redir_out.txt");
    h = CreateFileA(f, GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, 0, NULL); CHECK(h != INVALID_HANDLE_VALUE);
    memset(&o, 0, sizeof o); o.child = "pc_stdio"; o.args = "noread"; o.flags = CREATE_NO_WINDOW; o.hout = h; o.herr = h; o.inherit = TRUE;
    code = pc_run(&o, PC_TIMEOUT); EQV(code, 0); CloseHandle(h);
    h = CreateFileA(f, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL); ReadFile(h, buf, 511, &r, NULL); buf[r] = 0; CloseHandle(h);
    CHECK(strstr(buf, "out=hello-stdout") != NULL); CHECK(strstr(buf, "ERR=hello-stderr") != NULL);
}
TEST(pcio_stdout_to_nul_device) {
    REQUIRE(UK(CreateProcessW));
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE }; HANDLE h = CreateFileA("NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
    pc_opts o; DWORD code;
    CHECK(h != INVALID_HANDLE_VALUE);
    memset(&o, 0, sizeof o); o.child = "pc_stdprobe"; o.flags = CREATE_NO_WINDOW; o.hin = h; o.hout = h; o.herr = h; o.inherit = TRUE;
    code = pc_run(&o, PC_TIMEOUT); EQV(code, 10);
    CloseHandle(h);
}
TEST(pcio_std_handles_without_inherit_are_not_usable) {
    REQUIRE(UK(CreateProcessW));
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE }; HANDLE r, w; pc_opts o; DWORD code;
    CHECK(CreatePipe(&r, &w, &sa, 4096));
    memset(&o, 0, sizeof o); o.child = "pc_stdprobe"; o.flags = CREATE_NO_WINDOW; o.hout = w; o.herr = w; o.inherit = FALSE;
    code = pc_run(&o, PC_TIMEOUT);
    t_note("child write through non-inherited std handle: exit 0x%lx (spawn err %lu)", code, GetLastError());
    CloseHandle(r); CloseHandle(w);
}
TEST(pcio_non_inheritable_std_handle_with_inherit_true_observed) {
    REQUIRE(UK(CreateProcessW));
    HANDLE r, w; pc_opts o; DWORD code;
    CHECK(CreatePipe(&r, &w, NULL, 4096));
    memset(&o, 0, sizeof o); o.child = "pc_stdprobe"; o.flags = CREATE_NO_WINDOW; o.hout = w; o.herr = w; o.inherit = TRUE;
    code = pc_run(&o, PC_TIMEOUT);
    t_note("child write through non-inheritable std handle: exit 0x%lx (spawn err %lu)", code, GetLastError());
    CloseHandle(r); CloseHandle(w);
}
TEST(pcio_parent_std_handles_unchanged_by_redirection) {
    REQUIRE(UK(CreateProcessW));
    HANDLE a = GetStdHandle(STD_OUTPUT_HANDLE), b = GetStdHandle(STD_INPUT_HANDLE); pc_opts o; char out[1024]; DWORD code;
    memset(&o, 0, sizeof o); o.child = "pc_stdio"; o.args = "noread"; o.flags = CREATE_NO_WINDOW;
    CHECK(pc_capture(&o, out, sizeof out, NULL, 0, &code, PC_TIMEOUT));
    CHECK(GetStdHandle(STD_OUTPUT_HANDLE) == a); CHECK(GetStdHandle(STD_INPUT_HANDLE) == b);
}
TEST(pcio_child_waits_for_pipe_eof) {
    REQUIRE(UK(CreateProcessW));
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE }; HANDLE r, w; pc_opts o; PROCESS_INFORMATION pi;
    CHECK(CreatePipe(&r, &w, &sa, 4096)); SetHandleInformation(w, HANDLE_FLAG_INHERIT, 0);
    memset(&o, 0, sizeof o); o.child = "pc_waitpipe"; o.flags = CREATE_NO_WINDOW; o.hin = r; o.hout = GetStdHandle(STD_OUTPUT_HANDLE); o.herr = GetStdHandle(STD_ERROR_HANDLE); o.inherit = TRUE;
    { DWORD fl = 0; if (!GetHandleInformation(o.hout, &fl) || !(fl & HANDLE_FLAG_INHERIT)) o.hout = r; if (!GetHandleInformation(o.herr, &fl) || !(fl & HANDLE_FLAG_INHERIT)) o.herr = r; }
    CHECK(pc_launch(&o, &pi));
    CloseHandle(r);
    EQV(WaitForSingleObject(pi.hProcess, 300), WAIT_TIMEOUT);
    CloseHandle(w);                                    /* EOF -> child returns */
    EQV(WaitForSingleObject(pi.hProcess, PC_TIMEOUT), WAIT_OBJECT_0);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
}

/* ---------- STARTUPINFO ---------- */
static void startup_run(STARTUPINFOEXW *si, DWORD flags, int ansi, char *out, size_t cap) {
    pc_opts o; DWORD code; memset(&o, 0, sizeof o);
    o.child = "pc_startup"; o.si = si; o.flags = flags | CREATE_NO_WINDOW; o.ansi = ansi;
    CHECK(pc_capture(&o, out, cap, NULL, 0, &code, PC_TIMEOUT)); EQV(code, 0);
}
TEST(pcsi_geometry_and_show_fields_reach_child) {
    REQUIRE(UK(CreateProcessW));
    STARTUPINFOEXW si; char out[4096]; const DWORD m = STARTF_USEPOSITION | STARTF_USESIZE | STARTF_USESHOWWINDOW | STARTF_USECOUNTCHARS | STARTF_USEFILLATTRIBUTE;
    memset(&si, 0, sizeof si); si.StartupInfo.cb = sizeof(STARTUPINFOW);
    si.StartupInfo.dwFlags = m; si.StartupInfo.dwX = 11; si.StartupInfo.dwY = 22; si.StartupInfo.dwXSize = 333; si.StartupInfo.dwYSize = 444;
    si.StartupInfo.wShowWindow = SW_SHOWMINNOACTIVE; si.StartupInfo.dwXCountChars = 90; si.StartupInfo.dwYCountChars = 40; si.StartupInfo.dwFillAttribute = 0x1F;
    startup_run(&si, 0, 0, out, sizeof out);
    EQV(pc_unum(out, "flags", 0) & m, m);
    EQV(pc_num(out, "x", -1), 11); EQV(pc_num(out, "y", -1), 22);
    EQV(pc_num(out, "xs", -1), 333); EQV(pc_num(out, "ys", -1), 444);
    EQV(pc_num(out, "show", -1), SW_SHOWMINNOACTIVE);
    EQV(pc_num(out, "xc", -1), 90); EQV(pc_num(out, "yc", -1), 40); EQV(pc_num(out, "fill", -1), 0x1F);
}
TEST(pcsi_same_fields_via_createprocessa) {
    REQUIRE(UK(CreateProcessA));
    STARTUPINFOEXW si; char out[4096]; const DWORD m = STARTF_USEPOSITION | STARTF_USESIZE | STARTF_USESHOWWINDOW;
    memset(&si, 0, sizeof si); si.StartupInfo.cb = sizeof(STARTUPINFOW);
    si.StartupInfo.dwFlags = m; si.StartupInfo.dwX = 5; si.StartupInfo.dwY = 6; si.StartupInfo.dwXSize = 700; si.StartupInfo.dwYSize = 800; si.StartupInfo.wShowWindow = SW_SHOWMAXIMIZED;
    startup_run(&si, 0, 1, out, sizeof out);
    EQV(pc_unum(out, "flags", 0) & m, m);
    EQV(pc_num(out, "x", -1), 5); EQV(pc_num(out, "y", -1), 6); EQV(pc_num(out, "xs", -1), 700); EQV(pc_num(out, "ys", -1), 800);
    EQV(pc_num(out, "show", -1), SW_SHOWMAXIMIZED);
}
TEST(pcsi_title_wide_and_ansi_views) {
    REQUIRE(UK(CreateProcessW));
    STARTUPINFOEXW si; char out[4096], want[100];
    memset(&si, 0, sizeof si); si.StartupInfo.cb = sizeof(STARTUPINFOW); si.StartupInfo.lpTitle = L"k32t title \x00e9\x4e2d";
    startup_run(&si, 0, 0, out, sizeof out);
    u8(L"k32t title \x00e9\x4e2d", want, sizeof want);
    CHECK(pc_streq(out, "titleW", want));
    CHECK(strstr(pc_val(out, "titleA") ? pc_val(out, "titleA") : "", "k32t title ") == pc_val(out, "titleA"));
}
TEST(pcsi_title_via_createprocessa) {
    REQUIRE(UK(CreateProcessA));
    STARTUPINFOEXW si; char out[4096];
    memset(&si, 0, sizeof si); si.StartupInfo.cb = sizeof(STARTUPINFOW); si.StartupInfo.lpTitle = L"plain ascii title";
    startup_run(&si, 0, 1, out, sizeof out);
    CHECK(pc_streq(out, "titleW", "plain ascii title")); CHECK(pc_streq(out, "titleA", "plain ascii title"));
}
TEST(pcsi_long_title) {
    REQUIRE(UK(CreateProcessW));
    STARTUPINFOEXW si; char out[4096], want[600]; WCHAR t[300]; int i;
    for (i = 0; i < 250; i++) t[i] = L'A' + i % 26;
    t[250] = 0;
    memset(&si, 0, sizeof si); si.StartupInfo.cb = sizeof(STARTUPINFOW); si.StartupInfo.lpTitle = t;
    startup_run(&si, 0, 0, out, sizeof out); u8(t, want, sizeof want);
    CHECK(pc_streq(out, "titleW", want));
}
TEST(pcsi_null_title_defaults_in_child) {
    REQUIRE(UK(CreateProcessW));
    STARTUPINFOEXW si; char out[4096];
    memset(&si, 0, sizeof si); si.StartupInfo.cb = sizeof(STARTUPINFOW);
    startup_run(&si, 0, 0, out, sizeof out);
    t_note("child title with NULL lpTitle: null=%lld value=%s", pc_num(out, "titlenull", -1), pc_val(out, "titleA") ? "(present)" : "(absent)");
}
TEST(pcsi_reserved2_block_passed_through) {
    REQUIRE(UK(CreateProcessW));
    STARTUPINFOEXW si; char out[4096]; static BYTE blk[8] = { 0, 0, 0, 0, 'A', 'B', 'C', 'D' };
    memset(&si, 0, sizeof si); si.StartupInfo.cb = sizeof(STARTUPINFOW); si.StartupInfo.cbReserved2 = 8; si.StartupInfo.lpReserved2 = blk;
    startup_run(&si, 0, 0, out, sizeof out);
    EQV(pc_num(out, "cbres2", -1), 8); EQV(pc_num(out, "cbres2A", -1), 8);
    CHECK(pc_streq(out, "res2", "0000000041424344"));
}
TEST(pcsi_reserved2_absent_by_default) {
    REQUIRE(UK(CreateProcessW));
    STARTUPINFOEXW si; char out[4096];
    memset(&si, 0, sizeof si); si.StartupInfo.cb = sizeof(STARTUPINFOW);
    startup_run(&si, 0, 0, out, sizeof out);
    EQV(pc_num(out, "cbres2", -1), 0);
}
TEST(pcsi_child_getstartupinfo_wide_ansi_parity) {
    REQUIRE(UK(CreateProcessW));
    STARTUPINFOEXW si; char out[4096];
    memset(&si, 0, sizeof si); si.StartupInfo.cb = sizeof(STARTUPINFOW);
    si.StartupInfo.dwFlags = STARTF_USEPOSITION | STARTF_USESHOWWINDOW; si.StartupInfo.dwX = 3; si.StartupInfo.dwY = 4; si.StartupInfo.wShowWindow = SW_SHOWNOACTIVATE;
    startup_run(&si, 0, 0, out, sizeof out);
    EQV(pc_num(out, "cb", -1), sizeof(STARTUPINFOW)); EQV(pc_num(out, "cbA", -1), sizeof(STARTUPINFOA));
    EQV(pc_num(out, "flags", -1), pc_num(out, "flagsA", -2));
    EQV(pc_num(out, "x", -1), pc_num(out, "xA", -2)); EQV(pc_num(out, "y", -1), pc_num(out, "yA", -2));
    EQV(pc_num(out, "xs", -1), pc_num(out, "xsA", -2)); EQV(pc_num(out, "show", -1), pc_num(out, "showA", -2));
}
TEST(pcsi_wshowwindow_without_flag_observed) {
    REQUIRE(UK(CreateProcessW));
    STARTUPINFOEXW si; char out[4096];
    memset(&si, 0, sizeof si); si.StartupInfo.cb = sizeof(STARTUPINFOW); si.StartupInfo.wShowWindow = SW_SHOWMINIMIZED;
    startup_run(&si, 0, 0, out, sizeof out);
    EQV(pc_unum(out, "flags", 0) & STARTF_USESHOWWINDOW, 0);
    t_note("wShowWindow without STARTF_USESHOWWINDOW arrives as %lld", pc_num(out, "show", -1));
}
TEST(pcsi_cb_variants_observed) {
    REQUIRE(UK(CreateProcessW));
    STARTUPINFOEXW si; pc_opts o; PROCESS_INFORMATION pi; memset(&o, 0, sizeof o);
    memset(&si, 0, sizeof si); o.child = "pc_exit77"; o.si = &si; o.flags = CREATE_SUSPENDED;
    si.StartupInfo.cb = 0;
    SET_ERR(0); if (pc_launch(&o, &pi)) { t_note("cb=0 accepted"); pc_kill(&pi); } else t_note("cb=0 rejected err %lu", GetLastError());
    si.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    SET_ERR(0); if (pc_launch(&o, &pi)) { t_note("cb=sizeof(STARTUPINFOEX) without flag accepted"); pc_kill(&pi); } else t_note("cb=sizeof(STARTUPINFOEX) without flag rejected err %lu", GetLastError());
    si.StartupInfo.cb = sizeof(STARTUPINFOW); o.flags |= EXTENDED_STARTUPINFO_PRESENT;
    SET_ERR(0); if (pc_launch(&o, &pi)) { t_note("EXTENDED flag with short cb accepted"); pc_kill(&pi); } else t_note("EXTENDED flag with short cb rejected err %lu", GetLastError());
    si.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    SET_ERR(0); CHECK(pc_launch(&o, &pi)); pc_kill(&pi);   /* EX with NULL attribute list is fine */
}

/* ---------- GetStartupInfo in this process ---------- */
TEST(pcsi_getstartupinfo_this_process_wide) {
    REQUIRE(U(GetStartupInfoW));
    STARTUPINFOW a, b;
    memset(&a, 0xCC, sizeof a); memset(&b, 0x33, sizeof b);
    U(GetStartupInfoW)(&a); U(GetStartupInfoW)(&b);
    EQV(a.cb, sizeof(STARTUPINFOW));
    CHECK(a.dwFlags == b.dwFlags && a.dwX == b.dwX && a.dwY == b.dwY && a.wShowWindow == b.wShowWindow);
    CHECK(a.lpTitle == b.lpTitle || (a.lpTitle && b.lpTitle && !lstrcmpW(a.lpTitle, b.lpTitle)));
    CHECK(a.cbReserved2 == b.cbReserved2);
}
TEST(pcsi_getstartupinfo_this_process_ansi_matches_wide) {
    REQUIRE(U(GetStartupInfoA)); REQUIRE(U(GetStartupInfoW));
    STARTUPINFOA a; STARTUPINFOW w; char conv[1000];
    memset(&a, 0xCC, sizeof a); memset(&w, 0x33, sizeof w);
    U(GetStartupInfoA)(&a); U(GetStartupInfoW)(&w);
    EQV(a.cb, sizeof(STARTUPINFOA));
    CHECK(a.dwFlags == w.dwFlags && a.dwX == w.dwX && a.dwY == w.dwY && a.dwXSize == w.dwXSize && a.dwYSize == w.dwYSize);
    CHECK(a.wShowWindow == w.wShowWindow && a.cbReserved2 == w.cbReserved2);
    if (w.lpTitle && a.lpTitle) { WideCharToMultiByte(CP_ACP, 0, w.lpTitle, -1, conv, sizeof conv, NULL, NULL); CHECK(!strcmp(conv, a.lpTitle)); }
    else CHECK((w.lpTitle == NULL) == (a.lpTitle == NULL));
    if (w.lpDesktop && a.lpDesktop) { WideCharToMultiByte(CP_ACP, 0, w.lpDesktop, -1, conv, sizeof conv, NULL, NULL); CHECK(!strcmp(conv, a.lpDesktop)); }
}
TEST(pcsi_getstartupinfo_stable_across_calls) {
    REQUIRE(U(GetStartupInfoA));
    STARTUPINFOA a, b; int i;
    U(GetStartupInfoA)(&a);
    for (i = 0; i < 5; i++) { U(GetStartupInfoA)(&b); CHECK(a.dwFlags == b.dwFlags && a.hStdInput == b.hStdInput && a.hStdOutput == b.hStdOutput && a.hStdError == b.hStdError); }
}
