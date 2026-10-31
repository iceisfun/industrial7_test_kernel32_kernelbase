#include "g18.h"
/* gap18: CopyFileExA error paths, flags, progress/cancel semantics. */

typedef struct { int calls, chunk, stream_switch; LONGLONG total, last_xfer; int ret; int cancel_after; DWORD data_ok; LPVOID expect; int bad_handles; } g18_prog;
static DWORD CALLBACK g18_cb(LARGE_INTEGER total, LARGE_INTEGER xfer, LARGE_INTEGER ssize, LARGE_INTEGER sx,
                             DWORD stream, DWORD reason, HANDLE hs, HANDLE hd, LPVOID data) {
    g18_prog *p = (g18_prog *)data;
    p->calls++;
    if (reason == CALLBACK_CHUNK_FINISHED) { p->chunk++; p->last_xfer = xfer.QuadPart; }
    else if (reason == CALLBACK_STREAM_SWITCH) p->stream_switch++;
    p->total = total.QuadPart;
    if (hs == NULL || hs == INVALID_HANDLE_VALUE || hd == NULL || hd == INVALID_HANDLE_VALUE) p->bad_handles++;
    if (p->cancel_after && p->calls >= p->cancel_after) return p->ret;
    return PROGRESS_CONTINUE;
}

static void mk(const char *leaf, char *out, const char *content) {
    g18_path(out, MAX_PATH, leaf);
    g18_put(out, content, (DWORD)strlen(content));
}

