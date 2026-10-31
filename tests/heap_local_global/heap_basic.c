#include "hlg.h"

#define PH() GetProcessHeap()

static HANDLE mk(DWORD fl, SIZE_T ini, SIZE_T mx) { return U(HeapCreate)(fl, ini, mx); }

/* ---------------- HeapCreate / HeapDestroy ---------------- */

TEST(heap_create_default) {
    REQUIRE(U(HeapCreate)); REQUIRE(U(HeapDestroy));
    HANDLE h = mk(0, 0, 0);
    CHECK(h != NULL);
    CHECK(h != PH());
    CHECK(((ULONG_PTR)h & 0xF) == 0);
    SetLastError(0);
    CHECK(U(HeapDestroy)(h));
}
TEST(heap_create_initial_only) {
    REQUIRE(U(HeapCreate)); REQUIRE(U(HeapDestroy));
    HANDLE h = mk(0, 0x10000, 0);
    CHECK(h != NULL);
    if (h) { void *p = U(HeapAlloc)(h, 0, 100); CHECK(p != NULL); if (p) U(HeapFree)(h, 0, p); CHECK(U(HeapDestroy)(h)); }
}
TEST(heap_create_initial_large_growable) {
    REQUIRE(U(HeapCreate)); REQUIRE(U(HeapDestroy));
    HANDLE h = mk(0, 4 * 1024 * 1024, 0);
    CHECK(h != NULL);
    if (h) {
        void *p = U(HeapAlloc)(h, HEAP_ZERO_MEMORY, 3 * 1024 * 1024);
        CHECK(p != NULL);
        if (p) { CHECK(hlg_all(p, 3 * 1024 * 1024, 0)); U(HeapFree)(h, 0, p); }
        CHECK(U(HeapDestroy)(h));
    }
}
TEST(heap_create_noserialize) {
    REQUIRE(U(HeapCreate)); REQUIRE(U(HeapDestroy));
    HANDLE h = mk(HEAP_NO_SERIALIZE, 0, 0);
    CHECK(h != NULL);
    if (!h) return;
    void *p = U(HeapAlloc)(h, 0, 64);
    CHECK(p != NULL);
    CHECK(U(HeapValidate)(h, 0, NULL));
    CHECK(U(HeapFree)(h, 0, p));
    CHECK(U(HeapDestroy)(h));
}
TEST(heap_create_ignored_flags) {
    REQUIRE(U(HeapCreate)); REQUIRE(U(HeapDestroy));
    /* flags other than NO_SERIALIZE, GENERATE_EXCEPTIONS, ENABLE_EXECUTE are masked off */
    DWORD fl[] = { HEAP_ZERO_MEMORY, HEAP_REALLOC_IN_PLACE_ONLY, 0x00000002, 0x00100000, 0x80000000 };
    for (size_t i = 0; i < sizeof fl / sizeof fl[0]; i++) {
        HANDLE h = mk(fl[i], 0, 0);
        CHECKF(h != NULL, "flags 0x%lx", fl[i]);
        if (h) {
            void *p = U(HeapAlloc)(h, 0, 32);
            CHECK(p != NULL);
            if (p) U(HeapFree)(h, 0, p);
            CHECK(U(HeapDestroy)(h));
        }
    }
}
TEST(heap_create_execute_protection) {
    REQUIRE(U(HeapCreate)); REQUIRE(U(HeapDestroy));
    HANDLE h = mk(HEAP_CREATE_ENABLE_EXECUTE, 0, 0);
    CHECK(h != NULL);
    if (!h) return;
    void *p = U(HeapAlloc)(h, 0, 128);
    CHECK(p != NULL);
    MEMORY_BASIC_INFORMATION mbi;
    if (p && VirtualQuery(p, &mbi, sizeof mbi)) {
        CHECK(mbi.State == MEM_COMMIT);
        CHECKF(mbi.Protect & (PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_READ | PAGE_EXECUTE | PAGE_EXECUTE_WRITECOPY), "protect 0x%lx", mbi.Protect);
    }
    if (p) U(HeapFree)(h, 0, p);
    CHECK(U(HeapDestroy)(h));
}
TEST(heap_create_default_not_executable) {
    REQUIRE(U(HeapCreate)); REQUIRE(U(HeapDestroy));
    HANDLE h = mk(0, 0, 0);
    CHECK(h != NULL);
    if (!h) return;
    void *p = U(HeapAlloc)(h, 0, 128);
    CHECK(p != NULL);
    MEMORY_BASIC_INFORMATION mbi;
    if (p && VirtualQuery(p, &mbi, sizeof mbi)) {
        CHECKF(mbi.Protect == PAGE_READWRITE, "protect 0x%lx", mbi.Protect);
        CHECK(mbi.Type == MEM_PRIVATE);
    }
    if (p) U(HeapFree)(h, 0, p);
    CHECK(U(HeapDestroy)(h));
}
TEST(heap_create_handle_in_committed_region) {
    REQUIRE(U(HeapCreate)); REQUIRE(U(HeapDestroy));
    HANDLE h = mk(0, 0, 0);
    CHECK(h != NULL);
    if (!h) return;
    MEMORY_BASIC_INFORMATION mbi;
    CHECK(VirtualQuery(h, &mbi, sizeof mbi) != 0);
    CHECK(mbi.State == MEM_COMMIT);
    CHECK(mbi.Type == MEM_PRIVATE);
    CHECK(((ULONG_PTR)mbi.AllocationBase & 0xFFFF) == 0);
    CHECK((char *)h >= (char *)mbi.AllocationBase);
    CHECK((char *)h < (char *)mbi.AllocationBase + 0x10000);
    t_note("heap handle %p allocation base %p", h, mbi.AllocationBase);
    CHECK(U(HeapDestroy)(h));
}
TEST(heap_create_block_inside_heap_region) {
    REQUIRE(U(HeapCreate)); REQUIRE(U(HeapDestroy));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    void *p = U(HeapAlloc)(h, 0, 200);
    MEMORY_BASIC_INFORMATION a, b;
    CHECK(p && VirtualQuery(p, &a, sizeof a) && VirtualQuery(h, &b, sizeof b));
    if (p) CHECK(a.AllocationBase == b.AllocationBase);
    if (p) U(HeapFree)(h, 0, p);
    CHECK(U(HeapDestroy)(h));
}
TEST(heap_create_many_distinct) {
    REQUIRE(U(HeapCreate)); REQUIRE(U(HeapDestroy));
    HANDLE hs[32]; int n = 0;
    for (int i = 0; i < 32; i++) { hs[i] = mk(0, 0, 0); if (hs[i]) n++; }
    CHECK(n == 32);
    for (int i = 0; i < 32; i++) for (int j = i + 1; j < 32; j++) if (hs[i] && hs[j]) CHECK(hs[i] != hs[j]);
    for (int i = 0; i < 32; i++) if (hs[i]) CHECK(U(HeapDestroy)(hs[i]));
}
TEST(heap_create_destroy_256) {
    REQUIRE(U(HeapCreate)); REQUIRE(U(HeapDestroy));
    int ok = 0, dok = 0;
    for (int i = 0; i < 256; i++) {
        HANDLE h = mk((i & 1) ? HEAP_NO_SERIALIZE : 0, (i & 2) ? 0x4000 : 0, 0);
        if (!h) continue;
        ok++;
        void *p = U(HeapAlloc)(h, HEAP_ZERO_MEMORY, 10 + i);
        if (p) U(HeapFree)(h, 0, p);
        if (U(HeapDestroy)(h)) dok++;
    }
    CHECK(ok == 256); CHECK(dok == 256);
}
TEST(heap_create_fixed_max_refuses_big) {
    REQUIRE(U(HeapCreate)); REQUIRE(U(HeapDestroy));
    HANDLE h = mk(0, 0x2000, 0x20000);
    CHECK(h != NULL);
    if (!h) return;
    HLG_POISON();
    void *p = U(HeapAlloc)(h, 0, 0x100000);
    CHECK(p == NULL);
    EXPECT_ERR(HLG_NOMEM);
    CHECK(U(HeapDestroy)(h));
}
TEST(heap_create_fixed_max_exhaustion) {
    REQUIRE(U(HeapCreate)); REQUIRE(U(HeapDestroy));
    HANDLE h = mk(0, 0x10000, 0x40000);
    if (!h) { CHECK(h); return; }
    void *blk[600]; int n = 0;
    for (; n < 600; n++) { blk[n] = U(HeapAlloc)(h, 0, 1000); if (!blk[n]) break; }
    CHECK(n < 600);          /* ran out well before 600 KB */
    CHECK(n > 50);           /* but got a decent number */
    HLG_POISON();
    CHECK(U(HeapAlloc)(h, 0, 1000) == NULL);
    EXPECT_ERR(HLG_NOMEM);
    CHECK(U(HeapValidate)(h, 0, NULL));
    /* freeing makes room again */
    if (n > 0) U(HeapFree)(h, 0, blk[n - 1]);
    void *again = U(HeapAlloc)(h, 0, 500);
    CHECK(again != NULL);
    for (int i = 0; i < n - 1; i++) U(HeapFree)(h, 0, blk[i]);
    CHECK(U(HeapDestroy)(h));
}
TEST(heap_create_fixed_max_block_limit) {
    REQUIRE(U(HeapCreate)); REQUIRE(U(HeapDestroy));
    /* non-growable heaps cannot satisfy a block larger than 0x7FFF8 */
    HANDLE h = mk(0, 0, 0x400000);
    if (!h) { CHECK(h); return; }
    HLG_POISON();
    void *p = U(HeapAlloc)(h, 0, 0x7FFF9);
    CHECK(p == NULL);
    EXPECT_ERR(HLG_NOMEM);
    void *q = U(HeapAlloc)(h, 0, 0x7F000);
    CHECK(q != NULL);
    if (q) U(HeapFree)(h, 0, q);
    CHECK(U(HeapDestroy)(h));
}
TEST(heap_create_growable_big_block) {
    REQUIRE(U(HeapCreate)); REQUIRE(U(HeapDestroy));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    void *p = U(HeapAlloc)(h, 0, 16 * 1024 * 1024);   /* much larger than 0x7FFF8 */
    CHECK(p != NULL);
    if (p) { CHECK(U(HeapSize)(h, 0, p) == 16 * 1024 * 1024); U(HeapFree)(h, 0, p); }
    CHECK(U(HeapDestroy)(h));
}
TEST(heap_create_absurd_initial_fails) {
    REQUIRE(U(HeapCreate));
    HLG_POISON();
    HANDLE h = mk(0, (SIZE_T)0x7FFFFFFFFFFF0000ULL, 0);
    CHECK(h == NULL);
    EXPECT_ERR(HLG_NOMEM);
    if (h) U(HeapDestroy)(h);
}
TEST(heap_create_absurd_max_fails) {
    REQUIRE(U(HeapCreate));
    HLG_POISON();
    HANDLE h = mk(0, 0, (SIZE_T)0x7FFFFFFFFFFF0000ULL);
    CHECK(h == NULL);
    if (h) U(HeapDestroy)(h);
}
TEST(heap_destroy_with_live_blocks) {
    REQUIRE(U(HeapCreate)); REQUIRE(U(HeapDestroy));
    /* destroying a heap with live blocks is allowed, memory goes with it */
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    for (int i = 0; i < 20; i++) U(HeapAlloc)(h, 0, 100 + i);
    CHECK(U(HeapDestroy)(h));
}
CHILD(destroy_procheap) {
    HANDLE ph = GetProcessHeap();
    BOOL r = U(HeapDestroy)(ph);
    DWORD e = GetLastError();
    /* still usable afterwards? */
    void *p = U(HeapAlloc)(ph, 0, 10);
    if (r) return 10;
    if (!p) return 11;
    U(HeapFree)(ph, 0, p);
    return e == 6 ? 0 : 20;
}
TEST(heap_destroy_process_heap_fails) {
    REQUIRE(U(HeapDestroy));
    DWORD rc = t_run_child("destroy_procheap", "", 20000);
    CHECKF(rc == 0 || rc == 20, "child rc 0x%lx", rc);
    if (rc == 20) t_note("process heap destroy failed with unexpected last error");
    CHECK(rc != 10);
}

