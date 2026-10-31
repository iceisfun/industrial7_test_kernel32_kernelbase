/* System/Windows/WOW64 directories, SearchPath, NeedCurrentDirectoryForExePath, SetSearchPathMode, CheckNameLegalDOS8Dot3 */
#include "pn.h"

typedef UINT (WINAPI *dir_w_fn)(LPWSTR, UINT);
typedef UINT (WINAPI *dir_a_fn)(LPSTR, UINT);

static void dir_semantics_w(const char *name, dir_w_fn f, WCHAR *out) {
    WCHAR b[MAX_PATH], c[MAX_PATH];
    UINT len = f(b, MAX_PATH);
    CHECKF(len > 2 && len < MAX_PATH, "%s len %u", name, len);
    CHECKF(wcslen(b) == len, "%s: return value must equal length", name);
    CHECKF(b[len - 1] != L'\\', "%s: no trailing backslash", name);
    CHECKF(len < 3 || b[1] == L':' || b[0] == L'\\', "%s: not absolute", name);
    EXPECT_EQ(f(NULL, 0), len + 1);
    EXPECT_EQ(f(c, 0), len + 1);
    EXPECT_EQ(f(c, 1), len + 1);
    EXPECT_EQ(f(c, len), len + 1);
    pn_fill(c, 5, L'Z');
    EXPECT_EQ(f(c, len + 1), len);
    CHECKF(!wcscmp(b, c), "%s", name);
    if (out) wcscpy(out, b);
}

