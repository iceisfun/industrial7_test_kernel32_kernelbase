/* Fibers: ConvertThreadToFiber(Ex), CreateFiber(Ex), SwitchToFiber, DeleteFiber, ConvertFiberToThread, IsThreadAFiber. */
#include "tf.h"
#include <emmintrin.h>

static __typeof__(&ConvertThreadToFiber) pCTF;
static __typeof__(&ConvertThreadToFiberEx) pCTFE;
static __typeof__(&ConvertFiberToThread) pCFT;
static __typeof__(&CreateFiber) pCF;
static __typeof__(&CreateFiberEx) pCFE;
static __typeof__(&SwitchToFiber) pSTF;
static __typeof__(&DeleteFiber) pDF;
static __typeof__(&IsThreadAFiber) pITAF;
#define NEED_FIB() do { pCTF = U(ConvertThreadToFiber); pCTFE = U(ConvertThreadToFiberEx); pCFT = U(ConvertFiberToThread); \
    pCF = U(CreateFiber); pCFE = U(CreateFiberEx); pSTF = U(SwitchToFiber); pDF = U(DeleteFiber); pITAF = U(IsThreadAFiber); \
    REQUIRE(pCTF); REQUIRE(pCTFE); REQUIRE(pCFT); REQUIRE(pCF); REQUIRE(pCFE); REQUIRE(pSTF); REQUIRE(pDF); REQUIRE(pITAF); } while (0)

static char flog[512];
static int fl;
static void L(char c) { if (fl < 500) { flog[fl++] = c; flog[fl] = 0; } }
static void logreset(void) { fl = 0; flog[0] = 0; }
static LPVOID f_main, f_a, f_b;

#define FIB_BODY(name) static DWORD WINAPI name(LPVOID unused)
static DWORD run(LPTHREAD_START_ROUTINE f) { logreset(); return tf_run(f, NULL); }

/* ---- conversion state ---- */
FIB_BODY(w_isfiber) {
    DWORD bits = 0;
    if (pITAF()) bits |= 1;
    LPVOID f = pCTF((LPVOID)0x1234);
    if (!f) return 0x1000;
    if (!pITAF()) bits |= 2;
    if (GetCurrentFiber() != f) bits |= 4;
    if (GetFiberData() != (LPVOID)0x1234) bits |= 8;
    if (!pCFT()) bits |= 0x10;
    if (pITAF()) bits |= 0x20;
    return bits;
}
TEST(fib_convert_toggles_isthreadafiber) { NEED_FIB(); EQ(run(w_isfiber), 0); }
TEST(fib_main_thread_is_not_a_fiber) { NEED_FIB(); CHECK(!pITAF()); }

FIB_BODY(w_twice) {
    DWORD bits = 0;
    LPVOID f = pCTF(NULL);
    if (!f) return 0x1000;
    SetLastError(0);
    LPVOID g = pCTF(NULL);
    if (g != NULL) bits |= 1;
    if (GetLastError() != 0x500) bits |= 2;           /* ERROR_ALREADY_FIBER */
    SetLastError(0);
    g = pCTFE(NULL, 0);
    if (g != NULL) bits |= 4;
    if (GetLastError() != 0x500) bits |= 8;
    if (GetCurrentFiber() != f) bits |= 0x10;
    pCFT();
    return bits;
}
TEST(fib_convert_twice_fails_already_fiber) { NEED_FIB(); EQ(run(w_twice), 0); }

FIB_BODY(w_flags) {
    DWORD bits = 0;
    LPVOID f = pCTFE((LPVOID)7, 0);
    if (!f) bits |= 1; else pCFT();
    f = pCTFE((LPVOID)7, FIBER_FLAG_FLOAT_SWITCH);
    if (!f) bits |= 2; else pCFT();
    static const DWORD bad[] = { 2, 3, 4, 0x80000000, 0xFFFFFFFF, 0x100 };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0);
        f = pCTFE(NULL, bad[i]);
        if (f) { bits |= 4; pCFT(); }
        if (GetLastError() != ERROR_INVALID_PARAMETER) bits |= 8;
        if (pITAF()) bits |= 0x10;
    }
    return bits;
}
TEST(fib_convertex_flags) { NEED_FIB(); EQ(run(w_flags), 0); }

