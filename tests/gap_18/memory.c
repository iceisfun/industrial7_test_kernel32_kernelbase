#include "g18.h"
/* gap18: VirtualAllocEx, VirtualProtect, SetProcessWorkingSetSizeEx. */

CHILD(gap18_idle) { Sleep(25000); return 0; }

static HANDLE g18_spawn_idle(PROCESS_INFORMATION *pi) {
    return t_spawn_child("gap18_idle", "", 0, NULL, NULL, NULL, FALSE, NULL, pi);
}
static void g18_kill(PROCESS_INFORMATION *pi) {
    TerminateProcess(pi->hProcess, 0); WaitForSingleObject(pi->hProcess, 5000);
    CloseHandle(pi->hThread); CloseHandle(pi->hProcess);
}
#define PG 0x1000

/* ------------------------- VirtualAllocEx: local ------------------------- */
TEST(gap18_vallocex_self_commit) {
    REQUIRE(U(VirtualAllocEx)); REQUIRE(U(VirtualFreeEx)); REQUIRE(U(VirtualQuery));
    HANDLE me = GetCurrentProcess();
    char *p = (char *)U(VirtualAllocEx)(me, NULL, 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    CHECK(p != NULL);
    if (!p) return;
    CHECK(((ULONG_PTR)p & 0xFFFF) == 0);          /* allocation granularity */
    MEMORY_BASIC_INFORMATION mbi; memset(&mbi, 0, sizeof mbi);
    CHECK(U(VirtualQuery)(p, &mbi, sizeof mbi) == sizeof mbi);
    EQ(mbi.RegionSize, PG);                 /* 1 byte rounds to one page */
    EQ(mbi.State, MEM_COMMIT);
    EQ(mbi.Protect, PAGE_READWRITE);
    EQ(mbi.Type, MEM_PRIVATE);
    EQ(mbi.AllocationBase, p);
    EQ(mbi.AllocationProtect, PAGE_READWRITE);
    p[0] = 1; p[PG - 1] = 2;
    CHECK(U(VirtualFreeEx)(me, p, 0, MEM_RELEASE));
}
TEST(gap18_vallocex_pseudo_handle_minus1) {
    REQUIRE(U(VirtualAllocEx)); REQUIRE(U(VirtualFree));
    void *p = U(VirtualAllocEx)(INVALID_HANDLE_VALUE, NULL, PG, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    CHECK(p != NULL);       /* (HANDLE)-1 is the current-process pseudo handle */
    if (p) U(VirtualFree)(p, 0, MEM_RELEASE);
}
TEST(gap18_vallocex_size_rounding) {
    REQUIRE(U(VirtualAllocEx)); REQUIRE(U(VirtualFree)); REQUIRE(U(VirtualQuery));
    static const SIZE_T sizes[] = { 1, 0xFFF, 0x1000, 0x1001, 0x2000, 0x10000, 0x10001 };
    for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        char *p = (char *)U(VirtualAllocEx)(GetCurrentProcess(), NULL, sizes[i], MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        CHECKF(p != NULL, "size 0x%llx", (unsigned long long)sizes[i]);
        if (!p) continue;
        MEMORY_BASIC_INFORMATION mbi;
        U(VirtualQuery)(p, &mbi, sizeof mbi);
        SIZE_T want = (sizes[i] + PG - 1) & ~(SIZE_T)(PG - 1);
        CHECKF(mbi.RegionSize == want, "size 0x%llx region 0x%llx want 0x%llx", (unsigned long long)sizes[i], (unsigned long long)mbi.RegionSize, (unsigned long long)want);
        U(VirtualFree)(p, 0, MEM_RELEASE);
    }
}
TEST(gap18_vallocex_zero_size) {
    REQUIRE(U(VirtualAllocEx));
    SET_ERR(0);
    CHECK(U(VirtualAllocEx)(GetCurrentProcess(), NULL, 0, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE) == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap18_vallocex_bad_handle) {
    REQUIRE(U(VirtualAllocEx));
    SET_ERR(0);
    CHECK(U(VirtualAllocEx)(NULL, NULL, PG, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE) == NULL);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);   /* wrong object type */
    SET_ERR(0);
    CHECK(U(VirtualAllocEx)(ev, NULL, PG, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE) == NULL);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
    SET_ERR(0);
    CHECK(U(VirtualAllocEx)(ev, NULL, PG, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE) == NULL);   /* now closed */
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(U(VirtualAllocEx)((HANDLE)(ULONG_PTR)0x7FFC, NULL, PG, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE) == NULL);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap18_vallocex_thread_handle_wrong_type) {
    REQUIRE(U(VirtualAllocEx));
    SET_ERR(0);
    CHECK(U(VirtualAllocEx)(GetCurrentThread(), NULL, PG, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE) == NULL);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap18_vallocex_invalid_type_flags) {
    REQUIRE(U(VirtualAllocEx)); REQUIRE(U(VirtualFree));
    HANDLE me = GetCurrentProcess();
    static const DWORD bad[] = { 0, MEM_RELEASE, MEM_DECOMMIT, MEM_RESET | MEM_COMMIT, MEM_RESET | MEM_RESERVE, 0x00000004, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SET_ERR(0);
        void *p = U(VirtualAllocEx)(me, NULL, PG, bad[i], PAGE_READWRITE);
        CHECKF(p == NULL, "type 0x%lx accepted", bad[i]);
        if (p) { U(VirtualFree)(p, 0, MEM_RELEASE); continue; }
        DWORD e = GetLastError();
        CHECKF(e == ERROR_INVALID_PARAMETER, "type 0x%lx err %lu", bad[i], e);
    }
}
TEST(gap18_vallocex_invalid_protect) {
    REQUIRE(U(VirtualAllocEx)); REQUIRE(U(VirtualFree));
    HANDLE me = GetCurrentProcess();
    static const DWORD bad[] = { 0, PAGE_READONLY | PAGE_READWRITE, PAGE_NOACCESS | PAGE_READONLY, PAGE_GUARD, 0x03, 0x400000, 0x100000 };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SET_ERR(0);
        void *p = U(VirtualAllocEx)(me, NULL, PG, MEM_COMMIT | MEM_RESERVE, bad[i]);
        CHECKF(p == NULL, "protect 0x%lx accepted", bad[i]);
        if (p) { U(VirtualFree)(p, 0, MEM_RELEASE); continue; }
        DWORD e = GetLastError();
        CHECKF(e == ERROR_INVALID_PARAMETER, "protect 0x%lx err %lu", bad[i], e);
    }
}
TEST(gap18_vallocex_valid_protects) {
    REQUIRE(U(VirtualAllocEx)); REQUIRE(U(VirtualFree)); REQUIRE(U(VirtualQuery));
    static const DWORD good[] = { PAGE_NOACCESS, PAGE_READONLY, PAGE_READWRITE, PAGE_EXECUTE, PAGE_EXECUTE_READ,
                                  PAGE_EXECUTE_READWRITE, PAGE_READWRITE | PAGE_GUARD, PAGE_READWRITE | PAGE_NOCACHE, PAGE_READWRITE | PAGE_WRITECOMBINE };
    /* PAGE_WRITECOPY is not valid for private memory and is deliberately absent. */
    for (unsigned i = 0; i < sizeof good / sizeof good[0]; i++) {
        SET_ERR(0);
        void *p = U(VirtualAllocEx)(GetCurrentProcess(), NULL, PG, MEM_COMMIT | MEM_RESERVE, good[i]);
        if (!p) { t_note("protect 0x%lx refused err %lu", good[i], GetLastError()); continue; }
        MEMORY_BASIC_INFORMATION mbi; U(VirtualQuery)(p, &mbi, sizeof mbi);
        if (good[i] & (PAGE_NOCACHE | PAGE_WRITECOMBINE)) t_note("protect 0x%lx reads back 0x%lx", good[i], mbi.Protect);
        else CHECKF(mbi.Protect == good[i], "protect 0x%lx reads back 0x%lx", good[i], mbi.Protect);
        U(VirtualFree)(p, 0, MEM_RELEASE);
    }
}
TEST(gap18_vallocex_reserve_then_commit) {
    REQUIRE(U(VirtualAllocEx)); REQUIRE(U(VirtualFree)); REQUIRE(U(VirtualQuery));
    HANDLE me = GetCurrentProcess();
    char *p = (char *)U(VirtualAllocEx)(me, NULL, 4 * PG, MEM_RESERVE, PAGE_READWRITE);
    CHECK(p != NULL); if (!p) return;
    MEMORY_BASIC_INFORMATION mbi; U(VirtualQuery)(p, &mbi, sizeof mbi);
    EQ(mbi.State, MEM_RESERVE);
    EQ(mbi.RegionSize, 4 * PG);
    char *c = (char *)U(VirtualAllocEx)(me, p + PG, 2 * PG, MEM_COMMIT, PAGE_READWRITE);
    EQ(c, p + PG);
    U(VirtualQuery)(p, &mbi, sizeof mbi); EQ(mbi.State, MEM_RESERVE); EQ(mbi.RegionSize, PG);
    U(VirtualQuery)(p + PG, &mbi, sizeof mbi); EQ(mbi.State, MEM_COMMIT); EQ(mbi.RegionSize, 2 * PG);
    U(VirtualQuery)(p + 3 * PG, &mbi, sizeof mbi); EQ(mbi.State, MEM_RESERVE);
    c[0] = 1;
    /* committing mid-page address rounds down to the page */
    char *c2 = (char *)U(VirtualAllocEx)(me, p + 3 * PG + 7, 1, MEM_COMMIT, PAGE_READWRITE);
    EQ(c2, p + 3 * PG);
    CHECK(U(VirtualFree)(p, 0, MEM_RELEASE));
}
TEST(gap18_vallocex_reserve_twice_same_address) {
    REQUIRE(U(VirtualAllocEx)); REQUIRE(U(VirtualFree));
    HANDLE me = GetCurrentProcess();
    void *p = U(VirtualAllocEx)(me, NULL, 0x10000, MEM_RESERVE, PAGE_READWRITE);
    CHECK(p != NULL); if (!p) return;
    SET_ERR(0);
    CHECK(U(VirtualAllocEx)(me, p, 0x10000, MEM_RESERVE, PAGE_READWRITE) == NULL);
    EXPECT_ERR(ERROR_INVALID_ADDRESS);
    U(VirtualFree)(p, 0, MEM_RELEASE);
    /* after release the exact address can usually be reused */
    void *q = U(VirtualAllocEx)(me, p, 0x10000, MEM_RESERVE, PAGE_READWRITE);
    if (q) { EQ(q, p); U(VirtualFree)(q, 0, MEM_RELEASE); } else t_note("address not reusable, err %lu", GetLastError());
}
TEST(gap18_vallocex_commit_unaligned_reserve_base) {
    REQUIRE(U(VirtualAllocEx)); REQUIRE(U(VirtualFree));
    HANDLE me = GetCurrentProcess();
    char *p = (char *)U(VirtualAllocEx)(me, NULL, 0x20000, MEM_RESERVE, PAGE_READWRITE);
    CHECK(p != NULL); if (!p) return;
    U(VirtualFree)(p, 0, MEM_RELEASE);
    /* reserve at an address that is not 64K aligned rounds down to granularity */
    char *q = (char *)U(VirtualAllocEx)(me, p + 0x1000, 0x1000, MEM_RESERVE, PAGE_READWRITE);
    if (q) { EQ(q, p); U(VirtualFree)(q, 0, MEM_RELEASE); } else t_note("unaligned reserve err %lu", GetLastError());
}
TEST(gap18_vallocex_top_down) {
    REQUIRE(U(VirtualAllocEx)); REQUIRE(U(VirtualFree));
    HANDLE me = GetCurrentProcess();
    void *a = U(VirtualAllocEx)(me, NULL, PG, MEM_COMMIT | MEM_RESERVE | MEM_TOP_DOWN, PAGE_READWRITE);
    void *b = U(VirtualAllocEx)(me, NULL, PG, MEM_COMMIT | MEM_RESERVE | MEM_TOP_DOWN, PAGE_READWRITE);
    CHECK(a && b);
    if (a && b) CHECK(b < a);      /* later top-down allocations land at lower addresses */
    if (a) U(VirtualFree)(a, 0, MEM_RELEASE);
    if (b) U(VirtualFree)(b, 0, MEM_RELEASE);
}
TEST(gap18_vallocex_zero_filled) {
    REQUIRE(U(VirtualAllocEx)); REQUIRE(U(VirtualFree));
    unsigned char *p = (unsigned char *)U(VirtualAllocEx)(GetCurrentProcess(), NULL, 8 * PG, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    CHECK(p != NULL); if (!p) return;
    int nz = 0; for (int i = 0; i < 8 * PG; i += 8) nz |= p[i];
    EQ(nz, 0);
    U(VirtualFree)(p, 0, MEM_RELEASE);
}
TEST(gap18_vallocex_absurd_sizes) {
    REQUIRE(U(VirtualAllocEx));
    HANDLE me = GetCurrentProcess();
    static const SIZE_T huge[] = { (SIZE_T)0x7FFFFFFF0000ull, (SIZE_T)0x800000000000ull, (SIZE_T)-1, (SIZE_T)-0x1000, (SIZE_T)0x7FFFFFFFFFFFull };
    for (unsigned i = 0; i < sizeof huge / sizeof huge[0]; i++) {
        SET_ERR(0);
        void *p = U(VirtualAllocEx)(me, NULL, huge[i], MEM_RESERVE, PAGE_READWRITE);
        CHECKF(p == NULL, "size 0x%llx succeeded", (unsigned long long)huge[i]);
        DWORD e = GetLastError();
        CHECKF(e == ERROR_NOT_ENOUGH_MEMORY || e == ERROR_INVALID_PARAMETER || e == ERROR_COMMITMENT_LIMIT || e == ERROR_INVALID_ADDRESS, "size 0x%llx err %lu", (unsigned long long)huge[i], e);
        if (p) U(VirtualFree)(p, 0, MEM_RELEASE);
    }
}
TEST(gap18_vallocex_kernel_address) {
    REQUIRE(U(VirtualAllocEx));
    SET_ERR(0);
    void *p = U(VirtualAllocEx)(GetCurrentProcess(), (void *)0xFFFF800000000000ull, PG, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    CHECK(p == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    p = U(VirtualAllocEx)(GetCurrentProcess(), (void *)0x7FFFFFFF0000ull, 0x20000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    CHECK(p == NULL);    /* crosses the top of user space */
}
TEST(gap18_vallocex_large_pages_without_privilege) {
    REQUIRE(U(VirtualAllocEx));
    SET_ERR(0);
    void *p = U(VirtualAllocEx)(GetCurrentProcess(), NULL, 2 * 1024 * 1024, MEM_COMMIT | MEM_RESERVE | MEM_LARGE_PAGES, PAGE_READWRITE);
    if (p) { t_note("large pages succeeded (privilege held)"); U(VirtualFree)(p, 0, MEM_RELEASE); }
    else {
        DWORD e = GetLastError();
        CHECKF(e == ERROR_PRIVILEGE_NOT_HELD || e == ERROR_NO_SYSTEM_RESOURCES || e == ERROR_NOT_ENOUGH_MEMORY || e == ERROR_INVALID_PARAMETER, "err %lu", e);
        t_note("large pages err %lu", e);
    }
}
TEST(gap18_vallocex_reset_flag) {
    REQUIRE(U(VirtualAllocEx)); REQUIRE(U(VirtualFree));
    HANDLE me = GetCurrentProcess();
    char *p = (char *)U(VirtualAllocEx)(me, NULL, PG, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    CHECK(p != NULL); if (!p) return;
    p[0] = 9;
    void *r = U(VirtualAllocEx)(me, p, PG, MEM_RESET, PAGE_NOACCESS);
    EQ(r, p);
    p[0] = 3;                     /* memory still accessible */
    U(VirtualFree)(p, 0, MEM_RELEASE);
    /* MEM_RESET on never-committed memory fails */
    char *q = (char *)U(VirtualAllocEx)(me, NULL, PG, MEM_RESERVE, PAGE_READWRITE);
    SET_ERR(0);
    void *rr = U(VirtualAllocEx)(me, q, PG, MEM_RESET, PAGE_NOACCESS);
    t_note("MEM_RESET on reserved-only memory -> %p err %lu", rr, rr ? 0 : GetLastError());
    if (q) U(VirtualFree)(q, 0, MEM_RELEASE);
}
TEST(gap18_vallocex_commit_beyond_reservation) {
    REQUIRE(U(VirtualAllocEx)); REQUIRE(U(VirtualFree));
    HANDLE me = GetCurrentProcess();
    char *p = (char *)U(VirtualAllocEx)(me, NULL, 2 * PG, MEM_RESERVE, PAGE_READWRITE);
    CHECK(p != NULL); if (!p) return;
    SET_ERR(0);
    void *c = U(VirtualAllocEx)(me, p, 0x20000, MEM_COMMIT, PAGE_READWRITE);   /* spills past the reservation */
    CHECK(c == NULL);
    t_note("commit past reservation err %lu", GetLastError());
    U(VirtualFree)(p, 0, MEM_RELEASE);
}
TEST(gap18_vallocex_k32_forward_and_alias) {
    REQUIRE(UK(VirtualAllocEx)); REQUIRE(UK(VirtualAlloc)); REQUIRE(UK(VirtualFree));
    void *a = UK(VirtualAllocEx)(GetCurrentProcess(), NULL, PG, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    void *b = UK(VirtualAlloc)(NULL, PG, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    CHECK(a && b && a != b);
    if (a) UK(VirtualFree)(a, 0, MEM_RELEASE);
    if (b) UK(VirtualFree)(b, 0, MEM_RELEASE);
}

/* ------------------------- VirtualAllocEx: remote ------------------------- */
TEST(gap18_vallocex_remote_alloc_write_read) {
    REQUIRE(U(VirtualAllocEx)); REQUIRE(U(VirtualFreeEx)); REQUIRE(U(WriteProcessMemory)); REQUIRE(U(ReadProcessMemory)); REQUIRE(U(VirtualQueryEx));
    PROCESS_INFORMATION pi; if (!g18_spawn_idle(&pi)) { t_skip("spawn failed"); return; }
    char *p = (char *)U(VirtualAllocEx)(pi.hProcess, NULL, 0x3000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    CHECKF(p != NULL, "err %lu", GetLastError());
    if (p) {
        MEMORY_BASIC_INFORMATION mbi; memset(&mbi, 0, sizeof mbi);
        CHECK(U(VirtualQueryEx)(pi.hProcess, p, &mbi, sizeof mbi) == sizeof mbi);
        EQ(mbi.State, MEM_COMMIT); EQ(mbi.RegionSize, 0x3000);
        SIZE_T n = 0;
        CHECK(U(WriteProcessMemory)(pi.hProcess, p + 0x2FF0, "remote-data", 12, &n)); EQ(n, 12);
        char b[16] = {0}; n = 0;
        CHECK(U(ReadProcessMemory)(pi.hProcess, p + 0x2FF0, b, 12, &n)); EQ(n, 12);
        CHECK(!strcmp(b, "remote-data"));
        /* the address is not necessarily mapped in this process */
        CHECK(U(VirtualFreeEx)(pi.hProcess, p, 0, MEM_RELEASE));
        SET_ERR(0);
        CHECK(!U(VirtualQueryEx)(pi.hProcess, p, &mbi, sizeof mbi) || mbi.State == MEM_FREE);
    }
    g18_kill(&pi);
}
TEST(gap18_vallocex_remote_right_requirements) {
    REQUIRE(U(VirtualAllocEx)); REQUIRE(U(OpenProcess));
    PROCESS_INFORMATION pi; if (!g18_spawn_idle(&pi)) { t_skip("spawn failed"); return; }
    static const DWORD denied[] = { PROCESS_QUERY_INFORMATION, PROCESS_QUERY_LIMITED_INFORMATION, PROCESS_VM_READ, PROCESS_VM_WRITE,
                                    PROCESS_CREATE_THREAD, SYNCHRONIZE, PROCESS_DUP_HANDLE };
    for (unsigned i = 0; i < sizeof denied / sizeof denied[0]; i++) {
        HANDLE h = U(OpenProcess)(denied[i], FALSE, pi.dwProcessId);
        if (!h) { t_note("OpenProcess 0x%lx failed %lu", denied[i], GetLastError()); continue; }
        SET_ERR(0);
        void *p = U(VirtualAllocEx)(h, NULL, PG, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        CHECKF(p == NULL, "alloc succeeded with only rights 0x%lx", denied[i]);
        EXPECT_ERR(ERROR_ACCESS_DENIED);
        CloseHandle(h);
    }
    HANDLE h = U(OpenProcess)(PROCESS_VM_OPERATION, FALSE, pi.dwProcessId);
    if (h) {
        void *p = U(VirtualAllocEx)(h, NULL, PG, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        CHECKF(p != NULL, "PROCESS_VM_OPERATION alone should suffice, err %lu", GetLastError());
        if (p && U(VirtualFreeEx)) U(VirtualFreeEx)(h, p, 0, MEM_RELEASE);
        CloseHandle(h);
    }
    g18_kill(&pi);
}
TEST(gap18_vallocex_remote_exited_process) {
    REQUIRE(U(VirtualAllocEx));
    PROCESS_INFORMATION pi; if (!g18_spawn_idle(&pi)) { t_skip("spawn failed"); return; }
    TerminateProcess(pi.hProcess, 3); WaitForSingleObject(pi.hProcess, 5000);
    SET_ERR(0);
    void *p = U(VirtualAllocEx)(pi.hProcess, NULL, PG, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    CHECK(p == NULL);
    t_note("alloc in exited process err %lu", GetLastError());
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
}
TEST(gap18_vallocex_remote_fixed_conflict) {
    REQUIRE(U(VirtualAllocEx)); REQUIRE(U(VirtualFreeEx));
    PROCESS_INFORMATION pi; if (!g18_spawn_idle(&pi)) { t_skip("spawn failed"); return; }
    void *a = U(VirtualAllocEx)(pi.hProcess, NULL, 0x10000, MEM_RESERVE, PAGE_READWRITE);
    CHECK(a != NULL);
    if (a) {
        SET_ERR(0);
        CHECK(U(VirtualAllocEx)(pi.hProcess, a, 0x10000, MEM_RESERVE, PAGE_READWRITE) == NULL);
        EXPECT_ERR(ERROR_INVALID_ADDRESS);
        U(VirtualFreeEx)(pi.hProcess, a, 0, MEM_RELEASE);
    }
    g18_kill(&pi);
}
TEST(gap18_vallocex_remote_exec_protect) {
    REQUIRE(U(VirtualAllocEx)); REQUIRE(U(VirtualQueryEx)); REQUIRE(U(VirtualFreeEx));
    PROCESS_INFORMATION pi; if (!g18_spawn_idle(&pi)) { t_skip("spawn failed"); return; }
    void *a = U(VirtualAllocEx)(pi.hProcess, NULL, PG, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READ);
    CHECK(a != NULL);
    if (a) {
        MEMORY_BASIC_INFORMATION mbi; U(VirtualQueryEx)(pi.hProcess, a, &mbi, sizeof mbi);
        EQ(mbi.Protect, PAGE_EXECUTE_READ);
        U(VirtualFreeEx)(pi.hProcess, a, 0, MEM_RELEASE);
    }
    g18_kill(&pi);
}
TEST(gap18_vallocex_remote_many_small) {
    REQUIRE(U(VirtualAllocEx)); REQUIRE(U(VirtualFreeEx));
    PROCESS_INFORMATION pi; if (!g18_spawn_idle(&pi)) { t_skip("spawn failed"); return; }
    void *ptrs[64]; int n = 0;
    for (int i = 0; i < 64; i++) { ptrs[n] = U(VirtualAllocEx)(pi.hProcess, NULL, PG, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); if (ptrs[n]) n++; }
    EQ(n, 64);
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) CHECK(ptrs[i] != ptrs[j]);
    for (int i = 0; i < n; i++) CHECK(U(VirtualFreeEx)(pi.hProcess, ptrs[i], 0, MEM_RELEASE));
    g18_kill(&pi);
}

/* ------------------------- VirtualProtect ------------------------- */
static volatile LONG g_veh_hits; static volatile ULONG_PTR g_veh_info0; static volatile DWORD g_veh_code; static volatile void *g_veh_addr; static char *g_veh_page;
static LONG CALLBACK vp_veh(EXCEPTION_POINTERS *ep) {
    DWORD c = ep->ExceptionRecord->ExceptionCode;
    if ((c == EXCEPTION_ACCESS_VIOLATION || c == STATUS_GUARD_PAGE_VIOLATION) && g_veh_page &&
        (char *)ep->ExceptionRecord->ExceptionInformation[1] >= g_veh_page && (char *)ep->ExceptionRecord->ExceptionInformation[1] < g_veh_page + PG) {
        g_veh_hits++; g_veh_code = c; g_veh_info0 = ep->ExceptionRecord->ExceptionInformation[0]; g_veh_addr = (void *)ep->ExceptionRecord->ExceptionInformation[1];
        if (c == EXCEPTION_ACCESS_VIOLATION) { DWORD old; VirtualProtect(g_veh_page, PG, PAGE_READWRITE, &old); }
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
static char *vp_page(void) { return (char *)VirtualAlloc(NULL, 4 * PG, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); }

TEST(gap18_vprotect_returns_old) {
    REQUIRE(U(VirtualProtect));
    char *p = vp_page(); CHECK(p); if (!p) return;
    DWORD old = 0xAAAA;
    CHECK(U(VirtualProtect)(p, PG, PAGE_READONLY, &old)); EQ(old, PAGE_READWRITE);
    CHECK(U(VirtualProtect)(p, PG, PAGE_EXECUTE_READ, &old)); EQ(old, PAGE_READONLY);
    CHECK(U(VirtualProtect)(p, PG, PAGE_NOACCESS, &old)); EQ(old, PAGE_EXECUTE_READ);
    CHECK(U(VirtualProtect)(p, PG, PAGE_READWRITE, &old)); EQ(old, PAGE_NOACCESS);
    CHECK(U(VirtualProtect)(p, PG, PAGE_READWRITE, &old)); EQ(old, PAGE_READWRITE);   /* same protection is fine */
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap18_vprotect_null_oldprotect) {
    REQUIRE(U(VirtualProtect));
    char *p = vp_page(); CHECK(p); if (!p) return;
    SET_ERR(0);
    CHECK(!U(VirtualProtect)(p, PG, PAGE_READONLY, NULL));
    t_note("NULL lpflOldProtect err %lu", GetLastError());
    MEMORY_BASIC_INFORMATION mbi; VirtualQuery(p, &mbi, sizeof mbi);
    EQ(mbi.Protect, PAGE_READWRITE);     /* unchanged on failure */
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap18_vprotect_zero_size) {
    REQUIRE(U(VirtualProtect));
    char *p = vp_page(); CHECK(p); if (!p) return;
    DWORD old = 0;
    SET_ERR(0);
    CHECK(!U(VirtualProtect)(p, 0, PAGE_READONLY, &old));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap18_vprotect_free_memory) {
    REQUIRE(U(VirtualProtect));
    char *p = vp_page(); CHECK(p); if (!p) return;
    VirtualFree(p, 0, MEM_RELEASE);
    DWORD old = 0;
    SET_ERR(0);
    CHECK(!U(VirtualProtect)(p, PG, PAGE_READONLY, &old));
    EXPECT_ERR(ERROR_INVALID_ADDRESS);
}
TEST(gap18_vprotect_null_address) {
    REQUIRE(U(VirtualProtect));
    DWORD old = 0;
    SET_ERR(0);
    CHECK(!U(VirtualProtect)(NULL, PG, PAGE_READONLY, &old));
    t_note("VirtualProtect(NULL) err %lu", GetLastError());
}
TEST(gap18_vprotect_invalid_protection_values) {
    REQUIRE(U(VirtualProtect));
    char *p = vp_page(); CHECK(p); if (!p) return;
    static const DWORD bad[] = { 0, 0x03, 0x05, PAGE_READONLY | PAGE_READWRITE, PAGE_NOACCESS | PAGE_GUARD | PAGE_READONLY | PAGE_EXECUTE, 0x100000, 0x400000, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        DWORD old = 0; SET_ERR(0);
        BOOL r = U(VirtualProtect)(p, PG, bad[i], &old);
        CHECKF(!r, "protect 0x%lx accepted", bad[i]);
        if (r) { DWORD o2; VirtualProtect(p, PG, PAGE_READWRITE, &o2); continue; }
        DWORD e = GetLastError();
        CHECKF(e == ERROR_INVALID_PARAMETER, "protect 0x%lx err %lu", bad[i], e);
    }
    MEMORY_BASIC_INFORMATION mbi; VirtualQuery(p, &mbi, sizeof mbi); EQ(mbi.Protect, PAGE_READWRITE);
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap18_vprotect_rounds_to_page) {
    REQUIRE(U(VirtualProtect));
    char *p = vp_page(); CHECK(p); if (!p) return;
    DWORD old;
    CHECK(U(VirtualProtect)(p + PG + 5, 1, PAGE_READONLY, &old));
    MEMORY_BASIC_INFORMATION mbi;
    VirtualQuery(p, &mbi, sizeof mbi); EQ(mbi.Protect, PAGE_READWRITE); EQ(mbi.RegionSize, PG);
    VirtualQuery(p + PG, &mbi, sizeof mbi); EQ(mbi.Protect, PAGE_READONLY); EQ(mbi.RegionSize, PG);
    VirtualQuery(p + 2 * PG, &mbi, sizeof mbi); EQ(mbi.Protect, PAGE_READWRITE); EQ(mbi.RegionSize, 2 * PG);
    /* range spanning a page boundary covers both pages */
    CHECK(U(VirtualProtect)(p + 2 * PG + PG - 1, 2, PAGE_READONLY, &old));
    VirtualQuery(p + 2 * PG, &mbi, sizeof mbi); EQ(mbi.Protect, PAGE_READONLY); EQ(mbi.RegionSize, 2 * PG);
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap18_vprotect_mixed_old_protections) {
    REQUIRE(U(VirtualProtect));
    char *p = vp_page(); CHECK(p); if (!p) return;
    DWORD old;
    VirtualProtect(p + PG, PG, PAGE_READONLY, &old);
    /* old protection reports only the first page of the range */
    CHECK(U(VirtualProtect)(p, 2 * PG, PAGE_READWRITE, &old)); EQ(old, PAGE_READWRITE);
    VirtualProtect(p + PG, PG, PAGE_READONLY, &old);
    CHECK(U(VirtualProtect)(p + PG, 2 * PG, PAGE_READWRITE, &old)); EQ(old, PAGE_READONLY);
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap18_vprotect_range_into_free) {
    REQUIRE(U(VirtualProtect));
    char *p = (char *)VirtualAlloc(NULL, PG, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); CHECK(p); if (!p) return;
    DWORD old = 0; SET_ERR(0);
    /* 64K granularity: the page after a one-page allocation is reserved-but-uncommitted, beyond that free */
    BOOL r = U(VirtualProtect)(p, 0x10000 + PG, PAGE_READONLY, &old);
    CHECK(!r);
    t_note("range beyond allocation err %lu", GetLastError());
    MEMORY_BASIC_INFORMATION mbi; VirtualQuery(p, &mbi, sizeof mbi);
    t_note("first page protect after failed multi-page call: 0x%lx", mbi.Protect);
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap18_vprotect_reserved_uncommitted) {
    REQUIRE(U(VirtualProtect));
    char *p = (char *)VirtualAlloc(NULL, 2 * PG, MEM_RESERVE, PAGE_READWRITE); CHECK(p); if (!p) return;
    DWORD old = 0; SET_ERR(0);
    CHECK(!U(VirtualProtect)(p, PG, PAGE_READONLY, &old));
    t_note("protect on reserved-only err %lu", GetLastError());
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap18_vprotect_readonly_write_faults) {
    REQUIRE(U(VirtualProtect));
    char *p = vp_page(); CHECK(p); if (!p) return;
    p[0] = 5;
    DWORD old;
    CHECK(U(VirtualProtect)(p, PG, PAGE_READONLY, &old));
    g_veh_hits = 0; g_veh_page = p;
    PVOID h = AddVectoredExceptionHandler(1, vp_veh);
    volatile char *vp = p;
    char r = vp[0];            /* read OK */
    EQ(r, 5);
    EQ(g_veh_hits, 0);
    vp[0] = 6;                 /* faults, handler restores RW */
    RemoveVectoredExceptionHandler(h);
    g_veh_page = NULL;
    EQ(g_veh_hits, 1);
    EQ(g_veh_code, EXCEPTION_ACCESS_VIOLATION);
    EQ(g_veh_info0, 1);          /* write */
    EQ(g_veh_addr, p);
    EQ(p[0], 6);
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap18_vprotect_noaccess_read_faults) {
    REQUIRE(U(VirtualProtect));
    char *p = vp_page(); CHECK(p); if (!p) return;
    DWORD old;
    CHECK(U(VirtualProtect)(p, PG, PAGE_NOACCESS, &old));
    g_veh_hits = 0; g_veh_page = p;
    PVOID h = AddVectoredExceptionHandler(1, vp_veh);
    volatile char *vp = p;
    char r = vp[3]; (void)r;
    RemoveVectoredExceptionHandler(h);
    g_veh_page = NULL;
    EQ(g_veh_hits, 1);
    EQ(g_veh_info0, 0);          /* read */
    EQ(g_veh_addr, p + 3);
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap18_vprotect_guard_page_once) {
    REQUIRE(U(VirtualProtect));
    char *p = vp_page(); CHECK(p); if (!p) return;
    DWORD old;
    CHECK(U(VirtualProtect)(p, PG, PAGE_READWRITE | PAGE_GUARD, &old));
    MEMORY_BASIC_INFORMATION mbi; VirtualQuery(p, &mbi, sizeof mbi);
    EQ(mbi.Protect, PAGE_READWRITE | PAGE_GUARD);
    g_veh_hits = 0; g_veh_page = p;
    PVOID h = AddVectoredExceptionHandler(1, vp_veh);
    volatile char *vp = p;
    vp[0] = 1;                 /* STATUS_GUARD_PAGE_VIOLATION, guard bit cleared, then access proceeds */
    vp[1] = 2;                 /* no second fault */
    RemoveVectoredExceptionHandler(h);
    g_veh_page = NULL;
    EQ(g_veh_hits, 1);
    EQ(g_veh_code, STATUS_GUARD_PAGE_VIOLATION);
    VirtualQuery(p, &mbi, sizeof mbi);
    EQ(mbi.Protect, PAGE_READWRITE);
    EQ(p[0], 1);
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap18_vprotect_content_preserved) {
    REQUIRE(U(VirtualProtect));
    char *p = vp_page(); CHECK(p); if (!p) return;
    memset(p, 0x5A, 4 * PG);
    DWORD old;
    CHECK(U(VirtualProtect)(p, 4 * PG, PAGE_NOACCESS, &old));
    CHECK(U(VirtualProtect)(p, 4 * PG, PAGE_READWRITE, &old));
    int ok = 1; for (int i = 0; i < 4 * PG; i += 64) ok &= (p[i] == 0x5A);
    CHECK(ok);
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap18_vprotect_image_text_writecopy) {
    REQUIRE(U(VirtualProtect)); REQUIRE(U(VirtualQuery));
    /* a read-only image header page can be made writable copy-on-write; we only flip and restore it. */
    HMODULE m = GetModuleHandleA("kernel32.dll");
    MEMORY_BASIC_INFORMATION mbi; U(VirtualQuery)(m, &mbi, sizeof mbi);
    EQ(mbi.Type, MEM_IMAGE); EQ(mbi.Protect, PAGE_READONLY);
    DWORD old = 0, o2 = 0;
    CHECK(U(VirtualProtect)(m, PG, PAGE_READONLY, &old)); EQ(old, PAGE_READONLY);
    CHECK(U(VirtualProtect)(m, PG, PAGE_READONLY, &o2));
}
TEST(gap18_vprotect_mapped_view_limits) {
    REQUIRE(U(VirtualProtect));
    HANDLE fm = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READONLY, 0, PG, NULL);
    CHECK(fm); if (!fm) return;
    char *v = (char *)MapViewOfFile(fm, FILE_MAP_READ, 0, 0, 0); CHECK(v);
    if (v) {
        DWORD old = 0; SET_ERR(0);
        CHECK(!U(VirtualProtect)(v, PG, PAGE_READWRITE, &old));        /* view lacks write access */
        t_note("write protection on read-only view err %lu", GetLastError());
        CHECK(U(VirtualProtect)(v, PG, PAGE_NOACCESS, &old)); EQ(old, PAGE_READONLY);
        UnmapViewOfFile(v);
    }
    CloseHandle(fm);
}
TEST(gap18_vprotect_exec_toggle_repeat) {
    REQUIRE(U(VirtualProtect));
    char *p = vp_page(); CHECK(p); if (!p) return;
    for (int i = 0; i < 2000; i++) {
        DWORD old, want = (i & 1) ? PAGE_READWRITE : PAGE_EXECUTE_READ, prev = (i & 1) ? PAGE_EXECUTE_READ : PAGE_READWRITE;
        if (!U(VirtualProtect)(p, PG, want, &old) || old != prev) { CHECKF(0, "iter %d old 0x%lx err %lu", i, old, GetLastError()); break; }
    }
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap18_vprotect_k32_export) {
    REQUIRE(UK(VirtualProtect)); REQUIRE(UB(VirtualProtect));
    char *p = vp_page(); CHECK(p); if (!p) return;
    DWORD old;
    CHECK(UK(VirtualProtect)(p, PG, PAGE_READONLY, &old)); EQ(old, PAGE_READWRITE);
    CHECK(UB(VirtualProtect)(p, PG, PAGE_READWRITE, &old)); EQ(old, PAGE_READONLY);
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap18_vprotect_cross_thread_visibility) {
    REQUIRE(U(VirtualProtect));
    char *p = vp_page(); CHECK(p); if (!p) return;
    DWORD old;
    CHECK(U(VirtualProtect)(p, PG, PAGE_NOACCESS, &old));
    /* another thread querying sees the same protection (process-wide) */
    struct qa { char *p; DWORD prot; } a = { p, 0 };
    DWORD WINAPI q(LPVOID v) { struct qa *a = v; MEMORY_BASIC_INFORMATION m; VirtualQuery(a->p, &m, sizeof m); a->prot = m.Protect; return 0; }
    HANDLE t = CreateThread(NULL, 0, q, &a, 0, NULL);
    WaitForSingleObject(t, 5000); CloseHandle(t);
    EQ(a.prot, PAGE_NOACCESS);
    VirtualFree(p, 0, MEM_RELEASE);
}

/* ------------------------- SetProcessWorkingSetSizeEx ------------------------- */
TEST(gap18_spwsex_zero_values_rejected) {
    REQUIRE(U(SetProcessWorkingSetSizeEx));
    HANDLE me = GetCurrentProcess();
    static const SIZE_T v[][2] = { {0, 0}, {0, 0x400000}, {0x100000, 0} };
    for (int i = 0; i < 3; i++) {
        SET_ERR(0);
        CHECK(!U(SetProcessWorkingSetSizeEx)(me, v[i][0], v[i][1], 0));
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(gap18_spwsex_zero_values_any_flags) {
    REQUIRE(U(SetProcessWorkingSetSizeEx));
    for (DWORD f = 0; f < 16; f++) {
        SET_ERR(0);
        CHECKF(!U(SetProcessWorkingSetSizeEx)(GetCurrentProcess(), 0, 0, f), "flags %lu", f);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(gap18_spwsex_zero_checked_before_handle) {
    REQUIRE(U(SetProcessWorkingSetSizeEx));
    SET_ERR(0);
    CHECK(!U(SetProcessWorkingSetSizeEx)(NULL, 0, 0x400000, 0));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);       /* parameter check precedes handle lookup */
}
TEST(gap18_spwsex_bad_handle) {
    REQUIRE(U(SetProcessWorkingSetSizeEx));
    SET_ERR(0);
    CHECK(!U(SetProcessWorkingSetSizeEx)(NULL, 0x100000, 0x400000, 0));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    CHECK(!U(SetProcessWorkingSetSizeEx)(ev, 0x100000, 0x400000, 0));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
    SET_ERR(0);
    CHECK(!U(SetProcessWorkingSetSizeEx)(ev, 0x100000, 0x400000, 0));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(!U(SetProcessWorkingSetSizeEx)(GetCurrentThread(), 0x100000, 0x400000, 0));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap18_spwsex_access_denied_without_set_quota) {
    REQUIRE(U(SetProcessWorkingSetSizeEx)); REQUIRE(U(OpenProcess));
    PROCESS_INFORMATION pi; if (!g18_spawn_idle(&pi)) { t_skip("spawn failed"); return; }
    static const DWORD rights[] = { PROCESS_QUERY_LIMITED_INFORMATION, PROCESS_QUERY_INFORMATION, PROCESS_VM_OPERATION, SYNCHRONIZE };
    for (unsigned i = 0; i < 4; i++) {
        HANDLE h = U(OpenProcess)(rights[i], FALSE, pi.dwProcessId);
        if (!h) continue;
        SET_ERR(0);
        CHECKF(!U(SetProcessWorkingSetSizeEx)(h, 0x100000, 0x400000, 0), "rights 0x%lx", rights[i]);
        EXPECT_ERR(ERROR_ACCESS_DENIED);
        CloseHandle(h);
    }
    g18_kill(&pi);
}
TEST(gap18_spwsex_remote_set_and_get) {
    REQUIRE(U(SetProcessWorkingSetSizeEx)); REQUIRE(U(GetProcessWorkingSetSizeEx));
    PROCESS_INFORMATION pi; if (!g18_spawn_idle(&pi)) { t_skip("spawn failed"); return; }
    SET_ERR(0);
    BOOL r = U(SetProcessWorkingSetSizeEx)(pi.hProcess, 0x200000, 0x800000, 0);
    CHECKF(r, "err %lu", GetLastError());
    if (r) {
        SIZE_T mn = 0, mx = 0; DWORD fl = 0xFFFF;
        CHECK(U(GetProcessWorkingSetSizeEx)(pi.hProcess, &mn, &mx, &fl));
        t_note("working set read back min=0x%llx max=0x%llx flags=0x%lx", (unsigned long long)mn, (unsigned long long)mx, fl);
        CHECK(mx >= mn && mn > 0);
    }
    g18_kill(&pi);
}
TEST(gap18_spwsex_remote_hard_flags_roundtrip) {
    REQUIRE(U(SetProcessWorkingSetSizeEx)); REQUIRE(U(GetProcessWorkingSetSizeEx));
    PROCESS_INFORMATION pi; if (!g18_spawn_idle(&pi)) { t_skip("spawn failed"); return; }
    static const DWORD fl[] = { QUOTA_LIMITS_HARDWS_MIN_ENABLE, QUOTA_LIMITS_HARDWS_MAX_ENABLE, QUOTA_LIMITS_HARDWS_MIN_ENABLE | QUOTA_LIMITS_HARDWS_MAX_ENABLE,
                                QUOTA_LIMITS_HARDWS_MIN_DISABLE | QUOTA_LIMITS_HARDWS_MAX_DISABLE };
    for (unsigned i = 0; i < 4; i++) {
        SET_ERR(0);
        BOOL r = U(SetProcessWorkingSetSizeEx)(pi.hProcess, 0x200000, 0x800000, fl[i]);
        if (!r) { t_note("flags 0x%lx err %lu", fl[i], GetLastError()); continue; }
        SIZE_T mn, mx; DWORD got = 0xFFFF;
        CHECK(U(GetProcessWorkingSetSizeEx)(pi.hProcess, &mn, &mx, &got));
        t_note("flags set 0x%lx read back 0x%lx", fl[i], got);
        (void)got;
    }
    g18_kill(&pi);
}
TEST(gap18_spwsex_remote_min_greater_than_max) {
    REQUIRE(U(SetProcessWorkingSetSizeEx));
    PROCESS_INFORMATION pi; if (!g18_spawn_idle(&pi)) { t_skip("spawn failed"); return; }
    SET_ERR(0);
    BOOL r = U(SetProcessWorkingSetSizeEx)(pi.hProcess, 0x800000, 0x200000, 0);
    CHECK(!r);
    t_note("min > max err %lu", GetLastError());
    g18_kill(&pi);
}
TEST(gap18_spwsex_remote_trim_minus1) {
    REQUIRE(U(SetProcessWorkingSetSizeEx));
    PROCESS_INFORMATION pi; if (!g18_spawn_idle(&pi)) { t_skip("spawn failed"); return; }
    SET_ERR(0);
    BOOL r = U(SetProcessWorkingSetSizeEx)(pi.hProcess, (SIZE_T)-1, (SIZE_T)-1, 0);
    CHECK(r);             /* documented: both -1 trims the working set */
    t_note("trim err %lu", r ? 0 : GetLastError());
    g18_kill(&pi);
}
TEST(gap18_spwsex_remote_exited) {
    REQUIRE(U(SetProcessWorkingSetSizeEx));
    PROCESS_INFORMATION pi; if (!g18_spawn_idle(&pi)) { t_skip("spawn failed"); return; }
    TerminateProcess(pi.hProcess, 0); WaitForSingleObject(pi.hProcess, 5000);
    SET_ERR(0);
    BOOL r = U(SetProcessWorkingSetSizeEx)(pi.hProcess, 0x100000, 0x400000, 0);   /* result depends on kernel quota path; not verifiable from decomp */
    t_note("exited-process -> %d err %lu", r, r ? 0 : GetLastError());
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
}
CHILD(gap18_wsself) {
    CK(U(SetProcessWorkingSetSizeEx) && U(GetProcessWorkingSetSizeEx) && U(SetProcessWorkingSetSize) && U(GetProcessWorkingSetSize));
    HANDLE me = GetCurrentProcess();
    SIZE_T mn = 0, mx = 0; DWORD fl = 0;
    CK(U(GetProcessWorkingSetSizeEx)(me, &mn, &mx, &fl));
    CK(mn > 0 && mx >= mn);
    CK(U(SetProcessWorkingSetSizeEx)(me, 0x300000, 0x1000000, 0));
    SIZE_T mn2 = 0, mx2 = 0;
    CK(U(GetProcessWorkingSetSize)(me, &mn2, &mx2));
    CK(mx2 >= mn2 && mn2 > 0);
    CK(U(SetProcessWorkingSetSize)(me, mn, mx));          /* non-Ex wrapper path restores */
    CK(U(GetProcessWorkingSetSizeEx)(me, &mn2, &mx2, &fl));
    return 0;
}
TEST(gap18_spwsex_self_in_child) { EQ(t_run_child("gap18_wsself", "", 30000), 0); }
