/* RtlCaptureContext / RtlCaptureStackBackTrace / function tables / virtual unwind / RtlUnwind(Ex) / RtlRestoreContext. */
#include "eed.h"
#include "eed_asm.h"

__asm__(
    ".text\n"
    ".globl eed_cap_regs\need_cap_regs:\n"
    "  pushq %rbx\n  pushq %rsi\n  pushq %rdi\n  pushq %r12\n  pushq %r13\n  pushq %r14\n  pushq %r15\n  pushq %rbp\n"
    "  subq $40, %rsp\n"
    "  movq $0x1111, %rbx\n  movq $0x2222, %rsi\n  movq $0x3333, %rdi\n  movq $0x4444, %r12\n  movq $0x5555, %r13\n"
    "  movq $0x6666, %r14\n  movq $0x7777, %r15\n  movq $0x8888, %rbp\n"
    "  call *%r8\n"
    "  addq $40, %rsp\n  popq %rbp\n  popq %r15\n  popq %r14\n  popq %r13\n  popq %r12\n  popq %rdi\n  popq %rsi\n  popq %rbx\n  ret\n"
);
extern void eed_cap_regs(CONTEXT *c, void *unused, void (WINAPI *cap)(PCONTEXT));

typedef void (WINAPI *RtlCaptureContext_t)(PCONTEXT);
typedef USHORT (WINAPI *RtlCaptureStackBackTrace_t)(ULONG, ULONG, PVOID *, PULONG);
typedef PRUNTIME_FUNCTION (WINAPI *RtlLookupFunctionEntry_t)(DWORD64, PDWORD64, PUNWIND_HISTORY_TABLE);
typedef PEXCEPTION_ROUTINE (WINAPI *RtlVirtualUnwind_t)(DWORD, DWORD64, DWORD64, PRUNTIME_FUNCTION, PCONTEXT, PVOID *, PDWORD64, PKNONVOLATILE_CONTEXT_POINTERS);
typedef BOOLEAN (WINAPI *RtlAddFunctionTable_t)(PRUNTIME_FUNCTION, DWORD, DWORD64);
typedef BOOLEAN (WINAPI *RtlDeleteFunctionTable_t)(PRUNTIME_FUNCTION);
typedef BOOLEAN (WINAPI *RtlInstallFunctionTableCallback_t)(DWORD64, DWORD64, DWORD, PGET_RUNTIME_FUNCTION_CALLBACK, PVOID, PCWSTR);
typedef void (WINAPI *RtlUnwindEx_t)(PVOID, PVOID, PEXCEPTION_RECORD, PVOID, PCONTEXT, PUNWIND_HISTORY_TABLE);
typedef void (WINAPI *RtlUnwind_t)(PVOID, PVOID, PEXCEPTION_RECORD, PVOID);
typedef void (WINAPI *RtlRestoreContext_t)(PCONTEXT, PEXCEPTION_RECORD);
typedef PVOID (WINAPI *RtlPcToFileHeader_t)(PVOID, PVOID *);

#define PCAP    ((RtlCaptureContext_t)t_proc("RtlCaptureContext"))
#define PBT     ((RtlCaptureStackBackTrace_t)t_proc("RtlCaptureStackBackTrace"))
#define PLOOK   ((RtlLookupFunctionEntry_t)t_proc("RtlLookupFunctionEntry"))
#define PVUNW   ((RtlVirtualUnwind_t)t_proc("RtlVirtualUnwind"))
#define PADDFT  ((RtlAddFunctionTable_t)t_proc("RtlAddFunctionTable"))
#define PDELFT  ((RtlDeleteFunctionTable_t)t_proc("RtlDeleteFunctionTable"))
#define PINSTCB ((RtlInstallFunctionTableCallback_t)t_proc("RtlInstallFunctionTableCallback"))
#define PUNWEX  ((RtlUnwindEx_t)t_proc("RtlUnwindEx"))
#define PUNW    ((RtlUnwind_t)t_proc("RtlUnwind"))
#define PRESTORE ((RtlRestoreContext_t)t_proc("RtlRestoreContext"))
#define PPC2FH  ((RtlPcToFileHeader_t)t_proc("RtlPcToFileHeader"))

static HMODULE module_of(const void *addr) {
    HMODULE m = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)addr, &m);
    return m;
}

/* ---- RtlCaptureContext ---- */
static __attribute__((noinline)) void cap_ctx(CONTEXT *c) { PCAP(c); __asm__ volatile("" ::: "memory"); }
TEST(eed_ctx_capture_basic) {
    REQUIRE(PCAP);
    CONTEXT c; memset(&c, 0xCC, sizeof c);
    cap_ctx(&c);
    CHECKF((c.ContextFlags & CONTEXT_CONTROL) == CONTEXT_CONTROL, "flags 0x%lx", c.ContextFlags);
    CHECK((c.ContextFlags & CONTEXT_INTEGER) == CONTEXT_INTEGER);
    t_note("RtlCaptureContext ContextFlags = 0x%lx", c.ContextFlags);
    CHECKF(c.Rip >= (DWORD64)(ULONG_PTR)cap_ctx && c.Rip < (DWORD64)(ULONG_PTR)cap_ctx + 128, "Rip %llx not in cap_ctx %p", (unsigned long long)c.Rip, (void *)cap_ctx);
    ULONG_PTR lo, hi; GetCurrentThreadStackLimits(&lo, &hi);
    CHECK(c.Rsp >= lo && c.Rsp < hi);
    EXPECT_EQ(c.SegCs, 0x33);
    EXPECT_EQ(c.SegSs, 0x2B);
    CHECK(c.EFlags & 2);
    CHECK((c.EFlags & 0x100) == 0);
}
TEST(eed_ctx_capture_mxcsr_default) {
    REQUIRE(PCAP);
    CONTEXT c; cap_ctx(&c);
    EXPECT_EQ(c.MxCsr, 0x1F80);
}
TEST(eed_ctx_capture_rsp_below_caller) {
    REQUIRE(PCAP);
    CONTEXT c; cap_ctx(&c);
    /* the captured Rsp is the callee frame's: lower than the address of our own local */
    CHECK(c.Rsp < (DWORD64)(ULONG_PTR)&c);
}
TEST(eed_ctx_capture_registers) {
    REQUIRE(PCAP);
    CONTEXT c; memset(&c, 0, sizeof c);
    eed_cap_regs(&c, NULL, PCAP);
    EXPECT_EQ(c.Rbx, 0x1111); EXPECT_EQ(c.Rsi, 0x2222); EXPECT_EQ(c.Rdi, 0x3333);
    EXPECT_EQ(c.R12, 0x4444); EXPECT_EQ(c.R13, 0x5555); EXPECT_EQ(c.R14, 0x6666); EXPECT_EQ(c.R15, 0x7777);
    EXPECT_EQ(c.Rbp, 0x8888);
    EXPECT_EQ(c.Rcx, (DWORD64)(ULONG_PTR)&c);
}
TEST(eed_ctx_capture_writes_only_context) {
    REQUIRE(PCAP);
    struct { char pre[64]; CONTEXT c; char post[64]; } s;
    memset(&s, 0x5A, sizeof s);
    cap_ctx(&s.c);
    int ok = 1;
    for (int i = 0; i < 64; i++) if (s.pre[i] != 0x5A || s.post[i] != 0x5A) ok = 0;
    CHECK(ok);
}
TEST(eed_ctx_capture_forwarder_identity) {
    FARPROC a = (FARPROC)UK(RtlCaptureContext), b = (FARPROC)UNT(RtlCaptureContext);
    REQUIRE(a); REQUIRE(b);
    if (a != b) t_note("kernel32!RtlCaptureContext %p is not the ntdll export %p", a, b);
}
TEST(eed_ctx_capture_repeat_stable) {
    REQUIRE(PCAP);
    CONTEXT a, b; cap_ctx(&a); cap_ctx(&b);
    EXPECT_EQ(a.Rip, b.Rip); EXPECT_EQ(a.Rsp, b.Rsp); EXPECT_EQ(a.ContextFlags, b.ContextFlags);
}
static DWORD WINAPI th_ctx(LPVOID p) { cap_ctx((CONTEXT *)p); return 0; }
TEST(eed_ctx_capture_other_thread_stack) {
    REQUIRE(PCAP); REQUIRE(U(CreateThread));
    static CONTEXT c; memset(&c, 0, sizeof c);
    HANDLE h = U(CreateThread)(NULL, 0, th_ctx, &c, 0, NULL);
    CHECK(h != NULL);
    if (h) { WaitForSingleObject(h, 5000); CloseHandle(h); }
    CONTEXT me; cap_ctx(&me);
    CHECK(c.Rsp != 0 && (c.Rsp >> 16) != (me.Rsp >> 16));
}

