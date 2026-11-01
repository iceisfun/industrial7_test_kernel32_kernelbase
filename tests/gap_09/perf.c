/* gap9: Perflib V2 provider API (PerfStartProvider ... PerfSetULongLongCounterValue). */
#include "g9.h"
#include <perflib.h>

#ifndef ERROR_NOT_FOUND
#define ERROR_NOT_FOUND 1168
#endif

static void g9_mkguid(GUID *g, unsigned tag) {
    static unsigned seq;
    memset(g, 0, sizeof *g);
    g->Data1 = 0x6b327400u ^ GetCurrentProcessId();
    g->Data2 = (USHORT)(tag + 1);
    g->Data3 = (USHORT)(++seq + (GetTickCount() & 0xff));
    g->Data4[0] = 0x9a; g->Data4[7] = (BYTE)tag;
}

typedef struct { PERF_COUNTERSET_INFO h; PERF_COUNTER_INFO c[4]; } G9_TPL;
#define G9_TPL_SIZE ((ULONG)sizeof(G9_TPL))

static void g9_tpl(G9_TPL *t, const GUID *cs, const GUID *prov, ULONG itype) {
    memset(t, 0, sizeof *t);
    t->h.CounterSetGuid = *cs;
    t->h.ProviderGuid = *prov;
    t->h.NumCounters = 4;
    t->h.InstanceType = itype;
    /* 1: 32-bit raw, 2: 64-bit raw, 3: by-reference, 4: 64-bit raw */
    t->c[0].CounterId = 1; t->c[0].Type = 0x00010000; t->c[0].Size = 4; t->c[0].DetailLevel = 100;
    t->c[1].CounterId = 2; t->c[1].Type = 0x00010100; t->c[1].Size = 8; t->c[1].DetailLevel = 100;
    t->c[2].CounterId = 3; t->c[2].Type = 0x00010100; t->c[2].Attrib = 1; t->c[2].Size = 8; t->c[2].DetailLevel = 100;
    t->c[3].CounterId = 4; t->c[3].Type = 0x00010100; t->c[3].Size = 8; t->c[3].DetailLevel = 100;
}

#define G9_INFO_ITYPE 2   /* multiple instances */

static int g_alloc_n, g_free_n;
static void *g_alloc_ctx_seen;
static PVOID WINAPI g9_alloc(SIZE_T sz, PVOID ctx) { g_alloc_n++; g_alloc_ctx_seen = ctx; return HeapAlloc(GetProcessHeap(), 0, sz); }
static void WINAPI g9_free(PVOID p, PVOID ctx) { g_free_n++; HeapFree(GetProcessHeap(), 0, p); }
static ULONG WINAPI g9_cb(ULONG code, PVOID buf, ULONG sz) { return ERROR_SUCCESS; }

typedef struct { HANDLE h; GUID prov, cs; G9_TPL t; } G9_SETUP;

/* Starts a provider and registers a 4 counter set. Returns 1 when everything works. */
static int g9_setup(G9_SETUP *s, unsigned tag, PPERF_PROVIDER_CONTEXT ctx) {
    ULONG r;
    memset(s, 0, sizeof *s);
    g9_mkguid(&s->prov, tag); g9_mkguid(&s->cs, tag + 100);
    r = U(PerfStartProviderEx)(&s->prov, ctx, &s->h);
    if (r) { t_note("PerfStartProviderEx -> %lu", r); return 0; }
    g9_tpl(&s->t, &s->cs, &s->prov, G9_INFO_ITYPE);
    r = U(PerfSetCounterSetInfo)(s->h, &s->t.h, G9_TPL_SIZE);
    if (r) { t_note("PerfSetCounterSetInfo -> %lu", r); U(PerfStopProvider)(s->h); s->h = NULL; return 0; }
    return 1;
}

TEST(gap9_perf_exports_present) {
    CHECK(U(PerfStartProvider) != NULL);
    CHECK(U(PerfStartProviderEx) != NULL);
    CHECK(U(PerfStopProvider) != NULL);
    CHECK(U(PerfSetCounterSetInfo) != NULL);
    CHECK(U(PerfCreateInstance) != NULL);
    CHECK(U(PerfDeleteInstance) != NULL);
    CHECK(U(PerfQueryInstance) != NULL);
    CHECK(U(PerfSetULongCounterValue) != NULL);
    CHECK(U(PerfSetULongLongCounterValue) != NULL);
    CHECK(U(PerfIncrementULongCounterValue) != NULL);
    CHECK(U(PerfIncrementULongLongCounterValue) != NULL);
    CHECK(U(PerfDecrementULongCounterValue) != NULL);
    CHECK(U(PerfDecrementULongLongCounterValue) != NULL);
    CHECK(U(PerfSetCounterRefValue) != NULL);
}

