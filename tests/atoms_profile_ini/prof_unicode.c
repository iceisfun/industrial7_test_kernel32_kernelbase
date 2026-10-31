/* INI files in different encodings: ANSI, UTF-16LE with BOM, UTF-8 with BOM. */
#include "ap.h"

static void np(char *path, const char *leaf) { t_tmpfile(path, MAX_PATH, leaf); ap_rm(path); }
/* Write a UTF-16LE file with BOM from an ASCII/escape-free wide literal. */
static void put_u16(const char *path, const WCHAR *s, int bom) {
    size_t n = ap_wlen(s);
    unsigned char *buf = malloc(2 + n * 2); size_t o = 0;
    if (bom) { buf[o++] = 0xFF; buf[o++] = 0xFE; }
    memcpy(buf + o, s, n * 2); o += n * 2;
    ap_put(path, buf, o);
    free(buf);
}
static void u16_to_utf8ish(const char *path, char *out, size_t cap) { /* hex dump of first bytes */
    unsigned char b[64]; long n = ap_get(path, (char *)b, sizeof b); size_t o = 0;
    for (long i = 0; i < n && o + 4 < cap; i++) o += snprintf(out + o, 4, "%02x", b[i]);
    out[o] = 0;
}

TEST(profile_unicode_utf16_file_read_w) {
    REQUIRE(U(GetPrivateProfileStringW));
    char path[MAX_PATH]; WCHAR wp[MAX_PATH], b[64]; np(path, "u1.ini"); ap_wpath(wp, path);
    put_u16(path, L"[sec]\r\nkey=value\r\nuni=Ж中é\r\n[two]\r\nk2=v2\r\n", 1);
    DWORD r = U(GetPrivateProfileStringW)(L"sec", L"key", L"DEF", b, 64, wp);
    EXPECT_EQ(r, 5); CHECK(!lstrcmpW(b, L"value"));
    r = U(GetPrivateProfileStringW)(L"sec", L"uni", L"DEF", b, 64, wp);
    EXPECT_EQ(r, 3); CHECK(!lstrcmpW(b, L"Ж中é"));
    r = U(GetPrivateProfileStringW)(L"two", L"k2", L"DEF", b, 64, wp);
    EXPECT_EQ(r, 2); CHECK(!lstrcmpW(b, L"v2"));
    r = U(GetPrivateProfileStringW)(L"sec", L"missing", L"DEF", b, 64, wp);
    EXPECT_EQ(r, 3);
    ap_rm(path);
}

TEST(profile_unicode_utf16_file_read_a) {
    REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH]; np(path, "u2.ini");
    put_u16(path, L"[sec]\r\nkey=value\r\nuni=Ж\r\n", 1);
    EXPECT_STR(ap_val("sec", "key", path), "value");
    t_note("A read of non-ACP char in UTF-16 file: \"%s\"", ap_val("sec", "uni", path));
    ap_rm(path);
}

TEST(profile_unicode_utf16_names_and_sections) {
    REQUIRE(U(GetPrivateProfileStringW)); REQUIRE(U(GetPrivateProfileSectionW)); REQUIRE(U(GetPrivateProfileSectionNamesW));
    char path[MAX_PATH]; WCHAR wp[MAX_PATH], b[128]; np(path, "u3.ini"); ap_wpath(wp, path);
    put_u16(path, L"[a]\r\nx=1\r\ny=2\r\n[b]\r\nz=3\r\n", 1);
    EXPECT_EQ(U(GetPrivateProfileSectionNamesW)(b, 128, wp), 4);
    EXPECT_EQ(b[0], 'a'); EXPECT_EQ(b[2], 'b');
    EXPECT_EQ(U(GetPrivateProfileSectionW)(L"a", b, 128, wp), 8);
    EXPECT_EQ(U(GetPrivateProfileStringW)(L"a", NULL, L"", b, 128, wp), 4);
    EXPECT_EQ(U(GetPrivateProfileIntW)(L"b", L"z", 0, wp), 3);
    ap_rm(path);
}

