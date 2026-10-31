#define AIO_TU "pc"
#include "aio.h"

/* start a pending read on h (expects pending) */
static BOOL start_pending_read(HANDLE h, void *b, DWORD n, OVERLAPPED *ov) {
    SET_ERR(0); BOOL r = U(ReadFile)(h, b, n, NULL, ov);
    return !r && GetLastError() == ERROR_IO_PENDING;
}

TEST(aio_pipe_pending_read_state) {
    REQUIRE(U(ReadFile)); REQUIRE(U(GetOverlappedResult)); REQUIRE(U(GetOverlappedResultEx));
    HANDLE s, c; CHECK(make_pipe(&s, &c)); HANDLE ev = mkevent(); SetEvent(ev);
    OVERLAPPED ov; ovinit(&ov, 0, ev); BYTE b[64]; DWORD n = 99;
    SET_ERR(0);
    BOOL r = U(ReadFile)(s, b, 64, &n, &ov);
    CHECK(!r); EXPECT_ERR(ERROR_IO_PENDING); EXPECT_EQ(n, 0);
    EXPECT_EQ(ov.Internal, NTS_PENDING);
    CHECK(not_signaled(ev));    /* event reset when op starts */
    n = 123; SET_ERR(0);
    CHECK(!U(GetOverlappedResult)(s, &ov, &n, FALSE)); EXPECT_ERR(ERROR_IO_INCOMPLETE);
    n = 123; SET_ERR(0);
    CHECK(!U(GetOverlappedResultEx)(s, &ov, &n, 0, FALSE)); EXPECT_ERR(ERROR_IO_INCOMPLETE);
    SET_ERR(0);
    CHECK(!U(GetOverlappedResultEx)(s, &ov, &n, 50, FALSE)); EXPECT_ERR(WAIT_TIMEOUT);
    SET_ERR(0);
    CHECK(!U(GetOverlappedResultEx)(s, &ov, &n, 50, TRUE)); EXPECT_ERR(WAIT_TIMEOUT);
    /* complete */
    DWORD w; CHECK(WriteFile(c, "hello", 5, &w, NULL) || 1);
    OVERLAPPED wo; HANDLE we = mkevent(); ovinit(&wo, 0, we); WriteFile(c, "hello", 5, NULL, &wo);
    CHECK(WaitForSingleObject(ev, 5000) == WAIT_OBJECT_0);
    CHECK(U(GetOverlappedResult)(s, &ov, &n, FALSE)); EXPECT_EQ(n, 5);
    EXPECT_EQ((DWORD)ov.Internal, 0); EXPECT_EQ(ov.InternalHigh, 5);
    CHECK(!memcmp(b, "hello", 5));
    CloseHandle(we); CloseHandle(ev); closeh(&s); closeh(&c);
}

TEST(aio_pipe_gor_wait_blocks_until_write) {
    REQUIRE(U(ReadFile)); REQUIRE(U(GetOverlappedResult));
    HANDLE s, c; CHECK(make_pipe(&s, &c)); HANDLE ev = mkevent();
    OVERLAPPED ov; ovinit(&ov, 0, ev); BYTE b[64];
    CHECK(start_pending_read(s, b, 64, &ov));
    delaywr d = {c, 150, "abcdefg", 7}; HANDLE t = CreateThread(NULL, 0, delaywr_thread, &d, 0, NULL);
    DWORD t0 = GetTickCount(), n = 0;
    CHECK(U(GetOverlappedResult)(s, &ov, &n, TRUE));
    DWORD dt = GetTickCount() - t0;
    EXPECT_EQ(n, 7); CHECKF(dt >= 100 && dt < 4000, "dt=%lu", dt);
    WaitForSingleObject(t, 5000); CloseHandle(t);
    CloseHandle(ev); closeh(&s); closeh(&c);
}

TEST(aio_pipe_gor_wait_no_event_uses_handle) {
    REQUIRE(U(ReadFile)); REQUIRE(U(GetOverlappedResult));
    HANDLE s, c; CHECK(make_pipe(&s, &c));
    OVERLAPPED ov; ovinit(&ov, 0, NULL); BYTE b[64];
    CHECK(start_pending_read(s, b, 64, &ov));
    delaywr d = {c, 100, "xyz", 3}; HANDLE t = CreateThread(NULL, 0, delaywr_thread, &d, 0, NULL);
    DWORD n = 0; CHECK(U(GetOverlappedResult)(s, &ov, &n, TRUE)); EXPECT_EQ(n, 3);
    WaitForSingleObject(t, 5000); CloseHandle(t); closeh(&s); closeh(&c);
}