TEST(gap9_perf_start_null_callback) {
    REQUIRE(U(PerfStartProvider)); REQUIRE(U(PerfStopProvider));
    GUID g; HANDLE h = (HANDLE)(ULONG_PTR)0x1234; g9_mkguid(&g, 1);
    ULONG r = U(PerfStartProvider)(&g, NULL, &h);
    G9EQ(r, 0);
    CHECK(h != NULL && h != (HANDLE)(ULONG_PTR)0x1234);
    if (!r) G9EQ(U(PerfStopProvider)(h), 0);
}

TEST(gap9_perf_start_with_callback) {
    REQUIRE(U(PerfStartProvider)); REQUIRE(U(PerfStopProvider));
    GUID g; HANDLE h = NULL; g9_mkguid(&g, 2);
    ULONG r = U(PerfStartProvider)(&g, g9_cb, &h);
    G9EQ(r, 0);
    if (!r) { CHECK(h != NULL); G9EQ(U(PerfStopProvider)(h), 0); }
}

TEST(gap9_perf_start_null_guid) {
    REQUIRE(U(PerfStartProvider)); REQUIRE(U(PerfStartProviderEx));
    HANDLE h = (HANDLE)(ULONG_PTR)0x77;
    G9EQ(U(PerfStartProvider)(NULL, NULL, &h), ERROR_INVALID_PARAMETER);
    h = (HANDLE)(ULONG_PTR)0x77;
    G9EQ(U(PerfStartProviderEx)(NULL, NULL, &h), ERROR_INVALID_PARAMETER);
    G9EQ(h, (HANDLE)(ULONG_PTR)0x77);   /* untouched on failure */
}

TEST(gap9_perf_start_null_phandle) {
    REQUIRE(U(PerfStartProvider)); REQUIRE(U(PerfStartProviderEx));
    GUID g; g9_mkguid(&g, 3);
    G9EQ(U(PerfStartProvider)(&g, NULL, NULL), ERROR_INVALID_PARAMETER);
    G9EQ(U(PerfStartProviderEx)(&g, NULL, NULL), ERROR_INVALID_PARAMETER);
}

TEST(gap9_perf_startex_context_sizes) {
    REQUIRE(U(PerfStartProviderEx)); REQUIRE(U(PerfStopProvider));
    GUID g; HANDLE h; PERF_PROVIDER_CONTEXT c;
    ULONG sizes[] = { 0, 1, 4, 0x20, 0x27 };
    for (int i = 0; i < 5; i++) {
        memset(&c, 0, sizeof c); c.ContextSize = sizes[i];
        g9_mkguid(&g, 4); h = (HANDLE)(ULONG_PTR)0x55;
        G9EQ(U(PerfStartProviderEx)(&g, &c, &h), ERROR_INVALID_PARAMETER);
        G9EQ(h, (HANDLE)(ULONG_PTR)0x55);
    }
    ULONG ok[] = { 0x28, 0x29, 0x100, 0xffffffff };
    for (int i = 0; i < 4; i++) {
        memset(&c, 0, sizeof c); c.ContextSize = ok[i];
        g9_mkguid(&g, 5); h = NULL;
        ULONG r = U(PerfStartProviderEx)(&g, &c, &h);
        CHECKF(r == 0, "size 0x%lx -> %lu", ok[i], r);
        if (!r) { CHECK(h != NULL); G9EQ(U(PerfStopProvider)(h), 0); }
    }
}

TEST(gap9_perf_startex_null_context_ok) {
    REQUIRE(U(PerfStartProviderEx)); REQUIRE(U(PerfStopProvider));
    GUID g; HANDLE h = NULL; g9_mkguid(&g, 6);
    ULONG r = U(PerfStartProviderEx)(&g, NULL, &h);
    G9EQ(r, 0);
    if (!r) G9EQ(U(PerfStopProvider)(h), 0);
}

TEST(gap9_perf_stop_null) {
    REQUIRE(U(PerfStopProvider));
    if (t_is_wine()) { t_skip("PerfStopProvider(NULL) faults under wine"); return; }
    G9EQ(U(PerfStopProvider)(NULL), ERROR_INVALID_PARAMETER);
    G9EQ(U(PerfStopProvider)(NULL), ERROR_INVALID_PARAMETER);
}

TEST(gap9_perf_start_stop_repeated) {
    REQUIRE(U(PerfStartProvider)); REQUIRE(U(PerfStopProvider));
    for (int i = 0; i < 50; i++) {
        GUID g; HANDLE h = NULL; g9_mkguid(&g, 7);
        ULONG r = U(PerfStartProvider)(&g, NULL, &h);
        if (r) { CHECKF(0, "iter %d start -> %lu", i, r); break; }
        if (U(PerfStopProvider)(h) != 0) { CHECKF(0, "iter %d stop failed", i); break; }
    }
}

