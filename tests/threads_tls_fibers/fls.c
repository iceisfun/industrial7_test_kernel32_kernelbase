/* FlsAlloc / FlsFree / FlsGetValue / FlsSetValue. */
#include "tf.h"

static __typeof__(&FlsAlloc) fAlloc;
static __typeof__(&FlsFree) fFree;
static __typeof__(&FlsGetValue) fGet;
static __typeof__(&FlsSetValue) fSet;
#define NEED_FLS() do { fAlloc = U(FlsAlloc); fFree = U(FlsFree); fGet = U(FlsGetValue); fSet = U(FlsSetValue); \
    REQUIRE(fAlloc); REQUIRE(fFree); REQUIRE(fGet); REQUIRE(fSet); } while (0)

static volatile LONG cb_count;
static volatile ULONG_PTR cb_last;
static volatile DWORD cb_last_tid;
static volatile ULONG_PTR cb_sum;
static VOID WINAPI cb_record(PVOID p) {
    InterlockedIncrement(&cb_count);
    cb_last = (ULONG_PTR)p;
    cb_last_tid = GetCurrentThreadId();
    InterlockedExchangeAdd64((volatile LONG64 *)&cb_sum, (LONG64)(ULONG_PTR)p);
}
static void cb_reset(void) { cb_count = 0; cb_last = 0; cb_last_tid = 0; cb_sum = 0; }

static DWORD g_fidx;
static DWORD WINAPI th_fls_get(LPVOID p) { return (DWORD)(ULONG_PTR)fGet(g_fidx); }
static DWORD WINAPI th_fls_setget(LPVOID p) { fSet(g_fidx, p); return fGet(g_fidx) == p ? 1 : 0; }
static DWORD WINAPI th_fls_set_tid(LPVOID p) { fSet(g_fidx, p); return GetCurrentThreadId(); }

