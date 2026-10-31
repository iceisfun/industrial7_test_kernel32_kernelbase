/* PathAddBackslashA, PathAddExtensionA, PathAppendA, PathCommonPrefixA, PathFindNextComponentA, PathGetArgsA */
#include "g8.h"

TEST(gap8_exports_in_kernelbase) {
    static const char *names[] = {
        "PathAddBackslashA","PathAddExtensionA","PathAppendA","PathCommonPrefixA","PathCreateFromUrlA",
        "PathFindNextComponentA","PathGetArgsA","PathGetCharTypeA","PathGetDriveNumberA","PathIsFileSpecA",
        "PathIsLFNFileSpecA","PathIsPrefixA","PathIsRelativeA","PathIsRootA","PathIsSameRootA","PathIsUNCA",
        "PathIsUNCServerA","PathIsUNCServerShareA","PathIsURLA","PathMatchSpecA","PathMatchSpecExA",
        "PathMatchSpecExW","PathParseIconLocationA","PathQuoteSpacesA","PathRelativePathToA",
        "PathRemoveBackslashA","PathRemoveBlanksA","PathRemoveExtensionA","PathRemoveFileSpecA",
        "PathRenameExtensionA","PathSearchAndQualifyA","PathSearchAndQualifyW","PathSkipRootA",
        "PathStripPathA","PathStripToRootA" };
    if (t_is_wine()) { t_skip("exports live in shlwapi under wine"); return; }
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) {
        CHECKF(t_proc_kb(names[i]) != NULL, "kernelbase export %s", names[i]);
        /* none of these are kernel32 exports on Win10 */
        t_note("kernel32 export %s: %s", names[i], t_proc_k32(names[i]) ? "present" : "absent");
    }
}

/* ---------------- PathAddBackslashA ---------------- */
typedef struct { const char *in, *out; long off; } ab_case;
static const ab_case ab_tab[] = {
    { "", "", 0 }, { "C:", "C:\\", 3 }, { "C:\\", "C:\\", 3 }, { "C:\\a", "C:\\a\\", 5 },
    { "C:\\a\\", "C:\\a\\", 5 }, { "a", "a\\", 2 }, { "/", "/\\", 2 }, { "a b", "a b\\", 4 },
    { "a\\\\", "a\\\\", 3 }, { ".", ".\\", 2 }, { "\\", "\\", 1 }, { "\\\\srv\\share", "\\\\srv\\share\\", 12 },
    { "a/b", "a/b\\", 4 }, { "C:\\dir.ext", "C:\\dir.ext\\", 11 },
};
TEST(gap8_addbackslash_table) {
    REQUIRE(P(PathAddBackslashA));
    static char b[G8BUF];
    for (size_t i = 0; i < sizeof ab_tab / sizeof *ab_tab; i++) {
        const ab_case *c = &ab_tab[i];
        char *r = P(PathAddBackslashA)(g8_set(b, c->in));
        CHECKF(!strcmp(b, c->out), "in=\"%s\" got \"%s\" want \"%s\"", c->in, b, c->out);
        CHECKF(OFF(r, b) == c->off, "in=\"%s\" ret off %ld want %ld", c->in, OFF(r, b), c->off);
    }
}
TEST(gap8_addbackslash_null) {
    REQUIRE(P(PathAddBackslashA));
    CHECK(P(PathAddBackslashA)(NULL) == NULL);
}
TEST(gap8_addbackslash_returns_end) {
    REQUIRE(P(PathAddBackslashA));
    static char b[G8BUF];
    /* The result always points at the terminating NUL, also when nothing was appended. */
    char *r = P(PathAddBackslashA)(g8_set(b, "x\\y"));
    CHECK(r && *r == '\0' && r == b + strlen(b));
    r = P(PathAddBackslashA)(b);
    CHECK(r && *r == '\0' && r == b + strlen(b));
    CHECK(!strcmp(b, "x\\y\\"));
}
TEST(gap8_addbackslash_idempotent) {
    REQUIRE(P(PathAddBackslashA));
    static char b[G8BUF];
    g8_set(b, "C:\\foo");
    for (int i = 0; i < 20; i++) P(PathAddBackslashA)(b);
    CHECK(!strcmp(b, "C:\\foo\\"));
}
TEST(gap8_addbackslash_length_limit) {
    REQUIRE(P(PathAddBackslashA));
    static char b[G8BUF];
    /* buffer is assumed MAX_PATH: 258 chars + backslash = 259 fits, 259 chars does not */
    char *r = P(PathAddBackslashA)(g8_fill(b, 258, 'a'));
    CHECK(r == b + 259);
    CHECK(strlen(b) == 259 && b[258] == '\\');
    r = P(PathAddBackslashA)(g8_fill(b, 259, 'a'));
    CHECK(r == NULL);
    CHECK(strlen(b) == 259 && b[258] == 'a');
    r = P(PathAddBackslashA)(g8_fill(b, 300, 'a'));
    CHECK(r == NULL);
    CHECK(strlen(b) == 300);
    /* a string already ending in a backslash at the limit still fails: length check comes first */
    g8_fill(b, 259, 'a'); b[258] = '\\';
    r = P(PathAddBackslashA)(b);
    CHECK(r == NULL || r == b + 259);
    t_note("259-char path ending in backslash -> %s", r ? "returned end" : "NULL");
}