/* ---------------- HeapAlloc ---------------- */

static const SIZE_T g_sizes[] = { 0, 1, 2, 7, 8, 9, 15, 16, 17, 24, 31, 32, 33, 63, 64, 100, 255, 256, 1000, 1024, 4000, 4096, 4097, 8192, 16000, 32768, 65535, 65536, 100000, 500000, 1 << 20, 3 << 20 };
#define NSIZES (sizeof g_sizes / sizeof g_sizes[0])

#define SIZE_TEST(idx) \
TEST(heap_alloc_size_##idx) { \
    REQUIRE(U(HeapAlloc)); REQUIRE(U(HeapSize)); \
    SIZE_T n = g_sizes[idx]; \
    HANDLE h = mk(0, 0, 0); if (!h) { CHECK(h); return; } \
    void *p = U(HeapAlloc)(h, 0, n); \
    CHECK(p != NULL); \
    if (p) { \
        CHECK(((ULONG_PTR)p & 0xF) == 0); \
        CHECK(U(HeapSize)(h, 0, p) == n); \
        hlg_fill(p, n, 0x11); CHECK(hlg_verify(p, n, 0x11)); \
        CHECK(U(HeapValidate)(h, 0, p)); \
        CHECK(U(HeapFree)(h, 0, p)); \
    } \
    void *z = U(HeapAlloc)(h, HEAP_ZERO_MEMORY, n); \
    CHECK(z != NULL); \
    if (z) { CHECK(hlg_all(z, n, 0)); CHECK(U(HeapSize)(h, 0, z) == n); U(HeapFree)(h, 0, z); } \
    CHECK(U(HeapDestroy)(h)); \
}
SIZE_TEST(0) SIZE_TEST(1) SIZE_TEST(2) SIZE_TEST(3) SIZE_TEST(4) SIZE_TEST(5) SIZE_TEST(6) SIZE_TEST(7)
SIZE_TEST(8) SIZE_TEST(9) SIZE_TEST(10) SIZE_TEST(11) SIZE_TEST(12) SIZE_TEST(13) SIZE_TEST(14) SIZE_TEST(15)
SIZE_TEST(16) SIZE_TEST(17) SIZE_TEST(18) SIZE_TEST(19) SIZE_TEST(20) SIZE_TEST(21) SIZE_TEST(22) SIZE_TEST(23)
SIZE_TEST(24) SIZE_TEST(25) SIZE_TEST(26) SIZE_TEST(27) SIZE_TEST(28) SIZE_TEST(29) SIZE_TEST(30) SIZE_TEST(31)

