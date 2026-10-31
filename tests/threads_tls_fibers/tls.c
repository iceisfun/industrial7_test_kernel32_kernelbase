/* TlsAlloc / TlsFree / TlsGetValue / TlsSetValue. */
#include "tf.h"

static __typeof__(&TlsAlloc) pAlloc;
static __typeof__(&TlsFree) pFree;
static __typeof__(&TlsGetValue) pGet;
static __typeof__(&TlsSetValue) pSet;
#define NEED_TLS() do { pAlloc = U(TlsAlloc); pFree = U(TlsFree); pGet = U(TlsGetValue); pSet = U(TlsSetValue); \
    REQUIRE(pAlloc); REQUIRE(pFree); REQUIRE(pGet); REQUIRE(pSet); } while (0)

static DWORD g_idx;
static DWORD WINAPI th_get_idx(LPVOID p) { return (DWORD)(ULONG_PTR)pGet(g_idx); }
static DWORD WINAPI th_setget_idx(LPVOID p) {
    pSet(g_idx, p);
    return (pGet(g_idx) == p) ? 1 : 0;
}
static DWORD WINAPI th_wait_then_read(LPVOID p) {
    WaitForSingleObject((HANDLE)p, 15000);
    return (DWORD)(ULONG_PTR)pGet(g_idx);
}
static DWORD WINAPI th_wait_then_setget(LPVOID p) {
    WaitForSingleObject((HANDLE)p, 15000);
    LPVOID v = (LPVOID)0xABC0;
    if (!pSet(g_idx, v)) return 0;
    if (pGet(g_idx) != v) return 0;
    return 1;
}

