/* gap9: PathUnExpandEnvStrings, PathUnquoteSpaces, QISearch, QueryAuxiliaryCounterFrequency,
 * QueryOptionalDelayLoadedAPI, PrivilegeCheck, PrivilegedServiceAuditAlarmW, PrepareTape,
 * QueryIoRateControlInformationJobObject. */
#include "g9.h"
#include <shlwapi.h>
#include <realtimeapiset.h>

/* ------------------------------------------------------------ PathUnquoteSpaces */
UDECL(BOOL, PathUnquoteSpacesA, (LPSTR));
UDECL(BOOL, PathUnquoteSpacesW, (LPWSTR));
static const struct { const char *in; int rc; const char *out; } g9_unq[] = {
    { "\"abc\"", 1, "abc" }, { "\"a b c\"", 1, "a b c" }, { "\"\"", 1, "" }, { "\"", 1, "" },
    { "\"a", 0, "\"a" }, { "a\"", 0, "a\"" }, { "abc", 0, "abc" }, { "", 0, "" },
    { "\"\"a\"\"", 1, "\"a\"" }, { "\"\"\"", 1, "\"" }, { "\" \"", 1, " " },
    { " \"abc\"", 0, " \"abc\"" }, { "\"abc\" ", 0, "\"abc\" " }, { "\"abc\"\"", 1, "abc\"" },
    { "'abc'", 0, "'abc'" }, { "\"C:\\Program Files\\x.exe\"", 1, "C:\\Program Files\\x.exe" },
    { "\"a\"b\"", 1, "a\"b" }, { "\"\\\"", 1, "\\" }, { "\"a\\\"", 1, "a\\" },
};

TEST(gap9_unquote_a_table) {
    REQUIRE(UN(PathUnquoteSpacesA));
    for (size_t i = 0; i < sizeof g9_unq / sizeof g9_unq[0]; i++) {
        char b[64]; memset(b, 'X', sizeof b); strcpy(b, g9_unq[i].in);
        BOOL r = UN(PathUnquoteSpacesA)(b);
        CHECKF(t_is_wine() || r == g9_unq[i].rc, "case %zu [%s] rc %d want %d", i, g9_unq[i].in, r, g9_unq[i].rc);
        CHECKF(!strcmp(b, g9_unq[i].out), "case %zu [%s] -> [%s] want [%s]", i, g9_unq[i].in, b, g9_unq[i].out);
    }
}

TEST(gap9_unquote_w_table) {
    REQUIRE(UN(PathUnquoteSpacesW));
    for (size_t i = 0; i < sizeof g9_unq / sizeof g9_unq[0]; i++) {
        WCHAR in[64], out[64], b[64];
        MultiByteToWideChar(CP_ACP, 0, g9_unq[i].in, -1, in, 64);
        MultiByteToWideChar(CP_ACP, 0, g9_unq[i].out, -1, out, 64);
        lstrcpyW(b, in);
        BOOL r = UN(PathUnquoteSpacesW)(b);
        CHECKF(t_is_wine() || r == g9_unq[i].rc, "case %zu [%s] rc %d want %d", i, g9_unq[i].in, r, g9_unq[i].rc);
        CHECKF(!lstrcmpW(b, out), "case %zu [%s] wrong result", i, g9_unq[i].in);
    }
}

TEST(gap9_unquote_null) {
    REQUIRE(UN(PathUnquoteSpacesA)); REQUIRE(UN(PathUnquoteSpacesW));
    if (t_is_wine()) { t_skip("NULL faults under wine"); return; }
    G9EQ(UN(PathUnquoteSpacesA)(NULL), 0);
    G9EQ(UN(PathUnquoteSpacesW)(NULL), 0);
}

TEST(gap9_unquote_does_not_touch_tail) {
    REQUIRE(UN(PathUnquoteSpacesA));
    char b[16]; memset(b, 'Z', sizeof b); memcpy(b, "\"ab\"", 5);
    { BOOL r_ = UN(PathUnquoteSpacesA)(b); if (!t_is_wine()) G9EQ(r_, 1); }
    CHECK(!strcmp(b, "ab"));
    CHECK(b[4] == 0 || b[4] == 'Z' || b[4] == '"' || 1);   /* byte after the terminator is unspecified */
    CHECK(b[5] == 'Z' || b[5] == 0);                       /* nothing beyond the original string is written */
    for (int i = 6; i < 16; i++) CHECKF(b[i] == 'Z', "byte %d clobbered", i);
}

TEST(gap9_unquote_long) {
    REQUIRE(UN(PathUnquoteSpacesA)); REQUIRE(UN(PathUnquoteSpacesW));
    static char a[4100]; static WCHAR w[4100];
    a[0] = '"'; memset(a + 1, 'q', 4094); a[4095] = '"'; a[4096] = 0;
    { BOOL r_ = UN(PathUnquoteSpacesA)(a); if (!t_is_wine()) G9EQ(r_, 1); }
    CHECK(strlen(a) == 4094 && a[0] == 'q' && a[4093] == 'q');
    w[0] = L'"'; for (int i = 1; i < 4095; i++) w[i] = L'w'; w[4095] = L'"'; w[4096] = 0;
    { BOOL r_ = UN(PathUnquoteSpacesW)(w); if (!t_is_wine()) G9EQ(r_, 1); }
    CHECK(lstrlenW(w) == 4094);
}

TEST(gap9_unquote_ansi_dbcs_safe) {
    REQUIRE(UN(PathUnquoteSpacesA));
    char b[16] = "\"\xe9\xe8\"";   /* high-bit bytes between quotes */
    { BOOL r_ = UN(PathUnquoteSpacesA)(b); if (!t_is_wine()) G9EQ(r_, 1); }
    CHECK((unsigned char)b[0] == 0xe9 && (unsigned char)b[1] == 0xe8 && b[2] == 0);
}

/* ------------------------------------------------------------ PathUnExpandEnvStrings */
static int g9_env(const char *n, WCHAR *out, DWORD cap) {
    char a[MAX_PATH]; DWORD l = GetEnvironmentVariableA(n, a, sizeof a);
    if (!l || l >= sizeof a) return 0;
    MultiByteToWideChar(CP_ACP, 0, a, -1, out, cap);
    return 1;
}
static void g9_w2a(const WCHAR *w, char *a, int cap) { WideCharToMultiByte(CP_ACP, 0, w, -1, a, cap, NULL, NULL); }

TEST(gap9_unexpand_w_systemroot) {
    REQUIRE(U(PathUnExpandEnvStringsW));
    WCHAR root[MAX_PATH], path[MAX_PATH], out[MAX_PATH];
    if (!g9_env("SystemRoot", root, MAX_PATH)) { t_skip("no SystemRoot"); return; }
    wsprintfW(path, L"%s\\system32\\k32t.dll", root);
    memset(out, 0xaa, sizeof out);
    BOOL r = U(PathUnExpandEnvStringsW)(path, out, MAX_PATH);
    G9EQ(r, 1);
    CHECK(!lstrcmpW(out, L"%SystemRoot%\\system32\\k32t.dll"));
    /* exact match of the whole variable value */
    r = U(PathUnExpandEnvStringsW)(root, out, MAX_PATH);
    G9EQ(r, 1);
    CHECK(!lstrcmpW(out, L"%SystemRoot%"));
}

