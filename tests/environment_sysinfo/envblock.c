/* GetEnvironmentStrings*, FreeEnvironmentStrings*, ExpandEnvironmentStrings*, SetEnvironmentStringsW,
 * environment inheritance by child processes. */
#include "t.h"
#include "k.h"

/* ---------- block helpers ---------- */
typedef struct { int count; int hidden; int bad; size_t wchars; } blk_info;

static blk_info blk_scan_w(const WCHAR *b) {
    blk_info bi = {0, 0, 0, 0};
    const WCHAR *p = b;
    while (*p) {
        size_t l = wcslen(p);
        const WCHAR *eq = wcschr(p + 1, L'=');
        if (p[0] == L'=') bi.hidden++;
        if (!eq) bi.bad++;
        bi.count++;
        p += l + 1;
    }
    bi.wchars = (size_t)(p - b) + 1;
    return bi;
}
static const WCHAR *blk_find_w(const WCHAR *b, const WCHAR *name) {
    size_t nl = wcslen(name);
    for (const WCHAR *p = b; *p; p += wcslen(p) + 1)
        if (!_wcsnicmp(p, name, nl) && p[nl] == L'=') return p;
    return NULL;
}
static const char *blk_find_a(const char *b, const char *name) {
    size_t nl = strlen(name);
    for (const char *p = b; *p; p += strlen(p) + 1)
        if (!_strnicmp(p, name, nl) && p[nl] == '=') return p;
    return NULL;
}

