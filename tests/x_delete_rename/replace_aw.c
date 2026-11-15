/* ReplaceFileW against an open replaced file; A/W parity of DeleteFile and RemoveDirectory error paths;
 * a bare-name rename resolved in a child with its own current directory. */
#include "xdr.h"

static void rp_setup(WCHAR *a, WCHAR *b, const char *la, const char *lb) {
    xp(a, la); xp(b, lb);
    xmk(a, "REPLACED-OLD"); xmk(b, "REPLACEMENT");
}

TEST(xdr_replace_replaced_open_share_delete) {
    REQUIRE(U(ReplaceFileW)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; rp_setup(a, b, "xdr_p1a.txt", "xdr_p1b.txt");
    HANDLE h = xopen(a, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    SetLastError(0);
    BOOL r = U(ReplaceFileW)(a, b, NULL, 0, NULL, NULL);
    DWORD e = GetLastError();
    CHECKF(r, "GLE %lu", e);
    if (r) {
        CHECK(xcontent(a, "REPLACEMENT"));
        EXPECT_EQ(xattr_err(b), ERROR_FILE_NOT_FOUND);
        CHECK(xhcontent(h, "REPLACED-OLD"));
        FILE_STANDARD_INFO st; CHECK(xstd(h, &st));
        EXPECT_EQ(st.DeletePending, 1); EXPECT_EQ(st.NumberOfLinks, 0);
    }
    XCLOSE(h); xrm(a); xrm(b);
}
TEST(xdr_replace_replaced_open_share_delete_backup) {
    REQUIRE(U(ReplaceFileW)); REQUIRE(U(GetFinalPathNameByHandleW)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2], k[MAX_PATH * 2], fp[MAX_PATH * 2]; rp_setup(a, b, "xdr_p2a.txt", "xdr_p2b.txt");
    xp(k, "xdr_p2k.txt");
    HANDLE h = xopen(a, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    SetLastError(0);
    BOOL r = U(ReplaceFileW)(a, b, k, 0, NULL, NULL);
    DWORD e = GetLastError();
    CHECKF(r, "GLE %lu", e);
    if (r) {
        CHECK(xcontent(a, "REPLACEMENT"));
        CHECK(xcontent(k, "REPLACED-OLD"));
        fp[0] = 0;
        DWORD n = U(GetFinalPathNameByHandleW)(h, fp, MAX_PATH * 2, FILE_NAME_NORMALIZED | VOLUME_NAME_NONE);
        /* the old file kept its identity and now carries the backup name */
        CHECK(xhcontent(h, "REPLACED-OLD"));
        CHECK(n > 0 && wcsstr(fp, L"\\xdr_p2k.txt") != NULL);
    }
    XCLOSE(h); xrm(a); xrm(b); xrm(k);
}
TEST(xdr_replace_replaced_open_no_share_delete) {
    REQUIRE(U(ReplaceFileW)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; rp_setup(a, b, "xdr_p3a.txt", "xdr_p3b.txt");
    HANDLE h = xopen(a, GENERIC_READ, XSHARE_RW, OPEN_EXISTING, 0);
    SetLastError(0);
    BOOL r = U(ReplaceFileW)(a, b, NULL, 0, NULL, NULL);
    DWORD e = GetLastError();
    CHECKF(!r && e == ERROR_SHARING_VIOLATION, "r=%d GLE=%lu", r, e);
    CHECK(xcontent(a, "REPLACED-OLD"));
    CHECK(xcontent(b, "REPLACEMENT"));
    XCLOSE(h); xrm(a); xrm(b);
}
TEST(xdr_replace_replacement_open_share_delete) {
    REQUIRE(U(ReplaceFileW)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; rp_setup(a, b, "xdr_p4a.txt", "xdr_p4b.txt");
    HANDLE h = xopen(b, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    SetLastError(0);
    BOOL r = U(ReplaceFileW)(a, b, NULL, 0, NULL, NULL);
    DWORD e = GetLastError();
    /* ReplaceFile opens the replacement without sharing write access: a reader's handle is enough to stop it */
    CHECKF(!r && e == ERROR_SHARING_VIOLATION, "r=%d GLE=%lu", r, e);
    CHECK(xcontent(a, "REPLACED-OLD")); CHECK(xcontent(b, "REPLACEMENT"));
    XCLOSE(h); xrm(a); xrm(b);
}
TEST(xdr_replace_backup_existing_open) {
    REQUIRE(U(ReplaceFileW)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2], k[MAX_PATH * 2]; rp_setup(a, b, "xdr_p5a.txt", "xdr_p5b.txt");
    xp(k, "xdr_p5k.txt"); xmk(k, "BACKUP-OLD");
    HANDLE hk = xopen(k, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    SetLastError(0);
    BOOL r = U(ReplaceFileW)(a, b, k, 0, NULL, NULL);
    DWORD e = GetLastError();
    CHECKF(!r && e == ERROR_UNABLE_TO_REMOVE_REPLACED, "r=%d GLE=%lu", r, e);
    CHECK(xcontent(a, "REPLACED-OLD")); CHECK(xcontent(b, "REPLACEMENT"));
    CHECK(xhcontent(hk, "BACKUP-OLD"));
    XCLOSE(hk); xrm(a); xrm(b); xrm(k);
}
TEST(xdr_replace_backup_readonly) {
    REQUIRE(U(ReplaceFileW)); XDR_NEED_NTFS();
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2], k[MAX_PATH * 2]; rp_setup(a, b, "xdr_p6a.txt", "xdr_p6b.txt");
    xp(k, "xdr_p6k.txt"); xmk(k, "BACKUP-OLD");
    SetFileAttributesW(k, FILE_ATTRIBUTE_READONLY);
    SetLastError(0);
    BOOL r = U(ReplaceFileW)(a, b, k, 0, NULL, NULL);
    DWORD e = GetLastError();
    CHECKF(!r && e == ERROR_UNABLE_TO_REMOVE_REPLACED, "r=%d GLE=%lu", r, e);
    CHECK(xcontent(a, "REPLACED-OLD"));
    CHECK(xcontent(k, "BACKUP-OLD"));
    xrm(a); xrm(b); xrm(k);
}
/* ---- A/W parity ---- */
TEST(xdr_aw_delete_error_paths) {
    REQUIRE(U(DeleteFileW)); REQUIRE(U(DeleteFileA)); XDR_NEED_NTFS();
    char a[MAX_PATH * 2]; WCHAR w[MAX_PATH * 2];
    /* missing file, missing directory */
    xpa(a, "xdr_aw_none.txt"); xp(w, "xdr_aw_none.txt");
    XFAIL(U(DeleteFileA)(a), ERROR_FILE_NOT_FOUND); XFAIL(U(DeleteFileW)(w), ERROR_FILE_NOT_FOUND);
    xpa(a, "xdr_aw_nodir\\x.txt"); xp(w, "xdr_aw_nodir\\x.txt");
    XFAIL(U(DeleteFileA)(a), ERROR_PATH_NOT_FOUND); XFAIL(U(DeleteFileW)(w), ERROR_PATH_NOT_FOUND);
    /* read-only file */
    xpa(a, "xdr_aw_ro.txt"); xp(w, "xdr_aw_ro.txt"); xmk(w, "ro");
    SetFileAttributesW(w, FILE_ATTRIBUTE_READONLY);
    XFAIL(U(DeleteFileA)(a), ERROR_ACCESS_DENIED); XFAIL(U(DeleteFileW)(w), ERROR_ACCESS_DENIED);
    CHECK(xexists(w)); xrm(w);
    /* a directory name */
    xpa(a, "xdr_aw_dir"); xp(w, "xdr_aw_dir"); xmkdir(w);
    XFAIL(U(DeleteFileA)(a), ERROR_ACCESS_DENIED); XFAIL(U(DeleteFileW)(w), ERROR_ACCESS_DENIED);
    CHECK(xexists(w)); xrm(w);
    /* open without share-delete */
    xpa(a, "xdr_aw_sv.txt"); xp(w, "xdr_aw_sv.txt"); xmk(w, "sv");
    HANDLE h = xopen(w, GENERIC_READ, XSHARE_RW, OPEN_EXISTING, 0);
    XFAIL(U(DeleteFileA)(a), ERROR_SHARING_VIOLATION); XFAIL(U(DeleteFileW)(w), ERROR_SHARING_VIOLATION);
    XCLOSE(h); xrm(w);
    /* file used as a directory component */
    xpa(a, "xdr_aw_f.txt\\x"); xp(w, "xdr_aw_f.txt\\x");
    WCHAR fw[MAX_PATH * 2]; xp(fw, "xdr_aw_f.txt"); xmk(fw, "f");
    XFAIL(U(DeleteFileA)(a), ERROR_PATH_NOT_FOUND); XFAIL(U(DeleteFileW)(w), ERROR_PATH_NOT_FOUND);
    xrm(fw);
}
TEST(xdr_aw_delete_empty_and_root) {
    REQUIRE(U(DeleteFileW)); REQUIRE(U(DeleteFileA)); XDR_NEED_NTFS();
    SetLastError(0); BOOL ra = U(DeleteFileA)(""); DWORD ea = GetLastError();
    SetLastError(0); BOOL rw = U(DeleteFileW)(L""); DWORD ew = GetLastError();
    CHECKF(!ra && !rw && ea == ew, "empty: A r=%d GLE=%lu, W r=%d GLE=%lu", ra, ea, rw, ew);
    EXPECT_EQ(ew, ERROR_PATH_NOT_FOUND);
    /* the scratch directory with a trailing backslash */
    char a[MAX_PATH * 2]; WCHAR w[MAX_PATH * 2];
    xpa(a, "xdr_aw_tb"); xp(w, "xdr_aw_tb"); xmkdir(w);
    strcat(a, "\\"); wcscat(w, L"\\");
    SetLastError(0); ra = U(DeleteFileA)(a); ea = GetLastError();
    SetLastError(0); rw = U(DeleteFileW)(w); ew = GetLastError();
    CHECKF(!ra && !rw && ea == ew, "dir with trailing backslash: A r=%d GLE=%lu, W r=%d GLE=%lu", ra, ea, rw, ew);
    EXPECT_EQ(ew, ERROR_ACCESS_DENIED);
    w[wcslen(w) - 1] = 0; xrm(w);
}
TEST(xdr_aw_rmdir_error_paths) {
    REQUIRE(U(RemoveDirectoryW)); REQUIRE(U(RemoveDirectoryA)); XDR_NEED_NTFS();
    char a[MAX_PATH * 2]; WCHAR w[MAX_PATH * 2], f[MAX_PATH * 2];
    xpa(a, "xdr_awr_none"); xp(w, "xdr_awr_none");
    XFAIL(U(RemoveDirectoryA)(a), ERROR_FILE_NOT_FOUND); XFAIL(U(RemoveDirectoryW)(w), ERROR_FILE_NOT_FOUND);
    xpa(a, "xdr_awr_nodir\\d"); xp(w, "xdr_awr_nodir\\d");
    XFAIL(U(RemoveDirectoryA)(a), ERROR_PATH_NOT_FOUND); XFAIL(U(RemoveDirectoryW)(w), ERROR_PATH_NOT_FOUND);
    /* a file name */
    xpa(a, "xdr_awr_f.txt"); xp(w, "xdr_awr_f.txt"); xmk(w, "f");
    XFAIL(U(RemoveDirectoryA)(a), ERROR_DIRECTORY); XFAIL(U(RemoveDirectoryW)(w), ERROR_DIRECTORY);
    CHECK(xexists(w)); xrm(w);
    /* non-empty */
    xpa(a, "xdr_awr_ne"); xp(w, "xdr_awr_ne"); xmkdir(w); xp(f, "xdr_awr_ne\\c"); xmk(f, "c");
    XFAIL(U(RemoveDirectoryA)(a), ERROR_DIR_NOT_EMPTY); XFAIL(U(RemoveDirectoryW)(w), ERROR_DIR_NOT_EMPTY);
    xrm(f); xrm(w);
    /* empty name */
    XFAIL(U(RemoveDirectoryA)(""), ERROR_PATH_NOT_FOUND); XFAIL(U(RemoveDirectoryW)(L""), ERROR_PATH_NOT_FOUND);
    /* wildcard */
    xpa(a, "xdr_awr_*"); xp(w, "xdr_awr_*");
    XFAIL(U(RemoveDirectoryA)(a), ERROR_INVALID_NAME); XFAIL(U(RemoveDirectoryW)(w), ERROR_INVALID_NAME);
}
TEST(xdr_aw_rmdir_trailing_backslash_ok) {
    REQUIRE(U(RemoveDirectoryW)); REQUIRE(U(RemoveDirectoryA)); XDR_NEED_NTFS();
    char a[MAX_PATH * 2]; WCHAR w[MAX_PATH * 2];
    xpa(a, "xdr_awr_tb\\"); xp(w, "xdr_awr_tb");
    xmkdir(w); XOK(U(RemoveDirectoryA)(a)); CHECK(!xexists(w));
    xmkdir(w); wcscat(w, L"\\"); XOK(U(RemoveDirectoryW)(w));
    w[wcslen(w) - 1] = 0; CHECK(!xexists(w));
}
TEST(xdr_aw_delete_success_both) {
    REQUIRE(U(DeleteFileW)); REQUIRE(U(DeleteFileA)); XDR_NEED_NTFS();
    char a[MAX_PATH * 2]; WCHAR w[MAX_PATH * 2];
    xpa(a, "xdr_aw_ok.txt"); xp(w, "xdr_aw_ok.txt");
    /* the A form follows the same POSIX path: the name goes while a share-delete handle is open */
    xmk(w, "a");
    HANDLE h = xopen(w, GENERIC_READ, XSHARE_ALL, OPEN_EXISTING, 0);
    XOK(U(DeleteFileA)(a));
    EXPECT_EQ(xattr_err(w), ERROR_FILE_NOT_FOUND);
    XCLOSE(h);
    xmk(w, "w"); XOK(U(DeleteFileW)(w)); CHECK(!xexists(w));
}
TEST(xdr_aw_rmdir_posix_both) {
    REQUIRE(U(RemoveDirectoryW)); REQUIRE(U(RemoveDirectoryA)); XDR_NEED_NTFS();
    char a[MAX_PATH * 2]; WCHAR w[MAX_PATH * 2];
    xpa(a, "xdr_awr_px"); xp(w, "xdr_awr_px"); xmkdir(w);
    HANDLE h = xopendir(w, GENERIC_READ, XSHARE_ALL);
    XOK(U(RemoveDirectoryA)(a));
    EXPECT_EQ(xattr_err(w), ERROR_FILE_NOT_FOUND);
    XCLOSE(h);
}
/* ---- bare-name rename resolved against a child's current directory ---- */
CHILD(xdr_bare_rename) {
    /* argv[1] = source path, argv[2] = directory to make current */
    if (argc < 3) return 90;
    if (!SetCurrentDirectoryA(argv[2])) return 91;
    WCHAR src[MAX_PATH * 2]; MultiByteToWideChar(CP_ACP, 0, argv[1], -1, src, MAX_PATH * 2);
    HANDLE h = U(CreateFileW)(src, GENERIC_READ | DELETE, XSHARE_ALL, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 92;
    BOOL r = xren(h, XC_RENAMEEX, L"bare.txt", 0, NULL);
    DWORD e = GetLastError();
    CloseHandle(h);
    return r ? 0 : (int)(100 + (e & 0xFF));
}
TEST(xdr_renex_bare_name_uses_current_directory) {
    REQUIRE(U(SetFileInformationByHandle)); XDR_NEED_NTFS();
    char src[MAX_PATH * 2], cwd[MAX_PATH * 2], args[MAX_PATH * 5];
    WCHAR srcw[MAX_PATH * 2], in_cwd[MAX_PATH * 2], in_src_dir[MAX_PATH * 2], d[MAX_PATH * 2], sd[MAX_PATH * 2];
    xp(sd, "xdr_bn_src"); xmkdir(sd); xp(d, "xdr_bn_cwd"); xmkdir(d);
    xpa(src, "xdr_bn_src\\s.txt"); xp(srcw, "xdr_bn_src\\s.txt"); xmk(srcw, "bare");
    xpa(cwd, "xdr_bn_cwd");
    xp(in_cwd, "xdr_bn_cwd\\bare.txt"); xp(in_src_dir, "xdr_bn_src\\bare.txt");
    snprintf(args, sizeof args, "\"%s\" \"%s\"", src, cwd);
    DWORD rc = t_run_child("xdr_bare_rename", args, 20000);
    int where = xexists(in_cwd) ? 1 : xexists(in_src_dir) ? 2 : xexists(srcw) ? 3 : 0;
    t_note("bare-name rename: child rc=%lu, landed %s", rc, where == 1 ? "in the current directory" : where == 2 ? "beside the source" : where == 3 ? "nowhere (source kept)" : "?");
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(where, 1);
    xrm(in_cwd); xrm(in_src_dir); xrm(srcw); xrm(sd); xrm(d);
}
