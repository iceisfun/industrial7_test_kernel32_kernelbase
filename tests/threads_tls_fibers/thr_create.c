/* CreateThread / ExitThread / TerminateThread / ids / handles. */
#include "tf.h"

static LONG g_flag;
static DWORD WINAPI th_setflag(LPVOID p) { InterlockedExchange(&g_flag, 1); return 5; }
static DWORD WINAPI th_exit_early(LPVOID p) {
    BOOL (WINAPI *unused)(void) = NULL; (void)unused;
    void (WINAPI *ex)(DWORD) = (void (WINAPI *)(DWORD))t_proc("ExitThread");
    ex((DWORD)(ULONG_PTR)p);
    InterlockedExchange(&g_flag, 99);
    return 1;
}
static DWORD nest(int d, DWORD code) {
    void (WINAPI *ex)(DWORD) = (void (WINAPI *)(DWORD))t_proc("ExitThread");
    if (d == 0) { ex(code); return 0; }
    volatile char pad[64]; pad[0] = (char)d;
    return nest(d - 1, code) + pad[0];
}
static DWORD WINAPI th_exit_nested(LPVOID p) { nest(40, (DWORD)(ULONG_PTR)p); InterlockedExchange(&g_flag, 99); return 1; }

static struct { ULONG_PTR lo, hi, local; DWORD tid; } g_sb;
static DWORD WINAPI th_stackinfo(LPVOID p) {
    volatile int x = 0;
    ULONG_PTR lo = 0, hi = 0;
    void (WINAPI *lim)(PULONG_PTR, PULONG_PTR) = (void (WINAPI *)(PULONG_PTR, PULONG_PTR))t_proc("GetCurrentThreadStackLimits");
    if (lim) lim(&lo, &hi);
    g_sb.lo = lo; g_sb.hi = hi; g_sb.local = (ULONG_PTR)&x; g_sb.tid = GetCurrentThreadId();
    return 0;
}
static DWORD WINAPI th_touch(LPVOID p) {
    /* touch ~ (size) bytes of stack in 4K steps */
    size_t n = (size_t)(ULONG_PTR)p;
    volatile char *buf = (volatile char *)_alloca(n);
    for (size_t i = 0; i < n; i += 4096) buf[i] = 1;
    buf[n - 1] = 2;
    return 0x4242;
}

TEST(thr_create_basic) {
    REQUIRE(U(CreateThread));
    DWORD tid = 0;
    HANDLE h = U(CreateThread)(NULL, 0, tf_th_ret, (LPVOID)(ULONG_PTR)0x1234, 0, &tid);
    CHECK(h != NULL);
    CHECK(tid != 0);
    CHECK(tid != GetCurrentThreadId());
    EQ(tf_join(h), 0x1234);
}

TEST(thr_create_exitcode_table) {
    REQUIRE(U(CreateThread));
    static const DWORD v[] = { 0, 1, 2, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0xC0000005, 0x40010005, 258, 260, 0xDEADBEEF };
    for (size_t i = 0; i < sizeof v / sizeof v[0]; i++) {
        HANDLE h = U(CreateThread)(NULL, 0, tf_th_ret, (LPVOID)(ULONG_PTR)v[i], 0, NULL);
        CHECK(h != NULL);
        DWORD c = tf_join(h);
        CHECKF(c == v[i], "i=%zu got %lx want %lx", i, c, v[i]);
    }
}

TEST(thr_create_param_delivery_64bit) {
    REQUIRE(U(CreateThread));
    static const ULONG_PTR v[] = { 0, 1, 0x7FFFFFFF, 0xFFFFFFFF, 0x100000000ULL, 0x123456789ABCDEF0ULL, (ULONG_PTR)-1 };
    for (size_t i = 0; i < sizeof v / sizeof v[0]; i++) {
        tf_last_param = 0x5555;
        HANDLE h = U(CreateThread)(NULL, 0, tf_th_store, (LPVOID)v[i], 0, NULL);
        CHECK(h != NULL);
        tf_join(h);
        CHECKF(tf_last_param == v[i], "i=%zu got %llx", i, (unsigned long long)tf_last_param);
    }
}

TEST(thr_create_tid_optional) {
    REQUIRE(U(CreateThread));
    HANDLE h = U(CreateThread)(NULL, 0, tf_th_ret, (LPVOID)7, 0, NULL);
    CHECK(h != NULL);
    EQ(tf_join(h), 7);
}

TEST(thr_create_tid_matches_getthreadid) {
    REQUIRE(U(CreateThread)); REQUIRE(U(GetThreadId));
    HANDLE ev = tf_ev(TRUE, FALSE);
    DWORD tid = 0;
    HANDLE h = U(CreateThread)(NULL, 0, tf_th_waitev, ev, 0, &tid);
    CHECK(h != NULL);
    EQ(U(GetThreadId)(h), tid);
    SetEvent(ev);
    tf_join(h);
    CloseHandle(ev);
}

TEST(thr_create_tid_seen_inside_thread) {
    REQUIRE(U(CreateThread));
    g_sb.tid = 0;
    DWORD tid = 0;
    HANDLE h = U(CreateThread)(NULL, 0, th_stackinfo, NULL, 0, &tid);
    tf_join(h);
    EQ(g_sb.tid, tid);
}

