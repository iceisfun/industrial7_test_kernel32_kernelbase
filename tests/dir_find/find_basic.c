/* FindFirstFile{A,W} / FindNextFile{A,W} / FindClose: semantics and wildcard matching */
#include "dh.h"

static char *sub(char *buf, size_t cap, const char *dir, const char *leaf) { dh_path(buf, cap, dir, leaf); return buf; }

/* Verify pattern (relative to dir) yields exactly the expected set of non-dot names. exp==NULL-terminated. */
static void check_set(const char *dir, const char *pat, const char *const *exp) {
    char full[MAX_PATH * 2]; dh_name l[64];
    sub(full, sizeof full, dir, pat);
    int ne = 0; while (exp[ne]) ne++;
    SET_ERR(0);
    int n = dh_listA_nd(full, l, 64);
    if (ne == 0) {
        CHECKF(n <= 0, "pattern '%s' expected no match, got %d", pat, n);
        return;
    }
    CHECKF(n == ne, "pattern '%s': got %d names, want %d", pat, n, ne);
    for (int i = 0; i < ne; i++) CHECKF(n >= 0 && dh_has(l, n < 64 ? n : 64, exp[i]), "pattern '%s' missing '%s'", pat, exp[i]);
}

static char *wild_dir(void) {
    static char *d;
    if (d) return d;
    d = dh_dir("fb_wild");
    const char *f[] = { "a.txt", "ab.txt", "abc.txt", "a", "noext", "x.tar.gz", "b.TXT", "two words.txt", "a.b.c" };
    for (unsigned i = 0; i < sizeof f / sizeof *f; i++) dh_mkfile(d, f[i], i);
    return d;
}

