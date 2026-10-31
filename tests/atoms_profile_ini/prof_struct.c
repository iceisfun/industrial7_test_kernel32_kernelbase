/* GetPrivateProfileStructA/W, WritePrivateProfileStructA/W */
#include "ap.h"

#define GST U(GetPrivateProfileStructA)
#define WST U(WritePrivateProfileStructA)

static void np(char *path, const char *leaf) { t_tmpfile(path, MAX_PATH, leaf); ap_rm(path); }
static int file_has(const char *path, const char *needle) { char b[4096]; ap_get(path, b, sizeof b); return strstr(b, needle) != NULL; }

TEST(profile_struct_write_format_hex_with_checksum) {
    REQUIRE(U(WritePrivateProfileStructA));
    char path[MAX_PATH]; np(path, "st1.ini"); char b[128];
    EXPECT_EQ(WST("s", "key", (void *)"abacus", sizeof("abacus"), path), TRUE);
    ap_get(path, b, sizeof b);
    EXPECT_STR(b, "[s]\r\nkey=616261637573006F\r\n");
    ap_rm(path);
}

TEST(profile_struct_checksum_values) {
    REQUIRE(U(WritePrivateProfileStructA));
    char path[MAX_PATH]; np(path, "st2.ini"); char b[128];
    unsigned char one[] = { 0x01 }, ff[] = { 0xFF }, two[] = { 0xFF, 0x02 }, zeros[4] = { 0 };
    CHECK(WST("s", "one", one, 1, path));
    CHECK(WST("s", "ff", ff, 1, path));
    CHECK(WST("s", "wrap", two, 2, path));
    CHECK(WST("s", "zero", zeros, 4, path));
    ap_get(path, b, sizeof b);
    CHECK(strstr(b, "one=0101\r\n"));
    CHECK(strstr(b, "ff=FFFF\r\n"));
    CHECK(strstr(b, "wrap=FF0201\r\n"));      /* 0xFF+0x02 = 0x101 -> 0x01 */
    CHECK(strstr(b, "zero=0000000000\r\n"));
    ap_rm(path);
}

TEST(profile_struct_roundtrip_small) {
    REQUIRE(U(WritePrivateProfileStructA)); REQUIRE(U(GetPrivateProfileStructA));
    char path[MAX_PATH]; np(path, "st3.ini"); char buf[20];
    CHECK(WST("s", "key", (void *)"abacus", 7, path));
    memset(buf, 0, sizeof buf);
    EXPECT_EQ(GST("s", "key", buf, 7, path), TRUE);
    EXPECT_STR(buf, "abacus");
    ap_rm(path);
}

TEST(profile_struct_size_mismatch_fails) {
    REQUIRE(U(WritePrivateProfileStructA)); REQUIRE(U(GetPrivateProfileStructA));
    char path[MAX_PATH]; np(path, "st4.ini"); char buf[40];
    CHECK(WST("s", "key", (void *)"abacus", 7, path));
    for (UINT n = 0; n < 20; n++) {
        if (n == 7) continue;
        memset(buf, 0x55, sizeof buf);
        SET_ERR(0);
        BOOL r = GST("s", "key", buf, n, path);
        CHECKF(!r, "size %u unexpectedly succeeded", n);
        if (n == 6) t_note("size mismatch error: %lu", GetLastError());
    }
    ap_rm(path);
}

TEST(profile_struct_bad_checksum_and_hex) {
    REQUIRE(U(GetPrivateProfileStructA));
    char path[MAX_PATH]; np(path, "st5.ini"); char buf[16];
    struct { const char *v; int ok; } c[] = {
        { "616261637573006F", 1 },        /* good */
        { "636163747573006F", 0 },        /* checksum mismatch */
        { "6361637475730083", 1 },        /* "cactus" good checksum */
        { "636163747573008Q", 0 },        /* non-hex */
        { "16361637475730083", 0 },       /* odd length */
        
        { "63616374757300", 0 },          /* too short, no checksum */
        { "", 0 },
        { "00", 0 },
    };
    for (unsigned i = 0; i < sizeof c / sizeof *c; i++) {
        char t[64]; snprintf(t, sizeof t, "[s]\r\nkey=%s\r\n", c[i].v);
        ap_ini(path, "st5.ini", t);
        memset(buf, 0, sizeof buf);
        BOOL r = GST("s", "key", buf, 7, path);
        CHECKF((r != 0) == c[i].ok, "case %u \"%s\": got %d", i, c[i].v, r);
    }
    ap_ini(path, "st5b.ini", "[s]\r\nkey=6361637475730083AA\r\n");
    t_note("hex with trailing extra byte accepted: %d", GST("s", "key", buf, 7, path));
    ap_rm(path);
}

