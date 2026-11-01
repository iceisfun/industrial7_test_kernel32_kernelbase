#include "rk.h"

/* ---------------- RegCreateKeyEx / RegOpenKeyEx / RegCloseKey ---------------- */

TEST(regkb_create_new_then_existing) {
    REQUIRE(U(RegCreateKeyExW)); REQUIRE(U(RegCloseKey));
    HKEY r = FRESH(); REQUIRE(r);
    HKEY h = NULL; DWORD disp = 99;
    EQ(U(RegCreateKeyExW)(r, L"newkey", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &h, &disp), 0);
    CHECK(h != NULL); EQ(disp, REG_CREATED_NEW_KEY);
    EQ(REG_CREATED_NEW_KEY, 1); EQ(REG_OPENED_EXISTING_KEY, 2);
    EQ(U(RegCloseKey)(h), 0);
    h = NULL; disp = 99;
    EQ(U(RegCreateKeyExW)(r, L"newkey", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &h, &disp), 0);
    EQ(disp, REG_OPENED_EXISTING_KEY);
    U(RegCloseKey)(h); RegCloseKey(r);
}
TEST(regkb_create_a_new_then_existing) {
    REQUIRE(U(RegCreateKeyExA));
    HKEY r = FRESH(); REQUIRE(r);
    HKEY h = NULL; DWORD disp = 99;
    EQ(U(RegCreateKeyExA)(r, "newkey", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &h, &disp), 0);
    EQ(disp, REG_CREATED_NEW_KEY); RegCloseKey(h);
    EQ(U(RegCreateKeyExA)(r, "NEWKEY", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &h, &disp), 0);
    EQ(disp, REG_OPENED_EXISTING_KEY); RegCloseKey(h); RegCloseKey(r);
}
TEST(regkb_create_null_disposition_ok) {
    REQUIRE(U(RegCreateKeyExW));
    HKEY r = FRESH(); REQUIRE(r); HKEY h = NULL;
    EQ(U(RegCreateKeyExW)(r, L"nodisp", 0, NULL, 0, KEY_READ, NULL, &h, NULL), 0);
    CHECK(h != NULL); RegCloseKey(h); RegCloseKey(r);
}
TEST(regkb_create_null_subkey_returns_same_key) {
    REQUIRE(U(RegCreateKeyExW));
    HKEY r = FRESH(); REQUIRE(r); HKEY h = NULL; DWORD disp = 0;
    LSTATUS s = U(RegCreateKeyExW)(r, NULL, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &h, &disp);
    /* p1: RegCreateKeyExInternalW returns ERROR_BADKEY when lpSubKey (or phkResult) is NULL; only L"" is accepted */
    if (s == 0 && h) RegCloseKey(h);
    EQ(s, ERROR_BADKEY);
    RegCloseKey(r);
}
TEST(regkb_create_empty_subkey_returns_same_key) {
    REQUIRE(U(RegCreateKeyExW));
    HKEY r = FRESH(); REQUIRE(r); HKEY h = NULL; DWORD disp = 0;
    EQ(U(RegCreateKeyExW)(r, L"", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &h, &disp), 0);
    EQ(disp, REG_OPENED_EXISTING_KEY); CHECK(h != r);
    RegCloseKey(h); RegCloseKey(r);
}
TEST(regkb_create_null_result_param) {
    REQUIRE(U(RegCreateKeyExW)); REQUIRE(U(RegCreateKeyExA));
    HKEY r = FRESH(); REQUIRE(r);
    LSTATUS s = U(RegCreateKeyExW)(r, L"nullphk", 0, NULL, 0, KEY_READ, NULL, NULL, NULL);
    CHECK(s != 0); t_note("create(NULL phk) W = %ld", (long)s);
    s = U(RegCreateKeyExA)(r, "nullphk", 0, NULL, 0, KEY_READ, NULL, NULL, NULL);
    CHECK(s != 0); t_note("create(NULL phk) A = %ld", (long)s);
    RegCloseKey(r);
}
TEST(regkb_create_invalid_parent_handle) {
    REQUIRE(U(RegCreateKeyExW));
    HKEY h = (HKEY)0x1234;
    HKEY bad[] = { NULL, INVALID_HANDLE_VALUE, (HKEY)(ULONG_PTR)0x7fff0000 };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        h = NULL;
        LSTATUS s = U(RegCreateKeyExW)(bad[i], L"x", 0, NULL, 0, KEY_READ, NULL, &h, NULL);
        CHECKF(s != 0, "bad[%zu]", i);
        CHECK(h == NULL);
        if (s != ERROR_INVALID_HANDLE) t_note("create on invalid handle %zu: %ld", i, (long)s);
    }
}
TEST(regkb_create_unicode_class_roundtrip) {
    REQUIRE(U(RegCreateKeyExW)); REQUIRE(U(RegQueryInfoKeyW));
    HKEY r = FRESH(); REQUIRE(r); HKEY h = NULL;
    WCHAR cls[] = L"MyClassName";
    EQ(U(RegCreateKeyExW)(r, L"withclass", 0, cls, 0, KEY_ALL_ACCESS, NULL, &h, NULL), 0);
    WCHAR out[64] = {0}; DWORD n = 64, maxcls = 0;
    EQ(U(RegQueryInfoKeyW)(h, out, &n, NULL, NULL, NULL, &maxcls, NULL, NULL, NULL, NULL, NULL), 0);
    EQ(n, 11); EXPECT_WSTR(out, L"MyClassName");
    RegCloseKey(h);
    /* class is only applied on creation: reopening with a different class keeps the original */
    DWORD disp = 0;
    WCHAR cls2[] = L"Other";
    EQ(U(RegCreateKeyExW)(r, L"withclass", 0, cls2, 0, KEY_ALL_ACCESS, NULL, &h, &disp), 0);
    EQ(disp, REG_OPENED_EXISTING_KEY);
    n = 64; out[0] = 0;
    EQ(RegQueryInfoKeyW(h, out, &n, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL), 0);
    EXPECT_WSTR(out, L"MyClassName");
    RegCloseKey(h); RegCloseKey(r);
}
TEST(regkb_create_a_class_roundtrip) {
    REQUIRE(U(RegCreateKeyExA)); REQUIRE(U(RegQueryInfoKeyA));
    HKEY r = FRESH(); REQUIRE(r); HKEY h = NULL;
    char cls[] = "ClassA";
    EQ(U(RegCreateKeyExA)(r, "withclassA", 0, cls, 0, KEY_ALL_ACCESS, NULL, &h, NULL), 0);
    char out[64] = {0}; DWORD n = 64;
    EQ(U(RegQueryInfoKeyA)(h, out, &n, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL), 0);
    EQ(n, 6); EXPECT_STR(out, "ClassA");
    n = 6; out[0] = 'Z';
    LSTATUS s = U(RegQueryInfoKeyA)(h, out, &n, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    EQ(s, ERROR_MORE_DATA);
    RegCloseKey(h); RegCloseKey(r);
}
TEST(regkb_open_missing_key) {
    REQUIRE(U(RegOpenKeyExW)); REQUIRE(U(RegOpenKeyExA));
    HKEY r = FRESH(); REQUIRE(r); HKEY h = (HKEY)1;
    EQ(U(RegOpenKeyExW)(r, L"missing", 0, KEY_READ, &h), ERROR_FILE_NOT_FOUND);
    CHECK(h == NULL || h == (HKEY)1);
    EQ(U(RegOpenKeyExA)(r, "missing", 0, KEY_READ, &h), ERROR_FILE_NOT_FOUND);
    EQ(U(RegOpenKeyExW)(r, L"missing\\deeper", 0, KEY_READ, &h), ERROR_FILE_NOT_FOUND);
    RegCloseKey(r);
}
TEST(regkb_open_null_result_param) {
    REQUIRE(U(RegOpenKeyExW)); REQUIRE(U(RegOpenKeyExA));
    if (t_is_wine()) { t_skip("wine faults on NULL phkResult"); return; }
    HKEY r = FRESH(); REQUIRE(r); sc_subc(r, L"s");
    EQ(U(RegOpenKeyExW)(r, L"s", 0, KEY_READ, NULL), ERROR_INVALID_PARAMETER);
    EQ(U(RegOpenKeyExA)(r, "s", 0, KEY_READ, NULL), ERROR_INVALID_PARAMETER);
    RegCloseKey(r);
}
TEST(regkb_open_invalid_parent_handle) {
    REQUIRE(U(RegOpenKeyExW));
    HKEY bad[] = { NULL, INVALID_HANDLE_VALUE, (HKEY)(ULONG_PTR)0x7fff0000 };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        HKEY h = NULL;
        LSTATUS s = U(RegOpenKeyExW)(bad[i], L"x", 0, KEY_READ, &h);
        CHECKF(s != 0, "bad[%zu] s=%ld", i, (long)s);
        if (s != ERROR_INVALID_HANDLE) t_note("open on invalid handle %zu: %ld", i, (long)s);
    }
}
TEST(regkb_open_null_or_empty_subkey_duplicates) {
    REQUIRE(U(RegOpenKeyExW)); REQUIRE(U(RegCloseKey));
    HKEY r = FRESH(); REQUIRE(r); HKEY h1 = NULL, h2 = NULL;
    EQ(U(RegOpenKeyExW)(r, NULL, 0, KEY_READ, &h1), 0);
    EQ(U(RegOpenKeyExW)(r, L"", 0, KEY_READ, &h2), 0);
    CHECK(h1 && h2 && h1 != r && h2 != r && h1 != h2);
    EQ(U(RegCloseKey)(r), 0);
    /* the duplicates outlive the original handle */
    DWORD n = 0;
    EQ(RegQueryInfoKeyW(h1, NULL, NULL, NULL, &n, NULL, NULL, NULL, NULL, NULL, NULL, NULL), 0);
    EQ(RegQueryInfoKeyW(h2, NULL, NULL, NULL, &n, NULL, NULL, NULL, NULL, NULL, NULL, NULL), 0);
    RegCloseKey(h1); RegCloseKey(h2);
}
TEST(regkb_open_predefined_null_subkey) {
    REQUIRE(U(RegOpenKeyExW)); REQUIRE(U(RegCloseKey));
    HKEY pre[] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE, HKEY_CLASSES_ROOT, HKEY_USERS, HKEY_CURRENT_CONFIG };
    for (size_t i = 0; i < 5; i++) {
        HKEY h = NULL;
        EQ(U(RegOpenKeyExW)(pre[i], NULL, 0, KEY_READ, &h), 0);
        /* documented: NULL/empty subkey on a predefined key returns the same predefined handle */
        CHECK(h == pre[i]);
        if (h && h != pre[i]) U(RegCloseKey)(h);
    }
}
TEST(regkb_open_link_option_on_plain_key) {
    REQUIRE(U(RegOpenKeyExW));
    HKEY r = FRESH(); REQUIRE(r); sc_subc(r, L"plain"); HKEY h = NULL;
    EQ(U(RegOpenKeyExW)(r, L"plain", REG_OPTION_OPEN_LINK, KEY_READ, &h), 0);
    CHECK(h != NULL); RegCloseKey(h); RegCloseKey(r);
}
TEST(regkb_open_unknown_option_bits) {
    REQUIRE(U(RegOpenKeyExW));
    HKEY r = FRESH(); REQUIRE(r); sc_subc(r, L"plain"); HKEY h = NULL;
    LSTATUS s = U(RegOpenKeyExW)(r, L"plain", 0x40000000, KEY_READ, &h);
    t_note("RegOpenKeyExW(unknown option bits) = %ld", (long)s);
    if (s == 0) RegCloseKey(h);
    RegCloseKey(r);
}
TEST(regkb_open_case_insensitive) {
    REQUIRE(U(RegOpenKeyExW)); REQUIRE(U(RegCreateKeyExW));
    HKEY r = FRESH(); REQUIRE(r); HKEY h = NULL;
    EQ(U(RegCreateKeyExW)(r, L"MiXeDcAsE", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &h, NULL), 0); RegCloseKey(h);
    const WCHAR *v[] = { L"mixedcase", L"MIXEDCASE", L"MiXeDcAsE", L"mIxEdCaSe" };
    for (int i = 0; i < 4; i++) { EQ(U(RegOpenKeyExW)(r, v[i], 0, KEY_READ, &h), 0); RegCloseKey(h); }
    DWORD disp = 0;
    EQ(U(RegCreateKeyExW)(r, L"MIXEDCASE", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &h, &disp), 0);
    EQ(disp, REG_OPENED_EXISTING_KEY); RegCloseKey(h);
    EQ(rk_subcount(r), 1);
    WCHAR nm[64]; DWORD n = 64;
    EQ(RegEnumKeyExW(r, 0, nm, &n, NULL, NULL, NULL, NULL), 0);
    EXPECT_WSTR(nm, L"MiXeDcAsE");   /* name keeps the case used at creation */
    RegCloseKey(r);
}
TEST(regkb_open_case_insensitive_nonascii) {
    REQUIRE(U(RegOpenKeyExW));
    HKEY r = FRESH(); REQUIRE(r); sc_subc(r, L"\x00e9\x00e8" L"x"); HKEY h = NULL;
    EQ(U(RegOpenKeyExW)(r, L"\x00c9\x00c8" L"X", 0, KEY_READ, &h), 0);
    if (h) RegCloseKey(h);
    RegCloseKey(r);
}
TEST(regkb_create_nested_with_backslashes) {
    REQUIRE(U(RegCreateKeyExW)); REQUIRE(U(RegOpenKeyExW));
    HKEY r = FRESH(); REQUIRE(r); HKEY h = NULL; DWORD disp = 0;
    EQ(U(RegCreateKeyExW)(r, L"a\\b\\c", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &h, &disp), 0);
    EQ(disp, REG_CREATED_NEW_KEY); RegCloseKey(h);
    EQ(U(RegOpenKeyExW)(r, L"a\\b", 0, KEY_READ, &h), 0);
    EQ(rk_subcount(h), 1); RegCloseKey(h);
    EQ(U(RegOpenKeyExW)(r, L"a", 0, KEY_READ, &h), 0);
    EQ(rk_subcount(h), 1); RegCloseKey(h);
    EQ(rk_subcount(r), 1);
    /* creating a path whose prefix exists reports the disposition of the final key */
    EQ(U(RegCreateKeyExW)(r, L"a\\b\\d", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &h, &disp), 0);
    EQ(disp, REG_CREATED_NEW_KEY); RegCloseKey(h);
    EQ(U(RegCreateKeyExW)(r, L"a\\b\\c", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &h, &disp), 0);
    EQ(disp, REG_OPENED_EXISTING_KEY); RegCloseKey(h);
    RegCloseKey(r);
}
TEST(regkb_create_a_nested_with_backslashes) {
    REQUIRE(U(RegCreateKeyExA)); REQUIRE(U(RegOpenKeyExA));
    HKEY r = FRESH(); REQUIRE(r); HKEY h = NULL;
    EQ(U(RegCreateKeyExA)(r, "p\\q\\r", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &h, NULL), 0); RegCloseKey(h);
    EQ(U(RegOpenKeyExA)(r, "P\\Q", 0, KEY_READ, &h), 0); RegCloseKey(h);
    RegCloseKey(r);
}
TEST(regkb_slash_is_ordinary_key_char) {
    REQUIRE(U(RegCreateKeyExW)); REQUIRE(U(RegOpenKeyExW));
    HKEY r = FRESH(); REQUIRE(r); HKEY h = NULL;
    EQ(U(RegCreateKeyExW)(r, L"one/two", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &h, NULL), 0); RegCloseKey(h);
    EQ(rk_subcount(r), 1);
    EQ(U(RegOpenKeyExW)(r, L"one", 0, KEY_READ, &h), ERROR_FILE_NOT_FOUND);
    EQ(U(RegOpenKeyExW)(r, L"one/two", 0, KEY_READ, &h), 0); RegCloseKey(h);
    RegCloseKey(r);
}
TEST(regkb_odd_separators_observed) {
    REQUIRE(U(RegCreateKeyExW));
    HKEY r = FRESH(); REQUIRE(r);
    const WCHAR *names[] = { L"\\lead", L"trail\\", L"dbl\\\\mid", L"\\\\" , L".", L".." };
    for (int i = 0; i < 6; i++) {
        HKEY h = NULL; DWORD d = 0;
        LSTATUS s = U(RegCreateKeyExW)(r, names[i], 0, NULL, 0, KEY_ALL_ACCESS, NULL, &h, &d);
        t_note("create odd name #%d -> %ld (disp %lu)", i, (long)s, d);
        if (s == 0) RegCloseKey(h);
    }
    RegCloseKey(r);
}
TEST(regkb_key_name_length_255_ok) {
    REQUIRE(U(RegCreateKeyExW)); REQUIRE(U(RegOpenKeyExW));
    HKEY r = FRESH(); REQUIRE(r); HKEY h = NULL;
    WCHAR nm[300]; rk_fill_w(nm, 255, L'k');
    EQ(U(RegCreateKeyExW)(r, nm, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &h, NULL), 0); RegCloseKey(h);
    EQ(U(RegOpenKeyExW)(r, nm, 0, KEY_READ, &h), 0);
    WCHAR out[300]; DWORD n = 300; RegCloseKey(h);
    EQ(RegEnumKeyExW(r, 0, out, &n, NULL, NULL, NULL, NULL), 0);
    EQ(n, 255);
    RegCloseKey(r);
}
TEST(regkb_key_name_length_256_observed) {
    REQUIRE(U(RegCreateKeyExW));
    HKEY r = FRESH(); REQUIRE(r); HKEY h = NULL;
    WCHAR nm[300]; rk_fill_w(nm, 256, L'k');
    LSTATUS s = U(RegCreateKeyExW)(r, nm, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &h, NULL);
    t_note("RegCreateKeyExW(256-char component) = %ld", (long)s);
    if (s == 0) RegCloseKey(h);
    RegCloseKey(r);
}
TEST(regkb_deep_path_of_255_char_components) {
    REQUIRE(U(RegCreateKeyExW)); REQUIRE(U(RegOpenKeyExW)); REQUIRE(U(RegDeleteTreeW));
    HKEY r = FRESH(); REQUIRE(r);
    static WCHAR path[255 * 9 + 16]; path[0] = 0;
    for (int i = 0; i < 8; i++) {
        WCHAR comp[256]; rk_fill_w(comp, 255, (WCHAR)(L'a' + i));
        if (i) wcscat(path, L"\\");
        wcscat(path, comp);
    }
    HKEY h = NULL; DWORD disp = 0;
    EQ(U(RegCreateKeyExW)(r, path, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &h, &disp), 0);
    EQ(disp, REG_CREATED_NEW_KEY);
    if (h) RegCloseKey(h);
    EQ(U(RegOpenKeyExW)(r, path, 0, KEY_READ, &h), 0);
    if (h) RegCloseKey(h);
    EQ(U(RegDeleteTreeW)(r, path), 0);
    CHECK(!rk_existsw(r, path));
    RegCloseKey(r);
}
TEST(regkb_unicode_names_surrogates) {
    REQUIRE(U(RegCreateKeyExW)); REQUIRE(U(RegOpenKeyExW));
    HKEY r = FRESH(); REQUIRE(r); HKEY h = NULL;
    WCHAR nm[] = { 0xD834, 0xDD1E, 0x4E2D, 0x6587, L'-', 0x00e4, 0 };   /* U+1D11E, CJK, a-umlaut */
    EQ(U(RegCreateKeyExW)(r, nm, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &h, NULL), 0); RegCloseKey(h);
    EQ(U(RegOpenKeyExW)(r, nm, 0, KEY_READ, &h), 0); RegCloseKey(h);
    WCHAR out[32]; DWORD n = 32;
    EQ(RegEnumKeyExW(r, 0, out, &n, NULL, NULL, NULL, NULL), 0);
    EQ(n, 6); CHECK(!memcmp(out, nm, sizeof nm));
    RegCloseKey(r);
}
TEST(regkb_acp_key_name_a_vs_w) {
    REQUIRE(U(RegCreateKeyExA)); REQUIRE(U(RegOpenKeyExW));
    HKEY r = FRESH(); REQUIRE(r); HKEY h = NULL;
    char a[] = "caf\xe9_k"; WCHAR w[16];
    int wl = MultiByteToWideChar(CP_ACP, 0, a, -1, w, 16);
    CHECK(wl > 0);
    EQ(U(RegCreateKeyExA)(r, a, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &h, NULL), 0); RegCloseKey(h);
    EQ(U(RegOpenKeyExW)(r, w, 0, KEY_READ, &h), 0); RegCloseKey(h);
    WCHAR out[32]; DWORD n = 32;
    EQ(RegEnumKeyExW(r, 0, out, &n, NULL, NULL, NULL, NULL), 0);
    EXPECT_WSTR(out, w);
    RegCloseKey(r);
}
TEST(regkb_open_a_vs_w_parity) {
    REQUIRE(U(RegOpenKeyExA)); REQUIRE(U(RegOpenKeyExW));
    HKEY r = FRESH(); REQUIRE(r); sc_subc(r, L"parity"); HKEY ha = NULL, hw = NULL;
    EQ(U(RegOpenKeyExA)(r, "parity", 0, KEY_READ, &ha), 0);
    EQ(U(RegOpenKeyExW)(r, L"parity", 0, KEY_READ, &hw), 0);
    CHECK(ha && hw && ha != hw);
    RegCloseKey(ha); RegCloseKey(hw); RegCloseKey(r);
}

