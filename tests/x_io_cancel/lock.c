/* Pending byte-range locks: overlapped LockFileEx waiting on a conflicting range, and synchronous
 * LockFileEx blocked in a worker, cancelled with CancelIo, CancelIoEx and CancelSynchronousIo. */
#define XC_TU "lk"
#include "xc.h"

#define LOFF 100
#define LLEN 50
#define NEED_LOCK() REQUIRE(U(LockFileEx)); REQUIRE(U(UnlockFileEx)); REQUIRE(U(CancelIoEx)); REQUIRE(U(CancelIo))

/* Holder locks [LOFF, LOFF+LLEN); the overlapped waiter then queues an exclusive request for it. */
static BOOL lk_setup(xlockf *f, OVERLAPPED *ov, HANDLE ev) {
    if (!xlock_open(f, FALSE)) return FALSE;
    if (!xlock_hold(f->hold, LOFF, LLEN)) return FALSE;
    xov(ov, LOFF, ev);
    SetLastError(0);
    BOOL r = U(LockFileEx)(f->wait, LOCKFILE_EXCLUSIVE_LOCK, 0, LLEN, 0, ov);
    return !r && GetLastError() == ERROR_IO_PENDING;
}
/* Leave nothing pending on a waiter that a failed check may have left queued. */
static void lk_drain(xlockf *f, OVERLAPPED *ov) {
    if (ovst(ov) == NTS_PENDING) { U(CancelIoEx)(f->wait, ov); xfin(f->wait, ov, 3000); }
}

TEST(xcan_lock_pending_overlapped_state) {
    NEED_LOCK();
    xlockf f; OVERLAPPED ov; HANDLE ev = xev();
    CHECK(lk_setup(&f, &ov, ev));
    CHECK_PENDING(&ov);
    CHECK(!xsig(ev));
    DWORD n = 77; SetLastError(0);
    CHECK(!GetOverlappedResultEx(f.wait, &ov, &n, 0, FALSE)); EXPECT_ERR(ERROR_IO_INCOMPLETE);
    SetLastError(0);
    CHECK(!GetOverlappedResultEx(f.wait, &ov, &n, 60, FALSE)); EXPECT_ERR(WAIT_TIMEOUT);
    CHECK(!xsig(ev)); CHECK_PENDING(&ov);
    lk_drain(&f, &ov); CloseHandle(ev); xlock_close(&f);
}

TEST(xcan_lock_cancelioex_specific_aborts) {
    NEED_LOCK();
    xlockf f; OVERLAPPED ov; HANDLE ev = xev();
    CHECK(lk_setup(&f, &ov, ev));
    SetLastError(0xBEEF); CHECK(U(CancelIoEx)(f.wait, &ov));
    CHECK(xwait(ev, 5000));
    EXPECT_DW(ovst(&ov), NTS_CANCELLED); EXPECT_EQ(ov.InternalHigh, 0);
    CHECK_ABORTED(xnow(f.wait, &ov));
    lk_drain(&f, &ov); CloseHandle(ev); xlock_close(&f);
}

TEST(xcan_lock_cancelled_request_holds_no_lock) {
    NEED_LOCK();
    xlockf f; OVERLAPPED ov; HANDLE ev = xev();
    CHECK(lk_setup(&f, &ov, ev));
    CHECK(U(CancelIoEx)(f.wait, &ov)); CHECK_ABORTED(xfin(f.wait, &ov, 5000));
    CHECK(xunlock(f.hold, LOFF, LLEN));
    /* the cancelled request was never granted: a third handle gets the range at once */
    CHECK(xrange_free(f.path, LOFF, LLEN));
    OVERLAPPED u; xov(&u, LOFF, NULL);
    CHECK_FAILS(U(UnlockFileEx)(f.wait, 0, LLEN, 0, &u), ERROR_NOT_LOCKED);
    lk_drain(&f, &ov); CloseHandle(ev); xlock_close(&f);
}

