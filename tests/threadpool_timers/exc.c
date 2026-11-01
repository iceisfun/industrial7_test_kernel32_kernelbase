/* Invalid-parameter and exception behaviour of the threadpool APIs.
 * Most Tp* entry points do not return an error for bad pointers: they raise STATUS_INVALID_PARAMETER
 * (0xC000000D) in the calling thread. Each case runs in its own child process; the child installs
 * a top-level filter that terminates the process with the exception code. */
#include "tp.h"

static LONG WINAPI exc_uef(EXCEPTION_POINTERS *ep) {
    TerminateProcess(GetCurrentProcess(), ep->ExceptionRecord->ExceptionCode);
    return EXCEPTION_EXECUTE_HANDLER;
}

static VOID NTAPI exc_work_cb(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_WORK w) { (void)i; (void)p; (void)w; }
static VOID NTAPI exc_timer_cb(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_TIMER t) { (void)i; (void)p; (void)t; }
static VOID NTAPI exc_wait_cb(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_WAIT w, TP_WAIT_RESULT r) { (void)i; (void)p; (void)w; (void)r; }
static VOID NTAPI exc_io_cb(PTP_CALLBACK_INSTANCE i, PVOID p, PVOID o, ULONG r, ULONG_PTR n, PTP_IO io) { }
static VOID NTAPI exc_simple_cb(PTP_CALLBACK_INSTANCE i, PVOID p) { (void)i; (void)p; }

/* ---- callbacks that misbehave on a pool thread ---- */
static HANDLE g_x_ev;
static VOID NTAPI x_av(PTP_CALLBACK_INSTANCE i, PVOID p) { *(volatile int *)0 = 1; }
static VOID NTAPI x_raise(PTP_CALLBACK_INSTANCE i, PVOID p) { RaiseException(0xE0001234, 0, 0, NULL); }
static VOID NTAPI x_seteventtwice(PTP_CALLBACK_INSTANCE i, PVOID p) {
    void (WINAPI *f)(PTP_CALLBACK_INSTANCE, HANDLE) = (void *)t_proc("SetEventWhenCallbackReturns");
    f(i, g_x_ev); f(i, g_x_ev);
}
static VOID NTAPI x_seteventnull(PTP_CALLBACK_INSTANCE i, PVOID p) {
    void (WINAPI *f)(PTP_CALLBACK_INSTANCE, HANDLE) = (void *)t_proc("SetEventWhenCallbackReturns");
    f(i, NULL);
}
static VOID NTAPI x_seteventinv(PTP_CALLBACK_INSTANCE i, PVOID p) {
    void (WINAPI *f)(PTP_CALLBACK_INSTANCE, HANDLE) = (void *)t_proc("SetEventWhenCallbackReturns");
    f(i, INVALID_HANDLE_VALUE);
}
static VOID NTAPI x_semzero(PTP_CALLBACK_INSTANCE i, PVOID p) {
    void (WINAPI *f)(PTP_CALLBACK_INSTANCE, HANDLE, DWORD) = (void *)t_proc("ReleaseSemaphoreWhenCallbackReturns");
    f(i, g_x_ev, 0);
}
static VOID NTAPI x_semnull(PTP_CALLBACK_INSTANCE i, PVOID p) {
    void (WINAPI *f)(PTP_CALLBACK_INSTANCE, HANDLE, DWORD) = (void *)t_proc("ReleaseSemaphoreWhenCallbackReturns");
    f(i, NULL, 1);
}
static VOID NTAPI x_mutexnull(PTP_CALLBACK_INSTANCE i, PVOID p) {
    void (WINAPI *f)(PTP_CALLBACK_INSTANCE, HANDLE) = (void *)t_proc("ReleaseMutexWhenCallbackReturns");
    f(i, NULL);
}
static VOID NTAPI x_csnull(PTP_CALLBACK_INSTANCE i, PVOID p) {
    void (WINAPI *f)(PTP_CALLBACK_INSTANCE, PCRITICAL_SECTION) = (void *)t_proc("LeaveCriticalSectionWhenCallbackReturns");
    f(i, NULL);
}
static VOID NTAPI x_libnull(PTP_CALLBACK_INSTANCE i, PVOID p) {
    void (WINAPI *f)(PTP_CALLBACK_INSTANCE, HMODULE) = (void *)t_proc("FreeLibraryWhenCallbackReturns");
    f(i, NULL);
}
static VOID NTAPI x_disassoc_twice(PTP_CALLBACK_INSTANCE i, PVOID p) {
    void (WINAPI *f)(PTP_CALLBACK_INSTANCE) = (void *)t_proc("DisassociateCurrentThreadFromCallback");
    f(i); f(i);
}

