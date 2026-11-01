#include "rk.h"

/* helpers going through the tested entry points */
static LSTATUS vset(HKEY h, const WCHAR *n, DWORD t, const void *d, DWORD cb) { return U(RegSetValueExW)(h, n, 0, t, (const BYTE *)d, cb); }
static LSTATUS vget(HKEY h, const WCHAR *n, DWORD *t, void *d, DWORD *cb) { return U(RegQueryValueExW)(h, n, NULL, t, (BYTE *)d, cb); }

/* ---------------- RegSetValueEx / RegQueryValueEx round trips ---------------- */
TEST(regkb_val_roundtrip_dword) {
    REQUIRE(U(RegSetValueExW)); REQUIRE(U(RegQueryValueExW));
    HKEY h = FRESH(); REQUIRE(h);
    DWORD v = 0xDEADBEEF, out = 0, cb = 4, t = 0;
    EQ(vset(h, L"d", REG_DWORD, &v, 4), 0);
    EQ(vget(h, L"d", &t, &out, &cb), 0);
    EQ(t, REG_DWORD); EQ(cb, 4); EQ(out, 0xDEADBEEF);
    RegCloseKey(h);
}
TEST(regkb_val_roundtrip_dword_big_endian) {
    REQUIRE(U(RegSetValueExW)); REQUIRE(U(RegQueryValueExW));
    HKEY h = FRESH(); REQUIRE(h);
    BYTE be[4] = { 0x12, 0x34, 0x56, 0x78 }, out[4] = {0}; DWORD cb = 4, t = 0;
    EQ(vset(h, L"be", REG_DWORD_BIG_ENDIAN, be, 4), 0);
    EQ(vget(h, L"be", &t, out, &cb), 0);
    EQ(t, REG_DWORD_BIG_ENDIAN); EQ(cb, 4);
    CHECK(!memcmp(be, out, 4));    /* stored verbatim, no byte swap */
    RegCloseKey(h);
}
TEST(regkb_val_roundtrip_qword) {
    REQUIRE(U(RegSetValueExW)); REQUIRE(U(RegQueryValueExW));
    HKEY h = FRESH(); REQUIRE(h);
    unsigned long long v = 0x0123456789ABCDEFULL, out = 0; DWORD cb = 8, t = 0;
    EQ(vset(h, L"q", REG_QWORD, &v, 8), 0);
    EQ(vget(h, L"q", &t, &out, &cb), 0);
    EQ(t, REG_QWORD); EQ(cb, 8); CHECK(out == v);
    RegCloseKey(h);
}
TEST(regkb_val_roundtrip_sz) {
    REQUIRE(U(RegSetValueExW)); REQUIRE(U(RegQueryValueExW));
    HKEY h = FRESH(); REQUIRE(h);
    WCHAR s[] = L"hello world", out[32] = {0}; DWORD cb = sizeof out, t = 0;
    EQ(vset(h, L"s", REG_SZ, s, sizeof s), 0);
    EQ(vget(h, L"s", &t, out, &cb), 0);
    EQ(t, REG_SZ); EQ(cb, sizeof s); EXPECT_WSTR(out, s);
    RegCloseKey(h);
}
TEST(regkb_val_roundtrip_expand_sz_unexpanded) {
    REQUIRE(U(RegSetValueExW)); REQUIRE(U(RegQueryValueExW));
    HKEY h = FRESH(); REQUIRE(h);
    WCHAR s[] = L"%SystemRoot%\\system32", out[64] = {0}; DWORD cb = sizeof out, t = 0;
    EQ(vset(h, L"e", REG_EXPAND_SZ, s, sizeof s), 0);
    EQ(vget(h, L"e", &t, out, &cb), 0);
    EQ(t, REG_EXPAND_SZ); EXPECT_WSTR(out, s);   /* RegQueryValueEx never expands */
    RegCloseKey(h);
}
TEST(regkb_val_roundtrip_multi_sz) {
    REQUIRE(U(RegSetValueExW)); REQUIRE(U(RegQueryValueExW));
    HKEY h = FRESH(); REQUIRE(h);
    WCHAR m[] = L"one\0two\0three\0"; WCHAR out[64] = {0}; DWORD cb = sizeof out, t = 0;
    EQ(vset(h, L"m", REG_MULTI_SZ, m, sizeof m), 0);
    EQ(vget(h, L"m", &t, out, &cb), 0);
    EQ(t, REG_MULTI_SZ); EQ(cb, sizeof m); CHECK(!memcmp(m, out, sizeof m));
    RegCloseKey(h);
}
TEST(regkb_val_roundtrip_binary_all_bytes) {
    REQUIRE(U(RegSetValueExW)); REQUIRE(U(RegQueryValueExW));
    HKEY h = FRESH(); REQUIRE(h);
    BYTE d[256], out[256]; DWORD cb = 256, t = 0;
    for (int i = 0; i < 256; i++) d[i] = (BYTE)i;
    EQ(vset(h, L"b", REG_BINARY, d, 256), 0);
    EQ(vget(h, L"b", &t, out, &cb), 0);
    EQ(t, REG_BINARY); EQ(cb, 256); CHECK(!memcmp(d, out, 256));
    RegCloseKey(h);
}
TEST(regkb_val_roundtrip_none_and_link_types) {
    REQUIRE(U(RegSetValueExW)); REQUIRE(U(RegQueryValueExW));
    HKEY h = FRESH(); REQUIRE(h);
    BYTE d[5] = { 1, 2, 3, 4, 5 }, out[8]; DWORD cb, t;
    EQ(vset(h, L"none", REG_NONE, d, 5), 0);
    cb = 8; EQ(vget(h, L"none", &t, out, &cb), 0); EQ(t, REG_NONE); EQ(cb, 5); CHECK(!memcmp(d, out, 5));
    EQ(vset(h, L"link", REG_LINK, d, 4), 0);
    cb = 8; EQ(vget(h, L"link", &t, out, &cb), 0); EQ(t, REG_LINK); EQ(cb, 4);
    RegCloseKey(h);
}
TEST(regkb_val_all_defined_types_binary_payload) {
    REQUIRE(U(RegSetValueExW)); REQUIRE(U(RegQueryValueExW));
    HKEY h = FRESH(); REQUIRE(h);
    BYTE d[6] = { 9, 8, 7, 6, 5, 4 }, out[8];
    DWORD types[] = { REG_NONE, REG_BINARY, REG_DWORD, REG_DWORD_BIG_ENDIAN, REG_QWORD,
                      REG_RESOURCE_LIST, REG_FULL_RESOURCE_DESCRIPTOR, REG_RESOURCE_REQUIREMENTS_LIST };
    for (size_t i = 0; i < sizeof types / sizeof types[0]; i++) {
        DWORD cb = 8, t = 0; WCHAR nm[16]; swprintf(nm, 16, L"t%lu", types[i]);
        LSTATUS s = vset(h, nm, types[i], d, 6);
        CHECKF(s == 0, "type %lu set -> %ld", types[i], (long)s);
        if (s) continue;
        EQ(vget(h, nm, &t, out, &cb), 0);
        EQ(t, types[i]); EQ(cb, 6); CHECK(!memcmp(d, out, 6));
    }
    RegCloseKey(h);
}
TEST(regkb_val_unknown_type_values_roundtrip) {
    REQUIRE(U(RegSetValueExW)); REQUIRE(U(RegQueryValueExW));
    HKEY h = FRESH(); REQUIRE(h);
    DWORD types[] = { 12, 13, 100, 0x1234, 0x7FFFFFFF };
    for (size_t i = 0; i < sizeof types / sizeof types[0]; i++) {
        BYTE d[3] = { 1, 2, 3 }, out[4]; DWORD cb = 4, t = 0; WCHAR nm[16]; swprintf(nm, 16, L"u%lu", types[i]);
        LSTATUS s = vset(h, nm, types[i], d, 3);
        CHECKF(s == 0, "type 0x%lx set -> %ld", types[i], (long)s);
        if (s) continue;
        EQ(vget(h, nm, &t, out, &cb), 0); EQ(t, types[i]); EQ(cb, 3);
    }
    RegCloseKey(h);
}
TEST(regkb_val_type_ffffffff_observed) {
    REQUIRE(U(RegSetValueExW));
    HKEY h = FRESH(); REQUIRE(h); BYTE d[2] = { 1, 2 };
    LSTATUS s = vset(h, L"ff", 0xFFFFFFFF, d, 2);
    t_note("RegSetValueExW(type 0xFFFFFFFF) = %ld", (long)s);
    RegCloseKey(h);
}
TEST(regkb_val_odd_size_dword_and_sz_observed) {
    REQUIRE(U(RegSetValueExW)); REQUIRE(U(RegQueryValueExW));
    HKEY h = FRESH(); REQUIRE(h); BYTE d[8] = { 1, 2, 3, 4, 5, 6, 7, 8 }, out[16]; DWORD cb = 16, t;
    LSTATUS s = vset(h, L"dw3", REG_DWORD, d, 3);
    t_note("REG_DWORD with 3 bytes -> %ld", (long)s);
    if (s == 0) { vget(h, L"dw3", &t, out, &cb); t_note("   read back size %lu", cb); }
    s = vset(h, L"sz3", REG_SZ, d, 3);
    t_note("REG_SZ with 3 bytes -> %ld", (long)s);
    if (s == 0) { cb = 16; vget(h, L"sz3", &t, out, &cb); t_note("   read back size %lu", cb); }
    RegCloseKey(h);
}
TEST(regkb_val_overwrite_changes_type_and_size) {
    REQUIRE(U(RegSetValueExW)); REQUIRE(U(RegQueryValueExW));
    HKEY h = FRESH(); REQUIRE(h);
    DWORD v = 1, t, cb; BYTE out[64];
    EQ(vset(h, L"x", REG_DWORD, &v, 4), 0);
    WCHAR s[] = L"a longer string value";
    EQ(vset(h, L"x", REG_SZ, s, sizeof s), 0);
    cb = sizeof out; EQ(vget(h, L"x", &t, out, &cb), 0); EQ(t, REG_SZ); EQ(cb, sizeof s);
    EQ(vset(h, L"x", REG_DWORD, &v, 4), 0);
    cb = sizeof out; EQ(vget(h, L"x", &t, out, &cb), 0); EQ(t, REG_DWORD); EQ(cb, 4);
    EQ(rk_valcount(h), 1);
    RegCloseKey(h);
}
TEST(regkb_val_repeated_set_is_idempotent) {
    REQUIRE(U(RegSetValueExW));
    HKEY h = FRESH(); REQUIRE(h);
    for (DWORD i = 0; i < 500; i++) EQ(vset(h, L"counter", REG_DWORD, &i, 4), 0);
    DWORD v = 0, cb = 4, t; EQ(vget(h, L"counter", &t, &v, &cb), 0); EQ(v, 499); EQ(rk_valcount(h), 1);
    RegCloseKey(h);
}
/* kernelbase RegSetValueExW: Reserved != 0 returns 0x57 before anything else */
TEST(regkb_val_set_reserved_nonzero) {
    REQUIRE(U(RegSetValueExW));
    HKEY h = FRESH(); REQUIRE(h); DWORD v = 1;
    EQ(U(RegSetValueExW)(h, L"r", 1, REG_DWORD, (BYTE *)&v, 4), ERROR_INVALID_PARAMETER);
    EQ(U(RegSetValueExW)(h, L"r", 0x80000000, REG_DWORD, (BYTE *)&v, 4), ERROR_INVALID_PARAMETER);
    EQ(rk_valcount(h), 0);
    RegCloseKey(h);
}
TEST(regkb_val_set_zero_length_data_all_types) {
    REQUIRE(U(RegSetValueExW)); REQUIRE(U(RegQueryValueExW));
    HKEY h = FRESH(); REQUIRE(h);
    DWORD types[] = { REG_NONE, REG_SZ, REG_EXPAND_SZ, REG_BINARY, REG_DWORD, REG_MULTI_SZ, REG_QWORD };
    for (size_t i = 0; i < sizeof types / sizeof types[0]; i++) {
        WCHAR nm[16]; swprintf(nm, 16, L"z%lu", types[i]);
        BYTE b = 0x55; DWORD cb = 1, t = 99;
        LSTATUS s = vset(h, nm, types[i], &b, 0);
        CHECKF(s == 0, "type %lu zero-length set -> %ld", types[i], (long)s);
        if (s) continue;
        cb = 1; t = 99; s = vget(h, nm, &t, &b, &cb);
        EQ(s, 0); EQ(t, types[i]); EQ(cb, 0);
    }
    RegCloseKey(h);
}
TEST(regkb_val_set_null_data_zero_size) {
    REQUIRE(U(RegSetValueExW)); REQUIRE(U(RegQueryValueExW));
    HKEY h = FRESH(); REQUIRE(h);
    EQ(U(RegSetValueExW)(h, L"nulldata", 0, REG_BINARY, NULL, 0), 0);
    EQ(U(RegSetValueExW)(h, L"nulldata_sz", 0, REG_SZ, NULL, 0), 0);
    EQ(U(RegSetValueExW)(h, L"nulldata_none", 0, REG_NONE, NULL, 0), 0);
    DWORD t = 99, cb = 77; EQ(vget(h, L"nulldata", &t, NULL, &cb), 0); EQ(t, REG_BINARY); EQ(cb, 0);
    RegCloseKey(h);
}
TEST(regkb_val_set_null_data_nonzero_size_observed) {
    REQUIRE(U(RegSetValueExW));
    HKEY h = FRESH(); REQUIRE(h);
    LSTATUS s = U(RegSetValueExW)(h, L"nd", 0, REG_BINARY, NULL, 4);
    CHECK(s != 0);
    t_note("RegSetValueExW(NULL data, cb=4) = %ld", (long)s);
    RegCloseKey(h);
}
TEST(regkb_val_set_1mb_binary) {
    REQUIRE(U(RegSetValueExW)); REQUIRE(U(RegQueryValueExW));
    HKEY h = FRESH(); REQUIRE(h);
    DWORD n = 1024 * 1024; BYTE *d = malloc(n), *o = malloc(n); REQUIRE(d && o);
    for (DWORD i = 0; i < n; i++) d[i] = (BYTE)(i * 31 + (i >> 8));
    EQ(vset(h, L"big", REG_BINARY, d, n), 0);
    DWORD cb = 0, t = 0; EQ(vget(h, L"big", &t, NULL, &cb), 0); EQ(cb, n);
    cb = n; EQ(vget(h, L"big", &t, o, &cb), 0); EQ(cb, n); CHECK(!memcmp(d, o, n));
    DWORD mx = 0; RegQueryInfoKeyW(h, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &mx, NULL, NULL);
    EQ(mx, n);
    free(d); free(o); RegCloseKey(h);
}
TEST(regkb_val_set_1mb_string) {
    REQUIRE(U(RegSetValueExW)); REQUIRE(U(RegGetValueW));
    HKEY h = FRESH(); REQUIRE(h);
    DWORD chars = 512 * 1024; WCHAR *d = malloc((chars + 1) * 2); REQUIRE(d);
    rk_fill_w(d, chars, L'q');
    EQ(vset(h, L"bigsz", REG_SZ, d, (chars + 1) * 2), 0);
    DWORD cb = 0; EQ(U(RegGetValueW)(h, NULL, L"bigsz", RRF_RT_REG_SZ, NULL, NULL, &cb), 0);
    CHECK(cb >= (chars + 1) * 2);
    free(d); RegCloseKey(h);
}
TEST(regkb_val_data_larger_than_limit_observed) {
    REQUIRE(U(RegSetValueExW));
    HKEY h = FRESH(); REQUIRE(h);
    DWORD n = 4 * 1024 * 1024; BYTE *d = calloc(n, 1); REQUIRE(d);
    LSTATUS s = vset(h, L"huge", REG_BINARY, d, n);
    t_note("4 MB REG_BINARY set = %ld", (long)s);
    free(d); RegCloseKey(h);
}
TEST(regkb_val_name_lengths) {
    REQUIRE(U(RegSetValueExW)); REQUIRE(U(RegQueryValueExW)); REQUIRE(U(RegDeleteValueW));
    HKEY h = FRESH(); REQUIRE(h);
    static WCHAR nm[40000]; DWORD v = 4, cb, t, out;
    DWORD lens[] = { 1, 2, 255, 256, 1000, 16383 };
    for (size_t i = 0; i < sizeof lens / sizeof lens[0]; i++) {
        rk_fill_w(nm, lens[i], L'n');
        LSTATUS s = vset(h, nm, REG_DWORD, &v, 4);
        CHECKF(s == 0, "name length %lu -> %ld", lens[i], (long)s);
        if (s) continue;
        cb = 4; out = 0; EQ(vget(h, nm, &t, &out, &cb), 0); EQ(out, 4);
        EQ(U(RegDeleteValueW)(h, nm), 0);
    }
    RegCloseKey(h);
}
TEST(regkb_val_name_length_above_documented_max_observed) {
    REQUIRE(U(RegSetValueExW));
    HKEY h = FRESH(); REQUIRE(h);
    static WCHAR nm[40000]; DWORD v = 4;
    DWORD lens[] = { 16384, 32766, 32767 };
    for (size_t i = 0; i < 3; i++) {
        rk_fill_w(nm, lens[i], L'm');
        LSTATUS s = vset(h, nm, REG_DWORD, &v, 4);
        t_note("value name %lu chars -> %ld", lens[i], (long)s);
    }
    RegCloseKey(h);
}
TEST(regkb_val_name_length_32768_rejected) {
    REQUIRE(U(RegSetValueExW)); REQUIRE(U(RegQueryValueExW));
    HKEY h = FRESH(); REQUIRE(h);
    static WCHAR nm[40000]; DWORD v = 4;
    rk_fill_w(nm, 32768, L'm');
    EQ(vset(h, nm, REG_DWORD, &v, 4), ERROR_INVALID_PARAMETER);
    rk_fill_w(nm, 40000 - 1, L'm');
    EQ(vset(h, nm, REG_DWORD, &v, 4), ERROR_INVALID_PARAMETER);
    EQ(rk_valcount(h), 0);
    RegCloseKey(h);
}
TEST(regkb_val_name_unicode_and_backslash) {
    REQUIRE(U(RegSetValueExW)); REQUIRE(U(RegQueryValueExW));
    HKEY h = FRESH(); REQUIRE(h);
    const WCHAR *names[] = { L"back\\slash", L"\\", L"\\\\lead", L"\xD834\xDD1E surrogate", L"\x4E2D\x6587",
                             L"with space", L" lead and trail ", L"tab\there", L"/slash", L"a.b" };
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
        DWORD v = (DWORD)i + 1, out = 0, cb = 4, t;
        EQ(vset(h, names[i], REG_DWORD, &v, 4), 0);
        EQ(vget(h, names[i], &t, &out, &cb), 0); EQ(out, i + 1);
    }
    EQ(rk_valcount(h), sizeof names / sizeof names[0]);
    RegCloseKey(h);
}
TEST(regkb_val_name_case_insensitive) {
    REQUIRE(U(RegSetValueExW)); REQUIRE(U(RegQueryValueExW));
    HKEY h = FRESH(); REQUIRE(h); DWORD v = 1, out = 0, cb = 4, t;
    EQ(vset(h, L"CaseName", REG_DWORD, &v, 4), 0);
    EQ(vget(h, L"CASENAME", &t, &out, &cb), 0); EQ(out, 1);
    v = 2; EQ(vset(h, L"casename", REG_DWORD, &v, 4), 0);
    EQ(rk_valcount(h), 1);
    cb = 4; EQ(vget(h, L"CaseName", &t, &out, &cb), 0); EQ(out, 2);
    WCHAR nm[32]; DWORD n = 32;
    EQ(RegEnumValueW(h, 0, nm, &n, NULL, NULL, NULL, NULL), 0);
    t_note("value name case after overwrite: %ls", nm);
    RegCloseKey(h);
}
TEST(regkb_val_name_nonascii_case_insensitive) {
    REQUIRE(U(RegSetValueExW)); REQUIRE(U(RegQueryValueExW));
    HKEY h = FRESH(); REQUIRE(h); DWORD v = 1, out = 0, cb = 4, t;
    EQ(vset(h, L"\x00e9t\x00e9", REG_DWORD, &v, 4), 0);
    EQ(vget(h, L"\x00c9T\x00c9", &t, &out, &cb), 0);
    RegCloseKey(h);
}
TEST(regkb_val_default_value_name_forms) {
    REQUIRE(U(RegSetValueExW)); REQUIRE(U(RegQueryValueExW));
    HKEY h = FRESH(); REQUIRE(h);
    WCHAR a[] = L"first", b[] = L"second", out[16]; DWORD cb, t;
    EQ(vset(h, NULL, REG_SZ, a, sizeof a), 0);
    cb = sizeof out; EQ(vget(h, L"", &t, out, &cb), 0); EXPECT_WSTR(out, a);
    EQ(vset(h, L"", REG_SZ, b, sizeof b), 0);
    cb = sizeof out; EQ(vget(h, NULL, &t, out, &cb), 0); EXPECT_WSTR(out, b);
    EQ(rk_valcount(h), 1);
    WCHAR nm[4] = { L'x', 0 }; DWORD n = 4;
    EQ(RegEnumValueW(h, 0, nm, &n, NULL, &t, NULL, NULL), 0);
    EQ(n, 0); EQ(nm[0], 0);   /* the default value enumerates with an empty name */
    RegCloseKey(h);
}
TEST(regkb_val_default_value_a_forms) {
    REQUIRE(U(RegSetValueExA)); REQUIRE(U(RegQueryValueExA));
    HKEY h = FRESH(); REQUIRE(h); char out[16]; DWORD cb = sizeof out, t;
    EQ(U(RegSetValueExA)(h, NULL, 0, REG_SZ, (BYTE *)"dflt", 5), 0);
    EQ(U(RegQueryValueExA)(h, "", NULL, &t, (BYTE *)out, &cb), 0); EXPECT_STR(out, "dflt"); EQ(cb, 5);
    cb = sizeof out;
    EQ(U(RegQueryValueExA)(h, NULL, NULL, &t, (BYTE *)out, &cb), 0);
    RegCloseKey(h);
}

