/* PathMatchSpecA, PathMatchSpecExA/W, PathParseIconLocationA, PathQuoteSpacesA, PathRemoveBackslashA,
 * PathRemoveBlanksA, PathRemoveExtensionA, PathRemoveFileSpecA, PathRenameExtensionA */
#include "g8.h"

#ifndef PMSF_NORMAL
#define PMSF_NORMAL 0
#define PMSF_MULTIPLE 1
#define PMSF_DONT_STRIP_SPACES 0x10000
#endif

static void widen(WCHAR *w, const char *s) { int i = 0; while ((w[i] = (unsigned char)s[i])) i++; }

/* ---------------- PathMatchSpec ---------------- */
typedef struct { const char *file, *spec; int multi, normal; } ms_case;
static const ms_case ms_tab[] = {
    { "a.txt", "*.txt", 1, 1 }, { "A.TXT", "*.txt", 1, 1 }, { "a.txt", "*.doc", 0, 0 }, { "a.txt", "*", 1, 1 },
    { "a.txt", "*.*", 1, 1 }, { "a", "*.*", 1, 1 }, { "a.txt", "a.???", 1, 1 }, { "a.txt", "?.txt", 1, 1 },
    { "ab.txt", "?.txt", 0, 0 }, { "a.txt", "a.t?t", 1, 1 }, { "a.txt", "*.txt;*.doc", 1, 0 },
    { "a.doc", "*.txt;*.doc", 1, 0 }, { "a.xyz", "*.txt;*.doc", 0, 0 }, { "a.doc", "*.txt; *.doc", 1, 0 },
    { "a.txt", "a*", 1, 1 }, { "a.txt", "*txt", 1, 1 }, { "a.txt", "*x", 0, 0 }, { "a.txt", "a.txtx", 0, 0 },
    { "a.b.txt", "*.txt", 1, 1 }, { ".txt", "*.txt", 1, 1 }, { "txt", "*.txt", 0, 0 }, { "a.txt", "A.TxT", 1, 1 },
    { "", "*", 1, 1 }, { "x", "", 1, 0 }, { "", "", 1, 1 }, { "abc", "abc", 1, 1 }, { "abc", "abd", 0, 0 },
    { "abc", "ab", 0, 0 }, { "ab", "abc", 0, 0 }, { "a.txt", "*.t*", 1, 1 }, { "a.txt", " *.txt", 1, 1 },
    { "readme", "read*", 1, 1 }, { "readme", "*me", 1, 1 }, { "readme", "*ad*", 1, 1 }, { "readme", "*zz*", 0, 0 },
    { "a.txt", "??.txt", 0, 0 }, { "ab.txt", "??.txt", 1, 1 }, { "ab.txt", "??.???", 1, 1 }, { "ab.tx", "??.???", 1, 1 },
    { "x.c", "*.c;*.h;*.cpp", 1, 0 }, { "x.cpp", "*.c;*.h;*.cpp", 1, 0 }, { "x.o", "*.c;*.h;*.cpp", 0, 0 },
    { "dir.ext.txt", "*.ext.txt", 1, 1 }, { "FILE.DAT", "file.*", 1, 1 }, { "file", "file.*", 0, 0 },
};
TEST(gap8_matchspec_a_table) {
    REQUIRE(P(PathMatchSpecA));
    for (size_t i = 0; i < sizeof ms_tab / sizeof *ms_tab; i++) {
        const ms_case *c = &ms_tab[i];
        BOOL r = P(PathMatchSpecA)(c->file, c->spec);
        CHECKF(!!r == c->multi, "file=\"%s\" spec=\"%s\" got %d want %d", c->file, c->spec, r, c->multi);
    }
}
TEST(gap8_matchspec_a_null) {
    REQUIRE(P(PathMatchSpecA));
    if (t_is_wine()) { t_skip("NULL arguments fault under this emulator"); return; }
    CHECK(P(PathMatchSpecA)(NULL, "*") == FALSE);
    CHECK(P(PathMatchSpecA)("a", NULL) == FALSE);
    CHECK(P(PathMatchSpecA)(NULL, NULL) == FALSE);
}
TEST(gap8_matchspec_a_quirk_notes) {
    REQUIRE(P(PathMatchSpecA));
    t_note("MatchSpec(\"a.txt\",\"a.txt?\") = %d", P(PathMatchSpecA)("a.txt", "a.txt?"));
    t_note("MatchSpec(\"a\\\\b\",\"*\") = %d", P(PathMatchSpecA)("a\\b", "*"));
    t_note("MatchSpec(\"a.txt\",\";*.txt\") = %d", P(PathMatchSpecA)("a.txt", ";*.txt"));
    t_note("MatchSpec(\"a.txt\",\"*.doc;\") = %d", P(PathMatchSpecA)("a.txt", "*.doc;"));
}
TEST(gap8_matchspec_a_case_insensitive_all_letters) {
    REQUIRE(P(PathMatchSpecA));
    char f[2] = "a", s[2] = "A";
    for (int i = 0; i < 26; i++) {
        f[0] = (char)('a' + i); s[0] = (char)('A' + i);
        CHECK(P(PathMatchSpecA)(f, s) == TRUE);
        CHECK(P(PathMatchSpecA)(s, f) == TRUE);
        s[0] = (char)('A' + (i + 1) % 26);
        CHECK(P(PathMatchSpecA)(f, s) == FALSE);
    }
}
TEST(gap8_matchspec_a_many_alternatives) {
    REQUIRE(P(PathMatchSpecA));
    static char spec[4096];
    spec[0] = 0;
    for (int i = 0; i < 200; i++) { char t[16]; sprintf(t, "*.e%d;", i); strcat(spec, t); }
    CHECK(P(PathMatchSpecA)("f.e0", spec) == TRUE);
    CHECK(P(PathMatchSpecA)("f.e199", spec) == TRUE);
    CHECK(P(PathMatchSpecA)("f.e200", spec) == FALSE);
}
TEST(gap8_matchspec_a_star_pathological) {
    REQUIRE(P(PathMatchSpecA));
    /* bounded backtracking sanity: several stars against a mismatching name */
    CHECK(P(PathMatchSpecA)("aaaaaaaaaaaaaaaaaaaa", "*a*a*a*a*b") == FALSE);
    CHECK(P(PathMatchSpecA)("aaaaaaaaaaaaaaaaaaab", "*a*a*a*a*b") == TRUE);
}