FIB_BODY(w_notfiber) {
    SetLastError(0);
    BOOL r = pCFT();
    return (r ? 1 : 0) | (GetLastError() == 0x501 ? 0 : 2);   /* ERROR_ALREADY_THREAD */
}
TEST(fib_converttothread_on_plain_thread) { NEED_FIB(); EQ(run(w_notfiber), 0); }

FIB_BODY(w_reconvert) {
    DWORD bits = 0;
    for (int i = 0; i < 5; i++) {
        LPVOID f = pCTF((LPVOID)(ULONG_PTR)(i + 1));
        if (!f) { bits |= 1; break; }
        if (GetFiberData() != (LPVOID)(ULONG_PTR)(i + 1)) bits |= 2;
        if (!pCFT()) bits |= 4;
    }
    return bits;
}
TEST(fib_convert_back_and_forth) { NEED_FIB(); EQ(run(w_reconvert), 0); }

FIB_BODY(w_param_null) {
    LPVOID f = pCTF(NULL);
    DWORD bits = 0;
    if (!f) return 1;
    if (GetFiberData() != NULL) bits |= 2;
    if (*(LPVOID *)f != NULL) bits |= 4;
    pCFT();
    return bits;
}
TEST(fib_convert_null_param) { NEED_FIB(); EQ(run(w_param_null), 0); }

FIB_BODY(w_fiberdata_layout) {
    LPVOID f = pCTF((LPVOID)0xFEEDF00D);
    DWORD bits = 0;
    if (*(LPVOID *)f != (LPVOID)0xFEEDF00D) bits |= 1;   /* first field of the fiber is its data */
    pCFT();
    return bits;
}
TEST(fib_fiberdata_is_first_field) { NEED_FIB(); EQ(run(w_fiberdata_layout), 0); }

FIB_BODY(w_exit_while_fiber) {
    pCTF(NULL);
    return 0x21;       /* thread ends while still converted: no ConvertFiberToThread */
}
TEST(fib_thread_exit_while_fiber) { NEED_FIB(); EQ(run(w_exit_while_fiber), 0x21); }

/* ---- switching ---- */
static VOID WINAPI fa_basic(LPVOID p) { L('a'); pSTF(f_main); L('A'); pSTF(f_main); for (;;) pSTF(f_main); }
FIB_BODY(w_switch) {
    f_main = pCTF(NULL);
    f_a = pCF(0, fa_basic, NULL);
    if (!f_a) return 1;
    L('m'); pSTF(f_a); L('M'); pSTF(f_a); L('Z');
    pDF(f_a);
    pCFT();
    return 0;
}
TEST(fib_switch_order) { NEED_FIB(); EQ(run(w_switch), 0); EXPECT_STR(flog, "maMAZ"); }

static VOID WINAPI fb_chain(LPVOID p) { L('b'); pSTF(f_a); for (;;) pSTF(f_main); }
static VOID WINAPI fa_chain(LPVOID p) { L('a'); pSTF(f_b); L('A'); pSTF(f_main); for (;;) pSTF(f_main); }
FIB_BODY(w_chain) {
    f_main = pCTF(NULL);
    f_a = pCF(0, fa_chain, NULL);
    f_b = pCF(0, fb_chain, NULL);
    L('m'); pSTF(f_a); L('M');
    pDF(f_a); pDF(f_b);
    pCFT();
    return 0;
}
TEST(fib_switch_chain_three_fibers) { NEED_FIB(); EQ(run(w_chain), 0); EXPECT_STR(flog, "mabAM"); }

static LPVOID g_seen_param, g_seen_data, g_seen_cur;
static VOID WINAPI fa_param(LPVOID p) {
    g_seen_param = p; g_seen_data = GetFiberData(); g_seen_cur = GetCurrentFiber();
    for (;;) pSTF(f_main);
}
FIB_BODY(w_param) {
    f_main = pCTF((LPVOID)0x1);
    f_a = pCF(0, fa_param, (LPVOID)0xBEEF1234);
    g_seen_param = g_seen_data = g_seen_cur = NULL;
    pSTF(f_a);
    DWORD bits = 0;
    if (g_seen_param != (LPVOID)0xBEEF1234) bits |= 1;
    if (g_seen_data != (LPVOID)0xBEEF1234) bits |= 2;
    if (g_seen_cur != f_a) bits |= 4;
    if (GetCurrentFiber() != f_main) bits |= 8;
    if (GetFiberData() != (LPVOID)0x1) bits |= 0x10;
    pDF(f_a); pCFT();
    return bits;
}
TEST(fib_param_and_fiberdata) { NEED_FIB(); EQ(run(w_param), 0); }

