/* gap_10: SHRegCreateUSKey/CloseUSKey/EnumUSKey/EnumUSValue/DeleteUSValue/DeleteEmptyUSKey, SHLoadIndirectString, SHCoCreateInstance */
#include "g10.h"

#define USK_HKCU        0x1   /* SHREGSET_HKCU */
#define USK_FORCE_HKCU  0x2   /* SHREGSET_FORCE_HKCU */
#define E_HKCU 1              /* SHREGENUM_HKCU */
#define E_HKLM 0x10           /* SHREGENUM_HKLM */
#define E_BOTH 0x11           /* SHREGENUM_BOTH */
#define D_HKCU 1              /* SHREGDEL_HKCU */
#define D_HKLM 0x10           /* SHREGDEL_HKLM */
#define D_BOTH 0x11           /* SHREGDEL_BOTH */

static char g_root[160];      /* Software\k32t_<pid>_g10 */
static WCHAR g_rootw[160];

static void rmroot(void) { RegDeleteTreeA(HKEY_CURRENT_USER, g_root); }
static int g_atexit;
static int mkroot(void) {
    HKEY k; DWORD d;
    snprintf(g_root, sizeof g_root, "Software\\k32t_%lu_g10", GetCurrentProcessId());
    MultiByteToWideChar(CP_ACP, 0, g_root, -1, g_rootw, 160);
    rmroot();
    if (!g_atexit) { g_atexit = 1; atexit(rmroot); }
    if (RegCreateKeyExA(HKEY_CURRENT_USER, g_root, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, &d)) return 0;
    RegCloseKey(k);
    return 1;
}
static int keyexists(const char *sub) {
    char p[400]; HKEY k;
    snprintf(p, sizeof p, "%s\\%s", g_root, sub);
    if (RegOpenKeyExA(HKEY_CURRENT_USER, p, 0, KEY_READ, &k)) return 0;
    RegCloseKey(k); return 1;
}
static void mksub(const char *sub) {
    char p[400]; HKEY k;
    snprintf(p, sizeof p, "%s\\%s", g_root, sub);
    if (!RegCreateKeyExA(HKEY_CURRENT_USER, p, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL)) RegCloseKey(k);
}
static void setdw(const char *sub, const char *name, DWORD v) {
    char p[400]; HKEY k;
    snprintf(p, sizeof p, "%s\\%s", g_root, sub);
    if (!RegCreateKeyExA(HKEY_CURRENT_USER, p, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL)) { RegSetValueExA(k, name, 0, REG_DWORD, (BYTE *)&v, 4); RegCloseKey(k); }
}
static void setsz(const char *sub, const char *name, const char *v) {
    char p[400]; HKEY k;
    snprintf(p, sizeof p, "%s\\%s", g_root, sub);
    if (!RegCreateKeyExA(HKEY_CURRENT_USER, p, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL)) { RegSetValueExA(k, name, 0, REG_SZ, (BYTE *)v, (DWORD)strlen(v) + 1); RegCloseKey(k); }
}
static int hasvalue(const char *sub, const char *name) {
    char p[400]; HKEY k; LONG r = 2;
    snprintf(p, sizeof p, "%s\\%s", g_root, sub);
    if (!RegOpenKeyExA(HKEY_CURRENT_USER, p, 0, KEY_READ, &k)) { r = RegQueryValueExA(k, name, NULL, NULL, NULL, NULL); RegCloseKey(k); }
    return r == 0;
}
static HUSKEY mkus(const char *sub) {   /* open/create root\sub through the function under test */
    char p[400]; HUSKEY h = NULL;
    snprintf(p, sizeof p, "%s\\%s", g_root, sub);
    LONG r = U(SHRegCreateUSKeyA)(p, KEY_ALL_ACCESS, NULL, &h, USK_FORCE_HKCU);
    return r == 0 ? h : NULL;
}
#define NEED(f) REQUIRE(U(f))
#define ROOT() do { if (!mkroot()) { t_skip("cannot create test registry key"); return; } } while (0)
#define ERR_NO_MORE 259
#define ERR_MORE 234

/* ---------------- create / close ---------------- */

