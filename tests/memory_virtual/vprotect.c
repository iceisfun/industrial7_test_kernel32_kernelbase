#include "mvc.h"
/* VirtualProtect / VirtualProtectEx / VirtualQuery(Ex) / guard pages / execution */

TEST(mv_vp_basic_transitions) {
    REQUIRE(VPROT); REQUIRE(VQ);
    char *p = mv_alloc(PG);
    CHECK(p != NULL);
    if (!p) return;
    DWORD seq[] = { PAGE_READONLY, PAGE_NOACCESS, PAGE_READWRITE, PAGE_EXECUTE_READ, PAGE_EXECUTE, PAGE_EXECUTE_READWRITE, PAGE_READONLY };
    DWORD cur = PAGE_READWRITE;
    for (unsigned i = 0; i < sizeof seq / sizeof *seq; i++) {
        DWORD old = 0xCCCCCCCC;
        SET_ERR(0);
        BOOL r = VPROT(p, PG, seq[i], &old);
        CHECKF(r, "to 0x%lx err %lu", seq[i], GetLastError());
        EXPECT_EQ(old, cur);
        MEMORY_BASIC_INFORMATION m = mv_q(p);
        EXPECT_EQ(m.Protect, seq[i]);
        EXPECT_EQ(m.AllocationProtect, PAGE_READWRITE);   /* allocation protect never changes */
        cur = seq[i];
    }
    mv_free(p);
}
TEST(mv_vp_null_old_protect) {
    REQUIRE(VPROT);
    char *p = mv_alloc(PG);
    CHECK(p != NULL);
    SET_ERR(0);
    CHECK(!VPROT(p, PG, PAGE_READONLY, NULL));
    ERR1(ERROR_NOACCESS);
    MEMORY_BASIC_INFORMATION m = mv_q(p);
    EXPECT_EQ(m.Protect, PAGE_READWRITE);
    mv_free(p);
}
TEST(mv_vp_zero_size) {
    REQUIRE(VPROT);
    char *p = mv_alloc(PG);
    DWORD old = 0;
    SET_ERR(0);
    CHECK(!VPROT(p, 0, PAGE_READONLY, &old));
    ERR1(ERROR_INVALID_PARAMETER);
    mv_free(p);
}
TEST(mv_vp_invalid_new_protect) {
    REQUIRE(VPROT);
    char *p = mv_alloc(PG);
    DWORD bad[] = { 0, 3, 5, 6, 0x0C, 0x30, 0x100, 0x101, 0x200, 0x400, 0x1000, 0x80000000 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        DWORD old = 0;
        SET_ERR(0);
        BOOL r = VPROT(p, PG, bad[i], &old);
        CHECKF(!r, "protect 0x%lx", bad[i]);
        if (r) VPROT(p, PG, PAGE_READWRITE, &old); else ERR1(ERROR_INVALID_PARAMETER);
    }
    mv_free(p);
}
TEST(mv_vp_free_memory) {
    REQUIRE(VPROT);
    void *h = mv_hole(GR);
    DWORD old = 0;
    SET_ERR(0);
    CHECK(!VPROT(h, PG, PAGE_READONLY, &old));
    ERR1(ERROR_INVALID_ADDRESS);
    SET_ERR(0);
    CHECK(!VPROT(NULL, PG, PAGE_READONLY, &old));
    ERR2(ERROR_INVALID_ADDRESS, ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!VPROT((void *)0x7FFFFFFF0000ull, PG, PAGE_READONLY, &old));
    ERR2(ERROR_INVALID_ADDRESS, ERROR_INVALID_PARAMETER);
}
TEST(mv_vp_reserved_not_committed) {
    REQUIRE(VPROT);
    char *p = VALLOC(NULL, GR, MEM_RESERVE, PAGE_READWRITE);
    CHECK(p != NULL);
    if (!p) return;
    DWORD old = 0;
    SET_ERR(0);
    CHECK(!VPROT(p, PG, PAGE_READONLY, &old));
    t_note("VirtualProtect on reserved page: err=%lu", GetLastError());
    ERR2(ERROR_INVALID_ADDRESS, ERROR_NOACCESS);
    mv_free(p);
}
TEST(mv_vp_partial_page_affects_whole_page) {
    REQUIRE(VPROT);
    char *p = mv_alloc(2 * PG);
    DWORD old = 0;
    CHECK(VPROT(p + 0x10, 1, PAGE_READONLY, &old));
    MEMORY_BASIC_INFORMATION m = mv_q(p);
    EXPECT_EQ(m.Protect, PAGE_READONLY); EXPECT_EQ(m.RegionSize, PG);
    m = mv_q(p + PG);
    EXPECT_EQ(m.Protect, PAGE_READWRITE);
    /* range straddling a page boundary covers both pages */
    CHECK(VPROT(p + PG - 1, 2, PAGE_READONLY, &old));
    m = mv_q(p);
    EXPECT_EQ(m.RegionSize, 2 * PG); EXPECT_EQ(m.Protect, PAGE_READONLY);
    mv_free(p);
}
TEST(mv_vp_old_protect_is_first_page) {
    REQUIRE(VPROT);
    char *p = mv_alloc(3 * PG);
    DWORD old = 0;
    CHECK(VPROT(p + PG, PG, PAGE_READONLY, &old));
    CHECK(VPROT(p, 3 * PG, PAGE_EXECUTE_READ, &old));
    EXPECT_EQ(old, PAGE_READWRITE);              /* first page's protection */
    MEMORY_BASIC_INFORMATION m = mv_q(p);
    EXPECT_EQ(m.Protect, PAGE_EXECUTE_READ); EXPECT_EQ(m.RegionSize, 3 * PG);
    CHECK(VPROT(p + PG, 2 * PG, PAGE_READWRITE, &old));
    EXPECT_EQ(old, PAGE_EXECUTE_READ);
    mv_free(p);
}
TEST(mv_vp_span_reserved_tail_fails) {
    REQUIRE(VPROT);
    char *p = VALLOC(NULL, 4 * PG, MEM_RESERVE, PAGE_READWRITE);
    CHECK(p != NULL);
    if (!p) return;
    CHECK(VALLOC(p, 2 * PG, MEM_COMMIT, PAGE_READWRITE) == p);
    DWORD old = 0;
    SET_ERR(0);
    BOOL r = VPROT(p, 4 * PG, PAGE_READONLY, &old);
    CHECK(!r);
    t_note("protect across committed+reserved: r=%d err=%lu", r, GetLastError());
    mv_free(p);
}
TEST(mv_vp_span_two_allocations_fails) {
    REQUIRE(VPROT);
    void *h = mv_hole(2 * GR);
    char *a = h ? VALLOC(h, GR, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) : NULL;
    char *b = a ? VALLOC(a + GR, GR, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) : NULL;
    if (!a || !b) { t_note("could not get adjacent allocations"); mv_free(a); mv_free(b); return; }
    DWORD old = 0;
    SET_ERR(0);
    CHECK(!VPROT(a + GR - PG, 2 * PG, PAGE_READONLY, &old));
    t_note("protect across allocation boundary: err=%lu", GetLastError());
    mv_free(a); mv_free(b);
}
TEST(mv_vp_region_coalescing) {
    REQUIRE(VPROT);
    char *p = mv_alloc(8 * PG);
    DWORD old;
    MEMORY_BASIC_INFORMATION m = mv_q(p);
    EXPECT_EQ(m.RegionSize, 8 * PG);
    CHECK(VPROT(p + 3 * PG, PG, PAGE_READONLY, &old));
    m = mv_q(p);           EXPECT_EQ(m.RegionSize, 3 * PG);
    m = mv_q(p + 3 * PG);  EXPECT_EQ(m.RegionSize, PG); EXPECT_EQ(m.BaseAddress, p + 3 * PG);
    m = mv_q(p + 4 * PG);  EXPECT_EQ(m.RegionSize, 4 * PG);
    m = mv_q(p + 5 * PG + 0x321);
    EXPECT_EQ(m.BaseAddress, p + 5 * PG);
    EXPECT_EQ(m.AllocationBase, p);
    EXPECT_EQ(m.RegionSize, 3 * PG);
    CHECK(VPROT(p + 3 * PG, PG, PAGE_READWRITE, &old));
    m = mv_q(p);           EXPECT_EQ(m.RegionSize, 8 * PG);     /* coalesced again */
    mv_free(p);
}
TEST(mv_vq_query_sizes) {
    REQUIRE(VQ);
    char *p = mv_alloc(PG);
    MEMORY_BASIC_INFORMATION m;
    SIZE_T lens[] = { 0, 1, 8, 16, 24, 32, 40, 47 };
    for (unsigned i = 0; i < sizeof lens / sizeof *lens; i++) {
        SET_ERR(0);
        memset(&m, 0xEE, sizeof m);
        SIZE_T r = VQ(p, &m, lens[i]);
        CHECKF(r == 0, "len %Iu got %Iu", lens[i], r);
        ERR1(ERROR_BAD_LENGTH);
    }
    SET_ERR(0);
    EXPECT_EQ(VQ(p, &m, sizeof m), sizeof m);
    unsigned char big[128]; memset(big, 0xEE, sizeof big);
    SET_ERR(0);
    SIZE_T r = VQ(p, (PMEMORY_BASIC_INFORMATION)big, sizeof big);
    EXPECT_EQ(r, sizeof m);
    CHECK(big[sizeof m] == 0xEE);
    mv_free(p);
}
TEST(mv_vq_out_of_range_address) {
    REQUIRE(VQ);
    MEMORY_BASIC_INFORMATION m;
    ULONG_PTR addrs[] = { 0x7FFFFFFF0000ull, 0x7FFFFFFFFFFFull, 0x800000000000ull, 0xFFFF800000000000ull, (ULONG_PTR)-1, (ULONG_PTR)-0x1000 };
    for (unsigned i = 0; i < sizeof addrs / sizeof *addrs; i++) {
        SET_ERR(0);
        SIZE_T r = VQ((void *)addrs[i], &m, sizeof m);
        CHECKF(r == 0, "addr %p", (void *)addrs[i]);
        ERR1(ERROR_INVALID_PARAMETER);
    }
}
TEST(mv_vq_null_address_is_free) {
    REQUIRE(VQ);
    MEMORY_BASIC_INFORMATION m; memset(&m, 0xEE, sizeof m);
    SET_ERR(0);
    EXPECT_EQ(VQ(NULL, &m, sizeof m), sizeof m);
    EXPECT_EQ(m.BaseAddress, 0);
    EXPECT_EQ(m.AllocationBase, 0);
    EXPECT_EQ(m.State, MEM_FREE);
    EXPECT_EQ(m.Type, 0);
    /* the free region runs from the queried page up to the next allocation, so its size depends on the process layout (Win10: 0x7ffe0000) */
    CHECK(m.RegionSize >= (SIZE_T)(ULONG_PTR)mv_sysinfo().lpMinimumApplicationAddress && (m.RegionSize & (PG - 1)) == 0);
    EXPECT_EQ(m.Protect, PAGE_NOACCESS);
    t_note("free-region AllocationProtect=0x%lx", m.AllocationProtect);
    memset(&m, 0xEE, sizeof m);
    EXPECT_EQ(VQ((void *)0x1234, &m, sizeof m), sizeof m);
    EXPECT_EQ(m.BaseAddress, 0x1000);       /* free region: BaseAddress is the queried address rounded down to a page */
    EXPECT_EQ(m.State, MEM_FREE);
}
TEST(mv_vq_free_region_fields) {
    REQUIRE(VQ);
    char *h = mv_hole(GR);
    CHECK(h != NULL);
    MEMORY_BASIC_INFORMATION m = mv_q(h);
    EXPECT_EQ(m.State, MEM_FREE);
    EXPECT_EQ(m.AllocationBase, 0);
    EXPECT_EQ(m.Protect, PAGE_NOACCESS);
    CHECK(m.RegionSize >= GR);
    EXPECT_EQ(m.BaseAddress, h);
}
TEST(mv_vq_base_address_rounds_down) {
    REQUIRE(VQ);
    char *p = mv_alloc(2 * PG);
    for (unsigned off = 0; off < 2 * PG; off += 0x333) {
        MEMORY_BASIC_INFORMATION m = mv_q(p + off);
        char *pg = p + (off & ~(PG - 1));
        CHECKF(m.BaseAddress == pg, "off 0x%x base %p", off, m.BaseAddress);
        CHECK(m.RegionSize == 2 * PG - (pg - p));
        CHECK(m.AllocationBase == p);
    }
    mv_free(p);
}
TEST(mv_vq_image_headers_and_sections) {
    REQUIRE(VQ);
    HMODULE exe = GetModuleHandleA(NULL);
    MEMORY_BASIC_INFORMATION m = mv_q(exe);
    EXPECT_EQ(m.BaseAddress, exe);
    EXPECT_EQ(m.AllocationBase, exe);
    EXPECT_EQ(m.Type, MEM_IMAGE);
    EXPECT_EQ(m.State, MEM_COMMIT);
    EXPECT_EQ(m.Protect, PAGE_READONLY);
    EXPECT_EQ(m.AllocationProtect, PAGE_EXECUTE_WRITECOPY);
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)exe;
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)((char *)exe + dos->e_lfanew);
    IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
    int checked = 0;
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        char *va = (char *)exe + sec[i].VirtualAddress;
        m = mv_q(va);
        EXPECT_EQ(m.AllocationBase, exe);
        EXPECT_EQ(m.Type, MEM_IMAGE);
        EXPECT_EQ(m.State, MEM_COMMIT);
        DWORD ch = sec[i].Characteristics;
        if ((ch & IMAGE_SCN_MEM_EXECUTE) && !(ch & IMAGE_SCN_MEM_WRITE)) { EXPECT_EQ(m.Protect, PAGE_EXECUTE_READ); checked++; }
        else if (!(ch & IMAGE_SCN_MEM_WRITE) && (ch & IMAGE_SCN_MEM_READ)) { CHECKF(m.Protect == PAGE_READONLY, "sec %.8s prot 0x%lx", sec[i].Name, m.Protect); checked++; }
        else if (ch & IMAGE_SCN_MEM_WRITE) { CHECKF(m.Protect == PAGE_READWRITE || m.Protect == PAGE_WRITECOPY, "sec %.8s prot 0x%lx", sec[i].Name, m.Protect); checked++; }
    }
    CHECK(checked > 0);
    /* module in system directory */
    HMODULE k = GetModuleHandleA("kernel32.dll");
    m = mv_q(k);
    EXPECT_EQ(m.Type, MEM_IMAGE); EXPECT_EQ(m.AllocationBase, k);
}
TEST(mv_vq_stack_layout) {
    REQUIRE(VQ);
    volatile char local = 1;
    NT_TIB *tib = (NT_TIB *)NtCurrentTeb();
    char *dealloc = *(char **)((char *)tib + 0x1478);
    MEMORY_BASIC_INFORMATION m = mv_q((void *)&local);
    EXPECT_EQ(m.State, MEM_COMMIT);
    EXPECT_EQ(m.Type, MEM_PRIVATE);
    EXPECT_EQ(m.Protect, PAGE_READWRITE);
    EXPECT_EQ(m.AllocationBase, dealloc);
    CHECK((char *)m.BaseAddress + m.RegionSize == (char *)tib->StackBase);
    CHECK((char *)m.BaseAddress <= (char *)&local);
    if (m.BaseAddress != tib->StackLimit) t_note("committed stack base %p != StackLimit %p", m.BaseAddress, tib->StackLimit);
    /* the page just below StackLimit is the stack guard page (or first reserve page) */
    MEMORY_BASIC_INFORMATION g = mv_q((char *)tib->StackLimit - 1);
    EXPECT_EQ(g.AllocationBase, dealloc);
    CHECK(((g.Protect & PAGE_GUARD) && g.State == MEM_COMMIT) || g.State == MEM_RESERVE);
    /* bottom of allocation: reserved */
    MEMORY_BASIC_INFORMATION b = mv_q(dealloc);
    EXPECT_EQ(b.AllocationBase, dealloc);
    EXPECT_EQ(b.State, MEM_RESERVE);
}
TEST(mv_vq_heap_block) {
    REQUIRE(VQ);
    void *h = HeapAlloc(GetProcessHeap(), 0, 100);
    CHECK(h != NULL);
    MEMORY_BASIC_INFORMATION m = mv_q(h);
    EXPECT_EQ(m.State, MEM_COMMIT); EXPECT_EQ(m.Type, MEM_PRIVATE);
    EXPECT_EQ(m.Protect, PAGE_READWRITE);
    CHECK(m.AllocationBase != NULL);
    HeapFree(GetProcessHeap(), 0, h);
}
TEST(mv_vq_exe_image_vs_dll_image) {
    REQUIRE(VQ);
    char buf[MAX_PATH];
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    MEMORY_BASIC_INFORMATION a = mv_q(ntdll), b = mv_q((char *)ntdll + 0x1000);
    EXPECT_EQ(a.AllocationBase, ntdll); EXPECT_EQ(b.AllocationBase, ntdll);
    EXPECT_EQ(b.Type, MEM_IMAGE);
    CHECK(GetModuleFileNameA(ntdll, buf, sizeof buf) > 0);
}
TEST(mv_vq_global_data_and_code) {
    REQUIRE(VQ);
    static int gdata[4] = {1, 2, 3, 4};
    MEMORY_BASIC_INFORMATION m = mv_q(gdata);
    EXPECT_EQ(m.Type, MEM_IMAGE);
    CHECK(m.Protect == PAGE_READWRITE || m.Protect == PAGE_WRITECOPY);
    m = mv_q((void *)&mv_q);
    EXPECT_EQ(m.Type, MEM_IMAGE);
    CHECK(m.Protect == PAGE_EXECUTE_READ || m.Protect == PAGE_EXECUTE_WRITECOPY || m.Protect == PAGE_EXECUTE_READWRITE);
}
TEST(mv_vq_all_regions_walk) {
    REQUIRE(VQ);
    SYSTEM_INFO si = mv_sysinfo();
    char *a = (char *)si.lpMinimumApplicationAddress;
    char *end = (char *)si.lpMaximumApplicationAddress;
    int n = 0, comm = 0;
    char *prev_end = a;
    while (a < end && n < 100000) {
        MEMORY_BASIC_INFORMATION m;
        if (VQ(a, &m, sizeof m) != sizeof m) { CHECKF(0, "query failed at %p err %lu", a, GetLastError()); break; }
        CHECKF(m.BaseAddress == a, "walk base %p vs %p", m.BaseAddress, a);
        CHECK(m.RegionSize > 0 && (m.RegionSize & (PG - 1)) == 0);
        if (m.State == MEM_COMMIT) comm++;
        if (m.State != MEM_FREE) CHECK(m.AllocationBase != NULL && (char *)m.AllocationBase <= a);
        if (m.State == MEM_FREE) CHECK(m.AllocationBase == NULL);
        CHECK(m.State == MEM_COMMIT || m.State == MEM_RESERVE || m.State == MEM_FREE);
        a += m.RegionSize; n++;
    }
    CHECK(n > 10 && comm > 5);
    t_note("walked %d regions", n);
}
TEST(mv_vq_max_address_boundary) {
    REQUIRE(VQ);
    SYSTEM_INFO si = mv_sysinfo();
    MEMORY_BASIC_INFORMATION m;
    SET_ERR(0);
    SIZE_T r = VQ(si.lpMaximumApplicationAddress, &m, sizeof m);
    EXPECT_EQ(r, sizeof m);
    EXPECT_EQ(m.State, MEM_FREE);
    SET_ERR(0);
    r = VQ((char *)si.lpMaximumApplicationAddress + 1, &m, sizeof m);
    CHECK(r == 0);
    ERR1(ERROR_INVALID_PARAMETER);
}

