/* Generic rights on OpenProcess/OpenThread, DuplicateHandle with reduced or same access, implied
 * limited rights, and out-parameters left untouched on ERROR_ACCESS_DENIED. */
#include "xpt.h"

/* ---- probes: 1 = allowed, 0 = ERROR_ACCESS_DENIED, -1 = other failure ---- */
static int pr(int ok) { return ok ? 1 : GetLastError() == ERROR_ACCESS_DENIED ? 0 : -1; }
static int p_wait(HANDLE h) { SetLastError(0); return pr(WaitForSingleObject(h, 0) != WAIT_FAILED); }
static int p_pexit(HANDLE h) { DWORD c; return pr(U(GetExitCodeProcess)(h, &c)); }
static int p_ptimes(HANDLE h) { FILETIME a, b, c, d; return pr(U(GetProcessTimes)(h, &a, &b, &c, &d)); }
static int p_pclass(HANDLE h) { return pr(U(GetPriorityClass)(h) != 0); }
static int p_psetclass(HANDLE h) { return pr(U(SetPriorityClass)(h, NORMAL_PRIORITY_CLASS)); }
static int p_phcount(HANDLE h) { DWORD n; return pr(U(GetProcessHandleCount)(h, &n)); }
static int p_prpm(HANDLE h) { char b[2]; SIZE_T n; return pr(U(ReadProcessMemory)(h, GetModuleHandleA("kernel32.dll"), b, 2, &n)); }
static int p_palloc(HANDLE h, HANDLE full) {
    void *p = U(VirtualAllocEx)(h, NULL, 4096, MEM_RESERVE, PAGE_NOACCESS);
    int r = pr(p != NULL);
    if (p) VirtualFreeEx(full, p, 0, MEM_RELEASE);
    return r;
}
static int p_texit(HANDLE h) { DWORD c; return pr(U(GetExitCodeThread)(h, &c)); }
static int p_tprio(HANDLE h) { return pr(U(GetThreadPriority)(h) != THREAD_PRIORITY_ERROR_RETURN); }
static int p_tsetprio(HANDLE h) { return pr(U(SetThreadPriority)(h, THREAD_PRIORITY_NORMAL)); }
static int p_tioflag(HANDLE h) { BOOL b; return pr(U(GetThreadIOPendingFlag)(h, &b)); }
static int p_tsuspend(HANDLE h, HANDLE full) { DWORD r = U(SuspendThread)(h); int x = pr(r != (DWORD)-1); if (r != (DWORD)-1) ResumeThread(full); return x; }
static int p_tctx(HANDLE h, HANDLE full) {
    CONTEXT cx; memset(&cx, 0, sizeof cx); cx.ContextFlags = CONTEXT_CONTROL;
    SuspendThread(full); int x = pr(U(GetThreadContext)(h, &cx)); ResumeThread(full); return x;
}

/* Row of process probe results: wait, exitcode, class, setclass, rpm, valloc, handlecount. */
static void proc_row(const char *what, HANDLE h, HANDLE full, const int want[7]) {
    int got[7] = { p_wait(h), p_pexit(h), p_pclass(h), p_psetclass(h), p_prpm(h), p_palloc(h, full), p_phcount(h) };
    static const char *nm[7] = { "wait", "GetExitCodeProcess", "GetPriorityClass", "SetPriorityClass", "ReadProcessMemory", "VirtualAllocEx", "GetProcessHandleCount" };
    for (int i = 0; i < 7; i++) CHECKF(got[i] == want[i], "%s: %s got %d want %d", what, nm[i], got[i], want[i]);
}
/* Row of thread probe results: wait, exitcode, priority, setpriority, iopending, suspend, getcontext. */
static void thread_row(const char *what, HANDLE h, HANDLE full, const int want[7]) {
    int got[7] = { p_wait(h), p_texit(h), p_tprio(h), p_tsetprio(h), p_tioflag(h), p_tsuspend(h, full), p_tctx(h, full) };
    static const char *nm[7] = { "wait", "GetExitCodeThread", "GetThreadPriority", "SetThreadPriority", "GetThreadIOPendingFlag", "SuspendThread", "GetThreadContext" };
    for (int i = 0; i < 7; i++) CHECKF(got[i] == want[i], "%s: %s got %d want %d", what, nm[i], got[i], want[i]);
}

