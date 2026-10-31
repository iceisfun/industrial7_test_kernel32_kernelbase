/* NormalizeString / IsNormalizedString / IDN / scripts / IsNLSDefinedString */
#include "sc.h"
#include <limits.h>

#define NFC 1
#define NFD 2
#define NFKC 5
#define NFKD 6

typedef struct { int form; const WCHAR *src; const WCHAR *dst; } nfv_t;

static void run_norm(const nfv_t *v, int n, const char *tag) {
    if (!U(NormalizeString)) { t_skip("missing export: NormalizeString"); return; }
    for (int i = 0; i < n; i++) {
        WCHAR out[128]; int sl = lstrlenW(v[i].src), dl = lstrlenW(v[i].dst);
        SET_ERR(0);
        int est = U(NormalizeString)(v[i].form, v[i].src, sl, NULL, 0);
        CHECKF(est >= dl, "%s vec %d estimate %d < result %d (err %lu)", tag, i, est, dl, GetLastError());
        for (int k = 0; k < 128; k++) out[k] = 0x7777;
        SET_ERR(0);
        int r = U(NormalizeString)(v[i].form, v[i].src, sl, out, 128);
        CHECKF(r == dl, "%s vec %d got %d want %d (err %lu)", tag, i, r, dl, GetLastError());
        if (r == dl) {
            char g[256], e[256]; sc_wdump(g, sizeof g, out, r); sc_wdump(e, sizeof e, v[i].dst, dl);
            CHECKF(sc_weq(out, v[i].dst, dl), "%s vec %d content form %d got [%s] want [%s]", tag, i, v[i].form, g, e);
            CHECKF(out[dl] == 0x7777, "%s vec %d terminated/wrote past explicit length", tag, i);
        }
        /* -1: terminated, length includes the NUL */
        r = U(NormalizeString)(v[i].form, v[i].src, -1, out, 128);
        CHECKF(r == dl + 1 && out[dl] == 0, "%s vec %d -1 got %d want %d", tag, i, r, dl + 1);
        /* buffer of exactly the estimate works */
        int e2 = U(NormalizeString)(v[i].form, v[i].src, -1, NULL, 0);
        CHECKF(e2 >= dl + 1, "%s vec %d -1 estimate %d", tag, i, e2);
        if (e2 > 0 && e2 <= 128) { r = U(NormalizeString)(v[i].form, v[i].src, -1, out, e2); CHECKF(r == dl + 1, "%s vec %d estimate-sized buffer got %d", tag, i, r); }
        /* result is itself normalized and idempotent */
        if (U(IsNormalizedString)) {
            WCHAR tmp[128]; memcpy(tmp, v[i].dst, (dl + 1) * 2);
            SET_ERR(0);
            BOOL isn = U(IsNormalizedString)(v[i].form, tmp, -1);
            CHECKF(isn, "%s vec %d result not reported normalized (err %lu)", tag, i, GetLastError());
            r = U(NormalizeString)(v[i].form, tmp, dl, out, 128);
            CHECKF(r == dl && sc_weq(out, v[i].dst, dl), "%s vec %d not idempotent", tag, i);
        }
    }
}
#define NFTEST(name, ...) \
    static const nfv_t name##_v[] = { __VA_ARGS__ }; \
    TEST(norm_##name) { run_norm(name##_v, ARRAY_N(name##_v), #name); }

NFTEST(nfc,
    {NFC, L"e\x0301", L"\x00e9"}, {NFC, L"\x00e9", L"\x00e9"}, {NFC, L"E\x0301", L"\x00c9"}, {NFC, L"a\x0308", L"\x00e4"}, {NFC, L"n\x0303", L"\x00f1"}, {NFC, L"A\x030a", L"\x00c5"},
    {NFC, L"abc", L"abc"}, {NFC, L"xe\x0301y", L"x\x00e9y"}, {NFC, L"\x212b", L"\x00c5"}, {NFC, L"\x2126", L"\x03a9"}, {NFC, L"\x1100\x1161", L"\xac00"}, {NFC, L"\x1100\x1161\x11a8", L"\xac01"},
    {NFC, L"\xac00", L"\xac00"}, {NFC, L"\x1112\x1175", L"\xd788"}, {NFC, L"o\x0302\x0323", L"\x1ed9"}, {NFC, L"o\x0323\x0302", L"\x1ed9"}, {NFC, L"q\x0307\x0323", L"q\x0323\x0307"},
    {NFC, L"\x1e0b\x0323", L"\x1e0d\x0307"}, {NFC, L"\x1e0d\x0307", L"\x1e0d\x0307"}, {NFC, L"a\x0301\x0302", L"\x00e1\x0302"}, {NFC, L"\xfb01", L"\xfb01"}, {NFC, L"\xff21", L"\xff21"},
    {NFC, L"\x0041\x0300\x0301", L"\x00c0\x0301"}, {NFC, L"\x0438\x0306", L"\x0439"}, {NFC, L"\x3053\x3099", L"\x3054"}, {NFC, L"\x0627\x0653", L"\x0622"}, {NFC, L"\x0391\x0301", L"\x0386"},
    {NFC, L"\x00a0", L"\x00a0"}, {NFC, L"\x4e2d\x6587", L"\x4e2d\x6587"}, {NFC, L"\x0340", L"\x0300"}, {NFC, L"\x0344", L"\x0308\x0301"}, {NFC, L"\x1f71", L"\x03ac"})
NFTEST(nfd,
    {NFD, L"\x00e9", L"e\x0301"}, {NFD, L"e\x0301", L"e\x0301"}, {NFD, L"\x00c9", L"E\x0301"}, {NFD, L"\x00e4", L"a\x0308"}, {NFD, L"\x00f1", L"n\x0303"}, {NFD, L"\x00c5", L"A\x030a"}, {NFD, L"\x212b", L"A\x030a"},
    {NFD, L"abc", L"abc"}, {NFD, L"\x2126", L"\x03a9"}, {NFD, L"\xac00", L"\x1100\x1161"}, {NFD, L"\xac01", L"\x1100\x1161\x11a8"}, {NFD, L"\xd788", L"\x1112\x1175"}, {NFD, L"\xd7a3", L"\x1112\x1175\x11c2"},
    {NFD, L"\x1ed9", L"o\x0323\x0302"}, {NFD, L"o\x0302\x0323", L"o\x0323\x0302"}, {NFD, L"q\x0307\x0323", L"q\x0323\x0307"}, {NFD, L"\x1e0b\x0323", L"d\x0323\x0307"}, {NFD, L"\xfb01", L"\xfb01"},
    {NFD, L"\x00e7", L"c\x0327"}, {NFD, L"\x1e09", L"c\x0327\x0301"}, {NFD, L"\x0344", L"\x0308\x0301"}, {NFD, L"\x00fc\x00f6", L"u\x0308o\x0308"}, {NFD, L"\x3054", L"\x3053\x3099"},
    {NFD, L"\x0439", L"\x0438\x0306"}, {NFD, L"\x0622", L"\x0627\x0653"}, {NFD, L"\x4e2d", L"\x4e2d"}, {NFD, L"\x00c0\x0301", L"A\x0300\x0301"}, {NFD, L"\x1e9b\x0323", L"\x017f\x0323\x0307"})
NFTEST(nfkc,
    {NFKC, L"\xfb01", L"fi"}, {NFKC, L"\xfb03", L"ffi"}, {NFKC, L"\x2460", L"1"}, {NFKC, L"\xff21\xff22\xff23", L"ABC"}, {NFKC, L"\xff71", L"\x30a2"}, {NFKC, L"\xff76\xff9e", L"\x30ac"},
    {NFKC, L"x\x00b2", L"x2"}, {NFKC, L"\x2026", L"..."}, {NFKC, L"\x2122", L"TM"}, {NFKC, L"\x01c6", L"d\x017e"}, {NFKC, L"e\x0301", L"\x00e9"}, {NFKC, L"\x00a0", L" "}, {NFKC, L"\x3000", L" "},
    {NFKC, L"\xff10\xff19", L"09"}, {NFKC, L"\x2163", L"IV"}, {NFKC, L"\x33a1", L"m2"}, {NFKC, L"\x00b5", L"\x03bc"}, {NFKC, L"\x00aa", L"a"}, {NFKC, L"abc", L"abc"}, {NFKC, L"\xfb00", L"ff"},
    {NFKC, L"\x1100\x1161", L"\xac00"}, {NFKC, L"\x3392", L"MHz"}, {NFKC, L"\x2160\x2161", L"III"} , {NFKC, L"\x00bd", L"1\x2044" L"2"}, {NFKC, L"\x017f", L"s"}, {NFKC, L"\x2126", L"\x03a9"})
NFTEST(nfkd,
    {NFKD, L"\xfb01", L"fi"}, {NFKD, L"\x00e9", L"e\x0301"}, {NFKD, L"\xff21", L"A"}, {NFKD, L"\x2460", L"1"}, {NFKD, L"\xff71", L"\x30a2"}, {NFKD, L"\xff76\xff9e", L"\x30ab\x3099"}, {NFKD, L"\x30ac", L"\x30ab\x3099"},
    {NFKD, L"x\x00b2", L"x2"}, {NFKD, L"\x1e9b\x0323", L"s\x0323\x0307"}, {NFKD, L"\xac00", L"\x1100\x1161"}, {NFKD, L"abc", L"abc"}, {NFKD, L"\x00c5", L"A\x030a"}, {NFKD, L"\x2122", L"TM"}, {NFKD, L"\x01c6", L"dz\x030c"}, {NFKD, L"\x01c4", L"DZ\x030c"}, {NFKD, L"\x3000", L" "})

TEST(norm_estimate_and_buffer_errors) {
    REQUIRE(U(NormalizeString));
    static const WCHAR s[] = W_('e', 0x301, 'e', 0x301, 0);
    WCHAR out[8];
    int est = U(NormalizeString)(NFC, s, -1, NULL, 0);
    CHECK(est >= 3);
    SET_ERR(0);
    int r = U(NormalizeString)(NFC, s, -1, out, 1);
    CHECK(r <= 0); EXPECT_ERR(E_INSUFFICIENT_BUFFER);
    t_note("NFC 'eéé' with 1 char buffer: ret=%d", r);
    CHECK(-r >= 3 || r == 0);
    SET_ERR(0);
    r = U(NormalizeString)(NFC, s, -1, out, 2);
    CHECK(r <= 0); EXPECT_ERR(E_INSUFFICIENT_BUFFER);
    SET_ERR(0);
    r = U(NormalizeString)(NFD, L"\x00e9\x00e9", -1, out, 4);
    CHECK(r <= 0); EXPECT_ERR(E_INSUFFICIENT_BUFFER);
    t_note("NFD 'éé' into 4 chars: ret=%d", r);
    r = U(NormalizeString)(NFC, s, -1, out, 3);
    EXPECT_EQ(r, 3);
    /* the documented retry loop converges */
    WCHAR big[64]; int want = 1, tries = 0, done = 0;
    for (; tries < 10; tries++) {
        SET_ERR(0);
        want = U(NormalizeString)(NFD, L"\x1e09\x1e09\x1e09", -1, big, want);
        if (want > 0) { done = 1; break; }
        if (GetLastError() != E_INSUFFICIENT_BUFFER) break;
        want = -want;
    }
    CHECK(done); EXPECT_EQ(want, 10);
}
TEST(norm_invalid_form) {
    REQUIRE(U(NormalizeString));
    WCHAR out[8];
    static const int bad[] = {0, 3, 4, 7, 8, 9, 10, 100, -1, 0x7fffffff, (int)0x80000000};
    for (int i = 0; i < ARRAY_N(bad); i++) {
        SET_ERR(0);
        int r = U(NormalizeString)(bad[i], L"a", -1, out, 8);
        CHECKF(r <= 0 && GetLastError() == E_INVALID_PARAMETER, "form %d r=%d err=%lu", bad[i], r, GetLastError());
        if (U(IsNormalizedString)) {
            SET_ERR(0);
            BOOL b = U(IsNormalizedString)(bad[i], L"a", -1);
            CHECKF(!b, "IsNormalizedString form %d b=%d err=%lu", bad[i], b, GetLastError());
            if (GetLastError() != E_INVALID_PARAMETER) t_note("IsNormalizedString(form %d) err=%lu", bad[i], GetLastError());
        }
    }
}
TEST(norm_param_errors) {
    REQUIRE(U(NormalizeString));
    WCHAR out[8];
    if (!t_is_wine()) {
        SET_ERR(0); CHECK(U(NormalizeString)(NFC, NULL, -1, out, 8) <= 0); EXPECT_ERR(E_INVALID_PARAMETER);
        SET_ERR(0); CHECK(U(NormalizeString)(NFC, NULL, 1, NULL, 0) <= 0); EXPECT_ERR(E_INVALID_PARAMETER);
    }
    SET_ERR(0); int rn = U(NormalizeString)(NFC, L"a", -1, out, -1);
    t_note("NormalizeString cwDstLength=-1 -> %d err=%lu", rn, GetLastError());
    CHECK(rn <= 0);
    SET_ERR(0); rn = U(NormalizeString)(NFC, L"a", -2, out, 8);
    t_note("NormalizeString cwSrcLength=-2 -> %d err=%lu", rn, GetLastError());
    SET_ERR(0); int r = U(NormalizeString)(NFC, L"a", 0, out, 8);
    t_note("NormalizeString cwSrcLength=0 -> %d err=%lu", r, GetLastError());
    SET_ERR(0); r = U(NormalizeString)(NFC, L"", -1, out, 8);
    EXPECT_EQ(r, 1); EXPECT_EQ(out[0], 0);
    SET_ERR(0); r = U(NormalizeString)(NFC, L"", -1, NULL, 0);
    CHECK(r >= 1);
    if (!t_is_wine()) {
        SET_ERR(0); r = U(NormalizeString)(NFC, L"abc", 3, NULL, 5);
        t_note("NormalizeString dst NULL with size 5 -> %d err=%lu", r, GetLastError());
        CHECK(r <= 0);
    }
}
TEST(norm_invalid_unicode) {
    REQUIRE(U(NormalizeString));
    WCHAR out[16];
    static const struct { WCHAR s[8]; int n; } bad[] = {
        {W_(0xD800), 1}, {W_(0xDC00), 1}, {W_('a', 'b', 0xD800), 3}, {W_(0xD800, 'a'), 2}, {W_(0xDC00, 0xD800), 2}, {W_('a', 0xDFFF, 'b'), 3}, {W_('e', 0x301, 0xD800), 3},
    };
    for (int f = 0; f < 4; f++) {
        static const int forms[] = {NFC, NFD, NFKC, NFKD};
        for (int i = 0; i < ARRAY_N(bad); i++) {
            SET_ERR(0);
            int r = U(NormalizeString)(forms[f], bad[i].s, bad[i].n, out, 16);
            CHECKF(r <= 0 && GetLastError() == E_NO_UNICODE_TRANSLATION, "form %d vec %d r=%d err=%lu", forms[f], i, r, GetLastError());
            if (f == 0) t_note("NormalizeString lone surrogate vec %d -> %d", i, r);
            if (U(IsNormalizedString)) {
                SET_ERR(0);
                BOOL b = U(IsNormalizedString)(forms[f], bad[i].s, bad[i].n);
                CHECKF(!b, "IsNormalizedString form %d vec %d true", forms[f], i);
                if (GetLastError() != E_NO_UNICODE_TRANSLATION) t_note("IsNormalizedString invalid unicode err=%lu", GetLastError());
            }
        }
    }
    /* valid surrogate pair is fine */
    static const WCHAR ok[] = W_(0xD83D, 0xDE00, 'a');
    SET_ERR(0);
    EXPECT_EQ(U(NormalizeString)(NFC, ok, 3, out, 16), 3);
    CHECK(out[0] == 0xD83D && out[1] == 0xDE00 && out[2] == 'a');
    static const WCHAR sm[] = W_(0xD835, 0xDC00);   /* U+1D400 MATHEMATICAL BOLD CAPITAL A */
    EXPECT_EQ(U(NormalizeString)(NFKC, sm, 2, out, 16), 1); EXPECT_EQ(out[0], 'A');
    static const WCHAR cjk[] = W_(0xD87E, 0xDC00);   /* U+2F800 CJK COMPATIBILITY IDEOGRAPH */
    EXPECT_EQ(U(NormalizeString)(NFC, cjk, 2, out, 16), 1); EXPECT_EQ(out[0], 0x4E3D);
}
TEST(norm_long_string) {
    REQUIRE(U(NormalizeString)); REQUIRE(U(IsNormalizedString));
    enum { N = 20000 };
    WCHAR *s = HeapAlloc(GetProcessHeap(), 0, (N * 2 + 1) * 2), *d = HeapAlloc(GetProcessHeap(), 0, (N * 4 + 8) * 2);
    for (int i = 0; i < N; i++) { s[2 * i] = 'e'; s[2 * i + 1] = 0x301; }
    s[2 * N] = 0;
    SET_ERR(0);
    int est = U(NormalizeString)(NFC, s, -1, NULL, 0);
    CHECK(est >= N + 1);
    int r = U(NormalizeString)(NFC, s, -1, d, N * 4);
    EXPECT_EQ(r, N + 1);
    if (r == N + 1) CHECK(d[0] == 0xE9 && d[N - 1] == 0xE9 && d[N] == 0);
    CHECK(!U(IsNormalizedString)(NFC, s, -1));
    CHECK(U(IsNormalizedString)(NFC, d, -1));
    r = U(NormalizeString)(NFD, d, -1, s, N * 2 + 1);
    EXPECT_EQ(r, 2 * N + 1);
    HeapFree(GetProcessHeap(), 0, s); HeapFree(GetProcessHeap(), 0, d);
}
TEST(norm_hangul_exhaustive_roundtrip) {
    REQUIRE(U(NormalizeString));
    int bad = 0;
    for (unsigned c = 0xAC00; c <= 0xD7A3; c += 37) {
        WCHAR s[2] = {(WCHAR)c, 0}, d[8], c2[8];
        int n = U(NormalizeString)(NFD, s, 1, d, 8);
        unsigned s_index = c - 0xAC00, L = 0x1100 + s_index / 588, V = 0x1161 + (s_index % 588) / 28, T = 0x11A7 + s_index % 28;
        int want = T == 0x11A7 ? 2 : 3;
        if (n != want || d[0] != L || d[1] != V || (want == 3 && d[2] != T)) { bad++; continue; }
        int m = U(NormalizeString)(NFC, d, n, c2, 8);
        if (m != 1 || c2[0] != c) bad++;
    }
    EXPECT_EQ(bad, 0);
}
TEST(norm_isnormalized_table) {
    REQUIRE(U(IsNormalizedString));
    static const struct { int form; const WCHAR *s; BOOL r; } v[] = {
        {NFC, L"abc", 1}, {NFD, L"abc", 1}, {NFKC, L"abc", 1}, {NFKD, L"abc", 1}, {NFC, L"", 1}, {NFC, L"\x00e9", 1}, {NFC, L"e\x0301", 0}, {NFD, L"\x00e9", 0}, {NFD, L"e\x0301", 1},
        {NFKC, L"\xff71", 0}, {NFC, L"\xff71", 1}, {NFKD, L"\xfb01", 0}, {NFC, L"\xfb01", 1}, {NFD, L"\xfb01", 1}, {NFKC, L"\xfb01", 0}, {NFC, L"\x212b", 0}, {NFD, L"\x212b", 0},
        {NFC, L"\xac00", 1}, {NFD, L"\xac00", 0}, {NFC, L"\x1100\x1161", 0}, {NFD, L"\x1100\x1161", 1}, {NFKC, L"\x00e9", 1}, {NFKD, L"\x00e9", 0}, {NFC, L"a\x0323\x0302", 0}, {NFC, L"a\x0302\x0323", 0},
        {NFD, L"a\x0302\x0323", 0}, {NFD, L"a\x0323\x0302", 1}, {NFC, L"\x4e2d", 1}, {NFKC, L"\x2460", 0}, {NFC, L"\x2460", 1}, {NFKC, L"\x00a0", 0}, {NFC, L"\x00a0", 1}, {NFC, L"x\x0301", 1},
    };
    for (int i = 0; i < ARRAY_N(v); i++) {
        SET_ERR(0);
        BOOL r = U(IsNormalizedString)(v[i].form, v[i].s, -1);
        CHECKF(!!r == v[i].r, "vec %d form %d got %d want %d", i, v[i].form, r, v[i].r);
        r = U(IsNormalizedString)(v[i].form, v[i].s, lstrlenW(v[i].s));
        CHECKF(!!r == v[i].r, "vec %d explicit len got %d", i, r);
    }
    if (!t_is_wine()) {
        SET_ERR(0); CHECK(!U(IsNormalizedString)(NFC, NULL, -1)); EXPECT_ERR(E_INVALID_PARAMETER);
        SET_ERR(0); CHECK(!U(IsNormalizedString)(NFC, NULL, 3)); EXPECT_ERR(E_INVALID_PARAMETER);
    }
    SET_ERR(0); BOOL r = U(IsNormalizedString)(NFC, L"a", 0);
    t_note("IsNormalizedString cch=0 -> %d err=%lu", r, GetLastError());
}
TEST(norm_normalization_agreement) {
    REQUIRE(U(NormalizeString)); REQUIRE(U(IsNormalizedString));
    /* every output of a form is reported normalized for that form, over a wide sample */
    static const WCHAR *s[] = {L"e\x0301", L"\x00e9", L"\xfb01", L"\xac01", L"\x1100\x1161\x11a8", L"A\x030a", L"\x212b", L"o\x0302\x0323", L"\x1e0b\x0323", L"\xff21\xff22", L"\x2460", L"q\x0307\x0323", L"\x0344", L"abc"};
    static const int forms[] = {NFC, NFD, NFKC, NFKD};
    for (int i = 0; i < ARRAY_N(s); i++) for (int f = 0; f < 4; f++) {
        WCHAR out[32];
        int n = U(NormalizeString)(forms[f], s[i], -1, out, 32);
        CHECKF(n > 0, "vec %d form %d n=%d", i, forms[f], n);
        if (n > 0) CHECKF(U(IsNormalizedString)(forms[f], out, -1), "vec %d form %d output not normalized", i, forms[f]);
        /* NFD(NFC(x)) == NFD(x) */
        WCHAR a[32], b[32], c[32];
        int na = U(NormalizeString)(NFC, s[i], -1, a, 32);
        int nb = na > 0 ? U(NormalizeString)(NFD, a, -1, b, 32) : 0, nc = U(NormalizeString)(NFD, s[i], -1, c, 32);
        CHECKF(nb == nc && nb > 0 && sc_weq(b, c, nb), "vec %d NFD(NFC(x)) != NFD(x)", i);
    }
}

/* ---- IDN ---- */
typedef struct { DWORD fl; const WCHAR *uni; const char *ascii; } idnv_t;
static const idnv_t idn_v[] = {
    {0, L"example.com", "example.com"}, {0, L"EXAMPLE.COM", "EXAMPLE.COM"}, {0, L"a", "a"}, {0, L"www.example.com", "www.example.com"}, {0, L"example.com.", "example.com."},
    {0, L"b\x00fc" L"cher.de", "xn--bcher-kva.de"}, {0, L"M\x00fc" L"nchen", "xn--mnchen-3ya"}, {0, L"\x00f1.com", "xn--ida.com"}, {0, L"\x65e5\x672c\x8a9e.jp", "xn--wgv71a119e.jp"},
    {0, L"\x4e2d\x6587.com", "xn--fiq228c.com"}, {0, L"b\x00fc" L"cher.example.com", "xn--bcher-kva.example.com"}, {0, L"www.b\x00fc" L"cher.de", "www.xn--bcher-kva.de"}, {0, L"caf\x00e9.fr", "xn--caf-dma.fr"},
    {0, L"\x0645\x062b\x0627\x0644.com", "xn--mgbh0fb.com"}, {0, L"\x043f\x0440\x0438\x043c\x0435\x0440.com", "xn--e1afmkfd.com"}, {0, L"\x00e9.com", "xn--9ca.com"},
    {IDN_ALLOW_UNASSIGNED, L"example.com", "example.com"}, {IDN_USE_STD3_ASCII_RULES, L"example.com", "example.com"}, {IDN_USE_STD3_ASCII_RULES, L"a-b.com", "a-b.com"},
    {IDN_ALLOW_UNASSIGNED | IDN_USE_STD3_ASCII_RULES, L"b\x00fc" L"cher.de", "xn--bcher-kva.de"}, {0, L"a_b.com", "a_b.com"}, {0, L"123.com", "123.com"},
};
TEST(idn_to_ascii_table) {
    REQUIRE(U(IdnToAscii));
    for (int i = 0; i < ARRAY_N(idn_v); i++) {
        WCHAR out[256]; int al = (int)strlen(idn_v[i].ascii);
        SET_ERR(0);
        int q = U(IdnToAscii)(idn_v[i].fl, idn_v[i].uni, -1, NULL, 0);
        CHECKF(q == al + 1, "vec %d [%s] query got %d want %d (err %lu)", i, idn_v[i].ascii, q, al + 1, GetLastError());
        for (int k = 0; k < 256; k++) out[k] = 0x7777;
        int r = U(IdnToAscii)(idn_v[i].fl, idn_v[i].uni, -1, out, 256);
        CHECKF(r == al + 1, "vec %d [%s] got %d want %d (err %lu)", i, idn_v[i].ascii, r, al + 1, GetLastError());
        if (r == al + 1) {
            int same = 1; for (int k = 0; k < al; k++) if (out[k] != (WCHAR)(unsigned char)idn_v[i].ascii[k]) same = 0;
            CHECKF(same && out[al] == 0, "vec %d content mismatch", i);
        }
        r = U(IdnToAscii)(idn_v[i].fl, idn_v[i].uni, lstrlenW(idn_v[i].uni), out, 256);
        CHECKF(r == al, "vec %d explicit len got %d want %d", i, r, al);
        SET_ERR(0);
        r = U(IdnToAscii)(idn_v[i].fl, idn_v[i].uni, -1, out, al);
        CHECKF(r == 0 && GetLastError() == E_INSUFFICIENT_BUFFER, "vec %d short buffer r=%d err=%lu", i, r, GetLastError());
    }
}
TEST(idn_to_unicode_table) {
    REQUIRE(U(IdnToUnicode));
    static const struct { const WCHAR *ascii; const WCHAR *uni; } v[] = {
        {L"example.com", L"example.com"}, {L"xn--bcher-kva.de", L"b\x00fc" L"cher.de"}, {L"xn--mnchen-3ya", L"m\x00fc" L"nchen"}, {L"xn--ida.com", L"\x00f1.com"}, {L"xn--wgv71a119e.jp", L"\x65e5\x672c\x8a9e.jp"},
        {L"xn--fiq228c.com", L"\x4e2d\x6587.com"}, {L"www.xn--bcher-kva.de", L"www.b\x00fc" L"cher.de"}, {L"xn--caf-dma.fr", L"caf\x00e9.fr"}, {L"xn--e1afmkfd.com", L"\x043f\x0440\x0438\x043c\x0435\x0440.com"},
        {L"xn--9ca.com", L"\x00e9.com"}, {L"a", L"a"}, {L"example.com.", L"example.com."}, {L"xn--mgbh0fb.com", L"\x0645\x062b\x0627\x0644.com"},
    };
    for (int i = 0; i < ARRAY_N(v); i++) {
        WCHAR out[256]; int ul = lstrlenW(v[i].uni);
        SET_ERR(0);
        int q = U(IdnToUnicode)(0, v[i].ascii, -1, NULL, 0);
        CHECKF(q == ul + 1, "vec %d query got %d want %d (err %lu)", i, q, ul + 1, GetLastError());
        int r = U(IdnToUnicode)(0, v[i].ascii, -1, out, 256);
        CHECKF(r == ul + 1 && sc_weq(out, v[i].uni, ul) && out[ul] == 0, "vec %d got %d", i, r);
        r = U(IdnToUnicode)(0, v[i].ascii, lstrlenW(v[i].ascii), out, 256);
        CHECKF(r == ul && sc_weq(out, v[i].uni, ul), "vec %d explicit got %d", i, r);
        SET_ERR(0);
        r = U(IdnToUnicode)(0, v[i].ascii, -1, out, ul);
        CHECKF(r == 0 && GetLastError() == E_INSUFFICIENT_BUFFER, "vec %d short r=%d err=%lu", i, r, GetLastError());
    }
}
TEST(idn_roundtrip) {
    REQUIRE(U(IdnToAscii)); REQUIRE(U(IdnToUnicode));
    static const WCHAR *s[] = {L"b\x00fc" L"cher.de", L"\x65e5\x672c\x8a9e.jp", L"\x4e2d\x6587.com", L"\x043f\x0440\x0438\x043c\x0435\x0440.com", L"caf\x00e9.fr", L"\x00f1.com", L"example.com", L"\x0645\x062b\x0627\x0644.com", L"\x5bb6.\x4e2d\x56fd"};
    for (int i = 0; i < ARRAY_N(s); i++) {
        WCHAR a[256], u[256];
        int na = U(IdnToAscii)(0, s[i], -1, a, 256);
        CHECKF(na > 0, "vec %d to ascii err %lu", i, GetLastError());
        if (na <= 0) continue;
        for (int k = 0; k < na; k++) CHECKF(a[k] < 0x80, "vec %d non-ASCII output", i);
        int nu = U(IdnToUnicode)(0, a, -1, u, 256);
        CHECKF(nu == lstrlenW(s[i]) + 1 && sc_weq(u, s[i], nu), "vec %d roundtrip n=%d", i, nu);
    }
}
TEST(idn_to_ascii_errors) {
    REQUIRE(U(IdnToAscii));
    WCHAR out[300];
    static const WCHAR *bad[] = {L"a..b", L".com", L"..", L"a b.com", L"-bad.com", L"bad-.com", L"a_b.com"};
    for (int i = 0; i < ARRAY_N(bad); i++) {
        SET_ERR(0);
        int r = U(IdnToAscii)(IDN_USE_STD3_ASCII_RULES, bad[i], -1, out, 300);
        if (i == 0 || i == 1 || i == 2) CHECKF(r == 0 && GetLastError() == ERROR_INVALID_NAME, "empty label vec %d r=%d err=%lu", i, r, GetLastError());
        else t_note("STD3 vec %d -> %d err=%lu", i, r, r ? 0 : GetLastError());
    }
    WCHAR l64[80]; for (int i = 0; i < 64; i++) l64[i] = 'a'; l64[64] = 0;
    SET_ERR(0); int r = U(IdnToAscii)(0, l64, -1, out, 300);
    CHECK(r == 0); EXPECT_ERR(ERROR_INVALID_NAME);
    WCHAR l63[80]; for (int i = 0; i < 63; i++) l63[i] = 'a'; l63[63] = 0;
    SET_ERR(0); r = U(IdnToAscii)(0, l63, -1, out, 300);
    EXPECT_EQ(r, 64);
    SET_ERR(0); EXPECT_EQ(U(IdnToAscii)(0, NULL, -1, out, 300), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(IdnToAscii)(0, L"a", 0, out, 300), 0);
    t_note("IdnToAscii cch=0 err=%lu", GetLastError());
    SET_ERR(0); EXPECT_EQ(U(IdnToAscii)(0, L"a", -1, out, -1), 0);
    t_note("IdnToAscii cchASCII=-1 err=%lu", GetLastError());
    static const DWORD badf[] = {0x10, 0x20, 0x100, 0x80000000, 0xffffffff};
    for (int i = 0; i < ARRAY_N(badf); i++) { SET_ERR(0); r = U(IdnToAscii)(badf[i], L"a", -1, out, 300); CHECKF(r == 0 && GetLastError() == E_INVALID_FLAGS, "flags %lx r=%d err=%lu", badf[i], r, GetLastError()); }
    SET_ERR(0); r = U(IdnToAscii)(8, L"a", -1, out, 300);
    t_note("IdnToAscii IDN_RAW_PUNYCODE -> %d err=%lu", r, r ? 0 : GetLastError());
    WCHAR big[600]; for (int i = 0; i < 590; i++) big[i] = (i % 60 == 59) ? '.' : 'a'; big[590] = 0;
    SET_ERR(0); r = U(IdnToAscii)(0, big, -1, out, 300);
    t_note("IdnToAscii 590 chars -> %d err=%lu", r, r ? 0 : GetLastError());
    CHECK(r == 0);
}
TEST(idn_to_unicode_errors) {
    REQUIRE(U(IdnToUnicode));
    WCHAR out[100];
    SET_ERR(0); EXPECT_EQ(U(IdnToUnicode)(0, NULL, -1, out, 100), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    static const DWORD badf[] = {0x10, 0x20, 0x100, 0x80000000, 0xffffffff};
    for (int i = 0; i < ARRAY_N(badf); i++) { SET_ERR(0); int r = U(IdnToUnicode)(badf[i], L"a", -1, out, 100); CHECKF(r == 0 && GetLastError() == E_INVALID_FLAGS, "flags %lx r=%d err=%lu", badf[i], r, GetLastError()); }
    static const WCHAR *bad[] = {L"xn--", L"xn--!!!", L"xn---", L"a..b", L".com", L"xn--bcher-kvb.de"};
    for (int i = 0; i < ARRAY_N(bad); i++) { SET_ERR(0); int r = U(IdnToUnicode)(0, bad[i], -1, out, 100); t_note("IdnToUnicode bad vec %d -> %d err=%lu", i, r, r ? 0 : GetLastError()); }
    SET_ERR(0); int r = U(IdnToUnicode)(0, L"a..b", -1, out, 100);
    CHECK(r == 0); EXPECT_ERR(ERROR_INVALID_NAME);
    /* non-ASCII input to the ASCII->Unicode direction */
    SET_ERR(0); r = U(IdnToUnicode)(0, L"b\x00fc" L"cher.de", -1, out, 100);
    t_note("IdnToUnicode(non-ascii input) -> %d err=%lu", r, r ? 0 : GetLastError());
}
TEST(idn_nameprep) {
    REQUIRE(U(IdnToNameprepUnicode));
    WCHAR out[100];
    SET_ERR(0);
    int r = U(IdnToNameprepUnicode)(0, L"EXAMPLE.COM", -1, out, 100);
    EXPECT_EQ(r, 12);
    t_note("nameprep(EXAMPLE.COM) first chars: %04X %04X", out[0], out[1]);
    r = U(IdnToNameprepUnicode)(0, L"B\x00dc" L"CHER.de", -1, out, 100);
    EXPECT_EQ(r, 10); t_note("nameprep(BUECHER) first chars: %04X %04X", out[0], out[1]);
    r = U(IdnToNameprepUnicode)(0, L"e\x0301.com", -1, out, 100);
    EXPECT_EQ(r, 6); if (r == 6) CHECK_WEQ(out, L"\x00e9.com", 6);
    r = U(IdnToNameprepUnicode)(0, L"example.com", -1, NULL, 0);
    EXPECT_EQ(r, 12);
    SET_ERR(0); r = U(IdnToNameprepUnicode)(0, L"example.com", -1, out, 5);
    EXPECT_EQ(r, 0); EXPECT_ERR(E_INSUFFICIENT_BUFFER);
    if (!t_is_wine()) { SET_ERR(0); r = U(IdnToNameprepUnicode)(0, NULL, -1, out, 100); EXPECT_EQ(r, 0); EXPECT_ERR(E_INVALID_PARAMETER); }
    SET_ERR(0); r = U(IdnToNameprepUnicode)(0x10, L"a", -1, out, 100);
    EXPECT_EQ(r, 0); EXPECT_ERR(E_INVALID_FLAGS);
    r = U(IdnToNameprepUnicode)(0, L"Stra\x00dfe", -1, out, 100);
    t_note("nameprep(Strasse with sharp s) -> %d: %04X%04X%04X%04X%04X%04X", r, out[0], out[1], out[2], out[3], out[4], out[5]);
}

/* ---- scripts ---- */
TEST(scripts_get) {
    REQUIRE(U(GetStringScripts));
    static const struct { const WCHAR *s; const WCHAR *scr; } v[] = {
        {L"abc", L"Latn;"}, {L"\x03b1\x03b2\x03b3", L"Grek;"}, {L"abc\x03b1\x03b2\x03b3", L"Latn;Grek;"}, {L"\x4e2d\x6587", L"Hani;"}, {L"\x3042", L"Hira;"}, {L"\x30a2", L"Kana;"}, {L"\x043f\x0440", L"Cyrl;"},
        {L"\x05d0\x05d1", L"Hebr;"}, {L"\x0627\x0628", L"Arab;"}, {L"\x0e01", L"Thai;"}, {L"\xac00", L"Hang;"}, {L"\x0905", L"Deva;"}, {L"\x00e9", L"Latn;"}, {L"a\x043f", L"Latn;Cyrl;"}, {L"\x043f" L"a", L"Cyrl;Latn;"},
        {L"abcabc", L"Latn;"},
    };
    for (int i = 0; i < ARRAY_N(v); i++) {
        WCHAR out[64]; for (int k = 0; k < 64; k++) out[k] = 0x7777;
        SET_ERR(0);
        int r = U(GetStringScripts)(0, v[i].s, -1, out, 64);
        int want = lstrlenW(v[i].scr);
        t_check(r == want || r == want + 1, __FILE__, __LINE__, "GetStringScripts length", "vec %d r=%d want %d(+1) err=%lu", i, r, want, GetLastError());
        if (r > 0) CHECKF(sc_weq(out, v[i].scr, want), "vec %d content [%04X %04X ...]", i, out[0], out[1]);
        if (i == 0) t_note("GetStringScripts('abc') returned %d (string length %d)", r, want);
    }
}
TEST(scripts_get_errors) {
    REQUIRE(U(GetStringScripts));
    WCHAR out[64];
    SET_ERR(0); EXPECT_EQ(U(GetStringScripts)(0, NULL, -1, out, 64), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(GetStringScripts)(0x80, L"abc", -1, out, 64), 0); EXPECT_ERR(E_INVALID_FLAGS);
    SET_ERR(0); EXPECT_EQ(U(GetStringScripts)(0xffffffff, L"abc", -1, out, 64), 0); EXPECT_ERR(E_INVALID_FLAGS);
    SET_ERR(0); EXPECT_EQ(U(GetStringScripts)(0, L"abc", -1, out, -1), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(GetStringScripts)(0, L"abc", -1, NULL, 5), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(GetStringScripts)(0, L"abc", -1, out, 2), 0); EXPECT_ERR(E_INSUFFICIENT_BUFFER);
    SET_ERR(0); int q = U(GetStringScripts)(0, L"abc", -1, NULL, 0);
    t_note("GetStringScripts size query -> %d err=%lu", q, q ? 0 : GetLastError());
    SET_ERR(0); q = U(GetStringScripts)(0, L"123", -1, out, 64);
    t_note("GetStringScripts('123') -> %d [%04X %04X %04X %04X %04X]", q, out[0], out[1], out[2], out[3], out[4]);
    SET_ERR(0); q = U(GetStringScripts)(GSS_ALLOW_INHERITED_COMMON, L"123", -1, out, 64);
    t_note("GetStringScripts(flag 1,'123') -> %d [%04X %04X %04X %04X %04X]", q, out[0], out[1], out[2], out[3], out[4]);
}
TEST(scripts_verify) {
    REQUIRE(U(VerifyScripts));
    EXPECT_EQ(U(VerifyScripts)(0, L"Latn;", -1, L"Latn;", -1), TRUE);
    EXPECT_EQ(U(VerifyScripts)(0, L"Latn;Grek;", -1, L"Grek;", -1), TRUE);
    EXPECT_EQ(U(VerifyScripts)(0, L"Latn;Grek;", -1, L"Latn;Grek;", -1), TRUE);
    SET_ERR(0); EXPECT_EQ(U(VerifyScripts)(0, L"Latn;", -1, L"Grek;", -1), FALSE);
    EXPECT_EQ(U(VerifyScripts)(0, L"Latn;", -1, L"Latn;Grek;", -1), FALSE);
    EXPECT_EQ(U(VerifyScripts)(VS_ALLOW_LATIN, L"Grek;", -1, L"Latn;", -1), TRUE);
    EXPECT_EQ(U(VerifyScripts)(VS_ALLOW_LATIN, L"Grek;", -1, L"Latn;Grek;", -1), TRUE);
    EXPECT_EQ(U(VerifyScripts)(VS_ALLOW_LATIN, L"Grek;", -1, L"Cyrl;", -1), FALSE);
    EXPECT_EQ(U(VerifyScripts)(0, L"Grek;", -1, L"Latn;", -1), FALSE);
    EXPECT_EQ(U(VerifyScripts)(0, L"Latn;", 5, L"Latn;", 5), TRUE);
    SET_ERR(0); EXPECT_EQ(U(VerifyScripts)(0, NULL, -1, L"Latn;", -1), FALSE); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(VerifyScripts)(0, L"Latn;", -1, NULL, -1), FALSE); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(VerifyScripts)(0x80, L"Latn;", -1, L"Latn;", -1), FALSE); EXPECT_ERR(E_INVALID_FLAGS);
    SET_ERR(0); BOOL r = U(VerifyScripts)(0, L"Latn", -1, L"Latn;", -1);
    t_note("VerifyScripts(missing terminator in locale list) = %d err=%lu", r, GetLastError());
    SET_ERR(0); r = U(VerifyScripts)(0, L"Xxxx;", -1, L"Latn;", -1);
    t_note("VerifyScripts(unknown script) = %d err=%lu", r, GetLastError());
}
TEST(scripts_get_then_verify) {
    REQUIRE(U(GetStringScripts)); REQUIRE(U(VerifyScripts));
    static const WCHAR *s[] = {L"abc", L"\x03b1\x03b2", L"abc\x03b1", L"\x4e2d", L"\x043f" L"a"};
    for (int i = 0; i < ARRAY_N(s); i++) {
        WCHAR sc[64]; int n = U(GetStringScripts)(0, s[i], -1, sc, 64);
        CHECKF(n > 0, "vec %d", i);
        if (n > 0) CHECKF(U(VerifyScripts)(0, sc, -1, sc, -1), "vec %d self-verify", i);
    }
}
TEST(nls_defined_string) {
    REQUIRE(U(IsNLSDefinedString));
    SET_ERR(0);
    EXPECT_EQ(U(IsNLSDefinedString)(COMPARE_STRING, 0, NULL, L"abc", 3), TRUE);
    EXPECT_EQ(U(IsNLSDefinedString)(COMPARE_STRING, 0, NULL, L"abc", -1), TRUE);
    EXPECT_EQ(U(IsNLSDefinedString)(COMPARE_STRING, 0, NULL, L"\x00e9\x4e2d\x0430", -1), TRUE);
    EXPECT_EQ(U(IsNLSDefinedString)(COMPARE_STRING, 0, NULL, L"a\x0378", -1), FALSE);
    EXPECT_EQ(U(IsNLSDefinedString)(COMPARE_STRING, 0, NULL, L"\xd800", 1), FALSE);
    EXPECT_EQ(U(IsNLSDefinedString)(COMPARE_STRING, 0, NULL, L"\xd83d\xde00", 2), TRUE);
    if (!t_is_wine()) { SET_ERR(0); EXPECT_EQ(U(IsNLSDefinedString)(COMPARE_STRING, 0, NULL, NULL, 3), FALSE); EXPECT_ERR(E_INVALID_PARAMETER); }
    SET_ERR(0); BOOL r = U(IsNLSDefinedString)(0, 0, NULL, L"abc", 3);
    t_note("IsNLSDefinedString(function 0) = %d err=%lu", r, GetLastError());
    SET_ERR(0); r = U(IsNLSDefinedString)(COMPARE_STRING, 0xff, NULL, L"abc", 3);
    t_note("IsNLSDefinedString(flags 0xff) = %d err=%lu", r, GetLastError());
    SET_ERR(0); r = U(IsNLSDefinedString)(COMPARE_STRING, 0, NULL, L"abc", 0);
    t_note("IsNLSDefinedString(cch 0) = %d err=%lu", r, GetLastError());
}