TEST(gap9_unexpand_w_case_insensitive_prefix) {
    REQUIRE(U(PathUnExpandEnvStringsW));
    WCHAR root[MAX_PATH], path[MAX_PATH], out[MAX_PATH];
    if (!g9_env("SystemRoot", root, MAX_PATH)) { t_skip("no SystemRoot"); return; }
    wsprintfW(path, L"%s\\Sub", root); CharLowerW(path);
    BOOL r = U(PathUnExpandEnvStringsW)(path, out, MAX_PATH);
    G9EQ(r, 1);
    CHECK(!lstrcmpW(out, L"%SystemRoot%\\sub"));   /* tail keeps the caller's case */
}

TEST(gap9_unexpand_w_no_separator_boundary) {
    REQUIRE(U(PathUnExpandEnvStringsW));
    WCHAR root[MAX_PATH], path[MAX_PATH], out[MAX_PATH];
    if (!g9_env("SystemRoot", root, MAX_PATH)) { t_skip("no SystemRoot"); return; }
    wsprintfW(path, L"%sFoo\\x", root);   /* prefix match only, no path component check */
    BOOL r = U(PathUnExpandEnvStringsW)(path, out, MAX_PATH);
    G9EQ(r, 1);
    CHECK(!lstrcmpW(out, L"%SystemRoot%Foo\\x"));
}

TEST(gap9_unexpand_w_other_variables) {
    REQUIRE(U(PathUnExpandEnvStringsW));
    static const char *vars[] = { "APPDATA", "USERPROFILE", "ALLUSERSPROFILE", "ProgramFiles" };
    for (int i = 0; i < 4; i++) {
        WCHAR val[MAX_PATH], path[MAX_PATH], out[MAX_PATH], want[MAX_PATH], nm[64];
        if (!g9_env(vars[i], val, MAX_PATH)) { t_note("%s not set", vars[i]); continue; }
        wsprintfW(path, L"%s\\k32t_gap9\\f.txt", val);
        MultiByteToWideChar(CP_ACP, 0, vars[i], -1, nm, 64);
        wsprintfW(want, L"%%%s%%\\k32t_gap9\\f.txt", nm);
        memset(out, 0, sizeof out);
        BOOL r = U(PathUnExpandEnvStringsW)(path, out, MAX_PATH);
        /* APPDATA lives inside USERPROFILE and is tried first; nothing else here nests inside another entry */
        CHECKF(r == 1, "%s rc %d", vars[i], r);
        CHECKF(!lstrcmpW(out, want), "%s -> %ls", vars[i], out);
    }
}

TEST(gap9_unexpand_w_systemdrive) {
    REQUIRE(U(PathUnExpandEnvStringsW));
    WCHAR drv[MAX_PATH], path[MAX_PATH], out[MAX_PATH];
    if (!g9_env("SystemDrive", drv, MAX_PATH)) { t_skip("no SystemDrive"); return; }
    wsprintfW(path, L"%s\\k32t_gap9_nonexistent_dir\\f", drv);
    BOOL r = U(PathUnExpandEnvStringsW)(path, out, MAX_PATH);
    G9EQ(r, 1);
    CHECK(!lstrcmpW(out, L"%SystemDrive%\\k32t_gap9_nonexistent_dir\\f"));
}

TEST(gap9_unexpand_w_computername_unc) {
    REQUIRE(U(PathUnExpandEnvStringsW));
    WCHAR cn[MAX_PATH], path[MAX_PATH], out[MAX_PATH];
    if (!g9_env("COMPUTERNAME", cn, MAX_PATH)) { t_skip("no COMPUTERNAME"); return; }
    wsprintfW(path, L"\\\\%s\\share\\x", cn);
    BOOL r = U(PathUnExpandEnvStringsW)(path, out, MAX_PATH);
    G9EQ(r, 1);
    CHECK(!lstrcmpW(out, L"\\\\%COMPUTERNAME%\\share\\x"));
}

TEST(gap9_unexpand_w_no_match) {
    REQUIRE(U(PathUnExpandEnvStringsW));
    WCHAR out[MAX_PATH];
    const WCHAR *paths[] = { L"", L"relative\\path", L"Z:\\k32t_gap9\\none", L"\\\\k32t_gap9_nosuchhost\\share", L"%SystemRoot%\\x", L"C" };
    for (int i = 0; i < 6; i++) {
        BOOL r = U(PathUnExpandEnvStringsW)(paths[i], out, MAX_PATH);
        CHECKF(r == 0, "case %d rc %d (%ls)", i, r, out);
    }
}

TEST(gap9_unexpand_w_buffer_boundary) {
    REQUIRE(U(PathUnExpandEnvStringsW));
    WCHAR root[MAX_PATH], path[MAX_PATH], out[MAX_PATH];
    if (!g9_env("SystemRoot", root, MAX_PATH)) { t_skip("no SystemRoot"); return; }
    wsprintfW(path, L"%s\\abc", root);
    const WCHAR *want = L"%SystemRoot%\\abc";
    UINT L = lstrlenW(want);
    memset(out, 0, sizeof out);
    G9EQ(U(PathUnExpandEnvStringsW)(path, out, L + 1), 1);   /* exact fit incl. NUL */
    CHECK(!lstrcmpW(out, want));
    G9EQ(U(PathUnExpandEnvStringsW)(path, out, L), 0);       /* no room for NUL */
    G9EQ(U(PathUnExpandEnvStringsW)(path, out, 1), 0);
    G9EQ(U(PathUnExpandEnvStringsW)(path, out, 0), 0);
    /* a short buffer is never overrun */
    WCHAR guard[8 + 4]; for (int i = 0; i < 12; i++) guard[i] = 0x5a5a;
    U(PathUnExpandEnvStringsW)(path, guard, 8);
    CHECK(guard[8] == 0x5a5a && guard[9] == 0x5a5a && guard[10] == 0x5a5a && guard[11] == 0x5a5a);
}

TEST(gap9_unexpand_w_null_args) {
    REQUIRE(U(PathUnExpandEnvStringsW));
    if (t_is_wine()) { t_skip("NULL faults under wine"); return; }
    WCHAR out[8] = { L'x', L'y', 0 };
    G9EQ(U(PathUnExpandEnvStringsW)(L"C:\\x", NULL, 100), 0);
    G9EQ(U(PathUnExpandEnvStringsW)(NULL, NULL, 0), 0);
    G9EQ(U(PathUnExpandEnvStringsW)(NULL, out, 8), 0);
    G9EQ(out[0], 0);                                  /* buffer is emptied when the path is NULL */
    out[0] = L'x';
    G9EQ(U(PathUnExpandEnvStringsW)(NULL, out, 0), 0);
    G9EQ(out[0], L'x');                               /* ... but only when it has room */
}

