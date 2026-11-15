/* MoveFileEx / MoveFileW / MoveFileWithProgressW against open, read-only and directory targets,
 * and renames of directories with open children. */
#include "xdr.h"

static void two(WCHAR *a, WCHAR *b, const char *la, const char *lb) {
    xp(a, la); xp(b, lb);
    xmk(a, "SRC"); xmk(b, "DST-OLD");
}

TEST(xdr_move_replace_target_open_share_delete) {
    REQUIRE(U(MoveFileExW)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; two(a, b, "xdr_m1a.txt", "xdr_m1b.txt");
    HANDLE t = xopen(b, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    /* MoveFileEx does not ask for a POSIX replace: an open target blocks it even with share-delete */
    XFAIL(U(MoveFileExW)(a, b, MOVEFILE_REPLACE_EXISTING), ERROR_ACCESS_DENIED);
    CHECK(xcontent(b, "DST-OLD")); CHECK(xcontent(a, "SRC"));
    XCLOSE(t);
    XOK(U(MoveFileExW)(a, b, MOVEFILE_REPLACE_EXISTING));
    CHECK(xcontent(b, "SRC"));
    xrm(a); xrm(b);
}
TEST(xdr_move_replace_target_open_no_share_delete) {
    REQUIRE(U(MoveFileExW)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; two(a, b, "xdr_m2a.txt", "xdr_m2b.txt");
    HANDLE t = xopen(b, GENERIC_READ, XSHARE_RW, OPEN_EXISTING, 0);
    XFAIL(U(MoveFileExW)(a, b, MOVEFILE_REPLACE_EXISTING), ERROR_ACCESS_DENIED);
    CHECK(xcontent(b, "DST-OLD")); CHECK(xcontent(a, "SRC"));
    XCLOSE(t);
    XOK(U(MoveFileExW)(a, b, MOVEFILE_REPLACE_EXISTING));
    CHECK(xcontent(b, "SRC"));
    xrm(a); xrm(b);
}
TEST(xdr_move_no_replace_target_open) {
    REQUIRE(U(MoveFileExW)); REQUIRE(U(MoveFileW)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; two(a, b, "xdr_m3a.txt", "xdr_m3b.txt");
    HANDLE t = xopen(b, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    XFAIL(U(MoveFileExW)(a, b, 0), ERROR_ALREADY_EXISTS);
    XFAIL(U(MoveFileW)(a, b), ERROR_ALREADY_EXISTS);
    XCLOSE(t); xrm(a); xrm(b);
}
TEST(xdr_move_replace_readonly_target) {
    REQUIRE(U(MoveFileExW)); REQUIRE(U(MoveFileWithProgressW)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; two(a, b, "xdr_m4a.txt", "xdr_m4b.txt");
    SetFileAttributesW(b, FILE_ATTRIBUTE_READONLY);
    XFAIL(U(MoveFileExW)(a, b, MOVEFILE_REPLACE_EXISTING), ERROR_ACCESS_DENIED);
    XFAIL(U(MoveFileWithProgressW)(a, b, NULL, NULL, MOVEFILE_REPLACE_EXISTING), ERROR_ACCESS_DENIED);
    CHECK(xcontent(b, "DST-OLD"));
    CHECK(GetFileAttributesW(b) & FILE_ATTRIBUTE_READONLY);
    xrm(a); xrm(b);
}
TEST(xdr_move_replace_readonly_target_open) {
    REQUIRE(U(MoveFileExW)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; two(a, b, "xdr_m5a.txt", "xdr_m5b.txt");
    SetFileAttributesW(b, FILE_ATTRIBUTE_READONLY);
    HANDLE t = xopen(b, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    XFAIL(U(MoveFileExW)(a, b, MOVEFILE_REPLACE_EXISTING), ERROR_ACCESS_DENIED);
    XCLOSE(t); xrm(a); xrm(b);
}
TEST(xdr_move_replace_over_directory) {
    REQUIRE(U(MoveFileExW)); REQUIRE(U(MoveFileWithProgressW)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; xp(a, "xdr_m6a.txt"); xp(b, "xdr_m6d"); xmk(a, "SRC"); xmkdir(b);
    XFAIL(U(MoveFileExW)(a, b, MOVEFILE_REPLACE_EXISTING), ERROR_ACCESS_DENIED);
    XFAIL(U(MoveFileWithProgressW)(a, b, NULL, NULL, MOVEFILE_REPLACE_EXISTING), ERROR_ACCESS_DENIED);
    XFAIL(U(MoveFileExW)(a, b, 0), ERROR_ALREADY_EXISTS);
    CHECK(GetFileAttributesW(b) & FILE_ATTRIBUTE_DIRECTORY);
    CHECK(xcontent(a, "SRC"));
    xrm(a); xrm(b);
}
TEST(xdr_move_dir_replace_over_empty_dir) {
    REQUIRE(U(MoveFileExW)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; xp(a, "xdr_m7a"); xp(b, "xdr_m7b"); xmkdir(a); xmkdir(b);
    XFAIL(U(MoveFileExW)(a, b, MOVEFILE_REPLACE_EXISTING), ERROR_ACCESS_DENIED);
    XFAIL(U(MoveFileExW)(a, b, 0), ERROR_ALREADY_EXISTS);
    CHECK(xexists(a)); CHECK(xexists(b));
    xrm(a); xrm(b);
}
TEST(xdr_move_progress_replace_open_target) {
    REQUIRE(U(MoveFileWithProgressW)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; two(a, b, "xdr_m8a.txt", "xdr_m8b.txt");
    HANDLE t = xopen(b, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    SetLastError(0);
    BOOL r = U(MoveFileWithProgressW)(a, b, NULL, NULL, MOVEFILE_REPLACE_EXISTING);
    DWORD e = GetLastError();
    BOOL r2;
    /* like MoveFileExW: no POSIX replace, so an open target blocks it even with share-delete */
    CHECKF(!r && e == ERROR_ACCESS_DENIED, "progress r=%d GLE=%lu", r, e);
    CHECK(xcontent(b, "DST-OLD"));
    XCLOSE(t);
    /* MoveFileExW must agree with MoveFileWithProgressW */
    two(a, b, "xdr_m8a.txt", "xdr_m8b.txt");
    t = xopen(b, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    SetLastError(0);
    r2 = U(MoveFileExW)(a, b, MOVEFILE_REPLACE_EXISTING);
    CHECKF((r != 0) == (r2 != 0) && (r || e == GetLastError()), "progress r=%d GLE=%lu, ex r=%d GLE=%lu", r, e, r2, GetLastError());
    XCLOSE(t); xrm(a); xrm(b);
}
TEST(xdr_move_replace_hardlinked_target) {
    REQUIRE(U(MoveFileExW)); REQUIRE(U(CreateHardLinkW)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2], l[MAX_PATH * 2]; two(a, b, "xdr_m9a.txt", "xdr_m9b.txt");
    xp(l, "xdr_m9l.txt");
    XOK(U(CreateHardLinkW)(l, b, NULL));
    EXPECT_EQ(xlinks_by_name(b), 2);
    XOK(U(MoveFileExW)(a, b, MOVEFILE_REPLACE_EXISTING));
    CHECK(xcontent(b, "SRC"));
    CHECK(xcontent(l, "DST-OLD"));
    EXPECT_EQ(xlinks_by_name(l), 1);
    EXPECT_EQ(xlinks_by_name(b), 1);
    xrm(b); xrm(l);
}
TEST(xdr_move_onto_posix_deleted_name) {
    REQUIRE(U(MoveFileW)); REQUIRE(U(DeleteFileW)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; two(a, b, "xdr_m10a.txt", "xdr_m10b.txt");
    HANDLE t = xopen(b, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    XOK(U(DeleteFileW)(b));
    XOK(U(MoveFileW)(a, b));
    CHECK(xcontent(b, "SRC"));
    CHECK(xhcontent(t, "DST-OLD"));
    XCLOSE(t); xrm(b);
}
TEST(xdr_move_source_posix_deleted_name) {
    REQUIRE(U(MoveFileW)); REQUIRE(U(DeleteFileW)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; xp(a, "xdr_m11a.txt"); xp(b, "xdr_m11b.txt"); xmk(a, "SRC");
    HANDLE t = xopen(a, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    XOK(U(DeleteFileW)(a));
    XFAIL(U(MoveFileW)(a, b), ERROR_FILE_NOT_FOUND);
    XFAIL(U(MoveFileExW)(a, b, MOVEFILE_REPLACE_EXISTING), ERROR_FILE_NOT_FOUND);
    CHECK(!xexists(b));
    XCLOSE(t);
}
TEST(xdr_move_onto_legacy_pending_name) {
    REQUIRE(U(MoveFileExW)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; two(a, b, "xdr_m12a.txt", "xdr_m12b.txt");
    HANDLE t = xopen(b, GENERIC_READ | DELETE, XSHARE_ALL, OPEN_EXISTING, 0);
    XOK(xdisp_legacy(t, TRUE));
    SetLastError(0);
    BOOL r0 = U(MoveFileExW)(a, b, 0);
    DWORD e0 = GetLastError();
    CHECKF(!r0 && e0 == ERROR_ALREADY_EXISTS, "r=%d GLE=%lu", r0, e0);
    SetLastError(0);
    BOOL r = U(MoveFileExW)(a, b, MOVEFILE_REPLACE_EXISTING);
    DWORD e = GetLastError();
    CHECKF(!r && e == ERROR_ACCESS_DENIED, "r=%d GLE=%lu", r, e);
    XCLOSE(t);
    EXPECT_EQ(xattr_err(b), ERROR_FILE_NOT_FOUND);
    XOK(U(MoveFileExW)(a, b, 0));
    CHECK(xcontent(b, "SRC"));
    xrm(a); xrm(b);
}
TEST(xdr_move_open_source_handle_follows) {
    REQUIRE(U(MoveFileExW)); REQUIRE(U(GetFinalPathNameByHandleW)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2], fp[MAX_PATH * 2]; two(a, b, "xdr_m13a.txt", "xdr_m13b.txt");
    HANDLE s = xopen(a, GENERIC_READ | GENERIC_WRITE, XSHARE_ALL, OPEN_EXISTING, 0);
    XOK(U(MoveFileExW)(a, b, MOVEFILE_REPLACE_EXISTING));
    DWORD n = U(GetFinalPathNameByHandleW)(s, fp, MAX_PATH * 2, FILE_NAME_NORMALIZED | VOLUME_NAME_NONE);
    CHECK(n > 0 && wcsstr(fp, L"\\xdr_m13b.txt") != NULL);
    DWORD w; CHECK(WriteFile(s, "s", 1, &w, NULL));
    XCLOSE(s);
    CHECK(xcontent(b, "sRC"));
    xrm(b);
}
/* ---- directories with open children ---- */
TEST(xdr_move_dir_with_open_child_file) {
    REQUIRE(U(MoveFileW)); REQUIRE(U(MoveFileExW)); XDR_NEED_NTFS();
    WCHAR d[MAX_PATH * 2], d2[MAX_PATH * 2], f[MAX_PATH * 2];
    xp(d, "xdr_mc1"); xp(d2, "xdr_mc1x"); xmkdir(d); xp(f, "xdr_mc1\\c.txt"); xmk(f, "c");
    HANDLE c = xopen(f, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    XFAIL(U(MoveFileW)(d, d2), ERROR_ACCESS_DENIED);
    XFAIL(U(MoveFileExW)(d, d2, MOVEFILE_REPLACE_EXISTING), ERROR_ACCESS_DENIED);
    CHECK(xexists(d)); CHECK(!xexists(d2));
    XCLOSE(c);
    XOK(U(MoveFileW)(d, d2));
    WCHAR f2[MAX_PATH * 2]; xp(f2, "xdr_mc1x\\c.txt");
    CHECK(xcontent(f2, "c"));
    xrm(f2); xrm(d2);
}
TEST(xdr_renex_dir_with_open_child) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR d[MAX_PATH * 2], d2[MAX_PATH * 2], f[MAX_PATH * 2];
    xp(d, "xdr_mc2"); xp(d2, "xdr_mc2x"); xmkdir(d); xp(f, "xdr_mc2\\c.txt"); xmk(f, "c");
    HANDLE c = xopen(f, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    HANDLE h = xopendir(d, GENERIC_READ | DELETE, XSHARE_ALL);
    XOKH(h);
    XFAIL(xren(h, XC_RENAMEEX, d2, 0, NULL), ERROR_ACCESS_DENIED);
    XFAIL(xren(h, XC_RENAMEEX, d2, XR_POSIX, NULL), ERROR_ACCESS_DENIED);
    XFAIL(xren(h, XC_RENAME, d2, FALSE, NULL), ERROR_ACCESS_DENIED);
    XCLOSE(c);
    XOK(xren(h, XC_RENAMEEX, d2, 0, NULL));
    XCLOSE(h);
    WCHAR f2[MAX_PATH * 2]; xp(f2, "xdr_mc2x\\c.txt");
    CHECK(xcontent(f2, "c"));
    xrm(f2); xrm(d2);
}
TEST(xdr_move_dir_with_open_grandchild) {
    REQUIRE(U(MoveFileW)); XDR_NEED_NTFS();
    WCHAR d[MAX_PATH * 2], s[MAX_PATH * 2], d2[MAX_PATH * 2], f[MAX_PATH * 2];
    xp(d, "xdr_mc3"); xp(s, "xdr_mc3\\s"); xp(d2, "xdr_mc3x"); xmkdir(d); xmkdir(s);
    xp(f, "xdr_mc3\\s\\g.txt"); xmk(f, "g");
    HANDLE c = xopen(f, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    XFAIL(U(MoveFileW)(d, d2), ERROR_ACCESS_DENIED);
    XCLOSE(c);
    XOK(U(MoveFileW)(d, d2));
    WCHAR f2[MAX_PATH * 2], s2[MAX_PATH * 2]; xp(f2, "xdr_mc3x\\s\\g.txt"); xp(s2, "xdr_mc3x\\s");
    xrm(f2); xrm(s2); xrm(d2);
}
TEST(xdr_move_dir_with_open_child_dir_handle) {
    REQUIRE(U(MoveFileW)); XDR_NEED_NTFS();
    WCHAR d[MAX_PATH * 2], s[MAX_PATH * 2], d2[MAX_PATH * 2];
    xp(d, "xdr_mc4"); xp(s, "xdr_mc4\\s"); xp(d2, "xdr_mc4x"); xmkdir(d); xmkdir(s);
    HANDLE c = xopendir(s, FILE_READ_ATTRIBUTES, XSHARE_ALL);
    XOKH(c);
    XFAIL(U(MoveFileW)(d, d2), ERROR_ACCESS_DENIED);
    XCLOSE(c);
    XOK(U(MoveFileW)(d, d2));
    WCHAR s2[MAX_PATH * 2]; xp(s2, "xdr_mc4x\\s");
    xrm(s2); xrm(d2);
}
TEST(xdr_move_dir_with_posix_deleted_open_child) {
    REQUIRE(U(MoveFileW)); REQUIRE(U(DeleteFileW)); XDR_NEED_NTFS();
    WCHAR d[MAX_PATH * 2], d2[MAX_PATH * 2], f[MAX_PATH * 2];
    xp(d, "xdr_mc5"); xp(d2, "xdr_mc5x"); xmkdir(d); xp(f, "xdr_mc5\\c.txt"); xmk(f, "c");
    HANDLE c = xopen(f, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    XOK(U(DeleteFileW)(f));
    /* the open child no longer lives in this directory */
    XOK(U(MoveFileW)(d, d2));
    CHECK(xhcontent(c, "c"));
    XCLOSE(c);
    xrm(d2);
}
TEST(xdr_move_file_while_parent_dir_open) {
    REQUIRE(U(MoveFileW)); REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR d[MAX_PATH * 2], f[MAX_PATH * 2], g[MAX_PATH * 2], h2[MAX_PATH * 2];
    xp(d, "xdr_mp1"); xmkdir(d); xp(f, "xdr_mp1\\a.txt"); xp(g, "xdr_mp1\\b.txt"); xp(h2, "xdr_mp1\\c.txt"); xmk(f, "a");
    /* parent open without FILE_SHARE_DELETE and with full access */
    HANDLE pd = xopendir(d, GENERIC_READ | GENERIC_WRITE, XSHARE_RW);
    XOKH(pd);
    XOK(U(MoveFileW)(f, g));
    HANDLE fh = xopen(g, GENERIC_READ | DELETE, XSHARE_ALL, OPEN_EXISTING, 0);
    XOK(xren(fh, XC_RENAMEEX, h2, XR_POSIX, NULL));
    XCLOSE(fh);
    CHECK(xcontent(h2, "a"));
    /* the parent itself cannot move while it is open without share-delete */
    WCHAR d2[MAX_PATH * 2]; xp(d2, "xdr_mp1x");
    XFAIL(U(MoveFileW)(d, d2), ERROR_SHARING_VIOLATION);
    XCLOSE(pd);
    xrm(h2); xrm(d);
}
TEST(xdr_move_file_out_of_open_dir) {
    REQUIRE(U(MoveFileW)); XDR_NEED_NTFS();
    WCHAR d[MAX_PATH * 2], f[MAX_PATH * 2], g[MAX_PATH * 2];
    xp(d, "xdr_mp2"); xmkdir(d); xp(f, "xdr_mp2\\a.txt"); xp(g, "xdr_mp2_out.txt"); xmk(f, "a");
    HANDLE pd = xopendir(d, GENERIC_READ, XSHARE_ALL);
    XOK(U(MoveFileW)(f, g));
    XOK(U(MoveFileW)(g, f));
    XCLOSE(pd);
    xrm(f); xrm(d);
}
TEST(xdr_move_dir_open_itself_share_delete) {
    REQUIRE(U(MoveFileW)); XDR_NEED_NTFS();
    WCHAR d[MAX_PATH * 2], d2[MAX_PATH * 2];
    xp(d, "xdr_mp3"); xp(d2, "xdr_mp3x"); xmkdir(d);
    HANDLE pd = xopendir(d, GENERIC_READ, XSHARE_ALL);
    XOK(U(MoveFileW)(d, d2));
    EXPECT_EQ(xattr_err(d), ERROR_FILE_NOT_FOUND);
    XCLOSE(pd);
    xrm(d2);
}