TEST(aio_pipe_gorex_wait_completes) {
    REQUIRE(U(ReadFile)); REQUIRE(U(GetOverlappedResultEx));
    HANDLE s, c; CHECK(make_pipe(&s, &c)); HANDLE ev = mkevent();
    OVERLAPPED ov; ovinit(&ov, 0, ev); BYTE b[64];
    CHECK(start_pending_read(s, b, 64, &ov));
    delaywr d = {c, 100, "12345678", 8}; HANDLE t = CreateThread(NULL, 0, delaywr_thread, &d, 0, NULL);
    DWORD n = 0; CHECK(U(GetOverlappedResultEx)(s, &ov, &n, 5000, FALSE)); EXPECT_EQ(n, 8);
    WaitForSingleObject(t, 5000); CloseHandle(t); CloseHandle(ev); closeh(&s); closeh(&c);
}

static void CALLBACK noop_apc(ULONG_PTR p) { (*(volatile LONG *)p)++; }

TEST(aio_pipe_gorex_alertable_apc) {
    REQUIRE(U(ReadFile)); REQUIRE(U(GetOverlappedResultEx)); REQUIRE(U(QueueUserAPC));
    HANDLE s, c; CHECK(make_pipe(&s, &c)); HANDLE ev = mkevent();
    OVERLAPPED ov; ovinit(&ov, 0, ev); BYTE b[64]; volatile LONG hit = 0;
    CHECK(start_pending_read(s, b, 64, &ov));
    CHECK(U(QueueUserAPC)(noop_apc, GetCurrentThread(), (ULONG_PTR)&hit));
    DWORD n = 0; SET_ERR(0);
    CHECK(!U(GetOverlappedResultEx)(s, &ov, &n, 5000, TRUE)); EXPECT_ERR(WAIT_IO_COMPLETION);
    EXPECT_EQ(hit, 1);
    /* non-alertable: APC not delivered, plain timeout */
    CHECK(U(QueueUserAPC)(noop_apc, GetCurrentThread(), (ULONG_PTR)&hit));
    SET_ERR(0); CHECK(!U(GetOverlappedResultEx)(s, &ov, &n, 30, FALSE)); EXPECT_ERR(WAIT_TIMEOUT);
    EXPECT_EQ(hit, 1);
    SleepEx(0, TRUE); EXPECT_EQ(hit, 2);
    U(CancelIoEx)(s, &ov); GetOverlappedResult(s, &ov, &n, TRUE);
    CloseHandle(ev); closeh(&s); closeh(&c);
}

TEST(aio_pipe_partial_read_completes_with_available) {
    REQUIRE(U(ReadFile));
    HANDLE s, c; CHECK(make_pipe(&s, &c)); HANDLE ev = mkevent();
    OVERLAPPED ov; ovinit(&ov, 0, ev); BYTE b[100]; DWORD w;
    CHECK(start_pending_read(s, b, 100, &ov));
    OVERLAPPED wo; HANDLE we = mkevent(); ovinit(&wo, 0, we);
    iores x = do_write(c, "0123456789", 10, &wo); CHECK(x.ok); EXPECT_EQ(x.n, 10);
    DWORD n = 0; CHECK(GetOverlappedResultEx(s, &ov, &n, 5000, FALSE)); EXPECT_EQ(n, 10);
    CloseHandle(we); CloseHandle(ev); closeh(&s); closeh(&c);
}

TEST(aio_pipe_pending_reads_fifo_order) {
    REQUIRE(U(ReadFile));
    HANDLE s, c; CHECK(make_pipe(&s, &c));
    OVERLAPPED ov[3]; HANDLE ev[3]; BYTE b[3][4];
    for (int i = 0; i < 3; i++) { ev[i] = mkevent(); ovinit(&ov[i], 0, ev[i]); CHECK(start_pending_read(s, b[i], 4, &ov[i])); }
    const char *msgs[3] = {"AAAA", "BBBB", "CCCC"};
    HANDLE we = mkevent();
    for (int i = 0; i < 3; i++) {
        OVERLAPPED wo; ovinit(&wo, 0, we); ResetEvent(we);
        iores x = do_write(c, msgs[i], 4, &wo); CHECK(x.ok);
        CHECK(WaitForSingleObject(ev[i], 5000) == WAIT_OBJECT_0);
        CHECKF(!memcmp(b[i], msgs[i], 4), "read %d", i);
    }
    for (int i = 0; i < 3; i++) CloseHandle(ev[i]);
    CloseHandle(we); closeh(&s); closeh(&c);
}