TEST(gap10_usk_create_w_basic) {
    NEED(SHRegCreateUSKeyW); NEED(SHRegCloseUSKey); ROOT();
    WCHAR p[300]; HUSKEY h = (HUSKEY)(ULONG_PTR)0x1111;
    swprintf(p, 300, L"%ls\\us1", g_rootw);
    LONG r = U(SHRegCreateUSKeyW)(p, KEY_ALL_ACCESS, NULL, &h, USK_FORCE_HKCU);
    EQ1(r, 0);
    CHECK(h != NULL && h != (HUSKEY)(ULONG_PTR)0x1111);
    CHECK(keyexists("us1"));
    EQ1(U(SHRegCloseUSKey)(h), 0);
    rmroot();
}
TEST(gap10_usk_create_a_basic) {
    NEED(SHRegCreateUSKeyA); NEED(SHRegCloseUSKey); ROOT();
    char p[300]; HUSKEY h = NULL;
    snprintf(p, sizeof p, "%s\\us1a", g_root);
    EQ1(U(SHRegCreateUSKeyA)(p, KEY_ALL_ACCESS, NULL, &h, USK_FORCE_HKCU), 0);
    CHECK(h != NULL);
    CHECK(keyexists("us1a"));
    EQ1(U(SHRegCloseUSKey)(h), 0);
    rmroot();
}
TEST(gap10_usk_create_flag_variants_hkcu) {
    NEED(SHRegCreateUSKeyW); NEED(SHRegCloseUSKey); ROOT();
    static const DWORD fl[] = { USK_HKCU, USK_FORCE_HKCU, USK_HKCU | USK_FORCE_HKCU };
    for (int i = 0; i < 3; i++) {
        WCHAR p[300]; HUSKEY h = NULL; char sub[16];
        swprintf(p, 300, L"%ls\\fl%d", g_rootw, i); snprintf(sub, sizeof sub, "fl%d", i);
        EQ1(U(SHRegCreateUSKeyW)(p, KEY_ALL_ACCESS, NULL, &h, fl[i]), 0);
        CHECK(h != NULL);
        CHECK(keyexists(sub));
        if (h) U(SHRegCloseUSKey)(h);
    }
    rmroot();
}
/* decomp SHRegCreateUSKeyW: (dwFlags & 0xf) == 0 -> ERROR_INVALID_PARAMETER, *phNewUSKey zeroed */
TEST(gap10_usk_create_invalid_flags) {
    NEED(SHRegCreateUSKeyW); NEED(SHRegCreateUSKeyA); ROOT();
    static const DWORD bad[] = { 0, 0x10, 0x20, 0x100, 0x80000000, 0xFFFFFFF0 };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        WCHAR p[300]; char pa[300]; HUSKEY h = (HUSKEY)(ULONG_PTR)0x5555;
        swprintf(p, 300, L"%ls\\badfl", g_rootw);
        EQ1(U(SHRegCreateUSKeyW)(p, KEY_ALL_ACCESS, NULL, &h, bad[i]), ERROR_INVALID_PARAMETER);
        CHECK(h == NULL);
        h = (HUSKEY)(ULONG_PTR)0x5555;
        snprintf(pa, sizeof pa, "%s\\badfl", g_root);
        EQ1(U(SHRegCreateUSKeyA)(pa, KEY_ALL_ACCESS, NULL, &h, bad[i]), ERROR_INVALID_PARAMETER);
        CHECK(h == NULL);
    }
    CHECK(!keyexists("badfl"));
    rmroot();
}
TEST(gap10_usk_create_null_out_pointer) {
    NEED(SHRegCreateUSKeyW); NEED(SHRegCreateUSKeyA); ROOT();
    WCHAR p[300]; char pa[300];
    swprintf(p, 300, L"%ls\\nullout", g_rootw); snprintf(pa, sizeof pa, "%s\\nullout", g_root);
    EQ1(U(SHRegCreateUSKeyW)(p, KEY_ALL_ACCESS, NULL, NULL, USK_FORCE_HKCU), ERROR_INVALID_PARAMETER);
    EQ1(U(SHRegCreateUSKeyA)(pa, KEY_ALL_ACCESS, NULL, NULL, USK_FORCE_HKCU), ERROR_INVALID_PARAMETER);
    CHECK(!keyexists("nullout"));
    rmroot();
}
TEST(gap10_usk_create_a_null_path) {
    if (t_is_wine()) { t_skip("emulator faults on NULL USKEY/path"); return; }
    NEED(SHRegCreateUSKeyA);
    HUSKEY h = (HUSKEY)(ULONG_PTR)0x7777;
    SET_ERR(0);
    LONG r = U(SHRegCreateUSKeyA)(NULL, KEY_ALL_ACCESS, NULL, &h, USK_FORCE_HKCU);
    EQ1(r, ERROR_INVALID_PARAMETER);
}
/* decomp SHRegCreateUSKeyA: MultiByteToWideChar(-1) into WCHAR[256]; overflow -> ERROR_INSUFFICIENT_BUFFER */
TEST(gap10_usk_create_a_path_length_limit) {
    NEED(SHRegCreateUSKeyA); NEED(SHRegCloseUSKey); ROOT();
    char p[400]; HUSKEY h = NULL; size_t pre;
    snprintf(p, sizeof p, "%s\\", g_root); pre = strlen(p);
    /* 255 characters + NUL fit the internal 256-WCHAR conversion buffer */
    memset(p + pre, 'a', 255 - pre); p[255] = 0;
    EQ1(U(SHRegCreateUSKeyA)(p, KEY_ALL_ACCESS, NULL, &h, USK_FORCE_HKCU), 0);
    CHECK(h != NULL);
    if (h) U(SHRegCloseUSKey)(h);
    /* 256 characters do not */
    h = NULL;
    memset(p + pre, 'b', 256 - pre); p[256] = 0;
    EQ1(U(SHRegCreateUSKeyA)(p, KEY_ALL_ACCESS, NULL, &h, USK_FORCE_HKCU), ERROR_INSUFFICIENT_BUFFER);
    CHECK(h == NULL);
    rmroot();
}
TEST(gap10_usk_create_existing_keeps_values) {
    NEED(SHRegCreateUSKeyA); NEED(SHRegCloseUSKey); ROOT();
    setdw("keep", "val", 0xCAFE);
    HUSKEY h = mkus("keep");
    CHECK(h != NULL);
    CHECK(hasvalue("keep", "val"));
    if (h) U(SHRegCloseUSKey)(h);
    rmroot();
}
TEST(gap10_usk_create_twice_independent_handles) {
    NEED(SHRegCreateUSKeyA); NEED(SHRegCloseUSKey); ROOT();
    HUSKEY a = mkus("twice"), b = mkus("twice");
    CHECK(a != NULL && b != NULL && a != b);
    if (a) EQ1(U(SHRegCloseUSKey)(a), 0);
    if (b) EQ1(U(SHRegCloseUSKey)(b), 0);
    rmroot();
}
TEST(gap10_usk_create_relative) {
    NEED(SHRegCreateUSKeyW); NEED(SHRegCloseUSKey); ROOT();
    HUSKEY parent = mkus("par"), child = NULL;
    CHECK(parent != NULL);
    if (!parent) return;
    LONG r = U(SHRegCreateUSKeyW)(L"child", KEY_ALL_ACCESS, parent, &child, USK_FORCE_HKCU);
    EQ1(r, 0);
    CHECK(child != NULL);
    CHECK(keyexists("par\\child"));
    if (child) U(SHRegCloseUSKey)(child);
    child = NULL;
    r = U(SHRegCreateUSKeyW)(L"x\\y\\z", KEY_ALL_ACCESS, parent, &child, USK_FORCE_HKCU);
    EQ1(r, 0);
    CHECK(keyexists("par\\x\\y\\z"));
    if (child) U(SHRegCloseUSKey)(child);
    U(SHRegCloseUSKey)(parent);
    rmroot();
}
TEST(gap10_usk_create_relative_invalid_flags) {
    NEED(SHRegCreateUSKeyW); NEED(SHRegCloseUSKey); ROOT();
    HUSKEY parent = mkus("parf"), child = (HUSKEY)(ULONG_PTR)0x3333;
    if (!parent) { t_skip("no parent"); return; }
    EQ1(U(SHRegCreateUSKeyW)(L"child", KEY_ALL_ACCESS, parent, &child, 0), ERROR_INVALID_PARAMETER);
    CHECK(child == NULL);
    CHECK(!keyexists("parf\\child"));
    U(SHRegCloseUSKey)(parent);
    rmroot();
}
TEST(gap10_usk_create_read_access) {
    NEED(SHRegCreateUSKeyW); NEED(SHRegCloseUSKey); ROOT();
    WCHAR p[300]; HUSKEY h = NULL;
    swprintf(p, 300, L"%ls\\ro", g_rootw);
    LONG r = U(SHRegCreateUSKeyW)(p, KEY_READ, NULL, &h, USK_FORCE_HKCU);
    EQ1(r, 0);
    CHECK(keyexists("ro"));
    if (h) U(SHRegCloseUSKey)(h);
    rmroot();
}
TEST(gap10_usk_close_null) {
    NEED(SHRegCloseUSKey);
    EQ1(U(SHRegCloseUSKey)(NULL), ERROR_INVALID_PARAMETER);
}
TEST(gap10_usk_close_many) {
    NEED(SHRegCreateUSKeyA); NEED(SHRegCloseUSKey); ROOT();
    HUSKEY h[64];
    for (int i = 0; i < 64; i++) { h[i] = mkus("many"); CHECK(h[i] != NULL); }
    for (int i = 0; i < 64; i++) if (h[i]) EQ1(U(SHRegCloseUSKey)(h[i]), 0);
    rmroot();
}

/* ---------------- enumeration ---------------- */

static HUSKEY setup_enum_keys(void) {
    mksub("en\\k_a"); mksub("en\\k_b"); mksub("en\\k_c");
    return mkus("en");
}
static int in3(const char *s) { return !strcmp(s, "k_a") || !strcmp(s, "k_b") || !strcmp(s, "k_c"); }

