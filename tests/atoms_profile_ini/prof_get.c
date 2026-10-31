/* GetPrivateProfileStringA/W: lookup, parsing rules, buffer and return-value semantics. */
#include "ap.h"

#define BASE "[section1]\r\nname1=val1\r\nname2=\"val2\"\r\nname3\r\nname4=a\r\n[section2]\r\n"

static char *mkbase(char *path, const char *leaf) { ap_ini(path, leaf, BASE); return path; }

/* ---- table-driven value parsing ---- */
typedef struct { const char *content; const char *key; const char *want; int hard; } vp_case;
static const vp_case vp[] = {
    { "[s]\r\nk=v\r\n", "k", "v", 1 },
    { "[s]\r\nk= v\r\n", "k", "v", 1 },
    { "[s]\r\nk=v \r\n", "k", "v", 1 },
    { "[s]\r\nk=   v   \r\n", "k", "v", 1 },
    { "[s]\r\nk =v\r\n", "k", "v", 1 },
    { "[s]\r\nk   =   v\r\n", "k", "v", 1 },
    { "[s]\r\nk=two words\r\n", "k", "two words", 1 },
    { "[s]\r\nk=two  inner  spaces\r\n", "k", "two  inner  spaces", 1 },
    { "[s]\r\nk=\"v\"\r\n", "k", "v", 1 },
    { "[s]\r\nk=\"  v  \"\r\n", "k", "  v  ", 0 },
    { "[s]\r\nk=  \"v\"  \r\n", "k", "v", 0 },
    { "[s]\r\nk='v'\r\n", "k", "v", 0 },
    { "[s]\r\nk=\"v\r\n", "k", "\"v", 0 },
    { "[s]\r\nk=v\"\r\n", "k", "v\"", 0 },
    { "[s]\r\nk=\"\"\r\n", "k", "", 0 },
    { "[s]\r\nk=a=b\r\n", "k", "a=b", 1 },
    { "[s]\r\nk==x\r\n", "k", "=x", 1 },
    { "[s]\r\nk=a;b\r\n", "k", "a;b", 1 },
    { "[s]\r\nk=a ; b\r\n", "k", "a ; b", 1 },
    { "[s]\r\nk=;x\r\n", "k", ";x", 0 },
    { "[s]\r\nk=\r\n", "k", "", 1 },
    { "[s]\r\nk=   \r\n", "k", "", 1 },
    { "[s]\r\nk\r\n", "k", "DEF", 1 },
    { "[s]\r\n; k=v\r\n", "k", "DEF", 1 },
    { "[s]\r\n;k=v\r\n", "k", "DEF", 1 },
    { "[s]\r\n;k=v\r\nk=real\r\n", "k", "real", 1 },
    { "[s]\r\nk=v\tx\r\n", "k", "v\tx", 1 },
    { "[s]\r\nK=V\r\n", "k", "V", 1 },
    { "[s]\r\nk=V\r\n", "K", "V", 1 },
    { "[s]\r\nMixedKey=ok\r\n", "mIXEDkEY", "ok", 1 },
    { "[s]\r\nk=1\r\nk=2\r\n", "k", "1", 1 },
    { "[s]\r\nk=1\r\nK=2\r\n", "k", "1", 1 },
    { "[s]\r\nkey=1\r\nkey2=2\r\n", "key2", "2", 1 },
    { "[s]\r\nkey2=2\r\nkey=1\r\n", "key", "1", 1 },
    { "[s]\r\nkk=1\r\n", "k", "DEF", 1 },
    { "[s]\r\nk=1\r\n", "kk", "DEF", 1 },
    { "[s]\r\nk.with.dots=v\r\n", "k.with.dots", "v", 1 },
    { "[s]\r\nkey with spaces=v\r\n", "key with spaces", "v", 1 },
    { "[s]\r\nkey with spaces =v\r\n", "key with spaces", "v", 1 },
    { "[s]\r\n[x]k=v\r\n", "k", "DEF", 0 },
    { "[s]\r\nk=v\r\n[t]\r\nk=w\r\n", "k", "v", 1 },
    { "[s]\r\nk=\xe9\r\n", "k", "\xe9", 1 },
    { "[s]\r\nk=caf\xe9 au lait\r\n", "k", "caf\xe9 au lait", 1 },
    { "[s]\nk=lf\n", "k", "lf", 1 },
    { "[s]\rk=cr\r", "k", "cr", 1 },
    { "[s]\r\nk=nonl", "k", "nonl", 1 },
    { "[s]\r\nk=v\n", "k", "v", 1 },
    { "[s]\r\n\r\n\r\nk=v\r\n", "k", "v", 1 },
    { "\r\n\r\n[s]\r\nk=v\r\n", "k", "v", 1 },
    { "junk line\r\n[s]\r\nk=v\r\n", "k", "v", 1 },
    { "[s]\r\n  k=v\r\n", "k", "v", 0 },
    { "[s]\r\n\tk=v\r\n", "k", "v", 0 },
    { "[s]\r\nk=\tv\t\r\n", "k", "v", 0 },
    { "[s]\r\nk=0x10\r\n", "k", "0x10", 1 },
    { "[s]\r\nk=-5\r\n", "k", "-5", 1 },
    { "[s]\r\nk=%PATH%\r\n", "k", "%PATH%", 1 },
    { "[s]\r\nk=C:\\dir\\file.txt\r\n", "k", "C:\\dir\\file.txt", 1 },
    { "[s]\r\nk=[bracket]\r\n", "k", "[bracket]", 1 },
    { "[s]\r\nk=a\"b\"c\r\n", "k", "a\"b\"c", 1 },
};

