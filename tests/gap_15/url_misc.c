#include "gap15.h"

typedef HRESULT (WINAPI *g_combA)(LPCSTR, LPCSTR, LPSTR, LPDWORD, DWORD);
typedef HRESULT (WINAPI *g_combW)(LPCWSTR, LPCWSTR, LPWSTR, LPDWORD, DWORD);
typedef HRESULT (WINAPI *g_partA)(LPCSTR, LPSTR, LPDWORD, DWORD, DWORD);
typedef HRESULT (WINAPI *g_partW)(LPCWSTR, LPWSTR, LPDWORD, DWORD, DWORD);

/* ------------------------------------------------------------------ UrlCombine */
static void comb(const char *base, const char *rel, DWORD flags, DWORD cap, HRESULT *hrA, HRESULT *hrW, char *oa, size_t ocap, DWORD *nA, DWORD *nW)
{
    WCHAR wb[1024], wr[1024], wo[2048]; char ab[1024], ar[1024];
    strcpy(ab, base); strcpy(ar, rel);
    g_widen(base, wb, 1024); g_widen(rel, wr, 1024);
    char ao[2048]; DWORD n1 = cap, n2 = cap;
    memset(ao, 0, sizeof ao); memset(wo, 0, sizeof wo);
    *hrA = ((g_combA)U(UrlCombineA))(ab, ar, ao, &n1, flags);
    *hrW = ((g_combW)U(UrlCombineW))(wb, wr, wo, &n2, flags);
    char wn[2048]; g_narrow(wo, wn, sizeof wn);
    if (SUCCEEDED(*hrA) && SUCCEEDED(*hrW))
        CHECKF(!strcmp(ao, wn), "UrlCombine A/W differ (\"%s\",\"%s\"): \"%s\" vs \"%s\"", base, rel, ao, wn);
    strncpy(oa, ao, ocap - 1); oa[ocap - 1] = 0;
    *nA = n1; *nW = n2;
}
typedef struct { const char *base, *rel; DWORD flags; const char *want; int hard; } comb_case;
static void run_comb(const comb_case *c, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        HRESULT ha, hw; char o[2048]; DWORD na, nw;
        comb(c[i].base, c[i].rel, c[i].flags, 1024, &ha, &hw, o, sizeof o, &na, &nw);
        CHECKF(ha == hw, "UrlCombine(\"%s\",\"%s\") A/W hr 0x%lx/0x%lx", c[i].base, c[i].rel, (unsigned long)ha, (unsigned long)hw);
        if (c[i].hard) {
            CHECKF(ha == S_OK, "UrlCombine(\"%s\",\"%s\") hr=0x%lx", c[i].base, c[i].rel, (unsigned long)ha);
            CHECKF(!strcmp(o, c[i].want), "UrlCombine(\"%s\",\"%s\",0x%lx) -> \"%s\" want \"%s\"", c[i].base, c[i].rel, (unsigned long)c[i].flags, o, c[i].want);
            CHECKF(na == strlen(c[i].want) && nw == na, "UrlCombine count %lu/%lu", (unsigned long)na, (unsigned long)nw);
        } else
            t_note("UrlCombine(\"%s\",\"%s\",0x%lx): hr=0x%lx \"%s\"", c[i].base, c[i].rel, (unsigned long)c[i].flags, (unsigned long)ha, SUCCEEDED(ha) ? o : "-");
    }
}
TEST(gap15_urlcombine_args) {
    REQUIRE(U(UrlCombineA)); REQUIRE(U(UrlCombineW));
    char o[64]; WCHAR w[64]; DWORD n = 64;
    CHECK(U(UrlCombineA)(NULL, "x", o, &n, 0) == G_E_INVALIDARG);
    CHECK(U(UrlCombineA)("http://a/", NULL, o, &n, 0) == G_E_INVALIDARG);
    CHECK(U(UrlCombineA)("http://a/", "x", o, NULL, 0) == G_E_INVALIDARG);
    CHECK(U(UrlCombineW)(NULL, L"x", w, &n, 0) == G_E_INVALIDARG);
    CHECK(U(UrlCombineW)(L"http://a/", NULL, w, &n, 0) == G_E_INVALIDARG);
    CHECK(U(UrlCombineW)(L"http://a/", L"x", w, NULL, 0) == G_E_INVALIDARG);
    n = 0;
    HRESULT hr = U(UrlCombineW)(L"http://a/", L"x", w, &n, 0);
    t_note("UrlCombineW zero pcch hr=0x%lx n=%lu", (unsigned long)hr, (unsigned long)n);
    n = 0;
    hr = U(UrlCombineA)("http://a/", "x", o, &n, 0);
    t_note("UrlCombineA zero pcch hr=0x%lx n=%lu", (unsigned long)hr, (unsigned long)n);
}
TEST(gap15_urlcombine_basic) {
    REQUIRE(U(UrlCombineA)); REQUIRE(U(UrlCombineW));
    static const comb_case c[] = {
        { "http://www.a.com/dir/file.htm", "other.htm", 0, "http://www.a.com/dir/other.htm", 1 },
        { "http://www.a.com/dir/", "other.htm", 0, "http://www.a.com/dir/other.htm", 1 },
        { "http://www.a.com/dir/file.htm", "../other.htm", 0, "http://www.a.com/other.htm", 1 },
        { "http://www.a.com/a/b/c/d.htm", "../../x.htm", 0, "http://www.a.com/a/x.htm", 1 },
        { "http://www.a.com/dir/file.htm", "sub/x.htm", 0, "http://www.a.com/dir/sub/x.htm", 1 },
        { "http://www.a.com/dir/file.htm", "./x.htm", 0, "http://www.a.com/dir/x.htm", 1 },
        { "http://www.a.com/dir/file.htm", "/root.htm", 0, "http://www.a.com/root.htm", 1 },
        { "http://www.a.com/dir/file.htm", "http://b.com/y.htm", 0, "http://b.com/y.htm", 1 },
        { "http://www.a.com/dir/file.htm", "ftp://b.com/y", 0, "ftp://b.com/y", 1 },
        { "http://www.a.com/dir/file.htm", "//b.com/y.htm", 0, "http://b.com/y.htm", 1 },
        { "http://www.a.com/dir/file.htm", "#frag", 0, "http://www.a.com/dir/file.htm#frag", 1 },
        { "http://www.a.com/dir/file.htm", "x.htm#frag", 0, "http://www.a.com/dir/x.htm#frag", 1 },
        { "http://www.a.com/dir/file.htm", "x.htm?q=1", 0, "http://www.a.com/dir/x.htm?q=1", 1 },
        { "http://www.a.com", "x.htm", 0, "http://www.a.com/x.htm", 1 },
        { "", "http://b.com/y", 0, "http://b.com/y", 1 },
        { "http://www.a.com/dir/file.htm", "", 0, NULL, 0 },
        { "http://www.a.com/dir/file.htm", "?q=1", 0, NULL, 0 },
        { "http://www.a.com/dir/file.htm", "../../../x", 0, NULL, 0 },
        { "http://www.a.com/dir/file.htm", "x y", 0, NULL, 0 },
        { "http://www.a.com/dir/file.htm", "..", 0, NULL, 0 },
        { "http://www.a.com/dir/file.htm", ".", 0, NULL, 0 },
        { "http://www.a.com/dir/file.htm?x=1#f", "y", 0, NULL, 0 },
        { "http://www.a.com/dir/file.htm", "mailto:a@b.com", 0, NULL, 0 },
        { "http://www.a.com/dir/file.htm", "javascript:void(0)", 0, NULL, 0 },
        { "file:///c:/dir/file.txt", "x.txt", 0, "file:///c:/dir/x.txt", 1 },
        { "file:///c:/dir/file.txt", "../x.txt", 0, "file:///c:/x.txt", 1 },
        { "c:\\dir\\file.txt", "x.txt", 0, NULL, 0 },
        { "dir/file", "x", 0, NULL, 0 },
        { "http://www.a.com/dir/file.htm", "x.htm", URL_DONT_SIMPLIFY, "http://www.a.com/dir/x.htm", 1 },
        { "http://www.a.com/dir/file.htm", "../x.htm", URL_DONT_SIMPLIFY, NULL, 0 },
        { "http://www.a.com/dir/file.htm", "x y", URL_ESCAPE_SPACES_ONLY, "http://www.a.com/dir/x%20y", 1 },
        { "http://www.a.com/dir/file.htm", "x y", URL_ESCAPE_UNSAFE, NULL, 0 },
        { "http://www.a.com/dir/file.htm", "%41", URL_UNESCAPE, "http://www.a.com/dir/A", 1 },
    };
    run_comb(c, sizeof c / sizeof *c);
}
TEST(gap15_urlcombine_truncation) {
    REQUIRE(U(UrlCombineA)); REQUIRE(U(UrlCombineW));
    const char *want = "http://www.a.com/dir/other.htm";
    size_t len = strlen(want);
    HRESULT ha, hw; char o[256]; DWORD na, nw;
    comb("http://www.a.com/dir/file.htm", "other.htm", 0, (DWORD)len + 1, &ha, &hw, o, sizeof o, &na, &nw);
    CHECK(ha == S_OK && hw == S_OK && !strcmp(o, want) && na == len && nw == len);
    comb("http://www.a.com/dir/file.htm", "other.htm", 0, (DWORD)len, &ha, &hw, o, sizeof o, &na, &nw);
    CHECKF(ha == G_E_POINTER && hw == G_E_POINTER, "short hr 0x%lx/0x%lx", (unsigned long)ha, (unsigned long)hw);
    CHECKF(na == len + 1 && nw == len + 1, "needed %lu/%lu want %lu", (unsigned long)na, (unsigned long)nw, (unsigned long)len + 1);
    comb("http://www.a.com/dir/file.htm", "other.htm", 0, 1, &ha, &hw, o, sizeof o, &na, &nw);
    CHECKF(ha == G_E_POINTER && hw == G_E_POINTER, "cap1 hr 0x%lx/0x%lx", (unsigned long)ha, (unsigned long)hw);
    CHECKF(na == len + 1 && nw == len + 1, "cap1 needed %lu/%lu", (unsigned long)na, (unsigned long)nw);
    /* a retry with the reported size works */
    comb("http://www.a.com/dir/file.htm", "other.htm", 0, nw, &ha, &hw, o, sizeof o, &na, &nw);
    CHECK(ha == S_OK && !strcmp(o, want));
}
TEST(gap15_urlcombine_alias_base) {
    REQUIRE(U(UrlCombineW));
    WCHAR buf[256]; g_widen("http://www.a.com/dir/file.htm", buf, 256);
    DWORD n = 256;
    HRESULT hr = U(UrlCombineW)(buf, L"x.htm", buf, &n, 0);
    char o[256]; g_narrow(buf, o, sizeof o);
    t_note("UrlCombineW aliased base/out hr=0x%lx \"%s\"", (unsigned long)hr, o);
    CHECK(SUCCEEDED(hr));
}
TEST(gap15_urlcombine_repeat) {
    REQUIRE(U(UrlCombineA)); REQUIRE(U(UrlCombineW));
    for (int i = 0; i < 100; i++) {
        HRESULT ha, hw; char o[256]; DWORD na, nw;
        comb("http://www.a.com/dir/file.htm", "../x.htm", 0, 256, &ha, &hw, o, sizeof o, &na, &nw);
        CHECK(ha == S_OK && !strcmp(o, "http://www.a.com/x.htm"));
    }
}

