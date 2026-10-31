/* Anonymous pipes: CreatePipe, PeekNamedPipe on them, handle semantics. */
#include "pmc.h"

#define WR(h, s, n, pw) WriteFile(h, s, n, pw, NULL)

TEST(pmc_anon_basic_roundtrip) {
    REQUIRE(U(CreatePipe));
    HANDLE r = NULL, w = NULL; DWORD n = 0; char buf[16] = {0};
    CHECK(U(CreatePipe)(&r, &w, NULL, 0));
    CHECK(r != NULL && r != INVALID_HANDLE_VALUE && w != NULL && w != INVALID_HANDLE_VALUE && r != w);
    CHECK(WR(w, "hello", 5, &n)); EXPECT_EQ(n, 5);
    CHECK(ReadFile(r, buf, sizeof buf, &n, NULL)); EXPECT_EQ(n, 5);
    CHECK(!memcmp(buf, "hello", 5));
    CloseHandle(r); CloseHandle(w);
}
TEST(pmc_anon_null_attrs_not_inheritable) {
    REQUIRE(U(CreatePipe));
    HANDLE r, w; DWORD f = 0xff;
    CHECK(U(CreatePipe)(&r, &w, NULL, 0));
    CHECK(GetHandleInformation(r, &f)); CHECK(!(f & HANDLE_FLAG_INHERIT));
    f = 0xff; CHECK(GetHandleInformation(w, &f)); CHECK(!(f & HANDLE_FLAG_INHERIT));
    CloseHandle(r); CloseHandle(w);
}
TEST(pmc_anon_inherit_true) {
    REQUIRE(U(CreatePipe));
    HANDLE r, w; DWORD f = 0; SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    CHECK(U(CreatePipe)(&r, &w, &sa, 0));
    CHECK(GetHandleInformation(r, &f)); CHECK(f & HANDLE_FLAG_INHERIT);
    f = 0; CHECK(GetHandleInformation(w, &f)); CHECK(f & HANDLE_FLAG_INHERIT);
    CloseHandle(r); CloseHandle(w);
}
TEST(pmc_anon_inherit_false_explicit) {
    REQUIRE(U(CreatePipe));
    HANDLE r, w; DWORD f = 0xff; SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, FALSE };
    CHECK(U(CreatePipe)(&r, &w, &sa, 0));
    CHECK(GetHandleInformation(r, &f)); CHECK(!(f & HANDLE_FLAG_INHERIT));
    CloseHandle(r); CloseHandle(w);
}
TEST(pmc_anon_toggle_inherit_flag) {
    REQUIRE(U(CreatePipe));
    HANDLE r, w; DWORD f;
    CHECK(U(CreatePipe)(&r, &w, NULL, 0));
    CHECK(SetHandleInformation(r, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT));
    CHECK(GetHandleInformation(r, &f)); CHECK(f & HANDLE_FLAG_INHERIT);
    CHECK(SetHandleInformation(r, HANDLE_FLAG_INHERIT, 0));
    CHECK(GetHandleInformation(r, &f)); CHECK(!(f & HANDLE_FLAG_INHERIT));
    CloseHandle(r); CloseHandle(w);
}
TEST(pmc_anon_sizes) {
    REQUIRE(U(CreatePipe));
    DWORD sizes[] = { 0, 1, 2, 64, 512, 4096, 65536, 1 << 20 };
    for (int i = 0; i < 8; i++) {
        HANDLE r = NULL, w = NULL; DWORD n; char b[4];
        CHECKF(U(CreatePipe)(&r, &w, NULL, sizes[i]), "size %lu err %lu", sizes[i], GetLastError());
        if (!r) continue;
        CHECK(WR(w, "abc", 3, &n)); EXPECT_EQ(n, 3);
        CHECK(ReadFile(r, b, 4, &n, NULL)); EXPECT_EQ(n, 3);
        CloseHandle(r); CloseHandle(w);
    }
}
TEST(pmc_anon_filetype_pipe) {
    REQUIRE(U(CreatePipe));
    HANDLE r, w;
    CHECK(U(CreatePipe)(&r, &w, NULL, 0));
    EXPECT_EQ(GetFileType(r), FILE_TYPE_PIPE);
    EXPECT_EQ(GetFileType(w), FILE_TYPE_PIPE);
    CloseHandle(r); CloseHandle(w);
}
TEST(pmc_anon_filetype_after_close) {
    REQUIRE(U(CreatePipe));
    HANDLE r, w;
    CHECK(U(CreatePipe)(&r, &w, NULL, 0));
    CloseHandle(r); CloseHandle(w);
    SET_ERR(0);
    EXPECT_EQ(GetFileType(r), FILE_TYPE_UNKNOWN);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(pmc_anon_write_to_read_end) {
    REQUIRE(U(CreatePipe));
    HANDLE r, w; DWORD n = 77;
    CHECK(U(CreatePipe)(&r, &w, NULL, 0));
    SET_ERR(0);
    CHECK(!WR(r, "x", 1, &n));
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(r); CloseHandle(w);
}
TEST(pmc_anon_read_from_write_end) {
    REQUIRE(U(CreatePipe));
    HANDLE r, w; DWORD n = 77; char b[4];
    CHECK(U(CreatePipe)(&r, &w, NULL, 0));
    SET_ERR(0);
    CHECK(!ReadFile(w, b, 1, &n, NULL));
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(r); CloseHandle(w);
}
TEST(pmc_anon_read_after_writer_closed_empty) {
    REQUIRE(U(CreatePipe));
    HANDLE r, w; DWORD n = 77; char b[8];
    CHECK(U(CreatePipe)(&r, &w, NULL, 0));
    CloseHandle(w);
    SET_ERR(0);
    CHECK(!ReadFile(r, b, sizeof b, &n, NULL));
    EXPECT_ERR(ERROR_BROKEN_PIPE);
    EXPECT_EQ(n, 0);
    CloseHandle(r);
}
TEST(pmc_anon_read_data_then_broken) {
    REQUIRE(U(CreatePipe));
    HANDLE r, w; DWORD n; char b[8];
    CHECK(U(CreatePipe)(&r, &w, NULL, 0));
    CHECK(WR(w, "abcdef", 6, &n));
    CloseHandle(w);
    CHECK(ReadFile(r, b, 8, &n, NULL)); EXPECT_EQ(n, 6);
    SET_ERR(0);
    CHECK(!ReadFile(r, b, 8, &n, NULL)); EXPECT_ERR(ERROR_BROKEN_PIPE);
    SET_ERR(0);
    CHECK(!ReadFile(r, b, 8, &n, NULL)); EXPECT_ERR(ERROR_BROKEN_PIPE);   /* repeatable */
    CloseHandle(r);
}
TEST(pmc_anon_write_after_reader_closed) {
    REQUIRE(U(CreatePipe));
    HANDLE r, w; DWORD n = 77;
    CHECK(U(CreatePipe)(&r, &w, NULL, 0));
    CloseHandle(r);
    SET_ERR(0);
    CHECK(!WR(w, "x", 1, &n));
    CHECK_ERR_ONE_OF2(ERROR_BROKEN_PIPE, ERROR_NO_DATA);
    CloseHandle(w);
}
TEST(pmc_anon_zero_byte_write) {
    REQUIRE(U(CreatePipe));
    HANDLE r, w; DWORD n = 77, a = 99;
    CHECK(U(CreatePipe)(&r, &w, NULL, 0));
    CHECK(WR(w, "", 0, &n)); EXPECT_EQ(n, 0);
    CHECK(U(PeekNamedPipe)(r, NULL, 0, NULL, &a, NULL)); EXPECT_EQ(a, 0);
    CHECK(WR(w, "z", 1, &n));
    CHECK(U(PeekNamedPipe)(r, NULL, 0, NULL, &a, NULL)); EXPECT_EQ(a, 1);
    CloseHandle(r); CloseHandle(w);
}
TEST(pmc_anon_partial_reads) {
    REQUIRE(U(CreatePipe));
    HANDLE r, w; DWORD n; char b[16];
    CHECK(U(CreatePipe)(&r, &w, NULL, 0));
    CHECK(WR(w, "0123456789", 10, &n));
    CHECK(ReadFile(r, b, 4, &n, NULL)); EXPECT_EQ(n, 4); CHECK(!memcmp(b, "0123", 4));
    CHECK(ReadFile(r, b, 3, &n, NULL)); EXPECT_EQ(n, 3); CHECK(!memcmp(b, "456", 3));
    CHECK(ReadFile(r, b, 16, &n, NULL)); EXPECT_EQ(n, 3); CHECK(!memcmp(b, "789", 3));
    CloseHandle(r); CloseHandle(w);
}
TEST(pmc_anon_byte_stream_coalesces) {
    REQUIRE(U(CreatePipe));
    HANDLE r, w; DWORD n; char b[32];
    CHECK(U(CreatePipe)(&r, &w, NULL, 0));
    CHECK(WR(w, "aa", 2, &n)); CHECK(WR(w, "bbb", 3, &n)); CHECK(WR(w, "c", 1, &n));
    CHECK(ReadFile(r, b, 32, &n, NULL)); EXPECT_EQ(n, 6); CHECK(!memcmp(b, "aabbbc", 6));
    CloseHandle(r); CloseHandle(w);
}
TEST(pmc_anon_peek_empty) {
    REQUIRE(U(CreatePipe));
    HANDLE r, w; DWORD rd = 55, av = 55, left = 55; char b[8];
    CHECK(U(CreatePipe)(&r, &w, NULL, 0));
    CHECK(U(PeekNamedPipe)(r, b, sizeof b, &rd, &av, &left));
    EXPECT_EQ(rd, 0); EXPECT_EQ(av, 0);
    CloseHandle(r); CloseHandle(w);
}
TEST(pmc_anon_peek_does_not_consume) {
    REQUIRE(U(CreatePipe));
    HANDLE r, w; DWORD rd = 0, av = 0, n; char b[8] = {0};
    CHECK(U(CreatePipe)(&r, &w, NULL, 0));
    CHECK(WR(w, "peekme", 6, &n));
    for (int i = 0; i < 3; i++) {
        memset(b, 0, sizeof b); rd = av = 0;
        CHECK(U(PeekNamedPipe)(r, b, 4, &rd, &av, NULL));
        EXPECT_EQ(rd, 4); EXPECT_EQ(av, 6); CHECK(!memcmp(b, "peek", 4));
    }
    CHECK(ReadFile(r, b, 8, &n, NULL)); EXPECT_EQ(n, 6);
    CloseHandle(r); CloseHandle(w);
}
TEST(pmc_anon_peek_buffer_larger_than_data) {
    REQUIRE(U(CreatePipe));
    HANDLE r, w; DWORD rd = 0, av = 0, n; char b[64];
    CHECK(U(CreatePipe)(&r, &w, NULL, 0));
    CHECK(WR(w, "abc", 3, &n));
    CHECK(U(PeekNamedPipe)(r, b, sizeof b, &rd, &av, NULL));
    EXPECT_EQ(rd, 3); EXPECT_EQ(av, 3);
    CloseHandle(r); CloseHandle(w);
}
TEST(pmc_anon_peek_null_buffer_and_outputs) {
    REQUIRE(U(CreatePipe));
    HANDLE r, w; DWORD av = 0, n;
    CHECK(U(CreatePipe)(&r, &w, NULL, 0));
    CHECK(WR(w, "abcd", 4, &n));
    CHECK(U(PeekNamedPipe)(r, NULL, 0, NULL, &av, NULL)); EXPECT_EQ(av, 4);
    CHECK(U(PeekNamedPipe)(r, NULL, 0, NULL, NULL, NULL));
    CloseHandle(r); CloseHandle(w);
}
TEST(pmc_anon_peek_zero_size_with_buffer) {
    REQUIRE(U(CreatePipe));
    HANDLE r, w; DWORD rd = 9, av = 0, n; char b[4];
    CHECK(U(CreatePipe)(&r, &w, NULL, 0));
    CHECK(WR(w, "abcd", 4, &n));
    CHECK(U(PeekNamedPipe)(r, b, 0, &rd, &av, NULL)); EXPECT_EQ(rd, 0); EXPECT_EQ(av, 4);
    CloseHandle(r); CloseHandle(w);
}
TEST(pmc_anon_peek_after_writer_closed) {
    REQUIRE(U(CreatePipe));
    HANDLE r, w; DWORD av = 0, n; char b[4];
    CHECK(U(CreatePipe)(&r, &w, NULL, 0));
    CHECK(WR(w, "xy", 2, &n));
    CloseHandle(w);
    CHECK(U(PeekNamedPipe)(r, b, 4, &n, &av, NULL)); EXPECT_EQ(av, 2);
    CHECK(ReadFile(r, b, 4, &n, NULL));
    SET_ERR(0);
    CHECK(!U(PeekNamedPipe)(r, b, 4, &n, &av, NULL)); EXPECT_ERR(ERROR_BROKEN_PIPE);
    CloseHandle(r);
}
TEST(pmc_anon_peek_invalid_handles) {
    REQUIRE(U(PeekNamedPipe));
    DWORD av;
    SET_ERR(0); CHECK(!U(PeekNamedPipe)(INVALID_HANDLE_VALUE, NULL, 0, NULL, &av, NULL)); CHECK_ERR_ONE_OF2(ERROR_INVALID_HANDLE, ERROR_INVALID_FUNCTION);
    SET_ERR(0); CHECK(!U(PeekNamedPipe)(NULL, NULL, 0, NULL, &av, NULL)); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(pmc_anon_peek_on_file_handle) {
    REQUIRE(U(PeekNamedPipe));
    char p[MAX_PATH]; DWORD av;
    t_tmpfile(p, sizeof p, "pmc_peek.bin");
    HANDLE h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    SET_ERR(0); CHECK(!U(PeekNamedPipe)(h, NULL, 0, NULL, &av, NULL));
    CHECK_ERR_ONE_OF3(ERROR_INVALID_FUNCTION, ERROR_INVALID_HANDLE, ERROR_INVALID_PARAMETER);
    CloseHandle(h); DeleteFileA(p);
}
TEST(pmc_anon_peek_on_write_end) {
    REQUIRE(U(CreatePipe));
    HANDLE r, w; DWORD av = 0;
    CHECK(U(CreatePipe)(&r, &w, NULL, 0));
    SET_ERR(0);
    BOOL ok = U(PeekNamedPipe)(w, NULL, 0, NULL, &av, NULL);
    if (!ok) t_note("PeekNamedPipe on write end failed err=%lu", GetLastError());
    else t_note("PeekNamedPipe on write end succeeded avail=%lu", av);
    CloseHandle(r); CloseHandle(w);
}
TEST(pmc_anon_dup_write_end_keeps_pipe_open) {
    REQUIRE(U(CreatePipe));
    HANDLE r, w, w2; DWORD n; char b[8];
    CHECK(U(CreatePipe)(&r, &w, NULL, 0));
    CHECK(DuplicateHandle(GetCurrentProcess(), w, GetCurrentProcess(), &w2, 0, FALSE, DUPLICATE_SAME_ACCESS));
    CloseHandle(w);
    CHECK(WR(w2, "dup", 3, &n));
    CHECK(ReadFile(r, b, 8, &n, NULL)); EXPECT_EQ(n, 3);
    CloseHandle(w2);
    SET_ERR(0);
    CHECK(!ReadFile(r, b, 8, &n, NULL)); EXPECT_ERR(ERROR_BROKEN_PIPE);
    CloseHandle(r);
}
TEST(pmc_anon_dup_read_end) {
    REQUIRE(U(CreatePipe));
    HANDLE r, w, r2; DWORD n; char b[8];
    CHECK(U(CreatePipe)(&r, &w, NULL, 0));
    CHECK(DuplicateHandle(GetCurrentProcess(), r, GetCurrentProcess(), &r2, 0, FALSE, DUPLICATE_SAME_ACCESS));
    CloseHandle(r);
    CHECK(WR(w, "q", 1, &n)); EXPECT_EQ(n, 1);
    CHECK(ReadFile(r2, b, 8, &n, NULL)); EXPECT_EQ(n, 1);
    CloseHandle(r2); CloseHandle(w);
}
TEST(pmc_anon_dup_restricted_access_read_only) {
    REQUIRE(U(CreatePipe));
    HANDLE r, w, w2; DWORD n = 5;
    CHECK(U(CreatePipe)(&r, &w, NULL, 0));
    CHECK(DuplicateHandle(GetCurrentProcess(), w, GetCurrentProcess(), &w2, SYNCHRONIZE, FALSE, 0));
    SET_ERR(0);
    CHECK(!WR(w2, "x", 1, &n)); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(w2); CloseHandle(r); CloseHandle(w);
}
TEST(pmc_anon_nowait_read_empty) {
    REQUIRE(U(CreatePipe)); REQUIRE(U(SetNamedPipeHandleState));
    HANDLE r, w; DWORD mode = PIPE_NOWAIT, n = 5, st = 0; char b[4];
    CHECK(U(CreatePipe)(&r, &w, NULL, 0));
    CHECK(U(SetNamedPipeHandleState)(r, &mode, NULL, NULL));
    SET_ERR(0);
    CHECK(!ReadFile(r, b, 4, &n, NULL)); EXPECT_ERR(ERROR_NO_DATA);
    CHECK(U(GetNamedPipeHandleStateA)(r, &st, NULL, NULL, NULL, NULL, 0));
    CHECK(st & PIPE_NOWAIT);
    CloseHandle(r); CloseHandle(w);
}
TEST(pmc_anon_handlestate_default) {
    REQUIRE(U(CreatePipe)); REQUIRE(U(GetNamedPipeHandleStateA));
    HANDLE r, w; DWORD st = 99, cur = 99;
    CHECK(U(CreatePipe)(&r, &w, NULL, 0));
    CHECK(U(GetNamedPipeHandleStateA)(r, &st, &cur, NULL, NULL, NULL, 0));
    EXPECT_EQ(st, 0);
    EXPECT_EQ(cur, 1);
    CloseHandle(r); CloseHandle(w);
}
TEST(pmc_anon_getnamedpipeinfo_ends) {
    REQUIRE(U(CreatePipe)); REQUIRE(U(GetNamedPipeInfo));
    HANDLE r, w; DWORD fl = 0xff, ob = 0, ib = 0, mx = 0;
    CHECK(U(CreatePipe)(&r, &w, NULL, 0x2000));
    CHECK(U(GetNamedPipeInfo)(r, &fl, &ob, &ib, &mx));
    CHECK(fl & PIPE_SERVER_END); CHECK(!(fl & PIPE_TYPE_MESSAGE));
    EXPECT_EQ(mx, 1);
    t_note("anon read end sizes out=%lu in=%lu", ob, ib);
    fl = 0xff;
    CHECK(U(GetNamedPipeInfo)(w, &fl, NULL, NULL, NULL));
    CHECK(!(fl & PIPE_SERVER_END));
    CloseHandle(r); CloseHandle(w);
}
TEST(pmc_anon_getnamedpipeinfo_all_null) {
    REQUIRE(U(CreatePipe)); REQUIRE(U(GetNamedPipeInfo));
    HANDLE r, w;
    CHECK(U(CreatePipe)(&r, &w, NULL, 0));
    CHECK(U(GetNamedPipeInfo)(r, NULL, NULL, NULL, NULL));
    CloseHandle(r); CloseHandle(w);
}
TEST(pmc_anon_getnamedpipeinfo_invalid) {
    REQUIRE(U(GetNamedPipeInfo));
    DWORD fl;
    SET_ERR(0); CHECK(!U(GetNamedPipeInfo)(NULL, &fl, NULL, NULL, NULL)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "pmc_gnpi.bin");
    HANDLE h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    SET_ERR(0); CHECK(!U(GetNamedPipeInfo)(h, &fl, NULL, NULL, NULL));
    CHECK_ERR_ONE_OF3(ERROR_INVALID_FUNCTION, ERROR_INVALID_HANDLE, ERROR_INVALID_PARAMETER);
    CloseHandle(h); DeleteFileA(p);
}
TEST(pmc_anon_handlestate_on_file_fails) {
    REQUIRE(U(GetNamedPipeHandleStateA));
    char p[MAX_PATH]; DWORD st;
    t_tmpfile(p, sizeof p, "pmc_gnphs.bin");
    HANDLE h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    SET_ERR(0); CHECK(!U(GetNamedPipeHandleStateA)(h, &st, NULL, NULL, NULL, NULL, 0));
    CHECK(GetLastError() != 0);
    CloseHandle(h); DeleteFileA(p);
}
TEST(pmc_anon_handlestate_w_all_null_any_handle) {
    /* kernelbase decomp: with every optional pointer NULL no query is made and TRUE is returned */
    REQUIRE(U(GetNamedPipeHandleStateW));
    BOOL ok = U(GetNamedPipeHandleStateW)(INVALID_HANDLE_VALUE, NULL, NULL, NULL, NULL, NULL, 0);
    t_note("GetNamedPipeHandleStateW(INVALID, all NULL) = %d", ok);
}
TEST(pmc_anon_handlestate_invalid_handle) {
    REQUIRE(U(GetNamedPipeHandleStateW));
    DWORD st;
    SET_ERR(0); CHECK(!U(GetNamedPipeHandleStateW)(NULL, &st, NULL, NULL, NULL, NULL, 0)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(GetNamedPipeHandleStateW)(INVALID_HANDLE_VALUE, &st, NULL, NULL, NULL, NULL, 0));
    CHECK(GetLastError() != 0);
}
TEST(pmc_anon_transact_fails) {
    REQUIRE(U(CreatePipe)); REQUIRE(U(TransactNamedPipe));
    HANDLE r, w; char o[4], i[4]; DWORD n;
    CHECK(U(CreatePipe)(&r, &w, NULL, 0));
    SET_ERR(0);
    CHECK(!U(TransactNamedPipe)(w, o, 4, i, 4, &n, NULL));
    CHECK(GetLastError() != 0);
    CloseHandle(r); CloseHandle(w);
}

/* ---- threads ---- */
typedef struct { HANDLE h; size_t total; DWORD seed; DWORD sum; DWORD ok; } xfer_t;
static DWORD WINAPI writer_thr(void *p) {
    xfer_t *x = p; unsigned char *buf = malloc(65536); size_t left = x->total; DWORD pos = 0;
    while (left) {
        size_t c = left > 65536 ? 65536 : left; DWORD n;
        for (size_t i = 0; i < c; i++) buf[i] = (unsigned char)((pos + i) * 31 + x->seed);
        if (!WriteFile(x->h, buf, (DWORD)c, &n, NULL) || n != c) { x->ok = 0; break; }
        pos += (DWORD)c; left -= c; x->ok = 1;
    }
    free(buf);
    return 0;
}
TEST(pmc_anon_large_transfer_small_buffer) {
    REQUIRE(U(CreatePipe));
    HANDLE r, w; xfer_t x = { 0 }; unsigned char *buf = malloc(8192); DWORD n, sum = 0, exp = 0; size_t got = 0;
    CHECK(U(CreatePipe)(&r, &w, NULL, 4096));
    x.h = w; x.total = 1 << 20; x.seed = 7;
    HANDLE th = pmc_thread(writer_thr, &x);
    unsigned char *ref = malloc(x.total);
    for (size_t i = 0; i < x.total; i++) ref[i] = (unsigned char)(i * 31 + 7);
    exp = pmc_sum(ref, x.total, 0);
    while (ReadFile(r, buf, 8192, &n, NULL) && n) { sum = pmc_sum(buf, n, got ? sum : 0); got += n; if (got >= x.total) break; }
    EXPECT_EQ(got, x.total); EXPECT_EQ(sum, exp);
    pmc_join(th); CloseHandle(w); CloseHandle(r); free(buf); free(ref);
}
TEST(pmc_anon_large_transfer_8mb) {
    REQUIRE(U(CreatePipe));
    HANDLE r, w; xfer_t x = { 0 }; unsigned char *buf = malloc(65536); DWORD n, sum = 0; size_t got = 0;
    CHECK(U(CreatePipe)(&r, &w, NULL, 0));
    x.h = w; x.total = 8u << 20; x.seed = 3;
    HANDLE th = pmc_thread(writer_thr, &x);
    while (got < x.total && ReadFile(r, buf, 65536, &n, NULL) && n) { sum = pmc_sum(buf, n, got ? sum : 0); got += n; }
    unsigned char *ref = malloc(x.total);
    for (size_t i = 0; i < x.total; i++) ref[i] = (unsigned char)(i * 31 + 3);
    EXPECT_EQ(got, x.total); EXPECT_EQ(sum, pmc_sum(ref, x.total, 0));
    pmc_join(th); CloseHandle(w); CloseHandle(r); free(buf); free(ref);
}
static DWORD WINAPI delayed_writer(void *p) { Sleep(150); DWORD n; WriteFile((HANDLE)p, "late", 4, &n, NULL); return 0; }
TEST(pmc_anon_poll_timeout_then_data) {
    REQUIRE(U(CreatePipe));
    HANDLE r, w; DWORD av = 0; DWORD t0 = GetTickCount(); int polls = 0;
    CHECK(U(CreatePipe)(&r, &w, NULL, 0));
    /* nothing there for a while */
    for (int i = 0; i < 5; i++) { CHECK(U(PeekNamedPipe)(r, NULL, 0, NULL, &av, NULL)); EXPECT_EQ(av, 0); Sleep(5); }
    HANDLE th = pmc_thread(delayed_writer, w);
    while (GetTickCount() - t0 < 5000) { polls++; CHECK(U(PeekNamedPipe)(r, NULL, 0, NULL, &av, NULL)); if (av) break; Sleep(10); }
    EXPECT_EQ(av, 4);
    CHECK(polls > 1);
    pmc_join(th); CloseHandle(r); CloseHandle(w);
}
static DWORD WINAPI closer_thr(void *p) { Sleep(100); CloseHandle((HANDLE)p); return 0; }
TEST(pmc_anon_blocked_read_released_by_writer_close) {
    REQUIRE(U(CreatePipe));
    HANDLE r, w; DWORD n = 5; char b[4];
    CHECK(U(CreatePipe)(&r, &w, NULL, 0));
    HANDLE th = pmc_thread(closer_thr, w);
    SET_ERR(0);
    CHECK(!ReadFile(r, b, 4, &n, NULL)); EXPECT_ERR(ERROR_BROKEN_PIPE);
    pmc_join(th); CloseHandle(r);
}
TEST(pmc_anon_many_pairs) {
    REQUIRE(U(CreatePipe));
    HANDLE r[100], w[100]; int ok = 0;
    for (int i = 0; i < 100; i++) { if (U(CreatePipe)(&r[i], &w[i], NULL, 0)) ok++; else { r[i] = w[i] = NULL; } }
    EXPECT_EQ(ok, 100);
    for (int i = 0; i < 100; i++) { DWORD n; char c = (char)i, d = 0; if (!r[i]) continue; WriteFile(w[i], &c, 1, &n, NULL); ReadFile(r[i], &d, 1, &n, NULL); CHECK(d == c); CloseHandle(r[i]); CloseHandle(w[i]); }
}
TEST(pmc_anon_repeat_create_close) {
    REQUIRE(U(CreatePipe));
    for (int i = 0; i < 500; i++) { HANDLE r, w; if (!U(CreatePipe)(&r, &w, NULL, 0)) { CHECK(0); break; } CloseHandle(r); CloseHandle(w); }
}
TEST(pmc_anon_security_descriptor_null_dacl) {
    REQUIRE(U(CreatePipe));
    SECURITY_DESCRIPTOR sd; SECURITY_ATTRIBUTES sa = { sizeof sa, &sd, FALSE };
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
    HANDLE r, w; DWORD n; char b[4];
    CHECK(U(CreatePipe)(&r, &w, &sa, 0));
    CHECK(WR(w, "ok", 2, &n)); CHECK(ReadFile(r, b, 4, &n, NULL)); EXPECT_EQ(n, 2);
    CloseHandle(r); CloseHandle(w);
}
TEST(pmc_anon_overlapped_flag_absent) {
    /* CreatePipe handles are synchronous: asking for FILE_FLAG_OVERLAPPED-style behaviour is impossible; a
       read on an empty pipe with a writer alive must block, so verify via PeekNamedPipe only. */
    REQUIRE(U(CreatePipe));
    HANDLE r, w; DWORD av = 1;
    CHECK(U(CreatePipe)(&r, &w, NULL, 0));
    CHECK(U(PeekNamedPipe)(r, NULL, 0, NULL, &av, NULL)); EXPECT_EQ(av, 0);
    CloseHandle(r); CloseHandle(w);
}
TEST(pmc_anon_closehandle_twice) {
    REQUIRE(U(CreatePipe));
    HANDLE r, w;
    CHECK(U(CreatePipe)(&r, &w, NULL, 0));
    CHECK(CloseHandle(r));
    /* second close of a stale handle value reports an error (no debugger attached) */
    HANDLE dup = NULL;
    SET_ERR(0);
    CHECK(!DuplicateHandle(GetCurrentProcess(), r, GetCurrentProcess(), &dup, 0, FALSE, DUPLICATE_SAME_ACCESS));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(w);
}
