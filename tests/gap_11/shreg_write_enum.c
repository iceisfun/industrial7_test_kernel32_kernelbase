/* gap_11: SHRegWriteUSValue / SHRegSetUSValue / SHRegGetBoolUSValue / SHRegEnumUSValueW / SHRegQueryInfoUSKey.
 * Only the HKCU flags (SHREGSET_HKCU / SHREGSET_FORCE_HKCU) are ever used: nothing is written below HKLM. */
#include "g11.h"

#define OPENW U(SHRegOpenUSKeyW)
#define OPENA U(SHRegOpenUSKeyA)

static HUSKEY ow(const char *leaf, REGSAM sam) {
    WCHAR p[256]; HUSKEY h = NULL;
    g11_subw(p, 256, leaf);
    return OPENW(p, sam, NULL, &h, FALSE) == 0 ? h : NULL;
}
static HUSKEY oa(const char *leaf, REGSAM sam) {
    char p[256]; HUSKEY h = NULL;
    g11_sub(p, sizeof p, leaf);
    return OPENA(p, sam, NULL, &h, FALSE) == 0 ? h : NULL;
}
static int rd_dw(const char *leaf, const char *name, DWORD *out) {
    DWORD type = 0, cb = 4;
    LONG r = g11_rd(leaf, name, &type, out, &cb);
    return r == 0 && type == REG_DWORD && cb == 4;
}