/* ------------------------------------------------------------------ UrlGetPart */
static HRESULT part2(const char *in, DWORD part, DWORD flags, DWORD cap, char *oa, WCHAR *ow, DWORD *na, DWORD *nw, HRESULT *hw)
{
    char ain[1024]; WCHAR win[1024];
    strcpy(ain, in); g_widen(in, win, 1024);
    *na = cap; *nw = cap;
    memset(oa, 0, 1024); for (int i = 0; i < 1024; i++) ow[i] = 0;
    HRESULT ha = ((g_partA)U(UrlGetPartA))(ain, oa, na, part, flags);
    *hw = ((g_partW)U(UrlGetPartW))(win, ow, nw, part, flags);
    return ha;
}
typedef struct { const char *url; DWORD part, flags; const char *want; int hard; } part_case;
static void run_part(const part_case *c, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        char oa[1024]; WCHAR ow[1024]; DWORD na, nw; HRESULT hw;
        HRESULT ha = part2(c[i].url, c[i].part, c[i].flags, 1000, oa, ow, &na, &nw, &hw);
        CHECKF(SUCCEEDED(ha) == SUCCEEDED(hw), "UrlGetPart(\"%s\",%lu) A/W hr 0x%lx/0x%lx", c[i].url, (unsigned long)c[i].part, (unsigned long)ha, (unsigned long)hw);
        if (SUCCEEDED(ha) && SUCCEEDED(hw)) CHECKF(g_waeq(ow, oa), "UrlGetPart(\"%s\",%lu) A/W output differ \"%s\"", c[i].url, (unsigned long)c[i].part, oa);
        if (c[i].hard) {
            CHECKF(SUCCEEDED(ha), "UrlGetPart(\"%s\",%lu,0x%lx) hr=0x%lx", c[i].url, (unsigned long)c[i].part, (unsigned long)c[i].flags, (unsigned long)ha);
            CHECKF(!strcmp(oa, c[i].want), "UrlGetPart(\"%s\",%lu,0x%lx) -> \"%s\" want \"%s\"", c[i].url, (unsigned long)c[i].part, (unsigned long)c[i].flags, oa, c[i].want);
            if (SUCCEEDED(ha)) CHECKF(na == strlen(c[i].want) && nw == na, "UrlGetPart count %lu/%lu", (unsigned long)na, (unsigned long)nw);
        } else
            t_note("UrlGetPart(\"%s\",%lu,0x%lx): hr=0x%lx \"%s\"", c[i].url, (unsigned long)c[i].part, (unsigned long)c[i].flags, (unsigned long)ha, SUCCEEDED(ha) ? oa : "-");
    }
}
TEST(gap15_urlgetpart_args) {
    REQUIRE(U(UrlGetPartA)); REQUIRE(U(UrlGetPartW));
    char o[64]; WCHAR w[64]; DWORD n = 64;
    CHECK(U(UrlGetPartA)(NULL, o, &n, URL_PART_SCHEME, 0) == G_E_INVALIDARG);
    CHECK(U(UrlGetPartA)("http://a", NULL, &n, URL_PART_SCHEME, 0) == G_E_INVALIDARG);
    CHECK(U(UrlGetPartA)("http://a", o, NULL, URL_PART_SCHEME, 0) == G_E_INVALIDARG);
    CHECK(U(UrlGetPartA)("http://a", o, &n, URL_PART_NONE, 0) == G_E_INVALIDARG);
    CHECK(U(UrlGetPartW)(NULL, w, &n, URL_PART_SCHEME, 0) == G_E_INVALIDARG);
    CHECK(U(UrlGetPartW)(L"http://a", NULL, &n, URL_PART_SCHEME, 0) == G_E_INVALIDARG);
    CHECK(U(UrlGetPartW)(L"http://a", w, NULL, URL_PART_SCHEME, 0) == G_E_INVALIDARG);
    CHECK(U(UrlGetPartW)(L"http://a", w, &n, URL_PART_NONE, 0) == G_E_INVALIDARG);
    n = 0;
    CHECK(U(UrlGetPartA)("http://a", o, &n, URL_PART_SCHEME, 0) == G_E_INVALIDARG && n == 0);
    n = 0;
    CHECK(U(UrlGetPartW)(L"http://a", w, &n, URL_PART_SCHEME, 0) == G_E_INVALIDARG && n == 0);
}
TEST(gap15_urlgetpart_parts) {
    REQUIRE(U(UrlGetPartA)); REQUIRE(U(UrlGetPartW));
    static const part_case c[] = {
        { "http://user:pass@host.com:8080/path/x.htm?q=1&r=2#frag", URL_PART_SCHEME, 0, "http", 1 },
        { "http://user:pass@host.com:8080/path/x.htm?q=1&r=2#frag", URL_PART_HOSTNAME, 0, "host.com", 1 },
        { "http://user:pass@host.com:8080/path/x.htm?q=1&r=2#frag", URL_PART_USERNAME, 0, "user", 1 },
        { "http://user:pass@host.com:8080/path/x.htm?q=1&r=2#frag", URL_PART_PASSWORD, 0, "pass", 1 },
        { "http://user:pass@host.com:8080/path/x.htm?q=1&r=2#frag", URL_PART_PORT, 0, "8080", 1 },
        { "http://user:pass@host.com:8080/path/x.htm?q=1&r=2#frag", URL_PART_QUERY, 0, "?q=1&r=2#frag", 0 },
        { "http://host.com/p?q=1", URL_PART_QUERY, 0, "?q=1", 0 },
        { "http://host.com/p?q=1", URL_PART_HOSTNAME, 0, "host.com", 1 },
        { "http://host.com", URL_PART_HOSTNAME, 0, "host.com", 1 },
        { "http://host.com/", URL_PART_HOSTNAME, 0, "host.com", 1 },
        { "http://host.com:80", URL_PART_PORT, 0, "80", 1 },
        { "HTTP://HOST.COM/", URL_PART_SCHEME, 0, NULL, 0 },
        { "HTTP://HOST.COM/", URL_PART_HOSTNAME, 0, NULL, 0 },
        { "ftp://ftp.x.com/f", URL_PART_SCHEME, 0, "ftp", 1 },
        { "ftp://u@ftp.x.com/f", URL_PART_USERNAME, 0, "u", 1 },
        { "ftp://u@ftp.x.com/f", URL_PART_PASSWORD, 0, NULL, 0 },
        { "http://host.com/p", URL_PART_PORT, 0, NULL, 0 },
        { "http://host.com/p", URL_PART_USERNAME, 0, NULL, 0 },
        { "http://host.com/p", URL_PART_QUERY, 0, NULL, 0 },
        { "mailto:a@b.com", URL_PART_SCHEME, 0, "mailto", 1 },
        { "mailto:a@b.com", URL_PART_HOSTNAME, 0, NULL, 0 },
        { "javascript:alert(1)", URL_PART_SCHEME, 0, "javascript", 1 },
        { "file:///c:/dir/f.txt", URL_PART_SCHEME, 0, "file", 1 },
        { "file:///c:/dir/f.txt", URL_PART_HOSTNAME, 0, NULL, 0 },
        { "file://server/share/f.txt", URL_PART_HOSTNAME, 0, "server", 1 },
        { "www.host.com", URL_PART_SCHEME, 0, NULL, 0 },
        { "www.host.com", URL_PART_HOSTNAME, 0, NULL, 0 },
        { "", URL_PART_SCHEME, 0, NULL, 0 },
        { "http:", URL_PART_SCHEME, 0, "http", 1 },
        { "http:", URL_PART_HOSTNAME, 0, NULL, 0 },
        { "http://", URL_PART_HOSTNAME, 0, NULL, 0 },
        { "http://[::1]:80/", URL_PART_HOSTNAME, 0, NULL, 0 },
        { "http://host.com/p", 7, 0, NULL, 0 },
        { "http://host.com/p", 0x100, 0, NULL, 0 },
    };
    run_part(c, sizeof c / sizeof *c);
}
TEST(gap15_urlgetpart_keepscheme) {
    REQUIRE(U(UrlGetPartA)); REQUIRE(U(UrlGetPartW));
    static const part_case c[] = {
        { "http://user:pass@host.com:8080/p", URL_PART_SCHEME, URL_PARTFLAG_KEEPSCHEME, "http:", 0 },
        { "http://user:pass@host.com:8080/p", URL_PART_HOSTNAME, URL_PARTFLAG_KEEPSCHEME, "http://host.com", 0 },
        { "http://host.com/p", URL_PART_HOSTNAME, URL_PARTFLAG_KEEPSCHEME, "http://host.com", 0 },
        { "ftp://ftp.x.com/f", URL_PART_HOSTNAME, URL_PARTFLAG_KEEPSCHEME, "ftp://ftp.x.com", 0 },
        { "http://user:pass@host.com:8080/p", URL_PART_USERNAME, URL_PARTFLAG_KEEPSCHEME, "http:user", 0 },
        { "http://user:pass@host.com:8080/p", URL_PART_PORT, URL_PARTFLAG_KEEPSCHEME, "http:8080", 0 },
        { "http://user:pass@host.com:8080/p", URL_PART_PASSWORD, URL_PARTFLAG_KEEPSCHEME, "http:pass", 0 },
        { "mailto:a@b.com", URL_PART_SCHEME, URL_PARTFLAG_KEEPSCHEME, "mailto:", 0 },
    };
    run_part(c, sizeof c / sizeof *c);
}
TEST(gap15_urlgetpart_truncation) {
    REQUIRE(U(UrlGetPartA)); REQUIRE(U(UrlGetPartW));
    char oa[1024]; WCHAR ow[1024]; DWORD na, nw; HRESULT hw, ha;
    ha = part2("http://host.com/p", URL_PART_HOSTNAME, 0, 9, oa, ow, &na, &nw, &hw);
    CHECK(ha == S_OK && hw == S_OK && !strcmp(oa, "host.com") && na == 8 && nw == 8);
    ha = part2("http://host.com/p", URL_PART_HOSTNAME, 0, 8, oa, ow, &na, &nw, &hw);
    CHECKF(ha == G_E_POINTER && hw == G_E_POINTER, "short hr 0x%lx/0x%lx", (unsigned long)ha, (unsigned long)hw);
    CHECKF(na == 9 && nw == 9, "needed %lu/%lu", (unsigned long)na, (unsigned long)nw);
    ha = part2("http://host.com/p", URL_PART_HOSTNAME, 0, 1, oa, ow, &na, &nw, &hw);
    CHECK(ha == G_E_POINTER && hw == G_E_POINTER && na == 9 && nw == 9);
    ha = part2("http://host.com/p", URL_PART_SCHEME, 0, 5, oa, ow, &na, &nw, &hw);
    CHECK(ha == S_OK && !strcmp(oa, "http") && na == 4);
    ha = part2("http://host.com/p", URL_PART_SCHEME, 0, 4, oa, ow, &na, &nw, &hw);
    CHECK(ha == G_E_POINTER && na == 5);
}
TEST(gap15_urlgetpart_missing_part_empty) {
    REQUIRE(U(UrlGetPartW));
    WCHAR ow[64]; DWORD n = 64;
    for (int i = 0; i < 64; i++) ow[i] = 0x7e7e;
    HRESULT hr = U(UrlGetPartW)(L"http://host.com/p", ow, &n, URL_PART_PORT, 0);
    t_note("UrlGetPartW missing port hr=0x%lx n=%lu out0=0x%x", (unsigned long)hr, (unsigned long)n, ow[0]);
    if (SUCCEEDED(hr)) CHECKF(ow[0] == 0 && n == 0, "missing part should give empty string");
}

