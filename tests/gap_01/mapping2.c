/* CreateFileMapping2 */
#include "gap1.h"

UDECL(HANDLE, CreateFileMapping2, (HANDLE File, SECURITY_ATTRIBUTES *SecurityAttributes, ULONG DesiredAccess, ULONG PageProtection, ULONG AllocationAttributes, ULONG64 MaximumSize, PCWSTR Name, MEM_EXTENDED_PARAMETER *ExtendedParameters, ULONG ParameterCount));
#define CFM2 UN(CreateFileMapping2)
static void g1_name(wchar_t *out, size_t n, const char *tag) { _snwprintf(out, n, L"Local\\k32t_gap1_%lu_%hs", GetCurrentProcessId(), tag); }
static HANDLE g1_pagefile_map(ULONG prot, ULONG64 size) {
    return CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_ALL_ACCESS, prot, 0, size, NULL, NULL, 0);
}
static HANDLE g1_mkfile(const char *leaf, const void *data, DWORD n, DWORD access) {
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, leaf);
    HANDLE h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return h;
    DWORD w; if (n) WriteFile(h, data, n, &w, NULL);
    if (access != (GENERIC_READ | GENERIC_WRITE)) {
        CloseHandle(h);
        h = CreateFileA(p, access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }
    return h;
}

/* ---------- allocation attribute validation (user-mode, before the system call) ---------- */
TEST(gap1_cfm2_invalid_alloc_attributes) {
    REQUIRE(CFM2);
    static const ULONG bad[] = { 0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000,
        0x10000, 0x20000, 0x40000, 0x100000, 0x200000, 0x400000, 0x2000000, 0x20000000,
        0x2277FFFF, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SET_ERR(0);
        HANDLE h = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, bad[i], 4096, NULL, NULL, 0);
        CHECKF(h == NULL, "attr 0x%lx accepted", bad[i]);
        if (h) { CloseHandle(h); continue; }
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "attr 0x%lx err %lu", bad[i], GetLastError());
    }
}
TEST(gap1_cfm2_invalid_alloc_attr_each_bit) {
    REQUIRE(CFM2);
    /* every bit set in the rejection mask 0x2277FFFF individually */
    for (int b = 0; b < 32; b++) {
        ULONG m = 1u << b;
        if (!(0x2277FFFFu & m)) continue;
        SET_ERR(0);
        HANDLE h = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, m, 4096, NULL, NULL, 0);
        CHECKF(h == NULL && GetLastError() == ERROR_INVALID_PARAMETER, "bit %d: h=%p err=%lu", b, h, GetLastError());
        if (h) CloseHandle(h);
    }
}
TEST(gap1_cfm2_invalid_attr_rejected_before_handle_validation) {
    REQUIRE(CFM2);
    /* the flag check precedes any handle use */
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    HANDLE h = CFM2(ev, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0x1, 4096, NULL, NULL, 0);
    CHECK(h == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CloseHandle(ev);
}
TEST(gap1_cfm2_attr_zero_defaults_to_commit) {
    REQUIRE(CFM2);
    HANDLE h = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 8192, NULL, NULL, 0);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (!h) return;
    void *v = MapViewOfFile(h, FILE_MAP_WRITE, 0, 0, 0);
    CHECK(v != NULL);
    if (v) {
        MEMORY_BASIC_INFORMATION mbi;
        CHECK(VirtualQuery(v, &mbi, sizeof mbi));
        EXPECT_EQ(mbi.State, MEM_COMMIT);
        EXPECT_EQ(mbi.Type, MEM_MAPPED);
        EXPECT_EQ(mbi.RegionSize, 8192);
        UnmapViewOfFile(v);
    }
    CloseHandle(h);
}
TEST(gap1_cfm2_explicit_sec_commit_same_as_zero) {
    REQUIRE(CFM2);
    HANDLE h = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, SEC_COMMIT, 4096, NULL, NULL, 0);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (h) {
        void *v = MapViewOfFile(h, FILE_MAP_WRITE, 0, 0, 0);
        CHECK(v != NULL);
        if (v) { MEMORY_BASIC_INFORMATION mbi; VirtualQuery(v, &mbi, sizeof mbi); EXPECT_EQ(mbi.State, MEM_COMMIT); UnmapViewOfFile(v); }
        CloseHandle(h);
    }
}
TEST(gap1_cfm2_sec_reserve_view) {
    REQUIRE(CFM2);
    HANDLE h = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, SEC_RESERVE, 1 << 20, NULL, NULL, 0);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (!h) return;
    void *v = MapViewOfFile(h, FILE_MAP_WRITE, 0, 0, 0);
    if (v) {
        MEMORY_BASIC_INFORMATION mbi;
        VirtualQuery(v, &mbi, sizeof mbi);
        t_note("SEC_RESERVE view state=0x%lx type=0x%lx size=%llu", mbi.State, mbi.Type, (unsigned long long)mbi.RegionSize);
        EXPECT_EQ(mbi.Type, MEM_MAPPED);
        UnmapViewOfFile(v);
    } else t_note("MapViewOfFile on SEC_RESERVE section failed %lu", GetLastError());
    CloseHandle(h);
}
TEST(gap1_cfm2_sec_reserve_and_commit_together) {
    REQUIRE(CFM2);
    SET_ERR(0);
    HANDLE h = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, SEC_RESERVE | SEC_COMMIT, 4096, NULL, NULL, 0);
    t_note("SEC_RESERVE|SEC_COMMIT: h=%p err=%lu", h, GetLastError());
    if (h) CloseHandle(h);
}
TEST(gap1_cfm2_sec_image_on_pagefile) {
    REQUIRE(CFM2);
    SET_ERR(0);
    HANDLE h = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_ALL_ACCESS, PAGE_READONLY, SEC_IMAGE, 4096, NULL, NULL, 0);
    CHECK(h == NULL);
    t_note("SEC_IMAGE on pagefile: err=%lu", GetLastError());
    if (h) CloseHandle(h);
}
TEST(gap1_cfm2_sec_nocache_writecombine_accepted_by_filter) {
    REQUIRE(CFM2);
    static const ULONG a[] = { SEC_NOCACHE, SEC_WRITECOMBINE, SEC_64K_PAGES };
    for (size_t i = 0; i < 3; i++) {
        SET_ERR(0);
        HANDLE h = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, a[i] | SEC_COMMIT, 65536, NULL, NULL, 0);
        t_note("attr 0x%lx|COMMIT: h=%p err=%lu", a[i], h, GetLastError());
        if (h) CloseHandle(h);
    }
}