/* ---- RtlCaptureStackBackTrace ---- */
static __attribute__((noinline)) USHORT bt_leaf(PVOID *f, ULONG skip, ULONG count, PULONG hash) {
    USHORT n = PBT(skip, count, f, hash);
    __asm__ volatile("" ::: "memory");
    return n;
}
static __attribute__((noinline)) USHORT bt_rec(int d, PVOID *f, ULONG skip, ULONG count, PULONG hash) {
    if (d == 0) return bt_leaf(f, skip, count, hash);
    USHORT r = bt_rec(d - 1, f, skip, count, hash);
    __asm__ volatile("" ::: "memory");
    return r;
}
TEST(eed_bt_basic) {
    REQUIRE(PBT);
    PVOID f[64]; ULONG h = 0;
    USHORT n = bt_leaf(f, 0, 62, &h);
    CHECK(n >= 3);
    CHECKF((ULONG_PTR)f[0] > (ULONG_PTR)bt_leaf && (ULONG_PTR)f[0] < (ULONG_PTR)bt_leaf + 128, "frame0 %p vs bt_leaf %p", f[0], (void *)bt_leaf);
    CHECK(h != 0);
}
TEST(eed_bt_depth_counts_recursion) {
    REQUIRE(PBT);
    PVOID f[64]; ULONG h;
    USHORT n0 = bt_rec(0, f, 0, 62, &h);
    USHORT n5 = bt_rec(5, f, 0, 62, &h);
    USHORT n10 = bt_rec(10, f, 0, 62, &h);
    CHECKF(n5 == n0 + 5, "n0=%u n5=%u", n0, n5);
    CHECKF(n10 == n0 + 10, "n0=%u n10=%u", n0, n10);
}
TEST(eed_bt_recursion_frames_inside_function) {
    REQUIRE(PBT);
    PVOID f[64]; ULONG h;
    USHORT n = bt_rec(4, f, 0, 62, &h);
    CHECK(n >= 6);
    for (int i = 1; i <= 4; i++)
        CHECKF((ULONG_PTR)f[i] > (ULONG_PTR)bt_rec && (ULONG_PTR)f[i] < (ULONG_PTR)bt_rec + 128, "frame %d = %p", i, f[i]);
}
TEST(eed_bt_count_zero) {
    REQUIRE(PBT);
    PVOID f[4] = { (PVOID)1, (PVOID)1, (PVOID)1, (PVOID)1 }; ULONG h = 0x77;
    USHORT n = bt_leaf(f, 0, 0, &h);
    EXPECT_EQ(n, 0);
    EXPECT_EQ(f[0], 1);
}
TEST(eed_bt_count_limited) {
    REQUIRE(PBT);
    PVOID f[8]; ULONG h;
    for (ULONG c = 1; c <= 4; c++) {
        memset(f, 0, sizeof f);
        USHORT n = bt_rec(6, f, 0, c, &h);
        CHECKF(n == c, "count %lu returned %u", c, n);
        CHECK(f[c - 1] != NULL);
        if (c < 8) CHECK(f[c] == NULL);
    }
}
TEST(eed_bt_skip_shifts_frames) {
    REQUIRE(PBT);
    PVOID a[64], b[64], c2[64]; ULONG h;
    USHORT na = bt_rec(6, a, 0, 62, &h);
    USHORT nb = bt_rec(6, b, 1, 62, &h);
    USHORT nc = bt_rec(6, c2, 3, 62, &h);
    CHECK(na >= 8);
    EXPECT_EQ(nb, na - 1);
    EXPECT_EQ(nc, na - 3);
    /* frames 0..6 are inside bt_leaf/bt_rec (identical across the three calls); frame 7 is the call site in this test */
    for (int i = 0; i + 1 < 7; i++) if (a[i + 1] != b[i]) { CHECK(0); break; }
    for (int i = 0; i + 3 < 7; i++) if (a[i + 3] != c2[i]) { CHECK(0); break; }
}
TEST(eed_bt_skip_beyond_depth) {
    REQUIRE(PBT);
    PVOID f[16]; ULONG h = 5;
    USHORT n = bt_leaf(f, 500, 10, &h);
    EXPECT_EQ(n, 0);
}
TEST(eed_bt_hash_null_ok) {
    REQUIRE(PBT);
    PVOID f[16];
    USHORT n = bt_leaf(f, 0, 16, NULL);
    CHECK(n > 0);
}
static __attribute__((noinline)) void bt_twice(ULONG *h1, ULONG *h2, USHORT *n1, USHORT *n2) {
    PVOID f[64];
    for (int i = 0; i < 2; i++) {
        ULONG h; USHORT n = PBT(0, 62, f, &h);
        __asm__ volatile("" ::: "memory");
        if (i == 0) { *h1 = h; *n1 = n; } else { *h2 = h; *n2 = n; }
    }
}
static __attribute__((noinline)) void bt_hash_pair(int d, ULONG *a, ULONG *b) {
    PVOID f[64];
    for (int i = 0; i < 2; i++) {
        ULONG h = 0;
        bt_rec(d, f, 0, 62, &h);
        __asm__ volatile("" ::: "memory");
        if (i == 0) *a = h; else *b = h;
    }
}
TEST(eed_bt_hash_stable_and_distinguishes) {
    REQUIRE(PBT);
    PVOID f[64]; ULONG h1, h2, h3, h4;
    USHORT n1, n2;
    bt_twice(&h1, &h2, &n1, &n2);
    EXPECT_EQ(n1, n2);
    t_note("hash from same site twice (different call per loop iteration share the return address): %lx %lx", h1, h2);
    bt_hash_pair(3, &h3, &h4);
    CHECK(h3 != 0);
    EXPECT_EQ(h3, h4);                 /* same call site, same stack: same hash */
    ULONG h5, h6; bt_hash_pair(4, &h5, &h6);
    CHECK(h5 != h3);                   /* deeper stack: different hash */
}
static DWORD WINAPI th_bt(LPVOID p) { PVOID *f = (PVOID *)p; f[63] = (PVOID)(ULONG_PTR)bt_leaf(f, 0, 62, NULL); return 0; }
TEST(eed_bt_thread_stack_ends_in_ntdll) {
    REQUIRE(PBT); REQUIRE(U(CreateThread));
    PVOID f[64]; memset(f, 0, sizeof f);
    HANDLE h = U(CreateThread)(NULL, 0, th_bt, f, 0, NULL);
    CHECK(h != NULL);
    if (!h) return;
    WaitForSingleObject(h, 5000); CloseHandle(h);
    USHORT n = (USHORT)(ULONG_PTR)f[63];
    CHECK(n >= 4);
    if (n < 2) return;
    HMODULE last = module_of(f[n - 1]), prev = module_of(f[n - 2]);
    CHECKF(last == t_ntdll(), "last frame %p module %p (ntdll %p)", f[n - 1], last, t_ntdll());
    CHECKF(prev == t_kernel32(), "prev frame %p module %p (kernel32 %p)", f[n - 2], prev, t_kernel32());
    CHECK((ULONG_PTR)f[n - 3] >= (ULONG_PTR)th_bt && (ULONG_PTR)f[n - 3] < (ULONG_PTR)th_bt + 128);
}
TEST(eed_bt_main_thread_has_system_frames) {
    REQUIRE(PBT);
    PVOID f[64]; USHORT n = bt_leaf(f, 0, 62, NULL);
    int in_nt = 0, in_k32 = 0;
    for (int i = 0; i < n; i++) { HMODULE m = module_of(f[i]); if (m == t_ntdll()) in_nt = 1; if (m == t_kernel32()) in_k32 = 1; }
    CHECK(in_nt); CHECK(in_k32);
}
TEST(eed_bt_large_count_request) {
    REQUIRE(PBT);
    PVOID f[200]; ULONG h;
    USHORT n = bt_rec(8, f, 0, 200, &h);
    CHECK(n >= 10 && n < 200);
}
TEST(eed_bt_forwarder_identity) {
    FARPROC a = (FARPROC)UK(RtlCaptureStackBackTrace), b = (FARPROC)UNT(RtlCaptureStackBackTrace);
    REQUIRE(a); REQUIRE(b);
    if (a != b) t_note("kernel32 export %p is not the ntdll export %p", a, b);
}