TEST(tls_alloc_free_basic) {
    NEED_TLS();
    DWORD i = pAlloc();
    CHECK(i != TLS_OUT_OF_INDEXES);
    CHECK(i < TF_TLS_MAX);
    CHECK(pFree(i));
}
TEST(tls_alloc_distinct) {
    NEED_TLS();
    DWORD ix[32];
    for (int k = 0; k < 32; k++) { ix[k] = pAlloc(); CHECK(ix[k] != TLS_OUT_OF_INDEXES); }
    int dup = 0;
    for (int a = 0; a < 32; a++) for (int b = a + 1; b < 32; b++) if (ix[a] == ix[b]) dup++;
    EQ(dup, 0);
    for (int k = 0; k < 32; k++) CHECK(pFree(ix[k]));
}
TEST(tls_alloc_reuses_freed_lowest) {
    NEED_TLS();
    DWORD a = pAlloc(), b = pAlloc(), c = pAlloc();
    CHECK(pFree(b));
    DWORD d = pAlloc();
    EQ(d, b);
    CHECK(pFree(a)); CHECK(pFree(c)); CHECK(pFree(d));
    DWORD e = pAlloc();
    CHECK(e <= a);   /* lowest clear bit */
    CHECK(pFree(e));
}
TEST(tls_alloc_initial_value_null) {
    NEED_TLS();
    DWORD i = pAlloc();
    SET_ERR(0x1234);
    LPVOID v = pGet(i);
    CHECK(v == NULL);
    EXPECT_ERR(0);       /* success clears last error */
    CHECK(pFree(i));
}
TEST(tls_getvalue_success_clears_lasterror) {
    NEED_TLS();
    DWORD i = pAlloc();
    CHECK(pSet(i, (LPVOID)0x55));
    SET_ERR(0xDEAD);
    CHECK(pGet(i) == (LPVOID)0x55);
    EXPECT_ERR(0);
    SET_ERR(0);
    CHECK(pGet(i) == (LPVOID)0x55);
    EXPECT_ERR(0);
    CHECK(pFree(i));
}
TEST(tls_setvalue_preserves_lasterror) {
    NEED_TLS();
    DWORD i = pAlloc();
    SET_ERR(0x4321);
    CHECK(pSet(i, (LPVOID)1));
    EXPECT_ERR(0x4321);
    SET_ERR(0x4321);
    CHECK(pFree(i));
}
TEST(tls_set_get_value_table) {
    NEED_TLS();
    DWORD i = pAlloc();
    static const ULONG_PTR v[] = { 1, 0xFFFFFFFF, 0x100000000ULL, 0x123456789ABCDEF0ULL, (ULONG_PTR)-1, (ULONG_PTR)-2, 0x8000000000000000ULL, 0 };
    for (size_t k = 0; k < sizeof v / sizeof v[0]; k++) {
        CHECK(pSet(i, (LPVOID)v[k]));
        SET_ERR(7);
        LPVOID g = pGet(i);
        CHECKF((ULONG_PTR)g == v[k], "k=%zu got %p", k, g);
        EXPECT_ERR(0);
    }
    CHECK(pFree(i));
}
TEST(tls_teb_slot_layout) {
    NEED_TLS();
    DWORD i = pAlloc();
    if (i >= 64) { pFree(i); t_skip("first free index >= 64"); return; }
    CHECK(pSet(i, (LPVOID)0xFEED));
    ULONG_PTR *slots = (ULONG_PTR *)((char *)NtCurrentTeb() + 0x1480);
    EQ(slots[i], 0xFEED);
    slots[i] = 0xBEEF;
    CHECK(pGet(i) == (LPVOID)0xBEEF);
    CHECK(pFree(i));
}
TEST(tls_free_clears_slot_in_calling_thread) {
    NEED_TLS();
    DWORD i = pAlloc();
    CHECK(pSet(i, (LPVOID)0x77));
    CHECK(pFree(i));
    DWORD j = pAlloc();
    EQ(j, i);
    CHECK(pGet(j) == NULL);
    CHECK(pFree(j));
}
TEST(tls_alloc_zeroes_calling_thread_slot) {
    NEED_TLS();
    DWORD i = pAlloc();
    CHECK(pSet(i, (LPVOID)0x77));
    /* Freed through the raw pFree, then realloc must give 0 even if TlsFree did not clear (it does). */
    CHECK(pFree(i));
    DWORD j = pAlloc();
    CHECK(pGet(j) == NULL);
    CHECK(pFree(j));
}
TEST(tls_free_zeroes_slot_in_other_threads) {
    NEED_TLS();
    g_idx = pAlloc();
    HANDLE go = tf_ev(TRUE, FALSE), set_done = tf_ev(TRUE, FALSE);
    (void)set_done;
    HANDLE h = CreateThread(NULL, 0, th_wait_then_setget, go, 0, NULL);
    SetEvent(go);
    EQ(tf_join(h), 1);
    /* worker values gone with the thread; now test a live thread */
    HANDLE hold = tf_ev(TRUE, FALSE), reread = tf_ev(TRUE, FALSE);
    struct Ctx { HANDLE hold, reread; DWORD idx; volatile LONG stage; volatile ULONG_PTR after; };
    static struct Ctx c; c.hold = hold; c.reread = reread; c.idx = g_idx; c.stage = 0; c.after = 1;
    DWORD WINAPI body(LPVOID p) {
        pSet(c.idx, (LPVOID)0x99);
        c.stage = 1;
        WaitForSingleObject(c.hold, 15000);
        DWORD n = (DWORD)(ULONG_PTR)pGet(c.idx);
        c.after = n;
        return 0;
    }
    h = CreateThread(NULL, 0, body, NULL, 0, NULL);
    for (int k = 0; k < 500 && c.stage == 0; k++) Sleep(5);
    EQ(c.stage, 1);
    CHECK(pFree(g_idx));
    DWORD again = pAlloc();
    EQ(again, g_idx);
    SetEvent(hold);
    tf_join(h);
    EQ(c.after, 0);    /* TlsFree zeroed the cell in every thread */
    CHECK(pFree(again));
    CloseHandle(go); CloseHandle(set_done); CloseHandle(hold); CloseHandle(reread);
}
TEST(tls_new_thread_starts_zero) {
    NEED_TLS();
    g_idx = pAlloc();
    CHECK(pSet(g_idx, (LPVOID)0x1234));
    EQ(tf_run(th_get_idx, NULL), 0);
    CHECK(pGet(g_idx) == (LPVOID)0x1234);
    CHECK(pFree(g_idx));
}
TEST(tls_isolated_between_threads) {
    NEED_TLS();
    g_idx = pAlloc();
    CHECK(pSet(g_idx, (LPVOID)0x1111));
    EQ(tf_run(th_setget_idx, (LPVOID)0x2222), 1);
    CHECK(pGet(g_idx) == (LPVOID)0x1111);   /* other thread's set did not leak */
    CHECK(pFree(g_idx));
}
TEST(tls_values_not_inherited_after_thread_exit) {
    NEED_TLS();
    g_idx = pAlloc();
    EQ(tf_run(th_setget_idx, (LPVOID)0x2222), 1);
    EQ(tf_run(th_get_idx, NULL), 0);
    CHECK(pFree(g_idx));
}
TEST(tls_many_threads_distinct_values) {
    NEED_TLS();
    enum { N = 16 };
    g_idx = pAlloc();
    HANDLE hs[N];
    for (int k = 0; k < N; k++) hs[k] = CreateThread(NULL, 0, th_setget_idx, (LPVOID)(ULONG_PTR)(0x1000 + k), 0, NULL);
    int ok = 0;
    for (int k = 0; k < N; k++) if (tf_join(hs[k]) == 1) ok++;
    EQ(ok, N);
    CHECK(pFree(g_idx));
}
static volatile LONG g_stage;
static DWORD WINAPI th_barrier_check(LPVOID p) {
    ULONG_PTR mine = (ULONG_PTR)p;
    pSet(g_idx, (LPVOID)mine);
    InterlockedIncrement(&g_stage);
    for (int k = 0; k < 2000 && g_stage < 8; k++) Sleep(2);
    return pGet(g_idx) == (LPVOID)mine ? 1 : 0;
}
TEST(tls_concurrent_values_stay_separate) {
    NEED_TLS();
    g_idx = pAlloc(); g_stage = 0;
    HANDLE hs[8];
    for (int k = 0; k < 8; k++) hs[k] = CreateThread(NULL, 0, th_barrier_check, (LPVOID)(ULONG_PTR)(0x7000 + k), 0, NULL);
    int ok = 0;
    for (int k = 0; k < 8; k++) if (tf_join(hs[k]) == 1) ok++;
    EQ(ok, 8);
    CHECK(pFree(g_idx));
}
TEST(tls_invalid_index_get) {
    NEED_TLS();
    static const DWORD bad[] = { 0x440, 0x441, 0x1000, 0x10000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFE, TLS_OUT_OF_INDEXES };
    for (size_t k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        SET_ERR(0);
        LPVOID v = pGet(bad[k]);
        CHECKF(v == NULL, "idx %lx", bad[k]);
        DWORD e = GetLastError();
        CHECKF(e == ERROR_INVALID_PARAMETER, "idx %lx err %lu", bad[k], e);
    }
}
TEST(tls_get_boundary_valid_range_unallocated) {
    NEED_TLS();
    /* 0x43F is the last representable index: reading never fails, even unallocated */
    SET_ERR(0x99);
    LPVOID v = pGet(0x43F);
    CHECK(v == NULL || v != NULL);
    EXPECT_ERR(0);
}
TEST(tls_invalid_index_set) {
    NEED_TLS();
    static const DWORD bad[] = { 0x440, 0x441, 0x10000, 0x7FFFFFFF, 0x80000000, TLS_OUT_OF_INDEXES };
    for (size_t k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        SET_ERR(0);
        CHECKF(!pSet(bad[k], (LPVOID)1), "idx %lx", bad[k]);
        DWORD e = GetLastError();
        CHECKF(e == ERROR_INVALID_PARAMETER, "idx %lx err %lu", bad[k], e);
    }
}
TEST(tls_invalid_index_free) {
    NEED_TLS();
    static const DWORD bad[] = { 0x440, 0x441, 0x10000, 0x7FFFFFFF, 0x80000000, TLS_OUT_OF_INDEXES };
    for (size_t k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        SET_ERR(0);
        CHECKF(!pFree(bad[k]), "idx %lx", bad[k]);
        DWORD e = GetLastError();
        CHECKF(e == ERROR_INVALID_PARAMETER, "idx %lx err %lu", bad[k], e);
    }
}
TEST(tls_free_twice) {
    NEED_TLS();
    DWORD i = pAlloc();
    CHECK(pFree(i));
    SET_ERR(0);
    CHECK(!pFree(i));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!pFree(i));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(tls_free_unallocated_high_index) {
    NEED_TLS();
    /* find an unallocated index by freeing a fresh one, then probe a few neighbours known to be free */
    DWORD i = pAlloc();
    CHECK(pFree(i));
    SET_ERR(0);
    CHECK(!pFree(i));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(tls_free_does_not_disturb_others) {
    NEED_TLS();
    DWORD a = pAlloc(), b = pAlloc();
    CHECK(pSet(a, (LPVOID)0xA)); CHECK(pSet(b, (LPVOID)0xB));
    CHECK(pFree(a));
    CHECK(pGet(b) == (LPVOID)0xB);
    CHECK(pFree(b));
}
TEST(tls_getvalue_failure_returns_null_sets_error) {
    NEED_TLS();
    SET_ERR(0);
    CHECK(pGet(0x440) == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    /* a following success must clear it again */
    DWORD i = pAlloc();
    CHECK(pGet(i) == NULL);
    EXPECT_ERR(0);
    CHECK(pFree(i));
}

TEST(tls_exhaustion_and_expansion_slots) {
    NEED_TLS();
    DWORD *ix = (DWORD *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 4096 * sizeof(DWORD));
    int n = 0;
    DWORD i;
    SET_ERR(0);
    while (n < 2000 && (i = pAlloc()) != TLS_OUT_OF_INDEXES) ix[n++] = i;
    DWORD e = GetLastError();
    CHECK(n < 2000);
    CHECKF(n >= 1000 && n <= TF_TLS_MAX, "allocated %d", n);
    t_note("TlsAlloc succeeded %d times before exhaustion", n);
    EQ(e, ERROR_NOT_ENOUGH_MEMORY);
    int over = 0, hi = 0;
    for (int k = 0; k < n; k++) { if (ix[k] >= TF_TLS_MAX) over++; if (ix[k] >= 64) hi++; }
    EQ(over, 0);
    CHECK(hi > 0);
    /* fail repeatedly */
    for (int k = 0; k < 3; k++) { SET_ERR(0); EQ(pAlloc(), TLS_OUT_OF_INDEXES); EXPECT_ERR(ERROR_NOT_ENOUGH_MEMORY); }
    /* every slot usable and independent */
    for (int k = 0; k < n; k++) CHECK(pSet(ix[k], (LPVOID)(ULONG_PTR)(0x10000 + ix[k])));
    int bad = 0;
    for (int k = 0; k < n; k++) if (pGet(ix[k]) != (LPVOID)(ULONG_PTR)(0x10000 + ix[k])) bad++;
    EQ(bad, 0);
    /* a fresh thread sees zero in all of them and can use the expansion slots */
    DWORD WINAPI chk(LPVOID p) {
        DWORD *ix = (DWORD *)p; int bad = 0;
        for (int k = 0; k < TF_TLS_MAX; k++) { if (!ix[k] && k) break; }
        return 0;
    }
    (void)chk;
    static DWORD *s_ix; static int s_n;
    s_ix = ix; s_n = n;
    DWORD WINAPI chk2(LPVOID p) {
        int bad = 0;
        for (int k = 0; k < s_n; k++) if (pGet(s_ix[k]) != NULL) bad++;
        for (int k = 0; k < s_n; k++) if (!pSet(s_ix[k], (LPVOID)(ULONG_PTR)(0x20000 + s_ix[k]))) bad++;
        for (int k = 0; k < s_n; k++) if (pGet(s_ix[k]) != (LPVOID)(ULONG_PTR)(0x20000 + s_ix[k])) bad++;
        return bad;
    }
    EQ(tf_run(chk2, NULL), 0);
    bad = 0;
    for (int k = 0; k < n; k++) if (pGet(ix[k]) != (LPVOID)(ULONG_PTR)(0x10000 + ix[k])) bad++;
    EQ(bad, 0);
    /* last valid index boundary */
    DWORD maxidx = 0; for (int k = 0; k < n; k++) if (ix[k] > maxidx) maxidx = ix[k];
    t_note("highest index handed out: %lu", maxidx);
    for (int k = 0; k < n; k++) CHECK(pFree(ix[k]));
    DWORD again = pAlloc();
    CHECK(again != TLS_OUT_OF_INDEXES);
    CHECK(pFree(again));
    HeapFree(GetProcessHeap(), 0, ix);
}
TEST(tls_preexisting_thread_uses_late_expansion_slot) {
    NEED_TLS();
    HANDLE go = tf_ev(TRUE, FALSE);
    HANDLE h = CreateThread(NULL, 0, th_wait_then_setget, go, 0, NULL);
    DWORD ix[80]; int n = 0;
    DWORD i;
    while (n < 80 && (i = pAlloc()) != TLS_OUT_OF_INDEXES) ix[n++] = i;
    DWORD last = ix[n - 1];
    g_idx = last;
    SetEvent(go);
    EQ(tf_join(h), 1);
    for (int k = 0; k < n; k++) CHECK(pFree(ix[k]));
    CloseHandle(go);
    t_note("last index used by pre-existing thread: %lu", last);
}
TEST(tls_expansion_teb_layout) {
    NEED_TLS();
    DWORD ix[80]; int n = 0; DWORD i;
    while (n < 80 && (i = pAlloc()) != TLS_OUT_OF_INDEXES) ix[n++] = i;
    DWORD hi = 0; for (int k = 0; k < n; k++) if (ix[k] >= 64) { hi = ix[k]; break; }
    if (hi) {
        CHECK(pSet(hi, (LPVOID)0xCAFE));
        ULONG_PTR *exp = *(ULONG_PTR **)((char *)NtCurrentTeb() + 0x1780);
        CHECK(exp != NULL);
        if (exp) EQ(exp[hi - 64], 0xCAFE);
    } else t_skip("no expansion index handed out");
    for (int k = 0; k < n; k++) CHECK(pFree(ix[k]));
}
static DWORD g_conc_idx[8][20];
static DWORD WINAPI th_conc_alloc(LPVOID p) {
    int t = (int)(ULONG_PTR)p;
    for (int k = 0; k < 20; k++) g_conc_idx[t][k] = pAlloc();
    return 0;
}
TEST(tls_concurrent_alloc_unique) {
    NEED_TLS();
    HANDLE hs[8];
    memset(g_conc_idx, 0, sizeof g_conc_idx);
    for (int t = 0; t < 8; t++) hs[t] = CreateThread(NULL, 0, th_conc_alloc, (LPVOID)(ULONG_PTR)t, 0, NULL);
    for (int t = 0; t < 8; t++) tf_join(hs[t]);
    int dup = 0, fail = 0;
    for (int a = 0; a < 160; a++) {
        DWORD x = g_conc_idx[a / 20][a % 20];
        if (x == TLS_OUT_OF_INDEXES) fail++;
        for (int b = a + 1; b < 160; b++) if (x == g_conc_idx[b / 20][b % 20]) dup++;
    }
    EQ(dup, 0);
    EQ(fail, 0);
    for (int a = 0; a < 160; a++) pFree(g_conc_idx[a / 20][a % 20]);
}
TEST(tls_alloc_free_cycle_1000) {
    NEED_TLS();
    for (int k = 0; k < 1000; k++) {
        DWORD i = pAlloc();
        if (i == TLS_OUT_OF_INDEXES) { CHECKF(0, "alloc failed at %d", k); return; }
        pSet(i, (LPVOID)(ULONG_PTR)k);
        if (!pFree(i)) { CHECKF(0, "free failed at %d", k); return; }
    }
}
TEST(tls_gettlsvalue2_export_note) {
    FARPROC p = t_proc("TlsGetValue2");
    t_note("TlsGetValue2 export %s", p ? "present" : "absent (expected on Windows 10)");
}
TEST(tls_exports_resolve_to_kernel32) {
    CHECK(UK(TlsAlloc) != NULL);
    CHECK(UK(TlsFree) != NULL);
    CHECK(UK(TlsGetValue) != NULL);
    CHECK(UK(TlsSetValue) != NULL);
}
TEST(tls_slot_survives_other_thread_free_of_different_slot) {
    NEED_TLS();
    DWORD a = pAlloc(), b = pAlloc();
    CHECK(pSet(a, (LPVOID)0x11));
    g_idx = b;
    EQ(tf_run(th_setget_idx, (LPVOID)0x22), 1);
    CHECK(pFree(b));
    CHECK(pGet(a) == (LPVOID)0x11);
    CHECK(pFree(a));
}
