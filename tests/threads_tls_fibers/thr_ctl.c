/* Suspend/Resume, thread context, priority, affinity, ideal processor, description, info. */
#include "tf.h"

typedef struct { HANDLE h; HANDLE ev; DWORD tid; } worker;
static void w_start(worker *w) { w->ev = tf_ev(TRUE, FALSE); w->h = CreateThread(NULL, 0, tf_th_waitev, w->ev, 0, &w->tid); }
static void w_stop(worker *w) { SetEvent(w->ev); tf_join(w->h); CloseHandle(w->ev); w->h = w->ev = NULL; }
static HANDLE w_open(worker *w, DWORD access) { return OpenThread(access, FALSE, w->tid); }

/* ---------- suspend / resume ---------- */
TEST(thrctl_suspend_resume_counts) {
    REQUIRE(U(SuspendThread)); REQUIRE(U(ResumeThread));
    worker w; w_start(&w);
    EQ(U(ResumeThread)(w.h), 0);                  /* not suspended */
    EQ(U(SuspendThread)(w.h), 0);
    EQ(U(SuspendThread)(w.h), 1);
    EQ(U(SuspendThread)(w.h), 2);
    EQ(U(ResumeThread)(w.h), 3);
    EQ(U(ResumeThread)(w.h), 2);
    EQ(U(ResumeThread)(w.h), 1);
    EQ(U(ResumeThread)(w.h), 0);
    EQ(U(ResumeThread)(w.h), 0);
    w_stop(&w);
}
TEST(thrctl_suspend_blocks_exit) {
    REQUIRE(U(SuspendThread)); REQUIRE(U(ResumeThread));
    worker w; w_start(&w);
    Sleep(30);
    EQ(U(SuspendThread)(w.h), 0);
    SetEvent(w.ev);
    Sleep(150);
    EQ(WaitForSingleObject(w.h, 0), WAIT_TIMEOUT);
    DWORD c = 0; GetExitCodeThread(w.h, &c); EQ(c, STILL_ACTIVE);
    EQ(U(ResumeThread)(w.h), 1);
    EQ(WaitForSingleObject(w.h, 5000), WAIT_OBJECT_0);
    CloseHandle(w.h); CloseHandle(w.ev);
}
TEST(thrctl_suspend_max_count) {
    REQUIRE(U(SuspendThread)); REQUIRE(U(ResumeThread));
    worker w; w_start(&w);
    Sleep(20);
    int bad = 0;
    for (int i = 0; i < MAXIMUM_SUSPEND_COUNT; i++) { DWORD r = U(SuspendThread)(w.h); if (r != (DWORD)i) bad++; }
    EQ(bad, 0);
    SET_ERR(0);
    EQ(U(SuspendThread)(w.h), (DWORD)-1);
    DWORD e = GetLastError();
    CHECK(e != 0);
    t_note("suspend beyond MAXIMUM_SUSPEND_COUNT: err %lu", e);
    bad = 0;
    for (int i = MAXIMUM_SUSPEND_COUNT; i > 0; i--) { DWORD r = U(ResumeThread)(w.h); if (r != (DWORD)i) bad++; }
    EQ(bad, 0);
    EQ(U(ResumeThread)(w.h), 0);
    w_stop(&w);
}
TEST(thrctl_suspend_invalid_handles) {
    REQUIRE(U(SuspendThread)); REQUIRE(U(ResumeThread));
    HANDLE ev = tf_ev(TRUE, FALSE);
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, ev };
    for (int i = 0; i < 3; i++) {
        SET_ERR(0);
        EQ(U(SuspendThread)(bad[i]), (DWORD)-1);
        EXPECT_ERR(ERROR_INVALID_HANDLE);
        SET_ERR(0);
        EQ(U(ResumeThread)(bad[i]), (DWORD)-1);
        EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
    CloseHandle(ev);
}
TEST(thrctl_suspend_closed_handle) {
    REQUIRE(U(SuspendThread)); REQUIRE(U(ResumeThread));
    HANDLE h = CreateThread(NULL, 0, tf_th_ret, NULL, CREATE_SUSPENDED, NULL);
    ResumeThread(h); WaitForSingleObject(h, 5000); CloseHandle(h);
    SET_ERR(0);
    EQ(U(SuspendThread)(h), (DWORD)-1);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(thrctl_suspend_exited_thread) {
    REQUIRE(U(SuspendThread)); REQUIRE(U(ResumeThread));
    HANDLE h = CreateThread(NULL, 0, tf_th_ret, NULL, 0, NULL);
    WaitForSingleObject(h, 5000);
    SET_ERR(0);
    EQ(U(SuspendThread)(h), (DWORD)-1);
    t_note("SuspendThread(exited) err %lu", GetLastError());
    SET_ERR(0);
    DWORD r = U(ResumeThread)(h);
    t_note("ResumeThread(exited) ret %lx err %lu", r, GetLastError());
    CloseHandle(h);
}
TEST(thrctl_suspend_access_denied) {
    REQUIRE(U(SuspendThread)); REQUIRE(U(ResumeThread));
    worker w; w_start(&w);
    HANDLE o = w_open(&w, SYNCHRONIZE | THREAD_QUERY_LIMITED_INFORMATION);
    SET_ERR(0);
    EQ(U(SuspendThread)(o), (DWORD)-1);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(o);
    o = w_open(&w, THREAD_SUSPEND_RESUME);
    CHECK(o != NULL);
    EQ(U(SuspendThread)(o), 0);
    EQ(U(ResumeThread)(o), 1);
    CloseHandle(o);
    w_stop(&w);
}
TEST(thrctl_wow64_suspend_native) {
    REQUIRE(U(Wow64SuspendThread)); REQUIRE(U(ResumeThread));
    worker w; w_start(&w);
    SET_ERR(0);
    DWORD r = U(Wow64SuspendThread)(w.h);
    t_note("Wow64SuspendThread(native x64 thread) ret %lx err %lu", r, GetLastError());
    if (r != (DWORD)-1) ResumeThread(w.h);
    SET_ERR(0);
    r = U(Wow64SuspendThread)(NULL);
    EQ(r, (DWORD)-1);
    t_note("Wow64SuspendThread(NULL) err %lu", GetLastError());
    w_stop(&w);
}
TEST(thrctl_wow64_context_native) {
    REQUIRE(U(Wow64GetThreadContext)); REQUIRE(U(Wow64SetThreadContext));
    worker w; w_start(&w);
    WOW64_CONTEXT wc; memset(&wc, 0, sizeof wc); wc.ContextFlags = WOW64_CONTEXT_CONTROL;
    SET_ERR(0);
    BOOL r = U(Wow64GetThreadContext)(w.h, &wc);
    t_note("Wow64GetThreadContext(native) ret %d err %lu", r, GetLastError());
    SET_ERR(0);
    r = U(Wow64SetThreadContext)(NULL, &wc);
    CHECK(!r);
    t_note("Wow64SetThreadContext(NULL) err %lu", GetLastError());
    w_stop(&w);
}

/* ---------- thread context ---------- */
static HANDLE spin_start(void) {
    /* Start tf_regloop and return a suspended (count 1) handle with Rip inside the loop; NULL on failure. */
    DWORD tid;
    HANDLE h = CreateThread(NULL, 0, tf_regloop, NULL, 0, &tid);
    if (!h) return NULL;
    for (int i = 0; i < 3000; i++) {
        Sleep(1);
        if (SuspendThread(h) == (DWORD)-1) break;
        CONTEXT c; memset(&c, 0, sizeof c); c.ContextFlags = CONTEXT_CONTROL;
        if (GetThreadContext(h, &c) && c.Rip >= (DWORD64)(ULONG_PTR)tf_regloop && c.Rip < (DWORD64)(ULONG_PTR)tf_regloop_end) return h;
        ResumeThread(h);
    }
    TerminateThread(h, 0); CloseHandle(h);
    return NULL;
}
static DWORD spin_finish(HANDLE h, DWORD code) {
    CONTEXT c; memset(&c, 0, sizeof c); c.ContextFlags = CONTEXT_INTEGER;
    GetThreadContext(h, &c);
    c.Rax = code;
    SetThreadContext(h, &c);
    ResumeThread(h);
    return tf_join(h);
}

TEST(thrctx_control_rip_in_loop) {
    REQUIRE(U(GetThreadContext));
    HANDLE h = spin_start();
    CHECK(h != NULL); if (!h) return;
    CONTEXT c; memset(&c, 0xCC, sizeof c); c.ContextFlags = CONTEXT_CONTROL;
    CHECK(U(GetThreadContext)(h, &c));
    CHECK(c.Rip >= (DWORD64)(ULONG_PTR)tf_regloop && c.Rip < (DWORD64)(ULONG_PTR)tf_regloop_end);
    EQ(c.SegCs, 0x33);
    EQ(c.SegSs, 0x2B);
    CHECK(c.EFlags & 0x200);   /* IF */
    EQ(c.ContextFlags & CONTEXT_CONTROL, CONTEXT_CONTROL);
    EQ(spin_finish(h, 1), 1);
}
TEST(thrctx_rsp_in_stack_and_aligned) {
    REQUIRE(U(GetThreadContext)); REQUIRE(UNT(NtQueryInformationThread));
    HANDLE h = spin_start();
    CHECK(h != NULL); if (!h) return;
    CONTEXT c; memset(&c, 0, sizeof c); c.ContextFlags = CONTEXT_CONTROL;
    CHECK(U(GetThreadContext)(h, &c));
    struct { LONG st; PVOID teb; } tbi[4]; memset(tbi, 0, sizeof tbi);
    typedef LONG (WINAPI *NQIT)(HANDLE, ULONG, PVOID, ULONG, PULONG);
    NQIT q = (NQIT)t_proc_nt("NtQueryInformationThread");
    LONG st = q(h, 0, tbi, 48, NULL);
    EQ(st, 0);
    char *teb = (char *)tbi[0].teb;
    CHECK(teb != NULL);
    if (teb) {
        ULONG_PTR base = *(ULONG_PTR *)(teb + 8), dealloc = *(ULONG_PTR *)(teb + 0x1478);
        CHECK(c.Rsp < base && c.Rsp >= dealloc);
        EQ(c.Rsp & 0xF, 8);   /* inside a function body entered by CALL: Rsp = 8 mod 16 */
    }
    EQ(spin_finish(h, 2), 2);
}
TEST(thrctx_set_rax_changes_result) {
    REQUIRE(U(GetThreadContext)); REQUIRE(U(SetThreadContext));
    static const DWORD v[] = { 1, 0x7788, 0xFFFFFFFF, 0x80000000 };
    for (size_t i = 0; i < sizeof v / sizeof v[0]; i++) {
        HANDLE h = spin_start();
        CHECK(h != NULL); if (!h) return;
        CONTEXT c; memset(&c, 0, sizeof c); c.ContextFlags = CONTEXT_INTEGER;
        CHECK(U(GetThreadContext)(h, &c));
        EQ(c.Rax & 0xFFFFFFFF, 0);
        c.Rax = v[i];
        CHECK(U(SetThreadContext)(h, &c));
        CONTEXT d; memset(&d, 0, sizeof d); d.ContextFlags = CONTEXT_INTEGER;
        CHECK(U(GetThreadContext)(h, &d));
        EQ(d.Rax, v[i]);
        ResumeThread(h);
        EQ(tf_join(h), v[i]);
    }
}
TEST(thrctx_set_preserves_other_registers) {
    REQUIRE(U(GetThreadContext)); REQUIRE(U(SetThreadContext));
    HANDLE h = spin_start();
    CHECK(h != NULL); if (!h) return;
    CONTEXT a; memset(&a, 0, sizeof a); a.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    CHECK(U(GetThreadContext)(h, &a));
    CONTEXT s = a; s.Rbx = 0x1122334455667788ULL; s.R12 = 0xAABBCCDDEEFF0011ULL; s.Rax = 0;
    CHECK(U(SetThreadContext)(h, &s));
    CONTEXT b; memset(&b, 0, sizeof b); b.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    CHECK(U(GetThreadContext)(h, &b));
    EQ(b.Rbx, 0x1122334455667788ULL);
    EQ(b.R12, 0xAABBCCDDEEFF0011ULL);
    EQ(b.Rip, a.Rip);
    EQ(b.Rsp, a.Rsp);
    EQ(spin_finish(h, 3), 3);
}
TEST(thrctx_integer_only_leaves_control_fields) {
    REQUIRE(U(GetThreadContext));
    HANDLE h = spin_start();
    CHECK(h != NULL); if (!h) return;
    CONTEXT c; memset(&c, 0xCC, sizeof c); c.ContextFlags = CONTEXT_INTEGER;
    CHECK(U(GetThreadContext)(h, &c));
    EQ(c.ContextFlags & CONTEXT_INTEGER, CONTEXT_INTEGER);
    EQ(c.SegCs, 0xCCCC);          /* control fields not touched */
    EQ(c.Rip, 0xCCCCCCCCCCCCCCCCULL);
    EQ(spin_finish(h, 4), 4);
}
TEST(thrctx_floating_point_mxcsr) {
    REQUIRE(U(GetThreadContext));
    HANDLE h = spin_start();
    CHECK(h != NULL); if (!h) return;
    CONTEXT c; memset(&c, 0, sizeof c); c.ContextFlags = CONTEXT_FLOATING_POINT;
    CHECK(U(GetThreadContext)(h, &c));
    EQ(c.MxCsr, 0x1F80);
    EQ(c.FltSave.MxCsr, 0x1F80);
    EQ(c.FltSave.ControlWord, 0x27F);
    EQ(spin_finish(h, 5), 5);
}
TEST(thrctx_debug_registers_clear) {
    REQUIRE(U(GetThreadContext));
    HANDLE h = spin_start();
    CHECK(h != NULL); if (!h) return;
    CONTEXT c; memset(&c, 0xCC, sizeof c); c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    CHECK(U(GetThreadContext)(h, &c));
    EQ(c.ContextFlags & CONTEXT_DEBUG_REGISTERS, CONTEXT_DEBUG_REGISTERS);
    EQ(c.Dr7 & 0xFF, 0);
    EQ(spin_finish(h, 6), 6);
}
TEST(thrctx_full_and_all) {
    REQUIRE(U(GetThreadContext));
    HANDLE h = spin_start();
    CHECK(h != NULL); if (!h) return;
    CONTEXT c; memset(&c, 0, sizeof c); c.ContextFlags = CONTEXT_FULL;
    CHECK(U(GetThreadContext)(h, &c));
    CHECK(c.Rip != 0 && c.Rsp != 0);
    memset(&c, 0, sizeof c); c.ContextFlags = CONTEXT_ALL;
    CHECK(U(GetThreadContext)(h, &c));
    CHECK(c.Rip != 0);
    memset(&c, 0, sizeof c); c.ContextFlags = CONTEXT_SEGMENTS;
    CHECK(U(GetThreadContext)(h, &c));
    EQ(c.SegGs & 0xFFFF, 0x2B);
    EQ(spin_finish(h, 7), 7);
}
TEST(thrctx_flags_zero_and_arch_only) {
    REQUIRE(U(GetThreadContext));
    HANDLE h = spin_start();
    CHECK(h != NULL); if (!h) return;
    CONTEXT c; memset(&c, 0, sizeof c);
    c.ContextFlags = 0;
    SET_ERR(0);
    BOOL r = U(GetThreadContext)(h, &c);
    t_note("GetThreadContext(flags=0) ret %d err %lu", r, GetLastError());
    c.ContextFlags = CONTEXT_AMD64;
    SET_ERR(0);
    r = U(GetThreadContext)(h, &c);
    t_note("GetThreadContext(flags=CONTEXT_AMD64) ret %d err %lu", r, GetLastError());
    c.ContextFlags = 0x00010001;   /* i386 control flags */
    SET_ERR(0);
    r = U(GetThreadContext)(h, &c);
    t_note("GetThreadContext(i386 flags) ret %d err %lu", r, GetLastError());
    EQ(spin_finish(h, 8), 8);
}
TEST(thrctx_xstate) {
    REQUIRE(U(GetThreadContext)); REQUIRE(UN(InitializeContext)); REQUIRE(U(SetThreadContext));
    HANDLE h = spin_start();
    CHECK(h != NULL); if (!h) return;
    DWORD len = 0;
    SET_ERR(0);
    BOOL r = UN(InitializeContext)(NULL, CONTEXT_FULL | CONTEXT_XSTATE, NULL, &len);
    if (r || GetLastError() != ERROR_INSUFFICIENT_BUFFER || len < sizeof(CONTEXT)) {
        t_note("InitializeContext query: r=%d err=%lu len=%lu", r, GetLastError(), len);
        spin_finish(h, 9); return;
    }
    void *buf = HeapAlloc(GetProcessHeap(), 0, len);
    PCONTEXT pc = NULL;
    CHECK(UN(InitializeContext)(buf, CONTEXT_FULL | CONTEXT_XSTATE, &pc, &len));
    if (pc) {
        SET_ERR(0);
        r = U(GetThreadContext)(h, pc);
        CHECK(r);
        t_note("GetThreadContext(XSTATE) ret %d err %lu flags %lx", r, GetLastError(), pc->ContextFlags);
        if (r) {
            SET_ERR(0);
            r = U(SetThreadContext)(h, pc);
            t_note("SetThreadContext(XSTATE) ret %d err %lu", r, GetLastError());
        }
    }
    HeapFree(GetProcessHeap(), 0, buf);
    CONTEXT c; memset(&c, 0, sizeof c); c.ContextFlags = CONTEXT_INTEGER;
    GetThreadContext(h, &c);
    c.Rax = 9; SetThreadContext(h, &c); ResumeThread(h);
    EQ(tf_join(h), 9);
}
TEST(thrctx_invalid_handles) {
    REQUIRE(U(GetThreadContext)); REQUIRE(U(SetThreadContext));
    CONTEXT c; memset(&c, 0, sizeof c); c.ContextFlags = CONTEXT_CONTROL;
    HANDLE ev = tf_ev(TRUE, FALSE);
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, ev };
    for (int i = 0; i < 3; i++) {
        SET_ERR(0);
        CHECK(!U(GetThreadContext)(bad[i], &c));
        EXPECT_ERR(ERROR_INVALID_HANDLE);
        SET_ERR(0);
        CHECK(!U(SetThreadContext)(bad[i], &c));
        EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
    CloseHandle(ev);
}
TEST(thrctx_access_rights) {
    REQUIRE(U(GetThreadContext)); REQUIRE(U(SetThreadContext));
    worker w; w_start(&w);
    CONTEXT c; memset(&c, 0, sizeof c); c.ContextFlags = CONTEXT_CONTROL;
    HANDLE o = w_open(&w, THREAD_QUERY_LIMITED_INFORMATION);
    SET_ERR(0);
    CHECK(!U(GetThreadContext)(o, &c));
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    SET_ERR(0);
    CHECK(!U(SetThreadContext)(o, &c));
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(o);
    o = w_open(&w, THREAD_GET_CONTEXT);
    CHECK(o != NULL);
    CHECK(U(GetThreadContext)(o, &c));
    SET_ERR(0);
    CHECK(!U(SetThreadContext)(o, &c));
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(o);
    o = w_open(&w, THREAD_SET_CONTEXT);
    SET_ERR(0);
    CHECK(!U(GetThreadContext)(o, &c));
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(o);
    w_stop(&w);
}
TEST(thrctx_running_thread_ok) {
    REQUIRE(U(GetThreadContext));
    worker w; w_start(&w);
    Sleep(20);
    CONTEXT c; memset(&c, 0, sizeof c); c.ContextFlags = CONTEXT_CONTROL;
    SET_ERR(0);
    BOOL r = U(GetThreadContext)(w.h, &c);
    t_note("GetThreadContext(running thread) ret %d err %lu rip %llx", r, GetLastError(), c.Rip);
    w_stop(&w);
}
TEST(thrctx_new_suspended_thread_initial_state) {
    REQUIRE(U(GetThreadContext));
    HANDLE h = CreateThread(NULL, 0, tf_th_ret, (LPVOID)0x4321, CREATE_SUSPENDED, NULL);
    CONTEXT c; memset(&c, 0, sizeof c); c.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    CHECK(U(GetThreadContext)(h, &c));
    HMODULE nt = GetModuleHandleA("ntdll.dll");
    CHECK(tf_in_module(nt, (ULONG_PTR)c.Rip));
    EQ(c.SegCs, 0x33);
    t_note("new thread: Rcx=%llx Rdx=%llx (start=%p param=0x4321)", c.Rcx, c.Rdx, (void *)tf_th_ret);
    CHECK(c.Rsp != 0);
    TerminateThread(h, 0); CloseHandle(h);
}
TEST(thrctx_new_thread_set_param_register) {
    REQUIRE(U(GetThreadContext)); REQUIRE(U(SetThreadContext));
    HANDLE h = CreateThread(NULL, 0, tf_th_ret, (LPVOID)0x4321, CREATE_SUSPENDED, NULL);
    CONTEXT c; memset(&c, 0, sizeof c); c.ContextFlags = CONTEXT_INTEGER;
    CHECK(U(GetThreadContext)(h, &c));
    if (c.Rdx == 0x4321 && c.Rcx == (DWORD64)(ULONG_PTR)tf_th_ret) {
        c.Rdx = 0x9999;
        CHECK(U(SetThreadContext)(h, &c));
        ResumeThread(h);
        EQ(tf_join(h), 0x9999);
    } else {
        t_note("initial Rcx/Rdx not (start,param): %llx %llx", c.Rcx, c.Rdx);
        TerminateThread(h, 0); CloseHandle(h);
    }
}
TEST(thrctx_new_thread_redirect_start) {
    REQUIRE(U(GetThreadContext)); REQUIRE(U(SetThreadContext));
    HANDLE h = CreateThread(NULL, 0, tf_th_ret, (LPVOID)0x4321, CREATE_SUSPENDED, NULL);
    CONTEXT c; memset(&c, 0, sizeof c); c.ContextFlags = CONTEXT_INTEGER;
    CHECK(U(GetThreadContext)(h, &c));
    if (c.Rcx == (DWORD64)(ULONG_PTR)tf_th_ret) {
        c.Rcx = (DWORD64)(ULONG_PTR)tf_th_store;
        c.Rdx = 0xABCD;
        tf_last_param = 0;
        CHECK(U(SetThreadContext)(h, &c));
        ResumeThread(h);
        EQ(tf_join(h), 0);
        EQ(tf_last_param, 0xABCD);
    } else { t_note("Rcx not start address"); TerminateThread(h, 0); CloseHandle(h); }
}
TEST(thrctx_exited_thread) {
    REQUIRE(U(GetThreadContext));
    HANDLE h = CreateThread(NULL, 0, tf_th_ret, NULL, 0, NULL);
    WaitForSingleObject(h, 5000);
    CONTEXT c; memset(&c, 0, sizeof c); c.ContextFlags = CONTEXT_CONTROL;
    SET_ERR(0);
    BOOL r = U(GetThreadContext)(h, &c);
    t_note("GetThreadContext(exited) ret %d err %lu", r, GetLastError());
    CloseHandle(h);
}
TEST(thrctx_own_thread_pseudo) {
    REQUIRE(U(GetThreadContext));
    CONTEXT c; memset(&c, 0, sizeof c); c.ContextFlags = CONTEXT_CONTROL;
    SET_ERR(0);
    BOOL r = U(GetThreadContext)(GetCurrentThread(), &c);
    t_note("GetThreadContext(self) ret %d err %lu", r, GetLastError());
}

/* ---------- priority ---------- */
static const int levels[] = { THREAD_PRIORITY_IDLE, THREAD_PRIORITY_LOWEST, THREAD_PRIORITY_BELOW_NORMAL, THREAD_PRIORITY_NORMAL,
    THREAD_PRIORITY_ABOVE_NORMAL, THREAD_PRIORITY_HIGHEST, THREAD_PRIORITY_TIME_CRITICAL };
TEST(thrprio_default_normal) {
    REQUIRE(U(GetThreadPriority));
    worker w; w_start(&w);
    EQ(U(GetThreadPriority)(w.h), THREAD_PRIORITY_NORMAL);
    w_stop(&w);
}
TEST(thrprio_set_get_all_levels) {
    REQUIRE(U(GetThreadPriority)); REQUIRE(U(SetThreadPriority));
    worker w; w_start(&w);
    for (size_t i = 0; i < sizeof levels / sizeof levels[0]; i++) {
        CHECKF(U(SetThreadPriority)(w.h, levels[i]), "level %d", levels[i]);
        EQ(U(GetThreadPriority)(w.h), levels[i]);
    }
    CHECK(U(SetThreadPriority)(w.h, THREAD_PRIORITY_NORMAL));
    w_stop(&w);
}
TEST(thrprio_constants) {
    EQ(THREAD_PRIORITY_IDLE, -15);
    EQ(THREAD_PRIORITY_TIME_CRITICAL, 15);
    EQ(THREAD_PRIORITY_LOWEST, -2);
    EQ(THREAD_PRIORITY_HIGHEST, 2);
    EQ(THREAD_MODE_BACKGROUND_BEGIN, 0x10000);
    EQ(THREAD_MODE_BACKGROUND_END, 0x20000);
    EQ(THREAD_PRIORITY_ERROR_RETURN, 0x7FFFFFFF);
}
TEST(thrprio_repeat_same) {
    REQUIRE(U(GetThreadPriority)); REQUIRE(U(SetThreadPriority));
    worker w; w_start(&w);
    for (int i = 0; i < 20; i++) { CHECK(U(SetThreadPriority)(w.h, THREAD_PRIORITY_ABOVE_NORMAL)); }
    EQ(U(GetThreadPriority)(w.h), THREAD_PRIORITY_ABOVE_NORMAL);
    w_stop(&w);
}
TEST(thrprio_invalid_values) {
    REQUIRE(U(SetThreadPriority)); REQUIRE(U(GetThreadPriority));
    worker w; w_start(&w);
    static const int bad[] = { 100, -100, 0x7FFFFFFF, (int)0x80000000 };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SET_ERR(0);
        CHECKF(!U(SetThreadPriority)(w.h, bad[i]), "value %d", bad[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
    EQ(U(GetThreadPriority)(w.h), THREAD_PRIORITY_NORMAL);
    static const int odd[] = { 3, -3, 5, -5, 14, -14, 16, -16 };
    for (size_t i = 0; i < sizeof odd / sizeof odd[0]; i++) {
        SET_ERR(0);
        BOOL r = U(SetThreadPriority)(w.h, odd[i]);
        t_note("SetThreadPriority(%d) ret %d err %lu now %d", odd[i], r, GetLastError(), U(GetThreadPriority)(w.h));
        U(SetThreadPriority)(w.h, THREAD_PRIORITY_NORMAL);
    }
    w_stop(&w);
}
TEST(thrprio_background_mode_invalid_combo) {
    REQUIRE(U(SetThreadPriority));
    worker w; w_start(&w);
    SET_ERR(0);
    CHECK(!U(SetThreadPriority)(w.h, 0x30000));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    /* background mode applies to the calling thread only */
    SET_ERR(0);
    CHECK(!U(SetThreadPriority)(w.h, THREAD_MODE_BACKGROUND_BEGIN));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!U(SetThreadPriority)(w.h, THREAD_MODE_BACKGROUND_END));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    w_stop(&w);
}
static DWORD WINAPI th_bgmode(LPVOID p) {
    BOOL (WINAPI *sp)(HANDLE, int) = (BOOL (WINAPI *)(HANDLE, int))t_proc("SetThreadPriority");
    int (WINAPI *gp)(HANDLE) = (int (WINAPI *)(HANDLE))t_proc("GetThreadPriority");
    DWORD bits = 0;
    SetLastError(0);
    if (!sp(GetCurrentThread(), THREAD_MODE_BACKGROUND_END)) { bits |= 1; if (GetLastError() != 401) bits |= 0x100; } /* ERROR_THREAD_MODE_NOT_BACKGROUND */
    if (!sp(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN)) bits |= 2;
    SetLastError(0);
    if (sp(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN)) bits |= 4;
    else t_note("BACKGROUND_BEGIN twice: err %lu (documented ERROR_THREAD_MODE_ALREADY_BACKGROUND 400)", GetLastError());
    int pr = gp(GetCurrentThread());
    if (pr == 0x7FFFFFFF) bits |= 8;
    if (!sp(GetCurrentThread(), THREAD_MODE_BACKGROUND_END)) bits |= 0x10;
    SetLastError(0);
    if (sp(GetCurrentThread(), THREAD_MODE_BACKGROUND_END)) bits |= 0x20;
    else if (GetLastError() != 401) bits |= 0x400;
    if (gp(GetCurrentThread()) != pr && pr != 0 && gp(GetCurrentThread()) != 0) bits |= 0x40;
    return bits;
}
TEST(thrprio_background_mode_lifecycle) {
    REQUIRE(U(SetThreadPriority)); REQUIRE(U(GetThreadPriority));
    DWORD bits = tf_run(th_bgmode, NULL);
    CHECKF(bits == 0, "bits %lx", bits);
}
static DWORD WINAPI th_bgmode_exit(LPVOID p) {
    BOOL (WINAPI *sp)(HANDLE, int) = (BOOL (WINAPI *)(HANDLE, int))t_proc("SetThreadPriority");
    return sp(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN) ? 1 : 0;   /* exits while in background mode */
}
TEST(thrprio_background_thread_exit_clean) {
    REQUIRE(U(SetThreadPriority));
    EQ(tf_run(th_bgmode_exit, NULL), 1);
}
TEST(thrprio_invalid_handles) {
    REQUIRE(U(SetThreadPriority)); REQUIRE(U(GetThreadPriority));
    HANDLE ev = tf_ev(TRUE, FALSE);
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, ev };
    for (int i = 0; i < 3; i++) {
        SET_ERR(0);
        CHECK(!U(SetThreadPriority)(bad[i], 0));
        EXPECT_ERR(ERROR_INVALID_HANDLE);
        SET_ERR(0);
        EQ(U(GetThreadPriority)(bad[i]), THREAD_PRIORITY_ERROR_RETURN);
        EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
    CloseHandle(ev);
}
TEST(thrprio_set_rights) {
    REQUIRE(U(SetThreadPriority));
    worker w; w_start(&w);
    HANDLE o = w_open(&w, SYNCHRONIZE);
    SET_ERR(0);
    CHECK(!U(SetThreadPriority)(o, 1));
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(o);
    o = w_open(&w, THREAD_SET_LIMITED_INFORMATION | THREAD_QUERY_LIMITED_INFORMATION);
    SET_ERR(0);
    BOOL r = U(SetThreadPriority)(o, 1);
    t_note("SetThreadPriority with SET_LIMITED: ret %d err %lu", r, GetLastError());
    CloseHandle(o);
    o = w_open(&w, THREAD_SET_INFORMATION | THREAD_QUERY_INFORMATION);
    CHECK(U(SetThreadPriority)(o, THREAD_PRIORITY_BELOW_NORMAL));
    CloseHandle(o);
    w_stop(&w);
}
TEST(thrprio_pseudo_handle_in_worker) {
    REQUIRE(U(SetThreadPriority));
    worker w; w_start(&w);
    /* the pseudo handle in a different thread refers to that thread, not to the worker */
    int before = GetThreadPriority(GetCurrentThread());
    CHECK(U(SetThreadPriority)(w.h, THREAD_PRIORITY_LOWEST));
    EQ(GetThreadPriority(GetCurrentThread()), before);
    w_stop(&w);
}
TEST(thrprio_boost) {
    REQUIRE(U(GetThreadPriorityBoost)); REQUIRE(U(SetThreadPriorityBoost));
    worker w; w_start(&w);
    BOOL d = 0x55;
    CHECK(U(GetThreadPriorityBoost)(w.h, &d));
    EQ(d, FALSE);
    CHECK(U(SetThreadPriorityBoost)(w.h, TRUE));
    d = 0;
    CHECK(U(GetThreadPriorityBoost)(w.h, &d));
    EQ(d, TRUE);
    CHECK(U(SetThreadPriorityBoost)(w.h, 0x1234));   /* any nonzero disables */
    d = 0;
    CHECK(U(GetThreadPriorityBoost)(w.h, &d));
    t_note("boost disable after SetThreadPriorityBoost(0x1234): %d", d);
    CHECK(U(SetThreadPriorityBoost)(w.h, FALSE));
    d = 1;
    CHECK(U(GetThreadPriorityBoost)(w.h, &d));
    EQ(d, FALSE);
    w_stop(&w);
}
TEST(thrprio_boost_invalid) {
    REQUIRE(U(GetThreadPriorityBoost)); REQUIRE(U(SetThreadPriorityBoost));
    BOOL d;
    SET_ERR(0);
    CHECK(!U(GetThreadPriorityBoost)(NULL, &d));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(!U(SetThreadPriorityBoost)(NULL, TRUE));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE ev = tf_ev(TRUE, FALSE);
    SET_ERR(0);
    CHECK(!U(SetThreadPriorityBoost)(ev, TRUE));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
}

/* ---------- affinity / ideal processor ---------- */
static DWORD_PTR lowbit(DWORD_PTR m) { return m & (~m + 1); }
TEST(thraff_set_returns_previous) {
    REQUIRE(U(SetThreadAffinityMask));
    DWORD_PTR pm, sm;
    GetProcessAffinityMask(GetCurrentProcess(), &pm, &sm);
    worker w; w_start(&w);
    DWORD_PTR one = lowbit(pm);
    EQ(U(SetThreadAffinityMask)(w.h, one), pm);
    EQ(U(SetThreadAffinityMask)(w.h, pm), one);
    EQ(U(SetThreadAffinityMask)(w.h, pm), pm);
    w_stop(&w);
}
TEST(thraff_invalid_masks) {
    REQUIRE(U(SetThreadAffinityMask));
    DWORD_PTR pm, sm;
    GetProcessAffinityMask(GetCurrentProcess(), &pm, &sm);
    worker w; w_start(&w);
    SET_ERR(0);
    EQ(U(SetThreadAffinityMask)(w.h, 0), 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    DWORD_PTR out = ~sm;
    if (out) {
        SET_ERR(0);
        EQ(U(SetThreadAffinityMask)(w.h, lowbit(out)), 0);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
    SET_ERR(0);
    EQ(U(SetThreadAffinityMask)(w.h, (DWORD_PTR)1 << 63), (pm & ((DWORD_PTR)1 << 63)) ? pm : 0);
    t_note("mask bit63 err %lu", GetLastError());
    w_stop(&w);
}
TEST(thraff_invalid_handles) {
    REQUIRE(U(SetThreadAffinityMask));
    SET_ERR(0);
    EQ(U(SetThreadAffinityMask)(NULL, 1), 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE ev = tf_ev(TRUE, FALSE);
    SET_ERR(0);
    EQ(U(SetThreadAffinityMask)(ev, 1), 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
}
TEST(thraff_rights) {
    REQUIRE(U(SetThreadAffinityMask));
    DWORD_PTR pm, sm;
    GetProcessAffinityMask(GetCurrentProcess(), &pm, &sm);
    worker w; w_start(&w);
    HANDLE o = w_open(&w, THREAD_QUERY_LIMITED_INFORMATION);
    SET_ERR(0);
    EQ(U(SetThreadAffinityMask)(o, pm), 0);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(o);
    w_stop(&w);
}
TEST(thraff_group_affinity_get) {
    REQUIRE(U(GetThreadGroupAffinity));
    DWORD_PTR pm, sm;
    GetProcessAffinityMask(GetCurrentProcess(), &pm, &sm);
    worker w; w_start(&w);
    GROUP_AFFINITY ga; memset(&ga, 0xCC, sizeof ga);
    CHECK(U(GetThreadGroupAffinity)(w.h, &ga));
    CHECK(ga.Mask != 0);
    EQ(ga.Mask & ~sm, 0);
    EQ(ga.Reserved[0], 0);
    t_note("thread group %u mask %llx (process %llx)", ga.Group, (unsigned long long)ga.Mask, (unsigned long long)pm);
    w_stop(&w);
}
TEST(thraff_group_affinity_get_after_mask) {
    REQUIRE(U(GetThreadGroupAffinity)); REQUIRE(U(SetThreadAffinityMask));
    DWORD_PTR pm, sm;
    GetProcessAffinityMask(GetCurrentProcess(), &pm, &sm);
    worker w; w_start(&w);
    DWORD_PTR one = lowbit(pm);
    CHECK(U(SetThreadAffinityMask)(w.h, one) != 0);
    GROUP_AFFINITY ga; memset(&ga, 0, sizeof ga);
    CHECK(U(GetThreadGroupAffinity)(w.h, &ga));
    EQ(ga.Mask, one);
    U(SetThreadAffinityMask)(w.h, pm);
    w_stop(&w);
}
TEST(thraff_group_affinity_set) {
    REQUIRE(U(GetThreadGroupAffinity)); REQUIRE(U(SetThreadGroupAffinity));
    worker w; w_start(&w);
    GROUP_AFFINITY cur; memset(&cur, 0, sizeof cur);
    CHECK(U(GetThreadGroupAffinity)(w.h, &cur));
    GROUP_AFFINITY ga = cur, prev; memset(&prev, 0xCC, sizeof prev);
    ga.Mask = lowbit(cur.Mask);
    CHECK(U(SetThreadGroupAffinity)(w.h, &ga, &prev));
    EQ(prev.Mask, cur.Mask);
    EQ(prev.Group, cur.Group);
    CHECK(U(SetThreadGroupAffinity)(w.h, &cur, NULL));   /* previous optional */
    GROUP_AFFINITY now; CHECK(U(GetThreadGroupAffinity)(w.h, &now));
    EQ(now.Mask, cur.Mask);
    w_stop(&w);
}
TEST(thraff_group_affinity_set_invalid) {
    REQUIRE(U(GetThreadGroupAffinity)); REQUIRE(U(SetThreadGroupAffinity));
    worker w; w_start(&w);
    GROUP_AFFINITY cur; memset(&cur, 0, sizeof cur);
    CHECK(U(GetThreadGroupAffinity)(w.h, &cur));
    GROUP_AFFINITY ga = cur;
    ga.Mask = 0;
    SET_ERR(0);
    CHECK(!U(SetThreadGroupAffinity)(w.h, &ga, NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    ga = cur; ga.Group = 0x7FFF;
    SET_ERR(0);
    CHECK(!U(SetThreadGroupAffinity)(w.h, &ga, NULL));
    t_note("SetThreadGroupAffinity(group 0x7FFF) err %lu", GetLastError());
    SET_ERR(0);
    CHECK(!U(SetThreadGroupAffinity)(NULL, &cur, NULL));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(!U(GetThreadGroupAffinity)(NULL, &ga));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    GROUP_AFFINITY now; CHECK(U(GetThreadGroupAffinity)(w.h, &now));
    EQ(now.Mask, cur.Mask);
    w_stop(&w);
}
TEST(thrideal_legacy_query) {
    REQUIRE(U(SetThreadIdealProcessor));
    worker w; w_start(&w);
    DWORD prev = U(SetThreadIdealProcessor)(w.h, MAXIMUM_PROCESSORS);   /* query only */
    CHECK(prev != (DWORD)-1);
    DWORD prev2 = U(SetThreadIdealProcessor)(w.h, MAXIMUM_PROCESSORS);
    EQ(prev2, prev);
    w_stop(&w);
}
TEST(thrideal_legacy_set) {
    REQUIRE(U(SetThreadIdealProcessor));
    worker w; w_start(&w);
    DWORD orig = U(SetThreadIdealProcessor)(w.h, MAXIMUM_PROCESSORS);
    DWORD_PTR pm, sm;
    GetProcessAffinityMask(GetCurrentProcess(), &pm, &sm);
    DWORD cpu = 0; while (!((pm >> cpu) & 1)) cpu++;
    DWORD p = U(SetThreadIdealProcessor)(w.h, cpu);
    EQ(p, orig);
    EQ(U(SetThreadIdealProcessor)(w.h, MAXIMUM_PROCESSORS), cpu);
    U(SetThreadIdealProcessor)(w.h, orig);
    w_stop(&w);
}
TEST(thrideal_legacy_invalid) {
    REQUIRE(U(SetThreadIdealProcessor));
    worker w; w_start(&w);
    SET_ERR(0);
    EQ(U(SetThreadIdealProcessor)(NULL, 0), (DWORD)-1);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE ev = tf_ev(TRUE, FALSE);
    SET_ERR(0);
    EQ(U(SetThreadIdealProcessor)(ev, 0), (DWORD)-1);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
    SET_ERR(0);
    DWORD r = U(SetThreadIdealProcessor)(w.h, 63);
    t_note("SetThreadIdealProcessor(63): ret %lx err %lu", r, GetLastError());
    SET_ERR(0);
    r = U(SetThreadIdealProcessor)(w.h, 1000);
    t_note("SetThreadIdealProcessor(1000): ret %lx err %lu", r, GetLastError());
    w_stop(&w);
}
TEST(thrideal_ex_get_set) {
    REQUIRE(U(GetThreadIdealProcessorEx)); REQUIRE(U(SetThreadIdealProcessorEx));
    worker w; w_start(&w);
    PROCESSOR_NUMBER cur; memset(&cur, 0xCC, sizeof cur);
    CHECK(U(GetThreadIdealProcessorEx)(w.h, &cur));
    CHECK(cur.Number < 64);
    PROCESSOR_NUMBER n = cur, prev; memset(&prev, 0xCC, sizeof prev);
    CHECK(U(SetThreadIdealProcessorEx)(w.h, &n, &prev));
    EQ(prev.Group, cur.Group);
    EQ(prev.Number, cur.Number);
    CHECK(U(SetThreadIdealProcessorEx)(w.h, &n, NULL));
    PROCESSOR_NUMBER again; CHECK(U(GetThreadIdealProcessorEx)(w.h, &again));
    EQ(again.Group, cur.Group); EQ(again.Number, cur.Number);
    w_stop(&w);
}
TEST(thrideal_ex_invalid) {
    REQUIRE(U(GetThreadIdealProcessorEx)); REQUIRE(U(SetThreadIdealProcessorEx));
    worker w; w_start(&w);
    PROCESSOR_NUMBER cur; CHECK(U(GetThreadIdealProcessorEx)(w.h, &cur));
    PROCESSOR_NUMBER bad = cur; bad.Group = 0x7FFF;
    SET_ERR(0);
    CHECK(!U(SetThreadIdealProcessorEx)(w.h, &bad, NULL));
    t_note("ideal group 0x7FFF err %lu", GetLastError());
    bad = cur; bad.Number = 0xFE;
    SET_ERR(0);
    CHECK(!U(SetThreadIdealProcessorEx)(w.h, &bad, NULL));
    t_note("ideal number 0xFE err %lu", GetLastError());
    SET_ERR(0);
    CHECK(!U(SetThreadIdealProcessorEx)(NULL, &cur, NULL));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(!U(GetThreadIdealProcessorEx)(NULL, &cur));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    PROCESSOR_NUMBER after; CHECK(U(GetThreadIdealProcessorEx)(w.h, &after));
    EQ(after.Number, cur.Number);
    w_stop(&w);
}

/* ---------- description ---------- */
static HRESULT desc_set(HANDLE h, const wchar_t *s) { return UN(SetThreadDescription)(h, s); }
static int desc_eq(HANDLE h, const wchar_t *want) {
    PWSTR p = (PWSTR)(ULONG_PTR)0x1; HRESULT hr = UN(GetThreadDescription)(h, &p);
    int ok = SUCCEEDED(hr) && p && !wcscmp(p, want);
    if (SUCCEEDED(hr) && p) LocalFree(p);
    return ok;
}
TEST(thrdesc_roundtrip) {
    REQUIRE(UN(SetThreadDescription)); REQUIRE(UN(GetThreadDescription));
    worker w; w_start(&w);
    CHECK(SUCCEEDED(desc_set(w.h, L"k32t worker")));
    CHECK(desc_eq(w.h, L"k32t worker"));
    CHECK(SUCCEEDED(desc_set(w.h, L"changed")));
    CHECK(desc_eq(w.h, L"changed"));
    w_stop(&w);
}
TEST(thrdesc_initially_empty) {
    REQUIRE(UN(GetThreadDescription));
    worker w; w_start(&w);
    PWSTR p = NULL;
    HRESULT hr = UN(GetThreadDescription)(w.h, &p);
    CHECK(SUCCEEDED(hr));
    CHECK(p != NULL);
    if (p) { EQ(p[0], 0); LocalFree(p); }
    w_stop(&w);
}
TEST(thrdesc_empty_string) {
    REQUIRE(UN(SetThreadDescription)); REQUIRE(UN(GetThreadDescription));
    worker w; w_start(&w);
    CHECK(SUCCEEDED(desc_set(w.h, L"abc")));
    CHECK(SUCCEEDED(desc_set(w.h, L"")));
    CHECK(desc_eq(w.h, L""));
    w_stop(&w);
}
TEST(thrdesc_current_thread_pseudo) {
    REQUIRE(UN(SetThreadDescription)); REQUIRE(UN(GetThreadDescription));
    worker w; w_start(&w);
    CHECK(SUCCEEDED(desc_set(w.h, L"other")));
    HANDLE me = GetCurrentThread();
    CHECK(SUCCEEDED(desc_set(me, L"main-thread")));
    CHECK(desc_eq(me, L"main-thread"));
    CHECK(desc_eq(w.h, L"other"));
    CHECK(SUCCEEDED(desc_set(me, L"")));
    w_stop(&w);
}
TEST(thrdesc_unicode_and_lengths) {
    REQUIRE(UN(SetThreadDescription)); REQUIRE(UN(GetThreadDescription));
    worker w; w_start(&w);
    CHECK(SUCCEEDED(desc_set(w.h, L"é中文\U0001F600 x")));
    CHECK(desc_eq(w.h, L"é中文\U0001F600 x"));
    static const int lens[] = { 1, 15, 16, 31, 64, 255, 256, 1000, 4096 };
    for (size_t i = 0; i < sizeof lens / sizeof lens[0]; i++) {
        wchar_t *s = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, (lens[i] + 1) * 2);
        for (int j = 0; j < lens[i]; j++) s[j] = L'a' + (j % 26);
        s[lens[i]] = 0;
        CHECKF(SUCCEEDED(desc_set(w.h, s)), "len %d", lens[i]);
        CHECKF(desc_eq(w.h, s), "len %d", lens[i]);
        HeapFree(GetProcessHeap(), 0, s);
    }
    w_stop(&w);
}
TEST(thrdesc_too_long) {
    REQUIRE(UN(SetThreadDescription)); REQUIRE(UN(GetThreadDescription));
    worker w; w_start(&w);
    int n = 40000;
    wchar_t *s = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, (n + 1) * 2);
    for (int j = 0; j < n; j++) s[j] = L'z';
    s[n] = 0;
    CHECK(SUCCEEDED(desc_set(w.h, L"keep")));
    HRESULT hr = desc_set(w.h, s);
    CHECK(FAILED(hr));
    t_note("SetThreadDescription(40000 chars) hr=%lx", (unsigned long)hr);
    CHECK(desc_eq(w.h, L"keep"));
    HeapFree(GetProcessHeap(), 0, s);
    w_stop(&w);
}
TEST(thrdesc_invalid_handle) {
    REQUIRE(UN(SetThreadDescription)); REQUIRE(UN(GetThreadDescription));
    HRESULT hr = desc_set(NULL, L"x");
    CHECK(FAILED(hr));
    t_note("SetThreadDescription(NULL) hr=%lx", (unsigned long)hr);
    PWSTR p = (PWSTR)(ULONG_PTR)1;
    hr = UN(GetThreadDescription)(NULL, &p);
    CHECK(FAILED(hr));
    CHECK(p == NULL);   /* output cleared on failure */
    HANDLE ev = tf_ev(TRUE, FALSE);
    CHECK(FAILED(desc_set(ev, L"x")));
    p = (PWSTR)(ULONG_PTR)1;
    CHECK(FAILED(UN(GetThreadDescription)(ev, &p)));
    CHECK(p == NULL);
    CloseHandle(ev);
}
TEST(thrdesc_rights) {
    REQUIRE(UN(SetThreadDescription)); REQUIRE(UN(GetThreadDescription));
    worker w; w_start(&w);
    CHECK(SUCCEEDED(desc_set(w.h, L"rights")));
    HANDLE o = w_open(&w, THREAD_QUERY_LIMITED_INFORMATION);
    CHECK(desc_eq(o, L"rights"));
    CHECK(FAILED(desc_set(o, L"nope")));
    CloseHandle(o);
    o = w_open(&w, THREAD_SET_LIMITED_INFORMATION);
    CHECK(SUCCEEDED(desc_set(o, L"set-limited")));
    PWSTR p = (PWSTR)(ULONG_PTR)1;
    CHECK(FAILED(UN(GetThreadDescription)(o, &p)));
    CloseHandle(o);
    o = w_open(&w, SYNCHRONIZE);
    p = (PWSTR)(ULONG_PTR)1;
    CHECK(FAILED(UN(GetThreadDescription)(o, &p)));
    CloseHandle(o);
    CHECK(desc_eq(w.h, L"set-limited"));
    w_stop(&w);
}
TEST(thrdesc_per_thread_isolated) {
    REQUIRE(UN(SetThreadDescription)); REQUIRE(UN(GetThreadDescription));
    worker a, b; w_start(&a); w_start(&b);
    CHECK(SUCCEEDED(desc_set(a.h, L"A")));
    CHECK(SUCCEEDED(desc_set(b.h, L"B")));
    CHECK(desc_eq(a.h, L"A")); CHECK(desc_eq(b.h, L"B"));
    w_stop(&a); w_stop(&b);
}
TEST(thrdesc_after_exit) {
    REQUIRE(UN(SetThreadDescription)); REQUIRE(UN(GetThreadDescription));
    worker w; w_start(&w);
    CHECK(SUCCEEDED(desc_set(w.h, L"before-exit")));
    HANDLE h = w.h; HANDLE ev = w.ev;
    HANDLE dup; DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(), &dup, 0, FALSE, DUPLICATE_SAME_ACCESS);
    SetEvent(ev); WaitForSingleObject(h, 5000);
    PWSTR p = NULL;
    HRESULT hr = UN(GetThreadDescription)(dup, &p);
    t_note("GetThreadDescription(exited thread) hr=%lx str=%ls", (unsigned long)hr, p ? p : L"(null)");
    if (p) LocalFree(p);
    CloseHandle(dup); CloseHandle(h); CloseHandle(ev);
}
TEST(thrdesc_set_null_string) {
    REQUIRE(UN(SetThreadDescription));
    worker w; w_start(&w);
    HRESULT hr = UN(SetThreadDescription)(w.h, NULL);
    t_note("SetThreadDescription(h, NULL) hr=%lx", (unsigned long)hr);
    w_stop(&w);
}

/* ---------- Get/SetThreadInformation ---------- */
TEST(thrinfo_memory_priority) {
    REQUIRE(U(GetThreadInformation)); REQUIRE(U(SetThreadInformation));
    worker w; w_start(&w);
    tf_mem_prio mp = { 0xCC };
    CHECK(U(GetThreadInformation)(w.h, ThreadMemoryPriority, &mp, sizeof mp));
    EQ(mp.MemoryPriority, 5);
    for (ULONG v = 1; v <= 5; v++) {
        tf_mem_prio s = { v };
        CHECKF(U(SetThreadInformation)(w.h, ThreadMemoryPriority, &s, sizeof s), "prio %lu", v);
        mp.MemoryPriority = 0xCC;
        CHECK(U(GetThreadInformation)(w.h, ThreadMemoryPriority, &mp, sizeof mp));
        EQ(mp.MemoryPriority, v);
    }
    tf_mem_prio s = { 5 };
    CHECK(U(SetThreadInformation)(w.h, ThreadMemoryPriority, &s, sizeof s));
    w_stop(&w);
}
TEST(thrinfo_memory_priority_invalid) {
    REQUIRE(U(GetThreadInformation)); REQUIRE(U(SetThreadInformation));
    worker w; w_start(&w);
    static const ULONG bad[] = { 6, 7, 100, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        tf_mem_prio s = { bad[i] };
        SET_ERR(0);
        CHECKF(!U(SetThreadInformation)(w.h, ThreadMemoryPriority, &s, sizeof s), "prio %lu", bad[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
    tf_mem_prio s = { 3 };
    SET_ERR(0);
    CHECK(!U(SetThreadInformation)(w.h, ThreadMemoryPriority, &s, 2));   /* wrong size */
    t_note("SetThreadInformation(mem prio, size 2) err %lu", GetLastError());
    tf_mem_prio g;
    SET_ERR(0);
    CHECK(!U(GetThreadInformation)(w.h, ThreadMemoryPriority, &g, 1));
    t_note("GetThreadInformation(mem prio, size 1) err %lu", GetLastError());
    w_stop(&w);
}
TEST(thrinfo_invalid_class) {
    REQUIRE(U(GetThreadInformation)); REQUIRE(U(SetThreadInformation));
    worker w; w_start(&w);
    DWORD buf[4] = { 0 };
    static const int cls[] = { 3, 4, 5, 10, 100, -1 };
    for (size_t i = 0; i < sizeof cls / sizeof cls[0]; i++) {
        SET_ERR(0);
        CHECKF(!U(GetThreadInformation)(w.h, (THREAD_INFORMATION_CLASS)cls[i], buf, sizeof buf), "get class %d", cls[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
    static const int scls[] = { 4, 5, 10, 100, -1 };
    for (size_t i = 0; i < sizeof scls / sizeof scls[0]; i++) {
        SET_ERR(0);
        CHECKF(!U(SetThreadInformation)(w.h, (THREAD_INFORMATION_CLASS)scls[i], buf, sizeof buf), "set class %d", scls[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
    w_stop(&w);
}
TEST(thrinfo_invalid_handle) {
    REQUIRE(U(GetThreadInformation)); REQUIRE(U(SetThreadInformation));
    tf_mem_prio m = { 3 };
    SET_ERR(0);
    CHECK(!U(GetThreadInformation)(NULL, ThreadMemoryPriority, &m, sizeof m));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(!U(SetThreadInformation)(NULL, ThreadMemoryPriority, &m, sizeof m));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE ev = tf_ev(TRUE, FALSE);
    SET_ERR(0);
    CHECK(!U(GetThreadInformation)(ev, ThreadMemoryPriority, &m, sizeof m));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
}
TEST(thrinfo_power_throttling) {
    REQUIRE(U(SetThreadInformation));
    worker w; w_start(&w);
    tf_power_throttle p = { 1, 1, 1 };
    SET_ERR(0);
    BOOL r = U(SetThreadInformation)(w.h, (THREAD_INFORMATION_CLASS)3, &p, sizeof p);
    t_note("power throttling on: ret %d err %lu", r, GetLastError());
    p.StateMask = 0;
    r = U(SetThreadInformation)(w.h, (THREAD_INFORMATION_CLASS)3, &p, sizeof p);
    t_note("power throttling off: ret %d err %lu", r, GetLastError());
    p.ControlMask = 0; p.StateMask = 0;
    r = U(SetThreadInformation)(w.h, (THREAD_INFORMATION_CLASS)3, &p, sizeof p);
    t_note("power throttling default: ret %d err %lu", r, GetLastError());
    w_stop(&w);
}
TEST(thrinfo_power_throttling_invalid) {
    REQUIRE(U(SetThreadInformation));
    worker w; w_start(&w);
    tf_power_throttle p = { 1, 1, 1 };
    SET_ERR(0);
    CHECK(!U(SetThreadInformation)(w.h, (THREAD_INFORMATION_CLASS)3, &p, 8));      /* too small */
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    p.ControlMask = 2; p.StateMask = 2;
    SET_ERR(0);
    CHECK(!U(SetThreadInformation)(w.h, (THREAD_INFORMATION_CLASS)3, &p, sizeof p)); /* unknown control bit */
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    p.ControlMask = 0; p.StateMask = 1;
    SET_ERR(0);
    CHECK(!U(SetThreadInformation)(w.h, (THREAD_INFORMATION_CLASS)3, &p, sizeof p)); /* state bit outside control */
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    p.Version = 2; p.ControlMask = 1; p.StateMask = 1;
    SET_ERR(0);
    CHECK(!U(SetThreadInformation)(w.h, (THREAD_INFORMATION_CLASS)3, &p, sizeof p)); /* version */
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    p.Version = 0xFFFFFFFF;
    SET_ERR(0);
    CHECK(!U(SetThreadInformation)(w.h, (THREAD_INFORMATION_CLASS)3, &p, sizeof p));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    w_stop(&w);
}
TEST(thrinfo_get_power_throttling_unsupported) {
    REQUIRE(U(GetThreadInformation));
    worker w; w_start(&w);
    tf_power_throttle p = { 1, 1, 1 };
    SET_ERR(0);
    CHECK(!U(GetThreadInformation)(w.h, (THREAD_INFORMATION_CLASS)3, &p, sizeof p));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    w_stop(&w);
}
TEST(thrinfo_dynamic_code_policy_get) {
    REQUIRE(U(GetThreadInformation));
    worker w; w_start(&w);
    DWORD v = 0xCC;
    SET_ERR(0);
    BOOL r = U(GetThreadInformation)(w.h, (THREAD_INFORMATION_CLASS)2, &v, sizeof v);
    t_note("ThreadDynamicCodePolicy get: ret %d err %lu v=%lx", r, GetLastError(), v);
    w_stop(&w);
}
TEST(thrinfo_absolute_cpu_priority_get) {
    REQUIRE(U(GetThreadInformation));
    worker w; w_start(&w);
    DWORD v = 0xCC;
    SET_ERR(0);
    BOOL r = U(GetThreadInformation)(w.h, (THREAD_INFORMATION_CLASS)1, &v, sizeof v);
    t_note("ThreadAbsoluteCpuPriority get: ret %d err %lu v=%lx", r, GetLastError(), v);
    w_stop(&w);
}

/* ---------- error mode ---------- */
static DWORD WINAPI th_errmode(LPVOID p) {
    BOOL (WINAPI *set)(DWORD, LPDWORD) = (BOOL (WINAPI *)(DWORD, LPDWORD))t_proc("SetThreadErrorMode");
    DWORD (WINAPI *get)(void) = (DWORD (WINAPI *)(void))t_proc("GetThreadErrorMode");
    DWORD bits = 0, old = 0xCC;
    if (get() != 0) bits |= 1;
    static const DWORD vals[] = { 1, 2, 3, 0x8000, 0x8001, 0x8003, 0 };
    for (size_t i = 0; i < sizeof vals / sizeof vals[0]; i++) {
        old = 0xCC;
        if (!set(vals[i], &old)) bits |= 2;
        if (get() != vals[i]) bits |= 4;
        if (set(vals[i], &old) == 0 || old != vals[i]) bits |= 8;
    }
    static const DWORD bad[] = { 4, 8, 0x10, 0x100, 0x4000, 0x10000, 0x80000000, 0xFFFFFFFF, 0x8004 };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0);
        old = 0xCC;
        if (set(bad[i], &old)) bits |= 0x10;
        if (GetLastError() != ERROR_INVALID_PARAMETER) bits |= 0x20;
        if (old != 0xCC) bits |= 0x40;
    }
    if (!set(1, NULL)) bits |= 0x80;
    if (get() != 1) bits |= 0x100;
    set(0, NULL);
    return bits;
}
TEST(threrr_mode_roundtrip_and_invalid) {
    REQUIRE(U(SetThreadErrorMode)); REQUIRE(U(GetThreadErrorMode));
    DWORD bits = tf_run(th_errmode, NULL);
    CHECKF(bits == 0, "bits %lx", bits);
}
TEST(threrr_mode_thread_local) {
    REQUIRE(U(SetThreadErrorMode)); REQUIRE(U(GetThreadErrorMode));
    DWORD main_before = U(GetThreadErrorMode)();
    DWORD old;
    CHECK(U(SetThreadErrorMode)(SEM_FAILCRITICALERRORS, &old));
    EQ(old, main_before);
    EQ(U(GetThreadErrorMode)(), SEM_FAILCRITICALERRORS);
    /* a new thread starts with 0 irrespective of this thread */
    EQ(tf_run(th_errmode, NULL), 0);
    EQ(U(GetThreadErrorMode)(), SEM_FAILCRITICALERRORS);
    CHECK(U(SetThreadErrorMode)(main_before, NULL));
}
TEST(threrr_mode_independent_of_process_mode) {
    REQUIRE(U(SetThreadErrorMode)); REQUIRE(U(GetThreadErrorMode));
    UINT pm = GetErrorMode();
    DWORD t0 = U(GetThreadErrorMode)();
    CHECK(U(SetThreadErrorMode)(SEM_NOOPENFILEERRORBOX, NULL));
    EQ(GetErrorMode(), pm);
    CHECK(U(SetThreadErrorMode)(t0, NULL));
}

/* ---------- stack guarantee ---------- */
static DWORD WINAPI th_guarantee(LPVOID p) {
    BOOL (WINAPI *f)(PULONG) = (BOOL (WINAPI *)(PULONG))t_proc("SetThreadStackGuarantee");
    DWORD bits = 0;
    ULONG sz = 0;
    if (!f(&sz)) bits |= 1;
    ULONG initial = sz;
    sz = 0x10000;
    if (!f(&sz)) bits |= 2;
    if (sz != initial) bits |= 4;
    sz = 0;
    if (!f(&sz)) bits |= 8;
    if (sz < 0x10000) bits |= 0x10;
    ULONG cur = sz;
    sz = 0x8000;   /* smaller than current: no shrink */
    if (!f(&sz)) bits |= 0x20;
    sz = 0;
    f(&sz);
    if (sz < cur) bits |= 0x40;
    return bits;
}
TEST(thrstack_guarantee) {
    REQUIRE(U(SetThreadStackGuarantee));
    DWORD bits = tf_run(th_guarantee, NULL);
    CHECKF(bits == 0, "bits %lx", bits);
}
TEST(thrstack_guarantee_query_default) {
    REQUIRE(U(SetThreadStackGuarantee));
    ULONG sz = 0;
    CHECK(U(SetThreadStackGuarantee)(&sz));
    t_note("stack guarantee on main thread: %lu", sz);
}
TEST(thrstack_current_limits) {
    REQUIRE(U(GetCurrentThreadStackLimits));
    ULONG_PTR lo = 0, hi = 0;
    volatile int x;
    U(GetCurrentThreadStackLimits)(&lo, &hi);
    CHECK(lo < hi);
    CHECK((ULONG_PTR)&x >= lo && (ULONG_PTR)&x < hi);
    EQ(hi, *(ULONG_PTR *)((char *)NtCurrentTeb() + 8));
    EQ(lo, *(ULONG_PTR *)((char *)NtCurrentTeb() + 0x1478));
    CHECK(hi - lo >= 0x10000);
}

/* ---------- CPU sets ---------- */
static int first_cpuset_id(ULONG *id) {
    if (!UN(GetSystemCpuSetInformation)) return 0;
    ULONG len = 0;
    SET_ERR(0);
    UN(GetSystemCpuSetInformation)(NULL, 0, &len, GetCurrentProcess(), 0);
    if (len < 24) return 0;
    BYTE *buf = (BYTE *)HeapAlloc(GetProcessHeap(), 0, len);
    ULONG got = 0;
    int ok = 0;
    if (UN(GetSystemCpuSetInformation)(buf, len, &got, GetCurrentProcess(), 0) && got >= 24) {
        ULONG sz = *(ULONG *)buf, type = *(ULONG *)(buf + 4);
        if (sz >= 24 && type == 0) { *id = *(ULONG *)(buf + 8); ok = 1; }
    }
    HeapFree(GetProcessHeap(), 0, buf);
    return ok;
}
TEST(thrcpuset_default_empty) {
    REQUIRE(UN(GetThreadSelectedCpuSets));
    worker w; w_start(&w);
    ULONG req = 0xCC, ids[4];
    SET_ERR(0);
    BOOL r = UN(GetThreadSelectedCpuSets)(w.h, ids, 4, &req);
    CHECK(r);
    EQ(req, 0);
    r = UN(GetThreadSelectedCpuSets)(w.h, NULL, 0, &req);
    CHECK(r);
    EQ(req, 0);
    w_stop(&w);
}
TEST(thrcpuset_set_get_clear) {
    REQUIRE(UN(GetThreadSelectedCpuSets)); REQUIRE(UN(SetThreadSelectedCpuSets));
    ULONG id;
    if (!first_cpuset_id(&id)) { t_skip("no cpu set info"); return; }
    worker w; w_start(&w);
    SET_ERR(0);
    BOOL r = UN(SetThreadSelectedCpuSets)(w.h, &id, 1);
    if (!r) { t_note("SetThreadSelectedCpuSets failed err %lu", GetLastError()); w_stop(&w); return; }
    ULONG req = 0, out[4] = { 0 };
    CHECK(UN(GetThreadSelectedCpuSets)(w.h, out, 4, &req));
    EQ(req, 1);
    EQ(out[0], id);
    /* too-small buffer */
    SET_ERR(0);
    r = UN(GetThreadSelectedCpuSets)(w.h, NULL, 0, &req);
    CHECK(!r);
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    EQ(req, 1);
    /* clear */
    CHECK(UN(SetThreadSelectedCpuSets)(w.h, NULL, 0));
    CHECK(UN(GetThreadSelectedCpuSets)(w.h, out, 4, &req));
    EQ(req, 0);
    w_stop(&w);
}
TEST(thrcpuset_invalid) {
    REQUIRE(UN(GetThreadSelectedCpuSets)); REQUIRE(UN(SetThreadSelectedCpuSets));
    ULONG id = 0, req = 0;
    SET_ERR(0);
    CHECK(!UN(GetThreadSelectedCpuSets)(NULL, &id, 1, &req));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(!UN(SetThreadSelectedCpuSets)(NULL, &id, 1));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    worker w; w_start(&w);
    ULONG bogus = 0x7FFFFFF1;
    SET_ERR(0);
    CHECK(!UN(SetThreadSelectedCpuSets)(w.h, &bogus, 1));
    t_note("SetThreadSelectedCpuSets(bogus id) err %lu", GetLastError());
    w_stop(&w);
}

/* ---------- execution state ---------- */
static DWORD WINAPI th_execstate(LPVOID p) {
    EXECUTION_STATE (WINAPI *f)(EXECUTION_STATE) = (EXECUTION_STATE (WINAPI *)(EXECUTION_STATE))t_proc("SetThreadExecutionState");
    EXECUTION_STATE a = f(ES_CONTINUOUS);
    EXECUTION_STATE b = f(ES_CONTINUOUS);
    return (a != 0 ? 1 : 0) | (b != 0 ? 2 : 0);
}
TEST(threxec_continuous_only) {
    REQUIRE(U(SetThreadExecutionState));
    EQ(tf_run(th_execstate, NULL), 3);
}
