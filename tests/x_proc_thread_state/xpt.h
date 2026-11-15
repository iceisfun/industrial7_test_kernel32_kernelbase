/* Shared helpers for the x_proc_thread_state area: handle semantics of exited (zombie) threads
 * and own child processes, and access-mask enforcement with single-right handles.
 * Only threads of this process and our own child processes are ever targeted. All static. */
#ifndef XPT_H
#define XPT_H
#include "t.h"
#include <stdlib.h>

#ifndef STILL_ACTIVE
#define STILL_ACTIVE 259
#endif
#define XPT_THREAD_ALERT 0x0004
#ifndef THREAD_POWER_THROTTLING_CURRENT_VERSION
#define THREAD_POWER_THROTTLING_CURRENT_VERSION 1
#endif

/* The calls under test must run exactly once: single-evaluation comparisons. */
#define EQ(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)a_, (unsigned long long)b_); } while (0)
#define NE(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ != b_, __FILE__, __LINE__, #a " != " #b, "got 0x%llx", (unsigned long long)a_); } while (0)
/* Call that must fail with a given last error. */
#define FAILS(call, err) do { SetLastError(0xDEADBEEF); ULONG_PTR r_ = (ULONG_PTR)(call); DWORD e_ = GetLastError(); \
    t_check(r_ == 0, __FILE__, __LINE__, #call " fails", "returned 0x%llx", (unsigned long long)r_); \
    t_check(e_ == (DWORD)(err), __FILE__, __LINE__, #call " last error " #err, "got %lu want %lu", e_, (DWORD)(err)); } while (0)
#define SUCCEEDS(call) do { SetLastError(0xDEADBEEF); ULONG_PTR r_ = (ULONG_PTR)(call); DWORD e_ = GetLastError(); \
    t_check(r_ != 0, __FILE__, __LINE__, #call " succeeds", "returned 0 last error %lu", e_); } while (0)

UDECL(HRESULT, GetThreadDescription, (HANDLE, PWSTR *));
UDECL(HRESULT, SetThreadDescription, (HANDLE, PCWSTR));
UDECL(BOOL, IsWow64Process2, (HANDLE, USHORT *, USHORT *));
typedef LONG (NTAPI *xpt_NtQueryObject_pfn)(HANDLE, ULONG, PVOID, ULONG, PULONG);
typedef struct {
    ULONG Attributes; ULONG GrantedAccess; ULONG HandleCount; ULONG PointerCount;
    ULONG PagedPoolCharge; ULONG NonPagedPoolCharge; ULONG Reserved[3];
    ULONG NameInfoSize; ULONG TypeInfoSize; ULONG SecurityDescriptorSize; LARGE_INTEGER CreationTime;
} XPT_OBJ_BASIC;

/* Granted access of a handle (NtQueryObject ObjectBasicInformation), ~0 on failure. */
static ULONG xpt_access(HANDLE h) {
    xpt_NtQueryObject_pfn q = (xpt_NtQueryObject_pfn)t_proc_nt("NtQueryObject");
    XPT_OBJ_BASIC b; memset(&b, 0, sizeof b);
    if (!q || q(h, 0, &b, sizeof b, NULL) < 0) return 0xFFFFFFFFu;
    return b.GrantedAccess;
}
/* Number of open handles to the object behind h (same query), ~0 on failure. */
static ULONG xpt_hcount(HANDLE h) {
    xpt_NtQueryObject_pfn q = (xpt_NtQueryObject_pfn)t_proc_nt("NtQueryObject");
    XPT_OBJ_BASIC b; memset(&b, 0, sizeof b);
    if (!q || q(h, 0, &b, sizeof b, NULL) < 0) return 0xFFFFFFFFu;
    return b.HandleCount;
}

/* HRESULT_FROM_NT(STATUS_ACCESS_DENIED), returned by Get/SetThreadDescription on a denied handle. */
#define XPT_HR_DENIED ((HRESULT)0xD0000022L)
/* Their success value is HRESULT_FROM_NT(STATUS_SUCCESS), not S_OK. */
#define XPT_HR_NT_OK ((HRESULT)0x10000000L)

/* ---- result capture: ok flag and the last error (or HRESULT) of the call under test ---- */
static DWORD xpt_err;
static int xpt_b(int ok) { xpt_err = ok ? 0 : GetLastError(); return ok; }
#define XB(call) (SetLastError(0xDEADBEEF), xpt_b((call) != 0))

/* ---- single access rights ---- */
typedef struct { DWORD r; const char *n; } xpt_right;
static const xpt_right xpt_trights[] = {
    { SYNCHRONIZE, "SYNCHRONIZE" }, { THREAD_TERMINATE, "TERMINATE" }, { THREAD_SUSPEND_RESUME, "SUSPEND_RESUME" },
    { THREAD_GET_CONTEXT, "GET_CONTEXT" }, { THREAD_SET_CONTEXT, "SET_CONTEXT" }, { THREAD_SET_INFORMATION, "SET_INFORMATION" },
    { THREAD_QUERY_INFORMATION, "QUERY_INFORMATION" }, { THREAD_SET_LIMITED_INFORMATION, "SET_LIMITED_INFORMATION" },
    { THREAD_QUERY_LIMITED_INFORMATION, "QUERY_LIMITED_INFORMATION" },
};
#define XPT_NTR ((int)(sizeof xpt_trights / sizeof *xpt_trights))
static const xpt_right xpt_prights[] = {
    { SYNCHRONIZE, "SYNCHRONIZE" }, { PROCESS_TERMINATE, "TERMINATE" }, { PROCESS_CREATE_THREAD, "CREATE_THREAD" },
    { PROCESS_VM_OPERATION, "VM_OPERATION" }, { PROCESS_VM_READ, "VM_READ" }, { PROCESS_VM_WRITE, "VM_WRITE" },
    { PROCESS_DUP_HANDLE, "DUP_HANDLE" }, { PROCESS_SET_QUOTA, "SET_QUOTA" }, { PROCESS_SET_INFORMATION, "SET_INFORMATION" },
    { PROCESS_QUERY_INFORMATION, "QUERY_INFORMATION" }, { PROCESS_SUSPEND_RESUME, "SUSPEND_RESUME" },
    { PROCESS_QUERY_LIMITED_INFORMATION, "QUERY_LIMITED_INFORMATION" }, { PROCESS_SET_LIMITED_INFORMATION, "SET_LIMITED_INFORMATION" },
};
#define XPT_NPR ((int)(sizeof xpt_prights / sizeof *xpt_prights))

#define TQ  (THREAD_QUERY_INFORMATION | THREAD_QUERY_LIMITED_INFORMATION)
#define TS  (THREAD_SET_INFORMATION | THREAD_SET_LIMITED_INFORMATION)
#define PQ  (PROCESS_QUERY_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION)
#define PS  (PROCESS_SET_INFORMATION | PROCESS_SET_LIMITED_INFORMATION)

/* ---- own worker thread parked in a non-alertable bounded wait ---- */
typedef struct { HANDLE h, go, ready; DWORD tid; } xpt_tw;
static DWORD WINAPI xpt_park(LPVOID p) {
    xpt_tw *w = p;
    SetEvent(w->ready);
    WaitForSingleObject(w->go, 20000);
    return 0x51;
}
static int xpt_tw_start(xpt_tw *w) {
    memset(w, 0, sizeof *w);
    w->go = CreateEventA(NULL, TRUE, FALSE, NULL);
    w->ready = CreateEventA(NULL, TRUE, FALSE, NULL);
    w->h = CreateThread(NULL, 0, xpt_park, w, 0, &w->tid);
    if (!w->h) return 0;
    if (WaitForSingleObject(w->ready, 10000) != WAIT_OBJECT_0) return 0;
    Sleep(20);   /* let it enter the wait */
    return 1;
}
static void xpt_tw_stop(xpt_tw *w) {
    if (w->h) {
        for (int i = 0; i < 8; i++) { DWORD c = ResumeThread(w->h); if (c == (DWORD)-1 || c <= 1) break; }
        SetEvent(w->go);
        if (WaitForSingleObject(w->h, 10000) != WAIT_OBJECT_0) { TerminateThread(w->h, 0xDEAD); WaitForSingleObject(w->h, 5000); }
        CloseHandle(w->h);
    }
    if (w->go) CloseHandle(w->go);
    if (w->ready) CloseHandle(w->ready);
    memset(w, 0, sizeof *w);
}

/* ---- exited thread of this process (full-access handle kept open) ---- */
static DWORD WINAPI xpt_ret(LPVOID p) { return (DWORD)(ULONG_PTR)p; }
static HANDLE xpt_dead_thread(DWORD code, DWORD *tid_out) {
    DWORD tid = 0;
    HANDLE h = CreateThread(NULL, 0, xpt_ret, (LPVOID)(ULONG_PTR)code, 0, &tid);
    if (!h) return NULL;
    if (WaitForSingleObject(h, 10000) != WAIT_OBJECT_0) { TerminateThread(h, 0xDEAD); WaitForSingleObject(h, 5000); CloseHandle(h); return NULL; }
    if (tid_out) *tid_out = tid;
    return h;
}
/* Same, but with priority and description set while it was suspended and before it ran. */
static HANDLE xpt_dead_thread_prepared(DWORD code, int prio, DWORD *tid_out) {
    DWORD tid = 0;
    HANDLE h = CreateThread(NULL, 0, xpt_ret, (LPVOID)(ULONG_PTR)code, CREATE_SUSPENDED, &tid);
    if (!h) return NULL;
    SetThreadPriority(h, prio);
    SetThreadDescription_pfn sd = UN(SetThreadDescription);
    if (sd) sd(h, L"xpt-dead");
    ResumeThread(h);
    if (WaitForSingleObject(h, 10000) != WAIT_OBJECT_0) { TerminateThread(h, 0xDEAD); WaitForSingleObject(h, 5000); CloseHandle(h); return NULL; }
    if (tid_out) *tid_out = tid;
    return h;
}

/* ---- own child processes ---- */
/* Child that signals the inherited event (hex handle in argv[1]) and sleeps (bounded). */
static int xpt_sleeper_main(int argc, char **argv) {
    HANDLE ev = (HANDLE)(ULONG_PTR)strtoull(argc > 1 ? argv[1] : "0", NULL, 16);
    if (ev) SetEvent(ev);
    Sleep(25000);
    return 3;
}
/* Started sleeper child; full-access process handle, NULL on failure. pi->hThread kept open. */
static HANDLE xpt_spawn_sleeper(PROCESS_INFORMATION *pi) {
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE ev = CreateEventA(&sa, TRUE, FALSE, NULL);
    char arg[32]; snprintf(arg, sizeof arg, "%llx", (unsigned long long)(ULONG_PTR)ev);
    memset(pi, 0, sizeof *pi);
    HANDLE hp = t_spawn_child("xpt_sleeper", arg, 0, NULL, NULL, NULL, TRUE, NULL, pi);
    if (!hp) { CloseHandle(ev); return NULL; }
    if (WaitForSingleObject(ev, 15000) != WAIT_OBJECT_0) {
        TerminateProcess(hp, 1); WaitForSingleObject(hp, 5000); CloseHandle(pi->hThread); CloseHandle(hp); CloseHandle(ev); return NULL;
    }
    CloseHandle(ev);
    return hp;
}
static void xpt_kill(PROCESS_INFORMATION *pi) {
    if (!pi->hProcess) return;
    TerminateProcess(pi->hProcess, 9);
    WaitForSingleObject(pi->hProcess, 5000);
    if (pi->hThread) CloseHandle(pi->hThread);
    CloseHandle(pi->hProcess);
    memset(pi, 0, sizeof *pi);
}
#define XPT_CHILD_EXIT 0x2Au
/* Child that has already exited with XPT_CHILD_EXIT; both handles kept open. */
static HANDLE xpt_spawn_dead(PROCESS_INFORMATION *pi) {
    memset(pi, 0, sizeof *pi);
    HANDLE hp = t_spawn_child("xpt_quick", NULL, 0, NULL, NULL, NULL, FALSE, NULL, pi);
    if (!hp) return NULL;
    if (WaitForSingleObject(hp, 15000) != WAIT_OBJECT_0) { xpt_kill(pi); return NULL; }
    return hp;
}

/* Poll until OpenThread/OpenProcess by id fails (the id is released once the object is gone;
 * other components can hold a reference for a short time). Returns the last error, 0 if it never failed. */
static DWORD xpt_poll_open_gone(int is_thread, DWORD id) {
    for (int i = 0; i < 200; i++) {
        SetLastError(0);
        HANDLE h = is_thread ? U(OpenThread)(THREAD_QUERY_LIMITED_INFORMATION, FALSE, id)
                             : U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, id);
        DWORD e = GetLastError();
        if (!h) return e;
        CloseHandle(h);
        Sleep(10);
    }
    return 0;
}

/* Name of a right for messages. */
static const char *xpt_tname(DWORD r) { for (int i = 0; i < XPT_NTR; i++) if (xpt_trights[i].r == r) return xpt_trights[i].n; return "?"; }
static const char *xpt_pname(DWORD r) { for (int i = 0; i < XPT_NPR; i++) if (xpt_prights[i].r == r) return xpt_prights[i].n; return "?"; }

#endif
