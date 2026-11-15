/* CancelSynchronousIo against synchronous operations other than pipe reads: ConnectNamedPipe,
 * WaitNamedPipe(NMPWAIT_WAIT_FOREVER), mailslot reads, ReadDirectoryChangesW, TransactNamedPipe;
 * non-I/O waits; access and handle validation. Each object must still work after the cancel. */
#define XC_TU "sy"
#include "xc.h"

#define NEED_CSI() REQUIRE(U(CancelSynchronousIo))

/* Cancel the blocked worker and require the cancel to land. */
static void cancel_blocked(HANDLE t, xwork *w) {
    DWORD last = 0; int tries = 0;
    BOOL got = xcancel_sync_retry(t, w, 5000, &last, &tries);
    t_check(got, __FILE__, __LINE__, "CancelSynchronousIo TRUE", "last err %lu, tries %d, state %ld", last, tries, w->state);
}

/* ---- ConnectNamedPipe ---- */
static BOOL op_connect(xwork *w) { return U(ConnectNamedPipe)(w->h, NULL); }
static void unblock_connect(xwork *w) { HANDLE c = xclient(w->name, FALSE); if (c != INVALID_HANDLE_VALUE) { Sleep(50); CloseHandle(c); } }

TEST(xcan_sync_connect_cancelled) {
    NEED_CSI(); REQUIRE(U(ConnectNamedPipe));
    xwork *w = xwork_new(op_connect);
    w->h = xlisten_srv(w->name, sizeof w->name, FALSE); CHECK(w->h != INVALID_HANDLE_VALUE);
    HANDLE t = xwork_start(w);
    cancel_blocked(t, w);
    CHECK(xwork_join(t, w, 5000, unblock_connect));
    CHECK(!w->r); EXPECT_DW(w->err, ERROR_OPERATION_ABORTED);
    xclose(&w->h); xwork_free(w);
}

TEST(xcan_sync_connect_cancelled_pipe_listens_again) {
    NEED_CSI(); REQUIRE(U(ConnectNamedPipe));
    xwork *w = xwork_new(op_connect);
    w->h = xlisten_srv(w->name, sizeof w->name, FALSE); CHECK(w->h != INVALID_HANDLE_VALUE);
    HANDLE t = xwork_start(w);
    cancel_blocked(t, w);
    CHECK(xwork_join(t, w, 5000, unblock_connect));
    CHECK(!w->r);
    /* a new listen on the same instance blocks until a client arrives, then succeeds */
    t = xwork_start(w);
    CHECK(xstill_blocked(w, 100));
    HANDLE c = xclient(w->name, FALSE); CHECKF(c != INVALID_HANDLE_VALUE, "client open err %lu", GetLastError());
    CHECK(xwork_join(t, w, 5000, unblock_connect));
    CHECKF(w->r || w->err == ERROR_PIPE_CONNECTED, "second listen r=%d err=%lu", w->r, w->err);
    DWORD n = 0; BYTE b[4];
    BOOL wr = c != INVALID_HANDLE_VALUE && WriteFile(c, "ping", 4, &n, NULL) && n == 4;
    CHECK(wr);
    /* synchronous read in the test thread: only once the data is known to be there */
    if (wr) CHECK(ReadFile(w->h, b, 4, &n, NULL) && n == 4 && !memcmp(b, "ping", 4));
    xclose(&c); xclose(&w->h); xwork_free(w);
}

TEST(xcan_sync_connect_cancelled_client_connects_without_listen) {
    NEED_CSI(); REQUIRE(U(ConnectNamedPipe));
    xwork *w = xwork_new(op_connect);
    w->h = xlisten_srv(w->name, sizeof w->name, FALSE); CHECK(w->h != INVALID_HANDLE_VALUE);
    HANDLE t = xwork_start(w);
    cancel_blocked(t, w);
    CHECK(xwork_join(t, w, 5000, unblock_connect));
    /* the instance stays in the listening state: a client can open it and the next listen reports it */
    HANDLE c = xclient(w->name, FALSE); CHECKF(c != INVALID_HANDLE_VALUE, "err %lu", GetLastError());
    CHECK_FAILS(U(ConnectNamedPipe)(w->h, NULL), ERROR_PIPE_CONNECTED);
    xclose(&c); xclose(&w->h); xwork_free(w);
}

