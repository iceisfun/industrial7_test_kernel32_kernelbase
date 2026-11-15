/* What a cancel reaches: requests from all threads, duplicated versus re-opened handles, requests
 * whose issuing thread exits (with and without a completion port), and handle validation. */
#define XC_TU "sc"
#include "xc.h"

/* A worker that issues one overlapped request, then stays alive until told to exit. */
typedef struct { HANDLE h; OVERLAPPED ov; BYTE b[16]; HANDLE go, issued; int kind; BOOL pend; DWORD off; } issuer;
enum { K_READ, K_LOCK };
static DWORD WINAPI issuer_thread(void *p) {
    issuer *i = p; SetLastError(0); BOOL r;
    if (i->kind == K_READ) r = U(ReadFile)(i->h, i->b, sizeof i->b, NULL, &i->ov);
    else r = U(LockFileEx)(i->h, LOCKFILE_EXCLUSIVE_LOCK, 0, 10, 0, &i->ov);
    i->pend = !r && GetLastError() == ERROR_IO_PENDING;
    SetEvent(i->issued);
    if (i->go) WaitForSingleObject(i->go, 10000);
    return 0;
}
static HANDLE issue_on_thread(issuer *i, HANDLE h, int kind, DWORD off, HANDLE go) {
    memset(i, 0, sizeof *i); i->h = h; i->kind = kind; i->go = go; i->issued = xev();
    xov(&i->ov, off, xev());
    HANDLE t = CreateThread(NULL, 0, issuer_thread, i, 0, NULL);
    WaitForSingleObject(i->issued, 5000);
    return t;
}
static void issuer_end(issuer *i, HANDLE t) {
    if (ovst(&i->ov) == NTS_PENDING) { U(CancelIoEx)(i->h, &i->ov); xfin(i->h, &i->ov, 3000); }
    if (t) { xwait(t, 10000); CloseHandle(t); }
    CloseHandle(i->ov.hEvent); CloseHandle(i->issued);
}

TEST(xcan_scope_cancelioex_null_reaches_all_threads) {
    REQUIRE(U(CancelIoEx));
    HANDLE s, c; CHECK(xpipe(&s, &c)); HANDLE go = xev();
    issuer is[3]; HANDLE t[3];
    for (int i = 0; i < 3; i++) { t[i] = issue_on_thread(&is[i], s, K_READ, 0, go); CHECK(is[i].pend); }
    CHECK(U(CancelIoEx)(s, NULL));
    for (int i = 0; i < 3; i++) { CHECK_ABORTED(xfin(s, &is[i].ov, 5000)); }
    /* the issuers are still running: the cancel did not need them to exit */
    for (int i = 0; i < 3; i++) CHECK(WaitForSingleObject(t[i], 0) == WAIT_TIMEOUT);
    SetEvent(go);
    for (int i = 0; i < 3; i++) issuer_end(&is[i], t[i]);
    CloseHandle(go); xclose(&s); xclose(&c);
}

TEST(xcan_scope_cancelioex_null_locks_from_two_threads) {
    REQUIRE(U(CancelIoEx)); REQUIRE(U(LockFileEx));
    xlockf f; CHECK(xlock_open(&f, FALSE)); CHECK(xlock_hold(f.hold, 0, 100));
    HANDLE go = xev(); issuer a, b;
    HANDLE ta = issue_on_thread(&a, f.wait, K_LOCK, 0, go), tb = issue_on_thread(&b, f.wait, K_LOCK, 50, go);
    CHECK(a.pend && b.pend);
    CHECK(U(CancelIoEx)(f.wait, NULL));
    CHECK_ABORTED(xfin(f.wait, &a.ov, 5000)); CHECK_ABORTED(xfin(f.wait, &b.ov, 5000));
    CHECK(xunlock(f.hold, 0, 100)); CHECK(xrange_free(f.path, 0, 100));
    SetEvent(go); issuer_end(&a, ta); issuer_end(&b, tb);
    CloseHandle(go); xlock_close(&f);
}