/* ---------------- PathAddExtensionA ---------------- */
typedef struct { const char *path, *ext; int ret; const char *out; } ae_case;
static const ae_case ae_tab[] = {
    { "a", ".txt", 1, "a.txt" }, { "a", NULL, 1, "a.exe" }, { "a.b", ".c", 0, "a.b" },
    { "a.", ".c", 0, "a." }, { "a", "txt", 1, "atxt" }, { "dir.x\\f", ".c", 1, "dir.x\\f.c" },
    { "a b.c d", ".t", 1, "a b.c d.t" }, { "", ".x", 1, ".x" }, { "", NULL, 1, ".exe" },
    { "a", "", 1, "a" }, { "C:\\a\\b", ".exe", 1, "C:\\a\\b.exe" }, { "a.b", NULL, 0, "a.b" },
    { ".hidden", ".x", 0, ".hidden" }, { "a.b c", ".x", 1, "a.b c.x" }, { "a.tar.gz", ".x", 0, "a.tar.gz" },
    { "C:\\x.y\\z", NULL, 1, "C:\\x.y\\z.exe" }, { "a", ".EXE", 1, "a.EXE" }, { "a", ".a.b", 1, "a.a.b" },
};
TEST(gap8_addextension_table) {
    REQUIRE(P(PathAddExtensionA));
    static char b[G8BUF];
    for (size_t i = 0; i < sizeof ae_tab / sizeof *ae_tab; i++) {
        const ae_case *c = &ae_tab[i];
        BOOL r = P(PathAddExtensionA)(g8_set(b, c->path), c->ext);
        CHECKF(!!r == c->ret, "path=\"%s\" ext=%s ret %d want %d", c->path, c->ext ? c->ext : "(null)", r, c->ret);
        CHECKF(!strcmp(b, c->out), "path=\"%s\" got \"%s\" want \"%s\"", c->path, b, c->out);
    }
}
TEST(gap8_addextension_null) {
    REQUIRE(P(PathAddExtensionA));
    CHECK(P(PathAddExtensionA)(NULL, ".x") == FALSE);
    CHECK(P(PathAddExtensionA)(NULL, NULL) == FALSE);
}
TEST(gap8_addextension_length_boundary) {
    REQUIRE(P(PathAddExtensionA));
    static char b[G8BUF];
    /* needs len(path)+len(ext) < 260 */
    BOOL r = P(PathAddExtensionA)(g8_fill(b, 255, 'a'), ".txt");
    CHECK(r == TRUE && strlen(b) == 259 && !strcmp(b + 255, ".txt"));
    r = P(PathAddExtensionA)(g8_fill(b, 256, 'a'), ".txt");
    CHECK(r == FALSE && strlen(b) == 256);
    r = P(PathAddExtensionA)(g8_fill(b, 256, 'a'), ".tx");
    CHECK(r == TRUE && strlen(b) == 259);
    r = P(PathAddExtensionA)(g8_fill(b, 257, 'a'), ".tx");
    CHECK(r == FALSE && strlen(b) == 257);
    r = P(PathAddExtensionA)(g8_fill(b, 256, 'a'), NULL);   /* default ".exe" is 4 */
    CHECK(r == FALSE && strlen(b) == 256);
    r = P(PathAddExtensionA)(g8_fill(b, 259, 'a'), "");
    CHECK(r == TRUE && strlen(b) == 259);
}
TEST(gap8_addextension_repeat) {
    REQUIRE(P(PathAddExtensionA));
    static char b[G8BUF];
    g8_set(b, "name");
    CHECK(P(PathAddExtensionA)(b, ".dat") == TRUE);
    CHECK(P(PathAddExtensionA)(b, ".dat") == FALSE);
    CHECK(P(PathAddExtensionA)(b, NULL) == FALSE);
    CHECK(!strcmp(b, "name.dat"));
}

