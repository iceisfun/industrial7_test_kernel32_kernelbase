/* Process exit codes, TerminateProcess/ExitProcess semantics, waiting, OpenProcess access matrix. */
#include "pc.h"

typedef struct { LONG_PTR st; BYTE *peb; ULONG_PTR aff; LONG prio; ULONG_PTR pid; ULONG_PTR ppid; } PBI;

/* ---------- exit codes ---------- */
TEST(pexit_exitprocess_codes) {
    REQUIRE(UK(ExitProcess)); REQUIRE(U(GetExitCodeProcess));
    static const DWORD v[] = { 0, 1, 2, 77, 255, 256, 0x7FFF, 0xFFFF, 0x10000, 0x7FFFFFFF, 0x80000000, 0xC0000005, 0xDEADBEEF, 0xFFFFFFFF };
    int i;
    for (i = 0; i < (int)(sizeof v / sizeof v[0]); i++) {
        char a[32]; snprintf(a, sizeof a, "0x%lx", v[i]);
        DWORD c = pc_exit_of("pc_exitcode", a, 0);
        CHECKF(c == v[i], "ExitProcess(0x%lx) -> 0x%lx", v[i], c);
    }
}
TEST(pexit_exit_code_259_is_indistinguishable_but_signaled) {
    REQUIRE(UK(ExitProcess));
    pc_opts o; PROCESS_INFORMATION pi; DWORD c = 0;
    memset(&o, 0, sizeof o); o.child = "pc_exitcode"; o.args = "259";
    CHECK(pc_launch(&o, &pi));
    EQV(WaitForSingleObject(pi.hProcess, PC_TIMEOUT), WAIT_OBJECT_0);
    CHECK(GetExitCodeProcess(pi.hProcess, &c)); EQV(c, STILL_ACTIVE);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
}
TEST(pexit_main_return_values) {
    REQUIRE(UK(CreateProcessW));
    static const DWORD v[] = { 0, 3, 255, 0x1234, 0x7FFFFFFF, 0xFFFFFFFF, 0x80000001 }; int i;
    for (i = 0; i < 7; i++) { char a[32]; snprintf(a, sizeof a, "0x%lx", v[i]); DWORD c = pc_exit_of("pc_retcode", a, 0); CHECKF(c == v[i], "main returned 0x%lx -> 0x%lx", v[i], c); }
}
TEST(pexit_terminateprocess_on_self_in_child) {
    REQUIRE(U(TerminateProcess));
    static const DWORD v[] = { 0, 5, 0x4242, 0xC0000005, 0xFFFFFFFF }; int i;
    for (i = 0; i < 5; i++) { char a[32]; snprintf(a, sizeof a, "0x%lx", v[i]); DWORD c = pc_exit_of("pc_termself", a, 0); CHECKF(c == v[i], "TerminateProcess(self,0x%lx) -> 0x%lx", v[i], c); }
}
TEST(pexit_exitprocess_kills_other_threads) {
    REQUIRE(UK(ExitProcess));
    EQV(pc_exit_of("pc_exit_kills_threads", "", 0), 5);
}
TEST(pexit_exitprocess_from_secondary_thread) {
    REQUIRE(UK(ExitProcess));
    EQV(pc_exit_of("pc_exit_from_thread", "", 0), 9);
}
TEST(pexit_main_thread_exitthread_with_other_thread_running) {
    REQUIRE(UK(ExitThread));
    if (t_is_wine()) { t_skip("process lingers under wine"); return; }
    DWORD c = pc_exit_of("pc_main_exitthread", "", 0);
    t_note("process exit code when last thread calls ExitThread(7) after main ExitThread(3): %lu", c);
    CHECK(c != 0xDEAD0001);
}
static void crash_expect(const char *kind, const char *extra, DWORD want) {
    char a[64]; DWORD c; snprintf(a, sizeof a, "%s %s", kind, extra);
    c = pc_exit_of("pc_crash", a, 0);
    CHECKF(c == want, "pc_crash %s -> 0x%lx want 0x%lx", kind, c, want);
}
TEST(pexit_crash_access_violation_write) { crash_expect("av", "", 0xC0000005); }
TEST(pexit_crash_access_violation_read) { crash_expect("avread", "", 0xC0000005); }
TEST(pexit_crash_integer_divide_by_zero) { crash_expect("div0", "", 0xC0000094); }
TEST(pexit_crash_breakpoint_without_debugger) { crash_expect("int3", "", 0x80000003); }
TEST(pexit_crash_illegal_instruction) { crash_expect("ud2", "", 0xC000001D); }
TEST(pexit_crash_privileged_instruction) { crash_expect("hlt", "", 0xC0000096); }
TEST(pexit_crash_stack_overflow) { crash_expect("so", "", 0xC00000FD); }
TEST(pexit_crash_fastfail_interrupt) { crash_expect("fastfail", "", 0xC0000409); }
TEST(pexit_crash_raise_custom_exception) { crash_expect("raise", "0xE0001234", 0xE0001234); }
TEST(pexit_crash_raise_array_bounds) { crash_expect("raise", "0xC000008C", 0xC000008C); }
TEST(pexit_crash_unhandled_filter_execute_handler_keeps_code) { crash_expect("uef_handler", "", 0xC0000005); }
TEST(pexit_crash_unhandled_filter_continue_search_keeps_code) { crash_expect("uef_search", "", 0xC0000005); }
TEST(pexit_crash_unhandled_filter_may_exit_with_own_code) { crash_expect("uef_exit", "", 1234); }
TEST(pexit_crash_raise_fail_fast_exception_observed) {
    REQUIRE(U(RaiseFailFastException));
    DWORD c = pc_exit_of("pc_crash", "failfast", 0);
    t_note("RaiseFailFastException(NULL,NULL,0) exit code 0x%lx", c);
    CHECK(c != 0 && c != 0xDEAD0001 && c != 0xDEAD0000);
}

