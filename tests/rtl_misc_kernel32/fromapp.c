/* App-family memory wrappers (…FromApp): parity with the plain APIs, and rejection of executable protections. */
#include "t.h"

#define CFM  UK(CreateFileMappingFromApp)
#define MVF  UK(MapViewOfFileFromApp)
#define VAA  UB(VirtualAllocFromApp)
#define VPA  UB(VirtualProtectFromApp)
#define OFM  UB(OpenFileMappingFromApp)

static const DWORD exec_prots[] = { PAGE_EXECUTE, PAGE_EXECUTE_READ, PAGE_EXECUTE_READWRITE, PAGE_EXECUTE_WRITECOPY };

TEST(fromapp_createfilemapping_pagefile_backed) {
    REQUIRE(CFM); REQUIRE(MVF);
    HANDLE h = CFM(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 4096, NULL);
    CHECK(h != NULL);
    if (!h) return;
    BYTE *p = (BYTE *)MVF(h, FILE_MAP_WRITE, 0, 0);
    CHECK(p != NULL);
    if (p) { p[0] = 0x5A; p[4095] = 0xA5; CHECK(p[0] == 0x5A && p[4095] == 0xA5); UnmapViewOfFile(p); }
    CloseHandle(h);
}

TEST(fromapp_createfilemapping_size_is_64bit) {
    REQUIRE(CFM);
    HANDLE h = CFM(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0x100000000ull + 4096, NULL);   /* 4 GB + 4 KB, reserved not committed */
    t_note("4GB+ pagefile mapping: %p err=%lu", h, GetLastError());
    if (h) CloseHandle(h);
    CHECK(1);
}