/* ---------------- query semantics ---------------- */
TEST(regkb_val_query_missing_value) {
    REQUIRE(U(RegQueryValueExW)); REQUIRE(U(RegQueryValueExA));
    HKEY h = FRESH(); REQUIRE(h); DWORD t = 7, cb = 4, out;
    EQ(vget(h, L"nope", &t, &out, &cb), ERROR_FILE_NOT_FOUND);
    EQ(U(RegQueryValueExA)(h, "nope", NULL, &t, (BYTE *)&out, &cb), ERROR_FILE_NOT_FOUND);
    EQ(vget(h, NULL, &t, &out, &cb), ERROR_FILE_NOT_FOUND);   /* no default value yet */
    EQ(cb, 4);
    RegCloseKey(h);
}
TEST(regkb_val_query_all_null_outputs_existence_check) {
    REQUIRE(U(RegQueryValueExW)); REQUIRE(U(RegQueryValueExA));
    HKEY h = FRESH(); REQUIRE(h); sc_dw(h, L"d", 1);
    EQ(vget(h, L"d", NULL, NULL, NULL), 0);
    EQ(vget(h, L"nope", NULL, NULL, NULL), ERROR_FILE_NOT_FOUND);
    EQ(U(RegQueryValueExA)(h, "d", NULL, NULL, NULL, NULL), 0);
    RegCloseKey(h);
}
TEST(regkb_val_query_type_only) {
    REQUIRE(U(RegQueryValueExW));
    HKEY h = FRESH(); REQUIRE(h); sc_dw(h, L"d", 1); DWORD t = 0;
    EQ(vget(h, L"d", &t, NULL, NULL), 0); EQ(t, REG_DWORD);
    RegCloseKey(h);
}
TEST(regkb_val_query_size_only) {
    REQUIRE(U(RegQueryValueExW));
    HKEY h = FRESH(); REQUIRE(h); sc_sz(h, L"s", L"abcd"); DWORD cb = 123;
    EQ(vget(h, L"s", NULL, NULL, &cb), 0); EQ(cb, 10);
    RegCloseKey(h);
}
TEST(regkb_val_query_cbdata_ignored_on_size_query) {
    REQUIRE(U(RegQueryValueExW));
    HKEY h = FRESH(); REQUIRE(h); sc_dw(h, L"d", 1); DWORD cb = 0xFFFFFF;
    EQ(vget(h, L"d", NULL, NULL, &cb), 0); EQ(cb, 4);
    RegCloseKey(h);
}
TEST(regkb_val_query_buffer_too_small) {
    REQUIRE(U(RegQueryValueExW));
    HKEY h = FRESH(); REQUIRE(h);
    BYTE d[16]; for (int i = 0; i < 16; i++) d[i] = (BYTE)(i + 1);
    sc_raw(h, L"b", REG_BINARY, d, 16);
    BYTE out[32]; DWORD cb, t;
    DWORD sizes[] = { 0, 1, 8, 15 };
    for (int i = 0; i < 4; i++) {
        memset(out, 0xCC, sizeof out); cb = sizes[i]; t = 0;
        EQ(vget(h, L"b", &t, out, &cb), ERROR_MORE_DATA);
        EQ(cb, 16); EQ(t, REG_BINARY);
    }
    memset(out, 0xCC, sizeof out); cb = 16;
    EQ(vget(h, L"b", &t, out, &cb), 0); EQ(cb, 16); CHECK(!memcmp(out, d, 16)); EQ(out[16], 0xCC);
    cb = 20; EQ(vget(h, L"b", &t, out, &cb), 0); EQ(cb, 16);   /* cb is updated to the actual size */
    RegCloseKey(h);
}
TEST(regkb_val_query_no_partial_write_on_more_data_binary) {
    REQUIRE(U(RegQueryValueExW));
    HKEY h = FRESH(); REQUIRE(h);
    BYTE d[16] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 }; sc_raw(h, L"b", REG_BINARY, d, 16);
    BYTE out[16]; memset(out, 0xEE, 16); DWORD cb = 8, t;
    EQ(vget(h, L"b", &t, out, &cb), ERROR_MORE_DATA);
    t_note("bytes written on ERROR_MORE_DATA: %02x %02x", out[0], out[8]);
    RegCloseKey(h);
}
TEST(regkb_val_query_data_without_size_invalid) {
    REQUIRE(U(RegQueryValueExW)); REQUIRE(U(RegQueryValueExA));
    HKEY h = FRESH(); REQUIRE(h); sc_dw(h, L"d", 1); DWORD out;
    EQ(vget(h, L"d", NULL, &out, NULL), ERROR_INVALID_PARAMETER);
    EQ(U(RegQueryValueExA)(h, "d", NULL, NULL, (BYTE *)&out, NULL), ERROR_INVALID_PARAMETER);
    RegCloseKey(h);
}
TEST(regkb_val_query_reserved_must_be_null) {
    REQUIRE(U(RegQueryValueExW)); REQUIRE(U(RegQueryValueExA));
    HKEY h = FRESH(); REQUIRE(h); sc_dw(h, L"d", 1); DWORD out, cb = 4, rsv = 0, t;
    EQ(U(RegQueryValueExW)(h, L"d", &rsv, &t, (BYTE *)&out, &cb), ERROR_INVALID_PARAMETER);
    cb = 4;
    EQ(U(RegQueryValueExA)(h, "d", &rsv, &t, (BYTE *)&out, &cb), ERROR_INVALID_PARAMETER);
    RegCloseKey(h);
}
TEST(regkb_val_query_invalid_handle) {
    REQUIRE(U(RegQueryValueExW));
    DWORD out, cb = 4, t;
    EQ(vget(NULL, L"d", &t, &out, &cb), ERROR_INVALID_HANDLE);
    EQ(vget(INVALID_HANDLE_VALUE, L"d", &t, &out, &cb), ERROR_INVALID_HANDLE);
}
TEST(regkb_val_query_unterminated_string_gets_terminator_in_buffer) {
    REQUIRE(U(RegQueryValueExW));
    HKEY h = FRESH(); REQUIRE(h);
    WCHAR raw[3] = { L'a', L'b', L'c' };
    if (rk_ntset(h, L"unterm", REG_SZ, raw, 6) != 0) { t_skip("NtSetValueKey unavailable"); RegCloseKey(h); return; }
    WCHAR out[8]; DWORD cb = 6, t = 0;
    memset(out, 0xCC, sizeof out);
    EQ(vget(h, L"unterm", &t, out, &cb), 0); EQ(cb, 6); EQ(t, REG_SZ);
    EQ(out[0], L'a'); EQ(out[2], L'c'); EQ(out[3], 0xCCCC);   /* no room to terminate */
    memset(out, 0xCC, sizeof out); cb = 16;
    EQ(vget(h, L"unterm", &t, out, &cb), 0); EQ(cb, 6); EQ(out[3], 0);   /* room: terminator added, size not changed */
    RegCloseKey(h);
}
TEST(regkb_val_query_unterminated_multisz_gets_terminator) {
    REQUIRE(U(RegQueryValueExW));
    HKEY h = FRESH(); REQUIRE(h);
    WCHAR raw[4] = { L'a', 0, L'b', L'c' };
    if (rk_ntset(h, L"um", REG_MULTI_SZ, raw, 8) != 0) { t_skip("NtSetValueKey unavailable"); RegCloseKey(h); return; }
    WCHAR out[8]; DWORD cb = 16, t = 0; memset(out, 0xCC, sizeof out);
    EQ(vget(h, L"um", &t, out, &cb), 0); EQ(cb, 8); EQ(out[4], 0);
    RegCloseKey(h);
}
TEST(regkb_val_query_unterminated_binary_not_touched) {
    REQUIRE(U(RegQueryValueExW));
    HKEY h = FRESH(); REQUIRE(h); BYTE raw[4] = { 1, 2, 3, 4 };
    sc_raw(h, L"bin", REG_BINARY, raw, 4);
    BYTE out[8]; memset(out, 0xCC, 8); DWORD cb = 8, t;
    EQ(vget(h, L"bin", &t, out, &cb), 0); EQ(cb, 4); EQ(out[4], 0xCC);
    RegCloseKey(h);
}
TEST(regkb_val_set_unterminated_sz_terminator_added_when_followed_by_zero) {
    REQUIRE(U(RegSetValueExW)); REQUIRE(U(RegQueryValueExW));
    HKEY h = FRESH(); REQUIRE(h);
    _Alignas(16) WCHAR buf[8] = { L'a', L'b', L'c', 0, 0, 0, 0, 0 };
    DWORD cb = sizeof buf, t;
    EQ(U(RegSetValueExW)(h, L"s", 0, REG_SZ, (BYTE *)buf, 6), 0);
    WCHAR out[8]; cb = sizeof out;
    EQ(vget(h, L"s", &t, out, &cb), 0);
    t_note("unterminated REG_SZ set (zero follows) stored size %lu", cb);
    CHECK(cb == 8 || cb == 6);
    RegCloseKey(h);
}
TEST(regkb_val_set_terminated_sz_size_preserved) {
    REQUIRE(U(RegSetValueExW)); REQUIRE(U(RegQueryValueExW));
    HKEY h = FRESH(); REQUIRE(h);
    WCHAR s[] = L"abc\0def";   /* embedded NUL inside the declared size */
    DWORD cb = sizeof s, t; WCHAR out[16];
    EQ(vset(h, L"emb", REG_SZ, s, sizeof s), 0);
    cb = sizeof out; EQ(vget(h, L"emb", &t, out, &cb), 0); EQ(cb, sizeof s); CHECK(!memcmp(s, out, sizeof s));
    RegCloseKey(h);
}

