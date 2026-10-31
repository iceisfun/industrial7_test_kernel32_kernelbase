#include "g18.h"
/* gap18: CreateFiber, CreateFiberEx, SwitchToFiber, DeleteFiber, IsThreadAFiber.
 * Every fiber operation runs on a dedicated thread; the thread returns the source line
 * of the first failed condition (0 = pass), and the test thread asserts on it. */

typedef DWORD (*fb_body)(void);
static DWORD WINAPI fb_thread(LPVOID p) { return ((fb_body)p)(); }
static DWORD fb_run(fb_body f, DWORD *exitcode) {
    HANDLE t = CreateThread(NULL, 0, fb_thread, (LPVOID)f, 0, NULL);
    if (!t) return 0xDEAD0000;
    DWORD r = WaitForSingleObject(t, 20000);
    DWORD code = 0xDEAD0001;
    if (r == WAIT_OBJECT_0) GetExitCodeThread(t, &code); else TerminateThread(t, 1);
    CloseHandle(t);
    if (exitcode) *exitcode = code;
    return code;
}
#define RUN(f) EQ(fb_run(f, NULL), 0)

static LPVOID g_main; static LPVOID g_fibA, g_fibB;
static volatile LONG g_step; static volatile LONG g_log[64]; static volatile LONG g_n;
static LPVOID g_seen_data, g_seen_cur; static BOOL g_seen_isfiber; static void *g_seen_stack_base, *g_seen_stack_limit;
static void logv(LONG v) { if (g_n < 64) g_log[g_n++] = v; }

/* ------------------------- IsThreadAFiber ------------------------- */
static DWORD b_isfiber_basic(void) {
    CK(U(IsThreadAFiber) && U(ConvertThreadToFiber) && U(ConvertFiberToThread));
    CK(U(IsThreadAFiber)() == FALSE);
    LPVOID f = U(ConvertThreadToFiber)((LPVOID)0x77);
    CK(f != NULL);
    CK(U(IsThreadAFiber)() == TRUE);
    CK(GetFiberData() == (LPVOID)0x77);
    CK(GetCurrentFiber() == f);
    CK(U(ConvertFiberToThread)());
    CK(U(IsThreadAFiber)() == FALSE);
    return 0;
}
TEST(gap18_fiber_isthreadafiber_lifecycle) { RUN(b_isfiber_basic); }

static DWORD b_isfiber_otherthread(void) {
    CK(U(ConvertThreadToFiber)(NULL));
    DWORD WINAPI other(LPVOID p) { return U(IsThreadAFiber)() ? 1 : 0; }
    HANDLE t = CreateThread(NULL, 0, other, NULL, 0, NULL);
    WaitForSingleObject(t, 5000); DWORD c = 9; GetExitCodeThread(t, &c); CloseHandle(t);
    CK(c == 0);            /* per-thread state */
    CK(U(IsThreadAFiber)());
    CK(U(ConvertFiberToThread)());
    return 0;
}
TEST(gap18_fiber_isthreadafiber_per_thread) { RUN(b_isfiber_otherthread); }

TEST(gap18_fiber_isthreadafiber_main_thread_false) {
    REQUIRE(U(IsThreadAFiber));
    CHECK(U(IsThreadAFiber)() == FALSE);
}
TEST(gap18_fiber_isthreadafiber_exports) {
    REQUIRE(UK(IsThreadAFiber)); REQUIRE(UB(IsThreadAFiber));
    CHECK(UK(IsThreadAFiber)() == UB(IsThreadAFiber)());
}

/* ------------------------- ConvertThreadToFiber(Ex) ------------------------- */
static DWORD b_convert_twice(void) {
    CK(U(ConvertThreadToFiber)((LPVOID)1));
    SetLastError(0);
    CK(U(ConvertThreadToFiber)((LPVOID)2) == NULL);
    CK(GetLastError() == ERROR_ALREADY_FIBER);
    CK(GetFiberData() == (LPVOID)1);
    SetLastError(0);
    CK(U(ConvertThreadToFiberEx)((LPVOID)3, 0) == NULL);
    CK(GetLastError() == ERROR_ALREADY_FIBER);
    CK(U(ConvertFiberToThread)());
    return 0;
}
TEST(gap18_fiber_convert_twice_already_fiber) { RUN(b_convert_twice); }

