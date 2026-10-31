/* PathSearchAndQualifyA/W, PathSkipRootA, PathStripPathA, PathStripToRootA, PathRelativePathToA, PathCreateFromUrlA */
#include "g8.h"

static void widen(WCHAR *w, const char *s) { int i = 0; while ((w[i] = (unsigned char)s[i])) i++; }

/* ---------------- PathSkipRootA ---------------- */
typedef struct { const char *in; long off; } sr_case;
static const sr_case skip_tab[] = {
    { "C:\\a", 3 }, { "C:\\", 3 }, { "C:", -1 }, { "C:a", -1 }, { "\\a", 1 }, { "\\", 1 }, { "\\\\srv\\share\\x", 12 },
    { "\\\\srv\\share", 11 }, { "\\\\srv", 5 }, { "\\\\", 2 }, { "a", -1 }, { "", -1 }, { "/a", -1 },
    { "\\\\?\\C:\\x", -1 }, { "c:\\", 3 }, { "1:\\", -1 }, { "C:\\\\x", 3 }, { "\\\\srv\\", 6 },
    { "C:\\a\\b\\c", 3 }, { "\\a\\b", 1 }, { "..\\a", -1 }, { "C:/a", -1 },
};
TEST(gap8_skiproot_table) {
    REQUIRE(P(PathSkipRootA));
    static char b[G8BUF];
    for (size_t i = 0; i < sizeof skip_tab / sizeof *skip_tab; i++) {
        g8_set(b, skip_tab[i].in);
        char *r = P(PathSkipRootA)(b);
        CHECKF(OFF(r, b) == skip_tab[i].off, "in=\"%s\" off %ld want %ld", skip_tab[i].in, OFF(r, b), skip_tab[i].off);
    }
}
TEST(gap8_skiproot_null) {
    REQUIRE(P(PathSkipRootA));
    CHECK(P(PathSkipRootA)(NULL) == NULL);
}
TEST(gap8_skiproot_double_backslash_after_server_note) {
    REQUIRE(P(PathSkipRootA));
    static char b[G8BUF];
    g8_set(b, "\\\\srv\\\\x");
    char *r = P(PathSkipRootA)(b);
    t_note("SkipRoot(\"\\\\\\\\srv\\\\\\\\x\") off %ld", OFF(r, b));
}
TEST(gap8_skiproot_result_points_into_input) {
    REQUIRE(P(PathSkipRootA));
    static char b[G8BUF];
    static const char *ins[] = { "C:\\Windows\\System32", "\\\\h\\s\\f\\g", "\\x\\y" };
    for (size_t i = 0; i < 3; i++) {
        g8_set(b, ins[i]);
        char *r = P(PathSkipRootA)(b);
        CHECKF(r && r > b && r <= b + strlen(b), "in=\"%s\"", ins[i]);
    }
}

