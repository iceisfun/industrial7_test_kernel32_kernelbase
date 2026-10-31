/* Transacted file APIs: without a valid KTM transaction handle they must fail with ERROR_INVALID_TRANSACTION
 * (NULL and INVALID_HANDLE_VALUE are both rejected before any file system work is done). */
#include "fa.h"

#define ERR_INVALID_TRANSACTION 6700
static const HANDLE BADTX[2] = { NULL, INVALID_HANDLE_VALUE };

TEST(ftx_copyfile_transacted) {
    REQUIRE(U(CopyFileTransactedA)); REQUIRE(U(CopyFileTransactedW));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("tx_cs.txt", "x"); fa_path(s, "tx_cs.txt"); fa_path(d, "tx_cd.txt");
    WCHAR ws[MAX_PATH * 2], wd[MAX_PATH * 2]; fa_widen(ws, s); fa_widen(wd, d);
    for (int i = 0; i < 2; i++) {
        SetLastError(0); EXPECT_EQ(U(CopyFileTransactedA)(s, d, NULL, NULL, NULL, 0, BADTX[i]), FALSE); EXPECT_ERR(ERR_INVALID_TRANSACTION);
        SetLastError(0); EXPECT_EQ(U(CopyFileTransactedW)(ws, wd, NULL, NULL, NULL, 0, BADTX[i]), FALSE); EXPECT_ERR(ERR_INVALID_TRANSACTION);
    }
    CHECK(!fa_exists(d));
    fa_rm(s);
}
TEST(ftx_movefile_transacted) {
    REQUIRE(U(MoveFileTransactedA)); REQUIRE(U(MoveFileTransactedW));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("tx_ms.txt", "x"); fa_path(s, "tx_ms.txt"); fa_path(d, "tx_md.txt");
    WCHAR ws[MAX_PATH * 2], wd[MAX_PATH * 2]; fa_widen(ws, s); fa_widen(wd, d);
    for (int i = 0; i < 2; i++) {
        SetLastError(0); EXPECT_EQ(U(MoveFileTransactedA)(s, d, NULL, NULL, 0, BADTX[i]), FALSE); EXPECT_ERR(ERR_INVALID_TRANSACTION);
        SetLastError(0); EXPECT_EQ(U(MoveFileTransactedW)(ws, wd, NULL, NULL, 0, BADTX[i]), FALSE); EXPECT_ERR(ERR_INVALID_TRANSACTION);
    }
    CHECK(fa_exists(s)); CHECK(!fa_exists(d));
    fa_rm(s);
}
TEST(ftx_deletefile_transacted) {
    REQUIRE(U(DeleteFileTransactedA)); REQUIRE(U(DeleteFileTransactedW));
    char s[MAX_PATH * 2]; fa_mkfile_s("tx_ds.txt", "x"); fa_path(s, "tx_ds.txt"); WCHAR ws[MAX_PATH * 2]; fa_widen(ws, s);
    for (int i = 0; i < 2; i++) {
        SetLastError(0); EXPECT_EQ(U(DeleteFileTransactedA)(s, BADTX[i]), FALSE); EXPECT_ERR(ERR_INVALID_TRANSACTION);
        SetLastError(0); EXPECT_EQ(U(DeleteFileTransactedW)(ws, BADTX[i]), FALSE); EXPECT_ERR(ERR_INVALID_TRANSACTION);
    }
    CHECK(fa_exists(s));
    fa_rm(s);
}
TEST(ftx_getfileattributes_transacted) {
    REQUIRE(U(GetFileAttributesTransactedA)); REQUIRE(U(GetFileAttributesTransactedW));
    char s[MAX_PATH * 2]; fa_mkfile_s("tx_gs.txt", "x"); fa_path(s, "tx_gs.txt"); WCHAR ws[MAX_PATH * 2]; fa_widen(ws, s);
    WIN32_FILE_ATTRIBUTE_DATA d;
    for (int i = 0; i < 2; i++) {
        SetLastError(0); EXPECT_EQ(U(GetFileAttributesTransactedA)(s, GetFileExInfoStandard, &d, BADTX[i]), FALSE); EXPECT_ERR(ERR_INVALID_TRANSACTION);
        SetLastError(0); EXPECT_EQ(U(GetFileAttributesTransactedW)(ws, GetFileExInfoStandard, &d, BADTX[i]), FALSE); EXPECT_ERR(ERR_INVALID_TRANSACTION);
    }
    fa_rm(s);
}
TEST(ftx_setfileattributes_transacted) {
    REQUIRE(U(SetFileAttributesTransactedA)); REQUIRE(U(SetFileAttributesTransactedW));
    char s[MAX_PATH * 2]; fa_mkfile_s("tx_as.txt", "x"); fa_path(s, "tx_as.txt"); WCHAR ws[MAX_PATH * 2]; fa_widen(ws, s);
    for (int i = 0; i < 2; i++) {
        SetLastError(0); EXPECT_EQ(U(SetFileAttributesTransactedA)(s, FILE_ATTRIBUTE_HIDDEN, BADTX[i]), FALSE); EXPECT_ERR(ERR_INVALID_TRANSACTION);
        SetLastError(0); EXPECT_EQ(U(SetFileAttributesTransactedW)(ws, FILE_ATTRIBUTE_HIDDEN, BADTX[i]), FALSE); EXPECT_ERR(ERR_INVALID_TRANSACTION);
    }
    CHECK(!(GetFileAttributesA(s) & FILE_ATTRIBUTE_HIDDEN));
    fa_rm(s);
}
TEST(ftx_createhardlink_transacted) {
    REQUIRE(U(CreateHardLinkTransactedA)); REQUIRE(U(CreateHardLinkTransactedW));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("tx_hs.txt", "x"); fa_path(s, "tx_hs.txt"); fa_path(d, "tx_hd.txt");
    WCHAR ws[MAX_PATH * 2], wd[MAX_PATH * 2]; fa_widen(ws, s); fa_widen(wd, d);
    for (int i = 0; i < 2; i++) {
        SetLastError(0); EXPECT_EQ(U(CreateHardLinkTransactedA)(d, s, NULL, BADTX[i]), FALSE); EXPECT_ERR(ERR_INVALID_TRANSACTION);
        SetLastError(0); EXPECT_EQ(U(CreateHardLinkTransactedW)(wd, ws, NULL, BADTX[i]), FALSE); EXPECT_ERR(ERR_INVALID_TRANSACTION);
    }
    CHECK(!fa_exists(d));
    fa_rm(s);
}
TEST(ftx_createsymboliclink_transacted) {
    REQUIRE(U(CreateSymbolicLinkTransactedA)); REQUIRE(U(CreateSymbolicLinkTransactedW));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("tx_ss.txt", "x"); fa_path(s, "tx_ss.txt"); fa_path(d, "tx_sd.txt");
    WCHAR ws[MAX_PATH * 2], wd[MAX_PATH * 2]; fa_widen(ws, s); fa_widen(wd, d);
    for (int i = 0; i < 2; i++) {
        SetLastError(0); EXPECT_EQ(U(CreateSymbolicLinkTransactedA)(d, s, 0, BADTX[i]), 0); EXPECT_ERR(ERR_INVALID_TRANSACTION);
        SetLastError(0); EXPECT_EQ(U(CreateSymbolicLinkTransactedW)(wd, ws, 0, BADTX[i]), 0); EXPECT_ERR(ERR_INVALID_TRANSACTION);
    }
    CHECK(!fa_exists(d));
    fa_rm(s);
}
TEST(ftx_getcompressedfilesize_transacted) {
    REQUIRE(U(GetCompressedFileSizeTransactedA)); REQUIRE(U(GetCompressedFileSizeTransactedW));
    char s[MAX_PATH * 2]; fa_mkfile_s("tx_zs.txt", "abc"); fa_path(s, "tx_zs.txt"); WCHAR ws[MAX_PATH * 2]; fa_widen(ws, s);
    DWORD hi = 0x77;
    for (int i = 0; i < 2; i++) {
        SetLastError(0); EXPECT_EQ(U(GetCompressedFileSizeTransactedA)(s, &hi, BADTX[i]), INVALID_FILE_SIZE); EXPECT_ERR(ERR_INVALID_TRANSACTION);
        SetLastError(0); EXPECT_EQ(U(GetCompressedFileSizeTransactedW)(ws, &hi, BADTX[i]), INVALID_FILE_SIZE); EXPECT_ERR(ERR_INVALID_TRANSACTION);
    }
    EXPECT_EQ(hi, 0x77);
    fa_rm(s);
}
TEST(ftx_createfile_transacted) {
    REQUIRE(U(CreateFileTransactedA)); REQUIRE(U(CreateFileTransactedW));
    char s[MAX_PATH * 2]; fa_path(s, "tx_cf.txt"); WCHAR ws[MAX_PATH * 2]; fa_widen(ws, s);
    for (int i = 0; i < 2; i++) {
        SetLastError(0);
        HANDLE h = U(CreateFileTransactedA)(s, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL, BADTX[i], NULL, NULL);
        EXPECT_EQ(h, INVALID_HANDLE_VALUE); EXPECT_ERR(ERR_INVALID_TRANSACTION);
        SetLastError(0);
        h = U(CreateFileTransactedW)(ws, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL, BADTX[i], NULL, NULL);
        EXPECT_EQ(h, INVALID_HANDLE_VALUE); EXPECT_ERR(ERR_INVALID_TRANSACTION);
    }
    CHECK(!fa_exists(s));
}
TEST(ftx_createfile_transacted_extended_parameter) {
    REQUIRE(U(CreateFileTransactedW));
    char s[MAX_PATH * 2]; fa_path(s, "tx_cfx.txt"); WCHAR ws[MAX_PATH * 2]; fa_widen(ws, s);
    int dummy = 0;
    SetLastError(0);
    HANDLE h = U(CreateFileTransactedW)(ws, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL, NULL, NULL, &dummy);
    EXPECT_EQ(h, INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CHECK(!fa_exists(s));
}
TEST(ftx_directory_transacted) {
    REQUIRE(U(CreateDirectoryTransactedW)); REQUIRE(U(RemoveDirectoryTransactedW));
    char s[MAX_PATH * 2]; fa_path(s, "tx_dir"); WCHAR ws[MAX_PATH * 2]; fa_widen(ws, s);
    for (int i = 0; i < 2; i++) {
        SetLastError(0); EXPECT_EQ(U(CreateDirectoryTransactedW)(NULL, ws, NULL, BADTX[i]), FALSE); EXPECT_ERR(ERR_INVALID_TRANSACTION);
    }
    CHECK(!fa_exists(s));
    fa_mkdir("tx_dir");
    for (int i = 0; i < 2; i++) {
        SetLastError(0); EXPECT_EQ(U(RemoveDirectoryTransactedW)(ws, BADTX[i]), FALSE); EXPECT_ERR(ERR_INVALID_TRANSACTION);
    }
    CHECK(fa_exists(s)); fa_rm(s);
    if (U(CreateDirectoryTransactedA) && U(RemoveDirectoryTransactedA)) {
        SetLastError(0); EXPECT_EQ(U(CreateDirectoryTransactedA)(NULL, s, NULL, NULL), FALSE); EXPECT_ERR(ERR_INVALID_TRANSACTION);
        SetLastError(0); EXPECT_EQ(U(RemoveDirectoryTransactedA)(s, NULL), FALSE); EXPECT_ERR(ERR_INVALID_TRANSACTION);
    }
}
TEST(ftx_movefile_with_progress_transacted_w_exported) {
    REQUIRE(t_proc_k32("MoveFileWithProgressTransactedW"));
    CHECK(t_proc_k32("MoveFileWithProgressTransactedW") != NULL);
}
TEST(ftx_bad_nonnull_handle_is_not_a_transaction) {
    REQUIRE(U(DeleteFileTransactedW));
    char s[MAX_PATH * 2]; fa_mkfile_s("tx_nt.txt", "x"); fa_path(s, "tx_nt.txt"); WCHAR ws[MAX_PATH * 2]; fa_widen(ws, s);
    HANDLE ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    SetLastError(0);
    BOOL r = U(DeleteFileTransactedW)(ws, ev);
    DWORD e = GetLastError();
    t_note("DeleteFileTransactedW with an event handle: r=%d err=%lu", r, e);
    CHECK(!r);
    CHECK(fa_exists(s));
    CloseHandle(ev); fa_rm(s);
}