static DWORD b_convert_back_nonfiber(void) {
    SetLastError(0);
    CK(U(ConvertFiberToThread)() == FALSE);
    DWORD e = GetLastError();
    CK(e == 1281 /* ERROR_ALREADY_THREAD */);
    return 0;
}
TEST(gap18_fiber_convert_fiber_to_thread_on_plain_thread) { RUN(b_convert_back_nonfiber); }

static DWORD b_convert_reuse(void) {
    for (int i = 0; i < 20; i++) {
        CK(U(ConvertThreadToFiber)((LPVOID)(ULONG_PTR)(i + 1)));
        CK(GetFiberData() == (LPVOID)(ULONG_PTR)(i + 1));
        CK(U(ConvertFiberToThread)());
        CK(!U(IsThreadAFiber)());
    }
    return 0;
}
TEST(gap18_fiber_convert_repeat) { RUN(b_convert_reuse); }

static DWORD b_convertex_flags(void) {
    static const DWORD fl[] = { 2, 4, 0x10, 0x80000000, 0xFFFFFFFF };
    for (unsigned i = 0; i < 5; i++) {
        SetLastError(0);
        LPVOID f = U(ConvertThreadToFiberEx)(NULL, fl[i]);
        t_note("ConvertThreadToFiberEx flags 0x%lx -> %p err %lu", fl[i], f, f ? 0 : GetLastError());
        if (f) { if (!U(ConvertFiberToThread)()) return __LINE__; }
        else if (GetLastError() == 0) return __LINE__;
    }
    return 0;
}
TEST(gap18_fiber_convertex_invalid_flags) { REQUIRE(U(ConvertThreadToFiberEx)); RUN(b_convertex_flags); }

/* ------------------------- CreateFiber / SwitchToFiber ------------------------- */
static VOID CALLBACK fib_simple(LPVOID p) {
    g_seen_data = GetFiberData(); g_seen_cur = GetCurrentFiber(); g_seen_isfiber = U(IsThreadAFiber)();
    g_step++;
    U(SwitchToFiber)(g_main);
}
static DWORD b_create_switch(void) {
    CK(U(CreateFiber) && U(SwitchToFiber) && U(DeleteFiber));
    g_main = U(ConvertThreadToFiber)(NULL); CK(g_main);
    g_step = 0; g_seen_data = g_seen_cur = NULL; g_seen_isfiber = FALSE;
    LPVOID f = U(CreateFiber)(0, fib_simple, (LPVOID)0xABCD);
    CK(f != NULL);
    CK(g_step == 0);                 /* creating does not run it */
    U(SwitchToFiber)(f);
    CK(g_step == 1);
    CK(g_seen_data == (LPVOID)0xABCD);
    CK(g_seen_cur == f);
    CK(g_seen_isfiber);
    CK(GetCurrentFiber() == g_main);
    U(DeleteFiber)(f);
    CK(U(ConvertFiberToThread)());
    return 0;
}
TEST(gap18_fiber_create_switch_basic) { RUN(b_create_switch); }

static VOID CALLBACK fib_pingpong(LPVOID p) {
    for (;;) { logv((LONG)(ULONG_PTR)p); g_step++; U(SwitchToFiber)(g_main); }
}
static DWORD b_pingpong(void) {
    g_main = U(ConvertThreadToFiber)(NULL); CK(g_main);
    g_step = 0; g_n = 0;
    LPVOID f = U(CreateFiber)(0, fib_pingpong, (LPVOID)5);
    CK(f);
    for (int i = 0; i < 1000; i++) { U(SwitchToFiber)(f); logv(-1); }
    CK(g_step == 1000);
    CK(g_log[0] == 5 && g_log[1] == -1 && g_log[2] == 5);       /* resumed after its SwitchToFiber */
    U(DeleteFiber)(f);
    CK(U(ConvertFiberToThread)());
    return 0;
}
TEST(gap18_fiber_pingpong_1000) { RUN(b_pingpong); }

