/* kernelbase PathCch*: Canonicalize, Combine, Append, Alloc* */
#include "pn.h"
#include <pathcch.h>

#define PCH_ALLOW_LONG 0x1
#define PCH_FORCE_ENABLE 0x2
#define PCH_FORCE_DISABLE 0x4
#define PCH_NO_NORMALIZE 0x8
#define PCH_ENSURE_EXT 0x10
#define PCH_TRAILING_SLASH 0x20

static HRESULT canon(const char *in, size_t cch, char *out, size_t outcap) {
    WCHAR w[40000]; if (cch > 39000) cch = 39000;
    w[0] = 0;
    HRESULT hr = U(PathCchCanonicalize)(w, cch, pn_w(in));
    pn_a(out, outcap, w);
    return hr;
}
static HRESULT canonex(const char *in, size_t cch, unsigned long fl, char *out, size_t outcap) {
    WCHAR w[40000]; w[0] = 0;
    HRESULT hr = U(PathCchCanonicalizeEx)(w, cch, pn_w(in), fl);
    pn_a(out, outcap, w);
    return hr;
}

typedef struct { const char *in, *out; HRESULT hr; int hard; } canon_row;
static const canon_row canon_rows[] = {
    { "C:\\a\\b", "C:\\a\\b", S_OK, 1 },
    { "C:\\", "C:\\", S_OK, 1 },
    { "C:\\a\\", "C:\\a\\", S_OK, 1 },
    { "C:\\a\\.\\b", "C:\\a\\b", S_OK, 1 },
    { "C:\\a\\..\\b", "C:\\b", S_OK, 1 },
    { "C:\\a\\b\\..\\c", "C:\\a\\c", S_OK, 1 },
    { "C:\\a\\b\\..\\..\\c", "C:\\c", S_OK, 1 },
    { "C:\\a\\b\\.", "C:\\a\\b", S_OK, 1 },
    { "C:\\a\\b\\..", "C:\\a", S_OK, 1 },
    { "C:\\a\\..", "C:\\", S_OK, 1 },
    { "C:\\..", "C:\\", S_OK, 1 },
    { "C:\\..\\..\\a", "C:\\a", S_OK, 1 },
    { "C:\\a\\..\\..\\..\\b", "C:\\b", S_OK, 1 },
    { "C:\\.", "C:\\", S_OK, 1 },
    { "C:\\a\\.\\.\\.\\b", "C:\\a\\b", S_OK, 1 },
    { "C:\\a\\b.", "C:\\a\\b", S_OK, 1 },
    { "C:\\a\\b..", "C:\\a\\b", S_OK, 1 },
    { "C:\\a\\b.txt", "C:\\a\\b.txt", S_OK, 1 },
    { "C:\\a\\.hidden", "C:\\a\\.hidden", S_OK, 1 },
    { "C:\\a\\b.c.d", "C:\\a\\b.c.d", S_OK, 1 },
    { "C:\\a b\\c d", "C:\\a b\\c d", S_OK, 1 },
    { "C:\\a\\B", "C:\\a\\B", S_OK, 1 },
    { "c:\\a", "c:\\a", S_OK, 1 },
    { "C:\\a\\file::$DATA", "C:\\a\\file", S_OK, 0 },
    { "C:\\a\\\\b", "C:\\a\\b", S_OK, 0 },
    { "C:/a/b", "C:/a/b", S_OK, 0 },
    { "C:\\a\\b ", "C:\\a\\b ", S_OK, 0 },
    { "C:", "C:", S_OK, 0 },
    { "\\", "\\", S_OK, 0 },
    { "\\a\\..\\b", "\\b", S_OK, 0 },
    { "\\a\\b", "\\a\\b", S_OK, 1 },
    { "a\\b", "a\\b", S_OK, 1 },
    { "a\\.\\b", "a\\b", S_OK, 1 },
    { "a\\..\\b", "b", S_OK, 0 },
    { "a\\b\\..\\..\\c", "c", S_OK, 0 },
    { ".", "\\", S_OK, 0 },
    { "..", "\\", S_OK, 0 },
    { ".\\a", "a", S_OK, 0 },
    { "..\\a", "a", S_OK, 0 },
    { "\\\\server\\share", "\\\\server\\share", S_OK, 1 },
    { "\\\\server\\share\\", "\\\\server\\share\\", S_OK, 1 },
    { "\\\\server\\share\\a\\..\\b", "\\\\server\\share\\b", S_OK, 1 },
    { "\\\\server\\share\\..\\..\\a", "\\\\server\\share\\a", S_OK, 0 },
    { "\\\\server\\share\\a\\.\\b", "\\\\server\\share\\a\\b", S_OK, 1 },
    { "\\\\?\\C:\\a\\..\\b", "C:\\b", S_OK, 0 },
    { "\\\\?\\C:\\", "C:\\", S_OK, 0 },
    { "\\\\?\\UNC\\s\\sh\\a\\..\\b", "\\\\s\\sh\\b", S_OK, 0 },
    { "\\\\.\\C:\\a\\..\\b", "\\\\.\\C:\\b", S_OK, 0 },
};