static volatile LONG pp;
static VOID WINAPI fa_pp(LPVOID p) { for (;;) { pp++; pSTF(f_main); } }
FIB_BODY(w_pingpong) {
    f_main = pCTF(NULL);
    f_a = pCF(0, fa_pp, NULL);
    pp = 0;
    for (int i = 0; i < 2000; i++) pSTF(f_a);
    DWORD r = (DWORD)pp;
    pDF(f_a); pCFT();
    return r;
}
TEST(fib_ping_pong_2000) { NEED_FIB(); EQ(run(w_pingpong), 2000); }

static LPVOID fibs[16]; static int counts[16];
static VOID WINAPI fa_idx(LPVOID p) { int i = (int)(ULONG_PTR)p; for (;;) { counts[i]++; pSTF(f_main); } }
FIB_BODY(w_many) {
    f_main = pCTF(NULL);
    memset(counts, 0, sizeof counts);
    for (int i = 0; i < 16; i++) { fibs[i] = pCF(0, fa_idx, (LPVOID)(ULONG_PTR)i); if (!fibs[i]) return 0x1000 + i; }
    for (int r = 0; r < 3; r++) for (int i = 0; i < 16; i++) pSTF(fibs[i]);
    DWORD bad = 0;
    for (int i = 0; i < 16; i++) if (counts[i] != 3) bad++;
    for (int i = 0; i < 16; i++) pDF(fibs[i]);
    pCFT();
    return bad;
}
TEST(fib_many_fibers_round_robin) { NEED_FIB(); EQ(run(w_many), 0); }

static LPVOID big[100];
static VOID WINAPI fa_inc(LPVOID p) { for (;;) { (*(int *)p)++; pSTF(f_main); } }
FIB_BODY(w_hundred) {
    f_main = pCTF(NULL);
    static int c[100]; memset(c, 0, sizeof c);
    for (int i = 0; i < 100; i++) { big[i] = pCF(0x4000, fa_inc, &c[i]); if (!big[i]) return 0x1000 + i; }
    for (int i = 0; i < 100; i++) pSTF(big[i]);
    for (int i = 99; i >= 0; i--) pSTF(big[i]);
    DWORD bad = 0;
    for (int i = 0; i < 100; i++) if (c[i] != 2) bad++;
    for (int i = 0; i < 100; i++) pDF(big[i]);
    pCFT();
    return bad;
}
TEST(fib_hundred_live_fibers) { NEED_FIB(); EQ(run(w_hundred), 0); }

FIB_BODY(w_create_delete_loop) {
    for (int i = 0; i < 500; i++) {
        LPVOID f = pCF(0, fa_pp, NULL);
        if (!f) return 1 + i;
        pDF(f);
    }
    return 0;
}
TEST(fib_create_delete_unrun_500) { NEED_FIB(); EQ(run(w_create_delete_loop), 0); }

FIB_BODY(w_switch_self) {
    f_main = pCTF(NULL);
    pSTF(f_main);                 /* switching to the running fiber is a no-op */
    pSTF(GetCurrentFiber());
    pCFT();
    return 0x31;
}
TEST(fib_switch_to_self_noop) { NEED_FIB(); EQ(run(w_switch_self), 0x31); }

/* ---- stacks ---- */
static volatile int touched;
static VOID WINAPI fa_touch(LPVOID p) {
    for (;;) {
        size_t n = (size_t)(ULONG_PTR)p;
        volatile char *b = (volatile char *)_alloca(n);
        for (size_t i = 0; i < n; i += 4096) b[i] = 1;
        b[n - 1] = 1;
        touched++;
        pSTF(f_main);
    }
}
static DWORD stack_case(SIZE_T sz, SIZE_T touch) {
    f_main = pCTF(NULL);
    f_a = pCF(sz, fa_touch, (LPVOID)touch);
    DWORD r = 0;
    touched = 0;
    if (!f_a) r = 1; else { pSTF(f_a); pSTF(f_a); if (touched != 2) r = 2; pDF(f_a); }
    pCFT();
    return r;
}
FIB_BODY(w_st0) { return stack_case(0, 0x20000); }
FIB_BODY(w_st1) { return stack_case(1, 0x10000); }
FIB_BODY(w_st4k) { return stack_case(0x1000, 0x20000); }
FIB_BODY(w_st64k) { return stack_case(0x10000, 0x30000); }
FIB_BODY(w_st1m) { return stack_case(0x100000, 0x80000); }
FIB_BODY(w_st4m) { return stack_case(0x400000, 0x80000); }
TEST(fib_stack_default) { NEED_FIB(); EQ(run(w_st0), 0); }
TEST(fib_stack_commit_1) { NEED_FIB(); EQ(run(w_st1), 0); }
TEST(fib_stack_commit_4k) { NEED_FIB(); EQ(run(w_st4k), 0); }
TEST(fib_stack_commit_64k) { NEED_FIB(); EQ(run(w_st64k), 0); }
TEST(fib_stack_commit_1m) { NEED_FIB(); EQ(run(w_st1m), 0); }
TEST(fib_stack_commit_4m) { NEED_FIB(); EQ(run(w_st4m), 0); }