static void proc_generic(DWORD acc, ULONG want_mask, const int want[7], int term_allowed) {
    PROCESS_INFORMATION pi; HANDLE hp = xpt_spawn_sleeper(&pi);
    CHECK(hp != NULL); if (!hp) return;
    HANDLE h = U(OpenProcess)(acc, FALSE, pi.dwProcessId);
    CHECKF(h != NULL, "err=%lu", GetLastError());
    if (h) {
        EQ(xpt_access(h), want_mask);
        proc_row("generic", h, hp, want);
        SetLastError(0);
        BOOL t = U(TerminateProcess)(h, 0x88);
        EQ(pr(t), term_allowed);
        CloseHandle(h);
    }
    xpt_kill(&pi);
}
static void thread_generic(DWORD acc, ULONG want_mask, const int want[7], int term_allowed) {
    xpt_tw w; if (!xpt_tw_start(&w)) { CHECK(!"worker"); xpt_tw_stop(&w); return; }
    HANDLE h = U(OpenThread)(acc, FALSE, w.tid);
    CHECKF(h != NULL, "err=%lu", GetLastError());
    if (h) {
        EQ(xpt_access(h), want_mask);
        thread_row("generic", h, w.h, want);
        SetLastError(0);
        BOOL t = U(TerminateThread)(h, 0x88);
        EQ(pr(t), term_allowed);
        if (t) WaitForSingleObject(w.h, 5000);
        CloseHandle(h);
    }
    xpt_tw_stop(&w);
}

TEST(xpt_gen_process_generic_read) {
    static const int want[7] = { 0, 1, 1, 0, 1, 0, 1 };
    proc_generic(GENERIC_READ, STANDARD_RIGHTS_READ | PROCESS_VM_READ | PROCESS_QUERY_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION, want, 0);
}
TEST(xpt_gen_process_generic_write) {
    /* the process GENERIC_WRITE mapping includes QUERY_LIMITED_INFORMATION */
    static const int want[7] = { 0, 1, 1, 1, 0, 1, 1 };
    proc_generic(GENERIC_WRITE, STANDARD_RIGHTS_WRITE | PROCESS_CREATE_PROCESS | PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_WRITE |
                 PROCESS_DUP_HANDLE | PROCESS_SET_QUOTA | PROCESS_SET_INFORMATION | PROCESS_SUSPEND_RESUME | PROCESS_SET_LIMITED_INFORMATION |
                 PROCESS_QUERY_LIMITED_INFORMATION, want, 0);
}
TEST(xpt_gen_process_generic_execute) {
    static const int want[7] = { 1, 1, 1, 0, 0, 0, 1 };
    proc_generic(GENERIC_EXECUTE, STANDARD_RIGHTS_EXECUTE | SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE, want, 1);
}
TEST(xpt_gen_process_generic_all) {
    static const int want[7] = { 1, 1, 1, 1, 1, 1, 1 };
    proc_generic(GENERIC_ALL, PROCESS_ALL_ACCESS, want, 1);
}
TEST(xpt_gen_thread_generic_read) {
    static const int want[7] = { 0, 1, 1, 0, 1, 0, 1 };
    thread_generic(GENERIC_READ, STANDARD_RIGHTS_READ | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION | THREAD_QUERY_LIMITED_INFORMATION, want, 0);
}
TEST(xpt_gen_thread_generic_write) {
    static const int want[7] = { 0, 0, 0, 1, 0, 1, 0 };
    thread_generic(GENERIC_WRITE, STANDARD_RIGHTS_WRITE | THREAD_TERMINATE | THREAD_SUSPEND_RESUME | THREAD_SET_INFORMATION | THREAD_SET_CONTEXT | THREAD_SET_LIMITED_INFORMATION | THREAD_RESUME | XPT_THREAD_ALERT, want, 1);
}
TEST(xpt_gen_thread_generic_execute) {
    static const int want[7] = { 1, 1, 1, 0, 0, 0, 0 };
    thread_generic(GENERIC_EXECUTE, STANDARD_RIGHTS_EXECUTE | SYNCHRONIZE | THREAD_RESUME | THREAD_QUERY_LIMITED_INFORMATION, want, 0);
}
TEST(xpt_gen_thread_generic_all) {
    static const int want[7] = { 1, 1, 1, 1, 1, 1, 1 };
    thread_generic(GENERIC_ALL, THREAD_ALL_ACCESS, want, 1);
}
TEST(xpt_gen_maximum_allowed_own_child) {
    PROCESS_INFORMATION pi; HANDLE hp = xpt_spawn_sleeper(&pi);
    CHECK(hp != NULL); if (!hp) return;
    HANDLE h = U(OpenProcess)(MAXIMUM_ALLOWED, FALSE, pi.dwProcessId);
    CHECKF(h != NULL, "err=%lu", GetLastError());
    if (h) { EQ(xpt_access(h), PROCESS_ALL_ACCESS); CloseHandle(h); }
    xpt_kill(&pi);
}