TEST(envblock_w_basic_format) {
    REQUIRE(U(GetEnvironmentStringsW)); REQUIRE(U(FreeEnvironmentStringsW));
    WCHAR *b = U(GetEnvironmentStringsW)();
    CHECK(b != NULL);
    if (!b) return;
    blk_info bi = blk_scan_w(b);
    CHECK(bi.count > 3);
    EXPECT_EQ(bi.bad, 0);
    CHECK(U(FreeEnvironmentStringsW)(b));
}
TEST(envblock_a_basic_format) {
    REQUIRE(U(GetEnvironmentStringsA)); REQUIRE(U(FreeEnvironmentStringsA));
    char *b = U(GetEnvironmentStringsA)();
    CHECK(b != NULL);
    if (!b) return;
    int count = 0, bad = 0;
    const char *p = b;
    while (*p) { if (!strchr(p + 1, '=')) bad++; count++; p += strlen(p) + 1; }
    CHECK(count > 3);
    EXPECT_EQ(bad, 0);
    CHECK(U(FreeEnvironmentStringsA)(b));
}
TEST(envblock_unsuffixed_alias_matches_a) {
    REQUIRE(UB(GetEnvironmentStrings)); REQUIRE(UB(GetEnvironmentStringsA));
    CHECK((void *)UB(GetEnvironmentStrings) == (void *)UB(GetEnvironmentStringsA));
    char *b = UB(GetEnvironmentStrings)();
    CHECK(b != NULL);
    if (b) U(FreeEnvironmentStringsA)(b);
}
TEST(envblock_free_a_w_same_function) {
    REQUIRE(UB(FreeEnvironmentStringsA)); REQUIRE(UB(FreeEnvironmentStringsW));
    CHECK((void *)UB(FreeEnvironmentStringsA) == (void *)UB(FreeEnvironmentStringsW));
}
TEST(envblock_returns_distinct_copies) {
    REQUIRE(U(GetEnvironmentStringsW)); REQUIRE(U(FreeEnvironmentStringsW));
    WCHAR *a = U(GetEnvironmentStringsW)(), *b = U(GetEnvironmentStringsW)();
    CHECK(a && b);
    if (a && b) {
        CHECK(a != b);
        blk_info ia = blk_scan_w(a), ib = blk_scan_w(b);
        EXPECT_EQ(ia.wchars, ib.wchars);
        CHECK(!memcmp(a, b, ia.wchars * 2));
        /* a returned block is a private copy: scribbling on it must not change the real environment */
        U(SetEnvironmentVariableW)(L"K32T_ES_COPY", L"orig");
        WCHAR *c = U(GetEnvironmentStringsW)();
        WCHAR *e = (WCHAR *)blk_find_w(c, L"K32T_ES_COPY");
        CHECK(e != NULL);
        if (e) { e[wcslen(e) - 1] = L'X'; }
        WCHAR buf[16];
        EXPECT_EQ(U(GetEnvironmentVariableW)(L"K32T_ES_COPY", buf, 16), 4);
        EXPECT_WSTR(buf, L"orig");
        U(FreeEnvironmentStringsW)(c);
        U(SetEnvironmentVariableW)(L"K32T_ES_COPY", NULL);
    }
    if (a) U(FreeEnvironmentStringsW)(a);
    if (b) U(FreeEnvironmentStringsW)(b);
}
TEST(envblock_w_contains_set_variable) {
    REQUIRE(U(GetEnvironmentStringsW)); REQUIRE(U(FreeEnvironmentStringsW)); REQUIRE(U(SetEnvironmentVariableW));
    U(SetEnvironmentVariableW)(L"K32T_ES_V1", L"value one");
    WCHAR *b = U(GetEnvironmentStringsW)();
    const WCHAR *e = b ? blk_find_w(b, L"K32T_ES_V1") : NULL;
    CHECK(e != NULL);
    if (e) CHECK(!wcscmp(e, L"K32T_ES_V1=value one"));
    if (b) U(FreeEnvironmentStringsW)(b);
    U(SetEnvironmentVariableW)(L"K32T_ES_V1", NULL);
    b = U(GetEnvironmentStringsW)();
    CHECK(b && !blk_find_w(b, L"K32T_ES_V1"));
    if (b) U(FreeEnvironmentStringsW)(b);
}
TEST(envblock_a_contains_set_variable) {
    REQUIRE(U(GetEnvironmentStringsA)); REQUIRE(U(FreeEnvironmentStringsA)); REQUIRE(U(SetEnvironmentVariableA));
    U(SetEnvironmentVariableA)("K32T_ES_V2", "ansi value");
    char *b = U(GetEnvironmentStringsA)();
    const char *e = b ? blk_find_a(b, "K32T_ES_V2") : NULL;
    CHECK(e != NULL);
    if (e) EXPECT_STR(e, "K32T_ES_V2=ansi value");
    if (b) U(FreeEnvironmentStringsA)(b);
    U(SetEnvironmentVariableA)("K32T_ES_V2", NULL);
}
TEST(envblock_a_w_same_entry_count) {
    REQUIRE(U(GetEnvironmentStringsA)); REQUIRE(U(GetEnvironmentStringsW));
    REQUIRE(U(FreeEnvironmentStringsA)); REQUIRE(U(FreeEnvironmentStringsW));
    WCHAR *w = U(GetEnvironmentStringsW)(); char *a = U(GetEnvironmentStringsA)();
    CHECK(w && a);
    if (w && a) {
        blk_info bi = blk_scan_w(w);
        int n = 0; for (const char *p = a; *p; p += strlen(p) + 1) n++;
        EXPECT_EQ(n, bi.count);
    }
    if (w) U(FreeEnvironmentStringsW)(w);
    if (a) U(FreeEnvironmentStringsA)(a);
}
TEST(envblock_reflects_changes_immediately) {
    REQUIRE(U(GetEnvironmentStringsW)); REQUIRE(U(FreeEnvironmentStringsW)); REQUIRE(U(SetEnvironmentVariableW));
    WCHAR *b0 = U(GetEnvironmentStringsW)();
    blk_info i0 = blk_scan_w(b0);
    U(SetEnvironmentVariableW)(L"K32T_ES_CHG", L"1");
    WCHAR *b1 = U(GetEnvironmentStringsW)();
    blk_info i1 = blk_scan_w(b1);
    EXPECT_EQ(i1.count, i0.count + 1);
    EXPECT_EQ(i1.wchars, i0.wchars + wcslen(L"K32T_ES_CHG=1") + 1);
    U(SetEnvironmentVariableW)(L"K32T_ES_CHG", L"12345");
    WCHAR *b2 = U(GetEnvironmentStringsW)();
    blk_info i2 = blk_scan_w(b2);
    EXPECT_EQ(i2.count, i1.count);
    EXPECT_EQ(i2.wchars, i1.wchars + 4);
    /* the old snapshot does not change */
    CHECK(!wcscmp(blk_find_w(b1, L"K32T_ES_CHG"), L"K32T_ES_CHG=1"));
    U(SetEnvironmentVariableW)(L"K32T_ES_CHG", NULL);
    WCHAR *b3 = U(GetEnvironmentStringsW)();
    blk_info i3 = blk_scan_w(b3);
    EXPECT_EQ(i3.count, i0.count);
    EXPECT_EQ(i3.wchars, i0.wchars);
    U(FreeEnvironmentStringsW)(b0); U(FreeEnvironmentStringsW)(b1);
    U(FreeEnvironmentStringsW)(b2); U(FreeEnvironmentStringsW)(b3);
}
TEST(envblock_sorted_insertion_of_new_variables) {
    REQUIRE(U(GetEnvironmentStringsW)); REQUIRE(U(FreeEnvironmentStringsW)); REQUIRE(U(SetEnvironmentVariableW));
    U(SetEnvironmentVariableW)(L"K32T_ES_SORT_C", L"3");
    U(SetEnvironmentVariableW)(L"k32t_es_sort_a", L"1");
    U(SetEnvironmentVariableW)(L"K32T_ES_SORT_B", L"2");
    WCHAR *b = U(GetEnvironmentStringsW)();
    const WCHAR *a = blk_find_w(b, L"K32T_ES_SORT_A"), *bb = blk_find_w(b, L"K32T_ES_SORT_B"), *c = blk_find_w(b, L"K32T_ES_SORT_C");
    CHECK(a && bb && c);
    if (a && bb && c) CHECKF(a < bb && bb < c, "block order A=%p B=%p C=%p (variables are kept sorted)", a, bb, c);
    /* the stored name keeps the case it was created with */
    if (a) CHECK(!wcsncmp(a, L"k32t_es_sort_a=", 15));
    /* neighbours in the block are in non-decreasing (case-insensitive) order */
    int unsorted = 0; const WCHAR *prev = NULL;
    for (const WCHAR *p = b; *p; p += wcslen(p) + 1) {
        if (prev && _wcsicmp(prev, p) > 0) unsorted++;
        prev = p;
    }
    t_note("entries out of order in the process environment block: %d", unsorted);
    U(FreeEnvironmentStringsW)(b);
    U(SetEnvironmentVariableW)(L"K32T_ES_SORT_A", NULL); U(SetEnvironmentVariableW)(L"K32T_ES_SORT_B", NULL); U(SetEnvironmentVariableW)(L"K32T_ES_SORT_C", NULL);
}
TEST(envblock_hidden_equals_entries) {
    REQUIRE(U(GetEnvironmentStringsW)); REQUIRE(U(FreeEnvironmentStringsW)); REQUIRE(U(SetEnvironmentVariableW));
    U(SetEnvironmentVariableW)(L"=K32T_ES_HID", L"h");
    WCHAR *b = U(GetEnvironmentStringsW)();
    const WCHAR *e = blk_find_w(b + 0, L"") ? NULL : NULL;
    int found = 0;
    for (const WCHAR *p = b; *p; p += wcslen(p) + 1) if (!wcscmp(p, L"=K32T_ES_HID=h")) found = 1;
    CHECK(found);
    blk_info bi = blk_scan_w(b);
    t_note("hidden '=' entries in block: %d of %d", bi.hidden, bi.count);
    EXPECT_EQ(bi.bad, 0);
    U(FreeEnvironmentStringsW)(b);
    U(SetEnvironmentVariableW)(L"=K32T_ES_HID", NULL);
}
TEST(envblock_empty_value_entry) {
    REQUIRE(U(GetEnvironmentStringsW)); REQUIRE(U(FreeEnvironmentStringsW)); REQUIRE(U(SetEnvironmentVariableW));
    U(SetEnvironmentVariableW)(L"K32T_ES_EMPTY", L"");
    WCHAR *b = U(GetEnvironmentStringsW)();
    const WCHAR *e = blk_find_w(b, L"K32T_ES_EMPTY");
    CHECK(e != NULL);
    if (e) CHECK(!wcscmp(e, L"K32T_ES_EMPTY="));
    U(FreeEnvironmentStringsW)(b);
    U(SetEnvironmentVariableW)(L"K32T_ES_EMPTY", NULL);
}
TEST(envblock_value_with_equals_roundtrips) {
    REQUIRE(U(GetEnvironmentStringsW)); REQUIRE(U(FreeEnvironmentStringsW)); REQUIRE(U(SetEnvironmentVariableW));
    U(SetEnvironmentVariableW)(L"K32T_ES_EQ", L"a=b=c");
    WCHAR *b = U(GetEnvironmentStringsW)();
    const WCHAR *e = blk_find_w(b, L"K32T_ES_EQ");
    CHECK(e && !wcscmp(e, L"K32T_ES_EQ=a=b=c"));
    U(FreeEnvironmentStringsW)(b);
    U(SetEnvironmentVariableW)(L"K32T_ES_EQ", NULL);
}
TEST(envblock_free_many_cycles) {
    REQUIRE(U(GetEnvironmentStringsW)); REQUIRE(U(FreeEnvironmentStringsW));
    for (int i = 0; i < 500; i++) {
        WCHAR *b = U(GetEnvironmentStringsW)();
        if (!b) { CHECK(b); break; }
        if (!U(FreeEnvironmentStringsW)(b)) { CHECK(0); break; }
    }
    CHECK(1);
}
TEST(envblock_system_vars_present_in_block) {
    REQUIRE(U(GetEnvironmentStringsW)); REQUIRE(U(FreeEnvironmentStringsW));
    WCHAR *b = U(GetEnvironmentStringsW)();
    CHECK(blk_find_w(b, L"SystemRoot") != NULL);
    CHECK(blk_find_w(b, L"PATH") != NULL);
    CHECK(blk_find_w(b, L"windir") != NULL);
    U(FreeEnvironmentStringsW)(b);
}
TEST(envblock_get_matches_getenvironmentvariable) {
    REQUIRE(U(GetEnvironmentStringsW)); REQUIRE(U(FreeEnvironmentStringsW)); REQUIRE(U(GetEnvironmentVariableW));
    WCHAR *b = U(GetEnvironmentStringsW)();
    static WCHAR vb[32768];
    int checked = 0;
    for (const WCHAR *p = b; *p; p += wcslen(p) + 1) {
        if (p[0] == L'=') continue;
        const WCHAR *eq = wcschr(p, L'=');
        WCHAR name[512]; size_t nl = eq - p;
        if (nl >= 512) continue;
        memcpy(name, p, nl * 2); name[nl] = 0;
        DWORD n = U(GetEnvironmentVariableW)(name, vb, 32768);
        if (n != wcslen(eq + 1) || wcscmp(vb, eq + 1)) { CHECKF(0, "mismatch for %ls", name); }
        else checked++;
    }
    CHECK(checked > 3);
    U(FreeEnvironmentStringsW)(b);
}

