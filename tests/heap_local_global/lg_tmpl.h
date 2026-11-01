/* Template instantiated for the Local* and Global* families.
 * Before including define: K_IS_LOCAL (1/0), K_P (name prefix token), K_ALLOC, K_REALLOC, K_FREE,
 * K_LOCK, K_UNLOCK, K_FLAGS, K_SIZE, K_HANDLE, and K_DISCARDABLE / K_DISCMASK. */
#include "hlg.h"

#define CAT_(a, b) a##b
#define CAT(a, b) CAT_(a, b)
#define U_(x) U(x)
#define TEST_(n) TEST(n)
#define TN(x) CAT(K_P, x)
#define AL(f, n)       U_(K_ALLOC)((f), (n))
#define RE(h, n, f)    U_(K_REALLOC)((h), (n), (f))
#define FR(h)          U_(K_FREE)(h)
#define LK(h)          U_(K_LOCK)(h)
#define UL(h)          U_(K_UNLOCK)(h)
#define FL(h)          U_(K_FLAGS)(h)
#define SZ(h)          U_(K_SIZE)(h)
#define HD(p)          U_(K_HANDLE)(p)
#define MOVE 0x2
#define ZINIT 0x40
#define MODIFY 0x80

#define NEED() do { if (!U_(K_ALLOC) || !U_(K_FREE) || !U_(K_LOCK) || !U_(K_UNLOCK) || !U_(K_FLAGS) || !U_(K_SIZE) || !U_(K_REALLOC) || !U_(K_HANDLE)) { t_skip("missing export"); return; } } while (0)

static const SIZE_T CAT(K_P, _sizes)[] = { 0, 1, 2, 7, 8, 9, 15, 16, 17, 24, 31, 32, 33, 63, 64, 100, 255, 256, 1000, 1024, 4000, 4096, 4097, 8192, 16000, 32768, 65535, 65536, 100000, 500000, 1 << 20, 3 << 20 };

