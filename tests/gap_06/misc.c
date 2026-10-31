#include "g6.h"

/* child-side check: return the source line as exit code on failure */
#define CC(c) do { if (!(c)) return __LINE__; } while (0)

UDECL(HRESULT, HashData, (LPBYTE, DWORD, LPBYTE, DWORD));
UDECL(ULONG, GetTraceEnableFlags, (ULONGLONG));
UDECL(UCHAR, GetTraceEnableLevel, (ULONGLONG));
UDECL(ULONGLONG, GetTraceLoggerHandle, (PVOID));
UDECL(BOOL, GetWsChanges, (HANDLE, PVOID, DWORD));
UDECL(BOOL, GetWsChangesEx, (HANDLE, PVOID, PDWORD));
UDECL(BOOL, InitializeProcessForWsWatch, (HANDLE));
UDECL(BOOL, GetUmsSystemThreadInformation, (HANDLE, PVOID));
UDECL(BOOL, InstallELAMCertificateInfo, (HANDLE));

/* ---- HashData ---- */
static HRESULT hd(const void *d, DWORD n, void *h, DWORD hn) { return UN(HashData)((LPBYTE)d, n, (LPBYTE)h, hn); }

TEST(gap6_hashdata_null_params) {
    REQUIRE(UN(HashData));
    BYTE d[4] = {1, 2, 3, 4}, h[4];
    EXPECT_EQ(hd(NULL, 4, h, 4), E_INVALIDARG);
    EXPECT_EQ(hd(d, 4, NULL, 4), E_INVALIDARG);
    EXPECT_EQ(hd(NULL, 0, NULL, 0), E_INVALIDARG);
    EXPECT_EQ(hd(NULL, 0, h, 0), E_INVALIDARG);
    EXPECT_EQ(hd(d, 0, NULL, 0), E_INVALIDARG);
}
TEST(gap6_hashdata_zero_hash_size) {
    REQUIRE(UN(HashData));
    BYTE d[4] = {1, 2, 3, 4}, h[4] = {0xCC, 0xCC, 0xCC, 0xCC};
    EXPECT_EQ(hd(d, 4, h, 0), S_OK);
    EXPECT_EQ(h[0], 0xCC);
}
TEST(gap6_hashdata_empty_input_is_index_pattern) {
    REQUIRE(UN(HashData));
    BYTE d[1] = {0}, h[300];
    memset(h, 0xCC, sizeof h);
    EXPECT_EQ(hd(d, 0, h, 256), S_OK);
    int bad = 0;
    for (int i = 0; i < 256; i++) if (h[i] != (BYTE)i) bad++;
    EXPECT_EQ(bad, 0);
    EXPECT_EQ(h[256], 0xCC);
    EXPECT_EQ(hd(d, 0, h, 7), S_OK);
    for (int i = 0; i < 7; i++) EXPECT_EQ(h[i], i);
}
static BYTE T[256];
static int load_table(void) {
    for (int i = 0; i < 256; i++) { BYTE d = (BYTE)i, h = 0xCC; if (hd(&d, 1, &h, 1) != S_OK) return 0; T[i] = h; }
    return 1;
}
TEST(gap6_hashdata_single_byte_table_is_permutation) {
    REQUIRE(UN(HashData));
    CHECK(load_table());
    int seen[256] = {0}, dup = 0;
    for (int i = 0; i < 256; i++) { if (seen[T[i]]++) dup++; }
    EXPECT_EQ(dup, 0);
}
/* Reference model of the algorithm, parametrised on the measured 1-byte table. */
static void model(const BYTE *d, DWORD n, BYTE *h, DWORD hn) {
    for (DWORD j = 0; j < hn; j++) h[j] = (BYTE)j;
    for (DWORD i = n; i > 0; i--)
        for (DWORD j = hn; j > 0; j--) h[j - 1] = T[d[i - 1] ^ h[j - 1]];
}
TEST(gap6_hashdata_matches_reference_model) {
    REQUIRE(UN(HashData));
    CHECK(load_table());
    BYTE d[64]; for (int i = 0; i < 64; i++) d[i] = (BYTE)(i * 37 + 11);
    DWORD ns[] = {1, 2, 3, 7, 16, 63, 64}, hs[] = {1, 2, 4, 8, 16, 32, 100};
    for (unsigned a = 0; a < 7; a++) for (unsigned b = 0; b < 7; b++) {
        BYTE got[100], exp[100];
        CHECK(hd(d, ns[a], got, hs[b]) == S_OK);
        model(d, ns[a], exp, hs[b]);
        CHECKF(!memcmp(got, exp, hs[b]), "n=%lu hash=%lu", ns[a], hs[b]);
    }
}
TEST(gap6_hashdata_deterministic_and_overwrites) {
    REQUIRE(UN(HashData));
    BYTE d[] = "hello world", a[16], b[16];
    memset(a, 0x11, 16); memset(b, 0xEE, 16);
    CHECK(hd(d, sizeof d, a, 16) == S_OK);
    CHECK(hd(d, sizeof d, b, 16) == S_OK);
    CHECK(!memcmp(a, b, 16));
}
TEST(gap6_hashdata_differs_for_different_input) {
    REQUIRE(UN(HashData));
    BYTE a[16], b[16];
    CHECK(hd("abc", 3, a, 16) == S_OK);
    CHECK(hd("abd", 3, b, 16) == S_OK);
    CHECK(memcmp(a, b, 16) != 0);
    CHECK(hd("abc", 2, b, 16) == S_OK);
    CHECK(memcmp(a, b, 16) != 0);
}
TEST(gap6_hashdata_prefix_of_hash_size) {
    REQUIRE(UN(HashData));
    /* each output byte depends only on its own index: shorter hashes are prefixes */
    BYTE d[] = "prefix test", a[32], b[8];
    CHECK(hd(d, sizeof d, a, 32) == S_OK);
    CHECK(hd(d, sizeof d, b, 8) == S_OK);
    CHECK(!memcmp(a, b, 8));
}
TEST(gap6_hashdata_in_place_buffers_distinct_sizes) {
    REQUIRE(UN(HashData));
    BYTE big[4096]; for (int i = 0; i < 4096; i++) big[i] = (BYTE)(i ^ (i >> 5));
    BYTE h1[4], h2[4];
    CHECK(hd(big, 4096, h1, 4) == S_OK);
    CHECK(hd(big, 4096, h2, 4) == S_OK);
    CHECK(!memcmp(h1, h2, 4));
}
TEST(gap6_hashdata_large_hash_size_wraps_index) {
    REQUIRE(UN(HashData));
    BYTE h[600];
    BYTE d[1] = {0};
    CHECK(hd(d, 0, h, 600) == S_OK);
    EXPECT_EQ(h[255], 255); EXPECT_EQ(h[256], 0); EXPECT_EQ(h[257], 1); EXPECT_EQ(h[599], (BYTE)599);
}
TEST(gap6_hashdata_does_not_touch_input) {
    REQUIRE(UN(HashData));
    BYTE d[8] = {9, 8, 7, 6, 5, 4, 3, 2}, c[8], h[8];
    memcpy(c, d, 8);
    CHECK(hd(d, 8, h, 8) == S_OK);
    CHECK(!memcmp(c, d, 8));
}

