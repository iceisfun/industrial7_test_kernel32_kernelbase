/* Child-process entry points for the process_create area. Names are prefixed pc_. */
#include "pc.h"

static ULONG g_init_hem = 0xFFFFFFFF;   /* hard-error mode as inherited, captured before main() changes it */
static void __attribute__((constructor)) pc_early(void) {
    NtQueryInformationProcess_pfn q = UN(NtQueryInformationProcess);
    ULONG v = 0;
    if (q && q((HANDLE)-1, 12, &v, sizeof v, NULL) >= 0) g_init_hem = v;
}

CHILD(pc_exit77) { return 77; }
CHILD(pc_retcode) { return (int)strtoul(argv[1], NULL, 0); }
CHILD(pc_exitcode) { UK(ExitProcess)((UINT)strtoul(argv[1], NULL, 0)); return 99; }
CHILD(pc_termself) { U(TerminateProcess)(GetCurrentProcess(), (UINT)strtoul(argv[1], NULL, 0)); Sleep(10000); return 99; }
CHILD(pc_sleep) { Sleep(argc > 1 ? (DWORD)atoi(argv[1]) : 10000); return 0; }
CHILD(pc_touch) {
    HANDLE h = CreateFileA(argv[1], GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    return 0;
}

/* ---- crash children ---- */
static int __attribute__((noinline)) rec(int n) { volatile char b[4096]; b[0] = (char)n; return rec(n + 1) + b[0]; }
static LONG WINAPI uef_exit(PEXCEPTION_POINTERS p) { ExitProcess(1234); return EXCEPTION_CONTINUE_SEARCH; }
static LONG WINAPI uef_handler(PEXCEPTION_POINTERS p) { return EXCEPTION_EXECUTE_HANDLER; }
static LONG WINAPI uef_search(PEXCEPTION_POINTERS p) { return EXCEPTION_CONTINUE_SEARCH; }
CHILD(pc_crash) {
    const char *k = argv[1]; volatile int z = 0; volatile int *np = NULL;
    if (!strcmp(k, "av")) { *np = 1; }
    else if (!strcmp(k, "avread")) { return *np; }
    else if (!strcmp(k, "div0")) { return 5 / z; }
    else if (!strcmp(k, "int3")) { __debugbreak(); }
    else if (!strcmp(k, "ud2")) { __builtin_trap(); }
    else if (!strcmp(k, "hlt")) { __asm__ volatile("hlt"); }
    else if (!strcmp(k, "so")) { return rec(0); }
    else if (!strcmp(k, "fastfail")) { __asm__ volatile("movl $7,%%ecx\n\tint $0x29" ::: "ecx"); }
    else if (!strcmp(k, "raise")) { U(RaiseException)((DWORD)strtoul(argv[2], NULL, 0), 0, 0, NULL); }
    else if (!strcmp(k, "uef_handler")) { U(SetUnhandledExceptionFilter)(uef_handler); *np = 1; }
    else if (!strcmp(k, "uef_search")) { U(SetUnhandledExceptionFilter)(uef_search); *np = 1; }
    else if (!strcmp(k, "uef_exit")) { U(SetUnhandledExceptionFilter)(uef_exit); *np = 1; }
    else if (!strcmp(k, "failfast")) { U(RaiseFailFastException)(NULL, NULL, 0); }
    return 55;
}
CHILD(pc_dbgint3) { __debugbreak(); return 33; }

/* ---- thread / exit semantics ---- */
static DWORD WINAPI sleeper(LPVOID p) { Sleep(60000); return 0; }
static DWORD WINAPI exiter(LPVOID p) { Sleep(200); U(ExitProcess)((UINT)(ULONG_PTR)p); return 0; }
CHILD(pc_exit_kills_threads) {
    int i; for (i = 0; i < 4; i++) CloseHandle(CreateThread(NULL, 0, sleeper, NULL, 0, NULL));
    U(ExitProcess)(5); return 99;
}
CHILD(pc_exit_from_thread) {
    CloseHandle(CreateThread(NULL, 0, exiter, (LPVOID)9, 0, NULL));
    Sleep(20000); return 99;
}
static DWORD WINAPI lastth(LPVOID p) { Sleep(300); U(ExitThread)(7); return 0; }
CHILD(pc_main_exitthread) {
    CloseHandle(CreateThread(NULL, 0, lastth, NULL, 0, NULL));
    U(ExitThread)(3); return 99;
}
CHILD(pc_spawnbg) {
    pc_opts o; PROCESS_INFORMATION pi; memset(&o, 0, sizeof o); o.child = "pc_sleep"; o.args = "10000";
    if (!pc_launch(&o, &pi)) { pc_out("err=%lu\n", GetLastError()); return 1; }
    pc_out("gpid=%lu\n", pi.dwProcessId);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return 0;
}


CHILD(pc_modname) {
    WCHAR b[MAX_PATH * 2]; GetModuleFileNameW(NULL, b, MAX_PATH * 2);
    pc_outw("mod", b);
    return 0;
}
/* exit code: bit i set when handle argv[i+1] (hex) is valid in this process */
CHILD(pc_handles_x) {
    int i, m = 0; DWORD fl;
    for (i = 1; i < argc && i < 30; i++) if (U(GetHandleInformation)((HANDLE)(ULONG_PTR)strtoull(argv[i], NULL, 16), &fl)) m |= 1 << (i - 1);
    return m;
}
/* writes argv[2] through inherited file handle argv[1] */
CHILD(pc_filewrite) {
    DWORD w = 0; BOOL ok = WriteFile((HANDLE)(ULONG_PTR)strtoull(argv[1], NULL, 16), argv[2], (DWORD)strlen(argv[2]), &w, NULL);
    return ok ? 0 : 40;
}
CHILD(pc_stdprobe) {
    DWORD w = 0; BOOL ok = WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), "probe\n", 6, &w, NULL);
    return ok ? 10 : (int)(100 + GetLastError() % 100);
}