/* ---- RtlLookupFunctionEntry / RtlPcToFileHeader ---- */
TEST(eed_lfe_exe_function) {
    REQUIRE(PLOOK);
    DWORD64 base = 0;
    PRUNTIME_FUNCTION rf = PLOOK((DWORD64)(ULONG_PTR)cap_ctx, &base, NULL);
    CHECK(rf != NULL);
    EXPECT_EQ(base, (DWORD64)(ULONG_PTR)GetModuleHandleA(NULL));
    if (rf) {
        DWORD64 pc = (DWORD64)(ULONG_PTR)cap_ctx - base;
        CHECK(rf->BeginAddress <= pc && pc < rf->EndAddress);
        CHECK(rf->UnwindData != 0);
    }
}
TEST(eed_lfe_pc_range_boundaries) {
    REQUIRE(PLOOK);
    DWORD64 base;
    PRUNTIME_FUNCTION rf = PLOOK((DWORD64)(ULONG_PTR)cap_ctx, &base, NULL);
    REQUIRE(rf);
    DWORD64 b2;
    PRUNTIME_FUNCTION f1 = PLOOK(base + rf->BeginAddress, &b2, NULL);
    CHECK(f1 == rf);
    f1 = PLOOK(base + rf->EndAddress - 1, &b2, NULL);
    CHECK(f1 == rf);
    f1 = PLOOK(base + rf->EndAddress, &b2, NULL);
    CHECK(f1 != rf);
}
TEST(eed_lfe_ntdll_kernel32_functions) {
    REQUIRE(PLOOK);
    HMODULE mods[3] = { t_ntdll(), t_kernel32(), t_kernelbase() };
    const char *names[3] = { "RtlCaptureContext", "CreateFileW", "CreateFileW" };
    for (int i = 0; i < 3; i++) {
        FARPROC p = GetProcAddress(mods[i], names[i]);
        if (!p) continue;
        DWORD64 base = 0;
        PRUNTIME_FUNCTION rf = PLOOK((DWORD64)(ULONG_PTR)p, &base, NULL);
        /* kernel32!CreateFileW may be a forwarder stub resolving into kernelbase: only require consistency */
        HMODULE owner = module_of(p);
        if (!rf) { t_note("no function entry for %s in module %d", names[i], i); continue; }
        EXPECT_EQ(base, (DWORD64)(ULONG_PTR)owner);
    }
}
TEST(eed_lfe_unmapped_addresses_null) {
    REQUIRE(PLOOK);
    static const DWORD64 pcs[] = { 0, 1, 0x10, 0x1000, 0x7FFE0000, 0xFFFF800000000000ull };
    for (size_t i = 0; i < sizeof pcs / sizeof *pcs; i++) {
        DWORD64 base = 0x7777;
        PRUNTIME_FUNCTION rf = PLOOK(pcs[i], &base, NULL);
        CHECKF(rf == NULL, "pc %llx", (unsigned long long)pcs[i]);
        t_note("lookup(%llx): ImageBase out = %llx", (unsigned long long)pcs[i], (unsigned long long)base);
    }
}
TEST(eed_lfe_stack_and_heap_null) {
    REQUIRE(PLOOK);
    int local;
    DWORD64 base;
    CHECK(PLOOK((DWORD64)(ULONG_PTR)&local, &base, NULL) == NULL);
    void *p = malloc(64);
    CHECK(PLOOK((DWORD64)(ULONG_PTR)p, &base, NULL) == NULL);
    free(p);
}
TEST(eed_lfe_with_history_table) {
    REQUIRE(PLOOK);
    UNWIND_HISTORY_TABLE ht; memset(&ht, 0, sizeof ht);
    DWORD64 base, b2;
    PRUNTIME_FUNCTION r1 = PLOOK((DWORD64)(ULONG_PTR)cap_ctx, &base, &ht);
    PRUNTIME_FUNCTION r2 = PLOOK((DWORD64)(ULONG_PTR)cap_ctx, &b2, &ht);
    CHECK(r1 != NULL); CHECK(r1 == r2); EXPECT_EQ(base, b2);
    PRUNTIME_FUNCTION r3 = PLOOK((DWORD64)(ULONG_PTR)cap_ctx, &b2, NULL);
    CHECK(r1 == r3);
}
TEST(eed_lfe_null_history_and_repeat) {
    REQUIRE(PLOOK);
    DWORD64 b;
    PRUNTIME_FUNCTION first = PLOOK((DWORD64)(ULONG_PTR)bt_leaf, &b, NULL);
    for (int i = 0; i < 500; i++) if (PLOOK((DWORD64)(ULONG_PTR)bt_leaf + 3, &b, NULL) != first) { CHECK(0); break; }
}
TEST(eed_lfe_forwarder_identity) {
    FARPROC a = (FARPROC)UK(RtlLookupFunctionEntry), b = (FARPROC)UNT(RtlLookupFunctionEntry);
    REQUIRE(a); REQUIRE(b);
    if (a != b) t_note("kernel32 export %p is not the ntdll export %p", a, b);
}
TEST(eed_pctofileheader_exe_and_dlls) {
    REQUIRE(PPC2FH);
    PVOID base = NULL;
    PVOID r = PPC2FH((PVOID)cap_ctx, &base);
    EXPECT_EQ(r, GetModuleHandleA(NULL)); EXPECT_EQ(base, r);
    r = PPC2FH((PVOID)GetProcAddress(t_kernel32(), "Sleep"), &base);
    CHECK(r != NULL);
    r = PPC2FH((PVOID)GetProcAddress(t_ntdll(), "RtlCaptureContext"), &base);
    EXPECT_EQ(r, t_ntdll());
}
TEST(eed_pctofileheader_non_image) {
    REQUIRE(PPC2FH);
    PVOID base = (PVOID)1;
    int local;
    PVOID r = PPC2FH(&local, &base);
    EXPECT_EQ(r, 0);
    r = PPC2FH((PVOID)0x10, &base);
    EXPECT_EQ(r, 0);
}

/* ---- RtlVirtualUnwind on real frames ---- */
static __attribute__((noinline)) int unw_inner(DWORD64 *res) {
    CONTEXT c; PCAP(&c);
    DWORD64 base; PRUNTIME_FUNCTION rf = PLOOK(c.Rip, &base, NULL);
    if (!rf) return -1;
    PVOID hd = NULL; DWORD64 frame = 0;
    DWORD64 rsp0 = c.Rsp, rip0 = c.Rip;
    PEXCEPTION_ROUTINE h = PVUNW(UNW_FLAG_NHANDLER, base, c.Rip, rf, &c, &hd, &frame, NULL);
    res[0] = rip0; res[1] = rsp0; res[2] = c.Rip; res[3] = c.Rsp; res[4] = frame; res[5] = (DWORD64)(ULONG_PTR)h;
    __asm__ volatile("" ::: "memory");
    return 0;
}
static __attribute__((noinline)) int unw_outer(DWORD64 *res) { int r = unw_inner(res); __asm__ volatile("" ::: "memory"); return r + 0; }
TEST(eed_vu_unwind_one_frame_to_caller) {
    REQUIRE(PCAP); REQUIRE(PLOOK); REQUIRE(PVUNW);
    DWORD64 r[6];
    int rc = unw_outer(r);
    EXPECT_EQ(rc, 0);
    CHECKF(r[2] > (DWORD64)(ULONG_PTR)unw_outer && r[2] < (DWORD64)(ULONG_PTR)unw_outer + 128, "caller rip %llx, unw_outer %p", (unsigned long long)r[2], (void *)unw_outer);
    CHECK(r[3] > r[1]);                 /* Rsp moved up the stack */
    CHECK(r[4] >= r[1]);                /* establisher frame at/above the original Rsp */
    EXPECT_EQ(r[5], 0);                 /* UNW_FLAG_NHANDLER: no handler returned */
}
static __attribute__((noinline)) int walk_stack(DWORD64 *rips, int max) {
    CONTEXT c; PCAP(&c);
    int n = 0;
    while (n < max && c.Rip) {
        DWORD64 base;
        PRUNTIME_FUNCTION rf = PLOOK(c.Rip, &base, NULL);
        rips[n++] = c.Rip;
        if (!rf) break;
        PVOID hd; DWORD64 fr;
        PVUNW(UNW_FLAG_NHANDLER, base, c.Rip, rf, &c, &hd, &fr, NULL);
    }
    __asm__ volatile("" ::: "memory");
    return n;
}
static DWORD WINAPI th_walk(LPVOID p) { *(int *)((DWORD64 *)p + 63) = walk_stack((DWORD64 *)p, 60); return 0; }
TEST(eed_vu_walk_thread_stack_to_ntdll) {
    REQUIRE(PCAP); REQUIRE(PLOOK); REQUIRE(PVUNW); REQUIRE(U(CreateThread));
    DWORD64 rips[64]; memset(rips, 0, sizeof rips);
    HANDLE h = U(CreateThread)(NULL, 0, th_walk, rips, 0, NULL);
    CHECK(h != NULL); if (!h) return;
    WaitForSingleObject(h, 5000); CloseHandle(h);
    int n = *(int *)(rips + 63);
    CHECK(n >= 4);
    int k32 = 0, nt = 0;
    for (int i = 0; i < n; i++) { HMODULE m = module_of((void *)(ULONG_PTR)rips[i]); if (m == t_kernel32()) k32 = 1; if (m == t_ntdll()) nt = 1; }
    CHECK(k32); CHECK(nt);
    HMODULE lastm = module_of((void *)(ULONG_PTR)rips[n - 1]);
    CHECK(lastm == t_ntdll());
}
static __attribute__((noinline)) int cmp_walk_bt(void) {
    PVOID bt[40]; DWORD64 rips[40];
    USHORT nb = PBT(0, 40, bt, NULL);
    CONTEXT c; PCAP(&c);
    DWORD64 base; PRUNTIME_FUNCTION rf = PLOOK(c.Rip, &base, NULL);
    PVOID hd; DWORD64 fr;
    PVUNW(UNW_FLAG_NHANDLER, base, c.Rip, rf, &c, &hd, &fr, NULL);
    int n = 0;
    int match = 1;
    while (n + 1 < nb && c.Rip && n < 8) {
        if (c.Rip != (DWORD64)(ULONG_PTR)bt[n + 1]) match = 0;
        rf = PLOOK(c.Rip, &base, NULL);
        if (!rf) break;
        PVUNW(UNW_FLAG_NHANDLER, base, c.Rip, rf, &c, &hd, &fr, NULL);
        n++;
    }
    __asm__ volatile("" ::: "memory");
    return match && n >= 3;
}
TEST(eed_vu_walk_matches_backtrace) {
    REQUIRE(PCAP); REQUIRE(PLOOK); REQUIRE(PVUNW); REQUIRE(PBT);
    CHECK(cmp_walk_bt());
}