/* ---------------- PathStripPathA ---------------- */
static const struct { const char *in, *out; } strip_tab[] = {
    { "C:\\a\\b.txt", "b.txt" }, { "b.txt", "b.txt" }, { "C:\\a\\", "a\\" }, { "a/b", "b" }, { "\\\\srv\\share", "share" },
    { "\\\\srv\\share\\f.txt", "f.txt" }, { "", "" }, { "C:\\f", "f" }, { "\\f", "f" }, { "a\\b\\c", "c" },
    { "C:\\a b\\c d.e", "c d.e" }, { "a/b/c.d", "c.d" }, { "x", "x" }, { "C:\\dir.ext\\file", "file" },
};
TEST(gap8_stripath_table) {
    REQUIRE(P(PathStripPathA));
    static char b[G8BUF];
    for (size_t i = 0; i < sizeof strip_tab / sizeof *strip_tab; i++) {
        P(PathStripPathA)(g8_set(b, strip_tab[i].in));
        CHECKF(!strcmp(b, strip_tab[i].out), "in=\"%s\" got \"%s\" want \"%s\"", strip_tab[i].in, b, strip_tab[i].out);
    }
}
TEST(gap8_stripath_null) {
    REQUIRE(P(PathStripPathA));
    P(PathStripPathA)(NULL);
    CHECK(1);
}
TEST(gap8_stripath_idempotent) {
    REQUIRE(P(PathStripPathA));
    static char b[G8BUF];
    g8_set(b, "C:\\a\\b\\c.txt");
    P(PathStripPathA)(b); CHECK(!strcmp(b, "c.txt"));
    P(PathStripPathA)(b); CHECK(!strcmp(b, "c.txt"));
}
TEST(gap8_stripath_long) {
    REQUIRE(P(PathStripPathA));
    static char b[G8BUF];
    g8_fill(b, 500, 'd'); b[10] = '\\';
    P(PathStripPathA)(b);
    CHECK(strlen(b) == 489 && b[0] == 'd');
}
TEST(gap8_stripath_drive_relative_note) {
    REQUIRE(P(PathStripPathA));
    static char b[G8BUF];
    P(PathStripPathA)(g8_set(b, "C:file.txt"));
    t_note("StripPath(\"C:file.txt\") -> \"%s\"", b);
}

/* ---------------- PathStripToRootA ---------------- */
typedef struct { const char *in; int ret; const char *out; } st_case;
static const st_case sto_tab[] = {
    { "C:\\a\\b", 1, "C:\\" }, { "C:\\", 1, "C:\\" }, { "C:", 1, "C:" }, { "a\\b", 0, "" }, { "\\a\\b", 1, "\\" },
    { "\\\\srv\\share\\x", 1, "\\\\srv\\share" }, { "", 0, "" }, { "c:\\Windows", 1, "c:\\" },
    { "\\\\srv\\share", 1, "\\\\srv\\share" }, { "\\\\srv", 1, "\\\\srv" }, { "C:\\a", 1, "C:\\" }, { "a", 0, "" },
    { "\\", 1, "\\" },
};
TEST(gap8_striptoroot_table) {
    REQUIRE(P(PathStripToRootA));
    static char b[G8BUF];
    for (size_t i = 0; i < sizeof sto_tab / sizeof *sto_tab; i++) {
        const st_case *c = &sto_tab[i];
        BOOL r = P(PathStripToRootA)(g8_set(b, c->in));
        CHECKF(!!r == c->ret, "in=\"%s\" ret %d want %d", c->in, r, c->ret);
        CHECKF(!strcmp(b, c->out), "in=\"%s\" got \"%s\" want \"%s\"", c->in, b, c->out);
    }
}
TEST(gap8_striptoroot_null) {
    REQUIRE(P(PathStripToRootA));
    CHECK(P(PathStripToRootA)(NULL) == FALSE);
}
TEST(gap8_striptoroot_iterate_with_removefilespec) {
    REQUIRE(P(PathStripToRootA)); REQUIRE(P(PathRemoveFileSpecA));
    static char a[G8BUF], b[G8BUF];
    g8_set(a, "C:\\x\\y\\z");
    g8_set(b, "C:\\x\\y\\z");
    P(PathStripToRootA)(a);
    while (P(PathRemoveFileSpecA)(b)) ;
    CHECK(!strcmp(a, b));
}