TEST(gap18_copyexa_basic) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH], d[MAX_PATH]; mk("cx_b_s.txt", s, "hello copy"); g18_path(d, MAX_PATH, "cx_b_d.txt"); g18_rm(d);
    SET_ERR(0);
    CHECK(U(CopyFileExA)(s, d, NULL, NULL, NULL, 0));
    CHECK(g18_content_is(d, "hello copy"));
    CHECK(g18_content_is(s, "hello copy"));
    g18_rm(s); g18_rm(d);
}
TEST(gap18_copyexa_overwrite_default) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH], d[MAX_PATH]; mk("cx_ow_s.txt", s, "new"); mk("cx_ow_d.txt", d, "old-longer-content");
    CHECK(U(CopyFileExA)(s, d, NULL, NULL, NULL, 0));
    CHECK(g18_content_is(d, "new"));
    EQ(g18_size(d), 3);
    g18_rm(s); g18_rm(d);
}
TEST(gap18_copyexa_fail_if_exists) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH], d[MAX_PATH]; mk("cx_fe_s.txt", s, "new"); mk("cx_fe_d.txt", d, "old");
    SET_ERR(0);
    CHECK(!U(CopyFileExA)(s, d, NULL, NULL, NULL, COPY_FILE_FAIL_IF_EXISTS));
    EXPECT_ERR(ERROR_FILE_EXISTS);
    CHECK(g18_content_is(d, "old"));
    g18_rm(d);
    CHECK(U(CopyFileExA)(s, d, NULL, NULL, NULL, COPY_FILE_FAIL_IF_EXISTS));
    CHECK(g18_content_is(d, "new"));
    g18_rm(s); g18_rm(d);
}
TEST(gap18_copyexa_missing_source) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH], d[MAX_PATH]; g18_path(s, MAX_PATH, "cx_nosrc.txt"); g18_path(d, MAX_PATH, "cx_nosrc_d.txt");
    g18_rm(s); g18_rm(d);
    SET_ERR(0);
    CHECK(!U(CopyFileExA)(s, d, NULL, NULL, NULL, 0));
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    CHECK(!g18_exists(d));
}
TEST(gap18_copyexa_missing_source_dir) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH], d[MAX_PATH]; g18_path(s, MAX_PATH, "cx_nodir_zz\\f.txt"); g18_path(d, MAX_PATH, "cx_nodir_d.txt");
    SET_ERR(0);
    CHECK(!U(CopyFileExA)(s, d, NULL, NULL, NULL, 0));
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    CHECK(!g18_exists(d));
}
TEST(gap18_copyexa_missing_dest_dir) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH], d[MAX_PATH]; mk("cx_nd_s.txt", s, "x"); g18_path(d, MAX_PATH, "cx_nd_zz\\d.txt");
    SET_ERR(0);
    CHECK(!U(CopyFileExA)(s, d, NULL, NULL, NULL, 0));
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    g18_rm(s);
}
TEST(gap18_copyexa_source_is_directory) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH], d[MAX_PATH]; g18_path(s, MAX_PATH, "cx_dirsrc"); g18_path(d, MAX_PATH, "cx_dirsrc_d.txt");
    CreateDirectoryA(s, NULL);
    SET_ERR(0);
    CHECK(!U(CopyFileExA)(s, d, NULL, NULL, NULL, 0));
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CHECK(!g18_exists(d));
    g18_rmdir(s);
}
TEST(gap18_copyexa_dest_is_directory) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH], d[MAX_PATH]; mk("cx_dd_s.txt", s, "x"); g18_path(d, MAX_PATH, "cx_dd_dir");
    CreateDirectoryA(d, NULL);
    SET_ERR(0);
    CHECK(!U(CopyFileExA)(s, d, NULL, NULL, NULL, 0));
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CHECK(GetFileAttributesA(d) & FILE_ATTRIBUTE_DIRECTORY);
    g18_rm(s); g18_rmdir(d);
}
TEST(gap18_copyexa_dest_readonly) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH], d[MAX_PATH]; mk("cx_ro_s.txt", s, "new"); mk("cx_ro_d.txt", d, "old");
    SetFileAttributesA(d, FILE_ATTRIBUTE_READONLY);
    SET_ERR(0);
    CHECK(!U(CopyFileExA)(s, d, NULL, NULL, NULL, 0));
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CHECK(g18_content_is(d, "old"));
    g18_rm(s); g18_rm(d);
}
TEST(gap18_copyexa_readonly_source_attr_copied) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH], d[MAX_PATH]; mk("cx_ra_s.txt", s, "abc"); g18_path(d, MAX_PATH, "cx_ra_d.txt"); g18_rm(d);
    SetFileAttributesA(s, FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN);
    CHECK(U(CopyFileExA)(s, d, NULL, NULL, NULL, 0));
    DWORD a = GetFileAttributesA(d);
    CHECK(a != INVALID_FILE_ATTRIBUTES);
    CHECK(a & FILE_ATTRIBUTE_READONLY);
    CHECK(a & FILE_ATTRIBUTE_HIDDEN);
    g18_rm(s); g18_rm(d);
}
TEST(gap18_copyexa_same_file) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH]; mk("cx_same.txt", s, "same");
    SET_ERR(0);
    CHECK(!U(CopyFileExA)(s, s, NULL, NULL, NULL, 0));
    EXPECT_ERR(ERROR_SHARING_VIOLATION);
    CHECK(g18_content_is(s, "same"));
    g18_rm(s);
}
TEST(gap18_copyexa_source_locked_exclusive) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH], d[MAX_PATH]; mk("cx_lock_s.txt", s, "locked"); g18_path(d, MAX_PATH, "cx_lock_d.txt"); g18_rm(d);
    HANDLE h = CreateFileA(s, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    SET_ERR(0);
    CHECK(!U(CopyFileExA)(s, d, NULL, NULL, NULL, 0));
    EXPECT_ERR(ERROR_SHARING_VIOLATION);
    CloseHandle(h);
    CHECK(U(CopyFileExA)(s, d, NULL, NULL, NULL, 0));
    g18_rm(s); g18_rm(d);
}
TEST(gap18_copyexa_source_open_for_read_ok) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH], d[MAX_PATH]; mk("cx_rd_s.txt", s, "shared"); g18_path(d, MAX_PATH, "cx_rd_d.txt"); g18_rm(d);
    HANDLE h = CreateFileA(s, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    CHECK(U(CopyFileExA)(s, d, NULL, NULL, NULL, 0));
    CHECK(g18_content_is(d, "shared"));
    CloseHandle(h); g18_rm(s); g18_rm(d);
}
TEST(gap18_copyexa_empty_file) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH], d[MAX_PATH]; mk("cx_e_s.txt", s, ""); g18_path(d, MAX_PATH, "cx_e_d.txt"); g18_rm(d);
    CHECK(U(CopyFileExA)(s, d, NULL, NULL, NULL, 0));
    EQ(g18_size(d), 0);
    g18_rm(s); g18_rm(d);
}
TEST(gap18_copyexa_timestamps_preserved) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH], d[MAX_PATH]; mk("cx_ts_s.txt", s, "time"); g18_path(d, MAX_PATH, "cx_ts_d.txt"); g18_rm(d);
    HANDLE h = CreateFileA(s, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    FILETIME ft = { 0xD53E8000, 0x01D2A6C3 };   /* arbitrary fixed time */
    CHECK(SetFileTime(h, NULL, NULL, &ft));
    CloseHandle(h);
    CHECK(U(CopyFileExA)(s, d, NULL, NULL, NULL, 0));
    WIN32_FILE_ATTRIBUTE_DATA a, b;
    CHECK(GetFileAttributesExA(s, GetFileExInfoStandard, &a));
    CHECK(GetFileAttributesExA(d, GetFileExInfoStandard, &b));
    EQ(b.ftLastWriteTime.dwLowDateTime, a.ftLastWriteTime.dwLowDateTime);
    EQ(b.ftLastWriteTime.dwHighDateTime, a.ftLastWriteTime.dwHighDateTime);
    g18_rm(s); g18_rm(d);
}
TEST(gap18_copyexa_invalid_flags) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH], d[MAX_PATH]; mk("cx_if_s.txt", s, "x"); g18_path(d, MAX_PATH, "cx_if_d.txt"); g18_rm(d);
    /* mask of rejected bits taken from CopyFileExW */
    static const DWORD bad[] = { 0x10, 0x20, 0x40, 0x100, 0x200, 0x400, 0x4000, 0x10000, 0x20000, 0x80000, 0x100000,
                                 0x1000000, 0x8000000, 0x20000000, 0x40000000, 0x80000000, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SET_ERR(0);
        BOOL r = U(CopyFileExA)(s, d, NULL, NULL, NULL, bad[i]);
        CHECKF(!r, "flags 0x%lx accepted", bad[i]);
        DWORD e = GetLastError();
        CHECKF(e == ERROR_INVALID_PARAMETER, "flags 0x%lx err %lu", bad[i], e);
        CHECKF(!g18_exists(d), "dest created for flags 0x%lx", bad[i]);
        if (r) g18_rm(d);
    }
    g18_rm(s);
}
TEST(gap18_copyexa_valid_flags) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH], d[MAX_PATH]; mk("cx_vf_s.txt", s, "flagged"); g18_path(d, MAX_PATH, "cx_vf_d.txt");
    static const DWORD good[] = { 0, COPY_FILE_RESTARTABLE, COPY_FILE_OPEN_SOURCE_FOR_WRITE, COPY_FILE_ALLOW_DECRYPTED_DESTINATION,
                                  COPY_FILE_COPY_SYMLINK, COPY_FILE_NO_BUFFERING, 0x10000000 /* REQUEST_COMPRESSED_TRAFFIC */ };
    for (unsigned i = 0; i < sizeof good / sizeof good[0]; i++) {
        g18_rm(d); SET_ERR(0);
        BOOL r = U(CopyFileExA)(s, d, NULL, NULL, NULL, good[i]);
        DWORD e = GetLastError();
        if (!r) { CHECKF(e != ERROR_INVALID_PARAMETER || good[i] == 0x10000000, "flags 0x%lx rejected as invalid", good[i]); t_note("flags 0x%lx failed err %lu", good[i], e); }
        else CHECKF(g18_content_is(d, "flagged"), "flags 0x%lx content", good[i]);
    }
    g18_rm(s); g18_rm(d);
}
TEST(gap18_copyexa_pbcancel_preset) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH], d[MAX_PATH]; mk("cx_pc_s.txt", s, "cancel me"); g18_path(d, MAX_PATH, "cx_pc_d.txt"); g18_rm(d);
    BOOL cancel = TRUE;
    SET_ERR(0);
    CHECK(!U(CopyFileExA)(s, d, NULL, NULL, &cancel, 0));
    EXPECT_ERR(ERROR_REQUEST_ABORTED);
    t_note("dest exists after preset pbCancel: %d", g18_exists(d));
    g18_rm(s); g18_rm(d);
}
TEST(gap18_copyexa_pbcancel_false) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH], d[MAX_PATH]; mk("cx_pf_s.txt", s, "go"); g18_path(d, MAX_PATH, "cx_pf_d.txt"); g18_rm(d);
    BOOL cancel = FALSE;
    CHECK(U(CopyFileExA)(s, d, NULL, NULL, &cancel, 0));
    CHECK(!cancel);
    CHECK(g18_content_is(d, "go"));
    g18_rm(s); g18_rm(d);
}
static void mk_big(const char *path, DWORD n) {
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    static char buf[65536]; for (unsigned i = 0; i < sizeof buf; i++) buf[i] = (char)(i * 7 + 3);
    DWORD w; while (n) { DWORD c = n > sizeof buf ? sizeof buf : n; WriteFile(h, buf, c, &w, NULL); n -= c; }
    CloseHandle(h);
}
TEST(gap18_copyexa_progress_basic) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH], d[MAX_PATH]; g18_path(s, MAX_PATH, "cx_pg_s.bin"); g18_path(d, MAX_PATH, "cx_pg_d.bin"); g18_rm(d);
    mk_big(s, 1500000);
    g18_prog p; memset(&p, 0, sizeof p);
    CHECK(U(CopyFileExA)(s, d, g18_cb, &p, NULL, 0));
    EQ(p.total, 1500000);
    CHECK(p.calls >= 1);
    CHECK(p.chunk >= 1);
    CHECK(p.stream_switch >= 1);
    EQ(p.last_xfer, 1500000);
    EQ(p.bad_handles, 0);
    EQ(g18_size(d), 1500000);
    g18_rm(s); g18_rm(d);
}
TEST(gap18_copyexa_progress_cancel) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH], d[MAX_PATH]; g18_path(s, MAX_PATH, "cx_pcn_s.bin"); g18_path(d, MAX_PATH, "cx_pcn_d.bin"); g18_rm(d);
    mk_big(s, 1500000);
    g18_prog p; memset(&p, 0, sizeof p); p.ret = PROGRESS_CANCEL; p.cancel_after = 2;
    SET_ERR(0);
    CHECK(!U(CopyFileExA)(s, d, g18_cb, &p, NULL, 0));
    EXPECT_ERR(ERROR_REQUEST_ABORTED);
    CHECK(p.calls >= 2);
    CHECK(!g18_exists(d));        /* PROGRESS_CANCEL deletes the partial destination */
    g18_rm(s); g18_rm(d);
}
TEST(gap18_copyexa_progress_stop) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH], d[MAX_PATH]; g18_path(s, MAX_PATH, "cx_pst_s.bin"); g18_path(d, MAX_PATH, "cx_pst_d.bin"); g18_rm(d);
    mk_big(s, 1500000);
    g18_prog p; memset(&p, 0, sizeof p); p.ret = PROGRESS_STOP; p.cancel_after = 2;
    SET_ERR(0);
    CHECK(!U(CopyFileExA)(s, d, g18_cb, &p, NULL, 0));
    EXPECT_ERR(ERROR_REQUEST_ABORTED);
    t_note("PROGRESS_STOP leaves partial dest: exists=%d size=%lld", g18_exists(d), g18_size(d));
    g18_rm(s); g18_rm(d);
}
TEST(gap18_copyexa_progress_quiet) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH], d[MAX_PATH]; g18_path(s, MAX_PATH, "cx_pq_s.bin"); g18_path(d, MAX_PATH, "cx_pq_d.bin"); g18_rm(d);
    mk_big(s, 1500000);
    g18_prog p; memset(&p, 0, sizeof p); p.ret = PROGRESS_QUIET; p.cancel_after = 1;
    CHECK(U(CopyFileExA)(s, d, g18_cb, &p, NULL, 0));
    EQ(p.calls, 1);      /* no further callbacks after PROGRESS_QUIET */
    EQ(g18_size(d), 1500000);
    g18_rm(s); g18_rm(d);
}
TEST(gap18_copyexa_progress_empty_file) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH], d[MAX_PATH]; mk("cx_pe_s.txt", s, ""); g18_path(d, MAX_PATH, "cx_pe_d.txt"); g18_rm(d);
    g18_prog p; memset(&p, 0, sizeof p);
    CHECK(U(CopyFileExA)(s, d, g18_cb, &p, NULL, 0));
    EQ(p.total, 0);
    t_note("callbacks for empty file: %d", p.calls);
    g18_rm(s); g18_rm(d);
}
TEST(gap18_copyexa_progress_not_called_on_early_fail) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH], d[MAX_PATH]; g18_path(s, MAX_PATH, "cx_pnf_missing"); g18_path(d, MAX_PATH, "cx_pnf_d"); g18_rm(s);
    g18_prog p; memset(&p, 0, sizeof p);
    CHECK(!U(CopyFileExA)(s, d, g18_cb, &p, NULL, 0));
    EQ(p.calls, 0);
}
TEST(gap18_copyexa_null_and_empty_names) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH]; mk("cx_nn_s.txt", s, "x");
    SET_ERR(0);
    CHECK(!U(CopyFileExA)(s, "", NULL, NULL, NULL, 0));
    t_note("empty dest err %lu", GetLastError());
    SET_ERR(0);
    CHECK(!U(CopyFileExA)("", s, NULL, NULL, NULL, 0));
    t_note("empty src err %lu", GetLastError());
    SET_ERR(0);
    CHECK(!U(CopyFileExA)(s, NULL, NULL, NULL, NULL, 0));
    t_note("NULL dest err %lu", GetLastError());
    g18_rm(s);
}
TEST(gap18_copyexa_trailing_chars) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH], d[MAX_PATH]; mk("cx_tc_s.txt", s, "t"); g18_path(d, MAX_PATH, "cx_tc_d.txt\\");
    SET_ERR(0);
    CHECK(!U(CopyFileExA)(s, d, NULL, NULL, NULL, 0));
    t_note("dest with trailing backslash err %lu", GetLastError());
    char d2[MAX_PATH]; g18_path(d2, MAX_PATH, "cx_tc_w*.txt");
    SET_ERR(0);
    CHECK(!U(CopyFileExA)(s, d2, NULL, NULL, NULL, 0));
    EXPECT_ERR(ERROR_INVALID_NAME);
    g18_rm(s);
}
TEST(gap18_copyexa_wildcard_source) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH], d[MAX_PATH]; mk("cx_wc_a.txt", s, "t"); g18_path(s, MAX_PATH, "cx_wc_*.txt"); g18_path(d, MAX_PATH, "cx_wc_d.txt");
    SET_ERR(0);
    CHECK(!U(CopyFileExA)(s, d, NULL, NULL, NULL, 0));
    EXPECT_ERR(ERROR_INVALID_NAME);
    CHECK(!g18_exists(d));
    g18_path(s, MAX_PATH, "cx_wc_a.txt"); g18_rm(s);
}
TEST(gap18_copyexa_ansi_wide_parity) {
    REQUIRE(U(CopyFileExA)); REQUIRE(U(CopyFileExW));
    char s[MAX_PATH], da[MAX_PATH], dw[MAX_PATH]; mk("cx_par_s.txt", s, "parity");
    g18_path(da, MAX_PATH, "cx_par_a.txt"); g18_path(dw, MAX_PATH, "cx_par_w.txt"); g18_rm(da); g18_rm(dw);
    WCHAR ws[MAX_PATH], wd[MAX_PATH]; g18_widen(ws, MAX_PATH, s); g18_widen(wd, MAX_PATH, dw);
    CHECK(U(CopyFileExA)(s, da, NULL, NULL, NULL, 0));
    CHECK(U(CopyFileExW)(ws, wd, NULL, NULL, NULL, 0));
    CHECK(g18_content_is(da, "parity")); CHECK(g18_content_is(dw, "parity"));
    /* same failure for both */
    SET_ERR(0); CHECK(!U(CopyFileExA)(s, da, NULL, NULL, NULL, COPY_FILE_FAIL_IF_EXISTS)); DWORD ea = GetLastError();
    SET_ERR(0); CHECK(!U(CopyFileExW)(ws, wd, NULL, NULL, NULL, COPY_FILE_FAIL_IF_EXISTS)); DWORD ew = GetLastError();
    EQ(ea, ew);
    g18_rm(s); g18_rm(da); g18_rm(dw);
}
TEST(gap18_copyexa_ansi_high_chars) {
    REQUIRE(U(CopyFileExA));
    if (GetACP() != 1252) { t_skip("needs ACP 1252"); return; }
    char s[MAX_PATH], d[MAX_PATH]; g18_path(s, MAX_PATH, "cx_caf\xe9.txt"); g18_path(d, MAX_PATH, "cx_caf\xe9_copy.txt");
    CHECK(g18_put(s, "acc", 3)); g18_rm(d);
    CHECK(U(CopyFileExA)(s, d, NULL, NULL, NULL, 0));
    CHECK(g18_content_is(d, "acc"));
    g18_rm(s); g18_rm(d);
}
TEST(gap18_copyexa_long_name_260) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH], d[600]; mk("cx_ln_s.txt", s, "x");
    g18_path(d, sizeof d, "");
    size_t l = strlen(d); memset(d + l, 'a', 300); strcpy(d + l + 300, ".txt");
    SET_ERR(0);
    CHECK(!U(CopyFileExA)(s, d, NULL, NULL, NULL, 0));
    t_note("dest path %zu chars (no \\\\?\\ prefix, no manifest) err %lu", strlen(d), GetLastError());
    g18_rm(s);
}
TEST(gap18_copyexa_long_prefix_ok) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH], d[MAX_PATH + 8]; mk("cx_lp_s.txt", s, "longprefix");
    strcpy(d, "\\\\?\\"); g18_path(d + 4, MAX_PATH, "cx_lp_d.txt"); g18_rm(d);
    CHECK(U(CopyFileExA)(s, d, NULL, NULL, NULL, 0));
    CHECK(g18_content_is(d, "longprefix"));
    g18_rm(d); g18_rm(s);
}
TEST(gap18_copyexa_stream_copy) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH], d[MAX_PATH], sa[MAX_PATH + 8], da[MAX_PATH + 8];
    mk("cx_ads_s.txt", s, "main"); g18_path(d, MAX_PATH, "cx_ads_d.txt"); g18_rm(d);
    snprintf(sa, sizeof sa, "%s:alt", s);
    HANDLE h = CreateFileA(sa, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_skip("no ADS support"); g18_rm(s); return; }
    DWORD w; WriteFile(h, "altdata", 7, &w, NULL); CloseHandle(h);
    CHECK(U(CopyFileExA)(s, d, NULL, NULL, NULL, 0));
    snprintf(da, sizeof da, "%s:alt", d);
    CHECK(g18_content_is(da, "altdata"));
    g18_rm(s); g18_rm(d);
}
TEST(gap18_copyexa_dest_open_by_other) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH], d[MAX_PATH]; mk("cx_do_s.txt", s, "new"); mk("cx_do_d.txt", d, "old");
    HANDLE h = CreateFileA(d, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    SET_ERR(0);
    CHECK(!U(CopyFileExA)(s, d, NULL, NULL, NULL, 0));
    EXPECT_ERR(ERROR_SHARING_VIOLATION);
    CloseHandle(h);
    CHECK(g18_content_is(d, "old"));
    g18_rm(s); g18_rm(d);
}
TEST(gap18_copyexa_repeat_100) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH], d[MAX_PATH]; mk("cx_rp_s.txt", s, "repeat"); g18_path(d, MAX_PATH, "cx_rp_d.txt");
    for (int i = 0; i < 100; i++) {
        CHECKF(U(CopyFileExA)(s, d, NULL, NULL, NULL, 0), "iter %d err %lu", i, GetLastError());
    }
    CHECK(g18_content_is(d, "repeat"));
    g18_rm(s); g18_rm(d);
}
TEST(gap18_copyexa_k32_forward) {
    REQUIRE(UK(CopyFileExA));
    char s[MAX_PATH], d[MAX_PATH]; mk("cx_kf_s.txt", s, "k32"); g18_path(d, MAX_PATH, "cx_kf_d.txt"); g18_rm(d);
    CHECK(UK(CopyFileExA)(s, d, NULL, NULL, NULL, 0));
    CHECK(g18_content_is(d, "k32"));
    g18_rm(s); g18_rm(d);
}
TEST(gap18_copyexa_dest_is_dot_dir_names) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH], d[MAX_PATH]; mk("cx_dot_s.txt", s, "x");
    g18_path(d, MAX_PATH, ".");
    SET_ERR(0);
    CHECK(!U(CopyFileExA)(s, d, NULL, NULL, NULL, 0));
    t_note("dest '.' err %lu", GetLastError());
    g18_path(d, MAX_PATH, "..\\");
    SET_ERR(0);
    CHECK(!U(CopyFileExA)(s, d, NULL, NULL, NULL, 0));
    g18_rm(s);
}
TEST(gap18_copyexa_to_dos_device) {
    REQUIRE(U(CopyFileExA));
    char s[MAX_PATH]; mk("cx_nul_s.txt", s, "to nul");
    SET_ERR(0);
    BOOL r = U(CopyFileExA)(s, "NUL", NULL, NULL, NULL, 0);
    t_note("copy to NUL: %d err %lu", r, GetLastError());
    g18_rm(s);
}
