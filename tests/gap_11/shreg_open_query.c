/* gap_11: SHRegOpenUSKey / SHRegQueryUSValue / SHRegGetUSValue (A and W), private HKCU key only. */
#include "g11.h"

#define OPENA U(SHRegOpenUSKeyA)
#define OPENW U(SHRegOpenUSKeyW)
#define QA U(SHRegQueryUSValueA)
#define QW U(SHRegQueryUSValueW)
#define GA U(SHRegGetUSValueA)
#define GW U(SHRegGetUSValueW)

static HUSKEY open_a(const char *leaf, REGSAM sam) {
    char p[256]; HUSKEY h = NULL;
    g11_sub(p, sizeof p, leaf);
    LONG r = OPENA(p, sam, NULL, &h, FALSE);
    return r == 0 ? h : NULL;
}
static HUSKEY open_w(const char *leaf, REGSAM sam) {
    WCHAR p[256]; HUSKEY h = NULL;
    g11_subw(p, 256, leaf);
    LONG r = OPENW(p, sam, NULL, &h, FALSE);
    return r == 0 ? h : NULL;
}

/* ---------------- open / close ---------------- */
TEST(gap11_shreg_open_w_basic) {
    REQUIRE(OPENW); REQUIRE(pClose);
    g11_clean(); g11_mkdw("a", "v", 1);
    WCHAR p[256]; HUSKEY h = (HUSKEY)(ULONG_PTR)0x1234;
    g11_subw(p, 256, "a");
    EQ(OPENW(p, KEY_READ, NULL, &h, FALSE), 0);
    NE(h, NULL); NE(h, (HUSKEY)(ULONG_PTR)0x1234);
    if (h) EQ(pClose(h), 0);
    g11_clean();
}
TEST(gap11_shreg_open_a_basic) {
    REQUIRE(OPENA); REQUIRE(pClose);
    g11_clean(); g11_mkdw("a", "v", 1);
    char p[256]; HUSKEY h = (HUSKEY)(ULONG_PTR)0x1234;
    g11_sub(p, sizeof p, "a");
    EQ(OPENA(p, KEY_READ, NULL, &h, FALSE), 0);
    NE(h, NULL); NE(h, (HUSKEY)(ULONG_PTR)0x1234);
    if (h) EQ(pClose(h), 0);
    g11_clean();
}
TEST(gap11_shreg_open_w_missing) {
    REQUIRE(OPENW);
    g11_clean();
    WCHAR p[256]; HUSKEY h = (HUSKEY)(ULONG_PTR)0x1234;
    g11_subw(p, 256, "nokey");
    EQ(OPENW(p, KEY_READ, NULL, &h, FALSE), ERROR_FILE_NOT_FOUND);
    EQ(h, NULL);     /* output is zeroed before the open is attempted */
}
TEST(gap11_shreg_open_a_missing) {
    REQUIRE(OPENA);
    g11_clean();
    char p[256]; HUSKEY h = (HUSKEY)(ULONG_PTR)0x1234;
    g11_sub(p, sizeof p, "nokey");
    EQ(OPENA(p, KEY_READ, NULL, &h, FALSE), ERROR_FILE_NOT_FOUND);
    EQ(h, NULL);
}
TEST(gap11_shreg_open_null_phandle) {
    REQUIRE(OPENA); REQUIRE(OPENW);
    g11_clean(); g11_mkdw("a", "v", 1);
    char pa[256]; WCHAR pw[256];
    g11_sub(pa, sizeof pa, "a"); g11_subw(pw, 256, "a");
    EQ(OPENW(pw, KEY_READ, NULL, NULL, FALSE), ERROR_INVALID_PARAMETER);
    EQ(OPENA(pa, KEY_READ, NULL, NULL, FALSE), ERROR_INVALID_PARAMETER);
    EQ(OPENW(pw, KEY_READ, NULL, NULL, TRUE), ERROR_INVALID_PARAMETER);
    g11_clean();
}
TEST(gap11_shreg_open_a_null_path) {
    REQUIRE(OPENA);
    if (t_is_wine()) { t_skip("emulator dereferences a NULL path"); return; }
    HUSKEY h = (HUSKEY)(ULONG_PTR)0x1234;
    /* MultiByteToWideChar(NULL) fails with ERROR_INVALID_PARAMETER, which is returned */
    EQ(OPENA(NULL, KEY_READ, NULL, &h, FALSE), ERROR_INVALID_PARAMETER);
}
TEST(gap11_shreg_open_ignore_hkcu_hkcu_only) {
    REQUIRE(OPENA); REQUIRE(OPENW);
    g11_clean(); g11_mkdw("a", "v", 1);
    char pa[256]; WCHAR pw[256]; HUSKEY h = (HUSKEY)(ULONG_PTR)0x1234;
    g11_sub(pa, sizeof pa, "a"); g11_subw(pw, 256, "a");
    /* fIgnoreHKCU: only HKLM is consulted, where this private key does not exist */
    EQ(OPENA(pa, KEY_READ, NULL, &h, TRUE), ERROR_FILE_NOT_FOUND);
    EQ(h, NULL);
    h = (HUSKEY)(ULONG_PTR)0x1234;
    EQ(OPENW(pw, KEY_READ, NULL, &h, TRUE), ERROR_FILE_NOT_FOUND);
    EQ(h, NULL);
    g11_clean();
}
TEST(gap11_shreg_open_ignore_hkcu_hklm_key) {
    REQUIRE(OPENW); REQUIRE(pClose); REQUIRE(QW);
    HUSKEY h = NULL;
    EQ(OPENW(L"Software\\Microsoft\\Windows NT\\CurrentVersion", KEY_QUERY_VALUE, NULL, &h, TRUE), 0);
    if (!h) return;
    WCHAR buf[128]; DWORD cb = sizeof buf, type = 0;
    EQ(QW(h, L"ProductName", &type, buf, &cb, TRUE, NULL, 0), 0);
    EQ(type, REG_SZ);
    CHECK(buf[0] != 0);
    EQ(pClose(h), 0);
}
TEST(gap11_shreg_open_hklm_fallback_query) {
    REQUIRE(OPENA); REQUIRE(pClose); REQUIRE(QA);
    HUSKEY h = NULL;
    /* HKCU is tried first, then HKLM */
    EQ(OPENA("Software\\Microsoft\\Windows NT\\CurrentVersion", KEY_QUERY_VALUE, NULL, &h, FALSE), 0);
    if (!h) return;
    char buf[128]; DWORD cb = sizeof buf, type = 0;
    EQ(QA(h, "CurrentBuildNumber", &type, buf, &cb, FALSE, NULL, 0), 0);
    EQ(type, REG_SZ);
    CHECK(atoi(buf) >= 10240);
    EQ(cb, strlen(buf) + 1);
    EQ(pClose(h), 0);
}
TEST(gap11_shreg_open_path_length_a) {
    REQUIRE(OPENA);
    char p[400]; HUSKEY h;
    memset(p, 'a', sizeof p); p[255] = 0;
    h = NULL; LONG r = OPENA(p, KEY_READ, NULL, &h, FALSE);
    t_note("A path of 255 chars -> %ld", (long)r);
    NE(r, ERROR_INSUFFICIENT_BUFFER); NE(r, ERROR_BUFFER_OVERFLOW); NE(r, 0);
    p[255] = 'a'; p[256] = 0;           /* 256 chars + NUL = 257 > 256 WCHARs */
    h = NULL; r = OPENA(p, KEY_READ, NULL, &h, FALSE);
    EQ(r, ERROR_INSUFFICIENT_BUFFER);   /* MultiByteToWideChar into 256 WCHARs */
    EQ(h, NULL);
    p[299] = 0; memset(p, 'a', 299);
    r = OPENA(p, KEY_READ, NULL, &h, FALSE);
    EQ(r, ERROR_INSUFFICIENT_BUFFER);
}
TEST(gap11_shreg_open_path_length_w) {
    REQUIRE(OPENW);
    WCHAR p[400]; HUSKEY h;
    for (int i = 0; i < 400; i++) p[i] = L'a';
    p[255] = 0;
    h = NULL; LONG r = OPENW(p, KEY_READ, NULL, &h, FALSE);
    t_note("W path of 255 chars -> %ld", (long)r);
    NE(r, ERROR_BUFFER_OVERFLOW); NE(r, 0);
    p[255] = L'a'; p[256] = 0;
    h = NULL; r = OPENW(p, KEY_READ, NULL, &h, FALSE);
    EQ(r, ERROR_BUFFER_OVERFLOW);        /* StringCchCopyW into 256 WCHARs fails */
    EQ(h, NULL);
    p[256] = L'a'; p[390] = 0;
    r = OPENW(p, KEY_READ, NULL, &h, FALSE);
    EQ(r, ERROR_BUFFER_OVERFLOW);
}
TEST(gap11_shreg_open_relative) {
    REQUIRE(OPENW); REQUIRE(OPENA); REQUIRE(pClose); REQUIRE(QW);
    g11_clean(); g11_mkdw("par\\kid", "v", 42);
    HUSKEY par = open_w("par", KEY_READ), kid = NULL;
    NE(par, NULL); if (!par) return;
    EQ(OPENW(L"kid", KEY_READ, par, &kid, FALSE), 0);
    NE(kid, NULL);
    if (kid) {
        DWORD v = 0, cb = 4, type = 0;
        EQ(QW(kid, L"v", &type, &v, &cb, FALSE, NULL, 0), 0);
        EQ(type, REG_DWORD); EQ(v, 42); EQ(cb, 4);
        EQ(pClose(kid), 0);
    }
    kid = (HUSKEY)(ULONG_PTR)0x1234;
    EQ(OPENW(L"nokid", KEY_READ, par, &kid, FALSE), ERROR_FILE_NOT_FOUND);
    EQ(kid, NULL);
    kid = NULL;
    EQ(OPENA("kid", KEY_READ, par, &kid, FALSE), 0);
    if (kid) EQ(pClose(kid), 0);
    EQ(pClose(par), 0);
    g11_clean();
}
TEST(gap11_shreg_open_relative_ignore_hkcu) {
    REQUIRE(OPENW); REQUIRE(pClose);
    g11_clean(); g11_mkdw("par\\kid", "v", 42);
    HUSKEY par = open_w("par", KEY_READ), kid = (HUSKEY)(ULONG_PTR)0x1234;
    NE(par, NULL); if (!par) return;
    EQ(OPENW(L"kid", KEY_READ, par, &kid, TRUE), ERROR_FILE_NOT_FOUND);
    EQ(kid, NULL);
    EQ(pClose(par), 0);
    g11_clean();
}
TEST(gap11_shreg_open_relative_nested_twice) {
    REQUIRE(OPENW); REQUIRE(pClose); REQUIRE(GW);
    g11_clean(); g11_mkdw("p\\q\\r", "v", 7);
    HUSKEY p = open_w("p", KEY_READ), q = NULL, r = NULL;
    NE(p, NULL); if (!p) return;
    EQ(OPENW(L"q", KEY_READ, p, &q, FALSE), 0);
    if (q) EQ(OPENW(L"r", KEY_READ, q, &r, FALSE), 0);
    if (r) {
        DWORD v = 0, cb = 4;
        EQ(U(SHRegQueryUSValueW)(r, L"v", NULL, &v, &cb, FALSE, NULL, 0), 0);
        EQ(v, 7);
        pClose(r);
    }
    if (q) pClose(q);
    pClose(p);
    g11_clean();
}
TEST(gap11_shreg_open_sam_variants) {
    REQUIRE(OPENW); REQUIRE(pClose);
    g11_clean(); g11_mkdw("a", "v", 1);
    REGSAM sams[] = { KEY_READ, KEY_QUERY_VALUE, KEY_ENUMERATE_SUB_KEYS, KEY_QUERY_VALUE | KEY_SET_VALUE, KEY_ALL_ACCESS, KEY_WOW64_64KEY | KEY_READ };
    for (size_t i = 0; i < sizeof sams / sizeof sams[0]; i++) {
        WCHAR p[256]; HUSKEY h = NULL; g11_subw(p, 256, "a");
        LONG r = OPENW(p, sams[i], NULL, &h, FALSE);
        CHECKF(r == 0, "sam 0x%lx -> %ld", (unsigned long)sams[i], (long)r);
        if (h) pClose(h);
    }
    g11_clean();
}
TEST(gap11_shreg_close_null) {
    REQUIRE(pClose);
    EQ(pClose(NULL), ERROR_INVALID_PARAMETER);
}
TEST(gap11_shreg_open_repeated) {
    REQUIRE(OPENW); REQUIRE(pClose);
    g11_clean(); g11_mkdw("a", "v", 1);
    HUSKEY hs[20];
    for (int i = 0; i < 20; i++) { hs[i] = open_w("a", KEY_READ); NE(hs[i], NULL); }
    for (int i = 0; i < 20; i++) for (int j = i + 1; j < 20; j++) if (hs[i] && hs[i] == hs[j]) CHECK(0);
    for (int i = 0; i < 20; i++) if (hs[i]) EQ(pClose(hs[i]), 0);
    g11_clean();
}