TEST(profile_unicode_utf16_line_endings) {
    REQUIRE(U(GetPrivateProfileStringW));
    const WCHAR *c[] = { L"[s]\r\nk=1\r\n", L"[s]\nk=1\n", L"[s]\rk=1\r", L"[s]\r\nk=1" };
    char path[MAX_PATH]; WCHAR wp[MAX_PATH], b[16]; np(path, "u4.ini"); ap_wpath(wp, path);
    for (int i = 0; i < 4; i++) {
        put_u16(path, c[i], 1);
        CHECKF(U(GetPrivateProfileStringW)(L"s", L"k", L"DEF", b, 16, wp) == 1 && b[0] == '1', "ending %d", i);
    }
    ap_rm(path);
}

TEST(profile_unicode_utf16_without_bom_is_not_unicode) {
    REQUIRE(U(GetPrivateProfileStringW));
    char path[MAX_PATH]; WCHAR wp[MAX_PATH], b[16]; np(path, "u5.ini"); ap_wpath(wp, path);
    put_u16(path, L"[s]\r\nk=1\r\n", 0);
    DWORD r = U(GetPrivateProfileStringW)(L"s", L"k", L"DEF", b, 16, wp);
    t_note("UTF-16 without BOM: ret=%lu", r);
    CHECK(r == 3 || r == 1);
    ap_rm(path);
}

TEST(profile_unicode_utf16be_bom_note) {
    REQUIRE(U(GetPrivateProfileStringW));
    char path[MAX_PATH]; WCHAR wp[MAX_PATH], b[16]; np(path, "u6.ini"); ap_wpath(wp, path);
    const unsigned char d[] = { 0xFE, 0xFF, 0, '[', 0, 's', 0, ']', 0, '\r', 0, '\n', 0, 'k', 0, '=', 0, '1', 0, '\r', 0, '\n' };
    ap_put(path, d, sizeof d);
    DWORD r = U(GetPrivateProfileStringW)(L"s", L"k", L"DEF", b, 16, wp);
    t_note("UTF-16BE BOM file: ret=%lu", r);
    ap_rm(path);
}

TEST(profile_unicode_utf8_bom_treated_as_ansi) {
    REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH]; np(path, "u7.ini");
    ap_puts(path, "\xEF\xBB\xBF[first]\r\na=1\r\n[second]\r\nb=caf\xc3\xa9\r\n");
    if (!t_is_wine()) EXPECT_STR(ap_val("second", "b", path), "caf\xc3\xa9");      /* bytes pass through untouched */
    t_note("first section on BOM line: %s", ap_val("first", "a", path));
    ap_rm(path);
}

TEST(profile_unicode_utf8_bom_file_write_keeps_bytes) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "u8.ini"); char b[128];
    ap_puts(path, "\xEF\xBB\xBF[first]\r\na=1\r\n");
    CHECK(ap_wps("second", "b", "2", path));
    ap_get(path, b, sizeof b);
    t_note("UTF-8 BOM file after write starts with %02x%02x%02x", (unsigned char)b[0], (unsigned char)b[1], (unsigned char)b[2]);
    EXPECT_STR(ap_val("second", "b", path), "2");
    ap_rm(path);
}

TEST(profile_unicode_write_to_existing_utf16_keeps_utf16) {
    REQUIRE(U(WritePrivateProfileStringW)); REQUIRE(U(GetPrivateProfileStringW));
    char path[MAX_PATH]; WCHAR wp[MAX_PATH], b[64]; np(path, "u9.ini"); ap_wpath(wp, path);
    put_u16(path, L"[s]\r\na=1\r\n", 1);
    EXPECT_EQ(U(WritePrivateProfileStringW)(L"s", L"b", L"Ж中", wp), TRUE);
    unsigned char raw[256]; long n = ap_get(path, (char *)raw, sizeof raw);
    CHECK(n >= 4);
    EXPECT_EQ(raw[0], 0xFF); EXPECT_EQ(raw[1], 0xFE);
    EXPECT_EQ(n % 2, 0);
    DWORD r = U(GetPrivateProfileStringW)(L"s", L"b", L"DEF", b, 64, wp);
    EXPECT_EQ(r, 2); CHECK(!lstrcmpW(b, L"Ж中"));
    r = U(GetPrivateProfileStringW)(L"s", L"a", L"DEF", b, 64, wp);
    EXPECT_EQ(r, 1);
    char hex[200]; u16_to_utf8ish(path, hex, sizeof hex);
    t_note("utf16 file head: %s", hex);
    ap_rm(path);
}