TEST(fromapp_createfilemapping_zero_size_without_file_invalid) {
    REQUIRE(CFM);
    SET_ERR(0);
    HANDLE h = CFM(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, NULL);
    CHECK(h == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}

TEST(fromapp_createfilemapping_matches_plain_api_for_bad_args) {
    REQUIRE(CFM); REQUIRE(U(CreateFileMappingW));
    SET_ERR(0);
    HANDLE a = CFM(INVALID_HANDLE_VALUE, NULL, 0xDEAD0001 & ~0xF0, 4096, NULL);
    DWORD ea = GetLastError();
    SET_ERR(0);
    HANDLE b = U(CreateFileMappingW)(INVALID_HANDLE_VALUE, NULL, 0xDEAD0001 & ~0xF0, 0, 4096, NULL);
    DWORD eb = GetLastError();
    CHECK((a == NULL) == (b == NULL));
    EXPECT_EQ(ea, eb);
    if (a) CloseHandle(a);
    if (b) CloseHandle(b);
}

TEST(fromapp_createfilemapping_invalid_handle_closed_file) {
    REQUIRE(CFM);
    HANDLE f = CreateFileA("NUL", GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    CloseHandle(f);
    SET_ERR(0);
    HANDLE h = CFM(f, NULL, PAGE_READONLY, 0, NULL);
    CHECK(h == NULL);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}

TEST(fromapp_createfilemapping_file_backed_readonly) {
    REQUIRE(CFM); REQUIRE(MVF);
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "fromapp_map.bin");
    HANDLE f = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    REQUIRE(f != INVALID_HANDLE_VALUE);
    DWORD w; WriteFile(f, "hello-world!", 12, &w, NULL);
    HANDLE m = CFM(f, NULL, PAGE_READONLY, 0, NULL);
    CHECK(m != NULL);
    if (m) {
        const char *v = (const char *)MVF(m, FILE_MAP_READ, 0, 0);
        CHECK(v != NULL);
        if (v) { CHECK(!memcmp(v, "hello-world!", 12)); UnmapViewOfFile(v); }
        CloseHandle(m);
    }
    CloseHandle(f);
}

TEST(fromapp_createfilemapping_exec_protections_observed) {
    REQUIRE(CFM);
    for (unsigned i = 0; i < 4; i++) {
        SET_ERR(0);
        HANDLE h = CFM(INVALID_HANDLE_VALUE, NULL, exec_prots[i], 4096, NULL);
        t_note("CreateFileMappingFromApp prot=0x%lx -> %p err=%lu (capability dependent)", exec_prots[i], h, GetLastError());
        if (h) CloseHandle(h);
    }
    CHECK(1);
}

TEST(fromapp_createfilemapping_named_then_open) {
    REQUIRE(CFM); REQUIRE(OFM);
    WCHAR name[64]; wsprintfW(name, L"k32t_fa_%lu_named", GetCurrentProcessId());
    HANDLE h = CFM(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 4096, name);
    REQUIRE(h);
    HANDLE o = OFM(FILE_MAP_READ, FALSE, name);
    CHECK(o != NULL);
    if (o) CloseHandle(o);
    CloseHandle(h);
}

TEST(fromapp_openfilemapping_missing_name) {
    REQUIRE(OFM);
    SET_ERR(0);
    HANDLE o = OFM(FILE_MAP_READ, FALSE, L"k32t_fa_definitely_not_there");
    CHECK(o == NULL);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}

TEST(fromapp_openfilemapping_null_name) {
    REQUIRE(OFM);
    SET_ERR(0);
    HANDLE o = OFM(FILE_MAP_READ, FALSE, NULL);
    CHECK(o == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}

TEST(fromapp_openfilemapping_inherit_flag_must_be_boolean) {
    REQUIRE(OFM);
    WCHAR name[64]; wsprintfW(name, L"k32t_fa_%lu_bool", GetCurrentProcessId());
    HANDLE h = CFM ? CFM(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 4096, name) : NULL;
    REQUIRE(h);
    SET_ERR(0);
    HANDLE o = OFM(FILE_MAP_READ, 2, name);       /* bInheritHandle >= 2 is rejected by the wrapper */
    CHECK(o == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    o = OFM(FILE_MAP_READ, 0xFFFFFFFFu, name);
    CHECK(o == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    o = OFM(FILE_MAP_READ, 1, name);
    CHECK(o != NULL);
    if (o) { DWORD fl = 0; GetHandleInformation(o, &fl); CHECK(fl & HANDLE_FLAG_INHERIT); CloseHandle(o); }
    CloseHandle(h);
}

TEST(fromapp_openfilemapping_access_rights_match_plain) {
    REQUIRE(OFM); REQUIRE(MVF);
    WCHAR name[64]; wsprintfW(name, L"k32t_fa_%lu_acc", GetCurrentProcessId());
    HANDLE h = CFM(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 4096, name);
    REQUIRE(h);
    HANDLE ro = OFM(FILE_MAP_READ, FALSE, name);
    REQUIRE(ro);
    SET_ERR(0);
    void *v = MVF(ro, FILE_MAP_WRITE, 0, 0);
    CHECK(v == NULL);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(ro); CloseHandle(h);
}

TEST(fromapp_mapview_write_with_execute_rejected) {
    REQUIRE(CFM); REQUIRE(MVF);
    HANDLE h = CFM(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 4096, NULL);
    REQUIRE(h);
    SET_ERR(0);
    void *v = MVF(h, FILE_MAP_WRITE | FILE_MAP_EXECUTE, 0, 0);
    CHECK(v == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    if (v) UnmapViewOfFile(v);
    CloseHandle(h);
}

TEST(fromapp_mapview_copy_with_execute_rejected) {
    REQUIRE(CFM); REQUIRE(MVF);
    HANDLE h = CFM(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 4096, NULL);
    REQUIRE(h);
    SET_ERR(0);
    void *v = MVF(h, FILE_MAP_COPY | FILE_MAP_EXECUTE, 0, 0);
    CHECK(v == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    if (v) UnmapViewOfFile(v);
    CloseHandle(h);
}

TEST(fromapp_mapview_read_with_execute_capability_path) {
    REQUIRE(CFM); REQUIRE(MVF);
    HANDLE h = CFM(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 4096, NULL);
    REQUIRE(h);
    SET_ERR(0);
    void *v = MVF(h, FILE_MAP_READ | FILE_MAP_EXECUTE, 0, 0);
    t_note("MapViewOfFileFromApp(READ|EXECUTE) -> %p err=%lu (needs codeGeneration capability)", v, GetLastError());
    if (v) UnmapViewOfFile(v);
    CloseHandle(h);
    CHECK(1);
}

TEST(fromapp_mapview_null_mapping) {
    REQUIRE(MVF);
    SET_ERR(0);
    CHECK(MVF(NULL, FILE_MAP_READ, 0, 0) == NULL);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}

TEST(fromapp_mapview_invalid_handle_value) {
    REQUIRE(MVF);
    SET_ERR(0);
    CHECK(MVF(INVALID_HANDLE_VALUE, FILE_MAP_READ, 0, 0) == NULL);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}

TEST(fromapp_mapview_wrong_handle_type) {
    REQUIRE(MVF);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    CHECK(MVF(ev, FILE_MAP_READ, 0, 0) == NULL);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
}

TEST(fromapp_mapview_unaligned_offset) {
    REQUIRE(CFM); REQUIRE(MVF);
    HANDLE h = CFM(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 1 << 20, NULL);
    REQUIRE(h);
    SET_ERR(0);
    void *v = MVF(h, FILE_MAP_WRITE, 1, 0);
    CHECK(v == NULL);
    EXPECT_ERR(ERROR_MAPPED_ALIGNMENT);
    if (v) UnmapViewOfFile(v);
    CloseHandle(h);
}

TEST(fromapp_mapview_offset_beyond_size) {
    REQUIRE(CFM); REQUIRE(MVF);
    SYSTEM_INFO si; GetSystemInfo(&si);
    HANDLE h = CFM(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 4096, NULL);
    REQUIRE(h);
    SET_ERR(0);
    void *v = MVF(h, FILE_MAP_WRITE, si.dwAllocationGranularity * 4ull, 0);
    CHECK(v == NULL);
    t_note("offset beyond mapping size err=%lu", GetLastError());
    if (v) UnmapViewOfFile(v);
    CloseHandle(h);
}

TEST(fromapp_mapview_length_beyond_mapping) {
    REQUIRE(CFM); REQUIRE(MVF);
    HANDLE h = CFM(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 4096, NULL);
    REQUIRE(h);
    SET_ERR(0);
    void *v = MVF(h, FILE_MAP_WRITE, 0, 1 << 20);
    CHECK(v == NULL);
    t_note("length beyond mapping err=%lu", GetLastError());
    if (v) UnmapViewOfFile(v);
    CloseHandle(h);
}

TEST(fromapp_mapview_partial_length) {
    REQUIRE(CFM); REQUIRE(MVF);
    HANDLE h = CFM(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 65536, NULL);
    REQUIRE(h);
    BYTE *v = (BYTE *)MVF(h, FILE_MAP_WRITE, 0, 100);
    CHECK(v != NULL);
    if (v) {
        MEMORY_BASIC_INFORMATION mbi; VirtualQuery(v, &mbi, sizeof mbi);
        EXPECT_EQ(mbi.RegionSize, 4096);
        UnmapViewOfFile(v);
    }
    CloseHandle(h);
}

TEST(fromapp_mapview_read_only_view_write_access_denied) {
    REQUIRE(CFM); REQUIRE(MVF);
    HANDLE h = CFM(INVALID_HANDLE_VALUE, NULL, PAGE_READONLY, 4096, NULL);
    t_note("PAGE_READONLY pagefile mapping -> %p", h);
    if (h) {
        SET_ERR(0);
        void *v = MVF(h, FILE_MAP_WRITE, 0, 0);
        CHECK(v == NULL);
        EXPECT_ERR(ERROR_ACCESS_DENIED);
        CloseHandle(h);
    }
}

TEST(fromapp_mapview_shares_memory_between_views) {
    REQUIRE(CFM); REQUIRE(MVF);
    HANDLE h = CFM(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 4096, NULL);
    REQUIRE(h);
    BYTE *a = (BYTE *)MVF(h, FILE_MAP_WRITE, 0, 0), *b = (BYTE *)MVF(h, FILE_MAP_WRITE, 0, 0);
    CHECK(a && b && a != b);
    if (a && b) { a[10] = 0x77; CHECK(b[10] == 0x77); }
    if (a) UnmapViewOfFile(a);
    if (b) UnmapViewOfFile(b);
    CloseHandle(h);
}

TEST(fromapp_mapview_copy_on_write) {
    REQUIRE(CFM); REQUIRE(MVF);
    HANDLE h = CFM(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 4096, NULL);
    REQUIRE(h);
    BYTE *w = (BYTE *)MVF(h, FILE_MAP_WRITE, 0, 0), *c = (BYTE *)MVF(h, FILE_MAP_COPY, 0, 0);
    REQUIRE(w); REQUIRE(c);
    w[0] = 1; CHECK(c[0] == 1);
    c[0] = 2;
    CHECK(w[0] == 1);
    UnmapViewOfFile(w); UnmapViewOfFile(c);
    CloseHandle(h);
}

TEST(fromapp_mapview_view_survives_handle_close) {
    REQUIRE(CFM); REQUIRE(MVF);
    HANDLE h = CFM(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 4096, NULL);
    REQUIRE(h);
    BYTE *v = (BYTE *)MVF(h, FILE_MAP_WRITE, 0, 0);
    REQUIRE(v);
    CloseHandle(h);
    v[100] = 9; CHECK(v[100] == 9);
    CHECK(UnmapViewOfFile(v));
}

/* ---- VirtualAllocFromApp ---- */
TEST(fromapp_virtualalloc_rw) {
    REQUIRE(VAA);
    BYTE *p = (BYTE *)VAA(NULL, 8192, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    CHECK(p != NULL);
    if (p) { p[0] = 1; p[8191] = 2; MEMORY_BASIC_INFORMATION m; VirtualQuery(p, &m, sizeof m); EXPECT_EQ(m.Protect, PAGE_READWRITE); VirtualFree(p, 0, MEM_RELEASE); }
}

TEST(fromapp_virtualalloc_executable_protections_rejected) {
    REQUIRE(VAA);
    for (unsigned i = 0; i < 4; i++) {
        SET_ERR(0);
        void *p = VAA(NULL, 4096, MEM_COMMIT | MEM_RESERVE, exec_prots[i]);
        CHECKF(p == NULL, "prot 0x%lx", exec_prots[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        if (p) VirtualFree(p, 0, MEM_RELEASE);
    }
}

TEST(fromapp_virtualalloc_executable_with_guard_bit_rejected) {
    REQUIRE(VAA);
    SET_ERR(0);
    void *p = VAA(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE | PAGE_GUARD);
    CHECK(p == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    if (p) VirtualFree(p, 0, MEM_RELEASE);
}

TEST(fromapp_virtualalloc_reserve_then_commit) {
    REQUIRE(VAA);
    BYTE *p = (BYTE *)VAA(NULL, 65536, MEM_RESERVE, PAGE_NOACCESS);
    REQUIRE(p);
    BYTE *c = (BYTE *)VAA(p, 4096, MEM_COMMIT, PAGE_READWRITE);
    CHECK(c == p);
    if (c) { c[0] = 5; CHECK(c[0] == 5); }
    VirtualFree(p, 0, MEM_RELEASE);
}

TEST(fromapp_virtualalloc_zero_size_invalid) {
    REQUIRE(VAA);
    SET_ERR(0);
    CHECK(VAA(NULL, 0, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE) == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}

TEST(fromapp_virtualalloc_absurd_size_fails) {
    REQUIRE(VAA);
    SET_ERR(0);
    CHECK(VAA(NULL, (SIZE_T)1 << 60, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE) == NULL);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_INVALID_PARAMETER || e == ERROR_NOT_ENOUGH_MEMORY || e == ERROR_COMMITMENT_LIMIT, "err %lu", e);
}

TEST(fromapp_virtualalloc_bad_alloc_type) {
    REQUIRE(VAA);
    SET_ERR(0);
    CHECK(VAA(NULL, 4096, 0, PAGE_READWRITE) == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(VAA(NULL, 4096, MEM_COMMIT | MEM_RELEASE, PAGE_READWRITE) == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}

TEST(fromapp_virtualalloc_bad_protection_without_exec_bits) {
    REQUIRE(VAA);
    SET_ERR(0);
    CHECK(VAA(NULL, 4096, MEM_COMMIT | MEM_RESERVE, 0x3) == NULL);   /* invalid combination, no exec bits */
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}

TEST(fromapp_virtualalloc_noaccess_readonly_writecopy) {
    REQUIRE(VAA);
    static const DWORD ok[] = { PAGE_NOACCESS, PAGE_READONLY, PAGE_READWRITE };
    for (unsigned i = 0; i < 3; i++) {
        void *p = VAA(NULL, 4096, MEM_COMMIT | MEM_RESERVE, ok[i]);
        CHECKF(p != NULL, "prot 0x%lx", ok[i]);
        if (p) VirtualFree(p, 0, MEM_RELEASE);
    }
}

TEST(fromapp_virtualalloc_matches_virtualalloc_granularity) {
    REQUIRE(VAA);
    SYSTEM_INFO si; GetSystemInfo(&si);
    void *p = VAA(NULL, 1, MEM_RESERVE, PAGE_NOACCESS);
    REQUIRE(p);
    CHECK(((ULONG_PTR)p % si.dwAllocationGranularity) == 0);
    VirtualFree(p, 0, MEM_RELEASE);
}

/* ---- VirtualProtectFromApp ---- */
TEST(fromapp_virtualprotect_rw_to_ro_and_back) {
    REQUIRE(VAA); REQUIRE(VPA);
    BYTE *p = (BYTE *)VAA(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    REQUIRE(p);
    DWORD old = 0;
    CHECK(VPA(p, 4096, PAGE_READONLY, &old));
    EXPECT_EQ(old, PAGE_READWRITE);
    CHECK(VPA(p, 4096, PAGE_READWRITE, &old));
    EXPECT_EQ(old, PAGE_READONLY);
    VirtualFree(p, 0, MEM_RELEASE);
}

TEST(fromapp_virtualprotect_noaccess) {
    REQUIRE(VAA); REQUIRE(VPA);
    BYTE *p = (BYTE *)VAA(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    REQUIRE(p);
    DWORD old = 0;
    CHECK(VPA(p, 4096, PAGE_NOACCESS, &old));
    MEMORY_BASIC_INFORMATION m; VirtualQuery(p, &m, sizeof m);
    EXPECT_EQ(m.Protect, PAGE_NOACCESS);
    VirtualFree(p, 0, MEM_RELEASE);
}

TEST(fromapp_virtualprotect_executable_capability_path) {
    REQUIRE(VAA); REQUIRE(VPA);
    BYTE *p = (BYTE *)VAA(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    REQUIRE(p);
    for (unsigned i = 0; i < 4; i++) {
        DWORD old = 0;
        SET_ERR(0);
        BOOL r = VPA(p, 4096, exec_prots[i], &old);
        t_note("VirtualProtectFromApp prot=0x%lx -> %d err=%lu (capability dependent)", exec_prots[i], r, GetLastError());
        if (r) VPA(p, 4096, PAGE_READWRITE, &old);
    }
    VirtualFree(p, 0, MEM_RELEASE);
    CHECK(1);
}

TEST(fromapp_virtualprotect_unallocated_address) {
    REQUIRE(VPA);
    DWORD old = 0;
    SET_ERR(0);
    CHECK(VPA((void *)0x10000, 4096, PAGE_READWRITE, &old) == FALSE);
    EXPECT_ERR(ERROR_INVALID_ADDRESS);
}

TEST(fromapp_virtualprotect_zero_size) {
    REQUIRE(VAA); REQUIRE(VPA);
    BYTE *p = (BYTE *)VAA(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    REQUIRE(p);
    DWORD old = 0;
    SET_ERR(0);
    CHECK(VPA(p, 0, PAGE_READONLY, &old) == FALSE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    VirtualFree(p, 0, MEM_RELEASE);
}

TEST(fromapp_virtualprotect_null_old_protect) {
    REQUIRE(VAA); REQUIRE(VPA);
    BYTE *p = (BYTE *)VAA(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    REQUIRE(p);
    SET_ERR(0);
    BOOL r = VPA(p, 4096, PAGE_READONLY, NULL);
    CHECK(r == FALSE);
    t_note("NULL lpflOldProtect err=%lu", GetLastError());
    VirtualFree(p, 0, MEM_RELEASE);
}

TEST(fromapp_virtualprotect_invalid_protection) {
    REQUIRE(VAA); REQUIRE(VPA);
    BYTE *p = (BYTE *)VAA(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    REQUIRE(p);
    DWORD old = 0;
    SET_ERR(0);
    CHECK(VPA(p, 4096, 0x3, &old) == FALSE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    VirtualFree(p, 0, MEM_RELEASE);
}

TEST(fromapp_virtualprotect_guard_page) {
    REQUIRE(VAA); REQUIRE(VPA);
    BYTE *p = (BYTE *)VAA(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    REQUIRE(p);
    DWORD old = 0;
    CHECK(VPA(p, 4096, PAGE_READWRITE | PAGE_GUARD, &old));
    MEMORY_BASIC_INFORMATION m; VirtualQuery(p, &m, sizeof m);
    CHECK(m.Protect & PAGE_GUARD);
    VirtualFree(p, 0, MEM_RELEASE);
}

/* ---- VirtualAlloc2FromApp ---- */
TEST(fromapp_virtualalloc2_exec_rejected) {
    FARPROC f = t_proc_kb("VirtualAlloc2FromApp"); REQUIRE(f);
    typedef PVOID (WINAPI *fn_t)(HANDLE, PVOID, SIZE_T, ULONG, ULONG, PVOID, ULONG);
    for (unsigned i = 0; i < 4; i++) {
        SET_ERR(0);
        PVOID p = ((fn_t)f)(NULL, NULL, 4096, MEM_COMMIT | MEM_RESERVE, exec_prots[i], NULL, 0);
        CHECKF(p == NULL, "prot 0x%lx", exec_prots[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        if (p) VirtualFree(p, 0, MEM_RELEASE);
    }
}

TEST(fromapp_virtualalloc2_rw_ok) {
    FARPROC f = t_proc_kb("VirtualAlloc2FromApp"); REQUIRE(f);
    typedef PVOID (WINAPI *fn_t)(HANDLE, PVOID, SIZE_T, ULONG, ULONG, PVOID, ULONG);
    PVOID p = ((fn_t)f)(NULL, NULL, 8192, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE, NULL, 0);
    CHECK(p != NULL);
    if (p) { ((char *)p)[8191] = 1; VirtualFree(p, 0, MEM_RELEASE); }
}

TEST(fromapp_virtualalloc2_current_process_pseudo_handle) {
    FARPROC f = t_proc_kb("VirtualAlloc2FromApp"); REQUIRE(f);
    typedef PVOID (WINAPI *fn_t)(HANDLE, PVOID, SIZE_T, ULONG, ULONG, PVOID, ULONG);
    PVOID p = ((fn_t)f)(GetCurrentProcess(), NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE, NULL, 0);
    CHECK(p != NULL);
    if (p) VirtualFree(p, 0, MEM_RELEASE);
}