TEST(thr_create_suspended_not_running) {
    REQUIRE(U(CreateThread)); REQUIRE(U(ResumeThread));
    InterlockedExchange(&g_flag, 0);
    HANDLE h = U(CreateThread)(NULL, 0, th_setflag, NULL, CREATE_SUSPENDED, NULL);
    CHECK(h != NULL);
    Sleep(150);
    EQ(g_flag, 0);
    DWORD ec = 0; GetExitCodeThread(h, &ec);
    EQ(ec, STILL_ACTIVE);
    EQ(WaitForSingleObject(h, 20), WAIT_TIMEOUT);
    EQ(U(ResumeThread)(h), 1);
    EQ(tf_join(h), 5);
    EQ(g_flag, 1);
}

TEST(thr_create_suspended_double_suspend) {
    REQUIRE(U(CreateThread)); REQUIRE(U(ResumeThread)); REQUIRE(U(SuspendThread));
    InterlockedExchange(&g_flag, 0);
    HANDLE h = U(CreateThread)(NULL, 0, th_setflag, NULL, CREATE_SUSPENDED, NULL);
    EQ(U(SuspendThread)(h), 1);
    EQ(U(ResumeThread)(h), 2);
    Sleep(100);
    EQ(g_flag, 0);
    EQ(U(ResumeThread)(h), 1);
    EQ(tf_join(h), 5);
}

TEST(thr_create_flags_unknown_bits_ignored) {
    REQUIRE(U(CreateThread));
    /* CreateThread masks dwCreationFlags with 0x10004. */
    static const DWORD fl[] = { 0x1, 0x2, 0x8, 0x10, 0x100, 0x20000, 0x40000000, 0x80000000 };
    for (size_t i = 0; i < sizeof fl / sizeof fl[0]; i++) {
        HANDLE h = U(CreateThread)(NULL, 0, tf_th_ret, (LPVOID)9, fl[i], NULL);
        CHECKF(h != NULL, "flags %lx", fl[i]);
        EQ(tf_join(h), 9);
    }
}

