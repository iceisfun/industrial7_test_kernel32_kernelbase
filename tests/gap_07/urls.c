/* gap7: ParseURLA / ParseURLW (exported by kernelbase) */
#include "t.h"
#include <shlwapi.h>

#define K_INVALIDARG       ((HRESULT)0x80070057)
#define K_URL_SYNTAX       ((HRESULT)0x80041001)

struct purl_case {
    const char *url;
    int ok;              /* 1: S_OK */
    unsigned cch_proto;
    unsigned suffix_off; /* offset of suffix within url */
    int scheme;
};

static const struct purl_case purl_cases[] = {
    { "http://www.example.com/a?b=c",      1, 4, 5,  2 },
    { "HTTP://www.example.com",            1, 4, 5,  2 },
    { "HtTpS://host",                      1, 5, 6, 11 },
    { "ftp://ftp.example.com/x",           1, 3, 4,  1 },
    { "gopher://g",                        1, 6, 7,  3 },
    { "mailto:user@example.com",           1, 6, 7,  4 },
    { "news:comp.lang.c",                  1, 4, 5,  5 },
    { "nntp://n",                          1, 4, 5,  6 },
    { "telnet://t",                        1, 6, 7,  7 },
    { "wais://w",                          1, 4, 5,  8 },
    { "mk:@MSITStore:x.chm",               1, 2, 3, 10 },
    { "shell:Desktop",                     1, 5, 6, 12 },
    { "snews://s",                         1, 5, 6, 13 },
    { "local:abc",                         1, 5, 6, 14 },
    { "javascript:alert(1)",               1, 10, 11, 15 },
    { "vbscript:msgbox(1)",                1, 8, 9, 16 },
    { "about:blank",                       1, 5, 6, 17 },
    { "res://mod.dll/1",                   1, 3, 4, 18 },
    /* file: leading // (and one extra /) are skipped */
    { "file:///c:/dir/file.txt",           1, 4, 8, 9 },
    { "file://host/share/f",               1, 4, 7, 9 },
    { "file:/c:/x",                        1, 4, 5, 9 },
    { "file:",                             1, 4, 5, 9 },
    { "FILE:///C:/x",                      1, 4, 8, 9 },
    /* unknown schemes */
    { "foo:bar",                           1, 3, 4, 0 },
    { "xx:",                               1, 2, 3, 0 },
    { "a.b+c-d:payload",                   1, 7, 8, 0 },
    { "abc123:z",                          1, 6, 7, 0 },
    { "httpx://h",                         1, 5, 6, 0 },
    { "htt://h",                           1, 3, 4, 0 },
    /* only the first colon matters */
    { "http:a:b:c",                        1, 4, 5, 2 },
    /* no / bad scheme */
    { "",                                  0, 0, 0, 0 },
    { "nocolon",                           0, 0, 0, 0 },
    { ":nothing",                          0, 0, 0, 0 },
    { "c:\\windows",                       0, 0, 0, 0 },    /* single-character scheme is rejected */
    { "x:",                                0, 0, 0, 0 },
    { "ab c:rest",                         0, 0, 0, 0 },    /* space is not a scheme character */
    { "http//host:80",                     0, 0, 0, 0 },    /* '/' before the colon */
    { "ht*tp:x",                           0, 0, 0, 0 },
    { "\\\\server\\share",                 0, 0, 0, 0 },
    { "www.example.com",                   0, 0, 0, 0 },
    { " http://x",                         0, 0, 0, 0 },
};

static void wide(WCHAR *w, const char *s) { while ((*w++ = (unsigned char)*s++)) ; }

