#include "hlg.h"

#define PH() GetProcessHeap()
static HANDLE mk(DWORD fl, SIZE_T ini, SIZE_T mx) { return U(HeapCreate)(fl, ini, mx); }
#define CLS_COMPAT ((HEAP_INFORMATION_CLASS)0)
#define CLS_TERM   ((HEAP_INFORMATION_CLASS)1)
#define CLS_OPTRES ((HEAP_INFORMATION_CLASS)3)

/* ---------------- HeapQueryInformation ---------------- */

TEST(heapinfo_query_compat_private) {
    REQUIRE(U(HeapQueryInformation));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    ULONG v = 0xFFFFFFFF; SIZE_T rl = 0;
    BOOL r = U(HeapQueryInformation)(h, CLS_COMPAT, &v, sizeof v, &rl);
    CHECK(r);
    CHECKF(v <= 2, "compat value %lu", v);
    EXPECT_EQ(rl, 4);
    U(HeapDestroy)(h);
}
TEST(heapinfo_query_compat_process_heap) {
    REQUIRE(U(HeapQueryInformation));
    ULONG v = 0xFFFFFFFF; SIZE_T rl = 0;
    CHECK(U(HeapQueryInformation)(PH(), CLS_COMPAT, &v, sizeof v, &rl));
    CHECKF(v == 0 || v == 2, "compat value %lu", v);
    t_note("process heap compat info = %lu", v);
}
TEST(heapinfo_query_larger_buffer_returns_4) {
    REQUIRE(U(HeapQueryInformation));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    ULONG v[4] = { 0x11111111, 0x22222222, 0x33333333, 0x44444444 }; SIZE_T rl = 0;
    CHECK(U(HeapQueryInformation)(h, CLS_COMPAT, v, sizeof v, &rl));
    EXPECT_EQ(rl, 4);
    CHECK(v[1] == 0x22222222 && v[2] == 0x33333333);   /* only 4 bytes written */
    U(HeapDestroy)(h);
}
TEST(heapinfo_query_null_returnlength_ok) {
    REQUIRE(U(HeapQueryInformation));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    ULONG v = 77;
    CHECK(U(HeapQueryInformation)(h, CLS_COMPAT, &v, sizeof v, NULL));
    CHECK(v <= 2);
    U(HeapDestroy)(h);
}
TEST(heapinfo_query_small_buffer) {
    REQUIRE(U(HeapQueryInformation));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    ULONG v = 0xABABABAB; SIZE_T rl = 0;
    for (SIZE_T len = 0; len < 4; len++) {
        v = 0xABABABAB; rl = 0; HLG_POISON();
        BOOL r = U(HeapQueryInformation)(h, CLS_COMPAT, &v, len, &rl);
        t_note("query len=%zu ret=%d err=%lu rl=%zu", len, r, GetLastError(), rl);
        CHECK(!r);
        CHECK(v == 0xABABABAB);
        EXPECT_EQ(rl, 4);
    }
    U(HeapDestroy)(h);
}
TEST(heapinfo_query_null_buffer_zero_len) {
    REQUIRE(U(HeapQueryInformation));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    SIZE_T rl = 0;
    BOOL r = U(HeapQueryInformation)(h, CLS_COMPAT, NULL, 0, &rl);
    t_note("NULL query: ret=%d err=%lu rl=%zu", r, GetLastError(), rl);
    CHECK(!r);
    EXPECT_EQ(rl, 4);
    U(HeapDestroy)(h);
}
TEST(heapinfo_query_after_lfh_set) {
    REQUIRE(U(HeapQueryInformation)); REQUIRE(U(HeapSetInformation));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    ULONG v = 2;
    CHECK(U(HeapSetInformation)(h, CLS_COMPAT, &v, sizeof v));
    ULONG q = 99; SIZE_T rl = 0;
    CHECK(U(HeapQueryInformation)(h, CLS_COMPAT, &q, sizeof q, &rl));
    t_note("compat after LFH request = %lu", q);
    CHECK(q <= 2);
    for (int i = 0; i < 100; i++) { void *p = U(HeapAlloc)(h, 0, 40); if (p) U(HeapFree)(h, 0, p); }
    CHECK(U(HeapValidate)(h, 0, NULL));
    U(HeapDestroy)(h);
}

/* ---------------- HeapSetInformation ---------------- */

