#include "fio.h"

/* ---------- share matrix ---------- */
static const DWORD g_acc[] = {0, GENERIC_READ, GENERIC_WRITE, GENERIC_READ | GENERIC_WRITE, DELETE,
                              FILE_APPEND_DATA, FILE_READ_DATA, FILE_WRITE_DATA, FILE_READ_ATTRIBUTES | SYNCHRONIZE};
static const DWORD g_shr[] = {0, FILE_SHARE_READ, FILE_SHARE_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_SHARE_DELETE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_SHARE_READ | FILE_SHARE_DELETE,
                              FILE_SHARE_WRITE | FILE_SHARE_DELETE};
static DWORD need(DWORD a) {
    DWORD n = 0;
    if (a & (GENERIC_READ | FILE_READ_DATA | GENERIC_EXECUTE | FILE_EXECUTE | GENERIC_ALL)) n |= FILE_SHARE_READ;
    if (a & (GENERIC_WRITE | FILE_WRITE_DATA | FILE_APPEND_DATA | GENERIC_ALL)) n |= FILE_SHARE_WRITE;
    if (a & (DELETE | GENERIC_ALL)) n |= FILE_SHARE_DELETE;
    return n;
}
/* Share checks are skipped entirely for opens that request none of read/write/delete. */
static int conflict(DWORD a1, DWORD s1, DWORD a2, DWORD s2) { if (!need(a1) || !need(a2)) return 0; return (need(a2) & ~s1) || (need(a1) & ~s2); }