TEST(profile_get_value_parsing_table_a) {
    REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH];
    for (unsigned i = 0; i < sizeof vp / sizeof *vp; i++) {
        ap_ini(path, "vp_a.ini", vp[i].content);
        char buf[256]; memset(buf, 'z', sizeof buf);
        DWORD r = U(GetPrivateProfileStringA)("s", vp[i].key, "DEF", buf, sizeof buf, path);
        int ok = !strcmp(buf, vp[i].want) && r == strlen(vp[i].want);
        if (vp[i].hard) CHECKF(ok, "case %u: got \"%s\"(%lu) want \"%s\"", i, buf, r, vp[i].want);
        else if (!ok) t_note("case %u (soft): got \"%s\"(%lu) want \"%s\"", i, buf, r, vp[i].want);
    }
    ap_rm(path);
}

TEST(profile_get_value_parsing_table_w) {
    REQUIRE(U(GetPrivateProfileStringW));
    char path[MAX_PATH]; WCHAR wp[MAX_PATH], wk[64], wd[8], buf[256], want[256];
    ap_widen(wd, "DEF");
    for (unsigned i = 0; i < sizeof vp / sizeof *vp; i++) {
        ap_ini(path, "vp_w.ini", vp[i].content);
        ap_wpath(wp, path); ap_widen(wk, vp[i].key);
        for (int k = 0; k < 256; k++) buf[k] = L'z';
        DWORD r = U(GetPrivateProfileStringW)(L"s", wk, wd, buf, 256, wp);
        MultiByteToWideChar(CP_ACP, 0, vp[i].want, -1, want, 256);
        int ok = !lstrcmpW(buf, want) && r == (DWORD)ap_wlen(want);
        if (vp[i].hard) CHECKF(ok, "case %u: ret %lu", i, r);
        else if (!ok) t_note("case %u (soft W): ret %lu", i, r);
    }
    ap_rm(path);
}

/* ---- default value handling ---- */
typedef struct { const char *def; const char *want; } df_case;
static const df_case dfs[] = {
    { "default", "default" }, { "", "" }, { "x", "x" }, { "default  ", "default" }, { "one two  ", "one two" },
    { "one two", "one two" }, { "  lead", "  lead" }, { "a b c   ", "a b c" },
    { "tab\t", "tab\t" }, { "[sec]", "[sec]" }, { "k=v", "k=v" },
};
TEST(profile_get_default_value_table) {
    REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH]; mkbase(path, "def.ini");
    for (unsigned i = 0; i < sizeof dfs / sizeof *dfs; i++) {
        char buf[64], def[64]; strcpy(def, dfs[i].def);   /* writable copy */
        memset(buf, 'z', sizeof buf);
        DWORD r = U(GetPrivateProfileStringA)("section1", "nokey", def, buf, sizeof buf, path);
        CHECKF(!strcmp(buf, dfs[i].want) && r == strlen(dfs[i].want), "def \"%s\": got \"%s\"(%lu)", dfs[i].def, buf, r);
        memset(buf, 'z', sizeof buf);
        r = U(GetPrivateProfileStringA)("nosection", "name1", def, buf, sizeof buf, path);
        CHECKF(!strcmp(buf, dfs[i].want) && r == strlen(dfs[i].want), "def(sec) \"%s\": got \"%s\"(%lu)", dfs[i].def, buf, r);
    }
    ap_rm(path);
}

TEST(profile_get_default_null_is_empty) {
    REQUIRE(U(GetPrivateProfileStringA)); REQUIRE(U(GetPrivateProfileStringW));
    char path[MAX_PATH]; mkbase(path, "defnull.ini");
    char buf[32]; memset(buf, 'z', sizeof buf);
    EXPECT_EQ(U(GetPrivateProfileStringA)("section1", "nokey", NULL, buf, sizeof buf, path), 0);
    EXPECT_STR(buf, "");
    WCHAR wb[32], wp[MAX_PATH]; for (int i = 0; i < 32; i++) wb[i] = L'z';
    ap_wpath(wp, path);
    EXPECT_EQ(U(GetPrivateProfileStringW)(L"section1", L"nokey", NULL, wb, 32, wp), 0);
    EXPECT_EQ(wb[0], 0);
    ap_rm(path);
}

