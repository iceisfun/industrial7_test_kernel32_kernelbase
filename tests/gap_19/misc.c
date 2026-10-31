#include "t.h"
/* gap19: private namespaces, boundary integrity labels, shlwapi-style Path* exports
 * living in kernelbase, DEP policy, named pipe client name, LocalShrink,
 * MapViewOfFile3FromApp, SetDefaultCommConfigW. */

UDECL(HANDLE, OpenPrivateNamespaceW, (LPVOID, LPCWSTR));
UDECL(BOOL, AddIntegrityLabelToBoundaryDescriptor, (HANDLE *, PSID));
UDECL(BOOL, PathCanonicalizeW, (LPWSTR, LPCWSTR));
UDECL(LPSTR, PathCombineA, (LPSTR, LPCSTR, LPCSTR));
UDECL(BOOL, PathMatchSpecW, (LPCWSTR, LPCWSTR));
UDECL(HRESULT, PathCreateFromUrlAlloc, (LPCWSTR, LPWSTR *, DWORD));
UDECL(BOOL, GetProcessDEPPolicy, (HANDLE, LPDWORD, PBOOL));
UDECL(BOOL, SetDefaultCommConfigW, (LPCWSTR, LPCOMMCONFIG, DWORD));
UDECL(BOOL, GetNamedPipeClientComputerNameW, (HANDLE, LPWSTR, ULONG));
UDECL(SIZE_T, LocalShrink, (HLOCAL, UINT));
UDECL(SIZE_T, LocalCompact, (UINT));
UDECL(PVOID, MapViewOfFile3FromApp, (HANDLE, HANDLE, PVOID, ULONG64, SIZE_T, ULONG, ULONG, PVOID, ULONG));

/* ---------------- private namespaces ---------------- */
static HANDLE g19_bd(const WCHAR *name) { return CreateBoundaryDescriptorW(name, 0); }
static BOOL g19_addworld(HANDLE bd) {
    BYTE sid[SECURITY_MAX_SID_SIZE]; DWORD n = sizeof sid;
    if (!CreateWellKnownSid(WinWorldSid, NULL, sid, &n)) return FALSE;
    return AddSIDToBoundaryDescriptor(&bd, sid);
}
static void g19_nsname(WCHAR *o, const WCHAR *tag) { wsprintfW(o, L"%s%lu", tag, (unsigned long)GetCurrentProcessId()); }

TEST(gap19_opennamespace_not_existing) {
    REQUIRE(UN(OpenPrivateNamespaceW));
    WCHAR bn[64], al[64]; g19_nsname(bn, L"k32t_g19_bd_"); g19_nsname(al, L"k32t_g19_al_");
    HANDLE bd = g19_bd(bn);
    if (!bd) { t_skip("no boundary descriptor"); return; }
    g19_addworld(bd);
    SET_ERR(0);
    HANDLE h = UN(OpenPrivateNamespaceW)(bd, al);
    DWORD e = GetLastError();
    EXPECT_EQ(h, NULL);
    t_note("open nonexistent namespace -> err %lu", (unsigned long)e);
    CHECK(e != 0);
    if (h) ClosePrivateNamespace(h, 0);
    DeleteBoundaryDescriptor(bd);
}
TEST(gap19_opennamespace_create_then_open) {
    REQUIRE(UN(OpenPrivateNamespaceW));
    WCHAR bn[64], al[64], al2[64];
    g19_nsname(bn, L"k32t_g19_bd2_"); g19_nsname(al, L"k32t_g19_al2_"); g19_nsname(al2, L"k32t_g19_alo2_");
    HANDLE bd = g19_bd(bn);
    if (!bd) { t_skip("no boundary descriptor"); return; }
    if (!g19_addworld(bd)) { DeleteBoundaryDescriptor(bd); t_skip("no world sid"); return; }
    HANDLE ns = CreatePrivateNamespaceW(NULL, bd, al);
    if (!ns) { t_note("cannot create namespace, err %lu", (unsigned long)GetLastError()); DeleteBoundaryDescriptor(bd); t_skip("create denied"); return; }
    SET_ERR(0);
    HANDLE o = UN(OpenPrivateNamespaceW)(bd, al2);
    t_note("open with new alias -> %p err %lu", o, (unsigned long)GetLastError());
    CHECK(o != NULL);
    if (o) { CHECK(o != ns); ClosePrivateNamespace(o, 0); }
    /* same alias as the creating handle is already registered in this process */
    SET_ERR(0);
    HANDLE o2 = UN(OpenPrivateNamespaceW)(bd, al);
    t_note("open with same alias -> %p err %lu", o2, (unsigned long)GetLastError());
    if (o2) ClosePrivateNamespace(o2, 0);
    ClosePrivateNamespace(ns, PRIVATE_NAMESPACE_FLAG_DESTROY);
    DeleteBoundaryDescriptor(bd);
}
TEST(gap19_opennamespace_wrong_boundary) {
    REQUIRE(UN(OpenPrivateNamespaceW));
    WCHAR bn[64], bn2[64], al[64], al2[64];
    g19_nsname(bn, L"k32t_g19_bd3_"); g19_nsname(bn2, L"k32t_g19_bd3x_"); g19_nsname(al, L"k32t_g19_al3_"); g19_nsname(al2, L"k32t_g19_alo3_");
    HANDLE bd = g19_bd(bn), bd2 = g19_bd(bn2);
    if (!bd || !bd2) { t_skip("no boundary descriptor"); return; }
    g19_addworld(bd); g19_addworld(bd2);
    HANDLE ns = CreatePrivateNamespaceW(NULL, bd, al);
    if (!ns) { DeleteBoundaryDescriptor(bd); DeleteBoundaryDescriptor(bd2); t_skip("create denied"); return; }
    SET_ERR(0);
    HANDLE o = UN(OpenPrivateNamespaceW)(bd2, al2);
    t_note("open with other descriptor name -> %p err %lu", o, (unsigned long)GetLastError());
    EXPECT_EQ(o, NULL);
    if (o) ClosePrivateNamespace(o, 0);
    ClosePrivateNamespace(ns, PRIVATE_NAMESPACE_FLAG_DESTROY);
    DeleteBoundaryDescriptor(bd); DeleteBoundaryDescriptor(bd2);
}
TEST(gap19_opennamespace_null_alias) {
    REQUIRE(UN(OpenPrivateNamespaceW));
    WCHAR bn[64]; g19_nsname(bn, L"k32t_g19_bd4_");
    HANDLE bd = g19_bd(bn);
    if (!bd) { t_skip("no boundary descriptor"); return; }
    g19_addworld(bd);
    SET_ERR(0);
    HANDLE h = UN(OpenPrivateNamespaceW)(bd, NULL);
    t_note("NULL alias -> %p err %lu", h, (unsigned long)GetLastError());
    EXPECT_EQ(h, NULL);   /* nothing to open: namespace was never created */
    if (h) ClosePrivateNamespace(h, 0);
    DeleteBoundaryDescriptor(bd);
}
TEST(gap19_opennamespace_empty_boundary_no_sid) {
    REQUIRE(UN(OpenPrivateNamespaceW));
    WCHAR bn[64]; g19_nsname(bn, L"k32t_g19_bd5_");
    HANDLE bd = g19_bd(bn);
    if (!bd) { t_skip("no boundary descriptor"); return; }
    SET_ERR(0);
    HANDLE h = UN(OpenPrivateNamespaceW)(bd, L"k32t_g19_al5");
    t_note("descriptor without SIDs -> %p err %lu", h, (unsigned long)GetLastError());
    EXPECT_EQ(h, NULL);
    if (h) ClosePrivateNamespace(h, 0);
    DeleteBoundaryDescriptor(bd);
}
TEST(gap19_opennamespace_repeat_open_then_close) {
    REQUIRE(UN(OpenPrivateNamespaceW));
    WCHAR bn[64], al[64]; g19_nsname(bn, L"k32t_g19_bd6_"); g19_nsname(al, L"k32t_g19_al6_");
    HANDLE bd = g19_bd(bn);
    if (!bd) { t_skip("no boundary descriptor"); return; }
    g19_addworld(bd);
    HANDLE ns = CreatePrivateNamespaceW(NULL, bd, al);
    if (!ns) { DeleteBoundaryDescriptor(bd); t_skip("create denied"); return; }
    for (int i = 0; i < 5; i++) {
        WCHAR a2[64]; wsprintfW(a2, L"k32t_g19_rep%d_%lu", i, (unsigned long)GetCurrentProcessId());
        HANDLE o = UN(OpenPrivateNamespaceW)(bd, a2);
        CHECK(o != NULL);
        if (o) {
            EXPECT_EQ(ClosePrivateNamespace(o, 0), TRUE);
        }
    }
    ClosePrivateNamespace(ns, PRIVATE_NAMESPACE_FLAG_DESTROY);
    DeleteBoundaryDescriptor(bd);
}
TEST(gap19_opennamespace_object_in_namespace_visible_via_open) {
    REQUIRE(UN(OpenPrivateNamespaceW));
    WCHAR bn[64], al[64], al2[64];
    g19_nsname(bn, L"k32t_g19_bd7_"); g19_nsname(al, L"k32t_g19_al7_"); g19_nsname(al2, L"k32t_g19_alo7_");
    HANDLE bd = g19_bd(bn);
    if (!bd) { t_skip("no boundary descriptor"); return; }
    g19_addworld(bd);
    HANDLE ns = CreatePrivateNamespaceW(NULL, bd, al);
    if (!ns) { DeleteBoundaryDescriptor(bd); t_skip("create denied"); return; }
    WCHAR full[160]; wsprintfW(full, L"%s\\evt", al);
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, full);
    CHECK(ev != NULL);
    HANDLE o = UN(OpenPrivateNamespaceW)(bd, al2);
    if (o && ev) {
        WCHAR f2[160]; wsprintfW(f2, L"%s\\evt", al2);
        HANDLE ev2 = OpenEventW(EVENT_ALL_ACCESS, FALSE, f2);
        t_note("event via second alias -> %p err %lu", ev2, (unsigned long)GetLastError());
        CHECK(ev2 != NULL);
        if (ev2) CloseHandle(ev2);
    }
    if (o) ClosePrivateNamespace(o, 0);
    if (ev) CloseHandle(ev);
    ClosePrivateNamespace(ns, PRIVATE_NAMESPACE_FLAG_DESTROY);
    DeleteBoundaryDescriptor(bd);
}

