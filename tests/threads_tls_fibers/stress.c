/* Many-thread stress, TEB sanity and in-thread exception handling. */
#include "tf.h"

static __typeof__(&TlsAlloc) tAlloc; static __typeof__(&TlsFree) tFree; static __typeof__(&TlsGetValue) tGet; static __typeof__(&TlsSetValue) tSet;
static __typeof__(&FlsAlloc) fAlloc; static __typeof__(&FlsFree) fFree; static __typeof__(&FlsGetValue) fGet; static __typeof__(&FlsSetValue) fSet;

static DWORD g_t, g_f;
static HANDLE g_barrier;
static volatile LONG g_arrived, g_fcb;
static VOID WINAPI stress_fcb(PVOID p) { InterlockedIncrement(&g_fcb); }
static DWORD WINAPI th_stress(LPVOID p) {
    ULONG_PTR id = (ULONG_PTR)p;
    DWORD bits = 0;
    if (tGet(g_t) != NULL) bits |= 1;
    if (fGet(g_f) != NULL) bits |= 2;
    tSet(g_t, (LPVOID)(0x10000 + id));
    fSet(g_f, (LPVOID)(0x20000 + id));
    InterlockedIncrement(&g_arrived);
    WaitForSingleObject(g_barrier, 15000);
    if (tGet(g_t) != (LPVOID)(0x10000 + id)) bits |= 4;
    if (fGet(g_f) != (LPVOID)(0x20000 + id)) bits |= 8;
    SetLastError(0x1000 + (DWORD)id);
    Sleep(1);
    if (GetLastError() != 0x1000 + (DWORD)id) bits |= 0x10;
    return bits | ((DWORD)id << 8);
}
TEST(stress_128_threads_tls_fls) {
    tAlloc = U(TlsAlloc); tFree = U(TlsFree); tGet = U(TlsGetValue); tSet = U(TlsSetValue);
    fAlloc = U(FlsAlloc); fFree = U(FlsFree); fGet = U(FlsGetValue); fSet = U(FlsSetValue);
    REQUIRE(tAlloc); REQUIRE(tFree); REQUIRE(tGet); REQUIRE(tSet); REQUIRE(fAlloc); REQUIRE(fFree); REQUIRE(fGet); REQUIRE(fSet);
    REQUIRE(U(CreateThread));
    enum { N = 128 };
    g_t = tAlloc(); g_f = fAlloc(stress_fcb);
    g_barrier = tf_ev(TRUE, FALSE); g_arrived = 0; g_fcb = 0;
    HANDLE hs[N];
    int created = 0;
    for (int i = 0; i < N; i++) { hs[i] = U(CreateThread)(NULL, 0x10000, th_stress, (LPVOID)(ULONG_PTR)i, 0, NULL); if (hs[i]) created++; }
    EQ(created, N);
    for (int k = 0; k < 1000 && g_arrived < created; k++) Sleep(10);
    EQ(g_arrived, created);
    SetEvent(g_barrier);
    int bad = 0, idbad = 0;
    for (int i = 0; i < N; i++) {
        if (!hs[i]) continue;
        DWORD c = tf_join(hs[i]);
        if ((c & 0xFF) != 0) bad++;
        if ((c >> 8) != (DWORD)i) idbad++;
    }
    EQ(bad, 0);
    EQ(idbad, 0);
    EQ(g_fcb, N);       /* every thread's FLS value triggered the callback */
    tFree(g_t); fFree(g_f);
    CloseHandle(g_barrier);
}
TEST(stress_create_join_1000) {
    REQUIRE(U(CreateThread));
    int bad = 0;
    for (int i = 0; i < 1000; i++) {
        HANDLE h = U(CreateThread)(NULL, 0, tf_th_ret, (LPVOID)(ULONG_PTR)i, 0, NULL);
        if (!h) { bad++; continue; }
        if (tf_join(h) != (DWORD)i) bad++;
    }
    EQ(bad, 0);
}
TEST(stress_unique_exit_codes_64) {
    REQUIRE(U(CreateThread));
    enum { N = 64 };
    HANDLE hs[N];
    for (int i = 0; i < N; i++) hs[i] = U(CreateThread)(NULL, 0, tf_th_ret, (LPVOID)(ULONG_PTR)(i * 3 + 1), 0, NULL);
    int bad = 0;
    for (int i = 0; i < N; i++) if (tf_join(hs[i]) != (DWORD)(i * 3 + 1)) bad++;
    EQ(bad, 0);
}
static struct { ULONG_PTR lo, hi; int ok; } sr[64];
static DWORD WINAPI th_range(LPVOID p) {
    int i = (int)(ULONG_PTR)p;
    volatile int x;
    char *teb = (char *)NtCurrentTeb();
    ULONG_PTR base = *(ULONG_PTR *)(teb + 8), lim = *(ULONG_PTR *)(teb + 0x10), dealloc = *(ULONG_PTR *)(teb + 0x1478);
    sr[i].lo = dealloc; sr[i].hi = base;
    sr[i].ok = ((ULONG_PTR)&x < base && (ULONG_PTR)&x >= lim && lim >= dealloc && base > dealloc);
    HANDLE ev = (HANDLE)0; (void)ev;
    InterlockedIncrement(&g_arrived);
    for (int k = 0; k < 1000 && g_arrived < 64; k++) Sleep(5);   /* all threads alive together: stacks cannot be reused */
    return 0;
}
TEST(stress_stack_ranges_disjoint) {
    REQUIRE(U(CreateThread));
    enum { N = 64 };
    HANDLE hs[N];
    memset(sr, 0, sizeof sr);
    g_arrived = 0;
    for (int i = 0; i < N; i++) hs[i] = U(CreateThread)(NULL, 0, th_range, (LPVOID)(ULONG_PTR)i, 0, NULL);
    for (int i = 0; i < N; i++) tf_join(hs[i]);
    int bad = 0, overlap = 0;
    for (int i = 0; i < N; i++) if (!sr[i].ok) bad++;
    for (int i = 0; i < N; i++) for (int j = i + 1; j < N; j++) if (sr[i].lo < sr[j].hi && sr[j].lo < sr[i].hi) overlap++;
    EQ(bad, 0);
    EQ(overlap, 0);
}
static struct { ULONG_PTR teb, peb; DWORD pid, tid, err; } tb[16];
static DWORD WINAPI th_teb(LPVOID p) {
    int i = (int)(ULONG_PTR)p;
    char *teb = (char *)NtCurrentTeb();
    tb[i].teb = (ULONG_PTR)teb;
    tb[i].peb = *(ULONG_PTR *)(teb + 0x60);
    tb[i].pid = (DWORD)*(ULONG_PTR *)(teb + 0x40);
    tb[i].tid = (DWORD)*(ULONG_PTR *)(teb + 0x48);
    SetLastError(0x5000 + i);
    tb[i].err = *(DWORD *)(teb + 0x68);
    InterlockedIncrement(&g_arrived);
    for (int k = 0; k < 1000 && g_arrived < 16; k++) Sleep(5);
    if (*(ULONG_PTR *)(teb + 0x30) != (ULONG_PTR)teb) return 1;    /* Self */
    return GetCurrentThreadId() == tb[i].tid ? 0 : 2;
}
TEST(stress_teb_fields) {
    REQUIRE(U(CreateThread));
    enum { N = 16 };
    HANDLE hs[N];
    memset(tb, 0, sizeof tb);
    g_arrived = 0;
    DWORD tids[N];
    for (int i = 0; i < N; i++) hs[i] = U(CreateThread)(NULL, 0, th_teb, (LPVOID)(ULONG_PTR)i, 0, &tids[i]);
    int bad = 0;
    for (int i = 0; i < N; i++) if (tf_join(hs[i]) != 0) bad++;
    EQ(bad, 0);
    bad = 0;
    char *mteb = (char *)NtCurrentTeb();
    for (int i = 0; i < N; i++) {
        if (tb[i].peb != *(ULONG_PTR *)(mteb + 0x60)) { bad++; t_note("peb mismatch %d", i); }
        if (tb[i].pid != GetCurrentProcessId()) { bad++; t_note("pid mismatch %d: %lx", i, tb[i].pid); }
        if (tb[i].tid != tids[i]) { bad++; t_note("tid mismatch %d: %lx vs %lx", i, tb[i].tid, tids[i]); }
        if (tb[i].err != 0x5000u + i) { bad++; t_note("err mismatch %d: %lx", i, tb[i].err); }
        for (int j = i + 1; j < N; j++) if (tb[i].teb == tb[j].teb) bad++;
    }
    EQ(bad, 0);
}
TEST(stress_last_error_per_thread) {
    REQUIRE(U(CreateThread));
    SET_ERR(0x7777);
    DWORD WINAPI body(LPVOID p) {
        DWORD e0 = GetLastError();               /* new thread starts at 0 */
        SetLastError(0x8888);
        return e0;
    }
    EQ(tf_run(body, NULL), 0);
    EXPECT_ERR(0x7777);
}
TEST(stress_thread_param_rcx_is_full_pointer) {
    REQUIRE(U(CreateThread));
    static const ULONG_PTR v[] = { 0x7FFF12345678ULL, 0x800000000000ULL, 0xFFFF800000000000ULL, (ULONG_PTR)&v };
    for (size_t i = 0; i < 4; i++) {
        tf_last_param = 0;
        tf_join(U(CreateThread)(NULL, 0, tf_th_store, (LPVOID)v[i], 0, NULL));
        CHECKF(tf_last_param == v[i], "i=%zu", i);
    }
}

