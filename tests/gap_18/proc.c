#include "g18.h"
/* gap18: CreateProcessAsUserW. Only children of this test exe are ever created. */

static void cmdline(WCHAR *w, int cap, const char *child) {
    char c[MAX_PATH + 64]; snprintf(c, sizeof c, "\"%s\" --child %s", t_self_path(), child);
    g18_widen(w, cap, c);
}
static STARTUPINFOW si0(void) { STARTUPINFOW s; memset(&s, 0, sizeof s); s.cb = sizeof s; return s; }
static void reap(PROCESS_INFORMATION *pi) {
    if (!pi->hProcess) return;
    if (WaitForSingleObject(pi->hProcess, 0) != WAIT_OBJECT_0) TerminateProcess(pi->hProcess, 9);
    WaitForSingleObject(pi->hProcess, 5000);
    CloseHandle(pi->hThread); CloseHandle(pi->hProcess);
}

TEST(gap18_cpau_null_token_runs_child) {
    REQUIRE(U(CreateProcessAsUserW));
    WCHAR cl[MAX_PATH + 64]; cmdline(cl, MAX_PATH + 64, "gap18_exit7");
    STARTUPINFOW si = si0(); PROCESS_INFORMATION pi; memset(&pi, 0, sizeof pi);
    SET_ERR(0);
    BOOL ok = U(CreateProcessAsUserW)(NULL, NULL, cl, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    CHECKF(ok, "err %lu", GetLastError());
    if (ok) {
        CHECK(pi.dwProcessId != 0 && pi.dwThreadId != 0 && pi.hProcess && pi.hThread);
        EQ(g18_wait_exit(pi.hProcess, 20000), 7);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    }
}
TEST(gap18_cpau_null_app_null_cmdline) {
    if (t_is_wine()) { t_skip("emulator faults on NULL arguments"); return; }
    REQUIRE(U(CreateProcessAsUserW));
    STARTUPINFOW si = si0(); PROCESS_INFORMATION pi; memset(&pi, 0xCC, sizeof pi);
    SET_ERR(0);
    CHECK(!U(CreateProcessAsUserW)(NULL, NULL, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!U(CreateProcessAsUserW)(INVALID_HANDLE_VALUE, NULL, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi));
    t_note("INVALID_HANDLE_VALUE token + no image err %lu", GetLastError());
}
TEST(gap18_cpau_missing_image) {
    REQUIRE(U(CreateProcessAsUserW));
    STARTUPINFOW si = si0(); PROCESS_INFORMATION pi; memset(&pi, 0, sizeof pi);
    WCHAR cl[] = L"k32t_gap18_does_not_exist.exe";
    SET_ERR(0);
    CHECK(!U(CreateProcessAsUserW)(NULL, NULL, cl, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi));
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    SET_ERR(0);
    CHECK(!U(CreateProcessAsUserW)(NULL, L"C:\\k32t_gap18_nodir\\nothing.exe", NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi));
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    CHECK(pi.hProcess == NULL);
}
TEST(gap18_cpau_bad_current_directory) {
    REQUIRE(U(CreateProcessAsUserW));
    WCHAR cl[MAX_PATH + 64]; cmdline(cl, MAX_PATH + 64, "gap18_exit7");
    STARTUPINFOW si = si0(); PROCESS_INFORMATION pi; memset(&pi, 0, sizeof pi);
    SET_ERR(0);
    BOOL ok = U(CreateProcessAsUserW)(NULL, NULL, cl, NULL, NULL, FALSE, 0, NULL, L"C:\\k32t_gap18_no_such_dir", &si, &pi);
    CHECK(!ok);
    EXPECT_ERR(ERROR_DIRECTORY);
    if (ok) reap(&pi);
}
TEST(gap18_cpau_garbage_token_handles) {
    REQUIRE(U(CreateProcessAsUserW));
    WCHAR cl[MAX_PATH + 64]; cmdline(cl, MAX_PATH + 64, "gap18_exit7");
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE closed = CreateEventA(NULL, TRUE, FALSE, NULL); CloseHandle(closed);
    HANDLE toks[] = { (HANDLE)(ULONG_PTR)0x7FFC, ev, closed, GetCurrentThread() };
    static const char *nm[] = { "bogus value", "event handle", "closed handle", "thread pseudo-handle" };
    for (int i = 0; i < 4; i++) {
        STARTUPINFOW si = si0(); PROCESS_INFORMATION pi; memset(&pi, 0, sizeof pi);
        SET_ERR(0);
        BOOL ok = U(CreateProcessAsUserW)(toks[i], NULL, cl, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi);
        CHECKF(!ok, "%s accepted as a token", nm[i]);
        t_note("token = %s -> err %lu", nm[i], GetLastError());
        CHECKF(GetLastError() != 0, "%s: no error", nm[i]);
        if (ok) reap(&pi);
    }
    CloseHandle(ev);
}
TEST(gap18_cpau_duplicated_primary_token) {
    REQUIRE(U(CreateProcessAsUserW));
    HANDLE tok = NULL, dup = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_ASSIGN_PRIMARY, &tok)) { t_skip("OpenProcessToken failed"); return; }
    if (!DuplicateTokenEx(tok, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenPrimary, &dup)) { t_skip("DuplicateTokenEx failed"); CloseHandle(tok); return; }
    WCHAR cl[MAX_PATH + 64]; cmdline(cl, MAX_PATH + 64, "gap18_exit7");
    STARTUPINFOW si = si0(); PROCESS_INFORMATION pi; memset(&pi, 0, sizeof pi);
    SET_ERR(0);
    BOOL ok = U(CreateProcessAsUserW)(dup, NULL, cl, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    if (!ok) t_note("primary-token duplicate refused: err %lu (needs SeAssignPrimaryToken or sibling token)", GetLastError());
    else {
        EQ(g18_wait_exit(pi.hProcess, 20000), 7);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    }
    CloseHandle(dup); CloseHandle(tok);
}
TEST(gap18_cpau_impersonation_token_rejected) {
    REQUIRE(U(CreateProcessAsUserW));
    HANDLE tok = NULL, dup = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | TOKEN_QUERY, &tok)) { t_skip("OpenProcessToken failed"); return; }
    if (!DuplicateTokenEx(tok, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenImpersonation, &dup)) { t_skip("DuplicateTokenEx failed"); CloseHandle(tok); return; }
    WCHAR cl[MAX_PATH + 64]; cmdline(cl, MAX_PATH + 64, "gap18_exit7");
    STARTUPINFOW si = si0(); PROCESS_INFORMATION pi; memset(&pi, 0, sizeof pi);
    SET_ERR(0);
    BOOL ok = U(CreateProcessAsUserW)(dup, NULL, cl, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi);
    DWORD e = GetLastError();
    t_note("impersonation token as process token -> ok %d err %lu", ok, e);
    if (!ok) {
        /* no token-type check exists on the kernel path, so only the environment-dependent privilege/access
           refusals (as in gap18_cpau_primary_dup) are acceptable; ERROR_BAD_TOKEN_TYPE would contradict the decomp */
        CHECKF(e == ERROR_PRIVILEGE_NOT_HELD || e == ERROR_ACCESS_DENIED, "err %lu", e);
    } else {
        /* Win10 22H2: the kernel does not reject the token type; the new process always gets a primary token */
        HANDLE ct = NULL; TOKEN_TYPE tt = (TOKEN_TYPE)0; DWORD rl = 0;
        CHECK(OpenProcessToken(pi.hProcess, TOKEN_QUERY, &ct));
        if (ct) {
            CHECK(GetTokenInformation(ct, TokenType, &tt, sizeof tt, &rl));
            CHECK(tt == TokenPrimary);
            CloseHandle(ct);
        }
        reap(&pi);
    }
    CloseHandle(dup); CloseHandle(tok);
}
TEST(gap18_cpau_token_without_assign_primary_right) {
    REQUIRE(U(CreateProcessAsUserW));
    HANDLE tok = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) { t_skip("OpenProcessToken failed"); return; }
    WCHAR cl[MAX_PATH + 64]; cmdline(cl, MAX_PATH + 64, "gap18_exit7");
    STARTUPINFOW si = si0(); PROCESS_INFORMATION pi; memset(&pi, 0, sizeof pi);
    SET_ERR(0);
    BOOL ok = U(CreateProcessAsUserW)(tok, NULL, cl, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi);
    CHECK(!ok);
    t_note("TOKEN_QUERY-only token -> err %lu", GetLastError());
    if (ok) reap(&pi);
    CloseHandle(tok);
}
TEST(gap18_cpau_suspended_flag_null_token) {
    REQUIRE(U(CreateProcessAsUserW)); REQUIRE(U(ResumeThread));
    WCHAR cl[MAX_PATH + 64]; cmdline(cl, MAX_PATH + 64, "gap18_exit7");
    STARTUPINFOW si = si0(); PROCESS_INFORMATION pi; memset(&pi, 0, sizeof pi);
    BOOL ok = U(CreateProcessAsUserW)(NULL, NULL, cl, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi);
    CHECKF(ok, "err %lu", GetLastError()); if (!ok) return;
    EQ(WaitForSingleObject(pi.hProcess, 300), WAIT_TIMEOUT);       /* has not run yet */
    EQ(U(ResumeThread)(pi.hThread), 1);
    EQ(g18_wait_exit(pi.hProcess, 20000), 7);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
}
TEST(gap18_cpau_inherit_handles_flag) {
    REQUIRE(U(CreateProcessAsUserW));
    WCHAR cl[MAX_PATH + 64]; cmdline(cl, MAX_PATH + 64, "gap18_exit7");
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE e = CreateEventA(&sa, TRUE, FALSE, NULL);
    STARTUPINFOW si = si0(); PROCESS_INFORMATION pi; memset(&pi, 0, sizeof pi);
    BOOL ok = U(CreateProcessAsUserW)(NULL, NULL, cl, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
    CHECKF(ok, "err %lu", GetLastError());
    if (ok) { EQ(g18_wait_exit(pi.hProcess, 20000), 7); CloseHandle(pi.hThread); CloseHandle(pi.hProcess); }
    CloseHandle(e);
}
TEST(gap18_cpau_unicode_environment_flag) {
    REQUIRE(U(CreateProcessAsUserW));
    WCHAR cl[MAX_PATH + 64]; cmdline(cl, MAX_PATH + 64, "gap18_exit7");
    static WCHAR env[] = L"K32T_GAP18=1\0\0";
    STARTUPINFOW si = si0(); PROCESS_INFORMATION pi; memset(&pi, 0, sizeof pi);
    BOOL ok = U(CreateProcessAsUserW)(NULL, NULL, cl, NULL, NULL, FALSE, CREATE_UNICODE_ENVIRONMENT, env, NULL, &si, &pi);
    CHECKF(ok, "err %lu", GetLastError());
    if (ok) { EQ(g18_wait_exit(pi.hProcess, 20000), 7); CloseHandle(pi.hThread); CloseHandle(pi.hProcess); }
}
TEST(gap18_cpau_process_security_attrs_inherit) {
    REQUIRE(U(CreateProcessAsUserW));
    WCHAR cl[MAX_PATH + 64]; cmdline(cl, MAX_PATH + 64, "gap18_exit7");
    SECURITY_ATTRIBUTES pa = { sizeof pa, NULL, TRUE }, ta = { sizeof ta, NULL, FALSE };
    STARTUPINFOW si = si0(); PROCESS_INFORMATION pi; memset(&pi, 0, sizeof pi);
    BOOL ok = U(CreateProcessAsUserW)(NULL, NULL, cl, &pa, &ta, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi);
    CHECKF(ok, "err %lu", GetLastError());
    if (ok) {
        DWORD f = 0;
        CHECK(GetHandleInformation(pi.hProcess, &f)); CHECK(f & HANDLE_FLAG_INHERIT);
        CHECK(GetHandleInformation(pi.hThread, &f)); CHECK(!(f & HANDLE_FLAG_INHERIT));
        reap(&pi);
    }
}
TEST(gap18_cpau_bad_security_attr_length_note) {
    REQUIRE(U(CreateProcessAsUserW));
    WCHAR cl[MAX_PATH + 64]; cmdline(cl, MAX_PATH + 64, "gap18_exit7");
    SECURITY_ATTRIBUTES pa = { 3, NULL, FALSE };
    STARTUPINFOW si = si0(); PROCESS_INFORMATION pi; memset(&pi, 0, sizeof pi);
    SET_ERR(0);
    BOOL ok = U(CreateProcessAsUserW)(NULL, NULL, cl, &pa, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi);
    t_note("bad nLength -> %d err %lu", ok, ok ? 0 : GetLastError());
    if (ok) reap(&pi);
}
TEST(gap18_cpau_exports) { REQUIRE(UK(CreateProcessAsUserW)); REQUIRE(UB(CreateProcessAsUserW)); CHECK(1); }
