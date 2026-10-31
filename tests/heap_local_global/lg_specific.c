#include "hlg.h"

/* ---------- constants ---------- */
TEST(lgmisc_flag_constants) {
    EXPECT_EQ(LMEM_FIXED, 0); EXPECT_EQ(LMEM_MOVEABLE, 2); EXPECT_EQ(LMEM_NOCOMPACT, 0x10); EXPECT_EQ(LMEM_NODISCARD, 0x20);
    EXPECT_EQ(LMEM_ZEROINIT, 0x40); EXPECT_EQ(LMEM_MODIFY, 0x80); EXPECT_EQ(LMEM_DISCARDABLE, 0xF00);
    EXPECT_EQ(LHND, 0x42); EXPECT_EQ(LPTR, 0x40); EXPECT_EQ(LMEM_DISCARDED, 0x4000); EXPECT_EQ(LMEM_LOCKCOUNT, 0xFF);
    EXPECT_EQ(LMEM_INVALID_HANDLE, 0x8000);
    EXPECT_EQ(GMEM_FIXED, 0); EXPECT_EQ(GMEM_MOVEABLE, 2); EXPECT_EQ(GMEM_DISCARDABLE, 0x100); EXPECT_EQ(GMEM_DDESHARE, 0x2000);
    EXPECT_EQ(GMEM_SHARE, 0x2000); EXPECT_EQ(GMEM_NOTIFY, 0x4000); EXPECT_EQ(GHND, 0x42); EXPECT_EQ(GPTR, 0x40);
    EXPECT_EQ(GMEM_DISCARDED, 0x4000); EXPECT_EQ(GMEM_LOCKCOUNT, 0xFF); EXPECT_EQ(GMEM_INVALID_HANDLE, 0x8000);
}

/* ---------- GlobalLock special pointers ---------- */
TEST(global_lock_null_pointer) {
    REQUIRE(U(GlobalLock));
    HLG_POISON();
    CHECK(U(GlobalLock)(NULL) == NULL);
    EXPECT_ERR(HLG_INVALID_HANDLE);
}
TEST(global_lock_unreadable_pointer) {
    REQUIRE(U(GlobalLock));
    if (t_is_wine()) { t_skip("emulator dereferences the pointer"); return; }
    void *bad[] = { (void *)0x10, (void *)0x1000, (void *)0x20000 };
    for (int i = 0; i < 3; i++) {
        HLG_POISON();
        void *r = U(GlobalLock)(bad[i]);
        t_note("GlobalLock(%p) -> %p err %lu", bad[i], r, GetLastError());
        if (!IsBadReadPtr(bad[i], 1)) continue;
        CHECK(r == NULL);
        EXPECT_ERR(HLG_INVALID_HANDLE);
    }
}
TEST(global_lock_kernel_address) {
    REQUIRE(U(GlobalLock));
    if (t_is_wine()) { t_skip("emulator dereferences the pointer"); return; }
    HLG_POISON();
    CHECK(U(GlobalLock)((void *)0xFFFF800000000010ULL) == NULL);
    EXPECT_ERR(HLG_INVALID_HANDLE);
}
TEST(global_lock_stack_pointer_returned_as_is) {
    REQUIRE(U(GlobalLock));
    char buf[64] __attribute__((aligned(16))); memset(buf, 1, sizeof buf);
    CHECK(U(GlobalLock)(buf) == buf);
    CHECK(U(GlobalLock)(buf + 16) == buf + 16);
}
TEST(global_lock_static_pointer_returned_as_is) {
    REQUIRE(U(GlobalLock));
    static char g[64] __attribute__((aligned(16)));
    CHECK(U(GlobalLock)(g) == g);
}
TEST(local_lock_null_pointer) {
    REQUIRE(U(LocalLock));
    CHECK(U(LocalLock)(NULL) == NULL);
}
TEST(local_lock_user_pointer_returned_as_is) {
    REQUIRE(U(LocalLock));
    if (t_is_wine()) { t_skip("emulator dereferences the pointer"); return; }
    char buf[32] __attribute__((aligned(16)));
    CHECK(U(LocalLock)(buf) == buf);
    CHECK(U(LocalLock)((void *)0x10) == (void *)0x10);
}
TEST(local_lock_kernel_address) {
    REQUIRE(U(LocalLock));
    if (t_is_wine()) { t_skip("emulator dereferences the pointer"); return; }
    HLG_POISON();
    void *r = U(LocalLock)((void *)0xFFFF800000000010ULL);
    CHECK(r == NULL);
}
TEST(global_unlock_nonhandle_pointer_is_true) {
    REQUIRE(U(GlobalUnlock));
    char buf[16] __attribute__((aligned(16)));
    CHECK(U(GlobalUnlock)(buf));
    CHECK(U(GlobalUnlock)(NULL));
}
TEST(local_unlock_nonhandle_pointer_not_locked) {
    REQUIRE(U(LocalUnlock));
    char buf[16] __attribute__((aligned(16)));
    SET_ERR(0);
    CHECK(!U(LocalUnlock)(buf));
    EXPECT_ERR(HLG_NOT_LOCKED);
    SET_ERR(0);
    CHECK(!U(LocalUnlock)(NULL));
    EXPECT_ERR(HLG_NOT_LOCKED);
}

