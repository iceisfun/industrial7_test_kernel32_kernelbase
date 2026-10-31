#include "fio.h"

/* ---------- ReadFile / WriteFile ---------- */
TEST(fio_write_read_roundtrip) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(WriteFile)); REQUIRE(U(ReadFile));
    TP(p, "rw_rt.bin"); rmfile(p);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, 0, CREATE_NEW, 0); OK_H(h);
    DWORD n = 55;
    SET_ERR(0);
    CHECK(U(WriteFile)(h, "hello world", 11, &n, NULL)); EXPECT_EQ(n, 11);
    EXPECT_EQ(fpos(h), 11);
    EXPECT_EQ(hsize(h), 11);
    hsetpos(h, 0);
    char b[32] = {0}; n = 55;
    CHECK(U(ReadFile)(h, b, 5, &n, NULL)); EXPECT_EQ(n, 5); CHECK(!memcmp(b, "hello", 5));
    EXPECT_EQ(fpos(h), 5);
    CHECK(U(ReadFile)(h, b, 32, &n, NULL)); EXPECT_EQ(n, 6); CHECK(!memcmp(b, " world", 6));
    EXPECT_EQ(fpos(h), 11);
    CLOSE(h); rmfile(p);
}
TEST(fio_read_at_eof_succeeds_with_zero) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(ReadFile));
    TP(p, "rd_eof.bin"); mkfile_pat(p, 10);
    HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
    hsetpos(h, 10);
    char b[8]; DWORD n = 99;
    SET_ERR(0x1357);
    BOOL ok = U(ReadFile)(h, b, 8, &n, NULL);
    CHECK(ok); EXPECT_EQ(n, 0);
    EXPECT_EQ(fpos(h), 10);
    /* far beyond EOF */
    hsetpos(h, 100000);
    n = 99; ok = U(ReadFile)(h, b, 8, &n, NULL);
    CHECK(ok); EXPECT_EQ(n, 0);
    EXPECT_EQ(fpos(h), 100000);
    /* partial read straddling EOF */
    hsetpos(h, 6);
    n = 99; ok = U(ReadFile)(h, b, 8, &n, NULL); CHECK(ok); EXPECT_EQ(n, 4); EXPECT_EQ(fpos(h), 10);
    CLOSE(h); rmfile(p);
}
TEST(fio_read_zero_length) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(ReadFile));
    TP(p, "rd_zero.bin"); mkfile_pat(p, 10);
    HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
    char b[2]; DWORD n = 99;
    CHECK(U(ReadFile)(h, b, 0, &n, NULL)); EXPECT_EQ(n, 0); EXPECT_EQ(fpos(h), 0);
    n = 99; CHECK(U(ReadFile)(h, NULL, 0, &n, NULL)); EXPECT_EQ(n, 0);
    CLOSE(h); rmfile(p);
}
TEST(fio_read_null_bytes_read_pointer) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(ReadFile));
    TP(p, "rd_nullcnt.bin"); mkfile_pat(p, 10);
    HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
    char b[4] = {0};
    BOOL ok = U(ReadFile)(h, b, 4, NULL, NULL);
    CHECK(ok);                           /* synchronous read tolerates a NULL count on Windows 10 */
    CHECK(!memcmp(b, "ABCD", 4));
    EXPECT_EQ(fpos(h), 4);
    CLOSE(h); rmfile(p);
}
TEST(fio_write_null_bytes_written_pointer) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(WriteFile));
    TP(p, "wr_nullcnt.bin"); rmfile(p);
    HANDLE h = cfa(p, GENERIC_WRITE, 0, CREATE_NEW, 0); OK_H(h);
    BOOL ok = U(WriteFile)(h, "abcd", 4, NULL, NULL);
    CHECK(ok);
    EXPECT_EQ(hsize(h), 4);
    CLOSE(h); rmfile(p);
}
TEST(fio_write_zero_length) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(WriteFile));
    TP(p, "wr_zero.bin"); mkfile_pat(p, 10);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, 0, OPEN_EXISTING, 0); OK_H(h);
    hsetpos(h, 4);
    DWORD n = 99;
    CHECK(U(WriteFile)(h, "x", 0, &n, NULL)); EXPECT_EQ(n, 0);
    EXPECT_EQ(hsize(h), 10); EXPECT_EQ(fpos(h), 4);       /* zero length write does not truncate */
    n = 99; CHECK(U(WriteFile)(h, NULL, 0, &n, NULL)); EXPECT_EQ(n, 0);
    CLOSE(h); EXPECT_EQ(fsize_of(p), 10); rmfile(p);
}
TEST(fio_write_extends_with_zero_gap) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(WriteFile));
    TP(p, "wr_gap.bin"); rmfile(p);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, 0, CREATE_NEW, 0); OK_H(h);
    hsetpos(h, 5000);
    EXPECT_EQ(hsize(h), 0);
    DWORD n = 0; CHECK(U(WriteFile)(h, "Z", 1, &n, NULL)); EXPECT_EQ(n, 1);
    EXPECT_EQ(hsize(h), 5001);
    hsetpos(h, 0);
    unsigned char *b = (unsigned char *)malloc(5001);
    CHECK(ReadFile(h, b, 5001, &n, NULL)); EXPECT_EQ(n, 5001);
    int z = 1; for (int i = 0; i < 5000; i++) if (b[i]) z = 0;
    CHECK(z); EXPECT_EQ(b[5000], 'Z');
    free(b); CLOSE(h); rmfile(p);
}
TEST(fio_write_large_32mb) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(WriteFile)); REQUIRE(U(ReadFile));
    TP(p, "wr_32m.bin"); rmfile(p);
    const DWORD SZ = 32u << 20;
    unsigned char *b = (unsigned char *)VirtualAlloc(NULL, SZ, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!b) { t_skip("no memory"); return; }
    for (DWORD i = 0; i < SZ; i += 4) *(DWORD *)(b + i) = i * 2654435761u;
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, 0, CREATE_NEW, 0); OK_H(h);
    DWORD n = 0; BOOL ok = U(WriteFile)(h, b, SZ, &n, NULL);
    CHECK(ok); EXPECT_EQ(n, SZ);
    EXPECT_EQ(hsize(h), SZ);
    memset(b, 0, SZ); hsetpos(h, 0); n = 0;
    ok = U(ReadFile)(h, b, SZ, &n, NULL);
    CHECK(ok); EXPECT_EQ(n, SZ);
    int bad = 0; for (DWORD i = 0; i < SZ; i += 4) if (*(DWORD *)(b + i) != i * 2654435761u) { bad = 1; break; }
    CHECK(!bad);
    n = 77; ok = U(ReadFile)(h, b, 16, &n, NULL); CHECK(ok); EXPECT_EQ(n, 0);
    CLOSE(h); VirtualFree(b, 0, MEM_RELEASE); rmfile(p);
}
TEST(fio_write_in_chunks_position) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(WriteFile));
    TP(p, "wr_chunks.bin"); rmfile(p);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, 0, CREATE_NEW, 0); OK_H(h);
    DWORD n; LONGLONG tot = 0;
    for (int i = 0; i < 100; i++) {
        char c[7]; memset(c, 'a' + i % 26, sizeof c);
        CHECK(U(WriteFile)(h, c, sizeof c, &n, NULL)); EXPECT_EQ(n, 7); tot += n;
        if (fpos(h) != tot) { CHECKF(0, "pos %lld tot %lld at %d", fpos(h), tot, i); break; }
    }
    EXPECT_EQ(hsize(h), 700);
    CLOSE(h); rmfile(p);
}
TEST(fio_rw_invalid_handles) {
    REQUIRE(U(ReadFile)); REQUIRE(U(WriteFile));
    char b[8]; DWORD n = 77;
    EXPECT_FAIL_B(U(ReadFile)(INVALID_HANDLE_VALUE, b, 8, &n, NULL), ERROR_INVALID_HANDLE); EXPECT_EQ(n, 0);
    n = 77; EXPECT_FAIL_B(U(ReadFile)(NULL, b, 8, &n, NULL), ERROR_INVALID_HANDLE); EXPECT_EQ(n, 0);
    n = 77; EXPECT_FAIL_B(U(WriteFile)(INVALID_HANDLE_VALUE, b, 8, &n, NULL), ERROR_INVALID_HANDLE); EXPECT_EQ(n, 0);
    n = 77; EXPECT_FAIL_B(U(WriteFile)(NULL, b, 8, &n, NULL), ERROR_INVALID_HANDLE); EXPECT_EQ(n, 0);
    n = 77; EXPECT_FAIL_B(U(ReadFile)((HANDLE)(ULONG_PTR)0x7ffffff0, b, 8, &n, NULL), ERROR_INVALID_HANDLE); EXPECT_EQ(n, 0);
}
TEST(fio_rw_closed_handle) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(ReadFile));
    TP(p, "rw_closed.bin"); mkfile_pat(p, 8);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, 0, OPEN_EXISTING, 0); OK_H(h);
    CloseHandle(h);
    char b[8]; DWORD n = 77;
    EXPECT_FAIL_B(U(ReadFile)(h, b, 8, &n, NULL), ERROR_INVALID_HANDLE); EXPECT_EQ(n, 0);
    n = 77; EXPECT_FAIL_B(U(WriteFile)(h, b, 8, &n, NULL), ERROR_INVALID_HANDLE); EXPECT_EQ(n, 0);
    EXPECT_FAIL_B(U(FlushFileBuffers)(h), ERROR_INVALID_HANDLE);
    EXPECT_FAIL_B(U(SetEndOfFile)(h), ERROR_INVALID_HANDLE);
    EXPECT_FAIL_DW(U(GetFileSize)(h, NULL), INVALID_FILE_SIZE, ERROR_INVALID_HANDLE);
    EXPECT_FAIL_DW(U(SetFilePointer)(h, 0, NULL, FILE_BEGIN), INVALID_SET_FILE_POINTER, ERROR_INVALID_HANDLE);
    EXPECT_FAIL_B(U(CloseHandle)(h), ERROR_INVALID_HANDLE);
    rmfile(p);
}
TEST(fio_failed_io_does_not_move_position) {
    REQUIRE(U(CreateFileA));
    TP(p, "rw_failpos.bin"); mkfile_pat(p, 20);
    HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
    hsetpos(h, 7);
    DWORD n; BOOL ok = U(WriteFile)(h, "xx", 2, &n, NULL); CHECK(!ok);
    EXPECT_EQ(fpos(h), 7);
    CLOSE(h);
    h = cfa(p, GENERIC_WRITE, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
    hsetpos(h, 9);
    char b[4]; ok = U(ReadFile)(h, b, 4, &n, NULL); CHECK(!ok);
    EXPECT_EQ(fpos(h), 9);
    CLOSE(h); rmfile(p);
}
TEST(fio_read_returns_count_variants) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(ReadFile));
    TP(p, "rd_cnt.bin"); mkfile_pat(p, 100);
    HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
    static const DWORD req[] = {1, 2, 3, 99, 100, 101, 4096};
    for (size_t i = 0; i < sizeof req / sizeof *req; i++) {
        char b[4096]; DWORD n = 0; hsetpos(h, 0);
        CHECK(U(ReadFile)(h, b, req[i], &n, NULL));
        DWORD want = req[i] < 100 ? req[i] : 100;
        CHECKF(n == want, "req %lu got %lu", req[i], n);
        CHECKF(fpos(h) == (LONGLONG)want, "pos after %lu", req[i]);
        CHECK(b[0] == 'A');
    }
    CLOSE(h); rmfile(p);
}
TEST(fio_overlapped_write_read_offsets) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(ReadFile)); REQUIRE(U(WriteFile)); REQUIRE(U(GetOverlappedResult));
    TP(p, "ov_rw.bin"); rmfile(p);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, 0, CREATE_NEW, FILE_FLAG_OVERLAPPED); OK_H(h);
    OVERLAPPED ov; memset(&ov, 0, sizeof ov);
    ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    ov.Offset = 100;
    DWORD n = 0;
    BOOL ok = U(WriteFile)(h, "OVERLAPPED", 10, NULL, &ov);
    if (!ok) { EXPECT_ERR(ERROR_IO_PENDING); }
    CHECK(U(GetOverlappedResult)(h, &ov, &n, TRUE)); EXPECT_EQ(n, 10);
    EXPECT_EQ(hsize(h), 110);
    ResetEvent(ov.hEvent); memset(&ov, 0, sizeof ov.Internal * 2); ov.Offset = 100;
    char b[16] = {0};
    ok = U(ReadFile)(h, b, 10, NULL, &ov);
    if (!ok) { EXPECT_ERR(ERROR_IO_PENDING); }
    CHECK(U(GetOverlappedResult)(h, &ov, &n, TRUE)); EXPECT_EQ(n, 10);
    CHECK(!memcmp(b, "OVERLAPPED", 10));
    /* read at EOF with overlapped: failure ERROR_HANDLE_EOF */
    ov.Offset = 110; ResetEvent(ov.hEvent);
    SET_ERR(0);
    ok = U(ReadFile)(h, b, 10, &n, &ov);
    DWORD e = GetLastError();
    if (!ok && e == ERROR_IO_PENDING) { ok = U(GetOverlappedResult)(h, &ov, &n, TRUE); e = GetLastError(); }
    CHECK(!ok); EXPECT_EQ(e, ERROR_HANDLE_EOF);
    CloseHandle(ov.hEvent); CLOSE(h); rmfile(p);
}
TEST(fio_sync_handle_overlapped_offset_used) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(ReadFile));
    TP(p, "ov_sync.bin"); mkfile(p, "0123456789", 10);
    HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
    OVERLAPPED ov; memset(&ov, 0, sizeof ov); ov.Offset = 4;
    char b[8] = {0}; DWORD n = 0;
    CHECK(U(ReadFile)(h, b, 3, &n, &ov)); EXPECT_EQ(n, 3); CHECK(!memcmp(b, "456", 3));
    CLOSE(h); rmfile(p);
}
TEST(fio_overlapped_offset_high) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(ReadFile));
    TP(p, "ov_hi.bin"); mkfile_pat(p, 10);
    HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, FILE_FLAG_OVERLAPPED); OK_H(h);
    OVERLAPPED ov; memset(&ov, 0, sizeof ov); ov.OffsetHigh = 1; ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    char b[4]; DWORD n = 55;
    BOOL ok = U(ReadFile)(h, b, 4, &n, &ov);
    DWORD e = GetLastError();
    if (!ok && e == ERROR_IO_PENDING) { ok = U(GetOverlappedResult)(h, &ov, &n, TRUE); e = GetLastError(); }
    CHECK(!ok); EXPECT_EQ(e, ERROR_HANDLE_EOF);
    CloseHandle(ov.hEvent); CLOSE(h); rmfile(p);
}