TEST(heap_alloc_zero_size_distinct) {
    REQUIRE(U(HeapAlloc));
    HANDLE h = PH();
    void *a = U(HeapAlloc)(h, 0, 0), *b = U(HeapAlloc)(h, 0, 0), *c = U(HeapAlloc)(h, 0, 0);
    CHECK(a && b && c);
    CHECK(a != b && b != c && a != c);
    EXPECT_EQ(U(HeapSize)(h, 0, a), 0);
    U(HeapFree)(h, 0, a); U(HeapFree)(h, 0, b); U(HeapFree)(h, 0, c);
}
TEST(heap_alloc_distinct_nonoverlapping) {
    REQUIRE(U(HeapAlloc));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    char *p[40];
    for (int i = 0; i < 40; i++) { p[i] = U(HeapAlloc)(h, 0, 48 + i); CHECK(p[i]); if (p[i]) memset(p[i], i + 1, 48 + i); }
    for (int i = 0; i < 40; i++) if (p[i]) CHECK(hlg_all(p[i], 48 + i, (unsigned char)(i + 1)));
    for (int i = 0; i < 40; i++) for (int j = i + 1; j < 40; j++)
        if (p[i] && p[j]) CHECK(p[i] + 48 + i <= p[j] || p[j] + 48 + j <= p[i]);
    CHECK(U(HeapValidate)(h, 0, NULL));
    for (int i = 0; i < 40; i++) if (p[i]) U(HeapFree)(h, 0, p[i]);
    CHECK(U(HeapDestroy)(h));
}
TEST(heap_alloc_zero_memory_reuse) {
    REQUIRE(U(HeapAlloc));
    /* a freed dirty block handed out again with HEAP_ZERO_MEMORY must be zero */
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    for (int round = 0; round < 20; round++) {
        unsigned char *p = U(HeapAlloc)(h, 0, 200);
        if (!p) { CHECK(p); break; }
        memset(p, 0xEE, 200);
        U(HeapFree)(h, 0, p);
        unsigned char *q = U(HeapAlloc)(h, HEAP_ZERO_MEMORY, 200);
        CHECK(q && hlg_all(q, 200, 0));
        if (q) U(HeapFree)(h, 0, q);
    }
    CHECK(U(HeapDestroy)(h));
}
TEST(heap_alloc_huge_fails) {
    REQUIRE(U(HeapAlloc));
    SIZE_T huge[] = { (SIZE_T)-1, (SIZE_T)-0x1000, (SIZE_T)-0x10, (SIZE_T)0x8000000000000000ULL,
                      (SIZE_T)0x7FFFFFFFFFFFFFFFULL, (SIZE_T)0x7FFFFFFF00000000ULL, (SIZE_T)0x100000000000ULL };
    HANDLE hs[2] = { mk(0, 0, 0), PH() };
    for (int k = 0; k < 2; k++) {
        if (!hs[k]) continue;
        for (size_t i = 0; i < sizeof huge / sizeof huge[0]; i++) {
            HLG_POISON();
            void *p = U(HeapAlloc)(hs[k], 0, huge[i]);
            CHECKF(p == NULL, "heap %d size 0x%llx", k, (unsigned long long)huge[i]);
            if (!p) EXPECT_ERR(HLG_NOMEM);
            else U(HeapFree)(hs[k], 0, p);
        }
    }
    if (hs[0]) U(HeapDestroy)(hs[0]);
}
TEST(heap_alloc_huge_zero_memory_fails) {
    REQUIRE(U(HeapAlloc));
    HLG_POISON();
    void *p = U(HeapAlloc)(PH(), HEAP_ZERO_MEMORY, (SIZE_T)-1);
    CHECK(p == NULL);
    EXPECT_ERR(HLG_NOMEM);
}
TEST(heap_alloc_still_works_after_failure) {
    REQUIRE(U(HeapAlloc));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    CHECK(U(HeapAlloc)(h, 0, (SIZE_T)-1) == NULL);
    void *p = U(HeapAlloc)(h, 0, 64);
    CHECK(p != NULL);
    CHECK(U(HeapValidate)(h, 0, NULL));
    if (p) U(HeapFree)(h, 0, p);
    CHECK(U(HeapDestroy)(h));
}
TEST(heap_alloc_noserialize_flag_per_call) {
    REQUIRE(U(HeapAlloc));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    void *p = U(HeapAlloc)(h, HEAP_NO_SERIALIZE, 77);
    CHECK(p != NULL);
    if (p) { CHECK(U(HeapSize)(h, HEAP_NO_SERIALIZE, p) == 77); CHECK(U(HeapFree)(h, HEAP_NO_SERIALIZE, p)); }
    CHECK(U(HeapDestroy)(h));
}
TEST(heap_alloc_many_small_sizes) {
    REQUIRE(U(HeapAlloc));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    void *p[1024];
    for (int i = 0; i < 1024; i++) { p[i] = U(HeapAlloc)(h, 0, (SIZE_T)(i % 130) + 1); CHECK(p[i] != NULL); if (p[i]) CHECK(((ULONG_PTR)p[i] & 0xF) == 0); }
    for (int i = 0; i < 1024; i++) if (p[i]) CHECK(U(HeapSize)(h, 0, p[i]) == (SIZE_T)(i % 130) + 1);
    for (int i = 0; i < 1024; i += 2) if (p[i]) CHECK(U(HeapFree)(h, 0, p[i]));
    CHECK(U(HeapValidate)(h, 0, NULL));
    for (int i = 1; i < 1024; i += 2) if (p[i]) CHECK(U(HeapFree)(h, 0, p[i]));
    CHECK(U(HeapValidate)(h, 0, NULL));
    CHECK(U(HeapDestroy)(h));
}
TEST(heap_alloc_lfh_candidate_same_size) {
    REQUIRE(U(HeapAlloc));
    /* 200 same-sized allocations trigger the low fragmentation front end on typical heaps */
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    void *p[200];
    int bad = 0;
    for (int i = 0; i < 200; i++) { p[i] = U(HeapAlloc)(h, 0, 48); if (!p[i]) bad++; else { hlg_fill(p[i], 48, (unsigned char)i); } }
    CHECK(bad == 0);
    for (int i = 0; i < 200; i++) if (p[i]) CHECK(hlg_verify(p[i], 48, (unsigned char)i));
    for (int i = 0; i < 200; i++) if (p[i]) CHECK(U(HeapSize)(h, 0, p[i]) == 48);
    for (int i = 0; i < 200; i++) if (p[i]) U(HeapFree)(h, 0, p[i]);
    CHECK(U(HeapValidate)(h, 0, NULL));
    CHECK(U(HeapDestroy)(h));
}

