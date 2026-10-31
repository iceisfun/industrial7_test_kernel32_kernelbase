#include "g16.h"

static g16_VirtualAlloc2_t VA2(void) { return (g16_VirtualAlloc2_t)t_proc_kb("VirtualAlloc2"); }
CHILD(gap16_sleeper) { Sleep(25000); return 0; }
static HANDLE spawn_sleeper(PROCESS_INFORMATION *pi) { return t_spawn_child("gap16_sleeper", "", 0, NULL, NULL, NULL, FALSE, NULL, pi); }
static void kill_sleeper(HANDLE h, PROCESS_INFORMATION *pi) { TerminateProcess(h, 0); WaitForSingleObject(h, 5000); CloseHandle(h); CloseHandle(pi->hThread); }
static HANDLE dup_access(HANDLE h, DWORD acc) { HANDLE d = NULL; if (!DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(), &d, acc, FALSE, 0)) return NULL; return d; }
static void *page_alloc(DWORD prot) { return VirtualAlloc(NULL, 4096, MEM_RESERVE | MEM_COMMIT, prot); }
static HANDLE closed_handle(void) { HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL); CloseHandle(e); return e; }
#define SELF GetCurrentProcess()
#define NEED_Q REQUIRE(UK(VirtualQueryEx))
#define NEED_F REQUIRE(UK(VirtualFreeEx))
#define NEED_P REQUIRE(UK(VirtualProtectEx))
#define NEED_W REQUIRE(UK(WriteProcessMemory))
#define NEED_A REQUIRE(VA2())