TEST(fls_alloc_free_basic) {
    NEED_FLS();
    DWORD i = fAlloc(NULL);
    CHECK(i != TF_FLS_OUT);
    CHECK(i < 4080);
    CHECK(fFree(i));
}
TEST(fls_alloc_distinct) {
    NEED_FLS();
    DWORD ix[32];
    for (int k = 0; k < 32; k++) { ix[k] = fAlloc(NULL); CHECK(ix[k] != TF_FLS_OUT); }
    int dup = 0;
    for (int a = 0; a < 32; a++) for (int b = a + 1; b < 32; b++) if (ix[a] == ix[b]) dup++;
    EQ(dup, 0);
    for (int k = 0; k < 32; k++) CHECK(fFree(ix[k]));
}
TEST(fls_initial_value_null_and_lasterror) {
    NEED_FLS();
    DWORD i = fAlloc(NULL);
    SET_ERR(0x1234);
    CHECK(fGet(i) == NULL);
    EXPECT_ERR(0);
    CHECK(fFree(i));
}
TEST(fls_set_get_roundtrip_table) {
    NEED_FLS();
    DWORD i = fAlloc(NULL);
    static const ULONG_PTR v[] = { 1, 0xFFFFFFFF, 0x100000000ULL, 0x123456789ABCDEF0ULL, (ULONG_PTR)-1, 0 };
    for (size_t k = 0; k < sizeof v / sizeof v[0]; k++) {
        CHECK(fSet(i, (PVOID)v[k]));
        SET_ERR(9);
        PVOID g = fGet(i);
        CHECKF((ULONG_PTR)g == v[k], "k=%zu", k);
        EXPECT_ERR(0);
    }
    CHECK(fFree(i));
}
TEST(fls_setvalue_preserves_lasterror) {
    NEED_FLS();
    DWORD i = fAlloc(NULL);
    SET_ERR(0x4321);
    CHECK(fSet(i, (PVOID)5));
    EXPECT_ERR(0x4321);
    CHECK(fFree(i));
}
TEST(fls_thread_isolation) {
    NEED_FLS();
    g_fidx = fAlloc(NULL);
    CHECK(fSet(g_fidx, (PVOID)0x1111));
    EQ(tf_run(th_fls_setget, (LPVOID)0x2222), 1);
    CHECK(fGet(g_fidx) == (PVOID)0x1111);
    EQ(tf_run(th_fls_get, NULL), 0);   /* new thread: slot is NULL */
    CHECK(fFree(g_fidx));
}
TEST(fls_new_thread_value_null) {
    NEED_FLS();
    g_fidx = fAlloc(NULL);
    CHECK(fSet(g_fidx, (PVOID)0x55));
    EQ(tf_run(th_fls_get, NULL), 0);
    CHECK(fFree(g_fidx));
}
TEST(fls_invalid_get) {
    NEED_FLS();
    static const DWORD bad[] = { 0x1000, 0x10000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFE, TF_FLS_OUT };
    for (size_t k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        SET_ERR(0);
        CHECKF(fGet(bad[k]) == NULL, "idx %lx", bad[k]);
        DWORD e = GetLastError();
        CHECKF(e == ERROR_INVALID_PARAMETER, "idx %lx err %lu", bad[k], e);
    }
}
TEST(fls_invalid_set) {
    NEED_FLS();
    static const DWORD bad[] = { 0x1000, 0x10000, 0x7FFFFFFF, 0x80000000, TF_FLS_OUT };
    for (size_t k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        SET_ERR(0);
        CHECKF(!fSet(bad[k], (PVOID)1), "idx %lx", bad[k]);
        DWORD e = GetLastError();
        CHECKF(e == ERROR_INVALID_PARAMETER, "idx %lx err %lu", bad[k], e);
    }
}
TEST(fls_invalid_free) {
    NEED_FLS();
    static const DWORD bad[] = { 0x1000, 0x10000, 0x7FFFFFFF, 0x80000000, TF_FLS_OUT };
    for (size_t k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        SET_ERR(0);
        CHECKF(!fFree(bad[k]), "idx %lx", bad[k]);
        DWORD e = GetLastError();
        CHECKF(e == ERROR_INVALID_PARAMETER, "idx %lx err %lu", bad[k], e);
    }
}
TEST(fls_use_after_free) {
    NEED_FLS();
    DWORD i = fAlloc(NULL);
    CHECK(fSet(i, (PVOID)1));
    CHECK(fFree(i));
    SET_ERR(0);
    PVOID g = fGet(i);
    t_note("FlsGetValue(freed index): %p err %lu", g, GetLastError());
    SET_ERR(0);
    BOOL r = fSet(i, (PVOID)2);
    t_note("FlsSetValue(freed index): %d err %lu", r, GetLastError());
    SET_ERR(0);
    CHECK(!fFree(i));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(fls_realloc_value_cleared) {
    NEED_FLS();
    DWORD i = fAlloc(NULL);
    CHECK(fSet(i, (PVOID)0x1234));
    CHECK(fFree(i));
    DWORD j = fAlloc(NULL);
    CHECK(fGet(j) == NULL);
    CHECK(fFree(j));
}
TEST(fls_callback_on_thread_exit) {
    NEED_FLS();
    cb_reset();
    g_fidx = fAlloc(cb_record);
    DWORD tid = tf_run(th_fls_set_tid, (LPVOID)0x77);
    EQ(cb_count, 1);
    EQ(cb_last, 0x77);
    EQ(cb_last_tid, tid);      /* runs in the exiting thread */
    CHECK(fFree(g_fidx));
    EQ(cb_count, 1);           /* nothing left to free in other threads */
}
TEST(fls_callback_not_called_for_null_value) {
    NEED_FLS();
    cb_reset();
    g_fidx = fAlloc(cb_record);
    EQ(tf_run(th_fls_get, NULL), 0);                    /* never set */
    EQ(cb_count, 0);
    DWORD WINAPI set_null(LPVOID p) { fSet(g_fidx, (PVOID)5); fSet(g_fidx, NULL); return 0; }
    tf_run(set_null, NULL);
    EQ(cb_count, 0);
    CHECK(fFree(g_fidx));
    EQ(cb_count, 0);
}
TEST(fls_callback_on_flsfree_calling_thread) {
    NEED_FLS();
    cb_reset();
    DWORD i = fAlloc(cb_record);
    CHECK(fSet(i, (PVOID)0x99));
    CHECK(fFree(i));
    EQ(cb_count, 1);
    EQ(cb_last, 0x99);
    EQ(cb_last_tid, GetCurrentThreadId());
}
TEST(fls_callback_not_called_on_overwrite) {
    NEED_FLS();
    cb_reset();
    DWORD i = fAlloc(cb_record);
    CHECK(fSet(i, (PVOID)1));
    CHECK(fSet(i, (PVOID)2));
    CHECK(fSet(i, (PVOID)3));
    EQ(cb_count, 0);
    CHECK(fFree(i));
    EQ(cb_count, 1);
    EQ(cb_last, 3);
}
TEST(fls_callback_not_called_on_flsfree_without_value) {
    NEED_FLS();
    cb_reset();
    DWORD i = fAlloc(cb_record);
    CHECK(fFree(i));
    EQ(cb_count, 0);
}
TEST(fls_callback_each_thread) {
    NEED_FLS();
    cb_reset();
    g_fidx = fAlloc(cb_record);
    enum { N = 24 };
    HANDLE hs[N];
    for (int k = 0; k < N; k++) hs[k] = CreateThread(NULL, 0, th_fls_set_tid, (LPVOID)(ULONG_PTR)(k + 1), 0, NULL);
    for (int k = 0; k < N; k++) tf_join(hs[k]);
    EQ(cb_count, N);
    EQ(cb_sum, (N * (N + 1)) / 2);
    CHECK(fFree(g_fidx));
}
TEST(fls_callback_multiple_slots_per_thread) {
    NEED_FLS();
    cb_reset();
    static DWORD ix[5];
    for (int k = 0; k < 5; k++) ix[k] = fAlloc(cb_record);
    DWORD WINAPI setall(LPVOID p) { for (int k = 0; k < 5; k++) fSet(ix[k], (PVOID)(ULONG_PTR)(10 + k)); return 0; }
    tf_run(setall, NULL);
    EQ(cb_count, 5);
    EQ(cb_sum, 10 + 11 + 12 + 13 + 14);
    for (int k = 0; k < 5; k++) CHECK(fFree(ix[k]));
}
TEST(fls_flsfree_callback_for_other_live_thread) {
    NEED_FLS();
    cb_reset();
    g_fidx = fAlloc(cb_record);
    HANDLE hold = tf_ev(TRUE, FALSE), ready = tf_ev(TRUE, FALSE);
    static HANDLE s_hold, s_ready;
    s_hold = hold; s_ready = ready;
    DWORD WINAPI body(LPVOID p) { fSet(g_fidx, (PVOID)0x4242); SetEvent(s_ready); WaitForSingleObject(s_hold, 15000); return 0; }
    HANDLE h = CreateThread(NULL, 0, body, NULL, 0, NULL);
    WaitForSingleObject(ready, 10000);
    CHECK(fFree(g_fidx));
    t_note("FlsFree with live-thread value: callbacks=%ld last=%llx tid==main:%d", cb_count, (unsigned long long)cb_last, cb_last_tid == GetCurrentThreadId());
    LONG after_free = cb_count;
    SetEvent(hold);
    tf_join(h);
    if (after_free == 0) EQ(cb_count, 0);   /* slot is gone: no callback at thread exit either */
    CloseHandle(hold); CloseHandle(ready);
}
TEST(fls_callback_runs_with_same_value_after_free) {
    NEED_FLS();
    cb_reset();
    DWORD i = fAlloc(cb_record);
    CHECK(fSet(i, (PVOID)0xABCD));
    CHECK(fFree(i));
    EQ(cb_last, 0xABCD);
    /* callback registration removed with the slot: a new alloc has no callback unless given */
    DWORD j = fAlloc(NULL);
    CHECK(fSet(j, (PVOID)0x1));
    CHECK(fFree(j));
    EQ(cb_count, 1);
}
TEST(fls_alloc_null_callback_no_call) {
    NEED_FLS();
    cb_reset();
    g_fidx = fAlloc(NULL);
    tf_run(th_fls_set_tid, (LPVOID)5);
    CHECK(fFree(g_fidx));
    EQ(cb_count, 0);
}
TEST(fls_exhaustion) {
    NEED_FLS();
    DWORD *ix = (DWORD *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 8192 * sizeof(DWORD));
    int n = 0; DWORD i;
    SET_ERR(0);
    while (n < 8000 && (i = fAlloc(NULL)) != TF_FLS_OUT) ix[n++] = i;
    DWORD e = GetLastError();
    CHECK(n < 8000);
    CHECKF(n >= 128, "only %d slots", n);
    t_note("FlsAlloc succeeded %d times; failing err %lu", n, e);
    CHECK(e != 0);
    int over = 0; for (int k = 0; k < n; k++) if (ix[k] >= 4080) over++;
    EQ(over, 0);
    for (int k = 0; k < 3; k++) { SET_ERR(0); EQ(fAlloc(NULL), TF_FLS_OUT); CHECK(GetLastError() != 0); }
    /* use first, middle and last */
    int pick[3] = { 0, n / 2, n - 1 };
    for (int k = 0; k < 3; k++) {
        CHECK(fSet(ix[pick[k]], (PVOID)(ULONG_PTR)(0x300 + k)));
        CHECK(fGet(ix[pick[k]]) == (PVOID)(ULONG_PTR)(0x300 + k));
    }
    for (int k = 0; k < n; k++) CHECK(fFree(ix[k]));
    DWORD again = fAlloc(NULL);
    CHECK(again != TF_FLS_OUT);
    CHECK(fFree(again));
    HeapFree(GetProcessHeap(), 0, ix);
}
TEST(fls_high_slot_values_in_thread) {
    NEED_FLS();
    DWORD *ix = (DWORD *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 512 * sizeof(DWORD));
    int n = 0; DWORD i;
    while (n < 300 && (i = fAlloc(NULL)) != TF_FLS_OUT) ix[n++] = i;
    static DWORD *s_ix; static int s_n;
    s_ix = ix; s_n = n;
    DWORD WINAPI body(LPVOID p) {
        int bad = 0;
        for (int k = 0; k < s_n; k++) if (fGet(s_ix[k]) != NULL) bad++;
        for (int k = 0; k < s_n; k++) if (!fSet(s_ix[k], (PVOID)(ULONG_PTR)(1000 + k))) bad++;
        for (int k = 0; k < s_n; k++) if (fGet(s_ix[k]) != (PVOID)(ULONG_PTR)(1000 + k)) bad++;
        return bad;
    }
    EQ(tf_run(body, NULL), 0);
    for (int k = 0; k < n; k++) CHECK(fFree(ix[k]));
    HeapFree(GetProcessHeap(), 0, ix);
}
TEST(fls_getvalue2_export_note) {
    FARPROC p = t_proc("FlsGetValue2");
    t_note("FlsGetValue2 export %s", p ? "present" : "absent (expected on Windows 10)");
}
TEST(fls_exports_resolve_to_kernel32) {
    CHECK(UK(FlsAlloc) != NULL);
    CHECK(UK(FlsFree) != NULL);
    CHECK(UK(FlsGetValue) != NULL);
    CHECK(UK(FlsSetValue) != NULL);
}
TEST(fls_alloc_free_cycle_500) {
    NEED_FLS();
    for (int k = 0; k < 500; k++) {
        DWORD i = fAlloc(NULL);
        if (i == TF_FLS_OUT) { CHECKF(0, "alloc failed at %d", k); return; }
        fSet(i, (PVOID)(ULONG_PTR)(k + 1));
        if (!fFree(i)) { CHECKF(0, "free failed at %d", k); return; }
    }
}
TEST(fls_independent_of_tls) {
    NEED_FLS();
    DWORD t = TlsAlloc(), f = fAlloc(NULL);
    TlsSetValue(t, (LPVOID)0x10);
    CHECK(fSet(f, (PVOID)0x20));
    CHECK(TlsGetValue(t) == (LPVOID)0x10);
    CHECK(fGet(f) == (PVOID)0x20);
    CHECK(fFree(f));
    TlsFree(t);
}
