/* Synchronous ReadFile / WriteFile on a disk file with protected user buffers. */
#define UB_TU "f"
#include "ub.h"

typedef struct { BOOL r; DWORD e; DWORD n; LONGLONG pos; } xres;
static xres sread(HANDLE h, void *b, DWORD len) {
    xres x; x.n = 0x5A5A5A5A; SetLastError(0xBEEF);
    x.r = U(ReadFile)(h, b, len, &x.n, NULL); x.e = x.r ? 0 : GetLastError(); x.pos = ub_pos(h); return x;
}
static xres swrite(HANDLE h, const void *b, DWORD len) {
    xres x; x.n = 0x5A5A5A5A; SetLastError(0xBEEF);
    x.r = U(WriteFile)(h, b, len, &x.n, NULL); x.e = x.r ? 0 : GetLastError(); x.pos = ub_pos(h); return x;
}

/* Common body: an 8192-byte cached file, pointer at 100, one read of len into b. */
static void read_bad_common(BYTE *b, DWORD len, DWORD flags) {
    char p[MAX_PATH]; ub_path(p, "rd"); CHECK(ub_mkfile(p, 8192));
    HANDLE h = ub_open(p, GENERIC_READ, flags); CHECK(h != INVALID_HANDLE_VALUE);
    LONGLONG start = (flags & FILE_FLAG_NO_BUFFERING) ? 4096 : 100;
    ub_seek(h, start);
    xres x = sread(h, b, len);
    CHECK(!x.r); EXPECT_EQ(x.e, ERROR_NOACCESS); EXPECT_EQ(x.n, 0); EXPECT_EQ(x.pos, start);
    /* the same handle still reads normally afterwards */
    BYTE *ok = ub_va(PG, PAGE_READWRITE);
    x = sread(h, ok, 4096);
    CHECK(x.r); EXPECT_EQ(x.n, 4096); CHECK(ub_chk(ok, (size_t)start, 4096)); EXPECT_EQ(x.pos, start + 4096);
    ub_free(ok); closeh(&h); ub_rm(p);
}

TEST(xubuf_read_readonly_buffer_noaccess_error) {
    REQUIRE(U(ReadFile));
    BYTE *b = ub_pages(1, PAGE_READONLY, 0xCC);
    read_bad_common(b, 4096, 0);
    EXPECT_EQ(ub_prot(b), PAGE_READONLY); CHECK(ub_all(b, PG, 0xCC));
    ub_free(b);
}

TEST(xubuf_read_noaccess_buffer_noaccess_error) {
    REQUIRE(U(ReadFile));
    BYTE *b = ub_pages(1, PAGE_NOACCESS, 0xCC);
    read_bad_common(b, 4096, 0);
    ub_protect(b, PG, PAGE_READONLY); CHECK(ub_all(b, PG, 0xCC));
    ub_free(b);
}

TEST(xubuf_read_reserved_buffer_noaccess_error) {
    REQUIRE(U(ReadFile));
    BYTE *b = ub_reserve(PG);
    read_bad_common(b, 4096, 0);
    EXPECT_EQ(ub_state(b), MEM_RESERVE);
    ub_free(b);
}

TEST(xubuf_read_small_len_noaccess_error) {
    REQUIRE(U(ReadFile));
    BYTE *b = ub_pages(1, PAGE_NOACCESS, 0xCC);
    read_bad_common(b + 100, 1, 0);
    ub_free(b);
}

TEST(xubuf_read_span_noaccess_cached_no_transfer) {
    REQUIRE(U(ReadFile));
    BYTE *b = ub_span(PAGE_NOACCESS, 0xCC);
    read_bad_common(b, 8192, 0);
    CHECK(ub_all(b, PG, 0xCC));
    ub_free(b);
}

TEST(xubuf_read_span_unaligned_tail_cached) {
    REQUIRE(U(ReadFile));
    BYTE *b = ub_span(PAGE_NOACCESS, 0xCC);
    read_bad_common(b + PG - 100, 101, 0);
    CHECK(ub_all(b, PG, 0xCC));
    ub_free(b);
}

TEST(xubuf_read_span_readonly_second_page_cached) {
    REQUIRE(U(ReadFile));
    BYTE *b = ub_span(PAGE_READONLY, 0xCC);
    read_bad_common(b, 8192, 0);
    CHECK(ub_all(b, 2 * PG, 0xCC));
    ub_free(b);
}