TEST(gap8_matchspecex_a_flags) {
    REQUIRE(P(PathMatchSpecExA));
    for (size_t i = 0; i < sizeof ms_tab / sizeof *ms_tab; i++) {
        const ms_case *c = &ms_tab[i];
        HRESULT hm = P(PathMatchSpecExA)(c->file, c->spec, PMSF_MULTIPLE);
        HRESULT hn = P(PathMatchSpecExA)(c->file, c->spec, PMSF_NORMAL);
        CHECKF(hm == (c->multi ? S_OK : S_FALSE), "MULTIPLE file=\"%s\" spec=\"%s\" got 0x%lx", c->file, c->spec, (long)hm);
        CHECKF(hn == (c->normal ? S_OK : S_FALSE), "NORMAL file=\"%s\" spec=\"%s\" got 0x%lx", c->file, c->spec, (long)hn);
    }
}
TEST(gap8_matchspecex_w_flags) {
    REQUIRE(P(PathMatchSpecExW));
    WCHAR wf[64], ws[64];
    for (size_t i = 0; i < sizeof ms_tab / sizeof *ms_tab; i++) {
        const ms_case *c = &ms_tab[i];
        widen(wf, c->file); widen(ws, c->spec);
        HRESULT hm = P(PathMatchSpecExW)(wf, ws, PMSF_MULTIPLE);
        HRESULT hn = P(PathMatchSpecExW)(wf, ws, PMSF_NORMAL);
        CHECKF(hm == (c->multi ? S_OK : S_FALSE), "W MULTIPLE file=\"%s\" spec=\"%s\" got 0x%lx", c->file, c->spec, (long)hm);
        CHECKF(hn == (c->normal ? S_OK : S_FALSE), "W NORMAL file=\"%s\" spec=\"%s\" got 0x%lx", c->file, c->spec, (long)hn);
    }
}
TEST(gap8_matchspecex_ab_parity) {
    REQUIRE(P(PathMatchSpecExA)); REQUIRE(P(PathMatchSpecExW));
    WCHAR wf[64], ws[64];
    DWORD fl[] = { PMSF_NORMAL, PMSF_MULTIPLE, PMSF_NORMAL | PMSF_DONT_STRIP_SPACES, PMSF_MULTIPLE | PMSF_DONT_STRIP_SPACES };
    for (size_t i = 0; i < sizeof ms_tab / sizeof *ms_tab; i++)
        for (int k = 0; k < 4; k++) {
            const ms_case *c = &ms_tab[i];
            widen(wf, c->file); widen(ws, c->spec);
            HRESULT a = P(PathMatchSpecExA)(c->file, c->spec, fl[k]);
            HRESULT w = P(PathMatchSpecExW)(wf, ws, fl[k]);
            CHECKF(a == w, "A/W differ file=\"%s\" spec=\"%s\" flags=0x%lx: %lx vs %lx", c->file, c->spec, (long)fl[k], (long)a, (long)w);
        }
}
TEST(gap8_matchspecex_dont_strip_spaces) {
    REQUIRE(P(PathMatchSpecExA)); REQUIRE(P(PathMatchSpecExW));
    WCHAR wf[8], ws[16];
    widen(wf, "a.doc"); widen(ws, " *.doc");
    CHECK(P(PathMatchSpecExA)("a.doc", " *.doc", PMSF_NORMAL) == S_OK);
    CHECK(P(PathMatchSpecExA)("a.doc", " *.doc", PMSF_NORMAL | PMSF_DONT_STRIP_SPACES) == S_FALSE);
    CHECK(P(PathMatchSpecExA)("a.doc", "*.txt; *.doc", PMSF_MULTIPLE) == S_OK);
    CHECK(P(PathMatchSpecExA)("a.doc", "*.txt; *.doc", PMSF_MULTIPLE | PMSF_DONT_STRIP_SPACES) == S_FALSE);
    CHECK(P(PathMatchSpecExW)(wf, ws, PMSF_NORMAL) == S_OK);
    CHECK(P(PathMatchSpecExW)(wf, ws, PMSF_NORMAL | PMSF_DONT_STRIP_SPACES) == S_FALSE);
    /* a pattern that is literally a leading space then the name matches a name with the leading space */
    CHECK(P(PathMatchSpecExA)(" x", " x", PMSF_NORMAL | PMSF_DONT_STRIP_SPACES) == S_OK);
}
TEST(gap8_matchspecex_null_notes) {
    REQUIRE(P(PathMatchSpecExA)); REQUIRE(P(PathMatchSpecExW));
    if (t_is_wine()) { t_skip("NULL arguments fault under this emulator"); return; }
    t_note("ExA(NULL,\"*\",0)=0x%lx ExA(\"a\",NULL,0)=0x%lx ExA(NULL,NULL,1)=0x%lx",
           (long)P(PathMatchSpecExA)(NULL, "*", 0), (long)P(PathMatchSpecExA)("a", NULL, 0), (long)P(PathMatchSpecExA)(NULL, NULL, 1));
    WCHAR w[2] = { '*', 0 };
    t_note("ExW(NULL,\"*\",0)=0x%lx ExW(L\"a\",NULL,1)=0x%lx", (long)P(PathMatchSpecExW)(NULL, w, 0), (long)P(PathMatchSpecExW)(w, NULL, 1));
}
TEST(gap8_matchspecex_unknown_flags_note) {
    REQUIRE(P(PathMatchSpecExA));
    DWORD odd[] = { 2, 4, 0x10, 0x8000, 0x20000, 0x80000000, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof odd / sizeof *odd; i++)
        t_note("ExA(\"a.txt\",\"*.txt\",0x%lx) = 0x%lx", (long)odd[i], (long)P(PathMatchSpecExA)("a.txt", "*.txt", odd[i]));
}
TEST(gap8_matchspecex_w_unicode) {
    REQUIRE(P(PathMatchSpecExW));
    WCHAR f[8] = { 0x00e9, '.', 't', 'x', 't', 0 };         /* e-acute */
    WCHAR sl[8] = { 0x00c9, '.', '*', 0 };                  /* E-acute */
    WCHAR su[8] = { '?', '.', 't', 'x', 't', 0 };
    CHECK(P(PathMatchSpecExW)(f, su, PMSF_NORMAL) == S_OK);
    HRESULT hr = P(PathMatchSpecExW)(f, sl, PMSF_NORMAL);
    t_note("case-insensitive e-acute match -> 0x%lx", (long)hr);
}