/* ---------------- PathRelativePathToA ---------------- */
#define DIR FILE_ATTRIBUTE_DIRECTORY
typedef struct { const char *from; DWORD af; const char *to; DWORD at; int ret; const char *out; } rp_case;
static const rp_case rp_tab[] = {
    { "C:\\Win\\Desktop", DIR, "C:\\Win\\system\\x.txt", 0, 1, "..\\system\\x.txt" },
    { "C:\\a\\b", DIR, "C:\\a\\b\\f.txt", 0, 1, ".\\f.txt" },
    { "C:\\a", DIR, "C:\\a\\b\\c", DIR, 1, ".\\b\\c" },
    { "C:\\a\\b\\c", DIR, "C:\\a", DIR, 1, "..\\.." },
    { "C:\\a\\b", DIR, "C:\\", DIR, 1, "..\\.." },
    { "C:\\a\\b.txt", 0, "C:\\a\\c.txt", 0, 1, ".\\c.txt" },
    { "C:\\a", DIR, "D:\\a", DIR, 0, "" },
    { "C:\\A\\B", DIR, "c:\\a\\b\\f", 0, 1, ".\\f" },
    { "C:\\a\\b", DIR, "C:\\a\\c", DIR, 1, "..\\c" },
    { "C:\\a\\b\\c", DIR, "C:\\a\\x\\y\\z", DIR, 1, "..\\..\\x\\y\\z" },
    { "C:\\a\\b.txt", 0, "C:\\a\\b.txt", 0, 1, ".\\b.txt" },
    { "C:\\a", DIR, "C:\\a", DIR, 1, "." },
    { "\\\\srv\\sh\\a", DIR, "\\\\srv\\sh\\b", DIR, 1, "..\\b" },
    { "C:\\a", DIR, "\\\\srv\\sh", DIR, 0, "" },
};
TEST(gap8_relativepathto_table) {
    REQUIRE(P(PathRelativePathToA));
    static char b[G8BUF];
    for (size_t i = 0; i < sizeof rp_tab / sizeof *rp_tab; i++) {
        const rp_case *c = &rp_tab[i];
        memset(b, 'Q', 64); b[64] = 0;
        BOOL r = P(PathRelativePathToA)(b, c->from, c->af, c->to, c->at);
        CHECKF(!!r == c->ret, "from=\"%s\" to=\"%s\" ret %d want %d (out \"%s\")", c->from, c->to, r, c->ret, b);
        CHECKF(!strcmp(b, c->out), "from=\"%s\" to=\"%s\" out \"%s\" want \"%s\"", c->from, c->to, b, c->out);
    }
}
TEST(gap8_relativepathto_null_args) {
    REQUIRE(P(PathRelativePathToA));
    char o[64];
    strcpy(o, "junk");
    CHECK(P(PathRelativePathToA)(NULL, "C:\\a", DIR, "C:\\b", DIR) == FALSE);
    CHECK(P(PathRelativePathToA)(o, NULL, DIR, "C:\\b", DIR) == FALSE);
    CHECK(o[0] == '\0');
    strcpy(o, "junk");
    CHECK(P(PathRelativePathToA)(o, "C:\\a", DIR, NULL, DIR) == FALSE);
    CHECK(o[0] == '\0');
    strcpy(o, "junk");
    CHECK(P(PathRelativePathToA)(o, NULL, DIR, NULL, DIR) == FALSE);
    CHECK(o[0] == '\0');
}
TEST(gap8_relativepathto_only_dir_bit_matters) {
    REQUIRE(P(PathRelativePathToA));
    char o1[MAX_PATH], o2[MAX_PATH];
    /* any attribute bits other than DIRECTORY are ignored */
    P(PathRelativePathToA)(o1, "C:\\a\\b", DIR, "C:\\a\\c", DIR);
    P(PathRelativePathToA)(o2, "C:\\a\\b", DIR | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY, "C:\\a\\c", DIR | FILE_ATTRIBUTE_ARCHIVE);
    CHECK(!strcmp(o1, o2));
    P(PathRelativePathToA)(o1, "C:\\a\\b", 0, "C:\\a\\c", 0);
    P(PathRelativePathToA)(o2, "C:\\a\\b", FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_HIDDEN, "C:\\a\\c", FILE_ATTRIBUTE_ARCHIVE);
    CHECK(!strcmp(o1, o2));
}
TEST(gap8_relativepathto_roundtrip) {
    REQUIRE(P(PathRelativePathToA)); REQUIRE(P(PathAppendA));
    char rel[MAX_PATH], full[MAX_PATH];
    CHECK(P(PathRelativePathToA)(rel, "C:\\p\\q\\r", DIR, "C:\\p\\s\\t.txt", 0));
    strcpy(full, "C:\\p\\q\\r");
    CHECK(P(PathAppendA)(full, rel));
    CHECK(!strcmp(full, "C:\\p\\s\\t.txt"));
}