TEST(gap7_parseurlA_table) {
    REQUIRE(U(ParseURLA));
    for (size_t i = 0; i < sizeof purl_cases / sizeof purl_cases[0]; i++) {
        const struct purl_case *c = &purl_cases[i];
        PARSEDURLA pu; memset(&pu, 0xCC, sizeof pu); pu.cbSize = sizeof pu;
        HRESULT hr = U(ParseURLA)(c->url, &pu);
        if (c->ok) {
            CHECKF(hr == S_OK, "\"%s\" hr=%08lx", c->url, (unsigned long)hr);
            if (hr != S_OK) continue;
            CHECKF(pu.pszProtocol == c->url, "\"%s\" protocol ptr", c->url);
            CHECKF(pu.cchProtocol == c->cch_proto, "\"%s\" cchProtocol %u want %u", c->url, pu.cchProtocol, c->cch_proto);
            CHECKF(pu.pszSuffix == c->url + c->suffix_off, "\"%s\" suffix off %d want %u", c->url, (int)(pu.pszSuffix - c->url), c->suffix_off);
            CHECKF(pu.cchSuffix == strlen(c->url + c->suffix_off), "\"%s\" cchSuffix %u", c->url, pu.cchSuffix);
            CHECKF((int)pu.nScheme == c->scheme, "\"%s\" nScheme %d want %d", c->url, (int)pu.nScheme, c->scheme);
        } else {
            CHECKF(hr == K_URL_SYNTAX, "\"%s\" hr=%08lx want URL_E_INVALID_SYNTAX", c->url, (unsigned long)hr);
            CHECKF(pu.pszProtocol == NULL, "\"%s\" pszProtocol cleared", c->url);
        }
    }
}

TEST(gap7_parseurlW_table) {
    REQUIRE(U(ParseURLW));
    for (size_t i = 0; i < sizeof purl_cases / sizeof purl_cases[0]; i++) {
        const struct purl_case *c = &purl_cases[i];
        WCHAR w[128]; wide(w, c->url);
        PARSEDURLW pu; memset(&pu, 0xCC, sizeof pu); pu.cbSize = sizeof pu;
        HRESULT hr = U(ParseURLW)(w, &pu);
        if (c->ok) {
            CHECKF(hr == S_OK, "\"%s\" hr=%08lx", c->url, (unsigned long)hr);
            if (hr != S_OK) continue;
            CHECKF(pu.pszProtocol == w, "\"%s\" protocol ptr", c->url);
            CHECKF(pu.cchProtocol == c->cch_proto, "\"%s\" cchProtocol %u", c->url, pu.cchProtocol);
            CHECKF(pu.pszSuffix == w + c->suffix_off, "\"%s\" suffix off %d want %u", c->url, (int)(pu.pszSuffix - w), c->suffix_off);
            CHECKF(pu.cchSuffix == strlen(c->url + c->suffix_off), "\"%s\" cchSuffix %u", c->url, pu.cchSuffix);
            CHECKF((int)pu.nScheme == c->scheme, "\"%s\" nScheme %d want %d", c->url, (int)pu.nScheme, c->scheme);
        } else {
            CHECKF(hr == K_URL_SYNTAX, "\"%s\" hr=%08lx want URL_E_INVALID_SYNTAX", c->url, (unsigned long)hr);
        }
    }
}

TEST(gap7_parseurlA_invalid_args) {
    REQUIRE(U(ParseURLA));
    PARSEDURLA pu; memset(&pu, 0, sizeof pu); pu.cbSize = sizeof pu;
    if (t_is_wine()) { t_skip("NULL arguments are not validated by this implementation"); return; }
    CHECK(U(ParseURLA)(NULL, &pu) == K_INVALIDARG);
    CHECK(U(ParseURLA)("http://x", NULL) == K_INVALIDARG);
    CHECK(U(ParseURLA)(NULL, NULL) == K_INVALIDARG);
    static const DWORD sizes[] = { 0, 1, 4, 8, 20, 36, 39, 41, 44, 48, 64, 0x100, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        memset(&pu, 0, sizeof pu); pu.cbSize = sizes[i];
        HRESULT hr = U(ParseURLA)("http://x", &pu);
        CHECKF(hr == K_INVALIDARG, "cbSize=%lu hr=%08lx", sizes[i], (unsigned long)hr);
        CHECKF(pu.pszProtocol == NULL && pu.cchProtocol == 0, "output untouched for cbSize=%lu", sizes[i]);
    }
    memset(&pu, 0, sizeof pu); pu.cbSize = sizeof pu;
    CHECK(U(ParseURLA)("http://x", &pu) == S_OK);
}