TEST(gap10_usk_enumkey_a_all_flags) {
    NEED(SHRegEnumUSKeyA); NEED(SHRegCloseUSKey); ROOT();
    HUSKEY h = setup_enum_keys(); if (!h) { t_skip("no key"); return; }
    static const DWORD fl[] = { 0, E_HKCU };
    for (int f = 0; f < 2; f++) {
        int seen[3] = {0};
        for (DWORD i = 0; i < 3; i++) {
            char n[64] = {0}; DWORD cch = 64;
            LONG r = U(SHRegEnumUSKeyA)(h, i, n, &cch, (SHREGENUM_FLAGS)fl[f]);
            EQ1(r, 0); EQ1(cch, 3);
            CHECK(in3(n));
            if (in3(n)) seen[n[2] - 'a']++;
        }
        CHECKF(seen[0] == 1 && seen[1] == 1 && seen[2] == 1, "flags %lu each key exactly once", fl[f]);
        char n[64]; DWORD cch = 64;
        EQ1(U(SHRegEnumUSKeyA)(h, 3, n, &cch, (SHREGENUM_FLAGS)fl[f]), ERR_NO_MORE);
        cch = 64;
        EQ1(U(SHRegEnumUSKeyA)(h, 0xFFFFFFFF, n, &cch, (SHREGENUM_FLAGS)fl[f]), ERR_NO_MORE);
    }
    U(SHRegCloseUSKey)(h); rmroot();
}
TEST(gap10_usk_enumkey_w_all_flags) {
    NEED(SHRegEnumUSKeyW); NEED(SHRegCloseUSKey); ROOT();
    HUSKEY h = setup_enum_keys(); if (!h) { t_skip("no key"); return; }
    static const DWORD fl[] = { 0, E_HKCU };
    for (int f = 0; f < 2; f++) {
        int seen[3] = {0};
        for (DWORD i = 0; i < 3; i++) {
            WCHAR n[64] = {0}; DWORD cch = 64; char a[64];
            EQ1(U(SHRegEnumUSKeyW)(h, i, n, &cch, (SHREGENUM_FLAGS)fl[f]), 0);
            EQ1(cch, 3);
            WideCharToMultiByte(CP_ACP, 0, n, -1, a, 64, NULL, NULL);
            CHECK(in3(a));
            if (in3(a)) seen[a[2] - 'a']++;
        }
        CHECK(seen[0] == 1 && seen[1] == 1 && seen[2] == 1);
        WCHAR n[8]; DWORD cch = 8;
        EQ1(U(SHRegEnumUSKeyW)(h, 3, n, &cch, (SHREGENUM_FLAGS)fl[f]), ERR_NO_MORE);
    }
    U(SHRegCloseUSKey)(h); rmroot();
}
TEST(gap10_usk_enumkey_ab_parity) {
    NEED(SHRegEnumUSKeyA); NEED(SHRegEnumUSKeyW); NEED(SHRegCloseUSKey); ROOT();
    HUSKEY h = setup_enum_keys(); if (!h) { t_skip("no key"); return; }
    for (DWORD i = 0; i < 3; i++) {
        char a[64] = {0}; WCHAR w[64] = {0}; DWORD ca = 64, cw = 64, k;
        EQ1(U(SHRegEnumUSKeyA)(h, i, a, &ca, (SHREGENUM_FLAGS)E_HKCU), 0);
        EQ1(U(SHRegEnumUSKeyW)(h, i, w, &cw, (SHREGENUM_FLAGS)E_HKCU), 0);
        EQ1(ca, cw);
        for (k = 0; k < cw; k++) if ((WCHAR)(unsigned char)a[k] != w[k]) break;
        EQ1(k, cw);
    }
    U(SHRegCloseUSKey)(h); rmroot();
}
TEST(gap10_usk_enumkey_buffer_boundaries) {
    NEED(SHRegEnumUSKeyA); NEED(SHRegEnumUSKeyW); NEED(SHRegCloseUSKey); ROOT();
    HUSKEY h = setup_enum_keys(); if (!h) { t_skip("no key"); return; }
    static const DWORD fl[] = { 0, E_HKCU };
    for (int f = 0; f < 2; f++) {
        char a[16]; WCHAR w[16]; DWORD cch;
        cch = 4; EQ1(U(SHRegEnumUSKeyA)(h, 0, a, &cch, (SHREGENUM_FLAGS)fl[f]), 0); EQ1(cch, 3);
        cch = 3; EQ1(U(SHRegEnumUSKeyA)(h, 0, a, &cch, (SHREGENUM_FLAGS)fl[f]), ERR_MORE);
        cch = 1; EQ1(U(SHRegEnumUSKeyA)(h, 0, a, &cch, (SHREGENUM_FLAGS)fl[f]), ERR_MORE);
        cch = 4; EQ1(U(SHRegEnumUSKeyW)(h, 1, w, &cch, (SHREGENUM_FLAGS)fl[f]), 0); EQ1(cch, 3);
        cch = 3; EQ1(U(SHRegEnumUSKeyW)(h, 1, w, &cch, (SHREGENUM_FLAGS)fl[f]), ERR_MORE);
    }
    U(SHRegCloseUSKey)(h); rmroot();
}
TEST(gap10_usk_enumkey_leaf_has_no_children) {
    NEED(SHRegEnumUSKeyA); NEED(SHRegCloseUSKey); ROOT();
    HUSKEY h = mkus("leaf"); if (!h) { t_skip("no key"); return; }
    char n[16]; DWORD cch = 16;
    EQ1(U(SHRegEnumUSKeyA)(h, 0, n, &cch, (SHREGENUM_FLAGS)E_HKCU), ERR_NO_MORE);
    cch = 16;
    EQ1(U(SHRegEnumUSKeyA)(h, 0, n, &cch, (SHREGENUM_FLAGS)0), ERR_NO_MORE);
    U(SHRegCloseUSKey)(h); rmroot();
}
TEST(gap10_usk_enumkey_hklm_side_missing) {
    NEED(SHRegEnumUSKeyA); NEED(SHRegEnumUSKeyW); NEED(SHRegCloseUSKey); ROOT();
    HUSKEY h = setup_enum_keys(); if (!h) { t_skip("no key"); return; }
    char a[16]; WCHAR w[16]; DWORD cch = 16;
    /* the private key does not exist under HKLM */
    EQ1(U(SHRegEnumUSKeyA)(h, 0, a, &cch, (SHREGENUM_FLAGS)E_HKLM), ERROR_FILE_NOT_FOUND);
    cch = 16;
    EQ1(U(SHRegEnumUSKeyW)(h, 0, w, &cch, (SHREGENUM_FLAGS)E_HKLM), ERROR_FILE_NOT_FOUND);
    U(SHRegCloseUSKey)(h); rmroot();
}
TEST(gap10_usk_enumkey_invalid_args) {
    if (t_is_wine()) { t_skip("emulator faults on NULL USKEY/path"); return; }
    NEED(SHRegEnumUSKeyA); NEED(SHRegEnumUSKeyW); NEED(SHRegCloseUSKey); ROOT();
    HUSKEY h = setup_enum_keys(); if (!h) { t_skip("no key"); return; }
    char a[16]; WCHAR w[16]; DWORD cch;
    /* SHREGENUM_BOTH is not implemented, and is checked before anything else */
    cch = 16; EQ1(U(SHRegEnumUSKeyA)(h, 0, a, &cch, (SHREGENUM_FLAGS)E_BOTH), ERROR_CALL_NOT_IMPLEMENTED);
    cch = 16; EQ1(U(SHRegEnumUSKeyW)(h, 0, w, &cch, (SHREGENUM_FLAGS)E_BOTH), ERROR_CALL_NOT_IMPLEMENTED);
    cch = 16; EQ1(U(SHRegEnumUSKeyA)(NULL, 0, a, &cch, (SHREGENUM_FLAGS)E_BOTH), ERROR_CALL_NOT_IMPLEMENTED);
    cch = 16; EQ1(U(SHRegEnumUSKeyW)(NULL, 0, w, &cch, (SHREGENUM_FLAGS)E_BOTH), ERROR_CALL_NOT_IMPLEMENTED);
    static const DWORD bad[] = { 4, 8, 0x20, 0x100, 0x80000000 };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        cch = 16; EQ1(U(SHRegEnumUSKeyA)(h, 0, a, &cch, (SHREGENUM_FLAGS)bad[i]), ERROR_INVALID_PARAMETER);
        cch = 16; EQ1(U(SHRegEnumUSKeyW)(h, 0, w, &cch, (SHREGENUM_FLAGS)bad[i]), ERROR_INVALID_PARAMETER);
    }
    cch = 16; EQ1(U(SHRegEnumUSKeyA)(NULL, 0, a, &cch, (SHREGENUM_FLAGS)0), ERROR_INVALID_PARAMETER);
    cch = 16; EQ1(U(SHRegEnumUSKeyW)(NULL, 0, w, &cch, (SHREGENUM_FLAGS)E_HKCU), ERROR_INVALID_PARAMETER);
    U(SHRegCloseUSKey)(h); rmroot();
}