/* ---- JIT code with hand-written unwind info ---- */
#define JIT_CODE_LEN 0x0E
#pragma pack(push, 1)
typedef struct { unsigned char vf, prolog, ncodes, frame; unsigned char code0_off, code0_op, pad0, pad1; DWORD handler; DWORD hdata; } JIT_UNWIND;
#pragma pack(pop)
typedef struct { unsigned char *base; RUNTIME_FUNCTION *rf; JIT_UNWIND *ui; } jit_t;
/* layout: +0 code, +0x100 unwind info, +0x200 RUNTIME_FUNCTION[4], +0x300 handler thunk */
static int jit_make(jit_t *j, int with_handler) {
    j->base = U(VirtualAlloc)(NULL, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!j->base) return 0;
    static const unsigned char code[JIT_CODE_LEN] = { 0x48, 0x83, 0xEC, 0x28,  0xFF, 0xD1,  0x48, 0x83, 0xC4, 0x28,  0xC3,  0xCC, 0xCC, 0xCC };
    memcpy(j->base, code, sizeof code);
    j->ui = (JIT_UNWIND *)(j->base + 0x100);
    memset(j->ui, 0, sizeof *j->ui);
    j->ui->vf = 1 | (with_handler ? (UNW_FLAG_EHANDLER | UNW_FLAG_UHANDLER) << 3 : 0);
    j->ui->prolog = 4; j->ui->ncodes = 1; j->ui->frame = 0;
    j->ui->code0_off = 4; j->ui->code0_op = (4 << 4) | 2;      /* UWOP_ALLOC_SMALL, (0x28-8)/8 */
    j->ui->handler = 0x300; j->ui->hdata = 0xABCD1234;
    j->rf = (RUNTIME_FUNCTION *)(j->base + 0x200);
    memset(j->rf, 0, 4 * sizeof *j->rf);
    j->rf[0].BeginAddress = 0; j->rf[0].EndAddress = JIT_CODE_LEN; j->rf[0].UnwindData = 0x100;
    return 1;
}
static void jit_free(jit_t *j) { if (j->base) U(VirtualFree)(j->base, 0, MEM_RELEASE); j->base = NULL; }
#define NEEDJIT() REQUIRE(PADDFT); REQUIRE(PDELFT); REQUIRE(PLOOK); REQUIRE(PVUNW); REQUIRE(U(VirtualAlloc)); REQUIRE(U(VirtualFree))