static VOID CALLBACK fib_a(LPVOID p);
static VOID CALLBACK fib_b(LPVOID p);
static VOID CALLBACK fib_a(LPVOID p) { logv(1); U(SwitchToFiber)(g_fibB); logv(3); U(SwitchToFiber)(g_fibB); logv(5); U(SwitchToFiber)(g_main); for (;;) U(SwitchToFiber)(g_main); }
static VOID CALLBACK fib_b(LPVOID p) { logv(2); U(SwitchToFiber)(g_fibA); logv(4); U(SwitchToFiber)(g_fibA); for (;;) U(SwitchToFiber)(g_main); }
static DWORD b_two_fibers(void) {
    g_main = U(ConvertThreadToFiber)(NULL); CK(g_main);
    g_n = 0;
    g_fibA = U(CreateFiber)(0, fib_a, NULL); g_fibB = U(CreateFiber)(0, fib_b, NULL);
    CK(g_fibA && g_fibB && g_fibA != g_fibB);
    U(SwitchToFiber)(g_fibA);
    CK(g_n == 5);
    for (int i = 0; i < 5; i++) CK(g_log[i] == i + 1);
    U(DeleteFiber)(g_fibA); U(DeleteFiber)(g_fibB);
    CK(U(ConvertFiberToThread)());
    return 0;
}
TEST(gap18_fiber_two_fibers_interleave) { RUN(b_two_fibers); }

static DWORD g_fls; 
static VOID CALLBACK fib_fls(LPVOID p) {
    U(FlsSetValue)(g_fls, (PVOID)0x2222);
    logv((LONG)(ULONG_PTR)U(FlsGetValue)(g_fls));
    U(SwitchToFiber)(g_main);
    for (;;) U(SwitchToFiber)(g_main);
}
static DWORD b_fls(void) {
    CK(U(FlsAlloc) && U(FlsSetValue) && U(FlsGetValue) && U(FlsFree));
    g_main = U(ConvertThreadToFiber)(NULL); CK(g_main);
    g_fls = U(FlsAlloc)(NULL); CK(g_fls != FLS_OUT_OF_INDEXES);
    U(FlsSetValue)(g_fls, (PVOID)0x1111);
    g_n = 0;
    LPVOID f = U(CreateFiber)(0, fib_fls, NULL); CK(f);
    CK(U(FlsGetValue)(g_fls) == (PVOID)0x1111);
    U(SwitchToFiber)(f);
    CK(g_log[0] == 0x2222);
    CK(U(FlsGetValue)(g_fls) == (PVOID)0x1111);      /* FLS is per fiber */
    U(DeleteFiber)(f);
    CK(U(FlsFree)(g_fls));
    CK(U(ConvertFiberToThread)());
    return 0;
}
TEST(gap18_fiber_fls_is_per_fiber) { RUN(b_fls); }

static DWORD g_tls;
static VOID CALLBACK fib_tls(LPVOID p) {
    logv((LONG)(ULONG_PTR)TlsGetValue(g_tls));      /* TLS is per thread, shared by fibers */
    TlsSetValue(g_tls, (LPVOID)0x99);
    U(SwitchToFiber)(g_main);
    for (;;) U(SwitchToFiber)(g_main);
}
static DWORD b_tls(void) {
    g_main = U(ConvertThreadToFiber)(NULL); CK(g_main);
    g_tls = TlsAlloc(); TlsSetValue(g_tls, (LPVOID)0x55); g_n = 0;
    LPVOID f = U(CreateFiber)(0, fib_tls, NULL); CK(f);
    U(SwitchToFiber)(f);
    CK(g_log[0] == 0x55);
    CK(TlsGetValue(g_tls) == (LPVOID)0x99);
    U(DeleteFiber)(f); TlsFree(g_tls);
    CK(U(ConvertFiberToThread)());
    return 0;
}
TEST(gap18_fiber_tls_shared_across_fibers) { RUN(b_tls); }

