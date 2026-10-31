/* WritePrivateProfileStringA/W and WritePrivateProfileSectionA/W: output format and edit semantics. */
#include "ap.h"

#define WPS U(WritePrivateProfileStringA)

static void file_is_(int line, const char *path, const char *want) {
    char b[8192]; long n = ap_get(path, b, sizeof b);
    int ok = n == (long)strlen(want) && !memcmp(b, want, n);
    if (!ok) {
        char e1[300], e2[300]; size_t i, o = 0;
        for (i = 0; i < (size_t)n && o < 280; i++) o += snprintf(e1 + o, 8, b[i] == '\r' ? "\\r" : b[i] == '\n' ? "\\n" : "%c", b[i]);
        e1[o] = 0; o = 0;
        for (i = 0; i < strlen(want) && o < 280; i++) o += snprintf(e2 + o, 8, want[i] == '\r' ? "\\r" : want[i] == '\n' ? "\\n" : "%c", want[i]);
        e2[o] = 0;
        t_check(0, __FILE__, line, "file content", "got [%s] want [%s]", e1, e2);
    } else t_check(1, __FILE__, line, "file content", NULL);
}
#define FILE_IS(p, w) file_is_(__LINE__, p, w)

static void np(char *path, const char *leaf) { t_tmpfile(path, MAX_PATH, leaf); ap_rm(path); }

TEST(profile_write_creates_file_and_section) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "w1.ini");
    SET_ERR(0xdeadbeef);
    EXPECT_EQ(WPS("App", "key", "string", path), TRUE);
    FILE_IS(path, "[App]\r\nkey=string\r\n");
    ap_rm(path);
}

TEST(profile_write_file_not_bom_and_ansi) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "w2.ini"); char b[64];
    CHECK(WPS("A", "k", "v", path));
    ap_get(path, b, sizeof b);
    CHECK((unsigned char)b[0] == '[');
    ap_rm(path);
}

TEST(profile_write_new_file_from_wide_is_ansi) {
    REQUIRE(U(WritePrivateProfileStringW));
    char path[MAX_PATH]; WCHAR wp[MAX_PATH]; np(path, "w3.ini"); ap_wpath(wp, path);
    EXPECT_EQ(U(WritePrivateProfileStringW)(L"App", L"key", L"string", wp), TRUE);
    FILE_IS(path, "[App]\r\nkey=string\r\n");
    ap_rm(path);
}

TEST(profile_write_wide_nonascii_converted_with_acp) {
    REQUIRE(U(WritePrivateProfileStringW));
    char path[MAX_PATH]; WCHAR wp[MAX_PATH]; np(path, "w4.ini"); ap_wpath(wp, path);
    CHECK(U(WritePrivateProfileStringW)(L"S\u00e9c", L"k\u00e9y", L"caf\u00e9", wp));
    if (GetACP() == 1252) FILE_IS(path, "[S\xe9""c]\r\nk\xe9y=caf\xe9\r\n");
    else t_note("ACP %u: skipped exact bytes", GetACP());
    ap_rm(path);
}

TEST(profile_write_second_key_same_section) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "w5.ini");
    WPS("App1", "key1", "string1", path); WPS("App1", "key2", "string2", path);
    FILE_IS(path, "[App1]\r\nkey1=string1\r\nkey2=string2\r\n");
    ap_rm(path);
}

TEST(profile_write_build_up_and_delete_sequence) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "w6.ini");
    WPS("App1", "key1", "string1", path); WPS("App1", "key2", "string2", path);
    WPS("App1", "key3", "string3", path); WPS("App2", "key4", "string4", path);
    EXPECT_EQ(WPS("App3", "key5", "string5", path), TRUE);
    FILE_IS(path, "[App1]\r\nkey1=string1\r\nkey2=string2\r\nkey3=string3\r\n[App2]\r\nkey4=string4\r\n[App3]\r\nkey5=string5\r\n");
    EXPECT_EQ(WPS("App1", "key2", NULL, path), TRUE);
    FILE_IS(path, "[App1]\r\nkey1=string1\r\nkey3=string3\r\n[App2]\r\nkey4=string4\r\n[App3]\r\nkey5=string5\r\n");
    EXPECT_EQ(WPS("App1", "key2", NULL, path), TRUE);     /* deleting again is fine */
    FILE_IS(path, "[App1]\r\nkey1=string1\r\nkey3=string3\r\n[App2]\r\nkey4=string4\r\n[App3]\r\nkey5=string5\r\n");
    EXPECT_EQ(WPS("App1", NULL, "ignored", path), TRUE);  /* NULL key deletes the section */
    FILE_IS(path, "[App2]\r\nkey4=string4\r\n[App3]\r\nkey5=string5\r\n");
    EXPECT_EQ(WPS("App2", NULL, NULL, path), TRUE);
    FILE_IS(path, "[App3]\r\nkey5=string5\r\n");
    EXPECT_EQ(WPS("App3", "key5", NULL, path), TRUE);     /* last key removed, section header stays */
    FILE_IS(path, "[App3]\r\n");
    ap_rm(path);
}

