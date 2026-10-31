#define AIO_TU "fb"
#include "aio.h"

TEST(aio_file_read_basic) {
    REQUIRE(U(ReadFile)); REQUIRE(U(GetOverlappedResult));
    char p[MAX_PATH]; mkpath(p, "rb"); CHECK(make_file(p, 65536));
    HANDLE h = open_ov(p, GENERIC_READ, 0); CHECK(h != INVALID_HANDLE_VALUE);
    HANDLE ev = mkevent(); OVERLAPPED ov; ovinit(&ov, 0, ev);
    BYTE *b = calloc(1, 4096);
    iores x = do_read(h, b, 4096, &ov);
    CHECK(x.ok); EXPECT_EQ(x.n, 4096); CHECK(chkp(b, 0, 4096));
    EXPECT_EQ((DWORD)ov.Internal, 0); EXPECT_EQ(ov.InternalHigh, 4096);
    CHECK(signaled(ev));
    free(b); CloseHandle(ev); CloseHandle(h);
}

TEST(aio_file_read_offsets_table) {
    REQUIRE(U(ReadFile));
    char p[MAX_PATH]; mkpath(p, "off"); CHECK(make_file(p, 70000));
    HANDLE h = open_ov(p, GENERIC_READ, 0); HANDLE ev = mkevent();
    static const DWORD offs[] = {0, 1, 7, 511, 512, 513, 4095, 4096, 4097, 8191, 32768, 65535, 69899};
    for (unsigned i = 0; i < sizeof offs / sizeof offs[0]; i++) {
        BYTE b[100]; memset(b, 0xEE, sizeof b); OVERLAPPED ov; ovinit(&ov, offs[i], ev);
        iores x = do_read(h, b, 100, &ov);
        CHECKF(x.ok && x.n == 100 && chkp(b, offs[i], 100), "offset %lu", offs[i]);
    }
    CloseHandle(ev); CloseHandle(h);
}

TEST(aio_file_read_offsethigh_beyond_eof) {
    REQUIRE(U(ReadFile));
    char p[MAX_PATH]; mkpath(p, "oh"); CHECK(make_file(p, 4096));
    HANDLE h = open_ov(p, GENERIC_READ, 0); HANDLE ev = mkevent();
    OVERLAPPED ov; ovinit(&ov, 0x100000000ULL, ev); BYTE b[16];
    iores x = do_read(h, b, 16, &ov);
    CHECK(!x.ok); EXPECT_EQ(x.err, ERROR_HANDLE_EOF);
    EXPECT_EQ((DWORD)ov.Internal, NTS_EOF); EXPECT_EQ(ov.InternalHigh, 0);
    CloseHandle(ev); CloseHandle(h);
}

TEST(aio_file_read_eof_exact) {
    REQUIRE(U(ReadFile));
    char p[MAX_PATH]; mkpath(p, "eof"); CHECK(make_file(p, 8192));
    HANDLE h = open_ov(p, GENERIC_READ, 0); HANDLE ev = mkevent();
    OVERLAPPED ov; ovinit(&ov, 8192, ev); BYTE b[64]; DWORD n = 77;
    SET_ERR(0); BOOL r = U(ReadFile)(h, b, 64, &n, &ov);
    DWORD e = GetLastError();
    CHECK(!r); EXPECT_EQ(n, 0);
    if (e == ERROR_IO_PENDING) { t_note("EOF read went pending"); DWORD gn; CHECK(!GetOverlappedResultEx(h, &ov, &gn, 5000, FALSE)); e = GetLastError(); }
    EXPECT_EQ(e, ERROR_HANDLE_EOF);
    EXPECT_EQ((DWORD)ov.Internal, NTS_EOF);
    EXPECT_EQ(ov.InternalHigh, 0);
    /* GetOverlappedResult reports the same */
    DWORD gn = 55; SET_ERR(0);
    BOOL g = U(GetOverlappedResult)(h, &ov, &gn, FALSE);
    CHECK(!g); EXPECT_ERR(ERROR_HANDLE_EOF); EXPECT_EQ(gn, 0);
    CloseHandle(ev); CloseHandle(h);
}

TEST(aio_file_read_partial_last) {
    REQUIRE(U(ReadFile));
    char p[MAX_PATH]; mkpath(p, "part"); CHECK(make_file(p, 10000));
    HANDLE h = open_ov(p, GENERIC_READ, 0); HANDLE ev = mkevent();
    OVERLAPPED ov; ovinit(&ov, 9900, ev); BYTE b[4096]; memset(b, 0xAB, sizeof b);
    iores x = do_read(h, b, 4096, &ov);
    CHECK(x.ok); EXPECT_EQ(x.n, 100); CHECK(chkp(b, 9900, 100)); EXPECT_EQ(b[100], 0xAB);
    CloseHandle(ev); CloseHandle(h);
}

