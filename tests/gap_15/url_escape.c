#include "gap15.h"

#define FA(n) ((g_fnA4)U(n##A))
#define FW(n) ((g_fnW4)U(n##W))
#define NEED2(n) REQUIRE(U(n##A)); REQUIRE(U(n##W))

/* ------------------------------------------------------------------ UrlEscape */
TEST(gap15_urlescape_args) {
    NEED2(UrlEscape);
    g_args4("UrlEscape", FA(UrlEscape), FW(UrlEscape), "http://a/b c", 0);
    g_args4("UrlEscape", FA(UrlEscape), FW(UrlEscape), "http://a/b c", URL_ESCAPE_SPACES_ONLY);
    g_args4("UrlEscape", FA(UrlEscape), FW(UrlEscape), "http://a/b c", URL_ESCAPE_PERCENT);
}
TEST(gap15_urlescape_a_unsupported_flags) {
    REQUIRE(U(UrlEscapeA));
    /* decomp: the ANSI entry rejects URL_ESCAPE_AS_UTF8 / URL_ESCAPE_ASCII_URI_COMPONENT */
    char out[64]; DWORD n;
    static const DWORD fl[] = { 0x40000, 0x80000, 0xc0000 };
    for (size_t i = 0; i < 3; i++) {
        n = 64; HRESULT hr = U(UrlEscapeA)("http://a/b c", out, &n, fl[i]);
        CHECKF(hr == G_E_NOTIMPL, "flags 0x%lx hr=0x%lx", (unsigned long)fl[i], (unsigned long)hr);
    }
}
TEST(gap15_urlescape_basic) {
    NEED2(UrlEscape);
    static const g_case4 c[] = {
        { "http://a/b c", 0, S_OK, "http://a/b%20c", 1 },
        { "http://a/b c d", 0, S_OK, "http://a/b%20c%20d", 1 },
        { "http://a/ ", 0, S_OK, "http://a/%20", 1 },
        { "http://a/b", 0, S_OK, "http://a/b", 1 },
        { "http://a/b<c", 0, S_OK, "http://a/b%3Cc", 1 },
        { "http://a/b>c", 0, S_OK, "http://a/b%3Ec", 1 },
        { "http://a/b\"c", 0, S_OK, "http://a/b%22c", 1 },
        { "http://a/%20", 0, S_OK, "http://a/%20", 1 },
        { "http://a/100%", 0, S_OK, "http://a/100%", 1 },
        { "http://a/b{c}", 0, S_OK, NULL, 0 },
        { "http://a/b|c", 0, S_OK, NULL, 0 },
        { "http://a/b^c", 0, S_OK, NULL, 0 },
        { "http://a/b`c", 0, S_OK, NULL, 0 },
        { "http://a/b[c]", 0, S_OK, NULL, 0 },
        { "http://a/b\\c", 0, S_OK, NULL, 0 },
        { "http://a/b\001c", 0, S_OK, NULL, 0 },
        { "http://a/b\177c", 0, S_OK, NULL, 0 },
        { "http://a/b?c d#e f", 0, S_OK, NULL, 0 },
        { "http://a/b~c-d_e.f", 0, S_OK, "http://a/b~c-d_e.f", 1 },
        { "http://a/b c?q=1 2", 0, S_OK, NULL, 0 },
        { "", 0, S_OK, NULL, 0 },
        { "a b", 0, S_OK, NULL, 0 },
        { "mailto:a b@c", 0, S_OK, NULL, 0 },
        { "file:///c:/a b/c", 0, S_OK, "file:///c:/a%20b/c", 1 },
    };
    g_check4("UrlEscape", FA(UrlEscape), FW(UrlEscape), c, sizeof c / sizeof *c);
}
TEST(gap15_urlescape_percent_flag) {
    NEED2(UrlEscape);
    static const g_case4 c[] = {
        { "http://a/100%", URL_ESCAPE_PERCENT, S_OK, "http://a/100%25", 1 },
        { "http://a/%20", URL_ESCAPE_PERCENT, S_OK, "http://a/%2520", 1 },
        { "http://a/a%b", URL_ESCAPE_PERCENT, S_OK, "http://a/a%25b", 1 },
        { "http://a/a b%", URL_ESCAPE_PERCENT, S_OK, "http://a/a%20b%25", 1 },
        { "http://a/abc", URL_ESCAPE_PERCENT, S_OK, "http://a/abc", 1 },
    };
    g_check4("UrlEscape", FA(UrlEscape), FW(UrlEscape), c, sizeof c / sizeof *c);
}
TEST(gap15_urlescape_spaces_only) {
    NEED2(UrlEscape);
    static const g_case4 c[] = {
        { "http://a/b c", URL_ESCAPE_SPACES_ONLY, S_OK, "http://a/b%20c", 1 },
        { "a b c", URL_ESCAPE_SPACES_ONLY, S_OK, "a%20b%20c", 1 },
        { "a<b c>", URL_ESCAPE_SPACES_ONLY, S_OK, "a<b%20c>", 1 },
        { " ", URL_ESCAPE_SPACES_ONLY, S_OK, "%20", 1 },
        { "  ", URL_ESCAPE_SPACES_ONLY, S_OK, "%20%20", 1 },
        { "abc", URL_ESCAPE_SPACES_ONLY, S_OK, "abc", 1 },
        { "a%b", URL_ESCAPE_SPACES_ONLY, S_OK, "a%b", 1 },
        { "a\"b", URL_ESCAPE_SPACES_ONLY, S_OK, "a\"b", 1 },
    };
    g_check4("UrlEscape", FA(UrlEscape), FW(UrlEscape), c, sizeof c / sizeof *c);
}
TEST(gap15_urlescape_segment_only) {
    NEED2(UrlEscape);
    static const g_case4 c[] = {
        { "a b", URL_ESCAPE_SEGMENT_ONLY, S_OK, "a%20b", 1 },
        { "abc", URL_ESCAPE_SEGMENT_ONLY, S_OK, "abc", 1 },
        { "a?b", URL_ESCAPE_SEGMENT_ONLY, S_OK, NULL, 0 },
        { "a#b", URL_ESCAPE_SEGMENT_ONLY, S_OK, NULL, 0 },
        { "a/b", URL_ESCAPE_SEGMENT_ONLY, S_OK, NULL, 0 },
        { "a\\b", URL_ESCAPE_SEGMENT_ONLY, S_OK, NULL, 0 },
        { "a%b", URL_ESCAPE_SEGMENT_ONLY | URL_ESCAPE_PERCENT, S_OK, "a%25b", 1 },
        { "http://a/b c", URL_ESCAPE_SEGMENT_ONLY, S_OK, NULL, 0 },
    };
    g_check4("UrlEscape", FA(UrlEscape), FW(UrlEscape), c, sizeof c / sizeof *c);
}
TEST(gap15_urlescape_truncation) {
    NEED2(UrlEscape);
    g_trunc4("UrlEscape", FA(UrlEscape), FW(UrlEscape), "http://a/b c", 0, "http://a/b%20c");
    g_trunc4("UrlEscape", FA(UrlEscape), FW(UrlEscape), "x y", URL_ESCAPE_SPACES_ONLY, "x%20y");
    g_trunc4("UrlEscape", FA(UrlEscape), FW(UrlEscape), "http://a/b", 0, "http://a/b");
}
TEST(gap15_urlescape_repeat_idempotent) {
    NEED2(UrlEscape);
    g_res r1, r2;
    g_run4(FA(UrlEscape), FW(UrlEscape), "http://a/b c", 0, 256, &r1);
    for (int i = 0; i < 50; i++) {
        g_run4(FA(UrlEscape), FW(UrlEscape), "http://a/b c", 0, 256, &r2);
        CHECK(!strcmp(r1.a, r2.a));
    }
    /* escaping an escaped URL changes nothing */
    g_run4(FA(UrlEscape), FW(UrlEscape), r1.a, 0, 256, &r2);
    CHECKF(!strcmp(r1.a, r2.a), "re-escape \"%s\" -> \"%s\"", r1.a, r2.a);
}
TEST(gap15_urlescape_unicode_w) {
    REQUIRE(U(UrlEscapeW));
    WCHAR out[128]; DWORD n = 128;
    HRESULT hr = U(UrlEscapeW)(L"http://a/\x00e9", out, &n, 0);
    char o[128]; g_narrow(out, o, sizeof o);
    t_note("UrlEscapeW(e-acute) hr=0x%lx \"%s\" n=%lu", (unsigned long)hr, SUCCEEDED(hr) ? o : "-", (unsigned long)n);
    CHECK(SUCCEEDED(hr));
    n = 128;
    hr = U(UrlEscapeW)(L"http://a/\x00e9", out, &n, URL_ESCAPE_AS_UTF8);
    g_narrow(out, o, sizeof o);
    t_note("UrlEscapeW(e-acute, AS_UTF8) hr=0x%lx \"%s\"", (unsigned long)hr, SUCCEEDED(hr) ? o : "-");
    if (SUCCEEDED(hr)) CHECKF(!strcmp(o, "http://a/%C3%A9"), "got %s", o);
    n = 128;
    hr = U(UrlEscapeW)(L"http://a/\x20ac", out, &n, URL_ESCAPE_AS_UTF8);
    g_narrow(out, o, sizeof o);
    if (SUCCEEDED(hr)) CHECKF(!strcmp(o, "http://a/%E2%82%AC"), "got %s", o);
    n = 128;
    hr = U(UrlEscapeW)(L"http://a/\x00e9", out, &n, URL_ESCAPE_URI_COMPONENT);
    g_narrow(out, o, sizeof o);
    t_note("UrlEscapeW(e-acute, URI_COMPONENT) hr=0x%lx \"%s\"", (unsigned long)hr, SUCCEEDED(hr) ? o : "-");
}
TEST(gap15_urlescape_long_input) {
    NEED2(UrlEscape);
    char in[1100]; memset(in, 'a', 1000); in[1000] = 0;
    char *p = in + 500; *p = ' ';
    g_res r;
    g_run4(FA(UrlEscape), FW(UrlEscape), in, 0, 1500, &r);
    CHECK(r.hrA == S_OK && r.hrW == S_OK);
    CHECKF(r.nA == 1002 && r.nW == 1002, "long escape count %lu/%lu", (unsigned long)r.nA, (unsigned long)r.nW);
    CHECK(r.a[500] == '%' && r.a[501] == '2' && r.a[502] == '0');
}