/* ---------------- PathParseIconLocationA ---------------- */
typedef struct { const char *in; int ret; const char *out; } pi_case;
static const pi_case pi_tab[] = {
    { "f.dll,3", 3, "f.dll" }, { "f.dll,-3", -3, "f.dll" }, { "f.dll", 0, "f.dll" }, { "f.dll,0", 0, "f" "." "dll" },
    { "f.dll,abc", 0, "f.dll,abc" }, { "f.dll,", 0, "f.dll," }, { "\"C:\\a b.dll\",2", 2, "C:\\a b.dll" },
    { "  f.dll  ,1", 1, "f.dll" }, { "f.dll,3x", 3, "f.dll" }, { "a,b,7", 7, "a,b" }, { "f.dll,12", 12, "f.dll" },
    { "", 0, "" }, { "f.dll,x5", 0, "f.dll,x5" }, { "f.dll,-", 0, "f.dll,-" }, { "C:\\d\\i.ico,0", 0, "C:\\d\\i.ico" },
    { ",5", 5, "" }, { "f.dll,007", 7, "f.dll" }, { "f.dll,2147483647", 2147483647, "f.dll" },
    { "\"f.dll\"", 0, "f.dll" }, { "  f.dll  ", 0, "f.dll" }, { "f.dll,1,", 0, "f.dll,1," },
};
TEST(gap8_parseiconlocation_table) {
    REQUIRE(P(PathParseIconLocationA));
    static char b[G8BUF];
    for (size_t i = 0; i < sizeof pi_tab / sizeof *pi_tab; i++) {
        const pi_case *c = &pi_tab[i];
        int r = P(PathParseIconLocationA)(g8_set(b, c->in));
        CHECKF(r == c->ret, "in=\"%s\" ret %d want %d", c->in, r, c->ret);
        CHECKF(!strcmp(b, c->out), "in=\"%s\" out \"%s\" want \"%s\"", c->in, b, c->out);
    }
}
TEST(gap8_parseiconlocation_null) {
    REQUIRE(P(PathParseIconLocationA));
    CHECK(P(PathParseIconLocationA)(NULL) == 0);
}
TEST(gap8_parseiconlocation_space_notes) {
    REQUIRE(P(PathParseIconLocationA));
    static char b[G8BUF];
    static const char *ins[] = { "f.dll, 5", "f.dll,- 5", "f.dll,\t5", "f.dll,--5", "f.dll,+5" };
    for (size_t i = 0; i < sizeof ins / sizeof *ins; i++) {
        int r = P(PathParseIconLocationA)(g8_set(b, ins[i]));
        t_note("ParseIconLocation(\"%s\") = %d \"%s\"", ins[i], r, b);
    }
}