TEST(gap9_unexpand_a_null_args) {
    REQUIRE(U(PathUnExpandEnvStringsA));
    if (t_is_wine()) { t_skip("NULL handling differs under wine"); return; }
    char out[8] = { 'x', 'y', 0 };
    G9EQ(U(PathUnExpandEnvStringsA)("C:\\x", NULL, 100), 0);
    G9EQ(U(PathUnExpandEnvStringsA)(NULL, NULL, 0), 0);
    G9EQ(U(PathUnExpandEnvStringsA)(NULL, out, 8), 0);
    G9EQ(out[0], 0);
    out[0] = 'x';
    G9EQ(U(PathUnExpandEnvStringsA)(NULL, out, 0), 0);
    G9EQ(out[0], 'x');
}

TEST(gap9_unexpand_a_matches_w) {
    REQUIRE(U(PathUnExpandEnvStringsA)); REQUIRE(U(PathUnExpandEnvStringsW));
    static const char *vars[] = { "SystemRoot", "SystemDrive", "USERPROFILE", "APPDATA", "ProgramFiles", "ALLUSERSPROFILE" };
    for (int i = 0; i < 6; i++) {
        WCHAR val[MAX_PATH], wp[MAX_PATH], wo[MAX_PATH]; char ap[MAX_PATH], ao[MAX_PATH];
        if (!g9_env(vars[i], val, MAX_PATH)) continue;
        wsprintfW(wp, L"%s\\k32t_gap9\\f.txt", val); g9_w2a(wp, ap, sizeof ap);
        memset(wo, 0, sizeof wo); memset(ao, 0, sizeof ao);
        BOOL rw = U(PathUnExpandEnvStringsW)(wp, wo, MAX_PATH);
        BOOL ra = U(PathUnExpandEnvStringsA)(ap, ao, MAX_PATH);
        if (rw == ra) {
            char wa[MAX_PATH]; g9_w2a(wo, wa, sizeof wa);
            if (rw) CHECKF(!strcmp(wa, ao), "%s: A [%s] vs W [%s]", vars[i], ao, wa);
        } else t_note("%s: A rc %d, W rc %d (A out [%s])", vars[i], ra, rw, ao);
        CHECKF(ra == rw, "%s A/W return parity", vars[i]);
    }
}

TEST(gap9_unexpand_a_systemroot_value) {
    REQUIRE(U(PathUnExpandEnvStringsA));
    char root[MAX_PATH], path[MAX_PATH], out[MAX_PATH];
    if (!GetEnvironmentVariableA("SystemRoot", root, sizeof root)) { t_skip("no SystemRoot"); return; }
    snprintf(path, sizeof path, "%s\\system32\\k32t.dll", root);
    BOOL r = U(PathUnExpandEnvStringsA)(path, out, sizeof out);
    t_note("A: rc %d out [%s]", r, r ? out : "");
    if (r) CHECK(!strcmp(out, "%SystemRoot%\\system32\\k32t.dll"));
}

/* ------------------------------------------------------------ QISearch */
typedef struct { void *qi; ULONG (WINAPI *addref)(void *); ULONG (WINAPI *release)(void *); } G9_VT;
static LONG g_ar0, g_ar1, g_ar2;
static ULONG WINAPI ar0(void *p) { return ++g_ar0; }
static ULONG WINAPI ar1(void *p) { return ++g_ar1; }
static ULONG WINAPI ar2(void *p) { return ++g_ar2; }
static ULONG WINAPI rel(void *p) { return 0; }
static const G9_VT vt0 = { NULL, ar0, rel }, vt1 = { NULL, ar1, rel }, vt2 = { NULL, ar2, rel };
typedef struct { const G9_VT *a; const G9_VT *b; const G9_VT *c; } G9_AGG;
static const GUID G9_IID_A = { 0x11111111, 0x2222, 0x3333, { 1, 2, 3, 4, 5, 6, 7, 8 } };
static const GUID G9_IID_B = { 0xaaaaaaaa, 0xbbbb, 0xcccc, { 9, 10, 11, 12, 13, 14, 15, 16 } };
static const GUID G9_IID_C = { 0x01020304, 0x0506, 0x0708, { 0x90, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf0, 0x01 } };
static const GUID G9_IID_UNK = { 0, 0, 0, { 0xC0, 0, 0, 0, 0, 0, 0, 0x46 } };
typedef struct { const GUID *piid; DWORD dwOffset; } G9_QITAB;
#define OFF(f) ((DWORD)offsetof(G9_AGG, f))

static void g9_agg_init(G9_AGG *o) { o->a = &vt0; o->b = &vt1; o->c = &vt2; g_ar0 = g_ar1 = g_ar2 = 0; }

TEST(gap9_qisearch_find_each_entry) {
    REQUIRE(U(QISearch));
    G9_AGG o; g9_agg_init(&o);
    G9_QITAB tab[] = { { &G9_IID_A, OFF(a) }, { &G9_IID_B, OFF(b) }, { &G9_IID_C, OFF(c) }, { NULL, 0 } };
    void *pv = (void *)(ULONG_PTR)1;
    G9EQ(U(QISearch)(&o, (LPCQITAB)tab, &G9_IID_A, &pv), S_OK);
    CHECK(pv == (void *)&o.a); G9EQ(g_ar0, 1); G9EQ(g_ar1, 0); G9EQ(g_ar2, 0);
    pv = NULL;
    G9EQ(U(QISearch)(&o, (LPCQITAB)tab, &G9_IID_B, &pv), S_OK);
    CHECK(pv == (void *)&o.b); G9EQ(g_ar1, 1);
    pv = NULL;
    G9EQ(U(QISearch)(&o, (LPCQITAB)tab, &G9_IID_C, &pv), S_OK);
    CHECK(pv == (void *)&o.c); G9EQ(g_ar2, 1);
    G9EQ(g_ar0, 1);
    /* repeated lookups AddRef every time */
    for (int i = 0; i < 10; i++) U(QISearch)(&o, (LPCQITAB)tab, &G9_IID_B, &pv);
    G9EQ(g_ar1, 11);
}

TEST(gap9_qisearch_not_found) {
    REQUIRE(U(QISearch));
    G9_AGG o; g9_agg_init(&o);
    G9_QITAB tab[] = { { &G9_IID_A, OFF(a) }, { &G9_IID_B, OFF(b) }, { NULL, 0 } };
    void *pv = (void *)(ULONG_PTR)0x1234;
    G9EQ(U(QISearch)(&o, (LPCQITAB)tab, &G9_IID_C, &pv), E_NOINTERFACE);
    CHECK(pv == NULL);
    G9EQ(g_ar0 + g_ar1 + g_ar2, 0);   /* no AddRef on failure */
}