TEST(xcan_scope_cancelio_other_thread_requests_untouched) {
    REQUIRE(U(CancelIo));
    HANDLE s, c; CHECK(xpipe(&s, &c)); HANDLE go = xev(); issuer is;
    HANDLE t = issue_on_thread(&is, s, K_READ, 0, go); CHECK(is.pend);
    HANDLE ev = xev(); OVERLAPPED mine; xov(&mine, 0, ev); BYTE b[8];
    CHECK(xread_pend(s, b, sizeof b, &mine));
    CHECK(U(CancelIo)(s));
    CHECK_ABORTED(xfin(s, &mine, 5000));
    CHECK(WaitForSingleObject(is.ov.hEvent, 100) == WAIT_TIMEOUT); CHECK_PENDING(&is.ov);
    SetEvent(go); issuer_end(&is, t);
    CloseHandle(ev); CloseHandle(go); xclose(&s); xclose(&c);
}

/* ---- duplicated and re-opened handles ---- */
static HANDLE dup_of(HANDLE h) { HANDLE d = NULL; U(DuplicateHandle)(GetCurrentProcess(), h, GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS); return d; }

TEST(xcan_scope_dup_handle_cancels_original_requests) {
    REQUIRE(U(CancelIoEx)); REQUIRE(U(DuplicateHandle)); REQUIRE(U(LockFileEx));
    xlockf f; CHECK(xlock_open(&f, FALSE)); CHECK(xlock_hold(f.hold, 0, 10));
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev);
    CHECK_IOPEND(U(LockFileEx)(f.wait, LOCKFILE_EXCLUSIVE_LOCK, 0, 10, 0, &ov));
    HANDLE d = dup_of(f.wait); CHECK(d != NULL);
    CHECK(U(CancelIoEx)(d, NULL));
    CHECK_ABORTED(xfin(f.wait, &ov, 5000));
    xclose(&d); CloseHandle(ev); xlock_close(&f);
}

TEST(xcan_scope_dup_handle_cancels_specific_overlapped) {
    REQUIRE(U(CancelIoEx)); REQUIRE(U(DuplicateHandle));
    HANDLE s, c; CHECK(xpipe(&s, &c));
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev); BYTE b[8];
    CHECK(xread_pend(s, b, sizeof b, &ov));
    HANDLE d = dup_of(s); CHECK(d != NULL);
    CHECK(U(CancelIoEx)(d, &ov));
    CHECK_ABORTED(xfin(s, &ov, 5000));
    xclose(&d); CloseHandle(ev); xclose(&s); xclose(&c);
}

TEST(xcan_scope_original_cancels_requests_issued_via_dup) {
    REQUIRE(U(CancelIoEx)); REQUIRE(U(DuplicateHandle));
    HANDLE s, c; CHECK(xpipe(&s, &c));
    HANDLE d = dup_of(s); CHECK(d != NULL);
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev); BYTE b[8];
    CHECK(xread_pend(d, b, sizeof b, &ov));
    CHECK(U(CancelIoEx)(s, NULL));
    CHECK_ABORTED(xfin(d, &ov, 5000));
    xclose(&d); CloseHandle(ev); xclose(&s); xclose(&c);
}

TEST(xcan_scope_cancelio_via_dup_same_thread) {
    REQUIRE(U(CancelIo)); REQUIRE(U(DuplicateHandle));
    HANDLE s, c; CHECK(xpipe(&s, &c));
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev); BYTE b[8];
    CHECK(xread_pend(s, b, sizeof b, &ov));
    HANDLE d = dup_of(s);
    CHECK(U(CancelIo)(d));
    CHECK_ABORTED(xfin(s, &ov, 5000));
    xclose(&d); CloseHandle(ev); xclose(&s); xclose(&c);
}

TEST(xcan_scope_close_dup_keeps_request_pending) {
    REQUIRE(U(DuplicateHandle)); REQUIRE(U(CancelIoEx));
    HANDLE s, c; CHECK(xpipe(&s, &c));
    HANDLE d = dup_of(s);
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev); BYTE b[8];
    CHECK(xread_pend(d, b, sizeof b, &ov));
    xclose(&d);   /* another handle to the same file object stays open */
    CHECK(WaitForSingleObject(ev, 150) == WAIT_TIMEOUT); CHECK_PENDING(&ov);
    CHECK(xwrite_all(c, "dup", 3));
    xres x = xfin(s, &ov, 5000); CHECK(x.ok && x.n == 3);
    CloseHandle(ev); xclose(&s); xclose(&c);
}

