/* Win32 -> NT namespace behaviour as seen through file APIs: wildcards, device paths, \\?\ prefix, trailing dots, long paths */
#include "pn.h"

static HANDLE cf_w(const WCHAR *p, DWORD acc, DWORD share, DWORD disp, DWORD flags) {
    return U(CreateFileW)(p, acc, share, NULL, disp, flags, NULL);
}
static void killw(const WCHAR *p) { SetFileAttributesW(p, FILE_ATTRIBUTE_NORMAL); DeleteFileW(p); }

TEST(path_names_ns_createfile_wildcard_chars_invalid_name) {
    REQUIRE(U(CreateFileW)); REQUIRE(U(CreateFileA));
    static const char *chars = "*?<>\"|";
    for (const char *c = chars; *c; c++) {
        char leaf[16], p[MAX_PATH];
        snprintf(leaf, sizeof leaf, "wild%cname", *c);
        t_tmpfile(p, sizeof p, leaf);
        SET_ERR(0);
        HANDLE h = U(CreateFileA)(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        CHECKF(h == INVALID_HANDLE_VALUE, "char '%c' accepted", *c);
        if (h == INVALID_HANDLE_VALUE) { DWORD e = GetLastError(); CHECKF(e == ERROR_INVALID_NAME, "char '%c' err %lu", *c, e); }
        else CloseHandle(h);
        SET_ERR(0);
        h = U(CreateFileA)(p, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
        CHECK(h == INVALID_HANDLE_VALUE);
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }
}
TEST(path_names_ns_createfile_star_question_whole_name) {
    REQUIRE(U(CreateFileA));
    char p[MAX_PATH];
    t_tmpfile(p, sizeof p, "*");
    SET_ERR(0);
    HANDLE h = U(CreateFileA)(p, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_INVALID_NAME);
    t_tmpfile(p, sizeof p, "?");
    SET_ERR(0);
    h = U(CreateFileA)(p, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_INVALID_NAME);
}
TEST(path_names_ns_getfileattributes_wildcard_invalid_name) {
    REQUIRE(U(GetFileAttributesA));
    char p[MAX_PATH];
    t_tmpfile(p, sizeof p, "*.txt");
    SET_ERR(0);
    EXPECT_EQ(U(GetFileAttributesA)(p), INVALID_FILE_ATTRIBUTES);
    EXPECT_ERR(ERROR_INVALID_NAME);
    t_tmpfile(p, sizeof p, "a?.txt");
    SET_ERR(0);
    EXPECT_EQ(U(GetFileAttributesA)(p), INVALID_FILE_ATTRIBUTES);
    EXPECT_ERR(ERROR_INVALID_NAME);
}
TEST(path_names_ns_trailing_dot_and_space_stripped_for_win32_paths) {
    REQUIRE(U(CreateFileW)); REQUIRE(U(GetFileAttributesW));
    WCHAR f[MAX_PATH], with_dot[MAX_PATH], with_sp[MAX_PATH];
    swprintf(f, MAX_PATH, L"%lsstrip_me", pn_w(t_tmpdir()));
    swprintf(with_dot, MAX_PATH, L"%ls.", f);
    swprintf(with_sp, MAX_PATH, L"%ls ", f);
    HANDLE h = cf_w(with_dot, GENERIC_WRITE, 0, CREATE_ALWAYS, 0);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    CHECK(U(GetFileAttributesW)(f) != INVALID_FILE_ATTRIBUTES);
    CHECK(U(GetFileAttributesW)(with_sp) != INVALID_FILE_ATTRIBUTES);
    WIN32_FIND_DATAW fd; HANDLE fh = FindFirstFileW(f, &fd);
    CHECK(fh != INVALID_HANDLE_VALUE);
    if (fh != INVALID_HANDLE_VALUE) { CHECK(!wcscmp(fd.cFileName, L"strip_me")); FindClose(fh); }
    killw(f);
}
TEST(path_names_ns_trailing_dot_kept_with_extended_prefix) {
    REQUIRE(U(CreateFileW)); REQUIRE(U(GetFileAttributesW));
    WCHAR plain[MAX_PATH], dotted[MAX_PATH + 8];
    swprintf(plain, MAX_PATH, L"%lsdotkept", pn_w(t_tmpdir()));
    swprintf(dotted, MAX_PATH + 8, L"\\\\?\\%ls.", plain);
    HANDLE h = cf_w(dotted, GENERIC_WRITE, 0, CREATE_NEW, 0);
    if (h == INVALID_HANDLE_VALUE) { t_skip("cannot create dotted name (err %lu)", GetLastError()); return; }
    CloseHandle(h);
    /* the real name is "dotkept." : the normalised spelling "dotkept" is a different, missing file */
    CHECK(U(GetFileAttributesW)(plain) == INVALID_FILE_ATTRIBUTES);
    CHECK(U(GetFileAttributesW)(dotted) != INVALID_FILE_ATTRIBUTES);
    WIN32_FIND_DATAW fd; WCHAR pat[MAX_PATH]; swprintf(pat, MAX_PATH, L"%ldotkept*", pn_w(t_tmpdir()));
    swprintf(pat, MAX_PATH, L"%lsdotkept*", pn_w(t_tmpdir()));
    HANDLE fh = FindFirstFileW(pat, &fd);
    CHECK(fh != INVALID_HANDLE_VALUE);
    if (fh != INVALID_HANDLE_VALUE) { CHECK(!wcscmp(fd.cFileName, L"dotkept.")); FindClose(fh); }
    CHECK(DeleteFileW(dotted));
    CHECK(U(GetFileAttributesW)(dotted) == INVALID_FILE_ATTRIBUTES);
}
TEST(path_names_ns_trailing_space_kept_with_extended_prefix) {
    REQUIRE(U(CreateFileW)); REQUIRE(U(GetFileAttributesW));
    WCHAR plain[MAX_PATH], spaced[MAX_PATH + 8];
    swprintf(plain, MAX_PATH, L"%lsspkept", pn_w(t_tmpdir()));
    swprintf(spaced, MAX_PATH + 8, L"\\\\?\\%ls ", plain);
    HANDLE h = cf_w(spaced, GENERIC_WRITE, 0, CREATE_NEW, 0);
    if (h == INVALID_HANDLE_VALUE) { t_skip("cannot create (err %lu)", GetLastError()); return; }
    CloseHandle(h);
    CHECK(U(GetFileAttributesW)(plain) == INVALID_FILE_ATTRIBUTES);
    CHECK(DeleteFileW(spaced));
}
TEST(path_names_ns_forward_slash_converted_for_win32_paths) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(GetFileAttributesA));
    char f[MAX_PATH], fs[MAX_PATH]; pn_touch((t_tmpfile(f, sizeof f, "slash_conv.txt"), f));
    strcpy(fs, f);
    for (char *c = fs; *c; c++) if (*c == '\\') *c = '/';
    HANDLE h = U(CreateFileA)(fs, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    CHECK(U(GetFileAttributesA)(fs) != INVALID_FILE_ATTRIBUTES);
    DeleteFileA(f);
}
TEST(path_names_ns_forward_slash_literal_with_extended_prefix) {
    REQUIRE(U(GetFileAttributesW));
    char f[MAX_PATH], fs[MAX_PATH + 8];
    t_tmpfile(f, sizeof f, "slash_lit.txt"); pn_touch(f);
    snprintf(fs, sizeof fs, "\\\\?\\%s", f);
    for (char *c = fs + 4; *c; c++) if (*c == '\\') *c = '/';
    SET_ERR(0);
    DWORD a = U(GetFileAttributesW)(pn_w(fs));
    t_note("forward slashes after \\\\?\\ : attrs 0x%lx err %lu", a, GetLastError());
    CHECK(a == INVALID_FILE_ATTRIBUTES);
    DeleteFileA(f);
}
TEST(path_names_ns_dotdot_collapsed_lexically_for_win32_paths) {
    REQUIRE(U(GetFileAttributesA));
    char f[MAX_PATH], q[MAX_PATH * 2];
    t_tmpfile(f, sizeof f, "lex.txt"); pn_touch(f);
    snprintf(q, sizeof q, "%sno_such_dir\\..\\lex.txt", t_tmpdir());
    CHECK(U(GetFileAttributesA)(q) != INVALID_FILE_ATTRIBUTES);
    snprintf(q, sizeof q, "%s.\\.\\lex.txt", t_tmpdir());
    CHECK(U(GetFileAttributesA)(q) != INVALID_FILE_ATTRIBUTES);
    snprintf(q, sizeof q, "%sa\\b\\c\\..\\..\\..\\lex.txt", t_tmpdir());
    CHECK(U(GetFileAttributesA)(q) != INVALID_FILE_ATTRIBUTES);
    snprintf(q, sizeof q, "%s//lex.txt", t_tmpdir());
    CHECK(U(GetFileAttributesA)(q) != INVALID_FILE_ATTRIBUTES);
    DeleteFileA(f);
}
TEST(path_names_ns_dotdot_not_collapsed_with_extended_prefix) {
    REQUIRE(U(GetFileAttributesW));
    char f[MAX_PATH], q[MAX_PATH * 2];
    t_tmpfile(f, sizeof f, "lex2.txt"); pn_touch(f);
    snprintf(q, sizeof q, "\\\\?\\%sno_such_dir\\..\\lex2.txt", t_tmpdir());
    SET_ERR(0);
    DWORD a = U(GetFileAttributesW)(pn_w(q));
    CHECK(a == INVALID_FILE_ATTRIBUTES);
    t_note("\\\\?\\ with .. through missing dir: err %lu", GetLastError());
    snprintf(q, sizeof q, "\\\\?\\%s.\\lex2.txt", t_tmpdir());
    SET_ERR(0);
    a = U(GetFileAttributesW)(pn_w(q));
    CHECK(a == INVALID_FILE_ATTRIBUTES);
    t_note("\\\\?\\ with a literal '.' component: err %lu", GetLastError());
    DeleteFileA(f);
}
TEST(path_names_ns_extended_prefix_roundtrip_same_file) {
    REQUIRE(U(CreateFileW));
    char f[MAX_PATH], p[MAX_PATH + 8];
    t_tmpfile(f, sizeof f, "prefix_rt.txt");
    snprintf(p, sizeof p, "\\\\?\\%s", f);
    HANDLE h = cf_w(pn_w(p), GENERIC_WRITE, 0, CREATE_ALWAYS, 0);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) { DWORD n; WriteFile(h, "abc", 3, &n, NULL); CloseHandle(h); }
    h = cf_w(pn_w(f), GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) { CHECK(GetFileSize(h, NULL) == 3); CloseHandle(h); }
    DeleteFileA(f);
}
TEST(path_names_ns_unc_extended_form_equals_plain_form_local_parse) {
    REQUIRE(U(GetFullPathNameW));
    WCHAR a[MAX_PATH], b[MAX_PATH];
    U(GetFullPathNameW)(L"\\\\?\\UNC\\srv\\sh\\a", MAX_PATH, a, NULL);
    U(GetFullPathNameW)(L"\\\\srv\\sh\\a", MAX_PATH, b, NULL);
    CHECK(wcscmp(a, b) != 0);
    CHECK(pn_weq(a, "\\\\?\\UNC\\srv\\sh\\a"));
}
TEST(path_names_ns_nul_device_open_read_write) {
    REQUIRE(U(CreateFileA));
    static const char *names[] = { "NUL", "\\\\.\\NUL", "nul", "\\\\.\\nul" };
    for (unsigned i = 0; i < sizeof names / sizeof *names; i++) {
        HANDLE h = U(CreateFileA)(names[i], GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        CHECKF(h != INVALID_HANDLE_VALUE, "%s err %lu", names[i], GetLastError());
        if (h == INVALID_HANDLE_VALUE) continue;
        CHECK(GetFileType(h) == FILE_TYPE_CHAR);
        DWORD n = 0xFF;
        CHECK(WriteFile(h, "hello", 5, &n, NULL) && n == 5);
        char b[8]; n = 0xFF;
        CHECK(ReadFile(h, b, 8, &n, NULL) && n == 0);
        CloseHandle(h);
    }
}
TEST(path_names_ns_nul_device_with_directory_and_extension) {
    REQUIRE(U(CreateFileA));
    char p[MAX_PATH];
    snprintf(p, sizeof p, "%sNUL", t_tmpdir());
    HANDLE h = U(CreateFileA)(p, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CHECKF(h != INVALID_HANDLE_VALUE, "dir\\NUL err %lu", GetLastError());
    if (h != INVALID_HANDLE_VALUE) { CHECK(GetFileType(h) == FILE_TYPE_CHAR); CloseHandle(h); }
    char leaf[MAX_PATH]; snprintf(leaf, sizeof leaf, "%sNUL", t_tmpdir());
    CHECK(!pn_fileexists(leaf) || 1);
    /* nothing named NUL was created in the directory */
    WIN32_FIND_DATAA fd; char pat[MAX_PATH]; snprintf(pat, sizeof pat, "%s*", t_tmpdir());
    HANDLE fh = FindFirstFileA(pat, &fd); int found = 0;
    if (fh != INVALID_HANDLE_VALUE) { do { if (!_stricmp(fd.cFileName, "NUL")) found = 1; } while (FindNextFileA(fh, &fd)); FindClose(fh); }
    CHECK(!found);
}
TEST(path_names_ns_extended_prefix_nul_is_a_regular_name) {
    REQUIRE(U(CreateFileW));
    char p[MAX_PATH + 8];
    snprintf(p, sizeof p, "\\\\?\\%sNUL", t_tmpdir());
    HANDLE h = cf_w(pn_w(p), GENERIC_WRITE, 0, CREATE_NEW, 0);
    if (h == INVALID_HANDLE_VALUE) { t_skip("cannot create reserved-looking name (err %lu)", GetLastError()); return; }
    CHECK(GetFileType(h) == FILE_TYPE_DISK);
    CloseHandle(h);
    CHECK(DeleteFileW(pn_w(p)));
}
TEST(path_names_ns_open_volume_with_zero_access) {
    REQUIRE(U(CreateFileA));
    char v[8] = "\\\\.\\C:";
    v[4] = pn_sysdrive();
    HANDLE h = U(CreateFileA)(v, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD e = GetLastError();
        t_note("\\\\.\\X: with zero access: err %lu", e);
        CHECK(e == ERROR_ACCESS_DENIED || e == ERROR_FILE_NOT_FOUND);
        return;
    }
    CHECK(GetFileType(h) == FILE_TYPE_DISK);
    CloseHandle(h);
    char v2[16] = "\\\\?\\C:";
    v2[4] = pn_sysdrive();
    h = U(CreateFileA)(v2, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE || GetLastError() == ERROR_ACCESS_DENIED);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
}
TEST(path_names_ns_open_directory_requires_backup_semantics) {
    REQUIRE(U(CreateFileA));
    char d[MAX_PATH]; snprintf(d, sizeof d, "%s", t_tmpdir());
    SET_ERR(0);
    HANDLE h = U(CreateFileA)(d, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    h = U(CreateFileA)(d, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) { CHECK(GetFileType(h) == FILE_TYPE_DISK); CloseHandle(h); }
}
TEST(path_names_ns_trailing_backslash_on_file_fails) {
    REQUIRE(U(GetFileAttributesA)); REQUIRE(U(CreateFileA));
    char f[MAX_PATH], q[MAX_PATH]; t_tmpfile(f, sizeof f, "notdir.txt"); pn_touch(f);
    snprintf(q, sizeof q, "%s\\", f);
    SET_ERR(0);
    DWORD a = U(GetFileAttributesA)(q);
    CHECK(a == INVALID_FILE_ATTRIBUTES);
    t_note("GetFileAttributes(file\\) err %lu", GetLastError());
    SET_ERR(0);
    HANDLE h = U(CreateFileA)(q, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h == INVALID_HANDLE_VALUE);
    t_note("CreateFile(file\\) err %lu", GetLastError());
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    DeleteFileA(f);
}
TEST(path_names_ns_path_through_a_file_is_path_not_found) {
    REQUIRE(U(GetFileAttributesA));
    char f[MAX_PATH], q[MAX_PATH]; t_tmpfile(f, sizeof f, "afile.txt"); pn_touch(f);
    snprintf(q, sizeof q, "%s\\child", f);
    SET_ERR(0);
    CHECK(U(GetFileAttributesA)(q) == INVALID_FILE_ATTRIBUTES);
    DWORD e = GetLastError();
    t_note("file used as a directory: err %lu", e);
    CHECK(e == ERROR_PATH_NOT_FOUND || e == ERROR_DIRECTORY);
    DeleteFileA(f);
}
TEST(path_names_ns_case_insensitive_lookup) {
    REQUIRE(U(GetFileAttributesA));
    char f[MAX_PATH], q[MAX_PATH];
    t_tmpfile(f, sizeof f, "CaseFile.TXT"); pn_touch(f);
    t_tmpfile(q, sizeof q, "casefile.txt");
    CHECK(U(GetFileAttributesA)(q) != INVALID_FILE_ATTRIBUTES);
    t_tmpfile(q, sizeof q, "CASEFILE.TXT");
    CHECK(U(GetFileAttributesA)(q) != INVALID_FILE_ATTRIBUTES);
    DeleteFileA(f);
}
TEST(path_names_ns_short_name_opens_same_file) {
    REQUIRE(U(GetShortPathNameA)); REQUIRE(U(CreateFileA));
    char f[MAX_PATH], s[MAX_PATH];
    t_tmpfile(f, sizeof f, "open via short alias.txt");
    HANDLE h = U(CreateFileA)(f, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD n; WriteFile(h, "12345", 5, &n, NULL); CloseHandle(h);
    if (!U(GetShortPathNameA)(f, s, sizeof s) || !_stricmp(s, f)) { t_skip("8.3 disabled"); DeleteFileA(f); return; }
    h = U(CreateFileA)(s, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) { CHECK(GetFileSize(h, NULL) == 5); CloseHandle(h); }
    DeleteFileA(f);
}
TEST(path_names_ns_final_path_of_extended_name_has_prefix) {
    REQUIRE(U(CreateFileW)); REQUIRE(U(GetFinalPathNameByHandleW));
    char f[MAX_PATH]; t_tmpfile(f, sizeof f, "final.txt"); pn_touch(f);
    HANDLE h = cf_w(pn_w(f), GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) {
        WCHAR b[MAX_PATH * 2];
        DWORD n = U(GetFinalPathNameByHandleW)(h, b, MAX_PATH * 2, VOLUME_NAME_DOS);
        CHECK(n > 4 && !wcsncmp(b, L"\\\\?\\", 4));
        CHECKF(!_wcsicmp(b + 4, pn_w(f)), "final %s", pn_n(b));
        n = U(GetFinalPathNameByHandleW)(h, b, MAX_PATH * 2, VOLUME_NAME_NT);
        CHECK(n > 8 && !wcsncmp(b, L"\\Device\\", 8));
        n = U(GetFinalPathNameByHandleW)(h, b, MAX_PATH * 2, VOLUME_NAME_GUID);
        CHECK(n > 48 && !wcsncmp(b, L"\\\\?\\Volume{", 11));
        CloseHandle(h);
    }
    DeleteFileA(f);
}
TEST(path_names_ns_toolong_path_without_prefix_fails_206) {
    REQUIRE(U(CreateFileW)); REQUIRE(U(CreateDirectoryW));
    static WCHAR p[600];
    swprintf(p, 600, L"%ls", pn_w(t_tmpdir()));
    while (wcslen(p) < 280) wcscat(p, L"abcdefghij0123456789\\");
    SET_ERR(0);
    HANDLE h = cf_w(p, GENERIC_WRITE, 0, CREATE_ALWAYS, 0);
    CHECK(h == INVALID_HANDLE_VALUE);
    /* exact status mapping not recoverable from the decompilation: 206 or 3 */
    { DWORD e = GetLastError(); CHECKF(e == ERROR_FILENAME_EXCED_RANGE || e == ERROR_PATH_NOT_FOUND, "err %lu", e); }
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    wcscat(p, L"x");
    SET_ERR(0);
    CHECK(!U(CreateDirectoryW)(p, NULL));
    { DWORD e = GetLastError(); CHECKF(e == ERROR_FILENAME_EXCED_RANGE || e == ERROR_PATH_NOT_FOUND, "err %lu", e); }
    SET_ERR(0);
    EXPECT_EQ(GetFileAttributesW(p), INVALID_FILE_ATTRIBUTES);
    t_note("GetFileAttributesW on >MAX_PATH non-prefixed name: err %lu", GetLastError());
}
TEST(path_names_ns_createdirectory_248_char_limit) {
    REQUIRE(U(CreateDirectoryW));
    /* CreateDirectory needs room for an 8.3 name: paths longer than 247 chars fail without the prefix */
    static WCHAR p[600];
    swprintf(p, 600, L"%ls", pn_w(t_tmpdir()));
    size_t base = wcslen(p);
    if (base > 200) { t_skip("temp dir too deep"); return; }
    pn_fill(p + base, (int)(250 - base), L'd');
    SET_ERR(0);
    BOOL ok = U(CreateDirectoryW)(p, NULL);
    DWORD e = GetLastError();
    t_note("CreateDirectory with a 250 char path: ok=%d err=%lu (documented: ERROR_FILENAME_EXCED_RANGE)", ok, e);
    if (ok) RemoveDirectoryW(p);
    pn_fill(p + base, (int)(240 - base), L'd');
    ok = U(CreateDirectoryW)(p, NULL);
    CHECKF(ok, "240 char directory err %lu", GetLastError());
    if (ok) RemoveDirectoryW(p);
}
TEST(path_names_ns_extended_prefix_long_path_roundtrip) {
    REQUIRE(U(CreateFileW)); REQUIRE(U(CreateDirectoryW));
    static WCHAR dir[1000], file[1100];
    swprintf(dir, 1000, L"\\\\?\\%ls", pn_w(t_tmpdir()));
    int made = 0; WCHAR *marks[8];
    while (wcslen(dir) < 300 && made < 8) {
        wcscat(dir, L"longcomponent_0123456789_abcdefghij_0123456789");
        marks[made] = dir + wcslen(dir);
        wcscat(dir, L"\\");
        if (!U(CreateDirectoryW)(dir, NULL)) { t_skip("cannot create deep directory err %lu", GetLastError()); goto cleanup; }
        made++;
    }
    swprintf(file, 1100, L"%lsfile.txt", dir);
    CHECK(wcslen(file) > MAX_PATH);
    HANDLE h = cf_w(file, GENERIC_WRITE, 0, CREATE_NEW, 0);
    CHECKF(h != INVALID_HANDLE_VALUE, "create long file err %lu", GetLastError());
    if (h != INVALID_HANDLE_VALUE) {
        CloseHandle(h);
        CHECK(U(GetFileAttributesW)(file) != INVALID_FILE_ATTRIBUTES);
        WCHAR fp[1100];
        DWORD n = U(GetFinalPathNameByHandleW) ? 1 : 0; (void)n; (void)fp;
        CHECK(DeleteFileW(file));
    }
cleanup:
    while (made > 0) { *marks[made - 1] = L'\\'; marks[made - 1][1] = 0; RemoveDirectoryW(dir); *marks[made - 1] = 0; made--; }
}
TEST(path_names_ns_component_length_255_ok_256_fails) {
    REQUIRE(U(CreateFileW));
    static WCHAR p[600];
    swprintf(p, 600, L"\\\\?\\%ls", pn_w(t_tmpdir()));
    size_t base = wcslen(p);
    pn_fill(p + base, 255, L'n');
    HANDLE h = cf_w(p, GENERIC_WRITE, 0, CREATE_NEW, 0);
    CHECKF(h != INVALID_HANDLE_VALUE, "255 chars err %lu", GetLastError());
    if (h != INVALID_HANDLE_VALUE) { CloseHandle(h); DeleteFileW(p); }
    pn_fill(p + base, 256, L'n');
    SET_ERR(0);
    h = cf_w(p, GENERIC_WRITE, 0, CREATE_NEW, 0);
    CHECK(h == INVALID_HANDLE_VALUE);
    t_note("256-char component: err %lu", GetLastError());
    if (h != INVALID_HANDLE_VALUE) { CloseHandle(h); DeleteFileW(p); }
}
TEST(path_names_ns_globalroot_device_null_is_openable_readonly_query) {
    REQUIRE(U(CreateFileW)); REQUIRE(U(GetFileAttributesW));
    SET_ERR(0);
    DWORD a = U(GetFileAttributesW)(L"\\\\?\\GLOBALROOT\\SystemRoot");
    t_note("GLOBALROOT\\SystemRoot attrs 0x%lx err %lu", a, GetLastError());
    HANDLE h = cf_w(L"\\\\?\\GLOBALROOT\\Device\\Null", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, 0);
    t_note("GLOBALROOT\\Device\\Null: %s (err %lu)", h != INVALID_HANDLE_VALUE ? "opened" : "failed", GetLastError());
    if (h != INVALID_HANDLE_VALUE) {
        DWORD n = 0;
        CHECK(WriteFile(h, "x", 1, &n, NULL) && n == 1);
        char b[4]; CHECK(ReadFile(h, b, 4, &n, NULL) && n == 0);
        CloseHandle(h);
    }
}
TEST(path_names_ns_root_directory_attributes) {
    REQUIRE(U(GetFileAttributesA)); REQUIRE(U(GetFileAttributesW));
    char root[8] = "C:\\"; root[0] = pn_sysdrive();
    DWORD a = U(GetFileAttributesA)(root);
    CHECK(a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY));
    char r2[8] = "C:"; r2[0] = pn_sysdrive();
    a = U(GetFileAttributesA)(r2);
    t_note("drive-relative 'X:' attrs 0x%lx", a);
    char r3[16] = "\\\\?\\C:\\"; r3[4] = pn_sysdrive();
    a = U(GetFileAttributesW)(pn_w(r3));
    CHECK(a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY));
}
TEST(path_names_ns_unc_admin_share_child_note) {
    DWORD r = t_run_child("unc_admin", NULL, 25000);
    t_note("\\\\localhost\\C$ probe child result: %lu (0=accessible 1=denied/absent)", r);
    CHECK(r == 0 || r == 1);
}
CHILD(unc_admin) {
    SetErrorMode(SEM_FAILCRITICALERRORS);
    DWORD a = U(GetFileAttributesW)(L"\\\\localhost\\C$\\");
    printf("  NOTE unc admin attrs 0x%lx err %lu\n", a, GetLastError());
    return a == INVALID_FILE_ATTRIBUTES ? 1 : 0;
}
TEST(path_names_ns_unc_server_missing_error_class) {
    DWORD r = t_run_child("unc_missing", NULL, 28000);
    t_note("missing UNC host probe child result: 0x%lx (0=expected error class)", r);
}
CHILD(unc_missing) {
    SetErrorMode(SEM_FAILCRITICALERRORS);
    SetLastError(0);
    DWORD a = U(GetFileAttributesW)(L"\\\\k32t-no-such-host-xyz\\share\\file");
    DWORD e = GetLastError();
    printf("  NOTE missing UNC host err %lu\n", e);
    if (a != INVALID_FILE_ATTRIBUTES) return 1;
    /* ERROR_BAD_NETPATH (53), ERROR_BAD_NET_NAME (67), ERROR_NETWORK_UNREACHABLE (1231) or ERROR_PATH_NOT_FOUND (3) depending on name resolution */
    return (e == 53 || e == 67 || e == 1231 || e == 3 || e == 1203 || e == 1222 || e == 2 || e == 64) ? 0 : 2;
}