/* ---------------- SHRegSetUSValue ---------------- */
TEST(gap11_shreg_set_w_creates_key_and_value) {
    REQUIRE(U(SHRegSetUSValueW));
    g11_clean();
    WCHAR p[256]; g11_subw(p, 256, "newkey");
    DWORD v = 0x1234;
    EQ(U(SHRegSetUSValueW)(p, L"val", REG_DWORD, &v, 4, SHREGSET_FORCE_HKCU), 0);
    DWORD got = 0; CHECK(rd_dw("newkey", "val", &got)); EQ(got, 0x1234);
    g11_clean();
}
TEST(gap11_shreg_set_a_creates_key_and_value) {
    REQUIRE(U(SHRegSetUSValueA));
    g11_clean();
    char p[256]; g11_sub(p, sizeof p, "newkey");
    DWORD v = 0x4321;
    EQ(U(SHRegSetUSValueA)(p, "val", REG_DWORD, &v, 4, SHREGSET_FORCE_HKCU), 0);
    DWORD got = 0; CHECK(rd_dw("newkey", "val", &got)); EQ(got, 0x4321);
    g11_clean();
}
TEST(gap11_shreg_set_nested_path_created) {
    REQUIRE(U(SHRegSetUSValueW));
    g11_clean();
    WCHAR p[256]; g11_subw(p, 256, "x\\y\\z");
    DWORD v = 9;
    EQ(U(SHRegSetUSValueW)(p, L"v", REG_DWORD, &v, 4, SHREGSET_HKCU), 0);
    DWORD got = 0; CHECK(rd_dw("x\\y\\z", "v", &got)); EQ(got, 9);
    g11_clean();
}
TEST(gap11_shreg_set_flag_hkcu_does_not_overwrite) {
    REQUIRE(U(SHRegSetUSValueW));
    g11_clean();
    WCHAR p[256]; g11_subw(p, 256, "k");
    DWORD v = 1, got = 0;
    EQ(U(SHRegSetUSValueW)(p, L"v", REG_DWORD, &v, 4, SHREGSET_HKCU), 0);
    CHECK(rd_dw("k", "v", &got)); EQ(got, 1);
    v = 2;
    EQ(U(SHRegSetUSValueW)(p, L"v", REG_DWORD, &v, 4, SHREGSET_HKCU), 0);   /* exists: untouched, success */
    CHECK(rd_dw("k", "v", &got)); EQ(got, 1);
    g11_clean();
}
TEST(gap11_shreg_set_flag_force_overwrites) {
    REQUIRE(U(SHRegSetUSValueW));
    g11_clean();
    WCHAR p[256]; g11_subw(p, 256, "k");
    DWORD v = 1, got = 0;
    EQ(U(SHRegSetUSValueW)(p, L"v", REG_DWORD, &v, 4, SHREGSET_FORCE_HKCU), 0);
    v = 2;
    EQ(U(SHRegSetUSValueW)(p, L"v", REG_DWORD, &v, 4, SHREGSET_FORCE_HKCU), 0);
    CHECK(rd_dw("k", "v", &got)); EQ(got, 2);
    v = 3;
    EQ(U(SHRegSetUSValueW)(p, L"v", REG_DWORD, &v, 4, SHREGSET_HKCU | SHREGSET_FORCE_HKCU), 0);
    CHECK(rd_dw("k", "v", &got)); EQ(got, 3);
    g11_clean();
}
TEST(gap11_shreg_set_flag_hkcu_a_does_not_overwrite) {
    REQUIRE(U(SHRegSetUSValueA));
    g11_clean();
    char p[256]; g11_sub(p, sizeof p, "k");
    DWORD v = 1, got = 0;
    EQ(U(SHRegSetUSValueA)(p, "v", REG_DWORD, &v, 4, SHREGSET_HKCU), 0);
    v = 2;
    EQ(U(SHRegSetUSValueA)(p, "v", REG_DWORD, &v, 4, SHREGSET_HKCU), 0);
    CHECK(rd_dw("k", "v", &got)); EQ(got, 1);
    v = 5;
    EQ(U(SHRegSetUSValueA)(p, "v", REG_DWORD, &v, 4, SHREGSET_FORCE_HKCU), 0);
    CHECK(rd_dw("k", "v", &got)); EQ(got, 5);
    g11_clean();
}
TEST(gap11_shreg_set_flags_without_hive_bits) {
    REQUIRE(U(SHRegSetUSValueW)); REQUIRE(U(SHRegSetUSValueA));
    g11_clean();
    WCHAR pw[256]; char pa[256]; g11_subw(pw, 256, "k"); g11_sub(pa, sizeof pa, "k");
    DWORD v = 1, flags[] = { 0, 0x10, 0x20, 0x100, 0xFFFFFFF0 };
    for (size_t i = 0; i < sizeof flags / sizeof flags[0]; i++) {
        /* creation requires at least one of the four SHREGSET_* bits */
        CHECKF(U(SHRegSetUSValueW)(pw, L"v", REG_DWORD, &v, 4, flags[i]) == ERROR_INVALID_PARAMETER, "W flags 0x%lx", flags[i]);
        CHECKF(U(SHRegSetUSValueA)(pa, "v", REG_DWORD, &v, 4, flags[i]) == ERROR_INVALID_PARAMETER, "A flags 0x%lx", flags[i]);
    }
    DWORD got; CHECK(!rd_dw("k", "v", &got));      /* nothing was created */
    char p2[256]; HKEY k;
    g11_sub(p2, sizeof p2, "k");
    CHECK(RegOpenKeyExA(HKEY_CURRENT_USER, p2, 0, KEY_READ, &k) != 0);
    g11_clean();
}
TEST(gap11_shreg_set_types) {
    REQUIRE(U(SHRegSetUSValueW));
    g11_clean();
    WCHAR p[256]; g11_subw(p, 256, "t");
    EQ(U(SHRegSetUSValueW)(p, L"sz", REG_SZ, L"text", 10, 2), 0);
    EQ(U(SHRegSetUSValueW)(p, L"bin", REG_BINARY, "\x01\x02\x03", 3, 2), 0);
    unsigned long long q = 0x0102030405060708ULL;
    EQ(U(SHRegSetUSValueW)(p, L"qw", REG_QWORD, &q, 8, 2), 0);
    EQ(U(SHRegSetUSValueW)(p, L"empty", REG_BINARY, NULL, 0, 2), 0);
    WCHAR ms[] = L"a\0b\0";
    EQ(U(SHRegSetUSValueW)(p, L"ms", REG_MULTI_SZ, ms, sizeof ms, 2), 0);
    DWORD type, cb; BYTE buf[64];
    cb = sizeof buf; EQ(g11_rd("t", "sz", &type, buf, &cb), 0); EQ(type, REG_SZ); EQ(cb, 10);
    cb = sizeof buf; EQ(g11_rd("t", "bin", &type, buf, &cb), 0); EQ(type, REG_BINARY); EQ(cb, 3);
    cb = sizeof buf; EQ(g11_rd("t", "qw", &type, buf, &cb), 0); EQ(type, REG_QWORD); EQ(cb, 8);
    cb = sizeof buf; EQ(g11_rd("t", "empty", &type, buf, &cb), 0); EQ(type, REG_BINARY); EQ(cb, 0);
    cb = sizeof buf; EQ(g11_rd("t", "ms", &type, buf, &cb), 0); EQ(type, REG_MULTI_SZ); EQ(cb, sizeof ms);
    g11_clean();
}
TEST(gap11_shreg_set_default_value_name) {
    REQUIRE(U(SHRegSetUSValueW)); REQUIRE(U(SHRegSetUSValueA));
    g11_clean();
    WCHAR p[256]; g11_subw(p, 256, "d");
    EQ(U(SHRegSetUSValueW)(p, NULL, REG_SZ, L"wd", 6, 2), 0);
    char buf[16]; DWORD type, cb = sizeof buf;
    EQ(g11_rd("d", "", &type, buf, &cb), 0); EQ(type, REG_SZ); EQ(cb, 6);
    char pa[256]; g11_sub(pa, sizeof pa, "d");
    EQ(U(SHRegSetUSValueA)(pa, "", REG_SZ, "ad", 3, 2), 0);
    WCHAR wb[8]; cb = sizeof wb;
    EQ(g11_rd("d", "", &type, wb, &cb), 0);
    g11_clean();
}
TEST(gap11_shreg_set_a_sz_stored_as_unicode) {
    REQUIRE(U(SHRegSetUSValueA)); REQUIRE(U(SHRegGetUSValueW));
    g11_clean();
    char pa[256]; WCHAR pw[256]; g11_sub(pa, sizeof pa, "u"); g11_subw(pw, 256, "u");
    EQ(U(SHRegSetUSValueA)(pa, "s", REG_SZ, "ansi", 5, 2), 0);
    WCHAR out[16]; DWORD cb = sizeof out, type = 0;
    EQ(U(SHRegGetUSValueW)(pw, L"s", &type, out, &cb, FALSE, NULL, 0), 0);
    EXPECT_WSTR(out, L"ansi"); EQ(cb, 10);
    g11_clean();
}
TEST(gap11_shreg_set_w_sz_read_as_ansi) {
    REQUIRE(U(SHRegSetUSValueW)); REQUIRE(U(SHRegGetUSValueA));
    g11_clean();
    char pa[256]; WCHAR pw[256]; g11_sub(pa, sizeof pa, "u"); g11_subw(pw, 256, "u");
    EQ(U(SHRegSetUSValueW)(pw, L"s", REG_SZ, L"wide", 10, 2), 0);
    char out[16]; DWORD cb = sizeof out, type = 0;
    EQ(U(SHRegGetUSValueA)(pa, "s", &type, out, &cb, FALSE, NULL, 0), 0);
    EXPECT_STR(out, "wide"); EQ(cb, 5);
    g11_clean();
}
TEST(gap11_shreg_set_long_path) {
    REQUIRE(U(SHRegSetUSValueW)); REQUIRE(U(SHRegSetUSValueA));
    WCHAR pw[400]; char pa[400]; DWORD v = 1;
    for (int i = 0; i < 399; i++) { pw[i] = L'q'; pa[i] = 'q'; } pw[399] = 0; pa[399] = 0;
    LONG r = U(SHRegSetUSValueW)(pw, L"v", REG_DWORD, &v, 4, 2);
    t_note("SHRegSetUSValueW(399 char path) -> %ld", (long)r);
    NE(r, 0);
    r = U(SHRegSetUSValueA)(pa, "v", REG_DWORD, &v, 4, 2);
    t_note("SHRegSetUSValueA(399 char path) -> %ld", (long)r);
    NE(r, 0);
}
TEST(gap11_shreg_set_readable_back_with_get) {
    REQUIRE(U(SHRegSetUSValueW)); REQUIRE(U(SHRegGetUSValueW));
    g11_clean();
    WCHAR p[256]; g11_subw(p, 256, "rb");
    for (DWORD i = 0; i < 50; i++) {
        WCHAR n[16]; wsprintfW(n, L"v%lu", i);
        EQ(U(SHRegSetUSValueW)(p, n, REG_DWORD, &i, 4, 2), 0);
    }
    for (DWORD i = 0; i < 50; i++) {
        WCHAR n[16]; wsprintfW(n, L"v%lu", i);
        DWORD v = 0xFFFF, cb = 4;
        EQ(U(SHRegGetUSValueW)(p, n, NULL, &v, &cb, FALSE, NULL, 0), 0);
        EQ(v, i);
    }
    g11_clean();
}