TEST(find_a_basic_listing) {
    REQUIRE(U(FindFirstFileA));
    char *d = dh_dir("fb_list"), pat[MAX_PATH];
    dh_mkfile(d, "a.txt", 1); dh_mkfile(d, "b.txt", 2); dh_mkdir(d, "sub");
    sub(pat, sizeof pat, d, "*");
    dh_name l[16];
    int n = dh_listA(pat, l, 16);
    EXPECT_EQ(n, 5);
    CHECK(dh_has(l, n, ".") && dh_has(l, n, "..") && dh_has(l, n, "a.txt") && dh_has(l, n, "b.txt") && dh_has(l, n, "sub"));
}
TEST(find_w_basic_listing) {
    REQUIRE(U(FindFirstFileW)); REQUIRE(U(FindNextFileW)); REQUIRE(U(FindClose));
    char *d = dh_dir("fb_listw"), p[MAX_PATH]; WCHAR w[MAX_PATH];
    dh_mkfile(d, "a.txt", 1); dh_mkdir(d, "sub");
    sub(p, sizeof p, d, "*"); dh_w(w, MAX_PATH, p);
    WIN32_FIND_DATAW fd; int n = 0, seen_a = 0, seen_sub = 0, seen_dot = 0, seen_dd = 0;
    HANDLE h = U(FindFirstFileW)(w, &fd);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        n++;
        if (!wcscmp(fd.cFileName, L"a.txt")) seen_a = 1;
        if (!wcscmp(fd.cFileName, L"sub")) seen_sub = 1;
        if (!wcscmp(fd.cFileName, L".")) seen_dot = 1;
        if (!wcscmp(fd.cFileName, L"..")) seen_dd = 1;
    } while (U(FindNextFileW)(h, &fd));
    EXPECT_ERR(ERROR_NO_MORE_FILES);
    CHECK(U(FindClose)(h));
    EXPECT_EQ(n, 4);
    CHECK(seen_a && seen_sub && seen_dot && seen_dd);
}
TEST(find_dot_entries_are_directories) {
    REQUIRE(U(FindFirstFileA)); REQUIRE(U(FindNextFileA)); REQUIRE(U(FindClose));
    char *d = dh_dir("fb_dots"), pat[MAX_PATH]; WIN32_FIND_DATAA fd;
    dh_mkdir(d, "s");
    sub(pat, sizeof pat, d, "s\\*");
    HANDLE h = U(FindFirstFileA)(pat, &fd);
    CHECK(h != INVALID_HANDLE_VALUE);
    int dot = 0, dd = 0, n = 0;
    do {
        n++;
        if (!strcmp(fd.cFileName, ".")) { dot++; CHECK(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY); }
        if (!strcmp(fd.cFileName, "..")) { dd++; CHECK(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY); }
    } while (U(FindNextFileA)(h, &fd));
    U(FindClose)(h);
    EXPECT_EQ(dot, 1); EXPECT_EQ(dd, 1); EXPECT_EQ(n, 2);
}
TEST(find_empty_dir_returns_dots_only) {
    REQUIRE(U(FindFirstFileA));
    char *d = dh_dir("fb_empty"), pat[MAX_PATH]; dh_name l[8];
    sub(pat, sizeof pat, d, "*");
    int n = dh_listA(pat, l, 8);
    EXPECT_EQ(n, 2);
    CHECK(dh_has(l, n, ".") && dh_has(l, n, ".."));
}
TEST(find_root_has_no_dot_entries) {
    REQUIRE(U(FindFirstFileA));
    char root[8] = "C:\\*";
    root[0] = t_tmpdir()[0];
    dh_name *l = malloc(sizeof(dh_name) * 1100);
    int n = dh_listA(root, l, 1100);
    CHECK(n > 0);
    if (n > 0) { CHECK(!dh_has(l, n < 1100 ? n : 1100, ".")); CHECK(!dh_has(l, n < 1100 ? n : 1100, "..")); }
    free(l);
}
TEST(find_root_no_match) {
    REQUIRE(U(FindFirstFileA));
    char root[32] = "C:\\k32t_nomatch_*.zz"; WIN32_FIND_DATAA fd;
    root[0] = t_tmpdir()[0];
    SET_ERR(0);
    CHECK(U(FindFirstFileA)(root, &fd) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(find_no_match_error_2_a) {
    REQUIRE(U(FindFirstFileA));
    char *d = dh_dir("fb_nomatch"), pat[MAX_PATH]; WIN32_FIND_DATAA fd;
    dh_mkfile(d, "a.txt", 1);
    sub(pat, sizeof pat, d, "*.zzz");
    SET_ERR(0);
    CHECK(U(FindFirstFileA)(pat, &fd) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    sub(pat, sizeof pat, d, "nosuchfile.txt");
    CHECK(U(FindFirstFileA)(pat, &fd) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(find_no_match_error_2_w) {
    REQUIRE(U(FindFirstFileW));
    char *d = dh_dir("fb_nomatchw"), pat[MAX_PATH]; WCHAR w[MAX_PATH]; WIN32_FIND_DATAW fd;
    sub(pat, sizeof pat, d, "nothing*"); dh_w(w, MAX_PATH, pat);
    SET_ERR(0);
    CHECK(U(FindFirstFileW)(w, &fd) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(find_missing_directory_error_3) {
    REQUIRE(U(FindFirstFileA)); REQUIRE(U(FindFirstFileW));
    char *d = dh_dir("fb_nodir"), pat[MAX_PATH]; WCHAR w[MAX_PATH]; WIN32_FIND_DATAA fa; WIN32_FIND_DATAW fw;
    sub(pat, sizeof pat, d, "nodir\\*");
    SET_ERR(0);
    CHECK(U(FindFirstFileA)(pat, &fa) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    dh_w(w, MAX_PATH, pat);
    CHECK(U(FindFirstFileW)(w, &fw) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    sub(pat, sizeof pat, d, "nodir\\a\\b.txt");
    CHECK(U(FindFirstFileA)(pat, &fa) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(find_missing_drive) {
    REQUIRE(U(FindFirstFileA));
    DWORD mask = GetLogicalDrives(); char p[16] = "?:\\*"; int c; WIN32_FIND_DATAA fd;
    for (c = 25; c >= 0; c--) if (!(mask & (1u << c))) break;
    if (c < 0) { t_skip("no free drive"); return; }
    p[0] = 'A' + c;
    SET_ERR(0);
    CHECK(U(FindFirstFileA)(p, &fd) == INVALID_HANDLE_VALUE);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_PATH_NOT_FOUND || e == ERROR_INVALID_DRIVE, "err %lu", e);
}
TEST(find_empty_string_error) {
    REQUIRE(U(FindFirstFileA)); REQUIRE(U(FindFirstFileW));
    WIN32_FIND_DATAA fa; WIN32_FIND_DATAW fw;
    SET_ERR(0);
    CHECK(U(FindFirstFileA)("", &fa) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    SET_ERR(0);
    CHECK(U(FindFirstFileW)(L"", &fw) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(find_null_name) {
    REQUIRE(U(FindFirstFileW));
    WIN32_FIND_DATAW fw;
    SET_ERR(0);
    CHECK(U(FindFirstFileW)(NULL, &fw) == INVALID_HANDLE_VALUE);
    t_note("FindFirstFileW(NULL) err=%lu", GetLastError());
}
TEST(find_null_name_a) {
    REQUIRE(U(FindFirstFileA));
    WIN32_FIND_DATAA fa;
    SET_ERR(0);
    CHECK(U(FindFirstFileA)(NULL, &fa) == INVALID_HANDLE_VALUE);
    t_note("FindFirstFileA(NULL) err=%lu", GetLastError());
}
TEST(find_trailing_backslash_dir) {
    REQUIRE(U(FindFirstFileA));
    char *d = dh_dir("fb_trail"), pat[MAX_PATH]; WIN32_FIND_DATAA fd;
    dh_mkfile(d, "f", 1);
    snprintf(pat, sizeof pat, "%s\\", d);
    SET_ERR(0);
    HANDLE h = U(FindFirstFileA)(pat, &fd);
    CHECK(h == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    if (h != INVALID_HANDLE_VALUE) U(FindClose)(h);
}
TEST(find_directory_as_pattern) {
    REQUIRE(U(FindFirstFileA)); REQUIRE(U(FindNextFileA)); REQUIRE(U(FindClose));
    char *d = dh_dir("fb_dirpat"), pat[MAX_PATH]; WIN32_FIND_DATAA fd;
    dh_mkdir(d, "child"); dh_mkfile(d, "child\\inner.txt", 1);
    sub(pat, sizeof pat, d, "child");
    HANDLE h = U(FindFirstFileA)(pat, &fd);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    EXPECT_STR(fd.cFileName, "child");
    CHECK(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
    CHECK(!U(FindNextFileA)(h, &fd));
    EXPECT_ERR(ERROR_NO_MORE_FILES);
    U(FindClose)(h);
}
TEST(find_exact_file_name) {
    REQUIRE(U(FindFirstFileA)); REQUIRE(U(FindNextFileA)); REQUIRE(U(FindClose));
    char *d = dh_dir("fb_exact"), pat[MAX_PATH]; WIN32_FIND_DATAA fd;
    dh_mkfile(d, "FileName.Dat", 1234);
    sub(pat, sizeof pat, d, "filename.dat");
    HANDLE h = U(FindFirstFileA)(pat, &fd);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    EXPECT_STR(fd.cFileName, "FileName.Dat");     /* on-disk case, not pattern case */
    EXPECT_EQ(fd.nFileSizeLow, 1234); EXPECT_EQ(fd.nFileSizeHigh, 0);
    CHECK(!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));
    CHECK(fd.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE);
    CHECK(!U(FindNextFileA)(h, &fd));
    EXPECT_ERR(ERROR_NO_MORE_FILES);
    CHECK(!U(FindNextFileA)(h, &fd));
    EXPECT_ERR(ERROR_NO_MORE_FILES);
    CHECK(U(FindClose)(h));
}
TEST(find_next_after_end_repeats_error_18) {
    REQUIRE(U(FindFirstFileW)); REQUIRE(U(FindNextFileW)); REQUIRE(U(FindClose));
    char *d = dh_dir("fb_end"), pat[MAX_PATH]; WCHAR w[MAX_PATH]; WIN32_FIND_DATAW fd;
    dh_mkfile(d, "only", 1);
    sub(pat, sizeof pat, d, "only"); dh_w(w, MAX_PATH, pat);
    HANDLE h = U(FindFirstFileW)(w, &fd);
    CHECK(h != INVALID_HANDLE_VALUE);
    for (int i = 0; i < 5; i++) { SET_ERR(0); CHECK(!U(FindNextFileW)(h, &fd)); EXPECT_ERR(ERROR_NO_MORE_FILES); }
    U(FindClose)(h);
}
TEST(find_close_invalid_handle) {
    REQUIRE(U(FindClose));
    SET_ERR(0);
    CHECK(!U(FindClose)(INVALID_HANDLE_VALUE));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(find_next_invalid_handle) {
    REQUIRE(U(FindNextFileA)); REQUIRE(U(FindNextFileW));
    WIN32_FIND_DATAA fa; WIN32_FIND_DATAW fw;
    SET_ERR(0);
    CHECK(!U(FindNextFileA)(INVALID_HANDLE_VALUE, &fa));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(!U(FindNextFileW)(INVALID_HANDLE_VALUE, &fw));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(find_close_returns_true_and_is_independent) {
    REQUIRE(U(FindFirstFileA)); REQUIRE(U(FindNextFileA)); REQUIRE(U(FindClose));
    char *d = dh_dir("fb_indep"), pat[MAX_PATH]; WIN32_FIND_DATAA f1, f2;
    for (int i = 0; i < 5; i++) { char n[8]; snprintf(n, sizeof n, "f%d", i); dh_mkfile(d, n, 1); }
    sub(pat, sizeof pat, d, "f*");
    HANDLE h1 = U(FindFirstFileA)(pat, &f1), h2 = U(FindFirstFileA)(pat, &f2);
    CHECK(h1 != INVALID_HANDLE_VALUE && h2 != INVALID_HANDLE_VALUE && h1 != h2);
    CHECK(U(FindNextFileA)(h1, &f1));
    CHECK(U(FindNextFileA)(h1, &f1));
    CHECK(U(FindClose)(h1));
    int n = 1; while (U(FindNextFileA)(h2, &f2)) n++;
    EXPECT_EQ(n, 5);
    CHECK(U(FindClose)(h2));
}
TEST(find_next_after_sleep) {
    REQUIRE(U(FindFirstFileA));
    /* find handles can be moved to another thread; here: iterate fully from the same thread after a Sleep */
    char *d = dh_dir("fb_thread"), pat[MAX_PATH]; WIN32_FIND_DATAA fd;
    dh_mkfile(d, "x", 1); dh_mkfile(d, "y", 1);
    sub(pat, sizeof pat, d, "*");
    HANDLE h = U(FindFirstFileA)(pat, &fd);
    CHECK(h != INVALID_HANDLE_VALUE);
    Sleep(10);
    CHECK(U(FindNextFileA)(h, &fd));
    CHECK(U(FindNextFileA)(h, &fd));
    CHECK(U(FindNextFileA)(h, &fd));
    CHECK(!U(FindNextFileA)(h, &fd));
    U(FindClose)(h);
}
static DWORD WINAPI find_worker(LPVOID p) {
    WIN32_FIND_DATAA fd; int n = 0;
    HANDLE h = (HANDLE)p;
    while (U(FindNextFileA)(h, &fd)) n++;
    return n;
}
TEST(find_next_from_other_thread) {
    REQUIRE(U(FindFirstFileA)); REQUIRE(U(FindClose));
    char *d = dh_dir("fb_thread2"), pat[MAX_PATH]; WIN32_FIND_DATAA fd;
    for (int i = 0; i < 20; i++) { char n[8]; snprintf(n, sizeof n, "t%02d", i); dh_mkfile(d, n, 1); }
    sub(pat, sizeof pat, d, "t*");
    HANDLE h = U(FindFirstFileA)(pat, &fd);
    CHECK(h != INVALID_HANDLE_VALUE);
    HANDLE th = CreateThread(NULL, 0, find_worker, h, 0, NULL);
    WaitForSingleObject(th, 10000);
    DWORD code = 0; GetExitCodeThread(th, &code); CloseHandle(th);
    EXPECT_EQ(code, 19);
    U(FindClose)(h);
}
TEST(find_struct_times_and_sizes) {
    REQUIRE(U(FindFirstFileW)); REQUIRE(U(FindClose));
    char *d = dh_dir("fb_struct"), pat[MAX_PATH]; WCHAR w[MAX_PATH]; WIN32_FIND_DATAW fd; WIN32_FILE_ATTRIBUTE_DATA ad;
    dh_mkfile(d, "data.bin", 70000);
    sub(pat, sizeof pat, d, "data.bin"); dh_w(w, MAX_PATH, pat);
    HANDLE h = U(FindFirstFileW)(w, &fd);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    EXPECT_EQ(fd.nFileSizeLow, 70000); EXPECT_EQ(fd.nFileSizeHigh, 0);
    CHECK(GetFileAttributesExW(w, GetFileExInfoStandard, &ad));
    CHECK(!CompareFileTime(&fd.ftCreationTime, &ad.ftCreationTime));
    CHECK(!CompareFileTime(&fd.ftLastWriteTime, &ad.ftLastWriteTime));
    EXPECT_EQ(fd.dwFileAttributes, ad.dwFileAttributes);
    CHECK(fd.ftCreationTime.dwHighDateTime != 0);
    CHECK(fd.ftLastWriteTime.dwHighDateTime != 0);
    CHECK(fd.ftLastAccessTime.dwHighDateTime != 0);
    U(FindClose)(h);
}
TEST(find_struct_reserved_untouched_for_plain_file) {
    REQUIRE(U(FindFirstFileW)); REQUIRE(U(FindClose));
    char *d = dh_dir("fb_resv"), pat[MAX_PATH]; WCHAR w[MAX_PATH]; WIN32_FIND_DATAW fd;
    dh_mkfile(d, "plain", 1);
    sub(pat, sizeof pat, d, "plain"); dh_w(w, MAX_PATH, pat);
    memset(&fd, 0xCC, sizeof fd);
    HANDLE h = U(FindFirstFileW)(w, &fd);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    t_note("dwReserved0=0x%lx dwReserved1=0x%lx for a plain file", fd.dwReserved0, fd.dwReserved1);
    if (!t_is_wine()) { EXPECT_EQ(fd.dwReserved0, 0xCCCCCCCC); EXPECT_EQ(fd.dwReserved1, 0xCCCCCCCC); }
    U(FindClose)(h);
}
TEST(find_struct_large_size_high_part) {
    REQUIRE(U(FindFirstFileA)); REQUIRE(U(FindClose));
    char *d = dh_dir("fb_sparse"), p[MAX_PATH]; WIN32_FIND_DATAA fd;
    sub(p, sizeof p, d, "big.sparse");
    HANDLE f = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CHECK(f != INVALID_HANDLE_VALUE);
    LARGE_INTEGER pos; pos.QuadPart = 5LL * 1024 * 1024 * 1024 + 7;   /* sparse on NTFS only if flagged; set EOF only */
    BOOL ok = SetFilePointerEx(f, pos, NULL, FILE_BEGIN) && SetEndOfFile(f);
    CloseHandle(f);
    if (!ok) { t_skip("cannot extend file"); return; }
    HANDLE h = U(FindFirstFileA)(p, &fd);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) {
        EXPECT_EQ(fd.nFileSizeHigh, 1); EXPECT_EQ(fd.nFileSizeLow, (DWORD)(pos.QuadPart & 0xffffffff));
        U(FindClose)(h);
    }
    DeleteFileA(p);
}
TEST(find_attributes_reported) {
    REQUIRE(U(FindFirstFileA)); REQUIRE(U(FindClose));
    char *d = dh_dir("fb_attr"), p[MAX_PATH]; WIN32_FIND_DATAA fd;
    struct { const char *n; DWORD set; } t[] = {
        { "ro", FILE_ATTRIBUTE_READONLY }, { "hid", FILE_ATTRIBUTE_HIDDEN }, { "sys", FILE_ATTRIBUTE_SYSTEM },
        { "ro_hid", FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN }, { "arch", FILE_ATTRIBUTE_ARCHIVE },
        { "norm", FILE_ATTRIBUTE_NORMAL }, { "notidx", FILE_ATTRIBUTE_NOT_CONTENT_INDEXED }, { "temp", FILE_ATTRIBUTE_TEMPORARY } };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        dh_mkfile(d, t[i].n, 1);
        sub(p, sizeof p, d, t[i].n);
        SetFileAttributesA(p, t[i].set);
        HANDLE h = U(FindFirstFileA)(p, &fd);
        CHECKF(h != INVALID_HANDLE_VALUE, "%s", t[i].n);
        if (h == INVALID_HANDLE_VALUE) continue;
        DWORD want = t[i].set & ~FILE_ATTRIBUTE_NORMAL;
        if (t_is_wine() && (i == 3 || i == 5 || i == 6 || i == 7)) { t_note("%s attrs 0x%lx (emulator limits)", t[i].n, fd.dwFileAttributes); U(FindClose)(h); SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL); continue; }
        CHECKF((fd.dwFileAttributes & want) == want, "%s attrs 0x%lx want 0x%lx", t[i].n, fd.dwFileAttributes, want);
        if (t[i].set == FILE_ATTRIBUTE_NORMAL) CHECKF(fd.dwFileAttributes == FILE_ATTRIBUTE_NORMAL, "normal 0x%lx", fd.dwFileAttributes);
        U(FindClose)(h);
        SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL);
    }
}
TEST(find_hidden_and_system_files_are_listed) {
    REQUIRE(U(FindFirstFileA));
    char *d = dh_dir("fb_hidlist"), p[MAX_PATH]; dh_name l[8];
    dh_mkfile(d, "h", 1); dh_mkfile(d, "s", 1);
    sub(p, sizeof p, d, "h"); SetFileAttributesA(p, FILE_ATTRIBUTE_HIDDEN);
    sub(p, sizeof p, d, "s"); SetFileAttributesA(p, FILE_ATTRIBUTE_SYSTEM);
    sub(p, sizeof p, d, "*");
    int n = dh_listA_nd(p, l, 8);
    EXPECT_EQ(n, 2);
}
TEST(find_alt_name_empty_for_83_names) {
    REQUIRE(U(FindFirstFileA)); REQUIRE(U(FindClose));
    char *d = dh_dir("fb_alt83"), p[MAX_PATH]; WIN32_FIND_DATAA fd;
    dh_mkfile(d, "short.txt", 1); sub(p, sizeof p, d, "short.txt");
    HANDLE h = U(FindFirstFileA)(p, &fd);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) { EXPECT_STR(fd.cAlternateFileName, ""); U(FindClose)(h); }
}
TEST(find_alt_name_for_long_names) {
    REQUIRE(U(FindFirstFileA)); REQUIRE(U(FindClose));
    char *d = dh_dir("fb_altlong"), p[MAX_PATH]; WIN32_FIND_DATAA fd;
    dh_mkfile(d, "this is a very long file name.text", 1); sub(p, sizeof p, d, "this is a very long file name.text");
    HANDLE h = U(FindFirstFileA)(p, &fd);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    EXPECT_STR(fd.cFileName, "this is a very long file name.text");
    size_t al = strlen(fd.cAlternateFileName);
    if (al == 0) t_note("8.3 generation appears disabled on this volume");
    else {
        CHECK(al <= 12);
        CHECK(strchr(fd.cAlternateFileName, '~') != NULL);
        for (char *c = fd.cAlternateFileName; *c; c++) CHECK(!(*c >= 'a' && *c <= 'z'));
        /* the short name finds the same entry */
        char sp[MAX_PATH]; WIN32_FIND_DATAA f2;
        sub(sp, sizeof sp, d, fd.cAlternateFileName);
        HANDLE h2 = U(FindFirstFileA)(sp, &f2);
        CHECK(h2 != INVALID_HANDLE_VALUE);
        if (h2 != INVALID_HANDLE_VALUE) { EXPECT_STR(f2.cFileName, "this is a very long file name.text"); U(FindClose)(h2); }
    }
    U(FindClose)(h);
}
TEST(find_alt_name_w_matches_a) {
    REQUIRE(U(FindFirstFileA)); REQUIRE(U(FindFirstFileW)); REQUIRE(U(FindClose));
    char *d = dh_dir("fb_altpar"), p[MAX_PATH]; WIN32_FIND_DATAA fa; WIN32_FIND_DATAW fw; WCHAR w[MAX_PATH], conv[40];
    dh_mkfile(d, "Mixed Case Long Name.Document", 1); sub(p, sizeof p, d, "Mixed*");
    HANDLE ha = U(FindFirstFileA)(p, &fa); dh_w(w, MAX_PATH, p); HANDLE hw = U(FindFirstFileW)(w, &fw);
    CHECK(ha != INVALID_HANDLE_VALUE && hw != INVALID_HANDLE_VALUE);
    if (ha != INVALID_HANDLE_VALUE && hw != INVALID_HANDLE_VALUE) {
        dh_w(conv, 40, fa.cAlternateFileName);
        CHECK(!wcscmp(conv, fw.cAlternateFileName));
        dh_w(w, MAX_PATH, fa.cFileName);
        CHECK(!wcscmp(w, fw.cFileName));
        EXPECT_EQ(fa.dwFileAttributes, fw.dwFileAttributes);
    }
    if (ha != INVALID_HANDLE_VALUE) U(FindClose)(ha);
    if (hw != INVALID_HANDLE_VALUE) U(FindClose)(hw);
}
TEST(find_long_name_255) {
    REQUIRE(U(FindFirstFileW)); REQUIRE(U(FindClose));
    char *d = dh_dir("fb_n255"); WCHAR dw[MAX_PATH], w[MAX_PATH * 2], pat[MAX_PATH * 2]; WIN32_FIND_DATAW fd;
    dh_w(dw, MAX_PATH, d);
    int k = snwprintf(w, MAX_PATH * 2, L"\\\\?\\%ls\\", dw);
    for (int i = 0; i < 255; i++) w[k + i] = 'N';
    w[k + 255] = 0;
    HANDLE f = CreateFileW(w, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
    CHECK(f != INVALID_HANDLE_VALUE);
    if (f == INVALID_HANDLE_VALUE) return;
    CloseHandle(f);
    snwprintf(pat, MAX_PATH * 2, L"\\\\?\\%ls\\N*", dw);
    HANDLE h = U(FindFirstFileW)(pat, &fd);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) { EXPECT_EQ(wcslen(fd.cFileName), 255); U(FindClose)(h); }
    DeleteFileW(w);
}
TEST(find_long_name_a_255) {
    REQUIRE(U(FindFirstFileA)); REQUIRE(U(FindClose));
    char *d = dh_dir("fb_n255a"); WCHAR dw[MAX_PATH], w[MAX_PATH * 2]; WIN32_FIND_DATAA fd; char pat[MAX_PATH];
    dh_w(dw, MAX_PATH, d);
    int k = snwprintf(w, MAX_PATH * 2, L"\\\\?\\%ls\\", dw);
    for (int i = 0; i < 255; i++) w[k + i] = 'M';
    w[k + 255] = 0;
    HANDLE f = CreateFileW(w, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
    CHECK(f != INVALID_HANDLE_VALUE);
    if (f == INVALID_HANDLE_VALUE) return;
    CloseHandle(f);
    sub(pat, sizeof pat, d, "M*");
    HANDLE h = U(FindFirstFileA)(pat, &fd);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) { EXPECT_EQ(strlen(fd.cFileName), 255); U(FindClose)(h); }
    DeleteFileW(w);
}

/* ---- wildcard semantics (pattern conversion done by FindFirstFileExW) ---- */
#define WILD(nm, pat, ...) TEST(find_wild_##nm) { REQUIRE(U(FindFirstFileA)); const char *e[] = { __VA_ARGS__ NULL }; check_set(wild_dir(), pat, e); }
WILD(star,        "*", "a.txt", "ab.txt", "abc.txt", "a", "noext", "x.tar.gz", "b.TXT", "two words.txt", "a.b.c",)
WILD(star_dot_star, "*.*", "a.txt", "ab.txt", "abc.txt", "a", "noext", "x.tar.gz", "b.TXT", "two words.txt", "a.b.c",)
WILD(star_txt,    "*.txt", "a.txt", "ab.txt", "abc.txt", "b.TXT", "two words.txt",)
WILD(star_txt_upper, "*.TXT", "a.txt", "ab.txt", "abc.txt", "b.TXT", "two words.txt",)
WILD(a_star,      "a*", "a.txt", "ab.txt", "abc.txt", "a", "a.b.c",)
WILD(star_c,      "*c", "a.b.c",)
WILD(question,    "?", "a",)
WILD(two_question, "??", "a",)
WILD(a_q_txt,     "a?.txt", "a.txt", "ab.txt",)
WILD(a_qq_txt,    "a??.txt", "a.txt", "ab.txt", "abc.txt",)
WILD(qqq_txt,     "???.txt", "a.txt", "ab.txt", "abc.txt", "b.TXT",)
WILD(star_dot,    "*.", "a", "noext",)
WILD(star_dot_q,  "*.?", "a", "noext", "a.b.c",)
WILD(a_dot_star,  "a.*", "a", "a.txt", "a.b.c",)
TEST(find_wild_dot_star) {
    REQUIRE(U(FindFirstFileA));
    char *d = dh_dir("fb_wild2");
    dh_mkfile(d, ".hidden", 1); dh_mkfile(d, "plain", 1); dh_mkfile(d, "a.dot", 1);
    const char *e[] = { ".hidden", NULL }; check_set(d, ".*", e);
    const char *e2[] = { ".hidden", NULL }; check_set(d, "*.hidden", e2);
    const char *e3[] = { ".hidden", "plain", "a.dot", NULL }; check_set(d, "*", e3);
}
WILD(exact_ci,    "B.txt", "b.TXT",)
WILD(exact_space, "two words.txt", "two words.txt",)
WILD(prefix_space, "two *", "two words.txt",)
WILD(no_match,    "zz*")
WILD(dos_star,    "<.txt", "a.txt", "ab.txt", "abc.txt", "b.TXT", "two words.txt",)
WILD(dos_qm,      "a>.txt", "a.txt", "ab.txt",)
WILD(dos_dot,     "a\"txt", "a.txt",)
WILD(dos_star_gz, "<.gz", "x.tar.gz",)
WILD(trailing_space_pattern, "a.txt ", "a.txt",)
WILD(trailing_dot_pattern, "a.txt.", "a.txt",)
WILD(star_star,   "**", "a.txt", "ab.txt", "abc.txt", "a", "noext", "x.tar.gz", "b.TXT", "two words.txt", "a.b.c",)
WILD(multi_dot_ext, "*.c", "a.b.c",)
WILD(inner_star,  "a*c", "a.b.c",)
WILD(inner_star_txt, "a*t", "a.txt", "ab.txt", "abc.txt",)
WILD(x_tar,       "x.tar.*", "x.tar.gz",)
WILD(question_dot_question, "?.?", "a",)