TEST(aio_pipe_roundtrip_sizes) {
    REQUIRE(U(ReadFile)); REQUIRE(U(WriteFile));
    static const DWORD sz[] = {1, 2, 511, 4095, 4096, 4097, 20000, 65000};
    HANDLE s, c; CHECK(make_pipe(&s, &c)); HANDLE e1 = mkevent(), e2 = mkevent();
    BYTE *w = malloc(70000), *r = malloc(70000); fillp(w, 0, 70000);
    for (unsigned i = 0; i < sizeof sz / sizeof sz[0]; i++) {
        OVERLAPPED wo, ro; ovinit(&wo, 0, e1); ovinit(&ro, 0, e2); memset(r, 0, 70000);
        iores xw = do_write(c, w, sz[i], &wo);
        iores xr = do_read(s, r, sz[i], &ro);
        CHECKF(xw.ok && xw.n == sz[i] && xr.ok && xr.n == sz[i] && !memcmp(w, r, sz[i]), "size %lu", sz[i]);
    }
    free(w); free(r); CloseHandle(e1); CloseHandle(e2); closeh(&s); closeh(&c);
}

TEST(aio_pipe_write_exceeding_buffer_pending) {
    REQUIRE(U(WriteFile)); REQUIRE(U(ReadFile));
    HANDLE s, c; CHECK(make_pipe_sz(&s, &c, 4096)); HANDLE we = mkevent(), re = mkevent();
    DWORD N = 200000; BYTE *w = malloc(N), *r = malloc(N); fillp(w, 0, N);
    OVERLAPPED wo; ovinit(&wo, 0, we); SET_ERR(0);
    BOOL ok = U(WriteFile)(c, w, N, NULL, &wo);
    if (!ok) { EXPECT_ERR(ERROR_IO_PENDING); CHECK(not_signaled(we)); }
    else t_note("large pipe write completed immediately");
    /* drain */
    DWORD got = 0;
    while (got < N) {
        OVERLAPPED ro; ovinit(&ro, 0, re); iores x = do_read(s, r + got, N - got, &ro);
        if (!x.ok || !x.n) { CHECK(0); break; } got += x.n;
    }
    EXPECT_EQ(got, N); CHECK(!memcmp(r, w, N));
    DWORD n = 0; CHECK(GetOverlappedResultEx(c, &wo, &n, 5000, FALSE)); EXPECT_EQ(n, N);
    free(w); free(r); CloseHandle(we); CloseHandle(re); closeh(&s); closeh(&c);
}

TEST(aio_pipe_zero_byte_write_note) {
    REQUIRE(U(WriteFile));
    HANDLE s, c; CHECK(make_pipe(&s, &c)); HANDLE e = mkevent(); OVERLAPPED ov; ovinit(&ov, 0, e);
    iores x = do_write(c, "x", 0, &ov);
    t_note("zero-length pipe write: ok=%d err=%lu n=%lu", x.ok, x.err, x.n);
    CloseHandle(e); closeh(&s); closeh(&c);
}

TEST(aio_pipe_broken_pipe_on_pending_read) {
    REQUIRE(U(ReadFile));
    HANDLE s, c; CHECK(make_pipe(&s, &c)); HANDLE ev = mkevent();
    OVERLAPPED ov; ovinit(&ov, 0, ev); BYTE b[16];
    CHECK(start_pending_read(s, b, 16, &ov));
    closeh(&c);
    CHECK(WaitForSingleObject(ev, 5000) == WAIT_OBJECT_0);
    DWORD n = 7; SET_ERR(0);
    CHECK(!U(GetOverlappedResult)(s, &ov, &n, FALSE)); EXPECT_ERR(ERROR_BROKEN_PIPE); EXPECT_EQ(n, 0);
    EXPECT_EQ((DWORD)ov.Internal, NTS_BROKEN);
    /* new reads fail immediately */
    ovinit(&ov, 0, ev); SET_ERR(0);
    CHECK(!U(ReadFile)(s, b, 16, NULL, &ov)); EXPECT_ERR(ERROR_BROKEN_PIPE);
    CloseHandle(ev); closeh(&s);
}