/* ---------------- SHRegWriteUSValue ---------------- */
TEST(gap11_shreg_write_w_force) {
    REQUIRE(U(SHRegWriteUSValueW)); REQUIRE(pClose);
    g11_clean(); g11_mkdw("w", "other", 0);
    HUSKEY h = ow("w", KEY_READ | KEY_WRITE); NE(h, NULL); if (!h) return;
    DWORD v = 11, got = 0;
    EQ(U(SHRegWriteUSValueW)(h, L"v", REG_DWORD, &v, 4, SHREGSET_FORCE_HKCU), 0);
    CHECK(rd_dw("w", "v", &got)); EQ(got, 11);
    v = 12;
    EQ(U(SHRegWriteUSValueW)(h, L"v", REG_DWORD, &v, 4, SHREGSET_FORCE_HKCU), 0);
    CHECK(rd_dw("w", "v", &got)); EQ(got, 12);
    pClose(h); g11_clean();
}
TEST(gap11_shreg_write_a_force) {
    REQUIRE(U(SHRegWriteUSValueA)); REQUIRE(pClose);
    g11_clean(); g11_mkdw("w", "other", 0);
    HUSKEY h = oa("w", KEY_READ | KEY_WRITE); NE(h, NULL); if (!h) return;
    DWORD v = 21, got = 0;
    EQ(U(SHRegWriteUSValueA)(h, "v", REG_DWORD, &v, 4, SHREGSET_FORCE_HKCU), 0);
    CHECK(rd_dw("w", "v", &got)); EQ(got, 21);
    pClose(h); g11_clean();
}
TEST(gap11_shreg_write_no_overwrite_flag) {
    REQUIRE(U(SHRegWriteUSValueW)); REQUIRE(U(SHRegWriteUSValueA)); REQUIRE(pClose);
    g11_clean(); g11_mkdw("w", "v", 1);
    HUSKEY h = ow("w", KEY_READ | KEY_WRITE); NE(h, NULL); if (!h) return;
    DWORD v = 2, got = 0;
    EQ(U(SHRegWriteUSValueW)(h, L"v", REG_DWORD, &v, 4, SHREGSET_HKCU), 0);
    CHECK(rd_dw("w", "v", &got)); EQ(got, 1);
    EQ(U(SHRegWriteUSValueA)(h, "v", REG_DWORD, &v, 4, SHREGSET_HKCU), 0);
    CHECK(rd_dw("w", "v", &got)); EQ(got, 1);
    EQ(U(SHRegWriteUSValueW)(h, L"fresh", REG_DWORD, &v, 4, SHREGSET_HKCU), 0);   /* absent: written */
    CHECK(rd_dw("w", "fresh", &got)); EQ(got, 2);
    pClose(h); g11_clean();
}
TEST(gap11_shreg_write_flags_zero_is_noop) {
    REQUIRE(U(SHRegWriteUSValueW)); REQUIRE(U(SHRegWriteUSValueA)); REQUIRE(pClose);
    g11_clean(); g11_mkdw("w", "v", 1);
    HUSKEY h = ow("w", KEY_READ | KEY_WRITE); NE(h, NULL); if (!h) return;
    DWORD v = 99, got = 0;
    EQ(U(SHRegWriteUSValueW)(h, L"v", REG_DWORD, &v, 4, 0), 0);
    EQ(U(SHRegWriteUSValueA)(h, "v", REG_DWORD, &v, 4, 0), 0);
    EQ(U(SHRegWriteUSValueW)(h, L"v", REG_DWORD, &v, 4, 0x10), 0);   /* no recognised bits */
    EQ(U(SHRegWriteUSValueW)(h, L"new", REG_DWORD, &v, 4, 0), 0);
    CHECK(rd_dw("w", "v", &got)); EQ(got, 1);
    CHECK(!rd_dw("w", "new", &got));
    pClose(h); g11_clean();
}
TEST(gap11_shreg_write_requires_query_access_without_force) {
    REQUIRE(U(SHRegWriteUSValueW)); REQUIRE(U(SHRegWriteUSValueA)); REQUIRE(pClose);
    g11_clean(); g11_mkdw("w", "v", 1);
    /* KEY_WRITE does not include KEY_QUERY_VALUE: a non-forced write is rejected up front */
    HUSKEY h = ow("w", KEY_WRITE); NE(h, NULL); if (!h) return;
    DWORD v = 5, got = 0;
    EQ(U(SHRegWriteUSValueW)(h, L"n", REG_DWORD, &v, 4, SHREGSET_HKCU), ERROR_INVALID_PARAMETER);
    EQ(U(SHRegWriteUSValueA)(h, "n", REG_DWORD, &v, 4, SHREGSET_HKCU), ERROR_INVALID_PARAMETER);
    CHECK(!rd_dw("w", "n", &got));
    /* forcing skips that check and the write is performed */
    EQ(U(SHRegWriteUSValueW)(h, L"n", REG_DWORD, &v, 4, SHREGSET_FORCE_HKCU), 0);
    CHECK(rd_dw("w", "n", &got)); EQ(got, 5);
    pClose(h); g11_clean();
}
TEST(gap11_shreg_write_read_only_handle_denied) {
    REQUIRE(U(SHRegWriteUSValueW)); REQUIRE(U(SHRegWriteUSValueA)); REQUIRE(pClose);
    g11_clean(); g11_mkdw("w", "v", 1);
    HUSKEY h = ow("w", KEY_QUERY_VALUE); NE(h, NULL); if (!h) return;
    DWORD v = 5, got = 0;
    EQ(U(SHRegWriteUSValueW)(h, L"v", REG_DWORD, &v, 4, SHREGSET_FORCE_HKCU), ERROR_ACCESS_DENIED);
    EQ(U(SHRegWriteUSValueA)(h, "v", REG_DWORD, &v, 4, SHREGSET_FORCE_HKCU), ERROR_ACCESS_DENIED);
    CHECK(rd_dw("w", "v", &got)); EQ(got, 1);
    pClose(h); g11_clean();
}
TEST(gap11_shreg_write_types_and_sizes) {
    REQUIRE(U(SHRegWriteUSValueW)); REQUIRE(pClose);
    g11_clean(); g11_mkdw("w", "x", 0);
    HUSKEY h = ow("w", KEY_READ | KEY_WRITE); NE(h, NULL); if (!h) return;
    BYTE big[4096]; memset(big, 0x5a, sizeof big);
    EQ(U(SHRegWriteUSValueW)(h, L"big", REG_BINARY, big, sizeof big, 2), 0);
    EQ(U(SHRegWriteUSValueW)(h, L"zero", REG_BINARY, big, 0, 2), 0);
    EQ(U(SHRegWriteUSValueW)(h, L"none", REG_NONE, big, 3, 2), 0);
    BYTE out[5000]; DWORD type, cb = sizeof out;
    EQ(g11_rd("w", "big", &type, out, &cb), 0); EQ(type, REG_BINARY); EQ(cb, 4096); EQ(out[4095], 0x5a);
    cb = sizeof out; EQ(g11_rd("w", "zero", &type, out, &cb), 0); EQ(cb, 0);
    cb = sizeof out; EQ(g11_rd("w", "none", &type, out, &cb), 0); EQ(type, REG_NONE); EQ(cb, 3);
    pClose(h); g11_clean();
}
TEST(gap11_shreg_write_visible_through_query) {
    REQUIRE(U(SHRegWriteUSValueW)); REQUIRE(U(SHRegQueryUSValueW)); REQUIRE(pClose);
    g11_clean(); g11_mkdw("w", "x", 0);
    HUSKEY h = ow("w", KEY_READ | KEY_WRITE); NE(h, NULL); if (!h) return;
    EQ(U(SHRegWriteUSValueW)(h, L"s", REG_SZ, L"seen", 10, 2), 0);
    WCHAR out[8]; DWORD cb = sizeof out, type = 0;
    EQ(U(SHRegQueryUSValueW)(h, L"s", &type, out, &cb, FALSE, NULL, 0), 0);
    EXPECT_WSTR(out, L"seen");
    pClose(h); g11_clean();
}