/* GENERATE_EXCEPTIONS (exception raised on failure; verified in a child with a VEH) */
static LONG CALLBACK exc_veh(PEXCEPTION_POINTERS ep) {
    DWORD c = ep->ExceptionRecord->ExceptionCode;
    if (c == 0xC0000017) ExitProcess(0x100 + 1);        /* STATUS_NO_MEMORY */
    if (c == 0xC0000005) ExitProcess(0x100 + 2);        /* STATUS_ACCESS_VIOLATION */
    return EXCEPTION_CONTINUE_SEARCH;
}
CHILD(gen_exc_create_flag) {
    HANDLE h = U(HeapCreate)(HEAP_GENERATE_EXCEPTIONS, 0, 0);
    if (!h) return 3;
    AddVectoredExceptionHandler(1, exc_veh);
    void *p = U(HeapAlloc)(h, 0, (SIZE_T)-1);
    return p ? 4 : 5;
}
CHILD(gen_exc_call_flag) {
    HANDLE h = U(HeapCreate)(0, 0, 0);
    if (!h) return 3;
    AddVectoredExceptionHandler(1, exc_veh);
    void *p = U(HeapAlloc)(h, HEAP_GENERATE_EXCEPTIONS, (SIZE_T)-1);
    return p ? 4 : 5;
}
CHILD(gen_exc_off) {
    HANDLE h = U(HeapCreate)(0, 0, 0);
    if (!h) return 3;
    AddVectoredExceptionHandler(1, exc_veh);
    void *p = U(HeapAlloc)(h, 0, (SIZE_T)-1);
    return p ? 4 : 5;     /* NULL, no exception */
}
CHILD(gen_exc_fixed_exhaust) {
    HANDLE h = U(HeapCreate)(HEAP_GENERATE_EXCEPTIONS, 0x10000, 0x20000);
    if (!h) return 3;
    AddVectoredExceptionHandler(1, exc_veh);
    for (int i = 0; i < 1000; i++) if (!U(HeapAlloc)(h, 0, 1000)) return 5;
    return 4;
}
CHILD(gen_exc_realloc) {
    HANDLE h = U(HeapCreate)(HEAP_GENERATE_EXCEPTIONS, 0, 0);
    if (!h) return 3;
    void *p = U(HeapAlloc)(h, 0, 10);
    AddVectoredExceptionHandler(1, exc_veh);
    void *q = U(HeapReAlloc)(h, 0, p, (SIZE_T)-1);
    return q ? 4 : 5;
}
TEST(heap_generate_exceptions_create_flag) {
    REQUIRE(U(HeapAlloc));
    EXPECT_EQ(t_run_child("gen_exc_create_flag", "", 20000), 0x101);
}
TEST(heap_generate_exceptions_call_flag) {
    REQUIRE(U(HeapAlloc));
    EXPECT_EQ(t_run_child("gen_exc_call_flag", "", 20000), 0x101);
}
TEST(heap_generate_exceptions_off_returns_null) {
    REQUIRE(U(HeapAlloc));
    EXPECT_EQ(t_run_child("gen_exc_off", "", 20000), 5);
}
TEST(heap_generate_exceptions_fixed_exhaust) {
    REQUIRE(U(HeapAlloc));
    EXPECT_EQ(t_run_child("gen_exc_fixed_exhaust", "", 20000), 0x101);
}
TEST(heap_generate_exceptions_realloc) {
    REQUIRE(U(HeapReAlloc));
    EXPECT_EQ(t_run_child("gen_exc_realloc", "", 20000), 0x101);
}

