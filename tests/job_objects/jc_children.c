#include "jc.h"
/* Child-process bodies used by the job object tests. Exit codes encode results. */

static DWORD argn(int argc, char **argv, int i, DWORD def) { return argc > i ? (DWORD)strtoul(argv[i], NULL, 0) : def; }

CHILD(jo_sleep) { Sleep(argn(argc, argv, 1, 20000)); return 0; }
CHILD(jo_exit)  { ExitProcess(argn(argc, argv, 1, 0)); return 0; }
CHILD(jo_spin) {
    ULONGLONG end = GetTickCount64() + argn(argc, argv, 1, 15000);
    volatile unsigned long long x = 0;
    while (GetTickCount64() < end) { for (int i = 0; i < 100000; i++) x += i; }
    return 0;
}
CHILD(jo_crash) { volatile int *p = NULL; *p = 1; return 0; }
CHILD(jo_alloc) {
    SIZE_T n = (SIZE_T)strtoull(argc > 1 ? argv[1] : "1048576", NULL, 0);
    void *p = VirtualAlloc(NULL, n, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!p) return 0x1000 + GetLastError();
    for (SIZE_T i = 0; i < n; i += 4096) ((volatile char *)p)[i] = 1;
    return 0;
}
/* alloc two blocks: ret = (failing index << 16) | err, or 0 */
CHILD(jo_alloc2) {
    SIZE_T n1 = (SIZE_T)strtoull(argv[1], NULL, 0), n2 = (SIZE_T)strtoull(argv[2], NULL, 0);
    void *p = VirtualAlloc(NULL, n1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!p) return (1 << 16) | GetLastError();
    void *q = VirtualAlloc(NULL, n2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!q) return (2 << 16) | GetLastError();
    return 0;
}
/* spawn n jo_sleep children with creation flags; see tests for the result encoding. */
CHILD(jo_spawn) {
    DWORD flags = argn(argc, argv, 1, 0), n = argn(argc, argv, 2, 1), hold = argn(argc, argv, 3, 0);
    PROCESS_INFORMATION pis[16]; DWORD made = 0; int lastinjob = -1;
    if (n > 16) n = 16;
    for (DWORD i = 0; i < n; i++) {
        STARTUPINFOA si; char cmd[1024];
        memset(&si, 0, sizeof si); si.cb = sizeof si;
        snprintf(cmd, sizeof cmd, "\"%s\" --child jo_sleep 20000", t_self_path());
        if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, flags, NULL, NULL, &si, &pis[made])) {
            DWORD e = GetLastError();
            for (DWORD j = 0; j < made; j++) { TerminateProcess(pis[j].hProcess, 1); }
            return 0x1000 + e;
        }
        /* did it survive? */
        if (WaitForSingleObject(pis[made].hProcess, 1200) == WAIT_OBJECT_0) {
            DWORD code = 0; GetExitCodeProcess(pis[made].hProcess, &code);
            for (DWORD j = 0; j < made; j++) { TerminateProcess(pis[j].hProcess, 1); }
            return 0x2000000 | (i << 16) | (code & 0xFFFF);
        }
        BOOL in = FALSE;
        if (U(IsProcessInJob)(pis[made].hProcess, NULL, &in)) lastinjob = in ? 1 : 0;
        made++;
    }
    if (hold) { Sleep(20000); }
    for (DWORD j = 0; j < made; j++) { TerminateProcess(pis[j].hProcess, 1); }
    return 0x100 | (lastinjob == 1);
}
CHILD(jo_isjob) {
    BOOL in = FALSE;
    if (!U(IsProcessInJob)(GetCurrentProcess(), NULL, &in)) return 0x1000 + GetLastError();
    return in ? 1 : 0;
}
CHILD(jo_inherit) {
    HANDLE h = (HANDLE)(ULONG_PTR)strtoull(argv[1], NULL, 16);
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION a;
    if (!U(QueryInformationJobObject)(h, JobObjectBasicAccountingInformation, &a, sizeof a, NULL)) return 0x1000 + GetLastError();
    return 0;
}
CHILD(jo_prio) {
    DWORD init = GetPriorityClass(GetCurrentProcess());
    if (argc > 1) SetPriorityClass(GetCurrentProcess(), argn(argc, argv, 1, 0));
    DWORD after = GetPriorityClass(GetCurrentProcess());
    return (init << 16) | after;
}
CHILD(jo_aff) {
    DWORD_PTR pm = 0, sm = 0; GetProcessAffinityMask(GetCurrentProcess(), &pm, &sm);
    DWORD before = (DWORD)pm & 0xFFFF;
    SetProcessAffinityMask(GetCurrentProcess(), sm);
    GetProcessAffinityMask(GetCurrentProcess(), &pm, &sm);
    return before | (((DWORD)pm & 0xFFFF) << 16);
}
CHILD(jo_atom) {
    HANDLE ready = OpenEventA(EVENT_ALL_ACCESS, FALSE, argv[2]), go = OpenEventA(EVENT_ALL_ACCESS, FALSE, argv[3]);
    ATOM a = GlobalAddAtomA(argv[1]);
    if (ready) SetEvent(ready);
    if (go) WaitForSingleObject(go, 10000);
    if (a) GlobalDeleteAtom(a);
    return a ? 0 : 0x1000 + GetLastError();
}
CHILD(jo_desktop) {
    typedef HDESK (WINAPI *cd_t)(LPCSTR, LPCSTR, void *, DWORD, DWORD, void *);
    typedef BOOL (WINAPI *cl_t)(HDESK);
    HMODULE u = LoadLibraryA("user32.dll");
    cd_t cd = u ? (cd_t)GetProcAddress(u, "CreateDesktopA") : NULL; cl_t cl = u ? (cl_t)GetProcAddress(u, "CloseDesktop") : NULL;
    char nm[64]; if (!cd) return 0x3000;
    snprintf(nm, sizeof nm, "k32t_dsk_%lu", GetCurrentProcessId());
    HDESK d = cd(nm, NULL, NULL, 0, 0x0001 /*DESKTOP_READOBJECTS*/, NULL);
    if (!d) return 0x1000 + GetLastError();
    cl(d);
    return 0;
}
/* Create nested job B (named argv[1]) with optional per-process memory limit argv[2], assign self, try alloc argv[3], hold argv[4] ms. */
CHILD(jo_nest) {
    WCHAR nm[128]; const char *a = argv[1]; size_t i = 0;
    for (; a[i]; i++) nm[i] = a[i]; nm[i] = 0;
    HANDLE b = U(CreateJobObjectW)(NULL, nm);
    if (!b) return 0x1000 + GetLastError();
    SIZE_T lim = (SIZE_T)strtoull(argv[2], NULL, 0);
    if (lim) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION e; memset(&e, 0, sizeof e);
        e.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY; e.ProcessMemoryLimit = lim;
        if (!U(SetInformationJobObject)(b, JobObjectExtendedLimitInformation, &e, sizeof e)) return 0x4000 + GetLastError();
    }
    if (!U(AssignProcessToJobObject)(b, GetCurrentProcess())) return 0x5000 + GetLastError();
    DWORD rc = 0;
    SIZE_T asz = (SIZE_T)strtoull(argv[3], NULL, 0);
    if (asz) {
        void *p = VirtualAlloc(NULL, asz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        rc = p ? 0 : 0x100000 | GetLastError();
        if (p) VirtualFree(p, 0, MEM_RELEASE);
    }
    DWORD hold = argn(argc, argv, 4, 0);
    if (hold) Sleep(hold);
    return rc;
}
/* in a job already: create unnamed jobs, assign self to two in a row; ret 0 or (step<<16|err) */
CHILD(jo_nest2) {
    HANDLE b = U(CreateJobObjectW)(NULL, NULL), c = U(CreateJobObjectW)(NULL, NULL);
    if (!b || !c) return 0x1000 + GetLastError();
    if (!U(AssignProcessToJobObject)(b, GetCurrentProcess())) return (1 << 16) | GetLastError();
    if (!U(AssignProcessToJobObject)(c, GetCurrentProcess())) return (2 << 16) | GetLastError();
    DWORD ids[8]; DWORD asg = 0;
    int n = jc_pids(c, ids, 8, &asg);
    if (n != 1 || ids[0] != GetCurrentProcessId()) return (3 << 16);
    return 0;
}
/* open job named argv[1], try to remove nothing: just query its pid list; return count */
CHILD(jo_countjob) {
    WCHAR nm[128]; const char *a = argv[1]; size_t i = 0;
    for (; a[i]; i++) nm[i] = a[i]; nm[i] = 0;
    HANDLE h = U(OpenJobObjectW)(JOB_OBJECT_QUERY, FALSE, nm);
    if (!h) return 0x1000 + GetLastError();
    DWORD ids[16]; DWORD asg = 0; int n = jc_pids(h, ids, 16, &asg);
    return (DWORD)n < 0 ? 0x2000 : (asg & 0xFF);
}