/* ---------------- SHRegGetBoolUSValue ---------------- */
struct bcase { DWORD type; const void *data; DWORD cb; int want0, want1; };   /* result for fDefault 0 / 1 */
static const struct bcase bcases_w[] = {
    { REG_SZ, L"YES", 8, 1, 1 }, { REG_SZ, L"yes", 8, 1, 1 }, { REG_SZ, L"Yes", 8, 1, 1 }, { REG_SZ, L"yEs", 8, 1, 1 },
    { REG_SZ, L"TRUE", 10, 1, 1 }, { REG_SZ, L"true", 10, 1, 1 }, { REG_SZ, L"True", 10, 1, 1 }, { REG_SZ, L"1", 4, 1, 1 },
    { REG_SZ, L"NO", 6, 0, 0 }, { REG_SZ, L"no", 6, 0, 0 }, { REG_SZ, L"No", 6, 0, 0 },
    { REG_SZ, L"FALSE", 12, 0, 0 }, { REG_SZ, L"false", 12, 0, 0 }, { REG_SZ, L"False", 12, 0, 0 }, { REG_SZ, L"0", 4, 0, 0 },
    { REG_SZ, L"maybe", 12, 0, 1 }, { REG_SZ, L"", 2, 0, 1 }, { REG_SZ, L"2", 4, 0, 1 }, { REG_SZ, L" YES", 10, 0, 1 },
    { REG_SZ, L"YES ", 10, 0, 1 }, { REG_SZ, L"Y", 4, 0, 1 }, { REG_SZ, L"T", 4, 0, 1 }, { REG_SZ, L"-1", 6, 0, 1 },
    { REG_SZ, L"YESS", 10, 0, 1 }, { REG_SZ, L"TRU", 8, 0, 1 }, { REG_SZ, L"00", 6, 0, 1 }, { REG_SZ, L"01", 6, 0, 1 },
    { REG_SZ, L"ON", 6, 0, 1 }, { REG_SZ, L"OFF", 8, 0, 1 },
    { REG_EXPAND_SZ, L"YES", 8, 1, 1 }, { REG_EXPAND_SZ, L"FALSE", 12, 0, 0 },
    { REG_DWORD, "\x00\x00\x00\x00", 4, 0, 0 }, { REG_DWORD, "\x01\x00\x00\x00", 4, 1, 1 }, { REG_DWORD, "\x02\x00\x00\x00", 4, 1, 1 },
    { REG_DWORD, "\xff\xff\xff\xff", 4, 1, 1 }, { REG_DWORD, "\x00\x00\x00\x80", 4, 1, 1 }, { REG_DWORD, "\x00\x01\x00\x00", 4, 1, 1 },
    { REG_BINARY, "\x00\x00\x00\x00", 4, 0, 0 }, { REG_BINARY, "\x01\x00\x00\x00", 4, 1, 1 }, { REG_BINARY, "\x00\x00\x00\x01", 4, 1, 1 },
    { REG_QWORD, "\x01\x00\x00\x00\x00\x00\x00\x00", 8, 0, 1 },     /* unsupported type: default */
    { REG_MULTI_SZ, L"YES\0", 10, 0, 1 },
    { REG_NONE, "\x01\x00\x00\x00", 4, 0, 1 },
};
TEST(gap11_shreg_bool_w_table) {
    REQUIRE(U(SHRegGetBoolUSValueW));
    WCHAR p[256]; g11_subw(p, 256, "b");
    for (size_t i = 0; i < sizeof bcases_w / sizeof bcases_w[0]; i++) {
        g11_clean();
        g11_mk("b", "v", bcases_w[i].type, bcases_w[i].data, bcases_w[i].cb);
        BOOL r0 = U(SHRegGetBoolUSValueW)(p, L"v", FALSE, FALSE);
        BOOL r1 = U(SHRegGetBoolUSValueW)(p, L"v", FALSE, TRUE);
        CHECKF(r0 == bcases_w[i].want0, "case %zu type %lu default 0 got %d want %d", i, bcases_w[i].type, r0, bcases_w[i].want0);
        CHECKF(r1 == bcases_w[i].want1, "case %zu type %lu default 1 got %d want %d", i, bcases_w[i].type, r1, bcases_w[i].want1);
    }
    g11_clean();
}
TEST(gap11_shreg_bool_a_table) {
    REQUIRE(U(SHRegGetBoolUSValueA));
    char p[256]; g11_sub(p, sizeof p, "b");
    for (size_t i = 0; i < sizeof bcases_w / sizeof bcases_w[0]; i++) {
        if (bcases_w[i].type == REG_MULTI_SZ) continue;
        g11_clean();
        g11_mk("b", "v", bcases_w[i].type, bcases_w[i].data, bcases_w[i].cb);
        BOOL r0 = U(SHRegGetBoolUSValueA)(p, "v", FALSE, FALSE);
        BOOL r1 = U(SHRegGetBoolUSValueA)(p, "v", FALSE, TRUE);
        CHECKF(r0 == bcases_w[i].want0, "case %zu default 0 got %d want %d", i, r0, bcases_w[i].want0);
        CHECKF(r1 == bcases_w[i].want1, "case %zu default 1 got %d want %d", i, r1, bcases_w[i].want1);
    }
    g11_clean();
}
TEST(gap11_shreg_bool_ansi_sz_values) {
    REQUIRE(U(SHRegGetBoolUSValueA));
    /* strings stored through the ANSI registry API */
    char p[256]; g11_sub(p, sizeof p, "b");
    const char *yes[] = { "yes", "TRUE", "1" }, *no[] = { "no", "False", "0" };
    for (int i = 0; i < 3; i++) {
        g11_clean(); g11_mksz("b", "v", yes[i]);
        CHECKF(U(SHRegGetBoolUSValueA)(p, "v", FALSE, FALSE) == TRUE, "%s", yes[i]);
        g11_clean(); g11_mksz("b", "v", no[i]);
        CHECKF(U(SHRegGetBoolUSValueA)(p, "v", FALSE, TRUE) == FALSE, "%s", no[i]);
    }
    g11_clean();
}
TEST(gap11_shreg_bool_missing_value_and_key) {
    REQUIRE(U(SHRegGetBoolUSValueW)); REQUIRE(U(SHRegGetBoolUSValueA));
    g11_clean(); g11_mkdw("b", "other", 1);
    WCHAR pw[256]; char pa[256]; g11_subw(pw, 256, "b"); g11_sub(pa, sizeof pa, "b");
    EQ(U(SHRegGetBoolUSValueW)(pw, L"nope", FALSE, FALSE), 0);
    EQ(U(SHRegGetBoolUSValueW)(pw, L"nope", FALSE, TRUE), 1);
    EQ(U(SHRegGetBoolUSValueA)(pa, "nope", FALSE, FALSE), 0);
    EQ(U(SHRegGetBoolUSValueA)(pa, "nope", FALSE, TRUE), 1);
    g11_clean();
    EQ(U(SHRegGetBoolUSValueW)(pw, L"nope", FALSE, FALSE), 0);
    EQ(U(SHRegGetBoolUSValueW)(pw, L"nope", FALSE, TRUE), 1);
    EQ(U(SHRegGetBoolUSValueA)(pa, "nope", FALSE, TRUE), 1);
}
TEST(gap11_shreg_bool_nonzero_default_normalised_when_missing) {
    REQUIRE(U(SHRegGetBoolUSValueW));
    g11_clean();
    WCHAR pw[256]; g11_subw(pw, 256, "nokey");
    /* a missing value is answered from the YES/NO default string, so the result is 0 or 1 */
    EQ(U(SHRegGetBoolUSValueW)(pw, L"v", FALSE, 5), 1);
    EQ(U(SHRegGetBoolUSValueW)(pw, L"v", FALSE, -1), 1);
    EQ(U(SHRegGetBoolUSValueW)(pw, L"v", FALSE, 0), 0);
}
TEST(gap11_shreg_bool_unrecognised_returns_default_verbatim) {
    REQUIRE(U(SHRegGetBoolUSValueW));
    g11_clean(); g11_mksz("b", "v", "garbage");
    WCHAR pw[256]; g11_subw(pw, 256, "b");
    EQ(U(SHRegGetBoolUSValueW)(pw, L"v", FALSE, 5), 5);
    EQ(U(SHRegGetBoolUSValueW)(pw, L"v", FALSE, 0), 0);
    EQ(U(SHRegGetBoolUSValueW)(pw, L"v", FALSE, 1), 1);
    g11_clean();
}
TEST(gap11_shreg_bool_ignore_hkcu) {
    REQUIRE(U(SHRegGetBoolUSValueW));
    g11_clean(); g11_mksz("b", "v", "YES");
    WCHAR pw[256]; g11_subw(pw, 256, "b");
    EQ(U(SHRegGetBoolUSValueW)(pw, L"v", TRUE, FALSE), 0);    /* HKCU ignored: default */
    EQ(U(SHRegGetBoolUSValueW)(pw, L"v", FALSE, FALSE), 1);
    g11_clean();
}
TEST(gap11_shreg_bool_a_bad_args_return_default) {
    REQUIRE(U(SHRegGetBoolUSValueA));
    if (t_is_wine()) { t_skip("emulator dereferences NULL strings"); return; }
    /* failed ANSI->Unicode conversion of either name leaves the caller's default untouched */
    EQ(U(SHRegGetBoolUSValueA)(NULL, "v", FALSE, 7), 7);
    EQ(U(SHRegGetBoolUSValueA)("Software", NULL, FALSE, 7), 7);
    EQ(U(SHRegGetBoolUSValueA)(NULL, NULL, FALSE, 0), 0);
    char longp[400]; memset(longp, 'x', 399); longp[399] = 0;
    EQ(U(SHRegGetBoolUSValueA)(longp, "v", FALSE, 9), 9);
    EQ(U(SHRegGetBoolUSValueA)("Software", longp, FALSE, 9), 9);
}
TEST(gap11_shreg_bool_w_long_value_name) {
    REQUIRE(U(SHRegGetBoolUSValueW));
    g11_clean(); g11_mksz("b", "v", "YES");
    WCHAR pw[256], n[400]; g11_subw(pw, 256, "b");
    for (int i = 0; i < 399; i++) n[i] = L'n'; n[399] = 0;
    EQ(U(SHRegGetBoolUSValueW)(pw, n, FALSE, 0), 0);
    EQ(U(SHRegGetBoolUSValueW)(pw, n, FALSE, 1), 1);
    g11_clean();
}
TEST(gap11_shreg_bool_default_value_name) {
    REQUIRE(U(SHRegGetBoolUSValueW));
    g11_clean(); g11_mksz("b", "", "TRUE");
    WCHAR pw[256]; g11_subw(pw, 256, "b");
    EQ(U(SHRegGetBoolUSValueW)(pw, NULL, FALSE, 0), 1);
    EQ(U(SHRegGetBoolUSValueW)(pw, L"", FALSE, 0), 1);
    g11_clean();
}
TEST(gap11_shreg_bool_long_string_falls_back_to_default) {
    REQUIRE(U(SHRegGetBoolUSValueW));
    WCHAR big[400]; for (int i = 0; i < 399; i++) big[i] = L'Y'; big[399] = 0;
    g11_clean(); g11_mk("b", "v", REG_SZ, big, sizeof big);
    WCHAR pw[256]; g11_subw(pw, 256, "b");
    /* value does not fit the 0x208-byte buffer: the default answer is used */
    EQ(U(SHRegGetBoolUSValueW)(pw, L"v", FALSE, 0), 0);
    EQ(U(SHRegGetBoolUSValueW)(pw, L"v", FALSE, 1), 1);
    g11_clean();
}
TEST(gap11_shreg_bool_boundary_string_size) {
    REQUIRE(U(SHRegGetBoolUSValueW));
    /* 259 chars + NUL = 520 bytes = 0x208, exactly the internal buffer */
    WCHAR s[300]; for (int i = 0; i < 259; i++) s[i] = L'z'; s[259] = 0;
    g11_clean(); g11_mk("b", "v", REG_SZ, s, 520);
    WCHAR pw[256]; g11_subw(pw, 256, "b");
    EQ(U(SHRegGetBoolUSValueW)(pw, L"v", FALSE, 0), 0);
    EQ(U(SHRegGetBoolUSValueW)(pw, L"v", FALSE, 5), 5);     /* read ok, unrecognised: verbatim default */
    g11_clean();
}

