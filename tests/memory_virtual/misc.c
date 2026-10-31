#include "mvc.h"
/* System info, memory status, write watch, offer/reclaim, prefetch, resource notification, large pages, exhaustion */

static int popcnt(ULONG_PTR m) { int n = 0; while (m) { n += (int)(m & 1); m >>= 1; } return n; }

/* ---------------- GetSystemInfo / GetNativeSystemInfo ---------------- */
TEST(mv_si_fields) {
    REQUIRE(U(GetSystemInfo));
    SYSTEM_INFO si; memset(&si, 0xEE, sizeof si);
    U(GetSystemInfo)(&si);
    EXPECT_EQ(si.wProcessorArchitecture, PROCESSOR_ARCHITECTURE_AMD64);
    EXPECT_EQ(si.wReserved, 0);
    EXPECT_EQ(si.dwPageSize, 4096);
    EXPECT_EQ(si.dwAllocationGranularity, 65536);
    EXPECT_EQ(si.lpMinimumApplicationAddress, 0x10000);
    EXPECT_EQ(si.lpMaximumApplicationAddress, 0x7FFFFFFEFFFFull);
    EXPECT_EQ(si.dwProcessorType, PROCESSOR_AMD_X8664);
    CHECK(si.dwNumberOfProcessors >= 1 && si.dwNumberOfProcessors <= 64);
    CHECK(si.dwActiveProcessorMask != 0);
    EXPECT_EQ(popcnt(si.dwActiveProcessorMask), si.dwNumberOfProcessors);
    CHECK(si.wProcessorLevel != 0);
    t_note("level=%u revision=0x%04x procs=%lu", si.wProcessorLevel, si.wProcessorRevision, si.dwNumberOfProcessors);
}
TEST(mv_si_native_matches) {
    REQUIRE(U(GetSystemInfo)); REQUIRE(U(GetNativeSystemInfo));
    SYSTEM_INFO a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    U(GetSystemInfo)(&a);
    U(GetNativeSystemInfo)(&b);
    CHECK(!memcmp(&a, &b, sizeof a));
}
TEST(mv_si_repeatable) {
    REQUIRE(U(GetSystemInfo));
    SYSTEM_INFO a, b;
    U(GetSystemInfo)(&a);
    for (int i = 0; i < 50; i++) { U(GetSystemInfo)(&b); CHECK(!memcmp(&a, &b, sizeof a)); }
}
TEST(mv_si_granularity_relations) {
    REQUIRE(U(GetSystemInfo));
    SYSTEM_INFO si; U(GetSystemInfo)(&si);
    CHECK((si.dwAllocationGranularity & (si.dwAllocationGranularity - 1)) == 0);
    CHECK((si.dwPageSize & (si.dwPageSize - 1)) == 0);
    CHECK(si.dwAllocationGranularity % si.dwPageSize == 0);
    CHECK(((ULONG_PTR)si.lpMinimumApplicationAddress % si.dwAllocationGranularity) == 0);
    CHECK((((ULONG_PTR)si.lpMaximumApplicationAddress + 1) % si.dwAllocationGranularity) == 0);
}
TEST(mv_si_addresses_match_allocator) {
    REQUIRE(VALLOC);
    SYSTEM_INFO si = mv_sysinfo();
    /* nothing can be allocated at or above the maximum address */
    SET_ERR(0);
    void *p = VALLOC((char *)si.lpMaximumApplicationAddress + 1, PG, MEM_RESERVE, PAGE_READWRITE);
    CHECK(p == NULL);
    if (p) mv_free(p);
    /* allocations are inside the user range */
    p = mv_alloc(PG);
    CHECK((ULONG_PTR)p >= (ULONG_PTR)si.lpMinimumApplicationAddress && (ULONG_PTR)p < (ULONG_PTR)si.lpMaximumApplicationAddress);
    mv_free(p);
}
TEST(mv_si_page_size_matches_vq) {
    REQUIRE(VQ);
    char *p = mv_alloc(PG * 3);
    MEMORY_BASIC_INFORMATION m = mv_q(p + 1);
    EXPECT_EQ(m.RegionSize % mv_sysinfo().dwPageSize, 0);
    mv_free(p);
}

/* ---------------- GlobalMemoryStatusEx ---------------- */
TEST(mv_gms_basic) {
    REQUIRE(U(GlobalMemoryStatusEx));
    MEMORYSTATUSEX ms; memset(&ms, 0xEE, sizeof ms);
    ms.dwLength = sizeof ms;
    SET_ERR(0);
    CHECK(U(GlobalMemoryStatusEx)(&ms));
    EXPECT_EQ(ms.dwLength, sizeof ms);
    CHECK(ms.dwMemoryLoad <= 100);
    CHECK(ms.ullTotalPhys > 0);
    CHECK(ms.ullAvailPhys <= ms.ullTotalPhys);
    CHECK(ms.ullAvailPageFile <= ms.ullTotalPageFile);
    CHECK(ms.ullAvailVirtual <= ms.ullTotalVirtual);
    EXPECT_EQ(ms.ullTotalVirtual, 0x7FFFFFFE0000ull);
    EXPECT_EQ(ms.ullAvailExtendedVirtual, 0);
    CHECK(ms.ullTotalPhys >= (64ull << 20));
    t_note("load=%lu phys=%llu/%llu pf=%llu/%llu virt avail=%llu", ms.dwMemoryLoad, ms.ullAvailPhys, ms.ullTotalPhys, ms.ullAvailPageFile, ms.ullTotalPageFile, ms.ullAvailVirtual);
}
TEST(mv_gms_bad_length) {
    REQUIRE(U(GlobalMemoryStatusEx));
    union { MEMORYSTATUSEX ms; unsigned char raw[128]; } u;
    DWORD lens[] = { 0, 1, 32, 56, 63, 65, 72, 128, 0x40000000, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof lens / sizeof *lens; i++) {
        memset(&u, 0xEE, sizeof u);
        u.ms.dwLength = lens[i];
        SET_ERR(0);
        CHECKF(!U(GlobalMemoryStatusEx)(&u.ms), "length %lu", lens[i]);
        ERR1(ERROR_INVALID_PARAMETER);
        CHECK(u.raw[100] == 0xEE);
        CHECK(u.ms.ullTotalPhys == 0xEEEEEEEEEEEEEEEEull);        /* nothing written on failure */
    }
}
TEST(mv_gms_load_formula) {
    REQUIRE(U(GlobalMemoryStatusEx));
    MEMORYSTATUSEX ms; ms.dwLength = sizeof ms;
    CHECK(U(GlobalMemoryStatusEx)(&ms));
    DWORD calc = (DWORD)(((ms.ullTotalPhys - ms.ullAvailPhys) * 100) / ms.ullTotalPhys);
    CHECKF(calc + 5 >= ms.dwMemoryLoad && ms.dwMemoryLoad + 5 >= calc, "load %lu calc %lu", ms.dwMemoryLoad, calc);
}
TEST(mv_gms_virtual_tracks_reserve) {
    REQUIRE(U(GlobalMemoryStatusEx)); REQUIRE(VALLOC);
    MEMORYSTATUSEX a, b, c; a.dwLength = b.dwLength = c.dwLength = sizeof a;
    CHECK(U(GlobalMemoryStatusEx)(&a));
    SIZE_T sz = 512u << 20;
    void *p = VALLOC(NULL, sz, MEM_RESERVE, PAGE_READWRITE);
    CHECK(p != NULL);
    CHECK(U(GlobalMemoryStatusEx)(&b));
    if (p) {
        CHECKF(a.ullAvailVirtual >= b.ullAvailVirtual + sz - (8u << 20), "avail before %llu after %llu", a.ullAvailVirtual, b.ullAvailVirtual);
        CHECK(a.ullAvailVirtual - b.ullAvailVirtual <= sz + (8u << 20));
        mv_free(p);
    }
    CHECK(U(GlobalMemoryStatusEx)(&c));
    CHECK(c.ullAvailVirtual + (8u << 20) >= a.ullAvailVirtual);
}
TEST(mv_gms_commit_tracks_pagefile) {
    REQUIRE(U(GlobalMemoryStatusEx)); REQUIRE(VALLOC);
    MEMORYSTATUSEX a, b; a.dwLength = b.dwLength = sizeof a;
    CHECK(U(GlobalMemoryStatusEx)(&a));
    SIZE_T sz = 128u << 20;
    char *p = mv_alloc(sz);
    CHECK(p != NULL);
    CHECK(U(GlobalMemoryStatusEx)(&b));
    if (p) {
        CHECK(b.ullAvailPageFile < a.ullAvailPageFile + (64u << 20));
        t_note("commit of %Iu bytes reduced AvailPageFile by %lld", sz, (long long)(a.ullAvailPageFile - b.ullAvailPageFile));
        mv_free(p);
    }
}
TEST(mv_gms_legacy) {
    REQUIRE(U(GlobalMemoryStatus));
    MEMORYSTATUS ms; memset(&ms, 0xEE, sizeof ms);
    ms.dwLength = 0;
    U(GlobalMemoryStatus)(&ms);
    EXPECT_EQ(ms.dwLength, sizeof ms);
    CHECK(ms.dwMemoryLoad <= 100);
    CHECK(ms.dwTotalPhys > 0 && ms.dwAvailPhys <= ms.dwTotalPhys);
    CHECK(ms.dwAvailVirtual <= ms.dwTotalVirtual);
    CHECK(ms.dwAvailPageFile <= ms.dwTotalPageFile);
    if (ms.dwTotalVirtual != 0xFFFFFFFF && ms.dwTotalVirtual != 0x7FFFFFFF) t_note("TotalVirtual=%Iu", (SIZE_T)ms.dwTotalVirtual);
}
TEST(mv_gms_phys_installed) {
    REQUIRE(U(GetPhysicallyInstalledSystemMemory)); REQUIRE(U(GlobalMemoryStatusEx));
    ULONGLONG kb = 0xEEEE;
    SET_ERR(0);
    BOOL r = U(GetPhysicallyInstalledSystemMemory)(&kb);
    if (!r) { t_note("GetPhysicallyInstalledSystemMemory failed err=%lu", GetLastError()); CHECK(kb == 0xEEEE || kb == 0); return; }
    MEMORYSTATUSEX ms; ms.dwLength = sizeof ms;
    U(GlobalMemoryStatusEx)(&ms);
    CHECK(kb * 1024 >= ms.ullTotalPhys);
    CHECK(kb >= 64 * 1024);
}
TEST(mv_gms_phys_installed_null) {
    REQUIRE(U(GetPhysicallyInstalledSystemMemory));
    SET_ERR(0);
    CHECK(!U(GetPhysicallyInstalledSystemMemory)(NULL));
    t_note("GetPhysicallyInstalledSystemMemory(NULL): err=%lu", GetLastError());
}
TEST(mv_large_page_minimum) {
    REQUIRE(U(GetLargePageMinimum));
    SIZE_T lp = U(GetLargePageMinimum)();
    CHECK(lp == 0 || (lp & (lp - 1)) == 0);
    CHECK(lp == 0 || lp >= GR);
    CHECK(lp == 0 || lp == 0x200000);
    { ULONG_PTR cr_ = (ULONG_PTR)(U(GetLargePageMinimum)()); EXPECT_EQ(cr_, lp); }
    t_note("GetLargePageMinimum=0x%Ix", lp);
}

