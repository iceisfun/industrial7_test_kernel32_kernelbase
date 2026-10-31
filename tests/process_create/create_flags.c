/* CreateProcess: creation flags, suspended start state, debugging, jobs, console attachment. */
#include "pc.h"

typedef struct { LONG_PTR st; BYTE *peb; ULONG_PTR aff; LONG prio; ULONG_PTR pid; ULONG_PTR ppid; } PC_PBI;
static BYTE *child_image_base(HANDLE hp) {
    NtQueryInformationProcess_pfn q = UN(NtQueryInformationProcess); PC_PBI pbi; BYTE *base = NULL; SIZE_T n = 0;
    if (!q) return NULL;
    memset(&pbi, 0, sizeof pbi);
    if (q(hp, 0, &pbi, sizeof pbi, NULL) < 0 || !pbi.peb) return NULL;
    if (!ReadProcessMemory(hp, pbi.peb + 0x10, &base, sizeof base, &n)) return NULL;
    return base;
}
static DWORD self_entry_rva(void) {
    BYTE *b = (BYTE *)GetModuleHandleW(NULL); IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(b + ((IMAGE_DOS_HEADER *)b)->e_lfanew);
    return nt->OptionalHeader.AddressOfEntryPoint;
}

/* ---------- CREATE_SUSPENDED ---------- */
TEST(pcflag_suspended_resume_count_semantics) {
    REQUIRE(UK(CreateProcessW));
    PROCESS_INFORMATION pi; pc_opts o;
    memset(&o, 0, sizeof o); o.child = "pc_sleep"; o.args = "6000"; o.flags = CREATE_SUSPENDED;
    CHECK(pc_launch(&o, &pi));
    EQV(SuspendThread(pi.hThread), 1);      /* count goes 1 -> 2 */
    EQV(ResumeThread(pi.hThread), 2);
    EQV(ResumeThread(pi.hThread), 1);       /* now running */
    EQV(ResumeThread(pi.hThread), 0);       /* not suspended */
    EQV(WaitForSingleObject(pi.hProcess, 200), WAIT_TIMEOUT);
    pc_kill(&pi);
}
TEST(pcflag_suspended_process_is_still_active) {
    REQUIRE(UK(CreateProcessW));
    PROCESS_INFORMATION pi; DWORD code = 0;
    CHECK(pc_suspended("pc_exit77", "", 0, &pi));
    CHECK(GetExitCodeProcess(pi.hProcess, &code)); EQV(code, STILL_ACTIVE);
    CHECK(GetExitCodeThread(pi.hThread, &code)); EQV(code, STILL_ACTIVE);
    pc_kill(&pi);
}
TEST(pcflag_suspended_start_context) {
    REQUIRE(UK(CreateProcessW)); REQUIRE(U(GetThreadContext));
    PROCESS_INFORMATION pi; CONTEXT ctx; BYTE *base; HMODULE nt = t_ntdll(); ULONG_PTR rtlstart;
    CHECK(pc_suspended("pc_exit77", "", 0, &pi));
    base = child_image_base(pi.hProcess); CHECK(base != NULL);
    memset(&ctx, 0, sizeof ctx); ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    CHECK(U(GetThreadContext)(pi.hThread, &ctx));
    if (base) { EQV(ctx.Rcx, (ULONG_PTR)base + self_entry_rva()); }
    rtlstart = (ULONG_PTR)GetProcAddress(nt, "RtlUserThreadStart");
    t_note("suspended start: Rip=%p (RtlUserThreadStart=%p) Rdx=%p", (void *)ctx.Rip, (void *)rtlstart, (void *)ctx.Rdx);
    CHECK(ctx.Rsp != 0);
    CHECK((ctx.Rsp & 0xF) == 8 || (ctx.Rsp & 0xF) == 0);
    pc_kill(&pi);
}
TEST(pcflag_suspended_image_base_is_pe_header) {
    REQUIRE(UK(CreateProcessW));
    PROCESS_INFORMATION pi; BYTE *base; WORD mz = 0; SIZE_T n = 0;
    CHECK(pc_suspended("pc_exit77", "", 0, &pi));
    base = child_image_base(pi.hProcess); CHECK(base != NULL);
    if (base) { CHECK(ReadProcessMemory(pi.hProcess, base, &mz, 2, &n)); EQV(mz, 0x5A4D); }
    pc_kill(&pi);
}
TEST(pcflag_suspended_parent_id_is_creator) {
    REQUIRE(UK(CreateProcessW));
    PROCESS_INFORMATION pi; PC_PBI pbi; NtQueryInformationProcess_pfn q = UN(NtQueryInformationProcess); REQUIRE(q);
    CHECK(pc_suspended("pc_exit77", "", 0, &pi)); memset(&pbi, 0, sizeof pbi);
    CHECK(q(pi.hProcess, 0, &pbi, sizeof pbi, NULL) >= 0);
    EQV(pbi.ppid, GetCurrentProcessId()); EQV(pbi.pid, pi.dwProcessId);
    pc_kill(&pi);
}

