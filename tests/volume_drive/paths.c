/* GetVolumePathName, GetVolumeNameForVolumeMountPoint, GetVolumePathNamesForVolumeName */
#include "vd.h"

static BOOL vd_gvpn_a(const char *in, char *out, DWORD cch) { return U(GetVolumePathNameA)(in, out, cch); }

/* ---- GetVolumePathName ---- */
TEST(vd_volpath_root_of_root) {
    REQUIRE(U(GetVolumePathNameA));
    char r[4]; vd_sysroot(r); char o[MAX_PATH] = "";
    CHECK(vd_gvpn_a(r, o, sizeof o));
    EXPECT_EQ(vd_stricmp(o, r), 0);
}
TEST(vd_volpath_w_root_of_root) {
    REQUIRE(U(GetVolumePathNameW));
    char r[4]; vd_sysroot(r); WCHAR w[4], o[MAX_PATH]; vd_widen(r, w, 4);
    CHECK(U(GetVolumePathNameW)(w, o, MAX_PATH));
    CHECK(!lstrcmpiW(o, w));
}
TEST(vd_volpath_deep_file_path) {
    REQUIRE(U(GetVolumePathNameA));
    char p[MAX_PATH], r[4], o[MAX_PATH]; vd_sysroot(r);
    GetSystemDirectoryA(p, sizeof p); strcat(p, "\\kernel32.dll");
    CHECK(vd_gvpn_a(p, o, sizeof o)); EXPECT_EQ(vd_stricmp(o, r), 0);
}
TEST(vd_volpath_directory_with_trailing_backslash) {
    REQUIRE(U(GetVolumePathNameA));
    char p[MAX_PATH], r[4], o[MAX_PATH]; vd_sysroot(r);
    GetSystemDirectoryA(p, sizeof p); strcat(p, "\\");
    CHECK(vd_gvpn_a(p, o, sizeof o)); EXPECT_EQ(vd_stricmp(o, r), 0);
}
TEST(vd_volpath_nonexistent_file_on_existing_drive) {
    REQUIRE(U(GetVolumePathNameA));
    char p[MAX_PATH], r[4], o[MAX_PATH]; vd_sysroot(r);
    sprintf(p, "%s%s", r, "k32t_no_such_dir\\deeper\\nofile.txt");
    CHECK(vd_gvpn_a(p, o, sizeof o)); EXPECT_EQ(vd_stricmp(o, r), 0);
}
TEST(vd_volpath_tmpfile_before_creation) {
    REQUIRE(U(GetVolumePathNameA));
    char r[4]; vd_tmproot(r); if (!r[0]) { t_skip("tmp not a letter path"); return; }
    char p[MAX_PATH], o[MAX_PATH]; t_tmpfile(p, sizeof p, "not_yet_there.txt");
    CHECK(vd_gvpn_a(p, o, sizeof o)); EXPECT_EQ(vd_stricmp(o, r), 0);
}
TEST(vd_volpath_drive_relative_no_slash) {
    REQUIRE(U(GetVolumePathNameA));
    char r[4]; vd_sysroot(r); char s[3] = { r[0], ':', 0 }, o[MAX_PATH];
    CHECK(vd_gvpn_a(s, o, sizeof o)); EXPECT_EQ(vd_stricmp(o, r), 0);
}
TEST(vd_volpath_lowercase_input) {
    REQUIRE(U(GetVolumePathNameA));
    char r[4]; vd_sysroot(r); char p[32], o[MAX_PATH]; sprintf(p, "%c:\\windows", tolower(r[0]));
    CHECK(vd_gvpn_a(p, o, sizeof o)); EXPECT_EQ(vd_stricmp(o, r), 0);
}
TEST(vd_volpath_relative_dot) {
    REQUIRE(U(GetVolumePathNameA));
    char cwd[MAX_PATH], o[MAX_PATH], r[4]; GetCurrentDirectoryA(sizeof cwd, cwd);
    if (cwd[1] != ':') { t_skip("cwd not drive path"); return; }
    r[0] = cwd[0]; r[1] = ':'; r[2] = '\\'; r[3] = 0;
    CHECK(vd_gvpn_a(".", o, sizeof o)); EXPECT_EQ(vd_stricmp(o, r), 0);
    CHECK(vd_gvpn_a("some\\relative\\name.txt", o, sizeof o)); EXPECT_EQ(vd_stricmp(o, r), 0);
}
TEST(vd_volpath_rooted_no_drive) {
    REQUIRE(U(GetVolumePathNameA));
    char cwd[MAX_PATH], o[MAX_PATH], r[4]; GetCurrentDirectoryA(sizeof cwd, cwd);
    if (cwd[1] != ':') { t_skip("cwd not drive path"); return; }
    r[0] = cwd[0]; r[1] = ':'; r[2] = '\\'; r[3] = 0;
    CHECK(vd_gvpn_a("\\Windows", o, sizeof o)); EXPECT_EQ(vd_stricmp(o, r), 0);
}
TEST(vd_volpath_dotdot_resolution) {
    REQUIRE(U(GetVolumePathNameA));
    char r[4], p[64], o[MAX_PATH]; vd_sysroot(r); sprintf(p, "%sWindows\\..\\..\\..\\x", r);
    CHECK(vd_gvpn_a(p, o, sizeof o)); EXPECT_EQ(vd_stricmp(o, r), 0);
}
TEST(vd_volpath_forward_slashes) {
    REQUIRE(U(GetVolumePathNameA));
    char r[4], p[64], o[MAX_PATH]; vd_sysroot(r); sprintf(p, "%c:/Windows/System32", r[0]);
    CHECK(vd_gvpn_a(p, o, sizeof o)); EXPECT_EQ(vd_stricmp(o, r), 0);
}
TEST(vd_volpath_long_nonexistent_path) {
    REQUIRE(U(GetVolumePathNameA));
    char r[4], p[600], o[MAX_PATH]; vd_sysroot(r); strcpy(p, r);
    for (int i = 0; i < 40; i++) strcat(p, "abcdefghij\\");
    if (vd_gvpn_a(p, o, sizeof o)) EXPECT_EQ(vd_stricmp(o, r), 0);
    else t_note("long nonexistent path rejected: %lu", GetLastError());
}
TEST(vd_volpath_extended_prefix) {
    REQUIRE(U(GetVolumePathNameW));
    char r[4]; vd_sysroot(r); WCHAR w[64], o[MAX_PATH]; swprintf(w, 64, L"\\\\?\\%c:\\Windows", r[0]);
    CHECK(U(GetVolumePathNameW)(w, o, MAX_PATH));
    char a[MAX_PATH]; vd_narrow(o, a, sizeof a); t_note("GetVolumePathName(\\\\?\\X:\\Windows) = %s", a);
    size_t n = strlen(a); CHECK(n >= 3 && !vd_stricmp(a + n - 3, r));
}
TEST(vd_volpath_guid_input) {
    REQUIRE(U(GetVolumePathNameA));
    char r[4]; vd_sysroot(r); char g[64], in[128], o[MAX_PATH];
    if (!vd_volname(r, g)) { t_skip("no volume name"); return; }
    sprintf(in, "%sWindows", g);
    CHECK(vd_gvpn_a(in, o, sizeof o));
    t_note("GetVolumePathName(%s) = %s", in, o);
    CHECK(strlen(o) >= 3);
}
TEST(vd_volpath_ab_parity) {
    REQUIRE(U(GetVolumePathNameA)); REQUIRE(U(GetVolumePathNameW));
    char r[4]; vd_sysroot(r); char in[64], o[MAX_PATH], wa[MAX_PATH]; WCHAR wi[64], wo[MAX_PATH];
    sprintf(in, "%sWindows\\notepad.exe", r); vd_widen(in, wi, 64);
    CHECK(vd_gvpn_a(in, o, sizeof o)); CHECK(U(GetVolumePathNameW)(wi, wo, MAX_PATH));
    vd_narrow(wo, wa, sizeof wa); EXPECT_STR(o, wa);
}
TEST(vd_volpath_null_arguments) {
    REQUIRE(U(GetVolumePathNameW)); REQUIRE(U(GetVolumePathNameA));
    WCHAR o[MAX_PATH];
    SET_ERR(0); CHECK(!U(GetVolumePathNameW)(NULL, o, MAX_PATH)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!U(GetVolumePathNameW)(L"C:\\", NULL, MAX_PATH)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!U(GetVolumePathNameW)(L"C:\\", o, 0)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!U(GetVolumePathNameW)(NULL, NULL, 0)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(vd_volpath_empty_input_fails) {
    REQUIRE(U(GetVolumePathNameW));
    WCHAR o[MAX_PATH]; o[0] = 0x7777; SET_ERR(0);
    CHECK(!U(GetVolumePathNameW)(L"", o, MAX_PATH));
    CHECK(GetLastError() != 0);
    t_note("empty input error %lu", GetLastError());
}
TEST(vd_volpath_buffer_exact_fit) {
    REQUIRE(U(GetVolumePathNameW));
    char r[4]; vd_sysroot(r); WCHAR w[4], o[16]; vd_widen(r, w, 4);
    for (int i = 0; i < 16; i++) o[i] = 0x7777;
    CHECK(U(GetVolumePathNameW)(w, o, 4));
    CHECK(!lstrcmpiW(o, w)); CHECK(o[4] == 0x7777);
}
TEST(vd_volpath_buffer_too_small_trailing_backslash_trimmed) {
    REQUIRE(U(GetVolumePathNameW));
    char r[4]; vd_sysroot(r); WCHAR w[4], o[16]; vd_widen(r, w, 4);
    for (int i = 0; i < 16; i++) o[i] = 0x7777;
    /* three characters of room: "C:\" does not fit with its NUL, so the backslash is dropped */
    SET_ERR(0);
    BOOL ok = U(GetVolumePathNameW)(w, o, 3);
    CHECK(ok);
    if (ok) { CHECK(o[0] == w[0] && o[1] == L':' && o[2] == 0); }
}
TEST(vd_volpath_buffer_too_small_fails) {
    REQUIRE(U(GetVolumePathNameW));
    char r[4]; vd_sysroot(r); WCHAR w[4], o[16]; vd_widen(r, w, 4);
    for (DWORD cch = 1; cch <= 2; cch++) {
        for (int i = 0; i < 16; i++) o[i] = 0x7777;
        SET_ERR(0);
        CHECKF(!U(GetVolumePathNameW)(w, o, cch), "cch=%lu", cch);
        EXPECT_ERR(ERROR_FILENAME_EXCED_RANGE);
        CHECK(o[0] == 0);   /* output is cleared before work starts */
    }
}
TEST(vd_volpath_a_buffer_too_small) {
    REQUIRE(U(GetVolumePathNameA));
    char o[8]; memset(o, 0x77, sizeof o); SET_ERR(0);
    CHECK(!U(GetVolumePathNameA)("C:\\Windows", o, 1));
    DWORD e = GetLastError();
    CHECKF(e == ERROR_FILENAME_EXCED_RANGE || e == ERROR_INSUFFICIENT_BUFFER, "err %lu", e);
}
TEST(vd_volpath_nonexistent_drive_note) {
    REQUIRE(U(GetVolumePathNameA));
    char l = vd_freeletter(); if (!l) { t_skip("no free letter"); return; }
    char p[16], o[MAX_PATH]; sprintf(p, "%c:\\foo\\bar.txt", l); o[0] = 'x'; SET_ERR(0);
    BOOL ok = vd_gvpn_a(p, o, sizeof o);
    t_note("GetVolumePathName(%s) ok=%d err=%lu out=\"%s\"", p, ok, GetLastError(), ok ? o : "");
}
TEST(vd_volpath_output_is_volume_root_for_getvolumeinformation) {
    REQUIRE(U(GetVolumePathNameA)); REQUIRE(U(GetVolumeInformationA));
    char p[MAX_PATH], o[MAX_PATH]; t_tmpfile(p, sizeof p, "anything.txt");
    CHECK(vd_gvpn_a(p, o, sizeof o));
    DWORD ser = 0; CHECK(U(GetVolumeInformationA)(o, NULL, 0, &ser, NULL, NULL, NULL, 0));
}
TEST(vd_volpath_idempotent) {
    REQUIRE(U(GetVolumePathNameA));
    char p[MAX_PATH], o1[MAX_PATH], o2[MAX_PATH]; t_tmpfile(p, sizeof p, "x.txt");
    CHECK(vd_gvpn_a(p, o1, sizeof o1)); CHECK(vd_gvpn_a(o1, o2, sizeof o2)); EXPECT_STR(o1, o2);
}

/* ---- GetVolumeNameForVolumeMountPoint ---- */
TEST(vd_volname_system_root_format) {
    REQUIRE(U(GetVolumeNameForVolumeMountPointA));
    char r[4]; vd_sysroot(r); char g[128] = "";
    CHECK(U(GetVolumeNameForVolumeMountPointA)(r, g, 50));
    CHECKF(vd_is_guid_name_a(g), "got \"%s\"", g);
}
TEST(vd_volname_w_format) {
    REQUIRE(U(GetVolumeNameForVolumeMountPointW));
    char r[4]; vd_sysroot(r); WCHAR w[4], g[64]; vd_widen(r, w, 4);
    CHECK(U(GetVolumeNameForVolumeMountPointW)(w, g, 64));
    CHECK(vd_is_guid_name_w(g));
}
TEST(vd_volname_ab_parity) {
    REQUIRE(U(GetVolumeNameForVolumeMountPointA)); REQUIRE(U(GetVolumeNameForVolumeMountPointW));
    char r[4]; vd_sysroot(r); WCHAR w[4], g[64]; char a[64], n[64]; vd_widen(r, w, 4);
    CHECK(U(GetVolumeNameForVolumeMountPointA)(r, a, 64)); CHECK(U(GetVolumeNameForVolumeMountPointW)(w, g, 64));
    vd_narrow(g, n, 64); EXPECT_STR(a, n);
}
TEST(vd_volname_exact_buffer_50) {
    REQUIRE(U(GetVolumeNameForVolumeMountPointA));
    char r[4]; vd_sysroot(r); char g[64]; memset(g, 0x77, sizeof g);
    CHECK(U(GetVolumeNameForVolumeMountPointA)(r, g, 50));
    EXPECT_EQ(strlen(g), VD_GUID_LEN); CHECK(g[50] == 0x77);
}
TEST(vd_volname_buffer_49_fails_and_clears) {
    REQUIRE(U(GetVolumeNameForVolumeMountPointA)); REQUIRE(U(GetVolumeNameForVolumeMountPointW));
    char r[4]; vd_sysroot(r); WCHAR w[4], g[64]; char a[64]; vd_widen(r, w, 4);
    memset(a, 0x77, sizeof a); for (int i = 0; i < 64; i++) g[i] = 0x7777;
    SET_ERR(0);
    CHECK(!U(GetVolumeNameForVolumeMountPointW)(w, g, 49));
    CHECK(GetLastError() != 0);
    CHECK(g[0] == 0);    /* failure clears first char */
    CHECK(!U(GetVolumeNameForVolumeMountPointA)(r, a, 49));
    CHECK(a[0] == 0);
}
TEST(vd_volname_small_buffers_fail) {
    REQUIRE(U(GetVolumeNameForVolumeMountPointA));
    char r[4]; vd_sysroot(r); char g[64];
    for (DWORD n = 1; n < 49; n += 7) { memset(g, 0x77, sizeof g); CHECKF(!U(GetVolumeNameForVolumeMountPointA)(r, g, n), "n=%lu", n); CHECK(g[0] == 0); }
}
TEST(vd_volname_invalid_arguments) {
    REQUIRE(U(GetVolumeNameForVolumeMountPointW)); REQUIRE(U(GetVolumeNameForVolumeMountPointA));
    if (t_is_wine()) { t_skip("NULL arguments fault under wine"); return; }
    WCHAR g[64] = { 0x7777 }; char a[64] = { 'x' };
    SET_ERR(0); CHECK(!U(GetVolumeNameForVolumeMountPointW)(NULL, g, 64)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!U(GetVolumeNameForVolumeMountPointW)(L"C:\\", NULL, 64)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!U(GetVolumeNameForVolumeMountPointW)(L"C:\\", g, 0)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CHECK(g[0] == 0x7777);   /* untouched on parameter failure */
}
TEST(vd_volname_no_trailing_backslash_fails) {
    REQUIRE(U(GetVolumeNameForVolumeMountPointA));
    char s[3] = { vd_sysletter(), ':', 0 }, g[64] = "x"; SET_ERR(0);
    CHECK(!U(GetVolumeNameForVolumeMountPointA)(s, g, 64));
    t_note("no-backslash error %lu", GetLastError());
    CHECK(g[0] == 0);
}
TEST(vd_volname_nonexistent_drive) {
    REQUIRE(U(GetVolumeNameForVolumeMountPointA));
    char l = vd_freeletter(); if (!l) { t_skip("no free letter"); return; }
    char p[4] = { l, ':', '\\', 0 }, g[64] = "x"; SET_ERR(0);
    CHECK(!U(GetVolumeNameForVolumeMountPointA)(p, g, 64));
    t_note("error %lu", GetLastError()); CHECK(g[0] == 0);
}
TEST(vd_volname_non_mountpoint_directory) {
    REQUIRE(U(GetVolumeNameForVolumeMountPointA));
    char d[MAX_PATH], g[64] = "x"; GetSystemDirectoryA(d, sizeof d); strcat(d, "\\"); SET_ERR(0);
    CHECK(!U(GetVolumeNameForVolumeMountPointA)(d, g, 64));
    t_note("non mount point error %lu", GetLastError()); CHECK(g[0] == 0);
}
TEST(vd_volname_lowercase_drive_same_name) {
    REQUIRE(U(GetVolumeNameForVolumeMountPointA));
    char r[4]; vd_sysroot(r); char l[4] = { (char)tolower(r[0]), ':', '\\', 0 }, a[64], b[64];
    CHECK(U(GetVolumeNameForVolumeMountPointA)(r, a, 64)); CHECK(U(GetVolumeNameForVolumeMountPointA)(l, b, 64));
    EXPECT_EQ(vd_stricmp(a, b), 0);
}
TEST(vd_volname_dosdevice_prefix) {
    REQUIRE(U(GetVolumeNameForVolumeMountPointA));
    char r[4]; vd_sysroot(r); char p[16], a[64], b[64]; sprintf(p, "\\\\?\\%c:\\", r[0]);
    CHECK(U(GetVolumeNameForVolumeMountPointA)(r, a, 64));
    if (U(GetVolumeNameForVolumeMountPointA)(p, b, 64)) EXPECT_STR(a, b); else t_note("\\\\?\\ prefix unsupported: %lu", GetLastError());
}
TEST(vd_volname_stable_repeated) {
    REQUIRE(U(GetVolumeNameForVolumeMountPointA));
    char r[4]; vd_sysroot(r); char a[64], b[64]; CHECK(U(GetVolumeNameForVolumeMountPointA)(r, a, 64));
    for (int i = 0; i < 50; i++) { CHECK(U(GetVolumeNameForVolumeMountPointA)(r, b, 64)); if (strcmp(a, b)) { CHECK(0); break; } }
}
TEST(vd_volname_distinct_per_fixed_drive) {
    REQUIRE(U(GetVolumeNameForVolumeMountPointA));
    DWORD m = vd_fixed_mask(); char names[26][64]; int n = 0;
    for (int i = 0; i < 26; i++) {
        if (!((m >> i) & 1)) continue;
        char r[4] = { (char)('A' + i), ':', '\\', 0 };
        if (!U(GetVolumeNameForVolumeMountPointA)(r, names[n], 64)) continue;   /* subst drives have no volume name */
        for (int k = 0; k < n; k++) if (!strcmp(names[k], names[n])) { t_note("drives share a volume (subst/mount): %s", r); }
        n++;
    }
    CHECK(n >= 1);
}

/* ---- GetVolumePathNamesForVolumeName ---- */
static int vd_msz_total(const WCHAR *p) { int n = 0; while (*p) { int l = lstrlenW(p) + 1; n += l; p += l; } return n + 1; }
TEST(vd_volnames_system_volume_contains_root) {
    REQUIRE(U(GetVolumePathNamesForVolumeNameW));
    char r[4]; vd_sysroot(r); char g[64]; if (!vd_volname(r, g)) { t_skip("no volume name"); return; }
    WCHAR wg[64], buf[1024], wr[4]; vd_widen(g, wg, 64); vd_widen(r, wr, 4); DWORD ret = 0; int found = 0;
    CHECK(U(GetVolumePathNamesForVolumeNameW)(wg, buf, 1024, &ret));
    for (const WCHAR *p = buf; *p; p += lstrlenW(p) + 1) if (!lstrcmpiW(p, wr)) found = 1;
    CHECK(found);
    EXPECT_EQ(ret, vd_msz_total(buf));
}
TEST(vd_volnames_a_parity) {
    REQUIRE(U(GetVolumePathNamesForVolumeNameA)); REQUIRE(U(GetVolumePathNamesForVolumeNameW));
    char r[4]; vd_sysroot(r); char g[64]; if (!vd_volname(r, g)) { t_skip("no volume name"); return; }
    char ab[1024]; WCHAR wb[1024], wg[64]; DWORD ra = 0, rw = 0; vd_widen(g, wg, 64);
    CHECK(U(GetVolumePathNamesForVolumeNameA)(g, ab, 1024, &ra)); CHECK(U(GetVolumePathNamesForVolumeNameW)(wg, wb, 1024, &rw));
    EXPECT_EQ(ra, rw);
    int n = 0; for (const char *p = ab; *p; p += strlen(p) + 1) n++;
    int m = 0; for (const WCHAR *p = wb; *p; p += lstrlenW(p) + 1) m++;
    EXPECT_EQ(n, m);
}
TEST(vd_volnames_size_query_more_data) {
    REQUIRE(U(GetVolumePathNamesForVolumeNameW));
    char r[4]; vd_sysroot(r); char g[64]; if (!vd_volname(r, g)) { t_skip("no volume name"); return; }
    WCHAR wg[64], buf[1024]; vd_widen(g, wg, 64); DWORD need = 0, ret = 0;
    SET_ERR(0);
    CHECK(!U(GetVolumePathNamesForVolumeNameW)(wg, NULL, 0, &need));
    EXPECT_ERR(ERROR_MORE_DATA);
    CHECK(need >= 5);       /* "X:\" NUL + terminating NUL */
    CHECK(U(GetVolumePathNamesForVolumeNameW)(wg, buf, need, &ret));
    EXPECT_EQ(ret, need);
    SET_ERR(0);
    CHECK(!U(GetVolumePathNamesForVolumeNameW)(wg, buf, need - 1, &ret));
    EXPECT_ERR(ERROR_MORE_DATA); EXPECT_EQ(ret, need);
}
TEST(vd_volnames_tiny_buffer_more_data) {
    REQUIRE(U(GetVolumePathNamesForVolumeNameW));
    char r[4]; vd_sysroot(r); char g[64]; if (!vd_volname(r, g)) { t_skip("no volume name"); return; }
    WCHAR wg[64], buf[8]; vd_widen(g, wg, 64); DWORD ret = 0;
    for (DWORD n = 0; n <= 3; n++) { SET_ERR(0); CHECKF(!U(GetVolumePathNamesForVolumeNameW)(wg, buf, n, &ret), "n=%lu", n); EXPECT_ERR(ERROR_MORE_DATA); CHECK(ret >= 5); }
}
TEST(vd_volnames_null_return_length_ok) {
    REQUIRE(U(GetVolumePathNamesForVolumeNameW));
    char r[4]; vd_sysroot(r); char g[64]; if (!vd_volname(r, g)) { t_skip("no volume name"); return; }
    WCHAR wg[64], buf[1024]; vd_widen(g, wg, 64);
    CHECK(U(GetVolumePathNamesForVolumeNameW)(wg, buf, 1024, NULL));
    SET_ERR(0); CHECK(!U(GetVolumePathNamesForVolumeNameW)(wg, buf, 1, NULL)); EXPECT_ERR(ERROR_MORE_DATA);
}
TEST(vd_volnames_double_nul_terminated) {
    REQUIRE(U(GetVolumePathNamesForVolumeNameW));
    char r[4]; vd_sysroot(r); char g[64]; if (!vd_volname(r, g)) { t_skip("no volume name"); return; }
    WCHAR wg[64], buf[1024]; vd_widen(g, wg, 64); DWORD ret = 0;
    for (int i = 0; i < 1024; i++) buf[i] = 0x7777;
    CHECK(U(GetVolumePathNamesForVolumeNameW)(wg, buf, 1024, &ret));
    CHECK(ret >= 5 && ret < 1024);
    CHECK(buf[ret - 1] == 0 && buf[ret - 2] == 0);
}
TEST(vd_volnames_every_path_ends_with_backslash) {
    REQUIRE(U(GetVolumePathNamesForVolumeNameW));
    char r[4]; vd_sysroot(r); char g[64]; if (!vd_volname(r, g)) { t_skip("no volume name"); return; }
    WCHAR wg[64], buf[2048]; vd_widen(g, wg, 64); DWORD ret = 0;
    CHECK(U(GetVolumePathNamesForVolumeNameW)(wg, buf, 2048, &ret));
    for (const WCHAR *p = buf; *p; p += lstrlenW(p) + 1) CHECK(p[lstrlenW(p) - 1] == L'\\');
}
TEST(vd_volnames_trailing_backslash_required) {
    REQUIRE(U(GetVolumePathNamesForVolumeNameW));
    char r[4]; vd_sysroot(r); char g[64]; if (!vd_volname(r, g)) { t_skip("no volume name"); return; }
    WCHAR wg[64], buf[256]; DWORD ret = 0; vd_widen(g, wg, 64); wg[VD_GUID_LEN - 1] = 0;
    SET_ERR(0); CHECK(!U(GetVolumePathNamesForVolumeNameW)(wg, buf, 256, &ret));
    CHECK(GetLastError() != 0);
    t_note("no trailing backslash error %lu", GetLastError());
}
TEST(vd_volnames_non_guid_name_rejected) {
    REQUIRE(U(GetVolumePathNamesForVolumeNameW));
    WCHAR buf[256]; DWORD ret = 0; static const WCHAR *const bad[] = { L"C:\\", L"\\\\?\\C:\\", L"\\\\?\\Volume{zzzz}\\", L"\\\\?\\Volume{00000000-0000-0000-0000-000000000000}" };
    for (size_t i = 0; i < ARRAYSIZE(bad); i++) { SET_ERR(0); CHECKF(!U(GetVolumePathNamesForVolumeNameW)(bad[i], buf, 256, &ret), "case %zu", i); CHECK(GetLastError() != 0); }
}
TEST(vd_volnames_unknown_guid) {
    REQUIRE(U(GetVolumePathNamesForVolumeNameW));
    WCHAR buf[256]; DWORD ret = 0xCAFE; SET_ERR(0);
    BOOL ok = U(GetVolumePathNamesForVolumeNameW)(L"\\\\?\\Volume{00000000-0000-0000-0000-000000000000}\\", buf, 256, &ret);
    t_note("unknown guid ok=%d err=%lu ret=%lu", ok, GetLastError(), ret);
    if (ok) CHECK(buf[0] == 0);
}
TEST(vd_volnames_consistent_with_volname_roundtrip) {
    REQUIRE(U(GetVolumePathNamesForVolumeNameA)); REQUIRE(U(GetVolumeNameForVolumeMountPointA));
    DWORD m = vd_fixed_mask(); int checked = 0;
    for (int i = 0; i < 26; i++) {
        if (!((m >> i) & 1)) continue;
        char r[4] = { (char)('A' + i), ':', '\\', 0 }, g[64], buf[1024], again[64]; DWORD ret;
        if (!U(GetVolumeNameForVolumeMountPointA)(r, g, 64)) continue;
        if (!U(GetVolumePathNamesForVolumeNameA)(g, buf, sizeof buf, &ret)) continue;
        int found = 0; for (const char *p = buf; *p; p += strlen(p) + 1) if (!vd_stricmp(p, r)) { found = 1; if (U(GetVolumeNameForVolumeMountPointA)(p, again, 64)) EXPECT_STR(again, g); }
        CHECKF(found, "%s missing from its own volume's path list", r); checked++;
    }
    CHECK(checked >= 1);
}