/* ---------------- close ---------------- */
TEST(regkb_close_null_is_invalid_handle) {
    REQUIRE(U(RegCloseKey));
    EQ(U(RegCloseKey)(NULL), ERROR_INVALID_HANDLE);
}
TEST(regkb_close_twice_second_fails) {
    REQUIRE(U(RegCloseKey));
    HKEY r = FRESH(); REQUIRE(r);
    HKEY h = sc_sub(r, L"x"); REQUIRE(h);
    EQ(U(RegCloseKey)(h), 0);
    LSTATUS s = U(RegCloseKey)(h);
    EQ(s, ERROR_INVALID_HANDLE);
    RegCloseKey(r);
}
TEST(regkb_close_predefined_succeeds_and_stays_usable) {
    REQUIRE(U(RegCloseKey)); REQUIRE(U(RegOpenKeyExW));
    HKEY pre[] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE, HKEY_CLASSES_ROOT, HKEY_USERS, HKEY_CURRENT_CONFIG };
    for (size_t i = 0; i < 5; i++) {
        EQ(U(RegCloseKey)(pre[i]), 0);
        EQ(U(RegCloseKey)(pre[i]), 0);
    }
    HKEY h = NULL;
    EQ(U(RegOpenKeyExW)(HKEY_CURRENT_USER, L"Software", 0, KEY_READ, &h), 0);
    if (h) RegCloseKey(h);
    EQ(U(RegOpenKeyExW)(HKEY_LOCAL_MACHINE, L"SOFTWARE", 0, KEY_READ, &h), 0);
    if (h) RegCloseKey(h);
}
TEST(regkb_close_garbage_handle_fails) {
    REQUIRE(U(RegCloseKey));
    LSTATUS s = U(RegCloseKey)((HKEY)(ULONG_PTR)0x7fff0000);
    CHECK(s != 0);
    t_note("RegCloseKey(garbage) = %ld", (long)s);
}
TEST(regkb_closed_handle_use_fails) {
    REQUIRE(U(RegQueryValueExW)); REQUIRE(U(RegSetValueExW)); REQUIRE(U(RegOpenKeyExW));
    HKEY r = FRESH(); REQUIRE(r);
    HKEY h = sc_sub(r, L"gone"); RegCloseKey(h);
    DWORD v = 1, cb = 4, t;
    EQ(U(RegQueryValueExW)(h, L"v", NULL, &t, (BYTE *)&v, &cb), ERROR_INVALID_HANDLE);
    EQ(U(RegSetValueExW)(h, L"v", 0, REG_DWORD, (BYTE *)&v, 4), ERROR_INVALID_HANDLE);
    HKEY k = NULL;
    EQ(U(RegOpenKeyExW)(h, L"s", 0, KEY_READ, &k), ERROR_INVALID_HANDLE);
    RegCloseKey(r);
}

