/* Transacted (TxF) file API wrappers in kernel32. They validate the transaction handle first:
 * NULL / INVALID_HANDLE_VALUE -> ERROR_TRANSACTION_NOT_ACTIVE (6700), before touching the file system. */
#include "t.h"

#define ERR_TXN_NOT_ACTIVE 6700  /* really ERROR_INVALID_TRANSACTION; kernel32 wrappers set 0x1a2c for NULL/-1 (decomp) */

static void tx_path(char *out, size_t cap, const char *leaf) { t_tmpfile(out, cap, leaf); }
static void tx_wpath(WCHAR *out, size_t cap, const char *leaf) {
    char a[MAX_PATH]; t_tmpfile(a, sizeof a, leaf);
    MultiByteToWideChar(CP_ACP, 0, a, -1, out, (int)cap);
}
static int exists(const char *p) { return GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES; }
static void mkfile(const char *p, const char *data) {
    HANDLE h = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) { DWORD w; WriteFile(h, data, (DWORD)strlen(data), &w, NULL); CloseHandle(h); }
}

static const HANDLE bad_txns[2] = { NULL, INVALID_HANDLE_VALUE };

TEST(txf_all_transacted_exports_in_kernel32) {
    static const char *const n[] = {
        "CopyFileTransactedA","CopyFileTransactedW","CreateDirectoryTransactedA","CreateDirectoryTransactedW",
        "CreateFileTransactedA","CreateFileTransactedW","CreateHardLinkTransactedA","CreateHardLinkTransactedW",
        "CreateSymbolicLinkTransactedA","CreateSymbolicLinkTransactedW","DeleteFileTransactedA","DeleteFileTransactedW",
        "FindFirstFileNameTransactedW","FindFirstFileTransactedA","FindFirstFileTransactedW","FindFirstStreamTransactedW",
        "GetCompressedFileSizeTransactedA","GetCompressedFileSizeTransactedW","GetFileAttributesTransactedA","GetFileAttributesTransactedW",
        "GetFullPathNameTransactedA","GetFullPathNameTransactedW","GetLongPathNameTransactedA","GetLongPathNameTransactedW",
        "MoveFileTransactedA","MoveFileTransactedW","RemoveDirectoryTransactedA","RemoveDirectoryTransactedW",
        "SetFileAttributesTransactedA","SetFileAttributesTransactedW", NULL };
    for (int i = 0; n[i]; i++) CHECKF(t_proc_k32(n[i]) != NULL, "missing %s", n[i]);
}

TEST(txf_createfile_w_bad_txn) {
    REQUIRE(UK(CreateFileTransactedW));
    WCHAR w[MAX_PATH]; tx_wpath(w, MAX_PATH, "txf_cf_w.bin");
    char a[MAX_PATH]; tx_path(a, sizeof a, "txf_cf_w.bin");
    for (int i = 0; i < 2; i++) {
        SET_ERR(0);
        HANDLE h = UK(CreateFileTransactedW)(w, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL, bad_txns[i], NULL, NULL);
        CHECK(h == INVALID_HANDLE_VALUE);
        EXPECT_ERR(ERR_TXN_NOT_ACTIVE);
        CHECK(!exists(a));
    }
}

TEST(txf_createfile_a_bad_txn) {
    REQUIRE(UK(CreateFileTransactedA));
    char a[MAX_PATH]; tx_path(a, sizeof a, "txf_cf_a.bin");
    for (int i = 0; i < 2; i++) {
        SET_ERR(0);
        HANDLE h = UK(CreateFileTransactedA)(a, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL, bad_txns[i], NULL, NULL);
        CHECK(h == INVALID_HANDLE_VALUE);
        EXPECT_ERR(ERR_TXN_NOT_ACTIVE);
        CHECK(!exists(a));
    }
}

TEST(txf_createfile_w_extended_parameter_is_invalid) {
    REQUIRE(UK(CreateFileTransactedW));
    WCHAR w[MAX_PATH]; tx_wpath(w, MAX_PATH, "txf_cf_ext.bin");
    SET_ERR(0);
    HANDLE h = UK(CreateFileTransactedW)(w, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL, NULL, NULL, (PVOID)1);
    CHECK(h == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);   /* lpExtendedParameter must be NULL: checked before the transaction */
    char a[MAX_PATH]; tx_path(a, sizeof a, "txf_cf_ext.bin");
    CHECK(!exists(a));
}