TEST(profile_get_default_not_used_when_found) {
    REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH]; mkbase(path, "found.ini"); char b[32];
    EXPECT_EQ(ap_gps("section1", "name1", "default", b, sizeof b, path), 4);
    EXPECT_STR(b, "val1");
    EXPECT_EQ(ap_gps("section1", "name2", "default", b, sizeof b, path), 4);
    EXPECT_STR(b, "val2");
    EXPECT_EQ(ap_gps("section1", "name4", "default", b, sizeof b, path), 1);
    EXPECT_STR(b, "a");
    EXPECT_EQ(ap_gps("SECTION1", "NAME1", "default", b, sizeof b, path), 4);
    EXPECT_EQ(ap_gps("SeCtIoN1", "nAmE1", "default", b, sizeof b, path), 4);
    ap_rm(path);
}

TEST(profile_get_key_without_equals_is_default) {
    REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH]; mkbase(path, "noeq.ini"); char b[32];
    EXPECT_EQ(ap_gps("section1", "name3", "default", b, sizeof b, path), 7);
    EXPECT_STR(b, "default");
    ap_rm(path);
}

TEST(profile_get_missing_section_and_key) {
    REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH]; mkbase(path, "missing.ini"); char b[32];
    EXPECT_EQ(ap_gps("notasection", "name1", "default", b, sizeof b, path), 7);
    EXPECT_STR(b, "default");
    EXPECT_EQ(ap_gps("section1", "notakey", "default", b, sizeof b, path), 7);
    EXPECT_STR(b, "default");
    EXPECT_EQ(ap_gps("section2", "DoesntExist", "", b, sizeof b, path), 0);
    EXPECT_STR(b, "");
    /* key in the wrong section */
    EXPECT_EQ(ap_gps("section2", "name1", "d", b, sizeof b, path), 1);
    ap_rm(path);
}

TEST(profile_get_missing_file_returns_default) {
    REQUIRE(U(GetPrivateProfileStringA)); REQUIRE(U(GetPrivateProfileStringW));
    char path[MAX_PATH]; t_tmpfile(path, sizeof path, "no_such_file.ini"); ap_rm(path);
    char b[32]; memset(b, 'z', sizeof b);
    SET_ERR(0xdeadbeef);
    EXPECT_EQ(ap_gps("s", "k", "default", b, sizeof b, path), 7);
    EXPECT_STR(b, "default");
    t_note("missing file: last error %lu", GetLastError());
    WCHAR wp[MAX_PATH], wb[32]; ap_wpath(wp, path);
    EXPECT_EQ(U(GetPrivateProfileStringW)(L"s", L"k", L"default", wb, 32, wp), 7);
    CHECK(!lstrcmpW(wb, L"default"));
    EXPECT_EQ(ap_exists(path), 0);       /* reads never create the file */
}

TEST(profile_get_missing_dir_returns_default) {
    REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH]; t_tmpfile(path, sizeof path, "nodir\\sub\\x.ini"); char b[32];
    EXPECT_EQ(ap_gps("s", "k", "default", b, sizeof b, path), 7);
    EXPECT_STR(b, "default");
}

TEST(profile_get_filename_null_and_empty) {
    REQUIRE(U(GetPrivateProfileStringA));
    char b[32];
    /* NULL name means win.ini; a nonexistent section just yields the default */
    memset(b, 'z', sizeof b);
    EXPECT_EQ(ap_gps("k32t_no_such_section_ever", "name1", "default", b, sizeof b, NULL), 7);
    EXPECT_STR(b, "default");
    memset(b, 'z', sizeof b);
    EXPECT_EQ(ap_gps("section1", "name1", "default", b, sizeof b, ""), 7);
    EXPECT_STR(b, "default");
    memset(b, 'z', sizeof b);
    EXPECT_EQ(ap_gps("section1", "name1", "default", b, sizeof b, "k32t_nonexistent_name.ini"), 7);
    EXPECT_STR(b, "default");
}

TEST(profile_get_directory_as_file_is_default) {
    REQUIRE(U(GetPrivateProfileStringA));
    char b[32];
    EXPECT_EQ(ap_gps("s", "k", "default", b, sizeof b, t_tmpdir()), 7);
    EXPECT_STR(b, "default");
}