static HUSKEY setup_enum_values(void) {
    setdw("ev", "v_dword", 0x1234); setsz("ev", "v_sz", "hello");
    { char p[300]; HKEY k; BYTE bin[3] = { 1, 2, 3 };
      snprintf(p, sizeof p, "%s\\ev", g_root);
      if (!RegOpenKeyExA(HKEY_CURRENT_USER, p, 0, KEY_ALL_ACCESS, &k)) { RegSetValueExA(k, "v_bin", 0, REG_BINARY, bin, 3); RegCloseKey(k); } }
    return mkus("ev");
}
TEST(gap10_usk_enumvalue_a_all) {
    NEED(SHRegEnumUSValueA); NEED(SHRegCloseUSKey); ROOT();
    HUSKEY h = setup_enum_values(); if (!h) { t_skip("no key"); return; }
    static const DWORD fl[] = { 0, E_HKCU };
    for (int f = 0; f < 2; f++) {
        int seen = 0;
        for (DWORD i = 0; i < 3; i++) {
            char n[64] = {0}; DWORD cn = 64, ty = 0xAA, cb = 64; BYTE d[64] = {0};
            EQ1(U(SHRegEnumUSValueA)(h, i, n, &cn, &ty, d, &cb, (SHREGENUM_FLAGS)fl[f]), 0);
            if (!strcmp(n, "v_dword")) { seen |= 1; EQ1(cn, 7); EQ1(ty, REG_DWORD); EQ1(cb, 4); EQ1(*(DWORD *)d, 0x1234); }
            else if (!strcmp(n, "v_sz")) { seen |= 2; EQ1(cn, 4); EQ1(ty, REG_SZ); EQ1(cb, 6); EXPECT_STR((char *)d, "hello"); }
            else if (!strcmp(n, "v_bin")) { seen |= 4; EQ1(cn, 5); EQ1(ty, REG_BINARY); EQ1(cb, 3); CHECK(d[0] == 1 && d[1] == 2 && d[2] == 3); }
            else CHECKF(0, "unexpected value name \"%s\"", n);
        }
        EQ1(seen, 7);
        char n[64]; DWORD cn = 64;
        EQ1(U(SHRegEnumUSValueA)(h, 3, n, &cn, NULL, NULL, NULL, (SHREGENUM_FLAGS)fl[f]), ERR_NO_MORE);
    }
    U(SHRegCloseUSKey)(h); rmroot();
}
TEST(gap10_usk_enumvalue_w_all) {
    NEED(SHRegEnumUSValueW); NEED(SHRegCloseUSKey); ROOT();
    HUSKEY h = setup_enum_values(); if (!h) { t_skip("no key"); return; }
    int seen = 0;
    for (DWORD i = 0; i < 3; i++) {
        WCHAR n[64] = {0}; DWORD cn = 64, ty = 0xAA, cb = 64; BYTE d[64] = {0};
        EQ1(U(SHRegEnumUSValueW)(h, i, n, &cn, &ty, d, &cb, (SHREGENUM_FLAGS)E_HKCU), 0);
        if (!lstrcmpW(n, L"v_dword")) { seen |= 1; EQ1(ty, REG_DWORD); EQ1(*(DWORD *)d, 0x1234); EQ1(cn, 7); }
        else if (!lstrcmpW(n, L"v_sz")) { seen |= 2; EQ1(ty, REG_SZ); EQ1(cb, 12); EXPECT_WSTR((WCHAR *)d, L"hello"); }
        else if (!lstrcmpW(n, L"v_bin")) { seen |= 4; EQ1(ty, REG_BINARY); EQ1(cb, 3); }
        else CHECK(0);
    }
    EQ1(seen, 7);
    WCHAR n[8]; DWORD cn = 8;
    EQ1(U(SHRegEnumUSValueW)(h, 3, n, &cn, NULL, NULL, NULL, (SHREGENUM_FLAGS)0), ERR_NO_MORE);
    U(SHRegCloseUSKey)(h); rmroot();
}
TEST(gap10_usk_enumvalue_optional_outputs_null) {
    NEED(SHRegEnumUSValueA); NEED(SHRegCloseUSKey); ROOT();
    HUSKEY h = setup_enum_values(); if (!h) { t_skip("no key"); return; }
    char n[64]; DWORD cn = 64;
    EQ1(U(SHRegEnumUSValueA)(h, 0, n, &cn, NULL, NULL, NULL, (SHREGENUM_FLAGS)E_HKCU), 0);
    CHECK(cn >= 4 && cn <= 7);
    U(SHRegCloseUSKey)(h); rmroot();
}
TEST(gap10_usk_enumvalue_name_truncation) {
    NEED(SHRegEnumUSValueA); NEED(SHRegEnumUSValueW); NEED(SHRegCloseUSKey); ROOT();
    setsz("evt", "abcdef", "x");
    HUSKEY h = mkus("evt"); if (!h) { t_skip("no key"); return; }
    char a[16]; WCHAR w[16]; DWORD cn;
    cn = 7; EQ1(U(SHRegEnumUSValueA)(h, 0, a, &cn, NULL, NULL, NULL, (SHREGENUM_FLAGS)E_HKCU), 0); EQ1(cn, 6);
    cn = 6; EQ1(U(SHRegEnumUSValueA)(h, 0, a, &cn, NULL, NULL, NULL, (SHREGENUM_FLAGS)E_HKCU), ERR_MORE);
    cn = 6; EQ1(U(SHRegEnumUSValueA)(h, 0, a, &cn, NULL, NULL, NULL, (SHREGENUM_FLAGS)0), ERR_MORE);
    cn = 6; EQ1(U(SHRegEnumUSValueW)(h, 0, w, &cn, NULL, NULL, NULL, (SHREGENUM_FLAGS)E_HKCU), ERR_MORE);
    cn = 7; EQ1(U(SHRegEnumUSValueW)(h, 0, w, &cn, NULL, NULL, NULL, (SHREGENUM_FLAGS)E_HKCU), 0);
    U(SHRegCloseUSKey)(h); rmroot();
}
TEST(gap10_usk_enumvalue_data_truncation) {
    NEED(SHRegEnumUSValueA); NEED(SHRegCloseUSKey); ROOT();
    setsz("evd", "s", "hello");
    HUSKEY h = mkus("evd"); if (!h) { t_skip("no key"); return; }
    char n[16]; BYTE d[16]; DWORD cn = 16, ty = 0, cb = 3;
    EQ1(U(SHRegEnumUSValueA)(h, 0, n, &cn, &ty, d, &cb, (SHREGENUM_FLAGS)E_HKCU), ERR_MORE);
    CHECKF(cb == 6, "required size reported (got %lu)", cb);
    cn = 16; cb = 6;
    EQ1(U(SHRegEnumUSValueA)(h, 0, n, &cn, &ty, d, &cb, (SHREGENUM_FLAGS)E_HKCU), 0);
    EQ1(cb, 6);
    U(SHRegCloseUSKey)(h); rmroot();
}
TEST(gap10_usk_enumvalue_hklm_side_missing) {
    NEED(SHRegEnumUSValueA); NEED(SHRegEnumUSValueW); NEED(SHRegCloseUSKey); ROOT();
    HUSKEY h = setup_enum_values(); if (!h) { t_skip("no key"); return; }
    char a[16]; WCHAR w[16]; DWORD cn = 16;
    EQ1(U(SHRegEnumUSValueA)(h, 0, a, &cn, NULL, NULL, NULL, (SHREGENUM_FLAGS)E_HKLM), ERROR_FILE_NOT_FOUND);
    cn = 16;
    EQ1(U(SHRegEnumUSValueW)(h, 0, w, &cn, NULL, NULL, NULL, (SHREGENUM_FLAGS)E_HKLM), ERROR_FILE_NOT_FOUND);
    U(SHRegCloseUSKey)(h); rmroot();
}
TEST(gap10_usk_enumvalue_invalid_args) {
    if (t_is_wine()) { t_skip("emulator faults on NULL USKEY/path"); return; }
    NEED(SHRegEnumUSValueA); NEED(SHRegEnumUSValueW); NEED(SHRegCloseUSKey); ROOT();
    HUSKEY h = setup_enum_values(); if (!h) { t_skip("no key"); return; }
    char a[16]; WCHAR w[16]; DWORD cn;
    cn = 16; EQ1(U(SHRegEnumUSValueA)(h, 0, a, &cn, NULL, NULL, NULL, (SHREGENUM_FLAGS)E_BOTH), ERROR_CALL_NOT_IMPLEMENTED);
    cn = 16; EQ1(U(SHRegEnumUSValueW)(h, 0, w, &cn, NULL, NULL, NULL, (SHREGENUM_FLAGS)E_BOTH), ERROR_CALL_NOT_IMPLEMENTED);
    cn = 16; EQ1(U(SHRegEnumUSValueA)(NULL, 0, a, &cn, NULL, NULL, NULL, (SHREGENUM_FLAGS)E_BOTH), ERROR_CALL_NOT_IMPLEMENTED);
    static const DWORD bad[] = { 4, 8, 0x20, 0x80000000 };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        cn = 16; EQ1(U(SHRegEnumUSValueA)(h, 0, a, &cn, NULL, NULL, NULL, (SHREGENUM_FLAGS)bad[i]), ERROR_INVALID_PARAMETER);
        cn = 16; EQ1(U(SHRegEnumUSValueW)(h, 0, w, &cn, NULL, NULL, NULL, (SHREGENUM_FLAGS)bad[i]), ERROR_INVALID_PARAMETER);
    }
    cn = 16; EQ1(U(SHRegEnumUSValueA)(NULL, 0, a, &cn, NULL, NULL, NULL, (SHREGENUM_FLAGS)0), ERROR_INVALID_PARAMETER);
    cn = 16; EQ1(U(SHRegEnumUSValueW)(NULL, 0, w, &cn, NULL, NULL, NULL, (SHREGENUM_FLAGS)E_HKCU), ERROR_INVALID_PARAMETER);
    U(SHRegCloseUSKey)(h); rmroot();
}
TEST(gap10_usk_enumvalue_empty_key) {
    NEED(SHRegEnumUSValueA); NEED(SHRegCloseUSKey); ROOT();
    HUSKEY h = mkus("emptyv"); if (!h) { t_skip("no key"); return; }
    char a[16]; DWORD cn = 16;
    EQ1(U(SHRegEnumUSValueA)(h, 0, a, &cn, NULL, NULL, NULL, (SHREGENUM_FLAGS)E_HKCU), ERR_NO_MORE);
    cn = 16;
    EQ1(U(SHRegEnumUSValueA)(h, 0, a, &cn, NULL, NULL, NULL, (SHREGENUM_FLAGS)0), ERR_NO_MORE);
    U(SHRegCloseUSKey)(h); rmroot();
}