TEST(thr_createremoteex_invalid_flags) {
    REQUIRE(U(CreateRemoteThreadEx));
    static const DWORD fl[] = { 0x1, 0x2, 0x8, 0x10, 0x20000, 0x80000000, 0x10005, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof fl / sizeof fl[0]; i++) {
        SET_ERR(0);
        HANDLE h = U(CreateRemoteThreadEx)(GetCurrentProcess(), NULL, 0, tf_th_ret, NULL, fl[i], NULL, NULL);
        CHECKF(h == NULL, "flags %lx", fl[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        if (h) { TerminateThread(h, 0); CloseHandle(h); }
    }
}

TEST(thr_createremoteex_valid_flags) {
    REQUIRE(U(CreateRemoteThreadEx)); REQUIRE(U(ResumeThread));
    DWORD tid = 0;
    HANDLE h = U(CreateRemoteThreadEx)(GetCurrentProcess(), NULL, 0, tf_th_ret, (LPVOID)31, CREATE_SUSPENDED | STACK_SIZE_PARAM_IS_A_RESERVATION, NULL, &tid);
    CHECK(h != NULL);
    CHECK(tid != 0);
    EQ(U(ResumeThread)(h), 1);
    EQ(tf_join(h), 31);
}

TEST(thr_createremotethread_current_process) {
    REQUIRE(U(CreateRemoteThread));
    DWORD tid = 0;
    HANDLE h = U(CreateRemoteThread)(GetCurrentProcess(), NULL, 0, tf_th_ret, (LPVOID)44, 0, &tid);
    CHECK(h != NULL);
    CHECK(tid != 0);
    EQ(tf_join(h), 44);
}

TEST(thr_createremotethread_real_self_handle) {
    REQUIRE(U(CreateRemoteThread));
    HANDLE hp = OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId());
    CHECK(hp != NULL);
    if (!hp) return;
    HANDLE h = U(CreateRemoteThread)(hp, NULL, 0, tf_th_ret, (LPVOID)45, 0, NULL);
    CHECK(h != NULL);
    EQ(tf_join(h), 45);
    CloseHandle(hp);
}

TEST(thr_createremotethread_invalid_process) {
    REQUIRE(U(CreateRemoteThread));
    SET_ERR(0);
    HANDLE h = U(CreateRemoteThread)(NULL, NULL, 0, tf_th_ret, NULL, 0, NULL);
    CHECK(h == NULL);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    if (h) { TerminateThread(h, 0); CloseHandle(h); }
    HANDLE ev = tf_ev(TRUE, FALSE);
    SET_ERR(0);
    h = U(CreateRemoteThread)(ev, NULL, 0, tf_th_ret, NULL, 0, NULL);
    CHECK(h == NULL);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    if (h) { TerminateThread(h, 0); CloseHandle(h); }
    CloseHandle(ev);
}

TEST(thr_create_sa_inherit) {
    REQUIRE(U(CreateThread));
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE ev = tf_ev(TRUE, FALSE);
    HANDLE h = U(CreateThread)(&sa, 0, tf_th_waitev, ev, 0, NULL);
    CHECK(h != NULL);
    DWORD fl = 0; GetHandleInformation(h, &fl);
    CHECK(fl & HANDLE_FLAG_INHERIT);
    SetEvent(ev); tf_join(h);
    sa.bInheritHandle = FALSE;
    h = U(CreateThread)(&sa, 0, tf_th_waitev, ev, 0, NULL);
    fl = 0; GetHandleInformation(h, &fl);
    CHECK(!(fl & HANDLE_FLAG_INHERIT));
    tf_join(h);
    h = U(CreateThread)(NULL, 0, tf_th_waitev, ev, 0, NULL);
    fl = 0; GetHandleInformation(h, &fl);
    CHECK(!(fl & HANDLE_FLAG_INHERIT));
    tf_join(h);
    CloseHandle(ev);
}

TEST(thr_create_handle_access_full) {
    REQUIRE(U(CreateThread)); REQUIRE(U(SuspendThread)); REQUIRE(U(ResumeThread));
    HANDLE h = U(CreateThread)(NULL, 0, tf_th_ret, NULL, CREATE_SUSPENDED, NULL);
    /* Creator handle has THREAD_ALL_ACCESS: all operations allowed. */
    CONTEXT c; memset(&c, 0, sizeof c); c.ContextFlags = CONTEXT_CONTROL;
    CHECK(U(GetThreadContext)(h, &c));
    CHECK(U(SetThreadPriority)(h, THREAD_PRIORITY_NORMAL));
    U(ResumeThread)(h);
    tf_join(h);
}

static void stack_case(SIZE_T sz, DWORD fl, SIZE_T touch) {
    HANDLE h = U(CreateThread)(NULL, sz, th_touch, (LPVOID)touch, fl, NULL);
    CHECKF(h != NULL, "sz=%zx fl=%lx", sz, fl);
    DWORD c = tf_join(h);
    CHECKF(c == 0x4242, "sz=%zx fl=%lx touch=%zx got %lx", sz, fl, touch, c);
}
TEST(thr_create_stack_commit_sizes) {
    REQUIRE(U(CreateThread));
    static const SIZE_T sz[] = { 0, 1, 0xFFF, 0x1000, 0x1001, 0x4000, 0x10000, 0x10001, 0x100000, 0x400000 };
    for (size_t i = 0; i < sizeof sz / sizeof sz[0]; i++) stack_case(sz[i], 0, 0x8000);
}
TEST(thr_create_stack_commit_grows) {
    REQUIRE(U(CreateThread));
    /* Small commit but reserve default: stack grows on demand. */
    stack_case(0x1000, 0, 0x40000);
    stack_case(0, 0, 0x40000);
}
TEST(thr_create_stack_reservation_sizes) {
    REQUIRE(U(CreateThread));
    static const SIZE_T sz[] = { 0x40000, 0x80000, 0x100000, 0x200000, 0x1000000 };
    for (size_t i = 0; i < sizeof sz / sizeof sz[0]; i++) stack_case(sz[i], STACK_SIZE_PARAM_IS_A_RESERVATION, 0x8000);
}
TEST(thr_create_stack_reservation_limits) {
    REQUIRE(U(CreateThread));
    static const SIZE_T sz[] = { 0x100000, 0x200000, 0x400000, 0x1000000 };
    for (size_t i = 0; i < sizeof sz / sizeof sz[0]; i++) {
        memset(&g_sb, 0, sizeof g_sb);
        HANDLE h = U(CreateThread)(NULL, sz[i], th_stackinfo, NULL, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
        tf_join(h);
        if (!g_sb.hi) { t_skip("GetCurrentThreadStackLimits missing"); return; }
        SIZE_T span = g_sb.hi - g_sb.lo;
        CHECKF(span >= sz[i], "req %zx span %zx", sz[i], span);
        CHECKF(span <= sz[i] + 0x20000, "req %zx span %zx", sz[i], span);
        CHECK(g_sb.local >= g_sb.lo && g_sb.local < g_sb.hi);
    }
}
TEST(thr_create_stack_limits_match_teb) {
    REQUIRE(U(CreateThread));
    memset(&g_sb, 0, sizeof g_sb);
    tf_join(U(CreateThread)(NULL, 0, th_stackinfo, NULL, 0, NULL));
    if (!g_sb.hi) { t_skip("no limits"); return; }
    CHECK(g_sb.lo < g_sb.hi);
    CHECK((g_sb.hi & 0xFFF) == 0);
    CHECK((g_sb.lo & 0xFFFF) == 0);   /* allocation base: 64K aligned */
    CHECK(g_sb.hi - g_sb.lo >= 0x10000);
}
TEST(thr_create_stack_huge_fails) {
    REQUIRE(U(CreateThread));
    if (t_is_wine()) { t_skip("wine asserts in create_view for absurd reservations"); return; }
    static const SIZE_T sz[] = { (SIZE_T)0x10000000000ULL, (SIZE_T)0x7FFFFFFF0000ULL, (SIZE_T)-1 };
    for (size_t i = 0; i < sizeof sz / sizeof sz[0]; i++) {
        for (int r = 0; r < 2; r++) {
            SET_ERR(0);
            HANDLE h = U(CreateThread)(NULL, sz[i], tf_th_ret, NULL, r ? STACK_SIZE_PARAM_IS_A_RESERVATION : 0, NULL);
            CHECKF(h == NULL, "i=%zu r=%d", i, r);
            DWORD e = GetLastError();
            CHECKF(e != 0, "last error should be set");
            t_note("huge stack i=%zu r=%d err=%lu", i, r, e);
            if (h) { TerminateThread(h, 0); CloseHandle(h); }
        }
    }
}

TEST(thr_exitcode_stillactive) {
    REQUIRE(U(CreateThread)); REQUIRE(U(GetExitCodeThread));
    HANDLE ev = tf_ev(TRUE, FALSE);
    HANDLE h = U(CreateThread)(NULL, 0, tf_th_waitev, ev, 0, NULL);
    DWORD c = 0;
    CHECK(U(GetExitCodeThread)(h, &c));
    EQ(c, STILL_ACTIVE);
    SetEvent(ev);
    EQ(WaitForSingleObject(h, 5000), WAIT_OBJECT_0);
    CHECK(U(GetExitCodeThread)(h, &c));
    EQ(c, 0x77);
    CHECK(U(GetExitCodeThread)(h, &c));   /* repeatable */
    EQ(c, 0x77);
    CloseHandle(h); CloseHandle(ev);
}
TEST(thr_exitcode_returns_259_ambiguity) {
    REQUIRE(U(CreateThread)); REQUIRE(U(GetExitCodeThread));
    HANDLE h = U(CreateThread)(NULL, 0, tf_th_ret, (LPVOID)259, 0, NULL);
    WaitForSingleObject(h, 5000);
    DWORD c = 0;
    CHECK(U(GetExitCodeThread)(h, &c));
    EQ(c, 259);
    CloseHandle(h);
}
TEST(thr_exitcode_invalid_handles) {
    REQUIRE(U(GetExitCodeThread));
    DWORD c = 0x1234;
    SET_ERR(0);
    CHECK(!U(GetExitCodeThread)(NULL, &c));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(!U(GetExitCodeThread)(INVALID_HANDLE_VALUE, &c));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE ev = tf_ev(TRUE, FALSE);
    SET_ERR(0);
    CHECK(!U(GetExitCodeThread)(ev, &c));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
    EQ(c, 0x1234);
}
TEST(thr_exitcode_current_thread_pseudo) {
    REQUIRE(U(GetExitCodeThread));
    DWORD c = 0;
    CHECK(U(GetExitCodeThread)(GetCurrentThread(), &c));
    EQ(c, STILL_ACTIVE);
}
TEST(thr_exitcode_access_denied_sync_only) {
    REQUIRE(U(GetExitCodeThread)); REQUIRE(U(OpenThread));
    HANDLE ev = tf_ev(TRUE, FALSE);
    DWORD tid;
    HANDLE h = CreateThread(NULL, 0, tf_th_waitev, ev, 0, &tid);
    HANDLE o = U(OpenThread)(SYNCHRONIZE, FALSE, tid);
    CHECK(o != NULL);
    DWORD c = 0;
    SET_ERR(0);
    CHECK(!U(GetExitCodeThread)(o, &c));
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(o);
    o = U(OpenThread)(THREAD_QUERY_LIMITED_INFORMATION, FALSE, tid);
    CHECK(U(GetExitCodeThread)(o, &c));
    EQ(c, STILL_ACTIVE);
    CloseHandle(o);
    SetEvent(ev); tf_join(h); CloseHandle(ev);
}

TEST(thr_exitthread_code) {
    REQUIRE(U(ExitThread));
    InterlockedExchange(&g_flag, 0);
    static const DWORD v[] = { 0, 1, 0x55, 0xFFFFFFFF, 0xC000013A };
    for (size_t i = 0; i < sizeof v / sizeof v[0]; i++) {
        InterlockedExchange(&g_flag, 0);
        DWORD c = tf_run(th_exit_early, (LPVOID)(ULONG_PTR)v[i]);
        CHECKF(c == v[i], "i=%zu got %lx", i, c);
        EQ(g_flag, 0);
    }
}
TEST(thr_exitthread_deep_frames) {
    REQUIRE(U(ExitThread));
    InterlockedExchange(&g_flag, 0);
    EQ(tf_run(th_exit_nested, (LPVOID)0x99), 0x99);
    EQ(g_flag, 0);
}

TEST(thr_terminate_basic) {
    REQUIRE(U(TerminateThread));
    HANDLE ev = tf_ev(TRUE, FALSE);
    HANDLE h = CreateThread(NULL, 0, tf_th_waitev, ev, 0, NULL);
    Sleep(20);
    CHECK(U(TerminateThread)(h, 0x5A5A));
    EQ(WaitForSingleObject(h, 5000), WAIT_OBJECT_0);
    DWORD c = 0; GetExitCodeThread(h, &c);
    EQ(c, 0x5A5A);
    CloseHandle(h); CloseHandle(ev);
}
TEST(thr_terminate_suspended_thread) {
    REQUIRE(U(TerminateThread));
    InterlockedExchange(&g_flag, 0);
    HANDLE h = CreateThread(NULL, 0, th_setflag, NULL, CREATE_SUSPENDED, NULL);
    CHECK(U(TerminateThread)(h, 3));
    EQ(WaitForSingleObject(h, 5000), WAIT_OBJECT_0);
    DWORD c = 0; GetExitCodeThread(h, &c);
    EQ(c, 3);
    EQ(g_flag, 0);
    CloseHandle(h);
}
TEST(thr_terminate_codes) {
    REQUIRE(U(TerminateThread));
    static const DWORD v[] = { 0, 1, 259, 0xFFFFFFFF, 0xC0000005 };
    for (size_t i = 0; i < sizeof v / sizeof v[0]; i++) {
        HANDLE h = CreateThread(NULL, 0, tf_th_ret, NULL, CREATE_SUSPENDED, NULL);
        CHECK(U(TerminateThread)(h, v[i]));
        WaitForSingleObject(h, 5000);
        DWORD c = 0; GetExitCodeThread(h, &c);
        CHECKF(c == v[i], "i=%zu got %lx", i, c);
        CloseHandle(h);
    }
}
TEST(thr_terminate_invalid) {
    REQUIRE(U(TerminateThread));
    SET_ERR(0);
    CHECK(!U(TerminateThread)(NULL, 0));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE ev = tf_ev(TRUE, FALSE);
    SET_ERR(0);
    CHECK(!U(TerminateThread)(ev, 0));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
    SET_ERR(0);
    CHECK(!U(TerminateThread)(INVALID_HANDLE_VALUE, 0));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(thr_terminate_access_denied) {
    REQUIRE(U(TerminateThread)); REQUIRE(U(OpenThread));
    HANDLE ev = tf_ev(TRUE, FALSE);
    DWORD tid;
    HANDLE h = CreateThread(NULL, 0, tf_th_waitev, ev, 0, &tid);
    HANDLE o = U(OpenThread)(THREAD_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, tid);
    CHECK(o != NULL);
    SET_ERR(0);
    CHECK(!U(TerminateThread)(o, 1));
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    EQ(WaitForSingleObject(h, 30), WAIT_TIMEOUT);
    CloseHandle(o);
    SetEvent(ev); EQ(tf_join(h), 0x77); CloseHandle(ev);
}
TEST(thr_terminate_already_exited) {
    REQUIRE(U(TerminateThread));
    HANDLE h = CreateThread(NULL, 0, tf_th_ret, (LPVOID)8, 0, NULL);
    WaitForSingleObject(h, 5000);
    SET_ERR(0);
    BOOL r = U(TerminateThread)(h, 99);
    t_note("TerminateThread on exited thread: ret=%d err=%lu", r, GetLastError());
    DWORD c = 0; GetExitCodeThread(h, &c);
    if (t_is_wine()) t_note("wine exit code %lu", c); else
    EQ(c, 8);   /* exit code of an exited thread never changes */
    CloseHandle(h);
}

static volatile LONG g_fls_cb;
static VOID WINAPI fls_cb(PVOID p) { InterlockedIncrement(&g_fls_cb); }
static DWORD g_fls_idx;
static DWORD WINAPI th_flsset_wait(LPVOID p) {
    FlsSetValue(g_fls_idx, (PVOID)1);
    WaitForSingleObject((HANDLE)p, 15000);
    return 0;
}
TEST(thr_terminate_skips_fls_callback) {
    REQUIRE(U(TerminateThread)); REQUIRE(U(FlsAlloc));
    g_fls_cb = 0;
    g_fls_idx = U(FlsAlloc)(fls_cb);
    CHECK(g_fls_idx != TF_FLS_OUT);
    HANDLE ev = tf_ev(TRUE, FALSE);
    HANDLE h = CreateThread(NULL, 0, th_flsset_wait, ev, 0, NULL);
    Sleep(100);
    CHECK(U(TerminateThread)(h, 1));
    WaitForSingleObject(h, 5000);
    Sleep(50);
    EQ(g_fls_cb, 0);
    CloseHandle(h);
    /* clean thread exit does run it */
    ResetEvent(ev); SetEvent(ev);
    h = CreateThread(NULL, 0, th_flsset_wait, ev, 0, NULL);
    EQ(tf_join(h), 0);
    EQ(g_fls_cb, 1);
    FlsFree(g_fls_idx);
    CloseHandle(ev);
}

TEST(thr_getcurrentthread_pseudo) {
    REQUIRE(U(GetCurrentThread));
    EQ(U(GetCurrentThread)(), (HANDLE)-2);
    EQ(U(GetCurrentThread)(), U(GetCurrentThread)());
}
TEST(thr_getcurrentthread_pseudo_in_other_thread) {
    REQUIRE(U(GetCurrentThread));
    HANDLE h = U(GetCurrentThread)();
    EQ(h, (HANDLE)(LONG_PTR)-2);
    CHECK(h != GetCurrentProcess());
}
TEST(thr_getcurrentthreadid_matches_teb) {
    REQUIRE(U(GetCurrentThreadId));
    DWORD id = U(GetCurrentThreadId)();
    CHECK(id != 0);
    EQ(id, (DWORD)__readgsqword(0x48));
    EQ(id, GetCurrentThreadId());
    EQ(U(GetCurrentThreadId)(), id);
}
TEST(thr_getcurrentthreadid_differs_per_thread) {
    REQUIRE(U(GetCurrentThreadId));
    DWORD tid;
    HANDLE h = CreateThread(NULL, 0, tf_th_ret, NULL, CREATE_SUSPENDED, &tid);
    CHECK(tid != U(GetCurrentThreadId)());
    ResumeThread(h); tf_join(h);
}
static DWORD WINAPI th_getid(LPVOID p) {
    DWORD (WINAPI *f)(void) = (DWORD (WINAPI *)(void))t_proc("GetCurrentThreadId");
    return f();
}
TEST(thr_getcurrentthreadid_inside_thread) {
    REQUIRE(U(GetCurrentThreadId));
    DWORD tid;
    HANDLE h = CreateThread(NULL, 0, th_getid, NULL, 0, &tid);
    EQ(tf_join(h), tid);
}
TEST(thr_getthreadid_variants) {
    REQUIRE(U(GetThreadId));
    EQ(U(GetThreadId)(GetCurrentThread()), GetCurrentThreadId());
    HANDLE ev = tf_ev(TRUE, FALSE);
    DWORD tid;
    HANDLE h = CreateThread(NULL, 0, tf_th_waitev, ev, 0, &tid);
    EQ(U(GetThreadId)(h), tid);
    SET_ERR(0);
    EQ(U(GetThreadId)(NULL), 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    EQ(U(GetThreadId)(ev), 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    EQ(U(GetThreadId)(INVALID_HANDLE_VALUE), 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SetEvent(ev); tf_join(h); CloseHandle(ev);
}
TEST(thr_getthreadid_after_exit_and_close) {
    REQUIRE(U(GetThreadId));
    DWORD tid;
    HANDLE h = CreateThread(NULL, 0, tf_th_ret, NULL, 0, &tid);
    WaitForSingleObject(h, 5000);
    EQ(U(GetThreadId)(h), tid);
    CloseHandle(h);
    SET_ERR(0);
    EQ(U(GetThreadId)(h), 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(thr_getprocessidofthread) {
    REQUIRE(U(GetProcessIdOfThread));
    EQ(U(GetProcessIdOfThread)(GetCurrentThread()), GetCurrentProcessId());
    DWORD tid;
    HANDLE h = CreateThread(NULL, 0, tf_th_ret, NULL, CREATE_SUSPENDED, &tid);
    EQ(U(GetProcessIdOfThread)(h), GetCurrentProcessId());
    ResumeThread(h); tf_join(h);
    SET_ERR(0);
    EQ(U(GetProcessIdOfThread)(NULL), 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE ev = tf_ev(TRUE, FALSE);
    SET_ERR(0);
    EQ(U(GetProcessIdOfThread)(ev), 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
}
TEST(thr_duplicate_pseudo_handle) {
    REQUIRE(U(GetThreadId));
    HANDLE d = NULL;
    CHECK(DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS));
    CHECK(d != NULL && d != (HANDLE)-2);
    EQ(U(GetThreadId)(d), GetCurrentThreadId());
    CloseHandle(d);
}

TEST(thr_openthread_basic) {
    REQUIRE(U(OpenThread)); REQUIRE(U(GetThreadId));
    HANDLE ev = tf_ev(TRUE, FALSE);
    DWORD tid;
    HANDLE h = CreateThread(NULL, 0, tf_th_waitev, ev, 0, &tid);
    HANDLE o = U(OpenThread)(THREAD_QUERY_LIMITED_INFORMATION, FALSE, tid);
    CHECK(o != NULL);
    CHECK(o != h);
    EQ(U(GetThreadId)(o), tid);
    CloseHandle(o);
    o = U(OpenThread)(THREAD_ALL_ACCESS, FALSE, tid);
    CHECK(o != NULL);
    CloseHandle(o);
    o = U(OpenThread)(SYNCHRONIZE, FALSE, tid);
    CHECK(o != NULL);
    EQ(WaitForSingleObject(o, 10), WAIT_TIMEOUT);
    SetEvent(ev);
    EQ(WaitForSingleObject(o, 5000), WAIT_OBJECT_0);
    CloseHandle(o);
    tf_join(h); CloseHandle(ev);
}
TEST(thr_openthread_self) {
    REQUIRE(U(OpenThread));
    HANDLE o = U(OpenThread)(THREAD_ALL_ACCESS, FALSE, GetCurrentThreadId());
    CHECK(o != NULL);
    if (o) CloseHandle(o);
}
TEST(thr_openthread_inherit_flag) {
    REQUIRE(U(OpenThread));
    HANDLE o = U(OpenThread)(THREAD_QUERY_LIMITED_INFORMATION, TRUE, GetCurrentThreadId());
    DWORD fl = 0; GetHandleInformation(o, &fl);
    CHECK(fl & HANDLE_FLAG_INHERIT);
    CloseHandle(o);
    o = U(OpenThread)(THREAD_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentThreadId());
    fl = 0; GetHandleInformation(o, &fl);
    CHECK(!(fl & HANDLE_FLAG_INHERIT));
    CloseHandle(o);
}
TEST(thr_openthread_nonexistent) {
    REQUIRE(U(OpenThread));
    static const DWORD ids[] = { 0, 1, 2, 3, 0xFFFFFFFC, 0xFFFFFFFF, 0x7FFFFFFC };
    for (size_t i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        SET_ERR(0);
        HANDLE o = U(OpenThread)(THREAD_QUERY_LIMITED_INFORMATION, FALSE, ids[i]);
        CHECKF(o == NULL, "tid %lx", ids[i]);
        DWORD e = GetLastError();
        CHECKF(e == ERROR_INVALID_PARAMETER, "tid %lx err %lu", ids[i], e);
        if (o) CloseHandle(o);
    }
}
TEST(thr_openthread_zero_access) {
    REQUIRE(U(OpenThread));
    HANDLE o = U(OpenThread)(0, FALSE, GetCurrentThreadId());
    t_note("OpenThread(access 0) -> %p err %lu", o, GetLastError());
    if (o) CloseHandle(o);
}
TEST(thr_openthread_rights_enforced) {
    REQUIRE(U(OpenThread)); REQUIRE(U(SuspendThread)); REQUIRE(U(ResumeThread));
    REQUIRE(U(SetThreadPriority)); REQUIRE(U(GetThreadPriority));
    HANDLE ev = tf_ev(TRUE, FALSE);
    DWORD tid;
    HANDLE h = CreateThread(NULL, 0, tf_th_waitev, ev, 0, &tid);
    HANDLE o = U(OpenThread)(THREAD_QUERY_LIMITED_INFORMATION, FALSE, tid);
    SET_ERR(0);
    EQ(U(SuspendThread)(o), (DWORD)-1);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    SET_ERR(0);
    EQ(U(ResumeThread)(o), (DWORD)-1);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    SET_ERR(0);
    CHECK(!U(SetThreadPriority)(o, 0));
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    SET_ERR(0);
    EQ(U(GetThreadPriority)(o), 0);   /* THREAD_QUERY_LIMITED_INFORMATION suffices */
    CloseHandle(o);
    o = U(OpenThread)(SYNCHRONIZE, FALSE, tid);
    SET_ERR(0);
    EQ(U(GetThreadPriority)(o), 0x7FFFFFFF);   /* THREAD_PRIORITY_ERROR_RETURN */
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(o);
    SetEvent(ev); tf_join(h); CloseHandle(ev);
}

TEST(thr_times_basic) {
    REQUIRE(U(GetThreadTimes)); REQUIRE(U(CreateThread));
    FILETIME before, after, cr, ex, kt, ut;
    GetSystemTimeAsFileTime(&before);
    HANDLE ev = tf_ev(TRUE, FALSE);
    HANDLE h = U(CreateThread)(NULL, 0, tf_th_waitev, ev, 0, NULL);
    Sleep(30);
    GetSystemTimeAsFileTime(&after);
    memset(&cr, 0xCC, sizeof cr); memset(&ex, 0xCC, sizeof ex);
    CHECK(U(GetThreadTimes)(h, &cr, &ex, &kt, &ut));
    ULONGLONG b = ((ULONGLONG)before.dwHighDateTime << 32) | before.dwLowDateTime;
    ULONGLONG a = ((ULONGLONG)after.dwHighDateTime << 32) | after.dwLowDateTime;
    ULONGLONG c = ((ULONGLONG)cr.dwHighDateTime << 32) | cr.dwLowDateTime;
    CHECKF(c + 20000000ULL >= b && c <= a + 20000000ULL, "creation time out of window");
    EQ(ex.dwLowDateTime, 0);          /* still running: exit time zero */
    EQ(ex.dwHighDateTime, 0);
    SetEvent(ev); tf_join(h); CloseHandle(ev);
}
TEST(thr_times_exited_has_exit_time) {
    REQUIRE(U(GetThreadTimes));
    HANDLE h = CreateThread(NULL, 0, tf_th_ret, NULL, 0, NULL);
    WaitForSingleObject(h, 5000);
    FILETIME cr, ex, kt, ut;
    CHECK(U(GetThreadTimes)(h, &cr, &ex, &kt, &ut));
    ULONGLONG c = ((ULONGLONG)cr.dwHighDateTime << 32) | cr.dwLowDateTime;
    ULONGLONG e = ((ULONGLONG)ex.dwHighDateTime << 32) | ex.dwLowDateTime;
    CHECK(e != 0);
    CHECK(e >= c);
    CloseHandle(h);
}
TEST(thr_times_current_thread_pseudo) {
    REQUIRE(U(GetThreadTimes));
    FILETIME cr, ex, kt, ut;
    CHECK(U(GetThreadTimes)(GetCurrentThread(), &cr, &ex, &kt, &ut));
    CHECK(cr.dwHighDateTime != 0);
    EQ(ex.dwHighDateTime, 0);
}
static DWORD WINAPI th_burn(LPVOID p) {
    ULONGLONG end = GetTickCount64() + 80;
    volatile unsigned long long s = 0;
    while (GetTickCount64() < end) s += 1;
    return 0;
}
TEST(thr_times_cpu_accumulates) {
    REQUIRE(U(GetThreadTimes));
    HANDLE h = CreateThread(NULL, 0, th_burn, NULL, 0, NULL);
    WaitForSingleObject(h, 5000);
    FILETIME cr, ex, kt, ut;
    CHECK(U(GetThreadTimes)(h, &cr, &ex, &kt, &ut));
    ULONGLONG k = ((ULONGLONG)kt.dwHighDateTime << 32) | kt.dwLowDateTime;
    ULONGLONG u = ((ULONGLONG)ut.dwHighDateTime << 32) | ut.dwLowDateTime;
    if (t_is_wine() && k + u == 0) t_note("wine: no cpu time accounting"); else
    CHECKF(k + u > 0, "no cpu time accounted");
    CHECK(k + u < 100000000ULL);   /* < 10 s */
    CloseHandle(h);
}
TEST(thr_times_invalid_handle) {
    REQUIRE(U(GetThreadTimes));
    FILETIME a, b, c, d;
    SET_ERR(0);
    CHECK(!U(GetThreadTimes)(NULL, &a, &b, &c, &d));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE ev = tf_ev(TRUE, FALSE);
    SET_ERR(0);
    CHECK(!U(GetThreadTimes)(ev, &a, &b, &c, &d));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
}
TEST(thr_cycletime) {
    REQUIRE(UN(QueryThreadCycleTime));
    ULONG64 c1 = 0, c2 = 0;
    CHECK(UN(QueryThreadCycleTime)(GetCurrentThread(), &c1));
    CHECK(c1 != 0);
    th_burn(NULL);
    CHECK(UN(QueryThreadCycleTime)(GetCurrentThread(), &c2));
    CHECK(c2 > c1);
    SET_ERR(0);
    CHECK(!UN(QueryThreadCycleTime)(NULL, &c1));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}

TEST(thr_wait_semantics) {
    HANDLE ev = tf_ev(TRUE, FALSE);
    HANDLE h = CreateThread(NULL, 0, tf_th_waitev, ev, 0, NULL);
    EQ(WaitForSingleObject(h, 0), WAIT_TIMEOUT);
    SetEvent(ev);
    EQ(WaitForSingleObject(h, 5000), WAIT_OBJECT_0);
    EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);   /* stays signalled */
    CloseHandle(h); CloseHandle(ev);
}
TEST(thr_ioflag) {
    REQUIRE(U(GetThreadIOPendingFlag));
    BOOL f = 0x1234;
    CHECK(U(GetThreadIOPendingFlag)(GetCurrentThread(), &f));
    EQ(f, FALSE);
    HANDLE ev = tf_ev(TRUE, FALSE);
    HANDLE h = CreateThread(NULL, 0, tf_th_waitev, ev, 0, NULL);
    f = 1;
    CHECK(U(GetThreadIOPendingFlag)(h, &f));
    EQ(f, FALSE);
    SetEvent(ev); tf_join(h); CloseHandle(ev);
}
TEST(thr_ioflag_invalid) {
    REQUIRE(U(GetThreadIOPendingFlag));
    BOOL f = 0;
    SET_ERR(0);
    CHECK(!U(GetThreadIOPendingFlag)(NULL, &f));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE ev = tf_ev(TRUE, FALSE);
    SET_ERR(0);
    CHECK(!U(GetThreadIOPendingFlag)(ev, &f));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
}

TEST(thr_many_unique_ids) {
    REQUIRE(U(CreateThread));
    enum { N = 64 };
    HANDLE hs[N]; DWORD ids[N];
    HANDLE ev = tf_ev(TRUE, FALSE);
    for (int i = 0; i < N; i++) { hs[i] = U(CreateThread)(NULL, 0, tf_th_waitev, ev, 0, &ids[i]); CHECK(hs[i] != NULL); }
    int dup = 0;
    for (int i = 0; i < N; i++) for (int j = i + 1; j < N; j++) if (ids[i] == ids[j]) dup++;
    EQ(dup, 0);
    SetEvent(ev);
    for (int i = 0; i < N; i++) EQ(tf_join(hs[i]), 0x77);
    CloseHandle(ev);
}
TEST(thr_create_close_handle_thread_runs) {
    REQUIRE(U(CreateThread));
    InterlockedExchange(&g_flag, 0);
    HANDLE h = U(CreateThread)(NULL, 0, th_setflag, NULL, 0, NULL);
    CloseHandle(h);   /* closing the handle does not stop the thread */
    for (int i = 0; i < 200 && !g_flag; i++) Sleep(10);
    EQ(g_flag, 1);
}
TEST(thr_create_loop_100) {
    REQUIRE(U(CreateThread));
    for (int i = 0; i < 100; i++) {
        HANDLE h = U(CreateThread)(NULL, 0, tf_th_ret, (LPVOID)(ULONG_PTR)i, 0, NULL);
        if (!h) { CHECKF(0, "create %d failed", i); break; }
        CHECK(tf_join(h) == (DWORD)i);
    }
}
