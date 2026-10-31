/* Shared helpers for the memory_virtual area (all static). */
#ifndef MVC_H
#define MVC_H
#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x0A000008
#endif
#include "t.h"
#include <setjmp.h>

#define VALLOC   U(VirtualAlloc)
#define VALLOCEX U(VirtualAllocEx)
#define VFREE    U(VirtualFree)
#define VFREEEX  U(VirtualFreeEx)
#define VPROT    U(VirtualProtect)
#define VPROTEX  U(VirtualProtectEx)
#define VQ       U(VirtualQuery)
#define VQEX     U(VirtualQueryEx)

#define PG 0x1000u
#define GR 0x10000u

#undef STATUS_GUARD_PAGE_VIOLATION
#define STATUS_GUARD_PAGE_VIOLATION ((DWORD)0x80000001)

#define ERR1(a) do { DWORD e_ = GetLastError(); \
    t_check(e_ == (DWORD)(a), __FILE__, __LINE__, "GetLastError()==" #a, "got %lu", e_); } while (0)
#define ERR2(a, b) do { DWORD e_ = GetLastError(); \
    t_check(e_ == (DWORD)(a) || e_ == (DWORD)(b), __FILE__, __LINE__, "GetLastError() in {" #a "," #b "}", "got %lu", e_); } while (0)
#define ERR3(a, b, c) do { DWORD e_ = GetLastError(); \
    t_check(e_ == (DWORD)(a) || e_ == (DWORD)(b) || e_ == (DWORD)(c), __FILE__, __LINE__, "GetLastError() in {" #a "," #b "," #c "}", "got %lu", e_); } while (0)

static jmp_buf mv_jb;
static volatile int mv_arm;
static volatile DWORD mv_code;
static volatile ULONG_PTR mv_info0, mv_info1;
static PVOID mv_vh;

static LONG CALLBACK mv_veh(PEXCEPTION_POINTERS ep) {
    DWORD c = ep->ExceptionRecord->ExceptionCode;
    if (!mv_arm) return EXCEPTION_CONTINUE_SEARCH;
    if (c == EXCEPTION_ACCESS_VIOLATION || c == STATUS_GUARD_PAGE_VIOLATION || c == EXCEPTION_IN_PAGE_ERROR ||
        c == EXCEPTION_ILLEGAL_INSTRUCTION || c == EXCEPTION_PRIV_INSTRUCTION) {
        mv_code = c;
        mv_info0 = ep->ExceptionRecord->NumberParameters > 0 ? ep->ExceptionRecord->ExceptionInformation[0] : 0;
        mv_info1 = ep->ExceptionRecord->NumberParameters > 1 ? ep->ExceptionRecord->ExceptionInformation[1] : 0;
        mv_arm = 0;
        longjmp(mv_jb, 1);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
static void mv_veh_install(void) { if (!mv_vh) mv_vh = AddVectoredExceptionHandler(1, mv_veh); }

/* Probes: return 0 if the access succeeded, else the exception code (mv_info0/1 hold the AV details). */
static __attribute__((noinline)) DWORD mv_rd(const void *p, unsigned *out) {
    volatile const unsigned char *vp = (volatile const unsigned char *)p;
    mv_veh_install();
    if (setjmp(mv_jb) == 0) { mv_arm = 1; unsigned v = *vp; mv_arm = 0; if (out) *out = v; return 0; }
    return mv_code;
}
static __attribute__((noinline)) DWORD mv_wr(void *p, unsigned char v) {
    volatile unsigned char *vp = (volatile unsigned char *)p;
    mv_veh_install();
    if (setjmp(mv_jb) == 0) { mv_arm = 1; *vp = v; mv_arm = 0; return 0; }
    return mv_code;
}
static __attribute__((noinline)) DWORD mv_exec(void *p, int *ret) {
    int (*fn)(void) = (int (*)(void))p;
    mv_veh_install();
    if (setjmp(mv_jb) == 0) { mv_arm = 1; int r = fn(); mv_arm = 0; if (ret) *ret = r; return 0; }
    return mv_code;
}
/* mov eax,0x1234 ; ret */
static const unsigned char mv_stub[] = { 0xB8, 0x34, 0x12, 0x00, 0x00, 0xC3 };

static MEMORY_BASIC_INFORMATION mv_q(const void *p) {
    MEMORY_BASIC_INFORMATION m; memset(&m, 0, sizeof m);
    if (!VQ) return m;
    if (VQ(p, &m, sizeof m) != sizeof m) memset(&m, 0, sizeof m);
    return m;
}
static void mv_free(void *p) { if (p && VFREE) VFREE(p, 0, MEM_RELEASE); }
/* Reserve+commit RW region of size sz. */
static void *mv_alloc(SIZE_T sz) { return VALLOC(NULL, sz, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE); }
/* Find a free, granularity-aligned hole by allocating then releasing. */
static void *mv_hole(SIZE_T sz) {
    void *p = VALLOC(NULL, sz, MEM_RESERVE, PAGE_NOACCESS);
    if (p) VFREE(p, 0, MEM_RELEASE);
    return p;
}
static SYSTEM_INFO mv_sysinfo(void) { SYSTEM_INFO s; memset(&s, 0, sizeof s); GetSystemInfo(&s); return s; }
#endif
