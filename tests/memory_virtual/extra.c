#include "mvc.h"
/* Extra coverage: IsBad*Ptr, NUMA queries, FromApp variants, misc argument edge cases. */

/* ---------------- IsBad*Ptr (probe-only, never dereference unmapped memory in the test) ---------------- */
TEST(mv_isbad_read_null_and_zero) {
    REQUIRE(U(IsBadReadPtr));
    char buf[16] = {0};
    CHECK(U(IsBadReadPtr)(NULL, 1));
    CHECK(U(IsBadReadPtr)(NULL, 100));
    CHECK(!U(IsBadReadPtr)(NULL, 0));          /* zero length is never bad */
    CHECK(!U(IsBadReadPtr)(buf, 0));
    CHECK(!U(IsBadReadPtr)(buf, sizeof buf));
    CHECK(!U(IsBadReadPtr)(buf, 1));
}
TEST(mv_isbad_read_wraparound) {
    REQUIRE(U(IsBadReadPtr));
    char buf[16];
    if (t_is_wine()) { t_skip("wine probes before the overflow check"); return; }
    CHECK(U(IsBadReadPtr)(buf, (UINT_PTR)-1));  /* end pointer wraps below start */
}
TEST(mv_isbad_write_null_and_valid) {
    REQUIRE(U(IsBadWritePtr));
    char buf[16];
    CHECK(U(IsBadWritePtr)(NULL, 1));
    CHECK(!U(IsBadWritePtr)(NULL, 0));
    CHECK(!U(IsBadWritePtr)(buf, sizeof buf));
    if (!t_is_wine()) CHECK(U(IsBadWritePtr)(buf, (UINT_PTR)-1));
}
TEST(mv_isbad_code_null) {
    REQUIRE(U(IsBadCodePtr));
    CHECK(U(IsBadCodePtr)(NULL));
    CHECK(!U(IsBadCodePtr)((FARPROC)(void *)&mv_exec));
}
TEST(mv_isbad_string_null_and_valid) {
    REQUIRE(U(IsBadStringPtrA)); REQUIRE(U(IsBadStringPtrW));
    CHECK(U(IsBadStringPtrA)(NULL, 10));
    CHECK(U(IsBadStringPtrW)(NULL, 10));
    CHECK(!U(IsBadStringPtrA)(NULL, 0));
    CHECK(!U(IsBadStringPtrW)(NULL, 0));
    CHECK(!U(IsBadStringPtrA)("abc", 100));     /* stops at the terminator */
    CHECK(!U(IsBadStringPtrW)(L"abc", 100));
    CHECK(!U(IsBadStringPtrA)("abc", 3));
}
TEST(mv_isbad_read_protected_pages) {
    REQUIRE(U(IsBadReadPtr)); REQUIRE(U(IsBadWritePtr)); REQUIRE(VALLOC); REQUIRE(VPROT);
    char *p = mv_alloc(2 * PG);
    CHECK(p != NULL);
    if (!p) return;
    CHECK(!U(IsBadReadPtr)(p, 2 * PG));
    CHECK(!U(IsBadWritePtr)(p, 2 * PG));
    DWORD old;
    CHECK(VPROT(p + PG, PG, PAGE_READONLY, &old));
    CHECK(!U(IsBadReadPtr)(p, 2 * PG));
    mv_free(p);
}

/* ---------------- NUMA ---------------- */
TEST(mv_numa_highest_node) {
    REQUIRE(U(GetNumaHighestNodeNumber));
    ULONG n = 0xEEEE;
    SET_ERR(0);
    CHECK(U(GetNumaHighestNodeNumber)(&n));
    CHECK(n < 64);
    t_note("highest NUMA node %lu", n);
}
TEST(mv_numa_available_memory_node) {
    REQUIRE(U(GetNumaAvailableMemoryNodeEx)); if (t_is_wine()) { t_skip("unimplemented under wine"); return; } REQUIRE(U(GetNumaHighestNodeNumber));
    ULONG hi = 0;
    CHECK(U(GetNumaHighestNodeNumber)(&hi));
    ULONGLONG m = 0xEE;
    SET_ERR(0);
    BOOL r = U(GetNumaAvailableMemoryNodeEx)(0, &m);
    CHECK(r);
    if (r) CHECK(m != 0xEE);
    SET_ERR(0);
    m = 0;
    r = U(GetNumaAvailableMemoryNodeEx)(0xFFFF, &m);
    CHECK(!r);
    t_note("invalid node: err=%lu", GetLastError());
}
TEST(mv_numa_proximity_node) {
    REQUIRE(U(GetNumaProximityNodeEx));
    USHORT px = 0;
    SET_ERR(0);
    BOOL r = U(GetNumaProximityNodeEx)(0, &px);
    t_note("GetNumaProximityNodeEx(0): r=%d px=%u err=%lu", r, px, GetLastError());
    SET_ERR(0);
    r = U(GetNumaProximityNodeEx)(0xFFFF, &px);
    t_note("GetNumaProximityNodeEx(0xFFFF): r=%d err=%lu", r, GetLastError());
}