/* ---------- GMEM_DDESHARE / clipboard-style ---------- */
TEST(global_ddeshare_moveable_flags) {
    REQUIRE(U(GlobalAlloc)); REQUIRE(U(GlobalFlags));
    HANDLE h = U(GlobalAlloc)(GMEM_MOVEABLE | GMEM_DDESHARE, 64);
    CHECK(h != NULL);
    if (!h) return;
    UINT f = U(GlobalFlags)(h);
    CHECKF((f & GMEM_DDESHARE) == GMEM_DDESHARE, "flags 0x%x", f);
    EXPECT_EQ(f & GMEM_LOCKCOUNT, 0);
    U(GlobalFree)(h);
}
TEST(global_share_equals_ddeshare) {
    REQUIRE(U(GlobalAlloc));
    HANDLE h = U(GlobalAlloc)(GMEM_MOVEABLE | GMEM_SHARE, 64);
    CHECK(h != NULL);
    if (h) { CHECK((U(GlobalFlags)(h) & GMEM_DDESHARE) != 0); U(GlobalFree)(h); }
}
TEST(global_ddeshare_fixed) {
    REQUIRE(U(GlobalAlloc));
    void *p = U(GlobalAlloc)(GMEM_FIXED | GMEM_DDESHARE, 64);
    CHECK(p != NULL);
    if (p) { CHECK(((ULONG_PTR)p & 8) == 0); CHECK(U(GlobalSize)(p) == 64); CHECK(U(GlobalFree)(p) == NULL); }
}
TEST(global_clipboard_style_string) {
    REQUIRE(U(GlobalAlloc)); REQUIRE(U(GlobalLock));
    const char *s = "clipboard payload";
    SIZE_T n = strlen(s) + 1;
    HANDLE h = U(GlobalAlloc)(GMEM_MOVEABLE | GMEM_DDESHARE, n);
    CHECK(h != NULL);
    if (!h) return;
    char *p = U(GlobalLock)(h);
    CHECK(p != NULL);
    if (p) memcpy(p, s, n);
    CHECK(!U(GlobalUnlock)(h));     /* last unlock returns FALSE */
    EXPECT_EQ(U(GlobalSize)(h), n);
    p = U(GlobalLock)(h);
    CHECK(p && !strcmp(p, s));
    U(GlobalUnlock)(h);
    HANDLE h2 = U(GlobalHandle)(p);
    CHECK(h2 == h);
    CHECK(U(GlobalFree)(h) == NULL);
}
TEST(global_ghnd_gptr) {
    REQUIRE(U(GlobalAlloc));
    HANDLE h = U(GlobalAlloc)(GHND, 300);
    unsigned char *p = h ? U(GlobalLock)(h) : NULL;
    CHECK(p && hlg_all(p, 300, 0));
    if (h) { U(GlobalUnlock)(h); U(GlobalFree)(h); }
    unsigned char *q = U(GlobalAlloc)(GPTR, 300);
    CHECK(q && hlg_all(q, 300, 0));
    CHECK(((ULONG_PTR)q & 8) == 0);
    if (q) U(GlobalFree)(q);
}
TEST(global_realloc_moveable_keeps_ddeshare) {
    REQUIRE(U(GlobalAlloc)); REQUIRE(U(GlobalReAlloc));
    HANDLE h = U(GlobalAlloc)(GMEM_MOVEABLE | GMEM_DDESHARE, 64);
    HANDLE r = U(GlobalReAlloc)(h, 4000, GMEM_MOVEABLE);
    CHECK(r == h);
    UINT f = U(GlobalFlags)(h);
    t_note("flags after realloc 0x%x", f);
    CHECK((f & GMEM_DDESHARE) != 0);
    U(GlobalFree)(h);
}