/* ---- WaitNamedPipe ---- */
static BOOL op_waitpipe_w(xwork *w) { return U(WaitNamedPipeW)(xw(w->name), w->a); }
static BOOL op_waitpipe_a(xwork *w) { return U(WaitNamedPipeA)(w->name, w->a); }
/* Unblock: free the instance (disconnect the client) and listen again. */
static void unblock_waitpipe(xwork *w) {
    DisconnectNamedPipe(w->h2);
    OVERLAPPED ov; HANDLE ev = xev(); xov(&ov, 0, ev); DWORD n;
    U(ConnectNamedPipe)(w->h2, &ov);
    Sleep(300); CancelIoEx(w->h2, &ov); GetOverlappedResultEx(w->h2, &ov, &n, 2000, FALSE); CloseHandle(ev);
}
/* One-instance pipe whose only instance is connected: WaitNamedPipe has to wait. */
static BOOL busy_pipe(xwork *w, HANDLE *cli) {
    xpipename(w->name, sizeof w->name);
    w->h2 = CreateNamedPipeA(w->name, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, NULL);
    if (w->h2 == INVALID_HANDLE_VALUE) return FALSE;
    *cli = xclient(w->name, TRUE);
    return *cli != INVALID_HANDLE_VALUE;
}

TEST(xcan_sync_waitnamedpipew_forever_cancelled) {
    NEED_CSI(); REQUIRE(U(WaitNamedPipeW));
    xwork *w = xwork_new(op_waitpipe_w); HANDLE c; CHECK(busy_pipe(w, &c)); w->a = NMPWAIT_WAIT_FOREVER;
    HANDLE t = xwork_start(w);
    cancel_blocked(t, w);
    CHECK(xwork_join(t, w, 5000, unblock_waitpipe));
    CHECK(!w->r); EXPECT_DW(w->err, ERROR_OPERATION_ABORTED);
    xclose(&c); xclose(&w->h2); xwork_free(w);
}

TEST(xcan_sync_waitnamedpipea_forever_cancelled) {
    NEED_CSI(); REQUIRE(U(WaitNamedPipeA));
    xwork *w = xwork_new(op_waitpipe_a); HANDLE c; CHECK(busy_pipe(w, &c)); w->a = NMPWAIT_WAIT_FOREVER;
    HANDLE t = xwork_start(w);
    cancel_blocked(t, w);
    CHECK(xwork_join(t, w, 5000, unblock_waitpipe));
    CHECK(!w->r); EXPECT_DW(w->err, ERROR_OPERATION_ABORTED);
    xclose(&c); xclose(&w->h2); xwork_free(w);
}

TEST(xcan_sync_waitnamedpipe_timeout_cancelled_early) {
    NEED_CSI(); REQUIRE(U(WaitNamedPipeW));
    xwork *w = xwork_new(op_waitpipe_w); HANDLE c; CHECK(busy_pipe(w, &c)); w->a = 20000;
    HANDLE t = xwork_start(w);
    cancel_blocked(t, w);
    CHECK(xwork_join(t, w, 5000, unblock_waitpipe));
    CHECK(!w->r); EXPECT_DW(w->err, ERROR_OPERATION_ABORTED);
    CHECKF(w->t_ms < 5000, "took %lu ms", w->t_ms);
    xclose(&c); xclose(&w->h2); xwork_free(w);
}