/* ---------- ExpandEnvironmentStrings ---------- */
typedef struct { const char *src, *want; } xcase;
static const xcase xcases[] = {
    { "plain", "plain" },
    { "", "" },
    { "%K32T_XA%", "Hello" },
    { "a%K32T_XA%b", "aHellob" },
    { "%K32T_XA%%K32T_XB%", "HelloWorld" },
    { "%K32T_XA% %K32T_XB%", "Hello World" },
    { "%k32t_xa%", "Hello" },
    { "%K32T_XA%\\%K32T_XB%\\x", "Hello\\World\\x" },
    { "%K32T_NOPE%", "%K32T_NOPE%" },
    { "x%K32T_NOPE%y", "x%K32T_NOPE%y" },
    { "%K32T_NOPE%K32T_XA%", "%K32T_NOPEHello" },
    { "%", "%" },
    { "%%", "%%" },
    { "%%%%", "%%%%" },
    { "100%", "100%" },
    { "%%K32T_XA%", "%Hello" },
    { "%K32T_XA%%", "Hello%" },
    { "%K32T_XA", "%K32T_XA" },
    { "K32T_XA%", "K32T_XA%" },
    { "%K32T_XA%K32T_XB%", "HelloK32T_XB%" },
    { "%K32T_E%", "" },
    { "x%K32T_E%y", "xy" },
    { "%K32T_REC%", "%K32T_XA%" },
    { "% K32T_XA %", "% K32T_XA %" },
    { "%K32T_XA %", "%K32T_XA %" },
    { "%K32T_XA%%K32T_XA%%K32T_XA%", "HelloHelloHello" },
    { "%K32T_EQ%", "a=b" },
    { "%K32T_SP%", "  sp  " },
    { "no vars here 100% sure", "no vars here 100% sure" },
};
static void xsetup(void) {
    U(SetEnvironmentVariableA)("K32T_XA", "Hello");
    U(SetEnvironmentVariableA)("K32T_XB", "World");
    U(SetEnvironmentVariableA)("K32T_E", "");
    U(SetEnvironmentVariableA)("K32T_REC", "%K32T_XA%");
    U(SetEnvironmentVariableA)("K32T_EQ", "a=b");
    U(SetEnvironmentVariableA)("K32T_SP", "  sp  ");
}
static void xteardown(void) {
    const char *n[] = { "K32T_XA", "K32T_XB", "K32T_E", "K32T_REC", "K32T_EQ", "K32T_SP" };
    for (int i = 0; i < 6; i++) U(SetEnvironmentVariableA)(n[i], NULL);
}
TEST(envexpand_a_table) {
    REQUIRE(U(ExpandEnvironmentStringsA)); REQUIRE(U(SetEnvironmentVariableA));
    xsetup();
    for (unsigned i = 0; i < sizeof xcases / sizeof xcases[0]; i++) {
        char out[256]; memset(out, 0x55, sizeof out);
        DWORD n = U(ExpandEnvironmentStringsA)(xcases[i].src, out, 256);
        CHECKF(n == strlen(xcases[i].want) + 1, "src=\"%s\" ret=%lu want=%zu", xcases[i].src, n, strlen(xcases[i].want) + 1);
        CHECKF(!strcmp(out, xcases[i].want), "src=\"%s\" got=\"%s\" want=\"%s\"", xcases[i].src, out, xcases[i].want);
    }
    xteardown();
}
TEST(envexpand_w_table) {
    REQUIRE(U(ExpandEnvironmentStringsW)); REQUIRE(U(SetEnvironmentVariableA));
    xsetup();
    for (unsigned i = 0; i < sizeof xcases / sizeof xcases[0]; i++) {
        WCHAR src[128], want[128], out[256];
        MultiByteToWideChar(CP_ACP, 0, xcases[i].src, -1, src, 128);
        MultiByteToWideChar(CP_ACP, 0, xcases[i].want, -1, want, 128);
        for (int j = 0; j < 256; j++) out[j] = 0x5555;
        DWORD n = U(ExpandEnvironmentStringsW)(src, out, 256);
        CHECKF(n == strlen(xcases[i].want) + 1, "src=\"%s\" ret=%lu want=%zu", xcases[i].src, n, strlen(xcases[i].want) + 1);
        CHECKF(!wcscmp(out, want), "src=\"%s\" mismatch", xcases[i].src);
    }
    xteardown();
}
TEST(envexpand_w_size_semantics) {
    REQUIRE(U(ExpandEnvironmentStringsW)); REQUIRE(U(SetEnvironmentVariableA));
    xsetup();
    for (unsigned i = 0; i < sizeof xcases / sizeof xcases[0]; i++) {
        WCHAR src[128], out[256];
        MultiByteToWideChar(CP_ACP, 0, xcases[i].src, -1, src, 128);
        DWORD need = (DWORD)strlen(xcases[i].want) + 1;
        /* nSize == 0 / NULL buffer: required size */
        CHECKF(U(ExpandEnvironmentStringsW)(src, NULL, 0) == need, "src=\"%s\" null/0", xcases[i].src);
        CHECKF(U(ExpandEnvironmentStringsW)(src, out, 0) == need, "src=\"%s\" 0", xcases[i].src);
        /* one short: returns the required size, which is > nSize */
        if (need > 1) CHECKF(U(ExpandEnvironmentStringsW)(src, out, need - 1) == need, "src=\"%s\" need-1", xcases[i].src);
        /* exact */
        for (int j = 0; j < 256; j++) out[j] = 0x5555;
        CHECKF(U(ExpandEnvironmentStringsW)(src, out, need) == need, "src=\"%s\" exact", xcases[i].src);
        CHECKF(out[need - 1] == 0, "src=\"%s\" terminator", xcases[i].src);
        CHECKF(out[need] == 0x5555, "src=\"%s\" no overrun", xcases[i].src);
    }
    xteardown();
}
TEST(envexpand_a_size_semantics) {
    /* The ANSI implementation expands into a (nSize-1)-character buffer and, when that is too small,
     * retries with the exact size and returns the ANSI size + 1: so every too-small (and exact-fit)
     * call reports need+1, and a successful call needs nSize >= need+1 to produce output. */
    REQUIRE(U(ExpandEnvironmentStringsA)); REQUIRE(U(SetEnvironmentVariableA));
    xsetup();
    for (unsigned i = 0; i < sizeof xcases / sizeof xcases[0]; i++) {
        char out[256];
        DWORD need = (DWORD)strlen(xcases[i].want) + 1;
        DWORD r;
        r = U(ExpandEnvironmentStringsA)(xcases[i].src, NULL, 0);
        CHECKF(r == need + 1, "src=\"%s\" null/0 -> %lu (need %lu)", xcases[i].src, r, need);
        r = U(ExpandEnvironmentStringsA)(xcases[i].src, out, 0);
        CHECKF(r == need + 1, "src=\"%s\" 0 -> %lu", xcases[i].src, r);
        if (need > 1) { r = U(ExpandEnvironmentStringsA)(xcases[i].src, out, need - 1);
            CHECKF(r == need + 1, "src=\"%s\" need-1 -> %lu", xcases[i].src, r); }
        r = U(ExpandEnvironmentStringsA)(xcases[i].src, out, need);
        CHECKF(r == need + 1, "src=\"%s\" exact -> %lu", xcases[i].src, r);
        memset(out, 0x55, sizeof out);
        r = U(ExpandEnvironmentStringsA)(xcases[i].src, out, need + 1);
        CHECKF(r == need, "src=\"%s\" need+1 -> %lu", xcases[i].src, r);
        CHECKF(!strcmp(out, xcases[i].want), "src=\"%s\" need+1 content", xcases[i].src);
    }
    xteardown();
}
TEST(envexpand_w_truncation_does_not_write_past_nsize) {
    REQUIRE(U(ExpandEnvironmentStringsW)); REQUIRE(U(SetEnvironmentVariableA));
    xsetup();
    WCHAR out[32];
    for (DWORD ns = 1; ns < 12; ns++) {
        for (int j = 0; j < 32; j++) out[j] = 0x5555;
        DWORD n = U(ExpandEnvironmentStringsW)(L"%K32T_XA%%K32T_XB%", out, ns);
        EXPECT_EQ(n, 11);
        for (DWORD j = ns; j < 32; j++) if (out[j] != 0x5555) { CHECKF(0, "nSize=%lu wrote out[%lu]", ns, j); break; }
    }
    xteardown();
}
TEST(envexpand_w_null_source) {
    REQUIRE(U(ExpandEnvironmentStringsW));
    WCHAR out[4] = { 0x5555, 0x5555, 0x5555, 0x5555 };
    DWORD n = U(ExpandEnvironmentStringsW)(NULL, out, 4);
    t_note("ExpandEnvironmentStringsW(NULL) -> %lu out[0]=%x", n, out[0]);
    EXPECT_EQ(n, 1);
    EXPECT_EQ(out[0], 0);
}
TEST(envexpand_system_variables) {
    REQUIRE(U(ExpandEnvironmentStringsA)); REQUIRE(U(GetWindowsDirectoryA));
    char out[MAX_PATH * 2], wd[MAX_PATH];
    U(GetWindowsDirectoryA)(wd, MAX_PATH);
    DWORD n = U(ExpandEnvironmentStringsA)("%SystemRoot%\\system32", out, sizeof out);
    char want[MAX_PATH * 2]; snprintf(want, sizeof want, "%s\\system32", wd);
    EXPECT_EQ(n, strlen(want) + 1);
    CHECK(!_stricmp(out, want));
    n = U(ExpandEnvironmentStringsA)("%WINDIR%", out, sizeof out);
    CHECK(!_stricmp(out, wd));
    n = U(ExpandEnvironmentStringsA)("%systemdrive%\\", out, sizeof out);
    EXPECT_EQ(n, 4);
    CHECK(out[1] == ':' && out[2] == '\\');
}
TEST(envexpand_nonascii_variable_value_w) {
    REQUIRE(U(ExpandEnvironmentStringsW)); REQUIRE(U(SetEnvironmentVariableW));
    U(SetEnvironmentVariableW)(L"K32T_UNI", L"\x00E9\x4E2D");
    WCHAR out[32];
    EXPECT_EQ(U(ExpandEnvironmentStringsW)(L"<%K32T_UNI%>", out, 32), 5);
    CHECK(!wcscmp(out, L"<\x00E9\x4E2D>"));
    EXPECT_EQ(U(ExpandEnvironmentStringsW)(L"<%k32t_uni%>", out, 32), 5);
    U(SetEnvironmentVariableW)(L"K32T_UNI", NULL);
}
TEST(envexpand_long_value) {
    REQUIRE(U(ExpandEnvironmentStringsW)); REQUIRE(U(SetEnvironmentVariableW));
    static WCHAR v[5001], out[10100];
    for (int i = 0; i < 5000; i++) v[i] = L'a' + (i % 26);
    v[5000] = 0;
    U(SetEnvironmentVariableW)(L"K32T_LONGX", v);
    EXPECT_EQ(U(ExpandEnvironmentStringsW)(L"%K32T_LONGX%%K32T_LONGX%", out, 10100), 10001);
    CHECK(!wcsncmp(out, v, 5000) && !wcsncmp(out + 5000, v, 5000));
    EXPECT_EQ(U(ExpandEnvironmentStringsW)(L"%K32T_LONGX%%K32T_LONGX%", out, 10000), 10001);
    U(SetEnvironmentVariableW)(L"K32T_LONGX", NULL);
}
TEST(envexpand_ab_parity_for_ascii_sources) {
    REQUIRE(U(ExpandEnvironmentStringsA)); REQUIRE(U(ExpandEnvironmentStringsW)); REQUIRE(U(SetEnvironmentVariableA));
    xsetup();
    for (unsigned i = 0; i < sizeof xcases / sizeof xcases[0]; i++) {
        char oa[256]; WCHAR ow[256], src[128], conv[256];
        MultiByteToWideChar(CP_ACP, 0, xcases[i].src, -1, src, 128);
        DWORD na = U(ExpandEnvironmentStringsA)(xcases[i].src, oa, 256);
        DWORD nw = U(ExpandEnvironmentStringsW)(src, ow, 256);
        CHECKF(na == nw, "src=\"%s\" A=%lu W=%lu", xcases[i].src, na, nw);
        MultiByteToWideChar(CP_ACP, 0, oa, -1, conv, 256);
        CHECKF(!wcscmp(conv, ow), "src=\"%s\"", xcases[i].src);
    }
    xteardown();
}
TEST(envexpand_does_not_modify_environment) {
    REQUIRE(U(ExpandEnvironmentStringsA)); REQUIRE(U(SetEnvironmentVariableA)); REQUIRE(U(GetEnvironmentVariableA));
    char out[64], v[64];
    U(SetEnvironmentVariableA)("K32T_XA", "Hello");
    U(ExpandEnvironmentStringsA)("%K32T_XA%", out, 64);
    EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_XA", v, 64), 5);
    EXPECT_STR(v, "Hello");
    U(SetEnvironmentVariableA)("K32T_XA", NULL);
}
TEST(envexpand_updates_after_change) {
    REQUIRE(U(ExpandEnvironmentStringsA)); REQUIRE(U(SetEnvironmentVariableA));
    char out[64];
    U(SetEnvironmentVariableA)("K32T_XC", "one");
    U(ExpandEnvironmentStringsA)("%K32T_XC%", out, 64); EXPECT_STR(out, "one");
    U(SetEnvironmentVariableA)("K32T_XC", "two2");
    U(ExpandEnvironmentStringsA)("%K32T_XC%", out, 64); EXPECT_STR(out, "two2");
    U(SetEnvironmentVariableA)("K32T_XC", NULL);
    U(ExpandEnvironmentStringsA)("%K32T_XC%", out, 64); EXPECT_STR(out, "%K32T_XC%");
}

