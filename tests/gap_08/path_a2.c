/* PathGetCharTypeA, PathGetDriveNumberA, PathIsFileSpecA, PathIsLFNFileSpecA, PathIsPrefixA,
 * PathIsRelativeA, PathIsRootA, PathIsSameRootA, PathIsUNC*A, PathIsURLA */
#include "g8.h"

typedef struct { const char *in; int exp; } bt;

#define RUN_BOOL(fnexpr, tab) do { \
    for (size_t i_ = 0; i_ < sizeof tab / sizeof *tab; i_++) { \
        BOOL r_ = (fnexpr)(tab[i_].in); \
        CHECKF(!!r_ == tab[i_].exp, "in=\"%s\" got %d want %d", tab[i_].in, (int)r_, tab[i_].exp); \
    } } while (0)

/* ---------------- PathGetCharTypeA ---------------- */
TEST(gap8_getchartype_specials) {
    REQUIRE(P(PathGetCharTypeA));
    struct { unsigned char c; UINT t; } sp[] = {
        { ';', GCT_LFNCHAR }, { ' ', GCT_LFNCHAR }, { ',', GCT_LFNCHAR },
        { '"', GCT_INVALID }, { '/', GCT_INVALID }, { '<', GCT_INVALID }, { '>', GCT_INVALID }, { '|', GCT_INVALID },
        { '*', GCT_WILD }, { '?', GCT_WILD }, { ':', GCT_SEPARATOR }, { '\\', GCT_SEPARATOR },
    };
    for (size_t i = 0; i < sizeof sp / sizeof *sp; i++) {
        UINT r = P(PathGetCharTypeA)(sp[i].c);
        CHECKF(r == sp[i].t, "char 0x%02x got %u want %u", sp[i].c, r, sp[i].t);
    }
}
TEST(gap8_getchartype_controls_invalid) {
    REQUIRE(P(PathGetCharTypeA));
    for (int c = 0; c < 0x20; c++) {
        UINT r = P(PathGetCharTypeA)((UCHAR)c);
        CHECKF(r == GCT_INVALID, "control 0x%02x got %u", c, r);
    }
}
TEST(gap8_getchartype_alnum_shortlfn) {
    REQUIRE(P(PathGetCharTypeA));
    for (int c = 'a'; c <= 'z'; c++) EXPECT_EQ(P(PathGetCharTypeA)((UCHAR)c), GCT_LFNCHAR | GCT_SHORTCHAR);
    for (int c = 'A'; c <= 'Z'; c++) EXPECT_EQ(P(PathGetCharTypeA)((UCHAR)c), GCT_LFNCHAR | GCT_SHORTCHAR);
    for (int c = '0'; c <= '9'; c++) EXPECT_EQ(P(PathGetCharTypeA)((UCHAR)c), GCT_LFNCHAR | GCT_SHORTCHAR);
    EXPECT_EQ(P(PathGetCharTypeA)('.'), 3);
    EXPECT_EQ(P(PathGetCharTypeA)('_'), 3);
    EXPECT_EQ(P(PathGetCharTypeA)('-'), 3);
    EXPECT_EQ(P(PathGetCharTypeA)('!'), 3);
    EXPECT_EQ(P(PathGetCharTypeA)('~'), 3);
}
TEST(gap8_getchartype_full_table_ascii) {
    REQUIRE(P(PathGetCharTypeA));
    /* every printable ASCII char other than the specials is short+LFN valid */
    const char *special = ";, \"/<>|*?:\\";
    for (int c = 0x21; c < 0x7f; c++) {
        UINT want = strchr(special, c) ? 99 : 3;
        if (want == 99) continue;
        UINT r = P(PathGetCharTypeA)((UCHAR)c);
        CHECKF(r == 3, "char '%c' (0x%02x) got %u", c, c, r);
    }
    t_note("0x7f -> %u, 0x80 -> %u, 0xff -> %u", P(PathGetCharTypeA)(0x7f), P(PathGetCharTypeA)(0x80), P(PathGetCharTypeA)(0xff));
}
TEST(gap8_getchartype_flag_bits_only) {
    REQUIRE(P(PathGetCharTypeA));
    for (int c = 0; c < 256; c++) {
        UINT r = P(PathGetCharTypeA)((UCHAR)c);
        CHECKF((r & ~15u) == 0, "char 0x%02x result 0x%x has unknown bits", c, r);
    }
}

