#include "fio.h"

#define SHARE_ALL (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE)

TEST(fio_createfile_invalid_share_bits) {
    REQUIRE(U(CreateFileA));
    TP(p, "badshare.bin"); mkfile_pat(p, 4);
    static const DWORD bad[] = {8, 0x10, 0x80, 0x100, 0x80000000, 0xFFFFFFF8, 0xFFFFFFFF};
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        SET_ERR(0);
        HANDLE h = cfa(p, GENERIC_READ, bad[i], OPEN_EXISTING, 0);
        DWORD e = GetLastError();
        CHECKF(h == INVALID_HANDLE_VALUE && e == ERROR_INVALID_PARAMETER, "share=0x%lx h=%p gle=%lu", bad[i], h, e);
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }
    rmfile(p);
}
TEST(fio_createfile_access_system_security_needs_privilege) {
    REQUIRE(U(CreateFileA));
    TP(p, "sacl.bin"); mkfile_pat(p, 4);
    SET_ERR(0);
    HANDLE h = cfa(p, ACCESS_SYSTEM_SECURITY, SHARE_ALL, OPEN_EXISTING, 0);
    DWORD e = GetLastError();
    CHECKF(h == INVALID_HANDLE_VALUE && e == ERROR_PRIVILEGE_NOT_HELD, "h=%p gle=%lu", h, e);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    rmfile(p);
}
TEST(fio_createfile_readonly_with_delete_on_close) {
    REQUIRE(U(CreateFileA));
    TP(p, "ro_doc.bin"); rmfile(p);
    SET_ERR(0);
    HANDLE h = cfa(p, GENERIC_WRITE, 0, CREATE_NEW, FILE_ATTRIBUTE_READONLY | FILE_FLAG_DELETE_ON_CLOSE);
    DWORD e = GetLastError();
    CHECK(h == INVALID_HANDLE_VALUE); EXPECT_EQ(e, ERROR_ACCESS_DENIED);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    t_note("file left behind after failed create: %d", exists(p));
    rmfile(p);
}
TEST(fio_createfile_handle_values_are_aligned_and_distinct) {
    REQUIRE(U(CreateFileA));
    TP(p, "hv.bin"); mkfile_pat(p, 4);
    HANDLE h[8];
    for (int i = 0; i < 8; i++) { h[i] = cfa(p, GENERIC_READ, SHARE_ALL, OPEN_EXISTING, 0); OK_H(h[i]); CHECK(((ULONG_PTR)h[i] & 3) == 0); CHECK(h[i] != NULL); }
    for (int i = 0; i < 8; i++) for (int j = i + 1; j < 8; j++) CHECK(h[i] != h[j]);
    for (int i = 0; i < 8; i++) CloseHandle(h[i]);
    rmfile(p);
}
TEST(fio_createfile_many_handles_and_reuse) {
    REQUIRE(U(CreateFileA));
    TP(p, "many.bin"); mkfile_pat(p, 4);
    static HANDLE h[600];
    int n = 0;
    for (; n < 600; n++) { h[n] = cfa(p, GENERIC_READ, SHARE_ALL, OPEN_EXISTING, 0); if (h[n] == INVALID_HANDLE_VALUE) break; }
    EXPECT_EQ(n, 600);
    for (int i = 0; i < n; i++) CloseHandle(h[i]);
    HANDLE a = cfa(p, GENERIC_READ, SHARE_ALL, OPEN_EXISTING, 0); OK_H(a); CLOSE(a);
    rmfile(p);
}
TEST(fio_createfile_leading_spaces_preserved) {
    REQUIRE(U(CreateFileA));
    TP(p, "  lead space.bin"); rmfile(p);
    HANDLE h = cfa(p, GENERIC_WRITE, 0, CREATE_NEW, 0);
    if (h == INVALID_HANDLE_VALUE) { t_note("leading-space name create gle=%lu", GetLastError()); return; }
    CLOSE(h);
    WIN32_FIND_DATAA fd; char pat[MAX_PATH * 2]; snprintf(pat, sizeof pat, "%s*lead space.bin", t_tmpdir());
    HANDLE f = FindFirstFileA(pat, &fd);
    CHECK(f != INVALID_HANDLE_VALUE);
    if (f != INVALID_HANDLE_VALUE) { EXPECT_STR(fd.cFileName, "  lead space.bin"); FindClose(f); }
    rmfile(p);
}
TEST(fio_createfile_short_name_equivalence) {
    REQUIRE(U(CreateFileA));
    TP(p, "a_rather_long_file_name_for_8dot3.bin"); mkfile_pat(p, 7);
    char sh[MAX_PATH * 2];
    DWORD n = GetShortPathNameA(p, sh, sizeof sh);
    if (!n || !strcmp(sh, p)) { t_note("8.3 names unavailable"); rmfile(p); return; }
    HANDLE h = cfa(sh, GENERIC_READ, SHARE_ALL, OPEN_EXISTING, 0); OK_H(h);
    if (h != INVALID_HANDLE_VALUE) EXPECT_EQ(hsize(h), 7);
    CLOSE(h); rmfile(p);
}
TEST(fio_createfile_hardlink_same_file) {
    REQUIRE(U(CreateFileA));
    TP(p, "hl_a.bin"); TP(q, "hl_b.bin"); rmfile(p); rmfile(q); mkfile(p, "data", 4);
    if (!CreateHardLinkA(q, p, NULL)) { t_note("no hard links gle=%lu", GetLastError()); rmfile(p); return; }
    HANDLE a = cfa(p, GENERIC_READ | GENERIC_WRITE, 0, OPEN_EXISTING, 0); OK_H(a);
    EXPECT_FAIL_H(cfa(q, GENERIC_READ, SHARE_ALL, OPEN_EXISTING, 0), ERROR_SHARING_VIOLATION);   /* same stream, so same share rules */
    CLOSE(a); rmfile(p); rmfile(q);
}
TEST(fio_createfile_new_file_visible_in_directory_listing) {
    REQUIRE(U(CreateFileA));
    TP(p, "listed.bin"); rmfile(p);
    HANDLE h = cfa(p, GENERIC_WRITE, 0, CREATE_NEW, 0); OK_H(h);
    WIN32_FIND_DATAA fd; HANDLE f = FindFirstFileA(p, &fd);
    CHECK(f != INVALID_HANDLE_VALUE);
    if (f != INVALID_HANDLE_VALUE) { EXPECT_STR(fd.cFileName, "listed.bin"); EXPECT_EQ(fd.nFileSizeLow, 0); FindClose(f); }
    DWORD n; WriteFile(h, "12345", 5, &n, NULL);
    f = FindFirstFileA(p, &fd);
    if (f != INVALID_HANDLE_VALUE) { t_note("size reported in directory entry while open: %lu", fd.nFileSizeLow); FindClose(f); }
    CLOSE(h);
    f = FindFirstFileA(p, &fd); CHECK(f != INVALID_HANDLE_VALUE);
    if (f != INVALID_HANDLE_VALUE) { EXPECT_EQ(fd.nFileSizeLow, 5); FindClose(f); }
    rmfile(p);
}