/* ---------- children: inheritance and isolation ---------- */
static DWORD ea(const char *n, char *b, DWORD s) { return GetEnvironmentVariableA(n, b, s); }

/* argv: name expected-value ("-" => must be absent) */
CHILD(envchk) {
    char b[256];
    if (argc < 3) return 90;
    DWORD n = GetEnvironmentVariableA(argv[1], b, sizeof b);
    if (!strcmp(argv[2], "-")) return (n == 0 && GetLastError() == 203) ? 0 : 11;
    if (n != strlen(argv[2])) return 12;
    return strcmp(b, argv[2]) ? 13 : 0;
}
TEST(envchild_inherits_parent_variable) {
    REQUIRE(U(SetEnvironmentVariableA));
    U(SetEnvironmentVariableA)("K32T_INH1", "from-parent");
    EXPECT_EQ(t_run_child("envchk", "K32T_INH1 from-parent", 20000), 0);
    U(SetEnvironmentVariableA)("K32T_INH1", NULL);
}
TEST(envchild_does_not_see_deleted_variable) {
    REQUIRE(U(SetEnvironmentVariableA));
    U(SetEnvironmentVariableA)("K32T_INH2", "x");
    U(SetEnvironmentVariableA)("K32T_INH2", NULL);
    EXPECT_EQ(t_run_child("envchk", "K32T_INH2 -", 20000), 0);
}
TEST(envchild_inherits_changed_value) {
    REQUIRE(U(SetEnvironmentVariableA));
    U(SetEnvironmentVariableA)("K32T_INH3", "first");
    U(SetEnvironmentVariableA)("K32T_INH3", "second");
    EXPECT_EQ(t_run_child("envchk", "K32T_INH3 second", 20000), 0);
    U(SetEnvironmentVariableA)("K32T_INH3", NULL);
}
TEST(envchild_inherits_path_with_spaces) {
    REQUIRE(U(SetEnvironmentVariableA));
    U(SetEnvironmentVariableA)("K32T_INH4", "C:\\Program Files\\A B;D:\\x");
    PROCESS_INFORMATION pi;
    HANDLE h = t_spawn_child("envchk", "K32T_INH4 \"C:\\Program Files\\A B;D:\\x\"", 0, NULL, NULL, NULL, FALSE, NULL, &pi);
    CHECK(h != NULL);
    if (h) {
        DWORD code = 99;
        CHECK(WaitForSingleObject(h, 20000) == WAIT_OBJECT_0);
        GetExitCodeProcess(h, &code);
        EXPECT_EQ(code, 0);
        CloseHandle(pi.hThread); CloseHandle(h);
    }
    U(SetEnvironmentVariableA)("K32T_INH4", NULL);
}

