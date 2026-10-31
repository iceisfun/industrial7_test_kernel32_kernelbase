/* kernelbase PathCch*: backslash, extension, file spec, root and prefix helpers */
#include "pn.h"
#include <pathcch.h>

/* ------------------------- AddBackslash / AddBackslashEx ------------------------- */
TEST(path_names_pathcch_addbackslash_table) {
    REQUIRE(U(PathCchAddBackslash));
    static const struct { const char *in, *out; HRESULT hr; } t[] = {
        { "C:\\a", "C:\\a\\", S_OK }, { "C:\\a\\", "C:\\a\\", S_FALSE }, { "", "", S_FALSE },
        { "C:", "C:\\", S_OK }, { "C:\\", "C:\\", S_FALSE }, { "a", "a\\", S_OK }, { "a\\b", "a\\b\\", S_OK },
        { "\\\\s\\sh", "\\\\s\\sh\\", S_OK }, { "\\", "\\", S_FALSE }, { "a/", "a/\\", S_OK }, { "a b", "a b\\", S_OK },
        { "\\\\?\\C:", "\\\\?\\C:\\", S_OK }, { "C:\\a\\\\", "C:\\a\\\\", S_FALSE },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        WCHAR w[MAX_PATH]; wcscpy(w, pn_w(t[i].in));
        HRESULT hr = U(PathCchAddBackslash)(w, MAX_PATH);
        CHECKF(hr == t[i].hr && pn_weq(w, t[i].out), "row %u [%s] hr 0x%lx out [%s]", i, t[i].in, hr, pn_n(w));
    }
}
TEST(path_names_pathcch_addbackslash_buffer_boundaries) {
    REQUIRE(U(PathCchAddBackslash));
    WCHAR w[16];
    wcscpy(w, L"C:\\a");              /* 4 chars */
    EXPECT_HR(U(PathCchAddBackslash)(w, 6), S_OK);
    CHECK(pn_weq(w, "C:\\a\\"));
    wcscpy(w, L"C:\\a");
    EXPECT_HR(U(PathCchAddBackslash)(w, 5), HR_INSUFFICIENT);   /* no room for backslash + NUL */
    CHECK(pn_weq(w, "C:\\a"));
    wcscpy(w, L"C:\\a");
    EXPECT_HR(U(PathCchAddBackslash)(w, 4), HR_INSUFFICIENT);   /* no room for the NUL of the existing string */
    wcscpy(w, L"C:\\a");
    EXPECT_HR(U(PathCchAddBackslash)(w, 0), HR_INSUFFICIENT);
    wcscpy(w, L"C:\\a\\");
    EXPECT_HR(U(PathCchAddBackslash)(w, 6), S_FALSE);
    wcscpy(w, L"C:\\a\\");
    EXPECT_HR(U(PathCchAddBackslash)(w, 5), HR_INSUFFICIENT);
}
TEST(path_names_pathcch_addbackslashex_pointers) {
    REQUIRE(U(PathCchAddBackslashEx));
    WCHAR w[32]; PWSTR end = (PWSTR)SENT; size_t rem = 0x1234;
    wcscpy(w, L"C:\\a");
    EXPECT_HR(U(PathCchAddBackslashEx)(w, 10, &end, &rem), S_OK);
    CHECK(end == w + 5 && *end == 0);
    EXPECT_EQ(rem, 5);
    wcscpy(w, L"C:\\a\\"); end = (PWSTR)SENT; rem = 0x1234;
    EXPECT_HR(U(PathCchAddBackslashEx)(w, 10, &end, &rem), S_FALSE);
    CHECK(end == w + 5);
    EXPECT_EQ(rem, 5);
    wcscpy(w, L""); end = (PWSTR)SENT; rem = 0x1234;
    EXPECT_HR(U(PathCchAddBackslashEx)(w, 10, &end, &rem), S_FALSE);
    CHECK(end == w);
    EXPECT_EQ(rem, 10);
    /* failure zeroes both outputs */
    wcscpy(w, L"C:\\a"); end = (PWSTR)SENT; rem = 0x1234;
    EXPECT_HR(U(PathCchAddBackslashEx)(w, 4, &end, &rem), HR_INSUFFICIENT);
    CHECK(end == NULL); EXPECT_EQ(rem, 0);
    /* either output may be NULL */
    wcscpy(w, L"C:\\a");
    EXPECT_HR(U(PathCchAddBackslashEx)(w, 10, NULL, NULL), S_OK);
    wcscpy(w, L"C:\\a");
    EXPECT_HR(U(PathCchAddBackslashEx)(w, 10, &end, NULL), S_OK);
    wcscpy(w, L"C:\\a");
    EXPECT_HR(U(PathCchAddBackslashEx)(w, 10, NULL, &rem), S_OK);
    EXPECT_EQ(rem, 5);
}
TEST(path_names_pathcch_addbackslash_exact_fit_remaining_one) {
    REQUIRE(U(PathCchAddBackslashEx));
    WCHAR w[8]; PWSTR end; size_t rem;
    wcscpy(w, L"abc");
    EXPECT_HR(U(PathCchAddBackslashEx)(w, 5, &end, &rem), S_OK);
    CHECK(pn_weq(w, "abc\\")); CHECK(end == w + 4);
    EXPECT_EQ(rem, 1);
}