/* ---------- extended parameter validation ---------- */
TEST(gap1_cfm2_extparam_invalid_types) {
    REQUIRE(CFM2);
    static const unsigned types[] = { 0, 3, 4, 5, 6, 7, 8, 0x7F, 0x80, 0xFE, 0xFF };
    for (size_t i = 0; i < sizeof types / sizeof types[0]; i++) {
        MEM_EXTENDED_PARAMETER p; memset(&p, 0, sizeof p);
        p.Type = types[i]; p.ULong64 = 0;
        SET_ERR(0);
        HANDLE h = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 4096, NULL, &p, 1);
        CHECKF(h == NULL, "type %u accepted", types[i]);
        if (h) { CloseHandle(h); continue; }
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "type %u err %lu", types[i], GetLastError());
    }
}
TEST(gap1_cfm2_extparam_invalid_second_element) {
    REQUIRE(CFM2);
    MEM_EXTENDED_PARAMETER p[3]; memset(p, 0, sizeof p);
    p[0].Type = MemExtendedParameterNumaNode; p[0].ULong = 0;
    p[1].Type = MemExtendedParameterNumaNode; p[1].ULong = 0;
    p[2].Type = MemExtendedParameterInvalidType;
    SET_ERR(0);
    HANDLE h = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 4096, NULL, p, 3);
    CHECK(h == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    if (h) CloseHandle(h);
}
TEST(gap1_cfm2_extparam_only_first_count_elements_checked) {
    REQUIRE(CFM2);
    MEM_EXTENDED_PARAMETER p[2]; memset(p, 0, sizeof p);
    p[0].Type = MemExtendedParameterNumaNode; p[0].ULong = 0;
    p[1].Type = 0xFF;                       /* beyond the declared count */
    SET_ERR(0);
    HANDLE h = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 4096, NULL, p, 1);
    t_note("numa node 0 (count 1): h=%p err=%lu", h, GetLastError());
    if (h) CloseHandle(h);
}
TEST(gap1_cfm2_extparam_zero_count_ignores_array) {
    REQUIRE(CFM2);
    MEM_EXTENDED_PARAMETER p; memset(&p, 0, sizeof p);
    p.Type = 0xFF;
    SET_ERR(0);
    HANDLE h = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 4096, NULL, &p, 0);
    /* P1: kernelbase skips its own check for count 0, but the kernel (MiCaptureSectionCreateExtendedParameters)
     * rejects a non-NULL array with count 0 as STATUS_INVALID_PARAMETER. UNVERIFIED-until-rerun. */
    CHECK(h == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    if (h) CloseHandle(h);
}
TEST(gap1_cfm2_extparam_null_array_zero_count) {
    REQUIRE(CFM2);
    HANDLE h = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 4096, NULL, NULL, 0);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (h) CloseHandle(h);
}
TEST(gap1_cfm2_extparam_numa_node_zero) {
    REQUIRE(CFM2);
    MEM_EXTENDED_PARAMETER p; memset(&p, 0, sizeof p);
    p.Type = MemExtendedParameterNumaNode; p.ULong = 0;
    SET_ERR(0);
    HANDLE h = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 4096, NULL, &p, 1);
    t_note("numa node 0: h=%p err=%lu", h, GetLastError());
    if (h) CloseHandle(h);
}
TEST(gap1_cfm2_extparam_numa_node_out_of_range) {
    REQUIRE(CFM2);
    MEM_EXTENDED_PARAMETER p; memset(&p, 0, sizeof p);
    p.Type = MemExtendedParameterNumaNode; p.ULong = 0xFFFFFF;
    SET_ERR(0);
    HANDLE h = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 4096, NULL, &p, 1);
    t_note("numa node 0xFFFFFF: h=%p err=%lu", h, GetLastError());
    if (h) CloseHandle(h);
}
TEST(gap1_cfm2_extparam_address_requirements) {
    REQUIRE(CFM2);
    MEM_ADDRESS_REQUIREMENTS req; memset(&req, 0, sizeof req);
    MEM_EXTENDED_PARAMETER p; memset(&p, 0, sizeof p);
    p.Type = MemExtendedParameterAddressRequirements; p.Pointer = &req;
    SET_ERR(0);
    HANDLE h = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 4096, NULL, &p, 1);
    t_note("address requirements param on section: h=%p err=%lu", h, GetLastError());
    if (h) CloseHandle(h);
}