static VOID CALLBACK fib_stack(LPVOID p) {
    NT_TIB *tib = (NT_TIB *)NtCurrentTeb();
    g_seen_stack_base = tib->StackBase; g_seen_stack_limit = tib->StackLimit;
    volatile char big[20000]; big[0] = 1; big[19999] = 2; (void)big;     /* uses stack beyond the first page */
    g_step++;
    U(SwitchToFiber)(g_main);
    for (;;) U(SwitchToFiber)(g_main);
}
static DWORD b_stack(void) {
    g_main = U(ConvertThreadToFiber)(NULL); CK(g_main);
    NT_TIB *tib = (NT_TIB *)NtCurrentTeb();
    void *mbase = tib->StackBase; g_step = 0;
    LPVOID f = U(CreateFiber)(0, fib_stack, NULL); CK(f);
    U(SwitchToFiber)(f);
    CK(g_step == 1);
    CK(g_seen_stack_base != mbase);               /* TEB stack bounds follow the running fiber */
    CK(tib->StackBase == mbase);
    CK(g_seen_stack_base > g_seen_stack_limit);
    U(DeleteFiber)(f);
    CK(U(ConvertFiberToThread)());
    return 0;
}
TEST(gap18_fiber_stack_bounds_follow_fiber) { RUN(b_stack); }

static DWORD b_small_stack(void) {
    g_main = U(ConvertThreadToFiber)(NULL); CK(g_main);
    g_step = 0;
    LPVOID f = U(CreateFiber)(64 * 1024, fib_simple, NULL); CK(f);
    U(SwitchToFiber)(f); CK(g_step == 1);
    U(DeleteFiber)(f);
    LPVOID g = U(CreateFiber)(8 * 1024 * 1024, fib_simple, NULL); CK(g);
    U(SwitchToFiber)(g); CK(g_step == 2);
    U(DeleteFiber)(g);
    CK(U(ConvertFiberToThread)());
    return 0;
}
TEST(gap18_fiber_create_stack_sizes) { RUN(b_small_stack); }

static DWORD b_createex(void) {
    CK(U(CreateFiberEx));
    g_main = U(ConvertThreadToFiber)(NULL); CK(g_main);
    static const struct { SIZE_T commit, reserve; DWORD flags; } c[] = {
        { 0, 0, 0 }, { 0, 0, FIBER_FLAG_FLOAT_SWITCH }, { 0x4000, 0x100000, 0 }, { 0x10000, 0x10000, 0 },
        { 0, 0x200000, 0 }, { 0x2000, 0, 0 }, { 0x1000, 0x400000, FIBER_FLAG_FLOAT_SWITCH } };
    for (unsigned i = 0; i < sizeof c / sizeof c[0]; i++) {
        g_step = 0;
        LPVOID f = U(CreateFiberEx)(c[i].commit, c[i].reserve, c[i].flags, fib_simple, (LPVOID)(ULONG_PTR)i);
        if (!f) { CK(GetLastError() != 0); continue; }
        U(SwitchToFiber)(f);
        CK(g_step == 1);
        CK(g_seen_data == (LPVOID)(ULONG_PTR)i);
        U(DeleteFiber)(f);
    }
    CK(U(ConvertFiberToThread)());
    return 0;
}
TEST(gap18_fiber_createex_param_matrix) { RUN(b_createex); }

static DWORD b_createex_huge(void) {
    static const SIZE_T huge[] = { (SIZE_T)0x7FFFFFFF0000ull, (SIZE_T)-1, (SIZE_T)0x800000000000ull };
    for (unsigned i = 0; i < 3; i++) {
        SetLastError(0);
        LPVOID f = U(CreateFiberEx)(0, huge[i], 0, fib_simple, NULL);
        if (f) { U(DeleteFiber)(f); return __LINE__; }
        CK(GetLastError() != 0);
    }
    for (unsigned i = 0; i < 3; i++) {
        SetLastError(0);
        LPVOID f = U(CreateFiberEx)(huge[i], 0, 0, fib_simple, NULL);
        if (f) { U(DeleteFiber)(f); return __LINE__; }
        CK(GetLastError() != 0);
    }
    return 0;
}
TEST(gap18_fiber_createex_huge_sizes_fail) { REQUIRE(U(CreateFiberEx)); RUN(b_createex_huge); }