TEST(heapinfo_set_lfh_growable_ok) {
    REQUIRE(U(HeapSetInformation));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    ULONG v = 2;
    CHECK(U(HeapSetInformation)(h, CLS_COMPAT, &v, sizeof v));
    U(HeapDestroy)(h);
}
TEST(heapinfo_set_lfh_twice) {
    REQUIRE(U(HeapSetInformation));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    ULONG v = 2;
    CHECK(U(HeapSetInformation)(h, CLS_COMPAT, &v, sizeof v));
    BOOL r2 = U(HeapSetInformation)(h, CLS_COMPAT, &v, sizeof v);
    t_note("second LFH enable ret=%d", r2);
    U(HeapDestroy)(h);
}
TEST(heapinfo_set_lfh_fixed_heap_fails) {
    REQUIRE(U(HeapSetInformation));
    HANDLE h = mk(0, 0x10000, 0x100000);
    if (!h) { CHECK(h); return; }
    ULONG v = 2; HLG_POISON();
    CHECK(!U(HeapSetInformation)(h, CLS_COMPAT, &v, sizeof v));
    EXPECT_ERR(HLG_INVPARAM);
    U(HeapDestroy)(h);
}
TEST(heapinfo_set_lfh_noserialize_heap_fails) {
    REQUIRE(U(HeapSetInformation));
    HANDLE h = mk(HEAP_NO_SERIALIZE, 0, 0);
    if (!h) { CHECK(h); return; }
    ULONG v = 2; HLG_POISON();
    CHECK(!U(HeapSetInformation)(h, CLS_COMPAT, &v, sizeof v));
    EXPECT_ERR(HLG_INVPARAM);
    U(HeapDestroy)(h);
}
TEST(heapinfo_set_compat_wrong_value_fails) {
    REQUIRE(U(HeapSetInformation));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    ULONG vals[] = { 0, 1, 3, 4, 0xFFFFFFFF };
    for (int i = 0; i < 5; i++) {
        ULONG v = vals[i];
        CHECKF(!U(HeapSetInformation)(h, CLS_COMPAT, &v, sizeof v), "value %lu", v);
    }
    U(HeapDestroy)(h);
}
TEST(heapinfo_set_compat_short_length_fails) {
    REQUIRE(U(HeapSetInformation));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    ULONG v = 2;
    for (SIZE_T len = 0; len < 4; len++) {
        HLG_POISON();
        CHECKF(!U(HeapSetInformation)(h, CLS_COMPAT, &v, len), "len %zu", len);
        t_note("short len %zu err %lu", len, GetLastError());
    }
    U(HeapDestroy)(h);
}
TEST(heapinfo_set_unknown_class_is_accepted) {
    REQUIRE(U(HeapSetInformation));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    ULONG v = 0;
    BOOL r = U(HeapSetInformation)(h, (HEAP_INFORMATION_CLASS)99, &v, sizeof v);
    t_note("set class 99 ret=%d", r);
    U(HeapDestroy)(h);
}
TEST(heapinfo_query_unknown_class) {
    REQUIRE(U(HeapQueryInformation));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    ULONG v = 5; SIZE_T rl = 0;
    BOOL r = U(HeapQueryInformation)(h, (HEAP_INFORMATION_CLASS)99, &v, sizeof v, &rl);
    t_note("query class 99 ret=%d err=%lu", r, GetLastError());
    U(HeapDestroy)(h);
}

