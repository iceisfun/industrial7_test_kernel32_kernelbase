#include "rk.h"

static LSTATUS gv(HKEY h, const WCHAR *sub, const WCHAR *n, DWORD fl, DWORD *t, void *d, DWORD *cb) {
    return U(RegGetValueW)(h, sub, n, fl, t, d, cb);
}
/* A key holding one value of every common type. */
static HKEY mk_typed(const char *name) {
    HKEY h = rk_fresh(name); if (!h) return NULL;
    unsigned long long q = 0x1122334455667788ULL;
    BYTE bin[4] = { 1, 2, 3, 4 };
    WCHAR multi[] = L"aa\0bb\0";
    sc_raw(h, L"none", REG_NONE, bin, 4);
    sc_sz(h, L"sz", L"text");
    sc_raw(h, L"exp", REG_EXPAND_SZ, L"%SystemRoot%\\k32t", sizeof L"%SystemRoot%\\k32t");
    sc_raw(h, L"bin", REG_BINARY, bin, 4);
    sc_dw(h, L"dw", 0x01020304);
    sc_raw(h, L"multi", REG_MULTI_SZ, multi, sizeof multi);
    sc_raw(h, L"qw", REG_QWORD, &q, 8);
    return h;
}

TEST(regkb_getvalue_flags_zero_invalid) {
    REQUIRE(U(RegGetValueW)); REQUIRE(U(RegGetValueA));
    HKEY h = mk_typed(__func__); REQUIRE(h); DWORD v, cb = 4;
    EQ(gv(h, NULL, L"dw", 0, NULL, &v, &cb), ERROR_INVALID_PARAMETER);
    EQ(U(RegGetValueA)(h, NULL, "dw", 0, NULL, &v, &cb), ERROR_INVALID_PARAMETER);
    EQ(gv(h, NULL, L"dw", RRF_NOEXPAND, NULL, &v, &cb), ERROR_INVALID_PARAMETER);       /* only modifier bits */
    EQ(gv(h, NULL, L"dw", RRF_ZEROONFAILURE, NULL, &v, &cb), ERROR_INVALID_PARAMETER);
    RegCloseKey(h);
}
TEST(regkb_getvalue_null_key_invalid) {
    REQUIRE(U(RegGetValueW));
    DWORD v, cb = 4;
    EQ(gv(NULL, NULL, L"dw", RRF_RT_ANY, NULL, &v, &cb), ERROR_INVALID_PARAMETER);
}
TEST(regkb_getvalue_data_without_size_invalid) {
    REQUIRE(U(RegGetValueW));
    HKEY h = mk_typed(__func__); REQUIRE(h); DWORD v;
    EQ(gv(h, NULL, L"dw", RRF_RT_ANY, NULL, &v, NULL), ERROR_INVALID_PARAMETER);
    EQ(gv(h, L"", L"dw", RRF_RT_ANY, NULL, &v, NULL), ERROR_INVALID_PARAMETER);
    RegCloseKey(h);
}
TEST(regkb_getvalue_both_wow_flags_invalid) {
    REQUIRE(U(RegGetValueW));
    HKEY h = mk_typed(__func__); REQUIRE(h); DWORD v, cb = 4;
    EQ(gv(h, NULL, L"dw", RRF_RT_ANY | RRF_SUBKEY_WOW6464KEY | RRF_SUBKEY_WOW6432KEY, NULL, &v, &cb), ERROR_INVALID_PARAMETER);
    RegCloseKey(h);
}
TEST(regkb_getvalue_missing_value_and_subkey) {
    REQUIRE(U(RegGetValueW));
    HKEY h = mk_typed(__func__); REQUIRE(h); DWORD v, cb = 4;
    EQ(gv(h, NULL, L"nope", RRF_RT_ANY, NULL, &v, &cb), ERROR_FILE_NOT_FOUND);
    EQ(gv(h, L"nosub", L"dw", RRF_RT_ANY, NULL, &v, &cb), ERROR_FILE_NOT_FOUND);
    EQ(gv(h, L"nosub\\deeper", L"dw", RRF_RT_ANY, NULL, &v, &cb), ERROR_FILE_NOT_FOUND);
    RegCloseKey(h);
}
TEST(regkb_getvalue_type_match_matrix) {
    REQUIRE(U(RegGetValueW));
    HKEY h = mk_typed(__func__); REQUIRE(h);
    struct { const WCHAR *name; DWORD type; DWORD ok_flag; } vals[] = {
        { L"none", REG_NONE, RRF_RT_REG_NONE }, { L"sz", REG_SZ, RRF_RT_REG_SZ },
        { L"bin", REG_BINARY, RRF_RT_REG_BINARY }, { L"dw", REG_DWORD, RRF_RT_REG_DWORD },
        { L"multi", REG_MULTI_SZ, RRF_RT_REG_MULTI_SZ }, { L"qw", REG_QWORD, RRF_RT_REG_QWORD },
    };
    DWORD flags[] = { RRF_RT_REG_NONE, RRF_RT_REG_SZ, RRF_RT_REG_BINARY, RRF_RT_REG_DWORD, RRF_RT_REG_MULTI_SZ, RRF_RT_REG_QWORD };
    for (size_t i = 0; i < 6; i++) for (size_t j = 0; j < 6; j++) {
        BYTE buf[64]; DWORD cb = sizeof buf, t = 0;
        LSTATUS s = gv(h, NULL, vals[i].name, flags[j], &t, buf, &cb);
        DWORD want = (flags[j] == vals[i].ok_flag) ? 0 : ERROR_UNSUPPORTED_TYPE;
        CHECKF(s == want, "value %ls flag 0x%lx -> %ld want %lu", vals[i].name, flags[j], (long)s, want);
        if (s == 0) EQ(t, vals[i].type);
    }
    EQ(ERROR_UNSUPPORTED_TYPE, 1630);
    RegCloseKey(h);
}
TEST(regkb_getvalue_type_match_matrix_with_subkey) {
    REQUIRE(U(RegGetValueW));
    HKEY h = mk_typed("regkb_getvalue_type_match_matrix_with_subkey"); REQUIRE(h);
    HKEY r = rk_root();
    struct { const WCHAR *name; DWORD ok_flag; } vals[] = {
        { L"none", RRF_RT_REG_NONE }, { L"sz", RRF_RT_REG_SZ }, { L"bin", RRF_RT_REG_BINARY },
        { L"dw", RRF_RT_REG_DWORD }, { L"multi", RRF_RT_REG_MULTI_SZ }, { L"qw", RRF_RT_REG_QWORD },
    };
    DWORD flags[] = { RRF_RT_REG_NONE, RRF_RT_REG_SZ, RRF_RT_REG_BINARY, RRF_RT_REG_DWORD, RRF_RT_REG_MULTI_SZ, RRF_RT_REG_QWORD };
    for (size_t i = 0; i < 6; i++) for (size_t j = 0; j < 6; j++) {
        BYTE buf[64]; DWORD cb = sizeof buf, t;
        LSTATUS s = gv(r, L"regkb_getvalue_type_match_matrix_with_subkey", vals[i].name, flags[j], &t, buf, &cb);
        DWORD want = (flags[j] == vals[i].ok_flag) ? 0 : ERROR_UNSUPPORTED_TYPE;
        CHECKF(s == want, "subkey path: value %ls flag 0x%lx -> %ld want %lu", vals[i].name, flags[j], (long)s, want);
    }
    RegCloseKey(h);
}
TEST(regkb_getvalue_rrf_rt_any_all_types) {
    REQUIRE(U(RegGetValueW));
    HKEY h = mk_typed(__func__); REQUIRE(h);
    const WCHAR *n[] = { L"none", L"sz", L"bin", L"dw", L"multi", L"qw" };
    DWORD types[] = { REG_NONE, REG_SZ, REG_BINARY, REG_DWORD, REG_MULTI_SZ, REG_QWORD };
    for (int i = 0; i < 6; i++) {
        BYTE buf[64]; DWORD cb = sizeof buf, t = 0;
        EQ(gv(h, NULL, n[i], RRF_RT_ANY, &t, buf, &cb), 0); EQ(t, types[i]);
    }
    RegCloseKey(h);
}
TEST(regkb_getvalue_unknown_type_requires_rt_any) {
    REQUIRE(U(RegGetValueW));
    HKEY h = FRESH(); REQUIRE(h); BYTE d[3] = { 1, 2, 3 }, buf[8]; DWORD cb = 8, t;
    sc_raw(h, L"odd", 0x77, d, 3);
    EQ(gv(h, NULL, L"odd", RRF_RT_ANY, &t, buf, &cb), 0); EQ(t, 0x77);
    cb = 8; EQ(gv(h, NULL, L"odd", 0x7FFF, &t, buf, &cb), ERROR_UNSUPPORTED_TYPE);
    cb = 8; EQ(gv(h, NULL, L"odd", RRF_RT_REG_BINARY, &t, buf, &cb), ERROR_UNSUPPORTED_TYPE);
    RegCloseKey(h);
}
TEST(regkb_getvalue_link_and_resource_types_need_rt_any) {
    REQUIRE(U(RegGetValueW));
    HKEY h = FRESH(); REQUIRE(h); BYTE d[4] = { 1, 2, 3, 4 }, buf[8]; DWORD cb = 8, t;
    sc_raw(h, L"res", REG_RESOURCE_LIST, d, 4);
    EQ(gv(h, NULL, L"res", RRF_RT_ANY, &t, buf, &cb), 0); EQ(t, REG_RESOURCE_LIST);
    cb = 8; EQ(gv(h, NULL, L"res", RRF_RT_REG_BINARY, &t, buf, &cb), ERROR_UNSUPPORTED_TYPE);
    RegCloseKey(h);
}
TEST(regkb_getvalue_combined_flags) {
    REQUIRE(U(RegGetValueW));
    HKEY h = mk_typed(__func__); REQUIRE(h); BYTE buf[64]; DWORD cb = sizeof buf, t;
    DWORD fl = RRF_RT_REG_SZ | RRF_RT_REG_DWORD;
    EQ(gv(h, NULL, L"sz", fl, &t, buf, &cb), 0); EQ(t, REG_SZ);
    cb = sizeof buf; EQ(gv(h, NULL, L"dw", fl, &t, buf, &cb), 0); EQ(t, REG_DWORD);
    cb = sizeof buf; EQ(gv(h, NULL, L"bin", fl, &t, buf, &cb), ERROR_UNSUPPORTED_TYPE);
    RegCloseKey(h);
}
TEST(regkb_getvalue_rrf_rt_dword_qword_aliases) {
    REQUIRE(U(RegGetValueW));
    HKEY h = mk_typed(__func__); REQUIRE(h); BYTE buf[64]; DWORD cb = sizeof buf, t;
    EQ(RRF_RT_DWORD, (RRF_RT_REG_BINARY | RRF_RT_REG_DWORD));
    EQ(RRF_RT_QWORD, (RRF_RT_REG_BINARY | RRF_RT_REG_QWORD));
    cb = 4; EQ(gv(h, NULL, L"dw", RRF_RT_DWORD, &t, buf, &cb), 0); EQ(t, REG_DWORD);
    unsigned long long q = 0; cb = 8; EQ(gv(h, NULL, L"qw", RRF_RT_QWORD, &t, &q, &cb), 0); EQ(t, REG_QWORD); CHECK(q == 0x1122334455667788ULL);
    /* an 8-byte binary satisfies RRF_RT_QWORD, a 9-byte binary does not */
    BYTE b8[8] = { 1 }, b9[9] = { 1 };
    sc_raw(h, L"b8", REG_BINARY, b8, 8); sc_raw(h, L"b9", REG_BINARY, b9, 9);
    cb = sizeof buf; EQ(gv(h, NULL, L"b8", RRF_RT_QWORD, &t, buf, &cb), 0);
    cb = sizeof buf; EQ(gv(h, NULL, L"b9", RRF_RT_QWORD, &t, buf, &cb), ERROR_DATATYPE_MISMATCH);
    cb = sizeof buf; EQ(gv(h, L"", L"b9", RRF_RT_QWORD, &t, buf, &cb), ERROR_DATATYPE_MISMATCH);
    EQ(ERROR_DATATYPE_MISMATCH, 1629);
    RegCloseKey(h);
}
TEST(regkb_getvalue_rt_dword_on_binary_observed) {
    REQUIRE(U(RegGetValueW));
    HKEY h = FRESH(); REQUIRE(h); BYTE b4[4] = { 1, 2, 3, 4 }, b5[5] = { 1, 2, 3, 4, 5 }, buf[16]; DWORD cb, t;
    sc_raw(h, L"b4", REG_BINARY, b4, 4); sc_raw(h, L"b5", REG_BINARY, b5, 5);
    cb = 16; LSTATUS s = gv(h, NULL, L"b4", RRF_RT_DWORD, &t, buf, &cb); t_note("RT_DWORD on 4-byte binary = %ld", (long)s);
    cb = 16; s = gv(h, NULL, L"b5", RRF_RT_DWORD, &t, buf, &cb); t_note("RT_DWORD on 5-byte binary = %ld", (long)s);
    RegCloseKey(h);
}
TEST(regkb_getvalue_size_query_and_exact_buffer) {
    REQUIRE(U(RegGetValueW));
    HKEY h = mk_typed(__func__); REQUIRE(h); DWORD cb = 0, t = 0;
    EQ(gv(h, NULL, L"dw", RRF_RT_ANY, &t, NULL, &cb), 0); EQ(cb, 4); EQ(t, REG_DWORD);
    cb = 0; EQ(gv(h, NULL, L"bin", RRF_RT_ANY, &t, NULL, &cb), 0); EQ(cb, 4);
    cb = 0; EQ(gv(h, NULL, L"multi", RRF_RT_ANY, &t, NULL, &cb), 0); CHECK(cb >= 14);
    BYTE buf[8]; cb = 2;
    EQ(gv(h, NULL, L"bin", RRF_RT_ANY, &t, buf, &cb), ERROR_MORE_DATA); EQ(cb, 4);
    cb = 4; EQ(gv(h, NULL, L"bin", RRF_RT_ANY, &t, buf, &cb), 0);
    EQ(gv(h, NULL, L"dw", RRF_RT_ANY, NULL, NULL, NULL), 0);   /* existence check */
    EQ(gv(h, NULL, L"nope", RRF_RT_ANY, NULL, NULL, NULL), ERROR_FILE_NOT_FOUND);
    RegCloseKey(h);
}
TEST(regkb_getvalue_sz_size_query_covers_terminator) {
    REQUIRE(U(RegGetValueW));
    HKEY h = mk_typed(__func__); REQUIRE(h); DWORD cb = 0, t;
    EQ(gv(h, NULL, L"sz", RRF_RT_REG_SZ, &t, NULL, &cb), 0);
    CHECK(cb >= 10);
    t_note("size query of stored L\"text\" (10 bytes) reports %lu", cb);
    WCHAR *p = malloc(cb); REQUIRE(p); DWORD cb2 = cb;
    EQ(gv(h, NULL, L"sz", RRF_RT_REG_SZ, &t, p, &cb2), 0); EXPECT_WSTR(p, L"text");
    free(p);
    cb = 0; EQ(gv(rk_root(), L"test_regkb_getvalue_sz_size_query_covers_terminator", L"sz", RRF_RT_REG_SZ, &t, NULL, &cb), 0);
    CHECK(cb >= 10);
    RegCloseKey(h);
}
TEST(regkb_getvalue_sz_exact_and_short_buffer) {
    REQUIRE(U(RegGetValueW));
    HKEY h = mk_typed(__func__); REQUIRE(h); WCHAR out[16]; DWORD cb, t;
    cb = 10; memset(out, 0xCC, sizeof out); EQ(gv(h, NULL, L"sz", RRF_RT_REG_SZ, &t, out, &cb), 0); EQ(cb, 10); EXPECT_WSTR(out, L"text");
    cb = 8; EQ(gv(h, NULL, L"sz", RRF_RT_REG_SZ, &t, out, &cb), ERROR_MORE_DATA); CHECK(cb >= 10);
    cb = 0; EQ(gv(h, NULL, L"sz", RRF_RT_REG_SZ, &t, out, &cb), ERROR_MORE_DATA);
    RegCloseKey(h);
}
TEST(regkb_getvalue_unterminated_sz_is_terminated) {
    REQUIRE(U(RegGetValueW));
    HKEY h = FRESH(); REQUIRE(h);
    WCHAR raw[3] = { L'x', L'y', L'z' };
    if (rk_ntset(h, L"u", REG_SZ, raw, 6) != 0) { t_skip("NtSetValueKey unavailable"); RegCloseKey(h); return; }
    WCHAR out[8]; DWORD cb = sizeof out, t; memset(out, 0xCC, sizeof out);
    EQ(gv(h, NULL, L"u", RRF_RT_REG_SZ, &t, out, &cb), 0);
    EXPECT_WSTR(out, L"xyz"); EQ(cb, 8);                       /* size now includes the added terminator */
    cb = 6; memset(out, 0xCC, sizeof out);
    EQ(gv(h, NULL, L"u", RRF_RT_REG_SZ, &t, out, &cb), ERROR_MORE_DATA);   /* no room for terminator */
    CHECK(cb >= 8);
    /* same through a subkey path */
    cb = sizeof out; memset(out, 0xCC, sizeof out);
    EQ(gv(rk_root(), L"test_regkb_getvalue_unterminated_sz_is_terminated", L"u", RRF_RT_REG_SZ, &t, out, &cb), 0);
    EXPECT_WSTR(out, L"xyz"); EQ(cb, 8);
    RegCloseKey(h);
}
TEST(regkb_getvalue_unterminated_multisz_is_terminated) {
    REQUIRE(U(RegGetValueW));
    HKEY h = FRESH(); REQUIRE(h);
    WCHAR raw[4] = { L'a', 0, L'b', L'c' };
    if (rk_ntset(h, L"um", REG_MULTI_SZ, raw, 8) != 0) { t_skip("NtSetValueKey unavailable"); RegCloseKey(h); return; }
    WCHAR out[16]; DWORD cb = sizeof out, t; memset(out, 0xCC, sizeof out);
    EQ(gv(h, NULL, L"um", RRF_RT_REG_MULTI_SZ, &t, out, &cb), 0);
    CHECK(cb >= 10); EQ(out[2], L'b'); EQ(out[3], L'c'); EQ(out[4], 0); EQ(out[5], 0);   /* double-NUL terminated */
    RegCloseKey(h);
}
TEST(regkb_getvalue_empty_sz_and_multisz) {
    REQUIRE(U(RegGetValueW));
    HKEY h = FRESH(); REQUIRE(h); WCHAR out[8]; DWORD cb, t;
    sc_raw(h, L"es", REG_SZ, NULL, 0);
    memset(out, 0xCC, sizeof out); cb = sizeof out;
    EQ(gv(h, NULL, L"es", RRF_RT_REG_SZ, &t, out, &cb), 0); EQ(out[0], 0); CHECK(cb >= 2);
    sc_raw(h, L"em", REG_MULTI_SZ, NULL, 0);
    memset(out, 0xCC, sizeof out); cb = sizeof out;
    EQ(gv(h, NULL, L"em", RRF_RT_REG_MULTI_SZ, &t, out, &cb), 0); EQ(out[0], 0);
    RegCloseKey(h);
}
TEST(regkb_getvalue_expand_sz_is_expanded) {
    REQUIRE(U(RegGetValueW));
    HKEY h = mk_typed(__func__); REQUIRE(h); WCHAR out[MAX_PATH + 32], expect[MAX_PATH + 32], root[MAX_PATH]; DWORD cb = sizeof out, t = 0;
    DWORD n = GetEnvironmentVariableW(L"SystemRoot", root, MAX_PATH); CHECK(n > 0);
    swprintf(expect, MAX_PATH + 32, L"%ls\\k32t", root);
    EQ(gv(h, NULL, L"exp", RRF_RT_REG_SZ, &t, out, &cb), 0);
    EQ(t, REG_SZ);                    /* reported as plain REG_SZ after expansion */
    CHECK(!lstrcmpiW(out, expect)); EQ(cb, (lstrlenW(out) + 1) * 2);
    t = 0; cb = sizeof out;
    EQ(gv(rk_root(), L"test_regkb_getvalue_expand_sz_is_expanded", L"exp", RRF_RT_REG_SZ, &t, out, &cb), 0);
    EQ(t, REG_SZ); CHECK(!lstrcmpiW(out, expect));
    RegCloseKey(h);
}
TEST(regkb_getvalue_expand_sz_needs_noexpand_to_match_expand_flag) {
    REQUIRE(U(RegGetValueW));
    HKEY h = mk_typed(__func__); REQUIRE(h); WCHAR out[MAX_PATH + 32]; DWORD cb = sizeof out, t = 0;
    /* without RRF_NOEXPAND the value is expanded and becomes REG_SZ, so only RRF_RT_REG_SZ accepts it */
    EQ(gv(h, NULL, L"exp", RRF_RT_REG_EXPAND_SZ, &t, out, &cb), ERROR_UNSUPPORTED_TYPE);
    cb = sizeof out; EQ(gv(h, NULL, L"exp", RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND, &t, out, &cb), 0);
    EQ(t, REG_EXPAND_SZ); EXPECT_WSTR(out, L"%SystemRoot%\\k32t");
    cb = sizeof out; EQ(gv(h, NULL, L"exp", RRF_RT_REG_SZ | RRF_NOEXPAND, &t, out, &cb), ERROR_UNSUPPORTED_TYPE);
    cb = sizeof out; EQ(gv(h, NULL, L"exp", RRF_RT_ANY | RRF_NOEXPAND, &t, out, &cb), 0); EQ(t, REG_EXPAND_SZ);
    cb = sizeof out; EQ(gv(h, NULL, L"exp", RRF_RT_ANY, &t, out, &cb), 0); EQ(t, REG_SZ);
    RegCloseKey(h);
}
TEST(regkb_getvalue_expand_sz_size_query) {
    REQUIRE(U(RegGetValueW));
    HKEY h = mk_typed(__func__); REQUIRE(h); DWORD cb = 0, t = 0; WCHAR *p;
    EQ(gv(h, NULL, L"exp", RRF_RT_REG_SZ, &t, NULL, &cb), 0);
    CHECK(cb >= 2); DWORD need = cb;
    p = malloc(need + 64); REQUIRE(p); DWORD cb2 = need;
    LSTATUS s = gv(h, NULL, L"exp", RRF_RT_REG_SZ, &t, p, &cb2);
    CHECKF(s == 0 || s == ERROR_MORE_DATA, "second call %ld", (long)s);
    t_note("expand size query: %lu, second call %ld (%lu)", need, (long)s, cb2);
    free(p); RegCloseKey(h);
}
TEST(regkb_getvalue_expand_undefined_variable_kept) {
    REQUIRE(U(RegGetValueW));
    HKEY h = FRESH(); REQUIRE(h); WCHAR out[64]; DWORD cb = sizeof out, t;
    sc_raw(h, L"e", REG_EXPAND_SZ, L"%K32T_NO_SUCH_VAR_ZZ%\\x", sizeof L"%K32T_NO_SUCH_VAR_ZZ%\\x");
    EQ(gv(h, NULL, L"e", RRF_RT_REG_SZ, &t, out, &cb), 0); EXPECT_WSTR(out, L"%K32T_NO_SUCH_VAR_ZZ%\\x");
    RegCloseKey(h);
}
TEST(regkb_getvalue_zeroonfailure) {
    REQUIRE(U(RegGetValueW));
    HKEY h = mk_typed(__func__); REQUIRE(h); BYTE buf[16]; DWORD cb, t;
    memset(buf, 0xCC, 16); cb = 16;
    EQ(gv(h, NULL, L"nope", RRF_RT_ANY | RRF_ZEROONFAILURE, &t, buf, &cb), ERROR_FILE_NOT_FOUND);
    for (int i = 0; i < 16; i++) EQ(buf[i], 0);
    memset(buf, 0xCC, 16); cb = 16;
    EQ(gv(h, NULL, L"dw", RRF_RT_REG_SZ | RRF_ZEROONFAILURE, &t, buf, &cb), ERROR_UNSUPPORTED_TYPE);
    for (int i = 0; i < 16; i++) EQ(buf[i], 0);
    memset(buf, 0xCC, 16); cb = 16;
    EQ(gv(h, L"nosub", L"dw", RRF_RT_ANY | RRF_ZEROONFAILURE, &t, buf, &cb), ERROR_FILE_NOT_FOUND);
    EQ(buf[0], 0); EQ(buf[15], 0);
    memset(buf, 0xCC, 16); cb = 16;
    EQ(gv(h, NULL, L"nope", RRF_RT_ANY, &t, buf, &cb), ERROR_FILE_NOT_FOUND);   /* without the flag: untouched */
    EQ(buf[0], 0xCC);
    RegCloseKey(h);
}
TEST(regkb_getvalue_zeroonfailure_success_keeps_data) {
    REQUIRE(U(RegGetValueW));
    HKEY h = mk_typed(__func__); REQUIRE(h); DWORD v = 0, cb = 4;
    EQ(gv(h, NULL, L"dw", RRF_RT_REG_DWORD | RRF_ZEROONFAILURE, NULL, &v, &cb), 0); EQ(v, 0x01020304);
    RegCloseKey(h);
}
TEST(regkb_getvalue_zeroonfailure_more_data_observed) {
    REQUIRE(U(RegGetValueW));
    HKEY h = mk_typed(__func__); REQUIRE(h); BYTE buf[16]; DWORD cb = 2;
    memset(buf, 0xCC, 16);
    LSTATUS s = gv(h, NULL, L"bin", RRF_RT_ANY | RRF_ZEROONFAILURE, NULL, buf, &cb);
    EQ(s, ERROR_MORE_DATA);
    t_note("buf[0] after MORE_DATA with ZEROONFAILURE: 0x%02x", buf[0]);
    RegCloseKey(h);
}
TEST(regkb_getvalue_default_value) {
    REQUIRE(U(RegGetValueW));
    HKEY h = FRESH(); REQUIRE(h); WCHAR out[16]; DWORD cb = sizeof out, t;
    EQ(gv(h, NULL, NULL, RRF_RT_ANY, &t, out, &cb), ERROR_FILE_NOT_FOUND);
    sc_sz(h, NULL, L"dflt");
    EQ(gv(h, NULL, NULL, RRF_RT_REG_SZ, &t, out, &cb), 0); EXPECT_WSTR(out, L"dflt");
    cb = sizeof out; EQ(gv(h, NULL, L"", RRF_RT_REG_SZ, &t, out, &cb), 0);
    RegCloseKey(h);
}
TEST(regkb_getvalue_subkey_variants) {
    REQUIRE(U(RegGetValueW));
    HKEY h = FRESH(); REQUIRE(h); HKEY s = sc_sub(h, L"a\\b"); sc_dw(s, L"v", 42); RegCloseKey(s);
    DWORD v = 0, cb = 4;
    EQ(gv(h, L"a\\b", L"v", RRF_RT_REG_DWORD, NULL, &v, &cb), 0); EQ(v, 42);
    v = 0; cb = 4; EQ(gv(h, L"A\\B", L"v", RRF_RT_REG_DWORD, NULL, &v, &cb), 0); EQ(v, 42);
    cb = 4; EQ(gv(h, L"a", L"v", RRF_RT_REG_DWORD, NULL, &v, &cb), ERROR_FILE_NOT_FOUND);
    cb = 4; EQ(gv(h, L"", L"v", RRF_RT_REG_DWORD, NULL, &v, &cb), ERROR_FILE_NOT_FOUND);
    RegCloseKey(h);
}
TEST(regkb_getvalue_wow_subkey_flags_on_shared_hkcu) {
    REQUIRE(U(RegGetValueW));
    HKEY h = FRESH(); REQUIRE(h); HKEY s = sc_sub(h, L"sk"); sc_dw(s, L"v", 7); RegCloseKey(s);
    DWORD v = 0, cb = 4;
    EQ(gv(h, L"sk", L"v", RRF_RT_REG_DWORD | RRF_SUBKEY_WOW6464KEY, NULL, &v, &cb), 0); EQ(v, 7);
    v = 0; cb = 4; EQ(gv(h, L"sk", L"v", RRF_RT_REG_DWORD | RRF_SUBKEY_WOW6432KEY, NULL, &v, &cb), 0); EQ(v, 7);
    /* the flags only apply when a subkey is opened */
    v = 0; cb = 4; EQ(gv(h, NULL, L"nope", RRF_RT_REG_DWORD | RRF_SUBKEY_WOW6432KEY, NULL, &v, &cb), ERROR_FILE_NOT_FOUND);
    RegCloseKey(h);
}
TEST(regkb_getvalue_hklm_wow_views_differ) {
    REQUIRE(U(RegGetValueW));
    WCHAR a[MAX_PATH], b[MAX_PATH]; DWORD ca = sizeof a, cb = sizeof b, t;
    LSTATUS s64 = gv(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion", L"ProgramFilesDir", RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY, &t, a, &ca);
    LSTATUS s32 = gv(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion", L"ProgramFilesDir", RRF_RT_REG_SZ | RRF_SUBKEY_WOW6432KEY, &t, b, &cb);
    EQ(s64, 0); EQ(s32, 0);
    if (!s64 && !s32) {
        t_note("ProgramFilesDir 64: %ls  32: %ls", a, b);
        CHECK(wcsstr(a, L"(x86)") == NULL);
        CHECK(wcsstr(b, L"(x86)") != NULL);
    }
}
TEST(regkb_getvalue_a_basic) {
    REQUIRE(U(RegGetValueA));
    HKEY h = mk_typed(__func__); REQUIRE(h); char out[32]; DWORD cb = sizeof out, t;
    EQ(U(RegGetValueA)(h, NULL, "sz", RRF_RT_REG_SZ, &t, out, &cb), 0); EXPECT_STR(out, "text"); EQ(t, REG_SZ); EQ(cb, 5);
    DWORD v = 0; cb = 4; EQ(U(RegGetValueA)(h, NULL, "dw", RRF_RT_REG_DWORD, &t, &v, &cb), 0); EQ(v, 0x01020304);
    cb = sizeof out; EQ(U(RegGetValueA)(h, NULL, "dw", RRF_RT_REG_SZ, &t, out, &cb), ERROR_UNSUPPORTED_TYPE);
    cb = sizeof out; EQ(U(RegGetValueA)(h, NULL, "nope", RRF_RT_ANY, &t, out, &cb), ERROR_FILE_NOT_FOUND);
    cb = 2; EQ(U(RegGetValueA)(h, NULL, "sz", RRF_RT_REG_SZ, &t, out, &cb), ERROR_MORE_DATA); CHECK(cb >= 5);
    RegCloseKey(h);
}
TEST(regkb_getvalue_a_expand_and_noexpand) {
    REQUIRE(U(RegGetValueA));
    HKEY h = FRESH(); REQUIRE(h); char out[MAX_PATH + 32], root[MAX_PATH], expect[MAX_PATH + 32]; DWORD cb = sizeof out, t;
    sc_raw(h, L"e", REG_EXPAND_SZ, L"%SystemRoot%\\a", sizeof L"%SystemRoot%\\a");
    GetEnvironmentVariableA("SystemRoot", root, MAX_PATH); snprintf(expect, sizeof expect, "%s\\a", root);
    EQ(U(RegGetValueA)(h, NULL, "e", RRF_RT_REG_SZ, &t, out, &cb), 0); EQ(t, REG_SZ); CHECK(!lstrcmpiA(out, expect));
    cb = sizeof out; EQ(U(RegGetValueA)(h, NULL, "e", RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND, &t, out, &cb), 0);
    EQ(t, REG_EXPAND_SZ); EXPECT_STR(out, "%SystemRoot%\\a");
    RegCloseKey(h);
}
TEST(regkb_getvalue_a_acp_conversion) {
    REQUIRE(U(RegGetValueA));
    HKEY h = FRESH(); REQUIRE(h); WCHAR w[] = L"na\x00efve"; sc_sz(h, L"u", w);
    char exp[16], out[16]; WideCharToMultiByte(CP_ACP, 0, w, -1, exp, 16, NULL, NULL); DWORD cb = sizeof out, t;
    EQ(U(RegGetValueA)(h, NULL, "u", RRF_RT_REG_SZ, &t, out, &cb), 0); EXPECT_STR(out, exp);
    RegCloseKey(h);
}
TEST(regkb_getvalue_hklm_productname) {
    REQUIRE(U(RegGetValueW));
    WCHAR out[128]; DWORD cb = sizeof out, t = 0;
    EQ(gv(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"ProductName", RRF_RT_REG_SZ, &t, out, &cb), 0);
    EQ(t, REG_SZ); CHECK(lstrlenW(out) > 4); EQ(cb, (lstrlenW(out) + 1) * 2);
    t_note("ProductName = %ls", out);
}
TEST(regkb_getvalue_predefined_key_with_subkey_path) {
    REQUIRE(U(RegGetValueW));
    DWORD v = 0, cb = 4, t;
    EQ(gv(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"CurrentMajorVersionNumber", RRF_RT_REG_DWORD, &t, &v, &cb), 0);
    EQ(v, 10);
    EQ(gv(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"CurrentMajorVersionNumber", RRF_RT_REG_SZ, &t, &v, &cb), ERROR_UNSUPPORTED_TYPE);
}

/* ---------------- RegQueryMultipleValues ---------------- */
TEST(regkb_multiple_values_basic) {
    REQUIRE(UA(RegQueryMultipleValuesW));
    HKEY h = FRESH(); REQUIRE(h);
    sc_sz(h, L"alpha", L"first"); sc_dw(h, L"beta", 0x12345678);
    BYTE bin[3] = { 7, 8, 9 }; sc_raw(h, L"gamma", REG_BINARY, bin, 3);
    VALENTW ve[3] = { { L"alpha" }, { L"beta" }, { L"gamma" } };
    WCHAR buf[128]; DWORD tot = sizeof buf; memset(buf, 0, sizeof buf);
    EQ(UA(RegQueryMultipleValuesW)(h, ve, 3, buf, &tot), 0);
    EQ(ve[0].ve_type, REG_SZ); EQ(ve[0].ve_valuelen, 12);
    EQ(ve[1].ve_type, REG_DWORD); EQ(ve[1].ve_valuelen, 4);
    EQ(ve[2].ve_type, REG_BINARY); EQ(ve[2].ve_valuelen, 3);
    CHECK(!lstrcmpW((WCHAR *)ve[0].ve_valueptr, L"first"));
    CHECK(*(DWORD *)ve[1].ve_valueptr == 0x12345678);
    CHECK(!memcmp((void *)ve[2].ve_valueptr, bin, 3));
    CHECK(tot >= 12 + 4 + 3);
    CHECK((BYTE *)ve[0].ve_valueptr >= (BYTE *)buf && (BYTE *)ve[2].ve_valueptr < (BYTE *)buf + sizeof buf);
    RegCloseKey(h);
}
TEST(regkb_multiple_values_missing_value) {
    REQUIRE(UA(RegQueryMultipleValuesW));
    HKEY h = FRESH(); REQUIRE(h); sc_dw(h, L"here", 1);
    VALENTW ve[2] = { { L"here" }, { L"gone" } }; WCHAR buf[32]; DWORD tot = sizeof buf;
    EQ(UA(RegQueryMultipleValuesW)(h, ve, 2, buf, &tot), ERROR_FILE_NOT_FOUND);
    RegCloseKey(h);
}
TEST(regkb_multiple_values_small_buffer_reports_size) {
    REQUIRE(UA(RegQueryMultipleValuesW));
    HKEY h = FRESH(); REQUIRE(h); sc_sz(h, L"s1", L"0123456789"); sc_sz(h, L"s2", L"abcdefghij");
    VALENTW ve[2] = { { L"s1" }, { L"s2" } }; WCHAR buf[64]; DWORD tot = 8;
    EQ(UA(RegQueryMultipleValuesW)(h, ve, 2, buf, &tot), ERROR_MORE_DATA);
    CHECK(tot >= 44);
    DWORD need = tot; tot = need;
    EQ(UA(RegQueryMultipleValuesW)(h, ve, 2, buf, &tot), 0);
    CHECK(!lstrcmpW((WCHAR *)ve[1].ve_valueptr, L"abcdefghij"));
    RegCloseKey(h);
}
TEST(regkb_multiple_values_size_query_null_buffer) {
    REQUIRE(UA(RegQueryMultipleValuesW));
    HKEY h = FRESH(); REQUIRE(h); sc_dw(h, L"a", 1); sc_dw(h, L"b", 2);
    VALENTW ve[2] = { { L"A" }, { L"B" } }; DWORD tot = 0;
    LSTATUS s = UA(RegQueryMultipleValuesW)(h, ve, 2, NULL, &tot);
    CHECK(s == ERROR_MORE_DATA || s == 0);
    CHECK(tot >= 8);
    t_note("size query: status %ld total %lu", (long)s, tot);
    RegCloseKey(h);
}
TEST(regkb_multiple_values_a_variant) {
    REQUIRE(UA(RegQueryMultipleValuesA));
    HKEY h = FRESH(); REQUIRE(h); sc_sz(h, L"s", L"txt"); sc_dw(h, L"d", 3);
    VALENTA ve[2] = { { "s" }, { "d" } }; char buf[64]; DWORD tot = sizeof buf;
    EQ(UA(RegQueryMultipleValuesA)(h, ve, 2, buf, &tot), 0);
    EQ(ve[0].ve_type, REG_SZ); EQ(ve[1].ve_type, REG_DWORD);
    CHECK(!strcmp((char *)ve[0].ve_valueptr, "txt"));
    EQ(*(DWORD *)ve[1].ve_valueptr, 3);
    RegCloseKey(h);
}
TEST(regkb_multiple_values_invalid_handle) {
    REQUIRE(UA(RegQueryMultipleValuesW));
    VALENTW ve[1] = { { L"x" } }; WCHAR buf[8]; DWORD tot = sizeof buf;
    EQ(UA(RegQueryMultipleValuesW)(NULL, ve, 1, buf, &tot), ERROR_INVALID_HANDLE);
}
TEST(regkb_multiple_values_zero_count_observed) {
    REQUIRE(UA(RegQueryMultipleValuesW));
    HKEY h = FRESH(); REQUIRE(h); WCHAR buf[8]; DWORD tot = sizeof buf;
    LSTATUS s = UA(RegQueryMultipleValuesW)(h, NULL, 0, buf, &tot);
    t_note("RegQueryMultipleValuesW(0 values) = %ld total %lu", (long)s, tot);
    RegCloseKey(h);
}

/* ---------------- RegLoadMUIString ---------------- */
TEST(regkb_muistring_argument_validation) {
    REQUIRE(U(RegLoadMUIStringW));
    HKEY h = FRESH(); REQUIRE(h); sc_sz(h, L"m", L"@%SystemRoot%\\system32\\shell32.dll,-21770");
    WCHAR out[64]; DWORD cb = 0;
    EQ(U(RegLoadMUIStringW)(h, L"m", out, sizeof out, NULL, 0, NULL), ERROR_INVALID_PARAMETER);      /* pcbData required with flags 0 */
    EQ(U(RegLoadMUIStringW)(h, L"m", out, sizeof out, &cb, 2, NULL), ERROR_INVALID_PARAMETER);       /* unknown flag */
    EQ(U(RegLoadMUIStringW)(h, L"m", out, sizeof out, &cb, REG_MUI_STRING_TRUNCATE, NULL), ERROR_INVALID_PARAMETER); /* pcbData forbidden with TRUNCATE */
    EQ(U(RegLoadMUIStringW)(h, L"m", out, 3, &cb, 0, NULL), ERROR_INVALID_PARAMETER);                /* odd byte count */
    EQ(U(RegLoadMUIStringW)(h, L"m", NULL, 8, &cb, 0, NULL), ERROR_INVALID_PARAMETER);               /* buffer missing */
    RegCloseKey(h);
}
TEST(regkb_muistring_missing_value) {
    REQUIRE(U(RegLoadMUIStringW));
    HKEY h = FRESH(); REQUIRE(h); WCHAR out[64]; DWORD cb = 0;
    EQ(U(RegLoadMUIStringW)(h, L"absent", out, sizeof out, &cb, 0, NULL), ERROR_FILE_NOT_FOUND);
    RegCloseKey(h);
}
TEST(regkb_muistring_indirect_shell32_string) {
    REQUIRE(U(RegLoadMUIStringW));
    HKEY h = FRESH(); REQUIRE(h); sc_sz(h, L"m", L"@%SystemRoot%\\system32\\shell32.dll,-21770");
    WCHAR out[256] = {0}; DWORD cb = 0;
    LSTATUS s = U(RegLoadMUIStringW)(h, L"m", out, sizeof out, &cb, 0, NULL);
    t_note("shell32 -21770 -> %ld \"%ls\" (cb %lu)", (long)s, out, cb);
    if (!t_is_wine()) { EQ(s, 0); CHECK(out[0] != 0); EQ(cb, (lstrlenW(out) + 1) * 2); }
    RegCloseKey(h);
}
TEST(regkb_muistring_buffer_too_small_reports_size) {
    REQUIRE(U(RegLoadMUIStringW));
    HKEY h = FRESH(); REQUIRE(h); sc_sz(h, L"m", L"@%SystemRoot%\\system32\\shell32.dll,-21770");
    WCHAR out[256] = {0}; DWORD cb = 0;
    if (U(RegLoadMUIStringW)(h, L"m", out, sizeof out, &cb, 0, NULL) != 0) { t_skip("string not resolvable here"); RegCloseKey(h); return; }
    DWORD full = cb; WCHAR small[2]; cb = 0;
    LSTATUS s = U(RegLoadMUIStringW)(h, L"m", small, sizeof small, &cb, 0, NULL);
    EQ(s, ERROR_MORE_DATA); EQ(cb, full);
    cb = 0; WCHAR tiny[4]; memset(tiny, 0xCC, sizeof tiny);
    s = U(RegLoadMUIStringW)(h, L"m", tiny, sizeof tiny, NULL, REG_MUI_STRING_TRUNCATE, NULL);
    t_note("TRUNCATE into 4 wchars -> %ld \"%ls\"", (long)s, tiny);
    RegCloseKey(h);
}
TEST(regkb_muistring_plain_string_value_observed) {
    REQUIRE(U(RegLoadMUIStringW));
    HKEY h = FRESH(); REQUIRE(h); sc_sz(h, L"plain", L"just text"); sc_dw(h, L"dw", 5);
    WCHAR out[64] = {0}; DWORD cb = 0;
    LSTATUS s = U(RegLoadMUIStringW)(h, L"plain", out, sizeof out, &cb, 0, NULL);
    t_note("plain REG_SZ value -> %ld \"%ls\"", (long)s, out);
    s = U(RegLoadMUIStringW)(h, L"dw", out, sizeof out, &cb, 0, NULL);
    CHECK(s != 0); t_note("REG_DWORD value -> %ld", (long)s);
    RegCloseKey(h);
}
TEST(regkb_muistring_a_variant_exists) {
    REQUIRE(U(RegLoadMUIStringA));
    HKEY h = FRESH(); REQUIRE(h); char out[64]; DWORD cb = 0;
    EQ(U(RegLoadMUIStringA)(h, "absent", out, sizeof out, &cb, 0, NULL), ERROR_FILE_NOT_FOUND);
    EQ(U(RegLoadMUIStringA)(h, "absent", out, sizeof out, NULL, 0, NULL), ERROR_INVALID_PARAMETER);
    RegCloseKey(h);
}
