/* CreateProcessAsUserA, UMS, Ds*, DefineDosDeviceA, CreateTapePartition */
#include "g2.h"
#include <rpc.h>
#include <ntdsapi.h>

/* ---------------- CreateProcessAsUserA ---------------- */
typedef BOOL (WINAPI *cpau_fn)(HANDLE, LPCSTR, LPSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD, LPVOID, LPCSTR, LPSTARTUPINFOA, LPPROCESS_INFORMATION);

CHILD(gap2_echo) { return argc > 1 ? atoi(argv[1]) : 0; }

static BOOL launch_echo(cpau_fn f, HANDLE tok, int code, DWORD flags, PROCESS_INFORMATION *pi)
{
    char cmd[1024]; STARTUPINFOA si;
    memset(&si, 0, sizeof si); si.cb = sizeof si;
    memset(pi, 0, sizeof *pi);
    snprintf(cmd, sizeof cmd, "\"%s\" --child gap2_echo %d", t_self_path(), code);
    return f(tok, NULL, cmd, NULL, NULL, FALSE, flags, NULL, NULL, &si, pi);
}
static DWORD wait_exit(PROCESS_INFORMATION *pi)
{
    DWORD code = 0xDEAD0001;
    if (WaitForSingleObject(pi->hProcess, 20000) == WAIT_OBJECT_0) GetExitCodeProcess(pi->hProcess, &code);
    else TerminateProcess(pi->hProcess, 0xDEAD0001);
    CloseHandle(pi->hProcess); CloseHandle(pi->hThread);
    return code;
}

