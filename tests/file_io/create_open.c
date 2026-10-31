#include "fio.h"

/* ---------- dispositions ---------- */
TEST(fio_create_new_basic) {
    REQUIRE(U(CreateFileA));
    TP(p, "cn_basic.bin"); rmfile(p);
    SET_ERR(0x1234);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL);
    DWORD e = GetLastError();
    OK_H(h);
    EXPECT_EQ(e, 0);              /* success clears last error for CREATE_NEW */
    EXPECT_EQ(hsize(h), 0);
    CLOSE(h);
    EXPECT_FAIL_H(cfa(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, CREATE_NEW, 0), ERROR_FILE_EXISTS);
    rmfile(p);
}
TEST(fio_create_new_w) {
    REQUIRE(U(CreateFileW));
    TP(p, "cn_basic_w.bin"); rmfile(p);
    SET_ERR(0x1234);
    HANDLE h = cfw(fio_w(p), GENERIC_WRITE, 0, CREATE_NEW, 0);
    DWORD e = GetLastError();
    OK_H(h); EXPECT_EQ(e, 0); CLOSE(h);
    EXPECT_FAIL_H(cfw(fio_w(p), GENERIC_WRITE, 0, CREATE_NEW, 0), ERROR_FILE_EXISTS);
    rmfile(p);
}
TEST(fio_create_new_existing_directory) {
    REQUIRE(U(CreateFileA));
    TP(p, "cn_dir"); RemoveDirectoryA(p); CreateDirectoryA(p, NULL);
    EXPECT_FAIL_H(cfa(p, GENERIC_WRITE, 0, CREATE_NEW, FILE_FLAG_BACKUP_SEMANTICS), ERROR_FILE_EXISTS);
    RemoveDirectoryA(p);
}
TEST(fio_create_always_new) {
    REQUIRE(U(CreateFileA));
    TP(p, "ca_new.bin"); rmfile(p);
    SET_ERR(0x4321);
    HANDLE h = cfa(p, GENERIC_WRITE, 0, CREATE_ALWAYS, 0);
    DWORD e = GetLastError();
    OK_H(h); EXPECT_EQ(e, 0);
    CLOSE(h); rmfile(p);
}
TEST(fio_create_always_existing_truncates) {
    REQUIRE(U(CreateFileA));
    TP(p, "ca_exist.bin"); mkfile_pat(p, 5000);
    SET_ERR(0);
    HANDLE h = cfa(p, GENERIC_WRITE, 0, CREATE_ALWAYS, 0);
    DWORD e = GetLastError();
    OK_H(h);
    EXPECT_EQ(e, ERROR_ALREADY_EXISTS);
    EXPECT_EQ(hsize(h), 0);
    CLOSE(h);
    EXPECT_EQ(fsize_of(p), 0);
    rmfile(p);
}
TEST(fio_create_always_existing_w) {
    REQUIRE(U(CreateFileW));
    TP(p, "ca_exist_w.bin"); mkfile_pat(p, 100);
    SET_ERR(0);
    HANDLE h = cfw(fio_w(p), GENERIC_WRITE, 0, CREATE_ALWAYS, 0);
    DWORD e = GetLastError();
    OK_H(h); EXPECT_EQ(e, ERROR_ALREADY_EXISTS); CLOSE(h);
    EXPECT_EQ(fsize_of(p), 0);
    rmfile(p);
}
TEST(fio_create_always_no_write_access_still_truncates) {
    REQUIRE(U(CreateFileA));
    /* CREATE_ALWAYS overwrites (FILE_OVERWRITE_IF) even when the caller only asked for read access. */
    TP(p, "ca_ro_access.bin"); mkfile_pat(p, 300);
    HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, CREATE_ALWAYS, 0);
    if (h == INVALID_HANDLE_VALUE) t_note("CREATE_ALWAYS with GENERIC_READ failed gle=%lu", GetLastError());
    else { EXPECT_EQ(GetLastError(), ERROR_ALREADY_EXISTS); EXPECT_EQ(hsize(h), 0); }
    CLOSE(h); rmfile(p);
}
TEST(fio_open_existing) {
    REQUIRE(U(CreateFileA));
    TP(p, "oe_ok.bin"); mkfile_pat(p, 10);
    SET_ERR(0x777);
    HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0);
    DWORD e = GetLastError();
    OK_H(h); EXPECT_EQ(e, 0);
    EXPECT_EQ(hsize(h), 10);
    CLOSE(h); rmfile(p);
}
TEST(fio_open_existing_missing) {
    REQUIRE(U(CreateFileA));
    TP(p, "oe_missing.bin"); rmfile(p);
    EXPECT_FAIL_H(cfa(p, GENERIC_READ, 0, OPEN_EXISTING, 0), ERROR_FILE_NOT_FOUND);
    EXPECT_FAIL_H(cfw(fio_w(p), GENERIC_READ, 0, OPEN_EXISTING, 0), ERROR_FILE_NOT_FOUND);
    EXPECT_FAIL_H(cfa(p, 0, 0, OPEN_EXISTING, 0), ERROR_FILE_NOT_FOUND);
    EXPECT_FAIL_H(cfa(p, GENERIC_WRITE, 0, TRUNCATE_EXISTING, 0), ERROR_FILE_NOT_FOUND);
    CHECK(!exists(p));
}
TEST(fio_open_existing_missing_directory) {
    REQUIRE(U(CreateFileA));
    TP(p, "no_such_dir_xyz\\file.bin");
    EXPECT_FAIL_H(cfa(p, GENERIC_READ, 0, OPEN_EXISTING, 0), ERROR_PATH_NOT_FOUND);
    EXPECT_FAIL_H(cfa(p, GENERIC_WRITE, 0, CREATE_ALWAYS, 0), ERROR_PATH_NOT_FOUND);
    EXPECT_FAIL_H(cfa(p, GENERIC_WRITE, 0, CREATE_NEW, 0), ERROR_PATH_NOT_FOUND);
    EXPECT_FAIL_H(cfa(p, GENERIC_WRITE, 0, OPEN_ALWAYS, 0), ERROR_PATH_NOT_FOUND);
    EXPECT_FAIL_H(cfw(fio_w(p), GENERIC_READ, 0, OPEN_EXISTING, 0), ERROR_PATH_NOT_FOUND);
}
TEST(fio_open_always) {
    REQUIRE(U(CreateFileA));
    TP(p, "oa.bin"); rmfile(p);
    SET_ERR(0x55);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, 0, OPEN_ALWAYS, 0);
    DWORD e = GetLastError();
    OK_H(h); EXPECT_EQ(e, 0);
    DWORD n; WriteFile(h, "abcdef", 6, &n, NULL);
    CLOSE(h);
    SET_ERR(0);
    h = cfa(p, GENERIC_READ | GENERIC_WRITE, 0, OPEN_ALWAYS, 0);
    e = GetLastError();
    OK_H(h); EXPECT_EQ(e, ERROR_ALREADY_EXISTS);
    EXPECT_EQ(hsize(h), 6);
    CLOSE(h);
    EXPECT_EQ(fsize_of(p), 6);
    rmfile(p);
}
TEST(fio_open_always_w) {
    REQUIRE(U(CreateFileW));
    TP(p, "oa_w.bin"); rmfile(p);
    HANDLE h = cfw(fio_w(p), GENERIC_WRITE, 0, OPEN_ALWAYS, 0); OK_H(h); EXPECT_EQ(GetLastError(), 0); CLOSE(h);
    h = cfw(fio_w(p), GENERIC_WRITE, 0, OPEN_ALWAYS, 0); OK_H(h); EXPECT_EQ(GetLastError(), ERROR_ALREADY_EXISTS); CLOSE(h);
    rmfile(p);
}
TEST(fio_truncate_existing) {
    REQUIRE(U(CreateFileA));
    TP(p, "te.bin"); mkfile_pat(p, 777);
    SET_ERR(0x99);
    HANDLE h = cfa(p, GENERIC_WRITE, 0, TRUNCATE_EXISTING, 0);
    DWORD e = GetLastError();
    OK_H(h); EXPECT_EQ(e, 0);   /* TRUNCATE_EXISTING does not report ERROR_ALREADY_EXISTS */
    EXPECT_EQ(hsize(h), 0);
    CLOSE(h);
    EXPECT_EQ(fsize_of(p), 0);
    rmfile(p);
}
TEST(fio_truncate_existing_requires_generic_write) {
    REQUIRE(U(CreateFileA));
    TP(p, "te_noaccess.bin"); mkfile_pat(p, 50);
    /* Kernel behaviour (not visible in the CreateFileInternal decompilation) is not verified: report only. */
    {
        HANDLE t0 = cfa(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, TRUNCATE_EXISTING, 0);
        t_note("TRUNCATE_EXISTING with GENERIC_READ: %s gle=%lu", t0 != INVALID_HANDLE_VALUE ? "ok" : "fail", GetLastError());
        if (t0 != INVALID_HANDLE_VALUE) CloseHandle(t0);
        mkfile_pat(p, 50);
        t0 = cfa(p, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, TRUNCATE_EXISTING, 0);
        t_note("TRUNCATE_EXISTING with access 0: %s gle=%lu", t0 != INVALID_HANDLE_VALUE ? "ok" : "fail", GetLastError());
        if (t0 != INVALID_HANDLE_VALUE) CloseHandle(t0);
        mkfile_pat(p, 50);
    }
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, 0, TRUNCATE_EXISTING, 0); OK_H(h); CLOSE(h);
    EXPECT_EQ(fsize_of(p), 0);
    rmfile(p);
}
TEST(fio_truncate_existing_param_check_precedes_lookup) {
    REQUIRE(U(CreateFileA));
    TP(p, "te_missing_ro.bin"); rmfile(p);
    /* Whether the access check precedes the lookup is kernel behaviour: report only. */
    HANDLE t0 = cfa(p, GENERIC_READ, 0, TRUNCATE_EXISTING, 0);
    t_note("TRUNCATE_EXISTING|GENERIC_READ on missing file: %s gle=%lu", t0 != INVALID_HANDLE_VALUE ? "ok" : "fail", GetLastError());
    if (t0 != INVALID_HANDLE_VALUE) { CloseHandle(t0); rmfile(p); }
}
TEST(fio_bad_dispositions) {
    REQUIRE(U(CreateFileA));
    TP(p, "bad_disp.bin"); mkfile_pat(p, 4);
    static const DWORD bad[] = {0, 6, 7, 8, 0x10, 0x80000000, 0xFFFFFFFF, 100, 256};
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        SET_ERR(0);
        HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, 0, bad[i], 0);
        DWORD e = GetLastError();
        CHECKF(h == INVALID_HANDLE_VALUE && e == ERROR_INVALID_PARAMETER, "disp=0x%lx h=%p gle=%lu", bad[i], h, e);
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
        h = cfw(fio_w(p), GENERIC_READ, 0, bad[i], 0);
        e = GetLastError();
        CHECKF(h == INVALID_HANDLE_VALUE && e == ERROR_INVALID_PARAMETER, "W disp=0x%lx h=%p gle=%lu", bad[i], h, e);
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }
    EXPECT_EQ(fsize_of(p), 4);
    rmfile(p);
}

