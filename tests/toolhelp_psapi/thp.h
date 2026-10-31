/* shared helpers for toolhelp_psapi tests (all static) */
#ifndef THP_H
#define THP_H
#include "t.h"
#include <tlhelp32.h>
#include <psapi.h>
#include <wchar.h>

UDECL(BOOL, Process32First, (HANDLE, LPPROCESSENTRY32));
UDECL(BOOL, Process32Next, (HANDLE, LPPROCESSENTRY32));
UDECL(BOOL, Module32First, (HANDLE, LPMODULEENTRY32));
UDECL(BOOL, Module32Next, (HANDLE, LPMODULEENTRY32));

#define PSZ_PROC_W 0x238
#define PSZ_THREAD 0x1c
#define PSZ_MOD_W  0x438
#define PSZ_HEAPL  0x20

typedef struct { void *VirtualAddress; ULONG_PTR VirtualAttributes; } thp_wsex;
typedef struct { DWORD cb; DWORD PageFaultCount; SIZE_T Peak, WS, a, b, c, d, Pagefile, PeakPagefile, Private; } thp_pmc_ex;

#define THP_MAXPROC 4096

static inline void thp_chk_pe(HMODULE m, DWORD *size, DWORD *entry_rva) {
    IMAGE_DOS_HEADER *d = (IMAGE_DOS_HEADER *)m;
    IMAGE_NT_HEADERS *n = (IMAGE_NT_HEADERS *)((BYTE *)m + d->e_lfanew);
    *size = n->OptionalHeader.SizeOfImage;
    *entry_rva = n->OptionalHeader.AddressOfEntryPoint;
}

static inline const char *thp_base(const char *p) { const char *s = strrchr(p, '\\'); return s ? s + 1 : p; }
static inline const WCHAR *thp_basew(const WCHAR *p) { const WCHAR *s = wcsrchr(p, L'\\'); return s ? s + 1 : p; }

/* snapshot -> list of process entries. returns count (<=cap) or -1 */
static inline int thp_procs(PROCESSENTRY32W *out, int cap) {
    HANDLE s = U(CreateToolhelp32Snapshot)(TH32CS_SNAPPROCESS, 0);
    if (s == INVALID_HANDLE_VALUE) return -1;
    int n = 0;
    PROCESSENTRY32W e; e.dwSize = sizeof e;
    BOOL ok = U(Process32FirstW)(s, &e);
    while (ok && n < cap) {
        out[n++] = e;
        e.dwSize = sizeof e;
        ok = U(Process32NextW)(s, &e);
    }
    CloseHandle(s);
    return n;
}
static inline int thp_find_pid(DWORD pid, PROCESSENTRY32W *out) {
    static PROCESSENTRY32W l[THP_MAXPROC];
    int n = thp_procs(l, THP_MAXPROC);
    for (int i = 0; i < n; i++) if (l[i].th32ProcessID == pid) { if (out) *out = l[i]; return 1; }
    return 0;
}
/* thread ids of process pid from a THREAD snapshot */
static inline int thp_threads_of(DWORD pid, DWORD *tids, int cap) {
    HANDLE s = U(CreateToolhelp32Snapshot)(TH32CS_SNAPTHREAD, 0);
    if (s == INVALID_HANDLE_VALUE) return -1;
    int n = 0; THREADENTRY32 e; e.dwSize = sizeof e;
    BOOL ok = U(Thread32First)(s, &e);
    while (ok) {
        if (e.th32OwnerProcessID == pid && n < cap) tids[n++] = e.th32ThreadID;
        e.dwSize = sizeof e;
        ok = U(Thread32Next)(s, &e);
    }
    CloseHandle(s);
    return n;
}

/* a long-lived child (killed by the test) */
static inline HANDLE thp_spawn(PROCESS_INFORMATION *pi, DWORD flags) {
    return t_spawn_child("thp_sleeper", "", flags, NULL, NULL, NULL, FALSE, NULL, pi);
}
static inline void thp_reap(PROCESS_INFORMATION *pi) {
    if (pi->hProcess) {
        TerminateProcess(pi->hProcess, 0x5A5A);
        WaitForSingleObject(pi->hProcess, 10000);
        CloseHandle(pi->hProcess);
    }
    if (pi->hThread) CloseHandle(pi->hThread);
    pi->hProcess = pi->hThread = NULL;
}
/* wait until the module named `name` shows up in pid's module snapshot; returns base or NULL */
static inline BYTE *thp_wait_module(DWORD pid, const WCHAR *name, DWORD ms) {
    DWORD t0 = GetTickCount();
    do {
        HANDLE s = U(CreateToolhelp32Snapshot)(TH32CS_SNAPMODULE, pid);
        if (s != INVALID_HANDLE_VALUE) {
            MODULEENTRY32W m; m.dwSize = sizeof m;
            BOOL ok = U(Module32FirstW)(s, &m);
            while (ok) {
                if (!lstrcmpiW(m.szModule, name)) { BYTE *b = m.modBaseAddr; CloseHandle(s); return b; }
                m.dwSize = sizeof m;
                ok = U(Module32NextW)(s, &m);
            }
            CloseHandle(s);
        }
        Sleep(50);
    } while (GetTickCount() - t0 < ms);
    return NULL;
}
static inline DWORD thp_parent_pid(void) {
    typedef struct { LONG_PTR Exit; void *Peb; ULONG_PTR Aff; LONG_PTR Base; ULONG_PTR Pid; ULONG_PTR Ppid; } PBI;
    PBI p; memset(&p, 0, sizeof p);
    NTSTATUS (NTAPI *q)(HANDLE, ULONG, PVOID, ULONG, PULONG) = (void *)t_proc_nt("NtQueryInformationProcess");
    if (!q || q(GetCurrentProcess(), 0, &p, sizeof p, NULL) < 0) return 0;
    return (DWORD)p.Ppid;
}
static inline void thp_w2a(const WCHAR *w, char *a, int cap) { WideCharToMultiByte(CP_ACP, 0, w, -1, a, cap, NULL, NULL); a[cap - 1] = 0; }
static inline int thp_wcsi_tail(const WCHAR *s, const WCHAR *tail) {
    size_t a = wcslen(s), b = wcslen(tail);
    return a >= b && !lstrcmpiW(s + a - b, tail);
}
#endif
