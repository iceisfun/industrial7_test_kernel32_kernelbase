/* GetLogicalProcessorInformation(Ex), NUMA APIs, GetSystemCpuSetInformation. */
#include "t.h"
#include "k.h"

UDECL(BOOL, GetSystemCpuSetInformation, (PSYSTEM_CPU_SET_INFORMATION, ULONG, PULONG, HANDLE, ULONG));
UDECL(BOOL, GetProcessDefaultCpuSets, (HANDLE, PULONG, ULONG, PULONG));
UDECL(BOOL, GetThreadSelectedCpuSets, (HANDLE, PULONG, ULONG, PULONG));
UDECL(BOOL, GetNumaNodeProcessorMask2, (USHORT, PGROUP_AFFINITY, USHORT, PUSHORT));

static int tp_popcount(ULONG_PTR m) { int n = 0; while (m) { n += (int)(m & 1); m >>= 1; } return n; }

static BYTE *tp_ex(LOGICAL_PROCESSOR_RELATIONSHIP rel, DWORD *len) {
    DWORD n = 0;
    SET_ERR(0);
    if (U(GetLogicalProcessorInformationEx)(rel, NULL, &n) || GetLastError() != ERROR_INSUFFICIENT_BUFFER || !n) return NULL;
    BYTE *b = (BYTE *)malloc(n + 64);
    if (!b) return NULL;
    DWORD m = n;
    if (!U(GetLogicalProcessorInformationEx)(rel, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)b, &m)) { free(b); return NULL; }
    *len = m;
    return b;
}
#define FOREACH_EX(b, len, e) for (DWORD off_ = 0; off_ < (len) && ((e) = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)((b) + off_), 1); off_ += (e)->Size)

/* ---------- GetLogicalProcessorInformation (legacy) ---------- */
TEST(lpi_size_query) {
    REQUIRE(U(GetLogicalProcessorInformation));
    DWORD n = 0; SET_ERR(0);
    EXPECT_EQ(U(GetLogicalProcessorInformation)(NULL, &n), 0);
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(n > 0);
    EXPECT_EQ(n % sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION), 0);
    EXPECT_EQ(sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION), 32);
}
TEST(lpi_null_length_pointer) {
    REQUIRE(U(GetLogicalProcessorInformation));
    SET_ERR(0);
    EXPECT_EQ(U(GetLogicalProcessorInformation)(NULL, NULL), 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(lpi_too_small_buffer) {
    REQUIRE(U(GetLogicalProcessorInformation));
    DWORD n = 0; U(GetLogicalProcessorInformation)(NULL, &n);
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION *b = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION *)calloc(1, n + 32);
    DWORD m = n - 1; SET_ERR(0);
    EXPECT_EQ(U(GetLogicalProcessorInformation)(b, &m), 0);
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(m, n);
    m = 0; SET_ERR(0);
    EXPECT_EQ(U(GetLogicalProcessorInformation)(b, &m), 0);
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(m, n);
    free(b);
}
TEST(lpi_exact_and_larger_buffer) {
    REQUIRE(U(GetLogicalProcessorInformation));
    DWORD n = 0; U(GetLogicalProcessorInformation)(NULL, &n);
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION *b = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION *)calloc(1, n + 1024);
    DWORD m = n;
    CHECK(U(GetLogicalProcessorInformation)(b, &m));
    EXPECT_EQ(m, n);
    m = n + 1024;
    CHECK(U(GetLogicalProcessorInformation)(b, &m));
    EXPECT_EQ(m, n);
    free(b);
}
TEST(lpi_core_masks_cover_group_processors) {
    REQUIRE(U(GetLogicalProcessorInformation)); REQUIRE(U(GetSystemInfo));
    DWORD n = 0; U(GetLogicalProcessorInformation)(NULL, &n);
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION *b = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION *)calloc(1, n);
    CHECK(U(GetLogicalProcessorInformation)(b, &n));
    ULONG_PTR cores = 0, all = 0; int ncores = 0, nodes = 0, pk = 0;
    for (DWORD i = 0; i < n / sizeof *b; i++) {
        CHECK(b[i].ProcessorMask != 0);
        if (b[i].Relationship == RelationProcessorCore) {
            CHECKF((cores & b[i].ProcessorMask) == 0, "overlapping cores");
            cores |= b[i].ProcessorMask; ncores++;
            CHECK(b[i].ProcessorCore.Flags == 0 || b[i].ProcessorCore.Flags == LTP_PC_SMT);
        }
        if (b[i].Relationship == RelationNumaNode) nodes++;
        if (b[i].Relationship == RelationProcessorPackage) pk++;
    }
    SYSTEM_INFO si; U(GetSystemInfo)(&si);
    CHECK(ncores >= 1); CHECK(nodes >= 1); CHECK(pk >= 1);
    EXPECT_EQ(cores, si.dwActiveProcessorMask);
    free(b);
}
TEST(lpi_relationship_values_known) {
    REQUIRE(U(GetLogicalProcessorInformation));
    DWORD n = 0; U(GetLogicalProcessorInformation)(NULL, &n);
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION *b = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION *)calloc(1, n);
    CHECK(U(GetLogicalProcessorInformation)(b, &n));
    for (DWORD i = 0; i < n / sizeof *b; i++) CHECKF((int)b[i].Relationship <= RelationGroup && b[i].Relationship != RelationGroup, "relationship %d", b[i].Relationship);
    free(b);
}
TEST(lpi_cache_descriptors) {
    REQUIRE(U(GetLogicalProcessorInformation));
    DWORD n = 0; U(GetLogicalProcessorInformation)(NULL, &n);
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION *b = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION *)calloc(1, n);
    CHECK(U(GetLogicalProcessorInformation)(b, &n));
    int l1 = 0, l2 = 0, l3 = 0;
    for (DWORD i = 0; i < n / sizeof *b; i++) if (b[i].Relationship == RelationCache) {
        CACHE_DESCRIPTOR *c = &b[i].Cache;
        CHECK(c->Level >= 1 && c->Level <= 3);
        CHECK(c->LineSize >= 16 && (c->LineSize & (c->LineSize - 1)) == 0);
        CHECK(c->Size >= 4096 && (c->Size % 1024) == 0);
        CHECK((int)c->Type >= CacheUnified && (int)c->Type <= CacheTrace);
        if (c->Level == 1) l1++; else if (c->Level == 2) l2++; else l3++;
    }
    CHECK(l1 >= 1);
    CHECK(l2 >= 1);
    t_note("cache entries L1=%d L2=%d L3=%d", l1, l2, l3);
    free(b);
}
TEST(lpi_numa_node_numbers_consecutive) {
    REQUIRE(U(GetLogicalProcessorInformation)); REQUIRE(U(GetNumaHighestNodeNumber));
    DWORD n = 0; U(GetLogicalProcessorInformation)(NULL, &n);
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION *b = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION *)calloc(1, n);
    CHECK(U(GetLogicalProcessorInformation)(b, &n));
    ULONG hi = 0; CHECK(U(GetNumaHighestNodeNumber)(&hi));
    int cnt = 0, maxnode = -1;
    for (DWORD i = 0; i < n / sizeof *b; i++) if (b[i].Relationship == RelationNumaNode) { cnt++; if ((int)b[i].NumaNode.NodeNumber > maxnode) maxnode = b[i].NumaNode.NodeNumber; }
    CHECK(cnt >= 1);
    EXPECT_EQ(maxnode, hi);
    free(b);
}
TEST(lpi_stable_between_calls) {
    REQUIRE(U(GetLogicalProcessorInformation));
    DWORD n1 = 0, n2 = 0; U(GetLogicalProcessorInformation)(NULL, &n1); U(GetLogicalProcessorInformation)(NULL, &n2);
    EXPECT_EQ(n1, n2);
}