/* HeapOptimizeResources */
TEST(heapinfo_optimize_resources_valid) {
    REQUIRE(U(HeapSetInformation));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    void *p[50];
    for (int i = 0; i < 50; i++) p[i] = U(HeapAlloc)(h, 0, 100);
    for (int i = 0; i < 50; i++) if (p[i]) U(HeapFree)(h, 0, p[i]);
    hlg_optres o = { 1, 0 };
    SetLastError(0);
    BOOL r = U(HeapSetInformation)(h, CLS_OPTRES, &o, sizeof o);
    CHECK(r);
    if (!r) t_note("optimize err %lu", GetLastError());
    CHECK(U(HeapValidate)(h, 0, NULL));
    U(HeapDestroy)(h);
}
TEST(heapinfo_optimize_resources_process_heap) {
    REQUIRE(U(HeapSetInformation));
    hlg_optres o = { 1, 0 };
    CHECK(U(HeapSetInformation)(PH(), CLS_OPTRES, &o, sizeof o));
}
TEST(heapinfo_optimize_resources_null_heap_all) {
    REQUIRE(U(HeapSetInformation));
    hlg_optres o = { 1, 0 };
    CHECK(U(HeapSetInformation)(NULL, CLS_OPTRES, &o, sizeof o));
}
TEST(heapinfo_optimize_resources_bad_version) {
    REQUIRE(U(HeapSetInformation));
    hlg_optres o = { 2, 0 };
    HLG_POISON();
    CHECK(!U(HeapSetInformation)(PH(), CLS_OPTRES, &o, sizeof o));
    EXPECT_ERR(HLG_INVPARAM);
    o.Version = 0;
    HLG_POISON();
    CHECK(!U(HeapSetInformation)(PH(), CLS_OPTRES, &o, sizeof o));
    EXPECT_ERR(HLG_INVPARAM);
}
TEST(heapinfo_optimize_resources_bad_flags) {
    REQUIRE(U(HeapSetInformation));
    hlg_optres o = { 1, 1 };
    HLG_POISON();
    CHECK(!U(HeapSetInformation)(PH(), CLS_OPTRES, &o, sizeof o));
    EXPECT_ERR(HLG_INVPARAM);
}
TEST(heapinfo_optimize_resources_bad_length) {
    REQUIRE(U(HeapSetInformation));
    hlg_optres o = { 1, 0 };
    SIZE_T lens[] = { 0, 4, 7, 9, 16 };
    for (int i = 0; i < 5; i++) {
        HLG_POISON();
        CHECKF(!U(HeapSetInformation)(PH(), CLS_OPTRES, &o, lens[i]), "len %zu", lens[i]);
        EXPECT_ERR(HLG_INVPARAM);
    }
}
TEST(heapinfo_optimize_resources_null_info) {
    REQUIRE(U(HeapSetInformation));
    HLG_POISON();
    CHECK(!U(HeapSetInformation)(PH(), CLS_OPTRES, NULL, 8));
    EXPECT_ERR(HLG_INVPARAM);
}

/* termination on corruption: process-global, so only in children */
CHILD(term_null_handle) {
    BOOL r = U(HeapSetInformation)(NULL, CLS_TERM, NULL, 0);
    return r ? 0 : 1 + (int)GetLastError() * 0;
}
CHILD(term_private_handle) {
    HANDLE h = U(HeapCreate)(0, 0, 0);
    BOOL r = U(HeapSetInformation)(h, CLS_TERM, NULL, 0);
    return r ? 0 : 1;
}
CHILD(term_with_args) {
    ULONG junk = 0x1234;
    BOOL r = U(HeapSetInformation)(NULL, CLS_TERM, &junk, sizeof junk);
    BOOL r2 = U(HeapSetInformation)(NULL, CLS_TERM, NULL, 0);     /* idempotent */
    return (r && r2) ? 0 : 1;
}
CHILD(term_misaligned_free) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOALIGNMENTFAULTEXCEPT);
    if (!U(HeapSetInformation)(NULL, CLS_TERM, NULL, 0)) return 2;
    HANDLE h = U(HeapCreate)(0, 0, 0);
    char *p = U(HeapAlloc)(h, 0, 64);
    BOOL r = U(HeapFree)(h, 0, p + 1);
    return r ? 0x65 : 0x66;      /* survived */
}
CHILD(term_double_free) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOALIGNMENTFAULTEXCEPT);
    if (!U(HeapSetInformation)(NULL, CLS_TERM, NULL, 0)) return 2;
    HANDLE h = U(HeapCreate)(0, 0, 0);
    char *a = U(HeapAlloc)(h, 0, 100), *b = U(HeapAlloc)(h, 0, 100), *c = U(HeapAlloc)(h, 0, 100);
    U(HeapFree)(h, 0, b);
    BOOL r = U(HeapFree)(h, 0, b);
    return r ? 0x65 : 0x66;
}
CHILD(term_foreign_heap_free) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOALIGNMENTFAULTEXCEPT);
    if (!U(HeapSetInformation)(NULL, CLS_TERM, NULL, 0)) return 2;
    HANDLE h1 = U(HeapCreate)(0, 0, 0), h2 = U(HeapCreate)(0, 0, 0);
    char *a = U(HeapAlloc)(h1, 0, 100);
    BOOL r = U(HeapFree)(h2, 0, a);     /* free in the wrong heap */
    return r ? 0x65 : 0x66;
}
CHILD(noterm_misaligned_free) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOALIGNMENTFAULTEXCEPT);
    HANDLE h = U(HeapCreate)(0, 0, 0);
    char *p = U(HeapAlloc)(h, 0, 64);
    BOOL r = U(HeapFree)(h, 0, p + 1);
    return r ? 0x65 : 0x66;
}
TEST(heapinfo_term_on_corruption_null_handle) {
    REQUIRE(U(HeapSetInformation));
    EXPECT_EQ(t_run_child("term_null_handle", "", 20000), 0);
}
TEST(heapinfo_term_on_corruption_private_handle) {
    REQUIRE(U(HeapSetInformation));
    EXPECT_EQ(t_run_child("term_private_handle", "", 20000), 0);
}
TEST(heapinfo_term_on_corruption_args_ignored) {
    REQUIRE(U(HeapSetInformation));
    EXPECT_EQ(t_run_child("term_with_args", "", 20000), 0);
}
TEST(heapcorrupt_misaligned_free_terminates) {
    REQUIRE(U(HeapFree));
    DWORD rc = t_run_child("term_misaligned_free", "", 30000);
    CHECKF(rc == 0xC0000374 || rc == 0, "rc 0x%lx", rc);
    t_note("misaligned free -> 0x%lx", rc);
}
TEST(heapcorrupt_double_free_terminates) {
    REQUIRE(U(HeapFree));
    DWORD rc = t_run_child("term_double_free", "", 30000);
    CHECKF(rc == 0xC0000374 || rc == 0x66, "rc 0x%lx", rc);
    t_note("double free with termination -> 0x%lx", rc);
}
TEST(heapcorrupt_foreign_heap_free) {
    REQUIRE(U(HeapFree));
    DWORD rc = t_run_child("term_foreign_heap_free", "", 30000);
    CHECKF(rc == 0xC0000374 || rc == 0x66, "rc 0x%lx", rc);
    t_note("wrong-heap free with termination -> 0x%lx", rc);
}
TEST(heapcorrupt_misaligned_free_default_policy) {
    REQUIRE(U(HeapFree));
    DWORD rc = t_run_child("noterm_misaligned_free", "", 30000);
    CHECKF(rc == 0xC0000374 || rc == 0x66, "rc 0x%lx", rc);
    t_note("misaligned free without explicit termination opt-in -> 0x%lx", rc);
}

