/* Output buffers and output pointers in read-only or no-access memory for the
 * query APIs and for ReadFile/WriteFile's own out-parameters. Every case runs in
 * a child: the exit code is R_OK/R_ERR when the API returned, or the exception
 * code when it faulted in user mode. */
#define UB_TU "i"
#include "ub.h"

static void *ro_page(void) { return ub_pages(1, PAGE_READONLY, 0); }
static void *na_page(void) { return ub_pages(1, PAGE_NOACCESS, 0); }
static WCHAR *wpath(const char *a) { static WCHAR w[MAX_PATH * 2]; MultiByteToWideChar(CP_ACP, 0, a, -1, w, MAX_PATH * 2); return w; }

/* ---- ReadFile / WriteFile out-parameters ---- */
CHILD(xubuf_c_read_bytes_readonly) {
    ub_child_init();
    HANDLE h = ub_open(argv[1], GENERIC_READ, 0); if (h == INVALID_HANDLE_VALUE) return R_SETUP;
    BYTE b[64]; SetLastError(0xBEEF);
    return (int)R_BOOL(U(ReadFile)(h, b, sizeof b, (DWORD *)ro_page(), NULL));
}
CHILD(xubuf_c_write_bytes_readonly) {
    ub_child_init();
    HANDLE h = ub_open(argv[1], GENERIC_WRITE, 0); if (h == INVALID_HANDLE_VALUE) return R_SETUP;
    SetLastError(0xBEEF);
    return (int)R_BOOL(U(WriteFile)(h, "abc", 3, (DWORD *)ro_page(), NULL));
}
CHILD(xubuf_c_read_overlapped_readonly) {
    ub_child_init();
    HANDLE h = ub_open(argv[1], GENERIC_READ, FILE_FLAG_OVERLAPPED); if (h == INVALID_HANDLE_VALUE) return R_SETUP;
    BYTE b[64]; SetLastError(0xBEEF);
    return (int)R_BOOL(U(ReadFile)(h, b, sizeof b, NULL, (OVERLAPPED *)ro_page()));
}
CHILD(xubuf_c_read_overlapped_noaccess) {
    ub_child_init();
    HANDLE h = ub_open(argv[1], GENERIC_READ, FILE_FLAG_OVERLAPPED); if (h == INVALID_HANDLE_VALUE) return R_SETUP;
    BYTE b[64]; SetLastError(0xBEEF);
    return (int)R_BOOL(U(ReadFile)(h, b, sizeof b, NULL, (OVERLAPPED *)na_page()));
}
/* Both the buffer and the count are bad: which one is noticed first. */
CHILD(xubuf_c_read_both_bad) {
    ub_child_init();
    HANDLE h = ub_open(argv[1], GENERIC_READ, 0); if (h == INVALID_HANDLE_VALUE) return R_SETUP;
    SetLastError(0xBEEF);
    return (int)R_BOOL(U(ReadFile)(h, na_page(), 64, (DWORD *)ro_page(), NULL));
}

static DWORD run_on_file(const char *child, DWORD size) {
    char p[MAX_PATH]; ub_path(p, "f"); ub_mkfile(p, size);
    DWORD rc = ub_child(child, p);
    ub_rm(p); return rc;
}
TEST(xubuf_readfile_bytes_read_readonly_faults) { REQUIRE(U(ReadFile)); EXPECT_EQ(run_on_file("xubuf_c_read_bytes_readonly", 100), EXC_AV); }
TEST(xubuf_writefile_bytes_written_readonly_faults) { REQUIRE(U(WriteFile)); EXPECT_EQ(run_on_file("xubuf_c_write_bytes_readonly", 100), EXC_AV); }
TEST(xubuf_readfile_overlapped_readonly_faults) { REQUIRE(U(ReadFile)); EXPECT_EQ(run_on_file("xubuf_c_read_overlapped_readonly", 100), EXC_AV); }
TEST(xubuf_readfile_overlapped_noaccess_faults) { REQUIRE(U(ReadFile)); EXPECT_EQ(run_on_file("xubuf_c_read_overlapped_noaccess", 100), EXC_AV); }
TEST(xubuf_readfile_count_checked_before_buffer) { REQUIRE(U(ReadFile)); EXPECT_EQ(run_on_file("xubuf_c_read_both_bad", 100), EXC_AV); }

