/* CharLower/Upper(A/W/Buff), CharNext/Prev(A/W/ExA/ExW-less). */
#include "gap1.h"

#define CP_SJIS 932

/* ---------- CharUpperA / CharLowerA ---------- */
TEST(gap1_charuppera_null) { REQUIRE(U(CharUpperA)); CHECK(U(CharUpperA)(NULL) == NULL); }
TEST(gap1_charlowera_null) { REQUIRE(U(CharLowerA)); CHECK(U(CharLowerA)(NULL) == NULL); }
TEST(gap1_charupperw_null) { REQUIRE(U(CharUpperW)); CHECK(U(CharUpperW)(NULL) == NULL); }
TEST(gap1_charlowerw_null) { REQUIRE(U(CharLowerW)); CHECK(U(CharLowerW)(NULL) == NULL); }

TEST(gap1_charuppera_string_inplace_returns_same_ptr) {
    REQUIRE(U(CharUpperA));
    char b[] = "Hello, World 123 xyz!";
    LPSTR r = U(CharUpperA)(b);
    CHECK(r == b);
    EXPECT_STR(b, "HELLO, WORLD 123 XYZ!");
}
TEST(gap1_charlowera_string_inplace_returns_same_ptr) {
    REQUIRE(U(CharLowerA));
    char b[] = "Hello, World 123 XYZ!";
    LPSTR r = U(CharLowerA)(b);
    CHECK(r == b);
    EXPECT_STR(b, "hello, world 123 xyz!");
}
TEST(gap1_charuppera_empty_string) {
    REQUIRE(U(CharUpperA));
    char b[4] = "";
    CHECK(U(CharUpperA)(b) == b);
    EXPECT_EQ(b[0], 0);
}
TEST(gap1_charlowera_empty_string) {
    REQUIRE(U(CharLowerA));
    char b[4] = "";
    CHECK(U(CharLowerA)(b) == b);
    EXPECT_EQ(b[0], 0);
}
TEST(gap1_charuppera_does_not_touch_past_nul) {
    REQUIRE(U(CharUpperA));
    char b[8] = { 'a', 'b', 0, 'c', 'd', 0 };
    U(CharUpperA)(b);
    EXPECT_EQ(b[0], 'A'); EXPECT_EQ(b[1], 'B'); EXPECT_EQ(b[3], 'c'); EXPECT_EQ(b[4], 'd');
}
TEST(gap1_charlowera_does_not_touch_past_nul) {
    REQUIRE(U(CharLowerA));
    char b[8] = { 'A', 'B', 0, 'C', 'D', 0 };
    U(CharLowerA)(b);
    EXPECT_EQ(b[0], 'a'); EXPECT_EQ(b[1], 'b'); EXPECT_EQ(b[3], 'C'); EXPECT_EQ(b[4], 'D');
}
TEST(gap1_charuppera_single_ascii_all) {
    REQUIRE(U(CharUpperA));
    for (int c = 1; c < 128; c++) {
        int want = (c >= 'a' && c <= 'z') ? c - 32 : c;
        LPSTR r = U(CharUpperA)(G1_LP(c));
        CHECKF((ULONG_PTR)r == (ULONG_PTR)want, "c=%d got %p want %d", c, r, want);
    }
}
TEST(gap1_charlowera_single_ascii_all) {
    REQUIRE(U(CharLowerA));
    for (int c = 1; c < 128; c++) {
        int want = (c >= 'A' && c <= 'Z') ? c + 32 : c;
        LPSTR r = U(CharLowerA)(G1_LP(c));
        CHECKF((ULONG_PTR)r == (ULONG_PTR)want, "c=%d got %p want %d", c, r, want);
    }
}
TEST(gap1_charuppera_single_char_high_bits_of_ptr_dropped) {
    /* pointer with zero high 48 bits => LOWORD is the char; only the low byte is converted */
    REQUIRE(U(CharUpperA));
    CHECK((ULONG_PTR)U(CharUpperA)((LPSTR)(ULONG_PTR)0x61) == 0x41);
    CHECK((ULONG_PTR)U(CharUpperA)((LPSTR)(ULONG_PTR)0x7A) == 0x5A);
    CHECK((ULONG_PTR)U(CharUpperA)((LPSTR)(ULONG_PTR)0x40) == 0x40);
    CHECK((ULONG_PTR)U(CharUpperA)((LPSTR)(ULONG_PTR)0x7B) == 0x7B);
}
TEST(gap1_charlowera_single_char_boundaries) {
    REQUIRE(U(CharLowerA));
    CHECK((ULONG_PTR)U(CharLowerA)((LPSTR)(ULONG_PTR)0x41) == 0x61);
    CHECK((ULONG_PTR)U(CharLowerA)((LPSTR)(ULONG_PTR)0x5A) == 0x7A);
    CHECK((ULONG_PTR)U(CharLowerA)((LPSTR)(ULONG_PTR)0x40) == 0x40);
    CHECK((ULONG_PTR)U(CharLowerA)((LPSTR)(ULONG_PTR)0x5B) == 0x5B);
}
TEST(gap1_charuppera_cp1252_high_chars) {
    REQUIRE(U(CharUpperA)); REQUIRE(U(CharLowerA));
    if (GetACP() != 1252) { t_note("ACP=%u, skipping latin-1 byte mapping", GetACP()); return; }
    for (int c = 0xE0; c <= 0xFE; c++) {
        if (c == 0xF7) continue;
        CHECKF((ULONG_PTR)U(CharUpperA)(G1_LP(c)) == (ULONG_PTR)(c - 0x20), "c=%x", c);
    }
    for (int c = 0xC0; c <= 0xDE; c++) {
        if (c == 0xD7) continue;
        CHECKF((ULONG_PTR)U(CharLowerA)(G1_LP(c)) == (ULONG_PTR)(c + 0x20), "c=%x", c);
    }
    CHECK((ULONG_PTR)U(CharUpperA)(G1_LP(0xDF)) == 0xDF);
    CHECK((ULONG_PTR)U(CharUpperA)(G1_LP(0xF7)) == 0xF7);
    CHECK((ULONG_PTR)U(CharLowerA)(G1_LP(0xD7)) == 0xD7);
}
TEST(gap1_charuppera_string_cp1252_high_chars) {
    REQUIRE(U(CharUpperA)); REQUIRE(U(CharLowerA));
    if (GetACP() != 1252) { t_note("ACP=%u", GetACP()); return; }
    char b[] = "\xE4\xF6\xFC\xE9 abc";
    U(CharUpperA)(b);
    EXPECT_STR(b, "\xC4\xD6\xDC\xC9 ABC");
    U(CharLowerA)(b);
    EXPECT_STR(b, "\xE4\xF6\xFC\xE9 abc");
}
TEST(gap1_charuppera_idempotent_and_roundtrip) {
    REQUIRE(U(CharUpperA)); REQUIRE(U(CharLowerA));
    char b[64]; strcpy(b, "MiXeD CaSe_123");
    U(CharUpperA)(b); U(CharUpperA)(b);
    EXPECT_STR(b, "MIXED CASE_123");
    U(CharLowerA)(b); U(CharLowerA)(b);
    EXPECT_STR(b, "mixed case_123");
}

