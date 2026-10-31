#include "g18.h"
#include <winioctl.h>
/* gap18: *Transacted* wrappers, GetFileAttributesExA, OpenFileMappingW. */

UDECL(PVOID, RtlGetCurrentTransaction, (void));
UDECL(PVOID, RtlSetCurrentTransaction, (PVOID));
#define ERROR_INVALID_TRANSACTION_ 6700
typedef HANDLE (WINAPI *CreateTransaction_t)(LPSECURITY_ATTRIBUTES, LPGUID, DWORD, DWORD, DWORD, DWORD, LPWSTR);
typedef BOOL (WINAPI *RollbackTransaction_t)(HANDLE);

static HANDLE g18_txn(void) {
    if (t_is_wine()) return NULL;
    static HMODULE k; if (!k) k = LoadLibraryA("ktmw32.dll"); if (!k) return NULL;
    CreateTransaction_t ct = (CreateTransaction_t)GetProcAddress(k, "CreateTransaction"); if (!ct) return NULL;
    HANDLE h = ct(NULL, NULL, 0, 0, 0, 0, L"k32t gap18");
    return h == INVALID_HANDLE_VALUE ? NULL : h;
}
static void g18_txn_done(HANDLE h) {
    HMODULE k = GetModuleHandleA("ktmw32.dll");
    RollbackTransaction_t rb = k ? (RollbackTransaction_t)GetProcAddress(k, "RollbackTransaction") : NULL;
    if (h) { if (rb) rb(h); CloseHandle(h); }
}

/* ---------- invalid transaction handles (NULL and INVALID_HANDLE_VALUE are rejected up front) ---------- */
TEST(gap18_ffft_invalid_transaction) {
    REQUIRE(U(FindFirstFileTransactedW));
    WCHAR p[MAX_PATH]; char a[MAX_PATH]; g18_path(a, MAX_PATH, "*"); g18_widen(p, MAX_PATH, a);
    WIN32_FIND_DATAW fd;
    HANDLE bad[2] = { NULL, INVALID_HANDLE_VALUE };
    for (int i = 0; i < 2; i++) {
        memset(&fd, 0xCC, sizeof fd);
        SET_ERR(0);
        HANDLE h = U(FindFirstFileTransactedW)(p, FindExInfoStandard, &fd, FindExSearchNameMatch, NULL, 0, bad[i]);
        CHECK(h == INVALID_HANDLE_VALUE);
        EXPECT_ERR(ERROR_INVALID_TRANSACTION_);
        CHECK(fd.cFileName[0] == 0xCCCC);          /* output untouched */
    }
}
TEST(gap18_ffft_invalid_transaction_precedes_arg_checks) {
    REQUIRE(U(FindFirstFileTransactedW));
    WIN32_FIND_DATAW fd;
    SET_ERR(0);
    CHECK(U(FindFirstFileTransactedW)(NULL, FindExInfoStandard, &fd, FindExSearchNameMatch, NULL, 0, NULL) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_INVALID_TRANSACTION_);                /* transaction is validated before the name is looked at */
    SET_ERR(0);
    CHECK(U(FindFirstFileTransactedW)(L"C:\\", (FINDEX_INFO_LEVELS)77, &fd, FindExSearchNameMatch, NULL, 0, NULL) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_INVALID_TRANSACTION_);
}
TEST(gap18_ffst_invalid_transaction) {
    REQUIRE(U(FindFirstStreamTransactedW));
    WCHAR p[MAX_PATH]; char a[MAX_PATH]; g18_path(a, MAX_PATH, "stream_t.txt"); g18_put(a, "x", 1); g18_widen(p, MAX_PATH, a);
    WIN32_FIND_STREAM_DATA sd;
    HANDLE bad[2] = { NULL, INVALID_HANDLE_VALUE };
    for (int i = 0; i < 2; i++) {
        SET_ERR(0);
        HANDLE h = U(FindFirstStreamTransactedW)(p, FindStreamInfoStandard, &sd, 0, bad[i]);
        CHECK(h == INVALID_HANDLE_VALUE);
        EXPECT_ERR(ERROR_INVALID_TRANSACTION_);
    }
    g18_rm(a);
}
TEST(gap18_ffnt_invalid_transaction) {
    REQUIRE(U(FindFirstFileNameTransactedW));
    WCHAR p[MAX_PATH]; char a[MAX_PATH]; g18_path(a, MAX_PATH, "fname_t.txt"); g18_put(a, "x", 1); g18_widen(p, MAX_PATH, a);
    WCHAR buf[MAX_PATH]; DWORD len = MAX_PATH;
    HANDLE bad[2] = { NULL, INVALID_HANDLE_VALUE };
    for (int i = 0; i < 2; i++) {
        len = MAX_PATH;
        SET_ERR(0);
        HANDLE h = U(FindFirstFileNameTransactedW)(p, 0, &len, buf, bad[i]);
        CHECK(h == INVALID_HANDLE_VALUE);
        EXPECT_ERR(ERROR_INVALID_TRANSACTION_);
        EQ(len, MAX_PATH);
    }
    g18_rm(a);
}
TEST(gap18_glpnt_invalid_transaction) {
    REQUIRE(U(GetLongPathNameTransactedA));
    char buf[MAX_PATH]; memset(buf, 'Z', sizeof buf);
    HANDLE bad[2] = { NULL, INVALID_HANDLE_VALUE };
    for (int i = 0; i < 2; i++) {
        SET_ERR(0);
        EQ(U(GetLongPathNameTransactedA)(t_tmpdir(), buf, MAX_PATH, bad[i]), 0);
        EXPECT_ERR(ERROR_INVALID_TRANSACTION_);
        CHECK(buf[0] == 'Z');
    }
    SET_ERR(0);
    EQ(U(GetLongPathNameTransactedA)(NULL, NULL, 0, NULL), 0);
    EXPECT_ERR(ERROR_INVALID_TRANSACTION_);
}
TEST(gap18_transacted_current_transaction_conflict) {
    REQUIRE(U(FindFirstFileTransactedW)); REQUIRE(U(FindFirstStreamTransactedW)); REQUIRE(U(FindFirstFileNameTransactedW)); REQUIRE(U(GetLongPathNameTransactedA));
    REQUIRE(UN(RtlSetCurrentTransaction));
    /* A thread that already has a current transaction cannot start a second one: rejected up front,
       without ever touching the (fake) handles. The marker is cleared again right away. */
    WIN32_FIND_DATAW fd; WIN32_FIND_STREAM_DATA sd; WCHAR nb[MAX_PATH]; DWORD nl = MAX_PATH; char lb[MAX_PATH];
    __typeof__(&FindFirstFileTransactedW) f1 = U(FindFirstFileTransactedW);
    __typeof__(&FindFirstStreamTransactedW) f2 = U(FindFirstStreamTransactedW);
    __typeof__(&FindFirstFileNameTransactedW) f3 = U(FindFirstFileNameTransactedW);
    __typeof__(&GetLongPathNameTransactedA) f4 = U(GetLongPathNameTransactedA);
    RtlSetCurrentTransaction_pfn setT = UN(RtlSetCurrentTransaction); RtlGetCurrentTransaction_pfn getT = UN(RtlGetCurrentTransaction);
    setT((PVOID)(ULONG_PTR)0x4);
    SetLastError(0);
    HANDLE h1 = f1(L"C:\\", FindExInfoStandard, &fd, FindExSearchNameMatch, NULL, 0, (HANDLE)(ULONG_PTR)0x8);
    DWORD e1 = GetLastError();
    SetLastError(0);
    HANDLE h2 = f2(L"C:\\x", FindStreamInfoStandard, &sd, 0, (HANDLE)(ULONG_PTR)0x8);
    DWORD e2 = GetLastError();
    SetLastError(0);
    HANDLE h3 = f3(L"C:\\x", 0, &nl, nb, (HANDLE)(ULONG_PTR)0x8);
    DWORD e3 = GetLastError();
    SetLastError(0);
    DWORD r4 = f4("C:\\x", lb, MAX_PATH, (HANDLE)(ULONG_PTR)0x8);
    DWORD e4 = GetLastError();
    PVOID cur = getT ? getT() : NULL;
    setT(NULL);
    CHECK(h1 == INVALID_HANDLE_VALUE); CHECK(h2 == INVALID_HANDLE_VALUE); CHECK(h3 == INVALID_HANDLE_VALUE); EQ(r4, 0);
    EQ(e1, 0x1a45); EQ(e2, 0x1a45); EQ(e3, 0x1a45); EQ(e4, 0x1a45);
    EQ(cur, 0x4);                 /* the pre-existing marker is left alone */
    if (getT) EQ(getT(), NULL);
}
TEST(gap18_transacted_marker_reset_after_call) {
    REQUIRE(U(GetLongPathNameTransactedA)); REQUIRE(UN(RtlGetCurrentTransaction));
    /* A bogus, non-NULL handle makes the underlying call fail; the thread's transaction marker must be cleared afterwards. */
    char buf[MAX_PATH];
    SET_ERR(0);
    DWORD r = U(GetLongPathNameTransactedA)(t_tmpdir(), buf, MAX_PATH, (HANDLE)(ULONG_PTR)0x7FF4);
    t_note("bogus txn handle -> %lu err %lu", r, GetLastError());
    EQ(UN(RtlGetCurrentTransaction)(), NULL);
}
TEST(gap18_ffft_bogus_handle_marker_reset_and_result) {
    REQUIRE(U(FindFirstFileTransactedW)); REQUIRE(UN(RtlGetCurrentTransaction));
    WIN32_FIND_DATAW fd; WCHAR p[MAX_PATH]; g18_widen(p, MAX_PATH, t_tmpdir());
    SET_ERR(0);
    HANDLE h = U(FindFirstFileTransactedW)(p, FindExInfoStandard, &fd, FindExSearchNameMatch, NULL, 0, (HANDLE)(ULONG_PTR)0x7FF4);
    t_note("bogus txn FindFirstFile -> %p err %lu", h, GetLastError());
    if (h != INVALID_HANDLE_VALUE) FindClose(h);
    EQ(UN(RtlGetCurrentTransaction)(), NULL);
}

