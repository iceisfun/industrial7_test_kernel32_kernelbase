/* modules_loader: MUI information APIs and loader-adjacent extra edge cases */
#include "ml.h"

#ifndef MUI_QUERY_TYPE
#define MUI_QUERY_TYPE 0x1
#endif
UDECL(BOOL, GetFileMUIInfo, (DWORD, PCWSTR, PVOID, DWORD *));

TEST(mui_fileinfo_null_path) {
    REQUIRE(UN(GetFileMUIInfo));
    DWORD cb = 0;
    SetLastError(0);
    CHECK(!UN(GetFileMUIInfo)(MUI_QUERY_TYPE, NULL, NULL, &cb));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(mui_fileinfo_null_size_pointer) {
    REQUIRE(UN(GetFileMUIInfo));
    SetLastError(0);
    CHECK(!UN(GetFileMUIInfo)(MUI_QUERY_TYPE, L"kernel32.dll", NULL, NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(mui_fileinfo_size_without_buffer) {
    REQUIRE(UN(GetFileMUIInfo));
    char buf[16]; DWORD cb = sizeof buf;
    SetLastError(0);
    CHECK(!UN(GetFileMUIInfo)(MUI_QUERY_TYPE, L"kernel32.dll", NULL, &cb));   /* nonzero size, NULL buffer */
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(mui_fileinfo_size_query_system_dll) {
    REQUIRE(UN(GetFileMUIInfo));
    WCHAR p[MAX_PATH]; GetSystemDirectoryW(p, MAX_PATH); wcscat(p, L"\\kernel32.dll");
    DWORD cb = 0;
    SetLastError(0);
    BOOL r = UN(GetFileMUIInfo)(MUI_QUERY_TYPE, p, NULL, &cb);
    t_note("GetFileMUIInfo size query -> %d cb=%lu err=%lu", r, cb, GetLastError());
    if (!r && GetLastError() == ERROR_INSUFFICIENT_BUFFER) CHECK(cb >= 0x48);
}
TEST(mui_fileinfo_nonexistent_file) {
    REQUIRE(UN(GetFileMUIInfo));
    DWORD cb = 0;
    BOOL r = UN(GetFileMUIInfo)(MUI_QUERY_TYPE, L"C:\\k32t_no_such_dir\\none.dll", NULL, &cb);
    CHECK(!r);
    t_note("GetFileMUIInfo(nonexistent) err %lu", GetLastError());
}
TEST(ld_extra_all_space_name_rejected) {
    REQUIRE(U(LoadLibraryExW)); REQUIRE(U(LoadLibraryExA));
    /* trailing blanks are trimmed; a name of only blanks is empty and rejected (verified from the loader) */
    SetLastError(0);
    CHECK(U(LoadLibraryExW)(L"   ", NULL, 0) == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SetLastError(0);
    CHECK(U(LoadLibraryExA)(" ", NULL, 0) == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(ld_extra_datafile_flag_combos_reject) {
    REQUIRE(U(LoadLibraryExW));
    /* every value contains DATAFILE (0x2) together with DATAFILE_EXCLUSIVE (0x40) */
    static const DWORD f[] = { 0x43, 0x4A, 0x142, 0x1042, 0x8042 };
    for (int i = 0; i < 5; i++) {
        SetLastError(0);
        CHECKF(U(LoadLibraryExW)(L"kernel32.dll", NULL, f[i]) == NULL, "flags %lx", f[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