/* ---------- TerminateProcess ---------- */
TEST(pterm_terminate_suspended_child_sets_exit_code) {
    REQUIRE(U(TerminateProcess));
    static const DWORD v[] = { 0, 1, 0x1234, 0xC000013A, 0xFFFFFFFF }; int i;
    for (i = 0; i < 5; i++) {
        PROCESS_INFORMATION pi; DWORD c = 0xAA;
        CHECK(pc_suspended("pc_sleep", "20000", 0, &pi));
        CHECK(U(TerminateProcess)(pi.hProcess, v[i]));
        EQV(WaitForSingleObject(pi.hProcess, 5000), WAIT_OBJECT_0);
        CHECK(GetExitCodeProcess(pi.hProcess, &c)); CHECKF(c == v[i], "exit 0x%lx want 0x%lx", c, v[i]);
        c = 0xAA; CHECK(GetExitCodeThread(pi.hThread, &c)); t_note("thread exit code after TerminateProcess(0x%lx): 0x%lx", v[i], c);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    }
}
TEST(pterm_terminate_running_child) {
    REQUIRE(U(TerminateProcess));
    PROCESS_INFORMATION pi; pc_opts o; DWORD c = 0; memset(&o, 0, sizeof o); o.child = "pc_sleep"; o.args = "20000";
    CHECK(pc_launch(&o, &pi));
    EQV(WaitForSingleObject(pi.hProcess, 300), WAIT_TIMEOUT);
    CHECK(GetExitCodeProcess(pi.hProcess, &c)); EQV(c, STILL_ACTIVE);
    CHECK(U(TerminateProcess)(pi.hProcess, 99));
    EQV(WaitForSingleObject(pi.hProcess, 5000), WAIT_OBJECT_0);
    CHECK(GetExitCodeProcess(pi.hProcess, &c)); EQV(c, 99);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
}
TEST(pterm_terminate_already_terminated_access_denied) {
    REQUIRE(U(TerminateProcess));
    PROCESS_INFORMATION pi; DWORD c = 0;
    CHECK(pc_suspended("pc_sleep", "20000", 0, &pi));
    CHECK(U(TerminateProcess)(pi.hProcess, 5));
    WaitForSingleObject(pi.hProcess, 5000);
    SET_ERR(0); CHECK(!U(TerminateProcess)(pi.hProcess, 6)); EQV(GetLastError(), ERROR_ACCESS_DENIED);
    CHECK(GetExitCodeProcess(pi.hProcess, &c)); EQV(c, 5);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
}
TEST(pterm_terminate_after_normal_exit_access_denied) {
    REQUIRE(U(TerminateProcess));
    pc_opts o; PROCESS_INFORMATION pi; memset(&o, 0, sizeof o); o.child = "pc_exit77";
    CHECK(pc_launch(&o, &pi)); WaitForSingleObject(pi.hProcess, PC_TIMEOUT);
    SET_ERR(0); CHECK(!U(TerminateProcess)(pi.hProcess, 6)); EQV(GetLastError(), ERROR_ACCESS_DENIED);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
}
TEST(pterm_invalid_handles_error_6) {
    REQUIRE(U(TerminateProcess));
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL); PROCESS_INFORMATION pi;
    SET_ERR(0); CHECK(!U(TerminateProcess)(NULL, 1)); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(TerminateProcess)(ev, 1)); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    CHECK(pc_suspended("pc_sleep", "20000", 0, &pi));
    SET_ERR(0); CHECK(!U(TerminateProcess)(pi.hThread, 1)); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    EQV(WaitForSingleObject(pi.hProcess, 0), WAIT_TIMEOUT);
    pc_kill(&pi); CloseHandle(ev);
}
TEST(pterm_closed_handle_error_6) {
    REQUIRE(U(TerminateProcess));
    HANDLE h; PROCESS_INFORMATION pi;
    CHECK(pc_suspended("pc_sleep", "20000", 0, &pi));
    CHECK(DuplicateHandle(GetCurrentProcess(), pi.hProcess, GetCurrentProcess(), &h, 0, FALSE, DUPLICATE_SAME_ACCESS));
    CloseHandle(h);
    SET_ERR(0); CHECK(!U(TerminateProcess)(h, 1)); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    pc_kill(&pi);
}
TEST(pterm_handle_without_terminate_right_denied) {
    REQUIRE(U(TerminateProcess));
    PROCESS_INFORMATION pi; HANDLE h; DWORD c = 0;
    CHECK(pc_suspended("pc_sleep", "20000", 0, &pi));
    h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE | PROCESS_VM_READ, FALSE, pi.dwProcessId); CHECK(h != NULL);
    SET_ERR(0); CHECK(!U(TerminateProcess)(h, 1)); EQV(GetLastError(), ERROR_ACCESS_DENIED);
    CHECK(GetExitCodeProcess(pi.hProcess, &c)); EQV(c, STILL_ACTIVE);
    if (h) CloseHandle(h);
    pc_kill(&pi);
}
TEST(pterm_handle_with_only_terminate_right_works) {
    REQUIRE(U(TerminateProcess));
    PROCESS_INFORMATION pi; HANDLE h; DWORD c = 0;
    CHECK(pc_suspended("pc_sleep", "20000", 0, &pi));
    h = OpenProcess(PROCESS_TERMINATE, FALSE, pi.dwProcessId); CHECK(h != NULL);
    if (h) { CHECK(U(TerminateProcess)(h, 0x55)); CloseHandle(h); }
    EQV(WaitForSingleObject(pi.hProcess, 5000), WAIT_OBJECT_0);
    CHECK(GetExitCodeProcess(pi.hProcess, &c)); EQV(c, 0x55);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
}
TEST(pterm_terminate_does_not_kill_grandchild) {
    REQUIRE(U(TerminateProcess));
    pc_opts o; char out[1024]; DWORD code; unsigned long long gpid; HANDLE g; DWORD c = 0;
    memset(&o, 0, sizeof o); o.child = "pc_spawnbg"; o.flags = CREATE_NO_WINDOW;
    CHECK(pc_capture(&o, out, sizeof out, NULL, 0, &code, PC_TIMEOUT)); EQV(code, 0);
    gpid = pc_unum(out, "gpid", 0); CHECK(gpid != 0);
    g = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE, FALSE, (DWORD)gpid);
    CHECK(g != NULL);
    if (g) {
        EQV(WaitForSingleObject(g, 300), WAIT_TIMEOUT);
        CHECK(GetExitCodeProcess(g, &c)); EQV(c, STILL_ACTIVE);
        CHECK(U(TerminateProcess)(g, 0)); WaitForSingleObject(g, 5000); CloseHandle(g);
    }
}