/* ---------------- PathSearchAndQualifyA/W ---------------- */
TEST(gap8_searchandqualify_a_fullpath_input) {
    REQUIRE(P(PathSearchAndQualifyA));
    char o[MAX_PATH];
    CHECK(P(PathSearchAndQualifyA)("C:\\a\\b", o, MAX_PATH) == TRUE);
    CHECK(!strcmp(o, "C:\\a\\b"));
    CHECK(P(PathSearchAndQualifyA)("C:\\a\\.\\b\\..\\c", o, MAX_PATH) == TRUE);
    CHECK(!strcmp(o, "C:\\a\\c"));
    CHECK(P(PathSearchAndQualifyA)("C:/a/b", o, MAX_PATH) == TRUE);
    CHECK(!strcmp(o, "C:\\a\\b"));
}
TEST(gap8_searchandqualify_w_fullpath_input) {
    REQUIRE(P(PathSearchAndQualifyW));
    WCHAR in[64], o[MAX_PATH], want[64];
    widen(in, "C:\\a\\.\\b\\..\\c"); widen(want, "C:\\a\\c");
    CHECK(P(PathSearchAndQualifyW)(in, o, MAX_PATH) == TRUE);
    CHECK(!lstrcmpW(o, want));
}
TEST(gap8_searchandqualify_a_invalid_args) {
    REQUIRE(P(PathSearchAndQualifyA));
    if (t_is_wine()) { t_skip("NULL arguments fault under this emulator"); return; }
    char o[16];
    strcpy(o, "junk");
    CHECK(P(PathSearchAndQualifyA)(NULL, o, 16) == FALSE);
    CHECK(P(PathSearchAndQualifyA)("C:\\a", o, 0) == FALSE);
    CHECK(P(PathSearchAndQualifyA)("C:\\a", NULL, 16) == FALSE);
    CHECK(!strcmp(o, "junk"));         /* rejected before touching the buffer */
}
TEST(gap8_searchandqualify_w_invalid_args) {
    REQUIRE(P(PathSearchAndQualifyW));
    if (t_is_wine()) { t_skip("NULL arguments fault under this emulator"); return; }
    WCHAR o[16] = { 'j','u','n','k',0 }, in[8] = { 'C',':','\\','a',0 };
    CHECK(P(PathSearchAndQualifyW)(NULL, o, 16) == FALSE);
    CHECK(P(PathSearchAndQualifyW)(in, o, 0) == FALSE);
    CHECK(P(PathSearchAndQualifyW)(in, NULL, 16) == FALSE);
    CHECK(o[0] == 'j' && o[4] == 0);
}
TEST(gap8_searchandqualify_a_buffer_sizes) {
    REQUIRE(P(PathSearchAndQualifyA));
    char o[64];
    /* "C:\a\b" is 6 chars: needs 7 */
    memset(o, 'X', sizeof o);
    CHECK(P(PathSearchAndQualifyA)("C:\\a\\b", o, 7) == TRUE);
    CHECK(!strcmp(o, "C:\\a\\b"));
    memset(o, 'X', sizeof o);
    CHECK(P(PathSearchAndQualifyA)("C:\\a\\b", o, 6) == FALSE);
    CHECK(o[0] == '\0');
    memset(o, 'X', sizeof o);
    CHECK(P(PathSearchAndQualifyA)("C:\\a\\b", o, 1) == FALSE);
    CHECK(o[0] == '\0');
    memset(o, 'X', sizeof o);
    CHECK(P(PathSearchAndQualifyA)("C:\\a\\b", o, 2) == FALSE);
    CHECK(o[0] == '\0');
}
TEST(gap8_searchandqualify_w_buffer_sizes) {
    REQUIRE(P(PathSearchAndQualifyW));
    WCHAR in[16], o[64], want[16];
    widen(in, "C:\\a\\b"); widen(want, "C:\\a\\b");
    for (int i = 0; i < 64; i++) o[i] = 'X';
    CHECK(P(PathSearchAndQualifyW)(in, o, 7) == TRUE);
    CHECK(!lstrcmpW(o, want));
    for (int i = 0; i < 64; i++) o[i] = 'X';
    CHECK(P(PathSearchAndQualifyW)(in, o, 6) == FALSE);
    CHECK(o[0] == 0);
    CHECK(P(PathSearchAndQualifyW)(in, o, 1) == FALSE);
    CHECK(o[0] == 0);
}
TEST(gap8_searchandqualify_a_search_system_dll) {
    REQUIRE(P(PathSearchAndQualifyA));
    char o[MAX_PATH], ref[MAX_PATH], sys[MAX_PATH];
    CHECK(P(PathSearchAndQualifyA)("kernel32.dll", o, MAX_PATH) == TRUE);
    CHECK(o[1] == ':' && o[2] == '\\');
    char *fp;
    DWORD n = SearchPathA(NULL, "kernel32.dll", NULL, MAX_PATH, ref, &fp);
    CHECK(n > 0 && n < MAX_PATH);
    CHECK(!lstrcmpiA(o, ref));
    GetSystemDirectoryA(sys, MAX_PATH);
    CHECK(strlen(o) >= strlen(sys));
}
TEST(gap8_searchandqualify_ab_parity_search) {
    REQUIRE(P(PathSearchAndQualifyA)); REQUIRE(P(PathSearchAndQualifyW));
    static const char *ins[] = { "kernel32.dll", "ntdll.dll", "kernel32", "notepad.exe", "k32t_gap8_no_such_file.xyz", "x\\y", "C:\\q\\r", ".\\here", "..\\up", "C:rel" };
    for (size_t i = 0; i < sizeof ins / sizeof *ins; i++) {
        char oa[MAX_PATH]; WCHAR in[MAX_PATH], ow[MAX_PATH]; char back[MAX_PATH];
        widen(in, ins[i]);
        BOOL a = P(PathSearchAndQualifyA)(ins[i], oa, MAX_PATH);
        BOOL w = P(PathSearchAndQualifyW)(in, ow, MAX_PATH);
        CHECKF(a == w, "\"%s\" A=%d W=%d", ins[i], a, w);
        if (a && w) {
            WideCharToMultiByte(CP_ACP, 0, ow, -1, back, MAX_PATH, NULL, NULL);
            CHECKF(!lstrcmpiA(oa, back), "\"%s\" A=\"%s\" W=\"%s\"", ins[i], oa, back);
        }
    }
}
TEST(gap8_searchandqualify_nonexistent_bare_name_is_cwd_relative) {
    REQUIRE(P(PathSearchAndQualifyA));
    char o[MAX_PATH], ref[MAX_PATH], *fp;
    const char *name = "k32t_gap8_does_not_exist_zz.q";
    CHECK(P(PathSearchAndQualifyA)(name, o, MAX_PATH) == TRUE);
    GetFullPathNameA(name, MAX_PATH, ref, &fp);
    CHECK(!lstrcmpiA(o, ref));
}
TEST(gap8_searchandqualify_relative_with_separator_not_searched) {
    REQUIRE(P(PathSearchAndQualifyA));
    char o[MAX_PATH], ref[MAX_PATH], *fp;
    /* names containing a path separator are never looked up on the search path */
    CHECK(P(PathSearchAndQualifyA)("system32\\kernel32.dll", o, MAX_PATH) == TRUE);
    GetFullPathNameA("system32\\kernel32.dll", MAX_PATH, ref, &fp);
    CHECK(!lstrcmpiA(o, ref));
}
TEST(gap8_searchandqualify_empty_string_note) {
    REQUIRE(P(PathSearchAndQualifyA)); REQUIRE(P(PathSearchAndQualifyW));
    char o[MAX_PATH]; WCHAR w[MAX_PATH], e[1] = { 0 };
    BOOL r = P(PathSearchAndQualifyA)("", o, MAX_PATH);
    t_note("SearchAndQualifyA(\"\") -> %d \"%s\"", r, o);
    r = P(PathSearchAndQualifyW)(e, w, MAX_PATH);
    t_note("SearchAndQualifyW(\"\") -> %d len %d", r, lstrlenW(w));
}
TEST(gap8_searchandqualify_long_input_fails_cleanly) {
    REQUIRE(P(PathSearchAndQualifyA));
    static char in[600]; char o[MAX_PATH];
    memset(in, 'a', 599); in[599] = 0; in[0] = 'C'; in[1] = ':'; in[2] = '\\';
    memset(o, 'X', sizeof o);
    CHECK(P(PathSearchAndQualifyA)(in, o, MAX_PATH) == FALSE);
    CHECK(o[0] == '\0');
}