TEST(fio_share_matrix_createfilea) {
    REQUIRE(U(CreateFileA));
    TP(p, "shm.bin"); mkfile_pat(p, 16);
    int bad = 0, total = 0;
    for (size_t a1 = 0; a1 < sizeof g_acc / sizeof *g_acc; a1++)
    for (size_t s1 = 0; s1 < sizeof g_shr / sizeof *g_shr; s1++) {
        HANDLE h1 = cfa(p, g_acc[a1], g_shr[s1], OPEN_EXISTING, 0);
        if (h1 == INVALID_HANDLE_VALUE) { CHECKF(0, "first open a=%lx s=%lx gle=%lu", g_acc[a1], g_shr[s1], GetLastError()); continue; }
        for (size_t a2 = 0; a2 < sizeof g_acc / sizeof *g_acc; a2++)
        for (size_t s2 = 0; s2 < sizeof g_shr / sizeof *g_shr; s2++) {
            SET_ERR(0);
            HANDLE h2 = cfa(p, g_acc[a2], g_shr[s2], OPEN_EXISTING, 0);
            DWORD e = GetLastError();
            int cf = conflict(g_acc[a1], g_shr[s1], g_acc[a2], g_shr[s2]);
            total++;
            if (cf) { if (h2 != INVALID_HANDLE_VALUE || e != ERROR_SHARING_VIOLATION) { bad++; CHECKF(0, "a1=%lx s1=%lx a2=%lx s2=%lx expected sharing violation got h=%p gle=%lu", g_acc[a1], g_shr[s1], g_acc[a2], g_shr[s2], h2, e); } }
            else { if (h2 == INVALID_HANDLE_VALUE) { bad++; CHECKF(0, "a1=%lx s1=%lx a2=%lx s2=%lx expected success got gle=%lu", g_acc[a1], g_shr[s1], g_acc[a2], g_shr[s2], e); } }
            if (h2 != INVALID_HANDLE_VALUE) CloseHandle(h2);
        }
        CloseHandle(h1);
    }
    CHECK(total > 4000);
    if (!bad) CHECK(1);
    rmfile(p);
}
TEST(fio_share_matrix_createfilew_dispositions) {
    REQUIRE(U(CreateFileW));
    TP(p, "shm_w.bin"); mkfile_pat(p, 16);
    /* A second open with OPEN_ALWAYS must see the same sharing rules. */
    HANDLE h1 = cfw(fio_w(p), GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h1);
    EXPECT_FAIL_H(cfw(fio_w(p), GENERIC_WRITE, FILE_SHARE_READ, OPEN_ALWAYS, 0), ERROR_SHARING_VIOLATION);
    EXPECT_FAIL_H(cfw(fio_w(p), GENERIC_WRITE, FILE_SHARE_READ, CREATE_ALWAYS, 0), ERROR_SHARING_VIOLATION);
    EXPECT_FAIL_H(cfw(fio_w(p), GENERIC_WRITE, FILE_SHARE_READ, TRUNCATE_EXISTING, 0), ERROR_SHARING_VIOLATION);
    EXPECT_FAIL_H(cfw(fio_w(p), GENERIC_WRITE, FILE_SHARE_READ, CREATE_NEW, 0), ERROR_FILE_EXISTS);
    EXPECT_EQ(fsize_of(p), 16);
    HANDLE h2 = cfw(fio_w(p), GENERIC_READ, FILE_SHARE_READ, OPEN_ALWAYS, 0); OK_H(h2);
    EXPECT_EQ(GetLastError(), ERROR_ALREADY_EXISTS);
    CLOSE(h1); CLOSE(h2); rmfile(p);
}
TEST(fio_share_violation_leaves_content) {
    REQUIRE(U(CreateFileA));
    TP(p, "shv.bin"); mkfile_pat(p, 64);
    HANDLE h1 = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h1);
    EXPECT_FAIL_H(cfa(p, GENERIC_WRITE, 0, CREATE_ALWAYS, 0), ERROR_SHARING_VIOLATION);
    CLOSE(h1);
    EXPECT_EQ(fsize_of(p), 64);
    rmfile(p);
}
TEST(fio_share_none_blocks_everything_but_attrib_open) {
    REQUIRE(U(CreateFileA));
    TP(p, "sh0.bin"); mkfile_pat(p, 8);
    HANDLE h1 = cfa(p, GENERIC_READ | GENERIC_WRITE, 0, OPEN_EXISTING, 0); OK_H(h1);
    EXPECT_FAIL_H(cfa(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, 0), ERROR_SHARING_VIOLATION);
    HANDLE h2 = cfa(p, 0, 0, OPEN_EXISTING, 0);          /* zero access: attributes only */
    OK_H(h2);
    BY_HANDLE_FILE_INFORMATION bi;
    CHECK(GetFileInformationByHandle(h2, &bi)); EXPECT_EQ(bi.nFileSizeLow, 8);
    CLOSE(h2);
    h2 = cfa(p, FILE_READ_ATTRIBUTES | SYNCHRONIZE | READ_CONTROL, 0, OPEN_EXISTING, 0); OK_H(h2); CLOSE(h2);
    CLOSE(h1); rmfile(p);
}
TEST(fio_share_delete_and_deletefile) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(DeleteFileA));
    TP(p, "shd.bin"); mkfile_pat(p, 8);
    HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, 0); OK_H(h);
    EXPECT_FAIL_B(U(DeleteFileA)(p), ERROR_SHARING_VIOLATION);
    CHECK(exists(p));
    CLOSE(h);
    h = cfa(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, OPEN_EXISTING, 0); OK_H(h);
    BOOL ok = U(DeleteFileA)(p);
    CHECK(ok);
    if (ok) {
        /* Name disappears at once under POSIX delete semantics, or stays pending until close on older behaviour. */
        t_note("after DeleteFile with open share-delete handle: exists=%d", exists(p));
        DWORD n = 0; char b[8]; CHECK(ReadFile(h, b, 8, &n, NULL)); EXPECT_EQ(n, 8);
    }
    CLOSE(h);
    CHECK(!exists(p));
}
TEST(fio_share_write_only_conflict_with_append) {
    REQUIRE(U(CreateFileA));
    TP(p, "shapp.bin"); mkfile_pat(p, 8);
    HANDLE h1 = cfa(p, FILE_APPEND_DATA, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h1);
    EXPECT_FAIL_H(cfa(p, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, 0), ERROR_SHARING_VIOLATION);
    EXPECT_FAIL_H(cfa(p, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, 0), ERROR_SHARING_VIOLATION);
    HANDLE h2 = cfa(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, 0);
    OK_H(h2);
    CLOSE(h2); CLOSE(h1); rmfile(p);
}
TEST(fio_share_same_process_vs_child_process) {
    REQUIRE(U(CreateFileA));
    TP(p, "shchild.bin"); mkfile_pat(p, 8);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
    char arg[MAX_PATH * 2 + 8]; snprintf(arg, sizeof arg, "\"%s\"", p);
    EXPECT_EQ(t_run_child("fio_shchild", arg, 20000), 0);     /* child expects sharing violations for write */
    CLOSE(h);
    EXPECT_EQ(t_run_child("fio_shchild_free", arg, 20000), 0);
    rmfile(p);
}
CHILD(fio_shchild) {
    if (argc < 2 || !U(CreateFileA)) return 90;
    int r = 0;
    SetLastError(0);
    HANDLE h = U(CreateFileA)(argv[1], GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h != INVALID_HANDLE_VALUE || GetLastError() != ERROR_SHARING_VIOLATION) r |= 1;
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    h = U(CreateFileA)(argv[1], GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) r |= 2; else CloseHandle(h);
    return r;
}
CHILD(fio_shchild_free) {
    if (argc < 2 || !U(CreateFileA)) return 90;
    HANDLE h = U(CreateFileA)(argv[1], GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 1;
    CloseHandle(h);
    return 0;
}

/* ---------- access rights / mapping ---------- */
TEST(fio_generic_access_mapping) {
    REQUIRE(U(CreateFileA)); REQUIRE(UNT(NtQueryInformationFile));
    TP(p, "gmap.bin"); mkfile_pat(p, 4);
    static const struct { DWORD in, want; const char *n; } t[] = {
        {0, 0x00100080, "0"},
        {GENERIC_READ, 0x00120089, "GENERIC_READ"},
        {GENERIC_WRITE, 0x00120196, "GENERIC_WRITE"},
        {GENERIC_READ | GENERIC_WRITE, 0x0012019F, "READ|WRITE"},
        {GENERIC_EXECUTE, 0x001200A0, "GENERIC_EXECUTE"},
        {GENERIC_ALL, 0x001F01FF, "GENERIC_ALL"},
        {FILE_READ_DATA, 0x00100081, "FILE_READ_DATA"},
        {FILE_WRITE_DATA, 0x00100082 | 0x80, "FILE_WRITE_DATA"},
        {FILE_APPEND_DATA, 0x00100084 | 0x80, "FILE_APPEND_DATA"},
        {DELETE, 0x00110080, "DELETE"},
        {FILE_READ_ATTRIBUTES, 0x00100080, "FILE_READ_ATTRIBUTES"},
        {FILE_WRITE_ATTRIBUTES, 0x00100180, "FILE_WRITE_ATTRIBUTES"},
        {READ_CONTROL, 0x00120080, "READ_CONTROL"},
        {MAXIMUM_ALLOWED, 0, "MAXIMUM_ALLOWED"},
    };
    for (size_t i = 0; i < sizeof t / sizeof *t; i++) {
        HANDLE h = cfa(p, t[i].in, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, OPEN_EXISTING, 0);
        if (h == INVALID_HANDLE_VALUE) { CHECKF(0, "%s open gle=%lu", t[i].n, GetLastError()); continue; }
        DWORD m = fio_access_mask(h);
        if (t[i].in == MAXIMUM_ALLOWED) { CHECKF((m & 0x1F01FF) != 0, "max allowed mask 0x%lx", m); t_note("MAXIMUM_ALLOWED -> 0x%lx", m); }
        else CHECKF(m == t[i].want, "%s: mask 0x%lx want 0x%lx", t[i].n, m, t[i].want);
        CloseHandle(h);
    }
    rmfile(p);
}
TEST(fio_access_read_only_handle_rejects_write) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(WriteFile));
    TP(p, "acc_r.bin"); mkfile_pat(p, 10);
    HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
    DWORD n = 77;
    EXPECT_FAIL_B(U(WriteFile)(h, "x", 1, &n, NULL), ERROR_ACCESS_DENIED);
    EXPECT_EQ(n, 0);
    EXPECT_FAIL_B(U(SetEndOfFile)(h), ERROR_ACCESS_DENIED);
    EXPECT_EQ(fpos(h), 0);
    CLOSE(h); EXPECT_EQ(fsize_of(p), 10); rmfile(p);
}
TEST(fio_access_write_only_handle_rejects_read) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(ReadFile));
    TP(p, "acc_w.bin"); mkfile_pat(p, 10);
    HANDLE h = cfa(p, GENERIC_WRITE, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
    char b[4]; DWORD n = 77;
    EXPECT_FAIL_B(U(ReadFile)(h, b, 4, &n, NULL), ERROR_ACCESS_DENIED);
    EXPECT_EQ(n, 0);
    EXPECT_EQ(fpos(h), 0);
    CLOSE(h); rmfile(p);
}
TEST(fio_access_zero_handle_cannot_io) {
    REQUIRE(U(CreateFileA));
    TP(p, "acc_0.bin"); mkfile_pat(p, 10);
    HANDLE h = cfa(p, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, 0); OK_H(h);
    char b[4]; DWORD n = 77;
    EXPECT_FAIL_B(U(ReadFile)(h, b, 4, &n, NULL), ERROR_ACCESS_DENIED);
    EXPECT_FAIL_B(U(WriteFile)(h, b, 4, &n, NULL), ERROR_ACCESS_DENIED);
    EXPECT_FAIL_B(U(FlushFileBuffers)(h), ERROR_ACCESS_DENIED);
    CHECK(U(GetFileSizeEx) != NULL);
    LARGE_INTEGER s; CHECK(U(GetFileSizeEx)(h, &s)); EXPECT_EQ(s.QuadPart, 10);
    CLOSE(h); rmfile(p);
}
TEST(fio_access_append_only_always_appends) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(WriteFile));
    TP(p, "acc_app.bin"); mkfile(p, "0123456789", 10);
    HANDLE h = cfa(p, FILE_APPEND_DATA, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
    DWORD n = 0;
    CHECK(U(WriteFile)(h, "AB", 2, &n, NULL)); EXPECT_EQ(n, 2);
    hsetpos(h, 0);
    CHECK(U(WriteFile)(h, "CD", 2, &n, NULL)); EXPECT_EQ(n, 2);
    CLOSE(h);
    char b[32] = {0};
    HANDLE r = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(r);
    ReadFile(r, b, 32, &n, NULL); EXPECT_EQ(n, 14);
    CHECK(!memcmp(b, "0123456789ABCD", 14));
    CLOSE(r); rmfile(p);
}
TEST(fio_access_append_via_generic_write_without_append_right) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(WriteFile));
    TP(p, "acc_gw.bin"); mkfile(p, "0123456789", 10);
    HANDLE h = cfa(p, FILE_WRITE_DATA, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
    DWORD n = 0;
    hsetpos(h, 2);
    CHECK(U(WriteFile)(h, "XY", 2, &n, NULL)); EXPECT_EQ(n, 2);
    EXPECT_EQ(fpos(h), 4);
    CLOSE(h);
    char b[16] = {0};
    HANDLE r = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0);
    ReadFile(r, b, 16, &n, NULL); EXPECT_EQ(n, 10); CHECK(!memcmp(b, "01XY456789", 10));
    CLOSE(r); rmfile(p);
}
TEST(fio_access_readonly_attribute_blocks_write_open) {
    REQUIRE(U(CreateFileA));
    TP(p, "ro_attr.bin"); mkfile_pat(p, 10);
    SetFileAttributesA(p, FILE_ATTRIBUTE_READONLY);
    EXPECT_FAIL_H(cfa(p, GENERIC_WRITE, 0, OPEN_EXISTING, 0), ERROR_ACCESS_DENIED);
    EXPECT_FAIL_H(cfa(p, GENERIC_READ | GENERIC_WRITE, 0, OPEN_ALWAYS, 0), ERROR_ACCESS_DENIED);
    EXPECT_FAIL_H(cfa(p, FILE_APPEND_DATA, 0, OPEN_EXISTING, 0), ERROR_ACCESS_DENIED);
    EXPECT_FAIL_H(cfa(p, GENERIC_WRITE, 0, CREATE_ALWAYS, 0), ERROR_ACCESS_DENIED);
    EXPECT_FAIL_H(cfa(p, GENERIC_WRITE, 0, TRUNCATE_EXISTING, 0), ERROR_ACCESS_DENIED);
    EXPECT_FAIL_H(cfa(p, DELETE, FILE_SHARE_DELETE, OPEN_EXISTING, 0), ERROR_ACCESS_DENIED);
    EXPECT_FAIL_H(cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE), ERROR_ACCESS_DENIED);
    EXPECT_EQ(fsize_of(p), 10); CHECK(exists(p));
    HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h); CLOSE(h);
    h = cfa(p, FILE_WRITE_ATTRIBUTES, 0, OPEN_EXISTING, 0); OK_H(h); CLOSE(h);
    rmfile(p);
}
TEST(fio_attr_create_readonly_new_file_handle_writable) {
    REQUIRE(U(CreateFileA));
    TP(p, "ro_new.bin"); rmfile(p);
    HANDLE h = cfa(p, GENERIC_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_READONLY); OK_H(h);
    DWORD n = 0; CHECK(WriteFile(h, "abc", 3, &n, NULL)); EXPECT_EQ(n, 3);
    CLOSE(h);
    DWORD a = GetFileAttributesA(p);
    CHECK(a & FILE_ATTRIBUTE_READONLY);
    EXPECT_FAIL_H(cfa(p, GENERIC_WRITE, 0, OPEN_EXISTING, 0), ERROR_ACCESS_DENIED);
    rmfile(p);
}
TEST(fio_attr_flags_applied_to_new_file) {
    REQUIRE(U(CreateFileA));
    static const struct { DWORD fl; DWORD mustHave; DWORD mustNot; const char *n; } t[] = {
        {0, FILE_ATTRIBUTE_ARCHIVE, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_READONLY, "none"},
        {FILE_ATTRIBUTE_NORMAL, FILE_ATTRIBUTE_ARCHIVE, FILE_ATTRIBUTE_HIDDEN, "NORMAL"},
        {FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_ARCHIVE, FILE_ATTRIBUTE_SYSTEM, "HIDDEN"},
        {FILE_ATTRIBUTE_SYSTEM, FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_ARCHIVE, FILE_ATTRIBUTE_HIDDEN, "SYSTEM"},
        {FILE_ATTRIBUTE_READONLY, FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_ARCHIVE, FILE_ATTRIBUTE_HIDDEN, "READONLY"},
        {FILE_ATTRIBUTE_TEMPORARY, FILE_ATTRIBUTE_TEMPORARY, FILE_ATTRIBUTE_HIDDEN, "TEMPORARY"},
        {FILE_ATTRIBUTE_NOT_CONTENT_INDEXED, FILE_ATTRIBUTE_NOT_CONTENT_INDEXED, FILE_ATTRIBUTE_HIDDEN, "NOT_CONTENT_INDEXED"},
        {FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_READONLY, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_READONLY, 0, "H|S|R"},
        {FILE_ATTRIBUTE_ARCHIVE, FILE_ATTRIBUTE_ARCHIVE, FILE_ATTRIBUTE_HIDDEN, "ARCHIVE"},
        {FILE_ATTRIBUTE_DIRECTORY, 0, FILE_ATTRIBUTE_DIRECTORY, "DIRECTORY ignored"},
    };
    for (size_t i = 0; i < sizeof t / sizeof *t; i++) {
        TP(p, "attr_new.bin"); rmfile(p);
        HANDLE h = cfa(p, GENERIC_WRITE, 0, CREATE_NEW, t[i].fl);
        if (h == INVALID_HANDLE_VALUE) { CHECKF(0, "%s: create gle=%lu", t[i].n, GetLastError()); continue; }
        CloseHandle(h);
        DWORD a = GetFileAttributesA(p);
        CHECKF((a & t[i].mustHave) == t[i].mustHave, "%s: attrs 0x%lx missing 0x%lx", t[i].n, a, t[i].mustHave);
        CHECKF((a & t[i].mustNot) == 0, "%s: attrs 0x%lx unexpected 0x%lx", t[i].n, a, a & t[i].mustNot);
        rmfile(p);
    }
}
TEST(fio_attr_create_always_hidden_mismatch) {
    REQUIRE(U(CreateFileA));
    TP(p, "hid_ca.bin"); mkfile_pat(p, 10);
    SetFileAttributesA(p, FILE_ATTRIBUTE_HIDDEN);
    EXPECT_FAIL_H(cfa(p, GENERIC_WRITE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL), ERROR_ACCESS_DENIED);
    EXPECT_EQ(fsize_of(p), 10);
    HANDLE h = cfa(p, GENERIC_WRITE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN);
    OK_H(h); EXPECT_EQ(GetLastError(), ERROR_ALREADY_EXISTS); CLOSE(h);
    EXPECT_EQ(fsize_of(p), 0);
    /* Hidden-attribute interplay with TRUNCATE_EXISTING is file system behaviour: report only. */
    h = cfa(p, GENERIC_WRITE, 0, TRUNCATE_EXISTING, 0);
    t_note("TRUNCATE_EXISTING on hidden file: %s gle=%lu", h != INVALID_HANDLE_VALUE ? "ok" : "fail", GetLastError());
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    rmfile(p);
}
TEST(fio_attr_create_always_system_mismatch) {
    REQUIRE(U(CreateFileA));
    TP(p, "sys_ca.bin"); mkfile_pat(p, 10);
    SetFileAttributesA(p, FILE_ATTRIBUTE_SYSTEM);
    EXPECT_FAIL_H(cfa(p, GENERIC_WRITE, 0, CREATE_ALWAYS, 0), ERROR_ACCESS_DENIED);
    HANDLE h = cfa(p, GENERIC_WRITE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_SYSTEM); OK_H(h); CLOSE(h);
    rmfile(p);
}
TEST(fio_attr_open_existing_ignores_new_attributes) {
    REQUIRE(U(CreateFileA));
    TP(p, "attr_ign.bin"); mkfile_pat(p, 3);
    HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY); OK_H(h); CLOSE(h);
    DWORD a = GetFileAttributesA(p);
    CHECK(!(a & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY)));
    rmfile(p);
}
TEST(fio_attr_open_always_existing_ignores_attributes) {
    REQUIRE(U(CreateFileA));
    TP(p, "attr_oa.bin"); mkfile_pat(p, 3);
    HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_ALWAYS, FILE_ATTRIBUTE_HIDDEN); OK_H(h);
    EXPECT_EQ(GetLastError(), ERROR_ALREADY_EXISTS); CLOSE(h);
    CHECK(!(GetFileAttributesA(p) & FILE_ATTRIBUTE_HIDDEN));
    rmfile(p);
}