TEST(xcan_scope_reopenfile_handle_does_not_cancel) {
    REQUIRE(U(CancelIoEx)); REQUIRE(U(ReOpenFile)); REQUIRE(U(LockFileEx));
    xlockf f; CHECK(xlock_open(&f, FALSE)); CHECK(xlock_hold(f.hold, 0, 10));
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev);
    CHECK_IOPEND(U(LockFileEx)(f.wait, LOCKFILE_EXCLUSIVE_LOCK, 0, 10, 0, &ov));
    HANDLE r = U(ReOpenFile)(f.wait, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_FLAG_OVERLAPPED);
    CHECKF(r != INVALID_HANDLE_VALUE, "err %lu", GetLastError());
    /* a new file object: nothing of its own is pending */
    CHECK_FAILS(U(CancelIoEx)(r, NULL), ERROR_NOT_FOUND);
    CHECK_FAILS(U(CancelIoEx)(r, &ov), ERROR_NOT_FOUND);
    CHECK(WaitForSingleObject(ev, 50) == WAIT_TIMEOUT); CHECK_PENDING(&ov);
    CHECK(U(CancelIoEx)(f.wait, &ov)); CHECK_ABORTED(xfin(f.wait, &ov, 5000));
    xclose(&r); CloseHandle(ev); xlock_close(&f);
}

TEST(xcan_scope_reopenfile_requests_not_cancelled_by_original) {
    REQUIRE(U(CancelIoEx)); REQUIRE(U(ReOpenFile)); REQUIRE(U(LockFileEx));
    xlockf f; CHECK(xlock_open(&f, FALSE)); CHECK(xlock_hold(f.hold, 0, 10));
    HANDLE r = U(ReOpenFile)(f.wait, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_FLAG_OVERLAPPED);
    CHECK(r != INVALID_HANDLE_VALUE);
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev);
    CHECK_IOPEND(U(LockFileEx)(r, LOCKFILE_EXCLUSIVE_LOCK, 0, 10, 0, &ov));
    CHECK_FAILS(U(CancelIoEx)(f.wait, NULL), ERROR_NOT_FOUND);
    CHECK(WaitForSingleObject(ev, 50) == WAIT_TIMEOUT);
    CHECK(U(CancelIoEx)(r, NULL)); CHECK_ABORTED(xfin(r, &ov, 5000));
    xclose(&r); CloseHandle(ev); xlock_close(&f);
}

TEST(xcan_scope_reopenfile_close_original_keeps_reopened_request) {
    REQUIRE(U(ReOpenFile)); REQUIRE(U(LockFileEx));
    xlockf f; CHECK(xlock_open(&f, FALSE)); CHECK(xlock_hold(f.hold, 0, 10));
    HANDLE r = U(ReOpenFile)(f.wait, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_FLAG_OVERLAPPED);
    CHECK(r != INVALID_HANDLE_VALUE);
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev);
    CHECK_IOPEND(U(LockFileEx)(r, LOCKFILE_EXCLUSIVE_LOCK, 0, 10, 0, &ov));
    xclose(&f.wait);
    CHECK(WaitForSingleObject(ev, 100) == WAIT_TIMEOUT); CHECK_PENDING(&ov);
    CHECK(xunlock(f.hold, 0, 10));
    CHECK(xfin(r, &ov, 5000).ok);
    CHECK(xunlock(r, 0, 10));
    xclose(&r); CloseHandle(ev); xlock_close(&f);
}