TEST(profile_write_null_key_missing_section_creates_nothing) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "w7.ini");
    ap_puts(path, "[X]\r\na=1\r\n");
    EXPECT_EQ(WPS("Nope", NULL, "string", path), TRUE);
    FILE_IS(path, "[X]\r\na=1\r\n");
    EXPECT_EQ(WPS("Nope", "k", NULL, path), TRUE);
    FILE_IS(path, "[X]\r\na=1\r\n");
    EXPECT_EQ(WPS("X", "nokey", NULL, path), TRUE);
    FILE_IS(path, "[X]\r\na=1\r\n");
    ap_rm(path);
}

TEST(profile_write_empty_value) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "w8.ini");
    EXPECT_EQ(WPS("App", "key", "", path), TRUE);
    FILE_IS(path, "[App]\r\nkey=\r\n");
    EXPECT_STR(ap_val("App", "key", path), "");
    ap_rm(path);
}

TEST(profile_write_replace_in_place_keeps_order) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "w9.ini");
    ap_puts(path, "[s]\r\na=1\r\nb=2\r\nc=3\r\n[t]\r\na=x\r\n");
    CHECK(WPS("s", "b", "NEW", path));
    FILE_IS(path, "[s]\r\na=1\r\nb=NEW\r\nc=3\r\n[t]\r\na=x\r\n");
    CHECK(WPS("s", "b", "a-much-longer-value-than-before", path));
    FILE_IS(path, "[s]\r\na=1\r\nb=a-much-longer-value-than-before\r\nc=3\r\n[t]\r\na=x\r\n");
    CHECK(WPS("s", "b", "", path));
    FILE_IS(path, "[s]\r\na=1\r\nb=\r\nc=3\r\n[t]\r\na=x\r\n");
    ap_rm(path);
}

TEST(profile_write_same_value_twice_idempotent) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "w10.ini");
    WPS("s", "a", "1", path); WPS("s", "a", "1", path); WPS("s", "a", "1", path);
    FILE_IS(path, "[s]\r\na=1\r\n");
    ap_rm(path);
}

TEST(profile_write_appends_key_to_end_of_section) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "w11.ini");
    ap_puts(path, "[s]\r\na=1\r\n[t]\r\nb=2\r\n");
    CHECK(WPS("s", "new", "9", path));
    FILE_IS(path, "[s]\r\na=1\r\nnew=9\r\n[t]\r\nb=2\r\n");
    CHECK(WPS("t", "new2", "8", path));
    FILE_IS(path, "[s]\r\na=1\r\nnew=9\r\n[t]\r\nb=2\r\nnew2=8\r\n");
    ap_rm(path);
}

TEST(profile_write_case_insensitive_section_and_key) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "w12.ini");
    ap_puts(path, "[Section]\r\nKey=1\r\n");
    CHECK(WPS("SECTION", "key", "2", path));
    char b[256]; ap_get(path, b, sizeof b);
    int sections = 0, keys = 0;
    for (char *p = b; (p = strstr(p, "[")) != NULL; p++) sections++;
    for (char *p = b; *p; p++) if ((*p == 'k' || *p == 'K') && (p[1] == 'e' || p[1] == 'E') && (p[2] == 'y' || p[2] == 'Y') && p[3] == '=') keys++;
    EXPECT_EQ(sections, 1);
    EXPECT_EQ(keys, 1);
    EXPECT_STR(ap_val("Section", "Key", path), "2");
    t_note("file after case-insensitive replace: %s", b);
    ap_rm(path);
}