/* ---------------- delete value / delete empty key ---------------- */

TEST(gap10_usk_delvalue_w_hkcu) {
    if (t_is_wine()) { t_skip("SHRegDelete*USKey are not implemented in the emulator"); return; }
    NEED(SHRegDeleteUSValueW); NEED(SHRegCloseUSKey); ROOT();
    setsz("dv", "gone", "x"); setsz("dv", "stay", "y");
    HUSKEY h = mkus("dv"); if (!h) { t_skip("no key"); return; }
    EQ1(U(SHRegDeleteUSValueW)(h, L"gone", (SHREGDEL_FLAGS)D_HKCU), 0);
    CHECK(!hasvalue("dv", "gone")); CHECK(hasvalue("dv", "stay"));
    EQ1(U(SHRegDeleteUSValueW)(h, L"gone", (SHREGDEL_FLAGS)D_HKCU), ERROR_FILE_NOT_FOUND);
    U(SHRegCloseUSKey)(h); rmroot();
}
TEST(gap10_usk_delvalue_a_hkcu) {
    if (t_is_wine()) { t_skip("SHRegDelete*USKey are not implemented in the emulator"); return; }
    NEED(SHRegDeleteUSValueA); NEED(SHRegCloseUSKey); ROOT();
    setsz("dva", "gone", "x");
    HUSKEY h = mkus("dva"); if (!h) { t_skip("no key"); return; }
    EQ1(U(SHRegDeleteUSValueA)(h, "gone", (SHREGDEL_FLAGS)D_HKCU), 0);
    CHECK(!hasvalue("dva", "gone"));
    EQ1(U(SHRegDeleteUSValueA)(h, "gone", (SHREGDEL_FLAGS)D_HKCU), ERROR_FILE_NOT_FOUND);
    U(SHRegCloseUSKey)(h); rmroot();
}
TEST(gap10_usk_delvalue_default_flag) {
    if (t_is_wine()) { t_skip("SHRegDelete*USKey are not implemented in the emulator"); return; }
    NEED(SHRegDeleteUSValueW); NEED(SHRegDeleteUSValueA); NEED(SHRegCloseUSKey); ROOT();
    setsz("dvd", "one", "x"); setsz("dvd", "two", "y");
    HUSKEY h = mkus("dvd"); if (!h) { t_skip("no key"); return; }
    EQ1(U(SHRegDeleteUSValueW)(h, L"one", (SHREGDEL_FLAGS)0), 0);
    EQ1(U(SHRegDeleteUSValueA)(h, "two", (SHREGDEL_FLAGS)0), 0);
    CHECK(!hasvalue("dvd", "one")); CHECK(!hasvalue("dvd", "two"));
    /* missing value: the HKLM fallback does not find the private key either */
    EQ1(U(SHRegDeleteUSValueW)(h, L"one", (SHREGDEL_FLAGS)0), ERROR_FILE_NOT_FOUND);
    U(SHRegCloseUSKey)(h); rmroot();
}
TEST(gap10_usk_delvalue_hklm_flag_leaves_hkcu) {
    if (t_is_wine()) { t_skip("SHRegDelete*USKey are not implemented in the emulator"); return; }
    NEED(SHRegDeleteUSValueW); NEED(SHRegCloseUSKey); ROOT();
    setsz("dvh", "keep", "x");
    HUSKEY h = mkus("dvh"); if (!h) { t_skip("no key"); return; }
    /* only the HKLM side is touched; the private path does not exist there */
    EQ1(U(SHRegDeleteUSValueW)(h, L"keep", (SHREGDEL_FLAGS)D_HKLM), ERROR_FILE_NOT_FOUND);
    CHECK(hasvalue("dvh", "keep"));
    U(SHRegCloseUSKey)(h); rmroot();
}
TEST(gap10_usk_delvalue_both_flag) {
    if (t_is_wine()) { t_skip("SHRegDelete*USKey are not implemented in the emulator"); return; }
    NEED(SHRegDeleteUSValueW); NEED(SHRegCloseUSKey); ROOT();
    setsz("dvb", "val", "x");
    HUSKEY h = mkus("dvb"); if (!h) { t_skip("no key"); return; }
    SET_ERR(0);
    LONG r = U(SHRegDeleteUSValueW)(h, L"val", (SHREGDEL_FLAGS)D_BOTH);
    t_note("SHRegDeleteUSValueW(BOTH) on an HKCU-only key -> %ld", r);
    CHECK(!hasvalue("dvb", "val"));
    U(SHRegCloseUSKey)(h); rmroot();
}
TEST(gap10_usk_delvalue_invalid_args) {
    if (t_is_wine()) { t_skip("SHRegDelete*USKey are not implemented in the emulator"); return; }
    NEED(SHRegDeleteUSValueW); NEED(SHRegDeleteUSValueA); NEED(SHRegCloseUSKey); ROOT();
    setsz("dvi", "val", "x");
    HUSKEY h = mkus("dvi"); if (!h) { t_skip("no key"); return; }
    static const DWORD bad[] = { 4, 8, 0x20, 0x80000000 };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        EQ1(U(SHRegDeleteUSValueW)(h, L"val", (SHREGDEL_FLAGS)bad[i]), ERROR_INVALID_PARAMETER);
        EQ1(U(SHRegDeleteUSValueA)(h, "val", (SHREGDEL_FLAGS)bad[i]), ERROR_INVALID_PARAMETER);
    }
    CHECK(hasvalue("dvi", "val"));
    EQ1(U(SHRegDeleteUSValueW)(NULL, L"val", (SHREGDEL_FLAGS)D_HKCU), ERROR_INVALID_PARAMETER);
    EQ1(U(SHRegDeleteUSValueA)(NULL, "val", (SHREGDEL_FLAGS)0), ERROR_INVALID_PARAMETER);
    U(SHRegCloseUSKey)(h); rmroot();
}
TEST(gap10_usk_delvalue_a_null_name) {
    if (t_is_wine()) { t_skip("SHRegDelete*USKey are not implemented in the emulator"); return; }
    NEED(SHRegDeleteUSValueA); NEED(SHRegCloseUSKey); ROOT();
    HUSKEY h = mkus("dvn"); if (!h) { t_skip("no key"); return; }
    SET_ERR(0);
    LONG r = U(SHRegDeleteUSValueA)(h, NULL, (SHREGDEL_FLAGS)D_HKCU);
    t_note("SHRegDeleteUSValueA(NULL name) -> %ld", r);
    CHECK(r != 0);
    U(SHRegCloseUSKey)(h); rmroot();
}
TEST(gap10_usk_delvalue_many) {
    if (t_is_wine()) { t_skip("SHRegDelete*USKey are not implemented in the emulator"); return; }
    NEED(SHRegDeleteUSValueW); NEED(SHRegCloseUSKey); ROOT();
    char nm[16];
    for (int i = 0; i < 40; i++) { snprintf(nm, sizeof nm, "v%d", i); setdw("dvm", nm, i); }
    HUSKEY h = mkus("dvm"); if (!h) { t_skip("no key"); return; }
    for (int i = 0; i < 40; i++) {
        WCHAR w[16]; swprintf(w, 16, L"v%d", i); snprintf(nm, sizeof nm, "v%d", i);
        EQ1(U(SHRegDeleteUSValueW)(h, w, (SHREGDEL_FLAGS)D_HKCU), 0);
        CHECK(!hasvalue("dvm", nm));
    }
    U(SHRegCloseUSKey)(h); rmroot();
}