/* ---------------- SHRegQueryUSValue ---------------- */
TEST(gap11_shreg_query_dword_w) {
    REQUIRE(QW); REQUIRE(pClose);
    g11_clean(); g11_mkdw("a", "d", 0xA5A5A5A5);
    HUSKEY h = open_w("a", KEY_READ); NE(h, NULL); if (!h) return;
    DWORD v = 0, cb = 4, type = 99;
    EQ(QW(h, L"d", &type, &v, &cb, FALSE, NULL, 0), 0);
    EQ(type, REG_DWORD); EQ(v, 0xA5A5A5A5); EQ(cb, 4);
    pClose(h); g11_clean();
}
TEST(gap11_shreg_query_dword_a) {
    REQUIRE(QA); REQUIRE(pClose);
    g11_clean(); g11_mkdw("a", "d", 0x01020304);
    HUSKEY h = open_a("a", KEY_READ); NE(h, NULL); if (!h) return;
    DWORD v = 0, cb = 4, type = 99;
    EQ(QA(h, "d", &type, &v, &cb, FALSE, NULL, 0), 0);
    EQ(type, REG_DWORD); EQ(v, 0x01020304); EQ(cb, 4);
    pClose(h); g11_clean();
}
TEST(gap11_shreg_query_sz_parity) {
    REQUIRE(QA); REQUIRE(QW); REQUIRE(pClose);
    g11_clean(); g11_mksz("a", "s", "hello world");
    HUSKEY h = open_a("a", KEY_READ); NE(h, NULL); if (!h) return;
    char ba[64]; WCHAR bw[64]; DWORD cb = sizeof ba, type = 0;
    EQ(QA(h, "s", &type, ba, &cb, FALSE, NULL, 0), 0);
    EQ(type, REG_SZ); EQ(cb, 12); EXPECT_STR(ba, "hello world");
    cb = sizeof bw; type = 0;
    EQ(QW(h, L"s", &type, bw, &cb, FALSE, NULL, 0), 0);
    EQ(type, REG_SZ); EQ(cb, 24); EXPECT_WSTR(bw, L"hello world");
    pClose(h); g11_clean();
}
TEST(gap11_shreg_query_binary) {
    REQUIRE(QW); REQUIRE(pClose);
    g11_clean();
    BYTE data[9] = { 0, 1, 2, 3, 0xFF, 0xFE, 0, 0, 7 };
    g11_mk("a", "b", REG_BINARY, data, sizeof data);
    HUSKEY h = open_w("a", KEY_READ); NE(h, NULL); if (!h) return;
    BYTE out[32]; DWORD cb = sizeof out, type = 0;
    EQ(QW(h, L"b", &type, out, &cb, FALSE, NULL, 0), 0);
    EQ(type, REG_BINARY); EQ(cb, 9); CHECK(!memcmp(out, data, 9));
    pClose(h); g11_clean();
}
TEST(gap11_shreg_query_multisz_qword_none) {
    REQUIRE(QW); REQUIRE(pClose);
    g11_clean();
    WCHAR ms[] = L"one\0two\0";
    g11_mk("a", "m", REG_MULTI_SZ, ms, sizeof ms);
    unsigned long long q = 0x1122334455667788ULL;
    g11_mk("a", "q", REG_QWORD, &q, 8);
    g11_mk("a", "n", REG_NONE, "xy", 2);
    HUSKEY h = open_w("a", KEY_READ); NE(h, NULL); if (!h) return;
    BYTE out[64]; DWORD cb = sizeof out, type = 0;
    EQ(QW(h, L"m", &type, out, &cb, FALSE, NULL, 0), 0);
    EQ(type, REG_MULTI_SZ); EQ(cb, sizeof ms); CHECK(!memcmp(out, ms, sizeof ms));
    unsigned long long qo = 0; cb = 8; type = 0;
    EQ(QW(h, L"q", &type, &qo, &cb, FALSE, NULL, 0), 0);
    EQ(type, REG_QWORD); CHECK(qo == q);
    cb = sizeof out; type = 9;
    EQ(QW(h, L"n", &type, out, &cb, FALSE, NULL, 0), 0);
    EQ(type, REG_NONE); EQ(cb, 2);
    pClose(h); g11_clean();
}
TEST(gap11_shreg_query_expand_sz_is_expanded) {
    REQUIRE(QW); REQUIRE(pClose);
    g11_clean();
    g11_mk("a", "e", REG_EXPAND_SZ, L"%SystemRoot%\\x", 30);
    HUSKEY h = open_w("a", KEY_READ); NE(h, NULL); if (!h) return;
    WCHAR out[MAX_PATH], want[MAX_PATH]; DWORD cb = sizeof out, type = 0;
    EQ(QW(h, L"e", &type, out, &cb, FALSE, NULL, 0), 0);
    /* no RRF_NOEXPAND: the value is expanded and reported as REG_SZ */
    EQ(type, REG_SZ);
    DWORD n = GetEnvironmentVariableW(L"SystemRoot", want, MAX_PATH);
    if (n) { lstrcatW(want, L"\\x"); EXPECT_WSTR(out, want); }
    pClose(h); g11_clean();
}
TEST(gap11_shreg_query_default_value_name) {
    REQUIRE(QW); REQUIRE(QA); REQUIRE(pClose);
    g11_clean();
    g11_mksz("a", "", "dflt");
    HUSKEY h = open_w("a", KEY_READ); NE(h, NULL); if (!h) return;
    WCHAR bw[16]; char ba[16]; DWORD cb = sizeof bw, type = 0;
    EQ(QW(h, L"", &type, bw, &cb, FALSE, NULL, 0), 0);
    EXPECT_WSTR(bw, L"dflt");
    cb = sizeof ba;
    EQ(QA(h, "", &type, ba, &cb, FALSE, NULL, 0), 0);
    EXPECT_STR(ba, "dflt");
    cb = sizeof bw;
    EQ(QW(h, NULL, &type, bw, &cb, FALSE, NULL, 0), 0);   /* NULL name: the unnamed value */
    EXPECT_WSTR(bw, L"dflt");
    pClose(h); g11_clean();
}
TEST(gap11_shreg_query_missing_value) {
    REQUIRE(QW); REQUIRE(QA); REQUIRE(pClose);
    g11_clean(); g11_mkdw("a", "d", 1);
    HUSKEY h = open_w("a", KEY_READ); NE(h, NULL); if (!h) return;
    DWORD v = 0x77, cb = 4, type = 0;
    EQ(QW(h, L"nope", &type, &v, &cb, FALSE, NULL, 0), ERROR_FILE_NOT_FOUND);
    EQ(v, 0x77);
    EQ(QA(h, "nope", &type, &v, &cb, FALSE, NULL, 0), ERROR_FILE_NOT_FOUND);
    EQ(v, 0x77);
    pClose(h); g11_clean();
}
TEST(gap11_shreg_query_default_applied) {
    REQUIRE(QW); REQUIRE(QA); REQUIRE(pClose);
    g11_clean(); g11_mkdw("a", "d", 1);
    HUSKEY h = open_w("a", KEY_READ); NE(h, NULL); if (!h) return;
    DWORD def = 0xCAFEF00D, v = 0, cb = 4, type = REG_DWORD;
    EQ(QW(h, L"nope", &type, &v, &cb, FALSE, &def, 4), 0);
    EQ(v, 0xCAFEF00D); EQ(cb, 4); EQ(type, REG_DWORD);
    v = 0; cb = 4; type = REG_DWORD;
    EQ(QA(h, "nope", &type, &v, &cb, FALSE, &def, 4), 0);
    EQ(v, 0xCAFEF00D); EQ(cb, 4);
    /* the type reported with a default is the type the caller passed in */
    v = 0; cb = 4; type = REG_BINARY;
    EQ(QW(h, L"nope", &type, &v, &cb, FALSE, &def, 4), 0);
    EQ(type, REG_BINARY);
    pClose(h); g11_clean();
}
TEST(gap11_shreg_query_default_smaller_than_buffer) {
    REQUIRE(QW); REQUIRE(pClose);
    g11_clean(); g11_mkdw("a", "d", 1);
    HUSKEY h = open_w("a", KEY_READ); NE(h, NULL); if (!h) return;
    char out[32]; memset(out, 'x', sizeof out);
    DWORD cb = sizeof out, type = REG_BINARY;
    EQ(QW(h, L"nope", &type, out, &cb, FALSE, "abc", 3), 0);
    EQ(cb, 3); CHECK(!memcmp(out, "abc", 3)); EQ(out[3], 'x');
    pClose(h); g11_clean();
}
TEST(gap11_shreg_query_default_too_big_for_buffer) {
    REQUIRE(QW); REQUIRE(pClose);
    g11_clean(); g11_mkdw("a", "d", 1);
    HUSKEY h = open_w("a", KEY_READ); NE(h, NULL); if (!h) return;
    char out[4] = { 1, 1, 1, 1 };
    DWORD cb = 2, type = REG_BINARY;
    /* default (4 bytes) does not fit into the 2-byte buffer: the lookup error is returned */
    EQ(QW(h, L"nope", &type, out, &cb, FALSE, "abcd", 4), ERROR_FILE_NOT_FOUND);
    EQ(out[0], 1);
    pClose(h); g11_clean();
}
TEST(gap11_shreg_query_default_not_used_when_unusable) {
    REQUIRE(QW); REQUIRE(pClose);
    g11_clean(); g11_mkdw("a", "d", 1);
    HUSKEY h = open_w("a", KEY_READ); NE(h, NULL); if (!h) return;
    char out[8]; DWORD cb = 8, type = 0;
    EQ(QW(h, L"nope", &type, out, &cb, FALSE, NULL, 4), ERROR_FILE_NOT_FOUND);   /* NULL default */
    cb = 8;
    EQ(QW(h, L"nope", &type, out, &cb, FALSE, "abcd", 0), ERROR_FILE_NOT_FOUND); /* zero size default */
    cb = 8;
    EQ(QW(h, L"nope", &type, NULL, &cb, FALSE, "abcd", 4), ERROR_FILE_NOT_FOUND);/* NULL output */
    if (!t_is_wine()) EQ(QW(h, L"nope", &type, out, NULL, FALSE, "abcd", 4), ERROR_FILE_NOT_FOUND);/* NULL size: capacity 0 */
    pClose(h); g11_clean();
}
TEST(gap11_shreg_query_size_only) {
    REQUIRE(QW); REQUIRE(QA); REQUIRE(pClose);
    g11_clean(); g11_mksz("a", "s", "twelve chars");
    HUSKEY h = open_w("a", KEY_READ); NE(h, NULL); if (!h) return;
    DWORD cb = 0, type = 0;
    EQ(QW(h, L"s", &type, NULL, &cb, FALSE, NULL, 0), 0);
    /* size-only query of a REG_SZ: KERNELBASE RegGetValueW adds one WCHAR (2 bytes) to the size
     * RegQueryValueExW reports when pvData is NULL (12 chars + NUL = 26 bytes -> 28). */
    EQ(type, REG_SZ); EQ(cb, 28);
    cb = 0;
    EQ(QA(h, "s", &type, NULL, &cb, FALSE, NULL, 0), 0);
    t_note("SHRegQueryUSValueA size-only for 12-char REG_SZ -> cb=%lu", cb);   /* ANSI size estimate not verified */
    pClose(h); g11_clean();
}
TEST(gap11_shreg_query_exists_only) {
    REQUIRE(QW); REQUIRE(pClose);
    g11_clean(); g11_mkdw("a", "d", 1);
    HUSKEY h = open_w("a", KEY_READ); NE(h, NULL); if (!h) return;
    EQ(QW(h, L"d", NULL, NULL, NULL, FALSE, NULL, 0), 0);
    EQ(QW(h, L"zz", NULL, NULL, NULL, FALSE, NULL, 0), ERROR_FILE_NOT_FOUND);
    pClose(h); g11_clean();
}
TEST(gap11_shreg_query_buffer_too_small) {
    REQUIRE(QW); REQUIRE(pClose);
    g11_clean(); g11_mksz("a", "s", "0123456789");
    HUSKEY h = open_w("a", KEY_READ); NE(h, NULL); if (!h) return;
    WCHAR out[4]; DWORD cb = sizeof out, type = 0;
    LONG r = QW(h, L"s", &type, out, &cb, FALSE, NULL, 0);
    /* RegGetValue reports ERROR_MORE_DATA for the HKCU key, but the lookup then falls through to the
     * HKLM component, which fails for this private path: that error is what comes back. */
    t_note("short buffer -> %ld cb=%lu", (long)r, cb);
    NE(r, 0);
    cb = sizeof out;
    /* a default that fits the caller's capacity (8 bytes) papers over the failure */
    EQ(QW(h, L"s", &type, out, &cb, FALSE, "ab", 2), 0);
    EQ(cb, 2);
    pClose(h); g11_clean();
}
TEST(gap11_shreg_query_exact_buffer) {
    REQUIRE(QW); REQUIRE(pClose);
    g11_clean(); g11_mksz("a", "s", "0123456789");
    HUSKEY h = open_w("a", KEY_READ); NE(h, NULL); if (!h) return;
    WCHAR out[11]; DWORD cb = sizeof out, type = 0;
    EQ(QW(h, L"s", &type, out, &cb, FALSE, NULL, 0), 0);
    EQ(cb, 22); EXPECT_WSTR(out, L"0123456789");
    WCHAR out2[10]; cb = sizeof out2;
    NE(QW(h, L"s", &type, out2, &cb, FALSE, NULL, 0), 0);
    pClose(h); g11_clean();
}
TEST(gap11_shreg_query_null_hkey) {
    REQUIRE(QW); REQUIRE(QA);
    if (t_is_wine()) { t_skip("emulator dereferences a NULL USKEY"); return; }
    DWORD v, cb = 4, type;
    EQ(QW(NULL, L"x", &type, &v, &cb, FALSE, NULL, 0), ERROR_INVALID_PARAMETER);
    EQ(QA(NULL, "x", &type, &v, &cb, FALSE, NULL, 0), ERROR_INVALID_PARAMETER);
    EQ(QW(NULL, L"x", &type, &v, &cb, TRUE, NULL, 0), ERROR_INVALID_PARAMETER);
    cb = 4; DWORD def = 5;
    /* a default applies even when the key handle is invalid */
    EQ(QW(NULL, L"x", &type, &v, &cb, FALSE, &def, 4), 0);
    EQ(v, 5);
}
TEST(gap11_shreg_query_ignore_hkcu) {
    REQUIRE(QW); REQUIRE(QA); REQUIRE(pClose);
    g11_clean(); g11_mkdw("a", "d", 1);
    HUSKEY h = open_w("a", KEY_READ); NE(h, NULL); if (!h) return;
    DWORD v = 0, cb = 4, type = 0;
    EQ(QW(h, L"d", &type, &v, &cb, TRUE, NULL, 0), ERROR_FILE_NOT_FOUND);
    EQ(QA(h, "d", &type, &v, &cb, TRUE, NULL, 0), ERROR_FILE_NOT_FOUND);
    DWORD def = 9; cb = 4;
    EQ(QW(h, L"d", &type, &v, &cb, TRUE, &def, 4), 0);
    EQ(v, 9);
    pClose(h); g11_clean();
}
TEST(gap11_shreg_query_unicode_value) {
    REQUIRE(QW); REQUIRE(pClose);
    g11_clean();
    char p[256]; HKEY k; g11_sub(p, sizeof p, "a");
    RegCreateKeyExA(HKEY_CURRENT_USER, p, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL);
    WCHAR name[] = { 0x3b1, 0x3b2, 0x3b3, 0 }, val[] = { 0x44f, 0x44e, 0 };
    RegSetValueExW(k, name, 0, REG_SZ, (BYTE *)val, sizeof val);
    RegCloseKey(k);
    HUSKEY h = open_w("a", KEY_READ); NE(h, NULL); if (!h) return;
    WCHAR out[8]; DWORD cb = sizeof out, type = 0;
    EQ(QW(h, name, &type, out, &cb, FALSE, NULL, 0), 0);
    EXPECT_WSTR(out, val);
    pClose(h); g11_clean();
}
TEST(gap11_shreg_query_value_name_case_insensitive) {
    REQUIRE(QW); REQUIRE(pClose);
    g11_clean(); g11_mkdw("a", "MixedCase", 5);
    HUSKEY h = open_w("a", KEY_READ); NE(h, NULL); if (!h) return;
    DWORD v = 0, cb = 4;
    EQ(QW(h, L"mixedCASE", NULL, &v, &cb, FALSE, NULL, 0), 0);
    EQ(v, 5);
    pClose(h); g11_clean();
}

