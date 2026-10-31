/* FindFirstFileEx{A,W}: parameter validation, info levels, flags, devices, reparse points, large trees, unicode */
#include "dh.h"

static char *sub(char *buf, size_t cap, const char *dir, const char *leaf) { dh_path(buf, cap, dir, leaf); return buf; }

TEST(findex_invalid_info_level) {
    REQUIRE(U(FindFirstFileExW)); REQUIRE(U(FindFirstFileExA));
    char *d = dh_dir("fx_badlevel"), p[MAX_PATH]; WCHAR w[MAX_PATH]; WIN32_FIND_DATAW fw; WIN32_FIND_DATAA fa;
    dh_mkfile(d, "f", 1); sub(p, sizeof p, d, "*"); dh_w(w, MAX_PATH, p);
    int lv[] = { 2, 3, 7, 0xffff, -1 };
    for (unsigned i = 0; i < sizeof lv / sizeof *lv; i++) {
        SET_ERR(0);
        CHECKF(U(FindFirstFileExW)(w, (FINDEX_INFO_LEVELS)lv[i], &fw, FindExSearchNameMatch, NULL, 0) == INVALID_HANDLE_VALUE, "W level %d", lv[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        SET_ERR(0);
        CHECKF(U(FindFirstFileExA)(p, (FINDEX_INFO_LEVELS)lv[i], &fa, FindExSearchNameMatch, NULL, 0) == INVALID_HANDLE_VALUE, "A level %d", lv[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(findex_invalid_search_op) {
    REQUIRE(U(FindFirstFileExW));
    char *d = dh_dir("fx_badop"), p[MAX_PATH]; WCHAR w[MAX_PATH]; WIN32_FIND_DATAW fw;
    dh_mkfile(d, "f", 1); sub(p, sizeof p, d, "*"); dh_w(w, MAX_PATH, p);
    int ops[] = { 3, 4, 100, -1 };
    for (unsigned i = 0; i < sizeof ops / sizeof *ops; i++) {
        SET_ERR(0);
        CHECKF(U(FindFirstFileExW)(w, FindExInfoStandard, &fw, (FINDEX_SEARCH_OPS)ops[i], NULL, 0) == INVALID_HANDLE_VALUE, "op %d", ops[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(findex_limit_to_devices_not_supported) {
    REQUIRE(U(FindFirstFileExW));
    char *d = dh_dir("fx_devop"), p[MAX_PATH]; WCHAR w[MAX_PATH]; WIN32_FIND_DATAW fw;
    dh_mkfile(d, "f", 1); sub(p, sizeof p, d, "*"); dh_w(w, MAX_PATH, p);
    SET_ERR(0);
    CHECK(U(FindFirstFileExW)(w, FindExInfoStandard, &fw, FindExSearchLimitToDevices, NULL, 0) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_NOT_SUPPORTED);
    SET_ERR(0);
    CHECK(U(FindFirstFileExW)(w, FindExInfoBasic, &fw, FindExSearchLimitToDevices, NULL, FIND_FIRST_EX_LARGE_FETCH) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_NOT_SUPPORTED);
}
TEST(findex_invalid_flags) {
    REQUIRE(U(FindFirstFileExW));
    char *d = dh_dir("fx_badflags"), p[MAX_PATH]; WCHAR w[MAX_PATH]; WIN32_FIND_DATAW fw;
    dh_mkfile(d, "f", 1); sub(p, sizeof p, d, "*"); dh_w(w, MAX_PATH, p);
    DWORD fl[] = { 8, 0x10, 0x100, 0x80000000, 0xffffffff, 0x1000 };
    for (unsigned i = 0; i < sizeof fl / sizeof *fl; i++) {
        SET_ERR(0);
        CHECKF(U(FindFirstFileExW)(w, FindExInfoStandard, &fw, FindExSearchNameMatch, NULL, fl[i]) == INVALID_HANDLE_VALUE, "flags 0x%lx", fl[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(findex_valid_flag_combinations) {
    REQUIRE(U(FindFirstFileExW)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_okflags"), p[MAX_PATH]; WCHAR w[MAX_PATH]; WIN32_FIND_DATAW fw;
    dh_mkfile(d, "f", 1); sub(p, sizeof p, d, "*"); dh_w(w, MAX_PATH, p);
    for (DWORD fl = 0; fl <= 7; fl++) {
        HANDLE h = U(FindFirstFileExW)(w, FindExInfoStandard, &fw, FindExSearchNameMatch, NULL, fl);
        CHECKF(h != INVALID_HANDLE_VALUE, "flags %lu err %lu", fl, GetLastError());
        if (h != INVALID_HANDLE_VALUE) U(FindClose)(h);
    }
}
TEST(findex_invalid_params_do_not_touch_output) {
    REQUIRE(U(FindFirstFileExW));
    WIN32_FIND_DATAW fw; memset(&fw, 0xEE, sizeof fw);
    CHECK(U(FindFirstFileExW)(L"C:\\*", (FINDEX_INFO_LEVELS)9, &fw, FindExSearchNameMatch, NULL, 0) == INVALID_HANDLE_VALUE);
    CHECK(fw.dwFileAttributes == 0xEEEEEEEE && fw.cFileName[0] == 0xEEEE);
}
TEST(findex_basic_info_no_alt_name) {
    REQUIRE(U(FindFirstFileExW)); REQUIRE(U(FindFirstFileExA)); REQUIRE(U(FindNextFileW)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_basic"), p[MAX_PATH]; WCHAR w[MAX_PATH]; WIN32_FIND_DATAW fw; WIN32_FIND_DATAA fa;
    dh_mkfile(d, "a really long file name for short names.document", 5);
    sub(p, sizeof p, d, "a really*"); dh_w(w, MAX_PATH, p);
    memset(&fw, 0xAB, sizeof fw);
    HANDLE h = U(FindFirstFileExW)(w, FindExInfoBasic, &fw, FindExSearchNameMatch, NULL, 0);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) {
        CHECK(fw.cAlternateFileName[0] == 0);
        CHECK(!wcscmp(fw.cFileName, L"a really long file name for short names.document"));
        EXPECT_EQ(fw.nFileSizeLow, 5);
        U(FindClose)(h);
    }
    memset(&fa, 0xAB, sizeof fa);
    h = U(FindFirstFileExA)(p, FindExInfoBasic, &fa, FindExSearchNameMatch, NULL, 0);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) { CHECK(fa.cAlternateFileName[0] == 0); EXPECT_STR(fa.cFileName, "a really long file name for short names.document"); U(FindClose)(h); }
}
TEST(findex_basic_info_next_entries) {
    REQUIRE(U(FindFirstFileExW)); REQUIRE(U(FindNextFileW)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_basic2"), p[MAX_PATH]; WCHAR w[MAX_PATH]; WIN32_FIND_DATAW fw;
    for (int i = 0; i < 10; i++) { char n[64]; snprintf(n, sizeof n, "long file name number %d.txtx", i); dh_mkfile(d, n, i); }
    sub(p, sizeof p, d, "long*"); dh_w(w, MAX_PATH, p);
    HANDLE h = U(FindFirstFileExW)(w, FindExInfoBasic, &fw, FindExSearchNameMatch, NULL, 0);
    CHECK(h != INVALID_HANDLE_VALUE);
    int n = 0, noalt = 0;
    if (h != INVALID_HANDLE_VALUE) {
        do { n++; if (!fw.cAlternateFileName[0]) noalt++; } while (U(FindNextFileW)(h, &fw));
        EXPECT_ERR(ERROR_NO_MORE_FILES);
        U(FindClose)(h);
    }
    EXPECT_EQ(n, 10); EXPECT_EQ(noalt, 10);
}
TEST(findex_standard_vs_basic_same_names) {
    REQUIRE(U(FindFirstFileExW)); REQUIRE(U(FindNextFileW)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_samenames"), p[MAX_PATH]; WCHAR w[MAX_PATH]; WIN32_FIND_DATAW fw;
    for (int i = 0; i < 30; i++) { char n[64]; snprintf(n, sizeof n, "Entry Number %02d.data", i); dh_mkfile(d, n, i); }
    sub(p, sizeof p, d, "*"); dh_w(w, MAX_PATH, p);
    WCHAR *names[2]; int cnt[2];
    for (int k = 0; k < 2; k++) {
        names[k] = calloc(40, 300 * sizeof(WCHAR)); cnt[k] = 0;
        HANDLE h = U(FindFirstFileExW)(w, k ? FindExInfoBasic : FindExInfoStandard, &fw, FindExSearchNameMatch, NULL, 0);
        CHECK(h != INVALID_HANDLE_VALUE);
        if (h == INVALID_HANDLE_VALUE) continue;
        do { if (cnt[k] < 40) wcscpy(names[k] + 300 * cnt[k], fw.cFileName); cnt[k]++; } while (U(FindNextFileW)(h, &fw));
        U(FindClose)(h);
    }
    EXPECT_EQ(cnt[0], 32); EXPECT_EQ(cnt[1], 32);
    for (int i = 0; i < 32 && i < 40; i++) CHECKF(!wcscmp(names[0] + 300 * i, names[1] + 300 * i), "idx %d", i);
    free(names[0]); free(names[1]);
}
TEST(findex_limit_to_directories_is_advisory) {
    REQUIRE(U(FindFirstFileExW)); REQUIRE(U(FindNextFileW)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_limdir"), p[MAX_PATH]; WCHAR w[MAX_PATH]; WIN32_FIND_DATAW fw;
    dh_mkdir(d, "sub1"); dh_mkdir(d, "sub2"); dh_mkfile(d, "f1", 1); dh_mkfile(d, "f2", 1);
    sub(p, sizeof p, d, "*"); dh_w(w, MAX_PATH, p);
    SET_ERR(0);
    HANDLE h = U(FindFirstFileExW)(w, FindExInfoStandard, &fw, FindExSearchLimitToDirectories, NULL, 0);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    int dirs = 0, files = 0;
    do { if (fw.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) dirs++; else files++; } while (U(FindNextFileW)(h, &fw));
    U(FindClose)(h);
    CHECK(dirs >= 4);     /* ., .., sub1, sub2 are always present */
    t_note("LimitToDirectories: %d directory entries, %d file entries (filter is advisory; files are returned on NTFS)", dirs, files);
    EXPECT_EQ(files, 2);
}
TEST(findex_limit_to_directories_with_filter_pointer) {
    REQUIRE(U(FindFirstFileExW)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_limdir2"), p[MAX_PATH]; WCHAR w[MAX_PATH]; WIN32_FIND_DATAW fw;
    dh_mkdir(d, "s"); sub(p, sizeof p, d, "*"); dh_w(w, MAX_PATH, p);
    SET_ERR(0);
    HANDLE h = U(FindFirstFileExW)(w, FindExInfoStandard, &fw, FindExSearchLimitToDirectories, (LPVOID)w, 0);
    t_note("LimitToDirectories with non-NULL filter: handle=%p err=%lu", h, GetLastError());
    if (h != INVALID_HANDLE_VALUE) U(FindClose)(h);
}
TEST(findex_name_match_ignores_filter) {
    REQUIRE(U(FindFirstFileExW)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_namematch"), p[MAX_PATH]; WCHAR w[MAX_PATH]; WIN32_FIND_DATAW fw;
    dh_mkfile(d, "q", 1); sub(p, sizeof p, d, "*"); dh_w(w, MAX_PATH, p);
    HANDLE h = U(FindFirstFileExW)(w, FindExInfoStandard, &fw, FindExSearchNameMatch, NULL, 0);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) U(FindClose)(h);
}
TEST(findex_exa_matches_findfirstfilea) {
    REQUIRE(U(FindFirstFileExA)); REQUIRE(U(FindNextFileA)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_exa"), p[MAX_PATH]; dh_name l1[16], l2[16];
    for (int i = 0; i < 6; i++) { char n[16]; snprintf(n, sizeof n, "e%d.bin", i); dh_mkfile(d, n, i); }
    sub(p, sizeof p, d, "*.bin");
    int n1 = dh_listA(p, l1, 16), n2 = 0;
    WIN32_FIND_DATAA fa;
    HANDLE h = U(FindFirstFileExA)(p, FindExInfoStandard, &fa, FindExSearchNameMatch, NULL, 0);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) { do { if (n2 < 16) l2[n2] = (dh_name){{0}}, strcpy(l2[n2].n, fa.cFileName); n2++; } while (U(FindNextFileA)(h, &fa)); U(FindClose)(h); }
    EXPECT_EQ(n1, 6); EXPECT_EQ(n2, 6);
    for (int i = 0; i < 6; i++) CHECK(dh_has(l2, n2, l1[i].n));
}
TEST(findex_large_fetch_same_results) {
    REQUIRE(U(FindFirstFileExW)); REQUIRE(U(FindNextFileW)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_large"), p[MAX_PATH]; WCHAR w[MAX_PATH]; WIN32_FIND_DATAW fw;
    for (int i = 0; i < 600; i++) { char n[32]; snprintf(n, sizeof n, "lf_%04d.dat", i); dh_mkfile(d, n, 0); }
    sub(p, sizeof p, d, "*"); dh_w(w, MAX_PATH, p);
    int counts[8];
    for (DWORD fl = 0; fl < 8; fl++) {
        int n = 0;
        HANDLE h = U(FindFirstFileExW)(w, FindExInfoBasic, &fw, FindExSearchNameMatch, NULL, fl);
        if (h == INVALID_HANDLE_VALUE) { CHECKF(0, "flags %lu err %lu", fl, GetLastError()); counts[fl] = -1; continue; }
        do n++; while (U(FindNextFileW)(h, &fw));
        EXPECT_ERR(ERROR_NO_MORE_FILES);
        U(FindClose)(h);
        counts[fl] = n;
        EXPECT_EQ(n, 602);
    }
}
TEST(findex_case_sensitive_flag_exact_case) {
    REQUIRE(U(FindFirstFileExW)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_cs"), p[MAX_PATH]; WCHAR w[MAX_PATH]; WIN32_FIND_DATAW fw;
    dh_mkfile(d, "CaseName.txt", 1);
    sub(p, sizeof p, d, "CaseName.txt"); dh_w(w, MAX_PATH, p);
    HANDLE h = U(FindFirstFileExW)(w, FindExInfoStandard, &fw, FindExSearchNameMatch, NULL, FIND_FIRST_EX_CASE_SENSITIVE);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) U(FindClose)(h);
    sub(p, sizeof p, d, "casename.TXT"); dh_w(w, MAX_PATH, p);
    SET_ERR(0);
    h = U(FindFirstFileExW)(w, FindExInfoStandard, &fw, FindExSearchNameMatch, NULL, FIND_FIRST_EX_CASE_SENSITIVE);
    t_note("FIND_FIRST_EX_CASE_SENSITIVE with different-case pattern on a case-insensitive directory: %s (err %lu)", h != INVALID_HANDLE_VALUE ? "found" : "not found", GetLastError());
    if (h != INVALID_HANDLE_VALUE) U(FindClose)(h);
}
TEST(findex_case_insensitive_default) {
    REQUIRE(U(FindFirstFileExW)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_ci"), p[MAX_PATH]; WCHAR w[MAX_PATH]; WIN32_FIND_DATAW fw;
    dh_mkfile(d, "CaseName.txt", 1);
    sub(p, sizeof p, d, "CASENAME.TXT"); dh_w(w, MAX_PATH, p);
    HANDLE h = U(FindFirstFileExW)(w, FindExInfoStandard, &fw, FindExSearchNameMatch, NULL, 0);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) { CHECK(!wcscmp(fw.cFileName, L"CaseName.txt")); U(FindClose)(h); }
}
TEST(findex_device_nul) {
    REQUIRE(U(FindFirstFileExW)); REQUIRE(U(FindNextFileW)); REQUIRE(U(FindClose));
    WIN32_FIND_DATAW fw; memset(&fw, 0x77, sizeof fw);
    SET_ERR(0);
    HANDLE h = U(FindFirstFileExW)(L"NUL", FindExInfoStandard, &fw, FindExSearchNameMatch, NULL, 0);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) { t_note("FindFirstFile(NUL) err %lu", GetLastError()); return; }
    CHECK(!_wcsicmp(fw.cFileName, L"NUL"));
    EXPECT_EQ(fw.dwFileAttributes, FILE_ATTRIBUTE_ARCHIVE);
    EXPECT_EQ(fw.nFileSizeLow, 0); EXPECT_EQ(fw.nFileSizeHigh, 0);
    EXPECT_EQ(fw.ftCreationTime.dwLowDateTime, 0); EXPECT_EQ(fw.ftLastWriteTime.dwHighDateTime, 0);
    CHECK(fw.cAlternateFileName[0] == 0);
    SET_ERR(0);
    CHECK(!U(FindNextFileW)(h, &fw));
    EXPECT_ERR(ERROR_NO_MORE_FILES);
    CHECK(U(FindClose)(h));
}
TEST(findex_device_in_directory_path) {
    REQUIRE(U(FindFirstFileW)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_devpath"), p[MAX_PATH]; WCHAR w[MAX_PATH]; WIN32_FIND_DATAW fw;
    sub(p, sizeof p, d, "nul"); dh_w(w, MAX_PATH, p);
    HANDLE h = U(FindFirstFileW)(w, &fw);
    t_note("FindFirstFile(<dir>\\nul): %s name=%ls", h != INVALID_HANDLE_VALUE ? "device entry" : "failed", h != INVALID_HANDLE_VALUE ? fw.cFileName : L"-");
    if (h != INVALID_HANDLE_VALUE) { CHECK(fw.dwFileAttributes == FILE_ATTRIBUTE_ARCHIVE); U(FindClose)(h); }
}
TEST(findex_device_con_a) {
    REQUIRE(U(FindFirstFileA)); REQUIRE(U(FindClose));
    WIN32_FIND_DATAA fa;
    HANDLE h = U(FindFirstFileA)("CON", &fa);
    t_note("FindFirstFileA(CON): %s", h != INVALID_HANDLE_VALUE ? fa.cFileName : "failed");
    if (h != INVALID_HANDLE_VALUE) { CHECK(!lstrcmpiA(fa.cFileName, "CON")); U(FindClose)(h); }
}
TEST(findex_device_close_is_idempotent_sentinel) {
    REQUIRE(U(FindFirstFileW)); REQUIRE(U(FindClose));
    WIN32_FIND_DATAW fw;
    HANDLE h = U(FindFirstFileW)(L"NUL", &fw);
    if (h == INVALID_HANDLE_VALUE) { t_skip("no device find"); return; }
    if (h == (HANDLE)1) {            /* sentinel handle: FindClose never frees it */
        CHECK(U(FindClose)(h)); CHECK(U(FindClose)(h));
    } else { CHECK(U(FindClose)(h)); }
}
TEST(findex_junction_reparse_tag) {
    REQUIRE(U(FindFirstFileA)); REQUIRE(U(FindNextFileA)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_junc"), t[MAX_PATH], l[MAX_PATH], p[MAX_PATH]; WIN32_FIND_DATAA fd;
    sub(t, sizeof t, d, "target"); CreateDirectoryA(t, NULL);
    sub(l, sizeof l, d, "link"); CreateDirectoryA(l, NULL);
    if (!dh_mkjunction(l, t)) { t_skip("cannot create junction"); return; }
    sub(p, sizeof p, d, "link");
    HANDLE h = U(FindFirstFileA)(p, &fd);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) {
        CHECK(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT);
        CHECK(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
        EXPECT_EQ(fd.dwReserved0, 0xA0000003);     /* IO_REPARSE_TAG_MOUNT_POINT */
        U(FindClose)(h);
    }
    sub(p, sizeof p, d, "*");
    h = U(FindFirstFileA)(p, &fd);
    int seen = 0;
    if (h != INVALID_HANDLE_VALUE) {
        do { if (!strcmp(fd.cFileName, "link")) { seen = 1; EXPECT_EQ(fd.dwReserved0, 0xA0000003); }
             if (!strcmp(fd.cFileName, "target")) { CHECK(!(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)); } } while (U(FindNextFileA)(h, &fd));
        U(FindClose)(h);
    }
    CHECK(seen);
    /* enumerating through the junction lists the target's content */
    dh_mkfile(t, "inside.txt", 3);
    snprintf(p, sizeof p, "%s\\link\\*", d);
    dh_name lst[8]; int n = dh_listA_nd(p, lst, 8);
    EXPECT_EQ(n, 1); CHECK(dh_has(lst, n, "inside.txt"));
    DeleteFileA(strcat(strcpy(p, t), "\\inside.txt"));
    RemoveDirectoryA(l);
}
TEST(findex_symlink_reparse_tag) {
    REQUIRE(U(FindFirstFileA)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_sym"), t[MAX_PATH], l[MAX_PATH], tf[MAX_PATH], lf[MAX_PATH]; WIN32_FIND_DATAA fd;
    sub(t, sizeof t, d, "tdir"); CreateDirectoryA(t, NULL);
    sub(l, sizeof l, d, "ldir");
    if (!dh_mksymlink(l, t, 1)) { t_skip("cannot create symlink (needs developer mode or privilege)"); return; }
    HANDLE h = U(FindFirstFileA)(l, &fd);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) {
        CHECK(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT);
        CHECK(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
        EXPECT_EQ(fd.dwReserved0, 0xA000000C);
        U(FindClose)(h);
    }
    dh_mkfile(d, "tfile", 4);
    sub(tf, sizeof tf, d, "tfile"); sub(lf, sizeof lf, d, "lfile");
    if (dh_mksymlink(lf, tf, 0)) {
        h = U(FindFirstFileA)(lf, &fd);
        CHECK(h != INVALID_HANDLE_VALUE);
        if (h != INVALID_HANDLE_VALUE) {
            CHECK(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT);
            CHECK(!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));
            EXPECT_EQ(fd.dwReserved0, 0xA000000C);
            U(FindClose)(h);
        }
    }
}
TEST(findex_dangling_symlink_listed) {
    REQUIRE(U(FindFirstFileA));
    char *d = dh_dir("fx_dangle"), t[MAX_PATH], l[MAX_PATH]; WIN32_FIND_DATAA fd;
    sub(t, sizeof t, d, "gone"); sub(l, sizeof l, d, "dangler");
    if (!dh_mksymlink(l, t, 0)) { t_skip("cannot create symlink"); return; }
    HANDLE h = U(FindFirstFileA)(l, &fd);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) { EXPECT_EQ(fd.dwReserved0, 0xA000000C); U(FindClose)(h); }
}
TEST(findex_plain_dir_no_reparse_bit) {
    REQUIRE(U(FindFirstFileA)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_noreparse"), p[MAX_PATH]; WIN32_FIND_DATAA fd;
    dh_mkdir(d, "plain");
    sub(p, sizeof p, d, "plain");
    HANDLE h = U(FindFirstFileA)(p, &fd);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) { CHECK(!(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)); U(FindClose)(h); }
}

/* ---- trees ---- */
#define ND 40
#define NF 49
TEST(findex_tree_2000_entries_w) {
    REQUIRE(U(FindFirstFileExW)); REQUIRE(U(FindNextFileW)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_tree"), p[MAX_PATH]; WCHAR w[MAX_PATH]; WIN32_FIND_DATAW fw;
    for (int i = 0; i < ND; i++) {
        char dn[16]; snprintf(dn, sizeof dn, "D%02d", i);
        if (!dh_mkdir(d, dn)) { CHECKF(0, "mkdir %s", dn); return; }
        char sd[MAX_PATH]; sub(sd, sizeof sd, d, dn);
        for (int j = 0; j < NF; j++) { char fn[16]; snprintf(fn, sizeof fn, "F%02d.dat", j); dh_mkfile(sd, fn, j); }
    }
    static unsigned char seen[ND][NF]; memset(seen, 0, sizeof seen);
    int dirs = 0, files = 0, bad = 0, dots = 0;
    sub(p, sizeof p, d, "*"); dh_w(w, MAX_PATH, p);
    HANDLE h = U(FindFirstFileExW)(w, FindExInfoBasic, &fw, FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!wcscmp(fw.cFileName, L".") || !wcscmp(fw.cFileName, L"..")) { dots++; continue; }
        int di = -1;
        if (swscanf(fw.cFileName, L"D%d", &di) != 1 || di < 0 || di >= ND || !(fw.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) { bad++; continue; }
        dirs++;
        WCHAR sp[MAX_PATH]; WCHAR dw[MAX_PATH]; dh_w(dw, MAX_PATH, d);
        snwprintf(sp, MAX_PATH, L"%ls\\%ls\\*", dw, fw.cFileName);
        WIN32_FIND_DATAW f2;
        HANDLE h2 = U(FindFirstFileExW)(sp, FindExInfoBasic, &f2, FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
        if (h2 == INVALID_HANDLE_VALUE) { bad++; continue; }
        do {
            if (!wcscmp(f2.cFileName, L".") || !wcscmp(f2.cFileName, L"..")) continue;
            int fi = -1;
            if (swscanf(f2.cFileName, L"F%d.dat", &fi) != 1 || fi < 0 || fi >= NF) { bad++; continue; }
            if (seen[di][fi]) bad++;
            seen[di][fi] = 1; files++;
            if (f2.nFileSizeLow != (DWORD)fi) bad++;
        } while (U(FindNextFileW)(h2, &f2));
        U(FindClose)(h2);
    } while (U(FindNextFileW)(h, &fw));
    U(FindClose)(h);
    EXPECT_EQ(dirs, ND); EXPECT_EQ(files, ND * NF); EXPECT_EQ(bad, 0); EXPECT_EQ(dots, 2);
    EXPECT_EQ(dirs + files, 2000);
}
static int walk_a(const char *dir, int *dirs, int *files, int depth) {
    char pat[MAX_PATH * 2]; WIN32_FIND_DATAA fd; int bad = 0;
    snprintf(pat, sizeof pat, "%s\\*", dir);
    HANDLE h = U(FindFirstFileA)(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return 1;
    do {
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            (*dirs)++;
            char sub2[MAX_PATH * 2]; snprintf(sub2, sizeof sub2, "%s\\%s", dir, fd.cFileName);
            if (depth < 8) bad += walk_a(sub2, dirs, files, depth + 1);
        } else (*files)++;
    } while (U(FindNextFileA)(h, &fd));
    U(FindClose)(h);
    return bad;
}
TEST(findex_tree_walk_a_recursive) {
    REQUIRE(U(FindFirstFileA)); REQUIRE(U(FindNextFileA)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_treea"), p[MAX_PATH];
    int expd = 0, expf = 0;
    for (int i = 0; i < 6; i++) {
        char a[16]; snprintf(a, sizeof a, "a%d", i); dh_mkdir(d, a); expd++;
        char ap[MAX_PATH]; sub(ap, sizeof ap, d, a);
        for (int j = 0; j < 5; j++) {
            char b[16]; snprintf(b, sizeof b, "b%d", j); dh_mkdir(ap, b); expd++;
            char bp[MAX_PATH]; sub(bp, sizeof bp, ap, b);
            for (int k = 0; k < 4; k++) { char f[16]; snprintf(f, sizeof f, "f%d", k); dh_mkfile(bp, f, 1); expf++; }
            for (int k = 0; k < 3; k++) { char c[16]; snprintf(c, sizeof c, "c%d", k); dh_mkdir(bp, c); expd++; dh_mkfile(bp, "c0\\x", 1); }
        }
        dh_mkfile(ap, "top", 1); expf++;
    }
    expf += 5 * 6 * 0;
    int dirs = 0, files = 0;
    int bad = walk_a(d, &dirs, &files, 0);
    EXPECT_EQ(bad, 0); EXPECT_EQ(dirs, expd);
    CHECKF(files >= expf, "files %d expected at least %d", files, expf);
}
TEST(findex_single_dir_1500_files_a) {
    REQUIRE(U(FindFirstFileA)); REQUIRE(U(FindNextFileA)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_1500"), p[MAX_PATH];
    for (int i = 0; i < 1500; i++) { char n[32]; snprintf(n, sizeof n, "file_%05d.t", i); dh_mkfile(d, n, 0); }
    sub(p, sizeof p, d, "*");
    dh_name *l = malloc(sizeof(dh_name) * 1600);
    int n = dh_listA_nd(p, l, 1600);
    EXPECT_EQ(n, 1500);
    static unsigned char seen[1500]; memset(seen, 0, sizeof seen); int dup = 0, bad = 0;
    for (int i = 0; i < n && i < 1600; i++) { int v; if (sscanf(l[i].n, "file_%d.t", &v) != 1 || v < 0 || v >= 1500) { bad++; continue; } if (seen[v]) dup++; seen[v] = 1; }
    EXPECT_EQ(dup, 0); EXPECT_EQ(bad, 0);
    /* NTFS returns names in collation order; this is not a documented guarantee */
    int sorted = 1; for (int i = 1; i < n && i < 1600; i++) if (lstrcmpiA(l[i - 1].n, l[i].n) > 0) sorted = 0;
    t_note("enumeration order is %s", sorted ? "sorted (case-insensitive)" : "not sorted");
    free(l);
}
TEST(findex_order_dots_first) {
    REQUIRE(U(FindFirstFileA)); REQUIRE(U(FindNextFileA)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_order"), p[MAX_PATH]; WIN32_FIND_DATAA fd;
    dh_mkfile(d, "zzz", 0); dh_mkfile(d, "aaa", 0); dh_mkfile(d, "_under", 0); dh_mkfile(d, "!bang", 0);
    sub(p, sizeof p, d, "*");
    HANDLE h = U(FindFirstFileA)(p, &fd);
    CHECK(h != INVALID_HANDLE_VALUE);
    char order[256] = ""; int n = 0;
    if (h != INVALID_HANDLE_VALUE) { do { strcat(order, fd.cFileName); strcat(order, ","); n++; } while (U(FindNextFileA)(h, &fd)); U(FindClose)(h); }
    EXPECT_EQ(n, 6);
    t_note("order on this file system: %s", order);
    CHECK(strstr(order, ".,") == order);     /* "." comes first on NTFS */
}
TEST(findex_delete_during_enumeration) {
    REQUIRE(U(FindFirstFileA)); REQUIRE(U(FindNextFileA)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_delenum"), p[MAX_PATH]; WIN32_FIND_DATAA fd;
    for (int i = 0; i < 50; i++) { char n[16]; snprintf(n, sizeof n, "x%02d", i); dh_mkfile(d, n, 0); }
    sub(p, sizeof p, d, "x*");
    HANDLE h = U(FindFirstFileA)(p, &fd);
    CHECK(h != INVALID_HANDLE_VALUE);
    int n = 1;
    char q[MAX_PATH];
    DeleteFileA(sub(q, sizeof q, d, fd.cFileName));       /* delete the file just returned */
    while (U(FindNextFileA)(h, &fd)) { n++; DeleteFileA(sub(q, sizeof q, d, fd.cFileName)); }
    EXPECT_ERR(ERROR_NO_MORE_FILES);
    U(FindClose)(h);
    EXPECT_EQ(n, 50);
    dh_name l[4]; sub(p, sizeof p, d, "*");
    EXPECT_EQ(dh_listA_nd(p, l, 4), 0);
}
TEST(findex_create_during_enumeration_no_crash) {
    REQUIRE(U(FindFirstFileA)); REQUIRE(U(FindNextFileA)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_createenum"), p[MAX_PATH]; WIN32_FIND_DATAA fd;
    for (int i = 0; i < 20; i++) { char n[16]; snprintf(n, sizeof n, "m%02d", i); dh_mkfile(d, n, 0); }
    sub(p, sizeof p, d, "*");
    HANDLE h = U(FindFirstFileA)(p, &fd);
    CHECK(h != INVALID_HANDLE_VALUE);
    int n = 1;
    while (U(FindNextFileA)(h, &fd)) { n++; if (n == 5) for (int i = 0; i < 20; i++) { char nn[16]; snprintf(nn, sizeof nn, "n%02d", i); dh_mkfile(d, nn, 0); } if (n > 200) break; }
    U(FindClose)(h);
    CHECKF(n >= 22 && n <= 42, "n=%d", n);
    t_note("entries seen when 20 files were added mid-enumeration: %d (22 baseline)", n);
}
TEST(findex_rename_during_enumeration) {
    REQUIRE(U(FindFirstFileA)); REQUIRE(U(FindNextFileA)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_renenum"), p[MAX_PATH], a[MAX_PATH], b[MAX_PATH]; WIN32_FIND_DATAA fd;
    for (int i = 0; i < 10; i++) { char n[16]; snprintf(n, sizeof n, "r%02d", i); dh_mkfile(d, n, 0); }
    sub(p, sizeof p, d, "*");
    HANDLE h = U(FindFirstFileA)(p, &fd);
    int n = 1;
    MoveFileA(sub(a, sizeof a, d, "r00"), sub(b, sizeof b, d, "renamed"));
    while (U(FindNextFileA)(h, &fd)) n++;
    U(FindClose)(h);
    CHECKF(n >= 11 && n <= 13, "n=%d", n);
}
TEST(findex_handle_leak_loop) {
    REQUIRE(U(FindFirstFileA)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_leak"), p[MAX_PATH]; WIN32_FIND_DATAA fd; DWORD c0, c1;
    dh_mkfile(d, "a", 1); sub(p, sizeof p, d, "*");
    GetProcessHandleCount(GetCurrentProcess(), &c0);
    for (int i = 0; i < 3000; i++) { HANDLE h = U(FindFirstFileA)(p, &fd); if (h == INVALID_HANDLE_VALUE) { CHECKF(0, "iter %d", i); break; } U(FindClose)(h); }
    GetProcessHandleCount(GetCurrentProcess(), &c1);
    CHECKF(c1 <= c0 + 2, "handle count grew %lu -> %lu", c0, c1);
    for (int i = 0; i < 3000; i++) { HANDLE h = U(FindFirstFileA)(p, &fd); if (h == INVALID_HANDLE_VALUE) break; U(FindNextFileA)(h, &fd); U(FindClose)(h); }
    GetProcessHandleCount(GetCurrentProcess(), &c1);
    CHECKF(c1 <= c0 + 2, "handle count grew %lu -> %lu", c0, c1);
}
TEST(findex_many_open_handles) {
    REQUIRE(U(FindFirstFileA)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_many"), p[MAX_PATH]; WIN32_FIND_DATAA fd; HANDLE hs[200];
    dh_mkfile(d, "a", 1); sub(p, sizeof p, d, "*");
    int i;
    for (i = 0; i < 200; i++) { hs[i] = U(FindFirstFileA)(p, &fd); if (hs[i] == INVALID_HANDLE_VALUE) break; }
    EXPECT_EQ(i, 200);
    for (int j = 0; j < i; j++) CHECK(U(FindClose)(hs[j]));
}
TEST(findex_pattern_with_wildcard_directory) {
    REQUIRE(U(FindFirstFileA));
    char *d = dh_dir("fx_wilddir"), p[MAX_PATH]; WIN32_FIND_DATAA fd;
    dh_mkdir(d, "sub"); dh_mkfile(d, "sub\\x", 1);
    sub(p, sizeof p, d, "s*\\x");
    SET_ERR(0);
    HANDLE h = U(FindFirstFileA)(p, &fd);
    CHECK(h == INVALID_HANDLE_VALUE);
    DWORD e = GetLastError();
    t_note("wildcard in directory component: err %lu", e);
    CHECKF(e == ERROR_INVALID_NAME || e == ERROR_PATH_NOT_FOUND || e == ERROR_FILE_NOT_FOUND, "err %lu", e);
    if (h != INVALID_HANDLE_VALUE) U(FindClose)(h);
}
TEST(findex_file_as_directory) {
    REQUIRE(U(FindFirstFileA));
    char *d = dh_dir("fx_filedir"), p[MAX_PATH]; WIN32_FIND_DATAA fd;
    dh_mkfile(d, "afile", 1);
    sub(p, sizeof p, d, "afile\\*");
    SET_ERR(0);
    HANDLE h = U(FindFirstFileA)(p, &fd);
    CHECK(h == INVALID_HANDLE_VALUE);
    DWORD e = GetLastError();
    t_note("file used as directory in pattern: err %lu", e);
    CHECKF(e == ERROR_PATH_NOT_FOUND || e == ERROR_DIRECTORY || e == ERROR_FILE_NOT_FOUND, "err %lu", e);
    if (h != INVALID_HANDLE_VALUE) U(FindClose)(h);
}
TEST(findex_raw_prefix_pattern) {
    REQUIRE(U(FindFirstFileW)); REQUIRE(U(FindNextFileW)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_rawpat"); WCHAR dw[MAX_PATH], pat[MAX_PATH + 8]; WIN32_FIND_DATAW fw;
    dh_mkfile(d, "r1", 1); dh_mkfile(d, "r2", 1);
    dh_w(dw, MAX_PATH, d);
    snwprintf(pat, MAX_PATH + 8, L"\\\\?\\%ls\\r*", dw);
    HANDLE h = U(FindFirstFileW)(pat, &fw);
    CHECK(h != INVALID_HANDLE_VALUE);
    int n = 0;
    if (h != INVALID_HANDLE_VALUE) { do n++; while (U(FindNextFileW)(h, &fw)); U(FindClose)(h); }
    EXPECT_EQ(n, 2);
}
TEST(findex_long_path_without_prefix) {
    REQUIRE(U(FindFirstFileW)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_longpath"); WCHAR dw[MAX_PATH], cur[1100], raw[1200], pat[1200]; WIN32_FIND_DATAW fw;
    dh_w(dw, MAX_PATH, d);
    int k = snwprintf(cur, 1100, L"%ls", dw);
    int made = 0;
    while (k < 290) {
        cur[k++] = '\\';
        for (int i = 0; i < 50; i++) cur[k++] = 'v';
        cur[k] = 0;
        snwprintf(raw, 1200, L"\\\\?\\%ls", cur);
        if (!CreateDirectoryW(raw, NULL)) { CHECKF(0, "create err %lu", GetLastError()); return; }
        made++;
    }
    snwprintf(raw, 1200, L"\\\\?\\%ls\\*", cur);
    HANDLE h = U(FindFirstFileW)(raw, &fw);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) U(FindClose)(h);
    snwprintf(pat, 1200, L"%ls\\*", cur);
    SET_ERR(0);
    h = U(FindFirstFileW)(pat, &fw);
    DWORD e = GetLastError();
    t_note("FindFirstFileW with %d-char path and no \\\\?\\ prefix: %s err=%lu", (int)wcslen(pat), h == INVALID_HANDLE_VALUE ? "failed" : "succeeded", e);
    if (!t_is_wine()) { CHECK(h == INVALID_HANDLE_VALUE); CHECKF(e == ERROR_PATH_NOT_FOUND || e == ERROR_FILENAME_EXCED_RANGE, "err %lu", e); }
    if (h != INVALID_HANDLE_VALUE) U(FindClose)(h);
    dh_rmtree(d);
}

/* ---- unicode and special names ---- */
static const WCHAR *uni_names[] = { L"caf\u00e9.txt", L"\u65e5\u672c\u8a9e.txt", L"\U0001F600.txt", L"\u0434\u0430.txt", L"\u00df.txt", L"\u0391\u03b2.txt", L"\u05d0\u05d1.txt", L"na\u00efve file.txt" };
TEST(findex_unicode_roundtrip_w) {
    REQUIRE(U(FindFirstFileW)); REQUIRE(U(FindNextFileW)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_uni"); WCHAR dw[MAX_PATH], pat[MAX_PATH]; WIN32_FIND_DATAW fw;
    dh_w(dw, MAX_PATH, d);
    int N = sizeof uni_names / sizeof *uni_names;
    for (int i = 0; i < N; i++) CHECKF(dh_mkfileW(dw, uni_names[i], i + 1), "create #%d err %lu", i, GetLastError());
    snwprintf(pat, MAX_PATH, L"%ls\\*", dw);
    int found[16] = {0}, total = 0;
    HANDLE h = U(FindFirstFileW)(pat, &fw);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!wcscmp(fw.cFileName, L".") || !wcscmp(fw.cFileName, L"..")) continue;
            total++;
            for (int i = 0; i < N; i++) if (!wcscmp(fw.cFileName, uni_names[i])) { found[i]++; EXPECT_EQ(fw.nFileSizeLow, i + 1); }
        } while (U(FindNextFileW)(h, &fw));
        U(FindClose)(h);
    }
    EXPECT_EQ(total, N);
    for (int i = 0; i < N; i++) CHECKF(found[i] == 1, "name #%d found %d times", i, found[i]);
    dh_rmtreeW(dw);
}
TEST(findex_unicode_case_insensitive_match) {
    REQUIRE(U(FindFirstFileW)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_unicase"); WCHAR dw[MAX_PATH], pat[MAX_PATH]; WIN32_FIND_DATAW fw;
    dh_w(dw, MAX_PATH, d);
    dh_mkfileW(dw, L"caf\u00e9.txt", 1);
    const WCHAR *pats[] = { L"CAF\u00c9.TXT", L"caf\u00c9*", L"CAF?.txt", L"*\u00c9.txt" };
    for (unsigned i = 0; i < 4; i++) {
        snwprintf(pat, MAX_PATH, L"%ls\\%ls", dw, pats[i]);
        HANDLE h = U(FindFirstFileW)(pat, &fw);
        CHECKF(h != INVALID_HANDLE_VALUE, "pattern #%u err %lu", i, GetLastError());
        if (h != INVALID_HANDLE_VALUE) { CHECK(!wcscmp(fw.cFileName, L"caf\u00e9.txt")); U(FindClose)(h); }
    }
    dh_rmtreeW(dw);
}
TEST(findex_surrogate_pair_wildcard_units) {
    REQUIRE(U(FindFirstFileW)); REQUIRE(U(FindNextFileW)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_surr"); WCHAR dw[MAX_PATH], pat[MAX_PATH]; WIN32_FIND_DATAW fw;
    dh_w(dw, MAX_PATH, d);
    dh_mkfileW(dw, L"\U0001F600.s", 1); dh_mkfileW(dw, L"x.s", 1);
    /* '?' is one UTF-16 code unit: the pair needs two */
    snwprintf(pat, MAX_PATH, L"%ls\\??.s", dw);
    int n = 0; int emoji = 0;
    HANDLE h = U(FindFirstFileW)(pat, &fw);
    if (h != INVALID_HANDLE_VALUE) { do { n++; if (!wcscmp(fw.cFileName, L"\U0001F600.s")) emoji = 1; } while (U(FindNextFileW)(h, &fw)); U(FindClose)(h); }
    CHECK(emoji);
    EXPECT_EQ(n, 2);      /* "x.s" matches because '?' also matches zero characters at the end */
    dh_rmtreeW(dw);
}
TEST(findex_lone_surrogate_name) {
    REQUIRE(U(FindFirstFileW)); REQUIRE(U(FindNextFileW)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_lone"); WCHAR dw[MAX_PATH], pat[MAX_PATH]; WIN32_FIND_DATAW fw;
    dh_w(dw, MAX_PATH, d);
    if (!dh_mkfileW(dw, L"lone_\xd800_x", 1)) { t_note("lone surrogate file name rejected err %lu", GetLastError()); return; }
    snwprintf(pat, MAX_PATH, L"%ls\\lone*", dw);
    HANDLE h = U(FindFirstFileW)(pat, &fw);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) { CHECK(!wcscmp(fw.cFileName, L"lone_\xd800_x")); U(FindClose)(h); }
    dh_rmtreeW(dw);
}
TEST(findex_special_char_names) {
    REQUIRE(U(FindFirstFileA)); REQUIRE(U(FindNextFileA)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_special"), p[MAX_PATH];
    const char *names[] = { "a b", "a  b", "a,b", "a;b", "a=b", "a+b", "a[b]", "a{b}", "a(b)", "a&b", "a!b", "a@b", "a#b", "a$b", "a%b", "a^b", "a~b", "a'b", "a`b", "a-b", "a_b", "#", "%", "$" };
    int N = sizeof names / sizeof *names;
    for (int i = 0; i < N; i++) { if (!dh_mkfile(d, names[i], 0)) CHECKF(0, "create '%s' err %lu", names[i], GetLastError()); }
    dh_name *l = malloc(sizeof(dh_name) * 64);
    sub(p, sizeof p, d, "*");
    int n = dh_listA_nd(p, l, 64);
    EXPECT_EQ(n, N);
    for (int i = 0; i < N; i++) CHECKF(dh_has(l, n, names[i]), "missing '%s'", names[i]);
    free(l);
}
TEST(findex_trailing_dot_names_via_raw) {
    REQUIRE(U(FindFirstFileW)); REQUIRE(U(FindNextFileW)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_trailing"); WCHAR dw[MAX_PATH], rawf[MAX_PATH + 16], pat[MAX_PATH]; WIN32_FIND_DATAW fw;
    dh_w(dw, MAX_PATH, d);
    snwprintf(rawf, MAX_PATH + 16, L"\\\\?\\%ls\\dotted.", dw);
    HANDLE f = CreateFileW(rawf, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) { t_note("cannot create name with trailing dot err %lu", GetLastError()); return; }
    CloseHandle(f);
    snwprintf(pat, MAX_PATH, L"%ls\\dotted*", dw);
    HANDLE h = U(FindFirstFileW)(pat, &fw);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) { CHECK(!wcscmp(fw.cFileName, L"dotted.")); U(FindClose)(h); }
    snwprintf(pat, MAX_PATH, L"%ls\\dotted.", dw);
    h = U(FindFirstFileW)(pat, &fw);     /* trailing dot is stripped by path normalisation: looks for "dotted" */
    t_note("FindFirstFileW('dotted.') on file named 'dotted.': %s", h != INVALID_HANDLE_VALUE ? "found" : "not found");
    if (h != INVALID_HANDLE_VALUE) U(FindClose)(h);
    DeleteFileW(rawf);
}
TEST(findex_ads_not_listed_as_entries) {
    REQUIRE(U(FindFirstFileA)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_adsnolist"), p[MAX_PATH], s[MAX_PATH];
    dh_mkfile(d, "host", 1);
    sub(p, sizeof p, d, "host"); snprintf(s, sizeof s, "%s:stream", p);
    HANDLE h = CreateFileA(s, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_skip("no streams on this volume"); return; }
    CloseHandle(h);
    dh_name l[8]; sub(p, sizeof p, d, "*");
    int n = dh_listA_nd(p, l, 8);
    EXPECT_EQ(n, 1);
    CHECK(dh_has(l, n, "host"));
}
TEST(findex_ads_size_not_included) {
    REQUIRE(U(FindFirstFileA)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_adssize"), p[MAX_PATH], s[MAX_PATH]; WIN32_FIND_DATAA fd;
    dh_mkfile(d, "host", 10);
    sub(p, sizeof p, d, "host"); snprintf(s, sizeof s, "%s:s2", p);
    HANDLE h = CreateFileA(s, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_skip("no streams"); return; }
    char big[4000] = {0}; DWORD w; WriteFile(h, big, sizeof big, &w, NULL); CloseHandle(h);
    h = U(FindFirstFileA)(p, &fd);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) { EXPECT_EQ(fd.nFileSizeLow, 10); U(FindClose)(h); }
}
TEST(findex_time_fields_follow_writes) {
    REQUIRE(U(FindFirstFileW)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_times"), p[MAX_PATH]; WCHAR w[MAX_PATH]; WIN32_FIND_DATAW a, b;
    dh_mkfile(d, "tf", 1); sub(p, sizeof p, d, "tf"); dh_w(w, MAX_PATH, p);
    HANDLE h = U(FindFirstFileW)(w, &a); CHECK(h != INVALID_HANDLE_VALUE); if (h != INVALID_HANDLE_VALUE) U(FindClose)(h);
    FILETIME ft = { 0xD53E8000, 0x019DB1DE };    /* 1970-01-02 00:00:00 UTC-ish constant */
    HANDLE f = CreateFileW(w, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(SetFileTime(f, &ft, NULL, &ft)); CloseHandle(f);
    h = U(FindFirstFileW)(w, &b); CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) {
        CHECK(!CompareFileTime(&b.ftLastWriteTime, &ft));
        CHECK(!CompareFileTime(&b.ftCreationTime, &ft));
        U(FindClose)(h);
    }
}
typedef struct { char pat[MAX_PATH]; int n; } conc_job;
static DWORD WINAPI conc_worker(LPVOID a) {
    conc_job *j = a; dh_name *l = malloc(sizeof(dh_name) * 100);
    j->n = dh_listA_nd(j->pat, l, 100); free(l); return 0;
}
TEST(findex_concurrent_enumeration_threads) {
    REQUIRE(U(FindFirstFileA)); REQUIRE(U(FindNextFileA)); REQUIRE(U(FindClose));
    char *d = dh_dir("fx_conc"), p[MAX_PATH];
    for (int i = 0; i < 60; i++) { char n[16]; snprintf(n, sizeof n, "c%02d", i); dh_mkfile(d, n, 0); }
    sub(p, sizeof p, d, "*");
    static conc_job jobs[8]; HANDLE th[8];
    for (int i = 0; i < 8; i++) {
        strcpy(jobs[i].pat, p); jobs[i].n = 0;
        th[i] = CreateThread(NULL, 0, conc_worker, &jobs[i], 0, NULL);
    }
    WaitForMultipleObjects(8, th, TRUE, 15000);
    for (int i = 0; i < 8; i++) { CloseHandle(th[i]); EXPECT_EQ(jobs[i].n, 60); }
}