/* ---------- CharUpperBuffA / CharLowerBuffA ---------- */
TEST(gap1_charupperbuffa_zero_len) {
    REQUIRE(U(CharUpperBuffA));
    char b[] = "abc";
    EXPECT_EQ(U(CharUpperBuffA)(b, 0), 0);
    EXPECT_STR(b, "abc");
}
TEST(gap1_charlowerbuffa_zero_len) {
    REQUIRE(U(CharLowerBuffA));
    char b[] = "ABC";
    EXPECT_EQ(U(CharLowerBuffA)(b, 0), 0);
    EXPECT_STR(b, "ABC");
}
TEST(gap1_charupperbuffa_zero_len_null_ptr) {
    REQUIRE(U(CharUpperBuffA)); REQUIRE(U(CharLowerBuffA)); REQUIRE(U(CharUpperBuffW)); REQUIRE(U(CharLowerBuffW));
    EXPECT_EQ(U(CharUpperBuffA)(NULL, 0), 0);
    EXPECT_EQ(U(CharLowerBuffA)(NULL, 0), 0);
    EXPECT_EQ(U(CharUpperBuffW)(NULL, 0), 0);
    EXPECT_EQ(U(CharLowerBuffW)(NULL, 0), 0);
}
TEST(gap1_charupperbuffa_partial) {
    REQUIRE(U(CharUpperBuffA));
    static const struct { const char *in, *out; DWORD n; } t[] = {
        { "abcdef", "ABCdef", 3 }, { "abcdef", "ABCDEF", 6 }, { "abcdef", "Abcdef", 1 },
        { "abcdef", "abcdef", 0 }, { "a1b2c3", "A1B2C3", 6 }, { "xyz", "XYz", 2 },
    };
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++) {
        char b[32]; strcpy(b, t[i].in);
        DWORD r = U(CharUpperBuffA)(b, t[i].n);
        CHECKF(r == t[i].n, "case %zu ret %lu want %lu", i, r, t[i].n);
        CHECKF(!strcmp(b, t[i].out), "case %zu got %s want %s", i, b, t[i].out);
    }
}
TEST(gap1_charlowerbuffa_partial) {
    REQUIRE(U(CharLowerBuffA));
    static const struct { const char *in, *out; DWORD n; } t[] = {
        { "ABCDEF", "abcDEF", 3 }, { "ABCDEF", "abcdef", 6 }, { "ABCDEF", "aBCDEF", 1 },
        { "ABCDEF", "ABCDEF", 0 }, { "A1B2C3", "a1b2c3", 6 }, { "XYZ", "xyZ", 2 },
    };
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++) {
        char b[32]; strcpy(b, t[i].in);
        DWORD r = U(CharLowerBuffA)(b, t[i].n);
        CHECKF(r == t[i].n, "case %zu ret %lu want %lu", i, r, t[i].n);
        CHECKF(!strcmp(b, t[i].out), "case %zu got %s want %s", i, b, t[i].out);
    }
}
TEST(gap1_charbuffa_embedded_nul_processed) {
    REQUIRE(U(CharUpperBuffA)); REQUIRE(U(CharLowerBuffA));
    char b[] = { 'a', 0, 'b', 'c', 0 };
    EXPECT_EQ(U(CharUpperBuffA)(b, 4), 4);
    EXPECT_EQ(b[0], 'A'); EXPECT_EQ(b[1], 0); EXPECT_EQ(b[2], 'B'); EXPECT_EQ(b[3], 'C');
    EXPECT_EQ(U(CharLowerBuffA)(b, 4), 4);
    EXPECT_EQ(b[0], 'a'); EXPECT_EQ(b[2], 'b'); EXPECT_EQ(b[3], 'c');
}
TEST(gap1_charbuffa_no_nul_terminator_needed) {
    REQUIRE(U(CharUpperBuffA));
    char b[5]; memcpy(b, "abcde", 5);
    EXPECT_EQ(U(CharUpperBuffA)(b, 5), 5);
    CHECK(!memcmp(b, "ABCDE", 5));
}
TEST(gap1_charbuffa_large_buffer) {
    REQUIRE(U(CharUpperBuffA)); REQUIRE(U(CharLowerBuffA));
    enum { N = 5000 };
    char *b = (char *)HeapAlloc(GetProcessHeap(), 0, N + 1);
    CHECK(b != NULL); if (!b) return;
    memset(b, 'q', N); b[N] = 0;
    EXPECT_EQ(U(CharUpperBuffA)(b, N), N);
    int ok = 1; for (int i = 0; i < N; i++) if (b[i] != 'Q') ok = 0;
    CHECK(ok);
    EXPECT_EQ(U(CharLowerBuffA)(b, N), N);
    ok = 1; for (int i = 0; i < N; i++) if (b[i] != 'q') ok = 0;
    CHECK(ok);
    HeapFree(GetProcessHeap(), 0, b);
}
TEST(gap1_charbuffa_boundary_sizes) {
    REQUIRE(U(CharUpperBuffA));
    static const int sizes[] = { 1, 2, 255, 256, 257, 258, 511, 512, 513 };
    for (size_t k = 0; k < sizeof sizes / sizeof sizes[0]; k++) {
        int n = sizes[k];
        char *b = (char *)HeapAlloc(GetProcessHeap(), 0, n + 1);
        if (!b) { CHECK(0); return; }
        for (int i = 0; i < n; i++) b[i] = 'a' + (i % 26);
        b[n] = 'z';
        DWORD r = U(CharUpperBuffA)(b, n);
        CHECKF(r == (DWORD)n, "n=%d ret=%lu", n, r);
        int ok = 1;
        for (int i = 0; i < n; i++) if (b[i] != 'A' + (i % 26)) ok = 0;
        CHECKF(ok, "n=%d content", n);
        CHECKF(b[n] == 'z', "n=%d overrun", n);
        HeapFree(GetProcessHeap(), 0, b);
    }
}
TEST(gap1_charbuffa_all_ascii_bytes) {
    REQUIRE(U(CharUpperBuffA)); REQUIRE(U(CharLowerBuffA));
    char u[127], l[127];
    for (int i = 0; i < 127; i++) u[i] = l[i] = (char)(i + 1);
    EXPECT_EQ(U(CharUpperBuffA)(u, 127), 127);
    EXPECT_EQ(U(CharLowerBuffA)(l, 127), 127);
    for (int c = 1; c < 128; c++) {
        int wu = (c >= 'a' && c <= 'z') ? c - 32 : c;
        int wl = (c >= 'A' && c <= 'Z') ? c + 32 : c;
        CHECKF((unsigned char)u[c - 1] == wu, "up %d", c);
        CHECKF((unsigned char)l[c - 1] == wl, "lo %d", c);
    }
}