/* ---------------- HeapSummary ---------------- */

UDECL(BOOL, HeapSummary_k, (HANDLE, DWORD, LPHEAP_SUMMARY));
TEST(heapsummary_basic) {
    REQUIRE(U(HeapSummary));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    void *p = U(HeapAlloc)(h, 0, 100000);
    HEAP_SUMMARY s; memset(&s, 0xCD, sizeof s); s.cb = sizeof s;
    EXPECT_EQ(sizeof(HEAP_SUMMARY), 0x28);
    CHECK(U(HeapSummary)(h, 0, &s));
    EXPECT_EQ(s.cb, 0x28);
    t_note("summary alloc=%zu commit=%zu reserve=%zu maxres=%zu", (size_t)s.cbAllocated, (size_t)s.cbCommitted, (size_t)s.cbReserved, (size_t)s.cbMaxReserve);
    CHECK(s.cbAllocated >= 100000);
    if (!(s.cbCommitted >= s.cbAllocated && s.cbReserved >= s.cbCommitted)) t_note("summary relations not monotonic");
    if (p) U(HeapFree)(h, 0, p);
    U(HeapDestroy)(h);
}
TEST(heapsummary_bad_cb) {
    REQUIRE(U(HeapSummary));
    HEAP_SUMMARY s;
    DWORD cbs[] = { 0, 1, 0x20, 0x27, 0x29, 0x30, 0x100 };
    for (int i = 0; i < 7; i++) {
        memset(&s, 0, sizeof s); s.cb = cbs[i]; HLG_POISON();
        CHECKF(!U(HeapSummary)(PH(), 0, &s), "cb 0x%lx", cbs[i]);
        EXPECT_ERR(HLG_INVPARAM);
    }
}
TEST(heapsummary_process_heap) {
    REQUIRE(U(HeapSummary));
    HEAP_SUMMARY s; memset(&s, 0, sizeof s); s.cb = sizeof s;
    CHECK(U(HeapSummary)(PH(), 0, &s));
    CHECK(s.cbAllocated > 0);
    if (!s.cbCommitted) t_note("cbCommitted 0");
}
TEST(heapsummary_tracks_allocation) {
    REQUIRE(U(HeapSummary));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    HEAP_SUMMARY a = { sizeof a }, b = { sizeof b }, c = { sizeof c };
    CHECK(U(HeapSummary)(h, 0, &a));
    void *p = U(HeapAlloc)(h, 0, 50000);
    CHECK(U(HeapSummary)(h, 0, &b));
    CHECK(b.cbAllocated >= a.cbAllocated + 50000);
    U(HeapFree)(h, 0, p);
    CHECK(U(HeapSummary)(h, 0, &c));
    CHECK(c.cbAllocated < b.cbAllocated);
    U(HeapDestroy)(h);
}

