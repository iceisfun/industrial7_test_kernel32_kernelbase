#include "so.h"

#define NEED_CS() do { REQUIRE(U(InitializeCriticalSection)); REQUIRE(U(InitializeCriticalSectionAndSpinCount)); REQUIRE(U(InitializeCriticalSectionEx)); \
    REQUIRE(U(EnterCriticalSection)); REQUIRE(U(LeaveCriticalSection)); REQUIRE(U(TryEnterCriticalSection)); REQUIRE(U(DeleteCriticalSection)); \
    REQUIRE(U(SetCriticalSectionSpinCount)); } while (0)
static int mp(void) { SYSTEM_INFO si; GetSystemInfo(&si); return si.dwNumberOfProcessors > 1; }
#define CSTID() ((HANDLE)(ULONG_PTR)GetCurrentThreadId())

TEST(cs_struct_layout) {
    EXPECT_EQ(sizeof(CRITICAL_SECTION), 40);
    EXPECT_EQ(offsetof(CRITICAL_SECTION, DebugInfo), 0);
    EXPECT_EQ(offsetof(CRITICAL_SECTION, LockCount), 8);
    EXPECT_EQ(offsetof(CRITICAL_SECTION, RecursionCount), 12);
    EXPECT_EQ(offsetof(CRITICAL_SECTION, OwningThread), 16);
    EXPECT_EQ(offsetof(CRITICAL_SECTION, LockSemaphore), 24);
    EXPECT_EQ(offsetof(CRITICAL_SECTION, SpinCount), 32);
    EXPECT_EQ(CRITICAL_SECTION_NO_DEBUG_INFO, 0x01000000);
}
TEST(cs_init_fields) {
    NEED_CS();
    CRITICAL_SECTION cs; memset(&cs, 0xCC, sizeof cs);
    U(InitializeCriticalSection)(&cs);
    EXPECT_EQ(cs.LockCount, -1);
    EXPECT_EQ(cs.RecursionCount, 0);
    EXPECT_EQ(cs.OwningThread, NULL);
    EXPECT_EQ(cs.LockSemaphore, NULL);
    if (mp()) EXPECT_EQ(cs.SpinCount, 0x20007d0); else t_note("uniprocessor: SpinCount %llx", (unsigned long long)cs.SpinCount);
    U(DeleteCriticalSection)(&cs);
}
TEST(cs_enter_leave_state) {
    NEED_CS();
    CRITICAL_SECTION cs; U(InitializeCriticalSection)(&cs);
    U(EnterCriticalSection)(&cs);
    EXPECT_EQ(cs.RecursionCount, 1);
    EXPECT_EQ(cs.OwningThread, CSTID());
    CHECK((cs.LockCount & 1) == 0);   /* bit 0 clear == locked */
    U(LeaveCriticalSection)(&cs);
    EXPECT_EQ(cs.RecursionCount, 0);
    EXPECT_EQ(cs.OwningThread, NULL);
    EXPECT_EQ(cs.LockCount, -1);
    U(DeleteCriticalSection)(&cs);
}
TEST(cs_recursion) {
    NEED_CS();
    CRITICAL_SECTION cs; U(InitializeCriticalSection)(&cs);
    for (int i = 1; i <= 50; i++) { U(EnterCriticalSection)(&cs); if (cs.RecursionCount != i) { CHECKF(0, "rec %ld want %d", cs.RecursionCount, i); break; } }
    EXPECT_EQ(cs.OwningThread, CSTID());
    for (int i = 49; i >= 0; i--) { U(LeaveCriticalSection)(&cs); if (cs.RecursionCount != i) { CHECKF(0, "rec %ld want %d", cs.RecursionCount, i); break; } }
    EXPECT_EQ(cs.OwningThread, NULL);
    EXPECT_EQ(cs.LockCount, -1);
    U(DeleteCriticalSection)(&cs);
}
TEST(cs_try_enter) {
    NEED_CS();
    CRITICAL_SECTION cs; U(InitializeCriticalSection)(&cs);
    EXPECT_EQ(U(TryEnterCriticalSection)(&cs), TRUE);
    EXPECT_EQ(cs.RecursionCount, 1); EXPECT_EQ(cs.OwningThread, CSTID());
    EXPECT_EQ(U(TryEnterCriticalSection)(&cs), TRUE);   /* recursive */
    EXPECT_EQ(cs.RecursionCount, 2);
    U(LeaveCriticalSection)(&cs); U(LeaveCriticalSection)(&cs);
    EXPECT_EQ(cs.LockCount, -1);
    U(DeleteCriticalSection)(&cs);
}
static CRITICAL_SECTION g_cs; static volatile LONG g_try_res, g_state;
static DWORD WINAPI cs_try_thread(LPVOID p) { g_try_res = U(TryEnterCriticalSection)(&g_cs); if (g_try_res) U(LeaveCriticalSection)(&g_cs); return 0; }
TEST(cs_try_enter_fails_when_held_by_other_thread) {
    NEED_CS();
    U(InitializeCriticalSection)(&g_cs);
    U(EnterCriticalSection)(&g_cs);
    g_try_res = -1; so_join(so_start(cs_try_thread, NULL), 5000);
    EXPECT_EQ(g_try_res, 0);
    EXPECT_EQ(g_cs.RecursionCount, 1);
    U(LeaveCriticalSection)(&g_cs);
    g_try_res = -1; so_join(so_start(cs_try_thread, NULL), 5000);
    EXPECT_EQ(g_try_res, 1);
    U(DeleteCriticalSection)(&g_cs);
}
static DWORD WINAPI cs_block_thread(LPVOID p) {
    InterlockedExchange(&g_state, 1);
    U(EnterCriticalSection)(&g_cs);
    InterlockedExchange(&g_state, 2);
    U(LeaveCriticalSection)(&g_cs);
    return 0;
}
TEST(cs_enter_blocks_until_released) {
    NEED_CS();
    U(InitializeCriticalSection)(&g_cs); g_state = 0;
    U(EnterCriticalSection)(&g_cs);
    so_thr t = so_start(cs_block_thread, NULL);
    so_spin_until(&g_state, 1, 3000);
    Sleep(150);
    EXPECT_EQ(g_state, 1);                         /* still blocked */
    t_note("LockCount with one waiter: %ld", (long)g_cs.LockCount);
    CHECK((g_cs.LockCount & 1) == 0);
    U(LeaveCriticalSection)(&g_cs);
    EXPECT_EQ(so_join(t, 5000), 0);
    EXPECT_EQ(g_state, 2);
    U(DeleteCriticalSection)(&g_cs);
}
TEST(cs_contention_mutual_exclusion) {
    NEED_CS();
    static volatile LONG inside, bad; static int shared;
    U(InitializeCriticalSection)(&g_cs); inside = bad = shared = 0;
    DWORD WINAPI worker(LPVOID p) {
        for (int i = 0; i < 5000; i++) {
            U(EnterCriticalSection)(&g_cs);
            if (InterlockedIncrement(&inside) != 1) InterlockedIncrement(&bad);
            shared++;
            InterlockedDecrement(&inside);
            U(LeaveCriticalSection)(&g_cs);
        }
        return 0;
    }
    so_thr t[6];
    for (int i = 0; i < 6; i++) t[i] = so_start(worker, NULL);
    for (int i = 0; i < 6; i++) so_join(t[i], 30000);
    EXPECT_EQ(bad, 0); EXPECT_EQ(shared, 30000);
    EXPECT_EQ(g_cs.LockCount, -1);
    U(DeleteCriticalSection)(&g_cs);
}
TEST(cs_init_with_spin_count) {
    NEED_CS();
    CRITICAL_SECTION cs;
    static const DWORD spins[] = { 1, 100, 4000, 0x00FFFFFF };
    for (int i = 0; i < 4; i++) {
        memset(&cs, 0xCC, sizeof cs);
        SET_ERR(0);
        BOOL r = U(InitializeCriticalSectionAndSpinCount)(&cs, spins[i]);
        EXPECT_EQ(r, TRUE);
        EXPECT_EQ(cs.LockCount, -1); EXPECT_EQ(cs.RecursionCount, 0);
        if (mp()) EXPECT_EQ(cs.SpinCount & 0xFFFFFF, spins[i]);
        U(DeleteCriticalSection)(&cs);
    }
}
TEST(cs_init_spin_count_zero_and_high_bits) {
    NEED_CS();
    CRITICAL_SECTION cs;
    EXPECT_EQ(U(InitializeCriticalSectionAndSpinCount)(&cs, 0), TRUE);
    if (mp()) EXPECT_EQ(cs.SpinCount, 0x20007d0);
    U(DeleteCriticalSection)(&cs);
    /* the high bit requests pre-allocation of the event; the low 24 bits are the spin count */
    EXPECT_EQ(U(InitializeCriticalSectionAndSpinCount)(&cs, 0x80000000 | 200), TRUE);
    if (mp()) EXPECT_EQ(cs.SpinCount & 0xFFFFFF, 200);
    U(EnterCriticalSection)(&cs); U(LeaveCriticalSection)(&cs);
    U(DeleteCriticalSection)(&cs);
    EXPECT_EQ(U(InitializeCriticalSectionAndSpinCount)(&cs, 0xFFFFFFFF), TRUE);
    if (mp()) EXPECT_EQ(cs.SpinCount & 0xFFFFFF, 0xFFFFFF);
    U(DeleteCriticalSection)(&cs);
}
TEST(cs_ex_basic) {
    NEED_CS();
    CRITICAL_SECTION cs;
    SET_ERR(0);
    EXPECT_EQ(U(InitializeCriticalSectionEx)(&cs, 0, 0), TRUE);
    EXPECT_EQ(cs.LockCount, -1); EXPECT_EQ(cs.RecursionCount, 0);
    if (mp()) EXPECT_EQ(cs.SpinCount, 0x20007d0);
    U(EnterCriticalSection)(&cs); U(LeaveCriticalSection)(&cs);
    U(DeleteCriticalSection)(&cs);
    EXPECT_EQ(U(InitializeCriticalSectionEx)(&cs, 1234, 0), TRUE);
    if (mp()) EXPECT_EQ(cs.SpinCount, 1234);
    U(DeleteCriticalSection)(&cs);
}
TEST(cs_ex_no_debug_info_flag) {
    NEED_CS();
    CRITICAL_SECTION cs;
    EXPECT_EQ(U(InitializeCriticalSectionEx)(&cs, 50, CRITICAL_SECTION_NO_DEBUG_INFO), TRUE);
    EXPECT_EQ((ULONG_PTR)cs.DebugInfo, (ULONG_PTR)-1);
    CHECK((cs.SpinCount & CRITICAL_SECTION_NO_DEBUG_INFO) != 0);
    U(EnterCriticalSection)(&cs); EXPECT_EQ(cs.RecursionCount, 1); U(LeaveCriticalSection)(&cs);
    U(DeleteCriticalSection)(&cs);
}
TEST(cs_ex_force_debug_info_flag) {
    NEED_CS();
    CRITICAL_SECTION cs;
    SET_ERR(0);
    BOOL r = U(InitializeCriticalSectionEx)(&cs, 0, 0x10000000);
    CHECK(r);
    if (r) {
        CHECK((ULONG_PTR)cs.DebugInfo != (ULONG_PTR)-1 && cs.DebugInfo != NULL);
        U(DeleteCriticalSection)(&cs);
    }
}
TEST(cs_ex_invalid_arguments) {
    NEED_CS();
    CRITICAL_SECTION cs;
    struct { DWORD spin, flags; } bad[] = {
        { 0, 0x20000000 }, { 0, 0x40000000 }, { 0, 0x80000000 }, { 0, 0xE0000000 },   /* reserved flag bits */
        { 0, 0x11000000 },                                                              /* force + no-debug-info conflict */
        { 0x01000000, 0 }, { 0x80000000, 0 }, { 0xFF000000, 0 }, { 0x7F000000, 0 },     /* spin count > 24 bits */
    };
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        SET_ERR(0);
        BOOL r = U(InitializeCriticalSectionEx)(&cs, bad[i].spin, bad[i].flags);
        CHECKF(!r, "spin=0x%lx flags=0x%lx", bad[i].spin, bad[i].flags);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        if (r) U(DeleteCriticalSection)(&cs);
    }
}
TEST(cs_set_spin_count_returns_previous) {
    NEED_CS();
    if (!mp()) { t_skip("uniprocessor: spin count forced to 0"); return; }
    CRITICAL_SECTION cs;
    U(InitializeCriticalSectionAndSpinCount)(&cs, 100);
    EXPECT_EQ(U(SetCriticalSectionSpinCount)(&cs, 500), 100);
    EXPECT_EQ(cs.SpinCount & 0xFFFFFF, 500);
    EXPECT_EQ(U(SetCriticalSectionSpinCount)(&cs, 0), 500);
    EXPECT_EQ(cs.SpinCount & 0xFFFFFF, 0);
    EXPECT_EQ(U(SetCriticalSectionSpinCount)(&cs, 0x00FFFFFF), 0);
    EXPECT_EQ(cs.SpinCount & 0xFFFFFF, 0xFFFFFF);
    EXPECT_EQ(U(SetCriticalSectionSpinCount)(&cs, 7), 0xFFFFFF);
    U(DeleteCriticalSection)(&cs);
}
TEST(cs_set_spin_count_truncates_to_24_bits) {
    NEED_CS();
    if (!mp()) { t_skip("uniprocessor"); return; }
    CRITICAL_SECTION cs;
    U(InitializeCriticalSectionAndSpinCount)(&cs, 10);
    EXPECT_EQ(U(SetCriticalSectionSpinCount)(&cs, 0x01000005), 10);
    EXPECT_EQ(cs.SpinCount & 0xFFFFFF, 5);
    EXPECT_EQ(U(SetCriticalSectionSpinCount)(&cs, 0xFFFFFFFF), 5);
    EXPECT_EQ(cs.SpinCount & 0xFFFFFF, 0xFFFFFF);
    U(DeleteCriticalSection)(&cs);
}
TEST(cs_set_spin_count_preserves_flag_byte) {
    NEED_CS();
    if (!mp()) { t_skip("uniprocessor"); return; }
    CRITICAL_SECTION cs;
    U(InitializeCriticalSectionEx)(&cs, 20, CRITICAL_SECTION_NO_DEBUG_INFO);
    EXPECT_EQ(U(SetCriticalSectionSpinCount)(&cs, 99), 20);
    CHECK((cs.SpinCount & CRITICAL_SECTION_NO_DEBUG_INFO) != 0);
    EXPECT_EQ(cs.SpinCount & 0xFFFFFF, 99);
    U(DeleteCriticalSection)(&cs);
}
TEST(cs_works_with_spin_zero_and_large) {
    NEED_CS();
    static volatile LONG bad; static int shared; static DWORD spins[] = { 1, 0xFFFFFF };
    for (int k = 0; k < 2; k++) {
        U(InitializeCriticalSectionAndSpinCount)(&g_cs, spins[k]); bad = shared = 0;
        DWORD WINAPI w(LPVOID p) { for (int i = 0; i < 2000; i++) { U(EnterCriticalSection)(&g_cs); int v = shared; shared = v + 1; U(LeaveCriticalSection)(&g_cs); } return 0; }
        so_thr t[3]; for (int i = 0; i < 3; i++) t[i] = so_start(w, NULL);
        for (int i = 0; i < 3; i++) so_join(t[i], 30000);
        EXPECT_EQ(shared, 6000);
        U(DeleteCriticalSection)(&g_cs);
    }
}
TEST(cs_reinit_after_delete) {
    NEED_CS();
    CRITICAL_SECTION cs;
    for (int i = 0; i < 100; i++) {
        U(InitializeCriticalSection)(&cs);
        U(EnterCriticalSection)(&cs); U(EnterCriticalSection)(&cs);
        U(LeaveCriticalSection)(&cs); U(LeaveCriticalSection)(&cs);
        U(DeleteCriticalSection)(&cs);
    }
    CHECK(1);
}
TEST(cs_many_sections) {
    NEED_CS();
    enum { N = 2000 };
    CRITICAL_SECTION *cs = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, N * sizeof *cs);
    CHECK(cs != NULL); if (!cs) return;
    for (int i = 0; i < N; i++) U(InitializeCriticalSection)(&cs[i]);
    for (int i = 0; i < N; i++) { U(EnterCriticalSection)(&cs[i]); }
    for (int i = 0; i < N; i++) { if (cs[i].RecursionCount != 1) { CHECKF(0, "cs %d", i); break; } U(LeaveCriticalSection)(&cs[i]); }
    for (int i = 0; i < N; i++) U(DeleteCriticalSection)(&cs[i]);
    HeapFree(GetProcessHeap(), 0, cs);
}
TEST(cs_nested_two_sections_opposite_threads_no_deadlock_with_try) {
    NEED_CS();
    static CRITICAL_SECTION a, b; static volatile LONG done;
    U(InitializeCriticalSection)(&a); U(InitializeCriticalSection)(&b); done = 0;
    U(EnterCriticalSection)(&a);
    DWORD WINAPI th(LPVOID p) {
        U(EnterCriticalSection)(&b);
        BOOL got = U(TryEnterCriticalSection)(&a);   /* must fail: main holds a */
        InterlockedExchange(&done, got ? 2 : 1);
        if (got) U(LeaveCriticalSection)(&a);
        U(LeaveCriticalSection)(&b);
        return 0;
    }
    so_join(so_start(th, NULL), 5000);
    EXPECT_EQ(done, 1);
    U(LeaveCriticalSection)(&a);
    U(DeleteCriticalSection)(&a); U(DeleteCriticalSection)(&b);
}
TEST(cs_owner_thread_id_after_other_thread_use) {
    NEED_CS();
    static DWORD tid;
    U(InitializeCriticalSection)(&g_cs);
    DWORD WINAPI th(LPVOID p) { U(EnterCriticalSection)(&g_cs); tid = GetCurrentThreadId(); EXPECT_EQ(g_cs.OwningThread, (HANDLE)(ULONG_PTR)tid); U(LeaveCriticalSection)(&g_cs); return 0; }
    so_join(so_start(th, NULL), 5000);
    EXPECT_EQ(g_cs.OwningThread, NULL);
    CHECK(tid != GetCurrentThreadId());
    U(DeleteCriticalSection)(&g_cs);
}