/* ----------------------------------------------------------------- UrlUnescape */
TEST(gap15_urlunescape_args) {
    NEED2(UrlUnescape);
    g_args4("UrlUnescape", FA(UrlUnescape), FW(UrlUnescape), "http://a/%41", 0);
}
TEST(gap15_urlunescape_basic) {
    NEED2(UrlUnescape);
    static const g_case4 c[] = {
        { "%41%42%43", 0, S_OK, "ABC", 1 },
        { "%4a%4A", 0, S_OK, "JJ", 1 },
        { "a%20b", 0, S_OK, "a b", 1 },
        { "http://a/%7Euser", 0, S_OK, "http://a/~user", 1 },
        { "%25", 0, S_OK, "%", 1 },
        { "%2541", 0, S_OK, "%41", 1 },
        { "%", 0, S_OK, "%", 1 },
        { "%4", 0, S_OK, "%4", 1 },
        { "%zz", 0, S_OK, "%zz", 1 },
        { "%4z", 0, S_OK, "%4z", 1 },
        { "%z4", 0, S_OK, "%z4", 1 },
        { "100%", 0, S_OK, "100%", 1 },
        { "no escapes here", 0, S_OK, "no escapes here", 1 },
        { "%7f", 0, S_OK, "\177", 1 },
        { "a%2", 0, S_OK, "a%2", 1 },
        { "%%41", 0, S_OK, "%A", 1 },
        { "+", 0, S_OK, "+", 1 },
        { "%2B", 0, S_OK, "+", 1 },
    };
    g_check4("UrlUnescape", FA(UrlUnescape), FW(UrlUnescape), c, sizeof c / sizeof *c);
}
TEST(gap15_urlunescape_nul_octet) {
    REQUIRE(U(UrlUnescapeW)); REQUIRE(U(UrlUnescapeA));
    /* an escaped NUL is refused by the wide implementation */
    static const char *in[] = { "%00", "ab%00cd", "%41%00" };
    for (size_t i = 0; i < 3; i++) {
        WCHAR w[32], o[32]; DWORD n = 32; g_widen(in[i], w, 32);
        HRESULT hr = U(UrlUnescapeW)(w, o, &n, 0);
        CHECKF(hr == G_E_INVALIDARG, "W \"%s\" hr=0x%lx", in[i], (unsigned long)hr);
        char a[32], ao[32]; strcpy(a, in[i]); n = 32;
        hr = U(UrlUnescapeA)(a, ao, &n, 0);
        t_note("UrlUnescapeA(\"%s\") hr=0x%lx", in[i], (unsigned long)hr);
    }
}
TEST(gap15_urlunescape_high_bytes) {
    NEED2(UrlUnescape);
    /* %E9 -> single byte 0xE9 in the A output; W gets the char U+00E9 */
    char ain[] = "a%E9b", aout[32]; DWORD n = 32;
    HRESULT hr = U(UrlUnescapeA)(ain, aout, &n, 0);
    CHECK(hr == S_OK);
    CHECK((unsigned char)aout[1] == 0xe9 && aout[0] == 'a' && aout[2] == 'b' && aout[3] == 0);
    WCHAR win[] = L"a%E9b", wout[32]; n = 32;
    hr = U(UrlUnescapeW)(win, wout, &n, 0);
    CHECK(hr == S_OK && wout[1] == 0x00e9 && n == 3);
    /* %FF */
    n = 32; WCHAR w2[] = L"%FF"; hr = U(UrlUnescapeW)(w2, wout, &n, 0);
    CHECK(hr == S_OK && wout[0] == 0x00ff);
}
TEST(gap15_urlunescape_extra_info) {
    NEED2(UrlUnescape);
    static const g_case4 c[] = {
        { "a%41?b%41", URL_DONT_UNESCAPE_EXTRA_INFO, S_OK, "aA?b%41", 1 },
        { "a%41#b%41", URL_DONT_UNESCAPE_EXTRA_INFO, S_OK, "aA#b%41", 1 },
        { "a%41b%41", URL_DONT_UNESCAPE_EXTRA_INFO, S_OK, "aAbA", 1 },
        { "?%41", URL_DONT_UNESCAPE_EXTRA_INFO, S_OK, "?%41", 1 },
        { "%41?", URL_DONT_UNESCAPE_EXTRA_INFO, S_OK, "A?", 1 },
        { "a%41?b%41", 0, S_OK, "aA?bA", 1 },
        { "x%20y#z%20", URL_DONT_UNESCAPE_EXTRA_INFO, S_OK, "x y#z%20", 1 },
    };
    g_check4("UrlUnescape", FA(UrlUnescape), FW(UrlUnescape), c, sizeof c / sizeof *c);
}
TEST(gap15_urlunescape_truncation) {
    NEED2(UrlUnescape);
    g_trunc4("UrlUnescape", FA(UrlUnescape), FW(UrlUnescape), "a%20b%20c", 0, "a b c");
    g_trunc4("UrlUnescape", FA(UrlUnescape), FW(UrlUnescape), "abcdef", 0, "abcdef");
}
TEST(gap15_urlunescape_inplace) {
    NEED2(UrlUnescape);
    char a[] = "x%41%42y";
    HRESULT hr = U(UrlUnescapeA)(a, NULL, NULL, URL_UNESCAPE_INPLACE);
    CHECKF(hr == S_OK, "A hr=0x%lx", (unsigned long)hr);
    CHECKF(!strcmp(a, "xABy"), "A in place \"%s\"", a);
    WCHAR w[] = L"x%41%42y";
    hr = U(UrlUnescapeW)(w, NULL, NULL, URL_UNESCAPE_INPLACE);
    CHECKF(hr == S_OK, "W hr=0x%lx", (unsigned long)hr);
    CHECK(g_waeq(w, "xABy"));
    /* extra args ignored in place */
    char b[] = "%20";
    char out[8]; DWORD n = 8;
    hr = U(UrlUnescapeA)(b, out, &n, URL_UNESCAPE_INPLACE);
    CHECK(hr == S_OK && !strcmp(b, " "));
    /* no escapes: unchanged */
    char c[] = "plain";
    hr = U(UrlUnescapeA)(c, NULL, NULL, URL_UNESCAPE_INPLACE);
    CHECK(hr == S_OK && !strcmp(c, "plain"));
    /* %00 fails in place */
    WCHAR d[] = L"a%00b";
    hr = U(UrlUnescapeW)(d, NULL, NULL, URL_UNESCAPE_INPLACE);
    CHECKF(hr == G_E_INVALIDARG, "%%00 in place hr=0x%lx", (unsigned long)hr);
    /* extra-info flag in place */
    char e[] = "%41?%41";
    hr = U(UrlUnescapeA)(e, NULL, NULL, URL_UNESCAPE_INPLACE | URL_DONT_UNESCAPE_EXTRA_INFO);
    CHECK(hr == S_OK && !strcmp(e, "A?%41"));
    /* empty string */
    char f[] = "";
    hr = U(UrlUnescapeA)(f, NULL, NULL, URL_UNESCAPE_INPLACE);
    CHECK(hr == S_OK && f[0] == 0);
}
TEST(gap15_urlunescape_utf8) {
    REQUIRE(U(UrlUnescapeW)); REQUIRE(U(UrlUnescapeA));
    WCHAR in[] = L"%C3%A9", out[16]; DWORD n = 16;
    HRESULT hr = U(UrlUnescapeW)(in, out, &n, URL_UNESCAPE_AS_UTF8);
    CHECKF(hr == S_OK, "hr=0x%lx", (unsigned long)hr);
    CHECK(out[0] == 0x00e9 && out[1] == 0 && n == 1);
    WCHAR in2[] = L"%E2%82%AC!", out2[16]; n = 16;
    hr = U(UrlUnescapeW)(in2, out2, &n, URL_UNESCAPE_AS_UTF8);
    CHECK(hr == S_OK && out2[0] == 0x20ac && out2[1] == L'!');
    /* without the flag every octet is its own char */
    WCHAR in3[] = L"%C3%A9", out3[16]; n = 16;
    hr = U(UrlUnescapeW)(in3, out3, &n, 0);
    CHECK(hr == S_OK && out3[0] == 0x00c3 && out3[1] == 0x00a9 && n == 2);
    /* the ANSI entry rejects the UTF-8 flag */
    char ain[] = "%C3%A9", aout[16]; n = 16;
    hr = U(UrlUnescapeA)(ain, aout, &n, URL_UNESCAPE_AS_UTF8);
    CHECKF(hr == G_E_INVALIDARG, "A utf8 hr=0x%lx", (unsigned long)hr);
    /* in place utf8 */
    WCHAR in4[] = L"a%C3%A9b";
    hr = U(UrlUnescapeW)(in4, NULL, NULL, URL_UNESCAPE_INPLACE | URL_UNESCAPE_AS_UTF8);
    CHECK(hr == S_OK && in4[0] == L'a' && in4[1] == 0x00e9 && in4[2] == L'b');
}
TEST(gap15_urlunescape_roundtrip) {
    NEED2(UrlUnescape); NEED2(UrlEscape);
    const char *src = "http://a/x y<z>\"w";
    g_res e, u;
    g_run4(FA(UrlEscape), FW(UrlEscape), src, 0, 256, &e);
    g_run4(FA(UrlUnescape), FW(UrlUnescape), e.a, 0, 256, &u);
    CHECKF(!strcmp(u.a, src), "roundtrip \"%s\" -> \"%s\" -> \"%s\"", src, e.a, u.a);
}

