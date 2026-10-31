/* Sleep, SleepEx, QueueUserAPC, SwitchToThread. */
#include "tf.h"

static ULONGLONG qpc_ms(void) {
    LARGE_INTEGER f, c; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&c);
    return (ULONGLONG)(c.QuadPart * 1000 / f.QuadPart);
}

static volatile LONG apc_n;
static volatile ULONG_PTR apc_data[64];
static volatile DWORD apc_tid;
static char apc_order[128];
static void reset_apc(void) { apc_n = 0; apc_tid = 0; memset((void *)apc_data, 0, sizeof apc_data); apc_order[0] = 0; }
static VOID CALLBACK apc_record(ULONG_PTR d) {
    LONG i = InterlockedIncrement(&apc_n) - 1;
    if (i < 64) apc_data[i] = d;
    apc_tid = GetCurrentThreadId();
    size_t l = strlen(apc_order);
    if (l < 120) { apc_order[l] = (char)('0' + (d % 10)); apc_order[l + 1] = 0; }
}

TEST(sleep_zero_is_fast) {
    REQUIRE(U(Sleep));
    ULONGLONG t0 = qpc_ms();
    for (int i = 0; i < 1000; i++) U(Sleep)(0);
    CHECK(qpc_ms() - t0 < 2000);
}
TEST(sleep_waits) {
    REQUIRE(U(Sleep));
    static const DWORD v[] = { 50, 100, 250 };
    for (size_t i = 0; i < sizeof v / sizeof v[0]; i++) {
        ULONGLONG t0 = qpc_ms();
        U(Sleep)(v[i]);
        ULONGLONG el = qpc_ms() - t0;
        CHECKF(el + 20 >= v[i], "Sleep(%lu) took %llu", v[i], el);
        CHECKF(el < v[i] + 3000, "Sleep(%lu) took %llu", v[i], el);
    }
}
TEST(sleep_one_ms_many) {
    REQUIRE(U(Sleep));
    ULONGLONG t0 = qpc_ms();
    for (int i = 0; i < 50; i++) U(Sleep)(1);
    ULONGLONG el = qpc_ms() - t0;
    CHECK(el < 5000);
}
TEST(sleepex_nonalertable_returns_zero) {
    REQUIRE(U(SleepEx));
    EQ(U(SleepEx)(0, FALSE), 0);
    ULONGLONG t0 = qpc_ms();
    EQ(U(SleepEx)(60, FALSE), 0);
    CHECK(qpc_ms() - t0 + 20 >= 60);
}
TEST(sleepex_alertable_no_apc_returns_zero) {
    REQUIRE(U(SleepEx));
    EQ(U(SleepEx)(0, TRUE), 0);
    EQ(U(SleepEx)(30, TRUE), 0);
}
TEST(sleepex_apc_pending_returns_wait_io_completion) {
    REQUIRE(U(SleepEx)); REQUIRE(U(QueueUserAPC));
    reset_apc();
    CHECK(U(QueueUserAPC)(apc_record, GetCurrentThread(), 5));
    EQ(apc_n, 0);                         /* queued, not yet delivered */
    EQ(U(SleepEx)(0, TRUE), WAIT_IO_COMPLETION);
    EQ(apc_n, 1);
    EQ(apc_data[0], 5);
    EQ(apc_tid, GetCurrentThreadId());
    EQ(U(SleepEx)(0, TRUE), 0);           /* consumed */
    EQ(apc_n, 1);
}
TEST(sleepex_apc_not_delivered_to_nonalertable) {
    REQUIRE(U(SleepEx)); REQUIRE(U(QueueUserAPC));
    reset_apc();
    CHECK(U(QueueUserAPC)(apc_record, GetCurrentThread(), 7));
    U(Sleep)(20);
    EQ(U(SleepEx)(10, FALSE), 0);
    EQ(apc_n, 0);
    EQ(U(SleepEx)(0, TRUE), WAIT_IO_COMPLETION);
    EQ(apc_n, 1);
}
TEST(sleepex_bool_uses_low_byte) {
    REQUIRE(U(SleepEx)); REQUIRE(U(QueueUserAPC));
    reset_apc();
    CHECK(U(QueueUserAPC)(apc_record, GetCurrentThread(), 3));
    /* the BOOL is forwarded to the kernel as a byte: 0x100 is not alertable */
    EQ(U(SleepEx)(0, (BOOL)0x100), 0);
    EQ(apc_n, 0);
    EQ(U(SleepEx)(0, (BOOL)0x101), WAIT_IO_COMPLETION);
    EQ(apc_n, 1);
}
TEST(apc_fifo_order_single_wait) {
    REQUIRE(U(SleepEx)); REQUIRE(U(QueueUserAPC));
    reset_apc();
    for (ULONG_PTR i = 1; i <= 9; i++) CHECK(U(QueueUserAPC)(apc_record, GetCurrentThread(), i));
    EQ(U(SleepEx)(0, TRUE), WAIT_IO_COMPLETION);
    EQ(apc_n, 9);
    EXPECT_STR(apc_order, "123456789");
    for (int i = 0; i < 9; i++) EQ(apc_data[i], i + 1);
    EQ(U(SleepEx)(0, TRUE), 0);
}
TEST(apc_data_64bit) {
    REQUIRE(U(SleepEx)); REQUIRE(U(QueueUserAPC));
    reset_apc();
    static const ULONG_PTR v[] = { 0, 0xFFFFFFFF, 0x100000000ULL, 0x123456789ABCDEF0ULL, (ULONG_PTR)-1 };
    for (size_t i = 0; i < 5; i++) CHECK(U(QueueUserAPC)(apc_record, GetCurrentThread(), v[i]));
    U(SleepEx)(0, TRUE);
    EQ(apc_n, 5);
    for (size_t i = 0; i < 5; i++) CHECKF(apc_data[i] == v[i], "i=%zu", i);
}
TEST(apc_many_100) {
    REQUIRE(U(SleepEx)); REQUIRE(U(QueueUserAPC));
    reset_apc();
    for (int i = 0; i < 100; i++) CHECK(U(QueueUserAPC)(apc_record, GetCurrentThread(), (ULONG_PTR)i));
    EQ(U(SleepEx)(0, TRUE), WAIT_IO_COMPLETION);
    EQ(apc_n, 100);
    int ordered = 1;
    for (int i = 0; i < 64; i++) if (apc_data[i] != (ULONG_PTR)i) ordered = 0;
    EQ(ordered, 1);
}
static VOID CALLBACK apc_requeue(ULONG_PTR d) {
    apc_record(d);
    if (d < 3) QueueUserAPC(apc_requeue, GetCurrentThread(), d + 1);
}
TEST(apc_queued_from_apc) {
    REQUIRE(U(SleepEx));
    reset_apc();
    QueueUserAPC(apc_requeue, GetCurrentThread(), 1);
    DWORD r = U(SleepEx)(0, TRUE);
    EQ(r, WAIT_IO_COMPLETION);
    t_note("APCs run by one alertable wait when an APC queues another: %ld", apc_n);
    CHECK(apc_n >= 1);
    while (U(SleepEx)(0, TRUE) == WAIT_IO_COMPLETION) ;
    EQ(apc_n, 3);
}