TEST(gap2_cpau_null_token_runs_child) {
    cpau_fn f = (cpau_fn)t_proc("CreateProcessAsUserA");
    PROCESS_INFORMATION pi;
    REQUIRE(f);
    BOOL ok = launch_echo(f, NULL, 42, 0, &pi);
    t_note("CreateProcessAsUserA(NULL token): ok=%d err=%lu", ok, ok ? 0 : GetLastError());
    CHECK(ok);
    if (ok) {
        CHECK(pi.hProcess && pi.hThread && pi.dwProcessId && pi.dwThreadId);
        { DWORD xc = wait_exit(&pi); EXPECT_EQ(xc, 42); }
    }
}
TEST(gap2_cpau_suspended_resume_count) {
    cpau_fn f = (cpau_fn)t_proc("CreateProcessAsUserA");
    PROCESS_INFORMATION pi;
    REQUIRE(f);
    BOOL ok = launch_echo(f, NULL, 17, CREATE_SUSPENDED, &pi);
    CHECK(ok); if (!ok) return;
    EXPECT_EQ(WaitForSingleObject(pi.hProcess, 300), WAIT_TIMEOUT);
    { DWORD prev = ResumeThread(pi.hThread); EXPECT_EQ(prev, 1); }
    { DWORD xc = wait_exit(&pi); EXPECT_EQ(xc, 17); }
}
TEST(gap2_cpau_restricted_token) {
    cpau_fn f = (cpau_fn)t_proc("CreateProcessAsUserA");
    BOOL (WINAPI *crt)(HANDLE, DWORD, DWORD, PSID_AND_ATTRIBUTES, DWORD, PLUID_AND_ATTRIBUTES, DWORD, PSID_AND_ATTRIBUTES, PHANDLE) = (void *)t_proc("CreateRestrictedToken");
    HANDLE src, rt = NULL; PROCESS_INFORMATION pi;
    REQUIRE(f); REQUIRE(crt);
    src = g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_ASSIGN_PRIMARY);
    if (!src) { CHECK(0); return; }
    CHECK(crt(src, DISABLE_MAX_PRIVILEGE, 0, NULL, 0, NULL, 0, NULL, &rt));
    if (rt) {
        BOOL ok = launch_echo(f, rt, 23, 0, &pi);
        t_note("restricted-token child: ok=%d err=%lu", ok, ok ? 0 : GetLastError());
        CHECK(ok);
        if (ok) { DWORD xc = wait_exit(&pi); EXPECT_EQ(xc, 23); }
        CloseHandle(rt);
    }
    CloseHandle(src);
}
TEST(gap2_cpau_duplicate_primary_token) {
    cpau_fn f = (cpau_fn)t_proc("CreateProcessAsUserA");
    HANDLE src, dup = NULL; PROCESS_INFORMATION pi;
    REQUIRE(f);
    src = g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY);
    if (!src) { CHECK(0); return; }
    if (DuplicateTokenEx(src, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenPrimary, &dup)) {
        BOOL ok = launch_echo(f, dup, 5, 0, &pi);
        t_note("duplicate primary token child: ok=%d err=%lu", ok, ok ? 0 : GetLastError());
        if (ok) { DWORD xc = wait_exit(&pi); EXPECT_EQ(xc, 5); }
        CloseHandle(dup);
    }
    CloseHandle(src);
}
TEST(gap2_cpau_impersonation_token_rejected) {
    cpau_fn f = (cpau_fn)t_proc("CreateProcessAsUserA");
    HANDLE src, imp = NULL; PROCESS_INFORMATION pi;
    REQUIRE(f);
    src = g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY);
    if (!src) { CHECK(0); return; }
    if (DuplicateTokenEx(src, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenImpersonation, &imp)) {
        SET_ERR(0);
        BOOL ok = launch_echo(f, imp, 5, 0, &pi);
        CHECK(!ok);
        t_note("impersonation token: err=%lu", GetLastError());
        if (ok) wait_exit(&pi);
        CloseHandle(imp);
    }
    CloseHandle(src);
}
TEST(gap2_cpau_token_without_assign_primary) {
    cpau_fn f = (cpau_fn)t_proc("CreateProcessAsUserA");
    HANDLE src, dup = NULL; PROCESS_INFORMATION pi;
    REQUIRE(f);
    src = g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY);
    if (!src) { CHECK(0); return; }
    if (DuplicateTokenEx(src, TOKEN_QUERY | TOKEN_DUPLICATE, NULL, SecurityImpersonation, TokenPrimary, &dup)) {
        SET_ERR(0);
        BOOL ok = launch_echo(f, dup, 5, 0, &pi);
        CHECK(!ok);
        t_note("token without TOKEN_ASSIGN_PRIMARY: err=%lu", GetLastError());
        if (ok) wait_exit(&pi);
        CloseHandle(dup);
    }
    CloseHandle(src);
}
TEST(gap2_cpau_invalid_tokens) {
    cpau_fn f = (cpau_fn)t_proc("CreateProcessAsUserA");
    PROCESS_INFORMATION pi; HANDLE ev;
    REQUIRE(f);
    SET_ERR(0);
    CHECK(!launch_echo(f, (HANDLE)(ULONG_PTR)0x7FFFFFF0, 1, 0, &pi));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    CHECK(!launch_echo(f, ev, 1, 0, &pi));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
    SET_ERR(0);
    BOOL ok = launch_echo(f, INVALID_HANDLE_VALUE, 1, 0, &pi);
    CHECK(!ok);
    t_note("INVALID_HANDLE_VALUE token: err=%lu", GetLastError());
    if (ok) wait_exit(&pi);
}
TEST(gap2_cpau_no_application_no_command_line) {
    cpau_fn f = (cpau_fn)t_proc("CreateProcessAsUserA");
    STARTUPINFOA si; PROCESS_INFORMATION pi;
    REQUIRE(f);
    if (t_is_wine()) { t_skip("emulator faults on NULL command line"); return; }
    memset(&si, 0, sizeof si); si.cb = sizeof si;
    SET_ERR(0);
    CHECK(!f(NULL, NULL, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap2_cpau_missing_file) {
    cpau_fn f = (cpau_fn)t_proc("CreateProcessAsUserA");
    STARTUPINFOA si; PROCESS_INFORMATION pi; char app[MAX_PATH];
    REQUIRE(f);
    memset(&si, 0, sizeof si); si.cb = sizeof si;
    t_tmpfile(app, sizeof app, "gap2_missing.exe");
    SET_ERR(0);
    CHECK(!f(NULL, app, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi));
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    snprintf(app, sizeof app, "%sno_such_dir_gap2\\x.exe", t_tmpdir());
    SET_ERR(0);
    CHECK(!f(NULL, app, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi));
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(gap2_cpau_not_an_executable) {
    cpau_fn f = (cpau_fn)t_proc("CreateProcessAsUserA");
    STARTUPINFOA si; PROCESS_INFORMATION pi; char app[MAX_PATH]; HANDLE h; DWORD w;
    REQUIRE(f);
    memset(&si, 0, sizeof si); si.cb = sizeof si;
    t_tmpfile(app, sizeof app, "gap2_garbage.exe");
    h = CreateFileA(app, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(h, "this is not a PE image", 22, &w, NULL);
    CloseHandle(h);
    SET_ERR(0);
    CHECK(!f(NULL, app, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi));
    EXPECT_ERR(ERROR_BAD_EXE_FORMAT);
    DeleteFileA(app);
}
TEST(gap2_cpau_inherit_handles_env_and_cwd) {
    cpau_fn f = (cpau_fn)t_proc("CreateProcessAsUserA");
    STARTUPINFOA si; PROCESS_INFORMATION pi; char cmd[1024];
    REQUIRE(f);
    memset(&si, 0, sizeof si); si.cb = sizeof si;
    snprintf(cmd, sizeof cmd, "\"%s\" --child gap2_echo 9", t_self_path());
    BOOL ok = f(NULL, NULL, cmd, NULL, NULL, TRUE, 0, NULL, t_tmpdir(), &si, &pi);
    CHECK(ok);
    if (ok) { DWORD xc = wait_exit(&pi); EXPECT_EQ(xc, 9); }
}
TEST(gap2_cpau_bad_current_directory) {
    cpau_fn f = (cpau_fn)t_proc("CreateProcessAsUserA");
    STARTUPINFOA si; PROCESS_INFORMATION pi; char cmd[1024], cwd[MAX_PATH];
    REQUIRE(f);
    memset(&si, 0, sizeof si); si.cb = sizeof si;
    snprintf(cmd, sizeof cmd, "\"%s\" --child gap2_echo 9", t_self_path());
    snprintf(cwd, sizeof cwd, "%sgap2_no_such_cwd", t_tmpdir());
    SET_ERR(0);
    BOOL ok = f(NULL, NULL, cmd, NULL, NULL, FALSE, 0, NULL, cwd, &si, &pi);
    CHECK(!ok);
    t_note("nonexistent cwd: err=%lu", GetLastError());
    if (ok) wait_exit(&pi);
}
TEST(gap2_cpau_command_line_not_modified) {
    cpau_fn f = (cpau_fn)t_proc("CreateProcessAsUserA");
    STARTUPINFOA si; PROCESS_INFORMATION pi; char cmd[1024], orig[1024];
    REQUIRE(f);
    memset(&si, 0, sizeof si); si.cb = sizeof si;
    snprintf(cmd, sizeof cmd, "\"%s\" --child gap2_echo 3", t_self_path());
    strcpy(orig, cmd);
    BOOL ok = f(NULL, NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    CHECK(ok);
    CHECK(!strcmp(cmd, orig));      /* quoted command line is parsed without terminators being written */
    if (ok) wait_exit(&pi);
}

/* ---------------- DefineDosDeviceA : error paths that fail before any change ---------------- */
typedef BOOL (WINAPI *ddd_fn)(DWORD, LPCSTR, LPCSTR);
typedef BOOL (WINAPI *dddw_fn)(DWORD, LPCWSTR, LPCWSTR);

TEST(gap2_ddda_unknown_flags) {
    ddd_fn f = (ddd_fn)t_proc("DefineDosDeviceA");
    static const DWORD fl[] = { 0x20, 0x40, 0x80, 0x100, 0x10000, 0x80000000, 0xFFFFFFE0, 0x23, 0xFFFFFFFF };
    size_t i;
    REQUIRE(f);
    for (i = 0; i < sizeof fl / sizeof fl[0]; i++) {
        SET_ERR(0);
        CHECKF(!f(fl[i], "K32T_GAP2:", "C:\\"), "flags 0x%lx succeeded", fl[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(gap2_ddda_exact_match_requires_remove) {
    ddd_fn f = (ddd_fn)t_proc("DefineDosDeviceA");
    REQUIRE(f);
    SET_ERR(0);
    CHECK(!f(DDD_EXACT_MATCH_ON_REMOVE, "K32T_GAP2:", "C:\\"));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!f(DDD_EXACT_MATCH_ON_REMOVE | DDD_RAW_TARGET_PATH, "K32T_GAP2:", "C:\\"));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!f(DDD_EXACT_MATCH_ON_REMOVE, "K32T_GAP2:", NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap2_ddda_null_target_needs_remove) {
    ddd_fn f = (ddd_fn)t_proc("DefineDosDeviceA");
    REQUIRE(f);
    SET_ERR(0);
    CHECK(!f(0, "K32T_GAP2:", NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!f(DDD_RAW_TARGET_PATH, "K32T_GAP2:", NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!f(DDD_NO_BROADCAST_SYSTEM, "K32T_GAP2:", NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!f(DDD_RAW_TARGET_PATH | DDD_NO_BROADCAST_SYSTEM, "K32T_GAP2:", NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap2_ddda_luid_broadcast_invalid_combos) {
    ddd_fn f = (ddd_fn)t_proc("DefineDosDeviceA");
    REQUIRE(f);
    /* DDD_LUID_BROADCAST_DRIVE (0x10) takes only a device name, no target, and no other flags */
    SET_ERR(0);
    CHECK(!f(DDD_LUID_BROADCAST_DRIVE, "K32T_GAP2:", "C:\\"));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    {
        dddw_fn fw = (dddw_fn)t_proc("DefineDosDeviceW");
        if (fw && !t_is_wine()) {
            SET_ERR(0);
            CHECK(!fw(DDD_LUID_BROADCAST_DRIVE, NULL, NULL));      /* a device name is mandatory */
            EXPECT_ERR(ERROR_INVALID_PARAMETER);
        }
    }
    SET_ERR(0);
    CHECK(!f(DDD_LUID_BROADCAST_DRIVE | DDD_RAW_TARGET_PATH, "K32T_GAP2:", NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!f(DDD_LUID_BROADCAST_DRIVE | DDD_NO_BROADCAST_SYSTEM, "K32T_GAP2:", NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap2_ddda_w_parity_invalid) {
    ddd_fn fa = (ddd_fn)t_proc("DefineDosDeviceA");
    dddw_fn fw = (dddw_fn)t_proc("DefineDosDeviceW");
    static const DWORD fl[] = { 0x20, 0x4, 0x0, 0x1, 0xFFFFFFFF };
    size_t i;
    REQUIRE(fa); REQUIRE(fw);
    for (i = 0; i < sizeof fl / sizeof fl[0]; i++) {
        DWORD ea, ew;
        SET_ERR(0); BOOL a = fa(fl[i], "K32T_GAP2:", NULL); ea = GetLastError();
        SET_ERR(0); BOOL w = fw(fl[i], L"K32T_GAP2:", NULL); ew = GetLastError();
        CHECK(!a && !w);
        EXPECT_EQ(ea, ew);
        EXPECT_EQ(ea, ERROR_INVALID_PARAMETER);
    }
}
TEST(gap2_ddda_remove_flags_validation_only) {
    /* Intentionally not run with a valid combination: DefineDosDevice changes the
     * session's device namespace. Only argument-validation failures are exercised. */
    ddd_fn f = (ddd_fn)t_proc("DefineDosDeviceA");
    REQUIRE(f);
    SET_ERR(0);
    CHECK(!f(DDD_EXACT_MATCH_ON_REMOVE | DDD_NO_BROADCAST_SYSTEM, "K32T_GAP2:", NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}

/* ---------------- CreateTapePartition ---------------- */
typedef DWORD (WINAPI *ctp_fn)(HANDLE, DWORD, DWORD, DWORD);

TEST(gap2_ctp_null_handle) {
    ctp_fn f = (ctp_fn)t_proc("CreateTapePartition");
    REQUIRE(f);
    SET_ERR(0);
    DWORD r = f(NULL, TAPE_FIXED_PARTITIONS, 1, 0);
    EXPECT_EQ(r, ERROR_INVALID_HANDLE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap2_ctp_invalid_handle_value) {
    ctp_fn f = (ctp_fn)t_proc("CreateTapePartition");
    REQUIRE(f);
    SET_ERR(0);
    DWORD r = f(INVALID_HANDLE_VALUE, TAPE_FIXED_PARTITIONS, 1, 0);
    CHECK(r != NO_ERROR);
    t_note("INVALID_HANDLE_VALUE: ret=%lu err=%lu", r, GetLastError());
}
TEST(gap2_ctp_closed_and_wrong_type_handles) {
    ctp_fn f = (ctp_fn)t_proc("CreateTapePartition");
    HANDLE ev, c;
    REQUIRE(f);
    ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    DWORD r = f(ev, TAPE_SELECT_PARTITIONS, 2, 100);
    CHECK(r != NO_ERROR);
    t_note("event handle: ret=%lu", r);
    c = ev; CloseHandle(ev);
    SET_ERR(0);
    r = f(c, TAPE_SELECT_PARTITIONS, 2, 100);
    EXPECT_EQ(r, ERROR_INVALID_HANDLE);
}
TEST(gap2_ctp_regular_file_is_not_a_tape) {
    ctp_fn f = (ctp_fn)t_proc("CreateTapePartition");
    char p[MAX_PATH]; HANDLE h;
    REQUIRE(f);
    t_tmpfile(p, sizeof p, "gap2_tape.bin");
    h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE); if (h == INVALID_HANDLE_VALUE) return;
    SET_ERR(0);
    DWORD r = f(h, TAPE_INITIATOR_PARTITIONS, 1, 0);
    CHECK(r != NO_ERROR);
    t_note("regular file: ret=%lu", r);
    CloseHandle(h);
    DeleteFileA(p);
}
TEST(gap2_ctp_methods_on_non_tape) {
    ctp_fn f = (ctp_fn)t_proc("CreateTapePartition");
    char p[MAX_PATH]; HANDLE h; static const DWORD m[] = { TAPE_FIXED_PARTITIONS, TAPE_SELECT_PARTITIONS, TAPE_INITIATOR_PARTITIONS, 99, 0xFFFFFFFF };
    size_t i;
    REQUIRE(f);
    t_tmpfile(p, sizeof p, "gap2_tape2.bin");
    h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE); if (h == INVALID_HANDLE_VALUE) return;
    for (i = 0; i < sizeof m / sizeof m[0]; i++) CHECK(f(h, m[i], 1, 0) != NO_ERROR);
    CloseHandle(h);
    DeleteFileA(p);
}
TEST(gap2_ctp_directory_handle) {
    ctp_fn f = (ctp_fn)t_proc("CreateTapePartition");
    HANDLE h;
    REQUIRE(f);
    h = CreateFileA(t_tmpdir(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    CHECK(h != INVALID_HANDLE_VALUE); if (h == INVALID_HANDLE_VALUE) return;
    CHECK(f(h, TAPE_FIXED_PARTITIONS, 1, 0) != NO_ERROR);
    CloseHandle(h);
}
TEST(gap2_ctp_nul_device) {
    ctp_fn f = (ctp_fn)t_proc("CreateTapePartition");
    HANDLE h;
    REQUIRE(f);
    h = CreateFileA("\\\\.\\NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_skip("cannot open NUL"); return; }
    DWORD r = f(h, TAPE_FIXED_PARTITIONS, 1, 0);
    CHECK(r != NO_ERROR);
    t_note("NUL device: ret=%lu", r);
    CloseHandle(h);
}
TEST(gap2_ctp_no_tape_drive_paths) {
    /* \\.\TAPE0 normally does not exist; opening only, never sending partition commands */
    HANDLE h = CreateFileA("\\\\.\\TAPE0", 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) { t_note("a tape drive is present; no partition commands are sent"); CloseHandle(h); }
    else CHECK(GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_ACCESS_DENIED || GetLastError() == ERROR_PATH_NOT_FOUND);
}

/* ---------------- UMS ---------------- */
#define UMS_REQUIRE_REAL() do { if (t_is_wine()) { t_skip("UMS unimplemented in this emulator"); return; } } while (0)

TEST(gap2_ums_create_delete_completion_list) {
    BOOL (WINAPI *c)(PUMS_COMPLETION_LIST *) = (void *)t_proc("CreateUmsCompletionList");
    BOOL (WINAPI *d)(PUMS_COMPLETION_LIST) = (void *)t_proc("DeleteUmsCompletionList");
    PUMS_COMPLETION_LIST l = NULL;
    REQUIRE(c); REQUIRE(d); UMS_REQUIRE_REAL();
    SET_ERR(0);
    CHECK(c(&l));
    CHECK(l != NULL);
    if (l) CHECK(d(l));
}
TEST(gap2_ums_completion_list_repeat) {
    BOOL (WINAPI *c)(PUMS_COMPLETION_LIST *) = (void *)t_proc("CreateUmsCompletionList");
    BOOL (WINAPI *d)(PUMS_COMPLETION_LIST) = (void *)t_proc("DeleteUmsCompletionList");
    PUMS_COMPLETION_LIST l[16]; int i;
    REQUIRE(c); REQUIRE(d); UMS_REQUIRE_REAL();
    for (i = 0; i < 16; i++) { l[i] = NULL; CHECK(c(&l[i])); CHECK(l[i] != NULL); }
    for (i = 0; i < 16; i++) for (int j = i + 1; j < 16; j++) CHECK(l[i] != l[j]);
    for (i = 0; i < 16; i++) if (l[i]) CHECK(d(l[i]));
    for (i = 0; i < 1000; i++) { PUMS_COMPLETION_LIST x = NULL; if (!c(&x)) { CHECK(0); break; } d(x); }
}
TEST(gap2_ums_delete_null_completion_list) {
    BOOL (WINAPI *d)(PUMS_COMPLETION_LIST) = (void *)t_proc("DeleteUmsCompletionList");
    REQUIRE(d); UMS_REQUIRE_REAL();
    SET_ERR(0);
    CHECK(!d(NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap2_ums_create_delete_thread_context) {
    BOOL (WINAPI *c)(PUMS_CONTEXT *) = (void *)t_proc("CreateUmsThreadContext");
    BOOL (WINAPI *d)(PUMS_CONTEXT) = (void *)t_proc("DeleteUmsThreadContext");
    PUMS_CONTEXT x = NULL;
    REQUIRE(c); REQUIRE(d); UMS_REQUIRE_REAL();
    CHECK(c(&x));
    CHECK(x != NULL);
    if (x) CHECK(d(x));
}
TEST(gap2_ums_thread_context_repeat) {
    BOOL (WINAPI *c)(PUMS_CONTEXT *) = (void *)t_proc("CreateUmsThreadContext");
    BOOL (WINAPI *d)(PUMS_CONTEXT) = (void *)t_proc("DeleteUmsThreadContext");
    PUMS_CONTEXT x[16]; int i;
    REQUIRE(c); REQUIRE(d); UMS_REQUIRE_REAL();
    for (i = 0; i < 16; i++) { x[i] = NULL; CHECK(c(&x[i])); }
    for (i = 0; i < 16; i++) for (int j = i + 1; j < 16; j++) CHECK(x[i] != x[j]);
    for (i = 0; i < 16; i++) if (x[i]) CHECK(d(x[i]));
    for (i = 0; i < 1000; i++) { PUMS_CONTEXT y = NULL; if (!c(&y)) { CHECK(0); break; } d(y); }
}
TEST(gap2_ums_delete_null_thread_context) {
    BOOL (WINAPI *d)(PUMS_CONTEXT) = (void *)t_proc("DeleteUmsThreadContext");
    REQUIRE(d); UMS_REQUIRE_REAL();
    SET_ERR(0);
    CHECK(!d(NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap2_ums_dequeue_empty_list_zero_timeout) {
    BOOL (WINAPI *c)(PUMS_COMPLETION_LIST *) = (void *)t_proc("CreateUmsCompletionList");
    BOOL (WINAPI *d)(PUMS_COMPLETION_LIST) = (void *)t_proc("DeleteUmsCompletionList");
    BOOL (WINAPI *q)(PUMS_COMPLETION_LIST, DWORD, PUMS_CONTEXT *) = (void *)t_proc("DequeueUmsCompletionListItems");
    PUMS_COMPLETION_LIST l = NULL; PUMS_CONTEXT out = (PUMS_CONTEXT)0x1234;
    REQUIRE(c); REQUIRE(d); REQUIRE(q); UMS_REQUIRE_REAL();
    CHECK(c(&l)); if (!l) return;
    SET_ERR(0);
    BOOL ok = q(l, 0, &out);
    t_note("dequeue(empty, 0): ok=%d err=%lu out=%p", ok, GetLastError(), out);
    CHECK(out == NULL);
    d(l);
}
TEST(gap2_ums_dequeue_empty_list_short_timeout) {
    BOOL (WINAPI *c)(PUMS_COMPLETION_LIST *) = (void *)t_proc("CreateUmsCompletionList");
    BOOL (WINAPI *d)(PUMS_COMPLETION_LIST) = (void *)t_proc("DeleteUmsCompletionList");
    BOOL (WINAPI *q)(PUMS_COMPLETION_LIST, DWORD, PUMS_CONTEXT *) = (void *)t_proc("DequeueUmsCompletionListItems");
    PUMS_COMPLETION_LIST l = NULL; PUMS_CONTEXT out = (PUMS_CONTEXT)0x1234;
    DWORD t0, el;
    REQUIRE(c); REQUIRE(d); REQUIRE(q); UMS_REQUIRE_REAL();
    CHECK(c(&l)); if (!l) return;
    t0 = GetTickCount();
    BOOL ok = q(l, 100, &out);
    el = GetTickCount() - t0;
    t_note("dequeue(empty, 100ms): ok=%d err=%lu out=%p elapsed=%lums", ok, GetLastError(), out, el);
    CHECK(el >= 50 && el < 5000);
    CHECK(out == NULL);
    d(l);
}
TEST(gap2_ums_dequeue_invalid_args) {
    BOOL (WINAPI *c)(PUMS_COMPLETION_LIST *) = (void *)t_proc("CreateUmsCompletionList");
    BOOL (WINAPI *d)(PUMS_COMPLETION_LIST) = (void *)t_proc("DeleteUmsCompletionList");
    BOOL (WINAPI *q)(PUMS_COMPLETION_LIST, DWORD, PUMS_CONTEXT *) = (void *)t_proc("DequeueUmsCompletionListItems");
    PUMS_COMPLETION_LIST l = NULL; PUMS_CONTEXT out = NULL;
    REQUIRE(c); REQUIRE(d); REQUIRE(q); UMS_REQUIRE_REAL();
    SET_ERR(0);
    CHECK(!q(NULL, 0, &out));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CHECK(c(&l)); if (!l) return;
    SET_ERR(0);
    CHECK(!q(l, 0, NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    d(l);
}
TEST(gap2_ums_enter_scheduling_bad_version) {
    BOOL (WINAPI *c)(PUMS_COMPLETION_LIST *) = (void *)t_proc("CreateUmsCompletionList");
    BOOL (WINAPI *d)(PUMS_COMPLETION_LIST) = (void *)t_proc("DeleteUmsCompletionList");
    BOOL (WINAPI *e)(PUMS_SCHEDULER_STARTUP_INFO) = (void *)t_proc("EnterUmsSchedulingMode");
    PUMS_COMPLETION_LIST l = NULL; UMS_SCHEDULER_STARTUP_INFO si;
    static const ULONG vers[] = { 0, 1, 0xFF, 0x101, 0x200, 0x10000, 0xFFFFFFFF };
    size_t i;
    REQUIRE(c); REQUIRE(d); REQUIRE(e); UMS_REQUIRE_REAL();
    CHECK(c(&l)); if (!l) return;
    for (i = 0; i < sizeof vers / sizeof vers[0]; i++) {
        memset(&si, 0, sizeof si);
        si.UmsVersion = vers[i]; si.CompletionList = l; si.SchedulerProc = NULL; si.SchedulerParam = NULL;
        SET_ERR(0);
        CHECKF(!e(&si), "version 0x%lx accepted", vers[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
    d(l);
}

static VOID NTAPI gap2_sched(RTL_UMS_SCHEDULER_REASON reason, ULONG_PTR payload, PVOID param)
{
    ExitProcess(0x4A);
}
CHILD(gap2_ums_enter) {
    BOOL (WINAPI *c)(PUMS_COMPLETION_LIST *) = (void *)t_proc("CreateUmsCompletionList");
    BOOL (WINAPI *e)(PUMS_SCHEDULER_STARTUP_INFO) = (void *)t_proc("EnterUmsSchedulingMode");
    PUMS_COMPLETION_LIST l = NULL; UMS_SCHEDULER_STARTUP_INFO si;
    if (!c || !e) return 90;
    if (!c(&l)) return 91;
    memset(&si, 0, sizeof si);
    si.UmsVersion = UMS_VERSION; si.CompletionList = l; si.SchedulerProc = gap2_sched; si.SchedulerParam = NULL;
    if (!e(&si)) return 100 + (int)(GetLastError() & 0xFF);
    return 92;
}
TEST(gap2_ums_enter_scheduling_mode_in_child) {
    DWORD rc;
    REQUIRE(t_proc("EnterUmsSchedulingMode")); UMS_REQUIRE_REAL();
    rc = t_run_child("gap2_ums_enter", "", 30000);
    t_note("child exit code 0x%lx", rc);
    CHECK(rc != 0xDEAD0000 && rc != 0xDEAD0001);
    if (rc >= 100 && rc < 400) t_note("EnterUmsSchedulingMode failed in child, error %lu", rc - 100);
    else EXPECT_EQ(rc, 0x4A);
}
TEST(gap2_ums_exports_in_kernel32) {
    static const char *names[] = { "CreateUmsCompletionList", "CreateUmsThreadContext", "DeleteUmsCompletionList",
        "DeleteUmsThreadContext", "DequeueUmsCompletionListItems", "EnterUmsSchedulingMode" };
    size_t i;
    for (i = 0; i < sizeof names / sizeof names[0]; i++) CHECKF(t_proc_k32(names[i]) != NULL, "%s", names[i]);
}

/* ---------------- Ds* (ntdsapi forwarders in kernelbase) ---------------- */
typedef DWORD (WINAPI *mkcred_fn)(LPCWSTR, LPCWSTR, LPCWSTR, RPC_AUTH_IDENTITY_HANDLE *);
typedef VOID (WINAPI *freecred_fn)(RPC_AUTH_IDENTITY_HANDLE);

TEST(gap2_ds_exports_in_kernelbase) {
    static const char *names[] = { "DsBindWithSpnExW", "DsCrackNamesW", "DsFreeDomainControllerInfoW", "DsFreeNameResultW",
        "DsFreePasswordCredentials", "DsGetDomainControllerInfoW", "DsMakePasswordCredentialsW", "DsUnBindW" };
    size_t i;
    for (i = 0; i < sizeof names / sizeof names[0]; i++) CHECKF(t_proc_kb(names[i]) != NULL, "%s", names[i]);
}
TEST(gap2_ds_make_free_credentials) {
    mkcred_fn m = (mkcred_fn)t_proc("DsMakePasswordCredentialsW");
    freecred_fn fr = (freecred_fn)t_proc("DsFreePasswordCredentials");
    RPC_AUTH_IDENTITY_HANDLE h = NULL;
    REQUIRE(m); REQUIRE(fr);
    DWORD r = m(L"k32t_user", L"K32TDOM", L"k32t_password", &h);
    t_note("DsMakePasswordCredentialsW: %lu", r);
    EXPECT_EQ(r, ERROR_SUCCESS);
    if (r == ERROR_SUCCESS) {
        CHECK(h != NULL);
        fr(h);
    }
}
TEST(gap2_ds_make_credentials_many) {
    mkcred_fn m = (mkcred_fn)t_proc("DsMakePasswordCredentialsW");
    freecred_fn fr = (freecred_fn)t_proc("DsFreePasswordCredentials");
    RPC_AUTH_IDENTITY_HANDLE h[10]; int i;
    REQUIRE(m); REQUIRE(fr);
    for (i = 0; i < 10; i++) { h[i] = NULL; CHECK(m(L"u", L"d", L"p", &h[i]) == ERROR_SUCCESS); }
    for (i = 0; i < 10; i++) if (h[i]) fr(h[i]);
    for (i = 0; i < 200; i++) { RPC_AUTH_IDENTITY_HANDLE x = NULL; if (m(L"u", NULL, L"p", &x) == ERROR_SUCCESS && x) fr(x); }
}
TEST(gap2_ds_make_credentials_variants) {
    mkcred_fn m = (mkcred_fn)t_proc("DsMakePasswordCredentialsW");
    freecred_fn fr = (freecred_fn)t_proc("DsFreePasswordCredentials");
    RPC_AUTH_IDENTITY_HANDLE h = NULL;
    REQUIRE(m); REQUIRE(fr);
    DWORD r = m(L"user@example.com", NULL, L"pw", &h);
    t_note("UPN user, NULL domain: %lu", r);
    if (r == 0 && h) fr(h);
    h = NULL; r = m(L"user", L"dom", NULL, &h);
    t_note("NULL password: %lu", r);
    if (r == 0 && h) fr(h);
    h = NULL; r = m(L"", L"", L"", &h);
    t_note("empty strings: %lu", r);
    if (r == 0 && h) fr(h);
}
CHILD(gap2_ds_null_all) {
    mkcred_fn m = (mkcred_fn)t_proc("DsMakePasswordCredentialsW");
    freecred_fn fr = (freecred_fn)t_proc("DsFreePasswordCredentials");
    RPC_AUTH_IDENTITY_HANDLE h = (RPC_AUTH_IDENTITY_HANDLE)0x1234;
    if (!m) return 90;
    DWORD r = m(NULL, NULL, NULL, &h);
    if (r == 0 && h && h != (RPC_AUTH_IDENTITY_HANDLE)0x1234 && fr) fr(h);
    return (int)(r & 0xFFFF) | (h == NULL ? 0x10000 : 0);
}
TEST(gap2_ds_make_credentials_all_null_child) {
    DWORD rc;
    REQUIRE(t_proc_kb("DsMakePasswordCredentialsW"));
    rc = t_run_child("gap2_ds_null_all", "", 30000);
    t_note("child result 0x%lx (low16 = status, bit16 = handle NULL)", rc);
    CHECK(rc < 0xC0000000);
}
CHILD(gap2_ds_null_out) {
    mkcred_fn m = (mkcred_fn)t_proc("DsMakePasswordCredentialsW");
    if (!m) return 90;
    return (int)m(L"u", L"d", L"p", NULL);
}
TEST(gap2_ds_make_credentials_null_out_child) {
    DWORD rc;
    REQUIRE(t_proc_kb("DsMakePasswordCredentialsW"));
    rc = t_run_child("gap2_ds_null_out", "", 30000);
    t_note("NULL pAuthIdentity child result %lu", rc);
    CHECK(rc != 0xDEAD0000 && rc != 0xDEAD0001);
    if (rc < 0xC0000000) CHECK(rc != 0);    /* success with nowhere to store the handle would be wrong */
}
CHILD(gap2_ds_crack) {
    typedef DWORD (WINAPI *crack_fn)(HANDLE, DS_NAME_FLAGS, DS_NAME_FORMAT, DS_NAME_FORMAT, DWORD, const LPCWSTR *, PDS_NAME_RESULTW *);
    crack_fn c = (crack_fn)t_proc("DsCrackNamesW");
    void (WINAPI *fr)(PDS_NAME_RESULTW) = (void *)t_proc("DsFreeNameResultW");
    LPCWSTR names[1] = { L"CN=Jeff Smith,OU=Sales,DC=Fabrikam,DC=Com" };
    PDS_NAME_RESULTW res = NULL;
    if (!c || !fr) return 90;
    DWORD r = c(NULL, DS_NAME_FLAG_SYNTACTICAL_ONLY, DS_FQDN_1779_NAME, DS_CANONICAL_NAME, 1, names, &res);
    if (r != 0) return 100 + (int)(r & 0xFF);
    if (!res || res->cItems != 1) return 91;
    if (res->rItems[0].status != DS_NAME_NO_ERROR) { int s = (int)res->rItems[0].status; fr(res); return 110 + s; }
    int bad = 0;
    if (!res->rItems[0].pName || lstrcmpW(res->rItems[0].pName, L"Fabrikam.Com/Sales/Jeff Smith")) bad = 92;
    if (!res->rItems[0].pDomain || lstrcmpiW(res->rItems[0].pDomain, L"Fabrikam.Com")) bad = bad ? bad : 93;
    fr(res);
    return bad;
}
TEST(gap2_ds_crack_names_syntactical_only) {
    DWORD rc;
    REQUIRE(t_proc_kb("DsCrackNamesW"));
    rc = t_run_child("gap2_ds_crack", "", 30000);
    t_note("child result %lu", rc);
    CHECK(rc != 0xDEAD0000 && rc != 0xDEAD0001);
    if (rc >= 100 && rc < 400) t_note("syntactical crack not available here (code %lu)", rc);
    else EXPECT_EQ(rc, 0);
}
CHILD(gap2_ds_crack_bad) {
    typedef DWORD (WINAPI *crack_fn)(HANDLE, DS_NAME_FLAGS, DS_NAME_FORMAT, DS_NAME_FORMAT, DWORD, const LPCWSTR *, PDS_NAME_RESULTW *);
    crack_fn c = (crack_fn)t_proc("DsCrackNamesW");
    void (WINAPI *fr)(PDS_NAME_RESULTW) = (void *)t_proc("DsFreeNameResultW");
    LPCWSTR names[2] = { L"not a distinguished name", L"CN=a,DC=b" };
    PDS_NAME_RESULTW res = NULL;
    if (!c || !fr) return 90;
    DWORD r = c(NULL, DS_NAME_FLAG_SYNTACTICAL_ONLY, DS_FQDN_1779_NAME, DS_CANONICAL_NAME, 2, names, &res);
    if (r != 0) return 100 + (int)(r & 0xFF);
    int out = 0;
    if (res && res->cItems == 2) out = 10 * (int)res->rItems[0].status + (int)res->rItems[1].status;
    else out = 99;
    if (res) fr(res);
    return out;
}
TEST(gap2_ds_crack_names_syntax_error_child) {
    DWORD rc;
    REQUIRE(t_proc_kb("DsCrackNamesW"));
    rc = t_run_child("gap2_ds_crack_bad", "", 30000);
    t_note("child result %lu (10*status[0] + status[1])", rc);
    CHECK(rc != 0xDEAD0000 && rc != 0xDEAD0001);
}
CHILD(gap2_ds_crack_zero) {
    typedef DWORD (WINAPI *crack_fn)(HANDLE, DS_NAME_FLAGS, DS_NAME_FORMAT, DS_NAME_FORMAT, DWORD, const LPCWSTR *, PDS_NAME_RESULTW *);
    crack_fn c = (crack_fn)t_proc("DsCrackNamesW");
    PDS_NAME_RESULTW res = NULL;
    if (!c) return 90;
    DWORD r = c(NULL, DS_NAME_FLAG_SYNTACTICAL_ONLY, DS_FQDN_1779_NAME, DS_CANONICAL_NAME, 0, NULL, &res);
    return (int)(r & 0xFFFF);
}
TEST(gap2_ds_crack_names_zero_names_child) {
    DWORD rc;
    REQUIRE(t_proc_kb("DsCrackNamesW"));
    rc = t_run_child("gap2_ds_crack_zero", "", 30000);
    t_note("zero names: child result %lu", rc);
    CHECK(rc != 0xDEAD0000 && rc != 0xDEAD0001);
}
CHILD(gap2_ds_unbind_null) {
    DWORD (WINAPI *u)(HANDLE *) = (void *)t_proc("DsUnBindW");
    HANDLE h = NULL;
    if (!u) return 90;
    DWORD r = u(&h);
    return (int)(r & 0xFFFF);
}
TEST(gap2_ds_unbind_null_handle_child) {
    DWORD rc;
    REQUIRE(t_proc_kb("DsUnBindW"));
    rc = t_run_child("gap2_ds_unbind_null", "", 30000);
    t_note("DsUnBindW(&NULL): child result %lu", rc);
    CHECK(rc != 0xDEAD0000 && rc != 0xDEAD0001);
}
CHILD(gap2_ds_bind_nullout) {
    DWORD (WINAPI *b)(LPCWSTR, LPCWSTR, RPC_AUTH_IDENTITY_HANDLE, LPCWSTR, DWORD, HANDLE *) = (void *)t_proc("DsBindWithSpnExW");
    if (!b) return 90;
    DWORD r = b(NULL, NULL, NULL, NULL, 0, NULL);
    return (int)(r & 0xFFFF);
}
TEST(gap2_ds_bind_null_out_handle_child) {
    DWORD rc;
    REQUIRE(t_proc_kb("DsBindWithSpnExW"));
    rc = t_run_child("gap2_ds_bind_nullout", "", 30000);
    t_note("DsBindWithSpnExW(..., NULL phDS): child result %lu", rc);
    CHECK(rc != 0xDEAD0000 && rc != 0xDEAD0001);
    if (rc < 0xC0000000) CHECK(rc != 0);
}
CHILD(gap2_ds_dcinfo_null) {
    DWORD (WINAPI *g)(HANDLE, LPCWSTR, DWORD, DWORD *, void **) = (void *)t_proc("DsGetDomainControllerInfoW");
    if (!g) return 90;
    DWORD r = g(NULL, NULL, 1, NULL, NULL);
    return (int)(r & 0xFFFF);
}
TEST(gap2_ds_get_dc_info_null_args_child) {
    DWORD rc;
    REQUIRE(t_proc_kb("DsGetDomainControllerInfoW"));
    rc = t_run_child("gap2_ds_dcinfo_null", "", 30000);
    t_note("DsGetDomainControllerInfoW(NULL...): child result %lu", rc);
    CHECK(rc != 0xDEAD0000 && rc != 0xDEAD0001);
    if (rc < 0xC0000000) CHECK(rc != 0);
}
CHILD(gap2_ds_free_null) {
    void (WINAPI *a)(DWORD, DWORD, void *) = (void *)t_proc("DsFreeDomainControllerInfoW");
    void (WINAPI *b)(PDS_NAME_RESULTW) = (void *)t_proc("DsFreeNameResultW");
    if (!a || !b) return 90;
    a(1, 0, NULL);
    b(NULL);
    return 0;
}
TEST(gap2_ds_free_null_child) {
    DWORD rc;
    REQUIRE(t_proc_kb("DsFreeNameResultW"));
    rc = t_run_child("gap2_ds_free_null", "", 30000);
    t_note("Free(NULL) functions: child result 0x%lx", rc);
    CHECK(rc != 0xDEAD0000 && rc != 0xDEAD0001);
}