/* allocate argv[1] bytes then hold argv[2] ms; exit code 0x1000+err if the allocation fails */
CHILD(jo_allochold) {
    SIZE_T n = (SIZE_T)strtoull(argv[1], NULL, 0);
    void *p = VirtualAlloc(NULL, n, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!p) return 0x1000 + GetLastError();
    for (SIZE_T i = 0; i < n; i += 4096) ((volatile char *)p)[i] = 1;
    Sleep(argn(argc, argv, 2, 20000));
    return 0;
}
/* reserve argv[1] then commit argv[2] of it: 0 ok, (1<<16|err) reserve failed, (2<<16|err) commit failed */
CHILD(jo_reserve_commit) {
    SIZE_T r = (SIZE_T)strtoull(argv[1], NULL, 0), c = (SIZE_T)strtoull(argv[2], NULL, 0);
    void *p = VirtualAlloc(NULL, r, MEM_RESERVE, PAGE_READWRITE);
    if (!p) return (1 << 16) | GetLastError();
    if (!VirtualAlloc(p, c, MEM_COMMIT, PAGE_READWRITE)) return (2 << 16) | GetLastError();
    return 0;
}
/* SPI_SETBEEP with the current value (benign) */
CHILD(jo_spi) {
    typedef BOOL (WINAPI *spi_t)(UINT, UINT, PVOID, UINT);
    HMODULE u = LoadLibraryA("user32.dll");
    spi_t spi = u ? (spi_t)GetProcAddress(u, "SystemParametersInfoA") : NULL;
    BOOL b = FALSE;
    if (!spi) return 0x3000;
    if (!spi(0x0001 /*SPI_GETBEEP*/, 0, &b, 0)) return 0x2000 + GetLastError();
    SetLastError(0);
    if (!spi(0x0002 /*SPI_SETBEEP*/, b, NULL, 0)) return 0x1000 + GetLastError();
    return 0;
}
/* CDS_TEST of the current display mode: returns 100 + DISP_CHANGE_* (or 0x1000 if unavailable) */
CHILD(jo_display) {
    typedef BOOL (WINAPI *es_t)(LPCSTR, DWORD, DEVMODEA *);
    typedef LONG (WINAPI *cd_t)(LPCSTR, DEVMODEA *, HWND, DWORD, LPVOID);
    HMODULE u = LoadLibraryA("user32.dll");
    es_t es = u ? (es_t)GetProcAddress(u, "EnumDisplaySettingsA") : NULL;
    cd_t cd = u ? (cd_t)GetProcAddress(u, "ChangeDisplaySettingsExA") : NULL;
    DEVMODEA dm; memset(&dm, 0, sizeof dm); dm.dmSize = sizeof dm;
    if (!es || !cd || !es(NULL, (DWORD)-1, &dm)) return 0x1000;
    return 100 + cd(NULL, &dm, NULL, 0x2 /*CDS_TEST*/, NULL);
}
/* try to read the foreign window handle argv[1] metadata: GetWindowThreadProcessId on it; returns 0 if call works */
CHILD(jo_hwnd) {
    typedef DWORD (WINAPI *g_t)(HWND, LPDWORD);
    HMODULE u = LoadLibraryA("user32.dll");
    g_t g = u ? (g_t)GetProcAddress(u, "GetWindowThreadProcessId") : NULL;
    DWORD pid = 0; HWND h = (HWND)(ULONG_PTR)strtoull(argv[1], NULL, 16);
    if (!g) return 0x3000;
    SetLastError(0);
    DWORD tid = g(h, &pid);
    return tid ? 0 : 0x1000 + GetLastError();
}
