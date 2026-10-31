/* Extra pipe / mailslot edge cases: cancellation, large messages through small quotas, post-disconnect I/O. */
#include "pmc.h"

#define PIPE_NAME(n) char n[128]; pmc_pipename(n, sizeof n)

typedef struct { HANDLE h; DWORD ok; DWORD err; DWORD n; } blk;

static DWORD WINAPI blocked_read(void *p) {
    blk *b = p; char buf[8]; SetLastError(0);
    b->ok = ReadFile(b->h, buf, sizeof buf, &b->n, NULL); b->err = GetLastError();
    return 0;
}
static DWORD WINAPI blocked_connect(void *p) {
    blk *b = p; SetLastError(0);
    b->ok = ConnectNamedPipe(b->h, NULL); b->err = GetLastError();
    return 0;
}
/* repeat CancelSynchronousIo until it hits the (not yet blocked) thread */
static BOOL cancel_sync(HANDLE th) {
    __typeof__(&CancelSynchronousIo) f = U(CancelSynchronousIo);
    if (!f) return FALSE;
    for (int i = 0; i < 100; i++) { Sleep(20); if (f(th)) return TRUE; }
    return FALSE;
}

TEST(pmc_extra_cancel_sync_read_anon) {
    REQUIRE(U(CancelSynchronousIo)); REQUIRE(U(CreatePipe));
    HANDLE r, w; CHECK(U(CreatePipe)(&r, &w, NULL, 0));
    blk b = { r, 7, 0, 0 };
    HANDLE th = pmc_thread(blocked_read, &b);
    BOOL c = cancel_sync(th);
    CHECK(c);
    if (!c) CloseHandle(w);   /* release the reader on failure */
    pmc_join(th);
    CHECK(!b.ok);
    CHECK(b.err == ERROR_OPERATION_ABORTED || b.err == ERROR_CANCELLED);
    CloseHandle(r); if (c) CloseHandle(w);
}
TEST(pmc_extra_cancel_sync_connect) {
    REQUIRE(U(CancelSynchronousIo)); REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(ConnectNamedPipe));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    blk b = { s, 7, 0, 0 };
    HANDLE th = pmc_thread(blocked_connect, &b);
    BOOL c = cancel_sync(th);
    CHECK(c);
    if (!c) { HANDLE x = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0); CloseHandle(x); }
    pmc_join(th);
    CHECK(!b.ok);
    CHECK(b.err == ERROR_OPERATION_ABORTED || b.err == ERROR_CANCELLED);
    CloseHandle(s);
}
TEST(pmc_extra_cancel_sync_no_io_pending) {
    REQUIRE(U(CancelSynchronousIo));
    /* current thread has no synchronous I/O in flight (docs: ERROR_NOT_FOUND) */
    HANDLE th = OpenThread(THREAD_TERMINATE, FALSE, GetCurrentThreadId());
    CHECK(th != NULL);
    if (th) { SET_ERR(0); CHECK(!U(CancelSynchronousIo)(th)); EXPECT_ERR(ERROR_NOT_FOUND); CloseHandle(th); }
}
TEST(pmc_extra_cancel_sync_invalid_handle) {
    REQUIRE(U(CancelSynchronousIo));
    SET_ERR(0); CHECK(!U(CancelSynchronousIo)(NULL)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(CancelSynchronousIo)(INVALID_HANDLE_VALUE)); EXPECT_ERR(ERROR_INVALID_HANDLE);
}

TEST(pmc_extra_cancel_overlapped_connect) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(ConnectNamedPipe)); REQUIRE(U(CancelIoEx)); REQUIRE(U(GetOverlappedResult));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    OVERLAPPED ov = {0}; ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0); CHECK(!U(ConnectNamedPipe)(s, &ov)); EXPECT_ERR(ERROR_IO_PENDING);
    CHECK(U(CancelIoEx)(s, &ov));
    DWORD n = 0; SET_ERR(0);
    CHECK(!U(GetOverlappedResult)(s, &ov, &n, TRUE)); EXPECT_ERR(ERROR_OPERATION_ABORTED);
    /* the pipe is still usable: a fresh connect completes */
    ResetEvent(ov.hEvent);
    SET_ERR(0); CHECK(!U(ConnectNamedPipe)(s, &ov)); EXPECT_ERR(ERROR_IO_PENDING);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    CHECK(c != INVALID_HANDLE_VALUE);
    CHECK(WaitForSingleObject(ov.hEvent, 5000) == WAIT_OBJECT_0);
    CHECK(U(GetOverlappedResult)(s, &ov, &n, FALSE));
    if (c != INVALID_HANDLE_VALUE) CloseHandle(c);
    CloseHandle(ov.hEvent); CloseHandle(s);
}
TEST(pmc_extra_overlapped_resultex) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(GetOverlappedResultEx));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    OVERLAPPED ov = {0}; ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    char b[8]; DWORD n = 0;
    SET_ERR(0); CHECK(!ReadFile(s, b, 8, NULL, &ov)); EXPECT_ERR(ERROR_IO_PENDING);
    /* still pending: zero-wait gives I/O incomplete, a short wait times out */
    SET_ERR(0); CHECK(!U(GetOverlappedResultEx)(s, &ov, &n, 0, FALSE)); CHECK(GetLastError() == ERROR_IO_INCOMPLETE || GetLastError() == WAIT_TIMEOUT);
    SET_ERR(0); CHECK(!U(GetOverlappedResultEx)(s, &ov, &n, 50, FALSE)); EXPECT_ERR(WAIT_TIMEOUT);
    DWORD w; CHECK(WriteFile(c, "ex", 2, &w, NULL));
    CHECK(U(GetOverlappedResultEx)(s, &ov, &n, 5000, FALSE)); EXPECT_EQ(n, 2); CHECK(!memcmp(b, "ex", 2));
    CloseHandle(ov.hEvent); CloseHandle(c); CloseHandle(s);
}