/* ----------------------------------------------------------------------- UrlIs */
typedef struct { const char *url; int is; int want; /* -1 = observe, 0/1 expected bool */ } is_case;
static void run_is(const is_case *c, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        WCHAR w[512]; g_widen(c[i].url, w, 512);
        BOOL a = U(UrlIsA)(c[i].url, (URLIS)c[i].is);
        BOOL b = U(UrlIsW)(w, (URLIS)c[i].is);
        CHECKF(!!a == !!b, "UrlIs(\"%s\",%d) A=%d W=%d", c[i].url, c[i].is, a, b);
        if (c[i].want >= 0)
            CHECKF(!!b == c[i].want && !!a == c[i].want, "UrlIs(\"%s\",%d) A=%d W=%d want %d", c[i].url, c[i].is, a, b, c[i].want);
        else
            t_note("UrlIs(\"%s\",%d) A=%d W=%d", c[i].url, c[i].is, a, b);
    }
}
TEST(gap15_urlis_null) {
    REQUIRE(U(UrlIsA)); REQUIRE(U(UrlIsW));
    for (int k = 0; k <= 6; k++) {
        CHECKF(U(UrlIsA)(NULL, (URLIS)k) == FALSE, "A NULL kind %d", k);
        CHECKF(U(UrlIsW)(NULL, (URLIS)k) == FALSE, "W NULL kind %d", k);
    }
    REQUIRE(U(UrlIsOpaqueA)); REQUIRE(U(UrlIsOpaqueW)); REQUIRE(U(UrlIsNoHistoryA)); REQUIRE(U(UrlIsNoHistoryW));
    CHECK(U(UrlIsOpaqueA)(NULL) == FALSE);
    CHECK(U(UrlIsOpaqueW)(NULL) == FALSE);
    CHECK(U(UrlIsNoHistoryA)(NULL) == FALSE);
    CHECK(U(UrlIsNoHistoryW)(NULL) == FALSE);
}
TEST(gap15_urlis_url) {
    REQUIRE(U(UrlIsA)); REQUIRE(U(UrlIsW));
    static const is_case c[] = {
        { "http://www.a.com/", URLIS_URL, 1 }, { "https://a/", URLIS_URL, 1 }, { "ftp://a/", URLIS_URL, 1 },
        { "file:///c:/a", URLIS_URL, 1 }, { "mailto:a@b.com", URLIS_URL, 1 }, { "javascript:x", URLIS_URL, 1 },
        { "HTTP://A/", URLIS_URL, 1 }, { "about:blank", URLIS_URL, 1 }, { "news:comp.lang", URLIS_URL, 1 },
        { "", URLIS_URL, 0 }, { "plain", URLIS_URL, 0 }, { "www.a.com", URLIS_URL, 0 },
        { "c:\\dir\\file", URLIS_URL, 0 }, { "c:/dir/file", URLIS_URL, 0 }, { "\\\\server\\share", URLIS_URL, 0 },
        { ":", URLIS_URL, 0 }, { "://a", URLIS_URL, 0 }, { "a:b", URLIS_URL, 0 },
        { "http", URLIS_URL, 0 }, { "ht tp://a", URLIS_URL, 0 }, { "/path/x", URLIS_URL, 0 },
        { "foo:bar", URLIS_URL, -1 }, { "x-custom://a", URLIS_URL, -1 }, { "a.b+c-d:x", URLIS_URL, -1 },
        { "http:", URLIS_URL, 1 },
    };
    run_is(c, sizeof c / sizeof *c);
}
TEST(gap15_urlis_opaque_nohistory) {
    REQUIRE(U(UrlIsA)); REQUIRE(U(UrlIsW));
    static const is_case c[] = {
        { "mailto:a@b.com", URLIS_OPAQUE, 1 }, { "MAILTO:a@b.com", URLIS_OPAQUE, 1 },
        { "javascript:alert(1)", URLIS_OPAQUE, -1 }, { "vbscript:x", URLIS_OPAQUE, -1 },
        { "news:comp.lang.c", URLIS_OPAQUE, -1 }, { "about:blank", URLIS_OPAQUE, -1 },
        { "http://a/", URLIS_OPAQUE, 0 }, { "ftp://a/", URLIS_OPAQUE, 0 }, { "file:///c:/a", URLIS_OPAQUE, 0 },
        { "plain", URLIS_OPAQUE, 0 }, { "", URLIS_OPAQUE, 0 }, { "c:\\a", URLIS_OPAQUE, 0 },
        { "http://a/", URLIS_NOHISTORY, 0 }, { "ftp://a/", URLIS_NOHISTORY, 0 }, { "file:///c:/a", URLIS_NOHISTORY, 0 },
        { "plain", URLIS_NOHISTORY, 0 }, { "", URLIS_NOHISTORY, 0 },
        { "about:blank", URLIS_NOHISTORY, -1 }, { "javascript:x", URLIS_NOHISTORY, -1 },
        { "mailto:a@b", URLIS_NOHISTORY, -1 }, { "res://x/y", URLIS_NOHISTORY, -1 },
    };
    run_is(c, sizeof c / sizeof *c);
    /* the dedicated wrappers agree with UrlIs */
    static const char *urls[] = { "mailto:a@b.com", "http://a/", "about:blank", "javascript:x", "res://a/b", "" };
    for (size_t i = 0; i < sizeof urls / sizeof *urls; i++) {
        WCHAR w[64]; g_widen(urls[i], w, 64);
        CHECKF(!!U(UrlIsOpaqueA)(urls[i]) == !!U(UrlIsA)(urls[i], URLIS_OPAQUE), "opaque A \"%s\"", urls[i]);
        CHECKF(!!U(UrlIsOpaqueW)(w) == !!U(UrlIsW)(w, URLIS_OPAQUE), "opaque W \"%s\"", urls[i]);
        CHECKF(!!U(UrlIsNoHistoryA)(urls[i]) == !!U(UrlIsA)(urls[i], URLIS_NOHISTORY), "nohist A \"%s\"", urls[i]);
        CHECKF(!!U(UrlIsNoHistoryW)(w) == !!U(UrlIsW)(w, URLIS_NOHISTORY), "nohist W \"%s\"", urls[i]);
    }
    CHECK(U(UrlIsOpaqueA)("mailto:a@b.com"));
    CHECK(U(UrlIsOpaqueW)(L"mailto:a@b.com"));
    CHECK(!U(UrlIsOpaqueA)("http://a/"));
    CHECK(!U(UrlIsOpaqueW)(L"http://a/"));
    CHECK(!U(UrlIsNoHistoryA)("http://a/"));
    CHECK(!U(UrlIsNoHistoryW)(L"http://a/"));
}
TEST(gap15_urlis_fileurl_query_dir) {
    REQUIRE(U(UrlIsA)); REQUIRE(U(UrlIsW));
    static const is_case c[] = {
        { "file:///c:/a.txt", URLIS_FILEURL, 1 }, { "file://server/share/a", URLIS_FILEURL, 1 }, { "FILE:///c:/a", URLIS_FILEURL, 1 },
        { "http://a/", URLIS_FILEURL, 0 }, { "c:\\a.txt", URLIS_FILEURL, 0 }, { "", URLIS_FILEURL, 0 }, { "mailto:a@b", URLIS_FILEURL, 0 },
        { "http://a/p?x=1", URLIS_HASQUERY, -1 }, { "http://a/p?x", URLIS_HASQUERY, -1 }, { "http://a/p?", URLIS_HASQUERY, 0 },
        { "http://a/p", URLIS_HASQUERY, 0 }, { "http://a/p#f?x", URLIS_HASQUERY, -1 }, { "plain?x=1", URLIS_HASQUERY, 0 },
        { "http://a/dir/", URLIS_DIRECTORY, -1 }, { "http://a/dir/f.htm", URLIS_DIRECTORY, -1 }, { "http://a/", URLIS_DIRECTORY, -1 },
        { "http://a", URLIS_DIRECTORY, -1 }, { "file:///c:/dir/", URLIS_DIRECTORY, -1 }, { "plain", URLIS_DIRECTORY, -1 },
        { "http://a/p", URLIS_APPLIABLE, -1 }, { "www.a.com", URLIS_APPLIABLE, -1 }, { "plain", URLIS_APPLIABLE, -1 }, { "c:\\a", URLIS_APPLIABLE, -1 },
    };
    run_is(c, sizeof c / sizeof *c);
}
TEST(gap15_urlis_invalid_kind) {
    REQUIRE(U(UrlIsW));
    BOOL r = U(UrlIsW)(L"http://a/", (URLIS)99);
    t_note("UrlIsW invalid kind -> %d", r);
    CHECKF(r == FALSE, "invalid kind returns %d", r);
    BOOL ra = U(UrlIsA)("http://a/", (URLIS)99);
    t_note("UrlIsA invalid kind -> %d", ra);
    r = U(UrlIsW)(L"http://a/", (URLIS)-1);
    t_note("UrlIsW kind -1 -> %d", r);
}

