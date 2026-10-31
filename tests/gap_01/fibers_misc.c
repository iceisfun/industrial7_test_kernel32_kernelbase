/* ConvertThreadToFiber(Ex), ConvertFiberToThread, CallEnclave, CeipIsOptedIn,
 * Convert{Auxiliary,Performance}Counter*, Beep (export only: never called, it would sound the speaker). */
#include "gap1.h"

/* Fiber state is per thread: every test body runs on a fresh helper thread. */
#define FIBTEST(name, fn) \
    TEST(name) { REQUIRE(U(ConvertThreadToFiberEx)); REQUIRE(U(ConvertFiberToThread)); \
        DWORD r = g1_run_thread(fn); CHECKF(r == 0, "failure bits 0x%lx", r); }
#define FAILIF(c, bit) do { if (c) res |= (bit); } while (0)

static DWORD fn_flags_invalid(void) {
    DWORD res = 0;
    static const DWORD bad[] = { 2, 3, 4, 0x10, 0x100, 0x80000000, 0xFFFFFFFE, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0);
        LPVOID f = U(ConvertThreadToFiberEx)((LPVOID)1, bad[i]);
        if (f != NULL) { res |= 1; U(ConvertFiberToThread)(); }
        else if (GetLastError() != ERROR_INVALID_PARAMETER) res |= 2;
    }
    SetLastError(0);
    if (U(ConvertFiberToThread)()) res |= 4;                 /* still not a fiber */
    else if (GetLastError() != ERROR_ALREADY_THREAD) res |= 8;
    return res;
}
FIBTEST(gap1_convertthreadtofiberex_invalid_flags, fn_flags_invalid)

static DWORD fn_basic(void) {
    DWORD res = 0;
    LPVOID f = U(ConvertThreadToFiberEx)((LPVOID)0x1234, 0);
    if (!f) return 1;
    FAILIF(GetCurrentFiber() != f, 2);
    FAILIF(GetFiberData() != (PVOID)0x1234, 4);
    if (U(IsThreadAFiber)) FAILIF(!U(IsThreadAFiber)(), 8);
    if (!U(ConvertFiberToThread)()) res |= 16;
    FAILIF(GetCurrentFiber() != NULL, 32);
    if (U(IsThreadAFiber)) FAILIF(U(IsThreadAFiber)(), 64);
    return res;
}
FIBTEST(gap1_convertthreadtofiberex_basic_state, fn_basic)

static DWORD fn_float(void) {
    DWORD res = 0;
    LPVOID f = U(ConvertThreadToFiberEx)((LPVOID)0xABCD, FIBER_FLAG_FLOAT_SWITCH);
    if (!f) return 1;
    FAILIF(GetFiberData() != (PVOID)0xABCD, 2);
    FAILIF(GetCurrentFiber() != f, 4);
    FAILIF(!U(ConvertFiberToThread)(), 8);
    return res;
}
FIBTEST(gap1_convertthreadtofiberex_float_switch_flag, fn_float)

static DWORD fn_already(void) {
    DWORD res = 0;
    LPVOID f = U(ConvertThreadToFiberEx)(NULL, 0);
    if (!f) return 1;
    for (int i = 0; i < 3; i++) {
        SetLastError(0);
        LPVOID g = U(ConvertThreadToFiberEx)(NULL, 0);
        if (g) res |= 2;
        else if (GetLastError() != ERROR_ALREADY_FIBER) res |= 4;
    }
    SetLastError(0);
    if (U(ConvertThreadToFiberEx)(NULL, FIBER_FLAG_FLOAT_SWITCH)) res |= 8;
    else if (GetLastError() != ERROR_ALREADY_FIBER) res |= 16;
    FAILIF(GetCurrentFiber() != f, 32);                       /* failed attempts keep the original */
    FAILIF(!U(ConvertFiberToThread)(), 64);
    return res;
}
FIBTEST(gap1_convertthreadtofiberex_already_fiber, fn_already)

static DWORD fn_already_flags_checked_first(void) {
    /* invalid flags are rejected before the already-a-fiber test */
    DWORD res = 0;
    if (!U(ConvertThreadToFiberEx)(NULL, 0)) return 1;
    SetLastError(0);
    if (U(ConvertThreadToFiberEx)(NULL, 2)) res |= 2;
    else if (GetLastError() != ERROR_INVALID_PARAMETER) res |= 4;
    U(ConvertFiberToThread)();
    return res;
}
FIBTEST(gap1_convertthreadtofiberex_flags_checked_before_state, fn_already_flags_checked_first)

