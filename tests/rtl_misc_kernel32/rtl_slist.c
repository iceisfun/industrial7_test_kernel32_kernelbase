/* Interlocked singly linked lists: kernel32 exports InitializeSListHead, InterlockedPush/Pop/Flush, QueryDepthSList,
 * InterlockedPushListSList(Ex); ntdll Rtl* counterparts. */
#include "t.h"

typedef struct { SLIST_ENTRY e; ULONG id; ULONG pad; } __attribute__((aligned(16))) ITEM;

UDECL(void, InitializeSListHead, (PSLIST_HEADER));
UDECL(PSLIST_ENTRY, InterlockedPushEntrySList, (PSLIST_HEADER, PSLIST_ENTRY));
UDECL(PSLIST_ENTRY, InterlockedPopEntrySList, (PSLIST_HEADER));
UDECL(PSLIST_ENTRY, InterlockedFlushSList, (PSLIST_HEADER));
UDECL(USHORT, QueryDepthSList, (PSLIST_HEADER));
UDECL(PSLIST_ENTRY, InterlockedPushListSList, (PSLIST_HEADER, PSLIST_ENTRY, PSLIST_ENTRY, ULONG));
UDECL(PSLIST_ENTRY, InterlockedPushListSListEx, (PSLIST_HEADER, PSLIST_ENTRY, PSLIST_ENTRY, ULONG));
UDECL(PSLIST_ENTRY, RtlFirstEntrySList, (const SLIST_HEADER *));

#define INIT UN(InitializeSListHead)
#define PUSH UN(InterlockedPushEntrySList)
#define POP  UN(InterlockedPopEntrySList)
#define FLUSH UN(InterlockedFlushSList)
#define DEPTH UN(QueryDepthSList)

static const char *const sl_names[] = { "InitializeSListHead", "InterlockedPushEntrySList", "InterlockedPopEntrySList",
    "InterlockedFlushSList", "QueryDepthSList", "InterlockedPushListSList", "InterlockedPushListSListEx", NULL };

TEST(slist_exports_in_kernel32_and_kernelbase) {
    for (int i = 0; sl_names[i]; i++) {
        CHECKF(t_proc_k32(sl_names[i]) != NULL, "kernel32 missing %s", sl_names[i]);
        CHECKF(t_proc_kb(sl_names[i]) != NULL, "kernelbase missing %s", sl_names[i]);
    }
}

TEST(slist_ntdll_counterparts_exist) {
    static const char *const n[] = { "RtlInitializeSListHead", "RtlInterlockedPushEntrySList", "RtlInterlockedPopEntrySList",
        "RtlInterlockedFlushSList", "RtlQueryDepthSList", "RtlInterlockedPushListSListEx", "RtlFirstEntrySList", NULL };
    for (int i = 0; n[i]; i++) CHECKF(t_proc_nt(n[i]) != NULL, "ntdll missing %s", n[i]);
}

TEST(slist_init_empty_list) {
    REQUIRE(INIT); REQUIRE(POP); REQUIRE(DEPTH); REQUIRE(FLUSH);
    SLIST_HEADER h; memset(&h, 0xFF, sizeof h);
    INIT(&h);
    EXPECT_EQ(DEPTH(&h), 0);
    CHECK(POP(&h) == NULL);
    CHECK(FLUSH(&h) == NULL);
}

TEST(slist_init_clears_both_qwords) {
    REQUIRE(INIT);
    SLIST_HEADER h; memset(&h, 0xFF, sizeof h);
    INIT(&h);
    const unsigned long long *q = (const unsigned long long *)&h;
    EXPECT_EQ(q[0], 0);
    EXPECT_EQ(q[1] & ~0xFFFFull, 0);   /* the upper bits hold the (zero) pointer; low bits may carry the 'region' tag */
}

TEST(slist_push_returns_previous_first) {
    REQUIRE(INIT); REQUIRE(PUSH);
    SLIST_HEADER h; INIT(&h);
    ITEM a, b, c;
    CHECK(PUSH(&h, &a.e) == NULL);
    CHECK(PUSH(&h, &b.e) == &a.e);
    CHECK(PUSH(&h, &c.e) == &b.e);
    EXPECT_EQ(DEPTH(&h), 3);
    FLUSH(&h);
}

TEST(slist_pop_lifo_order) {
    REQUIRE(INIT); REQUIRE(PUSH); REQUIRE(POP);
    SLIST_HEADER h; INIT(&h);
    ITEM it[5];
    for (int i = 0; i < 5; i++) PUSH(&h, &it[i].e);
    for (int i = 4; i >= 0; i--) {
        PSLIST_ENTRY e = POP(&h);
        CHECKF(e == &it[i].e, "pop %d", i);
    }
    CHECK(POP(&h) == NULL);
}

