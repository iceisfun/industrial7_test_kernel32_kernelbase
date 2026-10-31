/* Shared helpers for the threads / TLS / FLS / fibers area. All static. */
#ifndef TF_H
#define TF_H
#include "t.h"
#include <stdint.h>

#ifndef STILL_ACTIVE
#define STILL_ACTIVE 259
#endif
#ifndef FIBER_FLAG_FLOAT_SWITCH
#define FIBER_FLAG_FLOAT_SWITCH 1
#endif
#ifndef TLS_OUT_OF_INDEXES
#define TLS_OUT_OF_INDEXES 0xFFFFFFFF
#endif
#define TF_FLS_OUT 0xFFFFFFFF
#define TF_TLS_MAX 1088
#define TF_STACK_RES 0x00010000

/* Win10 functions that older mingw headers lack. */
UDECL(HRESULT, GetThreadDescription, (HANDLE, PWSTR *));
UDECL(HRESULT, SetThreadDescription, (HANDLE, PCWSTR));
UDECL(BOOL, GetThreadSelectedCpuSets, (HANDLE, PULONG, ULONG, PULONG));
UDECL(BOOL, SetThreadSelectedCpuSets, (HANDLE, const ULONG *, ULONG));
UDECL(BOOL, GetSystemCpuSetInformation, (void *, ULONG, PULONG, HANDLE, ULONG));
UDECL(BOOL, InitializeContext, (PVOID, DWORD, PCONTEXT *, PDWORD));
UDECL(BOOL, QueryThreadCycleTime, (HANDLE, PULONG64));
typedef struct { ULONG Version, ControlMask, StateMask; } tf_power_throttle;

typedef struct { ULONG MemoryPriority; } tf_mem_prio;

static inline void tf_sleep(DWORD ms) { Sleep(ms); }
static inline HANDLE tf_ev(BOOL manual, BOOL init) { return CreateEventA(NULL, manual, init, NULL); }

static DWORD WINAPI tf_th_ret(LPVOID p) { return (DWORD)(ULONG_PTR)p; }
static DWORD WINAPI tf_th_waitev(LPVOID p) { WaitForSingleObject((HANDLE)p, 15000); return 0x77; }
static DWORD WINAPI tf_th_waitev_alert(LPVOID p) {
    DWORD r;
    do { r = WaitForSingleObjectEx((HANDLE)p, 15000, TRUE); } while (r == WAIT_IO_COMPLETION);
    return 0x78;
}
static volatile ULONG_PTR tf_last_param;
static DWORD WINAPI tf_th_store(LPVOID p) { tf_last_param = (ULONG_PTR)p; return 0; }

static inline HANDLE tf_start(LPTHREAD_START_ROUTINE f, LPVOID p, DWORD fl) {
    DWORD tid; return CreateThread(NULL, 0, f, p, fl, &tid);
}
/* Wait up to 10s for thread, return exit code (0xDEAD0001 on timeout, thread killed), close handle. */
static DWORD tf_join(HANDLE h) {
    DWORD code = 0xDEAD0001;
    if (!h) return 0xDEAD0000;
    if (WaitForSingleObject(h, 10000) == WAIT_OBJECT_0) GetExitCodeThread(h, &code);
    else TerminateThread(h, 0xDEAD0001);
    CloseHandle(h);
    return code;
}
/* Run fn on a fresh thread, return its exit code. */
static DWORD tf_run(LPTHREAD_START_ROUTINE f, LPVOID p) { return tf_join(tf_start(f, p, 0)); }

/* Spinner implemented in assembly: spins until EAX != 0, exits with EAX. */
extern DWORD WINAPI tf_regloop(LPVOID);
extern char tf_regloop_end[];

static inline int tf_in_module(HMODULE m, ULONG_PTR a) {
    if (!m) return 0;
    PIMAGE_DOS_HEADER d = (PIMAGE_DOS_HEADER)m;
    PIMAGE_NT_HEADERS n = (PIMAGE_NT_HEADERS)((char *)m + d->e_lfanew);
    return a >= (ULONG_PTR)m && a < (ULONG_PTR)m + n->OptionalHeader.SizeOfImage;
}
#define EQ(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)a_, (unsigned long long)b_); } while (0)
#define NE(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ != b_, __FILE__, __LINE__, #a " != " #b, "got 0x%llx", (unsigned long long)a_); } while (0)
#endif