/* ---------- SetFilePointer ---------- */
TEST(fio_setfilepointer_methods) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(SetFilePointer));
    TP(p, "sfp_m.bin"); mkfile_pat(p, 100);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, 0, OPEN_EXISTING, 0); OK_H(h);
    EXPECT_EQ(U(SetFilePointer)(h, 10, NULL, FILE_BEGIN), 10);
    EXPECT_EQ(U(SetFilePointer)(h, 5, NULL, FILE_CURRENT), 15);
    EXPECT_EQ(U(SetFilePointer)(h, -5, NULL, FILE_CURRENT), 10);
    EXPECT_EQ(U(SetFilePointer)(h, 0, NULL, FILE_END), 100);
    EXPECT_EQ(U(SetFilePointer)(h, -1, NULL, FILE_END), 99);
    EXPECT_EQ(U(SetFilePointer)(h, -100, NULL, FILE_END), 0);
    EXPECT_EQ(U(SetFilePointer)(h, 50, NULL, FILE_END), 150);
    EXPECT_EQ(hsize(h), 100);
    EXPECT_EQ(U(SetFilePointer)(h, 0, NULL, FILE_CURRENT), 150);
    EXPECT_EQ(U(SetFilePointer)(h, 0, NULL, FILE_BEGIN), 0);
    CLOSE(h); rmfile(p);
}
TEST(fio_setfilepointer_negative) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(SetFilePointer));
    TP(p, "sfp_n.bin"); mkfile_pat(p, 100);
    HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
    hsetpos(h, 40);
    EXPECT_FAIL_DW(U(SetFilePointer)(h, -1, NULL, FILE_BEGIN), INVALID_SET_FILE_POINTER, ERROR_NEGATIVE_SEEK);
    EXPECT_EQ(fpos(h), 40);                                  /* position unchanged by the failure */
    EXPECT_FAIL_DW(U(SetFilePointer)(h, -41, NULL, FILE_CURRENT), INVALID_SET_FILE_POINTER, ERROR_NEGATIVE_SEEK);
    EXPECT_FAIL_DW(U(SetFilePointer)(h, -101, NULL, FILE_END), INVALID_SET_FILE_POINTER, ERROR_NEGATIVE_SEEK);
    EXPECT_FAIL_DW(U(SetFilePointer)(h, (LONG)0x80000000, NULL, FILE_BEGIN), INVALID_SET_FILE_POINTER, ERROR_NEGATIVE_SEEK);
    LONG hi = -1;
    EXPECT_FAIL_DW(U(SetFilePointer)(h, 0, &hi, FILE_BEGIN), INVALID_SET_FILE_POINTER, ERROR_NEGATIVE_SEEK);
    hi = (LONG)0x80000000;
    EXPECT_FAIL_DW(U(SetFilePointer)(h, 5, &hi, FILE_BEGIN), INVALID_SET_FILE_POINTER, ERROR_NEGATIVE_SEEK);
    EXPECT_EQ(fpos(h), 40);
    CLOSE(h); rmfile(p);
}
TEST(fio_setfilepointer_bad_method) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(SetFilePointer));
    TP(p, "sfp_bm.bin"); mkfile_pat(p, 10);
    HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
    static const DWORD bad[] = {3, 4, 5, 100, 0x80000000, 0xFFFFFFFF};
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        SET_ERR(0);
        DWORD r = U(SetFilePointer)(h, 1, NULL, bad[i]);
        DWORD e = GetLastError();
        CHECKF(r == INVALID_SET_FILE_POINTER && e == ERROR_INVALID_PARAMETER, "method %lu ret=0x%lx gle=%lu", bad[i], r, e);
    }
    EXPECT_EQ(fpos(h), 0);
    CLOSE(h); rmfile(p);
}
TEST(fio_setfilepointer_high_dword_semantics) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(SetFilePointer));
    TP(p, "sfp_hi.bin"); mkfile_pat(p, 100);
    HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
    LONG hi = 1;
    SET_ERR(0);
    DWORD r = U(SetFilePointer)(h, 5, &hi, FILE_BEGIN);
    EXPECT_EQ(r, 5); EXPECT_EQ(hi, 1); EXPECT_EQ(GetLastError(), 0);
    EXPECT_EQ(fpos(h), 0x100000005LL);
    hi = 0; r = U(SetFilePointer)(h, 3, &hi, FILE_CURRENT);
    EXPECT_EQ(r, 8); EXPECT_EQ(hi, 1);                       /* relative to the 64-bit current position */
    hi = 0; r = U(SetFilePointer)(h, 0, &hi, FILE_BEGIN);
    EXPECT_EQ(r, 0); EXPECT_EQ(hi, 0);
    /* new position reported through the high pointer, even for FILE_END */
    hi = 0; r = U(SetFilePointer)(h, 0, &hi, FILE_END);
    EXPECT_EQ(r, 100); EXPECT_EQ(hi, 0);
    hi = 7; r = U(SetFilePointer)(h, 0, &hi, FILE_CURRENT);   /* input high ignored? it is added for FILE_CURRENT */
    EXPECT_EQ(hi, 7);
    CLOSE(h); rmfile(p);
}
TEST(fio_setfilepointer_invalid_set_file_pointer_ambiguity) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(SetFilePointer));
    TP(p, "sfp_amb.bin"); mkfile_pat(p, 10);
    HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
    LONG hi = 0;
    SET_ERR(0x5555);
    DWORD r = U(SetFilePointer)(h, (LONG)0xFFFFFFFF, &hi, FILE_BEGIN);  /* legit position 0x00000000FFFFFFFF */
    DWORD e = GetLastError();
    EXPECT_EQ(r, 0xFFFFFFFF); EXPECT_EQ(hi, 0);
    EXPECT_EQ(e, NO_ERROR);      /* success is signalled by last-error being cleared */
    EXPECT_EQ(fpos(h), 0xFFFFFFFFLL);
    /* Without high pointer, positions >= 4 GiB are rejected rather than truncated. */
    SET_ERR(0);
    r = U(SetFilePointer)(h, 1, NULL, FILE_CURRENT);
    e = GetLastError();
    CHECKF(r == INVALID_SET_FILE_POINTER && e == ERROR_INVALID_PARAMETER, "ret=0x%lx gle=%lu", r, e);
    EXPECT_EQ(fpos(h), 0xFFFFFFFFLL);
    CLOSE(h); rmfile(p);
}
TEST(fio_setfilepointer_two_gib_boundary) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(SetFilePointer));
    TP(p, "sfp_2g.bin"); mkfile_pat(p, 10);
    HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
    LONG hi = 0;
    DWORD r = U(SetFilePointer)(h, 0x7FFFFFFF, &hi, FILE_BEGIN);
    EXPECT_EQ(r, 0x7FFFFFFF); EXPECT_EQ(hi, 0);
    SET_ERR(0);
    r = U(SetFilePointer)(h, 1, NULL, FILE_CURRENT);        /* 0x80000000: still representable, high bit set */
    EXPECT_EQ(r, 0x80000000u);
    EXPECT_EQ(fpos(h), 0x80000000LL);
    r = U(SetFilePointer)(h, 0x7FFFFFFF, NULL, FILE_CURRENT);
    EXPECT_EQ(r, 0xFFFFFFFFu);
    EXPECT_ERR(NO_ERROR);          /* 0xFFFFFFFF as a legitimate result clears last-error */
    CLOSE(h); rmfile(p);
}
TEST(fio_setfilepointer_seek_past_eof_does_not_grow) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(SetFilePointer)); REQUIRE(U(GetFileSize));
    TP(p, "sfp_peof.bin"); mkfile_pat(p, 10);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, 0, OPEN_EXISTING, 0); OK_H(h);
    EXPECT_EQ(U(SetFilePointer)(h, 1000000, NULL, FILE_BEGIN), 1000000);
    EXPECT_EQ(U(GetFileSize)(h, NULL), 10);
    CLOSE(h); EXPECT_EQ(fsize_of(p), 10); rmfile(p);
}
TEST(fio_setfilepointerex_methods) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(SetFilePointerEx));
    TP(p, "sfpx_m.bin"); mkfile_pat(p, 100);
    HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
    LARGE_INTEGER d, r;
    d.QuadPart = 20; r.QuadPart = -5;
    CHECK(U(SetFilePointerEx)(h, d, &r, FILE_BEGIN)); EXPECT_EQ(r.QuadPart, 20);
    d.QuadPart = -5; CHECK(U(SetFilePointerEx)(h, d, &r, FILE_CURRENT)); EXPECT_EQ(r.QuadPart, 15);
    d.QuadPart = 0;  CHECK(U(SetFilePointerEx)(h, d, &r, FILE_END)); EXPECT_EQ(r.QuadPart, 100);
    d.QuadPart = -100; CHECK(U(SetFilePointerEx)(h, d, &r, FILE_END)); EXPECT_EQ(r.QuadPart, 0);
    d.QuadPart = 0x123456789ALL; CHECK(U(SetFilePointerEx)(h, d, &r, FILE_BEGIN)); EXPECT_EQ(r.QuadPart, 0x123456789ALL);
    d.QuadPart = 1; CHECK(U(SetFilePointerEx)(h, d, NULL, FILE_CURRENT));       /* NULL out pointer allowed */
    EXPECT_EQ(fpos(h), 0x123456789BLL);
    CLOSE(h); rmfile(p);
}
TEST(fio_setfilepointerex_errors) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(SetFilePointerEx));
    TP(p, "sfpx_e.bin"); mkfile_pat(p, 100);
    HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
    LARGE_INTEGER d, r;
    hsetpos(h, 30);
    d.QuadPart = -1; r.QuadPart = 1234;
    EXPECT_FAIL_B(U(SetFilePointerEx)(h, d, &r, FILE_BEGIN), ERROR_NEGATIVE_SEEK);
    EXPECT_EQ(r.QuadPart, 1234);                             /* output untouched on failure */
    d.QuadPart = -31; EXPECT_FAIL_B(U(SetFilePointerEx)(h, d, &r, FILE_CURRENT), ERROR_NEGATIVE_SEEK);
    d.QuadPart = -101; EXPECT_FAIL_B(U(SetFilePointerEx)(h, d, &r, FILE_END), ERROR_NEGATIVE_SEEK);
    d.QuadPart = (LONGLONG)0x8000000000000000ULL; EXPECT_FAIL_B(U(SetFilePointerEx)(h, d, &r, FILE_BEGIN), ERROR_NEGATIVE_SEEK);
    d.QuadPart = 0;
    EXPECT_FAIL_B(U(SetFilePointerEx)(h, d, &r, 3), ERROR_INVALID_PARAMETER);
    EXPECT_FAIL_B(U(SetFilePointerEx)(h, d, &r, 0xFFFFFFFF), ERROR_INVALID_PARAMETER);
    EXPECT_FAIL_B(U(SetFilePointerEx)(INVALID_HANDLE_VALUE, d, &r, FILE_BEGIN), ERROR_INVALID_HANDLE);
    EXPECT_FAIL_B(U(SetFilePointerEx)(NULL, d, &r, FILE_BEGIN), ERROR_INVALID_HANDLE);
    EXPECT_FAIL_B(U(SetFilePointerEx)(INVALID_HANDLE_VALUE, d, &r, FILE_END), ERROR_INVALID_HANDLE);
    EXPECT_EQ(fpos(h), 30);
    CLOSE(h); rmfile(p);
}
TEST(fio_setfilepointerex_huge_positions) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(SetFilePointerEx));
    TP(p, "sfpx_h.bin"); mkfile_pat(p, 10);
    HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
    LARGE_INTEGER d, r;
    d.QuadPart = 0x7FFFFFFFFFFFFFFFLL;
    SET_ERR(0);
    BOOL ok = U(SetFilePointerEx)(h, d, &r, FILE_BEGIN);
    t_note("seek to LLONG_MAX: ok=%d gle=%lu", ok, GetLastError());
    d.QuadPart = 1LL << 40; ok = U(SetFilePointerEx)(h, d, &r, FILE_BEGIN);
    CHECK(ok); if (ok) EXPECT_EQ(r.QuadPart, 1LL << 40);
    d.QuadPart = 0; ok = U(SetFilePointerEx)(h, d, &r, FILE_END); CHECK(ok); EXPECT_EQ(r.QuadPart, 10);
    CLOSE(h); rmfile(p);
}
TEST(fio_setfilepointer_directory_handle) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(SetFilePointer));
    TP(d, "sfp_dir"); RemoveDirectoryA(d); CreateDirectoryA(d, NULL);
    HANDLE h = cfa(d, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS); OK_H(h);
    SET_ERR(0);
    DWORD r = U(SetFilePointer)(h, 0, NULL, FILE_CURRENT);
    t_note("SetFilePointer(dir) = 0x%lx gle=%lu", r, GetLastError());
    CLOSE(h); RemoveDirectoryA(d);
}

