/* GetFullPathNameA/W: canonicalisation tables, device names, file part, buffer semantics. */
#include "pn.h"

typedef struct { const char *in; const char *out; int hard; } fp_case;

static const fp_case fp_table[] = {
    { "C:\\", "C:\\", 1 },
    { "C:\\a", "C:\\a", 1 },
    { "C:\\a\\b", "C:\\a\\b", 1 },
    { "C:\\a\\b\\", "C:\\a\\b\\", 1 },
    { "C:/a/b", "C:\\a\\b", 1 },
    { "C:/", "C:\\", 1 },
    { "C:\\a\\\\b", "C:\\a\\b", 1 },
    { "C:\\a\\\\\\\\b", "C:\\a\\b", 1 },
    { "C:\\a\\//b", "C:\\a\\b", 1 },
    { "C:\\a\\.\\b", "C:\\a\\b", 1 },
    { "C:\\.\\a", "C:\\a", 1 },
    { "C:\\a\\.\\.\\.\\b", "C:\\a\\b", 1 },
    { "C:\\a\\..\\b", "C:\\b", 1 },
    { "C:\\a\\b\\..\\c", "C:\\a\\c", 1 },
    { "C:\\a\\b\\..\\..\\c", "C:\\c", 1 },
    { "C:\\a\\b\\..\\..", "C:\\", 1 },
    { "C:\\a\\b\\..\\..\\..", "C:\\", 1 },
    { "C:\\..", "C:\\", 1 },
    { "C:\\..\\..\\a", "C:\\a", 1 },
    { "C:\\a\\b\\.", "C:\\a\\b", 1 },
    { "C:\\a\\b\\..", "C:\\a", 1 },
    { "C:\\a\\..", "C:\\", 1 },
    { "C:\\a\\b\\..\\c\\.\\d\\", "C:\\a\\c\\d\\", 1 },
    { "C:\\a\\b.", "C:\\a\\b", 1 },
    { "C:\\a\\b..", "C:\\a\\b", 1 },
    { "C:\\a\\b  ", "C:\\a\\b", 1 },
    { "C:\\a\\b. ", "C:\\a\\b", 1 },
    { "C:\\a\\b.txt.", "C:\\a\\b.txt", 1 },
    { "C:\\a\\.b", "C:\\a\\.b", 1 },
    { "C:\\a\\b.c.d", "C:\\a\\b.c.d", 1 },
    { "C:\\a b\\c d", "C:\\a b\\c d", 1 },
    { "C:\\a*?\\b", "C:\\a*?\\b", 1 },
    { "C:\\a\\*.txt", "C:\\a\\*.txt", 1 },
    { "C:\\A\\b\\C", "C:\\A\\b\\C", 1 },
    { "C:\\a\\b\\..\\B", "C:\\a\\B", 1 },
    { "C:\\a\\b\\", "C:\\a\\b\\", 1 },
    { "C:\\a\\b\\\\", "C:\\a\\b\\", 1 },
    { "C:\\a\\.", "C:\\a", 1 },
    { "C:\\.", "C:\\", 1 },
    { "C:\\a\\...\\b", "C:\\a\\b", 0 },
    { "C:\\a\\..\\.\\..\\b", "C:\\b", 1 },
    { "C:\\a\\b\\c\\d\\..\\..\\e", "C:\\a\\b\\e", 1 },
    /* UNC */
    { "\\\\server\\share", "\\\\server\\share", 1 },
    { "\\\\server\\share\\", "\\\\server\\share\\", 1 },
    { "\\\\server\\share\\a", "\\\\server\\share\\a", 1 },
    { "\\\\server\\share\\a\\..\\b", "\\\\server\\share\\b", 1 },
    { "\\\\server\\share\\..\\..\\a", "\\\\server\\share\\a", 1 },
    { "\\\\server\\share\\a\\.", "\\\\server\\share\\a", 1 },
    { "\\\\server\\share\\a\\\\b", "\\\\server\\share\\a\\b", 1 },
    { "//server/share/a", "\\\\server\\share\\a", 1 },
    { "\\\\server\\share\\a.", "\\\\server\\share\\a", 1 },
    { "\\\\server\\share\\a ", "\\\\server\\share\\a", 1 },
    { "\\\\127.0.0.1\\c$\\x", "\\\\127.0.0.1\\c$\\x", 1 },
    { "\\\\localhost\\c$\\x\\..\\y", "\\\\localhost\\c$\\y", 1 },
    /* \\?\ paths are still normalised by GetFullPathName (the file APIs skip it, this function does not) */
    { "\\\\?\\C:\\a\\..\\b", "\\\\?\\C:\\b", 1 },
    { "\\\\?\\C:\\a/b", "\\\\?\\C:\\a\\b", 1 },
    { "\\\\?\\C:\\a\\.\\b", "\\\\?\\C:\\a\\b", 1 },
    { "\\\\?\\C:\\a\\\\b", "\\\\?\\C:\\a\\b", 1 },
    { "\\\\?\\C:\\a.", "\\\\?\\C:\\a", 1 },
    { "\\\\?\\C:\\a ", "\\\\?\\C:\\a", 1 },
    { "\\\\?\\C:\\", "\\\\?\\C:\\", 1 },
    { "\\\\?\\C:\\a\\..", "\\\\?\\C:\\", 0 },
    { "\\\\?\\C:\\..\\..\\a", "\\\\?\\C:\\a", 0 },
    { "\\\\?\\UNC\\s\\sh\\a\\.\\b", "\\\\?\\UNC\\s\\sh\\a\\b", 1 },
    { "\\\\?\\Volume{01234567-89ab-cdef-0123-456789abcdef}\\a\\..\\b", "\\\\?\\Volume{01234567-89ab-cdef-0123-456789abcdef}\\b", 1 },
    { "\\\\?\\GLOBALROOT\\Device\\HarddiskVolume1\\x\\..\\y", "\\\\?\\GLOBALROOT\\Device\\HarddiskVolume1\\y", 1 },
    /* \\.\ device paths: normalised like normal paths */
    { "\\\\.\\C:\\a\\..\\b", "\\\\.\\C:\\b", 1 },
    { "\\\\.\\C:\\a\\.\\b", "\\\\.\\C:\\a\\b", 1 },
    { "\\\\.\\C:", "\\\\.\\C:", 1 },
    { "\\\\.\\PhysicalDrive0", "\\\\.\\PhysicalDrive0", 1 },
    { "\\\\.\\pipe\\foo", "\\\\.\\pipe\\foo", 1 },
    { "\\\\.\\pipe\\foo\\..\\bar", "\\\\.\\pipe\\bar", 1 },
    { "//./C:/a", "\\\\.\\C:\\a", 1 },
    { "\\\\.\\COM1", "\\\\.\\COM1", 1 },
    { "\\\\.\\NUL", "\\\\.\\NUL", 1 },
    { "\\\\.\\mailslot\\x", "\\\\.\\mailslot\\x", 1 },
};