/* ---------------------------------------------------------------- UrlGetLocation */
TEST(gap15_urlgetlocation) {
    REQUIRE(U(UrlGetLocationA)); REQUIRE(U(UrlGetLocationW));
    static const struct { const char *u; int off; } c[] = {
        { "http://a/b#frag", 10 }, { "http://a/b#", 10 }, { "http://a/b#x#y", 10 }, { "http://a/b?q=1#f", 14 },
        { "http://a/b", -1 }, { "http://a/b?q=1", -1 }, { "http://a/#", 9 }, { "ftp://a/f#g", 9 },
        { "foo#bar", -1 }, { "", -1 }, { "plain", -1 }, { "#frag", -1 }, { "mailto:a@b.com#x", -2 }, { "javascript:a#b", -2 },
    };
    for (size_t i = 0; i < sizeof c / sizeof *c; i++) {
        WCHAR w[128]; g_widen(c[i].u, w, 128);
        LPCSTR a = U(UrlGetLocationA)(c[i].u);
        LPCWSTR b = U(UrlGetLocationW)(w);
        if (c[i].off == -2) {
            /* result depends on the per-scheme flag table in kernelbase (bit 0 -> no fragment); not verified */
            t_note("UrlGetLocation(\"%s\") A=%s W=%s", c[i].u, a ? "non-null" : "NULL", b ? "non-null" : "NULL");
        } else if (c[i].off < 0) {
            CHECKF(a == NULL, "A(\"%s\") = %p want NULL", c[i].u, (const void *)a);
            CHECKF(b == NULL, "W(\"%s\") = %p want NULL", c[i].u, (const void *)b);
        } else {
            CHECKF(a == c[i].u + c[i].off, "A(\"%s\") offset %d want %d", c[i].u, a ? (int)(a - c[i].u) : -1, c[i].off);
            CHECKF(b == w + c[i].off, "W(\"%s\") offset %d want %d", c[i].u, b ? (int)(b - w) : -1, c[i].off);
            if (a) CHECK(*a == '#');
        }
    }
    CHECK(U(UrlGetLocationA)(NULL) == NULL);
    CHECK(U(UrlGetLocationW)(NULL) == NULL);
    const char *fu = "file:///c:/dir/a#b";
    WCHAR fw[64]; g_widen(fu, fw, 64);
    LPCSTR fa = U(UrlGetLocationA)(fu);
    LPCWSTR fb = U(UrlGetLocationW)(fw);
    t_note("UrlGetLocation(file url) A off=%d W off=%d", fa ? (int)(fa - fu) : -1, fb ? (int)(fb - fw) : -1);
}