/* ---------------- PathAppendA ---------------- */
typedef struct { const char *path, *more; int ret; const char *out; } ap_case;
static const ap_case ap_tab[] = {
    { "C:\\a", "b", 1, "C:\\a\\b" }, { "C:\\a\\", "b", 1, "C:\\a\\b" }, { "C:\\a", "\\b", 1, "C:\\a\\b" },
    { "C:\\a", "\\\\\\\\b", -1, NULL },
    { "C:\\a", "..\\b", 1, "C:\\b" }, { "C:\\a\\b", "..", 1, "C:\\a" }, { "C:\\a", ".", 1, "C:\\a" },
    { "C:\\a", "C:\\x", 1, "C:\\x" }, { "C:\\a", "b\\c\\", 1, "C:\\a\\b\\c\\" }, { "a", "b", 1, "a\\b" },
    { "C:\\", "b", 1, "C:\\b" }, { "C:\\a", "b.txt", 1, "C:\\a\\b.txt" }, { "C:\\a", "b c", 1, "C:\\a\\b c" },
    { "C:\\a", ".\\b", 1, "C:\\a\\b" }, { "C:\\a\\b\\c", "..\\..\\d", 1, "C:\\a\\d" },
    { "\\\\srv\\share", "f", 1, "\\\\srv\\share\\f" }, { "C:\\a", "..\\..\\..\\b", 1, "C:\\b" },
};
TEST(gap8_append_table) {
    REQUIRE(P(PathAppendA));
    static char b[G8BUF];
    for (size_t i = 0; i < sizeof ap_tab / sizeof *ap_tab; i++) {
        const ap_case *c = &ap_tab[i];
        BOOL r = P(PathAppendA)(g8_set(b, c->path), c->more);
        if (c->ret < 0) { t_note("Append(\"%s\",\"%s\") -> %d \"%s\"", c->path, c->more, r, b); continue; }
        CHECKF(!!r == c->ret, "path=\"%s\" more=\"%s\" ret %d", c->path, c->more, r);
        CHECKF(!strcmp(b, c->out), "path=\"%s\" more=\"%s\" got \"%s\" want \"%s\"", c->path, c->more, b, c->out);
    }
}
TEST(gap8_append_edge_notes) {
    REQUIRE(P(PathAppendA));
    static char b[G8BUF];
    static const char *pairs[][2] = { {"", "b"}, {"C:", "b"}, {"C:\\a", ""}, {"", ""}, {"C:\\a\\.\\b", "c"}, {"C:\\a", "C:b"}, {"C:\\a", "D:\\x"} };
    for (size_t i = 0; i < sizeof pairs / sizeof *pairs; i++) {
        BOOL r = P(PathAppendA)(g8_set(b, pairs[i][0]), pairs[i][1]);
        t_note("Append(\"%s\",\"%s\") -> %d \"%s\"", pairs[i][0], pairs[i][1], r, b);
    }
}
TEST(gap8_append_null) {
    REQUIRE(P(PathAppendA));
    static char b[G8BUF];
    CHECK(P(PathAppendA)(NULL, "x") == FALSE);
    CHECK(P(PathAppendA)(NULL, NULL) == FALSE);
    g8_set(b, "C:\\keep");
    CHECK(P(PathAppendA)(b, NULL) == FALSE);
    CHECK(!strcmp(b, "C:\\keep"));
}
TEST(gap8_append_length_boundary) {
    REQUIRE(P(PathAppendA));
    static char b[G8BUF], more[64];
    /* "C:\" + 252 'a' = 255 chars; + "\" + "bbb" = 259 fits */
    g8_set(b, "C:\\"); memset(b + 3, 'a', 252);
    strcpy(more, "bbb");
    BOOL r = P(PathAppendA)(b, more);
    CHECK(r == TRUE && strlen(b) == 259);
    g8_set(b, "C:\\"); memset(b + 3, 'a', 252);
    strcpy(more, "bbbb");
    r = P(PathAppendA)(b, more);
    CHECKF(r == FALSE, "260-char result must fail (ret %d, len %d)", r, (int)strlen(b));
}
TEST(gap8_append_chain) {
    REQUIRE(P(PathAppendA));
    static char b[G8BUF];
    g8_set(b, "C:\\base");
    CHECK(P(PathAppendA)(b, "one"));
    CHECK(P(PathAppendA)(b, "two"));
    CHECK(P(PathAppendA)(b, "three.txt"));
    CHECK(!strcmp(b, "C:\\base\\one\\two\\three.txt"));
    CHECK(P(PathAppendA)(b, "..\\..\\x"));
    CHECK(!strcmp(b, "C:\\base\\one\\x"));
}