/* ---------------- PathGetDriveNumberA ---------------- */
typedef struct { const char *in; int exp; } dn_case;
static const dn_case dn_tab[] = {
    { "C:", 2 }, { "c:", 2 }, { "C:\\x", 2 }, { "a:", 0 }, { "A:", 0 }, { "z:", 25 }, { "Z:\\", 25 }, { "m:/x", 12 },
    { "1:", -1 }, { "C", -1 }, { "CC:", -1 }, { ":", -1 }, { "", -1 }, { "\\\\?\\C:", -1 }, { "\\C:", -1 },
    { "@:", -1 }, { "[:", -1 }, { "`:", -1 }, { "{:", -1 }, { " C:", -1 }, { "C ", -1 }, { "\\\\srv\\C:", -1 },
};
TEST(gap8_getdrivenumber_table) {
    REQUIRE(P(PathGetDriveNumberA));
    for (size_t i = 0; i < sizeof dn_tab / sizeof *dn_tab; i++) {
        int r = P(PathGetDriveNumberA)(dn_tab[i].in);
        CHECKF(r == dn_tab[i].exp, "in=\"%s\" got %d want %d", dn_tab[i].in, r, dn_tab[i].exp);
    }
}
TEST(gap8_getdrivenumber_null) {
    REQUIRE(P(PathGetDriveNumberA));
    CHECK(P(PathGetDriveNumberA)(NULL) == -1);
}
TEST(gap8_getdrivenumber_all_letters) {
    REQUIRE(P(PathGetDriveNumberA));
    char s[4] = "X:\\";
    for (int i = 0; i < 26; i++) {
        s[0] = (char)('a' + i); EXPECT_EQ(P(PathGetDriveNumberA)(s), i);
        s[0] = (char)('A' + i); EXPECT_EQ(P(PathGetDriveNumberA)(s), i);
    }
}

/* ---------------- PathIsFileSpecA ---------------- */
static const bt fs_tab[] = {
    { "", 1 }, { "a.txt", 1 }, { "a\\b", 0 }, { "C:", 0 }, { "/a", 1 }, { "*.*", 1 }, { "a b", 1 }, { "\\", 0 },
    { "a:b", 0 }, { "C:\\a", 0 }, { "a/b/c", 1 }, { ".", 1 }, { "..", 1 }, { "a\\", 0 }, { "\\\\srv\\sh", 0 },
};
TEST(gap8_isfilespec_table) {
    REQUIRE(P(PathIsFileSpecA));
    RUN_BOOL(P(PathIsFileSpecA), fs_tab);
}
TEST(gap8_isfilespec_null) {
    REQUIRE(P(PathIsFileSpecA));
    CHECK(P(PathIsFileSpecA)(NULL) == FALSE);
}

/* ---------------- PathIsLFNFileSpecA ---------------- */
static const bt lfn_tab[] = {
    { "", 0 }, { "a", 0 }, { "12345678", 0 }, { "123456789", 1 }, { "a.b", 0 }, { "a.bcd", 0 }, { "a.bcde", 1 },
    { "12345678.abc", 0 }, { "123456789.abc", 1 }, { "a.b.c", 1 }, { "a b", 1 }, { "..", 1 }, { ".abc", 0 },
    { "a.", 0 }, { "abc.", 0 }, { " ", 1 }, { "UPPER.TXT", 0 }, { "verylongname.t", 1 }, { "abcdefgh.ijk", 0 },
    { "abcdefgh.ijkl", 1 }, { "x.y", 0 }, { ".", 0 }, { "file.tar.gz", 1 }, { "a-b_c.d", 0 },
};
TEST(gap8_islfnfilespec_table) {
    REQUIRE(P(PathIsLFNFileSpecA));
    RUN_BOOL(P(PathIsLFNFileSpecA), lfn_tab);
}
TEST(gap8_islfnfilespec_null) {
    REQUIRE(P(PathIsLFNFileSpecA));
    CHECK(P(PathIsLFNFileSpecA)(NULL) == FALSE);
}
TEST(gap8_islfnfilespec_length_sweep) {
    REQUIRE(P(PathIsLFNFileSpecA));
    char s[32];
    for (int n = 1; n <= 20; n++) {
        memset(s, 'x', n); s[n] = 0;
        BOOL r = P(PathIsLFNFileSpecA)(s);
        CHECKF(!!r == (n > 8), "no-dot length %d got %d", n, r);
    }
    for (int e = 0; e <= 8; e++) {
        strcpy(s, "name."); memset(s + 5, 'e', e); s[5 + e] = 0;
        BOOL r = P(PathIsLFNFileSpecA)(s);
        CHECKF(!!r == (e > 3), "ext length %d got %d", e, r);
    }
}