/* ---- information dumpers ---- */
CHILD(pc_cmdline) {
    LPWSTR w = U(GetCommandLineW)(); LPSTR a = U(GetCommandLineA)();
    WCHAR conv[40000]; int n;
    if (lstrlenW(w) < 3000) { pc_outw("cmdW", w); pc_out("cmdA=%s\n", a); }
    pc_out("lenW=%d\nlenA=%d\n", lstrlenW(w), lstrlenA(a));
    pc_out("stableW=%d\nstableA=%d\n", w == U(GetCommandLineW)(), a == U(GetCommandLineA)());
    n = MultiByteToWideChar(CP_ACP, 0, a, -1, conv, 40000);
    pc_out("consistent=%d\n", n > 0 && !lstrcmpW(conv, w));
    pc_out("argc=%d\n", argc);
    return 0;
}
CHILD(pc_startup) {
    STARTUPINFOW w; STARTUPINFOA a; int i;
    memset(&w, 0xCC, sizeof w); memset(&a, 0xCC, sizeof a);
    U(GetStartupInfoW)(&w); U(GetStartupInfoA)(&a);
    pc_out("cb=%lu\ncbA=%lu\nflags=%lu\nflagsA=%lu\n", w.cb, a.cb, w.dwFlags, a.dwFlags);
    pc_out("x=%lu\ny=%lu\nxs=%lu\nys=%lu\nxc=%lu\nyc=%lu\nfill=%lu\nshow=%u\n", w.dwX, w.dwY, w.dwXSize, w.dwYSize, w.dwXCountChars, w.dwYCountChars, w.dwFillAttribute, w.wShowWindow);
    pc_out("xA=%lu\nyA=%lu\nxsA=%lu\nysA=%lu\nshowA=%u\n", a.dwX, a.dwY, a.dwXSize, a.dwYSize, a.wShowWindow);
    pc_out("cbres2=%u\ncbres2A=%u\nres2null=%d\n", w.cbReserved2, a.cbReserved2, w.lpReserved2 == NULL);
    if (w.lpReserved2 && w.cbReserved2) { char hex[200] = ""; for (i = 0; i < w.cbReserved2 && i < 32; i++) sprintf(hex + strlen(hex), "%02x", w.lpReserved2[i]); pc_out("res2=%s\n", hex); }
    pc_outw("titleW", w.lpTitle); pc_out("titleA=%s\n", a.lpTitle ? a.lpTitle : "(null)");
    pc_out("titlenull=%d\n", w.lpTitle == NULL);
    pc_outw("desktopW", w.lpDesktop); pc_out("desktopnull=%d\n", w.lpDesktop == NULL);
    pc_out("stdin=%p\nstdout=%p\nstderr=%p\n", w.hStdInput, w.hStdOutput, w.hStdError);
    return 0;
}
CHILD(pc_env) {
    WCHAR *e = U(GetEnvironmentStringsW)(), *p; int n = 0; WCHAR v[300]; DWORD r;
    for (p = e; *p; p += lstrlenW(p) + 1) { if (*p == L'=') continue; pc_outw("env", p); n++; }
    pc_out("envcount=%d\n", n);
    U(FreeEnvironmentStringsW)(e);
    SetLastError(0);
    r = U(GetEnvironmentVariableW)(L"K32T_PC_VAR", v, 300);
    pc_out("varlen=%lu\n", r); if (r) pc_outw("var", v);
    r = U(GetEnvironmentVariableW)(L"PATH", v, 300);
    pc_out("pathlen=%lu\n", r);
    return 0;
}
CHILD(pc_cwd) {
    WCHAR b[1000]; DWORD n = U(GetCurrentDirectoryW)(1000, b);
    pc_out("n=%lu\n", n); pc_outw("cwd", b);
    return 0;
}
CHILD(pc_ids) {
    NtQueryInformationProcess_pfn q = UN(NtQueryInformationProcess);
    struct { LONG_PTR st; PVOID peb; ULONG_PTR aff; LONG prio; ULONG_PTR pid; ULONG_PTR ppid; } pbi;
    DWORD sess = 0xFFFF; DWORD_PTR pm = 0, sm = 0;
    memset(&pbi, 0, sizeof pbi);
    if (q) q((HANDLE)-1, 0, &pbi, sizeof pbi, NULL);
    pc_out("pid=%lu\ntid=%lu\nppid=%llu\n", GetCurrentProcessId(), GetCurrentThreadId(), (unsigned long long)pbi.ppid);
    pc_out("prio=0x%lx\nhem=%lu\n", U(GetPriorityClass)(GetCurrentProcess()), g_init_hem);
    U(ProcessIdToSessionId)(GetCurrentProcessId(), &sess); pc_out("session=%lu\n", sess);
    U(GetProcessAffinityMask)(GetCurrentProcess(), &pm, &sm); pc_out("aff=0x%llx\nsysaff=0x%llx\n", (unsigned long long)pm, (unsigned long long)sm);
    pc_out("tprio=%d\n", U(GetThreadPriority)(GetCurrentThread()));
    return 0;
}
CHILD(pc_console) {
    DWORD list[16]; DWORD n; DWORD mode = 0; BOOL ok;
    pc_out("win=%d\n", U(GetConsoleWindow)() != NULL);
    SetLastError(0); pc_out("cp=%u\n", U(GetConsoleCP)()); pc_out("cperr=%lu\n", GetLastError());
    n = U(GetConsoleProcessList)(list, 16); pc_out("plist=%lu\n", n);
    if (n == 1) pc_out("plistself=%d\n", list[0] == GetCurrentProcessId());
    ok = U(GetConsoleMode)(GetStdHandle(STD_INPUT_HANDLE), &mode); pc_out("inmode=%d\n", ok);
    return 0;
}
CHILD(pc_stdio) {
    char buf[1024]; DWORD got, w, n = 0; HANDLE in = GetStdHandle(STD_INPUT_HANDLE), out = GetStdHandle(STD_OUTPUT_HANDLE), err = GetStdHandle(STD_ERROR_HANDLE);
    int i;
    if (argc > 1 && !strcmp(argv[1], "noread")) goto outputs;
    while (n < sizeof buf - 1 && ReadFile(in, buf + n, (DWORD)(sizeof buf - 1 - n), &got, NULL) && got) n += got;
    buf[n] = 0;
    for (i = 0; buf[i]; i++) buf[i] = (char)toupper((unsigned char)buf[i]);
    pc_out("in=%s\n", buf);
outputs:
    pc_out("out=hello-stdout\n");
    WriteFile(err, "ERR=hello-stderr\n", 17, &w, NULL);
    pc_out("sameouterr=%d\n", out == err);
    return 0;
}
/* args: hex handle values, optional ":s" suffix to signal as event */
CHILD(pc_handles) {
    int i;
    for (i = 1; i < argc; i++) {
        ULONG_PTR v = (ULONG_PTR)strtoull(argv[i], NULL, 16); HANDLE h = (HANDLE)v; DWORD fl = 0; BOOL ok;
        ok = U(GetHandleInformation)(h, &fl);
        pc_out("h%llx=%d\n", (unsigned long long)v, ok);
        pc_out("i%llx=%d\n", (unsigned long long)v, ok && (fl & HANDLE_FLAG_INHERIT) != 0);
        if (ok && strstr(argv[i], ":s")) SetEvent(h);
        if (!ok) pc_out("e%llx=%lu\n", (unsigned long long)v, GetLastError());
    }
    return 0;
}
CHILD(pc_waitpipe) { /* block until stdin closes */
    char b[16]; DWORD g; ReadFile(GetStdHandle(STD_INPUT_HANDLE), b, sizeof b, &g, NULL); return 0;
}
CHILD(pc_mit_child) {
    DWORD p[4] = {0}; BOOL ok = U(GetProcessMitigationPolicy)(GetCurrentProcess(), (PROCESS_MITIGATION_POLICY)3, p, 4);
    pc_out("ok=%d\nraise=%lu\n", ok, p[0] & 1);
    ok = U(GetProcessMitigationPolicy)(GetCurrentProcess(), (PROCESS_MITIGATION_POLICY)13, p, 4);
    pc_out("cok=%d\nnochild=%lu\n", ok, p[0] & 1);
    return 0;
}
CHILD(pc_childpol_child) {
    pc_opts o; PROCESS_INFORMATION pi; DWORD p[4] = {0}; BOOL ok;
    ok = U(GetProcessMitigationPolicy)(GetCurrentProcess(), (PROCESS_MITIGATION_POLICY)13, p, 4);
    pc_out("cok=%d\nnochild=%lu\n", ok, p[0] & 1);
    memset(&o, 0, sizeof o); o.child = "pc_exit77";
    ok = pc_launch(&o, &pi);
    pc_out("created=%d\nerr=%lu\n", ok, ok ? 0 : GetLastError());
    if (ok) { TerminateProcess(pi.hProcess, 1); CloseHandle(pi.hProcess); CloseHandle(pi.hThread); }
    return 0;
}