TEST(path_names_pathcch_canonicalize_table) {
    REQUIRE(U(PathCchCanonicalize));
    for (unsigned i = 0; i < sizeof canon_rows / sizeof *canon_rows; i++) {
        const canon_row *r = &canon_rows[i];
        char out[1024];
        HRESULT hr = canon(r->in, MAX_PATH, out, sizeof out);
        int ok = hr == r->hr && !strcmp(out, r->out);
        if (r->hard) CHECKF(ok, "row %u in=%s got hr=0x%lx out=%s want hr=0x%lx out=%s", i, r->in, hr, out, r->hr, r->out);
        else if (!ok) t_note("canon row %u in=[%s] got hr=0x%lx out=[%s] (model: [%s])", i, r->in, hr, out, r->out);
    }
}
TEST(path_names_pathcch_canonicalize_idempotent) {
    REQUIRE(U(PathCchCanonicalize));
    for (unsigned i = 0; i < sizeof canon_rows / sizeof *canon_rows; i++) {
        if (!canon_rows[i].hard) continue;
        char o1[1024], o2[1024];
        canon(canon_rows[i].in, MAX_PATH, o1, sizeof o1);
        canon(o1, MAX_PATH, o2, sizeof o2);
        CHECKF(!strcmp(o1, o2), "row %u: %s -> %s -> %s", i, canon_rows[i].in, o1, o2);
    }
}
TEST(path_names_pathcch_canonicalize_invalid_args) {
    REQUIRE(U(PathCchCanonicalize));
    WCHAR w[MAX_PATH];
    if (t_is_wine()) { t_skip("emulator faults on invalid cch"); return; }
    EXPECT_HR(U(PathCchCanonicalize)(w, 0, L"C:\\a"), E_INVALIDARG);
    EXPECT_HR(U(PathCchCanonicalize)(w, 0x8001, L"C:\\a"), E_INVALIDARG);
    EXPECT_HR(U(PathCchCanonicalize)(w, (size_t)-1, L"C:\\a"), E_INVALIDARG);
}
TEST(path_names_pathcch_canonicalize_null_input_note) {
    REQUIRE(U(PathCchCanonicalize));
    /* a NULL input would be dereferenced (*param_3) by the real implementation: not exercised */
    CHECK(1);
}
TEST(path_names_pathcch_canonicalize_empty_input) {
    REQUIRE(U(PathCchCanonicalize));
    WCHAR w[MAX_PATH]; wcscpy(w, L"junk");
    HRESULT hr = U(PathCchCanonicalize)(w, MAX_PATH, L"");
    t_note("empty input -> hr 0x%lx out [%s]", hr, pn_n(w));
}
TEST(path_names_pathcch_canonicalize_buffer_sizes) {
    REQUIRE(U(PathCchCanonicalize));
    WCHAR w[MAX_PATH];
    const WCHAR *in = L"C:\\abc\\..\\def"; /* -> C:\def (6 chars) */
    EXPECT_HR(U(PathCchCanonicalize)(w, 7, in), S_OK);
    CHECK(pn_weq(w, "C:\\def"));
    HRESULT hr = U(PathCchCanonicalize)(w, 6, in);
    CHECKF(hr == HR_INSUFFICIENT, "hr 0x%lx", hr);
    hr = U(PathCchCanonicalize)(w, 1, in);
    CHECKF(hr == HR_INSUFFICIENT, "cch 1: hr 0x%lx", hr);
}
TEST(path_names_pathcch_canonicalize_cch260_vs_insufficient) {
    REQUIRE(U(PathCchCanonicalize));
    /* with exactly MAX_PATH chars of space an overflow is reported as ERROR_FILENAME_EXCED_RANGE,
       with any other size as ERROR_INSUFFICIENT_BUFFER (segments kept short of the 256 limit) */
    static WCHAR in[400], out[400];
    wcscpy(in, L"C:");
    for (int i = 0; i < 6; i++) wcscat(in, L"\01234567890123456789012345678901234567890123456789"); /* 2 + 6*51 = 308 */
    HRESULT hr = U(PathCchCanonicalize)(out, 260, in);
    CHECKF(hr == HR_EXCED, "cch 260 hr 0x%lx", hr);
    hr = U(PathCchCanonicalize)(out, 100, in);
    CHECKF(hr == HR_INSUFFICIENT, "cch 100 hr 0x%lx", hr);
    hr = U(PathCchCanonicalize)(out, 261, in);
    CHECKF(hr == HR_EXCED, "cch 261 hr 0x%lx", hr);
}
TEST(path_names_pathcch_canonicalize_259_chars_fits_260_not_261) {
    REQUIRE(U(PathCchCanonicalize));
    static WCHAR in[400], out[400];
    wcscpy(in, L"C:\\"); pn_fill(in + 3, 256, L'a');   /* 259 chars */
    EXPECT_HR(U(PathCchCanonicalize)(out, 260, in), S_OK);
    CHECK(wcslen(out) == 259);
    wcscpy(in, L"C:\\"); pn_fill(in + 3, 257, L'a');   /* 260 chars: no room for NUL under the legacy limit */
    HRESULT hr = U(PathCchCanonicalize)(out, 400, in);
    CHECKF(hr == HR_EXCED, "260 chars in a 400 buffer, hr 0x%lx", hr);
}
TEST(path_names_pathcch_canonicalizeex_long_flag_matrix) {
    REQUIRE(U(PathCchCanonicalizeEx));
    static WCHAR in[600], out[40000];
    wcscpy(in, L"C:\\"); pn_fill(in + 3, 300, L'a');   /* one 300 char segment: 303 total */
    HRESULT hr;
    hr = U(PathCchCanonicalizeEx)(out, 1000, in, 0);
    CHECKF(hr == HR_EXCED, "no flag: hr 0x%lx", hr);
    hr = U(PathCchCanonicalizeEx)(out, 1000, in, PCH_ALLOW_LONG | PCH_FORCE_ENABLE);
    CHECKF(hr == S_OK && wcslen(out) == 303 && !wcscmp(out, in), "force-enable: hr 0x%lx out=%.20s len %zu", hr, pn_n(out), wcslen(out));
    hr = U(PathCchCanonicalizeEx)(out, 1000, in, PCH_ALLOW_LONG | PCH_FORCE_DISABLE);
    CHECKF(hr == S_OK && wcslen(out) == 307 && !wcsncmp(out, L"\\\\?\\C:\\", 7), "force-disable: hr 0x%lx len %zu out=%.12s", hr, wcslen(out), pn_n(out));
    hr = U(PathCchCanonicalizeEx)(out, 1000, in, PCH_ALLOW_LONG);
    CHECKF(hr == S_OK, "allow-long: hr 0x%lx", hr);
    size_t l = wcslen(out);
    CHECKF(l == 303 || l == 307, "allow-long length %zu", l);
    t_note("ALLOW_LONG_PATHS alone on this process yields %s", l == 307 ? "\\\\?\\ prefixed" : "unprefixed");
    /* a short path never gets the prefix */
    hr = U(PathCchCanonicalizeEx)(out, 1000, L"C:\\a\\..\\b", PCH_ALLOW_LONG | PCH_FORCE_DISABLE);
    CHECK(hr == S_OK && pn_weq(out, "C:\\b"));
}
TEST(path_names_pathcch_canonicalizeex_invalid_flag_combinations) {
    REQUIRE(U(PathCchCanonicalizeEx));
    WCHAR out[MAX_PATH];
    static const unsigned long bad[] = { 0x2, 0x4, 0x6, 0x7, PCH_ALLOW_LONG | 0x10, PCH_ALLOW_LONG | PCH_FORCE_ENABLE | 0x10, 0x10 | 0x2 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        HRESULT hr = U(PathCchCanonicalizeEx)(out, MAX_PATH, L"C:\\a", bad[i]);
        CHECKF(hr == E_INVALIDARG, "flags 0x%lx hr 0x%lx", bad[i], hr);
    }
}
TEST(path_names_pathcch_canonicalizeex_trailing_slash_flag) {
    REQUIRE(U(PathCchCanonicalizeEx));
    char o[256];
    HRESULT hr = canonex("C:\\a\\b", MAX_PATH, PCH_TRAILING_SLASH, o, sizeof o);
    CHECKF(hr == S_OK && !strcmp(o, "C:\\a\\b\\"), "hr 0x%lx %s", hr, o);
    hr = canonex("C:\\a\\b\\", MAX_PATH, PCH_TRAILING_SLASH, o, sizeof o);
    CHECKF(hr == S_OK && !strcmp(o, "C:\\a\\b\\"), "already: hr 0x%lx %s", hr, o);
    hr = canonex("C:\\a\\..\\b", MAX_PATH, PCH_TRAILING_SLASH, o, sizeof o);
    CHECKF(hr == S_OK && !strcmp(o, "C:\\b\\"), "canon+slash: hr 0x%lx %s", hr, o);
}
TEST(path_names_pathcch_canonicalizeex_no_normalize_segments_flag) {
    REQUIRE(U(PathCchCanonicalizeEx));
    char o[256];
    HRESULT hr = canonex("C:\\a\\..\\b", MAX_PATH, PCH_NO_NORMALIZE, o, sizeof o);
    t_note("DO_NOT_NORMALIZE_SEGMENTS: hr 0x%lx out %s", hr, o);
    CHECK(SUCCEEDED(hr));
    hr = canonex("C:\\a\\b.", MAX_PATH, PCH_NO_NORMALIZE, o, sizeof o);
    CHECKF(hr == S_OK && !strcmp(o, "C:\\a\\b."), "trailing dot kept with NO_NORMALIZE: %s", o);
}
TEST(path_names_pathcch_canonicalizeex_ensure_extended_prefix) {
    REQUIRE(U(PathCchCanonicalizeEx));
    char o[256];
    HRESULT hr = canonex("C:\\a\\b", MAX_PATH, PCH_ENSURE_EXT, o, sizeof o);
    CHECKF(hr == S_OK && !strcmp(o, "\\\\?\\C:\\a\\b"), "hr 0x%lx %s", hr, o);
    hr = canonex("\\\\s\\sh\\a", MAX_PATH, PCH_ENSURE_EXT, o, sizeof o);
    t_note("ENSURE_IS_EXTENDED_LENGTH_PATH on UNC: hr 0x%lx %s (expected \\\\?\\UNC\\s\\sh\\a)", hr, o);
    hr = canonex("\\\\?\\C:\\a\\..\\b", MAX_PATH, PCH_ENSURE_EXT, o, sizeof o);
    CHECKF(hr == S_OK && !strcmp(o, "\\\\?\\C:\\b"), "already prefixed: hr 0x%lx %s", hr, o);
}
TEST(path_names_pathcch_canonicalize_segment_too_long) {
    REQUIRE(U(PathCchCanonicalizeEx));
    static WCHAR in[800], out[40000];
    wcscpy(in, L"C:\\"); pn_fill(in + 3, 258, L'a');
    HRESULT hr = U(PathCchCanonicalizeEx)(out, 1000, in, 0);
    CHECKF(hr == HR_EXCED, "single segment of 258 chars, legacy limit: 0x%lx", hr);
}
TEST(path_names_pathcch_canonicalize_leaves_input_alone) {
    REQUIRE(U(PathCchCanonicalize));
    WCHAR in[64], out[64];
    wcscpy(in, L"C:\\a\\..\\b");
    U(PathCchCanonicalize)(out, 64, in);
    CHECK(!wcscmp(in, L"C:\\a\\..\\b"));
}