/* ---------- size / handle semantics ---------- */
TEST(gap1_cfm2_pagefile_size_zero_fails) {
    REQUIRE(CFM2);
    SET_ERR(0);
    HANDLE h = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 0, NULL, NULL, 0);
    CHECK(h == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    if (h) CloseHandle(h);
}
TEST(gap1_cfm2_null_file_is_pagefile_backed) {
    REQUIRE(CFM2);
    HANDLE h = CFM2(NULL, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 4096, NULL, NULL, 0);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (!h) return;
    unsigned char *v = (unsigned char *)MapViewOfFile(h, FILE_MAP_WRITE, 0, 0, 0);
    CHECK(v != NULL);
    if (v) { int z = 1; for (int i = 0; i < 4096; i++) if (v[i]) z = 0; CHECK(z); UnmapViewOfFile(v); }
    CloseHandle(h);
}
TEST(gap1_cfm2_invalid_handle_value_is_pagefile_backed) {
    REQUIRE(CFM2);
    HANDLE h = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 4096, NULL, NULL, 0);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (h) CloseHandle(h);
}
TEST(gap1_cfm2_success_clears_last_error) {
    REQUIRE(CFM2);
    SET_ERR(0x1234);
    HANDLE h = g1_pagefile_map(PAGE_READWRITE, 4096);
    CHECK(h != NULL);
    EXPECT_ERR(0);
    if (h) CloseHandle(h);
}
TEST(gap1_cfm2_size_rounds_up_to_page) {
    REQUIRE(CFM2);
    static const ULONG64 sz[] = { 1, 100, 4095, 4096, 4097, 8191, 8192, 65536, 65537 };
    for (size_t i = 0; i < sizeof sz / sizeof sz[0]; i++) {
        HANDLE h = g1_pagefile_map(PAGE_READWRITE, sz[i]);
        CHECKF(h != NULL, "size %llu err %lu", sz[i], GetLastError());
        if (!h) continue;
        void *v = MapViewOfFile(h, FILE_MAP_WRITE, 0, 0, 0);
        CHECKF(v != NULL, "map size %llu", sz[i]);
        if (v) {
            MEMORY_BASIC_INFORMATION mbi; VirtualQuery(v, &mbi, sizeof mbi);
            SIZE_T want = (SIZE_T)((sz[i] + 4095) & ~4095ull);
            CHECKF(mbi.RegionSize == want, "size %llu region %llu want %llu", sz[i], (unsigned long long)mbi.RegionSize, (unsigned long long)want);
            UnmapViewOfFile(v);
        }
        CloseHandle(h);
    }
}
TEST(gap1_cfm2_absurd_size_fails) {
    REQUIRE(CFM2);
    SET_ERR(0);
    HANDLE h = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 0x7FFFFFFFFFFFFFFFull, NULL, NULL, 0);
    CHECK(h == NULL);
    DWORD e = GetLastError();
    t_note("size 0x7FFF.. -> err %lu", e);
    CHECK(e != 0);
    if (h) CloseHandle(h);
    SET_ERR(0);
    h = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 0xFFFFFFFFFFFFFFFFull, NULL, NULL, 0);
    CHECK(h == NULL);
    t_note("size ~0 -> err %lu", GetLastError());
    if (h) CloseHandle(h);
}
TEST(gap1_cfm2_two_views_alias) {
    REQUIRE(CFM2);
    HANDLE h = g1_pagefile_map(PAGE_READWRITE, 4096);
    if (!h) { CHECK(0); return; }
    char *a = (char *)MapViewOfFile(h, FILE_MAP_WRITE, 0, 0, 0);
    char *b = (char *)MapViewOfFile(h, FILE_MAP_READ, 0, 0, 0);
    CHECK(a && b && a != b);
    if (a && b) { strcpy(a, "gap1-data"); EXPECT_STR(b, "gap1-data"); }
    if (a) UnmapViewOfFile(a);
    if (b) UnmapViewOfFile(b);
    CloseHandle(h);
}
TEST(gap1_cfm2_data_persists_while_handle_open) {
    REQUIRE(CFM2);
    HANDLE h = g1_pagefile_map(PAGE_READWRITE, 4096);
    if (!h) { CHECK(0); return; }
    char *a = (char *)MapViewOfFile(h, FILE_MAP_WRITE, 0, 0, 0);
    if (a) { strcpy(a, "persist"); UnmapViewOfFile(a); }
    char *b = (char *)MapViewOfFile(h, FILE_MAP_READ, 0, 0, 0);
    CHECK(b != NULL);
    if (b) { EXPECT_STR(b, "persist"); UnmapViewOfFile(b); }
    CloseHandle(h);
}
TEST(gap1_cfm2_view_survives_handle_close) {
    REQUIRE(CFM2);
    HANDLE h = g1_pagefile_map(PAGE_READWRITE, 4096);
    if (!h) { CHECK(0); return; }
    char *a = (char *)MapViewOfFile(h, FILE_MAP_WRITE, 0, 0, 0);
    CHECK(a != NULL);
    CloseHandle(h);
    if (a) { strcpy(a, "alive"); EXPECT_STR(a, "alive"); UnmapViewOfFile(a); }
}

