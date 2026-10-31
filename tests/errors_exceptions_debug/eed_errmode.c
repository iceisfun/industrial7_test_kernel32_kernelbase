/* SetErrorMode / GetErrorMode / SetThreadErrorMode / GetThreadErrorMode.
 * Process error mode is process-global: everything that changes it runs in a child process. */
#include "eed.h"

#ifndef SEM_NOALIGNMENTFAULTEXCEPT
#define SEM_NOALIGNMENTFAULTEXCEPT 0x0004
#endif
#ifndef SEM_NOOPENFILEERRORBOX
#define SEM_NOOPENFILEERRORBOX 0x8000
#endif

#define BAD(n) do { return (n); } while (0)
#define NEED(c, n) do { if (!(c)) return (n); } while (0)

/* process mode: sequence of values, each Set returns the previous, Get returns the current */
CHILD(eed_em_seq) {
    UINT (WINAPI *set)(UINT) = (UINT (WINAPI *)(UINT))t_proc("SetErrorMode");
    UINT (WINAPI *get)(void) = (UINT (WINAPI *)(void))t_proc("GetErrorMode");
    NEED(set && get, 100);
    static const UINT seq[] = { 0, 1, 2, 3, 0x8000, 0x8001, 0x8002, 0x8003, 3, 0, 0x8003, 2, 1, 0 };
    UINT prev = get();
    for (size_t i = 0; i < sizeof seq / sizeof *seq; i++) {
        UINT r = set(seq[i]);
        NEED(r == prev, 1000 + (int)i);
        NEED(get() == seq[i], 2000 + (int)i);
        prev = seq[i];
    }
    return 0;
}
CHILD(eed_em_sticky_align) {
    UINT (WINAPI *set)(UINT) = (UINT (WINAPI *)(UINT))t_proc("SetErrorMode");
    UINT (WINAPI *get)(void) = (UINT (WINAPI *)(void))t_proc("GetErrorMode");
    NEED(set && get, 100);
    set(0);
    set(SEM_NOALIGNMENTFAULTEXCEPT);
    NEED(get() == SEM_NOALIGNMENTFAULTEXCEPT, 1);
    UINT r = set(0);                        /* the alignment bit can never be cleared once set */
    NEED(r == SEM_NOALIGNMENTFAULTEXCEPT, 2);
    NEED((get() & SEM_NOALIGNMENTFAULTEXCEPT) != 0, 3);
    r = set(SEM_FAILCRITICALERRORS);
    NEED((get() & 5) == 5, 4);
    r = set(SEM_NOGPFAULTERRORBOX);
    NEED(get() == (SEM_NOGPFAULTERRORBOX | SEM_NOALIGNMENTFAULTEXCEPT), 5);
    return 0;
}
CHILD(eed_em_roundtrip_bits) {
    UINT (WINAPI *set)(UINT) = (UINT (WINAPI *)(UINT))t_proc("SetErrorMode");
    UINT (WINAPI *get)(void) = (UINT (WINAPI *)(void))t_proc("GetErrorMode");
    NEED(set && get, 100);
    static const UINT bits[] = { 1, 2, 0x8000, 3, 0x8001, 0x8002, 0x8003 };
    for (size_t i = 0; i < sizeof bits / sizeof *bits; i++) {
        set(0); NEED(get() == 0, 10 + (int)i);
        set(bits[i]); NEED(get() == bits[i], 30 + (int)i);
        set(0); NEED(get() == 0, 50 + (int)i);
    }
    return 0;
}
CHILD(eed_em_get_no_side_effect) {
    UINT (WINAPI *set)(UINT) = (UINT (WINAPI *)(UINT))t_proc("SetErrorMode");
    UINT (WINAPI *get)(void) = (UINT (WINAPI *)(void))t_proc("GetErrorMode");
    NEED(set && get, 100);
    set(0x8003);
    for (int i = 0; i < 100; i++) NEED(get() == 0x8003, 1);
    return 0;
}
/* thread modes */
CHILD(eed_em_thread_basic) {
    BOOL (WINAPI *set)(DWORD, LPDWORD) = (BOOL (WINAPI *)(DWORD, LPDWORD))t_proc("SetThreadErrorMode");
    DWORD (WINAPI *get)(void) = (DWORD (WINAPI *)(void))t_proc("GetThreadErrorMode");
    UINT (WINAPI *pget)(void) = (UINT (WINAPI *)(void))t_proc("GetErrorMode");
    UINT (WINAPI *pset)(UINT) = (UINT (WINAPI *)(UINT))t_proc("SetErrorMode");
    NEED(set && get && pget && pset, 100);
    pset(0);
    NEED(get() == 0, 1);
    DWORD old = 0xFFFF;
    NEED(set(SEM_FAILCRITICALERRORS, &old), 2);
    NEED(old == 0, 3);
    NEED(get() == SEM_FAILCRITICALERRORS, 4);
    NEED(pget() == 0, 5);                       /* process mode unaffected */
    NEED(set(SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX, &old), 6);
    NEED(old == SEM_FAILCRITICALERRORS, 7);
    NEED(get() == (SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX), 8);
    NEED(pget() == 0, 9);
    NEED(set(3 | 0x8000, NULL), 10);            /* NULL old-mode pointer is allowed */
    NEED(get() == 0x8003, 11);
    NEED(set(0, &old), 12);
    NEED(old == 0x8003, 13);
    NEED(get() == 0, 14);
    pset(2);
    NEED(get() == 0, 15);                       /* process mode does not leak into thread mode */
    return 0;
}
CHILD(eed_em_thread_invalid_bits) {
    BOOL (WINAPI *set)(DWORD, LPDWORD) = (BOOL (WINAPI *)(DWORD, LPDWORD))t_proc("SetThreadErrorMode");
    DWORD (WINAPI *get)(void) = (DWORD (WINAPI *)(void))t_proc("GetThreadErrorMode");
    NEED(set && get, 100);
    static const DWORD bad[] = { 4, 8, 0x10, 0x100, 0x4000, 0x10000, 0x80000000, 0xFFFFFFFF, 0x8004, 7 };
    set(1, NULL);
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        DWORD old = 0x5555;
        SetLastError(0);
        BOOL r = set(bad[i], &old);
        NEED(!r, 200 + (int)i);
        NEED(GetLastError() == ERROR_INVALID_PARAMETER, 300 + (int)i);
        NEED(old == 0x5555, 400 + (int)i);       /* old mode not written on failure */
        NEED(get() == 1, 500 + (int)i);          /* state unchanged */
    }
    return 0;
}
static DWORD g_thr_initial = 0xFF, g_thr_main_seen = 0xFF;
static DWORD WINAPI th_mode(LPVOID p) {
    DWORD (WINAPI *get)(void) = (DWORD (WINAPI *)(void))t_proc("GetThreadErrorMode");
    BOOL (WINAPI *set)(DWORD, LPDWORD) = (BOOL (WINAPI *)(DWORD, LPDWORD))t_proc("SetThreadErrorMode");
    g_thr_initial = get();
    set(SEM_NOOPENFILEERRORBOX, NULL);
    return get();
}
CHILD(eed_em_thread_isolation) {
    BOOL (WINAPI *set)(DWORD, LPDWORD) = (BOOL (WINAPI *)(DWORD, LPDWORD))t_proc("SetThreadErrorMode");
    DWORD (WINAPI *get)(void) = (DWORD (WINAPI *)(void))t_proc("GetThreadErrorMode");
    UINT (WINAPI *pset)(UINT) = (UINT (WINAPI *)(UINT))t_proc("SetErrorMode");
    NEED(set && get && pset, 100);
    pset(3);
    set(SEM_NOGPFAULTERRORBOX, NULL);
    HANDLE h = CreateThread(NULL, 0, th_mode, NULL, 0, NULL);
    NEED(h != NULL, 1);
    WaitForSingleObject(h, 10000);
    DWORD ec = 0; GetExitCodeThread(h, &ec);
    CloseHandle(h);
    NEED(g_thr_initial == 0, 2);                       /* new threads start with no thread mode */
    NEED(ec == SEM_NOOPENFILEERRORBOX, 3);
    NEED(get() == SEM_NOGPFAULTERRORBOX, 4);           /* worker did not change ours */
    return 0;
}
/* thread mode (NOGPFAULTERRORBOX) is honoured by UnhandledExceptionFilter exactly like the process mode */
CHILD(eed_em_thread_mode_ueh) {
    LONG (WINAPI *uef)(PEXCEPTION_POINTERS) = (LONG (WINAPI *)(PEXCEPTION_POINTERS))t_proc("UnhandledExceptionFilter");
    LPTOP_LEVEL_EXCEPTION_FILTER (WINAPI *setuef)(LPTOP_LEVEL_EXCEPTION_FILTER) = (void *)t_proc("SetUnhandledExceptionFilter");
    BOOL (WINAPI *tset)(DWORD, LPDWORD) = (BOOL (WINAPI *)(DWORD, LPDWORD))t_proc("SetThreadErrorMode");
    UINT (WINAPI *pset)(UINT) = (UINT (WINAPI *)(UINT))t_proc("SetErrorMode");
    NEED(uef && setuef && tset && pset, 100);
    setuef(NULL);
    static EXCEPTION_RECORD er; static CONTEXT cx;
    RtlCaptureContext(&cx);
    er.ExceptionCode = 0xE0A00123; er.ExceptionFlags = 1; er.ExceptionAddress = (PVOID)(ULONG_PTR)cx.Rip;
    EXCEPTION_POINTERS ep = { &er, &cx };
    if (IsDebuggerPresent()) return 0;
    pset(SEM_NOGPFAULTERRORBOX);
    tset(0, NULL);
    NEED(uef(&ep) == EXCEPTION_EXECUTE_HANDLER, 1);       /* process mode */
    pset(0);
    tset(SEM_NOGPFAULTERRORBOX, NULL);
    NEED(uef(&ep) == EXCEPTION_EXECUTE_HANDLER, 2);       /* thread mode only */
    return 0;
}