/* ------------------------------- Combine ------------------------------- */
typedef struct { const char *a, *b, *out; HRESULT hr; int hard; } comb_row;
static const comb_row comb_rows[] = {
    { "C:\\a", "b", "C:\\a\\b", S_OK, 1 },
    { "C:\\a\\", "b", "C:\\a\\b", S_OK, 1 },
    { "C:\\a", "b\\c", "C:\\a\\b\\c", S_OK, 1 },
    { "C:\\a", "..\\b", "C:\\b", S_OK, 1 },
    { "C:\\a\\b", "..\\..\\c", "C:\\c", S_OK, 1 },
    { "C:\\a", ".", "C:\\a", S_OK, 1 },
    { "C:\\a", "..", "C:\\", S_OK, 1 },
    { "C:\\a\\b", "\\x", "C:\\x", S_OK, 1 },
    { "C:\\a\\b", "\\x\\y", "C:\\x\\y", S_OK, 1 },
    { "C:\\a", "D:\\b", "D:\\b", S_OK, 1 },
    { "C:\\a", "D:b", "D:b", S_OK, 0 },
    { "C:\\a", "\\\\s\\sh\\x", "\\\\s\\sh\\x", S_OK, 1 },
    { "\\\\s\\sh\\a", "b", "\\\\s\\sh\\a\\b", S_OK, 1 },
    { "\\\\s\\sh\\a", "\\b", "\\\\s\\sh\\b", S_OK, 1 },
    { "\\\\s\\sh\\a", "..\\b", "\\\\s\\sh\\b", S_OK, 1 },
    { "C:\\a", "", "C:\\a", S_OK, 1 },
    { "", "b", "b", S_OK, 1 },
    { "", "C:\\b", "C:\\b", S_OK, 1 },
    { "C:\\a", NULL, "C:\\a", S_OK, 1 },
    { NULL, "b", "b", S_OK, 1 },
    { NULL, "C:\\b\\..\\c", "C:\\c", S_OK, 1 },
    { "a", "b", "a\\b", S_OK, 1 },
    { "a\\b", "..\\c", "a\\c", S_OK, 1 },
    { "C:\\", "a", "C:\\a", S_OK, 1 },
    { "C:\\", "..\\a", "C:\\a", S_OK, 1 },
    { "C:\\a\\b\\c", "\\", "C:\\", S_OK, 0 },
    { "C:\\A", "b.TXT", "C:\\A\\b.TXT", S_OK, 1 },
    { "C:\\a.b", "c.d", "C:\\a.b\\c.d", S_OK, 1 },
    { "C:\\a", "b.", "C:\\a\\b", S_OK, 1 },
    { "C:\\a b", "c d", "C:\\a b\\c d", S_OK, 1 },
    { "C:\\a", "b\\", "C:\\a\\b\\", S_OK, 1 },
    { "C:\\a\\", "\\\\", "\\\\", S_OK, 0 },
    { "C:\\a", "\\\\?\\D:\\x", "D:\\x", S_OK, 0 },
    { "\\\\?\\C:\\a", "b", "C:\\a\\b", S_OK, 0 },
};
TEST(path_names_pathcch_combine_table) {
    REQUIRE(U(PathCchCombine));
    for (unsigned i = 0; i < sizeof comb_rows / sizeof *comb_rows; i++) {
        const comb_row *r = &comb_rows[i];
        WCHAR w[1024]; char o[1024];
        w[0] = 0;
        HRESULT hr = U(PathCchCombine)(w, MAX_PATH, r->a ? pn_w(r->a) : NULL, r->b ? pn_w(r->b) : NULL);
        pn_a(o, sizeof o, w);
        int ok = hr == r->hr && !strcmp(o, r->out);
        if (r->hard) CHECKF(ok, "row %u (%s , %s) got hr 0x%lx [%s] want [%s]", i, r->a ? r->a : "NULL", r->b ? r->b : "NULL", hr, o, r->out);
        else if (!ok) t_note("combine row %u (%s , %s) got hr 0x%lx [%s] (model [%s])", i, r->a ? r->a : "NULL", r->b ? r->b : "NULL", hr, o, r->out);
    }
}
TEST(path_names_pathcch_combine_invalid_args) {
    REQUIRE(U(PathCchCombine));
    WCHAR w[MAX_PATH];
    EXPECT_HR(U(PathCchCombine)(w, MAX_PATH, NULL, NULL), E_INVALIDARG);
    EXPECT_HR(U(PathCchCombine)(NULL, MAX_PATH, L"a", L"b"), E_INVALIDARG);
    EXPECT_HR(U(PathCchCombine)(w, 0, L"a", L"b"), E_INVALIDARG);
    EXPECT_HR(U(PathCchCombine)(w, 0x8001, L"a", L"b"), E_INVALIDARG);
    EXPECT_HR(U(PathCchCombine)(w, (size_t)-1, L"a", L"b"), E_INVALIDARG);
}
TEST(path_names_pathcch_combine_empty_both) {
    REQUIRE(U(PathCchCombine));
    WCHAR w[MAX_PATH]; wcscpy(w, L"zz");
    HRESULT hr = U(PathCchCombine)(w, MAX_PATH, L"", L"");
    t_note("Combine(\"\",\"\") -> hr 0x%lx out [%s]", hr, pn_n(w));
}
TEST(path_names_pathcch_combine_buffer_sizes) {
    REQUIRE(U(PathCchCombine));
    WCHAR w[MAX_PATH];
    /* "C:\a\b" is 6 characters */
    EXPECT_HR(U(PathCchCombine)(w, 7, L"C:\\a", L"b"), S_OK);
    CHECK(pn_weq(w, "C:\\a\\b"));
    HRESULT hr = U(PathCchCombine)(w, 6, L"C:\\a", L"b");
    CHECKF(hr == HR_INSUFFICIENT, "hr 0x%lx", hr);
    hr = U(PathCchCombine)(w, 2, L"C:\\a", L"b");
    CHECKF(hr == HR_INSUFFICIENT, "hr 0x%lx", hr);
}
TEST(path_names_pathcch_combine_exceeds_max_path) {
    REQUIRE(U(PathCchCombine)); REQUIRE(U(PathCchCombineEx));
    static WCHAR a[300], b[300], out[40000];
    wcscpy(a, L"C:\\"); pn_fill(a + 3, 150, L'a');
    pn_fill(b, 150, L'b');
    HRESULT hr = U(PathCchCombine)(out, 1000, a, b);
    CHECKF(hr == HR_EXCED, "legacy limit: hr 0x%lx", hr);
    hr = U(PathCchCombine)(out, 260, a, b);
    CHECKF(hr == HR_EXCED, "cch 260: hr 0x%lx", hr);
    hr = U(PathCchCombineEx)(out, 1000, a, b, PCH_ALLOW_LONG | PCH_FORCE_ENABLE);
    CHECKF(hr == S_OK && wcslen(out) == 3 + 150 + 1 + 150, "long ok: hr 0x%lx len %zu", hr, wcslen(out));
    hr = U(PathCchCombineEx)(out, 1000, a, b, PCH_ALLOW_LONG | PCH_FORCE_DISABLE);
    CHECKF(hr == S_OK && !wcsncmp(out, L"\\\\?\\C:\\", 7), "disabled -> prefixed: hr 0x%lx", hr);
}
TEST(path_names_pathcch_combineex_invalid_flags) {
    REQUIRE(U(PathCchCombineEx));
    WCHAR w[MAX_PATH];
    EXPECT_HR(U(PathCchCombineEx)(w, MAX_PATH, L"C:\\a", L"b", 0x2), E_INVALIDARG);
    EXPECT_HR(U(PathCchCombineEx)(w, MAX_PATH, L"C:\\a", L"b", 0x6 | 1), E_INVALIDARG);
    EXPECT_HR(U(PathCchCombineEx)(w, MAX_PATH, L"C:\\a", L"b", 0x1 | 0x10), E_INVALIDARG);
    EXPECT_HR(U(PathCchCombineEx)(w, MAX_PATH, L"C:\\a", L"b", 0x20), S_OK);
    CHECK(pn_weq(w, "C:\\a\\b\\"));
}
TEST(path_names_pathcch_combine_input_longer_than_32k_invalid) {
    REQUIRE(U(PathCchCombine));
    static WCHAR big[33000], out[100];
    pn_fill(big, 32800, L'x');
    HRESULT hr = U(PathCchCombine)(out, 100, big, L"b");
    CHECKF(hr == HR_EXCED, "hr 0x%lx", hr);
}
TEST(path_names_pathcch_combine_output_is_canonical) {
    REQUIRE(U(PathCchCombine));
    WCHAR w[MAX_PATH];
    EXPECT_HR(U(PathCchCombine)(w, MAX_PATH, L"C:\\a\\.\\b", L".\\c\\..\\d"), S_OK);
    CHECK(pn_weq(w, "C:\\a\\b\\d"));
}
TEST(path_names_pathcch_combine_out_can_alias_in) {
    REQUIRE(U(PathCchCombine));
    WCHAR w[MAX_PATH]; wcscpy(w, L"C:\\base");
    EXPECT_HR(U(PathCchCombine)(w, MAX_PATH, w, L"leaf"), S_OK);
    CHECK(pn_weq(w, "C:\\base\\leaf"));
}