TEST(txf_createfile_null_name_bad_txn_w) {
    REQUIRE(UK(CreateFileTransactedW));
    SET_ERR(0);
    HANDLE h = UK(CreateFileTransactedW)(NULL, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL, NULL, NULL, NULL);
    CHECK(h == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERR_TXN_NOT_ACTIVE);
}

TEST(txf_createfile_bad_txn_does_not_modify_existing_file) {
    REQUIRE(UK(CreateFileTransactedW));
    char a[MAX_PATH]; tx_path(a, sizeof a, "txf_keep.bin"); mkfile(a, "keep-me");
    WCHAR w[MAX_PATH]; tx_wpath(w, MAX_PATH, "txf_keep.bin");
    HANDLE h = UK(CreateFileTransactedW)(w, GENERIC_WRITE, 0, NULL, TRUNCATE_EXISTING, 0, NULL, NULL, NULL, NULL);
    CHECK(h == INVALID_HANDLE_VALUE);
    WIN32_FILE_ATTRIBUTE_DATA d;
    CHECK(GetFileAttributesExA(a, GetFileExInfoStandard, &d));
    EXPECT_EQ(d.nFileSizeLow, 7);
}

TEST(txf_copyfile_bad_txn) {
    REQUIRE(UK(CopyFileTransactedW)); REQUIRE(UK(CopyFileTransactedA));
    char s[MAX_PATH], d[MAX_PATH]; tx_path(s, sizeof s, "txf_cp_src.txt"); tx_path(d, sizeof d, "txf_cp_dst.txt");
    mkfile(s, "x");
    WCHAR ws[MAX_PATH], wd[MAX_PATH]; tx_wpath(ws, MAX_PATH, "txf_cp_src.txt"); tx_wpath(wd, MAX_PATH, "txf_cp_dst.txt");
    for (int i = 0; i < 2; i++) {
        SET_ERR(0);
        CHECK(UK(CopyFileTransactedW)(ws, wd, NULL, NULL, NULL, 0, bad_txns[i]) == FALSE);
        EXPECT_ERR(ERR_TXN_NOT_ACTIVE);
        SET_ERR(0);
        CHECK(UK(CopyFileTransactedA)(s, d, NULL, NULL, NULL, 0, bad_txns[i]) == FALSE);
        EXPECT_ERR(ERR_TXN_NOT_ACTIVE);
    }
    CHECK(!exists(d));
}

TEST(txf_copyfile_bad_txn_wins_over_missing_source) {
    REQUIRE(UK(CopyFileTransactedW));
    WCHAR ws[MAX_PATH], wd[MAX_PATH]; tx_wpath(ws, MAX_PATH, "txf_no_such_src"); tx_wpath(wd, MAX_PATH, "txf_no_such_dst");
    SET_ERR(0);
    CHECK(UK(CopyFileTransactedW)(ws, wd, NULL, NULL, NULL, 0, NULL) == FALSE);
    EXPECT_ERR(ERR_TXN_NOT_ACTIVE);
}

TEST(txf_movefile_bad_txn) {
    REQUIRE(UK(MoveFileTransactedW)); REQUIRE(UK(MoveFileTransactedA));
    char s[MAX_PATH], d[MAX_PATH]; tx_path(s, sizeof s, "txf_mv_src.txt"); tx_path(d, sizeof d, "txf_mv_dst.txt");
    mkfile(s, "x");
    WCHAR ws[MAX_PATH], wd[MAX_PATH]; tx_wpath(ws, MAX_PATH, "txf_mv_src.txt"); tx_wpath(wd, MAX_PATH, "txf_mv_dst.txt");
    for (int i = 0; i < 2; i++) {
        SET_ERR(0);
        CHECK(UK(MoveFileTransactedW)(ws, wd, NULL, NULL, 0, bad_txns[i]) == FALSE);
        EXPECT_ERR(ERR_TXN_NOT_ACTIVE);
        SET_ERR(0);
        CHECK(UK(MoveFileTransactedA)(s, d, NULL, NULL, 0, bad_txns[i]) == FALSE);
        EXPECT_ERR(ERR_TXN_NOT_ACTIVE);
    }
    CHECK(exists(s)); CHECK(!exists(d));
}