static DWORD fn_not_fiber(void) {
    DWORD res = 0;
    for (int i = 0; i < 3; i++) {
        SetLastError(0);
        if (U(ConvertFiberToThread)()) res |= 1;
        else if (GetLastError() != ERROR_ALREADY_THREAD) res |= 2;
    }
    return res;
}
FIBTEST(gap1_convertfibertothread_not_a_fiber, fn_not_fiber)

static DWORD fn_double_back(void) {
    DWORD res = 0;
    if (!U(ConvertThreadToFiberEx)(NULL, 0)) return 1;
    FAILIF(!U(ConvertFiberToThread)(), 2);
    SetLastError(0);
    if (U(ConvertFiberToThread)()) res |= 4;
    else if (GetLastError() != ERROR_ALREADY_THREAD) res |= 8;
    return res;
}
FIBTEST(gap1_convertfibertothread_twice, fn_double_back)

static DWORD fn_success_keeps_error(void) {
    DWORD res = 0;
    SetLastError(0x2468);
    LPVOID f = U(ConvertThreadToFiberEx)(NULL, 0);
    if (!f) return 1;
    /* ConvertThreadToFiberEx does not touch last error on success */
    FAILIF(GetLastError() != 0x2468, 2);
    SetLastError(0x1357);
    FAILIF(!U(ConvertFiberToThread)(), 4);
    FAILIF(GetLastError() != 0x1357, 8);
    return res;
}
FIBTEST(gap1_convertfiber_success_preserves_last_error, fn_success_keeps_error)

static DWORD fn_cycle(void) {
    DWORD res = 0;
    for (int i = 0; i < 200; i++) {
        LPVOID f = U(ConvertThreadToFiberEx)((LPVOID)(ULONG_PTR)(i + 1), i & 1);
        if (!f) { res |= 1; break; }
        if (GetFiberData() != (PVOID)(ULONG_PTR)(i + 1)) { res |= 2; }
        if (!U(ConvertFiberToThread)()) { res |= 4; break; }
    }
    return res;
}
FIBTEST(gap1_convertfiber_repeat_cycle, fn_cycle)

static DWORD fn_param_values(void) {
    DWORD res = 0;
    static const ULONG_PTR v[] = { 0, 1, 0x7FFFFFFF, 0x80000000, (ULONG_PTR)-1, (ULONG_PTR)0x123456789ABCDEF0ull };
    for (size_t i = 0; i < sizeof v / sizeof v[0]; i++) {
        LPVOID f = U(ConvertThreadToFiberEx)((LPVOID)v[i], 0);
        if (!f) { res |= 1; continue; }
        if ((ULONG_PTR)GetFiberData() != v[i]) res |= 2;
        U(ConvertFiberToThread)();
    }
    return res;
}
FIBTEST(gap1_convertthreadtofiberex_parameter_values, fn_param_values)

static DWORD fn_k32_plain(void) {
    DWORD res = 0;
    LPVOID (WINAPI *p)(LPVOID) = (LPVOID (WINAPI *)(LPVOID))t_proc("ConvertThreadToFiber");
    if (!p) return 1;
    LPVOID f = p((LPVOID)0x77);
    if (!f) return 2;
    FAILIF(GetFiberData() != (PVOID)0x77, 4);
    FAILIF(GetCurrentFiber() != f, 8);
    SetLastError(0);
    FAILIF(p((LPVOID)1) != NULL, 16);
    FAILIF(GetLastError() != ERROR_ALREADY_FIBER, 32);
    FAILIF(!U(ConvertFiberToThread)(), 64);
    return res;
}
FIBTEST(gap1_convertthreadtofiber_plain, fn_k32_plain)