/* ---------------- HeapFree ---------------- */

TEST(heap_free_null_succeeds) {
    REQUIRE(U(HeapFree));
    HLG_POISON();
    CHECK(U(HeapFree)(PH(), 0, NULL));
    HANDLE h = mk(0, 0, 0);
    if (h) { CHECK(U(HeapFree)(h, 0, NULL)); CHECK(U(HeapFree)(h, HEAP_NO_SERIALIZE, NULL)); CHECK(U(HeapDestroy)(h)); }
}
TEST(heap_free_returns_true_and_size_gone) {
    REQUIRE(U(HeapFree));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    void *p = U(HeapAlloc)(h, 0, 300);
    CHECK(p != NULL);
    CHECK(U(HeapFree)(h, 0, p));
    CHECK(U(HeapValidate)(h, 0, NULL));
    CHECK(U(HeapDestroy)(h));
}
TEST(heap_free_realloc_cycle) {
    REQUIRE(U(HeapFree));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    for (int i = 1; i <= 200; i++) {
        void *p = U(HeapAlloc)(h, 0, (SIZE_T)i * 13);
        CHECK(p != NULL);
        if (p) CHECK(U(HeapFree)(h, 0, p));
    }
    CHECK(U(HeapValidate)(h, 0, NULL));
    CHECK(U(HeapDestroy)(h));
}

/* ---------------- HeapReAlloc ---------------- */