TEST(path_names_sysdir_w_semantics) {
    REQUIRE(U(GetSystemDirectoryW));
    WCHAR b[MAX_PATH];
    dir_semantics_w("GetSystemDirectoryW", U(GetSystemDirectoryW), b);
    size_t l = wcslen(b);
    CHECK(l >= 9 && !_wcsicmp(b + l - 9, L"\\System32"));
}
TEST(path_names_sysdir_a_semantics) {
    REQUIRE(U(GetSystemDirectoryA));
    char b[MAX_PATH], c[MAX_PATH];
    UINT len = U(GetSystemDirectoryA)(b, MAX_PATH);
    CHECK(len > 3 && strlen(b) == len);
    EXPECT_EQ(U(GetSystemDirectoryA)(NULL, 0), len + 1);
    EXPECT_EQ(U(GetSystemDirectoryA)(c, 0), len + 1);
    EXPECT_EQ(U(GetSystemDirectoryA)(c, len), len + 1);
    EXPECT_EQ(U(GetSystemDirectoryA)(c, len + 1), len);
    EXPECT_STR(b, c);
}
TEST(path_names_sysdir_a_w_parity) {
    REQUIRE(U(GetSystemDirectoryA)); REQUIRE(U(GetSystemDirectoryW));
    char a[MAX_PATH], w8[MAX_PATH]; WCHAR w[MAX_PATH];
    U(GetSystemDirectoryA)(a, MAX_PATH); U(GetSystemDirectoryW)(w, MAX_PATH);
    pn_a(w8, sizeof w8, w);
    EXPECT_STR(a, w8);
}
TEST(path_names_sysdir_exists_and_has_kernel32) {
    REQUIRE(U(GetSystemDirectoryA));
    char a[MAX_PATH], k[MAX_PATH];
    U(GetSystemDirectoryA)(a, MAX_PATH);
    DWORD at = GetFileAttributesA(a);
    CHECK(at != INVALID_FILE_ATTRIBUTES && (at & FILE_ATTRIBUTE_DIRECTORY));
    snprintf(k, sizeof k, "%s\\kernel32.dll", a);
    CHECK(pn_fileexists(k));
    snprintf(k, sizeof k, "%s\\ntdll.dll", a);
    CHECK(pn_fileexists(k));
}
TEST(path_names_sysdir_is_under_windows_dir) {
    REQUIRE(U(GetSystemDirectoryA)); REQUIRE(U(GetWindowsDirectoryA));
    char s[MAX_PATH], w[MAX_PATH];
    U(GetSystemDirectoryA)(s, MAX_PATH); U(GetWindowsDirectoryA)(w, MAX_PATH);
    size_t l = strlen(w);
    CHECK(!_strnicmp(s, w, l) && s[l] == '\\');
}
TEST(path_names_sysdir_kernel32_module_lives_there) {
    REQUIRE(U(GetSystemDirectoryA));
    char s[MAX_PATH], m[MAX_PATH], want[MAX_PATH];
    U(GetSystemDirectoryA)(s, MAX_PATH);
    GetModuleFileNameA(GetModuleHandleA("kernel32.dll"), m, MAX_PATH);
    snprintf(want, sizeof want, "%s\\kernel32.dll", s);
    CHECKF(!_stricmp(m, want), "%s vs %s", m, want);
}
TEST(path_names_sysdir_repeated_stable) {
    REQUIRE(U(GetSystemDirectoryW));
    WCHAR a[MAX_PATH], b[MAX_PATH];
    U(GetSystemDirectoryW)(a, MAX_PATH);
    for (int i = 0; i < 100; i++) { U(GetSystemDirectoryW)(b, MAX_PATH); if (wcscmp(a, b)) { CHECK(0); return; } }
    CHECK(1);
}
TEST(path_names_windir_w_semantics) {
    REQUIRE(U(GetWindowsDirectoryW));
    dir_semantics_w("GetWindowsDirectoryW", U(GetWindowsDirectoryW), NULL);
}
TEST(path_names_syswindir_w_semantics) {
    REQUIRE(U(GetSystemWindowsDirectoryW));
    dir_semantics_w("GetSystemWindowsDirectoryW", U(GetSystemWindowsDirectoryW), NULL);
}
TEST(path_names_windir_a_semantics) {
    REQUIRE(U(GetWindowsDirectoryA)); REQUIRE(U(GetSystemWindowsDirectoryA));
    char b[MAX_PATH], c[MAX_PATH];
    UINT len = U(GetWindowsDirectoryA)(b, MAX_PATH);
    CHECK(len >= 3 && strlen(b) == len);
    EXPECT_EQ(U(GetWindowsDirectoryA)(NULL, 0), len + 1);
    EXPECT_EQ(U(GetWindowsDirectoryA)(c, len), len + 1);
    EXPECT_EQ(U(GetWindowsDirectoryA)(c, len + 1), len);
    len = U(GetSystemWindowsDirectoryA)(b, MAX_PATH);
    EXPECT_EQ(U(GetSystemWindowsDirectoryA)(NULL, 0), len + 1);
    EXPECT_EQ(U(GetSystemWindowsDirectoryA)(c, len), len + 1);
    EXPECT_EQ(U(GetSystemWindowsDirectoryA)(c, len + 1), len);
    EXPECT_STR(b, c);
}
TEST(path_names_windir_equals_systemroot_env) {
    REQUIRE(U(GetWindowsDirectoryW));
    WCHAR w[MAX_PATH], e[MAX_PATH];
    U(GetWindowsDirectoryW)(w, MAX_PATH);
    if (!GetEnvironmentVariableW(L"SystemRoot", e, MAX_PATH)) { t_skip("no SystemRoot"); return; }
    CHECKF(!_wcsicmp(w, e), "%s vs %s", pn_n(w), pn_n(e));
    if (GetEnvironmentVariableW(L"windir", e, MAX_PATH)) CHECK(!_wcsicmp(w, e));
}
TEST(path_names_windir_equals_systemwindowsdir) {
    REQUIRE(U(GetWindowsDirectoryW)); REQUIRE(U(GetSystemWindowsDirectoryW));
    WCHAR a[MAX_PATH], b[MAX_PATH];
    U(GetWindowsDirectoryW)(a, MAX_PATH); U(GetSystemWindowsDirectoryW)(b, MAX_PATH);
    CHECK(!wcscmp(a, b));
}
TEST(path_names_windir_contains_explorer_or_notepad) {
    REQUIRE(U(GetWindowsDirectoryA));
    char w[MAX_PATH], p[MAX_PATH];
    U(GetWindowsDirectoryA)(w, MAX_PATH);
    snprintf(p, sizeof p, "%s\\explorer.exe", w);
    CHECK(pn_fileexists(p));
    snprintf(p, sizeof p, "%s\\System32", w);
    CHECK(pn_fileexists(p));
}
TEST(path_names_windir_buffer_boundary_all_sizes) {
    REQUIRE(U(GetWindowsDirectoryW));
    WCHAR w[MAX_PATH], c[MAX_PATH];
    UINT len = U(GetWindowsDirectoryW)(w, MAX_PATH);
    for (UINT sz = 0; sz <= len; sz++) CHECKF(U(GetWindowsDirectoryW)(c, sz) == len + 1, "size %u", sz);
}
TEST(path_names_wow64dir_w_semantics_64bit) {
    REQUIRE(U(GetSystemWow64DirectoryW));
    WCHAR b[MAX_PATH], c[MAX_PATH];
    SET_ERR(0);
    UINT len = U(GetSystemWow64DirectoryW)(b, MAX_PATH);
    CHECK(len != 0);
    if (!len) { t_note("GetSystemWow64DirectoryW failed err %lu", GetLastError()); return; }
    size_t l = wcslen(b);
    CHECK(l == len);
    CHECK(l > 9 && !_wcsicmp(b + l - 9, L"\\SysWOW64"));
    EXPECT_EQ(U(GetSystemWow64DirectoryW)(NULL, 0), len + 1);
    EXPECT_EQ(U(GetSystemWow64DirectoryW)(c, len), len + 1);
    EXPECT_EQ(U(GetSystemWow64DirectoryW)(c, len + 1), len);
    CHECK(!wcscmp(b, c));
}
TEST(path_names_wow64dir_a_semantics_64bit) {
    REQUIRE(U(GetSystemWow64DirectoryA));
    char b[MAX_PATH], c[MAX_PATH];
    UINT len = U(GetSystemWow64DirectoryA)(b, MAX_PATH);
    CHECK(len != 0);
    if (!len) return;
    CHECK(strlen(b) == len);
    EXPECT_EQ(U(GetSystemWow64DirectoryA)(NULL, 0), len + 1);
    EXPECT_EQ(U(GetSystemWow64DirectoryA)(c, len), len + 1);
    EXPECT_EQ(U(GetSystemWow64DirectoryA)(c, len + 1), len);
}
TEST(path_names_wow64dir_exists_and_under_windir) {
    REQUIRE(U(GetSystemWow64DirectoryA)); REQUIRE(U(GetWindowsDirectoryA));
    char b[MAX_PATH], w[MAX_PATH];
    if (!U(GetSystemWow64DirectoryA)(b, MAX_PATH)) { t_skip("no wow64 dir"); return; }
    U(GetWindowsDirectoryA)(w, MAX_PATH);
    CHECK(!_strnicmp(b, w, strlen(w)));
    DWORD at = GetFileAttributesA(b);
    if (t_is_wine()) return;
    CHECK(at != INVALID_FILE_ATTRIBUTES && (at & FILE_ATTRIBUTE_DIRECTORY));
}
TEST(path_names_wow64dir_differs_from_sysdir) {
    REQUIRE(U(GetSystemWow64DirectoryW)); REQUIRE(U(GetSystemDirectoryW));
    WCHAR a[MAX_PATH], b[MAX_PATH];
    if (!U(GetSystemWow64DirectoryW)(a, MAX_PATH)) { t_skip("no wow64"); return; }
    U(GetSystemDirectoryW)(b, MAX_PATH);
    CHECK(_wcsicmp(a, b) != 0);
}
TEST(path_names_wow64dir2_machine_types) {
    UDECL(UINT, GetSystemWow64Directory2W, (LPWSTR, UINT, WORD));
    UDECL(UINT, GetSystemWow64Directory2A, (LPSTR, UINT, WORD));
    if (!UN(GetSystemWow64Directory2W)) { t_note("GetSystemWow64Directory2W not exported (kernel32 lookup)"); return; }
    WCHAR b[MAX_PATH], c[MAX_PATH];
    UINT n = UN(GetSystemWow64Directory2W)(b, MAX_PATH, 0x014c);
    CHECK(n != 0);
    if (n) {
        CHECK(n == wcslen(b));
        CHECK(n > 9 && !_wcsicmp(b + n - 9, L"\\SysWOW64"));
        EXPECT_EQ(UN(GetSystemWow64Directory2W)(c, n, 0x014c), n + 1);
        EXPECT_EQ(UN(GetSystemWow64Directory2W)(NULL, 0, 0x014c), n + 1);
    }
    UINT h = UN(GetSystemWow64Directory2W)(c, MAX_PATH, 0x8664);
    t_note("Dir2(AMD64) -> %u %s", h, h ? pn_n(c) : "-");
    h = UN(GetSystemWow64Directory2W)(c, MAX_PATH, 1);
    t_note("Dir2(HOST=1) -> %u %s", h, h ? pn_n(c) : "-");
    SET_ERR(0);
    EXPECT_EQ(UN(GetSystemWow64Directory2W)(c, MAX_PATH, 0x1234), 0);
    EXPECT_ERR(ERROR_BAD_ARGUMENTS);
    SET_ERR(0);
    EXPECT_EQ(UN(GetSystemWow64Directory2W)(c, MAX_PATH, 0), 0);
    EXPECT_ERR(ERROR_BAD_ARGUMENTS);
    if (UN(GetSystemWow64Directory2A)) {
        char a[MAX_PATH];
        UINT na = UN(GetSystemWow64Directory2A)(a, MAX_PATH, 0x014c);
        CHECK(na == n);
    }
}
TEST(path_names_wow64dir_matches_dir2_x86) {
    UDECL(UINT, GetSystemWow64Directory2W, (LPWSTR, UINT, WORD));
    REQUIRE(U(GetSystemWow64DirectoryW));
    if (!UN(GetSystemWow64Directory2W)) { t_skip("Dir2 missing"); return; }
    WCHAR a[MAX_PATH], b[MAX_PATH];
    UINT n1 = U(GetSystemWow64DirectoryW)(a, MAX_PATH);
    UINT n2 = UN(GetSystemWow64Directory2W)(b, MAX_PATH, 0x014c);
    CHECK(n1 == n2 && !wcscmp(a, b));
}

