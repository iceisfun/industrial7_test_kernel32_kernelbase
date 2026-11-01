/* RaiseException, vectored exception/continue handlers, hardware-generated exceptions. */
#include "eed.h"
#include "eed_asm.h"

__asm__(
    ".text\n"
    ".globl eed_av_write\need_av_write:\n  movb $1, (%rcx)\n.globl eed_av_write_after\need_av_write_after:\n  ret\n"
    ".globl eed_av_read\need_av_read:\n  movb (%rcx), %al\n.globl eed_av_read_after\need_av_read_after:\n  ret\n"
    ".globl eed_call_ptr\need_call_ptr:\n  subq $40, %rsp\n  callq *%rcx\n.globl eed_call_ptr_after\need_call_ptr_after:\n  addq $40, %rsp\n  ret\n"
    ".globl eed_div0\need_div0:\n  xorl %edx, %edx\n  movl $1, %eax\n  xorl %ecx, %ecx\n.globl eed_div0_insn\need_div0_insn:\n  divl %ecx\n.globl eed_div0_after\need_div0_after:\n  ret\n"
    ".globl eed_idiv_ovf\need_idiv_ovf:\n  movl $0x80000000, %eax\n  cltd\n  movl $-1, %ecx\n.globl eed_idiv_ovf_insn\need_idiv_ovf_insn:\n  idivl %ecx\n.globl eed_idiv_ovf_after\need_idiv_ovf_after:\n  ret\n"
    ".globl eed_ud2\need_ud2:\n.globl eed_ud2_insn\need_ud2_insn:\n  ud2\n.globl eed_ud2_after\need_ud2_after:\n  ret\n"
    ".globl eed_int3\need_int3:\n.globl eed_int3_insn\need_int3_insn:\n  int3\n.globl eed_int3_after\need_int3_after:\n  ret\n"
    ".globl eed_cli\need_cli:\n.globl eed_cli_insn\need_cli_insn:\n  cli\n.globl eed_cli_after\need_cli_after:\n  ret\n"
    ".globl eed_hlt\need_hlt:\n.globl eed_hlt_insn\need_hlt_insn:\n  hlt\n.globl eed_hlt_after\need_hlt_after:\n  ret\n"
    ".globl eed_movaps\need_movaps:\n.globl eed_movaps_insn\need_movaps_insn:\n  movaps (%rcx), %xmm0\n.globl eed_movaps_after\need_movaps_after:\n  ret\n"
    ".globl eed_tf\need_tf:\n  pushfq\n  orq $0x100, (%rsp)\n  popfq\n  nop\n  nop\n  nop\n  nop\n  nop\n  nop\n  ret\n"
    ".globl eed_write_dword\need_write_dword:\n  movl $0x77, (%rcx)\n.globl eed_write_dword_after\need_write_dword_after:\n  nop\n  ret\n"
    ".globl eed_hw_exec\need_hw_exec:\n  nop\n  nop\n  ret\n"
);

typedef struct {
    DWORD code, flags, nparams, tid;
    PVOID addr, chain;
    ULONG_PTR info[15];
    DWORD64 rip, rsp, rax, dr6;
    DWORD eflags, ctxflags;
} cap_t;

static cap_t g_cap[256];
static volatile LONG g_ncap;
static volatile int g_mode;               /* 0: only 0xE0A0xxxx codes; 1: fix up Rip; 2: continue any code */
static volatile DWORD64 g_fix_rip, g_fix_rax, g_fix_rsp_add;
static volatile int g_fix_set_rax;
static volatile LONG g_veh_calls;

static void capture(PEXCEPTION_POINTERS ep) {
    LONG i = InterlockedIncrement(&g_ncap) - 1;
    if (i >= 256) return;
    cap_t *c = &g_cap[i];
    PEXCEPTION_RECORD r = ep->ExceptionRecord;
    c->code = r->ExceptionCode; c->flags = r->ExceptionFlags; c->nparams = r->NumberParameters;
    c->addr = r->ExceptionAddress; c->chain = r->ExceptionRecord; c->tid = GetCurrentThreadId();
    for (int k = 0; k < 15; k++) c->info[k] = r->ExceptionInformation[k];
    c->rip = ep->ContextRecord->Rip; c->rsp = ep->ContextRecord->Rsp; c->rax = ep->ContextRecord->Rax;
    c->eflags = ep->ContextRecord->EFlags; c->ctxflags = ep->ContextRecord->ContextFlags; c->dr6 = ep->ContextRecord->Dr6;
}
static LONG CALLBACK veh_gen(PEXCEPTION_POINTERS ep) {
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (g_mode == 0 && (code & 0xFFFF0000) != 0xE0A00000) return EXCEPTION_CONTINUE_SEARCH;
    InterlockedIncrement(&g_veh_calls);
    capture(ep);
    if (g_mode == 1) {
        ep->ContextRecord->Rip = g_fix_rip;
        ep->ContextRecord->Rsp += g_fix_rsp_add;
        if (g_fix_set_rax) ep->ContextRecord->Rax = g_fix_rax;
        g_mode = 0; g_fix_rsp_add = 0; g_fix_set_rax = 0;
    }
    return EXCEPTION_CONTINUE_EXECUTION;
}
static PVOID g_vh;
static void veh_on(void) {
    g_ncap = 0; g_mode = 0; g_veh_calls = 0;
    memset(g_cap, 0, sizeof g_cap);
    g_vh = U(AddVectoredExceptionHandler)(1, veh_gen);
}
static void veh_off(void) { if (g_vh) { U(RemoveVectoredExceptionHandler)(g_vh); g_vh = NULL; } g_mode = 0; }

#define NEEDVEH() REQUIRE(U(AddVectoredExceptionHandler)); REQUIRE(U(RemoveVectoredExceptionHandler)); REQUIRE(U(RaiseException))
#define RAISE U(RaiseException)