/* custom environment block (ANSI) */
CHILD(envexact) {
    LPWCH b = GetEnvironmentStringsW();
    if (!b) return 20;
    int visible = 0, ok_a = 0, ok_b = 0;
    for (const WCHAR *p = b; *p; p += wcslen(p) + 1) {
        if (p[0] == L'=') continue;
        visible++;
        if (!wcscmp(p, L"K32T_CE_A=alpha")) ok_a = 1;
        else if (!wcscmp(p, L"K32T_CE_B=beta")) ok_b = 1;
    }
    FreeEnvironmentStringsW(b);
    if (!ok_a) return 21;
    if (!ok_b) return 22;
    if (visible != 3) return 30 + visible;   /* A, B and SystemRoot */
    char sr[MAX_PATH];
    return GetEnvironmentVariableA("SystemRoot", sr, MAX_PATH) ? 0 : 23;
}
TEST(envchild_custom_block_replaces_environment) {
    char sr[MAX_PATH], blk[1024]; size_t o = 0;
    GetEnvironmentVariableA("SystemRoot", sr, MAX_PATH);
    o += snprintf(blk + o, sizeof blk - o, "K32T_CE_A=alpha") + 1;
    o += snprintf(blk + o, sizeof blk - o, "K32T_CE_B=beta") + 1;
    o += snprintf(blk + o, sizeof blk - o, "SystemRoot=%s", sr) + 1;
    blk[o] = 0;
    U(SetEnvironmentVariableA)("K32T_INH_PARENT_ONLY", "p");
    PROCESS_INFORMATION pi; DWORD code = 99;
    HANDLE h = t_spawn_child("envexact", "", 0, NULL, NULL, NULL, FALSE, blk, &pi);
    CHECK(h != NULL);
    if (h) {
        CHECK(WaitForSingleObject(h, 20000) == WAIT_OBJECT_0);
        GetExitCodeProcess(h, &code);
        EXPECT_EQ(code, 0);
        CloseHandle(pi.hThread); CloseHandle(h);
    }
    U(SetEnvironmentVariableA)("K32T_INH_PARENT_ONLY", NULL);
}