TEST(profile_get_buffer_exact_and_truncated) {
    REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH]; mkbase(path, "buf.ini"); char b[32];
    memset(b, 'z', sizeof b); EXPECT_EQ(ap_gps("section1", "name1", "default", b, 0, path), 0);
    EXPECT_EQ(b[0], 'z');
    memset(b, 'z', sizeof b); EXPECT_EQ(ap_gps("section1", "name1", "default", b, 1, path), 0);
    EXPECT_EQ(b[0], 0); EXPECT_EQ(b[1], 'z');
    memset(b, 'z', sizeof b); EXPECT_EQ(ap_gps("section1", "name1", "default", b, 2, path), 1);
    EXPECT_STR(b, "v"); EXPECT_EQ(b[2], 'z');
    memset(b, 'z', sizeof b); EXPECT_EQ(ap_gps("section1", "name1", "default", b, 3, path), 2);
    EXPECT_STR(b, "va");
    memset(b, 'z', sizeof b); EXPECT_EQ(ap_gps("section1", "name1", "default", b, 4, path), 3);
    EXPECT_STR(b, "val"); EXPECT_EQ(b[4], 'z');
    memset(b, 'z', sizeof b); EXPECT_EQ(ap_gps("section1", "name1", "default", b, 5, path), 4);
    EXPECT_STR(b, "val1"); EXPECT_EQ(b[5], 'z');
    memset(b, 'z', sizeof b); EXPECT_EQ(ap_gps("section1", "name1", "default", b, 6, path), 4);
    memset(b, 'z', sizeof b); EXPECT_EQ(ap_gps("section1", "name4", "default", b, 1, path), 0);
    EXPECT_STR(b, "");
    memset(b, 'z', sizeof b); EXPECT_EQ(ap_gps("section1", "name4", "default", b, 2, path), 1);
    EXPECT_STR(b, "a");
    ap_rm(path);
}

TEST(profile_get_default_truncated_to_buffer) {
    REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH]; mkbase(path, "deftrunc.ini"); char b[32];
    memset(b, 'z', sizeof b); EXPECT_EQ(ap_gps("section1", "nokey", "default", b, 0, path), 0);
    EXPECT_EQ(b[0], 'z');
    memset(b, 'z', sizeof b); EXPECT_EQ(ap_gps("section1", "nokey", "default", b, 1, path), 0);
    EXPECT_STR(b, "");
    memset(b, 'z', sizeof b); EXPECT_EQ(ap_gps("section1", "nokey", "default", b, 4, path), 3);
    EXPECT_STR(b, "def");
    memset(b, 'z', sizeof b); EXPECT_EQ(ap_gps("section1", "nokey", "default", b, 7, path), 6);
    EXPECT_STR(b, "defaul");
    memset(b, 'z', sizeof b); EXPECT_EQ(ap_gps("section1", "nokey", "default", b, 8, path), 7);
    EXPECT_STR(b, "default");
    ap_rm(path);
}

TEST(profile_get_buffer_exact_w) {
    REQUIRE(U(GetPrivateProfileStringW));
    char path[MAX_PATH]; mkbase(path, "bufw.ini"); WCHAR wp[MAX_PATH], b[16]; ap_wpath(wp, path);
    for (DWORD n = 0; n <= 6; n++) {
        for (int i = 0; i < 16; i++) b[i] = L'z';
        DWORD want = n == 0 ? 0 : (n - 1 > 4 ? 4 : n - 1);
        DWORD r = U(GetPrivateProfileStringW)(L"section1", L"name1", L"default", b, n, wp);
        CHECKF(r == want, "nSize %lu -> %lu want %lu", n, r, want);
        if (n > 0) CHECK(b[want] == 0);
        CHECKF(b[n] == L'z', "wrote past nSize %lu", n);
    }
    ap_rm(path);
}

TEST(profile_get_keys_list_multisz) {
    REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH]; mkbase(path, "keys.ini"); char b[100], f[100];
    memset(b, 'z', sizeof b);
    DWORD r = ap_gps("section1", NULL, "default", b, sizeof b, path);
    EXPECT_EQ(r, 18);
    ap_flat(b, sizeof b, f, sizeof f);
    EXPECT_STR(f, "name1|name2|name4");
    CHECK(!memcmp(b, "name1\0name2\0name4\0\0", 19));
    ap_rm(path);
}

/* Multi-sz truncation model: data has T chars (including each entry's NUL, excluding the final one).
 * nSize >= T+2: full, returns T.  Otherwise returns nSize-2 (0 if nSize < 2), the buffer holds the
 * first nSize-2 data bytes followed by two NULs. */
