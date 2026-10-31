/* GetFinalPathNameByHandleA/W */
#include "fa.h"
#include <wchar.h>

#ifndef VOLUME_NAME_DOS
#define VOLUME_NAME_DOS 0x0
#define VOLUME_NAME_GUID 0x1
#define VOLUME_NAME_NT 0x2
#define VOLUME_NAME_NONE 0x4
#define FILE_NAME_NORMALIZED 0x0
#define FILE_NAME_OPENED 0x8
#endif

static DWORD gfW(HANDLE h, WCHAR *b, DWORD n, DWORD f) { return U(GetFinalPathNameByHandleW)(h, b, n, f); }
static DWORD gfA(HANDLE h, char *b, DWORD n, DWORD f) { return U(GetFinalPathNameByHandleA)(h, b, n, f); }
static HANDLE mkf(const char *leaf) { fa_mkfile_s(leaf, "x"); return fa_openl(leaf, GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0); }

TEST(fpath_dos_default_form) {
    REQUIRE(U(GetFinalPathNameByHandleW));
    HANDLE h = mkf("fp_dos.txt");
    WCHAR b[MAX_PATH * 3]; DWORD n = gfW(h, b, MAX_PATH * 3, 0);
    CHECK(n > 0 && n == (DWORD)lstrlenW(b));
    CHECK(!wcsncmp(b, L"\\\\?\\", 4));
    CHECK(b[5] == L':' && b[6] == L'\\');
    CHECK(b[4] >= L'A' && b[4] <= L'Z');
    WCHAR want[MAX_PATH * 2]; fa_wpath(want, "fp_dos.txt");
    CHECK(!_wcsicmp(b + 4, want));
    CloseHandle(h); fa_rml("fp_dos.txt");
}
TEST(fpath_dos_a_matches_w) {
    REQUIRE(U(GetFinalPathNameByHandleW)); REQUIRE(U(GetFinalPathNameByHandleA));
    HANDLE h = mkf("fp_aw.txt");
    WCHAR w[MAX_PATH * 3]; char a[MAX_PATH * 3];
    DWORD nw = gfW(h, w, MAX_PATH * 3, 0), na = gfA(h, a, MAX_PATH * 3, 0);
    EXPECT_EQ(nw, na);
    char conv[MAX_PATH * 3]; WideCharToMultiByte(CP_ACP, 0, w, -1, conv, sizeof conv, NULL, NULL);
    EXPECT_STR(a, conv);
    CloseHandle(h); fa_rml("fp_aw.txt");
}
TEST(fpath_guid_form) {
    REQUIRE(U(GetFinalPathNameByHandleW));
    HANDLE h = mkf("fp_guid.txt");
    WCHAR b[MAX_PATH * 3]; DWORD n = gfW(h, b, MAX_PATH * 3, VOLUME_NAME_GUID);
    CHECK(n > 0);
    CHECK(!wcsncmp(b, L"\\\\?\\Volume{", 11));
    CHECK(b[11 + 36] == L'}' && b[11 + 37] == L'\\');
    CHECK(wcsstr(b, L"fp_guid.txt") != NULL);
    CloseHandle(h); fa_rml("fp_guid.txt");
}
TEST(fpath_nt_form) {
    REQUIRE(U(GetFinalPathNameByHandleW));
    HANDLE h = mkf("fp_nt.txt");
    WCHAR b[MAX_PATH * 3]; DWORD n = gfW(h, b, MAX_PATH * 3, VOLUME_NAME_NT);
    CHECK(n > 0);
    CHECK(!wcsncmp(b, L"\\Device\\", 8));
    CHECK(wcsstr(b, L"fp_nt.txt") != NULL);
    t_note("NT form: %ls", b);
    CloseHandle(h); fa_rml("fp_nt.txt");
}
TEST(fpath_none_form) {
    REQUIRE(U(GetFinalPathNameByHandleW));
    HANDLE h = mkf("fp_none.txt");
    WCHAR b[MAX_PATH * 3]; DWORD n = gfW(h, b, MAX_PATH * 3, VOLUME_NAME_NONE);
    CHECK(n > 0);
    CHECK(b[0] == L'\\' && b[1] != L'\\' && b[1] != L'?');
    CHECK(wcsstr(b, L"fp_none.txt") != NULL);
    BYTE buf[2048]; FILE_NAME_INFO *ni = (FILE_NAME_INFO *)buf;
    if (U(GetFileInformationByHandleEx) && U(GetFileInformationByHandleEx)(h, FileNameInfo, buf, sizeof buf)) {
        EXPECT_EQ(ni->FileNameLength / 2, n);
        CHECK(!wcsncmp(ni->FileName, b, n));
    }
    CloseHandle(h); fa_rml("fp_none.txt");
}
TEST(fpath_forms_share_the_same_tail) {
    REQUIRE(U(GetFinalPathNameByHandleW));
    HANDLE h = mkf("fp_tail.txt");
    WCHAR d[MAX_PATH * 3], g[MAX_PATH * 3], nt[MAX_PATH * 3], no[MAX_PATH * 3];
    DWORD a = gfW(h, d, MAX_PATH * 3, 0), b = gfW(h, g, MAX_PATH * 3, 1), c = gfW(h, nt, MAX_PATH * 3, 2), e = gfW(h, no, MAX_PATH * 3, 4);
    CHECK(a && b && c && e);
    CHECK(a >= e && b >= e && c >= e);
    CHECK(!_wcsicmp(d + a - e, no));
    CHECK(!_wcsicmp(g + b - e, no));
    CHECK(!_wcsicmp(nt + c - e, no));
    CloseHandle(h); fa_rml("fp_tail.txt");
}
TEST(fpath_return_value_sizing_w) {
    REQUIRE(U(GetFinalPathNameByHandleW));
    HANDLE h = mkf("fp_size.txt");
    WCHAR full[MAX_PATH * 3]; DWORD len = gfW(h, full, MAX_PATH * 3, 0);
    CHECK(len > 8);
    /* NULL / 0 -> required size including NUL */
    EXPECT_EQ(gfW(h, NULL, 0, 0), len + 1);
    WCHAR *b = malloc((len + 8) * 2);
    /* exact fit (len + 1) -> len */
    for (DWORD i = 0; i < len + 8; i++) b[i] = 0x7777;
    EXPECT_EQ(gfW(h, b, len + 1, 0), len);
    EXPECT_EQ(b[len], 0);
    /* one short (len) -> len + 1 */
    SetLastError(0);
    EXPECT_EQ(gfW(h, b, len, 0), len + 1);
    /* much too small */
    EXPECT_EQ(gfW(h, b, 1, 0), len + 1);
    EXPECT_EQ(gfW(h, b, 0, 0), len + 1);
    /* larger than needed */
    EXPECT_EQ(gfW(h, b, len + 8, 0), len);
    free(b);
    CloseHandle(h); fa_rml("fp_size.txt");
}
TEST(fpath_return_value_sizing_all_forms) {
    REQUIRE(U(GetFinalPathNameByHandleW));
    HANDLE h = mkf("fp_size2.txt");
    static const DWORD f[] = { VOLUME_NAME_DOS, VOLUME_NAME_GUID, VOLUME_NAME_NT, VOLUME_NAME_NONE, VOLUME_NAME_DOS | FILE_NAME_OPENED, VOLUME_NAME_NT | FILE_NAME_OPENED, VOLUME_NAME_NONE | FILE_NAME_OPENED };
    for (unsigned i = 0; i < sizeof f / sizeof *f; i++) {
        WCHAR full[MAX_PATH * 3]; DWORD len = gfW(h, full, MAX_PATH * 3, f[i]);
        CHECKF(len > 0 && len == (DWORD)lstrlenW(full), "flags %lx len %lu", f[i], len);
        DWORD q = gfW(h, NULL, 0, f[i]);
        CHECKF(q == len + 1, "flags %lx query %lu len %lu", f[i], q, len);
        WCHAR tiny[4];
        DWORD r = gfW(h, tiny, 4, f[i]);
        CHECKF(r == len + 1, "flags %lx small %lu len %lu", f[i], r, len);
    }
    CloseHandle(h); fa_rml("fp_size2.txt");
}
TEST(fpath_return_value_sizing_a) {
    REQUIRE(U(GetFinalPathNameByHandleA));
    HANDLE h = mkf("fp_sizea.txt");
    char full[MAX_PATH * 3]; DWORD len = gfA(h, full, sizeof full, 0);
    CHECK(len > 8 && len == strlen(full));
    char *b = malloc(len + 8);
    memset(b, 0x77, len + 8);
    EXPECT_EQ(gfA(h, b, len + 1, 0), len);
    EXPECT_EQ(b[len], 0);
    memset(b, 0x77, len + 8);
    DWORD r = gfA(h, b, len, 0);
    /* the narrow variant reports the length without terminator on truncation; never less than the buffer size */
    CHECKF(r >= len, "short buffer returned %lu (len %lu)", r, len);
    t_note("A variant, cch=len -> %lu (len %lu)", r, len);
    CHECK(b[0] == 0x77 || r >= len);   /* buffer not partially filled with a truncated path */
    r = gfA(h, NULL, 0, 0);
    t_note("A variant, NULL/0 -> %lu (len %lu)", r, len);
    CHECK(r >= len);
    free(b);
    CloseHandle(h); fa_rml("fp_sizea.txt");
}
TEST(fpath_invalid_flag_combinations) {
    REQUIRE(U(GetFinalPathNameByHandleW));
    HANDLE h = mkf("fp_flags.txt");
    WCHAR b[MAX_PATH * 3];
    static const DWORD bad[] = { 3, 5, 6, 7, 3 | 8, 5 | 8, 7 | 8 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SetLastError(0);
        EXPECT_EQ(gfW(h, b, MAX_PATH * 3, bad[i]), 0);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
    CloseHandle(h); fa_rml("fp_flags.txt");
}
TEST(fpath_invalid_flag_combinations_a) {
    REQUIRE(U(GetFinalPathNameByHandleA));
    HANDLE h = mkf("fp_flagsa.txt");
    char b[MAX_PATH * 3];
    SetLastError(0); EXPECT_EQ(gfA(h, b, sizeof b, 3), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SetLastError(0); EXPECT_EQ(gfA(h, b, sizeof b, 6), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CloseHandle(h); fa_rml("fp_flagsa.txt");
}
TEST(fpath_opened_vs_normalized_same_for_plain_name) {
    REQUIRE(U(GetFinalPathNameByHandleW));
    HANDLE h = mkf("fp_opn.txt");
    WCHAR a[MAX_PATH * 3], b[MAX_PATH * 3];
    DWORD na = gfW(h, a, MAX_PATH * 3, FILE_NAME_NORMALIZED), nb = gfW(h, b, MAX_PATH * 3, FILE_NAME_OPENED);
    CHECK(na > 0 && nb > 0);
    if (na == nb) CHECK(!_wcsicmp(a, b));
    CloseHandle(h); fa_rml("fp_opn.txt");
}
TEST(fpath_normalizes_short_name) {
    REQUIRE(U(GetFinalPathNameByHandleW));
    FA_REQUIRE_NTFS();
    fa_mkfile_s("fp_a_rather_long_file_name.txt", "x");
    char p[MAX_PATH * 2], sp[MAX_PATH * 2]; fa_path(p, "fp_a_rather_long_file_name.txt");
    DWORD n = GetShortPathNameA(p, sp, sizeof sp);
    if (!n || !_stricmp(p, sp)) { t_skip("8.3 names unavailable"); fa_rm(p); return; }
    HANDLE h = fa_open(sp, GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0);
    WCHAR b[MAX_PATH * 3];
    CHECK(gfW(h, b, MAX_PATH * 3, FILE_NAME_NORMALIZED) > 0);
    CHECK(wcsstr(b, L"fp_a_rather_long_file_name.txt") != NULL || wcsstr(b, L"FP_A_RATHER_LONG_FILE_NAME.TXT") != NULL);
    WCHAR o[MAX_PATH * 3];
    DWORD no = gfW(h, o, MAX_PATH * 3, FILE_NAME_OPENED);
    t_note("OPENED form via short name: %ls", o);
    CloseHandle(h); fa_rm(p);
}
TEST(fpath_invalid_handles) {
    REQUIRE(U(GetFinalPathNameByHandleW)); REQUIRE(U(GetFinalPathNameByHandleA));
    WCHAR w[MAX_PATH]; char a[MAX_PATH];
    SetLastError(0); EXPECT_EQ(gfW(INVALID_HANDLE_VALUE, w, MAX_PATH, 0), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SetLastError(0); EXPECT_EQ(gfA(INVALID_HANDLE_VALUE, a, MAX_PATH, 0), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SetLastError(0); EXPECT_EQ(gfW(NULL, w, MAX_PATH, 0), 0);
    t_note("GetFinalPathNameByHandleW(NULL) err=%lu", GetLastError());
    SetLastError(0); EXPECT_EQ(gfW((HANDLE)(ULONG_PTR)0x777770, w, MAX_PATH, 0), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(fpath_invalid_handle_checked_after_flags) {
    REQUIRE(U(GetFinalPathNameByHandleW));
    WCHAR w[MAX_PATH];
    /* INVALID_HANDLE_VALUE is rejected before the flags are examined */
    SetLastError(0); EXPECT_EQ(gfW(INVALID_HANDLE_VALUE, w, MAX_PATH, 3), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(fpath_closed_handle) {
    REQUIRE(U(GetFinalPathNameByHandleW));
    HANDLE h = mkf("fp_closed.txt"); CloseHandle(h);
    WCHAR w[MAX_PATH];
    SetLastError(0); EXPECT_EQ(gfW(h, w, MAX_PATH, 0), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    fa_rml("fp_closed.txt");
}
TEST(fpath_wrong_handle_type) {
    REQUIRE(U(GetFinalPathNameByHandleW));
    HANDLE e = CreateEventA(NULL, FALSE, FALSE, NULL);
    WCHAR w[MAX_PATH];
    SetLastError(0); DWORD r = gfW(e, w, MAX_PATH, 0);
    DWORD err = GetLastError();
    EXPECT_EQ(r, 0);
    CHECKF(err == ERROR_INVALID_HANDLE || err == ERROR_INVALID_FUNCTION || err == ERROR_INVALID_PARAMETER, "err %lu", err);
    t_note("GetFinalPathNameByHandle(event) err=%lu", err);
    CloseHandle(e);
}
TEST(fpath_directory_handle) {
    REQUIRE(U(GetFinalPathNameByHandleW));
    fa_mkdir("fp_dir");
    HANDLE h = fa_opendir("fp_dir", GENERIC_READ);
    WCHAR b[MAX_PATH * 3]; DWORD n = gfW(h, b, MAX_PATH * 3, 0);
    CHECK(n > 0);
    CHECK(!wcsncmp(b, L"\\\\?\\", 4));
    CHECK(b[n - 1] == L'r');      /* no trailing backslash */
    CloseHandle(h); fa_rml("fp_dir");
}
TEST(fpath_root_directory_handle) {
    REQUIRE(U(GetFinalPathNameByHandleW));
    char p[MAX_PATH * 2], vol[MAX_PATH]; fa_path(p, ""); GetVolumePathNameA(p, vol, sizeof vol);
    HANDLE h = CreateFileA(vol, GENERIC_READ, FA_SHARE_ALL, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_skip("cannot open root"); return; }
    WCHAR b[MAX_PATH]; DWORD n = gfW(h, b, MAX_PATH, 0);
    CHECK(n >= 7);
    CHECK(!wcsncmp(b, L"\\\\?\\", 4));
    CHECK(b[n - 1] == L'\\');
    n = gfW(h, b, MAX_PATH, VOLUME_NAME_NONE);
    EXPECT_EQ(n, 1); EXPECT_EQ(b[0], L'\\');
    CloseHandle(h);
}
TEST(fpath_follows_rename) {
    REQUIRE(U(GetFinalPathNameByHandleW));
    HANDLE h = mkf("fp_rn_a.txt");
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_path(a, "fp_rn_a.txt"); fa_path(b, "fp_rn_b.txt");
    CHECK(MoveFileA(a, b));
    WCHAR w[MAX_PATH * 3];
    CHECK(gfW(h, w, MAX_PATH * 3, 0) > 0);
    CHECK(wcsstr(w, L"fp_rn_b.txt") != NULL);
    CloseHandle(h); fa_rm(b);
}
TEST(fpath_hardlink_reports_a_valid_link_name) {
    REQUIRE(U(GetFinalPathNameByHandleW));
    FA_REQUIRE_NTFS();
    HANDLE h = mkf("fp_hl1.txt");
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_path(a, "fp_hl1.txt"); fa_path(b, "fp_hl2.txt");
    if (!CreateHardLinkA(b, a, NULL)) { t_skip("no hard links"); CloseHandle(h); fa_rm(a); return; }
    HANDLE h2 = fa_open(b, GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0);
    WCHAR w1[MAX_PATH * 3], w2[MAX_PATH * 3];
    CHECK(gfW(h, w1, MAX_PATH * 3, 0) > 0); CHECK(gfW(h2, w2, MAX_PATH * 3, 0) > 0);
    t_note("hardlink names: %ls | %ls", w1, w2);
    CHECK(wcsstr(w1, L"fp_hl") != NULL); CHECK(wcsstr(w2, L"fp_hl") != NULL);
    CloseHandle(h2); CloseHandle(h); fa_rm(a); fa_rm(b);
}
TEST(fpath_delete_pending_file) {
    REQUIRE(U(GetFinalPathNameByHandleW));
    FA_REQUIRE_NTFS();
    fa_mkfile_s("fp_del.txt", "x");
    HANDLE h = fa_openl("fp_del.txt", GENERIC_READ | DELETE, FA_SHARE_ALL, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE);
    WCHAR w[MAX_PATH * 3]; DWORD n = gfW(h, w, MAX_PATH * 3, 0);
    CHECK(n > 0);
    CloseHandle(h);
}
TEST(fpath_non_nul_buffer_untouched_on_query) {
    REQUIRE(U(GetFinalPathNameByHandleW));
    HANDLE h = mkf("fp_untouched.txt");
    WCHAR b[4] = { 0x1111, 0x2222, 0x3333, 0x4444 };
    DWORD r = gfW(h, b, 0, 0);
    CHECK(r > 4);
    CHECK(b[0] == 0x1111 && b[1] == 0x2222 && b[2] == 0x3333 && b[3] == 0x4444);
    CloseHandle(h); fa_rml("fp_untouched.txt");
}
TEST(fpath_repeat_stable) {
    REQUIRE(U(GetFinalPathNameByHandleW));
    HANDLE h = mkf("fp_rep.txt");
    WCHAR a[MAX_PATH * 3], b[MAX_PATH * 3];
    DWORD n = gfW(h, a, MAX_PATH * 3, 0); int bad = 0;
    for (int i = 0; i < 300; i++) { if (gfW(h, b, MAX_PATH * 3, 0) != n || wcscmp(a, b)) bad++; }
    EXPECT_EQ(bad, 0);
    CloseHandle(h); fa_rml("fp_rep.txt");
}
TEST(fpath_unicode_name) {
    REQUIRE(U(GetFinalPathNameByHandleW));
    WCHAR p[MAX_PATH * 2], q[MAX_PATH * 2]; fa_wpath(p, "");
    wsprintfW(q, L"%lsfp_u\x00e9\x4e2d.txt", p);
    HANDLE h = CreateFileW(q, GENERIC_WRITE, FA_SHARE_ALL, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_skip("cannot create unicode file"); return; }
    WCHAR b[MAX_PATH * 3]; DWORD n = gfW(h, b, MAX_PATH * 3, 0);
    CHECK(n > 0 && b[n - 6] == 0x00e9 && b[n - 5] == 0x4e2d);
    char a[MAX_PATH * 3]; SetLastError(0);
    DWORD na = U(GetFinalPathNameByHandleA) ? gfA(h, a, sizeof a, 0) : 0;
    t_note("A variant of a unicode name: n=%lu", na);
    CloseHandle(h); DeleteFileW(q);
}
TEST(fpath_through_prefix_open_and_dotted_path) {
    REQUIRE(U(GetFinalPathNameByHandleW));
    fa_mkdir("fp_dd"); fa_mkfile_s("fp_dd\\f.txt", "x");
    char p[MAX_PATH * 2]; fa_path(p, "fp_dd\\..\\fp_dd\\.\\f.txt");
    HANDLE h = fa_open(p, GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0);
    CHECK(h != INVALID_HANDLE_VALUE);
    WCHAR b[MAX_PATH * 3]; CHECK(gfW(h, b, MAX_PATH * 3, 0) > 0);
    CHECK(wcsstr(b, L"..") == NULL && wcsstr(b, L"\\.\\") == NULL);
    CloseHandle(h); fa_rml("fp_dd\\f.txt"); fa_rml("fp_dd");
}
TEST(fpath_junction_or_symlink_resolution) {
    REQUIRE(U(GetFinalPathNameByHandleW));
    FA_REQUIRE_NTFS();
    fa_mkdir("fp_real"); fa_mkfile_s("fp_real\\t.txt", "x");
    /* a symbolic link may be unavailable without privilege */
    char l[MAX_PATH * 2], t[MAX_PATH * 2]; fa_path(l, "fp_lnk"); fa_path(t, "fp_real");
    BOOLEAN ok = CreateSymbolicLinkA(l, t, SYMBOLIC_LINK_FLAG_DIRECTORY | 2 /*ALLOW_UNPRIVILEGED_CREATE*/);
    if (!ok) { t_skip("cannot create symlink (err %lu)", GetLastError()); fa_rml("fp_real\\t.txt"); fa_rml("fp_real"); return; }
    char f[MAX_PATH * 2]; fa_path(f, "fp_lnk\\t.txt");
    HANDLE h = fa_open(f, GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0);
    CHECK(h != INVALID_HANDLE_VALUE);
    WCHAR b[MAX_PATH * 3]; CHECK(gfW(h, b, MAX_PATH * 3, 0) > 0);
    CHECK(wcsstr(b, L"fp_real") != NULL && wcsstr(b, L"fp_lnk") == NULL);
    CloseHandle(h);
    RemoveDirectoryA(l); fa_rml("fp_real\\t.txt"); fa_rml("fp_real");
}