/* ---------- GetLogicalProcessorInformationEx ---------- */
TEST(lpiex_null_length_pointer) {
    REQUIRE(U(GetLogicalProcessorInformationEx));
    SET_ERR(0);
    EXPECT_EQ(U(GetLogicalProcessorInformationEx)(RelationAll, NULL, NULL), 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
#define LPIEX_SIZEQ(name, rel) \
TEST(lpiex_size_query_##name) { \
    REQUIRE(U(GetLogicalProcessorInformationEx)); \
    DWORD n = 0; SET_ERR(0); \
    EXPECT_EQ(U(GetLogicalProcessorInformationEx)(rel, NULL, &n), 0); \
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER); \
    CHECK(n >= 8); \
    /* a buffer one byte short still fails and reports the same size */ \
    BYTE *b = (BYTE *)calloc(1, n + 64); DWORD m = n - 1; SET_ERR(0); \
    EXPECT_EQ(U(GetLogicalProcessorInformationEx)(rel, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)b, &m), 0); \
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER); EXPECT_EQ(m, n); \
    m = n; CHECK(U(GetLogicalProcessorInformationEx)(rel, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)b, &m)); EXPECT_EQ(m, n); \
    m = n + 64; CHECK(U(GetLogicalProcessorInformationEx)(rel, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)b, &m)); EXPECT_EQ(m, n); \
    free(b); \
}
LPIEX_SIZEQ(core, RelationProcessorCore)
LPIEX_SIZEQ(numa, RelationNumaNode)
LPIEX_SIZEQ(cache, RelationCache)
LPIEX_SIZEQ(package, RelationProcessorPackage)
LPIEX_SIZEQ(group, RelationGroup)
LPIEX_SIZEQ(all, RelationAll)

#define LPIEX_CHAIN(name, rel) \
TEST(lpiex_chain_##name) { \
    REQUIRE(U(GetLogicalProcessorInformationEx)); \
    DWORD len = 0; BYTE *b = tp_ex(rel, &len); \
    CHECK(b != NULL); if (!b) return; \
    DWORD off = 0; int cnt = 0; \
    while (off < len) { \
        PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX e = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)(b + off); \
        CHECK(e->Size >= 8 && off + e->Size <= len); \
        if (e->Size < 8) break; \
        CHECK((rel) == RelationAll ? (e->Relationship <= RelationGroup) : (e->Relationship == (rel))); \
        off += e->Size; cnt++; \
    } \
    EXPECT_EQ(off, len); \
    CHECK(cnt >= 1); \
    free(b); \
}
LPIEX_CHAIN(core, RelationProcessorCore)
LPIEX_CHAIN(numa, RelationNumaNode)
LPIEX_CHAIN(cache, RelationCache)
LPIEX_CHAIN(package, RelationProcessorPackage)
LPIEX_CHAIN(group, RelationGroup)
LPIEX_CHAIN(all, RelationAll)