static LPVOID g_main_fiber; static volatile LONG g_fib_ran; static volatile ULONG_PTR g_fib_data;
static VOID WINAPI fiber_proc(LPVOID p) {
    g_fib_ran++; g_fib_data = (ULONG_PTR)GetFiberData();
    SwitchToFiber(g_main_fiber);
    /* never resumed */
    for (;;) SwitchToFiber(g_main_fiber);
}
static DWORD fn_switch(void) {
    DWORD res = 0;
    g_fib_ran = 0; g_fib_data = 0;
    if (!U(CreateFiber) || !U(SwitchToFiber) || !U(DeleteFiber)) return 0;
    g_main_fiber = U(ConvertThreadToFiberEx)((LPVOID)0x42, 0);
    if (!g_main_fiber) return 1;
    LPVOID f = U(CreateFiber)(0, fiber_proc, (LPVOID)0x99);
    if (!f) { U(ConvertFiberToThread)(); return 2; }
    U(SwitchToFiber)(f);
    FAILIF(g_fib_ran != 1, 4);
    FAILIF(g_fib_data != 0x99, 8);
    FAILIF(GetCurrentFiber() != g_main_fiber, 16);
    FAILIF(GetFiberData() != (PVOID)0x42, 32);
    U(DeleteFiber)(f);
    FAILIF(!U(ConvertFiberToThread)(), 64);
    return res;
}
FIBTEST(gap1_convertthreadtofiber_switch_to_created_fiber_and_back, fn_switch)

static DWORD fn_cannot_delete_converted_after_back(void) {
    /* a converted thread can hand control to a fiber created before conversion was undone */
    DWORD res = 0;
    LPVOID f = U(ConvertThreadToFiberEx)(NULL, 0);
    FAILIF(!f, 1);
    FAILIF(f == GetCurrentFiber() ? 0 : 1, 2);
    U(ConvertFiberToThread)();
    /* convert again: gets a (possibly identical) valid fiber again */
    LPVOID g = U(ConvertThreadToFiberEx)(NULL, 0);
    FAILIF(!g, 4);
    FAILIF(GetCurrentFiber() != g, 8);
    U(ConvertFiberToThread)();
    return res;
}
FIBTEST(gap1_convertthreadtofiber_reconvert_after_back, fn_cannot_delete_converted_after_back)

static HANDLE g_go;
static LPVOID g_f1, g_f2; static ULONG_PTR g_d1, g_d2;
static DWORD WINAPI two_thr(LPVOID p) {
    ULONG_PTR k = (ULONG_PTR)p;
    LPVOID f = U(ConvertThreadToFiberEx)((LPVOID)(0x1000 + k), 0);
    if (k == 1) { g_f1 = f; g_d1 = (ULONG_PTR)GetFiberData(); } else { g_f2 = f; g_d2 = (ULONG_PTR)GetFiberData(); }
    WaitForSingleObject(g_go, 5000);
    /* leave the thread still converted: exiting a fiber thread is legal */
    return f ? 0 : 1;
}
TEST(gap1_convertthreadtofiber_independent_per_thread) {
    REQUIRE(U(ConvertThreadToFiberEx));
    g_go = CreateEventW(NULL, TRUE, FALSE, NULL);
    g_f1 = g_f2 = NULL;
    HANDLE h[2];
    h[0] = CreateThread(NULL, 0, two_thr, (LPVOID)1, 0, NULL);
    h[1] = CreateThread(NULL, 0, two_thr, (LPVOID)2, 0, NULL);
    Sleep(200);
    SetEvent(g_go);
    WaitForMultipleObjects(2, h, TRUE, 10000);
    DWORD c1 = 9, c2 = 9; GetExitCodeThread(h[0], &c1); GetExitCodeThread(h[1], &c2);
    EXPECT_EQ(c1, 0); EXPECT_EQ(c2, 0);
    CHECK(g_f1 != NULL && g_f2 != NULL && g_f1 != g_f2);
    EXPECT_EQ(g_d1, 0x1001); EXPECT_EQ(g_d2, 0x1002);
    CloseHandle(h[0]); CloseHandle(h[1]); CloseHandle(g_go);
}
static DWORD WINAPI exit_as_fiber(LPVOID p) {
    return U(ConvertThreadToFiberEx)(NULL, 0) ? 77 : 1;
}
TEST(gap1_convertthreadtofiber_thread_exit_while_fiber) {
    REQUIRE(U(ConvertThreadToFiberEx));
    for (int i = 0; i < 20; i++) {
        HANDLE h = CreateThread(NULL, 0, exit_as_fiber, NULL, 0, NULL);
        WaitForSingleObject(h, 10000);
        DWORD c = 0; GetExitCodeThread(h, &c);
        CHECK(c == 77);
        CloseHandle(h);
    }
}
TEST(gap1_convertthreadtofiber_main_thread_not_a_fiber_by_default) {
    REQUIRE(U(ConvertFiberToThread));
    /* the harness thread is a plain thread; run the probe on a helper to keep fiber state of main untouched */
    DWORD r = g1_run_thread(fn_not_fiber);
    EXPECT_EQ(r, 0);
}
TEST(gap1_convertthreadtofiber_exports_present) {
    CHECK(t_proc_k32("ConvertThreadToFiber") != NULL);
    CHECK(t_proc_k32("ConvertThreadToFiberEx") != NULL);
    CHECK(t_proc_k32("ConvertFiberToThread") != NULL);
}

