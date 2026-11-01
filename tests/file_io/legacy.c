#include "fio.h"

#ifndef HFILE_ERROR
#define HFILE_ERROR ((HFILE)-1)
#endif
#define H2(x) ((HANDLE)(LONG_PTR)(x))

static void ofs_fill(OFSTRUCT *o) { memset(o, 0xA5, sizeof *o); }

TEST(fio_ofstruct_layout) {
    EXPECT_EQ(sizeof(OFSTRUCT), 136);
    OFSTRUCT o; EXPECT_EQ((char *)&o.nErrCode - (char *)&o, 2);
    EXPECT_EQ((char *)&o.szPathName - (char *)&o, 8);
    EXPECT_EQ(sizeof o.szPathName, 128);
}
/* ---------- _lopen / _lcreat / _lclose / _lread / _lwrite / _llseek ---------- */
TEST(fio_lcreat_creates_and_truncates) {
    REQUIRE(U(_lcreat)); REQUIRE(U(_lclose));
    TP(p, "l_creat.bin"); rmfile(p);
    SET_ERR(0x77);
    HFILE h = U(_lcreat)(p, 0);
    DWORD e = GetLastError();
    CHECK(h != HFILE_ERROR); EXPECT_EQ(e, 0);
    CHECK(exists(p));
    EXPECT_EQ(U(_lclose)(h), 0);
    mkfile_pat(p, 100);
    SET_ERR(0);
    h = U(_lcreat)(p, 0); e = GetLastError();
    CHECK(h != HFILE_ERROR); EXPECT_EQ(e, ERROR_ALREADY_EXISTS);
    EXPECT_EQ(fsize_of(p), 0);
    U(_lclose)(h); rmfile(p);
}
TEST(fio_lcreat_attributes) {
    REQUIRE(U(_lcreat)); REQUIRE(U(_lclose));
    static const struct { int attr; DWORD want; } t[] = {{1, FILE_ATTRIBUTE_READONLY}, {2, FILE_ATTRIBUTE_HIDDEN}, {4, FILE_ATTRIBUTE_SYSTEM}, {3, FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN}};
    for (size_t i = 0; i < sizeof t / sizeof *t; i++) {
        TP(p, "l_creat_attr.bin"); rmfile(p);
        HFILE h = U(_lcreat)(p, t[i].attr);
        CHECKF(h != HFILE_ERROR, "attr %d gle=%lu", t[i].attr, GetLastError());
        if (h != HFILE_ERROR) {
            U(_lclose)(h);
            DWORD a = GetFileAttributesA(p);
            CHECKF((a & t[i].want) == t[i].want, "attr %d -> 0x%lx", t[i].attr, a);
        }
        rmfile(p);
    }
}
TEST(fio_lcreat_readonly_existing_fails) {
    REQUIRE(U(_lcreat));
    TP(p, "l_creat_ro.bin"); mkfile_pat(p, 5); SetFileAttributesA(p, FILE_ATTRIBUTE_READONLY);
    SET_ERR(0);
    HFILE h = U(_lcreat)(p, 0);
    DWORD e = GetLastError();
    CHECK(h == HFILE_ERROR); EXPECT_EQ(e, ERROR_ACCESS_DENIED);
    EXPECT_EQ(fsize_of(p), 5);
    rmfile(p);
}
TEST(fio_lcreat_bad_paths) {
    REQUIRE(U(_lcreat));
    SET_ERR(0); HFILE h = U(_lcreat)("", 0); DWORD e = GetLastError();
    CHECK(h == HFILE_ERROR); EXPECT_EQ(e, ERROR_PATH_NOT_FOUND);
    TP(p, "no_such_dir_l\\f.bin");
    SET_ERR(0); h = U(_lcreat)(p, 0); e = GetLastError();
    CHECK(h == HFILE_ERROR); EXPECT_EQ(e, ERROR_PATH_NOT_FOUND);
    SET_ERR(0); h = U(_lcreat)(NULL, 0); e = GetLastError();
    CHECK(h == HFILE_ERROR); EXPECT_EQ(e, ERROR_PATH_NOT_FOUND);
}
TEST(fio_lopen_modes) {
    REQUIRE(U(_lopen)); REQUIRE(U(_lclose)); REQUIRE(U(_lread)); REQUIRE(U(_lwrite));
    TP(p, "l_open.bin"); mkfile(p, "0123456789", 10);
    char b[16] = {0};
    HFILE h = U(_lopen)(p, OF_READ);
    CHECK(h != HFILE_ERROR);
    EXPECT_EQ(U(_lread)(h, b, 4), 4); CHECK(!memcmp(b, "0123", 4));
    SET_ERR(0);
    EXPECT_EQ(U(_lwrite)(h, "x", 1), (UINT)HFILE_ERROR); EXPECT_ERR(ERROR_ACCESS_DENIED);
    U(_lclose)(h);
    h = U(_lopen)(p, OF_WRITE);
    CHECK(h != HFILE_ERROR);
    SET_ERR(0);
    EXPECT_EQ(U(_lread)(h, b, 4), (UINT)HFILE_ERROR); EXPECT_ERR(ERROR_ACCESS_DENIED);
    EXPECT_EQ(U(_lwrite)(h, "AB", 2), 2);
    U(_lclose)(h);
    h = U(_lopen)(p, OF_READWRITE);
    CHECK(h != HFILE_ERROR);
    EXPECT_EQ(U(_lread)(h, b, 4), 4); CHECK(!memcmp(b, "AB23", 4));
    EXPECT_EQ(U(_lwrite)(h, "ZZ", 2), 2);
    U(_lclose)(h);
    h = U(_lopen)(p, 3);                      /* bit 1 set: read/write */
    CHECK(h != HFILE_ERROR); if (h != HFILE_ERROR) { SET_ERR(0); EXPECT_EQ(U(_lwrite)(h, "q", 1), 1); U(_lclose)(h); }
    rmfile(p);
}
TEST(fio_lopen_missing) {
    REQUIRE(U(_lopen));
    TP(p, "l_missing.bin"); rmfile(p);
    SET_ERR(0); HFILE h = U(_lopen)(p, OF_READ); DWORD e = GetLastError();
    CHECK(h == HFILE_ERROR); EXPECT_EQ(e, ERROR_FILE_NOT_FOUND);
    TP(q, "no_such_dir_l2\\f.bin");
    SET_ERR(0); h = U(_lopen)(q, OF_READWRITE); e = GetLastError();
    CHECK(h == HFILE_ERROR); EXPECT_EQ(e, ERROR_PATH_NOT_FOUND);
}
TEST(fio_lopen_share_modes) {
    REQUIRE(U(_lopen)); REQUIRE(U(_lclose));
    TP(p, "l_share.bin"); mkfile_pat(p, 10);
    /* exclusive */
    HFILE a = U(_lopen)(p, OF_READ | OF_SHARE_EXCLUSIVE); CHECK(a != HFILE_ERROR);
    SET_ERR(0); HFILE b = U(_lopen)(p, OF_READ | OF_SHARE_DENY_NONE); DWORD e = GetLastError();
    CHECK(b == HFILE_ERROR); EXPECT_EQ(e, ERROR_SHARING_VIOLATION);
    U(_lclose)(a);
    /* deny write: readers fine, writers refused */
    a = U(_lopen)(p, OF_READ | OF_SHARE_DENY_WRITE); CHECK(a != HFILE_ERROR);
    b = U(_lopen)(p, OF_READ | OF_SHARE_DENY_NONE); CHECK(b != HFILE_ERROR); if (b != HFILE_ERROR) U(_lclose)(b);
    SET_ERR(0); b = U(_lopen)(p, OF_WRITE | OF_SHARE_DENY_NONE); e = GetLastError();
    CHECK(b == HFILE_ERROR); EXPECT_EQ(e, ERROR_SHARING_VIOLATION);
    U(_lclose)(a);
    /* deny read */
    a = U(_lopen)(p, OF_WRITE | OF_SHARE_DENY_READ); CHECK(a != HFILE_ERROR);
    SET_ERR(0); b = U(_lopen)(p, OF_READ | OF_SHARE_DENY_NONE); e = GetLastError();
    CHECK(b == HFILE_ERROR); EXPECT_EQ(e, ERROR_SHARING_VIOLATION);
    b = U(_lopen)(p, OF_WRITE | OF_SHARE_DENY_NONE); CHECK(b != HFILE_ERROR); if (b != HFILE_ERROR) U(_lclose)(b);
    U(_lclose)(a);
    /* compatibility and deny-none permit read/write sharing */
    a = U(_lopen)(p, OF_READWRITE | OF_SHARE_COMPAT); CHECK(a != HFILE_ERROR);
    b = U(_lopen)(p, OF_READWRITE | OF_SHARE_DENY_NONE); CHECK(b != HFILE_ERROR); if (b != HFILE_ERROR) U(_lclose)(b);
    U(_lclose)(a);
    rmfile(p);
}
TEST(fio_lopen_returns_real_handle) {
    REQUIRE(U(_lopen)); REQUIRE(U(_lclose));
    TP(p, "l_real.bin"); mkfile(p, "abcdef", 6);
    HFILE h = U(_lopen)(p, OF_READ); CHECK(h != HFILE_ERROR);
    char b[8] = {0}; DWORD n = 0;
    CHECK(ReadFile(H2(h), b, 6, &n, NULL)); EXPECT_EQ(n, 6);
    EXPECT_EQ(GetFileType(H2(h)), FILE_TYPE_DISK);
    EXPECT_EQ(U(_lclose)(h), 0);
    CHECK(!CloseHandle(H2(h)));
    rmfile(p);
}
TEST(fio_lclose) {
    REQUIRE(U(_lopen)); REQUIRE(U(_lclose));
    TP(p, "l_close.bin"); mkfile_pat(p, 3);
    HFILE h = U(_lopen)(p, OF_READ); CHECK(h != HFILE_ERROR);
    SET_ERR(0);
    EXPECT_EQ(U(_lclose)(h), 0);
    SET_ERR(0);
    EXPECT_EQ(U(_lclose)(h), HFILE_ERROR); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    /* _lclose is CloseHandle((HANDLE)(LONG_PTR)h): -1 is the current-process pseudo handle, closing it succeeds */
    EXPECT_EQ(U(_lclose)((HFILE)-1), 0);
    SET_ERR(0);
    EXPECT_EQ(U(_lclose)(0), HFILE_ERROR); EXPECT_ERR(ERROR_INVALID_HANDLE);
    rmfile(p);
}
TEST(fio_lread_semantics) {
    REQUIRE(U(_lopen)); REQUIRE(U(_lread)); REQUIRE(U(_lclose)); REQUIRE(U(_hread));
    TP(p, "l_read.bin"); mkfile(p, "0123456789", 10);
    HFILE h = U(_lopen)(p, OF_READ); CHECK(h != HFILE_ERROR);
    char b[32];
    EXPECT_EQ(U(_lread)(h, b, 0), 0);
    EXPECT_EQ(U(_lread)(h, b, 3), 3);
    EXPECT_EQ(U(_hread)(h, b, 3), 3); CHECK(!memcmp(b, "345", 3));
    EXPECT_EQ(U(_lread)(h, b, 32), 4);           /* short read at end */
    EXPECT_EQ(U(_lread)(h, b, 32), 0);           /* EOF is not an error */
    EXPECT_EQ(U(_hread)(h, b, 32), 0);
    SET_ERR(0);
    EXPECT_EQ(U(_lread)((HFILE)-1, b, 4), (UINT)HFILE_ERROR); EXPECT_ERR(ERROR_INVALID_HANDLE);
    U(_lclose)(h);
    SET_ERR(0);
    EXPECT_EQ(U(_lread)(h, b, 4), (UINT)HFILE_ERROR); EXPECT_ERR(ERROR_INVALID_HANDLE);
    rmfile(p);
}
TEST(fio_lwrite_semantics) {
    REQUIRE(U(_lcreat)); REQUIRE(U(_lwrite)); REQUIRE(U(_hwrite)); REQUIRE(U(_lclose)); REQUIRE(U(_llseek));
    TP(p, "l_write.bin"); rmfile(p);
    HFILE h = U(_lcreat)(p, 0); CHECK(h != HFILE_ERROR);
    EXPECT_EQ(U(_lwrite)(h, "0123456789", 10), 10);
    EXPECT_EQ(U(_hwrite)(h, "ab", 2), 2);
    EXPECT_EQ(fsize_of(p), 12);
    /* zero-length write truncates at the current position */
    EXPECT_EQ(U(_llseek)(h, 5, 0), 5);
    EXPECT_EQ(U(_lwrite)(h, "", 0), 0);
    EXPECT_EQ(fsize_of(p), 5);
    EXPECT_EQ(U(_hwrite)(h, NULL, 0), 0);
    EXPECT_EQ(U(_llseek)(h, 100, 0), 100);
    EXPECT_EQ(U(_lwrite)(h, "", 0), 0);                    /* zero length at 100 extends */
    EXPECT_EQ(fsize_of(p), 100);
    SET_ERR(0);
    EXPECT_EQ(U(_lwrite)((HFILE)-1, "x", 1), (UINT)HFILE_ERROR); EXPECT_ERR(ERROR_INVALID_HANDLE);
    U(_lclose)(h);
    SET_ERR(0);
    EXPECT_EQ(U(_lwrite)(h, "x", 1), (UINT)HFILE_ERROR); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    EXPECT_EQ(U(_lwrite)(h, "", 0), (UINT)HFILE_ERROR); EXPECT_ERR(ERROR_INVALID_HANDLE);
    rmfile(p);
}
TEST(fio_llseek_origins) {
    REQUIRE(U(_lopen)); REQUIRE(U(_llseek)); REQUIRE(U(_lclose));
    TP(p, "l_seek.bin"); mkfile_pat(p, 100);
    HFILE h = U(_lopen)(p, OF_READ); CHECK(h != HFILE_ERROR);
    EXPECT_EQ(U(_llseek)(h, 10, 0), 10);
    EXPECT_EQ(U(_llseek)(h, 5, 1), 15);
    EXPECT_EQ(U(_llseek)(h, -5, 1), 10);
    EXPECT_EQ(U(_llseek)(h, 0, 2), 100);
    EXPECT_EQ(U(_llseek)(h, -10, 2), 90);
    EXPECT_EQ(U(_llseek)(h, 50, 2), 150);
    EXPECT_EQ(U(_llseek)(h, 0, 0), 0);
    SET_ERR(0);
    EXPECT_EQ(U(_llseek)(h, -1, 0), (LONG)-1); EXPECT_ERR(ERROR_NEGATIVE_SEEK);
    EXPECT_EQ(U(_llseek)(h, 0, 1), 0);
    static const int bad[] = {3, 4, -1, 100};
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        EXPECT_EQ(U(_llseek)(h, 5, bad[i]), (LONG)-1);
    }
    EXPECT_EQ(U(_llseek)(h, 0, 1), 0);                        /* position unchanged */
    U(_lclose)(h);
    SET_ERR(0);
    EXPECT_EQ(U(_llseek)(h, 0, 0), (LONG)-1); EXPECT_ERR(ERROR_INVALID_HANDLE);
    rmfile(p);
}
TEST(fio_l_functions_roundtrip) {
    REQUIRE(U(_lcreat)); REQUIRE(U(_lopen)); REQUIRE(U(_lwrite)); REQUIRE(U(_lread)); REQUIRE(U(_llseek)); REQUIRE(U(_lclose));
    TP(p, "l_rt.bin"); rmfile(p);
    HFILE h = U(_lcreat)(p, 0); CHECK(h != HFILE_ERROR);
    char big[4096]; for (int i = 0; i < 4096; i++) big[i] = (char)(i ^ (i >> 8));
    EXPECT_EQ(U(_lwrite)(h, big, 4096), 4096);
    EXPECT_EQ(U(_llseek)(h, 0, 0), 0);
    char rd[4096]; memset(rd, 0, sizeof rd);
    EXPECT_EQ(U(_lread)(h, rd, 4096), 4096);
    CHECK(!memcmp(big, rd, 4096));
    U(_lclose)(h);
    EXPECT_EQ(fsize_of(p), 4096);
    rmfile(p);
}