/* ------------------------------------------------------------ UrlCanonicalize */
TEST(gap15_urlcanon_args) {
    NEED2(UrlCanonicalize);
    g_args4("UrlCanonicalize", FA(UrlCanonicalize), FW(UrlCanonicalize), "http://a/b", 0);
    g_args4("UrlCanonicalize", FA(UrlCanonicalize), FW(UrlCanonicalize), "http://a/b", URL_UNESCAPE);
}
TEST(gap15_urlcanon_dots) {
    NEED2(UrlCanonicalize);
    static const g_case4 c[] = {
        { "http://www.a.com/b/../c", 0, S_OK, "http://www.a.com/c", 1 },
        { "http://www.a.com/b/./c", 0, S_OK, "http://www.a.com/b/c", 1 },
        { "http://www.a.com/b/c/../../d", 0, S_OK, "http://www.a.com/d", 1 },
        { "http://www.a.com/b/c/..", 0, S_OK, NULL, 0 },
        { "http://www.a.com/b/c/.", 0, S_OK, NULL, 0 },
        { "http://www.a.com/../x", 0, S_OK, NULL, 0 },
        { "http://www.a.com/b//c", 0, S_OK, NULL, 0 },
        { "http://www.a.com/b/../c", URL_DONT_SIMPLIFY, S_OK, NULL, 0 },
        { "http://www.a.com", 0, S_OK, NULL, 0 },
        { "http://www.a.com/", 0, S_OK, "http://www.a.com/", 1 },
        { "HTTP://WWW.A.COM/", 0, S_OK, NULL, 0 },
        { "http:\\\\www.a.com\\b\\c", 0, S_OK, NULL, 0 },
        { "http://www.a.com/b?x=../y", 0, S_OK, NULL, 0 },
        { "http://www.a.com/b#../y", 0, S_OK, NULL, 0 },
        { "file:///c:/a/../b", 0, S_OK, NULL, 0 },
        { "file:///c:\\a\\..\\b", 0, S_OK, NULL, 0 },
        { "c:\\a\\..\\b", 0, S_OK, NULL, 0 },
        { "a/../b", 0, S_OK, NULL, 0 },
        { "", 0, S_OK, NULL, 0 },
        { "mailto:a@b.com", 0, S_OK, NULL, 0 },
        { "javascript:alert(1)", 0, S_OK, NULL, 0 },
        { "about:blank", 0, S_OK, NULL, 0 },
        { "foo:bar/../baz", 0, S_OK, NULL, 0 },
    };
    g_check4("UrlCanonicalize", FA(UrlCanonicalize), FW(UrlCanonicalize), c, sizeof c / sizeof *c);
}
TEST(gap15_urlcanon_flags) {
    NEED2(UrlCanonicalize);
    static const g_case4 c[] = {
        { "http://a/%41", URL_UNESCAPE, S_OK, "http://a/A", 1 },
        { "http://a/%41%42", URL_UNESCAPE, S_OK, "http://a/AB", 1 },
        { "http://a/b c", URL_ESCAPE_SPACES_ONLY, S_OK, "http://a/b%20c", 1 },
        { "http://a/b c", URL_ESCAPE_UNSAFE, S_OK, "http://a/b%20c", 1 },
        { "http://a/b c", 0, S_OK, NULL, 0 },
        { "http://a/b<c", URL_ESCAPE_UNSAFE, S_OK, "http://a/b%3Cc", 1 },
        { "http://a/b c", URL_UNESCAPE | URL_ESCAPE_UNSAFE, S_OK, NULL, 0 },
        { "http://a/%20", URL_UNESCAPE, S_OK, "http://a/ ", 1 },
        { "http://a/b/../c", URL_DONT_SIMPLIFY, S_OK, NULL, 0 },
        { "http://a/b/../c", URL_WININET_COMPATIBILITY, S_OK, NULL, 0 },
        { "http://a/b/../c", URL_PLUGGABLE_PROTOCOL, S_OK, NULL, 0 },
        { "http://a/b/../c", URL_FILE_USE_PATHURL, S_OK, NULL, 0 },
        { "file:///c:/a b", URL_FILE_USE_PATHURL, S_OK, NULL, 0 },
        { "http://a/b c?x y#z w", URL_ESCAPE_SPACES_ONLY, S_OK, NULL, 0 },
        { "http://a/b%7Ec", URL_UNESCAPE, S_OK, "http://a/b~c", 1 },
    };
    g_check4("UrlCanonicalize", FA(UrlCanonicalize), FW(UrlCanonicalize), c, sizeof c / sizeof *c);
}
TEST(gap15_urlcanon_truncation) {
    NEED2(UrlCanonicalize);
    g_trunc4("UrlCanonicalize", FA(UrlCanonicalize), FW(UrlCanonicalize), "http://www.a.com/b/../c", 0, "http://www.a.com/c");
    g_trunc4("UrlCanonicalize", FA(UrlCanonicalize), FW(UrlCanonicalize), "http://a/%41", URL_UNESCAPE, "http://a/A");
}
TEST(gap15_urlcanon_repeat_cache) {
    NEED2(UrlCanonicalize);
    /* repeated calls with the same and with alternating flags must stay consistent
     * (the implementation keeps a one-entry cache) */
    g_res first;
    g_run4(FA(UrlCanonicalize), FW(UrlCanonicalize), "http://www.a.com/b/../c", 0, 256, &first);
    CHECK(!strcmp(first.a, "http://www.a.com/c"));
    for (int i = 0; i < 20; i++) {
        g_res r;
        g_run4(FA(UrlCanonicalize), FW(UrlCanonicalize), "http://www.a.com/c", 0, 256, &r);
        CHECK(!strcmp(r.a, "http://www.a.com/c"));
        g_run4(FA(UrlCanonicalize), FW(UrlCanonicalize), "http://www.a.com/c", URL_UNESCAPE, 256, &r);
        CHECK(!strcmp(r.a, "http://www.a.com/c"));
        g_run4(FA(UrlCanonicalize), FW(UrlCanonicalize), "http://www.a.com/%63", URL_UNESCAPE, 256, &r);
        CHECK(!strcmp(r.a, "http://www.a.com/c"));
        g_run4(FA(UrlCanonicalize), FW(UrlCanonicalize), "http://www.a.com/%63", 0, 256, &r);
        CHECK(r.hrA == S_OK);
    }
    /* cache hit with too-small buffer must still fail with E_POINTER and report size */
    g_res r;
    g_run4(FA(UrlCanonicalize), FW(UrlCanonicalize), "http://www.a.com/c", 0, 256, &r);
    g_run4(FA(UrlCanonicalize), FW(UrlCanonicalize), "http://www.a.com/c", 0, 5, &r);
    CHECKF(r.hrW == G_E_POINTER && r.nW == 19, "small buf hr=0x%lx n=%lu", (unsigned long)r.hrW, (unsigned long)r.nW);
}
TEST(gap15_urlcanon_inplace_alias) {
    REQUIRE(U(UrlCanonicalizeW));
    WCHAR buf[128]; g_widen("http://www.a.com/b/../c", buf, 128);
    DWORD n = 128;
    HRESULT hr = U(UrlCanonicalizeW)(buf, buf, &n, 0);
    char o[128]; g_narrow(buf, o, sizeof o);
    t_note("UrlCanonicalizeW aliased in/out hr=0x%lx \"%s\"", (unsigned long)hr, o);
    CHECK(SUCCEEDED(hr));
    if (SUCCEEDED(hr)) CHECKF(!strcmp(o, "http://www.a.com/c"), "got %s", o);
}
TEST(gap15_urlcanon_long) {
    NEED2(UrlCanonicalize);
    char in[1100] = "http://a/";
    size_t l = strlen(in);
    memset(in + l, 'x', 900); in[l + 900] = 0;
    g_res r;
    g_run4(FA(UrlCanonicalize), FW(UrlCanonicalize), in, 0, 1500, &r);
    CHECKF(r.hrA == S_OK && r.hrW == S_OK, "long hr A=0x%lx W=0x%lx", (unsigned long)r.hrA, (unsigned long)r.hrW);
    CHECK(!strcmp(r.a, in));
}