static void check_fp_row(const fp_case *c, int idx) {
    WCHAR bw[2048]; char ba[2048], oa[2048]; char *fa = NULL; WCHAR *fw = NULL;
    DWORD n = U(GetFullPathNameW)(pn_w(c->in), 2048, bw, &fw);
    pn_a(oa, sizeof oa, bw);
    if (c->hard) {
        CHECKF(n == strlen(c->out) && !strcmp(oa, c->out), "W row %d in=%s got(%lu)=%s want=%s", idx, c->in, n, oa, c->out);
    } else {
        if (!(n == strlen(c->out) && !strcmp(oa, c->out))) t_note("W row %d in=%s got=%s want=%s", idx, c->in, oa, c->out);
    }
    if (n && n < 2048) {
        CHECKF(pn_wlen(bw) == n, "W row %d return value must equal output length", idx);
    }
    n = U(GetFullPathNameA)(c->in, sizeof ba, ba, &fa);
    if (c->hard) {
        CHECKF(n == strlen(c->out) && !strcmp(ba, c->out), "A row %d in=%s got(%lu)=%s want=%s", idx, c->in, n, ba, c->out);
    } else if (!(n == strlen(c->out) && !strcmp(ba, c->out))) t_note("A row %d in=%s got=%s want=%s", idx, c->in, ba, c->out);
}