/* ---------- priority class ---------- */
TEST(pcflag_priority_class_flags) {
    REQUIRE(UK(CreateProcessW)); REQUIRE(U(GetPriorityClass));
    static const DWORD cls[] = { IDLE_PRIORITY_CLASS, BELOW_NORMAL_PRIORITY_CLASS, NORMAL_PRIORITY_CLASS, ABOVE_NORMAL_PRIORITY_CLASS, HIGH_PRIORITY_CLASS };
    int i;
    for (i = 0; i < 5; i++) {
        PROCESS_INFORMATION pi;
        CHECKF(pc_suspended("pc_exit77", "", cls[i], &pi), "class 0x%lx", cls[i]);
        EQV(U(GetPriorityClass)(pi.hProcess), cls[i]);
        pc_kill(&pi);
    }
}
TEST(pcflag_priority_class_default_is_normal) {
    REQUIRE(UK(CreateProcessW));
    PROCESS_INFORMATION pi;
    if (U(GetPriorityClass)(GetCurrentProcess()) != NORMAL_PRIORITY_CLASS) { t_skip("harness not at NORMAL class"); return; }
    CHECK(pc_suspended("pc_exit77", "", 0, &pi));
    EQV(U(GetPriorityClass)(pi.hProcess), NORMAL_PRIORITY_CLASS);
    pc_kill(&pi);
}
TEST(pcflag_priority_class_set_afterwards) {
    REQUIRE(UK(CreateProcessW)); REQUIRE(U(SetPriorityClass));
    PROCESS_INFORMATION pi; static const DWORD cls[] = { IDLE_PRIORITY_CLASS, BELOW_NORMAL_PRIORITY_CLASS, NORMAL_PRIORITY_CLASS, ABOVE_NORMAL_PRIORITY_CLASS, HIGH_PRIORITY_CLASS }; int i;
    CHECK(pc_suspended("pc_exit77", "", 0, &pi));
    for (i = 0; i < 5; i++) { CHECK(U(SetPriorityClass)(pi.hProcess, cls[i])); EQV(U(GetPriorityClass)(pi.hProcess), cls[i]); }
    pc_kill(&pi);
}
static void prio_inherit(DWORD cls, DWORD expect_child) {
    pc_opts o; char out[4096]; DWORD code; char a[32];
    snprintf(a, sizeof a, "0x%lx", cls);
    memset(&o, 0, sizeof o); o.child = "pc_prio_parent"; o.args = a;
    CHECK(pc_capture(&o, out, sizeof out, NULL, 0, &code, PC_TIMEOUT));
    EQV(code, 0);
    EQV(pc_unum(out, "self", 0), cls);
    EQV(pc_unum(out, "child", 0), expect_child);
}
TEST(pcflag_priority_idle_is_inherited) { REQUIRE(UK(CreateProcessW)); prio_inherit(IDLE_PRIORITY_CLASS, IDLE_PRIORITY_CLASS); }
TEST(pcflag_priority_below_normal_is_inherited) { REQUIRE(UK(CreateProcessW)); prio_inherit(BELOW_NORMAL_PRIORITY_CLASS, BELOW_NORMAL_PRIORITY_CLASS); }
TEST(pcflag_priority_high_parent_gives_normal_child) {
    REQUIRE(UK(CreateProcessW));
    /* only IDLE and BELOW_NORMAL are inherited; other classes produce NORMAL */
    prio_inherit(HIGH_PRIORITY_CLASS, NORMAL_PRIORITY_CLASS);
}
TEST(pcflag_priority_above_normal_parent_gives_normal_child) { REQUIRE(UK(CreateProcessW)); prio_inherit(ABOVE_NORMAL_PRIORITY_CLASS, NORMAL_PRIORITY_CLASS); }