/* ---------------- guard pages and access violations ---------------- */
TEST(mv_guard_one_shot_read) {
    REQUIRE(VALLOC); REQUIRE(VQ);
    char *p = VALLOC(NULL, 2 * PG, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE | PAGE_GUARD);
    CHECK(p != NULL);
    if (!p) return;
    EXPECT_EQ(mv_q(p).Protect, PAGE_READWRITE | PAGE_GUARD);
    { DWORD c_ = mv_rd(p, NULL); EXPECT_EQ(c_, STATUS_GUARD_PAGE_VIOLATION); }
    EXPECT_EQ(mv_info1, (ULONG_PTR)p);
    EXPECT_EQ(mv_q(p).Protect, PAGE_READWRITE);              /* guard bit cleared */
    EXPECT_EQ(mv_rd(p, NULL), 0);
    EXPECT_EQ(mv_wr(p, 1), 0);
    mv_free(p);
}
TEST(mv_guard_one_shot_write) {
    REQUIRE(VALLOC);
    char *p = VALLOC(NULL, PG, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE | PAGE_GUARD);
    CHECK(p != NULL);
    if (!p) return;
    { DWORD c_ = mv_wr(p + 5, 3); EXPECT_EQ(c_, STATUS_GUARD_PAGE_VIOLATION); }
    EXPECT_EQ(mv_info0, 1);
    EXPECT_EQ(mv_wr(p + 5, 3), 0);
    EXPECT_EQ(p[5], 3);
    mv_free(p);
}
TEST(mv_guard_via_virtualprotect) {
    REQUIRE(VPROT);
    char *p = mv_alloc(2 * PG);
    DWORD old;
    CHECK(VPROT(p, PG, PAGE_READONLY | PAGE_GUARD, &old));
    EXPECT_EQ(old, PAGE_READWRITE);
    EXPECT_EQ(mv_q(p).Protect, PAGE_READONLY | PAGE_GUARD);
    { DWORD c_ = mv_rd(p + 1, NULL); EXPECT_EQ(c_, STATUS_GUARD_PAGE_VIOLATION); }
    EXPECT_EQ(mv_q(p).Protect, PAGE_READONLY);
    /* the next page is unaffected */
    EXPECT_EQ(mv_wr(p + PG, 2), 0);
    mv_free(p);
}
TEST(mv_guard_only_touched_page_triggers) {
    REQUIRE(VPROT);
    char *p = mv_alloc(3 * PG);
    DWORD old;
    CHECK(VPROT(p, 3 * PG, PAGE_READWRITE | PAGE_GUARD, &old));
    { DWORD c_ = mv_wr(p + PG, 1); EXPECT_EQ(c_, STATUS_GUARD_PAGE_VIOLATION); }
    MEMORY_BASIC_INFORMATION m = mv_q(p);
    EXPECT_EQ(m.Protect, PAGE_READWRITE | PAGE_GUARD); EXPECT_EQ(m.RegionSize, PG);
    m = mv_q(p + PG);
    EXPECT_EQ(m.Protect, PAGE_READWRITE);
    m = mv_q(p + 2 * PG);
    EXPECT_EQ(m.Protect, PAGE_READWRITE | PAGE_GUARD);
    mv_free(p);
}
TEST(mv_guard_with_noaccess_invalid) {
    REQUIRE(VPROT);
    char *p = mv_alloc(PG);
    DWORD old;
    SET_ERR(0);
    CHECK(!VPROT(p, PG, PAGE_NOACCESS | PAGE_GUARD, &old));
    ERR1(ERROR_INVALID_PARAMETER);
    mv_free(p);
}
TEST(mv_prot_noaccess_faults) {
    REQUIRE(VPROT);
    char *p = mv_alloc(PG);
    DWORD old;
    p[0] = 1;
    CHECK(VPROT(p, PG, PAGE_NOACCESS, &old));
    EXPECT_EQ(mv_rd(p, NULL), EXCEPTION_ACCESS_VIOLATION); EXPECT_EQ(mv_info0, 0); EXPECT_EQ(mv_info1, (ULONG_PTR)p);
    EXPECT_EQ(mv_wr(p, 2), EXCEPTION_ACCESS_VIOLATION);     EXPECT_EQ(mv_info0, 1);
    CHECK(VPROT(p, PG, PAGE_READWRITE, &old));
    EXPECT_EQ(old, PAGE_NOACCESS);
    EXPECT_EQ(p[0], 1);                    /* contents preserved across NOACCESS */
    mv_free(p);
}
TEST(mv_prot_readonly_write_faults) {
    REQUIRE(VPROT);
    char *p = mv_alloc(PG);
    DWORD old;
    p[3] = 9;
    CHECK(VPROT(p, PG, PAGE_READONLY, &old));
    unsigned v = 0;
    EXPECT_EQ(mv_rd(p + 3, &v), 0); EXPECT_EQ(v, 9);
    EXPECT_EQ(mv_wr(p + 3, 1), EXCEPTION_ACCESS_VIOLATION); EXPECT_EQ(mv_info0, 1);
    EXPECT_EQ(p[3], 9);
    mv_free(p);
}
TEST(mv_prot_execute_stub) {
    REQUIRE(VPROT); REQUIRE(U(FlushInstructionCache));
    char *p = mv_alloc(PG);
    memcpy(p, mv_stub, sizeof mv_stub);
    DWORD old;
    CHECK(VPROT(p, PG, PAGE_EXECUTE_READ, &old));
    EXPECT_EQ(old, PAGE_READWRITE);
    SET_ERR(0);
    CHECK(U(FlushInstructionCache)(GetCurrentProcess(), p, sizeof mv_stub));
    int r = 0;
    EXPECT_EQ(mv_exec(p, &r), 0);
    EXPECT_EQ(r, 0x1234);
    EXPECT_EQ(mv_wr(p, 0), EXCEPTION_ACCESS_VIOLATION);
    mv_free(p);
}
TEST(mv_prot_execute_readwrite_direct) {
    REQUIRE(VALLOC);
    char *p = VALLOC(NULL, PG, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    CHECK(p != NULL);
    if (!p) return;
    memcpy(p, mv_stub, sizeof mv_stub);
    int r = 0;
    EXPECT_EQ(mv_exec(p, &r), 0);
    EXPECT_EQ(r, 0x1234);
    mv_free(p);
}
TEST(mv_prot_execute_only_page) {
    REQUIRE(VALLOC);
    char *p = VALLOC(NULL, PG, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    CHECK(p != NULL);
    if (!p) return;
    memcpy(p, mv_stub, sizeof mv_stub);
    DWORD old;
    CHECK(VPROT(p, PG, PAGE_EXECUTE, &old));
    int r = 0;
    EXPECT_EQ(mv_exec(p, &r), 0);
    EXPECT_EQ(r, 0x1234);
    EXPECT_EQ(mv_wr(p, 0), EXCEPTION_ACCESS_VIOLATION);
    DWORD rc = mv_rd(p, NULL);
    t_note("read of PAGE_EXECUTE page: %s", rc ? "faults" : "succeeds");
    mv_free(p);
}
TEST(mv_dep_nonexec_faults) {
    REQUIRE(VALLOC);
    DWORD prots[] = { PAGE_READWRITE, PAGE_READONLY, PAGE_NOACCESS };
    for (int i = 0; i < 3; i++) {
        char *p = VALLOC(NULL, PG, MEM_RESERVE | MEM_COMMIT, prots[i]);
        CHECK(p != NULL);
        if (!p) continue;
        if (prots[i] == PAGE_READWRITE) memcpy(p, mv_stub, sizeof mv_stub);
        int r = 0;
        DWORD c = mv_exec(p, &r);
        if (t_is_wine() && c == 0) { t_note("NX not enforced here"); }
        else { CHECKF(c == EXCEPTION_ACCESS_VIOLATION, "prot 0x%lx code 0x%lx", prots[i], c); EXPECT_EQ(mv_info0, 8); }
        mv_free(p);
    }
}
TEST(mv_dep_stack_and_heap_nonexec) {
    REQUIRE(VALLOC);
    unsigned char buf[16];
    memcpy(buf, mv_stub, sizeof mv_stub);
    int r = 0;
    DWORD c = mv_exec(buf, &r);
    if (t_is_wine() && c == 0) { t_note("NX not enforced here"); return; }
    EXPECT_EQ(c, EXCEPTION_ACCESS_VIOLATION); EXPECT_EQ(mv_info0, 8);
    void *h = HeapAlloc(GetProcessHeap(), 0, 16);
    memcpy(h, mv_stub, sizeof mv_stub);
    c = mv_exec(h, &r);
    EXPECT_EQ(c, EXCEPTION_ACCESS_VIOLATION); EXPECT_EQ(mv_info0, 8);
    HeapFree(GetProcessHeap(), 0, h);
}
TEST(mv_fic_basic) {
    REQUIRE(U(FlushInstructionCache));
    char *p = mv_alloc(PG);
    SET_ERR(0);
    CHECK(U(FlushInstructionCache)(GetCurrentProcess(), p, PG));
    CHECK(U(FlushInstructionCache)(GetCurrentProcess(), NULL, 0));
    CHECK(U(FlushInstructionCache)(GetCurrentProcess(), p, 0));
    mv_free(p);
}
TEST(mv_fic_handles) {
    REQUIRE(U(FlushInstructionCache));
    /* On native x64 the call only does work for WoW64 targets; it never validates the handle for others. */
    char *p = mv_alloc(PG);
    HANDLE hs[] = { NULL, (HANDLE)(ULONG_PTR)0xFFFC, GetCurrentProcess() };
    for (int i = 0; i < 3; i++) {
        SET_ERR(0);
        BOOL r = U(FlushInstructionCache)(hs[i], p, PG);
        if (!r) t_note("handle %p rejected err=%lu", hs[i], GetLastError());
        else CHECK(r);
    }
    HANDLE dup = NULL;
    if (DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(), &dup, SYNCHRONIZE, FALSE, 0)) {
        SET_ERR(0);
        BOOL r = U(FlushInstructionCache)(dup, p, PG);
        t_note("FlushInstructionCache with SYNCHRONIZE-only self handle: %d", r);
        CloseHandle(dup);
    }
    mv_free(p);
}

/* ---------------- VirtualProtectEx / QueryEx with handle rights ---------------- */
static HANDLE dup_self(DWORD access) {
    HANDLE h = NULL;
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(), &h, access, FALSE, 0)) return NULL;
    return h;
}
TEST(mv_vpex_self_pseudo_and_real_handle) {
    REQUIRE(VPROTEX); REQUIRE(VQEX);
    char *p = mv_alloc(PG);
    DWORD old = 0;
    CHECK(VPROTEX(GetCurrentProcess(), p, PG, PAGE_READONLY, &old));
    EXPECT_EQ(old, PAGE_READWRITE);
    HANDLE h = dup_self(PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_QUERY_INFORMATION);
    CHECK(h != NULL);
    if (h) {
        CHECK(VPROTEX(h, p, PG, PAGE_READWRITE, &old));
        EXPECT_EQ(old, PAGE_READONLY);
        MEMORY_BASIC_INFORMATION m;
        SET_ERR(0);
        EXPECT_EQ(VQEX(h, p, &m, sizeof m), sizeof m);
        EXPECT_EQ(m.Protect, PAGE_READWRITE);
        CloseHandle(h);
    }
    mv_free(p);
}
TEST(mv_vpex_access_rights) {
    REQUIRE(VPROTEX);
    char *p = mv_alloc(PG);
    DWORD old = 0;
    HANDLE h = dup_self(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION);   /* no VM_OPERATION */
    CHECK(h != NULL);
    SET_ERR(0);
    CHECK(!VPROTEX(h, p, PG, PAGE_READONLY, &old));
    ERR1(ERROR_ACCESS_DENIED);
    CloseHandle(h);
    mv_free(p);
}
TEST(mv_vpex_bad_handles) {
    REQUIRE(VPROTEX); REQUIRE(VQEX);
    char *p = mv_alloc(PG);
    DWORD old = 0;
    HANDLE bad[] = { NULL, (HANDLE)(ULONG_PTR)0x7777, (HANDLE)(ULONG_PTR)0xFFFC };
    for (int i = 0; i < 3; i++) {
        SET_ERR(0);
        CHECKF(!VPROTEX(bad[i], p, PG, PAGE_READONLY, &old), "h=%p", bad[i]);
        ERR1(ERROR_INVALID_HANDLE);
        MEMORY_BASIC_INFORMATION m;
        SET_ERR(0);
        CHECKF(VQEX(bad[i], p, &m, sizeof m) == 0, "h=%p", bad[i]);
        ERR1(ERROR_INVALID_HANDLE);
    }
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    CHECK(!VPROTEX(ev, p, PG, PAGE_READONLY, &old));
    ERR1(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
    mv_free(p);
}
TEST(mv_vqex_requires_query_rights) {
    REQUIRE(VQEX);
    char *p = mv_alloc(PG);
    MEMORY_BASIC_INFORMATION m;
    HANDLE h = dup_self(SYNCHRONIZE);
    SET_ERR(0);
    CHECK(VQEX(h, p, &m, sizeof m) == 0);
    ERR1(ERROR_ACCESS_DENIED);
    CloseHandle(h);
    h = dup_self(PROCESS_QUERY_LIMITED_INFORMATION);
    SET_ERR(0);
    SIZE_T r = VQEX(h, p, &m, sizeof m);
    t_note("VirtualQueryEx with QUERY_LIMITED_INFORMATION: r=%Iu err=%lu", r, GetLastError());
    CloseHandle(h);
    h = dup_self(PROCESS_QUERY_INFORMATION);
    SET_ERR(0);
    EXPECT_EQ(VQEX(h, p, &m, sizeof m), sizeof m);
    CloseHandle(h);
    h = dup_self(PROCESS_VM_READ);
    SET_ERR(0);
    r = VQEX(h, p, &m, sizeof m);
    t_note("VirtualQueryEx with VM_READ only: r=%Iu err=%lu", r, GetLastError());
    CloseHandle(h);
    mv_free(p);
}
TEST(mv_vq_vs_vqex_same) {
    REQUIRE(VQ); REQUIRE(VQEX);
    char *p = mv_alloc(3 * PG);
    MEMORY_BASIC_INFORMATION a, b;
    EXPECT_EQ(VQ(p + 10, &a, sizeof a), sizeof a);
    EXPECT_EQ(VQEX(GetCurrentProcess(), p + 10, &b, sizeof b), sizeof b);
    CHECK(!memcmp(&a, &b, sizeof a) || a.BaseAddress == b.BaseAddress);
    EXPECT_EQ(a.RegionSize, b.RegionSize);
    mv_free(p);
}
