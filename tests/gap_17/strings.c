#include "t.h"
/* gap17: lstrcpy*, lstrcpyn*, lstrlen*, uaw_wcslen/uaw_lstrlenW. */

typedef size_t (__cdecl *uaw_wcslen_pfn)(const WCHAR *);
typedef int (WINAPI *uaw_lstrlenW_pfn)(const WCHAR *);

#define SENT 0x5A

TEST(gap17_lstrlenA_basic) {
    REQUIRE(U(lstrlenA));
    EXPECT_EQ(U(lstrlenA)(""), 0);
    EXPECT_EQ(U(lstrlenA)("a"), 1);
    EXPECT_EQ(U(lstrlenA)("hello"), 5);
    EXPECT_EQ(U(lstrlenA)("a\0b"), 1);
    EXPECT_EQ(U(lstrlenA)("\xff\x80\xe9"), 3);
    EXPECT_EQ(U(lstrlenA)("tab\there\r\n"), 10);
}
TEST(gap17_lstrlenA_null) {
    if (t_is_wine()) { t_skip("wine faults on NULL lstrlen"); return; }
    REQUIRE(U(lstrlenA));
    SET_ERR(0x1234);
    EXPECT_EQ(U(lstrlenA)(NULL), 0);
    EXPECT_ERR(0x1234);   /* no last-error change (decomp: plain NULL check) */
}
TEST(gap17_lstrlenW_basic) {
    REQUIRE(U(lstrlenW));
    EXPECT_EQ(U(lstrlenW)(L""), 0);
    EXPECT_EQ(U(lstrlenW)(L"a"), 1);
    EXPECT_EQ(U(lstrlenW)(L"hello"), 5);
    EXPECT_EQ(U(lstrlenW)(L"a\0b"), 1);
    EXPECT_EQ(U(lstrlenW)(L"\xffff\x8000\x00e9"), 3);
    /* surrogate pair counts as two units */
    EXPECT_EQ(U(lstrlenW)(L"\xd83d\xde00"), 2);
}
TEST(gap17_lstrlenW_null) {
    if (t_is_wine()) { t_skip("wine faults on NULL lstrlen"); return; }
    REQUIRE(U(lstrlenW));
    SET_ERR(0x1234);
    EXPECT_EQ(U(lstrlenW)(NULL), 0);
    EXPECT_ERR(0x1234);
}
TEST(gap17_lstrlen_kernel32_forward) {
    if (t_is_wine()) { t_skip("wine faults on NULL lstrlen"); return; }
    REQUIRE(UK(lstrlenA)); REQUIRE(UK(lstrlenW)); REQUIRE(UB(lstrlenA)); REQUIRE(UB(lstrlenW));
    EXPECT_EQ(UK(lstrlenA)("abcd"), 4);
    EXPECT_EQ(UB(lstrlenA)("abcd"), 4);
    EXPECT_EQ(UK(lstrlenW)(L"abcd"), 4);
    EXPECT_EQ(UB(lstrlenW)(L"abcd"), 4);
    EXPECT_EQ(UK(lstrlenA)(NULL), 0);
    EXPECT_EQ(UB(lstrlenA)(NULL), 0);
    EXPECT_EQ(UK(lstrlenW)(NULL), 0);
    EXPECT_EQ(UB(lstrlenW)(NULL), 0);
}
TEST(gap17_lstrlen_alias_exports) {
    /* "lstrlen" is the A alias of lstrlenA in both modules */
    FARPROC a = t_proc_k32("lstrlen"), b = t_proc_k32("lstrlenA");
    if (!a || !b) { t_note("lstrlen alias not exported"); return; }
    EXPECT_EQ(((int (WINAPI *)(const char *))a)("xyz"), 3);
    EXPECT_EQ(((int (WINAPI *)(const char *))b)("xyz"), 3);
}
TEST(gap17_lstrlen_long) {
    REQUIRE(U(lstrlenA)); REQUIRE(U(lstrlenW));
    int sizes[] = { 255, 256, 259, 260, 261, 4095, 4096, 4097, 65535, 65536, 100000 };
    for (unsigned i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        int n = sizes[i];
        char *a = HeapAlloc(GetProcessHeap(), 0, n + 1);
        WCHAR *w = HeapAlloc(GetProcessHeap(), 0, (n + 1) * 2);
        if (!a || !w) { t_note("alloc failed"); break; }
        memset(a, 'q', n); a[n] = 0;
        for (int k = 0; k < n; k++) w[k] = (k & 1) ? 0x4141 : 0x0141;
        w[n] = 0;
        EXPECT_EQ(U(lstrlenA)(a), n);
        EXPECT_EQ(U(lstrlenW)(w), n);
        HeapFree(GetProcessHeap(), 0, a); HeapFree(GetProcessHeap(), 0, w);
    }
}
TEST(gap17_lstrlen_end_of_page) {
    REQUIRE(U(lstrlenA)); REQUIRE(U(lstrlenW));
    char *p = VirtualAlloc(NULL, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!p) { t_note("VirtualAlloc failed"); return; }
    DWORD old;
    VirtualProtect(p + 4096, 4096, PAGE_NOACCESS, &old);
    /* string ending exactly at the guard page boundary must not read past NUL */
    char *s = p + 4096 - 6;
    memcpy(s, "abcde", 6);
    EXPECT_EQ(U(lstrlenA)(s), 5);
    WCHAR *ws = (WCHAR *)(p + 4096 - 8);
    ws[0] = 'a'; ws[1] = 'b'; ws[2] = 'c'; ws[3] = 0;
    EXPECT_EQ(U(lstrlenW)(ws), 3);
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap17_lstrlenW_unaligned) {
    REQUIRE(U(lstrlenW));
    /* odd-address wide string: lstrlenW on x64 handles unaligned reads */
    BYTE buf[32];
    memset(buf, 0, sizeof buf);
    WCHAR src[] = L"abcd";
    memcpy(buf + 1, src, sizeof src);
    EXPECT_EQ(U(lstrlenW)((WCHAR *)(buf + 1)), 4);
}