/* ---------------- A variants ---------------- */
TEST(regkb_val_a_set_query_sz) {
    REQUIRE(U(RegSetValueExA)); REQUIRE(U(RegQueryValueExA));
    HKEY h = FRESH(); REQUIRE(h); char out[32]; DWORD cb = sizeof out, t;
    EQ(U(RegSetValueExA)(h, "s", 0, REG_SZ, (BYTE *)"ansi text", 10), 0);
    EQ(U(RegQueryValueExA)(h, "s", NULL, &t, (BYTE *)out, &cb), 0);
    EQ(t, REG_SZ); EQ(cb, 10); EXPECT_STR(out, "ansi text");
    WCHAR w[32]; cb = sizeof w; EQ(vget(h, L"s", &t, w, &cb), 0); EQ(cb, 20); EXPECT_WSTR(w, L"ansi text");
    RegCloseKey(h);
}
TEST(regkb_val_a_set_without_terminator_observed) {
    REQUIRE(U(RegSetValueExA)); REQUIRE(U(RegGetValueW));
    HKEY h = FRESH(); REQUIRE(h);
    EQ(U(RegSetValueExA)(h, "s", 0, REG_SZ, (BYTE *)"abcdXXXX", 4), 0);
    WCHAR w[16] = {0}; DWORD cb = sizeof w;
    EQ(U(RegGetValueW)(h, NULL, L"s", RRF_RT_REG_SZ, NULL, w, &cb), 0);
    EXPECT_WSTR(w, L"abcd");
    RegCloseKey(h);
}
TEST(regkb_val_a_nonascii_acp_roundtrip) {
    REQUIRE(U(RegSetValueExA)); REQUIRE(U(RegQueryValueExA)); REQUIRE(U(RegQueryValueExW));
    HKEY h = FRESH(); REQUIRE(h);
    char a[] = "caf\xe9 \xfc\xdf"; WCHAR expect[16];
    MultiByteToWideChar(CP_ACP, 0, a, -1, expect, 16);
    EQ(U(RegSetValueExA)(h, "n\xe9", 0, REG_SZ, (BYTE *)a, sizeof a), 0);
    WCHAR wname[8]; MultiByteToWideChar(CP_ACP, 0, "n\xe9", -1, wname, 8);
    WCHAR w[16]; DWORD cb = sizeof w, t;
    EQ(vget(h, wname, &t, w, &cb), 0); EXPECT_WSTR(w, expect);
    char back[32]; cb = sizeof back;
    EQ(U(RegQueryValueExA)(h, "n\xe9", NULL, &t, (BYTE *)back, &cb), 0); EXPECT_STR(back, a);
    RegCloseKey(h);
}
TEST(regkb_val_a_query_wide_stored_unicode_converts_to_acp) {
    REQUIRE(U(RegQueryValueExA));
    HKEY h = FRESH(); REQUIRE(h);
    WCHAR w[] = L"A\x00e9Z";
    sc_sz(h, L"u", w);
    char exp[16]; int n = WideCharToMultiByte(CP_ACP, 0, w, -1, exp, 16, NULL, NULL);
    char out[16]; DWORD cb = sizeof out, t;
    EQ(U(RegQueryValueExA)(h, "u", NULL, &t, (BYTE *)out, &cb), 0);
    EQ(t, REG_SZ); EQ(cb, n); EXPECT_STR(out, exp);
    RegCloseKey(h);
}
TEST(regkb_val_a_query_size_and_small_buffer) {
    REQUIRE(U(RegQueryValueExA));
    HKEY h = FRESH(); REQUIRE(h); sc_sz(h, L"s", L"abcdef");
    DWORD cb = 0, t;
    EQ(U(RegQueryValueExA)(h, "s", NULL, &t, NULL, &cb), 0);
    CHECK(cb >= 7); t_note("A size query for 6 chars -> %lu", cb);
    char out[16]; cb = 3;
    EQ(U(RegQueryValueExA)(h, "s", NULL, &t, (BYTE *)out, &cb), ERROR_MORE_DATA);
    CHECK(cb >= 7);
    cb = 6;   /* exactly the characters, no room for the terminator */
    LSTATUS s = U(RegQueryValueExA)(h, "s", NULL, &t, (BYTE *)out, &cb);
    CHECK(s == ERROR_MORE_DATA);
    cb = 7; EQ(U(RegQueryValueExA)(h, "s", NULL, &t, (BYTE *)out, &cb), 0); EQ(cb, 7); EXPECT_STR(out, "abcdef");
    RegCloseKey(h);
}
TEST(regkb_val_a_binary_not_converted) {
    REQUIRE(U(RegQueryValueExA)); REQUIRE(U(RegSetValueExA));
    HKEY h = FRESH(); REQUIRE(h);
    BYTE d[4] = { 0xE9, 0xFC, 0x00, 0xFF }, out[8]; DWORD cb = 8, t;
    EQ(U(RegSetValueExA)(h, "b", 0, REG_BINARY, d, 4), 0);
    EQ(U(RegQueryValueExA)(h, "b", NULL, &t, out, &cb), 0); EQ(cb, 4); CHECK(!memcmp(d, out, 4));
    RegCloseKey(h);
}
TEST(regkb_val_a_multi_sz) {
    REQUIRE(U(RegSetValueExA)); REQUIRE(U(RegQueryValueExA));
    HKEY h = FRESH(); REQUIRE(h); char m[] = "red\0green\0blue\0", out[32]; DWORD cb = sizeof out, t;
    EQ(U(RegSetValueExA)(h, "m", 0, REG_MULTI_SZ, (BYTE *)m, sizeof m), 0);
    EQ(U(RegQueryValueExA)(h, "m", NULL, &t, (BYTE *)out, &cb), 0);
    EQ(t, REG_MULTI_SZ); EQ(cb, sizeof m); CHECK(!memcmp(m, out, sizeof m));
    WCHAR w[32]; cb = sizeof w; EQ(vget(h, L"m", &t, w, &cb), 0); EQ(cb, 2 * sizeof m);
    RegCloseKey(h);
}
TEST(regkb_val_a_expand_sz_not_expanded) {
    REQUIRE(U(RegSetValueExA)); REQUIRE(U(RegQueryValueExA));
    HKEY h = FRESH(); REQUIRE(h); char s[] = "%TEMP%\\x", out[32]; DWORD cb = sizeof out, t;
    EQ(U(RegSetValueExA)(h, "e", 0, REG_EXPAND_SZ, (BYTE *)s, sizeof s), 0);
    EQ(U(RegQueryValueExA)(h, "e", NULL, &t, (BYTE *)out, &cb), 0); EQ(t, REG_EXPAND_SZ); EXPECT_STR(out, s);
    RegCloseKey(h);
}
TEST(regkb_val_a_reserved_and_null_checks) {
    REQUIRE(U(RegSetValueExA));
    HKEY h = FRESH(); REQUIRE(h); DWORD v = 1;
    EQ(U(RegSetValueExA)(h, "r", 1, REG_DWORD, (BYTE *)&v, 4), ERROR_INVALID_PARAMETER);
    EQ(U(RegSetValueExA)(h, "z", 0, REG_BINARY, NULL, 0), 0);
    RegCloseKey(h);
}
TEST(regkb_val_a_name_too_long_rejected) {
    REQUIRE(U(RegSetValueExA));
    HKEY h = FRESH(); REQUIRE(h);
    static char nm[40000]; memset(nm, 'x', 39999); nm[39999] = 0; DWORD v = 1;
    /* p1: the ANSI name is widened with RtlCreateUnicodeStringFromAsciiz, which fails (STATUS_NO_MEMORY)
     * once the UNICODE_STRING would exceed 0xFFFE bytes; RegSetValueExA maps that to ERROR_NOT_ENOUGH_MEMORY. */
    EQ(U(RegSetValueExA)(h, nm, 0, REG_DWORD, (BYTE *)&v, 4), ERROR_NOT_ENOUGH_MEMORY);
    RegCloseKey(h);
}

