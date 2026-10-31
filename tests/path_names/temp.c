/* GetTempPathA/W, GetTempPath2W, GetTempFileNameA/W */
#include "pn.h"

static const char *leafof(const char *p) { const char *s = strrchr(p, '\\'); return s ? s + 1 : p; }

TEST(path_names_temp_path_ends_with_backslash) {
    REQUIRE(U(GetTempPathW)); REQUIRE(U(GetTempPathA));
    WCHAR w[MAX_PATH]; char a[MAX_PATH];
    DWORD n = U(GetTempPathW)(MAX_PATH, w);
    CHECK(n > 3 && n < MAX_PATH);
    CHECK(w[n - 1] == L'\\' && w[n] == 0);
    CHECK(wcslen(w) == n);
    n = U(GetTempPathA)(MAX_PATH, a);
    CHECK(n > 3 && a[n - 1] == '\\' && strlen(a) == n);
}
TEST(path_names_temp_path_a_w_parity) {
    REQUIRE(U(GetTempPathW)); REQUIRE(U(GetTempPathA));
    WCHAR w[MAX_PATH]; char a[MAX_PATH], w8[MAX_PATH * 2];
    DWORD nw = U(GetTempPathW)(MAX_PATH, w);
    DWORD na = U(GetTempPathA)(MAX_PATH, a);
    pn_a(w8, sizeof w8, w);
    CHECK(nw == na || strlen(w8) != nw);
    CHECKF(!strcmp(a, w8), "A=%s W=%s", a, w8);
}
TEST(path_names_temp_path_is_existing_directory) {
    REQUIRE(U(GetTempPathA));
    char a[MAX_PATH];
    U(GetTempPathA)(MAX_PATH, a);
    DWORD at = GetFileAttributesA(a);
    CHECK(at != INVALID_FILE_ATTRIBUTES && (at & FILE_ATTRIBUTE_DIRECTORY));
}
TEST(path_names_temp_path_buffer_semantics_w) {
    REQUIRE(U(GetTempPathW));
    WCHAR w[MAX_PATH], c[MAX_PATH];
    DWORD len = U(GetTempPathW)(MAX_PATH, w);
    EXPECT_EQ(U(GetTempPathW)(0, NULL), len + 1);
    EXPECT_EQ(U(GetTempPathW)(0, c), len + 1);
    for (DWORD sz = 1; sz <= len; sz++) {
        DWORD n = U(GetTempPathW)(sz, c);
        CHECKF(n == len + 1, "size %lu returned %lu want %lu", sz, n, len + 1);
    }
    pn_fill(c, 10, L'Z');
    EXPECT_EQ(U(GetTempPathW)(len + 1, c), len);
    CHECK(!wcscmp(c, w));
}
TEST(path_names_temp_path_buffer_semantics_a) {
    REQUIRE(U(GetTempPathA));
    char a[MAX_PATH], c[MAX_PATH];
    DWORD len = U(GetTempPathA)(MAX_PATH, a);
    EXPECT_EQ(U(GetTempPathA)(0, NULL), len + 1);
    EXPECT_EQ(U(GetTempPathA)(0, c), len + 1);
    EXPECT_EQ(U(GetTempPathA)(len, c), len + 1);
    EXPECT_EQ(U(GetTempPathA)(len + 1, c), len);
    EXPECT_STR(c, a);
}
TEST(path_names_temp_path_repeated_stable) {
    REQUIRE(U(GetTempPathW));
    WCHAR a[MAX_PATH], b[MAX_PATH];
    U(GetTempPathW)(MAX_PATH, a);
    for (int i = 0; i < 200; i++) { U(GetTempPathW)(MAX_PATH, b); if (wcscmp(a, b)) { CHECKF(0, "iteration %d differs", i); return; } }
    CHECK(1);
}
TEST(path_names_temp_path_matches_tmp_env_when_set) {
    REQUIRE(U(GetTempPathW));
    WCHAR env[MAX_PATH], full[MAX_PATH], w[MAX_PATH];
    DWORD e = GetEnvironmentVariableW(L"TMP", env, MAX_PATH);
    if (!e) { t_skip("TMP not set in this process"); return; }
    GetFullPathNameW(env, MAX_PATH, full, NULL);
    size_t l = wcslen(full);
    if (full[l - 1] != L'\\') { full[l++] = L'\\'; full[l] = 0; }
    U(GetTempPathW)(MAX_PATH, w);
    CHECKF(!wcscmp(w, full), "temp=%s tmp=%s", pn_n(w), pn_n(full));
}
/* Child: run with a custom environment and verify GetTempPath against the expectation passed in argv[1]. */
CHILD(temppath_expect) {
    if (argc < 2) return 20;
    WCHAR w[MAX_PATH * 2]; char a[MAX_PATH * 2];
    char want[MAX_PATH * 2];
    snprintf(want, sizeof want, "%s", argv[1]);
    for (char *c = want; *c; c++) if (*c == '|') *c = '\\';   /* '|' stands for a backslash on the command line */
    argv[1] = want;
    DWORD n = U(GetTempPathW)(MAX_PATH * 2, w);
    pn_a(a, sizeof a, w);
    if (n != strlen(argv[1])) { printf("CHILDFAIL len %lu got %s want %s\n", n, a, argv[1]); return 21; }
    if (_stricmp(a, argv[1])) { printf("CHILDFAIL got %s want %s\n", a, argv[1]); return 22; }
    char b[MAX_PATH * 2];
    n = U(GetTempPathA)(MAX_PATH * 2, b);
    if (_stricmp(b, argv[1])) { printf("CHILDFAIL A got %s want %s\n", b, argv[1]); return 23; }
    return 0;
}
static DWORD run_temp_child(const char *env_extra, const char *expect) {
    char args[1024], envblk[4096]; size_t o = 0;
    snprintf(args, sizeof args, "\"%s\"", expect);
    for (char *c = args; *c; c++) if (*c == '\\') *c = '|';
    char root[MAX_PATH]; GetEnvironmentVariableA("SystemRoot", root, sizeof root);
    o += snprintf(envblk + o, sizeof envblk - o, "SystemRoot=%s", root) + 1;
    o += snprintf(envblk + o, sizeof envblk - o, "windir=%s", root) + 1;
    while (env_extra && *env_extra) { o += snprintf(envblk + o, sizeof envblk - o, "%s", env_extra) + 1; env_extra += strlen(env_extra) + 1; }
    envblk[o] = 0;
    PROCESS_INFORMATION pi;
    HANDLE h = t_spawn_child("temppath_expect", args, 0, NULL, NULL, NULL, FALSE, envblk, &pi);
    if (!h) return 0xDEAD0000;
    DWORD code = 0xDEAD0001;
    if (WaitForSingleObject(h, 20000) != WAIT_OBJECT_0) TerminateProcess(h, 1); else GetExitCodeProcess(h, &code);
    CloseHandle(pi.hThread); CloseHandle(h);
    return code;
}
TEST(path_names_temp_path_env_tmp_wins) {
    REQUIRE(U(GetTempPathW));
    char e[1024];
    snprintf(e, sizeof e, "TMP=C:\\k32t_tmp_dir%cTEMP=C:\\k32t_temp_dir%cUSERPROFILE=C:\\k32t_prof%c", 0, 0, 0);
    EXPECT_EQ(run_temp_child(e, "C:\\k32t_tmp_dir\\"), 0);
}
TEST(path_names_temp_path_env_temp_when_no_tmp) {
    REQUIRE(U(GetTempPathW));
    char e[1024];
    snprintf(e, sizeof e, "TEMP=C:\\k32t_temp_dir%cUSERPROFILE=C:\\k32t_prof%c", 0, 0);
    EXPECT_EQ(run_temp_child(e, "C:\\k32t_temp_dir\\"), 0);
}
TEST(path_names_temp_path_env_userprofile_fallback) {
    REQUIRE(U(GetTempPathW));
    char e[1024];
    snprintf(e, sizeof e, "USERPROFILE=C:\\k32t_prof%c", 0);
    EXPECT_EQ(run_temp_child(e, "C:\\k32t_prof\\"), 0);
}
TEST(path_names_temp_path_env_none_gives_windows_dir) {
    REQUIRE(U(GetTempPathW));
    char root[MAX_PATH], want[MAX_PATH];
    GetWindowsDirectoryA(root, sizeof root);
    snprintf(want, sizeof want, "%s\\", root);
    EXPECT_EQ(run_temp_child("", want), 0);
}
TEST(path_names_temp_path_env_trailing_backslash_not_doubled) {
    REQUIRE(U(GetTempPathW));
    char e[256];
    snprintf(e, sizeof e, "TMP=C:\\k32t_a\\b\\%c", 0);
    EXPECT_EQ(run_temp_child(e, "C:\\k32t_a\\b\\"), 0);
}
TEST(path_names_temp_path_env_is_canonicalised) {
    REQUIRE(U(GetTempPathW));
    char e[256];
    snprintf(e, sizeof e, "TMP=C:/k32t_a/x/../b%c", 0);
    EXPECT_EQ(run_temp_child(e, "C:\\k32t_a\\b\\"), 0);
}
TEST(path_names_temp_path_env_nonexistent_dir_not_validated) {
    REQUIRE(U(GetTempPathW));
    char e[256];
    snprintf(e, sizeof e, "TMP=C:\\k32t_surely_missing_dir_12345\\sub%c", 0);
    EXPECT_EQ(run_temp_child(e, "C:\\k32t_surely_missing_dir_12345\\sub\\"), 0);
}
TEST(path_names_temp_path_env_relative_resolved_against_cwd) {
    REQUIRE(U(GetTempPathW));
    /* a relative TMP is made absolute (via the child's cwd = this exe's cwd) */
    char cwd[MAX_PATH], want[MAX_PATH * 2], e[256];
    GetCurrentDirectoryA(sizeof cwd, cwd);
    size_t l = strlen(cwd);
    snprintf(want, sizeof want, "%s%srel_tmp_k32t\\", cwd, cwd[l - 1] == '\\' ? "" : "\\");
    snprintf(e, sizeof e, "TMP=rel_tmp_k32t%c", 0);
    EXPECT_EQ(run_temp_child(e, want), 0);
}
TEST(path_names_temp_path_env_unc_style) {
    REQUIRE(U(GetTempPathW));
    char e[256];
    snprintf(e, sizeof e, "TMP=\\\\server\\share\\t%c", 0);
    EXPECT_EQ(run_temp_child(e, "\\\\server\\share\\t\\"), 0);
}
TEST(path_names_temp_path_env_drive_root) {
    REQUIRE(U(GetTempPathW));
    char e[256];
    snprintf(e, sizeof e, "TMP=C:\\%c", 0);
    EXPECT_EQ(run_temp_child(e, "C:\\"), 0);
    snprintf(e, sizeof e, "TMP=C:%c", 0);
    t_note("TMP=C: (drive-relative) child result %lu", run_temp_child(e, "C:\\"));
}
TEST(path_names_temp_path_empty_tmp_var) {
    REQUIRE(U(GetTempPathW));
    char e[256];
    /* TMP defined but empty is treated as defined with zero length; result is note-only */
    snprintf(e, sizeof e, "TMP=%cTEMP=C:\\k32t_temp_dir%c", 0, 0);
    t_note("TMP empty, TEMP set -> child result %lu (0 means TEMP used)", run_temp_child(e, "C:\\k32t_temp_dir\\"));
}
TEST(path_names_temp_path2_exists_note) {
    FARPROC p = t_proc("GetTempPath2W");
    t_note("GetTempPath2W exported: %s", p ? "yes" : "no");
    if (!p) return;
    typedef DWORD (WINAPI *pfn)(DWORD, LPWSTR);
    pfn f = (pfn)p;
    WCHAR a[MAX_PATH], b[MAX_PATH];
    DWORD n2 = f(MAX_PATH, a);
    DWORD n1 = U(GetTempPathW)(MAX_PATH, b);
    t_note("GetTempPath2W=%s", pn_n(a));
    CHECK(n2 != 0);
    CHECK(f(0, NULL) == n2 + 1);
    /* for non-SYSTEM callers it equals GetTempPathW */
    if (n1 == n2) CHECK(!wcscmp(a, b));
}

