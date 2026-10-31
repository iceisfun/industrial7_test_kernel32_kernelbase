/* SetFileAttributesA/W. */
#include "fa.h"

static BOOL saA(const char *p, DWORD a) { return U(SetFileAttributesA)(p, a); }
static BOOL saW(const char *p, DWORD a) { WCHAR w[MAX_PATH * 2]; fa_widen(w, p); return U(SetFileAttributesW)(w, a); }
static DWORD ga(const char *p) { return GetFileAttributesA(p); }
static void mk(char *p, const char *leaf) { fa_mkfile_s(leaf, "data"); fa_path(p, leaf); }
#define MASKFULL (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_TEMPORARY | FILE_ATTRIBUTE_OFFLINE | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED)
/* emulators on a non-NTFS backend do not persist the last three flags */
#define MASKSET (t_is_wine() ? (MASKFULL & 0x27) : MASKFULL)

TEST(fsattr_each_flag_roundtrip_a) {
    REQUIRE(U(SetFileAttributesA));
    static const DWORD f[] = { FILE_ATTRIBUTE_READONLY, FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_SYSTEM, FILE_ATTRIBUTE_ARCHIVE, FILE_ATTRIBUTE_TEMPORARY, FILE_ATTRIBUTE_OFFLINE, FILE_ATTRIBUTE_NOT_CONTENT_INDEXED };
    char p[MAX_PATH * 2]; mk(p, "sa_each.txt");
    for (unsigned i = 0; i < sizeof f / sizeof *f; i++) {
        if (t_is_wine() && (f[i] & 0x3100)) continue;
        CHECKF(saA(p, f[i]), "set 0x%lx err %lu", f[i], GetLastError());
        DWORD a = ga(p);
        CHECKF((a & f[i]) == f[i], "flag 0x%lx not set: attr 0x%lx", f[i], a);
        /* flags other than the one requested are cleared (except ARCHIVE reappears only if asked) */
        CHECKF((a & MASKSET & ~f[i] & ~FILE_ATTRIBUTE_ARCHIVE) == 0, "extra bits in 0x%lx for 0x%lx", a, f[i]);
        CHECK(saA(p, FILE_ATTRIBUTE_NORMAL));
    }
    fa_rm(p);
}
TEST(fsattr_each_flag_roundtrip_w) {
    REQUIRE(U(SetFileAttributesW));
    static const DWORD f[] = { FILE_ATTRIBUTE_READONLY, FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_SYSTEM, FILE_ATTRIBUTE_ARCHIVE, FILE_ATTRIBUTE_TEMPORARY, FILE_ATTRIBUTE_OFFLINE, FILE_ATTRIBUTE_NOT_CONTENT_INDEXED };
    char p[MAX_PATH * 2]; mk(p, "sa_eachw.txt");
    for (unsigned i = 0; i < sizeof f / sizeof *f; i++) {
        if (t_is_wine() && (f[i] & 0x3100)) continue;
        CHECKF(saW(p, f[i]), "set 0x%lx err %lu", f[i], GetLastError());
        DWORD a = ga(p);
        CHECKF((a & f[i]) == f[i], "flag 0x%lx not set: attr 0x%lx", f[i], a);
        CHECK(saW(p, FILE_ATTRIBUTE_NORMAL));
    }
    fa_rm(p);
}
TEST(fsattr_combination_all_settable) {
    REQUIRE(U(SetFileAttributesW));
    char p[MAX_PATH * 2]; mk(p, "sa_all.txt");
    CHECK(saW(p, MASKSET));
    EXPECT_EQ(ga(p) & MASKSET & ~FILE_ATTRIBUTE_ARCHIVE, MASKSET & ~FILE_ATTRIBUTE_ARCHIVE);
    CHECK(saW(p, FILE_ATTRIBUTE_NORMAL));
    EXPECT_EQ(ga(p) & MASKSET & ~FILE_ATTRIBUTE_ARCHIVE, 0);
    fa_rm(p);
}
TEST(fsattr_normal_clears_everything) {
    REQUIRE(U(SetFileAttributesA));
    char p[MAX_PATH * 2]; mk(p, "sa_norm.txt");
    CHECK(saA(p, FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM));
    CHECK(saA(p, FILE_ATTRIBUTE_NORMAL));
    DWORD a = ga(p);
    EXPECT_EQ(a & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM), 0);
    CHECK(!(a & FILE_ATTRIBUTE_DIRECTORY));
    t_note("attr after NORMAL = 0x%lx", a);
    fa_rm(p);
}
TEST(fsattr_zero_behaves_like_normal) {
    REQUIRE(U(SetFileAttributesA));
    char p[MAX_PATH * 2]; mk(p, "sa_zero.txt");
    CHECK(saA(p, FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN));
    CHECK(saA(p, 0));
    EXPECT_EQ(ga(p) & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN), 0);
    fa_rm(p);
}
TEST(fsattr_archive_cleared_when_not_requested) {
    REQUIRE(U(SetFileAttributesA));
    char p[MAX_PATH * 2]; mk(p, "sa_arch.txt");
    CHECK(ga(p) & FILE_ATTRIBUTE_ARCHIVE);
    CHECK(saA(p, FILE_ATTRIBUTE_HIDDEN));
    DWORD a = ga(p);
    CHECK(a & FILE_ATTRIBUTE_HIDDEN);
    t_note("ARCHIVE after SetFileAttributes(HIDDEN) = %d", !!(a & FILE_ATTRIBUTE_ARCHIVE));
    CHECK(!(a & FILE_ATTRIBUTE_ARCHIVE));
    fa_rm(p);
}
TEST(fsattr_invalid_bits_ignored) {
    REQUIRE(U(SetFileAttributesW));
    char p[MAX_PATH * 2]; mk(p, "sa_inv.txt");
    DWORD before = ga(p);
    static const DWORD junk[] = { FILE_ATTRIBUTE_DIRECTORY, FILE_ATTRIBUTE_DEVICE, FILE_ATTRIBUTE_REPARSE_POINT, FILE_ATTRIBUTE_SPARSE_FILE,
                                  FILE_ATTRIBUTE_COMPRESSED, FILE_ATTRIBUTE_ENCRYPTED, FILE_ATTRIBUTE_INTEGRITY_STREAM, 0x00010000, 0x40000000, 0x80000000 };
    for (unsigned i = 0; i < sizeof junk / sizeof *junk; i++) {
        SetLastError(0);
        BOOL ok = saW(p, junk[i] | FILE_ATTRIBUTE_HIDDEN);
        CHECKF(ok, "0x%lx err %lu", junk[i], GetLastError());
        DWORD a = ga(p);
        CHECKF((a & FILE_ATTRIBUTE_HIDDEN) && !(a & FILE_ATTRIBUTE_DIRECTORY), "junk 0x%lx -> attr 0x%lx", junk[i], a);
        CHECKF(!(a & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_SPARSE_FILE | FILE_ATTRIBUTE_ENCRYPTED)), "junk 0x%lx -> attr 0x%lx", junk[i], a);
    }
    (void)before;
    CHECK(saW(p, FILE_ATTRIBUTE_NORMAL));
    fa_rm(p);
}
TEST(fsattr_directory_bit_does_not_convert_file) {
    REQUIRE(U(SetFileAttributesA));
    char p[MAX_PATH * 2]; mk(p, "sa_dirbit.txt");
    CHECK(saA(p, FILE_ATTRIBUTE_DIRECTORY));
    CHECK(!(ga(p) & FILE_ATTRIBUTE_DIRECTORY));
    HANDLE h = fa_open(p, GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    fa_rm(p);
}
TEST(fsattr_compressed_bit_ignored) {
    REQUIRE(U(SetFileAttributesA));
    char p[MAX_PATH * 2]; mk(p, "sa_comp.txt");
    CHECK(saA(p, FILE_ATTRIBUTE_COMPRESSED));
    CHECK(!(ga(p) & FILE_ATTRIBUTE_COMPRESSED));
    CHECK(saA(p, FILE_ATTRIBUTE_ENCRYPTED));
    CHECK(!(ga(p) & FILE_ATTRIBUTE_ENCRYPTED));
    fa_rm(p);
}
TEST(fsattr_readonly_blocks_write_open) {
    REQUIRE(U(SetFileAttributesA));
    char p[MAX_PATH * 2]; mk(p, "sa_ro.txt");
    CHECK(saA(p, FILE_ATTRIBUTE_READONLY));
    SetLastError(0);
    HANDLE h = fa_open(p, GENERIC_WRITE, FA_SHARE_ALL, OPEN_EXISTING, 0);
    EXPECT_EQ(h, INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    h = fa_open(p, GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    SetLastError(0);
    h = fa_open(p, GENERIC_WRITE, FA_SHARE_ALL, CREATE_ALWAYS, 0);
    EXPECT_EQ(h, INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    SetLastError(0);
    h = fa_open(p, FILE_APPEND_DATA, FA_SHARE_ALL, OPEN_EXISTING, 0);
    EXPECT_EQ(h, INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CHECK(saA(p, FILE_ATTRIBUTE_NORMAL));
    h = fa_open(p, GENERIC_WRITE, FA_SHARE_ALL, OPEN_EXISTING, 0);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    fa_rm(p);
}
TEST(fsattr_readonly_blocks_delete) {
    REQUIRE(U(SetFileAttributesA));
    char p[MAX_PATH * 2]; mk(p, "sa_rodel.txt");
    CHECK(saA(p, FILE_ATTRIBUTE_READONLY));
    SetLastError(0); EXPECT_EQ(DeleteFileA(p), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CHECK(fa_exists(p));
    fa_rm(p);
}
TEST(fsattr_readonly_attr_change_allowed_while_open) {
    REQUIRE(U(SetFileAttributesA));
    char p[MAX_PATH * 2]; mk(p, "sa_open.txt");
    HANDLE h = fa_open(p, GENERIC_READ | GENERIC_WRITE, 0, OPEN_EXISTING, 0);   /* exclusive */
    CHECK(h != INVALID_HANDLE_VALUE);
    CHECKF(saA(p, FILE_ATTRIBUTE_HIDDEN), "err %lu", GetLastError());
    CHECK(ga(p) & FILE_ATTRIBUTE_HIDDEN);
    CHECK(saA(p, FILE_ATTRIBUTE_NORMAL));
    CloseHandle(h); fa_rm(p);
}
TEST(fsattr_write_via_existing_handle_after_readonly) {
    REQUIRE(U(SetFileAttributesA));
    char p[MAX_PATH * 2]; mk(p, "sa_rowrite.txt");
    HANDLE h = fa_open(p, GENERIC_READ | GENERIC_WRITE, FA_SHARE_ALL, OPEN_EXISTING, 0);
    CHECK(saA(p, FILE_ATTRIBUTE_READONLY));
    DWORD w = 0;
    CHECK(WriteFile(h, "Z", 1, &w, NULL));
    CloseHandle(h);
    CHECK(saA(p, FILE_ATTRIBUTE_NORMAL));
    fa_rm(p);
}
TEST(fsattr_missing_file_errors) {
    REQUIRE(U(SetFileAttributesA)); REQUIRE(U(SetFileAttributesW));
    char p[MAX_PATH * 2]; fa_path(p, "sa_nofile.txt");
    SetLastError(0); EXPECT_EQ(saA(p, 1), FALSE); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    SetLastError(0); EXPECT_EQ(saW(p, 1), FALSE); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    fa_path(p, "sa_nodir\\x.txt");
    SetLastError(0); EXPECT_EQ(saA(p, 1), FALSE); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    SetLastError(0); EXPECT_EQ(saW(p, 1), FALSE); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(fsattr_wildcard_errors) {
    REQUIRE(U(SetFileAttributesA)); REQUIRE(U(SetFileAttributesW));
    static const char *bad[] = { "sa_*", "sa_?", "sa_<", "sa_>", "sa_\"" };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        char p[MAX_PATH * 2]; fa_path(p, bad[i]);
        SetLastError(0); EXPECT_EQ(saA(p, FILE_ATTRIBUTE_HIDDEN), FALSE); EXPECT_ERR(ERROR_INVALID_NAME);
        SetLastError(0); EXPECT_EQ(saW(p, FILE_ATTRIBUTE_HIDDEN), FALSE); EXPECT_ERR(ERROR_INVALID_NAME);
    }
}
TEST(fsattr_wildcard_does_not_touch_existing_file) {
    REQUIRE(U(SetFileAttributesA));
    char p[MAX_PATH * 2], q[MAX_PATH * 2]; mk(p, "sa_wcx.txt"); fa_path(q, "sa_wcx.*");
    SetLastError(0); EXPECT_EQ(saA(q, FILE_ATTRIBUTE_HIDDEN), FALSE);
    CHECK(!(ga(p) & FILE_ATTRIBUTE_HIDDEN));
    fa_rm(p);
}
TEST(fsattr_empty_and_null_name) {
    REQUIRE(U(SetFileAttributesA)); REQUIRE(U(SetFileAttributesW));
    SetLastError(0); EXPECT_EQ(U(SetFileAttributesA)("", 1), FALSE);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_PATH_NOT_FOUND || e == ERROR_FILE_NOT_FOUND || e == ERROR_INVALID_NAME, "err %lu", e);
    SetLastError(0); EXPECT_EQ(U(SetFileAttributesW)(L"", 1), FALSE);
    EXPECT_EQ(GetLastError(), e);
    SetLastError(0); EXPECT_EQ(U(SetFileAttributesW)(NULL, 1), FALSE);
    t_note("SetFileAttributesW(NULL) err=%lu", GetLastError());
}
TEST(fsattr_directory_hidden) {
    REQUIRE(U(SetFileAttributesA));
    char p[MAX_PATH * 2]; fa_mkdir("sa_dir"); fa_path(p, "sa_dir");
    CHECK(saA(p, FILE_ATTRIBUTE_HIDDEN));
    DWORD a = ga(p);
    CHECK(a & FILE_ATTRIBUTE_HIDDEN);
    CHECK(a & FILE_ATTRIBUTE_DIRECTORY);
    CHECK(saA(p, FILE_ATTRIBUTE_NORMAL));
    a = ga(p);
    CHECK(a & FILE_ATTRIBUTE_DIRECTORY);
    CHECK(!(a & FILE_ATTRIBUTE_HIDDEN));
    fa_rm(p);
}
TEST(fsattr_directory_flags_survive_zero) {
    REQUIRE(U(SetFileAttributesW));
    char p[MAX_PATH * 2]; fa_mkdir("sa_dir0"); fa_path(p, "sa_dir0");
    CHECK(saW(p, 0));
    CHECK(ga(p) & FILE_ATTRIBUTE_DIRECTORY);
    CHECK(saW(p, FILE_ATTRIBUTE_DIRECTORY));
    CHECK(ga(p) & FILE_ATTRIBUTE_DIRECTORY);
    fa_rm(p);
}
TEST(fsattr_readonly_directory_semantics) {
    REQUIRE(U(SetFileAttributesA));
    char p[MAX_PATH * 2], f[MAX_PATH * 2]; fa_mkdir("sa_rodir"); fa_path(p, "sa_rodir");
    CHECK(saA(p, FILE_ATTRIBUTE_READONLY));
    CHECK(ga(p) & FILE_ATTRIBUTE_READONLY);
    /* readonly on a directory does not prevent creating entries in it */
    fa_path(f, "sa_rodir\\child.txt");
    CHECK(fa_mkfile(f, "x", 1));
    fa_rm(f);
    /* ... but prevents removing it */
    SetLastError(0); EXPECT_EQ(RemoveDirectoryA(p), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CHECK(saA(p, FILE_ATTRIBUTE_NORMAL));
    CHECK(RemoveDirectoryA(p));
}
TEST(fsattr_does_not_change_write_time) {
    REQUIRE(U(SetFileAttributesA));
    char p[MAX_PATH * 2]; mk(p, "sa_wt.txt");
    WIN32_FILE_ATTRIBUTE_DATA a, b;
    GetFileAttributesExA(p, GetFileExInfoStandard, &a);
    Sleep(50);
    CHECK(saA(p, FILE_ATTRIBUTE_HIDDEN));
    GetFileAttributesExA(p, GetFileExInfoStandard, &b);
    EXPECT_EQ(fa_ft64(a.ftLastWriteTime), fa_ft64(b.ftLastWriteTime));
    EXPECT_EQ(fa_ft64(a.ftCreationTime), fa_ft64(b.ftCreationTime));
    EXPECT_EQ(a.nFileSizeLow, b.nFileSizeLow);
    fa_rm(p);
}
TEST(fsattr_idempotent_repeat) {
    REQUIRE(U(SetFileAttributesA));
    char p[MAX_PATH * 2]; mk(p, "sa_rep.txt");
    for (int i = 0; i < 100; i++) { if (!saA(p, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_TEMPORARY)) { CHECK(0); break; } }
    EXPECT_EQ(ga(p) & MASKSET & ~FILE_ATTRIBUTE_ARCHIVE, MASKSET & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_TEMPORARY));
    fa_rm(p);
}
TEST(fsattr_a_w_equivalent) {
    REQUIRE(U(SetFileAttributesA)); REQUIRE(U(SetFileAttributesW));
    char p[MAX_PATH * 2]; mk(p, "sa_aw.txt");
    CHECK(saA(p, FILE_ATTRIBUTE_SYSTEM)); DWORD a = ga(p) & MASKSET;
    CHECK(saA(p, FILE_ATTRIBUTE_NORMAL));
    CHECK(saW(p, FILE_ATTRIBUTE_SYSTEM)); DWORD w = ga(p) & MASKSET;
    EXPECT_EQ(a, w);
    fa_rm(p);
}
TEST(fsattr_prefixed_and_dotted_path) {
    REQUIRE(U(SetFileAttributesW));
    char p[MAX_PATH * 2], q[MAX_PATH * 2]; mk(p, "sa_pfx.txt");
    snprintf(q, sizeof q, "\\\\?\\%s", p);
    CHECK(saW(q, FILE_ATTRIBUTE_HIDDEN)); CHECK(ga(p) & FILE_ATTRIBUTE_HIDDEN);
    CHECK(saW(p, FILE_ATTRIBUTE_NORMAL));
    snprintf(q, sizeof q, "%s...", p);
    CHECK(saW(q, FILE_ATTRIBUTE_SYSTEM)); CHECK(ga(p) & FILE_ATTRIBUTE_SYSTEM);
    CHECK(saW(p, FILE_ATTRIBUTE_NORMAL));
    fa_rm(p);
}
TEST(fsattr_hidden_file_invisible_in_nothing_but_findfirst_visible) {
    REQUIRE(U(SetFileAttributesA));
    char p[MAX_PATH * 2]; mk(p, "sa_find.txt");
    CHECK(saA(p, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM));
    WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA(p, &fd);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) { CHECK((fd.dwFileAttributes & 6) == 6); FindClose(h); }
    CHECK(saA(p, FILE_ATTRIBUTE_NORMAL));
    fa_rm(p);
}
TEST(fsattr_reserved_device_name) {
    REQUIRE(U(SetFileAttributesA));
    /* attribute change on a DOS device name must not alter anything on disk; only the error path is of interest */
    SetLastError(0);
    BOOL r = U(SetFileAttributesA)("NUL", FILE_ATTRIBUTE_NORMAL);
    t_note("SetFileAttributes(NUL) = %d err=%lu", r, r ? 0 : GetLastError());
}
TEST(fsattr_stream_path) {
    REQUIRE(U(SetFileAttributesA));
    FA_REQUIRE_STREAMS();
    char p[MAX_PATH * 2], q[MAX_PATH * 2]; mk(p, "sa_ads.txt");
    snprintf(q, sizeof q, "%s:s", p); fa_mkfile(q, "1", 1);
    CHECK(saA(q, FILE_ATTRIBUTE_HIDDEN));
    t_note("main file attr after stream SetFileAttributes = 0x%lx", ga(p));
    CHECK(saA(p, FILE_ATTRIBUTE_NORMAL));
    fa_rm(p);
}
TEST(fsattr_get_sees_temporary_offline_flags) {
    REQUIRE(U(SetFileAttributesA));
    FA_REQUIRE_NTFS();
    char p[MAX_PATH * 2]; mk(p, "sa_to.txt");
    if (t_is_wine()) { t_skip("emulated fs does not store these flags"); return; }
    CHECK(saA(p, FILE_ATTRIBUTE_TEMPORARY | FILE_ATTRIBUTE_OFFLINE | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED));
    DWORD a = ga(p);
    CHECK(a & FILE_ATTRIBUTE_TEMPORARY); CHECK(a & FILE_ATTRIBUTE_OFFLINE); CHECK(a & FILE_ATTRIBUTE_NOT_CONTENT_INDEXED);
    BY_HANDLE_FILE_INFORMATION bi; HANDLE h = fa_openfile("sa_to.txt");
    CHECK(GetFileInformationByHandle(h, &bi)); EXPECT_EQ(bi.dwFileAttributes, a);
    CloseHandle(h);
    CHECK(saA(p, FILE_ATTRIBUTE_NORMAL)); fa_rm(p);
}
TEST(fsattr_new_file_attributes_via_createfile) {
    REQUIRE(U(SetFileAttributesA));
    char p[MAX_PATH * 2]; fa_path(p, "sa_cf.txt");
    HANDLE h = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY, NULL);
    CHECK(h != INVALID_HANDLE_VALUE); CloseHandle(h);
    DWORD a = ga(p);
    CHECK((a & 3) == 3);
    CHECK(saA(p, FILE_ATTRIBUTE_NORMAL));
    fa_rm(p);
}
TEST(fsattr_create_always_on_hidden_requires_matching_attrs) {
    REQUIRE(U(SetFileAttributesA));
    char p[MAX_PATH * 2]; mk(p, "sa_hid_ca.txt");
    CHECK(saA(p, FILE_ATTRIBUTE_HIDDEN));
    SetLastError(0);
    HANDLE h = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    EXPECT_EQ(h, INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    h = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    CHECK(saA(p, FILE_ATTRIBUTE_NORMAL)); fa_rm(p);
}
