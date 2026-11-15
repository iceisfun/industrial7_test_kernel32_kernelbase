/* Pending overlapped directory watches (ReadDirectoryChangesW / ReadDirectoryChangesExW) cancelled
 * with and without changes queued; state of the notify buffer; close versus cancel. */
#define XC_TU "dr"
#include "xc.h"

#define NEED_RDC() REQUIRE(U(ReadDirectoryChangesW)); REQUIRE(U(CancelIoEx))
#define FILT FILE_NOTIFY_CHANGE_FILE_NAME

typedef struct { char dir[MAX_PATH]; HANDLE h; DWORD buf[FNI_BUF_DWORDS]; } xwatch;
static BOOL wopen(xwatch *w) { xmkdir(w->dir, "w"); w->h = xdiropen(w->dir, TRUE); memset(w->buf, 0xA5, sizeof w->buf); return w->h != INVALID_HANDLE_VALUE; }
static void wclose(xwatch *w) { xclose(&w->h); xrmtree1(w->dir); }
/* Issue an overlapped watch; returns 1 pending, 2 completed at once, 0 failure. */
static int wissue(xwatch *w, OVERLAPPED *ov, LPOVERLAPPED_COMPLETION_ROUTINE cr) {
    DWORD n = 0; SetLastError(0);
    BOOL r = U(ReadDirectoryChangesW)(w->h, w->buf, sizeof w->buf, FALSE, FILT, &n, ov, cr);
    if (r) return ovst(ov) == NTS_PENDING ? 1 : 2;
    return 0;
}
static int buf_untouched(const xwatch *w) {
    const BYTE *b = (const BYTE *)w->buf; for (size_t i = 0; i < sizeof w->buf; i++) if (b[i] != 0xA5) return 0; return 1;
}

TEST(xcan_dir_rdc_cancel_aborted_buffer_untouched) {
    NEED_RDC();
    xwatch w; CHECK(wopen(&w)); HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev);
    EXPECT_DW(wissue(&w, &ov, NULL), 1);
    CHECK(U(CancelIoEx)(w.h, &ov));
    CHECK_ABORTED(xfin(w.h, &ov, 5000));
    EXPECT_DW(ovst(&ov), NTS_CANCELLED); EXPECT_EQ(ov.InternalHigh, 0);
    CHECK(buf_untouched(&w));
    CloseHandle(ev); wclose(&w);
}

TEST(xcan_dir_rdcex_extended_cancel_aborted) {
    NEED_RDC(); REQUIRE(U(ReadDirectoryChangesExW));
    xwatch w; CHECK(wopen(&w)); HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev); DWORD n = 0;
    SetLastError(0);
    BOOL r = U(ReadDirectoryChangesExW)(w.h, w.buf, sizeof w.buf, FALSE, FILT, &n, &ov, NULL, ReadDirectoryNotifyExtendedInformation);
    CHECKF(r, "err %lu", GetLastError()); CHECK_PENDING(&ov);
    CHECK(U(CancelIoEx)(w.h, &ov));
    CHECK_ABORTED(xfin(w.h, &ov, 5000));
    CHECK(buf_untouched(&w));
    CloseHandle(ev); wclose(&w);
}

TEST(xcan_dir_rdc_change_after_cancel_reported_next) {
    NEED_RDC();
    xwatch w; CHECK(wopen(&w)); HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev);
    EXPECT_DW(wissue(&w, &ov, NULL), 1);
    CHECK(U(CancelIoEx)(w.h, &ov)); CHECK_ABORTED(xfin(w.h, &ov, 5000));
    xtouch(w.dir, "late.txt");
    Sleep(50);
    xov(&ov, 0, ev);
    int k = wissue(&w, &ov, NULL); CHECK(k != 0);
    xres x = xfin(w.h, &ov, 3000);
    CHECKF(x.ok && x.n > 0, "ok=%d err=%lu n=%lu", x.ok, x.err, x.n);
    char nm[64] = ""; DWORD act = x.ok && x.n ? xfni_first(w.buf, nm, sizeof nm) : 0;
    EXPECT_DW(act, FILE_ACTION_ADDED); EXPECT_STR(nm, "late.txt");
    if (!x.ok) { U(CancelIoEx)(w.h, &ov); xfin(w.h, &ov, 3000); }
    CloseHandle(ev); wclose(&w);
}