static void strip_blank(char *s) {
    char *o = s; for (char *p = s; *p; p++) { if (*p == '\r' && p[1] == '\n' && (p == s || p[-1] == '\n')) { p++; continue; } *o++ = *p; } *o = 0;
}
TEST(profile_write_preserves_comments_and_blank_lines) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "w13.ini"); char b[512];
    ap_puts(path, "; top comment\r\n\r\n[s]\r\n; inside\r\na=1\r\n\r\nb=2\r\n[t]\r\n;t comment\r\nc=3\r\n");
    CHECK(WPS("s", "a", "9", path));
    ap_get(path, b, sizeof b);
    int exact = !strcmp(b, "; top comment\r\n\r\n[s]\r\n; inside\r\na=9\r\n\r\nb=2\r\n[t]\r\n;t comment\r\nc=3\r\n");
    strip_blank(b);
    CHECK(!strcmp(b, "; top comment\r\n[s]\r\n; inside\r\na=9\r\nb=2\r\n[t]\r\n;t comment\r\nc=3\r\n"));
    t_note("blank lines preserved on edit: %d", exact);
    CHECK(WPS("t", "d", "4", path));
    ap_get(path, b, sizeof b); strip_blank(b);
    CHECK(!strcmp(b, "; top comment\r\n[s]\r\n; inside\r\na=9\r\nb=2\r\n[t]\r\n;t comment\r\nc=3\r\nd=4\r\n"));
    ap_rm(path);
}

TEST(profile_write_preserves_unknown_lines) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "w14.ini");
    ap_puts(path, "[s]\r\nnoequals\r\na=1\r\n  indented = 2\r\n");
    CHECK(WPS("s", "a", "5", path));
    char b[256]; ap_get(path, b, sizeof b);
    CHECK(strstr(b, "noequals\r\n") != NULL);
    CHECK(strstr(b, "a=5\r\n") != NULL);
    CHECK(strstr(b, "indented") != NULL);
    ap_rm(path);
}

TEST(profile_write_data_before_first_section_preserved) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "w15.ini");
    ap_puts(path, "preamble line\r\nanother=1\r\n[s]\r\na=1\r\n");
    CHECK(WPS("s", "b", "2", path));
    FILE_IS(path, "preamble line\r\nanother=1\r\n[s]\r\na=1\r\nb=2\r\n");
    CHECK(WPS("new", "c", "3", path));
    FILE_IS(path, "preamble line\r\nanother=1\r\n[s]\r\na=1\r\nb=2\r\n[new]\r\nc=3\r\n");
    ap_rm(path);
}

TEST(profile_write_no_trailing_newline_in_original) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "w16.ini"); char b[256];
    ap_puts(path, "[s]\r\na=1");
    CHECK(WPS("s", "b", "2", path));
    EXPECT_STR(ap_val("s", "a", path), "1");
    EXPECT_STR(ap_val("s", "b", path), "2");
    ap_get(path, b, sizeof b);
    t_note("file after append to unterminated: [%s] len %zu", b, strlen(b));
    ap_puts(path, "[s]\r\na=1");
    CHECK(WPS("t", "c", "3", path));
    EXPECT_STR(ap_val("s", "a", path), "1");
    EXPECT_STR(ap_val("t", "c", path), "3");
    ap_rm(path);
}

TEST(profile_write_lf_only_original) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "w17.ini"); char b[256];
    ap_puts(path, "[s]\na=1\nb=2\n");
    CHECK(WPS("s", "c", "3", path));
    EXPECT_STR(ap_val("s", "a", path), "1");
    EXPECT_STR(ap_val("s", "b", path), "2");
    EXPECT_STR(ap_val("s", "c", path), "3");
    ap_get(path, b, sizeof b);
    t_note("LF file after append: has CRLF=%d", strstr(b, "\r\n") != NULL);
    ap_rm(path);
}

TEST(profile_write_value_whitespace_and_quotes) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "w18.ini"); char b[256];
    CHECK(WPS("s", "lead", "  v", path));
    CHECK(WPS("s", "trail", "v  ", path));
    CHECK(WPS("s", "quoted", "\"in quotes\"", path));
    CHECK(WPS("s", "padq", "\"  pad  \"", path));
    ap_get(path, b, sizeof b);
    t_note("written file: %s", b);
    EXPECT_STR(ap_val("s", "lead", path), "v");
    EXPECT_STR(ap_val("s", "trail", path), "v");
    EXPECT_STR(ap_val("s", "quoted", path), "in quotes");
    EXPECT_STR(ap_val("s", "padq", path), "  pad  ");
    ap_rm(path);
}