TEST(xcan_lock_cancelled_waiter_relocks_after_release) {
    NEED_LOCK();
    xlockf f; OVERLAPPED ov; HANDLE ev = xev();
    CHECK(lk_setup(&f, &ov, ev));
    CHECK(U(CancelIoEx)(f.wait, &ov)); CHECK_ABORTED(xfin(f.wait, &ov, 5000));
    CHECK(xunlock(f.hold, LOFF, LLEN));
    OVERLAPPED o2; xov(&o2, LOFF, ev); ResetEvent(ev);
    SetLastError(0); BOOL r = U(LockFileEx)(f.wait, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, LLEN, 0, &o2);
    if (!r && GetLastError() == ERROR_IO_PENDING) r = xfin(f.wait, &o2, 5000).ok;
    CHECK(r);
    CHECK(!xlock_hold(f.hold, LOFF, LLEN));
    CHECK(U(UnlockFileEx)(f.wait, 0, LLEN, 0, &o2));
    lk_drain(&f, &ov); CloseHandle(ev); xlock_close(&f);
}

TEST(xcan_lock_reissue_same_overlapped_after_cancel) {
    NEED_LOCK();
    xlockf f; OVERLAPPED ov; HANDLE ev = xev();
    CHECK(lk_setup(&f, &ov, ev));
    CHECK(U(CancelIoEx)(f.wait, &ov)); CHECK_ABORTED(xfin(f.wait, &ov, 5000));
    /* same OVERLAPPED, same event: pending again, then granted when the holder releases */
    ResetEvent(ev); ov.Offset = LOFF; ov.OffsetHigh = 0;
    CHECK_IOPEND(U(LockFileEx)(f.wait, LOCKFILE_EXCLUSIVE_LOCK, 0, LLEN, 0, &ov));
    CHECK_PENDING(&ov);
    CHECK(xunlock(f.hold, LOFF, LLEN));
    xres x = xfin(f.wait, &ov, 5000); CHECKF(x.ok, "err %lu", x.err);
    EXPECT_DW(ovst(&ov), 0);
    CHECK(!xrange_free(f.path, LOFF, LLEN));
    CHECK(xunlock(f.wait, LOFF, LLEN));
    lk_drain(&f, &ov); CloseHandle(ev); xlock_close(&f);
}

TEST(xcan_lock_cancelio_issuing_thread) {
    NEED_LOCK();
    xlockf f; OVERLAPPED ov; HANDLE ev = xev();
    CHECK(lk_setup(&f, &ov, ev));
    SetLastError(0xBEEF); CHECK(U(CancelIo)(f.wait));
    CHECK(xwait(ev, 5000)); EXPECT_DW(ovst(&ov), NTS_CANCELLED);
    CHECK_ABORTED(xnow(f.wait, &ov));
    CHECK(xunlock(f.hold, LOFF, LLEN)); CHECK(xrange_free(f.path, LOFF, LLEN));
    lk_drain(&f, &ov); CloseHandle(ev); xlock_close(&f);
}

static DWORD WINAPI lk_cancelio_thread(void *p) { SetLastError(0); BOOL r = U(CancelIo)((HANDLE)p); return r ? 1 : 0x10000 | GetLastError(); }
TEST(xcan_lock_cancelio_other_thread_cancels_nothing) {
    NEED_LOCK();
    xlockf f; OVERLAPPED ov; HANDLE ev = xev();
    CHECK(lk_setup(&f, &ov, ev));
    HANDLE t = CreateThread(NULL, 0, lk_cancelio_thread, f.wait, 0, NULL);
    CHECK(xwait(t, 5000)); DWORD rc = 0; GetExitCodeThread(t, &rc); CloseHandle(t);
    EXPECT_DW(rc, 1);
    CHECK(WaitForSingleObject(ev, 150) == WAIT_TIMEOUT); CHECK_PENDING(&ov);
    /* still a live request: the holder's release grants it */
    CHECK(xunlock(f.hold, LOFF, LLEN));
    CHECK(xfin(f.wait, &ov, 5000).ok);
    CHECK(xunlock(f.wait, LOFF, LLEN));
    lk_drain(&f, &ov); CloseHandle(ev); xlock_close(&f);
}