/* ---------- GetExitCodeProcess ---------- */
TEST(pgec_running_process_is_still_active) {
    REQUIRE(U(GetExitCodeProcess));
    PROCESS_INFORMATION pi; DWORD c = 0;
    CHECK(pc_suspended("pc_sleep", "20000", 0, &pi));
    CHECK(U(GetExitCodeProcess)(pi.hProcess, &c)); EQV(c, 259);
    pc_kill(&pi);
}
TEST(pgec_current_process_pseudo_handle) {
    REQUIRE(U(GetExitCodeProcess));
    DWORD c = 0; CHECK(U(GetExitCodeProcess)(GetCurrentProcess(), &c)); EQV(c, STILL_ACTIVE);
}
TEST(pgec_invalid_handles) {
    REQUIRE(U(GetExitCodeProcess));
    DWORD c = 0x77; HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL); PROCESS_INFORMATION pi;
    SET_ERR(0); CHECK(!U(GetExitCodeProcess)(NULL, &c)); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(GetExitCodeProcess)((HANDLE)(ULONG_PTR)0x1234, &c)); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(GetExitCodeProcess)(ev, &c)); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    CHECK(pc_suspended("pc_sleep", "20000", 0, &pi));
    SET_ERR(0); CHECK(!U(GetExitCodeProcess)(pi.hThread, &c)); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(GetExitCodeThread)(pi.hProcess, &c)); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    pc_kill(&pi); CloseHandle(ev);
}
TEST(pgec_requires_query_access) {
    REQUIRE(U(GetExitCodeProcess));
    PROCESS_INFORMATION pi; HANDLE s, q; DWORD c = 0;
    CHECK(pc_suspended("pc_sleep", "20000", 0, &pi));
    s = OpenProcess(SYNCHRONIZE, FALSE, pi.dwProcessId); q = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pi.dwProcessId);
    CHECK(s != NULL && q != NULL);
    if (s) { SET_ERR(0); CHECK(!U(GetExitCodeProcess)(s, &c)); EQV(GetLastError(), ERROR_ACCESS_DENIED); }
    if (q) { c = 0; CHECK(U(GetExitCodeProcess)(q, &c)); EQV(c, STILL_ACTIVE); }
    if (s) CloseHandle(s);
    if (q) CloseHandle(q);
    pc_kill(&pi);
}
TEST(pgec_stable_after_exit) {
    REQUIRE(U(GetExitCodeProcess));
    pc_opts o; PROCESS_INFORMATION pi; DWORD c = 0, d = 0; int i; memset(&o, 0, sizeof o); o.child = "pc_exit77";
    CHECK(pc_launch(&o, &pi)); WaitForSingleObject(pi.hProcess, PC_TIMEOUT);
    for (i = 0; i < 5; i++) { CHECK(U(GetExitCodeProcess)(pi.hProcess, &c)); EQV(c, 77); }
    CHECK(GetExitCodeThread(pi.hThread, &d)); t_note("main thread exit code %lu", d);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
}