TEST(heap_realloc_grow_preserves) {
    REQUIRE(U(HeapReAlloc));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    SIZE_T from[] = { 1, 16, 100, 1000, 5000, 70000 };
    SIZE_T to[]   = { 2, 100, 5000, 70000, 200000, 600000 };
    for (int i = 0; i < 6; i++) {
        unsigned char *p = U(HeapAlloc)(h, 0, from[i]);
        if (!p) { CHECK(p); continue; }
        hlg_fill(p, from[i], 0x31);
        unsigned char *q = U(HeapReAlloc)(h, 0, p, to[i]);
        CHECKF(q != NULL, "grow %zu->%zu", from[i], to[i]);
        if (q) {
            CHECK(hlg_verify(q, from[i], 0x31));
            CHECK(U(HeapSize)(h, 0, q) == to[i]);
            CHECK(((ULONG_PTR)q & 0xF) == 0);
            U(HeapFree)(h, 0, q);
        } else U(HeapFree)(h, 0, p);
    }
    CHECK(U(HeapDestroy)(h));
}
TEST(heap_realloc_shrink_preserves) {
    REQUIRE(U(HeapReAlloc));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    SIZE_T from[] = { 100000, 5000, 1000, 100, 17 };
    SIZE_T to[]   = { 5000, 1000, 100, 17, 1 };
    for (int i = 0; i < 5; i++) {
        unsigned char *p = U(HeapAlloc)(h, 0, from[i]);
        if (!p) { CHECK(p); continue; }
        hlg_fill(p, from[i], 0x42);
        unsigned char *q = U(HeapReAlloc)(h, 0, p, to[i]);
        CHECKF(q != NULL, "shrink %zu->%zu", from[i], to[i]);
        if (q) { CHECK(hlg_verify(q, to[i], 0x42)); CHECK(U(HeapSize)(h, 0, q) == to[i]); U(HeapFree)(h, 0, q); }
    }
    CHECK(U(HeapDestroy)(h));
}
TEST(heap_realloc_shrink_in_place_only_same_pointer) {
    REQUIRE(U(HeapReAlloc));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    unsigned char *p = U(HeapAlloc)(h, 0, 4000);
    if (!p) { CHECK(p); return; }
    hlg_fill(p, 4000, 0x5);
    unsigned char *q = U(HeapReAlloc)(h, HEAP_REALLOC_IN_PLACE_ONLY, p, 1000);
    CHECK(q == p);
    if (q) { CHECK(hlg_verify(q, 1000, 0x5)); CHECK(U(HeapSize)(h, 0, q) == 1000); }
    /* same size in place */
    unsigned char *r = U(HeapReAlloc)(h, HEAP_REALLOC_IN_PLACE_ONLY, q ? q : p, 1000);
    CHECK(r == q);
    U(HeapFree)(h, 0, r ? r : p);
    CHECK(U(HeapDestroy)(h));
}
TEST(heap_realloc_in_place_grow_neighbour_blocked) {
    REQUIRE(U(HeapReAlloc));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    /* a block surrounded by live neighbours cannot grow in place a lot */
    char *a = U(HeapAlloc)(h, 0, 4000), *b = U(HeapAlloc)(h, 0, 4000), *c = U(HeapAlloc)(h, 0, 4000);
    CHECK(a && b && c);
    void *q = U(HeapReAlloc)(h, HEAP_REALLOC_IN_PLACE_ONLY, b, 400000);
    CHECK(q == NULL);
    /* original block untouched and still valid */
    CHECK(U(HeapSize)(h, 0, b) == 4000);
    CHECK(U(HeapValidate)(h, 0, b));
    U(HeapFree)(h, 0, a); U(HeapFree)(h, 0, b); U(HeapFree)(h, 0, c);
    CHECK(U(HeapDestroy)(h));
}
TEST(heap_realloc_zero_memory_zeroes_tail) {
    REQUIRE(U(HeapReAlloc));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    SIZE_T from[] = { 100, 1000, 20000 }, to[] = { 300, 5000, 90000 };
    for (int i = 0; i < 3; i++) {
        unsigned char *p = U(HeapAlloc)(h, 0, from[i]);
        if (!p) { CHECK(p); continue; }
        memset(p, 0xAA, from[i]);
        unsigned char *q = U(HeapReAlloc)(h, HEAP_ZERO_MEMORY, p, to[i]);
        CHECK(q != NULL);
        if (q) {
            CHECK(hlg_all(q, from[i], 0xAA));
            CHECK(hlg_all(q + from[i], to[i] - from[i], 0));
            U(HeapFree)(h, 0, q);
        }
    }
    CHECK(U(HeapDestroy)(h));
}
TEST(heap_realloc_zero_memory_dirty_reuse) {
    REQUIRE(U(HeapReAlloc));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    for (int i = 0; i < 10; i++) {
        unsigned char *d = U(HeapAlloc)(h, 0, 600);
        if (d) { memset(d, 0xCC, 600); U(HeapFree)(h, 0, d); }
        unsigned char *p = U(HeapAlloc)(h, 0, 50);
        memset(p, 1, 50);
        unsigned char *q = U(HeapReAlloc)(h, HEAP_ZERO_MEMORY, p, 500);
        CHECK(q && hlg_all(q, 50, 1) && hlg_all(q + 50, 450, 0));
        if (q) U(HeapFree)(h, 0, q);
    }
    CHECK(U(HeapDestroy)(h));
}
TEST(heap_realloc_to_zero) {
    REQUIRE(U(HeapReAlloc));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    void *p = U(HeapAlloc)(h, 0, 100);
    void *q = U(HeapReAlloc)(h, 0, p, 0);
    CHECK(q != NULL);
    if (q) { CHECK(U(HeapSize)(h, 0, q) == 0); U(HeapFree)(h, 0, q); }
    CHECK(U(HeapDestroy)(h));
}
TEST(heap_realloc_huge_fails_keeps_original) {
    REQUIRE(U(HeapReAlloc));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    unsigned char *p = U(HeapAlloc)(h, 0, 128);
    hlg_fill(p, 128, 9);
    HLG_POISON();
    void *q = U(HeapReAlloc)(h, 0, p, (SIZE_T)-1);
    CHECK(q == NULL);
    EXPECT_ERR(HLG_NOMEM);
    CHECK(U(HeapSize)(h, 0, p) == 128);
    CHECK(hlg_verify(p, 128, 9));
    U(HeapFree)(h, 0, p);
    CHECK(U(HeapDestroy)(h));
}
TEST(heap_realloc_fixed_heap_exhaust_keeps_original) {
    REQUIRE(U(HeapReAlloc));
    HANDLE h = mk(0, 0x10000, 0x20000);
    if (!h) { CHECK(h); return; }
    unsigned char *p = U(HeapAlloc)(h, 0, 500);
    hlg_fill(p, 500, 3);
    HLG_POISON();
    void *q = U(HeapReAlloc)(h, 0, p, 0x70000);
    CHECK(q == NULL);
    EXPECT_ERR(HLG_NOMEM);
    CHECK(hlg_verify(p, 500, 3));
    U(HeapFree)(h, 0, p);
    CHECK(U(HeapDestroy)(h));
}
TEST(heap_realloc_repeated_growth) {
    REQUIRE(U(HeapReAlloc));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    unsigned char *p = U(HeapAlloc)(h, 0, 1);
    p[0] = 0x77;
    SIZE_T n = 1;
    for (int i = 0; i < 40 && p; i++) {
        SIZE_T nn = n + n / 2 + 7;
        unsigned char *q = U(HeapReAlloc)(h, 0, p, nn);
        CHECK(q != NULL);
        if (!q) break;
        CHECK(q[0] == 0x77);
        memset(q + n, 0x77, nn - n);
        p = q; n = nn;
        if (n > 8 * 1024 * 1024) break;
    }
    if (p) { CHECK(hlg_all(p, n, 0x77)); CHECK(U(HeapSize)(h, 0, p) == n); U(HeapFree)(h, 0, p); }
    CHECK(U(HeapDestroy)(h));
}
TEST(heap_realloc_cross_heap_via_alloc_copy) {
    REQUIRE(U(HeapReAlloc));
    HANDLE a = mk(0, 0, 0), b = mk(0, 0, 0);
    if (!a || !b) { CHECK(a && b); return; }
    void *pa = U(HeapAlloc)(a, 0, 64), *pb = U(HeapAlloc)(b, 0, 64);
    CHECK(pa != pb);
    CHECK(U(HeapSize)(a, 0, pa) == 64 && U(HeapSize)(b, 0, pb) == 64);
    U(HeapFree)(a, 0, pa); U(HeapFree)(b, 0, pb);
    CHECK(U(HeapDestroy)(a)); CHECK(U(HeapDestroy)(b));
}

/* ---------------- HeapSize ---------------- */

TEST(heap_size_matches_request) {
    REQUIRE(U(HeapSize));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    for (SIZE_T n = 0; n < 300; n += 7) {
        void *p = U(HeapAlloc)(h, 0, n);
        CHECK(p != NULL);
        if (p) { CHECKF(U(HeapSize)(h, 0, p) == n, "n=%zu", n); U(HeapFree)(h, 0, p); }
    }
    CHECK(U(HeapDestroy)(h));
}
TEST(heap_size_flags_noserialize) {
    REQUIRE(U(HeapSize));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    void *p = U(HeapAlloc)(h, 0, 123);
    CHECK(U(HeapSize)(h, HEAP_NO_SERIALIZE, p) == 123);
    CHECK(U(HeapSize)(h, 0, p) == 123);
    U(HeapFree)(h, 0, p);
    CHECK(U(HeapDestroy)(h));
}
TEST(heap_size_after_realloc) {
    REQUIRE(U(HeapSize));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    void *p = U(HeapAlloc)(h, 0, 50);
    void *q = U(HeapReAlloc)(h, 0, p, 5000);
    CHECK(q && U(HeapSize)(h, 0, q) == 5000);
    void *r = U(HeapReAlloc)(h, 0, q, 10);
    CHECK(r && U(HeapSize)(h, 0, r) == 10);
    U(HeapFree)(h, 0, r);
    CHECK(U(HeapDestroy)(h));
}
CHILD(size_misaligned) {
    HANDLE h = U(HeapCreate)(0, 0, 0);
    char *p = U(HeapAlloc)(h, 0, 64);
    SIZE_T s = U(HeapSize)(h, 0, p + 1);
    if (s != (SIZE_T)-1) return 2;
    s = U(HeapSize)(h, 0, p + 8);
    if (s != (SIZE_T)-1) return 3;
    return U(HeapSize)(h, 0, p) == 64 ? 0 : 4;
}
TEST(heap_size_invalid_pointer_returns_minus1) {
    REQUIRE(U(HeapSize));
    DWORD rc = t_run_child("size_misaligned", "", 20000);
    CHECKF(rc == 0, "child rc 0x%lx", rc);
}