typedef struct { HANDLE h; OVERLAPPED *ov; } lk_cx;
static DWORD WINAPI lk_cancelex_thread(void *p) { lk_cx *c = p; SetLastError(0); BOOL r = U(CancelIoEx)(c->h, c->ov); return r ? 1 : 0x10000 | GetLastError(); }
TEST(xcan_lock_cancelioex_from_other_thread) {
    NEED_LOCK();
    xlockf f; OVERLAPPED ov; HANDLE ev = xev();
    CHECK(lk_setup(&f, &ov, ev));
    lk_cx c = {f.wait, &ov};
    HANDLE t = CreateThread(NULL, 0, lk_cancelex_thread, &c, 0, NULL);
    CHECK(xwait(t, 5000)); DWORD rc = 0; GetExitCodeThread(t, &rc); CloseHandle(t);
    EXPECT_DW(rc, 1);
    CHECK_ABORTED(xfin(f.wait, &ov, 5000));
    lk_drain(&f, &ov); CloseHandle(ev); xlock_close(&f);
}

TEST(xcan_lock_cancelioex_null_cancels_all_ranges) {
    NEED_LOCK();
    xlockf f; OVERLAPPED o1, o2; HANDLE e1 = xev(), e2 = xev();
    CHECK(lk_setup(&f, &o1, e1));
    CHECK(xlock_hold(f.hold, 1000, 10));
    xov(&o2, 1000, e2);
    CHECK_IOPEND(U(LockFileEx)(f.wait, 0, 0, 10, 0, &o2));
    CHECK(U(CancelIoEx)(f.wait, NULL));
    CHECK_ABORTED(xfin(f.wait, &o1, 5000)); CHECK_ABORTED(xfin(f.wait, &o2, 5000));
    SetLastError(0); CHECK(!U(CancelIoEx)(f.wait, NULL)); EXPECT_ERR(ERROR_NOT_FOUND);
    lk_drain(&f, &o1); lk_drain(&f, &o2); CloseHandle(e1); CloseHandle(e2); xlock_close(&f);
}

TEST(xcan_lock_cancel_one_of_two_other_granted) {
    NEED_LOCK();
    xlockf f; OVERLAPPED o1, o2; HANDLE e1 = xev(), e2 = xev();
    CHECK(lk_setup(&f, &o1, e1));
    CHECK(xlock_hold(f.hold, 1000, 10));
    xov(&o2, 1000, e2);
    CHECK_IOPEND(U(LockFileEx)(f.wait, LOCKFILE_EXCLUSIVE_LOCK, 0, 10, 0, &o2));
    CHECK(U(CancelIoEx)(f.wait, &o1));
    CHECK_ABORTED(xfin(f.wait, &o1, 5000));
    CHECK(!xsig(e2)); CHECK_PENDING(&o2);
    CHECK(xunlock(f.hold, 1000, 10));
    CHECK(xfin(f.wait, &o2, 5000).ok); EXPECT_DW(ovst(&o2), 0);
    EXPECT_DW(ovst(&o1), NTS_CANCELLED);
    CHECK(xunlock(f.wait, 1000, 10));
    lk_drain(&f, &o1); lk_drain(&f, &o2); CloseHandle(e1); CloseHandle(e2); xlock_close(&f);
}