/* ---------------- RegDeleteValue / RegDeleteKeyValue ---------------- */
TEST(regkb_val_delete_value_basic) {
    REQUIRE(U(RegDeleteValueW)); REQUIRE(U(RegDeleteValueA));
    HKEY h = FRESH(); REQUIRE(h); sc_dw(h, L"a", 1); sc_dw(h, L"b", 2);
    EQ(U(RegDeleteValueW)(h, L"A"), 0);
    EQ(rk_valcount(h), 1);
    EQ(U(RegDeleteValueW)(h, L"a"), ERROR_FILE_NOT_FOUND);
    EQ(U(RegDeleteValueA)(h, "b"), 0);
    EQ(U(RegDeleteValueA)(h, "b"), ERROR_FILE_NOT_FOUND);
    EQ(rk_valcount(h), 0);
    RegCloseKey(h);
}
TEST(regkb_val_delete_default_value) {
    REQUIRE(U(RegDeleteValueW)); REQUIRE(U(RegDeleteValueA));
    HKEY h = FRESH(); REQUIRE(h);
    sc_sz(h, NULL, L"d"); EQ(rk_valcount(h), 1);
    EQ(U(RegDeleteValueW)(h, NULL), 0); EQ(rk_valcount(h), 0);
    sc_sz(h, L"", L"d"); EQ(U(RegDeleteValueW)(h, L""), 0); EQ(rk_valcount(h), 0);
    sc_sz(h, L"", L"d"); EQ(U(RegDeleteValueA)(h, NULL), 0); EQ(rk_valcount(h), 0);
    EQ(U(RegDeleteValueW)(h, NULL), ERROR_FILE_NOT_FOUND);
    RegCloseKey(h);
}
TEST(regkb_val_delete_value_invalid_handle) {
    REQUIRE(U(RegDeleteValueW));
    EQ(U(RegDeleteValueW)(NULL, L"x"), ERROR_INVALID_HANDLE);
}
TEST(regkb_val_delete_value_keeps_others_and_subkeys) {
    REQUIRE(U(RegDeleteValueW));
    HKEY h = FRESH(); REQUIRE(h); sc_dw(h, L"a", 1); sc_dw(h, L"b", 2); sc_subc(h, L"s");
    EQ(U(RegDeleteValueW)(h, L"s"), ERROR_FILE_NOT_FOUND);   /* subkey names are not values */
    EQ(rk_subcount(h), 1); EQ(rk_valcount(h), 2);
    RegCloseKey(h);
}
TEST(regkb_val_deletekeyvalue_w) {
    REQUIRE(U(RegDeleteKeyValueW));
    HKEY h = FRESH(); REQUIRE(h);
    HKEY s = sc_sub(h, L"sub"); sc_dw(s, L"val", 1); sc_dw(h, L"val", 2); RegCloseKey(s);
    EQ(U(RegDeleteKeyValueW)(h, L"sub", L"val"), 0);
    EQ(U(RegDeleteKeyValueW)(h, L"sub", L"val"), ERROR_FILE_NOT_FOUND);
    EQ(U(RegDeleteKeyValueW)(h, L"missing", L"val"), ERROR_FILE_NOT_FOUND);
    EQ(U(RegDeleteKeyValueW)(h, NULL, L"val"), 0);
    EQ(rk_valcount(h), 0);
    RegCloseKey(h);
}
TEST(regkb_val_deletekeyvalue_a) {
    REQUIRE(U(RegDeleteKeyValueA));
    HKEY h = FRESH(); REQUIRE(h);
    HKEY s = sc_sub(h, L"sub"); sc_dw(s, L"val", 1); RegCloseKey(s);
    EQ(U(RegDeleteKeyValueA)(h, "sub", "val"), 0);
    EQ(U(RegDeleteKeyValueA)(h, "sub", "val"), ERROR_FILE_NOT_FOUND);
    RegCloseKey(h);
}
TEST(regkb_val_deletekeyvalue_default) {
    REQUIRE(U(RegDeleteKeyValueW));
    HKEY h = FRESH(); REQUIRE(h);
    HKEY s = sc_sub(h, L"sub"); sc_sz(s, NULL, L"dv"); RegCloseKey(s);
    EQ(U(RegDeleteKeyValueW)(h, L"sub", NULL), 0);
    RegCloseKey(h);
}