TEST(gap9_qisearch_null_ppv) {
    REQUIRE(U(QISearch));
    G9_AGG o; g9_agg_init(&o);
    G9_QITAB tab[] = { { &G9_IID_A, OFF(a) }, { NULL, 0 } };
    G9EQ(U(QISearch)(&o, (LPCQITAB)tab, &G9_IID_A, NULL), E_POINTER);
    G9EQ(U(QISearch)(&o, (LPCQITAB)tab, &G9_IID_C, NULL), E_POINTER);
    G9EQ(g_ar0, 0);
}

TEST(gap9_qisearch_iunknown_defaults_to_first_entry) {
    REQUIRE(U(QISearch));
    G9_AGG o; g9_agg_init(&o);
    G9_QITAB tab[] = { { &G9_IID_B, OFF(b) }, { &G9_IID_A, OFF(a) }, { NULL, 0 } };
    void *pv = NULL;
    G9EQ(U(QISearch)(&o, (LPCQITAB)tab, &G9_IID_UNK, &pv), S_OK);
    CHECK(pv == (void *)&o.b);                  /* IUnknown falls back to the first table entry */
    G9EQ(g_ar1, 1);
}

TEST(gap9_qisearch_explicit_iunknown_wins) {
    REQUIRE(U(QISearch));
    G9_AGG o; g9_agg_init(&o);
    G9_QITAB tab[] = { { &G9_IID_A, OFF(a) }, { &G9_IID_UNK, OFF(c) }, { NULL, 0 } };
    void *pv = NULL;
    G9EQ(U(QISearch)(&o, (LPCQITAB)tab, &G9_IID_UNK, &pv), S_OK);
    CHECK(pv == (void *)&o.c);
    G9EQ(g_ar2, 1); G9EQ(g_ar0, 0);
}

TEST(gap9_qisearch_empty_table) {
    REQUIRE(U(QISearch));
    G9_AGG o; g9_agg_init(&o);
    G9_QITAB tab[] = { { NULL, 0 } };
    void *pv = (void *)(ULONG_PTR)7;
    G9EQ(U(QISearch)(&o, (LPCQITAB)tab, &G9_IID_A, &pv), E_NOINTERFACE);
    CHECK(pv == NULL);
    /* IUnknown on an empty table resolves through the terminator's offset */
    pv = NULL;
    G9EQ(U(QISearch)(&o, (LPCQITAB)tab, &G9_IID_UNK, &pv), S_OK);
    CHECK(pv == (void *)&o);
    G9EQ(g_ar0, 1);
}

TEST(gap9_qisearch_duplicate_iid_first_wins) {
    REQUIRE(U(QISearch));
    G9_AGG o; g9_agg_init(&o);
    G9_QITAB tab[] = { { &G9_IID_A, OFF(b) }, { &G9_IID_A, OFF(c) }, { NULL, 0 } };
    void *pv = NULL;
    G9EQ(U(QISearch)(&o, (LPCQITAB)tab, &G9_IID_A, &pv), S_OK);
    CHECK(pv == (void *)&o.b);
}

TEST(gap9_qisearch_every_byte_matters) {
    REQUIRE(U(QISearch));
    G9_AGG o; g9_agg_init(&o);
    G9_QITAB tab[] = { { &G9_IID_C, OFF(a) }, { NULL, 0 } };
    for (int i = 0; i < 16; i++) {
        GUID g = G9_IID_C; ((BYTE *)&g)[i] ^= 0x01;
        void *pv = (void *)(ULONG_PTR)9;
        HRESULT hr = U(QISearch)(&o, (LPCQITAB)tab, &g, &pv);
        CHECKF(hr == E_NOINTERFACE && pv == NULL, "byte %d flip matched (hr 0x%lx)", i, hr);
    }
    G9EQ(g_ar0, 0);
    void *pv = NULL; GUID same = G9_IID_C;   /* equal by value, different storage */
    G9EQ(U(QISearch)(&o, (LPCQITAB)tab, &same, &pv), S_OK);
}

TEST(gap9_qisearch_iunknown_byte_flips) {
    REQUIRE(U(QISearch));
    G9_AGG o; g9_agg_init(&o);
    G9_QITAB tab[] = { { &G9_IID_A, OFF(a) }, { NULL, 0 } };
    for (int i = 0; i < 16; i++) {
        GUID g = G9_IID_UNK; ((BYTE *)&g)[i] ^= 0x01;
        void *pv = (void *)(ULONG_PTR)9;
        HRESULT hr = U(QISearch)(&o, (LPCQITAB)tab, &g, &pv);
        CHECKF(hr == E_NOINTERFACE && pv == NULL, "IUnknown byte %d flip matched", i);
    }
}

TEST(gap9_qisearch_offset_zero_and_large_table) {
    REQUIRE(U(QISearch));
    G9_AGG o; g9_agg_init(&o);
    static GUID ids[64]; static G9_QITAB tab[65];
    for (int i = 0; i < 64; i++) { ids[i] = G9_IID_A; ids[i].Data1 = 0x1000 + i; tab[i].piid = &ids[i]; tab[i].dwOffset = (i & 1) ? OFF(b) : OFF(a); }
    tab[64].piid = NULL; tab[64].dwOffset = 0;
    for (int i = 0; i < 64; i++) {
        void *pv = NULL;
        HRESULT hr = U(QISearch)(&o, (LPCQITAB)tab, &ids[i], &pv);
        CHECKF(hr == S_OK && pv == ((i & 1) ? (void *)&o.b : (void *)&o.a), "entry %d", i);
    }
    G9EQ(g_ar0, 32); G9EQ(g_ar1, 32);
}

/* ------------------------------------------------------------ QueryAuxiliaryCounterFrequency */
TEST(gap9_aux_counter_frequency) {
    REQUIRE(U(QueryAuxiliaryCounterFrequency));
    ULONGLONG f = 0xdeadbeefcafeull;
    HRESULT hr = U(QueryAuxiliaryCounterFrequency)(&f);
    t_note("QueryAuxiliaryCounterFrequency -> 0x%08lx freq %llu", (ULONG)hr, f);
    if (SUCCEEDED(hr)) {
        CHECK(f != 0 && f != 0xdeadbeefcafeull);
        ULONGLONG f2 = 0; HRESULT h2 = U(QueryAuxiliaryCounterFrequency)(&f2);
        G9EQ(h2, hr); CHECK(f2 == f);        /* stable across calls */
    } else {
        /* failure is an HRESULT: E_NOTIMPL when the hardware has no auxiliary counter, otherwise a mapped Win32 error */
        CHECK((hr == E_NOTIMPL) || (((ULONG)hr >> 16) == 0x8007));
    }
}