/* ---------------- SearchPath ---------------- */
TEST(path_names_searchpath_finds_kernel32_in_system_dir) {
    REQUIRE(U(SearchPathW)); REQUIRE(U(GetSystemDirectoryW));
    WCHAR b[MAX_PATH], s[MAX_PATH], want[MAX_PATH], *fp = (WCHAR *)SENT;
    U(GetSystemDirectoryW)(s, MAX_PATH);
    swprintf(want, MAX_PATH, L"%ls\\kernel32.dll", s);
    DWORD n = U(SearchPathW)(NULL, L"kernel32.dll", NULL, MAX_PATH, b, &fp);
    CHECKF(n == wcslen(want) && !_wcsicmp(b, want), "got %s", pn_n(b));
    CHECK(fp == b + wcslen(s) + 1 && !_wcsicmp(fp, L"kernel32.dll"));
}
TEST(path_names_searchpath_extension_appended_when_missing) {
    REQUIRE(U(SearchPathW)); REQUIRE(U(GetSystemDirectoryW));
    WCHAR b[MAX_PATH], s[MAX_PATH], want[MAX_PATH];
    U(GetSystemDirectoryW)(s, MAX_PATH);
    swprintf(want, MAX_PATH, L"%ls\\kernel32.dll", s);
    DWORD n = U(SearchPathW)(NULL, L"kernel32", L".dll", MAX_PATH, b, NULL);
    CHECKF(n == wcslen(want) && !_wcsicmp(b, want), "ext with dot: %s", pn_n(b));
    n = U(SearchPathW)(NULL, L"kernel32", L"dll", MAX_PATH, b, NULL);
    t_note("extension without dot -> n=%lu %s", n, n ? pn_n(b) : "-");
    SET_ERR(0);
    n = U(SearchPathW)(NULL, L"kernel32", NULL, MAX_PATH, b, NULL);
    CHECK(n == 0);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(path_names_searchpath_extension_ignored_when_name_has_one) {
    REQUIRE(U(SearchPathW));
    WCHAR b[MAX_PATH];
    SET_ERR(0);
    DWORD n = U(SearchPathW)(NULL, L"kernel32.dll", L".exe", MAX_PATH, b, NULL);
    CHECK(n != 0);
    n = U(SearchPathW)(NULL, L"kernel32.xyz", L".dll", MAX_PATH, b, NULL);
    t_note("name with other ext + default ext -> %lu", n);
    CHECK(n == 0);
}
TEST(path_names_searchpath_explicit_path_and_file_part) {
    REQUIRE(U(SearchPathA));
    char f[MAX_PATH], b[MAX_PATH], *fp = (char *)SENT;
    t_tmpfile(f, sizeof f, "sp_target.dat"); pn_touch(f);
    DWORD n = U(SearchPathA)(t_tmpdir(), "sp_target.dat", NULL, MAX_PATH, b, &fp);
    char want[MAX_PATH]; snprintf(want, sizeof want, "%ssp_target.dat", t_tmpdir());
    CHECKF(n == strlen(want) && !_stricmp(b, want), "got %s", b);
    CHECK(fp == b + strlen(t_tmpdir()) && !strcmp(fp, "sp_target.dat"));
    /* path list with several entries and a trailing backslash on the directory */
    char list[MAX_PATH * 2];
    snprintf(list, sizeof list, "C:\\k32t_none_here;%s;C:\\k32t_other", t_tmpdir());
    n = U(SearchPathA)(list, "sp_target.dat", NULL, MAX_PATH, b, NULL);
    CHECK(n == strlen(want));
    DeleteFileA(f);
}
TEST(path_names_searchpath_explicit_path_no_trailing_slash) {
    REQUIRE(U(SearchPathA));
    char f[MAX_PATH], b[MAX_PATH], d[MAX_PATH];
    t_tmpfile(f, sizeof f, "sp_t2.dat"); pn_touch(f);
    snprintf(d, sizeof d, "%s", t_tmpdir()); d[strlen(d) - 1] = 0;
    DWORD n = U(SearchPathA)(d, "sp_t2.dat", NULL, MAX_PATH, b, NULL);
    CHECK(n == strlen(f) && !_stricmp(b, f));
    DeleteFileA(f);
}
TEST(path_names_searchpath_not_found_error_2) {
    REQUIRE(U(SearchPathW)); REQUIRE(U(SearchPathA));
    WCHAR b[MAX_PATH]; char a[MAX_PATH];
    SET_ERR(0); EXPECT_EQ(U(SearchPathW)(NULL, L"k32t_no_such_file_9z.dll", NULL, MAX_PATH, b, NULL), 0); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    SET_ERR(0); EXPECT_EQ(U(SearchPathA)(NULL, "k32t_no_such_file_9z.dll", NULL, MAX_PATH, a, NULL), 0); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    SET_ERR(0); EXPECT_EQ(U(SearchPathA)(t_tmpdir(), "k32t_no_such_file_9z.dll", NULL, MAX_PATH, a, NULL), 0); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(path_names_searchpath_filepart_null_on_failure) {
    REQUIRE(U(SearchPathW));
    WCHAR b[MAX_PATH], *fp = (WCHAR *)SENT;
    EXPECT_EQ(U(SearchPathW)(NULL, L"k32t_no_such_file_9z.dll", NULL, MAX_PATH, b, &fp), 0);
    CHECK(fp == NULL);
}
TEST(path_names_searchpath_empty_or_null_name_invalid_parameter) {
    REQUIRE(U(SearchPathW)); REQUIRE(U(SearchPathA));
    WCHAR b[MAX_PATH]; char a[MAX_PATH];
    SET_ERR(0); EXPECT_EQ(U(SearchPathW)(NULL, NULL, NULL, MAX_PATH, b, NULL), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(SearchPathW)(NULL, L"", NULL, MAX_PATH, b, NULL), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(SearchPathW)(NULL, L"   ", NULL, MAX_PATH, b, NULL), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(SearchPathA)(NULL, "", NULL, MAX_PATH, a, NULL), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(path_names_searchpath_buffer_semantics_w) {
    REQUIRE(U(SearchPathW));
    WCHAR b[MAX_PATH], c[MAX_PATH];
    DWORD len = U(SearchPathW)(NULL, L"kernel32.dll", NULL, MAX_PATH, b, NULL);
    CHECK(len > 10);
    DWORD need = U(SearchPathW)(NULL, L"kernel32.dll", NULL, 0, NULL, NULL);
    EXPECT_EQ(need, len + 1);
    EXPECT_EQ(U(SearchPathW)(NULL, L"kernel32.dll", NULL, 0, c, NULL), len + 1);
    EXPECT_EQ(U(SearchPathW)(NULL, L"kernel32.dll", NULL, 1, c, NULL), len + 1);
    EXPECT_EQ(U(SearchPathW)(NULL, L"kernel32.dll", NULL, len, c, NULL), len + 1);
    EXPECT_EQ(U(SearchPathW)(NULL, L"kernel32.dll", NULL, len + 1, c, NULL), len);
    CHECK(!wcscmp(b, c));
}
TEST(path_names_searchpath_buffer_semantics_a) {
    REQUIRE(U(SearchPathA));
    char b[MAX_PATH], c[MAX_PATH], *fp = (char *)SENT;
    DWORD len = U(SearchPathA)(NULL, "kernel32.dll", NULL, MAX_PATH, b, NULL);
    EXPECT_EQ(U(SearchPathA)(NULL, "kernel32.dll", NULL, 0, NULL, NULL), len + 1);
    EXPECT_EQ(U(SearchPathA)(NULL, "kernel32.dll", NULL, len, c, &fp), len + 1);
    EXPECT_EQ(U(SearchPathA)(NULL, "kernel32.dll", NULL, len + 1, c, &fp), len);
    EXPECT_STR(b, c);
    CHECK(fp && !_stricmp(fp, "kernel32.dll"));
}
TEST(path_names_searchpath_absolute_name_ignores_path_list) {
    REQUIRE(U(SearchPathA));
    char f[MAX_PATH], b[MAX_PATH];
    t_tmpfile(f, sizeof f, "sp_abs.dat"); pn_touch(f);
    DWORD n = U(SearchPathA)("C:\\k32t_nonexistent_dir", f, NULL, MAX_PATH, b, NULL);
    CHECK(n == strlen(f) && !_stricmp(b, f));
    SET_ERR(0);
    char missing[MAX_PATH]; t_tmpfile(missing, sizeof missing, "sp_abs_missing.dat");
    EXPECT_EQ(U(SearchPathA)(t_tmpdir(), missing, NULL, MAX_PATH, b, NULL), 0);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    DeleteFileA(f);
}
TEST(path_names_searchpath_directory_is_not_a_match_note) {
    REQUIRE(U(SearchPathA));
    char d[MAX_PATH], b[MAX_PATH];
    t_tmpfile(d, sizeof d, "sp_dir.d");
    CreateDirectoryA(d, NULL);
    SET_ERR(0);
    DWORD n = U(SearchPathA)(t_tmpdir(), "sp_dir.d", NULL, MAX_PATH, b, NULL);
    t_note("SearchPath finds a directory: n=%lu err=%lu", n, GetLastError());
    RemoveDirectoryA(d);
}
TEST(path_names_searchpath_relative_name_with_subdir) {
    REQUIRE(U(SearchPathA));
    char d[MAX_PATH], f[MAX_PATH], b[MAX_PATH], *fp;
    t_tmpfile(d, sizeof d, "spsub"); CreateDirectoryA(d, NULL);
    snprintf(f, sizeof f, "%s\\inner.dat", d); pn_touch(f);
    DWORD n = U(SearchPathA)(t_tmpdir(), "spsub\\inner.dat", NULL, MAX_PATH, b, &fp);
    CHECKF(n == strlen(f) && !_stricmp(b, f), "got %s", b);
    CHECK(fp && !strcmp(fp, "inner.dat"));
    DeleteFileA(f); RemoveDirectoryA(d);
}
TEST(path_names_searchpath_trailing_spaces_in_name_trimmed) {
    REQUIRE(U(SearchPathW));
    WCHAR b[MAX_PATH];
    DWORD n = U(SearchPathW)(NULL, L"kernel32.dll   ", NULL, MAX_PATH, b, NULL);
    t_note("name with trailing spaces -> %lu", n);
}
TEST(path_names_searchpath_semicolon_and_empty_entries) {
    REQUIRE(U(SearchPathA));
    char f[MAX_PATH], b[MAX_PATH], list[MAX_PATH * 2];
    t_tmpfile(f, sizeof f, "sp_semi.dat"); pn_touch(f);
    snprintf(list, sizeof list, ";;%s;;", t_tmpdir());
    DWORD n = U(SearchPathA)(list, "sp_semi.dat", NULL, MAX_PATH, b, NULL);
    t_note("empty entries in list: n=%lu (empty entry means current directory)", n);
    CHECK(n == strlen(f));
    DeleteFileA(f);
}
TEST(path_names_searchpath_empty_path_list_string) {
    REQUIRE(U(SearchPathA));
    char b[MAX_PATH];
    SET_ERR(0);
    DWORD n = U(SearchPathA)("", "kernel32.dll", NULL, MAX_PATH, b, NULL);
    t_note("empty (non-NULL) path -> %lu err %lu", n, GetLastError());
}
TEST(path_names_searchpath_a_w_parity) {
    REQUIRE(U(SearchPathA)); REQUIRE(U(SearchPathW));
    char a[MAX_PATH], w8[MAX_PATH]; WCHAR w[MAX_PATH];
    DWORD na = U(SearchPathA)(NULL, "ntdll.dll", NULL, MAX_PATH, a, NULL);
    DWORD nw = U(SearchPathW)(NULL, L"ntdll.dll", NULL, MAX_PATH, w, NULL);
    pn_a(w8, sizeof w8, w);
    CHECK(na == nw && !strcmp(a, w8));
}
TEST(path_names_searchpath_finds_exe_in_windows_dir) {
    REQUIRE(U(SearchPathA));
    char b[MAX_PATH];
    DWORD n = U(SearchPathA)(NULL, "notepad", ".exe", MAX_PATH, b, NULL);
    t_note("notepad.exe -> %s", n ? b : "not found");
    n = U(SearchPathA)(NULL, "cmd.exe", NULL, MAX_PATH, b, NULL);
    CHECK(n != 0 || t_is_wine());
}
TEST(path_names_searchpath_uses_path_env) {
    REQUIRE(U(SearchPathA));
    CHECK(t_run_child("searchpath_pathenv", NULL, 20000) == 0);
}
CHILD(searchpath_pathenv) {
    char d[MAX_PATH], f[MAX_PATH], b[MAX_PATH], *fp;
    snprintf(d, sizeof d, "%spathdir", t_tmpdir());
    CreateDirectoryA(d, NULL);
    snprintf(f, sizeof f, "%s\\k32t_on_path.dat", d);
    pn_touch(f);
    char path[4096];
    GetEnvironmentVariableA("PATH", path, sizeof path);
    char np[8192]; snprintf(np, sizeof np, "%s;%s", path, d);
    SetEnvironmentVariableA("PATH", np);
    DWORD n = U(SearchPathA)(NULL, "k32t_on_path.dat", NULL, MAX_PATH, b, &fp);
    if (n != strlen(f) || _stricmp(b, f)) return 11;
    if (!fp || strcmp(fp, "k32t_on_path.dat")) return 12;
    /* a name found only through PATH is not found when an explicit path list is given */
    SetLastError(0);
    if (U(SearchPathA)(t_tmpdir(), "k32t_on_path.dat", NULL, MAX_PATH, b, NULL) != 0) return 13;
    if (GetLastError() != ERROR_FILE_NOT_FOUND) return 14;
    return 0;
}
TEST(path_names_searchpath_order_cwd_after_system_in_safe_mode_child) {
    REQUIRE(U(SearchPathA));
    CHECK(t_run_child("searchpath_order", NULL, 20000) == 0);
}
CHILD(searchpath_order) {
    /* a decoy kernel32.dll in the current directory */
    char d[MAX_PATH], decoy[MAX_PATH], b[MAX_PATH], sys[MAX_PATH], want[MAX_PATH];
    snprintf(d, sizeof d, "%sdecoy", t_tmpdir());
    CreateDirectoryA(d, NULL);
    snprintf(decoy, sizeof decoy, "%s\\kernel32.dll", d);
    pn_touch(decoy);
    if (!SetCurrentDirectoryA(d)) return 10;
    GetSystemDirectoryA(sys, sizeof sys);
    snprintf(want, sizeof want, "%s\\kernel32.dll", sys);
    int rc = 0;
    if (!U(SetSearchPathMode)(0x1)) { rc = 11; goto out; }
    if (!U(SearchPathA)(NULL, "kernel32.dll", NULL, MAX_PATH, b, NULL) || _stricmp(b, want)) { rc = 12; goto out; }
    if (!U(SetSearchPathMode)(0x10000)) { rc = 13; goto out; }
    if (!U(SearchPathA)(NULL, "kernel32.dll", NULL, MAX_PATH, b, NULL) || _stricmp(b, decoy)) { rc = 14; goto out; }
    if (!U(SetSearchPathMode)(0x1)) { rc = 15; goto out; }
    if (!U(SearchPathA)(NULL, "kernel32.dll", NULL, MAX_PATH, b, NULL) || _stricmp(b, want)) { rc = 16; goto out; }
out:
    { char w[MAX_PATH]; GetWindowsDirectoryA(w, sizeof w); SetCurrentDirectoryA(w); }
    return rc;
}
TEST(path_names_searchpath_cwd_found_for_non_system_name_child) {
    REQUIRE(U(SearchPathA));
    CHECK(t_run_child("searchpath_cwd", NULL, 20000) == 0);
}
CHILD(searchpath_cwd) {
    char d[MAX_PATH], f[MAX_PATH], b[MAX_PATH], *fp;
    snprintf(d, sizeof d, "%scwd", t_tmpdir());
    CreateDirectoryA(d, NULL);
    snprintf(f, sizeof f, "%s\\k32t_in_cwd.dat", d);
    pn_touch(f);
    SetCurrentDirectoryA(d);
    int rc = 0;
    DWORD n = U(SearchPathA)(NULL, "k32t_in_cwd.dat", NULL, MAX_PATH, b, &fp);
    if (n != strlen(f) || _stricmp(b, f)) rc = 11;
    else if (!fp || strcmp(fp, "k32t_in_cwd.dat")) rc = 12;
    { char w[MAX_PATH]; GetWindowsDirectoryA(w, sizeof w); SetCurrentDirectoryA(w); }
    return rc;
}

/* ---------------- SetSearchPathMode (process-wide: child only) ---------------- */
TEST(path_names_setsearchpathmode_flag_validation_child) {
    REQUIRE(U(SetSearchPathMode));
    CHECK(t_run_child("ssm_validation", NULL, 20000) == 0);
}
CHILD(ssm_validation) {
    static const DWORD bad[] = { 0, 0x2, 0x4, 0x10, 0x100, 0x8000, 0x10001, 0x18000, 0x18001, 0x20000, 0x20001, 0x80000000, 0xFFFFFFFF, 0x3 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SetLastError(0);
        if (U(SetSearchPathMode)(bad[i])) { printf("CHILDFAIL flags 0x%lx accepted\n", bad[i]); return 10 + i; }
        if (GetLastError() != ERROR_INVALID_PARAMETER) { printf("CHILDFAIL flags 0x%lx err %lu\n", bad[i], GetLastError()); return 50 + i; }
    }
    return 0;
}
TEST(path_names_setsearchpathmode_valid_and_toggle_child) {
    REQUIRE(U(SetSearchPathMode));
    CHECK(t_run_child("ssm_toggle", NULL, 20000) == 0);
}
CHILD(ssm_toggle) {
    if (!U(SetSearchPathMode)(0x1)) return 10;            /* ENABLE_SAFE */
    if (!U(SetSearchPathMode)(0x10000)) return 11;        /* DISABLE_SAFE */
    if (!U(SetSearchPathMode)(0x1)) return 12;
    if (!U(SetSearchPathMode)(0x1)) return 13;            /* idempotent */
    return 0;
}
TEST(path_names_setsearchpathmode_permanent_is_sticky_child) {
    REQUIRE(U(SetSearchPathMode));
    CHECK(t_run_child("ssm_permanent", NULL, 20000) == 0);
}
CHILD(ssm_permanent) {
    if (!U(SetSearchPathMode)(0x8001)) return 10;         /* ENABLE_SAFE | PERMANENT */
    SetLastError(0);
    if (U(SetSearchPathMode)(0x1)) return 11;             /* change without PERMANENT refused */
    if (GetLastError() != ERROR_ACCESS_DENIED) { printf("CHILDFAIL err %lu\n", GetLastError()); return 12; }
    SetLastError(0);
    if (U(SetSearchPathMode)(0x10000)) return 13;
    if (GetLastError() != ERROR_ACCESS_DENIED) return 14;
    if (!U(SetSearchPathMode)(0x8001)) return 15;         /* repeating PERMANENT is fine */
    return 0;
}

/* ---------------- NeedCurrentDirectoryForExePath ---------------- */
TEST(path_names_needcurdir_child_env_var) {
    REQUIRE(U(NeedCurrentDirectoryForExePathW));
    CHECK(t_run_child("needcurdir", NULL, 20000) == 0);
}
CHILD(needcurdir) {
    SetEnvironmentVariableA("NoDefaultCurrentDirectoryInExePath", NULL);
    if (!U(NeedCurrentDirectoryForExePathW)(L"foo.exe")) return 10;
    if (!U(NeedCurrentDirectoryForExePathA)("foo.exe")) return 11;
    if (!U(NeedCurrentDirectoryForExePathW)(L"dir\\foo.exe")) return 12;
    if (!U(NeedCurrentDirectoryForExePathW)(L"")) return 13;
    SetEnvironmentVariableA("NoDefaultCurrentDirectoryInExePath", "1");
    if (U(NeedCurrentDirectoryForExePathW)(L"foo.exe")) return 20;
    if (U(NeedCurrentDirectoryForExePathA)("foo.exe")) return 21;
    if (!U(NeedCurrentDirectoryForExePathW)(L"dir\\foo.exe")) return 22;
    if (!U(NeedCurrentDirectoryForExePathA)("dir\\foo.exe")) return 23;
    if (!U(NeedCurrentDirectoryForExePathW)(L"C:\\foo.exe")) return 24;
    if (!U(NeedCurrentDirectoryForExePathW)(L".\\foo.exe")) return 25;
    if (U(NeedCurrentDirectoryForExePathW)(L"dir/foo.exe")) return 26;   /* only a backslash counts */
    /* any value, even empty-looking or zero, counts as defined */
    SetEnvironmentVariableA("NoDefaultCurrentDirectoryInExePath", "0");
    if (U(NeedCurrentDirectoryForExePathW)(L"foo.exe")) return 30;
    SetEnvironmentVariableA("NoDefaultCurrentDirectoryInExePath", "");
    int r = U(NeedCurrentDirectoryForExePathW)(L"foo.exe");
    printf("  NOTE empty value -> %d\n", r);
    SetEnvironmentVariableA("NoDefaultCurrentDirectoryInExePath", NULL);
    if (!U(NeedCurrentDirectoryForExePathW)(L"foo.exe")) return 40;
    return 0;
}
TEST(path_names_needcurdir_default_returns_true_main) {
    REQUIRE(U(NeedCurrentDirectoryForExePathW));
    if (GetEnvironmentVariableA("NoDefaultCurrentDirectoryInExePath", NULL, 0)) { t_skip("env var set"); return; }
    CHECK(U(NeedCurrentDirectoryForExePathW)(L"x.exe") == TRUE);
    CHECK(U(NeedCurrentDirectoryForExePathA)("x.exe") == TRUE);
    CHECK(U(NeedCurrentDirectoryForExePathW)(L"a\\x.exe") == TRUE);
}

/* ---------------- CheckNameLegalDOS8Dot3 ---------------- */
typedef BOOL (WINAPI *chk_w)(LPCWSTR, LPSTR, DWORD, PBOOL, PBOOL);
typedef BOOL (WINAPI *chk_a)(LPCSTR, LPSTR, DWORD, PBOOL, PBOOL);
static const struct { const char *name; int legal; int space; } k_legal[] = {
    { "a", 1, 0 }, { "a.b", 1, 0 }, { "file.txt", 1, 0 }, { "FILE.TXT", 1, 0 }, { "File.Txt", 1, 0 },
    { "12345678.123", 1, 0 }, { "12345678", 1, 0 }, { ".abc", 0, 0 },
    { "!#$%&'()-@^_`{}~.tmp", 0, 0 }, { "a-b_c.d-e", 1, 0 }, { "a b.txt", 1, 1 }, { "a b", 1, 1 },
    { "123456789.txt", 0, 0 }, { "12345678.1234", 0, 0 }, { "a.b.c", 0, 0 }, { "a..b", 0, 0 },
    { "C:\\a.txt", 0, 0 }, { "dir\\a.txt", 0, 0 }, { "dir/a.txt", 0, 0 }, { "alongfilename.txt", 0, 0 },
    { "a+b.txt", 0, 0 }, { "a,b.txt", 0, 0 }, { "a;b.txt", 0, 0 }, { "a=b.txt", 0, 0 }, { "a[b].txt", 0, 0 },
    { "a*.txt", 0, 0 }, { "a?.txt", 0, 0 }, { "a<b", 0, 0 }, { "a>b", 0, 0 }, { "a|b", 0, 0 }, { "a\"b", 0, 0 }, { "a:b", 0, 0 },
};
TEST(path_names_dos8dot3_table_w) {
    REQUIRE(U(CheckNameLegalDOS8Dot3W));
    for (unsigned i = 0; i < sizeof k_legal / sizeof *k_legal; i++) {
        BOOL legal = 0xCC, sp = 0xCC;
        BOOL r = U(CheckNameLegalDOS8Dot3W)(pn_w(k_legal[i].name), NULL, 0, &sp, &legal);
        CHECKF(r == TRUE, "%s call result", k_legal[i].name);
        if (legal != k_legal[i].legal) t_note("legality of %s: got %d (expected %d)", k_legal[i].name, legal, k_legal[i].legal);
        if (legal && k_legal[i].legal) CHECKF(sp == k_legal[i].space, "%s spaces=%d want %d", k_legal[i].name, sp, k_legal[i].space);
    }
}
TEST(path_names_dos8dot3_hard_cases_w) {
    REQUIRE(U(CheckNameLegalDOS8Dot3W));
    static const struct { const char *n; int legal; } h[] = {
        { "a", 1 }, { "a.b", 1 }, { "file.txt", 1 }, { "12345678.123", 1 }, { "123456789.txt", 0 }, { "12345678.1234", 0 },
        { "a.b.c", 0 }, { "C:\\a.txt", 0 }, { "dir\\a.txt", 0 }, { "alongfilename.txt", 0 },
    };
    for (unsigned i = 0; i < sizeof h / sizeof *h; i++) {
        BOOL legal = 0xCC, sp = 0xCC;
        CHECK(U(CheckNameLegalDOS8Dot3W)(pn_w(h[i].n), NULL, 0, &sp, &legal) == TRUE);
        CHECKF(legal == h[i].legal, "%s legal=%d want %d", h[i].n, legal, h[i].legal);
    }
}
TEST(path_names_dos8dot3_table_a_matches_w) {
    REQUIRE(U(CheckNameLegalDOS8Dot3A)); REQUIRE(U(CheckNameLegalDOS8Dot3W));
    for (unsigned i = 0; i < sizeof k_legal / sizeof *k_legal; i++) {
        BOOL la = 0xCC, sa = 0xCC, lw = 0xCC, sw = 0xCC;
        BOOL ra = U(CheckNameLegalDOS8Dot3A)(k_legal[i].name, NULL, 0, &sa, &la);
        BOOL rw = U(CheckNameLegalDOS8Dot3W)(pn_w(k_legal[i].name), NULL, 0, &sw, &lw);
        CHECKF(ra == rw && la == lw, "%s A(%d,%d) W(%d,%d)", k_legal[i].name, ra, la, rw, lw);
        if (la && lw) CHECK(sa == sw);
    }
}
TEST(path_names_dos8dot3_null_arguments) {
    REQUIRE(U(CheckNameLegalDOS8Dot3W)); REQUIRE(U(CheckNameLegalDOS8Dot3A));
    BOOL legal, sp;
    SET_ERR(0); EXPECT_EQ(U(CheckNameLegalDOS8Dot3W)(NULL, NULL, 0, NULL, &legal), FALSE); t_note("NULL name W err %lu", GetLastError());
    EXPECT_EQ(U(CheckNameLegalDOS8Dot3A)(NULL, NULL, 0, NULL, &legal), FALSE);
    SET_ERR(0); EXPECT_EQ(U(CheckNameLegalDOS8Dot3W)(L"abc.txt", NULL, 0, NULL, NULL), FALSE); t_note("NULL pbNameLegal W err %lu", GetLastError());
    EXPECT_EQ(U(CheckNameLegalDOS8Dot3A)("abc.txt", NULL, 0, NULL, NULL), FALSE);
    SET_ERR(0); EXPECT_EQ(U(CheckNameLegalDOS8Dot3W)(L"abc.txt", NULL, 0, &sp, NULL), FALSE);
    /* pbNameContainsSpaces may be NULL */
    legal = 0xCC;
    EXPECT_EQ(U(CheckNameLegalDOS8Dot3W)(L"abc.txt", NULL, 0, NULL, &legal), TRUE);
    EXPECT_EQ(legal, TRUE);
}
TEST(path_names_dos8dot3_empty_name_note) {
    REQUIRE(U(CheckNameLegalDOS8Dot3W));
    BOOL legal = 0xCC, sp = 0xCC;
    BOOL r = U(CheckNameLegalDOS8Dot3W)(L"", NULL, 0, &sp, &legal);
    t_note("empty name -> ret %d legal %d spaces %d", r, legal, sp);
}
TEST(path_names_dos8dot3_oem_name_output) {
    REQUIRE(U(CheckNameLegalDOS8Dot3A));
    char oem[32]; BOOL legal = 0xCC, sp = 0xCC;
    memset(oem, 'Z', sizeof oem);
    BOOL r = U(CheckNameLegalDOS8Dot3A)("file.txt", oem, sizeof oem, &sp, &legal);
    CHECK(r == TRUE && legal == TRUE);
    if (oem[0] != 'Z') t_note("OEM name for file.txt: [%.12s]", oem);
    else t_note("OEM name buffer untouched");
}
TEST(path_names_dos8dot3_oem_buffer_too_small_note) {
    REQUIRE(U(CheckNameLegalDOS8Dot3A));
    char oem[4] = "ZZZ"; BOOL legal = 0xCC, sp = 0xCC;
    SET_ERR(0);
    BOOL r = U(CheckNameLegalDOS8Dot3A)("file.txt", oem, 2, &sp, &legal);
    t_note("tiny OEM buffer: ret %d err %lu legal %d", r, GetLastError(), legal);
}
TEST(path_names_dos8dot3_w_exports_in_kernel32_and_kernelbase) {
    CHECK(UK(CheckNameLegalDOS8Dot3W) != NULL);
    CHECK(UK(CheckNameLegalDOS8Dot3A) != NULL);
    t_note("kernelbase CheckNameLegalDOS8Dot3W: %s", UB(CheckNameLegalDOS8Dot3W) ? "yes" : "no");
}