/* ---------- with a real (rolled-back, read-only) transaction ---------- */
TEST(gap18_ffft_with_real_transaction) {
    REQUIRE(U(FindFirstFileTransactedW));
    HANDLE tx = g18_txn(); if (!tx) { t_skip("no transaction support"); return; }
    char a[MAX_PATH], b[MAX_PATH]; g18_path(a, MAX_PATH, "tx_one.txt"); g18_path(b, MAX_PATH, "tx_two.txt"); g18_put(a, "1", 1); g18_put(b, "22", 2);
    char pat[MAX_PATH]; g18_path(pat, MAX_PATH, "tx_*.txt"); WCHAR wp[MAX_PATH]; g18_widen(wp, MAX_PATH, pat);
    WIN32_FIND_DATAW fd; int n = 0; DWORD sizes = 0;
    SET_ERR(0);
    HANDLE h = U(FindFirstFileTransactedW)(wp, FindExInfoStandard, &fd, FindExSearchNameMatch, NULL, 0, tx);
    CHECKF(h != INVALID_HANDLE_VALUE, "err %lu", GetLastError());
    if (h != INVALID_HANDLE_VALUE) {
        do { n++; sizes += fd.nFileSizeLow; } while (FindNextFileW(h, &fd));
        FindClose(h);
        EQ(n, 2); EQ(sizes, 3);
        EQ(UN(RtlGetCurrentTransaction) ? UN(RtlGetCurrentTransaction)() : NULL, NULL);
    }
    /* no match */
    char np[MAX_PATH]; g18_path(np, MAX_PATH, "tx_nomatch_*.zzz"); g18_widen(wp, MAX_PATH, np);
    SET_ERR(0);
    CHECK(U(FindFirstFileTransactedW)(wp, FindExInfoStandard, &fd, FindExSearchNameMatch, NULL, 0, tx) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    /* invalid info level / search op still validated by the underlying call */
    SET_ERR(0);
    CHECK(U(FindFirstFileTransactedW)(wp, (FINDEX_INFO_LEVELS)2, &fd, FindExSearchNameMatch, NULL, 0, tx) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(U(FindFirstFileTransactedW)(wp, FindExInfoStandard, &fd, (FINDEX_SEARCH_OPS)9, NULL, 0, tx) == INVALID_HANDLE_VALUE);
    t_note("search op 9 err %lu", GetLastError());
    g18_txn_done(tx); g18_rm(a); g18_rm(b);
}
TEST(gap18_ffft_basic_info_level_with_tx) {
    REQUIRE(U(FindFirstFileTransactedW));
    HANDLE tx = g18_txn(); if (!tx) { t_skip("no transaction support"); return; }
    char a[MAX_PATH]; g18_path(a, MAX_PATH, "txb_file.bin"); g18_put(a, "abcd", 4);
    WCHAR wp[MAX_PATH]; g18_widen(wp, MAX_PATH, a);
    WIN32_FIND_DATAW fd; memset(&fd, 0, sizeof fd);
    HANDLE h = U(FindFirstFileTransactedW)(wp, FindExInfoBasic, &fd, FindExSearchNameMatch, NULL, 0, tx);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) {
        EQ(fd.nFileSizeLow, 4);
        CHECK(!lstrcmpiW(fd.cFileName, L"txb_file.bin"));
        CHECK(fd.cAlternateFileName[0] == 0);       /* FindExInfoBasic skips the short name */
        FindClose(h);
    }
    g18_txn_done(tx); g18_rm(a);
}
TEST(gap18_ffst_with_real_transaction) {
    REQUIRE(U(FindFirstStreamTransactedW));
    HANDLE tx = g18_txn(); if (!tx) { t_skip("no transaction support"); return; }
    char a[MAX_PATH], s[MAX_PATH + 8]; g18_path(a, MAX_PATH, "txs_file.txt"); g18_put(a, "main", 4);
    snprintf(s, sizeof s, "%s:alt", a); HANDLE hs = CreateFileA(s, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (hs != INVALID_HANDLE_VALUE) { DWORD w; WriteFile(hs, "alt!", 4, &w, NULL); CloseHandle(hs); }
    WCHAR wp[MAX_PATH]; g18_widen(wp, MAX_PATH, a);
    WIN32_FIND_STREAM_DATA sd; int n = 0, saw_main = 0, saw_alt = 0;
    HANDLE h = U(FindFirstStreamTransactedW)(wp, FindStreamInfoStandard, &sd, 0, tx);
    CHECKF(h != INVALID_HANDLE_VALUE, "err %lu", GetLastError());
    if (h != INVALID_HANDLE_VALUE) {
        do { n++; if (!lstrcmpW(sd.cStreamName, L"::$DATA")) { saw_main = 1; EQ(sd.StreamSize.QuadPart, 4); }
             if (!lstrcmpW(sd.cStreamName, L":alt:$DATA")) saw_alt = 1; } while (FindNextStreamW(h, &sd));
        FindClose(h);
        CHECK(saw_main);
        if (hs != INVALID_HANDLE_VALUE) CHECK(saw_alt);
    }
    SET_ERR(0);
    CHECK(U(FindFirstStreamTransactedW)(wp, (STREAM_INFO_LEVELS)5, &sd, 0, tx) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(U(FindFirstStreamTransactedW)(wp, FindStreamInfoStandard, &sd, 1, tx) == INVALID_HANDLE_VALUE);      /* flags must be zero */
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    g18_txn_done(tx); g18_rm(a);
}
TEST(gap18_ffnt_with_real_transaction) {
    REQUIRE(U(FindFirstFileNameTransactedW));
    HANDLE tx = g18_txn(); if (!tx) { t_skip("no transaction support"); return; }
    char a[MAX_PATH], l[MAX_PATH]; g18_path(a, MAX_PATH, "txn_a.txt"); g18_path(l, MAX_PATH, "txn_b.txt"); g18_put(a, "n", 1); g18_rm(l);
    if (!CreateHardLinkA(l, a, NULL)) { t_skip("no hard links"); g18_txn_done(tx); g18_rm(a); return; }
    WCHAR wp[MAX_PATH], nm[MAX_PATH]; g18_widen(wp, MAX_PATH, a); DWORD len = MAX_PATH; int n = 0;
    HANDLE h = U(FindFirstFileNameTransactedW)(wp, 0, &len, nm, tx);
    CHECKF(h != INVALID_HANDLE_VALUE, "err %lu", GetLastError());
    if (h != INVALID_HANDLE_VALUE) {
        do { n++; CHECK(nm[0] == L'\\'); len = MAX_PATH; } while (FindNextFileNameW(h, &len, nm));
        FindClose(h);
        EQ(n, 2);
    }
    /* flags must be zero */
    len = MAX_PATH; SET_ERR(0);
    CHECK(U(FindFirstFileNameTransactedW)(wp, 1, &len, nm, tx) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    /* short buffer reports the needed length */
    len = 2; SET_ERR(0);
    HANDLE h2 = U(FindFirstFileNameTransactedW)(wp, 0, &len, nm, tx);
    CHECK(h2 == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_MORE_DATA);
    CHECK(len > 2);
    g18_txn_done(tx); g18_rm(a); g18_rm(l);
}
TEST(gap18_glpnt_with_real_transaction) {
    REQUIRE(U(GetLongPathNameTransactedA)); REQUIRE(U(GetLongPathNameA)); REQUIRE(U(GetShortPathNameA));
    HANDLE tx = g18_txn(); if (!tx) { t_skip("no transaction support"); return; }
    char a[MAX_PATH]; g18_path(a, MAX_PATH, "a_rather_long_file_name_for_8dot3.txt"); g18_put(a, "z", 1);
    char sh[MAX_PATH], lg[MAX_PATH], lg2[MAX_PATH];
    DWORD n = U(GetShortPathNameA)(a, sh, MAX_PATH);
    if (!n) { t_skip("no 8.3 names"); g18_txn_done(tx); g18_rm(a); return; }
    SET_ERR(0);
    DWORD r1 = U(GetLongPathNameTransactedA)(sh, lg, MAX_PATH, tx);
    DWORD r2 = U(GetLongPathNameA)(sh, lg2, MAX_PATH);
    EQ(r1, r2); CHECK(r1 > 0);
    CHECK(!strcmp(lg, lg2));
    /* size query */
    EQ(U(GetLongPathNameTransactedA)(sh, NULL, 0, tx), r1 + 1);
    /* too-small buffer returns required size including terminator */
    char tiny[4];
    EQ(U(GetLongPathNameTransactedA)(sh, tiny, 4, tx), r1 + 1);
    /* missing file */
    SET_ERR(0);
    EQ(U(GetLongPathNameTransactedA)("C:\\k32t_gap18_missing_dir\\x.txt", lg, MAX_PATH, tx), 0);
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    EQ(UN(RtlGetCurrentTransaction) ? UN(RtlGetCurrentTransaction)() : NULL, NULL);
    g18_txn_done(tx); g18_rm(a);
}
TEST(gap18_transacted_k32_exports) {
    REQUIRE(UK(FindFirstFileTransactedW)); REQUIRE(UK(FindFirstStreamTransactedW)); REQUIRE(UK(FindFirstFileNameTransactedW)); REQUIRE(UK(GetLongPathNameTransactedA));
    CHECK(UB(FindFirstFileTransactedW) == NULL || 1);
}

/* ---------- GetFileAttributesExA ---------- */
TEST(gap18_gfaexa_file_basic) {
    REQUIRE(U(GetFileAttributesExA));
    char p[MAX_PATH]; g18_path(p, MAX_PATH, "ga_file.bin"); g18_put(p, "0123456789", 10);
    WIN32_FILE_ATTRIBUTE_DATA d; memset(&d, 0xCC, sizeof d);
    SET_ERR(0);
    CHECK(U(GetFileAttributesExA)(p, GetFileExInfoStandard, &d));
    EQ(d.nFileSizeHigh, 0); EQ(d.nFileSizeLow, 10);
    CHECK(d.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE);
    CHECK(!(d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));
    CHECK(d.ftCreationTime.dwHighDateTime && d.ftLastWriteTime.dwHighDateTime && d.ftLastAccessTime.dwHighDateTime);
    EQ(d.dwFileAttributes, GetFileAttributesA(p));
    g18_rm(p);
}
TEST(gap18_gfaexa_directory) {
    REQUIRE(U(GetFileAttributesExA));
    WIN32_FILE_ATTRIBUTE_DATA d; memset(&d, 0xCC, sizeof d);
    CHECK(U(GetFileAttributesExA)(t_tmpdir(), GetFileExInfoStandard, &d));
    CHECK(d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
    EQ(d.nFileSizeLow, 0); EQ(d.nFileSizeHigh, 0);
}
TEST(gap18_gfaexa_attribute_bits) {
    REQUIRE(U(GetFileAttributesExA));
    char p[MAX_PATH]; g18_path(p, MAX_PATH, "ga_attr.bin"); g18_put(p, "x", 1);
    static const DWORD a[] = { FILE_ATTRIBUTE_READONLY, FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_NOT_CONTENT_INDEXED };
    for (unsigned i = 0; i < 4; i++) {
        SetFileAttributesA(p, a[i]);
        WIN32_FILE_ATTRIBUTE_DATA d; CHECK(U(GetFileAttributesExA)(p, GetFileExInfoStandard, &d));
        CHECKF((d.dwFileAttributes & a[i]) == a[i], "set 0x%lx got 0x%lx", a[i], d.dwFileAttributes);
    }
    g18_rm(p);
}
TEST(gap18_gfaexa_invalid_info_level) {
    REQUIRE(U(GetFileAttributesExA));
    WIN32_FILE_ATTRIBUTE_DATA d; memset(&d, 0xCC, sizeof d);
    static const int lv[] = { 1, 2, 3, 100, -1, 0x7FFFFFFF };
    for (unsigned i = 0; i < sizeof lv / sizeof lv[0]; i++) {
        SET_ERR(0);
        CHECKF(!U(GetFileAttributesExA)(t_tmpdir(), (GET_FILEEX_INFO_LEVELS)lv[i], &d), "level %d", lv[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
    unsigned char *b = (unsigned char *)&d; int touched = 0; for (size_t i = 0; i < sizeof d; i++) touched |= (b[i] != 0xCC);
    CHECK(!touched);
}
TEST(gap18_gfaexa_missing) {
    REQUIRE(U(GetFileAttributesExA));
    char p[MAX_PATH]; g18_path(p, MAX_PATH, "ga_missing.bin"); g18_rm(p);
    WIN32_FILE_ATTRIBUTE_DATA d; memset(&d, 0xCC, sizeof d);
    SET_ERR(0); CHECK(!U(GetFileAttributesExA)(p, GetFileExInfoStandard, &d)); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    g18_path(p, MAX_PATH, "ga_missing_dir\\f.bin");
    SET_ERR(0); CHECK(!U(GetFileAttributesExA)(p, GetFileExInfoStandard, &d)); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    unsigned char *b = (unsigned char *)&d; int touched = 0; for (size_t i = 0; i < sizeof d; i++) touched |= (b[i] != 0xCC);
    CHECK(!touched);
}
TEST(gap18_gfaexa_file_used_as_directory) {
    REQUIRE(U(GetFileAttributesExA));
    char p[MAX_PATH + 16]; g18_path(p, MAX_PATH, "ga_notdir.txt"); g18_put(p, "x", 1);
    char q[MAX_PATH + 16]; snprintf(q, sizeof q, "%s\\child", p);
    WIN32_FILE_ATTRIBUTE_DATA d;
    SET_ERR(0); CHECK(!U(GetFileAttributesExA)(q, GetFileExInfoStandard, &d));
    DWORD e = GetLastError();
    CHECK(e == ERROR_PATH_NOT_FOUND || e == ERROR_DIRECTORY);
    t_note("file\\child err %lu", e);
    g18_rm(p);
}
TEST(gap18_gfaexa_wildcard_and_invalid_names) {
    REQUIRE(U(GetFileAttributesExA));
    char p[MAX_PATH]; WIN32_FILE_ATTRIBUTE_DATA d;
    g18_path(p, MAX_PATH, "ga_*");
    SET_ERR(0); CHECK(!U(GetFileAttributesExA)(p, GetFileExInfoStandard, &d)); t_note("wildcard '*' err %lu", GetLastError());
    g18_path(p, MAX_PATH, "ga_?.bin");
    SET_ERR(0); CHECK(!U(GetFileAttributesExA)(p, GetFileExInfoStandard, &d)); t_note("wildcard '?' err %lu", GetLastError());
    g18_path(p, MAX_PATH, "ga_a<b");
    SET_ERR(0); CHECK(!U(GetFileAttributesExA)(p, GetFileExInfoStandard, &d)); t_note("'<' err %lu", GetLastError());
    g18_path(p, MAX_PATH, "ga_a|b");
    SET_ERR(0); CHECK(!U(GetFileAttributesExA)(p, GetFileExInfoStandard, &d)); t_note("'|' err %lu", GetLastError());
}
TEST(gap18_gfaexa_empty_and_dots) {
    REQUIRE(U(GetFileAttributesExA));
    WIN32_FILE_ATTRIBUTE_DATA d;
    SET_ERR(0); CHECK(!U(GetFileAttributesExA)("", GetFileExInfoStandard, &d)); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    char p[MAX_PATH]; g18_path(p, MAX_PATH, ".");
    CHECK(U(GetFileAttributesExA)(p, GetFileExInfoStandard, &d)); CHECK(d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
    g18_path(p, MAX_PATH, "..");
    CHECK(U(GetFileAttributesExA)(p, GetFileExInfoStandard, &d)); CHECK(d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
    CHECK(U(GetFileAttributesExA)(".", GetFileExInfoStandard, &d));
}
TEST(gap18_gfaexa_trailing_backslash_on_file) {
    REQUIRE(U(GetFileAttributesExA));
    char p[MAX_PATH + 4]; g18_path(p, MAX_PATH, "ga_tb.txt"); g18_put(p, "x", 1);
    strcat(p, "\\");
    WIN32_FILE_ATTRIBUTE_DATA d;
    SET_ERR(0); CHECK(!U(GetFileAttributesExA)(p, GetFileExInfoStandard, &d));
    t_note("file with trailing backslash err %lu", GetLastError());
    p[strlen(p) - 1] = 0; g18_rm(p);
}
TEST(gap18_gfaexa_trailing_dots_spaces_normalised) {
    REQUIRE(U(GetFileAttributesExA));
    char p[MAX_PATH + 8]; g18_path(p, MAX_PATH, "ga_trim.txt"); g18_put(p, "x", 1);
    char q[MAX_PATH + 8]; WIN32_FILE_ATTRIBUTE_DATA d;
    snprintf(q, sizeof q, "%s.", p);
    CHECK(U(GetFileAttributesExA)(q, GetFileExInfoStandard, &d));          /* Win32 path normalisation trims a trailing dot */
    snprintf(q, sizeof q, "%s  ", p);
    CHECK(U(GetFileAttributesExA)(q, GetFileExInfoStandard, &d));          /* ... and trailing spaces */
    snprintf(q, sizeof q, "%s", p); for (char *c = q; *c; c++) if (*c == '\\') *c = '/';
    CHECK(U(GetFileAttributesExA)(q, GetFileExInfoStandard, &d));          /* forward slashes are separators */
    g18_rm(p);
}
TEST(gap18_gfaexa_case_insensitive) {
    REQUIRE(U(GetFileAttributesExA));
    char p[MAX_PATH]; g18_path(p, MAX_PATH, "GA_Case.TXT"); g18_put(p, "xyz", 3);
    char q[MAX_PATH]; g18_path(q, MAX_PATH, "ga_case.txt");
    WIN32_FILE_ATTRIBUTE_DATA d; CHECK(U(GetFileAttributesExA)(q, GetFileExInfoStandard, &d)); EQ(d.nFileSizeLow, 3);
    g18_rm(p);
}
TEST(gap18_gfaexa_long_path_nomanifest) {
    REQUIRE(U(GetFileAttributesExA));
    char p[700]; g18_path(p, sizeof p, "");
    size_t l = strlen(p); memset(p + l, 'a', 300); p[l + 300] = 0;
    WIN32_FILE_ATTRIBUTE_DATA d; SET_ERR(0);
    CHECK(!U(GetFileAttributesExA)(p, GetFileExInfoStandard, &d));
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);          /* path conversion fails (no long-path opt-in): the API reports path-not-found */
    char q[800]; strcpy(q, "\\\\?\\"); strcat(q, p);
    SET_ERR(0);
    CHECK(!U(GetFileAttributesExA)(q, GetFileExInfoStandard, &d));
    t_note("\\\\?\\ + 300-char component err %lu", GetLastError());   /* over-long component: passed through, rejected by the file system */
}
TEST(gap18_gfaexa_long_prefix_and_big_file) {
    REQUIRE(U(GetFileAttributesExA));
    char p[MAX_PATH + 8]; strcpy(p, "\\\\?\\"); g18_path(p + 4, MAX_PATH, "ga_long.bin"); g18_put(p, "abc", 3);
    WIN32_FILE_ATTRIBUTE_DATA d; CHECK(U(GetFileAttributesExA)(p, GetFileExInfoStandard, &d)); EQ(d.nFileSizeLow, 3);
    g18_rm(p);
    /* sparse 5 GiB logical size to exercise nFileSizeHigh */
    char s[MAX_PATH]; g18_path(s, MAX_PATH, "ga_sparse.bin");
    HANDLE h = CreateFileA(s, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD ret; DeviceIoControl(h, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &ret, NULL);
    LARGE_INTEGER li; li.QuadPart = 5LL << 30;
    BOOL ok = SetFilePointerEx(h, li, NULL, FILE_BEGIN) && SetEndOfFile(h);
    CloseHandle(h);
    if (ok) {
        CHECK(U(GetFileAttributesExA)(s, GetFileExInfoStandard, &d));
        EQ(d.nFileSizeHigh, 1); EQ(d.nFileSizeLow, 0x40000000);
    } else t_note("could not create sparse file");
    g18_rm(s);
}
TEST(gap18_gfaexa_a_w_parity) {
    REQUIRE(U(GetFileAttributesExA)); REQUIRE(U(GetFileAttributesExW));
    char p[MAX_PATH]; g18_path(p, MAX_PATH, "ga_par.bin"); g18_put(p, "parity", 6);
    WCHAR w[MAX_PATH]; g18_widen(w, MAX_PATH, p);
    WIN32_FILE_ATTRIBUTE_DATA a, b; memset(&a, 1, sizeof a); memset(&b, 2, sizeof b);
    CHECK(U(GetFileAttributesExA)(p, GetFileExInfoStandard, &a)); CHECK(U(GetFileAttributesExW)(w, GetFileExInfoStandard, &b));
    CHECK(!memcmp(&a, &b, sizeof a));
    g18_rm(p);
}
TEST(gap18_gfaexa_matches_findfirst) {
    REQUIRE(U(GetFileAttributesExA)); REQUIRE(U(FindFirstFileA));
    char p[MAX_PATH]; g18_path(p, MAX_PATH, "ga_ff.bin"); g18_put(p, "findfirst", 9);
    WIN32_FIND_DATAA fd; HANDLE h = U(FindFirstFileA)(p, &fd); CHECK(h != INVALID_HANDLE_VALUE);
    WIN32_FILE_ATTRIBUTE_DATA d; CHECK(U(GetFileAttributesExA)(p, GetFileExInfoStandard, &d));
    if (h != INVALID_HANDLE_VALUE) {
        EQ(d.dwFileAttributes, fd.dwFileAttributes); EQ(d.nFileSizeLow, fd.nFileSizeLow);
        CHECK(!memcmp(&d.ftLastWriteTime, &fd.ftLastWriteTime, 8)); CHECK(!memcmp(&d.ftCreationTime, &fd.ftCreationTime, 8));
        FindClose(h);
    }
    g18_rm(p);
}
TEST(gap18_gfaexa_exclusively_locked_file) {
    REQUIRE(U(GetFileAttributesExA));
    char p[MAX_PATH]; g18_path(p, MAX_PATH, "ga_lock.bin"); g18_put(p, "locked", 6);
    HANDLE h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    WIN32_FILE_ATTRIBUTE_DATA d;
    CHECK(U(GetFileAttributesExA)(p, GetFileExInfoStandard, &d));           /* attributes are readable without opening the file */
    EQ(d.nFileSizeLow, 6);
    CloseHandle(h); g18_rm(p);
}
TEST(gap18_gfaexa_pending_delete) {
    REQUIRE(U(GetFileAttributesExA));
    char p[MAX_PATH]; g18_path(p, MAX_PATH, "ga_del.bin"); g18_put(p, "d", 1);
    HANDLE h = CreateFileA(p, GENERIC_READ | DELETE, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE, NULL);
    WIN32_FILE_ATTRIBUTE_DATA d; SET_ERR(0);
    BOOL r = U(GetFileAttributesExA)(p, GetFileExInfoStandard, &d);
    t_note("delete-pending file -> %d err %lu", r, GetLastError());
    CloseHandle(h);
    CHECK(!g18_exists(p));
}
TEST(gap18_gfaexa_dos_device_names) {
    REQUIRE(U(GetFileAttributesExA));
    static const char *n[] = { "NUL", "CON", "COM1", "LPT1", "\\\\.\\NUL", "\\\\.\\PhysicalDrive0" };
    for (unsigned i = 0; i < 6; i++) {
        WIN32_FILE_ATTRIBUTE_DATA d; SET_ERR(0);
        BOOL r = U(GetFileAttributesExA)(n[i], GetFileExInfoStandard, &d);
        t_note("%s -> %d err %lu attrs 0x%lx", n[i], r, GetLastError(), r ? d.dwFileAttributes : 0);
    }
}
TEST(gap18_gfaexa_system_files) {
    REQUIRE(U(GetFileAttributesExA));
    char sys[MAX_PATH], k[MAX_PATH + 16]; GetSystemDirectoryA(sys, MAX_PATH);
    snprintf(k, sizeof k, "%s\\kernel32.dll", sys);
    WIN32_FILE_ATTRIBUTE_DATA d; memset(&d, 0, sizeof d);
    CHECK(U(GetFileAttributesExA)(k, GetFileExInfoStandard, &d));
    CHECK(d.nFileSizeLow > 100000);
    CHECK(!(d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));
    CHECK(U(GetFileAttributesExA)(sys, GetFileExInfoStandard, &d)); CHECK(d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
    char root[4] = { sys[0], ':', '\\', 0 };
    CHECK(U(GetFileAttributesExA)(root, GetFileExInfoStandard, &d)); CHECK(d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
}
TEST(gap18_gfaexa_reparse_point_not_followed) {
    REQUIRE(U(GetFileAttributesExA));
    char d1[MAX_PATH], l[MAX_PATH], target[MAX_PATH]; g18_path(target, MAX_PATH, "ga_target.txt"); g18_put(target, "tgt", 3);
    g18_path(l, MAX_PATH, "ga_symlink.txt"); g18_rm(l);
    if (!CreateSymbolicLinkA(l, target, 0)) { t_skip("symlink privilege/support missing"); g18_rm(target); return; }
    WIN32_FILE_ATTRIBUTE_DATA d; CHECK(U(GetFileAttributesExA)(l, GetFileExInfoStandard, &d));
    CHECK(d.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT);          /* the link itself, not the target */
    g18_rm(l); g18_rm(target);
}
TEST(gap18_gfaexa_repeat_and_k32) {
    REQUIRE(UK(GetFileAttributesExA));
    char p[MAX_PATH]; g18_path(p, MAX_PATH, "ga_rep.bin"); g18_put(p, "r", 1);
    WIN32_FILE_ATTRIBUTE_DATA d;
    for (int i = 0; i < 2000; i++) { if (!UK(GetFileAttributesExA)(p, GetFileExInfoStandard, &d) || d.nFileSizeLow != 1) { CHECKF(0, "iter %d", i); break; } }
    g18_rm(p);
}
TEST(gap18_gfaexa_high_ansi_chars) {
    REQUIRE(U(GetFileAttributesExA));
    if (GetACP() != 1252) { t_skip("needs ACP 1252"); return; }
    char p[MAX_PATH]; g18_path(p, MAX_PATH, "ga_na\xefve.bin"); g18_put(p, "ok", 2);
    WIN32_FILE_ATTRIBUTE_DATA d; CHECK(U(GetFileAttributesExA)(p, GetFileExInfoStandard, &d)); EQ(d.nFileSizeLow, 2);
    g18_rm(p);
}

/* ---------- OpenFileMappingW ---------- */
static void om_name(WCHAR *w, const char *tag) { wsprintfW(w, L"Local\\k32t_g18_%S_%lu", tag, GetCurrentProcessId()); }

TEST(gap18_ofmw_null_name) {
    REQUIRE(U(OpenFileMappingW));
    SET_ERR(0);
    CHECK(U(OpenFileMappingW)(FILE_MAP_READ, FALSE, NULL) == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(U(OpenFileMappingW)(FILE_MAP_ALL_ACCESS, TRUE, NULL) == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(U(OpenFileMappingW)(0, FALSE, NULL) == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap18_ofmw_missing_name) {
    REQUIRE(U(OpenFileMappingW));
    WCHAR n[96]; om_name(n, "missing");
    SET_ERR(0);
    CHECK(U(OpenFileMappingW)(FILE_MAP_READ, FALSE, n) == NULL);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    wsprintfW(n, L"Global\\k32t_g18_missing_%lu", GetCurrentProcessId());
    SET_ERR(0);
    CHECK(U(OpenFileMappingW)(FILE_MAP_READ, FALSE, n) == NULL);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    wsprintfW(n, L"k32t_g18_nopfx_%lu", GetCurrentProcessId());
    SET_ERR(0);
    CHECK(U(OpenFileMappingW)(FILE_MAP_READ, FALSE, n) == NULL);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(gap18_ofmw_empty_name) {
    REQUIRE(U(OpenFileMappingW));
    SET_ERR(0);
    HANDLE h = U(OpenFileMappingW)(FILE_MAP_READ, FALSE, L"");
    t_note("empty name -> %p err %lu", h, h ? 0 : GetLastError());
    CHECK(h == NULL);
    if (h) CloseHandle(h);
}
TEST(gap18_ofmw_basic_open_and_share) {
    REQUIRE(U(OpenFileMappingW));
    WCHAR n[96]; om_name(n, "basic");
    HANDLE fm = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, n);
    CHECK(fm); if (!fm) return;
    char *v1 = (char *)MapViewOfFile(fm, FILE_MAP_ALL_ACCESS, 0, 0, 0); CHECK(v1); if (v1) strcpy(v1, "shared-view");
    HANDLE o = U(OpenFileMappingW)(FILE_MAP_ALL_ACCESS, FALSE, n);
    CHECK(o != NULL); CHECK(o != fm);
    if (o) {
        char *v2 = (char *)MapViewOfFile(o, FILE_MAP_READ, 0, 0, 0);
        CHECK(v2 && v2 != v1);
        if (v2) { CHECK(!strcmp(v2, "shared-view")); v1[0] = 'S'; CHECK(v2[0] == 'S'); UnmapViewOfFile(v2); }
        CloseHandle(o);
    }
    if (v1) UnmapViewOfFile(v1);
    CloseHandle(fm);
    SET_ERR(0);
    CHECK(U(OpenFileMappingW)(FILE_MAP_READ, FALSE, n) == NULL);       /* last handle closed: name is gone */
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(gap18_ofmw_name_is_case_sensitive) {
    REQUIRE(U(OpenFileMappingW));
    WCHAR n[96], up[96]; om_name(n, "CaseTest");
    HANDLE fm = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, n);
    CHECK(fm); if (!fm) return;
    lstrcpyW(up, n); CharUpperW(up);
    SET_ERR(0);
    HANDLE o = U(OpenFileMappingW)(FILE_MAP_READ, FALSE, up);
    CHECK(o == NULL);               /* named-object comparison is case sensitive */
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    if (o) CloseHandle(o);
    o = U(OpenFileMappingW)(FILE_MAP_READ, FALSE, n);
    CHECK(o != NULL);
    if (o) CloseHandle(o);
    CloseHandle(fm);
}
TEST(gap18_ofmw_local_prefix_equivalence) {
    REQUIRE(U(OpenFileMappingW));
    WCHAR n[96], bare[96]; wsprintfW(bare, L"k32t_g18_lp_%lu", GetCurrentProcessId()); wsprintfW(n, L"Local\\%s", bare);
    HANDLE fm = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, bare);
    CHECK(fm); if (!fm) return;
    HANDLE o = U(OpenFileMappingW)(FILE_MAP_READ, FALSE, n);
    CHECK(o != NULL);                      /* an unqualified name lives in the session (Local) namespace */
    if (o) CloseHandle(o);
    CloseHandle(fm);
}
TEST(gap18_ofmw_wrong_object_type) {
    REQUIRE(U(OpenFileMappingW));
    WCHAR n[96]; om_name(n, "typemix");
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, n);
    CHECK(ev); if (!ev) return;
    SET_ERR(0);
    CHECK(U(OpenFileMappingW)(FILE_MAP_READ, FALSE, n) == NULL);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
    HANDLE mu = CreateMutexW(NULL, FALSE, n);
    SET_ERR(0);
    CHECK(U(OpenFileMappingW)(FILE_MAP_READ, FALSE, n) == NULL);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(mu);
}
TEST(gap18_ofmw_path_components) {
    REQUIRE(U(OpenFileMappingW));
    WCHAR n[128];
    wsprintfW(n, L"Local\\k32t_g18_nodir_%lu\\inner", GetCurrentProcessId());
    SET_ERR(0);
    CHECK(U(OpenFileMappingW)(FILE_MAP_READ, FALSE, n) == NULL);
    t_note("name with missing subdirectory err %lu", GetLastError());
    CHECK(GetLastError() == ERROR_PATH_NOT_FOUND || GetLastError() == ERROR_FILE_NOT_FOUND);
    wsprintfW(n, L"\\\\?\\foo_%lu", GetCurrentProcessId());
    SET_ERR(0);
    CHECK(U(OpenFileMappingW)(FILE_MAP_READ, FALSE, n) == NULL);
    t_note("weird name err %lu", GetLastError());
}
TEST(gap18_ofmw_overlong_name) {
    REQUIRE(U(OpenFileMappingW));
    static WCHAR big[40000]; for (int i = 0; i < 39999; i++) big[i] = L'a'; big[39999] = 0;
    SET_ERR(0);
    CHECK(U(OpenFileMappingW)(FILE_MAP_READ, FALSE, big) == NULL);
    t_note("name > 32767 chars err %lu", GetLastError());
    CHECK(GetLastError() != 0);
}
TEST(gap18_ofmw_access_rights_matrix) {
    REQUIRE(U(OpenFileMappingW));
    WCHAR n[96]; om_name(n, "rights");
    HANDLE fm = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, n);
    CHECK(fm); if (!fm) return;
    static const DWORD acc[] = { FILE_MAP_READ, FILE_MAP_WRITE, FILE_MAP_COPY, FILE_MAP_ALL_ACCESS, FILE_MAP_EXECUTE | FILE_MAP_READ, SECTION_QUERY, SECTION_MAP_READ | SECTION_MAP_WRITE, 0 };
    for (unsigned i = 0; i < sizeof acc / sizeof acc[0]; i++) {
        SET_ERR(0);
        HANDLE o = U(OpenFileMappingW)(acc[i], FALSE, n);
        if (!o) t_note("access 0x%lx failed err %lu", acc[i], GetLastError());
        CHECKF(o != NULL || acc[i] == 0, "access 0x%lx err %lu", acc[i], GetLastError());
        if (o) CloseHandle(o);
    }
    CloseHandle(fm);
}
TEST(gap18_ofmw_access_limited_by_creator_handle_irrelevant) {
    REQUIRE(U(OpenFileMappingW));
    /* A read-only mapping object still grants write-map rights on open (object ACL, not page protection, decides),
       but a view with write access then fails. */
    WCHAR n[96]; om_name(n, "roobj");
    HANDLE fm = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READONLY, 0, 4096, n);
    CHECK(fm); if (!fm) return;
    HANDLE o = U(OpenFileMappingW)(FILE_MAP_WRITE, FALSE, n);
    CHECK(o != NULL);
    if (o) {
        SET_ERR(0);
        void *v = MapViewOfFile(o, FILE_MAP_WRITE, 0, 0, 0);
        CHECK(v == NULL);
        t_note("write view of read-only section err %lu", GetLastError());
        if (v) UnmapViewOfFile(v);
        CloseHandle(o);
    }
    CloseHandle(fm);
}
TEST(gap18_ofmw_inherit_flag) {
    REQUIRE(U(OpenFileMappingW));
    WCHAR n[96]; om_name(n, "inh");
    HANDLE fm = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, n);
    CHECK(fm); if (!fm) return;
    DWORD fl;
    HANDLE a = U(OpenFileMappingW)(FILE_MAP_READ, TRUE, n), b = U(OpenFileMappingW)(FILE_MAP_READ, FALSE, n);
    CHECK(a && b);
    if (a) { CHECK(GetHandleInformation(a, &fl)); CHECK(fl & HANDLE_FLAG_INHERIT); CloseHandle(a); }
    if (b) { CHECK(GetHandleInformation(b, &fl)); CHECK(!(fl & HANDLE_FLAG_INHERIT)); CloseHandle(b); }
    CloseHandle(fm);
}
TEST(gap18_ofmw_many_opens_distinct_handles) {
    REQUIRE(U(OpenFileMappingW));
    WCHAR n[96]; om_name(n, "many");
    HANDLE fm = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, n);
    CHECK(fm); if (!fm) return;
    HANDLE hs[200]; int c = 0;
    for (int i = 0; i < 200; i++) { hs[c] = U(OpenFileMappingW)(FILE_MAP_READ, FALSE, n); if (hs[c]) c++; }
    EQ(c, 200);
    for (int i = 0; i < c; i++) for (int j = i + 1; j < c; j++) if (hs[i] == hs[j]) { CHECK(0); i = c; break; }
    for (int i = 0; i < c; i++) CloseHandle(hs[i]);
    CloseHandle(fm);
}
TEST(gap18_ofmw_survives_creator_handle_while_view_mapped) {
    REQUIRE(U(OpenFileMappingW));
    WCHAR n[96]; om_name(n, "viewlife");
    HANDLE fm = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, n);
    char *v = (char *)MapViewOfFile(fm, FILE_MAP_ALL_ACCESS, 0, 0, 0); CHECK(v); if (v) strcpy(v, "alive");
    CloseHandle(fm);
    SET_ERR(0);
    HANDLE o = U(OpenFileMappingW)(FILE_MAP_READ, FALSE, n);
    CHECK(o == NULL);                       /* the name goes away with the last *handle*, views notwithstanding */
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    if (o) CloseHandle(o);
    if (v) { CHECK(!strcmp(v, "alive")); UnmapViewOfFile(v); }
}
CHILD(gap18_ofmw_child) {
    CK(argc >= 2 && U(OpenFileMappingW));
    WCHAR n[96]; wsprintfW(n, L"Local\\k32t_g18_xproc_%S", argv[1]);
    HANDLE o = U(OpenFileMappingW)(FILE_MAP_ALL_ACCESS, FALSE, n);
    CK(o != NULL);
    char *v = (char *)MapViewOfFile(o, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    CK(v != NULL);
    CK(!strcmp(v, "from-parent"));
    strcpy(v, "from-child");
    UnmapViewOfFile(v); CloseHandle(o);
    return 0;
}
TEST(gap18_ofmw_cross_process) {
    REQUIRE(U(OpenFileMappingW));
    char tag[32]; snprintf(tag, sizeof tag, "%lu", GetCurrentProcessId());
    WCHAR n[96]; wsprintfW(n, L"Local\\k32t_g18_xproc_%S", tag);
    HANDLE fm = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, n);
    CHECK(fm); if (!fm) return;
    char *v = (char *)MapViewOfFile(fm, FILE_MAP_ALL_ACCESS, 0, 0, 0); CHECK(v); if (!v) { CloseHandle(fm); return; }
    strcpy(v, "from-parent");
    EQ(t_run_child("gap18_ofmw_child", tag, 30000), 0);
    CHECK(!strcmp(v, "from-child"));
    UnmapViewOfFile(v); CloseHandle(fm);
}
TEST(gap18_ofmw_file_backed_mapping) {
    REQUIRE(U(OpenFileMappingW));
    char p[MAX_PATH]; g18_path(p, MAX_PATH, "om_file.bin"); g18_put(p, "file-backed-data", 16);
    HANDLE f = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    WCHAR n[96]; om_name(n, "filemap");
    HANDLE fm = CreateFileMappingW(f, NULL, PAGE_READWRITE, 0, 0, n);
    CHECK(fm);
    HANDLE o = fm ? U(OpenFileMappingW)(FILE_MAP_READ, FALSE, n) : NULL;
    CHECK(o != NULL);
    if (o) { char *v = (char *)MapViewOfFile(o, FILE_MAP_READ, 0, 0, 0); CHECK(v && !memcmp(v, "file-backed-data", 16)); if (v) UnmapViewOfFile(v); CloseHandle(o); }
    if (fm) CloseHandle(fm);
    CloseHandle(f); g18_rm(p);
}
TEST(gap18_ofmw_security_attrs_deny_via_dacl_note) {
    REQUIRE(U(OpenFileMappingW));
    WCHAR n[96]; om_name(n, "dup");
    HANDLE fm = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, n);
    CHECK(fm); if (!fm) return;
    /* a duplicate with reduced rights does not influence what a fresh open grants */
    HANDLE d = NULL; CHECK(DuplicateHandle(GetCurrentProcess(), fm, GetCurrentProcess(), &d, SECTION_MAP_READ, FALSE, 0));
    HANDLE o = U(OpenFileMappingW)(FILE_MAP_ALL_ACCESS, FALSE, n);
    CHECK(o != NULL);
    if (o) CloseHandle(o);
    if (d) CloseHandle(d);
    CloseHandle(fm);
}
TEST(gap18_ofmw_k32_forwarder) {
    REQUIRE(UK(OpenFileMappingW)); REQUIRE(UB(OpenFileMappingW));
    SET_ERR(0);
    CHECK(UK(OpenFileMappingW)(FILE_MAP_READ, FALSE, NULL) == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
