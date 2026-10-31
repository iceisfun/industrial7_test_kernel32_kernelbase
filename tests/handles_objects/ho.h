#ifndef HO_H
#define HO_H
#include "t.h"
#include <stdlib.h>

#ifndef EVENT_QUERY_STATE
#define EVENT_QUERY_STATE 0x0001
#endif
/* t.h's EXPECT_EQ/NE evaluate their arguments twice; calls under test must run exactly once. */
#undef EXPECT_EQ
#undef EXPECT_NE
#define EXPECT_EQ(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)a_, (unsigned long long)b_); } while (0)
#define EXPECT_NE(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ != b_, __FILE__, __LINE__, #a " != " #b, "got 0x%llx", (unsigned long long)a_); } while (0)
#define HO_PSEUDO_PROC ((HANDLE)(LONG_PTR)-1)
#define HO_PSEUDO_THREAD ((HANDLE)(LONG_PTR)-2)

typedef struct {
    ULONG Attributes; ULONG GrantedAccess; ULONG HandleCount; ULONG PointerCount;
    ULONG PagedPoolCharge; ULONG NonPagedPoolCharge; ULONG Reserved[3];
    ULONG NameInfoSize; ULONG TypeInfoSize; ULONG SecurityDescriptorSize; LARGE_INTEGER CreationTime;
} HO_OBJ_BASIC;
typedef LONG (NTAPI *ho_NtQueryObject_pfn)(HANDLE, ULONG, PVOID, ULONG, PULONG);
typedef ULONG (NTAPI *ho_RtlNtStatusToDosError_pfn)(LONG);

/* Granted access of a handle via NtQueryObject(ObjectBasicInformation); returns ~0 on failure. */
static ULONG ho_access(HANDLE h) {
    ho_NtQueryObject_pfn q = (ho_NtQueryObject_pfn)t_proc_nt("NtQueryObject");
    HO_OBJ_BASIC b; memset(&b, 0, sizeof b);
    if (!q || q(h, 0, &b, sizeof b, NULL) < 0) return 0xFFFFFFFFu;
    return b.GrantedAccess;
}
static ULONG ho_hcount(HANDLE h) {
    ho_NtQueryObject_pfn q = (ho_NtQueryObject_pfn)t_proc_nt("NtQueryObject");
    HO_OBJ_BASIC b; memset(&b, 0, sizeof b);
    if (!q || q(h, 0, &b, sizeof b, NULL) < 0) return 0xFFFFFFFFu;
    return b.HandleCount;
}
/* Process handle count (scaffolding path through the pointer). */
static DWORD ho_count(void) {
    DWORD n = 0;
    BOOL (WINAPI *p)(HANDLE, PDWORD) = (BOOL (WINAPI *)(HANDLE, PDWORD))t_proc("GetProcessHandleCount");
    if (!p || !p(GetCurrentProcess(), &n)) return 0xFFFFFFFFu;
    return n;
}
#define NEED_HC() do { if (!U(GetProcessHandleCount)) { t_skip("missing export: GetProcessHandleCount"); return; } \
    if (t_is_wine() && ho_count() == 0) { t_skip("emulator stub for process handle count"); return; } } while (0)
/* Is h a live handle in this process? (GetHandleInformation succeeds) */
static int ho_live(HANDLE h) {
    DWORD f; BOOL (WINAPI *p)(HANDLE, LPDWORD) = (BOOL (WINAPI *)(HANDLE, LPDWORD))t_proc("GetHandleInformation");
    DWORD e = GetLastError(); int r = p && p(h, &f); SetLastError(e); return r;
}
static DWORD ho_flags(HANDLE h) {
    DWORD f = 0xFFFFFFFFu; BOOL (WINAPI *p)(HANDLE, LPDWORD) = (BOOL (WINAPI *)(HANDLE, LPDWORD))t_proc("GetHandleInformation");
    if (p) { if (!p(h, &f)) return 0xFFFFFFFFu; }
    return f;
}
static void ho_wide(const char *s, WCHAR *w, int cap) { MultiByteToWideChar(CP_ACP, 0, s, -1, w, cap); }
static HANDLE ho_mkevent(BOOL inherit, BOOL manual, BOOL initial) {
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, inherit };
    return CreateEventA(&sa, manual, initial, NULL);
}
/* Padding: allocate n unnamed events so later handle values are above anything a fresh child owns. */
#define HO_PADMAX 600
static HANDLE g_ho_pad[HO_PADMAX]; static int g_ho_npad;
static void ho_pad(int n) { for (; g_ho_npad < n && g_ho_npad < HO_PADMAX; g_ho_npad++) g_ho_pad[g_ho_npad] = CreateEventA(NULL, TRUE, FALSE, NULL); }
static void ho_unpad(void) { while (g_ho_npad > 0) CloseHandle(g_ho_pad[--g_ho_npad]); }
static ULONG_PTR ho_parse(const char *s) { return (ULONG_PTR)strtoull(s, NULL, 16); }
static void ho_uniq(char *out, size_t cap, const char *tag) {
    static int ctr; snprintf(out, cap, "k32t_ho_%lu_%s_%d", GetCurrentProcessId(), tag, ++ctr);
}
/* Run a registered child with inheritable handles and hex args; returns exit code (0xDEAD0000/1 on spawn failure/timeout). */
static DWORD ho_run_inherit(const char *name, const char *args, DWORD flags) {
    PROCESS_INFORMATION pi; DWORD code = 0xDEAD0000;
    HANDLE h = t_spawn_child(name, args, flags, NULL, NULL, NULL, TRUE, NULL, &pi);
    if (!h) return 0xDEAD0000;
    if (WaitForSingleObject(h, 20000) != WAIT_OBJECT_0) { TerminateProcess(h, 1); WaitForSingleObject(h, 5000); code = 0xDEAD0001; }
    else GetExitCodeProcess(h, &code);
    CloseHandle(pi.hThread); CloseHandle(h);
    return code;
}
#endif
