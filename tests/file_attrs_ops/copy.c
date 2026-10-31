/* CopyFileA/W, CopyFileExA/W, CopyFile2 */
#include "fa.h"

static BOOL cpA(const char *s, const char *d, BOOL fail) { return U(CopyFileA)(s, d, fail); }
static BOOL cpW(const char *s, const char *d, BOOL fail) { WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_widen(a, s); fa_widen(b, d); return U(CopyFileW)(a, b, fail); }
static void pp(char *p, const char *leaf) { fa_path(p, leaf); }
static void mkpat(const char *leaf, DWORD size) {
    char p[MAX_PATH * 2]; fa_path(p, leaf);
    BYTE *b = malloc(size + 1);
    for (DWORD i = 0; i < size; i++) b[i] = (BYTE)(i * 7 + (i >> 8));
    fa_mkfile(p, b, size); free(b);
}
static int cmpfiles(const char *a, const char *b) {
    HANDLE ha = fa_open(a, GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0), hb = fa_open(b, GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0);
    int same = 0;
    if (ha != INVALID_HANDLE_VALUE && hb != INVALID_HANDLE_VALUE) {
        DWORD sa = GetFileSize(ha, NULL), sb = GetFileSize(hb, NULL);
        same = sa == sb;
        BYTE x[4096], y[4096];
        while (same) { DWORD na = 0, nb = 0; ReadFile(ha, x, sizeof x, &na, NULL); ReadFile(hb, y, sizeof y, &nb, NULL); if (na != nb || memcmp(x, y, na)) same = 0; if (!na) break; }
    }
    if (ha != INVALID_HANDLE_VALUE) CloseHandle(ha);
    if (hb != INVALID_HANDLE_VALUE) CloseHandle(hb);
    return same;
}

