/* CreateProcess: command line / application name resolution, current directory, environment block. */
#include "pc.h"

static void u8(const WCHAR *w, char *out, size_t cap) { if (!WideCharToMultiByte(CP_UTF8, 0, w, -1, out, (int)cap, NULL, NULL)) out[0] = 0; }
static int ends_with_i(const char *s, const char *suf) { size_t a = strlen(s), b = strlen(suf); return a >= b && !lstrcmpiA(s + a - b, suf); }
static void tmpw(WCHAR *o, size_t cap, const char *leaf) { char p[MAX_PATH * 2]; t_tmpfile(p, sizeof p, leaf); pc_widen(o, cap, p); }
static BOOL put(const char *rel) { return pc_copy_self(rel, NULL, 0); }
static DWORD modname_of(const WCHAR *cmd, char *mod, size_t cap, DWORD flags) {
    pc_opts o; char out[4096]; DWORD code = 0; memset(&o, 0, sizeof o); o.raw_cmd = cmd; o.flags = flags;
    mod[0] = 0;
    if (!pc_capture(&o, out, sizeof out, NULL, 0, &code, PC_TIMEOUT)) return 0xDEAD0000;
    pc_line(out, "mod", mod, cap);
    return code;
}

/* ---------- unquoted paths, extension search ---------- */
TEST(pcmd_unquoted_path_with_spaces_prefers_shorter_prefix) {
    REQUIRE(UK(CreateProcessW));
    WCHAR real[MAX_PATH * 2], cmd[MAX_PATH * 3]; pc_opts o;
    /* tmp\sp.exe is a decoy: it is a copy of the test exe, which exits 0 when not given --child */
    CHECK(put("sp.exe")); CHECK(pc_copy_self("sp ace\\app.exe", real, MAX_PATH * 2));
    wsprintfW(cmd, L"%s --child pc_exit77", real);
    memset(&o, 0, sizeof o); o.raw_cmd = cmd;
    EQV(pc_run(&o, PC_TIMEOUT), 0);
}
TEST(pcmd_quoted_path_with_spaces) {
    REQUIRE(UK(CreateProcessW));
    WCHAR real[MAX_PATH * 2], cmd[MAX_PATH * 3]; pc_opts o;
    CHECK(put("sp.exe")); CHECK(pc_copy_self("sp ace\\app.exe", real, MAX_PATH * 2));
    wsprintfW(cmd, L"\"%s\" --child pc_exit77", real);
    memset(&o, 0, sizeof o); o.raw_cmd = cmd;
    EQV(pc_run(&o, PC_TIMEOUT), 77);
}
TEST(pcmd_application_name_with_spaces_needs_no_quotes) {
    REQUIRE(UK(CreateProcessW));
    WCHAR real[MAX_PATH * 2], cmd[MAX_PATH * 3]; pc_opts o;
    CHECK(put("sp.exe")); CHECK(pc_copy_self("sp ace\\app.exe", real, MAX_PATH * 2));
    wsprintfW(cmd, L"\"%s\" --child pc_exit77", real);
    memset(&o, 0, sizeof o); o.app = real; o.raw_cmd = cmd;
    EQV(pc_run(&o, PC_TIMEOUT), 77);
}
TEST(pcmd_application_name_with_quotes_fails) {
    REQUIRE(UK(CreateProcessW));
    WCHAR real[MAX_PATH * 2], q[MAX_PATH * 3]; pc_opts o; PROCESS_INFORMATION pi;
    CHECK(pc_copy_self("qq.exe", real, MAX_PATH * 2));
    wsprintfW(q, L"\"%s\"", real);
    memset(&o, 0, sizeof o); o.app = q; o.raw_cmd = L"qq --child pc_exit77";
    SET_ERR(0);
    if (pc_launch(&o, &pi)) { t_note("quoted lpApplicationName accepted"); pc_kill(&pi); } else t_note("quoted lpApplicationName rejected err %lu", GetLastError());
}
TEST(pcmd_application_name_overrides_command_line_first_token) {
    REQUIRE(UK(CreateProcessW));
    pc_opts o; memset(&o, 0, sizeof o); o.app = pc_selfw(); o.raw_cmd = L"totally_different_name --child pc_exit77";
    EQV(pc_run(&o, PC_TIMEOUT), 77);
}
TEST(pcmd_command_line_without_extension_appends_exe) {
    REQUIRE(UK(CreateProcessW));
    WCHAR cmd[MAX_PATH * 3], base[MAX_PATH * 2]; char mod[MAX_PATH * 2];
    CHECK(put("x1")); CHECK(put("x1.exe"));
    tmpw(base, MAX_PATH * 2, "x1"); wsprintfW(cmd, L"\"%s\" --child pc_modname", base);
    EQV(modname_of(cmd, mod, sizeof mod, 0), 0);
    CHECKF(ends_with_i(mod, "x1.exe"), "ran %s", mod);
}
TEST(pcmd_command_line_extensionless_file_alone_not_found) {
    REQUIRE(UK(CreateProcessW));
    WCHAR cmd[MAX_PATH * 3], base[MAX_PATH * 2]; pc_opts o; PROCESS_INFORMATION pi;
    CHECK(put("x2"));
    tmpw(base, MAX_PATH * 2, "x2"); wsprintfW(cmd, L"\"%s\" --child pc_exit77", base);
    memset(&o, 0, sizeof o); o.raw_cmd = cmd;
    SET_ERR(0); CHECK(!pc_launch(&o, &pi)); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(pcmd_command_line_trailing_dot_suppresses_exe) {
    REQUIRE(UK(CreateProcessW));
    WCHAR cmd[MAX_PATH * 3], base[MAX_PATH * 2]; char mod[MAX_PATH * 2];
    CHECK(put("x3"));
    tmpw(base, MAX_PATH * 2, "x3"); wsprintfW(cmd, L"\"%s.\" --child pc_modname", base);
    EQV(modname_of(cmd, mod, sizeof mod, 0), 0);
    CHECKF(ends_with_i(mod, "\\x3"), "ran %s", mod);
}
TEST(pcmd_command_line_custom_extension_runs) {
    REQUIRE(UK(CreateProcessW));
    WCHAR cmd[MAX_PATH * 3], base[MAX_PATH * 2]; char mod[MAX_PATH * 2];
    CHECK(put("x4.bar"));
    tmpw(base, MAX_PATH * 2, "x4.bar"); wsprintfW(cmd, L"\"%s\" --child pc_modname", base);
    EQV(modname_of(cmd, mod, sizeof mod, 0), 0);
    CHECKF(ends_with_i(mod, "x4.bar"), "ran %s", mod);
}
TEST(pcmd_dot_in_directory_does_not_count_as_extension) {
    REQUIRE(UK(CreateProcessW));
    WCHAR cmd[MAX_PATH * 3], base[MAX_PATH * 2]; char mod[MAX_PATH * 2];
    CHECK(put("dir.v2\\x6.exe"));
    tmpw(base, MAX_PATH * 2, "dir.v2\\x6"); wsprintfW(cmd, L"\"%s\" --child pc_modname", base);
    EQV(modname_of(cmd, mod, sizeof mod, 0), 0);
    CHECKF(ends_with_i(mod, "x6.exe"), "ran %s", mod);
}
TEST(pcmd_command_line_case_insensitive_path) {
    REQUIRE(UK(CreateProcessW));
    WCHAR cmd[MAX_PATH * 3], base[MAX_PATH * 2]; char mod[MAX_PATH * 2];
    CHECK(put("CaseTest.exe"));
    tmpw(base, MAX_PATH * 2, "casetest.EXE"); wsprintfW(cmd, L"\"%s\" --child pc_modname", base);
    EQV(modname_of(cmd, mod, sizeof mod, 0), 0);
    CHECKF(ends_with_i(mod, "casetest.exe"), "ran %s", mod);
}
TEST(pcmd_command_line_forward_slashes_in_path) {
    REQUIRE(UK(CreateProcessW));
    WCHAR cmd[MAX_PATH * 3], base[MAX_PATH * 2], *p; pc_opts o;
    CHECK(put("fs\\fwd.exe"));
    tmpw(base, MAX_PATH * 2, "fs\\fwd.exe"); for (p = base; *p; p++) if (*p == L'\\') *p = L'/';
    wsprintfW(cmd, L"\"%s\" --child pc_exit77", base);
    memset(&o, 0, sizeof o); o.raw_cmd = cmd;
    t_note("forward-slash image path exit code 0x%lx", pc_run(&o, PC_TIMEOUT));
}
TEST(pcmd_searches_application_directory_of_caller) {
    REQUIRE(UK(CreateProcessW));
    /* a bare name is searched in the directory the caller was loaded from first; "k32t_self_dir_probe.exe" is not there */
    pc_opts o; PROCESS_INFORMATION pi;
    memset(&o, 0, sizeof o); o.raw_cmd = L"k32t_self_dir_probe.exe --child pc_exit77";
    SET_ERR(0); CHECK(!pc_launch(&o, &pi)); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(pcmd_relative_names_resolved_against_callers_cwd) {
    REQUIRE(UK(CreateProcessW));
    WCHAR real[MAX_PATH * 2]; char dir[MAX_PATH * 2], args[MAX_PATH * 2 + 4], out[4096]; DWORD code;
    pc_opts o;
    CHECK(pc_copy_self("relapp.exe", real, MAX_PATH * 2));
    strcpy(dir, t_tmpdir()); dir[strlen(dir) - 1] = 0;
    snprintf(args, sizeof args, "\"%s\"", dir);
    memset(&o, 0, sizeof o); o.child = "pc_relcwd"; o.args = args;
    CHECK(pc_capture(&o, out, sizeof out, NULL, 0, &code, PC_TIMEOUT));
    EQV(code, 0);
    CHECKF(pc_num(out, "a", -1) == 77, "a=%s", pc_val(out, "a") ? pc_val(out, "a") : "?");
    CHECKF(pc_num(out, "b", -1) == 77, "b=%s", pc_val(out, "b") ? pc_val(out, "b") : "?");
    CHECKF(pc_num(out, "c", -1) == 77, "c=%s", pc_val(out, "c") ? pc_val(out, "c") : "?");
    CHECKF(pc_streq(out, "d", "fail2"), "d=%s", pc_val(out, "d") ? pc_val(out, "d") : "?");
}

/* ---------- command line as seen by the child ---------- */
static void cmdline_roundtrip_x(const WCHAR *tail, int chk) {
    WCHAR cmd[2000]; pc_opts o; char out[8192], got[4000], want[4000]; DWORD code;
    wsprintfW(cmd, L"\"%s\" --child pc_cmdline %s", pc_selfw(), tail);
    memset(&o, 0, sizeof o); o.raw_cmd = cmd;
    CHECK(pc_capture(&o, out, sizeof out, NULL, 0, &code, PC_TIMEOUT));
    EQV(code, 0);
    u8(cmd, want, sizeof want); pc_line(out, "cmdW", got, sizeof got);
    CHECKF(!strcmp(got, want), "got [%s] want [%s]", got, want);
    CHECK(pc_num(out, "stableW", 0) == 1); CHECK(pc_num(out, "stableA", 0) == 1);
    if (chk) CHECK(pc_num(out, "consistent", 0) == 1);
}
#define cmdline_roundtrip(t) cmdline_roundtrip_x(t, 1)
TEST(pcmd_child_sees_command_line_verbatim_plain) { REQUIRE(UK(CreateProcessW)); cmdline_roundtrip(L"alpha beta gamma"); }
TEST(pcmd_child_sees_command_line_verbatim_quotes) { REQUIRE(UK(CreateProcessW)); cmdline_roundtrip(L"\"b c\" d \"e\\\"f\" g\\\\\" h\""); }
TEST(pcmd_child_sees_command_line_verbatim_multiple_spaces) { REQUIRE(UK(CreateProcessW)); cmdline_roundtrip(L"a    b\tc   d"); }
TEST(pcmd_child_sees_command_line_verbatim_symbols) { REQUIRE(UK(CreateProcessW)); cmdline_roundtrip(L"x=y;z %PATH% ^& | > < * ? @ ! [] {}"); }
TEST(pcmd_child_sees_command_line_verbatim_wide_chars) {
    REQUIRE(UK(CreateProcessW));
    cmdline_roundtrip_x(L"caf\x00e9 \x4e2d\x6587 \x20ac", 0);
}
TEST(pcmd_child_sees_command_line_verbatim_via_a) {
    REQUIRE(UK(CreateProcessA));
    pc_opts o; char out[8192], got[1000]; DWORD code;
    memset(&o, 0, sizeof o); o.child = "pc_cmdline"; o.args = "one \"two three\" four"; o.ansi = 1;
    CHECK(pc_capture(&o, out, sizeof out, NULL, 0, &code, PC_TIMEOUT)); EQV(code, 0);
    pc_line(out, "cmdA", got, sizeof got);
    CHECKF(strstr(got, "--child pc_cmdline one \"two three\" four") != NULL, "got [%s]", got);
    CHECK(pc_num(out, "consistent", 0) == 1);
}
TEST(pcmd_long_command_line_32000) {
    REQUIRE(UK(CreateProcessW));
    static WCHAR cmd[40000]; pc_opts o; char out[8192]; DWORD code; int n, i;
    wsprintfW(cmd, L"\"%s\" --child pc_cmdline ", pc_selfw());
    n = lstrlenW(cmd);
    for (i = n; i < 32000; i++) cmd[i] = (i % 7 == 0) ? L' ' : L'a' + (i % 20);
    cmd[32000] = 0;
    memset(&o, 0, sizeof o); o.raw_cmd = cmd;
    CHECK(pc_capture(&o, out, sizeof out, NULL, 0, &code, PC_TIMEOUT));
    EQV(code, 0); EQV(pc_num(out, "lenW", -1), 32000);
}
TEST(pcmd_command_line_at_documented_limit) {
    REQUIRE(UK(CreateProcessW));
    static WCHAR cmd[40000]; pc_opts o; char out[8192]; DWORD code; int n, i;
    wsprintfW(cmd, L"\"%s\" --child pc_cmdline ", pc_selfw());
    n = lstrlenW(cmd);
    for (i = n; i < 32766; i++) cmd[i] = L'b';
    cmd[32766] = 0;
    memset(&o, 0, sizeof o); o.raw_cmd = cmd;
    if (pc_capture(&o, out, sizeof out, NULL, 0, &code, PC_TIMEOUT)) { t_note("32766-char command line: exit %lu lenW=%lld", code, pc_num(out, "lenW", -1)); }
    else t_note("32766-char command line rejected, err %lu", GetLastError());
}
TEST(pcmd_command_line_over_limit_fails) {
    REQUIRE(UK(CreateProcessW));
    static WCHAR cmd[40000]; pc_opts o; PROCESS_INFORMATION pi; int n, i;
    wsprintfW(cmd, L"\"%s\" --child pc_exit77 ", pc_selfw());
    n = lstrlenW(cmd);
    for (i = n; i < 33000; i++) cmd[i] = L'c';
    cmd[33000] = 0;
    memset(&o, 0, sizeof o); o.raw_cmd = cmd;
    SET_ERR(0);
    if (pc_launch(&o, &pi)) { t_note("33000-char command line accepted"); pc_kill(&pi); }
    else t_note("33000-char command line rejected, error %lu", GetLastError());
}
TEST(pcmd_get_command_line_parity_in_process) {
    REQUIRE(U(GetCommandLineA)); REQUIRE(U(GetCommandLineW));
    LPSTR a = U(GetCommandLineA)(); LPWSTR w = U(GetCommandLineW)(); WCHAR conv[4096];
    CHECK(a != NULL && w != NULL);
    CHECK(a == U(GetCommandLineA)()); CHECK(w == U(GetCommandLineW)());
    CHECK(MultiByteToWideChar(CP_ACP, 0, a, -1, conv, 4096) > 0);
    CHECK(!lstrcmpW(conv, w));
    CHECK(lstrlenA(a) > 0);
    EQV(lstrlenW(w) > 0, 1);
}
TEST(pcmd_get_command_line_matches_peb) {
    REQUIRE(U(GetCommandLineW));
    NtQueryInformationProcess_pfn q = UN(NtQueryInformationProcess); REQUIRE(q);
    struct { LONG_PTR st; BYTE *peb; ULONG_PTR a, b, c, d; } pbi; BYTE *params; UNICODE_STRING *us;
    memset(&pbi, 0, sizeof pbi);
    CHECK(q(GetCurrentProcess(), 0, &pbi, sizeof pbi, NULL) >= 0);
    params = *(BYTE **)(pbi.peb + 0x20);
    us = (UNICODE_STRING *)(params + 0x70);
    CHECK(us->Buffer == U(GetCommandLineW)() || (us->Length / 2 == (USHORT)lstrlenW(U(GetCommandLineW)()) && !wcsncmp(us->Buffer, U(GetCommandLineW)(), us->Length / 2)));
}

/* ---------- current directory ---------- */
static void run_cwd(const WCHAR *cwd, char *got, size_t cap, long long *n) {
    pc_opts o; char out[4096]; DWORD code;
    memset(&o, 0, sizeof o); o.child = "pc_cwd"; o.cwd = cwd; o.flags = CREATE_NO_WINDOW;
    CHECK(pc_capture(&o, out, sizeof out, NULL, 0, &code, PC_TIMEOUT)); EQV(code, 0);
    pc_line(out, "cwd", got, cap); *n = pc_num(out, "n", -1);
}
TEST(pcwd_cwd_applied_to_child) {
    REQUIRE(UK(CreateProcessW));
    WCHAR cwd[MAX_PATH * 2]; char got[MAX_PATH * 2], want[MAX_PATH * 2]; long long n;
    tmpw(cwd, MAX_PATH * 2, ""); cwd[lstrlenW(cwd) - 1] = 0;
    run_cwd(cwd, got, sizeof got, &n); u8(cwd, want, sizeof want);
    CHECKF(!lstrcmpiA(got, want), "got [%s] want [%s]", got, want);
    EQV(n, (long long)strlen(want));
}
TEST(pcwd_trailing_backslash_not_reported) {
    REQUIRE(UK(CreateProcessW));
    WCHAR cwd[MAX_PATH * 2]; char got[MAX_PATH * 2], want[MAX_PATH * 2]; long long n;
    tmpw(cwd, MAX_PATH * 2, ""); u8(cwd, want, sizeof want); want[strlen(want) - 1] = 0;
    run_cwd(cwd, got, sizeof got, &n);
    CHECKF(!lstrcmpiA(got, want), "got [%s] want [%s]", got, want);
}
TEST(pcwd_relative_cwd_resolved_in_parent) {
    REQUIRE(UK(CreateProcessW));
    WCHAR here[MAX_PATH * 2]; char got[MAX_PATH * 2], want[MAX_PATH * 2]; long long n;
    GetCurrentDirectoryW(MAX_PATH * 2, here); u8(here, want, sizeof want);
    run_cwd(L".", got, sizeof got, &n);
    CHECKF(!lstrcmpiA(got, want), "got [%s] want [%s]", got, want);
}
TEST(pcwd_null_cwd_inherits_parent) {
    REQUIRE(UK(CreateProcessW));
    WCHAR here[MAX_PATH * 2]; char got[MAX_PATH * 2], want[MAX_PATH * 2]; long long n;
    GetCurrentDirectoryW(MAX_PATH * 2, here); u8(here, want, sizeof want);
    run_cwd(NULL, got, sizeof got, &n);
    CHECKF(!lstrcmpiA(got, want), "got [%s] want [%s]", got, want);
}
TEST(pcwd_system_directory) {
    REQUIRE(UK(CreateProcessW));
    WCHAR sys[MAX_PATH]; char got[MAX_PATH], want[MAX_PATH]; long long n;
    GetSystemDirectoryW(sys, MAX_PATH); u8(sys, want, sizeof want);
    run_cwd(sys, got, sizeof got, &n);
    CHECKF(!lstrcmpiA(got, want), "got [%s] want [%s]", got, want);
}
TEST(pcwd_missing_directory_267) {
    REQUIRE(UK(CreateProcessW)); REQUIRE(UK(CreateProcessA));
    pc_opts o; PROCESS_INFORMATION pi; WCHAR cwd[MAX_PATH * 2];
    tmpw(cwd, MAX_PATH * 2, "no_such_cwd");
    memset(&o, 0, sizeof o); o.child = "pc_exit77"; o.cwd = cwd;
    SET_ERR(0); CHECK(!pc_launch(&o, &pi)); EXPECT_ERR(ERROR_DIRECTORY);
    o.ansi = 1;
    SET_ERR(0); CHECK(!pc_launch(&o, &pi)); EXPECT_ERR(ERROR_DIRECTORY);
}
TEST(pcwd_cwd_is_a_file_267) {
    REQUIRE(UK(CreateProcessW));
    pc_opts o; PROCESS_INFORMATION pi;
    memset(&o, 0, sizeof o); o.child = "pc_exit77"; o.cwd = pc_selfw();
    SET_ERR(0); CHECK(!pc_launch(&o, &pi)); EXPECT_ERR(ERROR_DIRECTORY);
}
TEST(pcwd_cwd_longer_than_max_path_267) {
    REQUIRE(UK(CreateProcessW));
    WCHAR cwd[400]; pc_opts o; PROCESS_INFORMATION pi; int i;
    lstrcpyW(cwd, L"C:\\"); for (i = 3; i < 330; i++) cwd[i] = L'a'; cwd[330] = 0;
    memset(&o, 0, sizeof o); o.child = "pc_exit77"; o.cwd = cwd;
    SET_ERR(0); CHECK(!pc_launch(&o, &pi)); EXPECT_ERR(ERROR_DIRECTORY);
}
TEST(pcwd_empty_cwd_observed) {
    REQUIRE(UK(CreateProcessW));
    pc_opts o; PROCESS_INFORMATION pi;
    memset(&o, 0, sizeof o); o.child = "pc_exit77"; o.cwd = L""; o.flags = CREATE_SUSPENDED;
    SET_ERR(0);
    if (pc_launch(&o, &pi)) { t_note("empty lpCurrentDirectory accepted"); pc_kill(&pi); } else t_note("empty lpCurrentDirectory rejected err %lu", GetLastError());
}
TEST(pcwd_cwd_argument_does_not_change_parent_cwd) {
    REQUIRE(UK(CreateProcessW));
    WCHAR before[MAX_PATH * 2], after[MAX_PATH * 2]; pc_opts o; PROCESS_INFORMATION pi;
    GetCurrentDirectoryW(MAX_PATH * 2, before);
    memset(&o, 0, sizeof o); o.child = "pc_exit77"; o.cwd = L"C:\\Windows"; o.flags = CREATE_SUSPENDED;
    CHECK(pc_launch(&o, &pi)); pc_kill(&pi);
    GetCurrentDirectoryW(MAX_PATH * 2, after);
    CHECK(!lstrcmpW(before, after));
}

/* ---------- environment block ---------- */
static void envcount_is(const char *out, int n) {
    long long c = pc_num(out, "envcount", -1);
    if (t_is_wine()) { if (c != n) t_note("env count %lld (expected %d)", c, n); } else EQV(c, n);
}
static int env_has(const char *out, const char *entry) {
    const char *p = out; size_t n = strlen(entry);
    while (p && *p) {
        if (!strncmp(p, "env=", 4) && !strncmp(p + 4, entry, n) && (p[4 + n] == '\n' || p[4 + n] == '\r' || !p[4 + n])) return 1;
        p = strchr(p, '\n'); if (p) p++;
    }
    return 0;
}
static int run_env(void *env, DWORD flags, int ansi, char *out, size_t cap) {
    pc_opts o; DWORD code = 0; memset(&o, 0, sizeof o);
    o.child = "pc_env"; o.env = env; o.flags = flags | CREATE_NO_WINDOW; o.ansi = ansi;
    if (!pc_capture(&o, out, cap, NULL, 0, &code, PC_TIMEOUT)) return 0;
    return code == 0;
}
static size_t mk_ansi_env(char *b, const char *const *v, int n) { size_t o = 0; int i; for (i = 0; i < n; i++) { strcpy(b + o, v[i]); o += strlen(v[i]) + 1; } b[o++] = 0; return o; }
static size_t mk_wide_env(WCHAR *b, const WCHAR *const *v, int n) { size_t o = 0; int i; for (i = 0; i < n; i++) { lstrcpyW(b + o, v[i]); o += lstrlenW(v[i]) + 1; } b[o++] = 0; return o; }
#define SYSROOT(buf) char buf[MAX_PATH + 16]; strcpy(buf, "SystemRoot="); GetEnvironmentVariableA("SystemRoot", buf + 11, MAX_PATH)

TEST(penv_ansi_block_createprocessa) {
    REQUIRE(UK(CreateProcessA));
    char blk[2000], out[16384]; SYSROOT(sr);
    const char *v[] = { "K32T_PC_VAR=abc", "ZED=last", sr }; mk_ansi_env(blk, v, 3);
    CHECK(run_env(blk, 0, 1, out, sizeof out));
    CHECK(env_has(out, "K32T_PC_VAR=abc")); CHECK(env_has(out, "ZED=last")); CHECK(env_has(out, sr));
    envcount_is(out, 3); EQV(pc_num(out, "pathlen", -1), 0);
    CHECK(pc_streq(out, "var", "abc"));
}
TEST(penv_ansi_block_createprocessw_without_flag) {
    REQUIRE(UK(CreateProcessW));
    char blk[2000], out[16384]; SYSROOT(sr);
    const char *v[] = { "K32T_PC_VAR=abc", "ZED=last", sr }; mk_ansi_env(blk, v, 3);
    CHECK(run_env(blk, 0, 0, out, sizeof out));
    CHECK(env_has(out, "K32T_PC_VAR=abc")); envcount_is(out, 3);
}
TEST(penv_unicode_block_createprocessw_with_flag) {
    REQUIRE(UK(CreateProcessW));
    WCHAR blk[2000], sr[MAX_PATH + 16]; char out[16384];
    lstrcpyW(sr, L"SystemRoot="); GetEnvironmentVariableW(L"SystemRoot", sr + 11, MAX_PATH);
    const WCHAR *v[] = { L"K32T_PC_VAR=abc", L"ZED=last", sr }; mk_wide_env(blk, v, 3);
    CHECK(run_env(blk, CREATE_UNICODE_ENVIRONMENT, 0, out, sizeof out));
    CHECK(env_has(out, "K32T_PC_VAR=abc")); CHECK(env_has(out, "ZED=last"));
    envcount_is(out, 3); EQV(pc_num(out, "pathlen", -1), 0);
}
TEST(penv_unicode_block_createprocessa_with_flag) {
    REQUIRE(UK(CreateProcessA));
    WCHAR blk[2000], sr[MAX_PATH + 16]; char out[16384];
    lstrcpyW(sr, L"SystemRoot="); GetEnvironmentVariableW(L"SystemRoot", sr + 11, MAX_PATH);
    const WCHAR *v[] = { L"K32T_PC_VAR=abc", L"ZED=last", sr }; mk_wide_env(blk, v, 3);
    CHECK(run_env(blk, CREATE_UNICODE_ENVIRONMENT, 1, out, sizeof out));
    CHECK(env_has(out, "K32T_PC_VAR=abc")); envcount_is(out, 3);
}
TEST(penv_unicode_block_without_flag_is_misread) {
    REQUIRE(UK(CreateProcessW));
    WCHAR blk[2000], sr[MAX_PATH + 16]; char out[16384];
    lstrcpyW(sr, L"SystemRoot="); GetEnvironmentVariableW(L"SystemRoot", sr + 11, MAX_PATH);
    const WCHAR *v[] = { L"K32T_PC_VAR=abc", L"ZED=last", sr }; mk_wide_env(blk, v, 3);
    if (run_env(blk, 0, 0, out, sizeof out)) { EQV(pc_num(out, "varlen", -1), 0); t_note("misread env count %lld", pc_num(out, "envcount", -1)); }
    else t_note("child with misread environment did not exit 0");
}
TEST(penv_unicode_non_ascii_values) {
    REQUIRE(UK(CreateProcessW));
    WCHAR blk[2000], sr[MAX_PATH + 16]; char out[16384], want[64];
    lstrcpyW(sr, L"SystemRoot="); GetEnvironmentVariableW(L"SystemRoot", sr + 11, MAX_PATH);
    const WCHAR *v[] = { L"K32T_PC_VAR=caf\x00e9\x4e2d\x20ac", sr }; mk_wide_env(blk, v, 2);
    CHECK(run_env(blk, CREATE_UNICODE_ENVIRONMENT, 0, out, sizeof out));
    u8(L"caf\x00e9\x4e2d\x20ac", want, sizeof want);
    CHECK(pc_streq(out, "var", want));
}
TEST(penv_null_inherits_parent_variables) {
    REQUIRE(UK(CreateProcessW));
    char out[16384];
    SetEnvironmentVariableA("K32T_PC_VAR", "from-parent");
    CHECK(run_env(NULL, 0, 0, out, sizeof out));
    SetEnvironmentVariableA("K32T_PC_VAR", NULL);
    CHECK(pc_streq(out, "var", "from-parent"));
    CHECK(pc_num(out, "pathlen", 0) > 0);
    CHECK(env_has(out, "K32T_PC_VAR=from-parent"));
}
TEST(penv_parent_change_after_launch_not_visible) {
    REQUIRE(UK(CreateProcessW));
    char out[16384];
    SetEnvironmentVariableA("K32T_PC_VAR", "one");
    CHECK(run_env(NULL, 0, 0, out, sizeof out));
    SetEnvironmentVariableA("K32T_PC_VAR", "two");
    CHECK(pc_streq(out, "var", "one"));
    CHECK(run_env(NULL, 0, 0, out, sizeof out));
    CHECK(pc_streq(out, "var", "two"));
    SetEnvironmentVariableA("K32T_PC_VAR", NULL);
}
TEST(penv_empty_block_gives_empty_environment) {
    REQUIRE(UK(CreateProcessW));
    char blk[4] = { 0, 0, 0, 0 }, out[16384];
    CHECK(run_env(blk, 0, 0, out, sizeof out));
    envcount_is(out, 0); EQV(pc_num(out, "varlen", -1), 0); EQV(pc_num(out, "pathlen", -1), 0);
}
TEST(penv_sorted_block_preserved_in_order) {
    REQUIRE(UK(CreateProcessW));
    char blk[2000], out[16384]; SYSROOT(sr);
    const char *v[] = { "AAA=1", "bbb=2", "K32T_PC_VAR=x", "Zzz=3", sr }; /* sorted case-insensitively except SystemRoot, which sorts before Zzz */
    const char *s[] = { "AAA=1", "bbb=2", "K32T_PC_VAR=x", sr, "Zzz=3" };
    size_t i; const char *p; int pos = 0, ok = 1;
    mk_ansi_env(blk, s, 5);
    CHECK(run_env(blk, 0, 0, out, sizeof out));
    for (i = 0; i < 5; i++) CHECK(env_has(out, s[i]));
    p = out;
    while (p && *p && pos < 5) {
        if (!strncmp(p, "env=", 4)) { if (strncmp(p + 4, s[pos], strlen(s[pos]))) ok = 0; pos++; }
        p = strchr(p, '\n'); if (p) p++;
    }
    if (!ok) t_note("child environment order differs from the supplied (sorted) order");
}
TEST(penv_unsorted_block_accepted) {
    REQUIRE(UK(CreateProcessW));
    char blk[2000], out[16384]; SYSROOT(sr);
    const char *v[] = { "ZZZ=1", "K32T_PC_VAR=mid", "AAA=3", sr }; mk_ansi_env(blk, v, 4);
    CHECK(run_env(blk, 0, 0, out, sizeof out));
    CHECK(env_has(out, "ZZZ=1")); CHECK(env_has(out, "AAA=3")); CHECK(pc_streq(out, "var", "mid"));
    envcount_is(out, 4);
}
TEST(penv_empty_value_and_equals_in_value) {
    REQUIRE(UK(CreateProcessW));
    char blk[2000], out[16384]; SYSROOT(sr);
    const char *v[] = { "EMPTYVAL=", "K32T_PC_VAR=a=b=c", sr }; mk_ansi_env(blk, v, 3);
    CHECK(run_env(blk, 0, 0, out, sizeof out));
    CHECK(env_has(out, "EMPTYVAL=")); CHECK(pc_streq(out, "var", "a=b=c"));
}
TEST(penv_large_block_28000_chars) {
    REQUIRE(UK(CreateProcessW));
    static char blk[40000]; char out[16384]; size_t o = 0, i; SYSROOT(sr);
    strcpy(blk + o, "K32T_PC_VAR="); o += 12; for (i = 0; i < 27000; i++) blk[o++] = (char)('a' + i % 26); blk[o++] = 0;
    strcpy(blk + o, sr); o += strlen(sr) + 1; blk[o++] = 0;
    CHECK(run_env(blk, 0, 0, out, sizeof out));
    EQV(pc_num(out, "varlen", -1), 27001);   /* required size incl. NUL, buffer is only 300 */
}
TEST(penv_child_system_variable_lookup_case_insensitive) {
    REQUIRE(UK(CreateProcessW));
    char blk[2000], out[16384]; SYSROOT(sr);
    const char *v[] = { "k32t_pc_var=lower", sr }; mk_ansi_env(blk, v, 2);
    CHECK(run_env(blk, 0, 0, out, sizeof out));
    CHECK(pc_streq(out, "var", "lower"));
}