/* ---------- CallEnclave ---------- */
UDECL(BOOL, CallEnclave, (PVOID (WINAPI *Routine)(PVOID), PVOID Param, BOOL WaitForThread, PVOID *ReturnValue));
static volatile DWORD g_enc_tid; static volatile LONG g_enc_calls;
static PVOID WINAPI enc_add1(PVOID p) { g_enc_tid = GetCurrentThreadId(); g_enc_calls++; return (PVOID)((ULONG_PTR)p + 1); }
static PVOID WINAPI enc_ident(PVOID p) { g_enc_calls++; return p; }
TEST(gap1_callenclave_non_enclave_routine_runs_inline) {
    REQUIRE(UN(CallEnclave));
    if (t_is_wine()) { t_skip("CallEnclave unsupported under wine"); return; }
    PVOID ret = (PVOID)0x55; g_enc_calls = 0;
    BOOL r = UN(CallEnclave)(enc_add1, (PVOID)41, TRUE, &ret);
    CHECKF(r, "err %lu", GetLastError());
    if (r) { EXPECT_EQ(ret, 42); EXPECT_EQ(g_enc_calls, 1); EXPECT_EQ(g_enc_tid, GetCurrentThreadId()); }
}
TEST(gap1_callenclave_wait_flag_false) {
    REQUIRE(UN(CallEnclave));
    if (t_is_wine()) { t_skip("CallEnclave unsupported under wine"); return; }
    PVOID ret = (PVOID)0x55; g_enc_calls = 0;
    BOOL r = UN(CallEnclave)(enc_ident, (PVOID)0xBEEF, FALSE, &ret);
    CHECKF(r, "err %lu", GetLastError());
    if (r) { EXPECT_EQ(ret, 0xBEEF); EXPECT_EQ(g_enc_calls, 1); }
}
TEST(gap1_callenclave_param_values) {
    REQUIRE(UN(CallEnclave));
    if (t_is_wine()) { t_skip("CallEnclave unsupported under wine"); return; }
    static const ULONG_PTR v[] = { 0, 1, 0xFFFFFFFF, 0x100000000ull, (ULONG_PTR)-1, 0x123456789ABCDEF0ull };
    for (size_t i = 0; i < sizeof v / sizeof v[0]; i++) {
        PVOID ret = (PVOID)0x55;
        BOOL r = UN(CallEnclave)(enc_ident, (PVOID)v[i], TRUE, &ret);
        CHECKF(r && (ULONG_PTR)ret == v[i], "i=%zu", i);
    }
}
TEST(gap1_callenclave_repeated) {
    REQUIRE(UN(CallEnclave));
    if (t_is_wine()) { t_skip("CallEnclave unsupported under wine"); return; }
    g_enc_calls = 0;
    for (int i = 0; i < 1000; i++) {
        PVOID ret = NULL;
        if (!UN(CallEnclave)(enc_add1, (PVOID)(ULONG_PTR)i, (i & 1), &ret) || (ULONG_PTR)ret != (ULONG_PTR)i + 1) { CHECKF(0, "iter %d", i); return; }
    }
    EXPECT_EQ(g_enc_calls, 1000);
}
TEST(gap1_callenclave_exported_by_kernelbase) {
    if (t_is_wine()) { t_skip("export missing under wine"); return; }
    CHECK(t_proc_kb("CallEnclave") != NULL);
}