TEST(xcan_dir_rdc_queued_changes_complete_at_once_not_cancellable) {
    NEED_RDC();
    xwatch w; CHECK(wopen(&w)); HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev);
    EXPECT_DW(wissue(&w, &ov, NULL), 1);
    xtouch(w.dir, "first.txt");
    xres x = xfin(w.h, &ov, 5000); CHECK(x.ok && x.n > 0);
    /* no request outstanding: these changes are buffered by the watch */
    xtouch(w.dir, "second.txt"); xtouch(w.dir, "third.txt");
    Sleep(50);
    xov(&ov, 0, ev); memset(w.buf, 0xA5, sizeof w.buf);
    CHECK(wissue(&w, &ov, NULL) != 0);
    CHECK(xwait(ev, 2000));
    CHECK_FAILS(U(CancelIoEx)(w.h, &ov), ERROR_NOT_FOUND);
    x = xnow(w.h, &ov); CHECKF(x.ok && x.n > 0, "ok=%d err=%lu n=%lu", x.ok, x.err, x.n);
    char nm[64] = ""; if (x.ok && x.n) xfni_first(w.buf, nm, sizeof nm);
    EXPECT_STR(nm, "second.txt");
    CloseHandle(ev); wclose(&w);
}

TEST(xcan_dir_rdc_iocp_cancel_packet) {
    NEED_RDC(); REQUIRE(U(CreateIoCompletionPort)); REQUIRE(U(GetQueuedCompletionStatus));
    xwatch w; CHECK(wopen(&w));
    HANDLE port = U(CreateIoCompletionPort)(w.h, NULL, 0xD1, 0); CHECK(port != NULL);
    OVERLAPPED ov; xov(&ov, 0, NULL);
    EXPECT_DW(wissue(&w, &ov, NULL), 1);
    CHECK(U(CancelIoEx)(w.h, &ov));
    DWORD n = 9; ULONG_PTR k = 0; OVERLAPPED *po = NULL; SetLastError(0);
    CHECK(!U(GetQueuedCompletionStatus)(port, &n, &k, &po, 5000)); EXPECT_ERR(ERROR_OPERATION_ABORTED);
    EXPECT_DW(n, 0); EXPECT_EQ(k, 0xD1); EXPECT_EQ(po, &ov);
    SetLastError(0); CHECK(!U(GetQueuedCompletionStatus)(port, &n, &k, &po, 50)); EXPECT_ERR(WAIT_TIMEOUT);
    CHECK(buf_untouched(&w));
    CloseHandle(port); wclose(&w);
}

static volatile LONG g_rdc_calls; static DWORD g_rdc_err, g_rdc_n;
static void CALLBACK rdc_done(DWORD e, DWORD n, OVERLAPPED *ov) { g_rdc_err = e; g_rdc_n = n; InterlockedIncrement(&g_rdc_calls); }
TEST(xcan_dir_rdc_completion_routine_cancelled) {
    NEED_RDC();
    xwatch w; CHECK(wopen(&w)); OVERLAPPED ov; xov(&ov, 0, NULL);
    g_rdc_calls = 0; g_rdc_err = g_rdc_n = 0xBEEF;
    EXPECT_DW(wissue(&w, &ov, rdc_done), 1);
    CHECK(U(CancelIoEx)(w.h, &ov));
    Sleep(30); EXPECT_DW(g_rdc_calls, 0);
    EXPECT_DW(SleepEx(2000, TRUE), WAIT_IO_COMPLETION);
    EXPECT_DW(g_rdc_calls, 1); EXPECT_DW(g_rdc_err, ERROR_OPERATION_ABORTED); EXPECT_DW(g_rdc_n, 0);
    while (SleepEx(0, TRUE) == WAIT_IO_COMPLETION) {}
    wclose(&w);
}

TEST(xcan_dir_rdc_close_with_pending_watch) {
    NEED_RDC();
    xwatch w; CHECK(wopen(&w)); HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev);
    EXPECT_DW(wissue(&w, &ov, NULL), 1);
    HANDLE stale = w.h; xclose(&w.h);
    CHECK(xwait(ev, 5000));
    EXPECT_DW(ovst(&ov), NTS_NOTIFY_CLEANUP);
    xres x = xnow(stale, &ov);
    CHECKF(x.ok && x.n == 0, "ok=%d err=%lu n=%lu", x.ok, x.err, x.n);
    CHECK(buf_untouched(&w));
    CloseHandle(ev); xrmtree1(w.dir);
}