static void check_multisz_model(const char *what, const char *data, size_t T, DWORD (*get)(char *b, DWORD n), int strict_nul) {
    for (DWORD n = 0; n <= T + 4; n++) {
        char b[128]; memset(b, 'z', sizeof b);
        DWORD r = get(b, n);
        if (n > 0 && n < 4) { t_note("%s nSize %lu -> ret %lu (not asserted)", what, n, r); continue; }
        DWORD wantr = n >= T + 2 ? (DWORD)T : (n >= 2 ? n - 2 : 0);
        CHECKF(r == wantr, "%s nSize %lu: ret %lu want %lu", what, n, r, wantr);
        if (n == 0) { CHECKF(b[0] == 'z', "%s nSize 0 touched buffer", what); continue; }
        DWORD keep = wantr < (DWORD)T ? wantr : (DWORD)T;
        CHECKF(!memcmp(b, data, keep), "%s nSize %lu: prefix mismatch", what, n);
        CHECKF(b[keep] == 0, "%s nSize %lu: not terminated", what, n);
        if (strict_nul && !t_is_wine() && n >= 2 && wantr < (DWORD)T) CHECKF(b[keep + 1] == 0, "%s nSize %lu: no second NUL", what, n);
        CHECKF(b[n] == 'z' || n > T + 2, "%s nSize %lu: wrote past buffer", what, n);
    }
}
static char g_trunc_path[MAX_PATH]; static const char *g_trunc_sec;
static DWORD get_keys_cb(char *b, DWORD n) { return ap_gps(g_trunc_sec, NULL, "d", b, n, g_trunc_path); }
static DWORD get_secs_cb(char *b, DWORD n) { return ap_gps(NULL, "name1", "d", b, n, g_trunc_path); }
TEST(profile_get_keys_list_truncation) {
    REQUIRE(U(GetPrivateProfileStringA));
    mkbase(g_trunc_path, "keystr.ini"); g_trunc_sec = "section1";
    check_multisz_model("keys", "name1\0name2\0name4\0", 18, get_keys_cb, 1);
    check_multisz_model("sections(app NULL)", "section1\0section2\0", 18, get_secs_cb, 1);
    ap_rm(g_trunc_path);
}

TEST(profile_get_section_names_via_null_app) {
    REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH]; mkbase(path, "secnull.ini"); char b[100], f[100];
    memset(b, 'z', sizeof b);
    DWORD r = ap_gps(NULL, "name1", "default", b, sizeof b, path);
    EXPECT_EQ(r, 18);
    ap_flat(b, sizeof b, f, sizeof f);
    EXPECT_STR(f, "section1|section2");
    CHECK(!memcmp(b, "section1\0section2\0\0", 19));
    memset(b, 'z', sizeof b); EXPECT_EQ(ap_gps(NULL, "name1", "default", b, 16, path), 14);
    CHECK(!memcmp(b, "section1\0secti\0", 15));
    ap_rm(path);
}

TEST(profile_get_null_app_and_key_both) {
    REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH]; mkbase(path, "bothnull.ini"); char b[100], f[100];
    memset(b, 'z', sizeof b);
    DWORD r = ap_gps(NULL, NULL, "default", b, sizeof b, path);
    EXPECT_EQ(r, 18);
    ap_flat(b, sizeof b, f, sizeof f);
    EXPECT_STR(f, "section1|section2");
    ap_rm(path);
}

TEST(profile_get_keys_list_empty_section_and_missing) {
    REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH]; mkbase(path, "keysempty.ini"); char b[32];
    memset(b, 'z', sizeof b);
    { DWORD r2 = ap_gps("section2", NULL, "default", b, sizeof b, path);
      t_note("keys of empty section: ret=%lu b0=%d b1=%d", r2, b[0], b[1]); }
    memset(b, 'z', sizeof b);
    DWORD r = ap_gps("nosuch", NULL, "default", b, sizeof b, path);
    t_note("keys of missing section: ret=%lu b0=%d b1=%d", r, b[0], b[1]);
    CHECK(b[0] == 0 || r == 7);
    ap_rm(path);
}

TEST(profile_get_keys_list_comments_and_dups) {
    REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH]; char b[128], f[128];
    ap_ini(path, "keysdup.ini", "[s]\r\n;comment=1\r\na=1\r\nb=2\r\n\r\nc\r\nd=\r\n[t]\r\nz=1\r\n");
    memset(b, 'z', sizeof b);
    ap_gps("s", NULL, "", b, sizeof b, path);
    ap_flat(b, sizeof b, f, sizeof f);
    EXPECT_STR(f, "a|b|d");
    ap_rm(path);
    ap_ini(path, "keysdup2.ini", "[s]\r\na=1\r\na=2\r\nb=3\r\n");
    ap_gps("s", NULL, "", b, sizeof b, path);
    ap_flat(b, sizeof b, f, sizeof f);
    t_note("duplicate keys listed as: %s", f);
    ap_rm(path);
}

TEST(profile_get_section_names_trimmed_and_forms) {
    REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH]; char b[128], f[128];
    ap_ini(path, "secforms.ini", "[ spaced ]\r\nk=1\r\n[a b]\r\nk=2\r\n[UPPER]\r\nk=3\r\n");
    ap_gps(NULL, NULL, "", b, sizeof b, path);
    ap_flat(b, sizeof b, f, sizeof f);
    EXPECT_STR(f, "spaced|a b|UPPER");
    EXPECT_STR(ap_val("spaced", "k", path), "1");
    EXPECT_STR(ap_val("a b", "k", path), "2");
    EXPECT_STR(ap_val("upper", "k", path), "3");
    t_note("lookup with padded section name -> %s", ap_val(" spaced ", "k", path));
    ap_rm(path);
}

