/* How a cancelled operation is delivered in each completion mode: event + GetOverlappedResult(Ex),
 * completion port (GetQueuedCompletionStatus / Ex), ReadFileEx / WriteFileEx APCs, thread-pool I/O
 * callbacks; and that a completion is delivered exactly once. */
#define XC_TU "dl"
#include "xc.h"

typedef struct { BOOL r; DWORD err, n; ULONG_PTR key; OVERLAPPED *ov; } pkt;
static pkt getpkt(HANDLE port, DWORD ms) {
    pkt p; p.n = 0xFFFF; p.key = 0xFFFF; p.ov = (OVERLAPPED *)1; SetLastError(0);
    p.r = U(GetQueuedCompletionStatus)(port, &p.n, &p.key, &p.ov, ms); p.err = p.r ? 0 : GetLastError(); return p;
}
#define CHECK_NO_PKT(port) do { pkt q_ = getpkt(port, 50); \
    t_check(!q_.r && q_.err == WAIT_TIMEOUT && q_.ov == NULL, __FILE__, __LINE__, "no further packet", "r=%d err=%lu ov=%p", q_.r, q_.err, q_.ov); } while (0)
#define NEED_IOCP() REQUIRE(U(CreateIoCompletionPort)); REQUIRE(U(GetQueuedCompletionStatus)); REQUIRE(U(CancelIoEx))

/* ---- event / GetOverlappedResult ---- */
TEST(xcan_ev_signalled_on_cancel) {
    REQUIRE(U(CancelIoEx));
    HANDLE s, c; CHECK(xpipe(&s, &c));
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev); BYTE b[8];
    CHECK(xread_pend(s, b, sizeof b, &ov));
    CHECK(!xsig(ev));
    CHECK(U(CancelIoEx)(s, &ov));
    CHECK(xwait(ev, 5000));
    EXPECT_DW(ovst(&ov), NTS_CANCELLED); EXPECT_EQ(ov.InternalHigh, 0);
    CloseHandle(ev); xclose(&s); xclose(&c);
}

TEST(xcan_ev_gor_wait_true_returns_aborted) {
    REQUIRE(U(CancelIoEx)); REQUIRE(U(GetOverlappedResult));
    HANDLE s, c; CHECK(xpipe(&s, &c));
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev); BYTE b[8];
    CHECK(xread_pend(s, b, sizeof b, &ov));
    CHECK(U(CancelIoEx)(s, &ov));
    DWORD n = 55; SetLastError(0);
    CHECK(!U(GetOverlappedResult)(s, &ov, &n, TRUE)); EXPECT_ERR(ERROR_OPERATION_ABORTED); EXPECT_DW(n, 0);
    /* asking again gives the same answer */
    n = 55; SetLastError(0);
    CHECK(!U(GetOverlappedResult)(s, &ov, &n, FALSE)); EXPECT_ERR(ERROR_OPERATION_ABORTED); EXPECT_DW(n, 0);
    CloseHandle(ev); xclose(&s); xclose(&c);
}

TEST(xcan_ev_gor_wait_true_no_event_waits_on_handle) {
    REQUIRE(U(CancelIoEx)); REQUIRE(U(GetOverlappedResult));
    HANDLE s, c; CHECK(xpipe(&s, &c));
    OVERLAPPED ov; xov(&ov, 0, NULL); BYTE b[8];
    CHECK(xread_pend(s, b, sizeof b, &ov));
    CHECK(U(CancelIoEx)(s, &ov));
    DWORD n = 55; SetLastError(0);
    CHECK(!U(GetOverlappedResult)(s, &ov, &n, TRUE)); EXPECT_ERR(ERROR_OPERATION_ABORTED); EXPECT_DW(n, 0);
    xclose(&s); xclose(&c);
}