/* child modifications must not leak to the parent */
CHILD(envmodify) {
    SetEnvironmentVariableA("K32T_ISO_KEEP", "child-changed");
    SetEnvironmentVariableA("K32T_ISO_DEL", NULL);
    SetEnvironmentVariableA("K32T_ISO_NEW", "child-new");
    char b[64];
    if (GetEnvironmentVariableA("K32T_ISO_KEEP", b, 64) != 13) return 1;
    if (GetEnvironmentVariableA("K32T_ISO_DEL", b, 64) != 0) return 2;
    if (GetEnvironmentVariableA("K32T_ISO_NEW", b, 64) != 9) return 3;
    return 0;
}
TEST(envchild_modifications_do_not_leak_to_parent) {
    REQUIRE(U(SetEnvironmentVariableA)); REQUIRE(U(GetEnvironmentVariableA));
    char b[64];
    U(SetEnvironmentVariableA)("K32T_ISO_KEEP", "parent");
    U(SetEnvironmentVariableA)("K32T_ISO_DEL", "parent-del");
    EXPECT_EQ(t_run_child("envmodify", "", 20000), 0);
    EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_ISO_KEEP", b, 64), 6);
    EXPECT_STR(b, "parent");
    EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_ISO_DEL", b, 64), 10);
    SET_ERR(0);
    EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_ISO_NEW", b, 64), 0);
    EXPECT_ERR(203);
    U(SetEnvironmentVariableA)("K32T_ISO_KEEP", NULL);
    U(SetEnvironmentVariableA)("K32T_ISO_DEL", NULL);
}