TEST(aio_file_read_buffer_larger_than_file) {
    REQUIRE(U(ReadFile));
    char p[MAX_PATH]; mkpath(p, "big"); CHECK(make_file(p, 1000));
    HANDLE h = open_ov(p, GENERIC_READ, 0); HANDLE ev = mkevent();
    OVERLAPPED ov; ovinit(&ov, 0, ev); BYTE *b = calloc(1, 65536);
    iores x = do_read(h, b, 65536, &ov);
    CHECK(x.ok); EXPECT_EQ(x.n, 1000); CHECK(chkp(b, 0, 1000));
    free(b); CloseHandle(ev); CloseHandle(h);
}

TEST(aio_file_read_empty_file_eof) {
    REQUIRE(U(ReadFile));
    char p[MAX_PATH]; mkpath(p, "empty"); CHECK(make_file(p, 0));
    HANDLE h = open_ov(p, GENERIC_READ, 0); HANDLE ev = mkevent();
    OVERLAPPED ov; ovinit(&ov, 0, ev); BYTE b[8];
    iores x = do_read(h, b, 8, &ov);
    CHECK(!x.ok); EXPECT_EQ(x.err, ERROR_HANDLE_EOF);
    CloseHandle(ev); CloseHandle(h);
}

TEST(aio_file_read_zero_length) {
    REQUIRE(U(ReadFile));
    char p[MAX_PATH]; mkpath(p, "z0"); CHECK(make_file(p, 4096));
    HANDLE h = open_ov(p, GENERIC_READ, 0); HANDLE ev = mkevent();
    OVERLAPPED ov; ovinit(&ov, 100, ev); BYTE b[4];
    iores x = do_read(h, b, 0, &ov);
    CHECK(x.ok); EXPECT_EQ(x.n, 0);
    EXPECT_EQ((DWORD)ov.Internal, 0);
    CloseHandle(ev); CloseHandle(h);
}

TEST(aio_file_write_zero_length) {
    REQUIRE(U(WriteFile));
    char p[MAX_PATH]; mkpath(p, "wz"); CHECK(make_file(p, 4096));
    HANDLE h = open_ov(p, GENERIC_WRITE, 0); HANDLE ev = mkevent();
    OVERLAPPED ov; ovinit(&ov, 100, ev); BYTE b[4] = {1};
    iores x = do_write(h, b, 0, &ov);
    CHECK(x.ok); EXPECT_EQ(x.n, 0);
    CloseHandle(h);
    h = open_sync(p, GENERIC_READ); EXPECT_EQ(GetFileSize(h, NULL), 4096); CloseHandle(h);
    CloseHandle(ev);
}

TEST(aio_file_write_basic_verify) {
    REQUIRE(U(WriteFile));
    char p[MAX_PATH]; mkpath(p, "wb");
    HANDLE h = new_ov_file(p, 0); CHECK(h != INVALID_HANDLE_VALUE);
    HANDLE ev = mkevent(); BYTE *b = malloc(10000); fillp(b, 0, 10000);
    OVERLAPPED ov; ovinit(&ov, 0, ev);
    iores x = do_write(h, b, 10000, &ov);
    CHECK(x.ok); EXPECT_EQ(x.n, 10000); EXPECT_EQ((DWORD)ov.Internal, 0); EXPECT_EQ(ov.InternalHigh, 10000);
    CloseHandle(h);
    h = open_sync(p, GENERIC_READ); EXPECT_EQ(GetFileSize(h, NULL), 10000);
    BYTE *r = malloc(10000); DWORD n; CHECK(ReadFile(h, r, 10000, &n, NULL)); CHECK(n == 10000 && !memcmp(r, b, 10000));
    free(r); free(b); CloseHandle(h); CloseHandle(ev);
}

TEST(aio_file_write_offsets_and_gap_zero) {
    REQUIRE(U(WriteFile));
    char p[MAX_PATH]; mkpath(p, "wgap");
    HANDLE h = new_ov_file(p, 0); HANDLE ev = mkevent();
    BYTE b[256]; fillp(b, 1000, 256);
    OVERLAPPED ov; ovinit(&ov, 20000, ev);
    iores x = do_write(h, b, 256, &ov);
    CHECK(x.ok); EXPECT_EQ(x.n, 256);
    CloseHandle(h);
    h = open_sync(p, GENERIC_READ); EXPECT_EQ(GetFileSize(h, NULL), 20256);
    BYTE *r = malloc(20256); DWORD n; CHECK(ReadFile(h, r, 20256, &n, NULL)); EXPECT_EQ(n, 20256);
    int zero = 1; for (int i = 0; i < 20000; i++) if (r[i]) { zero = 0; break; }
    CHECK(zero); CHECK(!memcmp(r + 20000, b, 256));
    free(r); CloseHandle(h); CloseHandle(ev);
}