TEST(xcan_ev_gorex_pending_timeout_and_incomplete) {
    REQUIRE(U(GetOverlappedResultEx)); REQUIRE(U(CancelIoEx));
    HANDLE s, c; CHECK(xpipe(&s, &c));
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev); BYTE b[8]; DWORD n = 3;
    CHECK(xread_pend(s, b, sizeof b, &ov));
    SetLastError(0); CHECK(!U(GetOverlappedResultEx)(s, &ov, &n, 0, FALSE)); EXPECT_ERR(ERROR_IO_INCOMPLETE);
    SetLastError(0); CHECK(!U(GetOverlappedResultEx)(s, &ov, &n, 40, FALSE)); EXPECT_ERR(WAIT_TIMEOUT);
    SetLastError(0); CHECK(!U(GetOverlappedResultEx)(s, &ov, &n, 40, TRUE)); EXPECT_ERR(WAIT_TIMEOUT);
    CHECK_PENDING(&ov);
    CHECK(U(CancelIoEx)(s, &ov));
    n = 3; SetLastError(0);
    CHECK(!U(GetOverlappedResultEx)(s, &ov, &n, 5000, FALSE)); EXPECT_ERR(ERROR_OPERATION_ABORTED); EXPECT_DW(n, 0);
    n = 3; SetLastError(0);
    CHECK(!U(GetOverlappedResultEx)(s, &ov, &n, 0, FALSE)); EXPECT_ERR(ERROR_OPERATION_ABORTED);
    CloseHandle(ev); xclose(&s); xclose(&c);
}

TEST(xcan_ev_reissue_same_overlapped_after_cancel) {
    REQUIRE(U(CancelIoEx)); REQUIRE(U(ReadFile));
    HANDLE s, c; CHECK(xpipe(&s, &c));
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev); BYTE b[8];
    CHECK(xread_pend(s, b, sizeof b, &ov));
    CHECK(U(CancelIoEx)(s, &ov)); CHECK_ABORTED(xfin(s, &ov, 5000));
    CHECK(xsig(ev));
    /* the read resets the manual-reset event and reuses the stale OVERLAPPED */
    CHECK(xread_pend(s, b, sizeof b, &ov));
    CHECK(!xsig(ev)); CHECK_PENDING(&ov);
    CHECK(xwrite_all(c, "again", 5));
    xres x = xfin(s, &ov, 5000); CHECKF(x.ok && x.n == 5 && !memcmp(b, "again", 5), "ok=%d err=%lu n=%lu", x.ok, x.err, x.n);
    CloseHandle(ev); xclose(&s); xclose(&c);
}

TEST(xcan_ev_cancel_leaves_pipe_data_for_next_read) {
    REQUIRE(U(CancelIoEx));
    HANDLE s, c; CHECK(xpipe(&s, &c));
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev); BYTE b[8]; memset(b, 0xEE, sizeof b);
    CHECK(xread_pend(s, b, sizeof b, &ov));
    CHECK(U(CancelIoEx)(s, &ov)); CHECK_ABORTED(xfin(s, &ov, 5000));
    CHECK(b[0] == 0xEE && b[7] == 0xEE);   /* the buffer of a cancelled read is not touched */
    CHECK(xwrite_all(c, "data", 4));
    xov(&ov, 0, ev); BYTE b2[8];
    SetLastError(0); BOOL r = U(ReadFile)(s, b2, sizeof b2, NULL, &ov);
    xres x = (r || GetLastError() == ERROR_IO_PENDING) ? xfin(s, &ov, 3000) : (xres){FALSE, GetLastError(), 0};
    CHECK(x.ok && x.n == 4 && !memcmp(b2, "data", 4));
    CloseHandle(ev); xclose(&s); xclose(&c);
}

TEST(xcan_ev_cancel_not_pending_overlapped_not_found) {
    REQUIRE(U(CancelIoEx));
    HANDLE s, c; CHECK(xpipe(&s, &c));
    HANDLE ev = xev(); OVERLAPPED ov, other; xov(&ov, 0, ev); xov(&other, 0, NULL); BYTE b[8];
    CHECK(xread_pend(s, b, sizeof b, &ov));
    CHECK_FAILS(U(CancelIoEx)(s, &other), ERROR_NOT_FOUND);
    CHECK(!xsig(ev)); CHECK_PENDING(&ov);
    U(CancelIoEx)(s, &ov); xfin(s, &ov, 5000);
    CloseHandle(ev); xclose(&s); xclose(&c);
}