/* ---------------- delete ---------------- */
TEST(regkb_deletekey_basic_w) {
    REQUIRE(U(RegDeleteKeyW) || UA(RegDeleteKeyW));
    HKEY r = FRESH(); REQUIRE(r); sc_subc(r, L"del");
    CHECK(rk_exists(r, "del"));
    EQ(UA(RegDeleteKeyW)(r, L"del"), 0);
    CHECK(!rk_exists(r, "del"));
    EQ(UA(RegDeleteKeyW)(r, L"del"), ERROR_FILE_NOT_FOUND);
    RegCloseKey(r);
}
TEST(regkb_deletekey_basic_a) {
    REQUIRE(UA(RegDeleteKeyA));
    HKEY r = FRESH(); REQUIRE(r); sc_subc(r, L"del");
    EQ(UA(RegDeleteKeyA)(r, "DEL"), 0);
    CHECK(!rk_exists(r, "del"));
    EQ(UA(RegDeleteKeyA)(r, "del"), ERROR_FILE_NOT_FOUND);
    RegCloseKey(r);
}
TEST(regkb_deletekey_with_subkeys_denied) {
    REQUIRE(UA(RegDeleteKeyW)); REQUIRE(UA(RegDeleteKeyA)); REQUIRE(U(RegDeleteKeyExW)); REQUIRE(U(RegDeleteKeyExA));
    HKEY r = FRESH(); REQUIRE(r);
    HKEY h = sc_sub(r, L"par\\child\\leaf"); RegCloseKey(h);
    EQ(UA(RegDeleteKeyW)(r, L"par"), ERROR_ACCESS_DENIED);
    EQ(UA(RegDeleteKeyA)(r, "par"), ERROR_ACCESS_DENIED);
    EQ(U(RegDeleteKeyExW)(r, L"par", 0, 0), ERROR_ACCESS_DENIED);
    EQ(U(RegDeleteKeyExA)(r, "par", 0, 0), ERROR_ACCESS_DENIED);
    EQ(U(RegDeleteKeyExW)(r, L"par\\child", 0, 0), ERROR_ACCESS_DENIED);
    CHECK(rk_exists(r, "par\\child\\leaf"));
    EQ(U(RegDeleteKeyExW)(r, L"par\\child\\leaf", 0, 0), 0);
    EQ(U(RegDeleteKeyExW)(r, L"par\\child", 0, 0), 0);
    EQ(U(RegDeleteKeyExW)(r, L"par", 0, 0), 0);
    RegCloseKey(r);
}
TEST(regkb_deletekey_with_values_ok) {
    REQUIRE(U(RegDeleteKeyExW));
    HKEY r = FRESH(); REQUIRE(r);
    HKEY h = sc_sub(r, L"vals"); sc_dw(h, L"a", 1); sc_sz(h, L"b", L"x"); RegCloseKey(h);
    EQ(U(RegDeleteKeyExW)(r, L"vals", 0, 0), 0);
    CHECK(!rk_exists(r, "vals"));
    RegCloseKey(r);
}
TEST(regkb_deletekeyex_wow_flags) {
    REQUIRE(U(RegDeleteKeyExW)); REQUIRE(U(RegDeleteKeyExA));
    HKEY r = FRESH(); REQUIRE(r);
    sc_subc(r, L"k64"); sc_subc(r, L"k32"); sc_subc(r, L"kA64"); sc_subc(r, L"kA32");
    EQ(U(RegDeleteKeyExW)(r, L"k64", KEY_WOW64_64KEY, 0), 0);
    EQ(U(RegDeleteKeyExW)(r, L"k32", KEY_WOW64_32KEY, 0), 0);
    EQ(U(RegDeleteKeyExA)(r, "kA64", KEY_WOW64_64KEY, 0), 0);
    EQ(U(RegDeleteKeyExA)(r, "kA32", KEY_WOW64_32KEY, 0), 0);
    EQ(rk_subcount(r), 0);
    RegCloseKey(r);
}
TEST(regkb_deletekeyex_missing) {
    REQUIRE(U(RegDeleteKeyExW));
    HKEY r = FRESH(); REQUIRE(r);
    EQ(U(RegDeleteKeyExW)(r, L"nope", 0, 0), ERROR_FILE_NOT_FOUND);
    EQ(U(RegDeleteKeyExW)(r, L"nope\\deeper", 0, 0), ERROR_FILE_NOT_FOUND);
    RegCloseKey(r);
}
TEST(regkb_deletekeyex_reserved_nonzero) {
    REQUIRE(U(RegDeleteKeyExW));
    HKEY r = FRESH(); REQUIRE(r); sc_subc(r, L"keep");
    LSTATUS s = U(RegDeleteKeyExW)(r, L"keep", 0, 1);
    t_note("RegDeleteKeyExW(Reserved=1) = %ld", (long)s);
    RegCloseKey(r);
}
TEST(regkb_deletekey_invalid_parent) {
    REQUIRE(UA(RegDeleteKeyW));
    LSTATUS s = UA(RegDeleteKeyW)(NULL, L"zz");
    CHECK(s != 0);
    t_note("RegDeleteKeyW(NULL,...) = %ld", (long)s);
}
TEST(regkb_deletekey_null_subkey_observed) {
    REQUIRE(U(RegDeleteKeyExW));
    HKEY r = FRESH(); REQUIRE(r);
    HKEY h = sc_sub(r, L"selfdel"); REQUIRE(h);
    LSTATUS s = U(RegDeleteKeyExW)(h, NULL, 0, 0);
    t_note("RegDeleteKeyExW(h, NULL) = %ld, key still listed: %d", (long)s, rk_exists(r, "selfdel"));
    RegCloseKey(h); RegCloseKey(r);
}
TEST(regkb_delete_key_while_handle_open) {
    REQUIRE(U(RegDeleteKeyExW)); REQUIRE(U(RegSetValueExW)); REQUIRE(U(RegCreateKeyExW));
    REQUIRE(U(RegEnumKeyExW)); REQUIRE(U(RegQueryInfoKeyW)); REQUIRE(U(RegQueryValueExW));
    HKEY r = FRESH(); REQUIRE(r);
    HKEY h = sc_sub(r, L"doomed"); REQUIRE(h);
    sc_dw(h, L"a", 5);
    EQ(U(RegDeleteKeyExW)(r, L"doomed", 0, 0), 0);
    CHECK(!rk_exists(r, "doomed"));
    DWORD v = 2;
    EQ(U(RegSetValueExW)(h, L"b", 0, REG_DWORD, (BYTE *)&v, 4), ERROR_KEY_DELETED);
    HKEY k = NULL;
    EQ(U(RegCreateKeyExW)(h, L"child", 0, NULL, 0, KEY_READ, NULL, &k, NULL), ERROR_KEY_DELETED);
    WCHAR nm[16]; DWORD n = 16;
    EQ(U(RegEnumKeyExW)(h, 0, nm, &n, NULL, NULL, NULL, NULL), ERROR_KEY_DELETED);
    DWORD cnt = 0;
    EQ(U(RegQueryInfoKeyW)(h, NULL, NULL, NULL, &cnt, NULL, NULL, NULL, NULL, NULL, NULL, NULL), ERROR_KEY_DELETED);
    DWORD cb = 4, t;
    EQ(U(RegQueryValueExW)(h, L"a", NULL, &t, (BYTE *)&v, &cb), ERROR_KEY_DELETED);
    EQ(RegCloseKey(h), 0);
    /* the name is free again: a new key with the same name is a fresh, empty key */
    DWORD disp = 0;
    EQ(U(RegCreateKeyExW)(r, L"doomed", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, &disp), 0);
    EQ(disp, REG_CREATED_NEW_KEY);
    EQ(rk_valcount(k), 0);
    RegCloseKey(k); RegCloseKey(r);
}
TEST(regkb_delete_needs_delete_access) {
    REQUIRE(U(RegDeleteKeyExW)); REQUIRE(U(RegOpenKeyExW));
    HKEY r = FRESH(); REQUIRE(r); sc_subc(r, L"victim");
    HKEY ro = NULL;
    EQ(U(RegOpenKeyExW)(r, NULL, 0, KEY_READ, &ro), 0);
    LSTATUS s = U(RegDeleteKeyExW)(ro, L"victim", 0, 0);   /* DELETE is checked on the child, not the parent handle */
    t_note("RegDeleteKeyExW via KEY_READ parent = %ld", (long)s);
    RegCloseKey(ro); RegCloseKey(r);
}