TEST(xcan_lock_shared_waiter_cancelled) {
    NEED_LOCK();
    xlockf f; OVERLAPPED ov; HANDLE ev = xev();
    CHECK(xlock_open(&f, FALSE)); CHECK(xlock_hold(f.hold, LOFF, LLEN));
    xov(&ov, LOFF, ev);
    CHECK_IOPEND(U(LockFileEx)(f.wait, 0, 0, LLEN, 0, &ov));
    CHECK(U(CancelIoEx)(f.wait, &ov));
    CHECK_ABORTED(xfin(f.wait, &ov, 5000)); EXPECT_DW(ovst(&ov), NTS_CANCELLED);
    CHECK(xunlock(f.hold, LOFF, LLEN)); CHECK(xrange_free(f.path, LOFF, LLEN));
    lk_drain(&f, &ov); CloseHandle(ev); xlock_close(&f);
}

TEST(xcan_lock_granted_then_cancel_not_found) {
    NEED_LOCK();
    xlockf f; OVERLAPPED ov; HANDLE ev = xev();
    CHECK(lk_setup(&f, &ov, ev));
    CHECK(xunlock(f.hold, LOFF, LLEN));
    CHECK(xfin(f.wait, &ov, 5000).ok);
    CHECK_FAILS(U(CancelIoEx)(f.wait, &ov), ERROR_NOT_FOUND);
    CHECK_FAILS(U(CancelIoEx)(f.wait, NULL), ERROR_NOT_FOUND);
    EXPECT_DW(ovst(&ov), 0);
    CHECK(!xrange_free(f.path, LOFF, LLEN));   /* the granted lock stays */
    CHECK(xunlock(f.wait, LOFF, LLEN));
    lk_drain(&f, &ov); CloseHandle(ev); xlock_close(&f);
}

TEST(xcan_lock_cancel_twice_second_not_found) {
    NEED_LOCK();
    xlockf f; OVERLAPPED ov; HANDLE ev = xev();
    CHECK(lk_setup(&f, &ov, ev));
    CHECK(U(CancelIoEx)(f.wait, &ov));
    CHECK_ABORTED(xfin(f.wait, &ov, 5000));
    CHECK_FAILS(U(CancelIoEx)(f.wait, &ov), ERROR_NOT_FOUND);
    CHECK_ABORTED(xnow(f.wait, &ov));
    lk_drain(&f, &ov); CloseHandle(ev); xlock_close(&f);
}

TEST(xcan_lock_iocp_cancel_packet) {
    NEED_LOCK(); REQUIRE(U(CreateIoCompletionPort)); REQUIRE(U(GetQueuedCompletionStatus));
    xlockf f; CHECK(xlock_open(&f, FALSE)); CHECK(xlock_hold(f.hold, LOFF, LLEN));
    HANDLE port = U(CreateIoCompletionPort)(f.wait, NULL, 0x4C4B, 0); CHECK(port != NULL);
    OVERLAPPED ov; xov(&ov, LOFF, NULL);
    CHECK_IOPEND(U(LockFileEx)(f.wait, LOCKFILE_EXCLUSIVE_LOCK, 0, LLEN, 0, &ov));
    DWORD n = 9; ULONG_PTR k = 0; OVERLAPPED *po = NULL;
    SetLastError(0); CHECK(!U(GetQueuedCompletionStatus)(port, &n, &k, &po, 0)); EXPECT_ERR(WAIT_TIMEOUT);
    CHECK(U(CancelIoEx)(f.wait, &ov));
    n = 9; SetLastError(0);
    CHECK(!U(GetQueuedCompletionStatus)(port, &n, &k, &po, 5000)); EXPECT_ERR(ERROR_OPERATION_ABORTED);
    EXPECT_EQ(po, &ov); EXPECT_EQ(k, 0x4C4B); EXPECT_DW(n, 0);
    SetLastError(0); CHECK(!U(GetQueuedCompletionStatus)(port, &n, &k, &po, 100)); EXPECT_ERR(WAIT_TIMEOUT);
    EXPECT_EQ(po, NULL);
    lk_drain(&f, &ov); CloseHandle(port); xlock_close(&f);
}