TEST(profile_write_value_with_special_chars) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "w19.ini");
    const char *vals[] = { "a=b", "a;b", "[x]", "100%", "C:\\Program Files\\x", "tab\tin", "two  spaces", "'single'", "\xe9\xe8", "x=y=z" };
    for (unsigned i = 0; i < sizeof vals / sizeof *vals; i++) {
        char k[16]; snprintf(k, sizeof k, "k%u", i);
        CHECK(WPS("s", k, vals[i], path));
    }
    for (unsigned i = 0; i < sizeof vals / sizeof *vals; i++) {
        char k[16]; snprintf(k, sizeof k, "k%u", i);
        const char *g = ap_val("s", k, path);
        if (strcmp(g, vals[i])) { if (i == 7) t_note("single-quoted value read back as \"%s\"", g); else CHECKF(0, "value %u: wrote \"%s\" read \"%s\"", i, vals[i], g); }
    }
    ap_rm(path);
}

TEST(profile_write_section_and_key_names_with_spaces) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "w20.ini");
    CHECK(WPS("my section", "my key", "my value", path));
    FILE_IS(path, "[my section]\r\nmy key=my value\r\n");
    EXPECT_STR(ap_val("my section", "my key", path), "my value");
    EXPECT_STR(ap_val("MY SECTION", "MY KEY", path), "my value");
    ap_rm(path);
}

TEST(profile_write_names_with_brackets_note) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "w21.ini"); char b[128];
    CHECK(WPS("a]b", "k", "v", path));
    ap_get(path, b, sizeof b);
    t_note("section 'a]b' file: %s | read back: %s", b, ap_val("a]b", "k", path));
    ap_rm(path);
    CHECK(WPS("s", "k]x", "v", path));
    t_note("key 'k]x' read back: %s", ap_val("s", "k]x", path));
    ap_rm(path);
}

TEST(profile_write_value_with_embedded_newline_note) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "w22.ini"); char b[128];
    CHECK(WPS("s", "k", "line1\r\nline2", path));
    ap_get(path, b, sizeof b);
    t_note("value with newline written as: %s", b);
    ap_rm(path);
}

TEST(profile_write_value_length_limits) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "w23.ini");
    static char v[40000], r[40000];
    memset(v, 'v', 32767); v[32767] = 0;
    BOOL ok = WPS("s", "big", v, path);
    t_note("write of 32767-char value -> %d", ok);
    DWORD got = ap_gps("s", "big", "DEF", r, sizeof r, path);
    t_note("read back %lu chars", got);
    CHECK(got == 0 || got == 32767 || got == 3);
    memset(v, 'w', 39999); v[39999] = 0;
    SET_ERR(0);
    ok = WPS("s", "huge", v, path);
    t_note("write of 39999-char value -> %d err=%lu", ok, GetLastError());
    got = ap_gps("s", "huge", "DEF", r, sizeof r, path);
    t_note("read back %lu chars", got);
    ap_rm(path);
}

TEST(profile_write_many_sequential_keys) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "w24.ini");
    int bad = 0;
    for (int i = 0; i < 300; i++) { char k[16], v[16]; snprintf(k, sizeof k, "key%d", i); snprintf(v, sizeof v, "val%d", i); if (!WPS("s", k, v, path)) bad++; }
    EXPECT_EQ(bad, 0);
    for (int i = 0; i < 300; i += 13) { char k[16], v[16]; snprintf(k, sizeof k, "key%d", i); snprintf(v, sizeof v, "val%d", i); if (strcmp(ap_val("s", k, path), v)) bad++; }
    EXPECT_EQ(bad, 0);
    for (int i = 0; i < 300; i++) { char k[16]; snprintf(k, sizeof k, "key%d", i); if (!WPS("s", k, NULL, path)) bad++; }
    EXPECT_EQ(bad, 0);
    FILE_IS(path, "[s]\r\n");
    ap_rm(path);
}