/* ------------------------------ GetTempFileName ------------------------------ */
static int hexval(const char *s, unsigned *v) { char *e; *v = (unsigned)strtoul(s, &e, 16); return e != s && !strcmp(e, ".tmp"); }

TEST(path_names_tempfile_unique_nonzero_no_file_created_w) {
    REQUIRE(U(GetTempFileNameW));
    WCHAR out[MAX_PATH]; char a[MAX_PATH];
    UINT r = U(GetTempFileNameW)(pn_w(t_tmpdir()), L"abc", 0x1A2B, out);
    EXPECT_EQ(r, 0x1A2B);
    pn_a(a, sizeof a, out);
    char want[MAX_PATH]; snprintf(want, sizeof want, "%sabc1A2B.tmp", t_tmpdir());
    CHECKF(!_stricmp(a, want), "got %s want %s", a, want);
    CHECK(!pn_fileexists(a));
}
TEST(path_names_tempfile_unique_nonzero_no_file_created_a) {
    REQUIRE(U(GetTempFileNameA));
    char out[MAX_PATH], want[MAX_PATH];
    UINT r = U(GetTempFileNameA)(t_tmpdir(), "xyz", 0x00FF, out);
    EXPECT_EQ(r, 0xFF);
    snprintf(want, sizeof want, "%sxyzFF.tmp", t_tmpdir());
    CHECKF(!_stricmp(out, want), "got %s want %s", out, want);
    CHECK(!pn_fileexists(out));
}
TEST(path_names_tempfile_unique_hex_formatting) {
    REQUIRE(U(GetTempFileNameA));
    static const struct { UINT u; const char *hex; } t[] = { { 1, "1" }, { 0xF, "F" }, { 0x10, "10" }, { 0x123, "123" }, { 0xABCD, "ABCD" }, { 0xFFFF, "FFFF" } };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        char out[MAX_PATH], want[MAX_PATH];
        UINT r = U(GetTempFileNameA)(t_tmpdir(), "p", t[i].u, out);
        snprintf(want, sizeof want, "%sp%s.tmp", t_tmpdir(), t[i].hex);
        CHECKF(r == t[i].u && !_stricmp(out, want), "u=%x ret=%x out=%s want=%s", t[i].u, r, out, want);
    }
}
TEST(path_names_tempfile_unique_upper_bits_masked) {
    REQUIRE(U(GetTempFileNameA));
    char out[MAX_PATH], want[MAX_PATH];
    UINT r = U(GetTempFileNameA)(t_tmpdir(), "m", 0x12345, out);
    EXPECT_EQ(r, 0x2345);
    snprintf(want, sizeof want, "%sm2345.tmp", t_tmpdir());
    CHECKF(!_stricmp(out, want), "got %s", out);
    CHECK(!pn_fileexists(out));
}
TEST(path_names_tempfile_prefix_truncated_to_three) {
    REQUIRE(U(GetTempFileNameA)); REQUIRE(U(GetTempFileNameW));
    char out[MAX_PATH], want[MAX_PATH]; WCHAR ow[MAX_PATH];
    U(GetTempFileNameA)(t_tmpdir(), "abcdefgh", 0x42, out);
    snprintf(want, sizeof want, "%sabc42.tmp", t_tmpdir());
    CHECKF(!_stricmp(out, want), "A got %s", out);
    U(GetTempFileNameW)(pn_w(t_tmpdir()), L"abcdefgh", 0x42, ow);
    pn_a(out, sizeof out, ow);
    CHECKF(!_stricmp(out, want), "W got %s", out);
    U(GetTempFileNameA)(t_tmpdir(), "abc", 0x42, out);
    CHECKF(!_stricmp(out, want), "exact 3: %s", out);
    snprintf(want, sizeof want, "%sab42.tmp", t_tmpdir());
    U(GetTempFileNameA)(t_tmpdir(), "ab", 0x42, out);
    CHECKF(!_stricmp(out, want), "2 chars: %s", out);
}
TEST(path_names_tempfile_prefix_case_preserved) {
    REQUIRE(U(GetTempFileNameA));
    char out[MAX_PATH];
    U(GetTempFileNameA)(t_tmpdir(), "AbC", 0x7, out);
    CHECK(!strncmp(leafof(out), "AbC7", 4));
}
TEST(path_names_tempfile_empty_prefix) {
    REQUIRE(U(GetTempFileNameA));
    char out[MAX_PATH], want[MAX_PATH];
    UINT r = U(GetTempFileNameA)(t_tmpdir(), "", 0x9, out);
    EXPECT_EQ(r, 9);
    snprintf(want, sizeof want, "%s9.tmp", t_tmpdir());
    CHECKF(!_stricmp(out, want), "got %s", out);
}
TEST(path_names_tempfile_dir_with_and_without_trailing_backslash) {
    REQUIRE(U(GetTempFileNameA));
    char d[MAX_PATH], o1[MAX_PATH], o2[MAX_PATH];
    snprintf(d, sizeof d, "%s", t_tmpdir());
    U(GetTempFileNameA)(d, "k32", 0x33, o1);
    d[strlen(d) - 1] = 0;
    U(GetTempFileNameA)(d, "k32", 0x33, o2);
    CHECKF(!strcmp(o1, o2), "%s vs %s", o1, o2);
}
TEST(path_names_tempfile_relative_dot_dir) {
    REQUIRE(U(GetTempFileNameA));
    char out[MAX_PATH];
    UINT r = U(GetTempFileNameA)(".", "rel", 0x5, out);
    EXPECT_EQ(r, 5);
    CHECKF(!strcmp(out, ".\\rel5.tmp"), "got %s", out);
    CHECK(!pn_fileexists(out));
}
TEST(path_names_tempfile_zero_creates_file_w) {
    REQUIRE(U(GetTempFileNameW));
    WCHAR ow[MAX_PATH]; char o[MAX_PATH];
    UINT r = U(GetTempFileNameW)(pn_w(t_tmpdir()), L"k3z", 0, ow);
    CHECK(r != 0 && r <= 0xFFFF);
    pn_a(o, sizeof o, ow);
    CHECKF(pn_fileexists(o), "file %s must exist", o);
    HANDLE h = CreateFileA(o, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) { CHECK(GetFileSize(h, NULL) == 0); CloseHandle(h); }
    /* name = dir + 3-char prefix + hex(ret) + .tmp */
    const char *l = leafof(o);
    CHECK(!strncmp(l, "k3z", 3));
    unsigned v = 0;
    CHECK(hexval(l + 3, &v));
    CHECKF(v == r, "hex in name %x != return %x", v, r);
    DeleteFileA(o);
}
TEST(path_names_tempfile_zero_creates_file_a) {
    REQUIRE(U(GetTempFileNameA));
    char o[MAX_PATH];
    UINT r = U(GetTempFileNameA)(t_tmpdir(), "k3a", 0, o);
    CHECK(r != 0);
    CHECK(pn_fileexists(o));
    CHECK(!strncmp(o, t_tmpdir(), strlen(t_tmpdir())));
    CHECK(strlen(o) <= strlen(t_tmpdir()) + 3 + 4 + 4);
    DeleteFileA(o);
}
TEST(path_names_tempfile_zero_names_are_unique) {
    REQUIRE(U(GetTempFileNameA));
    enum { N = 60 };
    static char names[N][MAX_PATH]; static UINT rets[N];
    int made = 0;
    for (int i = 0; i < N; i++) {
        rets[i] = U(GetTempFileNameA)(t_tmpdir(), "uni", 0, names[i]);
        if (!rets[i]) break;
        made++;
    }
    EXPECT_EQ(made, N);
    int dup = 0;
    for (int i = 0; i < made; i++) for (int j = i + 1; j < made; j++) if (!_stricmp(names[i], names[j])) dup++;
    EXPECT_EQ(dup, 0);
    for (int i = 0; i < made; i++) { CHECK(pn_fileexists(names[i])); DeleteFileA(names[i]); }
}
TEST(path_names_tempfile_zero_skips_existing_name) {
    REQUIRE(U(GetTempFileNameA));
    /* every produced file already exists afterwards, so a second call can never return a name that is taken */
    char a[MAX_PATH], b[MAX_PATH];
    U(GetTempFileNameA)(t_tmpdir(), "skp", 0, a);
    U(GetTempFileNameA)(t_tmpdir(), "skp", 0, b);
    CHECK(_stricmp(a, b) != 0);
    DeleteFileA(a); DeleteFileA(b);
}
TEST(path_names_tempfile_unique_0x10000_acts_as_zero) {
    REQUIRE(U(GetTempFileNameA));
    char o[MAX_PATH];
    UINT r = U(GetTempFileNameA)(t_tmpdir(), "u10", 0x10000, o);
    CHECK(r != 0 && r <= 0xFFFF);
    CHECK(pn_fileexists(o));
    DeleteFileA(o);
}
TEST(path_names_tempfile_zero_in_readonly_attribute_dir_irrelevant) {
    REQUIRE(U(GetTempFileNameA));
    char o[MAX_PATH];
    UINT r = U(GetTempFileNameA)(t_tmpdir(), "att", 0, o);
    DWORD at = GetFileAttributesA(o);
    CHECK(r != 0 && at != INVALID_FILE_ATTRIBUTES);
    CHECK((at & FILE_ATTRIBUTE_DIRECTORY) == 0);
    CHECK(at & FILE_ATTRIBUTE_ARCHIVE);
    DeleteFileA(o);
}
TEST(path_names_tempfile_missing_dir_error_267) {
    REQUIRE(U(GetTempFileNameA)); REQUIRE(U(GetTempFileNameW));
    char d[MAX_PATH], o[MAX_PATH]; WCHAR ow[MAX_PATH];
    snprintf(d, sizeof d, "%sno_such_dir_here", t_tmpdir());
    SET_ERR(0); EXPECT_EQ(U(GetTempFileNameA)(d, "abc", 0, o), 0); EXPECT_ERR(ERROR_DIRECTORY);
    SET_ERR(0); EXPECT_EQ(U(GetTempFileNameA)(d, "abc", 5, o), 0); EXPECT_ERR(ERROR_DIRECTORY);
    SET_ERR(0); EXPECT_EQ(U(GetTempFileNameW)(pn_w(d), L"abc", 0, ow), 0); EXPECT_ERR(ERROR_DIRECTORY);
}
TEST(path_names_tempfile_path_is_a_file_error_267) {
    REQUIRE(U(GetTempFileNameA));
    char f[MAX_PATH], o[MAX_PATH];
    t_tmpfile(f, sizeof f, "notadir.txt"); pn_touch(f);
    SET_ERR(0); EXPECT_EQ(U(GetTempFileNameA)(f, "abc", 3, o), 0); EXPECT_ERR(ERROR_DIRECTORY);
    DeleteFileA(f);
}
TEST(path_names_tempfile_wildcard_dir_error_267) {
    REQUIRE(U(GetTempFileNameA));
    char d[MAX_PATH], o[MAX_PATH];
    snprintf(d, sizeof d, "%s*", t_tmpdir());
    SET_ERR(0); EXPECT_EQ(U(GetTempFileNameA)(d, "abc", 3, o), 0);
    t_note("wildcard dir err %lu", GetLastError());
}
TEST(path_names_tempfile_long_dir_buffer_overflow_111) {
    REQUIRE(U(GetTempFileNameW));
    static WCHAR d[400], o[600];
    /* no trailing backslash: lengths up to 247 pass the size check, 248+ yield ERROR_BUFFER_OVERFLOW */
    wcscpy(d, L"C:\\"); pn_fill(d + 3, 244, L'a');   /* 247 chars */
    SET_ERR(0); EXPECT_EQ(U(GetTempFileNameW)(d, L"abc", 1, o), 0); EXPECT_ERR(ERROR_DIRECTORY);
    pn_fill(d + 3, 245, L'a');                       /* 248 chars */
    SET_ERR(0); EXPECT_EQ(U(GetTempFileNameW)(d, L"abc", 1, o), 0); EXPECT_ERR(ERROR_BUFFER_OVERFLOW);
    pn_fill(d + 3, 300, L'a');
    SET_ERR(0); EXPECT_EQ(U(GetTempFileNameW)(d, L"abc", 1, o), 0); EXPECT_ERR(ERROR_BUFFER_OVERFLOW);
    /* with a trailing backslash 248 chars still pass, 249 do not */
    wcscpy(d, L"C:\\"); pn_fill(d + 3, 244, L'a'); d[247] = L'\\'; d[248] = 0;
    SET_ERR(0); EXPECT_EQ(U(GetTempFileNameW)(d, L"abc", 1, o), 0); EXPECT_ERR(ERROR_DIRECTORY);
    pn_fill(d + 3, 245, L'a'); d[248] = L'\\'; d[249] = 0;
    SET_ERR(0); EXPECT_EQ(U(GetTempFileNameW)(d, L"abc", 1, o), 0); EXPECT_ERR(ERROR_BUFFER_OVERFLOW);
}
TEST(path_names_tempfile_in_place_output_buffer) {
    REQUIRE(U(GetTempFileNameA));
    char b[MAX_PATH];
    snprintf(b, sizeof b, "%s", t_tmpdir());
    UINT r = U(GetTempFileNameA)(b, "ali", 0x21, b);
    EXPECT_EQ(r, 0x21);
    char want[MAX_PATH]; snprintf(want, sizeof want, "%sali21.tmp", t_tmpdir());
    CHECKF(!_stricmp(b, want), "got %s", b);
}
TEST(path_names_tempfile_null_prefix_note) {
    REQUIRE(U(GetTempFileNameW));
    WCHAR ow[MAX_PATH];
    SET_ERR(0);
    UINT r = U(GetTempFileNameW)(pn_w(t_tmpdir()), NULL, 0x44, ow);
    t_note("NULL prefix: ret %u err %lu out %s", r, GetLastError(), r ? pn_n(ow) : "-");
}
TEST(path_names_tempfile_empty_dir_is_current_drive_root_note) {
    REQUIRE(U(GetTempFileNameW));
    WCHAR ow[MAX_PATH];
    SET_ERR(0);
    UINT r = U(GetTempFileNameW)(L"", L"abc", 0x44, ow);
    t_note("empty dir: ret %u err %lu out %s", r, GetLastError(), r ? pn_n(ow) : "-");
}
TEST(path_names_tempfile_a_w_parity) {
    REQUIRE(U(GetTempFileNameA)); REQUIRE(U(GetTempFileNameW));
    char a[MAX_PATH], w8[MAX_PATH]; WCHAR w[MAX_PATH];
    UINT ra = U(GetTempFileNameA)(t_tmpdir(), "par", 0xBEE, a);
    UINT rw = U(GetTempFileNameW)(pn_w(t_tmpdir()), L"par", 0xBEE, w);
    pn_a(w8, sizeof w8, w);
    CHECK(ra == rw && !strcmp(a, w8));
}
TEST(path_names_tempfile_default_temp_dir_roundtrip) {
    REQUIRE(U(GetTempFileNameW)); REQUIRE(U(GetTempPathW));
    WCHAR d[MAX_PATH], o[MAX_PATH];
    U(GetTempPathW)(MAX_PATH, d);
    UINT r = U(GetTempFileNameW)(d, L"k32", 0, o);
    CHECK(r != 0);
    if (r) {
        CHECK(!wcsncmp(o, d, wcslen(d)));
        CHECK(GetFileAttributesW(o) != INVALID_FILE_ATTRIBUTES);
        DeleteFileW(o);
    }
}
TEST(path_names_tempfile_unc_style_dir_missing) {
    REQUIRE(U(GetTempFileNameA));
    char o[MAX_PATH];
    SET_ERR(0);
    UINT r = U(GetTempFileNameA)("\\\\k32t-no-such-host\\share", "abc", 1, o);
    CHECK(r == 0);
    t_note("unc missing err %lu", GetLastError());
}