/* ---- alloc: fixed ---- */
TEST_(TN(_alloc_fixed_basic)) {
    NEED();
    void *p = AL(0, 100);
    CHECK(p != NULL);
    if (!p) return;
    CHECK(((ULONG_PTR)p & 8) == 0);
    CHECK(((ULONG_PTR)p & 0xF) == 0);
    memset(p, 0x5A, 100);
    EXPECT_EQ(SZ(p), 100);
    EXPECT_EQ(FL(p), 0);
    CHECK(HD(p) == p);
    CHECK(FR(p) == NULL);
}
TEST_(TN(_alloc_fixed_zeroinit)) {
    NEED();
    for (int i = 0; i < 20; i++) {
        unsigned char *d = AL(0, 3000);
        if (d) { memset(d, 0xEE, 3000); FR(d); }
        unsigned char *p = AL(ZINIT, 3000);
        CHECK(p && hlg_all(p, 3000, 0));
        if (p) FR(p);
    }
}
TEST_(TN(_alloc_fixed_ptr_flags_pointer_macro)) {
    NEED();
    /* LPTR / GPTR */
    unsigned char *p = AL(0x40, 513);
    CHECK(p && hlg_all(p, 513, 0));
    if (p) { CHECK(SZ(p) == 513); FR(p); }
}
TEST_(TN(_alloc_fixed_distinct)) {
    NEED();
    void *p[64];
    for (int i = 0; i < 64; i++) { p[i] = AL(0, 24 + i); CHECK(p[i] != NULL); }
    for (int i = 0; i < 64; i++) for (int j = i + 1; j < 64; j++) CHECK(p[i] != p[j]);
    for (int i = 0; i < 64; i++) if (p[i]) CHECK(FR(p[i]) == NULL);
}
TEST_(TN(_alloc_fixed_pointer_usable_as_heap_block)) {
    NEED();
    void *p = AL(0, 321);
    CHECK(p != NULL);
    if (!p) return;
    CHECK(U(HeapValidate)(GetProcessHeap(), 0, p));
    EXPECT_EQ(U(HeapSize)(GetProcessHeap(), 0, p), 321);
    CHECK(FR(p) == NULL);
}
/* ---- alloc: moveable ---- */
TEST_(TN(_alloc_moveable_basic)) {
    NEED();
    HANDLE h = AL(MOVE, 200);
    CHECK(h != NULL);
    if (!h) return;
    CHECK(((ULONG_PTR)h & 8) != 0);
    EXPECT_EQ(SZ(h), 200);
    EXPECT_EQ(FL(h), 0);
    unsigned char *p = LK(h);
    CHECK(p != NULL);
    CHECK((void *)p != (void *)h);
    CHECK(((ULONG_PTR)p & 0xF) == 0);
    if (p) { hlg_fill(p, 200, 3); CHECK(hlg_verify(p, 200, 3)); }
    EXPECT_EQ(FL(h) & 0xFF, 1);
    CHECK(HD(p) == h);
    SET_ERR(0x1234);
    CHECK(!UL(h));
    EXPECT_ERR(0);
    CHECK(FR(h) == NULL);
}
TEST_(TN(_alloc_moveable_zeroinit)) {
    NEED();
    for (int i = 0; i < 10; i++) {
        HANDLE d = AL(MOVE, 2000);
        unsigned char *dp = d ? LK(d) : NULL;
        if (dp) memset(dp, 0xEE, 2000);
        if (d) { UL(d); FR(d); }
        HANDLE h = AL(MOVE | ZINIT, 2000);
        unsigned char *p = h ? LK(h) : NULL;
        CHECK(p && hlg_all(p, 2000, 0));
        if (h) { UL(h); FR(h); }
    }
}
TEST_(TN(_alloc_moveable_handle_stable_pointer)) {
    NEED();
    HANDLE h = AL(MOVE, 64);
    if (!h) { CHECK(h); return; }
    void *a = LK(h), *b = LK(h), *c = LK(h);
    CHECK(a && a == b && b == c);
    EXPECT_EQ(FL(h) & 0xFF, 3);
    UL(h); UL(h); UL(h);
    FR(h);
}
TEST_(TN(_alloc_moveable_many_handles)) {
    NEED();
    enum { N = 2000 };
    static HANDLE hs[N];
    int ok = 0;
    for (int i = 0; i < N; i++) {
        hs[i] = AL(MOVE, 16 + (i % 50));
        if (hs[i]) { ok++; unsigned char *p = LK(hs[i]); if (p) { memset(p, i, 16 + (i % 50)); UL(hs[i]); } }
    }
    CHECK(ok == N);
    for (int i = 0; i < N; i++) for (int j = i + 1; j < i + 6 && j < N; j++) if (hs[i] && hs[j]) CHECK(hs[i] != hs[j]);
    int good = 0;
    for (int i = 0; i < N; i++) if (hs[i]) {
        unsigned char *p = LK(hs[i]);
        if (p && hlg_all(p, 16 + (i % 50), (unsigned char)i)) good++;
        UL(hs[i]);
    }
    CHECK(good == N);
    for (int i = 0; i < N; i++) if (hs[i]) CHECK(FR(hs[i]) == NULL);
}
TEST_(TN(_alloc_moveable_zero_size_is_discarded)) {
    NEED();
    HANDLE h = AL(MOVE, 0);
    CHECK(h != NULL);
    if (!h) return;
    CHECK(((ULONG_PTR)h & 8) != 0);
    UINT f = FL(h);
    CHECKF(f != 0x8000 && (f & 0x4000), "flags 0x%x", f);
    EXPECT_EQ(SZ(h), 0);
    HLG_POISON();
    void *p = LK(h);
    CHECK(p == NULL);
    EXPECT_ERR(HLG_DISCARDED);
    CHECK(FR(h) == NULL);
}
TEST_(TN(_alloc_moveable_zero_size_then_grow)) {
    NEED();
    HANDLE h = AL(MOVE, 0);
    if (!h) { CHECK(h); return; }
    HANDLE r = RE(h, 128, MOVE);
    CHECK(r == h);
    EXPECT_EQ(FL(h) & 0x4000, 0);
    EXPECT_EQ(SZ(h), 128);
    unsigned char *p = LK(h);
    CHECK(p != NULL);
    if (p) { memset(p, 0x42, 128); CHECK(hlg_all(p, 128, 0x42)); }
    UL(h);
    FR(h);
}
TEST_(TN(_alloc_fixed_zero_size)) {
    NEED();
    void *p = AL(0, 0);
    CHECK(p != NULL);
    if (!p) return;
#if K_IS_LOCAL
    EXPECT_EQ(SZ(p), 0);
#else
    EXPECT_EQ(SZ(p), 1);        /* GlobalAlloc bumps 0 to 1 */
#endif
    CHECK(FR(p) == NULL);
}
TEST_(TN(_alloc_fixed_zero_size_distinct)) {
    NEED();
    void *a = AL(0, 0), *b = AL(0, 0);
    CHECK(a && b && a != b);
    FR(a); FR(b);
}

/* ---- invalid flags ---- */
TEST_(TN(_alloc_invalid_flags)) {
    NEED();
    UINT bad[] = {
        0x1, 0x4, 0x8, 0x80, 0x3, 0x5, 0x9, 0xC, 0x8000, 0x10000, 0x100000, 0x80000000, 0xFFFFFFFF, 0xFFFF0000, 0x40000000,
#if K_IS_LOCAL
        0x1000, 0x2000, 0x4000,
#endif
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        HLG_POISON();
        void *p = AL(bad[i], 32);
        CHECKF(p == NULL, "flags 0x%x", bad[i]);
        if (p) { FR(p); continue; }
        EXPECT_ERR(HLG_INVPARAM);
    }
}
TEST_(TN(_alloc_valid_flag_combos)) {
    NEED();
    UINT ok[] = {
        0, 0x2, 0x10, 0x20, 0x40, 0x42, 0x12, 0x22, 0x32, 0x62, 0x72, 0x30, 0x50, 0x70,
        K_DISCARDABLE, K_DISCARDABLE | 2, K_DISCARDABLE | 0x40 | 2,
#if !K_IS_LOCAL
        0x1000, 0x2000, 0x2002, 0x4000, 0x4002, 0x6002, 0x1002,
#endif
    };
    for (size_t i = 0; i < sizeof ok / sizeof ok[0]; i++) {
        void *p = AL(ok[i], 48);
        CHECKF(p != NULL, "flags 0x%x", ok[i]);
        if (p) { CHECKF(FR(p) == NULL, "free flags 0x%x", ok[i]); }
    }
}
TEST_(TN(_alloc_huge_fails)) {
    NEED();
    SIZE_T huge[] = { (SIZE_T)-1, (SIZE_T)-0x100, (SIZE_T)0x8000000000000000ULL, (SIZE_T)0x7FFFFFFFFFFFFFFFULL, (SIZE_T)0x100000000000ULL };
    UINT fl[] = { 0, 0x40, 0x2, 0x42 };
    for (size_t i = 0; i < sizeof huge / sizeof huge[0]; i++) for (size_t j = 0; j < 4; j++) {
        HLG_POISON();
        void *p = AL(fl[j], huge[i]);
        CHECKF(p == NULL, "size 0x%llx flags 0x%x", (unsigned long long)huge[i], fl[j]);
        if (p) { FR(p); continue; }
        EXPECT_ERR(HLG_NOMEM);
    }
}
TEST_(TN(_alloc_huge_moveable_does_not_leak_handles)) {
    NEED();
    for (int i = 0; i < 3000; i++) { void *p = AL(MOVE, (SIZE_T)-1); if (p) { FR(p); break; } }
    HANDLE h = AL(MOVE, 10);
    CHECK(h != NULL);
    if (h) FR(h);
}