/* ---- issuing thread exits ---- */
static DWORD WINAPI issue_read_and_exit(void *p) {
    void **a = p; SetLastError(0);
    BOOL r = U(ReadFile)((HANDLE)a[0], a[2], 8, NULL, (OVERLAPPED *)a[1]);
    return !r && GetLastError() == ERROR_IO_PENDING;
}
TEST(xcan_scope_thread_exit_cancels_pipe_read) {
    REQUIRE(U(ReadFile));
    HANDLE s, c; CHECK(xpipe(&s, &c));
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev); BYTE b[8];
    void *a[3] = {s, &ov, b};
    HANDLE t = CreateThread(NULL, 0, issue_read_and_exit, a, 0, NULL);
    CHECK(xwait(t, 5000)); DWORD rc = 0; GetExitCodeThread(t, &rc); CloseHandle(t); EXPECT_DW(rc, 1);
    CHECK(xwait(ev, 3000));
    EXPECT_DW(ovst(&ov), NTS_CANCELLED);
    CHECK_ABORTED(xnow(s, &ov));
    if (ovst(&ov) == NTS_PENDING) { U(CancelIoEx)(s, &ov); xfin(s, &ov, 3000); }
    CloseHandle(ev); xclose(&s); xclose(&c);
}

TEST(xcan_scope_thread_exit_iocp_read_survives) {
    REQUIRE(U(ReadFile)); REQUIRE(U(CreateIoCompletionPort)); REQUIRE(U(GetQueuedCompletionStatus));
    HANDLE s, c; CHECK(xpipe(&s, &c));
    HANDLE port = U(CreateIoCompletionPort)(s, NULL, 0xE1, 0);
    OVERLAPPED ov; xov(&ov, 0, NULL); BYTE b[8];
    void *a[3] = {s, &ov, b};
    HANDLE t = CreateThread(NULL, 0, issue_read_and_exit, a, 0, NULL);
    CHECK(xwait(t, 5000)); DWORD rc = 0; GetExitCodeThread(t, &rc); CloseHandle(t); EXPECT_DW(rc, 1);
    Sleep(100);
    /* a request on a completion-port handle is not tied to the issuing thread */
    CHECK_PENDING(&ov);
    CHECK(xwrite_all(c, "alive", 5));
    DWORD n = 0; ULONG_PTR k = 0; OVERLAPPED *po = NULL; SetLastError(0);
    BOOL r = U(GetQueuedCompletionStatus)(port, &n, &k, &po, 5000);
    CHECKF(r && n == 5 && po == &ov && k == 0xE1, "r=%d err=%lu n=%lu", r, GetLastError(), n);
    if (ovst(&ov) == NTS_PENDING) { U(CancelIoEx)(s, &ov); U(GetQueuedCompletionStatus)(port, &n, &k, &po, 3000); }
    CloseHandle(port); xclose(&s); xclose(&c);
}

TEST(xcan_scope_thread_exit_iocp_read_cancellable_later) {
    REQUIRE(U(ReadFile)); REQUIRE(U(CreateIoCompletionPort)); REQUIRE(U(GetQueuedCompletionStatus)); REQUIRE(U(CancelIoEx));
    HANDLE s, c; CHECK(xpipe(&s, &c));
    HANDLE port = U(CreateIoCompletionPort)(s, NULL, 0xE2, 0);
    OVERLAPPED ov; xov(&ov, 0, NULL); BYTE b[8];
    void *a[3] = {s, &ov, b};
    HANDLE t = CreateThread(NULL, 0, issue_read_and_exit, a, 0, NULL);
    CHECK(xwait(t, 5000)); CloseHandle(t);
    Sleep(50);
    CHECK(U(CancelIoEx)(s, &ov));
    DWORD n = 9; ULONG_PTR k = 0; OVERLAPPED *po = NULL; SetLastError(0);
    CHECK(!U(GetQueuedCompletionStatus)(port, &n, &k, &po, 5000)); EXPECT_ERR(ERROR_OPERATION_ABORTED);
    EXPECT_EQ(po, &ov); EXPECT_EQ(k, 0xE2); EXPECT_DW(n, 0);
    CloseHandle(port); xclose(&s); xclose(&c);
}