TEST(aio_file_write_overwrite_middle) {
    REQUIRE(U(WriteFile));
    char p[MAX_PATH]; mkpath(p, "wmid"); CHECK(make_file(p, 8192));
    HANDLE h = open_ov(p, GENERIC_READ | GENERIC_WRITE, 0); HANDLE ev = mkevent();
    BYTE b[100]; memset(b, 0x5A, 100); OVERLAPPED ov; ovinit(&ov, 4000, ev);
    iores x = do_write(h, b, 100, &ov); CHECK(x.ok); EXPECT_EQ(x.n, 100);
    BYTE r[8192]; ovinit(&ov, 0, ev); x = do_read(h, r, 8192, &ov); CHECK(x.ok); EXPECT_EQ(x.n, 8192);
    CHECK(chkp(r, 0, 4000)); CHECK(!memcmp(r + 4000, b, 100)); CHECK(chkp(r + 4100, 4100, 4092));
    CloseHandle(ev); CloseHandle(h);
}

TEST(aio_file_offset_fields_unchanged) {
    REQUIRE(U(ReadFile));
    char p[MAX_PATH]; mkpath(p, "offu"); CHECK(make_file(p, 8192));
    HANDLE h = open_ov(p, GENERIC_READ, 0); HANDLE ev = mkevent();
    OVERLAPPED ov; ovinit(&ov, 1234, ev); BYTE b[100];
    iores x = do_read(h, b, 100, &ov); CHECK(x.ok);
    EXPECT_EQ(ov.Offset, 1234); EXPECT_EQ(ov.OffsetHigh, 0); CHECK(ov.hEvent == ev);
    CloseHandle(ev); CloseHandle(h);
}

TEST(aio_file_internal_overwritten_on_start) {
    REQUIRE(U(ReadFile));
    char p[MAX_PATH]; mkpath(p, "intw"); CHECK(make_file(p, 8192));
    HANDLE h = open_ov(p, GENERIC_READ, 0); HANDLE ev = mkevent();
    OVERLAPPED ov; ovinit(&ov, 0, ev); BYTE b[100];
    ov.Internal = 0xDEAD; ov.InternalHigh = 0xBEEF;
    BOOL r = U(ReadFile)(h, b, 100, NULL, &ov);
    CHECK(ov.Internal != 0xDEAD);
    if (r) { EXPECT_EQ((DWORD)ov.Internal, 0); EXPECT_EQ(ov.InternalHigh, 100); }
    else { EXPECT_ERR(ERROR_IO_PENDING); }
    DWORD n; GetOverlappedResult(h, &ov, &n, TRUE);
    EXPECT_EQ((DWORD)ov.Internal, 0);
    CloseHandle(ev); CloseHandle(h);
}

TEST(aio_file_event_signalled_after_completion) {
    REQUIRE(U(ReadFile));
    char p[MAX_PATH]; mkpath(p, "evs"); CHECK(make_file(p, 8192));
    HANDLE h = open_ov(p, GENERIC_READ, 0); HANDLE ev = mkevent();
    SetEvent(ev); OVERLAPPED ov; ovinit(&ov, 0, ev); BYTE b[100];
    BOOL r = U(ReadFile)(h, b, 100, NULL, &ov);
    if (!r) { EXPECT_ERR(ERROR_IO_PENDING); }
    CHECK(WaitForSingleObject(ev, 5000) == WAIT_OBJECT_0);
    DWORD n; CHECK(GetOverlappedResult(h, &ov, &n, FALSE));
    CloseHandle(ev); CloseHandle(h);
}

TEST(aio_file_event_not_autoreset_by_gor) {
    REQUIRE(U(GetOverlappedResult));
    char p[MAX_PATH]; mkpath(p, "evm"); CHECK(make_file(p, 8192));
    HANDLE h = open_ov(p, GENERIC_READ, 0); HANDLE ev = mkevent();
    OVERLAPPED ov; ovinit(&ov, 0, ev); BYTE b[100];
    iores x = do_read(h, b, 100, &ov); CHECK(x.ok);
    DWORD n;
    for (int i = 0; i < 3; i++) { CHECK(U(GetOverlappedResult)(h, &ov, &n, TRUE)); EXPECT_EQ(n, 100); }
    CHECK(signaled(ev));
    CloseHandle(ev); CloseHandle(h);
}

