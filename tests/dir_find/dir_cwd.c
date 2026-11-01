/* GetCurrentDirectory / SetCurrentDirectory. Every SetCurrentDirectory runs in a child process. */
#include "dh.h"

#define CWD_CHILD_TEST(nm) \
    static int cwdbody_##nm(const char *d); \
    CHILD(cw_##nm) { return cwdbody_##nm(argc > 1 ? argv[1] : ""); } \
    TEST(cwd_##nm) { char a[MAX_PATH + 8]; char *d = dh_dir("cwd_" #nm); \
        snprintf(a, sizeof a, "\"%s\"", d); \
        DWORD r = t_run_child("cw_" #nm, a, 25000); \
        CHECKF(r == 0, "child failed at line %lu (0xdead0000/1 = spawn/timeout)", r); \
        dh_rmtree(d); } \
    static int cwdbody_##nm(const char *d)

static int cwd_is(const char *want) {
    char b[MAX_PATH * 2];
    DWORD n = U(GetCurrentDirectoryA)(sizeof b, b);
    return n && n < sizeof b && !lstrcmpiA(b, want);
}
static void cat(char *o, size_t c, const char *a, const char *b) { snprintf(o, c, "%s\\%s", a, b); }

/* ---- read-only queries, safe in the main process ---- */
TEST(cwd_get_size_query_a) {
    REQUIRE(U(GetCurrentDirectoryA));
    char b[MAX_PATH * 4];
    DWORD need = U(GetCurrentDirectoryA)(0, NULL);
    CHECK(need > 3);
    DWORD n = U(GetCurrentDirectoryA)(sizeof b, b);
    EXPECT_EQ(n + 1, need);
    EXPECT_EQ(strlen(b), n);
}
TEST(cwd_get_size_query_w) {
    REQUIRE(U(GetCurrentDirectoryW));
    WCHAR b[MAX_PATH * 4];
    DWORD need = U(GetCurrentDirectoryW)(0, NULL);
    CHECK(need > 3);
    DWORD n = U(GetCurrentDirectoryW)(MAX_PATH * 4, b);
    EXPECT_EQ(n + 1, need);
    EXPECT_EQ(wcslen(b), n);
}
TEST(cwd_get_exact_and_short_buffer_w) {
    REQUIRE(U(GetCurrentDirectoryW));
    WCHAR b[MAX_PATH * 4], tmp[MAX_PATH * 4];
    DWORD n = U(GetCurrentDirectoryW)(MAX_PATH * 4, b);
    CHECK(n > 0);
    /* exact: n + 1 slots -> success, returns n */
    memset(tmp, 0x55, sizeof tmp);
    DWORD r = U(GetCurrentDirectoryW)(n + 1, tmp);
    EXPECT_EQ(r, n);
    CHECK(!wcscmp(tmp, b));
    /* one short: returns required size including NUL */
    r = U(GetCurrentDirectoryW)(n, tmp);
    EXPECT_EQ(r, n + 1);
    r = U(GetCurrentDirectoryW)(1, tmp);
    EXPECT_EQ(r, n + 1);
    /* zero length with a real buffer */
    r = U(GetCurrentDirectoryW)(0, tmp);
    EXPECT_EQ(r, n + 1);
}
TEST(cwd_get_exact_and_short_buffer_a) {
    REQUIRE(U(GetCurrentDirectoryA));
    char b[MAX_PATH * 4], tmp[MAX_PATH * 4];
    DWORD n = U(GetCurrentDirectoryA)(sizeof b, b);
    CHECK(n > 0);
    memset(tmp, 0x55, sizeof tmp);
    DWORD r = U(GetCurrentDirectoryA)(n + 1, tmp);
    EXPECT_EQ(r, n);
    CHECK(!strcmp(tmp, b));
    r = U(GetCurrentDirectoryA)(n, tmp);
    EXPECT_EQ(r, n + 1);
    r = U(GetCurrentDirectoryA)(1, tmp);
    EXPECT_EQ(r, n + 1);
    r = U(GetCurrentDirectoryA)(0, tmp);
    EXPECT_EQ(r, n + 1);
}
TEST(cwd_get_short_buffer_leaves_buffer_alone_or_not) {
    REQUIRE(U(GetCurrentDirectoryA));
    char tmp[16];
    memset(tmp, 0x55, sizeof tmp);
    DWORD r = U(GetCurrentDirectoryA)(4, tmp);
    CHECK(r > 4);
    t_note("buffer after too-small GetCurrentDirectoryA: first byte 0x%02x", (unsigned char)tmp[0]);
    CHECK((unsigned char)tmp[8] == 0x55);   /* never writes beyond the stated size */
}
TEST(cwd_get_does_not_set_error_on_success) {
    REQUIRE(U(GetCurrentDirectoryW));
    WCHAR b[MAX_PATH * 4];
    SET_ERR(0x1234);
    DWORD n = U(GetCurrentDirectoryW)(MAX_PATH * 4, b);
    CHECK(n > 0);
    t_note("GetLastError after successful GetCurrentDirectoryW: 0x%lx", GetLastError());
}
TEST(cwd_get_a_w_parity) {
    REQUIRE(U(GetCurrentDirectoryA)); REQUIRE(U(GetCurrentDirectoryW));
    char a[MAX_PATH * 4]; WCHAR w[MAX_PATH * 4], conv[MAX_PATH * 4];
    DWORD na = U(GetCurrentDirectoryA)(sizeof a, a);
    DWORD nw = U(GetCurrentDirectoryW)(MAX_PATH * 4, w);
    CHECK(na && nw);
    MultiByteToWideChar(CP_ACP, 0, a, -1, conv, MAX_PATH * 4);
    if (!wcscmp(conv, w)) CHECK(1); else t_note("cwd not representable in ACP");
}
TEST(cwd_get_matches_fullpath_dot) {
    REQUIRE(U(GetCurrentDirectoryW)); REQUIRE(U(GetFullPathNameW));
    WCHAR c[MAX_PATH * 2], f[MAX_PATH * 2], *fp;
    DWORD n = U(GetCurrentDirectoryW)(MAX_PATH * 2, c);
    DWORD m = U(GetFullPathNameW)(L".", MAX_PATH * 2, f, &fp);
    EXPECT_EQ(n, m);
    CHECK(!wcscmp(c, f));
}
TEST(cwd_get_form) {
    REQUIRE(U(GetCurrentDirectoryA));
    char b[MAX_PATH * 2];
    DWORD n = U(GetCurrentDirectoryA)(sizeof b, b);
    CHECK(n >= 3);
    CHECK(b[1] == ':' || (b[0] == '\\' && b[1] == '\\'));
    if (n > 3) CHECK(b[n - 1] != '\\');     /* trailing backslash only for roots */
    else CHECK(b[2] == '\\');
}
TEST(cwd_get_huge_size_arg) {
    REQUIRE(U(GetCurrentDirectoryW));
    WCHAR b[MAX_PATH * 4];
    DWORD n = U(GetCurrentDirectoryW)(MAX_PATH * 4, b);
    DWORD r = U(GetCurrentDirectoryW)(0x7fffffff / 2, b);
    t_note("GetCurrentDirectoryW(huge) = %lu (expected len %lu)", r, n);
}

/* ---- Set in child ---- */
CWD_CHILD_TEST(set_abs_a) {
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(d));
    DH_CHILD_CHECK(cwd_is(d));
    return 0;
}
CWD_CHILD_TEST(set_abs_w) {
    WCHAR w[MAX_PATH], b[MAX_PATH * 2];
    dh_w(w, MAX_PATH, d);
    DH_CHILD_CHECK(U(SetCurrentDirectoryW)(w));
    DWORD n = U(GetCurrentDirectoryW)(MAX_PATH * 2, b);
    DH_CHILD_CHECK(n == wcslen(w) && !_wcsicmp(b, w));
    return 0;
}
CWD_CHILD_TEST(set_relative) {
    char p[MAX_PATH];
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(d));
    DH_CHILD_CHECK(CreateDirectoryA("r1", NULL));
    DH_CHILD_CHECK(CreateDirectoryA("r1\\r2", NULL));
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)("r1"));
    cat(p, sizeof p, d, "r1"); DH_CHILD_CHECK(cwd_is(p));
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)("r2"));
    cat(p, sizeof p, d, "r1\\r2"); DH_CHILD_CHECK(cwd_is(p));
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(".."));
    cat(p, sizeof p, d, "r1"); DH_CHILD_CHECK(cwd_is(p));
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)("."));
    DH_CHILD_CHECK(cwd_is(p));
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)("..\\r1\\r2\\.."));
    DH_CHILD_CHECK(cwd_is(p));
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(".."));
    return cwd_is(d) ? 0 : __LINE__;
}
CWD_CHILD_TEST(set_trailing_sep) {
    char p[MAX_PATH];
    snprintf(p, sizeof p, "%s\\", d);
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(p));
    DH_CHILD_CHECK(cwd_is(d));        /* no trailing backslash is kept */
    snprintf(p, sizeof p, "%s\\\\", d);
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(p));
    DH_CHILD_CHECK(cwd_is(d));
    snprintf(p, sizeof p, "%s/", d);
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(p));
    DH_CHILD_CHECK(cwd_is(d));
    return 0;
}
CWD_CHILD_TEST(set_forward_slashes) {
    char p[MAX_PATH], q[MAX_PATH];
    cat(q, sizeof q, d, "fw"); DH_CHILD_CHECK(CreateDirectoryA(q, NULL));
    snprintf(p, sizeof p, "%s/fw", d);
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(p));
    DH_CHILD_CHECK(cwd_is(q));
    return 0;
}
CWD_CHILD_TEST(set_nonexistent) {
    char p[MAX_PATH], before[MAX_PATH];
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(d));
    cat(p, sizeof p, d, "missing");
    SetLastError(0);
    DH_CHILD_CHECK(!U(SetCurrentDirectoryA)(p));
    DH_CHILD_CHECK(GetLastError() == ERROR_FILE_NOT_FOUND);
    DH_CHILD_CHECK(cwd_is(d));
    cat(p, sizeof p, d, "missing\\deeper");
    SetLastError(0);
    DH_CHILD_CHECK(!U(SetCurrentDirectoryA)(p));
    DH_CHILD_CHECK(GetLastError() == ERROR_PATH_NOT_FOUND);
    DH_CHILD_CHECK(cwd_is(d));
    return 0;
}
CWD_CHILD_TEST(set_nonexistent_w) {
    WCHAR w[MAX_PATH]; char p[MAX_PATH];
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(d));
    cat(p, sizeof p, d, "missing"); dh_w(w, MAX_PATH, p);
    SetLastError(0);
    DH_CHILD_CHECK(!U(SetCurrentDirectoryW)(w));
    DH_CHILD_CHECK(GetLastError() == ERROR_FILE_NOT_FOUND);
    return 0;
}
CWD_CHILD_TEST(set_file) {
    char p[MAX_PATH];
    DH_CHILD_CHECK(dh_mkfile(d, "afile", 1));
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(d));
    cat(p, sizeof p, d, "afile");
    SetLastError(0);
    DH_CHILD_CHECK(!U(SetCurrentDirectoryA)(p));
    DH_CHILD_CHECK(GetLastError() == ERROR_DIRECTORY);
    DH_CHILD_CHECK(cwd_is(d));
    return 0;
}
CWD_CHILD_TEST(set_null_w) {
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(d));
    SetLastError(0);
    DH_CHILD_CHECK(!U(SetCurrentDirectoryW)(NULL));
    { DWORD le = GetLastError(); printf("SetCurrentDirectoryW(NULL) err=%lu\n", le);
      if (!t_is_wine()) DH_CHILD_CHECK(le == ERROR_INVALID_PARAMETER); }
    DH_CHILD_CHECK(cwd_is(d));
    return 0;
}
CWD_CHILD_TEST(set_empty) {
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(d));
    SetLastError(0);
    BOOL ok = U(SetCurrentDirectoryA)("");
    DWORD e = GetLastError();
    printf("SetCurrentDirectory(\"\") ok=%d err=%lu\n", ok, e);
    DH_CHILD_CHECK(!ok);
    DH_CHILD_CHECK(cwd_is(d));
    return 0;
}
CWD_CHILD_TEST(set_wildcards) {
    char p[MAX_PATH];
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(d));
    const char *bad[] = { "*", "?", "a*b", "a<b", "a|b", "a\"b" };
    for (unsigned i = 0; i < 6; i++) {
        cat(p, sizeof p, d, bad[i]);
        SetLastError(0);
        DH_CHILD_CHECK(!U(SetCurrentDirectoryA)(p));
        DWORD e = GetLastError();
        DH_CHILD_CHECK(e == ERROR_INVALID_NAME || e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND);
        DH_CHILD_CHECK(cwd_is(d));
    }
    return 0;
}
CWD_CHILD_TEST(set_updates_env_drive_var) {
    char var[4] = { '=', 0, ':', 0 }, b[MAX_PATH * 2], p[MAX_PATH];
    var[1] = d[0];
    cat(p, sizeof p, d, "ev"); DH_CHILD_CHECK(CreateDirectoryA(p, NULL));
    /* Win10 SetCurrentDirectoryW -> RtlSetCurrentDirectory_U never writes the =X: variable (the shell does); only the cwd changes.
       P1 audit: the exact consequence is that =X: is left as it was before the call (present or absent). */
    char before[MAX_PATH * 2]; DWORD nb = GetEnvironmentVariableA(var, before, sizeof before);
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(p));
    DH_CHILD_CHECK(cwd_is(p));
    DWORD n = GetEnvironmentVariableA(var, b, sizeof b);
    DH_CHILD_CHECK(n == nb && (n == 0 || !lstrcmpA(b, before)));
    return 0;
}
CWD_CHILD_TEST(drive_relative_current_drive) {
    char p[MAX_PATH], rel[MAX_PATH];
    cat(p, sizeof p, d, "dr"); DH_CHILD_CHECK(CreateDirectoryA(p, NULL));
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(d));
    snprintf(rel, sizeof rel, "%c:dr", d[0]);
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(rel));
    DH_CHILD_CHECK(cwd_is(p));
    return 0;
}
CWD_CHILD_TEST(drive_relative_other_drive_env) {
    /* "Q:foo" resolves against the =Q: variable when Q is not the current drive */
    char b[MAX_PATH * 2]; WCHAR w[MAX_PATH * 2], *fp;
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(d));
    DWORD mask = GetLogicalDrives(); int c;
    for (c = 25; c >= 0; c--) if (!(mask & (1u << c))) break;
    DH_CHILD_CHECK(c >= 0);
    char var[4] = { '=', 'A' + c, ':', 0 };
    char val[64]; snprintf(val, sizeof val, "%c:\\k32t\\base", 'A' + c);
    DH_CHILD_CHECK(SetEnvironmentVariableA(var, val));
    char rel[16]; snprintf(rel, sizeof rel, "%c:foo", 'A' + c);
    DWORD n = U(GetFullPathNameA)(rel, sizeof b, b, NULL);
    /* P1 audit: the drive letter is unmapped, so ntdll RtlGetFullPathName_Ustr -> RtlpCheckRelativeDrive cannot
       NtOpenFile the directory named by =X: and RtlpResetDriveEnvironment rewrites =X: to "X:\\" before it is
       read: the path resolves against the drive root and the variable is reset (deterministic for an unmapped drive). */
    char want[80]; snprintf(want, sizeof want, "%c:\\foo", 'A' + c);
    DH_CHILD_CHECK(n && !lstrcmpiA(b, want));
    char ev_[64]; DWORD en_ = GetEnvironmentVariableA(var, ev_, sizeof ev_);
    char root_[8]; snprintf(root_, sizeof root_, "%c:\\", 'A' + c);
    DH_CHILD_CHECK(en_ == 3 && !lstrcmpiA(ev_, root_));
    /* without the variable it resolves against the drive root */
    DH_CHILD_CHECK(SetEnvironmentVariableA(var, NULL));
    n = U(GetFullPathNameA)(rel, sizeof b, b, NULL);
    snprintf(want, sizeof want, "%c:\\foo", 'A' + c);
    DH_CHILD_CHECK(n && !lstrcmpiA(b, want));
    return 0;
}
CWD_CHILD_TEST(set_unc_localhost) {
    char unc[MAX_PATH], b[MAX_PATH * 2];
    snprintf(unc, sizeof unc, "\\\\localhost\\%c$%s", d[0], d + 2);
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(d));
    SetLastError(0);
    BOOL ok = U(SetCurrentDirectoryA)(unc);
    printf("SetCurrentDirectory(%s) ok=%d err=%lu\n", unc, ok, GetLastError());
    if (ok) { U(GetCurrentDirectoryA)(sizeof b, b); printf("cwd now %s\n", b); DH_CHILD_CHECK(b[0] == '\\' && b[1] == '\\'); }
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(d));
    DH_CHILD_CHECK(cwd_is(d));
    return 0;
}
CWD_CHILD_TEST(set_unc_nonexistent_server) {
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(d));
    SetLastError(0);
    BOOL ok = U(SetCurrentDirectoryA)("\\\\k32t-no-such-host-7731\\share\\dir");
    DWORD e = GetLastError();
    DH_CHILD_CHECK(!ok);
    printf("unc nonexistent server err=%lu\n", e);
    DH_CHILD_CHECK(e != 0);
    DH_CHILD_CHECK(cwd_is(d));
    return 0;
}
CWD_CHILD_TEST(set_raw_prefix) {
    char p[MAX_PATH], b[MAX_PATH * 2];
    snprintf(p, sizeof p, "\\\\?\\%s", d);
    SetLastError(0);
    BOOL ok = U(SetCurrentDirectoryA)(p);
    printf("SetCurrentDirectory(\\\\?\\...) ok=%d err=%lu\n", ok, GetLastError());
    if (ok) { U(GetCurrentDirectoryA)(sizeof b, b); printf("cwd now %s\n", b); }
    return 0;
}
CWD_CHILD_TEST(set_long_path) {
    WCHAR dw[MAX_PATH], w[1100], raw[1200]; char b[2048];
    dh_w(dw, MAX_PATH, d);
    int k = snwprintf(w, 1100, L"%ls\\", dw);
    int segs = 0, mark[8];
    while (k < 280 && segs < 6) {
        for (int i = 0; i < 60; i++) w[k++] = 'l';
        w[k] = 0;
        snwprintf(raw, 1200, L"\\\\?\\%ls", w);
        DH_CHILD_CHECK(CreateDirectoryW(raw, NULL));
        w[k++] = '\\';
        segs++;
    }
    w[k - 1] = 0;
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(d));
    SetLastError(0);
    BOOL ok = U(SetCurrentDirectoryW)(w);
    DWORD e = GetLastError();
    printf("SetCurrentDirectoryW len %d ok=%d err=%lu\n", k - 1, ok, e);
    if (t_is_wine()) return 0;
    DH_CHILD_CHECK(!ok);
    DH_CHILD_CHECK(e == ERROR_FILENAME_EXCED_RANGE || e == ERROR_PATH_NOT_FOUND || e == ERROR_BAD_PATHNAME);
    return 0;
}
CWD_CHILD_TEST(get_buffer_after_set) {
    char p[MAX_PATH], b[MAX_PATH * 2];
    cat(p, sizeof p, d, "buffer_semantic");
    DH_CHILD_CHECK(CreateDirectoryA(p, NULL));
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(p));
    DWORD len = (DWORD)strlen(p);
    DH_CHILD_CHECK(U(GetCurrentDirectoryA)(0, NULL) == len + 1);
    DH_CHILD_CHECK(U(GetCurrentDirectoryA)(len + 1, b) == len);
    DH_CHILD_CHECK(U(GetCurrentDirectoryA)(len, b) == len + 1);
    DH_CHILD_CHECK(U(GetCurrentDirectoryA)(len + 2, b) == len);
    WCHAR w[MAX_PATH * 2];
    DH_CHILD_CHECK(U(GetCurrentDirectoryW)(len + 1, w) == len);
    DH_CHILD_CHECK(U(GetCurrentDirectoryW)(len, w) == len + 1);
    return 0;
}
CWD_CHILD_TEST(get_root_form) {
    char root[8] = { d[0], ':', '\\', 0 }, b[MAX_PATH];
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(root));
    DWORD n = U(GetCurrentDirectoryA)(sizeof b, b);
    DH_CHILD_CHECK(n == 3);
    DH_CHILD_CHECK(b[1] == ':' && b[2] == '\\');
    DH_CHILD_CHECK(U(GetCurrentDirectoryA)(0, NULL) == 4);
    DH_CHILD_CHECK(U(GetCurrentDirectoryA)(3, b) == 4);
    char root2[8] = { d[0], ':', 0 };
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(d));
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(root2));   /* "X:" = drive-relative, current dir of X (here: d) */
    DH_CHILD_CHECK(cwd_is(d));
    return 0;
}
CWD_CHILD_TEST(cwd_blocks_removal) {
    char p[MAX_PATH];
    cat(p, sizeof p, d, "held"); DH_CHILD_CHECK(CreateDirectoryA(p, NULL));
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(p));
    SetLastError(0);
    DH_CHILD_CHECK(!RemoveDirectoryA(p));
    DH_CHILD_CHECK(GetLastError() == ERROR_SHARING_VIOLATION);
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(d));
    DH_CHILD_CHECK(RemoveDirectoryA(p));
    return 0;
}
CWD_CHILD_TEST(cwd_blocks_rename) {
    char p[MAX_PATH], q[MAX_PATH];
    cat(p, sizeof p, d, "held2"); cat(q, sizeof q, d, "moved2");
    DH_CHILD_CHECK(CreateDirectoryA(p, NULL));
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(p));
    SetLastError(0);
    BOOL ok = MoveFileA(p, q);
    printf("rename of cwd: ok=%d err=%lu\n", ok, GetLastError());
    DH_CHILD_CHECK(!ok);
    DH_CHILD_CHECK(GetLastError() == ERROR_SHARING_VIOLATION || GetLastError() == ERROR_ACCESS_DENIED);
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(d));
    return 0;
}
CWD_CHILD_TEST(cwd_parent_removal_blocked) {
    char p[MAX_PATH], q[MAX_PATH];
    cat(p, sizeof p, d, "par"); cat(q, sizeof q, p, "kid");
    DH_CHILD_CHECK(CreateDirectoryA(p, NULL) && CreateDirectoryA(q, NULL));
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(q));
    SetLastError(0);
    DH_CHILD_CHECK(!RemoveDirectoryA(p));
    DH_CHILD_CHECK(GetLastError() == ERROR_DIR_NOT_EMPTY);
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(d));
    return 0;
}
CWD_CHILD_TEST(set_unicode_w) {
    WCHAR dw[MAX_PATH], w[MAX_PATH + 32], b[MAX_PATH * 2];
    dh_w(dw, MAX_PATH, d);
    snwprintf(w, MAX_PATH + 32, L"%ls\\\u00e9\u4e2d\u6587", dw);
    DH_CHILD_CHECK(CreateDirectoryW(w, NULL));
    DH_CHILD_CHECK(U(SetCurrentDirectoryW)(w));
    DWORD n = U(GetCurrentDirectoryW)(MAX_PATH * 2, b);
    DH_CHILD_CHECK(n == wcslen(w) && !_wcsicmp(b, w));
    return 0;
}
CWD_CHILD_TEST(set_case_as_given) {
    char p[MAX_PATH], q[MAX_PATH], b[MAX_PATH];
    cat(p, sizeof p, d, "CaseDir"); DH_CHILD_CHECK(CreateDirectoryA(p, NULL));
    cat(q, sizeof q, d, "CASEDIR");
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(q));
    U(GetCurrentDirectoryA)(sizeof b, b);
    printf("cwd after set with different case: %s\n", b);
    DH_CHILD_CHECK(!lstrcmpiA(b, p));
    return 0;
}
CWD_CHILD_TEST(set_trailing_dots_spaces) {
    char p[MAX_PATH], q[MAX_PATH];
    cat(p, sizeof p, d, "tds"); DH_CHILD_CHECK(CreateDirectoryA(p, NULL));
    cat(q, sizeof q, d, "tds. ");
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(q));
    DH_CHILD_CHECK(cwd_is(p));
    return 0;
}
CWD_CHILD_TEST(set_junction) {
    char t[MAX_PATH], l[MAX_PATH];
    cat(t, sizeof t, d, "jt"); cat(l, sizeof l, d, "jl");
    DH_CHILD_CHECK(CreateDirectoryA(t, NULL) && CreateDirectoryA(l, NULL));
    if (!dh_mkjunction(l, t)) { printf("no junction\n"); return 0; }
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(l));
    DH_CHILD_CHECK(cwd_is(l));            /* link path, not resolved target */
    return 0;
}
CWD_CHILD_TEST(set_repeat_loop) {
    char a[MAX_PATH], b[MAX_PATH];
    cat(a, sizeof a, d, "la"); cat(b, sizeof b, d, "lb");
    DH_CHILD_CHECK(CreateDirectoryA(a, NULL) && CreateDirectoryA(b, NULL));
    for (int i = 0; i < 2000; i++) {
        const char *t = (i & 1) ? b : a;
        DH_CHILD_CHECK(U(SetCurrentDirectoryA)(t));
        if (i % 500 == 0) DH_CHILD_CHECK(cwd_is(t));
    }
    DH_CHILD_CHECK(cwd_is(b));
    return 0;
}
CWD_CHILD_TEST(set_a_w_error_parity) {
    char p[MAX_PATH]; WCHAR w[MAX_PATH];
    cat(p, sizeof p, d, "nope\\x"); dh_w(w, MAX_PATH, p);
    SetLastError(0); BOOL a = U(SetCurrentDirectoryA)(p); DWORD ea = GetLastError();
    SetLastError(0); BOOL b = U(SetCurrentDirectoryW)(w); DWORD eb = GetLastError();
    DH_CHILD_CHECK(!a && !b && ea == eb);
    return 0;
}
CHILD(cw_leaf_check) { return cwd_is(argc > 1 ? argv[1] : "") ? 0 : 3; }
CWD_CHILD_TEST(inherited_by_child) {
    char p[MAX_PATH], a[MAX_PATH + 8];
    cat(p, sizeof p, d, "inh"); DH_CHILD_CHECK(CreateDirectoryA(p, NULL));
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(p));
    snprintf(a, sizeof a, "\"%s\"", p);
    DH_CHILD_CHECK(t_run_child("cw_leaf_check", a, 15000) == 0);
    return 0;
}
CWD_CHILD_TEST(set_dot_dot_at_root) {
    char root[8] = { d[0], ':', '\\', 0 }, b[MAX_PATH];
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(root));
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(".."));
    DH_CHILD_CHECK(U(GetCurrentDirectoryA)(sizeof b, b) == 3);
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)("..\\..\\.."));
    DH_CHILD_CHECK(U(GetCurrentDirectoryA)(sizeof b, b) == 3);
    return 0;
}
CWD_CHILD_TEST(set_nonexistent_drive) {
    DWORD mask = GetLogicalDrives(); int c;
    for (c = 25; c >= 0; c--) if (!(mask & (1u << c))) break;
    DH_CHILD_CHECK(c >= 0);
    char p[16]; snprintf(p, sizeof p, "%c:\\", 'A' + c);
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(d));
    SetLastError(0);
    DH_CHILD_CHECK(!U(SetCurrentDirectoryA)(p));
    DWORD e = GetLastError();
    DH_CHILD_CHECK(e == ERROR_PATH_NOT_FOUND || e == ERROR_INVALID_DRIVE || e == ERROR_FILE_NOT_FOUND);
    DH_CHILD_CHECK(cwd_is(d));
    return 0;
}
CWD_CHILD_TEST(set_dos_device_name) {
    char p[MAX_PATH];
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(d));
    cat(p, sizeof p, d, "NUL");
    SetLastError(0);
    BOOL ok = U(SetCurrentDirectoryA)(p);
    printf("SetCurrentDirectory(NUL) ok=%d err=%lu\n", ok, GetLastError());
    if (ok) DH_CHILD_CHECK(U(SetCurrentDirectoryA)(d));
    DH_CHILD_CHECK(cwd_is(d));
    return 0;
}
CWD_CHILD_TEST(dll_search_independent) {
    /* changing cwd does not break already-resolved function pointers */
    char p[MAX_PATH];
    cat(p, sizeof p, d, "sd"); DH_CHILD_CHECK(CreateDirectoryA(p, NULL));
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(p));
    DH_CHILD_CHECK(U(GetCurrentDirectoryA) != NULL);
    DH_CHILD_CHECK(GetModuleHandleA("kernel32.dll") != NULL);
    return 0;
}
CWD_CHILD_TEST(relative_file_ops_follow_cwd) {
    char p[MAX_PATH], q[MAX_PATH];
    cat(p, sizeof p, d, "rf"); DH_CHILD_CHECK(CreateDirectoryA(p, NULL));
    DH_CHILD_CHECK(U(SetCurrentDirectoryA)(p));
    HANDLE h = CreateFileA("made.txt", GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
    DH_CHILD_CHECK(h != INVALID_HANDLE_VALUE); CloseHandle(h);
    cat(q, sizeof q, p, "made.txt");
    DH_CHILD_CHECK(dh_exists(q));
    return 0;
}
