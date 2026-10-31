#include "g16.h"
static g16_VirtualAlloc2_t VA2(void) { return (g16_VirtualAlloc2_t)t_proc_kb("VirtualAlloc2"); }
#define NEED REQUIRE(VA2())
#define RW (MEM_RESERVE | MEM_COMMIT)
static void rel(void *p) { if (p) VirtualFree(p, 0, MEM_RELEASE); }
CHILD(gap16_sleeper2) { Sleep(25000); return 0; }

TEST(gap16_va2_exported_from_kernelbase_only) {
    CHECK(t_proc_kb("VirtualAlloc2") != NULL);
    t_note("kernel32 export present: %d", t_proc_k32("VirtualAlloc2") != NULL);
}
TEST(gap16_va2_basic_current_process) {
    NEED; MEMORY_BASIC_INFORMATION m; SET_ERR(0);
    char *p = VA2()(NULL, NULL, 4096, RW, PAGE_READWRITE, NULL, 0); REQUIRE(p);
    CHECK(((ULONG_PTR)p & 0xffff) == 0);       /* allocation granularity */
    p[0] = 1; p[4095] = 2; CHECK(p[0] == 1 && p[4095] == 2);
    VirtualQuery(p, &m, sizeof m); EXPECT_EQ(m.State, MEM_COMMIT); EXPECT_EQ(m.Protect, PAGE_READWRITE); EXPECT_EQ(m.RegionSize, 4096); EXPECT_EQ(m.Type, MEM_PRIVATE);
    int z = 1; for (int i = 1; i < 4095; i++) if (p[i]) z = 0; CHECK(z);
    rel(p);
}
TEST(gap16_va2_pseudo_handle_same_as_null) {
    NEED; char *a = VA2()(NULL, NULL, 4096, RW, PAGE_READWRITE, NULL, 0), *b = VA2()(GetCurrentProcess(), NULL, 4096, RW, PAGE_READWRITE, NULL, 0), *c = VA2()(INVALID_HANDLE_VALUE, NULL, 4096, RW, PAGE_READWRITE, NULL, 0);
    CHECK(a && b && c); CHECK(a != b && b != c && a != c);
    rel(a); rel(b); rel(c);
}
TEST(gap16_va2_size_rounding) {
    NEED; MEMORY_BASIC_INFORMATION m;
    static const SIZE_T sz[] = { 1, 4095, 4096, 4097, 8192, 0x10000, 0x10001 };
    for (int i = 0; i < 7; i++) {
        char *p = VA2()(NULL, NULL, sz[i], RW, PAGE_READWRITE, NULL, 0); CHECKF(p != NULL, "size %zu", sz[i]); if (!p) continue;
        VirtualQuery(p, &m, sizeof m); SIZE_T want = (sz[i] + 4095) & ~(SIZE_T)4095;
        CHECKF(m.RegionSize == want, "size %zu region %zu want %zu", sz[i], m.RegionSize, want);
        p[sz[i] - 1] = 1; rel(p);
    }
}
TEST(gap16_va2_invalid_size_and_type) {
    NEED; void *p;
    SET_ERR(0); p = VA2()(NULL, NULL, 0, RW, PAGE_READWRITE, NULL, 0); CHECK(p == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); p = VA2()(NULL, NULL, 4096, 0, PAGE_READWRITE, NULL, 0); CHECK(p == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    static const ULONG badtype[] = { MEM_DECOMMIT, MEM_RELEASE, MEM_RESET | MEM_COMMIT, MEM_FREE };
    for (int i = 0; i < 4; i++) { SET_ERR(0); p = VA2()(NULL, NULL, 4096, badtype[i], PAGE_READWRITE, NULL, 0); CHECKF(p == NULL, "type 0x%lx", badtype[i]); if (p) rel(p); else EXPECT_ERR(ERROR_INVALID_PARAMETER); }
}
TEST(gap16_va2_absurd_sizes_fail) {
    NEED; void *p;
    static const SIZE_T sz[] = { (SIZE_T)1 << 47, (SIZE_T)1 << 48, (SIZE_T)-1, (SIZE_T)-4096, (SIZE_T)0x7FFFFFFF0000ull };
    for (int i = 0; i < 5; i++) { SET_ERR(0); p = VA2()(NULL, NULL, sz[i], MEM_RESERVE, PAGE_READWRITE, NULL, 0); CHECKF(p == NULL, "size %zx", sz[i]); if (p) rel(p); else { EXPECT_ERR2(ERROR_INVALID_PARAMETER, ERROR_NOT_ENOUGH_MEMORY); t_note("size %zx err %lu", sz[i], GetLastError()); } }
}
TEST(gap16_va2_invalid_protection) {
    NEED; void *p;
    static const ULONG bad[] = { 0, 3, 6, PAGE_GUARD, 0xFFFFFFFFu, PAGE_READONLY | PAGE_READWRITE };
    for (int i = 0; i < 6; i++) { SET_ERR(0); p = VA2()(NULL, NULL, 4096, RW, bad[i], NULL, 0); CHECKF(p == NULL, "prot 0x%lx", bad[i]); if (p) rel(p); else EXPECT_ERR(ERROR_INVALID_PARAMETER); }
}
TEST(gap16_va2_protection_table) {
    NEED; MEMORY_BASIC_INFORMATION m;
    static const ULONG pr[] = { PAGE_NOACCESS, PAGE_READONLY, PAGE_READWRITE, PAGE_WRITECOPY, PAGE_EXECUTE, PAGE_EXECUTE_READ, PAGE_READWRITE | PAGE_GUARD, PAGE_READWRITE | PAGE_NOCACHE };
    for (int i = 0; i < 8; i++) {
        char *p = VA2()(NULL, NULL, 4096, RW, pr[i], NULL, 0);
        if (!p) { t_note("prot 0x%lx err %lu", pr[i], GetLastError()); continue; }
        VirtualQuery(p, &m, sizeof m);
        ULONG want = pr[i] == PAGE_WRITECOPY ? PAGE_WRITECOPY : pr[i];
        CHECKF(m.Protect == want || (pr[i] == PAGE_WRITECOPY && m.Protect == PAGE_READWRITE), "prot 0x%lx got 0x%lx", pr[i], m.Protect);
        rel(p);
    }
}
TEST(gap16_va2_reserve_then_commit) {
    NEED; MEMORY_BASIC_INFORMATION m;
    char *p = VA2()(NULL, NULL, 0x10000, MEM_RESERVE, PAGE_READWRITE, NULL, 0); REQUIRE(p);
    VirtualQuery(p, &m, sizeof m); EXPECT_EQ(m.State, MEM_RESERVE); EXPECT_EQ(m.RegionSize, 0x10000);
    SET_ERR(0); char *c = VA2()(NULL, p, 8192, MEM_COMMIT, PAGE_READWRITE, NULL, 0);
    CHECKF(c == p, "commit returned %p err %lu", c, GetLastError());
    VirtualQuery(p, &m, sizeof m); EXPECT_EQ(m.State, MEM_COMMIT); EXPECT_EQ(m.RegionSize, 8192);
    p[8191] = 3;
    c = VA2()(NULL, p + 0x8000, 4096, MEM_COMMIT, PAGE_READONLY, NULL, 0); CHECK(c == p + 0x8000);
    VirtualQuery(p + 0x8000, &m, sizeof m); EXPECT_EQ(m.Protect, PAGE_READONLY);
    rel(p);
}
TEST(gap16_va2_commit_outside_reservation_fails) {
    NEED; char *p = VA2()(NULL, NULL, 0x10000, MEM_RESERVE, PAGE_READWRITE, NULL, 0); REQUIRE(p); void *c;
    SET_ERR(0); c = VA2()(NULL, p, 0x20000, MEM_COMMIT, PAGE_READWRITE, NULL, 0); CHECK(c == NULL); t_note("commit beyond reservation err %lu", GetLastError());
    rel(p);
    SET_ERR(0); c = VA2()(NULL, p, 4096, MEM_COMMIT, PAGE_READWRITE, NULL, 0); t_note("commit in freed range -> %p (a fresh implicit reserve is allowed)", c); rel(c);
}
TEST(gap16_va2_fixed_base_address) {
    NEED; char *p = VA2()(NULL, NULL, 0x10000, MEM_RESERVE, PAGE_READWRITE, NULL, 0); REQUIRE(p); rel(p);
    SET_ERR(0); char *q = VA2()(NULL, p, 0x10000, MEM_RESERVE, PAGE_READWRITE, NULL, 0);
    if (q) { CHECK(q == p); SET_ERR(0); char *r = VA2()(NULL, p, 0x10000, MEM_RESERVE, PAGE_READWRITE, NULL, 0); CHECK(r == NULL); EXPECT_ERR2(ERROR_INVALID_ADDRESS, ERROR_INVALID_PARAMETER); rel(q); }
    else t_note("address reuse raced err %lu", GetLastError());
}
TEST(gap16_va2_base_address_rounded_down) {
    NEED; char *p = VA2()(NULL, NULL, 0x10000, MEM_RESERVE, PAGE_READWRITE, NULL, 0); REQUIRE(p); rel(p);
    char *q = VA2()(NULL, p + 0x1234, 0x10000, MEM_RESERVE, PAGE_READWRITE, NULL, 0);
    if (q) { CHECK(q == p); rel(q); } else t_note("rounded base err %lu", GetLastError());
}
TEST(gap16_va2_high_base_address_invalid) {
    NEED; void *p;
    static const ULONG_PTR a[] = { 0x7FFFFFFF0000ull, 0x800000000000ull, 0xFFFFFFFFFFFF0000ull };
    for (int i = 0; i < 3; i++) { SET_ERR(0); p = VA2()(NULL, (void *)a[i], 0x10000, MEM_RESERVE, PAGE_READWRITE, NULL, 0); CHECKF(p == NULL, "base %p", (void *)a[i]); if (p) rel(p); else EXPECT_ERR(ERROR_INVALID_PARAMETER); }
}
TEST(gap16_va2_top_down_and_write_watch) {
    NEED; MEMORY_BASIC_INFORMATION m;
    char *a = VA2()(NULL, NULL, 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE, NULL, 0);
    char *b = VA2()(NULL, NULL, 0x10000, MEM_RESERVE | MEM_COMMIT | MEM_TOP_DOWN, PAGE_READWRITE, NULL, 0);
    REQUIRE(a && b); CHECKF(b > a, "top-down %p should be above %p", b, a);
    char *w = VA2()(NULL, NULL, 8192, MEM_RESERVE | MEM_COMMIT | MEM_WRITE_WATCH, PAGE_READWRITE, NULL, 0);
    if (w) { w[0] = 1; void *addrs[4]; ULONG_PTR cnt = 4; ULONG gran = 0;
        UINT r = GetWriteWatch(WRITE_WATCH_FLAG_RESET, w, 8192, addrs, &cnt, &gran);
        CHECK(r == 0); CHECKF(cnt == 1 && addrs[0] == (void *)w, "cnt %zu", cnt); rel(w); } else t_note("write watch err %lu", GetLastError());
    rel(a); rel(b);
}
TEST(gap16_va2_large_pages_needs_privilege) {
    NEED; SET_ERR(0);
    void *p = VA2()(NULL, NULL, 2 << 20, MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES, PAGE_READWRITE, NULL, 0);
    if (p) { t_note("large pages succeeded"); rel(p); } else { t_note("large pages err %lu (expected privilege/size failure)", GetLastError()); }
}
TEST(gap16_va2_placeholder_requires_noaccess) {
    NEED; void *p; SET_ERR(0);
    p = VA2()(NULL, NULL, 0x10000, MEM_RESERVE | 0x40000, PAGE_READWRITE, NULL, 0);
    if (p) { t_note("placeholder with RW accepted"); rel(p); return; }
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); p = VA2()(NULL, NULL, 0x10000, MEM_COMMIT | 0x40000, PAGE_NOACCESS, NULL, 0); CHECK(p == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap16_va2_placeholder_basic) {
    NEED; MEMORY_BASIC_INFORMATION m; SET_ERR(0);
    char *p = VA2()(NULL, NULL, 0x20000, MEM_RESERVE | 0x40000, PAGE_NOACCESS, NULL, 0);
    if (!p) { t_skip("placeholders unavailable err %lu", GetLastError()); return; }
    VirtualQuery(p, &m, sizeof m); EXPECT_EQ(m.State, MEM_RESERVE); EXPECT_EQ(m.Type, MEM_PRIVATE); EXPECT_EQ(m.RegionSize, 0x20000);
    SET_ERR(0); t_note("decommit placeholder -> %d", VirtualFree(p, 0x10000, MEM_DECOMMIT));
    rel(p);
}
TEST(gap16_va2_extparam_bad_types) {
    NEED; g16_mep e; void *p; memset(&e, 0, sizeof e);
    static const ULONG64 ty[] = { 0, 3, 5, 6, 7, 8, 0xff, 0x13 };
    for (int i = 0; i < 8; i++) {
        e.Type = ty[i]; e.u.U64 = 0; SET_ERR(0);
        p = VA2()(NULL, NULL, 4096, RW, PAGE_READWRITE, &e, 1);
        CHECKF(p == NULL, "ext type %llu", (unsigned long long)ty[i]); if (p) rel(p); else EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(gap16_va2_extparam_second_entry_checked) {
    NEED; g16_mep e[3]; void *p; memset(e, 0, sizeof e);
    g16_addrreq rq = { 0, 0, 0 };
    e[0].Type = G16_MEP_ADDRREQ; e[0].u.Pointer = &rq; e[1].Type = 3; e[2].Type = G16_MEP_ADDRREQ; e[2].u.Pointer = &rq;
    SET_ERR(0); p = VA2()(NULL, NULL, 4096, RW, PAGE_READWRITE, e, 3); CHECK(p == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); p = VA2()(NULL, NULL, 4096, RW, PAGE_READWRITE, e, 1); t_note("only first entry counted -> %p err %lu", p, GetLastError()); rel(p);
    SET_ERR(0); p = VA2()(NULL, NULL, 4096, RW, PAGE_READWRITE, e, 2); CHECK(p == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap16_va2_extparam_ignored_when_count_zero) {
    NEED; g16_mep e; memset(&e, 0, sizeof e); e.Type = 99; SET_ERR(0);
    char *p = VA2()(NULL, NULL, 4096, RW, PAGE_READWRITE, &e, 0); CHECKF(p != NULL, "err %lu", GetLastError()); rel(p);
}
TEST(gap16_va2_addr_requirements_alignment) {
    NEED;
    static const SIZE_T al[] = { 0x10000, 0x20000, 0x100000, 0x400000 };
    for (int i = 0; i < 4; i++) {
        g16_addrreq rq = { NULL, NULL, al[i] }; g16_mep e; memset(&e, 0, sizeof e); e.Type = G16_MEP_ADDRREQ; e.u.Pointer = &rq;
        SET_ERR(0); char *p = VA2()(NULL, NULL, 0x10000, RW, PAGE_READWRITE, &e, 1);
        CHECKF(p != NULL, "align %zx err %lu", al[i], GetLastError());
        if (p) { CHECKF(((ULONG_PTR)p & (al[i] - 1)) == 0, "align %zx got %p", al[i], p); p[0] = 1; rel(p); }
    }
}
TEST(gap16_va2_addr_requirements_bad_alignment) {
    NEED;
    static const SIZE_T al[] = { 3, 0x30000, 0x18000, 0x10001 };
    for (int i = 0; i < 4; i++) {
        g16_addrreq rq = { NULL, NULL, al[i] }; g16_mep e; memset(&e, 0, sizeof e); e.Type = G16_MEP_ADDRREQ; e.u.Pointer = &rq;
        SET_ERR(0); char *p = VA2()(NULL, NULL, 0x10000, RW, PAGE_READWRITE, &e, 1);
        CHECKF(p == NULL, "align %zx accepted", al[i]); if (p) rel(p); else EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(gap16_va2_addr_requirements_range) {
    NEED; ULONG_PTR lo = 0x10000000000ull, hi = 0x10000FFFFFFull;
    g16_addrreq rq = { (PVOID)lo, (PVOID)hi, 0 }; g16_mep e; memset(&e, 0, sizeof e); e.Type = G16_MEP_ADDRREQ; e.u.Pointer = &rq;
    SET_ERR(0); char *p = VA2()(NULL, NULL, 0x10000, RW, PAGE_READWRITE, &e, 1);
    if (!p) { t_note("range alloc err %lu", GetLastError()); return; }
    CHECKF((ULONG_PTR)p >= lo && (ULONG_PTR)p + 0x10000 - 1 <= hi, "got %p", p); rel(p);
}
TEST(gap16_va2_addr_requirements_bad_highest) {
    NEED; g16_addrreq rq = { NULL, (PVOID)0x12345000, 0 }; g16_mep e; memset(&e, 0, sizeof e); e.Type = G16_MEP_ADDRREQ; e.u.Pointer = &rq;
    SET_ERR(0); char *p = VA2()(NULL, NULL, 0x10000, RW, PAGE_READWRITE, &e, 1);
    CHECK(p == NULL); if (p) rel(p); else EXPECT_ERR(ERROR_INVALID_PARAMETER);
    rq.Highest = (PVOID)0xFFF; SET_ERR(0); p = VA2()(NULL, NULL, 0x10000, RW, PAGE_READWRITE, &e, 1); CHECK(p == NULL); if (p) rel(p);
}
TEST(gap16_va2_addr_requirements_too_small_window) {
    NEED; g16_addrreq rq = { (PVOID)0x20000000000ull, (PVOID)0x20000000FFFull, 0 }; g16_mep e; memset(&e, 0, sizeof e); e.Type = G16_MEP_ADDRREQ; e.u.Pointer = &rq;
    SET_ERR(0); char *p = VA2()(NULL, NULL, 0x10000, RW, PAGE_READWRITE, &e, 1);
    CHECK(p == NULL); if (p) rel(p); else t_note("window too small err %lu", GetLastError());
}
TEST(gap16_va2_numa_node_param) {
    NEED; g16_mep e; memset(&e, 0, sizeof e); e.Type = G16_MEP_NUMA; e.u.U32 = 0; SET_ERR(0);
    char *p = VA2()(NULL, NULL, 4096, RW, PAGE_READWRITE, &e, 1);
    if (!p) { t_note("numa node 0 err %lu", GetLastError()); return; }
    p[0] = 1; rel(p);
    e.u.U32 = 0xFFFFFF; SET_ERR(0); p = VA2()(NULL, NULL, 4096, RW, PAGE_READWRITE, &e, 1);
    t_note("bogus numa node -> %p err %lu", p, GetLastError()); rel(p);
}
TEST(gap16_va2_count_without_array) {
    NEED; SET_ERR(0); void *p = VA2()(NULL, NULL, 4096, RW, PAGE_READWRITE, NULL, 1);
    t_note("count=1 array=NULL -> %p err %lu", p, GetLastError()); rel(p);
}
TEST(gap16_va2_bad_process_handles) {
    NEED; void *p; int dummy;
    SET_ERR(0); p = VA2()((HANDLE)(LONG_PTR)-2, NULL, 4096, RW, PAGE_READWRITE, NULL, 0); CHECK(p == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0); p = VA2()(e, NULL, 4096, RW, PAGE_READWRITE, NULL, 0); CHECK(p == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(e);
    SET_ERR(0); p = VA2()(e, NULL, 4096, RW, PAGE_READWRITE, NULL, 0); CHECK(p == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); p = VA2()(GetCurrentThread(), NULL, 4096, RW, PAGE_READWRITE, NULL, 0); CHECK(p == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap16_va2_remote_process) {
    NEED; PROCESS_INFORMATION pi; HANDLE h = t_spawn_child("gap16_sleeper2", "", 0, NULL, NULL, NULL, FALSE, NULL, &pi); REQUIRE(h);
    MEMORY_BASIC_INFORMATION m;
    char *p = VA2()(h, NULL, 0x3000, RW, PAGE_READWRITE, NULL, 0); CHECKF(p != NULL, "err %lu", GetLastError());
    if (p) {
        memset(&m, 0, sizeof m); VirtualQueryEx(h, p, &m, sizeof m); EXPECT_EQ(m.State, MEM_COMMIT); EXPECT_EQ(m.RegionSize, 0x3000);
        char src[8] = "remote!"; SIZE_T w = 0; CHECK(WriteProcessMemory(h, p, src, 8, &w)); char dst[8] = { 0 }; ReadProcessMemory(h, p, dst, 8, NULL); CHECK(!memcmp(src, dst, 8));
        VirtualFreeEx(h, p, 0, MEM_RELEASE);
    }
    SET_ERR(0); p = VA2()(h, NULL, 0, RW, PAGE_READWRITE, NULL, 0); CHECK(p == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    TerminateProcess(h, 0); WaitForSingleObject(h, 5000);
    SET_ERR(0); p = VA2()(h, NULL, 4096, RW, PAGE_READWRITE, NULL, 0); CHECK(p == NULL); t_note("terminated err %lu", GetLastError());
    CloseHandle(h); CloseHandle(pi.hThread);
}
TEST(gap16_va2_remote_access_rights) {
    NEED; PROCESS_INFORMATION pi; HANDLE h = t_spawn_child("gap16_sleeper2", "", 0, NULL, NULL, NULL, FALSE, NULL, &pi); REQUIRE(h);
    HANDLE a = NULL, b = NULL;
    DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(), &a, PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE, FALSE, 0);
    DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(), &b, PROCESS_VM_OPERATION, FALSE, 0);
    REQUIRE(a && b);
    SET_ERR(0); void *p = VA2()(a, NULL, 4096, RW, PAGE_READWRITE, NULL, 0); CHECK(p == NULL); EXPECT_ERR(ERROR_ACCESS_DENIED);
    SET_ERR(0); p = VA2()(b, NULL, 4096, RW, PAGE_READWRITE, NULL, 0); CHECKF(p != NULL, "err %lu", GetLastError());
    if (p) VirtualFreeEx(h, p, 0, MEM_RELEASE);
    CloseHandle(a); CloseHandle(b); TerminateProcess(h, 0); WaitForSingleObject(h, 5000); CloseHandle(h); CloseHandle(pi.hThread);
}
TEST(gap16_va2_many_allocations_distinct) {
    NEED; void *p[64]; int ok = 1;
    for (int i = 0; i < 64; i++) { p[i] = VA2()(NULL, NULL, 4096, RW, PAGE_READWRITE, NULL, 0); if (!p[i]) ok = 0; }
    CHECK(ok);
    for (int i = 0; i < 64; i++) for (int j = i + 1; j < 64; j++) if (p[i] && p[i] == p[j]) ok = 0;
    CHECK(ok); for (int i = 0; i < 64; i++) rel(p[i]);
}
TEST(gap16_va2_mem_reset_flag) {
    NEED; char *p = VA2()(NULL, NULL, 4096, RW, PAGE_READWRITE, NULL, 0); REQUIRE(p); p[0] = 5;
    SET_ERR(0); void *r = VA2()(NULL, p, 4096, MEM_RESET, PAGE_READWRITE, NULL, 0);
    CHECKF(r == (void *)p, "reset -> %p err %lu", r, GetLastError());
    SET_ERR(0); r = VA2()(NULL, NULL, 4096, MEM_RESET, PAGE_READWRITE, NULL, 0); t_note("reset no base err %lu", GetLastError());
    rel(p);
}
TEST(gap16_va2_free_with_virtualfree_decommit) {
    NEED; char *p = VA2()(NULL, NULL, 8192, RW, PAGE_READWRITE, NULL, 0); REQUIRE(p);
    CHECK(VirtualFree(p, 8192, MEM_DECOMMIT)); MEMORY_BASIC_INFORMATION m; VirtualQuery(p, &m, sizeof m); EXPECT_EQ(m.State, MEM_RESERVE); rel(p);
}