/* ---------------- PathCommonPrefixA ---------------- */
typedef struct { const char *f1, *f2; int ret; const char *out; } cp_case;
static const cp_case cp_tab[] = {
    { "C:\\win\\desktop\\temp.txt", "C:\\win\\system32\\autoexec.bat", 6, "C:\\win" },
    { "C:\\", "C:\\", 3, "C:\\" }, { "C:\\a", "C:\\b", 3, "C:\\" }, { "C:\\a", "C:\\a", 4, "C:\\a" },
    { "C:\\a\\b", "C:\\a\\c", 4, "C:\\a" }, { "C:\\A\\b", "c:\\a\\C", 4, "C:\\A" }, { "C:\\a", "D:\\a", 0, "" },
    { "a", "b", 0, "" }, { "\\\\srv\\sh\\a", "\\\\srv\\sh\\b", 8, "\\\\srv\\sh" },
    { "\\\\srv\\a", "C:\\a", 0, "" }, { "C:\\a", "\\\\srv\\a", 0, "" }, { "C:\\a\\b", "C:\\a\\bc", 4, "C:\\a" },
    { "C:\\abc", "C:\\abd", 3, "C:\\" }, { "C:\\a\\b", "C:\\a", 4, "C:\\a" }, { "C:\\a", "C:\\a\\b", 4, "C:\\a" },
    { "C:\\a\\b\\c", "C:\\a\\b\\d", 6, "C:\\a\\b" }, { "C:\\a\\b\\c", "C:\\a\\b\\c", 8, "C:\\a\\b\\c" },
    { "C:\\x\\y", "C:\\a\\y", 3, "C:\\" }, { "C:\\Win", "c:\\WIN", 6, "C:\\Win" }, { "", "", 0, "" },
    { "a\\b", "a\\c", 1, "a" },
};
TEST(gap8_commonprefix_table) {
    REQUIRE(P(PathCommonPrefixA));
    static char o[G8BUF];
    for (size_t i = 0; i < sizeof cp_tab / sizeof *cp_tab; i++) {
        const cp_case *c = &cp_tab[i];
        memset(o, 'Z', 32); o[32] = 0;
        int r = P(PathCommonPrefixA)(c->f1, c->f2, o);
        CHECKF(r == c->ret, "(\"%s\",\"%s\") ret %d want %d", c->f1, c->f2, r, c->ret);
        CHECKF(!strcmp(o, c->out), "(\"%s\",\"%s\") out \"%s\" want \"%s\"", c->f1, c->f2, o, c->out);
    }
}
TEST(gap8_commonprefix_null_args) {
    REQUIRE(P(PathCommonPrefixA));
    char o[16];
    strcpy(o, "zz");
    CHECK(P(PathCommonPrefixA)(NULL, "C:\\a", o) == 0);
    CHECK(!strcmp(o, "zz"));          /* untouched when an input is NULL */
    CHECK(P(PathCommonPrefixA)("C:\\a", NULL, o) == 0);
    CHECK(!strcmp(o, "zz"));
    CHECK(P(PathCommonPrefixA)(NULL, NULL, o) == 0);
    CHECK(!strcmp(o, "zz"));
    CHECK(P(PathCommonPrefixA)(NULL, NULL, NULL) == 0);
}
TEST(gap8_commonprefix_null_out) {
    REQUIRE(P(PathCommonPrefixA));
    CHECK(P(PathCommonPrefixA)("C:\\a\\b", "C:\\a\\c", NULL) == 4);
    CHECK(P(PathCommonPrefixA)("C:\\a", "D:\\a", NULL) == 0);
}
TEST(gap8_commonprefix_symmetry) {
    REQUIRE(P(PathCommonPrefixA));
    for (size_t i = 0; i < sizeof cp_tab / sizeof *cp_tab; i++) {
        const cp_case *c = &cp_tab[i];
        int a = P(PathCommonPrefixA)(c->f1, c->f2, NULL);
        int b = P(PathCommonPrefixA)(c->f2, c->f1, NULL);
        CHECKF(a == b, "(\"%s\",\"%s\") %d vs %d", c->f1, c->f2, a, b);
    }
}
TEST(gap8_commonprefix_drive_only) {
    REQUIRE(P(PathCommonPrefixA));
    char o[16] = "zz";
    int r = P(PathCommonPrefixA)("C:", "C:", o);
    t_note("CommonPrefix(\"C:\",\"C:\") = %d \"%s\"", r, o);
    r = P(PathCommonPrefixA)("C:/a/b", "C:/a/c", o);
    t_note("CommonPrefix forward slashes = %d \"%s\"", r, o);
}