TEST(fcopy_basic_a_w) {
    REQUIRE(U(CopyFileA)); REQUIRE(U(CopyFileW));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("cp_s.txt", "hello copy"); pp(s, "cp_s.txt"); pp(d, "cp_d.txt");
    CHECKF(cpA(s, d, TRUE), "err %lu", GetLastError());
    CHECK(fa_content_is(d, "hello copy"));
    fa_rm(d);
    CHECKF(cpW(s, d, TRUE), "err %lu", GetLastError());
    CHECK(fa_content_is(d, "hello copy"));
    CHECK(fa_content_is(s, "hello copy"));
    fa_rm(s); fa_rm(d);
}
TEST(fcopy_empty_file) {
    REQUIRE(U(CopyFileW));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("cp_e.txt", ""); pp(s, "cp_e.txt"); pp(d, "cp_e2.txt");
    CHECK(cpW(s, d, TRUE));
    HANDLE h = fa_open(d, GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0);
    CHECK(h != INVALID_HANDLE_VALUE); EXPECT_EQ(GetFileSize(h, NULL), 0); CloseHandle(h);
    fa_rm(s); fa_rm(d);
}
TEST(fcopy_sizes_content_preserved) {
    REQUIRE(U(CopyFileA));
    static const DWORD sz[] = { 1, 511, 512, 4095, 4096, 4097, 65535, 65536, 65537, 300000 };
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; pp(s, "cp_sz_s.bin"); pp(d, "cp_sz_d.bin");
    for (unsigned i = 0; i < sizeof sz / sizeof *sz; i++) {
        mkpat("cp_sz_s.bin", sz[i]);
        CHECKF(cpA(s, d, FALSE), "size %lu err %lu", sz[i], GetLastError());
        CHECKF(cmpfiles(s, d), "size %lu mismatch", sz[i]);
    }
    fa_rm(s); fa_rm(d);
}
TEST(fcopy_overwrite_semantics) {
    REQUIRE(U(CopyFileA));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("cp_o_s.txt", "new"); fa_mkfile_s("cp_o_d.txt", "old content longer"); pp(s, "cp_o_s.txt"); pp(d, "cp_o_d.txt");
    SetLastError(0); EXPECT_EQ(cpA(s, d, TRUE), FALSE); EXPECT_ERR(ERROR_FILE_EXISTS);
    CHECK(fa_content_is(d, "old content longer"));
    CHECK(cpA(s, d, FALSE));
    CHECK(fa_content_is(d, "new"));
    fa_rm(s); fa_rm(d);
}
TEST(fcopy_overwrite_semantics_w) {
    REQUIRE(U(CopyFileW));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("cp_ow_s.txt", "new"); fa_mkfile_s("cp_ow_d.txt", "old"); pp(s, "cp_ow_s.txt"); pp(d, "cp_ow_d.txt");
    SetLastError(0); EXPECT_EQ(cpW(s, d, TRUE), FALSE); EXPECT_ERR(ERROR_FILE_EXISTS);
    CHECK(cpW(s, d, FALSE));
    CHECK(fa_content_is(d, "new"));
    fa_rm(s); fa_rm(d);
}
TEST(fcopy_missing_source) {
    REQUIRE(U(CopyFileA)); REQUIRE(U(CopyFileW));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; pp(s, "cp_nosrc.txt"); pp(d, "cp_nosrc_d.txt");
    SetLastError(0); EXPECT_EQ(cpA(s, d, FALSE), FALSE); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    SetLastError(0); EXPECT_EQ(cpW(s, d, FALSE), FALSE); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    CHECK(!fa_exists(d));
    pp(s, "cp_nosrcdir\\x.txt");
    SetLastError(0); EXPECT_EQ(cpA(s, d, FALSE), FALSE); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    CHECK(!fa_exists(d));
}
TEST(fcopy_missing_dest_directory) {
    REQUIRE(U(CopyFileA)); REQUIRE(U(CopyFileW));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("cp_nd_s.txt", "x"); pp(s, "cp_nd_s.txt"); pp(d, "cp_nodir\\d.txt");
    SetLastError(0); EXPECT_EQ(cpA(s, d, FALSE), FALSE); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    SetLastError(0); EXPECT_EQ(cpW(s, d, FALSE), FALSE); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    fa_rm(s);
}
TEST(fcopy_source_is_directory) {
    REQUIRE(U(CopyFileA));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkdir("cp_srcdir"); pp(s, "cp_srcdir"); pp(d, "cp_srcdir_d");
    SetLastError(0); EXPECT_EQ(cpA(s, d, FALSE), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CHECK(!fa_exists(d));
    fa_rm(s);
}
TEST(fcopy_dest_is_directory) {
    REQUIRE(U(CopyFileA));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("cp_dd_s.txt", "x"); fa_mkdir("cp_dd_dir"); pp(s, "cp_dd_s.txt"); pp(d, "cp_dd_dir");
    SetLastError(0); EXPECT_EQ(cpA(s, d, FALSE), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    SetLastError(0); EXPECT_EQ(cpA(s, d, TRUE), FALSE);
    DWORD e = GetLastError(); CHECKF(e == ERROR_ACCESS_DENIED || e == ERROR_FILE_EXISTS, "err %lu", e);
    fa_rm(s); fa_rm(d);
}
TEST(fcopy_onto_itself_sharing_violation) {
    REQUIRE(U(CopyFileA)); REQUIRE(U(CopyFileW));
    char s[MAX_PATH * 2]; fa_mkfile_s("cp_self.txt", "keep me"); pp(s, "cp_self.txt");
    SetLastError(0); EXPECT_EQ(cpA(s, s, FALSE), FALSE); EXPECT_ERR(ERROR_SHARING_VIOLATION);
    SetLastError(0); EXPECT_EQ(cpW(s, s, FALSE), FALSE); EXPECT_ERR(ERROR_SHARING_VIOLATION);
    CHECK(fa_content_is(s, "keep me"));
    char u[MAX_PATH * 2]; pp(u, "CP_SELF.TXT");
    SetLastError(0); EXPECT_EQ(cpA(s, u, FALSE), FALSE); EXPECT_ERR(ERROR_SHARING_VIOLATION);
    CHECK(fa_content_is(s, "keep me"));
    fa_rm(s);
}
TEST(fcopy_onto_itself_fail_if_exists) {
    REQUIRE(U(CopyFileA));
    char s[MAX_PATH * 2]; fa_mkfile_s("cp_self2.txt", "keep"); pp(s, "cp_self2.txt");
    SetLastError(0); EXPECT_EQ(cpA(s, s, TRUE), FALSE);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_FILE_EXISTS || e == ERROR_SHARING_VIOLATION, "err %lu", e);
    t_note("copy onto itself with fail_if_exists err=%lu", e);
    CHECK(fa_content_is(s, "keep"));
    fa_rm(s);
}
TEST(fcopy_overwrite_readonly_dest_denied) {
    REQUIRE(U(CopyFileA));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("cp_ro_s.txt", "new"); fa_mkfile_s("cp_ro_d.txt", "old"); pp(s, "cp_ro_s.txt"); pp(d, "cp_ro_d.txt");
    SetFileAttributesA(d, FILE_ATTRIBUTE_READONLY);
    SetLastError(0); EXPECT_EQ(cpA(s, d, FALSE), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CHECK(fa_content_is(d, "old"));
    fa_rm(s); fa_rm(d);
}
TEST(fcopy_overwrite_readonly_dest_with_fail_flag) {
    REQUIRE(U(CopyFileA));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("cp_ro2_s.txt", "new"); fa_mkfile_s("cp_ro2_d.txt", "old"); pp(s, "cp_ro2_s.txt"); pp(d, "cp_ro2_d.txt");
    SetFileAttributesA(d, FILE_ATTRIBUTE_READONLY);
    SetLastError(0); EXPECT_EQ(cpA(s, d, TRUE), FALSE); EXPECT_ERR(ERROR_FILE_EXISTS);
    fa_rm(s); fa_rm(d);
}
TEST(fcopy_readonly_source_copied_with_attribute) {
    REQUIRE(U(CopyFileA));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("cp_rs.txt", "ro src"); pp(s, "cp_rs.txt"); pp(d, "cp_rs_d.txt");
    SetFileAttributesA(s, FILE_ATTRIBUTE_READONLY);
    CHECKF(cpA(s, d, TRUE), "err %lu", GetLastError());
    CHECK(GetFileAttributesA(d) & FILE_ATTRIBUTE_READONLY);
    fa_rm(s); fa_rm(d);
}
TEST(fcopy_attributes_copied) {
    REQUIRE(U(CopyFileW));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("cp_at.txt", "attrs"); pp(s, "cp_at.txt"); pp(d, "cp_at_d.txt");
    SetFileAttributesA(s, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
    CHECK(cpW(s, d, TRUE));
    DWORD a = GetFileAttributesA(d);
    CHECK((a & 6) == 6);
    SetFileAttributesA(d, FILE_ATTRIBUTE_NORMAL); fa_rm(s); fa_rm(d);
}
TEST(fcopy_overwrite_hidden_dest) {
    REQUIRE(U(CopyFileA));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("cp_hd_s.txt", "new"); fa_mkfile_s("cp_hd_d.txt", "old"); pp(s, "cp_hd_s.txt"); pp(d, "cp_hd_d.txt");
    SetFileAttributesA(d, FILE_ATTRIBUTE_HIDDEN);
    SetLastError(0);
    BOOL r = cpA(s, d, FALSE);
    t_note("overwrite hidden dest: r=%d err=%lu", r, r ? 0 : GetLastError());
    if (r) CHECK(fa_content_is(d, "new"));
    SetFileAttributesA(d, FILE_ATTRIBUTE_NORMAL); fa_rm(s); fa_rm(d);
}
TEST(fcopy_write_time_preserved) {
    REQUIRE(U(CopyFileA));
    if (!fa_is_ntfs() || t_is_wine()) { t_skip("needs real NTFS"); return; }
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("cp_wt.txt", "times"); pp(s, "cp_wt.txt"); pp(d, "cp_wt_d.txt");
    HANDLE h = fa_open(s, GENERIC_WRITE, FA_SHARE_ALL, OPEN_EXISTING, 0);
    FILETIME t = fa_mkft(FA_FT_2001 + 123456789ULL), a = fa_mkft(FA_FT_2001 + 5 * FA_SEC);
    SetFileTime(h, &t, &a, &t); CloseHandle(h);
    CHECK(cpA(s, d, TRUE));
    WIN32_FILE_ATTRIBUTE_DATA x, y; GetFileAttributesExA(s, GetFileExInfoStandard, &x); GetFileAttributesExA(d, GetFileExInfoStandard, &y);
    EXPECT_EQ(fa_ft64(y.ftLastWriteTime), fa_ft64(x.ftLastWriteTime));
    t_note("creation time src=%llx dst=%llx", fa_ft64(x.ftCreationTime), fa_ft64(y.ftCreationTime));
    t_note("access time src=%llx dst=%llx", fa_ft64(x.ftLastAccessTime), fa_ft64(y.ftLastAccessTime));
    fa_rm(s); fa_rm(d);
}
TEST(fcopy_source_unchanged) {
    REQUIRE(U(CopyFileA));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("cp_su.txt", "stay"); pp(s, "cp_su.txt"); pp(d, "cp_su_d.txt");
    WIN32_FILE_ATTRIBUTE_DATA x, y; GetFileAttributesExA(s, GetFileExInfoStandard, &x);
    CHECK(cpA(s, d, TRUE));
    GetFileAttributesExA(s, GetFileExInfoStandard, &y);
    EXPECT_EQ(fa_ft64(x.ftLastWriteTime), fa_ft64(y.ftLastWriteTime));
    EXPECT_EQ(x.nFileSizeLow, y.nFileSizeLow); EXPECT_EQ(x.dwFileAttributes, y.dwFileAttributes);
    fa_rm(s); fa_rm(d);
}
TEST(fcopy_alternate_streams_copied) {
    REQUIRE(U(CopyFileA));
    FA_REQUIRE_STREAMS();
    char s[MAX_PATH * 2], d[MAX_PATH * 2], q[MAX_PATH * 2], r[MAX_PATH * 2];
    fa_mkfile_s("cp_ads.txt", "main"); pp(s, "cp_ads.txt"); pp(d, "cp_ads_d.txt");
    snprintf(q, sizeof q, "%s:extra", s); CHECK(fa_mkfile(q, "stream data", 11));
    CHECK(cpA(s, d, TRUE));
    snprintf(r, sizeof r, "%s:extra", d);
    CHECK(fa_content_is(r, "stream data"));
    CHECK(fa_content_is(d, "main"));
    fa_rm(s); fa_rm(d);
}
TEST(fcopy_source_open_exclusive_denied) {
    REQUIRE(U(CopyFileA));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("cp_ex.txt", "locked"); pp(s, "cp_ex.txt"); pp(d, "cp_ex_d.txt");
    HANDLE h = fa_open(s, GENERIC_READ | GENERIC_WRITE, 0, OPEN_EXISTING, 0);
    SetLastError(0); EXPECT_EQ(cpA(s, d, FALSE), FALSE); EXPECT_ERR(ERROR_SHARING_VIOLATION);
    CHECK(!fa_exists(d));
    CloseHandle(h);
    CHECK(cpA(s, d, FALSE));
    fa_rm(s); fa_rm(d);
}
TEST(fcopy_source_open_share_read_ok) {
    REQUIRE(U(CopyFileA));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("cp_sr.txt", "readable"); pp(s, "cp_sr.txt"); pp(d, "cp_sr_d.txt");
    HANDLE h = fa_open(s, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0);
    CHECKF(cpA(s, d, FALSE), "err %lu", GetLastError());
    CloseHandle(h);
    fa_rm(s); fa_rm(d);
}
TEST(fcopy_source_open_for_write_share_write_denied_or_ok) {
    REQUIRE(U(CopyFileA));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("cp_sw.txt", "writing"); pp(s, "cp_sw.txt"); pp(d, "cp_sw_d.txt");
    HANDLE h = fa_open(s, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, 0);
    SetLastError(0);
    BOOL r = cpA(s, d, FALSE);
    t_note("copy of source open for write (share r+w): r=%d err=%lu", r, r ? 0 : GetLastError());
    CHECK(r);
    CloseHandle(h);
    fa_rm(s); fa_rm(d);
}
TEST(fcopy_source_open_share_write_only_denied) {
    REQUIRE(U(CopyFileA));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("cp_sw2.txt", "writing"); pp(s, "cp_sw2.txt"); pp(d, "cp_sw2_d.txt");
    HANDLE h = fa_open(s, GENERIC_WRITE, FILE_SHARE_WRITE, OPEN_EXISTING, 0);   /* does not allow others to read */
    SetLastError(0); EXPECT_EQ(cpA(s, d, FALSE), FALSE); EXPECT_ERR(ERROR_SHARING_VIOLATION);
    CloseHandle(h); fa_rm(s); fa_rm(d);
}
TEST(fcopy_dest_open_denied) {
    REQUIRE(U(CopyFileA));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("cp_do_s.txt", "new"); fa_mkfile_s("cp_do_d.txt", "old"); pp(s, "cp_do_s.txt"); pp(d, "cp_do_d.txt");
    HANDLE h = fa_open(d, GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0);
    SetLastError(0); BOOL r = cpA(s, d, FALSE);
    t_note("copy over file open for read with full share: r=%d err=%lu", r, r ? 0 : GetLastError());
    CloseHandle(h);
    h = fa_open(d, GENERIC_READ, 0, OPEN_EXISTING, 0);
    SetLastError(0); EXPECT_EQ(cpA(s, d, FALSE), FALSE); EXPECT_ERR(ERROR_SHARING_VIOLATION);
    CloseHandle(h); fa_rm(s); fa_rm(d);
}
TEST(fcopy_null_and_empty_names) {
    REQUIRE(U(CopyFileW)); REQUIRE(U(CopyFileA));
    char s[MAX_PATH * 2]; fa_mkfile_s("cp_nl.txt", "x"); pp(s, "cp_nl.txt");
    WCHAR ws[MAX_PATH * 2]; fa_widen(ws, s);
    SetLastError(0); EXPECT_EQ(U(CopyFileW)(ws, NULL, FALSE), FALSE);
    t_note("CopyFileW(src, NULL) err=%lu", GetLastError());
    SetLastError(0); EXPECT_EQ(U(CopyFileW)(NULL, ws, FALSE), FALSE);
    t_note("CopyFileW(NULL, dst) err=%lu", GetLastError());
    SetLastError(0); EXPECT_EQ(U(CopyFileW)(ws, L"", FALSE), FALSE);
    t_note("CopyFileW(src, \"\") err=%lu", GetLastError());
    SetLastError(0); EXPECT_EQ(U(CopyFileA)("", "x", FALSE), FALSE);
    t_note("CopyFileA(\"\", x) err=%lu", GetLastError());
    fa_rm(s);
}
TEST(fcopy_wildcard_names) {
    REQUIRE(U(CopyFileA));
    char s[MAX_PATH * 2], w[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("cp_wc.txt", "x"); pp(s, "cp_wc.txt"); pp(w, "cp_w*.txt"); pp(d, "cp_wc_d.txt");
    SetLastError(0); EXPECT_EQ(cpA(w, d, FALSE), FALSE);
    DWORD e = GetLastError(); CHECKF(e == ERROR_INVALID_NAME || e == ERROR_FILE_NOT_FOUND, "err %lu", e);
    t_note("wildcard source err=%lu", e);
    pp(w, "cp_d*.txt");
    SetLastError(0); EXPECT_EQ(cpA(s, w, FALSE), FALSE); EXPECT_ERR(ERROR_INVALID_NAME);
    fa_rm(s);
}
TEST(fcopy_trailing_dots_in_dest_stripped) {
    REQUIRE(U(CopyFileA));
    char s[MAX_PATH * 2], d[MAX_PATH * 2], e[MAX_PATH * 2]; fa_mkfile_s("cp_td.txt", "x"); pp(s, "cp_td.txt"); pp(d, "cp_td_d.txt..."); pp(e, "cp_td_d.txt");
    CHECK(cpA(s, d, TRUE));
    CHECK(fa_exists(e));
    fa_rm(s); fa_rm(e);
}
TEST(fcopy_long_prefixed_paths) {
    REQUIRE(U(CopyFileW));
    char s[MAX_PATH * 2], d[MAX_PATH * 2], qs[MAX_PATH * 2], qd[MAX_PATH * 2]; fa_mkfile_s("cp_lp.txt", "prefixed"); pp(s, "cp_lp.txt"); pp(d, "cp_lp_d.txt");
    snprintf(qs, sizeof qs, "\\\\?\\%s", s); snprintf(qd, sizeof qd, "\\\\?\\%s", d);
    CHECKF(cpW(qs, qd, TRUE), "err %lu", GetLastError());
    CHECK(fa_content_is(d, "prefixed"));
    fa_rm(s); fa_rm(d);
}
TEST(fcopy_unicode_name) {
    REQUIRE(U(CopyFileW));
    WCHAR p[MAX_PATH * 2], s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_wpath(p, "");
    wsprintfW(s, L"%lscp_\x00fc\x4e2d.txt", p); wsprintfW(d, L"%lscp_\x00fc\x4e2d_copy.txt", p);
    HANDLE h = CreateFileW(s, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_skip("cannot create unicode names"); return; }
    DWORD n; WriteFile(h, "uni", 3, &n, NULL); CloseHandle(h);
    CHECKF(U(CopyFileW)(s, d, TRUE), "err %lu", GetLastError());
    CHECK(GetFileAttributesW(d) != INVALID_FILE_ATTRIBUTES);
    DeleteFileW(s); DeleteFileW(d);
}
TEST(fcopy_to_stream_name) {
    REQUIRE(U(CopyFileA));
    FA_REQUIRE_STREAMS();
    char s[MAX_PATH * 2], d[MAX_PATH * 2], q[MAX_PATH * 2]; fa_mkfile_s("cp_ts_s.txt", "payload"); fa_mkfile_s("cp_ts_d.txt", "host"); pp(s, "cp_ts_s.txt"); pp(d, "cp_ts_d.txt");
    snprintf(q, sizeof q, "%s:copied", d);
    SetLastError(0); BOOL r = cpA(s, q, FALSE);
    t_note("copy to stream name: r=%d err=%lu", r, r ? 0 : GetLastError());
    if (r) CHECK(fa_content_is(q, "payload"));
    CHECK(fa_content_is(d, "host"));
    fa_rm(s); fa_rm(d);
}
TEST(fcopy_repeat_overwrite_loop) {
    REQUIRE(U(CopyFileA));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("cp_lp2.txt", "loop"); pp(s, "cp_lp2.txt"); pp(d, "cp_lp2_d.txt");
    int bad = 0;
    for (int i = 0; i < 200; i++) if (!cpA(s, d, FALSE)) bad++;
    EXPECT_EQ(bad, 0);
    CHECK(fa_content_is(d, "loop"));
    fa_rm(s); fa_rm(d);
}
TEST(fcopy_failed_copy_leaves_no_dest) {
    REQUIRE(U(CopyFileA));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; pp(s, "cp_nothing.txt"); pp(d, "cp_nothing_d.txt");
    cpA(s, d, FALSE);
    CHECK(!fa_exists(d));
}

/* ---------------- CopyFileEx ---------------- */
typedef struct {
    int calls; int chunk_calls; int stream_switch_calls;
    DWORD first_reason; LONGLONG first_total, first_xfer; DWORD first_streamno;
    LONGLONG last_xfer, last_total;
    LPVOID seen_data; int ret_first; int ret_rest; BOOL *cancel_on_call; int cancel_at;
    HANDLE hs, hd;
} pctx;
static DWORD CALLBACK cb(LARGE_INTEGER total, LARGE_INTEGER xfer, LARGE_INTEGER ssz, LARGE_INTEGER sxfer, DWORD sno, DWORD reason, HANDLE hs, HANDLE hd, LPVOID data) {
    pctx *c = (pctx *)data;
    if (c->calls == 0) { c->first_reason = reason; c->first_total = total.QuadPart; c->first_xfer = xfer.QuadPart; c->first_streamno = sno; c->hs = hs; c->hd = hd; }
    c->calls++;
    if (reason == CALLBACK_CHUNK_FINISHED) c->chunk_calls++; else c->stream_switch_calls++;
    c->last_xfer = xfer.QuadPart; c->last_total = total.QuadPart; c->seen_data = data;
    if (c->cancel_on_call && c->calls == c->cancel_at) *c->cancel_on_call = TRUE;
    return c->calls == 1 ? c->ret_first : c->ret_rest;
}
static BOOL cexW(const char *s, const char *d, LPPROGRESS_ROUTINE r, LPVOID data, LPBOOL cancel, DWORD flags) {
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_widen(a, s); fa_widen(b, d);
    return U(CopyFileExW)(a, b, r, data, cancel, flags);
}
static BOOL cexA(const char *s, const char *d, LPPROGRESS_ROUTINE r, LPVOID data, LPBOOL cancel, DWORD flags) { return U(CopyFileExA)(s, d, r, data, cancel, flags); }

TEST(fcopyex_basic_no_callback) {
    REQUIRE(U(CopyFileExA)); REQUIRE(U(CopyFileExW));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("ce_s.txt", "ex data"); pp(s, "ce_s.txt"); pp(d, "ce_d.txt");
    CHECKF(cexA(s, d, NULL, NULL, NULL, 0), "err %lu", GetLastError());
    CHECK(fa_content_is(d, "ex data")); fa_rm(d);
    CHECKF(cexW(s, d, NULL, NULL, NULL, 0), "err %lu", GetLastError());
    CHECK(fa_content_is(d, "ex data"));
    fa_rm(s); fa_rm(d);
}
TEST(fcopyex_fail_if_exists) {
    REQUIRE(U(CopyFileExW));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("ce_fe_s.txt", "new"); fa_mkfile_s("ce_fe_d.txt", "old"); pp(s, "ce_fe_s.txt"); pp(d, "ce_fe_d.txt");
    SetLastError(0); EXPECT_EQ(cexW(s, d, NULL, NULL, NULL, COPY_FILE_FAIL_IF_EXISTS), FALSE); EXPECT_ERR(ERROR_FILE_EXISTS);
    CHECK(fa_content_is(d, "old"));
    CHECK(cexW(s, d, NULL, NULL, NULL, 0));
    CHECK(fa_content_is(d, "new"));
    fa_rm(s); fa_rm(d);
}
TEST(fcopyex_invalid_flags_rejected) {
    REQUIRE(U(CopyFileExW)); REQUIRE(U(CopyFileExA));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("ce_if_s.txt", "x"); pp(s, "ce_if_s.txt"); pp(d, "ce_if_d.txt");
    DWORD rejected = 0xe91b4770;
    for (int bit = 0; bit < 32; bit++) {
        DWORD f = 1u << bit;
        if (!(rejected & f)) continue;
        SetLastError(0);
        BOOL r = cexW(s, d, NULL, NULL, NULL, f);
        DWORD e = GetLastError();
        CHECKF(!r && e == ERROR_INVALID_PARAMETER, "W flag 0x%lx r=%d err=%lu", f, r, e);
        SetLastError(0);
        r = cexA(s, d, NULL, NULL, NULL, f); e = GetLastError();
        CHECKF(!r && e == ERROR_INVALID_PARAMETER, "A flag 0x%lx r=%d err=%lu", f, r, e);
    }
    CHECK(!fa_exists(d));
    fa_rm(s);
}
TEST(fcopyex_invalid_flags_checked_before_files) {
    REQUIRE(U(CopyFileExW));
    SetLastError(0);
    EXPECT_EQ(U(CopyFileExW)(L"Z:\\no\\such", L"Z:\\no\\dest", NULL, NULL, NULL, 0x10), FALSE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(fcopyex_known_valid_flags_accepted) {
    REQUIRE(U(CopyFileExW));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("ce_vf_s.txt", "x"); pp(s, "ce_vf_s.txt"); pp(d, "ce_vf_d.txt");
    static const DWORD ok[] = { COPY_FILE_FAIL_IF_EXISTS, COPY_FILE_RESTARTABLE, COPY_FILE_ALLOW_DECRYPTED_DESTINATION, 0x1000 /*NO_BUFFERING*/, 0x40000 /*NO_OFFLOAD*/ };
    for (unsigned i = 0; i < sizeof ok / sizeof *ok; i++) {
        fa_rm(d);
        SetLastError(0);
        BOOL r = cexW(s, d, NULL, NULL, NULL, ok[i]);
        CHECKF(r || GetLastError() != ERROR_INVALID_PARAMETER, "flag 0x%lx rejected with 87", ok[i]);
        if (!r) t_note("flag 0x%lx -> err %lu", ok[i], GetLastError());
    }
    fa_rm(s); fa_rm(d);
}
TEST(fcopyex_progress_first_callback) {
    REQUIRE(U(CopyFileExW));
    mkpat("ce_pg_s.bin", 100000);
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; pp(s, "ce_pg_s.bin"); pp(d, "ce_pg_d.bin");
    pctx c; memset(&c, 0, sizeof c);
    CHECKF(cexW(s, d, cb, &c, NULL, 0), "err %lu", GetLastError());
    CHECK(c.calls >= 1);
    EXPECT_EQ(c.first_reason, CALLBACK_STREAM_SWITCH);
    EXPECT_EQ(c.first_total, 100000);
    EXPECT_EQ(c.first_xfer, 0);
    EXPECT_EQ(c.first_streamno, 1);
    CHECK(c.seen_data == &c);
    CHECK(c.hs != NULL && c.hs != INVALID_HANDLE_VALUE);
    CHECK(c.hd != NULL && c.hd != INVALID_HANDLE_VALUE);
    EXPECT_EQ(c.last_xfer, 100000);
    CHECK(cmpfiles(s, d));
    t_note("progress calls=%d chunk=%d switch=%d", c.calls, c.chunk_calls, c.stream_switch_calls);
    fa_rm(s); fa_rm(d);
}
TEST(fcopyex_progress_a_variant) {
    REQUIRE(U(CopyFileExA));
    mkpat("ce_pa_s.bin", 5000);
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; pp(s, "ce_pa_s.bin"); pp(d, "ce_pa_d.bin");
    pctx c; memset(&c, 0, sizeof c);
    CHECK(cexA(s, d, cb, &c, NULL, 0));
    EXPECT_EQ(c.first_reason, CALLBACK_STREAM_SWITCH);
    EXPECT_EQ(c.first_total, 5000);
    EXPECT_EQ(c.last_xfer, 5000);
    fa_rm(s); fa_rm(d);
}
TEST(fcopyex_progress_large_file_multiple_chunks) {
    REQUIRE(U(CopyFileExW));
    mkpat("ce_big_s.bin", 3 * 1024 * 1024);
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; pp(s, "ce_big_s.bin"); pp(d, "ce_big_d.bin");
    pctx c; memset(&c, 0, sizeof c);
    CHECK(cexW(s, d, cb, &c, NULL, 0));
    CHECKF(c.chunk_calls >= 2, "chunk callbacks %d", c.chunk_calls);
    EXPECT_EQ(c.last_xfer, 3 * 1024 * 1024);
    CHECK(cmpfiles(s, d));
    t_note("3MB copy: %d callbacks", c.calls);
    fa_rm(s); fa_rm(d);
}
TEST(fcopyex_progress_empty_file) {
    REQUIRE(U(CopyFileExW));
    fa_mkfile_s("ce_pe_s.txt", "");
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; pp(s, "ce_pe_s.txt"); pp(d, "ce_pe_d.txt");
    pctx c; memset(&c, 0, sizeof c);
    CHECKF(cexW(s, d, cb, &c, NULL, 0), "err %lu", GetLastError());
    t_note("empty-file copy callbacks=%d first_reason=%lu", c.calls, c.first_reason);
    CHECK(fa_exists(d));
    fa_rm(s); fa_rm(d);
}
TEST(fcopyex_progress_cancel_from_callback) {
    REQUIRE(U(CopyFileExW));
    mkpat("ce_cn_s.bin", 200000);
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; pp(s, "ce_cn_s.bin"); pp(d, "ce_cn_d.bin");
    pctx c; memset(&c, 0, sizeof c); c.ret_first = PROGRESS_CANCEL;
    SetLastError(0);
    EXPECT_EQ(cexW(s, d, cb, &c, NULL, 0), FALSE);
    EXPECT_ERR(ERROR_REQUEST_ABORTED);
    EXPECT_EQ(c.calls, 1);
    CHECK(!fa_exists(d));
    fa_rm(s);
}
TEST(fcopyex_progress_cancel_midway_removes_dest) {
    REQUIRE(U(CopyFileExW));
    mkpat("ce_cm_s.bin", 3 * 1024 * 1024);
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; pp(s, "ce_cm_s.bin"); pp(d, "ce_cm_d.bin");
    pctx c; memset(&c, 0, sizeof c); c.ret_first = PROGRESS_CONTINUE; c.ret_rest = PROGRESS_CANCEL;
    SetLastError(0);
    BOOL r = cexW(s, d, cb, &c, NULL, 0);
    DWORD e = GetLastError();
    CHECKF(!r && e == ERROR_REQUEST_ABORTED, "r=%d err=%lu", r, e);
    CHECK(!fa_exists(d));
    fa_rm(s);
}
TEST(fcopyex_progress_stop) {
    REQUIRE(U(CopyFileExW));
    mkpat("ce_st_s.bin", 200000);
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; pp(s, "ce_st_s.bin"); pp(d, "ce_st_d.bin");
    pctx c; memset(&c, 0, sizeof c); c.ret_first = PROGRESS_STOP;
    SetLastError(0);
    BOOL r = cexW(s, d, cb, &c, NULL, COPY_FILE_RESTARTABLE);
    DWORD e = GetLastError();
    CHECKF(!r && e == ERROR_REQUEST_ABORTED, "r=%d err=%lu", r, e);
    EXPECT_EQ(c.calls, 1);
    t_note("dest after STOP (restartable): %s", fa_exists(d) ? "kept" : "removed");
    fa_rm(s); fa_rm(d);
}
TEST(fcopyex_progress_stop_not_restartable) {
    REQUIRE(U(CopyFileExW));
    mkpat("ce_st2_s.bin", 200000);
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; pp(s, "ce_st2_s.bin"); pp(d, "ce_st2_d.bin");
    pctx c; memset(&c, 0, sizeof c); c.ret_first = PROGRESS_STOP;
    SetLastError(0);
    BOOL r = cexW(s, d, cb, &c, NULL, 0);
    t_note("STOP without RESTARTABLE: err=%lu dest %s", GetLastError(), fa_exists(d) ? "kept" : "removed");
    fa_rm(s); fa_rm(d);
}
TEST(fcopyex_progress_quiet) {
    REQUIRE(U(CopyFileExW));
    mkpat("ce_qt_s.bin", 3 * 1024 * 1024);
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; pp(s, "ce_qt_s.bin"); pp(d, "ce_qt_d.bin");
    pctx c; memset(&c, 0, sizeof c); c.ret_first = PROGRESS_QUIET;
    CHECKF(cexW(s, d, cb, &c, NULL, 0), "err %lu", GetLastError());
    EXPECT_EQ(c.calls, 1);
    CHECK(cmpfiles(s, d));
    fa_rm(s); fa_rm(d);
}
TEST(fcopyex_progress_continue_return_values) {
    REQUIRE(U(CopyFileExW));
    mkpat("ce_cv_s.bin", 3 * 1024 * 1024);
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; pp(s, "ce_cv_s.bin"); pp(d, "ce_cv_d.bin");
    pctx c; memset(&c, 0, sizeof c); c.ret_first = PROGRESS_CONTINUE; c.ret_rest = PROGRESS_CONTINUE;
    CHECK(cexW(s, d, cb, &c, NULL, 0));
    CHECK(c.calls > 1);
    CHECK(cmpfiles(s, d));
    fa_rm(s); fa_rm(d);
}
TEST(fcopyex_progress_unknown_return_value) {
    REQUIRE(U(CopyFileExW));
    mkpat("ce_uv_s.bin", 1000);
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; pp(s, "ce_uv_s.bin"); pp(d, "ce_uv_d.bin");
    pctx c; memset(&c, 0, sizeof c); c.ret_first = 77;
    SetLastError(0);
    BOOL r = cexW(s, d, cb, &c, NULL, 0);
    t_note("progress returning 77: r=%d err=%lu calls=%d", r, r ? 0 : GetLastError(), c.calls);
    fa_rm(s); fa_rm(d);
}
TEST(fcopyex_pbcancel_preset_true) {
    REQUIRE(U(CopyFileExW));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("ce_pc_s.txt", "cancel me"); pp(s, "ce_pc_s.txt"); pp(d, "ce_pc_d.txt");
    BOOL cancel = TRUE;
    SetLastError(0);
    EXPECT_EQ(cexW(s, d, NULL, NULL, &cancel, 0), FALSE);
    EXPECT_ERR(ERROR_REQUEST_ABORTED);
    CHECK(!fa_exists(d));
    fa_rm(s);
}
TEST(fcopyex_pbcancel_false_ok) {
    REQUIRE(U(CopyFileExW));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("ce_pf_s.txt", "fine"); pp(s, "ce_pf_s.txt"); pp(d, "ce_pf_d.txt");
    BOOL cancel = FALSE;
    CHECKF(cexW(s, d, NULL, NULL, &cancel, 0), "err %lu", GetLastError());
    CHECK(fa_content_is(d, "fine"));
    EXPECT_EQ(cancel, FALSE);
    fa_rm(s); fa_rm(d);
}
TEST(fcopyex_pbcancel_set_in_callback) {
    REQUIRE(U(CopyFileExW));
    mkpat("ce_pcb_s.bin", 3 * 1024 * 1024);
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; pp(s, "ce_pcb_s.bin"); pp(d, "ce_pcb_d.bin");
    BOOL cancel = FALSE;
    pctx c; memset(&c, 0, sizeof c); c.cancel_on_call = &cancel; c.cancel_at = 2;
    SetLastError(0);
    BOOL r = cexW(s, d, cb, &c, &cancel, 0);
    DWORD e = GetLastError();
    CHECKF(!r && e == ERROR_REQUEST_ABORTED, "r=%d err=%lu", r, e);
    CHECK(!fa_exists(d));
    fa_rm(s);
}
TEST(fcopyex_missing_source_no_callback) {
    REQUIRE(U(CopyFileExW));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; pp(s, "ce_miss.txt"); pp(d, "ce_miss_d.txt");
    pctx c; memset(&c, 0, sizeof c);
    SetLastError(0); EXPECT_EQ(cexW(s, d, cb, &c, NULL, 0), FALSE); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    EXPECT_EQ(c.calls, 0);
}
TEST(fcopyex_same_file) {
    REQUIRE(U(CopyFileExW));
    char s[MAX_PATH * 2]; fa_mkfile_s("ce_same.txt", "same"); pp(s, "ce_same.txt");
    SetLastError(0); EXPECT_EQ(cexW(s, s, NULL, NULL, NULL, 0), FALSE); EXPECT_ERR(ERROR_SHARING_VIOLATION);
    CHECK(fa_content_is(s, "same"));
    fa_rm(s);
}
TEST(fcopyex_source_directory) {
    REQUIRE(U(CopyFileExW));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkdir("ce_dir"); pp(s, "ce_dir"); pp(d, "ce_dir_copy");
    SetLastError(0); EXPECT_EQ(cexW(s, d, NULL, NULL, NULL, 0), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    fa_rm(s);
}
TEST(fcopyex_open_source_for_write_flag) {
    REQUIRE(U(CopyFileExW));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("ce_osw.txt", "w"); pp(s, "ce_osw.txt"); pp(d, "ce_osw_d.txt");
    HANDLE h = fa_open(s, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, 0);
    SetLastError(0);
    BOOL r = cexW(s, d, NULL, NULL, NULL, COPY_FILE_OPEN_SOURCE_FOR_WRITE);
    t_note("OPEN_SOURCE_FOR_WRITE with writer present: r=%d err=%lu", r, r ? 0 : GetLastError());
    CloseHandle(h); fa_rm(s); fa_rm(d);
}
TEST(fcopyex_readonly_dest) {
    REQUIRE(U(CopyFileExW));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("ce_ro_s.txt", "n"); fa_mkfile_s("ce_ro_d.txt", "o"); pp(s, "ce_ro_s.txt"); pp(d, "ce_ro_d.txt");
    SetFileAttributesA(d, FILE_ATTRIBUTE_READONLY);
    SetLastError(0); EXPECT_EQ(cexW(s, d, NULL, NULL, NULL, 0), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    fa_rm(s); fa_rm(d);
}
TEST(fcopyex_streams_progress_switches) {
    REQUIRE(U(CopyFileExW));
    FA_REQUIRE_STREAMS();
    char s[MAX_PATH * 2], d[MAX_PATH * 2], q[MAX_PATH * 2]; fa_mkfile_s("ce_as.txt", "main"); pp(s, "ce_as.txt"); pp(d, "ce_as_d.txt");
    snprintf(q, sizeof q, "%s:s1", s); fa_mkfile(q, "1111", 4);
    snprintf(q, sizeof q, "%s:s2", s); fa_mkfile(q, "22222", 5);
    pctx c; memset(&c, 0, sizeof c);
    CHECK(cexW(s, d, cb, &c, NULL, 0));
    CHECKF(c.stream_switch_calls >= 3, "stream switch callbacks %d (3 streams)", c.stream_switch_calls);
    snprintf(q, sizeof q, "%s:s2", d); CHECK(fa_content_is(q, "22222"));
    snprintf(q, sizeof q, "%s:s1", d); CHECK(fa_content_is(q, "1111"));
    fa_rm(s); fa_rm(d);
}
TEST(fcopyex_ads_skip_flag_value_accepted) {
    REQUIRE(U(CopyFileExW));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("ce_sk.txt", "m"); pp(s, "ce_sk.txt"); pp(d, "ce_sk_d.txt");
    SetLastError(0);
    BOOL r = cexW(s, d, NULL, NULL, NULL, 0x8000);
    CHECKF(r || GetLastError() != ERROR_INVALID_PARAMETER, "err %lu", GetLastError());
    fa_rm(s); fa_rm(d);
}

/* ---------------- CopyFile2 ---------------- */
typedef struct { DWORD dwSize; DWORD dwCopyFlags; BOOL *pfCancel; void *pProgressRoutine; PVOID pvCallbackContext; } cf2_params;
typedef struct { DWORD Type; DWORD pad; DWORD dwStreamNumber; DWORD dwReserved; HANDLE hSrc; HANDLE hDst; ULONGLONG a, b, c, d; } cf2_msg;
typedef HRESULT (WINAPI *cf2_fn)(PCWSTR, PCWSTR, const cf2_params *);
typedef struct { int n; int types[64]; int ret; int ret_at; PVOID ctx_seen; } cf2ctx;
static DWORD CALLBACK cf2cb(const cf2_msg *m, PVOID ctx) {
    cf2ctx *c = (cf2ctx *)ctx;
    if (c->n < 64) c->types[c->n] = (int)m->Type;
    c->n++; c->ctx_seen = ctx;
    return (c->ret_at && c->n < c->ret_at) ? 0 : c->ret;
}
static HRESULT cf2(const char *s, const char *d, const cf2_params *p) {
    cf2_fn f = (cf2_fn)t_proc("CopyFile2");
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_widen(a, s); fa_widen(b, d);
    return f(a, b, p);
}
#define H32(e) ((HRESULT)(0x80070000u | (e)))

TEST(fcopy2_basic_null_params) {
    REQUIRE(t_proc("CopyFile2"));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("c2_s.txt", "copy2"); pp(s, "c2_s.txt"); pp(d, "c2_d.txt");
    HRESULT hr = cf2(s, d, NULL);
    CHECKF(hr == S_OK, "hr 0x%lx", (unsigned long)hr);
    CHECK(fa_content_is(d, "copy2"));
    fa_rm(s); fa_rm(d);
}
TEST(fcopy2_overwrites_by_default) {
    REQUIRE(t_proc("CopyFile2"));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("c2_ov_s.txt", "new"); fa_mkfile_s("c2_ov_d.txt", "old"); pp(s, "c2_ov_s.txt"); pp(d, "c2_ov_d.txt");
    HRESULT hr = cf2(s, d, NULL);
    CHECKF(hr == S_OK, "hr 0x%lx", (unsigned long)hr);
    CHECK(fa_content_is(d, "new"));
    fa_rm(s); fa_rm(d);
}
TEST(fcopy2_fail_if_exists_hresult) {
    REQUIRE(t_proc("CopyFile2"));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("c2_fe_s.txt", "new"); fa_mkfile_s("c2_fe_d.txt", "old"); pp(s, "c2_fe_s.txt"); pp(d, "c2_fe_d.txt");
    cf2_params p; memset(&p, 0, sizeof p); p.dwSize = sizeof p; p.dwCopyFlags = COPY_FILE_FAIL_IF_EXISTS;
    HRESULT hr = cf2(s, d, &p);
    CHECKF(hr == H32(ERROR_FILE_EXISTS), "hr 0x%lx", (unsigned long)hr);
    CHECK(fa_content_is(d, "old"));
    fa_rm(s); fa_rm(d);
}
TEST(fcopy2_missing_source_hresult) {
    REQUIRE(t_proc("CopyFile2"));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; pp(s, "c2_miss.txt"); pp(d, "c2_miss_d.txt");
    HRESULT hr = cf2(s, d, NULL);
    CHECKF(hr == H32(ERROR_FILE_NOT_FOUND), "hr 0x%lx", (unsigned long)hr);
    pp(s, "c2_nodir\\miss.txt");
    hr = cf2(s, d, NULL);
    CHECKF(hr == H32(ERROR_PATH_NOT_FOUND), "hr 0x%lx", (unsigned long)hr);
}
TEST(fcopy2_does_not_set_last_error_contract) {
    REQUIRE(t_proc("CopyFile2"));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; pp(s, "c2_le.txt"); pp(d, "c2_le_d.txt");
    SetLastError(0);
    HRESULT hr = cf2(s, d, NULL);
    CHECK(FAILED(hr));
    t_note("GetLastError after failing CopyFile2 = %lu", GetLastError());
}
TEST(fcopy2_bad_struct_size) {
    REQUIRE(t_proc("CopyFile2"));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("c2_bs_s.txt", "x"); pp(s, "c2_bs_s.txt"); pp(d, "c2_bs_d.txt");
    static const DWORD sizes[] = { 0, 1, 0x10, 0x18, 0x1c, 0x21, 0x28, 0x40, 0xffffffff };
    for (unsigned i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        cf2_params p; memset(&p, 0, sizeof p); p.dwSize = sizes[i];
        HRESULT hr = cf2(s, d, &p);
        CHECKF(hr == E_INVALIDARG, "size 0x%lx hr 0x%lx", sizes[i], (unsigned long)hr);
    }
    CHECK(!fa_exists(d));
    fa_rm(s);
}
TEST(fcopy2_invalid_flags) {
    REQUIRE(t_proc("CopyFile2"));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("c2_if_s.txt", "x"); pp(s, "c2_if_s.txt"); pp(d, "c2_if_d.txt");
    static const DWORD bad[] = { 0x20000000, 0x40000000, 0x80000000, 0xe0000000 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        cf2_params p; memset(&p, 0, sizeof p); p.dwSize = sizeof p; p.dwCopyFlags = bad[i];
        HRESULT hr = cf2(s, d, &p);
        CHECKF(hr == E_INVALIDARG, "flags 0x%lx hr 0x%lx", bad[i], (unsigned long)hr);
    }
    /* 0x08000000 (COPY_FILE_REQUEST_COMPRESSED_TRAFFIC) cannot be combined with RESTARTABLE / RESUME_FROM_PAUSE */
    cf2_params p; memset(&p, 0, sizeof p); p.dwSize = sizeof p; p.dwCopyFlags = 0x08000000 | COPY_FILE_RESTARTABLE;
    HRESULT hr = cf2(s, d, &p);
    CHECKF(hr == E_INVALIDARG, "hr 0x%lx", (unsigned long)hr);
    p.dwCopyFlags = 0x08000000 | 0x4000;
    hr = cf2(s, d, &p);
    CHECKF(hr == E_INVALIDARG, "hr 0x%lx", (unsigned long)hr);
    CHECK(!fa_exists(d));
    fa_rm(s);
}
TEST(fcopy2_progress_messages) {
    REQUIRE(t_proc("CopyFile2"));
    mkpat("c2_pm_s.bin", 200000);
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; pp(s, "c2_pm_s.bin"); pp(d, "c2_pm_d.bin");
    cf2ctx c; memset(&c, 0, sizeof c);
    cf2_params p; memset(&p, 0, sizeof p); p.dwSize = sizeof p; p.pProgressRoutine = (void *)cf2cb; p.pvCallbackContext = &c;
    HRESULT hr = cf2(s, d, &p);
    CHECKF(hr == S_OK, "hr 0x%lx", (unsigned long)hr);
    CHECK(c.n >= 2);
    if (c.n >= 2) {
        t_note("first message type %d, last %d, count %d", c.types[0], c.types[c.n < 64 ? c.n - 1 : 63], c.n);
        CHECK(c.types[0] >= 1 && c.types[0] <= 6);
    }
    CHECK(c.ctx_seen == &c);
    CHECK(cmpfiles(s, d));
    fa_rm(s); fa_rm(d);
}
TEST(fcopy2_progress_cancel) {
    REQUIRE(t_proc("CopyFile2"));
    mkpat("c2_pc_s.bin", 200000);
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; pp(s, "c2_pc_s.bin"); pp(d, "c2_pc_d.bin");
    cf2ctx c; memset(&c, 0, sizeof c); c.ret = 1;   /* COPYFILE2_PROGRESS_CANCEL */
    cf2_params p; memset(&p, 0, sizeof p); p.dwSize = sizeof p; p.pProgressRoutine = (void *)cf2cb; p.pvCallbackContext = &c;
    HRESULT hr = cf2(s, d, &p);
    CHECKF(hr == H32(ERROR_REQUEST_ABORTED), "hr 0x%lx", (unsigned long)hr);
    CHECK(!fa_exists(d));
    fa_rm(s);
}
TEST(fcopy2_progress_quiet) {
    REQUIRE(t_proc("CopyFile2"));
    mkpat("c2_pq_s.bin", 200000);
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; pp(s, "c2_pq_s.bin"); pp(d, "c2_pq_d.bin");
    cf2ctx c; memset(&c, 0, sizeof c); c.ret = 3;   /* COPYFILE2_PROGRESS_QUIET */
    cf2_params p; memset(&p, 0, sizeof p); p.dwSize = sizeof p; p.pProgressRoutine = (void *)cf2cb; p.pvCallbackContext = &c;
    HRESULT hr = cf2(s, d, &p);
    CHECKF(hr == S_OK, "hr 0x%lx", (unsigned long)hr);
    EXPECT_EQ(c.n, 1);
    CHECK(cmpfiles(s, d));
    fa_rm(s); fa_rm(d);
}
TEST(fcopy2_pfcancel_preset) {
    REQUIRE(t_proc("CopyFile2"));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("c2_cn_s.txt", "x"); pp(s, "c2_cn_s.txt"); pp(d, "c2_cn_d.txt");
    BOOL cancel = TRUE;
    cf2_params p; memset(&p, 0, sizeof p); p.dwSize = sizeof p; p.pfCancel = &cancel;
    HRESULT hr = cf2(s, d, &p);
    CHECKF(hr == H32(ERROR_REQUEST_ABORTED), "hr 0x%lx", (unsigned long)hr);
    CHECK(!fa_exists(d));
    fa_rm(s);
}
TEST(fcopy2_same_file) {
    REQUIRE(t_proc("CopyFile2"));
    char s[MAX_PATH * 2]; fa_mkfile_s("c2_same.txt", "same"); pp(s, "c2_same.txt");
    HRESULT hr = cf2(s, s, NULL);
    CHECKF(hr == H32(ERROR_SHARING_VIOLATION), "hr 0x%lx", (unsigned long)hr);
    CHECK(fa_content_is(s, "same"));
    fa_rm(s);
}
TEST(fcopy2_readonly_dest_denied) {
    REQUIRE(t_proc("CopyFile2"));
    char s[MAX_PATH * 2], d[MAX_PATH * 2]; fa_mkfile_s("c2_ro_s.txt", "n"); fa_mkfile_s("c2_ro_d.txt", "o"); pp(s, "c2_ro_s.txt"); pp(d, "c2_ro_d.txt");
    SetFileAttributesA(d, FILE_ATTRIBUTE_READONLY);
    HRESULT hr = cf2(s, d, NULL);
    CHECKF(hr == H32(ERROR_ACCESS_DENIED), "hr 0x%lx", (unsigned long)hr);
    fa_rm(s); fa_rm(d);
}
TEST(fcopy2_null_names) {
    REQUIRE(t_proc("CopyFile2"));
    cf2_fn f = (cf2_fn)t_proc("CopyFile2");
    HRESULT hr = f(NULL, NULL, NULL);
    CHECK(FAILED(hr));
    t_note("CopyFile2(NULL,NULL) hr=0x%lx", (unsigned long)hr);
}
TEST(fcopy2_alternate_streams_copied) {
    REQUIRE(t_proc("CopyFile2"));
    FA_REQUIRE_STREAMS();
    char s[MAX_PATH * 2], d[MAX_PATH * 2], q[MAX_PATH * 2]; fa_mkfile_s("c2_as.txt", "m"); pp(s, "c2_as.txt"); pp(d, "c2_as_d.txt");
    snprintf(q, sizeof q, "%s:x", s); fa_mkfile(q, "xx", 2);
    HRESULT hr = cf2(s, d, NULL);
    CHECKF(hr == S_OK, "hr 0x%lx", (unsigned long)hr);
    snprintf(q, sizeof q, "%s:x", d); CHECK(fa_content_is(q, "xx"));
    fa_rm(s); fa_rm(d);
}