/* ---- completion port ---- */
TEST(xcan_iocp_gqcs_cancelled_packet_fields) {
    NEED_IOCP();
    HANDLE s, c; CHECK(xpipe(&s, &c));
    HANDLE port = U(CreateIoCompletionPort)(s, NULL, 0xABCD1234, 0); CHECK(port != NULL);
    OVERLAPPED ov; xov(&ov, 0, NULL); BYTE b[8];
    CHECK(xread_pend(s, b, sizeof b, &ov));
    CHECK(U(CancelIoEx)(s, &ov));
    pkt p = getpkt(port, 5000);
    CHECK(!p.r); EXPECT_DW(p.err, ERROR_OPERATION_ABORTED); EXPECT_DW(p.n, 0);
    EXPECT_EQ(p.key, 0xABCD1234); EXPECT_EQ(p.ov, &ov);
    EXPECT_DW(ovst(&ov), NTS_CANCELLED);
    CHECK_NO_PKT(port);
    CloseHandle(port); xclose(&s); xclose(&c);
}

TEST(xcan_iocp_gqcsex_cancelled_entries) {
    NEED_IOCP(); REQUIRE(U(GetQueuedCompletionStatusEx));
    HANDLE s, c; CHECK(xpipe(&s, &c));
    HANDLE port = U(CreateIoCompletionPort)(s, NULL, 0x77, 0);
    OVERLAPPED ov[3]; BYTE b[3][8];
    for (int i = 0; i < 3; i++) { xov(&ov[i], 0, NULL); CHECK(xread_pend(s, b[i], 8, &ov[i])); }
    CHECK(U(CancelIoEx)(s, NULL));
    OVERLAPPED_ENTRY e[8]; memset(e, 0xCC, sizeof e); ULONG got = 0, total = 0; int seen[3] = {0, 0, 0};
    for (int round = 0; round < 3 && total < 3; round++) {
        got = 0; SetLastError(0);
        if (!U(GetQueuedCompletionStatusEx)(port, e + total, 8 - total, &got, 2000, FALSE)) break;
        total += got;
    }
    EXPECT_DW(total, 3);
    for (ULONG i = 0; i < total && i < 8; i++) {
        EXPECT_DW((DWORD)e[i].Internal, NTS_CANCELLED); EXPECT_EQ(e[i].lpCompletionKey, 0x77);
        EXPECT_DW(e[i].dwNumberOfBytesTransferred, 0);
        for (int k = 0; k < 3; k++) if (e[i].lpOverlapped == &ov[k]) seen[k]++;
    }
    CHECK(seen[0] == 1 && seen[1] == 1 && seen[2] == 1);
    ULONG z = 5; SetLastError(0);
    CHECK(!U(GetQueuedCompletionStatusEx)(port, e, 8, &z, 50, FALSE)); EXPECT_ERR(WAIT_TIMEOUT);
    CloseHandle(port); xclose(&s); xclose(&c);
}

TEST(xcan_iocp_completed_then_cancel_single_packet) {
    NEED_IOCP();
    HANDLE s, c; CHECK(xpipe(&s, &c));
    HANDLE port = U(CreateIoCompletionPort)(s, NULL, 0x10, 0);
    OVERLAPPED ov; xov(&ov, 0, NULL); BYTE b[8];
    CHECK(xread_pend(s, b, sizeof b, &ov));
    CHECK(xwrite_all(c, "abc", 3));
    pkt p = getpkt(port, 5000); CHECKF(p.r && p.n == 3 && p.ov == &ov, "r=%d err=%lu n=%lu", p.r, p.err, p.n);
    CHECK_FAILS(U(CancelIoEx)(s, &ov), ERROR_NOT_FOUND);
    CHECK_FAILS(U(CancelIoEx)(s, NULL), ERROR_NOT_FOUND);
    CHECK_NO_PKT(port);
    CloseHandle(port); xclose(&s); xclose(&c);
}