/* ---------------- SHRegEnumUSValueW ---------------- */
static void enum_fixture(void) {
    g11_clean();
    g11_mkdw("e", "alpha", 1);
    g11_mksz("e", "beta", "bb");
    g11_mk("e", "gamma", REG_BINARY, "\x07\x08\x09", 3);
    g11_mkdw("e\\sub1", "x", 0);
}
TEST(gap11_shreg_enum_w_all_values) {
    REQUIRE(U(SHRegEnumUSValueW)); REQUIRE(pClose);
    enum_fixture();
    HUSKEY h = ow("e", KEY_READ); NE(h, NULL); if (!h) return;
    int seen = 0;
    for (DWORD i = 0; i < 3; i++) {
        WCHAR name[32]; DWORD cch = 32, type = 0, cb = 16; BYTE data[16];
        LONG r = U(SHRegEnumUSValueW)(h, i, name, &cch, &type, data, &cb, SHREGENUM_DEFAULT);
        EQ(r, 0);
        if (!lstrcmpW(name, L"alpha")) { seen |= 1; EQ(cch, 5); EQ(type, REG_DWORD); EQ(cb, 4); EQ(*(DWORD *)data, 1); }
        else if (!lstrcmpW(name, L"beta")) { seen |= 2; EQ(cch, 4); EQ(type, REG_SZ); EQ(cb, 6); EXPECT_STR((char *)data, "b"); }
        else if (!lstrcmpW(name, L"gamma")) { seen |= 4; EQ(cch, 5); EQ(type, REG_BINARY); EQ(cb, 3); CHECK(!memcmp(data, "\x07\x08\x09", 3)); }
        else CHECK(0);
    }
    EQ(seen, 7);
    pClose(h); g11_clean();
}
TEST(gap11_shreg_enum_w_no_more_items) {
    REQUIRE(U(SHRegEnumUSValueW)); REQUIRE(pClose);
    enum_fixture();
    HUSKEY h = ow("e", KEY_READ); NE(h, NULL); if (!h) return;
    WCHAR name[32]; DWORD cch;
    DWORD idx[] = { 3, 4, 100, 0x7FFFFFFF };
    for (size_t i = 0; i < 4; i++) {
        cch = 32;
        CHECKF(U(SHRegEnumUSValueW)(h, idx[i], name, &cch, NULL, NULL, NULL, SHREGENUM_DEFAULT) == ERROR_NO_MORE_ITEMS, "index %lu", idx[i]);
        cch = 32;
        CHECKF(U(SHRegEnumUSValueW)(h, idx[i], name, &cch, NULL, NULL, NULL, SHREGENUM_HKCU) == ERROR_NO_MORE_ITEMS, "hkcu index %lu", idx[i]);
    }
    pClose(h); g11_clean();
}
TEST(gap11_shreg_enum_w_name_only) {
    REQUIRE(U(SHRegEnumUSValueW)); REQUIRE(pClose);
    enum_fixture();
    HUSKEY h = ow("e", KEY_READ); NE(h, NULL); if (!h) return;
    WCHAR name[32]; DWORD cch = 32;
    EQ(U(SHRegEnumUSValueW)(h, 0, name, &cch, NULL, NULL, NULL, SHREGENUM_HKCU), 0);
    CHECK(cch >= 4 && cch <= 5);
    EQ(lstrlenW(name), cch);
    pClose(h); g11_clean();
}
TEST(gap11_shreg_enum_w_small_name_buffer) {
    REQUIRE(U(SHRegEnumUSValueW)); REQUIRE(pClose);
    g11_clean(); g11_mkdw("e", "LongValueName", 1);
    HUSKEY h = ow("e", KEY_READ); NE(h, NULL); if (!h) return;
    WCHAR name[32]; DWORD cch = 4;
    EQ(U(SHRegEnumUSValueW)(h, 0, name, &cch, NULL, NULL, NULL, SHREGENUM_DEFAULT), ERROR_MORE_DATA);
    cch = 4;
    EQ(U(SHRegEnumUSValueW)(h, 0, name, &cch, NULL, NULL, NULL, SHREGENUM_HKCU), ERROR_MORE_DATA);
    cch = 13;      /* 13 chars of name need 14 with the terminator */
    EQ(U(SHRegEnumUSValueW)(h, 0, name, &cch, NULL, NULL, NULL, SHREGENUM_HKCU), ERROR_MORE_DATA);
    cch = 14;
    EQ(U(SHRegEnumUSValueW)(h, 0, name, &cch, NULL, NULL, NULL, SHREGENUM_HKCU), 0);
    EQ(cch, 13); EXPECT_WSTR(name, L"LongValueName");
    pClose(h); g11_clean();
}
TEST(gap11_shreg_enum_w_small_data_buffer) {
    REQUIRE(U(SHRegEnumUSValueW)); REQUIRE(pClose);
    g11_clean(); g11_mk("e", "v", REG_BINARY, "0123456789", 10);
    HUSKEY h = ow("e", KEY_READ); NE(h, NULL); if (!h) return;
    WCHAR name[8]; DWORD cch = 8, type = 0, cb = 4; BYTE data[16];
    EQ(U(SHRegEnumUSValueW)(h, 0, name, &cch, &type, data, &cb, SHREGENUM_DEFAULT), ERROR_MORE_DATA);
    EQ(cb, 10);        /* required size is reported */
    pClose(h); g11_clean();
}
TEST(gap11_shreg_enum_w_flag_both_not_supported) {
    REQUIRE(U(SHRegEnumUSValueW)); REQUIRE(pClose);
    enum_fixture();
    HUSKEY h = ow("e", KEY_READ); NE(h, NULL); if (!h) return;
    WCHAR name[32]; DWORD cch = 32;
    EQ(U(SHRegEnumUSValueW)(h, 0, name, &cch, NULL, NULL, NULL, SHREGENUM_BOTH), ERROR_CALL_NOT_IMPLEMENTED);
    cch = 32;
    EQ(U(SHRegEnumUSValueW)(NULL, 0, name, &cch, NULL, NULL, NULL, SHREGENUM_BOTH), ERROR_CALL_NOT_IMPLEMENTED);
    pClose(h); g11_clean();
}
TEST(gap11_shreg_enum_w_invalid_flags) {
    REQUIRE(U(SHRegEnumUSValueW)); REQUIRE(pClose);
    enum_fixture();
    HUSKEY h = ow("e", KEY_READ); NE(h, NULL); if (!h) return;
    DWORD bad[] = { 2, 3, 0x20, 0x100, 0x12, 0x10000, 0x80000000, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        WCHAR name[32]; DWORD cch = 32;
        LONG r = U(SHRegEnumUSValueW)(h, 0, name, &cch, NULL, NULL, NULL, (SHREGENUM_FLAGS)bad[i]);
        CHECKF(r == ERROR_INVALID_PARAMETER, "flags 0x%lx -> %ld", bad[i], (long)r);
    }
    pClose(h); g11_clean();
}
TEST(gap11_shreg_enum_w_null_handle) {
    REQUIRE(U(SHRegEnumUSValueW));
    if (t_is_wine()) { t_skip("emulator dereferences a NULL USKEY"); return; }
    WCHAR name[32]; DWORD cch = 32;
    EQ(U(SHRegEnumUSValueW)(NULL, 0, name, &cch, NULL, NULL, NULL, SHREGENUM_DEFAULT), ERROR_INVALID_PARAMETER);
    EQ(U(SHRegEnumUSValueW)(NULL, 0, name, &cch, NULL, NULL, NULL, SHREGENUM_HKCU), ERROR_INVALID_PARAMETER);
    EQ(U(SHRegEnumUSValueW)(NULL, 0, name, &cch, NULL, NULL, NULL, SHREGENUM_HKLM), ERROR_INVALID_PARAMETER);
}
TEST(gap11_shreg_enum_w_hklm_flag_on_hkcu_only_key) {
    REQUIRE(U(SHRegEnumUSValueW)); REQUIRE(pClose);
    enum_fixture();
    HUSKEY h = ow("e", KEY_READ); NE(h, NULL); if (!h) return;
    WCHAR name[32]; DWORD cch = 32;
    /* the HKLM half of the handle is opened lazily and the private key does not exist there */
    EQ(U(SHRegEnumUSValueW)(h, 0, name, &cch, NULL, NULL, NULL, SHREGENUM_HKLM), ERROR_FILE_NOT_FOUND);
    pClose(h); g11_clean();
}
TEST(gap11_shreg_enum_w_hklm_key) {
    REQUIRE(U(SHRegEnumUSValueW)); REQUIRE(pClose);
    HUSKEY h = NULL;
    EQ(OPENW(L"Software\\Microsoft\\Windows NT\\CurrentVersion", KEY_READ, NULL, &h, TRUE), 0);
    if (!h) return;
    WCHAR name[260]; DWORD cch = 260, type = 0;
    EQ(U(SHRegEnumUSValueW)(h, 0, name, &cch, &type, NULL, NULL, SHREGENUM_HKLM), 0);
    CHECK(cch > 0 && cch == (DWORD)lstrlenW(name));
    int n = 0;
    for (DWORD i = 0; i < 1000; i++) {
        cch = 260;
        LONG r = U(SHRegEnumUSValueW)(h, i, name, &cch, NULL, NULL, NULL, SHREGENUM_HKLM);
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r != 0) { CHECKF(0, "index %lu -> %ld", i, (long)r); break; }
        n++;
    }
    CHECK(n >= 5);
    pClose(h);
}
TEST(gap11_shreg_enum_w_matches_query_info_count) {
    REQUIRE(U(SHRegEnumUSValueW)); REQUIRE(U(SHRegQueryInfoUSKeyW)); REQUIRE(pClose);
    enum_fixture();
    HUSKEY h = ow("e", KEY_READ); NE(h, NULL); if (!h) return;
    DWORD nv = 0;
    EQ(U(SHRegQueryInfoUSKeyW)(h, NULL, NULL, &nv, NULL, SHREGENUM_HKCU), 0);
    EQ(nv, 3);
    DWORD n = 0;
    for (DWORD i = 0; i < 20; i++) {
        WCHAR name[32]; DWORD cch = 32;
        if (U(SHRegEnumUSValueW)(h, i, name, &cch, NULL, NULL, NULL, SHREGENUM_HKCU) != 0) break;
        n++;
    }
    EQ(n, nv);
    pClose(h); g11_clean();
}
TEST(gap11_shreg_enum_w_empty_key) {
    REQUIRE(U(SHRegEnumUSValueW)); REQUIRE(pClose);
    g11_clean(); g11_mkdw("e\\only_sub", "x", 1);
    HUSKEY h = ow("e", KEY_READ); NE(h, NULL); if (!h) return;
    WCHAR name[32]; DWORD cch = 32;
    EQ(U(SHRegEnumUSValueW)(h, 0, name, &cch, NULL, NULL, NULL, SHREGENUM_DEFAULT), ERROR_NO_MORE_ITEMS);
    pClose(h); g11_clean();
}