/* ---- validation ---- */
TEST(xcan_scope_cancel_non_file_handles_invalid) {
    REQUIRE(U(CancelIoEx)); REQUIRE(U(CancelIo));
    HANDLE ev = xev();
    HANDLE port = U(CreateIoCompletionPort)(INVALID_HANDLE_VALUE, NULL, 0, 1);
    HANDLE th = OpenThread(THREAD_ALL_ACCESS, FALSE, GetCurrentThreadId());
    HANDLE hs[4] = {ev, port, th, GetCurrentProcess()};
    for (int i = 0; i < 4; i++) {
        OVERLAPPED ov; xov(&ov, 0, NULL);
        SetLastError(0); BOOL r = U(CancelIoEx)(hs[i], NULL); DWORD e = GetLastError();
        CHECKF(!r && e == ERROR_INVALID_HANDLE, "CancelIoEx(NULL) on #%d: r=%d err=%lu", i, r, e);
        SetLastError(0); r = U(CancelIoEx)(hs[i], &ov); e = GetLastError();
        CHECKF(!r && e == ERROR_INVALID_HANDLE, "CancelIoEx(&ov) on #%d: r=%d err=%lu", i, r, e);
        SetLastError(0); r = U(CancelIo)(hs[i]); e = GetLastError();
        CHECKF(!r && e == ERROR_INVALID_HANDLE, "CancelIo on #%d: r=%d err=%lu", i, r, e);
    }
    CloseHandle(th); CloseHandle(port); CloseHandle(ev);
}

TEST(xcan_scope_cancel_closed_handle_invalid) {
    REQUIRE(U(CancelIoEx)); REQUIRE(U(CancelIo));
    char p[MAX_PATH]; xpath(p, "cl");
    HANDLE h = xopen(p, TRUE, CREATE_ALWAYS); CHECK(h != INVALID_HANDLE_VALUE);
    HANDLE stale = h; xclose(&h);
    OVERLAPPED ov; xov(&ov, 0, NULL);
    CHECK_FAILS(U(CancelIoEx)(stale, NULL), ERROR_INVALID_HANDLE);
    CHECK_FAILS(U(CancelIoEx)(stale, &ov), ERROR_INVALID_HANDLE);
    CHECK_FAILS(U(CancelIo)(stale), ERROR_INVALID_HANDLE);
    DeleteFileA(p);
}

TEST(xcan_scope_cancel_file_nothing_pending) {
    REQUIRE(U(CancelIoEx)); REQUIRE(U(CancelIo));
    char p[MAX_PATH]; xpath(p, "np");
    HANDLE a = xopen(p, TRUE, CREATE_ALWAYS), b = xopen(p, FALSE, OPEN_EXISTING);
    CHECK(a != INVALID_HANDLE_VALUE && b != INVALID_HANDLE_VALUE);
    OVERLAPPED ov; xov(&ov, 0, NULL);
    CHECK_FAILS(U(CancelIoEx)(a, NULL), ERROR_NOT_FOUND);
    CHECK_FAILS(U(CancelIoEx)(b, &ov), ERROR_NOT_FOUND);
    SetLastError(0xBEEF); CHECK(U(CancelIo)(a)); SetLastError(0xBEEF); CHECK(U(CancelIo)(b));
    xclose(&a); xclose(&b); DeleteFileA(p);
}

TEST(xcan_scope_cancel_other_files_overlapped_not_found) {
    REQUIRE(U(CancelIoEx)); REQUIRE(U(LockFileEx));
    xlockf f; CHECK(xlock_open(&f, FALSE)); CHECK(xlock_hold(f.hold, 0, 10));
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev);
    CHECK_IOPEND(U(LockFileEx)(f.wait, LOCKFILE_EXCLUSIVE_LOCK, 0, 10, 0, &ov));
    /* the holder handle is the same file but another file object */
    CHECK_FAILS(U(CancelIoEx)(f.hold, &ov), ERROR_NOT_FOUND);
    CHECK_FAILS(U(CancelIoEx)(f.hold, NULL), ERROR_NOT_FOUND);
    CHECK(!xsig(ev)); CHECK_PENDING(&ov);
    CHECK(U(CancelIoEx)(f.wait, &ov)); CHECK_ABORTED(xfin(f.wait, &ov, 5000));
    CloseHandle(ev); xlock_close(&f);
}