TEST(xcan_iocp_completion_queued_not_dequeued_then_cancel) {
    NEED_IOCP();
    HANDLE s, c; CHECK(xpipe(&s, &c));
    HANDLE port = U(CreateIoCompletionPort)(s, NULL, 0x11, 0);
    OVERLAPPED ov; xov(&ov, 0, NULL); BYTE b[8];
    CHECK(xread_pend(s, b, sizeof b, &ov));
    CHECK(xwrite_all(c, "xy", 2));
    Sleep(50);
    /* the packet sits in the port: the op is no longer cancellable */
    CHECK_FAILS(U(CancelIoEx)(s, &ov), ERROR_NOT_FOUND);
    pkt p = getpkt(port, 5000); CHECKF(p.r && p.n == 2, "r=%d err=%lu n=%lu", p.r, p.err, p.n);
    CHECK_NO_PKT(port);
    CloseHandle(port); xclose(&s); xclose(&c);
}

TEST(xcan_iocp_cancel_twice_single_packet) {
    NEED_IOCP();
    HANDLE s, c; CHECK(xpipe(&s, &c));
    HANDLE port = U(CreateIoCompletionPort)(s, NULL, 0x12, 0);
    OVERLAPPED ov; xov(&ov, 0, NULL); BYTE b[8];
    CHECK(xread_pend(s, b, sizeof b, &ov));
    CHECK(U(CancelIoEx)(s, &ov));
    Sleep(20);
    CHECK_FAILS(U(CancelIoEx)(s, &ov), ERROR_NOT_FOUND);
    pkt p = getpkt(port, 5000); CHECK(!p.r && p.err == ERROR_OPERATION_ABORTED && p.ov == &ov);
    CHECK_NO_PKT(port);
    CloseHandle(port); xclose(&s); xclose(&c);
}

TEST(xcan_iocp_event_low_bit_suppresses_packet) {
    NEED_IOCP();
    HANDLE s, c; CHECK(xpipe(&s, &c));
    HANDLE port = U(CreateIoCompletionPort)(s, NULL, 0x13, 0);
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, (HANDLE)((ULONG_PTR)ev | 1)); BYTE b[8];
    CHECK(xread_pend(s, b, sizeof b, &ov));
    CHECK(U(CancelIoEx)(s, &ov));
    CHECK(xwait(ev, 5000));
    EXPECT_DW(ovst(&ov), NTS_CANCELLED);
    CHECK_NO_PKT(port);
    CloseHandle(ev); CloseHandle(port); xclose(&s); xclose(&c);
}

TEST(xcan_iocp_cancelio_issuing_thread_packet) {
    NEED_IOCP(); REQUIRE(U(CancelIo));
    HANDLE s, c; CHECK(xpipe(&s, &c));
    HANDLE port = U(CreateIoCompletionPort)(s, NULL, 0x14, 0);
    OVERLAPPED o1, o2; xov(&o1, 0, NULL); xov(&o2, 0, NULL); BYTE b1[8], b2[8];
    CHECK(xread_pend(s, b1, 8, &o1)); CHECK(xread_pend(s, b2, 8, &o2));
    CHECK(U(CancelIo)(s));
    pkt p1 = getpkt(port, 5000), p2 = getpkt(port, 5000);
    CHECK(!p1.r && p1.err == ERROR_OPERATION_ABORTED && !p2.r && p2.err == ERROR_OPERATION_ABORTED);
    CHECK((p1.ov == &o1 && p2.ov == &o2) || (p1.ov == &o2 && p2.ov == &o1));
    CHECK_NO_PKT(port);
    CloseHandle(port); xclose(&s); xclose(&c);
}

