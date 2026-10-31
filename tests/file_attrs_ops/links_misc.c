/* CreateHardLink, CreateSymbolicLink, GetCompressedFileSize, SetFileShortName,
 * bandwidth reservation, SetFileIoOverlappedRange, OpenFileById */
#include "fa.h"
#include <winioctl.h>

static void pp(char *p, const char *leaf) { fa_path(p, leaf); }
static int real_ntfs(void) { return fa_is_ntfs() && !t_is_wine(); }
static BOOL hlA(const char *n, const char *e) { return U(CreateHardLinkA)(n, e, NULL); }
static BOOL hlW(const char *n, const char *e) { WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_widen(a, n); fa_widen(b, e); return U(CreateHardLinkW)(a, b, NULL); }
static DWORD nlinks(const char *p) {
    HANDLE h = fa_open(p, 0, FA_SHARE_ALL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS);
    BY_HANDLE_FILE_INFORMATION bi; DWORD n = 0;
    if (h == INVALID_HANDLE_VALUE) return 0;
    if (GetFileInformationByHandle(h, &bi)) n = bi.nNumberOfLinks;
    CloseHandle(h); return n;
}

/* ---------------- CreateHardLink ---------------- */
TEST(flink_hard_basic_a_w) {
    REQUIRE(U(CreateHardLinkA)); REQUIRE(U(CreateHardLinkW)); FA_REQUIRE_NTFS();
    char e[MAX_PATH * 2], l1[MAX_PATH * 2], l2[MAX_PATH * 2]; fa_mkfile_s("hl_e.txt", "shared body"); pp(e, "hl_e.txt"); pp(l1, "hl_l1.txt"); pp(l2, "hl_l2.txt");
    CHECKF(hlA(l1, e), "A err %lu", GetLastError());
    CHECKF(hlW(l2, e), "W err %lu", GetLastError());
    CHECK(fa_content_is(l1, "shared body")); CHECK(fa_content_is(l2, "shared body"));
    EXPECT_EQ(nlinks(e), 3);
    fa_rm(l1); EXPECT_EQ(nlinks(e), 2);
    fa_rm(l2); fa_rm(e);
}
TEST(flink_hard_write_visible_through_all_names) {
    REQUIRE(U(CreateHardLinkW)); FA_REQUIRE_NTFS();
    char e[MAX_PATH * 2], l[MAX_PATH * 2]; fa_mkfile_s("hl_w.txt", "AAAA"); pp(e, "hl_w.txt"); pp(l, "hl_wl.txt");
    CHECK(hlW(l, e));
    HANDLE h = fa_open(l, GENERIC_WRITE, FA_SHARE_ALL, OPEN_EXISTING, 0); DWORD n; WriteFile(h, "BB", 2, &n, NULL); CloseHandle(h);
    CHECK(fa_content_is(e, "BBAA"));
    fa_rm(l); fa_rm(e);
}
TEST(flink_hard_survives_original_delete) {
    REQUIRE(U(CreateHardLinkA)); FA_REQUIRE_NTFS();
    char e[MAX_PATH * 2], l[MAX_PATH * 2]; fa_mkfile_s("hl_d.txt", "keep"); pp(e, "hl_d.txt"); pp(l, "hl_dl.txt");
    CHECK(hlA(l, e));
    CHECK(DeleteFileA(e));
    CHECK(fa_content_is(l, "keep")); EXPECT_EQ(nlinks(l), 1);
    fa_rm(l);
}
TEST(flink_hard_same_file_id) {
    REQUIRE(U(CreateHardLinkA)); FA_REQUIRE_NTFS();
    char e[MAX_PATH * 2], l[MAX_PATH * 2]; fa_mkfile_s("hl_i.txt", "id"); pp(e, "hl_i.txt"); pp(l, "hl_il.txt");
    CHECK(hlA(l, e));
    HANDLE a = fa_open(e, GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0), b = fa_open(l, GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0);
    BY_HANDLE_FILE_INFORMATION x, y; GetFileInformationByHandle(a, &x); GetFileInformationByHandle(b, &y);
    EXPECT_EQ(x.nFileIndexLow, y.nFileIndexLow); EXPECT_EQ(x.nFileIndexHigh, y.nFileIndexHigh);
    EXPECT_EQ(x.dwVolumeSerialNumber, y.dwVolumeSerialNumber);
    CloseHandle(a); CloseHandle(b); fa_rm(l); fa_rm(e);
}
TEST(flink_hard_dest_exists) {
    REQUIRE(U(CreateHardLinkA)); REQUIRE(U(CreateHardLinkW)); FA_REQUIRE_NTFS();
    char e[MAX_PATH * 2], l[MAX_PATH * 2]; fa_mkfile_s("hl_x1.txt", "a"); fa_mkfile_s("hl_x2.txt", "b"); pp(e, "hl_x1.txt"); pp(l, "hl_x2.txt");
    SetLastError(0); EXPECT_EQ(hlA(l, e), FALSE); EXPECT_ERR(ERROR_ALREADY_EXISTS);
    SetLastError(0); EXPECT_EQ(hlW(l, e), FALSE); EXPECT_ERR(ERROR_ALREADY_EXISTS);
    SetLastError(0); EXPECT_EQ(hlA(e, e), FALSE); EXPECT_ERR(ERROR_ALREADY_EXISTS);
    CHECK(fa_content_is(l, "b"));
    fa_rm(e); fa_rm(l);
}
TEST(flink_hard_missing_existing_file) {
    REQUIRE(U(CreateHardLinkA)); REQUIRE(U(CreateHardLinkW));
    char e[MAX_PATH * 2], l[MAX_PATH * 2]; pp(e, "hl_none.txt"); pp(l, "hl_none_l.txt");
    SetLastError(0); EXPECT_EQ(hlA(l, e), FALSE); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    SetLastError(0); EXPECT_EQ(hlW(l, e), FALSE); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    CHECK(!fa_exists(l));
    pp(e, "hl_nodir\\x.txt");
    SetLastError(0); EXPECT_EQ(hlA(l, e), FALSE); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(flink_hard_missing_link_dir) {
    REQUIRE(U(CreateHardLinkA)); FA_REQUIRE_NTFS();
    char e[MAX_PATH * 2], l[MAX_PATH * 2]; fa_mkfile_s("hl_md.txt", "x"); pp(e, "hl_md.txt"); pp(l, "hl_nodir\\l.txt");
    SetLastError(0); EXPECT_EQ(hlA(l, e), FALSE); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    fa_rm(e);
}
TEST(flink_hard_to_directory_denied) {
    REQUIRE(U(CreateHardLinkA)); FA_REQUIRE_NTFS();
    char e[MAX_PATH * 2], l[MAX_PATH * 2]; fa_mkdir("hl_dir"); pp(e, "hl_dir"); pp(l, "hl_dir_l");
    SetLastError(0); EXPECT_EQ(hlA(l, e), FALSE);
    DWORD er = GetLastError(); CHECKF(er == ERROR_ACCESS_DENIED || er == ERROR_INVALID_PARAMETER || er == ERROR_INVALID_FUNCTION, "err %lu", er);
    t_note("hard link to a directory err=%lu", er);
    CHECK(!fa_exists(l));
    fa_rm(e);
}
TEST(flink_hard_null_args) {
    REQUIRE(U(CreateHardLinkW));
    WCHAR w[] = L"C:\\x";
    SetLastError(0); EXPECT_EQ(U(CreateHardLinkW)(NULL, w, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SetLastError(0); EXPECT_EQ(U(CreateHardLinkW)(w, NULL, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SetLastError(0); EXPECT_EQ(U(CreateHardLinkW)(NULL, NULL, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    if (!t_is_wine()) { SetLastError(0); EXPECT_EQ(U(CreateHardLinkA)(NULL, "x", NULL), FALSE);
    t_note("CreateHardLinkA(NULL, x) err=%lu", GetLastError()); }
}
TEST(flink_hard_invalid_names) {
    REQUIRE(U(CreateHardLinkA)); FA_REQUIRE_NTFS();
    char e[MAX_PATH * 2], l[MAX_PATH * 2]; fa_mkfile_s("hl_in.txt", "x"); pp(e, "hl_in.txt");
    pp(l, "hl_in*.lnk");
    SetLastError(0); EXPECT_EQ(hlA(l, e), FALSE); EXPECT_ERR(ERROR_INVALID_NAME);
    pp(l, "hl_in?.lnk");
    SetLastError(0); EXPECT_EQ(hlA(l, e), FALSE); EXPECT_ERR(ERROR_INVALID_NAME);
    fa_rm(e);
}
TEST(flink_hard_readonly_shares_attributes) {
    REQUIRE(U(CreateHardLinkA)); FA_REQUIRE_NTFS();
    char e[MAX_PATH * 2], l[MAX_PATH * 2]; fa_mkfile_s("hl_ro.txt", "x"); pp(e, "hl_ro.txt"); pp(l, "hl_ro_l.txt");
    SetFileAttributesA(e, FILE_ATTRIBUTE_READONLY);
    CHECKF(hlA(l, e), "err %lu", GetLastError());
    CHECK(GetFileAttributesA(l) & FILE_ATTRIBUTE_READONLY);
    SetFileAttributesA(l, FILE_ATTRIBUTE_NORMAL);
    CHECK(!(GetFileAttributesA(e) & FILE_ATTRIBUTE_READONLY));
    fa_rm(l); fa_rm(e);
}
TEST(flink_hard_source_open_exclusive) {
    REQUIRE(U(CreateHardLinkA)); FA_REQUIRE_NTFS();
    char e[MAX_PATH * 2], l[MAX_PATH * 2]; fa_mkfile_s("hl_ex.txt", "x"); pp(e, "hl_ex.txt"); pp(l, "hl_ex_l.txt");
    HANDLE h = fa_open(e, GENERIC_READ | GENERIC_WRITE, 0, OPEN_EXISTING, 0);
    SetLastError(0); BOOL r = hlA(l, e);
    t_note("hard link to exclusively open file: r=%d err=%lu", r, r ? 0 : GetLastError());
    CloseHandle(h); fa_rm(l); fa_rm(e);
}
TEST(flink_hard_many_links) {
    REQUIRE(U(CreateHardLinkA)); FA_REQUIRE_NTFS();
    char e[MAX_PATH * 2]; fa_mkfile_s("hl_many.txt", "x"); pp(e, "hl_many.txt");
    int ok = 0;
    for (int i = 0; i < 40; i++) { char leaf[64], l[MAX_PATH * 2]; snprintf(leaf, sizeof leaf, "hl_many_%d.lnk", i); pp(l, leaf); if (hlA(l, e)) ok++; }
    EXPECT_EQ(ok, 40); EXPECT_EQ(nlinks(e), 41);
    for (int i = 0; i < 40; i++) { char leaf[64]; snprintf(leaf, sizeof leaf, "hl_many_%d.lnk", i); fa_rml(leaf); }
    fa_rm(e);
}
TEST(flink_hard_within_subdirectory) {
    REQUIRE(U(CreateHardLinkW)); FA_REQUIRE_NTFS();
    fa_mkdir("hl_sd"); char e[MAX_PATH * 2], l[MAX_PATH * 2]; fa_mkfile_s("hl_sd1.txt", "sub"); pp(e, "hl_sd1.txt"); pp(l, "hl_sd\\l.txt");
    CHECK(hlW(l, e)); CHECK(fa_content_is(l, "sub"));
    fa_rm(l); fa_rm(e); fa_rml("hl_sd");
}
TEST(flink_hard_prefixed_paths) {
    REQUIRE(U(CreateHardLinkW)); FA_REQUIRE_NTFS();
    char e[MAX_PATH * 2], l[MAX_PATH * 2], qe[MAX_PATH * 2], ql[MAX_PATH * 2]; fa_mkfile_s("hl_pf.txt", "pf"); pp(e, "hl_pf.txt"); pp(l, "hl_pf_l.txt");
    snprintf(qe, sizeof qe, "\\\\?\\%s", e); snprintf(ql, sizeof ql, "\\\\?\\%s", l);
    CHECKF(hlW(ql, qe), "err %lu", GetLastError());
    CHECK(fa_content_is(l, "pf"));
    fa_rm(l); fa_rm(e);
}
TEST(flink_hard_stream_source) {
    REQUIRE(U(CreateHardLinkA)); FA_REQUIRE_STREAMS();
    char e[MAX_PATH * 2], l[MAX_PATH * 2], q[MAX_PATH * 2]; fa_mkfile_s("hl_st.txt", "m"); pp(e, "hl_st.txt"); pp(l, "hl_st_l.txt");
    snprintf(q, sizeof q, "%s:s", e); fa_mkfile(q, "ss", 2);
    SetLastError(0); BOOL r = hlA(l, q);
    t_note("hard link from a stream name: r=%d err=%lu", r, r ? 0 : GetLastError());
    CHECK(!r);
    fa_rm(l); fa_rm(e);
}

/* ---------------- CreateSymbolicLink ---------------- */
static int try_symlink(const char *link, const char *target, DWORD flags, DWORD *err) {
    BOOLEAN r = U(CreateSymbolicLinkA)(link, target, flags);
    if (err) *err = GetLastError();
    return r != 0;
}
TEST(flink_sym_null_args) {
    REQUIRE(U(CreateSymbolicLinkW)); REQUIRE(U(CreateSymbolicLinkA));
    if (t_is_wine()) { t_skip("emulator faults on NULL names"); return; }
    WCHAR w[] = L"x";
    SetLastError(0); EXPECT_EQ(U(CreateSymbolicLinkW)(NULL, w, 0), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SetLastError(0); EXPECT_EQ(U(CreateSymbolicLinkW)(w, NULL, 0), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SetLastError(0); EXPECT_EQ(U(CreateSymbolicLinkW)(NULL, NULL, 0), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SetLastError(0); EXPECT_EQ(U(CreateSymbolicLinkW)(NULL, w, 3), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(flink_sym_privilege_or_success) {
    REQUIRE(U(CreateSymbolicLinkA));
    FA_REQUIRE_NTFS();
    char l[MAX_PATH * 2], t[MAX_PATH * 2]; fa_mkfile_s("sl_t.txt", "target body"); pp(l, "sl_l.txt"); pp(t, "sl_t.txt");
    DWORD e = 0;
    int ok = try_symlink(l, t, 0, &e);
    if (!ok) {
        CHECKF(e == ERROR_PRIVILEGE_NOT_HELD, "unexpected err %lu", e);
        t_note("symlink without privilege: err=%lu", e);
    } else {
        t_note("process holds SeCreateSymbolicLinkPrivilege (elevated)");
        CHECK(fa_content_is(l, "target body"));
        DWORD a = GetFileAttributesA(l); CHECK(a & FILE_ATTRIBUTE_REPARSE_POINT);
        DeleteFileA(l);
    }
    fa_rm(t);
}
TEST(flink_sym_unprivileged_flag) {
    REQUIRE(U(CreateSymbolicLinkA));
    FA_REQUIRE_NTFS();
    char l[MAX_PATH * 2], t[MAX_PATH * 2]; fa_mkfile_s("sl2_t.txt", "dev mode body"); pp(l, "sl2_l.txt"); pp(t, "sl2_t.txt");
    DWORD e = 0;
    int ok = try_symlink(l, t, 2 /*SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE*/, &e);
    if (!ok) {
        CHECKF(e == ERROR_PRIVILEGE_NOT_HELD, "unexpected err %lu", e);
        t_note("unprivileged symlink creation refused (Developer Mode off): err=%lu", e);
    } else {
        CHECK(fa_content_is(l, "dev mode body"));
        t_note("unprivileged symlink creation allowed");
        DeleteFileA(l);
    }
    fa_rm(t);
}
TEST(flink_sym_file_link_properties) {
    REQUIRE(U(CreateSymbolicLinkA)); REQUIRE(U(GetFileInformationByHandleEx));
    FA_REQUIRE_NTFS();
    char l[MAX_PATH * 2], t[MAX_PATH * 2]; fa_mkfile_s("sl3_t.txt", "props"); pp(l, "sl3_l.txt"); pp(t, "sl3_t.txt");
    DWORD e;
    if (!try_symlink(l, t, 2, &e) && !try_symlink(l, t, 0, &e)) { t_skip("cannot create symlink (err %lu)", e); fa_rm(t); return; }
    DWORD a = GetFileAttributesA(l);
    CHECK(a & FILE_ATTRIBUTE_REPARSE_POINT); CHECK(!(a & FILE_ATTRIBUTE_DIRECTORY));
    HANDLE h = fa_open(l, FILE_READ_ATTRIBUTES, FA_SHARE_ALL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT);
    CHECK(h != INVALID_HANDLE_VALUE);
    FILE_ATTRIBUTE_TAG_INFO ti; memset(&ti, 0, sizeof ti);
    CHECK(U(GetFileInformationByHandleEx)(h, FileAttributeTagInfo, &ti, sizeof ti));
    EXPECT_EQ(ti.ReparseTag, 0xA000000C);
    CloseHandle(h);
    WIN32_FIND_DATAA fd; HANDLE f = FindFirstFileA(l, &fd);
    if (f != INVALID_HANDLE_VALUE) { EXPECT_EQ(fd.dwReserved0, 0xA000000C); FindClose(f); }
    CHECK(DeleteFileA(l)); CHECK(fa_content_is(t, "props"));
    fa_rm(t);
}
TEST(flink_sym_directory_link) {
    REQUIRE(U(CreateSymbolicLinkA));
    FA_REQUIRE_NTFS();
    fa_mkdir("sl4_dir"); fa_mkfile_s("sl4_dir\\in.txt", "in dir");
    char l[MAX_PATH * 2], t[MAX_PATH * 2], f[MAX_PATH * 2]; pp(l, "sl4_l"); pp(t, "sl4_dir"); pp(f, "sl4_l\\in.txt");
    DWORD e;
    if (!try_symlink(l, t, SYMBOLIC_LINK_FLAG_DIRECTORY | 2, &e) && !try_symlink(l, t, SYMBOLIC_LINK_FLAG_DIRECTORY, &e)) { t_skip("cannot create symlink (err %lu)", e); fa_rml("sl4_dir\\in.txt"); fa_rml("sl4_dir"); return; }
    DWORD a = GetFileAttributesA(l);
    CHECK((a & FILE_ATTRIBUTE_DIRECTORY) && (a & FILE_ATTRIBUTE_REPARSE_POINT));
    CHECK(fa_content_is(f, "in dir"));
    SetLastError(0); EXPECT_EQ(DeleteFileA(l), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CHECK(RemoveDirectoryA(l));
    CHECK(fa_content_is((fa_path(f, "sl4_dir\\in.txt"), f), "in dir"));
    fa_rml("sl4_dir\\in.txt"); fa_rml("sl4_dir");
}
TEST(flink_sym_relative_target) {
    REQUIRE(U(CreateSymbolicLinkA));
    FA_REQUIRE_NTFS();
    fa_mkfile_s("sl5_t.txt", "relative");
    char l[MAX_PATH * 2]; pp(l, "sl5_l.txt");
    DWORD e;
    if (!try_symlink(l, "sl5_t.txt", 2, &e) && !try_symlink(l, "sl5_t.txt", 0, &e)) { t_skip("cannot create symlink (err %lu)", e); fa_rml("sl5_t.txt"); return; }
    CHECK(fa_content_is(l, "relative"));
    DeleteFileA(l); fa_rml("sl5_t.txt");
}
TEST(flink_sym_dangling) {
    REQUIRE(U(CreateSymbolicLinkA));
    FA_REQUIRE_NTFS();
    char l[MAX_PATH * 2], t[MAX_PATH * 2]; pp(l, "sl6_l.txt"); pp(t, "sl6_missing.txt");
    DWORD e;
    if (!try_symlink(l, t, 2, &e) && !try_symlink(l, t, 0, &e)) { t_skip("cannot create symlink (err %lu)", e); return; }
    SetLastError(0); DWORD a = GetFileAttributesA(l);
    t_note("attributes of dangling link: 0x%lx err=%lu", a, a == INVALID_FILE_ATTRIBUTES ? GetLastError() : 0);
    SetLastError(0);
    HANDLE h = fa_open(l, GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0);
    EXPECT_EQ(h, INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    DeleteFileA(l);
}
TEST(flink_sym_dest_exists) {
    REQUIRE(U(CreateSymbolicLinkA));
    FA_REQUIRE_NTFS();
    char l[MAX_PATH * 2], t[MAX_PATH * 2]; fa_mkfile_s("sl7_t.txt", "t"); fa_mkfile_s("sl7_l.txt", "existing"); pp(l, "sl7_l.txt"); pp(t, "sl7_t.txt");
    SetLastError(0); BOOLEAN r = U(CreateSymbolicLinkA)(l, t, 2);
    DWORD e = GetLastError();
    CHECK(!r);
    CHECKF(e == ERROR_ALREADY_EXISTS || e == ERROR_PRIVILEGE_NOT_HELD || e == ERROR_FILE_EXISTS, "err %lu", e);
    CHECK(fa_content_is(l, "existing"));
    fa_rm(l); fa_rm(t);
}
TEST(flink_sym_missing_link_dir) {
    REQUIRE(U(CreateSymbolicLinkA));
    FA_REQUIRE_NTFS();
    char l[MAX_PATH * 2], t[MAX_PATH * 2]; fa_mkfile_s("sl8_t.txt", "t"); pp(l, "sl8_nodir\\l.txt"); pp(t, "sl8_t.txt");
    SetLastError(0); BOOLEAN r = U(CreateSymbolicLinkA)(l, t, 2);
    DWORD e = GetLastError();
    CHECK(!r);
    CHECKF(e == ERROR_PATH_NOT_FOUND || e == ERROR_PRIVILEGE_NOT_HELD, "err %lu", e);
    fa_rm(t);
}
TEST(flink_sym_return_type_is_boolean_byte) {
    REQUIRE(U(CreateSymbolicLinkW));
    WCHAR w[] = L"x";
    BOOLEAN r = U(CreateSymbolicLinkW)(NULL, w, 0);
    EXPECT_EQ(r, 0);
    /* the SDK type is BOOLEAN (1 byte): upper bits of EAX are unspecified, only the low byte is meaningful */
    EXPECT_EQ(sizeof(BOOLEAN), 1);
}
TEST(flink_sym_a_w_parity_null) {
    REQUIRE(U(CreateSymbolicLinkA)); REQUIRE(U(CreateSymbolicLinkW));
    if (t_is_wine()) { t_skip("emulator faults on NULL names"); return; }
    SetLastError(0); BOOLEAN a = U(CreateSymbolicLinkA)("x", NULL, 0); DWORD ea = GetLastError();
    SetLastError(0); BOOLEAN w = U(CreateSymbolicLinkW)(L"x", NULL, 0); DWORD ew = GetLastError();
    EXPECT_EQ(a, w); EXPECT_EQ(ea, ew);
}

/* ---------------- GetCompressedFileSize ---------------- */
TEST(fcsize_plain_file) {
    REQUIRE(U(GetCompressedFileSizeA)); REQUIRE(U(GetCompressedFileSizeW));
    char p[MAX_PATH * 2]; pp(p, "cs_plain.bin");
    BYTE b[1500]; for (int i = 0; i < 1500; i++) b[i] = (BYTE)(i * 31 + 7);
    fa_mkfile(p, b, 1500);
    DWORD hi = 0xdeadbeef;
    DWORD lo = U(GetCompressedFileSizeA)(p, &hi);
    WCHAR w[MAX_PATH * 2]; fa_widen(w, p);
    DWORD hi2 = 0xdeadbeef; DWORD lo2 = U(GetCompressedFileSizeW)(w, &hi2);
    EXPECT_EQ(hi, 0); EXPECT_EQ(hi2, 0);
    EXPECT_EQ(lo, lo2);
    if (fa_is_ntfs() && !t_is_wine()) EXPECT_EQ(lo, 1500);
    else CHECK(lo >= 1500);
    fa_rm(p);
}
TEST(fcsize_null_high_pointer) {
    REQUIRE(U(GetCompressedFileSizeW));
    char p[MAX_PATH * 2]; fa_mkfile_s("cs_nh.txt", "12345"); pp(p, "cs_nh.txt"); WCHAR w[MAX_PATH * 2]; fa_widen(w, p);
    SetLastError(0);
    DWORD lo = U(GetCompressedFileSizeW)(w, NULL);
    CHECK(lo != INVALID_FILE_SIZE);
    CHECK(lo >= 5);
    fa_rm(p);
}
TEST(fcsize_empty_file) {
    REQUIRE(U(GetCompressedFileSizeA));
    char p[MAX_PATH * 2]; fa_mkfile_s("cs_empty.txt", ""); pp(p, "cs_empty.txt");
    DWORD hi = 7; DWORD lo = U(GetCompressedFileSizeA)(p, &hi);
    EXPECT_EQ(lo, 0); EXPECT_EQ(hi, 0);
    fa_rm(p);
}
TEST(fcsize_missing) {
    REQUIRE(U(GetCompressedFileSizeA)); REQUIRE(U(GetCompressedFileSizeW));
    char p[MAX_PATH * 2]; pp(p, "cs_none.txt"); WCHAR w[MAX_PATH * 2]; fa_widen(w, p);
    DWORD hi = 0x1234;
    SetLastError(0); EXPECT_EQ(U(GetCompressedFileSizeA)(p, &hi), INVALID_FILE_SIZE); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    SetLastError(0); EXPECT_EQ(U(GetCompressedFileSizeW)(w, &hi), INVALID_FILE_SIZE); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    pp(p, "cs_nodir\\x");
    SetLastError(0); EXPECT_EQ(U(GetCompressedFileSizeA)(p, &hi), INVALID_FILE_SIZE); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(fcsize_wildcard) {
    REQUIRE(U(GetCompressedFileSizeA));
    char p[MAX_PATH * 2]; pp(p, "cs_*.txt"); DWORD hi;
    SetLastError(0); EXPECT_EQ(U(GetCompressedFileSizeA)(p, &hi), INVALID_FILE_SIZE); EXPECT_ERR(ERROR_INVALID_NAME);
}
TEST(fcsize_null_name) {
    REQUIRE(U(GetCompressedFileSizeW));
    DWORD hi;
    SetLastError(0); EXPECT_EQ(U(GetCompressedFileSizeW)(NULL, &hi), INVALID_FILE_SIZE);
    t_note("GetCompressedFileSizeW(NULL) err=%lu", GetLastError());
}
TEST(fcsize_exclusive_open_ok) {
    REQUIRE(U(GetCompressedFileSizeA));
    char p[MAX_PATH * 2]; fa_mkfile_s("cs_ex.txt", "abcdef"); pp(p, "cs_ex.txt");
    HANDLE h = fa_open(p, GENERIC_READ | GENERIC_WRITE, 0, OPEN_EXISTING, 0);
    DWORD hi = 0; SetLastError(0);
    DWORD lo = U(GetCompressedFileSizeA)(p, &hi);
    CHECKF(lo != INVALID_FILE_SIZE, "err %lu", GetLastError());
    CloseHandle(h); fa_rm(p);
}
TEST(fcsize_sparse_file_small_physical) {
    REQUIRE(U(GetCompressedFileSizeW));
    if (!real_ntfs()) { t_skip("needs real NTFS"); return; }
    char p[MAX_PATH * 2]; pp(p, "cs_sparse.bin"); WCHAR w[MAX_PATH * 2]; fa_widen(w, p);
    HANDLE h = fa_open(p, GENERIC_READ | GENERIC_WRITE, FA_SHARE_ALL, CREATE_ALWAYS, 0);
    DWORD br;
    if (!DeviceIoControl(h, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &br, NULL)) { t_skip("sparse unsupported"); CloseHandle(h); fa_rm(p); return; }
    LARGE_INTEGER li; li.QuadPart = 5LL * 1024 * 1024 * 1024 + 4096;
    BOOL ok = SetFilePointerEx(h, li, NULL, FILE_BEGIN) && SetEndOfFile(h);
    CloseHandle(h);
    if (!ok) { t_skip("cannot extend"); fa_rm(p); return; }
    DWORD hi = 0xff, lo = U(GetCompressedFileSizeW)(w, &hi);
    CHECKF(lo != INVALID_FILE_SIZE || GetLastError() == 0, "err %lu", GetLastError());
    EXPECT_EQ(hi, 0);
    CHECKF(lo < 1024 * 1024, "physical size %lu for sparse 5GB file", lo);
    WIN32_FILE_ATTRIBUTE_DATA d; GetFileAttributesExW(w, GetFileExInfoStandard, &d);
    EXPECT_EQ(d.nFileSizeHigh, 1);
    fa_rm(p);
}
TEST(fcsize_ntfs_compressed_file) {
    REQUIRE(U(GetCompressedFileSizeA));
    if (!real_ntfs()) { t_skip("needs real NTFS"); return; }
    char p[MAX_PATH * 2]; pp(p, "cs_comp.bin");
    HANDLE h = fa_open(p, GENERIC_READ | GENERIC_WRITE, FA_SHARE_ALL, CREATE_ALWAYS, 0);
    USHORT fmt = COMPRESSION_FORMAT_DEFAULT; DWORD br;
    if (!DeviceIoControl(h, FSCTL_SET_COMPRESSION, &fmt, sizeof fmt, NULL, 0, &br, NULL)) { t_skip("compression unsupported on this volume (err %lu)", GetLastError()); CloseHandle(h); fa_rm(p); return; }
    BYTE *z = calloc(1, 1 << 20); DWORD w; WriteFile(h, z, 1 << 20, &w, NULL); free(z);
    FlushFileBuffers(h); CloseHandle(h);
    DWORD hi = 0, lo = U(GetCompressedFileSizeA)(p, &hi);
    CHECKF(hi == 0 && lo < (1 << 20), "compressed size %lu of 1MB zeros", lo);
    DWORD a = GetFileAttributesA(p);
    CHECK(a & FILE_ATTRIBUTE_COMPRESSED);
    WIN32_FILE_ATTRIBUTE_DATA d; GetFileAttributesExA(p, GetFileExInfoStandard, &d);
    EXPECT_EQ(d.nFileSizeLow, 1 << 20);
    fa_rm(p);
}
TEST(fcsize_directory) {
    REQUIRE(U(GetCompressedFileSizeA));
    fa_mkdir("cs_dir"); char p[MAX_PATH * 2]; pp(p, "cs_dir");
    DWORD hi = 0; SetLastError(0);
    DWORD lo = U(GetCompressedFileSizeA)(p, &hi);
    t_note("GetCompressedFileSize(directory) = %lu err=%lu", lo, lo == INVALID_FILE_SIZE ? GetLastError() : 0);
    fa_rm(p);
}
TEST(fcsize_alternate_stream) {
    REQUIRE(U(GetCompressedFileSizeA));
    FA_REQUIRE_STREAMS();
    char p[MAX_PATH * 2], q[MAX_PATH * 2]; fa_mkfile_s("cs_ads.txt", "main-data-is-longer"); pp(p, "cs_ads.txt");
    snprintf(q, sizeof q, "%s:s", p); fa_mkfile(q, "12", 2);
    DWORD hi = 0, lo = U(GetCompressedFileSizeA)(q, &hi);
    t_note("compressed size of stream = %lu (stream 2 bytes, main 19)", lo);
    CHECK(lo != INVALID_FILE_SIZE);
    fa_rm(p);
}

/* ---------------- SetFileShortName ---------------- */
TEST(fshort_null_name) {
    REQUIRE(U(SetFileShortNameW));
    HANDLE h = (fa_mkfile_s("sn_n.txt", "x"), fa_openl("sn_n.txt", GENERIC_READ | DELETE, FA_SHARE_ALL, OPEN_EXISTING, 0));
    SetLastError(0); EXPECT_EQ(U(SetFileShortNameW)(h, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CloseHandle(h); fa_rml("sn_n.txt");
}
TEST(fshort_invalid_handle) {
    REQUIRE(U(SetFileShortNameW)); REQUIRE(U(SetFileShortNameA));
    SetLastError(0); EXPECT_EQ(U(SetFileShortNameW)(INVALID_HANDLE_VALUE, L"ABC.TXT"), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SetLastError(0); EXPECT_EQ(U(SetFileShortNameA)(INVALID_HANDLE_VALUE, "ABC.TXT"), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SetLastError(0); EXPECT_EQ(U(SetFileShortNameW)(NULL, L"ABC.TXT"), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(fshort_wrong_handle_type) {
    REQUIRE(U(SetFileShortNameW));
    HANDLE e = CreateEventA(NULL, FALSE, FALSE, NULL);
    SetLastError(0); EXPECT_EQ(U(SetFileShortNameW)(e, L"ABC.TXT"), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(e);
}
TEST(fshort_requires_restore_privilege) {
    REQUIRE(U(SetFileShortNameW)); REQUIRE(U(SetFileShortNameA));
    FA_REQUIRE_NTFS();
    fa_mkfile_s("sn_priv_long_name.txt", "x");
    HANDLE h = fa_openl("sn_priv_long_name.txt", GENERIC_READ | GENERIC_WRITE | DELETE, FA_SHARE_ALL, OPEN_EXISTING, 0);
    SetLastError(0);
    BOOL r = U(SetFileShortNameW)(h, L"K32TSN.TXT");
    DWORD e = GetLastError();
    if (!r) {
        CHECKF(e == ERROR_PRIVILEGE_NOT_HELD || e == ERROR_ACCESS_DENIED || e == ERROR_INVALID_PARAMETER || e == ERROR_INVALID_FUNCTION || e == ERROR_NOT_SUPPORTED, "err %lu", e);
        t_note("SetFileShortNameW without SeRestorePrivilege err=%lu", e);
    } else t_note("SetFileShortNameW succeeded (privilege present or 8.3 names enabled)");
    SetLastError(0);
    r = U(SetFileShortNameA)(h, "K32TSN2.TXT");
    t_note("SetFileShortNameA r=%d err=%lu", r, r ? 0 : GetLastError());
    CloseHandle(h); fa_rml("sn_priv_long_name.txt");
}
TEST(fshort_invalid_short_name) {
    REQUIRE(U(SetFileShortNameW));
    FA_REQUIRE_NTFS();
    fa_mkfile_s("sn_bad_long_name.txt", "x");
    HANDLE h = fa_openl("sn_bad_long_name.txt", GENERIC_READ | DELETE, FA_SHARE_ALL, OPEN_EXISTING, 0);
    SetLastError(0);
    BOOL r = U(SetFileShortNameW)(h, L"this name is much too long for 8.3.txt");
    CHECK(!r);
    t_note("overlong short name err=%lu", GetLastError());
    SetLastError(0);
    r = U(SetFileShortNameW)(h, L"a*b.txt");
    CHECK(!r);
    CloseHandle(h); fa_rml("sn_bad_long_name.txt");
}

/* ---------------- Bandwidth reservation ---------------- */
TEST(fbw_get_invalid_handle) {
    REQUIRE(U(GetFileBandwidthReservation));
    DWORD period, bytes, xfer, outstanding; BOOL disc;
    SetLastError(0); EXPECT_EQ(U(GetFileBandwidthReservation)(INVALID_HANDLE_VALUE, &period, &bytes, &disc, &xfer, &outstanding), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SetLastError(0); EXPECT_EQ(U(GetFileBandwidthReservation)(NULL, &period, &bytes, &disc, &xfer, &outstanding), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(fbw_set_invalid_handle) {
    REQUIRE(U(SetFileBandwidthReservation));
    DWORD xfer, outstanding;
    SetLastError(0); EXPECT_EQ(U(SetFileBandwidthReservation)(INVALID_HANDLE_VALUE, 1000, 65536, FALSE, &xfer, &outstanding), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SetLastError(0); EXPECT_EQ(U(SetFileBandwidthReservation)(NULL, 1000, 65536, FALSE, &xfer, &outstanding), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(fbw_get_plain_file_unsupported) {
    REQUIRE(U(GetFileBandwidthReservation));
    fa_mkfile_s("bw_f.txt", "x");
    HANDLE h = fa_openl("bw_f.txt", GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0);
    DWORD period = 0x11, bytes = 0x22, xfer = 0x33, outstanding = 0x44; BOOL disc = 0x55;
    SetLastError(0);
    BOOL r = U(GetFileBandwidthReservation)(h, &period, &bytes, &disc, &xfer, &outstanding);
    DWORD e = GetLastError();
    CHECK(!r);
    t_note("GetFileBandwidthReservation on a disk file err=%lu", e);
    if (!r) { EXPECT_EQ(period, 0x11); EXPECT_EQ(xfer, 0x33); EXPECT_EQ(outstanding, 0x44); }
    CloseHandle(h); fa_rml("bw_f.txt");
}
TEST(fbw_set_plain_file_unsupported) {
    REQUIRE(U(SetFileBandwidthReservation));
    fa_mkfile_s("bw_s.txt", "x");
    HANDLE h = fa_openl("bw_s.txt", GENERIC_READ | GENERIC_WRITE, FA_SHARE_ALL, OPEN_EXISTING, 0);
    DWORD xfer = 0x33, outstanding = 0x44;
    SetLastError(0);
    BOOL r = U(SetFileBandwidthReservation)(h, 1000, 65536, FALSE, &xfer, &outstanding);
    t_note("SetFileBandwidthReservation on a disk file r=%d err=%lu", r, r ? 0 : GetLastError());
    CHECK(!r);
    CloseHandle(h); fa_rml("bw_s.txt");
}
TEST(fbw_wrong_handle_type) {
    REQUIRE(U(GetFileBandwidthReservation));
    HANDLE e = CreateEventA(NULL, FALSE, FALSE, NULL);
    DWORD period, bytes, xfer, outstanding; BOOL disc;
    SetLastError(0); EXPECT_EQ(U(GetFileBandwidthReservation)(e, &period, &bytes, &disc, &xfer, &outstanding), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(e);
}

/* ---------------- SetFileIoOverlappedRange ---------------- */
TEST(fioran_short_length) {
    REQUIRE(U(SetFileIoOverlappedRange));
    BYTE buf[64];
    static const ULONG lens[] = { 0, 1, 0x10, 0x1f };
    for (unsigned i = 0; i < sizeof lens / sizeof *lens; i++) {
        SetLastError(0); EXPECT_EQ(U(SetFileIoOverlappedRange)(INVALID_HANDLE_VALUE, buf, lens[i]), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
        SetLastError(0); EXPECT_EQ(U(SetFileIoOverlappedRange)(NULL, buf, lens[i]), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(fioran_invalid_handle_with_valid_length) {
    REQUIRE(U(SetFileIoOverlappedRange));
    BYTE buf[64];
    SetLastError(0); EXPECT_EQ(U(SetFileIoOverlappedRange)(INVALID_HANDLE_VALUE, buf, 0x20), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SetLastError(0); EXPECT_EQ(U(SetFileIoOverlappedRange)(NULL, buf, 0x1000), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(fioran_non_file_handle) {
    REQUIRE(U(SetFileIoOverlappedRange));
    HANDLE e = CreateEventA(NULL, FALSE, FALSE, NULL); BYTE buf[64];
    SetLastError(0); EXPECT_EQ(U(SetFileIoOverlappedRange)(e, buf, 0x20), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(e);
}

/* ---------------- OpenFileById ---------------- */
typedef struct { DWORD dwSize; DWORD Type; union { LARGE_INTEGER FileId; GUID ObjectId; FILE_ID_128 ExtendedFileId; } u; } fid_desc;
static int file_id64(HANDLE h, LONGLONG *id) {
    BY_HANDLE_FILE_INFORMATION bi;
    if (!GetFileInformationByHandle(h, &bi)) return 0;
    *id = ((LONGLONG)bi.nFileIndexHigh << 32) | bi.nFileIndexLow;
    return 1;
}
static HANDLE byid(HANDLE hint, LONGLONG id, DWORD access, DWORD share, DWORD flags) {
    FILE_ID_DESCRIPTOR d; memset(&d, 0, sizeof d); d.dwSize = sizeof d; d.Type = FileIdType; d.FileId.QuadPart = id;
    return U(OpenFileById)(hint, &d, access, share, NULL, flags);
}
TEST(fbyid_open_file_read_content) {
    REQUIRE(U(OpenFileById)); FA_REQUIRE_NTFS();
    fa_mkfile_s("id_f.txt", "opened by id");
    HANDLE f = fa_openl("id_f.txt", GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0), hint = fa_opendir("", GENERIC_READ);
    LONGLONG id; CHECK(file_id64(f, &id));
    HANDLE h = byid(hint, id, GENERIC_READ, FA_SHARE_ALL, 0);
    CHECKF(h != INVALID_HANDLE_VALUE, "err %lu", GetLastError());
    if (h != INVALID_HANDLE_VALUE) {
        char b[64] = { 0 }; DWORD n = 0; CHECK(ReadFile(h, b, sizeof b, &n, NULL)); EXPECT_EQ(n, 12);
        CHECK(!memcmp(b, "opened by id", 12));
        CloseHandle(h);
    }
    CloseHandle(f); CloseHandle(hint); fa_rml("id_f.txt");
}
TEST(fbyid_final_path_matches) {
    REQUIRE(U(OpenFileById)); REQUIRE(U(GetFinalPathNameByHandleW)); FA_REQUIRE_NTFS();
    fa_mkfile_s("id_fp.txt", "x");
    HANDLE f = fa_openl("id_fp.txt", GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0), hint = fa_opendir("", GENERIC_READ);
    LONGLONG id; file_id64(f, &id);
    HANDLE h = byid(hint, id, GENERIC_READ, FA_SHARE_ALL, 0);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) {
        WCHAR a[MAX_PATH * 3], b[MAX_PATH * 3];
        DWORD na = U(GetFinalPathNameByHandleW)(f, a, MAX_PATH * 3, 0), nb = U(GetFinalPathNameByHandleW)(h, b, MAX_PATH * 3, 0);
        CHECK(na > 0 && na == nb); CHECK(!wcsicmp(a, b));
        CloseHandle(h);
    }
    CloseHandle(f); CloseHandle(hint); fa_rml("id_fp.txt");
}
TEST(fbyid_open_directory) {
    REQUIRE(U(OpenFileById)); FA_REQUIRE_NTFS();
    fa_mkdir("id_dir");
    HANDLE d = fa_opendir("id_dir", GENERIC_READ), hint = fa_opendir("", GENERIC_READ);
    LONGLONG id; CHECK(file_id64(d, &id));
    SetLastError(0);
    HANDLE h = byid(hint, id, GENERIC_READ, FA_SHARE_ALL, FILE_FLAG_BACKUP_SEMANTICS);
    CHECKF(h != INVALID_HANDLE_VALUE, "err %lu", GetLastError());
    if (h != INVALID_HANDLE_VALUE) {
        BY_HANDLE_FILE_INFORMATION bi; CHECK(GetFileInformationByHandle(h, &bi));
        CHECK(bi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
        CloseHandle(h);
    }
    CloseHandle(d); CloseHandle(hint); fa_rml("id_dir");
}
TEST(fbyid_extended_file_id_type) {
    REQUIRE(U(OpenFileById)); REQUIRE(U(GetFileInformationByHandleEx)); FA_REQUIRE_NTFS();
    fa_mkfile_s("id_ext.txt", "extended");
    HANDLE f = fa_openl("id_ext.txt", GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0), hint = fa_opendir("", GENERIC_READ);
    FILE_ID_INFO fi; CHECK(U(GetFileInformationByHandleEx)(f, FileIdInfo, &fi, sizeof fi));
    FILE_ID_DESCRIPTOR d; memset(&d, 0, sizeof d); d.dwSize = sizeof d; d.Type = ExtendedFileIdType; d.ExtendedFileId = fi.FileId;
    SetLastError(0);
    HANDLE h = U(OpenFileById)(hint, &d, GENERIC_READ, FA_SHARE_ALL, NULL, 0);
    CHECKF(h != INVALID_HANDLE_VALUE, "err %lu", GetLastError());
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    CloseHandle(f); CloseHandle(hint); fa_rml("id_ext.txt");
}
TEST(fbyid_null_descriptor) {
    REQUIRE(U(OpenFileById));
    HANDLE hint = fa_opendir("", GENERIC_READ);
    SetLastError(0);
    HANDLE h = U(OpenFileById)(hint, NULL, GENERIC_READ, FA_SHARE_ALL, NULL, 0);
    EXPECT_EQ(h, INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CloseHandle(hint);
}
TEST(fbyid_descriptor_size_too_small) {
    REQUIRE(U(OpenFileById));
    HANDLE hint = fa_opendir("", GENERIC_READ);
    FILE_ID_DESCRIPTOR d; memset(&d, 0, sizeof d); d.Type = FileIdType; d.FileId.QuadPart = 5;
    static const DWORD sz[] = { 0, 1, 8, 0x10, 0x17 };
    for (unsigned i = 0; i < sizeof sz / sizeof *sz; i++) {
        d.dwSize = sz[i];
        SetLastError(0); HANDLE h = U(OpenFileById)(hint, &d, GENERIC_READ, FA_SHARE_ALL, NULL, 0);
        EXPECT_EQ(h, INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
    CloseHandle(hint);
}
TEST(fbyid_invalid_type) {
    REQUIRE(U(OpenFileById));
    HANDLE hint = fa_opendir("", GENERIC_READ);
    FILE_ID_DESCRIPTOR d; memset(&d, 0, sizeof d); d.dwSize = sizeof d; d.FileId.QuadPart = 5;
    static const DWORD ty[] = { 3, 4, 100, 0x80000000, 0xffffffff };
    for (unsigned i = 0; i < sizeof ty / sizeof *ty; i++) {
        d.Type = (FILE_ID_TYPE)ty[i];
        SetLastError(0); HANDLE h = U(OpenFileById)(hint, &d, GENERIC_READ, FA_SHARE_ALL, NULL, 0);
        EXPECT_EQ(h, INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
    CloseHandle(hint);
}
TEST(fbyid_invalid_volume_hint) {
    REQUIRE(U(OpenFileById)); FA_REQUIRE_NTFS();
    fa_mkfile_s("id_vh.txt", "x");
    HANDLE f = fa_openl("id_vh.txt", GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0);
    LONGLONG id; file_id64(f, &id);
    SetLastError(0); HANDLE h = byid(INVALID_HANDLE_VALUE, id, GENERIC_READ, FA_SHARE_ALL, 0);
    DWORD e = GetLastError();
    EXPECT_EQ(h, INVALID_HANDLE_VALUE);
    t_note("OpenFileById(INVALID_HANDLE_VALUE hint) err=%lu", e);
    SetLastError(0); h = byid(NULL, id, GENERIC_READ, FA_SHARE_ALL, 0);
    EXPECT_EQ(h, INVALID_HANDLE_VALUE);
    t_note("OpenFileById(NULL hint) err=%lu", GetLastError());
    HANDLE ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    SetLastError(0); h = byid(ev, id, GENERIC_READ, FA_SHARE_ALL, 0);
    EXPECT_EQ(h, INVALID_HANDLE_VALUE);
    t_note("OpenFileById(event hint) err=%lu", GetLastError());
    CloseHandle(ev); CloseHandle(f); fa_rml("id_vh.txt");
}
TEST(fbyid_nonexistent_id) {
    REQUIRE(U(OpenFileById)); FA_REQUIRE_NTFS();
    HANDLE hint = fa_opendir("", GENERIC_READ);
    SetLastError(0); HANDLE h = byid(hint, 0x7FFFFFFFFFFFLL, GENERIC_READ, FA_SHARE_ALL, 0);
    DWORD e = GetLastError();
    EXPECT_EQ(h, INVALID_HANDLE_VALUE);
    t_note("OpenFileById(bogus id) err=%lu", e);
    CHECKF(e == ERROR_FILE_NOT_FOUND || e == ERROR_INVALID_PARAMETER || e == ERROR_INVALID_FUNCTION || e == ERROR_PATH_NOT_FOUND, "err %lu", e);
    SetLastError(0); h = byid(hint, 0, GENERIC_READ, FA_SHARE_ALL, 0);
    EXPECT_EQ(h, INVALID_HANDLE_VALUE);
    t_note("OpenFileById(id 0) err=%lu", GetLastError());
    CloseHandle(hint);
}
TEST(fbyid_deleted_file_id) {
    REQUIRE(U(OpenFileById)); FA_REQUIRE_NTFS();
    fa_mkfile_s("id_del.txt", "x");
    HANDLE f = fa_openl("id_del.txt", GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0), hint = fa_opendir("", GENERIC_READ);
    LONGLONG id; file_id64(f, &id); CloseHandle(f);
    fa_rml("id_del.txt");
    SetLastError(0); HANDLE h = byid(hint, id, GENERIC_READ, FA_SHARE_ALL, 0);
    DWORD e = GetLastError();
    t_note("OpenFileById(deleted id) -> %s err=%lu", h == INVALID_HANDLE_VALUE ? "failed" : "opened (id reused)", e);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    else CHECKF(e == ERROR_FILE_NOT_FOUND || e == ERROR_INVALID_PARAMETER || e == ERROR_PATH_NOT_FOUND, "err %lu", e);
    CloseHandle(hint);
}
TEST(fbyid_access_and_share_enforced) {
    REQUIRE(U(OpenFileById)); FA_REQUIRE_NTFS();
    fa_mkfile_s("id_acc.txt", "abc");
    char p[MAX_PATH * 2]; pp(p, "id_acc.txt");
    HANDLE hint = fa_opendir("", GENERIC_READ), f = fa_open(p, GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0);
    LONGLONG id; file_id64(f, &id); CloseHandle(f);
    /* a holder that denies sharing makes a second by-id open fail */
    HANDLE holder = fa_open(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0);
    SetLastError(0); HANDLE h = byid(hint, id, GENERIC_WRITE, FA_SHARE_ALL, 0);
    EXPECT_EQ(h, INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_SHARING_VIOLATION);
    h = byid(hint, id, GENERIC_READ, FA_SHARE_ALL, 0);
    CHECK(h != INVALID_HANDLE_VALUE); if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    CloseHandle(holder);
    /* readonly attribute blocks write access */
    SetFileAttributesA(p, FILE_ATTRIBUTE_READONLY);
    SetLastError(0); h = byid(hint, id, GENERIC_WRITE, FA_SHARE_ALL, 0);
    EXPECT_EQ(h, INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL);
    CloseHandle(hint); fa_rm(p);
}
TEST(fbyid_handle_is_file_handle) {
    REQUIRE(U(OpenFileById)); FA_REQUIRE_NTFS();
    fa_mkfile_s("id_ft.txt", "0123456789");
    HANDLE f = fa_openl("id_ft.txt", GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0), hint = fa_opendir("", GENERIC_READ);
    LONGLONG id; file_id64(f, &id);
    HANDLE h = byid(hint, id, GENERIC_READ, FA_SHARE_ALL, 0);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) {
        EXPECT_EQ(GetFileType(h), FILE_TYPE_DISK);
        EXPECT_EQ(GetFileSize(h, NULL), 10);
        FILETIME a, b; CHECK(GetFileTime(h, &a, NULL, &b));
        CloseHandle(h);
    }
    CloseHandle(f); CloseHandle(hint); fa_rml("id_ft.txt");
}
TEST(fbyid_hardlink_open_by_id) {
    REQUIRE(U(OpenFileById)); FA_REQUIRE_NTFS();
    char e[MAX_PATH * 2], l[MAX_PATH * 2]; fa_mkfile_s("id_hl1.txt", "hl"); pp(e, "id_hl1.txt"); pp(l, "id_hl2.txt");
    if (!CreateHardLinkA(l, e, NULL)) { t_skip("no hard link"); fa_rm(e); return; }
    HANDLE f = fa_open(e, GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0), hint = fa_opendir("", GENERIC_READ);
    LONGLONG id; file_id64(f, &id);
    HANDLE h = byid(hint, id, GENERIC_READ, FA_SHARE_ALL, 0);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) { BY_HANDLE_FILE_INFORMATION bi; GetFileInformationByHandle(h, &bi); EXPECT_EQ(bi.nNumberOfLinks, 2); CloseHandle(h); }
    CloseHandle(f); CloseHandle(hint); fa_rm(e); fa_rm(l);
}
TEST(fbyid_object_id_type_unknown_guid) {
    REQUIRE(U(OpenFileById)); FA_REQUIRE_NTFS();
    HANDLE hint = fa_opendir("", GENERIC_READ);
    FILE_ID_DESCRIPTOR d; memset(&d, 0, sizeof d); d.dwSize = sizeof d; d.Type = ObjectIdType;
    d.ObjectId.Data1 = 0x6b32746b; d.ObjectId.Data2 = 0x1234; d.ObjectId.Data3 = 0x4321; memset(d.ObjectId.Data4, 0x5a, 8);
    SetLastError(0);
    HANDLE h = U(OpenFileById)(hint, &d, GENERIC_READ, FA_SHARE_ALL, NULL, 0);
    EXPECT_EQ(h, INVALID_HANDLE_VALUE);
    t_note("OpenFileById(unknown object id) err=%lu", GetLastError());
    CloseHandle(hint);
}