/* ---------- CharUpperW / CharLowerW (string + single char) ---------- */
TEST(gap1_charupperw_string_inplace) {
    REQUIRE(U(CharUpperW));
    WCHAR b[] = L"Hello, World 123 xyz!";
    CHECK(U(CharUpperW)(b) == b);
    EXPECT_WSTR(b, L"HELLO, WORLD 123 XYZ!");
}
TEST(gap1_charlowerw_string_inplace) {
    REQUIRE(U(CharLowerW));
    WCHAR b[] = L"Hello, World 123 XYZ!";
    CHECK(U(CharLowerW)(b) == b);
    EXPECT_WSTR(b, L"hello, world 123 xyz!");
}
TEST(gap1_charupperw_empty) {
    REQUIRE(U(CharUpperW)); REQUIRE(U(CharLowerW));
    WCHAR b[2] = L"";
    CHECK(U(CharUpperW)(b) == b);
    CHECK(U(CharLowerW)(b) == b);
    EXPECT_EQ(b[0], 0);
}
TEST(gap1_charupperw_single_ascii_all) {
    REQUIRE(U(CharUpperW)); REQUIRE(U(CharLowerW));
    for (int c = 1; c < 128; c++) {
        int wu = (c >= 'a' && c <= 'z') ? c - 32 : c;
        int wl = (c >= 'A' && c <= 'Z') ? c + 32 : c;
        CHECKF((ULONG_PTR)U(CharUpperW)(G1_LPW(c)) == (ULONG_PTR)wu, "up %d", c);
        CHECKF((ULONG_PTR)U(CharLowerW)(G1_LPW(c)) == (ULONG_PTR)wl, "lo %d", c);
    }
}
TEST(gap1_charupperw_latin1_single) {
    REQUIRE(U(CharUpperW)); REQUIRE(U(CharLowerW));
    for (int c = 0xE0; c <= 0xFE; c++) {
        if (c == 0xF7) continue;
        CHECKF((ULONG_PTR)U(CharUpperW)(G1_LPW(c)) == (ULONG_PTR)(c - 0x20), "up %x", c);
    }
    for (int c = 0xC0; c <= 0xDE; c++) {
        if (c == 0xD7) continue;
        CHECKF((ULONG_PTR)U(CharLowerW)(G1_LPW(c)) == (ULONG_PTR)(c + 0x20), "lo %x", c);
    }
    CHECK((ULONG_PTR)U(CharUpperW)(G1_LPW(0xDF)) == 0xDF);
    CHECK((ULONG_PTR)U(CharUpperW)(G1_LPW(0xF7)) == 0xF7);
    CHECK((ULONG_PTR)U(CharLowerW)(G1_LPW(0xD7)) == 0xD7);
    t_note("U+00FF -> U+%04X, U+00B5 -> U+%04X", (unsigned)(ULONG_PTR)U(CharUpperW)(G1_LPW(0xFF)), (unsigned)(ULONG_PTR)U(CharUpperW)(G1_LPW(0xB5)));
}
TEST(gap1_charupperw_greek_cyrillic_single) {
    REQUIRE(U(CharUpperW)); REQUIRE(U(CharLowerW));
    for (int c = 0x3B1; c <= 0x3C9; c++) {
        if (c == 0x3C2) continue;
        CHECKF((ULONG_PTR)U(CharUpperW)(G1_LPW(c)) == (ULONG_PTR)(c - 0x20), "up %x", c);
    }
    for (int c = 0x391; c <= 0x3A9; c++) {
        if (c == 0x3A2) continue;
        CHECKF((ULONG_PTR)U(CharLowerW)(G1_LPW(c)) == (ULONG_PTR)(c + 0x20), "lo %x", c);
    }
    for (int c = 0x430; c <= 0x44F; c++) CHECKF((ULONG_PTR)U(CharUpperW)(G1_LPW(c)) == (ULONG_PTR)(c - 0x20), "up %x", c);
    for (int c = 0x410; c <= 0x42F; c++) CHECKF((ULONG_PTR)U(CharLowerW)(G1_LPW(c)) == (ULONG_PTR)(c + 0x20), "lo %x", c);
    for (int c = 0x450; c <= 0x45F; c++) CHECKF((ULONG_PTR)U(CharUpperW)(G1_LPW(c)) == (ULONG_PTR)(c - 0x50), "up %x", c);
    for (int c = 0x400; c <= 0x40F; c++) CHECKF((ULONG_PTR)U(CharLowerW)(G1_LPW(c)) == (ULONG_PTR)(c + 0x50), "lo %x", c);
    t_note("final sigma upper -> U+%04X", (unsigned)(ULONG_PTR)U(CharUpperW)(G1_LPW(0x3C2)));
}
TEST(gap1_charupperw_caseless_unchanged) {
    REQUIRE(U(CharUpperW)); REQUIRE(U(CharLowerW));
    static const unsigned short v[] = { 0x05D0 /*hebrew alef*/, 0x4E2D /*CJK*/, 0x3042 /*hiragana*/, 0x0661, 0x2603, 0xFFFD };
    for (size_t i = 0; i < sizeof v / sizeof v[0]; i++) {
        CHECKF((ULONG_PTR)U(CharUpperW)(G1_LPW(v[i])) == v[i], "up %x", v[i]);
        CHECKF((ULONG_PTR)U(CharLowerW)(G1_LPW(v[i])) == v[i], "lo %x", v[i]);
    }
}
TEST(gap1_charupperw_unicode_string) {
    REQUIRE(U(CharUpperW)); REQUIRE(U(CharLowerW));
    WCHAR b[] = { 0xE4, 0xF6, 0x3B1, 0x3B2, 0x430, 'a', 0x4E2D, 0 };
    U(CharUpperW)(b);
    EXPECT_EQ(b[0], 0xC4); EXPECT_EQ(b[1], 0xD6); EXPECT_EQ(b[2], 0x391); EXPECT_EQ(b[3], 0x392); EXPECT_EQ(b[4], 0x410); EXPECT_EQ(b[5], 'A'); EXPECT_EQ(b[6], 0x4E2D);
    U(CharLowerW)(b);
    EXPECT_EQ(b[0], 0xE4); EXPECT_EQ(b[1], 0xF6); EXPECT_EQ(b[2], 0x3B1); EXPECT_EQ(b[3], 0x3B2); EXPECT_EQ(b[4], 0x430); EXPECT_EQ(b[5], 'a');
}
TEST(gap1_charupperw_full_range_idempotent) {
    /* upper(upper(x)) == upper(x), lower(lower(x)) == lower(x) for the BMP below 0x2000 */
    REQUIRE(U(CharUpperW)); REQUIRE(U(CharLowerW));
    int bad = 0, nonidem = 0;
    for (int c = 1; c < 0x2000; c++) {
        ULONG_PTR u = (ULONG_PTR)U(CharUpperW)(G1_LPW(c));
        ULONG_PTR l = (ULONG_PTR)U(CharLowerW)(G1_LPW(c));
        if (u == 0 || l == 0) bad++;
        if ((ULONG_PTR)U(CharUpperW)((LPWSTR)u) != u) nonidem++;
    }
    CHECKF(bad == 0, "bad=%d", bad);
    if (nonidem) t_note("%d chars where upper(upper(c)) != upper(c)", nonidem);
}