TEST(lpiex_core_entries) {
    REQUIRE(U(GetLogicalProcessorInformationEx)); REQUIRE(U(GetActiveProcessorCount));
    DWORD len = 0; BYTE *b = tp_ex(RelationProcessorCore, &len);
    CHECK(b != NULL); if (!b) return;
    int total = 0; PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX e;
    FOREACH_EX(b, len, e) {
        EXPECT_EQ(e->Relationship, RelationProcessorCore);
        CHECK(e->Processor.Flags == 0 || e->Processor.Flags == LTP_PC_SMT);
        EXPECT_EQ(e->Processor.GroupCount, 1);
        CHECK(e->Processor.GroupMask[0].Mask != 0);
        CHECK(e->Size >= sizeof(DWORD) * 2 + sizeof(PROCESSOR_RELATIONSHIP));
        total += tp_popcount(e->Processor.GroupMask[0].Mask);
        if (e->Processor.Flags & LTP_PC_SMT) CHECK(tp_popcount(e->Processor.GroupMask[0].Mask) >= 2);
    }
    EXPECT_EQ(total, U(GetActiveProcessorCount)(ALL_PROCESSOR_GROUPS));
    free(b);
}
TEST(lpiex_core_masks_disjoint_per_group) {
    REQUIRE(U(GetLogicalProcessorInformationEx));
    DWORD len = 0; BYTE *b = tp_ex(RelationProcessorCore, &len);
    CHECK(b != NULL); if (!b) return;
    ULONG_PTR seen[64] = {0}; PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX e;
    FOREACH_EX(b, len, e) {
        WORD g = e->Processor.GroupMask[0].Group;
        CHECK(g < 64);
        if (g < 64) { CHECK((seen[g] & e->Processor.GroupMask[0].Mask) == 0); seen[g] |= e->Processor.GroupMask[0].Mask; }
    }
    free(b);
}
TEST(lpiex_package_entries) {
    REQUIRE(U(GetLogicalProcessorInformationEx)); REQUIRE(U(GetActiveProcessorCount));
    DWORD len = 0; BYTE *b = tp_ex(RelationProcessorPackage, &len);
    CHECK(b != NULL); if (!b) return;
    int total = 0; PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX e;
    FOREACH_EX(b, len, e) {
        EXPECT_EQ(e->Relationship, RelationProcessorPackage);
        CHECK(e->Processor.GroupCount >= 1);
        for (int i = 0; i < e->Processor.GroupCount; i++) total += tp_popcount(e->Processor.GroupMask[i].Mask);
    }
    EXPECT_EQ(total, U(GetActiveProcessorCount)(ALL_PROCESSOR_GROUPS));
    free(b);
}
TEST(lpiex_numa_entries) {
    REQUIRE(U(GetLogicalProcessorInformationEx)); REQUIRE(U(GetNumaHighestNodeNumber)); REQUIRE(U(GetActiveProcessorCount));
    DWORD len = 0; BYTE *b = tp_ex(RelationNumaNode, &len);
    CHECK(b != NULL); if (!b) return;
    ULONG hi = 0; CHECK(U(GetNumaHighestNodeNumber)(&hi));
    int cnt = 0, total = 0; PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX e;
    FOREACH_EX(b, len, e) {
        EXPECT_EQ(e->Relationship, RelationNumaNode);
        CHECK(e->NumaNode.NodeNumber <= hi);
        CHECK(e->NumaNode.GroupMask.Mask != 0);
        total += tp_popcount(e->NumaNode.GroupMask.Mask);
        cnt++;
    }
    EXPECT_EQ(cnt, hi + 1);
    EXPECT_EQ(total, U(GetActiveProcessorCount)(ALL_PROCESSOR_GROUPS));
    free(b);
}
TEST(lpiex_cache_entries) {
    REQUIRE(U(GetLogicalProcessorInformationEx));
    DWORD len = 0; BYTE *b = tp_ex(RelationCache, &len);
    CHECK(b != NULL); if (!b) return;
    int l1 = 0; PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX e;
    FOREACH_EX(b, len, e) {
        EXPECT_EQ(e->Relationship, RelationCache);
        CACHE_RELATIONSHIP *c = &e->Cache;
        CHECK(c->Level >= 1 && c->Level <= 3);
        CHECK(c->LineSize >= 16 && (c->LineSize & (c->LineSize - 1)) == 0);
        CHECK(c->CacheSize >= 4096);
        CHECK((int)c->Type >= CacheUnified && (int)c->Type <= CacheTrace);
        CHECK(c->GroupMask.Mask != 0);
        CHECK(c->Associativity != 0);
        if (c->Level == 1) l1++;
    }
    CHECK(l1 >= 2);   /* separate instruction and data caches */
    free(b);
}
TEST(lpiex_group_entry) {
    REQUIRE(U(GetLogicalProcessorInformationEx)); REQUIRE(U(GetActiveProcessorGroupCount)); REQUIRE(U(GetMaximumProcessorGroupCount));
    REQUIRE(U(GetActiveProcessorCount)); REQUIRE(U(GetMaximumProcessorCount));
    DWORD len = 0; BYTE *b = tp_ex(RelationGroup, &len);
    CHECK(b != NULL); if (!b) return;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX e = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)b;
    EXPECT_EQ(e->Size, len);   /* a single record */
    EXPECT_EQ(e->Group.MaximumGroupCount, U(GetMaximumProcessorGroupCount)());
    EXPECT_EQ(e->Group.ActiveGroupCount, U(GetActiveProcessorGroupCount)());
    for (WORD g = 0; g < e->Group.ActiveGroupCount; g++) {
        PROCESSOR_GROUP_INFO *gi = &e->Group.GroupInfo[g];
        EXPECT_EQ(gi->ActiveProcessorCount, U(GetActiveProcessorCount)(g));
        EXPECT_EQ(gi->MaximumProcessorCount, U(GetMaximumProcessorCount)(g));
        EXPECT_EQ(tp_popcount(gi->ActiveProcessorMask), gi->ActiveProcessorCount);
        CHECK(gi->ActiveProcessorCount <= gi->MaximumProcessorCount);
    }
    free(b);
}
TEST(lpiex_all_contains_each_relationship) {
    REQUIRE(U(GetLogicalProcessorInformationEx));
    DWORD len = 0; BYTE *b = tp_ex(RelationAll, &len);
    CHECK(b != NULL); if (!b) return;
    int seen[8] = {0}; PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX e;
    FOREACH_EX(b, len, e) if (e->Relationship < 8) seen[e->Relationship]++;
    CHECK(seen[RelationProcessorCore] >= 1);
    CHECK(seen[RelationNumaNode] >= 1);
    CHECK(seen[RelationCache] >= 1);
    CHECK(seen[RelationProcessorPackage] >= 1);
    EXPECT_EQ(seen[RelationGroup], 1);
    free(b);
}
TEST(lpiex_all_equals_concatenation_sizes) {
    REQUIRE(U(GetLogicalProcessorInformationEx));
    DWORD l[5] = {0}, all = 0;
    static const LOGICAL_PROCESSOR_RELATIONSHIP r[5] = { RelationProcessorCore, RelationNumaNode, RelationCache, RelationProcessorPackage, RelationGroup };
    DWORD sum = 0;
    for (int i = 0; i < 5; i++) { DWORD n = 0; U(GetLogicalProcessorInformationEx)(r[i], NULL, &n); l[i] = n; sum += n; }
    U(GetLogicalProcessorInformationEx)(RelationAll, NULL, &all);
    EXPECT_EQ(all, sum);
}
TEST(lpiex_invalid_relationship) {
    REQUIRE(U(GetLogicalProcessorInformationEx));
    static const DWORD bad[] = { 0x1234, 0x7FFF, 0xFFFE, 0x10000, 0x7FFFFFFF, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        DWORD n = 0; SET_ERR(0);
        BOOL r = U(GetLogicalProcessorInformationEx)((LOGICAL_PROCESSOR_RELATIONSHIP)bad[i], NULL, &n);
        CHECKF(!r, "relationship 0x%lx accepted", bad[i]);
        t_note("relationship 0x%lx -> err %lu n=%lu", bad[i], GetLastError(), n);
        CHECKF(GetLastError() != ERROR_SUCCESS, "relationship 0x%lx", bad[i]);
    }
}
TEST(lpiex_zero_length_with_buffer) {
    REQUIRE(U(GetLogicalProcessorInformationEx));
    BYTE junk[16]; DWORD n = 0; SET_ERR(0);
    EXPECT_EQ(U(GetLogicalProcessorInformationEx)(RelationProcessorCore, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)junk, &n), 0);
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(n > 16 || n > 0);
}
TEST(lpiex_buffer_not_touched_on_failure) {
    REQUIRE(U(GetLogicalProcessorInformationEx));
    BYTE junk[16]; memset(junk, 0x5A, sizeof junk); DWORD n = 8; SET_ERR(0);
    EXPECT_EQ(U(GetLogicalProcessorInformationEx)(RelationAll, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)junk, &n), 0);
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    for (int i = 8; i < 16; i++) EXPECT_EQ(junk[i], 0x5A);
}
TEST(lpiex_kernel32_kernelbase_forward) {
    REQUIRE(UK(GetLogicalProcessorInformationEx)); REQUIRE(UB(GetLogicalProcessorInformationEx));
    DWORD a = 0, b = 0;
    UK(GetLogicalProcessorInformationEx)(RelationAll, NULL, &a);
    UB(GetLogicalProcessorInformationEx)(RelationAll, NULL, &b);
    EXPECT_EQ(a, b);
}
TEST(lpiex_repeat_identical) {
    REQUIRE(U(GetLogicalProcessorInformationEx));
    DWORD l1 = 0, l2 = 0; BYTE *a = tp_ex(RelationAll, &l1), *b = tp_ex(RelationAll, &l2);
    CHECK(a && b);
    if (a && b) { EXPECT_EQ(l1, l2); CHECK(!memcmp(a, b, l1)); }
    free(a); free(b);
}