/* ---------- Fix / Wire ---------- */
TEST(global_fix_unfix) {
    REQUIRE(U(GlobalFix)); REQUIRE(U(GlobalUnfix));
    HANDLE h = U(GlobalAlloc)(GMEM_MOVEABLE, 64);
    U(GlobalFix)(h);
    EXPECT_EQ(U(GlobalFlags)(h) & GMEM_LOCKCOUNT, 1);
    U(GlobalFix)(h);
    EXPECT_EQ(U(GlobalFlags)(h) & GMEM_LOCKCOUNT, 2);
    U(GlobalUnfix)(h);
    EXPECT_EQ(U(GlobalFlags)(h) & GMEM_LOCKCOUNT, 1);
    U(GlobalUnfix)(h);
    EXPECT_EQ(U(GlobalFlags)(h) & GMEM_LOCKCOUNT, 0);
    U(GlobalFree)(h);
}
TEST(global_fix_invalid_handle_value_noop) {
    REQUIRE(U(GlobalFix)); REQUIRE(U(GlobalUnfix));
    U(GlobalFix)((HGLOBAL)INVALID_HANDLE_VALUE);
    U(GlobalUnfix)((HGLOBAL)INVALID_HANDLE_VALUE);
    CHECK(1);
}
TEST(global_fix_fixed_block) {
    REQUIRE(U(GlobalFix));
    void *p = U(GlobalAlloc)(GMEM_FIXED, 32);
    U(GlobalFix)(p);
    U(GlobalUnfix)(p);
    CHECK(U(GlobalSize)(p) == 32);
    U(GlobalFree)(p);
}
TEST(global_wire_unwire) {
    REQUIRE(U(GlobalWire)); REQUIRE(U(GlobalUnWire));
    HANDLE h = U(GlobalAlloc)(GMEM_MOVEABLE, 64);
    void *p = U(GlobalWire)(h);
    CHECK(p != NULL);
    void *q = U(GlobalLock)(h);
    CHECK(p == q);
    EXPECT_EQ(U(GlobalFlags)(h) & GMEM_LOCKCOUNT, 2);
    SET_ERR(HLG_ERR_SENTINEL);
    CHECK(U(GlobalUnWire)(h) == TRUE);
    SET_ERR(HLG_ERR_SENTINEL);
    CHECK(U(GlobalUnWire)(h) == FALSE);
    EXPECT_ERR(NO_ERROR);
    SET_ERR(0);
    CHECK(U(GlobalUnWire)(h) == FALSE);
    EXPECT_ERR(HLG_NOT_LOCKED);
    U(GlobalFree)(h);
}
TEST(global_wire_fixed) {
    REQUIRE(U(GlobalWire)); REQUIRE(U(GlobalUnWire));
    void *p = U(GlobalAlloc)(GMEM_FIXED, 32);
    CHECK(U(GlobalWire)(p) == p);
    CHECK(U(GlobalUnWire)(p));
    U(GlobalFree)(p);
}
TEST(global_wire_null) {
    REQUIRE(U(GlobalWire));
    HLG_POISON();
    CHECK(U(GlobalWire)(NULL) == NULL);
    EXPECT_ERR(HLG_INVALID_HANDLE);
}
TEST(global_wire_freed_handle) {
    REQUIRE(U(GlobalWire));
    HANDLE h = U(GlobalAlloc)(GMEM_MOVEABLE, 16);
    U(GlobalFree)(h);
    HLG_POISON();
    CHECK(U(GlobalWire)(h) == NULL);
    EXPECT_ERR(HLG_INVALID_HANDLE);
}