/* ---------- CharUpperBuffW / CharLowerBuffW ---------- */
TEST(gap1_charupperbuffw_zero) {
    REQUIRE(U(CharUpperBuffW)); REQUIRE(U(CharLowerBuffW));
    WCHAR b[] = L"abc";
    EXPECT_EQ(U(CharUpperBuffW)(b, 0), 0);
    EXPECT_WSTR(b, L"abc");
    WCHAR c[] = L"ABC";
    EXPECT_EQ(U(CharLowerBuffW)(c, 0), 0);
    EXPECT_WSTR(c, L"ABC");
}
TEST(gap1_charupperbuffw_partial) {
    REQUIRE(U(CharUpperBuffW)); REQUIRE(U(CharLowerBuffW));
    WCHAR b[] = L"abcdef";
    EXPECT_EQ(U(CharUpperBuffW)(b, 3), 3);
    EXPECT_WSTR(b, L"ABCdef");
    EXPECT_EQ(U(CharUpperBuffW)(b, 6), 6);
    EXPECT_WSTR(b, L"ABCDEF");
    EXPECT_EQ(U(CharLowerBuffW)(b, 2), 2);
    EXPECT_WSTR(b, L"abCDEF");
    EXPECT_EQ(U(CharLowerBuffW)(b, 6), 6);
    EXPECT_WSTR(b, L"abcdef");
}
TEST(gap1_charbuffw_embedded_nul) {
    REQUIRE(U(CharUpperBuffW)); REQUIRE(U(CharLowerBuffW));
    WCHAR b[] = { 'a', 0, 'b', 'c', 0 };
    EXPECT_EQ(U(CharUpperBuffW)(b, 4), 4);
    EXPECT_EQ(b[0], 'A'); EXPECT_EQ(b[1], 0); EXPECT_EQ(b[2], 'B'); EXPECT_EQ(b[3], 'C');
    EXPECT_EQ(U(CharLowerBuffW)(b, 4), 4);
    EXPECT_EQ(b[0], 'a'); EXPECT_EQ(b[2], 'b'); EXPECT_EQ(b[3], 'c');
}
TEST(gap1_charbuffw_no_overrun) {
    REQUIRE(U(CharUpperBuffW));
    WCHAR b[8]; for (int i = 0; i < 8; i++) b[i] = 'a';
    EXPECT_EQ(U(CharUpperBuffW)(b, 5), 5);
    for (int i = 0; i < 5; i++) EXPECT_EQ(b[i], 'A');
    for (int i = 5; i < 8; i++) EXPECT_EQ(b[i], 'a');
}
TEST(gap1_charbuffw_large) {
    REQUIRE(U(CharUpperBuffW)); REQUIRE(U(CharLowerBuffW));
    enum { N = 20000 };
    WCHAR *b = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, (N + 1) * sizeof(WCHAR));
    CHECK(b != NULL); if (!b) return;
    for (int i = 0; i < N; i++) b[i] = (i & 1) ? 0xE4 : 'x';
    b[N] = 'z';
    EXPECT_EQ(U(CharUpperBuffW)(b, N), N);
    int ok = 1;
    for (int i = 0; i < N; i++) if (b[i] != ((i & 1) ? 0xC4 : 'X')) ok = 0;
    CHECK(ok); EXPECT_EQ(b[N], 'z');
    EXPECT_EQ(U(CharLowerBuffW)(b, N), N);
    ok = 1;
    for (int i = 0; i < N; i++) if (b[i] != ((i & 1) ? 0xE4 : 'x')) ok = 0;
    CHECK(ok);
    HeapFree(GetProcessHeap(), 0, b);
}
TEST(gap1_charbuffw_ret_matches_a_variant) {
    REQUIRE(U(CharUpperBuffW)); REQUIRE(U(CharUpperBuffA));
    for (DWORD n = 0; n < 40; n++) {
        char a[64]; WCHAR w[64];
        for (int i = 0; i < 64; i++) { a[i] = 'a' + (i % 26); w[i] = 'a' + (i % 26); }
        CHECKF(U(CharUpperBuffA)(a, n) == U(CharUpperBuffW)(w, n), "n=%lu", n);
        for (DWORD i = 0; i < n; i++) CHECK(a[i] == (char)w[i]);
    }
}
TEST(gap1_charw_vs_a_ascii_parity) {
    REQUIRE(U(CharUpperW)); REQUIRE(U(CharUpperA)); REQUIRE(U(CharLowerW)); REQUIRE(U(CharLowerA));
    for (int c = 1; c < 128; c++) {
        CHECK((ULONG_PTR)U(CharUpperA)(G1_LP(c)) == (ULONG_PTR)U(CharUpperW)(G1_LPW(c)));
        CHECK((ULONG_PTR)U(CharLowerA)(G1_LP(c)) == (ULONG_PTR)U(CharLowerW)(G1_LPW(c)));
    }
}