TEST(profile_struct_lowercase_hex_note) {
    REQUIRE(U(GetPrivateProfileStructA));
    char path[MAX_PATH]; ap_ini(path, "st6.ini", "[s]\r\nkey=616261637573006f\r\n"); char buf[16] = { 0 };
    BOOL r = GST("s", "key", buf, 7, path);
    t_note("lower-case hex accepted: %d", r);
    ap_rm(path);
}

TEST(profile_struct_missing_key_section_file) {
    REQUIRE(U(GetPrivateProfileStructA));
    char path[MAX_PATH]; ap_ini(path, "st7.ini", "[s]\r\nkey=616261637573006F\r\n"); char buf[16];
    SET_ERR(0xdeadbeef);
    EXPECT_EQ(GST("s", "nokey", buf, 7, path), FALSE);
    EXPECT_EQ(GST("nosec", "key", buf, 7, path), FALSE);
    char gone[MAX_PATH]; np(gone, "st7gone.ini");
    EXPECT_EQ(GST("s", "key", buf, 7, gone), FALSE);
    ap_rm(path);
}

TEST(profile_struct_buffer_not_written_on_failure) {
    REQUIRE(U(GetPrivateProfileStructA));
    char path[MAX_PATH]; ap_ini(path, "st8.ini", "[s]\r\nkey=636163747573006F\r\n"); char buf[16];
    memset(buf, 0x77, sizeof buf);
    EXPECT_EQ(GST("s", "key", buf, 7, path), FALSE);     /* bad checksum */
    int touched = 0; for (int i = 8; i < 16; i++) if (buf[i] != 0x77) touched++;
    EXPECT_EQ(touched, 0);                                /* never writes past the struct */
    t_note("buffer bytes 0..6 after failed read: %02x %02x %02x", buf[0], buf[1], buf[2]);
    ap_rm(path);
}

TEST(profile_struct_all_byte_values_roundtrip) {
    REQUIRE(U(WritePrivateProfileStructA)); REQUIRE(U(GetPrivateProfileStructA));
    char path[MAX_PATH]; np(path, "st9.ini");
    unsigned char in[256], out[256];
    for (int i = 0; i < 256; i++) in[i] = (unsigned char)i;
    EXPECT_EQ(WST("s", "all", in, 256, path), TRUE);
    memset(out, 0xAA, sizeof out);
    EXPECT_EQ(GST("s", "all", out, 256, path), TRUE);
    CHECK(!memcmp(in, out, 256));
    ap_rm(path);
}

TEST(profile_struct_large_roundtrip) {
    REQUIRE(U(WritePrivateProfileStructA)); REQUIRE(U(GetPrivateProfileStructA));
    char path[MAX_PATH]; np(path, "st10.ini");
    static unsigned char in[4000], out[4000];
    unsigned x = 7; for (int i = 0; i < 4000; i++) { x = x * 1664525 + 1013904223; in[i] = x >> 24; }
    BOOL w = WST("s", "big", in, 4000, path);
    t_note("write 4000-byte struct -> %d", w);
    if (w) { EXPECT_EQ(GST("s", "big", out, 4000, path), TRUE); CHECK(!memcmp(in, out, 4000)); }
    ap_rm(path);
}

TEST(profile_struct_read_raw_hex_with_getprofilestring) {
    REQUIRE(U(WritePrivateProfileStructA));
    char path[MAX_PATH]; np(path, "st11.ini"); char b[64];
    CHECK(WST("s", "key", (void *)"abacus", 7, path));
    EXPECT_EQ(ap_gps("s", "key", "d", b, sizeof b, path), 16);
    EXPECT_STR(b, "616261637573006F");
    ap_rm(path);
}

TEST(profile_struct_write_null_data_deletes_key) {
    REQUIRE(U(WritePrivateProfileStructA));
    char path[MAX_PATH]; np(path, "st12.ini"); char b[64];
    CHECK(WST("s", "key", (void *)"abacus", 7, path));
    SET_ERR(0);
    BOOL r = WST("s", "key", NULL, 7, path);
    t_note("write struct NULL -> %d err=%lu", r, GetLastError());
    CHECK(r);
    ap_get(path, b, sizeof b);
    EXPECT_STR(b, "[s]\r\n");
    ap_rm(path);
}