/* ---------------- PathQuoteSpacesA ---------------- */
static const pi_case qs_tab[] = {
    { "abc", 0, "abc" }, { "a b", 1, "\"a b\"" }, { " ", 1, "\" \"" }, { "", 0, "" },
    { "C:\\Program Files\\x.exe", 1, "\"C:\\Program Files\\x.exe\"" }, { "a\tb", 0, "a\tb" }, { "a  b", 1, "\"a  b\"" },
    { " lead", 1, "\" lead\"" }, { "trail ", 1, "\"trail \"" }, { "a,b", 0, "a,b" },
};
TEST(gap8_quotespaces_table) {
    REQUIRE(PQS);
    static char b[G8BUF];
    for (size_t i = 0; i < sizeof qs_tab / sizeof *qs_tab; i++) {
        const pi_case *c = &qs_tab[i];
        BOOL r = PQS(g8_set(b, c->in));
        CHECKF(!!r == c->ret, "in=\"%s\" ret %d want %d", c->in, r, c->ret);
        CHECKF(!strcmp(b, c->out), "in=\"%s\" out \"%s\" want \"%s\"", c->in, b, c->out);
    }
}
TEST(gap8_quotespaces_null) {
    REQUIRE(PQS);
    CHECK(PQS(NULL) == FALSE);
}
TEST(gap8_quotespaces_length_boundary) {
    REQUIRE(PQS);
    static char b[G8BUF];
    g8_fill(b, 257, 'a'); b[100] = ' ';
    BOOL r = PQS(b);
    CHECK(r == TRUE && strlen(b) == 259 && b[0] == '"' && b[258] == '"');
    g8_fill(b, 258, 'a'); b[100] = ' ';
    r = PQS(b);
    CHECK(r == FALSE && strlen(b) == 258 && b[0] == 'a');
}
TEST(gap8_quotespaces_roundtrip_unquote) {
    REQUIRE(PQS); REQUIRE(PUQS);
    static char b[G8BUF];
    g8_set(b, "C:\\My Dir\\f.txt");
    CHECK(PQS(b));
    CHECK(PUQS(b));
    CHECK(!strcmp(b, "C:\\My Dir\\f.txt"));
}
TEST(gap8_quotespaces_double_note) {
    REQUIRE(PQS);
    static char b[G8BUF];
    BOOL r = PQS(g8_set(b, "\"a b\""));
    t_note("QuoteSpaces on already-quoted -> %d \"%s\"", r, b);
}