TEST(eed_exc_struct_sizes) {
    EXPECT_EQ(sizeof(EXCEPTION_RECORD), 152);
    EXPECT_EQ(sizeof(CONTEXT), 1232);
    EXPECT_EQ(offsetof(EXCEPTION_RECORD, ExceptionInformation), 32);
    EXPECT_EQ(EXCEPTION_MAXIMUM_PARAMETERS, 15);
    EXPECT_EQ(offsetof(CONTEXT, Rip), 0xF8);
    EXPECT_EQ(offsetof(CONTEXT, Rsp), 0x98);
    EXPECT_EQ(offsetof(CONTEXT, EFlags), 0x44);
}
TEST(eed_exc_raise_basic_record) {
    NEEDVEH();
    veh_on();
    ULONG_PTR a[3] = { 0x11, 0x22, 0x33 };
    RAISE(0xE0A00001, 0, 3, a);
    veh_off();
    EXPECT_EQ(g_ncap, 1);
    EXPECT_EQ(g_cap[0].code, 0xE0A00001);
    EXPECT_EQ(g_cap[0].flags, 0);
    EXPECT_EQ(g_cap[0].nparams, 3);
    for (int i = 0; i < 3; i++) EXPECT_EQ(g_cap[0].info[i], a[i]);
    EXPECT_EQ(g_cap[0].chain, 0);
}
TEST(eed_exc_raise_exception_address_is_raiseexception) {
    NEEDVEH();
    veh_on();
    RAISE(0xE0A00002, 0, 0, NULL);
    veh_off();
    EXPECT_EQ(g_ncap, 1);
    /* decomp: RaiseException stores itself, but RtlRaiseException then overwrites ExceptionAddress with the
       unwound return address, i.e. a point inside RaiseException (KERNELBASE!RaiseException+N) */
    {   ULONG_PTR a = (ULONG_PTR)g_cap[0].addr, f = (ULONG_PTR)UB(RaiseException);
        CHECKF(a > f && a < f + 0x100, "addr %p RaiseException %p", g_cap[0].addr, (void *)f); }
}
TEST(eed_exc_raise_returns_when_continued) {
    NEEDVEH();
    volatile int after = 0;
    veh_on();
    RAISE(0xE0A00003, 0, 0, NULL);
    after = 1;
    veh_off();
    EXPECT_EQ(after, 1);
}
TEST(eed_exc_raise_arg_counts) {
    NEEDVEH();
    ULONG_PTR a[40];
    for (int i = 0; i < 40; i++) a[i] = 0x1000 + i;
    static const DWORD counts[] = { 0, 1, 2, 3, 7, 14, 15, 16, 17, 20, 32, 40 };
    for (size_t k = 0; k < sizeof counts / sizeof *counts; k++) {
        veh_on();
        RAISE(0xE0A00004, 0, counts[k], a);
        veh_off();
        DWORD want = counts[k] > 15 ? 15 : counts[k];
        CHECKF(g_ncap == 1 && g_cap[0].nparams == want, "n=%lu got nparams=%lu", counts[k], g_cap[0].nparams);
        for (DWORD i = 0; i < want; i++) if (g_cap[0].info[i] != a[i]) { CHECKF(0, "n=%lu info[%lu]", counts[k], i); break; }
    }
}
TEST(eed_exc_raise_null_args_ignores_count) {
    NEEDVEH();
    static const DWORD counts[] = { 0, 1, 5, 15, 16, 1000 };
    for (size_t k = 0; k < sizeof counts / sizeof *counts; k++) {
        veh_on();
        RAISE(0xE0A00005, 0, counts[k], NULL);
        veh_off();
        CHECKF(g_ncap == 1 && g_cap[0].nparams == 0, "n=%lu got %lu", counts[k], g_cap[0].nparams);
    }
}
TEST(eed_exc_raise_zero_count_ignores_args) {
    NEEDVEH();
    ULONG_PTR a[1] = { 5 };
    veh_on();
    RAISE(0xE0A00006, 0, 0, a);
    veh_off();
    EXPECT_EQ(g_cap[0].nparams, 0);
}
TEST(eed_exc_raise_flags_masked_to_noncontinuable_bit) {
    NEEDVEH();
    /* only bit 0 (EXCEPTION_NONCONTINUABLE) survives; noncontinuable cases are tested separately with longjmp */
    static const DWORD flags[] = { 0, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x80000000, 0xFFFFFFFE, 0x0000FFFE };
    for (size_t k = 0; k < sizeof flags / sizeof *flags; k++) {
        veh_on();
        RAISE(0xE0A00007, flags[k], 0, NULL);
        veh_off();
        CHECKF(g_ncap == 1 && g_cap[0].flags == 0, "flags 0x%lx -> record flags 0x%lx", flags[k], g_cap[0].flags);
    }
}
TEST(eed_exc_raise_context_flags_and_stack) {
    NEEDVEH();
    veh_on();
    RAISE(0xE0A00008, 0, 0, NULL);
    veh_off();
    CHECK((g_cap[0].ctxflags & CONTEXT_CONTROL) == CONTEXT_CONTROL);
    CHECK((g_cap[0].ctxflags & CONTEXT_INTEGER) == CONTEXT_INTEGER);
    ULONG_PTR lo = 0, hi = 0;
    GetCurrentThreadStackLimits(&lo, &hi);
    CHECKF(g_cap[0].rsp >= lo && g_cap[0].rsp < hi, "rsp %llx not in [%llx,%llx)", (unsigned long long)g_cap[0].rsp, (unsigned long long)lo, (unsigned long long)hi);
}
TEST(eed_exc_raise_context_rip_in_ntdll_or_k) {
    NEEDVEH();
    veh_on();
    RAISE(0xE0A00009, 0, 0, NULL);
    veh_off();
    HMODULE m = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)(ULONG_PTR)g_cap[0].rip, &m);
    CHECK(m == t_ntdll() || m == t_kernelbase());
}
TEST(eed_exc_raise_custom_codes) {
    NEEDVEH();
    /* any code with the 0xE0A0 prefix is handled by veh_gen in mode 0 */
    for (DWORD i = 0; i < 64; i++) {
        veh_on();
        RAISE(0xE0A00000 | (i * 997 & 0xFFFF), 0, 0, NULL);
        veh_off();
        if (g_ncap != 1 || g_cap[0].code != (0xE0A00000 | (i * 997 & 0xFFFF))) { CHECKF(0, "i=%lu", i); break; }
    }
}
TEST(eed_exc_raise_system_codes_mode2) {
    NEEDVEH();
    static const DWORD codes[] = { 0xC0000005, 0xC0000094, 0xC000001D, 0xC00000FD, 0x80000001, 0x80000003, 0x80000004, 0xC0000096, 0xC0000095, 0x40010006, 0xC000008C, 0xC000008D, 0xC000008E, 0xC0000090, 0xC0000091, 0xC0000092, 0xC0000093, 0xC0000025, 0xC0000026, 0xC00000FD, 0xC0000194, 0xC0000420 };
    for (size_t k = 0; k < sizeof codes / sizeof *codes; k++) {
        veh_on(); g_mode = 2;
        ULONG_PTR a[2] = { 1, 0x1234 };
        RAISE(codes[k], 0, 2, a);
        veh_off();
        CHECKF(g_ncap == 1 && g_cap[0].code == codes[k] && g_cap[0].nparams == 2 && g_cap[0].info[1] == 0x1234, "code 0x%lx ncap=%ld", codes[k], (long)g_ncap);
    }
}
TEST(eed_exc_raise_repeat_many) {
    NEEDVEH();
    veh_on();
    for (int i = 0; i < 1000; i++) RAISE(0xE0A0000A, 0, 0, NULL);
    veh_off();
    CHECKF(g_veh_calls == 1000, "calls %ld", (long)g_veh_calls);
}
static void rec_raise(int d) { if (d == 0) { RAISE(0xE0A0000B, 0, 0, NULL); return; } volatile char pad[200]; pad[0] = 1; rec_raise(d - 1); pad[1] = pad[0]; }
TEST(eed_exc_raise_deep_stack) {
    NEEDVEH();
    veh_on();
    rec_raise(100);
    veh_off();
    EXPECT_EQ(g_ncap, 1);
}
static DWORD WINAPI th_raise(LPVOID p) {
    ULONG_PTR a[1] = { (ULONG_PTR)p };
    RAISE(0xE0A0000C, 0, 1, a);
    return 0;
}
TEST(eed_exc_raise_in_other_thread) {
    NEEDVEH(); REQUIRE(U(CreateThread));
    veh_on();
    DWORD tid = 0;
    HANDLE h = U(CreateThread)(NULL, 0, th_raise, (LPVOID)0x99, 0, &tid);
    CHECK(h != NULL);
    if (h) { WaitForSingleObject(h, 5000); CloseHandle(h); }
    veh_off();
    EXPECT_EQ(g_ncap, 1);
    EXPECT_EQ(g_cap[0].tid, tid);
    EXPECT_EQ(g_cap[0].info[0], 0x99);
}
static DWORD WINAPI th_raise_many(LPVOID p) { for (int i = 0; i < 200; i++) RAISE(0xE0A0000D, 0, 0, NULL); return 0; }
TEST(eed_exc_raise_concurrent_threads) {
    NEEDVEH(); REQUIRE(U(CreateThread));
    veh_on();
    HANDLE h[4];
    for (int i = 0; i < 4; i++) h[i] = U(CreateThread)(NULL, 0, th_raise_many, NULL, 0, NULL);
    for (int i = 0; i < 4; i++) if (h[i]) { WaitForSingleObject(h[i], 10000); CloseHandle(h[i]); }
    veh_off();
    CHECKF(g_veh_calls == 800, "calls %ld", (long)g_veh_calls);
}
TEST(eed_exc_raise_kernel32_forward_identity) {
    FARPROC a = (FARPROC)UK(RaiseException), b = (FARPROC)UB(RaiseException);
    REQUIRE(a); REQUIRE(b);
    if (a != b) t_note("kernel32!RaiseException %p != kernelbase %p", a, b);
}