TEST(profile_struct_write_zero_size_note) {
    REQUIRE(U(WritePrivateProfileStructA));
    char path[MAX_PATH]; np(path, "st13.ini"); char b[64];
    SET_ERR(0);
    BOOL r = WST("s", "key", (void *)"x", 0, path);
    ap_get(path, b, sizeof b);
    t_note("write struct size 0 -> %d err=%lu file=[%s]", r, GetLastError(), b);
    ap_rm(path);
}

TEST(profile_struct_write_replaces_existing) {
    REQUIRE(U(WritePrivateProfileStructA)); REQUIRE(U(GetPrivateProfileStructA));
    char path[MAX_PATH]; np(path, "st14.ini"); char buf[16];
    CHECK(WST("s", "key", (void *)"first", 6, path));
    CHECK(WST("s", "key", (void *)"second!", 8, path));
    EXPECT_EQ(GST("s", "key", buf, 8, path), TRUE);
    EXPECT_STR(buf, "second!");
    EXPECT_EQ(GST("s", "key", buf, 6, path), FALSE);
    ap_rm(path);
}

TEST(profile_struct_case_insensitive_names) {
    REQUIRE(U(WritePrivateProfileStructA)); REQUIRE(U(GetPrivateProfileStructA));
    char path[MAX_PATH]; np(path, "st15.ini"); char buf[16];
    CHECK(WST("Sec", "Key", (void *)"abc", 4, path));
    EXPECT_EQ(GST("SEC", "KEY", buf, 4, path), TRUE);
    EXPECT_STR(buf, "abc");
    ap_rm(path);
}

TEST(profile_struct_w_variant_roundtrip) {
    REQUIRE(U(WritePrivateProfileStructW)); REQUIRE(U(GetPrivateProfileStructW));
    char path[MAX_PATH]; WCHAR wp[MAX_PATH]; np(path, "st16.ini"); ap_wpath(wp, path); char buf[16], b[64];
    EXPECT_EQ(U(WritePrivateProfileStructW)(L"s", L"key", (void *)"abacus", 7, wp), TRUE);
    ap_get(path, b, sizeof b);
    EXPECT_STR(b, "[s]\r\nkey=616261637573006F\r\n");
    memset(buf, 0, sizeof buf);
    EXPECT_EQ(U(GetPrivateProfileStructW)(L"s", L"key", buf, 7, wp), TRUE);
    EXPECT_STR(buf, "abacus");
    EXPECT_EQ(U(GetPrivateProfileStructW)(L"s", L"key", buf, 6, wp), FALSE);
    EXPECT_EQ(U(GetPrivateProfileStructW)(L"s", L"nokey", buf, 7, wp), FALSE);
    EXPECT_EQ(U(WritePrivateProfileStructW)(L"s", L"key", NULL, 7, wp), TRUE);
    ap_get(path, b, sizeof b);
    EXPECT_STR(b, "[s]\r\n");
    ap_rm(path);
}

TEST(profile_struct_with_struct_type) {
    REQUIRE(U(WritePrivateProfileStructA)); REQUIRE(U(GetPrivateProfileStructA));
    struct S { int a; short b; char c[6]; double d; } in = { -5, 300, "hello", 3.25 }, out;
    char path[MAX_PATH]; np(path, "st17.ini");
    memset(&in.c[5], 0, 1);
    CHECK(WST("pos", "rec", &in, sizeof in, path));
    memset(&out, 0, sizeof out);
    CHECK(GST("pos", "rec", &out, sizeof out, path));
    CHECK(out.a == -5 && out.b == 300 && !strcmp(out.c, "hello") && out.d == 3.25);
    ap_rm(path);
}

TEST(profile_struct_bad_path) {
    REQUIRE(U(WritePrivateProfileStructA));
    char path[MAX_PATH]; t_tmpfile(path, sizeof path, "nodir_st\\x.ini");
    SET_ERR(0);
    EXPECT_EQ(WST("s", "k", (void *)"a", 2, path), FALSE);
    CHECK(GetLastError() != 0);
}

TEST(profile_struct_readonly_file_write_fails) {
    REQUIRE(U(WritePrivateProfileStructA));
    char path[MAX_PATH]; ap_ini(path, "st18.ini", "[s]\r\na=1\r\n");
    SetFileAttributesA(path, FILE_ATTRIBUTE_READONLY);
    EXPECT_EQ(WST("s", "k", (void *)"a", 2, path), FALSE);
    SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
    char b[64]; ap_get(path, b, sizeof b);
    EXPECT_STR(b, "[s]\r\na=1\r\n");
    ap_rm(path);
}