TEST(profile_write_many_sections) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "w25.ini"); int bad = 0;
    for (int i = 0; i < 200; i++) { char s[16]; snprintf(s, sizeof s, "sec%d", i); if (!WPS(s, "k", "v", path)) bad++; }
    EXPECT_EQ(bad, 0);
    char b[4096]; DWORD r = U(GetPrivateProfileSectionNamesA)(b, sizeof b, path);
    DWORD want = 0; for (int i = 0; i < 200; i++) want += (DWORD)snprintf(NULL, 0, "sec%d", i) + 1;
    EXPECT_EQ(r, want);
    for (int i = 0; i < 200; i += 2) { char s[16]; snprintf(s, sizeof s, "sec%d", i); WPS(s, NULL, NULL, path); }
    r = U(GetPrivateProfileSectionNamesA)(b, sizeof b, path);
    want = 0; for (int i = 1; i < 200; i += 2) want += (DWORD)snprintf(NULL, 0, "sec%d", i) + 1;
    EXPECT_EQ(r, want);
    ap_rm(path);
}

TEST(profile_write_returns_false_on_bad_path) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; t_tmpfile(path, sizeof path, "nodir_w\\x.ini");
    SET_ERR(0);
    EXPECT_EQ(WPS("s", "k", "v", path), FALSE);
    t_note("write into missing directory: err=%lu", GetLastError());
    CHECK(GetLastError() != 0);
    EXPECT_EQ(ap_exists(path), 0);
}

TEST(profile_write_empty_filename) {
    REQUIRE(U(WritePrivateProfileStringA));
    SET_ERR(0);
    EXPECT_EQ(WPS("App", "key", "string", ""), FALSE);
    t_note("write to empty file name: err=%lu", GetLastError());
}

TEST(profile_write_to_directory_fails) {
    REQUIRE(U(WritePrivateProfileStringA));
    SET_ERR(0);
    EXPECT_EQ(WPS("s", "k", "v", t_tmpdir()), FALSE);
    CHECK(GetLastError() != 0);
    t_note("write to a directory: err=%lu", GetLastError());
}

TEST(profile_write_readonly_file_fails_unchanged) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "w26.ini");
    ap_puts(path, "[s]\r\na=1\r\n");
    SetFileAttributesA(path, FILE_ATTRIBUTE_READONLY);
    SET_ERR(0);
    EXPECT_EQ(WPS("s", "a", "2", path), FALSE);
    t_note("write to read-only file: err=%lu", GetLastError());
    CHECK(GetLastError() != 0);
    FILE_IS(path, "[s]\r\na=1\r\n");
    SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
    ap_rm(path);
}

TEST(profile_write_null_app_with_key_fails) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "w27.ini");
    ap_puts(path, "[s]\r\na=1\r\n");
    SET_ERR(0xdeadbeef);
    BOOL r1 = WPS(NULL, "key", "string", path);
    t_note("NULL app name: ret=%d err=%lu", r1, GetLastError());
    if (!r1) FILE_IS(path, "[s]\r\na=1\r\n");
    ap_rm(path);
    BOOL r2 = WPS(NULL, "key", "string", path);
    t_note("NULL app name, no file: ret=%d exists=%d", r2, ap_exists(path));
    ap_rm(path);
}

TEST(profile_write_null_everything_flushes_note) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "w28.ini");
    ap_puts(path, "[s]\r\na=1\r\n");
    SET_ERR(0);
    BOOL r = WPS(NULL, NULL, NULL, path);
    t_note("(NULL,NULL,NULL) on existing file -> %d err=%lu", r, GetLastError());
    FILE_IS(path, "[s]\r\na=1\r\n");
    ap_rm(path);
}

TEST(profile_write_sharing_violation_exclusive_open) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "w29.ini");
    HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    SET_ERR(0);
    EXPECT_EQ(WPS("s", "k", "12345", path), FALSE);
    EXPECT_ERR(ERROR_SHARING_VIOLATION);
    CloseHandle(h);
    char b[16]; EXPECT_EQ(ap_gps("s", "k", NULL, b, sizeof b, path), 0);
    ap_rm(path);
}

TEST(profile_write_other_handle_with_full_sharing_ok) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "w30.ini");
    HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    SET_ERR(0);
    BOOL r = WPS("s", "k", "12345", path);
    if (!r) t_note("write with shared handle open failed: err=%lu", GetLastError());
    CloseHandle(h);
    if (r) EXPECT_EQ(ap_gps("s", "k", NULL, (char[16]){0}, 16, path), 5);
    ap_rm(path);
}

TEST(profile_write_file_attributes_after_create) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "w31.ini");
    CHECK(WPS("s", "k", "v", path));
    DWORD a = GetFileAttributesA(path);
    CHECK(a != INVALID_FILE_ATTRIBUTES);
    CHECK(!(a & FILE_ATTRIBUTE_DIRECTORY));
    CHECK(!(a & FILE_ATTRIBUTE_READONLY));
    ap_rm(path);
}