/* ---------- OpenFile ---------- */
TEST(fio_openfile_parse) {
    REQUIRE(U(OpenFile)); if (strlen(t_tmpdir()) > 90) { t_skip("temp path too long for OFSTRUCT"); return; }
    TP(p, "of_parse.bin"); rmfile(p);
    OFSTRUCT o; ofs_fill(&o);
    SET_ERR(0x55);
    HFILE r = U(OpenFile)(p, &o, OF_PARSE);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(o.cBytes, sizeof(OFSTRUCT));
    EXPECT_EQ(o.fFixedDisk, 1);
    EXPECT_EQ(o.nErrCode, 0);
    EXPECT_EQ(o.Reserved1, 0); EXPECT_EQ(o.Reserved2, 0);
    char full[MAX_PATH]; GetFullPathNameA(p, sizeof full, full, NULL);
    if (strlen(full) < 128) EXPECT_STR(o.szPathName, full); else t_note("tmp path too long for OFSTRUCT");
    CHECK(!exists(p));                                   /* parse does not touch the file system */
    /* path normalisation */
    char q[MAX_PATH * 2]; snprintf(q, sizeof q, "%ssub\\..\\.\\of_parse.bin", t_tmpdir());
    ofs_fill(&o);
    r = U(OpenFile)(q, &o, OF_PARSE);
    EXPECT_EQ(r, 0);
    if (strlen(full) < 128) EXPECT_STR(o.szPathName, full);
    strcpy(q, p); for (char *c = q; *c; c++) if (*c == '\\') *c = '/';
    ofs_fill(&o);
    r = U(OpenFile)(q, &o, OF_PARSE);
    EXPECT_EQ(r, 0);
    if (strlen(full) < 128) EXPECT_STR(o.szPathName, full);
}
TEST(fio_openfile_parse_too_long) {
    REQUIRE(U(OpenFile)); if (strlen(t_tmpdir()) > 90) { t_skip("temp path too long for OFSTRUCT"); return; }
    char q[MAX_PATH * 2]; snprintf(q, sizeof q, "%s", t_tmpdir());
    size_t n = strlen(q); memset(q + n, 'x', 140); q[n + 140] = 0;
    OFSTRUCT o; ofs_fill(&o);
    SET_ERR(0);
    HFILE r = U(OpenFile)(q, &o, OF_PARSE);
    DWORD e = GetLastError();
    EXPECT_EQ(r, HFILE_ERROR); EXPECT_EQ(e, ERROR_INVALID_DATA);
    EXPECT_EQ(o.nErrCode, ERROR_INVALID_DATA);
}
TEST(fio_openfile_read_existing) {
    REQUIRE(U(OpenFile)); if (strlen(t_tmpdir()) > 90) { t_skip("temp path too long for OFSTRUCT"); return; } REQUIRE(U(_lclose));
    TP(p, "of_read.bin"); mkfile(p, "0123456789", 10);
    OFSTRUCT o; ofs_fill(&o);
    SET_ERR(0x55);
    HFILE h = U(OpenFile)(p, &o, OF_READ);
    CHECK(h != HFILE_ERROR);
    EXPECT_EQ(o.cBytes, sizeof(OFSTRUCT));
    EXPECT_EQ(o.nErrCode, 0);
    char full[MAX_PATH]; GetFullPathNameA(p, sizeof full, full, NULL);
    if (strlen(full) < 128) EXPECT_STR(o.szPathName, full);
    /* DOS date/time of last write */
    HANDLE hh = H2(h); FILETIME wt; WORD d = 0, t = 0;
    CHECK(GetFileTime(hh, NULL, NULL, &wt)); CHECK(FileTimeToDosDateTime(&wt, &d, &t));
    EXPECT_EQ(o.Reserved1, d); EXPECT_EQ(o.Reserved2, t);
    char b[16] = {0}; DWORD n; CHECK(ReadFile(hh, b, 10, &n, NULL)); EXPECT_EQ(n, 10);
    DWORD m = fio_access_mask(hh); CHECK(!(m & FILE_WRITE_DATA));
    U(_lclose)(h); rmfile(p);
}
TEST(fio_openfile_access_modes) {
    REQUIRE(U(OpenFile)); if (strlen(t_tmpdir()) > 90) { t_skip("temp path too long for OFSTRUCT"); return; } REQUIRE(U(_lclose));
    TP(p, "of_acc.bin"); mkfile(p, "0123456789", 10);
    OFSTRUCT o; DWORD n; char b[4];
    HFILE h = U(OpenFile)(p, &o, OF_WRITE); CHECK(h != HFILE_ERROR);
    if (h != HFILE_ERROR) {
        CHECK(!ReadFile(H2(h), b, 4, &n, NULL)); EXPECT_ERR(ERROR_ACCESS_DENIED);
        CHECK(WriteFile(H2(h), "W", 1, &n, NULL)); U(_lclose)(h);
    }
    h = U(OpenFile)(p, &o, OF_READWRITE); CHECK(h != HFILE_ERROR);
    if (h != HFILE_ERROR) {
        CHECK(ReadFile(H2(h), b, 4, &n, NULL)); CHECK(!memcmp(b, "W123", 4));
        CHECK(WriteFile(H2(h), "RW", 2, &n, NULL)); U(_lclose)(h);
    }
    h = U(OpenFile)(p, &o, OF_READ); CHECK(h != HFILE_ERROR);
    if (h != HFILE_ERROR) {
        CHECK(!WriteFile(H2(h), "x", 1, &n, NULL)); EXPECT_ERR(ERROR_ACCESS_DENIED); U(_lclose)(h);
    }
    rmfile(p);
}
TEST(fio_openfile_missing) {
    REQUIRE(U(OpenFile)); if (strlen(t_tmpdir()) > 90) { t_skip("temp path too long for OFSTRUCT"); return; }
    TP(p, "of_missing.bin"); rmfile(p);
    OFSTRUCT o; ofs_fill(&o);
    SET_ERR(0);
    HFILE h = U(OpenFile)(p, &o, OF_READ);
    DWORD e = GetLastError();
    EXPECT_EQ(h, HFILE_ERROR); EXPECT_EQ(e, ERROR_FILE_NOT_FOUND);
    EXPECT_EQ(o.nErrCode, ERROR_FILE_NOT_FOUND);
    char full[MAX_PATH]; GetFullPathNameA(p, sizeof full, full, NULL);
    if (strlen(full) < 128) EXPECT_STR(o.szPathName, full);       /* filled with the full path even on failure */
    TP(q, "no_such_dir_o\\f.bin");
    ofs_fill(&o); SET_ERR(0);
    h = U(OpenFile)(q, &o, OF_READ); e = GetLastError();
    EXPECT_EQ(h, HFILE_ERROR); EXPECT_EQ(e, ERROR_PATH_NOT_FOUND); EXPECT_EQ(o.nErrCode, ERROR_PATH_NOT_FOUND);
}
TEST(fio_openfile_create) {
    REQUIRE(U(OpenFile)); if (strlen(t_tmpdir()) > 90) { t_skip("temp path too long for OFSTRUCT"); return; } REQUIRE(U(_lclose));
    TP(p, "of_create.bin"); rmfile(p);
    OFSTRUCT o; ofs_fill(&o);
    HFILE h = U(OpenFile)(p, &o, OF_CREATE | OF_READWRITE);
    CHECK(h != HFILE_ERROR); EXPECT_EQ(o.nErrCode, 0); CHECK(exists(p));
    DWORD n; CHECK(WriteFile(H2(h), "12345", 5, &n, NULL));
    U(_lclose)(h);
    EXPECT_EQ(fsize_of(p), 5);
    ofs_fill(&o);
    h = U(OpenFile)(p, &o, OF_CREATE | OF_READWRITE);        /* truncates */
    CHECK(h != HFILE_ERROR);
    t_note("OF_CREATE over existing file: nErrCode=%u", o.nErrCode);
    CHECK(o.nErrCode == 0 || o.nErrCode == ERROR_ALREADY_EXISTS);
    EXPECT_EQ(fsize_of(p), 0);
    U(_lclose)(h);
    rmfile(p);
}
TEST(fio_openfile_exist) {
    REQUIRE(U(OpenFile)); if (strlen(t_tmpdir()) > 90) { t_skip("temp path too long for OFSTRUCT"); return; }
    TP(p, "of_exist.bin"); mkfile_pat(p, 3);
    OFSTRUCT o; ofs_fill(&o);
    SET_ERR(0);
    HFILE h = U(OpenFile)(p, &o, OF_EXIST);
    CHECK(h != HFILE_ERROR);
    t_note("OF_EXIST returned %d", (int)h);
    EXPECT_EQ(o.cBytes, sizeof(OFSTRUCT));
    /* the probe leaves no handle open: exclusive open still works */
    HANDLE x = cfa(p, GENERIC_READ | GENERIC_WRITE, 0, OPEN_EXISTING, 0); OK_H(x); CLOSE(x);
    rmfile(p);
    ofs_fill(&o); SET_ERR(0);
    h = U(OpenFile)(p, &o, OF_EXIST);
    EXPECT_EQ(h, HFILE_ERROR); EXPECT_ERR(ERROR_FILE_NOT_FOUND); EXPECT_EQ(o.nErrCode, ERROR_FILE_NOT_FOUND);
}
TEST(fio_openfile_exist_directory) {
    REQUIRE(U(OpenFile)); if (strlen(t_tmpdir()) > 90) { t_skip("temp path too long for OFSTRUCT"); return; }
    TP(d, "of_dir"); RemoveDirectoryA(d); CreateDirectoryA(d, NULL);
    OFSTRUCT o; ofs_fill(&o); SET_ERR(0);
    HFILE h = U(OpenFile)(d, &o, OF_EXIST);
    EXPECT_EQ(h, HFILE_ERROR); EXPECT_ERR(ERROR_ACCESS_DENIED); EXPECT_EQ(o.nErrCode, ERROR_ACCESS_DENIED);
    ofs_fill(&o); SET_ERR(0);
    h = U(OpenFile)(d, &o, OF_READ);
    EXPECT_EQ(h, HFILE_ERROR); EXPECT_ERR(ERROR_ACCESS_DENIED);
    RemoveDirectoryA(d);
}
TEST(fio_openfile_delete) {
    REQUIRE(U(OpenFile)); if (strlen(t_tmpdir()) > 90) { t_skip("temp path too long for OFSTRUCT"); return; }
    TP(p, "of_del.bin"); mkfile_pat(p, 3);
    OFSTRUCT o; ofs_fill(&o);
    SET_ERR(0);
    HFILE h = U(OpenFile)(p, &o, OF_DELETE);
    CHECK(h != HFILE_ERROR);
    EXPECT_EQ(o.nErrCode, 0); EXPECT_EQ(o.cBytes, sizeof(OFSTRUCT));
    CHECK(!exists(p));
    ofs_fill(&o);
    h = U(OpenFile)(p, &o, OF_DELETE);
    EXPECT_EQ(h, HFILE_ERROR); EXPECT_EQ(o.nErrCode, ERROR_FILE_NOT_FOUND);
}
TEST(fio_openfile_delete_in_use) {
    REQUIRE(U(OpenFile)); if (strlen(t_tmpdir()) > 90) { t_skip("temp path too long for OFSTRUCT"); return; }
    TP(p, "of_del_use.bin"); mkfile_pat(p, 3);
    HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, 0); OK_H(h);
    OFSTRUCT o; ofs_fill(&o);
    HFILE r = U(OpenFile)(p, &o, OF_DELETE);
    EXPECT_EQ(r, HFILE_ERROR);
    /* OpenFile ends with nErrCode = GetLastError(), which overwrites the preset 2: DeleteFileA's sharing violation. */
    EXPECT_EQ(o.nErrCode, ERROR_SHARING_VIOLATION);
    CHECK(exists(p));
    CLOSE(h); rmfile(p);
}
TEST(fio_openfile_share_exclusive) {
    REQUIRE(U(OpenFile)); if (strlen(t_tmpdir()) > 90) { t_skip("temp path too long for OFSTRUCT"); return; } REQUIRE(U(_lclose));
    TP(p, "of_share.bin"); mkfile_pat(p, 3);
    OFSTRUCT o, o2;
    HFILE a = U(OpenFile)(p, &o, OF_READ | OF_SHARE_EXCLUSIVE); CHECK(a != HFILE_ERROR);
    SET_ERR(0);
    HFILE b = U(OpenFile)(p, &o2, OF_READ | OF_SHARE_DENY_NONE);
    EXPECT_EQ(b, HFILE_ERROR); EXPECT_ERR(ERROR_SHARING_VIOLATION); EXPECT_EQ(o2.nErrCode, ERROR_SHARING_VIOLATION);
    U(_lclose)(a);
    a = U(OpenFile)(p, &o, OF_READ | OF_SHARE_DENY_WRITE); CHECK(a != HFILE_ERROR);
    SET_ERR(0);
    b = U(OpenFile)(p, &o2, OF_READWRITE | OF_SHARE_DENY_NONE);
    EXPECT_EQ(b, HFILE_ERROR); EXPECT_EQ(o2.nErrCode, ERROR_SHARING_VIOLATION);
    b = U(OpenFile)(p, &o2, OF_READ | OF_SHARE_DENY_NONE); CHECK(b != HFILE_ERROR); if (b != HFILE_ERROR) U(_lclose)(b);
    U(_lclose)(a);
    rmfile(p);
}
TEST(fio_openfile_reopen) {
    REQUIRE(U(OpenFile)); if (strlen(t_tmpdir()) > 90) { t_skip("temp path too long for OFSTRUCT"); return; } REQUIRE(U(_lclose));
    TP(p, "of_reopen.bin"); mkfile(p, "abcd", 4);
    OFSTRUCT o; HFILE h = U(OpenFile)(p, &o, OF_READ); CHECK(h != HFILE_ERROR);
    if (h != HFILE_ERROR) U(_lclose)(h);
    OFSTRUCT o2 = o;
    HFILE r = U(OpenFile)("ignored-name-when-reopening", &o2, OF_REOPEN | OF_READ);
    CHECK(r != HFILE_ERROR);
    if (r != HFILE_ERROR) {
        char b[8] = {0}; DWORD n; CHECK(ReadFile(H2(r), b, 4, &n, NULL)); CHECK(!memcmp(b, "abcd", 4));
        EXPECT_EQ(o2.nErrCode, 0);
        EXPECT_STR(o2.szPathName, o.szPathName);
        U(_lclose)(r);
    }
    /* reopen of a file that went away */
    rmfile(p);
    o2 = o; SET_ERR(0);
    r = U(OpenFile)("x", &o2, OF_REOPEN | OF_READ);
    EXPECT_EQ(r, HFILE_ERROR); EXPECT_EQ(o2.nErrCode, ERROR_FILE_NOT_FOUND);
}
TEST(fio_openfile_verify) {
    REQUIRE(U(OpenFile)); if (strlen(t_tmpdir()) > 90) { t_skip("temp path too long for OFSTRUCT"); return; } REQUIRE(U(_lclose));
    TP(p, "of_verify.bin"); mkfile(p, "abcd", 4);
    OFSTRUCT o; HFILE h = U(OpenFile)(p, &o, OF_READ); CHECK(h != HFILE_ERROR);
    if (h != HFILE_ERROR) U(_lclose)(h);
    OFSTRUCT o2 = o;
    HFILE r = U(OpenFile)("x", &o2, OF_REOPEN | OF_VERIFY | OF_READ);       /* unchanged: success */
    CHECK(r != HFILE_ERROR); if (r != HFILE_ERROR) U(_lclose)(r);
    /* change the write time by a day: verification must fail and the caller's buffer is restored */
    HANDLE w = cfa(p, FILE_WRITE_ATTRIBUTES | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, OPEN_EXISTING, 0);
    OK_H(w);
    FILETIME ft; GetFileTime(w, NULL, NULL, &ft);
    ULARGE_INTEGER u; u.LowPart = ft.dwLowDateTime; u.HighPart = ft.dwHighDateTime; u.QuadPart += 864000000000ULL * 3;
    ft.dwLowDateTime = u.LowPart; ft.dwHighDateTime = u.HighPart;
    CHECK(SetFileTime(w, NULL, NULL, &ft)); CLOSE(w);
    o2 = o; SET_ERR(0);
    r = U(OpenFile)("x", &o2, OF_REOPEN | OF_VERIFY | OF_READ);
    EXPECT_EQ(r, HFILE_ERROR);
    CHECK(!memcmp(&o2, &o, sizeof o) || o2.nErrCode != 0);
    o2 = o;
    r = U(OpenFile)("x", &o2, OF_REOPEN | OF_READ);                         /* without VERIFY it opens */
    CHECK(r != HFILE_ERROR); if (r != HFILE_ERROR) U(_lclose)(r);
    rmfile(p);
}
TEST(fio_openfile_searches_path_for_relative_names) {
    REQUIRE(U(OpenFile)); if (strlen(t_tmpdir()) > 90) { t_skip("temp path too long for OFSTRUCT"); return; }
    OFSTRUCT o; ofs_fill(&o);
    HFILE h = U(OpenFile)("kernel32.dll", &o, OF_READ);
    CHECK(h != HFILE_ERROR);
    if (h != HFILE_ERROR) {
        char sys[MAX_PATH]; GetSystemDirectoryA(sys, sizeof sys);
        CHECK(strstr(o.szPathName, "\\") != NULL);
        CHECK(!_strnicmp(o.szPathName, sys, 3));
        CHECK(strlen(o.szPathName) < 128);
        t_note("OpenFile(kernel32.dll) -> %s", o.szPathName);
        U(_lclose)(h);
    }
    ofs_fill(&o);
    h = U(OpenFile)("k32t_definitely_not_here.dll", &o, OF_READ);
    EXPECT_EQ(h, HFILE_ERROR); EXPECT_EQ(o.nErrCode, ERROR_FILE_NOT_FOUND);
}
TEST(fio_openfile_name_variants) {
    REQUIRE(U(OpenFile)); if (strlen(t_tmpdir()) > 90) { t_skip("temp path too long for OFSTRUCT"); return; }
    OFSTRUCT o; ofs_fill(&o); SET_ERR(0);
    HFILE h = U(OpenFile)("", &o, OF_READ);
    EXPECT_EQ(h, HFILE_ERROR);
    t_note("OpenFile(\"\") nErrCode=%u gle=%lu", o.nErrCode, GetLastError());
    ofs_fill(&o); SET_ERR(0);
    h = U(OpenFile)("NUL", &o, OF_READWRITE);
    t_note("OpenFile(NUL): %d nErrCode=%u", (int)h, o.nErrCode);
    if (h != HFILE_ERROR) CloseHandle(H2(h));
}
TEST(fio_openfile_long_path_fails) {
    REQUIRE(U(OpenFile)); if (strlen(t_tmpdir()) > 90) { t_skip("temp path too long for OFSTRUCT"); return; }
    char q[MAX_PATH * 2]; snprintf(q, sizeof q, "%s", t_tmpdir());
    size_t n = strlen(q); memset(q + n, 'y', 130); q[n + 130] = 0;
    OFSTRUCT o; ofs_fill(&o); SET_ERR(0);
    HFILE h = U(OpenFile)(q, &o, OF_READ);
    DWORD e = GetLastError();
    EXPECT_EQ(h, HFILE_ERROR); EXPECT_EQ(e, ERROR_INVALID_DATA);
    ofs_fill(&o);
    h = U(OpenFile)(q, &o, OF_CREATE | OF_READWRITE);
    EXPECT_EQ(h, HFILE_ERROR);
    CHECK(!exists(q));
}
TEST(fio_openfile_fixed_disk_flag) {
    REQUIRE(U(OpenFile)); if (strlen(t_tmpdir()) > 90) { t_skip("temp path too long for OFSTRUCT"); return; } REQUIRE(U(_lclose));
    TP(p, "of_fixed.bin"); mkfile_pat(p, 3);
    OFSTRUCT o; HFILE h = U(OpenFile)(p, &o, OF_READ); CHECK(h != HFILE_ERROR);
    char root[4] = {p[0], ':', '\\', 0};
    UINT dt = GetDriveTypeA(root);
    t_note("drive type %u fFixedDisk=%u", dt, o.fFixedDisk);
    if (dt == DRIVE_FIXED) EXPECT_EQ(o.fFixedDisk, 1);
    if (dt == DRIVE_REMOVABLE) EXPECT_EQ(o.fFixedDisk, 0);
    if (h != HFILE_ERROR) U(_lclose)(h);
    rmfile(p);
}