TEST(aio_file_no_event_file_handle_signalled) {
    REQUIRE(U(ReadFile));
    char p[MAX_PATH]; mkpath(p, "nev"); CHECK(make_file(p, 8192));
    HANDLE h = open_ov(p, GENERIC_READ, 0);
    OVERLAPPED ov; ovinit(&ov, 0, NULL); BYTE b[100];
    BOOL r = U(ReadFile)(h, b, 100, NULL, &ov);
    if (!r) { EXPECT_ERR(ERROR_IO_PENDING); }
    DWORD n = 0; CHECK(U(GetOverlappedResult)(h, &ov, &n, TRUE)); EXPECT_EQ(n, 100);
    CHECK(signaled(h));
    CloseHandle(h);
}

TEST(aio_file_lowbit_event_still_signalled) {
    REQUIRE(U(ReadFile));
    char p[MAX_PATH]; mkpath(p, "lb"); CHECK(make_file(p, 8192));
    HANDLE h = open_ov(p, GENERIC_READ, 0); HANDLE ev = mkevent();
    OVERLAPPED ov; ovinit(&ov, 0, (HANDLE)((ULONG_PTR)ev | 1)); BYTE b[100];
    BOOL r = U(ReadFile)(h, b, 100, NULL, &ov);
    if (!r) { EXPECT_ERR(ERROR_IO_PENDING); }
    CHECK(WaitForSingleObject(ev, 5000) == WAIT_OBJECT_0);
    CHECK(chkp(b, 0, 100));
    CloseHandle(ev); CloseHandle(h);
}

TEST(aio_file_gor_success_clears_error_and_reports_bytes) {
    REQUIRE(U(GetOverlappedResult));
    char p[MAX_PATH]; mkpath(p, "gor"); CHECK(make_file(p, 8192));
    HANDLE h = open_ov(p, GENERIC_READ, 0); HANDLE ev = mkevent();
    OVERLAPPED ov; ovinit(&ov, 0, ev); BYTE b[300];
    iores x = do_read(h, b, 300, &ov); CHECK(x.ok);
    DWORD n = 0; SET_ERR(1234);
    BOOL g = U(GetOverlappedResult)(h, &ov, &n, FALSE);
    CHECK(g); EXPECT_EQ(n, 300); EXPECT_ERR(0);
    n = 0; SET_ERR(1234); g = U(GetOverlappedResult)(h, &ov, &n, TRUE);
    CHECK(g); EXPECT_EQ(n, 300); EXPECT_ERR(0);
    CloseHandle(ev); CloseHandle(h);
}

TEST(aio_file_gorex_completed_variants) {
    REQUIRE(U(GetOverlappedResultEx));
    char p[MAX_PATH]; mkpath(p, "gorex"); CHECK(make_file(p, 8192));
    HANDLE h = open_ov(p, GENERIC_READ, 0); HANDLE ev = mkevent();
    OVERLAPPED ov; ovinit(&ov, 0, ev); BYTE b[300];
    iores x = do_read(h, b, 300, &ov); CHECK(x.ok);
    static const DWORD tmo[] = {0, 1, 1000, INFINITE};
    for (int a = 0; a < 2; a++)
        for (unsigned i = 0; i < 4; i++) {
            DWORD n = 0; SET_ERR(999);
            BOOL g = U(GetOverlappedResultEx)(h, &ov, &n, tmo[i], a);
            CHECKF(g && n == 300, "tmo=%lu alertable=%d", tmo[i], a);
            EXPECT_ERR(0);
        }
    CloseHandle(ev); CloseHandle(h);
}

TEST(aio_file_gorex_eof_error) {
    REQUIRE(U(GetOverlappedResultEx));
    char p[MAX_PATH]; mkpath(p, "gorexeof"); CHECK(make_file(p, 100));
    HANDLE h = open_ov(p, GENERIC_READ, 0); HANDLE ev = mkevent();
    OVERLAPPED ov; ovinit(&ov, 100, ev); BYTE b[16];
    iores x = do_read(h, b, 16, &ov); CHECK(!x.ok);
    DWORD n = 9; SET_ERR(0);
    BOOL g = U(GetOverlappedResultEx)(h, &ov, &n, 0, FALSE);
    CHECK(!g); EXPECT_ERR(ERROR_HANDLE_EOF); EXPECT_EQ(n, 0);
    n = 9; g = U(GetOverlappedResultEx)(h, &ov, &n, 1000, TRUE);
    CHECK(!g); EXPECT_ERR(ERROR_HANDLE_EOF);
    CloseHandle(ev); CloseHandle(h);
}