/* ---------------- PathCreateFromUrlA ---------------- */
#define E_INVALID 0x80070057L
TEST(gap8_createfromurl_basic) {
    REQUIRE(P(PathCreateFromUrlA));
    char o[MAX_PATH]; DWORD n = MAX_PATH;
    HRESULT hr = P(PathCreateFromUrlA)("file:///C:/dir/f.txt", o, &n, 0);
    CHECK(hr == S_OK);
    CHECK(!strcmp(o, "C:\\dir\\f.txt"));
    CHECK(n == 12);
}
TEST(gap8_createfromurl_escapes) {
    REQUIRE(P(PathCreateFromUrlA));
    char o[MAX_PATH]; DWORD n = MAX_PATH;
    HRESULT hr = P(PathCreateFromUrlA)("file:///C:/my%20dir/a%20b.txt", o, &n, 0);
    CHECK(hr == S_OK);
    CHECK(!strcmp(o, "C:\\my dir\\a b.txt"));
    n = MAX_PATH;
    hr = P(PathCreateFromUrlA)("file:///C:/a%2Bb", o, &n, 0);
    CHECK(hr == S_OK);
    t_note("%%2B decoded to \"%s\"", o);
}
TEST(gap8_createfromurl_drive_variants) {
    REQUIRE(P(PathCreateFromUrlA));
    static const struct { const char *url, *path; } t[] = {
        { "file:///c:/x", "c:\\x" }, { "file:///D:/a/b/c", "D:\\a\\b\\c" }, { "file:///C:/", "C:\\" },
    };
    for (size_t i = 0; i < sizeof t / sizeof *t; i++) {
        char o[MAX_PATH]; DWORD n = MAX_PATH;
        HRESULT hr = P(PathCreateFromUrlA)(t[i].url, o, &n, 0);
        CHECKF(hr == S_OK, "url=%s hr=0x%lx", t[i].url, (long)hr);
        CHECKF(!strcmp(o, t[i].path), "url=%s got \"%s\" want \"%s\"", t[i].url, o, t[i].path);
    }
}
TEST(gap8_createfromurl_unc_note) {
    REQUIRE(P(PathCreateFromUrlA));
    char o[MAX_PATH]; DWORD n = MAX_PATH;
    HRESULT hr = P(PathCreateFromUrlA)("file://srv/share/x.txt", o, &n, 0);
    t_note("file://srv/share/x.txt -> hr 0x%lx \"%s\"", (long)hr, o);
    n = MAX_PATH;
    hr = P(PathCreateFromUrlA)("file:C:/x", o, &n, 0);
    t_note("file:C:/x -> hr 0x%lx \"%s\"", (long)hr, o);
    n = MAX_PATH;
    hr = P(PathCreateFromUrlA)("file://localhost/C:/x", o, &n, 0);
    t_note("file://localhost/C:/x -> hr 0x%lx \"%s\"", (long)hr, o);
}
TEST(gap8_createfromurl_non_file_scheme) {
    REQUIRE(P(PathCreateFromUrlA));
    static const char *urls[] = { "http://example.com/x", "https://a/b", "ftp://h/f", "mailto:a@b.c" };
    for (size_t i = 0; i < sizeof urls / sizeof *urls; i++) {
        char o[MAX_PATH]; DWORD n = MAX_PATH;
        HRESULT hr = P(PathCreateFromUrlA)(urls[i], o, &n, 0);
        CHECKF(FAILED(hr), "url=%s must fail (hr 0x%lx)", urls[i], (long)hr);
        CHECKF(hr == (HRESULT)E_INVALID, "url=%s hr 0x%lx want E_INVALIDARG", urls[i], (long)hr);
    }
}
TEST(gap8_createfromurl_invalid_args) {
    REQUIRE(P(PathCreateFromUrlA));
    char o[MAX_PATH]; DWORD n = MAX_PATH, zero = 0;
    CHECK(P(PathCreateFromUrlA)(NULL, o, &n, 0) == (HRESULT)E_INVALID);
    CHECK(P(PathCreateFromUrlA)("file:///C:/x", NULL, &n, 0) == (HRESULT)E_INVALID);
    CHECK(P(PathCreateFromUrlA)("file:///C:/x", o, NULL, 0) == (HRESULT)E_INVALID);
    CHECK(P(PathCreateFromUrlA)("file:///C:/x", o, &zero, 0) == (HRESULT)E_INVALID);
    CHECK(zero == 0);
}
TEST(gap8_createfromurl_buffer_boundary) {
    REQUIRE(P(PathCreateFromUrlA));
    char o[MAX_PATH]; DWORD n;
    /* result "C:\dir\f.txt" is 12 chars: needs 13 */
    n = 13; memset(o, 'X', sizeof o);
    HRESULT hr = P(PathCreateFromUrlA)("file:///C:/dir/f.txt", o, &n, 0);
    CHECK(hr == S_OK && n == 12 && !strcmp(o, "C:\\dir\\f.txt"));
    n = 12; memset(o, 'X', sizeof o);
    hr = P(PathCreateFromUrlA)("file:///C:/dir/f.txt", o, &n, 0);
    CHECK(hr == (HRESULT)0x80004003L);      /* E_POINTER */
    CHECK(n == 13);
    n = 1; hr = P(PathCreateFromUrlA)("file:///C:/dir/f.txt", o, &n, 0);
    CHECK(hr == (HRESULT)0x80004003L);
    CHECK(n == 13);
    /* retry with the reported size works */
    n = 13;
    hr = P(PathCreateFromUrlA)("file:///C:/dir/f.txt", o, &n, 0);
    CHECK(hr == S_OK && n == 12);
}
TEST(gap8_createfromurl_repeat_stable) {
    REQUIRE(P(PathCreateFromUrlA));
    for (int i = 0; i < 200; i++) {
        char o[MAX_PATH]; DWORD n = MAX_PATH;
        HRESULT hr = P(PathCreateFromUrlA)("file:///C:/a/b/c.txt", o, &n, 0);
        if (hr != S_OK || strcmp(o, "C:\\a\\b\\c.txt")) { CHECK(0); break; }
    }
    CHECK(1);
}
TEST(gap8_createfromurl_not_a_url) {
    REQUIRE(P(PathCreateFromUrlA));
    static const char *ins[] = { "C:\\x\\y", "", "just text", "//srv/share" };
    for (size_t i = 0; i < sizeof ins / sizeof *ins; i++) {
        char o[MAX_PATH]; DWORD n = MAX_PATH;
        HRESULT hr = P(PathCreateFromUrlA)(ins[i], o, &n, 0);
        t_note("CreateFromUrl(\"%s\") -> hr 0x%lx \"%s\"", ins[i], (long)hr, SUCCEEDED(hr) ? o : "");
    }
}
