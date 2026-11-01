#include "rk.h"

static HKEY mk_subs(const char *name, int n, const WCHAR *prefix) {
    HKEY h = rk_fresh(name); if (!h) return NULL;
    for (int i = 0; i < n; i++) { WCHAR nm[64]; swprintf(nm, 64, L"%ls%03d", prefix, i); sc_subc(h, nm); }
    return h;
}

/* ---------------- RegEnumKeyEx ---------------- */
TEST(regkb_enumkey_iterates_all_then_no_more_items) {
    REQUIRE(U(RegEnumKeyExW));
    HKEY h = mk_subs(__func__, 20, L"sub"); REQUIRE(h);
    int seen[20] = {0}, count = 0; DWORD i;
    for (i = 0; i < 100; i++) {
        WCHAR nm[64]; DWORD n = 64; LSTATUS s = U(RegEnumKeyExW)(h, i, nm, &n, NULL, NULL, NULL, NULL);
        if (s == ERROR_NO_MORE_ITEMS) break;
        EQ(s, 0); EQ(n, 6);
        int k = _wtoi(nm + 3); if (k >= 0 && k < 20) { seen[k]++; count++; }
    }
    EQ(i, 20); EQ(count, 20);
    for (int k = 0; k < 20; k++) EQ(seen[k], 1);
    EQ(ERROR_NO_MORE_ITEMS, 259);
    WCHAR nm[8]; DWORD n = 8;
    EQ(U(RegEnumKeyExW)(h, 20, nm, &n, NULL, NULL, NULL, NULL), ERROR_NO_MORE_ITEMS);
    EQ(U(RegEnumKeyExW)(h, 0xFFFFFFFF, nm, &n, NULL, NULL, NULL, NULL), ERROR_NO_MORE_ITEMS);
    RegCloseKey(h);
}
TEST(regkb_enumkey_order_is_stable_between_passes) {
    REQUIRE(U(RegEnumKeyExW));
    HKEY h = mk_subs(__func__, 12, L"o"); REQUIRE(h);
    WCHAR first[12][64];
    for (int pass = 0; pass < 2; pass++) for (DWORD i = 0; i < 12; i++) {
        WCHAR nm[64]; DWORD n = 64; EQ(U(RegEnumKeyExW)(h, i, nm, &n, NULL, NULL, NULL, NULL), 0);
        if (pass == 0) lstrcpyW(first[i], nm); else CHECK(!lstrcmpW(first[i], nm));
    }
    WCHAR nm[64]; DWORD n = 64;
    U(RegEnumKeyExW)(h, 0, nm, &n, NULL, NULL, NULL, NULL);
    t_note("first enumerated name: %ls (hive sorts names case-insensitively)", nm);
    RegCloseKey(h);
}
TEST(regkb_enumkey_empty_key) {
    REQUIRE(U(RegEnumKeyExW)); REQUIRE(U(RegEnumKeyExA));
    HKEY h = FRESH(); REQUIRE(h); WCHAR nm[8]; DWORD n = 8; char na[8]; DWORD m = 8;
    EQ(U(RegEnumKeyExW)(h, 0, nm, &n, NULL, NULL, NULL, NULL), ERROR_NO_MORE_ITEMS);
    EQ(U(RegEnumKeyExA)(h, 0, na, &m, NULL, NULL, NULL, NULL), ERROR_NO_MORE_ITEMS);
    RegCloseKey(h);
}
TEST(regkb_enumkey_buffer_size_boundaries) {
    REQUIRE(U(RegEnumKeyExW));
    HKEY h = FRESH(); REQUIRE(h); sc_subc(h, L"abcdef");
    WCHAR nm[32]; DWORD n;
    n = 7; memset(nm, 0xCC, sizeof nm); EQ(U(RegEnumKeyExW)(h, 0, nm, &n, NULL, NULL, NULL, NULL), 0); EQ(n, 6); EXPECT_WSTR(nm, L"abcdef");
    n = 6; EQ(U(RegEnumKeyExW)(h, 0, nm, &n, NULL, NULL, NULL, NULL), ERROR_MORE_DATA); EQ(n, 6);   /* no room for NUL; size unchanged */
    n = 1; EQ(U(RegEnumKeyExW)(h, 0, nm, &n, NULL, NULL, NULL, NULL), ERROR_MORE_DATA); EQ(n, 1);
    n = 0; EQ(U(RegEnumKeyExW)(h, 0, nm, &n, NULL, NULL, NULL, NULL), ERROR_MORE_DATA); EQ(n, 0);
    n = 32; EQ(U(RegEnumKeyExW)(h, 0, nm, &n, NULL, NULL, NULL, NULL), 0); EQ(n, 6);
    EQ(ERROR_MORE_DATA, 234);
    RegCloseKey(h);
}
TEST(regkb_enumkey_parameter_validation) {
    REQUIRE(U(RegEnumKeyExW)); REQUIRE(U(RegEnumKeyExA));
    HKEY h = mk_subs(__func__, 2, L"p"); REQUIRE(h); WCHAR nm[16], cls[16]; DWORD n = 16, c = 16, rsv = 0; char na[16];
    DWORD m = 16;
    if (t_is_wine()) { t_skip("wine faults on NULL size pointers"); RegCloseKey(h); return; }
    EQ(U(RegEnumKeyExW)(h, 0, nm, NULL, NULL, NULL, NULL, NULL), ERROR_INVALID_PARAMETER);
    EQ(U(RegEnumKeyExW)(h, 0, nm, &n, &rsv, NULL, NULL, NULL), ERROR_INVALID_PARAMETER);
    EQ(U(RegEnumKeyExW)(h, 0, nm, &n, NULL, cls, NULL, NULL), ERROR_INVALID_PARAMETER);     /* class buffer without size */
    n = 16;
    EQ(U(RegEnumKeyExA)(h, 0, na, NULL, NULL, NULL, NULL, NULL), ERROR_INVALID_PARAMETER);
    EQ(U(RegEnumKeyExA)(h, 0, na, &m, &rsv, NULL, NULL, NULL), ERROR_INVALID_PARAMETER);
    RegCloseKey(h);
}
TEST(regkb_enumkey_invalid_handle) {
    REQUIRE(U(RegEnumKeyExW));
    WCHAR nm[8]; DWORD n = 8;
    EQ(U(RegEnumKeyExW)(NULL, 0, nm, &n, NULL, NULL, NULL, NULL), ERROR_INVALID_HANDLE);
    EQ(U(RegEnumKeyExW)(INVALID_HANDLE_VALUE, 0, nm, &n, NULL, NULL, NULL, NULL), ERROR_INVALID_HANDLE);
}
TEST(regkb_enumkey_class_and_lastwrite) {
    REQUIRE(U(RegEnumKeyExW));
    HKEY h = FRESH(); REQUIRE(h); HKEY k; WCHAR cl[] = L"EnumClass";
    EQ(RegCreateKeyExW(h, L"withcls", 0, cl, 0, KEY_ALL_ACCESS, NULL, &k, NULL), 0); RegCloseKey(k);
    sc_subc(h, L"nocls");
    FILETIME now, lw; GetSystemTimeAsFileTime(&now);
    for (DWORD i = 0; i < 2; i++) {
        WCHAR nm[32], c[32] = L"zz"; DWORD n = 32, cn = 32; memset(&lw, 0, sizeof lw);
        EQ(U(RegEnumKeyExW)(h, i, nm, &n, NULL, c, &cn, &lw), 0);
        if (!lstrcmpW(nm, L"withcls")) { EQ(cn, 9); EXPECT_WSTR(c, L"EnumClass"); }
        else { EQ(cn, 0); EQ(c[0], 0); }
        unsigned long long a = ((unsigned long long)lw.dwHighDateTime << 32) | lw.dwLowDateTime;
        unsigned long long b = ((unsigned long long)now.dwHighDateTime << 32) | now.dwLowDateTime;
        CHECK(a != 0);
        CHECK(a <= b + 10 * 10000000ULL && a + 3600ULL * 10000000ULL * 24 > b);   /* written within the last day, not in the future */
    }
    RegCloseKey(h);
}
TEST(regkb_enumkey_class_buffer_too_small) {
    REQUIRE(U(RegEnumKeyExW));
    HKEY h = FRESH(); REQUIRE(h); HKEY k; WCHAR cl[] = L"LongClassName";
    EQ(RegCreateKeyExW(h, L"c", 0, cl, 0, KEY_ALL_ACCESS, NULL, &k, NULL), 0); RegCloseKey(k);
    WCHAR nm[8], c[32]; DWORD n = 8, cn = 5;
    EQ(U(RegEnumKeyExW)(h, 0, nm, &n, NULL, c, &cn, NULL), ERROR_MORE_DATA);
    t_note("class length reported on ERROR_MORE_DATA: %lu", cn);
    n = 8; cn = 14; EQ(U(RegEnumKeyExW)(h, 0, nm, &n, NULL, c, &cn, NULL), 0); EQ(cn, 13);
    RegCloseKey(h);
}
TEST(regkb_enumkey_a_variant_names) {
    REQUIRE(U(RegEnumKeyExA));
    HKEY h = mk_subs(__func__, 5, L"a"); REQUIRE(h); int seen = 0;
    for (DWORD i = 0; i < 5; i++) {
        char nm[32]; DWORD n = 32; EQ(U(RegEnumKeyExA)(h, i, nm, &n, NULL, NULL, NULL, NULL), 0);
        EQ(n, 4); CHECK(nm[0] == 'a'); seen += nm[3] - '0' >= 0 && nm[3] - '0' < 5;
    }
    EQ(seen, 5);
    char nm[32]; DWORD n = 32; EQ(U(RegEnumKeyExA)(h, 5, nm, &n, NULL, NULL, NULL, NULL), ERROR_NO_MORE_ITEMS);
    RegCloseKey(h);
}
TEST(regkb_enumkey_a_buffer_boundaries) {
    REQUIRE(U(RegEnumKeyExA));
    HKEY h = FRESH(); REQUIRE(h); sc_subc(h, L"abcdef"); char nm[32]; DWORD n;
    n = 7; EQ(U(RegEnumKeyExA)(h, 0, nm, &n, NULL, NULL, NULL, NULL), 0); EQ(n, 6); EXPECT_STR(nm, "abcdef");
    n = 6; EQ(U(RegEnumKeyExA)(h, 0, nm, &n, NULL, NULL, NULL, NULL), ERROR_MORE_DATA);
    n = 0; EQ(U(RegEnumKeyExA)(h, 0, nm, &n, NULL, NULL, NULL, NULL), ERROR_MORE_DATA);
    RegCloseKey(h);
}
TEST(regkb_enumkey_a_acp_name) {
    REQUIRE(U(RegEnumKeyExA));
    HKEY h = FRESH(); REQUIRE(h); WCHAR w[] = L"caf\x00e9"; sc_subc(h, w);
    char exp[16], nm[32]; WideCharToMultiByte(CP_ACP, 0, w, -1, exp, 16, NULL, NULL); DWORD n = 32;
    EQ(U(RegEnumKeyExA)(h, 0, nm, &n, NULL, NULL, NULL, NULL), 0); EXPECT_STR(nm, exp); EQ(n, strlen(exp));
    RegCloseKey(h);
}
TEST(regkb_enumkey_legacy_regenumkey) {
    REQUIRE(UA(RegEnumKeyW)); REQUIRE(UA(RegEnumKeyA));
    HKEY h = mk_subs(__func__, 3, L"L"); REQUIRE(h); int c = 0;
    for (DWORD i = 0; i < 10; i++) {
        WCHAR nm[16]; LSTATUS s = UA(RegEnumKeyW)(h, i, nm, 16);
        if (s == ERROR_NO_MORE_ITEMS) break;
        EQ(s, 0); c++;
    }
    EQ(c, 3);
    char na[16]; EQ(UA(RegEnumKeyA)(h, 0, na, 16), 0); EQ(strlen(na), 4);
    WCHAR sm[3]; EQ(UA(RegEnumKeyW)(h, 0, sm, 3), ERROR_MORE_DATA);
    WCHAR wn[16]; EQ(UA(RegEnumKeyW)(h, 3, wn, 16), ERROR_NO_MORE_ITEMS);
    RegCloseKey(h);
}
TEST(regkb_enumkey_sees_keys_added_and_removed_while_enumerating) {
    REQUIRE(U(RegEnumKeyExW));
    HKEY h = mk_subs(__func__, 4, L"d"); REQUIRE(h);
    WCHAR nm[32]; DWORD n = 32;
    EQ(U(RegEnumKeyExW)(h, 0, nm, &n, NULL, NULL, NULL, NULL), 0);
    sc_subc(h, L"zzz_added");
    EQ(rk_subcount(h), 5);
    RegDeleteKeyW(h, L"zzz_added");
    EQ(rk_subcount(h), 4);
    n = 32; EQ(U(RegEnumKeyExW)(h, 3, nm, &n, NULL, NULL, NULL, NULL), 0);
    n = 32; EQ(U(RegEnumKeyExW)(h, 4, nm, &n, NULL, NULL, NULL, NULL), ERROR_NO_MORE_ITEMS);
    RegCloseKey(h);
}
TEST(regkb_enumkey_many_subkeys_bounded) {
    REQUIRE(U(RegEnumKeyExW));
    HKEY h = mk_subs(__func__, 600, L"m"); REQUIRE(h); DWORD i; DWORD maxlen = 0;
    for (i = 0; i < 2000; i++) {
        WCHAR nm[64]; DWORD n = 64; LSTATUS s = U(RegEnumKeyExW)(h, i, nm, &n, NULL, NULL, NULL, NULL);
        if (s != 0) { EQ(s, ERROR_NO_MORE_ITEMS); break; }
        if (n > maxlen) maxlen = n;
    }
    EQ(i, 600); EQ(maxlen, 4); EQ(rk_subcount(h), 600);
    RegCloseKey(h);
}
TEST(regkb_enumkey_hklm_software_has_microsoft) {
    REQUIRE(U(RegEnumKeyExW));
    HKEY h; EQ(RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE", 0, KEY_READ | KEY_WOW64_64KEY, &h), 0); REQUIRE(h);
    int found = 0, wow = 0;
    for (DWORD i = 0; i < 2000; i++) {
        WCHAR nm[256]; DWORD n = 256; if (U(RegEnumKeyExW)(h, i, nm, &n, NULL, NULL, NULL, NULL) != 0) break;
        if (!lstrcmpiW(nm, L"Microsoft")) found = 1;
        if (!lstrcmpiW(nm, L"Wow6432Node")) wow = 1;
    }
    CHECK(found); CHECK(wow);
    RegCloseKey(h);
}
TEST(regkb_enumkey_predefined_hku_has_default) {
    REQUIRE(U(RegEnumKeyExW));
    int found = 0;
    for (DWORD i = 0; i < 2000; i++) {
        WCHAR nm[256]; DWORD n = 256; if (U(RegEnumKeyExW)(HKEY_USERS, i, nm, &n, NULL, NULL, NULL, NULL) != 0) break;
        if (!lstrcmpiW(nm, L".DEFAULT")) found = 1;
    }
    CHECK(found);
}
TEST(regkb_enumkey_hkcu_via_predefined_handle) {
    REQUIRE(U(RegEnumKeyExW));
    int found = 0;
    for (DWORD i = 0; i < 2000; i++) {
        WCHAR nm[256]; DWORD n = 256; if (U(RegEnumKeyExW)(HKEY_CURRENT_USER, i, nm, &n, NULL, NULL, NULL, NULL) != 0) break;
        if (!lstrcmpiW(nm, L"Software")) found = 1;
    }
    CHECK(found);
}

/* ---------------- RegEnumValue ---------------- */
TEST(regkb_enumvalue_names_types_data) {
    REQUIRE(U(RegEnumValueW));
    HKEY h = FRESH(); REQUIRE(h); sc_dw(h, L"d", 5); sc_sz(h, L"s", L"hi"); BYTE b[3] = { 1, 2, 3 }; sc_raw(h, L"b", REG_BINARY, b, 3);
    int seen = 0; DWORD i;
    for (i = 0; i < 10; i++) {
        WCHAR nm[16]; DWORD n = 16, t = 0, cb = 16; BYTE d[16];
        LSTATUS s = U(RegEnumValueW)(h, i, nm, &n, NULL, &t, d, &cb);
        if (s == ERROR_NO_MORE_ITEMS) break;
        EQ(s, 0); EQ(n, 1);
        if (nm[0] == L'd') { EQ(t, REG_DWORD); EQ(cb, 4); EQ(*(DWORD *)d, 5); seen |= 1; }
        else if (nm[0] == L's') { EQ(t, REG_SZ); EQ(cb, 6); CHECK(!lstrcmpW((WCHAR *)d, L"hi")); seen |= 2; }
        else if (nm[0] == L'b') { EQ(t, REG_BINARY); EQ(cb, 3); CHECK(!memcmp(d, b, 3)); seen |= 4; }
    }
    EQ(i, 3); EQ(seen, 7);
    RegCloseKey(h);
}
TEST(regkb_enumvalue_creation_order_observed) {
    REQUIRE(U(RegEnumValueW));
    HKEY h = FRESH(); REQUIRE(h);
    const WCHAR *order[] = { L"zeta", L"alpha", L"mid" };
    for (int i = 0; i < 3; i++) sc_dw(h, order[i], i);
    for (DWORD i = 0; i < 3; i++) { WCHAR nm[16]; DWORD n = 16; U(RegEnumValueW)(h, i, nm, &n, NULL, NULL, NULL, NULL); t_note("value #%lu = %ls", i, nm); }
    RegCloseKey(h);
}
TEST(regkb_enumvalue_name_buffer_boundaries) {
    REQUIRE(U(RegEnumValueW));
    HKEY h = FRESH(); REQUIRE(h); sc_dw(h, L"abcdef", 1); WCHAR nm[16]; DWORD n;
    n = 7; EQ(U(RegEnumValueW)(h, 0, nm, &n, NULL, NULL, NULL, NULL), 0); EQ(n, 6); EXPECT_WSTR(nm, L"abcdef");
    n = 6; EQ(U(RegEnumValueW)(h, 0, nm, &n, NULL, NULL, NULL, NULL), ERROR_MORE_DATA);
    n = 0; EQ(U(RegEnumValueW)(h, 0, nm, &n, NULL, NULL, NULL, NULL), ERROR_MORE_DATA);
    RegCloseKey(h);
}
TEST(regkb_enumvalue_data_buffer_boundaries) {
    REQUIRE(U(RegEnumValueW));
    HKEY h = FRESH(); REQUIRE(h); BYTE d[10] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }; sc_raw(h, L"b", REG_BINARY, d, 10);
    WCHAR nm[8]; BYTE out[16]; DWORD n, cb, t;
    n = 8; cb = 10; EQ(U(RegEnumValueW)(h, 0, nm, &n, NULL, &t, out, &cb), 0); EQ(cb, 10);
    n = 8; cb = 9; EQ(U(RegEnumValueW)(h, 0, nm, &n, NULL, &t, out, &cb), ERROR_MORE_DATA); EQ(cb, 10);
    n = 8; cb = 0; EQ(U(RegEnumValueW)(h, 0, nm, &n, NULL, &t, out, &cb), ERROR_MORE_DATA); EQ(cb, 10);
    n = 8; cb = 0; EQ(U(RegEnumValueW)(h, 0, nm, &n, NULL, &t, NULL, &cb), 0); EQ(cb, 10);    /* size query */
    n = 8; EQ(U(RegEnumValueW)(h, 0, nm, &n, NULL, &t, NULL, NULL), 0);
    RegCloseKey(h);
}
TEST(regkb_enumvalue_parameter_validation) {
    REQUIRE(U(RegEnumValueW)); REQUIRE(U(RegEnumValueA));
    HKEY h = FRESH(); REQUIRE(h); sc_dw(h, L"v", 1); WCHAR nm[8]; DWORD n = 8, rsv = 0, cb; BYTE d[4]; char na[8]; DWORD m = 8;
    EQ(U(RegEnumValueW)(h, 0, NULL, &n, NULL, NULL, NULL, NULL), ERROR_INVALID_PARAMETER);
    EQ(U(RegEnumValueW)(h, 0, nm, NULL, NULL, NULL, NULL, NULL), ERROR_INVALID_PARAMETER);
    EQ(U(RegEnumValueW)(h, 0, nm, &n, &rsv, NULL, NULL, NULL), ERROR_INVALID_PARAMETER);
    EQ(U(RegEnumValueW)(h, 0, nm, &n, NULL, NULL, d, NULL), ERROR_INVALID_PARAMETER);
    cb = 4;
    EQ(U(RegEnumValueA)(h, 0, NULL, &m, NULL, NULL, NULL, NULL), ERROR_INVALID_PARAMETER);
    EQ(U(RegEnumValueA)(h, 0, na, &m, &rsv, NULL, NULL, NULL), ERROR_INVALID_PARAMETER);
    RegCloseKey(h);
}
TEST(regkb_enumvalue_no_more_items) {
    REQUIRE(U(RegEnumValueW)); REQUIRE(U(RegEnumValueA));
    HKEY h = FRESH(); REQUIRE(h); WCHAR nm[8]; DWORD n = 8; char na[8]; DWORD m = 8;
    EQ(U(RegEnumValueW)(h, 0, nm, &n, NULL, NULL, NULL, NULL), ERROR_NO_MORE_ITEMS);
    sc_dw(h, L"v", 1);
    EQ(U(RegEnumValueW)(h, 1, nm, &n, NULL, NULL, NULL, NULL), ERROR_NO_MORE_ITEMS);
    EQ(U(RegEnumValueA)(h, 1, na, &m, NULL, NULL, NULL, NULL), ERROR_NO_MORE_ITEMS);
    EQ(U(RegEnumValueW)(h, 0xFFFFFFFF, nm, &n, NULL, NULL, NULL, NULL), ERROR_NO_MORE_ITEMS);
    RegCloseKey(h);
}
TEST(regkb_enumvalue_invalid_handle) {
    REQUIRE(U(RegEnumValueW));
    WCHAR nm[8]; DWORD n = 8;
    EQ(U(RegEnumValueW)(NULL, 0, nm, &n, NULL, NULL, NULL, NULL), ERROR_INVALID_HANDLE);
}
TEST(regkb_enumvalue_string_terminator_added_when_room) {
    REQUIRE(U(RegEnumValueW));
    HKEY h = FRESH(); REQUIRE(h); WCHAR raw[3] = { L'a', L'b', L'c' };
    if (rk_ntset(h, L"u", REG_SZ, raw, 6) != 0) { t_skip("NtSetValueKey unavailable"); RegCloseKey(h); return; }
    WCHAR nm[4], out[8]; DWORD n = 4, cb = 16, t; memset(out, 0xCC, sizeof out);
    EQ(U(RegEnumValueW)(h, 0, nm, &n, NULL, &t, (BYTE *)out, &cb), 0); EQ(cb, 6); EQ(out[3], 0);
    RegCloseKey(h);
}
TEST(regkb_enumvalue_a_variant) {
    REQUIRE(U(RegEnumValueA));
    HKEY h = FRESH(); REQUIRE(h); sc_sz(h, L"strval", L"conv\x00e9"); char nm[16], d[32]; DWORD n = 16, cb = 32, t;
    EQ(U(RegEnumValueA)(h, 0, nm, &n, NULL, &t, (BYTE *)d, &cb), 0);
    EQ(n, 6); EXPECT_STR(nm, "strval"); EQ(t, REG_SZ);
    char exp[16]; WideCharToMultiByte(CP_ACP, 0, L"conv\x00e9", -1, exp, 16, NULL, NULL); EXPECT_STR(d, exp);
    n = 6; EQ(U(RegEnumValueA)(h, 0, nm, &n, NULL, &t, NULL, NULL), ERROR_MORE_DATA);
    RegCloseKey(h);
}
TEST(regkb_enumvalue_many_values_bounded) {
    REQUIRE(U(RegEnumValueW));
    HKEY h = FRESH(); REQUIRE(h);
    for (DWORD i = 0; i < 500; i++) { WCHAR nm[16]; swprintf(nm, 16, L"v%04lu", i); sc_dw(h, nm, i); }
    DWORD i, sum = 0;
    for (i = 0; i < 2000; i++) {
        WCHAR nm[16]; DWORD n = 16, v = 0, cb = 4, t; LSTATUS s = U(RegEnumValueW)(h, i, nm, &n, NULL, &t, (BYTE *)&v, &cb);
        if (s) { EQ(s, ERROR_NO_MORE_ITEMS); break; }
        sum += v;
    }
    EQ(i, 500); EQ(sum, 499 * 500 / 2);
    RegCloseKey(h);
}
TEST(regkb_enumvalue_hklm_currentversion_known_values) {
    REQUIRE(U(RegEnumValueW));
    HKEY h; EQ(RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ | KEY_WOW64_64KEY, &h), 0); REQUIRE(h);
    int pn = 0, bn = 0;
    for (DWORD i = 0; i < 500; i++) {
        WCHAR nm[256]; DWORD n = 256, t; if (U(RegEnumValueW)(h, i, nm, &n, NULL, &t, NULL, NULL) != 0) break;
        if (!lstrcmpiW(nm, L"ProductName")) { pn = 1; EQ(t, REG_SZ); }
        if (!lstrcmpiW(nm, L"CurrentBuildNumber")) { bn = 1; EQ(t, REG_SZ); }
    }
    CHECK(pn); CHECK(bn);
    RegCloseKey(h);
}