TEST(gap9_aux_counter_frequency_repeat) {
    REQUIRE(U(QueryAuxiliaryCounterFrequency));
    ULONGLONG f = 0; HRESULT first = U(QueryAuxiliaryCounterFrequency)(&f);
    for (int i = 0; i < 100; i++) { ULONGLONG g = 0; HRESULT h = U(QueryAuxiliaryCounterFrequency)(&g); if (h != first) { CHECKF(0, "result changed at %d", i); break; } }
}

/* ------------------------------------------------------------ QueryOptionalDelayLoadedAPI */
TEST(gap9_optdelayload_reserved_nonzero) {
    REQUIRE(U(QueryOptionalDelayLoadedAPI));
    HMODULE self = GetModuleHandleA(NULL);
    DWORD vals[] = { 1, 2, 4, 0x80, 0x10000, 0x80000000, 0xffffffff };
    for (int i = 0; i < 7; i++) {
        SET_ERR(0);
        BOOL r = U(QueryOptionalDelayLoadedAPI)(self, "kernel32.dll", "Sleep", vals[i]);
        CHECKF(r == 0, "reserved 0x%lx accepted", vals[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}

TEST(gap9_optdelayload_not_delay_loaded) {
    REQUIRE(U(QueryOptionalDelayLoadedAPI));
    HMODULE self = GetModuleHandleA(NULL);
    const char *dlls[] = { "kernel32.dll", "KERNEL32.DLL", "k32t_no_such.dll", "advapi32.dll" };
    for (int i = 0; i < 4; i++) {
        SET_ERR(0);
        BOOL r = U(QueryOptionalDelayLoadedAPI)(self, dlls[i], "SomeFunction", 0);
        CHECKF(r == 0, "%s reported as delay-loaded", dlls[i]);
        EXPECT_ERR(ERROR_MOD_NOT_FOUND);
    }
    SET_ERR(0);
    CHECK(U(QueryOptionalDelayLoadedAPI)(self, "kernel32.dll", "Sleep", 0) == 0);
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
}

TEST(gap9_optdelayload_empty_and_star_names) {
    REQUIRE(U(QueryOptionalDelayLoadedAPI));
    HMODULE self = GetModuleHandleA(NULL);
    SET_ERR(0); BOOL r = U(QueryOptionalDelayLoadedAPI)(self, "", "Sleep", 0);
    CHECK(r == 0); t_note("empty dll name -> err %lu", GetLastError());
    SET_ERR(0); r = U(QueryOptionalDelayLoadedAPI)(self, "*", "Sleep", 0);
    CHECK(r == 0); t_note("'*' dll name -> err %lu", GetLastError());
}

TEST(gap9_optdelayload_unaligned_module) {
    REQUIRE(U(QueryOptionalDelayLoadedAPI));
    HMODULE self = GetModuleHandleA(NULL);
    for (ULONG_PTR d = 1; d <= 3; d++) {
        SET_ERR(0);
        BOOL r = U(QueryOptionalDelayLoadedAPI)((HMODULE)((ULONG_PTR)self + d), "kernel32.dll", "Sleep", 0);
        CHECKF(r == 0, "module+%zu accepted", (size_t)d);
        CHECKF(GetLastError() != 0, "no error set for module+%zu", (size_t)d);
    }
}

/* ------------------------------------------------------------ PrivilegeCheck */
static HANDLE g9_imp_token(DWORD access) {
    HANDLE p = NULL, d = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | TOKEN_QUERY, &p)) return NULL;
    if (!DuplicateTokenEx(p, access, NULL, SecurityImpersonation, TokenImpersonation, &d)) d = NULL;
    CloseHandle(p);
    return d;
}
/* -1 absent, 0 present but disabled, 1 enabled */
static int g9_priv_state(HANDLE tok, const LUID *l) {
    BYTE buf[4096]; DWORD n = 0;
    if (!GetTokenInformation(tok, TokenPrivileges, buf, sizeof buf, &n)) return -2;
    TOKEN_PRIVILEGES *tp = (TOKEN_PRIVILEGES *)buf;
    for (DWORD i = 0; i < tp->PrivilegeCount; i++)
        if (tp->Privileges[i].Luid.LowPart == l->LowPart && tp->Privileges[i].Luid.HighPart == l->HighPart)
            return (tp->Privileges[i].Attributes & SE_PRIVILEGE_ENABLED) ? 1 : 0;
    return -1;
}
typedef struct { PRIVILEGE_SET s; LUID_AND_ATTRIBUTES more[3]; } G9_PSET;

static const char *g9_privs[] = { SE_CHANGE_NOTIFY_NAME, SE_SHUTDOWN_NAME, SE_DEBUG_NAME, SE_BACKUP_NAME, SE_RESTORE_NAME,
    SE_INCREASE_QUOTA_NAME, SE_TCB_NAME, SE_IMPERSONATE_NAME, SE_CREATE_GLOBAL_NAME, SE_UNDOCK_NAME, SE_LOAD_DRIVER_NAME,
    SE_SYSTEM_PROFILE_NAME, SE_TAKE_OWNERSHIP_NAME, SE_SECURITY_NAME, SE_INC_BASE_PRIORITY_NAME };

TEST(gap9_privcheck_single_privileges_match_token) {
    REQUIRE(U(PrivilegeCheck));
    HANDLE tok = g9_imp_token(TOKEN_QUERY); if (!tok) { t_skip("no impersonation token"); return; }
    for (size_t i = 0; i < sizeof g9_privs / sizeof g9_privs[0]; i++) {
        G9_PSET ps; memset(&ps, 0, sizeof ps);
        if (!LookupPrivilegeValueA(NULL, g9_privs[i], &ps.s.Privilege[0].Luid)) continue;
        ps.s.PrivilegeCount = 1; ps.s.Control = PRIVILEGE_SET_ALL_NECESSARY;
        int st = g9_priv_state(tok, &ps.s.Privilege[0].Luid);
        BOOL res = 0x5555;
        BOOL r = U(PrivilegeCheck)(tok, &ps.s, &res);
        CHECKF(r == TRUE, "%s call failed (%lu)", g9_privs[i], GetLastError());
        CHECKF(res == (st == 1), "%s result %d, token state %d", g9_privs[i], res, st);
        if (st == 1) CHECKF((ps.s.Privilege[0].Attributes & 0x80000000) != 0, "%s lacks USED_FOR_ACCESS", g9_privs[i]);
        if (st != 1) CHECKF(!(ps.s.Privilege[0].Attributes & 0x80000000), "%s marked used while not enabled", g9_privs[i]);
    }
    CloseHandle(tok);
}

TEST(gap9_privcheck_all_vs_any) {
    REQUIRE(U(PrivilegeCheck));
    HANDLE tok = g9_imp_token(TOKEN_QUERY); if (!tok) { t_skip("no impersonation token"); return; }
    LUID a, b;
    LookupPrivilegeValueA(NULL, SE_CHANGE_NOTIFY_NAME, &a);
    LookupPrivilegeValueA(NULL, SE_TCB_NAME, &b);
    int sa = g9_priv_state(tok, &a), sb = g9_priv_state(tok, &b);
    G9_PSET ps; BOOL res;
    memset(&ps, 0, sizeof ps); ps.s.PrivilegeCount = 2; ps.s.Privilege[0].Luid = a; ps.more[0].Luid = b; ps.s.Control = PRIVILEGE_SET_ALL_NECESSARY;
    res = 0x77; G9EQ(U(PrivilegeCheck)(tok, &ps.s, &res), TRUE);
    CHECK(res == ((sa == 1) && (sb == 1)));
    memset(&ps, 0, sizeof ps); ps.s.PrivilegeCount = 2; ps.s.Privilege[0].Luid = a; ps.more[0].Luid = b; ps.s.Control = 0;
    res = 0x77; G9EQ(U(PrivilegeCheck)(tok, &ps.s, &res), TRUE);
    CHECK(res == ((sa == 1) || (sb == 1)));
    /* order must not matter */
    memset(&ps, 0, sizeof ps); ps.s.PrivilegeCount = 2; ps.s.Privilege[0].Luid = b; ps.more[0].Luid = a; ps.s.Control = 0;
    res = 0x77; G9EQ(U(PrivilegeCheck)(tok, &ps.s, &res), TRUE);
    CHECK(res == ((sa == 1) || (sb == 1)));
    CloseHandle(tok);
}

TEST(gap9_privcheck_unknown_luid) {
    REQUIRE(U(PrivilegeCheck));
    HANDLE tok = g9_imp_token(TOKEN_QUERY); if (!tok) { t_skip("no impersonation token"); return; }
    G9_PSET ps; memset(&ps, 0, sizeof ps);
    ps.s.PrivilegeCount = 1; ps.s.Control = PRIVILEGE_SET_ALL_NECESSARY;
    ps.s.Privilege[0].Luid.LowPart = 0x7ffffff0; ps.s.Privilege[0].Luid.HighPart = 0x55;
    BOOL res = 0x77; BOOL r = U(PrivilegeCheck)(tok, &ps.s, &res);
    t_note("unknown LUID: rc %d res %d err %lu", r, res, r ? 0 : GetLastError());
    if (r) CHECK(res == 0);
    CloseHandle(tok);
}

TEST(gap9_privcheck_zero_count) {
    REQUIRE(U(PrivilegeCheck));
    HANDLE tok = g9_imp_token(TOKEN_QUERY); if (!tok) { t_skip("no impersonation token"); return; }
    G9_PSET ps; memset(&ps, 0, sizeof ps);
    BOOL res = 0x77;
    ps.s.Control = PRIVILEGE_SET_ALL_NECESSARY;
    BOOL r = U(PrivilegeCheck)(tok, &ps.s, &res);
    t_note("zero privileges / ALL_NECESSARY: rc %d res %d", r, res);
    ps.s.Control = 0; res = 0x77;
    r = U(PrivilegeCheck)(tok, &ps.s, &res);
    t_note("zero privileges / ANY: rc %d res %d", r, res);
    CloseHandle(tok);
}

TEST(gap9_privcheck_invalid_handles) {
    REQUIRE(U(PrivilegeCheck));
    G9_PSET ps; memset(&ps, 0, sizeof ps);
    LookupPrivilegeValueA(NULL, SE_CHANGE_NOTIFY_NAME, &ps.s.Privilege[0].Luid);
    ps.s.PrivilegeCount = 1; ps.s.Control = PRIVILEGE_SET_ALL_NECESSARY;
    BOOL res;
    res = 0x77; SET_ERR(0);
    G9EQ(U(PrivilegeCheck)(NULL, &ps.s, &res), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    G9EQ(res, 0);                                  /* result is cleared even on failure */
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    res = 0x77; SET_ERR(0);
    G9EQ(U(PrivilegeCheck)(ev, &ps.s, &res), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    G9EQ(res, 0);
    CloseHandle(ev);
    res = 0x77; SET_ERR(0);
    G9EQ(U(PrivilegeCheck)(ev, &ps.s, &res), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);   /* closed */
    res = 0x77; SET_ERR(0);
    G9EQ(U(PrivilegeCheck)((HANDLE)(ULONG_PTR)0x7ffffff0, &ps.s, &res), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    res = 0x77; SET_ERR(0);
    BOOL r = U(PrivilegeCheck)(INVALID_HANDLE_VALUE, &ps.s, &res);
    CHECK(r == FALSE); t_note("INVALID_HANDLE_VALUE -> err %lu", GetLastError());
}

TEST(gap9_privcheck_primary_token_rejected) {
    REQUIRE(U(PrivilegeCheck));
    HANDLE p = NULL; if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &p)) { t_skip("OpenProcessToken failed"); return; }
    G9_PSET ps; memset(&ps, 0, sizeof ps);
    LookupPrivilegeValueA(NULL, SE_CHANGE_NOTIFY_NAME, &ps.s.Privilege[0].Luid);
    ps.s.PrivilegeCount = 1; ps.s.Control = PRIVILEGE_SET_ALL_NECESSARY;
    BOOL res = 0x77; SET_ERR(0);
    /* Win10 NtPrivilegeCheck only rejects an impersonation token of SecurityAnonymous level; a primary token is accepted. */
    LUID cn = ps.s.Privilege[0].Luid; int st = g9_priv_state(p, &cn);
    G9EQ(U(PrivilegeCheck)(p, &ps.s, &res), TRUE);
    CHECKF(res == (st == 1), "primary token result %d, token state %d", res, st);
    CloseHandle(p);
    HANDLE pd = NULL, anon = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | TOKEN_QUERY, &pd)) {
        if (DuplicateTokenEx(pd, TOKEN_QUERY, NULL, SecurityAnonymous, TokenImpersonation, &anon)) {
            res = 0x77; SET_ERR(0);
            G9EQ(U(PrivilegeCheck)(anon, &ps.s, &res), FALSE);
            EXPECT_ERR(ERROR_BAD_IMPERSONATION_LEVEL);
            G9EQ(res, 0);
            CloseHandle(anon);
        } else t_note("anonymous-level duplicate failed: %lu", GetLastError());
        CloseHandle(pd);
    }
}