/* ---- exceptions in threads, recovered by a vectored handler ---- */
static volatile LONG veh_hits;
static volatile DWORD veh_tid[64];
static volatile ULONG_PTR veh_info0, veh_info1, veh_addr;
static volatile DWORD veh_code;
static LONG CALLBACK rec_veh(PEXCEPTION_POINTERS ep) {
    PEXCEPTION_RECORD er = ep->ExceptionRecord;
    ULONG_PTR rip = (ULONG_PTR)ep->ContextRecord->Rip;
    DWORD c = er->ExceptionCode;
    ULONG_PTR to = 0;
    if (c == 0xE0000042) { LONG i = InterlockedIncrement(&veh_hits) - 1; if (i < 64) veh_tid[i] = GetCurrentThreadId(); return EXCEPTION_CONTINUE_EXECUTION; }
    if (c == EXCEPTION_ACCESS_VIOLATION && rip == (ULONG_PTR)tf_fault_av_ins) to = (ULONG_PTR)tf_fault_av_after;
    else if (c == EXCEPTION_INT_DIVIDE_BY_ZERO && rip == (ULONG_PTR)tf_fault_div_ins) to = (ULONG_PTR)tf_fault_div_after;
    else if (c == EXCEPTION_ILLEGAL_INSTRUCTION && rip == (ULONG_PTR)tf_fault_ud_ins) to = (ULONG_PTR)tf_fault_ud_after;
    else if (c == EXCEPTION_PRIV_INSTRUCTION && rip == (ULONG_PTR)tf_fault_hlt_ins) to = (ULONG_PTR)tf_fault_hlt_after;
    else if (c == EXCEPTION_BREAKPOINT && (ULONG_PTR)er->ExceptionAddress >= (ULONG_PTR)tf_fault_bp_ins && (ULONG_PTR)er->ExceptionAddress < (ULONG_PTR)tf_fault_bp_ins + 2) { veh_code = c; veh_addr = (ULONG_PTR)er->ExceptionAddress; return EXCEPTION_CONTINUE_EXECUTION; }
    if (!to) return EXCEPTION_CONTINUE_SEARCH;
    veh_code = c; veh_addr = (ULONG_PTR)er->ExceptionAddress;
    veh_info0 = er->NumberParameters > 0 ? er->ExceptionInformation[0] : 0xCC;
    veh_info1 = er->NumberParameters > 1 ? er->ExceptionInformation[1] : 0xCC;
    ep->ContextRecord->Rip = to;
    return EXCEPTION_CONTINUE_EXECUTION;
}
static DWORD run_fault(LPTHREAD_START_ROUTINE f) {
    PVOID h = AddVectoredExceptionHandler(1, rec_veh);
    veh_code = 0; veh_addr = 0; veh_info0 = veh_info1 = 0xCC;
    DWORD r = tf_run(f, NULL);
    RemoveVectoredExceptionHandler(h);
    return r;
}
TEST(exc_thread_access_violation_recovered) {
    REQUIRE(U(CreateThread));
    EQ(run_fault(tf_fault_av), 0x1234);
    EQ(veh_code, EXCEPTION_ACCESS_VIOLATION);
    EQ(veh_addr, (ULONG_PTR)tf_fault_av_ins);
    EQ(veh_info0, 1);        /* write */
    EQ(veh_info1, 0);        /* address */
}
TEST(exc_thread_divide_by_zero_recovered) {
    REQUIRE(U(CreateThread));
    EQ(run_fault(tf_fault_div), 0x1235);
    EQ(veh_code, EXCEPTION_INT_DIVIDE_BY_ZERO);
    EQ(veh_addr, (ULONG_PTR)tf_fault_div_ins);
}
TEST(exc_thread_illegal_instruction_recovered) {
    REQUIRE(U(CreateThread));
    EQ(run_fault(tf_fault_ud), 0x1236);
    EQ(veh_code, EXCEPTION_ILLEGAL_INSTRUCTION);
}
TEST(exc_thread_privileged_instruction_recovered) {
    REQUIRE(U(CreateThread));
    EQ(run_fault(tf_fault_hlt), 0x1237);
    EQ(veh_code, EXCEPTION_PRIV_INSTRUCTION);
}
TEST(exc_thread_breakpoint_continue) {
    REQUIRE(U(CreateThread));
    if (IsDebuggerPresent()) { t_skip("debugger present"); return; }
    if (t_is_wine()) { t_skip("breakpoint continuation hangs under wine"); return; }
    EQ(run_fault(tf_fault_bp), 0x1238);
    EQ(veh_code, EXCEPTION_BREAKPOINT);
    if (veh_addr != (ULONG_PTR)tf_fault_bp_ins) t_note("breakpoint ExceptionAddress %llx (int3 at %p)", (unsigned long long)veh_addr, tf_fault_bp_ins);
}
static DWORD WINAPI th_raise_custom(LPVOID p) {
    RaiseException(0xE0000042, 0, 0, NULL);
    return 0x55;
}
TEST(exc_custom_exception_in_16_threads) {
    REQUIRE(U(CreateThread));
    PVOID h = AddVectoredExceptionHandler(1, rec_veh);
    veh_hits = 0; memset((void *)veh_tid, 0, sizeof veh_tid);
    enum { N = 16 };
    HANDLE hs[N]; DWORD tids[N];
    for (int i = 0; i < N; i++) hs[i] = U(CreateThread)(NULL, 0, th_raise_custom, NULL, 0, &tids[i]);
    int bad = 0;
    for (int i = 0; i < N; i++) if (tf_join(hs[i]) != 0x55) bad++;
    RemoveVectoredExceptionHandler(h);
    EQ(bad, 0);
    EQ(veh_hits, N);
    int found = 0;
    for (int i = 0; i < N; i++) { int f = 0; for (int j = 0; j < N; j++) if (veh_tid[j] == tids[i]) f = 1; found += f; }
    EQ(found, N);
}
static DWORD WINAPI th_raise_args(LPVOID p) {
    ULONG_PTR a[3] = { 1, 2, 3 };
    RaiseException(0xE0000043, 0, 3, a);
    return 0;
}
static volatile LONG args_n; static volatile ULONG_PTR args_seen[3]; static volatile DWORD args_np, args_flags;
static LONG CALLBACK args_veh(PEXCEPTION_POINTERS ep) {
    if (ep->ExceptionRecord->ExceptionCode != 0xE0000043) return EXCEPTION_CONTINUE_SEARCH;
    args_np = ep->ExceptionRecord->NumberParameters; args_flags = ep->ExceptionRecord->ExceptionFlags;
    for (int i = 0; i < 3; i++) args_seen[i] = ep->ExceptionRecord->ExceptionInformation[i];
    args_n++;
    return EXCEPTION_CONTINUE_EXECUTION;
}
TEST(exc_raise_exception_arguments_in_thread) {
    REQUIRE(U(CreateThread));
    PVOID h = AddVectoredExceptionHandler(1, args_veh);
    args_n = 0;
    EQ(tf_run(th_raise_args, NULL), 0);
    RemoveVectoredExceptionHandler(h);
    EQ(args_n, 1);
    EQ(args_np, 3);
    EQ(args_seen[0], 1); EQ(args_seen[1], 2); EQ(args_seen[2], 3);
    EQ(args_flags & EXCEPTION_NONCONTINUABLE, 0);
}