TEST(slist_depth_tracks_push_pop) {
    REQUIRE(INIT); REQUIRE(PUSH); REQUIRE(POP); REQUIRE(DEPTH);
    SLIST_HEADER h; INIT(&h);
    ITEM it[8];
    for (int i = 0; i < 8; i++) { PUSH(&h, &it[i].e); EXPECT_EQ(DEPTH(&h), i + 1); }
    for (int i = 7; i >= 0; i--) { POP(&h); EXPECT_EQ(DEPTH(&h), i); }
}

TEST(slist_flush_returns_chain_in_lifo_order_and_empties) {
    REQUIRE(INIT); REQUIRE(PUSH); REQUIRE(FLUSH);
    SLIST_HEADER h; INIT(&h);
    ITEM it[4];
    for (int i = 0; i < 4; i++) PUSH(&h, &it[i].e);
    PSLIST_ENTRY e = FLUSH(&h);
    EXPECT_EQ(DEPTH(&h), 0);
    CHECK(POP(&h) == NULL);
    int i = 3;
    while (e) { CHECKF(e == &it[i].e, "chain %d", i); e = e->Next; i--; }
    EXPECT_EQ(i, -1);
}

TEST(slist_flush_empty_twice) {
    REQUIRE(INIT); REQUIRE(FLUSH);
    SLIST_HEADER h; INIT(&h);
    CHECK(FLUSH(&h) == NULL);
    CHECK(FLUSH(&h) == NULL);
}

TEST(slist_reuse_after_flush) {
    REQUIRE(INIT); REQUIRE(PUSH); REQUIRE(FLUSH); REQUIRE(POP);
    SLIST_HEADER h; INIT(&h);
    ITEM a, b;
    PUSH(&h, &a.e); FLUSH(&h);
    CHECK(PUSH(&h, &b.e) == NULL);
    CHECK(POP(&h) == &b.e);
}

TEST(slist_reinit_discards_entries) {
    REQUIRE(INIT); REQUIRE(PUSH); REQUIRE(DEPTH);
    SLIST_HEADER h; INIT(&h);
    ITEM a; PUSH(&h, &a.e);
    INIT(&h);
    EXPECT_EQ(DEPTH(&h), 0);
}

TEST(slist_next_pointer_links) {
    REQUIRE(INIT); REQUIRE(PUSH);
    SLIST_HEADER h; INIT(&h);
    ITEM a, b;
    PUSH(&h, &a.e); PUSH(&h, &b.e);
    CHECK(b.e.Next == &a.e);
    CHECK(a.e.Next == NULL);
    FLUSH(&h);
}

TEST(slist_push_overwrites_entry_next) {
    REQUIRE(INIT); REQUIRE(PUSH);
    SLIST_HEADER h; INIT(&h);
    ITEM a; a.e.Next = (PSLIST_ENTRY)0x1234;
    PUSH(&h, &a.e);
    CHECK(a.e.Next == NULL);
    FLUSH(&h);
}

TEST(slist_many_entries_1000) {
    REQUIRE(INIT); REQUIRE(PUSH); REQUIRE(POP); REQUIRE(DEPTH);
    SLIST_HEADER h; INIT(&h);
    ITEM *it = (ITEM *)_aligned_malloc(sizeof(ITEM) * 1000, 16);
    REQUIRE(it);
    for (int i = 0; i < 1000; i++) { it[i].id = i; PUSH(&h, &it[i].e); }
    EXPECT_EQ(DEPTH(&h), 1000);
    for (int i = 999; i >= 0; i--) { ITEM *p = (ITEM *)POP(&h); if (!p || p->id != (ULONG)i) { CHECKF(0, "pop %d", i); break; } }
    EXPECT_EQ(DEPTH(&h), 0);
    _aligned_free(it);
}

TEST(slist_depth_wraps_at_16_bits_or_saturates) {
    REQUIRE(INIT); REQUIRE(PUSH); REQUIRE(DEPTH); REQUIRE(FLUSH);
    SLIST_HEADER h; INIT(&h);
    int n = 70000;
    ITEM *it = (ITEM *)_aligned_malloc(sizeof(ITEM) * n, 16);
    REQUIRE(it);
    for (int i = 0; i < n; i++) PUSH(&h, &it[i].e);
    USHORT d = DEPTH(&h);
    t_note("QueryDepthSList after 70000 pushes = %u", d);
    CHECK(d == (USHORT)n || d == 0xFFFF || d == (USHORT)(n & 0xFFFF));
    FLUSH(&h);
    _aligned_free(it);
}