/* ---- GetFileInformationByHandleEx ---- */
static DWORD gfi(const char *path, DWORD flags, FILE_INFO_BY_HANDLE_CLASS cls, void *buf, DWORD size) {
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, flags, NULL);
    if (h == INVALID_HANDLE_VALUE) return R_SETUP;
    SetLastError(0xBEEF);
    return R_BOOL(U(GetFileInformationByHandleEx)(h, cls, buf, size));
}
CHILD(xubuf_c_gfi_basic_readonly) { ub_child_init(); return (int)gfi(argv[1], 0, FileBasicInfo, ro_page(), sizeof(FILE_BASIC_INFO)); }
CHILD(xubuf_c_gfi_standard_noaccess) { ub_child_init(); return (int)gfi(argv[1], 0, FileStandardInfo, na_page(), sizeof(FILE_STANDARD_INFO)); }
CHILD(xubuf_c_gfi_name_readonly) { ub_child_init(); return (int)gfi(argv[1], 0, FileNameInfo, ro_page(), 1024); }
CHILD(xubuf_c_gfi_attrtag_readonly) { ub_child_init(); return (int)gfi(argv[1], 0, FileAttributeTagInfo, ro_page(), sizeof(FILE_ATTRIBUTE_TAG_INFO)); }
CHILD(xubuf_c_gfi_iddir_readonly) {
    ub_child_init();
    char d[MAX_PATH * 2]; snprintf(d, sizeof d, "%s", argv[1]);
    char *s = strrchr(d, '\\'); if (s) *s = 0;
    return (int)gfi(d, FILE_FLAG_BACKUP_SEMANTICS, FileIdBothDirectoryInfo, ro_page(), 4096);
}
CHILD(xubuf_c_gfi_basic_too_small_noaccess) { ub_child_init(); return (int)gfi(argv[1], 0, FileBasicInfo, na_page(), 4); }

TEST(xubuf_gfibhe_basic_readonly) { REQUIRE(U(GetFileInformationByHandleEx)); EXPECT_EQ(run_on_file("xubuf_c_gfi_basic_readonly", 10), R_ERR(ERROR_NOACCESS)); }
TEST(xubuf_gfibhe_standard_noaccess) { REQUIRE(U(GetFileInformationByHandleEx)); EXPECT_EQ(run_on_file("xubuf_c_gfi_standard_noaccess", 10), R_ERR(ERROR_NOACCESS)); }
TEST(xubuf_gfibhe_name_readonly) { REQUIRE(U(GetFileInformationByHandleEx)); EXPECT_EQ(run_on_file("xubuf_c_gfi_name_readonly", 10), R_ERR(ERROR_NOACCESS)); }
TEST(xubuf_gfibhe_attrtag_readonly) { REQUIRE(U(GetFileInformationByHandleEx)); EXPECT_EQ(run_on_file("xubuf_c_gfi_attrtag_readonly", 10), R_ERR(ERROR_NOACCESS)); }
TEST(xubuf_gfibhe_iddir_readonly) { REQUIRE(U(GetFileInformationByHandleEx)); EXPECT_EQ(run_on_file("xubuf_c_gfi_iddir_readonly", 10), R_ERR(ERROR_NOACCESS)); }
/* A size that is too small is rejected before the buffer is touched. */
TEST(xubuf_gfibhe_too_small_before_buffer) { REQUIRE(U(GetFileInformationByHandleEx)); EXPECT_EQ(run_on_file("xubuf_c_gfi_basic_too_small_noaccess", 10), R_ERR(ERROR_BAD_LENGTH)); }

/* ---- GetFileAttributesExW ---- */
CHILD(xubuf_c_gfaex_readonly) { ub_child_init(); SetLastError(0xBEEF); return (int)R_BOOL(U(GetFileAttributesExW)(wpath(argv[1]), GetFileExInfoStandard, ro_page())); }
CHILD(xubuf_c_gfaex_missing_noaccess) {
    ub_child_init(); char p[MAX_PATH * 2]; snprintf(p, sizeof p, "%s.missing", argv[1]); SetLastError(0xBEEF);
    return (int)R_BOOL(U(GetFileAttributesExW)(wpath(p), GetFileExInfoStandard, na_page()));
}
CHILD(xubuf_c_gfaex_badlevel_noaccess) { ub_child_init(); SetLastError(0xBEEF); return (int)R_BOOL(U(GetFileAttributesExW)(wpath(argv[1]), (GET_FILEEX_INFO_LEVELS)7, na_page())); }
TEST(xubuf_gfaex_readonly_out) { REQUIRE(U(GetFileAttributesExW)); EXPECT_EQ(run_on_file("xubuf_c_gfaex_readonly", 10), EXC_AV); }
TEST(xubuf_gfaex_missing_file_error_first) { REQUIRE(U(GetFileAttributesExW)); EXPECT_EQ(run_on_file("xubuf_c_gfaex_missing_noaccess", 10), R_ERR(ERROR_FILE_NOT_FOUND)); }
TEST(xubuf_gfaex_bad_level_error_first) { REQUIRE(U(GetFileAttributesExW)); EXPECT_EQ(run_on_file("xubuf_c_gfaex_badlevel_noaccess", 10), R_ERR(ERROR_INVALID_PARAMETER)); }

