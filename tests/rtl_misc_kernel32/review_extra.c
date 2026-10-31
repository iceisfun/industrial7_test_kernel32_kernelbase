/* Additional verified edge cases: app-family flag filters and trivial exports. */
#include "t.h"

#define XVAA UB(VirtualAllocFromApp)
#define XOFM UB(OpenFileMappingFromApp)
#define XMVF UK(MapViewOfFileFromApp)
#define XCFM UK(CreateFileMappingFromApp)

/* any bit of the 0xF0 protection nibble (executable family) -> 87 */
TEST(rmx_vaa_every_exec_bit_rejected) {
    REQUIRE(XVAA);
    static const DWORD bits[] = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0xF0 };
    for (int i = 0; i < 9; i++) {
        SET_ERR(0);
        void *p = XVAA(NULL, 4096, MEM_COMMIT | MEM_RESERVE, bits[i]);
        CHECKF(p == NULL, "prot %lx", bits[i]);
        if (p) VirtualFree(p, 0, MEM_RELEASE);
        else EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}

TEST(rmx_vaa_exec_rejected_for_reserve_only_request) {
    REQUIRE(XVAA);
    SET_ERR(0);
    void *p = XVAA(NULL, 65536, MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    CHECK(p == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    if (p) VirtualFree(p, 0, MEM_RELEASE);
}

TEST(rmx_ofm_inherit_values_above_one_rejected) {
    REQUIRE(XOFM);
    static const UINT v[] = { 2, 3, 0x100, 0x80000000u, 0xFFFFFFFFu };
    for (int i = 0; i < 5; i++) {
        SET_ERR(0);
        HANDLE h = XOFM(FILE_MAP_READ, v[i], L"k32t_rmx_nonexistent_mapping");
        CHECKF(h == NULL, "inherit %x", v[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        if (h) CloseHandle(h);
    }
}

TEST(rmx_ofm_valid_inherit_missing_name_gives_file_not_found) {
    REQUIRE(XOFM);
    for (UINT i = 0; i < 2; i++) {
        SET_ERR(0);
        HANDLE h = XOFM(FILE_MAP_READ, i, L"k32t_rmx_nonexistent_mapping2");
        CHECK(h == NULL);
        EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    }
}

TEST(rmx_mapview_execute_with_write_or_copy_always_87) {
    REQUIRE(XMVF); REQUIRE(XCFM);
    HANDLE h = XCFM(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 4096, NULL);
    REQUIRE(h);
    static const DWORD acc[] = { FILE_MAP_WRITE | FILE_MAP_EXECUTE, FILE_MAP_COPY | FILE_MAP_EXECUTE,
                                 FILE_MAP_ALL_ACCESS | FILE_MAP_EXECUTE };
    for (int i = 0; i < 3; i++) {
        SET_ERR(0);
        void *v = XMVF(h, acc[i], 0, 0);
        CHECKF(v == NULL, "access %lx", acc[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        if (v) UnmapViewOfFile(v);
    }
    CloseHandle(h);
}

TEST(rmx_mapview_plain_read_ok_without_execute) {
    REQUIRE(XMVF); REQUIRE(XCFM);
    HANDLE h = XCFM(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 4096, NULL);
    REQUIRE(h);
    BYTE *v = XMVF(h, FILE_MAP_READ, 0, 0);
    CHECK(v != NULL);
    if (v) { CHECK(v[0] == 0 && v[4095] == 0); UnmapViewOfFile(v); }
    CloseHandle(h);
}

TEST(rmx_sethandlecount_identity_matrix) {
    FARPROC p = t_proc_k32("SetHandleCount");
    REQUIRE(p);
    UINT (WINAPI *f)(UINT) = (void *)p;   /* body is: mov eax, ecx; ret */
    static const UINT v[] = { 0, 1, 2, 63, 64, 1000, 0x7FFFFFFF, 0x80000000u, 0xFFFFFFFEu, 0xFFFFFFFFu };
    for (int i = 0; i < 10; i++) EXPECT_EQ(f(v[i]), v[i]);
}

TEST(rmx_getconsoleinputwaithandle_is_stdin_handle_repeatable) {
    FARPROC p = t_proc_k32("GetConsoleInputWaitHandle");
    REQUIRE(p);
    HANDLE (WINAPI *f)(void) = (void *)p;   /* forwards to GetStdHandle(STD_INPUT_HANDLE) */
    HANDLE a = f(), b = f();
    EXPECT_EQ(a, b);
    EXPECT_EQ(a, GetStdHandle(STD_INPUT_HANDLE));
}

TEST(rmx_openconsolew_missing_file_error_matches_createfile) {
    FARPROC p = t_proc_k32("OpenConsoleW");
    REQUIRE(p);
    HANDLE (WINAPI *f)(LPCWSTR, DWORD, BOOL, DWORD) = (void *)p;
    char a[MAX_PATH]; WCHAR w[MAX_PATH];
    t_tmpfile(a, sizeof a, "rmx_openconsole_missing.bin");
    MultiByteToWideChar(CP_ACP, 0, a, -1, w, MAX_PATH);
    SET_ERR(0);
    HANDLE h1 = f(w, GENERIC_READ, FALSE, FILE_SHARE_READ);
    DWORD e1 = GetLastError();
    SET_ERR(0);
    HANDLE h2 = CreateFileW(w, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    DWORD e2 = GetLastError();
    CHECK(h1 == INVALID_HANDLE_VALUE && h2 == INVALID_HANDLE_VALUE);
    EXPECT_EQ(e1, e2);
}