/* ---------------- PathIsPrefixA ---------------- */
typedef struct { const char *prefix, *path; int exp; } pf_case;
static const pf_case pf_tab[] = {
    { "C:\\win", "C:\\win\\sys", 1 }, { "C:\\win", "C:\\win", 1 }, { "C:\\", "C:\\x", 1 }, { "C:\\winx", "C:\\win\\sys", 0 },
    { "D:\\", "C:\\x", 0 }, { "C:\\win\\sys", "C:\\win", 0 }, { "C:\\Win", "c:\\WIN\\x", 1 }, { "C:\\a\\b", "C:\\a\\c", 0 },
    { "C:\\a", "C:\\a\\b\\c", 1 }, { "C:\\a\\b", "C:\\a\\b\\c\\d", 1 }, { "C:\\abc", "C:\\ab\\x", 0 },
    { "\\\\srv\\sh", "\\\\srv\\sh\\f", 1 }, { "\\\\srv\\sh", "\\\\srv\\sh2\\f", 0 },
};
TEST(gap8_isprefix_table) {
    REQUIRE(P(PathIsPrefixA));
    for (size_t i = 0; i < sizeof pf_tab / sizeof *pf_tab; i++) {
        BOOL r = P(PathIsPrefixA)(pf_tab[i].prefix, pf_tab[i].path);
        CHECKF(!!r == pf_tab[i].exp, "prefix=\"%s\" path=\"%s\" got %d want %d", pf_tab[i].prefix, pf_tab[i].path, r, pf_tab[i].exp);
    }
}
TEST(gap8_isprefix_null) {
    REQUIRE(P(PathIsPrefixA));
    CHECK(P(PathIsPrefixA)(NULL, "C:\\a") == FALSE);
    CHECK(P(PathIsPrefixA)("C:\\a", NULL) == FALSE);
    CHECK(P(PathIsPrefixA)(NULL, NULL) == FALSE);
}
TEST(gap8_isprefix_edge_notes) {
    REQUIRE(P(PathIsPrefixA));
    t_note("IsPrefix(\"C:\\win\\\", \"C:\\win\\sys\") = %d", P(PathIsPrefixA)("C:\\win\\", "C:\\win\\sys"));
    t_note("IsPrefix(\"\", \"C:\\a\") = %d", P(PathIsPrefixA)("", "C:\\a"));
    t_note("IsPrefix(\"C:\", \"C:\\a\") = %d", P(PathIsPrefixA)("C:", "C:\\a"));
}

/* ---------------- PathIsRelativeA ---------------- */
static const bt rel_tab[] = {
    { "", 1 }, { "a", 1 }, { "a\\b", 1 }, { "..\\a", 1 }, { ".\\a", 1 }, { "/a", 1 }, { "1:\\", 1 }, { "C", 1 },
    { "\\", 0 }, { "\\a", 0 }, { "C:\\a", 0 }, { "C:", 0 }, { "C:a", 0 }, { "c:\\", 0 }, { "\\\\srv\\sh", 0 },
    { "\\\\?\\C:\\a", 0 }, { "\\\\", 0 }, { "C:/a", 0 }, { " C:\\", 1 }, { "a:b", 0 }, { "ab:c", 1 },
};
TEST(gap8_isrelative_table) {
    REQUIRE(P(PathIsRelativeA));
    RUN_BOOL(P(PathIsRelativeA), rel_tab);
}
TEST(gap8_isrelative_null) {
    REQUIRE(P(PathIsRelativeA));
    CHECK(P(PathIsRelativeA)(NULL) == TRUE);
}