/* ---------- page protection vs view access ---------- */
TEST(gap1_cfm2_prot_readonly_blocks_write_view) {
    REQUIRE(CFM2);
    HANDLE h = g1_pagefile_map(PAGE_READONLY, 4096);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (!h) return;
    SET_ERR(0);
    void *w = MapViewOfFile(h, FILE_MAP_WRITE, 0, 0, 0);
    CHECK(w == NULL);
    /* P1: Win10 maps this to STATUS_SECTION_PROTECTION (MiMapViewOfSection) -> ERROR_INVALID_PARAMETER.
     * UNVERIFIED-until-rerun. */
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    if (w) UnmapViewOfFile(w);
    void *r = MapViewOfFile(h, FILE_MAP_READ, 0, 0, 0);
    CHECK(r != NULL);
    if (r) UnmapViewOfFile(r);
    CloseHandle(h);
}
TEST(gap1_cfm2_prot_writecopy_view) {
    REQUIRE(CFM2);
    HANDLE h = g1_pagefile_map(PAGE_WRITECOPY, 4096);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (!h) return;
    char *a = (char *)MapViewOfFile(h, FILE_MAP_COPY, 0, 0, 0);
    char *b = (char *)MapViewOfFile(h, FILE_MAP_READ, 0, 0, 0);
    CHECK(a && b);
    if (a && b) { a[0] = 'x'; CHECK(b[0] == 0); }
    if (a) UnmapViewOfFile(a);
    if (b) UnmapViewOfFile(b);
    CloseHandle(h);
}
TEST(gap1_cfm2_prot_exec_variants) {
    REQUIRE(CFM2);
    static const ULONG p[] = { PAGE_EXECUTE_READ, PAGE_EXECUTE_READWRITE, PAGE_EXECUTE_WRITECOPY };
    for (size_t i = 0; i < 3; i++) {
        SET_ERR(0);
        HANDLE h = g1_pagefile_map(p[i], 4096);
        t_note("prot 0x%lx: h=%p err=%lu", p[i], h, GetLastError());
        CHECK(h != NULL);
        if (h) CloseHandle(h);
    }
}
TEST(gap1_cfm2_prot_invalid_values) {
    REQUIRE(CFM2);
    static const ULONG p[] = { 0, PAGE_NOACCESS, PAGE_GUARD, 0x3, 0x5, 0x100 | PAGE_READWRITE, 0xFFFFFFFF, 0x1000, PAGE_NOCACHE | PAGE_READWRITE };
    for (size_t i = 0; i < sizeof p / sizeof p[0]; i++) {
        SET_ERR(0);
        HANDLE h = g1_pagefile_map(p[i], 4096);
        t_note("prot 0x%lx: h=%p err=%lu", p[i], h, GetLastError());
        if (h) CloseHandle(h);
    }
}
TEST(gap1_cfm2_prot_zero_fails) {
    REQUIRE(CFM2);
    SET_ERR(0);
    HANDLE h = g1_pagefile_map(0, 4096);
    CHECK(h == NULL);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_INVALID_PARAMETER || e == ERROR_INVALID_FUNCTION || e == ERROR_NOACCESS, "err %lu", e);
    if (h) CloseHandle(h);
}

/* ---------- desired access ---------- */
TEST(gap1_cfm2_desired_access_read_only_handle) {
    REQUIRE(CFM2);
    HANDLE h = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_READ, PAGE_READWRITE, 0, 4096, NULL, NULL, 0);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (!h) return;
    SET_ERR(0);
    void *w = MapViewOfFile(h, FILE_MAP_WRITE, 0, 0, 0);
    CHECK(w == NULL);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    if (w) UnmapViewOfFile(w);
    void *r = MapViewOfFile(h, FILE_MAP_READ, 0, 0, 0);
    CHECK(r != NULL);
    if (r) UnmapViewOfFile(r);
    CloseHandle(h);
}
TEST(gap1_cfm2_desired_access_write_only_handle) {
    REQUIRE(CFM2);
    HANDLE h = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_WRITE, PAGE_READWRITE, 0, 4096, NULL, NULL, 0);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (!h) return;
    void *w = MapViewOfFile(h, FILE_MAP_WRITE, 0, 0, 0);
    CHECK(w != NULL);
    if (w) UnmapViewOfFile(w);
    CloseHandle(h);
}
TEST(gap1_cfm2_desired_access_zero) {
    REQUIRE(CFM2);
    SET_ERR(0);
    HANDLE h = CFM2(INVALID_HANDLE_VALUE, NULL, 0, PAGE_READWRITE, 0, 4096, NULL, NULL, 0);
    t_note("desired access 0: h=%p err=%lu", h, GetLastError());
    if (h) {
        SET_ERR(0);
        void *r = MapViewOfFile(h, FILE_MAP_READ, 0, 0, 0);
        CHECK(r == NULL);
        EXPECT_ERR(ERROR_ACCESS_DENIED);
        if (r) UnmapViewOfFile(r);
        CloseHandle(h);
    }
}
TEST(gap1_cfm2_desired_access_all) {
    REQUIRE(CFM2);
    HANDLE h = CFM2(INVALID_HANDLE_VALUE, NULL, SECTION_ALL_ACCESS, PAGE_READWRITE, 0, 4096, NULL, NULL, 0);
    CHECK(h != NULL);
    if (h) {
        void *x = MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, 0);
        CHECK(x != NULL);
        if (x) UnmapViewOfFile(x);
        CloseHandle(h);
    }
}