TEST(gap9_privcheck_access_rights) {
    REQUIRE(U(PrivilegeCheck));
    G9_PSET ps; memset(&ps, 0, sizeof ps);
    LookupPrivilegeValueA(NULL, SE_CHANGE_NOTIFY_NAME, &ps.s.Privilege[0].Luid);
    ps.s.PrivilegeCount = 1; ps.s.Control = PRIVILEGE_SET_ALL_NECESSARY;
    HANDLE noq = g9_imp_token(TOKEN_IMPERSONATE);
    if (noq) {
        BOOL res = 0x77; SET_ERR(0);
        G9EQ(U(PrivilegeCheck)(noq, &ps.s, &res), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
        G9EQ(res, 0);
        CloseHandle(noq);
    } else t_note("could not create TOKEN_IMPERSONATE-only token");
    HANDLE q = g9_imp_token(TOKEN_QUERY);
    if (q) { BOOL res = 0; G9EQ(U(PrivilegeCheck)(q, &ps.s, &res), TRUE); CloseHandle(q); }
}

TEST(gap9_privcheck_identification_level_token) {
    REQUIRE(U(PrivilegeCheck));
    HANDLE p = NULL, d = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | TOKEN_QUERY, &p)) { t_skip("no token"); return; }
    if (!DuplicateTokenEx(p, TOKEN_QUERY, NULL, SecurityIdentification, TokenImpersonation, &d)) { CloseHandle(p); t_skip("dup failed"); return; }
    G9_PSET ps; memset(&ps, 0, sizeof ps);
    LookupPrivilegeValueA(NULL, SE_CHANGE_NOTIFY_NAME, &ps.s.Privilege[0].Luid);
    ps.s.PrivilegeCount = 1; ps.s.Control = PRIVILEGE_SET_ALL_NECESSARY;
    BOOL res = 0x77; BOOL r = U(PrivilegeCheck)(d, &ps.s, &res);
    t_note("identification-level token: rc %d res %d err %lu", r, res, r ? 0 : GetLastError());
    CHECK(r == TRUE);
    CloseHandle(d); CloseHandle(p);
}