/* ---------------- SHRegQueryInfoUSKey ---------------- */
static void info_fixture(void) {
    g11_clean();
    g11_mkdw("i", "a", 1);
    g11_mkdw("i", "bbb", 2);
    g11_mkdw("i", "cc", 3);
    g11_mkdw("i\\s1", "x", 0);
    g11_mkdw("i\\sub22", "x", 0);
}
TEST(gap11_shreg_info_w_counts) {
    REQUIRE(U(SHRegQueryInfoUSKeyW)); REQUIRE(pClose);
    info_fixture();
    HUSKEY h = ow("i", KEY_READ); NE(h, NULL); if (!h) return;
    DWORD sk = 99, msk = 99, nv = 99, mv = 99;
    EQ(U(SHRegQueryInfoUSKeyW)(h, &sk, &msk, &nv, &mv, SHREGENUM_DEFAULT), 0);
    EQ(sk, 2); EQ(msk, 5); EQ(nv, 3); EQ(mv, 3);
    sk = msk = nv = mv = 99;
    EQ(U(SHRegQueryInfoUSKeyW)(h, &sk, &msk, &nv, &mv, SHREGENUM_HKCU), 0);
    EQ(sk, 2); EQ(msk, 5); EQ(nv, 3); EQ(mv, 3);
    pClose(h); g11_clean();
}
TEST(gap11_shreg_info_a_counts) {
    REQUIRE(U(SHRegQueryInfoUSKeyA)); REQUIRE(pClose);
    info_fixture();
    HUSKEY h = oa("i", KEY_READ); NE(h, NULL); if (!h) return;
    DWORD sk = 99, msk = 99, nv = 99, mv = 99;
    EQ(U(SHRegQueryInfoUSKeyA)(h, &sk, &msk, &nv, &mv, SHREGENUM_DEFAULT), 0);
    EQ(sk, 2); EQ(msk, 5); EQ(nv, 3); EQ(mv, 3);
    sk = msk = nv = mv = 99;
    EQ(U(SHRegQueryInfoUSKeyA)(h, &sk, &msk, &nv, &mv, SHREGENUM_HKCU), 0);
    EQ(sk, 2); EQ(nv, 3);
    pClose(h); g11_clean();
}
TEST(gap11_shreg_info_each_output_optional) {
    REQUIRE(U(SHRegQueryInfoUSKeyW)); REQUIRE(U(SHRegQueryInfoUSKeyA)); REQUIRE(pClose);
    info_fixture();
    HUSKEY h = ow("i", KEY_READ); NE(h, NULL); if (!h) return;
    DWORD v = 99;
    EQ(U(SHRegQueryInfoUSKeyW)(h, NULL, NULL, NULL, NULL, SHREGENUM_DEFAULT), 0);
    EQ(U(SHRegQueryInfoUSKeyA)(h, NULL, NULL, NULL, NULL, SHREGENUM_DEFAULT), 0);
    EQ(U(SHRegQueryInfoUSKeyW)(h, &v, NULL, NULL, NULL, SHREGENUM_DEFAULT), 0); EQ(v, 2);
    EQ(U(SHRegQueryInfoUSKeyW)(h, NULL, &v, NULL, NULL, SHREGENUM_DEFAULT), 0); EQ(v, 5);
    EQ(U(SHRegQueryInfoUSKeyW)(h, NULL, NULL, &v, NULL, SHREGENUM_DEFAULT), 0); EQ(v, 3);
    EQ(U(SHRegQueryInfoUSKeyW)(h, NULL, NULL, NULL, &v, SHREGENUM_DEFAULT), 0); EQ(v, 3);
    pClose(h); g11_clean();
}
TEST(gap11_shreg_info_empty_key) {
    REQUIRE(U(SHRegQueryInfoUSKeyW)); REQUIRE(pClose);
    g11_clean(); g11_mk("i", NULL, 0, NULL, 0);
    HUSKEY h = ow("i", KEY_READ); NE(h, NULL); if (!h) return;
    DWORD sk = 99, msk = 99, nv = 99, mv = 99;
    EQ(U(SHRegQueryInfoUSKeyW)(h, &sk, &msk, &nv, &mv, SHREGENUM_HKCU), 0);
    EQ(sk, 0); EQ(msk, 0); EQ(nv, 0); EQ(mv, 0);
    pClose(h); g11_clean();
}
TEST(gap11_shreg_info_flag_both) {
    REQUIRE(U(SHRegQueryInfoUSKeyW)); REQUIRE(U(SHRegQueryInfoUSKeyA)); REQUIRE(pClose);
    info_fixture();
    HUSKEY h = ow("i", KEY_READ); NE(h, NULL); if (!h) return;
    DWORD v;
    EQ(U(SHRegQueryInfoUSKeyW)(h, &v, NULL, NULL, NULL, SHREGENUM_BOTH), ERROR_CALL_NOT_IMPLEMENTED);
    EQ(U(SHRegQueryInfoUSKeyA)(h, &v, NULL, NULL, NULL, SHREGENUM_BOTH), ERROR_CALL_NOT_IMPLEMENTED);
    EQ(U(SHRegQueryInfoUSKeyW)(NULL, &v, NULL, NULL, NULL, SHREGENUM_BOTH), ERROR_CALL_NOT_IMPLEMENTED);
    EQ(U(SHRegQueryInfoUSKeyA)(NULL, &v, NULL, NULL, NULL, SHREGENUM_BOTH), ERROR_CALL_NOT_IMPLEMENTED);
    pClose(h); g11_clean();
}
TEST(gap11_shreg_info_null_handle) {
    REQUIRE(U(SHRegQueryInfoUSKeyW)); REQUIRE(U(SHRegQueryInfoUSKeyA));
    if (t_is_wine()) { t_skip("emulator dereferences a NULL USKEY"); return; }
    DWORD v;
    DWORD fl[] = { SHREGENUM_DEFAULT, SHREGENUM_HKCU, SHREGENUM_HKLM };
    for (int i = 0; i < 3; i++) {
        CHECKF(U(SHRegQueryInfoUSKeyW)(NULL, &v, NULL, NULL, NULL, (SHREGENUM_FLAGS)fl[i]) == ERROR_INVALID_PARAMETER, "W flags 0x%lx", fl[i]);
        CHECKF(U(SHRegQueryInfoUSKeyA)(NULL, &v, NULL, NULL, NULL, (SHREGENUM_FLAGS)fl[i]) == ERROR_INVALID_PARAMETER, "A flags 0x%lx", fl[i]);
    }
}
TEST(gap11_shreg_info_invalid_flags) {
    REQUIRE(U(SHRegQueryInfoUSKeyW)); REQUIRE(U(SHRegQueryInfoUSKeyA)); REQUIRE(pClose);
    info_fixture();
    HUSKEY h = ow("i", KEY_READ); NE(h, NULL); if (!h) return;
    DWORD bad[] = { 2, 3, 0x20, 0x100, 0x12, 0x80000000, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        DWORD v = 0;
        LONG rw = U(SHRegQueryInfoUSKeyW)(h, &v, NULL, NULL, NULL, (SHREGENUM_FLAGS)bad[i]);
        LONG ra = U(SHRegQueryInfoUSKeyA)(h, &v, NULL, NULL, NULL, (SHREGENUM_FLAGS)bad[i]);
        CHECKF(rw == ERROR_INVALID_PARAMETER, "W flags 0x%lx -> %ld", bad[i], (long)rw);
        CHECKF(ra == ERROR_INVALID_PARAMETER, "A flags 0x%lx -> %ld", bad[i], (long)ra);
    }
    pClose(h); g11_clean();
}
TEST(gap11_shreg_info_hklm_flag_on_hkcu_only_key) {
    REQUIRE(U(SHRegQueryInfoUSKeyW)); REQUIRE(U(SHRegQueryInfoUSKeyA)); REQUIRE(pClose);
    info_fixture();
    HUSKEY h = ow("i", KEY_READ); NE(h, NULL); if (!h) return;
    DWORD v = 0;
    EQ(U(SHRegQueryInfoUSKeyA)(h, &v, NULL, NULL, NULL, SHREGENUM_HKLM), ERROR_FILE_NOT_FOUND);
    LONG rw = U(SHRegQueryInfoUSKeyW)(h, &v, NULL, NULL, NULL, SHREGENUM_HKLM);
    t_note("SHRegQueryInfoUSKeyW(HKLM) on HKCU-only key -> %ld", (long)rw);
    pClose(h); g11_clean();
}
TEST(gap11_shreg_info_hklm_key) {
    REQUIRE(U(SHRegQueryInfoUSKeyW)); REQUIRE(U(SHRegQueryInfoUSKeyA)); REQUIRE(pClose);
    HUSKEY h = NULL;
    EQ(OPENW(L"Software\\Microsoft\\Windows NT\\CurrentVersion", KEY_READ, NULL, &h, TRUE), 0);
    if (!h) return;
    DWORD sk = 0, nv = 0, ska = 0, nva = 0;
    EQ(U(SHRegQueryInfoUSKeyW)(h, &sk, NULL, &nv, NULL, SHREGENUM_HKLM), 0);
    CHECK(sk > 0 && nv > 0);
    EQ(U(SHRegQueryInfoUSKeyA)(h, &ska, NULL, &nva, NULL, SHREGENUM_HKLM), 0);
    EQ(ska, sk); EQ(nva, nv);
    pClose(h);
}
TEST(gap11_shreg_info_grows_after_write) {
    REQUIRE(U(SHRegQueryInfoUSKeyW)); REQUIRE(U(SHRegWriteUSValueW)); REQUIRE(pClose);
    g11_clean(); g11_mkdw("i", "a", 1);
    HUSKEY h = ow("i", KEY_READ | KEY_WRITE); NE(h, NULL); if (!h) return;
    DWORD nv = 0, mv = 0, v = 1;
    EQ(U(SHRegQueryInfoUSKeyW)(h, NULL, NULL, &nv, &mv, SHREGENUM_HKCU), 0);
    EQ(nv, 1); EQ(mv, 1);
    EQ(U(SHRegWriteUSValueW)(h, L"longer_name", REG_DWORD, &v, 4, 2), 0);
    EQ(U(SHRegQueryInfoUSKeyW)(h, NULL, NULL, &nv, &mv, SHREGENUM_HKCU), 0);
    EQ(nv, 2); EQ(mv, 11);
    pClose(h); g11_clean();
}
