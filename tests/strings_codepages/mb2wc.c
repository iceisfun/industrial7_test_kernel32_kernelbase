/* MultiByteToWideChar */
#include "sc.h"

typedef struct { DWORD fl; const char *src; int n; WCHAR w[12]; } mbv_t;
#define MBV(fl, s, ...) { (fl), (s), (int)(sizeof((WCHAR[]){__VA_ARGS__}) / sizeof(WCHAR)), { __VA_ARGS__ } }

static int mb_soft;
#define SCHK(c, ...) do { if (mb_soft) { if (!(c)) t_note("soft: " __VA_ARGS__); } else CHECKF(c, __VA_ARGS__); } while (0)
#define SERR(e) do { if (mb_soft) { if (GetLastError() != (e)) t_note("soft: lasterror %lu want %d", GetLastError(), (int)(e)); } else EXPECT_ERR(e); } while (0)
static int mb_table(UINT cp, const mbv_t *v, int nv, const char *tag) {
    if (!U(MultiByteToWideChar)) { t_skip("missing export: MultiByteToWideChar"); return 0; }
    SET_ERR(0);
    int probe = U(MultiByteToWideChar)(cp, 0, "A", 1, NULL, 0);
    if (probe != 1) { t_note("%s: code page %u not usable here (probe=%d err=%lu)", tag, cp, probe, GetLastError()); t_skip("cp unavailable"); return 0; }
    for (int i = 0; i < nv; i++) {
        int sl = (int)strlen(v[i].src);
        WCHAR out[40];
        int q = U(MultiByteToWideChar)(cp, v[i].fl, v[i].src, sl, NULL, 0);
        SCHK(q == v[i].n, "%s vec %d size query got %d want %d", tag, i, q, v[i].n);
        for (int k = 0; k < 40; k++) out[k] = 0x7777;
        SET_ERR(0);
        int r = U(MultiByteToWideChar)(cp, v[i].fl, v[i].src, sl, out, 40);
        SCHK(r == v[i].n, "%s vec %d convert got %d want %d", tag, i, r, v[i].n);
        if (r == v[i].n) {
            char g[256], e[256]; sc_wdump(g, sizeof g, out, r); sc_wdump(e, sizeof e, v[i].w, r);
            SCHK(sc_weq(out, v[i].w, r), "%s vec %d content got [%s] want [%s]", tag, i, g, e);
            SCHK(out[r] == 0x7777, "%s vec %d wrote past result", tag, i);
        }
        /* cbMultiByte = -1 includes the terminator */
        for (int k = 0; k < 40; k++) out[k] = 0x7777;
        r = U(MultiByteToWideChar)(cp, v[i].fl, v[i].src, -1, out, 40);
        SCHK(r == v[i].n + 1, "%s vec %d -1 got %d want %d", tag, i, r, v[i].n + 1);
        if (r == v[i].n + 1) { SCHK(out[r - 1] == 0, "%s vec %d NUL", tag, i); SCHK(sc_weq(out, v[i].w, v[i].n), "%s vec %d content(-1)", tag, i); }
        /* exact-1 buffer fails with ERROR_INSUFFICIENT_BUFFER (when it is not a size query) */
        if (v[i].n >= 2) {
            SET_ERR(0);
            r = U(MultiByteToWideChar)(cp, v[i].fl, v[i].src, sl, out, v[i].n - 1);
            SCHK(r == 0, "%s vec %d short buffer returned %d", tag, i, r);
            SERR(E_INSUFFICIENT_BUFFER);
        }
        /* -1 with buffer lacking room for the NUL */
        if (v[i].n >= 1) {
            SET_ERR(0);
            r = U(MultiByteToWideChar)(cp, v[i].fl, v[i].src, -1, out, v[i].n);
            SCHK(r == 0, "%s vec %d -1 short got %d", tag, i, r);
            SERR(E_INSUFFICIENT_BUFFER);
        }
    }
    return 1;
}
#define MBTEST(name, cp, ...) \
    static const mbv_t name##_v[] = { __VA_ARGS__ }; \
    TEST(mb2wc_##name) { mb_table(cp, name##_v, ARRAY_N(name##_v), #name); }

#define MBTEST_SOFT(name, cp, ...) \
    static const mbv_t name##_v[] = { __VA_ARGS__ }; \
    TEST(mb2wc_##name) { mb_soft = 1; mb_table(cp, name##_v, ARRAY_N(name##_v), #name); mb_soft = 0; }

MBTEST(cp437, 437,
    MBV(0, "\x80\x81\x82\x83", 0xC7, 0xFC, 0xE9, 0xE2), MBV(0, "\xe1\xe2\xe3\xe4\xe5", 0xDF, 0x393, 0x3C0, 0x3A3, 0x3C3),
    MBV(0, "\xfb\xfc\xfd\xfe\xff", 0x221A, 0x207F, 0xB2, 0x25A0, 0xA0), MBV(0, "\xb0\xb1\xb2\xdb", 0x2591, 0x2592, 0x2593, 0x2588),
    MBV(0, "\xc4\xb3\xda\xbf", 0x2500, 0x2502, 0x250C, 0x2510), MBV(0, "ABC xyz", 'A', 'B', 'C', ' ', 'x', 'y', 'z'),
    MBV(0, "\x01\x1b\x7f", 1, 0x1b, 0x7f), MBV(4, "\x01\x02\x03\x0f\x7f", 0x263A, 0x263B, 0x2665, 0x263C, 0x2302),
    MBV(1, "\x80", 0xC7), MBV(0, "\xec\xed\xee", 0x221E, 0x3C6, 0x3B5))
MBTEST(cp850, 850,
    MBV(0, "\x9b\x9d\xd5\xe0", 0xF8, 0xD8, 0x131, 0xD3), MBV(0, "\xb5\xb6\xc7", 0xC1, 0xC2, 0xC3), MBV(0, "\x80\x84\xa5", 0xC7, 0xE4, 0xD1),
    MBV(0, "\xe1\xfa\xff", 0xDF, 0xB7, 0xA0))
MBTEST(cp852, 852, MBV(0, "\x80\xa0\x9f", 0xC7, 0xE1, 0x10D), MBV(0, "\xa4\xa5", 0x104, 0x105))
MBTEST(cp866, 866, MBV(0, "\x80\xa0\xe0\xf0", 0x410, 0x430, 0x440, 0x401), MBV(0, "\x8f\x90", 0x41F, 0x420))
MBTEST(cp874, 874, MBV(0, "\xa1\xdf\x80", 0xE01, 0xE3F, 0x20AC), MBV(0, "\x85\x91", 0x2026, 0x2018), MBV(0, "ab", 'a', 'b'))
MBTEST(cp1250, 1250,
    MBV(0, "\x8a\x8c\x8d\x8e\x8f", 0x160, 0x15A, 0x164, 0x17D, 0x179), MBV(0, "\xb9\xe8\xa3", 0x105, 0x10D, 0x141), MBV(0, "\x80", 0x20AC),
    MBV(0, "\xc0\xe0", 0x154, 0x155), MBV(0, "\xd8\xf8", 0x158, 0x159))
MBTEST(cp1251, 1251,
    MBV(0, "\xc0\xe0\xa8\xb8", 0x410, 0x430, 0x401, 0x451), MBV(0, "\x80\x88\xff", 0x402, 0x20AC, 0x44F), MBV(0, "\xdf\xfe", 0x42F, 0x44E),
    MBV(0, "\xb9", 0x2116))
MBTEST(cp1252, 1252,
    MBV(0, "\x80\x82\x83\x84\x85", 0x20AC, 0x201A, 0x192, 0x201E, 0x2026), MBV(0, "\x8a\x9a\x8e\x9e\x9f", 0x160, 0x161, 0x17D, 0x17E, 0x178),
    MBV(0, "\x91\x92\x93\x94\x95\x96\x97", 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014), MBV(0, "\x99", 0x2122),
    MBV(0, "\x81\x8d\x8f\x90\x9d", 0x81, 0x8D, 0x8F, 0x90, 0x9D), MBV(0, "\xa0\xe9\xff", 0xA0, 0xE9, 0xFF),
    MBV(0, "\x8b\x9b\x8c\x9c", 0x2039, 0x203A, 0x152, 0x153), MBV(0, "\x86\x87\x88\x89", 0x2020, 0x2021, 0x2C6, 0x2030),
    MBV(0, "\x98\x9a", 0x2DC, 0x161), MBV(0, "plain ascii", 'p', 'l', 'a', 'i', 'n', ' ', 'a', 's', 'c', 'i', 'i'),
    MBV(MB_PRECOMPOSED, "\xe9", 0xE9), MBV(MB_COMPOSITE, "\xe9", 'e', 0x301), MBV(MB_COMPOSITE, "\xc5", 'A', 0x30A),
    MBV(MB_COMPOSITE, "\xf1", 'n', 0x303), MBV(MB_COMPOSITE, "a\xfc" "b", 'a', 'u', 0x308, 'b'), MBV(MB_COMPOSITE, "\xc7", 'C', 0x327),
    MBV(MB_COMPOSITE, "\xa0\xdf", 0xA0, 0xDF), MBV(MB_USEGLYPHCHARS, "\xe9", 0xE9), MBV(MB_PRECOMPOSED | MB_USEGLYPHCHARS, "\xe9", 0xE9))
MBTEST(cp1253, 1253, MBV(0, "\xc1\xe1\xa1\xa2\xd9\xf9", 0x391, 0x3B1, 0x385, 0x386, 0x3A9, 0x3C9), MBV(0, "\xa3\xb6", 0xA3, 0xB6),MBV(0, "\xc0", 0x390))
MBTEST(cp1254, 1254, MBV(0, "\xd0\xdd\xde\xf0\xfd\xfe", 0x11E, 0x130, 0x15E, 0x11F, 0x131, 0x15F), MBV(0, "\xe7\xfc", 0xE7, 0xFC))
MBTEST(cp1255, 1255, MBV(0, "\xe0\xfa\xa4\xc0", 0x5D0, 0x5EA, 0x20AA, 0x5B0), MBV(0, "\xfd\xfe", 0x200E, 0x200F))
MBTEST(cp1256, 1256, MBV(0, "\xc7\xe3\x81\xff\x8d", 0x627, 0x645, 0x67E, 0x6D2, 0x686), MBV(0, "\x80\xa1", 0x20AC, 0x60C))
MBTEST(cp1257, 1257, MBV(0, "\xc0\xe0\xd0\xf0", 0x104, 0x105, 0x160, 0x161), MBV(0, "\xde\xfe", 0x17D, 0x17E))
MBTEST(cp1258, 1258, MBV(0, "\xc3\xd0\xdd\xfd", 0x102, 0x110, 0x1AF, 0x1B0), MBV(0, "\xe3\xf0", 0x103, 0x111))
MBTEST(cp28591, 28591, MBV(0, "\x80\xa0\xe9\xff", 0x80, 0xA0, 0xE9, 0xFF), MBV(0, "\x01\x7f\x9f", 1, 0x7f, 0x9f))
MBTEST(cp28592, 28592, MBV(0, "\xa1\xb1\xe8\xa9", 0x104, 0x105, 0x10D, 0x160), MBV(0, "\xa3\xb3", 0x141, 0x142))
MBTEST(cp28595, 28595, MBV(0, "\xb0\xd0\xf0", 0x410, 0x430, 0x2116), MBV(0, "\xa1\xf1", 0x401, 0x451))
MBTEST(cp28597, 28597, MBV(0, "\xc1\xe1", 0x391, 0x3B1), MBV(0, "\xd9\xf9", 0x3A9, 0x3C9))
MBTEST(cp28605, 28605, MBV(0, "\xa4\xbc\xbd\xbe", 0x20AC, 0x152, 0x153, 0x178), MBV(0, "\xa6\xa8", 0x160, 0x161))
MBTEST(cp20866, 20866, MBV(0, "\xc1\xe1\xa3", 0x430, 0x410, 0x451), MBV(0, "\xd0\xf0", 0x43F, 0x41F))
MBTEST(cp10000, 10000, MBV(0, "\x80\x8e\xa5\xd0", 0xC4, 0xE9, 0x2022, 0x2013), MBV(0, "\xc9\xa5", 0x2026, 0x2022))
MBTEST(cp20127, 20127, MBV(0, "ASCII 123", 'A', 'S', 'C', 'I', 'I', ' ', '1', '2', '3'), MBV(0, "\x01\x7f", 1, 0x7f))
MBTEST(cp932, 932,
    MBV(0, "\x82\xa0", 0x3042), MBV(0, "\x83\x41", 0x30A2), MBV(0, "\x93\xfa\x96\x7b", 0x65E5, 0x672C), MBV(0, "\xb1", 0xFF71), MBV(0, "\x81\x40", 0x3000),
    MBV(0, "\x5c\x7e", 0x5C, 0x7E), MBV(0, "\xa1\xdf", 0xFF61, 0xFF9F), MBV(0, "\x81\x7e", 0xD7), MBV(0, "\x87\x40", 0x2460), MBV(0, "\xfa\x40", 0x2170),
    MBV(0, "a\x82\xa0" "b", 'a', 0x3042, 'b'), MBV(0, "\x82\xa0\x82\xa2", 0x3042, 0x3044), MBV(0, "\x88\x9f", 0x4E9C))
MBTEST(cp936, 936, MBV(0, "\xd6\xd0\xce\xc4", 0x4E2D, 0x6587), MBV(0, "\xa1\xa1", 0x3000), MBV(0, "\xb0\xa1", 0x554A), MBV(0, "a\xd6\xd0" "b", 'a', 0x4E2D, 'b'), MBV(0, "\xc4\xe3\xba\xc3", 0x4F60, 0x597D))
MBTEST(cp949, 949, MBV(0, "\xc7\xd1\xb1\xdb", 0xD55C, 0xAE00), MBV(0, "\xb0\xa1", 0xAC00), MBV(0, "\xbe\xc8\xb3\xe7", 0xC548, 0xB155), MBV(0, "x\xb0\xa1y", 'x', 0xAC00, 'y'))
MBTEST(cp950, 950, MBV(0, "\xa4\xa4\xa4\xe5", 0x4E2D, 0x6587), MBV(0, "\xa1\x40", 0x3000), MBV(0, "\xa4\x40", 0x4E00), MBV(0, "\xa7\x41\xa6\x6e", 0x4F60, 0x597D))
MBTEST(cp54936, 54936, MBV(0, "\x81\x30\x81\x30", 0x80), MBV(0, "\xd6\xd0", 0x4E2D), MBV(0, "\x90\x30\x81\x30", 0xD800, 0xDC00), MBV(0, "\xa2\xe3", 0x20AC), MBV(0, "a\x81\x30\x81\x30" "b", 'a', 0x80, 'b'))
MBTEST(cp65000, 65000,
    MBV(0, "A+AOk-", 'A', 0xE9), MBV(0, "+Jjo-", 0x263A), MBV(0, "a+-b", 'a', '+', 'b'), MBV(0, "Hi Mom -+Jjo--!", 'H', 'i', ' ', 'M', 'o', 'm', ' ', '-', 0x263A, '-', '!'),
    MBV(0, "+ZeVnLIqe-", 0x65E5, 0x672C, 0x8A9E), MBV(0, "plain", 'p', 'l', 'a', 'i', 'n'), MBV(0, "+2D3eAA-", 0xD83D, 0xDE00), MBV(0, "+AOkA6QDp-", 0xE9, 0xE9, 0xE9))
MBTEST(cp65001, 65001,
    MBV(0, "\xc3\xa9", 0xE9), MBV(0, "\xe2\x82\xac", 0x20AC), MBV(0, "\xf0\x9f\x98\x80", 0xD83D, 0xDE00), MBV(0, "\xf0\x90\x80\x80", 0xD800, 0xDC00),
    MBV(0, "\xf4\x8f\xbf\xbf", 0xDBFF, 0xDFFF), MBV(0, "\xef\xbb\xbf", 0xFEFF), MBV(0, "\xed\x9f\xbf", 0xD7FF), MBV(0, "\xee\x80\x80", 0xE000),
    MBV(0, "\xef\xbf\xbf", 0xFFFF), MBV(0, "\xc2\x80", 0x80), MBV(0, "\xdf\xbf", 0x7FF), MBV(0, "\xe0\xa0\x80", 0x800), MBV(0, "\xc2\xa0", 0xA0),
    MBV(0, "ab\xc3\xa9" "cd", 'a', 'b', 0xE9, 'c', 'd'), MBV(0, "\xe4\xb8\xad\xe6\x96\x87", 0x4E2D, 0x6587), MBV(0, "\xef\xbf\xbd", 0xFFFD),
    MBV(0, "\xef\xb7\x90", 0xFDD0), MBV(0, "\xef\xbf\xbe", 0xFFFE), MBV(0, "\xf3\xa0\x80\x80", 0xDB40, 0xDC00), MBV(0, "\xf0\x9d\x84\x9e", 0xD834, 0xDD1E),
    MBV(MB_ERR_INVALID_CHARS, "\xc3\xa9", 0xE9), MBV(MB_ERR_INVALID_CHARS, "\xf0\x9f\x98\x80", 0xD83D, 0xDE00))

MBTEST_SOFT(cp65001_illformed, 65001,
    MBV(0, "\x80", 0xFFFD), MBV(0, "\xbf", 0xFFFD), MBV(0, "\xc0\x80", 0xFFFD, 0xFFFD), MBV(0, "\xc1\xbf", 0xFFFD, 0xFFFD),
    MBV(0, "\xe0\x80\x80", 0xFFFD, 0xFFFD, 0xFFFD), MBV(0, "\xe0\x9f\xbf", 0xFFFD, 0xFFFD, 0xFFFD), MBV(0, "\xed\xa0\x80", 0xFFFD, 0xFFFD, 0xFFFD),
    MBV(0, "\xed\xbf\xbf", 0xFFFD, 0xFFFD, 0xFFFD), MBV(0, "\xf4\x90\x80\x80", 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD), MBV(0, "\xf5\x80\x80\x80", 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD),
    MBV(0, "\xf8\x88\x80\x80\x80", 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD), MBV(0, "\xfc\x84\x80\x80\x80\x80", 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD),
    MBV(0, "\xfe", 0xFFFD), MBV(0, "\xff", 0xFFFD), MBV(0, "\xfe\xff", 0xFFFD, 0xFFFD),
    MBV(0, "\xe2\x82", 0xFFFD), MBV(0, "\xe2\x82" "A", 0xFFFD, 'A'), MBV(0, "\xf0\x9f\x98", 0xFFFD), MBV(0, "\xf0\x9f\x98" "A", 0xFFFD, 'A'),
    MBV(0, "\xf0\x9f", 0xFFFD), MBV(0, "\xc3", 0xFFFD), MBV(0, "\xc3" "A", 0xFFFD, 'A'), MBV(0, "\xe2" "(\xa1", 0xFFFD, '(', 0xFFFD),
    MBV(0, "a\x80" "b", 'a', 0xFFFD, 'b'), MBV(0, "\xc3\xa9\xc3", 0xE9, 0xFFFD), MBV(0, "\xc3\x28", 0xFFFD, '('), MBV(0, "\x80\x80\x80", 0xFFFD, 0xFFFD, 0xFFFD),
    MBV(0, "\xc2\xc2\xa0", 0xFFFD, 0xA0), MBV(0, "\xe0\xa0\xe0\xa0\x80", 0xFFFD, 0x800))

/* ---- parameter validation ---- */
TEST(mb2wc_params_basic) {
    REQUIRE(U(MultiByteToWideChar));
    WCHAR w[8];
    SET_ERR(0); EXPECT_EQ(U(MultiByteToWideChar)(CP_ACP, 0, NULL, 1, w, 8), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(MultiByteToWideChar)(CP_ACP, 0, NULL, -1, NULL, 0), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(MultiByteToWideChar)(CP_ACP, 0, "a", 0, w, 8), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(MultiByteToWideChar)(CP_ACP, 0, "a", 0, NULL, 0), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(MultiByteToWideChar)(CP_ACP, 0, "a", 1, w, -1), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(MultiByteToWideChar)(CP_ACP, 0, "a", 1, NULL, 1), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(MultiByteToWideChar)(CP_ACP, 0, "a", 1, NULL, 8), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(MultiByteToWideChar)(CP_ACP, 0, "a", 1, NULL, 0), 1); EXPECT_ERR(0);
    SET_ERR(0); EXPECT_EQ(U(MultiByteToWideChar)(CP_ACP, 0, "", -1, NULL, 0), 1);
    SET_ERR(0); EXPECT_EQ(U(MultiByteToWideChar)(CP_ACP, 0, "", -1, w, 8), 1); EXPECT_EQ(w[0], 0);
    SET_ERR(0); EXPECT_EQ(U(MultiByteToWideChar)(CP_UTF8, 0, "", -1, NULL, 0), 1);
}
TEST(mb2wc_overlapping_buffers) {
    REQUIRE(U(MultiByteToWideChar));
    char buf[64] = "overlap test";
    SET_ERR(0);
    EXPECT_EQ(U(MultiByteToWideChar)(CP_ACP, 0, buf, -1, (LPWSTR)buf, 32), 0);
    EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0);
    EXPECT_EQ(U(MultiByteToWideChar)(CP_UTF8, 0, buf, -1, (LPWSTR)buf, 32), 0);
    EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0);
    EXPECT_EQ(U(MultiByteToWideChar)(CP_UTF7, 0, buf, -1, (LPWSTR)buf, 32), 0);
    EXPECT_ERR(E_INVALID_PARAMETER);
}
TEST(mb2wc_invalid_codepages) {
    REQUIRE(U(MultiByteToWideChar));
    static const UINT cps[] = {4, 5, 99, 100, 999, 1199, 1200, 1201, 1202, 12000, 12001, 12345, 20000 - 1, 28590, 60000, 65002, 65535, 99999, 0x7fffffff, 0xffffffff, 0xfffffffe};
    WCHAR w[8];
    for (int i = 0; i < ARRAY_N(cps); i++) {
        SET_ERR(0);
        int r = U(MultiByteToWideChar)(cps[i], 0, "a", 1, w, 8);
        CHECKF(r == 0 && GetLastError() == E_INVALID_PARAMETER, "cp %u r=%d err=%lu", cps[i], r, GetLastError());
        SET_ERR(0);
        r = U(MultiByteToWideChar)(cps[i], 0, "a", -1, NULL, 0);
        CHECKF(r == 0 && GetLastError() == E_INVALID_PARAMETER, "query cp %u r=%d err=%lu", cps[i], r, GetLastError());
    }
}
TEST(mb2wc_invalid_flags_sbcs) {
    REQUIRE(U(MultiByteToWideChar));
    static const DWORD fl[] = {0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x8000, 0x10000, 0x80000000, 0xffffffff, 0x13, 0x18};
    WCHAR w[8];
    for (int i = 0; i < ARRAY_N(fl); i++) {
        SET_ERR(0);
        int r = U(MultiByteToWideChar)(1252, fl[i], "a", 1, w, 8);
        CHECKF(r == 0 && GetLastError() == E_INVALID_FLAGS, "flags %lx r=%d err=%lu", fl[i], r, GetLastError());
        SET_ERR(0);
        r = U(MultiByteToWideChar)(437, fl[i], "a", 1, NULL, 0);
        CHECKF(r == 0 && GetLastError() == E_INVALID_FLAGS, "437 flags %lx r=%d err=%lu", fl[i], r, GetLastError());
    }
    SET_ERR(0); EXPECT_EQ(U(MultiByteToWideChar)(1252, MB_PRECOMPOSED | MB_COMPOSITE, "a", 1, w, 8), 0); EXPECT_ERR(E_INVALID_FLAGS);
    /* valid single flags */
    EXPECT_EQ(U(MultiByteToWideChar)(1252, MB_PRECOMPOSED, "a", 1, w, 8), 1);
    EXPECT_EQ(U(MultiByteToWideChar)(1252, MB_COMPOSITE, "a", 1, w, 8), 1);
    EXPECT_EQ(U(MultiByteToWideChar)(1252, MB_USEGLYPHCHARS, "a", 1, w, 8), 1);
    EXPECT_EQ(U(MultiByteToWideChar)(1252, MB_ERR_INVALID_CHARS, "a", 1, w, 8), 1);
}
TEST(mb2wc_invalid_flags_utf8_utf7) {
    REQUIRE(U(MultiByteToWideChar));
    WCHAR w[8];
    static const DWORD bad[] = {0x10, 0x20, 0x40, 0x80, 0x100, 0x400, 0x1000, 0x80000000, 0xffffffff};
    for (int i = 0; i < ARRAY_N(bad); i++) {
        SET_ERR(0); int r = U(MultiByteToWideChar)(CP_UTF8, bad[i], "a", 1, w, 8);
        CHECKF(r == 0 && GetLastError() == E_INVALID_FLAGS, "utf8 flags %lx r=%d err=%lu", bad[i], r, GetLastError());
    }
    static const DWORD bad7[] = {1, 2, 4, 8, 0x10, 0x80, 0xffffffff};
    for (int i = 0; i < ARRAY_N(bad7); i++) {
        SET_ERR(0); int r = U(MultiByteToWideChar)(CP_UTF7, bad7[i], "a", 1, w, 8);
        CHECKF(r == 0 && GetLastError() == E_INVALID_FLAGS, "utf7 flags %lx r=%d err=%lu", bad7[i], r, GetLastError());
    }
    EXPECT_EQ(U(MultiByteToWideChar)(CP_UTF8, MB_ERR_INVALID_CHARS, "a", 1, w, 8), 1);
    EXPECT_EQ(U(MultiByteToWideChar)(CP_UTF7, 0, "a", 1, w, 8), 1);
    SET_ERR(0); int r = U(MultiByteToWideChar)(CP_UTF8, MB_PRECOMPOSED, "a", 1, w, 8);
    t_note("UTF-8 with MB_PRECOMPOSED: r=%d err=%lu", r, GetLastError());
    SET_ERR(0); r = U(MultiByteToWideChar)(CP_UTF8, MB_COMPOSITE, "a", 1, w, 8);
    t_note("UTF-8 with MB_COMPOSITE: r=%d err=%lu", r, GetLastError());
}
TEST(mb2wc_utf8_err_invalid_chars) {
    REQUIRE(U(MultiByteToWideChar));
    static const char *bad[] = {"\x80", "\xbf", "\xc0\x80", "\xc1\xbf", "\xe0\x80\x80", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xf8\x88\x80\x80\x80", "\xfe", "\xff",
        "\xe2\x82", "\xc3", "\xf0\x9f\x98", "a\x80", "\xc3\x28", "ab\xe2\x82" "c"};
    WCHAR w[16];
    for (int i = 0; i < ARRAY_N(bad); i++) {
        int sl = (int)strlen(bad[i]);
        SET_ERR(0);
        int r = U(MultiByteToWideChar)(CP_UTF8, MB_ERR_INVALID_CHARS, bad[i], sl, w, 16);
        CHECKF(r == 0 && GetLastError() == E_NO_UNICODE_TRANSLATION, "vec %d r=%d err=%lu", i, r, GetLastError());
        SET_ERR(0);
        r = U(MultiByteToWideChar)(CP_UTF8, MB_ERR_INVALID_CHARS, bad[i], sl, NULL, 0);
        CHECKF(r == 0 && GetLastError() == E_NO_UNICODE_TRANSLATION, "query vec %d r=%d err=%lu", i, r, GetLastError());
        SET_ERR(0);
        r = U(MultiByteToWideChar)(CP_UTF8, MB_ERR_INVALID_CHARS, bad[i], -1, w, 16);
        CHECKF(r == 0 && GetLastError() == E_NO_UNICODE_TRANSLATION, "-1 vec %d r=%d err=%lu", i, r, GetLastError());
        /* without the flag the same data converts */
        r = U(MultiByteToWideChar)(CP_UTF8, 0, bad[i], sl, w, 16);
        CHECKF(r > 0, "no flag vec %d r=%d", i, r);
    }
}
TEST(mb2wc_dbcs_err_invalid_chars) {
    REQUIRE(U(MultiByteToWideChar));
    if (!U(IsValidCodePage) || !U(IsValidCodePage)(932)) { t_skip("cp932 not installed"); return; }
    WCHAR w[8];
    SET_ERR(0);
    int r = U(MultiByteToWideChar)(932, MB_ERR_INVALID_CHARS, "\x81\x20", 2, w, 8);
    CHECKF(r == 0 && GetLastError() == E_NO_UNICODE_TRANSLATION, "bad trail r=%d err=%lu", r, GetLastError());
    SET_ERR(0);
    r = U(MultiByteToWideChar)(932, MB_ERR_INVALID_CHARS, "a\x82", 2, w, 8);
    CHECKF(r == 0 && GetLastError() == E_NO_UNICODE_TRANSLATION, "dangling lead r=%d err=%lu", r, GetLastError());
    SET_ERR(0);
    r = U(MultiByteToWideChar)(936, MB_ERR_INVALID_CHARS, "\xd6", 1, w, 8);
    CHECKF(r == 0 && GetLastError() == E_NO_UNICODE_TRANSLATION, "936 dangling r=%d err=%lu", r, GetLastError());
    r = U(MultiByteToWideChar)(932, MB_ERR_INVALID_CHARS, "\x82\xa0", 2, w, 8);
    EXPECT_EQ(r, 1);
    /* without the flag the stray bytes still produce output */
    r = U(MultiByteToWideChar)(932, 0, "\x81\x20", 2, w, 8);
    t_note("932 bad trail without flag -> %d chars: %04X %04X", r, w[0], r > 1 ? w[1] : 0);
    CHECK(r >= 1);
    r = U(MultiByteToWideChar)(932, 0, "\x82", 1, w, 8);
    t_note("932 dangling lead without flag -> %d chars: %04X", r, w[0]);
    CHECK(r == 1);
}
TEST(mb2wc_utf7_flag_and_misc) {
    REQUIRE(U(MultiByteToWideChar));
    WCHAR w[16];
    EXPECT_EQ(U(MultiByteToWideChar)(CP_UTF7, 0, "abc", 3, NULL, 0), 3);
    EXPECT_EQ(U(MultiByteToWideChar)(CP_UTF7, 0, "abc", -1, NULL, 0), 4);
    SET_ERR(0);
    EXPECT_EQ(U(MultiByteToWideChar)(CP_UTF7, 0, "abc", 3, w, 2), 0);
    EXPECT_ERR(E_INSUFFICIENT_BUFFER);
    EXPECT_EQ(U(MultiByteToWideChar)(CP_UTF7, 0, "abc", 3, w, 3), 3);
}
TEST(mb2wc_symbol_cp) {
    REQUIRE(U(MultiByteToWideChar));
    WCHAR w[8];
    static const WCHAR exp[] = W_(0x0001, 0xF020, 0xF041, 0xF0FF, 0xF07F);
    int r = U(MultiByteToWideChar)(CP_SYMBOL, 0, "\x01\x20\x41\xff\x7f", 5, w, 8);
    EXPECT_EQ(r, 5);
    if (r == 5) CHECK_WEQ(w, exp, 5);
    EXPECT_EQ(U(MultiByteToWideChar)(CP_SYMBOL, 0, "\x41", 1, NULL, 0), 1);
    EXPECT_EQ(U(MultiByteToWideChar)(CP_SYMBOL, 0, "ab", -1, NULL, 0), 3);
    SET_ERR(0);
    EXPECT_EQ(U(MultiByteToWideChar)(CP_SYMBOL, 0, "\x41\x42", 2, w, 1), 0);
    EXPECT_ERR(E_INSUFFICIENT_BUFFER);
    SET_ERR(0);
    EXPECT_EQ(U(MultiByteToWideChar)(CP_SYMBOL, MB_PRECOMPOSED, "\x41", 1, w, 8), 0);
    EXPECT_ERR(E_INVALID_FLAGS);
    SET_ERR(0);
    EXPECT_EQ(U(MultiByteToWideChar)(CP_SYMBOL, MB_ERR_INVALID_CHARS, "\x41", 1, w, 8), 0);
    EXPECT_ERR(E_INVALID_FLAGS);
}
TEST(mb2wc_pseudo_codepages_ascii) {
    REQUIRE(U(MultiByteToWideChar));
    static const UINT cps[] = {CP_ACP, CP_OEMCP, CP_MACCP, CP_THREAD_ACP};
    for (int i = 0; i < 4; i++) {
        WCHAR w[16];
        int r = U(MultiByteToWideChar)(cps[i], 0, "Hello, World!", -1, w, 16);
        CHECKF(r == 14, "cp %u r=%d", cps[i], r);
        CHECKF(w[0] == 'H' && w[12] == '!' && w[13] == 0, "cp %u content", cps[i]);
        EXPECT_EQ(U(MultiByteToWideChar)(cps[i], 0, "Hello, World!", 13, NULL, 0), 13);
    }
}
TEST(mb2wc_acp_oemcp_equivalence) {
    REQUIRE(U(MultiByteToWideChar)); REQUIRE(U(GetACP)); REQUIRE(U(GetOEMCP));
    unsigned char all[255]; for (int i = 0; i < 255; i++) all[i] = (unsigned char)(i + 1);
    WCHAR a[300], b[300];
    UINT acp = U(GetACP)(), oem = U(GetOEMCP)();
    int ra = U(MultiByteToWideChar)(CP_ACP, 0, (char *)all, 255, a, 300), rb = U(MultiByteToWideChar)(acp, 0, (char *)all, 255, b, 300);
    CHECK(ra > 0 && ra == rb);
    if (ra == rb) CHECK(sc_weq(a, b, ra));
    ra = U(MultiByteToWideChar)(CP_OEMCP, 0, (char *)all, 255, a, 300); rb = U(MultiByteToWideChar)(oem, 0, (char *)all, 255, b, 300);
    CHECK(ra > 0 && ra == rb);
    if (ra == rb) CHECK(sc_weq(a, b, ra));
    ra = U(MultiByteToWideChar)(CP_THREAD_ACP, 0, (char *)all, 255, a, 300);
    t_note("ACP=%u OEM=%u thread-acp result chars=%d", acp, oem, ra);
}
TEST(mb2wc_mac_codepage) {
    REQUIRE(U(MultiByteToWideChar));
    WCHAR a[4], b[4];
    int ra = U(MultiByteToWideChar)(CP_MACCP, 0, "\x8e", 1, a, 4), rb = U(MultiByteToWideChar)(10000, 0, "\x8e", 1, b, 4);
    CHECK(ra == 1 || ra == 0);
    if (ra == 1 && rb == 1) { t_note("MACCP 0x8e -> %04X (10000 -> %04X)", a[0], b[0]); }
}
TEST(mb2wc_embedded_nul) {
    REQUIRE(U(MultiByteToWideChar));
    WCHAR w[8];
    static const WCHAR e[] = W_('a', 0, 'b', 0, 'c');
    int r = U(MultiByteToWideChar)(CP_ACP, 0, "a\0b\0c", 5, w, 8);
    EXPECT_EQ(r, 5); if (r == 5) CHECK_WEQ(w, e, 5);
    r = U(MultiByteToWideChar)(CP_UTF8, 0, "a\0b\0c", 5, w, 8);
    EXPECT_EQ(r, 5); if (r == 5) CHECK_WEQ(w, e, 5);
    r = U(MultiByteToWideChar)(CP_ACP, 0, "a\0b\0c", -1, w, 8);
    EXPECT_EQ(r, 2);   /* stops at first NUL, includes it */
    r = U(MultiByteToWideChar)(CP_UTF8, 0, "a\0b\0c", -1, w, 8);
    EXPECT_EQ(r, 2);
    r = U(MultiByteToWideChar)(1252, 0, "\0", 1, w, 8); EXPECT_EQ(r, 1); EXPECT_EQ(w[0], 0);
    r = U(MultiByteToWideChar)(CP_UTF8, 0, "\0\0\0", 3, w, 8); EXPECT_EQ(r, 3);
}
TEST(mb2wc_negative_cb_other) {
    REQUIRE(U(MultiByteToWideChar));
    int r1 = U(MultiByteToWideChar)(CP_ACP, 0, "abc", -2, NULL, 0);
    int r2 = U(MultiByteToWideChar)(CP_ACP, 0, "abc", -1, NULL, 0);
    t_note("cb=-2 -> %d, cb=-1 -> %d", r1, r2);
    EXPECT_EQ(r2, 4);
    int r3 = U(MultiByteToWideChar)(CP_UTF8, 0, "abc", INT_MIN, NULL, 0);
    t_note("cb=INT_MIN utf8 -> %d", r3);
}
TEST(mb2wc_large_input) {
    REQUIRE(U(MultiByteToWideChar));
    enum { N = 200000 };
    char *a = HeapAlloc(GetProcessHeap(), 0, N + 1);
    WCHAR *w = HeapAlloc(GetProcessHeap(), 0, (N + 1) * 2);
    memset(a, 'q', N); a[N] = 0;
    static const UINT cps[] = {CP_ACP, CP_UTF8, 437, 1252, 28591};
    for (int i = 0; i < ARRAY_N(cps); i++) {
        EXPECT_EQ(U(MultiByteToWideChar)(cps[i], 0, a, N, NULL, 0), N);
        EXPECT_EQ(U(MultiByteToWideChar)(cps[i], 0, a, -1, NULL, 0), N + 1);
        EXPECT_EQ(U(MultiByteToWideChar)(cps[i], 0, a, -1, w, N + 1), N + 1);
        CHECK(w[0] == 'q' && w[N - 1] == 'q' && w[N] == 0);
    }
    HeapFree(GetProcessHeap(), 0, a); HeapFree(GetProcessHeap(), 0, w);
}
TEST(mb2wc_all_bytes_sbcs_roundtrip_count) {
    REQUIRE(U(MultiByteToWideChar));
    static const UINT cps[] = {437, 850, 1250, 1251, 1252, 1253, 1254, 1255, 1256, 1257, 1258, 874, 28591, 28592, 20866, 10000};
    for (int c = 0; c < ARRAY_N(cps); c++) {
        if (!U(IsValidCodePage) || !U(IsValidCodePage)(cps[c])) continue;
        unsigned char all[256]; for (int i = 0; i < 256; i++) all[i] = (unsigned char)i;
        WCHAR w[300];
        /* every byte of a single-byte code page yields exactly one UTF-16 unit (no flags) */
        int r = U(MultiByteToWideChar)(cps[c], 0, (char *)all, 256, w, 300);
        CHECKF(r == 256, "cp %u r=%d", cps[c], r);
        for (int i = 0; i < 0x80 && r == 256; i++) CHECKF(w[i] == i, "cp %u ascii %d -> %04X", cps[c], i, w[i]);
    }
}
TEST(mb2wc_utf8_all_scalar_boundaries) {
    REQUIRE(U(MultiByteToWideChar));
    /* encode a spread of scalar values by hand and verify they decode back */
    static const unsigned cps[] = {0, 1, 0x7f, 0x80, 0x7ff, 0x800, 0xd7ff, 0xe000, 0xfffd, 0xffff, 0x10000, 0x10ffff, 0x1f600, 0x2ffff, 0x40000, 0x10fffe};
    for (int i = 0; i < ARRAY_N(cps); i++) {
        unsigned c = cps[i]; char b[8]; int n = 0;
        if (c < 0x80) b[n++] = (char)c;
        else if (c < 0x800) { b[n++] = (char)(0xc0 | c >> 6); b[n++] = (char)(0x80 | (c & 0x3f)); }
        else if (c < 0x10000) { b[n++] = (char)(0xe0 | c >> 12); b[n++] = (char)(0x80 | (c >> 6 & 0x3f)); b[n++] = (char)(0x80 | (c & 0x3f)); }
        else { b[n++] = (char)(0xf0 | c >> 18); b[n++] = (char)(0x80 | (c >> 12 & 0x3f)); b[n++] = (char)(0x80 | (c >> 6 & 0x3f)); b[n++] = (char)(0x80 | (c & 0x3f)); }
        WCHAR w[4]; int r = U(MultiByteToWideChar)(CP_UTF8, MB_ERR_INVALID_CHARS, b, n, w, 4);
        int want = c >= 0x10000 ? 2 : 1;
        CHECKF(r == want, "U+%X r=%d", c, r);
        if (r == want) {
            unsigned got = want == 2 ? 0x10000 + ((w[0] - 0xd800u) << 10) + (w[1] - 0xdc00u) : w[0];
            CHECKF(got == c, "U+%X got U+%X", c, got);
        }
    }
}
TEST(mb2wc_sentinel_not_overrun) {
    REQUIRE(U(MultiByteToWideChar));
    WCHAR w[10]; for (int i = 0; i < 10; i++) w[i] = 0x4242;
    int r = U(MultiByteToWideChar)(CP_UTF8, 0, "abc", 3, w + 2, 3);
    EXPECT_EQ(r, 3);
    CHECK(w[0] == 0x4242 && w[1] == 0x4242 && w[5] == 0x4242);
    for (int i = 0; i < 10; i++) w[i] = 0x4242;
    SET_ERR(0);
    r = U(MultiByteToWideChar)(CP_UTF8, 0, "abcdef", 6, w, 3);
    EXPECT_EQ(r, 0); EXPECT_ERR(E_INSUFFICIENT_BUFFER);
    CHECK(w[3] == 0x4242 && w[4] == 0x4242);
    t_note("partial output on insufficient buffer: %04X %04X %04X", w[0], w[1], w[2]);
}
TEST(mb2wc_ansi_sensitive_note) {
    REQUIRE(U(GetACP));
    if (U(GetACP)() != 1252) t_note("system ACP is %u, CP_ACP-dependent expectations not checked", U(GetACP)());
    else {
        WCHAR w[4];
        EXPECT_EQ(U(MultiByteToWideChar)(CP_ACP, 0, "\x80\xe9", 2, w, 4), 2);
        CHECK(w[0] == 0x20AC && w[1] == 0xE9);
    }
}