typedef struct { HANDLE ready; DWORD (WINAPI *mode)(void *); LONG result; } actx;
static DWORD WINAPI th_sleepex_inf(LPVOID p) {
    actx *c = (actx *)p;
    SetEvent(c->ready);
    DWORD r = SleepEx(INFINITE, TRUE);
    return r;
}
static DWORD WINAPI th_sleepex_long(LPVOID p) {
    actx *c = (actx *)p;
    SetEvent(c->ready);
    return SleepEx(0xFFFFFFFE, TRUE);
}
static DWORD WINAPI th_wait_alert(LPVOID p) {
    actx *c = (actx *)p;
    HANDLE ev = tf_ev(TRUE, FALSE);
    SetEvent(c->ready);
    DWORD r = WaitForSingleObjectEx(ev, 15000, TRUE);
    CloseHandle(ev);
    return r;
}
static DWORD WINAPI th_wait_nonalert(LPVOID p) {
    actx *c = (actx *)p;
    SetEvent(c->ready);
    DWORD r = WaitForSingleObject((HANDLE)c->mode, 15000);   /* mode field reused as event handle */
    return r;
}
static void apc_to_worker_case(LPTHREAD_START_ROUTINE fn, DWORD expect) {
    reset_apc();
    actx c; c.ready = tf_ev(TRUE, FALSE);
    DWORD tid;
    HANDLE h = CreateThread(NULL, 0, fn, &c, 0, &tid);
    WaitForSingleObject(c.ready, 10000);
    Sleep(50);
    BOOL q = U(QueueUserAPC)(apc_record, h, 0x42);
    CHECK(q);
    DWORD r = tf_join(h);
    CHECKF(r == expect, "exit code %lu want %lu", r, expect);
    EQ(apc_n, 1);
    EQ(apc_data[0], 0x42);
    EQ(apc_tid, tid);
    CloseHandle(c.ready);
}
TEST(apc_wakes_sleepex_infinite) { REQUIRE(U(QueueUserAPC)); apc_to_worker_case(th_sleepex_inf, WAIT_IO_COMPLETION); }
TEST(apc_wakes_sleepex_long_timeout) { REQUIRE(U(QueueUserAPC)); apc_to_worker_case(th_sleepex_long, WAIT_IO_COMPLETION); }
TEST(apc_wakes_alertable_wait) { REQUIRE(U(QueueUserAPC)); apc_to_worker_case(th_wait_alert, WAIT_IO_COMPLETION); }
TEST(apc_not_run_for_nonalertable_wait) {
    REQUIRE(U(QueueUserAPC));
    reset_apc();
    actx c; c.ready = tf_ev(TRUE, FALSE);
    HANDLE gate = tf_ev(TRUE, FALSE);
    c.mode = (DWORD (WINAPI *)(void *))gate;
    HANDLE h = CreateThread(NULL, 0, th_wait_nonalert, &c, 0, NULL);
    WaitForSingleObject(c.ready, 10000);
    Sleep(30);
    CHECK(U(QueueUserAPC)(apc_record, h, 1));
    Sleep(150);
    EQ(apc_n, 0);
    SetEvent(gate);
    EQ(tf_join(h), WAIT_OBJECT_0);
    EQ(apc_n, 0);                 /* pending APCs die with the thread */
    CloseHandle(gate); CloseHandle(c.ready);
}
TEST(apc_to_running_thread_runs_when_it_waits) {
    REQUIRE(U(QueueUserAPC));
    reset_apc();
    HANDLE go = tf_ev(TRUE, FALSE);
    DWORD WINAPI body(LPVOID p) {
        WaitForSingleObject((HANDLE)p, 15000);       /* non-alertable */
        return SleepEx(2000, TRUE);                  /* APC delivered here */
    }
    HANDLE h = CreateThread(NULL, 0, body, go, 0, NULL);
    Sleep(30);
    CHECK(U(QueueUserAPC)(apc_record, h, 9));
    Sleep(30);
    EQ(apc_n, 0);
    SetEvent(go);
    EQ(tf_join(h), WAIT_IO_COMPLETION);
    EQ(apc_n, 1);
    CloseHandle(go);
}
TEST(apc_queue_to_new_suspended_thread) {
    REQUIRE(U(QueueUserAPC)); REQUIRE(U(ResumeThread));
    reset_apc();
    HANDLE h = CreateThread(NULL, 0, tf_th_ret, (LPVOID)3, CREATE_SUSPENDED, NULL);
    SET_ERR(0);
    BOOL q = U(QueueUserAPC)(apc_record, h, 0x11);
    t_note("QueueUserAPC on never-run suspended thread: %d err %lu", q, GetLastError());
    ResumeThread(h);
    EQ(tf_join(h), 3);
    t_note("APC executed before thread exit: %ld", apc_n);
}
TEST(apc_invalid_handles) {
    REQUIRE(U(QueueUserAPC));
    HANDLE ev = tf_ev(TRUE, FALSE);
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, ev };
    reset_apc();
    for (int i = 0; i < 3; i++) {
        SET_ERR(0);
        EQ(U(QueueUserAPC)(apc_record, bad[i], 1), 0);
        EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
    EQ(apc_n, 0);
    CloseHandle(ev);
}
TEST(apc_access_rights) {
    REQUIRE(U(QueueUserAPC));
    reset_apc();
    HANDLE ev = tf_ev(TRUE, FALSE);
    DWORD tid;
    HANDLE h = CreateThread(NULL, 0, tf_th_waitev_alert, ev, 0, &tid);
    HANDLE o = OpenThread(THREAD_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, tid);
    SET_ERR(0);
    EQ(U(QueueUserAPC)(apc_record, o, 1), 0);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(o);
    o = OpenThread(THREAD_SET_CONTEXT, FALSE, tid);
    CHECK(o != NULL);
    CHECK(U(QueueUserAPC)(apc_record, o, 2));
    CloseHandle(o);
    Sleep(100);
    EQ(apc_n, 1);
    EQ(apc_data[0], 2);
    SetEvent(ev);
    EQ(tf_join(h), 0x78);
    CloseHandle(ev);
}
TEST(apc_to_exited_thread) {
    REQUIRE(U(QueueUserAPC));
    reset_apc();
    HANDLE h = CreateThread(NULL, 0, tf_th_ret, NULL, 0, NULL);
    WaitForSingleObject(h, 5000);
    SET_ERR(0);
    BOOL q = U(QueueUserAPC)(apc_record, h, 1);
    t_note("QueueUserAPC on exited thread: %d err %lu", q, GetLastError());
    CHECK(!q);
    EQ(apc_n, 0);
    CloseHandle(h);
}
TEST(apc_queueuserapc2_absent_on_win10_note) {
    FARPROC p = t_proc("QueueUserAPC2");
    t_note("QueueUserAPC2 export %s", p ? "present" : "absent");
}
TEST(apc_exports_resolve) {
    CHECK(UK(Sleep) != NULL);
    CHECK(UK(SleepEx) != NULL);
    CHECK(UK(QueueUserAPC) != NULL);
    CHECK(UK(SwitchToThread) != NULL);
}
TEST(apc_across_many_threads) {
    REQUIRE(U(QueueUserAPC));
    reset_apc();
    enum { N = 16 };
    HANDLE hs[N]; HANDLE ev = tf_ev(TRUE, FALSE);
    for (int i = 0; i < N; i++) hs[i] = CreateThread(NULL, 0, tf_th_waitev_alert, ev, 0, NULL);
    Sleep(50);
    for (int i = 0; i < N; i++) CHECK(U(QueueUserAPC)(apc_record, hs[i], (ULONG_PTR)i));
    Sleep(150);
    EQ(apc_n, N);
    SetEvent(ev);
    for (int i = 0; i < N; i++) EQ(tf_join(hs[i]), 0x78);
    CloseHandle(ev);
}

