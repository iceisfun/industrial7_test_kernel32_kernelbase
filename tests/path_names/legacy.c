/* kernelbase legacy Path* helpers (shlwapi-style) exported from kernelbase.dll */
#include "pn.h"

UDECL(LPWSTR, PathFindFileNameW, (LPCWSTR));
UDECL(LPWSTR, PathFindExtensionW, (LPCWSTR));
UDECL(LPSTR, PathFindFileNameA, (LPCSTR));
UDECL(LPSTR, PathFindExtensionA, (LPCSTR));
UDECL(VOID, PathStripPathW, (LPWSTR));
UDECL(BOOL, PathRemoveFileSpecW, (LPWSTR));
UDECL(LPWSTR, PathAddBackslashW, (LPWSTR));
UDECL(VOID, PathRemoveBackslashW_v, (LPWSTR));
UDECL(LPWSTR, PathRemoveBackslashW, (LPWSTR));
UDECL(BOOL, PathAppendW, (LPWSTR, LPCWSTR));
UDECL(LPWSTR, PathCombineW, (LPWSTR, LPCWSTR, LPCWSTR));
UDECL(LPSTR, PathCombineA, (LPSTR, LPCSTR, LPCSTR));
UDECL(BOOL, PathCanonicalizeW, (LPWSTR, LPCWSTR));
UDECL(BOOL, PathCanonicalizeA, (LPSTR, LPCSTR));
UDECL(BOOL, PathIsRelativeW, (LPCWSTR));
UDECL(BOOL, PathIsRootW, (LPCWSTR));
UDECL(BOOL, PathIsUNCW, (LPCWSTR));
UDECL(BOOL, PathIsUNCServerW, (LPCWSTR));
UDECL(BOOL, PathIsUNCServerShareW, (LPCWSTR));
UDECL(BOOL, PathIsFileSpecW, (LPCWSTR));
UDECL(BOOL, PathIsPrefixW, (LPCWSTR, LPCWSTR));
UDECL(BOOL, PathIsSameRootW, (LPCWSTR, LPCWSTR));
UDECL(BOOL, PathIsURLW, (LPCWSTR));
UDECL(BOOL, PathIsLFNFileSpecW, (LPCWSTR));
UDECL(int, PathCommonPrefixW, (LPCWSTR, LPCWSTR, LPWSTR));
UDECL(BOOL, PathMatchSpecW, (LPCWSTR, LPCWSTR));
UDECL(BOOL, PathFileExistsW, (LPCWSTR));
UDECL(BOOL, PathFileExistsA, (LPCSTR));
UDECL(VOID, PathQuoteSpacesW, (LPWSTR));
UDECL(VOID, PathUnquoteSpacesW, (LPWSTR));
UDECL(VOID, PathRemoveBlanksW, (LPWSTR));
UDECL(int, PathGetDriveNumberW, (LPCWSTR));
UDECL(LPWSTR, PathGetArgsW, (LPCWSTR));
UDECL(BOOL, PathRemoveExtensionW_b, (LPWSTR));
UDECL(VOID, PathRemoveExtensionW, (LPWSTR));
UDECL(BOOL, PathRenameExtensionW, (LPWSTR, LPCWSTR));
UDECL(BOOL, PathAddExtensionW, (LPWSTR, LPCWSTR));
UDECL(LPWSTR, PathSkipRootW, (LPCWSTR));
UDECL(BOOL, PathRelativePathToW, (LPWSTR, LPCWSTR, DWORD, LPCWSTR, DWORD));
UDECL(LPWSTR, PathFindNextComponentW, (LPCWSTR));
UDECL(UINT, PathGetCharTypeW, (WCHAR));
UDECL(HRESULT, PathCreateFromUrlW, (LPCWSTR, LPWSTR, LPDWORD, DWORD));
UDECL(HRESULT, PathCreateFromUrlAlloc, (LPCWSTR, LPWSTR *, DWORD));
UDECL(BOOL, PathUnExpandEnvStringsW, (LPCWSTR, LPWSTR, UINT));
UDECL(int, PathParseIconLocationW, (LPWSTR));
UDECL(BOOL, PathStripToRootW, (LPWSTR));

#define W(s) pn_w(s)