/* ---- lstrcpyA / lstrcpyW ---- */
TEST(gap17_lstrcpyA_basic) {
    REQUIRE(U(lstrcpyA));
    char d[32];
    memset(d, SENT, sizeof d);
    LPSTR r = U(lstrcpyA)(d, "hello");
    EXPECT_EQ(r, d);
    EXPECT_STR(d, "hello");
    EXPECT_EQ(d[5], 0);
    EXPECT_EQ(d[6], SENT);   /* exactly strlen+1 bytes written */
    memset(d, SENT, sizeof d);
    r = U(lstrcpyA)(d, "");
    EXPECT_EQ(r, d);
    EXPECT_EQ(d[0], 0);
    EXPECT_EQ(d[1], SENT);
}
TEST(gap17_lstrcpyA_highbit_and_embedded) {
    REQUIRE(U(lstrcpyA));
    char d[16];
    memset(d, SENT, sizeof d);
    U(lstrcpyA)(d, "\xff\x80\x01z");
    EXPECT_EQ((unsigned char)d[0], 0xff);
    EXPECT_EQ((unsigned char)d[1], 0x80);
    EXPECT_EQ(d[2], 1);
    EXPECT_EQ(d[3], 'z');
    EXPECT_EQ(d[4], 0);
    EXPECT_EQ(d[5], SENT);
    memset(d, SENT, sizeof d);
    U(lstrcpyA)(d, "ab\0cd");
    EXPECT_EQ(d[2], 0);
    EXPECT_EQ(d[3], SENT);
}
TEST(gap17_lstrcpyA_exact_fit_and_lasterror) {
    REQUIRE(U(lstrcpyA));
    char *p = VirtualAlloc(NULL, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!p) return;
    DWORD old;
    VirtualProtect(p + 4096, 4096, PAGE_NOACCESS, &old);
    char *d = p + 4096 - 6;     /* exact fit for "abcde" + NUL */
    SET_ERR(0x4321);
    LPSTR r = U(lstrcpyA)(d, "abcde");
    EXPECT_EQ(r, d);
    EXPECT_STR(d, "abcde");
    EXPECT_ERR(0x4321);
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap17_lstrcpyA_long) {
    REQUIRE(U(lstrcpyA)); REQUIRE(U(lstrlenA));
    int n = 70000;
    char *s = HeapAlloc(GetProcessHeap(), 0, n + 1), *d = HeapAlloc(GetProcessHeap(), 0, n + 1);
    if (!s || !d) return;
    for (int i = 0; i < n; i++) s[i] = (char)('A' + i % 26);
    s[n] = 0;
    EXPECT_EQ(U(lstrcpyA)(d, s), d);
    EXPECT_EQ(U(lstrlenA)(d), n);
    CHECK(!memcmp(s, d, n + 1));
    HeapFree(GetProcessHeap(), 0, s); HeapFree(GetProcessHeap(), 0, d);
}
TEST(gap17_lstrcpyA_kernel32_export) {
    REQUIRE(UK(lstrcpyA));
    char d[8] = {0};
    EXPECT_EQ(UK(lstrcpyA)(d, "xyz"), d);
    EXPECT_STR(d, "xyz");
    FARPROC alias = t_proc_k32("lstrcpy");
    if (alias) { char e[8] = {0}; ((LPSTR (WINAPI *)(LPSTR, LPCSTR))alias)(e, "q"); EXPECT_STR(e, "q"); }
}
TEST(gap17_lstrcpyA_adjacent_overlap_forward) {
    REQUIRE(U(lstrcpyA));
    /* non-overlapping adjacent copy inside one buffer */
    char b[32] = "0123456789";
    U(lstrcpyA)(b + 16, b);
    EXPECT_STR(b + 16, "0123456789");
    EXPECT_STR(b, "0123456789");
}
TEST(gap17_lstrcpyW_basic) {
    REQUIRE(U(lstrcpyW));
    WCHAR d[32];
    for (int i = 0; i < 32; i++) d[i] = 0x5A5A;
    LPWSTR r = U(lstrcpyW)(d, L"hello");
    EXPECT_EQ(r, d);
    EXPECT_WSTR(d, L"hello");
    EXPECT_EQ(d[5], 0);
    EXPECT_EQ(d[6], 0x5A5A);
    for (int i = 0; i < 32; i++) d[i] = 0x5A5A;
    r = U(lstrcpyW)(d, L"");
    EXPECT_EQ(r, d);
    EXPECT_EQ(d[0], 0);
    EXPECT_EQ(d[1], 0x5A5A);
}
TEST(gap17_lstrcpyW_unicode_and_embedded) {
    REQUIRE(U(lstrcpyW));
    WCHAR d[16];
    for (int i = 0; i < 16; i++) d[i] = 0x5A5A;
    U(lstrcpyW)(d, L"\xffff\x8000\xd83d\xde00z");
    EXPECT_EQ(d[0], 0xffff); EXPECT_EQ(d[1], 0x8000);
    EXPECT_EQ(d[2], 0xd83d); EXPECT_EQ(d[3], 0xde00);
    EXPECT_EQ(d[4], 'z'); EXPECT_EQ(d[5], 0); EXPECT_EQ(d[6], 0x5A5A);
    for (int i = 0; i < 16; i++) d[i] = 0x5A5A;
    U(lstrcpyW)(d, L"ab\0cd");
    EXPECT_EQ(d[2], 0); EXPECT_EQ(d[3], 0x5A5A);
}
TEST(gap17_lstrcpyW_exact_fit_lasterror) {
    REQUIRE(U(lstrcpyW));
    char *p = VirtualAlloc(NULL, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!p) return;
    DWORD old;
    VirtualProtect(p + 4096, 4096, PAGE_NOACCESS, &old);
    WCHAR *d = (WCHAR *)(p + 4096 - 12);
    SET_ERR(0x4321);
    EXPECT_EQ(U(lstrcpyW)(d, L"abcde"), d);
    EXPECT_WSTR(d, L"abcde");
    EXPECT_ERR(0x4321);
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap17_lstrcpyW_long) {
    REQUIRE(U(lstrcpyW)); REQUIRE(U(lstrlenW));
    int n = 70000;
    WCHAR *s = HeapAlloc(GetProcessHeap(), 0, (n + 1) * 2), *d = HeapAlloc(GetProcessHeap(), 0, (n + 1) * 2);
    if (!s || !d) return;
    for (int i = 0; i < n; i++) s[i] = (WCHAR)(1 + i % 0xfff0);
    s[n] = 0;
    EXPECT_EQ(U(lstrcpyW)(d, s), d);
    EXPECT_EQ(U(lstrlenW)(d), n);
    CHECK(!memcmp(s, d, (n + 1) * 2));
    HeapFree(GetProcessHeap(), 0, s); HeapFree(GetProcessHeap(), 0, d);
}
TEST(gap17_lstrcpyW_kernel32_export) {
    REQUIRE(UK(lstrcpyW));
    WCHAR d[8] = {0};
    EXPECT_EQ(UK(lstrcpyW)(d, L"xyz"), d);
    EXPECT_WSTR(d, L"xyz");
}