TEST(xcan_lock_fail_immediately_conflict_queues_failure_packet) {
    NEED_LOCK(); REQUIRE(U(CreateIoCompletionPort)); REQUIRE(U(GetQueuedCompletionStatus));
    xlockf f; CHECK(xlock_open(&f, FALSE)); CHECK(xlock_hold(f.hold, LOFF, LLEN));
    HANDLE port = U(CreateIoCompletionPort)(f.wait, NULL, 7, 0); CHECK(port != NULL);
    OVERLAPPED ov; xov(&ov, LOFF, NULL);
    CHECK_FAILS(U(LockFileEx)(f.wait, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, LLEN, 0, &ov), ERROR_LOCK_VIOLATION);
    CHECK_FAILS(U(CancelIoEx)(f.wait, &ov), ERROR_NOT_FOUND);
    /* the immediate failure still reaches the port, once */
    DWORD n = 9; ULONG_PTR k = 0; OVERLAPPED *po = NULL;
    SetLastError(0); CHECK(!U(GetQueuedCompletionStatus)(port, &n, &k, &po, 1000)); EXPECT_ERR(ERROR_LOCK_VIOLATION);
    EXPECT_EQ(po, &ov); EXPECT_EQ(k, 7); EXPECT_DW(n, 0);
    SetLastError(0); CHECK(!U(GetQueuedCompletionStatus)(port, &n, &k, &po, 50)); EXPECT_ERR(WAIT_TIMEOUT);
    EXPECT_EQ(po, NULL);
    CloseHandle(port); xlock_close(&f);
}

typedef struct { HANDLE h; OVERLAPPED *ov; BOOL r; DWORD e, n; volatile LONG st; } lk_gor;
static DWORD WINAPI lk_gor_thread(void *p) {
    lk_gor *g = p;
    g->st = 1; SetLastError(0);
    g->r = U(GetOverlappedResult)(g->h, g->ov, &g->n, TRUE); g->e = g->r ? 0 : GetLastError();
    g->st = 2; return 0;
}
TEST(xcan_lock_gor_wait_in_worker_released_by_cancel) {
    NEED_LOCK();
    xlockf f; OVERLAPPED ov; HANDLE ev = xev();
    CHECK(lk_setup(&f, &ov, ev));
    /* GetOverlappedResult(bWait=TRUE) blocks in a worker until the cancel lands */
    lk_gor g = {f.wait, &ov, TRUE, 0, 9, 0};
    HANDLE t = CreateThread(NULL, 0, lk_gor_thread, &g, 0, NULL);
    for (int i = 0; i < 200 && g.st == 0; i++) Sleep(10);
    Sleep(50); CHECK(g.st == 1);
    CHECK(U(CancelIoEx)(f.wait, &ov));
    if (!xwait(t, 5000)) { CHECK(0); xunlock(f.hold, LOFF, LLEN); xwait(t, 5000); }
    CHECK(!g.r); EXPECT_DW(g.e, ERROR_OPERATION_ABORTED); EXPECT_DW(g.n, 0);
    CloseHandle(t);
    lk_drain(&f, &ov); CloseHandle(ev); xlock_close(&f);
}

static DWORD WINAPI lk_issue_and_exit(void *p) {
    lk_cx *c = p; SetLastError(0);
    BOOL r = U(LockFileEx)(c->h, LOCKFILE_EXCLUSIVE_LOCK, 0, LLEN, 0, c->ov);
    return !r && GetLastError() == ERROR_IO_PENDING;
}
TEST(xcan_lock_issuing_thread_exit_cancels) {
    NEED_LOCK();
    xlockf f; CHECK(xlock_open(&f, FALSE)); CHECK(xlock_hold(f.hold, LOFF, LLEN));
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, LOFF, ev);
    lk_cx c = {f.wait, &ov};
    HANDLE t = CreateThread(NULL, 0, lk_issue_and_exit, &c, 0, NULL);
    CHECK(xwait(t, 5000)); DWORD rc = 0; GetExitCodeThread(t, &rc); CloseHandle(t);
    EXPECT_DW(rc, 1);
    CHECK(xwait(ev, 3000));
    EXPECT_DW(ovst(&ov), NTS_CANCELLED);
    CHECK_ABORTED(xnow(f.wait, &ov));
    CHECK(xunlock(f.hold, LOFF, LLEN)); CHECK(xrange_free(f.path, LOFF, LLEN));
    lk_drain(&f, &ov); CloseHandle(ev); xlock_close(&f);
}