TEST(xubuf_read_span_reserved_second_page_cached) {
    REQUIRE(U(ReadFile));
    BYTE *b = ub_span(0, 0xCC);
    read_bad_common(b, 8192, 0);
    CHECK(ub_all(b, PG, 0xCC)); EXPECT_EQ(ub_state(b + PG), MEM_RESERVE);
    ub_free(b);
}

/* The buffer length is validated, not the bytes the file can supply: a file of
 * 1000 bytes read into a 8192-byte buffer whose second page is inaccessible. */
TEST(xubuf_read_span_short_file_len_decides) {
    REQUIRE(U(ReadFile));
    char p[MAX_PATH]; ub_path(p, "short"); CHECK(ub_mkfile(p, 1000));
    HANDLE h = ub_open(p, GENERIC_READ, 0);
    BYTE *b = ub_span(PAGE_NOACCESS, 0xCC);
    xres x = sread(h, b, 8192);
    CHECK(!x.r); EXPECT_EQ(x.e, ERROR_NOACCESS); EXPECT_EQ(x.n, 0); EXPECT_EQ(x.pos, 0);
    CHECK(ub_all(b, PG, 0xCC));
    x = sread(h, b, 4096);
    CHECK(x.r); EXPECT_EQ(x.n, 1000); CHECK(ub_chk(b, 0, 1000)); EXPECT_EQ(b[1000], 0xCC);
    ub_free(b); closeh(&h); ub_rm(p);
}

/* At end of file the buffer is still validated first. */
TEST(xubuf_read_at_eof_noaccess_error_first) {
    REQUIRE(U(ReadFile));
    char p[MAX_PATH]; ub_path(p, "eof"); CHECK(ub_mkfile(p, 4096));
    HANDLE h = ub_open(p, GENERIC_READ, 0); ub_seek(h, 4096);
    BYTE *b = ub_pages(1, PAGE_NOACCESS, 0);
    xres x = sread(h, b, 4096);
    CHECK(!x.r); EXPECT_EQ(x.e, ERROR_NOACCESS); EXPECT_EQ(x.n, 0); EXPECT_EQ(x.pos, 4096);
    ub_free(b); closeh(&h); ub_rm(p);
}

TEST(xubuf_read_zero_length_noaccess_ok) {
    REQUIRE(U(ReadFile));
    char p[MAX_PATH]; ub_path(p, "zero"); CHECK(ub_mkfile(p, 4096));
    HANDLE h = ub_open(p, GENERIC_READ, 0); ub_seek(h, 10);
    BYTE *b = ub_pages(1, PAGE_NOACCESS, 0);
    xres x = sread(h, b, 0);
    CHECK(x.r); EXPECT_EQ(x.n, 0); EXPECT_EQ(x.pos, 10);
    BYTE *r = ub_reserve(PG);
    x = sread(h, r, 0);
    CHECK(x.r); EXPECT_EQ(x.n, 0); EXPECT_EQ(x.pos, 10);
    ub_free(r); ub_free(b); closeh(&h); ub_rm(p);
}

TEST(xubuf_read_nobuffering_span_noaccess) {
    REQUIRE(U(ReadFile));
    BYTE *b = ub_span(PAGE_NOACCESS, 0xCC);
    read_bad_common(b, 8192, FILE_FLAG_NO_BUFFERING);
    CHECK(ub_all(b, PG, 0xCC));
    ub_free(b);
}

TEST(xubuf_read_nobuffering_readonly_buffer) {
    REQUIRE(U(ReadFile));
    BYTE *b = ub_pages(1, PAGE_READONLY, 0xCC);
    read_bad_common(b, 4096, FILE_FLAG_NO_BUFFERING);
    CHECK(ub_all(b, PG, 0xCC));
    ub_free(b);
}

TEST(xubuf_read_nobuffering_reserved_buffer) {
    REQUIRE(U(ReadFile));
    BYTE *b = ub_reserve(2 * PG);
    read_bad_common(b, 8192, FILE_FLAG_NO_BUFFERING);
    ub_free(b);
}

/* PAGE_GUARD: the read fails with the guard-page status as last error, the guard
 * is consumed by the failing call, the retry succeeds. */
TEST(xubuf_read_guard_page_consumed_retry_ok) {
    REQUIRE(U(ReadFile));
    char p[MAX_PATH]; ub_path(p, "guard"); CHECK(ub_mkfile(p, 8192));
    HANDLE h = ub_open(p, GENERIC_READ, 0); ub_seek(h, 64);
    BYTE *b = ub_va(PG, PAGE_READWRITE | PAGE_GUARD);
    xres x = sread(h, b, 4096);
    CHECK(!x.r); EXPECT_EQ(x.e, GLE_GUARD); EXPECT_EQ(x.n, 0); EXPECT_EQ(x.pos, 64);
    EXPECT_EQ(ub_prot(b), PAGE_READWRITE);
    x = sread(h, b, 4096);
    CHECK(x.r); EXPECT_EQ(x.n, 4096); CHECK(ub_chk(b, 64, 4096)); EXPECT_EQ(x.pos, 64 + 4096);
    ub_free(b); closeh(&h); ub_rm(p);
}