/* ---------- directories ---------- */
TEST(fio_dir_open_needs_backup_semantics) {
    REQUIRE(U(CreateFileA));
    TP(p, "dir_open"); RemoveDirectoryA(p); CreateDirectoryA(p, NULL);
    EXPECT_FAIL_H(cfa(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, 0), ERROR_ACCESS_DENIED);
    EXPECT_FAIL_H(cfa(p, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, 0), ERROR_ACCESS_DENIED);
    EXPECT_FAIL_H(cfw(fio_w(p), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, 0), ERROR_ACCESS_DENIED);
    HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS);
    OK_H(h);
    BY_HANDLE_FILE_INFORMATION bi; CHECK(GetFileInformationByHandle(h, &bi));
    CHECK(bi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
    EXPECT_EQ(GetFileType(h), FILE_TYPE_DISK);
    CLOSE(h);
    RemoveDirectoryA(p);
}
TEST(fio_dir_trailing_backslash) {
    REQUIRE(U(CreateFileA));
    TP(d, "dir_tb"); RemoveDirectoryA(d); CreateDirectoryA(d, NULL);
    char q[MAX_PATH * 2]; snprintf(q, sizeof q, "%s\\", d);
    HANDLE h = cfa(q, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS); OK_H(h); CLOSE(h);
    /* Without backup semantics a trailing separator reports path-not-found instead of access-denied. */
    EXPECT_FAIL_H(cfa(q, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, 0), ERROR_PATH_NOT_FOUND);
    RemoveDirectoryA(d);
}
TEST(fio_file_with_trailing_backslash) {
    REQUIRE(U(CreateFileA));
    TP(p, "ftb.bin"); mkfile_pat(p, 2);
    char q[MAX_PATH * 2]; snprintf(q, sizeof q, "%s\\", p);
    HANDLE h = cfa(q, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0);
    CHECK(h == INVALID_HANDLE_VALUE);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_INVALID_NAME || e == ERROR_PATH_NOT_FOUND || e == ERROR_DIRECTORY, "gle=%lu", e);
    t_note("file path with trailing backslash: gle=%lu", e);
    h = cfa(q, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS);
    CHECK(h == INVALID_HANDLE_VALUE); CLOSE(h);
    rmfile(p);
}
TEST(fio_dir_create_new_on_directory_name) {
    REQUIRE(U(CreateFileA));
    TP(d, "dir_cd"); RemoveDirectoryA(d); CreateDirectoryA(d, NULL);
    EXPECT_FAIL_H(cfa(d, GENERIC_WRITE, 0, CREATE_ALWAYS, FILE_FLAG_BACKUP_SEMANTICS), ERROR_ACCESS_DENIED);
    EXPECT_FAIL_H(cfa(d, GENERIC_WRITE, 0, TRUNCATE_EXISTING, FILE_FLAG_BACKUP_SEMANTICS), ERROR_ACCESS_DENIED);
    HANDLE h = cfa(d, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_ALWAYS, FILE_FLAG_BACKUP_SEMANTICS);
    OK_H(h); EXPECT_EQ(GetLastError(), ERROR_ALREADY_EXISTS); CLOSE(h);
    RemoveDirectoryA(d);
}
TEST(fio_dir_handle_io_fails) {
    REQUIRE(U(CreateFileA));
    TP(d, "dir_io"); RemoveDirectoryA(d); CreateDirectoryA(d, NULL);
    HANDLE h = cfa(d, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS); OK_H(h);
    char b[16]; DWORD n = 77;
    SET_ERR(0);
    BOOL ok = U(ReadFile)(h, b, 16, &n, NULL);
    DWORD e = GetLastError();
    CHECK(!ok); EXPECT_EQ(n, 0);
    CHECKF(e == ERROR_INVALID_FUNCTION || e == ERROR_ACCESS_DENIED, "gle=%lu", e);
    t_note("ReadFile(dir) gle=%lu", e);
    CLOSE(h);
    RemoveDirectoryA(d);
}
TEST(fio_dir_delete_on_close) {
    REQUIRE(U(CreateFileA));
    TP(d, "dir_doc"); RemoveDirectoryA(d); CreateDirectoryA(d, NULL);
    HANDLE h = cfa(d, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, OPEN_EXISTING,
                   FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_DELETE_ON_CLOSE);
    OK_H(h); CLOSE(h);
    CHECK(!exists(d));
    RemoveDirectoryA(d);
}