/* ---------------- access rights ---------------- */
TEST(regkb_read_handle_cannot_write) {
    REQUIRE(U(RegSetValueExW)); REQUIRE(U(RegCreateKeyExW)); REQUIRE(U(RegDeleteValueW)); REQUIRE(U(RegSetKeyValueW));
    HKEY r = FRESH(); REQUIRE(r); sc_dw(r, L"exists", 1);
    HKEY ro = NULL;
    EQ(RegOpenKeyExW(r, NULL, 0, KEY_READ, &ro), 0);
    DWORD v = 7;
    EQ(U(RegSetValueExW)(ro, L"x", 0, REG_DWORD, (BYTE *)&v, 4), ERROR_ACCESS_DENIED);
    HKEY k = NULL;
    /* p1: subkey creation is access-checked (CmpCheckCreateAccess) against the parent's security descriptor and
     * the caller's token, not the handle's granted rights, so this succeeds for the owner of the scratch key. */
    EQ(U(RegCreateKeyExW)(ro, L"sub", 0, NULL, 0, KEY_READ, NULL, &k, NULL), 0);
    if (k) { RegCloseKey(k); RegDeleteKeyW(r, L"sub"); }
    EQ(U(RegDeleteValueW)(ro, L"exists"), ERROR_ACCESS_DENIED);
    EQ(U(RegSetKeyValueW)(ro, NULL, L"y", REG_DWORD, &v, 4), ERROR_ACCESS_DENIED);
    CHECK(!rk_exists(r, "sub"));              /* removed again above */
    DWORD cb = 4, t; EQ(RegQueryValueExW(r, L"exists", NULL, &t, (BYTE *)&v, &cb), 0);
    RegCloseKey(ro); RegCloseKey(r);
}
TEST(regkb_write_only_handle_cannot_read) {
    REQUIRE(U(RegQueryValueExW)); REQUIRE(U(RegEnumKeyExW)); REQUIRE(U(RegEnumValueW)); REQUIRE(U(RegQueryInfoKeyW));
    HKEY r = FRESH(); REQUIRE(r); sc_dw(r, L"exists", 1); sc_subc(r, L"s");
    HKEY w = NULL;
    EQ(RegOpenKeyExW(r, NULL, 0, KEY_SET_VALUE, &w), 0);
    DWORD v, cb = 4, t;
    EQ(U(RegQueryValueExW)(w, L"exists", NULL, &t, (BYTE *)&v, &cb), ERROR_ACCESS_DENIED);
    WCHAR nm[16]; DWORD n = 16;
    EQ(U(RegEnumKeyExW)(w, 0, nm, &n, NULL, NULL, NULL, NULL), ERROR_ACCESS_DENIED);
    n = 16;
    EQ(U(RegEnumValueW)(w, 0, nm, &n, NULL, NULL, NULL, NULL), ERROR_ACCESS_DENIED);
    DWORD c;
    EQ(U(RegQueryInfoKeyW)(w, NULL, NULL, NULL, &c, NULL, NULL, NULL, NULL, NULL, NULL, NULL), ERROR_ACCESS_DENIED);
    RegCloseKey(w); RegCloseKey(r);
}
TEST(regkb_enum_subkeys_right_vs_query_value_right) {
    REQUIRE(U(RegEnumKeyExW)); REQUIRE(U(RegQueryValueExW));
    HKEY r = FRESH(); REQUIRE(r); sc_dw(r, L"exists", 1); sc_subc(r, L"s");
    HKEY q = NULL, e = NULL;
    EQ(RegOpenKeyExW(r, NULL, 0, KEY_QUERY_VALUE, &q), 0);
    EQ(RegOpenKeyExW(r, NULL, 0, KEY_ENUMERATE_SUB_KEYS, &e), 0);
    WCHAR nm[16]; DWORD n = 16, v, cb = 4, t;
    EQ(U(RegEnumKeyExW)(q, 0, nm, &n, NULL, NULL, NULL, NULL), ERROR_ACCESS_DENIED);
    n = 16;
    EQ(U(RegEnumKeyExW)(e, 0, nm, &n, NULL, NULL, NULL, NULL), 0);
    EQ(U(RegQueryValueExW)(q, L"exists", NULL, &t, (BYTE *)&v, &cb), 0);
    EQ(U(RegQueryValueExW)(e, L"exists", NULL, &t, (BYTE *)&v, &cb), ERROR_ACCESS_DENIED);
    RegCloseKey(q); RegCloseKey(e); RegCloseKey(r);
}
TEST(regkb_open_zero_access_observed) {
    REQUIRE(U(RegOpenKeyExW)); REQUIRE(U(RegQueryValueExW));
    HKEY r = FRESH(); REQUIRE(r); sc_dw(r, L"v", 1); HKEY h = NULL;
    LSTATUS s = U(RegOpenKeyExW)(r, NULL, 0, 0, &h);
    t_note("RegOpenKeyExW(samDesired=0) = %ld", (long)s);
    if (s == 0) {
        DWORD v, cb = 4, t;
        EQ(U(RegQueryValueExW)(h, L"v", NULL, &t, (BYTE *)&v, &cb), ERROR_ACCESS_DENIED);
        RegCloseKey(h);
    }
    RegCloseKey(r);
}
TEST(regkb_open_maximum_allowed) {
    REQUIRE(U(RegOpenKeyExW)); REQUIRE(U(RegSetValueExW));
    HKEY r = FRESH(); REQUIRE(r); HKEY h = NULL;
    EQ(U(RegOpenKeyExW)(r, NULL, 0, MAXIMUM_ALLOWED, &h), 0);
    DWORD v = 3;
    EQ(U(RegSetValueExW)(h, L"w", 0, REG_DWORD, (BYTE *)&v, 4), 0);
    RegCloseKey(h); RegCloseKey(r);
}
TEST(regkb_open_hklm_software_read_ok) {
    REQUIRE(U(RegOpenKeyExW));
    HKEY h = NULL;
    EQ(U(RegOpenKeyExW)(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &h), 0);
    if (h) RegCloseKey(h);
    EQ(U(RegOpenKeyExW)(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_QUERY_VALUE | KEY_WOW64_64KEY, &h), 0);
    if (h) RegCloseKey(h);
    EQ(U(RegOpenKeyExW)(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_QUERY_VALUE | KEY_WOW64_32KEY, &h), 0);
    if (h) RegCloseKey(h);
}
TEST(regkb_open_hklm_missing_key) {
    REQUIRE(U(RegOpenKeyExW));
    HKEY h = NULL;
    EQ(U(RegOpenKeyExW)(HKEY_LOCAL_MACHINE, L"SOFTWARE\\k32t_does_not_exist_ZZ", 0, KEY_READ, &h), ERROR_FILE_NOT_FOUND);
}
TEST(regkb_hklm_write_access_observed) {
    REQUIRE(U(RegOpenKeyExW));
    HKEY h = NULL;
    LSTATUS s = U(RegOpenKeyExW)(HKEY_LOCAL_MACHINE, L"SOFTWARE", 0, KEY_SET_VALUE, &h);
    t_note("open HKLM\\SOFTWARE for KEY_SET_VALUE = %ld (5 expected when not elevated)", (long)s);
    if (s == 0) RegCloseKey(h);
}