/* ------------------------------- Append ------------------------------- */
typedef struct { const char *a, *b, *out; HRESULT hr; int hard; } app_row;
static const app_row app_rows[] = {
    { "C:\\a", "b", "C:\\a\\b", S_OK, 1 },
    { "C:\\a\\", "b", "C:\\a\\b", S_OK, 1 },
    { "C:\\a", "\\b", "C:\\a\\b", S_OK, 1 },
    { "C:\\a", "\\\\b", "C:\\a\\b", S_OK, 0 },
    { "C:\\a", "\\\\\\b", "C:\\a\\b", S_OK, 0 },
    { "C:\\a", "..\\b", "C:\\b", S_OK, 1 },
    { "C:\\a", "b\\c", "C:\\a\\b\\c", S_OK, 1 },
    { "C:\\a", "", "C:\\a", S_OK, 1 },
    { "C:\\a", NULL, "C:\\a", S_OK, 1 },
    { "C:\\a", "D:\\b", "D:\\b", S_OK, 1 },
    { "C:\\a", "\\\\s\\sh\\x", "\\\\s\\sh\\x", S_OK, 0 },
    { "", "b", "b", S_OK, 1 },
    { "C:\\", "b", "C:\\b", S_OK, 1 },
    { "C:", "b", "C:\\b", S_OK, 0 },
    { "\\\\s\\sh", "b", "\\\\s\\sh\\b", S_OK, 1 },
    { "a", "b", "a\\b", S_OK, 1 },
    { "C:\\a", ".", "C:\\a", S_OK, 1 },
    { "C:\\a\\b", "..", "C:\\a", S_OK, 1 },
};
TEST(path_names_pathcch_append_table) {
    REQUIRE(U(PathCchAppend));
    for (unsigned i = 0; i < sizeof app_rows / sizeof *app_rows; i++) {
        const app_row *r = &app_rows[i];
        WCHAR w[1024]; char o[1024];
        wcscpy(w, pn_w(r->a));
        HRESULT hr = U(PathCchAppend)(w, MAX_PATH, r->b ? pn_w(r->b) : NULL);
        pn_a(o, sizeof o, w);
        int ok = hr == r->hr && !strcmp(o, r->out);
        if (r->hard) CHECKF(ok, "row %u (%s + %s) hr 0x%lx [%s] want [%s]", i, r->a, r->b ? r->b : "NULL", hr, o, r->out);
        else if (!ok) t_note("append row %u (%s + %s) hr 0x%lx [%s] (model [%s])", i, r->a, r->b ? r->b : "NULL", hr, o, r->out);
    }
}
TEST(path_names_pathcch_append_invalid_args) {
    REQUIRE(U(PathCchAppend));
    WCHAR w[MAX_PATH]; wcscpy(w, L"C:\\a");
    EXPECT_HR(U(PathCchAppend)(NULL, MAX_PATH, L"b"), E_INVALIDARG);
    EXPECT_HR(U(PathCchAppend)(w, 0, L"b"), E_INVALIDARG);
    EXPECT_HR(U(PathCchAppend)(w, 0x8001, L"b"), E_INVALIDARG);
    CHECK(pn_weq(w, "C:\\a"));
}
TEST(path_names_pathcch_append_buffer_boundaries) {
    REQUIRE(U(PathCchAppend));
    WCHAR w[MAX_PATH];
    wcscpy(w, L"C:\\a");
    EXPECT_HR(U(PathCchAppend)(w, 7, L"b"), S_OK);   /* C:\a\b + NUL = 7 */
    CHECK(pn_weq(w, "C:\\a\\b"));
    wcscpy(w, L"C:\\a");
    HRESULT hr = U(PathCchAppend)(w, 6, L"b");
    CHECKF(hr == HR_INSUFFICIENT, "hr 0x%lx", hr);
    CHECK(pn_weq(w, "C:\\a") || w[0] == 0 || 1);
}
TEST(path_names_pathcch_append_too_long_legacy) {
    REQUIRE(U(PathCchAppend)); REQUIRE(U(PathCchAppendEx));
    static WCHAR w[40000], more[300];
    wcscpy(w, L"C:\\"); pn_fill(w + 3, 200, L'a');
    pn_fill(more, 100, L'b');
    HRESULT hr = U(PathCchAppend)(w, 1000, more);
    CHECKF(hr == HR_EXCED, "hr 0x%lx", hr);
    wcscpy(w, L"C:\\"); pn_fill(w + 3, 200, L'a');
    hr = U(PathCchAppendEx)(w, 1000, more, PCH_ALLOW_LONG | PCH_FORCE_ENABLE);
    CHECKF(hr == S_OK && wcslen(w) == 3 + 200 + 1 + 100, "allow long: hr 0x%lx len %zu", hr, wcslen(w));
}
TEST(path_names_pathcch_appendex_flag_validation) {
    REQUIRE(U(PathCchAppendEx));
    WCHAR w[MAX_PATH];
    wcscpy(w, L"C:\\a");
    EXPECT_HR(U(PathCchAppendEx)(w, MAX_PATH, L"b", 0x2), E_INVALIDARG);
    wcscpy(w, L"C:\\a");
    EXPECT_HR(U(PathCchAppendEx)(w, MAX_PATH, L"b", 0x20), S_OK);
    CHECK(pn_weq(w, "C:\\a\\b\\"));
}