TEST(slist_push_list_basic) {
    REQUIRE(INIT); REQUIRE(UN(InterlockedPushListSList)); REQUIRE(POP); REQUIRE(DEPTH);
    SLIST_HEADER h; INIT(&h);
    ITEM a, b, c;
    a.e.Next = &b.e; b.e.Next = &c.e; c.e.Next = NULL;
    PSLIST_ENTRY prev = UN(InterlockedPushListSList)(&h, &a.e, &c.e, 3);
    CHECK(prev == NULL);
    EXPECT_EQ(DEPTH(&h), 3);
    CHECK(POP(&h) == &a.e);
    CHECK(POP(&h) == &b.e);
    CHECK(POP(&h) == &c.e);
}

TEST(slist_push_list_onto_nonempty_returns_previous) {
    REQUIRE(INIT); REQUIRE(UN(InterlockedPushListSList)); REQUIRE(PUSH); REQUIRE(DEPTH);
    SLIST_HEADER h; INIT(&h);
    ITEM base, a, b;
    PUSH(&h, &base.e);
    a.e.Next = &b.e; b.e.Next = NULL;
    PSLIST_ENTRY prev = UN(InterlockedPushListSList)(&h, &a.e, &b.e, 2);
    CHECK(prev == &base.e);
    CHECK(b.e.Next == &base.e);
    EXPECT_EQ(DEPTH(&h), 3);
    FLUSH(&h);
}

TEST(slist_push_list_single_entry) {
    REQUIRE(INIT); REQUIRE(UN(InterlockedPushListSList)); REQUIRE(DEPTH);
    SLIST_HEADER h; INIT(&h);
    ITEM a; a.e.Next = NULL;
    CHECK(UN(InterlockedPushListSList)(&h, &a.e, &a.e, 1) == NULL);
    EXPECT_EQ(DEPTH(&h), 1);
    FLUSH(&h);
}

TEST(slist_push_list_ex_basic) {
    REQUIRE(INIT); REQUIRE(UN(InterlockedPushListSListEx)); REQUIRE(DEPTH); REQUIRE(POP);
    SLIST_HEADER h; INIT(&h);
    ITEM a, b;
    a.e.Next = &b.e; b.e.Next = NULL;
    CHECK(UN(InterlockedPushListSListEx)(&h, &a.e, &b.e, 2) == NULL);
    EXPECT_EQ(DEPTH(&h), 2);
    CHECK(POP(&h) == &a.e);
    CHECK(POP(&h) == &b.e);
}

TEST(slist_push_list_ex_equals_push_list) {
    REQUIRE(INIT); REQUIRE(UN(InterlockedPushListSListEx)); REQUIRE(UN(InterlockedPushListSList)); REQUIRE(DEPTH);
    SLIST_HEADER h1, h2; INIT(&h1); INIT(&h2);
    ITEM a, b, c, d;
    a.e.Next = &b.e; b.e.Next = NULL; c.e.Next = &d.e; d.e.Next = NULL;
    UN(InterlockedPushListSList)(&h1, &a.e, &b.e, 2);
    UN(InterlockedPushListSListEx)(&h2, &c.e, &d.e, 2);
    EXPECT_EQ(DEPTH(&h1), DEPTH(&h2));
    FLUSH(&h1); FLUSH(&h2);
}

TEST(slist_ntdll_first_entry) {
    REQUIRE(INIT); REQUIRE(PUSH); REQUIRE(UN(RtlFirstEntrySList));
    FARPROC fe = t_proc_nt("RtlFirstEntrySList"); REQUIRE(fe);
    SLIST_HEADER h; INIT(&h);
    PSLIST_ENTRY (WINAPI *first)(const SLIST_HEADER *) = (void *)fe;
    CHECK(first(&h) == NULL);
    ITEM a, b;
    PUSH(&h, &a.e); CHECK(first(&h) == &a.e);
    PUSH(&h, &b.e); CHECK(first(&h) == &b.e);
    FLUSH(&h);
}

TEST(slist_ntdll_functions_interoperate_with_kernel32) {
    REQUIRE(INIT);
    void (WINAPI *rinit)(PSLIST_HEADER) = (void *)t_proc_nt("RtlInitializeSListHead");
    PSLIST_ENTRY (WINAPI *rpush)(PSLIST_HEADER, PSLIST_ENTRY) = (void *)t_proc_nt("RtlInterlockedPushEntrySList");
    PSLIST_ENTRY (WINAPI *rpop)(PSLIST_HEADER) = (void *)t_proc_nt("RtlInterlockedPopEntrySList");
    USHORT (WINAPI *rdepth)(PSLIST_HEADER) = (void *)t_proc_nt("RtlQueryDepthSList");
    REQUIRE(rinit); REQUIRE(rpush); REQUIRE(rpop); REQUIRE(rdepth);
    SLIST_HEADER h; rinit(&h);
    ITEM a, b;
    rpush(&h, &a.e);
    PUSH(&h, &b.e);
    EXPECT_EQ(DEPTH(&h), 2);
    EXPECT_EQ(rdepth(&h), 2);
    CHECK(rpop(&h) == &b.e);
    CHECK(POP(&h) == &a.e);
}