FIB_BODY(w_createex) {
    f_main = pCTF(NULL);
    DWORD bits = 0;
    static const struct { SIZE_T commit, reserve; DWORD flags; } cfg[] = {
        { 0, 0, 0 }, { 0x10000, 0x200000, 0 }, { 0x1000, 0x100000, FIBER_FLAG_FLOAT_SWITCH }, { 0, 0x400000, 0 }, { 0x20000, 0, FIBER_FLAG_FLOAT_SWITCH },
    };
    for (size_t i = 0; i < sizeof cfg / sizeof cfg[0]; i++) {
        touched = 0;
        LPVOID f = pCFE(cfg[i].commit, cfg[i].reserve, cfg[i].flags, fa_touch, (LPVOID)0x8000);
        if (!f) { bits |= 1u << i; continue; }
        pSTF(f);
        if (touched != 1) bits |= 0x100u << i;
        pDF(f);
    }
    pCFT();
    return bits;
}
TEST(fib_createex_stack_and_flags) { NEED_FIB(); EQ(run(w_createex), 0); }

FIB_BODY(w_createex_odd) {
    DWORD bits = 0;
    SetLastError(0);
    LPVOID f = pCFE(0x100000, 0x10000, 0, fa_touch, NULL);       /* reserve smaller than commit */
    t_note("CreateFiberEx(commit 1M, reserve 64K): %p err %lu", f, GetLastError());
    if (f) pDF(f);
    SetLastError(0);
    f = pCFE(0, 0, 2, fa_touch, NULL);                           /* unknown flag */
    t_note("CreateFiberEx(flags=2): %p err %lu", f, GetLastError());
    if (f) pDF(f);
    SetLastError(0);
    f = pCFE(0, 0, 0xFFFFFFFF, fa_touch, NULL);
    t_note("CreateFiberEx(flags=-1): %p err %lu", f, GetLastError());
    if (f) pDF(f);
    return bits;
}
TEST(fib_createex_odd_parameters) { NEED_FIB(); EQ(run(w_createex_odd), 0); }

FIB_BODY(w_huge) {
    static const SIZE_T sz[] = { (SIZE_T)0x10000000000ULL, (SIZE_T)-1 };
    DWORD bits = 0;
    for (size_t i = 0; i < 2; i++) {
        SetLastError(0);
        LPVOID f = pCF(sz[i], fa_pp, NULL);
        if (f) { bits |= 1; pDF(f); }
        if (GetLastError() == 0) bits |= 2;
        SetLastError(0);
        f = pCFE(0, sz[i], 0, fa_pp, NULL);
        if (f) { bits |= 4; pDF(f); }
        if (GetLastError() == 0) bits |= 8;
    }
    return bits;
}
TEST(fib_create_huge_stack_fails) {
    NEED_FIB();
    if (t_is_wine()) { t_skip("wine asserts on absurd reservations"); return; }
    EQ(run(w_huge), 0);
}