TEST(eed_ft_lookup_unregistered_null) {
    NEEDJIT();
    jit_t j; CHECK(jit_make(&j, 0));
    DWORD64 base = 0x55;
    CHECK(PLOOK((DWORD64)(ULONG_PTR)j.base + 6, &base, NULL) == NULL);
    jit_free(&j);
}
TEST(eed_ft_add_lookup_delete) {
    NEEDJIT();
    jit_t j; CHECK(jit_make(&j, 0));
    DWORD64 b = (DWORD64)(ULONG_PTR)j.base, ib;
    CHECK(PADDFT(j.rf, 1, b) != 0);
    PRUNTIME_FUNCTION r = PLOOK(b + 6, &ib, NULL);
    CHECK(r == &j.rf[0]); EXPECT_EQ(ib, b);
    CHECK(PLOOK(b, &ib, NULL) == &j.rf[0]);
    CHECK(PLOOK(b + JIT_CODE_LEN - 1, &ib, NULL) == &j.rf[0]);
    CHECK(PLOOK(b + JIT_CODE_LEN, &ib, NULL) == NULL);
    CHECK(PLOOK(b - 1, &ib, NULL) == NULL);
    CHECK(PLOOK(b + 0x800, &ib, NULL) == NULL);
    CHECK(PDELFT(j.rf) != 0);
    CHECK(PLOOK(b + 6, &ib, NULL) == NULL);
    jit_free(&j);
}
TEST(eed_ft_delete_twice) {
    NEEDJIT();
    jit_t j; CHECK(jit_make(&j, 0));
    CHECK(PADDFT(j.rf, 1, (DWORD64)(ULONG_PTR)j.base) != 0);
    CHECK(PDELFT(j.rf) != 0);
    BOOLEAN r = PDELFT(j.rf);
    EXPECT_EQ(r, 0);
    jit_free(&j);
}
TEST(eed_ft_delete_never_added) {
    NEEDJIT();
    jit_t j; CHECK(jit_make(&j, 0));
    BOOLEAN r = PDELFT(j.rf);
    EXPECT_EQ(r, 0);
    jit_free(&j);
}
TEST(eed_ft_multiple_entries_sorted) {
    NEEDJIT();
    jit_t j; CHECK(jit_make(&j, 0));
    /* three adjacent functions sharing the same unwind info */
    unsigned char *c = j.base;
    for (int i = 0; i < 3; i++) memcpy(c + 0x400 + i * 0x20, c, JIT_CODE_LEN);
    for (int i = 0; i < 3; i++) { j.rf[i].BeginAddress = 0x400 + i * 0x20; j.rf[i].EndAddress = 0x400 + i * 0x20 + JIT_CODE_LEN; j.rf[i].UnwindData = 0x100; }
    DWORD64 b = (DWORD64)(ULONG_PTR)j.base, ib;
    CHECK(PADDFT(j.rf, 3, b) != 0);
    for (int i = 0; i < 3; i++) CHECK(PLOOK(b + 0x400 + i * 0x20 + 5, &ib, NULL) == &j.rf[i]);
    CHECK(PLOOK(b + 0x400 + 0x20 - 1, &ib, NULL) == NULL);
    CHECK(PLOOK(b + 6, &ib, NULL) == NULL);
    PDELFT(j.rf);
    jit_free(&j);
}
TEST(eed_ft_two_tables_independent) {
    NEEDJIT();
    jit_t a, b; CHECK(jit_make(&a, 0)); CHECK(jit_make(&b, 0));
    DWORD64 ba = (DWORD64)(ULONG_PTR)a.base, bb = (DWORD64)(ULONG_PTR)b.base, ib;
    CHECK(PADDFT(a.rf, 1, ba)); CHECK(PADDFT(b.rf, 1, bb));
    CHECK(PLOOK(ba + 6, &ib, NULL) == &a.rf[0]);
    CHECK(PLOOK(bb + 6, &ib, NULL) == &b.rf[0]);
    CHECK(PDELFT(a.rf));
    CHECK(PLOOK(ba + 6, &ib, NULL) == NULL);
    CHECK(PLOOK(bb + 6, &ib, NULL) == &b.rf[0]);
    CHECK(PDELFT(b.rf));
    jit_free(&a); jit_free(&b);
}
TEST(eed_ft_forwarders_identity) {
    CHECK(UK(RtlAddFunctionTable) != NULL);
    CHECK(UK(RtlDeleteFunctionTable) != NULL);
    CHECK(UK(RtlInstallFunctionTableCallback) != NULL);
    CHECK(UK(RtlVirtualUnwind) != NULL);
    CHECK(UK(RtlUnwindEx) != NULL);
    CHECK(UK(RtlUnwind) != NULL);
    CHECK(UK(RtlRestoreContext) != NULL);
    CHECK(UK(RtlPcToFileHeader) != NULL);
    if (UK(RtlVirtualUnwind) != UNT(RtlVirtualUnwind)) t_note("kernel32!RtlVirtualUnwind is not the ntdll export");
}
static void fake_ctx(CONTEXT *c, jit_t *j, unsigned off, DWORD64 *stack) {
    memset(c, 0, sizeof *c);
    c->ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    c->Rip = (DWORD64)(ULONG_PTR)j->base + off; c->Rsp = (DWORD64)(ULONG_PTR)stack;
}
TEST(eed_ft_virtual_unwind_after_prolog) {
    NEEDJIT();
    jit_t j; CHECK(jit_make(&j, 0));
    DWORD64 b = (DWORD64)(ULONG_PTR)j.base, ib;
    CHECK(PADDFT(j.rf, 1, b));
    DWORD64 stack[16]; for (int i = 0; i < 16; i++) stack[i] = 0xAA00 + i;
    stack[5] = 0xDEADBEEF1234ull;               /* return address at Rsp + 0x28 */
    CONTEXT c; fake_ctx(&c, &j, 6, stack);
    PRUNTIME_FUNCTION rf = PLOOK(c.Rip, &ib, NULL);
    CHECK(rf == &j.rf[0]);
    PVOID hd = (PVOID)1; DWORD64 frame = 0;
    PEXCEPTION_ROUTINE h = PVUNW(UNW_FLAG_NHANDLER, b, c.Rip, rf, &c, &hd, &frame, NULL);
    EXPECT_EQ(h, 0);
    EXPECT_EQ(c.Rip, 0xDEADBEEF1234ull);
    EXPECT_EQ(c.Rsp, (DWORD64)(ULONG_PTR)&stack[6]);
    EXPECT_EQ(frame, (DWORD64)(ULONG_PTR)&stack[0]);
    PDELFT(j.rf);
    jit_free(&j);
}
TEST(eed_ft_virtual_unwind_offsets) {
    NEEDJIT();
    jit_t j; CHECK(jit_make(&j, 0));
    DWORD64 b = (DWORD64)(ULONG_PTR)j.base, ib;
    CHECK(PADDFT(j.rf, 1, b));
    DWORD64 stack[16];
    for (unsigned off = 0; off < 10; off++) {
        for (int i = 0; i < 16; i++) stack[i] = 0xAA00 + i;
        stack[0] = 0x1111; stack[5] = 0x5555;
        CONTEXT c; fake_ctx(&c, &j, off, stack);
        PRUNTIME_FUNCTION rf = PLOOK(c.Rip, &ib, NULL);
        PVOID hd; DWORD64 frame;
        PVUNW(UNW_FLAG_NHANDLER, b, c.Rip, rf, &c, &hd, &frame, NULL);
        if (off < 4) {         /* before `sub rsp,28h` completed: nothing to undo */
            CHECKF(c.Rip == 0x1111 && c.Rsp == (DWORD64)(ULONG_PTR)&stack[1], "off %u rip %llx rsp+%lld", off, (unsigned long long)c.Rip, (long long)(c.Rsp - (DWORD64)(ULONG_PTR)stack));
        } else if (off < 10) { /* body: 0x28 bytes allocated */
            CHECKF(c.Rip == 0x5555 && c.Rsp == (DWORD64)(ULONG_PTR)&stack[6], "off %u rip %llx rsp+%lld", off, (unsigned long long)c.Rip, (long long)(c.Rsp - (DWORD64)(ULONG_PTR)stack));
        }
    }
    PDELFT(j.rf);
    jit_free(&j);
}
TEST(eed_ft_virtual_unwind_returns_handler) {
    NEEDJIT();
    jit_t j; CHECK(jit_make(&j, 1));
    DWORD64 b = (DWORD64)(ULONG_PTR)j.base, ib;
    CHECK(PADDFT(j.rf, 1, b));
    DWORD64 stack[16]; for (int i = 0; i < 16; i++) stack[i] = 0;
    stack[5] = 0x4242;
    CONTEXT c; fake_ctx(&c, &j, 6, stack);
    PRUNTIME_FUNCTION rf = PLOOK(c.Rip, &ib, NULL);
    PVOID hd = NULL; DWORD64 frame;
    PEXCEPTION_ROUTINE h = PVUNW(UNW_FLAG_EHANDLER, b, c.Rip, rf, &c, &hd, &frame, NULL);
    EXPECT_EQ(h, (PEXCEPTION_ROUTINE)(b + 0x300));
    EXPECT_EQ(hd, (PVOID)(ULONG_PTR)(b + 0x100 + offsetof(JIT_UNWIND, hdata)));
    /* wrong flag class: no handler */
    CONTEXT c2; fake_ctx(&c2, &j, 6, stack);
    h = PVUNW(UNW_FLAG_NHANDLER, b, c2.Rip, rf, &c2, &hd, &frame, NULL);
    EXPECT_EQ(h, 0);
    PDELFT(j.rf);
    jit_free(&j);
}
TEST(eed_ft_virtual_unwind_nonvolatile_pointers) {
    NEEDJIT();
    jit_t j; CHECK(jit_make(&j, 0));
    DWORD64 b = (DWORD64)(ULONG_PTR)j.base, ib;
    CHECK(PADDFT(j.rf, 1, b));
    DWORD64 stack[16]; memset(stack, 0, sizeof stack); stack[5] = 0x4242;
    CONTEXT c; fake_ctx(&c, &j, 6, stack);
    KNONVOLATILE_CONTEXT_POINTERS np; memset(&np, 0, sizeof np);
    PRUNTIME_FUNCTION rf = PLOOK(c.Rip, &ib, NULL);
    PVOID hd; DWORD64 frame;
    PVUNW(UNW_FLAG_NHANDLER, b, c.Rip, rf, &c, &hd, &frame, &np);
    CHECK(c.Rip == 0x4242);
    PDELFT(j.rf);
    jit_free(&j);
}

typedef void (*jit_fn)(void (*cb)(void));
static void *g_bt_frames[40]; static USHORT g_bt_n;
static DWORD64 g_walk[40]; static int g_walk_n;
static void cb_collect(void) {
    g_bt_n = PBT(0, 40, g_bt_frames, NULL);
    CONTEXT c; PCAP(&c);
    g_walk_n = 0;
    while (g_walk_n < 40 && c.Rip) {
        DWORD64 base; PRUNTIME_FUNCTION rf = PLOOK(c.Rip, &base, NULL);
        g_walk[g_walk_n++] = c.Rip;
        if (!rf) break;
        PVOID hd; DWORD64 fr;
        PVUNW(UNW_FLAG_NHANDLER, base, c.Rip, rf, &c, &hd, &fr, NULL);
    }
}
TEST(eed_ft_stack_walk_through_jit_frame) {
    NEEDJIT(); REQUIRE(PBT); REQUIRE(PCAP);
    jit_t j; CHECK(jit_make(&j, 0));
    DWORD64 b = (DWORD64)(ULONG_PTR)j.base;
    CHECK(PADDFT(j.rf, 1, b));
    g_bt_n = 0;
    ((jit_fn)(void *)j.base)(cb_collect);
    int seen_bt = 0, seen_walk = 0, after_bt = 0, after_walk = 0;
    for (int i = 0; i < g_bt_n; i++) {
        if ((ULONG_PTR)g_bt_frames[i] == (ULONG_PTR)j.base + 6) { seen_bt = 1; if (i + 1 < g_bt_n && module_of(g_bt_frames[i + 1]) == GetModuleHandleA(NULL)) after_bt = 1; }
    }
    for (int i = 0; i < g_walk_n; i++) {
        if (g_walk[i] == b + 6) { seen_walk = 1; if (i + 1 < g_walk_n && module_of((void *)(ULONG_PTR)g_walk[i + 1]) == GetModuleHandleA(NULL)) after_walk = 1; }
    }
    CHECKF(seen_bt, "JIT return address not in backtrace (n=%u)", g_bt_n);
    CHECK(after_bt);
    CHECK(seen_walk); CHECK(after_walk);
    CHECK(g_bt_n >= 5);
    PDELFT(j.rf);
    jit_free(&j);
}