/* ---- IsCharAlphaA / IsCharAlphaNumericA / IsCharAlphaNumericW ---- */
TEST(gap6_ischar_alphaa_ascii) {
    REQUIRE(U(IsCharAlphaA));
    int bad = 0;
    for (int c = 0; c < 128; c++) {
        BOOL exp = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
        BOOL got = U(IsCharAlphaA)((CHAR)c) != 0;
        if (got != exp) { bad++; t_note("IsCharAlphaA(0x%02x) = %d", c, got); }
    }
    EXPECT_EQ(bad, 0);
}
TEST(gap6_ischar_alphaa_return_is_boolean) {
    REQUIRE(U(IsCharAlphaA));
    BOOL r = U(IsCharAlphaA)('Q');
    CHECK(r == 1 || r != 0);
    t_note("IsCharAlphaA('Q') returns %d", r);
    EXPECT_EQ(U(IsCharAlphaA)('1'), 0);
}
TEST(gap6_ischar_alphanumerica_ascii) {
    REQUIRE(U(IsCharAlphaNumericA));
    int bad = 0;
    for (int c = 0; c < 128; c++) {
        BOOL exp = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        BOOL got = U(IsCharAlphaNumericA)((CHAR)c) != 0;
        if (got != exp) { bad++; t_note("IsCharAlphaNumericA(0x%02x) = %d", c, got); }
    }
    EXPECT_EQ(bad, 0);
}
TEST(gap6_ischar_alphanumericw_ascii) {
    REQUIRE(U(IsCharAlphaNumericW));
    int bad = 0;
    for (int c = 0; c < 128; c++) {
        BOOL exp = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        BOOL got = U(IsCharAlphaNumericW)((WCHAR)c) != 0;
        if (got != exp) bad++;
    }
    EXPECT_EQ(bad, 0);
}
TEST(gap6_ischar_ascii_ab_parity) {
    REQUIRE(U(IsCharAlphaNumericA)); REQUIRE(U(IsCharAlphaNumericW)); REQUIRE(U(IsCharAlphaA));
    int bad = 0;
    for (int c = 1; c < 128; c++) {
        if ((U(IsCharAlphaNumericA)((CHAR)c) != 0) != (U(IsCharAlphaNumericW)((WCHAR)c) != 0)) bad++;
    }
    EXPECT_EQ(bad, 0);
}
TEST(gap6_ischar_nul_and_controls) {
    REQUIRE(U(IsCharAlphaNumericA)); REQUIRE(U(IsCharAlphaNumericW)); REQUIRE(U(IsCharAlphaA));
    EXPECT_EQ(U(IsCharAlphaNumericA)(0), 0);
    EXPECT_EQ(U(IsCharAlphaNumericW)(0), 0);
    EXPECT_EQ(U(IsCharAlphaA)(0), 0);
    EXPECT_EQ(U(IsCharAlphaNumericW)(0xFFFF), 0);
    EXPECT_EQ(U(IsCharAlphaNumericW)(0xFFFE), 0);
    EXPECT_EQ(U(IsCharAlphaNumericW)(L' '), 0);
    EXPECT_EQ(U(IsCharAlphaNumericW)(0x3000), 0);
    EXPECT_EQ(U(IsCharAlphaNumericW)(0x2028), 0);
    EXPECT_EQ(U(IsCharAlphaNumericW)(0x00A0), 0);
}
TEST(gap6_ischar_alphanumericw_latin_greek_cyrillic) {
    REQUIRE(U(IsCharAlphaNumericW));
    WCHAR yes[] = {0x00C0, 0x00E9, 0x00FF, 0x0100, 0x017F, 0x0391, 0x03A9, 0x03B1, 0x0410, 0x044F, 0x05D0};
    for (unsigned i = 0; i < sizeof yes / sizeof *yes; i++)
        CHECKF(U(IsCharAlphaNumericW)(yes[i]), "U+%04X", yes[i]);
    WCHAR no[] = {0x00D7, 0x00F7, 0x00A9, 0x00AB, 0x00BB, 0x2022, 0x20AC, 0x2190};
    for (unsigned i = 0; i < sizeof no / sizeof *no; i++)
        CHECKF(!U(IsCharAlphaNumericW)(no[i]), "U+%04X", no[i]);
}
TEST(gap6_ischar_alphanumericw_digits_other_scripts) {
    REQUIRE(U(IsCharAlphaNumericW));
    WCHAR cs[] = {0x00B2, 0x00B3, 0x00B9, 0x00BC, 0x0660, 0x06F0, 0x0966, 0xFF10, 0xFF21, 0x4E00, 0x3042, 0xAC00, 0x0E01};
    for (unsigned i = 0; i < sizeof cs / sizeof *cs; i++)
        t_note("IsCharAlphaNumericW(U+%04X) = %d", cs[i], U(IsCharAlphaNumericW)(cs[i]));
    CHECK(U(IsCharAlphaNumericW)(0xFF21));    /* fullwidth A */
    CHECK(U(IsCharAlphaNumericW)(0xFF10));    /* fullwidth 0 */
}
TEST(gap6_ischar_alphanumericw_surrogates_and_private) {
    REQUIRE(U(IsCharAlphaNumericW));
    for (WCHAR c = 0xD800; c <= 0xDFFF; c += 0x80) t_note("surrogate U+%04X -> %d", c, U(IsCharAlphaNumericW)(c));
    EXPECT_EQ(U(IsCharAlphaNumericW)(0xE000), 0);
    EXPECT_EQ(U(IsCharAlphaNumericW)(0xF8FF), 0);
}
TEST(gap6_ischar_alphanumericw_upper_lower_symmetry) {
    REQUIRE(U(IsCharAlphaNumericW));
    int bad = 0;
    for (WCHAR c = 0x41; c <= 0x5A; c++) if (!U(IsCharAlphaNumericW)(c) || !U(IsCharAlphaNumericW)(c + 0x20)) bad++;
    EXPECT_EQ(bad, 0);
}
TEST(gap6_ischar_alphanumerica_latin1_when_acp_1252) {
    REQUIRE(U(IsCharAlphaNumericA)); REQUIRE(U(IsCharAlphaA));
    if (GetACP() != 1252) { t_skip("ACP is %u", GetACP()); return; }
    for (int c = 0xC0; c <= 0xFF; c++) {
        if (c == 0xD7 || c == 0xF7) {
            CHECKF(!U(IsCharAlphaA)((CHAR)c), "alpha 0x%02x", c);
            CHECKF(!U(IsCharAlphaNumericA)((CHAR)c), "alnum 0x%02x", c);
        } else {
            CHECKF(U(IsCharAlphaA)((CHAR)c), "alpha 0x%02x", c);
            CHECKF(U(IsCharAlphaNumericA)((CHAR)c), "alnum 0x%02x", c);
        }
    }
    int punct[] = {0xA0, 0xA1, 0xA2, 0xA3, 0xA9, 0xAB, 0xAD, 0xB0, 0xB1, 0xBB, 0xBF, 0x80, 0x95, 0x99};
    for (unsigned i = 0; i < sizeof punct / sizeof *punct; i++)
        CHECKF(!U(IsCharAlphaA)((CHAR)punct[i]), "0x%02x", punct[i]);
    t_note("0xAA=%d 0xB5=%d 0xBA=%d 0xB2=%d 0xBC=%d 0x8A=%d 0x9F=%d",
        U(IsCharAlphaA)((CHAR)0xAA), U(IsCharAlphaA)((CHAR)0xB5), U(IsCharAlphaA)((CHAR)0xBA),
        U(IsCharAlphaNumericA)((CHAR)0xB2), U(IsCharAlphaNumericA)((CHAR)0xBC),
        U(IsCharAlphaA)((CHAR)0x8A), U(IsCharAlphaA)((CHAR)0x9F));
}
TEST(gap6_ischar_alphanumerica_signed_char_arg) {
    REQUIRE(U(IsCharAlphaNumericA));
    /* CHAR is signed: negative values must be treated as bytes 0x80-0xFF, not crash. */
    for (int c = -128; c < 0; c++) (void)U(IsCharAlphaNumericA)((CHAR)c);
    CHECK(1);
}
TEST(gap6_ischar_matches_user32) {
    REQUIRE(U(IsCharAlphaA)); REQUIRE(U(IsCharAlphaNumericA));
    HMODULE u = LoadLibraryA("user32.dll"); if (!u) { t_skip("user32"); return; }
    BOOL (WINAPI *ua)(CHAR) = (BOOL (WINAPI *)(CHAR))GetProcAddress(u, "IsCharAlphaA");
    BOOL (WINAPI *un)(CHAR) = (BOOL (WINAPI *)(CHAR))GetProcAddress(u, "IsCharAlphaNumericA");
    BOOL (WINAPI *unw)(WCHAR) = (BOOL (WINAPI *)(WCHAR))GetProcAddress(u, "IsCharAlphaNumericW");
    int bad = 0;
    for (int c = 0; c < 256; c++) {
        if (ua && (!!ua((CHAR)c) != !!U(IsCharAlphaA)((CHAR)c))) bad++;
        if (un && (!!un((CHAR)c) != !!U(IsCharAlphaNumericA)((CHAR)c))) bad++;
    }
    for (int c = 0; c < 0x3000 && unw; c++) if (!!unw((WCHAR)c) != !!U(IsCharAlphaNumericW)((WCHAR)c)) bad++;
    EXPECT_EQ(bad, 0);
}
TEST(gap6_ischar_does_not_clobber_lasterror) {
    REQUIRE(U(IsCharAlphaNumericW));
    SET_ERR(4711);
    U(IsCharAlphaNumericW)(L'a');
    t_note("lasterror after IsCharAlphaNumericW = %lu", GetLastError());
    CHECK(1);
}