/* ---------------- RegSetKeyValue ---------------- */
TEST(regkb_val_setkeyvalue_creates_subkey) {
    REQUIRE(U(RegSetKeyValueW)); REQUIRE(U(RegGetValueW));
    HKEY h = FRESH(); REQUIRE(h); DWORD v = 77;
    EQ(U(RegSetKeyValueW)(h, L"new\\nested", L"dw", REG_DWORD, &v, 4), 0);
    CHECK(rk_exists(h, "new\\nested"));
    DWORD out = 0, cb = 4;
    EQ(U(RegGetValueW)(h, L"new\\nested", L"dw", RRF_RT_REG_DWORD, NULL, &out, &cb), 0); EQ(out, 77);
    RegCloseKey(h);
}
TEST(regkb_val_setkeyvalue_null_subkey) {
    REQUIRE(U(RegSetKeyValueW));
    HKEY h = FRESH(); REQUIRE(h); DWORD v = 5, out = 0, cb = 4, t;
    EQ(U(RegSetKeyValueW)(h, NULL, L"direct", REG_DWORD, &v, 4), 0);
    EQ(vget(h, L"direct", &t, &out, &cb), 0); EQ(out, 5);
    EQ(U(RegSetKeyValueW)(h, L"", L"direct2", REG_DWORD, &v, 4), 0);
    RegCloseKey(h);
}
TEST(regkb_val_setkeyvalue_default_value) {
    REQUIRE(U(RegSetKeyValueW));
    HKEY h = FRESH(); REQUIRE(h); WCHAR s[] = L"deflt", out[16]; DWORD cb = sizeof out, t;
    EQ(U(RegSetKeyValueW)(h, L"k", NULL, REG_SZ, s, sizeof s), 0);
    HKEY k; EQ(RegOpenKeyExW(h, L"k", 0, KEY_READ, &k), 0);
    EQ(vget(k, NULL, &t, out, &cb), 0); EXPECT_WSTR(out, s);
    RegCloseKey(k); RegCloseKey(h);
}
TEST(regkb_val_setkeyvalue_a) {
    REQUIRE(U(RegSetKeyValueA)); REQUIRE(U(RegGetValueA));
    HKEY h = FRESH(); REQUIRE(h); char out[16]; DWORD cb = sizeof out;
    EQ(U(RegSetKeyValueA)(h, "ka\\sub", "sv", REG_SZ, "text", 5), 0);
    EQ(U(RegGetValueA)(h, "ka\\sub", "sv", RRF_RT_REG_SZ, NULL, out, &cb), 0); EXPECT_STR(out, "text"); EQ(cb, 5);
    RegCloseKey(h);
}
TEST(regkb_val_setkeyvalue_invalid_handle_and_access) {
    REQUIRE(U(RegSetKeyValueW));
    DWORD v = 1;
    CHECK(U(RegSetKeyValueW)(NULL, L"k", L"v", REG_DWORD, &v, 4) != 0);
    HKEY h = FRESH(); REQUIRE(h); HKEY ro; RegOpenKeyExW(h, NULL, 0, KEY_READ, &ro);
    /* p1: creating a subkey is checked against the parent's security descriptor, not the handle's
     * granted rights, so a subkey path would succeed; a NULL subkey writes the value on ro itself. */
    EQ(U(RegSetKeyValueW)(ro, NULL, L"v", REG_DWORD, &v, 4), ERROR_ACCESS_DENIED);
    /* the original subkey form: observed 0 on Windows 10 (create checked against the parent SD, value set on the new handle) */
    EQ(U(RegSetKeyValueW)(ro, L"k", L"v", REG_DWORD, &v, 4), 0);
    RegDeleteKeyW(h, L"k");
    RegCloseKey(ro); RegCloseKey(h);
}
TEST(regkb_val_setkeyvalue_reserved_name_too_long) {
    REQUIRE(U(RegSetKeyValueW));
    HKEY h = FRESH(); REQUIRE(h); static WCHAR nm[40000]; DWORD v = 1;
    rk_fill_w(nm, 32768, L'x');
    EQ(U(RegSetKeyValueW)(h, L"k", nm, REG_DWORD, &v, 4), ERROR_INVALID_PARAMETER);
    RegCloseKey(h);
}