/* ---------- error mode ---------- */
static ULONG hem_of(char *out) { return (ULONG)pc_unum(out, "hem", 0xFFFFFFF0); }
TEST(pcflag_error_mode_inherited_by_default) {
    REQUIRE(UK(CreateProcessW));
    NtQueryInformationProcess_pfn q = UN(NtQueryInformationProcess); ULONG mine = 0xFFFF; char out[4096];
    REQUIRE(q);
    CHECK(q(GetCurrentProcess(), 12, &mine, 4, NULL) >= 0);
    CHECK(pc_cap("pc_ids", "", CREATE_NO_WINDOW, out, sizeof out) == 0);
    EQV(hem_of(out), mine);
}
TEST(pcflag_default_error_mode_not_inherited) {
    REQUIRE(UK(CreateProcessW));
    NtQueryInformationProcess_pfn q = UN(NtQueryInformationProcess); ULONG mine = 0xFFFF; char out[4096];
    REQUIRE(q);
    CHECK(q(GetCurrentProcess(), 12, &mine, 4, NULL) >= 0);
    CHECK(pc_cap("pc_ids", "", CREATE_NO_WINDOW | CREATE_DEFAULT_ERROR_MODE, out, sizeof out) == 0);
    t_note("parent hard-error mode %lu, child with CREATE_DEFAULT_ERROR_MODE %lu", mine, hem_of(out));
    CHECK(hem_of(out) != mine);
}

/* ---------- console attachment ---------- */
static void console_probe(DWORD flags, char *out, size_t cap) {
    pc_opts o; DWORD code; STARTUPINFOEXW si; memset(&si, 0, sizeof si); si.StartupInfo.cb = sizeof(STARTUPINFOW);
    si.StartupInfo.dwFlags = STARTF_USESHOWWINDOW; si.StartupInfo.wShowWindow = SW_HIDE;
    memset(&o, 0, sizeof o); o.child = "pc_console"; o.flags = flags; o.si = &si;
    CHECK(pc_capture(&o, out, cap, NULL, 0, &code, PC_TIMEOUT)); EQV(code, 0);
}
TEST(pcflag_new_console_gets_own_console) {
    REQUIRE(UK(CreateProcessW));
    char out[2048]; console_probe(CREATE_NEW_CONSOLE, out, sizeof out);
    EQV(pc_num(out, "win", -1), 1);
    EQV(pc_num(out, "plist", -1), 1);
    EQV(pc_num(out, "plistself", -1), 1);
}
TEST(pcflag_detached_process_has_no_console) {
    REQUIRE(UK(CreateProcessW));
    char out[2048]; console_probe(DETACHED_PROCESS, out, sizeof out);
    EQV(pc_num(out, "win", -1), 0);
    EQV(pc_num(out, "cp", -1), 0);
    EQV(pc_num(out, "cperr", -1), ERROR_INVALID_HANDLE);
    EQV(pc_num(out, "plist", -1), 0);
}
TEST(pcflag_no_window_has_console_without_window) {
    REQUIRE(UK(CreateProcessW));
    char out[2048]; console_probe(CREATE_NO_WINDOW, out, sizeof out);
    EQV(pc_num(out, "win", -1), 0);
    CHECK(pc_num(out, "plist", 0) >= 1);
    CHECK(pc_num(out, "cp", 0) != 0);
}
TEST(pcflag_new_process_group_creates) {
    REQUIRE(UK(CreateProcessW));
    PROCESS_INFORMATION pi; pc_opts o; memset(&o, 0, sizeof o);
    o.child = "pc_exit77"; o.flags = CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW;
    EQV(pc_run(&o, PC_TIMEOUT), 77);
}
TEST(pcflag_inherit_parent_affinity_flag_accepted) {
    REQUIRE(UK(CreateProcessW));
    pc_opts o; memset(&o, 0, sizeof o);
    o.child = "pc_exit77"; o.flags = 0x00010000; /* INHERIT_PARENT_AFFINITY */
    EQV(pc_run(&o, PC_TIMEOUT), 77);
}
TEST(pcflag_separate_wow_vdm_ignored_for_win32_images) {
    REQUIRE(UK(CreateProcessW));
    pc_opts o; PROCESS_INFORMATION pi; memset(&o, 0, sizeof o);
    o.child = "pc_exit77"; o.flags = CREATE_SEPARATE_WOW_VDM;
    if (pc_launch(&o, &pi)) { WaitForSingleObject(pi.hProcess, PC_TIMEOUT); DWORD c = 0; GetExitCodeProcess(pi.hProcess, &c); EQV(c, 77); CloseHandle(pi.hProcess); CloseHandle(pi.hThread); }
    else t_note("CREATE_SEPARATE_WOW_VDM rejected err %lu", GetLastError());
}
TEST(pcflag_desktop_name_preserved_in_child) {
    REQUIRE(UK(CreateProcessW));
    STARTUPINFOEXW si; pc_opts o; char out[4096]; DWORD code;
    memset(&si, 0, sizeof si); si.StartupInfo.cb = sizeof(STARTUPINFOW); si.StartupInfo.lpDesktop = L"WinSta0\\Default";
    memset(&o, 0, sizeof o); o.child = "pc_startup"; o.si = &si; o.flags = CREATE_NO_WINDOW;
    CHECK(pc_capture(&o, out, sizeof out, NULL, 0, &code, PC_TIMEOUT)); EQV(code, 0);
    CHECK(pc_streq(out, "desktopW", "WinSta0\\Default"));
}