/* ---------------- SHRegGetUSValue ---------------- */
TEST(gap11_shreg_get_w_dword) {
    REQUIRE(GW);
    g11_clean(); g11_mkdw("a", "d", 1234);
    WCHAR p[256]; g11_subw(p, 256, "a");
    DWORD v = 0, cb = 4, type = 0;
    EQ(GW(p, L"d", &type, &v, &cb, FALSE, NULL, 0), 0);
    EQ(v, 1234); EQ(type, REG_DWORD); EQ(cb, 4);
    g11_clean();
}
TEST(gap11_shreg_get_a_dword) {
    REQUIRE(GA);
    g11_clean(); g11_mkdw("a", "d", 4321);
    char p[256]; g11_sub(p, sizeof p, "a");
    DWORD v = 0, cb = 4, type = 0;
    EQ(GA(p, "d", &type, &v, &cb, FALSE, NULL, 0), 0);
    EQ(v, 4321); EQ(type, REG_DWORD); EQ(cb, 4);
    g11_clean();
}
TEST(gap11_shreg_get_sz_parity) {
    REQUIRE(GA); REQUIRE(GW);
    g11_clean(); g11_mksz("a", "s", "parity");
    char pa[256]; WCHAR pw[256]; g11_sub(pa, sizeof pa, "a"); g11_subw(pw, 256, "a");
    char ba[32]; WCHAR bw[32]; DWORD cb = sizeof ba, type = 0;
    EQ(GA(pa, "s", &type, ba, &cb, FALSE, NULL, 0), 0);
    EXPECT_STR(ba, "parity"); EQ(cb, 7);
    cb = sizeof bw;
    EQ(GW(pw, L"s", &type, bw, &cb, FALSE, NULL, 0), 0);
    EXPECT_WSTR(bw, L"parity"); EQ(cb, 14);
    g11_clean();
}
TEST(gap11_shreg_get_missing_key) {
    REQUIRE(GA); REQUIRE(GW);
    g11_clean();
    char pa[256]; WCHAR pw[256]; g11_sub(pa, sizeof pa, "nokey"); g11_subw(pw, 256, "nokey");
    DWORD v = 0x55, cb = 4, type = 0;
    EQ(GW(pw, L"d", &type, &v, &cb, FALSE, NULL, 0), ERROR_FILE_NOT_FOUND);
    EQ(GA(pa, "d", &type, &v, &cb, FALSE, NULL, 0), ERROR_FILE_NOT_FOUND);
    EQ(v, 0x55);
}
TEST(gap11_shreg_get_missing_key_with_default) {
    REQUIRE(GA); REQUIRE(GW);
    g11_clean();
    char pa[256]; WCHAR pw[256]; g11_sub(pa, sizeof pa, "nokey"); g11_subw(pw, 256, "nokey");
    DWORD v = 0, cb = 4, type = REG_DWORD, def = 0x600D;
    EQ(GW(pw, L"d", &type, &v, &cb, FALSE, &def, 4), 0);
    EQ(v, 0x600D); EQ(cb, 4); EQ(type, REG_DWORD);
    v = 0; cb = 4;
    EQ(GA(pa, "d", &type, &v, &cb, FALSE, &def, 4), 0);
    EQ(v, 0x600D);
}
TEST(gap11_shreg_get_missing_value_with_default) {
    REQUIRE(GW);
    g11_clean(); g11_mkdw("a", "d", 1);
    WCHAR p[256]; g11_subw(p, 256, "a");
    DWORD v = 0, cb = 4, type = REG_DWORD, def = 77;
    EQ(GW(p, L"other", &type, &v, &cb, FALSE, &def, 4), 0);
    EQ(v, 77);
    /* the existing value wins over the default */
    cb = 4;
    EQ(GW(p, L"d", &type, &v, &cb, FALSE, &def, 4), 0);
    EQ(v, 1);
    g11_clean();
}
TEST(gap11_shreg_get_default_too_big) {
    REQUIRE(GW);
    g11_clean();
    WCHAR p[256]; g11_subw(p, 256, "nokey");
    char out[2]; DWORD cb = 2, type = 0;
    EQ(GW(p, L"d", &type, out, &cb, FALSE, "abcd", 4), ERROR_FILE_NOT_FOUND);
}
TEST(gap11_shreg_get_ignore_hkcu) {
    REQUIRE(GW);
    g11_clean(); g11_mkdw("a", "d", 1);
    WCHAR p[256]; g11_subw(p, 256, "a");
    DWORD v = 0, cb = 4, type = 0;
    EQ(GW(p, L"d", &type, &v, &cb, TRUE, NULL, 0), ERROR_FILE_NOT_FOUND);
    g11_clean();
}
TEST(gap11_shreg_get_hklm_fallback) {
    REQUIRE(GW);
    WCHAR out[64]; DWORD cb = sizeof out, type = 0;
    EQ(GW(L"Software\\Microsoft\\Windows NT\\CurrentVersion", L"ProductName", &type, out, &cb, FALSE, NULL, 0), 0);
    EQ(type, REG_SZ);
    CHECK(out[0] != 0);
    cb = sizeof out;
    EQ(GW(L"Software\\Microsoft\\Windows NT\\CurrentVersion", L"ProductName", &type, out, &cb, TRUE, NULL, 0), 0);
}
TEST(gap11_shreg_get_exists_only) {
    REQUIRE(GW);
    g11_clean(); g11_mkdw("a", "d", 1);
    WCHAR p[256]; g11_subw(p, 256, "a");
    EQ(GW(p, L"d", NULL, NULL, NULL, FALSE, NULL, 0), 0);
    EQ(GW(p, L"x", NULL, NULL, NULL, FALSE, NULL, 0), ERROR_FILE_NOT_FOUND);
    g11_clean();
}
TEST(gap11_shreg_get_long_names) {
    REQUIRE(GA); REQUIRE(GW);
    char pa[400]; WCHAR pw[400]; DWORD v, cb = 4;
    memset(pa, 'k', 399); pa[399] = 0;
    for (int i = 0; i < 399; i++) pw[i] = L'k';
    pw[399] = 0;
    /* path too long for the 256-char conversion buffer: open fails, default is applied */
    DWORD def = 3; v = 0;
    EQ(GA(pa, "d", NULL, &v, &cb, FALSE, &def, 4), 0);
    EQ(v, 3);
    cb = 4; v = 0;
    EQ(GW(pw, L"d", NULL, &v, &cb, FALSE, &def, 4), 0);
    EQ(v, 3);
    cb = 4;
    NE(GW(pw, L"d", NULL, &v, &cb, FALSE, NULL, 0), 0);
}
TEST(gap11_shreg_get_value_name_long) {
    REQUIRE(GW);
    g11_clean(); g11_mkdw("a", "d", 1);
    WCHAR p[256], n[300]; g11_subw(p, 256, "a");
    for (int i = 0; i < 299; i++) n[i] = L'v'; n[299] = 0;
    DWORD v = 0, cb = 4;
    NE(GW(p, n, NULL, &v, &cb, FALSE, NULL, 0), 0);
    g11_clean();
}
TEST(gap11_shreg_get_repeated) {
    REQUIRE(GW);
    g11_clean(); g11_mkdw("a", "d", 11);
    WCHAR p[256]; g11_subw(p, 256, "a");
    for (int i = 0; i < 500; i++) {
        DWORD v = 0, cb = 4;
        if (GW(p, L"d", NULL, &v, &cb, FALSE, NULL, 0) != 0 || v != 11) { CHECK(0); break; }
    }
    g11_clean();
}