/* ---------------------------------------------------------------------- UrlHash */
TEST(gap15_urlhash_args) {
    REQUIRE(U(UrlHashA)); REQUIRE(U(UrlHashW));
    /* decomp: NULL url/out/pcch -> E_INVALIDARG before any dereference */
    BYTE h[16];
    CHECK(U(UrlHashA)(NULL, h, 16) == G_E_INVALIDARG);
    CHECK(U(UrlHashA)("http://a/", NULL, 16) == G_E_INVALIDARG);
    CHECK(U(UrlHashW)(NULL, h, 16) == G_E_INVALIDARG);
    CHECK(U(UrlHashW)(L"http://a/", NULL, 16) == G_E_INVALIDARG);
    CHECK(U(UrlHashA)(NULL, NULL, 0) == G_E_INVALIDARG);
}
TEST(gap15_urlhash_basic) {
    REQUIRE(U(UrlHashA)); REQUIRE(U(UrlHashW));
    static const DWORD sizes[] = { 1, 2, 3, 4, 7, 8, 16, 20, 32, 64 };
    for (size_t i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        BYTE a[80], b[80], c[80], d[80];
        memset(a, 0xCC, sizeof a); memset(b, 0xCC, sizeof b); memset(c, 0xCC, sizeof c); memset(d, 0xCC, sizeof d);
        DWORD cb = sizes[i];
        CHECKF(U(UrlHashA)("http://www.example.com/a", a, cb) == S_OK, "A cb=%lu", (unsigned long)cb);
        CHECKF(U(UrlHashW)(L"http://www.example.com/a", b, cb) == S_OK, "W cb=%lu", (unsigned long)cb);
        CHECKF(memcmp(a, b, cb) == 0, "A/W hash differ cb=%lu", (unsigned long)cb);
        CHECKF(U(UrlHashA)("http://www.example.com/a", c, cb) == S_OK && !memcmp(a, c, cb), "deterministic cb=%lu", (unsigned long)cb);
        CHECKF(U(UrlHashA)("http://www.example.com/b", d, cb) == S_OK, "other url");
        if (cb >= 4) CHECKF(memcmp(a, d, cb) != 0, "different urls hashed equal at cb=%lu", (unsigned long)cb);
        for (DWORD k = cb; k < 80; k++) CHECKF(a[k] == 0xCC, "byte %lu beyond cb=%lu was written", (unsigned long)k, (unsigned long)cb);
    }
}
TEST(gap15_urlhash_inputs) {
    REQUIRE(U(UrlHashA)); REQUIRE(U(UrlHashW));
    BYTE a[16], b[16];
    memset(a, 0, 16); memset(b, 0, 16);
    CHECK(U(UrlHashA)("", a, 16) == S_OK);
    CHECK(U(UrlHashW)(L"", b, 16) == S_OK);
    CHECK(!memcmp(a, b, 16));
    BYTE c[16]; memset(c, 0, 16);
    CHECK(U(UrlHashA)("x", c, 16) == S_OK);
    CHECK(memcmp(a, c, 16) != 0);
    /* cb 0 writes nothing */
    BYTE z[4] = { 1, 2, 3, 4 };
    HRESULT hr = U(UrlHashA)("http://a/", z, 0);
    t_note("UrlHashA cb=0 hr=0x%lx", (unsigned long)hr);
    CHECK(z[0] == 1 && z[1] == 2 && z[2] == 3 && z[3] == 4);
    /* case sensitivity is implementation defined: observe */
    BYTE u1[16], u2[16];
    U(UrlHashA)("HTTP://WWW.A.COM/", u1, 16);
    U(UrlHashA)("http://www.a.com/", u2, 16);
    t_note("UrlHash case-insensitive: %s", memcmp(u1, u2, 16) ? "no" : "yes");
    /* prefix property between sizes */
    BYTE p4[4], p16[16];
    U(UrlHashA)("http://www.example.com/", p4, 4);
    U(UrlHashA)("http://www.example.com/", p16, 16);
    t_note("UrlHash 4-byte is prefix of 16-byte: %s", memcmp(p4, p16, 4) ? "no" : "yes");
    /* long url */
    char big[2048]; memset(big, 'q', 2047); big[2047] = 0;
    WCHAR wbig[2048]; g_widen(big, wbig, 2048);
    BYTE hb1[16], hb2[16];
    CHECK(U(UrlHashA)(big, hb1, 16) == S_OK);
    CHECK(U(UrlHashW)(wbig, hb2, 16) == S_OK);
    CHECK(!memcmp(hb1, hb2, 16));
    /* embedded high chars through W: converted to ANSI first */
    BYTE h3[16];
    hr = U(UrlHashW)(L"http://a/\x00e9", h3, 16);
    t_note("UrlHashW non-ASCII hr=0x%lx", (unsigned long)hr);
}