TEST(aio_pipe_write_to_closed_peer) {
    REQUIRE(U(WriteFile));
    HANDLE s, c; CHECK(make_pipe(&s, &c)); HANDLE ev = mkevent();
    closeh(&c);
    OVERLAPPED ov; ovinit(&ov, 0, ev); SET_ERR(0);
    CHECK(!U(WriteFile)(s, "abc", 3, NULL, &ov));
    DWORD e = GetLastError(); CHECKF(e == ERROR_NO_DATA || e == ERROR_BROKEN_PIPE, "err %lu", e);
    CloseHandle(ev); closeh(&s);
}

TEST(aio_pipe_cancelioex_specific_then_all) {
    REQUIRE(U(ReadFile)); REQUIRE(U(CancelIoEx));
    HANDLE s, c; CHECK(make_pipe(&s, &c));
    OVERLAPPED o1, o2; HANDLE e1 = mkevent(), e2 = mkevent(); BYTE b1[8], b2[8];
    ovinit(&o1, 0, e1); ovinit(&o2, 0, e2);
    CHECK(start_pending_read(s, b1, 8, &o1)); CHECK(start_pending_read(s, b2, 8, &o2));
    SET_ERR(0); CHECK(U(CancelIoEx)(s, &o1));
    CHECK(WaitForSingleObject(e1, 5000) == WAIT_OBJECT_0);
    DWORD n = 5; SET_ERR(0);
    CHECK(!GetOverlappedResult(s, &o1, &n, FALSE)); EXPECT_ERR(ERROR_OPERATION_ABORTED); EXPECT_EQ(n, 0);
    EXPECT_EQ((DWORD)o1.Internal, NTS_CANCELLED);
    CHECK(not_signaled(e2)); SET_ERR(0);
    CHECK(!GetOverlappedResult(s, &o2, &n, FALSE)); EXPECT_ERR(ERROR_IO_INCOMPLETE);
    SET_ERR(0); CHECK(U(CancelIoEx)(s, NULL));
    CHECK(WaitForSingleObject(e2, 5000) == WAIT_OBJECT_0);
    SET_ERR(0); CHECK(!GetOverlappedResult(s, &o2, &n, FALSE)); EXPECT_ERR(ERROR_OPERATION_ABORTED);
    CloseHandle(e1); CloseHandle(e2); closeh(&s); closeh(&c);
}

TEST(aio_pipe_cancelioex_not_found) {
    REQUIRE(U(CancelIoEx));
    HANDLE s, c; CHECK(make_pipe(&s, &c));
    SET_ERR(0); CHECK(!U(CancelIoEx)(s, NULL)); EXPECT_ERR(ERROR_NOT_FOUND);
    OVERLAPPED ov; ovinit(&ov, 0, NULL);
    SET_ERR(0); CHECK(!U(CancelIoEx)(s, &ov)); EXPECT_ERR(ERROR_NOT_FOUND);
    closeh(&s); closeh(&c);
}

TEST(aio_pipe_cancelioex_after_completion_not_found) {
    REQUIRE(U(CancelIoEx));
    HANDLE s, c; CHECK(make_pipe(&s, &c)); HANDLE e1 = mkevent(), e2 = mkevent();
    OVERLAPPED wo, ro; BYTE b[4];
    ovinit(&wo, 0, e1); ovinit(&ro, 0, e2);
    CHECK(do_write(c, "1234", 4, &wo).ok); CHECK(do_read(s, b, 4, &ro).ok);
    SET_ERR(0); CHECK(!U(CancelIoEx)(s, &ro)); EXPECT_ERR(ERROR_NOT_FOUND);
    SET_ERR(0); CHECK(!U(CancelIoEx)(s, NULL)); EXPECT_ERR(ERROR_NOT_FOUND);
    CloseHandle(e1); CloseHandle(e2); closeh(&s); closeh(&c);
}

TEST(aio_pipe_cancelioex_wrong_handle) {
    REQUIRE(U(CancelIoEx)); REQUIRE(U(ReadFile));
    HANDLE s, c; CHECK(make_pipe(&s, &c)); HANDLE ev = mkevent();
    OVERLAPPED ov; ovinit(&ov, 0, ev); BYTE b[4];
    CHECK(start_pending_read(s, b, 4, &ov));
    SET_ERR(0); CHECK(!U(CancelIoEx)(c, &ov)); EXPECT_ERR(ERROR_NOT_FOUND);
    CHECK(not_signaled(ev));
    CHECK(U(CancelIoEx)(s, &ov));
    DWORD n; GetOverlappedResult(s, &ov, &n, TRUE);
    CloseHandle(ev); closeh(&s); closeh(&c);
}