static ULONG_PTR s_main_base, s_main_lim, s_fib_base, s_fib_lim, s_fib_local;
static VOID WINAPI fa_teb(LPVOID p) {
    volatile int x = 0;
    char *teb = (char *)NtCurrentTeb();
    s_fib_base = *(ULONG_PTR *)(teb + 8); s_fib_lim = *(ULONG_PTR *)(teb + 0x10); s_fib_local = (ULONG_PTR)&x;
    for (;;) pSTF(f_main);
}
FIB_BODY(w_teb) {
    char *teb = (char *)NtCurrentTeb();
    ULONG_PTR b0 = *(ULONG_PTR *)(teb + 8), l0 = *(ULONG_PTR *)(teb + 0x10);
    f_main = pCTF(NULL);
    f_a = pCF(0, fa_teb, NULL);
    pSTF(f_a);
    s_main_base = *(ULONG_PTR *)(teb + 8); s_main_lim = *(ULONG_PTR *)(teb + 0x10);
    DWORD bits = 0;
    if (s_fib_base == b0) bits |= 1;                                /* fiber has its own stack range */
    if (!(s_fib_local < s_fib_base && s_fib_local >= s_fib_lim)) bits |= 2;
    if (s_main_base != b0 || s_main_lim != l0) bits |= 4;            /* restored on switch back */
    pDF(f_a); pCFT();
    return bits;
}
TEST(fib_teb_stack_bounds_follow_fiber) { NEED_FIB(); EQ(run(w_teb), 0); }

/* ---- TLS / FLS / last error ---- */
static DWORD g_t;
static VOID WINAPI fa_tls(LPVOID p) {
    TlsSetValue(g_t, (LPVOID)0x7777);
    pSTF(f_main);
    for (;;) pSTF(f_main);
}
FIB_BODY(w_tls) {
    f_main = pCTF(NULL);
    g_t = TlsAlloc();
    TlsSetValue(g_t, (LPVOID)0x1111);
    f_a = pCF(0, fa_tls, NULL);
    pSTF(f_a);
    DWORD bits = 0;
    if (TlsGetValue(g_t) != (LPVOID)0x7777) bits |= 1;               /* TLS is per thread, shared by fibers */
    pDF(f_a); pCFT();
    TlsFree(g_t);
    return bits;
}
TEST(fib_tls_shared_by_fibers) { NEED_FIB(); EQ(run(w_tls), 0); }

static volatile LONG fcb_n; static volatile ULONG_PTR fcb_v; static volatile LPVOID fcb_fib;
static VOID WINAPI fls_cb(PVOID p) { InterlockedIncrement(&fcb_n); fcb_v = (ULONG_PTR)p; fcb_fib = GetCurrentFiber(); }
static DWORD g_fls;
static ULONG_PTR fib_saw;
static VOID WINAPI fa_fls(LPVOID p) {
    fib_saw = (ULONG_PTR)FlsGetValue(g_fls);
    FlsSetValue(g_fls, (PVOID)0xBBB);
    pSTF(f_main);
    fib_saw = (ULONG_PTR)FlsGetValue(g_fls);
    for (;;) pSTF(f_main);
}
FIB_BODY(w_fls) {
    f_main = pCTF(NULL);
    fcb_n = 0; fcb_v = 0;
    g_fls = FlsAlloc(fls_cb);
    FlsSetValue(g_fls, (PVOID)0xAAA);
    f_a = pCF(0, fa_fls, NULL);
    pSTF(f_a);
    DWORD bits = 0;
    if (fib_saw != 0) bits |= 1;                                      /* new fiber has empty FLS */
    if (FlsGetValue(g_fls) != (PVOID)0xAAA) bits |= 2;                /* main fiber keeps its value */
    pSTF(f_a);
    if (fib_saw != 0xBBB) bits |= 4;                                  /* fiber keeps its value */
    if (fcb_n != 0) bits |= 8;
    pDF(f_a);
    if (fcb_n != 1 || fcb_v != 0xBBB) bits |= 0x10;                   /* DeleteFiber runs the FLS callback */
    FlsFree(g_fls);
    if (fcb_n != 2 || fcb_v != 0xAAA) bits |= 0x20;                   /* FlsFree runs it for the current fiber */
    pCFT();
    return bits;
}
TEST(fib_fls_is_per_fiber) { NEED_FIB(); EQ(run(w_fls), 0); }

static VOID WINAPI fa_fls_noset(LPVOID p) { for (;;) pSTF(f_main); }
FIB_BODY(w_fls_unset) {
    f_main = pCTF(NULL);
    fcb_n = 0;
    g_fls = FlsAlloc(fls_cb);
    f_a = pCF(0, fa_fls_noset, NULL);
    pSTF(f_a);
    pDF(f_a);
    DWORD bits = fcb_n ? 1 : 0;
    FlsFree(g_fls);
    pCFT();
    return bits;
}
TEST(fib_fls_callback_skipped_without_value) { NEED_FIB(); EQ(run(w_fls_unset), 0); }