/* ---------- SetEndOfFile ---------- */
TEST(fio_seteof_truncate_and_extend) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(SetEndOfFile));
    TP(p, "eof_te.bin"); mkfile_pat(p, 100);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, 0, OPEN_EXISTING, 0); OK_H(h);
    hsetpos(h, 30);
    CHECK(U(SetEndOfFile)(h));
    EXPECT_EQ(hsize(h), 30); EXPECT_EQ(fpos(h), 30);
    hsetpos(h, 200);
    CHECK(U(SetEndOfFile)(h));
    EXPECT_EQ(hsize(h), 200); EXPECT_EQ(fpos(h), 200);
    /* extension is zero filled; original bytes retained */
    hsetpos(h, 0);
    unsigned char b[200]; DWORD n = 0; CHECK(ReadFile(h, b, 200, &n, NULL)); EXPECT_EQ(n, 200);
    CHECK(b[0] == 'A' && b[29] == ('A' + 29 % 26));
    int z = 1; for (int i = 30; i < 200; i++) if (b[i]) z = 0;
    CHECK(z);
    hsetpos(h, 0);
    CHECK(U(SetEndOfFile)(h)); EXPECT_EQ(hsize(h), 0);
    CHECK(U(SetEndOfFile)(h)); EXPECT_EQ(hsize(h), 0);    /* idempotent */
    CLOSE(h); EXPECT_EQ(fsize_of(p), 0); rmfile(p);
}
TEST(fio_seteof_errors) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(SetEndOfFile));
    EXPECT_FAIL_B(U(SetEndOfFile)(INVALID_HANDLE_VALUE), ERROR_INVALID_HANDLE);
    EXPECT_FAIL_B(U(SetEndOfFile)(NULL), ERROR_INVALID_HANDLE);
    TP(p, "eof_err.bin"); mkfile_pat(p, 10);
    HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
    EXPECT_FAIL_B(U(SetEndOfFile)(h), ERROR_ACCESS_DENIED);
    CLOSE(h);
    h = cfa(p, FILE_APPEND_DATA, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
    EXPECT_FAIL_B(U(SetEndOfFile)(h), ERROR_ACCESS_DENIED);   /* needs FILE_WRITE_DATA */
    CLOSE(h);
    h = cfa(p, 0, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
    EXPECT_FAIL_B(U(SetEndOfFile)(h), ERROR_ACCESS_DENIED);
    CLOSE(h);
    EXPECT_EQ(fsize_of(p), 10);
    rmfile(p);
}
TEST(fio_seteof_mapped_section_blocks_truncate) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(SetEndOfFile));
    TP(p, "eof_map.bin"); mkfile_pat(p, 8192);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, 0); OK_H(h);
    HANDLE m = CreateFileMappingA(h, NULL, PAGE_READWRITE, 0, 0, NULL);
    if (!m) { t_skip("no mapping"); CLOSE(h); rmfile(p); return; }
    void *v = MapViewOfFile(m, FILE_MAP_WRITE, 0, 0, 0);
    hsetpos(h, 100);
    EXPECT_FAIL_B(U(SetEndOfFile)(h), ERROR_USER_MAPPED_FILE);     /* 1224 */
    EXPECT_EQ(hsize(h), 8192);
    hsetpos(h, 8192 + 4096);
    CHECK(U(SetEndOfFile)(h));                                     /* growing is allowed */
    if (v) UnmapViewOfFile(v);
    CloseHandle(m);
    hsetpos(h, 100);
    CHECK(U(SetEndOfFile)(h)); EXPECT_EQ(hsize(h), 100);
    CLOSE(h); rmfile(p);
}