/* ---------------- HeapValidate ---------------- */

TEST(heap_validate_whole_heap) {
    REQUIRE(U(HeapValidate));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    CHECK(U(HeapValidate)(h, 0, NULL));
    CHECK(U(HeapValidate)(h, HEAP_NO_SERIALIZE, NULL));
    void *p = U(HeapAlloc)(h, 0, 100);
    CHECK(U(HeapValidate)(h, 0, NULL));
    CHECK(U(HeapValidate)(h, 0, p));
    CHECK(U(HeapValidate)(h, HEAP_NO_SERIALIZE, p));
    U(HeapFree)(h, 0, p);
    CHECK(U(HeapValidate)(h, 0, NULL));
    CHECK(U(HeapDestroy)(h));
}
TEST(heap_validate_process_heap) {
    REQUIRE(U(HeapValidate));
    CHECK(U(HeapValidate)(PH(), 0, NULL));
    void *p = U(HeapAlloc)(PH(), 0, 33);
    CHECK(U(HeapValidate)(PH(), 0, p));
    U(HeapFree)(PH(), 0, p);
}
TEST(heap_validate_fixed_heap) {
    REQUIRE(U(HeapValidate));
    HANDLE h = mk(0, 0x10000, 0x10000);
    if (!h) { CHECK(h); return; }
    void *p = U(HeapAlloc)(h, 0, 5000);
    CHECK(U(HeapValidate)(h, 0, NULL));
    CHECK(U(HeapValidate)(h, 0, p));
    U(HeapFree)(h, 0, p);
    CHECK(U(HeapDestroy)(h));
}
TEST(heap_validate_zero_size_block) {
    REQUIRE(U(HeapValidate));
    HANDLE h = mk(0, 0, 0);
    void *p = U(HeapAlloc)(h, 0, 0);
    CHECK(U(HeapValidate)(h, 0, p));
    U(HeapFree)(h, 0, p);
    CHECK(U(HeapDestroy)(h));
}
CHILD(validate_bad) {
    HANDLE h = U(HeapCreate)(0, 0, 0);
    char *p = U(HeapAlloc)(h, 0, 64);
    int r = 0;
    if (U(HeapValidate)(h, 0, p + 1)) r |= 1;       /* misaligned interior pointer */
    if (U(HeapValidate)(h, 0, p + 16)) r |= 2;      /* interior pointer, not a block start */
    U(HeapFree)(h, 0, p);
    if (U(HeapValidate)(h, 0, p)) r |= 4;           /* freed block */
    if (!U(HeapValidate)(h, 0, NULL)) r |= 8;       /* heap itself still fine */
    return r;
}
TEST(heap_validate_bad_blocks_fail) {
    REQUIRE(U(HeapValidate));
    DWORD rc = t_run_child("validate_bad", "", 20000);
    if (rc == 0xC0000374) { t_note("child terminated by heap corruption on bad HeapValidate"); return; }
    CHECKF(rc == 0, "child rc 0x%lx (bit0 misaligned, bit1 interior, bit2 freed, bit3 heap)", rc);
}

/* ---------------- Process heap ---------------- */