TEST(xcan_sync_waitnamedpipe_works_after_cancel) {
    NEED_CSI(); REQUIRE(U(WaitNamedPipeW));
    xwork *w = xwork_new(op_waitpipe_w); HANDLE c; CHECK(busy_pipe(w, &c)); w->a = NMPWAIT_WAIT_FOREVER;
    HANDLE t = xwork_start(w);
    cancel_blocked(t, w);
    CHECK(xwork_join(t, w, 5000, unblock_waitpipe));
    /* a fresh wait on the same name: still busy -> times out; once the instance listens -> TRUE */
    CHECK_FAILS(U(WaitNamedPipeW)(xw(w->name), 100), ERROR_SEM_TIMEOUT);
    xclose(&c);
    CHECK(DisconnectNamedPipe(w->h2));
    OVERLAPPED ov; HANDLE ev = xev(); xov(&ov, 0, ev);
    CHECK_IOPEND(U(ConnectNamedPipe)(w->h2, &ov));
    SetLastError(0); BOOL r = U(WaitNamedPipeW)(xw(w->name), 2000);
    CHECKF(r, "err %lu", GetLastError());
    U(CancelIoEx)(w->h2, &ov); xfin(w->h2, &ov, 3000);
    CloseHandle(ev); xclose(&w->h2); xwork_free(w);
}

TEST(xcan_sync_waitnamedpipe_waiter_released_by_listen) {
    REQUIRE(U(WaitNamedPipeW));
    xwork *w = xwork_new(op_waitpipe_w); HANDLE c; CHECK(busy_pipe(w, &c)); w->a = NMPWAIT_WAIT_FOREVER;
    HANDLE t = xwork_start(w);
    CHECK(xstill_blocked(w, 150));
    xclose(&c);
    CHECK(DisconnectNamedPipe(w->h2));
    OVERLAPPED ov; HANDLE ev = xev(); xov(&ov, 0, ev);
    CHECK_IOPEND(U(ConnectNamedPipe)(w->h2, &ov));
    CHECK(xwork_join(t, w, 5000, unblock_waitpipe));
    CHECKF(w->r, "err %lu", w->err);
    U(CancelIoEx)(w->h2, &ov); xfin(w->h2, &ov, 3000);
    CloseHandle(ev); xclose(&w->h2); xwork_free(w);
}