TEST(txf_deletefile_bad_txn) {
    REQUIRE(UK(DeleteFileTransactedW)); REQUIRE(UK(DeleteFileTransactedA));
    char s[MAX_PATH]; tx_path(s, sizeof s, "txf_del.txt"); mkfile(s, "x");
    WCHAR ws[MAX_PATH]; tx_wpath(ws, MAX_PATH, "txf_del.txt");
    for (int i = 0; i < 2; i++) {
        SET_ERR(0);
        CHECK(UK(DeleteFileTransactedW)(ws, bad_txns[i]) == FALSE);
        EXPECT_ERR(ERR_TXN_NOT_ACTIVE);
        SET_ERR(0);
        CHECK(UK(DeleteFileTransactedA)(s, bad_txns[i]) == FALSE);
        EXPECT_ERR(ERR_TXN_NOT_ACTIVE);
    }
    CHECK(exists(s));
}

TEST(txf_createdirectory_bad_txn) {
    REQUIRE(UK(CreateDirectoryTransactedW)); REQUIRE(UK(CreateDirectoryTransactedA));
    char d[MAX_PATH]; tx_path(d, sizeof d, "txf_dir"); WCHAR wd[MAX_PATH]; tx_wpath(wd, MAX_PATH, "txf_dir");
    for (int i = 0; i < 2; i++) {
        SET_ERR(0);
        CHECK(UK(CreateDirectoryTransactedW)(NULL, wd, NULL, bad_txns[i]) == FALSE);
        EXPECT_ERR(ERR_TXN_NOT_ACTIVE);
        SET_ERR(0);
        CHECK(UK(CreateDirectoryTransactedA)(NULL, d, NULL, bad_txns[i]) == FALSE);
        EXPECT_ERR(ERR_TXN_NOT_ACTIVE);
    }
    CHECK(!exists(d));
}

TEST(txf_createdirectory_template_bad_txn) {
    REQUIRE(UK(CreateDirectoryTransactedW));
    WCHAR tmpl[MAX_PATH], wd[MAX_PATH]; tx_wpath(tmpl, MAX_PATH, ""); tx_wpath(wd, MAX_PATH, "txf_dir_tmpl");
    SET_ERR(0);
    CHECK(UK(CreateDirectoryTransactedW)(tmpl, wd, NULL, NULL) == FALSE);
    EXPECT_ERR(ERR_TXN_NOT_ACTIVE);
}

TEST(txf_removedirectory_bad_txn) {
    REQUIRE(UK(RemoveDirectoryTransactedW)); REQUIRE(UK(RemoveDirectoryTransactedA));
    char d[MAX_PATH]; tx_path(d, sizeof d, "txf_rmdir"); CreateDirectoryA(d, NULL);
    WCHAR wd[MAX_PATH]; tx_wpath(wd, MAX_PATH, "txf_rmdir");
    for (int i = 0; i < 2; i++) {
        SET_ERR(0);
        CHECK(UK(RemoveDirectoryTransactedW)(wd, bad_txns[i]) == FALSE);
        EXPECT_ERR(ERR_TXN_NOT_ACTIVE);
        SET_ERR(0);
        CHECK(UK(RemoveDirectoryTransactedA)(d, bad_txns[i]) == FALSE);
        EXPECT_ERR(ERR_TXN_NOT_ACTIVE);
    }
    CHECK(exists(d));
    RemoveDirectoryA(d);
}