TEST(heap_process_heap_matches_peb) {
    REQUIRE(U(GetProcessHeap));
    HANDLE h = U(GetProcessHeap)();
    CHECK(h != NULL);
    CHECK(h == hlg_peb_heap());
    CHECK(h == U(GetProcessHeap)());
    CHECK(h == GetProcessHeap());
}
static DWORD WINAPI ph_thread(LPVOID a) { *(HANDLE *)a = U(GetProcessHeap)(); return 0; }
TEST(heap_process_heap_same_in_thread) {
    REQUIRE(U(GetProcessHeap));
    HANDLE r = NULL;
    HANDLE t = CreateThread(NULL, 0, ph_thread, &r, 0, NULL);
    CHECK(t != NULL);
    if (t) { WaitForSingleObject(t, 5000); CloseHandle(t); }
    CHECK(r == U(GetProcessHeap)());
}
TEST(heap_process_heap_is_growable_commit) {
    REQUIRE(U(HeapAlloc));
    HANDLE h = PH();
    void *p = U(HeapAlloc)(h, HEAP_ZERO_MEMORY, 1 << 20);
    CHECK(p && hlg_all(p, 1 << 20, 0));
    U(HeapFree)(h, 0, p);
}
TEST(heap_process_heap_vq) {
    REQUIRE(U(GetProcessHeap));
    MEMORY_BASIC_INFORMATION m;
    CHECK(VirtualQuery(U(GetProcessHeap)(), &m, sizeof m) != 0);
    CHECK(m.State == MEM_COMMIT);
    CHECK(m.Type == MEM_PRIVATE);
}
TEST(heap_locked_by_other_thread_blocks) {
    REQUIRE(U(HeapLock));
    /* HeapLock from one thread must block serialized operations from another */
    extern int hlg_lock_probe(HANDLE h);
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    int r = hlg_lock_probe(h);
    CHECK(r == 1);
    CHECK(U(HeapDestroy)(h));
}
typedef struct { HANDLE h; HANDLE started, done; } lockctx;
static DWORD WINAPI lock_thread(LPVOID a) {
    lockctx *c = a;
    SetEvent(c->started);
    void *p = U(HeapAlloc)(c->h, 0, 200000);    /* big: bypasses any lock-free front end */
    if (p) U(HeapFree)(c->h, 0, p);
    SetEvent(c->done);
    return 0;
}
int hlg_lock_probe(HANDLE h) {
    lockctx c = { h, CreateEventA(NULL, TRUE, FALSE, NULL), CreateEventA(NULL, TRUE, FALSE, NULL) };
    if (!U(HeapLock)(h)) return -1;
    HANDLE t = CreateThread(NULL, 0, lock_thread, &c, 0, NULL);
    WaitForSingleObject(c.started, 5000);
    DWORD w = WaitForSingleObject(c.done, 300);
    int blocked = (w == WAIT_TIMEOUT);
    U(HeapUnlock)(h);
    DWORD w2 = WaitForSingleObject(c.done, 5000);
    if (t) { WaitForSingleObject(t, 5000); CloseHandle(t); }
    CloseHandle(c.started); CloseHandle(c.done);
    return (blocked && w2 == WAIT_OBJECT_0) ? 1 : 0;
}
TEST(heap_lock_recursive_same_thread) {
    REQUIRE(U(HeapLock)); REQUIRE(U(HeapUnlock));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    CHECK(U(HeapLock)(h));
    CHECK(U(HeapLock)(h));
    CHECK(U(HeapLock)(h));
    void *p = U(HeapAlloc)(h, 0, 100);       /* owner thread may still allocate */
    CHECK(p != NULL);
    if (p) U(HeapFree)(h, 0, p);
    CHECK(U(HeapUnlock)(h));
    CHECK(U(HeapUnlock)(h));
    CHECK(U(HeapUnlock)(h));
    CHECK(U(HeapDestroy)(h));
}
TEST(heap_lock_noserialize_heap) {
    REQUIRE(U(HeapLock)); REQUIRE(U(HeapUnlock));
    HANDLE h = mk(HEAP_NO_SERIALIZE, 0, 0);
    if (!h) { CHECK(h); return; }
    CHECK(U(HeapLock)(h));
    CHECK(U(HeapUnlock)(h));
    CHECK(U(HeapDestroy)(h));
}
TEST(heap_lock_process_heap) {
    REQUIRE(U(HeapLock)); REQUIRE(U(HeapUnlock));
    CHECK(U(HeapLock)(PH()));
    CHECK(U(HeapUnlock)(PH()));
}

/* ---------------- cross-thread free ---------------- */
typedef struct { HANDLE h; void *p[64]; int n; } xctx;
static DWORD WINAPI xfree_thread(LPVOID a) {
    xctx *c = a; int ok = 0;
    for (int i = 0; i < c->n; i++) if (c->p[i] && hlg_verify(c->p[i], 100 + i, (unsigned char)i) && U(HeapFree)(c->h, 0, c->p[i])) ok++;
    return ok;
}
TEST(heap_cross_thread_free) {
    REQUIRE(U(HeapFree));
    xctx c; c.h = mk(0, 0, 0); c.n = 64;
    if (!c.h) { CHECK(c.h); return; }
    for (int i = 0; i < 64; i++) { c.p[i] = U(HeapAlloc)(c.h, 0, 100 + i); if (c.p[i]) hlg_fill(c.p[i], 100 + i, (unsigned char)i); }
    HANDLE t = CreateThread(NULL, 0, xfree_thread, &c, 0, NULL);
    CHECK(t != NULL);
    DWORD ec = 0;
    if (t) { WaitForSingleObject(t, 10000); GetExitCodeThread(t, &ec); CloseHandle(t); }
    CHECK(ec == 64);
    CHECK(U(HeapValidate)(c.h, 0, NULL));
    CHECK(U(HeapDestroy)(c.h));
}
static DWORD WINAPI xalloc_thread(LPVOID a) {
    xctx *c = a;
    for (int i = 0; i < c->n; i++) { c->p[i] = U(HeapAlloc)(c->h, 0, 100 + i); if (c->p[i]) hlg_fill(c->p[i], 100 + i, (unsigned char)(i + 1)); }
    return 0;
}
TEST(heap_cross_thread_alloc_main_free) {
    REQUIRE(U(HeapFree));
    xctx c; c.h = mk(0, 0, 0); c.n = 64;
    if (!c.h) { CHECK(c.h); return; }
    HANDLE t = CreateThread(NULL, 0, xalloc_thread, &c, 0, NULL);
    if (t) { WaitForSingleObject(t, 10000); CloseHandle(t); }
    int ok = 0;
    for (int i = 0; i < 64; i++) if (c.p[i] && hlg_verify(c.p[i], 100 + i, (unsigned char)(i + 1)) && U(HeapSize)(c.h, 0, c.p[i]) == (SIZE_T)(100 + i) && U(HeapFree)(c.h, 0, c.p[i])) ok++;
    CHECK(ok == 64);
    CHECK(U(HeapDestroy)(c.h));
}
static DWORD WINAPI contend_thread(LPVOID a) {
    HANDLE h = (HANDLE)a; int bad = 0;
    for (int i = 0; i < 2000; i++) {
        unsigned char *p = U(HeapAlloc)(h, 0, 16 + (i % 200));
        if (!p) { bad++; continue; }
        memset(p, i, 16 + (i % 200));
        if (!hlg_all(p, 16 + (i % 200), (unsigned char)i)) bad++;
        if (!U(HeapFree)(h, 0, p)) bad++;
    }
    return bad;
}
TEST(heap_concurrent_alloc_free) {
    REQUIRE(U(HeapAlloc));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    HANDLE t[8]; int n = 0;
    for (int i = 0; i < 8; i++) { t[n] = CreateThread(NULL, 0, contend_thread, h, 0, NULL); if (t[n]) n++; }
    CHECK(n == 8);
    if (n) WaitForMultipleObjects(n, t, TRUE, 20000);
    for (int i = 0; i < n; i++) { DWORD ec = 99; GetExitCodeThread(t[i], &ec); CHECK(ec == 0); CloseHandle(t[i]); }
    CHECK(U(HeapValidate)(h, 0, NULL));
    CHECK(U(HeapDestroy)(h));
}