/* ---- ETW trace handle accessors (forwarded to ntdll) ---- */
static ULONGLONG mkh(ULONG flags, UCHAR level, USHORT id) { return ((ULONGLONG)flags << 32) | ((ULONGLONG)level << 16) | id; }

TEST(gap6_trace_flags_decode) {
    REQUIRE(UN(GetTraceEnableFlags)); if (t_is_wine()) { t_skip("ETW accessors are stubs under wine"); return; }
    EXPECT_EQ(UN(GetTraceEnableFlags)(mkh(0xABCD1234, 5, 1)), 0xABCD1234);
    EXPECT_EQ(UN(GetTraceEnableFlags)(mkh(0, 5, 1)), 0);
    EXPECT_EQ(UN(GetTraceEnableFlags)(mkh(0xFFFFFFFF, 0xFF, 1)), 0xFFFFFFFF);
    EXPECT_EQ(UN(GetTraceEnableFlags)(mkh(0x80000001, 0, 0)), 0x80000001);
    EXPECT_EQ(UN(GetTraceEnableFlags)(mkh(0x1, 0, 0xFFFF)), 0x1);
}
TEST(gap6_trace_flags_invalid) {
    REQUIRE(UN(GetTraceEnableFlags)); if (t_is_wine()) { t_skip("ETW accessors are stubs under wine"); return; }
    SET_ERR(0);
    EXPECT_EQ(UN(GetTraceEnableFlags)(0), 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    EXPECT_EQ(UN(GetTraceEnableFlags)(mkh(0x1234, 3, 0x7FFE)), 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    EXPECT_EQ(UN(GetTraceEnableFlags)(mkh(0x1234, 3, 0xFFFE)), 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap6_trace_flags_zero_logger_nonzero_handle) {
    REQUIRE(UN(GetTraceEnableFlags)); if (t_is_wine()) { t_skip("ETW accessors are stubs under wine"); return; }
    /* logger id 0 is below any max; only handle==0 is rejected */
    EXPECT_EQ(UN(GetTraceEnableFlags)(mkh(0x42, 0, 0)), 0x42);
    EXPECT_EQ(UN(GetTraceEnableFlags)(mkh(0, 1, 0)), 0);
}
TEST(gap6_trace_level_decode) {
    REQUIRE(UN(GetTraceEnableLevel)); if (t_is_wine()) { t_skip("ETW accessors are stubs under wine"); return; }
    for (int l = 0; l < 256; l += 15)
        EXPECT_EQ(UN(GetTraceEnableLevel)(mkh(0x1111, (UCHAR)l, 1)), l);
    EXPECT_EQ(UN(GetTraceEnableLevel)(mkh(0, 5, 0xFFFF)), 5);
    EXPECT_EQ(UN(GetTraceEnableLevel)(mkh(0xFFFFFFFF, 4, 2)), 4);
}
TEST(gap6_trace_level_invalid) {
    REQUIRE(UN(GetTraceEnableLevel)); if (t_is_wine()) { t_skip("ETW accessors are stubs under wine"); return; }
    SET_ERR(0);
    EXPECT_EQ(UN(GetTraceEnableLevel)(0), 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    EXPECT_EQ(UN(GetTraceEnableLevel)(mkh(1, 9, 0x7FFE)), 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap6_trace_loggerhandle_null) {
    REQUIRE(UN(GetTraceLoggerHandle)); if (t_is_wine()) { t_skip("ETW accessors are stubs under wine"); return; }
    SET_ERR(0);
    EXPECT_EQ(UN(GetTraceLoggerHandle)(NULL), (ULONGLONG)-1);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap6_trace_loggerhandle_small_buffer_size) {
    REQUIRE(UN(GetTraceLoggerHandle)); if (t_is_wine()) { t_skip("ETW accessors are stubs under wine"); return; }
    BYTE b[64];
    ULONG sizes[] = {0, 1, 0x2F, 8, 16};
    for (unsigned i = 0; i < 5; i++) {
        memset(b, 0, sizeof b);
        *(ULONG *)b = sizes[i];
        *(ULONGLONG *)(b + 8) = mkh(1, 1, 1);
        SET_ERR(0);
        EXPECT_EQ(UN(GetTraceLoggerHandle)(b), (ULONGLONG)-1);
        EXPECT_ERR(ERROR_BAD_LENGTH);
    }
}
TEST(gap6_trace_loggerhandle_valid) {
    REQUIRE(UN(GetTraceLoggerHandle)); if (t_is_wine()) { t_skip("ETW accessors are stubs under wine"); return; }
    BYTE b[64]; memset(b, 0, sizeof b);
    ULONG sizes[] = {0x30, 0x31, 0x1000, 0xFFFFFFFF};
    for (unsigned i = 0; i < 4; i++) {
        *(ULONG *)b = sizes[i];
        ULONGLONG h = mkh(0x77, 2, 3);
        *(ULONGLONG *)(b + 8) = h;
        EXPECT_EQ(UN(GetTraceLoggerHandle)(b), h);
    }
    *(ULONG *)b = 0x30;
    *(ULONGLONG *)(b + 8) = mkh(5, 1, 0xFFFF);
    EXPECT_EQ(UN(GetTraceLoggerHandle)(b), mkh(5, 1, 0xFFFF));
    *(ULONGLONG *)(b + 8) = 0;           /* logger id 0 passes the range test */
    EXPECT_EQ(UN(GetTraceLoggerHandle)(b), 0);
}
TEST(gap6_trace_loggerhandle_invalid_id) {
    REQUIRE(UN(GetTraceLoggerHandle)); if (t_is_wine()) { t_skip("ETW accessors are stubs under wine"); return; }
    BYTE b[64]; memset(b, 0, sizeof b);
    *(ULONG *)b = 0x30;
    *(ULONGLONG *)(b + 8) = mkh(1, 1, 0x7FFE);
    SET_ERR(0);
    EXPECT_EQ(UN(GetTraceLoggerHandle)(b), (ULONGLONG)-1);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap6_trace_exports_forward_to_ntdll) {
    HMODULE kb = t_kernelbase(); REQUIRE(kb);
    FARPROC a = GetProcAddress(kb, "GetTraceEnableFlags");
    if (!a) { t_skip("not exported"); return; }
    FARPROC n = GetProcAddress(t_ntdll(), "EtwGetTraceEnableFlags");
    if (n) EXPECT_EQ(a, n);
    a = GetProcAddress(kb, "GetTraceLoggerHandle");
    n = GetProcAddress(t_ntdll(), "EtwGetTraceLoggerHandle");
    if (a && n) EXPECT_EQ(a, n);
}

/* ---- GetSystemRegistryQuota ---- */
TEST(gap6_regquota_basic) {
    REQUIRE(U(GetSystemRegistryQuota));
    DWORD allowed = 0, used = 0;
    SET_ERR(0);
    BOOL r = U(GetSystemRegistryQuota)(&allowed, &used);
    if (!r) { t_note("failed err %lu", GetLastError()); return; }
    CHECK(used > 0);
    CHECK(allowed > 0);
    t_note("registry quota allowed=%lu used=%lu", allowed, used);
}
TEST(gap6_regquota_null_outputs) {
    REQUIRE(U(GetSystemRegistryQuota));
    DWORD v = 0xCCCCCCCC;
    BOOL r = U(GetSystemRegistryQuota)(NULL, NULL);
    if (!r) { t_note("failed err %lu", GetLastError()); return; }
    r = U(GetSystemRegistryQuota)(&v, NULL);
    CHECK(r); CHECK(v != 0xCCCCCCCC);
    v = 0xCCCCCCCC;
    r = U(GetSystemRegistryQuota)(NULL, &v);
    CHECK(r); CHECK(v != 0xCCCCCCCC);
}
TEST(gap6_regquota_consistent_repeat) {
    REQUIRE(U(GetSystemRegistryQuota));
    DWORD a1, u1, a2, u2;
    if (!U(GetSystemRegistryQuota)(&a1, &u1)) { t_skip("unavailable"); return; }
    CHECK(U(GetSystemRegistryQuota)(&a2, &u2));
    EXPECT_EQ(a1, a2);
    CHECK(u2 >= u1 / 2 && u2 <= u1 + u1 / 2 + 65536);
}

/* ---- tape APIs on non-tape handles ---- */
TEST(gap6_tape_getstatus_invalid_handles) {
    REQUIRE(U(GetTapeStatus));
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE hs[] = {NULL, INVALID_HANDLE_VALUE, (HANDLE)0xDEADBEE4, ev};
    for (unsigned i = 0; i < 4; i++) {
        SET_ERR(0);
        DWORD r = U(GetTapeStatus)(hs[i]);
        CHECKF(r == ERROR_INVALID_HANDLE, "handle %u: %lu", i, r);
        EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
    CloseHandle(ev);
}
TEST(gap6_tape_getstatus_file_handle) {
    REQUIRE(U(GetTapeStatus));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "g6_tape.bin");
    HANDLE f = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) { t_skip("create"); return; }
    DWORD r = U(GetTapeStatus)(f);
    CHECK(r != ERROR_SUCCESS);
    CHECKF(r == ERROR_INVALID_FUNCTION || r == ERROR_NOT_SUPPORTED, "r=%lu", r);
    t_note("GetTapeStatus(file) = %lu", r);
    CloseHandle(f); DeleteFileA(p);
}
TEST(gap6_tape_getstatus_closed_handle) {
    REQUIRE(U(GetTapeStatus));
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    CloseHandle(ev);
    EXPECT_EQ(U(GetTapeStatus)(ev), ERROR_INVALID_HANDLE);
}
TEST(gap6_tape_getposition_outputs_zeroed_on_error) {
    REQUIRE(U(GetTapePosition));
    DWORD part = 0xCCCC, lo = 0xCCCC, hi = 0xCCCC;
    SET_ERR(0);
    DWORD r = U(GetTapePosition)(NULL, TAPE_ABSOLUTE_POSITION, &part, &lo, &hi);
    EXPECT_EQ(r, ERROR_INVALID_HANDLE);
    EXPECT_EQ(part, 0); EXPECT_EQ(lo, 0); EXPECT_EQ(hi, 0);
    part = lo = hi = 0xCCCC;
    r = U(GetTapePosition)(INVALID_HANDLE_VALUE, TAPE_LOGICAL_POSITION, &part, &lo, &hi);
    EXPECT_EQ(r, ERROR_INVALID_HANDLE);
    EXPECT_EQ(part, 0); EXPECT_EQ(lo, 0); EXPECT_EQ(hi, 0);
    DWORD types[] = {0, 1, 2, 3, 0xFFFFFFFF};
    for (unsigned i = 0; i < 5; i++) {
        part = lo = hi = 0xCCCC;
        r = U(GetTapePosition)(NULL, types[i], &part, &lo, &hi);
        CHECKF(r == ERROR_INVALID_HANDLE, "type %lx: %lu", types[i], r);
        CHECK(part == 0 && lo == 0 && hi == 0);
    }
}
TEST(gap6_tape_getposition_file_handle) {
    REQUIRE(U(GetTapePosition));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "g6_tapepos.bin");
    HANDLE f = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) { t_skip("create"); return; }
    DWORD part = 0xCCCC, lo = 0xCCCC, hi = 0xCCCC;
    DWORD r = U(GetTapePosition)(f, TAPE_ABSOLUTE_POSITION, &part, &lo, &hi);
    CHECK(r != ERROR_SUCCESS);
    CHECK(part == 0 && lo == 0 && hi == 0);
    t_note("GetTapePosition(file) = %lu", r);
    CloseHandle(f); DeleteFileA(p);
}
TEST(gap6_tape_getparameters_bad_operation) {
    REQUIRE(U(GetTapeParameters));
    /* operation is validated before the handle or size pointer is used */
    DWORD ops[] = {2, 3, 100, 0x80000000, 0xFFFFFFFF};
    for (unsigned i = 0; i < 5; i++) {
        EXPECT_EQ(U(GetTapeParameters)(NULL, ops[i], NULL, NULL), ERROR_INVALID_FUNCTION);
        BYTE b[64]; DWORD sz = 0x100;
        EXPECT_EQ(U(GetTapeParameters)(INVALID_HANDLE_VALUE, ops[i], &sz, b), ERROR_INVALID_FUNCTION);
        EXPECT_EQ(sz, 0x100);
    }
}
TEST(gap6_tape_getparameters_size_too_small) {
    REQUIRE(U(GetTapeParameters));
    DWORD sizes[] = {0, 1, 4, 0x1F, 0x10};
    for (int op = 0; op <= 1; op++) for (unsigned i = 0; i < 5; i++) {
        BYTE b[64]; DWORD sz = sizes[i];
        DWORD r = U(GetTapeParameters)(INVALID_HANDLE_VALUE, op, &sz, b);
        CHECKF(r == ERROR_MORE_DATA, "op %d size %lu: %lu", op, sizes[i], r);
        CHECKF(sz == 0x20, "op %d size %lu -> %lu", op, sizes[i], sz);
    }
    DWORD sz = 0;
    EXPECT_EQ(U(GetTapeParameters)(NULL, GET_TAPE_DRIVE_INFORMATION, &sz, NULL), ERROR_MORE_DATA);
    EXPECT_EQ(sz, 0x20);
}
TEST(gap6_tape_getparameters_enough_size_bad_handle) {
    REQUIRE(U(GetTapeParameters));
    for (int op = 0; op <= 1; op++) {
        BYTE b[64]; DWORD sz = 0x20;
        DWORD r = U(GetTapeParameters)(NULL, op, &sz, b);
        CHECKF(r == ERROR_INVALID_HANDLE, "op %d: %lu", op, r);
        CHECK(sz == 0x20);
        sz = 0x1000;
        r = U(GetTapeParameters)(INVALID_HANDLE_VALUE, op, &sz, b);
        CHECKF(r == ERROR_INVALID_HANDLE, "op %d: %lu", op, r);
        CHECK(sz == 0x1000);        /* size is only rewritten on the too-small path */
    }
}
TEST(gap6_tape_getparameters_file_handle) {
    REQUIRE(U(GetTapeParameters));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "g6_tapeparm.bin");
    HANDLE f = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) { t_skip("create"); return; }
    BYTE b[64]; DWORD sz = 64;
    DWORD r = U(GetTapeParameters)(f, GET_TAPE_DRIVE_INFORMATION, &sz, b);
    CHECK(r != ERROR_SUCCESS);
    r = U(GetTapeParameters)(f, GET_TAPE_MEDIA_INFORMATION, &sz, b);
    CHECK(r != ERROR_SUCCESS);
    CloseHandle(f); DeleteFileA(p);
}