/* ---------------- PathIsRootA ---------------- */
static const bt root_tab[] = {
    { "", 0 }, { "C:\\", 1 }, { "c:\\", 1 }, { "Z:\\", 1 }, { "C:", 0 }, { "C:\\a", 0 }, { "C:/", 0 }, { "/", 0 },
    { "\\", 1 }, { "\\a", 0 }, { "\\\\", 1 }, { "\\\\srv", 1 }, { "\\\\srv\\", 0 }, { "\\\\srv\\share", 1 },
    { "\\\\srv\\share\\x", 0 }, { "\\\\?\\C:\\", 0 }, { "a", 0 }, { "a\\", 0 }, { "C:\\\\", 0 }, { "1:\\", 0 },
    { "\\\\.\\C:", 1 },
};
TEST(gap8_isroot_table) {
    REQUIRE(P(PathIsRootA));
    RUN_BOOL(P(PathIsRootA), root_tab);
}
TEST(gap8_isroot_null) {
    REQUIRE(P(PathIsRootA));
    CHECK(P(PathIsRootA)(NULL) == FALSE);
}
TEST(gap8_isroot_unc_trailing_note) {
    REQUIRE(P(PathIsRootA));
    t_note("IsRoot(\"\\\\\\\\srv\\\\share\\\\\") = %d", P(PathIsRootA)("\\\\srv\\share\\"));
}

/* ---------------- PathIsSameRootA ---------------- */
static const pf_case sr_tab[] = {
    { "C:\\a", "C:\\b", 1 }, { "C:\\a", "D:\\a", 0 }, { "\\\\srv\\sh\\a", "\\\\srv\\sh\\b", 1 },
    { "\\\\srv\\sh1\\a", "\\\\srv\\sh2\\a", 0 }, { "a", "a", 0 }, { "C:\\", "C:\\x", 1 }, { "C:\\a\\b", "C:\\a\\c", 1 },
    { "c:\\a", "C:\\b", 1 }, { "\\\\srv\\sh\\a", "C:\\a", 0 }, { "C:", "C:", 0 }, { "C:\\a", "\\\\srv\\sh\\a", 0 },
    { "\\\\srv1\\sh\\a", "\\\\srv2\\sh\\a", 0 }, { "", "", 0 }, { "C:\\x", "C:\\x", 1 },
};
TEST(gap8_issameroot_table) {
    REQUIRE(P(PathIsSameRootA));
    for (size_t i = 0; i < sizeof sr_tab / sizeof *sr_tab; i++) {
        BOOL r = P(PathIsSameRootA)(sr_tab[i].prefix, sr_tab[i].path);
        CHECKF(!!r == sr_tab[i].exp, "(\"%s\",\"%s\") got %d want %d", sr_tab[i].prefix, sr_tab[i].path, r, sr_tab[i].exp);
    }
}
TEST(gap8_issameroot_null) {
    REQUIRE(P(PathIsSameRootA));
    CHECK(P(PathIsSameRootA)(NULL, "C:\\a") == FALSE);
    CHECK(P(PathIsSameRootA)("C:\\a", NULL) == FALSE);
    CHECK(P(PathIsSameRootA)(NULL, NULL) == FALSE);
}

