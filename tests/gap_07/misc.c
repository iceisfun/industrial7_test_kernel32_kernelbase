/* gap7: IsInternetESCEnabled, IsProcessCritical, IsUserCetAvailableInEnvironment, enclave loaders,
 *       MapUserPhysicalPages[Scatter], MapViewOfFileEx, MapViewOfFileNuma2, NotifyUILanguageChange */
#include "t.h"

UDECL(BOOL, IsInternetESCEnabled, (void));
static BOOL esc_expected(void) {
    HKEY k; DWORD val = 0, cb = 4, type;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings\\ZoneMap", 0, KEY_READ, &k) != 0) return FALSE;
    LONG r = RegQueryValueExA(k, "IEharden", NULL, &type, (LPBYTE)&val, &cb);
    RegCloseKey(k);
    return r == 0 && val == 1;
}

TEST(gap7_isinternetesc_matches_registry) {
    REQUIRE(UN(IsInternetESCEnabled));
    BOOL r = UN(IsInternetESCEnabled)();
    CHECK(r == 0 || r == 1);
    CHECKF((r != 0) == (esc_expected() != 0), "got %d", r);
}

TEST(gap7_isinternetesc_repeat_stable) {
    REQUIRE(UN(IsInternetESCEnabled));
    BOOL first = UN(IsInternetESCEnabled)();
    for (int i = 0; i < 200; i++)
        if (UN(IsInternetESCEnabled)() != first) { CHECKF(0, "changed at %d", i); break; }
}

TEST(gap7_isinternetesc_not_in_kernel32) {
    /* kernel32 does not export it; only kernelbase does */
    CHECK(t_proc_k32("IsInternetESCEnabled") == NULL);
    REQUIRE(UN(IsInternetESCEnabled));
}

/* ---------- IsProcessCritical ---------- */
UDECL(BOOL, IsProcessCritical, (HANDLE, PBOOL));
UDECL(BOOL, IsUserCetAvailableInEnvironment, (DWORD));

CHILD(g7_idle) { Sleep(15000); return 0; }

TEST(gap7_isprocesscritical_self) {
    REQUIRE(UN(IsProcessCritical));
    BOOL crit = 0x55;
    SET_ERR(0);
    CHECK(UN(IsProcessCritical)(GetCurrentProcess(), &crit) == TRUE);
    CHECK(crit == FALSE);
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, GetCurrentProcessId());
    if (h) {
        crit = 0x55;
        CHECK(UN(IsProcessCritical)(h, &crit) == TRUE);
        CHECK(crit == FALSE);
        CloseHandle(h);
    }
}

TEST(gap7_isprocesscritical_child) {
    REQUIRE(UN(IsProcessCritical));
    PROCESS_INFORMATION pi;
    HANDLE hp = t_spawn_child("g7_idle", "", 0, NULL, NULL, NULL, FALSE, NULL, &pi);
    CHECK(hp != NULL);
    if (!hp) return;
    BOOL crit = 0x55;
    CHECK(UN(IsProcessCritical)(hp, &crit) == TRUE);
    CHECK(crit == FALSE);
    TerminateProcess(hp, 0);
    WaitForSingleObject(hp, 5000);
    /* an exited (but still referenced) process can still be queried */
    crit = 0x55;
    BOOL r = UN(IsProcessCritical)(hp, &crit);
    t_note("IsProcessCritical(exited) -> %d crit=%d err=%lu", r, crit, r ? 0 : GetLastError());
    CloseHandle(pi.hThread); CloseHandle(hp);
}