/* ============================ SRW ============================ */
#define NEED_SRW() do { REQUIRE(U(InitializeSRWLock)); REQUIRE(U(AcquireSRWLockExclusive)); REQUIRE(U(AcquireSRWLockShared)); \
    REQUIRE(U(ReleaseSRWLockExclusive)); REQUIRE(U(ReleaseSRWLockShared)); REQUIRE(U(TryAcquireSRWLockExclusive)); REQUIRE(U(TryAcquireSRWLockShared)); } while (0)

TEST(srw_init_and_basic_state) {
    NEED_SRW();
    SRWLOCK l; memset(&l, 0xCC, sizeof l);
    U(InitializeSRWLock)(&l);
    EXPECT_EQ((ULONG_PTR)l.Ptr, 0);
    EXPECT_EQ(sizeof(SRWLOCK), sizeof(void *));
    U(AcquireSRWLockExclusive)(&l);
    CHECK(l.Ptr != NULL);
    U(ReleaseSRWLockExclusive)(&l);
    EXPECT_EQ((ULONG_PTR)l.Ptr, 0);
    U(AcquireSRWLockShared)(&l);
    CHECK(l.Ptr != NULL);
    U(ReleaseSRWLockShared)(&l);
    EXPECT_EQ((ULONG_PTR)l.Ptr, 0);
}
TEST(srw_static_initializer) {
    NEED_SRW();
    SRWLOCK l = SRWLOCK_INIT;
    EXPECT_EQ((ULONG_PTR)l.Ptr, 0);
    U(AcquireSRWLockExclusive)(&l); U(ReleaseSRWLockExclusive)(&l);
    EXPECT_EQ((ULONG_PTR)l.Ptr, 0);
}
TEST(srw_try_acquire_matrix) {
    NEED_SRW();
    SRWLOCK l; U(InitializeSRWLock)(&l);
    /* free */
    EXPECT_EQ(U(TryAcquireSRWLockExclusive)(&l), TRUE);
    EXPECT_EQ(U(TryAcquireSRWLockExclusive)(&l), FALSE);   /* not recursive */
    EXPECT_EQ(U(TryAcquireSRWLockShared)(&l), FALSE);
    U(ReleaseSRWLockExclusive)(&l);
    EXPECT_EQ(U(TryAcquireSRWLockShared)(&l), TRUE);
    EXPECT_EQ(U(TryAcquireSRWLockShared)(&l), TRUE);       /* shared is shareable */
    EXPECT_EQ(U(TryAcquireSRWLockExclusive)(&l), FALSE);
    U(ReleaseSRWLockShared)(&l);
    EXPECT_EQ(U(TryAcquireSRWLockExclusive)(&l), FALSE);   /* one shared holder remains */
    U(ReleaseSRWLockShared)(&l);
    EXPECT_EQ(U(TryAcquireSRWLockExclusive)(&l), TRUE);
    U(ReleaseSRWLockExclusive)(&l);
    EXPECT_EQ((ULONG_PTR)l.Ptr, 0);
}
TEST(srw_try_returns_boolean_byte) {
    NEED_SRW();
    SRWLOCK l; U(InitializeSRWLock)(&l);
    BOOLEAN a = U(TryAcquireSRWLockExclusive)(&l), b = U(TryAcquireSRWLockExclusive)(&l);
    EXPECT_EQ(a, 1); EXPECT_EQ(b, 0);
    U(ReleaseSRWLockExclusive)(&l);
}
TEST(srw_recursive_shared) {
    NEED_SRW();
    SRWLOCK l; U(InitializeSRWLock)(&l);
    for (int i = 0; i < 100; i++) U(AcquireSRWLockShared)(&l);
    EXPECT_EQ(U(TryAcquireSRWLockExclusive)(&l), FALSE);
    for (int i = 0; i < 99; i++) U(ReleaseSRWLockShared)(&l);
    EXPECT_EQ(U(TryAcquireSRWLockExclusive)(&l), FALSE);
    U(ReleaseSRWLockShared)(&l);
    EXPECT_EQ((ULONG_PTR)l.Ptr, 0);
    EXPECT_EQ(U(TryAcquireSRWLockExclusive)(&l), TRUE);
    U(ReleaseSRWLockExclusive)(&l);
}
static SRWLOCK g_srw; static volatile LONG g_r1, g_r2;
static DWORD WINAPI srw_try_x(LPVOID p) { g_r1 = U(TryAcquireSRWLockExclusive)(&g_srw); if (g_r1) U(ReleaseSRWLockExclusive)(&g_srw); g_r2 = U(TryAcquireSRWLockShared)(&g_srw); if (g_r2) U(ReleaseSRWLockShared)(&g_srw); return 0; }
TEST(srw_exclusive_blocks_other_threads) {
    NEED_SRW();
    U(InitializeSRWLock)(&g_srw);
    U(AcquireSRWLockExclusive)(&g_srw);
    so_join(so_start(srw_try_x, NULL), 5000);
    EXPECT_EQ(g_r1, 0); EXPECT_EQ(g_r2, 0);
    U(ReleaseSRWLockExclusive)(&g_srw);
    so_join(so_start(srw_try_x, NULL), 5000);
    EXPECT_EQ(g_r1, 1); EXPECT_EQ(g_r2, 1);
}
TEST(srw_shared_blocks_exclusive_only) {
    NEED_SRW();
    U(InitializeSRWLock)(&g_srw);
    U(AcquireSRWLockShared)(&g_srw);
    so_join(so_start(srw_try_x, NULL), 5000);
    EXPECT_EQ(g_r1, 0); EXPECT_EQ(g_r2, 1);
    U(ReleaseSRWLockShared)(&g_srw);
}
TEST(srw_release_from_other_thread) {
    NEED_SRW();
    U(InitializeSRWLock)(&g_srw);
    U(AcquireSRWLockExclusive)(&g_srw);
    DWORD WINAPI rel(LPVOID p) { U(ReleaseSRWLockExclusive)(&g_srw); return 0; }
    so_join(so_start(rel, NULL), 5000);
    EXPECT_EQ(U(TryAcquireSRWLockExclusive)(&g_srw), TRUE);
    U(ReleaseSRWLockExclusive)(&g_srw);
}
TEST(srw_two_readers_concurrently_inside) {
    NEED_SRW();
    static volatile LONG inside, peak;
    U(InitializeSRWLock)(&g_srw); inside = peak = 0;
    DWORD WINAPI rd(LPVOID p) {
        U(AcquireSRWLockShared)(&g_srw);
        LONG n = InterlockedIncrement(&inside);
        if (n > peak) peak = n;
        DWORD t0 = GetTickCount();
        while (inside < 2 && GetTickCount() - t0 < 2000) Sleep(1);
        if (inside > peak) peak = inside;
        InterlockedDecrement(&inside);
        U(ReleaseSRWLockShared)(&g_srw);
        return 0;
    }
    so_thr t[2] = { so_start(rd, NULL), so_start(rd, NULL) };
    so_join(t[0], 6000); so_join(t[1], 6000);
    EXPECT_EQ(peak, 2);
}
TEST(srw_writer_waits_for_reader) {
    NEED_SRW();
    static volatile LONG st;
    U(InitializeSRWLock)(&g_srw); st = 0;
    U(AcquireSRWLockShared)(&g_srw);
    DWORD WINAPI wr(LPVOID p) { InterlockedExchange(&st, 1); U(AcquireSRWLockExclusive)(&g_srw); InterlockedExchange(&st, 2); U(ReleaseSRWLockExclusive)(&g_srw); return 0; }
    so_thr t = so_start(wr, NULL);
    so_spin_until(&st, 1, 3000); Sleep(150);
    EXPECT_EQ(st, 1);
    U(ReleaseSRWLockShared)(&g_srw);
    EXPECT_EQ(so_join(t, 5000), 0);
    EXPECT_EQ(st, 2);
}
TEST(srw_reader_waits_for_writer) {
    NEED_SRW();
    static volatile LONG st;
    U(InitializeSRWLock)(&g_srw); st = 0;
    U(AcquireSRWLockExclusive)(&g_srw);
    DWORD WINAPI rd(LPVOID p) { InterlockedExchange(&st, 1); U(AcquireSRWLockShared)(&g_srw); InterlockedExchange(&st, 2); U(ReleaseSRWLockShared)(&g_srw); return 0; }
    so_thr t = so_start(rd, NULL);
    so_spin_until(&st, 1, 3000); Sleep(150);
    EXPECT_EQ(st, 1);
    U(ReleaseSRWLockExclusive)(&g_srw);
    EXPECT_EQ(so_join(t, 5000), 0);
    EXPECT_EQ(st, 2);
    EXPECT_EQ((ULONG_PTR)g_srw.Ptr, 0);
}
TEST(srw_exclusive_mutual_exclusion) {
    NEED_SRW();
    static volatile LONG inside, bad; static int shared;
    U(InitializeSRWLock)(&g_srw); inside = bad = shared = 0;
    DWORD WINAPI w(LPVOID p) {
        for (int i = 0; i < 5000; i++) {
            U(AcquireSRWLockExclusive)(&g_srw);
            if (InterlockedIncrement(&inside) != 1) InterlockedIncrement(&bad);
            shared++; InterlockedDecrement(&inside);
            U(ReleaseSRWLockExclusive)(&g_srw);
        }
        return 0;
    }
    so_thr t[6]; for (int i = 0; i < 6; i++) t[i] = so_start(w, NULL);
    for (int i = 0; i < 6; i++) so_join(t[i], 30000);
    EXPECT_EQ(bad, 0); EXPECT_EQ(shared, 30000);
    EXPECT_EQ((ULONG_PTR)g_srw.Ptr, 0);
}
TEST(srw_mixed_readers_writers_consistency) {
    NEED_SRW();
    static volatile LONG bad; static volatile LONG a, b;
    U(InitializeSRWLock)(&g_srw); bad = 0; a = b = 0;
    DWORD WINAPI wr(LPVOID p) { for (int i = 0; i < 2000; i++) { U(AcquireSRWLockExclusive)(&g_srw); a++; b++; U(ReleaseSRWLockExclusive)(&g_srw); } return 0; }
    DWORD WINAPI rd(LPVOID p) { for (int i = 0; i < 4000; i++) { U(AcquireSRWLockShared)(&g_srw); if (a != b) InterlockedIncrement(&bad); U(ReleaseSRWLockShared)(&g_srw); } return 0; }
    so_thr t[5] = { so_start(wr, NULL), so_start(wr, NULL), so_start(rd, NULL), so_start(rd, NULL), so_start(rd, NULL) };
    for (int i = 0; i < 5; i++) so_join(t[i], 30000);
    EXPECT_EQ(bad, 0); EXPECT_EQ(a, 4000); EXPECT_EQ(b, 4000);
    EXPECT_EQ((ULONG_PTR)g_srw.Ptr, 0);
}
TEST(srw_lock_in_static_storage_reuse) {
    NEED_SRW();
    SRWLOCK l; U(InitializeSRWLock)(&l);
    for (int i = 0; i < 10000; i++) { U(AcquireSRWLockExclusive)(&l); U(ReleaseSRWLockExclusive)(&l); U(AcquireSRWLockShared)(&l); U(ReleaseSRWLockShared)(&l); }
    EXPECT_EQ((ULONG_PTR)l.Ptr, 0);
}