TEST(profile_write_preserves_hidden_attribute_note) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "w32.ini");
    ap_puts(path, "[s]\r\na=1\r\n");
    SetFileAttributesA(path, FILE_ATTRIBUTE_HIDDEN);
    BOOL r = WPS("s", "a", "2", path);
    t_note("write to hidden file -> %d attrs now 0x%lx", r, GetFileAttributesA(path));
    if (r) EXPECT_STR(ap_val("s", "a", path), "2");
    SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
    ap_rm(path);
}

TEST(profile_write_then_read_wide_parity) {
    REQUIRE(U(WritePrivateProfileStringA)); REQUIRE(U(WritePrivateProfileStringW)); REQUIRE(U(GetPrivateProfileStringW));
    char path[MAX_PATH]; WCHAR wp[MAX_PATH], b[64]; np(path, "w33.ini"); ap_wpath(wp, path);
    CHECK(WPS("a", "k", "ansi", path));
    CHECK(U(WritePrivateProfileStringW)(L"w", L"k", L"wide", wp));
    U(GetPrivateProfileStringW)(L"a", L"k", L"", b, 64, wp); CHECK(!lstrcmpW(b, L"ansi"));
    U(GetPrivateProfileStringW)(L"w", L"k", L"", b, 64, wp); CHECK(!lstrcmpW(b, L"wide"));
    EXPECT_STR(ap_val("w", "k", path), "wide");
    EXPECT_EQ(U(WritePrivateProfileStringW)(L"w", L"k", NULL, wp), TRUE);
    EXPECT_STR(ap_val("w", "k", path), "DEF");
    ap_rm(path);
}

/* ---- WritePrivateProfileSection ---- */
TEST(profile_write_section_creates) {
    REQUIRE(U(WritePrivateProfileSectionA));
    char path[MAX_PATH]; np(path, "ws1.ini");
    EXPECT_EQ(U(WritePrivateProfileSectionA)("sec", "a=1\0b=2\0", path), TRUE);
    FILE_IS(path, "[sec]\r\na=1\r\nb=2\r\n");
    ap_rm(path);
}

TEST(profile_write_section_replaces_whole_section) {
    REQUIRE(U(WritePrivateProfileSectionA));
    char path[MAX_PATH]; np(path, "ws2.ini");
    ap_puts(path, "[a]\r\nx=1\r\n[sec]\r\nold1=1\r\nold2=2\r\nold3=3\r\n[z]\r\ny=2\r\n");
    EXPECT_EQ(U(WritePrivateProfileSectionA)("sec", "new1=A\0new2=B\0", path), TRUE);
    FILE_IS(path, "[a]\r\nx=1\r\n[sec]\r\nnew1=A\r\nnew2=B\r\n[z]\r\ny=2\r\n");
    EXPECT_STR(ap_val("sec", "old1", path), "DEF");
    ap_rm(path);
}

TEST(profile_write_section_empty_data_clears_keys) {
    REQUIRE(U(WritePrivateProfileSectionA));
    char path[MAX_PATH]; np(path, "ws3.ini");
    ap_puts(path, "[sec]\r\nold=1\r\n[z]\r\ny=2\r\n");
    EXPECT_EQ(U(WritePrivateProfileSectionA)("sec", "", path), TRUE);
    char b[128]; ap_get(path, b, sizeof b);
    t_note("after empty section write: %s", b);
    EXPECT_STR(ap_val("sec", "old", path), "DEF");
    EXPECT_STR(ap_val("z", "y", path), "2");
    ap_rm(path);
}

TEST(profile_write_section_null_data) {
    REQUIRE(U(WritePrivateProfileSectionA));
    char path[MAX_PATH]; np(path, "ws4.ini");
    ap_puts(path, "[sec]\r\nold=1\r\n[z]\r\ny=2\r\n");
    SET_ERR(0);
    BOOL r = U(WritePrivateProfileSectionA)("sec", NULL, path);
    t_note("NULL data -> %d err=%lu", r, GetLastError());
    if (r) EXPECT_STR(ap_val("sec", "old", path), "DEF");
    EXPECT_STR(ap_val("z", "y", path), "2");
    ap_rm(path);
}