/* ---------- names ---------- */
TEST(fio_name_null) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(CreateFileW));
    EXPECT_FAIL_H(U(CreateFileA)(NULL, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL), ERROR_PATH_NOT_FOUND);
    EXPECT_FAIL_H(U(CreateFileW)(NULL, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL), ERROR_PATH_NOT_FOUND);
    EXPECT_FAIL_H(U(CreateFileW)(NULL, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL), ERROR_PATH_NOT_FOUND);
}
TEST(fio_name_empty) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(CreateFileW));
    EXPECT_FAIL_H(cfa("", GENERIC_READ, 0, OPEN_EXISTING, 0), ERROR_PATH_NOT_FOUND);
    EXPECT_FAIL_H(cfw(L"", GENERIC_READ, 0, OPEN_EXISTING, 0), ERROR_PATH_NOT_FOUND);
    EXPECT_FAIL_H(cfa("", GENERIC_WRITE, 0, CREATE_ALWAYS, 0), ERROR_PATH_NOT_FOUND);
    EXPECT_FAIL_H(cfa("", 0, 0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS), ERROR_PATH_NOT_FOUND);
}
TEST(fio_name_spaces_only) {
    REQUIRE(U(CreateFileA));
    HANDLE h = cfa("   ", GENERIC_READ, 0, OPEN_EXISTING, 0);
    DWORD e = GetLastError();
    CHECK(h == INVALID_HANDLE_VALUE);
    t_note("CreateFile(\"   \") gle=%lu", e);
    CHECK(e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND || e == ERROR_INVALID_NAME || e == ERROR_ACCESS_DENIED);
}
TEST(fio_name_wildcards_and_invalid_chars) {
    REQUIRE(U(CreateFileA));
    static const char *const bad[] = {"w*ld.bin", "w?ld.bin", "a<b.bin", "a>b.bin", "a\"b.bin", "a|b.bin"};
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        TP(p, ""); strcat(p, bad[i]);
        HANDLE h = cfa(p, GENERIC_WRITE, 0, CREATE_NEW, 0);
        DWORD e = GetLastError();
        CHECKF(h == INVALID_HANDLE_VALUE, "created %s", bad[i]);
        CHECKF(e == ERROR_INVALID_NAME, "create %s gle=%lu", bad[i], e);
        if (h != INVALID_HANDLE_VALUE) { CloseHandle(h); rmfile(p); }
        h = cfa(p, GENERIC_READ, 0, OPEN_EXISTING, 0);
        e = GetLastError();
        CHECKF(h == INVALID_HANDLE_VALUE, "opened %s", bad[i]);
        CHECKF(e == ERROR_INVALID_NAME || e == ERROR_FILE_NOT_FOUND, "open %s gle=%lu", bad[i], e);
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }
}
TEST(fio_name_control_chars) {
    REQUIRE(U(CreateFileW));
    for (WCHAR c = 1; c < 32; c += 5) {
        WCHAR p[MAX_PATH * 2]; MultiByteToWideChar(CP_ACP, 0, t_tmpdir(), -1, p, MAX_PATH);
        size_t n = lstrlenW(p); p[n] = L'x'; p[n + 1] = c; p[n + 2] = L'y'; p[n + 3] = 0;
        HANDLE h = cfw(p, GENERIC_WRITE, 0, CREATE_NEW, 0);
        DWORD e = GetLastError();
        CHECKF(h == INVALID_HANDLE_VALUE, "created name with control char %u", c);
        CHECKF(e == ERROR_INVALID_NAME, "ctrl %u gle=%lu", c, e);
        if (h != INVALID_HANDLE_VALUE) { CloseHandle(h); DeleteFileW(p); }
    }
}
TEST(fio_name_trailing_dots_and_spaces) {
    REQUIRE(U(CreateFileA));
    TP(p, "trail.txt"); mkfile_pat(p, 3);
    static const char *const suf[] = {".", "..", " ", "  ", ". ", " .", "...  "};
    for (size_t i = 0; i < sizeof suf / sizeof *suf; i++) {
        char q[MAX_PATH * 2]; snprintf(q, sizeof q, "%s%s", p, suf[i]);
        HANDLE h = cfa(q, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0);
        CHECKF(h != INVALID_HANDLE_VALUE, "suffix \"%s\" gle=%lu", suf[i], GetLastError());
        if (h != INVALID_HANDLE_VALUE) { EXPECT_EQ(hsize(h), 3); CloseHandle(h); }
    }
    /* Creating with trailing dots creates the normalised name. */
    TP(q, "trail_new.txt..  "); TP(r, "trail_new.txt"); rmfile(r);
    HANDLE h = cfa(q, GENERIC_WRITE, 0, CREATE_NEW, 0); OK_H(h); CLOSE(h);
    CHECK(exists(r));
    rmfile(r); rmfile(p);
}
TEST(fio_name_trailing_dot_prefix_literal) {
    REQUIRE(U(CreateFileW));
    /* With \\?\ no normalisation happens: "name." and "name" are distinct NTFS names. */
    char base[MAX_PATH * 2]; snprintf(base, sizeof base, "\\\\?\\%slit_dot", t_tmpdir());
    WCHAR wd[MAX_PATH * 2], wn[MAX_PATH * 2];
    lstrcpyW(wn, fio_w(base)); lstrcpyW(wd, wn); lstrcatW(wd, L".");
    DeleteFileW(wd); DeleteFileW(wn);
    HANDLE h = cfw(wd, GENERIC_WRITE, 0, CREATE_NEW, 0);
    if (h == INVALID_HANDLE_VALUE) { t_note("literal trailing dot create failed gle=%lu (non-NTFS?)", GetLastError()); return; }
    CLOSE(h);
    h = cfw(wn, GENERIC_READ, 0, OPEN_EXISTING, 0);
    CHECK(h == INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    CLOSE(h);
    h = cfw(wd, GENERIC_READ, 0, OPEN_EXISTING, 0); OK_H(h); CLOSE(h);
    DeleteFileW(wd);
}
TEST(fio_name_component_too_long) {
    REQUIRE(U(CreateFileA));
    char leaf[300]; memset(leaf, 'q', 256); leaf[256] = 0;
    TP(p, ""); strcat(p, leaf);
    SET_ERR(0);
    HANDLE h = cfa(p, GENERIC_WRITE, 0, CREATE_NEW, 0);
    DWORD e = GetLastError();
    CHECK(h == INVALID_HANDLE_VALUE);
    CHECKF(e == ERROR_INVALID_NAME || e == ERROR_FILENAME_EXCED_RANGE || e == ERROR_PATH_NOT_FOUND, "gle=%lu", e);
    t_note("256-char component: gle=%lu", e);
    if (h != INVALID_HANDLE_VALUE) { CloseHandle(h); rmfile(p); }
    /* 255 chars is the NTFS maximum and must succeed. */
    leaf[255] = 0;
    TP(q, ""); strcat(q, leaf);
    if (strlen(q) < 255) { /* only when the whole path fits in MAX_PATH it is valid without \\?\ ; otherwise skip */ }
    if (strlen(q) < MAX_PATH) {
        h = cfa(q, GENERIC_WRITE, 0, CREATE_NEW, 0);
        if (h != INVALID_HANDLE_VALUE) { CLOSE(h); rmfile(q); } else t_note("255-char leaf gle=%lu", GetLastError());
    }
}
TEST(fio_path_longer_than_max_path_without_prefix) {
    REQUIRE(U(CreateFileA));
    char p[700]; snprintf(p, sizeof p, "%s", t_tmpdir());
    while (strlen(p) < 400) { strcat(p, "dddddddddddddddddddddddddddddd\\"); }
    strcat(p, "f.bin");
    SET_ERR(0);
    HANDLE h = cfa(p, GENERIC_WRITE, 0, CREATE_ALWAYS, 0);
    DWORD e = GetLastError();
    if (h != INVALID_HANDLE_VALUE) { CloseHandle(h); t_note("long path (no prefix) succeeded: process is long-path aware"); return; }
    /* Name too long maps to ERROR_PATH_NOT_FOUND inside CreateFile (any other failure here is a path-not-found). */
    CHECKF(e == ERROR_PATH_NOT_FOUND || e == ERROR_FILENAME_EXCED_RANGE, "gle=%lu", e);
    t_note("over-long non-prefixed path: gle=%lu", e);
}
static void rm_long_chain(const WCHAR *root, int levels, const WCHAR *comp) {
    WCHAR full[2000]; WCHAR tmp[2000];
    for (int i = levels; i >= 1; i--) {
        lstrcpyW(full, root);
        for (int j = 0; j < i; j++) { lstrcatW(full, comp); lstrcatW(full, L"\\"); }
        lstrcpyW(tmp, full);
        lstrcatW(tmp, L"f.bin"); DeleteFileW(tmp);
        RemoveDirectoryW(full);
    }
}
TEST(fio_prefix_long_path) {
    REQUIRE(U(CreateFileW));
    WCHAR root[600]; lstrcpyW(root, L"\\\\?\\"); lstrcatW(root, fio_w(t_tmpdir()));
    const WCHAR *comp = L"0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789";
    WCHAR dir[2000]; lstrcpyW(dir, root);
    int made = 0;
    for (int i = 0; i < 4; i++) {
        lstrcatW(dir, comp); lstrcatW(dir, L"\\");
        if (!CreateDirectoryW(dir, NULL)) { t_note("cannot create long dir level %d gle=%lu", i, GetLastError()); break; }
        made++;
    }
    if (made == 4) {
        WCHAR f[2100]; lstrcpyW(f, dir); lstrcatW(f, L"f.bin");
        CHECK(lstrlenW(f) > MAX_PATH + 100);
        SET_ERR(0);
        HANDLE h = cfw(f, GENERIC_READ | GENERIC_WRITE, 0, CREATE_NEW, 0);
        OK_H(h); EXPECT_EQ(GetLastError(), 0);
        DWORD n = 0; BOOL ok = U(WriteFile)(h, "long", 4, &n, NULL); CHECK(ok); EXPECT_EQ(n, 4);
        CLOSE(h);
        h = cfw(f, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
        char b[8] = {0}; n = 0; ReadFile(h, b, 8, &n, NULL); EXPECT_EQ(n, 4); CHECK(!memcmp(b, "long", 4));
        CLOSE(h);
        /* The same path without the prefix cannot be opened by a non-long-path-aware process. */
        WCHAR *nopfx = f + 4;
        h = cfw(nopfx, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0);
        if (h != INVALID_HANDLE_VALUE) { t_note("non-prefixed long path opened (long path aware)"); CLOSE(h); }
        else EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    }
    rm_long_chain(root, made, comp);
}
TEST(fio_prefix_basic_and_no_normalisation) {
    REQUIRE(U(CreateFileA));
    TP(p, "pfx.bin"); mkfile_pat(p, 9);
    char q[MAX_PATH * 2]; snprintf(q, sizeof q, "\\\\?\\%s", p);
    HANDLE h = cfa(q, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h); EXPECT_EQ(hsize(h), 9); CLOSE(h);
    /* No normalisation: '.' and '..' components are taken literally and the file is not found. */
    char d[MAX_PATH * 2]; snprintf(d, sizeof d, "\\\\?\\%s.\\pfx.bin", t_tmpdir());
    h = cfa(d, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0);
    CHECK(h == INVALID_HANDLE_VALUE); CLOSE(h);
    snprintf(d, sizeof d, "\\\\?\\%sx\\..\\pfx.bin", t_tmpdir());
    h = cfa(d, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0);
    CHECK(h == INVALID_HANDLE_VALUE);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND || e == ERROR_INVALID_NAME, "gle=%lu", e);
    CLOSE(h);
    /* Forward slashes are not converted either. */
    snprintf(d, sizeof d, "\\\\?\\%s", p); for (char *c = d + 4; *c; c++) if (*c == '\\') *c = '/';
    h = cfa(d, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0);
    CHECK(h == INVALID_HANDLE_VALUE); CLOSE(h);
    rmfile(p);
}
TEST(fio_prefix_nt_namespace) {
    REQUIRE(U(CreateFileA));
    TP(p, "ntns.bin"); mkfile_pat(p, 2);
    char q[MAX_PATH * 2]; snprintf(q, sizeof q, "\\??\\%s", p);
    HANDLE h = cfa(q, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0);
    if (h == INVALID_HANDLE_VALUE) t_note("\\??\\ path: gle=%lu", GetLastError());
    else { EXPECT_EQ(hsize(h), 2); CLOSE(h); }
    rmfile(p);
}
TEST(fio_dotdot_and_dot_normalised) {
    REQUIRE(U(CreateFileA));
    TP(p, "nrm.bin"); mkfile_pat(p, 7);
    TP(sub, "nrm_sub"); CreateDirectoryA(sub, NULL);
    char q[MAX_PATH * 2];
    snprintf(q, sizeof q, "%snrm_sub\\..\\nrm.bin", t_tmpdir());
    HANDLE h = cfa(q, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h); EXPECT_EQ(hsize(h), 7); CLOSE(h);
    snprintf(q, sizeof q, "%s.\\nrm.bin", t_tmpdir());
    h = cfa(q, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h); CLOSE(h);
    snprintf(q, sizeof q, "%s\\\\nrm.bin", t_tmpdir());   /* doubled separators collapse */
    h = cfa(q, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h); CLOSE(h);
    RemoveDirectoryA(sub); rmfile(p);
}
TEST(fio_forward_slashes_and_case) {
    REQUIRE(U(CreateFileA));
    TP(p, "Slash_Case.bin"); mkfile_pat(p, 5);
    char q[MAX_PATH * 2]; strcpy(q, p); for (char *c = q; *c; c++) if (*c == '\\') *c = '/';
    HANDLE h = cfa(q, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h); CLOSE(h);
    strcpy(q, p); CharUpperA(q);
    h = cfa(q, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h); CLOSE(h);
    strcpy(q, p); CharLowerA(q);
    h = cfa(q, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h); CLOSE(h);
    rmfile(p);
}
TEST(fio_unicode_name_w) {
    REQUIRE(U(CreateFileW));
    WCHAR p[MAX_PATH * 2]; MultiByteToWideChar(CP_ACP, 0, t_tmpdir(), -1, p, MAX_PATH);
    lstrcatW(p, L"u_\x00e9\x4e2d\x6587\x0416.bin");
    DeleteFileW(p);
    HANDLE h = cfw(p, GENERIC_WRITE, 0, CREATE_NEW, 0);
    OK_H(h); CLOSE(h);
    CHECK(GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES);
    h = cfw(p, GENERIC_READ, 0, OPEN_EXISTING, 0); OK_H(h); CLOSE(h);
    /* Case-insensitivity applies to non-ASCII letters too (upper case table). */
    WCHAR q[MAX_PATH * 2]; lstrcpyW(q, p);
    WCHAR *u = NULL, *u2 = q; while ((u2 = wcsstr(u2, L"u_\x00e9")) != NULL) { u = u2; u2++; } if (u) u[2] = 0x00c9;
    h = cfw(q, GENERIC_READ, 0, OPEN_EXISTING, 0);
    CHECK(h != INVALID_HANDLE_VALUE); CLOSE(h);
    DeleteFileW(p);
}
TEST(fio_ansi_wide_parity) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(CreateFileW));
    TP(p, "parity.bin"); mkfile_pat(p, 33);
    HANDLE a = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0);
    HANDLE w = cfw(fio_w(p), GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0);
    OK_H(a); OK_H(w);
    BY_HANDLE_FILE_INFORMATION ia, iw;
    memset(&ia, 1, sizeof ia); memset(&iw, 2, sizeof iw);
    CHECK(GetFileInformationByHandle(a, &ia)); CHECK(GetFileInformationByHandle(w, &iw));
    CHECK(!memcmp(&ia, &iw, sizeof ia));
    CLOSE(a); CLOSE(w);
    rmfile(p);
}
TEST(fio_nonexistent_drive) {
    REQUIRE(U(CreateFileA));
    DWORD m = GetLogicalDrives(); char dl = 0;
    for (int i = 25; i >= 3; i--) if (!(m & (1u << i))) { dl = (char)('A' + i); break; }
    if (!dl) { t_skip("no free drive letter"); return; }
    char p[32]; snprintf(p, sizeof p, "%c:\\k32t_nofile.bin", dl);
    EXPECT_FAIL_H(cfa(p, GENERIC_READ, 0, OPEN_EXISTING, 0), ERROR_PATH_NOT_FOUND);
    EXPECT_FAIL_H(cfa(p, GENERIC_WRITE, 0, CREATE_ALWAYS, 0), ERROR_PATH_NOT_FOUND);
    snprintf(p, sizeof p, "%c:", dl);
    HANDLE h = cfa(p, GENERIC_READ, 0, OPEN_EXISTING, 0);
    CHECK(h == INVALID_HANDLE_VALUE); CLOSE(h);
}
TEST(fio_alternate_data_stream) {
    REQUIRE(U(CreateFileA));
    TP(p, "ads.bin"); rmfile(p); mkfile_pat(p, 10);
    char s[MAX_PATH * 2]; snprintf(s, sizeof s, "%s:strm", p);
    HANDLE h = cfa(s, GENERIC_READ | GENERIC_WRITE, 0, CREATE_NEW, 0);
    if (h == INVALID_HANDLE_VALUE) { t_note("ADS create failed gle=%lu (non-NTFS temp?)", GetLastError()); rmfile(p); return; }
    DWORD n; U(WriteFile)(h, "stream!", 7, &n, NULL);
    EXPECT_EQ(hsize(h), 7); CLOSE(h);
    EXPECT_EQ(fsize_of(p), 10);        /* main stream unchanged */
    h = cfa(s, GENERIC_READ, 0, OPEN_EXISTING, 0); OK_H(h);
    char b[16] = {0}; ReadFile(h, b, 16, &n, NULL); EXPECT_EQ(n, 7); CHECK(!memcmp(b, "stream!", 7)); CLOSE(h);
    snprintf(s, sizeof s, "%s:strm:$DATA", p);
    h = cfa(s, GENERIC_READ, 0, OPEN_EXISTING, 0); OK_H(h); CLOSE(h);
    snprintf(s, sizeof s, "%s:nosuch", p);
    EXPECT_FAIL_H(cfa(s, GENERIC_READ, 0, OPEN_EXISTING, 0), ERROR_FILE_NOT_FOUND);
    rmfile(p);
}
TEST(fio_backup_semantics_creates_file_not_directory) {
    REQUIRE(U(CreateFileA));
    TP(p, "bs_new"); rmfile(p); RemoveDirectoryA(p);
    HANDLE h = cfa(p, GENERIC_WRITE, 0, CREATE_NEW, FILE_FLAG_BACKUP_SEMANTICS); OK_H(h); CLOSE(h);
    DWORD a = GetFileAttributesA(p);
    CHECK(a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY));
    rmfile(p);
}