static DWORD b_createex_commit_gt_reserve(void) {
    SetLastError(0);
    LPVOID f = U(CreateFiberEx)(0x200000, 0x10000, 0, fib_simple, NULL);
    t_note("commit > reserve: %p err %lu", f, GetLastError());
    if (f) U(DeleteFiber)(f);
    return 0;
}
TEST(gap18_fiber_createex_commit_gt_reserve) { REQUIRE(U(CreateFiberEx)); RUN(b_createex_commit_gt_reserve); }

static DWORD b_createex_flags(void) {
    static const DWORD fl[] = { 0, 1, 2, 4, 0x10, 0x80000000, 0xFFFFFFFF };
    for (unsigned i = 0; i < 7; i++) {
        SetLastError(0);
        LPVOID f = U(CreateFiberEx)(0, 0, fl[i], fib_simple, NULL);
        t_note("CreateFiberEx flags 0x%lx -> %p err %lu", fl[i], f, f ? 0 : GetLastError());
        if (fl[i] == 0 || fl[i] == 1) { if (!f) return __LINE__; }
        if (f) U(DeleteFiber)(f);
    }
    return 0;
}
TEST(gap18_fiber_createex_flag_values) { REQUIRE(U(CreateFiberEx)); RUN(b_createex_flags); }

static DWORD b_create_no_convert(void) {
    /* A fiber can be created (and deleted) from a thread that was never converted. */
    LPVOID f = U(CreateFiber)(0, fib_simple, NULL);
    CK(f);
    CK(!U(IsThreadAFiber)());
    U(DeleteFiber)(f);
    return 0;
}
TEST(gap18_fiber_create_delete_without_convert) { REQUIRE(U(CreateFiber)); RUN(b_create_no_convert); }

static DWORD b_many_fibers(void) {
    g_main = U(ConvertThreadToFiber)(NULL); CK(g_main);
    LPVOID fs[100];
    for (int i = 0; i < 100; i++) { fs[i] = U(CreateFiber)(0, fib_simple, (LPVOID)(ULONG_PTR)i); CK(fs[i]); }
    g_step = 0;
    for (int i = 0; i < 100; i++) { U(SwitchToFiber)(fs[i]); CK(g_seen_data == (LPVOID)(ULONG_PTR)i); }
    CK(g_step == 100);
    for (int i = 0; i < 100; i++) U(DeleteFiber)(fs[i]);
    CK(U(ConvertFiberToThread)());
    return 0;
}
TEST(gap18_fiber_many_fibers_100) { REQUIRE(U(CreateFiber)); RUN(b_many_fibers); }

static DWORD b_create_delete_loop(void) {
    for (int i = 0; i < 2000; i++) {
        LPVOID f = U(CreateFiber)(0, fib_simple, NULL);
        CK(f);
        U(DeleteFiber)(f);
    }
    return 0;
}
TEST(gap18_fiber_create_delete_loop_2000) { REQUIRE(U(CreateFiber)); RUN(b_create_delete_loop); }

static DWORD b_switch_to_self(void) {
    g_main = U(ConvertThreadToFiber)(NULL); CK(g_main);
    U(SwitchToFiber)(g_main);      /* switching to the current fiber is a no-op */
    CK(GetCurrentFiber() == g_main);
    CK(U(ConvertFiberToThread)());
    return 0;
}
TEST(gap18_fiber_switch_to_self) { REQUIRE(U(SwitchToFiber)); RUN(b_switch_to_self); }

static VOID CALLBACK fib_float(LPVOID p) {
    double d = 1.0; for (int i = 0; i < 10; i++) d *= 1.5;
    logv((LONG)(d * 100));
    U(SwitchToFiber)(g_main);
    for (;;) U(SwitchToFiber)(g_main);
}
static DWORD b_float_switch(void) {
    g_main = U(ConvertThreadToFiberEx)(NULL, FIBER_FLAG_FLOAT_SWITCH); CK(g_main);
    g_n = 0;
    volatile double keep = 3.25;
    LPVOID f = U(CreateFiberEx)(0, 0, FIBER_FLAG_FLOAT_SWITCH, fib_float, NULL); CK(f);
    U(SwitchToFiber)(f);
    CK(g_log[0] == 5766);
    CK(keep == 3.25);
    U(DeleteFiber)(f);
    CK(U(ConvertFiberToThread)());
    return 0;
}
TEST(gap18_fiber_float_switch_flag) { REQUIRE(U(ConvertThreadToFiberEx)); REQUIRE(U(CreateFiberEx)); RUN(b_float_switch); }