/* ---- ReadFileEx / WriteFileEx ---- */
typedef struct { volatile LONG calls; DWORD err, n; OVERLAPPED *ov; DWORD tid; } apcrec;
static apcrec g_apc;
static void CALLBACK apc_done(DWORD err, DWORD n, OVERLAPPED *ov) {
    InterlockedIncrement(&g_apc.calls); g_apc.err = err; g_apc.n = n; g_apc.ov = ov; g_apc.tid = GetCurrentThreadId();
}
static void apc_reset(void) { memset(&g_apc, 0, sizeof g_apc); g_apc.err = 0xBEEF; g_apc.n = 0xBEEF; }
/* Drain APCs that a failed check may have left queued to this thread. */
static void apc_drain(void) { while (SleepEx(0, TRUE) == WAIT_IO_COMPLETION) {} }

TEST(xcan_apc_readfileex_cancelled_routine_aborted) {
    REQUIRE(U(ReadFileEx)); REQUIRE(U(CancelIoEx));
    HANDLE s, c; CHECK(xpipe(&s, &c)); apc_reset();
    OVERLAPPED ov; xov(&ov, 0, NULL); BYTE b[8];
    ov.Internal = 0x5A5A;   /* ReadFileEx does not set STATUS_PENDING: the field keeps the caller's value */
    SetLastError(0xBEEF); CHECK(U(ReadFileEx)(s, b, sizeof b, &ov, apc_done));
    EXPECT_DW(ovst(&ov), 0x5A5A);
    CHECK(U(CancelIoEx)(s, &ov));
    DWORD r = SleepEx(2000, TRUE);
    EXPECT_DW(r, WAIT_IO_COMPLETION);
    EXPECT_DW(g_apc.calls, 1); EXPECT_DW(g_apc.err, ERROR_OPERATION_ABORTED); EXPECT_DW(g_apc.n, 0);
    EXPECT_EQ(g_apc.ov, &ov); EXPECT_DW(g_apc.tid, GetCurrentThreadId());
    apc_drain(); xclose(&s); xclose(&c);
}

TEST(xcan_apc_readfileex_not_run_before_alertable_wait) {
    REQUIRE(U(ReadFileEx)); REQUIRE(U(CancelIoEx));
    HANDLE s, c; CHECK(xpipe(&s, &c)); apc_reset();
    OVERLAPPED ov; xov(&ov, 0, NULL); BYTE b[8];
    CHECK(U(ReadFileEx)(s, b, sizeof b, &ov, apc_done));
    CHECK(U(CancelIoEx)(s, &ov));
    Sleep(100);
    WaitForSingleObject(GetCurrentProcess(), 30);    /* non-alertable waits do not run it */
    EXPECT_DW(g_apc.calls, 0);
    EXPECT_DW(ovst(&ov), NTS_CANCELLED);             /* the status is already final */
    EXPECT_DW(SleepEx(0, TRUE), WAIT_IO_COMPLETION);
    EXPECT_DW(g_apc.calls, 1);
    EXPECT_DW(SleepEx(0, TRUE), 0);
    EXPECT_DW(g_apc.calls, 1);
    apc_drain(); xclose(&s); xclose(&c);
}

static DWORD WINAPI cancel_from_thread(void *p) { void **a = p; return U(CancelIoEx)((HANDLE)a[0], (OVERLAPPED *)a[1]); }
TEST(xcan_apc_readfileex_cancel_from_other_thread_runs_on_issuer) {
    REQUIRE(U(ReadFileEx)); REQUIRE(U(CancelIoEx));
    HANDLE s, c; CHECK(xpipe(&s, &c)); apc_reset();
    OVERLAPPED ov; xov(&ov, 0, NULL); BYTE b[8];
    CHECK(U(ReadFileEx)(s, b, sizeof b, &ov, apc_done));
    void *a[2] = {s, &ov}; HANDLE t = CreateThread(NULL, 0, cancel_from_thread, a, 0, NULL);
    CHECK(xwait(t, 5000)); DWORD rc = 0; GetExitCodeThread(t, &rc); CloseHandle(t); EXPECT_DW(rc, 1);
    EXPECT_DW(g_apc.calls, 0);
    EXPECT_DW(SleepEx(2000, TRUE), WAIT_IO_COMPLETION);
    EXPECT_DW(g_apc.calls, 1); EXPECT_DW(g_apc.tid, GetCurrentThreadId()); EXPECT_DW(g_apc.err, ERROR_OPERATION_ABORTED);
    apc_drain(); xclose(&s); xclose(&c);
}