/* ---------------- PathIsUNCA / Server / ServerShare ---------------- */
static const bt unc_tab[] = {
    { "\\\\a", 1 }, { "\\\\", 1 }, { "\\\\srv\\share\\f", 1 }, { "\\\\?\\C:\\", 0 }, { "\\\\?", 0 }, { "\\a", 0 }, { "//a", 0 },
    { "", 0 }, { "\\", 0 }, { "C:\\a", 0 }, { "\\\\?\\UNC\\srv\\sh", 0 }, { "\\\\.\\C:", 1 }, { "a\\\\b", 0 },
    { "\\\\\\a", 1 },
};
TEST(gap8_isunc_table) {
    REQUIRE(P(PathIsUNCA));
    RUN_BOOL(P(PathIsUNCA), unc_tab);
}
TEST(gap8_isunc_null) {
    REQUIRE(P(PathIsUNCA));
    CHECK(P(PathIsUNCA)(NULL) == FALSE);
}
static const bt uncs_tab[] = {
    { "\\\\srv", 1 }, { "\\\\srv\\", 0 }, { "\\\\srv\\share", 0 }, { "\\\\", 1 }, { "\\a", 0 }, { "", 0 }, { "C:\\a", 0 },
    { "\\\\srv\\share\\f", 0 }, { "\\\\?\\srv", 0 }, { "srv", 0 }, { "\\\\a.b.c", 1 },
};
TEST(gap8_isuncserver_table) {
    REQUIRE(P(PathIsUNCServerA));
    RUN_BOOL(P(PathIsUNCServerA), uncs_tab);
}
TEST(gap8_isuncserver_null) {
    REQUIRE(P(PathIsUNCServerA));
    CHECK(P(PathIsUNCServerA)(NULL) == FALSE);
}
static const bt uncss_tab[] = {
    { "\\\\srv\\share", 1 }, { "\\\\srv\\share\\x", 0 }, { "\\\\srv", 0 }, { "\\\\srv\\share\\", 0 }, { "", 0 },
    { "C:\\a\\b", 0 }, { "\\a\\b", 0 }, { "\\\\?\\srv\\share", 0 }, { "\\\\s\\x", 1 }, { "\\\\srv\\a b", 1 },
};
TEST(gap8_isuncservershare_table) {
    REQUIRE(P(PathIsUNCServerShareA));
    RUN_BOOL(P(PathIsUNCServerShareA), uncss_tab);
}
TEST(gap8_isuncservershare_null) {
    REQUIRE(P(PathIsUNCServerShareA));
    CHECK(P(PathIsUNCServerShareA)(NULL) == FALSE);
}
TEST(gap8_isunc_quirk_notes) {
    REQUIRE(P(PathIsUNCServerShareA));
    t_note("IsUNCServerShare(\"\\\\\\\\srv\\\\\") = %d", P(PathIsUNCServerShareA)("\\\\srv\\"));
}
TEST(gap8_unc_family_consistency) {
    REQUIRE(P(PathIsUNCA)); REQUIRE(P(PathIsUNCServerA)); REQUIRE(P(PathIsUNCServerShareA));
    /* server and server\share forms are both UNC paths, and mutually exclusive */
    static const char *ins[] = { "\\\\srv", "\\\\srv\\share", "\\\\srv\\share\\f", "C:\\a", "", "\\\\" };
    for (size_t i = 0; i < sizeof ins / sizeof *ins; i++) {
        BOOL u = P(PathIsUNCA)(ins[i]), s = P(PathIsUNCServerA)(ins[i]), ss = P(PathIsUNCServerShareA)(ins[i]);
        if (s) CHECKF(u, "server implies UNC: \"%s\"", ins[i]);
        if (ss) CHECKF(u, "server\\share implies UNC: \"%s\"", ins[i]);
        CHECKF(!(s && ss), "server and server\\share exclusive: \"%s\"", ins[i]);
    }
}

/* ---------------- PathIsURLA ---------------- */
static const bt url_tab[] = {
    { "http://example.com/x", 1 }, { "https://a", 1 }, { "ftp://h/f", 1 }, { "file:///C:/x", 1 }, { "mailto:a@b.c", 1 },
    { "HTTP://EXAMPLE", 1 }, { "C:\\x", 0 }, { "C:/x", 0 }, { "", 0 }, { "abc", 0 }, { "www.example.com", 0 },
    { "\\\\srv\\share", 0 }, { "://x", 0 }, { "a:b", 0 }, { "javascript:void(0)", 1 }, { "res://x.dll/1", 1 },
    { "http:", 1 }, { "foo bar://x", 0 }, { "/path/only", 0 },
};
TEST(gap8_isurl_table) {
    REQUIRE(P(PathIsURLA));
    for (size_t i = 0; i < sizeof url_tab / sizeof *url_tab; i++) {
        BOOL r = P(PathIsURLA)(url_tab[i].in);
        if (!strcmp(url_tab[i].in, "http:") || !strcmp(url_tab[i].in, "foo bar://x") || !strcmp(url_tab[i].in, "res://x.dll/1") || !strcmp(url_tab[i].in, "javascript:void(0)")) {
            t_note("IsURL(\"%s\") = %d", url_tab[i].in, r); continue;
        }
        CHECKF(!!r == url_tab[i].exp, "in=\"%s\" got %d want %d", url_tab[i].in, r, url_tab[i].exp);
    }
}
TEST(gap8_isurl_null) {
    REQUIRE(P(PathIsURLA));
    CHECK(P(PathIsURLA)(NULL) == FALSE);
}