TEST(aio_file_overlapped_reuse_loop) {
    REQUIRE(U(ReadFile));
    char p[MAX_PATH]; mkpath(p, "reuse"); CHECK(make_file(p, 100000));
    HANDLE h = open_ov(p, GENERIC_READ, 0); HANDLE ev = mkevent();
    OVERLAPPED ov; BYTE b[512]; int bad = 0;
    for (int i = 0; i < 1000; i++) {
        size_t off = (size_t)(i * 97) % 99000;
        ovinit(&ov, off, ev); ResetEvent(ev);
        iores x = do_read(h, b, 512, &ov);
        if (!(x.ok && x.n == 512 && chkp(b, off, 512))) bad++;
    }
    EXPECT_EQ(bad, 0);
    CloseHandle(ev); CloseHandle(h);
}

TEST(aio_file_multi_outstanding_reads_64) {
    REQUIRE(U(ReadFile));
    enum { N = 64, SZ = 65536 };
    char p[MAX_PATH]; mkpath(p, "m64"); CHECK(make_file(p, (size_t)N * SZ));
    HANDLE h = open_ov(p, GENERIC_READ, 0);
    OVERLAPPED ov[N]; HANDLE ev[N]; BYTE *buf = calloc(N, SZ);
    for (int i = 0; i < N; i++) { ev[i] = mkevent(); }
    for (int i = N - 1; i >= 0; i--) {   /* issue in reverse order */
        ovinit(&ov[i], (ULONGLONG)i * SZ, ev[i]);
        BOOL r = U(ReadFile)(h, buf + (size_t)i * SZ, SZ, NULL, &ov[i]);
        CHECKF(r || GetLastError() == ERROR_IO_PENDING, "issue %d", i);
    }
    DWORD w = WaitForMultipleObjects(N, ev, TRUE, 20000);
    CHECK(w == WAIT_OBJECT_0);
    int bad = 0;
    for (int i = 0; i < N; i++) {
        DWORD n = 0;
        if (!U(GetOverlappedResult)(h, &ov[i], &n, FALSE) || n != SZ || !chkp(buf + (size_t)i * SZ, (size_t)i * SZ, SZ)) bad++;
        CloseHandle(ev[i]);
    }
    EXPECT_EQ(bad, 0);
    free(buf); CloseHandle(h);
}

TEST(aio_file_multi_outstanding_writes_64) {
    REQUIRE(U(WriteFile));
    enum { N = 64, SZ = 65536 };
    char p[MAX_PATH]; mkpath(p, "w64");
    HANDLE h = new_ov_file(p, 0);
    OVERLAPPED ov[N]; HANDLE ev[N]; BYTE *buf = malloc((size_t)N * SZ); fillp(buf, 0, (size_t)N * SZ);
    for (int i = 0; i < N; i++) ev[i] = mkevent();
    for (int i = 0; i < N; i++) {
        ovinit(&ov[i], (ULONGLONG)i * SZ, ev[i]);
        BOOL r = U(WriteFile)(h, buf + (size_t)i * SZ, SZ, NULL, &ov[i]);
        CHECKF(r || GetLastError() == ERROR_IO_PENDING, "issue %d", i);
    }
    CHECK(WaitForMultipleObjects(N, ev, TRUE, 20000) == WAIT_OBJECT_0);
    int bad = 0;
    for (int i = 0; i < N; i++) { DWORD n = 0; if (!GetOverlappedResult(h, &ov[i], &n, FALSE) || n != SZ) bad++; CloseHandle(ev[i]); }
    EXPECT_EQ(bad, 0); CloseHandle(h);
    h = open_sync(p, GENERIC_READ); EXPECT_EQ(GetFileSize(h, NULL), (DWORD)(N * SZ));
    BYTE *r = malloc((size_t)N * SZ); DWORD n; CHECK(ReadFile(h, r, N * SZ, &n, NULL));
    CHECK(n == N * SZ && !memcmp(r, buf, N * SZ));
    free(r); free(buf); CloseHandle(h);
}

TEST(aio_file_read_4mb_single) {
    REQUIRE(U(ReadFile));
    char p[MAX_PATH]; mkpath(p, "4m"); size_t S = 4u << 20; CHECK(make_file(p, S));
    HANDLE h = open_ov(p, GENERIC_READ, 0); HANDLE ev = mkevent(); BYTE *b = calloc(1, S);
    OVERLAPPED ov; ovinit(&ov, 0, ev);
    iores x = do_read(h, b, (DWORD)S, &ov); CHECK(x.ok); EXPECT_EQ(x.n, S); CHECK(chkp(b, 0, S));
    free(b); CloseHandle(ev); CloseHandle(h);
}

TEST(aio_file_write_8mb_single) {
    REQUIRE(U(WriteFile));
    char p[MAX_PATH]; mkpath(p, "8mw"); size_t S = 8u << 20;
    HANDLE h = new_ov_file(p, 0); HANDLE ev = mkevent(); BYTE *b = malloc(S); fillp(b, 0, S);
    OVERLAPPED ov; ovinit(&ov, 0, ev);
    iores x = do_write(h, b, (DWORD)S, &ov); CHECK(x.ok); EXPECT_EQ(x.n, S);
    CloseHandle(h); h = open_sync(p, GENERIC_READ); EXPECT_EQ(GetFileSize(h, NULL), S); CloseHandle(h);
    free(b); CloseHandle(ev);
}