TEST(slist_header_alignment_and_size) {
    CHECK(sizeof(SLIST_HEADER) == 16);
    CHECK(__alignof__(SLIST_HEADER) == 16);
    CHECK(sizeof(SLIST_ENTRY) == 16);
    CHECK(__alignof__(SLIST_ENTRY) == 16);
}

/* ---- multi-threaded ---- */
#define NT_ 4
#define PER 2000
typedef struct { SLIST_HEADER *h; ITEM *items; int idx; } targ_t;
static DWORD WINAPI pusher(LPVOID p) {
    targ_t *t = (targ_t *)p;
    for (int i = 0; i < PER; i++) PUSH(t->h, &t->items[i].e);
    return 0;
}

TEST(slist_concurrent_push_then_pop_all) {
    REQUIRE(INIT); REQUIRE(PUSH); REQUIRE(POP); REQUIRE(DEPTH);
    static SLIST_HEADER h; INIT(&h);
    ITEM *all = (ITEM *)_aligned_malloc(sizeof(ITEM) * NT_ * PER, 16);
    REQUIRE(all);
    for (int i = 0; i < NT_ * PER; i++) all[i].id = i;
    targ_t a[NT_]; HANDLE th[NT_];
    for (int i = 0; i < NT_; i++) { a[i].h = &h; a[i].items = all + i * PER; th[i] = CreateThread(NULL, 0, pusher, &a[i], 0, NULL); }
    WaitForMultipleObjects(NT_, th, TRUE, 20000);
    for (int i = 0; i < NT_; i++) CloseHandle(th[i]);
    EXPECT_EQ(DEPTH(&h), NT_ * PER);
    static unsigned char seen[NT_ * PER]; memset(seen, 0, sizeof seen);
    int cnt = 0;
    ITEM *p;
    while ((p = (ITEM *)POP(&h)) != NULL) { if (p->id < NT_ * PER) seen[p->id]++; cnt++; }
    EXPECT_EQ(cnt, NT_ * PER);
    int dup = 0, miss = 0;
    for (int i = 0; i < NT_ * PER; i++) { if (seen[i] > 1) dup++; if (!seen[i]) miss++; }
    EXPECT_EQ(dup, 0); EXPECT_EQ(miss, 0);
    _aligned_free(all);
}

static DWORD WINAPI churner(LPVOID p) {
    SLIST_HEADER *h = (SLIST_HEADER *)p;
    long n = 0;
    for (int i = 0; i < 20000; i++) {
        PSLIST_ENTRY e = POP(h);
        if (e) { PUSH(h, e); n++; }
    }
    return (DWORD)n;
}

TEST(slist_concurrent_pop_push_churn_keeps_all_entries) {
    REQUIRE(INIT); REQUIRE(PUSH); REQUIRE(POP); REQUIRE(DEPTH);
    static SLIST_HEADER h; INIT(&h);
    ITEM *all = (ITEM *)_aligned_malloc(sizeof(ITEM) * 64, 16);
    REQUIRE(all);
    for (int i = 0; i < 64; i++) PUSH(&h, &all[i].e);
    HANDLE th[NT_];
    for (int i = 0; i < NT_; i++) th[i] = CreateThread(NULL, 0, churner, &h, 0, NULL);
    WaitForMultipleObjects(NT_, th, TRUE, 30000);
    for (int i = 0; i < NT_; i++) CloseHandle(th[i]);
    EXPECT_EQ(DEPTH(&h), 64);
    FLUSH(&h);
    _aligned_free(all);
}

TEST(slist_flush_concurrent_with_push) {
    REQUIRE(INIT); REQUIRE(PUSH); REQUIRE(FLUSH);
    static SLIST_HEADER h; INIT(&h);
    ITEM *all = (ITEM *)_aligned_malloc(sizeof(ITEM) * PER, 16);
    REQUIRE(all);
    targ_t a = { &h, all, 0 };
    HANDLE th = CreateThread(NULL, 0, pusher, &a, 0, NULL);
    int total = 0;
    while (WaitForSingleObject(th, 0) == WAIT_TIMEOUT) { PSLIST_ENTRY e = FLUSH(&h); while (e) { total++; e = e->Next; } }
    PSLIST_ENTRY e = FLUSH(&h); while (e) { total++; e = e->Next; }
    CloseHandle(th);
    EXPECT_EQ(total, PER);
    _aligned_free(all);
}