/* ============================ condition variables ============================ */
#define NEED_CV() do { REQUIRE(U(InitializeConditionVariable)); REQUIRE(U(SleepConditionVariableCS)); REQUIRE(U(SleepConditionVariableSRW)); \
    REQUIRE(U(WakeConditionVariable)); REQUIRE(U(WakeAllConditionVariable)); } while (0)
TEST(cv_init_state) {
    NEED_CV();
    CONDITION_VARIABLE cv; memset(&cv, 0xCC, sizeof cv);
    U(InitializeConditionVariable)(&cv);
    EXPECT_EQ((ULONG_PTR)cv.Ptr, 0);
    EXPECT_EQ(sizeof(CONDITION_VARIABLE), sizeof(void *));
    CONDITION_VARIABLE s = CONDITION_VARIABLE_INIT; EXPECT_EQ((ULONG_PTR)s.Ptr, 0);
    EXPECT_EQ(CONDITION_VARIABLE_LOCKMODE_SHARED, 1);
}
TEST(cv_wake_without_waiters) {
    NEED_CV();
    CONDITION_VARIABLE cv; U(InitializeConditionVariable)(&cv);
    for (int i = 0; i < 100; i++) { U(WakeConditionVariable)(&cv); U(WakeAllConditionVariable)(&cv); }
    t_note("CV word after wakes without waiters: %p", cv.Ptr);
}
TEST(cv_sleep_cs_timeout) {
    NEED_CV(); NEED_CS();
    CONDITION_VARIABLE cv; CRITICAL_SECTION cs;
    U(InitializeConditionVariable)(&cv); U(InitializeCriticalSection)(&cs);
    U(EnterCriticalSection)(&cs);
    DWORD t0 = GetTickCount();
    SET_ERR(0);
    EXPECT_EQ(U(SleepConditionVariableCS)(&cv, &cs, 100), FALSE);
    EXPECT_ERR(ERROR_TIMEOUT);
    EXPECT_EQ(ERROR_TIMEOUT, 1460);
    DWORD dt = GetTickCount() - t0;
    CHECKF(dt >= 80 && dt < 3000, "elapsed %lu", dt);
    EXPECT_EQ(cs.RecursionCount, 1);              /* reacquired on timeout */
    EXPECT_EQ(cs.OwningThread, CSTID());
    SET_ERR(0);
    EXPECT_EQ(U(SleepConditionVariableCS)(&cv, &cs, 0), FALSE);
    EXPECT_ERR(ERROR_TIMEOUT);
    EXPECT_EQ(cs.RecursionCount, 1);
    U(LeaveCriticalSection)(&cs);
    U(DeleteCriticalSection)(&cs);
}
TEST(cv_sleep_cs_timeout_with_recursion) {
    NEED_CV(); NEED_CS();
    CONDITION_VARIABLE cv; CRITICAL_SECTION cs;
    U(InitializeConditionVariable)(&cv); U(InitializeCriticalSection)(&cs);
    U(EnterCriticalSection)(&cs); U(EnterCriticalSection)(&cs); U(EnterCriticalSection)(&cs);
    SET_ERR(0);
    EXPECT_EQ(U(SleepConditionVariableCS)(&cv, &cs, 30), FALSE);
    EXPECT_ERR(ERROR_TIMEOUT);
    EXPECT_EQ(cs.RecursionCount, 3);              /* recursion count restored */
    for (int i = 0; i < 3; i++) U(LeaveCriticalSection)(&cs);
    EXPECT_EQ(cs.LockCount, -1);
    U(DeleteCriticalSection)(&cs);
}
TEST(cv_sleep_srw_exclusive_timeout) {
    NEED_CV(); NEED_SRW();
    CONDITION_VARIABLE cv; SRWLOCK l;
    U(InitializeConditionVariable)(&cv); U(InitializeSRWLock)(&l);
    U(AcquireSRWLockExclusive)(&l);
    SET_ERR(0);
    EXPECT_EQ(U(SleepConditionVariableSRW)(&cv, &l, 60, 0), FALSE);
    EXPECT_ERR(ERROR_TIMEOUT);
    EXPECT_EQ(U(TryAcquireSRWLockExclusive)(&l), FALSE);   /* still held exclusively */
    U(ReleaseSRWLockExclusive)(&l);
    EXPECT_EQ((ULONG_PTR)l.Ptr, 0);
}
TEST(cv_sleep_srw_shared_timeout) {
    NEED_CV(); NEED_SRW();
    CONDITION_VARIABLE cv; SRWLOCK l;
    U(InitializeConditionVariable)(&cv); U(InitializeSRWLock)(&l);
    U(AcquireSRWLockShared)(&l);
    SET_ERR(0);
    EXPECT_EQ(U(SleepConditionVariableSRW)(&cv, &l, 60, CONDITION_VARIABLE_LOCKMODE_SHARED), FALSE);
    EXPECT_ERR(ERROR_TIMEOUT);
    EXPECT_EQ(U(TryAcquireSRWLockShared)(&l), TRUE);
    EXPECT_EQ(U(TryAcquireSRWLockExclusive)(&l), FALSE);
    U(ReleaseSRWLockShared)(&l); U(ReleaseSRWLockShared)(&l);
    EXPECT_EQ((ULONG_PTR)l.Ptr, 0);
}
TEST(cv_sleep_srw_invalid_flags) {
    NEED_CV(); NEED_SRW();
    CONDITION_VARIABLE cv; SRWLOCK l;
    U(InitializeConditionVariable)(&cv); U(InitializeSRWLock)(&l);
    U(AcquireSRWLockExclusive)(&l);
    SET_ERR(0);
    BOOL r = U(SleepConditionVariableSRW)(&cv, &l, 10, 0x2);
    t_note("SleepConditionVariableSRW flags=2 -> %d err %lu", r, GetLastError());
    CHECK(!r);
    U(ReleaseSRWLockExclusive)(&l);
}
static CONDITION_VARIABLE g_cv; static CRITICAL_SECTION g_ccs; static volatile LONG g_ready, g_woke, g_flag;
static DWORD WINAPI cv_cs_waiter(LPVOID p) {
    U(EnterCriticalSection)(&g_ccs);
    InterlockedIncrement(&g_ready);
    BOOL ok = TRUE;
    while (!g_flag && ok) ok = U(SleepConditionVariableCS)(&g_cv, &g_ccs, 4000);
    if (g_flag) InterlockedIncrement(&g_woke);
    U(LeaveCriticalSection)(&g_ccs);
    return 0;
}
TEST(cv_wake_all_releases_every_cs_waiter) {
    NEED_CV(); NEED_CS();
    U(InitializeConditionVariable)(&g_cv); U(InitializeCriticalSection)(&g_ccs); g_ready = g_woke = g_flag = 0;
    so_thr t[5]; for (int i = 0; i < 5; i++) t[i] = so_start(cv_cs_waiter, NULL);
    so_spin_ge(&g_ready, 5, 3000); Sleep(150);
    U(EnterCriticalSection)(&g_ccs); g_flag = 1; U(LeaveCriticalSection)(&g_ccs);
    U(WakeAllConditionVariable)(&g_cv);
    for (int i = 0; i < 5; i++) so_join(t[i], 6000);
    EXPECT_EQ(g_woke, 5);
    U(DeleteCriticalSection)(&g_ccs);
}
static volatile LONG g_tickets;
static DWORD WINAPI cv_cs_ticket_waiter(LPVOID p) {
    U(EnterCriticalSection)(&g_ccs);
    InterlockedIncrement(&g_ready);
    BOOL ok = TRUE;
    while (g_tickets == 0 && ok) ok = U(SleepConditionVariableCS)(&g_cv, &g_ccs, 1200);
    if (g_tickets > 0) { g_tickets--; InterlockedIncrement(&g_woke); }
    U(LeaveCriticalSection)(&g_ccs);
    return 0;
}
TEST(cv_wake_single_releases_one_waiter) {
    NEED_CV(); NEED_CS();
    U(InitializeConditionVariable)(&g_cv); U(InitializeCriticalSection)(&g_ccs); g_ready = g_woke = g_tickets = 0;
    so_thr t[3]; for (int i = 0; i < 3; i++) t[i] = so_start(cv_cs_ticket_waiter, NULL);
    so_spin_ge(&g_ready, 3, 3000); Sleep(150);
    U(EnterCriticalSection)(&g_ccs); g_tickets = 1; U(LeaveCriticalSection)(&g_ccs);
    U(WakeConditionVariable)(&g_cv);
    Sleep(300);
    EXPECT_EQ(g_woke, 1);
    for (int i = 0; i < 3; i++) so_join(t[i], 6000);
    EXPECT_EQ(g_woke, 1);
    U(DeleteCriticalSection)(&g_ccs);
}
TEST(cv_wake_n_times_releases_n_waiters) {
    NEED_CV(); NEED_CS();
    U(InitializeConditionVariable)(&g_cv); U(InitializeCriticalSection)(&g_ccs); g_ready = g_woke = g_tickets = 0;
    so_thr t[4]; for (int i = 0; i < 4; i++) t[i] = so_start(cv_cs_ticket_waiter, NULL);
    so_spin_ge(&g_ready, 4, 3000); Sleep(150);
    for (int i = 0; i < 3; i++) { U(EnterCriticalSection)(&g_ccs); g_tickets++; U(LeaveCriticalSection)(&g_ccs); U(WakeConditionVariable)(&g_cv); }
    for (int i = 0; i < 4; i++) so_join(t[i], 6000);
    EXPECT_EQ(g_woke, 3);
    U(DeleteCriticalSection)(&g_ccs);
}
static SRWLOCK g_cvl;
static DWORD WINAPI cv_srw_waiter(LPVOID p) {
    int shared = (int)(INT_PTR)p;
    if (shared) U(AcquireSRWLockShared)(&g_cvl); else U(AcquireSRWLockExclusive)(&g_cvl);
    InterlockedIncrement(&g_ready);
    BOOL ok = TRUE;
    while (!g_flag && ok) ok = U(SleepConditionVariableSRW)(&g_cv, &g_cvl, 4000, shared ? CONDITION_VARIABLE_LOCKMODE_SHARED : 0);
    if (g_flag) InterlockedIncrement(&g_woke);
    if (shared) U(ReleaseSRWLockShared)(&g_cvl); else U(ReleaseSRWLockExclusive)(&g_cvl);
    return 0;
}
TEST(cv_srw_exclusive_waiters_wake_all) {
    NEED_CV(); NEED_SRW();
    U(InitializeConditionVariable)(&g_cv); U(InitializeSRWLock)(&g_cvl); g_ready = g_woke = g_flag = 0;
    so_thr t[4]; for (int i = 0; i < 4; i++) t[i] = so_start(cv_srw_waiter, (LPVOID)0);
    so_spin_ge(&g_ready, 4, 3000); Sleep(150);
    U(AcquireSRWLockExclusive)(&g_cvl); g_flag = 1; U(ReleaseSRWLockExclusive)(&g_cvl);
    U(WakeAllConditionVariable)(&g_cv);
    for (int i = 0; i < 4; i++) so_join(t[i], 6000);
    EXPECT_EQ(g_woke, 4);
    EXPECT_EQ((ULONG_PTR)g_cvl.Ptr, 0);
}
TEST(cv_srw_shared_waiters_wake_all) {
    NEED_CV(); NEED_SRW();
    U(InitializeConditionVariable)(&g_cv); U(InitializeSRWLock)(&g_cvl); g_ready = g_woke = g_flag = 0;
    so_thr t[4]; for (int i = 0; i < 4; i++) t[i] = so_start(cv_srw_waiter, (LPVOID)1);
    so_spin_ge(&g_ready, 4, 3000); Sleep(150);
    U(AcquireSRWLockExclusive)(&g_cvl); g_flag = 1; U(ReleaseSRWLockExclusive)(&g_cvl);
    U(WakeAllConditionVariable)(&g_cv);
    for (int i = 0; i < 4; i++) so_join(t[i], 6000);
    EXPECT_EQ(g_woke, 4);
    EXPECT_EQ((ULONG_PTR)g_cvl.Ptr, 0);
}
TEST(cv_producer_consumer_queue) {
    NEED_CV(); NEED_CS();
    static int q[16], cnt, head, tail; static CONDITION_VARIABLE notempty, notfull; static volatile LONG sum;
    U(InitializeConditionVariable)(&notempty); U(InitializeConditionVariable)(&notfull); U(InitializeCriticalSection)(&g_ccs);
    cnt = head = tail = 0; sum = 0;
    DWORD WINAPI prod(LPVOID p) {
        for (int i = 1; i <= 500; i++) {
            U(EnterCriticalSection)(&g_ccs);
            while (cnt == 16) if (!U(SleepConditionVariableCS)(&notfull, &g_ccs, 5000)) { U(LeaveCriticalSection)(&g_ccs); return 1; }
            q[tail] = i; tail = (tail + 1) % 16; cnt++;
            U(LeaveCriticalSection)(&g_ccs);
            U(WakeConditionVariable)(&notempty);
        }
        return 0;
    }
    DWORD WINAPI cons(LPVOID p) {
        for (int i = 0; i < 250; i++) {
            U(EnterCriticalSection)(&g_ccs);
            while (cnt == 0) if (!U(SleepConditionVariableCS)(&notempty, &g_ccs, 5000)) { U(LeaveCriticalSection)(&g_ccs); return 1; }
            InterlockedAdd(&sum, q[head]); head = (head + 1) % 16; cnt--;
            U(LeaveCriticalSection)(&g_ccs);
            U(WakeConditionVariable)(&notfull);
        }
        return 0;
    }
    so_thr t[3] = { so_start(prod, NULL), so_start(cons, NULL), so_start(cons, NULL) };
    for (int i = 0; i < 3; i++) EXPECT_EQ(so_join(t[i], 30000), 0);
    EXPECT_EQ(sum, 500 * 501 / 2);
    U(DeleteCriticalSection)(&g_ccs);
}
TEST(cv_sleep_infinite_woken) {
    NEED_CV(); NEED_CS();
    U(InitializeConditionVariable)(&g_cv); U(InitializeCriticalSection)(&g_ccs); g_ready = g_flag = 0;
    DWORD WINAPI w(LPVOID p) {
        U(EnterCriticalSection)(&g_ccs); InterlockedIncrement(&g_ready);
        BOOL r = TRUE;
        while (!g_flag) r = U(SleepConditionVariableCS)(&g_cv, &g_ccs, INFINITE);
        U(LeaveCriticalSection)(&g_ccs);
        return r ? 0 : 1;
    }
    so_thr t = so_start(w, NULL);
    so_spin_ge(&g_ready, 1, 3000); Sleep(100);
    U(EnterCriticalSection)(&g_ccs); g_flag = 1; U(LeaveCriticalSection)(&g_ccs);
    U(WakeConditionVariable)(&g_cv);
    EXPECT_EQ(so_join(t, 6000), 0);
    U(DeleteCriticalSection)(&g_ccs);
}