/* ---------- CeipIsOptedIn ---------- */
UDECL(BOOL, CeipIsOptedIn, (void));
TEST(gap1_ceipisoptedin_returns_bool) {
    REQUIRE(UN(CeipIsOptedIn));
    if (t_is_wine()) { t_skip("not implemented under wine"); return; }
    DWORD v = UN(CeipIsOptedIn)();
    t_note("CeipIsOptedIn=%lu", v);
    CHECK(v == 0 || v == 1);
}
TEST(gap1_ceipisoptedin_stable_across_calls) {
    REQUIRE(UN(CeipIsOptedIn));
    if (t_is_wine()) { t_skip("not implemented under wine"); return; }
    DWORD a = UN(CeipIsOptedIn)();
    for (int i = 0; i < 50; i++) { DWORD b = UN(CeipIsOptedIn)(); if (b != a) { CHECKF(0, "changed %lu -> %lu", a, b); break; } }
}
static DWORD WINAPI ceip_thr(LPVOID p) { return UN(CeipIsOptedIn)() ? 1 : 0; }
TEST(gap1_ceipisoptedin_same_on_other_thread) {
    REQUIRE(UN(CeipIsOptedIn));
    if (t_is_wine()) { t_skip("not implemented under wine"); return; }
    DWORD a = UN(CeipIsOptedIn)() ? 1 : 0;
    HANDLE h = CreateThread(NULL, 0, ceip_thr, NULL, 0, NULL);
    WaitForSingleObject(h, 10000);
    DWORD c = 9; GetExitCodeThread(h, &c); CloseHandle(h);
    EXPECT_EQ(c, a);
}
TEST(gap1_ceipisoptedin_exported_by_both) {
    if (t_is_wine()) { t_skip("export missing under wine"); return; }
    CHECK(t_proc_k32("CeipIsOptedIn") != NULL);
    CHECK(t_proc_kb("CeipIsOptedIn") != NULL);
}