/* ---------- high-dword out-parameter handling on failure ---------- */
TEST(fio_setfilepointer_high_out_on_failure) {
    REQUIRE(U(SetFilePointer)); REQUIRE(U(GetFileSize));
    LONG hi = 5;
    SET_ERR(0);
    DWORD r = U(SetFilePointer)(INVALID_HANDLE_VALUE, 0, &hi, FILE_BEGIN);
    CHECK(r == INVALID_SET_FILE_POINTER); EXPECT_ERR(ERROR_INVALID_HANDLE);
    EXPECT_EQ(hi, -1);                  /* failed set of the position stores -1 in the high part */
    hi = 5; SET_ERR(0);
    r = U(SetFilePointer)(INVALID_HANDLE_VALUE, 0, &hi, FILE_CURRENT);
    CHECK(r == INVALID_SET_FILE_POINTER); EXPECT_ERR(ERROR_INVALID_HANDLE);
    EXPECT_EQ(hi, 5);                   /* query failure leaves it alone */
    DWORD dh = 0x1234; SET_ERR(0);
    r = U(GetFileSize)(INVALID_HANDLE_VALUE, &dh);
    CHECK(r == INVALID_FILE_SIZE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    EXPECT_EQ(dh, 0x1234);
}
TEST(fio_setfilepointer_high_in_for_file_end) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(SetFilePointer));
    TP(p, "sfp_hend.bin"); mkfile_pat(p, 100);
    HANDLE h = cfa(p, GENERIC_READ, SHARE_ALL, OPEN_EXISTING, 0); OK_H(h);
    LONG hi = 2;
    DWORD r = U(SetFilePointer)(h, 3, &hi, FILE_END);
    EXPECT_EQ(r, 103); EXPECT_EQ(hi, 2);
    EXPECT_EQ(fpos(h), 0x200000067LL);
    hi = -1;
    r = U(SetFilePointer)(h, -50, &hi, FILE_END);       /* high -1 with low -50: 64-bit -50 relative to EOF -> 50 */
    EXPECT_EQ(r, 50); EXPECT_EQ(hi, 0);
    CLOSE(h); rmfile(p);
}
TEST(fio_setfilepointer_relative_uses_64bit_current) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(SetFilePointer));
    TP(p, "sfp_rel64.bin"); mkfile_pat(p, 10);
    HANDLE h = cfa(p, GENERIC_READ, SHARE_ALL, OPEN_EXISTING, 0); OK_H(h);
    hsetpos(h, 0x300000000LL);
    LONG hi = 0;
    DWORD r = U(SetFilePointer)(h, 1, &hi, FILE_CURRENT);
    EXPECT_EQ(r, 1); EXPECT_EQ(hi, 3);
    hi = -1;
    r = U(SetFilePointer)(h, -1, &hi, FILE_CURRENT);   /* -1 in 64 bits */
    EXPECT_EQ(r, 0); EXPECT_EQ(hi, 3);
    CLOSE(h); rmfile(p);
}