/* ---------- CharNext / CharPrev ---------- */
TEST(gap1_charnexta_basic) {
    REQUIRE(U(CharNextA));
    const char *s = "abc";
    CHECK(U(CharNextA)(s) == s + 1);
    CHECK(U(CharNextA)(s + 1) == s + 2);
    CHECK(U(CharNextA)(s + 2) == s + 3);
}
TEST(gap1_charnexta_at_nul_stays) {
    REQUIRE(U(CharNextA));
    const char *s = "abc";
    CHECK(U(CharNextA)(s + 3) == s + 3);
    CHECK(U(CharNextA)("") != NULL);
    const char e[2] = "";
    CHECK(U(CharNextA)(e) == e);
}
TEST(gap1_charnexta_walk_string) {
    REQUIRE(U(CharNextA));
    const char *s = "The quick brown fox";
    int n = 0;
    for (const char *p = s; *p; p = U(CharNextA)(p)) { n++; if (n > 100) break; }
    if (GetACP() != 932 && GetACP() != 936 && GetACP() != 949 && GetACP() != 950) EXPECT_EQ(n, (int)strlen(s));
}
TEST(gap1_charnextw_basic) {
    REQUIRE(U(CharNextW));
    const WCHAR *s = L"abc";
    CHECK(U(CharNextW)(s) == s + 1);
    CHECK(U(CharNextW)(s + 2) == s + 3);
    CHECK(U(CharNextW)(s + 3) == s + 3);
}
TEST(gap1_charnextw_empty_stays) {
    REQUIRE(U(CharNextW));
    const WCHAR e[2] = L"";
    CHECK(U(CharNextW)(e) == e);
}
TEST(gap1_charnextw_walk_ascii) {
    REQUIRE(U(CharNextW));
    const WCHAR *s = L"The quick brown fox";
    int n = 0;
    for (const WCHAR *p = s; *p; p = U(CharNextW)(p)) { n++; if (n > 100) break; }
    EXPECT_EQ(n, 19);
}
TEST(gap1_charnextw_skips_combining_mark) {
    /* U+0301 combining acute follows 'e': CharNextW treats base+nonspacing mark as one step */
    REQUIRE(U(CharNextW));
    const WCHAR s[] = { 'e', 0x0301, 'x', 0 };
    const WCHAR *r = U(CharNextW)(s);
    t_note("CharNextW(e+U+0301) advanced %d", (int)(r - s));
    CHECK(r == s + 2 || r == s + 1);
    CHECK(U(CharNextW)(s + 2) == s + 3);
}
TEST(gap1_charprevw_basic) {
    REQUIRE(U(CharPrevW));
    const WCHAR *s = L"abc";
    CHECK(U(CharPrevW)(s, s + 3) == s + 2);
    CHECK(U(CharPrevW)(s, s + 2) == s + 1);
    CHECK(U(CharPrevW)(s, s + 1) == s);
}
TEST(gap1_charprevw_at_start_stays) {
    REQUIRE(U(CharPrevW));
    const WCHAR *s = L"abc";
    CHECK(U(CharPrevW)(s, s) == s);
}
TEST(gap1_charprevw_current_before_start_returns_current) {
    REQUIRE(U(CharPrevW));
    const WCHAR *s = L"abc";
    CHECK(U(CharPrevW)(s + 2, s + 1) == s + 1);
    CHECK(U(CharPrevW)(s + 2, s) == s);
}
TEST(gap1_charprevw_combining_mark) {
    REQUIRE(U(CharPrevW));
    const WCHAR s[] = { 'a', 'e', 0x0301, 'x', 0 };
    const WCHAR *r = U(CharPrevW)(s, s + 3);
    t_note("CharPrevW over e+U+0301 -> index %d", (int)(r - s));
    CHECK(r == s + 1 || r == s + 2);
    CHECK(U(CharPrevW)(s, s + 4) == s + 3);
}
TEST(gap1_charprev_a_basic) {
    REQUIRE(U(CharPrevA));
    const char *s = "abc";
    CHECK(U(CharPrevA)(s, s) == s);
    CHECK(U(CharPrevA)(s, s + 1) == s);
    CHECK(U(CharPrevA)(s, s + 3) == s + 2);
    CHECK(U(CharPrevA)(s + 2, s + 1) == s + 1);
}
TEST(gap1_charprev_a_walk_back) {
    REQUIRE(U(CharPrevA));
    const char *s = "abcdef";
    const char *p = s + 6;
    int n = 0;
    while (p > s && n < 20) { p = U(CharPrevA)(s, p); n++; }
    if (GetACP() == 1252) { EXPECT_EQ(n, 6); CHECK(p == s); }
}