TEST(profile_unicode_ansi_write_to_existing_utf16_keeps_utf16) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "u10.ini");
    put_u16(path, L"[s]\r\na=1\r\n", 1);
    EXPECT_EQ(ap_wps("s", "b", "two", path), TRUE);
    unsigned char raw[256]; long n = ap_get(path, (char *)raw, sizeof raw);
    EXPECT_EQ(raw[0], 0xFF); EXPECT_EQ(raw[1], 0xFE);
    EXPECT_EQ(n % 2, 0);
    EXPECT_STR(ap_val("s", "b", path), "two");
    EXPECT_STR(ap_val("s", "a", path), "1");
    ap_rm(path);
}

TEST(profile_unicode_delete_in_utf16_file) {
    REQUIRE(U(WritePrivateProfileStringW)); REQUIRE(U(GetPrivateProfileStringW));
    char path[MAX_PATH]; WCHAR wp[MAX_PATH], b[64]; np(path, "u11.ini"); ap_wpath(wp, path);
    put_u16(path, L"[s]\r\na=1\r\nb=2\r\n[t]\r\nc=3\r\n", 1);
    EXPECT_EQ(U(WritePrivateProfileStringW)(L"s", L"a", NULL, wp), TRUE);
    EXPECT_EQ(U(GetPrivateProfileStringW)(L"s", L"a", L"DEF", b, 64, wp), 3);
    EXPECT_EQ(U(GetPrivateProfileStringW)(L"s", L"b", L"DEF", b, 64, wp), 1);
    EXPECT_EQ(U(WritePrivateProfileStringW)(L"s", NULL, NULL, wp), TRUE);
    EXPECT_EQ(U(GetPrivateProfileStringW)(L"s", L"b", L"DEF", b, 64, wp), 3);
    EXPECT_EQ(U(GetPrivateProfileStringW)(L"t", L"c", L"DEF", b, 64, wp), 1);
    unsigned char raw[8]; ap_get(path, (char *)raw, sizeof raw);
    EXPECT_EQ(raw[0], 0xFF); EXPECT_EQ(raw[1], 0xFE);
    ap_rm(path);
}

TEST(profile_unicode_section_write_in_utf16_file) {
    REQUIRE(U(WritePrivateProfileSectionW)); REQUIRE(U(GetPrivateProfileStringW));
    char path[MAX_PATH]; WCHAR wp[MAX_PATH], b[64]; np(path, "u12.ini"); ap_wpath(wp, path);
    put_u16(path, L"[s]\r\na=1\r\n", 1);
    static const WCHAR d[] = { 'n','=',0x416,0,'m','=','2',0,0 };
    EXPECT_EQ(U(WritePrivateProfileSectionW)(L"s", d, wp), TRUE);
    EXPECT_EQ(U(GetPrivateProfileStringW)(L"s", L"n", L"DEF", b, 64, wp), 1);
    CHECK(b[0] == 0x416);
    EXPECT_EQ(U(GetPrivateProfileStringW)(L"s", L"a", L"DEF", b, 64, wp), 3);
    unsigned char raw[4]; ap_get(path, (char *)raw, sizeof raw);
    EXPECT_EQ(raw[0], 0xFF); EXPECT_EQ(raw[1], 0xFE);
    ap_rm(path);
}

TEST(profile_unicode_struct_in_utf16_file) {
    REQUIRE(U(WritePrivateProfileStructW)); REQUIRE(U(GetPrivateProfileStructW));
    char path[MAX_PATH]; WCHAR wp[MAX_PATH]; np(path, "u13.ini"); ap_wpath(wp, path); char buf[16] = { 0 };
    put_u16(path, L"[s]\r\n", 1);
    EXPECT_EQ(U(WritePrivateProfileStructW)(L"s", L"k", (void *)"abacus", 7, wp), TRUE);
    EXPECT_EQ(U(GetPrivateProfileStructW)(L"s", L"k", buf, 7, wp), TRUE);
    EXPECT_STR(buf, "abacus");
    ap_rm(path);
}

TEST(profile_unicode_bom_only_file_note) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "u14.ini");
    const unsigned char bom[] = { 0xFF, 0xFE }; ap_put(path, bom, 2);
    EXPECT_EQ(ap_wps("s", "k", "v", path), TRUE);
    unsigned char raw[16]; long n = ap_get(path, (char *)raw, sizeof raw);
    t_note("BOM-only file after write: %ld bytes, head %02x %02x %02x %02x", n, raw[0], raw[1], raw[2], raw[3]);
    EXPECT_STR(ap_val("s", "k", path), "v");
    ap_rm(path);
}