/* ---- DuplicateHandle ---- */
static const int T_QL[7] = { 0, 1, 1, 0, 0, 0, 0 };
static const int T_SYNC[7] = { 1, 0, 0, 0, 0, 0, 0 };
static const int T_ALL[7] = { 1, 1, 1, 1, 1, 1, 1 };
static const int P_QL[7] = { 0, 1, 1, 0, 0, 0, 1 };
static const int P_VMOP[7] = { 0, 0, 0, 0, 0, 1, 0 };
static const int P_ALL[7] = { 1, 1, 1, 1, 1, 1, 1 };

static HANDLE dup_to(HANDLE src, DWORD acc, DWORD opt) {
    HANDLE d = NULL;
    if (!U(DuplicateHandle)(GetCurrentProcess(), src, GetCurrentProcess(), &d, acc, FALSE, opt)) return NULL;
    return d;
}
TEST(xpt_dup_thread_reduced_query_limited) {
    xpt_tw w; if (!xpt_tw_start(&w)) { CHECK(!"worker"); xpt_tw_stop(&w); return; }
    HANDLE d = dup_to(w.h, THREAD_QUERY_LIMITED_INFORMATION, 0), o = U(OpenThread)(THREAD_QUERY_LIMITED_INFORMATION, FALSE, w.tid);
    CHECK(d && o);
    if (d) { EQ(xpt_access(d), THREAD_QUERY_LIMITED_INFORMATION); thread_row("dup QL", d, w.h, T_QL); CloseHandle(d); }
    if (o) { EQ(xpt_access(o), THREAD_QUERY_LIMITED_INFORMATION); thread_row("open QL", o, w.h, T_QL); CloseHandle(o); }
    xpt_tw_stop(&w);
}
TEST(xpt_dup_thread_reduced_synchronize) {
    xpt_tw w; if (!xpt_tw_start(&w)) { CHECK(!"worker"); xpt_tw_stop(&w); return; }
    HANDLE d = dup_to(w.h, SYNCHRONIZE, 0);
    CHECK(d != NULL);
    if (d) { EQ(xpt_access(d), SYNCHRONIZE); thread_row("dup SYNC", d, w.h, T_SYNC); CloseHandle(d); }
    xpt_tw_stop(&w);
}
TEST(xpt_dup_thread_same_access_full) {
    xpt_tw w; if (!xpt_tw_start(&w)) { CHECK(!"worker"); xpt_tw_stop(&w); return; }
    HANDLE d = dup_to(w.h, 0, DUPLICATE_SAME_ACCESS);
    CHECK(d != NULL);
    if (d) { EQ(xpt_access(d), xpt_access(w.h)); thread_row("dup SAME", d, w.h, T_ALL); CloseHandle(d); }
    xpt_tw_stop(&w);
}
TEST(xpt_dup_thread_same_access_keeps_restriction) {
    xpt_tw w; if (!xpt_tw_start(&w)) { CHECK(!"worker"); xpt_tw_stop(&w); return; }
    HANDLE o = U(OpenThread)(THREAD_QUERY_LIMITED_INFORMATION, FALSE, w.tid);
    HANDLE d = o ? dup_to(o, 0, DUPLICATE_SAME_ACCESS) : NULL;
    CHECK(d != NULL);
    if (d) { EQ(xpt_access(d), THREAD_QUERY_LIMITED_INFORMATION); thread_row("dup SAME of QL", d, w.h, T_QL); CloseHandle(d); }
    if (o) CloseHandle(o);
    xpt_tw_stop(&w);
}
TEST(xpt_dup_thread_widen_from_restricted) {
    /* Asking for more than the source handle grants runs an access check against the thread object. */
    xpt_tw w; if (!xpt_tw_start(&w)) { CHECK(!"worker"); xpt_tw_stop(&w); return; }
    HANDLE o = U(OpenThread)(SYNCHRONIZE, FALSE, w.tid);
    HANDLE d = NULL;
    if (o) SUCCEEDS(U(DuplicateHandle)(GetCurrentProcess(), o, GetCurrentProcess(), &d, THREAD_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, 0));
    if (d) { EQ(xpt_access(d), THREAD_QUERY_LIMITED_INFORMATION | SYNCHRONIZE); CloseHandle(d); }
    if (o) CloseHandle(o);
    xpt_tw_stop(&w);
}
TEST(xpt_dup_thread_pseudo_reduced) {
    HANDLE d = dup_to(GetCurrentThread(), THREAD_QUERY_LIMITED_INFORMATION, 0);
    CHECK(d != NULL); if (!d) return;
    EQ(xpt_access(d), THREAD_QUERY_LIMITED_INFORMATION);
    EQ(U(GetThreadId)(d), GetCurrentThreadId());
    FAILS(U(SetThreadPriority)(d, THREAD_PRIORITY_NORMAL), ERROR_ACCESS_DENIED);
    SetLastError(0); EQ(U(SuspendThread)(d), (DWORD)-1); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(d);
}
TEST(xpt_dup_thread_close_source_reduced) {
    xpt_tw w; if (!xpt_tw_start(&w)) { CHECK(!"worker"); xpt_tw_stop(&w); return; }
    HANDLE src = dup_to(w.h, 0, DUPLICATE_SAME_ACCESS);
    HANDLE d = NULL;
    CHECK(src != NULL);
    if (src) SUCCEEDS(U(DuplicateHandle)(GetCurrentProcess(), src, GetCurrentProcess(), &d, SYNCHRONIZE, FALSE, DUPLICATE_CLOSE_SOURCE));
    if (d) { EQ(xpt_access(d), SYNCHRONIZE); thread_row("dup close-source SYNC", d, w.h, T_SYNC); CloseHandle(d); }
    xpt_tw_stop(&w);
}
TEST(xpt_dup_process_reduced_query_limited) {
    PROCESS_INFORMATION pi; HANDLE hp = xpt_spawn_sleeper(&pi);
    CHECK(hp != NULL); if (!hp) return;
    HANDLE d = dup_to(hp, PROCESS_QUERY_LIMITED_INFORMATION, 0), o = U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pi.dwProcessId);
    CHECK(d && o);
    if (d) { EQ(xpt_access(d), PROCESS_QUERY_LIMITED_INFORMATION); proc_row("dup QL", d, hp, P_QL); CloseHandle(d); }
    if (o) { EQ(xpt_access(o), PROCESS_QUERY_LIMITED_INFORMATION); proc_row("open QL", o, hp, P_QL); CloseHandle(o); }
    xpt_kill(&pi);
}
TEST(xpt_dup_process_reduced_vm_operation) {
    PROCESS_INFORMATION pi; HANDLE hp = xpt_spawn_sleeper(&pi);
    CHECK(hp != NULL); if (!hp) return;
    HANDLE d = dup_to(hp, PROCESS_VM_OPERATION, 0);
    CHECK(d != NULL);
    if (d) { EQ(xpt_access(d), PROCESS_VM_OPERATION); proc_row("dup VMOP", d, hp, P_VMOP); CloseHandle(d); }
    xpt_kill(&pi);
}
TEST(xpt_dup_process_same_access) {
    PROCESS_INFORMATION pi; HANDLE hp = xpt_spawn_sleeper(&pi);
    CHECK(hp != NULL); if (!hp) return;
    HANDLE d = dup_to(hp, 0, DUPLICATE_SAME_ACCESS);
    CHECK(d != NULL);
    if (d) { EQ(xpt_access(d), xpt_access(hp)); proc_row("dup SAME", d, hp, P_ALL); CloseHandle(d); }
    xpt_kill(&pi);
}
TEST(xpt_dup_process_exited_reduced) {
    PROCESS_INFORMATION pi; if (!xpt_spawn_dead(&pi)) { CHECK(!"child"); return; }
    HANDLE d = dup_to(pi.hProcess, PROCESS_QUERY_LIMITED_INFORMATION, 0);
    CHECK(d != NULL);
    if (d) {
        DWORD c = 0; SUCCEEDS(U(GetExitCodeProcess)(d, &c)); EQ(c, XPT_CHILD_EXIT);
        SetLastError(0); EQ(WaitForSingleObject(d, 0), WAIT_FAILED); EXPECT_ERR(ERROR_ACCESS_DENIED);
        CloseHandle(d);
    }
    xpt_kill(&pi);
}

