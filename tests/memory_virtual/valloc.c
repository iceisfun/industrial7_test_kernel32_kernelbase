#include "mvc.h"
/* VirtualAlloc / VirtualFree */

TEST(mv_va_zero_size) {
    REQUIRE(VALLOC);
    DWORD types[] = { MEM_COMMIT, MEM_RESERVE, MEM_COMMIT | MEM_RESERVE };
    for (int i = 0; i < 3; i++) {
        SET_ERR(0);
        CHECK(VALLOC(NULL, 0, types[i], PAGE_READWRITE) == NULL);
        ERR1(ERROR_INVALID_PARAMETER);
    }
}
TEST(mv_va_no_type) {
    REQUIRE(VALLOC);
    SET_ERR(0);
    CHECK(VALLOC(NULL, PG, 0, PAGE_READWRITE) == NULL);
    ERR1(ERROR_INVALID_PARAMETER);
}
TEST(mv_va_below_min_address) {
    REQUIRE(VALLOC);
    /* kernelbase rejects non-NULL lpAddress below the minimum application address before calling ntdll */
    ULONG_PTR addrs[] = { 1, 0x1000, 0x8000, 0xFFFF };
    for (int i = 0; i < 4; i++) {
        SET_ERR(0);
        CHECK(VALLOC((void *)addrs[i], PG, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) == NULL);
        ERR1(ERROR_INVALID_PARAMETER);
    }
}
TEST(mv_va_invalid_protect) {
    REQUIRE(VALLOC);
    DWORD bad[] = { 0, 0x03, 0x05, 0x06, 0x0C, 0x30, 0x100, 0x101, 0x200, 0x400, 0x201, 0x300 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SET_ERR(0);
        void *p = VALLOC(NULL, PG, MEM_RESERVE | MEM_COMMIT, bad[i]);
        CHECKF(p == NULL, "protect 0x%lx", bad[i]);
        if (p) mv_free(p); else ERR1(ERROR_INVALID_PARAMETER);
    }
}
TEST(mv_va_writecopy_private_invalid) {
    REQUIRE(VALLOC);
    DWORD bad[] = { PAGE_WRITECOPY, PAGE_EXECUTE_WRITECOPY };
    for (int i = 0; i < 2; i++) {
        SET_ERR(0);
        void *p = VALLOC(NULL, PG, MEM_RESERVE | MEM_COMMIT, bad[i]);
        CHECKF(p == NULL, "protect 0x%lx", bad[i]);
        if (p) mv_free(p); else ERR1(ERROR_INVALID_PARAMETER);
    }
}
TEST(mv_va_valid_protects) {
    REQUIRE(VALLOC); REQUIRE(VQ);
    DWORD ok[] = { PAGE_NOACCESS, PAGE_READONLY, PAGE_READWRITE, PAGE_EXECUTE, PAGE_EXECUTE_READ, PAGE_EXECUTE_READWRITE,
                   PAGE_READONLY | PAGE_GUARD, PAGE_READWRITE | PAGE_GUARD, PAGE_EXECUTE_READ | PAGE_GUARD,
                   PAGE_READWRITE | PAGE_NOCACHE, PAGE_READWRITE | PAGE_WRITECOMBINE, PAGE_READONLY | PAGE_NOCACHE };
    for (unsigned i = 0; i < sizeof ok / sizeof *ok; i++) {
        SET_ERR(0);
        void *p = VALLOC(NULL, PG, MEM_RESERVE | MEM_COMMIT, ok[i]);
        CHECKF(p != NULL, "protect 0x%lx err %lu", ok[i], GetLastError());
        if (!p) continue;
        MEMORY_BASIC_INFORMATION m = mv_q(p);
        if (ok[i] & (PAGE_NOCACHE | PAGE_WRITECOMBINE)) {
            EXPECT_EQ(m.Protect & 0xFF, ok[i] & 0xFF);
            if (m.Protect != ok[i]) t_note("protect 0x%lx reported as 0x%lx", ok[i], m.Protect);
        } else {
            EXPECT_EQ(m.Protect, ok[i]);
            EXPECT_EQ(m.AllocationProtect, ok[i]);
        }
        EXPECT_EQ(m.State, MEM_COMMIT);
        EXPECT_EQ(m.Type, MEM_PRIVATE);
        mv_free(p);
    }
}
TEST(mv_va_basic_commit) {
    REQUIRE(VALLOC); REQUIRE(VQ);
    unsigned char *p = VALLOC(NULL, 1, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    CHECK(p != NULL);
    if (!p) return;
    CHECK(((ULONG_PTR)p & (GR - 1)) == 0);
    MEMORY_BASIC_INFORMATION m = mv_q(p);
    EXPECT_EQ(m.BaseAddress, p);
    EXPECT_EQ(m.AllocationBase, p);
    EXPECT_EQ(m.RegionSize, PG);
    EXPECT_EQ(m.State, MEM_COMMIT);
    EXPECT_EQ(m.Type, MEM_PRIVATE);
    EXPECT_EQ(m.Protect, PAGE_READWRITE);
    EXPECT_EQ(m.AllocationProtect, PAGE_READWRITE);
    CHECK(p[0] == 0 && p[PG - 1] == 0);
    p[0] = 0xAA; p[PG - 1] = 0xBB;
    CHECK(p[0] == 0xAA && p[PG - 1] == 0xBB);
    mv_free(p);
}
TEST(mv_va_size_rounding) {
    REQUIRE(VALLOC); REQUIRE(VQ);
    struct { SIZE_T req, region; } t[] = { {1, PG}, {PG - 1, PG}, {PG, PG}, {PG + 1, 2 * PG}, {GR - 1, GR}, {GR, GR}, {GR + 1, GR + PG}, {3 * PG, 3 * PG} };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        void *p = VALLOC(NULL, t[i].req, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        CHECKF(p != NULL, "size %Iu", t[i].req);
        if (!p) continue;
        MEMORY_BASIC_INFORMATION m = mv_q(p);
        CHECKF(m.RegionSize == t[i].region, "req %Iu region %Iu want %Iu", t[i].req, m.RegionSize, t[i].region);
        mv_free(p);
    }
}
TEST(mv_va_reserve_only) {
    REQUIRE(VALLOC); REQUIRE(VQ);
    unsigned char *p = VALLOC(NULL, 4 * PG, MEM_RESERVE, PAGE_READWRITE);
    CHECK(p != NULL);
    if (!p) return;
    CHECK(((ULONG_PTR)p & (GR - 1)) == 0);
    MEMORY_BASIC_INFORMATION m = mv_q(p);
    EXPECT_EQ(m.State, MEM_RESERVE);
    EXPECT_EQ(m.Protect, 0);
    EXPECT_EQ(m.AllocationProtect, PAGE_READWRITE);
    EXPECT_EQ(m.Type, MEM_PRIVATE);
    EXPECT_EQ(m.RegionSize, 4 * PG);
    EXPECT_EQ(mv_rd(p, NULL), EXCEPTION_ACCESS_VIOLATION);
    EXPECT_EQ(mv_wr(p, 1), EXCEPTION_ACCESS_VIOLATION);
    mv_free(p);
}
TEST(mv_va_reserve_then_commit) {
    REQUIRE(VALLOC); REQUIRE(VQ);
    unsigned char *p = VALLOC(NULL, 8 * PG, MEM_RESERVE, PAGE_NOACCESS);
    CHECK(p != NULL);
    if (!p) return;
    unsigned char *c = VALLOC(p + 2 * PG, 2 * PG, MEM_COMMIT, PAGE_READWRITE);
    EXPECT_EQ(c, p + 2 * PG);
    MEMORY_BASIC_INFORMATION m = mv_q(p);
    EXPECT_EQ(m.State, MEM_RESERVE); EXPECT_EQ(m.RegionSize, 2 * PG);
    m = mv_q(p + 2 * PG);
    EXPECT_EQ(m.State, MEM_COMMIT); EXPECT_EQ(m.RegionSize, 2 * PG); EXPECT_EQ(m.Protect, PAGE_READWRITE);
    EXPECT_EQ(m.AllocationBase, p); EXPECT_EQ(m.AllocationProtect, PAGE_NOACCESS);
    m = mv_q(p + 4 * PG);
    EXPECT_EQ(m.State, MEM_RESERVE); EXPECT_EQ(m.RegionSize, 4 * PG);
    EXPECT_EQ(mv_wr(p + 2 * PG, 7), 0);
    EXPECT_EQ(mv_wr(p + 4 * PG, 7), EXCEPTION_ACCESS_VIOLATION);
    EXPECT_EQ(mv_info0, 1);                 /* write AV */
    EXPECT_EQ(mv_info1, (ULONG_PTR)(p + 4 * PG));
    EXPECT_EQ(mv_rd(p + PG, NULL), EXCEPTION_ACCESS_VIOLATION);
    EXPECT_EQ(mv_info0, 0);                 /* read AV */
    mv_free(p);
}
TEST(mv_va_commit_unaligned_address) {
    REQUIRE(VALLOC);
    unsigned char *p = VALLOC(NULL, 4 * PG, MEM_RESERVE, PAGE_READWRITE);
    CHECK(p != NULL);
    if (!p) return;
    void *c = VALLOC(p + PG + 0x234, 1, MEM_COMMIT, PAGE_READWRITE);
    EXPECT_EQ(c, p + PG);
    MEMORY_BASIC_INFORMATION m = mv_q(p + PG);
    EXPECT_EQ(m.State, MEM_COMMIT); EXPECT_EQ(m.RegionSize, PG);
    /* size spanning a page boundary commits both pages */
    c = VALLOC(p + 2 * PG + PG - 1, 2, MEM_COMMIT, PAGE_READWRITE);
    EXPECT_EQ(c, p + 2 * PG);
    m = mv_q(p + 2 * PG);
    EXPECT_EQ(m.State, MEM_COMMIT); EXPECT_EQ(m.RegionSize, 2 * PG);
    mv_free(p);
}
TEST(mv_va_recommit_changes_protect) {
    REQUIRE(VALLOC); REQUIRE(VQ);
    unsigned char *p = mv_alloc(2 * PG);
    CHECK(p != NULL);
    if (!p) return;
    p[10] = 0x5A;
    void *c = VALLOC(p, PG, MEM_COMMIT, PAGE_READONLY);
    EXPECT_EQ(c, p);
    MEMORY_BASIC_INFORMATION m = mv_q(p);
    EXPECT_EQ(m.Protect, PAGE_READONLY); EXPECT_EQ(m.RegionSize, PG);
    EXPECT_EQ(p[10], 0x5A);                 /* committing already-committed pages does not clear them */
    EXPECT_EQ(mv_wr(p, 1), EXCEPTION_ACCESS_VIOLATION);
    mv_free(p);
}
TEST(mv_va_commit_without_reserve_fixed) {
    REQUIRE(VALLOC);
    void *h = mv_hole(GR);
    CHECK(h != NULL);
    if (!h) return;
    SET_ERR(0);
    void *p = VALLOC(h, PG, MEM_COMMIT, PAGE_READWRITE);
    if (p) { t_note("MEM_COMMIT on free address succeeded"); mv_free(p); }
    else ERR1(ERROR_INVALID_ADDRESS);
}
TEST(mv_va_reserve_fixed_roundtrip) {
    REQUIRE(VALLOC); REQUIRE(VQ);
    void *h = mv_hole(4 * GR);
    CHECK(h != NULL);
    if (!h) return;
    void *p = VALLOC(h, GR, MEM_RESERVE, PAGE_READWRITE);
    if (!p) { t_note("hole reuse raced"); return; }
    EXPECT_EQ(p, h);
    /* reserving at the same address again conflicts */
    SET_ERR(0);
    void *q = VALLOC(h, GR, MEM_RESERVE, PAGE_READWRITE);
    CHECK(q == NULL);
    ERR1(ERROR_INVALID_ADDRESS);
    /* overlapping partially also conflicts */
    SET_ERR(0);
    q = VALLOC((char *)h + PG, PG, MEM_RESERVE, PAGE_READWRITE);
    CHECK(q == NULL);
    ERR1(ERROR_INVALID_ADDRESS);
    mv_free(p);
}
TEST(mv_va_reserve_address_rounds_down_to_granularity) {
    REQUIRE(VALLOC); REQUIRE(VQ);
    void *h = mv_hole(4 * GR);
    CHECK(h != NULL);
    if (!h) return;
    void *p = VALLOC((char *)h + PG, 2 * PG, MEM_RESERVE, PAGE_READWRITE);
    if (!p) { t_note("hole reuse raced err=%lu", GetLastError()); return; }
    EXPECT_EQ(p, h);
    MEMORY_BASIC_INFORMATION m = mv_q(h);
    EXPECT_EQ(m.AllocationBase, h);
    EXPECT_EQ(m.State, MEM_RESERVE);
    t_note("reserve [h+0x1000,+0x2000) RegionSize=0x%Ix", m.RegionSize);
    mv_free(p);
}
TEST(mv_va_reserve_commit_fixed_unaligned) {
    REQUIRE(VALLOC);
    void *h = mv_hole(4 * GR);
    CHECK(h != NULL);
    if (!h) return;
    void *p = VALLOC((char *)h + 0x1800, PG, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!p) { t_note("race err=%lu", GetLastError()); return; }
    EXPECT_EQ(p, h);                        /* base rounded down to the allocation granularity */
    mv_free(p);
}
TEST(mv_va_huge_sizes) {
    REQUIRE(VALLOC);
    SIZE_T sizes[] = { (SIZE_T)-1, (SIZE_T)-1 - 0xFFF, (SIZE_T)1 << 63, (SIZE_T)1 << 47, (SIZE_T)0x7FFFFFFF0000ull, (SIZE_T)1 << 46 };
    for (unsigned i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        SET_ERR(0);
        void *p = VALLOC(NULL, sizes[i], MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        CHECKF(p == NULL, "size 0x%Ix", sizes[i]);
        if (p) { mv_free(p); continue; }
        ERR3(ERROR_NOT_ENOUGH_MEMORY, ERROR_INVALID_PARAMETER, ERROR_COMMITMENT_LIMIT);
    }
}
TEST(mv_va_huge_commit_limit) {
    REQUIRE(VALLOC);
    /* absurd commit request that no machine can satisfy (reserve of that size is fine, commit is not) */
    SET_ERR(0);
    void *p = VALLOC(NULL, (SIZE_T)0x3FFF00000000ull, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    CHECK(p == NULL);
    if (p) { mv_free(p); return; }
    ERR3(ERROR_NOT_ENOUGH_MEMORY, ERROR_COMMITMENT_LIMIT, ERROR_INVALID_PARAMETER);
}
TEST(mv_va_reserve_large_then_commit_partial) {
    REQUIRE(VALLOC); REQUIRE(VQ);
    SIZE_T sz = (SIZE_T)256 << 20;
    char *p = VALLOC(NULL, sz, MEM_RESERVE, PAGE_READWRITE);
    CHECK(p != NULL);
    if (!p) return;
    MEMORY_BASIC_INFORMATION m = mv_q(p);
    EXPECT_EQ(m.RegionSize, sz);
    char *c = VALLOC(p + (128 << 20), 3 * PG, MEM_COMMIT, PAGE_READWRITE);
    EXPECT_EQ(c, p + (128 << 20));
    EXPECT_EQ(mv_wr(c + 2 * PG, 9), 0);
    mv_free(p);
}
TEST(mv_va_top_down) {
    REQUIRE(VALLOC);
    void *a = VALLOC(NULL, GR, MEM_RESERVE | MEM_TOP_DOWN, PAGE_READWRITE);
    void *b = VALLOC(NULL, GR, MEM_RESERVE | MEM_TOP_DOWN, PAGE_READWRITE);
    CHECK(a != NULL && b != NULL);
    CHECK(((ULONG_PTR)a & (GR - 1)) == 0);
    CHECK(((ULONG_PTR)b & (GR - 1)) == 0);
    if (a && b) t_note("top-down %p then %p (descending=%d)", a, b, (ULONG_PTR)b < (ULONG_PTR)a);
    void *c = VALLOC(NULL, GR, MEM_RESERVE, PAGE_READWRITE);
    if (a && c) t_note("normal %p vs topdown %p", c, a);
    mv_free(a); mv_free(b); mv_free(c);
}
TEST(mv_va_write_watch_flag_rules) {
    REQUIRE(VALLOC); REQUIRE(VQ);
    void *p = VALLOC(NULL, 4 * PG, MEM_RESERVE | MEM_COMMIT | MEM_WRITE_WATCH, PAGE_READWRITE);
    CHECK(p != NULL);
    mv_free(p);
    SET_ERR(0);
    p = VALLOC(NULL, 4 * PG, MEM_COMMIT | MEM_WRITE_WATCH, PAGE_READWRITE);
    if (p) { t_note("WRITE_WATCH without RESERVE succeeded"); mv_free(p); }
    else ERR1(ERROR_INVALID_PARAMETER);
}
TEST(mv_va_large_pages_needs_privilege) {
    REQUIRE(VALLOC); REQUIRE(U(GetLargePageMinimum));
    SIZE_T lp = U(GetLargePageMinimum)();
    if (!lp) { t_skip("large pages unsupported"); return; }
    SET_ERR(0);
    void *p = VALLOC(NULL, lp, MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES, PAGE_READWRITE);
    if (p) { t_note("large page allocation succeeded (privilege held)"); mv_free(p); return; }
    ERR3(ERROR_PRIVILEGE_NOT_HELD, ERROR_NOT_ENOUGH_MEMORY, ERROR_INVALID_PARAMETER);
    /* without MEM_RESERVE|MEM_COMMIT both, large pages request is invalid */
    SET_ERR(0);
    p = VALLOC(NULL, lp, MEM_RESERVE | MEM_LARGE_PAGES, PAGE_READWRITE);
    if (p) { mv_free(p); return; }
    ERR2(ERROR_INVALID_PARAMETER, ERROR_PRIVILEGE_NOT_HELD);
}
TEST(mv_va_large_pages_bad_size) {
    REQUIRE(VALLOC); REQUIRE(U(GetLargePageMinimum));
    SIZE_T lp = U(GetLargePageMinimum)();
    if (!lp) { t_skip("large pages unsupported"); return; }
    SET_ERR(0);
    void *p = VALLOC(NULL, PG, MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES, PAGE_READWRITE);
    CHECK(p == NULL);
    if (p) { mv_free(p); return; }
    ERR3(ERROR_INVALID_PARAMETER, ERROR_PRIVILEGE_NOT_HELD, ERROR_NOT_ENOUGH_MEMORY);
}
TEST(mv_va_physical_flag_rules) {
    REQUIRE(VALLOC);
    SET_ERR(0);
    void *p = VALLOC(NULL, PG, MEM_PHYSICAL | MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    CHECK(p == NULL);
    if (p) mv_free(p); else ERR2(ERROR_INVALID_PARAMETER, ERROR_PRIVILEGE_NOT_HELD);
    SET_ERR(0);
    p = VALLOC(NULL, PG, MEM_PHYSICAL | MEM_RESERVE, PAGE_READONLY);
    CHECK(p == NULL);
    if (p) mv_free(p); else ERR2(ERROR_INVALID_PARAMETER, ERROR_PRIVILEGE_NOT_HELD);
    SET_ERR(0);
    p = VALLOC(NULL, PG, MEM_PHYSICAL | MEM_RESERVE, PAGE_READWRITE);
    if (p) { t_note("MEM_PHYSICAL reserve succeeded (AWE privilege held)"); mv_free(p); }
    else ERR2(ERROR_PRIVILEGE_NOT_HELD, ERROR_INVALID_PARAMETER);
}
TEST(mv_va_reset_basic) {
    REQUIRE(VALLOC); REQUIRE(VQ);
    unsigned char *p = mv_alloc(4 * PG);
    CHECK(p != NULL);
    if (!p) return;
    memset(p, 0x77, 4 * PG);
    SET_ERR(0);
    void *r = VALLOC(p, 4 * PG, MEM_RESET, PAGE_NOACCESS);
    EXPECT_EQ(r, p);
    MEMORY_BASIC_INFORMATION m = mv_q(p);
    EXPECT_EQ(m.State, MEM_COMMIT); EXPECT_EQ(m.Protect, PAGE_READWRITE);
    EXPECT_EQ(m.RegionSize, 4 * PG);
    /* still accessible; contents are indeterminate */
    EXPECT_EQ(mv_wr(p, 1), 0);
    mv_free(p);
}
TEST(mv_va_reset_invalid) {
    REQUIRE(VALLOC);
    unsigned char *p = mv_alloc(2 * PG);
    CHECK(p != NULL);
    if (!p) return;
    SET_ERR(0);
    CHECK(VALLOC(p, PG, MEM_RESET | MEM_COMMIT, PAGE_READWRITE) == NULL);
    ERR1(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(VALLOC(p, PG, MEM_RESET | MEM_RESERVE, PAGE_READWRITE) == NULL);
    ERR1(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(VALLOC(p, 0, MEM_RESET, PAGE_READWRITE) == NULL);
    ERR1(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(VALLOC(p, PG, MEM_RESET | MEM_RESET_UNDO, PAGE_READWRITE) == NULL);
    ERR1(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(VALLOC(p, PG, MEM_RESET_UNDO | MEM_COMMIT, PAGE_READWRITE) == NULL);
    ERR1(ERROR_INVALID_PARAMETER);
    mv_free(p);
}
TEST(mv_va_reset_undo_roundtrip) {
    REQUIRE(VALLOC);
    unsigned char *p = mv_alloc(2 * PG);
    CHECK(p != NULL);
    if (!p) return;
    CHECK(VALLOC(p, 2 * PG, MEM_RESET, PAGE_READWRITE) == p);
    SET_ERR(0);
    void *u = VALLOC(p, 2 * PG, MEM_RESET_UNDO, PAGE_READWRITE);
    EXPECT_EQ(u, p);
    mv_free(p);
}
TEST(mv_va_reset_on_reserved_only) {
    REQUIRE(VALLOC);
    void *p = VALLOC(NULL, GR, MEM_RESERVE, PAGE_READWRITE);
    CHECK(p != NULL);
    if (!p) return;
    SET_ERR(0);
    void *r = VALLOC(p, PG, MEM_RESET, PAGE_READWRITE);
    t_note("MEM_RESET on reserved page: result=%p err=%lu", r, GetLastError());
    mv_free(p);
}
TEST(mv_va_memory_is_zeroed) {
    REQUIRE(VALLOC);
    for (int i = 0; i < 20; i++) {
        unsigned char *p = mv_alloc(16 * PG);
        CHECK(p != NULL);
        if (!p) return;
        int nz = 0;
        for (size_t k = 0; k < 16 * PG; k += 61) nz |= p[k];
        CHECK(nz == 0);
        memset(p, 0xFF, 16 * PG);
        mv_free(p);
    }
}
TEST(mv_va_distinct_regions) {
    REQUIRE(VALLOC);
    void *p[16];
    for (int i = 0; i < 16; i++) { p[i] = mv_alloc(PG); CHECK(p[i] != NULL); }
    for (int i = 0; i < 16; i++) for (int j = i + 1; j < 16; j++) CHECK(p[i] != p[j]);
    for (int i = 0; i < 16; i++) mv_free(p[i]);
}
TEST(mv_va_alloc_consumes_granule) {
    REQUIRE(VALLOC);
    /* two consecutive one-page allocations are at least one allocation granule apart */
    char *a = mv_alloc(1), *b = mv_alloc(1);
    CHECK(a && b);
    if (a && b) { ULONG_PTR d = a > b ? a - b : b - a; CHECK(d >= GR); }
    mv_free(a); mv_free(b);
}

/* ---------------- VirtualFree ---------------- */
TEST(mv_vf_release_basic) {
    REQUIRE(VALLOC); REQUIRE(VFREE); REQUIRE(VQ);
    void *p = mv_alloc(PG);
    CHECK(p != NULL);
    SET_ERR(0);
    CHECK(VFREE(p, 0, MEM_RELEASE));
    MEMORY_BASIC_INFORMATION m = mv_q(p);
    EXPECT_EQ(m.State, MEM_FREE);
    EXPECT_EQ(mv_rd(p, NULL), EXCEPTION_ACCESS_VIOLATION);
}
TEST(mv_vf_double_release) {
    REQUIRE(VFREE);
    void *p = mv_alloc(PG);
    CHECK(p != NULL);
    CHECK(VFREE(p, 0, MEM_RELEASE));
    SET_ERR(0);
    CHECK(!VFREE(p, 0, MEM_RELEASE));
    ERR1(ERROR_INVALID_ADDRESS);
}
TEST(mv_vf_release_with_size_invalid) {
    REQUIRE(VFREE);
    void *p = mv_alloc(2 * PG);
    CHECK(p != NULL);
    SET_ERR(0);
    CHECK(!VFREE(p, PG, MEM_RELEASE));
    ERR1(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!VFREE(p, 2 * PG, MEM_RELEASE));
    ERR1(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!VFREE(p, (SIZE_T)-1, MEM_RELEASE));
    ERR1(ERROR_INVALID_PARAMETER);
    CHECK(VFREE(p, 0, MEM_RELEASE));
}
TEST(mv_vf_bad_type) {
    REQUIRE(VFREE);
    void *p = mv_alloc(PG);
    CHECK(p != NULL);
    DWORD bad[] = { 0, MEM_COMMIT, MEM_RESERVE, MEM_RESET, MEM_DECOMMIT | MEM_RELEASE, 0x10000, 0x80000000, MEM_RELEASE | MEM_TOP_DOWN, 0x4, 0x8 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SET_ERR(0);
        BOOL r = VFREE(p, 0, bad[i]);
        CHECKF(!r, "type 0x%lx", bad[i]);
        if (r) { p = mv_alloc(PG); continue; }
        ERR1(ERROR_INVALID_PARAMETER);
    }
    mv_free(p);
}
TEST(mv_vf_release_non_base) {
    REQUIRE(VFREE);
    char *p = mv_alloc(4 * PG);
    CHECK(p != NULL);
    SET_ERR(0);
    CHECK(!VFREE(p + PG, 0, MEM_RELEASE));
    ERR2(ERROR_INVALID_PARAMETER, ERROR_INVALID_ADDRESS);
    /* p+1 is page-aligned down by the kernel and may legitimately free the
       allocation on Windows, so it is not asserted. */
    CHECK(VFREE(p, 0, MEM_RELEASE));
}
TEST(mv_vf_release_null_and_garbage) {
    REQUIRE(VFREE);
    SET_ERR(0);
    CHECK(!VFREE(NULL, 0, MEM_RELEASE));
    ERR2(ERROR_INVALID_ADDRESS, ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!VFREE((void *)0x10000, 0, MEM_RELEASE));
    ERR2(ERROR_INVALID_ADDRESS, ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!VFREE((void *)0x7FFFFFFF0000ull, 0, MEM_RELEASE));
    ERR2(ERROR_INVALID_ADDRESS, ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!VFREE((void *)(ULONG_PTR)-1, 0, MEM_RELEASE));
    ERR2(ERROR_INVALID_ADDRESS, ERROR_INVALID_PARAMETER);
}
TEST(mv_vf_decommit_ranges) {
    REQUIRE(VFREE); REQUIRE(VQ);
    char *p = mv_alloc(8 * PG);
    CHECK(p != NULL);
    if (!p) return;
    p[3 * PG] = 5;
    SET_ERR(0);
    CHECK(VFREE(p + 2 * PG, 3 * PG, MEM_DECOMMIT));
    MEMORY_BASIC_INFORMATION m = mv_q(p);
    EXPECT_EQ(m.State, MEM_COMMIT); EXPECT_EQ(m.RegionSize, 2 * PG);
    m = mv_q(p + 2 * PG);
    EXPECT_EQ(m.State, MEM_RESERVE); EXPECT_EQ(m.RegionSize, 3 * PG); EXPECT_EQ(m.Protect, 0);
    EXPECT_EQ(m.AllocationProtect, PAGE_READWRITE);
    m = mv_q(p + 5 * PG);
    EXPECT_EQ(m.State, MEM_COMMIT); EXPECT_EQ(m.RegionSize, 3 * PG);
    EXPECT_EQ(mv_rd(p + 3 * PG, NULL), EXCEPTION_ACCESS_VIOLATION);
    EXPECT_EQ(mv_rd(p + 2 * PG, NULL), EXCEPTION_ACCESS_VIOLATION);
    EXPECT_EQ(mv_wr(p + PG, 1), 0);
    /* recommit gives zero page again */
    CHECK(VALLOC(p + 3 * PG, PG, MEM_COMMIT, PAGE_READWRITE) == p + 3 * PG);
    EXPECT_EQ(p[3 * PG], 0);
    mv_free(p);
}