/* ---------- waiting ---------- */
TEST(pwait_two_processes_wait_any_and_all) {
    REQUIRE(U(TerminateProcess));
    PROCESS_INFORMATION a, b; HANDLE h[2]; DWORD r;
    CHECK(pc_suspended("pc_sleep", "20000", 0, &a)); CHECK(pc_suspended("pc_sleep", "20000", 0, &b));
    h[0] = a.hProcess; h[1] = b.hProcess;
    EQV(WaitForMultipleObjects(2, h, FALSE, 100), WAIT_TIMEOUT);
    TerminateProcess(b.hProcess, 1);
    r = WaitForMultipleObjects(2, h, FALSE, 5000); EQV(r, WAIT_OBJECT_0 + 1);
    EQV(WaitForMultipleObjects(2, h, TRUE, 100), WAIT_TIMEOUT);
    TerminateProcess(a.hProcess, 2);
    EQV(WaitForMultipleObjects(2, h, TRUE, 5000), WAIT_OBJECT_0);
    pc_kill(&a); pc_kill(&b);
}
TEST(pwait_process_handle_without_synchronize_fails_5) {
    REQUIRE(UK(CreateProcessW));
    PROCESS_INFORMATION pi; HANDLE q;
    CHECK(pc_suspended("pc_sleep", "20000", 0, &pi));
    q = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pi.dwProcessId); CHECK(q != NULL);
    if (q) { SET_ERR(0); EQV(WaitForSingleObject(q, 0), WAIT_FAILED); EQV(GetLastError(), ERROR_ACCESS_DENIED); CloseHandle(q); }
    pc_kill(&pi);
}
TEST(pwait_duplicated_process_handle_waits_too) {
    REQUIRE(UK(CreateProcessW));
    PROCESS_INFORMATION pi; HANDLE d; 
    CHECK(pc_suspended("pc_sleep", "20000", 0, &pi));
    CHECK(DuplicateHandle(GetCurrentProcess(), pi.hProcess, GetCurrentProcess(), &d, SYNCHRONIZE, FALSE, 0));
    EQV(WaitForSingleObject(d, 50), WAIT_TIMEOUT);
    TerminateProcess(pi.hProcess, 3);
    EQV(WaitForSingleObject(d, 5000), WAIT_OBJECT_0);
    CloseHandle(d); pc_kill(&pi);
}
TEST(pwait_process_handle_stays_valid_after_exit) {
    REQUIRE(U(GetProcessId));
    pc_opts o; PROCESS_INFORMATION pi; memset(&o, 0, sizeof o); o.child = "pc_exit77";
    CHECK(pc_launch(&o, &pi)); WaitForSingleObject(pi.hProcess, PC_TIMEOUT);
    EQV(U(GetProcessId)(pi.hProcess), pi.dwProcessId);
    EQV(U(GetThreadId)(pi.hThread), pi.dwThreadId);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
}
TEST(pwait_children_survive_creator_exit) {
    REQUIRE(UK(CreateProcessW));
    pc_opts o; char out[1024]; DWORD code; unsigned long long gpid; HANDLE g; PBI pbi; NtQueryInformationProcess_pfn q = UN(NtQueryInformationProcess);
    memset(&o, 0, sizeof o); o.child = "pc_spawnbg"; o.flags = CREATE_NO_WINDOW;
    CHECK(pc_capture(&o, out, sizeof out, NULL, 0, &code, PC_TIMEOUT)); EQV(code, 0);
    gpid = pc_unum(out, "gpid", 0);
    g = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE, FALSE, (DWORD)gpid);
    CHECK(g != NULL);
    if (g) {
        memset(&pbi, 0, sizeof pbi);
        if (q && q(g, 0, &pbi, sizeof pbi, NULL) >= 0) CHECK(pbi.ppid != 0);
        EQV(WaitForSingleObject(g, 200), WAIT_TIMEOUT);
        TerminateProcess(g, 0); WaitForSingleObject(g, 5000); CloseHandle(g);
    }
}