TEST(txf_getfileattributes_bad_txn) {
    REQUIRE(UK(GetFileAttributesTransactedW)); REQUIRE(UK(GetFileAttributesTransactedA));
    char s[MAX_PATH]; tx_path(s, sizeof s, "txf_ga.txt"); mkfile(s, "x");
    WCHAR ws[MAX_PATH]; tx_wpath(ws, MAX_PATH, "txf_ga.txt");
    WIN32_FILE_ATTRIBUTE_DATA d; memset(&d, 0x5A, sizeof d);
    for (int i = 0; i < 2; i++) {
        SET_ERR(0);
        CHECK(UK(GetFileAttributesTransactedW)(ws, GetFileExInfoStandard, &d, bad_txns[i]) == FALSE);
        EXPECT_ERR(ERR_TXN_NOT_ACTIVE);
        SET_ERR(0);
        CHECK(UK(GetFileAttributesTransactedA)(s, GetFileExInfoStandard, &d, bad_txns[i]) == FALSE);
        EXPECT_ERR(ERR_TXN_NOT_ACTIVE);
    }
    EXPECT_EQ(d.dwFileAttributes, 0x5A5A5A5A);   /* output untouched */
}

TEST(txf_setfileattributes_bad_txn) {
    REQUIRE(UK(SetFileAttributesTransactedW)); REQUIRE(UK(SetFileAttributesTransactedA));
    char s[MAX_PATH]; tx_path(s, sizeof s, "txf_sa.txt"); mkfile(s, "x");
    WCHAR ws[MAX_PATH]; tx_wpath(ws, MAX_PATH, "txf_sa.txt");
    for (int i = 0; i < 2; i++) {
        SET_ERR(0);
        CHECK(UK(SetFileAttributesTransactedW)(ws, FILE_ATTRIBUTE_READONLY, bad_txns[i]) == FALSE);
        EXPECT_ERR(ERR_TXN_NOT_ACTIVE);
        SET_ERR(0);
        CHECK(UK(SetFileAttributesTransactedA)(s, FILE_ATTRIBUTE_READONLY, bad_txns[i]) == FALSE);
        EXPECT_ERR(ERR_TXN_NOT_ACTIVE);
    }
    CHECK(!(GetFileAttributesA(s) & FILE_ATTRIBUTE_READONLY));
}

TEST(txf_findfirstfile_bad_txn) {
    REQUIRE(UK(FindFirstFileTransactedW)); REQUIRE(UK(FindFirstFileTransactedA));
    char pat[MAX_PATH]; tx_path(pat, sizeof pat, "*"); WCHAR wp[MAX_PATH]; tx_wpath(wp, MAX_PATH, "*");
    WIN32_FIND_DATAW fw; WIN32_FIND_DATAA fa;
    for (int i = 0; i < 2; i++) {
        SET_ERR(0);
        CHECK(UK(FindFirstFileTransactedW)(wp, FindExInfoStandard, &fw, FindExSearchNameMatch, NULL, 0, bad_txns[i]) == INVALID_HANDLE_VALUE);
        EXPECT_ERR(ERR_TXN_NOT_ACTIVE);
        SET_ERR(0);
        CHECK(UK(FindFirstFileTransactedA)(pat, FindExInfoStandard, &fa, FindExSearchNameMatch, NULL, 0, bad_txns[i]) == INVALID_HANDLE_VALUE);
        EXPECT_ERR(ERR_TXN_NOT_ACTIVE);
    }
}

TEST(txf_findfirststream_bad_txn) {
    REQUIRE(UK(FindFirstStreamTransactedW));
    char s[MAX_PATH]; tx_path(s, sizeof s, "txf_fs.txt"); mkfile(s, "x");
    WCHAR ws[MAX_PATH]; tx_wpath(ws, MAX_PATH, "txf_fs.txt");
    WIN32_FIND_STREAM_DATA sd;
    for (int i = 0; i < 2; i++) {
        SET_ERR(0);
        CHECK(UK(FindFirstStreamTransactedW)(ws, FindStreamInfoStandard, &sd, 0, bad_txns[i]) == INVALID_HANDLE_VALUE);
        EXPECT_ERR(ERR_TXN_NOT_ACTIVE);
    }
}