#define EMCHILD(testname, child, to) TEST(testname) { DWORD c = t_run_child(child, "", to); CHECKF(c == 0, "child exit 0x%lx", c); }
EMCHILD(eed_em_process_sequence, "eed_em_seq", 20000)
EMCHILD(eed_em_process_alignment_bit_is_sticky, "eed_em_sticky_align", 20000)
EMCHILD(eed_em_process_bits_roundtrip, "eed_em_roundtrip_bits", 20000)
EMCHILD(eed_em_process_get_pure, "eed_em_get_no_side_effect", 20000)
EMCHILD(eed_em_thread_basic_semantics, "eed_em_thread_basic", 20000)
EMCHILD(eed_em_thread_invalid_bits_87, "eed_em_thread_invalid_bits", 20000)
EMCHILD(eed_em_thread_per_thread_state, "eed_em_thread_isolation", 20000)
EMCHILD(eed_em_thread_mode_in_unhandled_filter, "eed_em_thread_mode_ueh", 20000)

TEST(eed_em_exports_present) {
    CHECK(UK(SetErrorMode) != NULL); CHECK(UK(GetErrorMode) != NULL);
    CHECK(UK(SetThreadErrorMode) != NULL); CHECK(UK(GetThreadErrorMode) != NULL);
    CHECK(UB(SetErrorMode) != NULL); CHECK(UB(GetErrorMode) != NULL);
    CHECK(UB(SetThreadErrorMode) != NULL); CHECK(UB(GetThreadErrorMode) != NULL);
}
TEST(eed_em_main_get_matches_harness_setup) {
    REQUIRE(U(GetErrorMode));
    /* harness startup sets FAILCRITICALERRORS | NOGPFAULTERRORBOX */
    UINT m = U(GetErrorMode)();
    CHECKF((m & 3) == 3, "mode 0x%x", m);
}
TEST(eed_em_main_set_returns_previous_and_restores) {
    REQUIRE(U(GetErrorMode)); REQUIRE(U(SetErrorMode));
    UINT orig = U(GetErrorMode)();
    UINT r = U(SetErrorMode)(orig);       /* same value: no net change */
    EXPECT_EQ(r, orig);
    EXPECT_EQ(U(GetErrorMode)(), orig);
}
TEST(eed_em_main_thread_mode_default_zero) {
    REQUIRE(U(GetThreadErrorMode));
    EXPECT_EQ(U(GetThreadErrorMode)(), 0);
}
TEST(eed_em_main_thread_mode_set_restore) {
    REQUIRE(U(GetThreadErrorMode)); REQUIRE(U(SetThreadErrorMode));
    DWORD old = 9;
    CHECK(U(SetThreadErrorMode)(SEM_NOOPENFILEERRORBOX, &old));
    EXPECT_EQ(old, 0);
    EXPECT_EQ(U(GetThreadErrorMode)(), SEM_NOOPENFILEERRORBOX);
    CHECK(U(SetThreadErrorMode)(0, &old));
    EXPECT_EQ(old, SEM_NOOPENFILEERRORBOX);
    EXPECT_EQ(U(GetThreadErrorMode)(), 0);
}
TEST(eed_em_main_thread_mode_does_not_change_process_mode) {
    REQUIRE(U(GetErrorMode)); REQUIRE(U(SetThreadErrorMode));
    UINT before = U(GetErrorMode)();
    CHECK(U(SetThreadErrorMode)(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX, NULL));
    EXPECT_EQ(U(GetErrorMode)(), before);
    CHECK(U(SetThreadErrorMode)(0, NULL));
}
