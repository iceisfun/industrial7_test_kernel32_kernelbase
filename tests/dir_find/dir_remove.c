/* RemoveDirectory{A,W} */
#include "dh.h"

static char *sub(char *buf, size_t cap, const char *dir, const char *leaf) { dh_path(buf, cap, dir, leaf); return buf; }

TEST(dir_remove_a_basic) {
    REQUIRE(U(RemoveDirectoryA));
    char *d = dh_dir("rm_a"), p[MAX_PATH];
    sub(p, sizeof p, d, "x"); CreateDirectoryA(p, NULL);
    CHECK(U(RemoveDirectoryA)(p));
    CHECK(!dh_exists(p));
    SET_ERR(0);
    CHECK(!U(RemoveDirectoryA)(p));
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(dir_remove_w_basic) {
    REQUIRE(U(RemoveDirectoryW));
    char *d = dh_dir("rm_w"), p[MAX_PATH]; WCHAR w[MAX_PATH];
    sub(p, sizeof p, d, "x"); CreateDirectoryA(p, NULL); dh_w(w, MAX_PATH, p);
    CHECK(U(RemoveDirectoryW)(w));
    CHECK(!dh_exists(p));
    SET_ERR(0);
    CHECK(!U(RemoveDirectoryW)(w));
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(dir_remove_missing_parent) {
    REQUIRE(U(RemoveDirectoryA));
    char *d = dh_dir("rm_nopar"), p[MAX_PATH];
    sub(p, sizeof p, d, "no\\such");
    CHECK(!U(RemoveDirectoryA)(p));
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(dir_remove_not_empty_file) {
    REQUIRE(U(RemoveDirectoryA)); REQUIRE(U(RemoveDirectoryW));
    char *d = dh_dir("rm_ne"), p[MAX_PATH]; WCHAR w[MAX_PATH];
    sub(p, sizeof p, d, "x"); CreateDirectoryA(p, NULL); dh_mkfile(p, "f", 1);
    SET_ERR(0);
    CHECK(!U(RemoveDirectoryA)(p));
    EXPECT_ERR(ERROR_DIR_NOT_EMPTY);
    dh_w(w, MAX_PATH, p);
    CHECK(!U(RemoveDirectoryW)(w));
    EXPECT_ERR(ERROR_DIR_NOT_EMPTY);
    CHECK(dh_isdir(p));
}
TEST(dir_remove_not_empty_subdir) {
    REQUIRE(U(RemoveDirectoryA));
    char *d = dh_dir("rm_nesub"), p[MAX_PATH];
    sub(p, sizeof p, d, "x"); CreateDirectoryA(p, NULL); dh_mkdir(p, "y");
    CHECK(!U(RemoveDirectoryA)(p));
    EXPECT_ERR(ERROR_DIR_NOT_EMPTY);
    char q[MAX_PATH]; sub(q, sizeof q, p, "y");
    CHECK(U(RemoveDirectoryA)(q));
    CHECK(U(RemoveDirectoryA)(p));
}
TEST(dir_remove_not_empty_hidden_file) {
    REQUIRE(U(RemoveDirectoryA));
    char *d = dh_dir("rm_nehid"), p[MAX_PATH], f[MAX_PATH];
    sub(p, sizeof p, d, "x"); CreateDirectoryA(p, NULL); dh_mkfile(p, "h", 1);
    sub(f, sizeof f, p, "h"); SetFileAttributesA(f, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
    CHECK(!U(RemoveDirectoryA)(p));
    EXPECT_ERR(ERROR_DIR_NOT_EMPTY);
    SetFileAttributesA(f, FILE_ATTRIBUTE_NORMAL); DeleteFileA(f);
    CHECK(U(RemoveDirectoryA)(p));
}
TEST(dir_remove_not_empty_readonly_file) {
    REQUIRE(U(RemoveDirectoryA));
    char *d = dh_dir("rm_nero"), p[MAX_PATH], f[MAX_PATH];
    sub(p, sizeof p, d, "x"); CreateDirectoryA(p, NULL); dh_mkfile(p, "r", 1);
    sub(f, sizeof f, p, "r"); SetFileAttributesA(f, FILE_ATTRIBUTE_READONLY);
    CHECK(!U(RemoveDirectoryA)(p));
    EXPECT_ERR(ERROR_DIR_NOT_EMPTY);
    SetFileAttributesA(f, FILE_ATTRIBUTE_NORMAL); DeleteFileA(f);
}
TEST(dir_remove_file_target) {
    REQUIRE(U(RemoveDirectoryA)); REQUIRE(U(RemoveDirectoryW));
    char *d = dh_dir("rm_isfile"), p[MAX_PATH]; WCHAR w[MAX_PATH];
    dh_mkfile(d, "f", 1); sub(p, sizeof p, d, "f"); dh_w(w, MAX_PATH, p);
    SET_ERR(0);
    CHECK(!U(RemoveDirectoryA)(p));
    EXPECT_ERR(ERROR_DIRECTORY);
    CHECK(!U(RemoveDirectoryW)(w));
    EXPECT_ERR(ERROR_DIRECTORY);
    CHECK(dh_exists(p));
}
TEST(dir_remove_empty_string) {
    REQUIRE(U(RemoveDirectoryA)); REQUIRE(U(RemoveDirectoryW));
    SET_ERR(0); CHECK(!U(RemoveDirectoryA)("")); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    SET_ERR(0); CHECK(!U(RemoveDirectoryW)(L"")); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(dir_remove_null) {
    REQUIRE(U(RemoveDirectoryW));
    SET_ERR(0);
    CHECK(!U(RemoveDirectoryW)(NULL));
    t_note("RemoveDirectoryW(NULL) err=%lu", GetLastError());
}
TEST(dir_remove_null_a) {
    REQUIRE(U(RemoveDirectoryA));
    SET_ERR(0);
    CHECK(!U(RemoveDirectoryA)(NULL));
    t_note("RemoveDirectoryA(NULL) err=%lu", GetLastError());
}
TEST(dir_remove_invalid_chars) {
    REQUIRE(U(RemoveDirectoryA));
    char *d = dh_dir("rm_bad"), p[MAX_PATH];
    const char *bad[] = { "a*", "a?", "a<", "a>", "a|", "a\"" };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        sub(p, sizeof p, d, bad[i]);
        SET_ERR(0);
        CHECKF(!U(RemoveDirectoryA)(p), "#%u", i);
        EXPECT_ERR(ERROR_INVALID_NAME);
    }
}
TEST(dir_remove_trailing_sep_dot_space) {
    REQUIRE(U(RemoveDirectoryA));
    char *d = dh_dir("rm_trail"), p[MAX_PATH];
    const char *names[] = { "t1\\", "t2.", "t3 ", "t4. ", "t5/" };
    for (int i = 0; i < 5; i++) {
        char raw[16]; snprintf(raw, sizeof raw, "t%d", i + 1);
        sub(p, sizeof p, d, raw); CreateDirectoryA(p, NULL);
        sub(p, sizeof p, d, names[i]);
        CHECKF(U(RemoveDirectoryA)(p), "%s err %lu", names[i], GetLastError());
        sub(p, sizeof p, d, raw);
        CHECK(!dh_exists(p));
    }
}
TEST(dir_remove_case_insensitive) {
    REQUIRE(U(RemoveDirectoryA));
    char *d = dh_dir("rm_case"), p[MAX_PATH];
    sub(p, sizeof p, d, "MiXeD"); CreateDirectoryA(p, NULL);
    sub(p, sizeof p, d, "mixed");
    CHECK(U(RemoveDirectoryA)(p));
}
TEST(dir_remove_short_name) {
    REQUIRE(U(RemoveDirectoryA));
    char *d = dh_dir("rm_short"), p[MAX_PATH], sp[MAX_PATH];
    sub(p, sizeof p, d, "a_fairly_long_directory_name"); CreateDirectoryA(p, NULL);
    DWORD n = GetShortPathNameA(p, sp, sizeof sp);
    if (!n || n >= sizeof sp || !strcmp(sp, p)) { t_note("no short name available"); U(RemoveDirectoryA)(p); return; }
    CHECK(U(RemoveDirectoryA)(sp));
    CHECK(!dh_exists(p));
}
TEST(dir_remove_readonly_attribute) {
    REQUIRE(U(RemoveDirectoryA));
    char *d = dh_dir("rm_ro"), p[MAX_PATH];
    sub(p, sizeof p, d, "ro"); CreateDirectoryA(p, NULL);
    SetFileAttributesA(p, FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_READONLY);
    SET_ERR(0);
    BOOL ok = U(RemoveDirectoryA)(p);
    t_note("RemoveDirectory on read-only dir: ok=%d err=%lu", ok, GetLastError());
    if (!ok) { SetFileAttributesA(p, FILE_ATTRIBUTE_DIRECTORY); CHECK(U(RemoveDirectoryA)(p)); }
}
TEST(dir_remove_open_handle_no_share_delete) {
    REQUIRE(U(RemoveDirectoryA));
    char *d = dh_dir("rm_open"), p[MAX_PATH];
    sub(p, sizeof p, d, "x"); CreateDirectoryA(p, NULL);
    HANDLE h = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    SET_ERR(0);
    CHECK(!U(RemoveDirectoryA)(p));
    EXPECT_ERR(ERROR_SHARING_VIOLATION);
    CloseHandle(h);
    CHECK(U(RemoveDirectoryA)(p));
}
TEST(dir_remove_open_handle_share_delete) {
    REQUIRE(U(RemoveDirectoryA));
    char *d = dh_dir("rm_opensd"), p[MAX_PATH];
    sub(p, sizeof p, d, "x"); CreateDirectoryA(p, NULL);
    HANDLE h = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    CHECK(U(RemoveDirectoryA)(p));
    CloseHandle(h);
    CHECK(!dh_exists(p));
}
TEST(dir_remove_while_find_handle_open) {
    REQUIRE(U(RemoveDirectoryA)); REQUIRE(U(FindFirstFileA));
    char *d = dh_dir("rm_findopen"), p[MAX_PATH], pat[MAX_PATH];
    sub(p, sizeof p, d, "x"); CreateDirectoryA(p, NULL); dh_mkfile(p, "f", 1);
    sub(pat, sizeof pat, p, "*");
    WIN32_FIND_DATAA fd; HANDLE h = U(FindFirstFileA)(pat, &fd);
    CHECK(h != INVALID_HANDLE_VALUE);
    DeleteFileA(strcat(strcpy(pat, p), "\\f"));
    SET_ERR(0);
    BOOL ok = U(RemoveDirectoryA)(p);
    t_note("RemoveDirectory with live FindFirstFile handle: ok=%d err=%lu", ok, GetLastError());
    U(FindClose)(h);
    if (!ok) CHECK(U(RemoveDirectoryA)(p));
}
TEST(dir_remove_open_file_inside) {
    REQUIRE(U(RemoveDirectoryA));
    char *d = dh_dir("rm_fileopen"), p[MAX_PATH], f[MAX_PATH];
    sub(p, sizeof p, d, "x"); CreateDirectoryA(p, NULL); sub(f, sizeof f, p, "f");
    HANDLE h = CreateFileA(f, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    CHECK(!U(RemoveDirectoryA)(p));
    EXPECT_ERR(ERROR_DIR_NOT_EMPTY);
    CloseHandle(h);
    CHECK(U(RemoveDirectoryA)(p));
}
TEST(dir_remove_dot_and_dotdot) {
    REQUIRE(U(RemoveDirectoryA));
    char *d = dh_dir("rm_dots"), p[MAX_PATH];
    sub(p, sizeof p, d, "x"); CreateDirectoryA(p, NULL); dh_mkfile(p, "keep", 1);
    char q[MAX_PATH];
    snprintf(q, sizeof q, "%s\\.", p);
    CHECK(!U(RemoveDirectoryA)(q));            /* normalises to x, which is non-empty */
    EXPECT_ERR(ERROR_DIR_NOT_EMPTY);
    snprintf(q, sizeof q, "%s\\..", p);
    CHECK(!U(RemoveDirectoryA)(q));            /* parent: non-empty (or in use) */
    DWORD e = GetLastError();
    CHECKF(e == ERROR_DIR_NOT_EMPTY || e == ERROR_SHARING_VIOLATION, "err %lu", e);
}
TEST(dir_remove_and_recreate_loop) {
    REQUIRE(U(RemoveDirectoryA)); REQUIRE(U(CreateDirectoryA));
    char *d = dh_dir("rm_loop"), p[MAX_PATH];
    sub(p, sizeof p, d, "x");
    int ok = 0;
    for (int i = 0; i < 200; i++) { if (!U(CreateDirectoryA)(p, NULL)) break; if (!U(RemoveDirectoryA)(p)) break; ok++; }
    EXPECT_EQ(ok, 200);
}
TEST(dir_remove_junction_keeps_target) {
    REQUIRE(U(RemoveDirectoryA));
    char *d = dh_dir("rm_junc"), tgt[MAX_PATH], lnk[MAX_PATH];
    sub(tgt, sizeof tgt, d, "target"); CreateDirectoryA(tgt, NULL); dh_mkfile(tgt, "inside", 1);
    sub(lnk, sizeof lnk, d, "link"); CreateDirectoryA(lnk, NULL);
    if (!dh_mkjunction(lnk, tgt)) { t_skip("cannot create junction"); return; }
    CHECK(GetFileAttributesA(lnk) & FILE_ATTRIBUTE_REPARSE_POINT);
    CHECK(U(RemoveDirectoryA)(lnk));
    CHECK(!dh_exists(lnk));
    char f[MAX_PATH]; sub(f, sizeof f, tgt, "inside");
    CHECK(dh_exists(f));
}
TEST(dir_remove_dir_symlink) {
    REQUIRE(U(RemoveDirectoryA));
    char *d = dh_dir("rm_sym"), tgt[MAX_PATH], lnk[MAX_PATH];
    sub(tgt, sizeof tgt, d, "target"); CreateDirectoryA(tgt, NULL); dh_mkfile(tgt, "inside", 1);
    sub(lnk, sizeof lnk, d, "link");
    if (!dh_mksymlink(lnk, tgt, 1)) { t_skip("cannot create symlink (privilege)"); return; }
    CHECK(U(RemoveDirectoryA)(lnk));
    CHECK(!dh_exists(lnk));
    char f[MAX_PATH]; sub(f, sizeof f, tgt, "inside");
    CHECK(dh_exists(f));
}
TEST(dir_remove_unicode) {
    REQUIRE(U(RemoveDirectoryW));
    char *d = dh_dir("rm_uni"); WCHAR dw[MAX_PATH], w[MAX_PATH + 32];
    dh_w(dw, MAX_PATH, d);
    snwprintf(w, MAX_PATH + 32, L"%ls\\日本語", dw);
    CHECK(CreateDirectoryW(w, NULL));
    CHECK(U(RemoveDirectoryW)(w));
    CHECK(GetFileAttributesW(w) == INVALID_FILE_ATTRIBUTES);
}
TEST(dir_remove_long_over_248) {
    REQUIRE(U(RemoveDirectoryW));
    char *d = dh_dir("rm_long"); WCHAR dw[MAX_PATH], w[1024], raw[1100];
    dh_w(dw, MAX_PATH, d);
    int k = snwprintf(w, 1024, L"%ls\\", dw);
    int need = 270 - k; if (need > 255) need = 255; if (need < 1) { t_skip("prefix"); return; }
    for (int i = 0; i < need; i++) w[k + i] = 'z';
    w[k + need] = 0;
    snwprintf(raw, 1100, L"\\\\?\\%ls", w);
    if (!CreateDirectoryW(raw, NULL)) { t_note("cannot create long dir %lu", GetLastError()); return; }
    SET_ERR(0);
    BOOL ok = U(RemoveDirectoryW)(w);
    t_note("RemoveDirectoryW len %d without prefix: ok=%d err=%lu", k + need, ok, GetLastError());
    if (!ok) CHECK(U(RemoveDirectoryW)(raw));
}
TEST(dir_remove_raw_prefix) {
    REQUIRE(U(RemoveDirectoryW));
    char *d = dh_dir("rm_raw"), p[MAX_PATH]; WCHAR w[MAX_PATH], r[MAX_PATH + 8];
    sub(p, sizeof p, d, "x"); CreateDirectoryA(p, NULL); dh_w(w, MAX_PATH, p);
    snwprintf(r, MAX_PATH + 8, L"\\\\?\\%ls", w);
    CHECK(U(RemoveDirectoryW)(r));
    CHECK(!dh_exists(p));
}
TEST(dir_remove_device_name) {
    REQUIRE(U(RemoveDirectoryW));
    char *d = dh_dir("rm_dev"), p[MAX_PATH]; WCHAR w[MAX_PATH];
    sub(p, sizeof p, d, "NUL"); dh_w(w, MAX_PATH, p);
    SET_ERR(0);
    CHECK(!U(RemoveDirectoryW)(w));
    t_note("RemoveDirectory(NUL) err=%lu", GetLastError());
}
TEST(dir_remove_root_refused) {
    REQUIRE(U(RemoveDirectoryA));
    /* a drive that does not exist: must fail without side effects */
    DWORD mask = GetLogicalDrives(); char p[8] = "?:\\";
    int c; for (c = 25; c >= 0; c--) if (!(mask & (1u << c))) break;
    if (c < 0) { t_skip("no free drive letter"); return; }
    p[0] = 'A' + c;
    SET_ERR(0);
    CHECK(!U(RemoveDirectoryA)(p));
    DWORD e = GetLastError();
    CHECKF(e == ERROR_PATH_NOT_FOUND || e == ERROR_INVALID_DRIVE || e == ERROR_FILE_NOT_FOUND, "err %lu", e);
}