/* ---- implied limited rights ---- */
TEST(xpt_impl_thread_query_info_allows_limited_ops) {
    xpt_tw w; if (!xpt_tw_start(&w)) { CHECK(!"worker"); xpt_tw_stop(&w); return; }
    HANDLE h = U(OpenThread)(THREAD_QUERY_INFORMATION, FALSE, w.tid);
    CHECK(h != NULL);
    if (h) {
        EQ(xpt_access(h), THREAD_QUERY_INFORMATION | THREAD_QUERY_LIMITED_INFORMATION);
        DWORD c; SUCCEEDS(U(GetExitCodeThread)(h, &c)); EQ(c, STILL_ACTIVE);
        EQ(U(GetThreadId)(h), w.tid);
        FILETIME a, b, d, e; SUCCEEDS(U(GetThreadTimes)(h, &a, &b, &d, &e));
        ULONG64 cy; SUCCEEDS(U(QueryThreadCycleTime)(h, &cy));
        if (UN(GetThreadDescription)) { PWSTR p = NULL; EQ(UN(GetThreadDescription)(h, &p), XPT_HR_NT_OK); if (p) LocalFree(p); }
        CloseHandle(h);
    }
    xpt_tw_stop(&w);
}
TEST(xpt_impl_thread_set_info_allows_limited_ops) {
    xpt_tw w; if (!xpt_tw_start(&w)) { CHECK(!"worker"); xpt_tw_stop(&w); return; }
    HANDLE h = U(OpenThread)(THREAD_SET_INFORMATION, FALSE, w.tid);
    CHECK(h != NULL);
    if (h) {
        EQ(xpt_access(h), THREAD_SET_INFORMATION | THREAD_SET_LIMITED_INFORMATION);
        SUCCEEDS(U(SetThreadPriority)(h, THREAD_PRIORITY_BELOW_NORMAL));
        EQ(GetThreadPriority(w.h), THREAD_PRIORITY_BELOW_NORMAL);
        if (UN(SetThreadDescription)) EQ(UN(SetThreadDescription)(h, L"xpt-impl"), XPT_HR_NT_OK);
        SUCCEEDS(U(SetThreadPriorityBoost)(h, FALSE));
        CloseHandle(h);
    }
    xpt_tw_stop(&w);
}
TEST(xpt_impl_process_query_info_allows_limited_ops) {
    PROCESS_INFORMATION pi; HANDLE hp = xpt_spawn_sleeper(&pi);
    CHECK(hp != NULL); if (!hp) return;
    HANDLE h = U(OpenProcess)(PROCESS_QUERY_INFORMATION, FALSE, pi.dwProcessId);
    CHECK(h != NULL);
    if (h) {
        EQ(xpt_access(h), PROCESS_QUERY_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION);
        DWORD c; SUCCEEDS(U(GetExitCodeProcess)(h, &c)); EQ(c, STILL_ACTIVE);
        EQ(U(GetProcessId)(h), pi.dwProcessId);
        WCHAR b[MAX_PATH * 2]; DWORD n = MAX_PATH * 2; SUCCEEDS(U(QueryFullProcessImageNameW)(h, 0, b, &n));
        EQ(U(GetPriorityClass)(h), NORMAL_PRIORITY_CLASS);
        CloseHandle(h);
    }
    xpt_kill(&pi);
}
TEST(xpt_impl_process_set_info_allows_limited_ops) {
    PROCESS_INFORMATION pi; HANDLE hp = xpt_spawn_sleeper(&pi);
    CHECK(hp != NULL); if (!hp) return;
    HANDLE h = U(OpenProcess)(PROCESS_SET_INFORMATION, FALSE, pi.dwProcessId);
    CHECK(h != NULL);
    if (h) {
        EQ(xpt_access(h), PROCESS_SET_INFORMATION | PROCESS_SET_LIMITED_INFORMATION);
        SUCCEEDS(U(SetPriorityClass)(h, BELOW_NORMAL_PRIORITY_CLASS));
        EQ(GetPriorityClass(hp), BELOW_NORMAL_PRIORITY_CLASS);
        CloseHandle(h);
    }
    xpt_kill(&pi);
}
TEST(xpt_impl_limited_pair_affinity_but_not_ideal) {
    xpt_tw w; if (!xpt_tw_start(&w)) { CHECK(!"worker"); xpt_tw_stop(&w); return; }
    HANDLE h = U(OpenThread)(THREAD_QUERY_LIMITED_INFORMATION | THREAD_SET_LIMITED_INFORMATION, FALSE, w.tid);
    CHECK(h != NULL);
    if (h) {
        EQ(xpt_access(h), THREAD_QUERY_LIMITED_INFORMATION | THREAD_SET_LIMITED_INFORMATION);
        DWORD_PTR pm, sm; GetProcessAffinityMask(GetCurrentProcess(), &pm, &sm);
        EQ(U(SetThreadAffinityMask)(h, pm), pm);
        PROCESSOR_NUMBER p; GetThreadIdealProcessorEx(w.h, &p);
        FAILS(U(SetThreadIdealProcessorEx)(h, &p, NULL), ERROR_ACCESS_DENIED);
        CloseHandle(h);
    }
    xpt_tw_stop(&w);
}
TEST(xpt_impl_setthreadaffinitymask_minimum) {
    xpt_tw w; if (!xpt_tw_start(&w)) { CHECK(!"worker"); xpt_tw_stop(&w); return; }
    DWORD_PTR pm, sm; GetProcessAffinityMask(GetCurrentProcess(), &pm, &sm);
    /* a set right and a query right are both needed; the limited forms suffice */
    static const DWORD combos[] = { THREAD_SET_INFORMATION | THREAD_QUERY_INFORMATION, THREAD_SET_INFORMATION | THREAD_QUERY_LIMITED_INFORMATION,
                                    THREAD_SET_LIMITED_INFORMATION | THREAD_QUERY_INFORMATION, THREAD_SET_LIMITED_INFORMATION | THREAD_QUERY_LIMITED_INFORMATION,
                                    THREAD_SET_INFORMATION, THREAD_SET_LIMITED_INFORMATION, THREAD_QUERY_INFORMATION, THREAD_SET_LIMITED_INFORMATION | SYNCHRONIZE };
    static const int want[] = { 1, 1, 1, 1, 0, 0, 0, 0 };
    for (int i = 0; i < 8; i++) {
        HANDLE h = U(OpenThread)(combos[i], FALSE, w.tid);
        if (!h) { CHECK(h != NULL); continue; }
        SetLastError(0);
        DWORD_PTR r = U(SetThreadAffinityMask)(h, pm);
        DWORD e = GetLastError();
        CHECKF((r != 0) == want[i], "access 0x%lx: r=0x%llx err=%lu", combos[i], (unsigned long long)r, e);
        if (r) EQ(r, pm); else CHECKF(e == ERROR_ACCESS_DENIED, "access 0x%lx err=%lu", combos[i], e);
        CloseHandle(h);
    }
    xpt_tw_stop(&w);
}
TEST(xpt_impl_setprocessaffinitymask_minimum) {
    PROCESS_INFORMATION pi; HANDLE hp = xpt_spawn_sleeper(&pi);
    CHECK(hp != NULL); if (!hp) return;
    DWORD_PTR pm, sm; GetProcessAffinityMask(hp, &pm, &sm);
    HANDLE h = U(OpenProcess)(PROCESS_SET_INFORMATION, FALSE, pi.dwProcessId);
    CHECK(h != NULL);
    if (h) { SUCCEEDS(U(SetProcessAffinityMask)(h, pm)); CloseHandle(h); }
    h = U(OpenProcess)(PROCESS_SET_LIMITED_INFORMATION | PROCESS_QUERY_INFORMATION, FALSE, pi.dwProcessId);
    CHECK(h != NULL);
    if (h) { FAILS(U(SetProcessAffinityMask)(h, pm), ERROR_ACCESS_DENIED); CloseHandle(h); }
    xpt_kill(&pi);
}

