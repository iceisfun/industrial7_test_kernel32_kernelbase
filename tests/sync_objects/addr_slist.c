#include "so.h"

/* ======================= WaitOnAddress ======================= */
#define NEED_WOA() do { REQUIRE(U(WaitOnAddress)); REQUIRE(U(WakeByAddressSingle)); REQUIRE(U(WakeByAddressAll)); } while (0)

TEST(woa_value_differs_returns_immediately) {
    NEED_WOA();
    union { UINT8 b; UINT16 w; UINT32 d; UINT64 q; } v, cmp;
    struct { int sz; } sizes[] = { {1}, {2}, {4}, {8} };
    for (int i = 0; i < 4; i++) {
        v.q = 0x1111111111111111ull; cmp.q = 0x2222222222222222ull;
        SET_ERR(0);
        DWORD t0 = GetTickCount();
        EXPECT_EQ(U(WaitOnAddress)(&v, &cmp, sizes[i].sz, 5000), TRUE);
        CHECK(GetTickCount() - t0 < 1000);
        v.q = 0x1111111111111111ull;
        EXPECT_EQ(U(WaitOnAddress)(&v, &cmp, sizes[i].sz, INFINITE), TRUE);
        EXPECT_EQ(U(WaitOnAddress)(&v, &cmp, sizes[i].sz, 0), TRUE);
    }
}
TEST(woa_equal_value_times_out) {
    NEED_WOA();
    union { UINT8 b; UINT16 w; UINT32 d; UINT64 q; } v, cmp;
    int sizes[] = { 1, 2, 4, 8 };
    for (int i = 0; i < 4; i++) {
        v.q = cmp.q = 0x0123456789ABCDEFull;
        SET_ERR(0);
        DWORD t0 = GetTickCount();
        EXPECT_EQ(U(WaitOnAddress)(&v, &cmp, sizes[i], 60), FALSE);
        EXPECT_ERR(ERROR_TIMEOUT);
        DWORD dt = GetTickCount() - t0;
        CHECKF(dt >= 40 && dt < 3000, "size %d elapsed %lu", sizes[i], dt);
        SET_ERR(0);
        EXPECT_EQ(U(WaitOnAddress)(&v, &cmp, sizes[i], 0), FALSE);
        EXPECT_ERR(ERROR_TIMEOUT);
    }
}
TEST(woa_only_compares_size_bytes) {
    NEED_WOA();
    UINT32 v = 0xAABBCC11, cmp = 0xAABBCC22;
    /* size 1 compares only the first byte: 0x11 vs 0x22 differ */
    EXPECT_EQ(U(WaitOnAddress)(&v, &cmp, 1, 30), TRUE);
    /* size 2 compares 0xCC11 vs 0xCC22: differ */
    EXPECT_EQ(U(WaitOnAddress)(&v, &cmp, 2, 30), TRUE);
    cmp = 0x11223311;     /* low byte equal, rest differs */
    SET_ERR(0);
    EXPECT_EQ(U(WaitOnAddress)(&v, &cmp, 1, 30), FALSE); EXPECT_ERR(ERROR_TIMEOUT);
    EXPECT_EQ(U(WaitOnAddress)(&v, &cmp, 4, 30), TRUE);
    cmp = 0xFFFFCC11;
    SET_ERR(0);
    EXPECT_EQ(U(WaitOnAddress)(&v, &cmp, 2, 30), FALSE); EXPECT_ERR(ERROR_TIMEOUT);
}
TEST(woa_invalid_sizes) {
    NEED_WOA();
    UINT64 v[4] = { 0 }, cmp[4] = { 0 };
    int bad[] = { 0, 3, 5, 6, 7, 9, 12, 16, 32, -1 };
    for (int i = 0; i < 10; i++) {
        SET_ERR(0);
        BOOL r = U(WaitOnAddress)(v, cmp, (SIZE_T)(LONG_PTR)bad[i], 10);
        CHECKF(!r, "size %d", bad[i]); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(woa_invalid_size_even_if_values_differ) {
    NEED_WOA();
    UINT64 v[2] = { 1, 1 }, cmp[2] = { 2, 2 };
    SET_ERR(0);
    BOOL r = U(WaitOnAddress)(v, cmp, 3, 10);
    t_note("size 3 with differing values -> %d err %lu", r, r ? 0 : GetLastError());
    SET_ERR(0);
    r = U(WaitOnAddress)(v, cmp, 16, 10);
    t_note("size 16 with differing values -> %d err %lu", r, r ? 0 : GetLastError());
}
static volatile LONG woa_val, woa_st, woa_woken;
static DWORD WINAPI woa_waiter(LPVOID p) {
    LONG undesired = 0;
    InterlockedIncrement(&woa_st);
    while (woa_val == undesired) {
        if (!U(WaitOnAddress)(&woa_val, &undesired, sizeof woa_val, 4000)) break;
    }
    if (woa_val != undesired) InterlockedIncrement(&woa_woken);
    return 0;
}
TEST(woa_wake_single_wakes_a_waiter) {
    NEED_WOA();
    woa_val = 0; woa_st = 0; woa_woken = 0;
    so_thr t = so_start(woa_waiter, NULL);
    so_spin_ge(&woa_st, 1, 3000); Sleep(150);
    woa_val = 1;
    U(WakeByAddressSingle)((PVOID)&woa_val);
    EXPECT_EQ(so_join(t, 6000), 0);
    EXPECT_EQ(woa_woken, 1);
}
TEST(woa_wake_all_wakes_every_waiter) {
    NEED_WOA();
    woa_val = 0; woa_st = 0; woa_woken = 0;
    so_thr t[5]; for (int i = 0; i < 5; i++) t[i] = so_start(woa_waiter, NULL);
    so_spin_ge(&woa_st, 5, 3000); Sleep(200);
    woa_val = 1;
    U(WakeByAddressAll)((PVOID)&woa_val);
    for (int i = 0; i < 5; i++) so_join(t[i], 6000);
    EXPECT_EQ(woa_woken, 5);
}
TEST(woa_wake_single_with_many_waiters_wakes_exactly_one) {
    NEED_WOA();
    static volatile LONG v, ready, returned;
    v = 0; ready = 0; returned = 0;
    DWORD WINAPI w(LPVOID p) {
        LONG undesired = 0; InterlockedIncrement(&ready);
        BOOL r = U(WaitOnAddress)(&v, &undesired, sizeof v, 1500);
        if (r) InterlockedIncrement(&returned);
        return 0;
    }
    so_thr t[4]; for (int i = 0; i < 4; i++) t[i] = so_start(w, NULL);
    so_spin_ge(&ready, 4, 3000); Sleep(200);
    U(WakeByAddressSingle)((PVOID)&v);       /* value unchanged: waiter may return from the kernel, but only one */
    Sleep(300);
    EXPECT_EQ(returned, 1);
    for (int i = 0; i < 4; i++) so_join(t[i], 6000);
    EXPECT_EQ(returned, 1);
}
TEST(woa_wake_different_address_does_not_wake) {
    NEED_WOA();
    static volatile LONG a, b, ready, returned;
    a = b = 0; ready = returned = 0;
    DWORD WINAPI w(LPVOID p) { LONG u = 0; InterlockedIncrement(&ready); if (U(WaitOnAddress)(&a, &u, sizeof a, 600)) InterlockedIncrement(&returned); return 0; }
    so_thr t = so_start(w, NULL);
    so_spin_ge(&ready, 1, 3000); Sleep(100);
    U(WakeByAddressAll)((PVOID)&b);
    Sleep(200);
    EXPECT_EQ(returned, 0);
    so_join(t, 5000);
    EXPECT_EQ(returned, 0);   /* timed out */
}
TEST(woa_wake_without_waiters) {
    NEED_WOA();
    LONG v = 0;
    for (int i = 0; i < 1000; i++) { U(WakeByAddressSingle)(&v); U(WakeByAddressAll)(&v); }
    CHECK(1);
}
TEST(woa_all_sizes_wake) {
    NEED_WOA();
    static union { UINT8 b; UINT16 w; UINT32 d; UINT64 q; } var;
    static volatile LONG ready, ok;
    int sizes[] = { 1, 2, 4, 8 };
    for (int k = 0; k < 4; k++) {
        var.q = 0; ready = ok = 0;
        static int sz; sz = sizes[k];
        DWORD WINAPI w(LPVOID p) {
            UINT64 und = 0; InterlockedIncrement(&ready);
            while (1) {
                UINT64 cur = 0; memcpy(&cur, &var, sz);
                if (cur != 0) { InterlockedIncrement(&ok); return 0; }
                if (!U(WaitOnAddress)(&var, &und, sz, 4000)) return 1;
            }
        }
        so_thr t = so_start(w, NULL);
        so_spin_ge(&ready, 1, 3000); Sleep(100);
        var.b = 1;
        U(WakeByAddressSingle)(&var);
        EXPECT_EQ(so_join(t, 6000), 0);
        EXPECT_EQ(ok, 1);
    }
}
TEST(woa_infinite_timeout_woken) {
    NEED_WOA();
    static volatile LONG v, st; v = st = 0;
    DWORD WINAPI w(LPVOID p) { LONG u = 0; InterlockedExchange(&st, 1); while (v == 0) U(WaitOnAddress)(&v, &u, sizeof v, INFINITE); InterlockedExchange(&st, 2); return 0; }
    so_thr t = so_start(w, NULL);
    so_spin_until(&st, 1, 3000); Sleep(100);
    v = 5; U(WakeByAddressSingle)((PVOID)&v);
    EXPECT_EQ(so_join(t, 6000), 0);
    EXPECT_EQ(st, 2);
}
TEST(woa_separate_compare_buffer_not_the_address) {
    NEED_WOA();
    LONG v = 7, cmp = 7;
    SET_ERR(0);
    EXPECT_EQ(U(WaitOnAddress)(&v, &cmp, sizeof v, 20), FALSE);
    EXPECT_ERR(ERROR_TIMEOUT);
    cmp = 8;
    EXPECT_EQ(U(WaitOnAddress)(&v, &cmp, sizeof v, 20), TRUE);
    EXPECT_EQ(cmp, 8); EXPECT_EQ(v, 7);   /* neither buffer modified */
}
TEST(woa_ping_pong_handshake) {
    NEED_WOA();
    static volatile LONG turn; static volatile LONG errs;
    turn = 0; errs = 0;
    DWORD WINAPI peer(LPVOID p) {
        for (int i = 0; i < 200; i++) {
            LONG u;
            while ((u = turn) != 1) { if (!U(WaitOnAddress)(&turn, &u, sizeof turn, 5000)) { InterlockedIncrement(&errs); return 1; } }
            turn = 0; U(WakeByAddressSingle)((PVOID)&turn);
        }
        return 0;
    }
    so_thr t = so_start(peer, NULL);
    for (int i = 0; i < 200; i++) {
        turn = 1; U(WakeByAddressSingle)((PVOID)&turn);
        LONG u;
        while ((u = turn) != 0) { if (!U(WaitOnAddress)(&turn, &u, sizeof turn, 5000)) { InterlockedIncrement(&errs); break; } }
    }
    EXPECT_EQ(so_join(t, 10000), 0);
    EXPECT_EQ(errs, 0);
}

/* ======================= synchronization barriers ======================= */
#define NEED_BAR() do { REQUIRE(U(InitializeSynchronizationBarrier)); REQUIRE(U(EnterSynchronizationBarrier)); REQUIRE(U(DeleteSynchronizationBarrier)); } while (0)
TEST(barrier_constants_and_size) {
    CHECK(sizeof(SYNCHRONIZATION_BARRIER) >= 32);   /* SDK layout; ntdll aligns within it */
    EXPECT_EQ(SYNCHRONIZATION_BARRIER_FLAGS_SPIN_ONLY, 1);
    EXPECT_EQ(SYNCHRONIZATION_BARRIER_FLAGS_BLOCK_ONLY, 2);
    EXPECT_EQ(SYNCHRONIZATION_BARRIER_FLAGS_NO_DELETE, 4);
}
TEST(barrier_init_validation) {
    NEED_BAR();
    SYNCHRONIZATION_BARRIER b;
    struct { LONG n, spin; int ok; } t[] = {
        { 1, 0, 1 }, { 1, -1, 1 }, { 2, 0, 1 }, { 64, 100, 1 }, { 1000, 4000, 1 }, { LONG_MAX, 0, 1 }, { 3, LONG_MAX, 1 },
        { 0, 0, 0 }, { -1, 0, 0 }, { LONG_MIN, 0, 0 }, { 1, -2, 0 }, { 1, LONG_MIN, 0 }, { 0, -1, 0 }, { -5, -5, 0 },
    };
    for (size_t i = 0; i < sizeof t / sizeof *t; i++) {
        memset(&b, 0xCC, sizeof b);
        SET_ERR(0);
        BOOL r = U(InitializeSynchronizationBarrier)(&b, t[i].n, t[i].spin);
        CHECKF((r != 0) == t[i].ok, "n=%ld spin=%ld got %d", t[i].n, t[i].spin, r);
        if (!t[i].ok) EXPECT_ERR(ERROR_INVALID_PARAMETER);
        if (r) EXPECT_EQ(U(DeleteSynchronizationBarrier)(&b), TRUE);
    }
}
TEST(barrier_single_thread_always_last) {
    NEED_BAR();
    SYNCHRONIZATION_BARRIER b;
    EXPECT_EQ(U(InitializeSynchronizationBarrier)(&b, 1, -1), TRUE);
    for (int i = 0; i < 20; i++) { if (U(EnterSynchronizationBarrier)(&b, 0) != TRUE) { CHECKF(0, "iter %d", i); break; } }
    DWORD fl[] = { SYNCHRONIZATION_BARRIER_FLAGS_SPIN_ONLY, SYNCHRONIZATION_BARRIER_FLAGS_BLOCK_ONLY, SYNCHRONIZATION_BARRIER_FLAGS_NO_DELETE,
                   SYNCHRONIZATION_BARRIER_FLAGS_SPIN_ONLY | SYNCHRONIZATION_BARRIER_FLAGS_NO_DELETE, SYNCHRONIZATION_BARRIER_FLAGS_BLOCK_ONLY | SYNCHRONIZATION_BARRIER_FLAGS_NO_DELETE };
    for (int i = 0; i < 5; i++) EXPECT_EQ(U(EnterSynchronizationBarrier)(&b, fl[i]), TRUE);
    EXPECT_EQ(U(DeleteSynchronizationBarrier)(&b), TRUE);
}
static SYNCHRONIZATION_BARRIER g_bar; static volatile LONG g_lastcnt, g_arrived, g_bad, g_round_total; static DWORD g_bflags; static LONG g_rounds;
static DWORD WINAPI bar_worker(LPVOID p) {
    for (LONG r = 0; r < g_rounds; r++) {
        InterlockedIncrement(&g_arrived);
        BOOL last = U(EnterSynchronizationBarrier)(&g_bar, g_bflags);
        if (last) InterlockedIncrement(&g_lastcnt);
        /* after the barrier everybody must have arrived for this round */
        if (g_arrived < (r + 1) * 4) InterlockedIncrement(&g_bad);
        U(EnterSynchronizationBarrier)(&g_bar, g_bflags);   /* second barrier so nobody races ahead into the next round */
    }
    return 0;
}
static void run_bar(DWORD flags, LONG spin, LONG rounds) {
    g_lastcnt = g_arrived = g_bad = 0; g_bflags = flags; g_rounds = rounds;
    EXPECT_EQ(U(InitializeSynchronizationBarrier)(&g_bar, 4, spin), TRUE);
    so_thr t[4]; for (int i = 0; i < 4; i++) t[i] = so_start(bar_worker, NULL);
    for (int i = 0; i < 4; i++) so_join(t[i], 20000);
    EXPECT_EQ(g_bad, 0);
    EXPECT_EQ(g_lastcnt, rounds);       /* exactly one thread per round gets TRUE at the first barrier */
    EXPECT_EQ(g_arrived, 4 * rounds);
    EXPECT_EQ(U(DeleteSynchronizationBarrier)(&g_bar), TRUE);
}
TEST(barrier_four_threads_default) { NEED_BAR(); run_bar(0, -1, 30); }
TEST(barrier_four_threads_block_only) { NEED_BAR(); run_bar(SYNCHRONIZATION_BARRIER_FLAGS_BLOCK_ONLY, 0, 30); }
TEST(barrier_four_threads_spin_only) { NEED_BAR(); run_bar(SYNCHRONIZATION_BARRIER_FLAGS_SPIN_ONLY, 100000, 20); }
TEST(barrier_four_threads_no_delete) { NEED_BAR(); run_bar(SYNCHRONIZATION_BARRIER_FLAGS_NO_DELETE, 2000, 20); }
TEST(barrier_reinitialize_after_delete) {
    NEED_BAR();
    SYNCHRONIZATION_BARRIER b;
    for (int i = 0; i < 20; i++) {
        EXPECT_EQ(U(InitializeSynchronizationBarrier)(&b, 1 + i % 3, i % 2 ? -1 : 50), TRUE);
        EXPECT_EQ(U(DeleteSynchronizationBarrier)(&b), TRUE);
    }
}
TEST(barrier_unaligned_storage_ok) {
    NEED_BAR();
    /* the barrier realigns itself inside its 48-byte buffer */
    char raw[64 + 8];
    SYNCHRONIZATION_BARRIER *b = (SYNCHRONIZATION_BARRIER *)(raw + 1);
    EXPECT_EQ(U(InitializeSynchronizationBarrier)(b, 1, 0), TRUE);
    EXPECT_EQ(U(EnterSynchronizationBarrier)(b, 0), TRUE);
    EXPECT_EQ(U(DeleteSynchronizationBarrier)(b), TRUE);
}
TEST(barrier_two_threads_blocks_until_both) {
    NEED_BAR();
    static SYNCHRONIZATION_BARRIER b; static volatile LONG st;
    st = 0;
    EXPECT_EQ(U(InitializeSynchronizationBarrier)(&b, 2, 0), TRUE);
    DWORD WINAPI w(LPVOID p) { InterlockedExchange(&st, 1); U(EnterSynchronizationBarrier)(&b, SYNCHRONIZATION_BARRIER_FLAGS_BLOCK_ONLY); InterlockedExchange(&st, 2); return 0; }
    so_thr t = so_start(w, NULL);
    so_spin_until(&st, 1, 3000); Sleep(200);
    EXPECT_EQ(st, 1);                                  /* still waiting for the 2nd participant */
    U(EnterSynchronizationBarrier)(&b, SYNCHRONIZATION_BARRIER_FLAGS_BLOCK_ONLY);
    EXPECT_EQ(so_join(t, 5000), 0);
    EXPECT_EQ(st, 2);
    EXPECT_EQ(U(DeleteSynchronizationBarrier)(&b), TRUE);
}

/* ======================= SList ======================= */
#define NEED_SL() do { REQUIRE(U(InitializeSListHead)); REQUIRE(U(InterlockedPushEntrySList)); REQUIRE(U(InterlockedPopEntrySList)); \
    REQUIRE(U(InterlockedFlushSList)); REQUIRE(U(QueryDepthSList)); } while (0)
typedef struct __attribute__((aligned(16))) item { SLIST_ENTRY e; int val; } item;
static __attribute__((aligned(16))) SLIST_HEADER g_sl;

TEST(slist_layout) {
    EXPECT_EQ(sizeof(SLIST_HEADER), 16);
    EXPECT_EQ(sizeof(SLIST_ENTRY), 16);
    EXPECT_EQ(__alignof__(SLIST_HEADER), 16);
    EXPECT_EQ(__alignof__(SLIST_ENTRY), 16);
}
TEST(slist_init_empty) {
    NEED_SL();
    SLIST_HEADER h __attribute__((aligned(16)));
    memset(&h, 0xCC, sizeof h);
    U(InitializeSListHead)(&h);
    EXPECT_EQ(h.Alignment, 0); t_note("SLIST_HEADER.Region after init: 0x%llx", (unsigned long long)h.Region);
    EXPECT_EQ(U(QueryDepthSList)(&h), 0);
    EXPECT_EQ((ULONG_PTR)U(InterlockedPopEntrySList)(&h), 0);
    EXPECT_EQ((ULONG_PTR)U(InterlockedFlushSList)(&h), 0);
    EXPECT_EQ(U(QueryDepthSList)(&h), 0);
}
TEST(slist_push_pop_lifo) {
    NEED_SL();
    item it[5] __attribute__((aligned(16)));
    U(InitializeSListHead)(&g_sl);
    PSLIST_ENTRY prev = (PSLIST_ENTRY)0x1;
    for (int i = 0; i < 5; i++) {
        it[i].val = i;
        prev = U(InterlockedPushEntrySList)(&g_sl, &it[i].e);
        EXPECT_EQ((ULONG_PTR)prev, i == 0 ? 0 : (ULONG_PTR)&it[i - 1].e);   /* returns the previous first entry */
        EXPECT_EQ(U(QueryDepthSList)(&g_sl), i + 1);
    }
    for (int i = 4; i >= 0; i--) {
        PSLIST_ENTRY e = U(InterlockedPopEntrySList)(&g_sl);
        EXPECT_EQ((ULONG_PTR)e, (ULONG_PTR)&it[i].e);
        EXPECT_EQ(U(QueryDepthSList)(&g_sl), i);
    }
    EXPECT_EQ((ULONG_PTR)U(InterlockedPopEntrySList)(&g_sl), 0);
}
TEST(slist_first_entry) {
    NEED_SL(); REQUIRE(UNT(RtlFirstEntrySList));
    item it[3] __attribute__((aligned(16)));
    U(InitializeSListHead)(&g_sl);
    EXPECT_EQ((ULONG_PTR)UNT(RtlFirstEntrySList)(&g_sl), 0);
    for (int i = 0; i < 3; i++) { U(InterlockedPushEntrySList)(&g_sl, &it[i].e); EXPECT_EQ((ULONG_PTR)UNT(RtlFirstEntrySList)(&g_sl), (ULONG_PTR)&it[i].e); }
    EXPECT_EQ(U(QueryDepthSList)(&g_sl), 3);   /* not consumed */
    U(InterlockedFlushSList)(&g_sl);
}
TEST(slist_flush_returns_chain_in_lifo_order) {
    NEED_SL();
    item it[6] __attribute__((aligned(16)));
    U(InitializeSListHead)(&g_sl);
    for (int i = 0; i < 6; i++) { it[i].val = i; U(InterlockedPushEntrySList)(&g_sl, &it[i].e); }
    PSLIST_ENTRY e = U(InterlockedFlushSList)(&g_sl);
    EXPECT_EQ(U(QueryDepthSList)(&g_sl), 0);
    EXPECT_EQ((ULONG_PTR)U(InterlockedPopEntrySList)(&g_sl), 0);
    int n = 0, expect = 5;
    while (e && n < 10) { item *p = (item *)e; if (p->val != expect) { CHECKF(0, "chain pos %d val %d want %d", n, p->val, expect); break; } expect--; n++; e = e->Next; }
    EXPECT_EQ(n, 6);
    EXPECT_EQ((ULONG_PTR)e, 0);
}
TEST(slist_flush_twice) {
    NEED_SL();
    item a __attribute__((aligned(16))), b __attribute__((aligned(16)));
    U(InitializeSListHead)(&g_sl);
    U(InterlockedPushEntrySList)(&g_sl, &a.e); U(InterlockedPushEntrySList)(&g_sl, &b.e);
    EXPECT_EQ((ULONG_PTR)U(InterlockedFlushSList)(&g_sl), (ULONG_PTR)&b.e);
    EXPECT_EQ((ULONG_PTR)U(InterlockedFlushSList)(&g_sl), 0);
}
TEST(slist_reuse_after_pop) {
    NEED_SL();
    item a __attribute__((aligned(16)));
    U(InitializeSListHead)(&g_sl);
    for (int i = 0; i < 1000; i++) {
        U(InterlockedPushEntrySList)(&g_sl, &a.e);
        if (U(InterlockedPopEntrySList)(&g_sl) != &a.e) { CHECKF(0, "iter %d", i); break; }
    }
    EXPECT_EQ(U(QueryDepthSList)(&g_sl), 0);
}
TEST(slist_depth_many) {
    NEED_SL();
    enum { N = 5000 };
    item *it = _aligned_malloc(N * sizeof(item), 16);
    CHECK(it != NULL); if (!it) return;
    U(InitializeSListHead)(&g_sl);
    for (int i = 0; i < N; i++) U(InterlockedPushEntrySList)(&g_sl, &it[i].e);
    EXPECT_EQ(U(QueryDepthSList)(&g_sl), N);
    int cnt = 0; while (U(InterlockedPopEntrySList)(&g_sl)) cnt++;
    EXPECT_EQ(cnt, N);
    _aligned_free(it);
}
TEST(slist_push_list_ex) {
    NEED_SL(); REQUIRE(U(InterlockedPushListSListEx));
    item it[6] __attribute__((aligned(16)));
    U(InitializeSListHead)(&g_sl);
    U(InterlockedPushEntrySList)(&g_sl, &it[0].e);
    /* chain it[1] -> it[2] -> it[3] (it[1] ends up first) */
    it[1].e.Next = &it[2].e; it[2].e.Next = &it[3].e; it[3].e.Next = NULL;
    PSLIST_ENTRY prev = U(InterlockedPushListSListEx)(&g_sl, &it[1].e, &it[3].e, 3);
    EXPECT_EQ((ULONG_PTR)prev, (ULONG_PTR)&it[0].e);
    EXPECT_EQ(U(QueryDepthSList)(&g_sl), 4);
    EXPECT_EQ((ULONG_PTR)U(InterlockedPopEntrySList)(&g_sl), (ULONG_PTR)&it[1].e);
    EXPECT_EQ((ULONG_PTR)U(InterlockedPopEntrySList)(&g_sl), (ULONG_PTR)&it[2].e);
    EXPECT_EQ((ULONG_PTR)U(InterlockedPopEntrySList)(&g_sl), (ULONG_PTR)&it[3].e);
    EXPECT_EQ((ULONG_PTR)U(InterlockedPopEntrySList)(&g_sl), (ULONG_PTR)&it[0].e);
    EXPECT_EQ(U(QueryDepthSList)(&g_sl), 0);
}
TEST(slist_push_list_into_empty) {
    NEED_SL(); REQUIRE(U(InterlockedPushListSListEx));
    item it[2] __attribute__((aligned(16)));
    U(InitializeSListHead)(&g_sl);
    it[0].e.Next = &it[1].e; it[1].e.Next = NULL;
    EXPECT_EQ((ULONG_PTR)U(InterlockedPushListSListEx)(&g_sl, &it[0].e, &it[1].e, 2), 0);
    EXPECT_EQ(U(QueryDepthSList)(&g_sl), 2);
    EXPECT_EQ((ULONG_PTR)U(InterlockedFlushSList)(&g_sl), (ULONG_PTR)&it[0].e);
}
TEST(slistx_push_list_single_entry) {
    NEED_SL(); REQUIRE(U(InterlockedPushListSListEx));
    item a __attribute__((aligned(16)));
    U(InitializeSListHead)(&g_sl);
    EXPECT_EQ((ULONG_PTR)U(InterlockedPushListSListEx)(&g_sl, &a.e, &a.e, 1), 0);
    EXPECT_EQ(U(QueryDepthSList)(&g_sl), 1);
    EXPECT_EQ((ULONG_PTR)U(InterlockedPopEntrySList)(&g_sl), (ULONG_PTR)&a.e);
}
TEST(slist_push_list_legacy_export) {
    NEED_SL();
    typedef PSLIST_ENTRY (WINAPI *pl_fn)(PSLIST_HEADER, PSLIST_ENTRY, PSLIST_ENTRY, ULONG);
    pl_fn f = (pl_fn)t_proc_k32("InterlockedPushListSList");
    REQUIRE(f);
    item it[2] __attribute__((aligned(16)));
    U(InitializeSListHead)(&g_sl);
    it[0].e.Next = &it[1].e; it[1].e.Next = NULL;
    EXPECT_EQ((ULONG_PTR)f(&g_sl, &it[0].e, &it[1].e, 2), 0);
    EXPECT_EQ(U(QueryDepthSList)(&g_sl), 2);
    U(InterlockedFlushSList)(&g_sl);
}
TEST(slist_header_encodes_depth_and_next) {
    NEED_SL();
    item a __attribute__((aligned(16))), b __attribute__((aligned(16)));
    U(InitializeSListHead)(&g_sl);
    U(InterlockedPushEntrySList)(&g_sl, &a.e);
    EXPECT_EQ(g_sl.HeaderX64.Depth, 1);
    U(InterlockedPushEntrySList)(&g_sl, &b.e);
    EXPECT_EQ(g_sl.HeaderX64.Depth, 2);
    U(InterlockedPopEntrySList)(&g_sl);
    EXPECT_EQ(g_sl.HeaderX64.Depth, 1);
    U(InterlockedPopEntrySList)(&g_sl);
    EXPECT_EQ(g_sl.HeaderX64.Depth, 0);
}
TEST(slist_header_sequence_advances) {
    NEED_SL();
    item a __attribute__((aligned(16)));
    U(InitializeSListHead)(&g_sl);
    ULONGLONG s0 = g_sl.HeaderX64.Sequence;
    U(InterlockedPushEntrySList)(&g_sl, &a.e);
    ULONGLONG s1 = g_sl.HeaderX64.Sequence;
    U(InterlockedPopEntrySList)(&g_sl);
    ULONGLONG s2 = g_sl.HeaderX64.Sequence;
    t_note("SList sequence %llu -> %llu -> %llu", (unsigned long long)s0, (unsigned long long)s1, (unsigned long long)s2);
    CHECK(s1 != s0 || s2 != s1);
}
TEST(slist_concurrent_push_pop) {
    NEED_SL();
    enum { T = 4, N = 2000 };
    static item *pool; static volatile LONG popped, bad; static unsigned char *seen;
    pool = _aligned_malloc(T * N * sizeof(item), 16); seen = calloc(T * N, 1);
    CHECK(pool && seen); if (!pool || !seen) return;
    U(InitializeSListHead)(&g_sl); popped = bad = 0;
    for (int i = 0; i < T * N; i++) pool[i].val = i;
    DWORD WINAPI pusher(LPVOID p) { int base = (int)(INT_PTR)p * N; for (int i = 0; i < N; i++) U(InterlockedPushEntrySList)(&g_sl, &pool[base + i].e); return 0; }
    so_thr t[T]; for (int i = 0; i < T; i++) t[i] = so_start(pusher, (LPVOID)(INT_PTR)i);
    for (int i = 0; i < T; i++) so_join(t[i], 20000);
    EXPECT_EQ(U(QueryDepthSList)(&g_sl), T * N);
    DWORD WINAPI popper(LPVOID p) { PSLIST_ENTRY e; while ((e = U(InterlockedPopEntrySList)(&g_sl))) { item *it = (item *)e; if (__sync_lock_test_and_set(&seen[it->val], 1)) InterlockedIncrement(&bad); InterlockedIncrement(&popped); } return 0; }
    for (int i = 0; i < T; i++) t[i] = so_start(popper, NULL);
    for (int i = 0; i < T; i++) so_join(t[i], 20000);
    EXPECT_EQ(popped, T * N); EXPECT_EQ(bad, 0);
    EXPECT_EQ(U(QueryDepthSList)(&g_sl), 0);
    _aligned_free(pool); free(seen);
}
TEST(slist_concurrent_mixed_push_pop_same_entries) {
    NEED_SL();
    enum { T = 4, N = 8 };
    static item *pool; static volatile LONG bad;
    pool = _aligned_malloc(N * sizeof(item), 16); CHECK(pool != NULL); if (!pool) return;
    U(InitializeSListHead)(&g_sl); bad = 0;
    for (int i = 0; i < N; i++) U(InterlockedPushEntrySList)(&g_sl, &pool[i].e);
    DWORD WINAPI churn(LPVOID p) { for (int i = 0; i < 20000; i++) { PSLIST_ENTRY e = U(InterlockedPopEntrySList)(&g_sl); if (e) U(InterlockedPushEntrySList)(&g_sl, e); } return 0; }
    so_thr t[T]; for (int i = 0; i < T; i++) t[i] = so_start(churn, NULL);
    for (int i = 0; i < T; i++) so_join(t[i], 30000);
    EXPECT_EQ(U(QueryDepthSList)(&g_sl), N);   /* ABA-safe: no entries lost or duplicated */
    int cnt = 0; while (U(InterlockedPopEntrySList)(&g_sl)) cnt++;
    EXPECT_EQ(cnt, N);
    _aligned_free(pool);
}
TEST(slist_ntdll_equivalents_exist) {
    CHECK(UNT(RtlInitializeSListHead) != NULL);
    CHECK(UNT(RtlInterlockedPushEntrySList) != NULL);
    CHECK(UNT(RtlInterlockedPopEntrySList) != NULL);
    CHECK(UNT(RtlInterlockedFlushSList) != NULL);
    CHECK(UNT(RtlQueryDepthSList) != NULL);
    CHECK(UNT(RtlFirstEntrySList) != NULL);
}
TEST(slist_kernel32_forwards_to_ntdll) {
    HMODULE k = t_kernel32();
    const char *names[] = { "InitializeSListHead", "InterlockedPushEntrySList", "InterlockedPopEntrySList", "InterlockedFlushSList", "QueryDepthSList" };
    for (int i = 0; i < 5; i++) {
        FARPROC p = GetProcAddress(k, names[i]);
        CHECKF(p != NULL, "%s", names[i]);
    }
}

/* ======================= Interlocked exports (where present) ======================= */
TEST(interlocked_exports_probe) {
    const char *names[] = { "InterlockedIncrement", "InterlockedDecrement", "InterlockedExchange", "InterlockedExchangeAdd", "InterlockedCompareExchange",
        "InterlockedIncrement64", "InterlockedDecrement64", "InterlockedExchange64", "InterlockedExchangeAdd64", "InterlockedCompareExchange64",
        "InterlockedAnd", "InterlockedOr", "InterlockedXor" };
    for (int i = 0; i < 13; i++) {
        FARPROC k = t_proc_k32(names[i]), b = t_proc_kb(names[i]), n = t_proc_nt(names[i]);
        t_note("%s exported: kernel32=%d kernelbase=%d ntdll=%d", names[i], !!k, !!b, !!n);
    }
}
TEST(interlocked_exports_semantics_if_present) {
    typedef LONG (WINAPI *f1)(volatile LONG *); typedef LONG (WINAPI *f2)(volatile LONG *, LONG); typedef LONG (WINAPI *f3)(volatile LONG *, LONG, LONG);
    f1 inc = (f1)t_proc("InterlockedIncrement"), dec = (f1)t_proc("InterlockedDecrement");
    f2 xch = (f2)t_proc("InterlockedExchange"), add = (f2)t_proc("InterlockedExchangeAdd");
    f3 cx = (f3)t_proc("InterlockedCompareExchange");
    if (!inc && !dec && !xch && !add && !cx) { t_skip("no Interlocked* exports (intrinsics on x64)"); return; }
    volatile LONG v = 5;
    if (inc) { EXPECT_EQ(inc(&v), 6); EXPECT_EQ(v, 6); }
    if (dec) { EXPECT_EQ(dec(&v), 5); EXPECT_EQ(v, 5); }
    if (xch) { EXPECT_EQ(xch(&v, 9), 5); EXPECT_EQ(v, 9); }
    if (add) { EXPECT_EQ(add(&v, 3), 9); EXPECT_EQ(v, 12); EXPECT_EQ(add(&v, -12), 12); EXPECT_EQ(v, 0); }
    if (cx) { v = 4; EXPECT_EQ(cx(&v, 8, 4), 4); EXPECT_EQ(v, 8); EXPECT_EQ(cx(&v, 1, 4), 8); EXPECT_EQ(v, 8); }
}
TEST(interlocked_intrinsic_semantics_reference) {
    /* compiler intrinsics used for scaffolding elsewhere in this area must behave as documented */
    volatile LONG v = 5; volatile LONGLONG q = 10;
    EXPECT_EQ(InterlockedIncrement(&v), 6); EXPECT_EQ(InterlockedDecrement(&v), 5);
    EXPECT_EQ(InterlockedExchange(&v, 7), 5); EXPECT_EQ(InterlockedExchangeAdd(&v, 3), 7); EXPECT_EQ(v, 10);
    EXPECT_EQ(InterlockedCompareExchange(&v, 1, 10), 10); EXPECT_EQ(v, 1);
    EXPECT_EQ(InterlockedAnd(&v, 0), 1); EXPECT_EQ(InterlockedOr(&v, 6), 0); EXPECT_EQ(InterlockedXor(&v, 3), 6); EXPECT_EQ(v, 5);
    EXPECT_EQ(InterlockedIncrement64(&q), 11); EXPECT_EQ(InterlockedExchangeAdd64(&q, 10), 11); EXPECT_EQ(q, 21);
    EXPECT_EQ(InterlockedCompareExchange64(&q, 2, 21), 21); EXPECT_EQ(q, 2);
}