/* grandchild inherits variables a child set itself */
CHILD(envgrand) {
    SetEnvironmentVariableA("K32T_GR", "set-by-child");
    return (int)t_run_child("envchk", "K32T_GR set-by-child", 20000);
}
TEST(envchild_grandchild_sees_childs_variable) {
    EXPECT_EQ(t_run_child("envgrand", "", 30000), 0);
}

/* SetEnvironmentStringsW */
CHILD(envsetstrings) {
    static const WCHAR good[] = L"K32T_NEW1=one\0K32T_NEW2=two\0\0";
    if (!SetEnvironmentStringsW((LPWCH)good)) return 1;
    WCHAR b[64];
    if (GetEnvironmentVariableW(L"K32T_NEW1", b, 64) != 3 || wcscmp(b, L"one")) return 2;
    if (GetEnvironmentVariableW(L"K32T_NEW2", b, 64) != 3) return 3;
    SetLastError(0);
    if (GetEnvironmentVariableW(L"PATH", b, 64) != 0 || GetLastError() != 203) return 4;
    if (GetEnvironmentVariableW(L"SystemRoot", b, 64) != 0) return 5;
    LPWCH blk = GetEnvironmentStringsW();
    int n = 0; for (const WCHAR *p = blk; *p; p += wcslen(p) + 1) n++;
    FreeEnvironmentStringsW(blk);
    if (n != 2) return 6;
    /* subsequent edits still work */
    if (!SetEnvironmentVariableW(L"K32T_NEW3", L"three")) return 7;
    if (GetEnvironmentVariableW(L"K32T_NEW3", b, 64) != 5) return 8;
    return 0;
}
TEST(envchild_setenvironmentstringsw_replaces_block) {
    REQUIRE(U(SetEnvironmentStringsW));
    EXPECT_EQ(t_run_child("envsetstrings", "", 20000), 0);
}
CHILD(envsetstrings_bad) {
    int fails = 0;
    static const WCHAR noeq[] = L"NOEQUALS\0\0";
    static const WCHAR emptyblk[] = L"\0\0";
    static const WCHAR leadeq[] = L"=ONLY\0\0";
    static const WCHAR second_bad[] = L"OK=1\0BAD\0\0";
    static const WCHAR emptyname[] = L"=value\0\0";
    const WCHAR *bad[] = { noeq, emptyblk, leadeq, second_bad, emptyname };
    for (int i = 0; i < 5; i++) {
        SetLastError(0);
        BOOL r = SetEnvironmentStringsW((LPWCH)bad[i]);
        if (r) { fails |= 1 << i; continue; }
        if (GetLastError() != ERROR_INVALID_PARAMETER) fails |= 1 << (i + 8);
    }
    /* environment untouched after the failures */
    char b[MAX_PATH];
    if (!GetEnvironmentVariableA("SystemRoot", b, MAX_PATH)) fails |= 1 << 20;
    return fails;
}
TEST(envchild_setenvironmentstringsw_rejects_malformed_blocks) {
    REQUIRE(U(SetEnvironmentStringsW));
    DWORD r = t_run_child("envsetstrings_bad", "", 20000);
    CHECKF(r == 0, "failure bitmask 0x%lx", r);
}
CHILD(envsetstrings_leadeq) {
    /* a leading '=' entry with a proper second '=' is accepted (hidden variables like =C:) */
    static const WCHAR blk[] = L"=Z:=Z:\\tmp\0K32T_V=1\0\0";
    if (!SetEnvironmentStringsW((LPWCH)blk)) return 1;
    WCHAR b[32];
    if (GetEnvironmentVariableW(L"=Z:", b, 32) != 6) return 2;
    return 0;
}
TEST(envchild_setenvironmentstringsw_accepts_hidden_entries) {
    REQUIRE(U(SetEnvironmentStringsW));
    EXPECT_EQ(t_run_child("envsetstrings_leadeq", "", 20000), 0);
}
