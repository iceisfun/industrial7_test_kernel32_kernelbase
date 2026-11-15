/* FileRenameInfoEx flag matrix and FileRenameInfo (legacy) over existing, open, read-only and directory targets. */
#include "xdr.h"

/* source file "src" opened for rename; target "dst" with its own content */
static HANDLE pair(WCHAR *a, WCHAR *b, const char *la, const char *lb) {
    xp(a, la); xp(b, lb);
    xmk(a, "SRC"); xmk(b, "DST-OLD");
    return xopen(a, GENERIC_READ | DELETE, XSHARE_ALL, OPEN_EXISTING, 0);
}

TEST(xdr_renex_replace_if_exists) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; HANDLE h = pair(a, b, "xdr_r1a.txt", "xdr_r1b.txt");
    XFAIL(xren(h, XC_RENAMEEX, b, 0, NULL), ERROR_ALREADY_EXISTS);
    XOK(xren(h, XC_RENAMEEX, b, XR_REPLACE, NULL));
    EXPECT_EQ(xattr_err(a), ERROR_FILE_NOT_FOUND);
    CHECK(xcontent(b, "SRC"));
    XCLOSE(h); xrm(b);
}
TEST(xdr_renex_replace_open_target_without_posix_denied) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; HANDLE h = pair(a, b, "xdr_r2a.txt", "xdr_r2b.txt");
    HANDLE t = xopen(b, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    XFAIL(xren(h, XC_RENAMEEX, b, XR_REPLACE, NULL), ERROR_ACCESS_DENIED);
    CHECK(xcontent(b, "DST-OLD")); CHECK(xexists(a));
    XCLOSE(t); XCLOSE(h); xrm(a); xrm(b);
}
TEST(xdr_renex_posix_replace_open_target) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; HANDLE h = pair(a, b, "xdr_r3a.txt", "xdr_r3b.txt");
    HANDLE t = xopen(b, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    XOK(xren(h, XC_RENAMEEX, b, XR_REPLACE | XR_POSIX, NULL));
    CHECK(xcontent(b, "SRC"));
    EXPECT_EQ(xattr_err(a), ERROR_FILE_NOT_FOUND);
    /* the replaced file lives on, unlinked, behind its open handle */
    CHECK(xhcontent(t, "DST-OLD"));
    FILE_STANDARD_INFO st; CHECK(xstd(t, &st));
    EXPECT_EQ(st.DeletePending, 1); EXPECT_EQ(st.NumberOfLinks, 0);
    XCLOSE(t); XCLOSE(h); xrm(b);
}
TEST(xdr_renex_posix_without_replace_collides) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; HANDLE h = pair(a, b, "xdr_r4a.txt", "xdr_r4b.txt");
    XFAIL(xren(h, XC_RENAMEEX, b, XR_POSIX, NULL), ERROR_ALREADY_EXISTS);
    XFAIL(xren(h, XC_RENAMEEX, b, XR_POSIX | XR_IGNRO, NULL), ERROR_ALREADY_EXISTS);
    CHECK(xcontent(b, "DST-OLD"));
    XCLOSE(h); xrm(a); xrm(b);
}
TEST(xdr_renex_posix_replace_target_no_share_delete) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; HANDLE h = pair(a, b, "xdr_r5a.txt", "xdr_r5b.txt");
    HANDLE t = xopen(b, GENERIC_READ, XSHARE_RW, OPEN_EXISTING, 0);
    SetLastError(0);
    BOOL r = xren(h, XC_RENAMEEX, b, XR_REPLACE | XR_POSIX, NULL);
    DWORD e = GetLastError();
    CHECKF(!r && e == ERROR_SHARING_VIOLATION, "r=%d GLE=%lu", r, e);
    CHECK(xcontent(b, "DST-OLD"));
    XCLOSE(t); XCLOSE(h); xrm(a); xrm(b);
}
TEST(xdr_renex_readonly_target_needs_ignore_flag) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; HANDLE h = pair(a, b, "xdr_r6a.txt", "xdr_r6b.txt");
    SetFileAttributesW(b, FILE_ATTRIBUTE_READONLY);
    XFAIL(xren(h, XC_RENAMEEX, b, XR_REPLACE, NULL), ERROR_ACCESS_DENIED);
    XFAIL(xren(h, XC_RENAMEEX, b, XR_REPLACE | XR_POSIX, NULL), ERROR_ACCESS_DENIED);
    CHECK(xcontent(b, "DST-OLD"));
    XOK(xren(h, XC_RENAMEEX, b, XR_REPLACE | XR_IGNRO, NULL));
    CHECK(xcontent(b, "SRC"));
    /* the target name now carries the source's (normal) attributes */
    DWORD at = GetFileAttributesW(b);
    EXPECT_EQ(at & FILE_ATTRIBUTE_READONLY, 0);
    XCLOSE(h); xrm(b);
}
TEST(xdr_renex_readonly_target_ignore_posix_open) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; HANDLE h = pair(a, b, "xdr_r7a.txt", "xdr_r7b.txt");
    SetFileAttributesW(b, FILE_ATTRIBUTE_READONLY);
    HANDLE t = xopen(b, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    XOK(xren(h, XC_RENAMEEX, b, XR_REPLACE | XR_POSIX | XR_IGNRO, NULL));
    CHECK(xcontent(b, "SRC"));
    CHECK(xhcontent(t, "DST-OLD"));
    XCLOSE(t); XCLOSE(h); xrm(b);
}
TEST(xdr_renex_ignore_readonly_alone_collides) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; HANDLE h = pair(a, b, "xdr_r8a.txt", "xdr_r8b.txt");
    SetFileAttributesW(b, FILE_ATTRIBUTE_READONLY);
    XFAIL(xren(h, XC_RENAMEEX, b, XR_IGNRO, NULL), ERROR_ALREADY_EXISTS);
    XCLOSE(h); xrm(a); xrm(b);
}
TEST(xdr_renex_readonly_source_renames) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; xp(a, "xdr_r9a.txt"); xp(b, "xdr_r9b.txt"); xmk(a, "RO");
    SetFileAttributesW(a, FILE_ATTRIBUTE_READONLY);
    HANDLE h = xopen(a, GENERIC_READ | DELETE, XSHARE_ALL, OPEN_EXISTING, 0);
    XOKH(h);
    XOK(xren(h, XC_RENAMEEX, b, 0, NULL));
    CHECK(GetFileAttributesW(b) & FILE_ATTRIBUTE_READONLY);
    XCLOSE(h); xrm(b);
}
TEST(xdr_renex_unknown_flags_not_supported) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; xp(a, "xdr_r10a.txt"); xp(b, "xdr_r10b.txt"); xmk(a, "x");
    HANDLE h = xopen(a, GENERIC_READ | DELETE, XSHARE_ALL, OPEN_EXISTING, 0);
    static const DWORD bad[] = { 0x200, 0x400, 0x1000, 0x10000, 0x80000000 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SetLastError(0);
        BOOL r = xren(h, XC_RENAMEEX, b, bad[i], NULL);
        DWORD e = GetLastError();
        /* NTFS answers STATUS_NOT_SUPPORTED for rename flags it does not know */
        CHECKF(!r && e == ERROR_NOT_SUPPORTED, "flags 0x%lx: r=%d GLE=%lu", bad[i], r, e);
    }
    CHECK(xexists(a)); CHECK(!xexists(b));
    XCLOSE(h); xrm(a); xrm(b);
}
TEST(xdr_renex_suppress_flags_accepted) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; xp(a, "xdr_r11a.txt"); xp(b, "xdr_r11b.txt"); xmk(a, "x");
    HANDLE h = xopen(a, GENERIC_READ | DELETE, XSHARE_ALL, OPEN_EXISTING, 0);
    XOK(xren(h, XC_RENAMEEX, b, XR_SUPPRESS_PIN, NULL));
    XOK(xren(h, XC_RENAMEEX, a, XR_SUPPRESS_SR, NULL));
    XOK(xren(h, XC_RENAMEEX, b, XR_SUPPRESS_PIN | XR_SUPPRESS_SR | XR_POSIX, NULL));
    CHECK(xcontent(b, "x"));
    for (DWORD f = 0x10; f <= 0x100; f <<= 1) {
        if (f == XR_IGNRO) continue;
        SetLastError(0);
        BOOL r = xren(h, XC_RENAMEEX, b, f, NULL);
        CHECKF(r, "rename-ex flag 0x%lx onto itself: GLE=%lu", f, GetLastError());
    }
    XCLOSE(h); xrm(a); xrm(b);
}
TEST(xdr_renex_dir_onto_existing_dir) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2], m[MAX_PATH * 2], m2[MAX_PATH * 2];
    xp(a, "xdr_rd1a"); xp(b, "xdr_rd1b"); xmkdir(a); xmkdir(b);
    xp(m, "xdr_rd1a\\mark.txt"); xmk(m, "mark"); xp(m2, "xdr_rd1b\\mark.txt");
    HANDLE h = xopendir(a, GENERIC_READ | DELETE, XSHARE_ALL);
    XOKH(h);
    XFAIL(xren(h, XC_RENAMEEX, b, 0, NULL), ERROR_ALREADY_EXISTS);
    XFAIL(xren(h, XC_RENAMEEX, b, XR_REPLACE, NULL), ERROR_ACCESS_DENIED);
    CHECK(xexists(a)); CHECK(xexists(b));
    /* with POSIX semantics an empty target directory is replaced */
    XOK(xren(h, XC_RENAMEEX, b, XR_REPLACE | XR_POSIX, NULL));
    EXPECT_EQ(xattr_err(a), ERROR_FILE_NOT_FOUND);
    CHECK(xcontent(m2, "mark"));
    XCLOSE(h); xrm(m2); xrm(b); xrm(a);
}
TEST(xdr_renex_dir_posix_onto_nonempty_dir) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2], c[MAX_PATH * 2];
    xp(a, "xdr_rd4a"); xp(b, "xdr_rd4b"); xmkdir(a); xmkdir(b); xp(c, "xdr_rd4b\\c.txt"); xmk(c, "c");
    HANDLE h = xopendir(a, GENERIC_READ | DELETE, XSHARE_ALL);
    SetLastError(0);
    BOOL r = xren(h, XC_RENAMEEX, b, XR_REPLACE | XR_POSIX, NULL);
    DWORD e = GetLastError();
    t_note("POSIX replace onto a non-empty directory: r=%d GLE=%lu", r, e);
    CHECKF(!r && e == ERROR_DIR_NOT_EMPTY, "r=%d GLE=%lu", r, e);
    CHECK(xexists(a)); CHECK(xcontent(c, "c"));
    XCLOSE(h); xrm(c); xrm(b); xrm(a);
}
TEST(xdr_renex_dir_posix_onto_open_empty_dir) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2];
    xp(a, "xdr_rd5a"); xp(b, "xdr_rd5b"); xmkdir(a); xmkdir(b);
    HANDLE t = xopendir(b, GENERIC_READ, XSHARE_ALL);
    HANDLE h = xopendir(a, GENERIC_READ | DELETE, XSHARE_ALL);
    SetLastError(0);
    BOOL r = xren(h, XC_RENAMEEX, b, XR_REPLACE | XR_POSIX, NULL);
    DWORD e = GetLastError();
    t_note("POSIX replace onto an open empty directory: r=%d GLE=%lu", r, e);
    CHECKF(r, "GLE %lu", e);
    if (r) { FILE_STANDARD_INFO st; CHECK(xstd(t, &st)); EXPECT_EQ(st.DeletePending, 1); EXPECT_EQ(st.Directory, 1); }
    XCLOSE(h); XCLOSE(t); xrm(b); xrm(a);
}
TEST(xdr_renex_dir_onto_existing_file) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; xp(a, "xdr_rd2a"); xp(b, "xdr_rd2b.txt"); xmkdir(a); xmk(b, "file");
    HANDLE h = xopendir(a, GENERIC_READ | DELETE, XSHARE_ALL);
    XFAIL(xren(h, XC_RENAMEEX, b, 0, NULL), ERROR_ALREADY_EXISTS);
    SetLastError(0);
    BOOL r = xren(h, XC_RENAMEEX, b, XR_REPLACE, NULL);
    DWORD e = GetLastError();
    t_note("directory replacing a file: r=%d GLE=%lu", r, e);
    CHECKF(r, "GLE %lu", e);
    if (r) { DWORD at = GetFileAttributesW(b); CHECK(at != INVALID_FILE_ATTRIBUTES && (at & FILE_ATTRIBUTE_DIRECTORY)); }
    XCLOSE(h); xrm(a); xrm(b);
}
TEST(xdr_renex_file_onto_existing_dir) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; xp(a, "xdr_rd3a.txt"); xp(b, "xdr_rd3b"); xmk(a, "f"); xmkdir(b);
    HANDLE h = xopen(a, GENERIC_READ | DELETE, XSHARE_ALL, OPEN_EXISTING, 0);
    XFAIL(xren(h, XC_RENAMEEX, b, XR_REPLACE, NULL), ERROR_ACCESS_DENIED);
    XFAIL(xren(h, XC_RENAMEEX, b, XR_REPLACE | XR_POSIX, NULL), ERROR_ACCESS_DENIED);
    CHECK(GetFileAttributesW(b) & FILE_ATTRIBUTE_DIRECTORY);
    XCLOSE(h); xrm(a); xrm(b);
}
TEST(xdr_renex_relative_root_directory) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], d[MAX_PATH * 2], b[MAX_PATH * 2];
    xp(a, "xdr_rr1.txt"); xmk(a, "rel"); xp(d, "xdr_rr1d"); xmkdir(d); xp(b, "xdr_rr1d\\moved.txt");
    HANDLE root = xopendir(d, FILE_TRAVERSE | FILE_ADD_FILE | SYNCHRONIZE, XSHARE_ALL);
    XOKH(root);
    HANDLE h = xopen(a, GENERIC_READ | DELETE, XSHARE_ALL, OPEN_EXISTING, 0);
    /* SetFileInformationByHandle does not pass a relative RootDirectory rename through */
    XFAIL(xren(h, XC_RENAMEEX, L"moved.txt", 0, root), ERROR_INVALID_PARAMETER);
    XFAIL(xren(h, XC_RENAMEEX, L"moved.txt", XR_REPLACE | XR_POSIX, root), ERROR_INVALID_PARAMETER);
    CHECK(!xexists(b));
    CHECK(xcontent(a, "rel"));
    XCLOSE(h); XCLOSE(root); xrm(a); xrm(b); xrm(d);
}
TEST(xdr_renex_relative_root_replace_and_subpath) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], d[MAX_PATH * 2], s[MAX_PATH * 2], b[MAX_PATH * 2];
    xp(a, "xdr_rr2.txt"); xmk(a, "rel2"); xp(d, "xdr_rr2d"); xmkdir(d); xp(s, "xdr_rr2d\\sub"); xmkdir(s);
    xp(b, "xdr_rr2d\\sub\\t.txt"); xmk(b, "old");
    HANDLE root = xopendir(d, FILE_TRAVERSE | SYNCHRONIZE, XSHARE_ALL);
    HANDLE h = xopen(a, GENERIC_READ | DELETE, XSHARE_ALL, OPEN_EXISTING, 0);
    XFAIL(xren(h, XC_RENAMEEX, L"sub\\t.txt", 0, root), ERROR_INVALID_PARAMETER);
    XFAIL(xren(h, XC_RENAMEEX, L"sub\\t.txt", XR_REPLACE, root), ERROR_INVALID_PARAMETER);
    CHECK(xcontent(b, "old")); CHECK(xcontent(a, "rel2"));
    /* the same target by full path works */
    XOK(xren(h, XC_RENAMEEX, b, XR_REPLACE | XR_POSIX, NULL));
    CHECK(xcontent(b, "rel2"));
    XCLOSE(h); XCLOSE(root); xrm(a); xrm(b); xrm(s); xrm(d);
}
TEST(xdr_ren_legacy_relative_root_directory) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], d[MAX_PATH * 2], b[MAX_PATH * 2];
    xp(a, "xdr_rr3.txt"); xmk(a, "rel3"); xp(d, "xdr_rr3d"); xmkdir(d); xp(b, "xdr_rr3d\\m.txt"); xmk(b, "old");
    HANDLE root = xopendir(d, FILE_TRAVERSE | SYNCHRONIZE, XSHARE_ALL);
    HANDLE h = xopen(a, GENERIC_READ | DELETE, XSHARE_ALL, OPEN_EXISTING, 0);
    XFAIL(xren(h, XC_RENAME, L"m.txt", 0, root), ERROR_INVALID_PARAMETER);
    XFAIL(xren(h, XC_RENAME, L"m.txt", 1, root), ERROR_INVALID_PARAMETER);
    CHECK(xcontent(b, "old")); CHECK(xcontent(a, "rel3"));
    XCLOSE(h); XCLOSE(root); xrm(a); xrm(b); xrm(d);
}
TEST(xdr_renex_root_is_a_file_handle) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], f[MAX_PATH * 2]; xp(a, "xdr_rr4.txt"); xmk(a, "x"); xp(f, "xdr_rr4f.txt"); xmk(f, "f");
    HANDLE root = xopen(f, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    HANDLE h = xopen(a, GENERIC_READ | DELETE, XSHARE_ALL, OPEN_EXISTING, 0);
    SetLastError(0);
    BOOL r = xren(h, XC_RENAMEEX, L"y.txt", 0, root);
    DWORD e = GetLastError();
    CHECKF(!r && e == ERROR_INVALID_PARAMETER, "r=%d GLE=%lu", r, e);
    CHECK(xexists(a));
    XCLOSE(h); XCLOSE(root); xrm(a); xrm(f);
}
TEST(xdr_renex_handle_follows_new_name) {
    REQUIRE(U(SetFileInformationByHandle)); REQUIRE(U(GetFinalPathNameByHandleW)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2], fp[MAX_PATH * 2];
    xp(a, "xdr_rf1.txt"); xp(b, "xdr_rf2.txt"); xmk(a, "x");
    HANDLE h = xopen(a, GENERIC_READ | DELETE, XSHARE_ALL, OPEN_EXISTING, 0);
    XOK(xren(h, XC_RENAMEEX, b, XR_POSIX, NULL));
    DWORD n = U(GetFinalPathNameByHandleW)(h, fp, MAX_PATH * 2, FILE_NAME_NORMALIZED | VOLUME_NAME_NONE);
    CHECK(n > 0 && wcsstr(fp, L"\\xdr_rf2.txt") != NULL);
    XCLOSE(h); xrm(b);
}
TEST(xdr_renex_onto_posix_deleted_name) {
    REQUIRE(U(SetFileInformationByHandle)); REQUIRE(U(DeleteFileW)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; HANDLE h = pair(a, b, "xdr_rp1a.txt", "xdr_rp1b.txt");
    HANDLE t = xopen(b, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    XOK(U(DeleteFileW)(b));
    XOK(xren(h, XC_RENAMEEX, b, 0, NULL));
    CHECK(xcontent(b, "SRC"));
    CHECK(xhcontent(t, "DST-OLD"));
    XCLOSE(t); XCLOSE(h); xrm(b);
}
TEST(xdr_renex_onto_legacy_pending_target) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; HANDLE h = pair(a, b, "xdr_rp2a.txt", "xdr_rp2b.txt");
    HANDLE t = xopen(b, GENERIC_READ | DELETE, XSHARE_ALL, OPEN_EXISTING, 0);
    XOK(xdisp_legacy(t, TRUE));
    XFAIL(xren(h, XC_RENAMEEX, b, 0, NULL), ERROR_ALREADY_EXISTS);
    XFAIL(xren(h, XC_RENAMEEX, b, XR_REPLACE, NULL), ERROR_ACCESS_DENIED);
    SetLastError(0);
    BOOL r = xren(h, XC_RENAMEEX, b, XR_REPLACE | XR_POSIX, NULL);
    DWORD e = GetLastError();
    CHECKF(!r && e == ERROR_ACCESS_DENIED, "r=%d GLE=%lu", r, e);
    XCLOSE(t);
    /* the pending delete completes on close; the name is then free */
    EXPECT_EQ(xattr_err(b), ERROR_FILE_NOT_FOUND);
    XOK(xren(h, XC_RENAMEEX, b, 0, NULL));
    XCLOSE(h);
    CHECK(xcontent(b, "SRC"));
    xrm(a); xrm(b);
}
TEST(xdr_renex_posix_replace_target_with_second_link) {
    REQUIRE(U(SetFileInformationByHandle)); REQUIRE(U(CreateHardLinkW)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2], l[MAX_PATH * 2]; HANDLE h = pair(a, b, "xdr_rl1a.txt", "xdr_rl1b.txt");
    xp(l, "xdr_rl1l.txt");
    XOK(U(CreateHardLinkW)(l, b, NULL));
    HANDLE t = xopen(b, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    XOK(xren(h, XC_RENAMEEX, b, XR_REPLACE | XR_POSIX, NULL));
    CHECK(xcontent(b, "SRC"));
    CHECK(xcontent(l, "DST-OLD"));
    FILE_STANDARD_INFO st; CHECK(xstd(t, &st));
    /* the handle's link was the one replaced: pending, while the file keeps its other link */
    EXPECT_EQ(st.NumberOfLinks, 1); EXPECT_EQ(st.DeletePending, 1);
    EXPECT_EQ(xlinks_by_name(l), 1);
    XCLOSE(t); XCLOSE(h); xrm(b); xrm(l);
}
TEST(xdr_renex_short_buffer_bad_length) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; xp(a, "xdr_rb1.txt"); xmk(a, "x"); xp(b, "xdr_rb1b.txt");
    HANDLE h = xopen(a, GENERIC_READ | DELETE, XSHARE_ALL, OPEN_EXISTING, 0);
    _Alignas(8) BYTE buf[1024]; memset(buf, 0, sizeof buf);
    FILE_RENAME_INFO *ri = (FILE_RENAME_INFO *)buf;
    DWORD bytes = (DWORD)wcslen(b) * 2;
    ri->FileNameLength = bytes; memcpy(ri->FileName, b, bytes);
    /* a buffer smaller than the structure itself, for both rename classes */
    XFAIL(xsi(h, XC_RENAMEEX, ri, (DWORD)sizeof(FILE_RENAME_INFO) - 1), ERROR_BAD_LENGTH);
    XFAIL(xsi(h, XC_RENAME, ri, (DWORD)sizeof(FILE_RENAME_INFO) - 1), ERROR_BAD_LENGTH);
    XFAIL(xsi(h, XC_RENAMEEX, ri, 0), ERROR_BAD_LENGTH);
    XCLOSE(h); xrm(a); xrm(b);
}
TEST(xdr_renex_dos_name_read_to_terminator) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; xp(a, "xdr_rt1.txt"); xmk(a, "t"); xp(b, "xdr_rt1b.txt");
    HANDLE h = xopen(a, GENERIC_READ | DELETE, XSHARE_ALL, OPEN_EXISTING, 0);
    _Alignas(8) BYTE buf[1024]; memset(buf, 0, sizeof buf);
    FILE_RENAME_INFO *ri = (FILE_RENAME_INFO *)buf;
    /* without a root the DOS name is converted up to its terminator: FileNameLength 0 still names b */
    memcpy(ri->FileName, b, wcslen(b) * 2);
    ri->FileNameLength = 0;
    XOK(xsi(h, XC_RENAMEEX, ri, sizeof buf));
    CHECK(xcontent(b, "t")); EXPECT_EQ(xattr_err(a), ERROR_FILE_NOT_FOUND);
    /* a length covering only a prefix of a: the whole terminated name is used again */
    memset(buf, 0, sizeof buf);
    memcpy(ri->FileName, a, wcslen(a) * 2);
    ri->FileNameLength = 6;
    XOK(xsi(h, XC_RENAME, ri, sizeof buf));
    CHECK(xcontent(a, "t")); EXPECT_EQ(xattr_err(b), ERROR_FILE_NOT_FOUND);
    XCLOSE(h); xrm(a); xrm(b);
}
/* ---- legacy FileRenameInfo ---- */
TEST(xdr_ren_legacy_replace_false_true) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; HANDLE h = pair(a, b, "xdr_rl2a.txt", "xdr_rl2b.txt");
    XFAIL(xren(h, XC_RENAME, b, FALSE, NULL), ERROR_ALREADY_EXISTS);
    CHECK(xcontent(b, "DST-OLD"));
    XOK(xren(h, XC_RENAME, b, TRUE, NULL));
    CHECK(xcontent(b, "SRC"));
    XCLOSE(h); xrm(b);
}
TEST(xdr_ren_legacy_open_target) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; HANDLE h = pair(a, b, "xdr_rl3a.txt", "xdr_rl3b.txt");
    HANDLE t = xopen(b, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    XFAIL(xren(h, XC_RENAME, b, TRUE, NULL), ERROR_ACCESS_DENIED);
    XCLOSE(t);
    HANDLE t2 = xopen(b, GENERIC_READ, XSHARE_RW, OPEN_EXISTING, 0);
    XFAIL(xren(h, XC_RENAME, b, TRUE, NULL), ERROR_ACCESS_DENIED);
    XFAIL(xren(h, XC_RENAME, b, FALSE, NULL), ERROR_ALREADY_EXISTS);
    XCLOSE(t2);
    XOK(xren(h, XC_RENAME, b, TRUE, NULL));
    XCLOSE(h); xrm(b);
}
TEST(xdr_ren_legacy_flags_word_low_byte) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; HANDLE h = pair(a, b, "xdr_rl4a.txt", "xdr_rl4b.txt");
    HANDLE t = xopen(b, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    /* the legacy class reads ReplaceIfExists as a BOOLEAN: 0x100 has a zero low byte, the POSIX bit means nothing */
    XFAIL(xren(h, XC_RENAME, b, 0x100, NULL), ERROR_ALREADY_EXISTS);
    SetLastError(0);
    BOOL r = xren(h, XC_RENAME, b, XR_POSIX, NULL);
    DWORD e = GetLastError();
    t_note("legacy class with ReplaceIfExists=2 over an open target: r=%d GLE=%lu", r, e);
    CHECKF(!r && e == ERROR_ACCESS_DENIED, "r=%d GLE=%lu", r, e);
    CHECK(xcontent(b, "DST-OLD"));
    XCLOSE(t);
    XOK(xren(h, XC_RENAME, b, 0x2, NULL));
    CHECK(xcontent(b, "SRC"));
    XCLOSE(h); xrm(b);
}
TEST(xdr_ren_legacy_dir_onto_dir) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; xp(a, "xdr_rl5a"); xp(b, "xdr_rl5b"); xmkdir(a); xmkdir(b);
    HANDLE h = xopendir(a, GENERIC_READ | DELETE, XSHARE_ALL);
    XFAIL(xren(h, XC_RENAME, b, FALSE, NULL), ERROR_ALREADY_EXISTS);
    XFAIL(xren(h, XC_RENAME, b, TRUE, NULL), ERROR_ACCESS_DENIED);
    XCLOSE(h); xrm(a); xrm(b);
}