/* ---------- flags ---------- */
TEST(fio_flag_delete_on_close) {
    REQUIRE(U(CreateFileA));
    TP(p, "doc.bin"); rmfile(p);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, CREATE_NEW, FILE_FLAG_DELETE_ON_CLOSE);
    OK_H(h); CHECK(exists(p));
    DWORD m = fio_access_mask(h);
    CHECKF(m & DELETE, "delete-on-close adds DELETE access, mask=0x%lx", m);
    CLOSE(h);
    CHECK(!exists(p));
}
TEST(fio_flag_delete_on_close_blocks_other_opens_without_share_delete) {
    REQUIRE(U(CreateFileA));
    TP(p, "doc_sh.bin"); rmfile(p);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, CREATE_NEW, FILE_FLAG_DELETE_ON_CLOSE);
    OK_H(h);
    /* A later opener must permit delete sharing since the first handle holds DELETE access. */
    EXPECT_FAIL_H(cfa(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, 0), ERROR_SHARING_VIOLATION);
    HANDLE h2 = cfa(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, OPEN_EXISTING, 0); OK_H(h2);
    CLOSE(h);
    t_note("after closing delete-on-close handle with other handle open: exists=%d", exists(p));
    CLOSE(h2);
    CHECK(!exists(p));
}
TEST(fio_flag_delete_on_close_no_share_conflict_with_no_share) {
    REQUIRE(U(CreateFileA));
    TP(p, "doc_sh0.bin"); rmfile(p);
    HANDLE h = cfa(p, GENERIC_WRITE, 0, CREATE_NEW, FILE_FLAG_DELETE_ON_CLOSE); OK_H(h);
    EXPECT_FAIL_H(cfa(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, OPEN_EXISTING, 0), ERROR_SHARING_VIOLATION);
    HANDLE z = cfa(p, 0, 0, OPEN_EXISTING, 0); OK_H(z); CLOSE(z);   /* zero-access opens never conflict */
    CLOSE(h);
}
TEST(fio_flag_delete_on_close_existing_file) {
    REQUIRE(U(CreateFileA));
    TP(p, "doc_ex.bin"); mkfile_pat(p, 10);
    HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, FILE_FLAG_DELETE_ON_CLOSE); OK_H(h);
    CHECK(exists(p));
    CLOSE(h);
    CHECK(!exists(p));
}
TEST(fio_flag_delete_on_close_duplicate_keeps_file) {
    REQUIRE(U(CreateFileA));
    TP(p, "doc_dup.bin"); rmfile(p);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, CREATE_NEW, FILE_FLAG_DELETE_ON_CLOSE); OK_H(h);
    HANDLE d = NULL;
    CHECK(DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS));
    CloseHandle(h);
    CHECK(exists(p));       /* the file object is still referenced by the duplicate */
    CloseHandle(d);
    CHECK(!exists(p));
}
TEST(fio_flag_write_through_and_no_buffering_mode_bits) {
    REQUIRE(U(CreateFileA)); REQUIRE(UNT(NtQueryInformationFile));
    TP(p, "modes.bin"); mkfile_pat(p, 8);
    HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
    DWORD m0 = fio_mode(h);
    CHECKF(m0 != 0xFFFFFFFF && (m0 & 0x20), "plain handle mode 0x%lx lacks SYNCHRONOUS_IO_NONALERT", m0);
    CHECK(!(m0 & (0x2 | 0x8)));
    CLOSE(h);
    h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, FILE_FLAG_WRITE_THROUGH); OK_H(h);
    CHECKF(fio_mode(h) & 0x2, "mode 0x%lx lacks WRITE_THROUGH", fio_mode(h)); CLOSE(h);
    h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING); OK_H(h);
    CHECKF(fio_mode(h) & 0x8, "mode 0x%lx lacks NO_INTERMEDIATE_BUFFERING", fio_mode(h)); CLOSE(h);
    h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN); OK_H(h);
    t_note("sequential-scan mode 0x%lx", fio_mode(h)); CLOSE(h);
    h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS); OK_H(h);
    t_note("random access mode 0x%lx", fio_mode(h)); CLOSE(h);
    h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, FILE_FLAG_OVERLAPPED); OK_H(h);
    DWORD mo = fio_mode(h);
    CHECKF(!(mo & 0x30), "overlapped handle mode 0x%lx has synchronous bits", mo); CLOSE(h);
    rmfile(p);
}
TEST(fio_flag_sqos_on_files) {
    REQUIRE(U(CreateFileA));
    TP(p, "sqos.bin"); mkfile_pat(p, 8);
    static const DWORD f[] = {SECURITY_SQOS_PRESENT, SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS,
        SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, SECURITY_SQOS_PRESENT | SECURITY_IMPERSONATION,
        SECURITY_SQOS_PRESENT | SECURITY_DELEGATION, SECURITY_SQOS_PRESENT | SECURITY_CONTEXT_TRACKING,
        SECURITY_SQOS_PRESENT | SECURITY_EFFECTIVE_ONLY, SECURITY_SQOS_PRESENT | SECURITY_IMPERSONATION | SECURITY_CONTEXT_TRACKING | SECURITY_EFFECTIVE_ONLY,
        SECURITY_ANONYMOUS, SECURITY_IDENTIFICATION, SECURITY_DELEGATION};
    for (size_t i = 0; i < sizeof f / sizeof *f; i++) {
        SET_ERR(0x31);
        HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, f[i]);
        CHECKF(h != INVALID_HANDLE_VALUE, "flags 0x%lx gle=%lu", f[i], GetLastError());
        if (h != INVALID_HANDLE_VALUE) { char b[4]; DWORD n = 0; CHECK(ReadFile(h, b, 4, &n, NULL)); EXPECT_EQ(n, 4); CloseHandle(h); }
    }
    rmfile(p);
}
TEST(fio_flag_misc_hints_accepted) {
    REQUIRE(U(CreateFileA));
    TP(p, "hints.bin"); mkfile_pat(p, 8);
    static const DWORD f[] = {FILE_FLAG_RANDOM_ACCESS, FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_WRITE_THROUGH,
        FILE_FLAG_OPEN_REPARSE_POINT, FILE_FLAG_POSIX_SEMANTICS, FILE_FLAG_BACKUP_SEMANTICS, FILE_FLAG_OVERLAPPED,
        FILE_FLAG_RANDOM_ACCESS | FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_OPEN_NO_RECALL,
        FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED | FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_POSIX_SEMANTICS};
    for (size_t i = 0; i < sizeof f / sizeof *f; i++) {
        HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, f[i]);
        CHECKF(h != INVALID_HANDLE_VALUE, "flags 0x%lx gle=%lu", f[i], GetLastError());
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }
    rmfile(p);
}
TEST(fio_flag_open_reparse_point_on_regular_file) {
    REQUIRE(U(CreateFileA));
    TP(p, "orp.bin"); mkfile_pat(p, 8);
    HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT); OK_H(h);
    EXPECT_EQ(hsize(h), 8); CLOSE(h); rmfile(p);
}
TEST(fio_security_attributes_inherit) {
    REQUIRE(U(CreateFileA));
    TP(p, "inh.bin"); mkfile_pat(p, 8);
    SECURITY_ATTRIBUTES sa; sa.nLength = sizeof sa; sa.lpSecurityDescriptor = NULL; sa.bInheritHandle = TRUE;
    HANDLE h = U(CreateFileA)(p, GENERIC_READ, FILE_SHARE_READ, &sa, OPEN_EXISTING, 0, NULL); OK_H(h);
    DWORD fl = 0; CHECK(GetHandleInformation(h, &fl)); CHECK(fl & HANDLE_FLAG_INHERIT); CLOSE(h);
    sa.bInheritHandle = FALSE;
    h = U(CreateFileA)(p, GENERIC_READ, FILE_SHARE_READ, &sa, OPEN_EXISTING, 0, NULL); OK_H(h);
    CHECK(GetHandleInformation(h, &fl)); CHECK(!(fl & HANDLE_FLAG_INHERIT)); CLOSE(h);
    h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
    CHECK(GetHandleInformation(h, &fl)); CHECK(!(fl & HANDLE_FLAG_INHERIT)); CLOSE(h);
    rmfile(p);
}
TEST(fio_security_attributes_bad_length) {
    REQUIRE(U(CreateFileA));
    TP(p, "sabad.bin"); mkfile_pat(p, 8);
    SECURITY_ATTRIBUTES sa; memset(&sa, 0, sizeof sa); sa.nLength = 0; sa.bInheritHandle = TRUE;
    HANDLE h = U(CreateFileA)(p, GENERIC_READ, FILE_SHARE_READ, &sa, OPEN_EXISTING, 0, NULL);
    t_note("CreateFile with SECURITY_ATTRIBUTES.nLength=0: handle=%p gle=%lu", h, GetLastError());
    CLOSE(h); rmfile(p);
}
TEST(fio_security_attributes_null_dacl_on_new_file) {
    REQUIRE(U(CreateFileA));
    TP(p, "sd.bin"); rmfile(p);
    SECURITY_DESCRIPTOR sd; InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
    SECURITY_ATTRIBUTES sa = {sizeof sa, &sd, FALSE};
    HANDLE h = U(CreateFileA)(p, GENERIC_WRITE, 0, &sa, CREATE_NEW, 0, NULL);
    OK_H(h); CLOSE(h); CHECK(exists(p)); rmfile(p);
}
TEST(fio_template_file_ignored_cases) {
    REQUIRE(U(CreateFileA));
    TP(p, "tmpl_t.bin"); mkfile_pat(p, 5);
    TP(q, "tmpl_n.bin"); rmfile(q);
    HANDLE t = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(t);
    HANDLE h = U(CreateFileA)(q, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, t); OK_H(h); CLOSE(h); rmfile(q);
    h = U(CreateFileA)(p, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, t); OK_H(h); CLOSE(h);
    CloseHandle(t);
    /* a stale/closed template handle is silently ignored */
    h = U(CreateFileA)(q, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, t);
    CHECKF(h != INVALID_HANDLE_VALUE, "stale template gle=%lu", GetLastError()); CLOSE(h); rmfile(q);
    h = U(CreateFileA)(q, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, INVALID_HANDLE_VALUE);
    CHECKF(h != INVALID_HANDLE_VALUE, "INVALID_HANDLE_VALUE template gle=%lu", GetLastError()); CLOSE(h); rmfile(q);
    rmfile(p);
}
TEST(fio_createfile_copies_nothing_from_template_size) {
    REQUIRE(U(CreateFileA));
    TP(p, "tmpl2_t.bin"); mkfile_pat(p, 500);
    TP(q, "tmpl2_n.bin"); rmfile(q);
    HANDLE t = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0);
    HANDLE h = U(CreateFileA)(q, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, t); OK_H(h);
    EXPECT_EQ(hsize(h), 0); CLOSE(h); CloseHandle(t); rmfile(q); rmfile(p);
}