/* ---- lstrcpynA ---- */
static void fillA(char *d, int n) { memset(d, SENT, n); }
TEST(gap17_lstrcpynA_table) {
    REQUIRE(U(lstrcpynA));
    static const struct { const char *src; int max; const char *expect; int written; } T[] = {
        { "hello", 1, "", 1 },
        { "hello", 2, "h", 2 },
        { "hello", 4, "hel", 4 },
        { "hello", 5, "hell", 5 },   /* exact-1: truncated */
        { "hello", 6, "hello", 6 },   /* exact fit */
        { "hello", 7, "hello", 6 },   /* larger: no padding */
        { "hello", 100, "hello", 6 },
        { "", 1, "", 1 },
        { "", 5, "", 1 },
        { "a", 1, "", 1 },
        { "a", 2, "a", 2 },
        { "a", 3, "a", 2 },
    };
    for (unsigned i = 0; i < sizeof T / sizeof *T; i++) {
        char d[128]; fillA(d, sizeof d);
        LPSTR r = U(lstrcpynA)(d, T[i].src, T[i].max);
        CHECKF(r == d, "case %u returns dest", i);
        CHECKF(!strcmp(d, T[i].expect), "case %u content '%s'", i, d);
        CHECKF(d[T[i].written] == SENT, "case %u no write past %d", i, T[i].written);
        for (int k = T[i].written; k < 8 + T[i].written && k < 128; k++)
            CHECKF(d[k] == SENT, "case %u byte %d untouched", i, k);
    }
}
TEST(gap17_lstrcpynA_zero_max) {
    REQUIRE(U(lstrcpynA));
    char d[8]; fillA(d, sizeof d);
    LPSTR r = U(lstrcpynA)(d, "abc", 0);
    EXPECT_EQ(r, d);
    for (int i = 0; i < 8; i++) CHECK(d[i] == SENT);
    /* NULL dest with 0 length: nothing is written (decomp: guarded by iMaxLength != 0) */
    r = U(lstrcpynA)(NULL, "abc", 0);
    EXPECT_EQ(r, NULL);
}
TEST(gap17_lstrcpynA_embedded_nul) {
    REQUIRE(U(lstrcpynA));
    char d[16]; fillA(d, sizeof d);
    U(lstrcpynA)(d, "ab\0cd", 10);
    EXPECT_EQ(d[0], 'a'); EXPECT_EQ(d[1], 'b'); EXPECT_EQ(d[2], 0); EXPECT_EQ(d[3], SENT);
}
TEST(gap17_lstrcpynA_highbit) {
    REQUIRE(U(lstrcpynA));
    char d[16]; fillA(d, sizeof d);
    U(lstrcpynA)(d, "\xff\xfe\xfd\xfc", 3);
    EXPECT_EQ((unsigned char)d[0], 0xff); EXPECT_EQ((unsigned char)d[1], 0xfe); EXPECT_EQ(d[2], 0);
}
TEST(gap17_lstrcpynA_src_not_read_past_limit) {
    REQUIRE(U(lstrcpynA));
    /* source without terminator, followed by a no-access page: only iMax chars are read */
    char *p = VirtualAlloc(NULL, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!p) return;
    DWORD old;
    VirtualProtect(p + 4096, 4096, PAGE_NOACCESS, &old);
    char *s = p + 4096 - 4;
    memcpy(s, "WXYZ", 4);
    char d[16]; fillA(d, sizeof d);
    U(lstrcpynA)(d, s, 4);
    EXPECT_STR(d, "WXY");
    U(lstrcpynA)(d, s, 3);
    EXPECT_STR(d, "WX");
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap17_lstrcpynA_lasterror_kernelbase) {
    REQUIRE(UK(lstrcpynA)); REQUIRE(UB(lstrcpynA));
    char d[8];
    SET_ERR(0x777);
    UK(lstrcpynA)(d, "abc", 8); EXPECT_STR(d, "abc");
    EXPECT_ERR(0x777);
    UB(lstrcpynA)(d, "xyz", 8); EXPECT_STR(d, "xyz");
    EXPECT_ERR(0x777);
    FARPROC alias = t_proc_k32("lstrcpyn");
    if (alias) { ((LPSTR (WINAPI *)(LPSTR, LPCSTR, int))alias)(d, "qq", 8); EXPECT_STR(d, "qq"); }
}
TEST(gap17_lstrcpynA_max_equals_len_plus_one_page) {
    REQUIRE(U(lstrcpynA));
    char *p = VirtualAlloc(NULL, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!p) return;
    DWORD old;
    VirtualProtect(p + 4096, 4096, PAGE_NOACCESS, &old);
    char *d = p + 4096 - 4;
    U(lstrcpynA)(d, "abcdefgh", 4);   /* writes exactly 4 bytes incl. NUL */
    EXPECT_STR(d, "abc");
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap17_lstrcpynA_long) {
    REQUIRE(U(lstrcpynA)); REQUIRE(U(lstrlenA));
    int n = 50000;
    char *s = HeapAlloc(GetProcessHeap(), 0, n + 1), *d = HeapAlloc(GetProcessHeap(), 0, n + 1);
    if (!s || !d) return;
    memset(s, 'k', n); s[n] = 0;
    int caps[] = { 1, 2, 255, 256, 4096, 49999, 50000, 50001 };
    for (unsigned i = 0; i < sizeof caps / sizeof *caps; i++) {
        memset(d, SENT, n + 1);
        U(lstrcpynA)(d, s, caps[i]);
        int expect = caps[i] - 1 < n ? caps[i] - 1 : n;
        CHECKF(U(lstrlenA)(d) == expect, "cap %d len", caps[i]);
    }
    HeapFree(GetProcessHeap(), 0, s); HeapFree(GetProcessHeap(), 0, d);
}