TEST(gap10_usk_delkey_w_hkcu) {
    if (t_is_wine()) { t_skip("SHRegDelete*USKey are not implemented in the emulator"); return; }
    NEED(SHRegDeleteEmptyUSKeyW); NEED(SHRegCloseUSKey); ROOT();
    mksub("dk\\victim"); mksub("dk\\other");
    HUSKEY h = mkus("dk"); if (!h) { t_skip("no key"); return; }
    EQ1(U(SHRegDeleteEmptyUSKeyW)(h, L"victim", (SHREGDEL_FLAGS)D_HKCU), 0);
    CHECK(!keyexists("dk\\victim")); CHECK(keyexists("dk\\other"));
    EQ1(U(SHRegDeleteEmptyUSKeyW)(h, L"victim", (SHREGDEL_FLAGS)D_HKCU), ERROR_FILE_NOT_FOUND);
    U(SHRegCloseUSKey)(h); rmroot();
}
TEST(gap10_usk_delkey_a_hkcu) {
    if (t_is_wine()) { t_skip("SHRegDelete*USKey are not implemented in the emulator"); return; }
    NEED(SHRegDeleteEmptyUSKeyA); NEED(SHRegCloseUSKey); ROOT();
    mksub("dka\\victim");
    HUSKEY h = mkus("dka"); if (!h) { t_skip("no key"); return; }
    EQ1(U(SHRegDeleteEmptyUSKeyA)(h, "victim", (SHREGDEL_FLAGS)D_HKCU), 0);
    CHECK(!keyexists("dka\\victim"));
    EQ1(U(SHRegDeleteEmptyUSKeyA)(h, "victim", (SHREGDEL_FLAGS)D_HKCU), ERROR_FILE_NOT_FOUND);
    U(SHRegCloseUSKey)(h); rmroot();
}
TEST(gap10_usk_delkey_default_flag) {
    if (t_is_wine()) { t_skip("SHRegDelete*USKey are not implemented in the emulator"); return; }
    NEED(SHRegDeleteEmptyUSKeyW); NEED(SHRegCloseUSKey); ROOT();
    mksub("dkd\\victim");
    HUSKEY h = mkus("dkd"); if (!h) { t_skip("no key"); return; }
    EQ1(U(SHRegDeleteEmptyUSKeyW)(h, L"victim", (SHREGDEL_FLAGS)0), 0);
    CHECK(!keyexists("dkd\\victim"));
    U(SHRegCloseUSKey)(h); rmroot();
}
TEST(gap10_usk_delkey_with_values_is_deleted) {
    if (t_is_wine()) { t_skip("SHRegDelete*USKey are not implemented in the emulator"); return; }
    NEED(SHRegDeleteEmptyUSKeyW); NEED(SHRegCloseUSKey); ROOT();
    setsz("dkv\\withval", "v", "x");
    HUSKEY h = mkus("dkv"); if (!h) { t_skip("no key"); return; }
    SET_ERR(0);
    LONG r = U(SHRegDeleteEmptyUSKeyW)(h, L"withval", (SHREGDEL_FLAGS)D_HKCU);
    t_note("SHRegDeleteEmptyUSKeyW(key with values only) -> %ld, exists=%d", r, keyexists("dkv\\withval"));
    U(SHRegCloseUSKey)(h); rmroot();
}
TEST(gap10_usk_delkey_with_subkeys_fails) {
    if (t_is_wine()) { t_skip("SHRegDelete*USKey are not implemented in the emulator"); return; }
    NEED(SHRegDeleteEmptyUSKeyW); NEED(SHRegCloseUSKey); ROOT();
    mksub("dks\\parent\\child");
    HUSKEY h = mkus("dks"); if (!h) { t_skip("no key"); return; }
    LONG r = U(SHRegDeleteEmptyUSKeyW)(h, L"parent", (SHREGDEL_FLAGS)D_HKCU);
    CHECK(r != 0);
    t_note("SHRegDeleteEmptyUSKeyW(key with subkeys) -> %ld", r);
    CHECK(keyexists("dks\\parent")); CHECK(keyexists("dks\\parent\\child"));
    U(SHRegCloseUSKey)(h); rmroot();
}
TEST(gap10_usk_delkey_invalid_args) {
    if (t_is_wine()) { t_skip("SHRegDelete*USKey are not implemented in the emulator"); return; }
    NEED(SHRegDeleteEmptyUSKeyW); NEED(SHRegDeleteEmptyUSKeyA); NEED(SHRegCloseUSKey); ROOT();
    mksub("dki\\victim");
    HUSKEY h = mkus("dki"); if (!h) { t_skip("no key"); return; }
    static const DWORD bad[] = { 4, 8, 0x20, 0x80000000 };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        EQ1(U(SHRegDeleteEmptyUSKeyW)(h, L"victim", (SHREGDEL_FLAGS)bad[i]), ERROR_INVALID_PARAMETER);
        EQ1(U(SHRegDeleteEmptyUSKeyA)(h, "victim", (SHREGDEL_FLAGS)bad[i]), ERROR_INVALID_PARAMETER);
    }
    CHECK(keyexists("dki\\victim"));
    EQ1(U(SHRegDeleteEmptyUSKeyW)(NULL, L"victim", (SHREGDEL_FLAGS)D_HKCU), ERROR_INVALID_PARAMETER);
    EQ1(U(SHRegDeleteEmptyUSKeyA)(NULL, "victim", (SHREGDEL_FLAGS)0), ERROR_INVALID_PARAMETER);
    U(SHRegCloseUSKey)(h); rmroot();
}
TEST(gap10_usk_delkey_a_overlong_name) {
    if (t_is_wine()) { t_skip("SHRegDelete*USKey are not implemented in the emulator"); return; }
    NEED(SHRegDeleteEmptyUSKeyA); NEED(SHRegCloseUSKey); ROOT();
    HUSKEY h = mkus("dko"); if (!h) { t_skip("no key"); return; }
    char nm[300]; memset(nm, 'z', 299); nm[299] = 0;
    SET_ERR(0);
    EQ1(U(SHRegDeleteEmptyUSKeyA)(h, nm, (SHREGDEL_FLAGS)D_HKCU), ERROR_INSUFFICIENT_BUFFER);
    U(SHRegCloseUSKey)(h); rmroot();
}
TEST(gap10_usk_delkey_hklm_flag_leaves_hkcu) {
    if (t_is_wine()) { t_skip("SHRegDelete*USKey are not implemented in the emulator"); return; }
    NEED(SHRegDeleteEmptyUSKeyW); NEED(SHRegCloseUSKey); ROOT();
    mksub("dkh\\victim");
    HUSKEY h = mkus("dkh"); if (!h) { t_skip("no key"); return; }
    EQ1(U(SHRegDeleteEmptyUSKeyW)(h, L"victim", (SHREGDEL_FLAGS)D_HKLM), ERROR_FILE_NOT_FOUND);
    CHECK(keyexists("dkh\\victim"));
    U(SHRegCloseUSKey)(h); rmroot();
}

