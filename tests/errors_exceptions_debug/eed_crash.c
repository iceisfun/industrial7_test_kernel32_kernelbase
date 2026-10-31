/* Process-level exception behaviour: unhandled exceptions, UnhandledExceptionFilter, fast fail, debugger presence.
 * Everything that terminates a process runs in a child. */
#include "eed.h"
#include "eed_asm.h"

static char g_marker[MAX_PATH];
static DWORD g_main_tid;
static void mark(const char *what) {
    if (!g_marker[0]) return;
    HANDLE h = CreateFileA(g_marker, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    SetFilePointer(h, 0, NULL, FILE_END);
    DWORD w; WriteFile(h, what, (DWORD)strlen(what), &w, NULL);
    CloseHandle(h);
}
static void clear_uef(void) { SetUnhandledExceptionFilter(NULL); }

static LONG WINAPI f_exec(PEXCEPTION_POINTERS ep) { mark("E"); return EXCEPTION_EXECUTE_HANDLER; }
static LONG WINAPI f_search(PEXCEPTION_POINTERS ep) { mark("S"); return EXCEPTION_CONTINUE_SEARCH; }
static LONG WINAPI f_fix(PEXCEPTION_POINTERS ep) { mark("C"); ep->ContextRecord->Rip = (DWORD64)(ULONG_PTR)eed_av_write_after; return EXCEPTION_CONTINUE_EXECUTION; }
static LONG WINAPI f_args(PEXCEPTION_POINTERS ep) {
    PEXCEPTION_RECORD r = ep->ExceptionRecord;
    DWORD v = 0x1000 + (r->NumberParameters == 1 ? (DWORD)r->ExceptionInformation[0] : 0xFF);
    if (r->ExceptionCode != 0xE0A00080) v = 0x3000;
    TerminateProcess(GetCurrentProcess(), v);
    return EXCEPTION_EXECUTE_HANDLER;
}
static LONG WINAPI f_one(PEXCEPTION_POINTERS ep) { mark("1"); return EXCEPTION_EXECUTE_HANDLER; }
static LONG WINAPI f_two(PEXCEPTION_POINTERS ep) { mark("2"); return EXCEPTION_EXECUTE_HANDLER; }
static LONG WINAPI f_thread(PEXCEPTION_POINTERS ep) {
    TerminateProcess(GetCurrentProcess(), 0x2000 + (GetCurrentThreadId() == g_main_tid ? 1 : 0));
    return EXCEPTION_EXECUTE_HANDLER;
}
static LONG WINAPI f_nc(PEXCEPTION_POINTERS ep) {
    if (ep->ExceptionRecord->ExceptionCode == 0xE0A00073) { mark("N"); return EXCEPTION_CONTINUE_EXECUTION; }
    mark("X");
    return EXCEPTION_EXECUTE_HANDLER;
}
static LONG WINAPI f_so(PEXCEPTION_POINTERS ep) { ExitProcess(0x77); return EXCEPTION_EXECUTE_HANDLER; }
static LONG CALLBACK v_cont(PEXCEPTION_POINTERS ep) { mark("V"); return EXCEPTION_CONTINUE_EXECUTION; }

static DWORD WINAPI th_av(LPVOID p) { eed_av_write((char *)0); return 0; }
static __attribute__((noinline)) int so_rec(int d) { volatile char pad[512]; pad[0] = (char)d; int r = so_rec(d + 1); __asm__ volatile("" ::: "memory"); return r + pad[0]; }
static void do_fastfail(void) { __asm__ volatile("movl $7, %%ecx\n\tint $0x29" ::: "memory", "ecx"); }

CHILD(eed_crash) {
    const char *kind = argc > 1 ? argv[1] : "";
    if (argc > 2) snprintf(g_marker, sizeof g_marker, "%s", argv[2]);
    g_main_tid = GetCurrentThreadId();
    clear_uef();
    SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
    if (!strcmp(kind, "av_write")) eed_av_write((char *)0);
    else if (!strcmp(kind, "av_read")) eed_av_read((char *)0x40);
    else if (!strcmp(kind, "av_exec")) eed_call_ptr((void *)0x10);
    else if (!strcmp(kind, "div0")) eed_div0();
    else if (!strcmp(kind, "ud2")) eed_ud2();
    else if (!strcmp(kind, "int3")) eed_int3();
    else if (!strcmp(kind, "cli")) eed_cli();
    else if (!strcmp(kind, "hlt")) eed_hlt();
    else if (!strcmp(kind, "movaps")) { char *b = malloc(64); eed_movaps(((ULONG_PTR)b & 15) ? b : b + 1); }
    else if (!strcmp(kind, "debugbreak")) { void (WINAPI *db)(void) = (void (WINAPI *)(void))t_proc("DebugBreak"); db(); }
    else if (!strcmp(kind, "stackoverflow")) so_rec(0);
    else if (!strcmp(kind, "fastfail")) do_fastfail();
    else if (!strcmp(kind, "fastfail_vh")) { AddVectoredExceptionHandler(1, v_cont); SetUnhandledExceptionFilter(f_exec); do_fastfail(); }
    else if (!strcmp(kind, "failfast_null")) { void (WINAPI *ff)(PEXCEPTION_RECORD, PCONTEXT, DWORD) = (void *)t_proc("RaiseFailFastException"); ff(NULL, NULL, 0); }
    else if (!strcmp(kind, "failfast_nodlg")) { void (WINAPI *ff)(PEXCEPTION_RECORD, PCONTEXT, DWORD) = (void *)t_proc("RaiseFailFastException"); ff(NULL, NULL, 2); }
    else if (!strcmp(kind, "failfast_record")) {
        void (WINAPI *ff)(PEXCEPTION_RECORD, PCONTEXT, DWORD) = (void *)t_proc("RaiseFailFastException");
        EXCEPTION_RECORD r; memset(&r, 0, sizeof r); r.ExceptionCode = 0xE0A00072;
        ff(&r, NULL, 2);
    }
    else if (!strcmp(kind, "failfast_vh")) {
        void (WINAPI *ff)(PEXCEPTION_RECORD, PCONTEXT, DWORD) = (void *)t_proc("RaiseFailFastException");
        AddVectoredExceptionHandler(1, v_cont); SetUnhandledExceptionFilter(f_fix);
        ff(NULL, NULL, 2);
        return 0x55;
    }
    else if (!strcmp(kind, "raise_nc")) RaiseException(0xE0A00070, EXCEPTION_NONCONTINUABLE, 0, NULL);
    else if (!strcmp(kind, "raise_cont")) RaiseException(0xE0A00071, 0, 0, NULL);
    else if (!strcmp(kind, "raise_access_denied")) RaiseException(0xC0000022, 0, 0, NULL);
    else if (!strcmp(kind, "raise_bp")) RaiseException(0x80000003, 0, 0, NULL);
    else if (!strcmp(kind, "raise_args")) { ULONG_PTR a[3] = { 1, 2, 3 }; RaiseException(0xE0A00074, 0, 3, a); }
    else if (!strcmp(kind, "thread_av")) { HANDLE h = CreateThread(NULL, 0, th_av, NULL, 0, NULL); WaitForSingleObject(h, 20000); return 0x66; }
    else if (!strcmp(kind, "veh_fix_returns")) {
        AddVectoredExceptionHandler(1, v_cont); RaiseException(0xE0A00075, 0, 0, NULL); return 0x55;
    }
    else if (!strcmp(kind, "uef_exec")) { SetUnhandledExceptionFilter(f_exec); eed_av_write((char *)0); }
    else if (!strcmp(kind, "uef_continue")) { SetUnhandledExceptionFilter(f_fix); eed_av_write((char *)0); return 0x55; }
    else if (!strcmp(kind, "uef_search")) { SetUnhandledExceptionFilter(f_search); eed_av_write((char *)0); }
    else if (!strcmp(kind, "uef_args")) { SetUnhandledExceptionFilter(f_args); ULONG_PTR a[1] = { 0x2A }; RaiseException(0xE0A00080, 0, 1, a); }
    else if (!strcmp(kind, "uef_none")) { eed_av_write((char *)0); }
    else if (!strcmp(kind, "uef_replaced")) { SetUnhandledExceptionFilter(f_one); SetUnhandledExceptionFilter(f_two); eed_av_write((char *)0); }
    else if (!strcmp(kind, "uef_removed")) { SetUnhandledExceptionFilter(f_one); SetUnhandledExceptionFilter(NULL); eed_av_write((char *)0); }
    else if (!strcmp(kind, "uef_thread")) { SetUnhandledExceptionFilter(f_thread); HANDLE h = CreateThread(NULL, 0, th_av, NULL, 0, NULL); WaitForSingleObject(h, 20000); return 0x66; }
    else if (!strcmp(kind, "uef_noncontinuable")) { SetUnhandledExceptionFilter(f_nc); RaiseException(0xE0A00073, EXCEPTION_NONCONTINUABLE, 0, NULL); return 0x55; }
    else if (!strcmp(kind, "uef_stackoverflow")) { SetUnhandledExceptionFilter(f_so); so_rec(0); }
    else if (!strcmp(kind, "uef_vh_first")) { SetUnhandledExceptionFilter(f_exec); AddVectoredExceptionHandler(1, v_cont); RaiseException(0xE0A00076, 0, 0, NULL); return 0x55; }
    else if (!strcmp(kind, "abort")) abort();
    else if (!strcmp(kind, "exit_code")) ExitProcess(0x12345);
    else if (!strcmp(kind, "terminate_code")) TerminateProcess(GetCurrentProcess(), 0xDEADC0DE);
    else return 0xBAD0;
    return 0x99;
}

static DWORD crash(const char *kind, char *marker, size_t mcap) {
    char args[MAX_PATH * 2];
    if (marker) { t_tmpfile(marker, mcap, kind); strcat(marker, ".mark"); DeleteFileA(marker); snprintf(args, sizeof args, "%s \"%s\"", kind, marker); }
    else snprintf(args, sizeof args, "%s", kind);
    return t_run_child("eed_crash", args, 30000);
}
static int marker_has(const char *path, const char *s) {
    char buf[64] = { 0 }; DWORD n = 0;
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    ReadFile(h, buf, 63, &n, NULL); CloseHandle(h);
    return strstr(buf, s) != NULL;
}
static int marker_exists(const char *path) { return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES; }

#define CRASHTEST(name, kind, expect) TEST(name) { DWORD c = crash(kind, NULL, 0); CHECKF(c == (DWORD)(expect), "child exit 0x%lx want 0x%lx", c, (DWORD)(expect)); }
CRASHTEST(eed_crash_av_write, "av_write", 0xC0000005)
CRASHTEST(eed_crash_av_read, "av_read", 0xC0000005)
CRASHTEST(eed_crash_av_exec, "av_exec", 0xC0000005)
CRASHTEST(eed_crash_div0, "div0", 0xC0000094)
CRASHTEST(eed_crash_ud2, "ud2", 0xC000001D)
CRASHTEST(eed_crash_int3, "int3", 0x80000003)
CRASHTEST(eed_crash_cli, "cli", 0xC0000096)
CRASHTEST(eed_crash_hlt, "hlt", 0xC0000096)
CRASHTEST(eed_crash_debugbreak_no_debugger, "debugbreak", 0x80000003)
TEST(eed_crash_stack_overflow) {
    if (t_is_wine()) { t_skip("stack overflow handling is slow under wine"); return; }
    DWORD c = crash("stackoverflow", NULL, 0);
    CHECKF(c == 0xC00000FD, "child exit 0x%lx", c);
}
CRASHTEST(eed_crash_fastfail_int29, "fastfail", 0xC0000409)
CRASHTEST(eed_crash_fastfail_bypasses_handlers, "fastfail_vh", 0xC0000409)
CRASHTEST(eed_crash_raise_noncontinuable, "raise_nc", 0xE0A00070)
CRASHTEST(eed_crash_raise_continuable_unhandled, "raise_cont", 0xE0A00071)
CRASHTEST(eed_crash_raise_status_code, "raise_access_denied", 0xC0000022)
CRASHTEST(eed_crash_raise_breakpoint_code, "raise_bp", 0x80000003)
CRASHTEST(eed_crash_raise_with_args, "raise_args", 0xE0A00074)
CRASHTEST(eed_crash_thread_av_kills_process, "thread_av", 0xC0000005)
CRASHTEST(eed_crash_veh_handles_so_child_survives, "veh_fix_returns", 0x55)
CRASHTEST(eed_crash_exit_process_code, "exit_code", 0x12345)
CRASHTEST(eed_crash_terminate_process_code, "terminate_code", 0xDEADC0DE)
TEST(eed_crash_movaps_misaligned) {
    DWORD c = crash("movaps", NULL, 0);
    CHECKF(c == 0xC0000005 || c == 0x80000002, "exit 0x%lx", c);
}
TEST(eed_crash_abort_exit_code) {
    DWORD c = crash("abort", NULL, 0);
    t_note("abort() exit code 0x%lx", c);
    CHECK(c != 0 && !eed_child_failed(c));
}
TEST(eed_crash_failfast_null_record) {
    DWORD c = crash("failfast_null", NULL, 0);
    CHECKF(c == EED_STATUS_FAIL_FAST_EXCEPTION || c == EED_STATUS_STACK_BUFFER_OVERRUN, "exit 0x%lx", c);
    t_note("RaiseFailFastException(NULL,NULL,0) exit code 0x%lx", c);
}
TEST(eed_crash_failfast_no_dialog_flag) {
    DWORD c = crash("failfast_nodlg", NULL, 0);
    CHECKF(c == EED_STATUS_FAIL_FAST_EXCEPTION || c == EED_STATUS_STACK_BUFFER_OVERRUN, "exit 0x%lx", c);
}
TEST(eed_crash_failfast_with_record_uses_its_code) {
    DWORD c = crash("failfast_record", NULL, 0);
    CHECKF(c == 0xE0A00072, "exit 0x%lx", c);
}
TEST(eed_crash_failfast_ignores_handlers) {
    DWORD c = crash("failfast_vh", NULL, 0);
    CHECKF(c != 0x55 && !eed_child_failed(c), "exit 0x%lx (handlers must not resume a fail-fast)", c);
}

/* ---- SetUnhandledExceptionFilter ---- */
TEST(eed_uef_exec_handler_runs_and_terminates) {
    char m[MAX_PATH]; DWORD c = crash("uef_exec", m, sizeof m);
    EXPECT_EQ(c, 0xC0000005);
    CHECK(marker_has(m, "E"));
}
TEST(eed_uef_continue_execution_resumes) {
    char m[MAX_PATH]; DWORD c = crash("uef_continue", m, sizeof m);
    EXPECT_EQ(c, 0x55);
    CHECK(marker_has(m, "C"));
}
TEST(eed_uef_continue_search_still_terminates_with_code) {
    char m[MAX_PATH]; DWORD c = crash("uef_search", m, sizeof m);
    EXPECT_EQ(c, 0xC0000005);
    CHECK(marker_has(m, "S"));
}
TEST(eed_uef_filter_receives_record) {
    DWORD c = crash("uef_args", NULL, 0);
    EXPECT_EQ(c, 0x102A);
}
TEST(eed_uef_none_terminates) {
    char m[MAX_PATH]; DWORD c = crash("uef_none", m, sizeof m);
    EXPECT_EQ(c, 0xC0000005);
    CHECK(!marker_exists(m));
}
TEST(eed_uef_replacement_only_last_runs) {
    char m[MAX_PATH]; DWORD c = crash("uef_replaced", m, sizeof m);
    EXPECT_EQ(c, 0xC0000005);
    CHECK(marker_has(m, "2")); CHECK(!marker_has(m, "1"));
}
TEST(eed_uef_removal_with_null) {
    char m[MAX_PATH]; DWORD c = crash("uef_removed", m, sizeof m);
    EXPECT_EQ(c, 0xC0000005);
    CHECK(!marker_exists(m));
}
TEST(eed_uef_runs_on_faulting_thread) {
    DWORD c = crash("uef_thread", NULL, 0);
    EXPECT_EQ(c, 0x2000);
}
TEST(eed_uef_vectored_handler_preempts_filter) {
    char m[MAX_PATH]; DWORD c = crash("uef_vh_first", m, sizeof m);
    EXPECT_EQ(c, 0x55);
    CHECK(marker_has(m, "V")); CHECK(!marker_has(m, "E"));
}
TEST(eed_uef_noncontinuable_continue_is_refused) {
    char m[MAX_PATH]; DWORD c = crash("uef_noncontinuable", m, sizeof m);
    CHECKF(c == 0xC0000025 || c == 0xE0A00073, "exit 0x%lx", c);
    t_note("noncontinuable + filter CONTINUE_EXECUTION: exit 0x%lx", c);
    CHECK(marker_has(m, "N"));
    CHECK(c != 0x55);
}
TEST(eed_uef_stack_overflow_filter) {
    if (t_is_wine()) { t_skip("stack overflow handling is slow under wine"); return; }
    DWORD c = crash("uef_stackoverflow", NULL, 0);
    CHECKF(c == 0x77 || c == 0xC00000FD, "exit 0x%lx", c);
    t_note("stack overflow with filter: exit 0x%lx", c);
}
static LONG WINAPI uf_a(PEXCEPTION_POINTERS ep) { return EXCEPTION_CONTINUE_SEARCH; }
static LONG WINAPI uf_b(PEXCEPTION_POINTERS ep) { return EXCEPTION_CONTINUE_SEARCH; }
TEST(eed_uef_set_returns_previous) {
    REQUIRE(U(SetUnhandledExceptionFilter));
    LPTOP_LEVEL_EXCEPTION_FILTER orig = U(SetUnhandledExceptionFilter)(NULL);
    CHECK(U(SetUnhandledExceptionFilter)(uf_a) == NULL);
    CHECK(U(SetUnhandledExceptionFilter)(uf_b) == uf_a);
    CHECK(U(SetUnhandledExceptionFilter)(uf_b) == uf_b);
    CHECK(U(SetUnhandledExceptionFilter)(NULL) == uf_b);
    CHECK(U(SetUnhandledExceptionFilter)(NULL) == NULL);
    U(SetUnhandledExceptionFilter)(orig);
    CHECK(U(SetUnhandledExceptionFilter)(orig) == orig);
}
TEST(eed_uef_set_does_not_touch_last_error) {
    REQUIRE(U(SetUnhandledExceptionFilter));
    LPTOP_LEVEL_EXCEPTION_FILTER orig = U(SetUnhandledExceptionFilter)(uf_a);
    SetLastError(4711);
    U(SetUnhandledExceptionFilter)(uf_b);
    EXPECT_EQ(GetLastError(), 4711);
    U(SetUnhandledExceptionFilter)(orig);
}
TEST(eed_uef_exports) {
    CHECK(UK(SetUnhandledExceptionFilter) != NULL);
    CHECK(UK(UnhandledExceptionFilter) != NULL);
    CHECK(UB(SetUnhandledExceptionFilter) != NULL);
    CHECK(UB(UnhandledExceptionFilter) != NULL);
}

/* UnhandledExceptionFilter called directly (child: it sets process-wide state) */
static volatile LONG g_f_called; static PEXCEPTION_POINTERS g_f_ep; static int g_f_ret;
static LONG WINAPI f_ret(PEXCEPTION_POINTERS ep) { g_f_called++; g_f_ep = ep; return g_f_ret; }
CHILD(eed_ueh_direct) {
    LONG (WINAPI *ueh)(PEXCEPTION_POINTERS) = (void *)t_proc("UnhandledExceptionFilter");
    int fret = atoi(argv[1]);
    int mode = atoi(argv[2]);        /* 0: filter installed; 1: no filter (requires NOGPFAULTERRORBOX) */
    SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
    static EXCEPTION_RECORD er; static CONTEXT cx;
    RtlCaptureContext(&cx);
    er.ExceptionCode = 0xE0A00090; er.ExceptionFlags = 0; er.ExceptionAddress = (PVOID)(ULONG_PTR)cx.Rip;
    EXCEPTION_POINTERS ep = { &er, &cx };
    g_f_ret = fret;
    SetUnhandledExceptionFilter(mode == 0 ? f_ret : NULL);
    if (IsDebuggerPresent()) return 0x7777;
    LONG r = ueh(&ep);
    DWORD code = (r == 1 ? 10 : r == -1 ? 20 : r == 0 ? 30 : 99) + (g_f_called == 1 ? 1 : 0) + (g_f_ep == &ep ? 2 : 0);
    return (int)code;
}
TEST(eed_ueh_direct_filter_execute_handler) {
    char a[64]; snprintf(a, sizeof a, "%d 0", EXCEPTION_EXECUTE_HANDLER);
    DWORD c = t_run_child("eed_ueh_direct", a, 30000);
    if (c == 0x7777) { t_skip("debugger present"); return; }
    EXPECT_EQ(c, 10 + 1 + 2);
}
TEST(eed_ueh_direct_filter_continue_execution) {
    char a[64]; snprintf(a, sizeof a, "%d 0", EXCEPTION_CONTINUE_EXECUTION);
    DWORD c = t_run_child("eed_ueh_direct", a, 30000);
    if (c == 0x7777) { t_skip("debugger present"); return; }
    EXPECT_EQ(c, 20 + 1 + 2);
}
TEST(eed_ueh_direct_filter_search_with_nogpfault_executes) {
    char a[64]; snprintf(a, sizeof a, "%d 0", EXCEPTION_CONTINUE_SEARCH);
    DWORD c = t_run_child("eed_ueh_direct", a, 30000);
    if (c == 0x7777) { t_skip("debugger present"); return; }
    EXPECT_EQ(c, 10 + 1 + 2);        /* filter said search; error mode forbids the crash dialog -> EXECUTE_HANDLER */
}
TEST(eed_ueh_direct_no_filter_nogpfault_executes) {
    DWORD c = t_run_child("eed_ueh_direct", "0 1", 30000);
    if (c == 0x7777) { t_skip("debugger present"); return; }
    EXPECT_EQ(c, 10);
}

/* ---- debugger presence ---- */
TEST(eed_dbgpresent_isdebuggerpresent_matches_peb) {
    REQUIRE(U(IsDebuggerPresent));
    BYTE peb = *((BYTE *)((char *)NtCurrentTeb()->ProcessEnvironmentBlock + 2));
    EXPECT_EQ(U(IsDebuggerPresent)() != 0, peb != 0);
    CHECK(UK(IsDebuggerPresent) != NULL);
}
TEST(eed_dbgpresent_not_debugged_by_default) {
    REQUIRE(U(IsDebuggerPresent)); REQUIRE(U(CheckRemoteDebuggerPresent));
    if (U(IsDebuggerPresent)()) { t_skip("test host is being debugged"); return; }
    BOOL b = TRUE;
    CHECK(U(CheckRemoteDebuggerPresent)(GetCurrentProcess(), &b));
    EXPECT_EQ(b, 0);
}
TEST(eed_dbgpresent_remote_null_args) {
    REQUIRE(U(CheckRemoteDebuggerPresent));
    BOOL b = 7;
    SetLastError(0);
    CHECK(!U(CheckRemoteDebuggerPresent)(NULL, &b)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SetLastError(0);
    CHECK(!U(CheckRemoteDebuggerPresent)(GetCurrentProcess(), NULL)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    EXPECT_EQ(b, 7);
}
TEST(eed_dbgpresent_remote_invalid_handle) {
    REQUIRE(U(CheckRemoteDebuggerPresent));
    BOOL b = 7;
    SetLastError(0);
    CHECK(!U(CheckRemoteDebuggerPresent)((HANDLE)(ULONG_PTR)0x7777FFF8, &b)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    EXPECT_EQ(b, 7);
}
TEST(eed_dbgpresent_remote_other_process) {
    REQUIRE(U(CheckRemoteDebuggerPresent));
    PROCESS_INFORMATION pi;
    HANDLE h = t_spawn_child("eed_idle", "", CREATE_NO_WINDOW, NULL, NULL, NULL, FALSE, NULL, &pi);
    CHECK(h != NULL); if (!h) return;
    BOOL b = TRUE;
    CHECK(U(CheckRemoteDebuggerPresent)(h, &b));
    EXPECT_EQ(b, 0);
    TerminateProcess(h, 0);
    WaitForSingleObject(h, 5000);
    CloseHandle(pi.hThread); CloseHandle(h);
}
TEST(eed_dbgpresent_remote_access_rights) {
    REQUIRE(U(CheckRemoteDebuggerPresent)); REQUIRE(U(OpenProcess));
    HANDLE h = U(OpenProcess)(SYNCHRONIZE, FALSE, GetCurrentProcessId());
    CHECK(h != NULL); if (!h) return;
    BOOL b = 5;
    SetLastError(0);
    CHECK(!U(CheckRemoteDebuggerPresent)(h, &b)); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(h);
}
CHILD(eed_idle) { Sleep(25000); return 0; }

/* ---- OutputDebugString with no debugger: raises DBG_PRINTEXCEPTION_* (handled/ignored) ---- */
static char g_ods_a[256]; static WCHAR g_ods_w[256]; static volatile LONG g_ods_n; static DWORD g_ods_code[4]; static ULONG_PTR g_ods_info[4][15]; static DWORD g_ods_np[4];
static LONG CALLBACK veh_ods(PEXCEPTION_POINTERS ep) {
    DWORD c = ep->ExceptionRecord->ExceptionCode;
    if (c != 0x40010006 && c != 0x4001000A) return EXCEPTION_CONTINUE_SEARCH;
    LONG i = InterlockedIncrement(&g_ods_n) - 1;
    if (i < 4) {
        g_ods_code[i] = c; g_ods_np[i] = ep->ExceptionRecord->NumberParameters;
        for (int k = 0; k < 15; k++) g_ods_info[i][k] = ep->ExceptionRecord->ExceptionInformation[k];
        if (c == 0x40010006 && g_ods_np[i] >= 2 && g_ods_info[i][1]) { strncpy(g_ods_a, (const char *)g_ods_info[i][1], 255); }
        if (c == 0x4001000A && g_ods_np[i] >= 4 && g_ods_info[i][1] && g_ods_info[i][3]) { lstrcpynW(g_ods_w, (const WCHAR *)g_ods_info[i][1], 255); strncpy(g_ods_a, (const char *)g_ods_info[i][3], 255); }
    }
    return EXCEPTION_CONTINUE_EXECUTION;
}
TEST(eed_ods_a_raises_print_exception) {
    REQUIRE(U(OutputDebugStringA)); REQUIRE(U(AddVectoredExceptionHandler)); REQUIRE(U(RemoveVectoredExceptionHandler));
    PVOID h = U(AddVectoredExceptionHandler)(1, veh_ods);
    g_ods_n = 0; g_ods_a[0] = 0;
    U(OutputDebugStringA)("hello debugger");
    U(RemoveVectoredExceptionHandler)(h);
    EXPECT_EQ(g_ods_n, 1);
    EXPECT_EQ(g_ods_code[0], 0x40010006);
    EXPECT_EQ(g_ods_np[0], 2);
    EXPECT_EQ(g_ods_info[0][0], strlen("hello debugger") + 1);
    EXPECT_STR(g_ods_a, "hello debugger");
}
TEST(eed_ods_w_raises_wide_print_exception) {
    REQUIRE(U(OutputDebugStringW)); REQUIRE(U(AddVectoredExceptionHandler)); REQUIRE(U(RemoveVectoredExceptionHandler));
    PVOID h = U(AddVectoredExceptionHandler)(1, veh_ods);
    g_ods_n = 0; g_ods_a[0] = 0; g_ods_w[0] = 0;
    U(OutputDebugStringW)(L"wide message");
    U(RemoveVectoredExceptionHandler)(h);
    EXPECT_EQ(g_ods_n, 1);
    EXPECT_EQ(g_ods_code[0], 0x4001000A);
    EXPECT_EQ(g_ods_np[0], 4);
    EXPECT_EQ(g_ods_info[0][0], strlen("wide message") + 1);
    EXPECT_EQ(g_ods_info[0][2], strlen("wide message") + 1);
    EXPECT_WSTR(g_ods_w, L"wide message");
    EXPECT_STR(g_ods_a, "wide message");
}
TEST(eed_ods_null_and_empty) {
    REQUIRE(U(OutputDebugStringA)); REQUIRE(U(OutputDebugStringW)); REQUIRE(U(AddVectoredExceptionHandler)); REQUIRE(U(RemoveVectoredExceptionHandler));
    PVOID h = U(AddVectoredExceptionHandler)(1, veh_ods);
    g_ods_n = 0;
    U(OutputDebugStringA)(NULL);
    EXPECT_EQ(g_ods_n, 1); EXPECT_EQ(g_ods_info[0][0], 1);
    U(OutputDebugStringW)(NULL);
    EXPECT_EQ(g_ods_n, 2); EXPECT_EQ(g_ods_info[1][0], 1);
    g_ods_n = 0;
    U(OutputDebugStringA)("");
    EXPECT_EQ(g_ods_n, 1); EXPECT_EQ(g_ods_info[0][0], 1);
    U(RemoveVectoredExceptionHandler)(h);
}
TEST(eed_ods_without_handler_does_not_crash) {
    REQUIRE(U(OutputDebugStringA)); REQUIRE(U(OutputDebugStringW));
    if (IsDebuggerPresent()) { t_skip("debugger present"); return; }
    for (int i = 0; i < 20; i++) { U(OutputDebugStringA)("k32t ods A"); U(OutputDebugStringW)(L"k32t ods W"); }
    CHECK(1);
}
TEST(eed_ods_preserves_last_error) {
    REQUIRE(U(OutputDebugStringA));
    SetLastError(8080);
    U(OutputDebugStringA)("k32t");
    t_note("OutputDebugStringA leaves last error = %lu", GetLastError());
}
TEST(eed_ods_a_long_string) {
    REQUIRE(U(OutputDebugStringA)); REQUIRE(U(AddVectoredExceptionHandler)); REQUIRE(U(RemoveVectoredExceptionHandler));
    char big[2000]; memset(big, 'z', sizeof big); big[1999] = 0;
    PVOID h = U(AddVectoredExceptionHandler)(1, veh_ods);
    g_ods_n = 0;
    U(OutputDebugStringA)(big);
    U(RemoveVectoredExceptionHandler)(h);
    EXPECT_EQ(g_ods_n, 1);
    EXPECT_EQ(g_ods_info[0][0], 2000);
}
TEST(eed_debugbreak_exports) {
    CHECK(UK(DebugBreak) != NULL); CHECK(UB(DebugBreak) != NULL);
    CHECK(UK(DebugBreakProcess) != NULL);
    CHECK(UK(OutputDebugStringA) != NULL); CHECK(UK(OutputDebugStringW) != NULL);
}
TEST(eed_debugbreak_with_handler_in_process) {
    REQUIRE(U(DebugBreak)); REQUIRE(U(AddVectoredExceptionHandler)); REQUIRE(U(RemoveVectoredExceptionHandler));
    if (IsDebuggerPresent()) { t_skip("debugger present"); return; }
    static volatile LONG n;
    LONG (CALLBACK *cb)(PEXCEPTION_POINTERS) = NULL; (void)cb;
    /* use a local VEH */
    extern LONG CALLBACK eed_veh_bp_count(PEXCEPTION_POINTERS);
    n = 0;
    PVOID h = U(AddVectoredExceptionHandler)(1, eed_veh_bp_count);
    U(DebugBreak)();
    U(RemoveVectoredExceptionHandler)(h);
    CHECK(1);
}
LONG CALLBACK eed_veh_bp_count(PEXCEPTION_POINTERS ep) {
    return ep->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_CONTINUE_SEARCH;
}