TEST(profile_write_section_entries_without_equals_and_spaces) {
    REQUIRE(U(WritePrivateProfileSectionA));
    char path[MAX_PATH]; np(path, "ws5.ini"); char b[128];
    CHECK(U(WritePrivateProfileSectionA)("sec", "plain\0k = v\0 padded \0", path));
    ap_get(path, b, sizeof b);
    t_note("written: %s", b);
    t_note("plain line kept: %d, spaced entry kept verbatim: %d", strstr(b, "plain\r\n") != NULL, strstr(b, "k = v\r\n") != NULL);
    CHECK(strstr(b, "[sec]\r\n") == b);
    ap_rm(path);
}

TEST(profile_write_section_case_insensitive_name) {
    REQUIRE(U(WritePrivateProfileSectionA));
    char path[MAX_PATH]; np(path, "ws6.ini");
    ap_puts(path, "[Sec]\r\nold=1\r\n");
    CHECK(U(WritePrivateProfileSectionA)("SEC", "n=2\0", path));
    EXPECT_STR(ap_val("sec", "n", path), "2");
    EXPECT_STR(ap_val("sec", "old", path), "DEF");
    char b[128]; DWORD r = U(GetPrivateProfileSectionNamesA)(b, sizeof b, path);
    EXPECT_EQ(r, 4);
    ap_rm(path);
}

TEST(profile_write_section_preserves_other_content) {
    REQUIRE(U(WritePrivateProfileSectionA));
    char path[MAX_PATH]; np(path, "ws7.ini");
    ap_puts(path, "; header\r\n[a]\r\nx=1\r\n; mid\r\n[b]\r\ny=2\r\n");
    CHECK(U(WritePrivateProfileSectionA)("b", "y=3\0z=4\0", path));
    char buf[256]; ap_get(path, buf, sizeof buf);
    CHECK(strstr(buf, "; header\r\n") == buf);
    CHECK(strstr(buf, "[a]\r\nx=1\r\n") != NULL);
    EXPECT_STR(ap_val("b", "y", path), "3");
    EXPECT_STR(ap_val("b", "z", path), "4");
    ap_rm(path);
}

TEST(profile_write_section_w_creates_ansi_file) {
    REQUIRE(U(WritePrivateProfileSectionW));
    char path[MAX_PATH]; WCHAR wp[MAX_PATH]; np(path, "ws8.ini"); ap_wpath(wp, path);
    static const WCHAR data[] = { 'a','=','1',0,'b','=','2',0,0 };
    EXPECT_EQ(U(WritePrivateProfileSectionW)(L"sec", data, wp), TRUE);
    FILE_IS(path, "[sec]\r\na=1\r\nb=2\r\n");
    ap_rm(path);
}

TEST(profile_write_section_bad_path) {
    REQUIRE(U(WritePrivateProfileSectionA));
    char path[MAX_PATH]; t_tmpfile(path, sizeof path, "nodir_ws\\x.ini");
    SET_ERR(0);
    EXPECT_EQ(U(WritePrivateProfileSectionA)("s", "a=1\0", path), FALSE);
    CHECK(GetLastError() != 0);
}

TEST(profile_write_section_then_section_get_roundtrip) {
    REQUIRE(U(WritePrivateProfileSectionA)); REQUIRE(U(GetPrivateProfileSectionA));
    char path[MAX_PATH]; np(path, "ws9.ini"); char b[128], f[128];
    CHECK(U(WritePrivateProfileSectionA)("rt", "one=1\0two=2\0three=3\0", path));
    DWORD r = U(GetPrivateProfileSectionA)("rt", b, sizeof b, path);
    EXPECT_EQ(r, 20);
    ap_flat(b, sizeof b, f, sizeof f);
    EXPECT_STR(f, "one=1|two=2|three=3");
    ap_rm(path);
}

TEST(profile_write_section_big) {
    REQUIRE(U(WritePrivateProfileSectionA));
    char path[MAX_PATH]; np(path, "ws10.ini");
    static char d[20000]; int n = 0;
    for (int i = 0; i < 1000; i++) n += snprintf(d + n, 24, "k%d=v%d", i, i) + 1;
    d[n++] = 0;
    CHECK(U(WritePrivateProfileSectionA)("big", d, path));
    EXPECT_STR(ap_val("big", "k0", path), "v0");
    EXPECT_STR(ap_val("big", "k999", path), "v999");
    ap_rm(path);
}
