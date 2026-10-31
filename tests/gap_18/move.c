#include "g18.h"
/* gap18: MoveFileA, MoveFileExA, MoveFileWithProgressA. Never uses MOVEFILE_DELAY_UNTIL_REBOOT
 * with a real effect (it would write PendingFileRenameOperations); only the invalid
 * combination that is rejected before anything happens. */

typedef BOOL (WINAPI *mv_ex_t)(LPCSTR, LPCSTR, DWORD);
typedef BOOL (WINAPI *mv_t)(LPCSTR, LPCSTR);

static void mk(const char *leaf, char *out, const char *content) { g18_path(out, MAX_PATH, leaf); g18_put(out, content, (DWORD)strlen(content)); }

TEST(gap18_movea_basic) {
    REQUIRE(U(MoveFileA));
    char s[MAX_PATH], d[MAX_PATH]; mk("mv_b_s.txt", s, "move me"); g18_path(d, MAX_PATH, "mv_b_d.txt"); g18_rm(d);
    SET_ERR(0);
    CHECK(U(MoveFileA)(s, d));
    CHECK(!g18_exists(s));
    CHECK(g18_content_is(d, "move me"));
    g18_rm(d);
}
TEST(gap18_movea_dest_exists) {
    REQUIRE(U(MoveFileA));
    char s[MAX_PATH], d[MAX_PATH]; mk("mv_de_s.txt", s, "src"); mk("mv_de_d.txt", d, "dst");
    SET_ERR(0);
    CHECK(!U(MoveFileA)(s, d));
    EXPECT_ERR(ERROR_ALREADY_EXISTS);
    CHECK(g18_content_is(s, "src")); CHECK(g18_content_is(d, "dst"));
    g18_rm(s); g18_rm(d);
}
TEST(gap18_movea_missing_source) {
    REQUIRE(U(MoveFileA));
    char s[MAX_PATH], d[MAX_PATH]; g18_path(s, MAX_PATH, "mv_ms_s.txt"); g18_path(d, MAX_PATH, "mv_ms_d.txt"); g18_rm(s); g18_rm(d);
    SET_ERR(0);
    CHECK(!U(MoveFileA)(s, d));
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    CHECK(!g18_exists(d));
}
TEST(gap18_movea_missing_source_dir) {
    REQUIRE(U(MoveFileA));
    char s[MAX_PATH], d[MAX_PATH]; g18_path(s, MAX_PATH, "mv_nsd_zz\\s.txt"); g18_path(d, MAX_PATH, "mv_nsd_d.txt");
    SET_ERR(0);
    CHECK(!U(MoveFileA)(s, d));
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(gap18_movea_missing_dest_dir) {
    REQUIRE(U(MoveFileA));
    char s[MAX_PATH], d[MAX_PATH]; mk("mv_ndd_s.txt", s, "x"); g18_path(d, MAX_PATH, "mv_ndd_zz\\d.txt");
    SET_ERR(0);
    CHECK(!U(MoveFileA)(s, d));
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    CHECK(g18_exists(s));
    g18_rm(s);
}
TEST(gap18_movea_into_subdir) {
    REQUIRE(U(MoveFileA));
    char s[MAX_PATH], sub[MAX_PATH], d[MAX_PATH]; mk("mv_sd_s.txt", s, "sub"); g18_path(sub, MAX_PATH, "mv_sd_dir"); CreateDirectoryA(sub, NULL);
    snprintf(d, sizeof d, "%s\\moved.txt", sub);
    CHECK(U(MoveFileA)(s, d));
    CHECK(g18_content_is(d, "sub")); CHECK(!g18_exists(s));
    g18_rm(d); g18_rmdir(sub);
}
TEST(gap18_movea_rename_directory) {
    REQUIRE(U(MoveFileA));
    char a[MAX_PATH], b[MAX_PATH], f[MAX_PATH + 16];
    g18_path(a, MAX_PATH, "mv_rd_a"); g18_path(b, MAX_PATH, "mv_rd_b"); g18_rmdir(b);
    CreateDirectoryA(a, NULL);
    snprintf(f, sizeof f, "%s\\in.txt", a); g18_put(f, "in", 2);
    CHECK(U(MoveFileA)(a, b));
    CHECK(!g18_exists(a));
    snprintf(f, sizeof f, "%s\\in.txt", b);
    CHECK(g18_content_is(f, "in"));
    g18_rm(f); g18_rmdir(b);
}
TEST(gap18_movea_dir_to_existing_dir) {
    REQUIRE(U(MoveFileA));
    char a[MAX_PATH], b[MAX_PATH]; g18_path(a, MAX_PATH, "mv_dd_a"); g18_path(b, MAX_PATH, "mv_dd_b");
    CreateDirectoryA(a, NULL); CreateDirectoryA(b, NULL);
    SET_ERR(0);
    CHECK(!U(MoveFileA)(a, b));
    EXPECT_ERR(ERROR_ALREADY_EXISTS);
    g18_rmdir(a); g18_rmdir(b);
}
TEST(gap18_movea_file_onto_directory) {
    REQUIRE(U(MoveFileA));
    char s[MAX_PATH], d[MAX_PATH]; mk("mv_fd_s.txt", s, "x"); g18_path(d, MAX_PATH, "mv_fd_dir"); CreateDirectoryA(d, NULL);
    SET_ERR(0);
    CHECK(!U(MoveFileA)(s, d));
    EXPECT_ERR(ERROR_ALREADY_EXISTS);
    g18_rm(s); g18_rmdir(d);
}
TEST(gap18_movea_same_name) {
    REQUIRE(U(MoveFileA));
    char s[MAX_PATH]; mk("mv_self.txt", s, "self");
    SET_ERR(0);
    BOOL r = U(MoveFileA)(s, s);
    t_note("MoveFileA(x, x) = %d err %lu", r, GetLastError());
    CHECK(g18_content_is(s, "self"));
    g18_rm(s);
}
TEST(gap18_movea_case_only_rename) {
    REQUIRE(U(MoveFileA));
    char s[MAX_PATH], d[MAX_PATH]; mk("mv_case_a.txt", s, "case"); g18_path(d, MAX_PATH, "MV_CASE_A.TXT");
    SET_ERR(0);
    BOOL r = U(MoveFileA)(s, d);
    t_note("case-only rename = %d err %lu", r, GetLastError());
    CHECK(g18_content_is(s, "case"));
    g18_rm(s); g18_rm(d);
}
TEST(gap18_movea_open_without_share_delete) {
    REQUIRE(U(MoveFileA));
    char s[MAX_PATH], d[MAX_PATH]; mk("mv_ns_s.txt", s, "x"); g18_path(d, MAX_PATH, "mv_ns_d.txt"); g18_rm(d);
    HANDLE h = CreateFileA(s, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    SET_ERR(0);
    CHECK(!U(MoveFileA)(s, d));
    EXPECT_ERR(ERROR_SHARING_VIOLATION);
    CHECK(g18_exists(s));
    CloseHandle(h);
    CHECK(U(MoveFileA)(s, d));
    g18_rm(d);
}
TEST(gap18_movea_open_with_share_delete) {
    REQUIRE(U(MoveFileA));
    char s[MAX_PATH], d[MAX_PATH]; mk("mv_sh_s.txt", s, "shared"); g18_path(d, MAX_PATH, "mv_sh_d.txt"); g18_rm(d);
    HANDLE h = CreateFileA(s, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    SET_ERR(0);
    CHECK(U(MoveFileA)(s, d));
    char b[16]; DWORD n = 0;
    CHECK(ReadFile(h, b, sizeof b, &n, NULL));   /* handle still reads the moved file */
    EQ(n, 6);
    CloseHandle(h);
    CHECK(g18_content_is(d, "shared"));
    g18_rm(d);
}
TEST(gap18_movea_wildcards) {
    REQUIRE(U(MoveFileA));
    char s[MAX_PATH], d[MAX_PATH]; mk("mv_wc_a.txt", s, "x"); g18_path(d, MAX_PATH, "mv_wc_*.txt");
    SET_ERR(0);
    CHECK(!U(MoveFileA)(s, d));
    EXPECT_ERR(ERROR_INVALID_NAME);
    CHECK(g18_exists(s));
    g18_rm(s);
    g18_path(s, MAX_PATH, "mv_wc_*.txt"); g18_path(d, MAX_PATH, "mv_wc_z.txt");
    SET_ERR(0);
    CHECK(!U(MoveFileA)(s, d));
    EXPECT_ERR(ERROR_INVALID_NAME);
}
TEST(gap18_movea_hardlink_names_survive) {
    REQUIRE(U(MoveFileA));
    char s[MAX_PATH], l[MAX_PATH], d[MAX_PATH]; mk("mv_hl_s.txt", s, "hl"); g18_path(l, MAX_PATH, "mv_hl_link.txt"); g18_path(d, MAX_PATH, "mv_hl_d.txt");
    g18_rm(l); g18_rm(d);
    if (!CreateHardLinkA(l, s, NULL)) { t_skip("no hardlinks"); g18_rm(s); return; }
    CHECK(U(MoveFileA)(s, d));
    CHECK(g18_content_is(l, "hl")); CHECK(g18_content_is(d, "hl"));
    g18_rm(l); g18_rm(d);
}
TEST(gap18_movea_ads_follows) {
    REQUIRE(U(MoveFileA));
    char s[MAX_PATH], d[MAX_PATH], sa[MAX_PATH + 8], da[MAX_PATH + 8]; mk("mv_ads_s.txt", s, "m"); g18_path(d, MAX_PATH, "mv_ads_d.txt"); g18_rm(d);
    snprintf(sa, sizeof sa, "%s:alt", s);
    HANDLE h = CreateFileA(sa, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_skip("no ADS"); g18_rm(s); return; }
    DWORD w; WriteFile(h, "alt", 3, &w, NULL); CloseHandle(h);
    CHECK(U(MoveFileA)(s, d));
    snprintf(da, sizeof da, "%s:alt", d);
    CHECK(g18_content_is(da, "alt"));
    g18_rm(d);
}
TEST(gap18_movea_k32_forward) {
    REQUIRE(UK(MoveFileA)); REQUIRE(UK(MoveFileExA)); REQUIRE(UK(MoveFileWithProgressA));
    char s[MAX_PATH], d[MAX_PATH]; mk("mv_kf_s.txt", s, "kf"); g18_path(d, MAX_PATH, "mv_kf_d.txt"); g18_rm(d);
    CHECK(UK(MoveFileA)(s, d)); CHECK(g18_exists(d));
    CHECK(UK(MoveFileExA)(d, s, 0)); CHECK(g18_exists(s));
    CHECK(UK(MoveFileWithProgressA)(s, d, NULL, NULL, 0)); CHECK(g18_exists(d));
    g18_rm(d);
}

/* ---------------- MoveFileExA ---------------- */
TEST(gap18_moveexa_flags0_dest_exists) {
    REQUIRE(U(MoveFileExA));
    char s[MAX_PATH], d[MAX_PATH]; mk("me_f0_s.txt", s, "src"); mk("me_f0_d.txt", d, "dst");
    SET_ERR(0);
    CHECK(!U(MoveFileExA)(s, d, 0));
    EXPECT_ERR(ERROR_ALREADY_EXISTS);
    g18_rm(s); g18_rm(d);
}
TEST(gap18_moveexa_replace_existing) {
    REQUIRE(U(MoveFileExA));
    char s[MAX_PATH], d[MAX_PATH]; mk("me_re_s.txt", s, "src"); mk("me_re_d.txt", d, "dst-longer");
    SET_ERR(0);
    CHECK(U(MoveFileExA)(s, d, MOVEFILE_REPLACE_EXISTING));
    CHECK(!g18_exists(s));
    CHECK(g18_content_is(d, "src"));
    g18_rm(d);
}
TEST(gap18_moveexa_replace_missing_dest_ok) {
    REQUIRE(U(MoveFileExA));
    char s[MAX_PATH], d[MAX_PATH]; mk("me_rm_s.txt", s, "src"); g18_path(d, MAX_PATH, "me_rm_d.txt"); g18_rm(d);
    CHECK(U(MoveFileExA)(s, d, MOVEFILE_REPLACE_EXISTING));
    CHECK(g18_content_is(d, "src"));
    g18_rm(d);
}
TEST(gap18_moveexa_replace_readonly_dest) {
    REQUIRE(U(MoveFileExA));
    char s[MAX_PATH], d[MAX_PATH]; mk("me_ro_s.txt", s, "src"); mk("me_ro_d.txt", d, "dst");
    SetFileAttributesA(d, FILE_ATTRIBUTE_READONLY);
    SET_ERR(0);
    CHECK(!U(MoveFileExA)(s, d, MOVEFILE_REPLACE_EXISTING));
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CHECK(g18_exists(s));
    CHECK(g18_content_is(d, "dst"));
    g18_rm(s); g18_rm(d);
}
TEST(gap18_moveexa_replace_open_dest) {
    REQUIRE(U(MoveFileExA));
    char s[MAX_PATH], d[MAX_PATH]; mk("me_od_s.txt", s, "src"); mk("me_od_d.txt", d, "dst");
    HANDLE h = CreateFileA(d, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    SET_ERR(0);
    BOOL r = U(MoveFileExA)(s, d, MOVEFILE_REPLACE_EXISTING);
    DWORD e = GetLastError();
    CloseHandle(h);
    t_note("replace open dest err %lu", e);
    g18_rm(s); g18_rm(d);
}
TEST(gap18_moveexa_replace_directory_target) {
    REQUIRE(U(MoveFileExA));
    char s[MAX_PATH], d[MAX_PATH]; mk("me_rd_s.txt", s, "src"); g18_path(d, MAX_PATH, "me_rd_dir"); CreateDirectoryA(d, NULL);
    SET_ERR(0);
    CHECK(!U(MoveFileExA)(s, d, MOVEFILE_REPLACE_EXISTING));
    t_note("replace dir with file err %lu", GetLastError());
    CHECK(g18_exists(s));
    CHECK(GetFileAttributesA(d) & FILE_ATTRIBUTE_DIRECTORY);
    g18_rm(s); g18_rmdir(d);
}
TEST(gap18_moveexa_replace_dir_with_dir) {
    REQUIRE(U(MoveFileExA));
    char a[MAX_PATH], b[MAX_PATH]; g18_path(a, MAX_PATH, "me_dd_a"); g18_path(b, MAX_PATH, "me_dd_b");
    CreateDirectoryA(a, NULL); CreateDirectoryA(b, NULL);
    SET_ERR(0);
    CHECK(!U(MoveFileExA)(a, b, MOVEFILE_REPLACE_EXISTING));   /* directories cannot be replaced */
    t_note("replace dir with dir err %lu", GetLastError());
    CHECK(g18_exists(a)); CHECK(g18_exists(b));
    g18_rmdir(a); g18_rmdir(b);
}
TEST(gap18_moveexa_copy_allowed_same_volume) {
    REQUIRE(U(MoveFileExA));
    char s[MAX_PATH], d[MAX_PATH]; mk("me_ca_s.txt", s, "ca"); g18_path(d, MAX_PATH, "me_ca_d.txt"); g18_rm(d);
    CHECK(U(MoveFileExA)(s, d, MOVEFILE_COPY_ALLOWED));
    CHECK(!g18_exists(s)); CHECK(g18_content_is(d, "ca"));
    g18_rm(d);
}
TEST(gap18_moveexa_write_through_and_trackable) {
    REQUIRE(U(MoveFileExA));
    char s[MAX_PATH], d[MAX_PATH]; mk("me_wt_s.txt", s, "wt"); g18_path(d, MAX_PATH, "me_wt_d.txt"); g18_rm(d);
    CHECK(U(MoveFileExA)(s, d, MOVEFILE_WRITE_THROUGH | MOVEFILE_COPY_ALLOWED));
    CHECK(g18_content_is(d, "wt"));
    CHECK(U(MoveFileExA)(d, s, MOVEFILE_FAIL_IF_NOT_TRACKABLE));
    CHECK(g18_content_is(s, "wt"));
    g18_rm(s);
}
TEST(gap18_moveexa_delay_with_hardlink_invalid) {
    REQUIRE(U(MoveFileExA));
    /* DELAY_UNTIL_REBOOT | CREATE_HARDLINK is rejected (ERROR_INVALID_PARAMETER) before anything is scheduled or changed. */
    char s[MAX_PATH], d[MAX_PATH]; mk("me_dh_s.txt", s, "x"); g18_path(d, MAX_PATH, "me_dh_d.txt"); g18_rm(d);
    SET_ERR(0);
    CHECK(!U(MoveFileExA)(s, d, MOVEFILE_DELAY_UNTIL_REBOOT | 0x10));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CHECK(g18_exists(s)); CHECK(!g18_exists(d));
    g18_rm(s);
}
TEST(gap18_moveexa_null_newname_no_delay_flag) {
    REQUIRE(U(MoveFileExA));
    /* NULL new name is only meaningful with DELAY_UNTIL_REBOOT; without it the destination path
     * conversion fails and the source is left untouched. */
    char s[MAX_PATH]; mk("me_nn_s.txt", s, "keep");
    SET_ERR(0);
    CHECK(!U(MoveFileExA)(s, NULL, 0));
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    CHECK(g18_content_is(s, "keep"));
    SET_ERR(0);
    CHECK(!U(MoveFileExA)(s, NULL, MOVEFILE_REPLACE_EXISTING));
    CHECK(g18_content_is(s, "keep"));
    g18_rm(s);
}
TEST(gap18_moveexa_unknown_flag_bits) {
    REQUIRE(U(MoveFileExA));
    char s[MAX_PATH], d[MAX_PATH]; mk("me_uf_s.txt", s, "x"); g18_path(d, MAX_PATH, "me_uf_d.txt"); g18_rm(d);
    static const DWORD bits[] = { 0x40, 0x80, 0x100, 0x10000, 0x80000000 };
    for (unsigned i = 0; i < sizeof bits / sizeof bits[0]; i++) {
        g18_rm(d); SET_ERR(0);
        BOOL r = U(MoveFileExA)(s, d, bits[i]);
        t_note("flags 0x%lx -> %d err %lu", bits[i], r, GetLastError());
        if (r) { char t[MAX_PATH]; strcpy(t, s); U(MoveFileExA)(d, t, 0); }
    }
    g18_rm(s); g18_rm(d);
}
TEST(gap18_moveexa_missing_source_with_replace) {
    REQUIRE(U(MoveFileExA));
    char s[MAX_PATH], d[MAX_PATH]; g18_path(s, MAX_PATH, "me_mr_s.txt"); mk("me_mr_d.txt", d, "keep"); g18_rm(s);
    SET_ERR(0);
    CHECK(!U(MoveFileExA)(s, d, MOVEFILE_REPLACE_EXISTING));
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    CHECK(g18_content_is(d, "keep"));
    g18_rm(d);
}
TEST(gap18_moveexa_long_prefix) {
    REQUIRE(U(MoveFileExA));
    char s[MAX_PATH + 8], d[MAX_PATH + 8];
    strcpy(s, "\\\\?\\"); g18_path(s + 4, MAX_PATH, "me_lp_s.txt"); g18_put(s, "lp", 2);
    strcpy(d, "\\\\?\\"); g18_path(d + 4, MAX_PATH, "me_lp_d.txt"); g18_rm(d);
    CHECK(U(MoveFileExA)(s, d, 0));
    CHECK(g18_content_is(d, "lp"));
    g18_rm(d);
}
TEST(gap18_moveexa_parity_w) {
    REQUIRE(U(MoveFileExA)); REQUIRE(U(MoveFileExW));
    char a[MAX_PATH], b[MAX_PATH], c[MAX_PATH]; mk("me_pw_a.txt", a, "pw"); mk("me_pw_b.txt", b, "other"); g18_path(c, MAX_PATH, "me_pw_c.txt"); g18_rm(c);
    WCHAR wa[MAX_PATH], wb[MAX_PATH], wc[MAX_PATH]; g18_widen(wa, MAX_PATH, a); g18_widen(wb, MAX_PATH, b); g18_widen(wc, MAX_PATH, c);
    SET_ERR(0); CHECK(!U(MoveFileExA)(a, b, 0)); DWORD e1 = GetLastError();
    SET_ERR(0); CHECK(!U(MoveFileExW)(wa, wb, 0)); DWORD e2 = GetLastError();
    EQ(e1, e2);
    CHECK(U(MoveFileExA)(a, c, 0)); CHECK(U(MoveFileExW)(wc, wa, 0)); CHECK(g18_content_is(a, "pw"));
    g18_rm(a); g18_rm(b); g18_rm(c);
}
TEST(gap18_moveexa_repeat_ping_pong) {
    REQUIRE(U(MoveFileExA));
    char a[MAX_PATH], b[MAX_PATH]; mk("me_pp_a.txt", a, "ping"); g18_path(b, MAX_PATH, "me_pp_b.txt"); g18_rm(b);
    for (int i = 0; i < 200; i++) {
        CHECKF(U(MoveFileExA)(i & 1 ? b : a, i & 1 ? a : b, 0), "iter %d err %lu", i, GetLastError());
    }
    CHECK(g18_content_is(a, "ping"));
    g18_rm(a); g18_rm(b);
}

/* ---------------- MoveFileWithProgressA ---------------- */
typedef struct { int calls; LPVOID want; int data_ok; int ret; } mvp_t;
static DWORD CALLBACK mvp_cb(LARGE_INTEGER t, LARGE_INTEGER x, LARGE_INTEGER ss, LARGE_INTEGER sx, DWORD sn, DWORD reason, HANDLE hs, HANDLE hd, LPVOID data) {
    mvp_t *p = (mvp_t *)data; p->calls++; if (data == p->want) p->data_ok++; return p->ret;
}
TEST(gap18_movewpa_basic_no_callback) {
    REQUIRE(U(MoveFileWithProgressA));
    char s[MAX_PATH], d[MAX_PATH]; mk("mp_b_s.txt", s, "p"); g18_path(d, MAX_PATH, "mp_b_d.txt"); g18_rm(d);
    CHECK(U(MoveFileWithProgressA)(s, d, NULL, NULL, 0));
    CHECK(g18_content_is(d, "p")); CHECK(!g18_exists(s));
    g18_rm(d);
}
TEST(gap18_movewpa_callback_same_volume) {
    REQUIRE(U(MoveFileWithProgressA));
    char s[MAX_PATH], d[MAX_PATH]; mk("mp_cb_s.txt", s, "p"); g18_path(d, MAX_PATH, "mp_cb_d.txt"); g18_rm(d);
    mvp_t p; memset(&p, 0, sizeof p); p.want = &p; p.ret = PROGRESS_CONTINUE;
    CHECK(U(MoveFileWithProgressA)(s, d, mvp_cb, &p, MOVEFILE_COPY_ALLOWED));
    CHECK(g18_content_is(d, "p"));
    t_note("progress calls for same-volume move: %d", p.calls);
    if (p.calls) EQ(p.data_ok, p.calls);
    g18_rm(d);
}
TEST(gap18_movewpa_dest_exists) {
    REQUIRE(U(MoveFileWithProgressA));
    char s[MAX_PATH], d[MAX_PATH]; mk("mp_de_s.txt", s, "s"); mk("mp_de_d.txt", d, "d");
    SET_ERR(0);
    CHECK(!U(MoveFileWithProgressA)(s, d, NULL, NULL, 0));
    EXPECT_ERR(ERROR_ALREADY_EXISTS);
    CHECK(U(MoveFileWithProgressA)(s, d, NULL, NULL, MOVEFILE_REPLACE_EXISTING));
    CHECK(g18_content_is(d, "s"));
    g18_rm(d);
}
TEST(gap18_movewpa_missing_source) {
    REQUIRE(U(MoveFileWithProgressA));
    char s[MAX_PATH], d[MAX_PATH]; g18_path(s, MAX_PATH, "mp_ms_s.txt"); g18_path(d, MAX_PATH, "mp_ms_d.txt"); g18_rm(s);
    mvp_t p; memset(&p, 0, sizeof p); p.want = &p;
    SET_ERR(0);
    CHECK(!U(MoveFileWithProgressA)(s, d, mvp_cb, &p, MOVEFILE_COPY_ALLOWED));
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    EQ(p.calls, 0);
}
TEST(gap18_movewpa_delay_hardlink_invalid) {
    REQUIRE(U(MoveFileWithProgressA));
    char s[MAX_PATH], d[MAX_PATH]; mk("mp_dh_s.txt", s, "x"); g18_path(d, MAX_PATH, "mp_dh_d.txt"); g18_rm(d);
    SET_ERR(0);
    CHECK(!U(MoveFileWithProgressA)(s, d, NULL, NULL, MOVEFILE_DELAY_UNTIL_REBOOT | 0x10));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CHECK(g18_exists(s));
    g18_rm(s);
}
TEST(gap18_movewpa_null_newname) {
    REQUIRE(U(MoveFileWithProgressA));
    char s[MAX_PATH]; mk("mp_nn_s.txt", s, "keep");
    SET_ERR(0);
    CHECK(!U(MoveFileWithProgressA)(s, NULL, NULL, NULL, 0));
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    CHECK(g18_content_is(s, "keep"));
    g18_rm(s);
}
TEST(gap18_movewpa_directory_with_files) {
    REQUIRE(U(MoveFileWithProgressA));
    char a[MAX_PATH], b[MAX_PATH], f[MAX_PATH + 16];
    g18_path(a, MAX_PATH, "mp_dir_a"); g18_path(b, MAX_PATH, "mp_dir_b"); g18_rmdir(b);
    CreateDirectoryA(a, NULL);
    for (int i = 0; i < 5; i++) { snprintf(f, sizeof f, "%s\\f%d", a, i); g18_put(f, "z", 1); }
    CHECK(U(MoveFileWithProgressA)(a, b, NULL, NULL, MOVEFILE_COPY_ALLOWED));
    for (int i = 0; i < 5; i++) { snprintf(f, sizeof f, "%s\\f%d", b, i); CHECK(g18_exists(f)); g18_rm(f); }
    CHECK(!g18_exists(a));
    g18_rmdir(b);
}
TEST(gap18_movewpa_wide_parity_error) {
    REQUIRE(U(MoveFileWithProgressA)); REQUIRE(U(MoveFileWithProgressW));
    char s[MAX_PATH], d[MAX_PATH]; g18_path(s, MAX_PATH, "mp_pw_nope\\s"); g18_path(d, MAX_PATH, "mp_pw_d");
    WCHAR ws[MAX_PATH], wd[MAX_PATH]; g18_widen(ws, MAX_PATH, s); g18_widen(wd, MAX_PATH, d);
    SET_ERR(0); CHECK(!U(MoveFileWithProgressA)(s, d, NULL, NULL, 0)); DWORD e1 = GetLastError();
    SET_ERR(0); CHECK(!U(MoveFileWithProgressW)(ws, wd, NULL, NULL, 0)); DWORD e2 = GetLastError();
    EQ(e1, e2);
    EQ(e1, ERROR_PATH_NOT_FOUND);
}