TEST(xcan_lock_close_waiter_with_pending_request) {
    NEED_LOCK();
    xlockf f; OVERLAPPED ov; HANDLE ev = xev();
    CHECK(lk_setup(&f, &ov, ev));
    HANDLE stale = f.wait; xclose(&f.wait);
    CHECK(xwait(ev, 5000));
    xres x = xnow(stale, &ov);
    /* not a cancel: the cleanup of the file object completes the waiter with STATUS_RANGE_NOT_LOCKED */
    CHECKF(!x.ok && x.err == ERROR_NOT_LOCKED && x.n == 0, "ok=%d err=%lu n=%lu Internal=0x%lx", x.ok, x.err, x.n, ovst(&ov));
    EXPECT_DW(ovst(&ov), 0xC000007Eu);
    CHECK(xunlock(f.hold, LOFF, LLEN)); CHECK(xrange_free(f.path, LOFF, LLEN));
    CloseHandle(ev); xlock_close(&f);
}

/* ---- synchronous waiter ---- */
static BOOL op_sync_lock(xwork *w) {
    OVERLAPPED ov; xov(&ov, w->a, NULL);
    return U(LockFileEx)(w->h, LOCKFILE_EXCLUSIVE_LOCK, 0, w->b, 0, &ov);
}
static void unblock_lock(xwork *w) { xunlock(w->h2, w->a, w->b); }

TEST(xcan_lock_sync_cancelsynchronousio) {
    NEED_LOCK(); REQUIRE(U(CancelSynchronousIo));
    xlockf f; CHECK(xlock_open(&f, TRUE)); CHECK(xlock_hold(f.hold, LOFF, LLEN));
    xwork *w = xwork_new(op_sync_lock); w->h = f.wait; w->h2 = f.hold; w->a = LOFF; w->b = LLEN;
    HANDLE t = xwork_start(w);
    DWORD last = 0; int tries = 0;
    BOOL got = xcancel_sync_retry(t, w, 5000, &last, &tries);
    CHECKF(got, "CancelSynchronousIo never TRUE (last err %lu, tries %d, state %ld)", last, tries, w->state);
    CHECK(xwork_join(t, w, 5000, unblock_lock));
    CHECK(!w->r); EXPECT_DW(w->err, ERROR_OPERATION_ABORTED);
    f.wait = w->h; xwork_free(w);
    xunlock(f.hold, LOFF, LLEN);
    CHECK(xrange_free(f.path, LOFF, LLEN));
    xlock_close(&f);
}

TEST(xcan_lock_sync_cancel_then_waiter_relocks) {
    NEED_LOCK(); REQUIRE(U(CancelSynchronousIo));
    xlockf f; CHECK(xlock_open(&f, TRUE)); CHECK(xlock_hold(f.hold, LOFF, LLEN));
    xwork *w = xwork_new(op_sync_lock); w->h = f.wait; w->h2 = f.hold; w->a = LOFF; w->b = LLEN;
    HANDLE t = xwork_start(w);
    CHECK(xcancel_sync_retry(t, w, 5000, NULL, NULL));
    CHECK(xwork_join(t, w, 5000, unblock_lock));
    CHECK(!w->r);
    f.wait = w->h;
    CHECK(xunlock(f.hold, LOFF, LLEN));
    /* the same synchronous handle can wait for and get the lock again */
    CHECK(xlock_hold(f.hold, LOFF, LLEN));
    w->state = XS_IDLE; t = xwork_start(w);
    CHECK(xstill_blocked(w, 100));
    CHECK(xunlock(f.hold, LOFF, LLEN));
    CHECK(xwork_join(t, w, 5000, NULL));
    CHECKF(w->r, "relock err %lu", w->err);
    CHECK(xunlock(f.wait, LOFF, LLEN));
    xwork_free(w); xlock_close(&f);
}