/* ---------------- PathRemoveBackslashA ---------------- */
typedef struct { const char *in, *out; long off; } rb_case;
static const rb_case rb_tab[] = {
    { "", "", 0 }, { "a", "a", 0 }, { "a\\", "a", 1 }, { "a\\\\", "a\\", 2 }, { "\\", "\\", 0 }, { "C:", "C:", 1 },
    { "C:\\a\\b\\", "C:\\a\\b", 6 }, { "C:\\a b\\", "C:\\a b", 6 }, { "C:\\", "C:\\", 2 }, { "C:\\a", "C:\\a", 3 },
    { "C:\\a\\", "C:\\a", 4 }, { "a/", "a/", 1 }, { "dir\\sub\\", "dir\\sub", 7 },
};
TEST(gap8_removebackslash_table) {
    REQUIRE(P(PathRemoveBackslashA));
    static char b[G8BUF];
    for (size_t i = 0; i < sizeof rb_tab / sizeof *rb_tab; i++) {
        const rb_case *c = &rb_tab[i];
        char *r = P(PathRemoveBackslashA)(g8_set(b, c->in));
        CHECKF(!strcmp(b, c->out), "in=\"%s\" out \"%s\" want \"%s\"", c->in, b, c->out);
        CHECKF(OFF(r, b) == c->off, "in=\"%s\" ret off %ld want %ld", c->in, OFF(r, b), c->off);
    }
}
TEST(gap8_removebackslash_null) {
    REQUIRE(P(PathRemoveBackslashA));
    CHECK(P(PathRemoveBackslashA)(NULL) == NULL);
}
TEST(gap8_removebackslash_unc_notes) {
    REQUIRE(P(PathRemoveBackslashA));
    static char b[G8BUF];
    static const char *ins[] = { "\\\\srv\\share\\", "\\\\srv\\", "\\\\srv", "\\\\", "\\\\srv\\share\\dir\\" };
    for (size_t i = 0; i < sizeof ins / sizeof *ins; i++) {
        char *r = P(PathRemoveBackslashA)(g8_set(b, ins[i]));
        t_note("RemoveBackslash(\"%s\") -> \"%s\" off %ld", ins[i], b, OFF(r, b));
    }
}
TEST(gap8_removebackslash_long_path) {
    REQUIRE(P(PathRemoveBackslashA));
    static char b[G8BUF];
    g8_fill(b, 600, 'a'); b[599] = '\\';
    char *r = P(PathRemoveBackslashA)(b);
    CHECK(strlen(b) == 599 && r == b + 599);
}
TEST(gap8_removebackslash_addbackslash_inverse) {
    REQUIRE(P(PathRemoveBackslashA)); REQUIRE(P(PathAddBackslashA));
    static char b[G8BUF];
    g8_set(b, "C:\\a\\b");
    P(PathAddBackslashA)(b); CHECK(!strcmp(b, "C:\\a\\b\\"));
    P(PathRemoveBackslashA)(b); CHECK(!strcmp(b, "C:\\a\\b"));
    g8_set(b, "C:\\");
    P(PathRemoveBackslashA)(b); CHECK(!strcmp(b, "C:\\"));
}