static DWORD WINAPI x_wi_av(LPVOID p) { *(volatile int *)0 = 1; return 0; }
static VOID CALLBACK x_wot_av(PVOID p, BOOLEAN f) { *(volatile int *)0 = 1; }
static VOID NTAPI x_timer_raise(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_TIMER t) { RaiseException(0xE0004321, 0, 0, NULL); }
static VOID NTAPI x_work_raise(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_WORK w) { RaiseException(0xE0005678, 0, 0, NULL); }
static VOID NTAPI x_maylong(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_WORK w) {
    BOOL (WINAPI *f)(PTP_CALLBACK_INSTANCE) = (void *)t_proc("CallbackMayRunLong");
    f(i);
}
static VOID NTAPI x_exit7(PTP_CALLBACK_INSTANCE i, PVOID p) { ExitProcess(7); }

/* Runs `cb` once on the default pool via TrySubmitThreadpoolCallback; survives -> falls through. */
static void run_simple(PTP_SIMPLE_CALLBACK cb) {
    BOOL (WINAPI *ts)(PTP_SIMPLE_CALLBACK, PVOID, PTP_CALLBACK_ENVIRON) = (void *)t_proc("TrySubmitThreadpoolCallback");
    g_x_ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    ts(cb, NULL, NULL);
    Sleep(6000);
}