/* ---- relative path search from a changed cwd. args: dir ---- */
CHILD(pc_relcwd) {
    pc_opts o; PROCESS_INFORMATION pi; DWORD code; WCHAR dirw[MAX_PATH];
    pc_widen(dirw, MAX_PATH, argv[1]);
    if (!SetCurrentDirectoryW(dirw)) { pc_out("fail=SetCurrentDirectory %lu\n", GetLastError()); return 1; }
    /* a: relative application name */
    memset(&o, 0, sizeof o); o.app = L"relapp.exe"; o.raw_cmd = L"relapp.exe --child pc_exit77";
    if (!pc_launch(&o, &pi)) pc_out("a=fail%lu\n", GetLastError());
    else { WaitForSingleObject(pi.hProcess, 20000); GetExitCodeProcess(pi.hProcess, &code); pc_out("a=%lu\n", code); CloseHandle(pi.hProcess); CloseHandle(pi.hThread); }
    /* b: relative name only in the command line (parent cwd is searched) */
    memset(&o, 0, sizeof o); o.raw_cmd = L"relapp.exe --child pc_exit77";
    if (!pc_launch(&o, &pi)) pc_out("b=fail%lu\n", GetLastError());
    else { WaitForSingleObject(pi.hProcess, 20000); GetExitCodeProcess(pi.hProcess, &code); pc_out("b=%lu\n", code); CloseHandle(pi.hProcess); CloseHandle(pi.hThread); }
    /* c: lpCurrentDirectory does not influence the search for the image */
    memset(&o, 0, sizeof o); o.app = L"relapp.exe"; o.raw_cmd = L"relapp.exe --child pc_exit77"; o.cwd = L"C:\\Windows";
    if (!pc_launch(&o, &pi)) pc_out("c=fail%lu\n", GetLastError());
    else { WaitForSingleObject(pi.hProcess, 20000); GetExitCodeProcess(pi.hProcess, &code); pc_out("c=%lu\n", code); CloseHandle(pi.hProcess); CloseHandle(pi.hThread); }
    /* d: a name that is not there */
    memset(&o, 0, sizeof o); o.raw_cmd = L"k32t_no_such_program_xyz --child pc_exit77";
    if (!pc_launch(&o, &pi)) pc_out("d=fail%lu\n", GetLastError()); else { pc_out("d=created\n"); TerminateProcess(pi.hProcess, 1); CloseHandle(pi.hProcess); CloseHandle(pi.hThread); }
    return 0;
}