/* ---------- file backed ---------- */
TEST(gap1_cfm2_file_readonly_section) {
    REQUIRE(CFM2);
    char data[8192]; for (int i = 0; i < 8192; i++) data[i] = (char)(i * 7);
    HANDLE f = g1_mkfile("g1_cfm2_ro.bin", data, sizeof data, GENERIC_READ);
    if (f == INVALID_HANDLE_VALUE) { t_skip("file create"); return; }
    HANDLE h = CFM2(f, NULL, FILE_MAP_READ, PAGE_READONLY, 0, 0, NULL, NULL, 0);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (h) {
        char *v = (char *)MapViewOfFile(h, FILE_MAP_READ, 0, 0, 0);
        CHECK(v != NULL);
        if (v) { CHECK(!memcmp(v, data, sizeof data)); UnmapViewOfFile(v); }
        CloseHandle(h);
    }
    CloseHandle(f);
}
TEST(gap1_cfm2_file_readwrite_writes_through) {
    REQUIRE(CFM2);
    char data[4096]; memset(data, 'a', sizeof data);
    HANDLE f = g1_mkfile("g1_cfm2_rw.bin", data, sizeof data, GENERIC_READ | GENERIC_WRITE);
    if (f == INVALID_HANDLE_VALUE) { t_skip("file create"); return; }
    HANDLE h = CFM2(f, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 0, NULL, NULL, 0);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (h) {
        char *v = (char *)MapViewOfFile(h, FILE_MAP_WRITE, 0, 0, 0);
        CHECK(v != NULL);
        if (v) { memcpy(v, "HELLO", 5); FlushViewOfFile(v, 0); UnmapViewOfFile(v); }
        CloseHandle(h);
        char back[8] = {0}; DWORD n = 0;
        SetFilePointer(f, 0, NULL, FILE_BEGIN);
        ReadFile(f, back, 5, &n, NULL);
        CHECK(n == 5 && !memcmp(back, "HELLO", 5));
    }
    CloseHandle(f);
}
TEST(gap1_cfm2_file_without_write_access_for_readwrite) {
    REQUIRE(CFM2);
    char data[4096]; memset(data, 'a', sizeof data);
    HANDLE f = g1_mkfile("g1_cfm2_nw.bin", data, sizeof data, GENERIC_READ);
    if (f == INVALID_HANDLE_VALUE) { t_skip("file create"); return; }
    SET_ERR(0);
    HANDLE h = CFM2(f, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 0, NULL, NULL, 0);
    CHECK(h == NULL);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    if (h) CloseHandle(h);
    CloseHandle(f);
}
TEST(gap1_cfm2_file_zero_length_size_zero) {
    REQUIRE(CFM2);
    HANDLE f = g1_mkfile("g1_cfm2_empty.bin", NULL, 0, GENERIC_READ | GENERIC_WRITE);
    if (f == INVALID_HANDLE_VALUE) { t_skip("file create"); return; }
    SET_ERR(0);
    HANDLE h = CFM2(f, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 0, NULL, NULL, 0);
    CHECK(h == NULL);
    EXPECT_ERR(ERROR_FILE_INVALID);
    if (h) CloseHandle(h);
    CloseHandle(f);
}
TEST(gap1_cfm2_file_grows_to_max_size) {
    REQUIRE(CFM2);
    char data[100]; memset(data, 'z', sizeof data);
    HANDLE f = g1_mkfile("g1_cfm2_grow.bin", data, sizeof data, GENERIC_READ | GENERIC_WRITE);
    if (f == INVALID_HANDLE_VALUE) { t_skip("file create"); return; }
    HANDLE h = CFM2(f, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 10000, NULL, NULL, 0);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (h) {
        LARGE_INTEGER sz; GetFileSizeEx(f, &sz);
        EXPECT_EQ(sz.QuadPart, 10000);
        CloseHandle(h);
    }
    CloseHandle(f);
}
TEST(gap1_cfm2_file_readonly_max_size_larger_than_file) {
    REQUIRE(CFM2);
    char data[100]; memset(data, 'z', sizeof data);
    HANDLE f = g1_mkfile("g1_cfm2_big.bin", data, sizeof data, GENERIC_READ);
    if (f == INVALID_HANDLE_VALUE) { t_skip("file create"); return; }
    SET_ERR(0);
    HANDLE h = CFM2(f, NULL, FILE_MAP_READ, PAGE_READONLY, 0, 100000, NULL, NULL, 0);
    CHECK(h == NULL);
    t_note("read-only section larger than file: err=%lu", GetLastError());
    if (h) CloseHandle(h);
    CloseHandle(f);
}
TEST(gap1_cfm2_file_handle_closed) {
    REQUIRE(CFM2);
    HANDLE f = g1_mkfile("g1_cfm2_closed.bin", "x", 1, GENERIC_READ | GENERIC_WRITE);
    if (f == INVALID_HANDLE_VALUE) { t_skip("file create"); return; }
    CloseHandle(f);
    SET_ERR(0);
    HANDLE h = CFM2(f, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 0, NULL, NULL, 0);
    CHECK(h == NULL);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    if (h) CloseHandle(h);
}
TEST(gap1_cfm2_file_handle_wrong_type) {
    REQUIRE(CFM2);
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    HANDLE h = CFM2(ev, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 4096, NULL, NULL, 0);
    CHECK(h == NULL);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    if (h) CloseHandle(h);
    CloseHandle(ev);
}
TEST(gap1_cfm2_file_handle_bogus_value) {
    REQUIRE(CFM2);
    SET_ERR(0);
    HANDLE h = CFM2((HANDLE)(ULONG_PTR)-2, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 4096, NULL, NULL, 0);
    CHECK(h == NULL);
    t_note("handle -2: err=%lu", GetLastError());
    if (h) CloseHandle(h);
}
TEST(gap1_cfm2_directory_handle) {
    REQUIRE(CFM2);
    HANDLE d = CreateFileA(t_tmpdir(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (d == INVALID_HANDLE_VALUE) { t_skip("dir open"); return; }
    SET_ERR(0);
    HANDLE h = CFM2(d, NULL, FILE_MAP_READ, PAGE_READONLY, 0, 0, NULL, NULL, 0);
    CHECK(h == NULL);
    t_note("directory handle: err=%lu", GetLastError());
    if (h) CloseHandle(h);
    CloseHandle(d);
}
TEST(gap1_cfm2_file_pipe_handle) {
    REQUIRE(CFM2);
    HANDLE r, w;
    if (!CreatePipe(&r, &w, NULL, 0)) { t_skip("pipe"); return; }
    SET_ERR(0);
    HANDLE h = CFM2(r, NULL, FILE_MAP_READ, PAGE_READONLY, 0, 4096, NULL, NULL, 0);
    CHECK(h == NULL);
    t_note("pipe handle: err=%lu", GetLastError());
    if (h) CloseHandle(h);
    CloseHandle(r); CloseHandle(w);
}
TEST(gap1_cfm2_file_sec_image_on_data_file) {
    REQUIRE(CFM2);
    char data[4096]; memset(data, 'q', sizeof data);
    HANDLE f = g1_mkfile("g1_cfm2_img.bin", data, sizeof data, GENERIC_READ | GENERIC_EXECUTE);
    if (f == INVALID_HANDLE_VALUE) { t_skip("file create"); return; }
    SET_ERR(0);
    HANDLE h = CFM2(f, NULL, FILE_MAP_READ | FILE_MAP_EXECUTE, PAGE_READONLY, SEC_IMAGE, 0, NULL, NULL, 0);
    CHECK(h == NULL);
    EXPECT_ERR(ERROR_BAD_EXE_FORMAT);
    if (h) CloseHandle(h);
    CloseHandle(f);
}
TEST(gap1_cfm2_file_sec_image_on_real_pe) {
    REQUIRE(CFM2);
    wchar_t p[MAX_PATH]; GetSystemDirectoryW(p, MAX_PATH); wcscat(p, L"\\kernel32.dll");
    HANDLE f = CreateFileW(p, GENERIC_READ | GENERIC_EXECUTE, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) { t_skip("open kernel32.dll"); return; }
    HANDLE h = CFM2(f, NULL, FILE_MAP_READ, PAGE_READONLY, SEC_IMAGE, 0, NULL, NULL, 0);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (h) {
        void *v = MapViewOfFile(h, FILE_MAP_READ, 0, 0, 0);
        CHECK(v != NULL);
        if (v) {
            CHECK(((unsigned char *)v)[0] == 'M' && ((unsigned char *)v)[1] == 'Z');
            MEMORY_BASIC_INFORMATION mbi; VirtualQuery(v, &mbi, sizeof mbi);
            EXPECT_EQ(mbi.Type, MEM_IMAGE);
            UnmapViewOfFile(v);
        }
        CloseHandle(h);
    }
    CloseHandle(f);
}
TEST(gap1_cfm2_file_sec_image_default_attr_is_not_image) {
    REQUIRE(CFM2);
    wchar_t p[MAX_PATH]; GetSystemDirectoryW(p, MAX_PATH); wcscat(p, L"\\kernel32.dll");
    HANDLE f = CreateFileW(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) { t_skip("open kernel32.dll"); return; }
    HANDLE h = CFM2(f, NULL, FILE_MAP_READ, PAGE_READONLY, 0, 0, NULL, NULL, 0);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (h) {
        void *v = MapViewOfFile(h, FILE_MAP_READ, 0, 0, 0);
        if (v) { MEMORY_BASIC_INFORMATION mbi; VirtualQuery(v, &mbi, sizeof mbi); EXPECT_EQ(mbi.Type, MEM_MAPPED); UnmapViewOfFile(v); }
        CloseHandle(h);
    }
    CloseHandle(f);
}

/* ---------- names ---------- */
TEST(gap1_cfm2_named_create_open_and_exists) {
    REQUIRE(CFM2);
    wchar_t nm[128]; g1_name(nm, 128, "named");
    SET_ERR(0x1234);
    HANDLE a = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 4096, nm, NULL, 0);
    CHECKF(a != NULL, "err %lu", GetLastError());
    if (!a) return;
    EXPECT_ERR(0);
    SET_ERR(0);
    HANDLE b = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 4096, nm, NULL, 0);
    CHECK(b != NULL);
    CHECK(b != a);
    EXPECT_ERR(ERROR_ALREADY_EXISTS);
    char *va = (char *)MapViewOfFile(a, FILE_MAP_WRITE, 0, 0, 0);
    char *vb = b ? (char *)MapViewOfFile(b, FILE_MAP_READ, 0, 0, 0) : NULL;
    if (va && vb) { strcpy(va, "shared"); EXPECT_STR(vb, "shared"); }
    CHECK(va && vb);
    HANDLE c = OpenFileMappingW(FILE_MAP_READ, FALSE, nm);
    CHECK(c != NULL);
    if (c) CloseHandle(c);
    if (va) UnmapViewOfFile(va);
    if (vb) UnmapViewOfFile(vb);
    if (b) CloseHandle(b);
    CloseHandle(a);
    SET_ERR(0);
    c = OpenFileMappingW(FILE_MAP_READ, FALSE, nm);
    CHECK(c == NULL);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    if (c) CloseHandle(c);
}
TEST(gap1_cfm2_named_existing_ignores_new_size) {
    REQUIRE(CFM2);
    wchar_t nm[128]; g1_name(nm, 128, "size");
    HANDLE a = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 4096, nm, NULL, 0);
    HANDLE b = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 65536, nm, NULL, 0);
    CHECK(a && b);
    if (b) {
        void *v = MapViewOfFile(b, FILE_MAP_READ, 0, 0, 0);
        if (v) { MEMORY_BASIC_INFORMATION mbi; VirtualQuery(v, &mbi, sizeof mbi); EXPECT_EQ(mbi.RegionSize, 4096); UnmapViewOfFile(v); }
        CloseHandle(b);
    }
    if (a) CloseHandle(a);
}
TEST(gap1_cfm2_named_type_collision_with_event) {
    REQUIRE(CFM2);
    wchar_t nm[128]; g1_name(nm, 128, "evt");
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, nm);
    if (!ev) { t_skip("event"); return; }
    SET_ERR(0);
    HANDLE h = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 4096, nm, NULL, 0);
    CHECK(h == NULL);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    if (h) CloseHandle(h);
    CloseHandle(ev);
}
TEST(gap1_cfm2_named_visible_to_classic_open_in_both_directions) {
    REQUIRE(CFM2);
    wchar_t nm[128]; g1_name(nm, 128, "xdir");
    HANDLE a = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, nm);
    CHECK(a != NULL);
    SET_ERR(0);
    HANDLE b = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 4096, nm, NULL, 0);
    CHECK(b != NULL);
    EXPECT_ERR(ERROR_ALREADY_EXISTS);
    if (b) CloseHandle(b);
    if (a) CloseHandle(a);
}
TEST(gap1_cfm2_name_with_backslash) {
    REQUIRE(CFM2);
    wchar_t nm[160]; _snwprintf(nm, 160, L"Local\\k32t_gap1_%lu_no\\such\\dir", GetCurrentProcessId());
    SET_ERR(0);
    HANDLE h = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 4096, nm, NULL, 0);
    CHECK(h == NULL);
    t_note("name with extra backslash: err=%lu", GetLastError());
    if (h) CloseHandle(h);
}
TEST(gap1_cfm2_name_empty_string) {
    REQUIRE(CFM2);
    SET_ERR(0);
    HANDLE h = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 4096, L"", NULL, 0);
    t_note("empty name: h=%p err=%lu", h, GetLastError());
    if (h) CloseHandle(h);
}
TEST(gap1_cfm2_name_case_insensitive) {
    REQUIRE(CFM2);
    wchar_t a[128], b[128]; g1_name(a, 128, "CaSe"); g1_name(b, 128, "cAsE");
    HANDLE h1 = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 4096, a, NULL, 0);
    SET_ERR(0);
    HANDLE h2 = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 4096, b, NULL, 0);
    /* P1: kernel object names are case-sensitive (BaseFormatObjectAttributes sets OBJ_OPENIF only, no
     * OBJ_CASE_INSENSITIVE): differing case yields two distinct sections, no ERROR_ALREADY_EXISTS. UNVERIFIED-until-rerun. */
    CHECK(h1 && h2);
    EXPECT_ERR(0);
    CHECK(h1 != h2);
    if (h1) CloseHandle(h1);
    if (h2) CloseHandle(h2);
}
CHILD(gap1_cfm2_reader) {
    wchar_t nm[128]; _snwprintf(nm, 128, L"Local\\k32t_gap1_%s_xproc", argc > 1 ? L"" : L"");
    if (argc < 2) return 90;
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, nm, 128);
    HANDLE h = OpenFileMappingW(FILE_MAP_READ, FALSE, nm);
    if (!h) return 91;
    char *v = (char *)MapViewOfFile(h, FILE_MAP_READ, 0, 0, 0);
    if (!v) return 92;
    int r = strcmp(v, "from-parent") ? 93 : 0;
    UnmapViewOfFile(v); CloseHandle(h);
    return r;
}
TEST(gap1_cfm2_cross_process_open) {
    REQUIRE(CFM2);
    wchar_t nm[128]; g1_name(nm, 128, "xproc");
    HANDLE a = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 4096, nm, NULL, 0);
    CHECK(a != NULL); if (!a) return;
    char *v = (char *)MapViewOfFile(a, FILE_MAP_WRITE, 0, 0, 0);
    if (v) strcpy(v, "from-parent");
    char nma[256]; WideCharToMultiByte(CP_ACP, 0, nm, -1, nma, sizeof nma, NULL, NULL);
    EXPECT_EQ(t_run_child("gap1_cfm2_reader", nma, 20000), 0);
    if (v) UnmapViewOfFile(v);
    CloseHandle(a);
}