TEST(profile_unicode_empty_file_write_is_ansi) {
    REQUIRE(U(WritePrivateProfileStringW));
    char path[MAX_PATH]; WCHAR wp[MAX_PATH]; np(path, "u15.ini"); ap_wpath(wp, path);
    ap_put(path, "", 0);
    EXPECT_EQ(U(WritePrivateProfileStringW)(L"s", L"k", L"v", wp), TRUE);
    char b[64]; ap_get(path, b, sizeof b);
    EXPECT_STR(b, "[s]\r\nk=v\r\n");
    ap_rm(path);
}

TEST(profile_unicode_ansi_file_wide_roundtrip_in_acp) {
    REQUIRE(U(WritePrivateProfileStringW)); REQUIRE(U(GetPrivateProfileStringW));
    char path[MAX_PATH]; WCHAR wp[MAX_PATH], b[64]; np(path, "u16.ini"); ap_wpath(wp, path);
    CHECK(U(WritePrivateProfileStringW)(L"s", L"k", L"café", wp));
    DWORD r = U(GetPrivateProfileStringW)(L"s", L"k", L"DEF", b, 64, wp);
    EXPECT_EQ(r, 4);
    if (GetACP() == 1252) CHECK(!lstrcmpW(b, L"café"));
    CHECK(U(WritePrivateProfileStringW)(L"s", L"k2", L"中", wp));
    r = U(GetPrivateProfileStringW)(L"s", L"k2", L"DEF", b, 64, wp);
    t_note("CJK char in ANSI file (ACP %u): ret %lu value %04x", GetACP(), r, b[0]);
    ap_rm(path);
}

TEST(profile_unicode_utf16_large_file) {
    REQUIRE(U(GetPrivateProfileStringW));
    char path[MAX_PATH]; WCHAR wp[MAX_PATH], b[64]; np(path, "u17.ini"); ap_wpath(wp, path);
    size_t cap = 200000; WCHAR *w = malloc(cap * sizeof(WCHAR)); size_t o = 0;
    o += wsprintfW(w + o, L"[big]\r\n");
    for (int i = 0; i < 5000; i++) o += wsprintfW(w + o, L"key%d=value%d\r\n", i, i);
    w[o] = 0;
    put_u16(path, w, 1); free(w);
    WCHAR k[16], want[24];
    wsprintfW(k, L"key%d", 4999); wsprintfW(want, L"value%d", 4999);
    DWORD r = U(GetPrivateProfileStringW)(L"big", k, L"DEF", b, 64, wp);
    CHECK(!lstrcmpW(b, want));
    EXPECT_EQ(r, 9);
    wsprintfW(k, L"key%d", 0);
    EXPECT_EQ(U(GetPrivateProfileStringW)(L"big", k, L"DEF", b, 64, wp), 6);
    ap_rm(path);
}

TEST(profile_unicode_wide_filename) {
    REQUIRE(U(WritePrivateProfileStringW)); REQUIRE(U(GetPrivateProfileStringW));
    WCHAR wp[MAX_PATH], b[16];
    WCHAR leaf[] = L"k32t_Ж中é.ini";
    wsprintfW(wp, L"%S%s", t_tmpdir(), leaf);
    DeleteFileW(wp);
    EXPECT_EQ(U(WritePrivateProfileStringW)(L"s", L"k", L"v", wp), TRUE);
    CHECK(GetFileAttributesW(wp) != INVALID_FILE_ATTRIBUTES);
    EXPECT_EQ(U(GetPrivateProfileStringW)(L"s", L"k", L"DEF", b, 16, wp), 1);
    DeleteFileW(wp);
}

TEST(profile_unicode_filename_with_spaces_and_dots) {
    REQUIRE(U(WritePrivateProfileStringA));
    const char *leaves[] = { "name with spaces.ini", "multi.dot.name.ini", "noext", "UPPER.INI", "a.ini.bak" };
    for (int i = 0; i < 5; i++) {
        char path[MAX_PATH]; np(path, leaves[i]);
        CHECKF(ap_wps("s", "k", "v", path), "%s", leaves[i]);
        CHECKF(!strcmp(ap_val("s", "k", path), "v"), "%s", leaves[i]);
        ap_rm(path);
    }
}