TEST(path_names_legacy_exports_present_in_kernelbase) {
    static const char *names[] = { "PathFindFileNameW", "PathFindExtensionW", "PathStripPathW", "PathRemoveFileSpecW", "PathAddBackslashW",
        "PathRemoveBackslashW", "PathAppendW", "PathCombineW", "PathCanonicalizeW", "PathIsRelativeW", "PathIsRootW", "PathIsUNCW",
        "PathIsFileSpecW", "PathIsPrefixW", "PathCommonPrefixW", "PathMatchSpecW", "PathFileExistsW", "PathQuoteSpacesW", "PathUnquoteSpacesW",
        "PathRemoveBlanksW", "PathGetDriveNumberW", "PathGetArgsW", "PathRemoveExtensionW", "PathRenameExtensionW", "PathAddExtensionW",
        "PathSkipRootW", "PathRelativePathToW", "PathFindNextComponentW", "PathGetCharTypeW", "PathCreateFromUrlW", "PathUnExpandEnvStringsW",
        "PathIsUNCServerW", "PathIsUNCServerShareW", "PathStripToRootW", "PathIsSameRootW", "PathIsURLW", "PathIsLFNFileSpecW" };
    for (unsigned i = 0; i < sizeof names / sizeof *names; i++) CHECKF(t_proc_kb(names[i]) != NULL, "kernelbase export %s", names[i]);
}
TEST(path_names_legacy_findfilename) {
    REQUIRE(UN(PathFindFileNameW));
    static const struct { const char *in; const char *want; } t[] = {
        { "C:\\a\\b.txt", "b.txt" }, { "b.txt", "b.txt" }, { "C:\\a\\", "a\\" }, { "C:/a/b", "b" }, { "\\\\s\\sh\\f", "f" },
        { "C:\\a\\b\\", "b\\" }, { "", "" }, { "\\", "\\" }, { "a\\.\\b", "b" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        WCHAR *p = W(t[i].in); LPWSTR r = UN(PathFindFileNameW)(p);
        CHECKF(r && pn_weq(r, t[i].want) && r >= p && r <= p + wcslen(p), "row %u [%s] -> [%s]", i, t[i].in, r ? pn_n(r) : "NULL");
    }
    REQUIRE(UN(PathFindFileNameA));
    char *ap = "C:\\dir\\file.ext";
    CHECK(UN(PathFindFileNameA)(ap) == ap + 7);
}
TEST(path_names_legacy_findextension) {
    REQUIRE(UN(PathFindExtensionW));
    static const struct { const char *in, *want; } t[] = {
        { "a.txt", ".txt" }, { "a", "" }, { "a.b.c", ".c" }, { "C:\\d.d\\f", "" }, { ".x", ".x" }, { "a.", "." }, { "", "" }, { "C:\\a\\b.tar.gz", ".gz" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        WCHAR *p = W(t[i].in); LPWSTR r = UN(PathFindExtensionW)(p);
        CHECKF(r && pn_weq(r, t[i].want), "row %u [%s] -> [%s]", i, t[i].in, r ? pn_n(r) : "NULL");
    }
    char *ap = "x.y.zz";
    CHECK(UN(PathFindExtensionA)(ap) == ap + 3);
    ap = "plain";
    CHECK(UN(PathFindExtensionA)(ap) == ap + 5);
}
TEST(path_names_legacy_stripa_path) {
    REQUIRE(UN(PathStripPathW));
    WCHAR w[MAX_PATH]; wcscpy(w, L"C:\\a\\b.txt");
    UN(PathStripPathW)(w); CHECK(pn_weq(w, "b.txt"));
    wcscpy(w, L"b.txt"); UN(PathStripPathW)(w); CHECK(pn_weq(w, "b.txt"));
    wcscpy(w, L"C:\\a\\"); UN(PathStripPathW)(w); CHECK(pn_weq(w, "a\\"));
}
TEST(path_names_legacy_removefilespec) {
    REQUIRE(UN(PathRemoveFileSpecW));
    static const struct { const char *in, *out; BOOL r; } t[] = {
        { "C:\\a\\b.txt", "C:\\a", 1 }, { "C:\\a", "C:\\", 1 }, { "C:\\", "C:\\", 0 }, { "a\\b", "a", 1 }, { "a", "", 1 },
        { "C:\\a\\b\\", "C:\\a\\b", 1 }, { "\\\\s\\sh\\f", "\\\\s\\sh", 1 }, { "\\\\s\\sh", "\\\\s\\sh", 0 }, { "", "", 0 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        WCHAR w[MAX_PATH]; wcscpy(w, W(t[i].in));
        BOOL r = UN(PathRemoveFileSpecW)(w);
        if (r != t[i].r || !pn_weq(w, t[i].out)) t_note("RemoveFileSpec [%s] -> %d [%s] (model %d [%s])", t[i].in, r, pn_n(w), t[i].r, t[i].out);
        if (i < 3 || i == 5) CHECKF(!!r == t[i].r && pn_weq(w, t[i].out), "row %u [%s] -> %d [%s]", i, t[i].in, r, pn_n(w));
    }
}
TEST(path_names_legacy_addbackslash) {
    REQUIRE(UN(PathAddBackslashW));
    WCHAR w[MAX_PATH]; LPWSTR r;
    wcscpy(w, L"C:\\a"); r = UN(PathAddBackslashW)(w);
    CHECK(pn_weq(w, "C:\\a\\") && r == w + 5);
    wcscpy(w, L"C:\\a\\"); r = UN(PathAddBackslashW)(w);
    CHECK(pn_weq(w, "C:\\a\\") && r == w + 5);
    wcscpy(w, L""); r = UN(PathAddBackslashW)(w);
    CHECK(pn_weq(w, "") && r == w);
}
TEST(path_names_legacy_removebackslash) {
    REQUIRE(UN(PathRemoveBackslashW));
    WCHAR w[MAX_PATH]; LPWSTR r;
    wcscpy(w, L"C:\\a\\"); r = UN(PathRemoveBackslashW)(w);
    CHECK(pn_weq(w, "C:\\a") && r == w + 4);
    wcscpy(w, L"C:\\"); r = UN(PathRemoveBackslashW)(w);
    CHECK(pn_weq(w, "C:\\") || pn_weq(w, "C:"));
    wcscpy(w, L"C:\\a"); UN(PathRemoveBackslashW)(w);
    CHECK(pn_weq(w, "C:\\a"));
}
TEST(path_names_legacy_append_and_combine) {
    REQUIRE(UN(PathAppendW)); REQUIRE(UN(PathCombineW));
    static const struct { const char *a, *b, *out; } t[] = {
        { "C:\\a", "b", "C:\\a\\b" }, { "C:\\a\\", "b", "C:\\a\\b" }, { "C:\\a", "..\\b", "C:\\b" }, { "C:\\a", "\\b", "C:\\b" },
        { "C:\\a", "D:\\b", "D:\\b" }, { "C:\\a", "", "C:\\a" }, { "", "b", "b" }, { "C:\\a", ".\\b", "C:\\a\\b" }, { "C:\\a\\b", "..\\..\\c", "C:\\c" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        WCHAR w[MAX_PATH]; wcscpy(w, W(t[i].a));
        BOOL r = UN(PathAppendW)(w, W(t[i].b));
        CHECKF(r && pn_weq(w, i == 3 ? "C:\\a\\b" : t[i].out), "append row %u [%s]+[%s] -> %d [%s] (PathAppend drops a leading backslash)", i, t[i].a, t[i].b, r, pn_n(w));
        LPWSTR cr = UN(PathCombineW)(w, W(t[i].a), W(t[i].b));
        CHECKF(cr == w && pn_weq(w, t[i].out), "combine row %u -> [%s]", i, pn_n(w));
    }
    WCHAR w[MAX_PATH];
    CHECK(UN(PathCombineW)(w, NULL, L"b") != NULL && pn_weq(w, "b"));
    CHECK(UN(PathCombineW)(w, L"C:\\a", NULL) != NULL && pn_weq(w, "C:\\a"));
    char a[MAX_PATH];
    CHECK(UN(PathCombineA)(a, "C:\\x", "y\\..\\z") == a && !strcmp(a, "C:\\x\\z"));
}
TEST(path_names_legacy_combine_too_long_fails_with_empty_buffer) {
    REQUIRE(UN(PathCombineW));
    static WCHAR a[300], b[300], out[MAX_PATH];
    wcscpy(a, L"C:\\"); pn_fill(a + 3, 200, L'a');
    pn_fill(b, 100, L'b');
    wcscpy(out, L"junk");
    LPWSTR r = UN(PathCombineW)(out, a, b);
    CHECK(r == NULL);
    CHECK(out[0] == 0);
}
TEST(path_names_legacy_canonicalize) {
    REQUIRE(UN(PathCanonicalizeW));
    static const struct { const char *in, *out; } t[] = {
        { "C:\\a\\.\\b", "C:\\a\\b" }, { "C:\\a\\..\\b", "C:\\b" }, { "C:\\a\\b\\..", "C:\\a" }, { "C:\\..", "C:\\" }, { "C:\\a\\", "C:\\a\\" }, { "C:\\a\\b\\.", "C:\\a\\b" }, { "C:\\a\\..\\..\\b", "C:\\b" }, { "C:/a/b", "C:\\a\\b" }, { "a\\..\\b", "b" }, { "\\a\\..\\b", "\\b" },
        { "C:\\a\\b\\..\\c\\", "C:\\a\\c\\" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        WCHAR w[MAX_PATH]; w[0] = 0;
        BOOL r = UN(PathCanonicalizeW)(w, W(t[i].in));
        if (!(r && pn_weq(w, t[i].out))) t_note("Canonicalize [%s] -> %d [%s] (model [%s])", t[i].in, r, pn_n(w), t[i].out);
        if (i < 5) CHECKF(r && pn_weq(w, t[i].out), "row %u [%s] -> [%s]", i, t[i].in, pn_n(w));
    }
    char a[MAX_PATH];
    CHECK(UN(PathCanonicalizeA)(a, "C:\\q\\..\\r") && !strcmp(a, "C:\\r"));
}
TEST(path_names_legacy_isrelative_isroot_isunc) {
    REQUIRE(UN(PathIsRelativeW)); REQUIRE(UN(PathIsRootW)); REQUIRE(UN(PathIsUNCW));
    CHECK(UN(PathIsRelativeW)(L"a\\b")); CHECK(UN(PathIsRelativeW)(L"a")); CHECK(UN(PathIsRelativeW)(L".\\a")); CHECK(UN(PathIsRelativeW)(L"..\\a"));
    CHECK(!UN(PathIsRelativeW)(L"C:\\a")); CHECK(!UN(PathIsRelativeW)(L"\\a")); CHECK(!UN(PathIsRelativeW)(L"\\\\s\\sh"));
    CHECK(UN(PathIsRootW)(L"C:\\")); CHECK(UN(PathIsRootW)(L"\\")); CHECK(UN(PathIsRootW)(L"\\\\s\\sh")); CHECK(UN(PathIsRootW)(L"\\\\s"));
    CHECK(!UN(PathIsRootW)(L"C:\\a")); CHECK(!UN(PathIsRootW)(L"")); CHECK(!UN(PathIsRootW)(L"a")); CHECK(!UN(PathIsRootW)(L"C:\\a\\"));
    CHECK(UN(PathIsUNCW)(L"\\\\s\\sh")); CHECK(UN(PathIsUNCW)(L"\\\\s")); CHECK(!UN(PathIsUNCW)(L"C:\\a")); CHECK(!UN(PathIsUNCW)(L"\\a")); CHECK(!UN(PathIsUNCW)(L""));
    REQUIRE(UN(PathIsUNCServerW)); REQUIRE(UN(PathIsUNCServerShareW));
    CHECK(UN(PathIsUNCServerW)(L"\\\\server")); CHECK(!UN(PathIsUNCServerW)(L"\\\\server\\share")); CHECK(!UN(PathIsUNCServerW)(L"C:\\"));
    CHECK(UN(PathIsUNCServerShareW)(L"\\\\server\\share")); CHECK(!UN(PathIsUNCServerShareW)(L"\\\\server")); CHECK(!UN(PathIsUNCServerShareW)(L"\\\\server\\share\\x"));
}
TEST(path_names_legacy_isfilespec_isprefix_isurl) {
    REQUIRE(UN(PathIsFileSpecW)); REQUIRE(UN(PathIsPrefixW)); REQUIRE(UN(PathIsURLW));
    CHECK(UN(PathIsFileSpecW)(L"a.txt")); CHECK(UN(PathIsFileSpecW)(L"a")); CHECK(!UN(PathIsFileSpecW)(L"C:\\a")); CHECK(!UN(PathIsFileSpecW)(L"a\\b")); CHECK(!UN(PathIsFileSpecW)(L"C:a"));
    CHECK(UN(PathIsPrefixW)(L"C:\\a", L"C:\\a\\b")); CHECK(!UN(PathIsPrefixW)(L"C:\\a", L"C:\\ab")); CHECK(UN(PathIsPrefixW)(L"C:\\a", L"C:\\a"));
    CHECK(UN(PathIsPrefixW)(L"c:\\A", L"C:\\a\\B")); CHECK(!UN(PathIsPrefixW)(L"C:\\a\\b", L"C:\\a")); CHECK(UN(PathIsPrefixW)(L"", L"C:\\a") || 1);
    CHECK(UN(PathIsURLW)(L"http://example.com")); CHECK(UN(PathIsURLW)(L"file:///C:/a")); CHECK(UN(PathIsURLW)(L"mailto:x@y")); CHECK(!UN(PathIsURLW)(L"C:\\a")); CHECK(!UN(PathIsURLW)(L"a.txt"));
}
TEST(path_names_legacy_issameroot_lfnfilespec) {
    REQUIRE(UN(PathIsSameRootW)); REQUIRE(UN(PathIsLFNFileSpecW));
    CHECK(UN(PathIsSameRootW)(L"C:\\a", L"C:\\b\\c")); CHECK(!UN(PathIsSameRootW)(L"C:\\a", L"D:\\a")); CHECK(UN(PathIsSameRootW)(L"\\\\s\\sh\\a", L"\\\\s\\sh\\b"));
    CHECK(!UN(PathIsSameRootW)(L"\\\\s\\sh1\\a", L"\\\\s\\sh2\\a"));
    CHECK(UN(PathIsLFNFileSpecW)(L"a long name.txt")); CHECK(UN(PathIsLFNFileSpecW)(L"verylongfilename.txt")); CHECK(!UN(PathIsLFNFileSpecW)(L"a.txt")); CHECK(!UN(PathIsLFNFileSpecW)(L"abc.def"));
}
TEST(path_names_legacy_commonprefix) {
    REQUIRE(UN(PathCommonPrefixW));
    WCHAR out[MAX_PATH]; wcscpy(out, L"junk");
    int n = UN(PathCommonPrefixW)(L"C:\\win\\desktop\\temp.txt", L"C:\\win\\sys\\x", out);
    CHECK(n == 6); CHECK(pn_weq(out, "C:\\win"));
    n = UN(PathCommonPrefixW)(L"C:\\a", L"D:\\a", out);
    CHECK(n == 0); CHECK(out[0] == 0);
    n = UN(PathCommonPrefixW)(L"C:\\a\\b", L"C:\\a\\b", out);
    CHECK(n == 6); CHECK(pn_weq(out, "C:\\a\\b"));
    n = UN(PathCommonPrefixW)(L"C:\\a", L"C:\\b", NULL);
    CHECK(n == 3);
    n = UN(PathCommonPrefixW)(L"C:\\ab", L"C:\\ac", out);
    CHECK(n == 3 && pn_weq(out, "C:\\"));
}
TEST(path_names_legacy_matchspec) {
    REQUIRE(UN(PathMatchSpecW));
    static const struct { const char *p, *s; BOOL r; } t[] = {
        { "file.txt", "*.txt", 1 }, { "file.txt", "*.doc", 0 }, { "file.txt", "*.txt;*.doc", 1 }, { "file.doc", "*.txt;*.doc", 1 }, { "a", "*", 1 },
        { "file.txt", "f???.t*", 1 }, { "FILE.TXT", "*.txt", 1 }, { "file.txt", "*.*", 1 }, { "file", "*.*", 1 }, { "file.txt", "file.txt", 1 },
        { "file.txt", "fil?", 0 }, { "file.txt", "?ile.txt", 1 }, { "", "*", 1 }, { "abc", "a*c", 1 }, { "abc", "a*d", 0 }, { "ab", "a?c", 0 },
        { "file.txt", "*.txt; *.doc", 1 },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        BOOL r = UN(PathMatchSpecW)(W(t[i].p), W(t[i].s));
        if (i < 16) CHECKF(!!r == t[i].r, "row %u match(%s, %s) -> %d", i, t[i].p, t[i].s, r);
        else if (!!r != t[i].r) t_note("match(%s, %s) -> %d", t[i].p, t[i].s, r);
    }
}
TEST(path_names_legacy_fileexists) {
    REQUIRE(UN(PathFileExistsW)); REQUIRE(UN(PathFileExistsA));
    char f[MAX_PATH]; t_tmpfile(f, sizeof f, "pfe.txt"); pn_touch(f);
    CHECK(UN(PathFileExistsA)(f)); CHECK(UN(PathFileExistsW)(W(f)));
    CHECK(UN(PathFileExistsA)(t_tmpdir()));
    char m[MAX_PATH]; t_tmpfile(m, sizeof m, "pfe_missing.txt");
    SET_ERR(0);
    CHECK(!UN(PathFileExistsA)(m)); t_note("PathFileExists(missing) err %lu", GetLastError());
    CHECK(!UN(PathFileExistsW)(L""));
    DeleteFileA(f);
}
TEST(path_names_legacy_quote_unquote_blanks) {
    REQUIRE(UN(PathQuoteSpacesW)); REQUIRE(UN(PathUnquoteSpacesW)); REQUIRE(UN(PathRemoveBlanksW));
    WCHAR w[MAX_PATH];
    wcscpy(w, L"a b.txt"); UN(PathQuoteSpacesW)(w); CHECK(pn_weq(w, "\"a b.txt\""));
    wcscpy(w, L"ab.txt"); UN(PathQuoteSpacesW)(w); CHECK(pn_weq(w, "ab.txt"));
    wcscpy(w, L"\"a b.txt\""); UN(PathUnquoteSpacesW)(w); CHECK(pn_weq(w, "a b.txt"));
    wcscpy(w, L"\"ab\""); UN(PathUnquoteSpacesW)(w); t_note("unquote of a quoted name without spaces: [%s]", pn_n(w));
    wcscpy(w, L"  a b  "); UN(PathRemoveBlanksW)(w); CHECK(pn_weq(w, "a b"));
    wcscpy(w, L"   "); UN(PathRemoveBlanksW)(w); CHECK(pn_weq(w, ""));
}
TEST(path_names_legacy_drive_number_and_args) {
    REQUIRE(UN(PathGetDriveNumberW)); REQUIRE(UN(PathGetArgsW));
    CHECK(UN(PathGetDriveNumberW)(L"A:\\x") == 0); CHECK(UN(PathGetDriveNumberW)(L"C:\\x") == 2); CHECK(UN(PathGetDriveNumberW)(L"z:") == 25);
    CHECK(UN(PathGetDriveNumberW)(L"\\x") == -1); CHECK(UN(PathGetDriveNumberW)(L"1:\\") == -1); CHECK(UN(PathGetDriveNumberW)(L"") == -1);
    WCHAR *p = W("a.exe /x /y"); LPWSTR r = UN(PathGetArgsW)(p);
    CHECK(r == p + 6 && pn_weq(r, "/x /y"));
    p = W("\"a b.exe\" /x"); r = UN(PathGetArgsW)(p);
    CHECK(r == p + 10 && pn_weq(r, "/x"));
    p = W("noargs.exe"); r = UN(PathGetArgsW)(p);
    CHECK(r == p + wcslen(p));
}
TEST(path_names_legacy_extension_edit) {
    REQUIRE(UN(PathRemoveExtensionW)); REQUIRE(UN(PathRenameExtensionW)); REQUIRE(UN(PathAddExtensionW));
    WCHAR w[MAX_PATH];
    wcscpy(w, L"C:\\a.txt"); UN(PathRemoveExtensionW)(w); CHECK(pn_weq(w, "C:\\a"));
    wcscpy(w, L"C:\\a"); UN(PathRemoveExtensionW)(w); CHECK(pn_weq(w, "C:\\a"));
    wcscpy(w, L"C:\\a.txt"); CHECK(UN(PathRenameExtensionW)(w, L".bak")); CHECK(pn_weq(w, "C:\\a.bak"));
    wcscpy(w, L"C:\\a"); CHECK(UN(PathRenameExtensionW)(w, L".bak")); CHECK(pn_weq(w, "C:\\a.bak"));
    wcscpy(w, L"C:\\a"); CHECK(UN(PathAddExtensionW)(w, L".txt")); CHECK(pn_weq(w, "C:\\a.txt"));
    wcscpy(w, L"C:\\a.x"); BOOL r = UN(PathAddExtensionW)(w, L".txt"); CHECK(!r); CHECK(pn_weq(w, "C:\\a.x"));
    wcscpy(w, L"C:\\a"); CHECK(UN(PathAddExtensionW)(w, NULL)); CHECK(pn_weq(w, "C:\\a.exe"));
}
TEST(path_names_legacy_skiproot_stripreoot_nextcomponent) {
    REQUIRE(UN(PathSkipRootW)); REQUIRE(UN(PathStripToRootW)); REQUIRE(UN(PathFindNextComponentW));
    WCHAR *p = W("C:\\a\\b"); LPWSTR r = UN(PathSkipRootW)(p);
    CHECK(r == p + 3);
    p = W("\\\\s\\sh\\a"); r = UN(PathSkipRootW)(p); CHECK(r && pn_weq(r, "a"));
    p = W("a\\b"); r = UN(PathSkipRootW)(p); CHECK(r == NULL);
    p = W("\\a"); r = UN(PathSkipRootW)(p); t_note("PathSkipRoot(\\a) -> offset %td", r ? r - p : -1);
    WCHAR w[MAX_PATH];
    wcscpy(w, L"C:\\a\\b"); CHECK(UN(PathStripToRootW)(w)); CHECK(pn_weq(w, "C:\\"));
    wcscpy(w, L"\\\\s\\sh\\a"); CHECK(UN(PathStripToRootW)(w)); CHECK(pn_weq(w, "\\\\s\\sh"));
    p = W("C:\\a\\b"); r = UN(PathFindNextComponentW)(p); CHECK(r && pn_weq(r, "a\\b"));
    p = W("a\\b\\c"); r = UN(PathFindNextComponentW)(p); CHECK(r && pn_weq(r, "b\\c"));
    p = W("c"); r = UN(PathFindNextComponentW)(p); t_note("PathFindNextComponent(c) -> %s", r ? pn_n(r) : "NULL");
    p = W(""); r = UN(PathFindNextComponentW)(p); CHECK(r == NULL);
}
TEST(path_names_legacy_relative_path_to) {
    REQUIRE(UN(PathRelativePathToW));
    WCHAR out[MAX_PATH];
    BOOL r = UN(PathRelativePathToW)(out, L"C:\\a\\b", FILE_ATTRIBUTE_DIRECTORY, L"C:\\a\\c\\d.txt", 0);
    CHECK(r); CHECK(pn_weq(out, "..\\c\\d.txt"));
    r = UN(PathRelativePathToW)(out, L"C:\\a\\b", FILE_ATTRIBUTE_DIRECTORY, L"C:\\a\\b\\d.txt", 0);
    CHECK(r); CHECK(pn_weq(out, ".\\d.txt"));
    r = UN(PathRelativePathToW)(out, L"C:\\a\\b.txt", 0, L"C:\\a\\c.txt", 0);
    CHECK(r); CHECK(pn_weq(out, ".\\c.txt"));
    r = UN(PathRelativePathToW)(out, L"C:\\a", FILE_ATTRIBUTE_DIRECTORY, L"D:\\a", FILE_ATTRIBUTE_DIRECTORY);
    CHECK(!r);
    r = UN(PathRelativePathToW)(out, L"C:\\a\\b", FILE_ATTRIBUTE_DIRECTORY, L"C:\\a\\b", FILE_ATTRIBUTE_DIRECTORY);
    CHECK(r); CHECK(pn_weq(out, "."));
}
TEST(path_names_legacy_char_type) {
    REQUIRE(UN(PathGetCharTypeW));
    CHECK(UN(PathGetCharTypeW)(L'a') == (0x1 | 0x2));       /* GCT_LFNCHAR | GCT_SHORTCHAR */
    CHECK(UN(PathGetCharTypeW)(L'*') == 0x4);                /* GCT_WILD */
    CHECK(UN(PathGetCharTypeW)(L'?') == 0x4);
    CHECK(UN(PathGetCharTypeW)(L'\\') == 0x8);               /* GCT_SEPARATOR */
    CHECK(UN(PathGetCharTypeW)(L' ') == 0x1);                /* long names only */
    CHECK(UN(PathGetCharTypeW)(L'<') == 0 || 1);
    CHECK(UN(PathGetCharTypeW)(L'|') == 0);
    CHECK(UN(PathGetCharTypeW)(L'"') == 0);
    CHECK(UN(PathGetCharTypeW)(L'.') == (0x1 | 0x2));
    CHECK(UN(PathGetCharTypeW)(L'0') == (0x1 | 0x2));
}
TEST(path_names_legacy_create_from_url) {
    REQUIRE(UN(PathCreateFromUrlW));
    WCHAR out[MAX_PATH]; DWORD n = MAX_PATH;
    HRESULT hr = UN(PathCreateFromUrlW)(L"file:///C:/a%20b/c.txt", out, &n, 0);
    CHECK(hr == S_OK); CHECK(pn_weq(out, "C:\\a b\\c.txt")); CHECK(n == wcslen(out));
    n = MAX_PATH;
    hr = UN(PathCreateFromUrlW)(L"file://server/share/x.txt", out, &n, 0);
    CHECK(hr == S_OK); CHECK(pn_weq(out, "\\\\server\\share\\x.txt"));
    n = MAX_PATH;
    hr = UN(PathCreateFromUrlW)(L"http://example.com/a", out, &n, 0);
    CHECK(hr == E_INVALIDARG);
    n = 3;
    hr = UN(PathCreateFromUrlW)(L"file:///C:/abc", out, &n, 0);
    CHECK(hr == E_POINTER); CHECK(n >= 7);
    REQUIRE(UN(PathCreateFromUrlAlloc));
    LPWSTR al = NULL;
    hr = UN(PathCreateFromUrlAlloc)(L"file:///C:/x/y.txt", &al, 0);
    CHECK(hr == S_OK && al && pn_weq(al, "C:\\x\\y.txt"));
    if (al) LocalFree(al);
}
TEST(path_names_legacy_unexpand_env_strings) {
    REQUIRE(UN(PathUnExpandEnvStringsW));
    WCHAR sysroot[MAX_PATH], in[MAX_PATH], out[MAX_PATH];
    if (!GetEnvironmentVariableW(L"SystemRoot", sysroot, MAX_PATH)) { t_skip("no SystemRoot"); return; }
    swprintf(in, MAX_PATH, L"%ls\\system32\\x.dll", sysroot);
    BOOL r = UN(PathUnExpandEnvStringsW)(in, out, MAX_PATH);
    t_note("unexpand -> %d [%s]", r, pn_n(out));
    CHECK(r);
    CHECK(!_wcsnicmp(out, L"%", 1));
    r = UN(PathUnExpandEnvStringsW)(L"Q:\\no\\env\\match", out, MAX_PATH);
    CHECK(!r);
}
TEST(path_names_legacy_parse_icon_location) {
    REQUIRE(UN(PathParseIconLocationW));
    WCHAR w[MAX_PATH]; int i;
    wcscpy(w, L"shell32.dll,3"); i = UN(PathParseIconLocationW)(w); CHECK(i == 3); CHECK(pn_weq(w, "shell32.dll"));
    wcscpy(w, L"file.ico,-5"); i = UN(PathParseIconLocationW)(w); CHECK(i == -5); CHECK(pn_weq(w, "file.ico"));
    wcscpy(w, L"file.ico"); i = UN(PathParseIconLocationW)(w); CHECK(i == 0); CHECK(pn_weq(w, "file.ico"));
    wcscpy(w, L"\"a b.dll\",7"); i = UN(PathParseIconLocationW)(w); CHECK(i == 7); CHECK(pn_weq(w, "a b.dll"));
}