TEST(aio_file_invalid_handles) {
    REQUIRE(U(ReadFile)); REQUIRE(U(WriteFile));
    BYTE b[16] = {0}; OVERLAPPED ov; HANDLE ev = mkevent();
    HANDLE bad[2] = {INVALID_HANDLE_VALUE, NULL};
    for (int i = 0; i < 2; i++) {
        ovinit(&ov, 0, ev); DWORD n = 5; SET_ERR(0);
        CHECK(!U(ReadFile)(bad[i], b, 16, &n, &ov)); EXPECT_ERR(ERROR_INVALID_HANDLE); EXPECT_EQ(n, 0);
        ovinit(&ov, 0, ev); SET_ERR(0);
        CHECK(!U(WriteFile)(bad[i], b, 16, NULL, &ov)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
    /* closed handle */
    char p[MAX_PATH]; mkpath(p, "inv"); CHECK(make_file(p, 100));
    HANDLE h = open_ov(p, GENERIC_READ | GENERIC_WRITE, 0); CloseHandle(h);
    ovinit(&ov, 0, ev); SET_ERR(0);
    CHECK(!U(ReadFile)(h, b, 16, NULL, &ov)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    ovinit(&ov, 0, ev); SET_ERR(0);
    CHECK(!U(WriteFile)(h, b, 16, NULL, &ov)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
}

TEST(aio_file_wrong_access_rights) {
    REQUIRE(U(ReadFile)); REQUIRE(U(WriteFile));
    char p[MAX_PATH]; mkpath(p, "acc"); CHECK(make_file(p, 4096));
    BYTE b[16] = {0}; HANDLE ev = mkevent(); OVERLAPPED ov;
    HANDLE hw = open_ov(p, GENERIC_WRITE, 0);
    ovinit(&ov, 0, ev); SET_ERR(0);
    CHECK(!U(ReadFile)(hw, b, 16, NULL, &ov)); EXPECT_ERR(ERROR_ACCESS_DENIED);
    HANDLE hr = open_ov(p, GENERIC_READ, 0);
    ovinit(&ov, 0, ev); SET_ERR(0);
    CHECK(!U(WriteFile)(hr, b, 16, NULL, &ov)); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(hw); CloseHandle(hr); CloseHandle(ev);
}

TEST(aio_file_sync_handle_with_overlapped_offset) {
    REQUIRE(U(ReadFile));
    char p[MAX_PATH]; mkpath(p, "sync"); CHECK(make_file(p, 8192));
    HANDLE h = open_sync(p, GENERIC_READ); OVERLAPPED ov; ovinit(&ov, 3000, NULL);
    BYTE b[100]; DWORD n = 0; SET_ERR(0);
    BOOL r = U(ReadFile)(h, b, 100, &n, &ov);
    CHECK(r); EXPECT_EQ(n, 100); CHECK(chkp(b, 3000, 100));
    LONG hi = 0; DWORD pos = SetFilePointer(h, 0, &hi, FILE_CURRENT);
    t_note("sync handle + OVERLAPPED: file pointer afterwards = %lu", pos);
    /* EOF on sync handle with OVERLAPPED */
    ovinit(&ov, 8192, NULL); n = 9; SET_ERR(0);
    r = U(ReadFile)(h, b, 100, &n, &ov);
    CHECK(!r); EXPECT_ERR(ERROR_HANDLE_EOF); EXPECT_EQ(n, 0);
    CloseHandle(h);
}

TEST(aio_file_flags_accepted) {
    REQUIRE(U(ReadFile));
    char p[MAX_PATH]; mkpath(p, "flg"); CHECK(make_file(p, 8192));
    static const DWORD fl[] = {FILE_FLAG_SEQUENTIAL_SCAN, FILE_FLAG_RANDOM_ACCESS, FILE_FLAG_WRITE_THROUGH,
        FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_WRITE_THROUGH, FILE_ATTRIBUTE_NORMAL, FILE_FLAG_OVERLAPPED};
    for (unsigned i = 0; i < sizeof fl / sizeof fl[0]; i++) {
        HANDLE h = open_ov(p, GENERIC_READ, fl[i]); CHECKF(h != INVALID_HANDLE_VALUE, "flags %lx", fl[i]);
        if (h == INVALID_HANDLE_VALUE) continue;
        HANDLE ev = mkevent(); OVERLAPPED ov; ovinit(&ov, 100, ev); BYTE b[64];
        iores x = do_read(h, b, 64, &ov); CHECKF(x.ok && x.n == 64 && chkp(b, 100, 64), "flags %lx", fl[i]);
        CloseHandle(ev); CloseHandle(h);
    }
}

TEST(aio_file_bytes_pointer_null_ok) {
    REQUIRE(U(ReadFile)); REQUIRE(U(WriteFile));
    char p[MAX_PATH]; mkpath(p, "nullb"); CHECK(make_file(p, 4096));
    HANDLE h = open_ov(p, GENERIC_READ | GENERIC_WRITE, 0); HANDLE ev = mkevent();
    OVERLAPPED ov; ovinit(&ov, 0, ev); BYTE b[64];
    BOOL r = U(ReadFile)(h, b, 64, NULL, &ov);
    CHECK(r || GetLastError() == ERROR_IO_PENDING);
    DWORD n; GetOverlappedResult(h, &ov, &n, TRUE);
    ovinit(&ov, 0, ev);
    r = U(WriteFile)(h, b, 64, NULL, &ov);
    CHECK(r || GetLastError() == ERROR_IO_PENDING);
    GetOverlappedResult(h, &ov, &n, TRUE);
    CloseHandle(ev); CloseHandle(h);
}

TEST(aio_file_append_offset_minus_one) {
    REQUIRE(U(WriteFile));
    char p[MAX_PATH]; mkpath(p, "app"); CHECK(make_file(p, 1000));
    HANDLE h = CreateFileA(p, FILE_APPEND_DATA | SYNCHRONIZE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    HANDLE ev = mkevent(); BYTE b[100]; memset(b, 0x77, 100);
    OVERLAPPED ov; ovinit(&ov, 0xFFFFFFFFFFFFFFFFULL, ev);
    iores x = do_write(h, b, 100, &ov);
    CHECK(x.ok); EXPECT_EQ(x.n, 100);
    CloseHandle(h);
    h = open_sync(p, GENERIC_READ); EXPECT_EQ(GetFileSize(h, NULL), 1100);
    BYTE r[1100]; DWORD n; CHECK(ReadFile(h, r, 1100, &n, NULL)); CHECK(chkp(r, 0, 1000)); CHECK(!memcmp(r + 1000, b, 100));
    CloseHandle(h); CloseHandle(ev);
}

TEST(aio_file_write_at_negative_offset_note) {
    REQUIRE(U(WriteFile)); REQUIRE(U(ReadFile));
    char p[MAX_PATH]; mkpath(p, "neg"); CHECK(make_file(p, 1000));
    HANDLE h = open_ov(p, GENERIC_READ | GENERIC_WRITE, 0); HANDLE ev = mkevent(); BYTE b[16] = {0};
    OVERLAPPED ov; ovinit(&ov, 0x8000000000000000ULL, ev);
    iores x = do_read(h, b, 16, &ov);
    CHECK(!x.ok); t_note("read at offset 0x8000000000000000: err %lu", x.err);
    ovinit(&ov, 0x8000000000000000ULL, ev);
    x = do_write(h, b, 16, &ov);
    CHECK(!x.ok); t_note("write at offset 0x8000000000000000: err %lu", x.err);
    CloseHandle(ev); CloseHandle(h);
}

TEST(aio_file_nul_device) {
    REQUIRE(U(ReadFile)); REQUIRE(U(WriteFile));
    HANDLE h = CreateFileA("NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    CHECK(h != INVALID_HANDLE_VALUE); if (h == INVALID_HANDLE_VALUE) return;
    HANDLE ev = mkevent(); OVERLAPPED ov; BYTE b[64] = {0};
    ovinit(&ov, 0, ev);
    iores x = do_write(h, b, 64, &ov); CHECK(x.ok); EXPECT_EQ(x.n, 64);
    ovinit(&ov, 0, ev);
    x = do_read(h, b, 64, &ov);
    t_note("NUL overlapped read: ok=%d err=%lu n=%lu", x.ok, x.err, x.n);
    CloseHandle(ev); CloseHandle(h);
}

TEST(aio_file_unbuffered_aligned_read) {
    REQUIRE(U(ReadFile));
    char p[MAX_PATH]; mkpath(p, "ub"); CHECK(make_file(p, 65536));
    HANDLE h = open_ov(p, GENERIC_READ, FILE_FLAG_NO_BUFFERING); CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    BYTE *b = VirtualAlloc(NULL, 65536, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); HANDLE ev = mkevent();
    OVERLAPPED ov; ovinit(&ov, 4096, ev);
    iores x = do_read(h, b, 8192, &ov); CHECK(x.ok); EXPECT_EQ(x.n, 8192); CHECK(chkp(b, 4096, 8192));
    ovinit(&ov, 0, ev);
    x = do_read(h, b, 65536, &ov); CHECK(x.ok); EXPECT_EQ(x.n, 65536); CHECK(chkp(b, 0, 65536));
    VirtualFree(b, 0, MEM_RELEASE); CloseHandle(ev); CloseHandle(h);
}

TEST(aio_file_unbuffered_unaligned_fails) {
    REQUIRE(U(ReadFile));
    char p[MAX_PATH]; mkpath(p, "ubu"); CHECK(make_file(p, 65536));
    HANDLE h = open_ov(p, GENERIC_READ, FILE_FLAG_NO_BUFFERING); if (h == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    BYTE *b = VirtualAlloc(NULL, 65536, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); HANDLE ev = mkevent(); OVERLAPPED ov;
    ovinit(&ov, 1, ev); iores x = do_read(h, b, 4096, &ov);
    CHECK(!x.ok); EXPECT_EQ(x.err, ERROR_INVALID_PARAMETER);   /* offset */
    ovinit(&ov, 0, ev); x = do_read(h, b, 100, &ov);
    CHECK(!x.ok); EXPECT_EQ(x.err, ERROR_INVALID_PARAMETER);    /* length */
    ovinit(&ov, 0, ev); x = do_read(h, b + 1, 4096, &ov);
    CHECK(!x.ok); EXPECT_EQ(x.err, ERROR_INVALID_PARAMETER);    /* buffer */
    VirtualFree(b, 0, MEM_RELEASE); CloseHandle(ev); CloseHandle(h);
}

TEST(aio_file_unbuffered_write_roundtrip) {
    REQUIRE(U(WriteFile)); REQUIRE(U(ReadFile));
    char p[MAX_PATH]; mkpath(p, "ubw");
    HANDLE h = new_ov_file(p, FILE_FLAG_NO_BUFFERING); if (h == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    BYTE *b = VirtualAlloc(NULL, 32768, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); fillp(b, 0, 32768);
    HANDLE ev = mkevent(); OVERLAPPED ov; ovinit(&ov, 0, ev);
    iores x = do_write(h, b, 32768, &ov); CHECK(x.ok); EXPECT_EQ(x.n, 32768);
    BYTE *r = VirtualAlloc(NULL, 32768, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    ovinit(&ov, 0, ev); x = do_read(h, r, 32768, &ov); CHECK(x.ok); CHECK(!memcmp(r, b, 32768));
    ovinit(&ov, 100, ev);   /* unaligned write offset */
    x = do_write(h, b, 4096, &ov); CHECK(!x.ok); EXPECT_EQ(x.err, ERROR_INVALID_PARAMETER);
    VirtualFree(b, 0, MEM_RELEASE); VirtualFree(r, 0, MEM_RELEASE); CloseHandle(ev); CloseHandle(h);
}

TEST(aio_file_sequential_reads_two_handles) {
    REQUIRE(U(ReadFile));
    char p[MAX_PATH]; mkpath(p, "two"); CHECK(make_file(p, 16384));
    HANDLE h1 = open_ov(p, GENERIC_READ, 0), h2 = open_ov(p, GENERIC_READ, 0);
    HANDLE e1 = mkevent(), e2 = mkevent(); OVERLAPPED o1, o2; BYTE b1[100], b2[100];
    ovinit(&o1, 0, e1); ovinit(&o2, 8000, e2);
    BOOL r1 = U(ReadFile)(h1, b1, 100, NULL, &o1), r2 = U(ReadFile)(h2, b2, 100, NULL, &o2);
    CHECK(r1 || GetLastError() == ERROR_IO_PENDING); (void)r2;
    HANDLE ev[2] = {e1, e2}; CHECK(WaitForMultipleObjects(2, ev, TRUE, 5000) == WAIT_OBJECT_0);
    CHECK(chkp(b1, 0, 100)); CHECK(chkp(b2, 8000, 100));
    CloseHandle(e1); CloseHandle(e2); CloseHandle(h1); CloseHandle(h2);
}

TEST(aio_file_gor_bwait_true_completed_no_event) {
    REQUIRE(U(GetOverlappedResult));
    char p[MAX_PATH]; mkpath(p, "gw"); CHECK(make_file(p, 4096));
    HANDLE h = open_ov(p, GENERIC_READ, 0); OVERLAPPED ov; ovinit(&ov, 0, NULL); BYTE b[10];
    BOOL r = U(ReadFile)(h, b, 10, NULL, &ov); CHECK(r || GetLastError() == ERROR_IO_PENDING);
    DWORD n = 0; CHECK(U(GetOverlappedResult)(h, &ov, &n, TRUE)); EXPECT_EQ(n, 10);
    CloseHandle(h);
}