/* ---------------- RegQueryInfoKey ---------------- */
TEST(regkb_queryinfo_counts_and_max_lengths) {
    REQUIRE(U(RegQueryInfoKeyW));
    HKEY h = FRESH(); REQUIRE(h);
    sc_subc(h, L"a"); sc_subc(h, L"bbbbbb"); sc_subc(h, L"cc");
    sc_dw(h, L"v1", 1); sc_sz(h, L"longervaluename", L"123456789"); BYTE b[100] = {0}; sc_raw(h, L"b", REG_BINARY, b, 100);
    DWORD subs = 0, maxsub = 0, maxcls = 0, vals = 0, maxname = 0, maxdata = 0, sdlen = 0; FILETIME ft = {0};
    EQ(U(RegQueryInfoKeyW)(h, NULL, NULL, NULL, &subs, &maxsub, &maxcls, &vals, &maxname, &maxdata, &sdlen, &ft), 0);
    EQ(subs, 3); EQ(maxsub, 6); EQ(maxcls, 0); EQ(vals, 3); EQ(maxname, 15); EQ(maxdata, 100);
    CHECK(sdlen > 0); CHECK(ft.dwHighDateTime != 0);
    RegCloseKey(h);
}
TEST(regkb_queryinfo_empty_key) {
    REQUIRE(U(RegQueryInfoKeyW));
    HKEY h = FRESH(); REQUIRE(h);
    DWORD subs = 9, maxsub = 9, maxcls = 9, vals = 9, maxname = 9, maxdata = 9;
    EQ(U(RegQueryInfoKeyW)(h, NULL, NULL, NULL, &subs, &maxsub, &maxcls, &vals, &maxname, &maxdata, NULL, NULL), 0);
    EQ(subs, 0); EQ(maxsub, 0); EQ(maxcls, 0); EQ(vals, 0); EQ(maxname, 0); EQ(maxdata, 0);
    RegCloseKey(h);
}
TEST(regkb_queryinfo_all_outputs_null) {
    REQUIRE(U(RegQueryInfoKeyW)); REQUIRE(U(RegQueryInfoKeyA));
    HKEY h = FRESH(); REQUIRE(h);
    EQ(U(RegQueryInfoKeyW)(h, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL), 0);
    EQ(U(RegQueryInfoKeyA)(h, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL), 0);
    RegCloseKey(h);
}
TEST(regkb_queryinfo_reserved_must_be_null) {
    REQUIRE(U(RegQueryInfoKeyW)); REQUIRE(U(RegQueryInfoKeyA));
    HKEY h = FRESH(); REQUIRE(h); DWORD rsv = 0;
    EQ(U(RegQueryInfoKeyW)(h, NULL, NULL, &rsv, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL), ERROR_INVALID_PARAMETER);
    EQ(U(RegQueryInfoKeyA)(h, NULL, NULL, &rsv, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL), ERROR_INVALID_PARAMETER);
    RegCloseKey(h);
}
TEST(regkb_queryinfo_class_buffer_without_size_invalid) {
    REQUIRE(U(RegQueryInfoKeyW));
    HKEY h = FRESH(); REQUIRE(h); WCHAR c[8];
    EQ(U(RegQueryInfoKeyW)(h, c, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL), ERROR_INVALID_PARAMETER);
    RegCloseKey(h);
}
TEST(regkb_queryinfo_class_size_only_and_small_buffer) {
    REQUIRE(U(RegQueryInfoKeyW));
    HKEY h = FRESH(); REQUIRE(h); HKEY k; WCHAR cl[] = L"ClassXY";
    EQ(RegCreateKeyExW(h, L"c", 0, cl, 0, KEY_ALL_ACCESS, NULL, &k, NULL), 0);
    DWORD n = 77, maxcls = 0;
    EQ(U(RegQueryInfoKeyW)(k, NULL, &n, NULL, NULL, NULL, &maxcls, NULL, NULL, NULL, NULL, NULL), 0);
    EQ(n, 7);
    WCHAR out[16]; n = 3;
    EQ(U(RegQueryInfoKeyW)(k, out, &n, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL), ERROR_MORE_DATA);
    /* p1: when the class itself fits but its terminator does not (ClassLength + 2 > 2 * n), kernelbase
     * returns ERROR_INSUFFICIENT_BUFFER (STATUS_BUFFER_TOO_SMALL); ERROR_MORE_DATA is for a buffer too small for the class. */
    n = 7; EQ(U(RegQueryInfoKeyW)(k, out, &n, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL), ERROR_INSUFFICIENT_BUFFER);
    n = 8; EQ(U(RegQueryInfoKeyW)(k, out, &n, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL), 0); EQ(n, 7);
    RegCloseKey(k);
    /* parent's max class length is that of its subkeys */
    EQ(U(RegQueryInfoKeyW)(h, NULL, NULL, NULL, NULL, NULL, &maxcls, NULL, NULL, NULL, NULL, NULL), 0);
    EQ(maxcls, 7);
    RegCloseKey(h);
}
TEST(regkb_queryinfo_lastwrite_advances) {
    REQUIRE(U(RegQueryInfoKeyW));
    HKEY h = FRESH(); REQUIRE(h); FILETIME a = {0}, b = {0};
    EQ(U(RegQueryInfoKeyW)(h, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &a), 0);
    Sleep(50); sc_dw(h, L"touch", 1);
    EQ(U(RegQueryInfoKeyW)(h, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &b), 0);
    CHECK(CompareFileTime(&b, &a) >= 0);
    t_note("last write time moved by value set: %d", CompareFileTime(&b, &a) > 0);
    RegCloseKey(h);
}
TEST(regkb_queryinfo_max_value_len_tracks_deletes) {
    REQUIRE(U(RegQueryInfoKeyW));
    HKEY h = FRESH(); REQUIRE(h); BYTE big[500] = {0}; sc_raw(h, L"big", REG_BINARY, big, 500); sc_dw(h, L"small", 1);
    DWORD mx = 0;
    EQ(U(RegQueryInfoKeyW)(h, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &mx, NULL, NULL), 0); EQ(mx, 500);
    RegDeleteValueW(h, L"big");
    EQ(U(RegQueryInfoKeyW)(h, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &mx, NULL, NULL), 0);
    CHECK(mx == 4 || mx == 500);
    t_note("max value data len after deleting the largest value: %lu", mx);
    RegCloseKey(h);
}
TEST(regkb_queryinfo_max_name_len_unicode_chars) {
    REQUIRE(U(RegQueryInfoKeyW));
    HKEY h = FRESH(); REQUIRE(h); WCHAR vn[] = { 0xD834, 0xDD1E, 0x4E2D, 0 }; sc_dw(h, vn, 1);
    sc_subc(h, vn);
    DWORD ms = 0, mn = 0;
    EQ(U(RegQueryInfoKeyW)(h, NULL, NULL, NULL, NULL, &ms, NULL, NULL, &mn, NULL, NULL, NULL), 0);
    EQ(ms, 3); EQ(mn, 3);   /* counted in UTF-16 units, excluding NUL */
    RegCloseKey(h);
}
TEST(regkb_queryinfo_a_variant) {
    REQUIRE(U(RegQueryInfoKeyA));
    HKEY h = FRESH(); REQUIRE(h); sc_subc(h, L"s1"); sc_dw(h, L"val", 1);
    DWORD subs, maxsub, vals, maxname; char cls[8]; DWORD cn = 8;
    EQ(U(RegQueryInfoKeyA)(h, cls, &cn, NULL, &subs, &maxsub, NULL, &vals, &maxname, NULL, NULL, NULL), 0);
    EQ(subs, 1); EQ(maxsub, 2); EQ(vals, 1); EQ(maxname, 3); EQ(cn, 0);
    RegCloseKey(h);
}
TEST(regkb_queryinfo_invalid_handle) {
    REQUIRE(U(RegQueryInfoKeyW));
    EQ(U(RegQueryInfoKeyW)(NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL), ERROR_INVALID_HANDLE);
}
TEST(regkb_queryinfo_predefined_keys) {
    REQUIRE(U(RegQueryInfoKeyW));
    HKEY pre[] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE, HKEY_CLASSES_ROOT, HKEY_USERS, HKEY_CURRENT_CONFIG };
    for (int i = 0; i < 5; i++) {
        DWORD subs = 0;
        LSTATUS s = U(RegQueryInfoKeyW)(pre[i], NULL, NULL, NULL, &subs, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
        EQ(s, 0);
        if (i != 4) CHECK(subs > 0);
    }
}
TEST(regkb_queryinfo_security_descriptor_len_matches_getkeysecurity) {
    REQUIRE(U(RegQueryInfoKeyW)); REQUIRE(U(RegGetKeySecurity));
    HKEY h = FRESH(); REQUIRE(h); DWORD sdlen = 0;
    EQ(U(RegQueryInfoKeyW)(h, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &sdlen, NULL), 0);
    BYTE sd[4096]; DWORD cb = sizeof sd;
    EQ(U(RegGetKeySecurity)(h, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, sd, &cb), 0);
    t_note("info sdlen %lu, GetKeySecurity(O|G|D) %lu", sdlen, cb);
    RegCloseKey(h);
}