/* ---------------- SHLoadIndirectString ---------------- */

TEST(gap10_shlis_plain_string) {
    NEED(SHLoadIndirectString);
    WCHAR b[32]; for (int i = 0; i < 32; i++) b[i] = 0x7777;
    HRESULT hr = U(SHLoadIndirectString)(L"plain text", b, 32, NULL);
    EQ1(hr, S_OK); EXPECT_WSTR(b, L"plain text");
}
TEST(gap10_shlis_empty_string) {
    NEED(SHLoadIndirectString);
    WCHAR b[8] = { 0x7777, 0x7777 };
    EQ1(U(SHLoadIndirectString)(L"", b, 8, NULL), S_OK);
    EQ1(b[0], 0);
}
TEST(gap10_shlis_exact_fit) {
    NEED(SHLoadIndirectString);
    WCHAR b[16]; for (int i = 0; i < 16; i++) b[i] = 0x7777;
    EQ1(U(SHLoadIndirectString)(L"12345", b, 6, NULL), S_OK);
    EXPECT_WSTR(b, L"12345"); EQ1(b[6], 0x7777);
}
TEST(gap10_shlis_truncation) {
    NEED(SHLoadIndirectString);
    WCHAR b[16]; for (int i = 0; i < 16; i++) b[i] = 0x7777;
    HRESULT hr = U(SHLoadIndirectString)(L"12345", b, 3, NULL);
    t_note("SHLoadIndirectString(truncate to 3) hr=0x%08lx buf=\"%ls\"", (unsigned long)hr, b);
    EQ1(b[3], 0x7777);
    CHECK(b[2] == 0 || b[2] == 0x7777 || b[2] == L'3');
}
TEST(gap10_shlis_zero_size_plain) {
    NEED(SHLoadIndirectString);
    WCHAR b[4] = { 0x7777, 0x7777, 0x7777, 0x7777 };
    HRESULT hr = U(SHLoadIndirectString)(L"abc", b, 0, NULL);
    t_note("SHLoadIndirectString(cch 0) hr=0x%08lx", (unsigned long)hr);
    EQ1(b[0], 0x7777);
}
TEST(gap10_shlis_in_place) {
    NEED(SHLoadIndirectString);
    WCHAR b[16] = L"inplace";
    EQ1(U(SHLoadIndirectString)(b, b, 16, NULL), S_OK);
    EXPECT_WSTR(b, L"inplace");
}
TEST(gap10_shlis_null_source) {
    NEED(SHLoadIndirectString);
    if (t_is_wine()) { t_skip("emulator faults on a NULL source"); return; }
    WCHAR b[4] = { 0x7777 };
    EQ1(U(SHLoadIndirectString)(NULL, b, 4, NULL), E_INVALIDARG);
    EQ1(b[0], 0x7777);
}
TEST(gap10_shlis_reserved_pointer_zeroed) {
    NEED(SHLoadIndirectString);
    WCHAR b[16]; void *rsv = (void *)(ULONG_PTR)0x1234;
    EQ1(U(SHLoadIndirectString)(L"abc", b, 16, &rsv), S_OK);
    EQ1(rsv, 0x1234);   /* Win10: the reserved argument is ignored (SHLoadIndirectString passes NULL on), left untouched */
}
TEST(gap10_shlis_indirect_bad_dll) {
    NEED(SHLoadIndirectString);
    WCHAR b[32]; for (int i = 0; i < 32; i++) b[i] = 0x7777;
    HRESULT hr = U(SHLoadIndirectString)(L"@k32t_no_such_dll_xyz.dll,-1", b, 32, NULL);
    t_note("SHLoadIndirectString(missing dll) hr=0x%08lx", (unsigned long)hr);
    CHECK(FAILED(hr));
    if (FAILED(hr)) EQ1(b[0], 0);
}
TEST(gap10_shlis_indirect_bad_id) {
    NEED(SHLoadIndirectString);
    WCHAR b[32]; for (int i = 0; i < 32; i++) b[i] = 0x7777;
    HRESULT hr = U(SHLoadIndirectString)(L"@%SystemRoot%\\System32\\user32.dll,-31999", b, 32, NULL);
    t_note("SHLoadIndirectString(missing id) hr=0x%08lx", (unsigned long)hr);
    CHECK(FAILED(hr));
    if (FAILED(hr)) EQ1(b[0], 0);
}
TEST(gap10_shlis_indirect_resource) {
    NEED(SHLoadIndirectString);
    HMODULE m = LoadLibraryExW(L"user32.dll", NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (!m) { t_skip("no user32"); return; }
    WCHAR direct[256]; int id = 0;
    for (int i = 1; i < 4000; i++) if (LoadStringW(m, i, direct, 256) > 3) { id = i; break; }
    FreeLibrary(m);
    if (!id) { t_skip("no user32 string found"); return; }
    WCHAR src[128], b[256];
    swprintf(src, 128, L"@%%SystemRoot%%\\System32\\user32.dll,-%d", id);
    HRESULT hr = U(SHLoadIndirectString)(src, b, 256, NULL);
    t_note("user32 string %d: hr=0x%08lx \"%ls\" (direct \"%ls\")", id, (unsigned long)hr, b, direct);
    EQ1(hr, S_OK);
    CHECK(b[0] != 0);
}
TEST(gap10_shlis_indirect_resource_truncated) {
    NEED(SHLoadIndirectString);
    HMODULE m = LoadLibraryExW(L"user32.dll", NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (!m) { t_skip("no user32"); return; }
    WCHAR direct[256]; int id = 0;
    for (int i = 1; i < 4000; i++) if (LoadStringW(m, i, direct, 256) > 3) { id = i; break; }
    FreeLibrary(m);
    if (!id) { t_skip("no user32 string found"); return; }
    WCHAR src[128], b[8];
    swprintf(src, 128, L"@%%SystemRoot%%\\System32\\user32.dll,-%d", id);
    HRESULT hr = U(SHLoadIndirectString)(src, b, 3, NULL);
    t_note("truncated indirect: hr=0x%08lx", (unsigned long)hr);
}

/* ---------------- SHCoCreateInstance ---------------- */

UDECL(HRESULT, SHCoCreateInstance, (PCWSTR, const CLSID *, IUnknown *, REFIID, void **));
static const GUID CLSID_ShellLink_ = { 0x00021401, 0, 0, { 0xC0, 0, 0, 0, 0, 0, 0, 0x46 } };
static const GUID IID_IUnknown_ = { 0, 0, 0, { 0xC0, 0, 0, 0, 0, 0, 0, 0x46 } };
static const GUID IID_IShellLinkW_ = { 0x000214F9, 0, 0, { 0xC0, 0, 0, 0, 0, 0, 0, 0x46 } };
static const GUID CLSID_Bogus_ = { 0x6b32c0de, 0x1234, 0x4321, { 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78 } };
typedef HRESULT (WINAPI *CoInit_t)(LPVOID, DWORD);
typedef void (WINAPI *CoUninit_t)(void);
static ULONG rel(void *p) { return ((IUnknown *)p)->lpVtbl->Release((IUnknown *)p); }

static HRESULT g_hr[8]; static void *g_pp[8]; static int g_init;
static volatile int g_fault[8]; static volatile DWORD g_guard_tid;
/* GUARD: SHCoCreateInstance with invalid/NULL ids may fault inside shell32 on Windows; the faulting
 * call runs in its own thread and the handler ends only that thread. */
static LONG CALLBACK shco_veh(PEXCEPTION_POINTERS ep) {
    if (g_guard_tid && g_guard_tid == GetCurrentThreadId() &&
        ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        g_guard_tid = 0;
        ExitThread(0xDEAD0000);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
static DWORD WINAPI shco_thread(LPVOID arg) {
    HMODULE ole = LoadLibraryA("ole32.dll");
    CoInit_t ci = ole ? (CoInit_t)GetProcAddress(ole, "CoInitializeEx") : NULL;
    CoUninit_t cu = ole ? (CoUninit_t)GetProcAddress(ole, "CoUninitialize") : NULL;
    int mode = (int)(ULONG_PTR)arg;
    g_init = 0;
    if (mode != 2 && ci) g_init = SUCCEEDED(ci(NULL, COINIT_APARTMENTTHREADED));
    void *p = NULL;
    if (mode == 0) {
        g_hr[0] = UN(SHCoCreateInstance)(NULL, &CLSID_ShellLink_, NULL, &IID_IUnknown_, &p); g_pp[0] = p; p = NULL;
        g_hr[1] = UN(SHCoCreateInstance)(L"{00021401-0000-0000-C000-000000000046}", NULL, NULL, &IID_IShellLinkW_, &p); g_pp[1] = p; p = NULL;
        g_hr[2] = UN(SHCoCreateInstance)(NULL, &CLSID_Bogus_, NULL, &IID_IUnknown_, &p); g_pp[2] = p; p = (void *)(ULONG_PTR)0;
    } else if (mode >= 13 && mode <= 15) {
        int i = mode - 10;
        g_hr[i] = E_FAIL; g_fault[i] = 1; /* stays 1 if the guard kills this thread */
        g_guard_tid = GetCurrentThreadId();
        if (i == 3) g_hr[i] = UN(SHCoCreateInstance)(L"not a guid", NULL, NULL, &IID_IUnknown_, &p);
        else if (i == 4) g_hr[i] = UN(SHCoCreateInstance)(NULL, NULL, NULL, &IID_IUnknown_, &p);
        else g_hr[i] = UN(SHCoCreateInstance)(L"{00021401-0000-0000-C000-000000000046}", &CLSID_Bogus_, NULL, &IID_IUnknown_, &p);
        g_guard_tid = 0; g_fault[i] = 0; g_pp[i] = p;   /* clear the guard: a later thread may reuse this id */
    } else if (mode == 2) {
        g_hr[0] = UN(SHCoCreateInstance)(NULL, &CLSID_ShellLink_, NULL, &IID_IUnknown_, &p); g_pp[0] = p;
    }
    if (g_init && cu) cu();
    return 0;
}
static void run_shco(int mode) {
    static PVOID veh;
    if (!veh) veh = AddVectoredExceptionHandler(1, shco_veh);
    HANDLE t = CreateThread(NULL, 0, shco_thread, (LPVOID)(ULONG_PTR)mode, 0, NULL);
    WaitForSingleObject(t, 20000); CloseHandle(t);
}
TEST(gap10_shco_shelllink_by_clsid) {
    REQUIRE(UN(SHCoCreateInstance));
    if (t_is_wine()) { t_skip("shell COM activation not exercised under emulator"); return; }
    run_shco(0);
    if (!g_init) { t_skip("COM init failed"); return; }
    t_note("SHCoCreateInstance(ShellLink) hr=0x%08lx", (unsigned long)g_hr[0]);
    EQ1(g_hr[0], S_OK);
    CHECK(g_pp[0] != NULL);
    if (g_pp[0]) EQ1(rel(g_pp[0]), 0);
}
TEST(gap10_shco_shelllink_by_string) {
    REQUIRE(UN(SHCoCreateInstance));
    if (t_is_wine()) { t_skip("shell COM activation not exercised under emulator"); return; }
    run_shco(0);
    if (!g_init) { t_skip("COM init failed"); return; }
    EQ1(g_hr[1], S_OK);
    CHECK(g_pp[1] != NULL);
    if (g_pp[1]) EQ1(rel(g_pp[1]), 0);
}
TEST(gap10_shco_unregistered_class) {
    REQUIRE(UN(SHCoCreateInstance));
    if (t_is_wine()) { t_skip("shell COM activation not exercised under emulator"); return; }
    run_shco(0);
    if (!g_init) { t_skip("COM init failed"); return; }
    t_note("unregistered class hr=0x%08lx ppv=%p", (unsigned long)g_hr[2], g_pp[2]);
    CHECK(FAILED(g_hr[2]));
    CHECK(g_pp[2] == NULL);
}
TEST(gap10_shco_bad_string_and_null_ids) {
    REQUIRE(UN(SHCoCreateInstance));
    if (t_is_wine()) { t_skip("shell COM activation not exercised under emulator"); return; }
    run_shco(13); if (!g_init) { t_skip("COM init failed"); return; }
    run_shco(14); run_shco(15);
    t_note("faulted: bad string=%d both NULL=%d both given=%d", g_fault[3], g_fault[4], g_fault[5]);
    t_note("bad string hr=0x%08lx; both NULL hr=0x%08lx; both given hr=0x%08lx", (unsigned long)g_hr[3], (unsigned long)g_hr[4], (unsigned long)g_hr[5]);
    /* P1 audit: windows.storage!SHCoCreateInstanceWorker uses pclsid when pszCLSID is NULL and
     * _SHCoCreateInstance first calls shlwapi!SHStringFromGUIDW(pclsid, ...), which reads the GUID bytes
     * without a NULL check: NULL/NULL faults (access violation) on Win10. A bad string is rejected by
     * SHCLSIDFromString before that point and must not fault; neither may the both-given call. */
    CHECK(!g_fault[3]); CHECK(FAILED(g_hr[3]));
    CHECK(g_fault[4]);
    CHECK(!g_fault[5]);
    if (!g_fault[3] && g_pp[3]) rel(g_pp[3]);
    if (!g_fault[4] && g_pp[4]) rel(g_pp[4]);
    if (!g_fault[5] && g_hr[5] == S_OK && g_pp[5]) rel(g_pp[5]);
}
TEST(gap10_shco_without_com_init) {
    REQUIRE(UN(SHCoCreateInstance));
    if (t_is_wine()) { t_skip("shell COM activation not exercised under emulator"); return; }
    run_shco(2);
    t_note("SHCoCreateInstance without CoInitialize hr=0x%08lx", (unsigned long)g_hr[0]);
    if (SUCCEEDED(g_hr[0]) && g_pp[0]) rel(g_pp[0]);
}