/* ---------------- legacy RegSetValue / RegQueryValue ---------------- */
TEST(regkb_val_legacy_setvalue_creates_subkey_default) {
    REQUIRE(UA(RegSetValueW)); REQUIRE(UA(RegQueryValueW));
    HKEY h = FRESH(); REQUIRE(h); WCHAR out[32]; LONG cb = sizeof out;
    EQ(UA(RegSetValueW)(h, L"legacy\\sub", REG_SZ, L"legacy text", 0), 0);
    CHECK(rk_exists(h, "legacy\\sub"));
    EQ(UA(RegQueryValueW)(h, L"legacy\\sub", out, &cb), 0);
    EXPECT_WSTR(out, L"legacy text"); EQ(cb, 24);
    HKEY k; EQ(RegOpenKeyExW(h, L"legacy\\sub", 0, KEY_READ, &k), 0);
    DWORD t = 0, c2 = sizeof out; EQ(RegQueryValueExW(k, NULL, NULL, &t, (BYTE *)out, &c2), 0); EQ(t, REG_SZ);
    RegCloseKey(k); RegCloseKey(h);
}
TEST(regkb_val_legacy_setvalue_non_sz_rejected) {
    REQUIRE(UA(RegSetValueW)); REQUIRE(UA(RegSetValueA));
    HKEY h = FRESH(); REQUIRE(h);
    EQ(UA(RegSetValueW)(h, L"s", REG_DWORD, L"xxxx", 4), ERROR_INVALID_PARAMETER);
    EQ(UA(RegSetValueA)(h, "s", REG_BINARY, "xxxx", 4), ERROR_INVALID_PARAMETER);
    EQ(UA(RegSetValueW)(h, L"s", REG_EXPAND_SZ, L"xxxx", 4), ERROR_INVALID_PARAMETER);
    RegCloseKey(h);
}
TEST(regkb_val_legacy_setvalue_null_subkey_sets_own_default) {
    REQUIRE(UA(RegSetValueW)); REQUIRE(UA(RegQueryValueW));
    HKEY h = FRESH(); REQUIRE(h); WCHAR out[16]; LONG cb = sizeof out;
    EQ(UA(RegSetValueW)(h, NULL, REG_SZ, L"mine", 0), 0);
    EQ(UA(RegQueryValueW)(h, NULL, out, &cb), 0); EXPECT_WSTR(out, L"mine");
    cb = sizeof out; EQ(UA(RegQueryValueW)(h, L"", out, &cb), 0);
    RegCloseKey(h);
}
TEST(regkb_val_legacy_queryvalue_size_and_small_buffer) {
    REQUIRE(UA(RegSetValueW)); REQUIRE(UA(RegQueryValueW));
    HKEY h = FRESH(); REQUIRE(h); WCHAR out[16]; LONG cb = 0;
    EQ(UA(RegSetValueW)(h, L"s", REG_SZ, L"abc", 0), 0);
    EQ(UA(RegQueryValueW)(h, L"s", NULL, &cb), 0); EQ(cb, 8);
    cb = 4; EQ(UA(RegQueryValueW)(h, L"s", out, &cb), ERROR_MORE_DATA); EQ(cb, 8);
    cb = 8; EQ(UA(RegQueryValueW)(h, L"s", out, &cb), 0);
    RegCloseKey(h);
}
TEST(regkb_val_legacy_queryvalue_missing_subkey) {
    REQUIRE(UA(RegQueryValueW)); REQUIRE(UA(RegQueryValueA));
    HKEY h = FRESH(); REQUIRE(h); WCHAR out[8]; LONG cb = sizeof out; char oa[8];
    EQ(UA(RegQueryValueW)(h, L"missing", out, &cb), ERROR_FILE_NOT_FOUND);
    cb = sizeof oa; EQ(UA(RegQueryValueA)(h, "missing", oa, &cb), ERROR_FILE_NOT_FOUND);
    RegCloseKey(h);
}
TEST(regkb_val_legacy_queryvalue_no_default_observed) {
    REQUIRE(UA(RegQueryValueW));
    HKEY h = FRESH(); REQUIRE(h); WCHAR out[8] = {1}; LONG cb = sizeof out;
    LSTATUS s = UA(RegQueryValueW)(h, NULL, out, &cb);
    t_note("RegQueryValueW on key without default = %ld cb=%ld", (long)s, (long)cb);
    RegCloseKey(h);
}
TEST(regkb_val_legacy_a_roundtrip) {
    REQUIRE(UA(RegSetValueA)); REQUIRE(UA(RegQueryValueA));
    HKEY h = FRESH(); REQUIRE(h); char out[32]; LONG cb = sizeof out;
    EQ(UA(RegSetValueA)(h, "la", REG_SZ, "ansi legacy", 0), 0);
    EQ(UA(RegQueryValueA)(h, "la", out, &cb), 0); EXPECT_STR(out, "ansi legacy"); EQ(cb, 12);
    RegCloseKey(h);
}