TEST(xcan_dir_rdc_two_pending_cancel_first) {
    NEED_RDC();
    xwatch w; CHECK(wopen(&w)); static DWORD buf2[FNI_BUF_DWORDS];
    HANDLE e1 = xev(), e2 = xev(); OVERLAPPED o1, o2; xov(&o1, 0, e1); xov(&o2, 0, e2);
    EXPECT_DW(wissue(&w, &o1, NULL), 1);
    DWORD n = 0;
    CHECK(U(ReadDirectoryChangesW)(w.h, buf2, sizeof buf2, FALSE, FILT, &n, &o2, NULL)); CHECK_PENDING(&o2);
    CHECK(U(CancelIoEx)(w.h, &o1));
    CHECK_ABORTED(xfin(w.h, &o1, 5000));
    CHECK(!xsig(e2)); CHECK_PENDING(&o2);
    xtouch(w.dir, "two.txt");
    xres x = xfin(w.h, &o2, 5000); CHECKF(x.ok && x.n > 0, "ok=%d err=%lu n=%lu", x.ok, x.err, x.n);
    char nm[64] = ""; if (x.ok && x.n) xfni_first(buf2, nm, sizeof nm);
    EXPECT_STR(nm, "two.txt");
    CHECK(buf_untouched(&w));
    if (!x.ok) { U(CancelIoEx)(w.h, NULL); xfin(w.h, &o2, 3000); }
    CloseHandle(e1); CloseHandle(e2); wclose(&w);
}

TEST(xcan_dir_rdc_cancelio_issuing_thread) {
    NEED_RDC(); REQUIRE(U(CancelIo));
    xwatch w; CHECK(wopen(&w)); HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev);
    EXPECT_DW(wissue(&w, &ov, NULL), 1);
    CHECK(U(CancelIo)(w.h));
    CHECK_ABORTED(xfin(w.h, &ov, 5000));
    CloseHandle(ev); wclose(&w);
}

TEST(xcan_dir_rdcex_reissue_after_cancel_extended_record) {
    NEED_RDC(); REQUIRE(U(ReadDirectoryChangesExW));
    xwatch w; CHECK(wopen(&w)); HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev); DWORD n = 0;
    CHECK(U(ReadDirectoryChangesExW)(w.h, w.buf, sizeof w.buf, FALSE, FILT, &n, &ov, NULL, ReadDirectoryNotifyExtendedInformation));
    CHECK(U(CancelIoEx)(w.h, &ov)); CHECK_ABORTED(xfin(w.h, &ov, 5000));
    xov(&ov, 0, ev);
    CHECK(U(ReadDirectoryChangesExW)(w.h, w.buf, sizeof w.buf, FALSE, FILT, &n, &ov, NULL, ReadDirectoryNotifyExtendedInformation));
    xtouch(w.dir, "ext.txt");
    xres x = xfin(w.h, &ov, 5000); CHECKF(x.ok && x.n > 0, "ok=%d err=%lu n=%lu", x.ok, x.err, x.n);
    if (x.ok && x.n >= sizeof(FILE_NOTIFY_EXTENDED_INFORMATION)) {
        FILE_NOTIFY_EXTENDED_INFORMATION *f = (void *)w.buf;
        EXPECT_DW(f->Action, FILE_ACTION_ADDED);
        EXPECT_DW(f->FileNameLength, 14);
        CHECK(f->FileNameLength == 14 && !memcmp(f->FileName, L"ext.txt", 14));
    }
    if (!x.ok) { U(CancelIoEx)(w.h, &ov); xfin(w.h, &ov, 3000); }
    CloseHandle(ev); wclose(&w);
}

TEST(xcan_dir_cancel_nothing_pending) {
    REQUIRE(U(CancelIoEx)); REQUIRE(U(CancelIo));
    xwatch w; CHECK(wopen(&w));
    CHECK_FAILS(U(CancelIoEx)(w.h, NULL), ERROR_NOT_FOUND);
    SetLastError(0xBEEF); CHECK(U(CancelIo)(w.h));
    wclose(&w);
}