TEST(xubuf_read_guard_second_page_span) {
    REQUIRE(U(ReadFile));
    char p[MAX_PATH]; ub_path(p, "guard2"); CHECK(ub_mkfile(p, 8192));
    HANDLE h = ub_open(p, GENERIC_READ, 0);
    BYTE *b = ub_span(PAGE_READWRITE | PAGE_GUARD, 0xCC);
    xres x = sread(h, b, 8192);
    CHECK(!x.r); EXPECT_EQ(x.e, GLE_GUARD); EXPECT_EQ(x.n, 0); EXPECT_EQ(x.pos, 0);
    EXPECT_EQ(ub_prot(b + PG), PAGE_READWRITE);
    CHECK(ub_all(b, PG, 0xCC));
    x = sread(h, b, 8192);
    CHECK(x.r); EXPECT_EQ(x.n, 8192); CHECK(ub_chk(b, 0, 8192));
    ub_free(b); closeh(&h); ub_rm(p);
}

TEST(xubuf_read_guard_nobuffering_consumed) {
    REQUIRE(U(ReadFile));
    char p[MAX_PATH]; ub_path(p, "guardnb"); CHECK(ub_mkfile(p, 8192));
    HANDLE h = ub_open(p, GENERIC_READ, FILE_FLAG_NO_BUFFERING);
    BYTE *b = ub_va(PG, PAGE_READWRITE | PAGE_GUARD);
    xres x = sread(h, b, 4096);
    CHECK(!x.r); EXPECT_EQ(x.e, GLE_GUARD); EXPECT_EQ(x.pos, 0);
    EXPECT_EQ(ub_prot(b), PAGE_READWRITE);
    x = sread(h, b, 4096);
    CHECK(x.r); EXPECT_EQ(x.n, 4096); CHECK(ub_chk(b, 0, 4096));
    ub_free(b); closeh(&h); ub_rm(p);
}

/* ---- writes ----
 * The source buffer of a disk-file write is not probed up front: the file system
 * faults on it and reports ERROR_INVALID_USER_BUFFER (1784), not ERROR_NOACCESS. */
static void write_bad_common(const BYTE *b, DWORD len, DWORD flags, DWORD want_err) {
    char p[MAX_PATH]; ub_path(p, "wr"); CHECK(ub_mkfile(p, 8192));
    HANDLE h = ub_open(p, GENERIC_READ | GENERIC_WRITE, flags); CHECK(h != INVALID_HANDLE_VALUE);
    ub_seek(h, 4096);
    xres x = swrite(h, b, len);
    CHECK(!x.r); EXPECT_EQ(x.e, want_err); EXPECT_EQ(x.n, 0); EXPECT_EQ(x.pos, 4096);
    EXPECT_EQ(ub_size(h), 8192);
    closeh(&h);
    CHECK(ub_file_pat(p, 0, 8192));
    ub_rm(p);
}

TEST(xubuf_write_noaccess_buffer_size_unchanged) {
    REQUIRE(U(WriteFile));
    BYTE *b = ub_pages(1, PAGE_NOACCESS, 0x11);
    write_bad_common(b, 4096, 0, ERROR_INVALID_USER_BUFFER);
    ub_free(b);
}

TEST(xubuf_write_reserved_buffer_size_unchanged) {
    REQUIRE(U(WriteFile));
    BYTE *b = ub_reserve(PG);
    write_bad_common(b, 4096, 0, ERROR_INVALID_USER_BUFFER);
    ub_free(b);
}

/* Appending from a no-access buffer does not grow the file. */
TEST(xubuf_write_noaccess_append_no_growth) {
    REQUIRE(U(WriteFile));
    char p[MAX_PATH]; ub_path(p, "app"); CHECK(ub_mkfile(p, 100));
    HANDLE h = ub_open(p, GENERIC_READ | GENERIC_WRITE, 0); ub_seek(h, 100);
    BYTE *b = ub_pages(1, PAGE_NOACCESS, 0);
    xres x = swrite(h, b, 4000);
    CHECK(!x.r); EXPECT_EQ(x.e, ERROR_INVALID_USER_BUFFER); EXPECT_EQ(x.n, 0); EXPECT_EQ(x.pos, 100);
    EXPECT_EQ(ub_size(h), 100);
    ub_free(b); closeh(&h); ub_rm(p);
}