TEST(xcan_lock_sync_cancelio_other_thread_noop) {
    NEED_LOCK(); REQUIRE(U(CancelSynchronousIo));
    xlockf f; CHECK(xlock_open(&f, TRUE)); CHECK(xlock_hold(f.hold, LOFF, LLEN));
    xwork *w = xwork_new(op_sync_lock); w->h = f.wait; w->h2 = f.hold; w->a = LOFF; w->b = LLEN;
    HANDLE t = xwork_start(w);
    CHECK(xstill_blocked(w, 150));
    SetLastError(0xBEEF); CHECK(U(CancelIo)(f.wait));     /* only this thread's requests */
    CHECK(xstill_blocked(w, 150));
    CHECK(xcancel_sync_retry(t, w, 5000, NULL, NULL));
    CHECK(xwork_join(t, w, 5000, unblock_lock));
    CHECK(!w->r); EXPECT_DW(w->err, ERROR_OPERATION_ABORTED);
    f.wait = w->h; xwork_free(w); xlock_close(&f);
}

TEST(xcan_lock_sync_cancelioex_null_from_other_thread) {
    NEED_LOCK();
    xlockf f; CHECK(xlock_open(&f, TRUE)); CHECK(xlock_hold(f.hold, LOFF, LLEN));
    xwork *w = xwork_new(op_sync_lock); w->h = f.wait; w->h2 = f.hold; w->a = LOFF; w->b = LLEN;
    HANDLE t = xwork_start(w);
    CHECK(xstill_blocked(w, 150));
    SetLastError(0xBEEF); BOOL r = U(CancelIoEx)(f.wait, NULL); DWORD e = GetLastError();
    CHECKF(r, "CancelIoEx(sync handle, NULL) err %lu", e);
    CHECK(xwork_join(t, w, 5000, unblock_lock));
    CHECK(!w->r); EXPECT_DW(w->err, ERROR_OPERATION_ABORTED);
    f.wait = w->h; xwork_free(w); xlock_close(&f);
}

static BOOL op_ov_lock_then_wait(xwork *w) {
    OVERLAPPED *ov = w->p; SetLastError(0);
    BOOL r = U(LockFileEx)(w->h, LOCKFILE_EXCLUSIVE_LOCK, 0, LLEN, 0, ov);
    w->n = r ? 0 : GetLastError();
    WaitForSingleObject(w->h2, 5000);   /* a non-I/O wait, ended by the test */
    SetLastError(w->n); return FALSE;
}
TEST(xcan_lock_cancelsyncio_ignores_overlapped_request) {
    NEED_LOCK(); REQUIRE(U(CancelSynchronousIo));
    xlockf f; CHECK(xlock_open(&f, FALSE)); CHECK(xlock_hold(f.hold, LOFF, LLEN));
    HANDLE ev = xev(), go = xev(); OVERLAPPED ov; xov(&ov, LOFF, ev);
    xwork *w = xwork_new(op_ov_lock_then_wait); w->h = f.wait; w->p = &ov; w->h2 = go;
    HANDLE t = xwork_start(w);
    for (int i = 0; i < 200 && w->n == 0; i++) Sleep(10);
    Sleep(20);
    CHECK_PENDING(&ov);
    CHECK_FAILS(U(CancelSynchronousIo)(t), ERROR_NOT_FOUND);
    Sleep(50); CHECK(!xsig(ev)); CHECK_PENDING(&ov);
    SetEvent(go);
    CHECK(xwork_join(t, w, 5000, NULL));
    EXPECT_DW(w->n, ERROR_IO_PENDING);
    lk_drain(&f, &ov);
    xwork_free(w); CloseHandle(go); CloseHandle(ev); xlock_close(&f);
}