/* ---------- sizes, type, flush ---------- */
TEST(fio_getfilesize_basic) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(GetFileSize));
    TP(p, "gfs.bin"); mkfile_pat(p, 12345);
    HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
    DWORD hi = 0xDEAD;
    SET_ERR(0x42);
    EXPECT_EQ(U(GetFileSize)(h, &hi), 12345); EXPECT_EQ(hi, 0);
    EXPECT_EQ(U(GetFileSize)(h, NULL), 12345);
    CLOSE(h);
    h = cfa(p, 0, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
    EXPECT_EQ(U(GetFileSize)(h, NULL), 12345);                  /* attribute-only handle suffices */
    CLOSE(h); rmfile(p);
}
TEST(fio_getfilesize_invalid_handles) {
    REQUIRE(U(GetFileSize)); REQUIRE(U(GetFileSizeEx));
    DWORD hi = 0xABCD;
    EXPECT_FAIL_DW(U(GetFileSize)(INVALID_HANDLE_VALUE, &hi), INVALID_FILE_SIZE, ERROR_INVALID_HANDLE);
    EXPECT_FAIL_DW(U(GetFileSize)(NULL, NULL), INVALID_FILE_SIZE, ERROR_INVALID_HANDLE);
    LARGE_INTEGER s; s.QuadPart = 0x1234;
    EXPECT_FAIL_B(U(GetFileSizeEx)(INVALID_HANDLE_VALUE, &s), ERROR_INVALID_HANDLE);
    EXPECT_FAIL_B(U(GetFileSizeEx)(NULL, &s), ERROR_INVALID_HANDLE);
}
TEST(fio_getfilesizeex_basic) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(GetFileSizeEx));
    TP(p, "gfsx.bin"); mkfile_pat(p, 70000);
    HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
    LARGE_INTEGER s; s.QuadPart = -1;
    CHECK(U(GetFileSizeEx)(h, &s)); EXPECT_EQ(s.QuadPart, 70000);
    CLOSE(h); rmfile(p);
}
TEST(fio_getfilesize_tracks_writes) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(GetFileSize)); REQUIRE(U(WriteFile));
    TP(p, "gfs_track.bin"); rmfile(p);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, CREATE_NEW, 0); OK_H(h);
    HANDLE h2 = cfa(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, 0); OK_H(h2);
    DWORD n;
    for (int i = 1; i <= 5; i++) {
        U(WriteFile)(h, "0123456789", 10, &n, NULL);
        EXPECT_EQ(U(GetFileSize)(h, NULL), 10 * i);
        EXPECT_EQ(U(GetFileSize)(h2, NULL), 10 * i);       /* visible through other handle without flush */
    }
    CLOSE(h); CLOSE(h2); rmfile(p);
}
TEST(fio_getfiletype) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(GetFileType));
    TP(p, "gft.bin"); mkfile_pat(p, 4);
    HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
    SET_ERR(0x99);
    EXPECT_EQ(U(GetFileType)(h), FILE_TYPE_DISK);
    EXPECT_EQ(GetLastError(), 0x99);                           /* not touched on success */
    CloseHandle(h);
    EXPECT_FAIL_DW(U(GetFileType)(h), FILE_TYPE_UNKNOWN, ERROR_INVALID_HANDLE);       /* closed */
    EXPECT_FAIL_DW(U(GetFileType)(NULL), FILE_TYPE_UNKNOWN, ERROR_INVALID_HANDLE);
    EXPECT_FAIL_DW(U(GetFileType)(INVALID_HANDLE_VALUE), FILE_TYPE_UNKNOWN, ERROR_INVALID_HANDLE);
    rmfile(p);
}
TEST(fio_getfiletype_pipe_and_char) {
    REQUIRE(U(GetFileType));
    HANDLE r, w;
    CHECK(CreatePipe(&r, &w, NULL, 0));
    EXPECT_EQ(U(GetFileType)(r), FILE_TYPE_PIPE); EXPECT_EQ(U(GetFileType)(w), FILE_TYPE_PIPE);
    CloseHandle(r); CloseHandle(w);
    HANDLE n = cfa("NUL", GENERIC_WRITE, 0, OPEN_EXISTING, 0);
    if (n != INVALID_HANDLE_VALUE) { EXPECT_EQ(U(GetFileType)(n), FILE_TYPE_CHAR); CloseHandle(n); }
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    DWORD t = U(GetFileType)(ev);
    t_note("GetFileType(event) = %lu gle=%lu", t, GetLastError());
    CloseHandle(ev);
    EXPECT_EQ(U(GetFileType)(GetCurrentProcess()), FILE_TYPE_UNKNOWN);
    HANDLE sk = CreateEventA(NULL, FALSE, FALSE, NULL); CloseHandle(sk);
}
TEST(fio_flushfilebuffers) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(FlushFileBuffers));
    TP(p, "flush.bin"); rmfile(p);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, 0, CREATE_NEW, 0); OK_H(h);
    DWORD n; WriteFile(h, "abc", 3, &n, NULL);
    SET_ERR(0x77);
    CHECK(U(FlushFileBuffers)(h));
    CHECK(U(FlushFileBuffers)(h));                 /* repeated */
    CLOSE(h);
    h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
    EXPECT_FAIL_B(U(FlushFileBuffers)(h), ERROR_ACCESS_DENIED);   /* flush needs write access */
    CLOSE(h);
    h = cfa(p, FILE_APPEND_DATA, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
    BOOL ok = U(FlushFileBuffers)(h); CHECK(ok);
    CLOSE(h);
    EXPECT_FAIL_B(U(FlushFileBuffers)(INVALID_HANDLE_VALUE), ERROR_INVALID_HANDLE);
    EXPECT_FAIL_B(U(FlushFileBuffers)(NULL), ERROR_INVALID_HANDLE);
    rmfile(p);
}
TEST(fio_flushfilebuffers_nul_and_pipe) {
    REQUIRE(U(FlushFileBuffers));
    HANDLE n = cfa("NUL", GENERIC_WRITE, 0, OPEN_EXISTING, 0);
    if (n != INVALID_HANDLE_VALUE) { SET_ERR(0); BOOL ok = U(FlushFileBuffers)(n); t_note("Flush(NUL)=%d gle=%lu", ok, GetLastError()); CloseHandle(n); }
    HANDLE r, w; CHECK(CreatePipe(&r, &w, NULL, 0));
    SET_ERR(0);
    BOOL ok = U(FlushFileBuffers)(r);
    t_note("Flush(pipe read end)=%d gle=%lu", ok, GetLastError());
    CloseHandle(r); CloseHandle(w);
}
TEST(fio_setfilevaliddata_errors) {
    REQUIRE(U(SetFileValidData)); REQUIRE(U(CreateFileA));
    EXPECT_FAIL_B(U(SetFileValidData)(INVALID_HANDLE_VALUE, 0), ERROR_INVALID_HANDLE);
    EXPECT_FAIL_B(U(SetFileValidData)(NULL, 100), ERROR_INVALID_HANDLE);
    TP(p, "svd.bin"); mkfile_pat(p, 4096);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, 0, OPEN_EXISTING, 0); OK_H(h);
    SET_ERR(0);
    BOOL ok = U(SetFileValidData)(h, 4096);
    DWORD e = GetLastError();
    /* Needs SE_MANAGE_VOLUME_NAME; without it (the usual case) the privilege error is reported. */
    t_note("SetFileValidData(4096) = %d gle=%lu", ok, e);
    CHECK(ok || e == ERROR_PRIVILEGE_NOT_HELD || e == ERROR_INVALID_PARAMETER || e == ERROR_ACCESS_DENIED);
    EXPECT_EQ(fsize_of(p), 4096);
    CLOSE(h);
    h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
    SET_ERR(0); ok = U(SetFileValidData)(h, 100);
    CHECK(!ok);
    e = GetLastError(); CHECKF(e == ERROR_ACCESS_DENIED || e == ERROR_PRIVILEGE_NOT_HELD, "gle=%lu", e);
    CLOSE(h); rmfile(p);
}
TEST(fio_getfileinformationbyhandle_basic) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(GetFileInformationByHandle));
    TP(p, "gfibh.bin"); rmfile(p);
    FILETIME now; GetSystemTimeAsFileTime(&now);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, CREATE_NEW, FILE_ATTRIBUTE_NORMAL); OK_H(h);
    DWORD n; WriteFile(h, "0123456789", 10, &n, NULL);
    BY_HANDLE_FILE_INFORMATION bi; memset(&bi, 0xCC, sizeof bi);
    CHECK(U(GetFileInformationByHandle)(h, &bi));
    CHECK(bi.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE);
    CHECK(!(bi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));
    EXPECT_EQ(bi.nFileSizeLow, 10); EXPECT_EQ(bi.nFileSizeHigh, 0);
    EXPECT_EQ(bi.nNumberOfLinks, 1);
    CHECK(bi.dwVolumeSerialNumber != 0xCCCCCCCC);
    CHECK(bi.nFileIndexLow != 0 || bi.nFileIndexHigh != 0);
    ULONGLONG c = ((ULONGLONG)bi.ftCreationTime.dwHighDateTime << 32) | bi.ftCreationTime.dwLowDateTime;
    ULONGLONG nw = ((ULONGLONG)now.dwHighDateTime << 32) | now.dwLowDateTime;
    CHECKF(c + 36000000000ULL > nw && c < nw + 36000000000ULL, "creation time off by more than an hour");
    CLOSE(h);
    h = cfa(p, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, 0); OK_H(h);
    BY_HANDLE_FILE_INFORMATION b2; CHECK(U(GetFileInformationByHandle)(h, &b2));
    EXPECT_EQ(b2.nFileIndexLow, bi.nFileIndexLow); EXPECT_EQ(b2.nFileIndexHigh, bi.nFileIndexHigh);
    EXPECT_EQ(b2.dwVolumeSerialNumber, bi.dwVolumeSerialNumber);
    EXPECT_EQ(b2.nFileSizeLow, 10);
    CLOSE(h); rmfile(p);
}
TEST(fio_getfileinformationbyhandle_errors) {
    REQUIRE(U(GetFileInformationByHandle));
    BY_HANDLE_FILE_INFORMATION bi;
    EXPECT_FAIL_B(U(GetFileInformationByHandle)(INVALID_HANDLE_VALUE, &bi), ERROR_INVALID_HANDLE);
    EXPECT_FAIL_B(U(GetFileInformationByHandle)(NULL, &bi), ERROR_INVALID_HANDLE);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    BOOL ok = U(GetFileInformationByHandle)(ev, &bi);
    CHECK(!ok); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
}
TEST(fio_getfileinformationbyhandle_dir_and_hardlink) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(GetFileInformationByHandle));
    TP(p, "gfi_hl.bin"); TP(q, "gfi_hl2.bin"); rmfile(p); rmfile(q); mkfile_pat(p, 3);
    if (!CreateHardLinkA(q, p, NULL)) { t_note("hard links unavailable gle=%lu", GetLastError()); rmfile(p); return; }
    HANDLE h = cfa(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, OPEN_EXISTING, 0); OK_H(h);
    HANDLE g = cfa(q, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, OPEN_EXISTING, 0); OK_H(g);
    BY_HANDLE_FILE_INFORMATION a, b;
    CHECK(U(GetFileInformationByHandle)(h, &a)); CHECK(U(GetFileInformationByHandle)(g, &b));
    EXPECT_EQ(a.nNumberOfLinks, 2); EXPECT_EQ(b.nNumberOfLinks, 2);
    EXPECT_EQ(a.nFileIndexLow, b.nFileIndexLow); EXPECT_EQ(a.nFileIndexHigh, b.nFileIndexHigh);
    CLOSE(h); CLOSE(g); rmfile(p); rmfile(q);
}
TEST(fio_getfileinformationbyhandle_timestamps_move) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(GetFileInformationByHandle));
    TP(p, "gfi_ts.bin"); rmfile(p);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, CREATE_NEW, 0); OK_H(h);
    BY_HANDLE_FILE_INFORMATION a, b;
    CHECK(U(GetFileInformationByHandle)(h, &a));
    Sleep(50);
    DWORD n; WriteFile(h, "x", 1, &n, NULL);
    CHECK(U(GetFileInformationByHandle)(h, &b));
    CHECK(CompareFileTime(&b.ftLastWriteTime, &a.ftLastWriteTime) >= 0);
    CHECK(CompareFileTime(&b.ftCreationTime, &a.ftCreationTime) == 0);
    EXPECT_EQ(b.nFileSizeLow, 1);
    CLOSE(h); rmfile(p);
}