/* ---- priority inheritance: set own class, create suspended child, report its class ---- */
CHILD(pc_prio_parent) {
    DWORD cls = (DWORD)strtoul(argv[1], NULL, 0); PROCESS_INFORMATION pi;
    if (!U(SetPriorityClass)(GetCurrentProcess(), cls)) { pc_out("setfail=%lu\n", GetLastError()); return 1; }
    pc_out("self=0x%lx\n", U(GetPriorityClass)(GetCurrentProcess()));
    if (!pc_suspended("pc_exit77", "", 0, &pi)) { pc_out("createfail=%lu\n", GetLastError()); return 1; }
    pc_out("child=0x%lx\n", U(GetPriorityClass)(pi.hProcess));
    pc_kill(&pi);
    return 0;
}

/* ---- job interplay ---- */
static HANDLE mkjob(DWORD limits) {
    HANDLE j = CreateJobObjectW(NULL, NULL);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION li; memset(&li, 0, sizeof li);
    li.BasicLimitInformation.LimitFlags = limits | JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(j, JobObjectExtendedLimitInformation, &li, sizeof li);
    return j;
}
CHILD(pc_job) {
    const char *k = argv[1]; BOOL inj = FALSE; HANDLE job; PROCESS_INFORMATION pi; pc_opts o;
    IsProcessInJob(GetCurrentProcess(), NULL, &inj);
    if (inj) { pc_out("skip=process already in a job\n"); return 0; }
    if (!strcmp(k, "nobreak")) job = mkjob(0);
    else if (!strcmp(k, "breakok")) job = mkjob(JOB_OBJECT_LIMIT_BREAKAWAY_OK);
    else job = mkjob(JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK);
    if (!AssignProcessToJobObject(job, GetCurrentProcess())) { pc_out("skip=assign failed %lu\n", GetLastError()); return 0; }
    memset(&o, 0, sizeof o); o.child = "pc_exit77"; o.flags = CREATE_SUSPENDED | CREATE_BREAKAWAY_FROM_JOB;
    if (!pc_launch(&o, &pi)) pc_out("bk_created=0\nbk_err=%lu\n", GetLastError());
    else { BOOL in = TRUE; IsProcessInJob(pi.hProcess, job, &in); pc_out("bk_created=1\nbk_injob=%d\n", in); pc_kill(&pi); }
    o.flags = CREATE_SUSPENDED;
    if (!pc_launch(&o, &pi)) pc_out("pl_created=0\npl_err=%lu\n", GetLastError());
    else { BOOL in = FALSE; IsProcessInJob(pi.hProcess, job, &in); pc_out("pl_created=1\npl_injob=%d\n", in); pc_kill(&pi); }
    return 0;
}

