/* CreateDirectory{A,W,ExA,ExW} */
#include "dh.h"
#include <sddl.h>
#include <aclapi.h>

static char *sub(char *buf, size_t cap, const char *dir, const char *leaf) { dh_path(buf, cap, dir, leaf); return buf; }

TEST(dir_create_a_basic) {
    REQUIRE(U(CreateDirectoryA));
    char *d = dh_dir("cr_a"), p[MAX_PATH];
    sub(p, sizeof p, d, "one");
    SET_ERR(0xdeadbeef);
    CHECK(U(CreateDirectoryA)(p, NULL));
    CHECK(dh_isdir(p));
    CHECK(!U(CreateDirectoryA)(p, NULL));
    EXPECT_ERR(ERROR_ALREADY_EXISTS);
}
TEST(dir_create_w_basic) {
    REQUIRE(U(CreateDirectoryW));
    char *d = dh_dir("cr_w"), p[MAX_PATH]; WCHAR w[MAX_PATH];
    sub(p, sizeof p, d, "one"); dh_w(w, MAX_PATH, p);
    CHECK(U(CreateDirectoryW)(w, NULL));
    CHECK(dh_isdir(p));
    CHECK(!U(CreateDirectoryW)(w, NULL));
    EXPECT_ERR(ERROR_ALREADY_EXISTS);
}
TEST(dir_create_exa_basic) {
    REQUIRE(U(CreateDirectoryExA));
    char *d = dh_dir("cr_exa"), tpl[MAX_PATH], p[MAX_PATH];
    sub(tpl, sizeof tpl, d, "tpl"); CreateDirectoryA(tpl, NULL);
    sub(p, sizeof p, d, "new");
    CHECK(U(CreateDirectoryExA)(tpl, p, NULL));
    CHECK(dh_isdir(p));
    CHECK(!U(CreateDirectoryExA)(tpl, p, NULL));
    EXPECT_ERR(ERROR_ALREADY_EXISTS);
}
TEST(dir_create_exw_basic) {
    REQUIRE(U(CreateDirectoryExW));
    char *d = dh_dir("cr_exw"), tpl[MAX_PATH], p[MAX_PATH]; WCHAR wt[MAX_PATH], wp[MAX_PATH];
    sub(tpl, sizeof tpl, d, "tpl"); CreateDirectoryA(tpl, NULL);
    sub(p, sizeof p, d, "new");
    dh_w(wt, MAX_PATH, tpl); dh_w(wp, MAX_PATH, p);
    CHECK(U(CreateDirectoryExW)(wt, wp, NULL));
    CHECK(dh_isdir(p));
    CHECK(!U(CreateDirectoryExW)(wt, wp, NULL));
    EXPECT_ERR(ERROR_ALREADY_EXISTS);
}
TEST(dir_create_ex_template_attributes) {
    REQUIRE(U(CreateDirectoryExA));
    char *d = dh_dir("cr_extpl"), tpl[MAX_PATH], p[MAX_PATH];
    sub(tpl, sizeof tpl, d, "tpl"); CreateDirectoryA(tpl, NULL);
    SetFileAttributesA(tpl, FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_HIDDEN);
    sub(p, sizeof p, d, "new");
    CHECK(U(CreateDirectoryExA)(tpl, p, NULL));
    DWORD a = GetFileAttributesA(p);
    CHECK(a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY));
    t_note("CreateDirectoryEx: template hidden -> new attrs 0x%lx", a);
    SetFileAttributesA(tpl, FILE_ATTRIBUTE_DIRECTORY);
}
TEST(dir_create_ex_template_missing) {
    REQUIRE(U(CreateDirectoryExA));
    char *d = dh_dir("cr_extplmiss"), tpl[MAX_PATH], p[MAX_PATH];
    sub(tpl, sizeof tpl, d, "nosuch"); sub(p, sizeof p, d, "new");
    SET_ERR(0);
    CHECK(!U(CreateDirectoryExA)(tpl, p, NULL));
    DWORD e = GetLastError();
    CHECKF(e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND, "err %lu", e);
    CHECK(!dh_exists(p));
}
TEST(dir_create_ex_template_is_file) {
    REQUIRE(U(CreateDirectoryExA));
    char *d = dh_dir("cr_extplfile"), tpl[MAX_PATH], p[MAX_PATH];
    dh_mkfile(d, "f", 1); sub(tpl, sizeof tpl, d, "f"); sub(p, sizeof p, d, "new");
    SET_ERR(0);
    BOOL ok = U(CreateDirectoryExA)(tpl, p, NULL);
    DWORD e = GetLastError();
    t_note("CreateDirectoryEx template is a file: ok=%d err=%lu", ok, e);
    if (ok) RemoveDirectoryA(p);
}
TEST(dir_create_ex_template_parent_missing) {
    REQUIRE(U(CreateDirectoryExA));
    char *d = dh_dir("cr_extplpar"), tpl[MAX_PATH], p[MAX_PATH];
    sub(tpl, sizeof tpl, d, "tpl"); CreateDirectoryA(tpl, NULL);
    sub(p, sizeof p, d, "nosuch\\new");
    CHECK(!U(CreateDirectoryExA)(tpl, p, NULL));
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(dir_create_missing_parent_a) {
    REQUIRE(U(CreateDirectoryA));
    char *d = dh_dir("cr_nest_a"), p[MAX_PATH];
    sub(p, sizeof p, d, "a\\b\\c");
    CHECK(!U(CreateDirectoryA)(p, NULL));
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    sub(p, sizeof p, d, "a");
    CHECK(!dh_exists(p));
}
TEST(dir_create_missing_parent_w) {
    REQUIRE(U(CreateDirectoryW));
    char *d = dh_dir("cr_nest_w"), p[MAX_PATH]; WCHAR w[MAX_PATH];
    sub(p, sizeof p, d, "a\\b"); dh_w(w, MAX_PATH, p);
    CHECK(!U(CreateDirectoryW)(w, NULL));
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(dir_create_parent_is_file) {
    REQUIRE(U(CreateDirectoryA));
    char *d = dh_dir("cr_parfile"), p[MAX_PATH];
    dh_mkfile(d, "f", 1);
    sub(p, sizeof p, d, "f\\sub");
    SET_ERR(0);
    CHECK(!U(CreateDirectoryA)(p, NULL));
    DWORD e = GetLastError();
    CHECKF(e == ERROR_PATH_NOT_FOUND || e == ERROR_DIRECTORY, "err %lu", e);
    t_note("create under a file: err %lu", e);
}
TEST(dir_create_nested_stepwise) {
    REQUIRE(U(CreateDirectoryA));
    char *d = dh_dir("cr_step"), p[MAX_PATH];
    sub(p, sizeof p, d, "a"); CHECK(U(CreateDirectoryA)(p, NULL));
    sub(p, sizeof p, d, "a\\b"); CHECK(U(CreateDirectoryA)(p, NULL));
    sub(p, sizeof p, d, "a\\b\\c"); CHECK(U(CreateDirectoryA)(p, NULL));
    CHECK(dh_isdir(p));
}
TEST(dir_create_existing_file_name) {
    REQUIRE(U(CreateDirectoryA));
    char *d = dh_dir("cr_exfile"), p[MAX_PATH];
    dh_mkfile(d, "f", 1); sub(p, sizeof p, d, "f");
    CHECK(!U(CreateDirectoryA)(p, NULL));
    EXPECT_ERR(ERROR_ALREADY_EXISTS);
    CHECK(!dh_isdir(p));
}
TEST(dir_create_existing_case_insensitive) {
    REQUIRE(U(CreateDirectoryA));
    char *d = dh_dir("cr_case"), p[MAX_PATH];
    sub(p, sizeof p, d, "AbC"); CHECK(U(CreateDirectoryA)(p, NULL));
    sub(p, sizeof p, d, "aBc");
    CHECK(!U(CreateDirectoryA)(p, NULL));
    EXPECT_ERR(ERROR_ALREADY_EXISTS);
    dh_name l[8];
    sub(p, sizeof p, d, "*");
    int n = dh_listA_nd(p, l, 8);
    EXPECT_EQ(n, 1);
    CHECK(dh_has(l, n, "AbC"));   /* creation case preserved */
}
TEST(dir_create_existing_case_file_vs_dir) {
    REQUIRE(U(CreateDirectoryA));
    char *d = dh_dir("cr_casefd"), p[MAX_PATH];
    dh_mkfile(d, "Name", 1);
    sub(p, sizeof p, d, "NAME");
    CHECK(!U(CreateDirectoryA)(p, NULL));
    EXPECT_ERR(ERROR_ALREADY_EXISTS);
}
TEST(dir_create_empty_string) {
    REQUIRE(U(CreateDirectoryA)); REQUIRE(U(CreateDirectoryW));
    SET_ERR(0);
    CHECK(!U(CreateDirectoryA)("", NULL));
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    SET_ERR(0);
    CHECK(!U(CreateDirectoryW)(L"", NULL));
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(dir_create_null_name) {
    REQUIRE(U(CreateDirectoryW));
    SET_ERR(0);
    CHECK(!U(CreateDirectoryW)(NULL, NULL));
    t_note("CreateDirectoryW(NULL) err=%lu", GetLastError());
}
TEST(dir_create_null_name_a) {
    REQUIRE(U(CreateDirectoryA));
    SET_ERR(0);
    CHECK(!U(CreateDirectoryA)(NULL, NULL));
    t_note("CreateDirectoryA(NULL) err=%lu", GetLastError());
}
TEST(dir_create_invalid_chars) {
    REQUIRE(U(CreateDirectoryW));
    char *d = dh_dir("cr_badchar"), p[MAX_PATH]; WCHAR w[MAX_PATH];
    const char *bad[] = { "a*b", "a?b", "a<b", "a>b", "a|b", "a\"b", "*", "?", "a\x01" "b", "a\x1f" "b" };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        sub(p, sizeof p, d, bad[i]); dh_w(w, MAX_PATH, p);
        SET_ERR(0);
        BOOL ok = U(CreateDirectoryW)(w, NULL);
        CHECKF(!ok, "name #%u created", i);
        if (!ok) { DWORD e = GetLastError(); CHECKF(e == ERROR_INVALID_NAME, "name #%u err %lu", i, e); }
        else RemoveDirectoryW(w);
    }
}
TEST(dir_create_invalid_chars_a) {
    REQUIRE(U(CreateDirectoryA));
    char *d = dh_dir("cr_badchar_a"), p[MAX_PATH];
    const char *bad[] = { "x*y", "x?y", "x<y", "x>y", "x|y", "x\"y" };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        sub(p, sizeof p, d, bad[i]);
        SET_ERR(0);
        CHECKF(!U(CreateDirectoryA)(p, NULL), "name #%u", i);
        EXPECT_ERR(ERROR_INVALID_NAME);
    }
}
TEST(dir_create_colon_in_name) {
    REQUIRE(U(CreateDirectoryA));
    char *d = dh_dir("cr_colon"), p[MAX_PATH];
    sub(p, sizeof p, d, "a:b");
    SET_ERR(0);
    BOOL ok = U(CreateDirectoryA)(p, NULL);
    t_note("CreateDirectory 'a:b' ok=%d err=%lu", ok, GetLastError());
    if (ok) RemoveDirectoryA(p);
}
TEST(dir_create_trailing_space_dot_w) {
    REQUIRE(U(CreateDirectoryW));
    char *d = dh_dir("cr_trail"), p[MAX_PATH]; WCHAR w[MAX_PATH];
    const char *names[] = { "ts1. ", "ts2...", "ts3  ", "ts4. . " };
    const char *norm[]  = { "ts1", "ts2", "ts3", "ts4" };
    for (int i = 0; i < 4; i++) {
        sub(p, sizeof p, d, names[i]); dh_w(w, MAX_PATH, p);
        CHECKF(U(CreateDirectoryW)(w, NULL), "i=%d err %lu", i, GetLastError());
        sub(p, sizeof p, d, norm[i]);
        CHECKF(dh_isdir(p), "normalised %s", norm[i]);
    }
    dh_name l[16];
    sub(p, sizeof p, d, "*");
    int n = dh_listA_nd(p, l, 16);
    EXPECT_EQ(n, 4);
    for (int i = 0; i < 4; i++) CHECKF(dh_has(l, n, norm[i]), "listed %s", norm[i]);
}
TEST(dir_create_trailing_dot_then_exists) {
    REQUIRE(U(CreateDirectoryA));
    char *d = dh_dir("cr_trail2"), p[MAX_PATH];
    sub(p, sizeof p, d, "same"); CHECK(U(CreateDirectoryA)(p, NULL));
    sub(p, sizeof p, d, "same.");
    CHECK(!U(CreateDirectoryA)(p, NULL));
    EXPECT_ERR(ERROR_ALREADY_EXISTS);
    sub(p, sizeof p, d, "same ");
    CHECK(!U(CreateDirectoryA)(p, NULL));
    EXPECT_ERR(ERROR_ALREADY_EXISTS);
}
TEST(dir_create_raw_prefix_trailing_dot) {
    REQUIRE(U(CreateDirectoryW)); REQUIRE(U(RemoveDirectoryW));
    char *d = dh_dir("cr_rawdot"), p[MAX_PATH]; WCHAR w[MAX_PATH + 8], raw[MAX_PATH + 8];
    sub(p, sizeof p, d, "rd."); dh_w(w, MAX_PATH, p);
    snwprintf(raw, MAX_PATH + 8, L"\\\\?\\%ls", w);
    if (!U(CreateDirectoryW)(raw, NULL)) { t_note("raw trailing dot create failed %lu", GetLastError()); return; }
    dh_name l[8]; sub(p, sizeof p, d, "*");
    int n = dh_listA_nd(p, l, 8);
    EXPECT_EQ(n, 1);
    CHECK(dh_has(l, n, "rd."));       /* \\?\ disables normalisation */
    sub(p, sizeof p, d, "rd");
    CHECK(!dh_exists(p));
    CHECK(U(RemoveDirectoryW)(raw));
}
TEST(dir_create_leading_space_kept) {
    REQUIRE(U(CreateDirectoryA));
    char *d = dh_dir("cr_lead"), p[MAX_PATH];
    sub(p, sizeof p, d, " lead");
    CHECK(U(CreateDirectoryA)(p, NULL));
    dh_name l[8]; sub(p, sizeof p, d, "*");
    int n = dh_listA_nd(p, l, 8);
    EXPECT_EQ(n, 1);
    CHECK(dh_has(l, n, " lead"));
}
TEST(dir_create_embedded_spaces) {
    REQUIRE(U(CreateDirectoryA));
    char *d = dh_dir("cr_spaces"), p[MAX_PATH];
    sub(p, sizeof p, d, "a b  c"); CHECK(U(CreateDirectoryA)(p, NULL));
    CHECK(dh_isdir(p));
    sub(p, sizeof p, d, "a b  c\\x y"); CHECK(U(CreateDirectoryA)(p, NULL));
}
TEST(dir_create_trailing_separator) {
    REQUIRE(U(CreateDirectoryA));
    char *d = dh_dir("cr_sep"), p[MAX_PATH];
    sub(p, sizeof p, d, "tb\\"); CHECK(U(CreateDirectoryA)(p, NULL));
    sub(p, sizeof p, d, "tb"); CHECK(dh_isdir(p));
}
TEST(dir_create_forward_slashes) {
    REQUIRE(U(CreateDirectoryA));
    char *d = dh_dir("cr_fwd"), p[MAX_PATH], q[MAX_PATH];
    snprintf(p, sizeof p, "%s/fs", d);
    CHECK(U(CreateDirectoryA)(p, NULL));
    sub(q, sizeof q, d, "fs"); CHECK(dh_isdir(q));
    snprintf(p, sizeof p, "%s/fs/in", d);
    CHECK(U(CreateDirectoryA)(p, NULL));
}
TEST(dir_create_doubled_separators) {
    REQUIRE(U(CreateDirectoryA));
    char *d = dh_dir("cr_dbl"), p[MAX_PATH];
    sub(p, sizeof p, d, "x"); CreateDirectoryA(p, NULL);
    snprintf(p, sizeof p, "%s\\\\x\\\\y", d);
    CHECK(U(CreateDirectoryA)(p, NULL));
    sub(p, sizeof p, d, "x\\y"); CHECK(dh_isdir(p));
}
TEST(dir_create_dot_components) {
    REQUIRE(U(CreateDirectoryA));
    char *d = dh_dir("cr_dots"), p[MAX_PATH];
    sub(p, sizeof p, d, "x"); CreateDirectoryA(p, NULL);
    snprintf(p, sizeof p, "%s\\x\\.\\y", d);
    CHECK(U(CreateDirectoryA)(p, NULL));
    snprintf(p, sizeof p, "%s\\x\\y\\..\\z", d);
    CHECK(U(CreateDirectoryA)(p, NULL));
    sub(p, sizeof p, d, "x\\z"); CHECK(dh_isdir(p));
}
TEST(dir_create_dot_names) {
    REQUIRE(U(CreateDirectoryA));
    char *d = dh_dir("cr_dotnames"), p[MAX_PATH];
    sub(p, sizeof p, d, "x"); CreateDirectoryA(p, NULL);
    snprintf(p, sizeof p, "%s\\x\\.", d);
    CHECK(!U(CreateDirectoryA)(p, NULL));
    EXPECT_ERR(ERROR_ALREADY_EXISTS);
    snprintf(p, sizeof p, "%s\\x\\..", d);
    CHECK(!U(CreateDirectoryA)(p, NULL));
    EXPECT_ERR(ERROR_ALREADY_EXISTS);
}
TEST(dir_create_reserved_device_name) {
    REQUIRE(U(CreateDirectoryW));
    char *d = dh_dir("cr_dev"), p[MAX_PATH]; WCHAR w[MAX_PATH];
    const char *n[] = { "NUL", "nul", "CON", "COM1", "LPT1", "AUX", "PRN" };
    for (unsigned i = 0; i < sizeof n / sizeof *n; i++) {
        sub(p, sizeof p, d, n[i]); dh_w(w, MAX_PATH, p);
        SET_ERR(0);
        BOOL ok = U(CreateDirectoryW)(w, NULL);
        if (t_is_wine() && ok) { RemoveDirectoryW(w); continue; }
        CHECKF(!ok, "device %s", n[i]);
        if (!ok) { DWORD e = GetLastError(); if (!t_is_wine()) CHECKF(e == ERROR_DIRECTORY, "%s err %lu", n[i], e); t_note("CreateDirectory(%s) err=%lu", n[i], e); }
    }
}
TEST(dir_create_nul_exact_error) {
    REQUIRE(U(CreateDirectoryW));
    char *d = dh_dir("cr_nul"), p[MAX_PATH]; WCHAR w[MAX_PATH];
    sub(p, sizeof p, d, "NUL"); dh_w(w, MAX_PATH, p);
    if (t_is_wine()) { t_skip("wine creates a dir named NUL"); return; }
    CHECK(!U(CreateDirectoryW)(w, NULL));
    EXPECT_ERR(ERROR_DIRECTORY);   /* kernelbase maps failures on DOS device names to STATUS_NOT_A_DIRECTORY */
}
TEST(dir_create_component_255) {
    REQUIRE(U(CreateDirectoryW)); REQUIRE(U(RemoveDirectoryW));
    char *d = dh_dir("cr_c255"); WCHAR dw[MAX_PATH], w[MAX_PATH * 2];
    dh_w(dw, MAX_PATH, d);
    int k = snwprintf(w, MAX_PATH * 2, L"\\\\?\\%ls\\", dw);
    for (int i = 0; i < 255; i++) w[k + i] = 'n';
    w[k + 255] = 0;
    SET_ERR(0);
    BOOL ok = U(CreateDirectoryW)(w, NULL);
    CHECKF(ok, "err %lu", GetLastError());
    if (!ok) return;
    CHECK(GetFileAttributesW(w) & FILE_ATTRIBUTE_DIRECTORY);
    dh_name l[4]; char pat[MAX_PATH]; snprintf(pat, sizeof pat, "%s\\*", d);
    int n = dh_listA_nd(pat, l, 4);
    EXPECT_EQ(n, 1);
    CHECK(n == 1 && strlen(l[0].n) == 255);
    CHECK(U(RemoveDirectoryW)(w));
}
TEST(dir_create_component_256) {
    REQUIRE(U(CreateDirectoryW));
    char *d = dh_dir("cr_c256"); WCHAR w[MAX_PATH * 2], dw[MAX_PATH];
    dh_w(dw, MAX_PATH, d);
    int k = snwprintf(w, MAX_PATH * 2, L"\\\\?\\%ls\\", dw);
    for (int i = 0; i < 256; i++) w[k + i] = 'm';
    w[k + 256] = 0;
    SET_ERR(0);
    BOOL ok = U(CreateDirectoryW)(w, NULL);
    CHECK(!ok);
    if (!ok) { DWORD e = GetLastError(); t_note("256-char component err %lu", e); CHECKF(e == ERROR_INVALID_NAME || e == ERROR_FILENAME_EXCED_RANGE || e == ERROR_BAD_PATHNAME, "err %lu", e); }
    else RemoveDirectoryW(w);
}
static void make_len_path(const char *d, int total, char *out, size_t cap) {
    /* d + "\" + leaf of 'p' chars so that strlen == total */
    int pre = (int)strlen(d) + 1;
    int n = total - pre;
    if (n < 1) n = 1;
    if (n > 255) n = 255;
    snprintf(out, cap, "%s\\", d);
    memset(out + pre, 'p', n); out[pre + n] = 0;
}
TEST(dir_create_length_247_ok) {
    REQUIRE(U(CreateDirectoryW)); REQUIRE(U(RemoveDirectoryW));
    char *d = dh_dir("cr_l247"), p[1024]; WCHAR w[1024];
    make_len_path(d, 247, p, sizeof p);
    if ((int)strlen(p) != 247) { t_skip("tmp prefix too long"); return; }
    dh_w(w, 1024, p);
    SET_ERR(0);
    BOOL ok = U(CreateDirectoryW)(w, NULL);
    CHECKF(ok, "err %lu", GetLastError());
    if (ok) CHECK(U(RemoveDirectoryW)(w));
}
TEST(dir_create_length_248_range) {
    REQUIRE(U(CreateDirectoryW));
    char *d = dh_dir("cr_l248"), p[1024]; WCHAR w[1024];
    make_len_path(d, 248, p, sizeof p);
    if ((int)strlen(p) != 248) { t_skip("tmp prefix too long"); return; }
    dh_w(w, 1024, p);
    SET_ERR(0);
    BOOL ok = U(CreateDirectoryW)(w, NULL);
    CHECK(!ok);                                /* non-long-path-aware process: 247 char directory limit */
    EXPECT_ERR(ERROR_FILENAME_EXCED_RANGE);
    if (ok) RemoveDirectoryW(w);
}
TEST(dir_create_length_a_248_range) {
    REQUIRE(U(CreateDirectoryA));
    char *d = dh_dir("cr_l248a"), p[1024];
    make_len_path(d, 248, p, sizeof p);
    if ((int)strlen(p) != 248) { t_skip("tmp prefix too long"); return; }
    SET_ERR(0);
    BOOL ok = U(CreateDirectoryA)(p, NULL);
    CHECK(!ok);
    EXPECT_ERR(ERROR_FILENAME_EXCED_RANGE);
    if (ok) RemoveDirectoryA(p);
}
TEST(dir_create_length_over_max_path) {
    REQUIRE(U(CreateDirectoryA));
    char *d = dh_dir("cr_l300"), p[1024];
    snprintf(p, sizeof p, "%s\\", d);
    int k = (int)strlen(p);
    for (int i = 0; i < 4; i++) { memset(p + k, 'q', 90); p[k + 90] = 0; strcat(p, "\\"); k = (int)strlen(p); }
    p[k - 1] = 0;
    SET_ERR(0);
    CHECK(!U(CreateDirectoryA)(p, NULL));
    DWORD e = GetLastError();
    CHECKF(e == ERROR_FILENAME_EXCED_RANGE, "err %lu", e);
}
TEST(dir_create_raw_long_nested) {
    REQUIRE(U(CreateDirectoryW)); REQUIRE(U(RemoveDirectoryW));
    char *d = dh_dir("cr_rawlong"); WCHAR dw[MAX_PATH], w[2048];
    dh_w(dw, MAX_PATH, d);
    int k = snwprintf(w, 2048, L"\\\\?\\%ls", dw);
    int marks[8], nm = 0;
    for (int i = 0; i < 5; i++) {
        w[k++] = '\\';
        for (int j = 0; j < 100; j++) w[k++] = 'a' + i;
        w[k] = 0; marks[nm++] = k;
        if (!U(CreateDirectoryW)(w, NULL)) { CHECKF(0, "level %d err %lu", i, GetLastError()); break; }
    }
    CHECK(GetFileAttributesW(w) != INVALID_FILE_ATTRIBUTES);
    CHECK(k > 500);
    for (int i = nm - 1; i >= 0; i--) { w[marks[i]] = 0; U(RemoveDirectoryW)(w); }
}
TEST(dir_create_raw_prefix_basic) {
    REQUIRE(U(CreateDirectoryW));
    char *d = dh_dir("cr_raw"), p[MAX_PATH]; WCHAR w[MAX_PATH], r[MAX_PATH + 8];
    sub(p, sizeof p, d, "viaraw"); dh_w(w, MAX_PATH, p);
    snwprintf(r, MAX_PATH + 8, L"\\\\?\\%ls", w);
    CHECK(U(CreateDirectoryW)(r, NULL));
    CHECK(dh_isdir(p));
    CHECK(!U(CreateDirectoryW)(r, NULL));
    EXPECT_ERR(ERROR_ALREADY_EXISTS);
}
TEST(dir_create_dot_device_prefix) {
    REQUIRE(U(CreateDirectoryW));
    char *d = dh_dir("cr_dotprefix"), p[MAX_PATH]; WCHAR w[MAX_PATH], r[MAX_PATH + 8];
    sub(p, sizeof p, d, "viadot"); dh_w(w, MAX_PATH, p);
    snwprintf(r, MAX_PATH + 8, L"\\\\.\\%ls", w);
    BOOL ok = U(CreateDirectoryW)(r, NULL);
    t_note("create via \\\\.\\ prefix: ok=%d err=%lu", ok, GetLastError());
    if (ok) CHECK(dh_isdir(p));
}
TEST(dir_create_security_attributes_null_sd) {
    REQUIRE(U(CreateDirectoryA));
    char *d = dh_dir("cr_sa1"), p[MAX_PATH];
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    sub(p, sizeof p, d, "sa"); CHECK(U(CreateDirectoryA)(p, &sa));
    CHECK(dh_isdir(p));
}
TEST(dir_create_security_attributes_sd) {
    REQUIRE(U(CreateDirectoryA));
    char *d = dh_dir("cr_sa2"), p[MAX_PATH];
    PSECURITY_DESCRIPTOR sd = NULL;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorA("D:(A;OICI;GA;;;WD)(A;OICI;GA;;;BA)", SDDL_REVISION_1, &sd, NULL)) { t_skip("no sd"); return; }
    SECURITY_ATTRIBUTES sa = { sizeof sa, sd, FALSE };
    sub(p, sizeof p, d, "sa"); CHECK(U(CreateDirectoryA)(p, &sa));
    CHECK(dh_isdir(p));
    PSECURITY_DESCRIPTOR got = NULL; PACL dacl = NULL;
    DWORD r = GetNamedSecurityInfoA(p, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, &dacl, NULL, &got);
    CHECK(r == ERROR_SUCCESS && dacl != NULL);
    if (got) LocalFree(got);
    LocalFree(sd);
}
TEST(dir_create_security_attributes_ex) {
    REQUIRE(U(CreateDirectoryExW));
    char *d = dh_dir("cr_sa3"), tpl[MAX_PATH], p[MAX_PATH]; WCHAR wt[MAX_PATH], wp[MAX_PATH];
    sub(tpl, sizeof tpl, d, "tpl"); CreateDirectoryA(tpl, NULL); sub(p, sizeof p, d, "n");
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, FALSE };
    dh_w(wt, MAX_PATH, tpl); dh_w(wp, MAX_PATH, p);
    CHECK(U(CreateDirectoryExW)(wt, wp, &sa));
    CHECK(dh_isdir(p));
}
TEST(dir_create_unicode_names) {
    REQUIRE(U(CreateDirectoryW)); REQUIRE(U(RemoveDirectoryW));
    char *d = dh_dir("cr_uni"); WCHAR dw[MAX_PATH], w[MAX_PATH + 32];
    dh_w(dw, MAX_PATH, d);
    const WCHAR *names[] = { L"caf\u00e9", L"\u4e2d\u6587\u76ee\u5f55", L"\u0434\u0438\u0440", L"\U0001F600", L"\U00020000x", L"\u00fc\u00f1\u00ee" };
    for (unsigned i = 0; i < sizeof names / sizeof *names; i++) {
        snwprintf(w, MAX_PATH + 32, L"%ls\\%ls", dw, names[i]);
        CHECKF(U(CreateDirectoryW)(w, NULL), "i=%u err %lu", i, GetLastError());
        CHECK(GetFileAttributesW(w) != INVALID_FILE_ATTRIBUTES);
        CHECK(!U(CreateDirectoryW)(w, NULL));
        EXPECT_ERR(ERROR_ALREADY_EXISTS);
    }
    for (unsigned i = 0; i < sizeof names / sizeof *names; i++) {
        snwprintf(w, MAX_PATH + 32, L"%ls\\%ls", dw, names[i]);
        CHECK(U(RemoveDirectoryW)(w));
    }
}
TEST(dir_create_unicode_case_insensitive) {
    REQUIRE(U(CreateDirectoryW));
    char *d = dh_dir("cr_unicase"); WCHAR dw[MAX_PATH], w[MAX_PATH + 32];
    dh_w(dw, MAX_PATH, d);
    snwprintf(w, MAX_PATH + 32, L"%ls\\\u00c9cole", dw);
    CHECK(U(CreateDirectoryW)(w, NULL));
    snwprintf(w, MAX_PATH + 32, L"%ls\\\u00e9COLE", dw);
    CHECK(!U(CreateDirectoryW)(w, NULL));
    EXPECT_ERR(ERROR_ALREADY_EXISTS);
}
TEST(dir_create_lone_surrogate) {
    REQUIRE(U(CreateDirectoryW)); REQUIRE(U(RemoveDirectoryW));
    char *d = dh_dir("cr_lone"); WCHAR dw[MAX_PATH], w[MAX_PATH + 32];
    dh_w(dw, MAX_PATH, d);
    snwprintf(w, MAX_PATH + 32, L"%ls\\ls_\xd800_x", dw);
    SET_ERR(0);
    BOOL ok = U(CreateDirectoryW)(w, NULL);
    t_note("lone high surrogate dir name: ok=%d err=%lu (NTFS stores raw UTF-16)", ok, GetLastError());
    if (ok) { CHECK(GetFileAttributesW(w) != INVALID_FILE_ATTRIBUTES); U(RemoveDirectoryW)(w); }
}
TEST(dir_create_a_acp_roundtrip) {
    REQUIRE(U(CreateDirectoryA));
    char *d = dh_dir("cr_acp"), p[MAX_PATH]; WCHAR w[MAX_PATH];
    sub(p, sizeof p, d, "ascii_only_name");
    CHECK(U(CreateDirectoryA)(p, NULL));
    dh_w(w, MAX_PATH, p);
    CHECK(GetFileAttributesW(w) != INVALID_FILE_ATTRIBUTES);
}
TEST(dir_create_a_vs_w_parity_errors) {
    REQUIRE(U(CreateDirectoryA)); REQUIRE(U(CreateDirectoryW));
    char *d = dh_dir("cr_par"), p[MAX_PATH]; WCHAR w[MAX_PATH];
    sub(p, sizeof p, d, "m\\n"); dh_w(w, MAX_PATH, p);
    SET_ERR(0); BOOL a = U(CreateDirectoryA)(p, NULL); DWORD ea = GetLastError();
    SET_ERR(0); BOOL b = U(CreateDirectoryW)(w, NULL); DWORD eb = GetLastError();
    EXPECT_EQ(a, b); EXPECT_EQ(ea, eb);
}
TEST(dir_create_drive_relative_missing_drive) {
    REQUIRE(U(CreateDirectoryA));
    /* find an unused drive letter */
    DWORD mask = GetLogicalDrives(); char p[16] = "?:\\k32t_nodrive";
    int c;
    for (c = 25; c >= 0; c--) if (!(mask & (1u << c))) break;
    if (c < 0) { t_skip("all drive letters in use"); return; }
    p[0] = 'A' + c;
    SET_ERR(0);
    CHECK(!U(CreateDirectoryA)(p, NULL));
    DWORD e = GetLastError();
    CHECKF(e == ERROR_PATH_NOT_FOUND || e == ERROR_INVALID_DRIVE || e == ERROR_FILE_NOT_FOUND, "err %lu", e);
}
TEST(dir_create_exists_vs_missing_leaf_attr) {
    REQUIRE(U(CreateDirectoryA));
    char *d = dh_dir("cr_attr"), p[MAX_PATH];
    sub(p, sizeof p, d, "newd"); CHECK(U(CreateDirectoryA)(p, NULL));
    DWORD a = GetFileAttributesA(p);
    CHECK(a & FILE_ATTRIBUTE_DIRECTORY);
    CHECK(!(a & FILE_ATTRIBUTE_READONLY));
    CHECK(!(a & FILE_ATTRIBUTE_HIDDEN));
    CHECK(!(a & FILE_ATTRIBUTE_REPARSE_POINT));
}
TEST(dir_create_many_siblings) {
    REQUIRE(U(CreateDirectoryA)); REQUIRE(U(RemoveDirectoryA));
    char *d = dh_dir("cr_many"), p[MAX_PATH], n[32];
    int ok = 0;
    for (int i = 0; i < 300; i++) { snprintf(n, sizeof n, "d%04d", i); sub(p, sizeof p, d, n); ok += U(CreateDirectoryA)(p, NULL) != 0; }
    EXPECT_EQ(ok, 300);
    dh_name *l = malloc(sizeof(dh_name) * 400);
    sub(p, sizeof p, d, "*");
    int cnt = dh_listA_nd(p, l, 400);
    EXPECT_EQ(cnt, 300);
    free(l);
    ok = 0;
    for (int i = 0; i < 300; i++) { snprintf(n, sizeof n, "d%04d", i); sub(p, sizeof p, d, n); ok += U(RemoveDirectoryA)(p) != 0; }
    EXPECT_EQ(ok, 300);
}
TEST(dir_create_deep_chain) {
    REQUIRE(U(CreateDirectoryA));
    char *d = dh_dir("cr_deep"), p[MAX_PATH];
    snprintf(p, sizeof p, "%s", d);
    int made = 0;
    for (int i = 0; i < 20; i++) {
        size_t l = strlen(p);
        if (l + 4 > 240) break;
        strcat(p, "\\ab");
        if (!U(CreateDirectoryA)(p, NULL)) break;
        made++;
    }
    CHECK(made >= 10);
    CHECK(dh_isdir(p));
}
TEST(dir_create_ex_copies_nothing_inside) {
    REQUIRE(U(CreateDirectoryExA));
    char *d = dh_dir("cr_excopy"), tpl[MAX_PATH], p[MAX_PATH];
    sub(tpl, sizeof tpl, d, "tpl"); CreateDirectoryA(tpl, NULL);
    dh_mkfile(tpl, "inside.txt", 3);
    sub(p, sizeof p, d, "n");
    CHECK(U(CreateDirectoryExA)(tpl, p, NULL));
    dh_name l[4]; char pat[MAX_PATH]; sub(pat, sizeof pat, p, "*");
    EXPECT_EQ(dh_listA_nd(pat, l, 4), 0);   /* contents are not copied */
}