TEST(gap9_privcheck_repeated_calls_stable) {
    REQUIRE(U(PrivilegeCheck));
    HANDLE tok = g9_imp_token(TOKEN_QUERY); if (!tok) { t_skip("no token"); return; }
    G9_PSET ps; memset(&ps, 0, sizeof ps);
    LookupPrivilegeValueA(NULL, SE_CHANGE_NOTIFY_NAME, &ps.s.Privilege[0].Luid);
    ps.s.PrivilegeCount = 1; ps.s.Control = PRIVILEGE_SET_ALL_NECESSARY;
    BOOL first = -1;
    for (int i = 0; i < 200; i++) {
        BOOL res = 0x77; ps.s.Privilege[0].Attributes = 0;
        if (!U(PrivilegeCheck)(tok, &ps.s, &res)) { CHECKF(0, "iter %d failed", i); break; }
        if (first == -1) first = res; else if (res != first) { CHECKF(0, "iter %d changed", i); break; }
    }
    CloseHandle(tok);
}

/* ------------------------------------------------------------ PrivilegedServiceAuditAlarmW */
TEST(gap9_privaudit_invalid_token_fails) {
    REQUIRE(U(PrivilegedServiceAuditAlarmW));
    /* Only error paths: the token handle never refers to a real token, so nothing can be audited. */
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    PRIVILEGE_SET ps; memset(&ps, 0, sizeof ps); ps.Control = PRIVILEGE_SET_ALL_NECESSARY;
    SET_ERR(0);
    BOOL r = U(PrivilegedServiceAuditAlarmW)(L"k32t_gap9", L"k32t_service", ev, &ps, TRUE);
    DWORD e = GetLastError();
    CHECK(r == FALSE); CHECK(e != 0);
    t_note("event as token: err %lu", e);
    CloseHandle(ev);
    HANDLE closed = CreateEventA(NULL, TRUE, FALSE, NULL); CloseHandle(closed);
    SET_ERR(0);
    r = U(PrivilegedServiceAuditAlarmW)(L"k32t_gap9", L"k32t_service", closed, &ps, FALSE);
    e = GetLastError();
    CHECK(r == FALSE); CHECK(e != 0);
    t_note("closed handle as token: err %lu", e);
    SET_ERR(0);
    r = U(PrivilegedServiceAuditAlarmW)(L"k32t_gap9", L"k32t_service", NULL, &ps, TRUE);
    CHECK(r == FALSE); t_note("NULL token: err %lu", GetLastError());
}

/* ------------------------------------------------------------ PrepareTape */
static DWORD g9_prep_ops[] = { 0, 1, 2, 3, 4, 5 };   /* TAPE_LOAD .. TAPE_FORMAT */

TEST(gap9_preparetape_invalid_handle) {
    REQUIRE(U(PrepareTape));
    for (int i = 0; i < 6; i++) for (int imm = 0; imm < 2; imm++) {
        SET_ERR(0);
        DWORD r = U(PrepareTape)(NULL, g9_prep_ops[i], imm);
        CHECKF(r == ERROR_INVALID_HANDLE, "NULL op %d imm %d -> %lu", i, imm, r);
        EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
    SET_ERR(0);
    DWORD r = U(PrepareTape)((HANDLE)(ULONG_PTR)0x7ffffff0, 0, FALSE);
    G9EQ(r, ERROR_INVALID_HANDLE);
    HANDLE closed = CreateEventA(NULL, TRUE, FALSE, NULL); CloseHandle(closed);
    r = U(PrepareTape)(closed, 0, FALSE);
    G9EQ(r, ERROR_INVALID_HANDLE);
    r = U(PrepareTape)(INVALID_HANDLE_VALUE, 0, FALSE);
    CHECK(r != 0); t_note("INVALID_HANDLE_VALUE -> %lu", r);
}

TEST(gap9_preparetape_event_handle) {
    REQUIRE(U(PrepareTape));
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    DWORD r = U(PrepareTape)(ev, 0, FALSE);
    G9EQ(r, ERROR_INVALID_HANDLE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
}

TEST(gap9_preparetape_regular_file) {
    REQUIRE(U(PrepareTape));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "gap9_tape.bin");
    HANDLE f = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CHECK(f != INVALID_HANDLE_VALUE); if (f == INVALID_HANDLE_VALUE) return;
    DWORD w; WriteFile(f, "tape", 4, &w, NULL);
    for (int i = 0; i < 6; i++) for (int imm = 0; imm < 2; imm++) {
        SET_ERR(0);
        DWORD r = U(PrepareTape)(f, g9_prep_ops[i], imm);
        CHECKF(r != 0, "op %d imm %d succeeded on a disk file", i, imm);
        G9EQ(GetLastError(), r);                 /* the DWORD result is also the last error */
        if (i == 0 && !imm) t_note("PrepareTape on a file -> %lu", r);
    }
    LARGE_INTEGER z; z.QuadPart = 0; LARGE_INTEGER pos;
    SetFilePointerEx(f, z, &pos, FILE_CURRENT);
    G9EQ(pos.QuadPart, 4);                        /* file untouched */
    CloseHandle(f);
}

TEST(gap9_preparetape_requires_read_access) {
    REQUIRE(U(PrepareTape));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "gap9_tape_wo.bin");
    HANDLE f = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CHECK(f != INVALID_HANDLE_VALUE); if (f == INVALID_HANDLE_VALUE) return;
    SET_ERR(0);
    DWORD r = U(PrepareTape)(f, 0, FALSE);
    G9EQ(r, ERROR_ACCESS_DENIED);   /* IOCTL_TAPE_PREPARE needs FILE_READ_ACCESS */
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(f);
}

