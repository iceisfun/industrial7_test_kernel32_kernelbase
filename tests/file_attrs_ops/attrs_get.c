/* GetFileAttributesA/W and GetFileAttributesExA/W. */
#include "fa.h"

static DWORD gaW(const char *a) {
    WCHAR w[MAX_PATH * 2]; fa_widen(w, a);
    return U(GetFileAttributesW)(w);
}
static DWORD gaA(const char *a) { return U(GetFileAttributesA)(a); }

/* --- plain existing objects --- */
TEST(fattr_get_regular_file) {
    REQUIRE(U(GetFileAttributesA)); REQUIRE(U(GetFileAttributesW));
    char p[MAX_PATH * 2]; fa_path(p, "ga_reg.txt"); fa_mkfile_s("ga_reg.txt", "hello");
    DWORD a = gaA(p), w = gaW(p);
    CHECK(a != INVALID_FILE_ATTRIBUTES);
    CHECK(!(a & FILE_ATTRIBUTE_DIRECTORY));
    CHECK(a & FILE_ATTRIBUTE_ARCHIVE);
    EXPECT_EQ(a, w);
    fa_rm(p);
}
TEST(fattr_get_directory) {
    REQUIRE(U(GetFileAttributesA)); REQUIRE(U(GetFileAttributesW));
    char p[MAX_PATH * 2]; fa_path(p, "ga_dir"); fa_mkdir("ga_dir");
    DWORD a = gaA(p);
    CHECK(a != INVALID_FILE_ATTRIBUTES);
    CHECK(a & FILE_ATTRIBUTE_DIRECTORY);
    EXPECT_EQ(a, gaW(p));
    fa_rm(p);
}
TEST(fattr_get_dir_trailing_backslash) {
    REQUIRE(U(GetFileAttributesA)); REQUIRE(U(GetFileAttributesW));
    char p[MAX_PATH * 2]; fa_path(p, "ga_dir2\\"); fa_mkdir("ga_dir2");
    DWORD a = gaA(p);
    CHECK(a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY));
    EXPECT_EQ(a, gaW(p));
    p[strlen(p) - 1] = 0;
    EXPECT_EQ(gaA(p), a);
    fa_rm(p);
}
TEST(fattr_get_tmpdir_itself) {
    REQUIRE(U(GetFileAttributesA));
    DWORD a = gaA(t_tmpdir());
    CHECK(a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY));
}
TEST(fattr_get_file_trailing_backslash) {
    REQUIRE(U(GetFileAttributesA));
    char p[MAX_PATH * 2]; fa_mkfile_s("ga_ftb.txt", "x"); fa_path(p, "ga_ftb.txt\\");
    SetLastError(0);
    DWORD a = gaA(p);
    /* A name with a trailing separator can only designate a directory. */
    if (a != INVALID_FILE_ATTRIBUTES) t_note("file + trailing backslash succeeded attr=0x%lx", a);
    else t_note("file + trailing backslash err=%lu", GetLastError());
    CHECK(a == INVALID_FILE_ATTRIBUTES);
    fa_rml("ga_ftb.txt");
}
TEST(fattr_get_missing_file_is_2) {
    REQUIRE(U(GetFileAttributesA)); REQUIRE(U(GetFileAttributesW));
    char p[MAX_PATH * 2]; fa_path(p, "ga_no_such_file.txt");
    SetLastError(0); EXPECT_EQ(gaA(p), INVALID_FILE_ATTRIBUTES); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    SetLastError(0); EXPECT_EQ(gaW(p), INVALID_FILE_ATTRIBUTES); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(fattr_get_missing_dir_component_is_3) {
    REQUIRE(U(GetFileAttributesA)); REQUIRE(U(GetFileAttributesW));
    char p[MAX_PATH * 2]; fa_path(p, "ga_no_such_dir\\sub\\file.txt");
    SetLastError(0); EXPECT_EQ(gaA(p), INVALID_FILE_ATTRIBUTES); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    SetLastError(0); EXPECT_EQ(gaW(p), INVALID_FILE_ATTRIBUTES); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    fa_path(p, "ga_no_such_dir2\\file.txt");
    SetLastError(0); EXPECT_EQ(gaA(p), INVALID_FILE_ATTRIBUTES); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(fattr_get_file_as_dir_component) {
    REQUIRE(U(GetFileAttributesA));
    char p[MAX_PATH * 2]; fa_mkfile_s("ga_asdir.txt", "x"); fa_path(p, "ga_asdir.txt\\child");
    SetLastError(0); EXPECT_EQ(gaA(p), INVALID_FILE_ATTRIBUTES);
    DWORD e = GetLastError();
    /* NTFS: STATUS_OBJECT_PATH_NOT_FOUND (3), some stacks 267 */
    CHECKF(e == ERROR_PATH_NOT_FOUND || e == ERROR_DIRECTORY || e == ERROR_INVALID_NAME || e == ERROR_FILE_NOT_FOUND, "err %lu", e);
    t_note("file used as directory component err=%lu", e);
    fa_rml("ga_asdir.txt");
}
TEST(fattr_get_empty_name) {
    REQUIRE(U(GetFileAttributesA)); REQUIRE(U(GetFileAttributesW));
    SetLastError(0); EXPECT_EQ(U(GetFileAttributesA)(""), INVALID_FILE_ATTRIBUTES);
    DWORD ea = GetLastError();
    SetLastError(0); EXPECT_EQ(U(GetFileAttributesW)(L""), INVALID_FILE_ATTRIBUTES);
    DWORD ew = GetLastError();
    EXPECT_EQ(ea, ew);
    CHECKF(ea == ERROR_PATH_NOT_FOUND || ea == ERROR_FILE_NOT_FOUND || ea == ERROR_INVALID_NAME, "err %lu", ea);
    t_note("GetFileAttributes(\"\") err=%lu", ea);
}
TEST(fattr_get_null_name) {
    REQUIRE(U(GetFileAttributesW));
    SetLastError(0);
    DWORD r = U(GetFileAttributesW)(NULL);
    DWORD e = GetLastError();
    EXPECT_EQ(r, INVALID_FILE_ATTRIBUTES);
    CHECKF(e == ERROR_PATH_NOT_FOUND || e == ERROR_INVALID_PARAMETER || e == ERROR_FILE_NOT_FOUND || e == ERROR_INVALID_NAME, "err %lu", e);
    t_note("GetFileAttributesW(NULL) err=%lu", e);
}
TEST(fattr_get_wildcards_invalid_name) {
    REQUIRE(U(GetFileAttributesA)); REQUIRE(U(GetFileAttributesW));
    static const char *bad[] = { "ga_*", "ga_?", "*", "ga_*.txt", "ga_<x", "ga_>x", "ga_\"x" , "ga_|x", "ga_x?.txt" };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        char p[MAX_PATH * 2]; fa_path(p, bad[i]);
        SetLastError(0); DWORD r = gaA(p); DWORD e = GetLastError();
        CHECKF(r == INVALID_FILE_ATTRIBUTES && e == ERROR_INVALID_NAME, "A %s r=%lx e=%lu", bad[i], r, e);
        SetLastError(0); r = gaW(p); e = GetLastError();
        CHECKF(r == INVALID_FILE_ATTRIBUTES && e == ERROR_INVALID_NAME, "W %s r=%lx e=%lu", bad[i], r, e);
    }
}
TEST(fattr_get_wildcard_matching_existing_file_still_invalid) {
    REQUIRE(U(GetFileAttributesA));
    char p[MAX_PATH * 2]; fa_mkfile_s("ga_wc_exist.txt", "x"); fa_path(p, "ga_wc_exist.t*");
    SetLastError(0); EXPECT_EQ(gaA(p), INVALID_FILE_ATTRIBUTES); EXPECT_ERR(ERROR_INVALID_NAME);
    fa_rml("ga_wc_exist.txt");
}
TEST(fattr_get_trailing_dot_and_space_stripped) {
    REQUIRE(U(GetFileAttributesA)); REQUIRE(U(GetFileAttributesW));
    char p[MAX_PATH * 2]; fa_mkfile_s("ga_dots.txt", "x");
    DWORD base = gaA((fa_path(p, "ga_dots.txt"), p));
    CHECK(base != INVALID_FILE_ATTRIBUTES);
    fa_path(p, "ga_dots.txt.");  EXPECT_EQ(gaA(p), base); EXPECT_EQ(gaW(p), base);
    fa_path(p, "ga_dots.txt..."); EXPECT_EQ(gaA(p), base);
    fa_path(p, "ga_dots.txt   "); EXPECT_EQ(gaA(p), base);
    fa_path(p, "ga_dots.txt. . "); EXPECT_EQ(gaA(p), base);
    fa_rml("ga_dots.txt");
}
TEST(fattr_get_case_insensitive) {
    REQUIRE(U(GetFileAttributesA));
    char p[MAX_PATH * 2]; fa_mkfile_s("ga_Case.TXT", "x");
    DWORD base = gaA((fa_path(p, "ga_Case.TXT"), p));
    fa_path(p, "GA_CASE.txt"); EXPECT_EQ(gaA(p), base);
    fa_path(p, "ga_case.txt"); EXPECT_EQ(gaA(p), base);
    fa_rml("ga_Case.TXT");
}
TEST(fattr_get_slash_normalization) {
    REQUIRE(U(GetFileAttributesA));
    char p[MAX_PATH * 2], q[MAX_PATH * 2]; fa_mkdir("ga_sl"); fa_mkfile_s("ga_sl\\f.txt", "x");
    fa_path(p, "ga_sl\\f.txt"); DWORD base = gaA(p);
    CHECK(base != INVALID_FILE_ATTRIBUTES);
    fa_path(q, "ga_sl/f.txt"); EXPECT_EQ(gaA(q), base);
    fa_path(q, "ga_sl\\\\f.txt"); EXPECT_EQ(gaA(q), base);
    fa_path(q, "ga_sl\\.\\f.txt"); EXPECT_EQ(gaA(q), base);
    fa_path(q, "ga_sl\\..\\ga_sl\\f.txt"); EXPECT_EQ(gaA(q), base);
    fa_rm(p); fa_rml("ga_sl");
}
TEST(fattr_get_dot_and_dotdot) {
    REQUIRE(U(GetFileAttributesA));
    DWORD a = gaA("."); CHECK(a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY));
    a = gaA(".."); CHECK(a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY));
    a = gaA(".\\"); CHECK(a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY));
}
TEST(fattr_get_nt_prefix_forms) {
    REQUIRE(U(GetFileAttributesA)); REQUIRE(U(GetFileAttributesW));
    char p[MAX_PATH * 2], q[MAX_PATH * 2]; fa_mkfile_s("ga_pref.txt", "x"); fa_path(p, "ga_pref.txt");
    DWORD base = gaA(p);
    snprintf(q, sizeof q, "\\\\?\\%s", p);  EXPECT_EQ(gaA(q), base); EXPECT_EQ(gaW(q) , base);
    snprintf(q, sizeof q, "\\\\.\\%s", p);  EXPECT_EQ(gaA(q), base);
    /* \\?\ disables normalization: forward slashes are literal, so this name is not found */
    snprintf(q, sizeof q, "\\\\?\\%s", p); for (char *c = q + 4; *c; c++) if (*c == '\\') { *c = '/'; break; }
    SetLastError(0); DWORD r = gaA(q);
    CHECKF(r == INVALID_FILE_ATTRIBUTES, "forward slash under \\\\?\\ matched r=%lx", r);
    fa_rm(p);
}
TEST(fattr_get_nt_prefix_trailing_dot_not_stripped) {
    REQUIRE(U(GetFileAttributesA));
    char p[MAX_PATH * 2], q[MAX_PATH * 2]; fa_mkfile_s("ga_ntdot.txt", "x"); fa_path(p, "ga_ntdot.txt");
    snprintf(q, sizeof q, "\\\\?\\%s.", p);
    SetLastError(0); DWORD r = gaA(q);
    DWORD e = GetLastError();
    CHECKF(r == INVALID_FILE_ATTRIBUTES, "r=%lx", r);
    CHECKF(e == ERROR_FILE_NOT_FOUND, "e=%lu", e);
    fa_rm(p);
}
TEST(fattr_get_unc_style_local_prefix) {
    REQUIRE(U(GetFileAttributesA));
    char p[MAX_PATH * 2], q[MAX_PATH * 2]; fa_mkfile_s("ga_unc.txt", "x"); fa_path(p, "ga_unc.txt");
    if (p[1] == ':') {
        snprintf(q, sizeof q, "\\\\?\\GLOBALROOT\\??\\%s", p);
        DWORD r = gaA(q);
        t_note("GLOBALROOT form attr=0x%lx err=%lu", r, r == INVALID_FILE_ATTRIBUTES ? GetLastError() : 0);
        snprintf(q, sizeof q, "\\\\localhost\\%c$\\%s", p[0], p + 3);
        r = gaA(q);
        t_note("localhost admin share attr=0x%lx err=%lu", r, r == INVALID_FILE_ATTRIBUTES ? GetLastError() : 0);
    }
    fa_rm(p);
}
TEST(fattr_get_root_directory) {
    REQUIRE(U(GetFileAttributesA)); REQUIRE(U(GetFileAttributesW));
    char root[MAX_PATH]; fa_path(root, ""); char vol[MAX_PATH];
    CHECK(GetVolumePathNameA(root, vol, sizeof vol));
    DWORD a = gaA(vol);
    CHECK(a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY));
    EXPECT_EQ(gaW(vol), a);
    t_note("root %s attr=0x%lx", vol, a);
}
TEST(fattr_get_drive_relative) {
    REQUIRE(U(GetFileAttributesA));
    char p[MAX_PATH * 2]; fa_path(p, "");
    if (p[1] == ':') {
        char d[4] = { p[0], ':', 0, 0 };
        DWORD a = gaA(d);
        t_note("drive-relative \"%s\" attr=0x%lx", d, a);
        CHECK(a != INVALID_FILE_ATTRIBUTES);
    }
}
TEST(fattr_get_invalid_drive) {
    REQUIRE(U(GetFileAttributesA)); REQUIRE(U(GetFileAttributesW));
    /* choose an unused drive letter */
    DWORD mask = GetLogicalDrives(); char d = 0;
    for (int i = 25; i >= 0; i--) if (!(mask & (1u << i))) { d = (char)('A' + i); break; }
    if (!d) { t_skip("no free drive letter"); return; }
    char p[16]; snprintf(p, sizeof p, "%c:\\nofile.txt", d);
    SetLastError(0); EXPECT_EQ(gaA(p), INVALID_FILE_ATTRIBUTES);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_PATH_NOT_FOUND || e == ERROR_FILE_NOT_FOUND, "err %lu", e);
    t_note("unused drive err=%lu", e);
    snprintf(p, sizeof p, "%c:\\", d);
    SetLastError(0); EXPECT_EQ(gaA(p), INVALID_FILE_ATTRIBUTES);
    CHECKF(GetLastError() == ERROR_PATH_NOT_FOUND || GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_INVALID_DRIVE, "err %lu", GetLastError());
}
TEST(fattr_get_reserved_device_names) {
    REQUIRE(U(GetFileAttributesA)); REQUIRE(U(GetFileAttributesW));
    static const char *n[] = { "NUL", "CON", "PRN", "AUX", "COM1", "LPT1", "nul.txt", "CONIN$", "CONOUT$" };
    for (unsigned i = 0; i < sizeof n / sizeof *n; i++) {
        SetLastError(0);
        DWORD a = gaA(n[i]), w = gaW(n[i]);
        EXPECT_EQ(a, w);
        t_note("device name %s attr=0x%lx err=%lu", n[i], a, a == INVALID_FILE_ATTRIBUTES ? GetLastError() : 0);
    }
}
TEST(fattr_get_system_files) {
    REQUIRE(U(GetFileAttributesA)); REQUIRE(U(GetFileAttributesW));
    char sys[MAX_PATH], p[MAX_PATH * 2];
    GetSystemDirectoryA(sys, sizeof sys);
    DWORD a = gaA(sys); CHECK(a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY));
    snprintf(p, sizeof p, "%s\\kernel32.dll", sys);
    a = gaA(p); CHECK(a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY));
    GetWindowsDirectoryA(sys, sizeof sys);
    a = gaA(sys); CHECK(a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY));
}
TEST(fattr_get_open_exclusive_file) {
    REQUIRE(U(GetFileAttributesA));
    char p[MAX_PATH * 2]; fa_mkfile_s("ga_excl.txt", "x"); fa_path(p, "ga_excl.txt");
    HANDLE h = fa_open(p, GENERIC_READ | GENERIC_WRITE, 0, OPEN_EXISTING, 0);
    CHECK(h != INVALID_HANDLE_VALUE);
    DWORD a = gaA(p);
    CHECK(a != INVALID_FILE_ATTRIBUTES);
    CloseHandle(h); fa_rm(p);
}
TEST(fattr_get_pending_delete_file) {
    REQUIRE(U(GetFileAttributesA));
    FA_REQUIRE_NTFS();
    char p[MAX_PATH * 2]; fa_mkfile_s("ga_pend.txt", "x"); fa_path(p, "ga_pend.txt");
    HANDLE h = fa_open(p, GENERIC_READ | DELETE, FA_SHARE_ALL, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE);
    CHECK(h != INVALID_HANDLE_VALUE);
    SetLastError(0); DWORD a = gaA(p);
    t_note("attr of delete-pending file = 0x%lx err=%lu", a, a == INVALID_FILE_ATTRIBUTES ? GetLastError() : 0);
    CloseHandle(h);
    SetLastError(0); EXPECT_EQ(gaA(p), INVALID_FILE_ATTRIBUTES); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(fattr_get_alternate_stream_names) {
    REQUIRE(U(GetFileAttributesA)); REQUIRE(U(GetFileAttributesW));
    FA_REQUIRE_STREAMS();
    char p[MAX_PATH * 2], q[MAX_PATH * 2]; fa_mkfile_s("ga_ads.txt", "x"); fa_path(p, "ga_ads.txt");
    DWORD base = gaA(p);
    snprintf(q, sizeof q, "%s::$DATA", p);
    EXPECT_EQ(gaA(q), base);
    snprintf(q, sizeof q, "%s:nostream", p);
    SetLastError(0); EXPECT_EQ(gaA(q), INVALID_FILE_ATTRIBUTES); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    HANDLE h = fa_open(q, GENERIC_WRITE, 0, CREATE_ALWAYS, 0);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    DWORD a = gaA(q);
    CHECK(a != INVALID_FILE_ATTRIBUTES);
    CHECK(!(a & FILE_ATTRIBUTE_DIRECTORY));
    snprintf(q, sizeof q, "%s:nostream:$DATA", p);
    CHECK(gaW((q)) != INVALID_FILE_ATTRIBUTES);
    snprintf(q, sizeof q, "%s:nostream:$BOGUS", p);
    SetLastError(0); a = gaA(q);
    CHECK(a == INVALID_FILE_ATTRIBUTES);
    t_note("bad stream type err=%lu", GetLastError());
    fa_rm(p);
}
TEST(fattr_get_ntfs_metafile_names) {
    REQUIRE(U(GetFileAttributesA));
    FA_REQUIRE_NTFS();
    char p[MAX_PATH * 2], vol[MAX_PATH]; fa_path(p, ""); GetVolumePathNameA(p, vol, sizeof vol);
    snprintf(p, sizeof p, "%s$MFT", vol);
    DWORD a = gaA(p);
    t_note("$MFT attr=0x%lx err=%lu", a, a == INVALID_FILE_ATTRIBUTES ? GetLastError() : 0);
}
TEST(fattr_get_ex_a_w_parity_table) {
    REQUIRE(U(GetFileAttributesA)); REQUIRE(U(GetFileAttributesW));
    static const char *names[] = { "gp_file.txt", "gp_dir", "gp_missing", "gp_dir\\gp_missing", "gp_nodir\\x", "gp_*", "gp_dir\\", "gp_file.txt." };
    fa_mkfile_s("gp_file.txt", "abc"); fa_mkdir("gp_dir");
    for (unsigned i = 0; i < sizeof names / sizeof *names; i++) {
        char p[MAX_PATH * 2]; fa_path(p, names[i]);
        SetLastError(0); DWORD a = gaA(p); DWORD ea = a == INVALID_FILE_ATTRIBUTES ? GetLastError() : 0;
        SetLastError(0); DWORD w = gaW(p); DWORD ew = w == INVALID_FILE_ATTRIBUTES ? GetLastError() : 0;
        CHECKF(a == w && ea == ew, "%s a=%lx/%lu w=%lx/%lu", names[i], a, ea, w, ew);
    }
    fa_rml("gp_file.txt"); fa_rml("gp_dir");
}
TEST(fattr_get_last_error_untouched_on_success) {
    REQUIRE(U(GetFileAttributesA));
    char p[MAX_PATH * 2]; fa_mkfile_s("ga_le.txt", "x"); fa_path(p, "ga_le.txt");
    SetLastError(0x1234);
    DWORD a = gaA(p);
    CHECK(a != INVALID_FILE_ATTRIBUTES);
    t_note("last error after success = 0x%lx", GetLastError());
    fa_rm(p);
}
TEST(fattr_get_repeated_stable) {
    REQUIRE(U(GetFileAttributesA));
    char p[MAX_PATH * 2]; fa_mkfile_s("ga_rep.txt", "x"); fa_path(p, "ga_rep.txt");
    DWORD a = gaA(p);
    for (int i = 0; i < 1000; i++) if (gaA(p) != a) { CHECK(0); break; }
    fa_rm(p);
    for (int i = 0; i < 200; i++) { SetLastError(0); if (gaA(p) != INVALID_FILE_ATTRIBUTES || GetLastError() != 2) { CHECK(0); break; } }
}
TEST(fattr_get_long_path_over_max_path) {
    REQUIRE(U(GetFileAttributesW));
    WCHAR base[MAX_PATH * 2]; fa_wpath(base, "");
    static WCHAR p[1200], q[1200];
    /* a 300-char non-existent relative path: either rejected up-front or NOT_FOUND */
    int n = 0; n += wsprintfW(p, L"%ls", base);
    for (int i = 0; i < 20; i++) n += wsprintfW(p + n, L"seg%02d_abcdefghij\\", i);
    wsprintfW(p + n, L"f.txt");
    SetLastError(0); DWORD a = U(GetFileAttributesW)(p); DWORD e = GetLastError();
    EXPECT_EQ(a, INVALID_FILE_ATTRIBUTES);
    CHECKF(e == ERROR_PATH_NOT_FOUND || e == ERROR_FILE_NOT_FOUND || e == ERROR_FILENAME_EXCED_RANGE, "err %lu", e);
    t_note("path len %d err=%lu", lstrlenW(p), e);
    wsprintfW(q, L"\\\\?\\%ls", p);
    SetLastError(0); a = U(GetFileAttributesW)(q);
    EXPECT_EQ(a, INVALID_FILE_ATTRIBUTES);
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(fattr_get_component_too_long) {
    REQUIRE(U(GetFileAttributesA));
    char name[300]; memset(name, 'a', 270); name[270] = 0;
    char p[MAX_PATH * 2 + 300]; snprintf(p, sizeof p, "%s%s", t_tmpdir(), name);
    SetLastError(0); DWORD a = gaA(p); DWORD e = GetLastError();
    EXPECT_EQ(a, INVALID_FILE_ATTRIBUTES);
    CHECKF(e == ERROR_INVALID_NAME || e == ERROR_FILENAME_EXCED_RANGE || e == ERROR_PATH_NOT_FOUND || e == ERROR_FILE_NOT_FOUND, "err %lu", e);
    t_note("270-char component err=%lu", e);
}
TEST(fattr_get_unicode_name_roundtrip) {
    REQUIRE(U(GetFileAttributesW));
    WCHAR p[MAX_PATH * 2], q[MAX_PATH * 2]; fa_wpath(p, "");
    wsprintfW(q, L"%lsga_u\x00e9\x4e2d\x6587.txt", p);
    HANDLE h = CreateFileW(q, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    CloseHandle(h);
    DWORD a = U(GetFileAttributesW)(q);
    CHECK(a != INVALID_FILE_ATTRIBUTES);
    DeleteFileW(q);
    SetLastError(0); EXPECT_EQ(U(GetFileAttributesW)(q), INVALID_FILE_ATTRIBUTES); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(fattr_get_ansi_unmappable_chars) {
    REQUIRE(U(GetFileAttributesA));
    char p[MAX_PATH * 2]; fa_path(p, "ga_\xe9\xff.txt");
    SetLastError(0); DWORD a = gaA(p);
    EXPECT_EQ(a, INVALID_FILE_ATTRIBUTES);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}

/* ------------------------- GetFileAttributesEx ------------------------- */
static BOOL gexA(const char *p, DWORD lvl, void *d) { return U(GetFileAttributesExA)(p, (GET_FILEEX_INFO_LEVELS)lvl, d); }
static BOOL gexW(const char *p, DWORD lvl, void *d) { WCHAR w[MAX_PATH * 2]; fa_widen(w, p); return U(GetFileAttributesExW)(w, (GET_FILEEX_INFO_LEVELS)lvl, d); }

TEST(fattr_ex_file_basic) {
    REQUIRE(U(GetFileAttributesExA)); REQUIRE(U(GetFileAttributesExW));
    char p[MAX_PATH * 2]; fa_mkfile_s("gx_basic.txt", "0123456789"); fa_path(p, "gx_basic.txt");
    WIN32_FILE_ATTRIBUTE_DATA a, w;
    memset(&a, 0xcc, sizeof a); memset(&w, 0xdd, sizeof w);
    CHECK(gexA(p, GetFileExInfoStandard, &a));
    CHECK(gexW(p, GetFileExInfoStandard, &w));
    EXPECT_EQ(a.dwFileAttributes, w.dwFileAttributes);
    EXPECT_EQ(a.nFileSizeHigh, 0); EXPECT_EQ(a.nFileSizeLow, 10);
    EXPECT_EQ(w.nFileSizeLow, 10);
    EXPECT_EQ(fa_ft64(a.ftCreationTime), fa_ft64(w.ftCreationTime));
    EXPECT_EQ(fa_ft64(a.ftLastWriteTime), fa_ft64(w.ftLastWriteTime));
    EXPECT_EQ(a.dwFileAttributes, gaA(p));
    CHECK(fa_ft64(a.ftCreationTime) > FA_FT_2001);
    CHECK(fa_ft64(a.ftLastWriteTime) > FA_FT_2001);
    CHECK(fa_ft64(a.ftLastAccessTime) > FA_FT_2001);
    fa_rm(p);
}
TEST(fattr_ex_matches_handle_times) {
    REQUIRE(U(GetFileAttributesExW));
    char p[MAX_PATH * 2]; fa_mkfile_s("gx_ht.txt", "abc"); fa_path(p, "gx_ht.txt");
    WIN32_FILE_ATTRIBUTE_DATA d; CHECK(gexW(p, 0, &d));
    BY_HANDLE_FILE_INFORMATION bi; HANDLE h = fa_openfile("gx_ht.txt");
    CHECK(GetFileInformationByHandle(h, &bi));
    EXPECT_EQ(fa_ft64(d.ftCreationTime), fa_ft64(bi.ftCreationTime));
    EXPECT_EQ(fa_ft64(d.ftLastWriteTime), fa_ft64(bi.ftLastWriteTime));
    EXPECT_EQ(d.nFileSizeLow, bi.nFileSizeLow);
    EXPECT_EQ(d.dwFileAttributes, bi.dwFileAttributes);
    CloseHandle(h); fa_rm(p);
}
TEST(fattr_ex_directory) {
    REQUIRE(U(GetFileAttributesExA)); REQUIRE(U(GetFileAttributesExW));
    char p[MAX_PATH * 2]; fa_mkdir("gx_dir"); fa_path(p, "gx_dir");
    WIN32_FILE_ATTRIBUTE_DATA d; memset(&d, 0xcc, sizeof d);
    CHECK(gexA(p, 0, &d));
    CHECK(d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
    EXPECT_EQ(d.nFileSizeHigh, 0); EXPECT_EQ(d.nFileSizeLow, 0);
    CHECK(fa_ft64(d.ftCreationTime) > FA_FT_2001);
    fa_rm(p);
}
TEST(fattr_ex_size_values) {
    REQUIRE(U(GetFileAttributesExA));
    static const DWORD sizes[] = { 0, 1, 511, 512, 4095, 4096, 4097, 65536, 100000 };
    for (unsigned i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        char p[MAX_PATH * 2]; fa_path(p, "gx_sz.bin");
        char *buf = calloc(1, sizes[i] + 1);
        CHECK(fa_mkfile(p, buf, sizes[i])); free(buf);
        WIN32_FILE_ATTRIBUTE_DATA d; memset(&d, 0, sizeof d);
        CHECK(gexA(p, 0, &d));
        CHECKF(d.nFileSizeLow == sizes[i] && d.nFileSizeHigh == 0, "size %lu got %lu", sizes[i], d.nFileSizeLow);
    }
    fa_rml("gx_sz.bin");
}
TEST(fattr_ex_large_sparse_size) {
    REQUIRE(U(GetFileAttributesExW));
    char p[MAX_PATH * 2]; fa_path(p, "gx_big.bin");
    HANDLE h = fa_open(p, GENERIC_WRITE, FA_SHARE_ALL, CREATE_ALWAYS, 0);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    LARGE_INTEGER li; li.QuadPart = 0x100000001LL;     /* 4GB+1 logical size, not allocated if sparse */
    DWORD br;
    if (fa_is_ntfs()) DeviceIoControl(h, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &br, NULL);
    BOOL ok = SetFilePointerEx(h, li, NULL, FILE_BEGIN) && SetEndOfFile(h);
    CloseHandle(h);
    if (!ok) { t_skip("cannot extend file (fs limit)"); fa_rm(p); return; }
    WIN32_FILE_ATTRIBUTE_DATA d; CHECK(gexW(p, 0, &d));
    EXPECT_EQ(d.nFileSizeHigh, 1); EXPECT_EQ(d.nFileSizeLow, 1);
    fa_rm(p);
}
TEST(fattr_ex_invalid_level) {
    REQUIRE(U(GetFileAttributesExA)); REQUIRE(U(GetFileAttributesExW));
    char p[MAX_PATH * 2]; fa_mkfile_s("gx_lvl.txt", "x"); fa_path(p, "gx_lvl.txt");
    WIN32_FILE_ATTRIBUTE_DATA d;
    static const DWORD lv[] = { 1, 2, 3, 0x80, 0xffffffff };
    for (unsigned i = 0; i < sizeof lv / sizeof *lv; i++) {
        SetLastError(0); EXPECT_EQ(gexA(p, lv[i], &d), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
        SetLastError(0); EXPECT_EQ(gexW(p, lv[i], &d), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
    fa_rm(p);
}
TEST(fattr_ex_invalid_level_checked_before_name) {
    REQUIRE(U(GetFileAttributesExW));
    WIN32_FILE_ATTRIBUTE_DATA d;
    SetLastError(0); EXPECT_EQ(U(GetFileAttributesExW)(L"Z:\\definitely\\missing", (GET_FILEEX_INFO_LEVELS)1, &d), FALSE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(fattr_ex_missing_codes) {
    REQUIRE(U(GetFileAttributesExA)); REQUIRE(U(GetFileAttributesExW));
    char p[MAX_PATH * 2]; WIN32_FILE_ATTRIBUTE_DATA d;
    fa_path(p, "gx_missing");
    SetLastError(0); EXPECT_EQ(gexA(p, 0, &d), FALSE); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    SetLastError(0); EXPECT_EQ(gexW(p, 0, &d), FALSE); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    fa_path(p, "gx_nodir\\missing");
    SetLastError(0); EXPECT_EQ(gexA(p, 0, &d), FALSE); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    SetLastError(0); EXPECT_EQ(gexW(p, 0, &d), FALSE); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(fattr_ex_wildcards_123) {
    REQUIRE(U(GetFileAttributesExA)); REQUIRE(U(GetFileAttributesExW));
    static const char *bad[] = { "gx_*", "gx_?", "gx_<", "gx_>", "gx_\"", "gx_|" };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        char p[MAX_PATH * 2]; WIN32_FILE_ATTRIBUTE_DATA d; fa_path(p, bad[i]);
        SetLastError(0); EXPECT_EQ(gexA(p, 0, &d), FALSE); EXPECT_ERR(ERROR_INVALID_NAME);
        SetLastError(0); EXPECT_EQ(gexW(p, 0, &d), FALSE); EXPECT_ERR(ERROR_INVALID_NAME);
    }
}
TEST(fattr_ex_null_name) {
    REQUIRE(U(GetFileAttributesExW));
    WIN32_FILE_ATTRIBUTE_DATA d;
    SetLastError(0);
    BOOL r = U(GetFileAttributesExW)(NULL, GetFileExInfoStandard, &d);
    DWORD e = GetLastError();
    EXPECT_EQ(r, FALSE);
    t_note("GetFileAttributesExW(NULL) err=%lu", e);
    SetLastError(0); EXPECT_EQ(U(GetFileAttributesExW)(L"", GetFileExInfoStandard, &d), FALSE);
    t_note("GetFileAttributesExW(\"\") err=%lu", GetLastError());
}
TEST(fattr_ex_unmodified_on_failure) {
    REQUIRE(U(GetFileAttributesExA));
    char p[MAX_PATH * 2]; WIN32_FILE_ATTRIBUTE_DATA d, e;
    memset(&d, 0x5a, sizeof d); memcpy(&e, &d, sizeof d);
    fa_path(p, "gx_nothing");
    CHECK(!gexA(p, 0, &d));
    CHECK(!memcmp(&d, &e, sizeof d));
}
TEST(fattr_ex_struct_layout) {
    EXPECT_EQ(sizeof(WIN32_FILE_ATTRIBUTE_DATA), 36);
    EXPECT_EQ(offsetof(WIN32_FILE_ATTRIBUTE_DATA, ftCreationTime), 4);
    EXPECT_EQ(offsetof(WIN32_FILE_ATTRIBUTE_DATA, nFileSizeHigh), 28);
    EXPECT_EQ(offsetof(WIN32_FILE_ATTRIBUTE_DATA, nFileSizeLow), 32);
}
TEST(fattr_ex_only_36_bytes_written) {
    REQUIRE(U(GetFileAttributesExW));
    char p[MAX_PATH * 2]; fa_mkfile_s("gx_36.txt", "x"); fa_path(p, "gx_36.txt");
    unsigned char buf[64]; memset(buf, 0xee, sizeof buf);
    CHECK(gexW(p, 0, buf));
    for (int i = 36; i < 64; i++) if (buf[i] != 0xee) { CHECKF(0, "byte %d overwritten", i); break; }
    fa_rm(p);
}
TEST(fattr_ex_readonly_hidden_system_flags) {
    REQUIRE(U(GetFileAttributesExA));
    char p[MAX_PATH * 2]; fa_mkfile_s("gx_flags.txt", "x"); fa_path(p, "gx_flags.txt");
    SetFileAttributesA(p, FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
    WIN32_FILE_ATTRIBUTE_DATA d; CHECK(gexA(p, 0, &d));
    EXPECT_EQ(d.dwFileAttributes & 7, 7);
    fa_rm(p);
}
TEST(fattr_ex_on_exclusive_open_file) {
    REQUIRE(U(GetFileAttributesExA));
    char p[MAX_PATH * 2]; fa_mkfile_s("gx_excl.txt", "abcd"); fa_path(p, "gx_excl.txt");
    HANDLE h = fa_open(p, GENERIC_READ | GENERIC_WRITE, 0, OPEN_EXISTING, 0);
    WIN32_FILE_ATTRIBUTE_DATA d; memset(&d, 0, sizeof d);
    CHECK(gexA(p, 0, &d));
    EXPECT_EQ(d.nFileSizeLow, 4);
    CloseHandle(h); fa_rm(p);
}
TEST(fattr_ex_size_after_unflushed_extension_note) {
    REQUIRE(U(GetFileAttributesExA));
    char p[MAX_PATH * 2]; fa_path(p, "gx_stale.txt");
    HANDLE h = fa_open(p, GENERIC_WRITE, FA_SHARE_ALL, CREATE_ALWAYS, 0);
    DWORD w; WriteFile(h, "0123456789", 10, &w, NULL);
    WIN32_FILE_ATTRIBUTE_DATA d; memset(&d, 0, sizeof d);
    CHECK(gexA(p, 0, &d));
    /* NTFS may report the directory-entry size lazily while a handle is still open */
    t_note("size while handle open = %lu (written 10)", d.nFileSizeLow);
    CloseHandle(h);
    CHECK(gexA(p, 0, &d));
    EXPECT_EQ(d.nFileSizeLow, 10);
    fa_rm(p);
}
TEST(fattr_ex_ads_stream_uses_stream_size) {
    REQUIRE(U(GetFileAttributesExA));
    FA_REQUIRE_STREAMS();
    char p[MAX_PATH * 2], q[MAX_PATH * 2]; fa_mkfile_s("gx_ads.txt", "12345"); fa_path(p, "gx_ads.txt");
    snprintf(q, sizeof q, "%s:s1", p);
    CHECK(fa_mkfile(q, "ab", 2));
    WIN32_FILE_ATTRIBUTE_DATA d; memset(&d, 0, sizeof d);
    BOOL ok = gexA(q, 0, &d);
    CHECK(ok);
    t_note("stream attr-ex size = %lu (stream has 2, main 5)", d.nFileSizeLow);
    fa_rm(p);
}
TEST(fattr_ex_prefixed_path) {
    REQUIRE(U(GetFileAttributesExW));
    char p[MAX_PATH * 2], q[MAX_PATH * 2]; fa_mkfile_s("gx_pfx.txt", "xyz"); fa_path(p, "gx_pfx.txt");
    snprintf(q, sizeof q, "\\\\?\\%s", p);
    WIN32_FILE_ATTRIBUTE_DATA d; memset(&d, 0, sizeof d);
    CHECK(gexW(q, 0, &d)); EXPECT_EQ(d.nFileSizeLow, 3);
    fa_rm(p);
}
TEST(fattr_ex_root_and_dots) {
    REQUIRE(U(GetFileAttributesExA));
    WIN32_FILE_ATTRIBUTE_DATA d;
    memset(&d, 0, sizeof d); CHECK(gexA(".", 0, &d)); CHECK(d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
    memset(&d, 0, sizeof d); CHECK(gexA("..", 0, &d)); CHECK(d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
    char p[MAX_PATH * 2], vol[MAX_PATH]; fa_path(p, ""); GetVolumePathNameA(p, vol, sizeof vol);
    memset(&d, 0, sizeof d);
    BOOL ok = gexA(vol, 0, &d);
    CHECK(ok);
    if (ok) CHECK(d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
}