/* ---------- debugging ---------- */
static int debug_run(const char *child, const char *args, DWORD flags, int *bps, int *avs, DWORD *exit_code, int *first_ok) {
    pc_opts o; PROCESS_INFORMATION pi; DEBUG_EVENT ev; int n = 0, done = 0, seen_create = 0;
    *bps = *avs = 0; *exit_code = 0xDEAD0000; *first_ok = 0;
    memset(&o, 0, sizeof o); o.child = child; o.args = args; o.flags = flags;
    if (!pc_launch(&o, &pi)) return 0;
    while (!done && n++ < 2000) {
        DWORD cont = DBG_CONTINUE;
        if (!U(WaitForDebugEvent)(&ev, 8000)) break;
        switch (ev.dwDebugEventCode) {
        case CREATE_PROCESS_DEBUG_EVENT:
            if (!seen_create) { seen_create = 1; *first_ok = (n == 1) && ev.dwProcessId == pi.dwProcessId && ev.dwThreadId == pi.dwThreadId && ev.u.CreateProcessInfo.hProcess != NULL && ev.u.CreateProcessInfo.lpBaseOfImage != NULL && ev.u.CreateProcessInfo.hFile != NULL; }
            if (ev.u.CreateProcessInfo.hFile) CloseHandle(ev.u.CreateProcessInfo.hFile);
            break;
        case LOAD_DLL_DEBUG_EVENT: if (ev.u.LoadDll.hFile) CloseHandle(ev.u.LoadDll.hFile); break;
        case EXCEPTION_DEBUG_EVENT:
            if (ev.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_BREAKPOINT) (*bps)++;
            else if (ev.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_ACCESS_VIOLATION) { (*avs)++; cont = DBG_EXCEPTION_NOT_HANDLED; }
            else cont = DBG_EXCEPTION_NOT_HANDLED;
            break;
        case EXIT_PROCESS_DEBUG_EVENT: *exit_code = ev.u.ExitProcess.dwExitCode; done = 1; break;
        }
        U(ContinueDebugEvent)(ev.dwProcessId, ev.dwThreadId, cont);
    }
    if (!done) { TerminateProcess(pi.hProcess, 1); }
    WaitForSingleObject(pi.hProcess, 5000);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return done;
}
TEST(pcflag_debug_process_event_sequence) {
    REQUIRE(UK(CreateProcessW)); REQUIRE(U(WaitForDebugEvent)); REQUIRE(U(ContinueDebugEvent));
    int bps, avs, first; DWORD code;
    CHECK(debug_run("pc_exit77", "", DEBUG_PROCESS | DEBUG_ONLY_THIS_PROCESS, &bps, &avs, &code, &first));
    CHECK(first); EQV(code, 77); CHECK(bps >= 1); EQV(avs, 0);
}
TEST(pcflag_debug_only_this_process_flag_alone) {
    REQUIRE(UK(CreateProcessW)); REQUIRE(U(WaitForDebugEvent));
    int bps, avs, first; DWORD code;
    CHECK(debug_run("pc_exit77", "", DEBUG_ONLY_THIS_PROCESS, &bps, &avs, &code, &first));
    CHECK(first); EQV(code, 77);
}
TEST(pcflag_debug_child_breakpoint_reaches_debugger) {
    REQUIRE(UK(CreateProcessW)); REQUIRE(U(WaitForDebugEvent));
    int bps, avs, first; DWORD code;
    CHECK(debug_run("pc_dbgint3", "", DEBUG_PROCESS | DEBUG_ONLY_THIS_PROCESS, &bps, &avs, &code, &first));
    EQV(code, 33); CHECK(bps >= 2);
}
TEST(pcflag_debug_child_access_violation_first_and_second_chance) {
    REQUIRE(UK(CreateProcessW)); REQUIRE(U(WaitForDebugEvent));
    int bps, avs, first; DWORD code;
    CHECK(debug_run("pc_crash", "av", DEBUG_PROCESS | DEBUG_ONLY_THIS_PROCESS, &bps, &avs, &code, &first));
    EQV(code, 0xC0000005); EQV(avs, 2);
}
TEST(pcflag_debug_flag_gives_nonexistent_file_error_unchanged) {
    REQUIRE(UK(CreateProcessW));
    pc_opts o; PROCESS_INFORMATION pi; memset(&o, 0, sizeof o);
    o.raw_cmd = L"k32t_missing_zz.exe"; o.flags = DEBUG_PROCESS;
    SET_ERR(0); CHECK(!pc_launch(&o, &pi)); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}