/* RtlInstallFunctionTableCallback */
static volatile LONG g_cb_calls; static volatile DWORD64 g_cb_pc; static PVOID g_cb_ctx; static PRUNTIME_FUNCTION g_cb_ret;
static PRUNTIME_FUNCTION NTAPI ft_callback(DWORD64 ControlPc, PVOID Context) {
    InterlockedIncrement(&g_cb_calls); g_cb_pc = ControlPc; g_cb_ctx = Context;
    return g_cb_ret;
}
TEST(eed_ftcb_install_lookup_delete) {
    NEEDJIT(); REQUIRE(PINSTCB);
    jit_t j; CHECK(jit_make(&j, 0));
    DWORD64 b = (DWORD64)(ULONG_PTR)j.base, ib;
    g_cb_calls = 0; g_cb_ret = &j.rf[0];
    BOOLEAN r = PINSTCB(b | 3, b, 0x1000, ft_callback, (PVOID)0x5151, NULL);
    CHECK(r != 0);
    PRUNTIME_FUNCTION rf = PLOOK(b + 6, &ib, NULL);
    CHECK(rf == &j.rf[0]);
    CHECK(g_cb_calls >= 1);
    EXPECT_EQ(g_cb_pc, b + 6);
    EXPECT_EQ(g_cb_ctx, 0x5151);
    EXPECT_EQ(ib, b);
    g_cb_ret = NULL;
    CHECK(PLOOK(b + 0x80, &ib, NULL) == NULL);
    g_cb_ret = &j.rf[0];
    CHECK(PDELFT((PRUNTIME_FUNCTION)(b | 3)) != 0);
    LONG before = g_cb_calls;
    CHECK(PLOOK(b + 6, &ib, NULL) == NULL);
    EXPECT_EQ(g_cb_calls, before);
    jit_free(&j);
}
TEST(eed_ftcb_range_limits_callback) {
    NEEDJIT(); REQUIRE(PINSTCB);
    jit_t j; CHECK(jit_make(&j, 0));
    DWORD64 b = (DWORD64)(ULONG_PTR)j.base, ib;
    g_cb_calls = 0; g_cb_ret = &j.rf[0];
    CHECK(PINSTCB(b | 3, b + 0x100, 0x100, ft_callback, NULL, NULL));
    PLOOK(b + 6, &ib, NULL);                  /* outside [b+0x100, b+0x200): callback must not fire */
    EXPECT_EQ(g_cb_calls, 0);
    PLOOK(b + 0x180, &ib, NULL);
    EXPECT_EQ(g_cb_calls, 1);
    PLOOK(b + 0x200, &ib, NULL);              /* one past the end */
    EXPECT_EQ(g_cb_calls, 1);
    PDELFT((PRUNTIME_FUNCTION)(b | 3));
    jit_free(&j);
}
TEST(eed_ftcb_unwind_through_callback_table) {
    NEEDJIT(); REQUIRE(PINSTCB); REQUIRE(PBT); REQUIRE(PCAP);
    jit_t j; CHECK(jit_make(&j, 0));
    DWORD64 b = (DWORD64)(ULONG_PTR)j.base;
    g_cb_ret = &j.rf[0];
    CHECK(PINSTCB(b | 3, b, 0x1000, ft_callback, NULL, NULL));
    g_bt_n = 0;
    ((jit_fn)(void *)j.base)(cb_collect);
    int seen = 0; for (int i = 0; i < g_bt_n; i++) if ((ULONG_PTR)g_bt_frames[i] == (ULONG_PTR)j.base + 6) seen = 1;
    CHECK(seen);
    PDELFT((PRUNTIME_FUNCTION)(b | 3));
    jit_free(&j);
}
TEST(eed_ftcb_identifier_without_low_bits_fails) {
    NEEDJIT(); REQUIRE(PINSTCB);
    jit_t j; CHECK(jit_make(&j, 0));
    DWORD64 b = (DWORD64)(ULONG_PTR)j.base;
    BOOLEAN r = PINSTCB(b, b, 0x1000, ft_callback, NULL, NULL);
    EXPECT_EQ(r, 0);
    if (r) PDELFT((PRUNTIME_FUNCTION)b);
    jit_free(&j);
}

/* growable tables (Win8+) */
typedef NTSTATUS (NTAPI *RtlAddGrowableFunctionTable_t)(PVOID *, PRUNTIME_FUNCTION, DWORD, DWORD, ULONG_PTR, ULONG_PTR);
typedef void (NTAPI *RtlGrowFunctionTable_t)(PVOID, DWORD);
typedef void (NTAPI *RtlDeleteGrowableFunctionTable_t)(PVOID);
TEST(eed_ftg_growable_table) {
    NEEDJIT();
    RtlAddGrowableFunctionTable_t add = (RtlAddGrowableFunctionTable_t)t_proc_nt("RtlAddGrowableFunctionTable");
    RtlGrowFunctionTable_t grow = (RtlGrowFunctionTable_t)t_proc_nt("RtlGrowFunctionTable");
    RtlDeleteGrowableFunctionTable_t del = (RtlDeleteGrowableFunctionTable_t)t_proc_nt("RtlDeleteGrowableFunctionTable");
    REQUIRE(add); REQUIRE(grow); REQUIRE(del);
    jit_t j; CHECK(jit_make(&j, 0));
    unsigned char *c = j.base;
    memcpy(c + 0x400, c, JIT_CODE_LEN);
    j.rf[1].BeginAddress = 0x400; j.rf[1].EndAddress = 0x400 + JIT_CODE_LEN; j.rf[1].UnwindData = 0x100;
    DWORD64 b = (DWORD64)(ULONG_PTR)j.base, ib;
    PVOID handle = NULL;
    NTSTATUS st = add(&handle, j.rf, 1, 4, (ULONG_PTR)j.base, (ULONG_PTR)j.base + 0x1000);
    EXPECT_EQ(st, 0);
    CHECK(handle != NULL);
    if (st == 0) {
        CHECK(PLOOK(b + 6, &ib, NULL) == &j.rf[0]);
        CHECK(PLOOK(b + 0x405, &ib, NULL) == NULL);
        grow(handle, 2);
        CHECK(PLOOK(b + 0x405, &ib, NULL) == &j.rf[1]);
        del(handle);
        CHECK(PLOOK(b + 6, &ib, NULL) == NULL);
    }
    jit_free(&j);
}

/* ---- language specific handler (exception + unwind) on a JIT frame ---- */
static void make_thunk(jit_t *j, void *target) {
    unsigned char *t = j->base + 0x300;
    t[0] = 0x48; t[1] = 0xB8; memcpy(t + 2, &target, 8); t[10] = 0xFF; t[11] = 0xE0;
}
typedef struct {
    LONG calls; DWORD code[8], flags[8]; PVOID frame[8]; DWORD64 control_pc[8], image_base[8], target_ip[8];
    PRUNTIME_FUNCTION fentry[8]; PVOID hdata[8]; PEXCEPTION_ROUTINE lang[8];
} hlog_t;
static hlog_t g_h;
static volatile EXCEPTION_DISPOSITION g_h_ret = ExceptionContinueSearch;
static EXCEPTION_DISPOSITION NTAPI jit_handler(PEXCEPTION_RECORD er, PVOID frame, PCONTEXT ctx, PDISPATCHER_CONTEXT dc) {
    LONG i = InterlockedIncrement(&g_h.calls) - 1;
    if (i < 8) {
        g_h.code[i] = er->ExceptionCode; g_h.flags[i] = er->ExceptionFlags; g_h.frame[i] = frame;
        g_h.control_pc[i] = dc->ControlPc; g_h.image_base[i] = dc->ImageBase; g_h.target_ip[i] = dc->TargetIp;
        g_h.fentry[i] = dc->FunctionEntry; g_h.hdata[i] = dc->HandlerData; g_h.lang[i] = dc->LanguageHandler;
    }
    return g_h_ret;
}
static volatile LONG g_uef_hits;
static LONG WINAPI uef_safety(PEXCEPTION_POINTERS ep) {
    if (ep->ExceptionRecord->ExceptionCode == 0xE0A00060 && !(ep->ExceptionRecord->ExceptionFlags & EXCEPTION_NONCONTINUABLE)) { InterlockedIncrement(&g_uef_hits); return EXCEPTION_CONTINUE_EXECUTION; }
    return EXCEPTION_CONTINUE_SEARCH;
}
static void cb_raise(void) { RaiseException(0xE0A00060, 0, 1, (const ULONG_PTR[]){ 0x77 }); }
TEST(eed_ft_language_handler_called_for_exception) {
    NEEDJIT(); REQUIRE(U(RaiseException));
    jit_t j; CHECK(jit_make(&j, 1));
    make_thunk(&j, jit_handler);
    DWORD64 b = (DWORD64)(ULONG_PTR)j.base;
    CHECK(PADDFT(j.rf, 1, b));
    memset(&g_h, 0, sizeof g_h);
    g_h_ret = ExceptionContinueExecution;
    volatile int after = 0;
    g_uef_hits = 0;
    LPTOP_LEVEL_EXCEPTION_FILTER oldf = SetUnhandledExceptionFilter(uef_safety);   /* safety net: keeps a mismatch from killing the run */
    ((jit_fn)(void *)j.base)(cb_raise);
    SetUnhandledExceptionFilter(oldf);
    after = 1;
    EXPECT_EQ(after, 1);
    EXPECT_EQ(g_uef_hits, 0);
    EXPECT_EQ(g_h.calls, 1);
    EXPECT_EQ(g_h.code[0], 0xE0A00060);
    EXPECT_EQ(g_h.flags[0], 0);
    EXPECT_EQ(g_h.image_base[0], b);
    EXPECT_EQ(g_h.fentry[0], &j.rf[0]);
    EXPECT_EQ(g_h.lang[0], (PEXCEPTION_ROUTINE)(b + 0x300));
    EXPECT_EQ(g_h.hdata[0], (PVOID)(ULONG_PTR)(b + 0x100 + offsetof(JIT_UNWIND, hdata)));
    CHECKF(g_h.control_pc[0] >= b && g_h.control_pc[0] <= b + JIT_CODE_LEN, "ControlPc %llx", (unsigned long long)g_h.control_pc[0]);
    t_note("handler ControlPc offset %lld EstablisherFrame %p", (long long)(g_h.control_pc[0] - b), g_h.frame[0]);
    PDELFT(j.rf);
    jit_free(&j);
}