/* ---- mailslot ---- */
static BOOL op_slot_read(xwork *w) { w->n = 0; return U(ReadFile)(w->h, w->buf, sizeof w->buf, &w->n, NULL); }
static void unblock_slot(xwork *w) {
    HANDLE c = CreateFileA(w->name, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (c != INVALID_HANDLE_VALUE) { DWORD n; WriteFile(c, "u", 1, &n, NULL); CloseHandle(c); }
}
static BOOL slot_write(const char *name, const char *msg) {
    HANDLE c = CreateFileA(name, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (c == INVALID_HANDLE_VALUE) return FALSE;
    DWORD n = 0; BOOL r = WriteFile(c, msg, (DWORD)strlen(msg), &n, NULL); CloseHandle(c); return r && n == strlen(msg);
}

/* A CreateMailslot handle is not a synchronous file object: ReadFile without an OVERLAPPED waits for
 * the asynchronous request in user mode, so CancelSynchronousIo finds nothing and CancelIoEx is
 * what ends the read. */
static void slot_cancel_ex(xwork *w) { U(CancelIoEx)(w->h, NULL); }
/* CancelSynchronousIo keeps failing with ERROR_NOT_FOUND while the worker stays blocked. */
static void slot_sync_cancel_not_found(HANDLE t, xwork *w) {
    CHECK(xstill_blocked(w, 150));
    int nf = 0;
    for (int i = 0; i < 10; i++) { SetLastError(0); if (!U(CancelSynchronousIo)(t) && GetLastError() == ERROR_NOT_FOUND) nf++; Sleep(5); }
    EXPECT_DW(nf, 10);
    CHECK(xstill_blocked(w, 50));
}

TEST(xcan_sync_mailslot_read_cancelsyncio_not_found) {
    NEED_CSI(); REQUIRE(U(CancelIoEx));
    xwork *w = xwork_new(op_slot_read); xslotname(w->name, sizeof w->name);
    w->h = CreateMailslotA(w->name, 0, MAILSLOT_WAIT_FOREVER, NULL); CHECK(w->h != INVALID_HANDLE_VALUE);
    HANDLE t = xwork_start(w);
    slot_sync_cancel_not_found(t, w);
    /* the read was not interrupted: it still takes the next message */
    CHECK(slot_write(w->name, "still"));
    CHECK(xwork_join(t, w, 5000, slot_cancel_ex));
    CHECK(w->r); EXPECT_DW(w->n, 5);
    xclose(&w->h); xwork_free(w);
}

TEST(xcan_sync_mailslot_read_forever_cancelioex_ends_it) {
    REQUIRE(U(CancelIoEx));
    xwork *w = xwork_new(op_slot_read); xslotname(w->name, sizeof w->name);
    w->h = CreateMailslotA(w->name, 0, MAILSLOT_WAIT_FOREVER, NULL); CHECK(w->h != INVALID_HANDLE_VALUE);
    HANDLE t = xwork_start(w);
    CHECK(xstill_blocked(w, 150));
    SetLastError(0); CHECKF(U(CancelIoEx)(w->h, NULL), "err %lu", GetLastError());
    CHECK(xwork_join(t, w, 5000, unblock_slot));
    CHECK(!w->r); EXPECT_DW(w->err, ERROR_OPERATION_ABORTED); EXPECT_DW(w->n, 0);
    xclose(&w->h); xwork_free(w);
}

TEST(xcan_sync_mailslot_reads_after_cancel) {
    REQUIRE(U(CancelIoEx));
    xwork *w = xwork_new(op_slot_read); xslotname(w->name, sizeof w->name);
    w->h = CreateMailslotA(w->name, 0, MAILSLOT_WAIT_FOREVER, NULL); CHECK(w->h != INVALID_HANDLE_VALUE);
    HANDLE t = xwork_start(w);
    CHECK(xstill_blocked(w, 150));
    CHECK(U(CancelIoEx)(w->h, NULL));
    CHECK(xwork_join(t, w, 5000, unblock_slot));
    CHECK(!w->r);
    /* nothing was consumed or queued by the cancelled read */
    DWORD next = 0, cnt = 9;
    CHECK(GetMailslotInfo(w->h, NULL, &next, &cnt, NULL)); EXPECT_DW(next, MAILSLOT_NO_MESSAGE); EXPECT_DW(cnt, 0);
    CHECK(slot_write(w->name, "after"));
    t = xwork_start(w);
    CHECK(xwork_join(t, w, 5000, unblock_slot));
    CHECK(w->r); EXPECT_DW(w->n, 5); CHECK(!memcmp(w->buf, "after", 5));
    xclose(&w->h); xwork_free(w);
}

TEST(xcan_sync_mailslot_blocked_read_gets_message) {
    xwork *w = xwork_new(op_slot_read); xslotname(w->name, sizeof w->name);
    w->h = CreateMailslotA(w->name, 0, MAILSLOT_WAIT_FOREVER, NULL); CHECK(w->h != INVALID_HANDLE_VALUE);
    HANDLE t = xwork_start(w);
    CHECK(xstill_blocked(w, 150));
    CHECK(slot_write(w->name, "wake"));
    CHECK(xwork_join(t, w, 5000, unblock_slot));
    CHECK(w->r); EXPECT_DW(w->n, 4);
    xclose(&w->h); xwork_free(w);
}

TEST(xcan_sync_mailslot_timeout_read_cancelled_early) {
    REQUIRE(U(CancelIoEx));
    xwork *w = xwork_new(op_slot_read); xslotname(w->name, sizeof w->name);
    w->h = CreateMailslotA(w->name, 0, 20000, NULL); CHECK(w->h != INVALID_HANDLE_VALUE);
    HANDLE t = xwork_start(w);
    CHECK(xstill_blocked(w, 150));
    CHECK(U(CancelIoEx)(w->h, NULL));
    CHECK(xwork_join(t, w, 5000, unblock_slot));
    CHECK(!w->r); EXPECT_DW(w->err, ERROR_OPERATION_ABORTED);
    CHECKF(w->t_ms < 5000, "took %lu ms", w->t_ms);
    xclose(&w->h); xwork_free(w);
}

TEST(xcan_sync_mailslot_zero_timeout_not_blocking) {
    NEED_CSI();
    char name[160]; xslotname(name, sizeof name);
    HANDLE h = CreateMailslotA(name, 0, 0, NULL); CHECK(h != INVALID_HANDLE_VALUE);
    BYTE b[16]; DWORD n = 7;
    CHECK_FAILS(U(ReadFile)(h, b, sizeof b, &n, NULL), ERROR_SEM_TIMEOUT);
    CHECK_FAILS(U(CancelIoEx)(h, NULL), ERROR_NOT_FOUND);
    xclose(&h);
}

/* ---- ReadDirectoryChangesW (synchronous) ---- */
static BOOL op_rdc_sync(xwork *w) {
    w->n = 0xFFFF;
    return U(ReadDirectoryChangesW)(w->h, w->buf, sizeof w->buf, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, &w->n, NULL, NULL);
}
static void unblock_rdc(xwork *w) { xtouch(w->name, "unblock.txt"); }

TEST(xcan_sync_rdc_cancelled) {
    NEED_CSI(); REQUIRE(U(ReadDirectoryChangesW));
    xwork *w = xwork_new(op_rdc_sync); xmkdir(w->name, "rdc");
    w->h = xdiropen(w->name, FALSE); CHECK(w->h != INVALID_HANDLE_VALUE);
    HANDLE t = xwork_start(w);
    cancel_blocked(t, w);
    CHECK(xwork_join(t, w, 5000, unblock_rdc));
    CHECK(!w->r); EXPECT_DW(w->err, ERROR_OPERATION_ABORTED);
    xclose(&w->h); xrmtree1(w->name); xwork_free(w);
}

TEST(xcan_sync_rdc_watch_works_after_cancel) {
    NEED_CSI(); REQUIRE(U(ReadDirectoryChangesW));
    xwork *w = xwork_new(op_rdc_sync); xmkdir(w->name, "rdc");
    w->h = xdiropen(w->name, FALSE); CHECK(w->h != INVALID_HANDLE_VALUE);
    HANDLE t = xwork_start(w);
    cancel_blocked(t, w);
    CHECK(xwork_join(t, w, 5000, unblock_rdc));
    CHECK(!w->r);
    t = xwork_start(w);
    CHECK(xstill_blocked(w, 100));
    xtouch(w->name, "second.txt");
    CHECK(xwork_join(t, w, 5000, unblock_rdc));
    CHECKF(w->r, "err %lu", w->err);
    char nm[64] = ""; DWORD act = w->r && w->n ? xfni_first(w->buf, nm, sizeof nm) : 0;
    EXPECT_DW(act, FILE_ACTION_ADDED); EXPECT_STR(nm, "second.txt");
    xclose(&w->h); xrmtree1(w->name); xwork_free(w);
}

TEST(xcan_sync_rdc_change_after_cancel_is_queued) {
    NEED_CSI(); REQUIRE(U(ReadDirectoryChangesW));
    xwork *w = xwork_new(op_rdc_sync); xmkdir(w->name, "rdc");
    w->h = xdiropen(w->name, FALSE); CHECK(w->h != INVALID_HANDLE_VALUE);
    HANDLE t = xwork_start(w);
    cancel_blocked(t, w);
    CHECK(xwork_join(t, w, 5000, unblock_rdc));
    /* the watch set up by the first call outlives its cancelled request: a change made with
     * no request outstanding is reported by the next call at once */
    xtouch(w->name, "queued.txt");
    t = xwork_start(w);
    CHECK(xwork_join(t, w, 5000, unblock_rdc));
    CHECKF(w->r, "err %lu", w->err);
    char nm[64] = ""; DWORD act = w->r && w->n ? xfni_first(w->buf, nm, sizeof nm) : 0;
    EXPECT_DW(act, FILE_ACTION_ADDED); EXPECT_STR(nm, "queued.txt");
    xclose(&w->h); xrmtree1(w->name); xwork_free(w);
}

/* ---- TransactNamedPipe (synchronous) ---- */
static BOOL op_transact(xwork *w) {
    w->n = 0xFFFF;
    return U(TransactNamedPipe)(w->h, (void *)"request", 7, w->buf, sizeof w->buf, &w->n, NULL);
}
static void unblock_transact(xwork *w) {
    BYTE b[64]; DWORD n, avail = 0;
    /* the server handle is synchronous: read the request only when it is there */
    if (PeekNamedPipe(w->h2, NULL, 0, NULL, &avail, NULL) && avail) ReadFile(w->h2, b, sizeof b, &n, NULL);
    WriteFile(w->h2, "r", 1, &n, NULL);
}

TEST(xcan_sync_transact_cancelled_request_delivered) {
    NEED_CSI(); REQUIRE(U(TransactNamedPipe));
    xwork *w = xwork_new(op_transact); HANDLE s, c;
    CHECK(xpipe_ex(&s, &c, 4096, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE, FALSE, FALSE));
    w->h = c; w->h2 = s;
    HANDLE t = xwork_start(w);
    cancel_blocked(t, w);
    CHECK(xwork_join(t, w, 5000, unblock_transact));
    CHECK(!w->r); EXPECT_DW(w->err, ERROR_OPERATION_ABORTED);
    /* the write half of the transaction already happened: the server still gets the request */
    BYTE b[64]; DWORD n = 0; DWORD avail = 0;
    CHECK(PeekNamedPipe(s, NULL, 0, NULL, &avail, NULL)); EXPECT_DW(avail, 7);
    if (avail) { CHECK(ReadFile(s, b, sizeof b, &n, NULL)); EXPECT_DW(n, 7); }
    xclose(&w->h); xclose(&w->h2); xwork_free(w);
}

/* ---- non-I/O waits: nothing to cancel, the wait goes on ---- */
static BOOL op_wait_event(xwork *w) { DWORD r = WaitForSingleObject(w->h2, 10000); w->n = r; return r == WAIT_OBJECT_0; }
static void unblock_event(xwork *w) { SetEvent(w->h2); }

TEST(xcan_sync_cancel_thread_in_wfso_not_found) {
    NEED_CSI();
    xwork *w = xwork_new(op_wait_event); w->h2 = xev();
    HANDLE t = xwork_start(w);
    CHECK(xstill_blocked(w, 100));
    CHECK_FAILS(U(CancelSynchronousIo)(t), ERROR_NOT_FOUND);
    CHECK(xstill_blocked(w, 100));
    SetEvent(w->h2);
    CHECK(xwork_join(t, w, 5000, unblock_event));
    CHECK(w->r); EXPECT_DW(w->n, WAIT_OBJECT_0);
    xclose(&w->h2); xwork_free(w);
}

static BOOL op_sleepex(xwork *w) { w->n = SleepEx(600, TRUE); return TRUE; }
TEST(xcan_sync_cancel_thread_in_sleepex_not_interrupted) {
    NEED_CSI();
    xwork *w = xwork_new(op_sleepex);
    HANDLE t = xwork_start(w);
    Sleep(100);
    CHECK_FAILS(U(CancelSynchronousIo)(t), ERROR_NOT_FOUND);
    CHECK(xwork_join(t, w, 5000, NULL));
    EXPECT_DW(w->n, 0);
    CHECKF(w->t_ms >= 500, "SleepEx returned after %lu ms", w->t_ms);
    xwork_free(w);
}

static BOOL op_gqcs(xwork *w) {
    ULONG_PTR k = 0; OVERLAPPED *po = NULL; DWORD n = 0;
    BOOL r = U(GetQueuedCompletionStatus)(w->h2, &n, &k, &po, 10000); w->n = (DWORD)k; return r;
}
static void unblock_gqcs(xwork *w) { PostQueuedCompletionStatus(w->h2, 0, 0xEE, NULL); }
TEST(xcan_sync_cancel_thread_in_gqcs_not_found) {
    NEED_CSI(); REQUIRE(U(CreateIoCompletionPort)); REQUIRE(U(GetQueuedCompletionStatus));
    xwork *w = xwork_new(op_gqcs); w->h2 = U(CreateIoCompletionPort)(INVALID_HANDLE_VALUE, NULL, 0, 1);
    HANDLE t = xwork_start(w);
    CHECK(xstill_blocked(w, 100));
    CHECK_FAILS(U(CancelSynchronousIo)(t), ERROR_NOT_FOUND);
    CHECK(xstill_blocked(w, 100));
    CHECK(PostQueuedCompletionStatus(w->h2, 3, 0x51, NULL));
    CHECK(xwork_join(t, w, 5000, unblock_gqcs));
    CHECK(w->r); EXPECT_DW(w->n, 0x51);
    xclose(&w->h2); xwork_free(w);
}

/* ---- validation ---- */
TEST(xcan_sync_cancel_current_thread_not_found) {
    NEED_CSI();
    CHECK_FAILS(U(CancelSynchronousIo)(GetCurrentThread()), ERROR_NOT_FOUND);
}

TEST(xcan_sync_cancel_without_thread_terminate_denied) {
    NEED_CSI();
    xwork *w = xwork_new(op_connect);
    w->h = xlisten_srv(w->name, sizeof w->name, FALSE); CHECK(w->h != INVALID_HANDLE_VALUE);
    HANDLE t = xwork_start(w);
    CHECK(xstill_blocked(w, 100));
    HANDLE lim = OpenThread(THREAD_ALL_ACCESS & ~THREAD_TERMINATE, FALSE, w->tid);
    CHECK(lim != NULL);
    if (lim) { CHECK_FAILS(U(CancelSynchronousIo)(lim), ERROR_ACCESS_DENIED); CloseHandle(lim); }
    CHECK(xstill_blocked(w, 50));
    /* THREAD_TERMINATE alone is enough */
    HANDLE term = OpenThread(THREAD_TERMINATE, FALSE, w->tid); CHECK(term != NULL);
    if (term) {
        BOOL got = FALSE; DWORD e = 0;
        for (int i = 0; i < 500 && !got; i++) { SetLastError(0); got = U(CancelSynchronousIo)(term); e = GetLastError(); if (!got) Sleep(10); }
        CHECKF(got, "err %lu", e); CloseHandle(term);
    }
    CHECK(xwork_join(t, w, 5000, unblock_connect));
    CHECK(!w->r); EXPECT_DW(w->err, ERROR_OPERATION_ABORTED);
    xclose(&w->h); xwork_free(w);
}

TEST(xcan_sync_cancel_non_thread_handles) {
    NEED_CSI();
    HANDLE ev = xev();
    CHECK_FAILS(U(CancelSynchronousIo)(ev), ERROR_INVALID_HANDLE);
    CHECK_FAILS(U(CancelSynchronousIo)(GetCurrentProcess()), ERROR_INVALID_HANDLE);
    HANDLE stale = xev(); CloseHandle(stale);
    CHECK_FAILS(U(CancelSynchronousIo)(stale), ERROR_INVALID_HANDLE);
    CloseHandle(ev);
}

static DWORD WINAPI quick_exit_thread(void *p) { return 5; }
TEST(xcan_sync_cancel_exited_thread_not_found) {
    NEED_CSI();
    HANDLE t = CreateThread(NULL, 0, quick_exit_thread, NULL, 0, NULL);
    CHECK(xwait(t, 5000));
    CHECK_FAILS(U(CancelSynchronousIo)(t), ERROR_NOT_FOUND);
    CloseHandle(t);
}

TEST(xcan_sync_second_cancel_after_completion_not_found) {
    NEED_CSI(); REQUIRE(U(ConnectNamedPipe));
    xwork *w = xwork_new(op_connect);
    w->h = xlisten_srv(w->name, sizeof w->name, FALSE); CHECK(w->h != INVALID_HANDLE_VALUE);
    HANDLE t = xwork_start(w);
    HANDLE t2 = NULL; DuplicateHandle(GetCurrentProcess(), t, GetCurrentProcess(), &t2, 0, FALSE, DUPLICATE_SAME_ACCESS);
    cancel_blocked(t, w);
    CHECK(xwork_join(t, w, 5000, unblock_connect));
    CHECK(!w->r);
    /* the thread is gone; a second cancel through another handle finds nothing */
    CHECK_FAILS(U(CancelSynchronousIo)(t2), ERROR_NOT_FOUND);
    CloseHandle(t2); xclose(&w->h); xwork_free(w);
}
