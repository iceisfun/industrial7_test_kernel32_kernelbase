/* Shared helpers for the gap_01 area. */
#ifndef GAP1_H
#define GAP1_H
#include "t.h"
#include <winbase.h>
#include <securitybaseapi.h>
#include <memoryapi.h>
#include <realtimeapiset.h>
#include <fibersapi.h>
#include <appmodel.h>

/* Export lookups must not disturb GetLastError (GetProcAddress sets it on a miss, and U() falls through
 * kernel32 -> kernelbase): save/restore around every resolution. */
static FARPROC g1_res(FARPROC (*f)(const char *), const char *n) { DWORD e = GetLastError(); FARPROC p = f(n); SetLastError(e); return p; }
#undef U
#undef UK
#undef UB
#undef UNT
#undef UN
#define U(name)   ((__typeof__(&name))g1_res(t_proc, #name))
#define UK(name)  ((__typeof__(&name))g1_res(t_proc_k32, #name))
#define UB(name)  ((__typeof__(&name))g1_res(t_proc_kb, #name))
#define UNT(name) ((__typeof__(&name))g1_res(t_proc_nt, #name))
#define UN(name)  ((name##_pfn)g1_res(t_proc, #name))

#ifndef ERROR_ALREADY_FIBER
#define ERROR_ALREADY_FIBER 1280
#endif
#ifndef ERROR_ALREADY_THREAD
#define ERROR_ALREADY_THREAD 1281
#endif
#ifndef E_BOUNDS
#define E_BOUNDS ((HRESULT)0x8000000BL)
#endif
#define G1_LP(c) ((LPSTR)(ULONG_PTR)(unsigned char)(c))
#define G1_LPW(c) ((LPWSTR)(ULONG_PTR)(unsigned short)(c))

/* run fn on a fresh thread, return its exit code (0xFFFFFFFF on failure) */
static DWORD WINAPI g1_thunk(LPVOID p) { return ((DWORD (*)(void))p)(); }
static DWORD g1_run_thread(DWORD (*fn)(void)) {
    HANDLE h = CreateThread(NULL, 0, g1_thunk, (LPVOID)fn, 0, NULL);
    if (!h) return 0xFFFFFFFF;
    DWORD r = 0xFFFFFFFF;
    if (WaitForSingleObject(h, 20000) == WAIT_OBJECT_0) GetExitCodeThread(h, &r);
    CloseHandle(h);
    return r;
}
#endif
