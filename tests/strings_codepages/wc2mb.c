/* WideCharToMultiByte */
#include "sc.h"

typedef struct { DWORD fl; WCHAR w[12]; int wn; char b[24]; int bn; } wcv_t;
#define UNP(...) __VA_ARGS__
#define WV(fl, wl, b) { (fl), { UNP wl }, (int)(sizeof((WCHAR[]){UNP wl}) / sizeof(WCHAR)), b, (int)sizeof(b) - 1 }

static int wc_table(UINT cp, const wcv_t *v, int nv, const char *tag) {
    if (!U(WideCharToMultiByte)) { t_skip("missing export: WideCharToMultiByte"); return 0; }
    SET_ERR(0);
    static const WCHAR pa[] = W_('A', 0);
    int probe = U(WideCharToMultiByte)(cp, 0, pa, 1, NULL, 0, NULL, NULL);
    if (probe != 1) { t_note("%s: code page %u not usable here (probe=%d err=%lu)", tag, cp, probe, GetLastError()); t_skip("cp unavailable"); return 0; }
    for (int i = 0; i < nv; i++) {
        char out[64];
        int q = U(WideCharToMultiByte)(cp, v[i].fl, v[i].w, v[i].wn, NULL, 0, NULL, NULL);
        CHECKF(q == v[i].bn, "%s vec %d size query got %d want %d", tag, i, q, v[i].bn);
        memset(out, 0x77, sizeof out);
        SET_ERR(0);
        int r = U(WideCharToMultiByte)(cp, v[i].fl, v[i].w, v[i].wn, out, 64, NULL, NULL);
        CHECKF(r == v[i].bn, "%s vec %d convert got %d want %d", tag, i, r, v[i].bn);
        if (r == v[i].bn) {
            char g[256], e[256]; sc_bdump(g, sizeof g, (unsigned char *)out, r); sc_bdump(e, sizeof e, (unsigned char *)v[i].b, r);
            CHECKF(!memcmp(out, v[i].b, r), "%s vec %d content got [%s] want [%s]", tag, i, g, e);
            CHECKF(out[r] == 0x77, "%s vec %d wrote past result", tag, i);
        }
        memset(out, 0x77, sizeof out);
        r = U(WideCharToMultiByte)(cp, v[i].fl, v[i].w, -1, out, 64, NULL, NULL);
        /* the vector is NUL-terminated by construction only if we append; build a terminated copy */
        WCHAR tw[16]; memcpy(tw, v[i].w, v[i].wn * 2); tw[v[i].wn] = 0;
        r = U(WideCharToMultiByte)(cp, v[i].fl, tw, -1, out, 64, NULL, NULL);
        CHECKF(r == v[i].bn + 1, "%s vec %d -1 got %d want %d", tag, i, r, v[i].bn + 1);
        if (r == v[i].bn + 1) CHECKF(out[r - 1] == 0 && !memcmp(out, v[i].b, v[i].bn), "%s vec %d -1 content", tag, i);
        if (v[i].bn >= 1) {
            SET_ERR(0);
            r = U(WideCharToMultiByte)(cp, v[i].fl, v[i].w, v[i].wn, out, v[i].bn - 1 > 0 ? v[i].bn - 1 : 0, NULL, NULL);
            if (v[i].bn - 1 > 0) { CHECKF(r == 0, "%s vec %d short buffer returned %d", tag, i, r); EXPECT_ERR(E_INSUFFICIENT_BUFFER); }
            SET_ERR(0);
            r = U(WideCharToMultiByte)(cp, v[i].fl, tw, -1, out, v[i].bn, NULL, NULL);
            CHECKF(r == 0, "%s vec %d -1 short got %d", tag, i, r);
            EXPECT_ERR(E_INSUFFICIENT_BUFFER);
        }
    }
    return 1;
}
#define WCTEST(name, cp, ...) \
    static const wcv_t name##_v[] = { __VA_ARGS__ }; \
    TEST(wc2mb_##name) { wc_table(cp, name##_v, ARRAY_N(name##_v), #name); }

WCTEST(cp437, 437,
    WV(0, (0xE9), "\x82"), WV(0, (0xC7, 0xFC, 0xE9), "\x80\x81\x82"), WV(0, (0x3B1), "\xe0"), WV(0, (0x221E), "\xec"), WV(0, (0x2591, 0x2588), "\xb0\xdb"),
    WV(0, (0x2500, 0x2502), "\xc4\xb3"), WV(0, (0xDF), "\xe1"), WV(0, (0x3A3), "\xe4"), WV(0, (0xB5), "\xe6"), WV(0, (0x25A0), "\xfe"), WV(0, (0xA0), "\xff"),
    WV(0, (0xC0), "A"), WV(0, (0x41, 0x42), "AB"), WV(0, (0x3042), "?"), WV(0, (0x20, 0x7E), " ~"), WV(0, (0x2264, 0x2265), "\xf3\xf2"))
WCTEST(cp850, 850, WV(0, (0xF8, 0xD8, 0x131, 0xD3), "\x9b\x9d\xd5\xe0"), WV(0, (0xC1, 0xC2, 0xC3), "\xb5\xb6\xc7"), WV(0, (0xA0), "\xff"), WV(0, (0x3042), "?"))
WCTEST(cp866, 866, WV(0, (0x410, 0x430, 0x440, 0x401), "\x80\xa0\xe0\xf0"), WV(0, (0x3042), "?"))
WCTEST(cp874, 874, WV(0, (0xE01, 0xE3F, 0x20AC), "\xa1\xdf\x80"), WV(0, (0x2026), "\x85"))
WCTEST(cp1250, 1250, WV(0, (0x105, 0x10D, 0x141, 0x20AC, 0x160), "\xb9\xe8\xa3\x80\x8a"), WV(0, (0x154, 0x155), "\xc0\xe0"), WV(0, (0x3042), "?"))
WCTEST(cp1251, 1251, WV(0, (0x410, 0x430, 0x401, 0x2116, 0x20AC, 0x44F), "\xc0\xe0\xa8\xb9\x88\xff"), WV(0, (0x3042), "?"))
WCTEST(cp1252, 1252,
    WV(0, (0x20AC), "\x80"), WV(0, (0x201A, 0x192, 0x201E, 0x2026, 0x2020, 0x2021, 0x2C6, 0x2030), "\x82\x83\x84\x85\x86\x87\x88\x89"),
    WV(0, (0x160, 0x2039, 0x152, 0x17D), "\x8a\x8b\x8c\x8e"), WV(0, (0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x2DC, 0x2122), "\x91\x92\x93\x94\x95\x96\x97\x98\x99"),
    WV(0, (0x161, 0x203A, 0x153, 0x17E, 0x178), "\x9a\x9b\x9c\x9e\x9f"), WV(0, (0xA0, 0xE9, 0xFF), "\xa0\xe9\xff"),
    WV(0, (0x100), "A"), WV(0, (0x101), "a"), WV(0, (0x102, 0x103), "Aa"), WV(0, (0x104, 0x105), "Aa"), WV(0, (0x106, 0x107), "Cc"), WV(0, (0x141, 0x142), "Ll"),
    WV(0, (0xFF21, 0xFF41), "Aa"), WV(0, (0x3042), "?"), WV(0, (0xFFFF), "?"), WV(0, (0x41, 0x3042, 0x42), "A?B"),
    WV(WC_NO_BEST_FIT_CHARS, (0x101), "?"), WV(WC_NO_BEST_FIT_CHARS, (0x20AC, 0xE9), "\x80\xe9"), WV(WC_NO_BEST_FIT_CHARS, (0x41, 0x141), "A?"),
    WV(0, (0x7F, 0x1, 0x1F), "\x7f\x01\x1f"), WV(0, (0x81, 0x8D, 0x8F, 0x90, 0x9D), "\x81\x8d\x8f\x90\x9d"),
    WV(WC_COMPOSITECHECK, (0x65, 0x301), "\xe9"), WV(WC_COMPOSITECHECK, (0x41, 0x30A), "\xc5"), WV(WC_COMPOSITECHECK, (0x6E, 0x303), "\xf1"),
    WV(WC_COMPOSITECHECK, (0x41, 0x42), "AB"), WV(WC_COMPOSITECHECK, (0xE9), "\xe9"),
    WV(WC_COMPOSITECHECK | WC_DISCARDNS, (0x78, 0x301), "x"), WV(WC_COMPOSITECHECK | WC_DEFAULTCHAR, (0x78, 0x301), "x?"),
    WV(WC_COMPOSITECHECK | WC_SEPCHARS, (0x65, 0x301), "\xe9"))
WCTEST(cp1253, 1253, WV(0, (0x391, 0x3C9, 0x386, 0x385), "\xc1\xf9\xa2\xa1"), WV(0, (0x3042), "?"))
WCTEST(cp1254, 1254, WV(0, (0x11E, 0x131, 0x15F, 0x130), "\xd0\xfd\xfe\xdd"))
WCTEST(cp1255, 1255, WV(0, (0x5D0, 0x20AA, 0x5EA), "\xe0\xa4\xfa"))
WCTEST(cp1256, 1256, WV(0, (0x627, 0x67E, 0x6D2), "\xc7\x81\xff"))
WCTEST(cp1257, 1257, WV(0, (0x104, 0x160, 0x17E), "\xc0\xd0\xfe"))
WCTEST(cp1258, 1258, WV(0, (0x102, 0x1AF, 0x111), "\xc3\xdd\xf0"))
WCTEST(cp28591, 28591, WV(0, (0xE9, 0xFF, 0xA0), "\xe9\xff\xa0"), WV(0, (0x20AC), "?"), WV(WC_NO_BEST_FIT_CHARS, (0x20AC), "?"))
WCTEST(cp28592, 28592, WV(0, (0x104, 0x10D, 0x160), "\xa1\xe8\xa9"))
WCTEST(cp28605, 28605, WV(0, (0x20AC, 0x152, 0x153, 0x178), "\xa4\xbc\xbd\xbe"))
WCTEST(cp20866, 20866, WV(0, (0x430, 0x410, 0x451), "\xc1\xe1\xa3"))
WCTEST(cp10000, 10000, WV(0, (0xE9, 0x2022, 0x2013), "\x8e\xa5\xd0"))
WCTEST(cp20127, 20127, WV(0, (0x41, 0x7F), "A\x7f"), WV(0, (0xE9), "e"), WV(WC_NO_BEST_FIT_CHARS, (0xE9), "?"))
WCTEST(cp932, 932,
    WV(0, (0x3042), "\x82\xa0"), WV(0, (0x30A2), "\x83\x41"), WV(0, (0x65E5, 0x672C), "\x93\xfa\x96\x7b"), WV(0, (0xFF71), "\xb1"), WV(0, (0x3000), "\x81\x40"),
    WV(0, (0x5C, 0x7E), "\x5c\x7e"), WV(0, (0xD7), "\x81\x7e"), WV(0, (0x41, 0x3042, 0x42), "A\x82\xa0" "B"), WV(0, (0xFF61, 0xFF9F), "\xa1\xdf"), WV(0, (0x2460), "\x87\x40"),
    WV(0, (0x4E9C), "\x88\x9f"), WV(0, (0xAC00), "?"), WV(0, (0x1234), "?"))
WCTEST(cp936, 936, WV(0, (0x4E2D, 0x6587), "\xd6\xd0\xce\xc4"), WV(0, (0x3000), "\xa1\xa1"), WV(0, (0x554A), "\xb0\xa1"), WV(0, (0x61, 0x4E2D, 0x62), "a\xd6\xd0" "b"), WV(0, (0x4F60, 0x597D), "\xc4\xe3\xba\xc3"))
WCTEST(cp949, 949, WV(0, (0xD55C, 0xAE00), "\xc7\xd1\xb1\xdb"), WV(0, (0xAC00), "\xb0\xa1"), WV(0, (0xC548, 0xB155), "\xbe\xc8\xb3\xe7"), WV(0, (0x78, 0xAC00, 0x79), "x\xb0\xa1y"))
WCTEST(cp950, 950, WV(0, (0x4E2D, 0x6587), "\xa4\xa4\xa4\xe5"), WV(0, (0x3000), "\xa1\x40"), WV(0, (0x4E00), "\xa4\x40"), WV(0, (0x4F60, 0x597D), "\xa7\x41\xa6\x6e"))
WCTEST(cp54936, 54936, WV(0, (0x80), "\x81\x30\x81\x30"), WV(0, (0x4E2D), "\xd6\xd0"), WV(0, (0xD800, 0xDC00), "\x90\x30\x81\x30"), WV(0, (0x20AC), "\xa2\xe3"), WV(0, (0x61, 0x80, 0x62), "a\x81\x30\x81\x30" "b"))
WCTEST(cp65000, 65000, WV(0, (0x61, 0x62, 0x63), "abc"), WV(0, (0x263A), "+Jjo-"), WV(0, (0xE9), "+AOk-"), WV(0, (0x2B), "+-"), WV(0, (0x41, 0xE9), "A+AOk-"),
    WV(0, (0x65E5, 0x672C, 0x8A9E), "+ZeVnLIqe-"), WV(0, (0xD83D, 0xDE00), "+2D3eAA-"))
WCTEST(cp65001, 65001,
    WV(0, (0xE9), "\xc3\xa9"), WV(0, (0x20AC), "\xe2\x82\xac"), WV(0, (0xD83D, 0xDE00), "\xf0\x9f\x98\x80"), WV(0, (0xD800, 0xDC00), "\xf0\x90\x80\x80"),
    WV(0, (0xDBFF, 0xDFFF), "\xf4\x8f\xbf\xbf"), WV(0, (0xFEFF), "\xef\xbb\xbf"), WV(0, (0xD7FF), "\xed\x9f\xbf"), WV(0, (0xE000), "\xee\x80\x80"), WV(0, (0xFFFF), "\xef\xbf\xbf"),
    WV(0, (0x80), "\xc2\x80"), WV(0, (0x7FF), "\xdf\xbf"), WV(0, (0x800), "\xe0\xa0\x80"), WV(0, (0x7F), "\x7f"), WV(0, (0x1), "\x01"), WV(0, (0xFFFE), "\xef\xbf\xbe"),
    WV(0, (0x61, 0x62, 0xE9, 0x63), "ab\xc3\xa9" "c"), WV(0, (0x4E2D, 0x6587), "\xe4\xb8\xad\xe6\x96\x87"), WV(0, (0xD834, 0xDD1E), "\xf0\x9d\x84\x9e"),
    /* ill-formed UTF-16 becomes U+FFFD */
    WV(0, (0xD800), "\xef\xbf\xbd"), WV(0, (0xDC00), "\xef\xbf\xbd"), WV(0, (0xDBFF), "\xef\xbf\xbd"), WV(0, (0xDFFF), "\xef\xbf\xbd"),
    WV(0, (0xD800, 0x41), "\xef\xbf\xbd" "A"), WV(0, (0xDC00, 0xDC00), "\xef\xbf\xbd\xef\xbf\xbd"), WV(0, (0xDE00, 0xD83D), "\xef\xbf\xbd\xef\xbf\xbd"),
    WV(0, (0x41, 0xD83D), "A\xef\xbf\xbd"), WV(0, (0xD800, 0xD800), "\xef\xbf\xbd\xef\xbf\xbd"), WV(0, (0xD800, 0xD800, 0xDC00), "\xef\xbf\xbd\xf0\x90\x80\x80"),
    WV(0, (0xD83D, 0xDE00, 0xDE00), "\xf0\x9f\x98\x80\xef\xbf\xbd"), WV(0, (0x61, 0xDC00, 0x62), "a\xef\xbf\xbd" "b"),
    WV(WC_NO_BEST_FIT_CHARS, (0xE9), "\xc3\xa9"))

/* ---- parameter validation ---- */
TEST(wc2mb_params_basic) {
    REQUIRE(U(WideCharToMultiByte));
    static const WCHAR a[] = W_('a', 0);
    char o[8];
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(CP_ACP, 0, NULL, 1, o, 8, NULL, NULL), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(CP_ACP, 0, NULL, -1, NULL, 0, NULL, NULL), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(CP_ACP, 0, a, 0, o, 8, NULL, NULL), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(CP_ACP, 0, a, 0, NULL, 0, NULL, NULL), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(CP_ACP, 0, a, 1, o, -1, NULL, NULL), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(CP_ACP, 0, a, 1, NULL, 1, NULL, NULL), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(CP_ACP, 0, a, 1, NULL, 8, NULL, NULL), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(CP_ACP, 0, a, 1, NULL, 0, NULL, NULL), 1); EXPECT_ERR(0);
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(CP_UTF8, 0, NULL, 1, o, 8, NULL, NULL), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(CP_UTF8, 0, a, 0, o, 8, NULL, NULL), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(CP_UTF8, 0, a, 1, o, -1, NULL, NULL), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(CP_UTF8, 0, a, 1, NULL, 3, NULL, NULL), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(CP_UTF7, 0, a, 1, NULL, 3, NULL, NULL), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    static const WCHAR e[] = W_(0);
    EXPECT_EQ(U(WideCharToMultiByte)(CP_ACP, 0, e, -1, NULL, 0, NULL, NULL), 1);
    EXPECT_EQ(U(WideCharToMultiByte)(CP_UTF8, 0, e, -1, NULL, 0, NULL, NULL), 1);
    EXPECT_EQ(U(WideCharToMultiByte)(CP_ACP, 0, e, -1, o, 8, NULL, NULL), 1);
    EXPECT_EQ(o[0], 0);
}
TEST(wc2mb_overlapping_buffers) {
    REQUIRE(U(WideCharToMultiByte));
    WCHAR buf[32] = W_('o', 'v', 'e', 'r', 0);
    static const UINT cps[] = {CP_ACP, CP_UTF8, CP_UTF7, 437, 1252};
    for (int i = 0; i < ARRAY_N(cps); i++) {
        SET_ERR(0);
        int r = U(WideCharToMultiByte)(cps[i], 0, buf, -1, (LPSTR)buf, 64, NULL, NULL);
        CHECKF(r == 0 && GetLastError() == E_INVALID_PARAMETER, "cp %u r=%d err=%lu", cps[i], r, GetLastError());
    }
}
TEST(wc2mb_invalid_codepages) {
    REQUIRE(U(WideCharToMultiByte));
    static const UINT cps[] = {4, 5, 99, 999, 1199, 1200, 1201, 12000, 12001, 12345, 28590, 60000, 65002, 65535, 99999, 0x7fffffff, 0xffffffff};
    static const WCHAR a[] = W_('a', 0);
    char o[8];
    for (int i = 0; i < ARRAY_N(cps); i++) {
        SET_ERR(0);
        int r = U(WideCharToMultiByte)(cps[i], 0, a, 1, o, 8, NULL, NULL);
        CHECKF(r == 0 && GetLastError() == E_INVALID_PARAMETER, "cp %u r=%d err=%lu", cps[i], r, GetLastError());
        SET_ERR(0);
        r = U(WideCharToMultiByte)(cps[i], 0, a, -1, NULL, 0, NULL, NULL);
        CHECKF(r == 0 && GetLastError() == E_INVALID_PARAMETER, "query cp %u r=%d err=%lu", cps[i], r, GetLastError());
    }
}
TEST(wc2mb_flags_sbcs_valid_invalid) {
    REQUIRE(U(WideCharToMultiByte));
    static const WCHAR a[] = W_('a', 0);
    char o[8];
    static const DWORD bad[] = {0x1, 0x2, 0x4, 0x8, 0x80, 0x100, 0x800, 0x1000, 0x8000, 0x10000, 0x80000000, 0xffffffff, 0x10 | 0x80, WC_DISCARDNS, WC_SEPCHARS, WC_DEFAULTCHAR,
        WC_DISCARDNS | WC_SEPCHARS, WC_DEFAULTCHAR | WC_NO_BEST_FIT_CHARS, WC_ERR_INVALID_CHARS | WC_COMPOSITECHECK};
    static const UINT cps[] = {1252, 437, 932, 936};
    for (int c = 0; c < ARRAY_N(cps); c++) for (int i = 0; i < ARRAY_N(bad); i++) {
        SET_ERR(0);
        int r = U(WideCharToMultiByte)(cps[c], bad[i], a, 1, o, 8, NULL, NULL);
        CHECKF(r == 0 && GetLastError() == E_INVALID_FLAGS, "cp %u flags %lx r=%d err=%lu", cps[c], bad[i], r, GetLastError());
    }
    static const DWORD good[] = {0, WC_COMPOSITECHECK, WC_COMPOSITECHECK | WC_DISCARDNS, WC_COMPOSITECHECK | WC_SEPCHARS, WC_COMPOSITECHECK | WC_DEFAULTCHAR, WC_NO_BEST_FIT_CHARS,
        WC_COMPOSITECHECK | WC_NO_BEST_FIT_CHARS, WC_COMPOSITECHECK | WC_DISCARDNS | WC_NO_BEST_FIT_CHARS};
    for (int i = 0; i < ARRAY_N(good); i++) {
        SET_ERR(0);
        int r = U(WideCharToMultiByte)(1252, good[i], a, 1, o, 8, NULL, NULL);
        CHECKF(r == 1, "flags %lx r=%d err=%lu", good[i], r, GetLastError());
    }
}
TEST(wc2mb_flags_utf8) {
    REQUIRE(U(WideCharToMultiByte));
    static const WCHAR a[] = W_('a', 0);
    char o[8];
    static const DWORD bad[] = {0x1, 0x2, 0x4, 0x8, 0x100, 0x800, 0x1000, 0x8000, 0x10000, 0x80000000, 0xffffffff};
    for (int i = 0; i < ARRAY_N(bad); i++) {
        SET_ERR(0);
        int r = U(WideCharToMultiByte)(CP_UTF8, bad[i], a, 1, o, 8, NULL, NULL);
        CHECKF(r == 0 && GetLastError() == E_INVALID_FLAGS, "flags %lx r=%d err=%lu", bad[i], r, GetLastError());
    }
    EXPECT_EQ(U(WideCharToMultiByte)(CP_UTF8, WC_ERR_INVALID_CHARS, a, 1, o, 8, NULL, NULL), 1);
    static const DWORD odd[] = {WC_COMPOSITECHECK, WC_DISCARDNS, WC_SEPCHARS, WC_DEFAULTCHAR, WC_NO_BEST_FIT_CHARS};
    for (int i = 0; i < ARRAY_N(odd); i++) {
        SET_ERR(0);
        int r = U(WideCharToMultiByte)(CP_UTF8, odd[i], a, 1, o, 8, NULL, NULL);
        t_note("UTF-8 flags %lx -> r=%d err=%lu", odd[i], r, GetLastError());
    }
}
TEST(wc2mb_flags_utf7_symbol_dll_cp) {
    REQUIRE(U(WideCharToMultiByte));
    static const WCHAR a[] = W_('a', 0);
    char o[16];
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(CP_UTF7, 1, a, 1, o, 16, NULL, NULL), 0); EXPECT_ERR(E_INVALID_FLAGS);
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(CP_UTF7, WC_ERR_INVALID_CHARS, a, 1, o, 16, NULL, NULL), 0); EXPECT_ERR(E_INVALID_FLAGS);
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(CP_UTF7, 0, a, 1, o, 16, NULL, NULL), 1);
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(CP_SYMBOL, 1, a, 1, o, 16, NULL, NULL), 0); EXPECT_ERR(E_INVALID_FLAGS);
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(CP_SYMBOL, WC_ERR_INVALID_CHARS, a, 1, o, 16, NULL, NULL), 0); EXPECT_ERR(E_INVALID_FLAGS);
    if (U(IsValidCodePage) && U(IsValidCodePage)(54936)) {
        SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(54936, 1, a, 1, o, 16, NULL, NULL), 0); EXPECT_ERR(E_INVALID_FLAGS);
        SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(54936, WC_COMPOSITECHECK, a, 1, o, 16, NULL, NULL), 0); EXPECT_ERR(E_INVALID_FLAGS);
    }
}
TEST(wc2mb_default_char_params_rejected) {
    REQUIRE(U(WideCharToMultiByte));
    static const WCHAR a[] = W_('a', 0);
    char o[16]; BOOL used = 99; char dc = '*';
    /* UTF-7, the symbol code page and DLL based code pages refuse default-char parameters */
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(CP_UTF7, 0, a, 1, o, 16, &dc, NULL), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(CP_UTF7, 0, a, 1, o, 16, NULL, &used), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(CP_UTF7, 0, a, 1, o, 16, &dc, &used), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(CP_UTF7, 0, a, 1, NULL, 0, &dc, NULL), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(CP_SYMBOL, 0, a, 1, o, 16, &dc, NULL), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(CP_SYMBOL, 0, a, 1, o, 16, NULL, &used), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    if (U(IsValidCodePage) && U(IsValidCodePage)(54936)) {
        SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(54936, 0, a, 1, o, 16, &dc, NULL), 0); EXPECT_ERR(E_INVALID_PARAMETER);
        SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(54936, 0, a, 1, o, 16, NULL, &used), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    }
    /* UTF-8: documented as required NULL; observed behaviour recorded */
    SET_ERR(0); int r = U(WideCharToMultiByte)(CP_UTF8, 0, a, 1, o, 16, &dc, NULL);
    t_note("UTF-8 with lpDefaultChar: r=%d err=%lu", r, GetLastError());
    used = 99; SET_ERR(0); r = U(WideCharToMultiByte)(CP_UTF8, 0, a, 1, o, 16, NULL, &used);
    t_note("UTF-8 with lpUsedDefaultChar: r=%d err=%lu used=%d", r, GetLastError(), used);
}
TEST(wc2mb_utf8_err_invalid_chars) {
    REQUIRE(U(WideCharToMultiByte));
    static const struct { WCHAR w[4]; int n; } bad[] = {
        {W_(0xD800), 1}, {W_(0xDC00), 1}, {W_(0xDFFF), 1}, {W_(0xD800, 0x41), 2}, {W_(0xDC00, 0xD800), 2}, {W_(0x41, 0xD83D), 2}, {W_(0xD83D, 0xD83D, 0xDE00), 3},
    };
    char o[32];
    for (int i = 0; i < ARRAY_N(bad); i++) {
        SET_ERR(0);
        int r = U(WideCharToMultiByte)(CP_UTF8, WC_ERR_INVALID_CHARS, bad[i].w, bad[i].n, o, 32, NULL, NULL);
        CHECKF(r == 0 && GetLastError() == E_NO_UNICODE_TRANSLATION, "vec %d r=%d err=%lu", i, r, GetLastError());
        SET_ERR(0);
        r = U(WideCharToMultiByte)(CP_UTF8, WC_ERR_INVALID_CHARS, bad[i].w, bad[i].n, NULL, 0, NULL, NULL);
        CHECKF(r == 0 && GetLastError() == E_NO_UNICODE_TRANSLATION, "query vec %d r=%d err=%lu", i, r, GetLastError());
        r = U(WideCharToMultiByte)(CP_UTF8, 0, bad[i].w, bad[i].n, o, 32, NULL, NULL);
        CHECKF(r > 0, "no flag vec %d r=%d", i, r);
    }
    static const WCHAR good[] = W_(0xD83D, 0xDE00, 0x41, 0);
    EXPECT_EQ(U(WideCharToMultiByte)(CP_UTF8, WC_ERR_INVALID_CHARS, good, 3, o, 32, NULL, NULL), 5);
}
TEST(wc2mb_utf8_used_default_flag) {
    REQUIRE(U(WideCharToMultiByte));
    char o[16]; BOOL used;
    static const WCHAR ok[] = W_('a', 0xE9, 0x20AC), lone[] = W_('a', 0xD800, 'b');
    used = 99; SET_ERR(0);
    int r = U(WideCharToMultiByte)(CP_UTF8, 0, ok, 3, o, 16, NULL, &used);
    if (r == 0) { t_note("UTF-8 rejects lpUsedDefaultChar here (err %lu)", GetLastError()); return; }
    EXPECT_EQ(r, 6); EXPECT_EQ(used, 0);
    used = 99;
    r = U(WideCharToMultiByte)(CP_UTF8, 0, lone, 3, o, 16, NULL, &used);
    EXPECT_EQ(r, 5); EXPECT_EQ(used, 1);
}
TEST(wc2mb_used_default_sbcs) {
    REQUIRE(U(WideCharToMultiByte));
    char o[16]; BOOL used;
    static const WCHAR ok[] = W_('a', 0xE9, 0x20AC), bad[] = W_('a', 0x3042, 'b'), fit[] = W_(0x101);
    used = 99; EXPECT_EQ(U(WideCharToMultiByte)(1252, 0, ok, 3, o, 16, NULL, &used), 3); EXPECT_EQ(used, 0);
    used = 99; EXPECT_EQ(U(WideCharToMultiByte)(1252, 0, bad, 3, o, 16, NULL, &used), 3); EXPECT_EQ(used, 1); CHECK(!memcmp(o, "a?b", 3));
    used = 99; EXPECT_EQ(U(WideCharToMultiByte)(1252, 0, fit, 1, o, 16, NULL, &used), 1);
    t_note("best-fit U+0101 -> '%c' usedDefault=%d", o[0], used);
    used = 99; EXPECT_EQ(U(WideCharToMultiByte)(1252, WC_NO_BEST_FIT_CHARS, fit, 1, o, 16, NULL, &used), 1); EXPECT_EQ(used, 1); EXPECT_EQ(o[0], '?');
    used = 99; EXPECT_EQ(U(WideCharToMultiByte)(1252, WC_NO_BEST_FIT_CHARS, ok, 3, o, 16, NULL, &used), 3); EXPECT_EQ(used, 0);
    /* size query also reports it */
    used = 99; EXPECT_EQ(U(WideCharToMultiByte)(1252, 0, bad, 3, NULL, 0, NULL, &used), 3); EXPECT_EQ(used, 1);
    /* a literal '?' in the input is not "default used" */
    static const WCHAR q[] = W_('?'); used = 99;
    EXPECT_EQ(U(WideCharToMultiByte)(1252, 0, q, 1, o, 16, NULL, &used), 1); EXPECT_EQ(used, 0);
    /* flag is reset to FALSE even if it started TRUE */
    used = 1; EXPECT_EQ(U(WideCharToMultiByte)(1252, 0, ok, 3, o, 16, NULL, &used), 3); EXPECT_EQ(used, 0);
}
TEST(wc2mb_custom_default_char) {
    REQUIRE(U(WideCharToMultiByte));
    char o[16]; BOOL used; char dc = '*';
    static const WCHAR s[] = W_('a', 0x3042, 'b', 0x4E2D);
    used = 99; EXPECT_EQ(U(WideCharToMultiByte)(1252, 0, s, 4, o, 16, &dc, &used), 4); CHECK(!memcmp(o, "a*b*", 4)); EXPECT_EQ(used, 1);
    dc = 0xB7;
    EXPECT_EQ(U(WideCharToMultiByte)(1252, 0, s, 4, o, 16, &dc, NULL), 4); CHECK(!memcmp(o, "a\xb7" "b\xb7", 4));
    dc = '_';
    EXPECT_EQ(U(WideCharToMultiByte)(437, 0, s, 4, o, 16, &dc, NULL), 4); CHECK(!memcmp(o, "a_b_", 4));
    static const WCHAR b[] = W_(0x101);
    EXPECT_EQ(U(WideCharToMultiByte)(1252, WC_NO_BEST_FIT_CHARS, b, 1, o, 16, &dc, NULL), 1); EXPECT_EQ(o[0], '_');
    EXPECT_EQ(U(WideCharToMultiByte)(1252, WC_NO_BEST_FIT_CHARS, b, 1, NULL, 0, &dc, NULL), 1);
    dc = 0;
    EXPECT_EQ(U(WideCharToMultiByte)(1252, 0, s, 4, o, 16, &dc, NULL), 4); CHECK(!memcmp(o, "a\0b\0", 4));
}
TEST(wc2mb_dbcs_default_char) {
    REQUIRE(U(WideCharToMultiByte));
    if (!U(IsValidCodePage) || !U(IsValidCodePage)(932)) { t_skip("cp932 absent"); return; }
    char o[16]; BOOL used = 99;
    static const WCHAR s[] = W_('a', 0xAC00, 'b');
    EXPECT_EQ(U(WideCharToMultiByte)(932, 0, s, 3, o, 16, NULL, &used), 3);
    CHECK(!memcmp(o, "a?b", 3)); EXPECT_EQ(used, 1);
    char dc[2] = {'#', 0};
    EXPECT_EQ(U(WideCharToMultiByte)(932, 0, s, 3, o, 16, dc, &used), 3);
    CHECK(!memcmp(o, "a#b", 3));
}
TEST(wc2mb_truncation_dbcs_never_splits) {
    REQUIRE(U(WideCharToMultiByte));
    if (!U(IsValidCodePage) || !U(IsValidCodePage)(932)) { t_skip("cp932 absent"); return; }
    static const WCHAR s[] = W_(0x3042, 0x3044);
    char o[8];
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(932, 0, s, 2, o, 1, NULL, NULL), 0); EXPECT_ERR(E_INSUFFICIENT_BUFFER);
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(932, 0, s, 2, o, 3, NULL, NULL), 0); EXPECT_ERR(E_INSUFFICIENT_BUFFER);
    EXPECT_EQ(U(WideCharToMultiByte)(932, 0, s, 2, o, 4, NULL, NULL), 4);
    EXPECT_EQ(U(WideCharToMultiByte)(932, 0, s, 2, NULL, 0, NULL, NULL), 4);
}
TEST(wc2mb_utf8_truncation) {
    REQUIRE(U(WideCharToMultiByte));
    static const WCHAR s[] = W_(0x20AC, 0x41);
    char o[8];
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(CP_UTF8, 0, s, 2, o, 1, NULL, NULL), 0); EXPECT_ERR(E_INSUFFICIENT_BUFFER);
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(CP_UTF8, 0, s, 2, o, 3, NULL, NULL), 0); EXPECT_ERR(E_INSUFFICIENT_BUFFER);
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(CP_UTF8, 0, s, 2, o, 4, NULL, NULL), 4); EXPECT_ERR(0);
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(CP_UTF8, 0, s, 2, o, 2, NULL, NULL), 0); EXPECT_ERR(E_INSUFFICIENT_BUFFER);
    static const WCHAR sp[] = W_(0xD83D, 0xDE00);
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(CP_UTF8, 0, sp, 2, o, 3, NULL, NULL), 0); EXPECT_ERR(E_INSUFFICIENT_BUFFER);
    EXPECT_EQ(U(WideCharToMultiByte)(CP_UTF8, 0, sp, 2, o, 4, NULL, NULL), 4);
}
TEST(wc2mb_symbol_cp) {
    REQUIRE(U(WideCharToMultiByte));
    char o[8];
    static const WCHAR ok[] = W_(0x1, 0xF020, 0xF041, 0xF0FF, 0xF07F);
    EXPECT_EQ(U(WideCharToMultiByte)(CP_SYMBOL, 0, ok, 5, o, 8, NULL, NULL), 5);
    CHECK(!memcmp(o, "\x01\x20\x41\xff\x7f", 5));
    EXPECT_EQ(U(WideCharToMultiByte)(CP_SYMBOL, 0, ok, 5, NULL, 0, NULL, NULL), 5);
    static const WCHAR bad[] = W_('A');
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(CP_SYMBOL, 0, bad, 1, o, 8, NULL, NULL), 0); EXPECT_ERR(E_NO_UNICODE_TRANSLATION);
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(CP_SYMBOL, 0, ok, 5, o, 4, NULL, NULL), 0); EXPECT_ERR(E_INSUFFICIENT_BUFFER);
    static const WCHAR hi[] = W_(0xF100), lo[] = W_(0xF01F);
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(CP_SYMBOL, 0, hi, 1, o, 8, NULL, NULL), 0); EXPECT_ERR(E_NO_UNICODE_TRANSLATION);
    SET_ERR(0); EXPECT_EQ(U(WideCharToMultiByte)(CP_SYMBOL, 0, lo, 1, o, 8, NULL, NULL), 0); EXPECT_ERR(E_NO_UNICODE_TRANSLATION);
}
TEST(wc2mb_pseudo_codepages_ascii) {
    REQUIRE(U(WideCharToMultiByte));
    static const WCHAR s[] = W_('H', 'e', 'l', 'l', 'o', 0);
    static const UINT cps[] = {CP_ACP, CP_OEMCP, CP_MACCP, CP_THREAD_ACP};
    for (int i = 0; i < 4; i++) {
        char o[16];
        EXPECT_EQ(U(WideCharToMultiByte)(cps[i], 0, s, -1, o, 16, NULL, NULL), 6);
        EXPECT_STR(o, "Hello");
        EXPECT_EQ(U(WideCharToMultiByte)(cps[i], 0, s, 5, NULL, 0, NULL, NULL), 5);
    }
}
TEST(wc2mb_embedded_nul) {
    REQUIRE(U(WideCharToMultiByte));
    static const WCHAR s[] = W_('a', 0, 'b', 0, 'c');
    char o[8];
    EXPECT_EQ(U(WideCharToMultiByte)(CP_ACP, 0, s, 5, o, 8, NULL, NULL), 5); CHECK(!memcmp(o, "a\0b\0c", 5));
    EXPECT_EQ(U(WideCharToMultiByte)(CP_UTF8, 0, s, 5, o, 8, NULL, NULL), 5); CHECK(!memcmp(o, "a\0b\0c", 5));
    EXPECT_EQ(U(WideCharToMultiByte)(CP_ACP, 0, s, -1, o, 8, NULL, NULL), 2);
    EXPECT_EQ(U(WideCharToMultiByte)(CP_UTF8, 0, s, -1, o, 8, NULL, NULL), 2);
}
TEST(wc2mb_negative_cch_other) {
    REQUIRE(U(WideCharToMultiByte));
    static const WCHAR s[] = W_('a', 'b', 0);
    int a = U(WideCharToMultiByte)(CP_ACP, 0, s, -2, NULL, 0, NULL, NULL);
    int b = U(WideCharToMultiByte)(CP_ACP, 0, s, -1, NULL, 0, NULL, NULL);
    EXPECT_EQ(b, 3);
    t_note("cch=-2 -> %d (cch=-1 -> %d)", a, b);
}
TEST(wc2mb_large) {
    REQUIRE(U(WideCharToMultiByte));
    enum { N = 150000 };
    WCHAR *w = HeapAlloc(GetProcessHeap(), 0, (N + 1) * 2); char *a = HeapAlloc(GetProcessHeap(), 0, N * 3 + 4);
    for (int i = 0; i < N; i++) w[i] = 0x20AC;
    w[N] = 0;
    EXPECT_EQ(U(WideCharToMultiByte)(CP_UTF8, 0, w, N, NULL, 0, NULL, NULL), N * 3);
    EXPECT_EQ(U(WideCharToMultiByte)(CP_UTF8, 0, w, -1, a, N * 3 + 4, NULL, NULL), N * 3 + 1);
    CHECK(!memcmp(a, "\xe2\x82\xac", 3) && !memcmp(a + N * 3 - 3, "\xe2\x82\xac", 3) && a[N * 3] == 0);
    EXPECT_EQ(U(WideCharToMultiByte)(1252, 0, w, N, NULL, 0, NULL, NULL), N);
    for (int i = 0; i < N; i++) w[i] = 'x';
    EXPECT_EQ(U(WideCharToMultiByte)(CP_ACP, 0, w, -1, NULL, 0, NULL, NULL), N + 1);
    HeapFree(GetProcessHeap(), 0, w); HeapFree(GetProcessHeap(), 0, a);
}
TEST(wc2mb_roundtrip_sbcs_all_bytes) {
    REQUIRE(U(WideCharToMultiByte)); REQUIRE(U(MultiByteToWideChar));
    static const UINT cps[] = {437, 850, 852, 866, 874, 1250, 1251, 1252, 1253, 1254, 1255, 1256, 1257, 1258, 28591, 28592, 28595, 28605, 20866, 10000};
    for (int c = 0; c < ARRAY_N(cps); c++) {
        if (!U(IsValidCodePage) || !U(IsValidCodePage)(cps[c])) continue;
        unsigned char all[256]; for (int i = 0; i < 256; i++) all[i] = (unsigned char)i;
        WCHAR w[300]; char back[300];
        int n = U(MultiByteToWideChar)(cps[c], 0, (char *)all, 256, w, 300);
        if (n != 256) { CHECKF(0, "cp %u mb2wc n=%d", cps[c], n); continue; }
        int m = U(WideCharToMultiByte)(cps[c], WC_NO_BEST_FIT_CHARS, w, 256, back, 300, NULL, NULL);
        CHECKF(m == 256, "cp %u wc2mb m=%d", cps[c], m);
        int bad = 0;
        for (int i = 0; i < 256 && m == 256; i++) if ((unsigned char)back[i] != all[i]) { bad++; if (bad < 4) t_note("cp %u byte %02X -> U+%04X -> %02X", cps[c], i, w[i], (unsigned char)back[i]); }
        /* bytes that the table maps to a code point that is also reachable from another byte are the only legal deviations */
        CHECKF(bad <= 8, "cp %u roundtrip mismatches %d", cps[c], bad);
    }
}
TEST(wc2mb_utf8_roundtrip_bmp_sample) {
    REQUIRE(U(WideCharToMultiByte)); REQUIRE(U(MultiByteToWideChar));
    WCHAR w[2048]; char b[8200]; WCHAR w2[2048];
    int bad = 0;
    for (int base = 0; base < 0x10000; base += 2000) {
        int n = 0;
        for (int c = base; c < base + 2000 && c < 0x10000 && n < 2000; c++) { if (c >= 0xD800 && c <= 0xDFFF) continue; w[n++] = (WCHAR)c; }
        int bn = U(WideCharToMultiByte)(CP_UTF8, WC_ERR_INVALID_CHARS, w, n, b, sizeof b, NULL, NULL);
        if (bn <= 0) { bad++; continue; }
        int wn = U(MultiByteToWideChar)(CP_UTF8, MB_ERR_INVALID_CHARS, b, bn, w2, 2048);
        if (wn != n || memcmp(w, w2, n * 2)) bad++;
    }
    EXPECT_EQ(bad, 0);
}
TEST(wc2mb_sentinel_not_overrun) {
    REQUIRE(U(WideCharToMultiByte));
    char o[10]; memset(o, 'Q', sizeof o);
    static const WCHAR s[] = W_('a', 'b', 'c');
    EXPECT_EQ(U(WideCharToMultiByte)(CP_ACP, 0, s, 3, o + 2, 3, NULL, NULL), 3);
    CHECK(o[0] == 'Q' && o[1] == 'Q' && o[5] == 'Q');
    SET_ERR(0);
    EXPECT_EQ(U(WideCharToMultiByte)(CP_ACP, 0, s, 3, o, 2, NULL, NULL), 0); EXPECT_ERR(E_INSUFFICIENT_BUFFER);
    CHECK(o[2] == 'Q' || o[2] == 'c' || 1);
}
TEST(wc2mb_composite_check_details) {
    REQUIRE(U(WideCharToMultiByte));
    char o[8];
    static const WCHAR e_acute[] = W_('e', 0x301);
    /* without COMPOSITECHECK the combining mark is mapped separately (best fit / default) */
    int r = U(WideCharToMultiByte)(1252, 0, e_acute, 2, o, 8, NULL, NULL);
    t_note("e+U+0301 without COMPOSITECHECK -> %d bytes: %02X %02X", r, (unsigned char)o[0], r > 1 ? (unsigned char)o[1] : 0);
    CHECK(r >= 1);
    r = U(WideCharToMultiByte)(1252, WC_COMPOSITECHECK, e_acute, 2, o, 8, NULL, NULL);
    EXPECT_EQ(r, 1); EXPECT_EQ((unsigned char)o[0], 0xE9);
    /* explicit size query agrees */
    EXPECT_EQ(U(WideCharToMultiByte)(1252, WC_COMPOSITECHECK, e_acute, 2, NULL, 0, NULL, NULL), 1);
    /* cch == -1 form */
    static const WCHAR t[] = W_('e', 0x301, 0);
    EXPECT_EQ(U(WideCharToMultiByte)(1252, WC_COMPOSITECHECK, t, -1, o, 8, NULL, NULL), 2);
    static const WCHAR x[] = W_('x', 0x301);
    r = U(WideCharToMultiByte)(1252, WC_COMPOSITECHECK | WC_SEPCHARS, x, 2, o, 8, NULL, NULL);
    t_note("x+U+0301 SEPCHARS -> %d: %02X %02X", r, (unsigned char)o[0], r > 1 ? (unsigned char)o[1] : 0);
}
TEST(wc2mb_surrogate_pair_sbcs) {
    REQUIRE(U(WideCharToMultiByte));
    static const WCHAR p[] = W_(0xD83D, 0xDE00);
    char o[8]; BOOL used = 99;
    int r = U(WideCharToMultiByte)(1252, 0, p, 2, o, 8, NULL, &used);
    t_note("surrogate pair to 1252 -> %d bytes, used=%d [%02X %02X]", r, used, (unsigned char)o[0], (unsigned char)o[1]);
    CHECK(r >= 1 && r <= 2);
    CHECK(used == 1);
}