/* ---- lstrcpynW ---- */
static void fillW(WCHAR *d, int n) { for (int i = 0; i < n; i++) d[i] = 0x5A5A; }
TEST(gap17_lstrcpynW_table) {
    REQUIRE(U(lstrcpynW));
    static const struct { const WCHAR *src; int max; const WCHAR *expect; int written; } T[] = {
        { L"hello", 1, L"", 1 },
        { L"hello", 2, L"h", 2 },
        { L"hello", 4, L"hel", 4 },
        { L"hello", 5, L"hell", 5 },
        { L"hello", 6, L"hello", 6 },
        { L"hello", 7, L"hello", 6 },
        { L"hello", 100, L"hello", 6 },
        { L"", 1, L"", 1 },
        { L"", 5, L"", 1 },
        { L"a", 1, L"", 1 },
        { L"a", 2, L"a", 2 },
    };
    for (unsigned i = 0; i < sizeof T / sizeof *T; i++) {
        WCHAR d[128]; fillW(d, 128);
        LPWSTR r = U(lstrcpynW)(d, T[i].src, T[i].max);
        CHECKF(r == d, "case %u returns dest", i);
        CHECKF(!lstrcmpW(d, T[i].expect), "case %u content", i);
        for (int k = T[i].written; k < 8 + T[i].written; k++)
            CHECKF(d[k] == 0x5A5A, "case %u wchar %d untouched", i, k);
    }
}
TEST(gap17_lstrcpynW_zero_max) {
    REQUIRE(U(lstrcpynW));
    WCHAR d[8]; fillW(d, 8);
    EXPECT_EQ(U(lstrcpynW)(d, L"abc", 0), d);
    for (int i = 0; i < 8; i++) CHECK(d[i] == 0x5A5A);
}
TEST(gap17_lstrcpynW_unicode) {
    REQUIRE(U(lstrcpynW));
    WCHAR d[16]; fillW(d, 16);
    U(lstrcpynW)(d, L"\xffff\x8000\xd83d\xde00", 3);
    EXPECT_EQ(d[0], 0xffff); EXPECT_EQ(d[1], 0x8000); EXPECT_EQ(d[2], 0); EXPECT_EQ(d[3], 0x5A5A);
    /* truncation may split a surrogate pair: no special handling */
    fillW(d, 16);
    U(lstrcpynW)(d, L"a\xd83d\xde00", 3);
    EXPECT_EQ(d[0], 'a'); EXPECT_EQ(d[1], 0xd83d); EXPECT_EQ(d[2], 0);
}
TEST(gap17_lstrcpynW_embedded_nul) {
    REQUIRE(U(lstrcpynW));
    WCHAR d[16]; fillW(d, 16);
    U(lstrcpynW)(d, L"ab\0cd", 10);
    EXPECT_EQ(d[2], 0); EXPECT_EQ(d[3], 0x5A5A);
}
TEST(gap17_lstrcpynW_src_not_read_past_limit) {
    REQUIRE(U(lstrcpynW));
    char *p = VirtualAlloc(NULL, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!p) return;
    DWORD old;
    VirtualProtect(p + 4096, 4096, PAGE_NOACCESS, &old);
    WCHAR *s = (WCHAR *)(p + 4096 - 8);
    s[0] = 'W'; s[1] = 'X'; s[2] = 'Y'; s[3] = 'Z';
    WCHAR d[16]; fillW(d, 16);
    U(lstrcpynW)(d, s, 4);
    EXPECT_WSTR(d, L"WXY");
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap17_lstrcpynW_lasterror_kernelbase) {
    REQUIRE(UK(lstrcpynW)); REQUIRE(UB(lstrcpynW));
    WCHAR d[8];
    SET_ERR(0x777);
    UK(lstrcpynW)(d, L"abc", 8); EXPECT_WSTR(d, L"abc");
    EXPECT_ERR(0x777);
    UB(lstrcpynW)(d, L"xyz", 8); EXPECT_WSTR(d, L"xyz");
    EXPECT_ERR(0x777);
}
TEST(gap17_lstrcpynW_long) {
    REQUIRE(U(lstrcpynW)); REQUIRE(U(lstrlenW));
    int n = 50000;
    WCHAR *s = HeapAlloc(GetProcessHeap(), 0, (n + 1) * 2), *d = HeapAlloc(GetProcessHeap(), 0, (n + 1) * 2);
    if (!s || !d) return;
    for (int i = 0; i < n; i++) s[i] = 'k';
    s[n] = 0;
    int caps[] = { 1, 2, 255, 256, 4096, 49999, 50000, 50001 };
    for (unsigned i = 0; i < sizeof caps / sizeof *caps; i++) {
        fillW(d, n + 1);
        U(lstrcpynW)(d, s, caps[i]);
        int expect = caps[i] - 1 < n ? caps[i] - 1 : n;
        CHECKF(U(lstrlenW)(d) == expect, "cap %d len", caps[i]);
    }
    HeapFree(GetProcessHeap(), 0, s); HeapFree(GetProcessHeap(), 0, d);
}