TEST(xubuf_write_readonly_buffer_succeeds) {
    REQUIRE(U(WriteFile));
    char p[MAX_PATH]; ub_path(p, "wro"); CHECK(ub_mkfile(p, 0));
    HANDLE h = ub_open(p, GENERIC_READ | GENERIC_WRITE, 0);
    BYTE *b = ub_va(2 * PG, PAGE_READWRITE); ub_fill(b, 0, 2 * PG); ub_protect(b, 2 * PG, PAGE_READONLY);
    xres x = swrite(h, b, 2 * PG);
    CHECK(x.r); EXPECT_EQ(x.n, 2 * PG); EXPECT_EQ(x.pos, 2 * PG); EXPECT_EQ(ub_size(h), 2 * PG);
    closeh(&h); CHECK(ub_file_pat(p, 0, 2 * PG));
    ub_free(b); ub_rm(p);
}

TEST(xubuf_write_nobuffering_span_noaccess) {
    REQUIRE(U(WriteFile));
    BYTE *b = ub_span(PAGE_NOACCESS, 0x11);
    write_bad_common(b, 8192, FILE_FLAG_NO_BUFFERING, ERROR_INVALID_USER_BUFFER);
    ub_free(b);
}

TEST(xubuf_write_nobuffering_noaccess) {
    REQUIRE(U(WriteFile));
    BYTE *b = ub_pages(1, PAGE_NOACCESS, 0x11);
    write_bad_common(b, 4096, FILE_FLAG_NO_BUFFERING, ERROR_INVALID_USER_BUFFER);
    ub_free(b);
}

TEST(xubuf_write_zero_length_noaccess_ok) {
    REQUIRE(U(WriteFile));
    char p[MAX_PATH]; ub_path(p, "wz"); CHECK(ub_mkfile(p, 50));
    HANDLE h = ub_open(p, GENERIC_READ | GENERIC_WRITE, 0); ub_seek(h, 50);
    BYTE *b = ub_pages(1, PAGE_NOACCESS, 0);
    xres x = swrite(h, b, 0);
    CHECK(x.r); EXPECT_EQ(x.n, 0); EXPECT_EQ(x.pos, 50); EXPECT_EQ(ub_size(h), 50);
    ub_free(b); closeh(&h); ub_rm(p);
}

/* Cached write whose source runs into a no-access page: result, byte count,
 * file size and content pinned from Windows 10. */
TEST(xubuf_write_span_noaccess_cached) {
    REQUIRE(U(WriteFile));
    char p[MAX_PATH]; ub_path(p, "wspan"); CHECK(ub_mkfile(p, 0));
    HANDLE h = ub_open(p, GENERIC_READ | GENERIC_WRITE, 0);
    BYTE *b = ub_span(PAGE_NOACCESS, 0); ub_fill(b, 0, PG);
    xres x = swrite(h, b, 2 * PG);
    CHECK(!x.r); EXPECT_EQ(x.e, ERROR_INVALID_USER_BUFFER); EXPECT_EQ(x.n, 0); EXPECT_EQ(x.pos, 0);
    EXPECT_EQ(ub_size(h), 0);
    ub_free(b); closeh(&h); ub_rm(p);
}

TEST(xubuf_write_guard_page_consumed_retry_ok) {
    REQUIRE(U(WriteFile));
    char p[MAX_PATH]; ub_path(p, "wguard"); CHECK(ub_mkfile(p, 0));
    HANDLE h = ub_open(p, GENERIC_READ | GENERIC_WRITE, 0);
    BYTE *b = ub_va(PG, PAGE_READWRITE); ub_fill(b, 0, PG); ub_protect(b, PG, PAGE_READWRITE | PAGE_GUARD);
    xres x = swrite(h, b, PG);
    CHECK(!x.r); EXPECT_EQ(x.e, ERROR_INVALID_USER_BUFFER); EXPECT_EQ(x.n, 0); EXPECT_EQ(ub_size(h), 0);
    EXPECT_EQ(ub_prot(b), PAGE_READWRITE);
    ub_seek(h, 0);
    x = swrite(h, b, PG);
    CHECK(x.r); EXPECT_EQ(x.n, PG); EXPECT_EQ(ub_size(h), PG);
    closeh(&h); CHECK(ub_file_pat(p, 0, PG));
    ub_free(b); ub_rm(p);
}