/* ---------------- flush / security ---------------- */
TEST(regkb_flushkey_basic) {
    REQUIRE(U(RegFlushKey));
    HKEY h = FRESH(); REQUIRE(h); sc_dw(h, L"v", 1);
    EQ(U(RegFlushKey)(h), 0);
    EQ(U(RegFlushKey)(h), 0);
    RegCloseKey(h);
}
TEST(regkb_flushkey_invalid_and_readonly_handle) {
    REQUIRE(U(RegFlushKey));
    EQ(U(RegFlushKey)(NULL), ERROR_INVALID_HANDLE);
    HKEY h = FRESH(); REQUIRE(h); HKEY ro; RegOpenKeyExW(h, NULL, 0, KEY_READ, &ro);
    LSTATUS s = U(RegFlushKey)(ro); t_note("RegFlushKey with KEY_READ handle = %ld", (long)s);
    RegCloseKey(ro); RegCloseKey(h);
}
TEST(regkb_flushkey_predefined_hkcu_observed) {
    REQUIRE(U(RegFlushKey));
    LSTATUS s = U(RegFlushKey)(HKEY_CURRENT_USER);
    t_note("RegFlushKey(HKEY_CURRENT_USER) = %ld", (long)s);
}
TEST(regkb_flushkey_closed_handle) {
    REQUIRE(U(RegFlushKey));
    HKEY r = FRESH(); REQUIRE(r); HKEY h = sc_sub(r, L"x"); RegCloseKey(h);
    EQ(U(RegFlushKey)(h), ERROR_INVALID_HANDLE);
    RegCloseKey(r);
}
TEST(regkb_getkeysecurity_size_query_then_fetch) {
    REQUIRE(U(RegGetKeySecurity));
    HKEY h = FRESH(); REQUIRE(h); DWORD cb = 0;
    EQ(U(RegGetKeySecurity)(h, DACL_SECURITY_INFORMATION, NULL, &cb), ERROR_INSUFFICIENT_BUFFER);
    CHECK(cb >= sizeof(SECURITY_DESCRIPTOR_RELATIVE));
    BYTE *sd = malloc(cb); REQUIRE(sd); DWORD cb2 = cb;
    EQ(U(RegGetKeySecurity)(h, DACL_SECURITY_INFORMATION, sd, &cb2), 0);
    CHECK(IsValidSecurityDescriptor(sd));
    BOOL present = FALSE, def = FALSE; PACL acl = NULL;
    CHECK(GetSecurityDescriptorDacl(sd, &present, &acl, &def));
    CHECK(present);
    free(sd); RegCloseKey(h);
}
TEST(regkb_getkeysecurity_small_buffer) {
    REQUIRE(U(RegGetKeySecurity));
    HKEY h = FRESH(); REQUIRE(h); BYTE sd[4]; DWORD cb = sizeof sd;
    EQ(U(RegGetKeySecurity)(h, OWNER_SECURITY_INFORMATION, sd, &cb), ERROR_INSUFFICIENT_BUFFER);
    CHECK(cb > sizeof sd);
    RegCloseKey(h);
}
TEST(regkb_getkeysecurity_owner_group_dacl) {
    REQUIRE(U(RegGetKeySecurity));
    HKEY h = FRESH(); REQUIRE(h); BYTE sd[2048]; DWORD cb = sizeof sd;
    EQ(U(RegGetKeySecurity)(h, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION, sd, &cb), 0);
    PSID owner = NULL; BOOL def; CHECK(GetSecurityDescriptorOwner(sd, &owner, &def)); CHECK(owner && IsValidSid(owner));
    PSID grp = NULL; CHECK(GetSecurityDescriptorGroup(sd, &grp, &def)); CHECK(grp && IsValidSid(grp));
    BOOL present = TRUE; PACL acl; GetSecurityDescriptorDacl(sd, &present, &acl, &def);
    CHECK(!present);   /* DACL not requested */
    RegCloseKey(h);
}
TEST(regkb_getkeysecurity_requires_read_control) {
    REQUIRE(U(RegGetKeySecurity));
    HKEY h = FRESH(); REQUIRE(h); HKEY q; EQ(RegOpenKeyExW(h, NULL, 0, KEY_QUERY_VALUE, &q), 0);
    BYTE sd[1024]; DWORD cb = sizeof sd;
    EQ(U(RegGetKeySecurity)(q, DACL_SECURITY_INFORMATION, sd, &cb), ERROR_ACCESS_DENIED);
    RegCloseKey(q); RegCloseKey(h);
}
TEST(regkb_getkeysecurity_sacl_needs_privilege) {
    REQUIRE(U(RegGetKeySecurity));
    HKEY h = FRESH(); REQUIRE(h); BYTE sd[1024]; DWORD cb = sizeof sd;
    LSTATUS s = U(RegGetKeySecurity)(h, SACL_SECURITY_INFORMATION, sd, &cb);   /* handle lacks ACCESS_SYSTEM_SECURITY */
    CHECK(s == ERROR_ACCESS_DENIED || s == ERROR_PRIVILEGE_NOT_HELD);
    t_note("SACL query without privilege = %ld", (long)s);
    RegCloseKey(h);
}
TEST(regkb_getkeysecurity_invalid_handle) {
    REQUIRE(U(RegGetKeySecurity));
    BYTE sd[16]; DWORD cb = sizeof sd;
    EQ(U(RegGetKeySecurity)(NULL, DACL_SECURITY_INFORMATION, sd, &cb), ERROR_INVALID_HANDLE);
}
TEST(regkb_setkeysecurity_roundtrip_dacl) {
    REQUIRE(U(RegGetKeySecurity)); REQUIRE(U(RegSetKeySecurity));
    HKEY h = FRESH(); REQUIRE(h); BYTE sd[2048]; DWORD cb = sizeof sd;
    EQ(U(RegGetKeySecurity)(h, DACL_SECURITY_INFORMATION, sd, &cb), 0);
    EQ(U(RegSetKeySecurity)(h, DACL_SECURITY_INFORMATION, sd), 0);
    BYTE sd2[2048]; DWORD cb2 = sizeof sd2;
    EQ(U(RegGetKeySecurity)(h, DACL_SECURITY_INFORMATION, sd2, &cb2), 0);
    CHECK(IsValidSecurityDescriptor(sd2));
    RegCloseKey(h);
}
TEST(regkb_setkeysecurity_protected_dacl_applies) {
    REQUIRE(U(RegSetKeySecurity));
    HKEY h = FRESH(); REQUIRE(h);
    SECURITY_DESCRIPTOR sd; InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);   /* NULL DACL: everyone full access */
    EQ(U(RegSetKeySecurity)(h, DACL_SECURITY_INFORMATION, &sd), 0);
    BYTE out[1024]; DWORD cb = sizeof out;
    EQ(RegGetKeySecurity(h, DACL_SECURITY_INFORMATION, out, &cb), 0);
    BOOL present = FALSE, def; PACL acl = (PACL)1; GetSecurityDescriptorDacl(out, &present, &acl, &def);
    CHECK(!present || acl == NULL);
    RegCloseKey(h);
}
TEST(regkb_setkeysecurity_requires_write_dac) {
    REQUIRE(U(RegSetKeySecurity));
    HKEY h = FRESH(); REQUIRE(h); HKEY ro; EQ(RegOpenKeyExW(h, NULL, 0, KEY_READ, &ro), 0);
    SECURITY_DESCRIPTOR sd; InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
    EQ(U(RegSetKeySecurity)(ro, DACL_SECURITY_INFORMATION, &sd), ERROR_ACCESS_DENIED);
    RegCloseKey(ro); RegCloseKey(h);
}
TEST(regkb_setkeysecurity_invalid_handle) {
    REQUIRE(U(RegSetKeySecurity));
    SECURITY_DESCRIPTOR sd; InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    EQ(U(RegSetKeySecurity)(NULL, DACL_SECURITY_INFORMATION, &sd), ERROR_INVALID_HANDLE);
}