/* ---------- OpenProcess ---------- */
TEST(popen_own_pid_access_matrix) {
    REQUIRE(U(OpenProcess));
    static const DWORD rights[] = { PROCESS_QUERY_LIMITED_INFORMATION, PROCESS_QUERY_INFORMATION, PROCESS_VM_READ, PROCESS_VM_WRITE | PROCESS_VM_OPERATION, SYNCHRONIZE,
        PROCESS_DUP_HANDLE, PROCESS_SET_INFORMATION, PROCESS_CREATE_THREAD, PROCESS_CREATE_PROCESS, PROCESS_SET_QUOTA, PROCESS_SUSPEND_RESUME, PROCESS_ALL_ACCESS,
        PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE | PROCESS_VM_READ, READ_CONTROL, PROCESS_SET_LIMITED_INFORMATION };
    NtQueryObject_pfn q = UN(NtQueryObject); int i;
    for (i = 0; i < (int)(sizeof rights / sizeof rights[0]); i++) {
        HANDLE h = U(OpenProcess)(rights[i], FALSE, GetCurrentProcessId()); PC_OBI b; memset(&b, 0, sizeof b);
        CHECKF(h != NULL, "right 0x%lx err %lu", rights[i], GetLastError());
        if (h && q && q(h, 0, &b, sizeof b, NULL) >= 0) CHECKF((b.GrantedAccess & rights[i]) == rights[i], "granted 0x%lx for 0x%lx", b.GrantedAccess, rights[i]);
        if (h) CloseHandle(h);
    }
}
TEST(popen_handle_value_properties) {
    REQUIRE(U(OpenProcess));
    HANDLE h = U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    CHECK(h != NULL && h != INVALID_HANDLE_VALUE && h != GetCurrentProcess());
    CHECK(((ULONG_PTR)h & 3) == 0);
    CHECK(U(GetProcessId)(h) == GetCurrentProcessId());
    CloseHandle(h);
}
TEST(popen_inherit_flag) {
    REQUIRE(U(OpenProcess));
    HANDLE a = U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION, TRUE, GetCurrentProcessId()), b = U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    DWORD fl = 0;
    CHECK(a && b);
    if (a) { CHECK(GetHandleInformation(a, &fl)); CHECK(fl & HANDLE_FLAG_INHERIT); CloseHandle(a); }
    fl = 0xFF; if (b) { CHECK(GetHandleInformation(b, &fl)); CHECK(!(fl & HANDLE_FLAG_INHERIT)); CloseHandle(b); }
}
TEST(popen_pid_zero_is_invalid_parameter) {
    REQUIRE(U(OpenProcess));
    SET_ERR(0); CHECK(U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, 0) == NULL); EQV(GetLastError(), ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(U(OpenProcess)(PROCESS_ALL_ACCESS, FALSE, 0) == NULL); EQV(GetLastError(), ERROR_INVALID_PARAMETER);
}
TEST(popen_nonexistent_pids_invalid_parameter) {
    REQUIRE(U(OpenProcess));
    static const DWORD p[] = { 1, 2, 3, 5, 0x7FFFFFFF, 0xFFFFFFFC, 0xFFFFFFFF, 0x3FFFFFFC }; int i;
    for (i = 0; i < 8; i++) { SET_ERR(0); HANDLE h = U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, p[i]); CHECKF(h == NULL, "pid 0x%lx opened", p[i]); if (h) CloseHandle(h); else EQV(GetLastError(), ERROR_INVALID_PARAMETER); }
}
TEST(popen_system_process_is_protected) {
    REQUIRE(U(OpenProcess));
    static const DWORD deny[] = { PROCESS_TERMINATE, PROCESS_VM_WRITE, PROCESS_VM_OPERATION, PROCESS_VM_READ, PROCESS_CREATE_THREAD, PROCESS_DUP_HANDLE, PROCESS_ALL_ACCESS }; int i;
    HANDLE l = U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, 4);
    t_note("System (pid 4) PROCESS_QUERY_LIMITED_INFORMATION %s", l ? "opened" : "denied");
    if (l) CloseHandle(l);
    for (i = 0; i < 7; i++) { SET_ERR(0); HANDLE h = U(OpenProcess)(deny[i], FALSE, 4); CHECKF(h == NULL, "System opened with 0x%lx", deny[i]); if (h) CloseHandle(h); else EQV(GetLastError(), ERROR_ACCESS_DENIED); }
    { HANDLE h = U(OpenProcess)(PROCESS_QUERY_INFORMATION, FALSE, 4); t_note("System PROCESS_QUERY_INFORMATION %s", h ? "opened" : "denied"); if (h) CloseHandle(h); }
}
TEST(popen_exited_process_with_open_handle_still_openable) {
    REQUIRE(U(OpenProcess));
    pc_opts o; PROCESS_INFORMATION pi; HANDLE h; DWORD c = 0; memset(&o, 0, sizeof o); o.child = "pc_exit77";
    CHECK(pc_launch(&o, &pi)); WaitForSingleObject(pi.hProcess, PC_TIMEOUT);
    h = U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pi.dwProcessId);
    CHECK(h != NULL);
    if (h) { CHECK(GetExitCodeProcess(h, &c)); EQV(c, 77); EQV(WaitForSingleObject(h, 0), WAIT_OBJECT_0); CloseHandle(h); }
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
}
TEST(popen_exited_process_after_all_handles_closed) {
    REQUIRE(U(OpenProcess));
    pc_opts o; PROCESS_INFORMATION pi; HANDLE h; DWORD pid; memset(&o, 0, sizeof o); o.child = "pc_exit77";
    CHECK(pc_launch(&o, &pi)); WaitForSingleObject(pi.hProcess, PC_TIMEOUT); pid = pi.dwProcessId;
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    Sleep(100);
    SET_ERR(0); h = U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (h) { t_note("pid %lu reused or still referenced", pid); CloseHandle(h); } else { t_note("OpenProcess on dead pid error %lu", GetLastError()); CHECK(GetLastError() == ERROR_INVALID_PARAMETER); }
}
TEST(popen_zero_and_generic_access_observed) {
    REQUIRE(U(OpenProcess));
    HANDLE h; DWORD me = GetCurrentProcessId();
    SET_ERR(0); h = U(OpenProcess)(0, FALSE, me); t_note("OpenProcess(0 access) -> %s err %lu", h ? "handle" : "NULL", GetLastError()); if (h) CloseHandle(h);
    SET_ERR(0); h = U(OpenProcess)(GENERIC_READ, FALSE, me); t_note("OpenProcess(GENERIC_READ) -> %s err %lu", h ? "handle" : "NULL", GetLastError()); if (h) CloseHandle(h);
    SET_ERR(0); h = U(OpenProcess)(GENERIC_ALL, FALSE, me); t_note("OpenProcess(GENERIC_ALL) -> %s err %lu", h ? "handle" : "NULL", GetLastError()); if (h) CloseHandle(h);
    SET_ERR(0); h = U(OpenProcess)(MAXIMUM_ALLOWED, FALSE, me);
    CHECK(h != NULL);
    if (h) { NtQueryObject_pfn q = UN(NtQueryObject); PC_OBI b; memset(&b, 0, sizeof b); if (q && q(h, 0, &b, sizeof b, NULL) >= 0) t_note("MAXIMUM_ALLOWED granted 0x%lx", b.GrantedAccess); CloseHandle(h); }
}
TEST(popen_access_rights_enforced_on_operations) {
    REQUIRE(U(OpenProcess));
    PROCESS_INFORMATION pi; HANDLE sync, lim, vmr; DWORD c = 0; WCHAR name[MAX_PATH]; DWORD n; BYTE b[16]; SIZE_T got = 0;
    CHECK(pc_suspended("pc_sleep", "20000", 0, &pi));
    sync = U(OpenProcess)(SYNCHRONIZE, FALSE, pi.dwProcessId);
    lim = U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pi.dwProcessId);
    vmr = U(OpenProcess)(PROCESS_VM_READ, FALSE, pi.dwProcessId);
    CHECK(sync && lim && vmr);
    if (sync) {
        SET_ERR(0); CHECK(U(GetProcessId)(sync) == 0); EQV(GetLastError(), ERROR_ACCESS_DENIED);
        SET_ERR(0); CHECK(U(GetPriorityClass)(sync) == 0); EQV(GetLastError(), ERROR_ACCESS_DENIED);
        n = MAX_PATH; SET_ERR(0); CHECK(!U(QueryFullProcessImageNameW)(sync, 0, name, &n)); EQV(GetLastError(), ERROR_ACCESS_DENIED);
        SET_ERR(0); CHECK(!ReadProcessMemory(sync, (void *)0x10000, b, 4, &got)); EQV(GetLastError(), ERROR_ACCESS_DENIED);
    }
    if (lim) {
        CHECK(U(GetProcessId)(lim) == pi.dwProcessId);
        CHECK(U(GetPriorityClass)(lim) != 0);
        n = MAX_PATH; CHECK(U(QueryFullProcessImageNameW)(lim, 0, name, &n));
        SET_ERR(0); CHECK(!ReadProcessMemory(lim, (void *)0x10000, b, 4, &got)); EQV(GetLastError(), ERROR_ACCESS_DENIED);
        SET_ERR(0); CHECK(!U(SetPriorityClass)(lim, NORMAL_PRIORITY_CLASS)); EQV(GetLastError(), ERROR_ACCESS_DENIED);
    }
    if (vmr) {
        SET_ERR(0); CHECK(!WriteProcessMemory(vmr, (void *)0x10000, b, 4, &got)); EQV(GetLastError(), ERROR_ACCESS_DENIED);
        SET_ERR(0); CHECK(U(GetProcessId)(vmr) == 0); EQV(GetLastError(), ERROR_ACCESS_DENIED);
    }
    if (sync) CloseHandle(sync);
    if (lim) CloseHandle(lim);
    if (vmr) CloseHandle(vmr);
    pc_kill(&pi);
}
TEST(popen_many_open_close_cycles) {
    REQUIRE(U(OpenProcess));
    int i; for (i = 0; i < 500; i++) { HANDLE h = U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId()); if (!h) { CHECKF(0, "iteration %d err %lu", i, GetLastError()); break; } CloseHandle(h); }
}
TEST(popen_open_parent_of_this_process) {
    REQUIRE(U(OpenProcess));
    PBI pbi; NtQueryInformationProcess_pfn q = UN(NtQueryInformationProcess); HANDLE h; REQUIRE(q);
    memset(&pbi, 0, sizeof pbi); q(GetCurrentProcess(), 0, &pbi, sizeof pbi, NULL);
    SET_ERR(0); h = U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pbi.ppid);
    t_note("parent pid %llu %s (err %lu)", (unsigned long long)pbi.ppid, h ? "opened" : "not available", GetLastError());
    if (h) CloseHandle(h);
}