CHILD(tp_exc) {
    const char *c = argc > 1 ? argv[1] : "";
    SetUnhandledExceptionFilter(exc_uef);
    TP_CALLBACK_ENVIRON env;
    InitializeThreadpoolEnvironment(&env);
    FARPROC f;
    PVOID dummy = NULL;
    HANDLE ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    FILETIME ft; tp_rel_ft(&ft, 100000);

#define CALL(t, name) ((t)t_proc(name))
    if (!strcmp(c, "work_null_cb")) { CALL(PTP_WORK (WINAPI *)(PTP_WORK_CALLBACK, PVOID, PTP_CALLBACK_ENVIRON), "CreateThreadpoolWork")(NULL, NULL, NULL); }
    else if (!strcmp(c, "work_badflags")) {
        env.u.Flags = 4;
        CALL(PTP_WORK (WINAPI *)(PTP_WORK_CALLBACK, PVOID, PTP_CALLBACK_ENVIRON), "CreateThreadpoolWork")(exc_work_cb, NULL, &env);
    }
    else if (!strcmp(c, "timer_null_cb")) { CALL(PTP_TIMER (WINAPI *)(PTP_TIMER_CALLBACK, PVOID, PTP_CALLBACK_ENVIRON), "CreateThreadpoolTimer")(NULL, NULL, NULL); }
    else if (!strcmp(c, "timer_badflags")) {
        env.u.Flags = 8;
        CALL(PTP_TIMER (WINAPI *)(PTP_TIMER_CALLBACK, PVOID, PTP_CALLBACK_ENVIRON), "CreateThreadpoolTimer")(exc_timer_cb, NULL, &env);
    }
    else if (!strcmp(c, "wait_null_cb")) { CALL(PTP_WAIT (WINAPI *)(PTP_WAIT_CALLBACK, PVOID, PTP_CALLBACK_ENVIRON), "CreateThreadpoolWait")(NULL, NULL, NULL); }
    else if (!strcmp(c, "wait_badflags")) {
        env.u.Flags = 16;
        CALL(PTP_WAIT (WINAPI *)(PTP_WAIT_CALLBACK, PVOID, PTP_CALLBACK_ENVIRON), "CreateThreadpoolWait")(exc_wait_cb, NULL, &env);
    }
    else if (!strcmp(c, "io_null_cb")) { CALL(PTP_IO (WINAPI *)(HANDLE, PTP_WIN32_IO_CALLBACK, PVOID, PTP_CALLBACK_ENVIRON), "CreateThreadpoolIo")(ev, NULL, NULL, NULL); }
    else if (!strcmp(c, "trysubmit_null")) { CALL(BOOL (WINAPI *)(PTP_SIMPLE_CALLBACK, PVOID, PTP_CALLBACK_ENVIRON), "TrySubmitThreadpoolCallback")(NULL, NULL, NULL); }
    else if (!strcmp(c, "trysubmit_badflags")) {
        env.u.Flags = 4;
        CALL(BOOL (WINAPI *)(PTP_SIMPLE_CALLBACK, PVOID, PTP_CALLBACK_ENVIRON), "TrySubmitThreadpoolCallback")(exc_simple_cb, NULL, &env);
    }
    else if (!strcmp(c, "io_null_handle")) { CALL(PTP_IO (WINAPI *)(HANDLE, PTP_WIN32_IO_CALLBACK, PVOID, PTP_CALLBACK_ENVIRON), "CreateThreadpoolIo")(NULL, exc_io_cb, NULL, NULL); }
    else if (!strcmp(c, "cb_longenv_maylong")) {
        /* A LongFunction environment makes the pool call TpCallbackMayRunLong itself before the callback runs;
         * a second, explicit CallbackMayRunLong in the callback then raises STATUS_INVALID_PARAMETER. */
        SetThreadpoolCallbackRunsLong(&env);
        PTP_WORK w = CALL(PTP_WORK (WINAPI *)(PTP_WORK_CALLBACK, PVOID, PTP_CALLBACK_ENVIRON), "CreateThreadpoolWork")(x_maylong, NULL, &env);
        CALL(void (WINAPI *)(PTP_WORK), "SubmitThreadpoolWork")(w);
        Sleep(6000);
    }
    else if (!strcmp(c, "submit_null")) { CALL(void (WINAPI *)(PTP_WORK), "SubmitThreadpoolWork")(NULL); }
    else if (!strcmp(c, "waitwork_null")) { CALL(void (WINAPI *)(PTP_WORK, BOOL), "WaitForThreadpoolWorkCallbacks")(NULL, FALSE); }
    else if (!strcmp(c, "closework_null")) { CALL(void (WINAPI *)(PTP_WORK), "CloseThreadpoolWork")(NULL); }
    else if (!strcmp(c, "settimer_null")) { CALL(void (WINAPI *)(PTP_TIMER, PFILETIME, DWORD, DWORD), "SetThreadpoolTimer")(NULL, &ft, 0, 0); }
    else if (!strcmp(c, "settimer_null_cancel")) { CALL(void (WINAPI *)(PTP_TIMER, PFILETIME, DWORD, DWORD), "SetThreadpoolTimer")(NULL, NULL, 0, 0); }
    else if (!strcmp(c, "settimerex_null")) { CALL(BOOL (WINAPI *)(PTP_TIMER, PFILETIME, DWORD, DWORD), "SetThreadpoolTimerEx")(NULL, &ft, 0, 0); }
    else if (!strcmp(c, "closetimer_null")) { CALL(void (WINAPI *)(PTP_TIMER), "CloseThreadpoolTimer")(NULL); }
    else if (!strcmp(c, "waittimer_null")) { CALL(void (WINAPI *)(PTP_TIMER, BOOL), "WaitForThreadpoolTimerCallbacks")(NULL, FALSE); }
    else if (!strcmp(c, "istimerset_null")) { CALL(BOOL (WINAPI *)(PTP_TIMER), "IsThreadpoolTimerSet")(NULL); }
    else if (!strcmp(c, "startio_null")) { CALL(void (WINAPI *)(PTP_IO), "StartThreadpoolIo")(NULL); }
    else if (!strcmp(c, "cancelio_null")) { CALL(void (WINAPI *)(PTP_IO), "CancelThreadpoolIo")(NULL); }
    else if (!strcmp(c, "waitio_null")) { CALL(void (WINAPI *)(PTP_IO, BOOL), "WaitForThreadpoolIoCallbacks")(NULL, FALSE); }
    else if (!strcmp(c, "closeio_null")) { CALL(void (WINAPI *)(PTP_IO), "CloseThreadpoolIo")(NULL); }
    else if (!strcmp(c, "closemembers_null")) { CALL(void (WINAPI *)(PTP_CLEANUP_GROUP, BOOL, PVOID), "CloseThreadpoolCleanupGroupMembers")(NULL, FALSE, NULL); }
    else if (!strcmp(c, "closepool_null")) { CALL(void (WINAPI *)(PTP_POOL), "CloseThreadpool")(NULL); }
    else if (!strcmp(c, "setmax_null")) { CALL(void (WINAPI *)(PTP_POOL, DWORD), "SetThreadpoolThreadMaximum")(NULL, 1); }
    else if (!strcmp(c, "setmin_null")) { CALL(BOOL (WINAPI *)(PTP_POOL, DWORD), "SetThreadpoolThreadMinimum")(NULL, 1); }
    else if (!strcmp(c, "setmax_negative")) {
        PTP_POOL p = CALL(PTP_POOL (WINAPI *)(PVOID), "CreateThreadpool")(NULL);
        CALL(void (WINAPI *)(PTP_POOL, DWORD), "SetThreadpoolThreadMaximum")(p, 0x80000000u);
    }
    else if (!strcmp(c, "setmin_negative")) {
        PTP_POOL p = CALL(PTP_POOL (WINAPI *)(PVOID), "CreateThreadpool")(NULL);
        CALL(BOOL (WINAPI *)(PTP_POOL, DWORD), "SetThreadpoolThreadMinimum")(p, 0x80000000u);
    }
    else if (!strcmp(c, "createpool_reserved")) { CALL(PTP_POOL (WINAPI *)(PVOID), "CreateThreadpool")((PVOID)1); }
    else if (!strcmp(c, "cbinst_null_setevent")) { CALL(void (WINAPI *)(PTP_CALLBACK_INSTANCE, HANDLE), "SetEventWhenCallbackReturns")(NULL, ev); }
    else if (!strcmp(c, "cbinst_null_sem")) { CALL(void (WINAPI *)(PTP_CALLBACK_INSTANCE, HANDLE, DWORD), "ReleaseSemaphoreWhenCallbackReturns")(NULL, ev, 1); }
    else if (!strcmp(c, "cbinst_null_mutex")) { CALL(void (WINAPI *)(PTP_CALLBACK_INSTANCE, HANDLE), "ReleaseMutexWhenCallbackReturns")(NULL, ev); }
    else if (!strcmp(c, "cbinst_null_cs")) { CRITICAL_SECTION cs; InitializeCriticalSection(&cs); CALL(void (WINAPI *)(PTP_CALLBACK_INSTANCE, PCRITICAL_SECTION), "LeaveCriticalSectionWhenCallbackReturns")(NULL, &cs); }
    else if (!strcmp(c, "cbinst_null_lib")) { CALL(void (WINAPI *)(PTP_CALLBACK_INSTANCE, HMODULE), "FreeLibraryWhenCallbackReturns")(NULL, GetModuleHandleA("kernel32.dll")); }
    else if (!strcmp(c, "cbinst_null_disassoc")) { CALL(void (WINAPI *)(PTP_CALLBACK_INSTANCE), "DisassociateCurrentThreadFromCallback")(NULL); }
    else if (!strcmp(c, "cb_seteventtwice")) { run_simple(x_seteventtwice); }
    else if (!strcmp(c, "cb_seteventnull")) { run_simple(x_seteventnull); }
    else if (!strcmp(c, "cb_seteventinv")) { run_simple(x_seteventinv); }
    else if (!strcmp(c, "cb_semzero")) { run_simple(x_semzero); }
    else if (!strcmp(c, "cb_semnull")) { run_simple(x_semnull); }
    else if (!strcmp(c, "cb_mutexnull")) { run_simple(x_mutexnull); }
    else if (!strcmp(c, "cb_csnull")) { run_simple(x_csnull); }
    else if (!strcmp(c, "cb_libnull")) { run_simple(x_libnull); }
    else if (!strcmp(c, "cb_disassoc_twice")) { run_simple(x_disassoc_twice); }
    else if (!strcmp(c, "cb_av")) { run_simple(x_av); }
    else if (!strcmp(c, "cb_raise")) { run_simple(x_raise); }
    else if (!strcmp(c, "cb_exit7")) { run_simple(x_exit7); }
    else if (!strcmp(c, "qwi_av")) {
        CALL(BOOL (WINAPI *)(LPTHREAD_START_ROUTINE, PVOID, ULONG), "QueueUserWorkItem")(x_wi_av, NULL, 0);
        Sleep(6000);
    }
    else if (!strcmp(c, "tq_av")) {
        HANDLE t;
        CALL(BOOL (WINAPI *)(PHANDLE, HANDLE, WAITORTIMERCALLBACK, PVOID, DWORD, DWORD, ULONG), "CreateTimerQueueTimer")(&t, NULL, x_wot_av, NULL, 10, 0, 0);
        Sleep(6000);
    }
    else if (!strcmp(c, "rw_av")) {
        HANDLE w;
        CALL(BOOL (WINAPI *)(PHANDLE, HANDLE, WAITORTIMERCALLBACK, PVOID, ULONG, ULONG), "RegisterWaitForSingleObject")(&w, ev, x_wot_av, NULL, 10, WT_EXECUTEONLYONCE);
        Sleep(6000);
    }
    else if (!strcmp(c, "timer_raise")) {
        PTP_TIMER t = CALL(PTP_TIMER (WINAPI *)(PTP_TIMER_CALLBACK, PVOID, PTP_CALLBACK_ENVIRON), "CreateThreadpoolTimer")(x_timer_raise, NULL, NULL);
        CALL(void (WINAPI *)(PTP_TIMER, PFILETIME, DWORD, DWORD), "SetThreadpoolTimer")(t, &ft, 0, 0);
        FILETIME soon; tp_rel_ft(&soon, 10);
        CALL(void (WINAPI *)(PTP_TIMER, PFILETIME, DWORD, DWORD), "SetThreadpoolTimer")(t, &soon, 0, 0);
        Sleep(6000);
    }
    else if (!strcmp(c, "work_raise")) {
        PTP_WORK w = CALL(PTP_WORK (WINAPI *)(PTP_WORK_CALLBACK, PVOID, PTP_CALLBACK_ENVIRON), "CreateThreadpoolWork")(x_work_raise, NULL, NULL);
        CALL(void (WINAPI *)(PTP_WORK), "SubmitThreadpoolWork")(w);
        Sleep(6000);
    }
    else return 0x7002;
    (void)f; (void)dummy;
    return TP_NO_EXC;
}