TEST(path_names_fullpath_table_all) {
    REQUIRE(U(GetFullPathNameW)); REQUIRE(U(GetFullPathNameA));
    for (unsigned i = 0; i < sizeof fp_table / sizeof *fp_table; i++) check_fp_row(&fp_table[i], i);
}
TEST(path_names_fullpath_table_a_matches_w) {
    REQUIRE(U(GetFullPathNameW)); REQUIRE(U(GetFullPathNameA));
    for (unsigned i = 0; i < sizeof fp_table / sizeof *fp_table; i++) {
        char ba[1024], bw8[1024]; WCHAR bw[1024];
        DWORD na = U(GetFullPathNameA)(fp_table[i].in, sizeof ba, ba, NULL);
        DWORD nw = U(GetFullPathNameW)(pn_w(fp_table[i].in), 1024, bw, NULL);
        pn_a(bw8, sizeof bw8, bw);
        CHECKF(na == nw && !strcmp(ba, bw8), "row %u %s: A=%s W=%s", i, fp_table[i].in, ba, bw8);
    }
}
TEST(path_names_fullpath_idempotent) {
    REQUIRE(U(GetFullPathNameW));
    for (unsigned i = 0; i < sizeof fp_table / sizeof *fp_table; i++) {
        if (!fp_table[i].hard) continue;
        WCHAR a[1024], b[1024];
        DWORD n1 = U(GetFullPathNameW)(pn_w(fp_table[i].in), 1024, a, NULL);
        DWORD n2 = U(GetFullPathNameW)(a, 1024, b, NULL);
        CHECKF(n1 == n2 && !wcscmp(a, b), "row %u %s not idempotent: %s -> %s", i, fp_table[i].in, pn_n(a), pn_n(b));
    }
}
TEST(path_names_fullpath_empty_and_blank_invalid_name) {
    REQUIRE(U(GetFullPathNameW)); REQUIRE(U(GetFullPathNameA));
    WCHAR bw[64]; char ba[64];
    SET_ERR(0); EXPECT_EQ(U(GetFullPathNameW)(L"", 64, bw, NULL), 0); EXPECT_ERR(ERROR_INVALID_NAME);
    SET_ERR(0); EXPECT_EQ(U(GetFullPathNameA)("", 64, ba, NULL), 0); EXPECT_ERR(ERROR_INVALID_NAME);
    SET_ERR(0); EXPECT_EQ(U(GetFullPathNameW)(L" ", 64, bw, NULL), 0); EXPECT_ERR(ERROR_INVALID_NAME);
    SET_ERR(0); EXPECT_EQ(U(GetFullPathNameW)(L"      ", 64, bw, NULL), 0); EXPECT_ERR(ERROR_INVALID_NAME);
    SET_ERR(0); EXPECT_EQ(U(GetFullPathNameA)("   ", 64, ba, NULL), 0); EXPECT_ERR(ERROR_INVALID_NAME);
}
TEST(path_names_fullpath_null_name_w) {
    REQUIRE(U(GetFullPathNameW));
    WCHAR bw[64];
    SET_ERR(0); EXPECT_EQ(U(GetFullPathNameW)(NULL, 64, bw, NULL), 0); EXPECT_ERR(ERROR_INVALID_NAME);
    SET_ERR(0); EXPECT_EQ(U(GetFullPathNameW)(NULL, 0, NULL, NULL), 0); EXPECT_ERR(ERROR_INVALID_NAME);
}
TEST(path_names_fullpath_filepart_basic) {
    REQUIRE(U(GetFullPathNameW)); REQUIRE(U(GetFullPathNameA));
    WCHAR bw[MAX_PATH], *fw = (WCHAR *)SENT; char ba[MAX_PATH], *fa = (char *)SENT;
    DWORD n = U(GetFullPathNameW)(L"C:\\dir\\file.txt", MAX_PATH, bw, &fw);
    EXPECT_EQ(n, 15); CHECK(fw == bw + 7); CHECK(fw && !wcscmp(fw, L"file.txt"));
    n = U(GetFullPathNameA)("C:\\dir\\file.txt", MAX_PATH, ba, &fa);
    EXPECT_EQ(n, 15); CHECK(fa == ba + 7); CHECK(fa && !strcmp(fa, "file.txt"));
}
TEST(path_names_fullpath_filepart_null_for_trailing_slash) {
    REQUIRE(U(GetFullPathNameW)); REQUIRE(U(GetFullPathNameA));
    static const char *ins[] = { "C:\\dir\\", "C:\\dir\\sub\\", "C:\\", "C:/dir/", "C:\\a\\b\\..\\" , "\\\\server\\share\\x\\", "\\\\?\\C:\\dir\\" };
    for (unsigned i = 0; i < sizeof ins / sizeof *ins; i++) {
        WCHAR bw[MAX_PATH], *fw = (WCHAR *)SENT; char ba[MAX_PATH], *fa = (char *)SENT;
        DWORD n = U(GetFullPathNameW)(pn_w(ins[i]), MAX_PATH, bw, &fw);
        CHECKF(n != 0, "%s", ins[i]);
        CHECKF(fw == NULL, "W %s: file part must be NULL (got %p)", ins[i], fw);
        n = U(GetFullPathNameA)(ins[i], MAX_PATH, ba, &fa);
        CHECKF(n != 0 && fa == NULL, "A %s: file part must be NULL", ins[i]);
    }
}
TEST(path_names_fullpath_filepart_dot_dotdot_results) {
    REQUIRE(U(GetFullPathNameW));
    /* "." and ".." components collapse before the file part is determined */
    WCHAR bw[MAX_PATH], *fw = (WCHAR *)SENT;
    U(GetFullPathNameW)(L"C:\\a\\b\\.", MAX_PATH, bw, &fw);
    CHECK(fw && !wcscmp(fw, L"b"));
    fw = (WCHAR *)SENT;
    U(GetFullPathNameW)(L"C:\\a\\b\\..", MAX_PATH, bw, &fw);
    CHECK(fw && !wcscmp(fw, L"a"));
    fw = (WCHAR *)SENT;
    U(GetFullPathNameW)(L"C:\\a\\..", MAX_PATH, bw, &fw);
    CHECK(fw == NULL);
    fw = (WCHAR *)SENT;
    U(GetFullPathNameW)(L"C:\\a\\b.txt.", MAX_PATH, bw, &fw);
    CHECK(fw && !wcscmp(fw, L"b.txt"));
}
TEST(path_names_fullpath_filepart_unc_and_prefixed) {
    REQUIRE(U(GetFullPathNameW));
    WCHAR bw[MAX_PATH], *fw = (WCHAR *)SENT;
    U(GetFullPathNameW)(L"\\\\srv\\sh\\dir\\leaf", MAX_PATH, bw, &fw);
    CHECK(fw && !wcscmp(fw, L"leaf"));
    fw = (WCHAR *)SENT;
    U(GetFullPathNameW)(L"\\\\?\\C:\\dir\\leaf", MAX_PATH, bw, &fw);
    CHECK(fw && !wcscmp(fw, L"leaf"));
    fw = (WCHAR *)SENT;
    U(GetFullPathNameW)(L"\\\\.\\C:\\dir\\x\\..\\leaf", MAX_PATH, bw, &fw);
    CHECK(fw && !wcscmp(fw, L"leaf"));
    fw = (WCHAR *)SENT;
    DWORD n = U(GetFullPathNameW)(L"\\\\srv\\sh", MAX_PATH, bw, &fw);
    t_note("UNC \\\\srv\\sh filepart %s (n=%lu)", fw ? pn_n(fw) : "NULL", n);
}
TEST(path_names_fullpath_filepart_not_touched_on_failure) {
    REQUIRE(U(GetFullPathNameW));
    WCHAR bw[8], *fw = (WCHAR *)SENT;
    SET_ERR(0);
    DWORD n = U(GetFullPathNameW)(L"", 8, bw, &fw);
    EXPECT_EQ(n, 0);
    t_note("filepart after empty-name failure: %p (NULL expected? sentinel=%p)", fw, (void *)SENT);
}
TEST(path_names_fullpath_filepart_null_ptr_ok) {
    REQUIRE(U(GetFullPathNameW)); REQUIRE(U(GetFullPathNameA));
    WCHAR bw[MAX_PATH]; char ba[MAX_PATH];
    EXPECT_EQ(U(GetFullPathNameW)(L"C:\\x\\y", MAX_PATH, bw, NULL), 6);
    EXPECT_EQ(U(GetFullPathNameA)("C:\\x\\y", MAX_PATH, ba, NULL), 6);
}
TEST(path_names_fullpath_buffer_sizes_w) {
    REQUIRE(U(GetFullPathNameW));
    const WCHAR *in = L"C:\\dir\\file.txt"; /* 15 chars */
    WCHAR bw[64], *fw;
    /* size query with 0 and NULL buffer: required size including the NUL */
    EXPECT_EQ(U(GetFullPathNameW)(in, 0, NULL, NULL), 16);
    EXPECT_EQ(U(GetFullPathNameW)(in, 0, bw, NULL), 16);
    for (int sz = 1; sz <= 15; sz++) {
        fw = (WCHAR *)SENT;
        DWORD n = U(GetFullPathNameW)(in, sz, bw, &fw);
        CHECKF(n == 16, "size %d returned %lu (want required 16)", sz, n);
    }
    pn_fill(bw, 40, L'Z');
    fw = NULL;
    DWORD n = U(GetFullPathNameW)(in, 16, bw, &fw);
    EXPECT_EQ(n, 15);
    CHECK(!wcscmp(bw, in));
    CHECK(fw == bw + 7);
    EXPECT_EQ(bw[16], L'Z'); /* nothing written past the NUL */
    n = U(GetFullPathNameW)(in, 64, bw, NULL);
    EXPECT_EQ(n, 15);
}
TEST(path_names_fullpath_buffer_sizes_a) {
    REQUIRE(U(GetFullPathNameA));
    const char *in = "C:\\dir\\file.txt";
    char ba[64], *fa;
    EXPECT_EQ(U(GetFullPathNameA)(in, 0, NULL, NULL), 16);
    EXPECT_EQ(U(GetFullPathNameA)(in, 0, ba, NULL), 16);
    for (int sz = 1; sz <= 15; sz++) {
        DWORD n = U(GetFullPathNameA)(in, sz, ba, NULL);
        CHECKF(n == 16, "size %d returned %lu", sz, n);
    }
    memset(ba, 'Z', sizeof ba);
    fa = NULL;
    EXPECT_EQ(U(GetFullPathNameA)(in, 16, ba, &fa), 15);
    EXPECT_STR(ba, in);
    CHECK(fa == ba + 7);
    EXPECT_EQ(ba[16], 'Z');
}
TEST(path_names_fullpath_buffer_exactly_len_returns_len_plus_one) {
    REQUIRE(U(GetFullPathNameW));
    WCHAR bw[64];
    EXPECT_EQ(U(GetFullPathNameW)(L"C:\\a", 4, bw, NULL), 5);
    EXPECT_EQ(U(GetFullPathNameW)(L"C:\\a", 5, bw, NULL), 4);
    EXPECT_EQ(U(GetFullPathNameW)(L"C:\\", 3, bw, NULL), 4);
    EXPECT_EQ(U(GetFullPathNameW)(L"C:\\", 4, bw, NULL), 3);
}
TEST(path_names_fullpath_repeated_calls_stable) {
    REQUIRE(U(GetFullPathNameW));
    WCHAR a[MAX_PATH], b[MAX_PATH];
    DWORD n0 = U(GetFullPathNameW)(L"C:\\x\\..\\y\\z", MAX_PATH, a, NULL);
    for (int i = 0; i < 500; i++) {
        DWORD n = U(GetFullPathNameW)(L"C:\\x\\..\\y\\z", MAX_PATH, b, NULL);
        if (n != n0 || wcscmp(a, b)) { CHECKF(0, "iteration %d differs", i); return; }
    }
    CHECK(1);
}
TEST(path_names_fullpath_device_names_w) {
    REQUIRE(U(GetFullPathNameW));
    static const char *devs[] = { "NUL", "CON", "PRN", "AUX", "COM1", "COM2", "COM9", "LPT1", "LPT9", "nul", "con", "Aux", "com3" };
    for (unsigned i = 0; i < sizeof devs / sizeof *devs; i++) {
        WCHAR bw[64], *fw = (WCHAR *)SENT; char want[32];
        snprintf(want, sizeof want, "\\\\.\\%s", devs[i]);
        DWORD n = U(GetFullPathNameW)(pn_w(devs[i]), 64, bw, &fw);
        CHECKF(n == strlen(want) && pn_weq_ci(bw, want), "%s -> %s (n=%lu)", devs[i], pn_n(bw), n);
        CHECKF(fw == NULL, "%s: file part for a device name is NULL (got %p)", devs[i], fw);
    }
}
TEST(path_names_fullpath_device_names_a) {
    REQUIRE(U(GetFullPathNameA));
    static const char *devs[] = { "NUL", "CON", "PRN", "AUX", "COM1", "LPT1" };
    for (unsigned i = 0; i < sizeof devs / sizeof *devs; i++) {
        char ba[64], want[32]; char *fa = (char *)SENT;
        snprintf(want, sizeof want, "\\\\.\\%s", devs[i]);
        DWORD n = U(GetFullPathNameA)(devs[i], 64, ba, &fa);
        CHECKF(n == strlen(want) && !_stricmp(ba, want), "%s -> %s", devs[i], ba);
        CHECK(fa == NULL);
    }
}
TEST(path_names_fullpath_device_name_with_directory) {
    REQUIRE(U(GetFullPathNameW));
    static const char *ins[] = { "C:\\dir\\NUL", "C:\\NUL", "\\dir\\CON", ".\\NUL", "..\\NUL", "sub\\AUX" };
    for (unsigned i = 0; i < sizeof ins / sizeof *ins; i++) {
        WCHAR bw[MAX_PATH];
        DWORD n = U(GetFullPathNameW)(pn_w(ins[i]), MAX_PATH, bw, NULL);
        CHECKF(n >= 7 && !wcsncmp(bw, L"\\\\.\\", 4), "%s -> %s", ins[i], pn_n(bw));
    }
}
TEST(path_names_fullpath_device_name_with_extension_or_colon) {
    REQUIRE(U(GetFullPathNameW));
    static const char *ins[] = { "NUL.txt", "con.anything", "NUL:", "COM1:", "LPT1.x.y", "NUL .txt" };
    for (unsigned i = 0; i < sizeof ins / sizeof *ins; i++) {
        WCHAR bw[MAX_PATH];
        DWORD n = U(GetFullPathNameW)(pn_w(ins[i]), MAX_PATH, bw, NULL);
        t_note("%s -> %s (n=%lu)", ins[i], pn_n(bw), n);
    }
}
TEST(path_names_fullpath_non_device_lookalikes) {
    REQUIRE(U(GetFullPathNameW));
    static const char *ins[] = { "NULL", "CONSOLE", "COM0", "LPT0", "COMX", "AUXX", "PRNT", "NU", "COM10" };
    for (unsigned i = 0; i < sizeof ins / sizeof *ins; i++) {
        WCHAR bw[MAX_PATH];
        DWORD n = U(GetFullPathNameW)(pn_w(ins[i]), MAX_PATH, bw, NULL);
        CHECKF(n != 0 && wcsncmp(bw, L"\\\\.\\", 4) != 0, "%s -> %s", ins[i], pn_n(bw));
    }
}
TEST(path_names_fullpath_device_buffer_semantics) {
    REQUIRE(U(GetFullPathNameW));
    WCHAR bw[16];
    /* "\\.\NUL" is 7 characters */
    EXPECT_EQ(U(GetFullPathNameW)(L"NUL", 0, NULL, NULL), 8);
    EXPECT_EQ(U(GetFullPathNameW)(L"NUL", 7, bw, NULL), 8);
    EXPECT_EQ(U(GetFullPathNameW)(L"NUL", 8, bw, NULL), 7);
    CHECK(pn_weq(bw, "\\\\.\\NUL"));
}
TEST(path_names_fullpath_relative_matches_cwd) {
    REQUIRE(U(GetFullPathNameW)); REQUIRE(U(GetFullPathNameA));
    char cwd[MAX_PATH], want[MAX_PATH * 2], got[MAX_PATH * 2];
    GetCurrentDirectoryA(sizeof cwd, cwd);
    size_t l = strlen(cwd);
    int root = l >= 1 && cwd[l - 1] == '\\';
    snprintf(want, sizeof want, "%s%sfoo.txt", cwd, root ? "" : "\\");
    DWORD n = U(GetFullPathNameA)("foo.txt", sizeof got, got, NULL);
    CHECKF(n == strlen(want) && !strcmp(got, want), "got %s want %s", got, want);
    snprintf(want, sizeof want, "%s%ssub\\x", cwd, root ? "" : "\\");
    n = U(GetFullPathNameA)("sub/x", sizeof got, got, NULL);
    CHECKF(!strcmp(got, want), "got %s want %s", got, want);
    snprintf(want, sizeof want, "%s%sfoo.txt", cwd, root ? "" : "\\");
    n = U(GetFullPathNameA)(".\\foo.txt", sizeof got, got, NULL);
    CHECKF(!strcmp(got, want), "got %s want %s", got, want);
    n = U(GetFullPathNameA)("sub\\..\\foo.txt", sizeof got, got, NULL);
    CHECKF(!strcmp(got, want), "got %s want %s", got, want);
}
TEST(path_names_fullpath_dot_is_cwd) {
    REQUIRE(U(GetFullPathNameW));
    WCHAR cwd[MAX_PATH], got[MAX_PATH], *fw = (WCHAR *)SENT;
    GetCurrentDirectoryW(MAX_PATH, cwd);
    DWORD n = U(GetFullPathNameW)(L".", MAX_PATH, got, &fw);
    CHECK(n == wcslen(cwd) && !wcscmp(got, cwd));
    /* "." resolves to the current directory; its last component is the file part unless root */
    if (wcslen(cwd) > 3) CHECK(fw && !wcscmp(fw, wcsrchr(cwd, L'\\') + 1));
    else CHECK(fw == NULL);
}
TEST(path_names_fullpath_dotdot_is_parent_of_cwd) {
    REQUIRE(U(GetFullPathNameW));
    WCHAR cwd[MAX_PATH], got[MAX_PATH], want[MAX_PATH];
    GetCurrentDirectoryW(MAX_PATH, cwd);
    wcscpy(want, cwd);
    WCHAR *s = wcsrchr(want, L'\\');
    if (s && s > want + 2) *s = 0; else wcscpy(want + 3, L"");
    DWORD n = U(GetFullPathNameW)(L"..", MAX_PATH, got, NULL);
    CHECKF(n == wcslen(got) && !_wcsicmp(got, want), "got %s want %s", pn_n(got), pn_n(want));
}
TEST(path_names_fullpath_rooted_uses_cwd_drive) {
    REQUIRE(U(GetFullPathNameW));
    WCHAR cwd[MAX_PATH], got[MAX_PATH];
    GetCurrentDirectoryW(MAX_PATH, cwd);
    if (cwd[1] != L':') { t_skip("cwd has no drive letter"); return; }
    DWORD n = U(GetFullPathNameW)(L"\\foo\\bar", MAX_PATH, got, NULL);
    CHECK(n == 11 || n == 10 + 0);
    CHECK(got[0] == cwd[0] && got[1] == L':' && !wcscmp(got + 2, L"\\foo\\bar"));
    n = U(GetFullPathNameW)(L"/foo/bar", MAX_PATH, got, NULL);
    CHECK(got[0] == cwd[0] && !wcscmp(got + 2, L"\\foo\\bar"));
    n = U(GetFullPathNameW)(L"\\", MAX_PATH, got, NULL);
    CHECK(n == 3 && got[0] == cwd[0] && !wcscmp(got + 1, L":\\"));
    n = U(GetFullPathNameW)(L"\\a\\..\\..\\b", MAX_PATH, got, NULL);
    CHECK(got[0] == cwd[0] && !wcscmp(got + 2, L"\\b"));
}
TEST(path_names_fullpath_drive_relative_other_drive_default_root) {
    REQUIRE(U(GetFullPathNameW));
    /* No per-drive cwd variable exists for an unused drive letter: resolves to its root.
       Pick a letter not equal to the current drive and with no "=X:" env var. */
    WCHAR cwd[MAX_PATH], got[MAX_PATH], var[4] = { L'=', 0, L':', 0 };
    GetCurrentDirectoryW(MAX_PATH, cwd);
    for (WCHAR c = L'Y'; c >= L'T'; c--) {
        if (c == towupper(cwd[0])) continue;
        var[1] = c;
        if (GetEnvironmentVariableW(var, got, MAX_PATH)) continue;
        WCHAR in[16]; swprintf(in, 16, L"%c:foo", c);
        DWORD n = U(GetFullPathNameW)(in, MAX_PATH, got, NULL);
        WCHAR want[16]; swprintf(want, 16, L"%c:\\foo", c);
        CHECKF(n == 6 && !_wcsicmp(got, want), "got %s", pn_n(got));
        swprintf(in, 16, L"%c:", c);
        n = U(GetFullPathNameW)(in, MAX_PATH, got, NULL);
        swprintf(want, 16, L"%c:\\", c);
        CHECKF(n == 3 && !_wcsicmp(got, want), "bare drive got %s", pn_n(got));
        return;
    }
    t_skip("no free drive letter");
}
TEST(path_names_fullpath_drive_relative_same_drive_uses_cwd) {
    REQUIRE(U(GetFullPathNameW));
    WCHAR cwd[MAX_PATH], got[MAX_PATH], in[MAX_PATH], want[MAX_PATH * 2];
    GetCurrentDirectoryW(MAX_PATH, cwd);
    if (cwd[1] != L':') { t_skip("no drive"); return; }
    swprintf(in, MAX_PATH, L"%c:foo", cwd[0]);
    size_t l = wcslen(cwd);
    swprintf(want, MAX_PATH * 2, L"%ls%ls", cwd, cwd[l - 1] == L'\\' ? L"foo" : L"\\foo");
    DWORD n = U(GetFullPathNameW)(in, MAX_PATH, got, NULL);
    CHECKF(n == wcslen(want) && !wcscmp(got, want), "got %s want %s", pn_n(got), pn_n(want));
    /* lower-case drive letter variant addresses the same drive */
    in[0] = towlower(in[0]);
    n = U(GetFullPathNameW)(in, MAX_PATH, got, NULL);
    CHECKF(n == wcslen(want) && !_wcsicmp(got, want), "lower: got %s", pn_n(got));
}
TEST(path_names_fullpath_long_path_over_max_path_w) {
    REQUIRE(U(GetFullPathNameW));
    static WCHAR in[1200], out[1200];
    wcscpy(in, L"C:");
    for (int i = 0; i < 12; i++) wcscat(in, L"\\0123456789012345678901234567890123456789"); /* 12*41+2 = 494 */
    DWORD n = U(GetFullPathNameW)(in, 1200, out, NULL);
    EXPECT_EQ(n, wcslen(in));
    CHECK(!wcscmp(in, out));
    EXPECT_EQ(U(GetFullPathNameW)(in, 0, NULL, NULL), wcslen(in) + 1);
}
TEST(path_names_fullpath_long_path_collapses_w) {
    REQUIRE(U(GetFullPathNameW));
    static WCHAR in[2400], out[2400];
    wcscpy(in, L"C:\\base");
    for (int i = 0; i < 100; i++) wcscat(in, L"\\sub\\..");
    wcscat(in, L"\\end");
    DWORD n = U(GetFullPathNameW)(in, 2400, out, NULL);
    EXPECT_EQ(n, 11);
    CHECK(pn_weq(out, "C:\\base\\end"));
}
TEST(path_names_fullpath_long_path_a_fails_over_max_path) {
    REQUIRE(U(GetFullPathNameA));
    static char in[800], out[800];
    strcpy(in, "C:");
    for (int i = 0; i < 8; i++) strcat(in, "\\0123456789012345678901234567890123456789"); /* 2+8*41 = 330 */
    SET_ERR(0);
    DWORD n = U(GetFullPathNameA)(in, sizeof out, out, NULL);
    EXPECT_EQ(n, 0);
    EXPECT_ERR(ERROR_FILENAME_EXCED_RANGE);
}
TEST(path_names_fullpath_a_limit_boundary_259_260_261) {
    REQUIRE(U(GetFullPathNameA)); REQUIRE(U(GetFullPathNameW));
    static char in[400], out[400];
    for (int len = 258; len <= 262; len++) {
        strcpy(in, "C:\\");
        int k = 3;
        while (k < len) { in[k++] = 'a'; }
        in[len] = 0;
        SET_ERR(0);
        DWORD n = U(GetFullPathNameA)(in, sizeof out, out, NULL);
        /* the ANSI worker uses a 520-byte scratch buffer: at most 259 chars + NUL */
        if (len <= 259) CHECKF(n == (DWORD)len, "len %d: n=%lu err=%lu", len, n, GetLastError());
        else { CHECKF(n == 0, "len %d: n=%lu", len, n); EXPECT_ERR(ERROR_FILENAME_EXCED_RANGE); }
    }
}
TEST(path_names_fullpath_max_char_component_w) {
    REQUIRE(U(GetFullPathNameW));
    static WCHAR in[1000], out[1000];
    wcscpy(in, L"C:\\");
    pn_fill(in + 3, 300, L'q'); /* single 300 char component: no 255 limit in path parsing */
    DWORD n = U(GetFullPathNameW)(in, 1000, out, NULL);
    EXPECT_EQ(n, 303);
}
TEST(path_names_fullpath_very_long_32k_w) {
    REQUIRE(U(GetFullPathNameW));
    static WCHAR in[34000], out[34000];
    wcscpy(in, L"\\\\?\\C:");
    int len = 6;
    while (len + 51 < 32000) { wcscpy(in + len, L"\\01234567890123456789012345678901234567890123456789"); len += 51; }
    DWORD n = U(GetFullPathNameW)(in, 34000, out, NULL);
    CHECKF(n == (DWORD)len, "n=%lu len=%d", n, len);
    /* and without the prefix, canonicalised */
    in[0] = L'C'; in[1] = L':';
    wcscpy(in + 2, in + 6);
    n = U(GetFullPathNameW)(in, 34000, out, NULL);
    CHECKF(n == (DWORD)(len - 4), "plain n=%lu want %d", n, len - 4);
}
TEST(path_names_fullpath_over_32k_w_note) {
    REQUIRE(U(GetFullPathNameW));
    static WCHAR in[40000], out[40000];
    wcscpy(in, L"C:");
    pn_fill(in + 2, 33000, L'a');
    in[2] = L'\\';
    SET_ERR(0);
    DWORD n = U(GetFullPathNameW)(in, 40000, out, NULL);
    t_note("33000-char path: n=%lu err=%lu", n, GetLastError());
    CHECK(n == 0 || n == 33002);
}
TEST(path_names_fullpath_wildcards_preserved) {
    REQUIRE(U(GetFullPathNameW));
    WCHAR bw[MAX_PATH], *fw;
    DWORD n = U(GetFullPathNameW)(L"C:\\d\\*.*", MAX_PATH, bw, &fw);
    EXPECT_EQ(n, 8); CHECK(pn_weq(bw, "C:\\d\\*.*")); CHECK(fw && !wcscmp(fw, L"*.*"));
    n = U(GetFullPathNameW)(L"C:\\d\\a?c", MAX_PATH, bw, &fw);
    CHECK(pn_weq(bw, "C:\\d\\a?c"));
    n = U(GetFullPathNameW)(L"C:\\d\\<>\"|", MAX_PATH, bw, &fw);
    CHECK(n == 9);
}
TEST(path_names_fullpath_colon_in_name_not_rejected) {
    REQUIRE(U(GetFullPathNameW));
    WCHAR bw[MAX_PATH];
    DWORD n = U(GetFullPathNameW)(L"C:\\d\\file:stream:$DATA", MAX_PATH, bw, NULL);
    CHECK(n != 0);
    t_note("stream path -> %s", pn_n(bw));
}
TEST(path_names_fullpath_trailing_space_dot_in_middle_kept) {
    REQUIRE(U(GetFullPathNameW));
    WCHAR bw[MAX_PATH];
    /* only the final component is trimmed; middle components keep trailing dots/spaces? note */
    DWORD n = U(GetFullPathNameW)(L"C:\\a.\\b ", MAX_PATH, bw, NULL);
    t_note("C:\\a.\\b  -> %s (n=%lu)", pn_n(bw), n);
    CHECK(n != 0);
    CHECK(pn_weq(bw, "C:\\a.\\b") || pn_weq(bw, "C:\\a\\b"));
}
TEST(path_names_fullpath_leading_spaces) {
    REQUIRE(U(GetFullPathNameW));
    WCHAR bw[MAX_PATH];
    DWORD n = U(GetFullPathNameW)(L"C:\\ a\\ b", MAX_PATH, bw, NULL);
    CHECK(pn_weq(bw, "C:\\ a\\ b"));
    EXPECT_EQ(n, 8);
}
TEST(path_names_fullpath_unicode_roundtrip_w) {
    REQUIRE(U(GetFullPathNameW));
    static const WCHAR in[] = { 'C', ':', '\\', 0x00E9, 0x4E2D, 0x6587, '\\', 0x0416, '.', 't', 'x', 't', 0 };
    WCHAR bw[MAX_PATH], *fw;
    DWORD n = U(GetFullPathNameW)(in, MAX_PATH, bw, &fw);
    CHECK(n == 12 && !wcscmp(bw, in));
    CHECK(fw == bw + 7);
}
TEST(path_names_fullpath_a_ansi_conversion) {
    REQUIRE(U(GetFullPathNameA)); REQUIRE(U(GetFullPathNameW));
    char ba[MAX_PATH], *fa = (char *)SENT;
    /* ASCII-only bytes always survive; high bytes are converted through the ANSI/OEM page and back */
    DWORD n = U(GetFullPathNameA)("C:\\abc\\def", MAX_PATH, ba, &fa);
    CHECK(n == 10 && fa == ba + 7 && !strcmp(fa, "def"));
    n = U(GetFullPathNameA)("C:\\a\xe9\\b", MAX_PATH, ba, &fa);
    CHECK(n != 0);
}
TEST(path_names_fullpath_a_filepart_offset_with_multibyte) {
    REQUIRE(U(GetFullPathNameA));
    char ba[MAX_PATH], *fa = (char *)SENT;
    DWORD n = U(GetFullPathNameA)("C:\\x\\..\\yy\\zzz", MAX_PATH, ba, &fa);
    CHECK(n == 9 && !strcmp(ba, "C:\\yy\\zzz") && fa == ba + 6);
}
TEST(path_names_fullpath_a_too_small_buffer_filepart) {
    REQUIRE(U(GetFullPathNameA));
    char ba[8] = "abcdefg", *fa = (char *)SENT;
    DWORD n = U(GetFullPathNameA)("C:\\dir\\file.txt", 8, ba, &fa);
    EXPECT_EQ(n, 16);
}
TEST(path_names_fullpath_kernel32_forwards_to_kernelbase) {
    REQUIRE(UK(GetFullPathNameW)); REQUIRE(UB(GetFullPathNameW));
    WCHAR a[MAX_PATH], b[MAX_PATH];
    DWORD na = UK(GetFullPathNameW)(L"C:\\q\\..\\r", MAX_PATH, a, NULL);
    DWORD nb = UB(GetFullPathNameW)(L"C:\\q\\..\\r", MAX_PATH, b, NULL);
    CHECK(na == nb && !wcscmp(a, b));
    REQUIRE(UK(GetFullPathNameA)); REQUIRE(UB(GetFullPathNameA));
    char sa[MAX_PATH], sb[MAX_PATH];
    CHECK(UK(GetFullPathNameA)("C:\\q\\..\\r", MAX_PATH, sa, NULL) == UB(GetFullPathNameA)("C:\\q\\..\\r", MAX_PATH, sb, NULL));
    CHECK(!strcmp(sa, sb));
}
TEST(path_names_fullpath_transacted_exports_exist) {
    CHECK(UK(GetFullPathNameTransactedW) != NULL || t_is_wine());
    CHECK(UK(GetFullPathNameTransactedA) != NULL || t_is_wine());
}
TEST(path_names_fullpath_transacted_null_transaction) {
    REQUIRE(UK(GetFullPathNameTransactedW));
    if (t_is_wine()) { t_skip("transacted stubs"); return; }
    WCHAR bw[MAX_PATH], *fw;
    SET_ERR(0);
    DWORD n = UK(GetFullPathNameTransactedW)(L"C:\\a\\..\\b", MAX_PATH, bw, &fw, NULL);
    CHECK(n == 4 && pn_weq(bw, "C:\\b"));
}
