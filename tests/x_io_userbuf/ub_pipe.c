/* Anonymous and named pipes with protected user buffers: ReadFile, WriteFile,
 * PeekNamedPipe, TransactNamedPipe, CallNamedPipeA. */
#define UB_TU "p"
#include "ub.h"

static const char k_msg[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!?";

typedef struct { BOOL r; DWORD e; DWORD n; } pres;
static pres pread_sync(HANDLE h, void *b, DWORD len) {
    pres x; x.n = 0x5A5A5A5A; SetLastError(0xBEEF);
    x.r = U(ReadFile)(h, b, len, &x.n, NULL); x.e = x.r ? 0 : GetLastError(); return x;
}
static pres pwrite_sync(HANDLE h, const void *b, DWORD len) {
    pres x; x.n = 0x5A5A5A5A; SetLastError(0xBEEF);
    x.r = U(WriteFile)(h, b, len, &x.n, NULL); x.e = x.r ? 0 : GetLastError(); return x;
}

/* A failed read into bad memory consumes nothing: a later valid read returns all of it. */
static void anon_read_bad(BYTE *bad, DWORD len, DWORD want) {
    HANDLE r, w; DWORD n; BYTE got[128];
    CHECK(CreatePipe(&r, &w, NULL, 0));
    CHECK(WriteFile(w, k_msg, 64, &n, NULL) && n == 64);
    pres x = pread_sync(r, bad, len);
    CHECK(!x.r); EXPECT_EQ(x.e, want); EXPECT_EQ(x.n, 0);
    EXPECT_EQ(ub_avail(r), 64);
    memset(got, 0, sizeof got);
    x = pread_sync(r, got, sizeof got);
    CHECK(x.r); EXPECT_EQ(x.n, 64); CHECK(!memcmp(got, k_msg, 64));
    CloseHandle(r); CloseHandle(w);
}

TEST(xubuf_anon_read_noaccess_keeps_data) {
    REQUIRE(U(ReadFile));
    BYTE *b = ub_pages(1, PAGE_NOACCESS, 0);
    anon_read_bad(b, 64, ERROR_NOACCESS);
    ub_free(b);
}

TEST(xubuf_anon_read_readonly_keeps_data) {
    REQUIRE(U(ReadFile));
    BYTE *b = ub_pages(1, PAGE_READONLY, 0xCC);
    anon_read_bad(b, 64, ERROR_NOACCESS);
    CHECK(ub_all(b, PG, 0xCC));
    ub_free(b);
}

TEST(xubuf_anon_read_span_keeps_data) {
    REQUIRE(U(ReadFile));
    BYTE *b = ub_span(PAGE_NOACCESS, 0xCC);
    anon_read_bad(b + PG - 32, 64, ERROR_NOACCESS);
    CHECK(ub_all(b, PG, 0xCC));
    ub_free(b);
}

TEST(xubuf_anon_read_guard_keeps_data_and_consumes_guard) {
    REQUIRE(U(ReadFile));
    BYTE *b = ub_va(PG, PAGE_READWRITE | PAGE_GUARD);
    anon_read_bad(b, 64, GLE_GUARD);
    EXPECT_EQ(ub_prot(b), PAGE_READWRITE);
    ub_free(b);
}

TEST(xubuf_anon_write_noaccess_nothing_queued) {
    REQUIRE(U(WriteFile));
    HANDLE r, w; CHECK(CreatePipe(&r, &w, NULL, 0));
    BYTE *b = ub_pages(1, PAGE_NOACCESS, 0);
    pres x = pwrite_sync(w, b, 64);
    CHECK(!x.r); EXPECT_EQ(x.e, ERROR_NOACCESS); EXPECT_EQ(x.n, 0);
    EXPECT_EQ(ub_avail(r), 0);
    /* the pipe still works */
    DWORD n; CHECK(WriteFile(w, k_msg, 10, &n, NULL)); EXPECT_EQ(ub_avail(r), 10);
    ub_free(b); CloseHandle(r); CloseHandle(w);
}

TEST(xubuf_anon_write_readonly_ok) {
    REQUIRE(U(WriteFile));
    HANDLE r, w; CHECK(CreatePipe(&r, &w, NULL, 0));
    BYTE *b = ub_va(PG, PAGE_READWRITE); memcpy(b, k_msg, 64); ub_protect(b, PG, PAGE_READONLY);
    pres x = pwrite_sync(w, b, 64);
    CHECK(x.r); EXPECT_EQ(x.n, 64); EXPECT_EQ(ub_avail(r), 64);
    BYTE got[64]; DWORD n = 0; CHECK(ReadFile(r, got, 64, &n, NULL) && n == 64); CHECK(!memcmp(got, k_msg, 64));
    ub_free(b); CloseHandle(r); CloseHandle(w);
}

/* Named byte pipe, synchronous client. */
TEST(xubuf_npipe_byte_read_noaccess_keeps_data) {
    REQUIRE(U(ReadFile));
    HANDLE s, c; CHECK(ub_npipe(&s, &c, FALSE, FALSE, NULL));
    CHECK(ub_pwrite(s, k_msg, 40));
    BYTE *b = ub_pages(1, PAGE_NOACCESS, 0);
    pres x = pread_sync(c, b, 40);
    CHECK(!x.r); EXPECT_EQ(x.e, ERROR_NOACCESS); EXPECT_EQ(x.n, 0);
    EXPECT_EQ(ub_avail(c), 40);
    BYTE got[64]; x = pread_sync(c, got, 64);
    CHECK(x.r); EXPECT_EQ(x.n, 40); CHECK(!memcmp(got, k_msg, 40));
    ub_free(b); closeh(&c); closeh(&s);
}

/* Message pipe: the failed read neither consumes nor splits the first message. */
TEST(xubuf_npipe_msg_read_noaccess_keeps_message) {
    REQUIRE(U(ReadFile));
    HANDLE s, c; CHECK(ub_npipe(&s, &c, TRUE, FALSE, NULL));
    CHECK(ub_pwrite(s, k_msg, 30)); CHECK(ub_pwrite(s, k_msg + 30, 20));
    BYTE *b = ub_pages(1, PAGE_NOACCESS, 0);
    pres x = pread_sync(c, b, 64);
    CHECK(!x.r); EXPECT_EQ(x.e, ERROR_NOACCESS); EXPECT_EQ(x.n, 0);
    EXPECT_EQ(ub_avail(c), 50); EXPECT_EQ(ub_msgleft(c), 30);
    BYTE got[64]; memset(got, 0, sizeof got);
    x = pread_sync(c, got, 64);
    CHECK(x.r); EXPECT_EQ(x.n, 30); CHECK(!memcmp(got, k_msg, 30));
    x = pread_sync(c, got, 64);
    CHECK(x.r); EXPECT_EQ(x.n, 20); CHECK(!memcmp(got, k_msg + 30, 20));
    ub_free(b); closeh(&c); closeh(&s);
}

/* A buffer shorter than the message in valid memory followed by no-access memory:
 * the probe covers the requested length, so nothing is read (no ERROR_MORE_DATA split). */
TEST(xubuf_npipe_msg_read_span_no_partial_message) {
    REQUIRE(U(ReadFile));
    HANDLE s, c; CHECK(ub_npipe(&s, &c, TRUE, FALSE, NULL));
    CHECK(ub_pwrite(s, k_msg, 30));
    BYTE *b = ub_span(PAGE_NOACCESS, 0xCC);
    pres x = pread_sync(c, b + PG - 10, 30);
    CHECK(!x.r); EXPECT_EQ(x.e, ERROR_NOACCESS); EXPECT_EQ(x.n, 0);
    CHECK(ub_all(b, PG, 0xCC));
    EXPECT_EQ(ub_msgleft(c), 30);
    ub_free(b); closeh(&c); closeh(&s);
}

TEST(xubuf_npipe_msg_write_noaccess_no_message) {
    REQUIRE(U(WriteFile));
    HANDLE s, c; CHECK(ub_npipe(&s, &c, TRUE, FALSE, NULL));
    BYTE *b = ub_pages(1, PAGE_NOACCESS, 0);
    pres x = pwrite_sync(c, b, 30);
    CHECK(!x.r); EXPECT_EQ(x.e, ERROR_NOACCESS); EXPECT_EQ(x.n, 0);
    EXPECT_EQ(ub_avail(s), 0);
    CHECK(ub_pwrite(c, k_msg, 5)); EXPECT_EQ(ub_msgleft(s), 5);
    ub_free(b); closeh(&c); closeh(&s);
}

TEST(xubuf_npipe_write_span_noaccess_nothing_written) {
    REQUIRE(U(WriteFile));
    HANDLE s, c; CHECK(ub_npipe(&s, &c, FALSE, FALSE, NULL));
    BYTE *b = ub_span(PAGE_NOACCESS, 'x');
    pres x = pwrite_sync(c, b + PG - 8, 16);
    CHECK(!x.r); EXPECT_EQ(x.e, ERROR_NOACCESS); EXPECT_EQ(x.n, 0);
    EXPECT_EQ(ub_avail(s), 0);
    ub_free(b); closeh(&c); closeh(&s);
}

/* ---- PeekNamedPipe in children (copy into the caller's buffer may fault in user mode) ---- */
#define R_KEEP 0x00800000u
static DWORD peek_child(BYTE *buf, DWORD size, DWORD *avail_ptr, DWORD *read_ptr, BOOL data) {
    HANDLE r, w; DWORD n;
    if (!CreatePipe(&r, &w, NULL, 0)) return R_SETUP;
    if (data && !(WriteFile(w, k_msg, 64, &n, NULL) && n == 64)) return R_SETUP;
    SetLastError(0xBEEF);
    BOOL ok = U(PeekNamedPipe)(r, buf, size, read_ptr, avail_ptr, NULL);
    DWORD e = GetLastError(), left = 0;
    PeekNamedPipe(r, NULL, 0, NULL, &left, NULL);
    return (ok ? R_OK(0) : R_ERR(e)) | (left == (data ? 64u : 0u) ? R_KEEP : 0);
}
CHILD(xubuf_c_peek_noaccess) { ub_child_init(); DWORD a = 0, n = 0; return (int)peek_child(ub_pages(1, PAGE_NOACCESS, 0), 64, &a, &n, TRUE); }
CHILD(xubuf_c_peek_readonly) { ub_child_init(); DWORD a = 0, n = 0; return (int)peek_child(ub_pages(1, PAGE_READONLY, 0), 64, &a, &n, TRUE); }
CHILD(xubuf_c_peek_zero_noaccess) { ub_child_init(); DWORD a = 0, n = 0; return (int)peek_child(ub_pages(1, PAGE_NOACCESS, 0), 0, &a, &n, TRUE); }
CHILD(xubuf_c_peek_empty_noaccess) { ub_child_init(); DWORD a = 0, n = 0; return (int)peek_child(ub_pages(1, PAGE_NOACCESS, 0), 64, &a, &n, FALSE); }
CHILD(xubuf_c_peek_avail_readonly) {
    ub_child_init(); BYTE buf[64]; DWORD n = 0;
    DWORD *a = (DWORD *)ub_pages(1, PAGE_READONLY, 0);
    return (int)peek_child(buf, 64, a, &n, TRUE);
}
CHILD(xubuf_c_peek_read_readonly) {
    ub_child_init(); BYTE buf[64]; DWORD a = 0;
    DWORD *n = (DWORD *)ub_pages(1, PAGE_READONLY, 0);
    return (int)peek_child(buf, 64, &a, n, TRUE);
}

TEST(xubuf_peek_noaccess_buffer_with_data) {
    REQUIRE(U(PeekNamedPipe));
    EXPECT_EQ(ub_child("xubuf_c_peek_noaccess", ""), R_ERR(ERROR_NOACCESS) | R_KEEP);
}
TEST(xubuf_peek_readonly_buffer_with_data) {
    REQUIRE(U(PeekNamedPipe));
    EXPECT_EQ(ub_child("xubuf_c_peek_readonly", ""), R_ERR(ERROR_NOACCESS) | R_KEEP);
}
TEST(xubuf_peek_zero_size_noaccess_buffer) {
    REQUIRE(U(PeekNamedPipe));
    EXPECT_EQ(ub_child("xubuf_c_peek_zero_noaccess", ""), R_OK(0) | R_KEEP);
}
TEST(xubuf_peek_empty_pipe_noaccess_buffer) {
    REQUIRE(U(PeekNamedPipe));
    EXPECT_EQ(ub_child("xubuf_c_peek_empty_noaccess", ""), R_OK(0) | R_KEEP);
}
/* The count out-parameters are written inside a guarded region: an error, not a fault. */
TEST(xubuf_peek_total_avail_readonly) {
    REQUIRE(U(PeekNamedPipe));
    EXPECT_EQ(ub_child("xubuf_c_peek_avail_readonly", ""), R_ERR(ERROR_NOACCESS) | R_KEEP);
}
TEST(xubuf_peek_bytes_read_readonly) {
    REQUIRE(U(PeekNamedPipe));
    EXPECT_EQ(ub_child("xubuf_c_peek_read_readonly", ""), R_ERR(ERROR_NOACCESS) | R_KEEP);
}

/* ---- TransactNamedPipe (overlapped client, bounded) ---- */
typedef struct { BOOL r; DWORD e; BOOL pending; } tres;
static tres transact(HANDLE c, void *in, DWORD nin, void *out, DWORD nout, OVERLAPPED *ov) {
    tres x; SetLastError(0xBEEF);
    x.r = U(TransactNamedPipe)(c, in, nin, out, nout, NULL, ov);
    x.e = x.r ? 0 : GetLastError(); x.pending = !x.r && x.e == ERROR_IO_PENDING;
    return x;
}
/* After a failed transaction: nothing reached the server, and a valid transaction works. */
static void transact_after(HANDLE s, HANDLE c) {
    EXPECT_EQ(ub_avail(s), 0);
    HANDLE ev = ub_event(FALSE); OVERLAPPED ov; ub_ov(&ov, 0, ev);
    BYTE out[64]; memset(out, 0, sizeof out);
    tres x = transact(c, (void *)k_msg, 10, out, sizeof out, &ov);
    CHECK(x.pending);
    BYTE req[64]; DWORD e = 0;
    EXPECT_EQ(ub_pread(s, req, sizeof req, &e), 10); CHECK(!memcmp(req, k_msg, 10));
    CHECK(ub_pwrite(s, k_msg + 10, 7));
    DWORD n = 0; BOOL done = GetOverlappedResultEx(c, &ov, &n, 5000, FALSE);
    CHECK(done); EXPECT_EQ(n, 7); CHECK(!memcmp(out, k_msg + 10, 7));
    if (x.pending && !done) { CancelIoEx(c, &ov); GetOverlappedResultEx(c, &ov, &n, 5000, FALSE); }
    CloseHandle(ev);
}

/* The transaction goes pending and completes with STATUS_ACCESS_VIOLATION; no request
 * reaches the server and the pipe stays usable. */
static void transact_bad(BYTE *in, BYTE *out, DWORD want) {
    HANDLE s, c; CHECK(ub_npipe(&s, &c, TRUE, TRUE, NULL));
    HANDLE ev = ub_event(TRUE); OVERLAPPED ov; ub_ov(&ov, 0, ev);
    tres x = transact(c, in ? in : (BYTE *)k_msg, 10, out, 64, &ov);
    CHECK(!x.r); EXPECT_EQ(x.e, ERROR_IO_PENDING);
    DWORD n = 0x5A5A5A5A; SetLastError(0);
    BOOL g = GetOverlappedResultEx(c, &ov, &n, 5000, FALSE); DWORD ge = g ? 0 : GetLastError();
    if (!g && ge == WAIT_TIMEOUT) { CancelIoEx(c, &ov); GetOverlappedResultEx(c, &ov, &n, 5000, FALSE); }
    CHECK(!g); EXPECT_EQ(ge, want); EXPECT_EQ(n, 0);
    EXPECT_EQ((DWORD)ov.Internal, 0xC0000005u);
    transact_after(s, c);
    CloseHandle(ev); closeh(&c); closeh(&s);
}

TEST(xubuf_transact_noaccess_out_nothing_sent) {
    REQUIRE(U(TransactNamedPipe));
    BYTE *b = ub_pages(1, PAGE_NOACCESS, 0);
    transact_bad(NULL, b, ERROR_NOACCESS);
    ub_free(b);
}
TEST(xubuf_transact_readonly_out_nothing_sent) {
    REQUIRE(U(TransactNamedPipe));
    BYTE *b = ub_pages(1, PAGE_READONLY, 0);
    transact_bad(NULL, b, ERROR_NOACCESS);
    ub_free(b);
}
TEST(xubuf_transact_noaccess_in_nothing_sent) {
    REQUIRE(U(TransactNamedPipe));
    BYTE *b = ub_pages(1, PAGE_NOACCESS, 0); BYTE out[64];
    transact_bad(b, out, ERROR_NOACCESS);
    ub_free(b);
}

/* ---- CallNamedPipeA (worker thread, bounded) ---- */
typedef struct { const char *name; void *out; DWORD nout; BOOL r; DWORD e; DWORD n; } cnp;
static DWORD WINAPI cnp_thread(void *p) {
    cnp *a = p; a->n = 0x5A5A5A5A; SetLastError(0xBEEF);
    a->r = U(CallNamedPipeA)((LPSTR)a->name, (void *)k_msg, 10, a->out, a->nout, &a->n, 2000);
    a->e = a->r ? 0 : GetLastError();
    return 0;
}
static void callnp_bad(BYTE *out) {
    char name[160]; ub_pipename(name, sizeof name);
    HANDLE s = CreateNamedPipeA(name, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 1, 4096, 4096, 0, NULL);
    CHECK(s != INVALID_HANDLE_VALUE);
    HANDLE ev = ub_event(FALSE); OVERLAPPED ov; ub_ov(&ov, 0, ev);
    BOOL cr = ConnectNamedPipe(s, &ov); CHECK(!cr && GetLastError() == ERROR_IO_PENDING);
    cnp a; memset(&a, 0, sizeof a); a.name = name; a.out = out; a.nout = 64;
    HANDLE th = CreateThread(NULL, 0, cnp_thread, &a, 0, NULL);
    DWORD w = WaitForSingleObject(th, 4000);
    int served = 0;
    if (w == WAIT_TIMEOUT) {
        /* the call wrote its request and waits for a reply: answer it to unblock */
        BYTE req[64]; DWORD e; ub_pread(s, req, sizeof req, &e); ub_pwrite(s, k_msg, 5); served = 1;
        w = WaitForSingleObject(th, 5000);
    }
    if (w != WAIT_OBJECT_0) { CloseHandle(s); s = NULL; w = WaitForSingleObject(th, 10000) == WAIT_OBJECT_0 ? WAIT_TIMEOUT : WAIT_FAILED; }
    CHECK(!served); EXPECT_EQ(w, WAIT_OBJECT_0);
    CHECK(!a.r); EXPECT_EQ(a.e, ERROR_NOACCESS);
    CHECK(ub_signaled(ev));           /* the client did connect */
    /* the client handle is closed again; no request message was left behind */
    DWORD avail = 0x77; SetLastError(0);
    if (s) {
        BOOL pk = PeekNamedPipe(s, NULL, 0, NULL, &avail, NULL);
        CHECK(!pk); EXPECT_ERR(ERROR_BROKEN_PIPE);
    }
    CloseHandle(th); CloseHandle(ev); if (s) CloseHandle(s);
}

TEST(xubuf_callnamedpipe_noaccess_out) {
    REQUIRE(U(CallNamedPipeA));
    BYTE *b = ub_pages(1, PAGE_NOACCESS, 0);
    callnp_bad(b);
    ub_free(b);
}
TEST(xubuf_callnamedpipe_readonly_out) {
    REQUIRE(U(CallNamedPipeA));
    BYTE *b = ub_pages(1, PAGE_READONLY, 0);
    callnp_bad(b);
    ub_free(b);
}