/* ------------------------- DeleteFiber ------------------------- */
static VOID CALLBACK fib_delete_self(LPVOID p) {
    g_step++;
    U(DeleteFiber)(GetCurrentFiber());       /* the running fiber deletes itself: the thread exits */
    g_step += 100;                           /* never reached */
}
static DWORD b_delete_self(void) {
    g_main = U(ConvertThreadToFiber)(NULL); CK(g_main);
    g_step = 0;
    LPVOID f = U(CreateFiber)(0, fib_delete_self, NULL); CK(f);
    U(SwitchToFiber)(f);
    return 55;                               /* not reached: thread terminated with exit code 1 */
}
TEST(gap18_fiber_delete_current_exits_thread) {
    REQUIRE(U(DeleteFiber)); REQUIRE(U(CreateFiber));
    DWORD code = 0; fb_run(b_delete_self, &code);
    EQ(code, 1);
    EQ(g_step, 1);
}
static VOID CALLBACK fib_return(LPVOID p) { g_step++; }
static DWORD b_fiber_returns(void) {
    g_main = U(ConvertThreadToFiber)(NULL); CK(g_main);
    g_step = 0;
    LPVOID f = U(CreateFiber)(0, fib_return, NULL); CK(f);
    U(SwitchToFiber)(f);
    return 66;      /* a fiber function that returns terminates the thread */
}
TEST(gap18_fiber_return_from_fiber_exits_thread) {
    REQUIRE(U(CreateFiber));
    DWORD code = 99; fb_run(b_fiber_returns, &code);
    EQ(g_step, 1);
    CHECK(code != 66);
    t_note("thread exit code after fiber function returned: %lu", code);
}
static DWORD b_delete_main_fiber(void) {
    LPVOID m = U(ConvertThreadToFiber)(NULL); CK(m);
    U(DeleteFiber)(m);        /* deleting the thread's own (converted) fiber ends the thread */
    return 77;
}
TEST(gap18_fiber_delete_converted_fiber_exits_thread) {
    REQUIRE(U(DeleteFiber)); REQUIRE(U(ConvertThreadToFiber));
    DWORD code = 0; fb_run(b_delete_main_fiber, &code);
    EQ(code, 1);
}
static DWORD b_delete_unstarted_then_other(void) {
    g_main = U(ConvertThreadToFiber)(NULL); CK(g_main);
    LPVOID a = U(CreateFiber)(0, fib_simple, NULL), b = U(CreateFiber)(0, fib_simple, NULL);
    CK(a && b);
    U(DeleteFiber)(a);
    g_step = 0;
    U(SwitchToFiber)(b);
    CK(g_step == 1);
    U(DeleteFiber)(b);
    CK(U(ConvertFiberToThread)());
    return 0;
}
TEST(gap18_fiber_delete_unstarted_fiber) { REQUIRE(U(DeleteFiber)); RUN(b_delete_unstarted_then_other); }

static DWORD g_fls_cb_hits; static DWORD g_fls_idx2;
static VOID WINAPI fls_cb(PVOID p) { if (p == (PVOID)0x4242) g_fls_cb_hits++; }
static VOID CALLBACK fib_flsval(LPVOID p) {
    U(FlsSetValue)(g_fls_idx2, (PVOID)0x4242);
    U(SwitchToFiber)(g_main);
    for (;;) U(SwitchToFiber)(g_main);
}
static DWORD b_delete_runs_fls_cb(void) {
    g_main = U(ConvertThreadToFiber)(NULL); CK(g_main);
    g_fls_idx2 = U(FlsAlloc)(fls_cb); CK(g_fls_idx2 != FLS_OUT_OF_INDEXES);
    g_fls_cb_hits = 0;
    LPVOID f = U(CreateFiber)(0, fib_flsval, NULL); CK(f);
    U(SwitchToFiber)(f);
    CK(g_fls_cb_hits == 0);
    U(DeleteFiber)(f);
    CK(g_fls_cb_hits == 1);            /* FLS callbacks run when the fiber is deleted */
    U(FlsFree)(g_fls_idx2);
    CK(U(ConvertFiberToThread)());
    return 0;
}
TEST(gap18_fiber_delete_invokes_fls_callbacks) { REQUIRE(U(FlsAlloc)); REQUIRE(U(DeleteFiber)); RUN(b_delete_runs_fls_cb); }