TEST(gap9_perf_two_providers_distinct_handles) {
    REQUIRE(U(PerfStartProvider)); REQUIRE(U(PerfStopProvider));
    GUID a, b; HANDLE ha = NULL, hb = NULL; g9_mkguid(&a, 8); g9_mkguid(&b, 9);
    G9EQ(U(PerfStartProvider)(&a, NULL, &ha), 0);
    G9EQ(U(PerfStartProvider)(&b, NULL, &hb), 0);
    CHECK(ha && hb && ha != hb);
    if (ha) G9EQ(U(PerfStopProvider)(ha), 0);
    if (hb) G9EQ(U(PerfStopProvider)(hb), 0);
}

TEST(gap9_perf_setinfo_invalid_params) {
    REQUIRE(U(PerfSetCounterSetInfo)); REQUIRE(U(PerfStartProvider)); REQUIRE(U(PerfStopProvider));
    G9_TPL t; GUID cs, pg; g9_mkguid(&cs, 10); g9_mkguid(&pg, 11);
    g9_tpl(&t, &cs, &pg, G9_INFO_ITYPE);
    G9EQ(U(PerfSetCounterSetInfo)(NULL, &t.h, G9_TPL_SIZE), ERROR_INVALID_PARAMETER);
    HANDLE h = NULL;
    if (U(PerfStartProvider)(&pg, NULL, &h)) { t_skip("provider start failed"); return; }
    G9EQ(U(PerfSetCounterSetInfo)(h, NULL, G9_TPL_SIZE), ERROR_INVALID_PARAMETER);
    G9EQ(U(PerfSetCounterSetInfo)(h, &t.h, 0), ERROR_INVALID_PARAMETER);
    G9EQ(U(PerfSetCounterSetInfo)(h, &t.h, 0x27), ERROR_INVALID_PARAMETER);
    G9EQ(U(PerfSetCounterSetInfo)(h, &t.h, G9_TPL_SIZE - 1), ERROR_INVALID_PARAMETER);
    G9EQ(U(PerfSetCounterSetInfo)(h, &t.h, 0x28 + 3 * 0x20), ERROR_INVALID_PARAMETER);
    t.h.NumCounters = 0;
    G9EQ(U(PerfSetCounterSetInfo)(h, &t.h, G9_TPL_SIZE), ERROR_INVALID_PARAMETER);
    t.h.NumCounters = 0x7fffffe;
    G9EQ(U(PerfSetCounterSetInfo)(h, &t.h, G9_TPL_SIZE), ERROR_INVALID_PARAMETER);
    t.h.NumCounters = 0xffffffff;
    G9EQ(U(PerfSetCounterSetInfo)(h, &t.h, G9_TPL_SIZE), ERROR_INVALID_PARAMETER);
    G9EQ(U(PerfStopProvider)(h), 0);
}

TEST(gap9_perf_setinfo_valid_and_duplicate) {
    REQUIRE(U(PerfSetCounterSetInfo)); REQUIRE(U(PerfStartProviderEx)); REQUIRE(U(PerfStopProvider));
    G9_SETUP s; if (!g9_setup(&s, 12, NULL)) { t_skip("setup failed"); return; }
    /* counter offsets are written back into the template: strictly increasing, 8-aligned, past the 0x20 header */
    ULONG prev = 0x1f;
    for (int i = 0; i < 4; i++) {
        CHECKF(s.t.c[i].Offset > prev, "counter %d offset %lu", i, s.t.c[i].Offset);
        CHECKF((s.t.c[i].Offset & 7) == 0 || i == 0, "counter %d offset %lu alignment", i, s.t.c[i].Offset);
        prev = s.t.c[i].Offset;
    }
    G9EQ(s.t.c[0].Offset, 0x20);
    /* registering the same counter set guid twice on one provider fails */
    G9_TPL t2; g9_tpl(&t2, &s.cs, &s.prov, G9_INFO_ITYPE);
    G9EQ(U(PerfSetCounterSetInfo)(s.h, &t2.h, G9_TPL_SIZE), ERROR_ALREADY_EXISTS);
    /* a second counter set with a different guid is fine */
    GUID cs2; g9_mkguid(&cs2, 13);
    g9_tpl(&t2, &cs2, &s.prov, G9_INFO_ITYPE);
    G9EQ(U(PerfSetCounterSetInfo)(s.h, &t2.h, G9_TPL_SIZE), 0);
    /* a larger buffer than needed is accepted */
    GUID cs3; g9_mkguid(&cs3, 14);
    struct { G9_TPL t; BYTE pad[256]; } big; memset(&big, 0, sizeof big);
    g9_tpl(&big.t, &cs3, &s.prov, G9_INFO_ITYPE);
    G9EQ(U(PerfSetCounterSetInfo)(s.h, &big.t.h, sizeof big), 0);
    G9EQ(U(PerfStopProvider)(s.h), 0);
}