static VOID WINAPI fa_fls_thread_exit(LPVOID p) {
    FlsSetValue(g_fls, (PVOID)0xCCC);
    for (;;) pSTF(f_main);
}
FIB_BODY(w_fls_exit) {
    f_main = pCTF(NULL);
    g_fls = FlsAlloc(fls_cb);
    FlsSetValue(g_fls, (PVOID)0xAAA);
    f_a = pCF(0, fa_fls_thread_exit, NULL);
    pSTF(f_a);
    return 0;      /* thread exits while both fibers still exist */
}
TEST(fib_fls_thread_exit_callbacks) {
    NEED_FIB();
    fcb_n = 0; fcb_v = 0;
    EQ(run(w_fls_exit), 0);
    t_note("FLS callbacks at thread exit with converted fiber: %ld (last value %llx)", fcb_n, (unsigned long long)fcb_v);
    CHECK(fcb_n >= 1);
}

static DWORD le;
static VOID WINAPI fa_le(LPVOID p) { SetLastError(0x1357); pSTF(f_main); for (;;) pSTF(f_main); }
FIB_BODY(w_lasterror) {
    f_main = pCTF(NULL);
    f_a = pCF(0, fa_le, NULL);
    SetLastError(0x2468);
    pSTF(f_a);
    le = GetLastError();
    pDF(f_a); pCFT();
    return 0;
}
TEST(fib_last_error_is_per_thread) {
    NEED_FIB();
    EQ(run(w_lasterror), 0);
    t_note("last error after switching back from fiber that set 0x1357: %lx", le);
}

/* ---- delete / exit semantics ---- */
FIB_BODY(w_delete_self) {
    LPVOID f = pCTF(NULL);
    pDF(f);                       /* deleting the running converted fiber ends the thread */
    return 0x99;
}
TEST(fib_delete_running_fiber_exits_thread) { NEED_FIB(); EQ(run(w_delete_self), 1); }

static VOID WINAPI fa_delete_other(LPVOID p) { pDF(f_b); L('d'); pSTF(f_main); for (;;) pSTF(f_main); }
static VOID WINAPI fb_idle(LPVOID p) { L('b'); for (;;) pSTF(f_main); }
FIB_BODY(w_delete_other) {
    f_main = pCTF(NULL);
    f_b = pCF(0, fb_idle, NULL);
    f_a = pCF(0, fa_delete_other, NULL);
    pSTF(f_b);
    L('m');
    pSTF(f_a);
    L('M');
    pDF(f_a); pCFT();
    return 0;
}
TEST(fib_fiber_deletes_another_fiber) { NEED_FIB(); EQ(run(w_delete_other), 0); EXPECT_STR(flog, "bmdM"); }

static VOID WINAPI fa_exit_thread(LPVOID p) {
    void (WINAPI *ex)(DWORD) = (void (WINAPI *)(DWORD))t_proc("ExitThread");
    L('x');
    ex(0x66);
    L('!');
}
FIB_BODY(w_exit_in_fiber) {
    f_main = pCTF(NULL);
    f_a = pCF(0, fa_exit_thread, NULL);
    pSTF(f_a);
    L('?');
    return 0;
}
TEST(fib_exitthread_inside_fiber) { NEED_FIB(); EQ(run(w_exit_in_fiber), 0x66); EXPECT_STR(flog, "x"); }

static VOID WINAPI fa_returns(LPVOID p) { L('r'); }
FIB_BODY(w_fiber_returns) {
    f_main = pCTF(NULL);
    f_a = pCF(0, fa_returns, NULL);
    pSTF(f_a);
    L('?');             /* never reached: returning from a fiber function ends the thread */
    return 0x44;
}
TEST(fib_fiber_function_return_ends_thread) {
    NEED_FIB();
    DWORD c = run(w_fiber_returns);
    t_note("thread exit code after fiber function returned: %lx", c);
    CHECK(c != 0xDEAD0001 && c != 0xDEAD0000);
    EXPECT_STR(flog, "r");
}