/* ---------------- PathRemoveBlanksA ---------------- */
static const pi_case bl_tab[] = {
    { "", 0, "" }, { " ", 0, "" }, { "   ", 0, "" }, { "a", 0, "a" }, { " a", 0, "a" }, { "a ", 0, "a" },
    { "  a b  ", 0, "a b" }, { "a  b", 0, "a  b" }, { "\ta\t", 0, "\ta\t" }, { " \ta\t ", 0, "\ta\t" },
    { "C:\\dir name\\ ", 0, "C:\\dir name\\" }, { "  \"q w\"  ", 0, "\"q w\"" },
};
TEST(gap8_removeblanks_table) {
    REQUIRE(P(PathRemoveBlanksA));
    static char b[G8BUF];
    for (size_t i = 0; i < sizeof bl_tab / sizeof *bl_tab; i++) {
        P(PathRemoveBlanksA)(g8_set(b, bl_tab[i].in));
        CHECKF(!strcmp(b, bl_tab[i].out), "in=\"%s\" out \"%s\" want \"%s\"", bl_tab[i].in, b, bl_tab[i].out);
    }
}
TEST(gap8_removeblanks_null) {
    REQUIRE(P(PathRemoveBlanksA));
    P(PathRemoveBlanksA)(NULL);
    CHECK(1);
}
TEST(gap8_removeblanks_long) {
    REQUIRE(P(PathRemoveBlanksA));
    static char b[G8BUF];
    g8_fill(b, 800, ' '); b[100] = 'x'; b[500] = 'y';
    P(PathRemoveBlanksA)(b);
    CHECK(b[0] == 'x' && strlen(b) == 401 && b[400] == 'y');
}
TEST(gap8_removeblanks_idempotent) {
    REQUIRE(P(PathRemoveBlanksA));
    static char b[G8BUF], c[G8BUF];
    g8_set(b, "   a  b   ");
    P(PathRemoveBlanksA)(b);
    strcpy(c, b);
    P(PathRemoveBlanksA)(b);
    CHECK(!strcmp(b, c));
}

/* ---------------- PathRemoveExtensionA ---------------- */
typedef struct { const char *in, *out; } io_case;
static const io_case re_tab[] = {
    { "a.txt", "a" }, { "a.b.c", "a.b" }, { "a", "a" }, { "dir.x\\a", "dir.x\\a" }, { "a.", "a" }, { ".txt", "" },
    { "a b.c d", "a b.c d" }, { "C:\\d.x\\f.ext", "C:\\d.x\\f" }, { "", "" }, { "a.b c", "a.b c" },
    { "C:\\a\\b.tar.gz", "C:\\a\\b.tar" }, { "a.txt ", "a.txt " }, { "..", "." }, { ".", "" }, { "a/b.c", "a/b" },
};
TEST(gap8_removeextension_table) {
    REQUIRE(P(PathRemoveExtensionA));
    static char b[G8BUF];
    for (size_t i = 0; i < sizeof re_tab / sizeof *re_tab; i++) {
        P(PathRemoveExtensionA)(g8_set(b, re_tab[i].in));
        CHECKF(!strcmp(b, re_tab[i].out), "in=\"%s\" out \"%s\" want \"%s\"", re_tab[i].in, b, re_tab[i].out);
    }
}
TEST(gap8_removeextension_null) {
    REQUIRE(P(PathRemoveExtensionA));
    P(PathRemoveExtensionA)(NULL);
    CHECK(1);
}
TEST(gap8_removeextension_too_long_untouched) {
    REQUIRE(P(PathRemoveExtensionA));
    static char b[G8BUF];
    /* longer than MAX_PATH: the extension search fails and the string is left alone */
    g8_fill(b, 300, 'a'); strcpy(b + 296, ".txt");
    P(PathRemoveExtensionA)(b);
    CHECK(strlen(b) == 300);
    g8_fill(b, 255, 'a'); strcpy(b + 251, ".txt");
    P(PathRemoveExtensionA)(b);
    CHECK(strlen(b) == 251);
}
TEST(gap8_removeextension_repeat) {
    REQUIRE(P(PathRemoveExtensionA));
    static char b[G8BUF];
    g8_set(b, "a.b.c.d");
    P(PathRemoveExtensionA)(b); CHECK(!strcmp(b, "a.b.c"));
    P(PathRemoveExtensionA)(b); CHECK(!strcmp(b, "a.b"));
    P(PathRemoveExtensionA)(b); CHECK(!strcmp(b, "a"));
    P(PathRemoveExtensionA)(b); CHECK(!strcmp(b, "a"));
}