/* ---- GetVolumeInformationByHandleW: every out-parameter is guarded (ERROR_NOACCESS) ---- */
static DWORD gvi(const char *path, int which, void *bad) {
    HANDLE h = ub_open(path, GENERIC_READ, 0); if (h == INVALID_HANDLE_VALUE) return R_SETUP;
    WCHAR name[MAX_PATH + 1], fs[MAX_PATH + 1]; DWORD serial, maxc, flags;
    SetLastError(0xBEEF);
    BOOL r = U(GetVolumeInformationByHandleW)(h,
        which == 0 ? (WCHAR *)bad : name, MAX_PATH + 1,
        which == 1 ? (DWORD *)bad : &serial, which == 2 ? (DWORD *)bad : &maxc,
        which == 3 ? (DWORD *)bad : &flags, which == 4 ? (WCHAR *)bad : fs, MAX_PATH + 1);
    return R_BOOL(r);
}
CHILD(xubuf_c_gvi_name_readonly) { ub_child_init(); return (int)gvi(argv[1], 0, ro_page()); }
CHILD(xubuf_c_gvi_serial_readonly) { ub_child_init(); return (int)gvi(argv[1], 1, ro_page()); }
CHILD(xubuf_c_gvi_flags_readonly) { ub_child_init(); return (int)gvi(argv[1], 3, ro_page()); }
CHILD(xubuf_c_gvi_fsname_noaccess) { ub_child_init(); return (int)gvi(argv[1], 4, na_page()); }
TEST(xubuf_gvibh_volume_name_readonly) { REQUIRE(U(GetVolumeInformationByHandleW)); EXPECT_EQ(run_on_file("xubuf_c_gvi_name_readonly", 10), R_ERR(ERROR_NOACCESS)); }
TEST(xubuf_gvibh_serial_readonly) { REQUIRE(U(GetVolumeInformationByHandleW)); EXPECT_EQ(run_on_file("xubuf_c_gvi_serial_readonly", 10), R_ERR(ERROR_NOACCESS)); }
TEST(xubuf_gvibh_flags_readonly) { REQUIRE(U(GetVolumeInformationByHandleW)); EXPECT_EQ(run_on_file("xubuf_c_gvi_flags_readonly", 10), R_ERR(ERROR_NOACCESS)); }
TEST(xubuf_gvibh_fs_name_noaccess) { REQUIRE(U(GetVolumeInformationByHandleW)); EXPECT_EQ(run_on_file("xubuf_c_gvi_fsname_noaccess", 10), R_ERR(ERROR_NOACCESS)); }

/* ---- FindFirstFileW / FindNextFileW ---- */
CHILD(xubuf_c_findfirst_readonly) {
    ub_child_init(); SetLastError(0xBEEF);
    HANDLE f = U(FindFirstFileW)(wpath(argv[1]), (WIN32_FIND_DATAW *)ro_page());
    return (int)(f != INVALID_HANDLE_VALUE ? R_OK(0) : R_ERR(GetLastError()));
}
CHILD(xubuf_c_findfirst_nomatch_noaccess) {
    ub_child_init(); char p[MAX_PATH * 2]; snprintf(p, sizeof p, "%s.none*", argv[1]); SetLastError(0xBEEF);
    HANDLE f = U(FindFirstFileW)(wpath(p), (WIN32_FIND_DATAW *)na_page());
    return (int)(f != INVALID_HANDLE_VALUE ? R_OK(0) : R_ERR(GetLastError()));
}
CHILD(xubuf_c_findnext_readonly) {
    ub_child_init();
    char d[MAX_PATH * 2]; snprintf(d, sizeof d, "%s", argv[1]);
    char *s = strrchr(d, '\\'); if (!s) return R_SETUP; strcpy(s + 1, "*");
    WIN32_FIND_DATAW fd; HANDLE f = FindFirstFileW(wpath(d), &fd);
    if (f == INVALID_HANDLE_VALUE) return R_SETUP;
    SetLastError(0xBEEF);
    return (int)R_BOOL(U(FindNextFileW)(f, (WIN32_FIND_DATAW *)ro_page()));
}
/* Exhausted enumeration: the end is reported without touching the buffer. */
CHILD(xubuf_c_findnext_end_noaccess) {
    ub_child_init();
    WIN32_FIND_DATAW fd; HANDLE f = FindFirstFileW(wpath(argv[1]), &fd);
    if (f == INVALID_HANDLE_VALUE) return R_SETUP;
    SetLastError(0xBEEF);
    return (int)R_BOOL(U(FindNextFileW)(f, (WIN32_FIND_DATAW *)na_page()));
}
TEST(xubuf_findfirst_data_readonly) { REQUIRE(U(FindFirstFileW)); EXPECT_EQ(run_on_file("xubuf_c_findfirst_readonly", 10), EXC_AV); }
TEST(xubuf_findfirst_nomatch_error_first) { REQUIRE(U(FindFirstFileW)); EXPECT_EQ(run_on_file("xubuf_c_findfirst_nomatch_noaccess", 10), R_ERR(ERROR_FILE_NOT_FOUND)); }
TEST(xubuf_findnext_data_readonly) { REQUIRE(U(FindNextFileW)); EXPECT_EQ(run_on_file("xubuf_c_findnext_readonly", 10), EXC_AV); }
TEST(xubuf_findnext_end_noaccess_no_more_files) { REQUIRE(U(FindNextFileW)); EXPECT_EQ(run_on_file("xubuf_c_findnext_end_noaccess", 10), R_ERR(ERROR_NO_MORE_FILES)); }