/* ---------------- options ---------------- */
TEST(regkb_volatile_key_rules) {
    REQUIRE(U(RegCreateKeyExW));
    HKEY r = FRESH(); REQUIRE(r); HKEY v = NULL, c = NULL; DWORD disp;
    EQ(U(RegCreateKeyExW)(r, L"vol", 0, NULL, REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &v, &disp), 0);
    EQ(disp, REG_CREATED_NEW_KEY);
    EQ(REG_OPTION_NON_VOLATILE, 0); EQ(REG_OPTION_VOLATILE, 1);
    /* a non-volatile child of a volatile parent is refused */
    EQ(U(RegCreateKeyExW)(v, L"stable", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &c, NULL), ERROR_CHILD_MUST_BE_VOLATILE);
    EQ(U(RegCreateKeyExW)(v, L"volchild", 0, NULL, REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &c, NULL), 0);
    if (c) RegCloseKey(c);
    /* opening an existing key ignores the option */
    EQ(U(RegCreateKeyExW)(r, L"vol", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &c, &disp), 0);
    EQ(disp, REG_OPENED_EXISTING_KEY);
    if (c) RegCloseKey(c);
    RegCloseKey(v); RegCloseKey(r);
}
TEST(regkb_volatile_child_of_stable_ok) {
    REQUIRE(U(RegCreateKeyExW));
    HKEY r = FRESH(); REQUIRE(r); HKEY h = NULL;
    EQ(U(RegCreateKeyExW)(r, L"v1", 0, NULL, REG_OPTION_VOLATILE, KEY_ALL_ACCESS, NULL, &h, NULL), 0);
    if (h) { sc_dw(h, L"x", 1); RegCloseKey(h); }
    RegCloseKey(r);
}
TEST(regkb_backup_restore_option_without_privilege) {
    REQUIRE(U(RegCreateKeyExW));
    HKEY r = FRESH(); REQUIRE(r); HKEY h = NULL; DWORD d = 0;
    LSTATUS s = U(RegCreateKeyExW)(r, L"br", 0, NULL, REG_OPTION_BACKUP_RESTORE, KEY_ALL_ACCESS, NULL, &h, &d);
    t_note("REG_OPTION_BACKUP_RESTORE create = %ld", (long)s);
    if (s == 0) RegCloseKey(h);
    RegCloseKey(r);
}
TEST(regkb_security_attributes_null_dacl) {
    REQUIRE(U(RegCreateKeyExW));
    HKEY r = FRESH(); REQUIRE(r); HKEY h = NULL;
    SECURITY_DESCRIPTOR sd;
    CHECK(InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION));
    CHECK(SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE));
    SECURITY_ATTRIBUTES sa = { sizeof sa, &sd, FALSE };
    EQ(U(RegCreateKeyExW)(r, L"nulldacl", 0, NULL, 0, KEY_ALL_ACCESS, &sa, &h, NULL), 0);
    if (h) RegCloseKey(h);
    EQ(rk_exists(r, "nulldacl"), 1);
    /* inherit flag in the attributes must not matter for registry handles */
    sa.bInheritHandle = TRUE; sa.lpSecurityDescriptor = NULL;
    EQ(U(RegCreateKeyExW)(r, L"inh", 0, NULL, 0, KEY_ALL_ACCESS, &sa, &h, NULL), 0);
    if (h) {
        DWORD fl = 0; CHECK(GetHandleInformation(h, &fl));
        t_note("registry handle inherit flag with bInheritHandle: %lu", fl & HANDLE_FLAG_INHERIT);
        RegCloseKey(h);
    }
    RegCloseKey(r);
}
TEST(regkb_handle_default_not_inheritable) {
    REQUIRE(U(RegCreateKeyExW));
    HKEY r = FRESH(); REQUIRE(r); HKEY h = NULL;
    EQ(U(RegCreateKeyExW)(r, L"hh", 0, NULL, 0, KEY_READ, NULL, &h, NULL), 0);
    DWORD fl = 0; CHECK(GetHandleInformation(h, &fl));
    EQ(fl & HANDLE_FLAG_INHERIT, 0);
    RegCloseKey(h); RegCloseKey(r);
}
TEST(regkb_handles_are_distinct_per_open) {
    REQUIRE(U(RegOpenKeyExW));
    HKEY r = FRESH(); REQUIRE(r); sc_subc(r, L"s");
    HKEY a = NULL, b = NULL;
    EQ(U(RegOpenKeyExW)(r, L"s", 0, KEY_READ, &a), 0);
    EQ(U(RegOpenKeyExW)(r, L"s", 0, KEY_READ, &b), 0);
    CHECK(a != b);
    RegCloseKey(a);
    DWORD n; EQ(RegQueryInfoKeyW(b, NULL, NULL, NULL, &n, NULL, NULL, NULL, NULL, NULL, NULL, NULL), 0);
    RegCloseKey(b); RegCloseKey(r);
}
TEST(regkb_key_handle_is_kernel_handle) {
    REQUIRE(U(RegOpenKeyExW));
    HKEY r = FRESH(); REQUIRE(r);
    DWORD fl = 0;
    CHECK(GetHandleInformation(r, &fl));
    HKEY dup = NULL;
    CHECK(DuplicateHandle(GetCurrentProcess(), r, GetCurrentProcess(), (HANDLE *)&dup, 0, FALSE, DUPLICATE_SAME_ACCESS));
    if (dup) {
        sc_dw(dup, L"viadup", 9);
        DWORD v = 0, cb = 4, t;
        EQ(U(RegQueryValueExW)(r, L"viadup", NULL, &t, (BYTE *)&v, &cb), 0);
        EQ(v, 9);
        RegCloseKey(dup);
    }
    RegCloseKey(r);
}
TEST(regkb_connect_registry_local) {
    REQUIRE(UA(RegConnectRegistryW));
    HKEY h = NULL;
    EQ(UA(RegConnectRegistryW)(NULL, HKEY_LOCAL_MACHINE, &h), 0);
    CHECK(h != NULL);
    if (h) {
        HKEY k = NULL;
        EQ(RegOpenKeyExW(h, L"SOFTWARE", 0, KEY_READ, &k), 0);
        if (k) RegCloseKey(k);
        RegCloseKey(h);
    }
    h = NULL;
    EQ(UA(RegConnectRegistryW)(NULL, HKEY_CURRENT_USER, &h), 0);
    if (h) RegCloseKey(h);
}
TEST(regkb_connect_registry_a_local_and_bad_hive) {
    REQUIRE(UA(RegConnectRegistryA));
    HKEY h = NULL;
    EQ(UA(RegConnectRegistryA)(NULL, HKEY_USERS, &h), 0);
    if (h) RegCloseKey(h);
    h = NULL;
    LSTATUS s = UA(RegConnectRegistryA)(NULL, HKEY_CLASSES_ROOT, &h);
    t_note("RegConnectRegistryA(NULL, HKCR) = %ld", (long)s);
    if (s == 0) RegCloseKey(h);
}
TEST(regkb_open_current_user) {
    REQUIRE(U(RegOpenCurrentUser));
    HKEY h = NULL;
    EQ(U(RegOpenCurrentUser)(KEY_READ, &h), 0);
    CHECK(h != NULL);
    if (h) {
        HKEY k = NULL;
        EQ(RegOpenKeyExW(h, L"Software", 0, KEY_READ, &k), 0);
        if (k) RegCloseKey(k);
        EQ(RegCloseKey(h), 0);
    }
}
TEST(regkb_open_user_classes_root) {
    REQUIRE(U(RegOpenUserClassesRoot));
    HANDLE tok = NULL; HKEY h = NULL;
    CHECK(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_IMPERSONATE, &tok));
    LSTATUS s = U(RegOpenUserClassesRoot)(tok, 0, KEY_READ, &h);
    t_note("RegOpenUserClassesRoot(process token) = %ld", (long)s);
    if (s == 0) RegCloseKey(h);
    if (tok) CloseHandle(tok);
}