/* ================================================================ VirtualQueryEx */
TEST(gap16_vqex_committed_page) {
    NEED_Q; MEMORY_BASIC_INFORMATION m; void *p = page_alloc(PAGE_READWRITE); REQUIRE(p);
    memset(&m, 0xcc, sizeof m);
    SIZE_T r = UK(VirtualQueryEx)(SELF, p, &m, sizeof m);
    EXPECT_EQ(r, sizeof(MEMORY_BASIC_INFORMATION));
    EXPECT_EQ(m.BaseAddress, p); EXPECT_EQ(m.AllocationBase, p);
    EXPECT_EQ(m.AllocationProtect, PAGE_READWRITE); EXPECT_EQ(m.Protect, PAGE_READWRITE);
    EXPECT_EQ(m.State, MEM_COMMIT); EXPECT_EQ(m.Type, MEM_PRIVATE); EXPECT_EQ(m.RegionSize, 4096);
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap16_vqex_offset_inside_page) {
    NEED_Q; MEMORY_BASIC_INFORMATION m; char *p = page_alloc(PAGE_READWRITE); REQUIRE(p);
    static const int off[] = { 1, 7, 100, 2048, 4095 };
    for (int i = 0; i < 5; i++) {
        memset(&m, 0, sizeof m);
        CHECK(UK(VirtualQueryEx)(SELF, p + off[i], &m, sizeof m) == sizeof m);
        CHECKF(m.BaseAddress == p, "off %d base %p", off[i], m.BaseAddress);
        CHECK(m.RegionSize == 4096);
    }
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap16_vqex_pseudo_and_invalid_handle_value_are_self) {
    NEED_Q; MEMORY_BASIC_INFORMATION a, b; int local = 0; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    CHECK(UK(VirtualQueryEx)(SELF, &local, &a, sizeof a) == sizeof a);
    CHECK(UK(VirtualQueryEx)(INVALID_HANDLE_VALUE, &local, &b, sizeof b) == sizeof b);
    CHECK(memcmp(&a, &b, sizeof a) == 0);
    EXPECT_EQ(a.State, MEM_COMMIT); EXPECT_EQ(a.Type, MEM_PRIVATE);
}
TEST(gap16_vqex_null_address_is_free) {
    NEED_Q; MEMORY_BASIC_INFORMATION m; memset(&m, 0xcc, sizeof m);
    SIZE_T r = UK(VirtualQueryEx)(SELF, NULL, &m, sizeof m);
    EXPECT_EQ(r, sizeof m);
    EXPECT_EQ(m.State, MEM_FREE); EXPECT_EQ(m.BaseAddress, NULL);
    EXPECT_EQ(m.AllocationProtect, 0); EXPECT_EQ(m.Protect, PAGE_NOACCESS); EXPECT_EQ(m.Type, 0);
    t_note("null region size 0x%llx", (unsigned long long)m.RegionSize);
}
TEST(gap16_vqex_image_header) {
    NEED_Q; MEMORY_BASIC_INFORMATION m; HMODULE exe = GetModuleHandleA(NULL); memset(&m, 0, sizeof m);
    CHECK(UK(VirtualQueryEx)(SELF, exe, &m, sizeof m) == sizeof m);
    EXPECT_EQ(m.Type, MEM_IMAGE); EXPECT_EQ(m.State, MEM_COMMIT);
    EXPECT_EQ(m.AllocationBase, exe); EXPECT_EQ(m.BaseAddress, exe);
    EXPECT_EQ(m.Protect, PAGE_READONLY);
    EXPECT_EQ(m.AllocationProtect & 0xff, PAGE_EXECUTE_WRITECOPY);
}
TEST(gap16_vqex_image_ntdll_sections) {
    NEED_Q; MEMORY_BASIC_INFORMATION m; HMODULE nt = t_ntdll(); IMAGE_DOS_HEADER *dos = (void *)nt;
    IMAGE_NT_HEADERS *nth = (void *)((char *)nt + dos->e_lfanew); IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(nth);
    int seenx = 0, seenw = 0;
    for (int i = 0; i < nth->FileHeader.NumberOfSections; i++) {
        char *a = (char *)nt + s[i].VirtualAddress; memset(&m, 0, sizeof m);
        CHECK(UK(VirtualQueryEx)(SELF, a, &m, sizeof m) == sizeof m);
        CHECK(m.AllocationBase == (void *)nt); CHECK(m.Type == MEM_IMAGE);
        if (s[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) { seenx++; CHECKF(m.Protect == PAGE_EXECUTE_READ, "prot 0x%lx", m.Protect); }
        else if (!(s[i].Characteristics & IMAGE_SCN_MEM_WRITE)) CHECKF(m.Protect == PAGE_READONLY, "ro sec %d prot 0x%lx", i, m.Protect);
    }
    CHECK(seenx >= 1);
}
TEST(gap16_vqex_length_table) {
    NEED_Q; char buf[8192]; void *p = page_alloc(PAGE_READWRITE); REQUIRE(p);
    static const SIZE_T bad[] = { 0, 1, 4, 8, 16, 32, 40, 47 };
    static const SIZE_T good[] = { 48, 49, 56, 64, 4096 };
    for (int i = 0; i < 8; i++) {
        memset(buf, 0x5a, sizeof buf); SET_ERR(0);
        SIZE_T r = UK(VirtualQueryEx)(SELF, p, (PMEMORY_BASIC_INFORMATION)buf, bad[i]);
        CHECKF(r == 0, "len %zu r %zu", bad[i], r); EXPECT_ERR(ERROR_BAD_LENGTH);
        CHECKF((unsigned char)buf[0] == 0x5a, "buffer touched len %zu", bad[i]);
    }
    for (int i = 0; i < 5; i++) {
        SIZE_T r = UK(VirtualQueryEx)(SELF, p, (PMEMORY_BASIC_INFORMATION)buf, good[i]);
        CHECKF(r == sizeof(MEMORY_BASIC_INFORMATION), "len %zu r %zu", good[i], r);
    }
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap16_vqex_high_addresses) {
    NEED_Q; MEMORY_BASIC_INFORMATION m;
    static const ULONG_PTR a[] = { 0x7FFFFFFF0000ull, 0x7FFFFFFFFFFFull, 0x800000000000ull, 0xFFFF800000000000ull, 0xFFFFFFFFFFFFFFFFull, 0x7FFFFFFF1000ull };
    for (int i = 0; i < 6; i++) {
        SET_ERR(0); SIZE_T r = UK(VirtualQueryEx)(SELF, (void *)a[i], &m, sizeof m);
        CHECKF(r == 0, "addr %p r %zu", (void *)a[i], r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(gap16_vqex_last_user_page) {
    NEED_Q; MEMORY_BASIC_INFORMATION m; memset(&m, 0, sizeof m);
    SIZE_T r = UK(VirtualQueryEx)(SELF, (void *)0x7FFFFFFEF000ull, &m, sizeof m);
    CHECK(r == sizeof m); if (r) EXPECT_EQ(m.State, MEM_FREE);
}
TEST(gap16_vqex_bad_handles) {
    NEED_Q; MEMORY_BASIC_INFORMATION m; int x;
    SET_ERR(0); CHECK(UK(VirtualQueryEx)(NULL, &x, &m, sizeof m) == 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(UK(VirtualQueryEx)(closed_handle(), &x, &m, sizeof m) == 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0); CHECK(UK(VirtualQueryEx)(e, &x, &m, sizeof m) == 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(e);
    SET_ERR(0); CHECK(UK(VirtualQueryEx)((HANDLE)(LONG_PTR)-2, &x, &m, sizeof m) == 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE t = GetCurrentThread();
    SET_ERR(0); CHECK(UK(VirtualQueryEx)(t, &x, &m, sizeof m) == 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap16_vqex_null_buffer_fails) {
    NEED_Q; int x; SET_ERR(0);
    SIZE_T r = UK(VirtualQueryEx)(SELF, &x, NULL, sizeof(MEMORY_BASIC_INFORMATION));
    CHECK(r == 0); t_note("null buffer err %lu", GetLastError());
}
TEST(gap16_vqex_reserve_commit_layout) {
    NEED_Q; MEMORY_BASIC_INFORMATION m; char *p = VirtualAlloc(NULL, 0x10000, MEM_RESERVE, PAGE_READWRITE); REQUIRE(p);
    CHECK(VirtualAlloc(p + 4096, 4096, MEM_COMMIT, PAGE_READWRITE) != NULL);
    memset(&m, 0, sizeof m); CHECK(UK(VirtualQueryEx)(SELF, p, &m, sizeof m) == sizeof m);
    EXPECT_EQ(m.State, MEM_RESERVE); EXPECT_EQ(m.RegionSize, 4096); EXPECT_EQ(m.Protect, 0); EXPECT_EQ(m.AllocationProtect, PAGE_READWRITE); EXPECT_EQ(m.Type, MEM_PRIVATE);
    memset(&m, 0, sizeof m); CHECK(UK(VirtualQueryEx)(SELF, p + 4096, &m, sizeof m) == sizeof m);
    EXPECT_EQ(m.State, MEM_COMMIT); EXPECT_EQ(m.RegionSize, 4096); EXPECT_EQ(m.Protect, PAGE_READWRITE); EXPECT_EQ(m.AllocationBase, p);
    memset(&m, 0, sizeof m); CHECK(UK(VirtualQueryEx)(SELF, p + 8192, &m, sizeof m) == sizeof m);
    EXPECT_EQ(m.State, MEM_RESERVE); EXPECT_EQ(m.RegionSize, 0x10000 - 8192); EXPECT_EQ(m.BaseAddress, p + 8192);
    memset(&m, 0, sizeof m); CHECK(UK(VirtualQueryEx)(SELF, p + 0x10000, &m, sizeof m) == sizeof m);
    CHECK(m.AllocationBase != (void *)p);
    VirtualFree(p, 0, MEM_RELEASE);
    memset(&m, 0, sizeof m); CHECK(UK(VirtualQueryEx)(SELF, p, &m, sizeof m) == sizeof m);
    EXPECT_EQ(m.State, MEM_FREE);
}
TEST(gap16_vqex_guard_and_noaccess) {
    NEED_Q; MEMORY_BASIC_INFORMATION m; char *p = page_alloc(PAGE_READWRITE | PAGE_GUARD); REQUIRE(p);
    memset(&m, 0, sizeof m); CHECK(UK(VirtualQueryEx)(SELF, p, &m, sizeof m) == sizeof m);
    EXPECT_EQ(m.Protect, PAGE_READWRITE | PAGE_GUARD); EXPECT_EQ(m.AllocationProtect, PAGE_READWRITE | PAGE_GUARD);
    VirtualFree(p, 0, MEM_RELEASE);
    p = page_alloc(PAGE_NOACCESS); REQUIRE(p);
    memset(&m, 0, sizeof m); CHECK(UK(VirtualQueryEx)(SELF, p, &m, sizeof m) == sizeof m);
    EXPECT_EQ(m.Protect, PAGE_NOACCESS); EXPECT_EQ(m.State, MEM_COMMIT);
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap16_vqex_mapped_view) {
    NEED_Q; MEMORY_BASIC_INFORMATION m;
    HANDLE fm = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 8192, NULL); REQUIRE(fm);
    void *v = MapViewOfFile(fm, FILE_MAP_WRITE, 0, 0, 0);
    if (v) {
        memset(&m, 0, sizeof m); CHECK(UK(VirtualQueryEx)(SELF, v, &m, sizeof m) == sizeof m);
        EXPECT_EQ(m.Type, MEM_MAPPED); EXPECT_EQ(m.State, MEM_COMMIT); EXPECT_EQ(m.RegionSize, 8192); EXPECT_EQ(m.Protect, PAGE_READWRITE);
        UnmapViewOfFile(v);
    } else CHECK(0);
    CloseHandle(fm);
}
TEST(gap16_vqex_walk_address_space) {
    NEED_Q; MEMORY_BASIC_INFORMATION m; ULONG_PTR cur = 0; int n = 0, bad = 0, sawfree = 0, sawimg = 0, sawpriv = 0;
    for (; n < 100000; n++) {
        memset(&m, 0, sizeof m);
        if (UK(VirtualQueryEx)(SELF, (void *)cur, &m, sizeof m) != sizeof m) break;
        if ((ULONG_PTR)m.BaseAddress != cur || m.RegionSize == 0 || (m.RegionSize & 0xfff)) { bad++; break; }
        if (m.State == MEM_FREE) sawfree++; else if (m.Type == MEM_IMAGE) sawimg++; else if (m.Type == MEM_PRIVATE) sawpriv++;
        ULONG_PTR nx = cur + m.RegionSize; if (nx <= cur) break; cur = nx;
    }
    CHECKF(!bad, "region inconsistency at %p", (void *)cur); CHECK(n > 10); CHECK(sawfree && sawimg && sawpriv);
    CHECKF(cur >= 0x7FFFFFFF0000ull, "walk ended at %p", (void *)cur);
}
TEST(gap16_vqex_remote_process) {
    NEED_Q; PROCESS_INFORMATION pi; HANDLE h = spawn_sleeper(&pi); REQUIRE(h);
    MEMORY_BASIC_INFORMATION m; memset(&m, 0, sizeof m);
    CHECK(UK(VirtualQueryEx)(h, t_ntdll(), &m, sizeof m) == sizeof m);
    EXPECT_EQ(m.AllocationBase, t_ntdll()); EXPECT_EQ(m.Type, MEM_IMAGE); EXPECT_EQ(m.State, MEM_COMMIT);
    memset(&m, 0, sizeof m); CHECK(UK(VirtualQueryEx)(h, NULL, &m, sizeof m) == sizeof m); EXPECT_EQ(m.State, MEM_FREE);
    SET_ERR(0); CHECK(UK(VirtualQueryEx)(h, (void *)0x7FFFFFFF0000ull, &m, sizeof m) == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(UK(VirtualQueryEx)(h, NULL, &m, 8) == 0); EXPECT_ERR(ERROR_BAD_LENGTH);
    kill_sleeper(h, &pi);
}
TEST(gap16_vqex_access_rights) {
    NEED_Q; PROCESS_INFORMATION pi; HANDLE h = spawn_sleeper(&pi); REQUIRE(h);
    MEMORY_BASIC_INFORMATION m; HANDLE lim = dup_access(h, PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE); REQUIRE(lim);
    SET_ERR(0); CHECK(UK(VirtualQueryEx)(lim, t_ntdll(), &m, sizeof m) == 0); EXPECT_ERR(ERROR_ACCESS_DENIED);
    HANDLE rd = dup_access(h, PROCESS_VM_READ); REQUIRE(rd);
    SET_ERR(0); CHECK(UK(VirtualQueryEx)(rd, t_ntdll(), &m, sizeof m) == 0); EXPECT_ERR(ERROR_ACCESS_DENIED);
    HANDLE qi = dup_access(h, PROCESS_QUERY_INFORMATION); REQUIRE(qi);
    CHECK(UK(VirtualQueryEx)(qi, t_ntdll(), &m, sizeof m) == sizeof m);
    CloseHandle(lim); CloseHandle(rd); CloseHandle(qi); kill_sleeper(h, &pi);
}
TEST(gap16_vqex_terminated_process) {
    NEED_Q; PROCESS_INFORMATION pi; HANDLE h = spawn_sleeper(&pi); REQUIRE(h);
    HANDLE keep = dup_access(h, PROCESS_ALL_ACCESS); REQUIRE(keep);
    kill_sleeper(h, &pi);
    MEMORY_BASIC_INFORMATION m; SET_ERR(0);
    CHECK(UK(VirtualQueryEx)(keep, t_ntdll(), &m, sizeof m) == 0); t_note("terminated process query err %lu", GetLastError());
    CloseHandle(keep);
}
TEST(gap16_vqex_repeat_stable) {
    NEED_Q; MEMORY_BASIC_INFORMATION a, b; void *p = page_alloc(PAGE_READWRITE); REQUIRE(p); memset(&a, 0, sizeof a);
    CHECK(UK(VirtualQueryEx)(SELF, p, &a, sizeof a) == sizeof a);
    for (int i = 0; i < 1000; i++) { memset(&b, 0, sizeof b); CHECK(UK(VirtualQueryEx)(SELF, p, &b, sizeof b) == sizeof b);
        if (a.BaseAddress != b.BaseAddress || a.AllocationBase != b.AllocationBase || a.RegionSize != b.RegionSize || a.State != b.State || a.Protect != b.Protect || a.Type != b.Type) { CHECK(0); break; } }
    VirtualFree(p, 0, MEM_RELEASE);
}

/* ================================================================ VirtualFreeEx */
TEST(gap16_vfex_release_self) {
    NEED_F; MEMORY_BASIC_INFORMATION m; void *p = page_alloc(PAGE_READWRITE); REQUIRE(p);
    SET_ERR(0); CHECK(UK(VirtualFreeEx)(SELF, p, 0, MEM_RELEASE) == TRUE);
    memset(&m, 0, sizeof m); VirtualQuery(p, &m, sizeof m); EXPECT_EQ(m.State, MEM_FREE);
}
TEST(gap16_vfex_release_via_invalid_handle_value) {
    NEED_F; void *p = page_alloc(PAGE_READWRITE); REQUIRE(p);
    CHECK(UK(VirtualFreeEx)(INVALID_HANDLE_VALUE, p, 0, MEM_RELEASE) == TRUE);
}
TEST(gap16_vfex_bad_flags) {
    NEED_F; char *p = page_alloc(PAGE_READWRITE); REQUIRE(p);
    static const DWORD hard[] = { 0, 0xC000, 0x8004, 0x10000, 0x40000000, 0x20000, 0x1000, 0x2000, 0x80000000u, 0x8008 };
    for (int i = 0; i < 10; i++) {
        SET_ERR(0); BOOL r = UK(VirtualFreeEx)(SELF, p, 0, hard[i]);
        CHECKF(!r, "flags 0x%lx succeeded", hard[i]);
        if (!r) EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
    static const DWORD soft[] = { 1, 2, 3 };
    for (int i = 0; i < 3; i++) { SET_ERR(0); BOOL r = UK(VirtualFreeEx)(SELF, p, 0, soft[i]); CHECKF(!r, "flags 0x%lx", soft[i]); t_note("flags 0x%lx err %lu", soft[i], GetLastError()); }
    MEMORY_BASIC_INFORMATION m; VirtualQuery(p, &m, sizeof m); EXPECT_EQ(m.State, MEM_COMMIT);
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap16_vfex_release_with_size) {
    NEED_F; char *p = page_alloc(PAGE_READWRITE); REQUIRE(p);
    static const SIZE_T sz[] = { 1, 4096, 0x10000 };
    for (int i = 0; i < 3; i++) { SET_ERR(0); CHECK(!UK(VirtualFreeEx)(SELF, p, sz[i], MEM_RELEASE)); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
    MEMORY_BASIC_INFORMATION m; VirtualQuery(p, &m, sizeof m); EXPECT_EQ(m.State, MEM_COMMIT);
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap16_vfex_double_release) {
    NEED_F; void *p = page_alloc(PAGE_READWRITE); REQUIRE(p);
    CHECK(UK(VirtualFreeEx)(SELF, p, 0, MEM_RELEASE));
    SET_ERR(0); CHECK(!UK(VirtualFreeEx)(SELF, p, 0, MEM_RELEASE)); EXPECT_ERR(ERROR_INVALID_ADDRESS);
}
TEST(gap16_vfex_release_not_at_base) {
    NEED_F; char *p = VirtualAlloc(NULL, 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE); REQUIRE(p);
    SET_ERR(0); CHECK(!UK(VirtualFreeEx)(SELF, p + 4096, 0, MEM_RELEASE)); EXPECT_ERR2(ERROR_INVALID_PARAMETER, ERROR_INVALID_ADDRESS);
    SET_ERR(0); BOOL r = UK(VirtualFreeEx)(SELF, p + 1, 0, MEM_RELEASE);   /* rounded down to the page, which is the allocation base */
    t_note("release at base+1 -> %d err %lu", r, GetLastError());
    if (!r) CHECK(UK(VirtualFreeEx)(SELF, p, 0, MEM_RELEASE));
}
TEST(gap16_vfex_release_null_address) {
    NEED_F; SET_ERR(0); CHECK(!UK(VirtualFreeEx)(SELF, NULL, 0, MEM_RELEASE)); EXPECT_ERR2(ERROR_INVALID_PARAMETER, ERROR_INVALID_ADDRESS);
    SET_ERR(0); CHECK(!UK(VirtualFreeEx)(SELF, NULL, 4096, MEM_DECOMMIT)); t_note("decommit null err %lu", GetLastError());
}
TEST(gap16_vfex_release_never_allocated) {
    NEED_F; SET_ERR(0); CHECK(!UK(VirtualFreeEx)(SELF, (void *)0x10000000000ull, 0, MEM_RELEASE)); t_note("err %lu", GetLastError());
    SET_ERR(0); CHECK(!UK(VirtualFreeEx)(SELF, (void *)0x7FFFFFFF0000ull, 0, MEM_RELEASE)); t_note("high err %lu", GetLastError());
}
TEST(gap16_vfex_decommit) {
    NEED_F; MEMORY_BASIC_INFORMATION m; char *p = VirtualAlloc(NULL, 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE); REQUIRE(p);
    p[0] = 1; p[4096] = 2;
    CHECK(UK(VirtualFreeEx)(SELF, p + 4096, 4096, MEM_DECOMMIT));
    VirtualQuery(p + 4096, &m, sizeof m); EXPECT_EQ(m.State, MEM_RESERVE);
    VirtualQuery(p, &m, sizeof m); EXPECT_EQ(m.State, MEM_COMMIT); EXPECT_EQ(m.RegionSize, 4096);
    CHECK(UK(VirtualFreeEx)(SELF, p + 4096, 4096, MEM_DECOMMIT));   /* idempotent */
    CHECK(UK(VirtualFreeEx)(SELF, p + 1, 1, MEM_DECOMMIT));          /* rounds out to the page */
    VirtualQuery(p, &m, sizeof m); EXPECT_EQ(m.State, MEM_RESERVE);
    VirtualQuery(p, &m, sizeof m); EXPECT_EQ(m.AllocationBase, p);
    CHECK(UK(VirtualFreeEx)(SELF, p, 0, MEM_RELEASE));
}
TEST(gap16_vfex_decommit_then_recommit_zeroed) {
    NEED_F; char *p = VirtualAlloc(NULL, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE); REQUIRE(p);
    memset(p, 0xab, 4096);
    CHECK(UK(VirtualFreeEx)(SELF, p, 4096, MEM_DECOMMIT));
    CHECK(VirtualAlloc(p, 4096, MEM_COMMIT, PAGE_READWRITE) == p);
    int z = 1; for (int i = 0; i < 4096; i++) if (p[i]) z = 0; CHECK(z);
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap16_vfex_decommit_whole_allocation_keeps_reservation) {
    NEED_F; MEMORY_BASIC_INFORMATION m; char *p = VirtualAlloc(NULL, 3 * 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE); REQUIRE(p);
    CHECK(UK(VirtualFreeEx)(SELF, p, 3 * 4096, MEM_DECOMMIT));
    VirtualQuery(p, &m, sizeof m); EXPECT_EQ(m.State, MEM_RESERVE); EXPECT_EQ(m.AllocationBase, p);
    CHECK(UK(VirtualFreeEx)(SELF, p, 0, MEM_RELEASE));
    VirtualQuery(p, &m, sizeof m); EXPECT_EQ(m.State, MEM_FREE);
}
TEST(gap16_vfex_bad_handles) {
    NEED_F; char *p = page_alloc(PAGE_READWRITE); REQUIRE(p);
    SET_ERR(0); CHECK(!UK(VirtualFreeEx)(NULL, p, 0, MEM_RELEASE)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!UK(VirtualFreeEx)(closed_handle(), p, 0, MEM_RELEASE)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0); CHECK(!UK(VirtualFreeEx)(e, p, 0, MEM_RELEASE)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(e);
    SET_ERR(0); CHECK(!UK(VirtualFreeEx)((HANDLE)(LONG_PTR)-2, p, 0, MEM_RELEASE)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    MEMORY_BASIC_INFORMATION m; VirtualQuery(p, &m, sizeof m); EXPECT_EQ(m.State, MEM_COMMIT);
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap16_vfex_mapped_view_not_freeable) {
    NEED_F; HANDLE fm = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, NULL); REQUIRE(fm);
    void *v = MapViewOfFile(fm, FILE_MAP_WRITE, 0, 0, 0); REQUIRE(v);
    SET_ERR(0); BOOL r = UK(VirtualFreeEx)(SELF, v, 0, MEM_RELEASE); CHECK(!r); t_note("err %lu", GetLastError());
    if (r) { CloseHandle(fm); return; }
    CHECK(UnmapViewOfFile(v)); CloseHandle(fm);
}
TEST(gap16_vfex_remote_lifecycle) {
    NEED_F; NEED_A; PROCESS_INFORMATION pi; HANDLE h = spawn_sleeper(&pi); REQUIRE(h);
    MEMORY_BASIC_INFORMATION m;
    void *p = VA2()(h, NULL, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE, NULL, 0); CHECK(p != NULL);
    if (p) {
        memset(&m, 0, sizeof m); CHECK(VirtualQueryEx(h, p, &m, sizeof m) == sizeof m); EXPECT_EQ(m.State, MEM_COMMIT);
        CHECK(UK(VirtualFreeEx)(h, p, 4096 * 2, MEM_DECOMMIT));
        CHECK(VirtualQueryEx(h, p, &m, sizeof m) == sizeof m); EXPECT_EQ(m.State, MEM_RESERVE);
        SET_ERR(0); CHECK(!UK(VirtualFreeEx)(h, p, 4096, MEM_RELEASE)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
        CHECK(UK(VirtualFreeEx)(h, p, 0, MEM_RELEASE));
        CHECK(VirtualQueryEx(h, p, &m, sizeof m) == sizeof m); EXPECT_EQ(m.State, MEM_FREE);
        SET_ERR(0); CHECK(!UK(VirtualFreeEx)(h, p, 0, MEM_RELEASE)); EXPECT_ERR(ERROR_INVALID_ADDRESS);
    }
    kill_sleeper(h, &pi);
}
TEST(gap16_vfex_access_rights) {
    NEED_F; NEED_A; PROCESS_INFORMATION pi; HANDLE h = spawn_sleeper(&pi); REQUIRE(h);
    void *p = VA2()(h, NULL, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE, NULL, 0); REQUIRE(p);
    HANDLE q = dup_access(h, PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE); REQUIRE(q);
    SET_ERR(0); CHECK(!UK(VirtualFreeEx)(q, p, 0, MEM_RELEASE)); EXPECT_ERR(ERROR_ACCESS_DENIED);
    HANDLE ok = dup_access(h, PROCESS_VM_OPERATION); REQUIRE(ok);
    CHECK(UK(VirtualFreeEx)(ok, p, 0, MEM_RELEASE));
    CloseHandle(q); CloseHandle(ok); kill_sleeper(h, &pi);
}
TEST(gap16_vfex_placeholder_split_coalesce) {
    NEED_F; NEED_A; MEMORY_BASIC_INFORMATION m;
    char *p = VA2()(NULL, NULL, 0x20000, MEM_RESERVE | 0x40000 /*MEM_RESERVE_PLACEHOLDER*/, PAGE_NOACCESS, NULL, 0);
    if (!p) { t_skip("placeholders unsupported err %lu", GetLastError()); return; }
    memset(&m, 0, sizeof m); VirtualQuery(p, &m, sizeof m); EXPECT_EQ(m.State, MEM_RESERVE); EXPECT_EQ(m.RegionSize, 0x20000);
    SET_ERR(0); BOOL r = UK(VirtualFreeEx)(SELF, p, 0x10000, MEM_RELEASE | 0x2);
    CHECKF(r, "split err %lu", GetLastError());
    if (r) {
        VirtualQuery(p, &m, sizeof m); EXPECT_EQ(m.RegionSize, 0x10000);
        VirtualQuery(p + 0x10000, &m, sizeof m); EXPECT_EQ(m.RegionSize, 0x10000); EXPECT_EQ(m.BaseAddress, p + 0x10000);
        SET_ERR(0); r = UK(VirtualFreeEx)(SELF, p, 0x20000, MEM_RELEASE | 0x1);
        CHECKF(r, "coalesce err %lu", GetLastError());
        if (r) { VirtualQuery(p, &m, sizeof m); EXPECT_EQ(m.RegionSize, 0x20000); }
    }
    CHECK(UK(VirtualFreeEx)(SELF, p, 0, MEM_RELEASE));
}
TEST(gap16_vfex_placeholder_split_unaligned_fails) {
    NEED_F; NEED_A;
    char *p = VA2()(NULL, NULL, 0x20000, MEM_RESERVE | 0x40000, PAGE_NOACCESS, NULL, 0);
    if (!p) { t_skip("placeholders unsupported"); return; }
    SET_ERR(0); BOOL r1 = UK(VirtualFreeEx)(SELF, p, 0x20000, MEM_RELEASE | 0x2); CHECK(!r1);   /* splitting off the whole placeholder is not a split */
    t_note("whole-size split err %lu", GetLastError());
    SET_ERR(0); BOOL r2 = UK(VirtualFreeEx)(SELF, p, 0x1000, MEM_RELEASE | 0x2); t_note("page-sized split -> %d err %lu", r2, GetLastError());
    CHECK(UK(VirtualFreeEx)(SELF, p, 0, MEM_RELEASE));
}

/* ================================================================ VirtualProtectEx */
TEST(gap16_vpex_basic_transitions) {
    NEED_P; MEMORY_BASIC_INFORMATION m; char *p = page_alloc(PAGE_READWRITE); REQUIRE(p); DWORD old = 0xdead;
    static const DWORD seq[] = { PAGE_READONLY, PAGE_NOACCESS, PAGE_READWRITE, PAGE_EXECUTE_READ, PAGE_EXECUTE_READWRITE, PAGE_EXECUTE, PAGE_READWRITE };
    DWORD prev = PAGE_READWRITE;
    for (int i = 0; i < 7; i++) {
        old = 0xdead; SET_ERR(0);
        BOOL r = UK(VirtualProtectEx)(SELF, p, 4096, seq[i], &old);
        if (!r && (seq[i] == PAGE_WRITECOPY || seq[i] == PAGE_EXECUTE_READWRITE)) { t_note("prot 0x%lx unsupported err %lu", seq[i], GetLastError()); continue; }
        CHECKF(r, "prot 0x%lx err %lu", seq[i], GetLastError());
        CHECKF(old == prev, "old 0x%lx want 0x%lx", old, prev);
        VirtualQuery(p, &m, sizeof m); CHECKF(m.Protect == seq[i], "prot 0x%lx got 0x%lx", seq[i], m.Protect);
        prev = seq[i];
    }
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap16_vpex_invalid_protection_values) {
    NEED_P; char *p = page_alloc(PAGE_READWRITE); REQUIRE(p); DWORD old;
    static const DWORD bad[] = { 0, 6, PAGE_GUARD, PAGE_NOCACHE, PAGE_WRITECOMBINE, PAGE_READONLY | PAGE_READWRITE, 0xFFFFFFFFu, 0x3, 0x5, PAGE_NOACCESS | PAGE_EXECUTE };
    for (int i = 0; i < (int)(sizeof bad / sizeof bad[0]); i++) {
        old = 0xdead; SET_ERR(0);
        BOOL r = UK(VirtualProtectEx)(SELF, p, 4096, bad[i], &old);
        CHECKF(!r, "prot 0x%lx succeeded", bad[i]);
        if (!r) EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
    MEMORY_BASIC_INFORMATION m; VirtualQuery(p, &m, sizeof m); EXPECT_EQ(m.Protect, PAGE_READWRITE);
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap16_vpex_size_zero_and_rounding) {
    NEED_P; MEMORY_BASIC_INFORMATION m; char *p = VirtualAlloc(NULL, 3 * 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE); REQUIRE(p); DWORD old;
    SET_ERR(0); CHECK(!UK(VirtualProtectEx)(SELF, p, 0, PAGE_READONLY, &old)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CHECK(UK(VirtualProtectEx)(SELF, p + 4096 + 100, 1, PAGE_READONLY, &old));
    VirtualQuery(p, &m, sizeof m); EXPECT_EQ(m.Protect, PAGE_READWRITE); EXPECT_EQ(m.RegionSize, 4096);
    VirtualQuery(p + 4096, &m, sizeof m); EXPECT_EQ(m.Protect, PAGE_READONLY); EXPECT_EQ(m.RegionSize, 4096);
    CHECK(UK(VirtualProtectEx)(SELF, p + 4095, 2, PAGE_NOACCESS, &old));      /* straddles pages 0 and 1 */
    VirtualQuery(p, &m, sizeof m); EXPECT_EQ(m.Protect, PAGE_NOACCESS); EXPECT_EQ(m.RegionSize, 8192);
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap16_vpex_null_old_protect) {
    NEED_P; MEMORY_BASIC_INFORMATION m; char *p = page_alloc(PAGE_READWRITE); REQUIRE(p);
    SET_ERR(0); CHECK(!UK(VirtualProtectEx)(SELF, p, 4096, PAGE_READONLY, NULL)); EXPECT_ERR(ERROR_NOACCESS);
    VirtualQuery(p, &m, sizeof m); EXPECT_EQ(m.Protect, PAGE_READWRITE);
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap16_vpex_unallocated_and_crossing) {
    NEED_P; DWORD old; char *p = page_alloc(PAGE_READWRITE); REQUIRE(p);
    SET_ERR(0); CHECK(!UK(VirtualProtectEx)(SELF, NULL, 4096, PAGE_READONLY, &old)); EXPECT_ERR2(ERROR_INVALID_ADDRESS, ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!UK(VirtualProtectEx)(SELF, (void *)0x7FFFFFFF0000ull, 4096, PAGE_READONLY, &old)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    /* committed page followed by reserved-only space in the same allocation */
    SET_ERR(0); BOOL r = UK(VirtualProtectEx)(SELF, p, 8192, PAGE_READONLY, &old); CHECK(!r); t_note("cross into reserved err %lu", GetLastError());
    UK(VirtualProtectEx)(SELF, p, 4096, PAGE_READWRITE, &old);
    SET_ERR(0); CHECK(!UK(VirtualProtectEx)(SELF, p + 4096, 4096, PAGE_READONLY, &old)); t_note("reserved-only err %lu", GetLastError());
    SET_ERR(0); CHECK(!UK(VirtualProtectEx)(SELF, p, (SIZE_T)-1, PAGE_READONLY, &old)); t_note("huge size err %lu", GetLastError());
    VirtualFree(p, 0, MEM_RELEASE);
    SET_ERR(0); CHECK(!UK(VirtualProtectEx)(SELF, p, 4096, PAGE_READONLY, &old)); EXPECT_ERR2(ERROR_INVALID_ADDRESS, ERROR_INVALID_PARAMETER);
}
TEST(gap16_vpex_guard_page_semantics) {
    NEED_P; MEMORY_BASIC_INFORMATION m; volatile char *p = page_alloc(PAGE_READWRITE); REQUIRE(p); DWORD old;
    CHECK(UK(VirtualProtectEx)(SELF, (void *)p, 4096, PAGE_READWRITE | PAGE_GUARD, &old)); EXPECT_EQ(old, PAGE_READWRITE);
    VirtualQuery((void *)p, &m, sizeof m); EXPECT_EQ(m.Protect, PAGE_READWRITE | PAGE_GUARD);
    CHECK(UK(VirtualProtectEx)(SELF, (void *)p, 4096, PAGE_READWRITE, &old)); EXPECT_EQ(old, PAGE_READWRITE | PAGE_GUARD);
    p[0] = 7; CHECK(p[0] == 7);
    VirtualFree((void *)p, 0, MEM_RELEASE);
}
TEST(gap16_vpex_old_protect_preserved_data) {
    NEED_P; char *p = page_alloc(PAGE_READWRITE); REQUIRE(p); DWORD old;
    memset(p, 0x3c, 4096);
    CHECK(UK(VirtualProtectEx)(SELF, p, 4096, PAGE_READONLY, &old));
    int ok = 1; for (int i = 0; i < 4096; i++) if (p[i] != 0x3c) ok = 0; CHECK(ok);
    CHECK(UK(VirtualProtectEx)(SELF, p, 4096, PAGE_NOACCESS, &old)); CHECK(UK(VirtualProtectEx)(SELF, p, 4096, PAGE_READWRITE, &old));
    ok = 1; for (int i = 0; i < 4096; i++) if (p[i] != 0x3c) ok = 0; CHECK(ok);
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap16_vpex_image_page_readonly_to_same) {
    NEED_P; MEMORY_BASIC_INFORMATION m; char *exe = (char *)GetModuleHandleA(NULL); DWORD old = 0;
    /* header page is PAGE_READONLY; setting it to the same value is a no-op that reports it */
    CHECK(UK(VirtualProtectEx)(SELF, exe, 4096, PAGE_READONLY, &old)); EXPECT_EQ(old, PAGE_READONLY);
    VirtualQuery(exe, &m, sizeof m); EXPECT_EQ(m.Protect, PAGE_READONLY);
}
TEST(gap16_vpex_bad_handles) {
    NEED_P; char *p = page_alloc(PAGE_READWRITE); REQUIRE(p); DWORD old;
    SET_ERR(0); CHECK(!UK(VirtualProtectEx)(NULL, p, 4096, PAGE_READONLY, &old)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!UK(VirtualProtectEx)(closed_handle(), p, 4096, PAGE_READONLY, &old)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0); CHECK(!UK(VirtualProtectEx)(e, p, 4096, PAGE_READONLY, &old)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(e);
    CHECK(UK(VirtualProtectEx)(INVALID_HANDLE_VALUE, p, 4096, PAGE_READONLY, &old));
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap16_vpex_remote_and_rights) {
    NEED_P; NEED_A; PROCESS_INFORMATION pi; HANDLE h = spawn_sleeper(&pi); REQUIRE(h);
    MEMORY_BASIC_INFORMATION m; DWORD old = 0;
    char *p = VA2()(h, NULL, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE, NULL, 0); REQUIRE(p);
    CHECK(UK(VirtualProtectEx)(h, p, 4096, PAGE_READONLY, &old)); EXPECT_EQ(old, PAGE_READWRITE);
    VirtualQueryEx(h, p, &m, sizeof m); EXPECT_EQ(m.Protect, PAGE_READONLY);
    HANDLE q = dup_access(h, PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE); REQUIRE(q);
    SET_ERR(0); CHECK(!UK(VirtualProtectEx)(q, p, 4096, PAGE_READWRITE, &old)); EXPECT_ERR(ERROR_ACCESS_DENIED);
    VirtualQueryEx(h, p, &m, sizeof m); EXPECT_EQ(m.Protect, PAGE_READONLY);
    SET_ERR(0); CHECK(!UK(VirtualProtectEx)(h, p, 4096, PAGE_READWRITE, NULL)); EXPECT_ERR(ERROR_NOACCESS);
    HANDLE ok = dup_access(h, PROCESS_VM_OPERATION); REQUIRE(ok);
    CHECK(UK(VirtualProtectEx)(ok, p, 4096, PAGE_READWRITE, &old)); EXPECT_EQ(old, PAGE_READONLY);
    CloseHandle(q); CloseHandle(ok); kill_sleeper(h, &pi);
}

/* ================================================================ WriteProcessMemory */
TEST(gap16_wpm_self_basic) {
    NEED_W; char *p = page_alloc(PAGE_READWRITE); REQUIRE(p); SIZE_T w = 0xdead; char src[16]; memcpy(src, "0123456789abcdef", 16);
    SET_ERR(0); CHECK(UK(WriteProcessMemory)(SELF, p + 3, src, 16, &w)); EXPECT_EQ(w, 16);
    CHECK(memcmp(p + 3, src, 16) == 0); CHECK(p[2] == 0 && p[19] == 0);
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap16_wpm_null_written_ptr) {
    NEED_W; char *p = page_alloc(PAGE_READWRITE); REQUIRE(p); char src[4] = { 1, 2, 3, 4 };
    CHECK(UK(WriteProcessMemory)(SELF, p, src, 4, NULL)); CHECK(p[3] == 4);
    CHECK(UK(WriteProcessMemory)(INVALID_HANDLE_VALUE, p, src, 1, NULL));
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap16_wpm_size_table) {
    NEED_W; char *p = page_alloc(PAGE_READWRITE); REQUIRE(p); static char src[4096];
    static const SIZE_T sz[] = { 1, 2, 3, 7, 8, 15, 16, 255, 256, 1000, 4095, 4096 };
    for (int i = 0; i < 12; i++) {
        memset(p, 0, 4096); memset(src, 0x40 + i, sizeof src); SIZE_T w = 0;
        BOOL r = UK(WriteProcessMemory)(SELF, p, src, sz[i], &w);
        CHECKF(r && w == sz[i], "size %zu r %d w %zu", sz[i], r, w);
        int ok = 1; for (SIZE_T k = 0; k < 4096; k++) if (p[k] != (k < sz[i] ? 0x40 + i : 0)) ok = 0; CHECKF(ok, "content size %zu", sz[i]);
    }
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap16_wpm_zero_size) {
    NEED_W; char *p = page_alloc(PAGE_READWRITE); REQUIRE(p); SIZE_T w = 0xdead; char src = 5; SET_ERR(0);
    BOOL r = UK(WriteProcessMemory)(SELF, p, &src, 0, &w); t_note("size 0: r=%d w=%zu err=%lu", r, w, GetLastError());
    CHECK(p[0] == 0); VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap16_wpm_readonly_target_fails) {
    NEED_W; char *p = page_alloc(PAGE_READONLY); REQUIRE(p); SIZE_T w = 0xdead; char src[8] = "abcdefg"; SET_ERR(0);
    BOOL r = UK(WriteProcessMemory)(SELF, p, src, 8, &w);
    CHECK(!r); EXPECT_ERR2(ERROR_NOACCESS, ERROR_PARTIAL_COPY); CHECK(*(volatile char *)p == 0);
    t_note("readonly: w=%zu", w);
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap16_wpm_noaccess_target_fails) {
    NEED_W; char *p = page_alloc(PAGE_NOACCESS); REQUIRE(p); SIZE_T w = 0xdead; char src[8] = "abcdefg"; SET_ERR(0);
    CHECK(!UK(WriteProcessMemory)(SELF, p, src, 8, &w)); EXPECT_ERR2(ERROR_NOACCESS, ERROR_PARTIAL_COPY); t_note("noaccess w=%zu", w);
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap16_wpm_execute_read_target_writable_and_restored) {
    NEED_W; MEMORY_BASIC_INFORMATION m; char *p = page_alloc(PAGE_EXECUTE_READ); REQUIRE(p); SIZE_T w = 0; char src[4] = { 9, 8, 7, 6 };
    SET_ERR(0); BOOL r = UK(WriteProcessMemory)(SELF, p, src, 4, &w);
    CHECKF(r && w == 4, "r %d w %zu err %lu", r, w, GetLastError());
    VirtualQuery(p, &m, sizeof m); EXPECT_EQ(m.Protect, PAGE_EXECUTE_READ);
    CHECK(p[0] == 9 && p[3] == 6);
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap16_wpm_unmapped_target) {
    NEED_W; char *p = page_alloc(PAGE_READWRITE); REQUIRE(p); char src[8] = { 1 }; SIZE_T w = 0xdead;
    VirtualFree(p, 0, MEM_RELEASE);
    SET_ERR(0); CHECK(!UK(WriteProcessMemory)(SELF, p, src, 8, &w)); EXPECT_ERR2(ERROR_NOACCESS, ERROR_PARTIAL_COPY); t_note("freed w=%zu", w);
    SET_ERR(0); CHECK(!UK(WriteProcessMemory)(SELF, NULL, src, 8, &w)); EXPECT_ERR2(ERROR_NOACCESS, ERROR_PARTIAL_COPY);
}
TEST(gap16_wpm_partial_copy_across_boundary) {
    NEED_W; char *p = VirtualAlloc(NULL, 0x10000, MEM_RESERVE, PAGE_READWRITE); REQUIRE(p);
    CHECK(VirtualAlloc(p, 4096, MEM_COMMIT, PAGE_READWRITE) == (void *)p);
    static char src[8192]; memset(src, 0x77, sizeof src); SIZE_T w = 0xdead; SET_ERR(0);
    BOOL r = UK(WriteProcessMemory)(SELF, p, src, 8192, &w);
    CHECK(!r); EXPECT_ERR2(ERROR_PARTIAL_COPY, ERROR_NOACCESS);
    t_note("partial copy w=%zu first=%02x", w, (unsigned char)p[0]);
    /* a write ending exactly at the boundary succeeds */
    w = 0; CHECK(UK(WriteProcessMemory)(SELF, p + 4096 - 8, src, 8, &w)); EXPECT_EQ(w, 8);
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap16_wpm_huge_size_fails) {
    NEED_W; char *p = page_alloc(PAGE_READWRITE); REQUIRE(p); char src[8] = { 1 }; SIZE_T w = 0xdead;
    static const SIZE_T sz[] = { (SIZE_T)1 << 47, (SIZE_T)-1 };
    for (int i = 0; i < 2; i++) { SET_ERR(0); CHECK(!UK(WriteProcessMemory)(SELF, p, src, sz[i], &w)); t_note("huge %zx err %lu w=%zu", sz[i], GetLastError(), w); }
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap16_wpm_bad_handles) {
    NEED_W; char *p = page_alloc(PAGE_READWRITE); REQUIRE(p); char src[4] = { 1 }; SIZE_T w;
    SET_ERR(0); CHECK(!UK(WriteProcessMemory)(NULL, p, src, 4, &w)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!UK(WriteProcessMemory)(closed_handle(), p, src, 4, &w)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0); CHECK(!UK(WriteProcessMemory)(e, p, src, 4, &w)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(e); CHECK(p[0] == 0);
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap16_wpm_remote_roundtrip) {
    NEED_W; NEED_A; PROCESS_INFORMATION pi; HANDLE h = spawn_sleeper(&pi); REQUIRE(h);
    char *p = VA2()(h, NULL, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE, NULL, 0); REQUIRE(p);
    char src[64], dst[64]; for (int i = 0; i < 64; i++) src[i] = (char)(i * 3 + 1); SIZE_T w = 0, rd = 0;
    CHECK(UK(WriteProcessMemory)(h, p + 100, src, 64, &w)); EXPECT_EQ(w, 64);
    memset(dst, 0, 64); CHECK(ReadProcessMemory(h, p + 100, dst, 64, &rd)); EXPECT_EQ(rd, 64); CHECK(!memcmp(src, dst, 64));
    /* boundary exactly at end of page */
    CHECK(UK(WriteProcessMemory)(h, p + 4096 - 4, src, 4, &w)); EXPECT_EQ(w, 4);
    SET_ERR(0); w = 0xdead; CHECK(!UK(WriteProcessMemory)(h, p + 4096 - 4, src, 8, &w)); EXPECT_ERR2(ERROR_PARTIAL_COPY, ERROR_NOACCESS);
    kill_sleeper(h, &pi);
}
TEST(gap16_wpm_remote_readonly_and_exec) {
    NEED_W; NEED_A; PROCESS_INFORMATION pi; HANDLE h = spawn_sleeper(&pi); REQUIRE(h); MEMORY_BASIC_INFORMATION m;
    char *p = VA2()(h, NULL, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READONLY, NULL, 0); REQUIRE(p); SIZE_T w = 0; char src[4] = { 1, 2, 3, 4 };
    SET_ERR(0); CHECK(!UK(WriteProcessMemory)(h, p, src, 4, &w)); EXPECT_ERR2(ERROR_NOACCESS, ERROR_PARTIAL_COPY);
    DWORD old; VirtualProtectEx(h, p, 4096, PAGE_EXECUTE_READ, &old);
    SET_ERR(0); CHECK(UK(WriteProcessMemory)(h, p, src, 4, &w)); EXPECT_EQ(w, 4);
    VirtualQueryEx(h, p, &m, sizeof m); EXPECT_EQ(m.Protect, PAGE_EXECUTE_READ);
    char dst[4] = { 0 }; ReadProcessMemory(h, p, dst, 4, NULL); CHECK(!memcmp(dst, src, 4));
    kill_sleeper(h, &pi);
}
TEST(gap16_wpm_access_rights) {
    NEED_W; NEED_A; PROCESS_INFORMATION pi; HANDLE h = spawn_sleeper(&pi); REQUIRE(h);
    char *p = VA2()(h, NULL, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE, NULL, 0); REQUIRE(p); char src[4] = { 1, 2, 3, 4 }; SIZE_T w = 0xdead;
    HANDLE r1 = dup_access(h, PROCESS_VM_READ | PROCESS_QUERY_INFORMATION), r2 = dup_access(h, PROCESS_VM_OPERATION), r3 = dup_access(h, PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE);
    REQUIRE(r1 && r2 && r3);
    SET_ERR(0); CHECK(!UK(WriteProcessMemory)(r1, p, src, 4, &w)); EXPECT_ERR(ERROR_ACCESS_DENIED);
    SET_ERR(0); CHECK(!UK(WriteProcessMemory)(r2, p, src, 4, &w)); EXPECT_ERR(ERROR_ACCESS_DENIED);
    SET_ERR(0); CHECK(!UK(WriteProcessMemory)(r3, p, src, 4, &w)); EXPECT_ERR(ERROR_ACCESS_DENIED);
    char dst[4] = { 0 }; ReadProcessMemory(h, p, dst, 4, NULL); CHECK(dst[0] == 0);
    HANDLE r4 = dup_access(h, PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION); REQUIRE(r4);
    SET_ERR(0); BOOL r = UK(WriteProcessMemory)(r4, p, src, 4, &w); t_note("VM_WRITE|QUERY only: r=%d err=%lu", r, GetLastError());
    CloseHandle(r1); CloseHandle(r2); CloseHandle(r3); CloseHandle(r4); kill_sleeper(h, &pi);
}
TEST(gap16_wpm_terminated_process) {
    NEED_W; NEED_A; PROCESS_INFORMATION pi; HANDLE h = spawn_sleeper(&pi); REQUIRE(h);
    char *p = VA2()(h, NULL, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE, NULL, 0); REQUIRE(p);
    HANDLE keep = dup_access(h, PROCESS_ALL_ACCESS); REQUIRE(keep); kill_sleeper(h, &pi);
    char src[4] = { 1 }; SIZE_T w = 0xdead; SET_ERR(0);
    CHECK(!UK(WriteProcessMemory)(keep, p, src, 4, &w)); t_note("terminated err %lu", GetLastError());
    CloseHandle(keep);
}
TEST(gap16_wpm_repeat_many) {
    NEED_W; char *p = page_alloc(PAGE_READWRITE); REQUIRE(p); SIZE_T w; int ok = 1;
    for (int i = 0; i < 2000; i++) { char b = (char)i; if (!UK(WriteProcessMemory)(SELF, p + (i & 0xfff), &b, 1, &w) || w != 1 || p[i & 0xfff] != b) ok = 0; }
    CHECK(ok); VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap16_wpm_cross_thread_visibility) {
    NEED_W; volatile char *p = page_alloc(PAGE_READWRITE); REQUIRE(p); SIZE_T w; char v = 0x5a;
    CHECK(UK(WriteProcessMemory)(SELF, (void *)(p + 64), &v, 1, &w)); MemoryBarrier(); CHECK(p[64] == 0x5a);
    VirtualFree((void *)p, 0, MEM_RELEASE);
}