TEST(switchtothread_basic) {
    REQUIRE(U(SwitchToThread));
    int yielded = 0;
    for (int i = 0; i < 200; i++) if (U(SwitchToThread)()) yielded++;
    t_note("SwitchToThread returned nonzero %d/200 times", yielded);
}
static volatile LONG st_flag;
static DWORD WINAPI th_st_setter(LPVOID p) { InterlockedExchange(&st_flag, 1); return 0; }
TEST(switchtothread_lets_ready_thread_run) {
    REQUIRE(U(SwitchToThread));
    st_flag = 0;
    HANDLE h = CreateThread(NULL, 0, th_st_setter, NULL, 0, NULL);
    for (int i = 0; i < 20000 && !st_flag; i++) U(SwitchToThread)();
    CHECK(st_flag == 1);
    tf_join(h);
}
TEST(sleep_concurrent_threads) {
    REQUIRE(U(Sleep));
    DWORD WINAPI body(LPVOID p) { Sleep(100); return 1; }
    enum { N = 32 };
    HANDLE hs[N];
    ULONGLONG t0 = qpc_ms();
    for (int i = 0; i < N; i++) hs[i] = CreateThread(NULL, 0, body, NULL, 0, NULL);
    int ok = 0;
    for (int i = 0; i < N; i++) if (tf_join(hs[i]) == 1) ok++;
    EQ(ok, N);
    CHECK(qpc_ms() - t0 < 4000);
}