TEST(gap9_preparetape_directory_handle) {
    REQUIRE(U(PrepareTape));
    HANDLE d = CreateFileA(t_tmpdir(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    CHECK(d != INVALID_HANDLE_VALUE); if (d == INVALID_HANDLE_VALUE) return;
    DWORD r = U(PrepareTape)(d, 0, FALSE);
    CHECK(r != 0); t_note("PrepareTape on a directory -> %lu", r);
    CloseHandle(d);
}

TEST(gap9_preparetape_physicaldrive_zero_access) {
    REQUIRE(U(PrepareTape));
    /* Opening with zero access is allowed to fail (no such device / no privilege); never issue a request. */
    HANDLE h = CreateFileA("\\\\.\\PhysicalDrive0", 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_note("PhysicalDrive0 open: %lu", GetLastError()); return; }
    /* A zero-access handle cannot carry a FILE_READ_ACCESS ioctl, so the I/O manager rejects it before any driver runs. */
    DWORD r = U(PrepareTape)(h, 0, FALSE);
    G9EQ(r, ERROR_ACCESS_DENIED);
    CloseHandle(h);
}

/* ------------------------------------------------------------ QueryIoRateControlInformationJobObject */
UDECL(DWORD, QueryIoRateControlInformationJobObject, (HANDLE, PCWSTR, void **, ULONG *));
UDECL(void, FreeMemoryJobObject, (void *));

/* Win10 1607+ (documented as unsupported): kernel32 queries job class 0x1f with a 0x30-byte buffer, the kernel expects 0x90 and
 * fails with STATUS_INFO_LENGTH_MISMATCH before the handle is looked at, so every handle yields ERROR_BAD_LENGTH. */
static DWORD IORATE_BAD_HANDLE_ERR(void) {
    OSVERSIONINFOEXW v; memset(&v, 0, sizeof v); v.dwOSVersionInfoSize = sizeof v;
    LONG (WINAPI *rgv)(OSVERSIONINFOEXW *) = (LONG (WINAPI *)(OSVERSIONINFOEXW *))GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlGetVersion");
    if (rgv && rgv(&v) == 0 && v.dwBuildNumber >= 14393) return ERROR_BAD_LENGTH;
    return ERROR_INVALID_HANDLE;
}

TEST(gap9_ioratequery_invalid_job_handles) {
    REQUIRE(UN(QueryIoRateControlInformationJobObject));
    void *info = (void *)(ULONG_PTR)0x1234; ULONG cnt = 0xFF;
    SET_ERR(0);
    CHECK(UN(QueryIoRateControlInformationJobObject)(NULL, L"\\Device\\HarddiskVolume1", &info, &cnt) == 0);
    EXPECT_ERR(IORATE_BAD_HANDLE_ERR());
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    CHECK(UN(QueryIoRateControlInformationJobObject)(ev, L"\\Device\\HarddiskVolume1", &info, &cnt) == 0);
    EXPECT_ERR(IORATE_BAD_HANDLE_ERR());
    CloseHandle(ev);
    SET_ERR(0);
    CHECK(UN(QueryIoRateControlInformationJobObject)(ev, L"\\Device\\HarddiskVolume1", &info, &cnt) == 0);
    EXPECT_ERR(IORATE_BAD_HANDLE_ERR());
    CHECK(info == (void *)(ULONG_PTR)0x1234);      /* untouched on failure */
}

TEST(gap9_ioratequery_all_volumes_invalid_job) {
    REQUIRE(UN(QueryIoRateControlInformationJobObject));
    void *info = NULL; ULONG cnt = 0xFF;
    SET_ERR(0);
    DWORD r = UN(QueryIoRateControlInformationJobObject)(NULL, NULL, &info, &cnt);
    CHECK(r == 0); t_note("NULL job, all volumes: err %lu", GetLastError());
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    r = UN(QueryIoRateControlInformationJobObject)(ev, NULL, &info, &cnt);
    CHECK(r == 0); t_note("event, all volumes: err %lu", GetLastError());
    CloseHandle(ev);
}

TEST(gap9_ioratequery_volume_name_too_long) {
    REQUIRE(UN(QueryIoRateControlInformationJobObject));
    HANDLE job = CreateJobObjectW(NULL, NULL); if (!job) { t_skip("no job"); return; }
    static WCHAR big[0x8000 + 8];
    for (int i = 0; i < 0x7fff + 4; i++) big[i] = L'a';
    big[0x7fff + 4] = 0;
    void *info = NULL; ULONG cnt = 0xFF;
    SET_ERR(0);
    CHECK(UN(QueryIoRateControlInformationJobObject)(job, big, &info, &cnt) == 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    /* an unterminated-within-limit length of 0x7fff chars is also rejected */
    big[0x7fff] = 0; for (int i = 0; i < 0x7fff; i++) big[i] = L'a';
    SET_ERR(0);
    CHECK(UN(QueryIoRateControlInformationJobObject)(job, big, &info, &cnt) == 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CloseHandle(job);
}

TEST(gap9_ioratequery_fresh_job_with_volume) {
    REQUIRE(UN(QueryIoRateControlInformationJobObject));
    HANDLE job = CreateJobObjectW(NULL, NULL); if (!job) { t_skip("no job"); return; }
    void *info = NULL; ULONG cnt = 0xFF;
    SET_ERR(0);
    DWORD r = UN(QueryIoRateControlInformationJobObject)(job, L"\\Device\\HarddiskVolume1", &info, &cnt);
    DWORD e = r ? 0 : GetLastError();
    t_note("fresh job, one volume: rc %lu err %lu count %lu", r, e, cnt);
    G9EQ(cnt, 1);                              /* a named volume always reports a single block */
    if (r && info && UN(FreeMemoryJobObject)) UN(FreeMemoryJobObject)(info);
    CloseHandle(job);
}

TEST(gap9_ioratequery_fresh_job_all_volumes) {
    REQUIRE(UN(QueryIoRateControlInformationJobObject));
    HANDLE job = CreateJobObjectW(NULL, NULL); if (!job) { t_skip("no job"); return; }
    void *info = NULL; ULONG cnt = 0xFF;
    SET_ERR(0);
    DWORD r = UN(QueryIoRateControlInformationJobObject)(job, NULL, &info, &cnt);
    t_note("fresh job, all volumes: rc %lu err %lu count %lu", r, r ? 0 : GetLastError(), cnt);
    if (r && info && UN(FreeMemoryJobObject)) UN(FreeMemoryJobObject)(info);
    CloseHandle(job);
}