/* ---------- threads ---------- */
typedef struct { HANDLE h; char c; int n; DWORD fail; } WT;
static DWORD WINAPI writer_thread(LPVOID a) {
    WT *t = (WT *)a; char rec[16]; memset(rec, t->c, 16);
    for (int i = 0; i < t->n; i++) { DWORD n = 0; if (!U(WriteFile)(t->h, rec, 16, &n, NULL) || n != 16) t->fail++; }
    return 0;
}
TEST(fio_concurrent_writes_on_one_sync_handle_do_not_overlap) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(WriteFile));
    TP(p, "conc_w.bin"); rmfile(p);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, SHARE_ALL, CREATE_NEW, 0); OK_H(h);
    WT t[4]; HANDLE th[4];
    for (int i = 0; i < 4; i++) { t[i].h = h; t[i].c = (char)('a' + i); t[i].n = 1000; t[i].fail = 0; th[i] = CreateThread(NULL, 0, writer_thread, &t[i], 0, NULL); CHECK(th[i] != NULL); }
    for (int i = 0; i < 4; i++) { if (th[i]) { EXPECT_EQ(WaitForSingleObject(th[i], 30000), WAIT_OBJECT_0); CloseHandle(th[i]); } }
    EXPECT_EQ(hsize(h), 64000);
    EXPECT_EQ(fpos(h), 64000);
    char *b = (char *)malloc(64000); DWORD n = 0;
    hsetpos(h, 0); CHECK(ReadFile(h, b, 64000, &n, NULL)); EXPECT_EQ(n, 64000);
    int bad = 0, cnt[4] = {0};
    for (int r = 0; r < 4000; r++) {
        char c = b[r * 16]; for (int k = 1; k < 16; k++) if (b[r * 16 + k] != c) bad++;
        if (c >= 'a' && c <= 'd') cnt[c - 'a']++; else bad++;
    }
    CHECK(bad == 0);
    for (int i = 0; i < 4; i++) { EXPECT_EQ(cnt[i], 1000); EXPECT_EQ(t[i].fail, 0); }
    free(b); CLOSE(h); rmfile(p);
}
static DWORD WINAPI closer_thread(LPVOID a) { CloseHandle((HANDLE)a); return 0; }
TEST(fio_handle_closed_from_other_thread) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(ReadFile));
    TP(p, "xthread.bin"); mkfile_pat(p, 8);
    HANDLE h = cfa(p, GENERIC_READ, SHARE_ALL, OPEN_EXISTING, 0); OK_H(h);
    HANDLE t = CreateThread(NULL, 0, closer_thread, h, 0, NULL);
    WaitForSingleObject(t, 10000); CloseHandle(t);
    char b[4]; DWORD n = 77;
    EXPECT_FAIL_B(U(ReadFile)(h, b, 4, &n, NULL), ERROR_INVALID_HANDLE);
    /* the file can be opened exclusively again: the object was really released */
    HANDLE x = cfa(p, GENERIC_READ | GENERIC_WRITE, 0, OPEN_EXISTING, 0); OK_H(x); CLOSE(x);
    rmfile(p);
}
typedef struct { HANDLE h; char buf[32]; DWORD n; BOOL ok; DWORD err; } RT;
static DWORD WINAPI reader_thread(LPVOID a) {
    RT *t = (RT *)a;
    t->ok = U(ReadFile)(t->h, t->buf, sizeof t->buf, &t->n, NULL);
    t->err = GetLastError();
    return 0;
}
TEST(fio_pipe_read_blocks_until_data_in_thread) {
    REQUIRE(U(ReadFile)); REQUIRE(U(WriteFile));
    HANDLE r, w; CHECK(CreatePipe(&r, &w, NULL, 0));
    RT t; memset(&t, 0, sizeof t); t.h = r;
    HANDLE th = CreateThread(NULL, 0, reader_thread, &t, 0, NULL);
    EXPECT_EQ(WaitForSingleObject(th, 200), WAIT_TIMEOUT);
    DWORD n; CHECK(U(WriteFile)(w, "wake up", 7, &n, NULL));
    EXPECT_EQ(WaitForSingleObject(th, 10000), WAIT_OBJECT_0);
    CHECK(t.ok); EXPECT_EQ(t.n, 7);
    CloseHandle(th); CloseHandle(r); CloseHandle(w);
}
TEST(fio_pipe_blocked_read_released_by_writer_close) {
    REQUIRE(U(ReadFile));
    HANDLE r, w; CHECK(CreatePipe(&r, &w, NULL, 0));
    RT t; memset(&t, 0, sizeof t); t.h = r; t.ok = TRUE; t.n = 99;
    HANDLE th = CreateThread(NULL, 0, reader_thread, &t, 0, NULL);
    EXPECT_EQ(WaitForSingleObject(th, 200), WAIT_TIMEOUT);
    CloseHandle(w);
    EXPECT_EQ(WaitForSingleObject(th, 10000), WAIT_OBJECT_0);
    CHECK(!t.ok); EXPECT_EQ(t.err, ERROR_BROKEN_PIPE); EXPECT_EQ(t.n, 0);
    CloseHandle(th); CloseHandle(r);
}