/* ---------------- PathRemoveFileSpecA ---------------- */
static const pi_case rf_tab[] = {
    { "C:\\a\\b.txt", 1, "C:\\a" }, { "C:\\a", 1, "C:\\" }, { "C:\\", 0, "C:\\" }, { "C:", 0, "C:" },
    { "C:\\a\\", 1, "C:\\a" }, { "a", 1, "" }, { "a\\b", 1, "a" }, { "\\a", 1, "\\" }, { "\\", 0, "\\" }, { "", 0, "" },
    { "a\\", 1, "a" }, { "\\\\srv\\share\\f", 1, "\\\\srv\\share" }, { "C:\\a\\b\\c.d\\e", 1, "C:\\a\\b\\c.d" },
    { "C:\\a\\b\\", 1, "C:\\a\\b" }, { "\\a\\b", 1, "\\a" }, { "C:\\dir\\sub\\file.ext", 1, "C:\\dir\\sub" },
    { "file.ext", 1, "" }, { "C:file", 1, "C:" }, { "C:\\x y\\z w.t", 1, "C:\\x y" },
};
TEST(gap8_removefilespec_table) {
    REQUIRE(P(PathRemoveFileSpecA));
    static char b[G8BUF];
    for (size_t i = 0; i < sizeof rf_tab / sizeof *rf_tab; i++) {
        const pi_case *c = &rf_tab[i];
        BOOL r = P(PathRemoveFileSpecA)(g8_set(b, c->in));
        if (!strcmp(c->in, "C:file")) { t_note("RemoveFileSpec(\"C:file\") -> %d \"%s\"", r, b); continue; }
        CHECKF(!!r == c->ret, "in=\"%s\" ret %d want %d", c->in, r, c->ret);
        CHECKF(!strcmp(b, c->out), "in=\"%s\" out \"%s\" want \"%s\"", c->in, b, c->out);
    }
}
TEST(gap8_removefilespec_null) {
    REQUIRE(P(PathRemoveFileSpecA));
    CHECK(P(PathRemoveFileSpecA)(NULL) == FALSE);
}
TEST(gap8_removefilespec_repeat_to_root) {
    REQUIRE(P(PathRemoveFileSpecA));
    static char b[G8BUF];
    g8_set(b, "C:\\a\\b\\c");
    CHECK(P(PathRemoveFileSpecA)(b) && !strcmp(b, "C:\\a\\b"));
    CHECK(P(PathRemoveFileSpecA)(b) && !strcmp(b, "C:\\a"));
    CHECK(P(PathRemoveFileSpecA)(b) && !strcmp(b, "C:\\"));
    CHECK(P(PathRemoveFileSpecA)(b) == FALSE && !strcmp(b, "C:\\"));
}
TEST(gap8_removefilespec_unc_notes) {
    REQUIRE(P(PathRemoveFileSpecA));
    static char b[G8BUF];
    static const char *ins[] = { "\\\\srv\\share", "\\\\srv", "\\\\srv\\share\\", "\\\\srv\\share\\d\\f", "C:/a/b", "\\\\?\\C:\\a\\b", "C:\\a\\\\b" };
    for (size_t i = 0; i < sizeof ins / sizeof *ins; i++) {
        BOOL r = P(PathRemoveFileSpecA)(g8_set(b, ins[i]));
        t_note("RemoveFileSpec(\"%s\") -> %d \"%s\"", ins[i], r, b);
    }
}
TEST(gap8_removefilespec_unc_deep) {
    REQUIRE(P(PathRemoveFileSpecA));
    static char b[G8BUF];
    g8_set(b, "\\\\srv\\share\\d\\f");
    CHECK(P(PathRemoveFileSpecA)(b) == TRUE);
    CHECK(!strcmp(b, "\\\\srv\\share\\d"));
    CHECK(P(PathRemoveFileSpecA)(b) == TRUE);
    CHECK(!strcmp(b, "\\\\srv\\share"));
}