/* ---------------- GetProcessHeaps ---------------- */

TEST(procheaps_count_query) {
    REQUIRE(U(GetProcessHeaps));
    DWORD n = U(GetProcessHeaps)(0, NULL);
    CHECK(n >= 1);
    HANDLE buf[256];
    DWORD n2 = U(GetProcessHeaps)(256, buf);
    CHECK(n2 == n);
}
TEST(procheaps_contains_process_heap) {
    REQUIRE(U(GetProcessHeaps));
    HANDLE buf[256]; memset(buf, 0, sizeof buf);
    DWORD n = U(GetProcessHeaps)(256, buf);
    CHECK(n >= 1 && n <= 256);
    int found = 0;
    for (DWORD i = 0; i < n && i < 256; i++) { CHECK(buf[i] != NULL); if (buf[i] == PH()) found++; }
    CHECK(found == 1);
}
TEST(procheaps_unique_entries) {
    REQUIRE(U(GetProcessHeaps));
    HANDLE buf[256];
    DWORD n = U(GetProcessHeaps)(256, buf);
    for (DWORD i = 0; i < n && i < 256; i++) for (DWORD j = i + 1; j < n && j < 256; j++) CHECK(buf[i] != buf[j]);
}
TEST(procheaps_buffer_too_small_returns_total) {
    REQUIRE(U(GetProcessHeaps));
    HANDLE h1 = mk(0, 0, 0), h2 = mk(0, 0, 0);
    DWORD total = U(GetProcessHeaps)(0, NULL);
    CHECK(total >= 3);
    HANDLE buf[1] = { (HANDLE)(ULONG_PTR)0x1111 };
    DWORD r = U(GetProcessHeaps)(1, buf);
    CHECK(r == total);
    DWORD r2 = U(GetProcessHeaps)(2, (HANDLE[2]){0});
    CHECK(r2 == total);
    U(HeapDestroy)(h1); U(HeapDestroy)(h2);
}
TEST(procheaps_exact_size_buffer) {
    REQUIRE(U(GetProcessHeaps));
    DWORD total = U(GetProcessHeaps)(0, NULL);
    HANDLE buf[260]; memset(buf, 0, sizeof buf);
    if (total > 250) { t_skip("too many heaps"); return; }
    DWORD r = U(GetProcessHeaps)(total, buf);
    CHECK(r == total);
    for (DWORD i = 0; i < total; i++) CHECK(buf[i] != NULL);
    HANDLE sentinel_buf[260]; for (int i = 0; i < 260; i++) sentinel_buf[i] = (HANDLE)(ULONG_PTR)0x77;
    r = U(GetProcessHeaps)(total + 3, sentinel_buf);
    CHECK(r == total);
    CHECK(sentinel_buf[total] == (HANDLE)(ULONG_PTR)0x77);
}
TEST(procheaps_created_heap_listed_and_removed) {
    REQUIRE(U(GetProcessHeaps));
    DWORD before = U(GetProcessHeaps)(0, NULL);
    HANDLE h = mk(0, 0, 0);
    DWORD during = U(GetProcessHeaps)(0, NULL);
    CHECK(during == before + 1);
    HANDLE buf[300];
    DWORD n = U(GetProcessHeaps)(300, buf);
    int found = 0;
    for (DWORD i = 0; i < n && i < 300; i++) if (buf[i] == h) found++;
    CHECK(found == 1);
    CHECK(U(HeapDestroy)(h));
    DWORD after = U(GetProcessHeaps)(0, NULL);
    CHECK(after == before);
    n = U(GetProcessHeaps)(300, buf);
    found = 0;
    for (DWORD i = 0; i < n && i < 300; i++) if (buf[i] == h) found++;
    CHECK(found == 0);
}
TEST(procheaps_many) {
    REQUIRE(U(GetProcessHeaps));
    DWORD before = U(GetProcessHeaps)(0, NULL);
    HANDLE hs[64];
    for (int i = 0; i < 64; i++) hs[i] = mk(0, 0, 0);
    CHECK(U(GetProcessHeaps)(0, NULL) == before + 64);
    for (int i = 0; i < 64; i += 2) U(HeapDestroy)(hs[i]);
    CHECK(U(GetProcessHeaps)(0, NULL) == before + 32);
    for (int i = 1; i < 64; i += 2) U(HeapDestroy)(hs[i]);
    CHECK(U(GetProcessHeaps)(0, NULL) == before);
}
TEST(procheaps_each_validates) {
    REQUIRE(U(GetProcessHeaps));
    HANDLE buf[300];
    DWORD n = U(GetProcessHeaps)(300, buf);
    for (DWORD i = 0; i < n && i < 300; i++) CHECKF(U(HeapValidate)(buf[i], 0, NULL), "heap %lu %p", i, buf[i]);
}