/* ------------------------------------------------------------------- UrlCompare */
TEST(gap15_urlcompare_basic) {
    REQUIRE(U(UrlCompareA)); REQUIRE(U(UrlCompareW));
    static const struct { const char *a, *b; BOOL ign; int sign; } c[] = {
        { "http://a/b", "http://a/b", FALSE, 0 },
        { "http://a/b", "http://a/b", TRUE, 0 },
        { "http://a/b/", "http://a/b", FALSE, 1 },
        { "http://a/b", "http://a/b/", FALSE, -1 },
        { "http://a/b/", "http://a/b", TRUE, 0 },
        { "http://a/b", "http://a/b/", TRUE, 0 },
        { "http://a/b//", "http://a/b", TRUE, 1 },
        { "http://a/b/", "http://a/b//", TRUE, -1 },
        { "http://a/%41", "http://a/A", FALSE, 0 },
        { "http://a/A", "http://a/%41", FALSE, 0 },
        { "http://a/%41", "http://a/%61", FALSE, 9 },
        { "http://a/a", "http://a/B", FALSE, 9 },
        { "http://a/B", "http://a/a", FALSE, 9 },
        { "http://a/B", "http://a/b", FALSE, 9 },
        { "a", "b", FALSE, -1 },
        { "b", "a", FALSE, 1 },
        { "", "", FALSE, 0 },
        { "", "a", FALSE, -1 },
        { "a", "", FALSE, 1 },
        { "/", "", TRUE, 0 },
        { "/", "", FALSE, 1 },
        { "http://a/b%20c", "http://a/b c", FALSE, 0 },
        { "http://a/b%2", "http://a/b%2", FALSE, 0 },
        { "ab", "abc", FALSE, -1 },
    };
    for (size_t i = 0; i < sizeof c / sizeof *c; i++) {
        WCHAR wa[128], wb[128]; g_widen(c[i].a, wa, 128); g_widen(c[i].b, wb, 128);
        int ra = U(UrlCompareA)(c[i].a, c[i].b, c[i].ign);
        int rw = U(UrlCompareW)(wa, wb, c[i].ign);
        int sa = ra < 0 ? -1 : ra > 0 ? 1 : 0, sw = rw < 0 ? -1 : rw > 0 ? 1 : 0;
        if (c[i].sign == 9) { CHECKF(ra != 0 && rw != 0, "UrlCompare(\"%s\",\"%s\") should differ", c[i].a, c[i].b); continue; }
        CHECKF(sa == c[i].sign, "UrlCompareA(\"%s\",\"%s\",%d)=%d want sign %d", c[i].a, c[i].b, c[i].ign, ra, c[i].sign);
        CHECKF(sw == c[i].sign, "UrlCompareW(\"%s\",\"%s\",%d)=%d want sign %d", c[i].a, c[i].b, c[i].ign, rw, c[i].sign);
    }
}
TEST(gap15_urlcompare_self_and_antisym) {
    REQUIRE(U(UrlCompareA)); REQUIRE(U(UrlCompareW));
    static const char *s[] = { "http://a/", "http://a/b", "http://b/", "file:///c:/x", "x", "", "http://a/b/c" };
    for (size_t i = 0; i < 7; i++) {
        CHECK(U(UrlCompareA)(s[i], s[i], FALSE) == 0);
        for (size_t j = 0; j < 7; j++) {
            int x = U(UrlCompareA)(s[i], s[j], FALSE), y = U(UrlCompareA)(s[j], s[i], FALSE);
            CHECKF((x < 0) == (y > 0) && (x == 0) == (y == 0), "antisymmetry \"%s\" \"%s\"", s[i], s[j]);
        }
    }
}
TEST(gap15_urlcompare_ignoreslash_values) {
    REQUIRE(U(UrlCompareA));
    /* any nonzero fIgnoreSlash behaves as TRUE */
    CHECK(U(UrlCompareA)("http://a/b/", "http://a/b", 2) == 0);
    CHECK(U(UrlCompareA)("http://a/b/", "http://a/b", (BOOL)-1) == 0);
}

