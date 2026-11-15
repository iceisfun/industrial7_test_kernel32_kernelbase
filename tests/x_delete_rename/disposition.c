/* FileDispositionInfo (legacy) versus FileDispositionInfoEx flag combinations on files and directories. */
#include "xdr.h"

static HANDLE mkd(WCHAR *p, const char *leaf, DWORD acc) {
    xp(p, leaf);
    xmk(p, "disp");
    return xopen(p, acc, XSHARE_ALL, OPEN_EXISTING, 0);
}

TEST(xdr_dispex_delete_without_posix_keeps_name) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR p[MAX_PATH * 2]; HANDLE h = mkd(p, "xdr_x1.txt", GENERIC_READ | DELETE);
    HANDLE o = xopen(p, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    XOKH(o);
    XOK(xdisp(h, XD_DEL));
    EXPECT_EQ(xattr_err(p), ERROR_ACCESS_DENIED);
    XCLOSE(h);
    /* not POSIX: the name stays (delete pending) while another handle is open */
    EXPECT_EQ(xattr_err(p), ERROR_ACCESS_DENIED);
    FILE_STANDARD_INFO st; CHECK(xstd(o, &st));
    /* a delete-pending link is not counted */
    EXPECT_EQ(st.DeletePending, 1); EXPECT_EQ(st.NumberOfLinks, 0);
    XCLOSE(o);
    EXPECT_EQ(xattr_err(p), ERROR_FILE_NOT_FOUND);
}
TEST(xdr_dispex_posix_name_until_setter_closes) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR p[MAX_PATH * 2]; HANDLE h = mkd(p, "xdr_x2.txt", GENERIC_READ | DELETE);
    HANDLE o = xopen(p, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    XOKH(o);
    XOK(xdisp(h, XD_DEL | XD_POSIX));
    DWORD before = xattr_err(p);
    EXPECT_EQ(before, ERROR_ACCESS_DENIED);
    XCLOSE(h);
    EXPECT_EQ(xattr_err(p), ERROR_FILE_NOT_FOUND);
    FILE_STANDARD_INFO st; CHECK(xstd(o, &st));
    EXPECT_EQ(st.DeletePending, 1); EXPECT_EQ(st.NumberOfLinks, 0);
    CHECK(xhcontent(o, "disp"));
    XCLOSE(o);
}
TEST(xdr_dispex_flags_zero_clears_pending) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR p[MAX_PATH * 2]; HANDLE h = mkd(p, "xdr_x3.txt", GENERIC_READ | DELETE);
    XOK(xdisp(h, XD_DEL));
    FILE_STANDARD_INFO st; CHECK(xstd(h, &st)); EXPECT_EQ(st.DeletePending, 1);
    XOK(xdisp(h, FILE_DISPOSITION_FLAG_DO_NOT_DELETE));
    CHECK(xstd(h, &st)); EXPECT_EQ(st.DeletePending, 0);
    XCLOSE(h);
    CHECK(xcontent(p, "disp"));
    xrm(p);
}
TEST(xdr_dispex_set_then_legacy_false_clears) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR p[MAX_PATH * 2]; HANDLE h = mkd(p, "xdr_x4.txt", GENERIC_READ | DELETE);
    XOK(xdisp(h, XD_DEL));
    XOK(xdisp_legacy(h, FALSE));
    XCLOSE(h);
    CHECK(xexists(p));
    xrm(p);
}
TEST(xdr_disp_legacy_true_then_ex_zero_clears) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR p[MAX_PATH * 2]; HANDLE h = mkd(p, "xdr_x5.txt", GENERIC_READ | DELETE);
    XOK(xdisp_legacy(h, TRUE));
    XOK(xdisp(h, 0));
    XCLOSE(h);
    CHECK(xexists(p));
    xrm(p);
}
TEST(xdr_dispex_posix_then_clear_before_close) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR p[MAX_PATH * 2]; HANDLE h = mkd(p, "xdr_x6.txt", GENERIC_READ | DELETE);
    HANDLE o = xopen(p, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    XOK(xdisp(h, XD_DEL | XD_POSIX));
    SetLastError(0);
    BOOL r = xdisp(h, 0);
    CHECK(r);
    XCLOSE(h);
    EXPECT_EQ(xattr_err(p), 0);
    FILE_STANDARD_INFO st; CHECK(xstd(o, &st)); EXPECT_EQ(st.DeletePending, 0); EXPECT_EQ(st.NumberOfLinks, 1);
    XCLOSE(o);
    xrm(p);
}
TEST(xdr_disp_legacy_nonzero_byte_means_delete) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    static const BYTE vals[] = { 1, 2, 0x80, 0xFF };
    for (unsigned i = 0; i < sizeof vals; i++) {
        WCHAR p[MAX_PATH * 2]; HANDLE h = mkd(p, "xdr_x7.txt", GENERIC_READ | DELETE);
        XOK(xdisp_legacy(h, vals[i]));
        FILE_STANDARD_INFO st; CHECK(xstd(h, &st));
        CHECKF(st.DeletePending == 1, "byte 0x%02x DeletePending=%d", vals[i], st.DeletePending);
        XCLOSE(h);
        CHECKF(!xexists(p), "byte 0x%02x left the file", vals[i]);
        xrm(p);
    }
}
TEST(xdr_dispex_on_close_clears_delete_on_close) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR p[MAX_PATH * 2]; xp(p, "xdr_x8.txt"); xmk(p, "doc");
    HANDLE h = xopen(p, GENERIC_READ | DELETE, XSHARE_ALL, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE);
    XOKH(h);
    SetLastError(0);
    BOOL r = xdisp(h, XD_CLOSE);
    CHECK(r);
    XCLOSE(h);
    CHECK(xcontent(p, "doc"));
    xrm(p);
}
TEST(xdr_disp_legacy_false_keeps_delete_on_close) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR p[MAX_PATH * 2]; xp(p, "xdr_x9.txt"); xmk(p, "doc");
    HANDLE h = xopen(p, GENERIC_READ | DELETE, XSHARE_ALL, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE);
    XOKH(h);
    XOK(xdisp_legacy(h, FALSE));
    XOK(xdisp(h, 0));
    XCLOSE(h);
    /* the open-time FILE_DELETE_ON_CLOSE is not a disposition: clearing the disposition leaves it */
    EXPECT_EQ(xattr_err(p), ERROR_FILE_NOT_FOUND);
    xrm(p);
}
TEST(xdr_dispex_on_close_with_delete_not_supported) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR p[MAX_PATH * 2]; HANDLE h = mkd(p, "xdr_x10.txt", GENERIC_READ | DELETE);
    /* NTFS takes ON_CLOSE only to clear an open-time delete-on-close, never together with DELETE */
    XFAIL(xdisp(h, XD_DEL | XD_CLOSE), ERROR_NOT_SUPPORTED);
    XFAIL(xdisp(h, XD_DEL | XD_POSIX | XD_CLOSE), ERROR_NOT_SUPPORTED);
    XFAIL(xdisp(h, XD_DEL | XD_CLOSE | XD_IGNRO), ERROR_NOT_SUPPORTED);
    FILE_STANDARD_INFO st; CHECK(xstd(h, &st)); EXPECT_EQ(st.DeletePending, 0);
    /* ON_CLOSE alone on a handle without delete-on-close is accepted and changes nothing */
    XOK(xdisp(h, XD_CLOSE));
    XCLOSE(h);
    CHECK(xcontent(p, "disp"));
    xrm(p);
}
TEST(xdr_dispex_on_close_delete_on_delete_on_close_handle) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR p[MAX_PATH * 2]; xp(p, "xdr_x11.txt"); xmk(p, "doc");
    HANDLE h = xopen(p, GENERIC_READ | DELETE, XSHARE_ALL, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE);
    XOKH(h);
    SetLastError(0);
    BOOL r = xdisp(h, XD_DEL | XD_CLOSE);
    DWORD e = GetLastError();
    /* accepted on a handle that already carries delete-on-close, with or without POSIX */
    CHECKF(r, "GLE=%lu", e);
    XOK(xdisp(h, XD_DEL | XD_POSIX | XD_CLOSE));
    XCLOSE(h);
    EXPECT_EQ(xattr_err(p), ERROR_FILE_NOT_FOUND);
    xrm(p);
}
TEST(xdr_dispex_readonly_posix_denied) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR p[MAX_PATH * 2]; HANDLE h = mkd(p, "xdr_x12.txt", GENERIC_READ | DELETE);
    SetFileAttributesW(p, FILE_ATTRIBUTE_READONLY);
    XFAIL(xdisp(h, XD_DEL | XD_POSIX), ERROR_ACCESS_DENIED);
    XFAIL(xdisp_legacy(h, TRUE), ERROR_ACCESS_DENIED);
    FILE_STANDARD_INFO st; CHECK(xstd(h, &st)); EXPECT_EQ(st.DeletePending, 0);
    XCLOSE(h);
    CHECK(xexists(p));
    xrm(p);
}
TEST(xdr_dispex_readonly_ignore_posix_ok) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR p[MAX_PATH * 2]; HANDLE h = mkd(p, "xdr_x13.txt", GENERIC_READ | DELETE);
    SetFileAttributesW(p, FILE_ATTRIBUTE_READONLY);
    HANDLE o = xopen(p, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    XOK(xdisp(h, XD_DEL | XD_POSIX | XD_IGNRO));
    XCLOSE(h);
    EXPECT_EQ(xattr_err(p), ERROR_FILE_NOT_FOUND);
    /* the attribute itself is untouched on the surviving file */
    FILE_BASIC_INFO bi; memset(&bi, 0, sizeof bi);
    CHECK(xgi(o, FileBasicInfo, &bi, sizeof bi));
    CHECK(bi.FileAttributes & FILE_ATTRIBUTE_READONLY);
    XCLOSE(o);
}
TEST(xdr_dispex_ignore_readonly_alone_no_delete) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR p[MAX_PATH * 2]; HANDLE h = mkd(p, "xdr_x14.txt", GENERIC_READ | DELETE);
    SetFileAttributesW(p, FILE_ATTRIBUTE_READONLY);
    SetLastError(0);
    BOOL r = xdisp(h, XD_IGNRO);
    CHECK(r);
    FILE_STANDARD_INFO st; CHECK(xstd(h, &st)); EXPECT_EQ(st.DeletePending, 0);
    XCLOSE(h);
    CHECK(xexists(p));
    xrm(p);
}
TEST(xdr_dispex_unknown_bits_not_supported) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR p[MAX_PATH * 2]; HANDLE h = mkd(p, "xdr_x15.txt", GENERIC_READ | DELETE);
    static const DWORD bad[] = { 0x20, 0x40, 0x80, 0x100, 0x1000, 0x10000, 0x80000000, 0xFFFFFFE0 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SetLastError(0);
        BOOL r = xdisp(h, bad[i]);
        DWORD e = GetLastError();
        /* NTFS answers STATUS_NOT_SUPPORTED for disposition bits it does not know */
        CHECKF(!r && e == ERROR_NOT_SUPPORTED, "flags 0x%lx: r=%d GLE=%lu", bad[i], r, e);
        SetLastError(0);
        r = xdisp(h, bad[i] | XD_DEL);
        e = GetLastError();
        CHECKF(!r && e == ERROR_NOT_SUPPORTED, "flags 0x%lx|DELETE: r=%d GLE=%lu", bad[i], r, e);
    }
    FILE_STANDARD_INFO st; CHECK(xstd(h, &st)); EXPECT_EQ(st.DeletePending, 0);
    XCLOSE(h);
    CHECK(xexists(p));
    xrm(p);
}
TEST(xdr_dispex_force_image_check_accepted) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR p[MAX_PATH * 2]; HANDLE h = mkd(p, "xdr_x16.txt", GENERIC_READ | DELETE);
    XOK(xdisp(h, XD_DEL | XD_IMG));
    XOK(xdisp(h, XD_IMG));
    FILE_STANDARD_INFO st; CHECK(xstd(h, &st)); EXPECT_EQ(st.DeletePending, 0);
    XOK(xdisp(h, XD_DEL | XD_POSIX | XD_IMG));
    XCLOSE(h);
    EXPECT_EQ(xattr_err(p), ERROR_FILE_NOT_FOUND);
}
TEST(xdr_dispex_requires_delete_access) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR p[MAX_PATH * 2]; HANDLE h = mkd(p, "xdr_x17.txt", GENERIC_READ | GENERIC_WRITE);
    XFAIL(xdisp(h, XD_DEL | XD_POSIX), ERROR_ACCESS_DENIED);
    XFAIL(xdisp(h, XD_DEL), ERROR_ACCESS_DENIED);
    XFAIL(xdisp(h, 0), ERROR_ACCESS_DENIED);
    XCLOSE(h);
    CHECK(xexists(p));
    xrm(p);
}
TEST(xdr_dispex_not_a_query_class) {
    REQUIRE(U(GetFileInformationByHandleEx)); XDR_NEED_NTFS();
    WCHAR p[MAX_PATH * 2]; HANDLE h = mkd(p, "xdr_x18.txt", GENERIC_READ | DELETE);
    FILE_DISPOSITION_INFO_EX d; d.Flags = 0x5A5A;
    XFAIL(xgi(h, XC_DISPEX, &d, sizeof d), ERROR_INVALID_PARAMETER);
    FILE_DISPOSITION_INFO l; memset(&l, 0, sizeof l);
    XFAIL(xgi(h, XC_DISP, &l, sizeof l), ERROR_INVALID_PARAMETER);
    XCLOSE(h);
    xrm(p);
}
TEST(xdr_dispex_twice_idempotent) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR p[MAX_PATH * 2]; HANDLE h = mkd(p, "xdr_x19.txt", GENERIC_READ | DELETE);
    XOK(xdisp(h, XD_DEL | XD_POSIX));
    XOK(xdisp(h, XD_DEL | XD_POSIX));
    XOK(xdisp(h, XD_DEL));
    XCLOSE(h);
    EXPECT_EQ(xattr_err(p), ERROR_FILE_NOT_FOUND);
}
/* ---- directories ---- */
TEST(xdr_dispex_dir_posix_empty_with_other_handle) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR d[MAX_PATH * 2]; xp(d, "xdr_xd1"); xmkdir(d);
    HANDLE h = xopendir(d, GENERIC_READ | DELETE, XSHARE_ALL);
    HANDLE o = xopendir(d, GENERIC_READ, XSHARE_ALL);
    XOKH(h); XOKH(o);
    XOK(xdisp(h, XD_DEL | XD_POSIX));
    XCLOSE(h);
    EXPECT_EQ(xattr_err(d), ERROR_FILE_NOT_FOUND);
    /* the name is free again although the directory is still open */
    XOK(CreateDirectoryW(d, NULL));
    FILE_STANDARD_INFO st; CHECK(xstd(o, &st));
    EXPECT_EQ(st.Directory, 1); EXPECT_EQ(st.DeletePending, 1);
    XCLOSE(o);
    xrm(d);
}
TEST(xdr_dispex_dir_legacy_with_other_handle) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR d[MAX_PATH * 2]; xp(d, "xdr_xd2"); xmkdir(d);
    HANDLE h = xopendir(d, GENERIC_READ | DELETE, XSHARE_ALL);
    HANDLE o = xopendir(d, GENERIC_READ, XSHARE_ALL);
    XOK(xdisp(h, XD_DEL));
    XCLOSE(h);
    EXPECT_EQ(xattr_err(d), ERROR_ACCESS_DENIED);
    XFAIL(CreateDirectoryW(d, NULL), ERROR_ACCESS_DENIED);
    XCLOSE(o);
    EXPECT_EQ(xattr_err(d), ERROR_FILE_NOT_FOUND);
}
TEST(xdr_dispex_dir_nonempty_dir_not_empty) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR d[MAX_PATH * 2], f[MAX_PATH * 2]; xp(d, "xdr_xd3"); xmkdir(d); xp(f, "xdr_xd3\\c.txt"); xmk(f, "c");
    HANDLE h = xopendir(d, GENERIC_READ | DELETE, XSHARE_ALL);
    XFAIL(xdisp(h, XD_DEL), ERROR_DIR_NOT_EMPTY);
    XFAIL(xdisp(h, XD_DEL | XD_POSIX), ERROR_DIR_NOT_EMPTY);
    XFAIL(xdisp(h, XD_DEL | XD_POSIX | XD_IGNRO), ERROR_DIR_NOT_EMPTY);
    XFAIL(xdisp_legacy(h, TRUE), ERROR_DIR_NOT_EMPTY);
    SetLastError(0);
    BOOL r = xdisp(h, XD_DEL | XD_CLOSE);
    CHECKF(!r && GetLastError() == ERROR_NOT_SUPPORTED, "DELETE|ON_CLOSE on a non-empty directory: r=%d GLE=%lu", r, GetLastError());
    XCLOSE(h);
    CHECK(xexists(d)); CHECK(xexists(f));
    xrm(f); xrm(d);
}
TEST(xdr_dispex_dir_only_posix_deleted_child) {
    REQUIRE(U(SetFileInformationByHandle)); REQUIRE(U(DeleteFileW)); XDR_NEED_NTFS();
    WCHAR d[MAX_PATH * 2], f[MAX_PATH * 2]; xp(d, "xdr_xd4"); xmkdir(d); xp(f, "xdr_xd4\\c.txt"); xmk(f, "c");
    HANDLE c = xopen(f, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    XOK(U(DeleteFileW)(f));
    HANDLE h = xopendir(d, GENERIC_READ | DELETE, XSHARE_ALL);
    XOK(xdisp(h, XD_DEL));
    XCLOSE(h);
    EXPECT_EQ(xattr_err(d), ERROR_FILE_NOT_FOUND);
    CHECK(xhcontent(c, "c"));
    XCLOSE(c);
}
TEST(xdr_dispex_dir_readonly) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR d[MAX_PATH * 2]; xp(d, "xdr_xd5"); xmkdir(d);
    SetFileAttributesW(d, FILE_ATTRIBUTE_READONLY);
    HANDLE h = xopendir(d, GENERIC_READ | DELETE, XSHARE_ALL);
    XOKH(h);
    XFAIL(xdisp(h, XD_DEL), ERROR_ACCESS_DENIED);
    XFAIL(xdisp(h, XD_DEL | XD_POSIX), ERROR_ACCESS_DENIED);
    XFAIL(xdisp_legacy(h, TRUE), ERROR_ACCESS_DENIED);
    XOK(xdisp(h, XD_DEL | XD_POSIX | XD_IGNRO));
    XCLOSE(h);
    EXPECT_EQ(xattr_err(d), ERROR_FILE_NOT_FOUND);
    xrm(d);
}
TEST(xdr_dispex_dir_clear_pending) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR d[MAX_PATH * 2]; xp(d, "xdr_xd6"); xmkdir(d);
    HANDLE h = xopendir(d, GENERIC_READ | DELETE, XSHARE_ALL);
    XOK(xdisp(h, XD_DEL | XD_POSIX));
    XOK(xdisp(h, 0));
    XCLOSE(h);
    CHECK(xexists(d));
    xrm(d);
}