/* ------------------------- RemoveBackslash / Ex ------------------------- */
TEST(path_names_pathcch_removebackslash_table) {
    REQUIRE(U(PathCchRemoveBackslash));
    static const struct { const char *in, *out; HRESULT hr; int hard; } t[] = {
        { "C:\\a\\", "C:\\a", S_OK, 1 }, { "C:\\a", "C:\\a", S_FALSE, 1 }, { "C:\\", "C:\\", S_FALSE, 1 },
        { "", "", S_FALSE, 1 }, { "\\", "\\", S_FALSE, 1 }, { "a\\", "a", S_OK, 1 }, { "a\\b\\", "a\\b", S_OK, 1 },
        { "\\\\s\\sh\\", "\\\\s\\sh", S_OK, 1 }, { "\\\\s\\sh", "\\\\s\\sh", S_FALSE, 1 }, { "C:\\a\\\\", "C:\\a\\", S_OK, 1 },
        { "C:", "C:", S_FALSE, 1 }, { "C:\\a b\\", "C:\\a b", S_OK, 1 }, { "\\\\?\\C:\\", "\\\\?\\C:\\", S_FALSE, 1 },
        { "\\\\?\\C:\\a\\", "\\\\?\\C:\\a", S_OK, 1 }, { "a/", "a/", S_FALSE, 1 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        WCHAR w[MAX_PATH]; wcscpy(w, pn_w(t[i].in));
        HRESULT hr = U(PathCchRemoveBackslash)(w, MAX_PATH);
        CHECKF(hr == t[i].hr && pn_weq(w, t[i].out), "row %u [%s] hr 0x%lx out [%s]", i, t[i].in, hr, pn_n(w));
    }
}
TEST(path_names_pathcch_removebackslash_cch_validation) {
    REQUIRE(U(PathCchRemoveBackslash));
    WCHAR w[16];
    wcscpy(w, L"C:\\a\\");     /* 5 chars */
    EXPECT_HR(U(PathCchRemoveBackslash)(w, 5), E_INVALIDARG);   /* NUL must lie inside cch */
    EXPECT_HR(U(PathCchRemoveBackslash)(w, 0), E_INVALIDARG);
    EXPECT_HR(U(PathCchRemoveBackslash)(w, 6), S_OK);
    CHECK(pn_weq(w, "C:\\a"));
}
TEST(path_names_pathcch_removebackslashex_pointers) {
    REQUIRE(U(PathCchRemoveBackslashEx));
    WCHAR w[32]; PWSTR end = (PWSTR)SENT; size_t rem = 0x1234;
    wcscpy(w, L"C:\\a");
    EXPECT_HR(U(PathCchRemoveBackslashEx)(w, 10, &end, &rem), S_FALSE);
    CHECK(end == w + 4);
    EXPECT_EQ(rem, 6);
    /* root keeps its backslash: end points AT it */
    wcscpy(w, L"C:\\"); end = (PWSTR)SENT; rem = 0x1234;
    EXPECT_HR(U(PathCchRemoveBackslashEx)(w, 10, &end, &rem), S_FALSE);
    CHECK(end == w + 2);
    EXPECT_EQ(rem, 8);
    wcscpy(w, L""); end = (PWSTR)SENT; rem = 0x1234;
    EXPECT_HR(U(PathCchRemoveBackslashEx)(w, 10, &end, &rem), S_FALSE);
    CHECK(end == w);
    EXPECT_EQ(rem, 10);
    /* invalid cch zeroes the outputs */
    wcscpy(w, L"C:\\a\\"); end = (PWSTR)SENT; rem = 0x1234;
    EXPECT_HR(U(PathCchRemoveBackslashEx)(w, 5, &end, &rem), E_INVALIDARG);
    CHECK(end == NULL); EXPECT_EQ(rem, 0);
    wcscpy(w, L"C:\\a\\");
    EXPECT_HR(U(PathCchRemoveBackslashEx)(w, 10, NULL, NULL), S_OK);
    CHECK(pn_weq(w, "C:\\a"));
}
TEST(path_names_pathcch_removebackslashex_pointers_after_removal) {
    REQUIRE(U(PathCchRemoveBackslashEx));
    WCHAR w[32]; PWSTR end = (PWSTR)SENT; size_t rem = 0x1234;
    wcscpy(w, L"C:\\a\\");
    HRESULT hr = U(PathCchRemoveBackslashEx)(w, 10, &end, &rem);
    CHECK(hr == S_OK && pn_weq(w, "C:\\a"));
    t_note("after removal: end=%td rem=%zu (expect end at NUL, index 4, remaining 6)", end ? end - w : -1, rem);
    CHECK(end == w + 4 && rem == 6);
}

/* ------------------------- FindExtension ------------------------- */
TEST(path_names_pathcch_findextension_table) {
    REQUIRE(U(PathCchFindExtension));
    static const struct { const char *in; const char *ext; } t[] = {
        { "a.txt", ".txt" }, { "C:\\dir\\a.txt", ".txt" }, { "a", "" }, { "a.b.c", ".c" }, { "C:\\d.d\\f", "" },
        { ".bashrc", ".bashrc" }, { "a.", "." }, { "a. b", "" }, { "a.txt ", "" }, { "a.tx t", "" },
        { "C:\\a\\.b", ".b" }, { "a\\.\\b", "" }, { "", "" }, { "...", "." }, { "a..b", ".b" }, { "a.b\\", "" },
        { "C:\\a.b\\c.d.e", ".e" }, { "dir.x\\", "" }, { "a.b.", "." }, { "\\\\s\\sh\\f.x", ".x" },
        { "\\\\?\\C:\\f.x", ".x" }, { "a.txt:stream", ".txt:stream" }, { "a b.c", ".c" }, { " .x", ".x" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        WCHAR *p = pn_w(t[i].in); PCWSTR ext = (PCWSTR)SENT;
        HRESULT hr = U(PathCchFindExtension)(p, MAX_PATH, &ext);
        CHECKF(hr == S_OK && ext && pn_weq(ext, t[i].ext) && ext >= p && ext <= p + wcslen(p), "row %u [%s] hr 0x%lx ext [%s]", i, t[i].in, hr, ext ? pn_n(ext) : "NULL");
    }
}
TEST(path_names_pathcch_findextension_no_ext_points_to_nul) {
    REQUIRE(U(PathCchFindExtension));
    WCHAR *p = pn_w("C:\\dir\\file"); PCWSTR ext = NULL;
    EXPECT_HR(U(PathCchFindExtension)(p, MAX_PATH, &ext), S_OK);
    CHECK(ext == p + wcslen(p));
    p = pn_w("file.tar.gz");
    EXPECT_HR(U(PathCchFindExtension)(p, MAX_PATH, &ext), S_OK);
    CHECK(ext == p + 8);
}
TEST(path_names_pathcch_findextension_cch_validation) {
    REQUIRE(U(PathCchFindExtension));
    WCHAR *p = pn_w("a.txt"); PCWSTR ext = (PCWSTR)SENT;
    EXPECT_HR(U(PathCchFindExtension)(p, 0, &ext), E_INVALIDARG);
    CHECK(ext == NULL);
    ext = (PCWSTR)SENT;
    EXPECT_HR(U(PathCchFindExtension)(p, 5, &ext), E_INVALIDARG);   /* NUL not within the 5 chars */
    CHECK(ext == NULL);
    EXPECT_HR(U(PathCchFindExtension)(p, 6, &ext), S_OK);
    EXPECT_HR(U(PathCchFindExtension)(p, 0x8001, &ext), E_INVALIDARG);
    EXPECT_HR(U(PathCchFindExtension)(p, (size_t)-1, &ext), E_INVALIDARG);
    ext = (PCWSTR)SENT;
    EXPECT_HR(U(PathCchFindExtension)(NULL, MAX_PATH, &ext), E_INVALIDARG);
    CHECK(ext == NULL);
}
TEST(path_names_pathcch_findextension_32k_limit) {
    REQUIRE(U(PathCchFindExtension));
    static WCHAR big[0x8002]; PCWSTR ext;
    pn_fill(big, 0x7ff0, L'a'); big[100] = L'.';
    EXPECT_HR(U(PathCchFindExtension)(big, 0x8000, &ext), S_OK);
    CHECK(ext == big + 100);
}

/* ------------------------- AddExtension / RenameExtension / RemoveExtension ------------------------- */
TEST(path_names_pathcch_addextension_table) {
    REQUIRE(U(PathCchAddExtension));
    static const struct { const char *in, *ext, *out; HRESULT hr; } t[] = {
        { "C:\\a", ".txt", "C:\\a.txt", S_OK }, { "C:\\a", "txt", "C:\\a.txt", S_OK }, { "C:\\a.b", ".txt", "C:\\a.b", S_FALSE },
        { "C:\\a", "", "C:\\a", S_OK }, { "C:\\a", ".", "C:\\a", S_OK }, { "a", ".x", "a.x", S_OK }, { "", ".x", ".x", S_OK },
        { "C:\\d.d\\f", ".x", "C:\\d.d\\f.x", S_OK }, { "C:\\a\\", ".x", "C:\\a\\.x", S_OK }, { "a.", ".x", "a.", S_FALSE },
        { "a ", ".x", "a .x", S_OK }, { ".hidden", ".x", ".hidden", S_FALSE },
        { "\\\\s\\sh\\f", ".x", "\\\\s\\sh\\f.x", S_OK },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        WCHAR w[MAX_PATH]; wcscpy(w, pn_w(t[i].in));
        HRESULT hr = U(PathCchAddExtension)(w, MAX_PATH, pn_w(t[i].ext));
        CHECKF(hr == t[i].hr && pn_weq(w, t[i].out), "row %u [%s]+[%s] hr 0x%lx out [%s]", i, t[i].in, t[i].ext, hr, pn_n(w));
    }
}
TEST(path_names_pathcch_addextension_invalid) {
    REQUIRE(U(PathCchAddExtension));
    WCHAR w[MAX_PATH];
    wcscpy(w, L"C:\\a");
    EXPECT_HR(U(PathCchAddExtension)(NULL, MAX_PATH, L".x"), E_INVALIDARG);
    EXPECT_HR(U(PathCchAddExtension)(w, 0, L".x"), E_INVALIDARG);
    EXPECT_HR(U(PathCchAddExtension)(w, 0x8001, L".x"), E_INVALIDARG);
    EXPECT_HR(U(PathCchAddExtension)(w, MAX_PATH, NULL), E_INVALIDARG);
    CHECK(pn_weq(w, "C:\\a"));
    static const char *odd[] = { ".tar.gz", "a b", ".a b", ".a\\b", "\\x", ".x ", " .x", "a:b" };
    for (unsigned i = 0; i < sizeof odd / sizeof *odd; i++) {
        wcscpy(w, L"C:\\a");
        HRESULT hr = U(PathCchAddExtension)(w, MAX_PATH, pn_w(odd[i]));
        t_note("AddExtension ext [%s] -> hr 0x%lx out [%s]", odd[i], hr, pn_n(w));
    }
}
TEST(path_names_pathcch_addextension_buffer_boundaries) {
    REQUIRE(U(PathCchAddExtension));
    WCHAR w[32];
    wcscpy(w, L"C:\\a");            /* + ".txt" = 8 chars */
    EXPECT_HR(U(PathCchAddExtension)(w, 9, L".txt"), S_OK);
    CHECK(pn_weq(w, "C:\\a.txt"));
    wcscpy(w, L"C:\\a");
    EXPECT_HR(U(PathCchAddExtension)(w, 8, L".txt"), HR_INSUFFICIENT);
    CHECK(pn_weq(w, "C:\\a"));       /* restored on failure */
    wcscpy(w, L"C:\\a");
    EXPECT_HR(U(PathCchAddExtension)(w, 8, L"txt"), HR_INSUFFICIENT);
    CHECK(pn_weq(w, "C:\\a"));
}
TEST(path_names_pathcch_addextension_legacy_limit_clamps_cch) {
    REQUIRE(U(PathCchAddExtension));
    static WCHAR w[2000];
    wcscpy(w, L"C:\\"); pn_fill(w + 3, 254, L'a');           /* 257 chars; +".txt" = 261 > 259 */
    HRESULT hr = U(PathCchAddExtension)(w, 1000, L".txt");
    CHECKF(hr == HR_EXCED, "cch 1000 for non-prefixed path: hr 0x%lx", hr);
    CHECK(wcslen(w) == 257);
    wcscpy(w, L"C:\\"); pn_fill(w + 3, 252, L'a');           /* 255 + 4 = 259: fits */
    EXPECT_HR(U(PathCchAddExtension)(w, 1000, L".txt"), S_OK);
    CHECK(wcslen(w) == 259);
    /* \\?\ paths are not clamped */
    wcscpy(w, L"\\\\?\\C:\\"); pn_fill(w + 7, 400, L'a');
    EXPECT_HR(U(PathCchAddExtension)(w, 1000, L".txt"), S_OK);
    CHECK(wcslen(w) == 407 + 4);
    wcscpy(w, L"\\\\?\\C:\\"); pn_fill(w + 7, 400, L'a');
    EXPECT_HR(U(PathCchAddExtension)(w, 0x8000, L".txt"), S_OK);
}
TEST(path_names_pathcch_renameextension_table) {
    REQUIRE(U(PathCchRenameExtension));
    static const struct { const char *in, *ext, *out; } t[] = {
        { "C:\\a.txt", ".bak", "C:\\a.bak" }, { "C:\\a", ".bak", "C:\\a.bak" }, { "C:\\a.txt", "bak", "C:\\a.bak" },
        { "C:\\a.txt", "", "C:\\a" }, { "C:\\a.txt", ".", "C:\\a" }, { "C:\\a.b.c", ".x", "C:\\a.b.x" }, { "a", ".x", "a.x" },
        { "C:\\d.d\\f", ".x", "C:\\d.d\\f.x" }, { "C:\\a", "", "C:\\a" }, { ".hidden", ".x", ".x" }, { "a.", ".x", "a.x" },
        { "C:\\a.txt", ".longerextension", "C:\\a.longerextension" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        WCHAR w[MAX_PATH]; wcscpy(w, pn_w(t[i].in));
        HRESULT hr = U(PathCchRenameExtension)(w, MAX_PATH, pn_w(t[i].ext));
        CHECKF(hr == S_OK && pn_weq(w, t[i].out), "row %u [%s]->[%s] hr 0x%lx out [%s]", i, t[i].in, t[i].ext, hr, pn_n(w));
    }
}
TEST(path_names_pathcch_renameextension_invalid_and_overflow) {
    REQUIRE(U(PathCchRenameExtension));
    WCHAR w[32];
    wcscpy(w, L"C:\\a.b");
    EXPECT_HR(U(PathCchRenameExtension)(NULL, MAX_PATH, L".x"), E_INVALIDARG);
    EXPECT_HR(U(PathCchRenameExtension)(w, 0, L".x"), E_INVALIDARG);
    EXPECT_HR(U(PathCchRenameExtension)(w, 0x8001, L".x"), E_INVALIDARG);
    EXPECT_HR(U(PathCchRenameExtension)(w, MAX_PATH, NULL), E_INVALIDARG);
    EXPECT_HR(U(PathCchRenameExtension)(w, 7, L".xyz"), HR_INSUFFICIENT);   /* C:\a.xyz needs 9 */
    wcscpy(w, L"C:\\a.b");
    EXPECT_HR(U(PathCchRenameExtension)(w, 9, L".xyz"), S_OK);
    CHECK(pn_weq(w, "C:\\a.xyz"));
}
TEST(path_names_pathcch_removeextension_table) {
    REQUIRE(U(PathCchRemoveExtension));
    static const struct { const char *in, *out; HRESULT hr; } t[] = {
        { "C:\\a.txt", "C:\\a", S_OK }, { "C:\\a", "C:\\a", S_FALSE }, { "a.b.c", "a.b", S_OK }, { ".bashrc", "", S_OK },
        { "C:\\d.d\\f", "C:\\d.d\\f", S_FALSE }, { "a.txt ", "a.txt ", S_FALSE }, { "a.", "a", S_OK }, { "", "", S_FALSE },
        { "\\\\s\\sh\\f.x", "\\\\s\\sh\\f", S_OK }, { "a. b", "a. b", S_FALSE }, { "C:\\a.b\\", "C:\\a.b\\", S_FALSE },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        WCHAR w[MAX_PATH]; wcscpy(w, pn_w(t[i].in));
        HRESULT hr = U(PathCchRemoveExtension)(w, MAX_PATH);
        CHECKF(hr == t[i].hr && pn_weq(w, t[i].out), "row %u [%s] hr 0x%lx out [%s]", i, t[i].in, hr, pn_n(w));
    }
}
TEST(path_names_pathcch_removeextension_invalid) {
    REQUIRE(U(PathCchRemoveExtension));
    WCHAR w[16]; wcscpy(w, L"a.b");
    EXPECT_HR(U(PathCchRemoveExtension)(NULL, MAX_PATH), E_INVALIDARG);
    EXPECT_HR(U(PathCchRemoveExtension)(w, 0), E_INVALIDARG);
    EXPECT_HR(U(PathCchRemoveExtension)(w, 0x8001), E_INVALIDARG);
    EXPECT_HR(U(PathCchRemoveExtension)(w, 3), E_INVALIDARG);   /* NUL outside cch */
    EXPECT_HR(U(PathCchRemoveExtension)(w, 4), S_OK);
    CHECK(pn_weq(w, "a"));
}

/* ------------------------- RemoveFileSpec ------------------------- */
TEST(path_names_pathcch_removefilespec_table) {
    REQUIRE(U(PathCchRemoveFileSpec));
    static const struct { const char *in, *out; HRESULT hr; int hard; } t[] = {
        { "C:\\a\\b.txt", "C:\\a", S_OK, 1 }, { "C:\\a\\b", "C:\\a", S_OK, 1 }, { "C:\\a", "C:\\", S_OK, 1 }, { "C:\\", "C:\\", S_FALSE, 1 },
        { "C:\\a\\", "C:\\a", S_OK, 1 }, { "C:\\a\\b\\", "C:\\a\\b", S_OK, 1 }, { "C:\\a\\b\\c\\d.txt", "C:\\a\\b\\c", S_OK, 1 },
        { "C:", "C:", S_FALSE, 1 }, { "\\\\s\\sh\\a", "\\\\s\\sh", S_OK, 1 }, { "\\\\s\\sh\\a\\b", "\\\\s\\sh\\a", S_OK, 1 },
        { "\\\\s\\sh", "\\\\s\\sh", S_FALSE, 1 }, { "a\\b", "a", S_OK, 1 }, { "a", "", S_OK, 1 }, { "", "", S_FALSE, 1 },
        { "\\a", "\\", S_OK, 1 }, { "\\", "\\", S_FALSE, 1 }, { "\\a\\b", "\\a", S_OK, 1 }, { "\\\\?\\C:\\a\\b", "\\\\?\\C:\\a", S_OK, 1 },
        { "\\\\?\\C:\\", "\\\\?\\C:\\", S_FALSE, 1 }, { "\\\\?\\C:\\a", "\\\\?\\C:\\", S_OK, 1 }, { "C:\\a b\\c d.txt", "C:\\a b", S_OK, 1 },
        { "a\\b\\c", "a\\b", S_OK, 1 }, { ".\\a", ".", S_OK, 1 }, { "\\\\s\\sh\\", "\\\\s\\sh", S_OK, 0 }, { "C:\\a\\\\b", "C:\\a\\", S_OK, 0 },
        { "C:a", "C:", S_OK, 0 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        WCHAR w[MAX_PATH]; wcscpy(w, pn_w(t[i].in));
        HRESULT hr = U(PathCchRemoveFileSpec)(w, MAX_PATH);
        int ok = hr == t[i].hr && pn_weq(w, t[i].out);
        if (t[i].hard) CHECKF(ok, "row %u [%s] hr 0x%lx out [%s] want [%s]", i, t[i].in, hr, pn_n(w), t[i].out);
        else if (!ok) t_note("RemoveFileSpec [%s] -> hr 0x%lx [%s] (model [%s])", t[i].in, hr, pn_n(w), t[i].out);
    }
}
TEST(path_names_pathcch_removefilespec_repeat_walks_to_root) {
    REQUIRE(U(PathCchRemoveFileSpec));
    WCHAR w[MAX_PATH]; wcscpy(w, L"C:\\a\\b\\c");
    static const char *want[] = { "C:\\a\\b", "C:\\a", "C:\\" };
    for (int i = 0; i < 3; i++) { EXPECT_HR(U(PathCchRemoveFileSpec)(w, MAX_PATH), S_OK); CHECK(pn_weq(w, want[i])); }
    EXPECT_HR(U(PathCchRemoveFileSpec)(w, MAX_PATH), S_FALSE);
    CHECK(pn_weq(w, "C:\\"));
}
TEST(path_names_pathcch_removefilespec_invalid) {
    REQUIRE(U(PathCchRemoveFileSpec));
    WCHAR w[MAX_PATH]; wcscpy(w, L"C:\\a\\b");
    EXPECT_HR(U(PathCchRemoveFileSpec)(NULL, MAX_PATH), E_INVALIDARG);
    EXPECT_HR(U(PathCchRemoveFileSpec)(w, 0), E_INVALIDARG);
    EXPECT_HR(U(PathCchRemoveFileSpec)(w, 0x8001), E_INVALIDARG);
    EXPECT_HR(U(PathCchRemoveFileSpec)(w, 3), E_INVALIDARG);    /* root end lies outside cch */
}

/* ------------------------- SkipRoot ------------------------- */
TEST(path_names_pathcch_skiproot_table) {
    REQUIRE(U(PathCchSkipRoot));
    static const struct { const char *in, *rest; int hard; } t[] = {
        { "C:\\a", "a", 1 }, { "C:\\", "", 1 }, { "C:", "", 1 }, { "c:\\a\\b", "a\\b", 1 }, { "\\a", "a", 1 }, { "\\", "", 1 },
        { "\\\\s\\sh\\a\\b", "a\\b", 1 }, { "\\\\s\\sh", "", 1 }, { "\\\\s\\sh\\", "", 1 }, { "\\\\s", "", 1 }, { "\\\\", "", 0 },
        { "\\\\?\\C:\\a", "a", 1 }, { "\\\\?\\C:\\", "", 1 }, { "\\\\?\\C:", "", 1 },
        { "\\\\?\\UNC\\s\\sh\\a", "a", 0 }, { "\\\\?\\Volume{01234567-89ab-cdef-0123-456789abcdef}\\a", "a", 1 },
        { "\\\\?\\Volume{01234567-89ab-cdef-0123-456789abcdef}\\", "", 1 }, { "C:a", "a", 0 }, { "\\\\.\\C:\\a", "a", 0 },
        { "C:\\a\\..\\b", "a\\..\\b", 1 }, { "C://a", "/a", 0 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        WCHAR *p = pn_w(t[i].in); PCWSTR end = (PCWSTR)SENT;
        HRESULT hr = U(PathCchSkipRoot)(p, &end);
        int ok = hr == S_OK && end && pn_weq(end, t[i].rest) && end >= p && end <= p + wcslen(p);
        if (t[i].hard) CHECKF(ok, "row %u [%s] hr 0x%lx rest [%s] want [%s]", i, t[i].in, hr, end && end != (PCWSTR)SENT ? pn_n(end) : "?", t[i].rest);
        else if (!ok) t_note("SkipRoot [%s] -> hr 0x%lx rest [%s] (model [%s])", t[i].in, hr, end && end != (PCWSTR)SENT ? pn_n(end) : "?", t[i].rest);
    }
}
TEST(path_names_pathcch_skiproot_invalid) {
    REQUIRE(U(PathCchSkipRoot));
    static const char *bad[] = { "a\\b", "a", ".\\a", "..\\a", "1:\\a", "/a", "file.txt", "$:\\a" };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        PCWSTR end = (PCWSTR)SENT;
        HRESULT hr = U(PathCchSkipRoot)(pn_w(bad[i]), &end);
        CHECKF(hr == E_INVALIDARG, "[%s] hr 0x%lx", bad[i], hr);
    }
    PCWSTR end = (PCWSTR)SENT;
    EXPECT_HR(U(PathCchSkipRoot)(NULL, &end), E_INVALIDARG);
    EXPECT_HR(U(PathCchSkipRoot)(L"", &end), E_INVALIDARG);
    EXPECT_HR(U(PathCchSkipRoot)(L"C:\\a", NULL), E_INVALIDARG);
}
TEST(path_names_pathcch_skiproot_relative_leaves_output_null) {
    REQUIRE(U(PathCchSkipRoot));
    PCWSTR end = (PCWSTR)SENT;
    EXPECT_HR(U(PathCchSkipRoot)(L"a\\b", &end), E_INVALIDARG);
    CHECK(end == NULL);
}

/* ------------------------- StripToRoot ------------------------- */
TEST(path_names_pathcch_striptoroot_table) {
    REQUIRE(U(PathCchStripToRoot));
    static const struct { const char *in, *out; HRESULT hr; int hard; } t[] = {
        { "C:\\a\\b", "C:\\", S_OK, 1 }, { "C:\\", "C:\\", S_FALSE, 1 }, { "C:", "C:", S_FALSE, 1 }, { "\\a\\b", "\\", S_OK, 1 },
        { "\\\\s\\sh\\a\\b", "\\\\s\\sh", S_OK, 1 }, { "\\\\s\\sh", "\\\\s\\sh", S_FALSE, 1 }, { "\\\\?\\C:\\a\\b", "\\\\?\\C:\\", S_OK, 1 },
        { "C:\\a", "C:\\", S_OK, 1 }, { "\\", "\\", S_FALSE, 1 }, { "\\\\s\\sh\\a", "\\\\s\\sh", S_OK, 1 },
        { "\\\\?\\C:\\", "\\\\?\\C:\\", S_FALSE, 1 }, { "c:\\a b\\c", "c:\\", S_OK, 1 },
        { "\\\\?\\UNC\\s\\sh\\a", "\\\\?\\UNC\\s\\sh", S_OK, 0 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        WCHAR w[MAX_PATH]; wcscpy(w, pn_w(t[i].in));
        HRESULT hr = U(PathCchStripToRoot)(w, MAX_PATH);
        int ok = hr == t[i].hr && pn_weq(w, t[i].out);
        if (t[i].hard) CHECKF(ok, "row %u [%s] hr 0x%lx out [%s] want [%s]", i, t[i].in, hr, pn_n(w), t[i].out);
        else if (!ok) t_note("StripToRoot [%s] -> hr 0x%lx [%s] (model [%s])", t[i].in, hr, pn_n(w), t[i].out);
    }
}
TEST(path_names_pathcch_striptoroot_failure_clears_buffer) {
    REQUIRE(U(PathCchStripToRoot));
    static const char *bad[] = { "a\\b", "file.txt", ".\\x", "" };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        WCHAR w[MAX_PATH]; wcscpy(w, pn_w(bad[i]));
        HRESULT hr = U(PathCchStripToRoot)(w, MAX_PATH);
        CHECKF(hr == E_INVALIDARG, "[%s] hr 0x%lx", bad[i], hr);
        CHECKF(w[0] == 0, "[%s] buffer must be emptied on failure, got [%s]", bad[i], pn_n(w));
    }
}
TEST(path_names_pathcch_striptoroot_cch_validation) {
    REQUIRE(U(PathCchStripToRoot));
    WCHAR w[MAX_PATH]; wcscpy(w, L"C:\\a\\b");
    EXPECT_HR(U(PathCchStripToRoot)(NULL, MAX_PATH), E_INVALIDARG);
    EXPECT_HR(U(PathCchStripToRoot)(w, 0), E_INVALIDARG);
    EXPECT_HR(U(PathCchStripToRoot)(w, 0x8001), E_INVALIDARG);
    wcscpy(w, L"C:\\a\\b");
    EXPECT_HR(U(PathCchStripToRoot)(w, 3), E_INVALIDARG);
}

/* ------------------------- IsRoot / IsUNCEx ------------------------- */
TEST(path_names_pathcch_isroot_table) {
    REQUIRE(U(PathCchIsRoot));
    static const struct { const char *in; BOOL want; int hard; } t[] = {
        { "C:\\", 1, 1 }, { "c:\\", 1, 1 }, { "Z:\\", 1, 1 }, { "C:", 0, 1 }, { "C:\\a", 0, 1 }, { "C:\\a\\", 0, 1 }, { "\\", 1, 1 }, { "\\a", 0, 1 },
        { "", 0, 1 }, { "a", 0, 1 }, { "\\\\s\\sh", 1, 1 }, { "\\\\s\\sh\\", 0, 1 }, { "\\\\s\\sh\\a", 0, 1 }, { "\\\\s\\", 0, 1 },
        { "\\\\?\\C:\\", 1, 1 }, { "\\\\?\\C:", 0, 1 }, { "\\\\?\\C:\\a", 0, 1 }, { "C:\\\\", 0, 1 }, { "1:\\", 0, 1 },
        { "\\\\?\\Volume{01234567-89ab-cdef-0123-456789abcdef}\\", 1, 1 }, { "\\\\?\\Volume{01234567-89ab-cdef-0123-456789abcdef}", 0, 1 },
        { "\\\\?\\Volume{01234567-89ab-cdef-0123-456789abcdef}\\a", 0, 1 }, { "\\\\", 1, 0 }, { "\\\\s", 1, 0 }, { "\\\\?\\UNC\\s\\sh", 1, 0 },
        { "C:/", 0, 1 }, { "/", 0, 0 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        BOOL r = U(PathCchIsRoot)(pn_w(t[i].in));
        if (t[i].hard) CHECKF(!!r == t[i].want, "row %u [%s] got %d want %d", i, t[i].in, r, t[i].want);
        else if (!!r != t[i].want) t_note("IsRoot [%s] -> %d (model %d)", t[i].in, r, t[i].want);
    }
    CHECK(U(PathCchIsRoot)(NULL) == FALSE);
}
TEST(path_names_pathiscuncex_table) {
    REQUIRE(U(PathIsUNCEx));
    static const struct { const char *in; BOOL want; const char *server; int hard; } t[] = {
        { "\\\\server\\share", 1, "server\\share", 1 }, { "\\\\server", 1, "server", 1 }, { "\\\\server\\share\\a", 1, "server\\share\\a", 1 },
        { "C:\\a", 0, NULL, 1 }, { "\\a", 0, NULL, 1 }, { "a", 0, NULL, 1 }, { "", 0, NULL, 1 }, { "\\\\?\\C:\\a", 0, NULL, 1 }, { "/server/share", 0, NULL, 1 },
        { "\\\\?\\Volume{01234567-89ab-cdef-0123-456789abcdef}\\", 0, NULL, 1 }, { "\\\\", 1, "", 1 },
        { "\\\\?\\UNC\\s\\sh", 1, "s\\sh", 0 }, { "\\\\.\\C:\\", 1, "C:\\", 0 }, { "\\\\?\\unc\\s\\sh", 1, "s\\sh", 0 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        WCHAR *p = pn_w(t[i].in); PCWSTR srv = (PCWSTR)SENT;
        BOOL r = U(PathIsUNCEx)(p, &srv);
        int ok = !!r == t[i].want && (t[i].want ? (srv && pn_weq(srv, t[i].server)) : srv == NULL);
        if (t[i].hard) CHECKF(ok, "row %u [%s] got %d srv [%s]", i, t[i].in, r, srv && srv != (PCWSTR)SENT ? pn_n(srv) : "NULL");
        else if (!ok) t_note("IsUNCEx [%s] -> %d srv [%s] (model %d [%s])", t[i].in, r, srv && srv != (PCWSTR)SENT ? pn_n(srv) : "NULL", t[i].want, t[i].server ? t[i].server : "NULL");
    }
}
TEST(path_names_pathiscuncex_null_server_ok) {
    REQUIRE(U(PathIsUNCEx));
    CHECK(U(PathIsUNCEx)(L"\\\\s\\sh", NULL) == TRUE);
    CHECK(U(PathIsUNCEx)(L"C:\\x", NULL) == FALSE);
}

/* ------------------------- StripPrefix ------------------------- */
TEST(path_names_pathcch_stripprefix_table) {
    REQUIRE(U(PathCchStripPrefix));
    static const struct { const char *in, *out; HRESULT hr; int hard; } t[] = {
        { "\\\\?\\C:\\a", "C:\\a", S_OK, 1 }, { "\\\\?\\C:\\", "C:\\", S_OK, 1 }, { "\\\\?\\C:", "C:", S_OK, 1 }, { "\\\\?\\c:\\a\\b", "c:\\a\\b", S_OK, 1 },
        { "\\\\?\\UNC\\s\\sh\\a", "\\\\s\\sh\\a", S_OK, 1 }, { "\\\\?\\unc\\s\\sh", "\\\\s\\sh", S_OK, 1 }, { "\\\\?\\UNC\\s", "\\\\s", S_OK, 1 },
        { "C:\\a", "C:\\a", S_FALSE, 1 }, { "\\\\s\\sh", "\\\\s\\sh", S_FALSE, 1 }, { "a", "a", S_FALSE, 1 }, { "", "", S_FALSE, 1 }, { "\\\\.\\C:\\a", "\\\\.\\C:\\a", S_FALSE, 1 },
        { "\\\\?\\Volume{01234567-89ab-cdef-0123-456789abcdef}\\", "\\\\?\\Volume{01234567-89ab-cdef-0123-456789abcdef}\\", S_FALSE, 1 },
        { "\\\\?\\1:\\a", "\\\\?\\1:\\a", S_FALSE, 1 }, { "\\\\?\\C", "\\\\?\\C", S_FALSE, 1 }, { "\\\\?\\GLOBALROOT\\x", "\\\\?\\GLOBALROOT\\x", S_FALSE, 1 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        WCHAR w[MAX_PATH]; wcscpy(w, pn_w(t[i].in));
        HRESULT hr = U(PathCchStripPrefix)(w, MAX_PATH);
        int ok = hr == t[i].hr && pn_weq(w, t[i].out);
        if (t[i].hard) CHECKF(ok, "row %u [%s] hr 0x%lx out [%s] want [%s]", i, t[i].in, hr, pn_n(w), t[i].out);
        else if (!ok) t_note("StripPrefix [%s] -> hr 0x%lx [%s]", t[i].in, hr, pn_n(w));
    }
}
TEST(path_names_pathcch_stripprefix_invalid) {
    REQUIRE(U(PathCchStripPrefix));
    WCHAR w[MAX_PATH];
    EXPECT_HR(U(PathCchStripPrefix)(NULL, MAX_PATH), E_INVALIDARG);
    wcscpy(w, L"\\\\?\\C:\\a");
    EXPECT_HR(U(PathCchStripPrefix)(w, 0), E_INVALIDARG);
    EXPECT_HR(U(PathCchStripPrefix)(w, 0x8001), E_INVALIDARG);
    wcscpy(w, L"\\\\?\\C:\\a");
    EXPECT_HR(U(PathCchStripPrefix)(w, 3), E_INVALIDARG);
    wcscpy(w, L"\\\\?\\UNC\\x");
    EXPECT_HR(U(PathCchStripPrefix)(w, 7), E_INVALIDARG);
}