/* ---------- jobs ---------- */
static void job_scenario(const char *kind, int expect_bk_created, int expect_bk_injob, int expect_pl_injob) {
    pc_opts o; char out[2048]; DWORD code;
    memset(&o, 0, sizeof o); o.child = "pc_job"; o.args = kind;
    CHECK(pc_capture(&o, out, sizeof out, NULL, 0, &code, PC_TIMEOUT)); EQV(code, 0);
    if (pc_val(out, "skip")) { t_skip("%s", pc_val(out, "skip")); return; }
    EQV(pc_num(out, "bk_created", -1), expect_bk_created);
    if (!expect_bk_created) EQV(pc_num(out, "bk_err", -1), ERROR_ACCESS_DENIED);
    else EQV(pc_num(out, "bk_injob", -1), expect_bk_injob);
    EQV(pc_num(out, "pl_created", -1), 1);
    EQV(pc_num(out, "pl_injob", -1), expect_pl_injob);
}
TEST(pcflag_job_breakaway_denied_without_limit_flag) { REQUIRE(UK(CreateProcessW)); job_scenario("nobreak", 0, 0, 1); }
TEST(pcflag_job_breakaway_allowed_with_breakaway_ok) { REQUIRE(UK(CreateProcessW)); job_scenario("breakok", 1, 0, 1); }
TEST(pcflag_job_silent_breakaway_leaves_job_without_flag) {
    REQUIRE(UK(CreateProcessW));
    pc_opts o; char out[2048]; DWORD code;
    memset(&o, 0, sizeof o); o.child = "pc_job"; o.args = "silent";
    CHECK(pc_capture(&o, out, sizeof out, NULL, 0, &code, PC_TIMEOUT)); EQV(code, 0);
    if (pc_val(out, "skip")) { t_skip("%s", pc_val(out, "skip")); return; }
    EQV(pc_num(out, "pl_created", -1), 1); EQV(pc_num(out, "pl_injob", -1), 0);
}
TEST(pcflag_job_inherited_by_default_and_breakaway_flag_without_job) {
    REQUIRE(UK(CreateProcessW));
    PROCESS_INFORMATION pi; BOOL inj = FALSE, mine = FALSE; pc_opts o;
    IsProcessInJob(GetCurrentProcess(), NULL, &mine);
    if (mine) { t_skip("test process is in a job"); return; }
    memset(&o, 0, sizeof o); o.child = "pc_exit77"; o.flags = CREATE_SUSPENDED | CREATE_BREAKAWAY_FROM_JOB;
    CHECK(pc_launch(&o, &pi));   /* no job: flag is harmless */
    IsProcessInJob(pi.hProcess, NULL, &inj); CHECK(!inj);
    pc_kill(&pi);
}