/* ------------------------------------------------------------------ UrlFixupW */
TEST(gap15_urlfixup_observe) {
    REQUIRE(U(UrlFixupW));
    static const char *in[] = { "http://www.example.com/", "www.example.com", "example.com", "ftp://x/y", "", "c:\\dir\\file.txt", "gopher.example.com", "foo", "mailto:a@b" };
    for (size_t i = 0; i < sizeof in / sizeof *in; i++) {
        WCHAR w[512], out[512]; g_widen(in[i], w, 512);
        for (int k = 0; k < 512; k++) out[k] = 0x7e7e;
        HRESULT hr = U(UrlFixupW)(w, out, 512);
        char o[512]; g_narrow(out, o, sizeof o);
        t_note("UrlFixupW(\"%s\") hr=0x%lx \"%s\"", in[i], (unsigned long)hr, o);
        CHECKF(hr == S_OK || hr == S_FALSE || FAILED(hr), "sane");
    }
}
TEST(gap15_urlfixup_known_scheme) {
    REQUIRE(U(UrlFixupW));
    WCHAR out[512];
    for (int k = 0; k < 512; k++) out[k] = 0x7e7e;
    HRESULT hr = U(UrlFixupW)(L"http://www.example.com/a", out, 512);
    char o[512]; g_narrow(out, o, sizeof o);
    /* an already-valid http URL is left alone */
    CHECKF(SUCCEEDED(hr), "hr=0x%lx", (unsigned long)hr);
    if (SUCCEEDED(hr)) CHECKF(!strcmp(o, "http://www.example.com/a"), "got \"%s\"", o);
}