/* ---------------- PathFindNextComponentA ---------------- */
typedef struct { const char *in; long off; } fn_case;
static const fn_case fnc_tab[] = {
    { "", -1 }, { "a", 1 }, { "a\\b\\c", 2 }, { "\\a", 1 }, { "a\\", 2 }, { "C:\\a", 3 },
    { "\\\\srv\\share", 2 }, { "a\\\\b", 3 }, { "\\\\\\a", 2 }, { "abc", 3 }, { "ab\\cd", 3 },
    { "a/b\\c", 4 }, { "\\", 1 }, { "\\\\", 2 },
};
TEST(gap8_findnextcomponent_table) {
    REQUIRE(P(PathFindNextComponentA));
    static char b[G8BUF];
    for (size_t i = 0; i < sizeof fnc_tab / sizeof *fnc_tab; i++) {
        g8_set(b, fnc_tab[i].in);
        char *r = P(PathFindNextComponentA)(b);
        CHECKF(OFF(r, b) == fnc_tab[i].off, "in=\"%s\" off %ld want %ld", fnc_tab[i].in, OFF(r, b), fnc_tab[i].off);
    }
}
TEST(gap8_findnextcomponent_null) {
    REQUIRE(P(PathFindNextComponentA));
    CHECK(P(PathFindNextComponentA)(NULL) == NULL);
}
TEST(gap8_findnextcomponent_walk) {
    REQUIRE(P(PathFindNextComponentA));
    static char b[G8BUF];
    g8_set(b, "a\\bb\\ccc\\d");
    const char *want[] = { "a", "bb", "ccc", "d" };
    const char *p = b; int n = 0;
    while (p && *p && n < 10) {
        size_t l = strcspn(p, "\\");
        CHECKF(n < 4 && l == strlen(want[n]) && !strncmp(p, want[n], l), "component %d", n);
        p = P(PathFindNextComponentA)(p); n++;
    }
    CHECK(n == 4);
    CHECK(p && *p == '\0');   /* walks to the NUL, not to NULL */
}

/* ---------------- PathGetArgsA ---------------- */
static const fn_case ga_tab[] = {
    { "", 0 }, { "prog", 4 }, { "prog arg", 5 }, { "prog   arg", 7 }, { "prog arg1 arg2", 5 }, { "\"a b\" c", 6 },
    { "prog ", 5 }, { " x", 1 }, { "\"prog name\"", 11 }, { "prog \"x y\"", 5 }, { "a\tb", 3 }, { "a  ", 3 },
    { "\"C:\\Program Files\\x.exe\" /q /v", 25 }, { "\"unterminated arg", 17 }, { "a \"b", 2 },
    { "C:\\x.exe -flag \"v w\"", 9 },
};
TEST(gap8_getargs_table) {
    REQUIRE(P(PathGetArgsA));
    static char b[G8BUF];
    for (size_t i = 0; i < sizeof ga_tab / sizeof *ga_tab; i++) {
        g8_set(b, ga_tab[i].in);
        char *r = P(PathGetArgsA)(b);
        CHECKF(OFF(r, b) == ga_tab[i].off, "in=\"%s\" off %ld want %ld", ga_tab[i].in, OFF(r, b), ga_tab[i].off);
    }
}
TEST(gap8_getargs_null) {
    REQUIRE(P(PathGetArgsA));
    CHECK(P(PathGetArgsA)(NULL) == NULL);
}
TEST(gap8_getargs_never_null_for_string) {
    REQUIRE(P(PathGetArgsA));
    static char b[G8BUF];
    static const char *ins[] = { "", " ", "  ", "\"", "\"\"", "x", "x ", "x y", "\" \"" };
    for (size_t i = 0; i < sizeof ins / sizeof *ins; i++) {
        g8_set(b, ins[i]);
        char *r = P(PathGetArgsA)(b);
        CHECKF(r && r >= b && r <= b + strlen(b), "in=\"%s\" must point within the string (incl. its NUL)", ins[i]);
    }
}