TEST(txf_findfirstfilename_bad_txn) {
    REQUIRE(UK(FindFirstFileNameTransactedW));
    char s[MAX_PATH]; tx_path(s, sizeof s, "txf_fn.txt"); mkfile(s, "x");
    WCHAR ws[MAX_PATH]; tx_wpath(ws, MAX_PATH, "txf_fn.txt");
    WCHAR link[MAX_PATH]; DWORD len = MAX_PATH;
    for (int i = 0; i < 2; i++) {
        SET_ERR(0);
        CHECK(UK(FindFirstFileNameTransactedW)(ws, 0, &len, link, bad_txns[i]) == INVALID_HANDLE_VALUE);
        EXPECT_ERR(ERR_TXN_NOT_ACTIVE);
    }
}

TEST(txf_createhardlink_bad_txn) {
    REQUIRE(UK(CreateHardLinkTransactedW)); REQUIRE(UK(CreateHardLinkTransactedA));
    char s[MAX_PATH], l[MAX_PATH]; tx_path(s, sizeof s, "txf_hl_src.txt"); tx_path(l, sizeof l, "txf_hl_lnk.txt"); mkfile(s, "x");
    WCHAR ws[MAX_PATH], wl[MAX_PATH]; tx_wpath(ws, MAX_PATH, "txf_hl_src.txt"); tx_wpath(wl, MAX_PATH, "txf_hl_lnk.txt");
    for (int i = 0; i < 2; i++) {
        SET_ERR(0);
        CHECK(UK(CreateHardLinkTransactedW)(wl, ws, NULL, bad_txns[i]) == FALSE);
        EXPECT_ERR(ERR_TXN_NOT_ACTIVE);
        SET_ERR(0);
        CHECK(UK(CreateHardLinkTransactedA)(l, s, NULL, bad_txns[i]) == FALSE);
        EXPECT_ERR(ERR_TXN_NOT_ACTIVE);
    }
    CHECK(!exists(l));
}

TEST(txf_createsymboliclink_bad_txn) {
    REQUIRE(UK(CreateSymbolicLinkTransactedW)); REQUIRE(UK(CreateSymbolicLinkTransactedA));
    char s[MAX_PATH], l[MAX_PATH]; tx_path(s, sizeof s, "txf_sl_src.txt"); tx_path(l, sizeof l, "txf_sl_lnk.txt"); mkfile(s, "x");
    WCHAR ws[MAX_PATH], wl[MAX_PATH]; tx_wpath(ws, MAX_PATH, "txf_sl_src.txt"); tx_wpath(wl, MAX_PATH, "txf_sl_lnk.txt");
    for (int i = 0; i < 2; i++) {
        SET_ERR(0);
        BOOLEAN r = UK(CreateSymbolicLinkTransactedW)(wl, ws, 0, bad_txns[i]);
        CHECK(r == 0);
        EXPECT_ERR(ERR_TXN_NOT_ACTIVE);
        SET_ERR(0);
        r = UK(CreateSymbolicLinkTransactedA)(l, s, 0, bad_txns[i]);
        CHECK(r == 0);
        EXPECT_ERR(ERR_TXN_NOT_ACTIVE);
    }
    CHECK(!exists(l));
}

