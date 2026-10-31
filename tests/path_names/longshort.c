/* GetLongPathNameA/W, GetShortPathNameA/W, 8.3 generation and case handling. */
#include "pn.h"

/* Create <tmp>\<leaf> and return nonzero if the volume generates a distinct short name for it. */
static int make_long_file(const char *leaf, char *full, size_t cap) {
    t_tmpfile(full, cap, leaf);
    pn_touch(full);
    char sh[MAX_PATH * 2];
    DWORD n = U(GetShortPathNameA)(full, sh, sizeof sh);
    return n != 0 && strcasecmp(sh, full) != 0;
}
static const char *leafof(const char *p) { const char *s = strrchr(p, '\\'); return s ? s + 1 : p; }

TEST(path_names_long_null_is_invalid_parameter_w) {
    REQUIRE(U(GetLongPathNameW));
    WCHAR b[MAX_PATH];
    SET_ERR(0);
    EXPECT_EQ(U(GetLongPathNameW)(NULL, b, MAX_PATH), 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    EXPECT_EQ(U(GetLongPathNameW)(NULL, NULL, 0), 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(path_names_short_null_is_invalid_parameter_w) {
    REQUIRE(U(GetShortPathNameW));
    WCHAR b[MAX_PATH];
    SET_ERR(0);
    EXPECT_EQ(U(GetShortPathNameW)(NULL, b, MAX_PATH), 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    EXPECT_EQ(U(GetShortPathNameW)(NULL, NULL, 0), 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(path_names_long_null_a_note) {
    REQUIRE(U(GetLongPathNameA)); REQUIRE(U(GetShortPathNameA));
    char b[MAX_PATH];
    SET_ERR(0);
    DWORD n = U(GetLongPathNameA)(NULL, b, MAX_PATH);
    t_note("GetLongPathNameA(NULL) -> %lu err %lu", n, GetLastError());
    CHECK(n == 0);
    SET_ERR(0);
    n = U(GetShortPathNameA)(NULL, b, MAX_PATH);
    t_note("GetShortPathNameA(NULL) -> %lu err %lu", n, GetLastError());
    CHECK(n == 0);
}
TEST(path_names_long_nonexistent_file_errors) {
    REQUIRE(U(GetLongPathNameW)); REQUIRE(U(GetLongPathNameA));
    char p[MAX_PATH]; WCHAR b[MAX_PATH], pw[MAX_PATH];
    t_tmpfile(p, sizeof p, "does_not_exist_file.txt");
    MultiByteToWideChar(CP_ACP, 0, p, -1, pw, MAX_PATH);
    SET_ERR(0); EXPECT_EQ(U(GetLongPathNameW)(pw, b, MAX_PATH), 0); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    SET_ERR(0); EXPECT_EQ(U(GetLongPathNameA)(p, (char *)b, sizeof b), 0); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    /* a missing intermediate directory is PATH_NOT_FOUND */
    t_tmpfile(p, sizeof p, "no_such_dir\\file.txt");
    SET_ERR(0); EXPECT_EQ(U(GetLongPathNameA)(p, (char *)b, sizeof b), 0); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(path_names_short_nonexistent_file_errors) {
    REQUIRE(U(GetShortPathNameW)); REQUIRE(U(GetShortPathNameA));
    char p[MAX_PATH]; WCHAR b[MAX_PATH], pw[MAX_PATH];
    t_tmpfile(p, sizeof p, "does_not_exist_file.txt");
    MultiByteToWideChar(CP_ACP, 0, p, -1, pw, MAX_PATH);
    SET_ERR(0); EXPECT_EQ(U(GetShortPathNameW)(pw, b, MAX_PATH), 0); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    SET_ERR(0); EXPECT_EQ(U(GetShortPathNameA)(p, (char *)b, sizeof b), 0); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    t_tmpfile(p, sizeof p, "no_such_dir\\file.txt");
    SET_ERR(0); EXPECT_EQ(U(GetShortPathNameA)(p, (char *)b, sizeof b), 0); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(path_names_long_short_empty_string) {
    REQUIRE(U(GetLongPathNameW)); REQUIRE(U(GetShortPathNameW));
    WCHAR b[MAX_PATH];
    SET_ERR(0);
    DWORD n = U(GetLongPathNameW)(L"", b, MAX_PATH);
    t_note("GetLongPathNameW(\"\") -> %lu err %lu", n, GetLastError());
    CHECK(n == 0);
    SET_ERR(0);
    n = U(GetShortPathNameW)(L"", b, MAX_PATH);
    t_note("GetShortPathNameW(\"\") -> %lu err %lu", n, GetLastError());
    CHECK(n == 0);
}
TEST(path_names_long_wildcard_name_fails) {
    REQUIRE(U(GetLongPathNameA)); REQUIRE(U(GetShortPathNameA));
    char p[MAX_PATH], b[MAX_PATH];
    t_tmpfile(p, sizeof p, "*");
    SET_ERR(0); EXPECT_EQ(U(GetLongPathNameA)(p, b, MAX_PATH), 0);
    t_note("long wildcard err %lu", GetLastError());
    t_tmpfile(p, sizeof p, "a?b");
    SET_ERR(0); EXPECT_EQ(U(GetShortPathNameA)(p, b, MAX_PATH), 0);
    t_note("short wildcard err %lu", GetLastError());
}
TEST(path_names_long_existing_ascii_returns_same) {
    REQUIRE(U(GetLongPathNameA)); REQUIRE(U(GetLongPathNameW));
    char p[MAX_PATH], b[MAX_PATH]; WCHAR bw[MAX_PATH];
    t_tmpfile(p, sizeof p, "plain.txt");
    pn_touch(p);
    DWORD n = U(GetLongPathNameA)(p, b, MAX_PATH);
    CHECK(n == strlen(p) && !strcmp(b, p));
    n = U(GetLongPathNameW)(pn_w(p), bw, MAX_PATH);
    CHECK(n == strlen(p) && pn_weq(bw, p));
    DeleteFileA(p);
}
TEST(path_names_long_buffer_semantics_w) {
    REQUIRE(U(GetLongPathNameW));
    char p[MAX_PATH]; WCHAR bw[MAX_PATH], *pw;
    t_tmpfile(p, sizeof p, "buf_sem.txt"); pn_touch(p);
    pw = pn_w(p);
    DWORD len = (DWORD)strlen(p);
    EXPECT_EQ(U(GetLongPathNameW)(pw, NULL, 0), len + 1);
    EXPECT_EQ(U(GetLongPathNameW)(pw, bw, 0), len + 1);
    EXPECT_EQ(U(GetLongPathNameW)(pw, bw, 1), len + 1);
    EXPECT_EQ(U(GetLongPathNameW)(pw, bw, len), len + 1);
    pn_fill(bw, 8, L'Z');
    bw[8] = 0;
    EXPECT_EQ(U(GetLongPathNameW)(pw, bw, len + 1), len);
    CHECK(pn_weq(bw, p));
    DeleteFileA(p);
}
TEST(path_names_long_buffer_semantics_a) {
    REQUIRE(U(GetLongPathNameA));
    char p[MAX_PATH], b[MAX_PATH];
    t_tmpfile(p, sizeof p, "buf_sem_a.txt"); pn_touch(p);
    DWORD len = (DWORD)strlen(p);
    EXPECT_EQ(U(GetLongPathNameA)(p, NULL, 0), len + 1);
    EXPECT_EQ(U(GetLongPathNameA)(p, b, 0), len + 1);
    EXPECT_EQ(U(GetLongPathNameA)(p, b, len), len + 1);
    EXPECT_EQ(U(GetLongPathNameA)(p, b, len + 1), len);
    EXPECT_STR(b, p);
    DeleteFileA(p);
}
TEST(path_names_short_buffer_semantics_w) {
    REQUIRE(U(GetShortPathNameW));
    char p[MAX_PATH]; WCHAR bw[MAX_PATH], *pw;
    t_tmpfile(p, sizeof p, "short_sem_long_name.txt"); pn_touch(p);
    pw = pn_w(p);
    DWORD need = U(GetShortPathNameW)(pw, NULL, 0);
    CHECK(need > 1);
    EXPECT_EQ(U(GetShortPathNameW)(pw, bw, 0), need);
    EXPECT_EQ(U(GetShortPathNameW)(pw, bw, 1), need);
    EXPECT_EQ(U(GetShortPathNameW)(pw, bw, need - 1), need);
    DWORD n = U(GetShortPathNameW)(pw, bw, need);
    EXPECT_EQ(n, need - 1);
    CHECK(wcslen(bw) == n);
    DeleteFileA(p);
}
TEST(path_names_short_buffer_semantics_a) {
    REQUIRE(U(GetShortPathNameA));
    char p[MAX_PATH], b[MAX_PATH];
    t_tmpfile(p, sizeof p, "short_sem_a_long_name.txt"); pn_touch(p);
    DWORD need = U(GetShortPathNameA)(p, NULL, 0);
    CHECK(need > 1);
    EXPECT_EQ(U(GetShortPathNameA)(p, b, need - 1), need);
    DWORD n = U(GetShortPathNameA)(p, b, need);
    EXPECT_EQ(n, need - 1);
    CHECK(strlen(b) == n);
    DeleteFileA(p);
}
TEST(path_names_short_name_generation_8dot3) {
    REQUIRE(U(GetShortPathNameA)); REQUIRE(U(GetLongPathNameA));
    char full[MAX_PATH], sh[MAX_PATH], lg[MAX_PATH];
    if (!make_long_file("This is a long file name.text", full, sizeof full)) { t_skip("8.3 name generation disabled on this volume"); DeleteFileA(full); return; }
    DWORD n = U(GetShortPathNameA)(full, sh, sizeof sh);
    CHECK(n == strlen(sh));
    const char *leaf = leafof(sh);
    size_t ll = strlen(leaf);
    CHECKF(ll <= 12, "short leaf too long: %s", leaf);
    CHECKF(strchr(leaf, '~') != NULL, "short leaf %s has no ~", leaf);
    CHECKF(strchr(leaf, ' ') == NULL, "short leaf %s has a space", leaf);
    CHECKF(!strcasecmp(leaf + ll - 5, "~1.TEX") || strstr(leaf, "~") != NULL, "%s", leaf);
    for (const char *c = leaf; *c; c++) CHECKF(!islower((unsigned char)*c), "short name %s must be upper-case", leaf);
    /* the directory part (k32t_<pid>) is already 8.3-compliant or shortened; round trip must restore the long name */
    n = U(GetLongPathNameA)(sh, lg, sizeof lg);
    CHECKF(n == strlen(full) && !strcasecmp(lg, full), "long(%s) = %s want %s", sh, lg, full);
    DeleteFileA(full);
}
TEST(path_names_short_generation_collisions_get_distinct_numbers) {
    REQUIRE(U(GetShortPathNameA));
    char a[MAX_PATH], b[MAX_PATH], c[MAX_PATH], sa[MAX_PATH], sb[MAX_PATH], sc[MAX_PATH];
    if (!make_long_file("collision_name_alpha.dat", a, sizeof a)) { t_skip("8.3 disabled"); DeleteFileA(a); return; }
    make_long_file("collision_name_beta.dat", b, sizeof b);
    make_long_file("collision_name_gamma.dat", c, sizeof c);
    U(GetShortPathNameA)(a, sa, sizeof sa); U(GetShortPathNameA)(b, sb, sizeof sb); U(GetShortPathNameA)(c, sc, sizeof sc);
    CHECK(strcasecmp(sa, sb) && strcasecmp(sb, sc) && strcasecmp(sa, sc));
    CHECKF(!strncasecmp(leafof(sa), "COLLIS~", 7), "%s", leafof(sa));
    CHECKF(!strncasecmp(leafof(sb), "COLLIS~", 7), "%s", leafof(sb));
    CHECKF(!strncasecmp(leafof(sc), "COLLIS~", 7), "%s", leafof(sc));
    CHECKF(!strcasecmp(leafof(sa), "COLLIS~1.DAT"), "first-created gets ~1: %s", leafof(sa));
    CHECKF(!strcasecmp(leafof(sb), "COLLIS~2.DAT"), "second gets ~2: %s", leafof(sb));
    CHECKF(!strcasecmp(leafof(sc), "COLLIS~3.DAT"), "third gets ~3: %s", leafof(sc));
    DeleteFileA(a); DeleteFileA(b); DeleteFileA(c);
}
TEST(path_names_short_generation_extension_truncated_to_3) {
    REQUIRE(U(GetShortPathNameA));
    char f[MAX_PATH], s[MAX_PATH];
    if (!make_long_file("shortext_file.extension", f, sizeof f)) { t_skip("8.3 disabled"); DeleteFileA(f); return; }
    U(GetShortPathNameA)(f, s, sizeof s);
    const char *l = leafof(s);
    CHECKF(!strcasecmp(l, "SHORTE~1.EXT"), "%s", l);
    DeleteFileA(f);
}
TEST(path_names_short_generation_illegal_chars_replaced) {
    REQUIRE(U(GetShortPathNameA));
    char f[MAX_PATH], s[MAX_PATH];
    if (!make_long_file("a+b,c;d=e[f]g long.txt", f, sizeof f)) { t_skip("8.3 disabled"); DeleteFileA(f); return; }
    U(GetShortPathNameA)(f, s, sizeof s);
    const char *l = leafof(s);
    CHECKF(strpbrk(l, "+,;=[] ") == NULL, "illegal chars survived in %s", l);
    CHECKF(strchr(l, '~') != NULL, "%s", l);
    DeleteFileA(f);
}
TEST(path_names_short_not_generated_for_legal_83_names) {
    REQUIRE(U(GetShortPathNameA));
    char f[MAX_PATH], s[MAX_PATH];
    /* the scratch directory name itself may be shortened; only the leaf is compared */
    t_tmpfile(f, sizeof f, "UPPER.TXT"); pn_touch(f);
    DWORD n = U(GetShortPathNameA)(f, s, sizeof s);
    CHECK(n != 0 && n == strlen(s));
    CHECK(!strcmp(leafof(s), "UPPER.TXT"));
    DeleteFileA(f);
    t_tmpfile(f, sizeof f, "lower.txt"); pn_touch(f);
    n = U(GetShortPathNameA)(f, s, sizeof s);
    CHECK(!strcasecmp(leafof(s), "lower.txt"));
    DeleteFileA(f);
}
TEST(path_names_short_of_short_is_unchanged) {
    REQUIRE(U(GetShortPathNameA));
    char f[MAX_PATH], s1[MAX_PATH], s2[MAX_PATH];
    if (!make_long_file("idempotent short name.txt", f, sizeof f)) { t_skip("8.3 disabled"); DeleteFileA(f); return; }
    U(GetShortPathNameA)(f, s1, sizeof s1);
    DWORD n = U(GetShortPathNameA)(s1, s2, sizeof s2);
    CHECK(n == strlen(s1) && !strcasecmp(s1, s2));
    DeleteFileA(f);
}
TEST(path_names_long_of_long_is_unchanged) {
    REQUIRE(U(GetLongPathNameA));
    char f[MAX_PATH], s2[MAX_PATH];
    t_tmpfile(f, sizeof f, "already long file name.txt"); pn_touch(f);
    DWORD n = U(GetLongPathNameA)(f, s2, sizeof s2);
    CHECK(n == strlen(f) && !strcmp(f, s2));
    DeleteFileA(f);
}
TEST(path_names_long_expands_each_short_component) {
    REQUIRE(U(GetLongPathNameA)); REQUIRE(U(GetShortPathNameA));
    char d1[MAX_PATH], d2[MAX_PATH], f[MAX_PATH], s[MAX_PATH], l[MAX_PATH];
    t_tmpfile(d1, sizeof d1, "first long directory");
    CreateDirectoryA(d1, NULL);
    snprintf(d2, sizeof d2, "%s\\second long directory", d1);
    CreateDirectoryA(d2, NULL);
    snprintf(f, sizeof f, "%s\\third long file name.txt", d2);
    pn_touch(f);
    DWORD n = U(GetShortPathNameA)(f, s, sizeof s);
    if (!n || !strcasecmp(s, f)) { t_skip("8.3 disabled"); }
    else {
        CHECK(strchr(s, ' ') == NULL);
        n = U(GetLongPathNameA)(s, l, sizeof l);
        CHECKF(n == strlen(f) && !strcasecmp(l, f), "got %s want %s", l, f);
        /* only the short components of a mixed path are expanded */
        char mix[MAX_PATH * 2];
        char dshort[MAX_PATH];
        U(GetShortPathNameA)(d1, dshort, sizeof dshort);
        snprintf(mix, sizeof mix, "%s\\second long directory\\third long file name.txt", dshort);
        n = U(GetLongPathNameA)(mix, l, sizeof l);
        CHECKF(n == strlen(f) && !strcasecmp(l, f), "mixed: got %s want %s", l, f);
    }
    DeleteFileA(f); RemoveDirectoryA(d2); RemoveDirectoryA(d1);
}
TEST(path_names_long_directory_trailing_backslash_kept) {
    REQUIRE(U(GetLongPathNameA));
    char d[MAX_PATH], b[MAX_PATH];
    snprintf(d, sizeof d, "%s", t_tmpdir());   /* ends with backslash */
    DWORD n = U(GetLongPathNameA)(d, b, sizeof b);
    CHECK(n == strlen(d) && !strcasecmp(b, d));
    d[strlen(d) - 1] = 0;
    n = U(GetLongPathNameA)(d, b, sizeof b);
    CHECK(n == strlen(d) && !strcasecmp(b, d));
}
TEST(path_names_short_directory_trailing_backslash_kept) {
    REQUIRE(U(GetShortPathNameA));
    char d[MAX_PATH], b[MAX_PATH];
    snprintf(d, sizeof d, "%s", t_tmpdir());
    DWORD n = U(GetShortPathNameA)(d, b, sizeof b);
    CHECK(n != 0 && b[n - 1] == '\\');
}
TEST(path_names_long_short_root_directory) {
    REQUIRE(U(GetLongPathNameA)); REQUIRE(U(GetShortPathNameA));
    char root[8] = "C:\\", b[MAX_PATH];
    root[0] = pn_sysdrive();
    EXPECT_EQ(U(GetLongPathNameA)(root, b, sizeof b), 3);
    EXPECT_STR(b, root);
    EXPECT_EQ(U(GetShortPathNameA)(root, b, sizeof b), 3);
    EXPECT_STR(b, root);
    EXPECT_EQ(U(GetLongPathNameA)(root, NULL, 0), 4);
    EXPECT_EQ(U(GetShortPathNameA)(root, NULL, 0), 4);
}
TEST(path_names_long_short_relative_forms_unchanged) {
    REQUIRE(U(GetLongPathNameA)); REQUIRE(U(GetShortPathNameA));
    char b[MAX_PATH];
    static const char *rel[] = { ".", "..", ".\\..", "..\\.." };
    for (unsigned i = 0; i < sizeof rel / sizeof *rel; i++) {
        DWORD n = U(GetLongPathNameA)(rel[i], b, sizeof b);
        CHECKF(n == strlen(rel[i]) && !strcmp(b, rel[i]), "long %s -> %s", rel[i], b);
        n = U(GetShortPathNameA)(rel[i], b, sizeof b);
        CHECKF(n == strlen(rel[i]) && !strcmp(b, rel[i]), "short %s -> %s", rel[i], b);
    }
}
TEST(path_names_long_short_relative_file_in_cwd_child) {
    CHECK(t_run_child("long_relative", NULL, 20000) == 0);
}
CHILD(long_relative) {
    char d[MAX_PATH];
    snprintf(d, sizeof d, "%s", t_tmpdir());
    if (!SetCurrentDirectoryA(d)) return 10;
    pn_touch("rel file with spaces.txt");
    char b[MAX_PATH];
    DWORD n = U(GetLongPathNameA)("rel file with spaces.txt", b, sizeof b);
    if (n != 24 || strcmp(b, "rel file with spaces.txt")) return 11;
    n = U(GetLongPathNameA)(".\\rel file with spaces.txt", b, sizeof b);
    if (n != 26 || strcmp(b, ".\\rel file with spaces.txt")) return 12;
    n = U(GetShortPathNameA)("rel file with spaces.txt", b, sizeof b);
    if (!n) return 13;
    if (strchr(b, '\\')) return 14; /* relative stays relative */
    n = U(GetLongPathNameA)(b, b + 100, 100);
    if (n != 24) return 16;
    DeleteFileA("rel file with spaces.txt");
    { char w[MAX_PATH]; GetWindowsDirectoryA(w, sizeof w); SetCurrentDirectoryA(w); }
    return 0;
}
TEST(path_names_long_case_of_nonshort_components) {
    REQUIRE(U(GetLongPathNameA));
    char f[MAX_PATH], q[MAX_PATH], b[MAX_PATH];
    t_tmpfile(f, sizeof f, "MixedCaseName.Txt"); pn_touch(f);
    t_tmpfile(q, sizeof q, "mixedcasename.txt");
    DWORD n = U(GetLongPathNameA)(q, b, sizeof b);
    CHECK(n == strlen(q));
    t_note("GetLongPathName of differently cased name returns %s (on-disk case is %s)", leafof(b), leafof(f));
    DeleteFileA(f);
}
TEST(path_names_long_short_case_of_short_name_restored) {
    REQUIRE(U(GetLongPathNameA)); REQUIRE(U(GetShortPathNameA));
    char f[MAX_PATH], s[MAX_PATH], b[MAX_PATH];
    if (!make_long_file("CaseSensitive Long Name.TXT", f, sizeof f)) { t_skip("8.3 disabled"); DeleteFileA(f); return; }
    U(GetShortPathNameA)(f, s, sizeof s);
    for (char *c = s; *c; c++) *c = (char)tolower((unsigned char)*c);
    DWORD n = U(GetLongPathNameA)(s, b, sizeof b);
    CHECKF(n == strlen(f) && !strcmp(leafof(b), "CaseSensitive Long Name.TXT"), "%s", b);
    DeleteFileA(f);
}
TEST(path_names_case_preserved_by_filesystem) {
    char f[MAX_PATH]; WIN32_FIND_DATAA fd;
    t_tmpfile(f, sizeof f, "CaSe_PrEsErVeD.TxT"); pn_touch(f);
    HANDLE h = FindFirstFileA(f, &fd);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) { EXPECT_STR(fd.cFileName, "CaSe_PrEsErVeD.TxT"); FindClose(h); }
    char q[MAX_PATH]; t_tmpfile(q, sizeof q, "case_preserved.txt");
    CHECK(GetFileAttributesA(q) != INVALID_FILE_ATTRIBUTES);
    DeleteFileA(f);
}
TEST(path_names_long_extended_prefix_w) {
    REQUIRE(U(GetLongPathNameW)); REQUIRE(U(GetShortPathNameW));
    char f[MAX_PATH], pre[MAX_PATH + 8]; WCHAR b[MAX_PATH + 8];
    t_tmpfile(f, sizeof f, "prefixed file name.txt"); pn_touch(f);
    snprintf(pre, sizeof pre, "\\\\?\\%s", f);
    DWORD n = U(GetLongPathNameW)(pn_w(pre), b, MAX_PATH + 8);
    CHECKF(n == strlen(pre) && pn_weq(b, pre), "n=%lu b=%s", n, pn_n(b));
    n = U(GetShortPathNameW)(pn_w(pre), b, MAX_PATH + 8);
    CHECK(n != 0 && !wcsncmp(b, L"\\\\?\\", 4));
    DeleteFileA(f);
}
TEST(path_names_long_over_max_path_w) {
    REQUIRE(U(GetLongPathNameW)); REQUIRE(U(GetShortPathNameW));
    /* build \\?\<tmp>\<d1>\<d2>... deeper than MAX_PATH using short components */
    static WCHAR dir[1000], buf[1100];
    WCHAR *tmp = pn_w(t_tmpdir());
    wcscpy(dir, L"\\\\?\\"); wcscat(dir, tmp);
    size_t base = wcslen(dir);
    int created = 0;
    while (wcslen(dir) < 320 && created < 20) {
        wcscat(dir, L"d0123456789abcdef0123456789\\");
        if (!CreateDirectoryW(dir, NULL)) break;
        created++;
    }
    if (wcslen(dir) < 270) { t_skip("could not create deep directory (%d)", created); goto cleanup; }
    {
        DWORD len = (DWORD)wcslen(dir);
        EXPECT_EQ(U(GetLongPathNameW)(dir, NULL, 0), len + 1);
        EXPECT_EQ(U(GetLongPathNameW)(dir, buf, 1100), len);
        CHECK(!wcscmp(buf, dir));
        DWORD n = U(GetShortPathNameW)(dir, buf, 1100);
        CHECK(n != 0);
    }
cleanup:
    while (created-- > 0) {
        RemoveDirectoryW(dir);
        WCHAR *e = dir + wcslen(dir) - 1; *e = 0;
        WCHAR *s = wcsrchr(dir, L'\\'); if (s) s[1] = 0;
    }
}
TEST(path_names_long_inplace_same_buffer) {
    REQUIRE(U(GetLongPathNameW)); REQUIRE(U(GetShortPathNameW)); REQUIRE(U(GetLongPathNameA));
    char f[MAX_PATH]; WCHAR b[MAX_PATH];
    t_tmpfile(f, sizeof f, "in place buffer name.txt"); pn_touch(f);
    wcscpy(b, pn_w(f));
    DWORD n = U(GetShortPathNameW)(b, b, MAX_PATH);
    CHECK(n != 0 && n == wcslen(b));
    n = U(GetLongPathNameW)(b, b, MAX_PATH);
    CHECKF(n == strlen(f) && !_wcsicmp(b, pn_w(f)), "got %s", pn_n(b));
    char a[MAX_PATH]; strcpy(a, f);
    n = U(GetLongPathNameA)(a, a, MAX_PATH);
    CHECK(n == strlen(f) && !strcmp(a, f));
    DeleteFileA(f);
}
TEST(path_names_long_a_w_parity_unicode_free_names) {
    REQUIRE(U(GetLongPathNameW)); REQUIRE(U(GetLongPathNameA)); REQUIRE(U(GetShortPathNameW)); REQUIRE(U(GetShortPathNameA));
    char f[MAX_PATH], a[MAX_PATH], aw[MAX_PATH]; WCHAR w[MAX_PATH];
    t_tmpfile(f, sizeof f, "parity long file name.txt"); pn_touch(f);
    DWORD na = U(GetShortPathNameA)(f, a, sizeof a);
    DWORD nw = U(GetShortPathNameW)(pn_w(f), w, MAX_PATH);
    pn_a(aw, sizeof aw, w);
    CHECK(na == nw && !strcmp(a, aw));
    na = U(GetLongPathNameA)(a, a, sizeof a);
    nw = U(GetLongPathNameW)(w, w, MAX_PATH);
    pn_a(aw, sizeof aw, w);
    CHECK(na == nw && !strcmp(a, aw));
    DeleteFileA(f);
}
TEST(path_names_long_unicode_filename) {
    REQUIRE(U(GetLongPathNameW)); REQUIRE(U(GetShortPathNameW));
    WCHAR f[MAX_PATH], b[MAX_PATH];
    swprintf(f, MAX_PATH, L"%ls\u4e2d\u6587\u00e9file.txt", pn_w(t_tmpdir()));
    HANDLE h = CreateFileW(f, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_skip("cannot create unicode file"); return; }
    CloseHandle(h);
    DWORD n = U(GetLongPathNameW)(f, b, MAX_PATH);
    CHECK(n == wcslen(f) && !wcscmp(b, f));
    n = U(GetShortPathNameW)(f, b, MAX_PATH);
    CHECK(n != 0);
    if (n) { WCHAR l[MAX_PATH]; DWORD m = U(GetLongPathNameW)(b, l, MAX_PATH); CHECK(m == wcslen(f) && !wcscmp(l, f)); }
    DeleteFileW(f);
}
TEST(path_names_long_short_directory_symbolic_dotted_components) {
    REQUIRE(U(GetLongPathNameA));
    char f[MAX_PATH], q[MAX_PATH * 2], b[MAX_PATH];
    t_tmpfile(f, sizeof f, "dotdot target.txt"); pn_touch(f);
    snprintf(q, sizeof q, "%ssub\\..\\dotdot target.txt", t_tmpdir());
    SET_ERR(0);
    DWORD n = U(GetLongPathNameA)(q, b, sizeof b);
    t_note("path with nonexistent dir + .. : n=%lu err=%lu", n, GetLastError());
    char d[MAX_PATH]; snprintf(d, sizeof d, "%ssub", t_tmpdir()); CreateDirectoryA(d, NULL);
    n = U(GetLongPathNameA)(q, b, sizeof b);
    CHECK(n == strlen(q));
    CHECK(!strcmp(b, q)); /* .. is not collapsed by GetLongPathName */
    RemoveDirectoryA(d); DeleteFileA(f);
}
TEST(path_names_short_dir_with_dot_extension_chars) {
    REQUIRE(U(GetShortPathNameA));
    char d[MAX_PATH], s[MAX_PATH];
    snprintf(d, sizeof d, "%sdir.with.dots.in.name", t_tmpdir());
    CreateDirectoryA(d, NULL);
    DWORD n = U(GetShortPathNameA)(d, s, sizeof s);
    CHECK(n != 0);
    const char *l = leafof(s);
    if (strcasecmp(s, d)) CHECKF(strchr(l, '~') != NULL && strlen(l) <= 12, "%s", l);
    RemoveDirectoryA(d);
}
TEST(path_names_short_trailing_dot_space_names) {
    REQUIRE(U(GetShortPathNameA)); REQUIRE(U(GetLongPathNameA));
    char f[MAX_PATH], b[MAX_PATH];
    t_tmpfile(f, sizeof f, "trailing_dot_test.");
    pn_touch(f);
    /* the dot is stripped by path normalisation: the same file exists as "trailing_dot_test" */
    char g[MAX_PATH]; t_tmpfile(g, sizeof g, "trailing_dot_test");
    CHECK(GetFileAttributesA(g) != INVALID_FILE_ATTRIBUTES);
    DWORD n = U(GetLongPathNameA)(g, b, sizeof b);
    CHECK(n == strlen(g));
    DeleteFileA(g);
}
TEST(path_names_long_short_exported_from_kernel32_and_kernelbase) {
    CHECK(UK(GetLongPathNameW) != NULL); CHECK(UK(GetLongPathNameA) != NULL);
    CHECK(UK(GetShortPathNameW) != NULL); CHECK(UK(GetShortPathNameA) != NULL);
    CHECK(UB(GetLongPathNameW) != NULL); CHECK(UB(GetShortPathNameW) != NULL);
    t_note("GetShortPathNameA in kernelbase: %s", UB(GetShortPathNameA) ? "yes" : "no");
    CHECK(UK(GetLongPathNameTransactedW) != NULL || t_is_wine());
    CHECK(UK(GetShortPathNameW) != NULL);
}