/* ---- RtlUnwindEx / RtlUnwind driving language-specific handlers during unwind ---- */
static volatile int g_ustage, g_umode, g_use_rec;
static volatile DWORD64 g_urax;
static PVOID g_utarget, g_utip;
static EXCEPTION_RECORD g_urec;
static CONTEXT g_uscratch;
static void cb_unwind(void) {
    if (g_umode == 0) PUNWEX(g_utarget, g_utip, g_use_rec ? &g_urec : NULL, (PVOID)0x99, &g_uscratch, NULL);
    else PUNW(g_utarget, g_utip, g_use_rec ? &g_urec : NULL, (PVOID)0x99);
    g_ustage = 98;      /* not reached */
}
static __attribute__((noinline, optimize("O0"))) int unwind_target(jit_t *j, DWORD64 *tip_out) {
    CONTEXT c;
    g_ustage = 0;
    PCAP(&c);
    __asm__ volatile("movq %%rax, %0" : "=m"(g_urax));
    if (g_ustage == 0) {
        g_ustage = 1;
        CONTEXT t = c; DWORD64 base; PRUNTIME_FUNCTION rf = PLOOK(t.Rip, &base, NULL); PVOID hd; DWORD64 frame;
        PVUNW(UNW_FLAG_NHANDLER, base, t.Rip, rf, &t, &hd, &frame, NULL);
        g_utarget = (PVOID)(ULONG_PTR)frame; g_utip = (PVOID)(ULONG_PTR)c.Rip;
        *tip_out = c.Rip;
        ((jit_fn)(void *)j->base)(cb_unwind);
        g_ustage = 98;
        return -1;
    }
    return g_ustage;
}
static void run_unwind(int mode, int use_rec, int expect_handler) {
    NEEDJIT();
    if (!PUNWEX || !PUNW || !PCAP) { t_skip("missing"); return; }
    jit_t j; if (!jit_make(&j, 1)) { CHECK(0); return; }
    make_thunk(&j, jit_handler);
    DWORD64 b = (DWORD64)(ULONG_PTR)j.base;
    CHECK(PADDFT(j.rf, 1, b));
    memset(&g_h, 0, sizeof g_h);
    g_h_ret = ExceptionContinueSearch;
    g_umode = mode; g_use_rec = use_rec;
    memset(&g_urec, 0, sizeof g_urec);
    g_urec.ExceptionCode = 0xE0A00061; g_urec.ExceptionFlags = 0;
    DWORD64 tip = 0;
    int r = unwind_target(&j, &tip);
    EXPECT_EQ(r, 1);                       /* arrived back at the capture point, not past the unwind */
    EXPECT_EQ(g_urax, 0x99);               /* ReturnValue delivered in Rax */
    if (expect_handler) {
        EXPECT_EQ(g_h.calls, 1);
        CHECKF(g_h.flags[0] & EXCEPTION_UNWINDING, "flags 0x%lx", g_h.flags[0]);
        CHECK(!(g_h.flags[0] & EXCEPTION_EXIT_UNWIND));
        CHECK(!(g_h.flags[0] & EXCEPTION_TARGET_UNWIND));
        EXPECT_EQ(g_h.code[0], use_rec ? 0xE0A00061 : EED_STATUS_UNWIND);
        EXPECT_EQ(g_h.image_base[0], b);
        EXPECT_EQ(g_h.fentry[0], &j.rf[0]);
        EXPECT_EQ(g_h.target_ip[0], tip);
        CHECK(g_h.control_pc[0] >= b && g_h.control_pc[0] <= b + JIT_CODE_LEN);
    }
    if (use_rec) CHECK(g_urec.ExceptionFlags & EXCEPTION_UNWINDING);
    PDELFT(j.rf);
    jit_free(&j);
}
TEST(eed_unwindex_null_record_calls_handler) { run_unwind(0, 0, 1); }
TEST(eed_unwindex_custom_record_calls_handler) { run_unwind(0, 1, 1); }
TEST(eed_unwind_legacy_null_record_calls_handler) { run_unwind(1, 0, 1); }
TEST(eed_unwind_legacy_custom_record_calls_handler) { run_unwind(1, 1, 1); }

/* ---- RtlRestoreContext ---- */
static __attribute__((noinline, optimize("O0"))) int restore_same(void) {
    CONTEXT c; volatile int stage = 0;
    PCAP(&c);
    if (stage == 0) { stage = 1; PRESTORE(&c, NULL); stage = 99; return -1; }
    return stage;
}
TEST(eed_restore_context_returns_to_capture_point) {
    REQUIRE(PCAP); REQUIRE(PRESTORE);
    EXPECT_EQ(restore_same(), 1);
}
__asm__(
    ".text\n"
    ".globl eed_rr\need_rr:\n"
    "  pushq %rbx\n  pushq %rbp\n  pushq %rsi\n  pushq %rdi\n  pushq %r12\n  pushq %r13\n  pushq %r14\n  pushq %r15\n"
    "  movq %rsp, eed_rr_savedrsp(%rip)\n"
    "  leaq eed_rr_back(%rip), %rax\n  movq %rax, eed_rr_resume(%rip)\n"
    "  subq $40, %rsp\n"
    "  xorl %edx, %edx\n"
    "  call *%r8\n"
    "  int3\n"
    "eed_rr_back:\n"
    "  movq eed_rr_savedrsp(%rip), %rsp\n"
    "  popq %r15\n  popq %r14\n  popq %r13\n  popq %r12\n  popq %rdi\n  popq %rsi\n  popq %rbp\n  popq %rbx\n"
    "  movl $1, %eax\n  ret\n"
    ".globl eed_rr_target\need_rr_target:\n"
    "  movq %rax, eed_rr_rax(%rip)\n  movq %rbx, eed_rr_rbx(%rip)\n  movq %r12, eed_rr_r12(%rip)\n  movq %rsp, eed_rr_rsp(%rip)\n"
    "  jmp *eed_rr_resume(%rip)\n"
    ".data\n"
    ".globl eed_rr_savedrsp\need_rr_savedrsp: .quad 0\n"
    ".globl eed_rr_resume\need_rr_resume: .quad 0\n"
    ".globl eed_rr_rax\need_rr_rax: .quad 0\n"
    ".globl eed_rr_rbx\need_rr_rbx: .quad 0\n"
    ".globl eed_rr_r12\need_rr_r12: .quad 0\n"
    ".globl eed_rr_rsp\need_rr_rsp: .quad 0\n"
    ".text\n"
);
extern int eed_rr(CONTEXT *c, void *unused, void *restore_fn);
extern char eed_rr_target[];
extern volatile DWORD64 eed_rr_rax, eed_rr_rbx, eed_rr_r12, eed_rr_rsp;
TEST(eed_restore_context_to_modified_rip) {
    REQUIRE(PRESTORE);
    static char stk[512] __attribute__((aligned(16)));
    CONTEXT c; memset(&c, 0, sizeof c);
    c.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    c.Rip = (DWORD64)(ULONG_PTR)eed_rr_target;
    c.Rsp = (DWORD64)(ULONG_PTR)(stk + 256) - 8;
    c.Rax = 0xA1A1; c.Rbx = 0xB2B2; c.R12 = 0xC3C3;
    c.EFlags = 0x202; c.SegCs = 0x33; c.SegSs = 0x2B;
    eed_rr_rax = eed_rr_rbx = eed_rr_r12 = eed_rr_rsp = 0;
    int r = eed_rr(&c, NULL, (void *)PRESTORE);
    EXPECT_EQ(r, 1);
    EXPECT_EQ(eed_rr_rax, 0xA1A1);
    EXPECT_EQ(eed_rr_rbx, 0xB2B2);
    EXPECT_EQ(eed_rr_r12, 0xC3C3);
    EXPECT_EQ(eed_rr_rsp, c.Rsp);
}
TEST(eed_restore_context_only_flagged_parts) {
    REQUIRE(PRESTORE);
    static char stk[512] __attribute__((aligned(16)));
    CONTEXT c; memset(&c, 0, sizeof c);
    c.ContextFlags = CONTEXT_CONTROL;          /* integer registers NOT restored */
    c.Rip = (DWORD64)(ULONG_PTR)eed_rr_target;
    c.Rsp = (DWORD64)(ULONG_PTR)(stk + 256) - 8;
    c.Rax = 0xDEAD; c.Rbx = 0xBEEF; c.R12 = 0xF00D;
    c.EFlags = 0x202; c.SegCs = 0x33; c.SegSs = 0x2B;
    eed_rr_rax = eed_rr_rbx = eed_rr_r12 = eed_rr_rsp = 0;
    int r = eed_rr(&c, NULL, (void *)PRESTORE);
    EXPECT_EQ(r, 1);
    EXPECT_EQ(eed_rr_rsp, c.Rsp);
    CHECK(eed_rr_rbx != 0xBEEF);
    CHECK(eed_rr_r12 != 0xF00D);
}
static __attribute__((noinline, optimize("O0"))) int restore_many(int n) {
    CONTEXT c; volatile int cnt = 0;
    PCAP(&c);
    cnt = cnt + 1;
    if (cnt < n) PRESTORE(&c, NULL);
    return cnt;
}
TEST(eed_restore_context_loop) {
    REQUIRE(PCAP); REQUIRE(PRESTORE);
    /* cnt is a stack local that survives the restore: the loop runs n times */
    EXPECT_EQ(restore_many(50), 50);
}