/* ---- GetFullPathNameW ---- */
CHILD(xubuf_c_fullpath_readonly) {
    ub_child_init(); SetLastError(0xBEEF);
    DWORD n = U(GetFullPathNameW)(L"C:\\k32t_x\\a\\..\\b.txt", MAX_PATH, (WCHAR *)ro_page(), NULL);
    return (int)(n ? R_OK(n) : R_ERR(GetLastError()));
}
CHILD(xubuf_c_fullpath_small_readonly) {
    ub_child_init(); SetLastError(0xBEEF);
    DWORD n = U(GetFullPathNameW)(L"C:\\k32t_x\\a\\..\\b.txt", 4, (WCHAR *)ro_page(), NULL);
    return (int)(n ? R_OK(n) : R_ERR(GetLastError()));
}
CHILD(xubuf_c_fullpath_filepart_readonly) {
    ub_child_init(); WCHAR out[MAX_PATH]; SetLastError(0xBEEF);
    DWORD n = U(GetFullPathNameW)(L"C:\\k32t_x\\b.txt", MAX_PATH, out, (WCHAR **)ro_page());
    return (int)(n ? R_OK(n) : R_ERR(GetLastError()));
}
TEST(xubuf_fullpath_out_readonly_faults) { REQUIRE(U(GetFullPathNameW)); EXPECT_EQ(ub_child("xubuf_c_fullpath_readonly", ""), EXC_AV); }
/* Even with a length too small for the result the buffer is written (faults). */
TEST(xubuf_fullpath_small_readonly_still_faults) { REQUIRE(U(GetFullPathNameW)); EXPECT_EQ(ub_child("xubuf_c_fullpath_small_readonly", ""), EXC_AV); }
TEST(xubuf_fullpath_filepart_readonly_faults) { REQUIRE(U(GetFullPathNameW)); EXPECT_EQ(ub_child("xubuf_c_fullpath_filepart_readonly", ""), EXC_AV); }

/* ---- GetFinalPathNameByHandleW ---- */
static DWORD gfp(const char *path, WCHAR *buf, DWORD cch) {
    HANDLE h = ub_open(path, GENERIC_READ, 0); if (h == INVALID_HANDLE_VALUE) return R_SETUP;
    SetLastError(0xBEEF);
    DWORD n = U(GetFinalPathNameByHandleW)(h, buf, cch, 0);
    /* R_OK(1): the return value is a required size larger than the buffer */
    return n ? R_OK(n > cch) : R_ERR(GetLastError());
}
CHILD(xubuf_c_finalpath_readonly) { ub_child_init(); return (int)gfp(argv[1], (WCHAR *)ro_page(), MAX_PATH); }
CHILD(xubuf_c_finalpath_noaccess) { ub_child_init(); return (int)gfp(argv[1], (WCHAR *)na_page(), MAX_PATH); }
CHILD(xubuf_c_finalpath_small_noaccess) { ub_child_init(); return (int)gfp(argv[1], (WCHAR *)na_page(), 2); }
TEST(xubuf_finalpath_out_readonly_faults) { REQUIRE(U(GetFinalPathNameByHandleW)); EXPECT_EQ(run_on_file("xubuf_c_finalpath_readonly", 10), EXC_AV); }
TEST(xubuf_finalpath_out_noaccess_faults) { REQUIRE(U(GetFinalPathNameByHandleW)); EXPECT_EQ(run_on_file("xubuf_c_finalpath_noaccess", 10), EXC_AV); }
/* Too small: returns the required length, nothing written. */
TEST(xubuf_finalpath_small_noaccess_returns_size) { REQUIRE(U(GetFinalPathNameByHandleW)); EXPECT_EQ(run_on_file("xubuf_c_finalpath_small_noaccess", 10), R_OK(1)); }