TEST(profile_get_section_header_variants_note) {
    REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH];
    const char *c[] = {
        "[s] ; trailing comment\r\nk=1\r\n", "[s\r\nk=1\r\n", "s]\r\nk=1\r\n", "[s]x\r\nk=1\r\n",
        "[a]b]\r\nk=1\r\n", "  [s]\r\nk=1\r\n", "[[s]]\r\nk=1\r\n", "[s ]\r\nk=1\r\n", "[ s]\r\nk=1\r\n",
    };
    for (unsigned i = 0; i < sizeof c / sizeof *c; i++) {
        ap_ini(path, "sechdr.ini", c[i]);
        t_note("header variant %u: sec 's' -> \"%s\"", i, ap_val("s", "k", path));
    }
    ap_rm(path);
}

TEST(profile_get_duplicate_sections_first_wins) {
    REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH]; ap_ini(path, "dupsec.ini", "[s]\r\na=1\r\n[t]\r\nb=2\r\n[s]\r\nc=3\r\na=9\r\n");
    EXPECT_STR(ap_val("s", "a", path), "1");
    EXPECT_STR(ap_val("t", "b", path), "2");
    t_note("key of second duplicate section 'c' -> %s", ap_val("s", "c", path));
    char b[64], f[64]; ap_gps(NULL, NULL, "", b, sizeof b, path); ap_flat(b, sizeof b, f, sizeof f);
    t_note("section names with duplicate: %s", f);
    ap_rm(path);
}

TEST(profile_get_line_endings) {
    REQUIRE(U(GetPrivateProfileStringA));
    const char *ends[] = { "\r\n", "\n", "\r", "\n\r", "\r\r\n" };
    char path[MAX_PATH];
    for (unsigned i = 0; i < 5; i++) {
        char c[256]; snprintf(c, sizeof c, "[s]%sa=1%sb=two%sc=\"three\"%s[t]%sd=4%s", ends[i], ends[i], ends[i], ends[i], ends[i], ends[i]);
        ap_ini(path, "endings.ini", c);
        CHECKF(!strcmp(ap_val("s", "a", path), "1"), "ending %u a", i);
        CHECKF(!strcmp(ap_val("s", "b", path), "two"), "ending %u b", i);
        CHECKF(!strcmp(ap_val("s", "c", path), "three"), "ending %u c", i);
        CHECKF(!strcmp(ap_val("t", "d", path), "4"), "ending %u d", i);
    }
    ap_rm(path);
}

TEST(profile_get_empty_and_tiny_files) {
    REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH]; ap_ini(path, "tiny.ini", ""); ap_put(path, "", 0);
    char b[16];
    EXPECT_EQ(ap_gps("s", "k", "d", b, sizeof b, path), 1);
    EXPECT_STR(b, "d");
    ap_puts(path, "[");  EXPECT_EQ(ap_gps("s", "k", "d", b, sizeof b, path), 1);
    ap_puts(path, "[s");  EXPECT_EQ(ap_gps("s", "k", "d", b, sizeof b, path), 1);
    ap_puts(path, "[s]");  EXPECT_EQ(ap_gps("s", "k", "d", b, sizeof b, path), 1);
    ap_puts(path, "[s]k");  EXPECT_EQ(ap_gps("s", "k", "d", b, sizeof b, path), 1);
    ap_puts(path, "[s]\r\nk");  EXPECT_EQ(ap_gps("s", "k", "d", b, sizeof b, path), 1);
    ap_puts(path, "[s]\r\nk=");  EXPECT_EQ(ap_gps("s", "k", "d", b, sizeof b, path), 0);
    ap_puts(path, "[s]\r\nk=x");  EXPECT_EQ(ap_gps("s", "k", "d", b, sizeof b, path), 1);
    EXPECT_STR(b, "x");
    ap_puts(path, "\r\n\r\n\r\n"); EXPECT_EQ(ap_gps("s", "k", "d", b, sizeof b, path), 1);
    ap_puts(path, "[s]\r\n"); EXPECT_EQ(ap_gps("s", "k", "d", b, sizeof b, path), 1);
    ap_rm(path);
}