/* ---------- GlobalReAlloc specifics ---------- */
TEST(global_realloc_fixed_to_moveable_modify) {
    REQUIRE(U(GlobalReAlloc));
    unsigned char *p = U(GlobalAlloc)(GMEM_FIXED, 200);
    hlg_fill(p, 200, 0x3C);
    HANDLE r = U(GlobalReAlloc)(p, 0, GMEM_MODIFY | GMEM_MOVEABLE);
    CHECK(r != NULL);
    if (!r) { U(GlobalFree)(p); return; }
    t_note("fixed->moveable conversion yielded %s (%p)", ((ULONG_PTR)r & 8) ? "a handle" : "a pointer", r);
    CHECK(U(GlobalSize)(r) == 200);
    unsigned char *q = U(GlobalLock)(r);
    CHECK(q && hlg_verify(q, 200, 0x3C));
    U(GlobalUnlock)(r);
    U(GlobalFree)(r);
}
TEST(local_realloc_fixed_modify_returns_same) {
    REQUIRE(U(LocalReAlloc));
    void *p = U(LocalAlloc)(LMEM_FIXED, 100);
    HLG_POISON();
    void *r = U(LocalReAlloc)(p, 0, LMEM_MODIFY);
    CHECK(r == p);
    r = U(LocalReAlloc)(p, 0, LMEM_MODIFY | LMEM_DISCARDABLE);
    CHECK(r == p);
    EXPECT_EQ(U(LocalSize)(p), 100);
    U(LocalFree)(p);
}
TEST(global_realloc_fixed_modify_only_noop) {
    REQUIRE(U(GlobalReAlloc));
    void *p = U(GlobalAlloc)(GMEM_FIXED, 100);
    void *r = U(GlobalReAlloc)(p, 0, GMEM_MODIFY);
    CHECK(r == p);
    EXPECT_EQ(U(GlobalSize)(p), 100);
    U(GlobalFree)(p);
}
TEST(global_discard_macro) {
    REQUIRE(U(GlobalReAlloc));
    HANDLE h = U(GlobalAlloc)(GMEM_MOVEABLE | GMEM_DISCARDABLE, 100);
    HANDLE r = U(GlobalReAlloc)(h, 0, GMEM_MOVEABLE);     /* GlobalDiscard(h) */
    CHECK(r == h);
    CHECK((U(GlobalFlags)(h) & GMEM_DISCARDED) != 0);
    EXPECT_EQ(U(GlobalSize)(h), 0);
    U(GlobalFree)(h);
}
TEST(local_discard_macro) {
    REQUIRE(U(LocalReAlloc));
    HANDLE h = U(LocalAlloc)(LMEM_MOVEABLE | LMEM_DISCARDABLE, 100);
    HANDLE r = U(LocalReAlloc)(h, 0, LMEM_MOVEABLE);      /* LocalDiscard(h) */
    CHECK(r == h);
    CHECK((U(LocalFlags)(h) & LMEM_DISCARDED) != 0);
    EXPECT_EQ(U(LocalSize)(h), 0);
    U(LocalFree)(h);
}

/* ---------- compact / shrink ---------- */
TEST(global_compact_returns_without_crash) {
    REQUIRE(U(GlobalCompact));
    SIZE_T a = U(GlobalCompact)(0), b = U(GlobalCompact)(0x10000), c = U(GlobalCompact)(0xFFFFFFFF);
    t_note("GlobalCompact -> %zu %zu %zu", a, b, c);
    CHECK(a != (SIZE_T)-1);
    CHECK(U(HeapValidate)(GetProcessHeap(), 0, NULL));
}
TEST(local_compact_returns_without_crash) {
    REQUIRE(U(LocalCompact));
    SIZE_T a = U(LocalCompact)(0), b = U(LocalCompact)(0x10000);
    t_note("LocalCompact -> %zu %zu", a, b);
    CHECK(a != (SIZE_T)-1);
    CHECK(U(HeapValidate)(GetProcessHeap(), 0, NULL));
}
TEST(local_shrink_returns_without_crash) {
    REQUIRE(U(LocalShrink));
    HANDLE h = U(LocalAlloc)(LMEM_MOVEABLE, 1000);
    SIZE_T a = U(LocalShrink)(h, 0);
    t_note("LocalShrink -> %zu", a);
    CHECK(a != (SIZE_T)-1);
    EXPECT_EQ(U(LocalSize)(h), 1000);
    U(LocalFree)(h);
}
TEST(compact_family_shares_implementation) {
    HMODULE k = t_kernel32();
    FARPROC g = GetProcAddress(k, "GlobalCompact"), l = GetProcAddress(k, "LocalCompact"), s = GetProcAddress(k, "LocalShrink");
    if (!g || !l || !s) { t_skip("missing export"); return; }
    t_note("GlobalCompact=%p LocalCompact=%p LocalShrink=%p", g, l, s);
    if (g != l || l != s) t_note("compact family does not share one address");
}
TEST(compact_after_free_does_not_invalidate) {
    REQUIRE(U(GlobalCompact));
    unsigned char *p[40];
    for (int i = 0; i < 40; i++) { p[i] = U(GlobalAlloc)(GMEM_FIXED, 5000); hlg_fill(p[i], 5000, (unsigned char)i); }
    for (int i = 0; i < 40; i += 2) U(GlobalFree)(p[i]);
    U(GlobalCompact)(0);
    for (int i = 1; i < 40; i += 2) CHECK(hlg_verify(p[i], 5000, (unsigned char)i));
    for (int i = 1; i < 40; i += 2) U(GlobalFree)(p[i]);
}