/* ---- size loops ---- */
#define SIZE_ALLOC_TEST(idx) \
TEST_(CAT(K_P, _alloc_size_##idx)) { \
    NEED(); \
    SIZE_T n = CAT(K_P, _sizes)[idx]; \
    void *f = AL(0, n); \
    CHECK(f != NULL); \
    if (f) { \
        SIZE_T exp = n; if (!K_IS_LOCAL && n == 0) exp = 1; \
        CHECK(SZ(f) == exp); hlg_fill(f, exp, 1); CHECK(hlg_verify(f, exp, 1)); CHECK(FR(f) == NULL); \
    } \
    HANDLE m = AL(MOVE | ZINIT, n); \
    CHECK(m != NULL); \
    if (m) { \
        CHECK(SZ(m) == n); \
        unsigned char *p = LK(m); \
        if (n) { CHECK(p != NULL); if (p) { CHECK(hlg_all(p, n, 0)); hlg_fill(p, n, 2); CHECK(hlg_verify(p, n, 2)); } } \
        else CHECK(p == NULL); \
        if (p) CHECK(UL(m) == FALSE); \
        CHECK(FR(m) == NULL); \
    } \
}
SIZE_ALLOC_TEST(0) SIZE_ALLOC_TEST(1) SIZE_ALLOC_TEST(2) SIZE_ALLOC_TEST(3) SIZE_ALLOC_TEST(4) SIZE_ALLOC_TEST(5)
SIZE_ALLOC_TEST(6) SIZE_ALLOC_TEST(7) SIZE_ALLOC_TEST(8) SIZE_ALLOC_TEST(9) SIZE_ALLOC_TEST(10) SIZE_ALLOC_TEST(11)
SIZE_ALLOC_TEST(12) SIZE_ALLOC_TEST(13) SIZE_ALLOC_TEST(14) SIZE_ALLOC_TEST(15) SIZE_ALLOC_TEST(16) SIZE_ALLOC_TEST(17)
SIZE_ALLOC_TEST(18) SIZE_ALLOC_TEST(19) SIZE_ALLOC_TEST(20) SIZE_ALLOC_TEST(21) SIZE_ALLOC_TEST(22) SIZE_ALLOC_TEST(23)
SIZE_ALLOC_TEST(24) SIZE_ALLOC_TEST(25) SIZE_ALLOC_TEST(26) SIZE_ALLOC_TEST(27) SIZE_ALLOC_TEST(28) SIZE_ALLOC_TEST(29)
SIZE_ALLOC_TEST(30) SIZE_ALLOC_TEST(31)

/* ---- free ---- */
TEST_(TN(_free_null)) {
    NEED();
    HLG_POISON();
    CHECK(FR(NULL) == NULL);
}
TEST_(TN(_free_fixed_returns_null)) {
    NEED();
    void *p = AL(0, 10);
    HLG_POISON();
    CHECK(FR(p) == NULL);
}
TEST_(TN(_free_moveable_returns_null)) {
    NEED();
    HANDLE h = AL(MOVE, 10);
    CHECK(FR(h) == NULL);
}
TEST_(TN(_free_moveable_twice)) {
    NEED();
    HANDLE h = AL(MOVE, 40);
    CHECK(FR(h) == NULL);
    HLG_POISON();
    HANDLE r = FR(h);
    CHECK(r == h);
    EXPECT_ERR(HLG_INVALID_HANDLE);
}
TEST_(TN(_free_moveable_while_locked)) {
    NEED();
    HANDLE h = AL(MOVE, 40);
    void *p = LK(h); LK(h);
    CHECK(p != NULL);
    CHECK(FR(h) == NULL);
}
TEST_(TN(_free_discarded_handle)) {
    NEED();
    HANDLE h = AL(MOVE, 0);
    CHECK(FR(h) == NULL);
}
TEST_(TN(_free_bogus_handle)) {
    NEED();
    HLG_POISON();
    HANDLE r = FR(HLG_BOGUS_HANDLE);
    CHECK(r == HLG_BOGUS_HANDLE);
    EXPECT_ERR(HLG_INVALID_HANDLE);
}
TEST_(TN(_free_freed_handle_all_ops_error6)) {
    NEED();
    HANDLE h = AL(MOVE, 40);
    CHECK(FR(h) == NULL);
    HLG_POISON(); CHECK(LK(h) == NULL); EXPECT_ERR(HLG_INVALID_HANDLE);
    HLG_POISON(); CHECK(FL(h) == 0x8000); EXPECT_ERR(HLG_INVALID_HANDLE);
    HLG_POISON(); CHECK(SZ(h) == 0); EXPECT_ERR(HLG_INVALID_HANDLE);
    HLG_POISON(); CHECK(RE(h, 100, MOVE) == NULL); EXPECT_ERR(HLG_INVALID_HANDLE);
}
TEST_(TN(_bogus_handle_all_ops_error6)) {
    NEED();
    HANDLE h = HLG_BOGUS_HANDLE;
    HLG_POISON(); CHECK(LK(h) == NULL); EXPECT_ERR(HLG_INVALID_HANDLE);
    HLG_POISON(); CHECK(FL(h) == 0x8000); EXPECT_ERR(HLG_INVALID_HANDLE);
    HLG_POISON(); CHECK(SZ(h) == 0); EXPECT_ERR(HLG_INVALID_HANDLE);
    HLG_POISON(); CHECK(RE(h, 100, MOVE) == NULL); EXPECT_ERR(HLG_INVALID_HANDLE);
}

/* ---- lock / unlock ---- */
TEST_(TN(_lock_unlock_sequence)) {
    NEED();
    HANDLE h = AL(MOVE, 32);
    if (!h) { CHECK(h); return; }
    CHECK(LK(h)); CHECK(LK(h)); CHECK(LK(h));
    EXPECT_EQ(FL(h) & 0xFF, 3);
    SET_ERR(HLG_ERR_SENTINEL);
    CHECK(UL(h) == TRUE);
    EXPECT_EQ(FL(h) & 0xFF, 2);
    CHECK(UL(h) == TRUE);
    CHECK(FL(h) == 1);
    SET_ERR(HLG_ERR_SENTINEL);
    CHECK(UL(h) == FALSE);          /* count reaches zero: FALSE with NO_ERROR */
    EXPECT_ERR(NO_ERROR);
    EXPECT_EQ(FL(h) & 0xFF, 0);
    SET_ERR(0);
    CHECK(UL(h) == FALSE);          /* not locked any more */
    EXPECT_ERR(HLG_NOT_LOCKED);
    EXPECT_EQ(FL(h) & 0xFF, 0);
    FR(h);
}
TEST_(TN(_unlock_unlocked_moveable_repeated)) {
    NEED();
    HANDLE h = AL(MOVE, 32);
    for (int i = 0; i < 5; i++) {
        SET_ERR(0);
        CHECK(UL(h) == FALSE);
        EXPECT_ERR(HLG_NOT_LOCKED);
    }
    EXPECT_EQ(FL(h), 0);
    FR(h);
}
TEST_(TN(_lock_count_saturates_at_255)) {
    NEED();
    HANDLE h = AL(MOVE, 32);
    if (!h) { CHECK(h); return; }
    void *first = NULL;
    for (int i = 0; i < 300; i++) { void *p = LK(h); if (i == 0) first = p; CHECK(p == first); }
    EXPECT_EQ(FL(h) & 0xFF, 255);
    int trues = 0, falses = 0;
    for (int i = 0; i < 255; i++) { SET_ERR(HLG_ERR_SENTINEL); if (UL(h)) trues++; else falses++; }
    CHECK(trues == 254);
    CHECK(falses == 1);
    EXPECT_ERR(NO_ERROR);
    EXPECT_EQ(FL(h) & 0xFF, 0);
    SET_ERR(0);
    CHECK(!UL(h));
    EXPECT_ERR(HLG_NOT_LOCKED);
    FR(h);
}
TEST_(TN(_lock_count_254_to_255)) {
    NEED();
    HANDLE h = AL(MOVE, 32);
    for (int i = 0; i < 254; i++) LK(h);
    EXPECT_EQ(FL(h) & 0xFF, 254);
    LK(h);
    EXPECT_EQ(FL(h) & 0xFF, 255);
    LK(h);
    EXPECT_EQ(FL(h) & 0xFF, 255);
    FR(h);
}
TEST_(TN(_lock_fixed_returns_pointer)) {
    NEED();
    void *p = AL(0, 64);
    void *q = LK(p);
    CHECK(q == p);
    CHECK(FL(p) == 0);
    FR(p);
}
TEST_(TN(_lock_null)) {
    NEED();
    void *r = LK(NULL);
    CHECK(r == NULL);
}
TEST_(TN(_unlock_fixed)) {
    NEED();
    void *p = AL(0, 64);
    SET_ERR(0);
    BOOL r = UL(p);
    DWORD e = GetLastError();
#if K_IS_LOCAL
    CHECK(r == FALSE);
    CHECK(e == HLG_NOT_LOCKED);
#else
    CHECK(r == TRUE);
    t_note("GlobalUnlock(fixed) err=%lu", e);
#endif
    FR(p);
}
TEST_(TN(_unlock_freed_handle)) {
    NEED();
    HANDLE h = AL(MOVE, 16);
    FR(h);
    HLG_POISON();
    BOOL r = UL(h);
    EXPECT_ERR(HLG_INVALID_HANDLE);
    t_note("unlock freed handle ret=%d", r);
}
TEST_(TN(_unlock_bogus_handle)) {
    NEED();
    HLG_POISON();
    BOOL r = UL(HLG_BOGUS_HANDLE);
    EXPECT_ERR(HLG_INVALID_HANDLE);
    t_note("unlock bogus handle ret=%d", r);
}
TEST_(TN(_lock_discarded_fails)) {
    NEED();
    HANDLE h = AL(MOVE, 64);
    CHECK(RE(h, 0, MOVE) == h);       /* discard */
    HLG_POISON();
    CHECK(LK(h) == NULL);
    EXPECT_ERR(HLG_DISCARDED);
    EXPECT_EQ(FL(h) & 0xFF, 0);
    FR(h);
}

/* ---- flags ---- */
TEST_(TN(_flags_discardable)) {
    NEED();
    HANDLE h = AL(MOVE | K_DISCARDABLE, 64);
    if (!h) { CHECK(h); return; }
    UINT f = FL(h);
    CHECKF((f & K_DISCMASK) == K_DISCMASK, "flags 0x%x", f);
    LK(h);
    EXPECT_EQ(FL(h) & 0xFF, 1);
    CHECK((FL(h) & K_DISCMASK) == K_DISCMASK);
    UL(h);
    FR(h);
}
TEST_(TN(_flags_modify_discardable)) {
    NEED();
    HANDLE h = AL(MOVE, 64);
    CHECK((FL(h) & K_DISCMASK) == 0);
    CHECK(RE(h, 0, MODIFY | K_DISCARDABLE) == h);
    CHECK((FL(h) & K_DISCMASK) == K_DISCMASK);
    CHECK(SZ(h) == 64);          /* MODIFY ignores the size */
    CHECK(RE(h, 0, MODIFY) == h);
    CHECK((FL(h) & K_DISCMASK) == 0);
    FR(h);
}
TEST_(TN(_flags_modify_ignores_size)) {
    NEED();
    HANDLE h = AL(MOVE, 64);
    CHECK(RE(h, 99999, MODIFY) == h);
    EXPECT_EQ(SZ(h), 64);
    CHECK(RE(h, 0, MODIFY | K_DISCARDABLE) == h);
    EXPECT_EQ(SZ(h), 64);
    FR(h);
}
TEST_(TN(_flags_freed_and_bogus)) {
    NEED();
    HANDLE h = AL(MOVE, 64);
    FR(h);
    HLG_POISON();
    CHECK(FL(h) == 0x8000);
    EXPECT_ERR(HLG_INVALID_HANDLE);
}
TEST_(TN(_flags_does_not_modify_lastError_on_success)) {
    NEED();
    HANDLE h = AL(MOVE, 64);
    SET_ERR(0x4321);
    FL(h);
    t_note("last error after Flags: %lu", GetLastError());
    FR(h);
}

/* ---- size ---- */
TEST_(TN(_size_after_realloc)) {
    NEED();
    HANDLE h = AL(MOVE, 50);
    EXPECT_EQ(SZ(h), 50);
    CHECK(RE(h, 5000, MOVE) == h);
    EXPECT_EQ(SZ(h), 5000);
    CHECK(RE(h, 7, MOVE) == h);
    EXPECT_EQ(SZ(h), 7);
    FR(h);
}
TEST_(TN(_size_fixed_after_realloc)) {
    NEED();
    void *p = AL(0, 500);
    void *q = RE(p, 3000, MOVE);
    CHECK(q != NULL);
    if (q) { EXPECT_EQ(SZ(q), 3000); FR(q); } else FR(p);
}

/* ---- handle ---- */
TEST_(TN(_handle_of_locked_pointer)) {
    NEED();
    HANDLE h = AL(MOVE, 64);
    void *p = LK(h);
    HLG_POISON();
    CHECK(HD(p) == h);
    UL(h);
    FR(h);
}
TEST_(TN(_handle_of_fixed_is_pointer)) {
    NEED();
    void *p = AL(0, 64);
    CHECK(HD(p) == p);
    FR(p);
}
TEST_(TN(_handle_survives_realloc_move)) {
    NEED();
    HANDLE h = AL(MOVE, 16);
    void *old = LK(h); UL(h);
    HANDLE r = RE(h, 1 << 20, MOVE);
    CHECK(r == h);
    void *now = LK(h);
    CHECK(now != NULL);
    CHECK(HD(now) == h);
    t_note("block moved: %d", now != old);
    UL(h);
    FR(h);
}

/* ---- realloc ---- */
TEST_(TN(_realloc_moveable_grow_preserves)) {
    NEED();
    SIZE_T from[] = { 1, 16, 100, 1000, 5000, 70000 }, to[] = { 2, 100, 5000, 70000, 200000, 600000 };
    for (int i = 0; i < 6; i++) {
        HANDLE h = AL(MOVE, from[i]);
        unsigned char *p = LK(h);
        hlg_fill(p, from[i], 0x31); UL(h);
        HANDLE r = RE(h, to[i], MOVE);
        CHECKF(r == h, "%zu -> %zu", from[i], to[i]);
        p = LK(h);
        CHECK(p && hlg_verify(p, from[i], 0x31));
        EXPECT_EQ(SZ(h), to[i]);
        UL(h); FR(h);
    }
}
TEST_(TN(_realloc_moveable_shrink_preserves)) {
    NEED();
    HANDLE h = AL(MOVE, 10000);
    unsigned char *p = LK(h); hlg_fill(p, 10000, 0x44); UL(h);
    CHECK(RE(h, 100, MOVE) == h);
    p = LK(h);
    CHECK(p && hlg_verify(p, 100, 0x44));
    EXPECT_EQ(SZ(h), 100);
    UL(h); FR(h);
}
TEST_(TN(_realloc_moveable_zeroinit_tail)) {
    NEED();
    HANDLE h = AL(MOVE, 100);
    unsigned char *p = LK(h); memset(p, 0xAA, 100); UL(h);
    CHECK(RE(h, 700, MOVE | ZINIT) == h);
    p = LK(h);
    CHECK(p && hlg_all(p, 100, 0xAA) && hlg_all(p + 100, 600, 0));
    UL(h); FR(h);
}
TEST_(TN(_realloc_moveable_no_move_flag_unlocked)) {
    NEED();
    /* an unlocked handle may move even without the MOVEABLE flag: the handle stays valid */
    HANDLE h = AL(MOVE, 64);
    unsigned char *p = LK(h); hlg_fill(p, 64, 6); UL(h);
    HANDLE r = RE(h, 100000, 0);
    CHECK(r == h);
    p = LK(h);
    CHECK(p && hlg_verify(p, 64, 6));
    UL(h); FR(h);
}
TEST_(TN(_realloc_locked_shrink_keeps_pointer)) {
    NEED();
    HANDLE h = AL(MOVE, 4000);
    unsigned char *p = LK(h); hlg_fill(p, 4000, 8);
    HANDLE r = RE(h, 1000, 0);
    CHECK(r == h);
    unsigned char *q = LK(h);
    CHECK(q == p);
    CHECK(q && hlg_verify(q, 1000, 8));
    EXPECT_EQ(FL(h) & 0xFF, 2);
    UL(h); UL(h); FR(h);
}
TEST_(TN(_realloc_locked_preserves_lock_count)) {
    NEED();
    HANDLE h = AL(MOVE, 64);
    LK(h); LK(h);
    HANDLE r = RE(h, 128, MOVE);
    CHECK(r == h);
    EXPECT_EQ(FL(h) & 0xFF, 2);
    UL(h); UL(h); FR(h);
}
TEST_(TN(_realloc_fixed_shrink_same_pointer)) {
    NEED();
    unsigned char *p = AL(0, 4000);
    hlg_fill(p, 4000, 0x12);
    unsigned char *q = RE(p, 1000, 0);
    /* P1: without MOVEABLE the call is in-place-only (HEAP_REALLOC_IN_PLACE_ONLY). An NT-heap block
       that lives in the low-fragmentation heap cannot be resized in place and the call returns NULL
       with the original block intact (ntdll RtlpReAllocateHeap). A non-NULL result must be p. */
    if (q) { CHECK(q == p); CHECK(hlg_verify(q, 1000, 0x12)); EXPECT_EQ(SZ(q), 1000); FR(q); }
    else { CHECK(hlg_verify(p, 4000, 0x12)); EXPECT_EQ(SZ(p), 4000); FR(p); }
}
TEST_(TN(_realloc_fixed_grow_moveable_flag)) {
    NEED();
    SIZE_T from[] = { 10, 500, 8000 }, to[] = { 800, 90000, 300000 };
    for (int i = 0; i < 3; i++) {
        unsigned char *p = AL(0, from[i]);
        hlg_fill(p, from[i], 0x21);
        unsigned char *q = RE(p, to[i], MOVE);
        CHECK(q != NULL);
        if (q) { CHECK(((ULONG_PTR)q & 8) == 0); CHECK(hlg_verify(q, from[i], 0x21)); EXPECT_EQ(SZ(q), to[i]); FR(q); } else FR(p);
    }
}
TEST_(TN(_realloc_fixed_zeroinit_tail)) {
    NEED();
    unsigned char *p = AL(0, 200);
    memset(p, 0xBB, 200);
    unsigned char *q = RE(p, 5000, MOVE | ZINIT);
    CHECK(q && hlg_all(q, 200, 0xBB) && hlg_all(q + 200, 4800, 0));
    if (q) FR(q);
}
TEST_(TN(_realloc_fixed_zeroinit_tail_inplace_only)) {
    NEED();
    unsigned char *p = AL(0, 4000);
    memset(p, 0xBB, 4000);
    unsigned char *q = RE(p, 2000, ZINIT);        /* shrink in place */
    /* P1: in-place-only; an LFH-backed block yields NULL with the original intact (see shrink_same_pointer). */
    if (q) { CHECK(q == p); CHECK(hlg_all(q, 2000, 0xBB)); }
    else CHECK(hlg_all(p, 4000, 0xBB));
    FR(q ? q : p);
}
TEST_(TN(_realloc_invalid_flags)) {
    NEED();
    HANDLE h = AL(MOVE, 32);
    UINT bad[] = { 0x1, 0x4, 0x8, 0x3, 0x100, 0xF00, 0x10000, 0x80000000, 0xFFFFFFFF
#if K_IS_LOCAL
        , 0x200, 0x1000, 0x2000, 0x4000, 0x8000
#else
        , 0x8000
#endif
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        HLG_POISON();
        HANDLE r = RE(h, 64, bad[i]);
        CHECKF(r == NULL, "flags 0x%x", bad[i]);
        if (!r) EXPECT_ERR(HLG_INVPARAM);
    }
    EXPECT_EQ(SZ(h), 32);
    FR(h);
}
TEST_(TN(_realloc_invalid_flags_fixed)) {
    NEED();
    void *p = AL(0, 32);
    UINT bad[] = { 0x1, 0x4, 0x8, 0x100, 0x10000 };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        HLG_POISON();
        void *r = RE(p, 64, bad[i]);
        CHECKF(r == NULL, "flags 0x%x", bad[i]);
        if (!r) EXPECT_ERR(HLG_INVPARAM);
    }
    EXPECT_EQ(SZ(p), 32);
    FR(p);
}
TEST_(TN(_realloc_huge_fails_keeps_original)) {
    NEED();
    HANDLE h = AL(MOVE, 64);
    unsigned char *p = LK(h); hlg_fill(p, 64, 5); UL(h);
    HLG_POISON();
    CHECK(RE(h, (SIZE_T)-1, MOVE) == NULL);
    EXPECT_ERR(HLG_NOMEM);
    EXPECT_EQ(SZ(h), 64);
    p = LK(h);
    CHECK(p && hlg_verify(p, 64, 5));
    UL(h); FR(h);
}
TEST_(TN(_realloc_fixed_huge_fails_keeps_original)) {
    NEED();
    unsigned char *p = AL(0, 64);
    hlg_fill(p, 64, 5);
    HLG_POISON();
    CHECK(RE(p, (SIZE_T)-1, MOVE) == NULL);
    EXPECT_ERR(HLG_NOMEM);
    CHECK(hlg_verify(p, 64, 5));
    EXPECT_EQ(SZ(p), 64);
    FR(p);
}
TEST_(TN(_realloc_discard_unlocked)) {
    NEED();
    HANDLE h = AL(MOVE, 64);
    CHECK(RE(h, 0, MOVE) == h);
    UINT f = FL(h);
    CHECKF(f != 0x8000 && (f & 0x4000), "flags 0x%x", f);
    EXPECT_EQ(SZ(h), 0);
    HLG_POISON(); CHECK(LK(h) == NULL); EXPECT_ERR(HLG_DISCARDED);
    /* bring it back */
    CHECK(RE(h, 32, MOVE) == h);
    EXPECT_EQ(FL(h) & 0x4000, 0);
    unsigned char *p = LK(h);
    CHECK(p != NULL);
    UL(h); FR(h);
}
TEST_(TN(_realloc_discard_locked_does_not_discard)) {
    NEED();
    HANDLE h = AL(MOVE, 64);
    LK(h);
    HANDLE r = RE(h, 0, MOVE);
    t_note("discard of locked handle returned %p (handle %p)", r, h);
    CHECK(r == NULL);
    EXPECT_EQ(SZ(h), 64);
    CHECK(FL(h) != 0x8000);
    EXPECT_EQ(FL(h) & 0x4000, 0);
    UL(h); FR(h);
}
TEST_(TN(_realloc_repeated_resize)) {
    NEED();
    HANDLE h = AL(MOVE, 1);
    SIZE_T n = 1;
    hlg_rng r; hlg_seed(&r, 99);
    for (int i = 0; i < 300; i++) {
        SIZE_T nn = 1 + hlg_next(&r) % 20000;
        unsigned char *p = LK(h);
        SIZE_T keep = nn < n ? nn : n;
        if (i == 0) { p[0] = 0x9C; }
        UL(h);
        HANDLE x = RE(h, nn, MOVE);
        CHECK(x == h);
        if (x != h) break;
        p = LK(h);
        CHECK(p && p[0] == 0x9C);
        if (!p) break;
        UL(h);
        n = nn;
    }
    EXPECT_EQ(SZ(h), n);
    FR(h);
}

