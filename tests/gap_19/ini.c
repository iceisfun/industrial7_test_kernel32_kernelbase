#include "t.h"
/* gap19: WritePrivateProfileStringA depth and WriteProfileSectionW (win.ini variant is
 * never called: it would modify %WINDIR%\win.ini; only its export is checked). */

UDECL(BOOL, WritePrivateProfileStringA, (LPCSTR, LPCSTR, LPCSTR, LPCSTR));
UDECL(BOOL, WriteProfileSectionW, (LPCWSTR, LPCWSTR));
UDECL(DWORD, GetPrivateProfileStringA, (LPCSTR, LPCSTR, LPCSTR, LPSTR, DWORD, LPCSTR));

static char *g19_inipath(const char *leaf, char *out, size_t cap) { t_tmpfile(out, cap, leaf); DeleteFileA(out); return out; }
static BOOL g19_w(const char *f, const char *s, const char *k, const char *v) { return UN(WritePrivateProfileStringA)(s, k, v, f); }
static void g19_flush(const char *f) { UN(WritePrivateProfileStringA)(NULL, NULL, NULL, f); }
static DWORD g19_filesize(const char *f) {
    HANDLE h = CreateFileA(f, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0xffffffff;
    DWORD s = GetFileSize(h, NULL); CloseHandle(h); return s;
}
static DWORD g19_slurp(const char *f, char *b, DWORD cap) {
    HANDLE h = CreateFileA(f, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    DWORD n = 0; if (h == INVALID_HANDLE_VALUE) return 0;
    ReadFile(h, b, cap - 1, &n, NULL); CloseHandle(h); b[n] = 0; return n;
}
static void g19_get(const char *f, const char *s, const char *k, char *out, DWORD cap) {
    UN(GetPrivateProfileStringA)(s, k, "<def>", out, cap, f);
}

TEST(gap19_wpps_exported_by_kernel32) {
    CHECK(UK(WritePrivateProfileStringA) != NULL);
    CHECK(UK(WriteProfileSectionW) != NULL);
    CHECK(UK(WriteProfileStringA) != NULL);
}
TEST(gap19_wpps_basic_roundtrip_and_file_content) {
    REQUIRE(UN(WritePrivateProfileStringA)); REQUIRE(UN(GetPrivateProfileStringA));
    char f[MAX_PATH]; g19_inipath("basic.ini", f, sizeof f);
    SET_ERR(0);
    EXPECT_EQ(g19_w(f, "Sec", "Key", "Val"), TRUE);
    g19_flush(f);
    char b[256]; g19_slurp(f, b, sizeof b);
    CHECK(strstr(b, "[Sec]\r\n") != NULL);
    CHECK(strstr(b, "Key=Val\r\n") != NULL);
    char o[64]; g19_get(f, "Sec", "Key", o, sizeof o);
    EXPECT_STR(o, "Val");
    DeleteFileA(f);
}
TEST(gap19_wpps_creates_ansi_file_no_bom) {
    REQUIRE(UN(WritePrivateProfileStringA));
    char f[MAX_PATH]; g19_inipath("nobom.ini", f, sizeof f);
    EXPECT_EQ(g19_w(f, "s", "k", "v"), TRUE);
    g19_flush(f);
    char b[256]; DWORD n = g19_slurp(f, b, sizeof b);
    CHECK(n > 4);
    CHECK((BYTE)b[0] != 0xff && (BYTE)b[0] != 0xfe && (BYTE)b[0] != 0xef);
    CHECK(b[0] == '[');
    DeleteFileA(f);
}
TEST(gap19_wpps_case_insensitive_lookup) {
    REQUIRE(UN(WritePrivateProfileStringA)); REQUIRE(UN(GetPrivateProfileStringA));
    char f[MAX_PATH]; g19_inipath("case.ini", f, sizeof f);
    g19_w(f, "MySec", "MyKey", "one");
    char o[64];
    g19_get(f, "mysec", "MYKEY", o, sizeof o); EXPECT_STR(o, "one");
    /* writing with other case updates the existing entry rather than adding */
    EXPECT_EQ(g19_w(f, "MYSEC", "mykey", "two"), TRUE);
    g19_get(f, "MySec", "MyKey", o, sizeof o); EXPECT_STR(o, "two");
    g19_flush(f);
    char b[256]; g19_slurp(f, b, sizeof b);
    int cnt = 0; for (const char *p = b; (p = strstr(p, "=")); p++) cnt++;
    EXPECT_EQ(cnt, 1);
    DeleteFileA(f);
}
TEST(gap19_wpps_overwrite_value) {
    REQUIRE(UN(WritePrivateProfileStringA)); REQUIRE(UN(GetPrivateProfileStringA));
    char f[MAX_PATH]; g19_inipath("over.ini", f, sizeof f);
    for (int i = 0; i < 50; i++) {
        char v[16]; snprintf(v, sizeof v, "val%d", i);
        EXPECT_EQ(g19_w(f, "s", "k", v), TRUE);
    }
    char o[64]; g19_get(f, "s", "k", o, sizeof o); EXPECT_STR(o, "val49");
    g19_flush(f);
    char b[512]; g19_slurp(f, b, sizeof b);
    CHECK(strstr(b, "val48") == NULL);
    DeleteFileA(f);
}
TEST(gap19_wpps_delete_key) {
    REQUIRE(UN(WritePrivateProfileStringA)); REQUIRE(UN(GetPrivateProfileStringA));
    char f[MAX_PATH]; g19_inipath("delkey.ini", f, sizeof f);
    g19_w(f, "s", "a", "1"); g19_w(f, "s", "b", "2");
    EXPECT_EQ(g19_w(f, "s", "a", NULL), TRUE);
    char o[64];
    g19_get(f, "s", "a", o, sizeof o); EXPECT_STR(o, "<def>");
    g19_get(f, "s", "b", o, sizeof o); EXPECT_STR(o, "2");
    /* deleting a key that does not exist still succeeds */
    EXPECT_EQ(g19_w(f, "s", "nokey", NULL), TRUE);
    DeleteFileA(f);
}
TEST(gap19_wpps_delete_section) {
    REQUIRE(UN(WritePrivateProfileStringA)); REQUIRE(UN(GetPrivateProfileStringA));
    char f[MAX_PATH]; g19_inipath("delsec.ini", f, sizeof f);
    g19_w(f, "s1", "a", "1"); g19_w(f, "s2", "b", "2");
    EXPECT_EQ(g19_w(f, "s1", NULL, NULL), TRUE);
    char o[64];
    g19_get(f, "s1", "a", o, sizeof o); EXPECT_STR(o, "<def>");
    g19_get(f, "s2", "b", o, sizeof o); EXPECT_STR(o, "2");
    g19_flush(f);
    char b[256]; g19_slurp(f, b, sizeof b);
    CHECK(strstr(b, "[s1]") == NULL);
    CHECK(strstr(b, "[s2]") != NULL);
    DeleteFileA(f);
}
TEST(gap19_wpps_key_order_appended) {
    REQUIRE(UN(WritePrivateProfileStringA));
    char f[MAX_PATH]; g19_inipath("order.ini", f, sizeof f);
    g19_w(f, "s", "z", "1"); g19_w(f, "s", "a", "2"); g19_w(f, "s", "m", "3");
    g19_flush(f);
    char b[256]; g19_slurp(f, b, sizeof b);
    char *pz = strstr(b, "z=1"), *pa = strstr(b, "a=2"), *pm = strstr(b, "m=3");
    CHECK(pz && pa && pm);
    if (pz && pa && pm) CHECK(pz < pa && pa < pm);
    DeleteFileA(f);
}
TEST(gap19_wpps_multiple_sections) {
    REQUIRE(UN(WritePrivateProfileStringA)); REQUIRE(UN(GetPrivateProfileStringA));
    char f[MAX_PATH]; g19_inipath("multi.ini", f, sizeof f);
    g19_w(f, "A", "k", "1"); g19_w(f, "B", "k", "2"); g19_w(f, "A", "k2", "3");
    char o[64];
    g19_get(f, "A", "k", o, sizeof o); EXPECT_STR(o, "1");
    g19_get(f, "B", "k", o, sizeof o); EXPECT_STR(o, "2");
    g19_get(f, "A", "k2", o, sizeof o); EXPECT_STR(o, "3");
    g19_flush(f);
    char b[256]; g19_slurp(f, b, sizeof b);
    /* k2 was inserted into section A, before [B] */
    char *p2 = strstr(b, "k2=3"), *pb = strstr(b, "[B]");
    CHECK(p2 && pb && p2 < pb);
    DeleteFileA(f);
}
TEST(gap19_wpps_empty_value) {
    REQUIRE(UN(WritePrivateProfileStringA)); REQUIRE(UN(GetPrivateProfileStringA));
    char f[MAX_PATH]; g19_inipath("empty.ini", f, sizeof f);
    EXPECT_EQ(g19_w(f, "s", "k", ""), TRUE);
    char o[64]; g19_get(f, "s", "k", o, sizeof o);
    EXPECT_STR(o, "");       /* present-but-empty differs from missing */
    g19_flush(f);
    char b[256]; g19_slurp(f, b, sizeof b);
    CHECK(strstr(b, "k=\r\n") != NULL);
    DeleteFileA(f);
}
TEST(gap19_wpps_value_with_equals_and_semicolon) {
    REQUIRE(UN(WritePrivateProfileStringA)); REQUIRE(UN(GetPrivateProfileStringA));
    char f[MAX_PATH]; g19_inipath("eq.ini", f, sizeof f);
    EXPECT_EQ(g19_w(f, "s", "k", "a=b=c"), TRUE);
    char o[64]; g19_get(f, "s", "k", o, sizeof o); EXPECT_STR(o, "a=b=c");
    EXPECT_EQ(g19_w(f, "s", "semi", "x;y"), TRUE);
    g19_get(f, "s", "semi", o, sizeof o); EXPECT_STR(o, "x;y");
    DeleteFileA(f);
}
TEST(gap19_wpps_spaces_in_names) {
    REQUIRE(UN(WritePrivateProfileStringA)); REQUIRE(UN(GetPrivateProfileStringA));
    char f[MAX_PATH]; g19_inipath("sp.ini", f, sizeof f);
    EXPECT_EQ(g19_w(f, "my section", "my key", "my value"), TRUE);
    char o[64]; g19_get(f, "my section", "my key", o, sizeof o); EXPECT_STR(o, "my value");
    DeleteFileA(f);
}
TEST(gap19_wpps_long_value) {
    REQUIRE(UN(WritePrivateProfileStringA)); REQUIRE(UN(GetPrivateProfileStringA));
    char f[MAX_PATH]; g19_inipath("long.ini", f, sizeof f);
    char *v = malloc(8001), *o = malloc(9000);
    memset(v, 'x', 8000); v[8000] = 0;
    EXPECT_EQ(g19_w(f, "s", "k", v), TRUE);
    DWORD n = UN(GetPrivateProfileStringA)("s", "k", "", o, 9000, f);
    EXPECT_EQ(n, 8000);
    CHECK(!strcmp(o, v));
    free(v); free(o);
    DeleteFileA(f);
}
TEST(gap19_wpps_many_keys) {
    REQUIRE(UN(WritePrivateProfileStringA)); REQUIRE(UN(GetPrivateProfileStringA));
    char f[MAX_PATH]; g19_inipath("many.ini", f, sizeof f);
    int ok = 0;
    for (int i = 0; i < 300; i++) {
        char k[16], v[16]; snprintf(k, sizeof k, "k%d", i); snprintf(v, sizeof v, "v%d", i);
        ok += g19_w(f, "s", k, v) ? 1 : 0;
    }
    EXPECT_EQ(ok, 300);
    char o[32];
    g19_get(f, "s", "k0", o, sizeof o); EXPECT_STR(o, "v0");
    g19_get(f, "s", "k299", o, sizeof o); EXPECT_STR(o, "v299");
    DeleteFileA(f);
}
TEST(gap19_wpps_flush_all_null) {
    REQUIRE(UN(WritePrivateProfileStringA));
    char f[MAX_PATH]; g19_inipath("flush.ini", f, sizeof f);
    g19_w(f, "s", "k", "v");
    SET_ERR(0);
    BOOL r = UN(WritePrivateProfileStringA)(NULL, NULL, NULL, f);
    t_note("flush-only call returned %d (documented: zero when it flushes)", r);
    CHECK(g19_filesize(f) != 0xffffffff);
    DeleteFileA(f);
}
TEST(gap19_wpps_preserves_utf16_file) {
    REQUIRE(UN(WritePrivateProfileStringA)); REQUIRE(UN(GetPrivateProfileStringA));
    char f[MAX_PATH]; g19_inipath("u16.ini", f, sizeof f);
    HANDLE h = CreateFileA(f, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    static const WCHAR body[] = L"\xfeff[s]\r\nk=v\r\n";
    DWORD w; WriteFile(h, body, sizeof body - sizeof(WCHAR), &w, NULL); CloseHandle(h);
    DWORD before = g19_filesize(f);
    EXPECT_EQ(g19_w(f, "s", "k2", "v2"), TRUE);
    g19_flush(f);
    char b[256]; DWORD n = g19_slurp(f, b, sizeof b);
    CHECK(n > before);
    CHECK((BYTE)b[0] == 0xff && (BYTE)b[1] == 0xfe);   /* still UTF-16LE with BOM */
    char o[32];
    g19_get(f, "s", "k2", o, sizeof o); EXPECT_STR(o, "v2");
    g19_get(f, "s", "k", o, sizeof o); EXPECT_STR(o, "v");
    DeleteFileA(f);
}
TEST(gap19_wpps_missing_directory) {
    REQUIRE(UN(WritePrivateProfileStringA));
    char f[MAX_PATH]; t_tmpfile(f, sizeof f, "no_such_dir_g19\\x.ini");
    SET_ERR(0);
    BOOL r = UN(WritePrivateProfileStringA)("s", "k", "v", f);
    EXPECT_EQ(r, FALSE);
    DWORD e = GetLastError();
    t_note("missing dir -> error %lu", (unsigned long)e);
    CHECK(e == ERROR_PATH_NOT_FOUND || e == ERROR_FILE_NOT_FOUND || e != 0);
}
TEST(gap19_wpps_readonly_file) {
    REQUIRE(UN(WritePrivateProfileStringA)); REQUIRE(UN(GetPrivateProfileStringA));
    char f[MAX_PATH]; g19_inipath("ro.ini", f, sizeof f);
    g19_w(f, "s", "k", "orig");
    g19_flush(f);
    SetFileAttributesA(f, FILE_ATTRIBUTE_READONLY);
    SET_ERR(0);
    BOOL r = UN(WritePrivateProfileStringA)("s", "k", "changed", f);
    DWORD e = GetLastError();
    EXPECT_EQ(r, FALSE);
    t_note("readonly -> error %lu", (unsigned long)e);
    SetFileAttributesA(f, FILE_ATTRIBUTE_NORMAL);
    char o[32]; g19_get(f, "s", "k", o, sizeof o);
    EXPECT_STR(o, "orig");
    DeleteFileA(f);
}
TEST(gap19_wpps_locked_file) {
    REQUIRE(UN(WritePrivateProfileStringA));
    char f[MAX_PATH]; g19_inipath("locked.ini", f, sizeof f);
    g19_w(f, "s", "k", "orig"); g19_flush(f);
    HANDLE h = CreateFileA(f, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);   /* exclusive */
    CHECK(h != INVALID_HANDLE_VALUE);
    SET_ERR(0);
    BOOL r = UN(WritePrivateProfileStringA)("s", "k", "new", f);
    DWORD e = GetLastError();
    EXPECT_EQ(r, FALSE);
    t_note("exclusive-locked -> error %lu", (unsigned long)e);
    CloseHandle(h);
    DeleteFileA(f);
}
TEST(gap19_wpps_delete_in_missing_file) {
    REQUIRE(UN(WritePrivateProfileStringA));
    char f[MAX_PATH]; g19_inipath("nofile.ini", f, sizeof f);
    SET_ERR(0);
    BOOL r = UN(WritePrivateProfileStringA)("s", "k", NULL, f);
    t_note("delete key in missing file -> %d err %lu", r, (unsigned long)GetLastError());
    g19_flush(f);
    DeleteFileA(f);
}
TEST(gap19_wpps_bracketed_section_name) {
    REQUIRE(UN(WritePrivateProfileStringA)); REQUIRE(UN(GetPrivateProfileStringA));
    char f[MAX_PATH]; g19_inipath("brk.ini", f, sizeof f);
    BOOL r = g19_w(f, "a]b", "k", "v");
    t_note("section with ] -> %d", r);
    char o[32]; g19_get(f, "a]b", "k", o, sizeof o);
    t_note("read back '%s'", o);
    DeleteFileA(f);
}
TEST(gap19_wpps_ansi_high_chars_roundtrip) {
    REQUIRE(UN(WritePrivateProfileStringA)); REQUIRE(UN(GetPrivateProfileStringA));
    char f[MAX_PATH]; g19_inipath("hi.ini", f, sizeof f);
    EXPECT_EQ(g19_w(f, "s", "k", "caf\xe9"), TRUE);
    char o[32]; g19_get(f, "s", "k", o, sizeof o); EXPECT_STR(o, "caf\xe9");
    g19_flush(f);
    char b[256]; g19_slurp(f, b, sizeof b);
    CHECK(strstr(b, "caf\xe9") != NULL);   /* bytes written verbatim in an ANSI file */
    DeleteFileA(f);
}
TEST(gap19_wpps_key_quotes_preserved_on_disk) {
    REQUIRE(UN(WritePrivateProfileStringA)); REQUIRE(UN(GetPrivateProfileStringA));
    char f[MAX_PATH]; g19_inipath("q.ini", f, sizeof f);
    EXPECT_EQ(g19_w(f, "s", "k", "\"quoted\""), TRUE);
    g19_flush(f);
    char b[256]; g19_slurp(f, b, sizeof b);
    CHECK(strstr(b, "k=\"quoted\"") != NULL);
    char o[32]; g19_get(f, "s", "k", o, sizeof o);
    EXPECT_STR(o, "quoted");    /* surrounding quotes stripped on read */
    DeleteFileA(f);
}
TEST(gap19_wpps_two_files_independent) {
    REQUIRE(UN(WritePrivateProfileStringA)); REQUIRE(UN(GetPrivateProfileStringA));
    char f1[MAX_PATH], f2[MAX_PATH]; g19_inipath("i1.ini", f1, sizeof f1); g19_inipath("i2.ini", f2, sizeof f2);
    g19_w(f1, "s", "k", "one"); g19_w(f2, "s", "k", "two");
    char o[32];
    g19_get(f1, "s", "k", o, sizeof o); EXPECT_STR(o, "one");
    g19_get(f2, "s", "k", o, sizeof o); EXPECT_STR(o, "two");
    DeleteFileA(f1); DeleteFileA(f2);
}
TEST(gap19_wpps_null_section_with_key_is_not_a_write) {
    REQUIRE(UN(WritePrivateProfileStringA));
    char f[MAX_PATH]; g19_inipath("nullsec.ini", f, sizeof f);
    SET_ERR(0);
    BOOL r = UN(WritePrivateProfileStringA)(NULL, "k", "v", f);
    t_note("NULL section, key set -> %d err %lu", r, (unsigned long)GetLastError());
    char b[64]; DWORD n = g19_slurp(f, b, sizeof b);
    CHECK(n == 0 || strstr(b, "k=v") == NULL);
    DeleteFileA(f);
}
static char g19_sf[MAX_PATH];
static DWORD WINAPI g19_wk(LPVOID p) {
    int id = (int)(INT_PTR)p;
    for (int i = 0; i < 20; i++) {
        char k[24]; snprintf(k, sizeof k, "t%d_%d", id, i);
        UN(WritePrivateProfileStringA)("s", k, "v", g19_sf);
    }
    return 0;
}
TEST(gap19_wpps_crosses_threads) {
    REQUIRE(UN(WritePrivateProfileStringA)); REQUIRE(UN(GetPrivateProfileStringA));
    g19_inipath("thr.ini", g19_sf, sizeof g19_sf);
    HANDLE th[4];
    for (int i = 0; i < 4; i++) th[i] = CreateThread(NULL, 0, g19_wk, (LPVOID)(INT_PTR)i, 0, NULL);
    WaitForMultipleObjects(4, th, TRUE, 20000);
    for (int i = 0; i < 4; i++) CloseHandle(th[i]);
    int found = 0;
    for (int t = 0; t < 4; t++) for (int i = 0; i < 20; i++) {
        char k[24], o[8]; snprintf(k, sizeof k, "t%d_%d", t, i);
        g19_get(g19_sf, "s", k, o, sizeof o);
        if (!strcmp(o, "v")) found++;
    }
    t_note("threaded writes visible: %d / 80", found);
    CHECK(found > 0);
    DeleteFileA(g19_sf);
}

/* ---- WriteProfileSectionW: operates on win.ini, so no call is made ---- */
TEST(gap19_writeprofilesectionw_export_only) {
    /* The call forwards to WritePrivateProfileSectionW(app, str, NULL) = %WINDIR%\win.ini.
     * Calling it would modify a system file, so only the export is verified. */
    FARPROC p = (FARPROC)UK(WriteProfileSectionW);
    CHECK(p != NULL);
    FARPROC q = (FARPROC)UK(WriteProfileSectionA);
    CHECK(q != NULL);
    CHECK(p != q);
}