/* ---------------- AddIntegrityLabelToBoundaryDescriptor ---------------- */
static PSID g19_label(DWORD rid) {
    SID_IDENTIFIER_AUTHORITY a = SECURITY_MANDATORY_LABEL_AUTHORITY;
    PSID s = NULL;
    if (!AllocateAndInitializeSid(&a, 1, rid, 0,0,0,0,0,0,0, &s)) return NULL;
    return s;
}
TEST(gap19_addintlabel_valid_levels) {
    REQUIRE(UN(AddIntegrityLabelToBoundaryDescriptor));
    DWORD rids[] = { SECURITY_MANDATORY_LOW_RID, SECURITY_MANDATORY_MEDIUM_RID, SECURITY_MANDATORY_HIGH_RID, SECURITY_MANDATORY_SYSTEM_RID };
    for (unsigned i = 0; i < 4; i++) {
        WCHAR bn[64]; wsprintfW(bn, L"k32t_g19_il%u_%lu", i, (unsigned long)GetCurrentProcessId());
        HANDLE bd = g19_bd(bn);
        if (!bd) { t_skip("no boundary descriptor"); return; }
        PSID s = g19_label(rids[i]);
        SET_ERR(0);
        BOOL ok = UN(AddIntegrityLabelToBoundaryDescriptor)(&bd, s);
        t_note("rid 0x%lx -> %d err %lu", (unsigned long)rids[i], ok, (unsigned long)GetLastError());
        EXPECT_EQ(ok, TRUE);
        CHECK(bd != NULL);
        FreeSid(s);
        DeleteBoundaryDescriptor(bd);
    }
}
TEST(gap19_addintlabel_non_label_sid) {
    REQUIRE(UN(AddIntegrityLabelToBoundaryDescriptor));
    WCHAR bn[64]; g19_nsname(bn, L"k32t_g19_ilw_");
    HANDLE bd = g19_bd(bn);
    if (!bd) { t_skip("no boundary descriptor"); return; }
    BYTE sid[SECURITY_MAX_SID_SIZE]; DWORD n = sizeof sid;
    CreateWellKnownSid(WinWorldSid, NULL, sid, &n);
    SET_ERR(0);
    BOOL ok = UN(AddIntegrityLabelToBoundaryDescriptor)(&bd, sid);
    t_note("world SID as integrity label -> %d err %lu", ok, (unsigned long)GetLastError());
    DeleteBoundaryDescriptor(bd);
}
TEST(gap19_addintlabel_twice) {
    REQUIRE(UN(AddIntegrityLabelToBoundaryDescriptor));
    WCHAR bn[64]; g19_nsname(bn, L"k32t_g19_il2x_");
    HANDLE bd = g19_bd(bn);
    if (!bd) { t_skip("no boundary descriptor"); return; }
    PSID s = g19_label(SECURITY_MANDATORY_LOW_RID);
    BOOL a = UN(AddIntegrityLabelToBoundaryDescriptor)(&bd, s);
    SET_ERR(0);
    BOOL b = UN(AddIntegrityLabelToBoundaryDescriptor)(&bd, s);
    t_note("first %d second %d err %lu", a, b, (unsigned long)GetLastError());
    EXPECT_EQ(a, TRUE);
    FreeSid(s);
    DeleteBoundaryDescriptor(bd);
}
TEST(gap19_addintlabel_with_sid_and_open) {
    REQUIRE(UN(AddIntegrityLabelToBoundaryDescriptor)); REQUIRE(UN(OpenPrivateNamespaceW));
    WCHAR bn[64], al[64]; g19_nsname(bn, L"k32t_g19_ilo_"); g19_nsname(al, L"k32t_g19_ilal_");
    HANDLE bd = g19_bd(bn);
    if (!bd) { t_skip("no boundary descriptor"); return; }
    g19_addworld(bd);
    PSID s = g19_label(SECURITY_MANDATORY_MEDIUM_RID);
    BOOL ok = UN(AddIntegrityLabelToBoundaryDescriptor)(&bd, s);
    EXPECT_EQ(ok, TRUE);
    /* the descriptor with a label is distinct from one without: nothing to open */
    SET_ERR(0);
    HANDLE h = UN(OpenPrivateNamespaceW)(bd, al);
    EXPECT_EQ(h, NULL);
    if (h) ClosePrivateNamespace(h, 0);
    FreeSid(s);
    DeleteBoundaryDescriptor(bd);
}