/* ---------- security attributes ---------- */
TEST(gap1_cfm2_inheritable_handle) {
    REQUIRE(CFM2);
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE h = CFM2(INVALID_HANDLE_VALUE, &sa, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 4096, NULL, NULL, 0);
    CHECK(h != NULL); if (!h) return;
    DWORD fl = 0; CHECK(GetHandleInformation(h, &fl));
    CHECK(fl & HANDLE_FLAG_INHERIT);
    CloseHandle(h);
}
TEST(gap1_cfm2_non_inheritable_by_default) {
    REQUIRE(CFM2);
    HANDLE h = CFM2(INVALID_HANDLE_VALUE, NULL, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 4096, NULL, NULL, 0);
    CHECK(h != NULL); if (!h) return;
    DWORD fl = 0xFF; CHECK(GetHandleInformation(h, &fl));
    CHECK(!(fl & HANDLE_FLAG_INHERIT));
    CloseHandle(h);
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, FALSE };
    h = CFM2(INVALID_HANDLE_VALUE, &sa, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 4096, NULL, NULL, 0);
    CHECK(h != NULL); if (!h) return;
    fl = 0xFF; GetHandleInformation(h, &fl);
    CHECK(!(fl & HANDLE_FLAG_INHERIT));
    CloseHandle(h);
}
TEST(gap1_cfm2_security_descriptor_denies_dacl_empty) {
    REQUIRE(CFM2);
    /* a NULL DACL grants everybody; an empty DACL grants nobody: opening by name afterwards must fail with ACCESS_DENIED */
    SECURITY_DESCRIPTOR sd; BYTE acl[64];
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    InitializeAcl((PACL)acl, sizeof acl, ACL_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, (PACL)acl, FALSE);
    SECURITY_ATTRIBUTES sa = { sizeof sa, &sd, FALSE };
    wchar_t nm[128]; g1_name(nm, 128, "emptydacl");
    HANDLE a = CFM2(INVALID_HANDLE_VALUE, &sa, FILE_MAP_ALL_ACCESS, PAGE_READWRITE, 0, 4096, nm, NULL, 0);
    CHECKF(a != NULL, "err %lu", GetLastError());
    if (!a) return;
    SET_ERR(0);
    HANDLE b = OpenFileMappingW(FILE_MAP_READ, FALSE, nm);
    CHECK(b == NULL);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    if (b) CloseHandle(b);
    CloseHandle(a);
}