TEST(profile_get_binary_garbage_file) {
    REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH]; ap_ini(path, "garbage.ini", NULL);
    unsigned char g[4096];
    unsigned x = 12345;
    for (int i = 0; i < 4096; i++) { x = x * 1103515245 + 12345; g[i] = (unsigned char)(x >> 16); }
    ap_put(path, g, sizeof g);
    char b[32];
    DWORD r = ap_gps("s", "k", "default", b, sizeof b, path);
    EXPECT_EQ(r, 7);
    EXPECT_STR(b, "default");
    unsigned char z[2000] = { 0 };
    ap_put(path, z, sizeof z);
    r = ap_gps("s", "k", "default", b, sizeof b, path);
    EXPECT_EQ(r, 7);
    ap_rm(path);
}

TEST(profile_get_nuls_before_first_section) {
    REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH]; ap_ini(path, "nuls.ini", NULL);
    const char data[] = "Data \0 before \0 first \0 section\r\n[section1]\r\nkey1=string1\r\n";
    ap_put(path, data, sizeof data - 1);
    t_note("value after NUL-prefixed junk: %s", ap_val("section1", "key1", path));
    ap_rm(path);
}

TEST(profile_get_long_value) {
    REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH]; ap_ini(path, "longval.ini", NULL);
    static char c[20000], b[20000];
    int n = snprintf(c, sizeof c, "[s]\r\nk=");
    for (int i = 0; i < 10000; i++) c[n++] = 'a' + i % 26;
    n += snprintf(c + n, 16, "\r\nafter=1\r\n");
    ap_put(path, c, n);
    EXPECT_EQ(ap_gps("s", "k", "d", b, sizeof b, path), 10000);
    CHECK(b[0] == 'a' && b[9999] == 'a' + 9999 % 26 && b[10000] == 0);
    EXPECT_STR(ap_val("s", "after", path), "1");
    EXPECT_EQ(ap_gps("s", "k", "d", b, 100, path), 99);
    ap_rm(path);
}

TEST(profile_get_large_buffer_values) {
    REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH]; mkbase(path, "largebuf.ini");
    static char b[70000];
    memset(b, 'z', sizeof b);
    EXPECT_EQ(ap_gps("section1", "name1", "d", b, sizeof b, path), 4);
    EXPECT_STR(b, "val1");
    EXPECT_EQ(b[5], 'z');
    ap_rm(path);
}

TEST(profile_get_wide_ansi_parity) {
    REQUIRE(U(GetPrivateProfileStringA)); REQUIRE(U(GetPrivateProfileStringW));
    char path[MAX_PATH]; mkbase(path, "parity.ini"); WCHAR wp[MAX_PATH]; ap_wpath(wp, path);
    const char *keys[] = { "name1", "name2", "name3", "name4", "nokey" };
    for (int i = 0; i < 5; i++) {
        char a[32]; WCHAR w[32], wk[32], wd[8], wa[32]; ap_widen(wk, keys[i]); ap_widen(wd, "DEF");
        DWORD ra = ap_gps("section1", keys[i], "DEF", a, sizeof a, path);
        DWORD rw = U(GetPrivateProfileStringW)(L"section1", wk, wd, w, 32, wp);
        ap_widen(wa, a);
        CHECKF(ra == rw && !lstrcmpW(wa, w), "key %s: A=%lu W=%lu", keys[i], ra, rw);
    }
    char ba[100], f1[100], f2[100]; WCHAR bw[100];
    DWORD ra = ap_gps("section1", NULL, "", ba, 100, path);
    DWORD rw = U(GetPrivateProfileStringW)(L"section1", NULL, L"", bw, 100, wp);
    EXPECT_EQ(ra, rw);
    ap_flat(ba, 100, f1, 100); ap_flatw(bw, 100, f2, 100);
    EXPECT_STR(f1, f2);
    ap_rm(path);
}

TEST(profile_get_wide_key_list_truncation) {
    REQUIRE(U(GetPrivateProfileStringW));
    char path[MAX_PATH]; mkbase(path, "wkeys.ini"); WCHAR wp[MAX_PATH], b[64]; ap_wpath(wp, path);
    for (int i = 0; i < 64; i++) b[i] = L'z';
    EXPECT_EQ(U(GetPrivateProfileStringW)(L"section1", NULL, L"", b, 16, wp), 14);
    CHECK(b[14] == 0 && b[15] == 0 && b[13] == L'a');
    for (int i = 0; i < 64; i++) b[i] = L'z';
    EXPECT_EQ(U(GetPrivateProfileStringW)(NULL, NULL, L"", b, 10, wp), 8);
    CHECK(b[8] == 0 && b[9] == 0 && b[0] == L's');
    EXPECT_EQ(b[10], L'z');
    ap_rm(path);
}