TEST(xcan_apc_readfileex_cancelio_two_requests) {
    REQUIRE(U(ReadFileEx)); REQUIRE(U(CancelIo));
    HANDLE s, c; CHECK(xpipe(&s, &c)); apc_reset();
    OVERLAPPED o1, o2; xov(&o1, 0, NULL); xov(&o2, 0, NULL); BYTE b1[8], b2[8];
    CHECK(U(ReadFileEx)(s, b1, 8, &o1, apc_done)); CHECK(U(ReadFileEx)(s, b2, 8, &o2, apc_done));
    CHECK(U(CancelIo)(s));
    DWORD t0 = GetTickCount();
    while (g_apc.calls < 2 && GetTickCount() - t0 < 3000) SleepEx(100, TRUE);
    EXPECT_DW(g_apc.calls, 2); EXPECT_DW(g_apc.err, ERROR_OPERATION_ABORTED);
    apc_drain(); xclose(&s); xclose(&c);
}

TEST(xcan_apc_writefileex_cancelled_routine_aborted) {
    REQUIRE(U(WriteFileEx)); REQUIRE(U(CancelIoEx));
    HANDLE s, c; CHECK(xpipe_ex(&s, &c, 1024, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE, TRUE, TRUE)); apc_reset();
    static BYTE big[64 * 1024]; memset(big, 'q', sizeof big);
    OVERLAPPED ov; xov(&ov, 0, NULL); ov.Internal = 0x5A5A;
    CHECK(U(WriteFileEx)(c, big, sizeof big, &ov, apc_done));
    EXPECT_DW(ovst(&ov), 0x5A5A);
    CHECK(U(CancelIoEx)(c, &ov));
    Sleep(50); EXPECT_DW(g_apc.calls, 0);
    EXPECT_DW(SleepEx(2000, TRUE), WAIT_IO_COMPLETION);
    EXPECT_DW(g_apc.calls, 1); EXPECT_DW(g_apc.err, ERROR_OPERATION_ABORTED); EXPECT_DW(g_apc.n, 0);
    EXPECT_EQ(g_apc.ov, &ov);
    apc_drain(); xclose(&s); xclose(&c);
}

TEST(xcan_apc_readfileex_handle_closed) {
    REQUIRE(U(ReadFileEx));
    HANDLE s, c; CHECK(xpipe(&s, &c)); apc_reset();
    OVERLAPPED ov; xov(&ov, 0, NULL); BYTE b[8];
    CHECK(U(ReadFileEx)(s, b, sizeof b, &ov, apc_done));
    xclose(&s);
    EXPECT_DW(SleepEx(2000, TRUE), WAIT_IO_COMPLETION);
    EXPECT_DW(g_apc.calls, 1);
    CHECKF(g_apc.err == ERROR_BROKEN_PIPE, "err %lu Internal 0x%lx", g_apc.err, ovst(&ov));
    EXPECT_DW(g_apc.n, 0);
    apc_drain(); xclose(&c);
}

TEST(xcan_apc_readfileex_cancelled_gorex_alertable_runs_apc) {
    REQUIRE(U(ReadFileEx)); REQUIRE(U(GetOverlappedResultEx)); REQUIRE(U(CancelIoEx));
    HANDLE s, c; CHECK(xpipe(&s, &c)); apc_reset();
    OVERLAPPED ov; xov(&ov, 0, NULL); BYTE b[8];
    CHECK(U(ReadFileEx)(s, b, sizeof b, &ov, apc_done));
    CHECK(U(CancelIoEx)(s, &ov));
    /* an alertable GetOverlappedResultEx delivers the queued routine and reports the APC,
     * leaving the byte count alone; a non-waiting call then gives the cancel status */
    DWORD n = 9; SetLastError(0);
    CHECK(!U(GetOverlappedResultEx)(s, &ov, &n, 2000, TRUE)); EXPECT_ERR(WAIT_IO_COMPLETION);
    EXPECT_DW(n, 9); EXPECT_DW(g_apc.calls, 1); EXPECT_DW(g_apc.err, ERROR_OPERATION_ABORTED);
    SetLastError(0);
    CHECK(!U(GetOverlappedResultEx)(s, &ov, &n, 0, FALSE)); EXPECT_ERR(ERROR_OPERATION_ABORTED); EXPECT_DW(n, 0);
    EXPECT_DW(SleepEx(0, TRUE), 0);
    apc_drain(); xclose(&s); xclose(&c);
}