/* ============================ InitOnce ============================ */
#define NEED_IO() do { REQUIRE(U(InitOnceInitialize)); REQUIRE(U(InitOnceExecuteOnce)); REQUIRE(U(InitOnceBeginInitialize)); REQUIRE(U(InitOnceComplete)); } while (0)
static volatile LONG io_calls;
static BOOL CALLBACK io_fn_ok(PINIT_ONCE o, PVOID param, PVOID *ctx) { InterlockedIncrement(&io_calls); if (ctx) *ctx = param; return TRUE; }
static BOOL CALLBACK io_fn_fail(PINIT_ONCE o, PVOID param, PVOID *ctx) { InterlockedIncrement(&io_calls); return FALSE; }
static BOOL CALLBACK io_fn_slow(PINIT_ONCE o, PVOID param, PVOID *ctx) { InterlockedIncrement(&io_calls); Sleep(100); if (ctx) *ctx = param; return TRUE; }
TEST(initonce_initialize) {
    NEED_IO();
    INIT_ONCE o; memset(&o, 0xCC, sizeof o);
    U(InitOnceInitialize)(&o);
    EXPECT_EQ((ULONG_PTR)o.Ptr, 0);
    INIT_ONCE s = INIT_ONCE_STATIC_INIT; EXPECT_EQ((ULONG_PTR)s.Ptr, 0);
    EXPECT_EQ(INIT_ONCE_CHECK_ONLY, 1); EXPECT_EQ(INIT_ONCE_ASYNC, 2); EXPECT_EQ(INIT_ONCE_INIT_FAILED, 4); EXPECT_EQ(INIT_ONCE_CTX_RESERVED_BITS, 2);
}
TEST(initonce_execute_runs_once) {
    NEED_IO();
    INIT_ONCE o = INIT_ONCE_STATIC_INIT; io_calls = 0;
    PVOID ctx = (PVOID)0x1111;
    EXPECT_EQ(U(InitOnceExecuteOnce)(&o, io_fn_ok, (PVOID)0x1000, &ctx), TRUE);
    EXPECT_EQ((ULONG_PTR)ctx, 0x1000);
    EXPECT_EQ(io_calls, 1);
    ctx = NULL;
    EXPECT_EQ(U(InitOnceExecuteOnce)(&o, io_fn_ok, (PVOID)0x2000, &ctx), TRUE);
    EXPECT_EQ((ULONG_PTR)ctx, 0x1000);           /* context from the first run */
    EXPECT_EQ(io_calls, 1);
    EXPECT_EQ(U(InitOnceExecuteOnce)(&o, io_fn_ok, (PVOID)0x3000, NULL), TRUE);
    EXPECT_EQ(io_calls, 1);
    CHECK(o.Ptr != NULL);
}
TEST(initonce_execute_null_context) {
    NEED_IO();
    INIT_ONCE o = INIT_ONCE_STATIC_INIT; io_calls = 0;
    EXPECT_EQ(U(InitOnceExecuteOnce)(&o, io_fn_ok, (PVOID)0x1000, NULL), TRUE);
    EXPECT_EQ(io_calls, 1);
}
TEST(initonce_execute_failure_allows_retry) {
    NEED_IO();
    INIT_ONCE o = INIT_ONCE_STATIC_INIT; io_calls = 0;
    SET_ERR(1234);
    EXPECT_EQ(U(InitOnceExecuteOnce)(&o, io_fn_fail, NULL, NULL), FALSE);
    t_note("last error after failing initializer: %lu (expected to be untouched: 1234)", GetLastError());
    EXPECT_EQ(io_calls, 1);
    EXPECT_EQ((ULONG_PTR)o.Ptr, 0);               /* back to uninitialized */
    EXPECT_EQ(U(InitOnceExecuteOnce)(&o, io_fn_fail, NULL, NULL), FALSE);
    EXPECT_EQ(io_calls, 2);
    PVOID ctx = NULL;
    EXPECT_EQ(U(InitOnceExecuteOnce)(&o, io_fn_ok, (PVOID)0x40, &ctx), TRUE);
    EXPECT_EQ(io_calls, 3);
    EXPECT_EQ((ULONG_PTR)ctx, 0x40);
}
static volatile DWORD io_rb_tid, io_rb_exc; static volatile LONG io_rb_ret;
static LONG CALLBACK io_rb_veh(PEXCEPTION_POINTERS p) {
    if (GetCurrentThreadId() == io_rb_tid && p->ExceptionRecord->ExceptionCode == 0xc00000f1) {
        io_rb_exc = p->ExceptionRecord->ExceptionCode; ExitThread(0);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
static DWORD WINAPI io_rb_thread(LPVOID p) {
    INIT_ONCE o = INIT_ONCE_STATIC_INIT; PVOID ctx = NULL; (void)p;
    io_rb_tid = GetCurrentThreadId();
    io_rb_ret = U(InitOnceExecuteOnce)(&o, io_fn_ok, (PVOID)0x3, &ctx);
    return 0;
}
TEST(initonce_execute_context_reserved_bits) {
    NEED_IO();
    INIT_ONCE o = INIT_ONCE_STATIC_INIT; io_calls = 0; PVOID ctx = NULL;
    (void)o; (void)ctx;
    /* Win10 RtlRunOnceExecuteOnce raises STATUS_INVALID_PARAMETER_5 (0xc00000f1) when the stored context
     * has reserved low bits, so run it on a helper thread with a vectored handler that ends the thread. */
    io_rb_tid = 0; io_rb_exc = 0; io_rb_ret = 0;
    PVOID veh = AddVectoredExceptionHandler(1, io_rb_veh);
    so_thr t = so_start(io_rb_thread, NULL);
    so_join(t, 10000);
    if (veh) RemoveVectoredExceptionHandler(veh);
    t_note("InitOnceExecuteOnce with context 0x3 -> ret %ld exception 0x%lx", io_rb_ret, (unsigned long)io_rb_exc);
    CHECK(!io_rb_ret);                                   /* never reports success */
    /* RtlRunOnceComplete fails and RtlRunOnceExecuteOnce raises it (ntdll FUN_1800b7470 -> raise path) */
    CHECKF(io_rb_exc == 0xc00000f1, "exception 0x%lx want STATUS_INVALID_PARAMETER_5", (unsigned long)io_rb_exc);
}
TEST(initonce_execute_concurrent_single_run) {
    NEED_IO();
    static INIT_ONCE o; static volatile LONG okc;
    memset(&o, 0, sizeof o); io_calls = 0; okc = 0;
    DWORD WINAPI w(LPVOID p) { PVOID c = NULL; if (U(InitOnceExecuteOnce)(&o, io_fn_slow, (PVOID)0x500, &c) && c == (PVOID)0x500) InterlockedIncrement(&okc); return 0; }
    so_thr t[8]; for (int i = 0; i < 8; i++) t[i] = so_start(w, NULL);
    for (int i = 0; i < 8; i++) so_join(t[i], 10000);
    EXPECT_EQ(io_calls, 1); EXPECT_EQ(okc, 8);
}
TEST(initonce_begin_complete_sync) {
    NEED_IO();
    INIT_ONCE o = INIT_ONCE_STATIC_INIT; BOOL pending = 7; PVOID ctx = (PVOID)0xABC0;
    EXPECT_EQ(U(InitOnceBeginInitialize)(&o, 0, &pending, &ctx), TRUE);
    EXPECT_EQ(pending, TRUE);
    EXPECT_EQ(U(InitOnceComplete)(&o, 0, (PVOID)0x1000), TRUE);
    pending = 7; ctx = NULL;
    EXPECT_EQ(U(InitOnceBeginInitialize)(&o, 0, &pending, &ctx), TRUE);
    EXPECT_EQ(pending, FALSE);
    EXPECT_EQ((ULONG_PTR)ctx, 0x1000);
    SET_ERR(0);
    EXPECT_EQ(U(InitOnceComplete)(&o, 0, (PVOID)0x2000), FALSE);   /* already complete */
    EXPECT_ERR(ERROR_GEN_FAILURE);
}
TEST(initonce_begin_complete_null_context) {
    NEED_IO();
    INIT_ONCE o = INIT_ONCE_STATIC_INIT; BOOL pending = 7;
    EXPECT_EQ(U(InitOnceBeginInitialize)(&o, 0, &pending, NULL), TRUE);
    EXPECT_EQ(pending, TRUE);
    EXPECT_EQ(U(InitOnceComplete)(&o, 0, NULL), TRUE);
    pending = 7;
    PVOID ctx = (PVOID)1;
    EXPECT_EQ(U(InitOnceBeginInitialize)(&o, 0, &pending, &ctx), TRUE);
    EXPECT_EQ(pending, FALSE);
    EXPECT_EQ((ULONG_PTR)ctx, 0);
}
TEST(initonce_complete_failed_resets) {
    NEED_IO();
    INIT_ONCE o = INIT_ONCE_STATIC_INIT; BOOL pending = 7;
    EXPECT_EQ(U(InitOnceBeginInitialize)(&o, 0, &pending, NULL), TRUE); EXPECT_EQ(pending, TRUE);
    EXPECT_EQ(U(InitOnceComplete)(&o, INIT_ONCE_INIT_FAILED, NULL), TRUE);
    EXPECT_EQ((ULONG_PTR)o.Ptr, 0);
    pending = 7;
    EXPECT_EQ(U(InitOnceBeginInitialize)(&o, 0, &pending, NULL), TRUE); EXPECT_EQ(pending, TRUE);   /* initializer runs again */
    EXPECT_EQ(U(InitOnceComplete)(&o, 0, (PVOID)0x80), TRUE);
}
TEST(initonce_begin_without_pending_ptr_is_not_safe_skip) {
    NEED_IO();
    /* the pending pointer is mandatory; just exercise the documented path with check-only */
    INIT_ONCE o = INIT_ONCE_STATIC_INIT; BOOL pending = 7; PVOID ctx = NULL;
    SET_ERR(0);
    BOOL r = U(InitOnceBeginInitialize)(&o, INIT_ONCE_CHECK_ONLY, &pending, &ctx);
    CHECK(!r);
    EXPECT_ERR(ERROR_GEN_FAILURE);
    EXPECT_EQ((ULONG_PTR)o.Ptr, 0);
}
TEST(initonce_check_only_after_complete) {
    NEED_IO();
    INIT_ONCE o = INIT_ONCE_STATIC_INIT; BOOL pending = 7; PVOID ctx = NULL;
    U(InitOnceBeginInitialize)(&o, 0, &pending, NULL);
    U(InitOnceComplete)(&o, 0, (PVOID)0x44);
    pending = 7;
    EXPECT_EQ(U(InitOnceBeginInitialize)(&o, INIT_ONCE_CHECK_ONLY, &pending, &ctx), TRUE);
    EXPECT_EQ(pending, FALSE);
    EXPECT_EQ((ULONG_PTR)ctx, 0x44);
}
TEST(initonce_check_only_while_in_progress) {
    NEED_IO();
    INIT_ONCE o = INIT_ONCE_STATIC_INIT; BOOL pending = 7; PVOID ctx = NULL;
    U(InitOnceBeginInitialize)(&o, 0, &pending, NULL);
    SET_ERR(0);
    BOOL r = U(InitOnceBeginInitialize)(&o, INIT_ONCE_CHECK_ONLY, &pending, &ctx);
    CHECK(!r);
    EXPECT_ERR(ERROR_GEN_FAILURE);
    U(InitOnceComplete)(&o, INIT_ONCE_INIT_FAILED, NULL);
}
TEST(initonce_invalid_begin_flags) {
    NEED_IO();
    DWORD bad[] = { 0x8, 0x10, 0x80, 0x100, 0x80000000, 0xFFFFFFF8 };
    for (int i = 0; i < 6; i++) {
        INIT_ONCE o = INIT_ONCE_STATIC_INIT; BOOL pending = 7; PVOID ctx = NULL;
        SET_ERR(0);
        BOOL r = U(InitOnceBeginInitialize)(&o, bad[i], &pending, &ctx);
        CHECKF(!r, "flags 0x%lx", bad[i]); EXPECT_ERR(ERROR_INVALID_PARAMETER);
        EXPECT_EQ((ULONG_PTR)o.Ptr, 0);
    }
}
TEST(initonce_invalid_complete_flags) {
    NEED_IO();
    INIT_ONCE o = INIT_ONCE_STATIC_INIT; BOOL pending;
    U(InitOnceBeginInitialize)(&o, 0, &pending, NULL);
    DWORD bad[] = { INIT_ONCE_CHECK_ONLY, 0x8, 0x10, 0x80000000 };
    for (int i = 0; i < 4; i++) { SET_ERR(0); BOOL r = U(InitOnceComplete)(&o, bad[i], NULL); CHECKF(!r, "flags 0x%lx", bad[i]); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
    EXPECT_EQ(U(InitOnceComplete)(&o, 0, NULL), TRUE);
}
TEST(initonce_complete_without_begin) {
    NEED_IO();
    INIT_ONCE o = INIT_ONCE_STATIC_INIT;
    SET_ERR(0);
    EXPECT_EQ(U(InitOnceComplete)(&o, 0, NULL), FALSE);
    EXPECT_ERR(ERROR_GEN_FAILURE);
    EXPECT_EQ((ULONG_PTR)o.Ptr, 0);
}
TEST(initonce_complete_context_reserved_bits) {
    NEED_IO();
    INIT_ONCE o = INIT_ONCE_STATIC_INIT; BOOL pending;
    U(InitOnceBeginInitialize)(&o, 0, &pending, NULL);
    PVOID bad[] = { (PVOID)1, (PVOID)2, (PVOID)3, (PVOID)0x1001 };
    for (int i = 0; i < 4; i++) { SET_ERR(0); BOOL r = U(InitOnceComplete)(&o, 0, bad[i]); CHECKF(!r, "ctx %p", bad[i]); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
    EXPECT_EQ(U(InitOnceComplete)(&o, 0, (PVOID)0x1004), TRUE);
}
TEST(initonce_async_two_initializers) {
    NEED_IO();
    INIT_ONCE o = INIT_ONCE_STATIC_INIT; BOOL p1 = 7, p2 = 7; PVOID ctx = NULL;
    EXPECT_EQ(U(InitOnceBeginInitialize)(&o, INIT_ONCE_ASYNC, &p1, &ctx), TRUE); EXPECT_EQ(p1, TRUE);
    EXPECT_EQ(U(InitOnceBeginInitialize)(&o, INIT_ONCE_ASYNC, &p2, &ctx), TRUE); EXPECT_EQ(p2, TRUE);   /* both may initialize */
    EXPECT_EQ(U(InitOnceComplete)(&o, INIT_ONCE_ASYNC, (PVOID)0x100), TRUE);
    SET_ERR(0);
    EXPECT_EQ(U(InitOnceComplete)(&o, INIT_ONCE_ASYNC, (PVOID)0x200), FALSE);   /* loser must discard its result */
    EXPECT_ERR(ERROR_GEN_FAILURE);
    BOOL p3 = 7;
    EXPECT_EQ(U(InitOnceBeginInitialize)(&o, INIT_ONCE_ASYNC, &p3, &ctx), TRUE);
    EXPECT_EQ(p3, FALSE);
    EXPECT_EQ((ULONG_PTR)ctx, 0x100);
}
TEST(initonce_async_failed_complete) {
    NEED_IO();
    INIT_ONCE o = INIT_ONCE_STATIC_INIT; BOOL p = 7;
    EXPECT_EQ(U(InitOnceBeginInitialize)(&o, INIT_ONCE_ASYNC, &p, NULL), TRUE);
    /* only one of ASYNC / INIT_FAILED may be given, and an async init must complete with ASYNC */
    SET_ERR(0); EXPECT_EQ(U(InitOnceComplete)(&o, INIT_ONCE_ASYNC | INIT_ONCE_INIT_FAILED, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(InitOnceComplete)(&o, INIT_ONCE_INIT_FAILED, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(InitOnceComplete)(&o, 0, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    EXPECT_EQ(U(InitOnceComplete)(&o, INIT_ONCE_ASYNC, NULL), TRUE);
    p = 7; EXPECT_EQ(U(InitOnceBeginInitialize)(&o, 0, &p, NULL), TRUE); EXPECT_EQ(p, FALSE);
}
TEST(initonce_sync_second_caller_blocks_until_complete) {
    NEED_IO();
    static INIT_ONCE o; static volatile LONG st, pend2; static PVOID got;
    memset(&o, 0, sizeof o); st = 0; pend2 = 7; got = NULL;
    BOOL pending = 0;
    EXPECT_EQ(U(InitOnceBeginInitialize)(&o, 0, &pending, NULL), TRUE);
    DWORD WINAPI w(LPVOID p) { BOOL pn = 7; PVOID c = NULL; InterlockedExchange(&st, 1); BOOL r = U(InitOnceBeginInitialize)(&o, 0, &pn, &c); got = c; pend2 = pn; InterlockedExchange(&st, r ? 2 : 3); return 0; }
    so_thr t = so_start(w, NULL);
    so_spin_until(&st, 1, 3000); Sleep(200);
    EXPECT_EQ(st, 1);   /* blocked while initialization is pending */
    EXPECT_EQ(U(InitOnceComplete)(&o, 0, (PVOID)0x900), TRUE);
    so_join(t, 5000);
    EXPECT_EQ(st, 2); EXPECT_EQ(pend2, FALSE); EXPECT_EQ((ULONG_PTR)got, 0x900);
}
TEST(initonce_two_independent_objects) {
    NEED_IO();
    INIT_ONCE a = INIT_ONCE_STATIC_INIT, b = INIT_ONCE_STATIC_INIT; io_calls = 0;
    U(InitOnceExecuteOnce)(&a, io_fn_ok, NULL, NULL);
    U(InitOnceExecuteOnce)(&b, io_fn_ok, NULL, NULL);
    EXPECT_EQ(io_calls, 2);
    U(InitOnceInitialize)(&a);
    U(InitOnceExecuteOnce)(&a, io_fn_ok, NULL, NULL);
    EXPECT_EQ(io_calls, 3);                         /* re-armed by InitOnceInitialize */
}

TEST(initonce_begin_flag_combinations) {
    NEED_IO();
    DWORD bad[] = { INIT_ONCE_CHECK_ONLY | INIT_ONCE_ASYNC, INIT_ONCE_INIT_FAILED, 5, 6, 7 };
    for (int i = 0; i < 5; i++) {
        INIT_ONCE o = INIT_ONCE_STATIC_INIT; BOOL pending = 7;
        SET_ERR(0);
        EXPECT_EQ(U(InitOnceBeginInitialize)(&o, bad[i], &pending, NULL), FALSE);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        EXPECT_EQ((ULONG_PTR)o.Ptr, 0);
    }
}
TEST(initonce_mixing_sync_and_async_modes) {
    NEED_IO();
    INIT_ONCE o = INIT_ONCE_STATIC_INIT; BOOL p = 7;
    /* async initialization in progress: a sync Begin is a usage error */
    EXPECT_EQ(U(InitOnceBeginInitialize)(&o, INIT_ONCE_ASYNC, &p, NULL), TRUE);
    SET_ERR(0); EXPECT_EQ(U(InitOnceBeginInitialize)(&o, 0, &p, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    EXPECT_EQ(U(InitOnceComplete)(&o, INIT_ONCE_ASYNC, NULL), TRUE);
    /* sync initialization in progress: an async Begin is a usage error and ASYNC completion is rejected */
    INIT_ONCE s = INIT_ONCE_STATIC_INIT;
    EXPECT_EQ(U(InitOnceBeginInitialize)(&s, 0, &p, NULL), TRUE);
    SET_ERR(0); EXPECT_EQ(U(InitOnceBeginInitialize)(&s, INIT_ONCE_ASYNC, &p, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(InitOnceComplete)(&s, INIT_ONCE_ASYNC, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    EXPECT_EQ(U(InitOnceComplete)(&s, 0, NULL), TRUE);
}
TEST(initonce_complete_failed_with_context_rejected) {
    NEED_IO();
    INIT_ONCE o = INIT_ONCE_STATIC_INIT; BOOL p;
    U(InitOnceBeginInitialize)(&o, 0, &p, NULL);
    SET_ERR(0); EXPECT_EQ(U(InitOnceComplete)(&o, INIT_ONCE_INIT_FAILED, (PVOID)0x100), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    EXPECT_EQ(U(InitOnceComplete)(&o, INIT_ONCE_INIT_FAILED, NULL), TRUE);
}
TEST(initonce_complete_on_completed_with_flags) {
    NEED_IO();
    INIT_ONCE o = INIT_ONCE_STATIC_INIT; BOOL p;
    U(InitOnceBeginInitialize)(&o, 0, &p, NULL);
    U(InitOnceComplete)(&o, 0, (PVOID)0x10);
    SET_ERR(0); EXPECT_EQ(U(InitOnceComplete)(&o, INIT_ONCE_INIT_FAILED, NULL), FALSE); EXPECT_ERR(ERROR_GEN_FAILURE);
    SET_ERR(0); EXPECT_EQ(U(InitOnceComplete)(&o, INIT_ONCE_ASYNC, NULL), FALSE); EXPECT_ERR(ERROR_GEN_FAILURE);
    PVOID c = NULL;
    EXPECT_EQ(U(InitOnceBeginInitialize)(&o, 0, &p, &c), TRUE); EXPECT_EQ(p, FALSE); EXPECT_EQ((ULONG_PTR)c, 0x10);
}
TEST(initonce_execute_failed_then_other_thread_runs) {
    NEED_IO();
    static INIT_ONCE o; static volatile LONG phase;
    memset(&o, 0, sizeof o); io_calls = 0; phase = 0;
    EXPECT_EQ(U(InitOnceExecuteOnce)(&o, io_fn_fail, NULL, NULL), FALSE);
    DWORD WINAPI w(LPVOID p) { PVOID c = NULL; BOOL r = U(InitOnceExecuteOnce)(&o, io_fn_ok, (PVOID)0x60, &c); return r && c == (PVOID)0x60 ? 0 : 1; }
    EXPECT_EQ(so_join(so_start(w, NULL), 5000), 0);
    EXPECT_EQ(io_calls, 2);
}