/* ---------- Local/Global interplay ---------- */
TEST(interplay_local_handle_global_functions) {
    REQUIRE(U(LocalAlloc)); REQUIRE(U(GlobalLock));
    HANDLE h = U(LocalAlloc)(LMEM_MOVEABLE, 50);
    unsigned char *p = U(GlobalLock)(h);
    CHECK(p != NULL);
    if (p) hlg_fill(p, 50, 7);
    EXPECT_EQ(U(LocalFlags)(h) & 0xFF, 1);
    EXPECT_EQ(U(GlobalSize)(h), 50);
    CHECK(U(GlobalHandle)(p) == h);
    CHECK(U(LocalHandle)(p) == h);
    CHECK(U(GlobalUnlock)(h) == FALSE);
    p = U(LocalLock)(h);
    CHECK(p && hlg_verify(p, 50, 7));
    U(LocalUnlock)(h);
    CHECK(U(GlobalFree)(h) == NULL);
}
TEST(interplay_global_handle_local_functions) {
    REQUIRE(U(GlobalAlloc)); REQUIRE(U(LocalLock));
    HANDLE h = U(GlobalAlloc)(GMEM_MOVEABLE | GMEM_ZEROINIT, 70);
    unsigned char *p = U(LocalLock)(h);
    CHECK(p && hlg_all(p, 70, 0));
    EXPECT_EQ(U(GlobalFlags)(h) & 0xFF, 1);
    EXPECT_EQ(U(LocalSize)(h), 70);
    CHECK(U(LocalUnlock)(h) == FALSE);
    CHECK(U(LocalFree)(h) == NULL);
}
TEST(interplay_fixed_cross_free) {
    REQUIRE(U(LocalAlloc)); REQUIRE(U(GlobalAlloc));
    void *a = U(LocalAlloc)(LMEM_FIXED, 40), *b = U(GlobalAlloc)(GMEM_FIXED, 40);
    CHECK(U(GlobalSize)(a) == 40);
    CHECK(U(LocalSize)(b) == 40);
    CHECK(U(GlobalFree)(a) == NULL);
    CHECK(U(LocalFree)(b) == NULL);
}
TEST(interplay_realloc_cross) {
    REQUIRE(U(LocalAlloc)); REQUIRE(U(GlobalReAlloc));
    HANDLE h = U(LocalAlloc)(LMEM_MOVEABLE, 20);
    HANDLE r = U(GlobalReAlloc)(h, 4000, GMEM_MOVEABLE);
    CHECK(r == h);
    EXPECT_EQ(U(LocalSize)(h), 4000);
    r = U(LocalReAlloc)(h, 10, LMEM_MOVEABLE);
    CHECK(r == h);
    EXPECT_EQ(U(GlobalSize)(h), 10);
    U(LocalFree)(h);
}
TEST(interplay_heapalloc_block_as_fixed) {
    REQUIRE(U(HeapAlloc));
    void *p = U(HeapAlloc)(GetProcessHeap(), HEAP_ZERO_MEMORY, 128);
    CHECK(U(LocalSize)(p) == 128);
    CHECK(U(GlobalSize)(p) == 128);
    CHECK(U(LocalHandle)(p) == p);
    CHECK(U(GlobalHandle)(p) == p);
    CHECK(U(LocalFlags)(p) == 0);
    CHECK(U(GlobalFlags)(p) == 0);
    void *q = U(LocalReAlloc)(p, 300, LMEM_MOVEABLE);
    CHECK(q != NULL);
    CHECK(U(HeapSize)(GetProcessHeap(), 0, q ? q : p) == 300);
    U(HeapFree)(GetProcessHeap(), 0, q ? q : p);
}
TEST(interplay_handle_and_fixed_coexist) {
    REQUIRE(U(LocalAlloc));
    HANDLE hs[100]; void *fs[100];
    for (int i = 0; i < 100; i++) { hs[i] = U(LocalAlloc)(LMEM_MOVEABLE, 33 + i); fs[i] = U(GlobalAlloc)(GMEM_FIXED, 33 + i); }
    for (int i = 0; i < 100; i++) { CHECK(U(GlobalSize)(hs[i]) == (SIZE_T)(33 + i)); CHECK(U(LocalSize)(fs[i]) == (SIZE_T)(33 + i)); }
    for (int i = 0; i < 100; i++) { U(GlobalFree)(hs[i]); U(LocalFree)(fs[i]); }
}

