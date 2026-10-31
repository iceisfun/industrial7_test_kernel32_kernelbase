/* MoveFile family, ReplaceFile, DeleteFile */
#include "fa.h"

static void pp(char *p, const char *leaf) { fa_path(p, leaf); }
static BOOL mvA(const char *s, const char *d) { return U(MoveFileA)(s, d); }
static BOOL mvW(const char *s, const char *d) { WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_widen(a, s); fa_widen(b, d); return U(MoveFileW)(a, b); }
static BOOL mvexW(const char *s, const char *d, DWORD f) { WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_widen(a, s); fa_widen(b, d); return U(MoveFileExW)(a, b, f); }
static BOOL mvexA(const char *s, const char *d, DWORD f) { return U(MoveFileExA)(s, d, f); }
static int real_ntfs(void) { return fa_is_ntfs() && !t_is_wine(); }

/* ---------------- MoveFile / MoveFileEx ---------------- */
TEST(fmove_basic_a_w) {
    REQUIRE(U(MoveFileA)); REQUIRE(U(MoveFileW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2], c[MAX_PATH * 2]; fa_mkfile_s("mv_a.txt", "moving"); pp(a, "mv_a.txt"); pp(b, "mv_b.txt"); pp(c, "mv_c.txt");
    CHECKF(mvA(a, b), "err %lu", GetLastError());
    CHECK(!fa_exists(a)); CHECK(fa_content_is(b, "moving"));
    CHECKF(mvW(b, c), "err %lu", GetLastError());
    CHECK(!fa_exists(b)); CHECK(fa_content_is(c, "moving"));
    fa_rm(c);
}
TEST(fmove_missing_source) {
    REQUIRE(U(MoveFileA)); REQUIRE(U(MoveFileW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; pp(a, "mv_none.txt"); pp(b, "mv_none_d.txt");
    SetLastError(0); EXPECT_EQ(mvA(a, b), FALSE); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    SetLastError(0); EXPECT_EQ(mvW(a, b), FALSE); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    pp(a, "mv_nodir\\x.txt");
    SetLastError(0); EXPECT_EQ(mvA(a, b), FALSE); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    CHECK(!fa_exists(b));
}
TEST(fmove_missing_dest_dir) {
    REQUIRE(U(MoveFileA)); REQUIRE(U(MoveFileW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("mv_md.txt", "x"); pp(a, "mv_md.txt"); pp(b, "mv_nodir\\d.txt");
    SetLastError(0); EXPECT_EQ(mvA(a, b), FALSE); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    SetLastError(0); EXPECT_EQ(mvW(a, b), FALSE); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    CHECK(fa_exists(a));
    fa_rm(a);
}
TEST(fmove_dest_exists_without_replace) {
    REQUIRE(U(MoveFileA)); REQUIRE(U(MoveFileW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("mv_e1.txt", "src"); fa_mkfile_s("mv_e2.txt", "dst"); pp(a, "mv_e1.txt"); pp(b, "mv_e2.txt");
    SetLastError(0); EXPECT_EQ(mvA(a, b), FALSE); EXPECT_ERR(ERROR_ALREADY_EXISTS);
    SetLastError(0); EXPECT_EQ(mvW(a, b), FALSE); EXPECT_ERR(ERROR_ALREADY_EXISTS);
    CHECK(fa_content_is(a, "src")); CHECK(fa_content_is(b, "dst"));
    fa_rm(a); fa_rm(b);
}
TEST(fmove_dest_exists_ex_flags0) {
    REQUIRE(U(MoveFileExA)); REQUIRE(U(MoveFileExW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("mv_x1.txt", "src"); fa_mkfile_s("mv_x2.txt", "dst"); pp(a, "mv_x1.txt"); pp(b, "mv_x2.txt");
    SetLastError(0); EXPECT_EQ(mvexA(a, b, 0), FALSE); EXPECT_ERR(ERROR_ALREADY_EXISTS);
    SetLastError(0); EXPECT_EQ(mvexW(a, b, 0), FALSE); EXPECT_ERR(ERROR_ALREADY_EXISTS);
    fa_rm(a); fa_rm(b);
}
TEST(fmove_replace_existing) {
    REQUIRE(U(MoveFileExA)); REQUIRE(U(MoveFileExW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("mv_r1.txt", "src"); fa_mkfile_s("mv_r2.txt", "dst"); pp(a, "mv_r1.txt"); pp(b, "mv_r2.txt");
    CHECKF(mvexA(a, b, MOVEFILE_REPLACE_EXISTING), "err %lu", GetLastError());
    CHECK(!fa_exists(a)); CHECK(fa_content_is(b, "src"));
    fa_mkfile_s("mv_r1.txt", "again");
    CHECKF(mvexW(a, b, MOVEFILE_REPLACE_EXISTING), "err %lu", GetLastError());
    CHECK(fa_content_is(b, "again"));
    fa_rm(b);
}
TEST(fmove_replace_existing_when_dest_absent) {
    REQUIRE(U(MoveFileExW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("mv_ra.txt", "src"); pp(a, "mv_ra.txt"); pp(b, "mv_rb.txt");
    CHECK(mvexW(a, b, MOVEFILE_REPLACE_EXISTING));
    CHECK(fa_content_is(b, "src"));
    fa_rm(b);
}
TEST(fmove_replace_readonly_dest_denied) {
    REQUIRE(U(MoveFileExW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("mv_ro1.txt", "src"); fa_mkfile_s("mv_ro2.txt", "dst"); pp(a, "mv_ro1.txt"); pp(b, "mv_ro2.txt");
    SetFileAttributesA(b, FILE_ATTRIBUTE_READONLY);
    SetLastError(0); EXPECT_EQ(mvexW(a, b, MOVEFILE_REPLACE_EXISTING), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CHECK(fa_content_is(a, "src")); CHECK(fa_content_is(b, "dst"));
    fa_rm(a); fa_rm(b);
}
TEST(fmove_replace_directory_dest_denied) {
    REQUIRE(U(MoveFileExW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("mv_rd1.txt", "src"); fa_mkdir("mv_rd2"); pp(a, "mv_rd1.txt"); pp(b, "mv_rd2");
    SetLastError(0); EXPECT_EQ(mvexW(a, b, MOVEFILE_REPLACE_EXISTING), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    fa_rm(a); fa_rm(b);
}
TEST(fmove_replace_dir_over_file_denied) {
    REQUIRE(U(MoveFileExW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkdir("mv_df1"); fa_mkfile_s("mv_df2.txt", "dst"); pp(a, "mv_df1"); pp(b, "mv_df2.txt");
    SetLastError(0); EXPECT_EQ(mvexW(a, b, MOVEFILE_REPLACE_EXISTING), FALSE);
    DWORD e = GetLastError(); CHECKF(e == ERROR_ACCESS_DENIED || e == ERROR_ALREADY_EXISTS, "err %lu", e);
    CHECK(fa_content_is(b, "dst"));
    fa_rm(a); fa_rm(b);
}
TEST(fmove_replace_nonempty_dir_over_dir) {
    REQUIRE(U(MoveFileExW));
    fa_mkdir("mv_dd1"); fa_mkdir("mv_dd2");
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; pp(a, "mv_dd1"); pp(b, "mv_dd2");
    SetLastError(0); EXPECT_EQ(mvexW(a, b, MOVEFILE_REPLACE_EXISTING), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    SetLastError(0); EXPECT_EQ(mvW(a, b), FALSE); EXPECT_ERR(ERROR_ALREADY_EXISTS);
    fa_rm(a); fa_rm(b);
}
TEST(fmove_directory_rename_with_contents) {
    REQUIRE(U(MoveFileW));
    fa_mkdir("mv_dr1"); fa_mkfile_s("mv_dr1\\f.txt", "inside");
    char a[MAX_PATH * 2], b[MAX_PATH * 2], f[MAX_PATH * 2]; pp(a, "mv_dr1"); pp(b, "mv_dr2"); pp(f, "mv_dr2\\f.txt");
    CHECKF(mvW(a, b), "err %lu", GetLastError());
    CHECK(!fa_exists(a)); CHECK(fa_content_is(f, "inside"));
    fa_rm(f); fa_rm(b);
}
TEST(fmove_directory_into_own_subtree) {
    REQUIRE(U(MoveFileW));
    fa_mkdir("mv_ds"); fa_mkdir("mv_ds\\sub");
    char a[MAX_PATH * 2], b[MAX_PATH * 2], s[MAX_PATH * 2]; pp(a, "mv_ds"); pp(b, "mv_ds\\sub\\ds"); pp(s, "mv_ds\\sub");
    SetLastError(0); EXPECT_EQ(mvW(a, b), FALSE);
    t_note("move dir into own subtree err=%lu", GetLastError());
    CHECK(fa_exists(a));
    fa_rm(s); fa_rm(a);
}
TEST(fmove_file_into_subdirectory) {
    REQUIRE(U(MoveFileA));
    fa_mkdir("mv_sd"); fa_mkfile_s("mv_sdf.txt", "x");
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; pp(a, "mv_sdf.txt"); pp(b, "mv_sd\\f.txt");
    CHECK(mvA(a, b)); CHECK(fa_exists(b));
    fa_rm(b); fa_rml("mv_sd");
}
TEST(fmove_to_existing_directory_name_with_trailing_slash) {
    REQUIRE(U(MoveFileA));
    fa_mkdir("mv_ts"); fa_mkfile_s("mv_tsf.txt", "x");
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; pp(a, "mv_tsf.txt"); pp(b, "mv_ts\\");
    SetLastError(0); BOOL r = mvA(a, b);
    t_note("move file onto 'dir\\' r=%d err=%lu", r, r ? 0 : GetLastError());
    CHECK(!r);
    fa_rm(a); fa_rml("mv_ts");
}
TEST(fmove_source_open_without_share_delete) {
    REQUIRE(U(MoveFileW)); REQUIRE(U(MoveFileExW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("mv_lk.txt", "locked"); pp(a, "mv_lk.txt"); pp(b, "mv_lk2.txt");
    HANDLE h = fa_open(a, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, 0);
    SetLastError(0); EXPECT_EQ(mvW(a, b), FALSE); EXPECT_ERR(ERROR_SHARING_VIOLATION);
    CHECK(fa_exists(a)); CHECK(!fa_exists(b));
    CloseHandle(h);
    CHECK(mvW(a, b));
    fa_rm(b);
}
TEST(fmove_source_open_with_share_delete) {
    REQUIRE(U(MoveFileW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("mv_sdl.txt", "shared"); pp(a, "mv_sdl.txt"); pp(b, "mv_sdl2.txt");
    HANDLE h = fa_open(a, GENERIC_READ | GENERIC_WRITE, FA_SHARE_ALL, OPEN_EXISTING, 0);
    CHECKF(mvW(a, b), "err %lu", GetLastError());
    DWORD n; CHECK(WriteFile(h, "S", 1, &n, NULL));
    CloseHandle(h);
    CHECK(fa_content_is(b, "Shared"));
    fa_rm(b);
}
TEST(fmove_source_open_for_read_only_share_read) {
    REQUIRE(U(MoveFileExW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("mv_sr.txt", "x"); pp(a, "mv_sr.txt"); pp(b, "mv_sr2.txt");
    HANDLE h = fa_open(a, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0);
    SetLastError(0); EXPECT_EQ(mvexW(a, b, MOVEFILE_REPLACE_EXISTING), FALSE); EXPECT_ERR(ERROR_SHARING_VIOLATION);
    CloseHandle(h); fa_rm(a);
}
TEST(fmove_dest_open_replace) {
    REQUIRE(U(MoveFileExW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("mv_do1.txt", "src"); fa_mkfile_s("mv_do2.txt", "dst"); pp(a, "mv_do1.txt"); pp(b, "mv_do2.txt");
    HANDLE h = fa_open(b, GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0);
    SetLastError(0); BOOL r = mvexW(a, b, MOVEFILE_REPLACE_EXISTING);
    DWORD e = GetLastError();
    CHECKF(!r && e == ERROR_ACCESS_DENIED, "replace over opened destination: r=%d err=%lu", r, e);
    CloseHandle(h); fa_rm(a); fa_rm(b);
}
TEST(fmove_preserves_attributes_and_times) {
    REQUIRE(U(MoveFileW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("mv_at1.txt", "attr"); pp(a, "mv_at1.txt"); pp(b, "mv_at2.txt");
    SetFileAttributesA(a, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
    WIN32_FILE_ATTRIBUTE_DATA x, y; GetFileAttributesExA(a, GetFileExInfoStandard, &x);
    CHECK(mvW(a, b));
    GetFileAttributesExA(b, GetFileExInfoStandard, &y);
    EXPECT_EQ(y.dwFileAttributes & 6, 6);
    EXPECT_EQ(fa_ft64(x.ftCreationTime), fa_ft64(y.ftCreationTime));
    EXPECT_EQ(fa_ft64(x.ftLastWriteTime), fa_ft64(y.ftLastWriteTime));
    SetFileAttributesA(b, FILE_ATTRIBUTE_NORMAL); fa_rm(b);
}
TEST(fmove_readonly_source_moves) {
    REQUIRE(U(MoveFileA));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("mv_rs1.txt", "ro"); pp(a, "mv_rs1.txt"); pp(b, "mv_rs2.txt");
    SetFileAttributesA(a, FILE_ATTRIBUTE_READONLY);
    CHECKF(mvA(a, b), "err %lu", GetLastError());
    CHECK(GetFileAttributesA(b) & FILE_ATTRIBUTE_READONLY);
    SetFileAttributesA(b, FILE_ATTRIBUTE_NORMAL); fa_rm(b);
}
TEST(fmove_case_only_rename) {
    REQUIRE(U(MoveFileW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("mv_case.txt", "x"); pp(a, "mv_case.txt"); pp(b, "MV_CASE.TXT");
    SetLastError(0);
    BOOL r = mvW(a, b);
    CHECKF(r, "err %lu", GetLastError());
    WIN32_FIND_DATAA fd; HANDLE f = FindFirstFileA(b, &fd);
    if (f != INVALID_HANDLE_VALUE) { if (fa_is_ntfs() && !t_is_wine()) EXPECT_STR(fd.cFileName, "MV_CASE.TXT"); FindClose(f); }
    fa_rm(b);
}
TEST(fmove_onto_itself) {
    REQUIRE(U(MoveFileW)); REQUIRE(U(MoveFileExW));
    char a[MAX_PATH * 2]; fa_mkfile_s("mv_self.txt", "me"); pp(a, "mv_self.txt");
    SetLastError(0); BOOL r = mvW(a, a);
    t_note("MoveFile onto itself r=%d err=%lu", r, r ? 0 : GetLastError());
    SetLastError(0); r = mvexW(a, a, MOVEFILE_REPLACE_EXISTING);
    t_note("MoveFileEx(replace) onto itself r=%d err=%lu", r, r ? 0 : GetLastError());
    CHECK(fa_content_is(a, "me"));
    fa_rm(a);
}
TEST(fmove_invalid_names) {
    REQUIRE(U(MoveFileW)); REQUIRE(U(MoveFileA));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("mv_inv.txt", "x"); pp(a, "mv_inv.txt");
    static const char *bad[] = { "mv_i*.txt", "mv_i?.txt", "mv_i<.txt", "mv_i>.txt", "mv_i\".txt", "mv_i|.txt" };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        pp(b, bad[i]);
        SetLastError(0); BOOL r = mvW(a, b); DWORD e = GetLastError();
        CHECKF(!r && e == ERROR_INVALID_NAME, "dest %s r=%d err=%lu", bad[i], r, e);
        SetLastError(0); r = mvA(a, b); e = GetLastError();
        CHECKF(!r && e == ERROR_INVALID_NAME, "A dest %s r=%d err=%lu", bad[i], r, e);
    }
    CHECK(fa_exists(a));
    fa_rm(a);
}
TEST(fmove_wildcard_source) {
    REQUIRE(U(MoveFileW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("mv_ws.txt", "x"); pp(a, "mv_w*.txt"); pp(b, "mv_ws2.txt");
    SetLastError(0); EXPECT_EQ(mvW(a, b), FALSE); EXPECT_ERR(ERROR_INVALID_NAME);
    CHECK(!fa_exists(b));
    fa_rml("mv_ws.txt");
}
TEST(fmove_null_arguments) {
    REQUIRE(U(MoveFileW)); REQUIRE(U(MoveFileExW));
    char a[MAX_PATH * 2]; fa_mkfile_s("mv_nl.txt", "x"); pp(a, "mv_nl.txt"); WCHAR w[MAX_PATH * 2]; fa_widen(w, a);
    SetLastError(0); EXPECT_EQ(U(MoveFileExW)(w, NULL, 0), FALSE); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    CHECK(fa_exists(a));
    SetLastError(0); EXPECT_EQ(U(MoveFileExW)(NULL, w, 0), FALSE); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    SetLastError(0); EXPECT_EQ(U(MoveFileW)(w, NULL), FALSE); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    SetLastError(0); EXPECT_EQ(U(MoveFileW)(NULL, NULL), FALSE);
    t_note("MoveFileW(NULL,NULL) err=%lu", GetLastError());
    CHECK(fa_exists(a));
    fa_rm(a);
}
TEST(fmove_invalid_flag_combination) {
    REQUIRE(U(MoveFileExW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("mv_fc.txt", "x"); pp(a, "mv_fc.txt"); pp(b, "mv_fc2.txt");
    /* DELAY_UNTIL_REBOOT | CREATE_HARDLINK is rejected immediately (before any registry access) */
    SetLastError(0); EXPECT_EQ(mvexW(a, b, MOVEFILE_DELAY_UNTIL_REBOOT | 0x10), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CHECK(fa_exists(a)); CHECK(!fa_exists(b));
    fa_rm(a);
}
TEST(fmove_write_through_flag) {
    REQUIRE(U(MoveFileExW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("mv_wt.txt", "wt"); pp(a, "mv_wt.txt"); pp(b, "mv_wt2.txt");
    CHECKF(mvexW(a, b, MOVEFILE_WRITE_THROUGH), "err %lu", GetLastError());
    CHECK(fa_content_is(b, "wt"));
    fa_mkfile_s("mv_wt.txt", "wt2");
    CHECKF(mvexW(a, b, MOVEFILE_WRITE_THROUGH | MOVEFILE_REPLACE_EXISTING), "err %lu", GetLastError());
    CHECK(fa_content_is(b, "wt2"));
    fa_rm(b);
}
TEST(fmove_copy_allowed_same_volume_is_rename) {
    REQUIRE(U(MoveFileExW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("mv_ca.txt", "ca"); pp(a, "mv_ca.txt"); pp(b, "mv_ca2.txt");
    WIN32_FILE_ATTRIBUTE_DATA x, y; GetFileAttributesExA(a, GetFileExInfoStandard, &x);
    CHECK(mvexW(a, b, MOVEFILE_COPY_ALLOWED));
    GetFileAttributesExA(b, GetFileExInfoStandard, &y);
    /* a rename keeps the creation time; a copy+delete would not */
    EXPECT_EQ(fa_ft64(x.ftCreationTime), fa_ft64(y.ftCreationTime));
    fa_rm(b);
}
TEST(fmove_create_hardlink_flag) {
    REQUIRE(U(MoveFileExW));
    FA_REQUIRE_NTFS();
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("mv_hl.txt", "link"); pp(a, "mv_hl.txt"); pp(b, "mv_hl2.txt");
    SetLastError(0);
    BOOL r = mvexW(a, b, 0x10);
    if (!r) { t_note("MOVEFILE_CREATE_HARDLINK err=%lu", GetLastError()); fa_rm(a); return; }
    CHECK(fa_exists(a)); CHECK(fa_content_is(b, "link"));
    HANDLE h = fa_open(a, GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0);
    BY_HANDLE_FILE_INFORMATION bi; GetFileInformationByHandle(h, &bi);
    EXPECT_EQ(bi.nNumberOfLinks, 2);
    CloseHandle(h);
    fa_rm(a); fa_rm(b);
}
TEST(fmove_hardlink_flag_existing_dest) {
    REQUIRE(U(MoveFileExW));
    FA_REQUIRE_NTFS();
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("mv_hl3.txt", "link"); fa_mkfile_s("mv_hl4.txt", "other"); pp(a, "mv_hl3.txt"); pp(b, "mv_hl4.txt");
    SetLastError(0); BOOL r = mvexW(a, b, 0x10);
    CHECK(!r);
    if (!r) EXPECT_ERR(ERROR_ALREADY_EXISTS);
    fa_rm(a); fa_rm(b);
}
TEST(fmove_stream_names) {
    REQUIRE(U(MoveFileW));
    FA_REQUIRE_STREAMS();
    char a[MAX_PATH * 2], b[MAX_PATH * 2], q[MAX_PATH * 2]; fa_mkfile_s("mv_as.txt", "main"); pp(a, "mv_as.txt"); pp(b, "mv_as2.txt");
    snprintf(q, sizeof q, "%s:s", a); fa_mkfile(q, "stream", 6);
    CHECK(mvW(a, b));
    snprintf(q, sizeof q, "%s:s", b); CHECK(fa_content_is(q, "stream"));
    fa_rm(b);
}
TEST(fmove_trailing_dot_normalization) {
    REQUIRE(U(MoveFileW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2], e[MAX_PATH * 2]; fa_mkfile_s("mv_td.txt", "x"); pp(a, "mv_td.txt"); pp(b, "mv_td2.txt.."); pp(e, "mv_td2.txt");
    CHECK(mvW(a, b)); CHECK(fa_exists(e));
    fa_rm(e);
}
TEST(fmove_prefixed_paths) {
    REQUIRE(U(MoveFileW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2], qa[MAX_PATH * 2], qb[MAX_PATH * 2]; fa_mkfile_s("mv_pf.txt", "pf"); pp(a, "mv_pf.txt"); pp(b, "mv_pf2.txt");
    snprintf(qa, sizeof qa, "\\\\?\\%s", a); snprintf(qb, sizeof qb, "\\\\?\\%s", b);
    CHECKF(mvW(qa, qb), "err %lu", GetLastError());
    CHECK(fa_content_is(b, "pf"));
    fa_rm(b);
}
TEST(fmove_unicode_names) {
    REQUIRE(U(MoveFileW));
    WCHAR p[MAX_PATH * 2], s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_wpath(p, "");
    wsprintfW(s, L"%lsmv_\x00e9\x4e2d.txt", p); wsprintfW(d, L"%lsmv_\x00e9\x4e2d_b.txt", p);
    HANDLE h = CreateFileW(s, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_skip("cannot create unicode names"); return; }
    CloseHandle(h);
    CHECKF(U(MoveFileW)(s, d), "err %lu", GetLastError());
    CHECK(GetFileAttributesW(d) != INVALID_FILE_ATTRIBUTES && GetFileAttributesW(s) == INVALID_FILE_ATTRIBUTES);
    DeleteFileW(d);
}
TEST(fmove_chain_rename_loop) {
    REQUIRE(U(MoveFileA));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("mv_cl_a.txt", "chain"); pp(a, "mv_cl_a.txt"); pp(b, "mv_cl_b.txt");
    int bad = 0;
    for (int i = 0; i < 200; i++) { if (!(i & 1 ? mvA(b, a) : mvA(a, b))) bad++; }
    EXPECT_EQ(bad, 0);
    CHECK(fa_content_is(a, "chain"));
    fa_rm(a);
}
TEST(fmove_ex_a_w_parity_errors) {
    REQUIRE(U(MoveFileExA)); REQUIRE(U(MoveFileExW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; pp(a, "mv_par_none.txt"); pp(b, "mv_par_d.txt");
    SetLastError(0); BOOL ra = mvexA(a, b, 0); DWORD ea = GetLastError();
    SetLastError(0); BOOL rw = mvexW(a, b, 0); DWORD ew = GetLastError();
    EXPECT_EQ(ra, rw); EXPECT_EQ(ea, ew);
}

/* MoveFileWithProgress */
typedef struct { int calls; DWORD ret; } mctx;
static DWORD CALLBACK mcb(LARGE_INTEGER t, LARGE_INTEGER x, LARGE_INTEGER st, LARGE_INTEGER sx, DWORD sn, DWORD reason, HANDLE hs, HANDLE hd, LPVOID data) {
    mctx *c = (mctx *)data; c->calls++; return c->ret;
}
TEST(fmove_progress_same_volume_no_callbacks) {
    REQUIRE(U(MoveFileWithProgressW)); REQUIRE(U(MoveFileWithProgressA));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("mp_a.txt", "progress"); pp(a, "mp_a.txt"); pp(b, "mp_b.txt");
    WCHAR wa[MAX_PATH * 2], wb[MAX_PATH * 2]; fa_widen(wa, a); fa_widen(wb, b);
    mctx c = { 0, PROGRESS_CONTINUE };
    CHECKF(U(MoveFileWithProgressW)(wa, wb, mcb, &c, MOVEFILE_COPY_ALLOWED), "err %lu", GetLastError());
    EXPECT_EQ(c.calls, 0);
    CHECK(fa_content_is(b, "progress"));
    CHECKF(U(MoveFileWithProgressA)(b, a, mcb, &c, 0), "err %lu", GetLastError());
    EXPECT_EQ(c.calls, 0);
    CHECK(fa_content_is(a, "progress"));
    fa_rm(a);
}
TEST(fmove_progress_null_routine) {
    REQUIRE(U(MoveFileWithProgressW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("mp_n.txt", "x"); pp(a, "mp_n.txt"); pp(b, "mp_n2.txt");
    WCHAR wa[MAX_PATH * 2], wb[MAX_PATH * 2]; fa_widen(wa, a); fa_widen(wb, b);
    CHECK(U(MoveFileWithProgressW)(wa, wb, NULL, NULL, 0));
    SetLastError(0); EXPECT_EQ(U(MoveFileWithProgressW)(wa, wb, NULL, NULL, 0), FALSE); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    fa_rm(b);
}
TEST(fmove_progress_errors_match_moveex) {
    REQUIRE(U(MoveFileWithProgressW)); REQUIRE(U(MoveFileExW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("mp_e1.txt", "1"); fa_mkfile_s("mp_e2.txt", "2"); pp(a, "mp_e1.txt"); pp(b, "mp_e2.txt");
    WCHAR wa[MAX_PATH * 2], wb[MAX_PATH * 2]; fa_widen(wa, a); fa_widen(wb, b);
    SetLastError(0); EXPECT_EQ(U(MoveFileWithProgressW)(wa, wb, NULL, NULL, 0), FALSE); EXPECT_ERR(ERROR_ALREADY_EXISTS);
    CHECK(U(MoveFileWithProgressW)(wa, wb, NULL, NULL, MOVEFILE_REPLACE_EXISTING));
    CHECK(fa_content_is(b, "1"));
    fa_rm(b);
}
TEST(fmove_progress_invalid_flags) {
    REQUIRE(U(MoveFileWithProgressW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("mp_f.txt", "x"); pp(a, "mp_f.txt"); pp(b, "mp_f2.txt");
    WCHAR wa[MAX_PATH * 2], wb[MAX_PATH * 2]; fa_widen(wa, a); fa_widen(wb, b);
    SetLastError(0); EXPECT_EQ(U(MoveFileWithProgressW)(wa, wb, NULL, NULL, MOVEFILE_DELAY_UNTIL_REBOOT | 0x10), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CHECK(fa_exists(a)); fa_rm(a);
}

/* ---------------- DeleteFile ---------------- */
TEST(fdelete_basic_a_w) {
    REQUIRE(U(DeleteFileA)); REQUIRE(U(DeleteFileW));
    char p[MAX_PATH * 2]; fa_mkfile_s("df_a.txt", "x"); pp(p, "df_a.txt");
    CHECKF(U(DeleteFileA)(p), "err %lu", GetLastError()); CHECK(!fa_exists(p));
    fa_mkfile_s("df_a.txt", "x"); WCHAR w[MAX_PATH * 2]; fa_widen(w, p);
    CHECKF(U(DeleteFileW)(w), "err %lu", GetLastError()); CHECK(!fa_exists(p));
}
TEST(fdelete_missing) {
    REQUIRE(U(DeleteFileA)); REQUIRE(U(DeleteFileW));
    char p[MAX_PATH * 2]; pp(p, "df_none.txt"); WCHAR w[MAX_PATH * 2]; fa_widen(w, p);
    SetLastError(0); EXPECT_EQ(U(DeleteFileA)(p), FALSE); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    SetLastError(0); EXPECT_EQ(U(DeleteFileW)(w), FALSE); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    pp(p, "df_nodir\\x.txt");
    SetLastError(0); EXPECT_EQ(U(DeleteFileA)(p), FALSE); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(fdelete_twice) {
    REQUIRE(U(DeleteFileA));
    char p[MAX_PATH * 2]; fa_mkfile_s("df_tw.txt", "x"); pp(p, "df_tw.txt");
    CHECK(U(DeleteFileA)(p));
    SetLastError(0); EXPECT_EQ(U(DeleteFileA)(p), FALSE); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(fdelete_readonly_denied) {
    REQUIRE(U(DeleteFileA)); REQUIRE(U(DeleteFileW));
    char p[MAX_PATH * 2]; fa_mkfile_s("df_ro.txt", "x"); pp(p, "df_ro.txt"); WCHAR w[MAX_PATH * 2]; fa_widen(w, p);
    SetFileAttributesA(p, FILE_ATTRIBUTE_READONLY);
    SetLastError(0); EXPECT_EQ(U(DeleteFileA)(p), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    SetLastError(0); EXPECT_EQ(U(DeleteFileW)(w), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CHECK(fa_exists(p));
    SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL);
    CHECK(U(DeleteFileA)(p));
}
TEST(fdelete_hidden_system_ok) {
    REQUIRE(U(DeleteFileA));
    char p[MAX_PATH * 2]; fa_mkfile_s("df_hs.txt", "x"); pp(p, "df_hs.txt");
    SetFileAttributesA(p, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
    CHECKF(U(DeleteFileA)(p), "err %lu", GetLastError()); CHECK(!fa_exists(p));
}
TEST(fdelete_directory_denied) {
    REQUIRE(U(DeleteFileA)); REQUIRE(U(DeleteFileW));
    char p[MAX_PATH * 2]; fa_mkdir("df_dir"); pp(p, "df_dir"); WCHAR w[MAX_PATH * 2]; fa_widen(w, p);
    SetLastError(0); EXPECT_EQ(U(DeleteFileA)(p), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    SetLastError(0); EXPECT_EQ(U(DeleteFileW)(w), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CHECK(fa_exists(p)); fa_rm(p);
}
TEST(fdelete_open_without_share_delete) {
    REQUIRE(U(DeleteFileA));
    char p[MAX_PATH * 2]; fa_mkfile_s("df_op.txt", "x"); pp(p, "df_op.txt");
    HANDLE h = fa_open(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, 0);
    SetLastError(0); EXPECT_EQ(U(DeleteFileA)(p), FALSE); EXPECT_ERR(ERROR_SHARING_VIOLATION);
    CHECK(fa_exists(p));
    CloseHandle(h);
    CHECK(U(DeleteFileA)(p));
}
TEST(fdelete_open_exclusive) {
    REQUIRE(U(DeleteFileA));
    char p[MAX_PATH * 2]; fa_mkfile_s("df_ex.txt", "x"); pp(p, "df_ex.txt");
    HANDLE h = fa_open(p, GENERIC_READ | GENERIC_WRITE, 0, OPEN_EXISTING, 0);
    SetLastError(0); EXPECT_EQ(U(DeleteFileA)(p), FALSE); EXPECT_ERR(ERROR_SHARING_VIOLATION);
    CloseHandle(h); fa_rm(p);
}
TEST(fdelete_open_with_share_delete) {
    REQUIRE(U(DeleteFileA));
    char p[MAX_PATH * 2]; fa_mkfile_s("df_sd.txt", "x"); pp(p, "df_sd.txt");
    HANDLE h = fa_open(p, GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0);
    CHECKF(U(DeleteFileA)(p), "err %lu", GetLastError());
    DWORD a = GetFileAttributesA(p);
    t_note("name state while handle still open: attr=0x%lx err=%lu", a, a == INVALID_FILE_ATTRIBUTES ? GetLastError() : 0);
    /* the handle itself stays usable */
    char b[8]; DWORD n = 0; BOOL rd = ReadFile(h, b, 1, &n, NULL);
    CHECK(rd);
    CloseHandle(h);
    CHECK(!fa_exists(p));
    SetLastError(0); EXPECT_EQ(U(DeleteFileA)(p), FALSE); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(fdelete_share_delete_handle_with_write_access) {
    REQUIRE(U(DeleteFileA));
    char p[MAX_PATH * 2]; fa_mkfile_s("df_sw.txt", "x"); pp(p, "df_sw.txt");
    HANDLE h = fa_open(p, GENERIC_READ | GENERIC_WRITE, FA_SHARE_ALL, OPEN_EXISTING, 0);
    CHECKF(U(DeleteFileA)(p), "err %lu", GetLastError());
    DWORD n; CHECK(WriteFile(h, "y", 1, &n, NULL));
    CloseHandle(h);
    CHECK(!fa_exists(p));
}
TEST(fdelete_recreate_while_pending) {
    REQUIRE(U(DeleteFileA));
    FA_REQUIRE_NTFS();
    char p[MAX_PATH * 2]; fa_mkfile_s("df_rc.txt", "x"); pp(p, "df_rc.txt");
    HANDLE h = fa_open(p, GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0);
    CHECK(U(DeleteFileA)(p));
    SetLastError(0);
    HANDLE n = fa_open(p, GENERIC_WRITE, FA_SHARE_ALL, CREATE_NEW, 0);
    DWORD e = GetLastError();
    t_note("CREATE_NEW while a deleted file is still open: %s err=%lu", n == INVALID_HANDLE_VALUE ? "failed" : "ok", e);
    if (n == INVALID_HANDLE_VALUE) CHECKF(e == ERROR_ACCESS_DENIED, "err %lu", e);
    if (n != INVALID_HANDLE_VALUE) CloseHandle(n);
    CloseHandle(h); fa_rm(p);
}
TEST(fdelete_wildcard_and_invalid) {
    REQUIRE(U(DeleteFileA)); REQUIRE(U(DeleteFileW));
    char keep[MAX_PATH * 2], p[MAX_PATH * 2]; fa_mkfile_s("df_keep.txt", "x"); pp(keep, "df_keep.txt");
    static const char *bad[] = { "df_k*", "df_keep.t?t", "df_<", "df_>", "df_\"", "df_|" };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        pp(p, bad[i]); WCHAR w[MAX_PATH * 2]; fa_widen(w, p);
        SetLastError(0); EXPECT_EQ(U(DeleteFileA)(p), FALSE); EXPECT_ERR(ERROR_INVALID_NAME);
        SetLastError(0); EXPECT_EQ(U(DeleteFileW)(w), FALSE); EXPECT_ERR(ERROR_INVALID_NAME);
    }
    CHECK(fa_exists(keep)); fa_rm(keep);
}
TEST(fdelete_null_and_empty) {
    REQUIRE(U(DeleteFileW)); REQUIRE(U(DeleteFileA));
    SetLastError(0); EXPECT_EQ(U(DeleteFileW)(NULL), FALSE);
    DWORD e = GetLastError(); CHECKF(e == ERROR_PATH_NOT_FOUND || e == ERROR_INVALID_PARAMETER || e == ERROR_FILE_NOT_FOUND || e == ERROR_INVALID_NAME, "err %lu", e);
    t_note("DeleteFileW(NULL) err=%lu", e);
    SetLastError(0); EXPECT_EQ(U(DeleteFileW)(L""), FALSE);
    t_note("DeleteFileW(\"\") err=%lu", GetLastError());
    SetLastError(0); EXPECT_EQ(U(DeleteFileA)(""), FALSE);
}
TEST(fdelete_stream_only) {
    REQUIRE(U(DeleteFileA));
    FA_REQUIRE_STREAMS();
    char p[MAX_PATH * 2], q[MAX_PATH * 2]; fa_mkfile_s("df_st.txt", "main"); pp(p, "df_st.txt");
    snprintf(q, sizeof q, "%s:s", p); fa_mkfile(q, "stream", 6);
    CHECKF(U(DeleteFileA)(q), "err %lu", GetLastError());
    CHECK(fa_content_is(p, "main"));
    CHECK(GetFileAttributesA(q) == INVALID_FILE_ATTRIBUTES);
    fa_rm(p);
}
TEST(fdelete_hardlink_leaves_other_name) {
    REQUIRE(U(DeleteFileA));
    FA_REQUIRE_NTFS();
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("df_hl1.txt", "linked"); pp(a, "df_hl1.txt"); pp(b, "df_hl2.txt");
    if (!CreateHardLinkA(b, a, NULL)) { t_skip("no hard link"); fa_rm(a); return; }
    CHECK(U(DeleteFileA)(a));
    CHECK(!fa_exists(a)); CHECK(fa_content_is(b, "linked"));
    fa_rm(b);
}
TEST(fdelete_trailing_slash_on_file) {
    REQUIRE(U(DeleteFileA));
    char p[MAX_PATH * 2]; fa_mkfile_s("df_sl.txt", "x"); pp(p, "df_sl.txt\\");
    SetLastError(0); BOOL r = U(DeleteFileA)(p);
    t_note("DeleteFile('file\\') r=%d err=%lu", r, r ? 0 : GetLastError());
    CHECK(!r);
    fa_rml("df_sl.txt");
}
TEST(fdelete_trailing_dots_normalized) {
    REQUIRE(U(DeleteFileA));
    char p[MAX_PATH * 2]; fa_mkfile_s("df_td.txt", "x"); pp(p, "df_td.txt. ");
    CHECK(U(DeleteFileA)(p));
    pp(p, "df_td.txt"); CHECK(!fa_exists(p));
}
TEST(fdelete_large_file) {
    REQUIRE(U(DeleteFileA));
    char p[MAX_PATH * 2]; pp(p, "df_big.bin");
    BYTE *b = calloc(1, 4 << 20); fa_mkfile(p, b, 4 << 20); free(b);
    CHECK(U(DeleteFileA)(p)); CHECK(!fa_exists(p));
}
TEST(fdelete_many_files_loop) {
    REQUIRE(U(DeleteFileA));
    int bad = 0;
    for (int i = 0; i < 200; i++) {
        char leaf[64], p[MAX_PATH * 2]; snprintf(leaf, sizeof leaf, "df_many_%d.txt", i);
        fa_mkfile_s(leaf, "x"); fa_path(p, leaf);
        if (!U(DeleteFileA)(p) || fa_exists(p)) bad++;
    }
    EXPECT_EQ(bad, 0);
}
TEST(fdelete_unicode_name) {
    REQUIRE(U(DeleteFileW));
    WCHAR p[MAX_PATH * 2], q[MAX_PATH * 2]; fa_wpath(p, "");
    wsprintfW(q, L"%ldf_\x00e9\x4e2d.txt", p);
    wsprintfW(q, L"%lsdf_\x00e9\x4e2d.txt", p);
    HANDLE h = CreateFileW(q, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_skip("cannot create"); return; }
    CloseHandle(h);
    CHECK(U(DeleteFileW)(q)); CHECK(GetFileAttributesW(q) == INVALID_FILE_ATTRIBUTES);
}
TEST(fdelete_in_readonly_directory) {
    REQUIRE(U(DeleteFileA));
    fa_mkdir("df_rodir"); fa_mkfile_s("df_rodir\\f.txt", "x");
    char d[MAX_PATH * 2], f[MAX_PATH * 2]; pp(d, "df_rodir"); pp(f, "df_rodir\\f.txt");
    SetFileAttributesA(d, FILE_ATTRIBUTE_READONLY);
    CHECKF(U(DeleteFileA)(f), "err %lu", GetLastError());
    SetFileAttributesA(d, FILE_ATTRIBUTE_NORMAL); fa_rm(d);
}

/* ---------------- ReplaceFile ---------------- */
static BOOL repW(const char *a, const char *b, const char *bk, DWORD fl, LPVOID ex, LPVOID rs) {
    WCHAR wa[MAX_PATH * 2], wb[MAX_PATH * 2], wk[MAX_PATH * 2];
    fa_widen(wa, a); fa_widen(wb, b); if (bk) fa_widen(wk, bk);
    return U(ReplaceFileW)(wa, wb, bk ? wk : NULL, fl, ex, rs);
}
TEST(freplace_basic_no_backup) {
    REQUIRE(U(ReplaceFileA)); REQUIRE(U(ReplaceFileW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("rp_a.txt", "original"); fa_mkfile_s("rp_b.txt", "replacement"); pp(a, "rp_a.txt"); pp(b, "rp_b.txt");
    CHECKF(U(ReplaceFileA)(a, b, NULL, 0, NULL, NULL), "err %lu", GetLastError());
    CHECK(fa_content_is(a, "replacement")); CHECK(!fa_exists(b));
    fa_mkfile_s("rp_b.txt", "again");
    CHECKF(repW(a, b, NULL, 0, NULL, NULL), "err %lu", GetLastError());
    CHECK(fa_content_is(a, "again")); CHECK(!fa_exists(b));
    fa_rm(a);
}
TEST(freplace_with_backup) {
    REQUIRE(U(ReplaceFileA));
    char a[MAX_PATH * 2], b[MAX_PATH * 2], k[MAX_PATH * 2]; fa_mkfile_s("rp2_a.txt", "original"); fa_mkfile_s("rp2_b.txt", "replacement"); pp(a, "rp2_a.txt"); pp(b, "rp2_b.txt"); pp(k, "rp2_k.txt");
    CHECKF(U(ReplaceFileA)(a, b, k, 0, NULL, NULL), "err %lu", GetLastError());
    CHECK(fa_content_is(a, "replacement")); CHECK(fa_content_is(k, "original")); CHECK(!fa_exists(b));
    fa_rm(a); fa_rm(k);
}
TEST(freplace_with_backup_w_and_existing_backup) {
    REQUIRE(U(ReplaceFileW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2], k[MAX_PATH * 2]; fa_mkfile_s("rp3_a.txt", "original"); fa_mkfile_s("rp3_b.txt", "replacement"); fa_mkfile_s("rp3_k.txt", "stale backup"); pp(a, "rp3_a.txt"); pp(b, "rp3_b.txt"); pp(k, "rp3_k.txt");
    CHECKF(repW(a, b, k, 0, NULL, NULL), "err %lu", GetLastError());
    CHECK(fa_content_is(a, "replacement")); CHECK(fa_content_is(k, "original"));
    fa_rm(a); fa_rm(k);
}
TEST(freplace_flags_valid_range) {
    REQUIRE(U(ReplaceFileW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2];
    for (DWORD f = 0; f < 8; f++) {
        fa_mkfile_s("rp4_a.txt", "o"); fa_mkfile_s("rp4_b.txt", "n"); pp(a, "rp4_a.txt"); pp(b, "rp4_b.txt");
        SetLastError(0);
        BOOL r = repW(a, b, NULL, f, NULL, NULL);
        CHECKF(r || GetLastError() != ERROR_INVALID_PARAMETER, "flags %lu rejected", f);
        if (!r) t_note("ReplaceFile flags=%lu -> err %lu", f, GetLastError());
        fa_rm(a); fa_rm(b);
    }
}
TEST(freplace_invalid_flags) {
    REQUIRE(U(ReplaceFileW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("rp5_a.txt", "o"); fa_mkfile_s("rp5_b.txt", "n"); pp(a, "rp5_a.txt"); pp(b, "rp5_b.txt");
    static const DWORD bad[] = { 8, 0x10, 0x100, 0x80000000, 0xffffffff };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SetLastError(0); EXPECT_EQ(repW(a, b, NULL, bad[i], NULL, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
    CHECK(fa_content_is(a, "o")); CHECK(fa_content_is(b, "n"));
    fa_rm(a); fa_rm(b);
}
TEST(freplace_reserved_params_must_be_null) {
    REQUIRE(U(ReplaceFileW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("rp6_a.txt", "o"); fa_mkfile_s("rp6_b.txt", "n"); pp(a, "rp6_a.txt"); pp(b, "rp6_b.txt");
    int dummy;
    SetLastError(0); EXPECT_EQ(repW(a, b, NULL, 0, &dummy, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SetLastError(0); EXPECT_EQ(repW(a, b, NULL, 0, NULL, &dummy), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CHECK(fa_content_is(a, "o")); CHECK(fa_content_is(b, "n"));
    fa_rm(a); fa_rm(b);
}
TEST(freplace_null_names) {
    REQUIRE(U(ReplaceFileW));
    WCHAR w[] = L"x";
    SetLastError(0); EXPECT_EQ(U(ReplaceFileW)(NULL, w, NULL, 0, NULL, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SetLastError(0); EXPECT_EQ(U(ReplaceFileW)(w, NULL, NULL, 0, NULL, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SetLastError(0); EXPECT_EQ(U(ReplaceFileW)(NULL, NULL, NULL, 0, NULL, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SetLastError(0); EXPECT_EQ(U(ReplaceFileA)(NULL, "x", NULL, 0, NULL, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(freplace_missing_replaced_file) {
    REQUIRE(U(ReplaceFileW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("rp7_b.txt", "n"); pp(a, "rp7_missing.txt"); pp(b, "rp7_b.txt");
    SetLastError(0); BOOL r = repW(a, b, NULL, 0, NULL, NULL); DWORD e = GetLastError();
    CHECK(!r);
    CHECKF(e == ERROR_FILE_NOT_FOUND || e == ERROR_UNABLE_TO_REMOVE_REPLACED || e == ERROR_UNABLE_TO_MOVE_REPLACEMENT, "err %lu", e);
    t_note("ReplaceFile with missing replaced file err=%lu", e);
    CHECK(fa_content_is(b, "n")); CHECK(!fa_exists(a));
    fa_rm(b);
}
TEST(freplace_missing_replacement_file) {
    REQUIRE(U(ReplaceFileW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("rp8_a.txt", "o"); pp(a, "rp8_a.txt"); pp(b, "rp8_missing.txt");
    SetLastError(0); BOOL r = repW(a, b, NULL, 0, NULL, NULL); DWORD e = GetLastError();
    CHECK(!r);
    CHECKF(e == ERROR_FILE_NOT_FOUND || e == ERROR_UNABLE_TO_MOVE_REPLACEMENT || e == ERROR_UNABLE_TO_REMOVE_REPLACED, "err %lu", e);
    t_note("ReplaceFile with missing replacement err=%lu", e);
    CHECK(fa_content_is(a, "o"));
    fa_rm(a);
}
TEST(freplace_missing_both) {
    REQUIRE(U(ReplaceFileW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; pp(a, "rp9_a.txt"); pp(b, "rp9_b.txt");
    SetLastError(0); EXPECT_EQ(repW(a, b, NULL, 0, NULL, NULL), FALSE);
    t_note("ReplaceFile with both missing err=%lu", GetLastError());
}
TEST(freplace_missing_dir) {
    REQUIRE(U(ReplaceFileW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; pp(a, "rp10_nodir\\a.txt"); pp(b, "rp10_nodir\\b.txt");
    SetLastError(0); EXPECT_EQ(repW(a, b, NULL, 0, NULL, NULL), FALSE);
    t_note("ReplaceFile in missing dir err=%lu", GetLastError());
}
TEST(freplace_same_file) {
    REQUIRE(U(ReplaceFileW));
    char a[MAX_PATH * 2]; fa_mkfile_s("rp11.txt", "same"); pp(a, "rp11.txt");
    SetLastError(0); BOOL r = repW(a, a, NULL, 0, NULL, NULL);
    t_note("ReplaceFile(same, same) r=%d err=%lu", r, r ? 0 : GetLastError());
    CHECK(!r);
    fa_rm(a);
}
TEST(freplace_replaced_readonly) {
    REQUIRE(U(ReplaceFileW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("rp12_a.txt", "o"); fa_mkfile_s("rp12_b.txt", "n"); pp(a, "rp12_a.txt"); pp(b, "rp12_b.txt");
    SetFileAttributesA(a, FILE_ATTRIBUTE_READONLY);
    SetLastError(0); BOOL r = repW(a, b, NULL, 0, NULL, NULL);
    t_note("ReplaceFile with readonly replaced file: r=%d err=%lu", r, r ? 0 : GetLastError());
    SetFileAttributesA(a, FILE_ATTRIBUTE_NORMAL); fa_rm(a); fa_rm(b);
}
TEST(freplace_replaced_open_exclusive) {
    REQUIRE(U(ReplaceFileW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("rp13_a.txt", "o"); fa_mkfile_s("rp13_b.txt", "n"); pp(a, "rp13_a.txt"); pp(b, "rp13_b.txt");
    HANDLE h = fa_open(a, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0);
    SetLastError(0); BOOL r = repW(a, b, NULL, 0, NULL, NULL); DWORD e = GetLastError();
    CHECK(!r);
    CHECKF(e == ERROR_SHARING_VIOLATION || e == ERROR_UNABLE_TO_REMOVE_REPLACED || e == ERROR_ACCESS_DENIED, "err %lu", e);
    t_note("ReplaceFile with locked replaced file err=%lu", e);
    CHECK(fa_content_is(b, "n"));
    CloseHandle(h); fa_rm(a); fa_rm(b);
}
TEST(freplace_replacement_open_exclusive) {
    REQUIRE(U(ReplaceFileW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("rp14_a.txt", "o"); fa_mkfile_s("rp14_b.txt", "n"); pp(a, "rp14_a.txt"); pp(b, "rp14_b.txt");
    HANDLE h = fa_open(b, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0);
    SetLastError(0); BOOL r = repW(a, b, NULL, 0, NULL, NULL); DWORD e = GetLastError();
    CHECK(!r);
    t_note("ReplaceFile with locked replacement err=%lu", e);
    CHECK(fa_content_is(a, "o"));
    CloseHandle(h); fa_rm(a); fa_rm(b);
}
TEST(freplace_keeps_replaced_attributes) {
    REQUIRE(U(ReplaceFileW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("rp15_a.txt", "o"); fa_mkfile_s("rp15_b.txt", "n"); pp(a, "rp15_a.txt"); pp(b, "rp15_b.txt");
    SetFileAttributesA(a, FILE_ATTRIBUTE_HIDDEN);
    CHECKF(repW(a, b, NULL, 0, NULL, NULL), "err %lu", GetLastError());
    DWORD at = GetFileAttributesA(a);
    t_note("attributes after ReplaceFile of a hidden file: 0x%lx", at);
    SetFileAttributesA(a, FILE_ATTRIBUTE_NORMAL); fa_rm(a);
}
TEST(freplace_creation_time_note) {
    REQUIRE(U(ReplaceFileW));
    if (!real_ntfs()) { t_skip("needs real NTFS"); return; }
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("rp16_a.txt", "o"); fa_mkfile_s("rp16_b.txt", "n"); pp(a, "rp16_a.txt"); pp(b, "rp16_b.txt");
    HANDLE h = fa_open(a, GENERIC_WRITE, FA_SHARE_ALL, OPEN_EXISTING, 0);
    FILETIME t = fa_mkft(FA_FT_2001 + 77 * FA_SEC); SetFileTime(h, &t, NULL, NULL); CloseHandle(h);
    CHECK(repW(a, b, NULL, 0, NULL, NULL));
    WIN32_FILE_ATTRIBUTE_DATA d; GetFileAttributesExA(a, GetFileExInfoStandard, &d);
    t_note("creation time after ReplaceFile: %llx (replaced file had %llx)", fa_ft64(d.ftCreationTime), (unsigned long long)(FA_FT_2001 + 77 * FA_SEC));
    fa_rm(a);
}
TEST(freplace_backup_same_as_replaced) {
    REQUIRE(U(ReplaceFileW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("rp17_a.txt", "o"); fa_mkfile_s("rp17_b.txt", "n"); pp(a, "rp17_a.txt"); pp(b, "rp17_b.txt");
    SetLastError(0); BOOL r = repW(a, b, a, 0, NULL, NULL);
    t_note("ReplaceFile with backup == replaced: r=%d err=%lu", r, r ? 0 : GetLastError());
    fa_rm(a); fa_rm(b);
}
TEST(freplace_backup_missing_dir) {
    REQUIRE(U(ReplaceFileW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2], k[MAX_PATH * 2]; fa_mkfile_s("rp18_a.txt", "o"); fa_mkfile_s("rp18_b.txt", "n"); pp(a, "rp18_a.txt"); pp(b, "rp18_b.txt"); pp(k, "rp18_nodir\\k.txt");
    SetLastError(0); BOOL r = repW(a, b, k, 0, NULL, NULL);
    t_note("ReplaceFile with backup in missing dir: r=%d err=%lu", r, r ? 0 : GetLastError());
    CHECK(!r);
    if (!r) { CHECK(fa_content_is(a, "o")); CHECK(fa_content_is(b, "n")); }
    fa_rm(a); fa_rm(b);
}
TEST(freplace_ignore_merge_errors_flag) {
    REQUIRE(U(ReplaceFileW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("rp19_a.txt", "o"); fa_mkfile_s("rp19_b.txt", "n"); pp(a, "rp19_a.txt"); pp(b, "rp19_b.txt");
    CHECKF(repW(a, b, NULL, REPLACEFILE_IGNORE_MERGE_ERRORS, NULL, NULL), "err %lu", GetLastError());
    CHECK(fa_content_is(a, "n"));
    fa_rm(a);
}
TEST(freplace_ignore_acl_errors_flag) {
    REQUIRE(U(ReplaceFileW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_mkfile_s("rp20_a.txt", "o"); fa_mkfile_s("rp20_b.txt", "n"); pp(a, "rp20_a.txt"); pp(b, "rp20_b.txt");
    CHECKF(repW(a, b, NULL, REPLACEFILE_IGNORE_ACL_ERRORS, NULL, NULL), "err %lu", GetLastError());
    CHECK(fa_content_is(a, "n"));
    fa_rm(a);
}
TEST(freplace_a_w_parity_error) {
    REQUIRE(U(ReplaceFileA)); REQUIRE(U(ReplaceFileW));
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; pp(a, "rp21_a.txt"); pp(b, "rp21_b.txt");
    SetLastError(0); BOOL ra = U(ReplaceFileA)(a, b, NULL, 0, NULL, NULL); DWORD ea = GetLastError();
    SetLastError(0); BOOL rw = repW(a, b, NULL, 0, NULL, NULL); DWORD ew = GetLastError();
    EXPECT_EQ(ra, rw); EXPECT_EQ(ea, ew);
}