TEST(aio_pipe_cancel_invalid_handles) {
    REQUIRE(U(CancelIoEx)); REQUIRE(U(CancelIo));
    OVERLAPPED ov; ovinit(&ov, 0, NULL);
    HANDLE bad[2] = {INVALID_HANDLE_VALUE, NULL};
    for (int i = 0; i < 2; i++) {
        SET_ERR(0); CHECK(!U(CancelIoEx)(bad[i], &ov)); EXPECT_ERR(ERROR_INVALID_HANDLE);
        SET_ERR(0); CHECK(!U(CancelIoEx)(bad[i], NULL)); EXPECT_ERR(ERROR_INVALID_HANDLE);
        SET_ERR(0); CHECK(!U(CancelIo)(bad[i])); EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
    HANDLE s, c; make_pipe(&s, &c); HANDLE stale = s; closeh(&s);
    SET_ERR(0); CHECK(!U(CancelIoEx)(stale, NULL)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(CancelIo)(stale)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    closeh(&c);
}

TEST(aio_pipe_cancelio_cancels_this_thread_ops) {
    REQUIRE(U(CancelIo)); REQUIRE(U(ReadFile));
    HANDLE s, c; CHECK(make_pipe(&s, &c));
    OVERLAPPED o1, o2; HANDLE e1 = mkevent(), e2 = mkevent(); BYTE b1[8], b2[8];
    ovinit(&o1, 0, e1); ovinit(&o2, 0, e2);
    CHECK(start_pending_read(s, b1, 8, &o1)); CHECK(start_pending_read(s, b2, 8, &o2));
    SET_ERR(0); CHECK(U(CancelIo)(s));
    CHECK(WaitForSingleObject(e1, 5000) == WAIT_OBJECT_0); CHECK(WaitForSingleObject(e2, 5000) == WAIT_OBJECT_0);
    DWORD n; SET_ERR(0); CHECK(!GetOverlappedResult(s, &o1, &n, FALSE)); EXPECT_ERR(ERROR_OPERATION_ABORTED);
    SET_ERR(0); CHECK(!GetOverlappedResult(s, &o2, &n, FALSE)); EXPECT_ERR(ERROR_OPERATION_ABORTED);
    CloseHandle(e1); CloseHandle(e2); closeh(&s); closeh(&c);
}

TEST(aio_pipe_cancelio_nothing_pending_succeeds) {
    REQUIRE(U(CancelIo));
    HANDLE s, c; CHECK(make_pipe(&s, &c));
    SET_ERR(0); CHECK(U(CancelIo)(s));
    CHECK(U(CancelIo)(c));
    closeh(&s); closeh(&c);
}

static DWORD WINAPI cancelio_other_thread(void *p) { return U(CancelIo)((HANDLE)p); }

TEST(aio_pipe_cancelio_other_thread_ops_note) {
    REQUIRE(U(CancelIo)); REQUIRE(U(ReadFile));
    HANDLE s, c; CHECK(make_pipe(&s, &c)); HANDLE ev = mkevent();
    OVERLAPPED ov; ovinit(&ov, 0, ev); BYTE b[8];
    CHECK(start_pending_read(s, b, 8, &ov));
    HANDLE t = CreateThread(NULL, 0, cancelio_other_thread, s, 0, NULL);
    WaitForSingleObject(t, 5000); DWORD rc = 0; GetExitCodeThread(t, &rc); CloseHandle(t);
    CHECK(rc != 0);   /* CancelIo reports success when it cancelled nothing */
    CHECK(not_signaled(ev));   /* issued by another thread: untouched */
    CHECK(U(CancelIoEx)(s, NULL));
    DWORD n; GetOverlappedResult(s, &ov, &n, TRUE);
    CloseHandle(ev); closeh(&s); closeh(&c);
}

static DWORD WINAPI cancelex_thread(void *p) { void **a = p; return U(CancelIoEx)((HANDLE)a[0], (OVERLAPPED *)a[1]); }
TEST(aio_pipe_cancelioex_from_other_thread) {
    REQUIRE(U(CancelIoEx)); REQUIRE(U(ReadFile));
    HANDLE s, c; CHECK(make_pipe(&s, &c)); HANDLE ev = mkevent();
    OVERLAPPED ov; ovinit(&ov, 0, ev); BYTE b[8];
    CHECK(start_pending_read(s, b, 8, &ov));
    void *args[2] = {s, &ov}; HANDLE t = CreateThread(NULL, 0, cancelex_thread, args, 0, NULL);
    WaitForSingleObject(t, 5000); DWORD rc = 0; GetExitCodeThread(t, &rc); CloseHandle(t);
    EXPECT_EQ(rc, 1);
    CHECK(WaitForSingleObject(ev, 5000) == WAIT_OBJECT_0);
    DWORD n; SET_ERR(0); CHECK(!GetOverlappedResult(s, &ov, &n, FALSE)); EXPECT_ERR(ERROR_OPERATION_ABORTED);
    CloseHandle(ev); closeh(&s); closeh(&c);
}

static HANDLE g_sync_r;
static volatile LONG g_sync_state;
static DWORD g_sync_err;
static DWORD WINAPI blocking_read_thread(void *p) {
    BYTE b[16]; DWORD n = 0;
    g_sync_state = 1;
    BOOL r = ReadFile(g_sync_r, b, 16, &n, NULL);
    g_sync_err = r ? 0 : GetLastError();
    g_sync_state = r ? 3 : 2;
    return 0;
}

TEST(aio_pipe_cancelsynchronousio_blocked_read) {
    REQUIRE(U(CancelSynchronousIo));
    HANDLE r, w; CHECK(CreatePipe(&r, &w, NULL, 0));
    g_sync_r = r; g_sync_state = 0; g_sync_err = 0;
    HANDLE t = CreateThread(NULL, 0, blocking_read_thread, NULL, 0, NULL);
    BOOL got = FALSE; DWORD last = 0;
    for (int i = 0; i < 500 && !got; i++) {
        Sleep(10);
        if (g_sync_state == 0) continue;
        SET_ERR(0); got = U(CancelSynchronousIo)(t); last = GetLastError();
    }
    CHECKF(got, "last error %lu", last);
    CHECK(WaitForSingleObject(t, 5000) == WAIT_OBJECT_0);
    EXPECT_EQ(g_sync_state, 2); EXPECT_EQ(g_sync_err, ERROR_OPERATION_ABORTED);
    CloseHandle(t); CloseHandle(r); CloseHandle(w);
}

TEST(aio_pipe_cancelsynchronousio_nothing_pending) {
    REQUIRE(U(CancelSynchronousIo));
    HANDLE t = OpenThread(THREAD_ALL_ACCESS, FALSE, GetCurrentThreadId());
    SET_ERR(0); CHECK(!U(CancelSynchronousIo)(t)); EXPECT_ERR(ERROR_NOT_FOUND);
    CloseHandle(t);
}

TEST(aio_pipe_cancelsynchronousio_invalid) {
    REQUIRE(U(CancelSynchronousIo));
    SET_ERR(0); CHECK(!U(CancelSynchronousIo)(NULL)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(CancelSynchronousIo)(INVALID_HANDLE_VALUE)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE ev = mkevent();
    SET_ERR(0); CHECK(!U(CancelSynchronousIo)(ev)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
    HANDLE t = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentThreadId());
    if (t) { SET_ERR(0); CHECK(!U(CancelSynchronousIo)(t)); EXPECT_ERR(ERROR_ACCESS_DENIED); CloseHandle(t); }
}

TEST(aio_pipe_close_handle_with_pending_read) {
    REQUIRE(U(ReadFile)); REQUIRE(U(GetOverlappedResult));
    HANDLE s, c; CHECK(make_pipe(&s, &c)); HANDLE ev = mkevent();
    OVERLAPPED ov; ovinit(&ov, 0, ev); BYTE b[8];
    CHECK(start_pending_read(s, b, 8, &ov));
    HANDLE stale = s; closeh(&s);
    CHECK(WaitForSingleObject(ev, 5000) == WAIT_OBJECT_0);
    DWORD n = 3; SET_ERR(0);
    CHECK(!U(GetOverlappedResult)(stale, &ov, &n, FALSE)); EXPECT_EQ(n, 0);
    { DWORD e = GetLastError(); CHECKF(e == ERROR_OPERATION_ABORTED || e == ERROR_BROKEN_PIPE, "err %lu", e);
      if (e != ERROR_OPERATION_ABORTED) t_note("closing own pipe handle with pending read completed with err %lu", e); }
    CloseHandle(ev); closeh(&c);
}

TEST(aio_pipe_server_disconnect_pending_read) {
    REQUIRE(U(ReadFile));
    HANDLE s, c; CHECK(make_pipe(&s, &c)); HANDLE ev = mkevent();
    OVERLAPPED ov; ovinit(&ov, 0, ev); BYTE b[8];
    CHECK(start_pending_read(c, b, 8, &ov));
    closeh(&s);
    CHECK(WaitForSingleObject(ev, 5000) == WAIT_OBJECT_0);
    DWORD n; SET_ERR(0); CHECK(!GetOverlappedResult(c, &ov, &n, FALSE)); EXPECT_ERR(ERROR_BROKEN_PIPE);
    CloseHandle(ev); closeh(&c);
}

TEST(aio_pipe_connectnamedpipe_overlapped_pending) {
    REQUIRE(U(ConnectNamedPipe));
    char name[128]; snprintf(name, sizeof name, "\\\\.\\pipe\\k32t_aio_cn_%lu", GetCurrentProcessId());
    HANDLE s = CreateNamedPipeA(name, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, NULL);
    CHECK(s != INVALID_HANDLE_VALUE); if (s == INVALID_HANDLE_VALUE) return;
    HANDLE ev = mkevent(); OVERLAPPED ov; ovinit(&ov, 0, ev);
    SET_ERR(0); BOOL r = U(ConnectNamedPipe)(s, &ov);
    CHECK(!r); EXPECT_ERR(ERROR_IO_PENDING); EXPECT_EQ(ov.Internal, NTS_PENDING);
    DWORD n; SET_ERR(0); CHECK(!GetOverlappedResult(s, &ov, &n, FALSE)); EXPECT_ERR(ERROR_IO_INCOMPLETE);
    HANDLE c = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(c != INVALID_HANDLE_VALUE);
    CHECK(WaitForSingleObject(ev, 5000) == WAIT_OBJECT_0);
    CHECK(GetOverlappedResult(s, &ov, &n, FALSE));
    /* now connected: another connect fails with ERROR_PIPE_CONNECTED */
    ovinit(&ov, 0, ev); SET_ERR(0);
    CHECK(!U(ConnectNamedPipe)(s, &ov)); EXPECT_ERR(ERROR_PIPE_CONNECTED);
    closeh(&c); CloseHandle(ev); closeh(&s);
}

TEST(aio_pipe_connect_pending_cancelled) {
    REQUIRE(U(ConnectNamedPipe)); REQUIRE(U(CancelIoEx));
    char name[128]; snprintf(name, sizeof name, "\\\\.\\pipe\\k32t_aio_cc_%lu", GetCurrentProcessId());
    HANDLE s = CreateNamedPipeA(name, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, NULL);
    CHECK(s != INVALID_HANDLE_VALUE); if (s == INVALID_HANDLE_VALUE) return;
    HANDLE ev = mkevent(); OVERLAPPED ov; ovinit(&ov, 0, ev);
    CHECK(!U(ConnectNamedPipe)(s, &ov)); EXPECT_ERR(ERROR_IO_PENDING);
    CHECK(U(CancelIoEx)(s, &ov));
    DWORD n; SET_ERR(0); CHECK(!GetOverlappedResultEx(s, &ov, &n, 5000, FALSE)); EXPECT_ERR(ERROR_OPERATION_ABORTED);
    CloseHandle(ev); closeh(&s);
}

TEST(aio_pipe_anonymous_sync_handles_with_overlapped) {
    REQUIRE(U(ReadFile));
    HANDLE r, w; CHECK(CreatePipe(&r, &w, NULL, 0)); DWORD n;
    CHECK(WriteFile(w, "abcdef", 6, &n, NULL));
    OVERLAPPED ov; ovinit(&ov, 0, NULL); BYTE b[16] = {0}; n = 0;
    BOOL ok = U(ReadFile)(r, b, 16, &n, &ov);
    CHECK(ok); EXPECT_EQ(n, 6); CHECK(!memcmp(b, "abcdef", 6));
    CloseHandle(r); CloseHandle(w);
}

TEST(aio_pipe_bytes_ptr_zeroed_on_pending) {
    REQUIRE(U(ReadFile));
    HANDLE s, c; CHECK(make_pipe(&s, &c)); HANDLE ev = mkevent(); OVERLAPPED ov; ovinit(&ov, 0, ev);
    BYTE b[4]; DWORD n = 0x1234;
    SET_ERR(0); BOOL r = U(ReadFile)(s, b, 4, &n, &ov);
    CHECK(!r); EXPECT_ERR(ERROR_IO_PENDING); EXPECT_EQ(n, 0);
    U(CancelIoEx)(s, &ov); GetOverlappedResult(s, &ov, &n, TRUE);
    CloseHandle(ev); closeh(&s); closeh(&c);
}

static DWORD WINAPI getres_thread(void *p) {
    void **a = p; DWORD n = 0;
    BOOL r = GetOverlappedResult((HANDLE)a[0], (OVERLAPPED *)a[1], &n, TRUE);
    return r ? n : 0xFFFFFFFF;
}

TEST(aio_pipe_gor_from_different_thread) {
    REQUIRE(U(ReadFile));
    HANDLE s, c; CHECK(make_pipe(&s, &c)); HANDLE ev = mkevent(); OVERLAPPED ov; ovinit(&ov, 0, ev); BYTE b[32];
    CHECK(start_pending_read(s, b, 32, &ov));
    void *args[2] = {s, &ov}; HANDLE t = CreateThread(NULL, 0, getres_thread, args, 0, NULL);
    Sleep(50); OVERLAPPED wo; HANDLE we = mkevent(); ovinit(&wo, 0, we);
    CHECK(do_write(c, "thread", 6, &wo).ok);
    CHECK(WaitForSingleObject(t, 5000) == WAIT_OBJECT_0);
    DWORD rc = 0; GetExitCodeThread(t, &rc); EXPECT_EQ(rc, 6);
    CloseHandle(t); CloseHandle(we); CloseHandle(ev); closeh(&s); closeh(&c);
}

static volatile LONG g_exit_state;
static DWORD WINAPI issue_and_exit(void *p) {
    void **a = p; BYTE b[8];
    ReadFile((HANDLE)a[0], b, 8, NULL, (OVERLAPPED *)a[1]);
    return 0;
}
TEST(aio_pipe_issuing_thread_exit_note) {
    REQUIRE(U(ReadFile));
    HANDLE s, c; CHECK(make_pipe(&s, &c)); HANDLE ev = mkevent(); OVERLAPPED ov; ovinit(&ov, 0, ev);
    void *args[2] = {s, &ov}; HANDLE t = CreateThread(NULL, 0, issue_and_exit, args, 0, NULL);
    WaitForSingleObject(t, 5000); CloseHandle(t);
    DWORD w = WaitForSingleObject(ev, 1000);
    t_note("pending pipe read after issuing thread exited: event %s, Internal=0x%lx", w == WAIT_OBJECT_0 ? "signalled (cancelled)" : "still pending", (DWORD)ov.Internal);
    if (w != WAIT_OBJECT_0) { U(CancelIoEx)(s, &ov); DWORD n; GetOverlappedResultEx(s, &ov, &n, 5000, FALSE); }
    CloseHandle(ev); closeh(&s); closeh(&c);
}

TEST(aio_pipe_fsctl_peek_note) {
    REQUIRE(U(DeviceIoControl));
    HANDLE s, c; CHECK(make_pipe(&s, &c)); HANDLE ev = mkevent(), we = mkevent();
    BYTE buf[sizeof(FILE_PIPE_PEEK_BUFFER_) + 64];
    OVERLAPPED wo; ovinit(&wo, 0, we); CHECK(do_write(c, "12345", 5, &wo).ok);
    OVERLAPPED ov; ovinit(&ov, 0, ev); DWORD n = 0; memset(buf, 0, sizeof buf);
    SET_ERR(0); BOOL r = U(DeviceIoControl)(s, 0x11400C /*FSCTL_PIPE_PEEK*/, NULL, 0, buf, sizeof buf, &n, &ov);
    DWORD e = GetLastError();
    if (!r && e == ERROR_IO_PENDING) { r = GetOverlappedResultEx(s, &ov, &n, 5000, FALSE); e = r ? 0 : GetLastError(); }
    FILE_PIPE_PEEK_BUFFER_ *pk = (void *)buf;
    t_note("DeviceIoControl(FSCTL_PIPE_PEEK): ret %d err %lu avail %lu", r, e, pk->ReadDataAvailable);
    if (r) EXPECT_EQ(pk->ReadDataAvailable, 5);
    CloseHandle(ev); CloseHandle(we); closeh(&s); closeh(&c);
}
