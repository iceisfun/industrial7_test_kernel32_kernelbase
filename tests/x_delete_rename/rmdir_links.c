/* RemoveDirectory with open handles and read-only directories; hard links deleted while open. */
#include "xdr.h"

TEST(xdr_rmdir_open_share_delete_name_gone) {
    REQUIRE(U(RemoveDirectoryW)); XDR_NEED_NTFS();
    WCHAR d[MAX_PATH * 2]; xp(d, "xdr_rd_o1"); xmkdir(d);
    HANDLE h = xopendir(d, GENERIC_READ, XSHARE_ALL);
    XOKH(h);
    XOK(U(RemoveDirectoryW)(d));
    EXPECT_EQ(xattr_err(d), ERROR_FILE_NOT_FOUND);
    /* the name is free while the old directory is still open */
    XOK(CreateDirectoryW(d, NULL));
    FILE_STANDARD_INFO st; CHECK(xstd(h, &st));
    EXPECT_EQ(st.Directory, 1); EXPECT_EQ(st.DeletePending, 1);
    XCLOSE(h);
    XOK(U(RemoveDirectoryW)(d));
}
TEST(xdr_rmdir_survivor_cannot_create_children) {
    REQUIRE(U(RemoveDirectoryW)); XDR_NEED_NTFS();
    WCHAR d[MAX_PATH * 2]; xp(d, "xdr_rd_o2"); xmkdir(d);
    HANDLE h = xopendir(d, GENERIC_READ | FILE_ADD_FILE | FILE_TRAVERSE, XSHARE_ALL);
    XOKH(h);
    XOK(U(RemoveDirectoryW)(d));
    /* a relative create under the deleted directory, through NtCreateFile with the handle as root */
    typedef NTSTATUS (NTAPI *ntcf)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK, PLARGE_INTEGER, ULONG, ULONG, ULONG, ULONG, PVOID, ULONG);
    ntcf f = (ntcf)t_proc_nt("NtCreateFile");
    if (f) {
        UNICODE_STRING us; WCHAR nm[] = L"child.txt";
        us.Buffer = nm; us.Length = (USHORT)(wcslen(nm) * 2); us.MaximumLength = us.Length + 2;
        OBJECT_ATTRIBUTES oa; memset(&oa, 0, sizeof oa); oa.Length = sizeof oa; oa.RootDirectory = h; oa.ObjectName = &us; oa.Attributes = OBJ_CASE_INSENSITIVE;
        IO_STATUS_BLOCK io; HANDLE c = NULL;
        NTSTATUS s = f(&c, GENERIC_WRITE | SYNCHRONIZE, &oa, &io, NULL, 0, XSHARE_ALL, 2 /* FILE_CREATE */, 0x20 /* SYNCHRONOUS_IO_NONALERT */, NULL, 0);
        t_note("create under a POSIX-deleted directory: status 0x%08lx", (unsigned long)s);
        CHECKF(s == (NTSTATUS)0xC0000056, "status 0x%08lx want STATUS_DELETE_PENDING", (unsigned long)s);
        if (s >= 0) CloseHandle(c);
    }
    XCLOSE(h);
}
TEST(xdr_rmdir_open_no_share_delete) {
    REQUIRE(U(RemoveDirectoryW)); REQUIRE(U(RemoveDirectoryA)); XDR_NEED_NTFS();
    WCHAR d[MAX_PATH * 2]; char da[MAX_PATH * 2]; xp(d, "xdr_rd_o3"); xpa(da, "xdr_rd_o3"); xmkdir(d);
    HANDLE h = xopendir(d, GENERIC_READ, XSHARE_RW);
    XFAIL(U(RemoveDirectoryW)(d), ERROR_SHARING_VIOLATION);
    XFAIL(U(RemoveDirectoryA)(da), ERROR_SHARING_VIOLATION);
    CHECK(xexists(d));
    XCLOSE(h);
    XOK(U(RemoveDirectoryA)(da));
}
TEST(xdr_rmdir_readonly_directory) {
    REQUIRE(U(RemoveDirectoryW)); REQUIRE(U(RemoveDirectoryA)); XDR_NEED_NTFS();
    WCHAR d[MAX_PATH * 2]; char da[MAX_PATH * 2]; xp(d, "xdr_rd_ro"); xpa(da, "xdr_rd_ro"); xmkdir(d);
    XOK(U(SetFileAttributesW)(d, FILE_ATTRIBUTE_READONLY));
    XFAIL(U(RemoveDirectoryW)(d), ERROR_ACCESS_DENIED);
    XFAIL(U(RemoveDirectoryA)(da), ERROR_ACCESS_DENIED);
    CHECK(xexists(d));
    XOK(U(SetFileAttributesW)(d, FILE_ATTRIBUTE_NORMAL));
    XOK(U(RemoveDirectoryW)(d));
}
TEST(xdr_rmdir_with_open_child_handle) {
    REQUIRE(U(RemoveDirectoryW)); XDR_NEED_NTFS();
    WCHAR d[MAX_PATH * 2], f[MAX_PATH * 2]; xp(d, "xdr_rd_c1"); xmkdir(d); xp(f, "xdr_rd_c1\\c.txt"); xmk(f, "c");
    HANDLE c = xopen(f, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    XFAIL(U(RemoveDirectoryW)(d), ERROR_DIR_NOT_EMPTY);
    XCLOSE(c);
    XFAIL(U(RemoveDirectoryW)(d), ERROR_DIR_NOT_EMPTY);
    xrm(f);
    XOK(U(RemoveDirectoryW)(d));
}
TEST(xdr_rmdir_posix_deleted_subdir_open) {
    REQUIRE(U(RemoveDirectoryW)); XDR_NEED_NTFS();
    WCHAR d[MAX_PATH * 2], s[MAX_PATH * 2]; xp(d, "xdr_rd_c2"); xmkdir(d); xp(s, "xdr_rd_c2\\s"); xmkdir(s);
    HANDLE c = xopendir(s, GENERIC_READ, XSHARE_ALL);
    XOK(U(RemoveDirectoryW)(s));
    XOK(U(RemoveDirectoryW)(d));
    EXPECT_EQ(xattr_err(d), ERROR_FILE_NOT_FOUND);
    XCLOSE(c);
}
TEST(xdr_rmdir_hidden_system_directory) {
    REQUIRE(U(RemoveDirectoryW)); XDR_NEED_NTFS();
    WCHAR d[MAX_PATH * 2]; xp(d, "xdr_rd_hs"); xmkdir(d);
    XOK(U(SetFileAttributesW)(d, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM));
    XOK(U(RemoveDirectoryW)(d));
    EXPECT_EQ(xattr_err(d), ERROR_FILE_NOT_FOUND);
}
/* ---- hard links ---- */
TEST(xdr_link_delete_other_name_while_open) {
    REQUIRE(U(DeleteFileW)); REQUIRE(U(CreateHardLinkW)); XDR_NEED_NTFS();
    WCHAR f[MAX_PATH * 2], g[MAX_PATH * 2]; xp(f, "xdr_hl1f.txt"); xp(g, "xdr_hl1g.txt"); xmk(f, "linked");
    XOK(U(CreateHardLinkW)(g, f, NULL));
    HANDLE h = xopen(f, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    FILE_STANDARD_INFO st; CHECK(xstd(h, &st)); EXPECT_EQ(st.NumberOfLinks, 2);
    XOK(U(DeleteFileW)(g));
    EXPECT_EQ(xattr_err(g), ERROR_FILE_NOT_FOUND);
    CHECK(xstd(h, &st)); EXPECT_EQ(st.NumberOfLinks, 1); EXPECT_EQ(st.DeletePending, 0);
    CHECK(xcontent(f, "linked"));
    XOK(U(DeleteFileW)(f));
    CHECK(xstd(h, &st)); EXPECT_EQ(st.NumberOfLinks, 0); EXPECT_EQ(st.DeletePending, 1);
    XCLOSE(h);
}
TEST(xdr_link_delete_opened_name) {
    REQUIRE(U(DeleteFileW)); REQUIRE(U(CreateHardLinkW)); REQUIRE(U(GetFinalPathNameByHandleW)); XDR_NEED_NTFS();
    WCHAR f[MAX_PATH * 2], g[MAX_PATH * 2], fp[MAX_PATH * 2]; xp(f, "xdr_hl2f.txt"); xp(g, "xdr_hl2g.txt"); xmk(f, "linked2");
    XOK(U(CreateHardLinkW)(g, f, NULL));
    HANDLE h = xopen(g, GENERIC_READ | GENERIC_WRITE, XSHARE_ALL, OPEN_EXISTING, 0);
    XOK(U(DeleteFileW)(g));
    EXPECT_EQ(xattr_err(g), ERROR_FILE_NOT_FOUND);
    CHECK(xcontent(f, "linked2"));
    FILE_STANDARD_INFO st; CHECK(xstd(h, &st));
    /* the handle's own link is gone (delete pending), the file still has the other one */
    EXPECT_EQ(st.NumberOfLinks, 1);
    EXPECT_EQ(st.DeletePending, 1);
    fp[0] = 0;
    DWORD n = U(GetFinalPathNameByHandleW)(h, fp, MAX_PATH * 2, FILE_NAME_NORMALIZED | VOLUME_NAME_NONE);
    /* the handle names the unlinked link under $Extend\$Deleted, not the surviving name */
    CHECKF(n > 0 && !_wcsnicmp(fp, L"\\$Extend\\$Deleted\\", 18), "n=%lu", n);
    /* writes through the handle reach the surviving name */
    DWORD w; CHECK(WriteFile(h, "L", 1, &w, NULL));
    XCLOSE(h);
    CHECK(xcontent(f, "Linked2"));
    xrm(f);
}
TEST(xdr_link_count_by_name_updates) {
    REQUIRE(U(DeleteFileW)); REQUIRE(U(CreateHardLinkW)); XDR_NEED_NTFS();
    WCHAR f[MAX_PATH * 2], g[MAX_PATH * 2], k[MAX_PATH * 2];
    xp(f, "xdr_hl3f.txt"); xp(g, "xdr_hl3g.txt"); xp(k, "xdr_hl3k.txt"); xmk(f, "x");
    XOK(U(CreateHardLinkW)(g, f, NULL));
    XOK(U(CreateHardLinkW)(k, f, NULL));
    EXPECT_EQ(xlinks_by_name(f), 3);
    HANDLE h = xopen(k, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    XOK(U(DeleteFileW)(g));
    EXPECT_EQ(xlinks_by_name(f), 2);
    EXPECT_EQ(xlinks_by_name(k), 2);
    /* a new link may reuse the deleted name at once */
    XOK(U(CreateHardLinkW)(g, k, NULL));
    EXPECT_EQ(xlinks_by_name(f), 3);
    XCLOSE(h);
    xrm(f); xrm(g); xrm(k);
}
TEST(xdr_link_create_onto_posix_deleted_name) {
    REQUIRE(U(DeleteFileW)); REQUIRE(U(CreateHardLinkW)); XDR_NEED_NTFS();
    WCHAR f[MAX_PATH * 2], g[MAX_PATH * 2]; xp(f, "xdr_hl4f.txt"); xp(g, "xdr_hl4g.txt"); xmk(f, "f"); xmk(g, "g");
    HANDLE h = xopen(g, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    XFAIL(U(CreateHardLinkW)(g, f, NULL), ERROR_ALREADY_EXISTS);
    XOK(U(DeleteFileW)(g));
    XOK(U(CreateHardLinkW)(g, f, NULL));
    CHECK(xcontent(g, "f"));
    CHECK(xhcontent(h, "g"));
    XCLOSE(h);
    xrm(f); xrm(g);
}
TEST(xdr_link_create_from_posix_deleted_file) {
    REQUIRE(U(DeleteFileW)); REQUIRE(U(CreateHardLinkW)); XDR_NEED_NTFS();
    WCHAR f[MAX_PATH * 2], g[MAX_PATH * 2]; xp(f, "xdr_hl5f.txt"); xp(g, "xdr_hl5g.txt"); xmk(f, "f");
    HANDLE h = xopen(f, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    XOK(U(DeleteFileW)(f));
    XFAIL(U(CreateHardLinkW)(g, f, NULL), ERROR_FILE_NOT_FOUND);
    CHECK(!xexists(g));
    XCLOSE(h);
}
TEST(xdr_link_dispex_posix_one_link) {
    REQUIRE(U(SetFileInformationByHandle)); REQUIRE(U(CreateHardLinkW)); XDR_NEED_NTFS();
    WCHAR f[MAX_PATH * 2], g[MAX_PATH * 2]; xp(f, "xdr_hl6f.txt"); xp(g, "xdr_hl6g.txt"); xmk(f, "six");
    XOK(U(CreateHardLinkW)(g, f, NULL));
    HANDLE h = xopen(g, GENERIC_READ | DELETE, XSHARE_ALL, OPEN_EXISTING, 0);
    HANDLE o = xopen(f, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    XOK(xdisp(h, XD_DEL | XD_POSIX));
    XCLOSE(h);
    EXPECT_EQ(xattr_err(g), ERROR_FILE_NOT_FOUND);
    FILE_STANDARD_INFO st; CHECK(xstd(o, &st)); EXPECT_EQ(st.NumberOfLinks, 1); EXPECT_EQ(st.DeletePending, 0);
    XCLOSE(o);
    CHECK(xcontent(f, "six"));
    xrm(f);
}
TEST(xdr_link_legacy_delete_one_link_open) {
    REQUIRE(U(SetFileInformationByHandle)); REQUIRE(U(CreateHardLinkW)); XDR_NEED_NTFS();
    WCHAR f[MAX_PATH * 2], g[MAX_PATH * 2]; xp(f, "xdr_hl7f.txt"); xp(g, "xdr_hl7g.txt"); xmk(f, "seven");
    XOK(U(CreateHardLinkW)(g, f, NULL));
    HANDLE h = xopen(g, GENERIC_READ | DELETE, XSHARE_ALL, OPEN_EXISTING, 0);
    HANDLE o = xopen(f, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    XOK(xdisp_legacy(h, TRUE));
    FILE_STANDARD_INFO st; CHECK(xstd(o, &st));
    /* a delete-pending link no longer counts; the file itself is not delete pending */
    EXPECT_EQ(st.NumberOfLinks, 1); EXPECT_EQ(st.DeletePending, 0);
    XCLOSE(h);
    /* the pending link goes with the last handle opened through it, not with the file */
    EXPECT_EQ(xattr_err(g), ERROR_FILE_NOT_FOUND);
    CHECK(xstd(o, &st)); EXPECT_EQ(st.NumberOfLinks, 1);
    XCLOSE(o);
    EXPECT_EQ(xattr_err(g), ERROR_FILE_NOT_FOUND);
    CHECK(xcontent(f, "seven"));
    EXPECT_EQ(xlinks_by_name(f), 1);
    xrm(f);
}