/* ---------- big allocations ---------- */
TEST(lgmisc_big_fixed_zeroinit) {
    REQUIRE(U(GlobalAlloc));
    SIZE_T n = 48u << 20;
    unsigned char *p = U(GlobalAlloc)(GPTR, n);
    CHECK(p != NULL);
    if (!p) return;
    CHECK(U(GlobalSize)(p) == n);
    CHECK(hlg_all(p, n, 0));
    p[0] = 1; p[n - 1] = 2;
    U(GlobalFree)(p);
}
TEST(lgmisc_big_moveable_realloc_chain) {
    REQUIRE(U(LocalAlloc));
    HANDLE h = U(LocalAlloc)(LHND, 1 << 20);
    for (SIZE_T n = 2 << 20; n <= (24u << 20); n *= 2) {
        HANDLE r = U(LocalReAlloc)(h, n, LMEM_MOVEABLE | LMEM_ZEROINIT);
        CHECK(r == h);
        EXPECT_EQ(U(LocalSize)(h), n);
        unsigned char *p = U(LocalLock)(h);
        CHECK(p && p[n - 1] == 0 && p[(n >> 1) + 5] == 0);
        if (p) p[n - 1] = 9;
        U(LocalUnlock)(h);
    }
    U(LocalFree)(h);
}
TEST(lgmisc_alloc_free_cycles_reuse) {
    REQUIRE(U(LocalAlloc));
    for (int i = 0; i < 20000; i++) {
        HANDLE h = U(LocalAlloc)(LMEM_MOVEABLE, 10 + (i & 63));
        if (!h) { CHECK(h); return; }
        if (U(LocalFree)(h) != NULL) { CHECK(0); return; }
    }
    CHECK(1);
}