/* ---- relation to the process heap ---- */
TEST_(TN(_fixed_block_is_process_heap_block)) {
    NEED();
    void *p = AL(0, 77);
    HANDLE ph = GetProcessHeap();
    CHECK(U(HeapValidate)(ph, 0, p));
    EXPECT_EQ(U(HeapSize)(ph, 0, p), 77);
    CHECK(U(HeapFree)(ph, 0, p));          /* heap free is equivalent for fixed blocks */
}
TEST_(TN(_moveable_locked_ptr_is_process_heap_block)) {
    NEED();
    HANDLE h = AL(MOVE, 123);
    void *p = LK(h);
    HANDLE ph = GetProcessHeap();
    CHECK(U(HeapValidate)(ph, 0, p));
    EXPECT_EQ(U(HeapSize)(ph, 0, p), 123);
    UL(h);
    FR(h);
}
TEST_(TN(_free_heapalloc_block)) {
    NEED();
    void *p = U(HeapAlloc)(GetProcessHeap(), 0, 88);
    CHECK(p != NULL);
    EXPECT_EQ(SZ(p), 88);
    CHECK(FR(p) == NULL);
}
TEST_(TN(_heapfree_of_moveable_pointer_is_not_how_to_free)) {
    NEED();
    /* the handle (not the pointer) owns the table slot; freeing through the handle must work after use */
    HANDLE h = AL(MOVE, 50);
    void *p = LK(h);
    CHECK(p != NULL);
    UL(h);
    CHECK(FR(h) == NULL);
}