/* ---- noncontinuable: continuing yields STATUS_NONCONTINUABLE_EXCEPTION ---- */
static jmp_buf g_nc_jb;
static volatile int g_nc_armed;
static cap_t g_nc_first, g_nc_second;
static LONG CALLBACK veh_nc(PEXCEPTION_POINTERS ep) {
    DWORD c = ep->ExceptionRecord->ExceptionCode;
    if (c == 0xE0A00020) {
        capture(ep);
        return EXCEPTION_CONTINUE_EXECUTION;    /* illegal for a noncontinuable exception */
    }
    if (c == STATUS_NONCONTINUABLE_EXCEPTION && g_nc_armed) {
        capture(ep);
        g_nc_armed = 0;
        longjmp(g_nc_jb, 1);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
TEST(eed_exc_noncontinuable_continue_raises_c0000025) {
    NEEDVEH();
    PVOID h = U(AddVectoredExceptionHandler)(1, veh_nc);
    CHECK(h != NULL);
    g_ncap = 0; memset(g_cap, 0, sizeof g_cap);
    volatile int reached = 0;
    if (!setjmp(g_nc_jb)) {
        g_nc_armed = 1;
        RAISE(0xE0A00020, EXCEPTION_NONCONTINUABLE, 0, NULL);
        reached = 1;
    }
    g_nc_armed = 0;
    U(RemoveVectoredExceptionHandler)(h);
    /* Windows 10: a vectored handler returning EXCEPTION_CONTINUE_EXECUTION ends RtlDispatchException immediately and the
       thread resumes after RaiseException. STATUS_NONCONTINUABLE_EXCEPTION is only raised when a frame-based (SEH) handler
       returns ExceptionContinueExecution for a noncontinuable record. */
    EXPECT_EQ(reached, 1);
    EXPECT_EQ(g_ncap, 1);
    EXPECT_EQ(g_cap[0].code, 0xE0A00020);
    EXPECT_EQ(g_cap[0].flags & EXCEPTION_NONCONTINUABLE, EXCEPTION_NONCONTINUABLE);
}
TEST(eed_exc_noncontinuable_flag_visible_to_handler) {
    NEEDVEH();
    PVOID h = U(AddVectoredExceptionHandler)(1, veh_nc);
    g_ncap = 0; memset(g_cap, 0, sizeof g_cap);
    if (!setjmp(g_nc_jb)) { g_nc_armed = 1; RAISE(0xE0A00020, 0xFFFFFFFF, 0, NULL); }
    g_nc_armed = 0;
    U(RemoveVectoredExceptionHandler)(h);
    EXPECT_EQ(g_cap[0].flags, EXCEPTION_NONCONTINUABLE);
}

/* ---- vectored handler registration ---- */
static LONG CALLBACK veh_nop(PEXCEPTION_POINTERS ep) { return EXCEPTION_CONTINUE_SEARCH; }
TEST(eed_veh_add_remove) {
    REQUIRE(U(AddVectoredExceptionHandler)); REQUIRE(U(RemoveVectoredExceptionHandler));
    PVOID h = U(AddVectoredExceptionHandler)(0, veh_nop);
    CHECK(h != NULL);
    ULONG rr = U(RemoveVectoredExceptionHandler)(h);
    EXPECT_NE(rr, 0);
}
TEST(eed_veh_remove_twice) {
    REQUIRE(U(AddVectoredExceptionHandler)); REQUIRE(U(RemoveVectoredExceptionHandler));
    PVOID h = U(AddVectoredExceptionHandler)(1, veh_nop);
    CHECK(U(RemoveVectoredExceptionHandler)(h) != 0);
    EXPECT_EQ(U(RemoveVectoredExceptionHandler)(h), 0);
}
TEST(eed_veh_remove_bogus_handle) {
    REQUIRE(U(RemoveVectoredExceptionHandler));
    EXPECT_EQ(U(RemoveVectoredExceptionHandler)(NULL), 0);
    EXPECT_EQ(U(RemoveVectoredExceptionHandler)((PVOID)(ULONG_PTR)0x1234), 0);
    EXPECT_EQ(U(RemoveVectoredExceptionHandler)((PVOID)(ULONG_PTR)-1), 0);
}
TEST(eed_veh_handles_are_distinct) {
    REQUIRE(U(AddVectoredExceptionHandler)); REQUIRE(U(RemoveVectoredExceptionHandler));
    PVOID a = U(AddVectoredExceptionHandler)(0, veh_nop), b = U(AddVectoredExceptionHandler)(0, veh_nop), c = U(AddVectoredExceptionHandler)(1, veh_nop);
    CHECK(a && b && c); CHECK(a != b && b != c && a != c);
    CHECK(U(RemoveVectoredExceptionHandler)(b));
    CHECK(U(RemoveVectoredExceptionHandler)(a));
    CHECK(U(RemoveVectoredExceptionHandler)(c));
}
TEST(eed_veh_handle_is_not_function_pointer) {
    REQUIRE(U(AddVectoredExceptionHandler)); REQUIRE(U(RemoveVectoredExceptionHandler));
    PVOID a = U(AddVectoredExceptionHandler)(0, veh_nop);
    CHECK(a != (PVOID)veh_nop);
    U(RemoveVectoredExceptionHandler)(a);
}
TEST(eed_veh_many_handles_churn) {
    REQUIRE(U(AddVectoredExceptionHandler)); REQUIRE(U(RemoveVectoredExceptionHandler));
    PVOID h[200];
    for (int i = 0; i < 200; i++) h[i] = U(AddVectoredExceptionHandler)(i & 1, veh_nop);
    int bad = 0;
    for (int i = 0; i < 200; i++) if (!h[i] || !U(RemoveVectoredExceptionHandler)(h[i])) bad++;
    EXPECT_EQ(bad, 0);
}

static char g_ord[64]; static volatile int g_on;
static void ord_add(char c) { if (g_on < 63) { g_ord[g_on++] = c; g_ord[g_on] = 0; } }
#define ORDH(name, ch, ret) static LONG CALLBACK name(PEXCEPTION_POINTERS ep) { \
    if (ep->ExceptionRecord->ExceptionCode != 0xE0A00030) return EXCEPTION_CONTINUE_SEARCH; \
    ord_add(ch); return ret; }
ORDH(ord_a, 'a', EXCEPTION_CONTINUE_SEARCH)
ORDH(ord_b, 'b', EXCEPTION_CONTINUE_SEARCH)
ORDH(ord_c, 'c', EXCEPTION_CONTINUE_SEARCH)
ORDH(ord_d, 'd', EXCEPTION_CONTINUE_SEARCH)
ORDH(ord_stop, 'S', EXCEPTION_CONTINUE_EXECUTION)
ORDH(ord_late, 'L', EXCEPTION_CONTINUE_EXECUTION)
TEST(eed_veh_order_first_and_last) {
    NEEDVEH();
    g_on = 0; g_ord[0] = 0;
    PVOID l1 = U(AddVectoredExceptionHandler)(0, ord_a);
    PVOID l2 = U(AddVectoredExceptionHandler)(0, ord_b);
    PVOID f1 = U(AddVectoredExceptionHandler)(1, ord_c);
    PVOID f2 = U(AddVectoredExceptionHandler)(1, ord_d);
    PVOID fin = U(AddVectoredExceptionHandler)(0, ord_stop);
    RAISE(0xE0A00030, 0, 0, NULL);
    U(RemoveVectoredExceptionHandler)(l1); U(RemoveVectoredExceptionHandler)(l2); U(RemoveVectoredExceptionHandler)(f1);
    U(RemoveVectoredExceptionHandler)(f2); U(RemoveVectoredExceptionHandler)(fin);
    EXPECT_STR(g_ord, "dcabS");
}
TEST(eed_veh_continue_execution_stops_chain) {
    NEEDVEH();
    g_on = 0; g_ord[0] = 0;
    PVOID a = U(AddVectoredExceptionHandler)(0, ord_a);
    PVOID s = U(AddVectoredExceptionHandler)(0, ord_stop);
    PVOID b = U(AddVectoredExceptionHandler)(0, ord_b);
    RAISE(0xE0A00030, 0, 0, NULL);
    U(RemoveVectoredExceptionHandler)(a); U(RemoveVectoredExceptionHandler)(s); U(RemoveVectoredExceptionHandler)(b);
    EXPECT_STR(g_ord, "aS");
}
TEST(eed_veh_removed_handler_not_called) {
    NEEDVEH();
    g_on = 0; g_ord[0] = 0;
    PVOID a = U(AddVectoredExceptionHandler)(0, ord_a);
    PVOID s = U(AddVectoredExceptionHandler)(0, ord_stop);
    U(RemoveVectoredExceptionHandler)(a);
    RAISE(0xE0A00030, 0, 0, NULL);
    U(RemoveVectoredExceptionHandler)(s);
    EXPECT_STR(g_ord, "S");
}
TEST(eed_veh_same_function_twice_is_called_twice) {
    NEEDVEH();
    g_on = 0; g_ord[0] = 0;
    PVOID a = U(AddVectoredExceptionHandler)(0, ord_a);
    PVOID a2 = U(AddVectoredExceptionHandler)(0, ord_a);
    PVOID s = U(AddVectoredExceptionHandler)(0, ord_stop);
    RAISE(0xE0A00030, 0, 0, NULL);
    U(RemoveVectoredExceptionHandler)(a); U(RemoveVectoredExceptionHandler)(a2); U(RemoveVectoredExceptionHandler)(s);
    EXPECT_STR(g_ord, "aaS");
}
TEST(eed_veh_first_flag_nonzero_values_mean_first) {
    NEEDVEH();
    g_on = 0; g_ord[0] = 0;
    PVOID s = U(AddVectoredExceptionHandler)(0, ord_stop);
    PVOID a = U(AddVectoredExceptionHandler)(7, ord_a);
    PVOID b = U(AddVectoredExceptionHandler)(0x80000000, ord_b);
    RAISE(0xE0A00030, 0, 0, NULL);
    U(RemoveVectoredExceptionHandler)(a); U(RemoveVectoredExceptionHandler)(s); U(RemoveVectoredExceptionHandler)(b);
    EXPECT_STR(g_ord, "baS");
}
static PVOID g_self_h;
static LONG CALLBACK veh_self_remove(PEXCEPTION_POINTERS ep) {
    if (ep->ExceptionRecord->ExceptionCode != 0xE0A00031) return EXCEPTION_CONTINUE_SEARCH;
    ord_add('R');
    U(RemoveVectoredExceptionHandler)(g_self_h);
    return EXCEPTION_CONTINUE_EXECUTION;
}
TEST(eed_veh_handler_removes_itself) {
    NEEDVEH();
    g_on = 0; g_ord[0] = 0;
    veh_on();
    g_self_h = U(AddVectoredExceptionHandler)(1, veh_self_remove);
    RAISE(0xE0A00031, 0, 0, NULL);
    RAISE(0xE0A00031, 0, 0, NULL);       /* second raise handled by veh_gen only */
    veh_off();
    EXPECT_STR(g_ord, "R");
    EXPECT_EQ(g_veh_calls, 1);
}
static LONG CALLBACK veh_nested(PEXCEPTION_POINTERS ep) {
    if (ep->ExceptionRecord->ExceptionCode == 0xE0A00032) {
        ord_add('1');
        RAISE(0xE0A00033, 0, 0, NULL);
        ord_add('2');
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (ep->ExceptionRecord->ExceptionCode == 0xE0A00033) { ord_add('N'); return EXCEPTION_CONTINUE_EXECUTION; }
    return EXCEPTION_CONTINUE_SEARCH;
}
TEST(eed_veh_nested_raise_inside_handler) {
    NEEDVEH();
    g_on = 0; g_ord[0] = 0;
    PVOID h = U(AddVectoredExceptionHandler)(1, veh_nested);
    RAISE(0xE0A00032, 0, 0, NULL);
    U(RemoveVectoredExceptionHandler)(h);
    EXPECT_STR(g_ord, "1N2");
}
static LONG CALLBACK veh_modify_arg(PEXCEPTION_POINTERS ep) {
    if (ep->ExceptionRecord->ExceptionCode != 0xE0A00034) return EXCEPTION_CONTINUE_SEARCH;
    ep->ExceptionRecord->ExceptionInformation[0] = 0xABCD;       /* visible to later handlers */
    return EXCEPTION_CONTINUE_SEARCH;
}
TEST(eed_veh_record_modifications_visible_to_next_handler) {
    NEEDVEH();
    veh_on();
    PVOID h = U(AddVectoredExceptionHandler)(1, veh_modify_arg);
    ULONG_PTR a[1] = { 1 };
    RAISE(0xE0A00034, 0, 1, a);
    U(RemoveVectoredExceptionHandler)(h);
    veh_off();
    EXPECT_EQ(g_cap[0].info[0], 0xABCD);
}

/* ---- vectored continue handlers ---- */
static char g_corder[64]; static volatile int g_con;
static void cord_add(char c) { if (g_con < 63) { g_corder[g_con++] = c; g_corder[g_con] = 0; } }
static LONG CALLBACK vch_x(PEXCEPTION_POINTERS ep) { if (ep->ExceptionRecord->ExceptionCode == 0xE0A00040) cord_add('x'); return EXCEPTION_CONTINUE_SEARCH; }
static LONG CALLBACK vch_y(PEXCEPTION_POINTERS ep) { if (ep->ExceptionRecord->ExceptionCode == 0xE0A00040) cord_add('y'); return EXCEPTION_CONTINUE_EXECUTION; }
static LONG CALLBACK vch_z(PEXCEPTION_POINTERS ep) { if (ep->ExceptionRecord->ExceptionCode == 0xE0A00040) cord_add('z'); return EXCEPTION_CONTINUE_SEARCH; }
static LONG CALLBACK veh_v(PEXCEPTION_POINTERS ep) { if (ep->ExceptionRecord->ExceptionCode == 0xE0A00040) { cord_add('V'); return EXCEPTION_CONTINUE_EXECUTION; } return EXCEPTION_CONTINUE_SEARCH; }
TEST(eed_vch_add_remove) {
    REQUIRE(U(AddVectoredContinueHandler)); REQUIRE(U(RemoveVectoredContinueHandler));
    PVOID h = U(AddVectoredContinueHandler)(0, vch_x);
    CHECK(h != NULL);
    CHECK(U(RemoveVectoredContinueHandler)(h) != 0);
    EXPECT_EQ(U(RemoveVectoredContinueHandler)(h), 0);
    EXPECT_EQ(U(RemoveVectoredContinueHandler)(NULL), 0);
}
TEST(eed_vch_called_after_veh_handles) {
    NEEDVEH(); REQUIRE(U(AddVectoredContinueHandler)); REQUIRE(U(RemoveVectoredContinueHandler));
    g_con = 0; g_corder[0] = 0;
    PVOID v = U(AddVectoredExceptionHandler)(1, veh_v);
    PVOID c = U(AddVectoredContinueHandler)(1, vch_x);
    RAISE(0xE0A00040, 0, 0, NULL);
    U(RemoveVectoredContinueHandler)(c); U(RemoveVectoredExceptionHandler)(v);
    EXPECT_STR(g_corder, "Vx");
}
TEST(eed_vch_order_first_and_last) {
    NEEDVEH(); REQUIRE(U(AddVectoredContinueHandler)); REQUIRE(U(RemoveVectoredContinueHandler));
    g_con = 0; g_corder[0] = 0;
    PVOID v = U(AddVectoredExceptionHandler)(1, veh_v);
    PVOID cl = U(AddVectoredContinueHandler)(0, vch_x);
    PVOID cf = U(AddVectoredContinueHandler)(1, vch_z);
    RAISE(0xE0A00040, 0, 0, NULL);
    U(RemoveVectoredContinueHandler)(cl); U(RemoveVectoredContinueHandler)(cf); U(RemoveVectoredExceptionHandler)(v);
    EXPECT_STR(g_corder, "Vzx");
}
TEST(eed_vch_return_value_stops_chain_or_not) {
    NEEDVEH(); REQUIRE(U(AddVectoredContinueHandler)); REQUIRE(U(RemoveVectoredContinueHandler));
    g_con = 0; g_corder[0] = 0;
    PVOID v = U(AddVectoredExceptionHandler)(1, veh_v);
    PVOID c1 = U(AddVectoredContinueHandler)(0, vch_y);
    PVOID c2 = U(AddVectoredContinueHandler)(0, vch_x);
    RAISE(0xE0A00040, 0, 0, NULL);
    U(RemoveVectoredContinueHandler)(c1); U(RemoveVectoredContinueHandler)(c2); U(RemoveVectoredExceptionHandler)(v);
    CHECK(strstr(g_corder, "Vy") == g_corder);
    t_note("continue handler chain after CONTINUE_EXECUTION return: %s", g_corder);
}
TEST(eed_vch_not_called_for_removed) {
    NEEDVEH(); REQUIRE(U(AddVectoredContinueHandler)); REQUIRE(U(RemoveVectoredContinueHandler));
    g_con = 0; g_corder[0] = 0;
    PVOID v = U(AddVectoredExceptionHandler)(1, veh_v);
    PVOID c = U(AddVectoredContinueHandler)(1, vch_x);
    U(RemoveVectoredContinueHandler)(c);
    RAISE(0xE0A00040, 0, 0, NULL);
    U(RemoveVectoredExceptionHandler)(v);
    EXPECT_STR(g_corder, "V");
}
TEST(eed_vch_not_called_without_exception) {
    REQUIRE(U(AddVectoredContinueHandler)); REQUIRE(U(RemoveVectoredContinueHandler));
    g_con = 0; g_corder[0] = 0;
    PVOID c = U(AddVectoredContinueHandler)(1, vch_x);
    SetLastError(1); GetTickCount(); Sleep(1);
    U(RemoveVectoredContinueHandler)(c);
    EXPECT_STR(g_corder, "");
}
TEST(eed_vch_kernel32_forwarders) {
    CHECK(UK(AddVectoredContinueHandler) != NULL);
    CHECK(UK(RemoveVectoredContinueHandler) != NULL);
    CHECK(UK(AddVectoredExceptionHandler) != NULL);
    CHECK(UK(RemoveVectoredExceptionHandler) != NULL);
}

/* ---- hardware exceptions with fix-up ---- */
#define FIXRIP(sym) do { g_fix_rip = (DWORD64)(ULONG_PTR)(sym); g_mode = 1; } while (0)
TEST(eed_hw_av_write_null) {
    NEEDVEH();
    veh_on(); FIXRIP(eed_av_write_after);
    eed_av_write((char *)0x10);
    veh_off();
    EXPECT_EQ(g_ncap, 1);
    EXPECT_EQ(g_cap[0].code, EXCEPTION_ACCESS_VIOLATION);
    EXPECT_EQ(g_cap[0].nparams, 2);
    EXPECT_EQ(g_cap[0].info[0], 1);
    EXPECT_EQ(g_cap[0].info[1], 0x10);
    EXPECT_EQ(g_cap[0].addr, (void *)eed_av_write);
    EXPECT_EQ(g_cap[0].rip, (DWORD64)(ULONG_PTR)eed_av_write);
    EXPECT_EQ(g_cap[0].flags, 0);
}
TEST(eed_hw_av_read_null) {
    NEEDVEH();
    veh_on(); FIXRIP(eed_av_read_after);
    eed_av_read((char *)0);
    veh_off();
    EXPECT_EQ(g_ncap, 1);
    EXPECT_EQ(g_cap[0].code, EXCEPTION_ACCESS_VIOLATION);
    EXPECT_EQ(g_cap[0].info[0], 0);
    EXPECT_EQ(g_cap[0].info[1], 0);
}
TEST(eed_hw_av_exact_address_reported) {
    NEEDVEH();
    static const ULONG_PTR addrs[] = { 1, 0x7FF, 0x1000, 0x12345, 0xFFFF, 0x10000, 0xFFFFFFFFull, 0x7FFFFFFF0000ull };
    for (size_t i = 0; i < sizeof addrs / sizeof *addrs; i++) {
        veh_on(); FIXRIP(eed_av_read_after);
        eed_av_read((char *)addrs[i]);
        veh_off();
        if (g_ncap != 1) { t_note("addr %llx did not fault", (unsigned long long)addrs[i]); continue; }
        CHECKF(g_cap[0].info[1] == addrs[i], "addr %llx reported %llx", (unsigned long long)addrs[i], (unsigned long long)g_cap[0].info[1]);
    }
}
TEST(eed_hw_av_noncanonical) {
    NEEDVEH();
    veh_on(); FIXRIP(eed_av_read_after);
    eed_av_read((char *)0x8000000000000000ull);
    veh_off();
    EXPECT_EQ(g_ncap, 1);
    EXPECT_EQ(g_cap[0].code, EXCEPTION_ACCESS_VIOLATION);
    EXPECT_EQ(g_cap[0].nparams, 2);
    t_note("non-canonical read: info[0]=%llx info[1]=%llx", (unsigned long long)g_cap[0].info[0], (unsigned long long)g_cap[0].info[1]);
}
TEST(eed_hw_av_kernel_address) {
    NEEDVEH();
    veh_on(); FIXRIP(eed_av_read_after);
    eed_av_read((char *)0xFFFFF78000000000ull);        /* KUSER_SHARED_DATA region is readable at 0x7FFE0000 only */
    veh_off();
    EXPECT_EQ(g_ncap, 1);
    EXPECT_EQ(g_cap[0].code, EXCEPTION_ACCESS_VIOLATION);
}
TEST(eed_hw_av_noaccess_page) {
    NEEDVEH(); REQUIRE(U(VirtualAlloc)); REQUIRE(U(VirtualFree));
    char *p = U(VirtualAlloc)(NULL, 0x2000, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS);
    CHECK(p != NULL); if (!p) return;
    veh_on(); FIXRIP(eed_av_read_after);
    eed_av_read(p + 0x123);
    veh_off();
    EXPECT_EQ(g_ncap, 1); EXPECT_EQ(g_cap[0].code, EXCEPTION_ACCESS_VIOLATION);
    EXPECT_EQ(g_cap[0].info[0], 0); EXPECT_EQ(g_cap[0].info[1], (ULONG_PTR)(p + 0x123));
    veh_on(); FIXRIP(eed_av_write_after);
    eed_av_write(p + 0x1FFF);
    veh_off();
    EXPECT_EQ(g_cap[0].info[0], 1); EXPECT_EQ(g_cap[0].info[1], (ULONG_PTR)(p + 0x1FFF));
    U(VirtualFree)(p, 0, MEM_RELEASE);
}
TEST(eed_hw_av_readonly_page_write) {
    NEEDVEH(); REQUIRE(U(VirtualAlloc)); REQUIRE(U(VirtualFree));
    char *p = U(VirtualAlloc)(NULL, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READONLY);
    CHECK(p != NULL); if (!p) return;
    veh_on(); FIXRIP(eed_av_read_after);
    eed_av_read(p);            /* read OK: no exception */
    g_mode = 0;
    EXPECT_EQ(g_ncap, 0);
    FIXRIP(eed_av_write_after);
    eed_av_write(p + 7);
    veh_off();
    EXPECT_EQ(g_ncap, 1); EXPECT_EQ(g_cap[0].info[0], 1); EXPECT_EQ(g_cap[0].info[1], (ULONG_PTR)(p + 7));
    U(VirtualFree)(p, 0, MEM_RELEASE);
}
TEST(eed_hw_av_reserved_not_committed) {
    NEEDVEH(); REQUIRE(U(VirtualAlloc)); REQUIRE(U(VirtualFree));
    char *p = U(VirtualAlloc)(NULL, 0x10000, MEM_RESERVE, PAGE_READWRITE);
    CHECK(p != NULL); if (!p) return;
    veh_on(); FIXRIP(eed_av_write_after);
    eed_av_write(p + 0x5000);
    veh_off();
    EXPECT_EQ(g_ncap, 1); EXPECT_EQ(g_cap[0].code, EXCEPTION_ACCESS_VIOLATION); EXPECT_EQ(g_cap[0].info[1], (ULONG_PTR)(p + 0x5000));
    U(VirtualFree)(p, 0, MEM_RELEASE);
}
TEST(eed_hw_av_freed_page) {
    NEEDVEH(); REQUIRE(U(VirtualAlloc)); REQUIRE(U(VirtualFree));
    char *p = U(VirtualAlloc)(NULL, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    CHECK(p != NULL); if (!p) return;
    *p = 1;
    U(VirtualFree)(p, 0, MEM_RELEASE);
    veh_on(); FIXRIP(eed_av_read_after);
    eed_av_read(p);
    veh_off();
    EXPECT_EQ(g_ncap, 1); EXPECT_EQ(g_cap[0].code, EXCEPTION_ACCESS_VIOLATION); EXPECT_EQ(g_cap[0].info[1], (ULONG_PTR)p);
}
TEST(eed_hw_av_decommitted_page) {
    NEEDVEH(); REQUIRE(U(VirtualAlloc)); REQUIRE(U(VirtualFree));
    char *p = U(VirtualAlloc)(NULL, 0x2000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    CHECK(p != NULL); if (!p) return;
    U(VirtualFree)(p + 0x1000, 0x1000, MEM_DECOMMIT);
    eed_av_write(p);   /* still fine */
    veh_on(); FIXRIP(eed_av_write_after);
    eed_av_write(p + 0x1000);
    veh_off();
    EXPECT_EQ(g_ncap, 1); EXPECT_EQ(g_cap[0].info[1], (ULONG_PTR)(p + 0x1000));
    U(VirtualFree)(p, 0, MEM_RELEASE);
}
TEST(eed_hw_av_execute_null) {
    NEEDVEH();
    veh_on(); FIXRIP(eed_call_ptr_after); g_fix_rsp_add = 8;
    eed_call_ptr((void *)0x10);
    veh_off();
    EXPECT_EQ(g_ncap, 1); EXPECT_EQ(g_cap[0].code, EXCEPTION_ACCESS_VIOLATION);
    EXPECT_EQ(g_cap[0].info[0], 8);
    EXPECT_EQ(g_cap[0].info[1], 0x10);
    EXPECT_EQ(g_cap[0].addr, (void *)0x10);
}
TEST(eed_hw_av_execute_data_page_dep) {
    NEEDVEH(); REQUIRE(U(VirtualAlloc)); REQUIRE(U(VirtualFree));
    unsigned char *p = U(VirtualAlloc)(NULL, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    CHECK(p != NULL); if (!p) return;
    p[0] = 0xC3;
    veh_on(); FIXRIP(eed_call_ptr_after); g_fix_rsp_add = 8;
    eed_call_ptr(p);
    veh_off();
    EXPECT_EQ(g_ncap, 1); EXPECT_EQ(g_cap[0].code, EXCEPTION_ACCESS_VIOLATION);
    EXPECT_EQ(g_cap[0].info[0], 8); EXPECT_EQ(g_cap[0].info[1], (ULONG_PTR)p);
    U(VirtualFree)(p, 0, MEM_RELEASE);
}
TEST(eed_hw_exec_after_protect_works) {
    NEEDVEH(); REQUIRE(U(VirtualAlloc)); REQUIRE(U(VirtualProtect)); REQUIRE(U(VirtualFree));
    unsigned char *p = U(VirtualAlloc)(NULL, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    CHECK(p != NULL); if (!p) return;
    p[0] = 0xC3;
    DWORD old;
    CHECK(U(VirtualProtect)(p, 0x1000, PAGE_EXECUTE_READ, &old));
    veh_on();
    eed_call_ptr(p);
    veh_off();
    EXPECT_EQ(g_ncap, 0);
    U(VirtualFree)(p, 0, MEM_RELEASE);
}
TEST(eed_hw_guard_page) {
    NEEDVEH(); REQUIRE(U(VirtualAlloc)); REQUIRE(U(VirtualFree)); REQUIRE(U(VirtualQuery));
    char *p = U(VirtualAlloc)(NULL, 0x2000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE | PAGE_GUARD);
    CHECK(p != NULL); if (!p) return;
    veh_on(); g_mode = 2;                  /* continue: the access is retried, guard now removed */
    volatile char c = *(volatile char *)(p + 0x10);
    veh_off();
    EXPECT_EQ(g_ncap, 1);
    EXPECT_EQ(g_cap[0].code, STATUS_GUARD_PAGE_VIOLATION);
    EXPECT_EQ(g_cap[0].nparams, 2);
    EXPECT_EQ(g_cap[0].info[0], 0);
    EXPECT_EQ(g_cap[0].info[1], (ULONG_PTR)(p + 0x10));
    MEMORY_BASIC_INFORMATION mbi;
    U(VirtualQuery)(p, &mbi, sizeof mbi);
    EXPECT_EQ(mbi.Protect & PAGE_GUARD, 0);
    EXPECT_EQ(mbi.Protect & 0xFF, PAGE_READWRITE);
    U(VirtualQuery)(p + 0x1000, &mbi, sizeof mbi);
    EXPECT_EQ(mbi.Protect & PAGE_GUARD, PAGE_GUARD);
    U(VirtualFree)(p, 0, MEM_RELEASE);
}
TEST(eed_hw_guard_page_write_access_type) {
    NEEDVEH(); REQUIRE(U(VirtualAlloc)); REQUIRE(U(VirtualFree));
    char *p = U(VirtualAlloc)(NULL, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE | PAGE_GUARD);
    CHECK(p != NULL); if (!p) return;
    veh_on(); g_mode = 2;
    *(volatile char *)(p + 3) = 5;
    veh_off();
    EXPECT_EQ(g_ncap, 1); EXPECT_EQ(g_cap[0].code, STATUS_GUARD_PAGE_VIOLATION);
    EXPECT_EQ(g_cap[0].info[0], 1);
    EXPECT_EQ(p[3], 5);
    U(VirtualFree)(p, 0, MEM_RELEASE);
}
TEST(eed_hw_div_by_zero) {
    NEEDVEH();
    veh_on(); FIXRIP(eed_div0_after);
    eed_div0();
    veh_off();
    EXPECT_EQ(g_ncap, 1);
    EXPECT_EQ(g_cap[0].code, EXCEPTION_INT_DIVIDE_BY_ZERO);
    EXPECT_EQ(g_cap[0].nparams, 0);
    EXPECT_EQ(g_cap[0].addr, (void *)eed_div0_insn);
    EXPECT_EQ(g_cap[0].rip, (DWORD64)(ULONG_PTR)eed_div0_insn);
}
TEST(eed_hw_int_overflow_idiv) {
    NEEDVEH();
    veh_on(); FIXRIP(eed_idiv_ovf_after);
    eed_idiv_ovf();
    veh_off();
    EXPECT_EQ(g_ncap, 1);
    /* INT_MIN / -1 raises #DE, which Windows reports as divide error or overflow */
    CHECKF(g_cap[0].code == EXCEPTION_INT_OVERFLOW || g_cap[0].code == EXCEPTION_INT_DIVIDE_BY_ZERO, "code 0x%lx", g_cap[0].code);
    t_note("idiv overflow code: 0x%lx", g_cap[0].code);
}
TEST(eed_hw_ud2_illegal_instruction) {
    NEEDVEH();
    veh_on(); FIXRIP(eed_ud2_after);
    eed_ud2();
    veh_off();
    EXPECT_EQ(g_ncap, 1);
    EXPECT_EQ(g_cap[0].code, EXCEPTION_ILLEGAL_INSTRUCTION);
    EXPECT_EQ(g_cap[0].addr, (void *)eed_ud2_insn);
    EXPECT_EQ(g_cap[0].rip, (DWORD64)(ULONG_PTR)eed_ud2_insn);
}
TEST(eed_hw_int3_breakpoint) {
    NEEDVEH();
    veh_on(); FIXRIP(eed_int3_after);
    eed_int3();
    veh_off();
    EXPECT_EQ(g_ncap, 1);
    EXPECT_EQ(g_cap[0].code, EXCEPTION_BREAKPOINT);
    /* the handler context Rip is adjusted back onto the int3 by the dispatcher; accept either, report which */
    CHECKF(g_cap[0].rip == (DWORD64)(ULONG_PTR)eed_int3_after || g_cap[0].rip == (DWORD64)(ULONG_PTR)eed_int3_insn, "rip %llx", (unsigned long long)g_cap[0].rip);
    t_note("int3 handler Rip %s the int3", g_cap[0].rip == (DWORD64)(ULONG_PTR)eed_int3_insn ? "at" : "past");
    t_note("int3: ExceptionAddress=%p (insn %p) nparams=%lu info0=%llx", g_cap[0].addr, (void *)eed_int3_insn, g_cap[0].nparams, (unsigned long long)g_cap[0].info[0]);
}
static volatile LONG g_bp_n;
static LONG CALLBACK veh_bp_escape(PEXCEPTION_POINTERS ep) {
    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_BREAKPOINT) return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedIncrement(&g_bp_n) >= 3) ep->ContextRecord->Rip = (DWORD64)(ULONG_PTR)eed_int3_after;   /* escape a re-execution loop */
    return EXCEPTION_CONTINUE_EXECUTION;
}
TEST(eed_hw_int3_continue_without_fixup) {
    NEEDVEH();
    PVOID h = U(AddVectoredExceptionHandler)(1, veh_bp_escape);
    g_bp_n = 0;
    eed_int3();                           /* resumes right after the int3 with the unmodified context */
    U(RemoveVectoredExceptionHandler)(h);
    CHECKF(g_bp_n == 1 || g_bp_n == 3, "handler calls %ld", (long)g_bp_n);   /* 3: context Rip was on the int3 and re-executed */
    t_note("int3 continue without fixup: %ld handler call(s)", (long)g_bp_n);
}
TEST(eed_hw_privileged_cli) {
    NEEDVEH();
    veh_on(); FIXRIP(eed_cli_after);
    eed_cli();
    veh_off();
    EXPECT_EQ(g_ncap, 1);
    EXPECT_EQ(g_cap[0].code, EXCEPTION_PRIV_INSTRUCTION);
    EXPECT_EQ(g_cap[0].addr, (void *)eed_cli_insn);
}
TEST(eed_hw_privileged_hlt) {
    NEEDVEH();
    veh_on(); FIXRIP(eed_hlt_after);
    eed_hlt();
    veh_off();
    EXPECT_EQ(g_ncap, 1);
    EXPECT_EQ(g_cap[0].code, EXCEPTION_PRIV_INSTRUCTION);
}
TEST(eed_hw_movaps_misaligned) {
    NEEDVEH();
    char *buf = (char *)malloc(64);
    char *q = (char *)(((ULONG_PTR)buf + 15) & ~(ULONG_PTR)15) + 1;
    veh_on(); FIXRIP(eed_movaps_after);
    eed_movaps(q);
    veh_off();
    free(buf);
    EXPECT_EQ(g_ncap, 1);
    CHECKF(g_cap[0].code == EXCEPTION_ACCESS_VIOLATION || g_cap[0].code == EXCEPTION_DATATYPE_MISALIGNMENT, "code 0x%lx", g_cap[0].code);
    t_note("movaps misaligned: code 0x%lx info0=%llx info1=%llx", g_cap[0].code, (unsigned long long)g_cap[0].info[0], (unsigned long long)g_cap[0].info[1]);
}
TEST(eed_hw_handler_modifies_rax) {
    NEEDVEH();
    veh_on(); FIXRIP(eed_ud2_after); g_fix_rax = 0x1234567; g_fix_set_rax = 1;
    typedef ULONG_PTR (*fn)(void);
    void *vp = (void *)eed_ud2; fn f = (fn)vp;
    ULONG_PTR r = f();
    veh_off();
    EXPECT_EQ(r, 0x1234567);
}
TEST(eed_hw_exception_in_other_thread_fixup) {
    NEEDVEH(); REQUIRE(U(CreateThread));
    veh_on();
    DWORD tid;
    HANDLE h = U(CreateThread)(NULL, 0, (LPTHREAD_START_ROUTINE)(void *)eed_ud2, NULL, CREATE_SUSPENDED, &tid);
    CHECK(h != NULL);
    if (h) { FIXRIP(eed_ud2_after); ResumeThread(h); WaitForSingleObject(h, 5000); CloseHandle(h); }
    veh_off();
    EXPECT_EQ(g_ncap, 1);
    EXPECT_EQ(g_cap[0].tid, tid);
    EXPECT_EQ(g_cap[0].code, EXCEPTION_ILLEGAL_INSTRUCTION);
}

/* ---- single-step (trap flag) ---- */
static volatile LONG g_ss_count;
static LONG CALLBACK veh_ss(PEXCEPTION_POINTERS ep) {
    if (ep->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP) return EXCEPTION_CONTINUE_SEARCH;
    InterlockedIncrement(&g_ss_count);
    if (g_ss_count >= 3) ep->ContextRecord->EFlags &= ~0x100u;
    return EXCEPTION_CONTINUE_EXECUTION;
}
TEST(eed_hw_trap_flag_single_step) {
    NEEDVEH();
    if (t_is_wine()) { t_skip("trap flag unreliable under wine"); return; }
    PVOID h = U(AddVectoredExceptionHandler)(1, veh_ss);
    g_ss_count = 0;
    eed_tf();
    U(RemoveVectoredExceptionHandler)(h);
    EXPECT_EQ(g_ss_count, 3);
}