static DWORD b_fiber_data_roundtrip(void) {
    static const ULONG_PTR vals[] = { 0, 1, 0xDEADBEEF, (ULONG_PTR)-1, 0x7FFFFFFFFFFFFFF0ull };
    for (unsigned i = 0; i < 5; i++) {
        LPVOID f = U(ConvertThreadToFiber)((LPVOID)vals[i]); CK(f);
        CK(GetFiberData() == (LPVOID)vals[i]);
        CK(U(ConvertFiberToThread)());
    }
    return 0;
}
TEST(gap18_fiber_data_pointer_roundtrip) { REQUIRE(U(ConvertThreadToFiber)); RUN(b_fiber_data_roundtrip); }

static DWORD b_cross_thread_switch_safe_state(void) {
    /* A fiber created on one thread has its own handle value distinct per creation. */
    LPVOID a = U(CreateFiber)(0, fib_simple, NULL), b = U(CreateFiber)(0, fib_simple, NULL);
    CK(a && b && a != b);
    U(DeleteFiber)(a); U(DeleteFiber)(b);
    return 0;
}
TEST(gap18_fiber_handles_distinct) { REQUIRE(U(CreateFiber)); RUN(b_cross_thread_switch_safe_state); }

static VOID CALLBACK fib_exc(LPVOID p) {
    /* an exception handled inside a fiber (vectored) does not disturb the thread */
    g_step++;
    U(SwitchToFiber)(g_main);
    for (;;) U(SwitchToFiber)(g_main);
}
static DWORD b_getlasterror_per_thread(void) {
    g_main = U(ConvertThreadToFiber)(NULL); CK(g_main);
    SetLastError(0x1111);
    LPVOID f = U(CreateFiber)(0, fib_exc, NULL); CK(f);
    SetLastError(0x2222);
    U(SwitchToFiber)(f);
    CK(GetLastError() == 0x2222);       /* last error lives in the TEB (per thread, not per fiber) */
    U(DeleteFiber)(f);
    CK(U(ConvertFiberToThread)());
    return 0;
}
TEST(gap18_fiber_lasterror_is_per_thread) { REQUIRE(U(CreateFiber)); RUN(b_getlasterror_per_thread); }

static DWORD b_getcurrentthreadid_in_fiber(void) {
    g_main = U(ConvertThreadToFiber)(NULL); CK(g_main);
    DWORD tid = GetCurrentThreadId();
    DWORD got = 0;
    VOID CALLBACK fb(LPVOID p) { *(DWORD *)p = GetCurrentThreadId(); U(SwitchToFiber)(g_main); for (;;) U(SwitchToFiber)(g_main); }
    LPVOID f = U(CreateFiber)(0, fb, &got); CK(f);
    U(SwitchToFiber)(f);
    CK(got == tid);
    U(DeleteFiber)(f);
    CK(U(ConvertFiberToThread)());
    return 0;
}
TEST(gap18_fiber_same_thread_id) { REQUIRE(U(CreateFiber)); RUN(b_getcurrentthreadid_in_fiber); }

TEST(gap18_fiber_k32_exports) {
    REQUIRE(UK(CreateFiber)); REQUIRE(UK(CreateFiberEx)); REQUIRE(UK(SwitchToFiber)); REQUIRE(UK(DeleteFiber));
    REQUIRE(UB(CreateFiber)); REQUIRE(UB(CreateFiberEx)); REQUIRE(UB(SwitchToFiber)); REQUIRE(UB(DeleteFiber));
    CHECK(1);
}