/* ---------- devices ---------- */
TEST(fio_device_nul) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(WriteFile)); REQUIRE(U(ReadFile));
    static const char *const n[] = {"NUL", "\\\\.\\NUL", "nul", "\\\\?\\GLOBALROOT\\Device\\Null", "\\??\\NUL"};
    for (size_t i = 0; i < sizeof n / sizeof *n; i++) {
        HANDLE h = cfa(n[i], GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, 0);
        if (h == INVALID_HANDLE_VALUE) { t_note("open %s failed gle=%lu", n[i], GetLastError()); if (i < 3) CHECKF(0, "%s", n[i]); continue; }
        EXPECT_EQ(GetFileType(h), FILE_TYPE_CHAR);
        DWORD w = 0; char b[16];
        CHECK(U(WriteFile)(h, "hello", 5, &w, NULL)); EXPECT_EQ(w, 5);
        DWORD r = 99; CHECK(U(ReadFile)(h, b, 16, &r, NULL)); EXPECT_EQ(r, 0);
        CloseHandle(h);
    }
}
TEST(fio_device_nul_create_dispositions) {
    REQUIRE(U(CreateFileA));
    HANDLE h = cfa("NUL", GENERIC_WRITE, 0, CREATE_ALWAYS, 0); 
    CHECKF(h != INVALID_HANDLE_VALUE, "gle=%lu", GetLastError()); CLOSE(h);
    h = cfa("NUL", GENERIC_WRITE, 0, OPEN_ALWAYS, 0);
    CHECKF(h != INVALID_HANDLE_VALUE, "gle=%lu", GetLastError()); CLOSE(h);
    h = cfa("NUL", GENERIC_WRITE, 0, CREATE_NEW, 0);
    t_note("CREATE_NEW NUL: %p gle=%lu", h, GetLastError()); CLOSE(h);
}
TEST(fio_device_nul_seek_and_size) {
    REQUIRE(U(CreateFileA));
    HANDLE h = cfa("NUL", GENERIC_READ | GENERIC_WRITE, 0, OPEN_EXISTING, 0); OK_H(h);
    LARGE_INTEGER z, r; z.QuadPart = 100;
    BOOL ok = U(SetFilePointerEx)(h, z, &r, FILE_BEGIN);
    t_note("SetFilePointerEx(NUL) = %d pos=%lld gle=%lu", ok, ok ? r.QuadPart : -1LL, GetLastError());
    LARGE_INTEGER s; ok = U(GetFileSizeEx)(h, &s);
    t_note("GetFileSizeEx(NUL) = %d size=%lld gle=%lu", ok, ok ? s.QuadPart : -1LL, GetLastError());
    CLOSE(h);
}
TEST(fio_device_dos_names_with_extension) {
    REQUIRE(U(CreateFileA));
    HANDLE h = cfa("NUL.txt", GENERIC_WRITE, 0, OPEN_ALWAYS, 0);
    t_note("NUL.txt opened=%d gle=%lu type=%lu", h != INVALID_HANDLE_VALUE, GetLastError(), h != INVALID_HANDLE_VALUE ? GetFileType(h) : 0);
    CLOSE(h);
    h = cfa("nul:", GENERIC_WRITE, 0, OPEN_ALWAYS, 0);
    t_note("nul: opened=%d gle=%lu", h != INVALID_HANDLE_VALUE, GetLastError());
    CLOSE(h);
    char p[MAX_PATH * 2]; snprintf(p, sizeof p, "%sNUL", t_tmpdir());
    h = cfa(p, GENERIC_WRITE, 0, OPEN_ALWAYS, 0);
    t_note("<tmp>\\NUL opened=%d gle=%lu type=%lu", h != INVALID_HANDLE_VALUE, GetLastError(), h != INVALID_HANDLE_VALUE ? GetFileType(h) : 0);
    CLOSE(h);
    if (exists(p)) rmfile(p);
}
TEST(fio_device_prefix_nul_literal) {
    REQUIRE(U(CreateFileA));
    /* "\\?\C:\...\NUL" is a literal file name, not the device. */
    char p[MAX_PATH * 2]; snprintf(p, sizeof p, "\\\\?\\%sNUL", t_tmpdir());
    HANDLE h = cfa(p, GENERIC_WRITE, 0, CREATE_NEW, 0);
    if (h == INVALID_HANDLE_VALUE) { t_note("literal NUL create gle=%lu", GetLastError()); return; }
    EXPECT_EQ(GetFileType(h), FILE_TYPE_DISK); CLOSE(h);
    DeleteFileA(p);
}
TEST(fio_device_con_aux_com_prn_probe) {
    REQUIRE(U(CreateFileA));
    static const char *const n[] = {"AUX", "COM1", "COM2", "LPT1", "PRN", "CLOCK$", "COM0", "COM10", "\\\\.\\COM1", "\\\\.\\COM256", "\\\\.\\CONOUT$", "\\\\.\\CONIN$"};
    for (size_t i = 0; i < sizeof n / sizeof *n; i++) {
        HANDLE h = cfa(n[i], 0, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, 0);
        DWORD e = GetLastError();
        t_note("open %s with zero access: %s gle=%lu", n[i], h != INVALID_HANDLE_VALUE ? "ok" : "fail", h != INVALID_HANDLE_VALUE ? 0 : e);
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }
}
TEST(fio_device_volume_zero_access) {
    REQUIRE(U(CreateFileA));
    char sys[MAX_PATH]; GetSystemDirectoryA(sys, sizeof sys);
    char v[8]; snprintf(v, sizeof v, "\\\\.\\%c:", sys[0]);
    HANDLE h = cfa(v, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, 0);
    if (h == INVALID_HANDLE_VALUE) { t_note("volume open zero access gle=%lu", GetLastError()); return; }
    DWORD ft = GetFileType(h);
    t_note("volume handle GetFileType=%lu", ft);
    CHECK(ft == FILE_TYPE_DISK);
    DWORD n = 0; char b[8];
    SET_ERR(0);
    BOOL ok = U(ReadFile)(h, b, 8, &n, NULL);
    CHECK(!ok); EXPECT_EQ(GetLastError(), ERROR_ACCESS_DENIED);
    CloseHandle(h);
}
TEST(fio_device_physicaldrive_zero_access) {
    REQUIRE(U(CreateFileA));
    HANDLE h = cfa("\\\\.\\PhysicalDrive0", 0, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, 0);
    t_note("PhysicalDrive0 zero access: %s gle=%lu", h != INVALID_HANDLE_VALUE ? "ok" : "fail", GetLastError());
    if (h != INVALID_HANDLE_VALUE) { EXPECT_EQ(GetFileType(h), FILE_TYPE_DISK); CloseHandle(h); }
    HANDLE h2 = cfa("\\\\.\\PhysicalDrive9999", 0, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, 0);
    CHECK(h2 == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    CLOSE(h2);
}
TEST(fio_device_nonexistent_device_path) {
    REQUIRE(U(CreateFileA));
    EXPECT_FAIL_H(cfa("\\\\.\\k32t_no_such_device", GENERIC_READ, 0, OPEN_EXISTING, 0), ERROR_FILE_NOT_FOUND);
    EXPECT_FAIL_H(cfa("\\\\.\\pipe\\k32t_no_such_pipe_zzz", GENERIC_READ, 0, OPEN_EXISTING, 0), ERROR_FILE_NOT_FOUND);
    EXPECT_FAIL_H(cfa("\\\\.\\mailslot\\k32t_no_such_slot_zzz", GENERIC_WRITE, FILE_SHARE_READ, OPEN_EXISTING, 0), ERROR_FILE_NOT_FOUND);
}