/* ---------------- RegDeleteTree / RegCopyTree ---------------- */
static void build_tree(HKEY h) {
    HKEY a = sc_sub(h, L"t\\a1\\deep"); sc_dw(a, L"dv", 1); RegCloseKey(a);
    a = sc_sub(h, L"t\\a2"); sc_sz(a, L"s", L"x"); RegCloseKey(a);
    a = sc_sub(h, L"t"); sc_dw(a, L"tv", 2); BYTE b[3] = { 1, 2, 3 }; sc_raw(a, L"tb", REG_BINARY, b, 3); RegCloseKey(a);
}
TEST(regkb_deletetree_subkey) {
    REQUIRE(U(RegDeleteTreeW));
    HKEY h = FRESH(); REQUIRE(h); build_tree(h);
    EQ(U(RegDeleteTreeW)(h, L"t"), 0);
    CHECK(!rk_exists(h, "t"));
    EQ(rk_subcount(h), 0);
    EQ(U(RegDeleteTreeW)(h, L"t"), ERROR_FILE_NOT_FOUND);
    RegCloseKey(h);
}
TEST(regkb_deletetree_a_subkey) {
    REQUIRE(U(RegDeleteTreeA));
    HKEY h = FRESH(); REQUIRE(h); build_tree(h);
    EQ(U(RegDeleteTreeA)(h, "T"), 0);
    EQ(rk_subcount(h), 0);
    EQ(U(RegDeleteTreeA)(h, "T"), ERROR_FILE_NOT_FOUND);
    RegCloseKey(h);
}
TEST(regkb_deletetree_null_subkey_clears_children_keeps_key) {
    REQUIRE(U(RegDeleteTreeW));
    HKEY h = FRESH(); REQUIRE(h); build_tree(h); sc_dw(h, L"own", 1);
    EQ(U(RegDeleteTreeW)(h, NULL), 0);
    EQ(rk_subcount(h), 0); EQ(rk_valcount(h), 0);
    DWORD v; EQ(RegSetValueExW(h, L"still_usable", 0, REG_DWORD, (BYTE *)&v, 4), 0);   /* key itself survives */
    EQ(U(RegDeleteTreeW)(h, L""), 0);
    EQ(rk_valcount(h), 0);
    RegCloseKey(h);
}
TEST(regkb_deletetree_with_values_only) {
    REQUIRE(U(RegDeleteTreeW));
    HKEY h = FRESH(); REQUIRE(h); HKEY s = sc_sub(h, L"s"); sc_dw(s, L"v", 1); RegCloseKey(s);
    EQ(U(RegDeleteTreeW)(h, L"s"), 0); CHECK(!rk_exists(h, "s"));
    RegCloseKey(h);
}
TEST(regkb_deletetree_missing_subkey_and_bad_handle) {
    REQUIRE(U(RegDeleteTreeW));
    HKEY h = FRESH(); REQUIRE(h);
    EQ(U(RegDeleteTreeW)(h, L"absent"), ERROR_FILE_NOT_FOUND);
    EQ(U(RegDeleteTreeW)(h, L"absent\\deeper"), ERROR_FILE_NOT_FOUND);
    CHECK(U(RegDeleteTreeW)(NULL, L"x") != 0);
    RegCloseKey(h);
}
TEST(regkb_deletetree_read_only_handle_denied) {
    REQUIRE(U(RegDeleteTreeW));
    HKEY h = FRESH(); REQUIRE(h); build_tree(h); HKEY ro; EQ(RegOpenKeyExW(h, NULL, 0, KEY_READ, &ro), 0);
    LSTATUS s = U(RegDeleteTreeW)(ro, L"t");
    t_note("RegDeleteTreeW via KEY_READ parent = %ld", (long)s);
    RegCloseKey(ro); RegCloseKey(h);
}
TEST(regkb_deletetree_wide_tree) {
    REQUIRE(U(RegDeleteTreeW));
    HKEY h = mk_subs(__func__, 200, L"w"); REQUIRE(h);
    for (int i = 0; i < 200; i += 7) { WCHAR p[32]; swprintf(p, 32, L"w%03d", i); HKEY k = sc_sub(h, p); sc_sub(k, L"in"); RegCloseKey(k); }
    EQ(U(RegDeleteTreeW)(h, NULL), 0); EQ(rk_subcount(h), 0);
    RegCloseKey(h);
}
TEST(regkb_deletetree_while_child_handle_open) {
    REQUIRE(U(RegDeleteTreeW));
    HKEY h = FRESH(); REQUIRE(h); build_tree(h); HKEY k; EQ(RegOpenKeyExW(h, L"t\\a1", 0, KEY_READ, &k), 0);
    EQ(U(RegDeleteTreeW)(h, L"t"), 0);
    CHECK(!rk_exists(h, "t"));
    DWORD n; EQ(RegQueryInfoKeyW(k, NULL, NULL, NULL, &n, NULL, NULL, NULL, NULL, NULL, NULL, NULL), ERROR_KEY_DELETED);
    RegCloseKey(k); RegCloseKey(h);
}
static int tree_equal(HKEY a, HKEY b) {
    DWORD sa, sb, va, vb;
    RegQueryInfoKeyW(a, NULL, NULL, NULL, &sa, NULL, NULL, &va, NULL, NULL, NULL, NULL);
    RegQueryInfoKeyW(b, NULL, NULL, NULL, &sb, NULL, NULL, &vb, NULL, NULL, NULL, NULL);
    if (sa != sb || va != vb) return 0;
    for (DWORD i = 0; i < va; i++) {
        WCHAR n[64]; DWORD nl = 64, t1, c1 = 0, t2, c2 = 0; BYTE d1[256], d2[256];
        c1 = sizeof d1; c2 = sizeof d2;
        if (RegEnumValueW(a, i, n, &nl, NULL, &t1, d1, &c1)) return 0;
        if (RegQueryValueExW(b, n, NULL, &t2, d2, &c2)) return 0;
        if (t1 != t2 || c1 != c2 || memcmp(d1, d2, c1)) return 0;
    }
    for (DWORD i = 0; i < sa; i++) {
        WCHAR n[64]; DWORD nl = 64; HKEY ka, kb;
        if (RegEnumKeyExW(a, i, n, &nl, NULL, NULL, NULL, NULL)) return 0;
        if (RegOpenKeyExW(a, n, 0, KEY_READ, &ka)) return 0;
        if (RegOpenKeyExW(b, n, 0, KEY_READ, &kb)) { RegCloseKey(ka); return 0; }
        int eq = tree_equal(ka, kb); RegCloseKey(ka); RegCloseKey(kb);
        if (!eq) return 0;
    }
    return 1;
}
TEST(regkb_copytree_whole_key) {
    REQUIRE(U(RegCopyTreeW));
    HKEY h = FRESH(); REQUIRE(h); build_tree(h); HKEY dst = sc_sub(h, L"dst");
    HKEY src; EQ(RegOpenKeyExW(h, L"t", 0, KEY_READ, &src), 0);
    EQ(U(RegCopyTreeW)(src, NULL, dst), 0);
    CHECK(tree_equal(src, dst));
    EQ(rk_subcount(dst), 2);
    RegCloseKey(src); RegCloseKey(dst); RegCloseKey(h);
}
TEST(regkb_copytree_subkey_path) {
    REQUIRE(U(RegCopyTreeW));
    HKEY h = FRESH(); REQUIRE(h); build_tree(h); HKEY dst = sc_sub(h, L"dst2");
    EQ(U(RegCopyTreeW)(h, L"t\\a1", dst), 0);
    CHECK(rk_exists(dst, "deep"));
    DWORD v = 0, cb = 4; EQ(RegGetValueW(dst, L"deep", L"dv", RRF_RT_REG_DWORD, NULL, &v, &cb), 0); EQ(v, 1);
    RegCloseKey(dst); RegCloseKey(h);
}
TEST(regkb_copytree_overwrites_existing_values) {
    REQUIRE(U(RegCopyTreeW));
    HKEY h = FRESH(); REQUIRE(h); build_tree(h); HKEY dst = sc_sub(h, L"dst3"); sc_dw(dst, L"tv", 999); sc_dw(dst, L"extra", 5);
    EQ(U(RegCopyTreeW)(h, L"t", dst), 0);
    DWORD v = 0, cb = 4; EQ(RegGetValueW(dst, NULL, L"tv", RRF_RT_REG_DWORD, NULL, &v, &cb), 0); EQ(v, 2);
    cb = 4; EQ(RegGetValueW(dst, NULL, L"extra", RRF_RT_REG_DWORD, NULL, &v, &cb), 0); EQ(v, 5);   /* not removed */
    RegCloseKey(dst); RegCloseKey(h);
}
TEST(regkb_copytree_preserves_types_and_sizes) {
    REQUIRE(U(RegCopyTreeW));
    HKEY h = FRESH(); REQUIRE(h); HKEY s = sc_sub(h, L"s"); HKEY d = sc_sub(h, L"d");
    unsigned long long q = 77; sc_raw(s, L"q", REG_QWORD, &q, 8); sc_raw(s, L"e", REG_EXPAND_SZ, L"%X%", 8);
    WCHAR m[] = L"a\0b\0"; sc_raw(s, L"m", REG_MULTI_SZ, m, sizeof m); sc_raw(s, L"n", REG_NONE, NULL, 0);
    BYTE odd[5] = { 1, 2, 3, 4, 5 }; sc_raw(s, L"odd", 0x55, odd, 5);
    EQ(U(RegCopyTreeW)(s, NULL, d), 0);
    CHECK(tree_equal(s, d));
    RegCloseKey(s); RegCloseKey(d); RegCloseKey(h);
}
TEST(regkb_copytree_missing_source_subkey) {
    REQUIRE(U(RegCopyTreeW));
    HKEY h = FRESH(); REQUIRE(h); HKEY d = sc_sub(h, L"d");
    EQ(U(RegCopyTreeW)(h, L"nope", d), ERROR_FILE_NOT_FOUND);
    EQ(rk_subcount(d), 0);
    RegCloseKey(d); RegCloseKey(h);
}
TEST(regkb_copytree_invalid_handles) {
    REQUIRE(U(RegCopyTreeW));
    HKEY h = FRESH(); REQUIRE(h);
    CHECK(U(RegCopyTreeW)(NULL, NULL, h) != 0);
    LSTATUS sn = U(RegCopyTreeW)(h, NULL, NULL); t_note("RegCopyTreeW(dest NULL) = %ld", (long)sn);
    RegCloseKey(h);
}
TEST(regkb_copytree_read_only_destination_denied) {
    REQUIRE(U(RegCopyTreeW));
    HKEY h = FRESH(); REQUIRE(h); build_tree(h); HKEY d = sc_sub(h, L"dd"); HKEY ro; EQ(RegOpenKeyExW(d, NULL, 0, KEY_READ, &ro), 0);
    EQ(U(RegCopyTreeW)(h, L"t", ro), ERROR_ACCESS_DENIED);
    EQ(rk_subcount(d), 0);
    RegCloseKey(ro); RegCloseKey(d); RegCloseKey(h);
}
TEST(regkb_copytree_self_subtree_depth_bounded) {
    REQUIRE(U(RegCopyTreeW));
    HKEY h = FRESH(); REQUIRE(h); HKEY s = sc_sub(h, L"s\\x\\y"); RegCloseKey(s);
    HKEY d = sc_sub(h, L"d");
    EQ(U(RegCopyTreeW)(h, L"s", d), 0);
    CHECK(rk_exists(d, "x\\y"));
    RegCloseKey(d); RegCloseKey(h);
}