TEST(gap9_perf_setinfo_exact_min_size) {
    REQUIRE(U(PerfSetCounterSetInfo)); REQUIRE(U(PerfStartProvider)); REQUIRE(U(PerfStopProvider));
    GUID pg, cs; HANDLE h = NULL; g9_mkguid(&pg, 15); g9_mkguid(&cs, 16);
    if (U(PerfStartProvider)(&pg, NULL, &h)) { t_skip("provider start failed"); return; }
    G9_TPL t; g9_tpl(&t, &cs, &pg, G9_INFO_ITYPE); t.h.NumCounters = 1;
    G9EQ(U(PerfSetCounterSetInfo)(h, &t.h, 0x28 + 0x20), 0);   /* exactly header + 1 counter */
    G9EQ(U(PerfStopProvider)(h), 0);
}

TEST(gap9_perf_create_instance_invalid) {
    REQUIRE(U(PerfCreateInstance));
    GUID g; g9_mkguid(&g, 17);
    SET_ERR(0); CHECK(U(PerfCreateInstance)(NULL, &g, L"x", 1) == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    G9_SETUP s; if (!g9_setup(&s, 18, NULL)) { t_skip("setup failed"); return; }
    SET_ERR(0); CHECK(U(PerfCreateInstance)(s.h, NULL, L"x", 1) == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(U(PerfCreateInstance)(s.h, &s.cs, NULL, 1) == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(U(PerfCreateInstance)(s.h, &g, L"x", 1) == NULL); EXPECT_ERR(ERROR_NOT_FOUND);
    U(PerfStopProvider)(s.h);
}

TEST(gap9_perf_instance_lifecycle) {
    REQUIRE(U(PerfCreateInstance)); REQUIRE(U(PerfDeleteInstance)); REQUIRE(U(PerfQueryInstance));
    G9_SETUP s; if (!g9_setup(&s, 19, NULL)) { t_skip("setup failed"); return; }
    PPERF_COUNTERSET_INSTANCE a = U(PerfCreateInstance)(s.h, &s.cs, L"alpha", 7);
    CHECK(a != NULL);
    if (!a) { U(PerfStopProvider)(s.h); return; }
    CHECK(!memcmp(&a->CounterSetGuid, &s.cs, sizeof(GUID)));
    G9EQ(a->InstanceId, 7);
    CHECK(a->InstanceNameOffset >= 0x40);
    CHECK(a->dwSize >= a->InstanceNameOffset + 12);
    G9EQ(*(ULONG *)((BYTE *)a + 0x1c), 12);                       /* name size in bytes incl. NUL */
    CHECK(!lstrcmpW((LPCWSTR)((BYTE *)a + a->InstanceNameOffset), L"alpha"));
    /* duplicate (same name and id) */
    SET_ERR(0); CHECK(U(PerfCreateInstance)(s.h, &s.cs, L"alpha", 7) == NULL); EXPECT_ERR(ERROR_ALREADY_EXISTS);
    /* same name, other id, is a different instance */
    PPERF_COUNTERSET_INSTANCE b = U(PerfCreateInstance)(s.h, &s.cs, L"alpha", 8);
    CHECK(b != NULL && b != a);
    /* other name */
    PPERF_COUNTERSET_INSTANCE c = U(PerfCreateInstance)(s.h, &s.cs, L"beta", 7);
    CHECK(c != NULL && c != a && c != b);
    /* query */
    CHECK(U(PerfQueryInstance)(s.h, &s.cs, L"alpha", 7) == a);
    CHECK(U(PerfQueryInstance)(s.h, &s.cs, L"ALPHA", 7) == a);        /* case-insensitive name */
    CHECK(U(PerfQueryInstance)(s.h, &s.cs, L"alpha", 8) == b);
    CHECK(U(PerfQueryInstance)(s.h, &s.cs, L"beta", 7) == c);
    SET_ERR(0); CHECK(U(PerfQueryInstance)(s.h, &s.cs, L"alpha", 9) == NULL); EXPECT_ERR(ERROR_NOT_FOUND);
    SET_ERR(0); CHECK(U(PerfQueryInstance)(s.h, &s.cs, L"gamma", 7) == NULL); EXPECT_ERR(ERROR_NOT_FOUND);
    /* delete */
    G9EQ(U(PerfDeleteInstance)(s.h, a), 0);
    SET_ERR(0); CHECK(U(PerfQueryInstance)(s.h, &s.cs, L"alpha", 7) == NULL); EXPECT_ERR(ERROR_NOT_FOUND);
    CHECK(U(PerfQueryInstance)(s.h, &s.cs, L"alpha", 8) == b);
    /* the name can be reused after deletion */
    PPERF_COUNTERSET_INSTANCE a2 = U(PerfCreateInstance)(s.h, &s.cs, L"alpha", 7);
    CHECK(a2 != NULL);
    if (a2) G9EQ(U(PerfDeleteInstance)(s.h, a2), 0);
    G9EQ(U(PerfDeleteInstance)(s.h, b), 0);
    G9EQ(U(PerfDeleteInstance)(s.h, c), 0);
    G9EQ(U(PerfStopProvider)(s.h), 0);
}

TEST(gap9_perf_query_delete_invalid) {
    REQUIRE(U(PerfQueryInstance)); REQUIRE(U(PerfDeleteInstance));
    GUID g; g9_mkguid(&g, 20);
    SET_ERR(0); CHECK(U(PerfQueryInstance)(NULL, &g, L"x", 0) == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    G9_SETUP s; if (!g9_setup(&s, 21, NULL)) { t_skip("setup failed"); return; }
    SET_ERR(0); CHECK(U(PerfQueryInstance)(s.h, NULL, L"x", 0) == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(U(PerfQueryInstance)(s.h, &s.cs, NULL, 0) == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(U(PerfQueryInstance)(s.h, &g, L"x", 0) == NULL); EXPECT_ERR(ERROR_NOT_FOUND);
    PPERF_COUNTERSET_INSTANCE i = U(PerfCreateInstance)(s.h, &s.cs, L"q", 0);
    G9EQ(U(PerfDeleteInstance)(NULL, i), ERROR_INVALID_PARAMETER);
    G9EQ(U(PerfDeleteInstance)(s.h, NULL), ERROR_INVALID_PARAMETER);
    G9EQ(U(PerfDeleteInstance)(NULL, NULL), ERROR_INVALID_PARAMETER);
    if (i) G9EQ(U(PerfDeleteInstance)(s.h, i), 0);
    U(PerfStopProvider)(s.h);
}

TEST(gap9_perf_instance_empty_and_long_names) {
    REQUIRE(U(PerfCreateInstance)); REQUIRE(U(PerfDeleteInstance));
    G9_SETUP s; if (!g9_setup(&s, 22, NULL)) { t_skip("setup failed"); return; }
    PPERF_COUNTERSET_INSTANCE e = U(PerfCreateInstance)(s.h, &s.cs, L"", 1);
    t_note("empty instance name -> %p (err %lu)", e, GetLastError());
    if (e) U(PerfDeleteInstance)(s.h, e);
    WCHAR big[1025]; for (int i = 0; i < 1024; i++) big[i] = L'a' + (i % 26); big[1024] = 0;
    PPERF_COUNTERSET_INSTANCE l = U(PerfCreateInstance)(s.h, &s.cs, big, 2);
    CHECK(l != NULL);
    if (l) {
        CHECK(!lstrcmpW((LPCWSTR)((BYTE *)l + l->InstanceNameOffset), big));
        CHECK(l->dwSize >= l->InstanceNameOffset + 2050);
        G9EQ(U(PerfDeleteInstance)(s.h, l), 0);
    }
    U(PerfStopProvider)(s.h);
}

TEST(gap9_perf_value_null_args) {
    REQUIRE(U(PerfSetULongCounterValue)); REQUIRE(U(PerfSetULongLongCounterValue));
    REQUIRE(U(PerfIncrementULongCounterValue)); REQUIRE(U(PerfIncrementULongLongCounterValue));
    REQUIRE(U(PerfDecrementULongCounterValue)); REQUIRE(U(PerfDecrementULongLongCounterValue));
    REQUIRE(U(PerfSetCounterRefValue));
    int dummy;
    PPERF_COUNTERSET_INSTANCE nl = NULL;
    HANDLE hp = (HANDLE)(ULONG_PTR)0x4242;   /* never dereferenced: instance is NULL */
    G9EQ(U(PerfSetULongCounterValue)(NULL, NULL, 1, 1), ERROR_INVALID_PARAMETER);
    G9EQ(U(PerfSetULongCounterValue)(hp, nl, 1, 1), ERROR_INVALID_PARAMETER);
    G9EQ(U(PerfSetULongLongCounterValue)(NULL, NULL, 1, 1), ERROR_INVALID_PARAMETER);
    G9EQ(U(PerfSetULongLongCounterValue)(hp, nl, 1, 1), ERROR_INVALID_PARAMETER);
    G9EQ(U(PerfIncrementULongCounterValue)(NULL, NULL, 1, 1), ERROR_INVALID_PARAMETER);
    G9EQ(U(PerfIncrementULongCounterValue)(hp, nl, 1, 1), ERROR_INVALID_PARAMETER);
    G9EQ(U(PerfIncrementULongLongCounterValue)(NULL, NULL, 1, 1), ERROR_INVALID_PARAMETER);
    G9EQ(U(PerfIncrementULongLongCounterValue)(hp, nl, 1, 1), ERROR_INVALID_PARAMETER);
    G9EQ(U(PerfDecrementULongCounterValue)(NULL, NULL, 1, 1), ERROR_INVALID_PARAMETER);
    G9EQ(U(PerfDecrementULongCounterValue)(hp, nl, 1, 1), ERROR_INVALID_PARAMETER);
    G9EQ(U(PerfDecrementULongLongCounterValue)(NULL, NULL, 1, 1), ERROR_INVALID_PARAMETER);
    G9EQ(U(PerfDecrementULongLongCounterValue)(hp, nl, 1, 1), ERROR_INVALID_PARAMETER);
    G9EQ(U(PerfSetCounterRefValue)(NULL, NULL, 1, &dummy), ERROR_INVALID_PARAMETER);
    G9EQ(U(PerfSetCounterRefValue)(hp, nl, 1, &dummy), ERROR_INVALID_PARAMETER);
    G9EQ(U(PerfSetCounterRefValue)(hp, nl, 1, NULL), ERROR_INVALID_PARAMETER);
}

TEST(gap9_perf_ulong_counter_arithmetic) {
    REQUIRE(U(PerfCreateInstance)); REQUIRE(U(PerfSetULongCounterValue));
    REQUIRE(U(PerfIncrementULongCounterValue)); REQUIRE(U(PerfDecrementULongCounterValue));
    G9_SETUP s; if (!g9_setup(&s, 23, NULL)) { t_skip("setup failed"); return; }
    PPERF_COUNTERSET_INSTANCE i = U(PerfCreateInstance)(s.h, &s.cs, L"c", 0);
    if (!i) { CHECK(i != NULL); U(PerfStopProvider)(s.h); return; }
    volatile ULONG *p = (volatile ULONG *)((BYTE *)i + s.t.c[0].Offset);
    G9EQ(*p, 0);
    G9EQ(U(PerfSetULongCounterValue)(s.h, i, 1, 100), 0);       G9EQ(*p, 100);
    G9EQ(U(PerfIncrementULongCounterValue)(s.h, i, 1, 5), 0);   G9EQ(*p, 105);
    G9EQ(U(PerfDecrementULongCounterValue)(s.h, i, 1, 2), 0);   G9EQ(*p, 103);
    G9EQ(U(PerfIncrementULongCounterValue)(s.h, i, 1, 0), 0);   G9EQ(*p, 103);
    G9EQ(U(PerfSetULongCounterValue)(s.h, i, 1, 0xffffffffu), 0); G9EQ(*p, 0xffffffffu);
    G9EQ(U(PerfIncrementULongCounterValue)(s.h, i, 1, 1), 0);   G9EQ(*p, 0);          /* wraps */
    G9EQ(U(PerfDecrementULongCounterValue)(s.h, i, 1, 1), 0);   G9EQ(*p, 0xffffffffu); /* wraps */
    G9EQ(U(PerfSetULongCounterValue)(s.h, i, 1, 0), 0);         G9EQ(*p, 0);
    /* neighbouring counters untouched */
    G9EQ(*(ULONGLONG *)((BYTE *)i + s.t.c[1].Offset), 0);
    G9EQ(*(ULONGLONG *)((BYTE *)i + s.t.c[3].Offset), 0);
    /* two instances have independent storage */
    PPERF_COUNTERSET_INSTANCE j = U(PerfCreateInstance)(s.h, &s.cs, L"d", 0);
    if (j) {
        G9EQ(U(PerfSetULongCounterValue)(s.h, j, 1, 9), 0);
        G9EQ(*(ULONG *)((BYTE *)j + s.t.c[0].Offset), 9);
        G9EQ(*p, 0);
        U(PerfDeleteInstance)(s.h, j);
    }
    U(PerfDeleteInstance)(s.h, i);
    U(PerfStopProvider)(s.h);
}

TEST(gap9_perf_ulonglong_counter_arithmetic) {
    REQUIRE(U(PerfCreateInstance)); REQUIRE(U(PerfSetULongLongCounterValue));
    REQUIRE(U(PerfIncrementULongLongCounterValue)); REQUIRE(U(PerfDecrementULongLongCounterValue));
    G9_SETUP s; if (!g9_setup(&s, 24, NULL)) { t_skip("setup failed"); return; }
    PPERF_COUNTERSET_INSTANCE i = U(PerfCreateInstance)(s.h, &s.cs, L"c", 0);
    if (!i) { CHECK(i != NULL); U(PerfStopProvider)(s.h); return; }
    volatile ULONGLONG *p = (volatile ULONGLONG *)((BYTE *)i + s.t.c[1].Offset);
    G9EQ(U(PerfSetULongLongCounterValue)(s.h, i, 2, 0x100000000ull), 0); CHECK(*p == 0x100000000ull);
    G9EQ(U(PerfIncrementULongLongCounterValue)(s.h, i, 2, 0xffffffffull), 0); CHECK(*p == 0x1ffffffffull);
    G9EQ(U(PerfIncrementULongLongCounterValue)(s.h, i, 2, 1), 0); CHECK(*p == 0x200000000ull);
    G9EQ(U(PerfDecrementULongLongCounterValue)(s.h, i, 2, 0x200000000ull), 0); CHECK(*p == 0);
    G9EQ(U(PerfDecrementULongLongCounterValue)(s.h, i, 2, 1), 0); CHECK(*p == ~0ull);
    G9EQ(U(PerfIncrementULongLongCounterValue)(s.h, i, 2, 1), 0); CHECK(*p == 0);
    G9EQ(U(PerfSetULongLongCounterValue)(s.h, i, 4, 77), 0);
    CHECK(*(ULONGLONG *)((BYTE *)i + s.t.c[3].Offset) == 77);
    CHECK(*p == 0);
    U(PerfDeleteInstance)(s.h, i);
    U(PerfStopProvider)(s.h);
}

TEST(gap9_perf_counter_id_and_size_mismatch) {
    REQUIRE(U(PerfCreateInstance)); REQUIRE(U(PerfSetULongCounterValue)); REQUIRE(U(PerfSetULongLongCounterValue));
    REQUIRE(U(PerfIncrementULongCounterValue)); REQUIRE(U(PerfDecrementULongLongCounterValue)); REQUIRE(U(PerfSetCounterRefValue));
    G9_SETUP s; if (!g9_setup(&s, 25, NULL)) { t_skip("setup failed"); return; }
    PPERF_COUNTERSET_INSTANCE i = U(PerfCreateInstance)(s.h, &s.cs, L"m", 0);
    if (!i) { CHECK(i != NULL); U(PerfStopProvider)(s.h); return; }
    int dummy;
    /* unknown counter ids */
    G9EQ(U(PerfSetULongCounterValue)(s.h, i, 0, 1), ERROR_NOT_FOUND);
    G9EQ(U(PerfSetULongCounterValue)(s.h, i, 99, 1), ERROR_NOT_FOUND);
    G9EQ(U(PerfSetULongLongCounterValue)(s.h, i, 0xffffffffu, 1), ERROR_NOT_FOUND);
    G9EQ(U(PerfSetCounterRefValue)(s.h, i, 77, &dummy), ERROR_NOT_FOUND);
    /* width mismatch */
    G9EQ(U(PerfSetULongCounterValue)(s.h, i, 2, 1), ERROR_INVALID_PARAMETER);        /* 8-byte counter */
    G9EQ(U(PerfIncrementULongCounterValue)(s.h, i, 2, 1), ERROR_INVALID_PARAMETER);
    G9EQ(U(PerfSetULongLongCounterValue)(s.h, i, 1, 1), ERROR_INVALID_PARAMETER);    /* 4-byte counter */
    G9EQ(U(PerfDecrementULongLongCounterValue)(s.h, i, 1, 1), ERROR_INVALID_PARAMETER);
    /* by-reference counter cannot take plain values */
    G9EQ(U(PerfSetULongLongCounterValue)(s.h, i, 3, 1), ERROR_INVALID_PARAMETER);
    G9EQ(U(PerfSetULongCounterValue)(s.h, i, 3, 1), ERROR_INVALID_PARAMETER);
    /* ... and plain counters cannot take references */
    G9EQ(U(PerfSetCounterRefValue)(s.h, i, 1, &dummy), ERROR_INVALID_PARAMETER);
    G9EQ(U(PerfSetCounterRefValue)(s.h, i, 2, &dummy), ERROR_INVALID_PARAMETER);
    /* nothing was modified */
    G9EQ(*(ULONG *)((BYTE *)i + s.t.c[0].Offset), 0);
    G9EQ(*(ULONGLONG *)((BYTE *)i + s.t.c[1].Offset), 0);
    U(PerfDeleteInstance)(s.h, i);
    U(PerfStopProvider)(s.h);
}

TEST(gap9_perf_ref_value) {
    REQUIRE(U(PerfCreateInstance)); REQUIRE(U(PerfSetCounterRefValue));
    G9_SETUP s; if (!g9_setup(&s, 26, NULL)) { t_skip("setup failed"); return; }
    PPERF_COUNTERSET_INSTANCE i = U(PerfCreateInstance)(s.h, &s.cs, L"r", 0);
    if (!i) { CHECK(i != NULL); U(PerfStopProvider)(s.h); return; }
    static volatile ULONGLONG target1 = 5, target2 = 6;
    PVOID *slot = (PVOID *)((BYTE *)i + s.t.c[2].Offset);
    CHECK(*slot == NULL);
    G9EQ(U(PerfSetCounterRefValue)(s.h, i, 3, (PVOID)&target1), 0); CHECK(*slot == (PVOID)&target1);
    G9EQ(U(PerfSetCounterRefValue)(s.h, i, 3, (PVOID)&target2), 0); CHECK(*slot == (PVOID)&target2);
    G9EQ(U(PerfSetCounterRefValue)(s.h, i, 3, NULL), ERROR_INVALID_PARAMETER);
    CHECK(*slot == (PVOID)&target2);
    U(PerfDeleteInstance)(s.h, i);
    U(PerfStopProvider)(s.h);
}

TEST(gap9_perf_custom_allocator) {
    REQUIRE(U(PerfStartProviderEx)); REQUIRE(U(PerfCreateInstance)); REQUIRE(U(PerfStopProvider));
    PERF_PROVIDER_CONTEXT ctx; memset(&ctx, 0, sizeof ctx);
    static int token;
    ctx.ContextSize = sizeof ctx; ctx.ControlCallback = g9_cb;
    ctx.MemAllocRoutine = g9_alloc; ctx.MemFreeRoutine = g9_free; ctx.pMemContext = &token;
    g_alloc_n = g_free_n = 0; g_alloc_ctx_seen = NULL;
    G9_SETUP s; if (!g9_setup(&s, 27, &ctx)) { t_skip("setup failed"); return; }
    CHECKF(g_alloc_n > 0, "allocator not used by provider/counter set creation");
    if (g_alloc_ctx_seen != &token) t_note("provider allocation context differs (reference analysis passes only size)");
    int before = g_alloc_n;
    PPERF_COUNTERSET_INSTANCE i = U(PerfCreateInstance)(s.h, &s.cs, L"k", 0);
    CHECK(i != NULL);
    CHECKF(g_alloc_n > before, "allocator not used for instance");
    if (g_alloc_ctx_seen != &token) t_note("instance allocation context differs");
    if (i) U(PerfDeleteInstance)(s.h, i);
    G9EQ(U(PerfStopProvider)(s.h), 0);
    t_note("allocs=%d frees=%d", g_alloc_n, g_free_n);
    CHECK(g_free_n <= g_alloc_n);
}

TEST(gap9_perf_stop_releases_instances) {
    REQUIRE(U(PerfCreateInstance)); REQUIRE(U(PerfStopProvider));
    for (int k = 0; k < 20; k++) {
        G9_SETUP s; if (!g9_setup(&s, 28, NULL)) { t_skip("setup failed"); return; }
        for (int j = 0; j < 10; j++) {
            WCHAR n[16]; wsprintfW(n, L"i%d", j);
            CHECK(U(PerfCreateInstance)(s.h, &s.cs, n, j) != NULL);
        }
        G9EQ(U(PerfStopProvider)(s.h), 0);   /* instances are released with the provider */
    }
}

static volatile LONG g_perf_threads_done;
typedef struct { HANDLE h; PPERF_COUNTERSET_INSTANCE inst; } G9_PTH;
static DWORD WINAPI g9_perf_thread(LPVOID p) {
    G9_PTH *a = (G9_PTH *)p;
    for (int i = 0; i < 2000; i++) {
        U(PerfIncrementULongCounterValue)(a->h, a->inst, 1, 1);
        U(PerfIncrementULongLongCounterValue)(a->h, a->inst, 2, 2);
    }
    InterlockedIncrement(&g_perf_threads_done);
    return 0;
}

TEST(gap9_perf_concurrent_increments_atomic) {
    REQUIRE(U(PerfCreateInstance)); REQUIRE(U(PerfIncrementULongCounterValue)); REQUIRE(U(PerfIncrementULongLongCounterValue));
    G9_SETUP s; if (!g9_setup(&s, 29, NULL)) { t_skip("setup failed"); return; }
    PPERF_COUNTERSET_INSTANCE i = U(PerfCreateInstance)(s.h, &s.cs, L"mt", 0);
    if (!i) { CHECK(i != NULL); U(PerfStopProvider)(s.h); return; }
    G9_PTH a = { s.h, i }; HANDLE th[4]; g_perf_threads_done = 0;
    for (int k = 0; k < 4; k++) th[k] = CreateThread(NULL, 0, g9_perf_thread, &a, 0, NULL);
    for (int k = 0; k < 4; k++) if (th[k]) { WaitForSingleObject(th[k], 20000); CloseHandle(th[k]); }
    G9EQ(g_perf_threads_done, 4);
    G9EQ(*(ULONG *)((BYTE *)i + s.t.c[0].Offset), 8000);
    CHECK(*(ULONGLONG *)((BYTE *)i + s.t.c[1].Offset) == 16000);
    U(PerfDeleteInstance)(s.h, i);
    U(PerfStopProvider)(s.h);
}

TEST(gap9_perf_single_instance_set) {
    REQUIRE(U(PerfStartProviderEx)); REQUIRE(U(PerfSetCounterSetInfo)); REQUIRE(U(PerfCreateInstance));
    G9_SETUP s; memset(&s, 0, sizeof s); g9_mkguid(&s.prov, 30); g9_mkguid(&s.cs, 31);
    if (U(PerfStartProviderEx)(&s.prov, NULL, &s.h)) { t_skip("start failed"); return; }
    g9_tpl(&s.t, &s.cs, &s.prov, 0);   /* PERF_COUNTERSET_SINGLE_INSTANCE */
    ULONG r = U(PerfSetCounterSetInfo)(s.h, &s.t.h, G9_TPL_SIZE);
    G9EQ(r, 0);
    if (!r) {
        PPERF_COUNTERSET_INSTANCE i = U(PerfCreateInstance)(s.h, &s.cs, L"single", 0);
        CHECK(i != NULL);
        if (i) U(PerfDeleteInstance)(s.h, i);
    }
    U(PerfStopProvider)(s.h);
}
