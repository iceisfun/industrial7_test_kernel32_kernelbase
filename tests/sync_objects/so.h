/* Local helpers for the sync_objects area (all static). */
#ifndef SO_H
#define SO_H
#include "t.h"
#include <aclapi.h>
#include <sddl.h>
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
/* single-evaluation versions (the harness macros evaluate their arguments twice) */
#undef EXPECT_EQ
#undef EXPECT_NE
#define EXPECT_EQ(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)a_, (unsigned long long)b_); } while (0)
#define EXPECT_NE(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ != b_, __FILE__, __LINE__, #a " != " #b, "got 0x%llx", (unsigned long long)a_); } while (0)

static inline void so_name(char *out, size_t cap, const char *tag) {
    static volatile LONG ctr;
    snprintf(out, cap, "k32t_%lu_%ld_%s", GetCurrentProcessId(), (long)InterlockedIncrement(&ctr), tag);
}
static inline void so_wide(const char *a, WCHAR *w, int n) { MultiByteToWideChar(CP_ACP, 0, a, -1, w, n); }

/* tiny thread helper */
typedef struct { HANDLE h; } so_thr;
static inline so_thr so_start(LPTHREAD_START_ROUTINE fn, void *arg) {
    so_thr t; DWORD id; t.h = CreateThread(NULL, 0, fn, arg, 0, &id); return t;
}
static inline DWORD so_join(so_thr t, DWORD ms) {
    DWORD code = 0xFFFFFFF0;
    if (!t.h) return code;
    if (WaitForSingleObject(t.h, ms) != WAIT_OBJECT_0) { code = 0xFFFFFFF1; }
    else GetExitCodeThread(t.h, &code);
    CloseHandle(t.h);
    return code;
}
/* wait until *p == v (polling, bounded) */
static inline int so_spin_until(volatile LONG *p, LONG v, DWORD ms) {
    DWORD t0 = GetTickCount();
    while (*p != v) { if (GetTickCount() - t0 > ms) return 0; Sleep(1); }
    return 1;
}
static inline int so_spin_ge(volatile LONG *p, LONG v, DWORD ms) {
    DWORD t0 = GetTickCount();
    while (*p < v) { if (GetTickCount() - t0 > ms) return 0; Sleep(1); }
    return 1;
}
/* build a SECURITY_ATTRIBUTES whose DACL is empty (denies every open) */
typedef struct { SECURITY_ATTRIBUTES sa; SECURITY_DESCRIPTOR sd; ACL acl; } so_denysa;
static inline SECURITY_ATTRIBUTES *so_deny(so_denysa *d, BOOL inherit) {
    memset(d, 0, sizeof *d);
    InitializeAcl(&d->acl, sizeof d->acl, ACL_REVISION);
    InitializeSecurityDescriptor(&d->sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&d->sd, TRUE, &d->acl, FALSE);
    d->sa.nLength = sizeof d->sa; d->sa.lpSecurityDescriptor = &d->sd; d->sa.bInheritHandle = inherit;
    return &d->sa;
}
static inline int so_inheritable(HANDLE h) {
    DWORD f = 0; if (!GetHandleInformation(h, &f)) return -1; return (f & HANDLE_FLAG_INHERIT) != 0;
}
static inline DWORD so_session(void) { DWORD s = 0; ProcessIdToSessionId(GetCurrentProcessId(), &s); return s; }
#endif