/* ---- out-parameters untouched on ERROR_ACCESS_DENIED ---- */
#define SENT 0xA5
static int all_sent(const void *p, size_t n) { const BYTE *b = p; for (size_t i = 0; i < n; i++) if (b[i] != SENT) return 0; return 1; }

TEST(xpt_sent_getthreadtimes) {
    xpt_tw w; if (!xpt_tw_start(&w)) { CHECK(!"worker"); xpt_tw_stop(&w); return; }
    HANDLE h = U(OpenThread)(SYNCHRONIZE, FALSE, w.tid);
    if (h) {
        FILETIME t[4]; memset(t, SENT, sizeof t);
        FAILS(U(GetThreadTimes)(h, &t[0], &t[1], &t[2], &t[3]), ERROR_ACCESS_DENIED);
        CHECK(all_sent(t, sizeof t));
        CloseHandle(h);
    } else CHECK(h != NULL);
    xpt_tw_stop(&w);
}
TEST(xpt_sent_getexitcodethread_and_cycle) {
    xpt_tw w; if (!xpt_tw_start(&w)) { CHECK(!"worker"); xpt_tw_stop(&w); return; }
    HANDLE h = U(OpenThread)(SYNCHRONIZE, FALSE, w.tid);
    if (h) {
        DWORD c; memset(&c, SENT, sizeof c);
        FAILS(U(GetExitCodeThread)(h, &c), ERROR_ACCESS_DENIED);
        CHECK(all_sent(&c, sizeof c));
        ULONG64 cy; memset(&cy, SENT, sizeof cy);
        FAILS(U(QueryThreadCycleTime)(h, &cy), ERROR_ACCESS_DENIED);
        CHECK(all_sent(&cy, sizeof cy));
        BOOL b; memset(&b, SENT, sizeof b);
        FAILS(U(GetThreadPriorityBoost)(h, &b), ERROR_ACCESS_DENIED);
        CHECK(all_sent(&b, sizeof b));
        memset(&b, SENT, sizeof b);
        FAILS(U(GetThreadIOPendingFlag)(h, &b), ERROR_ACCESS_DENIED);
        CHECK(all_sent(&b, sizeof b));
        CloseHandle(h);
    } else CHECK(h != NULL);
    xpt_tw_stop(&w);
}
TEST(xpt_sent_thread_affinity_ideal_context) {
    xpt_tw w; if (!xpt_tw_start(&w)) { CHECK(!"worker"); xpt_tw_stop(&w); return; }
    HANDLE h = U(OpenThread)(SYNCHRONIZE, FALSE, w.tid);
    if (h) {
        GROUP_AFFINITY g; memset(&g, SENT, sizeof g);
        FAILS(U(GetThreadGroupAffinity)(h, &g), ERROR_ACCESS_DENIED);
        CHECK(all_sent(&g, sizeof g));
        PROCESSOR_NUMBER p; memset(&p, SENT, sizeof p);
        FAILS(U(GetThreadIdealProcessorEx)(h, &p), ERROR_ACCESS_DENIED);
        CHECK(all_sent(&p, sizeof p));
        MEMORY_PRIORITY_INFORMATION m; memset(&m, SENT, sizeof m);
        FAILS(U(GetThreadInformation)(h, ThreadMemoryPriority, &m, sizeof m), ERROR_ACCESS_DENIED);
        CHECK(all_sent(&m, sizeof m));
        CONTEXT cx; memset(&cx, SENT, sizeof cx); cx.ContextFlags = CONTEXT_CONTROL;
        FAILS(U(GetThreadContext)(h, &cx), ERROR_ACCESS_DENIED);
        CHECK(all_sent(&cx.Rip, sizeof cx.Rip) && all_sent(&cx.Rsp, sizeof cx.Rsp));
        CloseHandle(h);
    } else CHECK(h != NULL);
    xpt_tw_stop(&w);
}
TEST(xpt_sent_getthreaddescription) {
    REQUIRE(UN(GetThreadDescription));
    xpt_tw w; if (!xpt_tw_start(&w)) { CHECK(!"worker"); xpt_tw_stop(&w); return; }
    HANDLE h = U(OpenThread)(SYNCHRONIZE, FALSE, w.tid);
    if (h) {
        PWSTR p = (PWSTR)(ULONG_PTR)0xA5A5A5A5A5A5A5A5ull;
        EQ(UN(GetThreadDescription)(h, &p), XPT_HR_DENIED);
        EQ(p, NULL);   /* the out pointer is cleared, not left alone */
        CloseHandle(h);
    } else CHECK(h != NULL);
    xpt_tw_stop(&w);
}
TEST(xpt_sent_getprocesstimes_and_cycle) {
    PROCESS_INFORMATION pi; HANDLE hp = xpt_spawn_sleeper(&pi);
    CHECK(hp != NULL); if (!hp) return;
    HANDLE h = U(OpenProcess)(SYNCHRONIZE, FALSE, pi.dwProcessId);
    if (h) {
        FILETIME t[4]; memset(t, SENT, sizeof t);
        FAILS(U(GetProcessTimes)(h, &t[0], &t[1], &t[2], &t[3]), ERROR_ACCESS_DENIED);
        CHECK(all_sent(t, sizeof t));
        ULONG64 cy; memset(&cy, SENT, sizeof cy);
        FAILS(U(QueryProcessCycleTime)(h, &cy), ERROR_ACCESS_DENIED);
        CHECK(all_sent(&cy, sizeof cy));
        DWORD c; memset(&c, SENT, sizeof c);
        FAILS(U(GetExitCodeProcess)(h, &c), ERROR_ACCESS_DENIED);
        CHECK(all_sent(&c, sizeof c));
        CloseHandle(h);
    } else CHECK(h != NULL);
    xpt_kill(&pi);
}
TEST(xpt_sent_process_affinity_counts_boost) {
    PROCESS_INFORMATION pi; HANDLE hp = xpt_spawn_sleeper(&pi);
    CHECK(hp != NULL); if (!hp) return;
    HANDLE h = U(OpenProcess)(SYNCHRONIZE, FALSE, pi.dwProcessId);
    if (h) {
        DWORD_PTR a[2]; memset(a, SENT, sizeof a);
        FAILS(U(GetProcessAffinityMask)(h, &a[0], &a[1]), ERROR_ACCESS_DENIED);
        CHECK(all_sent(a, sizeof a));
        DWORD n; memset(&n, SENT, sizeof n);
        FAILS(U(GetProcessHandleCount)(h, &n), ERROR_ACCESS_DENIED);
        CHECK(all_sent(&n, sizeof n));
        IO_COUNTERS io; memset(&io, SENT, sizeof io);
        FAILS(U(GetProcessIoCounters)(h, &io), ERROR_ACCESS_DENIED);
        CHECK(all_sent(&io, sizeof io));
        BOOL b; memset(&b, SENT, sizeof b);
        FAILS(U(GetProcessPriorityBoost)(h, &b), ERROR_ACCESS_DENIED);
        CHECK(all_sent(&b, sizeof b));
        CloseHandle(h);
    } else CHECK(h != NULL);
    xpt_kill(&pi);
}
TEST(xpt_sent_queryfullprocessimagenamew) {
    PROCESS_INFORMATION pi; HANDLE hp = xpt_spawn_sleeper(&pi);
    CHECK(hp != NULL); if (!hp) return;
    HANDLE h = U(OpenProcess)(SYNCHRONIZE, FALSE, pi.dwProcessId);
    if (h) {
        WCHAR b[64]; memset(b, SENT, sizeof b); DWORD n = 64;
        FAILS(U(QueryFullProcessImageNameW)(h, 0, b, &n), ERROR_ACCESS_DENIED);
        CHECK(all_sent(b, sizeof b));
        EQ(n, 64);
        CloseHandle(h);
    } else CHECK(h != NULL);
    xpt_kill(&pi);
}
TEST(xpt_sent_getprocessinformation_and_wow64) {
    PROCESS_INFORMATION pi; HANDLE hp = xpt_spawn_sleeper(&pi);
    CHECK(hp != NULL); if (!hp) return;
    HANDLE h = U(OpenProcess)(SYNCHRONIZE, FALSE, pi.dwProcessId);
    if (h) {
        MEMORY_PRIORITY_INFORMATION m; memset(&m, SENT, sizeof m);
        FAILS(U(GetProcessInformation)(h, ProcessMemoryPriority, &m, sizeof m), ERROR_ACCESS_DENIED);
        CHECK(all_sent(&m, sizeof m));
        if (UN(IsWow64Process2)) {
            USHORT u[2]; memset(u, SENT, sizeof u);
            FAILS(UN(IsWow64Process2)(h, &u[0], &u[1]), ERROR_ACCESS_DENIED);
            CHECK(all_sent(u, sizeof u));
        }
        CloseHandle(h);
    } else CHECK(h != NULL);
    xpt_kill(&pi);
}