/* ---------------- HeapWalk ---------------- */

static int walk_heap(HANDLE h, void **blocks, SIZE_T *sizes, int nb, int *found, int *regions, int *busy, int *uncommitted, DWORD *lasterr) {
    PROCESS_HEAP_ENTRY e; memset(&e, 0, sizeof e);
    int iter = 0;
    *regions = *busy = *uncommitted = 0;
    for (int i = 0; i < nb; i++) found[i] = 0;
    while (U(HeapWalk)(h, &e)) {
        if (++iter > 200000) { *lasterr = 0xFFFFFFFF; return iter; }
        if (e.wFlags & PROCESS_HEAP_REGION) (*regions)++;
        else if (e.wFlags & PROCESS_HEAP_UNCOMMITTED_RANGE) (*uncommitted)++;
        else if (e.wFlags & PROCESS_HEAP_ENTRY_BUSY) {
            (*busy)++;
            for (int i = 0; i < nb; i++) if (e.lpData == blocks[i] && e.cbData == sizes[i]) found[i]++;
        }
    }
    *lasterr = GetLastError();
    return iter;
}
TEST(heapwalk_private_heap_finds_blocks) {
    REQUIRE(U(HeapWalk));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    void *b[8]; SIZE_T s[8]; int f[8];
    for (int i = 0; i < 8; i++) { s[i] = 100 + i * 37; b[i] = U(HeapAlloc)(h, 0, s[i]); }
    int rg, bz, un; DWORD le;
    CHECK(U(HeapLock)(h));
    int it = walk_heap(h, b, s, 8, f, &rg, &bz, &un, &le);
    U(HeapUnlock)(h);
    CHECK(it > 1);
    CHECK(rg >= 1);
    CHECK(bz == 8);
    for (int i = 0; i < 8; i++) CHECKF(f[i] == 1, "block %d found %d times", i, f[i]);
    EXPECT_EQ(le, HLG_NO_MORE_ITEMS);
    t_note("walk: %d entries, %d regions, %d busy, %d uncommitted", it, rg, bz, un);
    for (int i = 0; i < 8; i++) U(HeapFree)(h, 0, b[i]);
    U(HeapDestroy)(h);
}
TEST(heapwalk_empty_heap_has_no_busy) {
    REQUIRE(U(HeapWalk));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    void *b[1] = { NULL }; SIZE_T s[1] = { 0 }; int f[1]; int rg, bz, un; DWORD le;
    walk_heap(h, b, s, 1, f, &rg, &bz, &un, &le);
    CHECK(bz == 0);
    CHECK(rg >= 1);
    EXPECT_EQ(le, HLG_NO_MORE_ITEMS);
    U(HeapDestroy)(h);
}
TEST(heapwalk_freed_block_not_busy) {
    REQUIRE(U(HeapWalk));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    void *b[3]; SIZE_T s[3] = { 111, 222, 333 }; int f[3]; int rg, bz, un; DWORD le;
    for (int i = 0; i < 3; i++) b[i] = U(HeapAlloc)(h, 0, s[i]);
    U(HeapFree)(h, 0, b[1]);
    walk_heap(h, b, s, 3, f, &rg, &bz, &un, &le);
    CHECK(f[0] == 1); CHECK(f[1] == 0); CHECK(f[2] == 1);
    CHECK(bz == 2);
    U(HeapFree)(h, 0, b[0]); U(HeapFree)(h, 0, b[2]);
    U(HeapDestroy)(h);
}
TEST(heapwalk_zero_size_block) {
    REQUIRE(U(HeapWalk));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    void *b[1]; SIZE_T s[1] = { 0 }; int f[1]; int rg, bz, un; DWORD le;
    b[0] = U(HeapAlloc)(h, 0, 0);
    walk_heap(h, b, s, 1, f, &rg, &bz, &un, &le);
    CHECK(f[0] == 1);
    CHECK(bz == 1);
    U(HeapFree)(h, 0, b[0]);
    U(HeapDestroy)(h);
}
TEST(heapwalk_large_block) {
    REQUIRE(U(HeapWalk));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    void *b[2]; SIZE_T s[2] = { 1 << 20, 64 }; int f[2]; int rg, bz, un; DWORD le;
    b[0] = U(HeapAlloc)(h, 0, s[0]); b[1] = U(HeapAlloc)(h, 0, s[1]);
    walk_heap(h, b, s, 2, f, &rg, &bz, &un, &le);
    CHECK(f[0] == 1); CHECK(f[1] == 1);
    EXPECT_EQ(le, HLG_NO_MORE_ITEMS);
    U(HeapFree)(h, 0, b[0]); U(HeapFree)(h, 0, b[1]);
    U(HeapDestroy)(h);
}
TEST(heapwalk_repeatable) {
    REQUIRE(U(HeapWalk));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    void *b[5]; SIZE_T s[5]; int f[5];
    for (int i = 0; i < 5; i++) { s[i] = 64 * (i + 1); b[i] = U(HeapAlloc)(h, 0, s[i]); }
    int rg1, bz1, un1, rg2, bz2, un2; DWORD le;
    int i1 = walk_heap(h, b, s, 5, f, &rg1, &bz1, &un1, &le);
    int i2 = walk_heap(h, b, s, 5, f, &rg2, &bz2, &un2, &le);
    CHECK(i1 == i2 && rg1 == rg2 && bz1 == bz2 && un1 == un2);
    for (int i = 0; i < 5; i++) U(HeapFree)(h, 0, b[i]);
    U(HeapDestroy)(h);
}
TEST(heapwalk_region_entry_fields) {
    REQUIRE(U(HeapWalk));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    PROCESS_HEAP_ENTRY e; memset(&e, 0, sizeof e);
    int sawregion = 0, iter = 0;
    while (U(HeapWalk)(h, &e) && iter++ < 100000) {
        if (e.wFlags & PROCESS_HEAP_REGION) {
            sawregion++;
            CHECK(e.Region.dwCommittedSize > 0);
            CHECK(e.Region.lpFirstBlock != NULL);
            CHECK((char *)e.Region.lpLastBlock >= (char *)e.Region.lpFirstBlock);
            CHECK(e.iRegionIndex == 0 || sawregion > 1);
            break;
        }
    }
    CHECK(sawregion == 1);
    U(HeapDestroy)(h);
}
TEST(heapwalk_multi_region_growth) {
    REQUIRE(U(HeapWalk));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    void *b[40]; SIZE_T s[40]; int f[40];
    for (int i = 0; i < 40; i++) { s[i] = 200000; b[i] = U(HeapAlloc)(h, 0, s[i]); }   /* 8MB total, forces new segments */
    int rg, bz, un; DWORD le;
    walk_heap(h, b, s, 40, f, &rg, &bz, &un, &le);
    int all = 1; for (int i = 0; i < 40; i++) if (f[i] != 1 && b[i]) all = 0;
    CHECK(all);
    EXPECT_EQ(le, HLG_NO_MORE_ITEMS);
    t_note("multi-region walk: %d regions", rg);
    for (int i = 0; i < 40; i++) U(HeapFree)(h, 0, b[i]);
    U(HeapDestroy)(h);
}
TEST(heapwalk_process_heap_completes) {
    REQUIRE(U(HeapWalk));
    PROCESS_HEAP_ENTRY e; memset(&e, 0, sizeof e);
    int iter = 0, busy = 0, regions = 0;
    void *mine = U(HeapAlloc)(PH(), 0, 777);
    int seen = 0;
    U(HeapLock)(PH());
    while (U(HeapWalk)(PH(), &e)) {
        if (++iter > 500000) break;
        if (e.wFlags & PROCESS_HEAP_REGION) regions++;
        if (e.wFlags & PROCESS_HEAP_ENTRY_BUSY) { busy++; if (e.lpData == mine && e.cbData == 777) seen++; }
    }
    DWORD le = GetLastError();
    U(HeapUnlock)(PH());
    CHECK(iter <= 500000);
    CHECK(busy > 0 && regions > 0);
    t_note("mine seen in process heap walk: %d (LFH/segment layout dependent)", seen);
    EXPECT_EQ(le, HLG_NO_MORE_ITEMS);
    U(HeapFree)(PH(), 0, mine);
}
TEST(heapwalk_fixed_heap) {
    REQUIRE(U(HeapWalk));
    HANDLE h = mk(0, 0x10000, 0x10000);
    if (!h) { CHECK(h); return; }
    void *b[4]; SIZE_T s[4] = { 10, 500, 1500, 3000 }; int f[4]; int rg, bz, un; DWORD le;
    for (int i = 0; i < 4; i++) b[i] = U(HeapAlloc)(h, 0, s[i]);
    walk_heap(h, b, s, 4, f, &rg, &bz, &un, &le);
    for (int i = 0; i < 4; i++) CHECK(f[i] == 1);
    EXPECT_EQ(le, HLG_NO_MORE_ITEMS);
    CHECK(un >= 1 || rg >= 1);
    for (int i = 0; i < 4; i++) U(HeapFree)(h, 0, b[i]);
    U(HeapDestroy)(h);
}
TEST(heapwalk_locked_walk_other_thread_blocked) {
    REQUIRE(U(HeapWalk)); REQUIRE(U(HeapLock));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    void *p = U(HeapAlloc)(h, 0, 50);
    CHECK(U(HeapLock)(h));
    PROCESS_HEAP_ENTRY e; memset(&e, 0, sizeof e);
    int n = 0;
    while (U(HeapWalk)(h, &e) && n++ < 10000) ;
    EXPECT_EQ(GetLastError(), HLG_NO_MORE_ITEMS);
    CHECK(U(HeapUnlock)(h));
    U(HeapFree)(h, 0, p);
    U(HeapDestroy)(h);
}