/* ---- initial context of a new suspended thread + debug registers ---- */
TEST(eed_ctx_new_thread_initial_context) {
    REQUIRE(U(CreateThread)); REQUIRE(U(GetThreadContext));
    HANDLE h = U(CreateThread)(NULL, 0, th_ctx, &(CONTEXT){0}, CREATE_SUSPENDED, NULL);
    CHECK(h != NULL); if (!h) return;
    CONTEXT c; memset(&c, 0, sizeof c); c.ContextFlags = CONTEXT_FULL;
    BOOL ok = U(GetThreadContext)(h, &c);
    CHECK(ok);
    if (ok) {
        CHECKF(module_of((void *)(ULONG_PTR)c.Rip) == t_ntdll(), "Rip %llx", (unsigned long long)c.Rip);
        t_note("new thread: Rip=%llx Rcx=%llx Rdx=%llx", (unsigned long long)c.Rip, (unsigned long long)c.Rcx, (unsigned long long)c.Rdx);
        ULONG_PTR lo, hi;
        CHECK(c.Rsp != 0);
        CHECK(c.SegCs == 0x33);
    }
    TerminateThread(h, 0);
    CloseHandle(h);
}
TEST(eed_ctx_get_set_roundtrip_suspended_thread) {
    REQUIRE(U(CreateThread)); REQUIRE(U(GetThreadContext)); REQUIRE(U(SetThreadContext));
    HANDLE h = U(CreateThread)(NULL, 0, th_ctx, &(CONTEXT){0}, CREATE_SUSPENDED, NULL);
    CHECK(h != NULL); if (!h) return;
    CONTEXT c; memset(&c, 0, sizeof c); c.ContextFlags = CONTEXT_FULL;
    CHECK(U(GetThreadContext)(h, &c));
    c.Rbx = 0x1234567; c.R12 = 0x7654321;
    CHECK(U(SetThreadContext)(h, &c));
    CONTEXT d; memset(&d, 0, sizeof d); d.ContextFlags = CONTEXT_FULL;
    CHECK(U(GetThreadContext)(h, &d));
    EXPECT_EQ(d.Rbx, 0x1234567); EXPECT_EQ(d.R12, 0x7654321);
    EXPECT_EQ(d.Rip, c.Rip); EXPECT_EQ(d.Rsp, c.Rsp);
    TerminateThread(h, 0);
    CloseHandle(h);
}
TEST(eed_ctx_get_thread_context_invalid_handle) {
    REQUIRE(U(GetThreadContext)); REQUIRE(U(SetThreadContext));
    CONTEXT c; memset(&c, 0, sizeof c); c.ContextFlags = CONTEXT_FULL;
    SetLastError(0);
    CHECK(!U(GetThreadContext)(NULL, &c)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SetLastError(0);
    CHECK(!U(GetThreadContext)(INVALID_HANDLE_VALUE, &c));
    t_note("GetThreadContext(INVALID_HANDLE_VALUE) err %lu", GetLastError());
    SetLastError(0);
    CHECK(!U(SetThreadContext)(NULL, &c)); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(eed_ctx_get_thread_context_needs_access) {
    REQUIRE(U(CreateThread)); REQUIRE(U(GetThreadContext)); REQUIRE(U(OpenThread));
    HANDLE h = U(CreateThread)(NULL, 0, th_ctx, &(CONTEXT){0}, CREATE_SUSPENDED, NULL);
    CHECK(h != NULL); if (!h) return;
    DWORD tid = GetThreadId(h);
    HANDLE ro = U(OpenThread)(THREAD_QUERY_LIMITED_INFORMATION, FALSE, tid);
    CHECK(ro != NULL);
    if (ro) {
        CONTEXT c; memset(&c, 0, sizeof c); c.ContextFlags = CONTEXT_CONTROL;
        SetLastError(0);
        CHECK(!U(GetThreadContext)(ro, &c)); EXPECT_ERR(ERROR_ACCESS_DENIED);
        CloseHandle(ro);
    }
    TerminateThread(h, 0);
    CloseHandle(h);
}

/* hardware breakpoints via the debug registers of a thread we created */
static volatile DWORD g_hw_var;
static volatile LONG g_hw_hits; static volatile DWORD64 g_hw_dr6, g_hw_rip; static volatile DWORD g_hw_code; static volatile int g_hw_exec_mode;
static LONG CALLBACK veh_hw(PEXCEPTION_POINTERS ep) {
    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP) return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedIncrement(&g_hw_hits) > 5) { ep->ContextRecord->Dr7 = 0; return EXCEPTION_CONTINUE_EXECUTION; }
    g_hw_dr6 = ep->ContextRecord->Dr6; g_hw_rip = ep->ContextRecord->Rip; g_hw_code = ep->ExceptionRecord->ExceptionCode;
    if (g_hw_exec_mode) ep->ContextRecord->EFlags |= 0x10000;     /* resume flag, else the instruction breakpoint re-fires */
    return EXCEPTION_CONTINUE_EXECUTION;
}
static void hw_run(int exec) {
    REQUIRE(U(CreateThread)); REQUIRE(U(SetThreadContext)); REQUIRE(U(GetThreadContext)); REQUIRE(U(AddVectoredExceptionHandler));
    if (t_is_wine()) { t_skip("debug registers not supported under wine"); return; }
    g_hw_hits = 0; g_hw_dr6 = 0; g_hw_var = 0; g_hw_exec_mode = exec;
    PVOID hv = U(AddVectoredExceptionHandler)(1, veh_hw);
    HANDLE h = U(CreateThread)(NULL, 0, exec ? (LPTHREAD_START_ROUTINE)(void *)eed_hw_exec : (LPTHREAD_START_ROUTINE)(void *)eed_write_dword,
                                (LPVOID)&g_hw_var, CREATE_SUSPENDED, NULL);
    CHECK(h != NULL);
    if (h) {
        CONTEXT c; memset(&c, 0, sizeof c); c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        c.Dr0 = exec ? (DWORD64)(ULONG_PTR)eed_hw_exec : (DWORD64)(ULONG_PTR)&g_hw_var;
        c.Dr7 = exec ? 0x1 : (0x1 | (1u << 16) | (3u << 18));      /* L0, RW0=exec or write, LEN0=1 or 4 bytes */
        BOOL ok = U(SetThreadContext)(h, &c);
        CHECK(ok);
        CONTEXT d; memset(&d, 0, sizeof d); d.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        CHECK(U(GetThreadContext)(h, &d));
        EXPECT_EQ(d.Dr0, c.Dr0);
        CHECK((d.Dr7 & 1) == 1);
        ResumeThread(h);
        WaitForSingleObject(h, 5000);
        CloseHandle(h);
    }
    U(RemoveVectoredExceptionHandler)(hv);
    if (g_hw_hits == 0) { t_note("no hardware breakpoint event (debug registers unavailable on this host?)"); return; }
    EXPECT_EQ(g_hw_hits, 1);
    EXPECT_EQ(g_hw_code, EXCEPTION_SINGLE_STEP);
    CHECK((g_hw_dr6 & 1) == 1);
    if (exec) { EXPECT_EQ(g_hw_rip, (DWORD64)(ULONG_PTR)eed_hw_exec); }
    else { EXPECT_EQ(g_hw_rip, (DWORD64)(ULONG_PTR)eed_write_dword_after); EXPECT_EQ(g_hw_var, 0x77); }
}
TEST(eed_hwbp_data_write_breakpoint) { hw_run(0); }
TEST(eed_hwbp_execute_breakpoint) { hw_run(1); }