TEST(gap7_parseurlW_invalid_args) {
    REQUIRE(U(ParseURLW));
    PARSEDURLW pu; memset(&pu, 0, sizeof pu); pu.cbSize = sizeof pu;
    if (t_is_wine()) { t_skip("NULL arguments are not validated by this implementation"); return; }
    CHECK(U(ParseURLW)(NULL, &pu) == K_INVALIDARG);
    CHECK(U(ParseURLW)(L"http://x", NULL) == K_INVALIDARG);
    CHECK(U(ParseURLW)(NULL, NULL) == K_INVALIDARG);
    static const DWORD sizes[] = { 0, 1, 4, 8, 20, 36, 39, 41, 44, 48, 64, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        memset(&pu, 0, sizeof pu); pu.cbSize = sizes[i];
        CHECKF(U(ParseURLW)(L"http://x", &pu) == K_INVALIDARG, "cbSize=%lu", sizes[i]);
    }
}

TEST(gap7_parseurl_struct_size) {
    CHECK(sizeof(PARSEDURLA) == 0x28);
    CHECK(sizeof(PARSEDURLW) == 0x28);
}

TEST(gap7_parseurlW_nonascii_scheme_rejected) {
    REQUIRE(U(ParseURLW));
    PARSEDURLW pu; memset(&pu, 0, sizeof pu); pu.cbSize = sizeof pu;
    CHECK(U(ParseURLW)(L"h\x00e9tp://x", &pu) == K_URL_SYNTAX);
    CHECK(U(ParseURLW)(L"\x4e2d\x6587:x", &pu) == K_URL_SYNTAX);
    CHECK(U(ParseURLW)(L"ht\x0100p:x", &pu) == K_URL_SYNTAX);
    /* a non-ASCII suffix is fine */
    memset(&pu, 0, sizeof pu); pu.cbSize = sizeof pu;
    HRESULT hr = U(ParseURLW)(L"foo:\x4e2d\x6587", &pu);
    CHECK(hr == S_OK);
    CHECK(pu.cchSuffix == 2);
}

TEST(gap7_parseurlA_suffix_extent_long) {
    REQUIRE(U(ParseURLA));
    char url[2048]; strcpy(url, "http://");
    memset(url + 7, 'a', 2000); url[2007] = 0;
    PARSEDURLA pu; memset(&pu, 0, sizeof pu); pu.cbSize = sizeof pu;
    CHECK(U(ParseURLA)(url, &pu) == S_OK);
    CHECK(pu.cchSuffix == 2002);
    CHECK(pu.pszSuffix == url + 5);
}

TEST(gap7_parseurlA_does_not_modify_input) {
    REQUIRE(U(ParseURLA));
    char url[] = "https://example.com/path";
    char copy[sizeof url]; memcpy(copy, url, sizeof url);
    PARSEDURLA pu; memset(&pu, 0, sizeof pu); pu.cbSize = sizeof pu;
    CHECK(U(ParseURLA)(url, &pu) == S_OK);
    CHECK(!memcmp(url, copy, sizeof url));
    /* repeat: identical results */
    PARSEDURLA p2; memset(&p2, 0, sizeof p2); p2.cbSize = sizeof p2;
    CHECK(U(ParseURLA)(url, &p2) == S_OK);
    CHECK(!memcmp(&pu, &p2, sizeof pu));
}

TEST(gap7_parseurl_AW_parity) {
    REQUIRE(U(ParseURLA)); REQUIRE(U(ParseURLW));
    for (size_t i = 0; i < sizeof purl_cases / sizeof purl_cases[0]; i++) {
        const struct purl_case *c = &purl_cases[i];
        WCHAR w[128]; wide(w, c->url);
        PARSEDURLA a; a.cbSize = sizeof a; PARSEDURLW b; b.cbSize = sizeof b;
        HRESULT ha = U(ParseURLA)(c->url, &a), hb = U(ParseURLW)(w, &b);
        CHECKF(ha == hb, "\"%s\" A=%08lx W=%08lx", c->url, (unsigned long)ha, (unsigned long)hb);
        if (ha == S_OK && hb == S_OK) {
            CHECKF(a.cchProtocol == b.cchProtocol && a.cchSuffix == b.cchSuffix && a.nScheme == b.nScheme, "\"%s\" field parity", c->url);
        }
    }
}

TEST(gap7_parseurl_scheme_is_exported_by_kernelbase) {
    REQUIRE(UB(ParseURLA)); REQUIRE(UB(ParseURLW));
    CHECK(UB(ParseURLA) == U(ParseURLA));
    CHECK(UB(ParseURLW) == U(ParseURLW));
}