/* ---- thread-pool I/O ---- */
#define NEED_TPIO() REQUIRE(U(CreateThreadpoolIo)); REQUIRE(U(StartThreadpoolIo)); REQUIRE(U(CancelThreadpoolIo)); \
    REQUIRE(U(WaitForThreadpoolIoCallbacks)); REQUIRE(U(CloseThreadpoolIo))
typedef struct { volatile LONG n; ULONG res; ULONG_PTR bytes; PVOID ov; PTP_IO io; HANDLE ev; } tprec;
static VOID NTAPI tp_cb(PTP_CALLBACK_INSTANCE inst, PVOID ctx, PVOID ov, ULONG res, ULONG_PTR bytes, PTP_IO io) {
    tprec *r = ctx; r->res = res; r->bytes = bytes; r->ov = ov; r->io = io; InterlockedIncrement(&r->n); SetEvent(r->ev);
}
typedef struct { PTP_IO io; } tpwait;
static DWORD WINAPI tp_wait_thread(void *p) { U(WaitForThreadpoolIoCallbacks)(((tpwait *)p)->io, FALSE); return 0; }
/* WaitForThreadpoolIoCallbacks returns within ms (it hangs when a Start is left unbalanced). */
static BOOL tp_wait_bounded(PTP_IO io, DWORD ms) {
    tpwait w = {io}; HANDLE t = CreateThread(NULL, 0, tp_wait_thread, &w, 0, NULL);
    BOOL ok = xwait(t, ms);
    if (ok) CloseHandle(t);   /* a stuck waiter is leaked with its io object */
    return ok;
}

TEST(xcan_tp_cancelled_read_callback_aborted) {
    NEED_TPIO(); REQUIRE(U(CancelIoEx));
    HANDLE s, c; CHECK(xpipe(&s, &c));
    tprec r; memset(&r, 0, sizeof r); r.ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    PTP_IO io = U(CreateThreadpoolIo)(s, tp_cb, &r, NULL); CHECK(io != NULL); if (!io) return;
    OVERLAPPED ov; xov(&ov, 0, NULL); BYTE b[8];
    U(StartThreadpoolIo)(io);
    CHECK(xread_pend(s, b, sizeof b, &ov));
    CHECK(U(CancelIoEx)(s, &ov));
    CHECK(xwait(r.ev, 5000));
    EXPECT_DW(r.n, 1); EXPECT_DW(r.res, ERROR_OPERATION_ABORTED); EXPECT_EQ(r.bytes, 0);
    EXPECT_EQ(r.ov, &ov); EXPECT_EQ(r.io, io);
    CHECK(tp_wait_bounded(io, 5000));
    U(CloseThreadpoolIo)(io); CloseHandle(r.ev); xclose(&s); xclose(&c);
}