TEST(profile_get_wide_nonascii_value) {
    REQUIRE(U(GetPrivateProfileStringW));
    char path[MAX_PATH]; ap_ini(path, "wnonascii.ini", NULL);
    ap_puts(path, "[s]\r\nk=caf\xe9\r\n");
    WCHAR wp[MAX_PATH], b[32], want[32]; ap_wpath(wp, path);
    DWORD r = U(GetPrivateProfileStringW)(L"s", L"k", L"d", b, 32, wp);
    MultiByteToWideChar(CP_ACP, 0, "caf\xe9", -1, want, 32);
    EXPECT_EQ(r, 4);
    CHECK(!lstrcmpW(b, want));
    /* non-ASCII key lookups */
    WCHAR wk[8]; wk[0] = 'c'; wk[1] = 0;
    r = U(GetPrivateProfileStringW)(L"s", wk, L"d", b, 32, wp);
    EXPECT_EQ(r, 1);
    ap_rm(path);
}

TEST(profile_get_repeated_calls_stable) {
    REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH]; mkbase(path, "repeat.ini"); char b[32]; int bad = 0;
    for (int i = 0; i < 2000; i++) { if (ap_gps("section1", "name1", "d", b, sizeof b, path) != 4 || strcmp(b, "val1")) bad++; }
    EXPECT_EQ(bad, 0);
    ap_rm(path);
}

TEST(profile_get_sees_external_modification) {
    REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH]; ap_ini(path, "refresh.ini", "[s]\r\nk=one\r\n");
    EXPECT_STR(ap_val("s", "k", path), "one");
    ap_puts(path, "[s]\r\nk=two\r\n");
    EXPECT_STR(ap_val("s", "k", path), "two");
    ap_puts(path, "[s]\r\nk=three-longer-value\r\n");
    EXPECT_STR(ap_val("s", "k", path), "three-longer-value");
    ap_rm(path);
    EXPECT_STR(ap_val("s", "k", path), "DEF");
    ap_puts(path, "[s]\r\nk=back\r\n");
    EXPECT_STR(ap_val("s", "k", path), "back");
    ap_rm(path);
}

TEST(profile_get_readonly_file_readable) {
    REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH]; ap_ini(path, "ro.ini", "[s]\r\nk=ro\r\n");
    SetFileAttributesA(path, FILE_ATTRIBUTE_READONLY);
    EXPECT_STR(ap_val("s", "k", path), "ro");
    SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
    ap_rm(path);
}

TEST(profile_get_file_open_elsewhere) {
    REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH]; ap_ini(path, "shared.ini", "[s]\r\nk=shared\r\n");
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    EXPECT_STR(ap_val("s", "k", path), "shared");
    CloseHandle(h);
    h = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    EXPECT_STR(ap_val("s", "k", path), "shared");
    CloseHandle(h);
    /* exclusive open elsewhere: the read cannot open the file, so the default comes back */
    h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    SET_ERR(0);
    EXPECT_STR(ap_val("s", "k", path), "DEF");
    t_note("exclusive-open read: last error %lu", GetLastError());
    CloseHandle(h);
    ap_rm(path);
}

TEST(profile_get_delete_on_close_file) {
    REQUIRE(U(GetPrivateProfileIntA));
    char path[MAX_PATH]; t_tmpfile(path, sizeof path, "doc.ini");
    HANDLE h = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    DWORD w; const char c[] = "[s]\nk=123\n"; WriteFile(h, c, sizeof c - 1, &w, NULL);
    EXPECT_EQ(U(GetPrivateProfileIntA)("s", "k", 0, path), 123);
    CloseHandle(h);
    EXPECT_EQ(ap_exists(path), 0);
}

TEST(profile_get_many_keys_one_section) {
    REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH]; static char c[40000]; int n = snprintf(c, sizeof c, "[s]\r\n");
    for (int i = 0; i < 1000; i++) n += snprintf(c + n, 64, "key%04d=value%04d\r\n", i, i);
    ap_ini(path, "manykeys.ini", NULL); ap_put(path, c, n);
    int bad = 0;
    for (int i = 0; i < 1000; i += 7) {
        char k[16], want[16]; snprintf(k, sizeof k, "key%04d", i); snprintf(want, sizeof want, "value%04d", i);
        if (strcmp(ap_val("s", k, path), want)) bad++;
    }
    EXPECT_EQ(bad, 0);
    static char b[20000];
    DWORD r = ap_gps("s", NULL, "", b, sizeof b, path);
    EXPECT_EQ(r, 1000 * 8);
    ap_rm(path);
}

TEST(profile_get_file_with_utf8_bom_is_ansi) {
    REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH]; ap_ini(path, "bom8.ini", NULL);
    ap_put(path, "\xEF\xBB\xBF[first]\r\na=1\r\n[second]\r\nb=2\r\n", 3 + 29);
    EXPECT_STR(ap_val("second", "b", path), "2");
    t_note("section on the BOM line: %s", ap_val("first", "a", path));
    char b[64], f[64]; ap_gps(NULL, NULL, "", b, sizeof b, path); ap_flat(b, sizeof b, f, sizeof f);
    t_note("section names with UTF-8 BOM: %s", f);
    ap_rm(path);
}
