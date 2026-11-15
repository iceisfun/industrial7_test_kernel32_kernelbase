/* DeleteFileW on NTFS: the name goes away at once (POSIX delete) even while other handles stay open. */
#include "xdr.h"

/* creates dir\leaf with data and an independent share-all read/write handle on it */
static HANDLE mk_open(WCHAR *path, const char *leaf, const char *data) {
    xp(path, leaf);
    xmk(path, data);
    return xopen(path, GENERIC_READ | GENERIC_WRITE, XSHARE_ALL, OPEN_EXISTING, 0);
}

TEST(xdr_delete_sharedel_name_gone_at_once) {
    REQUIRE(U(DeleteFileW)); XDR_NEED_NTFS();
    WCHAR p[MAX_PATH * 2]; HANDLE h = mk_open(p, "xdr_dg1.txt", "abc");
    XOKH(h);
    XOK(U(DeleteFileW)(p));
    EXPECT_EQ(xattr_err(p), ERROR_FILE_NOT_FOUND);
    XCLOSE(h);
    EXPECT_EQ(xattr_err(p), ERROR_FILE_NOT_FOUND);
}
TEST(xdr_delete_sharedel_findfirst_not_listed) {
    REQUIRE(U(DeleteFileW)); REQUIRE(U(FindFirstFileW)); XDR_NEED_NTFS();
    WCHAR d[MAX_PATH * 2], p[MAX_PATH * 2]; WIN32_FIND_DATAW fd;
    xp(d, "xdr_dg2"); xmkdir(d);
    xp(p, "xdr_dg2\\f.txt"); xmk(p, "abc");
    HANDLE h = xopen(p, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    XOKH(h);
    EXPECT_EQ(xlisted(d, L"f.txt"), 1);
    XOK(U(DeleteFileW)(p));
    EXPECT_EQ(xlisted(d, L"f.txt"), 0);
    SetLastError(0);
    HANDLE f = U(FindFirstFileW)(p, &fd);
    CHECKF(f == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_NOT_FOUND, "FindFirstFileW exact name: %p GLE %lu", f, GetLastError());
    if (f != INVALID_HANDLE_VALUE) FindClose(f);
    XCLOSE(h);
    xrm(d);
}
TEST(xdr_delete_sharedel_create_new_same_name) {
    REQUIRE(U(DeleteFileW)); XDR_NEED_NTFS();
    WCHAR p[MAX_PATH * 2]; HANDLE h = mk_open(p, "xdr_dg3.txt", "old-data");
    XOK(U(DeleteFileW)(p));
    HANDLE n = xopen(p, GENERIC_READ | GENERIC_WRITE, XSHARE_ALL, CREATE_NEW, 0);
    XOKH(n);
    if (n != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER s; s.QuadPart = -1; GetFileSizeEx(n, &s);
        EXPECT_EQ(s.QuadPart, 0);
        DWORD w; WriteFile(n, "new", 3, &w, NULL);
    }
    /* the two handles refer to different files */
    CHECK(xhcontent(h, "old-data"));
    XCLOSE(h);
    XCLOSE(n);
    CHECK(xcontent(p, "new"));
    xrm(p);
}
TEST(xdr_delete_sharedel_open_existing_not_found) {
    REQUIRE(U(DeleteFileW)); XDR_NEED_NTFS();
    WCHAR p[MAX_PATH * 2]; HANDLE h = mk_open(p, "xdr_dg4.txt", "abc");
    XOK(U(DeleteFileW)(p));
    SetLastError(0);
    HANDLE o = xopen(p, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    CHECKF(o == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_NOT_FOUND, "OPEN_EXISTING after delete: %p GLE %lu", o, GetLastError());
    XCLOSE(o);
    XFAIL(U(DeleteFileW)(p), ERROR_FILE_NOT_FOUND);
    XCLOSE(h);
}
TEST(xdr_delete_survivor_standard_info) {
    REQUIRE(U(DeleteFileW)); REQUIRE(U(GetFileInformationByHandleEx)); XDR_NEED_NTFS();
    WCHAR p[MAX_PATH * 2]; HANDLE h = mk_open(p, "xdr_dg5.txt", "abcdef");
    FILE_STANDARD_INFO st;
    CHECK(xstd(h, &st));
    EXPECT_EQ(st.DeletePending, 0); EXPECT_EQ(st.NumberOfLinks, 1);
    XOK(U(DeleteFileW)(p));
    CHECK(xstd(h, &st));
    EXPECT_EQ(st.DeletePending, 1);
    EXPECT_EQ(st.NumberOfLinks, 0);
    EXPECT_EQ(st.EndOfFile.QuadPart, 6);
    EXPECT_EQ(st.Directory, 0);
    XCLOSE(h);
}
TEST(xdr_delete_survivor_read_write_extend) {
    REQUIRE(U(DeleteFileW)); XDR_NEED_NTFS();
    WCHAR p[MAX_PATH * 2]; HANDLE h = mk_open(p, "xdr_dg6.txt", "0123456789");
    XOK(U(DeleteFileW)(p));
    CHECK(xhcontent(h, "0123456789"));
    LARGE_INTEGER z; z.QuadPart = 0;
    CHECK(SetFilePointerEx(h, z, NULL, FILE_END));
    DWORD w = 0; CHECK(WriteFile(h, "ABCDEFGHIJ", 10, &w, NULL)); EXPECT_EQ(w, 10);
    LARGE_INTEGER s; s.QuadPart = -1; CHECK(GetFileSizeEx(h, &s)); EXPECT_EQ(s.QuadPart, 20);
    CHECK(xhcontent(h, "0123456789ABCDEFGHIJ"));
    CHECK(FlushFileBuffers(h));
    z.QuadPart = 4; CHECK(SetFilePointerEx(h, z, NULL, FILE_BEGIN)); CHECK(SetEndOfFile(h));
    CHECK(xhcontent(h, "0123"));
    EXPECT_EQ(xattr_err(p), ERROR_FILE_NOT_FOUND);
    XCLOSE(h);
}
TEST(xdr_delete_survivor_final_path) {
    REQUIRE(U(DeleteFileW)); REQUIRE(U(GetFinalPathNameByHandleW)); XDR_NEED_NTFS();
    WCHAR p[MAX_PATH * 2], fp[MAX_PATH * 2]; HANDLE h = mk_open(p, "xdr_dg7.txt", "abc");
    DWORD n = U(GetFinalPathNameByHandleW)(h, fp, MAX_PATH * 2, FILE_NAME_NORMALIZED | VOLUME_NAME_NONE);
    CHECK(n > 0 && wcsstr(fp, L"xdr_dg7.txt") != NULL);
    XOK(U(DeleteFileW)(p));
    SetLastError(0);
    fp[0] = 0;
    n = U(GetFinalPathNameByHandleW)(h, fp, MAX_PATH * 2, FILE_NAME_NORMALIZED | VOLUME_NAME_NONE);
    DWORD e = GetLastError();
    t_note("final path after POSIX delete: n=%lu GLE=%lu '%ls'", n, e, fp);
    CHECKF(n > 0 && !_wcsnicmp(fp, L"\\$Extend\\$Deleted\\", 18), "n=%lu GLE=%lu", n, e);
    CHECK(wcsstr(fp, L"xdr_dg7") == NULL);
    fp[0] = 0;
    n = U(GetFinalPathNameByHandleW)(h, fp, MAX_PATH * 2, FILE_NAME_OPENED | VOLUME_NAME_NONE);
    t_note("opened-name final path after POSIX delete: n=%lu '%ls'", n, fp);
    XCLOSE(h);
}
TEST(xdr_delete_survivor_legacy_undelete_rejected) {
    REQUIRE(U(DeleteFileW)); REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR p[MAX_PATH * 2]; xp(p, "xdr_dg8.txt"); xmk(p, "abc");
    HANDLE h = xopen(p, GENERIC_READ | DELETE, XSHARE_ALL, OPEN_EXISTING, 0);
    XOKH(h);
    XOK(U(DeleteFileW)(p));
    SetLastError(0);
    BOOL r = xdisp_legacy(h, FALSE);
    DWORD e = GetLastError();
    CHECKF(!r && e == ERROR_ACCESS_DENIED, "r=%d GLE=%lu", r, e);
    FILE_STANDARD_INFO st; CHECK(xstd(h, &st));
    EXPECT_EQ(st.DeletePending, 1);
    XFAIL(xdisp(h, 0), ERROR_ACCESS_DENIED);
    XCLOSE(h);
    /* whatever the call returned, the name cannot come back */
    EXPECT_EQ(xattr_err(p), ERROR_FILE_NOT_FOUND);
}
TEST(xdr_delete_survivor_rename_rejected) {
    REQUIRE(U(DeleteFileW)); REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR p[MAX_PATH * 2], q[MAX_PATH * 2]; xp(p, "xdr_dg9.txt"); xp(q, "xdr_dg9b.txt"); xmk(p, "abc");
    HANDLE h = xopen(p, GENERIC_READ | DELETE, XSHARE_ALL, OPEN_EXISTING, 0);
    XOKH(h);
    XOK(U(DeleteFileW)(p));
    SetLastError(0);
    BOOL r = xren(h, XC_RENAME, q, 0, NULL);
    DWORD e = GetLastError();
    CHECKF(!r && e == ERROR_ACCESS_DENIED, "r=%d GLE=%lu", r, e);
    CHECK(!xexists(q));
    XCLOSE(h);
    xrm(q);
}
TEST(xdr_delete_parent_rmdir_after_posix_child) {
    REQUIRE(U(DeleteFileW)); REQUIRE(U(RemoveDirectoryW)); XDR_NEED_NTFS();
    WCHAR d[MAX_PATH * 2], p[MAX_PATH * 2];
    xp(d, "xdr_dpar"); xmkdir(d);
    xp(p, "xdr_dpar\\only.txt"); xmk(p, "child");
    HANDLE h = xopen(p, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    XOKH(h);
    XFAIL(U(RemoveDirectoryW)(d), ERROR_DIR_NOT_EMPTY);
    XOK(U(DeleteFileW)(p));
    XOK(U(RemoveDirectoryW)(d));
    EXPECT_EQ(xattr_err(d), ERROR_FILE_NOT_FOUND);
    CHECK(xhcontent(h, "child"));
    XCLOSE(h);
    xrm(d);
}
TEST(xdr_delete_parent_rmdir_legacy_pending_child) {
    REQUIRE(U(RemoveDirectoryW)); REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR d[MAX_PATH * 2], p[MAX_PATH * 2];
    xp(d, "xdr_dlp"); xmkdir(d);
    xp(p, "xdr_dlp\\only.txt"); xmk(p, "child");
    HANDLE h = xopen(p, GENERIC_READ | DELETE, XSHARE_ALL, OPEN_EXISTING, 0);
    XOKH(h);
    XOK(xdisp_legacy(h, TRUE));
    /* legacy delete keeps the name until the last handle closes */
    XFAIL(U(RemoveDirectoryW)(d), ERROR_DIR_NOT_EMPTY);
    EXPECT_EQ(xlisted(d, L"only.txt"), 1);
    XCLOSE(h);
    XOK(U(RemoveDirectoryW)(d));
    xrm(d);
}
TEST(xdr_delete_attr_only_handle_does_not_share_check) {
    REQUIRE(U(DeleteFileW)); XDR_NEED_NTFS();
    WCHAR p[MAX_PATH * 2]; xp(p, "xdr_dao.txt"); xmk(p, "abc");
    /* no read/write/delete access: such an open takes no part in share checks */
    HANDLE h = xopen(p, FILE_READ_ATTRIBUTES, 0, OPEN_EXISTING, 0);
    XOKH(h);
    XOK(U(DeleteFileW)(p));
    EXPECT_EQ(xattr_err(p), ERROR_FILE_NOT_FOUND);
    FILE_STANDARD_INFO st; CHECK(xstd(h, &st));
    EXPECT_EQ(st.DeletePending, 1);
    XCLOSE(h);
}
TEST(xdr_delete_on_legacy_pending_name_denied) {
    REQUIRE(U(DeleteFileW)); REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR p[MAX_PATH * 2]; xp(p, "xdr_dlpn.txt"); xmk(p, "abc");
    HANDLE h = xopen(p, GENERIC_READ | DELETE, XSHARE_ALL, OPEN_EXISTING, 0);
    XOKH(h);
    XOK(xdisp_legacy(h, TRUE));
    XFAIL(U(DeleteFileW)(p), ERROR_ACCESS_DENIED);
    EXPECT_EQ(xattr_err(p), ERROR_ACCESS_DENIED);
    SetLastError(0);
    HANDLE n = xopen(p, GENERIC_WRITE, XSHARE_ALL, CREATE_NEW, 0);
    CHECKF(n == INVALID_HANDLE_VALUE && GetLastError() == ERROR_ACCESS_DENIED, "CREATE_NEW over delete-pending: %p GLE %lu", n, GetLastError());
    XCLOSE(n);
    XCLOSE(h);
    EXPECT_EQ(xattr_err(p), ERROR_FILE_NOT_FOUND);
}
TEST(xdr_delete_recreate_then_listed_once) {
    REQUIRE(U(DeleteFileW)); XDR_NEED_NTFS();
    WCHAR d[MAX_PATH * 2], p[MAX_PATH * 2];
    xp(d, "xdr_drl"); xmkdir(d);
    xp(p, "xdr_drl\\f.txt"); xmk(p, "first");
    HANDLE h = xopen(p, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    XOK(U(DeleteFileW)(p));
    CHECK(xmk(p, "second!"));
    EXPECT_EQ(xlisted(d, L"f.txt"), 1);
    WIN32_FIND_DATAW fd; HANDLE f = U(FindFirstFileW)(p, &fd);
    CHECK(f != INVALID_HANDLE_VALUE);
    if (f != INVALID_HANDLE_VALUE) { EXPECT_EQ(fd.nFileSizeLow, 7); FindClose(f); }
    /* a second POSIX delete while the first deleted file is still open */
    HANDLE h2 = xopen(p, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    XOK(U(DeleteFileW)(p));
    EXPECT_EQ(xlisted(d, L"f.txt"), 0);
    CHECK(xhcontent(h, "first"));
    CHECK(xhcontent(h2, "second!"));
    XCLOSE(h); XCLOSE(h2);
    XOK(U(RemoveDirectoryW)(d));
}
TEST(xdr_delete_mapped_view_denied) {
    REQUIRE(U(DeleteFileW)); XDR_NEED_NTFS();
    WCHAR p[MAX_PATH * 2]; xp(p, "xdr_dmv.txt"); xmk(p, "mapped-data");
    HANDLE h = CreateFileW(p, GENERIC_READ, XSHARE_ALL, NULL, OPEN_EXISTING, 0, NULL);
    HANDLE m = CreateFileMappingW(h, NULL, PAGE_READONLY, 0, 0, NULL);
    const char *v = m ? (const char *)MapViewOfFile(m, FILE_MAP_READ, 0, 0, 0) : NULL;
    CloseHandle(h);
    if (m) CloseHandle(m);
    CHECK(v != NULL);
    SetLastError(0);
    BOOL r = U(DeleteFileW)(p);
    DWORD e = GetLastError();
    /* a mapped user view keeps the file from being deleted */
    CHECKF(!r && e == ERROR_ACCESS_DENIED, "r=%d GLE=%lu", r, e);
    EXPECT_EQ(xattr_err(p), 0);
    if (v) { CHECK(!memcmp(v, "mapped-data", 11)); UnmapViewOfFile(v); }
    XOK(U(DeleteFileW)(p));
    xrm(p);
}
TEST(xdr_delete_running_image_copy_denied) {
    REQUIRE(U(DeleteFileW)); XDR_NEED_NTFS();
    /* a loaded image section: the copy of this exe mapped as a DLL-like image */
    WCHAR p[MAX_PATH * 2]; xp(p, "xdr_img.dll");
    WCHAR self[MAX_PATH * 2]; GetModuleFileNameW(NULL, self, MAX_PATH * 2);
    CHECK(CopyFileW(self, p, FALSE));
    HMODULE mod = LoadLibraryExW(p, NULL, DONT_RESOLVE_DLL_REFERENCES);
    CHECKF(mod != NULL, "LoadLibraryEx GLE %lu", GetLastError());
    SetLastError(0);
    BOOL r = U(DeleteFileW)(p);
    DWORD e = GetLastError();
    CHECKF(!r && e == ERROR_ACCESS_DENIED, "r=%d GLE=%lu", r, e);
    if (mod) FreeLibrary(mod);
    XOK(U(DeleteFileW)(p));
    xrm(p);
}
TEST(xdr_delete_dir_handle_on_file_name_parent_open) {
    REQUIRE(U(DeleteFileW)); XDR_NEED_NTFS();
    WCHAR d[MAX_PATH * 2], p[MAX_PATH * 2];
    xp(d, "xdr_dpo"); xmkdir(d);
    xp(p, "xdr_dpo\\f.txt"); xmk(p, "abc");
    /* the parent directory is open without FILE_SHARE_DELETE: deleting a child is still allowed */
    HANDLE hd = xopendir(d, GENERIC_READ, XSHARE_RW);
    XOKH(hd);
    XOK(U(DeleteFileW)(p));
    EXPECT_EQ(xattr_err(p), ERROR_FILE_NOT_FOUND);
    XFAIL(U(RemoveDirectoryW)(d), ERROR_SHARING_VIOLATION);
    XCLOSE(hd);
    XOK(U(RemoveDirectoryW)(d));
}