TEST(xcan_tp_cancel_after_sync_failure_balanced) {
    NEED_TPIO();
    HANDLE s, c; CHECK(xpipe(&s, &c));
    tprec r; memset(&r, 0, sizeof r); r.ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    PTP_IO io = U(CreateThreadpoolIo)(s, tp_cb, &r, NULL); CHECK(io != NULL); if (!io) return;
    OVERLAPPED ov; xov(&ov, 0, NULL);
    /* a read that fails at once (bad buffer) queues no completion: the Start is undone */
    U(StartThreadpoolIo)(io);
    SetLastError(0); BOOL ok = U(ReadFile)(s, NULL, 8, NULL, &ov); DWORD e = GetLastError();
    CHECKF(!ok && e != ERROR_IO_PENDING, "r=%d err=%lu", ok, e);
    U(CancelThreadpoolIo)(io);
    CHECK(tp_wait_bounded(io, 5000));
    EXPECT_DW(r.n, 0);
    /* the next real operation produces exactly one callback */
    BYTE b[8]; xov(&ov, 0, NULL);
    U(StartThreadpoolIo)(io);
    CHECK(xread_pend(s, b, sizeof b, &ov));
    CHECK(xwrite_all(c, "tp", 2));
    CHECK(xwait(r.ev, 5000));
    CHECK(tp_wait_bounded(io, 5000));
    EXPECT_DW(r.n, 1); EXPECT_DW(r.res, 0); EXPECT_EQ(r.bytes, 2);
    U(CloseThreadpoolIo)(io); CloseHandle(r.ev); xclose(&s); xclose(&c);
}

TEST(xcan_tp_start_cancel_without_io_balanced) {
    NEED_TPIO();
    HANDLE s, c; CHECK(xpipe(&s, &c));
    tprec r; memset(&r, 0, sizeof r); r.ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    PTP_IO io = U(CreateThreadpoolIo)(s, tp_cb, &r, NULL); CHECK(io != NULL); if (!io) return;
    for (int i = 0; i < 3; i++) U(StartThreadpoolIo)(io);
    for (int i = 0; i < 3; i++) U(CancelThreadpoolIo)(io);
    CHECK(tp_wait_bounded(io, 5000));
    EXPECT_DW(r.n, 0);
    U(CloseThreadpoolIo)(io); CloseHandle(r.ev); xclose(&s); xclose(&c);
}

TEST(xcan_tp_cancelio_null_all_callbacks_aborted) {
    NEED_TPIO(); REQUIRE(U(CancelIoEx));
    HANDLE s, c; CHECK(xpipe(&s, &c));
    tprec r; memset(&r, 0, sizeof r); r.ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    PTP_IO io = U(CreateThreadpoolIo)(s, tp_cb, &r, NULL); CHECK(io != NULL); if (!io) return;
    OVERLAPPED ov[3]; BYTE b[3][8];
    for (int i = 0; i < 3; i++) { xov(&ov[i], 0, NULL); U(StartThreadpoolIo)(io); CHECK(xread_pend(s, b[i], 8, &ov[i])); }
    CHECK(U(CancelIoEx)(s, NULL));
    DWORD t0 = GetTickCount(); while (r.n < 3 && GetTickCount() - t0 < 5000) Sleep(10);
    CHECK(tp_wait_bounded(io, 5000));
    EXPECT_DW(r.n, 3); EXPECT_DW(r.res, ERROR_OPERATION_ABORTED);
    for (int i = 0; i < 3; i++) EXPECT_DW(ovst(&ov[i]), NTS_CANCELLED);
    U(CloseThreadpoolIo)(io); CloseHandle(r.ev); xclose(&s); xclose(&c);
}

TEST(xcan_tp_cancelled_listen_callback) {
    NEED_TPIO(); REQUIRE(U(ConnectNamedPipe)); REQUIRE(U(CancelIoEx));
    char name[160]; HANDLE s = xlisten_srv(name, sizeof name, TRUE); CHECK(s != INVALID_HANDLE_VALUE);
    tprec r; memset(&r, 0, sizeof r); r.ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    PTP_IO io = U(CreateThreadpoolIo)(s, tp_cb, &r, NULL); CHECK(io != NULL); if (!io) return;
    OVERLAPPED ov; xov(&ov, 0, NULL);
    U(StartThreadpoolIo)(io);
    CHECK_IOPEND(U(ConnectNamedPipe)(s, &ov));
    CHECK(U(CancelIoEx)(s, &ov));
    CHECK(xwait(r.ev, 5000));
    EXPECT_DW(r.res, ERROR_OPERATION_ABORTED); EXPECT_EQ(r.ov, &ov); EXPECT_EQ(r.bytes, 0);
    CHECK(tp_wait_bounded(io, 5000));
    U(CloseThreadpoolIo)(io); CloseHandle(r.ev); xclose(&s);
}