/* ---------------- PathRenameExtensionA ---------------- */
typedef struct { const char *in, *ext; int ret; const char *out; } rn_case;
static const rn_case rn_tab[] = {
    { "a.txt", ".bak", 1, "a.bak" }, { "a", ".bak", 1, "a.bak" }, { "a.txt", "bak", 1, "abak" }, { "a.txt", "", 1, "a" },
    { "a.b.c", ".d", 1, "a.b.d" }, { "dir.x\\f", ".y", 1, "dir.x\\f.y" }, { "a.txt", ".", 1, "a." }, { ".txt", ".x", 1, ".x" },
    { "", ".x", 1, ".x" }, { "C:\\a\\b.old", ".new", 1, "C:\\a\\b.new" }, { "a b.c d", ".x", 1, "a b.c d.x" },
    { "a.", ".b", 1, "a.b" }, { "a.txt", ".longer_extension", 1, "a.longer_extension" },
};
TEST(gap8_renameextension_table) {
    REQUIRE(P(PathRenameExtensionA));
    static char b[G8BUF];
    for (size_t i = 0; i < sizeof rn_tab / sizeof *rn_tab; i++) {
        const rn_case *c = &rn_tab[i];
        BOOL r = P(PathRenameExtensionA)(g8_set(b, c->in), c->ext);
        CHECKF(!!r == c->ret, "in=\"%s\" ext=\"%s\" ret %d", c->in, c->ext, r);
        CHECKF(!strcmp(b, c->out), "in=\"%s\" ext=\"%s\" out \"%s\" want \"%s\"", c->in, c->ext, b, c->out);
    }
}
TEST(gap8_renameextension_null) {
    REQUIRE(P(PathRenameExtensionA));
    if (t_is_wine()) { t_skip("NULL arguments fault under this emulator"); return; }
    static char b[G8BUF];
    CHECK(P(PathRenameExtensionA)(NULL, ".x") == FALSE);
    CHECK(P(PathRenameExtensionA)(g8_set(b, "a.b"), NULL) == FALSE);
    CHECK(!strcmp(b, "a.b"));
    CHECK(P(PathRenameExtensionA)(NULL, NULL) == FALSE);
}
TEST(gap8_renameextension_length_boundary) {
    REQUIRE(P(PathRenameExtensionA));
    static char b[G8BUF];
    BOOL r = P(PathRenameExtensionA)(g8_fill(b, 255, 'a'), ".txt");
    CHECK(r == TRUE && strlen(b) == 259);
    r = P(PathRenameExtensionA)(g8_fill(b, 256, 'a'), ".txt");
    CHECK(r == FALSE && strlen(b) == 256);
    /* replacing a long extension by a short one always works */
    g8_fill(b, 250, 'a'); strcpy(b + 250, ".abcdefgh");
    r = P(PathRenameExtensionA)(b, ".x");
    CHECK(r == TRUE && strlen(b) == 252);
}
TEST(gap8_renameextension_vs_addextension) {
    REQUIRE(P(PathRenameExtensionA)); REQUIRE(P(PathAddExtensionA));
    static char b[G8BUF];
    /* Rename replaces; Add refuses when an extension exists */
    g8_set(b, "f.old");
    CHECK(P(PathAddExtensionA)(b, ".new") == FALSE && !strcmp(b, "f.old"));
    CHECK(P(PathRenameExtensionA)(b, ".new") == TRUE && !strcmp(b, "f.new"));
}