/* ---- threads ---- */
typedef struct { HANDLE h; int ok; } CAT(K_P, _tctx);
static DWORD WINAPI CAT(K_P, _worker)(LPVOID a) {
    CAT(K_P, _tctx) *c = a;
    unsigned char *p = LK(c->h);
    c->ok = p && hlg_verify(p, 256, 0x77) && SZ(c->h) == 256;
    if (p) { memset(p, 0x11, 256); UL(c->h); }
    return 0;
}
TEST_(TN(_cross_thread_use)) {
    NEED();
    CAT(K_P, _tctx) c;
    c.h = AL(MOVE, 256); c.ok = 0;
    unsigned char *p = LK(c.h); hlg_fill(p, 256, 0x77); UL(c.h);
    HANDLE t = CreateThread(NULL, 0, CAT(K_P, _worker), &c, 0, NULL);
    if (t) { WaitForSingleObject(t, 5000); CloseHandle(t); }
    CHECK(c.ok);
    p = LK(c.h);
    CHECK(p && hlg_all(p, 256, 0x11));
    UL(c.h); FR(c.h);
}
static DWORD WINAPI CAT(K_P, _freer)(LPVOID a) {
    CAT(K_P, _tctx) *c = a;
    c->ok = (FR(c->h) == NULL);
    return 0;
}
TEST_(TN(_cross_thread_free)) {
    NEED();
    CAT(K_P, _tctx) c;
    c.h = AL(MOVE, 256); c.ok = 0;
    HANDLE t = CreateThread(NULL, 0, CAT(K_P, _freer), &c, 0, NULL);
    if (t) { WaitForSingleObject(t, 5000); CloseHandle(t); }
    CHECK(c.ok);
    HLG_POISON();
    CHECK(SZ(c.h) == 0);
    EXPECT_ERR(HLG_INVALID_HANDLE);
}
static DWORD WINAPI CAT(K_P, _hammer)(LPVOID a) {
    int bad = 0; hlg_rng r; hlg_seed(&r, (unsigned long long)(ULONG_PTR)a + 1);
    for (int i = 0; i < 1500; i++) {
        SIZE_T n = 1 + hlg_next(&r) % 3000;
        HANDLE h = AL(MOVE, n);
        if (!h) { bad++; continue; }
        unsigned char *p = LK(h);
        if (!p) { bad++; FR(h); continue; }
        hlg_fill(p, n, (unsigned char)i);
        if (!hlg_verify(p, n, (unsigned char)i)) bad++;
        UL(h);
        if (FR(h) != NULL) bad++;
    }
    return bad;
}
TEST_(TN(_concurrent_handle_table)) {
    NEED();
    HANDLE t[6]; int n = 0;
    for (int i = 0; i < 6; i++) { t[n] = CreateThread(NULL, 0, CAT(K_P, _hammer), (LPVOID)(ULONG_PTR)i, 0, NULL); if (t[n]) n++; }
    CHECK(n == 6);
    if (n) WaitForMultipleObjects(n, t, TRUE, 30000);
    for (int i = 0; i < n; i++) { DWORD ec = 99; GetExitCodeThread(t[i], &ec); CHECK(ec == 0); CloseHandle(t[i]); }
}