/* ------------------------------------------------------------- UrlCreateFromPath */
TEST(gap15_urlfrompath_args) {
    REQUIRE(U(UrlCreateFromPathA)); REQUIRE(U(UrlCreateFromPathW));
    /* decomp: NULL url/out/pcch -> E_INVALIDARG before any dereference */
    g_args4("UrlCreateFromPath", (g_fnA4)U(UrlCreateFromPathA), (g_fnW4)U(UrlCreateFromPathW), "c:\\dir", 0);
}
TEST(gap15_urlfrompath_basic) {
    REQUIRE(U(UrlCreateFromPathA)); REQUIRE(U(UrlCreateFromPathW));
    static const g_case4 c[] = {
        { "c:\\dir\\file.txt", 0, S_OK, "file:///c:/dir/file.txt", 1 },
        { "C:\\dir\\file.txt", 0, S_OK, "file:///C:/dir/file.txt", 1 },
        { "c:\\a b\\c.txt", 0, S_OK, "file:///c:/a%20b/c.txt", 1 },
        { "c:\\", 0, S_OK, "file:///c:/", 1 },
        { "c:/dir/file.txt", 0, S_OK, "file:///c:/dir/file.txt", 1 },
        { "c:\\dir\\", 0, S_OK, "file:///c:/dir/", 1 },
        { "c:\\a#b", 0, S_OK, NULL, 0 },
        { "c:\\a%b", 0, S_OK, NULL, 0 },
        { "c:\\a&b", 0, S_OK, NULL, 0 },
        { "c:\\a;b", 0, S_OK, NULL, 0 },
        { "c:\\a+b", 0, S_OK, NULL, 0 },
        { "\\\\server\\share\\f.txt", 0, S_OK, "file://server/share/f.txt", 1 },
        { "\\\\server\\share\\a b.txt", 0, S_OK, "file://server/share/a%20b.txt", 1 },
        { "\\dir\\file", 0, S_OK, NULL, 0 },
        { "dir\\file", 0, S_OK, NULL, 0 },
        { "file", 0, S_OK, NULL, 0 },
        { "", 0, S_OK, NULL, 0 },
        { "c:", 0, S_OK, NULL, 0 },
        { "http://a/b", 0, S_FALSE, NULL, 0 },
        { "file:///c:/x", 0, S_FALSE, NULL, 0 },
        { "c|\\dir", 0, S_OK, NULL, 0 },
        { "\\\\?\\c:\\dir\\f", 0, S_OK, NULL, 0 },
    };
    for (size_t i = 0; i < sizeof c / sizeof *c; i++) {
        g_res r; g_run4((g_fnA4)U(UrlCreateFromPathA), (g_fnW4)U(UrlCreateFromPathW), c[i].in, c[i].flags, 512, &r);
        CHECKF(r.hrA == r.hrW, "UrlCreateFromPath(\"%s\") A/W hr 0x%lx/0x%lx", c[i].in, (unsigned long)r.hrA, (unsigned long)r.hrW);
        if (SUCCEEDED(r.hrA) && SUCCEEDED(r.hrW)) CHECKF(g_waeq(r.w, r.a), "A/W differ for \"%s\"", c[i].in);
        if (c[i].hard) {
            CHECKF(r.hrA == S_OK, "UrlCreateFromPath(\"%s\") hr=0x%lx", c[i].in, (unsigned long)r.hrA);
            CHECKF(!strcmp(r.a, c[i].out), "UrlCreateFromPath(\"%s\") -> \"%s\" want \"%s\"", c[i].in, r.a, c[i].out);
            CHECKF(r.nA == strlen(c[i].out) && r.nW == r.nA, "count %lu/%lu", (unsigned long)r.nA, (unsigned long)r.nW);
        } else
            t_note("UrlCreateFromPath(\"%s\") hr=0x%lx \"%s\"", c[i].in, (unsigned long)r.hrA, SUCCEEDED(r.hrA) ? r.a : "-");
    }
}
TEST(gap15_urlfrompath_flags) {
    REQUIRE(U(UrlCreateFromPathA)); REQUIRE(U(UrlCreateFromPathW));
    g_res r;
    g_run4((g_fnA4)U(UrlCreateFromPathA), (g_fnW4)U(UrlCreateFromPathW), "c:\\dir\\a b", URL_ESCAPE_SPACES_ONLY, 256, &r);
    t_note("UrlCreateFromPath ESCAPE_SPACES_ONLY hr=0x%lx \"%s\"", (unsigned long)r.hrA, SUCCEEDED(r.hrA) ? r.a : "-");
    g_run4((g_fnA4)U(UrlCreateFromPathA), (g_fnW4)U(UrlCreateFromPathW), "c:\\dir\\a b", URL_ESCAPE_PERCENT, 256, &r);
    t_note("UrlCreateFromPath ESCAPE_PERCENT hr=0x%lx \"%s\"", (unsigned long)r.hrA, SUCCEEDED(r.hrA) ? r.a : "-");
    g_run4((g_fnA4)U(UrlCreateFromPathA), (g_fnW4)U(UrlCreateFromPathW), "c:\\dir\\a b", URL_UNESCAPE, 256, &r);
    t_note("UrlCreateFromPath UNESCAPE hr=0x%lx \"%s\"", (unsigned long)r.hrA, SUCCEEDED(r.hrA) ? r.a : "-");
}
TEST(gap15_urlfrompath_truncation) {
    REQUIRE(U(UrlCreateFromPathA)); REQUIRE(U(UrlCreateFromPathW));
    g_trunc4("UrlCreateFromPath", (g_fnA4)U(UrlCreateFromPathA), (g_fnW4)U(UrlCreateFromPathW), "c:\\dir\\file.txt", 0, "file:///c:/dir/file.txt");
}
TEST(gap15_urlfrompath_roundtrip_pathfromurl) {
    REQUIRE(U(UrlCreateFromPathW));
    /* the URL must canonicalize to itself */
    REQUIRE(U(UrlCanonicalizeW));
    WCHAR url[256], can[256]; DWORD n = 256, m = 256;
    HRESULT hr = U(UrlCreateFromPathW)(L"c:\\dir\\sub\\file.txt", url, &n, 0);
    CHECK(hr == S_OK);
    hr = U(UrlCanonicalizeW)(url, can, &m, 0);
    CHECK(hr == S_OK);
    t_note("canonicalized file url: %ls", can);
    CHECK(!lstrcmpW(url, can));
}