/* ---------------- PathCanonicalizeW ---------------- */
static void g19_canon(const WCHAR *in, WCHAR *out, BOOL *ok) {
    for (int i = 0; i < 8; i++) out[i] = 0xdddd;
    *ok = UN(PathCanonicalizeW)(out, in);
}
TEST(gap19_pathcanon_null_params) {
    REQUIRE(UN(PathCanonicalizeW));
    WCHAR b[MAX_PATH]; wcscpy(b, L"junk");
    SET_ERR(0);
    EXPECT_EQ(UN(PathCanonicalizeW)(b, NULL), FALSE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    EXPECT_EQ(b[0], 0);      /* output cleared before the path check */
    SET_ERR(0);
    EXPECT_EQ(UN(PathCanonicalizeW)(NULL, L"C:\\a"), FALSE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    EXPECT_EQ(UN(PathCanonicalizeW)(NULL, NULL), FALSE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap19_pathcanon_dotdot_table) {
    REQUIRE(UN(PathCanonicalizeW));
    static const struct { const WCHAR *in, *out; } t[] = {
        { L"C:\\a\\b\\..\\c", L"C:\\a\\c" },
        { L"C:\\a\\.\\b", L"C:\\a\\b" },
        { L"C:\\a\\b\\.", L"C:\\a\\b" },
        { L"C:\\a\\b\\..", L"C:\\a" },
        { L"C:\\a\\..\\..", L"C:\\" },
        { L"C:\\..", L"C:\\" },
        { L"C:\\a\\b\\..\\..\\c", L"C:\\c" },
        { L"C:\\a\\\\b", L"C:\\a\\b" },
        { L"C:/a/b", L"C:\\a\\b" },
        { L"C:\\a/b\\c", L"C:\\a\\b\\c" },
        { L"C:\\a\\b\\", L"C:\\a\\b\\" },
        { L"C:\\a\\..\\b\\", L"C:\\b\\" },
        { L"C:\\", L"C:\\" },
        { L"\\\\server\\share\\a\\..\\b", L"\\\\server\\share\\b" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        WCHAR o[MAX_PATH] = {0}; SET_ERR(0);
        BOOL ok = UN(PathCanonicalizeW)(o, t[i].in);
        if (!ok || lstrcmpW(o, t[i].out))
            t_note("canon case %u: ok=%d err=%lu", i, ok, (unsigned long)GetLastError());
        if (i == 2 || i == 7) continue;   /* trailing "." / doubled separators: observed only */
        CHECKF(ok && !lstrcmpW(o, t[i].out), "case %u", i);
    }
}
TEST(gap19_pathcanon_relative) {
    REQUIRE(UN(PathCanonicalizeW));
    static const struct { const WCHAR *in; } t[] = { {L"a\\b\\..\\c"}, {L"a\\.\\b"}, {L"..\\a"}, {L"."}, {L".."}, {L""}, {L"\\"}, {L"\\a\\..\\b"} };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        WCHAR o[MAX_PATH] = {0}; SET_ERR(0);
        BOOL ok = UN(PathCanonicalizeW)(o, t[i].in);
        t_note("canon relative '%ls' -> %d '%ls' err %lu", t[i].in, ok, ok ? o : L"", (unsigned long)GetLastError());
    }
    WCHAR o[MAX_PATH];
    EXPECT_EQ(UN(PathCanonicalizeW)(o, L"a\\b\\..\\c"), TRUE);
    EXPECT_WSTR(o, L"a\\c");
}
TEST(gap19_pathcanon_extended_prefix) {
    REQUIRE(UN(PathCanonicalizeW));
    WCHAR o[MAX_PATH] = {0};
    SET_ERR(0);
    BOOL ok = UN(PathCanonicalizeW)(o, L"\\\\?\\C:\\a\\b");
    t_note("\\\\?\\ prefix -> %d '%ls' err %lu", ok, o, (unsigned long)GetLastError());
    ok = UN(PathCanonicalizeW)(o, L"\\\\?\\C:\\a\\..\\b");
    t_note("\\\\?\\ with .. -> %d '%ls'", ok, o);
}
TEST(gap19_pathcanon_too_long) {
    REQUIRE(UN(PathCanonicalizeW));
    WCHAR in[400]; wcscpy(in, L"C:\\");
    for (int i = 0; i < 300; i++) in[3 + i] = L'a';
    in[303] = 0;
    WCHAR o[MAX_PATH]; wcscpy(o, L"zzz");
    SET_ERR(0);
    BOOL ok = UN(PathCanonicalizeW)(o, in);
    DWORD e = GetLastError();
    EXPECT_EQ(ok, FALSE);
    t_note("303-char path -> err %lu", (unsigned long)e);
    CHECK(e != 0);
}
TEST(gap19_pathcanon_max_path_boundary) {
    REQUIRE(UN(PathCanonicalizeW));
    for (int len = 250; len <= 262; len++) {
        WCHAR in[300]; wcscpy(in, L"C:\\");
        for (int i = 3; i < len; i++) in[i] = L'a';
        in[len] = 0;
        WCHAR o[MAX_PATH]; SET_ERR(0);
        BOOL ok = UN(PathCanonicalizeW)(o, in);
        t_note("len %d -> %d err %lu", len, ok, (unsigned long)GetLastError());
        if (len <= 258) CHECKF(ok, "len %d should fit", len);
        if (len >= 262) CHECKF(!ok, "len %d should not fit", len);
    }
}
TEST(gap19_pathcanon_in_place) {
    REQUIRE(UN(PathCanonicalizeW));
    WCHAR b[MAX_PATH]; wcscpy(b, L"C:\\x\\..\\y");
    SET_ERR(0);
    BOOL ok = UN(PathCanonicalizeW)(b, b);
    t_note("in-place -> %d '%ls'", ok, b);
}

/* ---------------- PathCombineA ---------------- */
TEST(gap19_pathcombine_basic_table) {
    REQUIRE(UN(PathCombineA));
    static const struct { const char *d, *f, *r; } t[] = {
        { "C:\\a", "b", "C:\\a\\b" },
        { "C:\\a\\", "b", "C:\\a\\b" },
        { "C:\\a", "b\\c", "C:\\a\\b\\c" },
        { "C:\\a", "C:\\x", "C:\\x" },
        { "C:\\a\\b", "\\c", "C:\\c" },
        { "C:\\a\\b", "..\\c", "C:\\a\\c" },
        { "C:\\a\\b", ".\\c", "C:\\a\\b\\c" },
        { "C:\\a", "..\\..\\c", "C:\\c" },
        { "\\\\srv\\sh\\d", "f", "\\\\srv\\sh\\d\\f" },
        { NULL, "rel", "rel" },
        { "rel", "x", "rel\\x" },
        { "C:\\a", "\\\\srv\\sh", "\\\\srv\\sh" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        char o[MAX_PATH]; memset(o, 'Q', sizeof o);
        LPSTR r = UN(PathCombineA)(o, t[i].d, t[i].f);
        if (r != o || strcmp(o, t[i].r))
            t_note("combine %u: ret %p '%s'", i, r, r ? o : "");
        CHECKF(r == o && !strcmp(o, t[i].r), "case %u", i);
    }
}
TEST(gap19_pathcombine_null_dest) {
    REQUIRE(UN(PathCombineA));
    EXPECT_EQ(UN(PathCombineA)(NULL, "C:\\a", "b"), NULL);
}
TEST(gap19_pathcombine_both_null) {
    REQUIRE(UN(PathCombineA));
    char o[MAX_PATH];
    LPSTR r = UN(PathCombineA)(o, NULL, NULL);
    t_note("both NULL -> %p", r);
    EXPECT_EQ(r, NULL);
}
TEST(gap19_pathcombine_null_file_or_dir) {
    REQUIRE(UN(PathCombineA));
    char o[MAX_PATH];
    LPSTR r = UN(PathCombineA)(o, "C:\\a\\b", NULL);
    t_note("file NULL -> %p '%s'", r, r ? o : "");
    if (r) EXPECT_STR(o, "C:\\a\\b");
    r = UN(PathCombineA)(o, "", "b");
    t_note("dir empty -> %p '%s'", r, r ? o : "");
    r = UN(PathCombineA)(o, "C:\\a", "");
    t_note("file empty -> %p '%s'", r, r ? o : "");
}
TEST(gap19_pathcombine_too_long) {
    REQUIRE(UN(PathCombineA));
    char d[260], f[260], o[MAX_PATH];
    strcpy(d, "C:\\"); memset(d + 3, 'a', 150); d[153] = 0;
    memset(f, 'b', 150); f[150] = 0;
    LPSTR r = UN(PathCombineA)(o, d, f);
    EXPECT_EQ(r, NULL);   /* 153 + 1 + 150 > MAX_PATH-1 */
    memset(f, 'b', 100); f[100] = 0;
    r = UN(PathCombineA)(o, d, f);
    CHECK(r == o);        /* 153 + 1 + 100 = 254 fits */
    if (r) EXPECT_EQ(strlen(o), 254);
}
TEST(gap19_pathcombine_dest_aliases_dir) {
    REQUIRE(UN(PathCombineA));
    char o[MAX_PATH]; strcpy(o, "C:\\a");
    LPSTR r = UN(PathCombineA)(o, o, "b");
    t_note("dest==dir -> %p '%s'", r, o);
    CHECK(r == o); EXPECT_STR(o, "C:\\a\\b");
}
TEST(gap19_pathcombine_drive_only) {
    REQUIRE(UN(PathCombineA));
    char o[MAX_PATH];
    LPSTR r = UN(PathCombineA)(o, "C:", "b");
    t_note("'C:' + 'b' -> %p '%s'", r, r ? o : "");
    r = UN(PathCombineA)(o, "C:\\", "b");
    CHECK(r == o); EXPECT_STR(o, "C:\\b");
    r = UN(PathCombineA)(o, "C:\\", "..");
    CHECK(r == o); EXPECT_STR(o, "C:\\");
}

/* ---------------- PathMatchSpecW ---------------- */
TEST(gap19_pathmatchspec_table) {
    REQUIRE(UN(PathMatchSpecW));
    static const struct { const WCHAR *f, *s; BOOL r; } t[] = {
        { L"a.txt", L"*.txt", TRUE }, { L"a.TXT", L"*.txt", TRUE }, { L"a.txt", L"*.TXT", TRUE },
        { L"a.txt", L"*.doc", FALSE }, { L"a.txt", L"a.*", TRUE }, { L"a.txt", L"*", TRUE },
        { L"a.txt", L"a.t?t", TRUE }, { L"a.txt", L"a.t?", FALSE }, { L"a.txt", L"?.txt", TRUE },
        { L"ab.txt", L"?.txt", FALSE }, { L"a.txt", L"a.txt", TRUE }, { L"a.txt", L"b.txt", FALSE },
        { L"a.txt", L"*.doc;*.txt", TRUE }, { L"a.txt", L"*.doc;*.xls", FALSE },
        { L"a.txt", L"*.doc; *.txt", TRUE },   /* whitespace after ';' skipped */
        { L"file.tar.gz", L"*.gz", TRUE }, { L"file.tar.gz", L"*.tar", FALSE },
        { L"x", L"x", TRUE }, { L"x", L"y", FALSE }, { L"x", L"??", FALSE }, { L"xy", L"??", TRUE },
        { L"abc", L"a*c", TRUE }, { L"abc", L"a*d", FALSE }, { L"abc", L"*b*", TRUE },
        { L"C:\\dir\\a.txt", L"*.txt", TRUE }, { L"a.txt", L"A.TXT", TRUE },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        BOOL r = UN(PathMatchSpecW)(t[i].f, t[i].s);
        CHECKF(!!r == !!t[i].r, "'%ls' vs '%ls' got %d want %d", t[i].f, t[i].s, r, t[i].r);
    }
}
TEST(gap19_pathmatchspec_special_cases) {
    REQUIRE(UN(PathMatchSpecW));
    static const struct { const WCHAR *f, *s; } t[] = {
        { L"abc", L"*.*" }, { L"a.b", L"*.*" }, { L"", L"*" }, { L"", L"" }, { L"a", L"" },
        { L"a.txt", L";" }, { L"a.txt", L";*.txt" }, { L"a.txt", L"*.txt;" },
        { L".txt", L"*.txt" }, { L"a.", L"*." }, { L"a.txt", L"**" }, { L"a.txt", L"*?*" },
        { L"a.txt", L"*.t*" }, { L"a b.txt", L"a b.*" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++)
        t_note("'%ls' vs '%ls' -> %d", t[i].f, t[i].s, UN(PathMatchSpecW)(t[i].f, t[i].s));
}
TEST(gap19_pathmatchspec_long_inputs) {
    REQUIRE(UN(PathMatchSpecW));
    WCHAR f[300];
    for (int i = 0; i < 299; i++) f[i] = L'a';
    f[295] = L'.'; f[296] = L't'; f[297] = L'x'; f[298] = L't'; f[299] = 0;
    EXPECT_EQ(UN(PathMatchSpecW)(f, L"*.txt"), TRUE);
    EXPECT_EQ(UN(PathMatchSpecW)(f, L"*.doc"), FALSE);
}
TEST(gap19_pathmatchspec_does_not_touch_lasterror) {
    REQUIRE(UN(PathMatchSpecW));
    SET_ERR(0x4321);
    UN(PathMatchSpecW)(L"a.txt", L"*.txt");
    t_note("last error after match: 0x%lx", (unsigned long)GetLastError());
}

/* ---------------- PathCreateFromUrlAlloc ---------------- */
TEST(gap19_pathfromurl_table) {
    REQUIRE(UN(PathCreateFromUrlAlloc));
    static const struct { const WCHAR *in, *out; } t[] = {
        { L"file:///C:/a/b.txt", L"C:\\a\\b.txt" },
        { L"file:///c:/dir/", L"c:\\dir\\" },
        { L"file:///C:/a%20b/c.txt", L"C:\\a b\\c.txt" },
        { L"file://server/share/x.txt", L"\\\\server\\share\\x.txt" },
        { L"FILE:///C:/Upper.TXT", L"C:\\Upper.TXT" },
        { L"file:///C:/%41%42", L"C:\\AB" },
    };
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        LPWSTR out = (LPWSTR)0x1;
        HRESULT hr = UN(PathCreateFromUrlAlloc)(t[i].in, &out, 0);
        if (hr != S_OK) t_note("case %u hr 0x%lx", i, (unsigned long)hr);
        CHECKF(hr == S_OK, "case %u hr 0x%lx", i, (unsigned long)hr);
        if (hr == S_OK) {
            CHECKF(out && !lstrcmpW(out, t[i].out), "case %u got '%ls'", i, out ? out : L"(null)");
            LocalFree(out);
        }
    }
}
TEST(gap19_pathfromurl_invalid) {
    if (t_is_wine()) { t_skip("wine deviates from Windows here"); return; }
    REQUIRE(UN(PathCreateFromUrlAlloc));
    LPWSTR out = (LPWSTR)0x1;
    HRESULT hr = UN(PathCreateFromUrlAlloc)(NULL, &out, 0);
    EXPECT_EQ((DWORD)hr, 0x80070057);
    EXPECT_EQ(out, NULL);
    out = (LPWSTR)0x1;
    hr = UN(PathCreateFromUrlAlloc)(L"http://example.com/a", &out, 0);
    EXPECT_EQ((DWORD)hr, 0x80070057);   /* E_INVALIDARG: not a file URL */
    EXPECT_EQ(out, NULL);
    out = (LPWSTR)0x1;
    hr = UN(PathCreateFromUrlAlloc)(L"", &out, 0);
    t_note("empty -> hr 0x%lx out %p", (unsigned long)hr, out);
    CHECK(hr != S_OK || out != NULL);
    if (hr == S_OK && out) LocalFree(out);
    out = (LPWSTR)0x1;
    hr = UN(PathCreateFromUrlAlloc)(L"C:\\already\\a\\path", &out, 0);
    t_note("plain path -> hr 0x%lx '%ls'", (unsigned long)hr, hr == S_OK && out ? out : L"");
    if (hr == S_OK && out) LocalFree(out);
}
TEST(gap19_pathfromurl_flags) {
    REQUIRE(UN(PathCreateFromUrlAlloc));
    DWORD flags[] = { 0, 1, 2, 0x80000000, 0xffffffff };
    for (unsigned i = 0; i < 5; i++) {
        LPWSTR out = NULL;
        HRESULT hr = UN(PathCreateFromUrlAlloc)(L"file:///C:/a%20b", &out, flags[i]);
        t_note("flags 0x%lx -> hr 0x%lx '%ls'", (unsigned long)flags[i], (unsigned long)hr, hr == S_OK && out ? out : L"");
        if (hr == S_OK && out) LocalFree(out);
    }
}
TEST(gap19_pathfromurl_pipe_and_variants) {
    REQUIRE(UN(PathCreateFromUrlAlloc));
    const WCHAR *in[] = { L"file:///C|/a/b", L"file:C:/a/b", L"file://localhost/C:/a/b", L"file:////server/share/f", L"file:///C:/a/b?x=1", L"file:///C:/a/b#frag" };
    for (unsigned i = 0; i < 6; i++) {
        LPWSTR out = NULL;
        HRESULT hr = UN(PathCreateFromUrlAlloc)(in[i], &out, 0);
        t_note("'%ls' -> hr 0x%lx '%ls'", in[i], (unsigned long)hr, hr == S_OK && out ? out : L"");
        if (hr == S_OK && out) LocalFree(out);
    }
}
TEST(gap19_pathfromurl_result_is_localalloc) {
    REQUIRE(UN(PathCreateFromUrlAlloc));
    LPWSTR out = NULL;
    HRESULT hr = UN(PathCreateFromUrlAlloc)(L"file:///C:/x", &out, 0);
    EXPECT_EQ(hr, S_OK);
    if (out) {
        CHECK(LocalSize(out) >= (lstrlenW(out) + 1) * sizeof(WCHAR));
        HLOCAL fr = LocalFree(out);
        EXPECT_EQ(fr, NULL);
    }
}
TEST(gap19_pathfromurl_long) {
    REQUIRE(UN(PathCreateFromUrlAlloc));
    WCHAR in[600]; wcscpy(in, L"file:///C:/");
    int n = lstrlenW(in);
    for (int i = 0; i < 400; i++) in[n + i] = L'a';
    in[n + 400] = 0;
    LPWSTR out = NULL;
    HRESULT hr = UN(PathCreateFromUrlAlloc)(in, &out, 0);
    t_note("400-char file component -> hr 0x%lx len %d", (unsigned long)hr, out ? lstrlenW(out) : -1);
    if (hr == S_OK && out) { EXPECT_EQ(lstrlenW(out), 3 + 400); LocalFree(out); }
}

/* ---------------- GetProcessDEPPolicy ---------------- */
TEST(gap19_depolicy_current_process) {
    REQUIRE(UN(GetProcessDEPPolicy));
    DWORD fl = 0xdeadbeef; BOOL perm = 0x55;
    SET_ERR(0);
    BOOL ok = UN(GetProcessDEPPolicy)(GetCurrentProcess(), &fl, &perm);
    DWORD e = GetLastError();
    t_note("ret %d flags 0x%lx permanent %d err %lu", ok, (unsigned long)fl, perm, (unsigned long)e);
    EXPECT_EQ(ok, TRUE);   /* ProcessExecuteFlags query works for the current process */
    if (ok) {
        CHECK(fl == 0 || fl == 1 || fl == 3);   /* decomp: 0 disabled, 1 enabled, 3 enabled+ATL thunk off */
    } else {
        EXPECT_EQ(fl, 0xdeadbeef);    /* outputs written only on success */
        EXPECT_EQ(perm, 0x55);
    }
}
TEST(gap19_depolicy_real_handle_same_as_pseudo) {
    REQUIRE(UN(GetProcessDEPPolicy));
    HANDLE h = NULL;
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(), &h, PROCESS_QUERY_INFORMATION, FALSE, 0)) { t_skip("dup failed"); return; }
    DWORD f1 = 7, f2 = 9; BOOL p1 = 1, p2 = 1;
    BOOL a = UN(GetProcessDEPPolicy)(GetCurrentProcess(), &f1, &p1);
    BOOL b = UN(GetProcessDEPPolicy)(h, &f2, &p2);
    EXPECT_EQ(a, b);
    if (a && b) EXPECT_EQ(f1, f2);
    CloseHandle(h);
}
TEST(gap19_depolicy_null_handle) {
    if (t_is_wine()) { t_skip("wine deviates from Windows here"); return; }
    REQUIRE(UN(GetProcessDEPPolicy));
    DWORD fl = 0xdeadbeef; BOOL perm = 0x55;
    SET_ERR(0);
    EXPECT_EQ(UN(GetProcessDEPPolicy)(NULL, &fl, &perm), FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    EXPECT_EQ(fl, 0xdeadbeef);
    EXPECT_EQ(perm, 0x55);
}
TEST(gap19_depolicy_closed_handle) {
    if (t_is_wine()) { t_skip("wine deviates from Windows here"); return; }
    REQUIRE(UN(GetProcessDEPPolicy));
    HANDLE h = NULL;
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(), &h, PROCESS_QUERY_INFORMATION, FALSE, 0)) { t_skip("dup failed"); return; }
    CloseHandle(h);
    DWORD fl = 1; BOOL perm = 1;
    SET_ERR(0);
    EXPECT_EQ(UN(GetProcessDEPPolicy)(h, &fl, &perm), FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    EXPECT_EQ(fl, 1);
}
TEST(gap19_depolicy_non_process_handle) {
    if (t_is_wine()) { t_skip("wine deviates from Windows here"); return; }
    REQUIRE(UN(GetProcessDEPPolicy));
    HANDLE ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    DWORD fl = 1; BOOL perm = 1;
    SET_ERR(0);
    EXPECT_EQ(UN(GetProcessDEPPolicy)(ev, &fl, &perm), FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    t_note("event handle -> err %lu", (unsigned long)GetLastError());
    CloseHandle(ev);
}
TEST(gap19_depolicy_handle_without_query_right) {
    REQUIRE(UN(GetProcessDEPPolicy));
    HANDLE h = NULL;
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(), &h, SYNCHRONIZE, FALSE, 0)) { t_skip("dup failed"); return; }
    DWORD fl = 1; BOOL perm = 1;
    SET_ERR(0);
    BOOL ok = UN(GetProcessDEPPolicy)(h, &fl, &perm);
    t_note("SYNCHRONIZE-only handle -> %d err %lu", ok, (unsigned long)GetLastError());
    if (!ok) EXPECT_EQ(fl, 1);
    CloseHandle(h);
}
TEST(gap19_depolicy_child_process) {
    REQUIRE(UN(GetProcessDEPPolicy));
    STARTUPINFOA si = { sizeof si }; PROCESS_INFORMATION pi;
    PROCESS_INFORMATION pi2;
    HANDLE ch = t_spawn_child("g19_sleeper", "", CREATE_SUSPENDED, NULL, NULL, NULL, FALSE, NULL, &pi2);
    if (!ch) { t_skip("spawn failed"); return; }
    DWORD fl = 0; BOOL perm = 0;
    BOOL ok = UN(GetProcessDEPPolicy)(pi2.hProcess, &fl, &perm);
    t_note("child process -> %d flags 0x%lx perm %d", ok, (unsigned long)fl, perm);
    TerminateProcess(pi2.hProcess, 0);
    WaitForSingleObject(pi2.hProcess, 5000);
    CloseHandle(pi2.hThread); CloseHandle(pi2.hProcess);
}
CHILD(g19_sleeper) { Sleep(5000); return 0; }

/* ---------------- SetDefaultCommConfigW ---------------- */
TEST(gap19_setdefaultcommconfig_unknown_port) {
    if (t_is_wine()) { t_skip("wine deviates from Windows here"); return; }
    REQUIRE(UN(SetDefaultCommConfigW));
    COMMCONFIG cc; memset(&cc, 0, sizeof cc); cc.dwSize = sizeof cc;
    SET_ERR(0x1234);
    BOOL ok = UN(SetDefaultCommConfigW)(L"k32t_nosuchport", &cc, sizeof cc);
    EXPECT_EQ(ok, FALSE);
    t_note("unknown port -> err 0x%lx", (unsigned long)GetLastError());
}
TEST(gap19_setdefaultcommconfig_empty_name) {
    if (t_is_wine()) { t_skip("wine deviates from Windows here"); return; }
    REQUIRE(UN(SetDefaultCommConfigW));
    COMMCONFIG cc; memset(&cc, 0, sizeof cc); cc.dwSize = sizeof cc;
    SET_ERR(0x1234);
    BOOL ok = UN(SetDefaultCommConfigW)(L"", &cc, sizeof cc);
    EXPECT_EQ(ok, FALSE);
    t_note("empty name -> err 0x%lx", (unsigned long)GetLastError());
}
TEST(gap19_setdefaultcommconfig_non_com_names) {
    if (t_is_wine()) { t_skip("wine deviates from Windows here"); return; }
    REQUIRE(UN(SetDefaultCommConfigW));
    COMMCONFIG cc; memset(&cc, 0, sizeof cc); cc.dwSize = sizeof cc;
    const WCHAR *names[] = { L"LPT1X", L"COM", L"COMx", L"NUL", L"C:\\", L"\\\\.\\k32t_nodev" };
    for (unsigned i = 0; i < 6; i++) {
        SET_ERR(0);
        BOOL ok = UN(SetDefaultCommConfigW)(names[i], &cc, sizeof cc);
        t_note("'%ls' -> %d err %lu", names[i], ok, (unsigned long)GetLastError());
        EXPECT_EQ(ok, FALSE);
    }
}
/* note: a real COMn name would load serialui.dll and persist port defaults, so it is not exercised. */

/* ---------------- GetNamedPipeClientComputerNameW ---------------- */
static HANDLE g19_pipe_server(WCHAR *name, size_t cap) {
    wsprintfW(name, L"\\\\.\\pipe\\k32t_g19_cn_%lu_%lu", (unsigned long)GetCurrentProcessId(), (unsigned long)GetTickCount());
    return CreateNamedPipeW(name, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512, 0, NULL);
}
TEST(gap19_pipeclientname_local_client) {
    if (t_is_wine()) { t_skip("GetNamedPipeClientComputerNameW unimplemented in wine"); return; }
    REQUIRE(UN(GetNamedPipeClientComputerNameW));
    WCHAR nm[128]; HANDLE s = g19_pipe_server(nm, 128);
    if (s == INVALID_HANDLE_VALUE) { t_skip("no pipe"); return; }
    HANDLE c = CreateFileW(nm, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(c != INVALID_HANDLE_VALUE);
    ConnectNamedPipe(s, NULL);
    WCHAR b[64] = L"unset";
    SET_ERR(0);
    BOOL ok = UN(GetNamedPipeClientComputerNameW)(s, b, 64);
    DWORD e = GetLastError();
    EXPECT_EQ(ok, FALSE);
    EXPECT_EQ(e, ERROR_PIPE_LOCAL);       /* 229: client is on this machine (decomp maps NOT_FOUND to it) */
    CloseHandle(c); CloseHandle(s);
}
TEST(gap19_pipeclientname_small_or_null_buffer_local) {
    if (t_is_wine()) { t_skip("GetNamedPipeClientComputerNameW unimplemented in wine"); return; }
    REQUIRE(UN(GetNamedPipeClientComputerNameW));
    WCHAR nm[128]; HANDLE s = g19_pipe_server(nm, 128);
    if (s == INVALID_HANDLE_VALUE) { t_skip("no pipe"); return; }
    HANDLE c = CreateFileW(nm, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    ConnectNamedPipe(s, NULL);
    SET_ERR(0);
    BOOL ok = UN(GetNamedPipeClientComputerNameW)(s, NULL, 0);
    EXPECT_EQ(ok, FALSE);
    t_note("NULL buf len 0 -> err %lu", (unsigned long)GetLastError());
    WCHAR b[2];
    SET_ERR(0);
    ok = UN(GetNamedPipeClientComputerNameW)(s, b, 1);
    EXPECT_EQ(ok, FALSE);
    t_note("len 1 -> err %lu", (unsigned long)GetLastError());
    CloseHandle(c); CloseHandle(s);
}
TEST(gap19_pipeclientname_client_end) {
    if (t_is_wine()) { t_skip("GetNamedPipeClientComputerNameW unimplemented in wine"); return; }
    REQUIRE(UN(GetNamedPipeClientComputerNameW));
    WCHAR nm[128]; HANDLE s = g19_pipe_server(nm, 128);
    if (s == INVALID_HANDLE_VALUE) { t_skip("no pipe"); return; }
    HANDLE c = CreateFileW(nm, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    ConnectNamedPipe(s, NULL);
    WCHAR b[64];
    SET_ERR(0);
    BOOL ok = UN(GetNamedPipeClientComputerNameW)(c, b, 64);
    EXPECT_EQ(ok, FALSE);
    t_note("client end -> err %lu", (unsigned long)GetLastError());
    CloseHandle(c); CloseHandle(s);
}
TEST(gap19_pipeclientname_not_connected) {
    if (t_is_wine()) { t_skip("GetNamedPipeClientComputerNameW unimplemented in wine"); return; }
    REQUIRE(UN(GetNamedPipeClientComputerNameW));
    WCHAR nm[128]; HANDLE s = g19_pipe_server(nm, 128);
    if (s == INVALID_HANDLE_VALUE) { t_skip("no pipe"); return; }
    WCHAR b[64];
    SET_ERR(0);
    BOOL ok = UN(GetNamedPipeClientComputerNameW)(s, b, 64);
    EXPECT_EQ(ok, FALSE);
    t_note("no client yet -> err %lu", (unsigned long)GetLastError());
    CHECK(GetLastError() != 0);
    CloseHandle(s);
}
TEST(gap19_pipeclientname_after_disconnect) {
    if (t_is_wine()) { t_skip("GetNamedPipeClientComputerNameW unimplemented in wine"); return; }
    REQUIRE(UN(GetNamedPipeClientComputerNameW));
    WCHAR nm[128]; HANDLE s = g19_pipe_server(nm, 128);
    if (s == INVALID_HANDLE_VALUE) { t_skip("no pipe"); return; }
    HANDLE c = CreateFileW(nm, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    ConnectNamedPipe(s, NULL);
    CloseHandle(c);
    WCHAR b[64];
    SET_ERR(0);
    BOOL ok = UN(GetNamedPipeClientComputerNameW)(s, b, 64);
    EXPECT_EQ(ok, FALSE);
    t_note("client gone -> err %lu", (unsigned long)GetLastError());
    CloseHandle(s);
}
TEST(gap19_pipeclientname_invalid_handles) {
    if (t_is_wine()) { t_skip("GetNamedPipeClientComputerNameW unimplemented in wine"); return; }
    REQUIRE(UN(GetNamedPipeClientComputerNameW));
    WCHAR b[64];
    SET_ERR(0);
    EXPECT_EQ(UN(GetNamedPipeClientComputerNameW)(NULL, b, 64), FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    EXPECT_EQ(UN(GetNamedPipeClientComputerNameW)(INVALID_HANDLE_VALUE, b, 64), FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap19_pipeclientname_non_pipe_handle) {
    if (t_is_wine()) { t_skip("GetNamedPipeClientComputerNameW unimplemented in wine"); return; }
    REQUIRE(UN(GetNamedPipeClientComputerNameW));
    WCHAR b[64];
    HANDLE ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    SET_ERR(0);
    EXPECT_EQ(UN(GetNamedPipeClientComputerNameW)(ev, b, 64), FALSE);
    t_note("event -> err %lu", (unsigned long)GetLastError());
    CloseHandle(ev);
    char f[MAX_PATH]; t_tmpfile(f, sizeof f, "notapipe.bin");
    HANDLE h = CreateFileA(f, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    SET_ERR(0);
    EXPECT_EQ(UN(GetNamedPipeClientComputerNameW)(h, b, 64), FALSE);
    t_note("file -> err %lu", (unsigned long)GetLastError());
    CloseHandle(h);
    DeleteFileA(f);
}

/* ---------------- LocalShrink ---------------- */
TEST(gap19_localshrink_same_entry_as_compact) {
    REQUIRE(UN(LocalShrink));
    if (t_is_wine()) { t_skip("alias layout differs under wine"); return; }
    FARPROC a = (FARPROC)UK(LocalShrink), b = t_proc_k32("LocalCompact"), c = t_proc_k32("GlobalCompact");
    EXPECT_EQ(a, b);
    EXPECT_EQ(a, c);
}
TEST(gap19_localshrink_preserves_block) {
    REQUIRE(UN(LocalShrink));
    char *p = (char *)LocalAlloc(LMEM_FIXED, 256);
    CHECK(p != NULL); if (!p) return;
    for (int i = 0; i < 256; i++) p[i] = (char)i;
    UN(LocalShrink)((HLOCAL)p, 16);
    int ok = 1; for (int i = 0; i < 256; i++) if (p[i] != (char)i) ok = 0;
    CHECK(ok);
    EXPECT_EQ(LocalSize(p), 256);       /* "shrink" never resizes the block */
    LocalFree(p);
}
TEST(gap19_localshrink_moveable_handle) {
    REQUIRE(UN(LocalShrink));
    HLOCAL h = LocalAlloc(LMEM_MOVEABLE, 128);
    CHECK(h != NULL); if (!h) return;
    char *p = LocalLock(h); memset(p, 0x5a, 128); LocalUnlock(h);
    UN(LocalShrink)(h, 0);
    p = LocalLock(h);
    CHECK(p && p[0] == 0x5a && p[127] == 0x5a);
    LocalUnlock(h);
    EXPECT_EQ(LocalSize(h), 128);
    LocalFree(h);
}
TEST(gap19_localshrink_invalid_handles_harmless) {
    REQUIRE(UN(LocalShrink));
    /* decomp: the arguments are ignored (RtlCompactHeap(BaseHeap,0)) */
    SIZE_T a = UN(LocalShrink)(NULL, 0);
    SIZE_T b = UN(LocalShrink)((HLOCAL)(ULONG_PTR)1, 0xffffffff);
    SIZE_T c = UN(LocalShrink)(INVALID_HANDLE_VALUE, 0x10000);
    t_note("returns %llu %llu %llu", (unsigned long long)a, (unsigned long long)b, (unsigned long long)c);
    char *p = LocalAlloc(LMEM_FIXED, 32);
    CHECK(p != NULL);
    if (p) LocalFree(p);
}
TEST(gap19_localshrink_does_not_set_error) {
    REQUIRE(UN(LocalShrink));
    HLOCAL h = LocalAlloc(LMEM_FIXED, 64);
    SET_ERR(0x2468);
    UN(LocalShrink)(h, 8);
    t_note("last error after LocalShrink: 0x%lx", (unsigned long)GetLastError());
    LocalFree(h);
}
TEST(gap19_localshrink_many_blocks_survive) {
    REQUIRE(UN(LocalShrink));
    enum { N = 200 };
    char *ps[N];
    for (int i = 0; i < N; i++) { ps[i] = LocalAlloc(LMEM_FIXED, 40 + i); memset(ps[i], i, 40 + i); }
    for (int i = 0; i < N; i += 2) { LocalFree(ps[i]); ps[i] = NULL; }
    UN(LocalShrink)(NULL, 0);
    int bad = 0;
    for (int i = 1; i < N; i += 2) {
        for (int j = 0; j < 40 + i; j++) if (ps[i][j] != (char)i) { bad++; break; }
        LocalFree(ps[i]);
    }
    EXPECT_EQ(bad, 0);
}
TEST(gap19_localshrink_thread_safe) {
    REQUIRE(UN(LocalShrink));
    for (int i = 0; i < 200; i++) UN(LocalShrink)(NULL, 0);
    CHECK(1);
}

/* ---------------- MapViewOfFile3FromApp ---------------- */
static HANDLE g19_section(DWORD prot, DWORD size) {
    return CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, prot, 0, size, NULL);
}
TEST(gap19_mapview3app_basic_rw) {
    REQUIRE(UN(MapViewOfFile3FromApp));
    HANDLE m = g19_section(PAGE_READWRITE, 0x10000);
    if (!m) { t_skip("no section"); return; }
    SET_ERR(0);
    void *v = UN(MapViewOfFile3FromApp)(m, GetCurrentProcess(), NULL, 0, 0, 0, PAGE_READWRITE, NULL, 0);
    DWORD e = GetLastError();
    t_note("map -> %p err %lu", v, (unsigned long)e);
    CHECK(v != NULL);
    if (v) {
        MEMORY_BASIC_INFORMATION mbi;
        CHECK(VirtualQuery(v, &mbi, sizeof mbi) == sizeof mbi);
        EXPECT_EQ(mbi.Type, MEM_MAPPED);
        EXPECT_EQ(mbi.Protect, PAGE_READWRITE);
        EXPECT_EQ(mbi.RegionSize, 0x10000);
        ((volatile char *)v)[0] = 1; ((volatile char *)v)[0xffff] = 2;
        EXPECT_EQ(UnmapViewOfFile(v), TRUE);
    }
    CloseHandle(m);
}
TEST(gap19_mapview3app_two_views_alias) {
    REQUIRE(UN(MapViewOfFile3FromApp));
    HANDLE m = g19_section(PAGE_READWRITE, 0x10000);
    if (!m) { t_skip("no section"); return; }
    char *a = UN(MapViewOfFile3FromApp)(m, GetCurrentProcess(), NULL, 0, 0, 0, PAGE_READWRITE, NULL, 0);
    char *b = UN(MapViewOfFile3FromApp)(m, GetCurrentProcess(), NULL, 0, 0, 0, PAGE_READONLY, NULL, 0);
    CHECK(a && b && a != b);
    if (a && b) { a[100] = 0x42; EXPECT_EQ(b[100], 0x42); }
    if (a) UnmapViewOfFile(a);
    if (b) UnmapViewOfFile(b);
    CloseHandle(m);
}
TEST(gap19_mapview3app_view_size_and_offset) {
    REQUIRE(UN(MapViewOfFile3FromApp));
    HANDLE m = g19_section(PAGE_READWRITE, 0x30000);
    if (!m) { t_skip("no section"); return; }
    char *v = UN(MapViewOfFile3FromApp)(m, GetCurrentProcess(), NULL, 0x10000, 0x8000, 0, PAGE_READWRITE, NULL, 0);
    CHECK(v != NULL);
    if (v) {
        MEMORY_BASIC_INFORMATION mbi; VirtualQuery(v, &mbi, sizeof mbi);
        EXPECT_EQ(mbi.RegionSize, 0x8000);
        UnmapViewOfFile(v);
    }
    SET_ERR(0);
    v = UN(MapViewOfFile3FromApp)(m, GetCurrentProcess(), NULL, 0x1000, 0x1000, 0, PAGE_READWRITE, NULL, 0);
    t_note("unaligned offset 0x1000 -> %p err %lu", v, (unsigned long)GetLastError());
    EXPECT_EQ(v, NULL);
    if (v) UnmapViewOfFile(v);
    SET_ERR(0);
    v = UN(MapViewOfFile3FromApp)(m, GetCurrentProcess(), NULL, 0x40000, 0, 0, PAGE_READWRITE, NULL, 0);
    t_note("offset past end -> %p err %lu", v, (unsigned long)GetLastError());
    EXPECT_EQ(v, NULL);
    if (v) UnmapViewOfFile(v);
    CloseHandle(m);
}
TEST(gap19_mapview3app_exec_readwrite_rejected) {
    REQUIRE(UN(MapViewOfFile3FromApp));
    HANDLE m = g19_section(PAGE_READWRITE, 0x10000);
    if (!m) { t_skip("no section"); return; }
    DWORD bad[] = { PAGE_EXECUTE_READWRITE, PAGE_EXECUTE_WRITECOPY, PAGE_EXECUTE_READWRITE | PAGE_GUARD };
    for (unsigned i = 0; i < 3; i++) {
        SET_ERR(0);
        void *v = UN(MapViewOfFile3FromApp)(m, GetCurrentProcess(), NULL, 0, 0, 0, bad[i], NULL, 0);
        EXPECT_EQ(v, NULL);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        if (v) UnmapViewOfFile(v);
    }
    CloseHandle(m);
}
TEST(gap19_mapview3app_exec_needs_capability) {
    REQUIRE(UN(MapViewOfFile3FromApp));
    HANDLE m = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_EXECUTE_READWRITE, 0, 0x10000, NULL);
    if (!m) { t_skip("no exec section"); return; }
    DWORD prot[] = { PAGE_EXECUTE, PAGE_EXECUTE_READ };
    for (unsigned i = 0; i < 2; i++) {
        SET_ERR(0);
        void *v = UN(MapViewOfFile3FromApp)(m, GetCurrentProcess(), NULL, 0, 0, 0, prot[i], NULL, 0);
        t_note("prot 0x%lx -> %p err %lu (5 without codeGeneration capability)", (unsigned long)prot[i], v, (unsigned long)GetLastError());
        if (v) UnmapViewOfFile(v);
    }
    CloseHandle(m);
}
TEST(gap19_mapview3app_base_address_needs_placeholder) {
    REQUIRE(UN(MapViewOfFile3FromApp));
    HANDLE m = g19_section(PAGE_READWRITE, 0x10000);
    if (!m) { t_skip("no section"); return; }
    SET_ERR(0);
    void *v = UN(MapViewOfFile3FromApp)(m, GetCurrentProcess(), (PVOID)0x50000000, 0, 0, 0, PAGE_READWRITE, NULL, 0);
    EXPECT_EQ(v, NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    if (v) UnmapViewOfFile(v);
    SET_ERR(0);
    v = UN(MapViewOfFile3FromApp)(m, GetCurrentProcess(), (PVOID)0x1, 0, 0, MEM_RESERVE, PAGE_READWRITE, NULL, 0);
    EXPECT_EQ(v, NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CloseHandle(m);
}
TEST(gap19_mapview3app_invalid_handles) {
    REQUIRE(UN(MapViewOfFile3FromApp));
    SET_ERR(0);
    void *v = UN(MapViewOfFile3FromApp)(NULL, GetCurrentProcess(), NULL, 0, 0, 0, PAGE_READWRITE, NULL, 0);
    EXPECT_EQ(v, NULL);
    t_note("NULL mapping -> err %lu", (unsigned long)GetLastError());
    CHECK(GetLastError() != 0);
    HANDLE ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    SET_ERR(0);
    v = UN(MapViewOfFile3FromApp)(ev, GetCurrentProcess(), NULL, 0, 0, 0, PAGE_READWRITE, NULL, 0);
    EXPECT_EQ(v, NULL);
    t_note("event as mapping -> err %lu", (unsigned long)GetLastError());
    CloseHandle(ev);
    HANDLE m = g19_section(PAGE_READWRITE, 0x10000);
    if (m) {
        SET_ERR(0);
        v = UN(MapViewOfFile3FromApp)(m, NULL, NULL, 0, 0, 0, PAGE_READWRITE, NULL, 0);
        t_note("NULL process -> %p err %lu", v, (unsigned long)GetLastError());
        if (v) UnmapViewOfFile(v);
        CloseHandle(m);
    }
}
TEST(gap19_mapview3app_protection_exceeds_section) {
    REQUIRE(UN(MapViewOfFile3FromApp));
    HANDLE m = g19_section(PAGE_READONLY, 0x10000);
    if (!m) { t_skip("RO pagefile section unsupported"); return; }
    SET_ERR(0);
    void *v = UN(MapViewOfFile3FromApp)(m, GetCurrentProcess(), NULL, 0, 0, 0, PAGE_READWRITE, NULL, 0);
    t_note("RW view of RO section -> %p err %lu", v, (unsigned long)GetLastError());
    EXPECT_EQ(v, NULL);
    if (v) UnmapViewOfFile(v);
    v = UN(MapViewOfFile3FromApp)(m, GetCurrentProcess(), NULL, 0, 0, 0, PAGE_READONLY, NULL, 0);
    CHECK(v != NULL);
    if (v) UnmapViewOfFile(v);
    CloseHandle(m);
}
TEST(gap19_mapview3app_writecopy) {
    REQUIRE(UN(MapViewOfFile3FromApp));
    HANDLE m = g19_section(PAGE_READWRITE, 0x10000);
    if (!m) { t_skip("no section"); return; }
    char *a = UN(MapViewOfFile3FromApp)(m, GetCurrentProcess(), NULL, 0, 0, 0, PAGE_READWRITE, NULL, 0);
    char *b = UN(MapViewOfFile3FromApp)(m, GetCurrentProcess(), NULL, 0, 0, 0, PAGE_WRITECOPY, NULL, 0);
    CHECK(a && b);
    if (a && b) {
        a[0] = 1; EXPECT_EQ(b[0], 1);
        b[0] = 2;               /* private copy on write */
        EXPECT_EQ(a[0], 1); EXPECT_EQ(b[0], 2);
    }
    if (a) UnmapViewOfFile(a);
    if (b) UnmapViewOfFile(b);
    CloseHandle(m);
}
TEST(gap19_mapview3app_after_unmap_unreadable_and_handle_closed_ok) {
    REQUIRE(UN(MapViewOfFile3FromApp));
    HANDLE m = g19_section(PAGE_READWRITE, 0x10000);
    if (!m) { t_skip("no section"); return; }
    char *a = UN(MapViewOfFile3FromApp)(m, GetCurrentProcess(), NULL, 0, 0, 0, PAGE_READWRITE, NULL, 0);
    CloseHandle(m);            /* view keeps the section alive */
    CHECK(a != NULL);
    if (a) {
        a[0] = 9; EXPECT_EQ(a[0], 9);
        MEMORY_BASIC_INFORMATION mbi;
        VirtualQuery(a, &mbi, sizeof mbi);
        EXPECT_EQ(mbi.State, MEM_COMMIT);
        UnmapViewOfFile(a);
        VirtualQuery(a, &mbi, sizeof mbi);
        CHECK(mbi.State != MEM_COMMIT || mbi.Type != MEM_MAPPED);
    }
}
TEST(gap19_mapview3app_many_views) {
    REQUIRE(UN(MapViewOfFile3FromApp));
    HANDLE m = g19_section(PAGE_READWRITE, 0x10000);
    if (!m) { t_skip("no section"); return; }
    void *vs[64]; int n = 0;
    for (int i = 0; i < 64; i++) { vs[i] = UN(MapViewOfFile3FromApp)(m, GetCurrentProcess(), NULL, 0, 0, 0, PAGE_READWRITE, NULL, 0); if (vs[i]) n++; }
    EXPECT_EQ(n, 64);
    for (int i = 0; i < 64; i++) if (vs[i]) UnmapViewOfFile(vs[i]);
    CloseHandle(m);
}
TEST(gap19_mapview3app_extended_params_count_zero_ptr_null) {
    REQUIRE(UN(MapViewOfFile3FromApp));
    HANDLE m = g19_section(PAGE_READWRITE, 0x10000);
    if (!m) { t_skip("no section"); return; }
    SET_ERR(0);
    void *v = UN(MapViewOfFile3FromApp)(m, GetCurrentProcess(), NULL, 0, 0, 0, PAGE_READWRITE, NULL, 3);
    t_note("count 3 with NULL params -> %p err %lu", v, (unsigned long)GetLastError());
    EXPECT_EQ(v, NULL);
    if (v) UnmapViewOfFile(v);
    CloseHandle(m);
}