/* ---- self-checking scenarios: CHILD pc_scn <name> ---- */
static int g_cf;
#define CK(c) do { if (!(c)) { pc_out("fail=%s (line %d, err %lu)\n", #c, __LINE__, GetLastError()); g_cf++; } } while (0)
#define CKE(c, e) do { DWORD e__ = GetLastError(); if (!(c)) { pc_out("fail=%s (line %d)\n", #c, __LINE__); g_cf++; } else if (e__ != (DWORD)(e)) { pc_out("fail=%s: error %lu want %lu (line %d)\n", #c, e__, (DWORD)(e), __LINE__); g_cf++; } } while (0)
#define NOTE(...) do { char b__[300]; snprintf(b__, sizeof b__, __VA_ARGS__); pc_out("note=%s\n", b__); } while (0)

static void scn_prio_bg(void) {
    HANDLE self = GetCurrentProcess(), real; DWORD orig = U(GetPriorityClass)(self); PROCESS_INFORMATION pi;
    SetLastError(0); CKE(!U(SetPriorityClass)(self, PROCESS_MODE_BACKGROUND_END), 403);
    CK(U(SetPriorityClass)(self, PROCESS_MODE_BACKGROUND_BEGIN));
    NOTE("class while in background mode: 0x%lx (orig 0x%lx)", U(GetPriorityClass)(self), orig);
    SetLastError(0); CKE(!U(SetPriorityClass)(self, PROCESS_MODE_BACKGROUND_BEGIN), 402);
    CK(U(SetPriorityClass)(self, PROCESS_MODE_BACKGROUND_END));
    CK(U(GetPriorityClass)(self) == orig);
    SetLastError(0); CKE(!U(SetPriorityClass)(self, PROCESS_MODE_BACKGROUND_END), 403);
    real = OpenProcess(PROCESS_SET_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    CK(real != NULL);
    if (real) {
        CK(U(SetPriorityClass)(real, PROCESS_MODE_BACKGROUND_BEGIN));
        CK(U(SetPriorityClass)(real, PROCESS_MODE_BACKGROUND_END));
        CloseHandle(real);
    }
    if (pc_suspended("pc_sleep", "5000", 0, &pi)) {
        SetLastError(0); CKE(!U(SetPriorityClass)(pi.hProcess, PROCESS_MODE_BACKGROUND_BEGIN), ERROR_INVALID_PARAMETER);
        pc_kill(&pi);
    }
}
static void scn_prio_set(void) {
    static const DWORD cls[] = { IDLE_PRIORITY_CLASS, BELOW_NORMAL_PRIORITY_CLASS, NORMAL_PRIORITY_CLASS, ABOVE_NORMAL_PRIORITY_CLASS, HIGH_PRIORITY_CLASS };
    HANDLE self = GetCurrentProcess(); int i; DWORD orig = U(GetPriorityClass)(self);
    for (i = 0; i < 5; i++) { CK(U(SetPriorityClass)(self, cls[i])); CK(U(GetPriorityClass)(self) == cls[i]); }
    CK(U(SetPriorityClass)(self, orig));
    { static const DWORD bad[] = { 0, 1, 2, 0x10, 0x200, 0x400 };
      for (i = 0; i < 6; i++) { SetLastError(0); CKE(!U(SetPriorityClass)(self, bad[i]), ERROR_INVALID_PARAMETER); } }
    CK(U(GetPriorityClass)(self) == orig);
}
static void scn_affinity(void) {
    HANDLE self = GetCurrentProcess(); DWORD_PTR pm = 0, sm = 0, p2 = 0, s2 = 0, low;
    CK(U(GetProcessAffinityMask)(self, &pm, &sm));
    CK(pm != 0 && sm != 0 && (pm & ~sm) == 0);
    low = pm & (~pm + 1);
    CK(U(SetProcessAffinityMask)(self, low));
    CK(U(GetProcessAffinityMask)(self, &p2, &s2)); CK(p2 == low); CK(s2 == sm);
    SetLastError(0); CKE(!U(SetProcessAffinityMask)(self, 0), ERROR_INVALID_PARAMETER);
    { DWORD_PTR out = ~sm; if (out) { out = out & (~out + 1); SetLastError(0); CK(!U(SetProcessAffinityMask)(self, out)); NOTE("outside-mask affinity error %lu", GetLastError()); } }
    CK(U(GetProcessAffinityMask)(self, &p2, &s2)); CK(p2 == low);
    CK(U(SetProcessAffinityMask)(self, pm));
    CK(U(GetProcessAffinityMask)(self, &p2, &s2)); CK(p2 == pm);
    SetLastError(0); CKE(!U(GetProcessAffinityMask)(NULL, &p2, &s2), ERROR_INVALID_HANDLE);
    SetLastError(0); CKE(!U(SetProcessAffinityMask)(NULL, 1), ERROR_INVALID_HANDLE);
    { HANDLE ro = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
      if (ro) { SetLastError(0); CKE(!U(SetProcessAffinityMask)(ro, pm), ERROR_ACCESS_DENIED); CloseHandle(ro); } }
}
static void scn_shutdown(void) {
    DWORD lvl = 0xFFFF, fl = 0xFFFF;
    CK(U(GetProcessShutdownParameters)(&lvl, &fl)); CK(lvl == 0x280); CK(fl == 0);
    CK(U(SetProcessShutdownParameters)(0x100, SHUTDOWN_NORETRY));
    CK(U(GetProcessShutdownParameters)(&lvl, &fl)); CK(lvl == 0x100); CK(fl == SHUTDOWN_NORETRY);
    CK(U(SetProcessShutdownParameters)(0x3FF, 0));
    CK(U(GetProcessShutdownParameters)(&lvl, &fl)); CK(lvl == 0x3FF); CK(fl == 0);
    CK(U(SetProcessShutdownParameters)(0x4FF, 0));
    CK(U(GetProcessShutdownParameters)(&lvl, &fl)); NOTE("level 0x4FF -> 0x%lx", lvl);
    if (U(SetProcessShutdownParameters)(0x0FF, 0)) NOTE("level 0xFF accepted"); else NOTE("level 0xFF rejected err %lu", GetLastError());
    if (U(SetProcessShutdownParameters)(0x500, 0)) NOTE("level 0x500 accepted"); else NOTE("level 0x500 rejected err %lu", GetLastError());
    CK(U(SetProcessShutdownParameters)(0x280, 0));
    CK(U(GetProcessShutdownParameters)(&lvl, &fl)); CK(lvl == 0x280);
}
static void scn_mitig_set(void) {
    DWORD p = 1, q = 0; BOOL ok; HANDLE self = GetCurrentProcess();
    /* StrictHandleCheck is one-way: it is turned on and never asserted off again */
    ok = U(SetProcessMitigationPolicy)((PROCESS_MITIGATION_POLICY)3, &p, 4); CK(ok);
    if (ok) {
        CK(U(GetProcessMitigationPolicy)(self, (PROCESS_MITIGATION_POLICY)3, &q, 4)); CK((q & 1) == 1);
        p = 0; SetLastError(0); ok = U(SetProcessMitigationPolicy)((PROCESS_MITIGATION_POLICY)3, &p, 4); NOTE("clearing strict handle check -> %d err %lu", ok, GetLastError());
    }
    p = 1;
    SetLastError(0); CK(!U(SetProcessMitigationPolicy)((PROCESS_MITIGATION_POLICY)0x7fff, &p, 4)); CK(GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0); CK(!U(SetProcessMitigationPolicy)((PROCESS_MITIGATION_POLICY)3, &p, 0)); CK(GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0); CK(!U(SetProcessMitigationPolicy)((PROCESS_MITIGATION_POLICY)3, &p, 64)); CK(GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0); CK(!U(SetProcessMitigationPolicy)((PROCESS_MITIGATION_POLICY)3, &p, 3)); CK(GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0); CK(!U(SetProcessMitigationPolicy)((PROCESS_MITIGATION_POLICY)-1, &p, 4)); CK(GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0); CK(!U(SetProcessMitigationPolicy)((PROCESS_MITIGATION_POLICY)0, &p, 8)); NOTE("DEP policy set err %lu", GetLastError());
}
typedef struct { ULONG Version, ControlMask, StateMask; } PC_PTS;
static void scn_procinfo(void) {
    HANDLE self = GetCurrentProcess(); ULONG mp = 0; PC_PTS t; ULONG leap[2];
    /* memory priority (class 0) */
    CK(U(GetProcessInformation)(self, ProcessMemoryPriority, &mp, 4)); CK(mp == 5);
    CK(U(SetProcessInformation)(self, ProcessMemoryPriority, &(ULONG){3}, 4));
    CK(U(GetProcessInformation)(self, ProcessMemoryPriority, &mp, 4)); CK(mp == 3);
    CK(U(SetProcessInformation)(self, ProcessMemoryPriority, &(ULONG){1}, 4));
    CK(U(GetProcessInformation)(self, ProcessMemoryPriority, &mp, 4)); CK(mp == 1);
    SetLastError(0); CK(!U(SetProcessInformation)(self, ProcessMemoryPriority, &(ULONG){8}, 4)); NOTE("memprio 8 err %lu", GetLastError());
    SetLastError(0); CK(!U(GetProcessInformation)(self, ProcessMemoryPriority, &mp, 3)); NOTE("memprio size 3 err %lu", GetLastError());
    SetLastError(0); CK(!U(GetProcessInformation)(self, (PROCESS_INFORMATION_CLASS)0x7f, &mp, 4)); CK(GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0); CK(!U(SetProcessInformation)(self, (PROCESS_INFORMATION_CLASS)0x7f, &mp, 4)); CK(GetLastError() == ERROR_INVALID_PARAMETER);
    CK(U(SetProcessInformation)(self, ProcessMemoryPriority, &(ULONG){5}, 4));
    SetLastError(0); CKE(!U(GetProcessInformation)(NULL, ProcessMemoryPriority, &mp, 4), ERROR_INVALID_HANDLE);
    /* power throttling (class 4) */
    memset(&t, 0, sizeof t); t.Version = 1;
    CK(U(GetProcessInformation)(self, ProcessPowerThrottling, &t, sizeof t)); NOTE("initial throttling control=%lu state=%lu", t.ControlMask, t.StateMask);
    t.Version = 1; t.ControlMask = 1; t.StateMask = 1;
    CK(U(SetProcessInformation)(self, ProcessPowerThrottling, &t, sizeof t));
    memset(&t, 0, sizeof t); t.Version = 1;
    CK(U(GetProcessInformation)(self, ProcessPowerThrottling, &t, sizeof t)); CK(t.ControlMask & 1); CK(t.StateMask & 1);
    t.Version = 1; t.ControlMask = 1; t.StateMask = 0;
    CK(U(SetProcessInformation)(self, ProcessPowerThrottling, &t, sizeof t));
    memset(&t, 0, sizeof t); t.Version = 1;
    CK(U(GetProcessInformation)(self, ProcessPowerThrottling, &t, sizeof t)); CK(t.ControlMask & 1); CK(!(t.StateMask & 1));
    t.Version = 1; t.ControlMask = 0; t.StateMask = 0;
    CK(U(SetProcessInformation)(self, ProcessPowerThrottling, &t, sizeof t));
    t.Version = 0; t.ControlMask = 1; t.StateMask = 1; SetLastError(0);
    { BOOL r_ = U(SetProcessInformation)(self, ProcessPowerThrottling, &t, sizeof t); NOTE("version 0 -> %d err %lu", r_, GetLastError()); }
    t.Version = 2; SetLastError(0);
    { BOOL r_ = U(SetProcessInformation)(self, ProcessPowerThrottling, &t, sizeof t); NOTE("version 2 -> %d err %lu", r_, GetLastError()); }
    t.Version = 1; t.ControlMask = 0x80; t.StateMask = 0; SetLastError(0);
    { BOOL r_ = U(SetProcessInformation)(self, ProcessPowerThrottling, &t, sizeof t); NOTE("unknown control bit -> %d err %lu", r_, GetLastError()); }
    t.Version = 1; t.ControlMask = 1; SetLastError(0);
    CK(!U(SetProcessInformation)(self, ProcessPowerThrottling, &t, sizeof t - 4)); CK(GetLastError() == ERROR_BAD_LENGTH);
    /* leap second (class 8) */
    memset(leap, 0xAA, sizeof leap);
    if (U(GetProcessInformation)(self, (PROCESS_INFORMATION_CLASS)8, leap, 8)) {
        NOTE("leap second flags=%lu", leap[0]);
        leap[0] = 1; leap[1] = 0; CK(U(SetProcessInformation)(self, (PROCESS_INFORMATION_CLASS)8, leap, 8));
        memset(leap, 0xAA, sizeof leap); CK(U(GetProcessInformation)(self, (PROCESS_INFORMATION_CLASS)8, leap, 8)); CK(leap[0] == 1);
        leap[0] = 0; leap[1] = 0; CK(U(SetProcessInformation)(self, (PROCESS_INFORMATION_CLASS)8, leap, 8));
        leap[0] = 2; SetLastError(0); CK(!U(SetProcessInformation)(self, (PROCESS_INFORMATION_CLASS)8, leap, 8)); CK(GetLastError() == ERROR_INVALID_PARAMETER);
    } else NOTE("leap second class unsupported err %lu", GetLastError());
    { ULONG pl = 0xAA; if (U(GetProcessInformation)(self, (PROCESS_INFORMATION_CLASS)7, &pl, 4)) NOTE("protection level 0x%lx", pl); else NOTE("protection level class err %lu", GetLastError()); }
}
static void scn_errmode(void) {
    UINT old;
    old = U(SetErrorMode)(SEM_NOALIGNMENTFAULTEXCEPT);
    CK(U(SetErrorMode)(old) == SEM_NOALIGNMENTFAULTEXCEPT);
}
typedef struct { DWORD Size; int Type; union { struct { DWORD Id; WORD Group; BYTE LogicalProcessorIndex, CoreIndex, LastLevelCacheIndex, NumaNodeIndex, EfficiencyClass, Flags; DWORD Reserved; DWORD64 AllocationTag; } CpuSet; } u; } PC_CPUSET;
static void scn_cpuset(void) {
    ULONG len = 0, n; BYTE *buf; DWORD id = 0, req = 0xFFFF, ids[4]; ULONG got = 0; BOOL ok;
    SetLastError(0);
    ok = UN(GetSystemCpuSetInformation)(NULL, 0, &len, GetCurrentProcess(), 0);
    CK(!ok); CK(GetLastError() == ERROR_INSUFFICIENT_BUFFER); CK(len >= 32);
    buf = (BYTE *)HeapAlloc(GetProcessHeap(), 0, len + 64);
    CK(UN(GetSystemCpuSetInformation)((PVOID)buf, len + 64, &len, GetCurrentProcess(), 0));
    { ULONG off = 0, cnt = 0; while (off < len) { PC_CPUSET *e = (PC_CPUSET *)(buf + off); CK(e->Size >= 32); CK(e->Type == 0); if (cnt == 0) id = e->u.CpuSet.Id; cnt++; off += e->Size ? e->Size : 32; }
      CK(cnt >= 1); { SYSTEM_INFO si; GetSystemInfo(&si); NOTE("cpu sets %lu, logical processors %lu", cnt, si.dwNumberOfProcessors); } }
    /* default cpu set: none by default */
    CK(UN(GetProcessDefaultCpuSets)(GetCurrentProcess(), NULL, 0, &req)); CK(req == 0);
    ids[0] = id;
    CK(UN(SetProcessDefaultCpuSets)(GetCurrentProcess(), ids, 1));
    req = 0xFFFF; SetLastError(0); ok = UN(GetProcessDefaultCpuSets)(GetCurrentProcess(), NULL, 0, &req);
    CK(!ok); CK(GetLastError() == ERROR_INSUFFICIENT_BUFFER); CK(req == 1);
    memset(ids, 0, sizeof ids); CK(UN(GetProcessDefaultCpuSets)(GetCurrentProcess(), ids, 4, &req)); CK(req == 1); CK(ids[0] == id);
    SetLastError(0); ids[0] = 0x7FFFFFF0; CK(!UN(SetProcessDefaultCpuSets)(GetCurrentProcess(), ids, 1)); NOTE("bad cpu set id error %lu", GetLastError());
    CK(UN(SetProcessDefaultCpuSets)(GetCurrentProcess(), NULL, 0));
    req = 0xFFFF; CK(UN(GetProcessDefaultCpuSets)(GetCurrentProcess(), NULL, 0, &req)); CK(req == 0);
    HeapFree(GetProcessHeap(), 0, buf);
}
static struct { const char *n; void (*f)(void); } g_scn[] = {
    { "cpuset", scn_cpuset },
    { "prio_bg", scn_prio_bg }, { "prio_set", scn_prio_set }, { "affinity", scn_affinity }, { "shutdown", scn_shutdown },
    { "mitig_set", scn_mitig_set }, { "procinfo", scn_procinfo }, { "errmode", scn_errmode },
};
CHILD(pc_scn) {
    size_t i;
    for (i = 0; i < sizeof g_scn / sizeof g_scn[0]; i++) if (!strcmp(g_scn[i].n, argv[1])) { g_scn[i].f(); return g_cf ? 1 : 0; }
    pc_out("fail=unknown scenario %s\n", argv[1]);
    return 2;
}

/* child run under a pseudo console: reports console state through its (console) stdout */
CHILD(pc_pty) {
    DWORD mode = 0, w; HANDLE o = GetStdHandle(STD_OUTPUT_HANDLE), i = GetStdHandle(STD_INPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi; char b[256]; int n;
    BOOL outcon = U(GetConsoleMode)(o, &mode), incon = U(GetConsoleMode)(i, &mode), sb;
    memset(&csbi, 0, sizeof csbi); sb = GetConsoleScreenBufferInfo(o, &csbi);
    n = snprintf(b, sizeof b, "PTYHELLO outcon=%d incon=%d sb=%d cols=%d rows=%d\r\n", outcon, incon, sb, csbi.srWindow.Right - csbi.srWindow.Left + 1, csbi.srWindow.Bottom - csbi.srWindow.Top + 1);
    WriteFile(o, b, n, &w, NULL);
    return 0;
}