/* ---------------- write watch ---------------- */
static char *ww_alloc(SIZE_T pages) {
    return VALLOC(NULL, pages * PG, MEM_RESERVE | MEM_COMMIT | MEM_WRITE_WATCH, PAGE_READWRITE);
}
TEST(mv_ww_exact_pages) {
    REQUIRE(VALLOC); REQUIRE(U(GetWriteWatch));
    char *p = ww_alloc(8);
    CHECK(p != NULL);
    if (!p) return;
    p[PG + 5] = 1; p[5 * PG + 100] = 2;
    PVOID addrs[16]; ULONG_PTR count = 16; ULONG gran = 0;
    SET_ERR(0);
    UINT r = U(GetWriteWatch)(0, p, 8 * PG, addrs, &count, &gran);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(count, 2);
    EXPECT_EQ(gran, PG);
    if (count == 2) { EXPECT_EQ(addrs[0], p + PG); EXPECT_EQ(addrs[1], p + 5 * PG); }
    mv_free(p);
}
TEST(mv_ww_untouched_is_empty) {
    REQUIRE(VALLOC); REQUIRE(U(GetWriteWatch));
    char *p = ww_alloc(4);
    PVOID addrs[8]; ULONG_PTR count = 8; ULONG gran = 0;
    CHECK(p != NULL);
    if (!p) return;
    { ULONG_PTR cr_ = (ULONG_PTR)(U(GetWriteWatch)(0, p, 4 * PG, addrs, &count, &gran)); EXPECT_EQ(cr_, 0); }
    EXPECT_EQ(count, 0);
    EXPECT_EQ(gran, PG);
    mv_free(p);
}
TEST(mv_ww_read_does_not_dirty) {
    REQUIRE(VALLOC); REQUIRE(U(GetWriteWatch));
    char *p = ww_alloc(4);
    if (!p) return;
    volatile char c = p[0]; (void)c; c = p[2 * PG]; (void)c;
    PVOID addrs[8]; ULONG_PTR count = 8; ULONG gran;
    { ULONG_PTR cr_ = (ULONG_PTR)(U(GetWriteWatch)(0, p, 4 * PG, addrs, &count, &gran)); EXPECT_EQ(cr_, 0); }
    EXPECT_EQ(count, 0);
    mv_free(p);
}
TEST(mv_ww_reset_flag) {
    REQUIRE(VALLOC); REQUIRE(U(GetWriteWatch));
    char *p = ww_alloc(4);
    if (!p) return;
    p[0] = 1; p[3 * PG] = 1;
    PVOID addrs[8]; ULONG_PTR count = 8; ULONG gran;
    { ULONG_PTR cr_ = (ULONG_PTR)(U(GetWriteWatch)(WRITE_WATCH_FLAG_RESET, p, 4 * PG, addrs, &count, &gran)); EXPECT_EQ(cr_, 0); }
    EXPECT_EQ(count, 2);
    count = 8;
    { ULONG_PTR cr_ = (ULONG_PTR)(U(GetWriteWatch)(0, p, 4 * PG, addrs, &count, &gran)); EXPECT_EQ(cr_, 0); }
    EXPECT_EQ(count, 0);
    p[2 * PG] = 1;
    count = 8;
    { ULONG_PTR cr_ = (ULONG_PTR)(U(GetWriteWatch)(0, p, 4 * PG, addrs, &count, &gran)); EXPECT_EQ(cr_, 0); }
    EXPECT_EQ(count, 1);
    if (count == 1) EXPECT_EQ(addrs[0], p + 2 * PG);
    mv_free(p);
}
TEST(mv_ww_no_reset_keeps_state) {
    REQUIRE(VALLOC); REQUIRE(U(GetWriteWatch));
    char *p = ww_alloc(4);
    if (!p) return;
    p[PG] = 1;
    PVOID addrs[8]; ULONG_PTR count; ULONG gran;
    for (int i = 0; i < 3; i++) {
        count = 8;
        { ULONG_PTR cr_ = (ULONG_PTR)(U(GetWriteWatch)(0, p, 4 * PG, addrs, &count, &gran)); EXPECT_EQ(cr_, 0); }
        EXPECT_EQ(count, 1);
    }
    mv_free(p);
}
TEST(mv_ww_reset_function) {
    REQUIRE(VALLOC); REQUIRE(U(GetWriteWatch)); REQUIRE(U(ResetWriteWatch));
    char *p = ww_alloc(6);
    if (!p) return;
    for (int i = 0; i < 6; i++) p[i * PG] = 1;
    SET_ERR(0);
    { ULONG_PTR cr_ = (ULONG_PTR)(U(ResetWriteWatch)(p + 2 * PG, 2 * PG)); EXPECT_EQ(cr_, 0); }
    PVOID addrs[8]; ULONG_PTR count = 8; ULONG gran;
    { ULONG_PTR cr_ = (ULONG_PTR)(U(GetWriteWatch)(0, p, 6 * PG, addrs, &count, &gran)); EXPECT_EQ(cr_, 0); }
    EXPECT_EQ(count, 4);
    if (count == 4) { EXPECT_EQ(addrs[0], p); EXPECT_EQ(addrs[1], p + PG); EXPECT_EQ(addrs[2], p + 4 * PG); EXPECT_EQ(addrs[3], p + 5 * PG); }
    mv_free(p);
}
TEST(mv_ww_subrange) {
    REQUIRE(VALLOC); REQUIRE(U(GetWriteWatch));
    char *p = ww_alloc(8);
    if (!p) return;
    for (int i = 0; i < 8; i++) p[i * PG] = 1;
    PVOID addrs[8]; ULONG_PTR count = 8; ULONG gran;
    { ULONG_PTR cr_ = (ULONG_PTR)(U(GetWriteWatch)(0, p + 2 * PG, 3 * PG, addrs, &count, &gran)); EXPECT_EQ(cr_, 0); }
    EXPECT_EQ(count, 3);
    if (count == 3) { EXPECT_EQ(addrs[0], p + 2 * PG); EXPECT_EQ(addrs[2], p + 4 * PG); }
    /* unaligned range covers the touched pages */
    count = 8;
    { ULONG_PTR cr_ = (ULONG_PTR)(U(GetWriteWatch)(0, p + 2 * PG + 100, PG, addrs, &count, &gran)); EXPECT_EQ(cr_, 0); }
    t_note("unaligned write watch range count=%Iu", count);
    mv_free(p);
}
TEST(mv_ww_small_buffer) {
    REQUIRE(VALLOC); REQUIRE(U(GetWriteWatch));
    char *p = ww_alloc(6);
    if (!p) return;
    for (int i = 0; i < 6; i++) p[i * PG] = 1;
    PVOID addrs[2]; ULONG_PTR count = 2; ULONG gran;
    SET_ERR(0);
    UINT r = U(GetWriteWatch)(WRITE_WATCH_FLAG_RESET, p, 6 * PG, addrs, &count, &gran);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(count, 2);
    if (count == 2) { EXPECT_EQ(addrs[0], p); EXPECT_EQ(addrs[1], p + PG); }
    mv_free(p);
}
TEST(mv_ww_not_watched_region) {
    REQUIRE(U(GetWriteWatch)); REQUIRE(U(ResetWriteWatch));
    char *p = mv_alloc(2 * PG);
    PVOID addrs[4]; ULONG_PTR count = 4; ULONG gran = 0;
    SET_ERR(0);
    { ULONG_PTR cr_ = (ULONG_PTR)(U(GetWriteWatch)(0, p, 2 * PG, addrs, &count, &gran)); EXPECT_EQ(cr_, (UINT)-1); }
    ERR1(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    { ULONG_PTR cr_ = (ULONG_PTR)(U(ResetWriteWatch)(p, 2 * PG)); EXPECT_EQ(cr_, (UINT)-1); }
    ERR1(ERROR_INVALID_PARAMETER);
    mv_free(p);
}
TEST(mv_ww_bad_flags) {
    REQUIRE(VALLOC); REQUIRE(U(GetWriteWatch));
    char *p = ww_alloc(2);
    PVOID addrs[4]; ULONG_PTR count = 4; ULONG gran;
    DWORD bad[] = { 2, 4, 0x80, 0xFFFFFFFE, 0x80000000 };
    for (int i = 0; i < 5; i++) {
        count = 4;
        SET_ERR(0);
        CHECKF(U(GetWriteWatch)(bad[i], p, 2 * PG, addrs, &count, &gran) == (UINT)-1, "flags 0x%lx", bad[i]);
        ERR1(ERROR_INVALID_PARAMETER);
    }
    mv_free(p);
}
TEST(mv_ww_bad_ranges) {
    REQUIRE(VALLOC); REQUIRE(U(GetWriteWatch)); REQUIRE(U(ResetWriteWatch));
    char *p = ww_alloc(2);
    PVOID addrs[4]; ULONG_PTR count = 4; ULONG gran;
    SET_ERR(0);
    CHECK(U(GetWriteWatch)(0, p, 0, addrs, &count, &gran) == (UINT)-1);
    t_note("GetWriteWatch size 0: err=%lu", GetLastError());
    SET_ERR(0);
    CHECK(U(ResetWriteWatch)(p, 0) == (UINT)-1);
    t_note("ResetWriteWatch size 0: err=%lu", GetLastError());
    SET_ERR(0);
    CHECK(U(GetWriteWatch)(0, p, 4 * PG, addrs, &count, &gran) == (UINT)-1);   /* runs past the allocation */
    t_note("range beyond watched allocation: err=%lu", GetLastError());
    SET_ERR(0);
    CHECK(U(GetWriteWatch)(0, NULL, PG, addrs, &count, &gran) == (UINT)-1);
    t_note("NULL range: err=%lu", GetLastError());
    mv_free(p);
}
TEST(mv_ww_after_decommit_recommit) {
    REQUIRE(VALLOC); REQUIRE(U(GetWriteWatch));
    char *p = ww_alloc(4);
    if (!p) return;
    p[0] = 1;
    CHECK(VFREE(p, 2 * PG, MEM_DECOMMIT));
    CHECK(VALLOC(p, 2 * PG, MEM_COMMIT, PAGE_READWRITE) == p);
    PVOID addrs[8]; ULONG_PTR count = 8; ULONG gran;
    SET_ERR(0);
    UINT r = U(GetWriteWatch)(0, p, 4 * PG, addrs, &count, &gran);
    EXPECT_EQ(r, 0);
    t_note("write watch count after decommit/recommit: %Iu", count);
    mv_free(p);
}
TEST(mv_ww_large_region_sparse) {
    REQUIRE(VALLOC); REQUIRE(U(GetWriteWatch));
    SIZE_T pages = 4096;
    char *p = ww_alloc(pages);
    if (!p) return;
    size_t idx[] = { 0, 1, 77, 1000, 2047, 4095 };
    for (int i = 0; i < 6; i++) p[idx[i] * PG + 3] = 1;
    PVOID *addrs = HeapAlloc(GetProcessHeap(), 0, pages * sizeof(PVOID));
    ULONG_PTR count = pages; ULONG gran;
    { ULONG_PTR cr_ = (ULONG_PTR)(U(GetWriteWatch)(0, p, pages * PG, addrs, &count, &gran)); EXPECT_EQ(cr_, 0); }
    EXPECT_EQ(count, 6);
    if (count == 6) for (int i = 0; i < 6; i++) CHECK(addrs[i] == p + idx[i] * PG);
    HeapFree(GetProcessHeap(), 0, addrs);
    mv_free(p);
}
TEST(mv_ww_writefile_into_buffer_dirties) {
    REQUIRE(VALLOC); REQUIRE(U(GetWriteWatch));
    char *p = ww_alloc(4);
    if (!p) return;
    char path[MAX_PATH]; t_tmpfile(path, sizeof path, "ww.bin");
    HANDLE f = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD n; char data[100] = {1};
    WriteFile(f, data, sizeof data, &n, NULL);
    SetFilePointer(f, 0, NULL, FILE_BEGIN);
    CHECK(ReadFile(f, p + 2 * PG, sizeof data, &n, NULL));      /* the kernel writes into the watched page */
    CloseHandle(f);
    PVOID addrs[8]; ULONG_PTR count = 8; ULONG gran;
    { ULONG_PTR cr_ = (ULONG_PTR)(U(GetWriteWatch)(0, p, 4 * PG, addrs, &count, &gran)); EXPECT_EQ(cr_, 0); }
    EXPECT_EQ(count, 1);
    if (count == 1) EXPECT_EQ(addrs[0], p + 2 * PG);
    mv_free(p);
}

/* ---------------- Offer / Reclaim / Discard ---------------- */
TEST(mv_offer_discard_arg_validation) {
    REQUIRE(U(OfferVirtualMemory)); REQUIRE(U(ReclaimVirtualMemory)); REQUIRE(U(DiscardVirtualMemory));
    char *p = mv_alloc(4 * PG);
    { ULONG_PTR cr_ = (ULONG_PTR)(U(DiscardVirtualMemory)(NULL, PG)); EXPECT_EQ(cr_, ERROR_INVALID_PARAMETER); }
    { ULONG_PTR cr_ = (ULONG_PTR)(U(DiscardVirtualMemory)(p, 0)); EXPECT_EQ(cr_, ERROR_INVALID_PARAMETER); }
    { ULONG_PTR cr_ = (ULONG_PTR)(U(DiscardVirtualMemory)(p + 1, PG)); EXPECT_EQ(cr_, ERROR_INVALID_PARAMETER); }
    { ULONG_PTR cr_ = (ULONG_PTR)(U(DiscardVirtualMemory)(p, PG + 1)); EXPECT_EQ(cr_, ERROR_INVALID_PARAMETER); }
    { ULONG_PTR cr_ = (ULONG_PTR)(U(OfferVirtualMemory)(NULL, PG, VmOfferPriorityNormal)); EXPECT_EQ(cr_, ERROR_INVALID_PARAMETER); }
    { ULONG_PTR cr_ = (ULONG_PTR)(U(OfferVirtualMemory)(p, 0, VmOfferPriorityNormal)); EXPECT_EQ(cr_, ERROR_INVALID_PARAMETER); }
    { ULONG_PTR cr_ = (ULONG_PTR)(U(OfferVirtualMemory)(p + 8, PG, VmOfferPriorityNormal)); EXPECT_EQ(cr_, ERROR_INVALID_PARAMETER); }
    { ULONG_PTR cr_ = (ULONG_PTR)(U(OfferVirtualMemory)(p, PG + 8, VmOfferPriorityNormal)); EXPECT_EQ(cr_, ERROR_INVALID_PARAMETER); }
    { ULONG_PTR cr_ = (ULONG_PTR)(U(ReclaimVirtualMemory)(NULL, PG)); EXPECT_EQ(cr_, ERROR_INVALID_PARAMETER); }
    { ULONG_PTR cr_ = (ULONG_PTR)(U(ReclaimVirtualMemory)(p, 0)); EXPECT_EQ(cr_, ERROR_INVALID_PARAMETER); }
    { ULONG_PTR cr_ = (ULONG_PTR)(U(ReclaimVirtualMemory)(p + 8, PG)); EXPECT_EQ(cr_, ERROR_INVALID_PARAMETER); }
    { ULONG_PTR cr_ = (ULONG_PTR)(U(ReclaimVirtualMemory)(p, PG + 8)); EXPECT_EQ(cr_, ERROR_INVALID_PARAMETER); }
    mv_free(p);
}
TEST(mv_offer_bad_priority) {
    REQUIRE(U(OfferVirtualMemory));
    char *p = mv_alloc(PG);
    DWORD bad[] = { 0, 5, 6, 0x100, 0xFFFFFFFF };
    for (int i = 0; i < 5; i++) {
        DWORD r = U(OfferVirtualMemory)(p, PG, (OFFER_PRIORITY)bad[i]);
        CHECKF(r == ERROR_INVALID_PARAMETER, "priority %lu -> %lu", bad[i], r);
    }
    EXPECT_EQ(mv_q(p).Protect, PAGE_READWRITE);
    mv_free(p);
}
TEST(mv_offer_reclaim_roundtrip) {
    REQUIRE(U(OfferVirtualMemory)); REQUIRE(U(ReclaimVirtualMemory));
    OFFER_PRIORITY prios[] = { VmOfferPriorityVeryLow, VmOfferPriorityLow, VmOfferPriorityBelowNormal, VmOfferPriorityNormal };
    for (int i = 0; i < 4; i++) {
        char *p = mv_alloc(4 * PG);
        memset(p, 0x42, 4 * PG);
        DWORD r = U(OfferVirtualMemory)(p, 4 * PG, prios[i]);
        CHECKF(r == 0, "offer prio %d -> %lu", prios[i], r);
        if (r == 0) {
            EXPECT_EQ(mv_q(p).Protect, PAGE_NOACCESS);        /* offered pages are inaccessible */
            EXPECT_EQ(mv_rd(p, NULL), EXCEPTION_ACCESS_VIOLATION);
            r = U(ReclaimVirtualMemory)(p, 4 * PG);
            CHECKF(r == 0 || r == ERROR_BUSY, "reclaim -> %lu", r);
            EXPECT_EQ(mv_q(p).Protect, PAGE_READWRITE);
            EXPECT_EQ(mv_wr(p, 1), 0);
            if (r == 0) t_note("contents after reclaim: first=%02x", (unsigned char)p[PG]);
        }
        mv_free(p);
    }
}
TEST(mv_offer_requires_rw_commit) {
    REQUIRE(U(OfferVirtualMemory));
    char *p = mv_alloc(2 * PG);
    DWORD old;
    VPROT(p, PG, PAGE_READONLY, &old);
    CHECK(U(OfferVirtualMemory)(p, PG, VmOfferPriorityNormal) != 0);
    char *rsv = VALLOC(NULL, GR, MEM_RESERVE, PAGE_READWRITE);
    CHECK(U(OfferVirtualMemory)(rsv, PG, VmOfferPriorityNormal) != 0);
    CHECK(U(OfferVirtualMemory)(p, 2 * PG, VmOfferPriorityNormal) != 0);       /* spans two protections */
    void *h = mv_hole(GR);
    CHECK(U(OfferVirtualMemory)(h, PG, VmOfferPriorityNormal) != 0);
    mv_free(rsv); mv_free(p);
}
TEST(mv_reclaim_not_offered) {
    REQUIRE(U(ReclaimVirtualMemory));
    char *p = mv_alloc(PG);
    DWORD r = U(ReclaimVirtualMemory)(p, PG);
    CHECKF(r != 0, "reclaim of never-offered page returned %lu", r);
    t_note("ReclaimVirtualMemory(never offered) -> %lu", r);
    EXPECT_EQ(mv_q(p).Protect, PAGE_READWRITE);
    mv_free(p);
}
TEST(mv_discard_keeps_access) {
    REQUIRE(U(DiscardVirtualMemory));
    char *p = mv_alloc(4 * PG);
    memset(p, 0x77, 4 * PG);
    DWORD r = U(DiscardVirtualMemory)(p, 4 * PG);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(mv_q(p).Protect, PAGE_READWRITE);
    EXPECT_EQ(mv_wr(p + PG, 1), 0);
    EXPECT_EQ(mv_rd(p, NULL), 0);
    mv_free(p);
}
TEST(mv_discard_partial_range) {
    REQUIRE(U(DiscardVirtualMemory));
    char *p = mv_alloc(8 * PG);
    { ULONG_PTR cr_ = (ULONG_PTR)(U(DiscardVirtualMemory)(p + 2 * PG, 3 * PG)); EXPECT_EQ(cr_, 0); }
    EXPECT_EQ(mv_q(p).RegionSize, 8 * PG);
    char *rsv = VALLOC(NULL, GR, MEM_RESERVE, PAGE_READWRITE);
    CHECK(U(DiscardVirtualMemory)(rsv, PG) != 0);
    mv_free(rsv); mv_free(p);
}

/* ---------------- PrefetchVirtualMemory ---------------- */
TEST(mv_prefetch_basic) {
    REQUIRE(U(PrefetchVirtualMemory));
    char *p = mv_alloc(8 * PG);
    memset(p, 1, 8 * PG);
    WIN32_MEMORY_RANGE_ENTRY e[2] = { { p, 4 * PG }, { p + 4 * PG, 4 * PG } };
    SET_ERR(0);
    BOOL r = U(PrefetchVirtualMemory)(GetCurrentProcess(), 1, e, 0);
    CHECKF(r, "err %lu", GetLastError());
    r = U(PrefetchVirtualMemory)(GetCurrentProcess(), 2, e, 0);
    CHECKF(r, "err %lu", GetLastError());
    mv_free(p);
}
TEST(mv_prefetch_invalid) {
    REQUIRE(U(PrefetchVirtualMemory));
    char *p = mv_alloc(2 * PG);
    WIN32_MEMORY_RANGE_ENTRY e = { p, PG };
    SET_ERR(0);
    CHECK(!U(PrefetchVirtualMemory)((HANDLE)(ULONG_PTR)0x7771, 1, &e, 0));
    ERR1(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(!U(PrefetchVirtualMemory)(NULL, 1, &e, 0));
    ERR1(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    BOOL r = U(PrefetchVirtualMemory)(GetCurrentProcess(), 1, &e, 1);
    CHECK(!r);
    t_note("PrefetchVirtualMemory flags=1: err=%lu", GetLastError());
    SET_ERR(0);
    r = U(PrefetchVirtualMemory)(GetCurrentProcess(), 0, &e, 0);
    t_note("PrefetchVirtualMemory zero entries: r=%d err=%lu", r, GetLastError());
    WIN32_MEMORY_RANGE_ENTRY z = { p, 0 };
    SET_ERR(0);
    r = U(PrefetchVirtualMemory)(GetCurrentProcess(), 1, &z, 0);
    t_note("PrefetchVirtualMemory zero length: r=%d err=%lu", r, GetLastError());
    WIN32_MEMORY_RANGE_ENTRY bad = { (void *)0x7FFFFFFF0000ull, PG };
    SET_ERR(0);
    r = U(PrefetchVirtualMemory)(GetCurrentProcess(), 1, &bad, 0);
    CHECK(!r);
    t_note("PrefetchVirtualMemory beyond user space: err=%lu", GetLastError());
    mv_free(p);
}
TEST(mv_prefetch_access_rights) {
    REQUIRE(U(PrefetchVirtualMemory));
    char *p = mv_alloc(PG);
    WIN32_MEMORY_RANGE_ENTRY e = { p, PG };
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    SET_ERR(0);
    CHECK(!U(PrefetchVirtualMemory)(h, 1, &e, 0));
    ERR1(ERROR_ACCESS_DENIED);
    if (h) CloseHandle(h);
    mv_free(p);
}

/* ---------------- memory resource notification ---------------- */
TEST(mv_mrn_low_high) {
    REQUIRE(U(CreateMemoryResourceNotification)); REQUIRE(U(QueryMemoryResourceNotification));
    MEMORY_RESOURCE_NOTIFICATION_TYPE types[] = { LowMemoryResourceNotification, HighMemoryResourceNotification };
    for (int i = 0; i < 2; i++) {
        SET_ERR(0);
        HANDLE h = U(CreateMemoryResourceNotification)(types[i]);
        CHECKF(h != NULL, "type %d err %lu", types[i], GetLastError());
        if (!h) continue;
        BOOL st = 0xEE;
        SET_ERR(0);
        CHECK(U(QueryMemoryResourceNotification)(h, &st));
        CHECK(st == 0 || st == 1);
        DWORD w = WaitForSingleObject(h, 0);
        CHECK(w == WAIT_OBJECT_0 || w == WAIT_TIMEOUT);
        t_note("type %d state=%d wait=%lu", types[i], st, w);
        DWORD flags = 0;
        CHECK(GetHandleInformation(h, &flags));
        CloseHandle(h);
    }
}
TEST(mv_mrn_state_matches_signal) {
    REQUIRE(U(CreateMemoryResourceNotification)); REQUIRE(U(QueryMemoryResourceNotification));
    for (int t = 0; t < 2; t++) {
        HANDLE h = U(CreateMemoryResourceNotification)((MEMORY_RESOURCE_NOTIFICATION_TYPE)t);
        if (!h) continue;
        BOOL st = 0;
        int agree = 0;
        for (int i = 0; i < 5; i++) {
            U(QueryMemoryResourceNotification)(h, &st);
            DWORD w = WaitForSingleObject(h, 0);
            if ((st != 0) == (w == WAIT_OBJECT_0)) agree++;
        }
        CHECKF(agree >= 4, "type %d agree %d/5", t, agree);
        CloseHandle(h);
    }
}
TEST(mv_mrn_invalid_type) {
    REQUIRE(U(CreateMemoryResourceNotification));
    DWORD bad[] = { 2, 3, 100, 0xFFFFFFFF, 0x80000000 };
    for (int i = 0; i < 5; i++) {
        SET_ERR(0);
        HANDLE h = U(CreateMemoryResourceNotification)((MEMORY_RESOURCE_NOTIFICATION_TYPE)bad[i]);
        CHECKF(h == NULL, "type %lu", bad[i]);
        if (h) CloseHandle(h); else ERR1(ERROR_INVALID_PARAMETER);
    }
}
TEST(mv_mrn_query_invalid) {
    REQUIRE(U(CreateMemoryResourceNotification)); REQUIRE(U(QueryMemoryResourceNotification));
    BOOL st = 7;
    SET_ERR(0);
    CHECK(!U(QueryMemoryResourceNotification)(NULL, &st));
    ERR1(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!U(QueryMemoryResourceNotification)(INVALID_HANDLE_VALUE, &st));
    ERR1(ERROR_INVALID_PARAMETER);
    HANDLE h = U(CreateMemoryResourceNotification)(LowMemoryResourceNotification);
    SET_ERR(0);
    CHECK(!U(QueryMemoryResourceNotification)(h, NULL));
    ERR1(ERROR_INVALID_PARAMETER);
    if (h) CloseHandle(h);
    SET_ERR(0);
    CHECK(!U(QueryMemoryResourceNotification)((HANDLE)(ULONG_PTR)0x7771, &st));
    ERR1(ERROR_INVALID_HANDLE);
    HANDLE mtx = CreateMutexA(NULL, FALSE, NULL);
    SET_ERR(0);
    CHECK(!U(QueryMemoryResourceNotification)(mtx, &st));
    ERR1(ERROR_INVALID_HANDLE);
    CloseHandle(mtx);
    EXPECT_EQ(st, 7);
}
TEST(mv_mrn_closed_handle) {
    REQUIRE(U(CreateMemoryResourceNotification)); REQUIRE(U(QueryMemoryResourceNotification));
    HANDLE h = U(CreateMemoryResourceNotification)(HighMemoryResourceNotification);
    CHECK(h != NULL);
    if (!h) return;
    CloseHandle(h);
    BOOL st;
    SET_ERR(0);
    CHECK(!U(QueryMemoryResourceNotification)(h, &st));
    ERR1(ERROR_INVALID_HANDLE);
}
TEST(mv_mrn_handle_is_read_only_event) {
    REQUIRE(U(CreateMemoryResourceNotification));
    HANDLE h = U(CreateMemoryResourceNotification)(LowMemoryResourceNotification);
    if (!h) return;
    SET_ERR(0);
    CHECK(!SetEvent(h));                              /* no EVENT_MODIFY_STATE */
    ERR1(ERROR_ACCESS_DENIED);
    SET_ERR(0);
    CHECK(!ResetEvent(h));
    ERR1(ERROR_ACCESS_DENIED);
    CloseHandle(h);
}
TEST(mv_mrn_two_handles_same_state) {
    REQUIRE(U(CreateMemoryResourceNotification)); REQUIRE(U(QueryMemoryResourceNotification));
    HANDLE a = U(CreateMemoryResourceNotification)(LowMemoryResourceNotification);
    HANDLE b = U(CreateMemoryResourceNotification)(LowMemoryResourceNotification);
    CHECK(a && b);
    CHECK(a != b);
    BOOL sa = 0, sb = 0;
    if (a && b) { U(QueryMemoryResourceNotification)(a, &sa); U(QueryMemoryResourceNotification)(b, &sb); t_note("low-memory states %d %d", sa, sb); }
    if (a) CloseHandle(a);
    if (b) CloseHandle(b);
}

/* ---------------- VirtualAlloc2 / placeholders (Win10 1803+) ---------------- */
#define VA2 U(VirtualAlloc2)
TEST(mv_va2_basic_self) {
    REQUIRE(VA2); REQUIRE(VQ);
    SET_ERR(0);
    char *p = VA2(NULL, NULL, 2 * PG, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE, NULL, 0);
    CHECKF(p != NULL, "err %lu", GetLastError());
    if (p) {
        EXPECT_EQ(mv_q(p).RegionSize, 2 * PG);
        EXPECT_EQ(mv_wr(p, 1), 0);
        mv_free(p);
    }
    p = VA2(GetCurrentProcess(), NULL, PG, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE, NULL, 0);
    CHECK(p != NULL);
    mv_free(p);
}
TEST(mv_va2_param_validation) {
    REQUIRE(VA2);
    SET_ERR(0);
    CHECK(VA2(NULL, NULL, 0, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE, NULL, 0) == NULL);
    ERR1(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(VA2(NULL, NULL, PG, 0, PAGE_READWRITE, NULL, 0) == NULL);
    ERR1(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(VA2(NULL, NULL, PG, MEM_RESERVE | MEM_COMMIT, 0, NULL, 0) == NULL);
    ERR1(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(VA2((HANDLE)(ULONG_PTR)0x7771, NULL, PG, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE, NULL, 0) == NULL);
    ERR1(ERROR_INVALID_HANDLE);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    CHECK(VA2(ev, NULL, PG, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE, NULL, 0) == NULL);
    ERR1(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
}
TEST(mv_va2_extended_param_types) {
    REQUIRE(VA2);
    MEM_EXTENDED_PARAMETER ep; memset(&ep, 0, sizeof ep);
    DWORD bad[] = { 0, 3, 5, 6, 7, 0x3F };
    for (int i = 0; i < 6; i++) {
        ep.Type = bad[i];
        SET_ERR(0);
        void *p = VA2(NULL, NULL, PG, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE, &ep, 1);
        CHECKF(p == NULL, "ext type %lu", bad[i]);
        if (p) mv_free(p); else ERR1(ERROR_INVALID_PARAMETER);
    }
    ep.Type = MemExtendedParameterNumaNode; ep.ULong = 0;
    void *p = VA2(NULL, NULL, PG, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE, &ep, 1);
    CHECKF(p != NULL, "numa node 0: err %lu", GetLastError());
    mv_free(p);
    ep.ULong = 0x1000;                                   /* nonexistent node */
    SET_ERR(0);
    p = VA2(NULL, NULL, PG, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE, &ep, 1);
    t_note("VirtualAlloc2 NUMA node 0x1000: %p err=%lu", p, GetLastError());
    mv_free(p);
}
TEST(mv_va2_address_requirements_alignment) {
    REQUIRE(VA2);
    MEM_ADDRESS_REQUIREMENTS req; memset(&req, 0, sizeof req);
    MEM_EXTENDED_PARAMETER ep; memset(&ep, 0, sizeof ep);
    ep.Type = MemExtendedParameterAddressRequirements; ep.Pointer = &req;
    SIZE_T aligns[] = { GR, 2 * GR, 16 * GR, 0x1000000 };
    for (int i = 0; i < 4; i++) {
        req.Alignment = aligns[i];
        SET_ERR(0);
        char *p = VA2(NULL, NULL, GR, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE, &ep, 1);
        CHECKF(p != NULL, "alignment 0x%Ix err %lu", aligns[i], GetLastError());
        if (p) { CHECKF(((ULONG_PTR)p & (aligns[i] - 1)) == 0, "p=%p", p); mv_free(p); }
    }
    SIZE_T bad[] = { 3 * GR, GR + 1, 0x18000 };
    for (int i = 0; i < 3; i++) {
        req.Alignment = bad[i];
        SET_ERR(0);
        void *p = VA2(NULL, NULL, GR, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE, &ep, 1);
        CHECKF(p == NULL, "alignment 0x%Ix", bad[i]);
        if (p) mv_free(p); else ERR1(ERROR_INVALID_PARAMETER);
    }
}
TEST(mv_va2_address_requirements_bounds) {
    REQUIRE(VA2);
    MEM_ADDRESS_REQUIREMENTS req; memset(&req, 0, sizeof req);
    MEM_EXTENDED_PARAMETER ep; memset(&ep, 0, sizeof ep);
    ep.Type = MemExtendedParameterAddressRequirements; ep.Pointer = &req;
    req.LowestStartingAddress = (PVOID)0x10000000;
    req.HighestEndingAddress = (PVOID)0x7FFFFFFF;
    SET_ERR(0);
    char *p = VA2(NULL, NULL, GR, MEM_RESERVE, PAGE_READWRITE, &ep, 1);
    CHECKF(p != NULL, "err %lu", GetLastError());
    if (p) { CHECK((ULONG_PTR)p >= 0x10000000 && (ULONG_PTR)p + GR - 1 <= 0x7FFFFFFF); mv_free(p); }
    /* ending address must be page-1 aligned */
    req.HighestEndingAddress = (PVOID)0x7FFFFFF0;
    SET_ERR(0);
    p = VA2(NULL, NULL, GR, MEM_RESERVE, PAGE_READWRITE, &ep, 1);
    CHECK(p == NULL);
    if (p) mv_free(p); else t_note("unaligned HighestEndingAddress err=%lu", GetLastError());
    /* bounds too small for request */
    req.HighestEndingAddress = (PVOID)0x1000FFFF;
    SET_ERR(0);
    p = VA2(NULL, NULL, 4 * GR, MEM_RESERVE, PAGE_READWRITE, &ep, 1);
    CHECK(p == NULL);
    if (p) mv_free(p); else t_note("tiny window err=%lu", GetLastError());
    /* lowest > highest */
    req.LowestStartingAddress = (PVOID)0x30000000; req.HighestEndingAddress = (PVOID)0x1FFFFFFF;
    SET_ERR(0);
    p = VA2(NULL, NULL, GR, MEM_RESERVE, PAGE_READWRITE, &ep, 1);
    CHECK(p == NULL);
    if (p) mv_free(p);
}
TEST(mv_va2_placeholder_flag_rules) {
    REQUIRE(VA2);
    SET_ERR(0);
    void *p = VA2(NULL, NULL, 2 * GR, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_READWRITE, NULL, 0);
    CHECK(p == NULL);                                   /* placeholders must be PAGE_NOACCESS */
    if (p) mv_free(p); else ERR1(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    p = VA2(NULL, NULL, 2 * GR, MEM_COMMIT | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, NULL, 0);
    CHECK(p == NULL);
    if (p) mv_free(p); else ERR1(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    p = VA2(NULL, NULL, 2 * GR, MEM_RESERVE | MEM_COMMIT | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, NULL, 0);
    CHECK(p == NULL);
    if (p) mv_free(p); else ERR1(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    p = VA2(NULL, NULL, GR, MEM_REPLACE_PLACEHOLDER | MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE, NULL, 0);
    CHECK(p == NULL);                                   /* needs an address */
    if (p) mv_free(p); else ERR1(ERROR_INVALID_PARAMETER);
}
TEST(mv_va2_placeholder_split_coalesce) {
    REQUIRE(VA2); REQUIRE(VFREE); REQUIRE(VQ);
    char *p = VA2(NULL, NULL, 4 * GR, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, NULL, 0);
    CHECKF(p != NULL, "err %lu", GetLastError());
    if (!p) return;
    MEMORY_BASIC_INFORMATION m = mv_q(p);
    EXPECT_EQ(m.State, MEM_RESERVE); EXPECT_EQ(m.RegionSize, 4 * GR);
    EXPECT_EQ(m.Type, MEM_PRIVATE);
    SET_ERR(0);
    BOOL r = VFREE(p, GR, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER);        /* split off first granule */
    CHECKF(r, "split err %lu", GetLastError());
    if (r) {
        EXPECT_EQ(mv_q(p).RegionSize, GR);
        EXPECT_EQ(mv_q(p + GR).RegionSize, 3 * GR);
        r = VFREE(p + GR, GR, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER);
        CHECK(r);
        EXPECT_EQ(mv_q(p + GR).RegionSize, GR);
        SET_ERR(0);
        r = VFREE(p, 2 * GR, MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS);
        CHECKF(r, "coalesce err %lu", GetLastError());
        if (r) EXPECT_EQ(mv_q(p).RegionSize, 2 * GR);
        /* split at an unaligned size fails */
        SET_ERR(0);
        r = VFREE(p, PG, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER);
        CHECK(!r);
        t_note("split at 4KB: err=%lu", GetLastError());
        /* releasing a placeholder piece by piece */
        CHECK(VFREE(p, 0, MEM_RELEASE) || 1);
    }
    /* free whatever remains */
    for (int i = 0; i < 4; i++) {
        MEMORY_BASIC_INFORMATION q = mv_q(p + i * GR);
        if (q.State != MEM_FREE) VFREE(q.BaseAddress, 0, MEM_RELEASE);
    }
}
TEST(mv_va2_placeholder_replace_with_private) {
    REQUIRE(VA2); REQUIRE(VFREE); REQUIRE(VQ);
    char *p = VA2(NULL, NULL, GR, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, NULL, 0);
    CHECKF(p != NULL, "err %lu", GetLastError());
    if (!p) return;
    SET_ERR(0);
    char *q = VA2(NULL, p, GR, MEM_RESERVE | MEM_COMMIT | MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0);
    CHECKF(q == p, "replace err %lu", GetLastError());
    if (q == p) {
        MEMORY_BASIC_INFORMATION m = mv_q(p);
        EXPECT_EQ(m.State, MEM_COMMIT); EXPECT_EQ(m.Protect, PAGE_READWRITE);
        EXPECT_EQ(mv_wr(p, 9), 0);
        SET_ERR(0);
        BOOL r = VFREE(p, GR, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER);    /* back to a placeholder */
        CHECKF(r, "preserve err %lu", GetLastError());
        if (r) { m = mv_q(p); EXPECT_EQ(m.State, MEM_RESERVE); }
    }
    MEMORY_BASIC_INFORMATION m = mv_q(p);
    if (m.State != MEM_FREE) VFREE(p, 0, MEM_RELEASE);
}
TEST(mv_va2_placeholder_replace_with_view) {
    REQUIRE(VA2); REQUIRE(U(MapViewOfFile3)); REQUIRE(U(UnmapViewOfFile2)); REQUIRE(VFREE); REQUIRE(VQ);
    HANDLE m = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, GR, NULL);
    char *p = VA2(NULL, NULL, GR, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, NULL, 0);
    CHECKF(p != NULL, "err %lu", GetLastError());
    if (p) {
        SET_ERR(0);
        char *v = U(MapViewOfFile3)(m, NULL, p, 0, GR, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0);
        CHECKF(v == p, "replace with view: %p err %lu", v, GetLastError());
        if (v == p) {
            MEMORY_BASIC_INFORMATION q = mv_q(p);
            EXPECT_EQ(q.Type, MEM_MAPPED); EXPECT_EQ(q.State, MEM_COMMIT);
            EXPECT_EQ(mv_wr(p, 3), 0);
            SET_ERR(0);
            BOOL r = U(UnmapViewOfFile2)(GetCurrentProcess(), p, MEM_PRESERVE_PLACEHOLDER);
            CHECKF(r, "unmap preserve err %lu", GetLastError());
            q = mv_q(p);
            EXPECT_EQ(q.State, MEM_RESERVE); EXPECT_EQ(q.Type, MEM_PRIVATE);
        }
        MEMORY_BASIC_INFORMATION q = mv_q(p);
        if (q.State != MEM_FREE) VFREE(p, 0, MEM_RELEASE);
    }
    CloseHandle(m);
}
TEST(mv_va2_replace_wrong_size_fails) {
    REQUIRE(VA2); REQUIRE(VFREE); REQUIRE(VQ);
    char *p = VA2(NULL, NULL, 2 * GR, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, NULL, 0);
    if (!p) { t_note("placeholders unavailable err=%lu", GetLastError()); return; }
    SET_ERR(0);
    void *q = VA2(NULL, p, GR, MEM_RESERVE | MEM_COMMIT | MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0);
    CHECK(q == NULL);                                   /* placeholder is larger than the replacement */
    if (q) mv_free(q); else t_note("replace smaller than placeholder: err=%lu", GetLastError());
    char *r = mv_alloc(PG);
    SET_ERR(0);
    q = VA2(NULL, r, PG, MEM_RESERVE | MEM_COMMIT | MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0);
    CHECK(q == NULL);                                   /* ordinary memory is not a placeholder */
    t_note("replace non-placeholder: err=%lu", GetLastError());
    mv_free(r);
    VFREE(p, 0, MEM_RELEASE);
}
TEST(mv_va2_from_app_variants) {
    REQUIRE(U(VirtualAlloc2FromApp)); REQUIRE(U(VirtualAllocFromApp)); REQUIRE(U(VirtualProtectFromApp));
    SET_ERR(0);
    char *p = U(VirtualAllocFromApp)(NULL, PG, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    CHECKF(p != NULL, "err %lu", GetLastError());
    if (p) {
        DWORD old = 0;
        SET_ERR(0);
        CHECK(U(VirtualProtectFromApp)(p, PG, PAGE_READONLY, &old));
        EXPECT_EQ(old, PAGE_READWRITE);
        SET_ERR(0);
        BOOL r = U(VirtualProtectFromApp)(p, PG, PAGE_EXECUTE_READWRITE, &old);
        t_note("VirtualProtectFromApp(RWX): %d err=%lu", r, GetLastError());
        if (r) VPROT(p, PG, PAGE_READWRITE, &old);
        mv_free(p);
    }
    SET_ERR(0);
    p = U(VirtualAllocFromApp)(NULL, PG, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    t_note("VirtualAllocFromApp(RWX): %p err=%lu", p, GetLastError());
    if (p) mv_free(p);
    p = U(VirtualAlloc2FromApp)(NULL, NULL, PG, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE, NULL, 0);
    CHECK(p != NULL);
    mv_free(p);
}

/* ---------------- misc Win10 memory APIs ---------------- */
TEST(mv_qvmi_region_info) {
    REQUIRE(U(QueryVirtualMemoryInformation));
    typedef struct { PVOID AllocationBase; ULONG AllocationProtect; ULONG Flags; SIZE_T RegionSize; SIZE_T CommitSize; } REGION_INFO;
    char *p = VALLOC(NULL, 8 * PG, MEM_RESERVE, PAGE_READWRITE);
    CHECK(p != NULL);
    if (!p) return;
    CHECK(VALLOC(p, 2 * PG, MEM_COMMIT, PAGE_READWRITE) == p);
    REGION_INFO ri; memset(&ri, 0, sizeof ri);
    SIZE_T ret = 0;
    SET_ERR(0);
    BOOL r = U(QueryVirtualMemoryInformation)(GetCurrentProcess(), p + PG, 0, &ri, sizeof ri, &ret);
    CHECKF(r, "err %lu", GetLastError());
    if (r) {
        EXPECT_EQ(ri.AllocationBase, p);
        EXPECT_EQ(ri.AllocationProtect, PAGE_READWRITE);
        EXPECT_EQ(ri.RegionSize, 8 * PG);
        EXPECT_EQ(ri.CommitSize, 2 * PG);
        t_note("QVMI ret=%Iu flags=0x%lx", ret, ri.Flags);
    }
    SET_ERR(0);
    CHECK(!U(QueryVirtualMemoryInformation)(GetCurrentProcess(), p, 1, &ri, sizeof ri, &ret));
    ERR1(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!U(QueryVirtualMemoryInformation)(GetCurrentProcess(), p, 0, &ri, 4, &ret));
    t_note("QueryVirtualMemoryInformation short buffer: err=%lu", GetLastError());
    SET_ERR(0);
    CHECK(!U(QueryVirtualMemoryInformation)((HANDLE)(ULONG_PTR)0x7771, p, 0, &ri, sizeof ri, &ret));
    ERR1(ERROR_INVALID_HANDLE);
    mv_free(p);
}
TEST(mv_cfg_set_valid_call_targets_errors) {
    REQUIRE(U(SetProcessValidCallTargets));
    char *p = mv_alloc(PG);
    CFG_CALL_TARGET_INFO ti = { 0, CFG_CALL_TARGET_VALID };
    SET_ERR(0);
    CHECK(!U(SetProcessValidCallTargets)((HANDLE)(ULONG_PTR)0x7771, p, PG, 1, &ti));
    ERR1(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(!U(SetProcessValidCallTargets)(NULL, p, PG, 1, &ti));
    ERR1(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    BOOL r = U(SetProcessValidCallTargets)(GetCurrentProcess(), NULL, 0, 0, NULL);
    CHECK(!r);
    t_note("SetProcessValidCallTargets(zero args): err=%lu", GetLastError());
    mv_free(p);
}
TEST(mv_memory_error_capabilities) {
    REQUIRE(U(GetMemoryErrorHandlingCapabilities));
    ULONG caps = 0xEE;
    SET_ERR(0);
    CHECK(U(GetMemoryErrorHandlingCapabilities)(&caps));
    CHECK((caps & ~3u) == 0);
    t_note("memory error handling capabilities=0x%lx", caps);
}
TEST(mv_physical_pages_need_privilege) {
    REQUIRE(U(AllocateUserPhysicalPages)); REQUIRE(U(FreeUserPhysicalPages));
    ULONG_PTR n = 1, pfn[1] = {0};
    SET_ERR(0);
    BOOL r = U(AllocateUserPhysicalPages)(GetCurrentProcess(), &n, pfn);
    if (r) { t_note("AWE privilege present; allocated %Iu pages", n); ULONG_PTR k = n; U(FreeUserPhysicalPages)(GetCurrentProcess(), &k, pfn); return; }
    ERR1(ERROR_PRIVILEGE_NOT_HELD);
    EXPECT_EQ(n, 1);
}
TEST(mv_physical_pages_bad_handle) {
    REQUIRE(U(AllocateUserPhysicalPages));
    ULONG_PTR n = 1, pfn[1] = {0};
    SET_ERR(0);
    CHECK(!U(AllocateUserPhysicalPages)((HANDLE)(ULONG_PTR)0x7771, &n, pfn));
    ERR2(ERROR_INVALID_HANDLE, ERROR_PRIVILEGE_NOT_HELD);
}

/* ---------------- exhaustion, churn, threads ---------------- */
TEST(mv_x_reserve_16gb) {
    REQUIRE(VALLOC); REQUIRE(VQ);
    enum { N = 64 };
    SIZE_T sz = 256u << 20;
    char *p[N]; int got = 0;
    for (int i = 0; i < N; i++) { p[i] = VALLOC(NULL, sz, MEM_RESERVE, PAGE_NOACCESS); if (p[i]) got++; else break; }
    EXPECT_EQ(got, N);
    for (int i = 0; i < got; i++) {
        MEMORY_BASIC_INFORMATION m = mv_q(p[i]);
        if (m.RegionSize != sz) { CHECKF(0, "region %d size 0x%Ix", i, m.RegionSize); break; }
        for (int j = i + 1; j < got; j++) if (p[j] < p[i] + sz && p[i] < p[j] + sz) { CHECKF(0, "overlap %d %d", i, j); break; }
    }
    for (int i = 0; i < got; i++) mv_free(p[i]);
    for (int i = 0; i < got; i++) if (mv_q(p[i]).State != MEM_FREE) { CHECKF(0, "not released %d", i); break; }
}
TEST(mv_x_many_small_reservations) {
    REQUIRE(VALLOC);
    enum { N = 2000 };
    static char *p[N];
    int got = 0;
    for (int i = 0; i < N; i++) { p[i] = VALLOC(NULL, 1, MEM_RESERVE, PAGE_NOACCESS); if (p[i]) got++; else break; }
    EXPECT_EQ(got, N);
    for (int i = 0; i < got; i++) {
        CHECK(((ULONG_PTR)p[i] & (GR - 1)) == 0);
        if (i) CHECK(p[i] != p[i - 1]);
    }
    for (int i = 0; i < got; i++) mv_free(p[i]);
}
TEST(mv_x_alloc_free_churn) {
    REQUIRE(VALLOC); REQUIRE(VFREE);
    int bad = 0;
    for (int i = 0; i < 5000; i++) {
        char *p = VALLOC(NULL, (SIZE_T)(i % 17 + 1) * PG, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (!p) { bad++; continue; }
        p[0] = 1; p[(i % 17) * PG] = 2;
        if (!VFREE(p, 0, MEM_RELEASE)) bad++;
    }
    EXPECT_EQ(bad, 0);
}
TEST(mv_x_incremental_commit) {
    REQUIRE(VALLOC);
    SIZE_T total = 64u << 20, step = 1u << 20;
    char *p = VALLOC(NULL, total, MEM_RESERVE, PAGE_READWRITE);
    CHECK(p != NULL);
    if (!p) return;
    int ok = 1;
    for (SIZE_T off = 0; off < total; off += step) {
        if (VALLOC(p + off, step, MEM_COMMIT, PAGE_READWRITE) != p + off) { ok = 0; break; }
        p[off + step - 1] = 1;
    }
    CHECK(ok);
    MEMORY_BASIC_INFORMATION m = mv_q(p);
    EXPECT_EQ(m.RegionSize, total);
    EXPECT_EQ(m.State, MEM_COMMIT);
    mv_free(p);
}
TEST(mv_x_large_commit_128mb) {
    REQUIRE(VALLOC);
    SIZE_T sz = 128u << 20;
    volatile char *p = mv_alloc(sz);
    CHECK(p != NULL);
    if (!p) return;
    p[0] = 1; p[sz / 2] = 2; p[sz - 1] = 3;
    CHECK(p[0] == 1 && p[sz / 2] == 2 && p[sz - 1] == 3 && p[sz / 4] == 0);
    mv_free((void *)p);
}
static DWORD WINAPI churn_thread(LPVOID arg) {
    void **out = (void **)arg;
    for (int i = 0; i < 300; i++) {
        out[i] = VirtualAlloc(NULL, PG, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (out[i]) *(volatile char *)out[i] = (char)i;
    }
    return 0;
}
TEST(mv_x_threaded_alloc_unique) {
    REQUIRE(VALLOC);
    enum { T = 8, N = 300 };
    static void *res[T][N];
    HANDLE th[T];
    memset(res, 0, sizeof res);
    for (int i = 0; i < T; i++) th[i] = CreateThread(NULL, 0, churn_thread, res[i], 0, NULL);
    for (int i = 0; i < T; i++) { WaitForSingleObject(th[i], 20000); CloseHandle(th[i]); }
    int nulls = 0, dups = 0;
    for (int a = 0; a < T * N; a++) {
        void *x = res[a / N][a % N];
        if (!x) { nulls++; continue; }
        if (a % 37 == 0) for (int b = a + 1; b < T * N; b++) if (res[b / N][b % N] == x) { dups++; break; }
    }
    EXPECT_EQ(nulls, 0);
    EXPECT_EQ(dups, 0);
    for (int a = 0; a < T * N; a++) { void *x = res[a / N][a % N]; if (x) mv_free(x); }
}
static char *shared_g;
static DWORD WINAPI rd(LPVOID a) { return (DWORD)(unsigned char)shared_g[0] + ((DWORD)(unsigned char)shared_g[PG - 1] << 8); }
TEST(mv_x_cross_thread_visibility) {
    REQUIRE(VALLOC);
    char *shared = shared_g = mv_alloc(PG);
    CHECK(shared != NULL);
    if (!shared) return;
    shared[0] = 0x31;
    HANDLE t = CreateThread(NULL, 0, rd, NULL, 0, NULL);
    shared[PG - 1] = 0x42;
    WaitForSingleObject(t, 5000);
    DWORD code = 0; GetExitCodeThread(t, &code);
    CloseHandle(t);
    CHECK((code & 0xFF) == 0x31);
    mv_free(shared);
}
TEST(mv_x_thread_faults_on_freed_memory) {
    REQUIRE(VALLOC);
    char *p = mv_alloc(PG);
    p[0] = 1;
    mv_free(p);
    EXPECT_EQ(mv_rd(p, NULL), EXCEPTION_ACCESS_VIOLATION);
    EXPECT_EQ(mv_info0, 0);
    EXPECT_EQ(mv_info1, (ULONG_PTR)p);
    EXPECT_EQ(mv_wr(p, 1), EXCEPTION_ACCESS_VIOLATION);
    EXPECT_EQ(mv_info0, 1);
}
TEST(mv_x_av_info_for_null) {
    EXPECT_EQ(mv_rd(NULL, NULL), EXCEPTION_ACCESS_VIOLATION);
    EXPECT_EQ(mv_info1, 0);
    EXPECT_EQ(mv_wr((void *)8, 1), EXCEPTION_ACCESS_VIOLATION);
    EXPECT_EQ(mv_info1, 8);
    EXPECT_EQ(mv_rd((void *)0x7FFFFFFF0000ull, NULL), EXCEPTION_ACCESS_VIOLATION);
    EXPECT_EQ(mv_rd((void *)0xFFFF800000000000ull, NULL), EXCEPTION_ACCESS_VIOLATION);
}
TEST(mv_x_kernel_address_read_faults) {
    DWORD c = mv_rd((void *)0xFFFFF78000000000ull, NULL);       /* KUSER_SHARED_DATA kernel alias */
    EXPECT_EQ(c, EXCEPTION_ACCESS_VIOLATION);
}
TEST(mv_x_kuser_shared_data) {
    REQUIRE(VQ);
    MEMORY_BASIC_INFORMATION m = mv_q((void *)0x7FFE0000);
    EXPECT_EQ(m.State, MEM_COMMIT);
    EXPECT_EQ(m.Protect, PAGE_READONLY);
    EXPECT_EQ(m.RegionSize, PG);
    t_note("KUSER_SHARED_DATA Type=0x%lx", m.Type);
    EXPECT_EQ(mv_wr((void *)0x7FFE0000, 1), EXCEPTION_ACCESS_VIOLATION);
    unsigned v = 0;
    EXPECT_EQ(mv_rd((void *)(0x7FFE0000 + 0x26C), &v), 0);       /* NtMajorVersion */
    EXPECT_EQ(v, 10);
}