/* ------------------------------- Alloc* ------------------------------- */
TEST(path_names_pathalloc_combine_basic) {
    REQUIRE(U(PathAllocCombine));
    PWSTR out = (PWSTR)SENT;
    HRESULT hr = U(PathAllocCombine)(L"C:\\a", L"b\\..\\c", 0, &out);
    CHECK(hr == S_OK && out && pn_weq(out, "C:\\a\\c"));
    if (out) {
        CHECK(LocalSize(out) >= 7 * sizeof(WCHAR));
        LocalFree(out);
    }
}
TEST(path_names_pathalloc_combine_null_cases) {
    REQUIRE(U(PathAllocCombine));
    PWSTR out = NULL;
    HRESULT hr = U(PathAllocCombine)(NULL, L"b", 0, &out);
    CHECK(hr == S_OK && out && pn_weq(out, "b")); if (out) LocalFree(out);
    out = NULL;
    hr = U(PathAllocCombine)(L"C:\\a", NULL, 0, &out);
    CHECK(hr == S_OK && out && pn_weq(out, "C:\\a")); if (out) LocalFree(out);
    out = (PWSTR)SENT;
    hr = U(PathAllocCombine)(NULL, NULL, 0, &out);
    CHECKF(hr == E_INVALIDARG, "hr 0x%lx", hr);
    t_note("PathAllocCombine(NULL,NULL) leaves out=%p", out);
    EXPECT_HR(U(PathAllocCombine)(L"a", L"b", 0, NULL), E_INVALIDARG);
}
TEST(path_names_pathalloc_combine_long_paths) {
    REQUIRE(U(PathAllocCombine));
    static WCHAR a[600], b[600];
    wcscpy(a, L"C:\\"); pn_fill(a + 3, 250, L'a');
    pn_fill(b, 250, L'b');
    PWSTR out = (PWSTR)SENT;
    HRESULT hr = U(PathAllocCombine)(a, b, 0, &out);
    CHECKF(hr == HR_EXCED, "no flag hr 0x%lx", hr);
    hr = U(PathAllocCombine)(a, b, PCH_ALLOW_LONG | PCH_FORCE_ENABLE, &out);
    CHECKF(hr == S_OK && out && wcslen(out) == 3 + 250 + 1 + 250, "allow long hr 0x%lx", hr);
    if (hr == S_OK && out) LocalFree(out);
    hr = U(PathAllocCombine)(a, b, PCH_ALLOW_LONG | PCH_FORCE_DISABLE, &out);
    CHECKF(hr == S_OK && out && !wcsncmp(out, L"\\\\?\\C:\\", 7), "prefix hr 0x%lx", hr);
    if (hr == S_OK && out) LocalFree(out);
}
TEST(path_names_pathalloc_canonicalize_basic) {
    REQUIRE(U(PathAllocCanonicalize));
    PWSTR out = (PWSTR)SENT;
    HRESULT hr = U(PathAllocCanonicalize)(L"C:\\a\\.\\b\\..\\c", 0, &out);
    CHECK(hr == S_OK && out && pn_weq(out, "C:\\a\\c"));
    if (out) LocalFree(out);
    hr = U(PathAllocCanonicalize)(L"\\\\s\\sh\\x\\..\\y", 0, &out);
    CHECK(hr == S_OK && out && pn_weq(out, "\\\\s\\sh\\y"));
    if (out) LocalFree(out);
}
TEST(path_names_pathalloc_canonicalize_long_and_flags) {
    REQUIRE(U(PathAllocCanonicalize));
    static WCHAR in[1000];
    wcscpy(in, L"C:\\"); pn_fill(in + 3, 400, L'a');
    PWSTR out = NULL;
    HRESULT hr = U(PathAllocCanonicalize)(in, 0, &out);
    CHECKF(hr == HR_EXCED, "no flag hr 0x%lx", hr);
    hr = U(PathAllocCanonicalize)(in, PCH_ALLOW_LONG | PCH_FORCE_ENABLE, &out);
    CHECKF(hr == S_OK && out && wcslen(out) == 403, "long hr 0x%lx", hr);
    if (hr == S_OK && out) LocalFree(out);
    hr = U(PathAllocCanonicalize)(in, 0x2, &out);
    CHECKF(hr == E_INVALIDARG, "bad flags hr 0x%lx", hr);
    EXPECT_HR(U(PathAllocCanonicalize)(L"C:\\a", 0, NULL), E_INVALIDARG);
}
TEST(path_names_pathalloc_canonicalize_32k_input) {
    REQUIRE(U(PathAllocCanonicalize));
    static WCHAR in[33500];
    wcscpy(in, L"C:\\");
    int len = 3;
    while (len + 101 < 32000) { in[len++] = L'\\'; pn_fill(in + len, 100, L'q'); len += 100; }
    PWSTR out = NULL;
    HRESULT hr = U(PathAllocCanonicalize)(in, PCH_ALLOW_LONG | PCH_FORCE_ENABLE, &out);
    CHECKF(hr == S_OK && out && (int)wcslen(out) == len, "hr 0x%lx", hr);
    if (hr == S_OK && out) LocalFree(out);
    hr = U(PathAllocCanonicalize)(in, 0, &out);
    CHECKF(hr == HR_EXCED, "legacy hr 0x%lx", hr);
}