/* ---- exceptions ---- */
static volatile LPVOID veh_fiber; static volatile LONG veh_n;
static LONG CALLBACK fib_veh(PEXCEPTION_POINTERS ep) {
    if (ep->ExceptionRecord->ExceptionCode != 0xE0001234) return EXCEPTION_CONTINUE_SEARCH;
    veh_fiber = GetCurrentFiber(); veh_n++;
    return EXCEPTION_CONTINUE_EXECUTION;
}
static VOID WINAPI fa_raise(LPVOID p) {
    RaiseException(0xE0001234, 0, 0, NULL);
    L('r');
    pSTF(f_main);
    for (;;) pSTF(f_main);
}
FIB_BODY(w_veh) {
    f_main = pCTF(NULL);
    PVOID h = AddVectoredExceptionHandler(1, fib_veh);
    veh_n = 0; veh_fiber = NULL;
    f_a = pCF(0, fa_raise, NULL);
    pSTF(f_a);
    RaiseException(0xE0001234, 0, 0, NULL);
    RemoveVectoredExceptionHandler(h);
    DWORD bits = 0;
    if (veh_n != 2) bits |= 1;
    if (veh_fiber != f_main) bits |= 2;       /* last raise happened on the main fiber */
    pDF(f_a); pCFT();
    return bits;
}
TEST(fib_vectored_exception_in_fiber) { NEED_FIB(); EQ(run(w_veh), 0); EXPECT_STR(flog, "r"); }

/* ---- register preservation across a switch ---- */
static VOID WINAPI fa_clobber(LPVOID p) {
    for (;;) {
        __asm__ volatile("pxor %%xmm6, %%xmm6\n pxor %%xmm7, %%xmm7\n" ::: "xmm6", "xmm7");
        pSTF(f_main);
    }
}
FIB_BODY(w_xmm) {
    f_main = pCTF(NULL);
    f_a = pCF(0, fa_clobber, NULL);
    unsigned long long in = 0x1122334455667788ULL, out = 0, in7 = 0x99AABBCCDDEEFF00ULL, out7 = 0;
    __asm__ volatile("movq %0, %%xmm6\n movq %1, %%xmm7\n" :: "r"(in), "r"(in7) : "xmm6", "xmm7");
    pSTF(f_a);
    __asm__ volatile("movq %%xmm6, %0\n movq %%xmm7, %1\n" : "=r"(out), "=r"(out7));
    pDF(f_a); pCFT();
    return (out == in ? 0 : 1) | (out7 == in7 ? 0 : 2);
}
TEST(fib_nonvolatile_xmm_preserved) { NEED_FIB(); EQ(run(w_xmm), 0); }

static unsigned csr_in_fiber;
static VOID WINAPI fa_csr(LPVOID p) {
    for (;;) { csr_in_fiber = _mm_getcsr(); _mm_setcsr(_mm_getcsr() | 0x6000); pSTF(f_main); }
}
FIB_BODY(w_csr) {
    f_main = pCTFE(NULL, FIBER_FLAG_FLOAT_SWITCH);
    f_a = pCFE(0, 0, FIBER_FLAG_FLOAT_SWITCH, fa_csr, NULL);
    unsigned before = _mm_getcsr();
    pSTF(f_a);
    unsigned after = _mm_getcsr();
    _mm_setcsr(before);
    t_note("MxCsr: main before %x, new fiber start %x, main after switch back %x", before, csr_in_fiber, after);
    pDF(f_a); pCFT();
    return 0;
}
TEST(fib_float_switch_mxcsr_note) { NEED_FIB(); EQ(run(w_csr), 0); }

/* ---- per-thread independence ---- */
static DWORD WINAPI th_fiber_round(LPVOID p) {
    LPVOID f = pCTF(p);
    DWORD bits = 0;
    if (GetFiberData() != p) bits |= 1;
    if (GetCurrentFiber() != f) bits |= 2;
    LPVOID a = pCF(0, fa_inc, &bits);       /* fa_inc increments *p each time it is entered */
    DWORD before = bits;
    (void)before;
    pDF(a);
    pCFT();
    return bits;
}
TEST(fib_independent_per_thread) {
    NEED_FIB();
    enum { N = 8 };
    HANDLE hs[N];
    for (int i = 0; i < N; i++) hs[i] = CreateThread(NULL, 0, th_fiber_round, (LPVOID)(ULONG_PTR)(0x100 + i), 0, NULL);
    int bad = 0;
    for (int i = 0; i < N; i++) if (tf_join(hs[i]) != 0) bad++;
    EQ(bad, 0);
}
TEST(fib_currentfiber_on_plain_thread_note) {
    LPVOID f = GetCurrentFiber();
    t_note("NT_TIB.FiberData on a non-fiber thread: %p", f);
}