/* DBCS tables via explicit code page 932 (Shift-JIS) */
static int have_cp932(void) {
    CPINFO ci;
    return GetCPInfo(CP_SJIS, &ci) && ci.MaxCharSize == 2;
}
TEST(gap1_charnextexa_null) {
    REQUIRE(U(CharNextExA));
    if (t_is_wine()) { t_skip("faults on NULL under wine"); return; }
    CHECK(U(CharNextExA)(1252, NULL, 0) == NULL);
    CHECK(U(CharNextExA)(CP_SJIS, NULL, 0) == NULL);
    CHECK(U(CharNextExA)(0, NULL, 0) == NULL);
}
TEST(gap1_charnextexa_sbcs) {
    REQUIRE(U(CharNextExA));
    const char *s = "abc";
    CHECK(U(CharNextExA)(1252, s, 0) == s + 1);
    CHECK(U(CharNextExA)(1252, s + 2, 0) == s + 3);
    CHECK(U(CharNextExA)(1252, s + 3, 0) == s + 3);
    CHECK(U(CharNextExA)(1252, "\xE4z", 0) != NULL);
}
TEST(gap1_charnextexa_flags_ignored) {
    REQUIRE(U(CharNextExA));
    const char *s = "abc";
    CHECK(U(CharNextExA)(1252, s, 0xFFFFFFFF) == s + 1);
    CHECK(U(CharNextExA)(1252, s, 1) == s + 1);
}
TEST(gap1_charnextexa_at_nul_stays) {
    REQUIRE(U(CharNextExA));
    const char e[4] = "";
    CHECK(U(CharNextExA)(1252, e, 0) == e);
    if (have_cp932()) CHECK(U(CharNextExA)(CP_SJIS, e, 0) == e);
}
TEST(gap1_charnextexa_sjis_lead_trail) {
    REQUIRE(U(CharNextExA)); if (!have_cp932()) { t_skip("cp932 unavailable"); return; }
    const char s[] = "\x81\x40" "a\x82\xA0z";
    CHECK(U(CharNextExA)(CP_SJIS, s, 0) == s + 2);
    CHECK(U(CharNextExA)(CP_SJIS, s + 2, 0) == s + 3);
    CHECK(U(CharNextExA)(CP_SJIS, s + 3, 0) == s + 5);
    CHECK(U(CharNextExA)(CP_SJIS, s + 5, 0) == s + 6);
}
TEST(gap1_charnextexa_sjis_lead_then_nul) {
    /* lead byte followed by NUL: pointer is advanced over the lead byte only */
    REQUIRE(U(CharNextExA)); if (!have_cp932()) { t_skip("cp932 unavailable"); return; }
    const char s[] = { (char)0x81, 0, 'a' };
    CHECK(U(CharNextExA)(CP_SJIS, s, 0) == s + 1);
}
TEST(gap1_charnextexa_sjis_lead_ranges) {
    REQUIRE(U(CharNextExA)); if (!have_cp932()) { t_skip("cp932 unavailable"); return; }
    for (int c = 0x81; c <= 0x9F; c++) { char s[4] = { (char)c, 0x40, 'a', 0 }; CHECKF(U(CharNextExA)(CP_SJIS, s, 0) == s + 2, "lead %x", c); }
    for (int c = 0xE0; c <= 0xFC; c++) { char s[4] = { (char)c, 0x40, 'a', 0 }; CHECKF(U(CharNextExA)(CP_SJIS, s, 0) == s + 2, "lead %x", c); }
    for (int c = 0x20; c <= 0x7E; c++) { char s[4] = { (char)c, 0x40, 'a', 0 }; CHECKF(U(CharNextExA)(CP_SJIS, s, 0) == s + 1, "sb %x", c); }
    for (int c = 0xA1; c <= 0xDF; c++) { char s[4] = { (char)c, 0x40, 'a', 0 }; CHECKF(U(CharNextExA)(CP_SJIS, s, 0) == s + 1, "halfwidth kana %x", c); }
}
TEST(gap1_charnextexa_sbcs_ignores_dbcs_bytes) {
    REQUIRE(U(CharNextExA));
    const char s[] = "\x81\x40z";
    CHECK(U(CharNextExA)(1252, s, 0) == s + 1);
}
TEST(gap1_charprevexa_basic_sbcs) {
    REQUIRE(U(CharPrevExA));
    const char *s = "abc";
    CHECK(U(CharPrevExA)(1252, s, s + 3, 0) == s + 2);
    CHECK(U(CharPrevExA)(1252, s, s + 1, 0) == s);
    CHECK(U(CharPrevExA)(1252, s, s, 0) == s);
    CHECK(U(CharPrevExA)(1252, s + 2, s + 1, 0) == s + 1);
}
TEST(gap1_charprevexa_flags_ignored) {
    REQUIRE(U(CharPrevExA));
    const char *s = "abc";
    CHECK(U(CharPrevExA)(1252, s, s + 2, 0xFFFFFFFF) == s + 1);
}
TEST(gap1_charprevexa_sjis_pairs) {
    REQUIRE(U(CharPrevExA)); if (!have_cp932()) { t_skip("cp932 unavailable"); return; }
    const char s[] = "a\x81\x40" "b\x82\xA0z";
    CHECK(U(CharPrevExA)(CP_SJIS, s, s + 3, 0) == s + 1);
    CHECK(U(CharPrevExA)(CP_SJIS, s, s + 4, 0) == s + 3);
    CHECK(U(CharPrevExA)(CP_SJIS, s, s + 6, 0) == s + 4);
    CHECK(U(CharPrevExA)(CP_SJIS, s, s + 7, 0) == s + 6);
    CHECK(U(CharPrevExA)(CP_SJIS, s, s + 1, 0) == s);
}
TEST(gap1_charprevexa_sjis_trail_that_looks_like_lead) {
    REQUIRE(U(CharPrevExA)); if (!have_cp932()) { t_skip("cp932 unavailable"); return; }
    /* 0x81 0x81 0x81 0x81: two pairs; walking back from the end steps by two */
    const char s[] = "\x81\x81\x81\x81z";
    CHECK(U(CharPrevExA)(CP_SJIS, s, s + 4, 0) == s + 2);
    CHECK(U(CharPrevExA)(CP_SJIS, s, s + 2, 0) == s);
    CHECK(U(CharPrevExA)(CP_SJIS, s, s + 5, 0) == s + 4);
}
TEST(gap1_charprevexa_sjis_odd_run_of_leads) {
    REQUIRE(U(CharPrevExA)); if (!have_cp932()) { t_skip("cp932 unavailable"); return; }
    /* 81 81 81 61: pairing is counted from the start of the buffer, backwards over the lead run */
    const char s[] = "\x81\x81\x81" "a";
    CHECK(U(CharPrevExA)(CP_SJIS, s, s + 3, 0) == s + 2);
    CHECK(U(CharPrevExA)(CP_SJIS, s, s + 4, 0) == s + 2);
}
TEST(gap1_charprevexa_start_after_current) {
    REQUIRE(U(CharPrevExA));
    const char *s = "abc";
    CHECK(U(CharPrevExA)(1252, s + 3, s, 0) == s);
}
TEST(gap1_charprev_next_inverse_ascii) {
    REQUIRE(U(CharPrevExA)); REQUIRE(U(CharNextExA));
    const char *s = "hello world";
    for (const char *p = s; *p; p++) {
        const char *n = U(CharNextExA)(1252, p, 0);
        CHECK(U(CharPrevExA)(1252, s, n, 0) == p);
    }
}
TEST(gap1_charprev_next_inverse_w_ascii) {
    REQUIRE(U(CharPrevW)); REQUIRE(U(CharNextW));
    const WCHAR *s = L"hello world";
    for (const WCHAR *p = s; *p; p++) {
        const WCHAR *n = U(CharNextW)(p);
        CHECK(U(CharPrevW)(s, n) == p);
    }
}