static void exc_expect(const char *name, DWORD want) {
    DWORD r = tp_exc_run(name);
    CHECKF(r == want, "case %s: exit 0x%08lx want 0x%08lx", name, r, want);
}

TEST(tp_exc_create_null_callbacks_raise) {
    REQUIRE(U(CreateThreadpoolWork));
    if (t_is_wine()) { t_skip("wine returns errors instead of raising"); return; }
    static const char *const names[] = { "work_null_cb", "timer_null_cb", "wait_null_cb", "io_null_cb", "trysubmit_null" };
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) exc_expect(names[i], 0xC000000D);
}
TEST(tp_exc_create_invalid_env_flags_raise) {
    REQUIRE(U(CreateThreadpoolWork));
    if (t_is_wine()) { t_skip("wine returns errors instead of raising"); return; }
    static const char *const names[] = { "work_badflags", "timer_badflags", "wait_badflags", "trysubmit_badflags" };
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) exc_expect(names[i], 0xC000000D);
}
TEST(tp_exc_null_work_and_timer_objects_raise) {
    REQUIRE(U(SubmitThreadpoolWork));
    if (t_is_wine()) { t_skip("wine tolerates NULL objects"); return; }
    static const char *const names[] = {
        "submit_null", "waitwork_null", "closework_null", "settimer_null", "settimer_null_cancel",
        "settimerex_null", "closetimer_null", "waittimer_null", "istimerset_null",
    };
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) exc_expect(names[i], 0xC000000D);
}
TEST(tp_exc_null_io_objects_raise) {
    REQUIRE(U(StartThreadpoolIo));
    if (t_is_wine()) { t_skip("wine tolerates NULL objects"); return; }
    static const char *const names[] = { "startio_null", "cancelio_null", "waitio_null", "closeio_null" };
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) exc_expect(names[i], 0xC000000D);
}
TEST(tp_exc_null_pool_and_group_raise) {
    REQUIRE(U(CloseThreadpool));
    if (t_is_wine()) { t_skip("wine tolerates NULL pools"); return; }
    static const char *const names[] = { "closemembers_null", "closepool_null", "setmax_null", "setmin_null", "createpool_reserved" };
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) exc_expect(names[i], 0xC000000D);
}
TEST(tp_exc_negative_thread_counts_raise) {
    REQUIRE(U(CreateThreadpool));
    if (t_is_wine()) { t_skip("wine accepts huge thread counts"); return; }
    exc_expect("setmax_negative", 0xC000000D);
    exc_expect("setmin_negative", 0xC000000D);
}
TEST(tp_exc_null_callback_instance_raises) {
    REQUIRE(U(SetEventWhenCallbackReturns));
    if (t_is_wine()) { t_skip("wine tolerates NULL instances"); return; }
    static const char *const names[] = {
        "cbinst_null_setevent", "cbinst_null_sem", "cbinst_null_mutex", "cbinst_null_cs",
        "cbinst_null_lib", "cbinst_null_disassoc",
    };
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) exc_expect(names[i], 0xC000000D);
}
TEST(tp_exc_bad_completion_registrations_kill_process) {
    REQUIRE(U(SetEventWhenCallbackReturns));
    if (t_is_wine()) { t_skip("wine tolerates bad completion registrations"); return; }
    static const char *const names[] = {
        "cb_seteventtwice", "cb_seteventnull", "cb_seteventinv", "cb_semzero", "cb_semnull",
        "cb_mutexnull", "cb_csnull", "cb_libnull", "cb_disassoc_twice",
    };
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) {
        DWORD r = tp_exc_run(names[i]);
        CHECKF(r != TP_NO_EXC && r != 0xDEAD0001 && r != 0xDEAD0000, "case %s survived or hung: 0x%08lx", names[i], r);
        if (r != 0xC000000D) t_note("case %s: exit code 0x%08lx", names[i], r);
    }
}
TEST(tp_exc_unhandled_exception_in_callback_terminates_process) {
    REQUIRE(U(TrySubmitThreadpoolCallback));
    DWORD r = tp_exc_run("cb_av");
    CHECKF(r == 0xC0000005, "exit 0x%08lx", r);
    r = tp_exc_run("cb_raise");
    CHECKF(r == 0xE0001234, "exit 0x%08lx", r);
}

TEST(tp_exc_unhandled_exception_in_other_callback_kinds) {
    REQUIRE(U(QueueUserWorkItem)); REQUIRE(U(CreateThreadpoolWork));
    DWORD r = tp_exc_run("qwi_av");
    CHECKF(r == 0xC0000005, "QueueUserWorkItem: exit 0x%08lx", r);
    r = tp_exc_run("tq_av");
    CHECKF(r == 0xC0000005, "timer queue: exit 0x%08lx", r);
    r = tp_exc_run("rw_av");
    CHECKF(r == 0xC0000005, "registered wait: exit 0x%08lx", r);
    r = tp_exc_run("timer_raise");
    CHECKF(r == 0xE0004321, "threadpool timer: exit 0x%08lx", r);
    r = tp_exc_run("work_raise");
    CHECKF(r == 0xE0005678, "work object: exit 0x%08lx", r);
}
TEST(tp_exc_exit_process_from_callback) {
    REQUIRE(U(TrySubmitThreadpoolCallback));
    DWORD r = tp_exc_run("cb_exit7");
    CHECKF(r == 7, "exit 0x%08lx", r);
}