/* ----------------------------------------------------------------- UrlApplyScheme */
TEST(gap15_urlapply_args) {
    REQUIRE(U(UrlApplySchemeA)); REQUIRE(U(UrlApplySchemeW));
    g_args4("UrlApplyScheme", (g_fnA4)U(UrlApplySchemeA), (g_fnW4)U(UrlApplySchemeW), "www.a.com", URL_APPLY_GUESSSCHEME);
}
TEST(gap15_urlapply_has_scheme) {
    REQUIRE(U(UrlApplySchemeA)); REQUIRE(U(UrlApplySchemeW));
    /* an input that already has a scheme is not modified: S_FALSE */
    static const char *in[] = { "http://www.a.com", "ftp://a/b", "file:///c:/x", "mailto:a@b.com", "https://a" };
    for (size_t i = 0; i < sizeof in / sizeof *in; i++) {
        static const DWORD fl[] = { URL_APPLY_GUESSSCHEME, URL_APPLY_GUESSFILE, URL_APPLY_GUESSSCHEME | URL_APPLY_GUESSFILE, URL_APPLY_DEFAULT };
        for (size_t k = 0; k < 4; k++) {
            g_res r;
            g_run4((g_fnA4)U(UrlApplySchemeA), (g_fnW4)U(UrlApplySchemeW), in[i], fl[k], 256, &r);
            CHECKF(r.hrA == S_FALSE && r.hrW == S_FALSE, "UrlApplyScheme(\"%s\",0x%lx) hr=0x%lx/0x%lx", in[i], (unsigned long)fl[k], (unsigned long)r.hrA, (unsigned long)r.hrW);
        }
    }
}
TEST(gap15_urlapply_guess) {
    REQUIRE(U(UrlApplySchemeA)); REQUIRE(U(UrlApplySchemeW));
    static const g_case4 c[] = {
        { "www.winehq.org", URL_APPLY_GUESSSCHEME, S_OK, NULL, 0 },
        { "www.winehq.org", URL_APPLY_GUESSSCHEME | URL_APPLY_DEFAULT, S_OK, NULL, 0 },
        { "ftp.winehq.org", URL_APPLY_GUESSSCHEME, S_OK, NULL, 0 },
        { "gopher.winehq.org", URL_APPLY_GUESSSCHEME, S_OK, NULL, 0 },
        { "winehq.org", URL_APPLY_GUESSSCHEME, S_OK, NULL, 0 },
        { "plain", URL_APPLY_GUESSSCHEME, S_OK, NULL, 0 },
        { "plain", URL_APPLY_DEFAULT, S_OK, NULL, 0 },
        { "plain", URL_APPLY_FORCEAPPLY | URL_APPLY_DEFAULT, S_OK, NULL, 0 },
        { "plain", 0, S_OK, NULL, 0 },
        { "c:\\dir\\file.txt", URL_APPLY_GUESSFILE, S_OK, NULL, 0 },
        { "c:\\dir\\file.txt", URL_APPLY_GUESSSCHEME, S_OK, NULL, 0 },
        { "c:\\dir\\file.txt", URL_APPLY_GUESSFILE | URL_APPLY_GUESSSCHEME, S_OK, NULL, 0 },
        { "\\\\server\\share\\f", URL_APPLY_GUESSFILE, S_OK, NULL, 0 },
        { "c:/dir/file.txt", URL_APPLY_GUESSFILE, S_OK, NULL, 0 },
        { "www.winehq.org", URL_APPLY_FORCEAPPLY | URL_APPLY_GUESSSCHEME, S_OK, NULL, 0 },
        { "", URL_APPLY_GUESSSCHEME, S_OK, NULL, 0 },
    };
    g_check4("UrlApplyScheme", (g_fnA4)U(UrlApplySchemeA), (g_fnW4)U(UrlApplySchemeW), c, sizeof c / sizeof *c);
}
TEST(gap15_urlapply_www_http) {
    REQUIRE(U(UrlApplySchemeW));
    WCHAR out[256]; DWORD n = 256;
    HRESULT hr = U(UrlApplySchemeW)(L"www.winehq.org", out, &n, URL_APPLY_GUESSSCHEME);
    char o[256]; g_narrow(out, o, sizeof o);
    CHECKF(hr == S_OK, "hr=0x%lx", (unsigned long)hr);
    if (hr == S_OK) CHECKF(!strncmp(o, "http://", 7) && strstr(o, "www.winehq.org"), "got \"%s\"", o);
}
TEST(gap15_urlapply_truncation) {
    REQUIRE(U(UrlApplySchemeW));
    WCHAR out[256]; DWORD n = 256;
    HRESULT hr = U(UrlApplySchemeW)(L"www.winehq.org", out, &n, URL_APPLY_GUESSSCHEME);
    if (hr != S_OK) { t_skip("no scheme guess available"); return; }
    DWORD len = n;
    n = len; /* exact len without room for NUL */
    hr = U(UrlApplySchemeW)(L"www.winehq.org", out, &n, URL_APPLY_GUESSSCHEME);
    CHECKF(hr == G_E_POINTER, "short hr=0x%lx", (unsigned long)hr);
    CHECKF(n == len + 1, "needed %lu want %lu", (unsigned long)n, (unsigned long)len + 1);
    n = 1;
    hr = U(UrlApplySchemeW)(L"www.winehq.org", out, &n, URL_APPLY_GUESSSCHEME);
    CHECK(hr == G_E_POINTER && n == len + 1);
    n = len + 1;
    hr = U(UrlApplySchemeW)(L"www.winehq.org", out, &n, URL_APPLY_GUESSSCHEME);
    CHECK(hr == S_OK && n == len);
}