/* ---- uaw_wcslen / uaw_lstrlenW ---- */
TEST(gap17_uaw_wcslen_basic) {
    uaw_wcslen_pfn f = (uaw_wcslen_pfn)t_proc_k32("uaw_wcslen");
    REQUIRE(f);
    EXPECT_EQ(f(L""), 0);
    EXPECT_EQ(f(L"a"), 1);
    EXPECT_EQ(f(L"hello world"), 11);
    EXPECT_EQ(f(L"a\0b"), 1);
    EXPECT_EQ(f(L"\xffff\x8000"), 2);
    EXPECT_EQ(f(L"\xd83d\xde00"), 2);
}
TEST(gap17_uaw_wcslen_unaligned) {
    uaw_wcslen_pfn f = (uaw_wcslen_pfn)t_proc_k32("uaw_wcslen");
    REQUIRE(f);
    BYTE buf[64];
    WCHAR src[] = L"unaligned";
    for (int off = 0; off < 4; off++) {
        memset(buf, 0, sizeof buf);
        memcpy(buf + off, src, sizeof src);
        CHECKF(f((WCHAR *)(buf + off)) == 9, "offset %d", off);
    }
}
TEST(gap17_uaw_wcslen_long_and_page_end) {
    uaw_wcslen_pfn f = (uaw_wcslen_pfn)t_proc_k32("uaw_wcslen");
    REQUIRE(f);
    int n = 70000;
    WCHAR *s = HeapAlloc(GetProcessHeap(), 0, (n + 1) * 2);
    if (!s) return;
    for (int i = 0; i < n; i++) s[i] = 'z';
    s[n] = 0;
    EXPECT_EQ(f(s), n);
    HeapFree(GetProcessHeap(), 0, s);
    char *p = VirtualAlloc(NULL, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!p) return;
    DWORD old;
    VirtualProtect(p + 4096, 4096, PAGE_NOACCESS, &old);
    WCHAR *e = (WCHAR *)(p + 4096 - 6);
    e[0] = 'a'; e[1] = 'b'; e[2] = 0;
    EXPECT_EQ(f(e), 2);
    VirtualFree(p, 0, MEM_RELEASE);
}
TEST(gap17_uaw_wcslen_matches_lstrlenW) {
    uaw_wcslen_pfn f = (uaw_wcslen_pfn)t_proc_k32("uaw_wcslen");
    REQUIRE(f); REQUIRE(U(lstrlenW));
    const WCHAR *v[] = { L"", L"x", L"path\\to\\file.txt", L"\x00e9\x4e2d\x6587" };
    for (unsigned i = 0; i < 4; i++)
        CHECKF((int)f(v[i]) == U(lstrlenW)(v[i]), "case %u", i);
}
TEST(gap17_uaw_lstrlenW_basic) {
    uaw_lstrlenW_pfn f = (uaw_lstrlenW_pfn)t_proc_k32("uaw_lstrlenW");
    REQUIRE(f);
    EXPECT_EQ(f(NULL), 0);   /* NULL checked (decomp) */
    EXPECT_EQ(f(L""), 0);
    EXPECT_EQ(f(L"abcdef"), 6);
    BYTE buf[32] = {0};
    WCHAR src[] = L"abc";
    memcpy(buf + 1, src, sizeof src);
    EXPECT_EQ(f((WCHAR *)(buf + 1)), 3);
}