/* ---------- Auxiliary counter conversion ---------- */
static int hr_known(HRESULT hr) {
    if (hr == S_OK || hr == E_NOTIMPL || hr == E_BOUNDS) return 1;
    return ((DWORD)hr >> 16) == 0x8007;
}
TEST(gap1_convertauxiliarycounter_valid_pointers) {
    REQUIRE(U(ConvertAuxiliaryCounterToPerformanceCounter));
    if (t_is_wine()) { t_skip("not implemented under wine"); return; }
    ULONGLONG pc = 0x77, err = 0x77;
    HRESULT hr = U(ConvertAuxiliaryCounterToPerformanceCounter)(1000, &pc, &err);
    t_note("aux->pc: hr=0x%08lx pc=%llu err=%llu", (DWORD)hr, pc, err);
    CHECKF(hr_known(hr), "hr=0x%08lx", (DWORD)hr);
}
TEST(gap1_convertperformancecounter_valid_pointers) {
    REQUIRE(U(ConvertPerformanceCounterToAuxiliaryCounter));
    if (t_is_wine()) { t_skip("not implemented under wine"); return; }
    LARGE_INTEGER q; QueryPerformanceCounter(&q);
    ULONGLONG ac = 0x77, err = 0x77;
    HRESULT hr = U(ConvertPerformanceCounterToAuxiliaryCounter)(q.QuadPart, &ac, &err);
    t_note("pc->aux: hr=0x%08lx aux=%llu err=%llu", (DWORD)hr, ac, err);
    CHECKF(hr_known(hr), "hr=0x%08lx", (DWORD)hr);
}
TEST(gap1_convertauxiliarycounter_optional_error_pointer) {
    REQUIRE(U(ConvertAuxiliaryCounterToPerformanceCounter)); REQUIRE(U(ConvertPerformanceCounterToAuxiliaryCounter));
    if (t_is_wine()) { t_skip("not implemented under wine"); return; }
    ULONGLONG v = 0;
    HRESULT h1 = U(ConvertAuxiliaryCounterToPerformanceCounter)(1000, &v, NULL);
    HRESULT h2 = U(ConvertPerformanceCounterToAuxiliaryCounter)(1000, &v, NULL);
    t_note("NULL error ptr: aux->pc 0x%08lx, pc->aux 0x%08lx", (DWORD)h1, (DWORD)h2);
    CHECK(hr_known(h1)); CHECK(hr_known(h2));
}
TEST(gap1_convertauxiliarycounter_null_result_pointer) {
    REQUIRE(U(ConvertAuxiliaryCounterToPerformanceCounter)); REQUIRE(U(ConvertPerformanceCounterToAuxiliaryCounter));
    if (t_is_wine()) { t_skip("not implemented under wine"); return; }
    HRESULT h1 = U(ConvertAuxiliaryCounterToPerformanceCounter)(1000, NULL, NULL);
    HRESULT h2 = U(ConvertPerformanceCounterToAuxiliaryCounter)(1000, NULL, NULL);
    t_note("NULL result ptr: aux->pc 0x%08lx, pc->aux 0x%08lx", (DWORD)h1, (DWORD)h2);
    CHECK(FAILED(h1) || h1 == S_OK);
    CHECK(FAILED(h2) || h2 == S_OK);
}
TEST(gap1_convertauxiliarycounter_extremes) {
    REQUIRE(U(ConvertAuxiliaryCounterToPerformanceCounter)); REQUIRE(U(ConvertPerformanceCounterToAuxiliaryCounter));
    if (t_is_wine()) { t_skip("not implemented under wine"); return; }
    static const ULONGLONG v[] = { 0, 1, 0x7FFFFFFFFFFFFFFFull, 0x8000000000000000ull, 0xFFFFFFFFFFFFFFFFull };
    for (size_t i = 0; i < sizeof v / sizeof v[0]; i++) {
        ULONGLONG o = 0x77, e = 0x77;
        HRESULT a = U(ConvertAuxiliaryCounterToPerformanceCounter)(v[i], &o, &e);
        HRESULT b = U(ConvertPerformanceCounterToAuxiliaryCounter)(v[i], &o, &e);
        CHECKF(hr_known(a), "aux->pc(%llx) hr=0x%08lx", v[i], (DWORD)a);
        CHECKF(hr_known(b), "pc->aux(%llx) hr=0x%08lx", v[i], (DWORD)b);
    }
}
TEST(gap1_convertauxiliarycounter_roundtrip_when_supported) {
    REQUIRE(U(ConvertAuxiliaryCounterToPerformanceCounter)); REQUIRE(U(ConvertPerformanceCounterToAuxiliaryCounter));
    if (t_is_wine()) { t_skip("not implemented under wine"); return; }
    LARGE_INTEGER q; QueryPerformanceCounter(&q);
    ULONGLONG aux = 0, e1 = 0;
    HRESULT h1 = U(ConvertPerformanceCounterToAuxiliaryCounter)(q.QuadPart, &aux, &e1);
    if (h1 != S_OK) { t_skip("auxiliary counter unsupported on this host (0x%08lx)", (DWORD)h1); return; }
    ULONGLONG pc = 0, e2 = 0;
    HRESULT h2 = U(ConvertAuxiliaryCounterToPerformanceCounter)(aux, &pc, &e2);
    CHECKF(h2 == S_OK, "hr=0x%08lx", (DWORD)h2);
    LONGLONG d = (LONGLONG)pc - q.QuadPart; if (d < 0) d = -d;
    CHECKF((ULONGLONG)d <= e1 + e2 + 16, "diff %lld err %llu/%llu", d, e1, e2);
}
TEST(gap1_convertauxiliarycounter_consistent_unsupported_result) {
    REQUIRE(U(ConvertAuxiliaryCounterToPerformanceCounter));
    if (t_is_wine()) { t_skip("not implemented under wine"); return; }
    ULONGLONG o, e;
    HRESULT first = U(ConvertAuxiliaryCounterToPerformanceCounter)(5, &o, &e);
    for (int i = 0; i < 20; i++) {
        HRESULT h = U(ConvertAuxiliaryCounterToPerformanceCounter)(5, &o, &e);
        if ((h == S_OK) != (first == S_OK)) { CHECKF(0, "support flipped 0x%08lx -> 0x%08lx", (DWORD)first, (DWORD)h); break; }
    }
}
TEST(gap1_convertauxiliarycounter_does_not_set_last_error) {
    REQUIRE(U(ConvertAuxiliaryCounterToPerformanceCounter));
    if (t_is_wine()) { t_skip("not implemented under wine"); return; }
    ULONGLONG o, e;
    SET_ERR(0x6677);
    U(ConvertAuxiliaryCounterToPerformanceCounter)(5, &o, &e);
    DWORD le = GetLastError();
    t_note("last error after call: %lu", le);
}
TEST(gap1_convertauxiliarycounter_export_kernelbase) {
    CHECK(t_proc_kb("ConvertAuxiliaryCounterToPerformanceCounter") != NULL);
    CHECK(t_proc_kb("ConvertPerformanceCounterToAuxiliaryCounter") != NULL);
}

/* ---------- Beep ---------- */
/* Beep is never invoked: it opens \Device\Beep and drives the PC speaker / default sound device for the
 * requested duration, and no argument error is rejected before the driver call. Only the export is checked. */
TEST(gap1_beep_exported_not_called) {
    CHECK(t_proc_k32("Beep") != NULL);
    CHECK(t_proc_kb("Beep") != NULL);
}