typedef struct { HANDLE h; DWORD len; DWORD ok; DWORD err; DWORD wrote; } bigw;
static DWORD WINAPI big_writer(void *p) {
    bigw *x = p; unsigned char *b = malloc(x->len);
    if (!b) { x->ok = 0; return 1; }
    pmc_fill(b, x->len, 99);
    x->ok = WriteFile(x->h, b, x->len, &x->wrote, NULL); x->err = GetLastError();
    free(b); return 0;
}
TEST(pmc_extra_message_larger_than_quota) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    DWORD mode = PIPE_READMODE_MESSAGE; CHECK(SetNamedPipeHandleState(c, &mode, NULL, NULL));
    bigw x = { s, 10000, 0, 0, 0 };
    HANDLE th = pmc_thread(big_writer, &x);
    unsigned char *rb = malloc(10000), *exp = malloc(10000); DWORD got = 0, n = 0; int more = 0, done = 0;
    pmc_fill(exp, 10000, 99);
    DWORD t0 = GetTickCount();
    while (got < 10000 && GetTickCount() - t0 < 10000) {
        BOOL ok = ReadFile(c, rb + got, 1000, &n, NULL);
        got += n;
        if (ok) { done = 1; break; }
        if (GetLastError() != ERROR_MORE_DATA) break;
        more++;
    }
    pmc_join(th);
    CHECK(done); EXPECT_EQ(got, 10000); EXPECT_EQ(more, 9);
    CHECK(!memcmp(rb, exp, 10000));
    CHECK(x.ok); EXPECT_EQ(x.wrote, 10000);
    free(rb); free(exp); CloseHandle(c); CloseHandle(s);
}
TEST(pmc_extra_client_write_after_disconnect) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(ConnectNamedPipe)); REQUIRE(U(DisconnectNamedPipe));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    CHECK(U(DisconnectNamedPipe)(s));
    DWORD n = 55; SET_ERR(0);
    CHECK(!WriteFile(c, "x", 1, &n, NULL));
    CHECK_ERR_ONE_OF4(ERROR_NO_DATA, ERROR_BROKEN_PIPE, ERROR_PIPE_NOT_CONNECTED, ERROR_NO_DATA);
    SET_ERR(0);
    char b[4]; CHECK(!ReadFile(c, b, 4, &n, NULL));
    CHECK_ERR_ONE_OF3(ERROR_BROKEN_PIPE, ERROR_PIPE_NOT_CONNECTED, ERROR_NO_DATA);
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_extra_peek_after_disconnect) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(DisconnectNamedPipe)); REQUIRE(U(PeekNamedPipe));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    CHECK(U(DisconnectNamedPipe)(s));
    DWORD av = 9; SET_ERR(0);
    CHECK(!U(PeekNamedPipe)(c, NULL, 0, NULL, &av, NULL));
    CHECK_ERR_ONE_OF3(ERROR_BROKEN_PIPE, ERROR_PIPE_NOT_CONNECTED, ERROR_NO_DATA);
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_extra_filetype_named_pipe) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    EXPECT_EQ(GetFileType(s), FILE_TYPE_PIPE);
    EXPECT_EQ(GetFileType(c), FILE_TYPE_PIPE);
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_extra_mailslot_hierarchical_name) {
    REQUIRE(U(CreateMailslotA));
    char a[128]; pmc_slotname(a, sizeof a);
    char nm[160]; snprintf(nm, sizeof nm, "%s\\sub\\leaf", a);
    SET_ERR(0);
    HANDLE h = U(CreateMailslotA)(nm, 0, 0, NULL);
    t_note("mailslot hierarchical name: %s err=%lu", h != INVALID_HANDLE_VALUE ? "created" : "failed", GetLastError());
    if (h != INVALID_HANDLE_VALUE) {
        HANDLE c = CreateFileA(nm, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (c != INVALID_HANDLE_VALUE) { DWORD n; char b[8]; CHECK(WriteFile(c, "h", 1, &n, NULL)); CHECK(ReadFile(h, b, 8, &n, NULL)); EXPECT_EQ(n, 1); CloseHandle(c); }
        CloseHandle(h);
    }
}
TEST(pmc_extra_mailslot_server_handle_not_writable) {
    REQUIRE(U(CreateMailslotA));
    char nm[128]; pmc_slotname(nm, sizeof nm);
    HANDLE h = U(CreateMailslotA)(nm, 0, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    DWORD n = 0; SET_ERR(0);
    BOOL ok = WriteFile(h, "x", 1, &n, NULL);
    t_note("WriteFile on mailslot server handle: ok=%d err=%lu", ok, GetLastError());
    CloseHandle(h);
}