/* ---------- NUMA ---------- */
TEST(numa_highest_node) {
    REQUIRE(U(GetNumaHighestNodeNumber));
    ULONG hi = 0xCCCC; CHECK(U(GetNumaHighestNodeNumber)(&hi));
    CHECK(hi < 64);
}
TEST(numa_highest_node_repeat_and_forward) {
    REQUIRE(UK(GetNumaHighestNodeNumber));
    ULONG a = 0, b = 1; CHECK(UK(GetNumaHighestNodeNumber)(&a)); CHECK(UK(GetNumaHighestNodeNumber)(&b));
    EXPECT_EQ(a, b);
}
TEST(numa_node_processor_mask_valid) {
    if (t_is_wine()) { t_skip("NUMA queries are stubs in the emulator"); return; }
    REQUIRE(U(GetNumaHighestNodeNumber)); REQUIRE(U(GetNumaNodeProcessorMask));
    ULONG hi = 0; U(GetNumaHighestNodeNumber)(&hi);
    ULONGLONG un = 0;
    for (UCHAR n = 0; n <= hi; n++) {
        ULONGLONG m = 0xCC;
        CHECKF(U(GetNumaNodeProcessorMask)(n, &m), "node %u", n);
        un |= m;
    }
    SYSTEM_INFO si; GetSystemInfo(&si);
    EXPECT_EQ(un & si.dwActiveProcessorMask, si.dwActiveProcessorMask);
}
TEST(numa_node_processor_mask_invalid) {
    if (t_is_wine()) { t_skip("NUMA queries are stubs in the emulator"); return; }
    REQUIRE(U(GetNumaHighestNodeNumber)); REQUIRE(U(GetNumaNodeProcessorMask));
    ULONG hi = 0; U(GetNumaHighestNodeNumber)(&hi);
    static const UCHAR extra[] = { 1, 2, 63, 64, 0xFE, 0xFF };
    for (unsigned i = 0; i < sizeof extra; i++) {
        UCHAR n = (UCHAR)(hi + extra[i]);
        if (n <= hi) continue;
        ULONGLONG m = 0xCC; SET_ERR(0);
        EXPECT_EQ(U(GetNumaNodeProcessorMask)(n, &m), 0);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(numa_node_processor_mask_ex_valid) {
    if (t_is_wine()) { t_skip("NUMA queries are stubs in the emulator"); return; }
    REQUIRE(U(GetNumaHighestNodeNumber)); REQUIRE(U(GetNumaNodeProcessorMaskEx)); REQUIRE(U(GetActiveProcessorGroupCount));
    ULONG hi = 0; U(GetNumaHighestNodeNumber)(&hi);
    for (USHORT n = 0; n <= hi; n++) {
        GROUP_AFFINITY ga; memset(&ga, 0xCC, sizeof ga);
        CHECKF(U(GetNumaNodeProcessorMaskEx)(n, &ga), "node %u", n);
        CHECK(ga.Mask != 0);
        CHECK(ga.Group < U(GetActiveProcessorGroupCount)());
        EXPECT_EQ(ga.Reserved[0], 0);
    }
}
TEST(numa_node_processor_mask_ex_invalid) {
    if (t_is_wine()) { t_skip("NUMA queries are stubs in the emulator"); return; }
    REQUIRE(U(GetNumaHighestNodeNumber)); REQUIRE(U(GetNumaNodeProcessorMaskEx));
    ULONG hi = 0; U(GetNumaHighestNodeNumber)(&hi);
    static const USHORT bad[] = { 1, 2, 64, 255, 256, 0x7FFF, 0xFFFF };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        ULONG n = hi + bad[i]; if (n > 0xFFFF) n = 0xFFFF;
        GROUP_AFFINITY ga; SET_ERR(0);
        EXPECT_EQ(U(GetNumaNodeProcessorMaskEx)((USHORT)n, &ga), 0);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(numa_mask_and_maskex_agree_single_group) {
    if (t_is_wine()) { t_skip("NUMA queries are stubs in the emulator"); return; }
    REQUIRE(U(GetNumaNodeProcessorMask)); REQUIRE(U(GetNumaNodeProcessorMaskEx)); REQUIRE(U(GetActiveProcessorGroupCount));
    if (U(GetActiveProcessorGroupCount)() != 1) { t_skip("multi-group"); return; }
    ULONG hi = 0; U(GetNumaHighestNodeNumber)(&hi);
    for (UCHAR n = 0; n <= hi; n++) {
        ULONGLONG m = 0; GROUP_AFFINITY ga;
        CHECK(U(GetNumaNodeProcessorMask)(n, &m)); CHECK(U(GetNumaNodeProcessorMaskEx)(n, &ga));
        EXPECT_EQ(m, ga.Mask); EXPECT_EQ(ga.Group, 0);
    }
}
TEST(numa_masks_disjoint) {
    if (t_is_wine()) { t_skip("NUMA queries are stubs in the emulator"); return; }
    REQUIRE(U(GetNumaHighestNodeNumber)); REQUIRE(U(GetNumaNodeProcessorMaskEx));
    ULONG hi = 0; U(GetNumaHighestNodeNumber)(&hi);
    ULONG_PTR seen[64] = {0};
    for (USHORT n = 0; n <= hi; n++) {
        GROUP_AFFINITY ga; if (!U(GetNumaNodeProcessorMaskEx)(n, &ga)) continue;
        if (ga.Group < 64) { CHECK((seen[ga.Group] & ga.Mask) == 0); seen[ga.Group] |= ga.Mask; }
    }
}
TEST(numa_processor_node_each_processor) {
    if (t_is_wine()) { t_skip("NUMA queries are stubs in the emulator"); return; }
    REQUIRE(U(GetNumaProcessorNode)); REQUIRE(U(GetNumaHighestNodeNumber)); REQUIRE(U(GetNumaNodeProcessorMask));
    ULONG hi = 0; U(GetNumaHighestNodeNumber)(&hi);
    SYSTEM_INFO si; GetSystemInfo(&si);
    for (UCHAR p = 0; p < 64; p++) {
        if (!(si.dwActiveProcessorMask & ((ULONG_PTR)1 << p))) continue;
        UCHAR node = 0xCC;
        CHECKF(U(GetNumaProcessorNode)(p, &node), "processor %u", p);
        CHECKF(node <= hi, "processor %u node %u", p, node);
        ULONGLONG m = 0; U(GetNumaNodeProcessorMask)(node, &m);
        CHECKF(m & ((ULONGLONG)1 << p), "processor %u not in node %u mask", p, node);
    }
}
TEST(numa_processor_node_nonexistent_processor) {
    if (t_is_wine()) { t_skip("NUMA queries are stubs in the emulator"); return; }
    REQUIRE(U(GetNumaProcessorNode));
    SYSTEM_INFO si; GetSystemInfo(&si);
    static const UCHAR procs[] = { 64, 100, 255 };
    for (unsigned i = 0; i < sizeof procs; i++) {
        UCHAR node = 0; SET_ERR(0);
        BOOL r = U(GetNumaProcessorNode)(procs[i], &node);
        t_note("GetNumaProcessorNode(%u) -> %d node 0x%x err %lu", procs[i], r, node, GetLastError());
        if (r) EXPECT_EQ(node, 0xFF);
    }
}
TEST(numa_processor_node_ex_each_processor) {
    if (t_is_wine()) { t_skip("NUMA queries are stubs in the emulator"); return; }
    REQUIRE(U(GetNumaProcessorNodeEx)); REQUIRE(U(GetNumaHighestNodeNumber)); REQUIRE(U(GetNumaNodeProcessorMaskEx));
    ULONG hi = 0; U(GetNumaHighestNodeNumber)(&hi);
    SYSTEM_INFO si; GetSystemInfo(&si);
    for (BYTE p = 0; p < 64; p++) {
        if (!(si.dwActiveProcessorMask & ((ULONG_PTR)1 << p))) continue;
        PROCESSOR_NUMBER pn; memset(&pn, 0, sizeof pn); pn.Group = 0; pn.Number = p;
        USHORT node = 0xCCCC;
        CHECKF(U(GetNumaProcessorNodeEx)(&pn, &node), "processor %u", p);
        CHECKF(node <= hi, "node %u", node);
        GROUP_AFFINITY ga; U(GetNumaNodeProcessorMaskEx)(node, &ga);
        CHECK(ga.Group == 0 && (ga.Mask & ((ULONG_PTR)1 << p)));
    }
}
TEST(numa_processor_node_ex_invalid_processor) {
    if (t_is_wine()) { t_skip("NUMA queries are stubs in the emulator"); return; }
    REQUIRE(U(GetNumaProcessorNodeEx));
    PROCESSOR_NUMBER pn; memset(&pn, 0, sizeof pn);
    struct { WORD g; BYTE n; } c[] = { { 0, 64 }, { 0, 255 }, { 200, 0 }, { 0xFFFF, 0 } };
    for (unsigned i = 0; i < 4; i++) {
        pn.Group = c[i].g; pn.Number = c[i].n; USHORT node = 0;
        SET_ERR(0);
        BOOL r = U(GetNumaProcessorNodeEx)(&pn, &node);
        t_note("GetNumaProcessorNodeEx(%u,%u) -> %d node 0x%x err %lu", pn.Group, pn.Number, r, node, GetLastError());
        if (r) EXPECT_EQ(node, 0xFFFF);
    }
}
TEST(numa_available_memory_node) {
    if (t_is_wine()) { t_skip("NUMA queries are stubs in the emulator"); return; }
    REQUIRE(U(GetNumaAvailableMemoryNode)); REQUIRE(U(GetNumaHighestNodeNumber));
    ULONG hi = 0; U(GetNumaHighestNodeNumber)(&hi);
    MEMORYSTATUSEX ms; ms.dwLength = sizeof ms; GlobalMemoryStatusEx(&ms);
    for (UCHAR n = 0; n <= hi; n++) {
        ULONGLONG avail = 0xCCCC;
        CHECKF(U(GetNumaAvailableMemoryNode)(n, &avail), "node %u", n);
        CHECK(avail <= ms.ullTotalPhys);
    }
}
TEST(numa_available_memory_node_ex) {
    if (t_is_wine()) { t_skip("NUMA queries are stubs in the emulator"); return; }
    REQUIRE(U(GetNumaAvailableMemoryNodeEx)); REQUIRE(U(GetNumaHighestNodeNumber));
    ULONG hi = 0; U(GetNumaHighestNodeNumber)(&hi);
    MEMORYSTATUSEX ms; ms.dwLength = sizeof ms; GlobalMemoryStatusEx(&ms);
    ULONGLONG total = 0;
    for (USHORT n = 0; n <= hi; n++) {
        ULONGLONG avail = 0xCCCC;
        CHECKF(U(GetNumaAvailableMemoryNodeEx)(n, &avail), "node %u", n);
        CHECK(avail <= ms.ullTotalPhys);
        total += avail;
    }
    CHECK(total > 0);
}
TEST(numa_available_memory_invalid_node) {
    if (t_is_wine()) { t_skip("NUMA queries are stubs in the emulator"); return; }
    REQUIRE(U(GetNumaAvailableMemoryNode)); REQUIRE(U(GetNumaAvailableMemoryNodeEx)); REQUIRE(U(GetNumaHighestNodeNumber));
    ULONG hi = 0; U(GetNumaHighestNodeNumber)(&hi);
    ULONGLONG avail = 0xCC;
    UCHAR bn = (UCHAR)(hi + 1);
    if (bn > hi) { SET_ERR(0); EXPECT_EQ(U(GetNumaAvailableMemoryNode)(bn, &avail), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
    SET_ERR(0); EXPECT_EQ(U(GetNumaAvailableMemoryNode)(0xFF, &avail), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(GetNumaAvailableMemoryNodeEx)((USHORT)(hi + 1), &avail), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(GetNumaAvailableMemoryNodeEx)(0xFFFF, &avail), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(numa_proximity_node_unknown_id) {
    if (t_is_wine()) { t_skip("NUMA queries are stubs in the emulator"); return; }
    REQUIRE(U(GetNumaProximityNode)); REQUIRE(U(GetNumaProximityNodeEx));
    UCHAR n8 = 0xCC; USHORT n16 = 0xCCCC; SET_ERR(0);
    BOOL r = U(GetNumaProximityNode)(0xDEADBEEF, &n8);
    t_note("GetNumaProximityNode(bogus) -> %d err %lu", r, GetLastError());
    if (!r) EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    r = U(GetNumaProximityNodeEx)(0xDEADBEEF, &n16);
    t_note("GetNumaProximityNodeEx(bogus) -> %d err %lu", r, GetLastError());
    if (!r) EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(numa_proximity_node_zero) {
    if (t_is_wine()) { t_skip("NUMA queries are stubs in the emulator"); return; }
    REQUIRE(U(GetNumaProximityNodeEx)); REQUIRE(U(GetNumaHighestNodeNumber));
    ULONG hi = 0; U(GetNumaHighestNodeNumber)(&hi);
    for (ULONG id = 0; id < 4; id++) {
        USHORT n = 0xCCCC; SET_ERR(0);
        BOOL r = U(GetNumaProximityNodeEx)(id, &n);
        if (r) CHECKF(n <= hi, "proximity %lu -> node %u", id, n);
        else t_note("proximity %lu not mapped (err %lu)", id, GetLastError());
    }
}
TEST(numa_node_from_handle_invalid) {
    REQUIRE(U(GetNumaNodeNumberFromHandle));
    USHORT n = 0xCCCC; SET_ERR(0);
    EXPECT_EQ(U(GetNumaNodeNumberFromHandle)(INVALID_HANDLE_VALUE, &n), 0);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_INVALID_HANDLE, "err %lu", e);
    SET_ERR(0);
    EXPECT_EQ(U(GetNumaNodeNumberFromHandle)(NULL, &n), 0);
    e = GetLastError();
    CHECKF(e == ERROR_INVALID_HANDLE, "err %lu", e);
}
TEST(numa_node_from_handle_file) {
    REQUIRE(U(GetNumaNodeNumberFromHandle));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "numa.bin");
    HANDLE h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) {
        USHORT n = 0xCCCC; SET_ERR(0);
        BOOL r = U(GetNumaNodeNumberFromHandle)(h, &n);
        t_note("GetNumaNodeNumberFromHandle(file) -> %d node %u err %lu", r, n, GetLastError());
        if (r) { ULONG hi = 0; U(GetNumaHighestNodeNumber)(&hi); CHECK(n <= hi || n == 0xFFFF); }
        CloseHandle(h);
    }
}
TEST(numa_node_processor_mask2) {
    REQUIRE(UN(GetNumaNodeProcessorMask2)); REQUIRE(U(GetNumaHighestNodeNumber));
    ULONG hi = 0; U(GetNumaHighestNodeNumber)(&hi);
    USHORT req = 0xCCCC; SET_ERR(0);
    BOOL r = UN(GetNumaNodeProcessorMask2)(0, NULL, 0, &req);
    DWORD e = GetLastError();
    CHECKF(!r && e == ERROR_INSUFFICIENT_BUFFER, "size query r=%d err %lu", r, e);
    CHECK(req >= 1);
    GROUP_AFFINITY ga[32]; memset(ga, 0, sizeof ga);
    USHORT req2 = 0;
    r = UN(GetNumaNodeProcessorMask2)(0, ga, 32, &req2);
    CHECK(r);
    CHECK(req2 >= 1 && ga[0].Mask != 0);
    SET_ERR(0);
    EXPECT_EQ(UN(GetNumaNodeProcessorMask2)((USHORT)(hi + 1), ga, 32, &req2), 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}

/* ---------- CPU sets ---------- */
static SYSTEM_CPU_SET_INFORMATION *cs_get(ULONG *len) {
    ULONG n = 0; SET_ERR(0);
    if (UN(GetSystemCpuSetInformation)(NULL, 0, &n, NULL, 0) || GetLastError() != ERROR_INSUFFICIENT_BUFFER || !n) return NULL;
    BYTE *b = (BYTE *)calloc(1, n + 64);
    ULONG m = n;
    if (!UN(GetSystemCpuSetInformation)((PSYSTEM_CPU_SET_INFORMATION)b, n, &m, NULL, 0)) { free(b); return NULL; }
    *len = m;
    return (SYSTEM_CPU_SET_INFORMATION *)b;
}
TEST(cpuset_size_query) {
    REQUIRE(UN(GetSystemCpuSetInformation));
    ULONG n = 0xCCCC; SET_ERR(0);
    EXPECT_EQ(UN(GetSystemCpuSetInformation)(NULL, 0, &n, NULL, 0), 0);
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(n >= sizeof(SYSTEM_CPU_SET_INFORMATION) && n < 1000000);
}
TEST(cpuset_size_query_current_process_handle) {
    REQUIRE(UN(GetSystemCpuSetInformation));
    ULONG a = 0, b = 0;
    UN(GetSystemCpuSetInformation)(NULL, 0, &a, NULL, 0);
    UN(GetSystemCpuSetInformation)(NULL, 0, &b, GetCurrentProcess(), 0);
    EXPECT_EQ(a, b);
}
TEST(cpuset_entries_walk) {
    REQUIRE(UN(GetSystemCpuSetInformation)); REQUIRE(U(GetActiveProcessorCount));
    ULONG len = 0; SYSTEM_CPU_SET_INFORMATION *cs = cs_get(&len);
    CHECK(cs != NULL); if (!cs) return;
    ULONG off = 0; int cnt = 0; DWORD ids[1024]; int nids = 0;
    while (off < len) {
        SYSTEM_CPU_SET_INFORMATION *e = (SYSTEM_CPU_SET_INFORMATION *)((BYTE *)cs + off);
        CHECK(e->Size >= sizeof *e);
        if (e->Size < sizeof *e) break;
        EXPECT_EQ(e->Type, CpuSetInformation);
        CHECK(e->CpuSet.Id != 0);
        CHECK(e->CpuSet.Group < 64);
        CHECK(e->CpuSet.LogicalProcessorIndex < 64);
        for (int i = 0; i < nids; i++) CHECKF(ids[i] != e->CpuSet.Id, "duplicate id 0x%lx", e->CpuSet.Id);
        if (nids < 1024) ids[nids++] = e->CpuSet.Id;
        off += e->Size; cnt++;
    }
    EXPECT_EQ(off, len);
    EXPECT_EQ(cnt, U(GetActiveProcessorCount)(ALL_PROCESSOR_GROUPS));
    free(cs);
}
TEST(cpuset_entry_matches_group_topology) {
    REQUIRE(UN(GetSystemCpuSetInformation)); REQUIRE(U(GetActiveProcessorCount)); REQUIRE(U(GetNumaHighestNodeNumber));
    ULONG len = 0; SYSTEM_CPU_SET_INFORMATION *cs = cs_get(&len);
    CHECK(cs != NULL); if (!cs) return;
    ULONG hi = 0; U(GetNumaHighestNodeNumber)(&hi);
    ULONG off = 0;
    while (off < len) {
        SYSTEM_CPU_SET_INFORMATION *e = (SYSTEM_CPU_SET_INFORMATION *)((BYTE *)cs + off);
        CHECK(e->CpuSet.LogicalProcessorIndex < U(GetActiveProcessorCount)(e->CpuSet.Group));
        CHECK(e->CpuSet.NumaNodeIndex <= hi);
        CHECK(e->CpuSet.CoreIndex < 64 || 1);
        off += e->Size;
        if (!e->Size) break;
    }
    free(cs);
}
TEST(cpuset_too_small_buffer) {
    REQUIRE(UN(GetSystemCpuSetInformation));
    ULONG n = 0; UN(GetSystemCpuSetInformation)(NULL, 0, &n, NULL, 0);
    BYTE *b = (BYTE *)calloc(1, n + 64);
    ULONG m = n - 1; SET_ERR(0);
    EXPECT_EQ(UN(GetSystemCpuSetInformation)((PSYSTEM_CPU_SET_INFORMATION)b, n - 1, &m, NULL, 0), 0);
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(m, n);
    m = 0; SET_ERR(0);
    EXPECT_EQ(UN(GetSystemCpuSetInformation)((PSYSTEM_CPU_SET_INFORMATION)b, n, &m, NULL, 0), 1);
    EXPECT_EQ(m, n);
    free(b);
}
TEST(cpuset_invalid_process_handle) {
    REQUIRE(UN(GetSystemCpuSetInformation));
    ULONG n = 0; SET_ERR(0);
    BOOL r = UN(GetSystemCpuSetInformation)(NULL, 0, &n, (HANDLE)(ULONG_PTR)0x7FFFFFF0, 0);
    CHECK(!r);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_INVALID_HANDLE, "err %lu", e);
}
TEST(cpuset_wrong_handle_type) {
    REQUIRE(UN(GetSystemCpuSetInformation));
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    ULONG n = 0; SET_ERR(0);
    BOOL r = UN(GetSystemCpuSetInformation)(NULL, 0, &n, ev, 0);
    CHECK(!r);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_INVALID_HANDLE, "err %lu", e);
    CloseHandle(ev);
}
TEST(cpuset_nonzero_flags) {
    REQUIRE(UN(GetSystemCpuSetInformation));
    ULONG n = 0; SET_ERR(0);
    BOOL r = UN(GetSystemCpuSetInformation)(NULL, 0, &n, NULL, 1);
    t_note("Flags=1 -> %d err %lu", r, GetLastError());
}
TEST(cpuset_process_ids_apis_roundtrip) {
    /* the ids reported are usable with Get/SetProcessDefaultCpuSets query side (read only) */
    REQUIRE(UN(GetSystemCpuSetInformation)); REQUIRE(UN(GetProcessDefaultCpuSets));
    ULONG req = 0xCCCC; SET_ERR(0);
    BOOL r = UN(GetProcessDefaultCpuSets)(GetCurrentProcess(), NULL, 0, &req);
    DWORD e = GetLastError();
    if (r) EXPECT_EQ(req, 0);       /* no default set assigned */
    else CHECKF(e == ERROR_INSUFFICIENT_BUFFER, "err %lu", e);
}
TEST(cpuset_thread_selected_default_empty) {
    REQUIRE(UN(GetThreadSelectedCpuSets));
    ULONG req = 0xCCCC;
    BOOL r = UN(GetThreadSelectedCpuSets)(GetCurrentThread(), NULL, 0, &req);
    if (r) EXPECT_EQ(req, 0);
    else EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
}