/* ---- stress ---- */
TEST_(TN(_stress_mixed)) {
    NEED();
    enum { SLOTS = 256 };
    static struct { HANDLE h; SIZE_T n; unsigned char seed; int moveable; } s[SLOTS];
    memset(s, 0, sizeof s);
    hlg_rng r; hlg_seed(&r, 12345);
    int bad = 0;
    for (int it = 0; it < 6000; it++) {
        int i = hlg_next(&r) % SLOTS;
        unsigned op = hlg_next(&r) % 4;
        if (!s[i].h) {
            s[i].n = 1 + hlg_next(&r) % 4096; s[i].seed = (unsigned char)it; s[i].moveable = hlg_next(&r) & 1;
            s[i].h = AL(s[i].moveable ? MOVE : 0, s[i].n);
            if (!s[i].h) { bad++; continue; }
            void *p = s[i].moveable ? LK(s[i].h) : s[i].h;
            hlg_fill(p, s[i].n, s[i].seed);
            if (s[i].moveable) UL(s[i].h);
        } else if (op == 0) {
            void *p = s[i].moveable ? LK(s[i].h) : s[i].h;
            if (!p || !hlg_verify(p, s[i].n, s[i].seed)) bad++;
            if (s[i].moveable) UL(s[i].h);
            if (FR(s[i].h) != NULL) bad++;
            s[i].h = NULL;
        } else if (op == 1) {
            SIZE_T nn = 1 + hlg_next(&r) % 8192, keep = nn < s[i].n ? nn : s[i].n;
            HANDLE x = RE(s[i].h, nn, MOVE);
            if (!x) { bad++; continue; }
            s[i].h = x;
            void *p = s[i].moveable ? LK(x) : x;
            if (!p || !hlg_verify(p, keep, s[i].seed)) bad++;
            s[i].n = nn;
            hlg_fill(p, nn, s[i].seed);
            if (s[i].moveable) UL(x);
        } else {
            if (SZ(s[i].h) != s[i].n) bad++;
        }
    }
    CHECK(bad == 0);
    for (int i = 0; i < SLOTS; i++) if (s[i].h) FR(s[i].h);
}