/* ---------- GlobalMemoryStatus ---------- */
TEST(memstatus_legacy_basic) {
    REQUIRE(U(GlobalMemoryStatus));
    MEMORYSTATUS m; memset(&m, 0xCD, sizeof m); m.dwLength = 0;
    U(GlobalMemoryStatus)(&m);
    EXPECT_EQ(m.dwLength, sizeof(MEMORYSTATUS));
    EXPECT_EQ(sizeof(MEMORYSTATUS), 56);
    CHECK(m.dwMemoryLoad <= 100);
    CHECK(m.dwTotalPhys > 0);
    CHECK(m.dwAvailPhys <= m.dwTotalPhys);
    CHECK(m.dwTotalPageFile > 0);
    CHECK(m.dwAvailPageFile <= m.dwTotalPageFile);
    CHECK(m.dwTotalVirtual > (SIZE_T)1 << 30);
    CHECK(m.dwAvailVirtual <= m.dwTotalVirtual);
    CHECK(m.dwAvailVirtual > 0);
}
TEST(memstatus_legacy_matches_ex) {
    REQUIRE(U(GlobalMemoryStatus)); REQUIRE(U(GlobalMemoryStatusEx));
    MEMORYSTATUS m; MEMORYSTATUSEX x; memset(&x, 0, sizeof x); x.dwLength = sizeof x;
    CHECK(U(GlobalMemoryStatusEx)(&x));
    U(GlobalMemoryStatus)(&m);
    CHECK(m.dwTotalPhys == x.ullTotalPhys);
    CHECK(m.dwTotalVirtual == x.ullTotalVirtual);
    CHECK(m.dwMemoryLoad + 3 >= x.dwMemoryLoad && m.dwMemoryLoad <= x.dwMemoryLoad + 3);
}
TEST(memstatus_ignores_input_length) {
    REQUIRE(U(GlobalMemoryStatus));
    MEMORYSTATUS m; memset(&m, 0, sizeof m); m.dwLength = 0x12345;
    U(GlobalMemoryStatus)(&m);
    EXPECT_EQ(m.dwLength, 56);
    CHECK(m.dwTotalPhys > 0);
}
TEST(memstatus_ex_basic) {
    REQUIRE(U(GlobalMemoryStatusEx));
    MEMORYSTATUSEX x; memset(&x, 0xCD, sizeof x); x.dwLength = sizeof x;
    EXPECT_EQ(sizeof(MEMORYSTATUSEX), 64);
    CHECK(U(GlobalMemoryStatusEx)(&x));
    CHECK(x.dwLength == 64);
    CHECK(x.dwMemoryLoad <= 100);
    CHECK(x.ullTotalPhys > (1ULL << 26));
    CHECK(x.ullAvailPhys <= x.ullTotalPhys);
    CHECK(x.ullTotalPageFile >= 0 && x.ullAvailPageFile <= x.ullTotalPageFile);
    CHECK(x.ullTotalVirtual > (1ULL << 40));
    CHECK(x.ullTotalVirtual <= (1ULL << 47));
    CHECK(x.ullAvailVirtual <= x.ullTotalVirtual);
    EXPECT_EQ(x.ullAvailExtendedVirtual, 0);
    t_note("virt total 0x%llx avail 0x%llx", x.ullTotalVirtual, x.ullAvailVirtual);
}
TEST(memstatus_ex_bad_length) {
    REQUIRE(U(GlobalMemoryStatusEx));
    DWORD lens[] = { 0, 1, 56, 63, 65, 128, 0xFFFFFFFF };
    for (int i = 0; i < 7; i++) {
        MEMORYSTATUSEX x; memset(&x, 0, sizeof x); x.dwLength = lens[i];
        HLG_POISON();
        BOOL r = U(GlobalMemoryStatusEx)(&x);
        CHECKF(!r, "length %lu", lens[i]);
        if (!r) EXPECT_ERR(HLG_INVPARAM);
    }
}
TEST(memstatus_ex_null) {
    REQUIRE(U(GlobalMemoryStatusEx));
    /* NULL is dereferenced for dwLength on this OS; only documented for a valid pointer */
    CHECK(1);
}
TEST(memstatus_ex_consistent_across_calls) {
    REQUIRE(U(GlobalMemoryStatusEx));
    MEMORYSTATUSEX a, b; memset(&a, 0, sizeof a); a.dwLength = sizeof a; b = a;
    CHECK(U(GlobalMemoryStatusEx)(&a)); CHECK(U(GlobalMemoryStatusEx)(&b));
    CHECK(a.ullTotalPhys == b.ullTotalPhys);
    CHECK(a.ullTotalVirtual == b.ullTotalVirtual);
}
TEST(memstatus_ex_virtual_tracks_reserve) {
    REQUIRE(U(GlobalMemoryStatusEx));
    MEMORYSTATUSEX a, b; memset(&a, 0, sizeof a); a.dwLength = sizeof a; b = a;
    U(GlobalMemoryStatusEx)(&a);
    void *p = VirtualAlloc(NULL, 256u << 20, MEM_RESERVE, PAGE_NOACCESS);
    U(GlobalMemoryStatusEx)(&b);
    if (p) { CHECK(b.ullAvailVirtual < a.ullAvailVirtual); VirtualFree(p, 0, MEM_RELEASE); }
}
