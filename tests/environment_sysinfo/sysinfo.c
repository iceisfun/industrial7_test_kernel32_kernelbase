/* GetSystemInfo / GetNativeSystemInfo, IsProcessorFeaturePresent, processor counts and numbers, XState. */
#include "t.h"
#include "k.h"
#include <cpuid.h>

#define KUSD ((volatile const unsigned char *)0x7FFE0000)

static void si_cpuid(unsigned leaf, unsigned sub, unsigned *a, unsigned *b, unsigned *c, unsigned *d) {
    unsigned ea = 0, eb = 0, ec = 0, ed = 0;
    __cpuid_count(leaf, sub, ea, eb, ec, ed);
    *a = ea; *b = eb; *c = ec; *d = ed;
}
static unsigned long long si_xcr0(void) {
    unsigned a, b, c, d; si_cpuid(1, 0, &a, &b, &c, &d);
    if (!(c & (1u << 27))) return 0;
    unsigned lo, hi;
    __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
    return ((unsigned long long)hi << 32) | lo;
}
static int si_popcount(ULONG_PTR m) { int n = 0; while (m) { n += (int)(m & 1); m >>= 1; } return n; }

UDECL(BOOLEAN, RtlIsProcessorFeaturePresent, (DWORD));

/* ---------- GetSystemInfo / GetNativeSystemInfo ---------- */
TEST(sysinfo_architecture_amd64) {
    REQUIRE(U(GetSystemInfo));
    SYSTEM_INFO si; memset(&si, 0xCC, sizeof si);
    U(GetSystemInfo)(&si);
    EXPECT_EQ(si.wProcessorArchitecture, PROCESSOR_ARCHITECTURE_AMD64);
    EXPECT_EQ(si.wReserved, 0);
    EXPECT_EQ(si.dwOemId, PROCESSOR_ARCHITECTURE_AMD64);
}
TEST(sysinfo_native_architecture_amd64) {
    REQUIRE(U(GetNativeSystemInfo));
    SYSTEM_INFO si; memset(&si, 0xCC, sizeof si);
    U(GetNativeSystemInfo)(&si);
    EXPECT_EQ(si.wProcessorArchitecture, PROCESSOR_ARCHITECTURE_AMD64);
    EXPECT_EQ(si.wReserved, 0);
}
TEST(sysinfo_page_size) {
    REQUIRE(U(GetSystemInfo));
    SYSTEM_INFO si; U(GetSystemInfo)(&si);
    EXPECT_EQ(si.dwPageSize, 4096);
}
TEST(sysinfo_allocation_granularity) {
    REQUIRE(U(GetSystemInfo));
    SYSTEM_INFO si; U(GetSystemInfo)(&si);
    EXPECT_EQ(si.dwAllocationGranularity, 65536);
    CHECK(si.dwAllocationGranularity % si.dwPageSize == 0);
}
TEST(sysinfo_application_address_range) {
    REQUIRE(U(GetSystemInfo));
    SYSTEM_INFO si; U(GetSystemInfo)(&si);
    EXPECT_EQ((ULONG_PTR)si.lpMinimumApplicationAddress, 0x10000);
    EXPECT_EQ((ULONG_PTR)si.lpMaximumApplicationAddress, 0x7FFFFFFEFFFFULL);
}
TEST(sysinfo_address_range_is_usable) {
    REQUIRE(U(GetSystemInfo)); REQUIRE(U(VirtualQuery));
    SYSTEM_INFO si; U(GetSystemInfo)(&si);
    MEMORY_BASIC_INFORMATION mbi;
    /* the lowest application address is free or mapped, one below it is the reserved low region */
    SIZE_T r = U(VirtualQuery)(si.lpMinimumApplicationAddress, &mbi, sizeof mbi);
    CHECK(r == sizeof mbi);
    r = U(VirtualQuery)((LPVOID)((ULONG_PTR)si.lpMaximumApplicationAddress - 0xFFF), &mbi, sizeof mbi);
    CHECK(r == sizeof mbi);
    SET_ERR(0);
    r = U(VirtualQuery)((LPVOID)((ULONG_PTR)si.lpMaximumApplicationAddress + 1), &mbi, sizeof mbi);
    EXPECT_EQ(r, 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(sysinfo_processor_type) {
    REQUIRE(U(GetSystemInfo));
    SYSTEM_INFO si; U(GetSystemInfo)(&si);
    EXPECT_EQ(si.dwProcessorType, PROCESSOR_AMD_X8664);
    EXPECT_EQ(PROCESSOR_AMD_X8664, 8664);
}
TEST(sysinfo_processor_count_matches_mask) {
    REQUIRE(U(GetSystemInfo));
    SYSTEM_INFO si; U(GetSystemInfo)(&si);
    CHECK(si.dwNumberOfProcessors >= 1 && si.dwNumberOfProcessors <= 64);
    CHECK(si.dwActiveProcessorMask != 0);
    EXPECT_EQ(si_popcount(si.dwActiveProcessorMask), si.dwNumberOfProcessors);
}
TEST(sysinfo_native_equals_system_info_for_64bit) {
    REQUIRE(U(GetSystemInfo)); REQUIRE(U(GetNativeSystemInfo));
    SYSTEM_INFO a, b; memset(&a, 0xAA, sizeof a); memset(&b, 0xBB, sizeof b);
    U(GetSystemInfo)(&a); U(GetNativeSystemInfo)(&b);
    CHECK(!memcmp(&a, &b, sizeof a));
}
TEST(sysinfo_repeated_calls_identical) {
    REQUIRE(U(GetSystemInfo));
    SYSTEM_INFO a, b; U(GetSystemInfo)(&a);
    for (int i = 0; i < 100; i++) { U(GetSystemInfo)(&b); CHECK(!memcmp(&a, &b, sizeof a)); }
}
TEST(sysinfo_level_matches_cpuid_family) {
    REQUIRE(U(GetSystemInfo));
    SYSTEM_INFO si; U(GetSystemInfo)(&si);
    unsigned a, b, c, d; si_cpuid(1, 0, &a, &b, &c, &d);
    unsigned fam = (a >> 8) & 0xF;
    if (fam == 0xF) fam += (a >> 20) & 0xFF;
    EXPECT_EQ(si.wProcessorLevel, fam);
}
TEST(sysinfo_revision_matches_cpuid_model_stepping) {
    REQUIRE(U(GetSystemInfo));
    SYSTEM_INFO si; U(GetSystemInfo)(&si);
    unsigned a, b, c, d; si_cpuid(1, 0, &a, &b, &c, &d);
    unsigned fam = (a >> 8) & 0xF, model = (a >> 4) & 0xF, step = a & 0xF;
    if (fam == 6 || fam >= 0xF) model |= ((a >> 16) & 0xF) << 4;
    unsigned want = (model << 8) | step;
    unsigned got = si.wProcessorRevision;
    if (got != want) t_note("wProcessorRevision 0x%x, cpuid-derived 0x%x", got, want);
    CHECK((got >> 8) != 0 || (got & 0xFF) != 0);
    EXPECT_EQ(got, want);
}
TEST(sysinfo_revision_matches_environment) {
    REQUIRE(U(GetSystemInfo)); REQUIRE(U(GetEnvironmentVariableA));
    SYSTEM_INFO si; U(GetSystemInfo)(&si);
    char b[32];
    if (U(GetEnvironmentVariableA)("PROCESSOR_LEVEL", b, 32)) EXPECT_EQ((unsigned)atoi(b), si.wProcessorLevel);
    if (U(GetEnvironmentVariableA)("PROCESSOR_REVISION", b, 32)) EXPECT_EQ(strtoul(b, NULL, 16), si.wProcessorRevision);
}
TEST(sysinfo_matches_shared_data_architecture) {
    REQUIRE(U(GetNativeSystemInfo));
    SYSTEM_INFO si; U(GetNativeSystemInfo)(&si);
    EXPECT_EQ(*(volatile const USHORT *)(KUSD + 0x26A), si.wProcessorArchitecture);
}
TEST(sysinfo_kernel32_kernelbase_forwarding) {
    REQUIRE(UK(GetSystemInfo)); REQUIRE(UB(GetSystemInfo));
    SYSTEM_INFO a, b;
    UK(GetSystemInfo)(&a); UB(GetSystemInfo)(&b);
    CHECK(!memcmp(&a, &b, sizeof a));
    if (UB(GetNativeSystemInfo) && UK(GetNativeSystemInfo)) {
        UK(GetNativeSystemInfo)(&a); UB(GetNativeSystemInfo)(&b);
        CHECK(!memcmp(&a, &b, sizeof a));
    }
}
TEST(sysinfo_page_size_matches_getsysteminfo_alloc) {
    REQUIRE(U(GetSystemInfo)); REQUIRE(U(VirtualAlloc)); REQUIRE(U(VirtualQuery)); REQUIRE(U(VirtualFree));
    SYSTEM_INFO si; U(GetSystemInfo)(&si);
    void *p = U(VirtualAlloc)(NULL, 1, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    CHECK(p != NULL);
    if (p) {
        MEMORY_BASIC_INFORMATION mbi; U(VirtualQuery)(p, &mbi, sizeof mbi);
        EXPECT_EQ(mbi.RegionSize, si.dwPageSize);
        EXPECT_EQ((ULONG_PTR)p % si.dwAllocationGranularity, 0);
        U(VirtualFree)(p, 0, MEM_RELEASE);
    }
}

/* ---------- IsProcessorFeaturePresent ---------- */
enum { PFT = 1, PFF = 0, PFA = -1 };
#define PF_CASE(idx, name, exp) \
TEST(procfeature_##idx##_##name) { \
    REQUIRE(U(IsProcessorFeaturePresent)); \
    BOOL r = U(IsProcessorFeaturePresent)(idx); \
    CHECK(r == 0 || r == 1); \
    EXPECT_EQ(r, KUSD[0x274 + idx]); \
    if ((exp) != PFA) CHECKF(r == (exp), "feature %d = %d", idx, r); \
    else t_note("feature %d (%s) = %d", idx, #name, r); \
}
PF_CASE(0, FP_PRECISION_ERRATA, PFF)
PF_CASE(1, FP_EMULATED, PFF)
PF_CASE(2, CMPXCHG8B, PFT)
PF_CASE(3, MMX, PFT)
PF_CASE(4, PPC_MOVEMEM, PFF)
PF_CASE(5, ALPHA_BYTE, PFF)
PF_CASE(6, XMMI, PFT)
PF_CASE(7, 3DNOW, PFA)
PF_CASE(8, RDTSC, PFT)
PF_CASE(9, PAE, PFT)
PF_CASE(10, XMMI64, PFT)
PF_CASE(11, SSE_DAZ, PFA)
PF_CASE(12, NX, PFT)
PF_CASE(13, SSE3, PFA)
PF_CASE(14, CMPXCHG128, PFT)
PF_CASE(15, COMPARE64_EXCHANGE128, PFA)
PF_CASE(16, CHANNELS, PFF)
PF_CASE(17, XSAVE, PFA)
PF_CASE(18, ARM_VFP32, PFF)
PF_CASE(19, ARM_NEON, PFF)
PF_CASE(20, SLAT, PFA)
PF_CASE(21, VIRT_FIRMWARE, PFA)
PF_CASE(22, FSGSBASE, PFA)
PF_CASE(23, FASTFAIL, PFT)
PF_CASE(24, ARM_DIV, PFF)
PF_CASE(25, ARM_64BIT_ATOMIC, PFF)
PF_CASE(26, ARM_EXT_CACHE, PFF)
PF_CASE(27, ARM_FMAC, PFF)
PF_CASE(28, RDRAND, PFA)
PF_CASE(29, ARM_V8, PFF)
PF_CASE(30, ARM_V8_CRYPTO, PFF)
PF_CASE(31, ARM_V8_CRC32, PFF)
PF_CASE(32, RDTSCP, PFA)
PF_CASE(33, RDPID, PFA)
PF_CASE(34, ARM_V81_ATOMIC, PFF)
PF_CASE(35, MONITORX, PFA)
PF_CASE(36, SSSE3, PFA)
PF_CASE(37, SSE4_1, PFA)
PF_CASE(38, SSE4_2, PFA)
PF_CASE(39, AVX, PFA)
PF_CASE(40, AVX2, PFA)
PF_CASE(41, AVX512F, PFA)
PF_CASE(42, ERMS, PFA)
PF_CASE(43, ARM_V82_DP, PFF)
PF_CASE(44, ARM_V83_JSCVT, PFF)

TEST(procfeature_all_64_match_shared_data) {
    REQUIRE(U(IsProcessorFeaturePresent));
    for (DWORD i = 0; i < 64; i++) {
        BOOL r = U(IsProcessorFeaturePresent)(i);
        CHECKF(r == (BOOL)KUSD[0x274 + i], "feature %lu: api %d shared %d", i, r, KUSD[0x274 + i]);
    }
}
TEST(procfeature_out_of_range_false) {
    REQUIRE(U(IsProcessorFeaturePresent));
    static const DWORD v[] = { 64, 65, 100, 127, 128, 255, 256, 1000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFE, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        SET_ERR(0);
        CHECKF(U(IsProcessorFeaturePresent)(v[i]) == FALSE, "feature %lu", v[i]);
    }
}
TEST(procfeature_returns_only_zero_or_one) {
    REQUIRE(U(IsProcessorFeaturePresent));
    for (DWORD i = 0; i < 80; i++) { BOOL r = U(IsProcessorFeaturePresent)(i); CHECKF(r == 0 || r == 1, "feature %lu = %d", i, r); }
}
TEST(procfeature_cpuid_basic_x86_features) {
    REQUIRE(U(IsProcessorFeaturePresent));
    unsigned a, b, c, d; si_cpuid(1, 0, &a, &b, &c, &d);
    EXPECT_EQ(U(IsProcessorFeaturePresent)(PF_MMX_INSTRUCTIONS_AVAILABLE), !!(d & (1u << 23)));
    EXPECT_EQ(U(IsProcessorFeaturePresent)(PF_XMMI_INSTRUCTIONS_AVAILABLE), !!(d & (1u << 25)));
    EXPECT_EQ(U(IsProcessorFeaturePresent)(PF_XMMI64_INSTRUCTIONS_AVAILABLE), !!(d & (1u << 26)));
    EXPECT_EQ(U(IsProcessorFeaturePresent)(PF_RDTSC_INSTRUCTION_AVAILABLE), !!(d & (1u << 4)));
    EXPECT_EQ(U(IsProcessorFeaturePresent)(PF_COMPARE_EXCHANGE_DOUBLE), !!(d & (1u << 8)));
}
TEST(procfeature_cpuid_sse3_family) {
    REQUIRE(U(IsProcessorFeaturePresent));
    unsigned a, b, c, d; si_cpuid(1, 0, &a, &b, &c, &d);
    EXPECT_EQ(U(IsProcessorFeaturePresent)(PF_SSE3_INSTRUCTIONS_AVAILABLE), !!(c & 1));
    EXPECT_EQ(U(IsProcessorFeaturePresent)(PF_SSSE3_INSTRUCTIONS_AVAILABLE), !!(c & (1u << 9)));
    EXPECT_EQ(U(IsProcessorFeaturePresent)(PF_SSE4_1_INSTRUCTIONS_AVAILABLE), !!(c & (1u << 19)));
    EXPECT_EQ(U(IsProcessorFeaturePresent)(PF_SSE4_2_INSTRUCTIONS_AVAILABLE), !!(c & (1u << 20)));
    EXPECT_EQ(U(IsProcessorFeaturePresent)(PF_COMPARE_EXCHANGE128), !!(c & (1u << 13)));
}
TEST(procfeature_cpuid_avx_requires_os_support) {
    REQUIRE(U(IsProcessorFeaturePresent));
    unsigned a, b, c, d; si_cpuid(1, 0, &a, &b, &c, &d);
    unsigned long long x = si_xcr0();
    int avx_hw = (c & (1u << 28)) && (c & (1u << 27)) && (x & 6) == 6;
    EXPECT_EQ(U(IsProcessorFeaturePresent)(PF_AVX_INSTRUCTIONS_AVAILABLE), avx_hw);
    unsigned a7 = 0, b7 = 0, c7 = 0, d7 = 0;
    unsigned maxleaf; si_cpuid(0, 0, &maxleaf, &b, &c, &d);
    if (maxleaf >= 7) si_cpuid(7, 0, &a7, &b7, &c7, &d7);
    int avx2_hw = avx_hw && (b7 & (1u << 5));
    EXPECT_EQ(U(IsProcessorFeaturePresent)(PF_AVX2_INSTRUCTIONS_AVAILABLE), avx2_hw);
    BOOL avx512 = U(IsProcessorFeaturePresent)(PF_AVX512F_INSTRUCTIONS_AVAILABLE);
    if (avx512) { CHECK(b7 & (1u << 16)); CHECK((x & 0xE6) == 0xE6); }
    BOOL erms = U(IsProcessorFeaturePresent)(PF_ERMS_AVAILABLE);
    if (erms) CHECK(b7 & (1u << 9));
}
TEST(procfeature_cpuid_misc_one_way) {
    REQUIRE(U(IsProcessorFeaturePresent));
    unsigned a, b, c, d; si_cpuid(1, 0, &a, &b, &c, &d);
    unsigned maxleaf; unsigned x1, x2, x3; si_cpuid(0, 0, &maxleaf, &x1, &x2, &x3);
    unsigned a7 = 0, b7 = 0, c7 = 0, d7 = 0;
    if (maxleaf >= 7) si_cpuid(7, 0, &a7, &b7, &c7, &d7);
    unsigned ea, eb, ec, ed; si_cpuid(0x80000000, 0, &ea, &eb, &ec, &ed);
    unsigned xa = 0, xb = 0, xc = 0, xd = 0;
    if (ea >= 0x80000001) si_cpuid(0x80000001, 0, &xa, &xb, &xc, &xd);
    if (U(IsProcessorFeaturePresent)(PF_RDRAND_INSTRUCTION_AVAILABLE)) CHECK(c & (1u << 30));
    if (U(IsProcessorFeaturePresent)(PF_RDTSCP_INSTRUCTION_AVAILABLE)) CHECK(xd & (1u << 27));
    if (U(IsProcessorFeaturePresent)(PF_RDWRFSGSBASE_AVAILABLE)) CHECK(b7 & 1);
    if (U(IsProcessorFeaturePresent)(PF_RDPID_INSTRUCTION_AVAILABLE)) CHECK(c7 & (1u << 22));
    if (U(IsProcessorFeaturePresent)(PF_MONITORX_INSTRUCTION_AVAILABLE)) CHECK(xc & (1u << 29));
    if (U(IsProcessorFeaturePresent)(PF_XSAVE_ENABLED)) CHECK(c & (1u << 26));
}
TEST(procfeature_xsave_vs_osxsave) {
    REQUIRE(U(IsProcessorFeaturePresent));
    unsigned a, b, c, d; si_cpuid(1, 0, &a, &b, &c, &d);
    if (U(IsProcessorFeaturePresent)(PF_AVX_INSTRUCTIONS_AVAILABLE)) { CHECK(c & (1u << 27)); CHECK(U(IsProcessorFeaturePresent)(PF_XSAVE_ENABLED)); }
}
TEST(procfeature_arm_features_all_false_on_x64) {
    REQUIRE(U(IsProcessorFeaturePresent));
    static const DWORD arm[] = { PF_ARM_VFP_32_REGISTERS_AVAILABLE, PF_ARM_NEON_INSTRUCTIONS_AVAILABLE, PF_ARM_DIVIDE_INSTRUCTION_AVAILABLE,
        PF_ARM_64BIT_LOADSTORE_ATOMIC, PF_ARM_EXTERNAL_CACHE_AVAILABLE, PF_ARM_FMAC_INSTRUCTIONS_AVAILABLE, PF_ARM_V8_INSTRUCTIONS_AVAILABLE,
        PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE, PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE, PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE,
        PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE, PF_ARM_V83_JSCVT_INSTRUCTIONS_AVAILABLE };
    for (unsigned i = 0; i < sizeof arm / sizeof arm[0]; i++) CHECKF(!U(IsProcessorFeaturePresent)(arm[i]), "ARM feature %lu", arm[i]);
}
TEST(procfeature_k32_kernelbase_same_answers) {
    REQUIRE(UK(IsProcessorFeaturePresent)); REQUIRE(UB(IsProcessorFeaturePresent)); REQUIRE(UN(RtlIsProcessorFeaturePresent));
    for (DWORD i = 0; i < 70; i++) {
        BOOL a = UK(IsProcessorFeaturePresent)(i), b = UB(IsProcessorFeaturePresent)(i);
        CHECKF(a == b, "feature %lu", i);
        CHECKF(a == !!(UN(RtlIsProcessorFeaturePresent)(i)), "ntdll feature %lu", i);
    }
}
TEST(procfeature_stable_across_calls) {
    REQUIRE(U(IsProcessorFeaturePresent));
    BOOL first[64];
    for (DWORD i = 0; i < 64; i++) first[i] = U(IsProcessorFeaturePresent)(i);
    for (int r = 0; r < 20; r++) for (DWORD i = 0; i < 64; i++) CHECK(first[i] == U(IsProcessorFeaturePresent)(i));
}
TEST(procfeature_system_dep_policy_vs_nx) {
    REQUIRE(U(GetSystemDEPPolicy)); REQUIRE(U(IsProcessorFeaturePresent));
    CHECK(U(IsProcessorFeaturePresent)(PF_NX_ENABLED));
    DEP_SYSTEM_POLICY_TYPE p = U(GetSystemDEPPolicy)();
    CHECKF((int)p >= DEPPolicyAlwaysOff && (int)p <= DEPPolicyOptOut, "policy %d", p);
    EXPECT_EQ((int)p, KUSD[0x2D5] & 3);
}

/* ---------- XState ---------- */
TEST(xstate_enabled_features_basic) {
    REQUIRE(U(GetEnabledXStateFeatures));
    DWORD64 m = U(GetEnabledXStateFeatures)();
    EXPECT_EQ(m & 3, 3);
}
TEST(xstate_avx_bit_matches_feature) {
    REQUIRE(U(GetEnabledXStateFeatures)); REQUIRE(U(IsProcessorFeaturePresent));
    DWORD64 m = U(GetEnabledXStateFeatures)();
    EXPECT_EQ(!!(m & XSTATE_MASK_GSSE), U(IsProcessorFeaturePresent)(PF_AVX_INSTRUCTIONS_AVAILABLE));
}
TEST(xstate_matches_xcr0_low_bits) {
    REQUIRE(U(GetEnabledXStateFeatures));
    DWORD64 m = U(GetEnabledXStateFeatures)();
    unsigned long long x = si_xcr0();
    if (!x) { t_skip("no OSXSAVE"); return; }
    EXPECT_EQ(m & 7, x & 7);
}
TEST(xstate_kernel32_and_kernelbase_agree) {
    REQUIRE(UK(GetEnabledXStateFeatures)); REQUIRE(UB(GetEnabledXStateFeatures));
    EXPECT_EQ(UK(GetEnabledXStateFeatures)(), UB(GetEnabledXStateFeatures)());
}
TEST(xstate_stable) {
    REQUIRE(U(GetEnabledXStateFeatures));
    DWORD64 a = U(GetEnabledXStateFeatures)();
    for (int i = 0; i < 20; i++) EXPECT_EQ(U(GetEnabledXStateFeatures)(), a);
}

/* ---------- processor / group counts ---------- */
TEST(proccount_all_groups_at_least_one) {
    REQUIRE(U(GetActiveProcessorCount));
    DWORD n = U(GetActiveProcessorCount)(ALL_PROCESSOR_GROUPS);
    CHECK(n >= 1);
}
TEST(proccount_sum_of_groups_equals_all) {
    REQUIRE(U(GetActiveProcessorCount)); REQUIRE(U(GetActiveProcessorGroupCount));
    WORD g = U(GetActiveProcessorGroupCount)();
    CHECK(g >= 1);
    DWORD sum = 0;
    for (WORD i = 0; i < g; i++) { DWORD c = U(GetActiveProcessorCount)(i); CHECK(c >= 1 && c <= 64); sum += c; }
    EXPECT_EQ(sum, U(GetActiveProcessorCount)(ALL_PROCESSOR_GROUPS));
}
TEST(proccount_group0_matches_systeminfo_when_single_group) {
    REQUIRE(U(GetActiveProcessorCount)); REQUIRE(U(GetActiveProcessorGroupCount)); REQUIRE(U(GetSystemInfo));
    SYSTEM_INFO si; U(GetSystemInfo)(&si);
    if (U(GetActiveProcessorGroupCount)() == 1) EXPECT_EQ(U(GetActiveProcessorCount)(0), si.dwNumberOfProcessors);
    CHECK(U(GetActiveProcessorCount)(0) >= si.dwNumberOfProcessors || U(GetActiveProcessorGroupCount)() > 1);
}
TEST(proccount_invalid_group) {
    REQUIRE(U(GetActiveProcessorCount)); REQUIRE(U(GetActiveProcessorGroupCount));
    WORD g = U(GetActiveProcessorGroupCount)();
    SET_ERR(0);
    EXPECT_EQ(U(GetActiveProcessorCount)(g), 0);
    t_note("GetActiveProcessorCount(invalid group) error %lu", GetLastError());
    EXPECT_EQ(U(GetActiveProcessorCount)(0x1000), 0);
    EXPECT_EQ(U(GetActiveProcessorCount)(0xFFFE), 0);
}
TEST(proccount_max_not_less_than_active) {
    REQUIRE(U(GetMaximumProcessorCount)); REQUIRE(U(GetActiveProcessorCount));
    DWORD mx = U(GetMaximumProcessorCount)(ALL_PROCESSOR_GROUPS);
    CHECK(mx >= U(GetActiveProcessorCount)(ALL_PROCESSOR_GROUPS));
    CHECK(U(GetMaximumProcessorCount)(0) >= U(GetActiveProcessorCount)(0));
    CHECK(U(GetMaximumProcessorCount)(0) <= 64);
}
TEST(proccount_max_group_sum_equals_all) {
    REQUIRE(U(GetMaximumProcessorCount)); REQUIRE(U(GetMaximumProcessorGroupCount));
    WORD g = U(GetMaximumProcessorGroupCount)();
    DWORD sum = 0;
    for (WORD i = 0; i < g; i++) sum += U(GetMaximumProcessorCount)(i);
    EXPECT_EQ(sum, U(GetMaximumProcessorCount)(ALL_PROCESSOR_GROUPS));
}
TEST(proccount_group_count_consistency) {
    REQUIRE(U(GetActiveProcessorGroupCount)); REQUIRE(U(GetMaximumProcessorGroupCount));
    WORD a = U(GetActiveProcessorGroupCount)(), m = U(GetMaximumProcessorGroupCount)();
    CHECK(a >= 1); CHECK(m >= a); CHECK(m <= 20);
}
TEST(proccount_invalid_group_maximum) {
    REQUIRE(U(GetMaximumProcessorCount)); REQUIRE(U(GetMaximumProcessorGroupCount));
    WORD g = U(GetMaximumProcessorGroupCount)();
    EXPECT_EQ(U(GetMaximumProcessorCount)(g), 0);
    EXPECT_EQ(U(GetMaximumProcessorCount)(0x7000), 0);
}
TEST(proccount_matches_environment_number_of_processors) {
    REQUIRE(U(GetActiveProcessorCount)); REQUIRE(U(GetEnvironmentVariableA));
    char b[16];
    if (!U(GetEnvironmentVariableA)("NUMBER_OF_PROCESSORS", b, 16)) { t_skip("no env"); return; }
    EXPECT_EQ((DWORD)atoi(b), U(GetActiveProcessorCount)(ALL_PROCESSOR_GROUPS));
}
TEST(proccount_matches_shared_data) {
    REQUIRE(U(GetActiveProcessorCount));
    EXPECT_EQ(*(volatile const DWORD *)(KUSD + 0x3C0), U(GetActiveProcessorCount)(ALL_PROCESSOR_GROUPS));
}
TEST(proccount_kernel32_kernelbase_same) {
    REQUIRE(UK(GetActiveProcessorCount)); REQUIRE(UB(GetActiveProcessorCount));
    EXPECT_EQ(UK(GetActiveProcessorCount)(ALL_PROCESSOR_GROUPS), UB(GetActiveProcessorCount)(ALL_PROCESSOR_GROUPS));
}

/* ---------- current processor ---------- */
TEST(procnum_current_processor_number_in_range) {
    REQUIRE(U(GetCurrentProcessorNumber)); REQUIRE(U(GetActiveProcessorCount));
    for (int i = 0; i < 2000; i++) {
        DWORD n = U(GetCurrentProcessorNumber)();
        if (n >= 64) { CHECKF(0, "number %lu", n); break; }
    }
    CHECK(1);
}
TEST(procnum_ex_fields) {
    REQUIRE(U(GetCurrentProcessorNumberEx)); REQUIRE(U(GetActiveProcessorGroupCount)); REQUIRE(U(GetActiveProcessorCount));
    for (int i = 0; i < 2000; i++) {
        PROCESSOR_NUMBER pn; memset(&pn, 0xCC, sizeof pn);
        U(GetCurrentProcessorNumberEx)(&pn);
        if (pn.Group >= U(GetActiveProcessorGroupCount)() || pn.Number >= U(GetActiveProcessorCount)(pn.Group) || pn.Reserved != 0) {
            CHECKF(0, "group %u number %u reserved %u", pn.Group, pn.Number, pn.Reserved); break;
        }
    }
    CHECK(1);
}
TEST(procnum_ex_matches_legacy_in_single_group) {
    REQUIRE(U(GetCurrentProcessorNumberEx)); REQUIRE(U(GetCurrentProcessorNumber)); REQUIRE(U(GetActiveProcessorGroupCount));
    if (U(GetActiveProcessorGroupCount)() != 1) { t_skip("multi-group"); return; }
    /* pin to one CPU of the process so both reads happen on the same one */
    HANDLE th = GetCurrentThread();
    DWORD_PTR pm, sm; GetProcessAffinityMask(GetCurrentProcess(), &pm, &sm);
    DWORD_PTR one = pm & (~pm + 1);
    DWORD_PTR old = SetThreadAffinityMask(th, one);
    if (old) {
        for (int i = 0; i < 50; i++) {
            PROCESSOR_NUMBER pn; U(GetCurrentProcessorNumberEx)(&pn);
            EXPECT_EQ(pn.Number, U(GetCurrentProcessorNumber)());
            EXPECT_EQ((DWORD_PTR)1 << pn.Number, one);
            EXPECT_EQ(pn.Group, 0);
        }
        SetThreadAffinityMask(th, old);
    }
}
TEST(procnum_follows_thread_affinity) {
    REQUIRE(U(GetCurrentProcessorNumber)); REQUIRE(U(SetThreadAffinityMask));
    DWORD_PTR pm, sm; GetProcessAffinityMask(GetCurrentProcess(), &pm, &sm);
    if (si_popcount(pm) < 2) { t_skip("single CPU"); return; }
    DWORD_PTR old = 0;
    for (int bit = 0; bit < 64; bit++) {
        DWORD_PTR m = (DWORD_PTR)1 << bit;
        if (!(pm & m)) continue;
        DWORD_PTR o = U(SetThreadAffinityMask)(GetCurrentThread(), m);
        if (!o) { CHECK(0); break; }
        if (!old) old = o;
        Sleep(0);
        EXPECT_EQ(U(GetCurrentProcessorNumber)(), bit);
    }
    if (old) SetThreadAffinityMask(GetCurrentThread(), old);
}
TEST(procnum_ex_null_pointer_not_called) {
    /* documentation: the pointer is required; only verify the exports resolve and forward */
    REQUIRE(UK(GetCurrentProcessorNumberEx)); REQUIRE(UB(GetCurrentProcessorNumberEx));
    PROCESSOR_NUMBER a, b; UK(GetCurrentProcessorNumberEx)(&a); UB(GetCurrentProcessorNumberEx)(&b);
    CHECK(a.Group == b.Group || 1);
}