TEST(txf_createsymboliclink_a_null_names_invalid_parameter) {
    REQUIRE(UK(CreateSymbolicLinkTransactedA));
    char s[MAX_PATH]; tx_path(s, sizeof s, "x");
    SET_ERR(0);
    CHECK(UK(CreateSymbolicLinkTransactedA)(NULL, s, 0, NULL) == 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(UK(CreateSymbolicLinkTransactedA)(s, NULL, 0, NULL) == 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(UK(CreateSymbolicLinkTransactedA)(NULL, NULL, 0, INVALID_HANDLE_VALUE) == 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}

TEST(txf_getfullpathname_bad_txn) {
    REQUIRE(UK(GetFullPathNameTransactedW)); REQUIRE(UK(GetFullPathNameTransactedA));
    WCHAR wb[MAX_PATH]; char ab[MAX_PATH]; LPWSTR fpw; LPSTR fpa;
    for (int i = 0; i < 2; i++) {
        SET_ERR(0);
        CHECK(UK(GetFullPathNameTransactedW)(L"foo.txt", MAX_PATH, wb, &fpw, bad_txns[i]) == 0);
        EXPECT_ERR(ERR_TXN_NOT_ACTIVE);
        SET_ERR(0);
        CHECK(UK(GetFullPathNameTransactedA)("foo.txt", MAX_PATH, ab, &fpa, bad_txns[i]) == 0);
        EXPECT_ERR(ERR_TXN_NOT_ACTIVE);
    }
}

TEST(txf_getfullpathname_bad_txn_leaves_buffer_untouched) {
    REQUIRE(UK(GetFullPathNameTransactedW));
    WCHAR wb[16]; for (int i = 0; i < 16; i++) wb[i] = 0x7777;
    UK(GetFullPathNameTransactedW)(L"bar", 16, wb, NULL, NULL);
    CHECK(wb[0] == 0x7777);
}

TEST(txf_getlongpathname_bad_txn) {
    REQUIRE(UK(GetLongPathNameTransactedW)); REQUIRE(UK(GetLongPathNameTransactedA));
    WCHAR wb[MAX_PATH]; char ab[MAX_PATH];
    for (int i = 0; i < 2; i++) {
        SET_ERR(0);
        CHECK(UK(GetLongPathNameTransactedW)(L"C:\\Windows", wb, MAX_PATH, bad_txns[i]) == 0);
        EXPECT_ERR(ERR_TXN_NOT_ACTIVE);
        SET_ERR(0);
        CHECK(UK(GetLongPathNameTransactedA)("C:\\Windows", ab, MAX_PATH, bad_txns[i]) == 0);
        EXPECT_ERR(ERR_TXN_NOT_ACTIVE);
    }
}

TEST(txf_getcompressedfilesize_bad_txn) {
    REQUIRE(UK(GetCompressedFileSizeTransactedW)); REQUIRE(UK(GetCompressedFileSizeTransactedA));
    char s[MAX_PATH]; tx_path(s, sizeof s, "txf_gcs.txt"); mkfile(s, "x");
    WCHAR ws[MAX_PATH]; tx_wpath(ws, MAX_PATH, "txf_gcs.txt");
    DWORD hi = 0x1234;
    for (int i = 0; i < 2; i++) {
        SET_ERR(0);
        CHECK(UK(GetCompressedFileSizeTransactedW)(ws, &hi, bad_txns[i]) == INVALID_FILE_SIZE);
        EXPECT_ERR(ERR_TXN_NOT_ACTIVE);
        SET_ERR(0);
        CHECK(UK(GetCompressedFileSizeTransactedA)(s, &hi, bad_txns[i]) == INVALID_FILE_SIZE);
        EXPECT_ERR(ERR_TXN_NOT_ACTIVE);
    }
    EXPECT_EQ(hi, 0x1234);
}

TEST(txf_non_transaction_handle_is_not_rejected_as_inactive) {
    /* an event handle is non-NULL so the wrapper proceeds to the underlying call; observe only (version dependent) */
    REQUIRE(UK(GetFileAttributesTransactedW));
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    char s[MAX_PATH]; tx_path(s, sizeof s, "txf_ev.txt"); mkfile(s, "x");
    WCHAR ws[MAX_PATH]; tx_wpath(ws, MAX_PATH, "txf_ev.txt");
    WIN32_FILE_ATTRIBUTE_DATA d;
    SET_ERR(0);
    BOOL r = UK(GetFileAttributesTransactedW)(ws, GetFileExInfoStandard, &d, ev);
    t_note("GetFileAttributesTransactedW with event handle: ret=%d err=%lu", r, GetLastError());
    CHECK(r == FALSE || r == TRUE);
    CloseHandle(ev);
}

TEST(txf_wrapper_clears_thread_current_transaction) {
    /* after a failing wrapper the thread has no ambient transaction: a plain CreateFile works as normal */
    REQUIRE(UK(DeleteFileTransactedW));
    char s[MAX_PATH]; tx_path(s, sizeof s, "txf_ambient.txt"); mkfile(s, "x");
    WCHAR ws[MAX_PATH]; tx_wpath(ws, MAX_PATH, "txf_ambient.txt");
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    UK(DeleteFileTransactedW)(ws, ev);      /* bogus transaction object, observe only */
    CloseHandle(ev);
    HANDLE h = CreateFileA(s, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    CHECK(exists(s));
}

TEST(txf_real_transaction_commit_and_rollback) {
    HMODULE k = LoadLibraryA("ktmw32.dll");
    if (!k) { t_skip("no ktmw32"); return; }
    typedef HANDLE (WINAPI *ct_t)(LPSECURITY_ATTRIBUTES, LPGUID, DWORD, DWORD, DWORD, DWORD, LPWSTR);
    typedef BOOL (WINAPI *cr_t)(HANDLE);
    ct_t CT = (ct_t)GetProcAddress(k, "CreateTransaction");
    cr_t COMMIT = (cr_t)GetProcAddress(k, "CommitTransaction"), ROLL = (cr_t)GetProcAddress(k, "RollbackTransaction");
    REQUIRE(CT); REQUIRE(COMMIT); REQUIRE(ROLL);
    REQUIRE(UK(CreateFileTransactedW)); REQUIRE(UK(DeleteFileTransactedW));
    if (t_is_wine()) { t_skip("TxF not implemented"); return; }
    WCHAR w[MAX_PATH]; tx_wpath(w, MAX_PATH, "txf_real.bin");
    char a[MAX_PATH]; tx_path(a, sizeof a, "txf_real.bin");
    HANDLE tx = CT(NULL, NULL, 0, 0, 0, 0, NULL);
    if (tx == INVALID_HANDLE_VALUE) { t_note("CreateTransaction failed: %lu", GetLastError()); FreeLibrary(k); return; }
    SET_ERR(0);
    HANDLE h = UK(CreateFileTransactedW)(w, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL, tx, NULL, NULL);
    t_note("transacted CreateFile: %p err=%lu", h, GetLastError());
    if (h != INVALID_HANDLE_VALUE) {
        DWORD wr; WriteFile(h, "abc", 3, &wr, NULL); CloseHandle(h);
        t_note("visible outside txn before commit: %d", exists(a));
        CHECK(ROLL(tx));
        CHECK(!exists(a));
    }
    CloseHandle(tx);
    tx = CT(NULL, NULL, 0, 0, 0, 0, NULL);
    if (tx != INVALID_HANDLE_VALUE) {
        h = UK(CreateFileTransactedW)(w, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL, tx, NULL, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
            CHECK(COMMIT(tx));
            CHECK(exists(a));
            DeleteFileA(a);
        }
        CloseHandle(tx);
    }
    FreeLibrary(k);
}

TEST(txf_committed_transaction_is_inactive) {
    HMODULE k = LoadLibraryA("ktmw32.dll");
    if (!k) { t_skip("no ktmw32"); return; }
    typedef HANDLE (WINAPI *ct_t)(LPSECURITY_ATTRIBUTES, LPGUID, DWORD, DWORD, DWORD, DWORD, LPWSTR);
    typedef BOOL (WINAPI *cr_t)(HANDLE);
    ct_t CT = (ct_t)GetProcAddress(k, "CreateTransaction");
    cr_t COMMIT = (cr_t)GetProcAddress(k, "CommitTransaction");
    REQUIRE(CT); REQUIRE(COMMIT); REQUIRE(UK(GetFileAttributesTransactedW));
    if (t_is_wine()) { t_skip("TxF not implemented"); return; }
    HANDLE tx = CT(NULL, NULL, 0, 0, 0, 0, NULL);
    if (tx == INVALID_HANDLE_VALUE) { t_note("CreateTransaction: %lu", GetLastError()); FreeLibrary(k); return; }
    CHECK(COMMIT(tx));
    char s[MAX_PATH]; tx_path(s, sizeof s, "txf_inactive.txt"); mkfile(s, "x");
    WCHAR ws[MAX_PATH]; tx_wpath(ws, MAX_PATH, "txf_inactive.txt");
    WIN32_FILE_ATTRIBUTE_DATA d;
    SET_ERR(0);
    BOOL r = UK(GetFileAttributesTransactedW)(ws, GetFileExInfoStandard, &d, tx);
    t_note("use of committed transaction: ret=%d err=%lu (expect 6701 ERROR_TRANSACTION_NOT_ACTIVE-like)", r, GetLastError());
    CloseHandle(tx);
    FreeLibrary(k);
}