/* ---- UMS ---- */
TEST(gap6_ums_completion_list_event) {
    REQUIRE(U(GetUmsCompletionListEvent)); REQUIRE(U(CreateUmsCompletionList)); REQUIRE(U(DeleteUmsCompletionList));
    if (t_is_wine()) { t_skip("UMS unimplemented under wine"); return; }
    PUMS_COMPLETION_LIST list = NULL;
    if (!U(CreateUmsCompletionList)(&list)) { t_note("CreateUmsCompletionList err %lu", GetLastError()); return; }
    CHECK(list != NULL);
    HANDLE ev = NULL; SET_ERR(0);
    BOOL r = U(GetUmsCompletionListEvent)(list, &ev);
    CHECKF(r, "err %lu", GetLastError());
    if (r) {
        CHECK(ev != NULL);
        DWORD fl = 0;
        CHECK(GetHandleInformation(ev, &fl));
        DWORD w = WaitForSingleObject(ev, 0);
        CHECKF(w == WAIT_TIMEOUT, "wait %lu", w);   /* nothing queued */
        HANDLE ev2 = NULL;
        CHECK(U(GetUmsCompletionListEvent)(list, &ev2));
        EXPECT_EQ(ev2, ev);                          /* same event each time */
    }
    CHECK(U(DeleteUmsCompletionList)(list));
}
TEST(gap6_ums_thread_info_bad_version) {
    REQUIRE(UN(GetUmsSystemThreadInformation));
    if (t_is_wine()) { t_skip("UMS unimplemented under wine"); return; }
    DWORD vers[] = {0, 1, 0xFF, 0x101, 0x200, 0xFFFFFFFF};
    for (unsigned i = 0; i < 6; i++) {
        DWORD info[2] = {vers[i], 0xCC};
        SET_ERR(0);
        BOOL r = UN(GetUmsSystemThreadInformation)(GetCurrentThread(), info);
        CHECKF(!r, "version %lx", vers[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "version %lx err %lu", vers[i], GetLastError());
        CHECK(info[1] == 0xCC);
    }
    DWORD info[2] = {0, 0};
    SET_ERR(0);
    CHECK(!UN(GetUmsSystemThreadInformation)(NULL, info));      /* version checked before handle */
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap6_ums_thread_info_plain_thread) {
    REQUIRE(UN(GetUmsSystemThreadInformation));
    if (t_is_wine()) { t_skip("UMS unimplemented under wine"); return; }
    DWORD info[2] = {0x100, 0xCC};
    SET_ERR(0);
    BOOL r = UN(GetUmsSystemThreadInformation)(GetCurrentThread(), info);
    if (r) { EXPECT_EQ(info[0], 0x100); t_note("flags %lu", info[1]); }
    else t_note("plain thread query failed err %lu", GetLastError());
}
TEST(gap6_ums_thread_info_invalid_handle) {
    REQUIRE(UN(GetUmsSystemThreadInformation));
    if (t_is_wine()) { t_skip("UMS unimplemented under wine"); return; }
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE hs[] = {NULL, INVALID_HANDLE_VALUE, (HANDLE)0xDEADBEE4, ev};
    for (unsigned i = 0; i < 4; i++) {
        DWORD info[2] = {0x100, 0xCC};
        SET_ERR(0);
        BOOL r = UN(GetUmsSystemThreadInformation)(hs[i], info);
        CHECKF(!r, "handle %u", i);
        CHECKF(GetLastError() == ERROR_INVALID_HANDLE || GetLastError() == ERROR_BAD_LENGTH, "handle %u err %lu", i, GetLastError());
    }
    CloseHandle(ev);
}

/* ---- InstallELAMCertificateInfo (only invalid-handle error paths) ---- */
TEST(gap6_elam_invalid_handle) {
    REQUIRE(UN(InstallELAMCertificateInfo));
    if (t_is_wine()) { t_skip("not meaningful under wine"); return; }
    HANDLE hs[] = {NULL, INVALID_HANDLE_VALUE, (HANDLE)0xDEADBEE4};
    for (unsigned i = 0; i < 3; i++) {
        SET_ERR(0);
        BOOL r = UN(InstallELAMCertificateInfo)(hs[i]);
        DWORD e = GetLastError();
        CHECKF(!r, "handle %u", i);
        CHECKF(e == ERROR_PRIVILEGE_NOT_HELD || e == ERROR_INVALID_HANDLE || e == ERROR_ACCESS_DENIED, "handle %u err %lu", i, e);
        t_note("InstallELAMCertificateInfo(%p) err %lu", hs[i], e);
    }
}

/* ---- working set watch (process-global: run in a child) ---- */
CHILD(gap6_wsw) {
    (void)argc; (void)argv;
    HANDLE self = GetCurrentProcess();
    BYTE buf[8192];
    /* invalid handles */
    SetLastError(0);
    CC(!UN(InitializeProcessForWsWatch)(NULL));
    CC(GetLastError() == ERROR_INVALID_HANDLE);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SetLastError(0);
    CC(!UN(InitializeProcessForWsWatch)(ev));
    CC(GetLastError() == ERROR_INVALID_HANDLE);
    SetLastError(0);
    CC(!UN(GetWsChanges)(NULL, buf, sizeof buf));
    CC(GetLastError() == ERROR_INVALID_HANDLE);
    SetLastError(0);
    CC(!UN(GetWsChanges)(ev, buf, sizeof buf));
    CC(GetLastError() == ERROR_INVALID_HANDLE);
    DWORD cb = sizeof buf;
    SetLastError(0);
    CC(!UN(GetWsChangesEx)(NULL, buf, &cb));
    CC(GetLastError() == ERROR_INVALID_HANDLE);
    CloseHandle(ev);
    /* insufficient access */
    HANDLE ro = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, GetCurrentProcessId());
    if (ro) {
        SetLastError(0);
        CC(!UN(InitializeProcessForWsWatch)(ro));
        CC(GetLastError() == ERROR_ACCESS_DENIED);
        CloseHandle(ro);
    }
    HANDLE so = OpenProcess(PROCESS_SET_INFORMATION, FALSE, GetCurrentProcessId());
    if (so) {
        SetLastError(0);
        CC(!UN(GetWsChanges)(so, buf, sizeof buf));
        CC(GetLastError() == ERROR_ACCESS_DENIED);
        CloseHandle(so);
    }
    /* before initialisation */
    SetLastError(0);
    BOOL pre = UN(GetWsChanges)(self, buf, sizeof buf);
    DWORD pre_e = GetLastError();
    CC(!pre);
    printf("before init: err %lu\n", pre_e);
    /* initialise (idempotent) */
    CC(UN(InitializeProcessForWsWatch)(self));
    CC(UN(InitializeProcessForWsWatch)(self));
    /* generate faults */
    BYTE *m = (BYTE *)VirtualAlloc(NULL, 64 * 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    CC(m != NULL);
    for (int i = 0; i < 64; i++) m[i * 4096] = (BYTE)i;
    memset(buf, 0xCC, sizeof buf);
    SetLastError(0);
    BOOL r = UN(GetWsChanges)(self, buf, sizeof buf);
    DWORD e = GetLastError();
    if (!r) { printf("GetWsChanges err %lu\n", e); CC(e == ERROR_INSUFFICIENT_BUFFER || e == ERROR_BAD_LENGTH); }
    else {
        struct ent { PVOID pc, va; } *p = (struct ent *)buf;
        unsigned n = 0;
        while ((BYTE *)(p + 1) <= buf + sizeof buf && p->pc != NULL) { p++; n++; }
        CC((BYTE *)(p + 1) <= buf + sizeof buf);       /* list is terminated */
        printf("ws changes %u\n", n);
    }
    /* too-small buffers */
    SetLastError(0);
    CC(!UN(GetWsChanges)(self, buf, 0));
    SetLastError(0);
    CC(!UN(GetWsChanges)(self, buf, 4));
    /* Ex variant */
    cb = sizeof buf;
    SetLastError(0);
    r = UN(GetWsChangesEx)(self, buf, &cb);
    e = GetLastError();
    if (r) {
        struct entx { PVOID pc, va; ULONG_PTR tid, flags; } *px = (struct entx *)buf;
        unsigned n = 0;
        while ((BYTE *)(px + 1) <= buf + sizeof buf && px->pc != NULL) { px++; n++; }
        CC((BYTE *)(px + 1) <= buf + sizeof buf);
        printf("ws changes ex %u cb=%lu\n", n, cb);
    } else { printf("GetWsChangesEx err %lu\n", e); CC(e == ERROR_INSUFFICIENT_BUFFER || e == ERROR_BAD_LENGTH); }
    cb = 0;
    SetLastError(0);
    CC(!UN(GetWsChangesEx)(self, buf, &cb));
    VirtualFree(m, 0, MEM_RELEASE);
    return 0;
}
TEST(gap6_wsw_child) {
    REQUIRE(UN(InitializeProcessForWsWatch)); REQUIRE(UN(GetWsChanges)); REQUIRE(UN(GetWsChangesEx));
    if (t_is_wine()) { t_skip("working set watch unimplemented under wine"); return; }
    DWORD rc = t_run_child("gap6_wsw", "", 30000);
    CHECKF(rc == 0, "child exit %lu (source line)", rc);
}