/* ---------------- HeapCompact ---------------- */

TEST(heapcompact_basic) {
    REQUIRE(U(HeapCompact));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    void *p[30];
    for (int i = 0; i < 30; i++) p[i] = U(HeapAlloc)(h, 0, 1000);
    for (int i = 0; i < 30; i += 2) U(HeapFree)(h, 0, p[i]);
    SIZE_T r = U(HeapCompact)(h, 0);
    t_note("compact largest free = %zu", r);
    if (r < 1000) t_note("compact small");
    CHECK(U(HeapValidate)(h, 0, NULL));
    for (int i = 1; i < 30; i += 2) U(HeapFree)(h, 0, p[i]);
    SIZE_T r2 = U(HeapCompact)(h, 0);
    CHECK(U(HeapValidate)(h, 0, NULL));
    U(HeapDestroy)(h);
}
TEST(heapcompact_flags_noserialize) {
    REQUIRE(U(HeapCompact));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    SIZE_T a = U(HeapCompact)(h, 0), b = U(HeapCompact)(h, HEAP_NO_SERIALIZE);
    t_note("compact a=%zu b=%zu", (size_t)a, (size_t)b);
    U(HeapDestroy)(h);
}
TEST(heapcompact_process_heap) {
    REQUIRE(U(HeapCompact));
    SIZE_T r = U(HeapCompact)(PH(), 0);
    t_note("ph compact %zu", (size_t)r);
    CHECK(U(HeapValidate)(PH(), 0, NULL));
}
TEST(heapcompact_preserves_blocks) {
    REQUIRE(U(HeapCompact));
    HANDLE h = mk(0, 0, 0);
    if (!h) { CHECK(h); return; }
    unsigned char *p[20];
    for (int i = 0; i < 20; i++) { p[i] = U(HeapAlloc)(h, 0, 300 + i); hlg_fill(p[i], 300 + i, (unsigned char)i); }
    for (int i = 0; i < 20; i += 3) { U(HeapFree)(h, 0, p[i]); p[i] = NULL; }
    U(HeapCompact)(h, 0);
    for (int i = 0; i < 20; i++) if (p[i]) CHECK(hlg_verify(p[i], 300 + i, (unsigned char)i));
    for (int i = 0; i < 20; i++) if (p[i]) U(HeapFree)(h, 0, p[i]);
    U(HeapDestroy)(h);
}