/* ---------- parity with CreateFileMappingW ---------- */
TEST(gap1_cfm2_matches_classic_for_pagefile) {
    REQUIRE(CFM2); REQUIRE(U(CreateFileMappingW));
    HANDLE a = CFM2(INVALID_HANDLE_VALUE, NULL, SECTION_ALL_ACCESS, PAGE_READWRITE, SEC_COMMIT, 12345, NULL, NULL, 0);
    HANDLE b = U(CreateFileMappingW)(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 12345, NULL);
    CHECK(a && b);
    if (a && b) {
        void *va = MapViewOfFile(a, FILE_MAP_WRITE, 0, 0, 0), *vb = MapViewOfFile(b, FILE_MAP_WRITE, 0, 0, 0);
        MEMORY_BASIC_INFORMATION ma, mb;
        if (va && vb) { VirtualQuery(va, &ma, sizeof ma); VirtualQuery(vb, &mb, sizeof mb); EXPECT_EQ(ma.RegionSize, mb.RegionSize); EXPECT_EQ(ma.State, mb.State); EXPECT_EQ(ma.Protect, mb.Protect); }
        if (va) UnmapViewOfFile(va);
        if (vb) UnmapViewOfFile(vb);
    }
    if (a) CloseHandle(a);
    if (b) CloseHandle(b);
}
TEST(gap1_cfm2_handle_duplicate_and_close_order) {
    REQUIRE(CFM2);
    HANDLE a = g1_pagefile_map(PAGE_READWRITE, 4096), d = NULL;
    CHECK(a != NULL); if (!a) return;
    CHECK(DuplicateHandle(GetCurrentProcess(), a, GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS));
    CloseHandle(a);
    void *v = d ? MapViewOfFile(d, FILE_MAP_WRITE, 0, 0, 0) : NULL;
    CHECK(v != NULL);
    if (v) UnmapViewOfFile(v);
    if (d) CloseHandle(d);
}
TEST(gap1_cfm2_many_sections) {
    REQUIRE(CFM2);
    HANDLE h[200]; int n = 0;
    for (; n < 200; n++) { h[n] = g1_pagefile_map(PAGE_READWRITE, 4096); if (!h[n]) break; }
    CHECK(n == 200);
    for (int i = 0; i < n; i++) CloseHandle(h[i]);
}
TEST(gap1_cfm2_map_offset_alignment_on_section) {
    REQUIRE(CFM2);
    HANDLE h = g1_pagefile_map(PAGE_READWRITE, 3 * 65536);
    CHECK(h != NULL); if (!h) return;
    SET_ERR(0);
    void *v = MapViewOfFile(h, FILE_MAP_WRITE, 0, 4096, 4096);   /* offset not a multiple of allocation granularity */
    CHECK(v == NULL);
    EXPECT_ERR(ERROR_MAPPED_ALIGNMENT);
    if (v) UnmapViewOfFile(v);
    v = MapViewOfFile(h, FILE_MAP_WRITE, 0, 65536, 4096);
    CHECK(v != NULL);
    if (v) UnmapViewOfFile(v);
    SET_ERR(0);
    v = MapViewOfFile(h, FILE_MAP_WRITE, 0, 4 * 65536, 4096);   /* beyond section */
    CHECK(v == NULL);
    if (v) UnmapViewOfFile(v);
    CloseHandle(h);
}