/* ---------------- FromApp variants ---------------- */
TEST(mv_fromapp_open_mapping_invalid_inherit) {
    REQUIRE(U(OpenFileMappingFromApp));
    SET_ERR(0);
    HANDLE h = U(OpenFileMappingFromApp)(FILE_MAP_READ, 2, L"k32t_nonexistent_fa");
    CHECK(h == NULL);
    ERR1(ERROR_INVALID_PARAMETER);              /* bInheritHandle must be 0 or 1 */
}
TEST(mv_fromapp_open_mapping_roundtrip) {
    REQUIRE(U(OpenFileMappingFromApp)); REQUIRE(U(CreateFileMappingW));
    WCHAR nm[64]; wsprintfW(nm, L"k32t_fa_%lu", GetCurrentProcessId());
    HANDLE m = U(CreateFileMappingW)(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, nm);
    CHECK(m != NULL);
    if (!m) return;
    HANDLE o = U(OpenFileMappingFromApp)(FILE_MAP_READ, 0, nm);
    CHECK(o != NULL);
    if (o) CloseHandle(o);
    SET_ERR(0);
    o = U(OpenFileMappingFromApp)(FILE_MAP_READ, 0, L"k32t_nonexistent_fa");
    CHECK(o == NULL);
    ERR1(ERROR_FILE_NOT_FOUND);
    CloseHandle(m);
}
TEST(mv_fromapp_virtualalloc_roundtrip) {
    REQUIRE(U(VirtualAllocFromApp)); REQUIRE(U(VirtualProtectFromApp));
    void *p = U(VirtualAllocFromApp)(NULL, PG, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    CHECK(p != NULL);
    if (!p) return;
    DWORD old = 0;
    CHECK(U(VirtualProtectFromApp)(p, PG, PAGE_READONLY, &old));
    EXPECT_EQ(old, PAGE_READWRITE);
    SET_ERR(0);
    BOOL xr = U(VirtualProtectFromApp)(p, PG, PAGE_EXECUTE_READWRITE, &old);   /* requires codeGeneration capability */
    t_note("VirtualProtectFromApp(RWX): r=%d err=%lu", xr, GetLastError());
    mv_free(p);
}
TEST(mv_fromapp_virtualalloc_exec_rejected) {
    REQUIRE(U(VirtualAllocFromApp));
    SET_ERR(0);
    void *p = U(VirtualAllocFromApp)(NULL, PG, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    CHECK(p == NULL);                           /* any execute bit (0xf0) is rejected */
    ERR1(ERROR_INVALID_PARAMETER);
    if (p) mv_free(p);
}

/* ---------------- Misc argument edges ---------------- */
TEST(mv_flush_icache_self_stub) {
    REQUIRE(U(FlushInstructionCache)); REQUIRE(VALLOC);
    unsigned char *p = VALLOC(NULL, PG, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    CHECK(p != NULL);
    if (!p) return;
    memcpy(p, mv_stub, sizeof mv_stub);
    CHECK(U(FlushInstructionCache)(GetCurrentProcess(), p, sizeof mv_stub));
    int rv = 0;
    DWORD c = mv_exec(p, &rv);
    if (c == 0) EXPECT_EQ(rv, 0x1234);
    mv_free(p);
}
TEST(mv_getlargepageminimum_pow2) {
    REQUIRE(U(GetLargePageMinimum));
    SIZE_T m = U(GetLargePageMinimum)();
    if (m) { CHECK((m & (m - 1)) == 0); CHECK(m >= 0x10000); }
    t_note("large page minimum 0x%Ix", m);
}
TEST(mv_valloc_large_pages_needs_privilege_or_ok) {
    REQUIRE(VALLOC); REQUIRE(U(GetLargePageMinimum));
    SIZE_T lp = U(GetLargePageMinimum)();
    if (!lp) { t_skip("no large page support"); return; }
    if (t_is_wine()) { t_skip("wine reports 87"); return; }
    SET_ERR(0);
    void *p = VALLOC(NULL, lp, MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES, PAGE_READWRITE);
    if (!p) { ERR2(ERROR_PRIVILEGE_NOT_HELD, ERROR_NO_SYSTEM_RESOURCES); }
    else mv_free(p);
}