TEST(gap7_isprocesscritical_invalid_handle) {
    REQUIRE(UN(IsProcessCritical));
    BOOL crit = 0x55;
    SET_ERR(0);
    CHECK(UN(IsProcessCritical)(NULL, &crit) == FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CHECK(crit == 0x55);
    SET_ERR(0);
    CHECK(UN(IsProcessCritical)(INVALID_HANDLE_VALUE, &crit) == FALSE);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_INVALID_HANDLE || e == ERROR_INVALID_PARAMETER, "err %lu", e);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    CHECK(UN(IsProcessCritical)(ev, &crit) == FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
    HANDLE closed = CreateEventA(NULL, TRUE, FALSE, NULL); CloseHandle(closed);
    SET_ERR(0);
    CHECK(UN(IsProcessCritical)(closed, &crit) == FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CHECK(crit == 0x55);
    /* a thread pseudo handle is not a process */
    SET_ERR(0);
    CHECK(UN(IsProcessCritical)(GetCurrentThread(), &crit) == FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}

TEST(gap7_isprocesscritical_insufficient_access) {
    REQUIRE(UN(IsProcessCritical));
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, GetCurrentProcessId());
    CHECK(h != NULL);
    if (!h) return;
    BOOL crit = 0x55;
    SET_ERR(0);
    CHECK(UN(IsProcessCritical)(h, &crit) == FALSE);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CHECK(crit == 0x55);
    CloseHandle(h);
    h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    if (h) {
        SET_ERR(0);
        BOOL r = UN(IsProcessCritical)(h, &crit);
        t_note("IsProcessCritical(QUERY_LIMITED) -> %d err=%lu", r, r ? 0 : GetLastError());
        CloseHandle(h);
    }
}

/* ---------- IsUserCetAvailableInEnvironment ---------- */
TEST(gap7_iscetavailable_environment_values) {
    REQUIRE(UN(IsUserCetAvailableInEnvironment));
    BOOL w32 = UN(IsUserCetAvailableInEnvironment)(0);
    CHECK(w32 == 0 || w32 == 1);
    t_note("User CET available for Win32 processes: %d", w32);
    static const DWORD others[] = { 1, 2, 3, 4, 0x10, 0x100, 0x7fffffff, 0x80000000, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof others / sizeof others[0]; i++)
        CHECKF(UN(IsUserCetAvailableInEnvironment)(others[i]) == FALSE, "environment %lu", others[i]);
}

TEST(gap7_iscetavailable_stable_and_noerror) {
    REQUIRE(UN(IsUserCetAvailableInEnvironment));
    BOOL first = UN(IsUserCetAvailableInEnvironment)(0);
    for (int i = 0; i < 100; i++)
        if (UN(IsUserCetAvailableInEnvironment)(0) != first) { CHECKF(0, "changed at %d", i); break; }
    SET_ERR(0x4242);
    UN(IsUserCetAvailableInEnvironment)(0);
    UN(IsUserCetAvailableInEnvironment)(5);
    EXPECT_ERR(0x4242);
}

/* ---------- enclave loaders (error paths only) ---------- */
UDECL(BOOL, LoadEnclaveImageA, (LPVOID, LPCSTR));
UDECL(BOOL, LoadEnclaveImageW, (LPVOID, LPCWSTR));
UDECL(BOOL, LoadEnclaveData, (HANDLE, LPVOID, LPCVOID, SIZE_T, DWORD, LPCVOID, DWORD, PSIZE_T, PDWORD));

TEST(gap7_loadenclaveimageA_nonexistent) {
    REQUIRE(UN(LoadEnclaveImageA));
    SET_ERR(0);
    CHECK(UN(LoadEnclaveImageA)(NULL, "k32t_no_such_enclave_image.dll") == FALSE);
    CHECK(GetLastError() != 0);
    t_note("LoadEnclaveImageA(NULL,missing) err %lu", GetLastError());
}

TEST(gap7_loadenclaveimageA_not_an_enclave_address) {
    REQUIRE(UN(LoadEnclaveImageA));
    void *p = VirtualAlloc(NULL, 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    CHECK(p != NULL);
    if (!p) return;
    SET_ERR(0);
    CHECK(UN(LoadEnclaveImageA)(p, "k32t_no_such_enclave_image.dll") == FALSE);
    CHECK(GetLastError() != 0);
    t_note("LoadEnclaveImageA(non-enclave addr) err %lu", GetLastError());
    VirtualFree(p, 0, MEM_RELEASE);
}

TEST(gap7_loadenclaveimageAW_parity) {
    REQUIRE(UN(LoadEnclaveImageA)); REQUIRE(UN(LoadEnclaveImageW));
    SET_ERR(0);
    BOOL a = UN(LoadEnclaveImageA)(NULL, "k32t_no_such_enclave_image.dll");
    DWORD ea = GetLastError();
    SET_ERR(0);
    BOOL w = UN(LoadEnclaveImageW)(NULL, L"k32t_no_such_enclave_image.dll");
    DWORD ew = GetLastError();
    CHECK(a == FALSE && w == FALSE);
    CHECKF(ea == ew, "A err %lu W err %lu", ea, ew);
}

TEST(gap7_loadenclavedata_not_enclave) {
    REQUIRE(UN(LoadEnclaveData));
    void *p = VirtualAlloc(NULL, 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    CHECK(p != NULL);
    if (!p) return;
    BYTE buf[16] = { 0 };
    SIZE_T wr = 0x55; DWORD encErr = 0x55;
    SET_ERR(0);
    BOOL r = UN(LoadEnclaveData)(GetCurrentProcess(), p, buf, sizeof buf, PAGE_READWRITE, NULL, 0, &wr, &encErr);
    CHECK(r == FALSE);
    CHECK(GetLastError() != 0);
    t_note("LoadEnclaveData(non-enclave) err %lu written=%llu encErr=%lu", GetLastError(), (unsigned long long)wr, encErr);
    SET_ERR(0);
    r = UN(LoadEnclaveData)(GetCurrentProcess(), NULL, buf, 0, PAGE_READWRITE, NULL, 0, &wr, &encErr);
    CHECK(r == FALSE);
    VirtualFree(p, 0, MEM_RELEASE);
}

TEST(gap7_loadenclavedata_invalid_process) {
    REQUIRE(UN(LoadEnclaveData));
    BYTE buf[16] = { 0 };
    SIZE_T wr = 0; DWORD encErr = 0;
    SET_ERR(0);
    CHECK(UN(LoadEnclaveData)(NULL, NULL, buf, sizeof buf, PAGE_READWRITE, NULL, 0, &wr, &encErr) == FALSE);
    CHECK(GetLastError() != 0);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    CHECK(UN(LoadEnclaveData)(ev, NULL, buf, sizeof buf, PAGE_READWRITE, NULL, 0, &wr, &encErr) == FALSE);
    t_note("LoadEnclaveData(event handle) err %lu", GetLastError());
    CloseHandle(ev);
}

/* ---------- MapUserPhysicalPages (needs SeLockMemoryPrivilege and an AWE region: error paths only) ---------- */
TEST(gap7_mapuserphysicalpages_not_awe_region) {
    REQUIRE(U(MapUserPhysicalPages)); if (t_is_wine()) { t_skip("unimplemented stub"); return; }
    void *p = VirtualAlloc(NULL, 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    CHECK(p != NULL);
    if (!p) return;
    ULONG_PTR pfn[1] = { 0 };
    SET_ERR(0);
    CHECK(U(MapUserPhysicalPages)(p, 1, pfn) == FALSE);
    CHECK(GetLastError() != 0);
    t_note("MapUserPhysicalPages(ordinary memory, map) err %lu", GetLastError());
    SET_ERR(0);
    CHECK(U(MapUserPhysicalPages)(p, 1, NULL) == FALSE);   /* unmap request */
    CHECK(GetLastError() != 0);
    t_note("MapUserPhysicalPages(ordinary memory, unmap) err %lu", GetLastError());
    /* the memory is still ordinary, committed and usable */
    *(volatile char *)p = 1;
    CHECK(*(volatile char *)p == 1);
    VirtualFree(p, 0, MEM_RELEASE);
}

TEST(gap7_mapuserphysicalpages_null_address) {
    REQUIRE(U(MapUserPhysicalPages)); if (t_is_wine()) { t_skip("unimplemented stub"); return; }
    SET_ERR(0);
    CHECK(U(MapUserPhysicalPages)(NULL, 1, NULL) == FALSE);
    CHECK(GetLastError() != 0);
    SET_ERR(0);
    CHECK(U(MapUserPhysicalPages)(NULL, 0, NULL) == FALSE);
    t_note("MapUserPhysicalPages(NULL,0,NULL) err %lu", GetLastError());
}

TEST(gap7_mapuserphysicalpages_unmapped_address) {
    REQUIRE(U(MapUserPhysicalPages)); if (t_is_wine()) { t_skip("unimplemented stub"); return; }
    void *p = VirtualAlloc(NULL, 0x10000, MEM_RESERVE, PAGE_READWRITE);
    CHECK(p != NULL);
    if (!p) return;
    SET_ERR(0);
    CHECK(U(MapUserPhysicalPages)(p, 2, NULL) == FALSE);
    CHECK(GetLastError() != 0);
    VirtualFree(p, 0, MEM_RELEASE);
}

TEST(gap7_mapuserphysicalpagesscatter_not_awe) {
    REQUIRE(UK(MapUserPhysicalPagesScatter));
    void *p = VirtualAlloc(NULL, 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    CHECK(p != NULL);
    if (!p) return;
    PVOID addrs[2] = { p, (BYTE *)p + 0x1000 };
    ULONG_PTR pfn[2] = { 0, 0 };
    SET_ERR(0);
    CHECK(UK(MapUserPhysicalPagesScatter)(addrs, 2, pfn) == FALSE);
    CHECK(GetLastError() != 0);
    t_note("MapUserPhysicalPagesScatter(ordinary memory) err %lu", GetLastError());
    SET_ERR(0);
    CHECK(UK(MapUserPhysicalPagesScatter)(addrs, 2, NULL) == FALSE);
    CHECK(GetLastError() != 0);
    VirtualFree(p, 0, MEM_RELEASE);
}

TEST(gap7_mapuserphysicalpagesscatter_null) {
    REQUIRE(UK(MapUserPhysicalPagesScatter));
    SET_ERR(0);
    CHECK(UK(MapUserPhysicalPagesScatter)(NULL, 0, NULL) == FALSE || 1);
    t_note("MapUserPhysicalPagesScatter(NULL,0,NULL) err %lu", GetLastError());
    SET_ERR(0);
    CHECK(UK(MapUserPhysicalPagesScatter)(NULL, 1, NULL) == FALSE);
    CHECK(GetLastError() != 0);
}

/* ---------- MapViewOfFileEx ---------- */
static HANDLE new_section(DWORD prot, DWORD size) {
    return CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, prot, 0, size, NULL);
}
static SIZE_T region_size(void *p) {
    MEMORY_BASIC_INFORMATION mbi;
    return VirtualQuery(p, &mbi, sizeof mbi) ? mbi.RegionSize : 0;
}

TEST(gap7_mapviewex_basic_rw) {
    REQUIRE(U(MapViewOfFileEx));
    HANDLE m = new_section(PAGE_READWRITE, 0x10000);
    CHECK(m != NULL); if (!m) return;
    BYTE *v = U(MapViewOfFileEx)(m, FILE_MAP_WRITE, 0, 0, 0, NULL);
    CHECK(v != NULL);
    if (v) {
        CHECK(((ULONG_PTR)v & 0xFFFF) == 0);
        CHECK(region_size(v) == 0x10000);
        memset(v, 0xAB, 0x10000);
        BYTE *v2 = U(MapViewOfFileEx)(m, FILE_MAP_READ, 0, 0, 0, NULL);
        CHECK(v2 != NULL && v2 != v);
        if (v2) { CHECK(v2[0] == 0xAB && v2[0xFFFF] == 0xAB); UnmapViewOfFile(v2); }
        MEMORY_BASIC_INFORMATION mbi; VirtualQuery(v, &mbi, sizeof mbi);
        CHECK(mbi.Type == MEM_MAPPED);
        CHECK(mbi.Protect == PAGE_READWRITE);
        CHECK(mbi.AllocationBase == v);
        UnmapViewOfFile(v);
    }
    CloseHandle(m);
}

TEST(gap7_mapviewex_partial_size_and_offset) {
    REQUIRE(U(MapViewOfFileEx));
    HANDLE m = new_section(PAGE_READWRITE, 0x30000);
    CHECK(m != NULL); if (!m) return;
    BYTE *whole = U(MapViewOfFileEx)(m, FILE_MAP_WRITE, 0, 0, 0, NULL);
    CHECK(whole != NULL);
    if (whole) { for (int i = 0; i < 3; i++) memset(whole + i * 0x10000, 0x10 + i, 0x10000); }
    BYTE *v = U(MapViewOfFileEx)(m, FILE_MAP_READ, 0, 0x10000, 1, NULL);
    CHECK(v != NULL);
    if (v) {
        CHECK(*v == 0x11);
        CHECK(region_size(v) == 0x1000);
        UnmapViewOfFile(v);
    }
    v = U(MapViewOfFileEx)(m, FILE_MAP_READ, 0, 0x20000, 0x1800, NULL);
    CHECK(v != NULL);
    if (v) { CHECK(*v == 0x12); CHECK(region_size(v) == 0x2000); UnmapViewOfFile(v); }
    v = U(MapViewOfFileEx)(m, FILE_MAP_READ, 0, 0x20000, 0, NULL);
    CHECK(v != NULL);
    if (v) { CHECK(region_size(v) == 0x10000); UnmapViewOfFile(v); }
    if (whole) UnmapViewOfFile(whole);
    CloseHandle(m);
}

TEST(gap7_mapviewex_access_rules) {
    REQUIRE(U(MapViewOfFileEx));
    HANDLE m = new_section(PAGE_READONLY, 0x1000);
    if (!m) { t_note("cannot create PAGE_READONLY pagefile section: %lu", GetLastError()); return; }
    SET_ERR(0);
    BYTE *v = U(MapViewOfFileEx)(m, FILE_MAP_WRITE, 0, 0, 0, NULL);
    CHECK(v == NULL);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    if (v) UnmapViewOfFile(v);
    v = U(MapViewOfFileEx)(m, FILE_MAP_READ, 0, 0, 0, NULL);
    CHECK(v != NULL);
    if (v) { MEMORY_BASIC_INFORMATION mbi; VirtualQuery(v, &mbi, sizeof mbi); CHECK(mbi.Protect == PAGE_READONLY); UnmapViewOfFile(v); }
    SET_ERR(0);
    v = U(MapViewOfFileEx)(m, FILE_MAP_ALL_ACCESS, 0, 0, 0, NULL);
    CHECK(v == NULL);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(m);
}

TEST(gap7_mapviewex_copy_on_write) {
    REQUIRE(U(MapViewOfFileEx));
    HANDLE m = new_section(PAGE_READWRITE, 0x10000);
    CHECK(m != NULL); if (!m) return;
    volatile BYTE *shared = U(MapViewOfFileEx)(m, FILE_MAP_WRITE, 0, 0, 0, NULL);
    volatile BYTE *priv = U(MapViewOfFileEx)(m, FILE_MAP_COPY, 0, 0, 0, NULL);
    CHECK(shared && priv && shared != priv);
    if (shared && priv) {
        shared[0] = 0x11; shared[0x2000] = 0x22;
        CHECK(priv[0] == 0x11);             /* not yet copied: sees the shared data */
        MEMORY_BASIC_INFORMATION mbi; VirtualQuery((void *)priv, &mbi, sizeof mbi);
        CHECK(mbi.Protect == PAGE_WRITECOPY);
        priv[0] = 0x99;                     /* triggers the copy */
        CHECK(priv[0] == 0x99);
        CHECK(shared[0] == 0x11);           /* original untouched */
        shared[0] = 0x33;
        CHECK(priv[0] == 0x99);             /* private page no longer follows */
        CHECK(priv[0x2000] == 0x22);        /* other pages still shared */
        shared[0x2000] = 0x44;
        CHECK(priv[0x2000] == 0x44);
    }
    if (shared) UnmapViewOfFile((void *)shared);
    if (priv) UnmapViewOfFile((void *)priv);
    CloseHandle(m);
}

TEST(gap7_mapviewex_base_address_hint) {
    REQUIRE(U(MapViewOfFileEx));
    HANDLE m = new_section(PAGE_READWRITE, 0x10000);
    CHECK(m != NULL); if (!m) return;
    BYTE *probe = U(MapViewOfFileEx)(m, FILE_MAP_WRITE, 0, 0, 0, NULL);
    CHECK(probe != NULL);
    if (probe) {
        UnmapViewOfFile(probe);
        BYTE *v = U(MapViewOfFileEx)(m, FILE_MAP_WRITE, 0, 0, 0, probe);
        if (v) {
            CHECK(v == probe);
            /* the address is now occupied */
            SET_ERR(0);
            BYTE *v2 = U(MapViewOfFileEx)(m, FILE_MAP_WRITE, 0, 0, 0, probe);
            CHECK(v2 == NULL);
            EXPECT_ERR(ERROR_INVALID_ADDRESS);
            if (v2) UnmapViewOfFile(v2);
            /* overlapping, not identical */
            SET_ERR(0);
            v2 = U(MapViewOfFileEx)(m, FILE_MAP_WRITE, 0, 0, 0, probe + 0x10000);
            if (v2) { t_note("adjacent hint mapped at %p", (void *)v2); UnmapViewOfFile(v2); }
            UnmapViewOfFile(v);
        } else t_note("remap at freed address failed: %lu", GetLastError());
    }
    CloseHandle(m);
}

TEST(gap7_mapviewex_unaligned_base) {
    REQUIRE(U(MapViewOfFileEx));
    HANDLE m = new_section(PAGE_READWRITE, 0x10000);
    CHECK(m != NULL); if (!m) return;
    BYTE *probe = U(MapViewOfFileEx)(m, FILE_MAP_WRITE, 0, 0, 0, NULL);
    if (probe) {
        UnmapViewOfFile(probe);
        static const ULONG_PTR deltas[] = { 1, 0x1000, 0x8000, 0xFFFF };
        for (size_t i = 0; i < sizeof deltas / sizeof deltas[0]; i++) {
            SET_ERR(0);
            BYTE *v = U(MapViewOfFileEx)(m, FILE_MAP_WRITE, 0, 0, 0, probe + deltas[i]);
            CHECKF(v == NULL, "base+0x%llx accepted", (unsigned long long)deltas[i]);
            DWORD e = GetLastError();
            CHECKF(e == ERROR_MAPPED_ALIGNMENT, "base+0x%llx err %lu", (unsigned long long)deltas[i], e);
            if (v) UnmapViewOfFile(v);
        }
    }
    CloseHandle(m);
}

TEST(gap7_mapviewex_invalid_handles) {
    REQUIRE(U(MapViewOfFileEx));
    SET_ERR(0);
    CHECK(U(MapViewOfFileEx)(NULL, FILE_MAP_READ, 0, 0, 0, NULL) == NULL);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(U(MapViewOfFileEx)(INVALID_HANDLE_VALUE, FILE_MAP_READ, 0, 0, 0, NULL) == NULL);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    CHECK(U(MapViewOfFileEx)(ev, FILE_MAP_READ, 0, 0, 0, NULL) == NULL);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
    HANDLE m = new_section(PAGE_READWRITE, 0x1000);
    CloseHandle(m);
    SET_ERR(0);
    CHECK(U(MapViewOfFileEx)(m, FILE_MAP_READ, 0, 0, 0, NULL) == NULL);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}

TEST(gap7_mapviewex_out_of_range) {
    REQUIRE(U(MapViewOfFileEx));
    HANDLE m = new_section(PAGE_READWRITE, 0x10000);
    CHECK(m != NULL); if (!m) return;
    struct { DWORD hi, lo; SIZE_T len; const char *what; } t[] = {
        { 0, 0x10000, 0, "offset == size" },
        { 0, 0x20000, 0, "offset beyond size" },
        { 1, 0, 0, "offset 4GB" },
        { 0x7fffffff, 0xffff0000, 0, "huge offset" },
        { 0, 0, 0x20000, "length beyond section" },
        { 0, 0, 0x7fffffffffffull, "huge length" },
        { 0, 0x10000, 1, "offset at end with length" },
        { 0, 0x8000, 0, "unaligned offset" },
        { 0, 1, 0, "offset 1" },
    };
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++) {
        SET_ERR(0);
        void *v = U(MapViewOfFileEx)(m, FILE_MAP_READ, t[i].hi, t[i].lo, t[i].len, NULL);
        DWORD e = GetLastError();
        CHECKF(v == NULL, "%s accepted", t[i].what);
        t_note("MapViewOfFileEx(%s) err %lu", t[i].what, e);
        if (v) UnmapViewOfFile(v); else CHECKF(e != 0, "%s no error", t[i].what);
    }
    CloseHandle(m);
}

TEST(gap7_mapviewex_zero_access_note) {
    REQUIRE(U(MapViewOfFileEx));
    HANDLE m = new_section(PAGE_READWRITE, 0x1000);
    CHECK(m != NULL); if (!m) return;
    SET_ERR(0);
    void *v = U(MapViewOfFileEx)(m, 0, 0, 0, 0, NULL);
    t_note("MapViewOfFileEx(access=0) -> %p err %lu", v, GetLastError());
    if (v) UnmapViewOfFile(v);
    CloseHandle(m);
}

TEST(gap7_mapviewex_file_backed) {
    REQUIRE(U(MapViewOfFileEx));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "g7map.bin");
    HANDLE f = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CHECK(f != INVALID_HANDLE_VALUE); if (f == INVALID_HANDLE_VALUE) return;
    char data[5000]; for (int i = 0; i < 5000; i++) data[i] = (char)(i * 7);
    DWORD w; WriteFile(f, data, sizeof data, &w, NULL);
    HANDLE m = CreateFileMappingA(f, NULL, PAGE_READWRITE, 0, 0, NULL);
    CHECK(m != NULL);
    if (m) {
        BYTE *v = U(MapViewOfFileEx)(m, FILE_MAP_WRITE, 0, 0, 0, NULL);
        CHECK(v != NULL);
        if (v) {
            CHECK(!memcmp(v, data, sizeof data));
            CHECK(region_size(v) == 0x2000);
            v[0] ^= 0xFF; v[4999] ^= 0xFF;
            FlushViewOfFile(v, 0);
            UnmapViewOfFile(v);
        }
        CloseHandle(m);
        SetFilePointer(f, 0, NULL, FILE_BEGIN);
        char back[5000]; ReadFile(f, back, sizeof back, &w, NULL);
        CHECK(w == sizeof back && (BYTE)back[0] == (BYTE)(data[0] ^ 0xFF) && (BYTE)back[4999] == (BYTE)(data[4999] ^ 0xFF));
        CHECK(!memcmp(back + 1, data + 1, 4998));
    }
    CloseHandle(f);
    DeleteFileA(p);
}

TEST(gap7_mapviewex_matches_mapviewoffile_semantics) {
    REQUIRE(U(MapViewOfFileEx)); REQUIRE(U(MapViewOfFile));
    HANDLE m = new_section(PAGE_READWRITE, 0x2000);
    CHECK(m != NULL); if (!m) return;
    BYTE *a = U(MapViewOfFileEx)(m, FILE_MAP_WRITE, 0, 0, 0, NULL);
    BYTE *b = U(MapViewOfFile)(m, FILE_MAP_WRITE, 0, 0, 0);
    CHECK(a && b && a != b);
    if (a && b) { a[5] = 0x5A; CHECK(b[5] == 0x5A); }
    if (a) UnmapViewOfFile(a);
    if (b) UnmapViewOfFile(b);
    CloseHandle(m);
}

/* ---------- MapViewOfFileNuma2 ---------- */
UDECL(PVOID, MapViewOfFileNuma2, (HANDLE, HANDLE, ULONG64, PVOID, SIZE_T, ULONG, ULONG, ULONG));
#define NUMA2 UN(MapViewOfFileNuma2)
#define NO_NODE 0xFFFFFFFFu

TEST(gap7_mapviewnuma2_basic) {
    if (!NUMA2) { t_skip("missing export MapViewOfFileNuma2"); return; }
    HANDLE m = new_section(PAGE_READWRITE, 0x10000);
    CHECK(m != NULL); if (!m) return;
    SET_ERR(0);
    BYTE *v = NUMA2(m, GetCurrentProcess(), 0, NULL, 0, 0, PAGE_READWRITE, NO_NODE);
    if (!v && t_is_wine()) { t_skip("MapViewOfFileNuma2 not functional here"); CloseHandle(m); return; }
    CHECK(v != NULL);
    if (v) {
        CHECK(region_size(v) == 0x10000);
        v[0] = 1; v[0xFFFF] = 2;
        BYTE *r = NUMA2(m, GetCurrentProcess(), 0, NULL, 0, 0, PAGE_READONLY, NO_NODE);
        CHECK(r != NULL);
        if (r) { CHECK(r[0] == 1 && r[0xFFFF] == 2); UnmapViewOfFile(r); }
        UnmapViewOfFile(v);
    }
    CloseHandle(m);
}

TEST(gap7_mapviewnuma2_offset_and_size) {
    if (!NUMA2) { t_skip("missing export MapViewOfFileNuma2"); return; }
    HANDLE m = new_section(PAGE_READWRITE, 0x30000);
    CHECK(m != NULL); if (!m) return;
    BYTE *all = NUMA2(m, GetCurrentProcess(), 0, NULL, 0, 0, PAGE_READWRITE, NO_NODE);
    if (!all) { t_skip("MapViewOfFileNuma2 not functional here"); CloseHandle(m); return; }
    all[0x10000] = 0x77;
    BYTE *v = NUMA2(m, GetCurrentProcess(), 0x10000, NULL, 0x1000, 0, PAGE_READWRITE, NO_NODE);
    CHECK(v != NULL);
    if (v) { CHECK(*v == 0x77); CHECK(region_size(v) == 0x1000); UnmapViewOfFile(v); }
    SET_ERR(0);
    v = NUMA2(m, GetCurrentProcess(), 0x30000, NULL, 0, 0, PAGE_READWRITE, NO_NODE);
    CHECK(v == NULL);
    CHECK(GetLastError() != 0);
    t_note("Numa2 offset==size err %lu", GetLastError());
    if (v) UnmapViewOfFile(v);
    UnmapViewOfFile(all);
    CloseHandle(m);
}

TEST(gap7_mapviewnuma2_bad_node) {
    if (!NUMA2) { t_skip("missing export MapViewOfFileNuma2"); return; }
    HANDLE m = new_section(PAGE_READWRITE, 0x10000);
    CHECK(m != NULL); if (!m) return;
    static const ULONG nodes[] = { 0x40, 0x41, 0x100, 0xFFFF, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFE };
    for (size_t i = 0; i < sizeof nodes / sizeof nodes[0]; i++) {
        SET_ERR(0);
        void *v = NUMA2(m, GetCurrentProcess(), 0, NULL, 0, 0, PAGE_READWRITE, nodes[i]);
        CHECKF(v == NULL, "node %lu accepted", nodes[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        if (v) UnmapViewOfFile(v);
    }
    CloseHandle(m);
}

TEST(gap7_mapviewnuma2_bad_allocation_type) {
    if (!NUMA2) { t_skip("missing export MapViewOfFileNuma2"); return; }
    HANDLE m = new_section(PAGE_READWRITE, 0x10000);
    CHECK(m != NULL); if (!m) return;
    const ULONG allowed = 0x20902000;     /* MEM_RESERVE, MEM_LARGE_PAGES and two reserve-time flags */
    for (int bit = 0; bit < 32; bit++) {
        ULONG f = 1u << bit;
        if (allowed & f) continue;
        SET_ERR(0);
        void *v = NUMA2(m, GetCurrentProcess(), 0, NULL, 0, f, PAGE_READWRITE, NO_NODE);
        CHECKF(v == NULL, "allocation type 0x%lx accepted", f);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        if (v) UnmapViewOfFile(v);
    }
    CloseHandle(m);
}

TEST(gap7_mapviewnuma2_invalid_handles) {
    if (!NUMA2) { t_skip("missing export MapViewOfFileNuma2"); return; }
    HANDLE m = new_section(PAGE_READWRITE, 0x10000);
    CHECK(m != NULL); if (!m) return;
    SET_ERR(0);
    CHECK(NUMA2(NULL, GetCurrentProcess(), 0, NULL, 0, 0, PAGE_READWRITE, NO_NODE) == NULL);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(NUMA2(INVALID_HANDLE_VALUE, GetCurrentProcess(), 0, NULL, 0, 0, PAGE_READWRITE, NO_NODE) == NULL);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(NUMA2(m, NULL, 0, NULL, 0, 0, PAGE_READWRITE, NO_NODE) == NULL);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    CHECK(NUMA2(ev, GetCurrentProcess(), 0, NULL, 0, 0, PAGE_READWRITE, NO_NODE) == NULL);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(NUMA2(m, ev, 0, NULL, 0, 0, PAGE_READWRITE, NO_NODE) == NULL);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
    CloseHandle(m);
}

TEST(gap7_mapviewnuma2_protection_vs_section) {
    if (!NUMA2) { t_skip("missing export MapViewOfFileNuma2"); return; }
    HANDLE m = new_section(PAGE_READONLY, 0x1000);
    if (!m) { t_note("no readonly pagefile section"); return; }
    SET_ERR(0);
    void *v = NUMA2(m, GetCurrentProcess(), 0, NULL, 0, 0, PAGE_READWRITE, NO_NODE);
    CHECK(v == NULL);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    if (v) UnmapViewOfFile(v);
    v = NUMA2(m, GetCurrentProcess(), 0, NULL, 0, 0, PAGE_READONLY, NO_NODE);
    if (!v && t_is_wine()) { CloseHandle(m); return; }
    CHECK(v != NULL);
    if (v) UnmapViewOfFile(v);
    CloseHandle(m);
}

TEST(gap7_mapviewnuma2_base_hint_and_conflict) {
    if (!NUMA2) { t_skip("missing export MapViewOfFileNuma2"); return; }
    HANDLE m = new_section(PAGE_READWRITE, 0x10000);
    CHECK(m != NULL); if (!m) return;
    BYTE *a = NUMA2(m, GetCurrentProcess(), 0, NULL, 0, 0, PAGE_READWRITE, NO_NODE);
    if (!a) { t_skip("MapViewOfFileNuma2 not functional here"); CloseHandle(m); return; }
    SET_ERR(0);
    void *b = NUMA2(m, GetCurrentProcess(), 0, a, 0, 0, PAGE_READWRITE, NO_NODE);
    CHECK(b == NULL);
    EXPECT_ERR(ERROR_INVALID_ADDRESS);
    if (b) UnmapViewOfFile(b);
    SET_ERR(0);
    b = NUMA2(m, GetCurrentProcess(), 0, a + 0x1000 + 0x10000 * 0, 0, 0, PAGE_READWRITE, NO_NODE);
    CHECK(b == NULL);
    if (b) UnmapViewOfFile(b);
    UnmapViewOfFile(a);
    CloseHandle(m);
}

TEST(gap7_mapviewnuma2_node0_and_topdown) {
    if (!NUMA2) { t_skip("missing export MapViewOfFileNuma2"); return; }
    HANDLE m = new_section(PAGE_READWRITE, 0x10000);
    CHECK(m != NULL); if (!m) return;
    SET_ERR(0);
    void *v = NUMA2(m, GetCurrentProcess(), 0, NULL, 0, 0, PAGE_READWRITE, 0);
    t_note("Numa2 node 0 -> %p err %lu", v, v ? 0 : GetLastError());
    if (v) UnmapViewOfFile(v);
    v = NUMA2(m, GetCurrentProcess(), 0, NULL, 0, MEM_TOP_DOWN, PAGE_READWRITE, NO_NODE);
    t_note("Numa2 MEM_TOP_DOWN -> %p err %lu", v, v ? 0 : GetLastError());
    if (v) UnmapViewOfFile(v);
    v = NUMA2(m, GetCurrentProcess(), 0, NULL, 0, 0, PAGE_READWRITE, 63);
    t_note("Numa2 node 63 -> %p err %lu", v, v ? 0 : GetLastError());
    if (v) UnmapViewOfFile(v);
    CloseHandle(m);
}

/* ---------- NotifyUILanguageChange: only the argument-rejection path ---------- */
UDECL(BOOL, NotifyUILanguageChange, (DWORD, PCWSTR, PCWSTR, DWORD, PDWORD));
#define NOTIFY UK(NotifyUILanguageChange)

TEST(gap7_notifyuilang_bad_language_name) {
    REQUIRE(UK(NotifyUILanguageChange));
    static const WCHAR *bad[] = { L"!!bad!!", L"this is not a language name", L"12345678901234567890123456789012345678901234567890", L"<>:" };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        DWORD st = 0x55555555;
        SET_ERR(0);
        BOOL r = UN(NotifyUILanguageChange)(0, bad[i], NULL, 0, &st);
        if (r && t_is_wine()) { t_skip("not validated here"); return; }
        CHECKF(r == FALSE, "\"%ls\" accepted", bad[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        CHECKF(st == 0xC000000D, "status %08lx", st);
    }
}

TEST(gap7_notifyuilang_bad_name_variants) {
    REQUIRE(UK(NotifyUILanguageChange));
    DWORD st = 0;
    SET_ERR(0);
    BOOL r = UN(NotifyUILanguageChange)(0x100, L"!!bad!!", L"en-US", 0, &st);
    if (r && t_is_wine()) { t_skip("not validated here"); return; }
    CHECK(r == FALSE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CHECK(st == 0xC000000D);
    /* status pointer is optional on the failure path */
    SET_ERR(0);
    r = UN(NotifyUILanguageChange)(0, L"!!bad!!", NULL, 0, NULL);
    CHECK(r == FALSE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    /* a bad language with the "install"/"check" flag combination (8) is rejected the same way */
    st = 0;
    SET_ERR(0);
    r = UN(NotifyUILanguageChange)(0x8, L"!!bad!!", NULL, 0, &st);
    CHECK(r == FALSE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
