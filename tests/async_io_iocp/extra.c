#define AIO_TU "xt"
#include "aio.h"

/* ---- synthetic OVERLAPPED completion handling (no real IO) ---- */
typedef struct { OVERLAPPED *ov; HANDLE ev; DWORD delay; ULONG_PTR status; ULONG_PTR info; } setter;
static DWORD WINAPI setter_thread(void *p) {
    setter *s = p; Sleep(s->delay);
    s->ov->InternalHigh = s->info; s->ov->Internal = s->status; MemoryBarrier();
    SetEvent(s->ev); return 0;
}
static void CALLBACK count_apc(ULONG_PTR p) { InterlockedIncrement((volatile LONG *)p); }

TEST(aio_xt_gor_synthetic_pending_is_incomplete) {
    REQUIRE(U(GetOverlappedResult)); REQUIRE(U(GetOverlappedResultEx));
    OVERLAPPED ov; memset(&ov, 0, sizeof ov); ov.Internal = NTS_PENDING; ov.InternalHigh = 0x55;
    DWORD n = 77; SET_ERR(0);
    CHECK(!U(GetOverlappedResult)((HANDLE)(ULONG_PTR)0x1234, &ov, &n, FALSE)); EXPECT_ERR(ERROR_IO_INCOMPLETE);
    for (int a = 0; a < 2; a++) {
        SET_ERR(0); CHECKF(!U(GetOverlappedResultEx)((HANDLE)(ULONG_PTR)0x1234, &ov, &n, 0, a), "alertable %d", a);
        EXPECT_ERR(ERROR_IO_INCOMPLETE);
    }
    EXPECT_EQ(n, 77);   /* untouched on the incomplete path */
}

TEST(aio_xt_gorex_synthetic_wait_completes_via_event) {
    REQUIRE(U(GetOverlappedResultEx));
    HANDLE ev = mkevent(); OVERLAPPED ov; ovinit(&ov, 0, ev); ov.Internal = NTS_PENDING;
    setter s = {&ov, ev, 100, 0, 321};
    HANDLE t = CreateThread(NULL, 0, setter_thread, &s, 0, NULL);
    DWORD n = 0; SET_ERR(99);
    CHECK(U(GetOverlappedResultEx)((HANDLE)(ULONG_PTR)0x1234, &ov, &n, 5000, FALSE));
    EXPECT_EQ(n, 321); EXPECT_ERR(0);
    WaitForSingleObject(t, 5000); CloseHandle(t); CloseHandle(ev);
}

TEST(aio_xt_gor_synthetic_wait_reports_error_status) {
    REQUIRE(U(GetOverlappedResult));
    static const struct { ULONG_PTR st; DWORD err; } cases[] = {{NTS_EOF, ERROR_HANDLE_EOF}, {NTS_CANCELLED, ERROR_OPERATION_ABORTED}, {NTS_BROKEN, ERROR_BROKEN_PIPE}};
    for (unsigned i = 0; i < 3; i++) {
        HANDLE ev = mkevent(); OVERLAPPED ov; ovinit(&ov, 0, ev); ov.Internal = NTS_PENDING;
        setter s = {&ov, ev, 60, cases[i].st, 0};
        HANDLE t = CreateThread(NULL, 0, setter_thread, &s, 0, NULL);
        DWORD n = 9; SET_ERR(0);
        CHECKF(!U(GetOverlappedResult)((HANDLE)(ULONG_PTR)0x1234, &ov, &n, TRUE), "case %u", i);
        EXPECT_ERR(cases[i].err); EXPECT_EQ(n, 0);
        WaitForSingleObject(t, 5000); CloseHandle(t); CloseHandle(ev);
    }
}

TEST(aio_xt_gor_wait_on_handle_when_no_event_masks_low_bit) {
    REQUIRE(U(GetOverlappedResult));
    /* with hEvent == NULL the wait target is hFile; its low bit is masked off */
    HANDLE ev = mkevent(); OVERLAPPED ov; ovinit(&ov, 0, NULL); ov.Internal = NTS_PENDING;
    setter s = {&ov, ev, 80, 0, 11};
    HANDLE t = CreateThread(NULL, 0, setter_thread, &s, 0, NULL);
    DWORD n = 0; SET_ERR(0);
    CHECK(U(GetOverlappedResult)((HANDLE)((ULONG_PTR)ev | 1), &ov, &n, TRUE)); EXPECT_EQ(n, 11);
    WaitForSingleObject(t, 5000); CloseHandle(t); CloseHandle(ev);
}

TEST(aio_xt_gorex_synthetic_timeout_and_apc) {
    REQUIRE(U(GetOverlappedResultEx)); REQUIRE(U(QueueUserAPC));
    HANDLE ev = mkevent(); OVERLAPPED ov; ovinit(&ov, 0, ev); ov.Internal = NTS_PENDING;
    DWORD n = 0; SET_ERR(0);
    CHECK(!U(GetOverlappedResultEx)((HANDLE)(ULONG_PTR)0x1234, &ov, &n, 40, FALSE)); EXPECT_ERR(WAIT_TIMEOUT);
    volatile LONG hit = 0;
    CHECK(U(QueueUserAPC)(count_apc, GetCurrentThread(), (ULONG_PTR)&hit));
    SET_ERR(0); CHECK(!U(GetOverlappedResultEx)((HANDLE)(ULONG_PTR)0x1234, &ov, &n, 5000, TRUE)); EXPECT_ERR(WAIT_IO_COMPLETION);
    EXPECT_EQ(hit, 1);
    CloseHandle(ev);
}

/* ---- cancel on files ---- */
TEST(aio_xt_cancelioex_file_nothing_to_cancel) {
    REQUIRE(U(CancelIoEx));
    char p[MAX_PATH]; mkpath(p, "cx"); make_file(p, 8192);
    HANDLE h = open_ov(p, GENERIC_READ, 0), hs = open_sync(p, GENERIC_READ); HANDLE ev = mkevent();
    SET_ERR(0); CHECK(!U(CancelIoEx)(h, NULL)); EXPECT_ERR(ERROR_NOT_FOUND);
    SET_ERR(0); CHECK(!U(CancelIoEx)(hs, NULL)); EXPECT_ERR(ERROR_NOT_FOUND);
    OVERLAPPED ov; BYTE b[100]; ovinit(&ov, 0, ev);
    CHECK(do_read(h, b, 100, &ov).ok);
    SET_ERR(0); CHECK(!U(CancelIoEx)(h, &ov)); EXPECT_ERR(ERROR_NOT_FOUND);
    OVERLAPPED never; ovinit(&never, 0, NULL);
    SET_ERR(0); CHECK(!U(CancelIoEx)(h, &never)); EXPECT_ERR(ERROR_NOT_FOUND);
    CloseHandle(ev); CloseHandle(h); CloseHandle(hs);
}

TEST(aio_xt_cancelio_file_nothing_pending_succeeds) {
    REQUIRE(U(CancelIo));
    char p[MAX_PATH]; mkpath(p, "cio"); make_file(p, 4096);
    HANDLE h = open_ov(p, GENERIC_READ, 0), hs = open_sync(p, GENERIC_READ);
    SET_ERR(0); CHECK(U(CancelIo)(h)); CHECK(U(CancelIo)(hs));
    CloseHandle(h); CloseHandle(hs);
}

/* ---- association semantics ---- */
TEST(aio_xt_duplicate_handle_shares_association) {
    REQUIRE(U(CreateIoCompletionPort)); REQUIRE(U(ReadFile));
    char p[MAX_PATH]; mkpath(p, "dup"); make_file(p, 8192);
    HANDLE f = open_ov(p, GENERIC_READ, 0), d = NULL; HANDLE port = U(CreateIoCompletionPort)(INVALID_HANDLE_VALUE, NULL, 0, 0);
    CHECK(U(CreateIoCompletionPort)(f, port, 0x4D, 0) == port);
    CHECK(DuplicateHandle(GetCurrentProcess(), f, GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS));
    OVERLAPPED ov; ovinit(&ov, 64, NULL); BYTE b[100];
    BOOL r = U(ReadFile)(d, b, 100, NULL, &ov); CHECK(r || GetLastError() == ERROR_IO_PENDING);
    DWORD n; ULONG_PTR k; OVERLAPPED *o = NULL;
    CHECK(U(GetQueuedCompletionStatus)(port, &n, &k, &o, 5000));
    EXPECT_EQ(k, 0x4D); EXPECT_EQ(n, 100); CHECK(o == &ov); CHECK(chkp(b, 64, 100));
    HANDLE port2 = U(CreateIoCompletionPort)(INVALID_HANDLE_VALUE, NULL, 0, 0);
    SET_ERR(0); HANDLE r2 = U(CreateIoCompletionPort)(d, port2, 1, 0);
    CHECK(r2 == NULL); t_note("associate duplicate of an associated handle: err %lu", GetLastError());
    CloseHandle(d); CloseHandle(f); CloseHandle(port); CloseHandle(port2);
}

TEST(aio_xt_many_files_one_port_keys) {
    REQUIRE(U(CreateIoCompletionPort)); REQUIRE(U(ReadFile));
    enum { N = 32 };
    HANDLE port = U(CreateIoCompletionPort)(INVALID_HANDLE_VALUE, NULL, 0, 0);
    HANDLE f[N]; OVERLAPPED ov[N]; BYTE b[N][64]; char p[MAX_PATH];
    for (int i = 0; i < N; i++) {
        mkpath(p, "mf"); make_file(p, 4096 + i); f[i] = open_ov(p, GENERIC_READ, 0);
        CHECK(U(CreateIoCompletionPort)(f[i], port, 1000 + i, 0) == port);
        ovinit(&ov[i], i * 10, NULL); U(ReadFile)(f[i], b[i], 64, NULL, &ov[i]);
    }
    char seen[N] = {0}; int bad = 0;
    for (int i = 0; i < N; i++) {
        DWORD n; ULONG_PTR k; OVERLAPPED *o = NULL;
        if (!U(GetQueuedCompletionStatus)(port, &n, &k, &o, 5000)) { bad++; continue; }
        int idx = (int)(o - ov);
        if (idx < 0 || idx >= N || k != (ULONG_PTR)(1000 + idx) || n != 64 || seen[idx] || !chkp(b[idx], idx * 10, 64)) bad++; else seen[idx] = 1;
    }
    EXPECT_EQ(bad, 0);
    for (int i = 0; i < N; i++) CloseHandle(f[i]);
    CloseHandle(port);
}

TEST(aio_xt_both_pipe_ends_distinct_keys) {
    REQUIRE(U(ReadFile)); REQUIRE(U(WriteFile));
    HANDLE s, c; CHECK(make_pipe(&s, &c)); HANDLE port = U(CreateIoCompletionPort)(INVALID_HANDLE_VALUE, NULL, 0, 0);
    CHECK(U(CreateIoCompletionPort)(s, port, 0xAA, 0) == port); CHECK(U(CreateIoCompletionPort)(c, port, 0xBB, 0) == port);
    OVERLAPPED ro, wo; ovinit(&ro, 0, NULL); ovinit(&wo, 0, NULL); BYTE b[16];
    SET_ERR(0); BOOL r = U(ReadFile)(s, b, 16, NULL, &ro); CHECK(!r); EXPECT_ERR(ERROR_IO_PENDING);
    r = U(WriteFile)(c, "keys", 4, NULL, &wo); CHECK(r || GetLastError() == ERROR_IO_PENDING);
    int sawr = 0, sawc = 0;
    for (int i = 0; i < 2; i++) {
        DWORD n; ULONG_PTR k; OVERLAPPED *o = NULL;
        CHECK(U(GetQueuedCompletionStatus)(port, &n, &k, &o, 5000));
        if (o == &ro) { sawr++; EXPECT_EQ(k, 0xAA); EXPECT_EQ(n, 4); }
        else if (o == &wo) { sawc++; EXPECT_EQ(k, 0xBB); EXPECT_EQ(n, 4); }
    }
    EXPECT_EQ(sawr, 1); EXPECT_EQ(sawc, 1); CHECK(!memcmp(b, "keys", 4));
    closeh(&s); closeh(&c); CloseHandle(port);
}

TEST(aio_xt_ports_create_close_many) {
    REQUIRE(U(CreateIoCompletionPort)); REQUIRE(U(PostQueuedCompletionStatus)); REQUIRE(U(GetQueuedCompletionStatus));
    enum { N = 300 }; static HANDLE ports[N]; int bad = 0;
    for (int i = 0; i < N; i++) { ports[i] = U(CreateIoCompletionPort)(INVALID_HANDLE_VALUE, NULL, 0, 0); if (!ports[i]) bad++; }
    for (int i = 0; i < N; i++) if (ports[i]) U(PostQueuedCompletionStatus)(ports[i], i, i + 1, NULL);
    for (int i = N - 1; i >= 0; i--) {
        DWORD n = 0; ULONG_PTR k = 0; OVERLAPPED *o;
        if (!ports[i] || !U(GetQueuedCompletionStatus)(ports[i], &n, &k, &o, 1000) || n != (DWORD)i || k != (ULONG_PTR)i + 1) bad++;
        if (ports[i]) CloseHandle(ports[i]);
    }
    EXPECT_EQ(bad, 0);
}

TEST(aio_xt_posted_overlapped_pointer_not_dereferenced) {
    REQUIRE(U(PostQueuedCompletionStatus)); REQUIRE(U(GetQueuedCompletionStatus));
    HANDLE port = U(CreateIoCompletionPort)(INVALID_HANDLE_VALUE, NULL, 0, 0);
    OVERLAPPED ov; memset(&ov, 0, sizeof ov); ov.Internal = NTS_CANCELLED; ov.InternalHigh = 0x4242;
    CHECK(U(PostQueuedCompletionStatus)(port, 3, 4, &ov));
    DWORD n = 0; ULONG_PTR k = 0; OVERLAPPED *o = NULL;
    SET_ERR(0); CHECK(U(GetQueuedCompletionStatus)(port, &n, &k, &o, 1000));   /* status comes from the packet, not from ov */
    CHECK(o == &ov); EXPECT_EQ(n, 3); EXPECT_EQ(k, 4);
    EXPECT_EQ((DWORD)ov.Internal, NTS_CANCELLED); EXPECT_EQ(ov.InternalHigh, 0x4242);
    CloseHandle(port);
}

TEST(aio_xt_gqcs_large_timeouts_with_packet_available) {
    REQUIRE(U(GetQueuedCompletionStatus)); REQUIRE(U(PostQueuedCompletionStatus));
    HANDLE port = U(CreateIoCompletionPort)(INVALID_HANDLE_VALUE, NULL, 0, 0);
    static const DWORD tmo[] = {0, 1, 0x7FFFFFFF, 0xFFFFFFFE, INFINITE};
    for (unsigned i = 0; i < 5; i++) {
        U(PostQueuedCompletionStatus)(port, i, 100 + i, NULL);
        DWORD n = 99; ULONG_PTR k = 0; OVERLAPPED *o = (OVERLAPPED *)1; DWORD t0 = GetTickCount();
        BOOL r = U(GetQueuedCompletionStatus)(port, &n, &k, &o, tmo[i]);
        CHECKF(r && n == i && k == 100 + i && o == NULL && GetTickCount() - t0 < 2000, "timeout %lx", tmo[i]);
    }
    CloseHandle(port);
}

TEST(aio_xt_gqcsex_alertable_returns_queued_packets) {
    REQUIRE(U(GetQueuedCompletionStatusEx)); REQUIRE(U(PostQueuedCompletionStatus));
    HANDLE port = U(CreateIoCompletionPort)(INVALID_HANDLE_VALUE, NULL, 0, 0);
    U(PostQueuedCompletionStatus)(port, 1, 2, NULL); U(PostQueuedCompletionStatus)(port, 3, 4, NULL);
    OVERLAPPED_ENTRY e[8]; ULONG rem = 0; SET_ERR(0);
    CHECK(U(GetQueuedCompletionStatusEx)(port, e, 8, &rem, 1000, TRUE));
    EXPECT_EQ(rem, 2); EXPECT_EQ(e[0].lpCompletionKey, 2); EXPECT_EQ(e[1].dwNumberOfBytesTransferred, 3);
    CHECK(e[0].lpOverlapped == NULL);
    CloseHandle(port);
}

/* ---- ReOpenFile ---- */
TEST(aio_xt_reopenfile_adds_overlapped) {
    HANDLE (WINAPI *pReOpen)(HANDLE, DWORD, DWORD, DWORD) = (void *)UK(ReOpenFile);
    REQUIRE(pReOpen); REQUIRE(U(ReadFile)); REQUIRE(U(CreateIoCompletionPort));
    char p[MAX_PATH]; mkpath(p, "reopen"); make_file(p, 8192);
    HANDLE s = open_sync(p, GENERIC_READ);
    HANDLE port = U(CreateIoCompletionPort)(INVALID_HANDLE_VALUE, NULL, 0, 0);
    SET_ERR(0); CHECK(U(CreateIoCompletionPort)(s, port, 1, 0) == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER);   /* sync handle */
    HANDLE a = pReOpen(s, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_FLAG_OVERLAPPED);
    CHECKF(a != INVALID_HANDLE_VALUE, "ReOpenFile err %lu", GetLastError());
    if (a != INVALID_HANDLE_VALUE) {
        CHECK(U(CreateIoCompletionPort)(a, port, 2, 0) == port);
        OVERLAPPED ov; ovinit(&ov, 300, NULL); BYTE b[200];
        BOOL r = U(ReadFile)(a, b, 200, NULL, &ov); CHECK(r || GetLastError() == ERROR_IO_PENDING);
        DWORD n; ULONG_PTR k; OVERLAPPED *o = NULL;
        CHECK(U(GetQueuedCompletionStatus)(port, &n, &k, &o, 5000)); EXPECT_EQ(k, 2); EXPECT_EQ(n, 200); CHECK(chkp(b, 300, 200));
        CloseHandle(a);
    }
    CloseHandle(s); CloseHandle(port);
}

/* ---- unbuffered partial tail ---- */
TEST(aio_xt_unbuffered_read_partial_tail) {
    REQUIRE(U(ReadFile));
    char p[MAX_PATH]; mkpath(p, "ubt"); make_file(p, 10000);
    HANDLE h = open_ov(p, GENERIC_READ, FILE_FLAG_NO_BUFFERING); if (h == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    BYTE *b = VirtualAlloc(NULL, 16384, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); HANDLE ev = mkevent();
    OVERLAPPED ov; ovinit(&ov, 8192, ev);
    iores x = do_read(h, b, 8192, &ov);
    CHECK(x.ok); EXPECT_EQ(x.n, 10000 - 8192); CHECK(chkp(b, 8192, 10000 - 8192));
    VirtualFree(b, 0, MEM_RELEASE); CloseHandle(ev); CloseHandle(h);
}

/* ---- file growth ---- */
TEST(aio_xt_overlapped_write_extends_file) {
    REQUIRE(U(WriteFile));
    char p[MAX_PATH]; mkpath(p, "grow"); HANDLE h = new_ov_file(p, 0); HANDLE ev = mkevent(); BYTE b[512]; fillp(b, 0, 512);
    LARGE_INTEGER sz;
    for (int i = 0; i < 8; i++) {
        OVERLAPPED ov; ovinit(&ov, (ULONGLONG)i * 512, ev); ResetEvent(ev);
        iores x = do_write(h, b, 512, &ov); CHECKF(x.ok && x.n == 512, "write %d", i);
        CHECK(GetFileSizeEx(h, &sz)); CHECKF(sz.QuadPart == (i + 1) * 512, "size %lld after write %d", sz.QuadPart, i);
    }
    CloseHandle(ev); CloseHandle(h);
}

/* ---- message-mode pipes ---- */
static BOOL make_msg_pipe(HANDLE *srv, HANDLE *cli) {
    static int seq; char name[160]; snprintf(name, sizeof name, "\\\\.\\pipe\\k32t_aio_msg_%lu_%d", GetCurrentProcessId(), ++seq);
    *srv = CreateNamedPipeA(name, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 1, 4096, 4096, 0, NULL);
    if (*srv == INVALID_HANDLE_VALUE) return FALSE;
    *cli = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (*cli == INVALID_HANDLE_VALUE) { CloseHandle(*srv); *srv = INVALID_HANDLE_VALUE; return FALSE; }
    DWORD mode = PIPE_READMODE_MESSAGE; SetNamedPipeHandleState(*cli, &mode, NULL, NULL);
    OVERLAPPED ov; HANDLE ev = mkevent(); ovinit(&ov, 0, ev);
    BOOL r = ConnectNamedPipe(*srv, &ov); if (!r && GetLastError() == ERROR_IO_PENDING) WaitForSingleObject(ev, 5000);
    CloseHandle(ev); return TRUE;
}

TEST(aio_xt_msgpipe_more_data_overlapped) {
    REQUIRE(U(ReadFile)); REQUIRE(U(WriteFile)); REQUIRE(U(GetOverlappedResult));
    HANDLE s, c; if (!make_msg_pipe(&s, &c)) { CHECK(0); return; }
    HANDLE ev = mkevent(), we = mkevent(); OVERLAPPED wo, ro; ovinit(&wo, 0, we);
    CHECK(do_write(c, "0123456789", 10, &wo).ok);
    BYTE b[16]; memset(b, 0, sizeof b); ovinit(&ro, 0, ev); DWORD n = 77;
    SET_ERR(0); BOOL r = U(ReadFile)(s, b, 4, &n, &ro); DWORD e = GetLastError();
    if (!r && e == ERROR_IO_PENDING) { WaitForSingleObject(ev, 5000); r = U(GetOverlappedResult)(s, &ro, &n, FALSE); e = GetLastError(); }
    CHECK(!r); EXPECT_EQ(e, ERROR_MORE_DATA); CHECK(!memcmp(b, "0123", 4));
    EXPECT_EQ((DWORD)ro.Internal, 0x80000005u); EXPECT_EQ(ro.InternalHigh, 4);
    /* remainder of the same message */
    ovinit(&ro, 0, ev); ResetEvent(ev);
    iores x = do_read(s, b + 4, 12, &ro); CHECK(x.ok); EXPECT_EQ(x.n, 6); CHECK(!memcmp(b, "0123456789", 10));
    CloseHandle(ev); CloseHandle(we); closeh(&s); closeh(&c);
}

TEST(aio_xt_msgpipe_more_data_iocp_packet) {
    REQUIRE(U(ReadFile)); REQUIRE(U(GetQueuedCompletionStatus));
    HANDLE s, c; if (!make_msg_pipe(&s, &c)) { CHECK(0); return; }
    HANDLE port = U(CreateIoCompletionPort)(INVALID_HANDLE_VALUE, NULL, 0, 0); U(CreateIoCompletionPort)(s, port, 0x31, 0);
    HANDLE we = mkevent(); OVERLAPPED wo, ro; ovinit(&wo, 0, we); ovinit(&ro, 0, NULL); BYTE b[8];
    CHECK(do_write(c, "ABCDEFGH", 8, &wo).ok);
    SET_ERR(0); BOOL r = U(ReadFile)(s, b, 3, NULL, &ro); CHECK(!r);
    DWORD n = 0; ULONG_PTR k = 0; OVERLAPPED *o = NULL; SET_ERR(0);
    BOOL g = U(GetQueuedCompletionStatus)(port, &n, &k, &o, 5000);
    CHECK(!g); EXPECT_ERR(ERROR_MORE_DATA); CHECK(o == &ro); EXPECT_EQ(k, 0x31); EXPECT_EQ(n, 3);
    CloseHandle(we); closeh(&s); closeh(&c); CloseHandle(port);
}

TEST(aio_xt_transactnamedpipe_overlapped) {
    BOOL (WINAPI *pTrans)(HANDLE, LPVOID, DWORD, LPVOID, DWORD, LPDWORD, LPOVERLAPPED) = (void *)U(TransactNamedPipe);
    REQUIRE(pTrans); REQUIRE(U(ReadFile));
    HANDLE s, c; if (!make_msg_pipe(&s, &c)) { CHECK(0); return; }
    HANDLE cev = mkevent(), sev = mkevent(), wev = mkevent();
    OVERLAPPED cov, sov, wov; ovinit(&cov, 0, cev); BYTE out[64]; DWORD n = 0;
    SET_ERR(0); BOOL r = pTrans(c, "request", 7, out, sizeof out, &n, &cov);
    CHECK(!r); EXPECT_ERR(ERROR_IO_PENDING);
    BYTE in[32]; ovinit(&sov, 0, sev);
    iores x = do_read(s, in, sizeof in, &sov); CHECK(x.ok); EXPECT_EQ(x.n, 7); CHECK(!memcmp(in, "request", 7));
    CHECK(not_signaled(cev));
    ovinit(&wov, 0, wev); CHECK(do_write(s, "response!", 9, &wov).ok);
    CHECK(WaitForSingleObject(cev, 5000) == WAIT_OBJECT_0);
    n = 0; CHECK(GetOverlappedResult(c, &cov, &n, FALSE)); EXPECT_EQ(n, 9); CHECK(!memcmp(out, "response!", 9));
    CloseHandle(cev); CloseHandle(sev); CloseHandle(wev); closeh(&s); closeh(&c);
}

TEST(aio_xt_connect_already_connected_client) {
    REQUIRE(U(ConnectNamedPipe));
    char name[128]; snprintf(name, sizeof name, "\\\\.\\pipe\\k32t_aio_ac_%lu", GetCurrentProcessId());
    HANDLE s = CreateNamedPipeA(name, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, NULL);
    CHECK(s != INVALID_HANDLE_VALUE); if (s == INVALID_HANDLE_VALUE) return;
    HANDLE c = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL); CHECK(c != INVALID_HANDLE_VALUE);
    HANDLE ev = mkevent(); OVERLAPPED ov; ovinit(&ov, 0, ev);
    SET_ERR(0); BOOL r = U(ConnectNamedPipe)(s, &ov);
    CHECK(!r); EXPECT_ERR(ERROR_PIPE_CONNECTED);
    CloseHandle(ev); closeh(&c); closeh(&s);
}

/* ---- cross-process overlapped pipe ---- */
CHILD(aio_xt_pipe_client) {
    if (argc < 2) return 10;
    if (!WaitNamedPipeA(argv[1], 5000)) return 11;
    HANDLE h = CreateFileA(argv[1], GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 12;
    DWORD n = 0; if (!WriteFile(h, "from-child", 10, &n, NULL) || n != 10) return 13;
    char b[16] = {0}; if (!ReadFile(h, b, 5, &n, NULL) || n != 5) return 14;
    if (memcmp(b, "reply", 5)) return 15;
    CloseHandle(h); return 0;
}
TEST(aio_xt_cross_process_overlapped_pipe) {
    REQUIRE(U(ReadFile)); REQUIRE(U(WriteFile));
    char name[128]; snprintf(name, sizeof name, "\\\\.\\pipe\\k32t_aio_xp_%lu", GetCurrentProcessId());
    HANDLE s = CreateNamedPipeA(name, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, NULL);
    CHECK(s != INVALID_HANDLE_VALUE); if (s == INVALID_HANDLE_VALUE) return;
    HANDLE port = U(CreateIoCompletionPort)(INVALID_HANDLE_VALUE, NULL, 0, 0); U(CreateIoCompletionPort)(s, port, 0x99, 0);
    PROCESS_INFORMATION pi; HANDLE proc = t_spawn_child("aio_xt_pipe_client", name, 0, NULL, NULL, NULL, FALSE, NULL, &pi);
    CHECK(proc != NULL); if (!proc) { CloseHandle(s); CloseHandle(port); return; }
    OVERLAPPED co; ovinit(&co, 0, NULL);
    BOOL r = ConnectNamedPipe(s, &co); DWORD e = GetLastError();
    CHECK(!r && (e == ERROR_IO_PENDING || e == ERROR_PIPE_CONNECTED));
    OVERLAPPED ro; ovinit(&ro, 0, NULL); BYTE b[32] = {0};
    OVERLAPPED wo; ovinit(&wo, 0, NULL);
    DWORD n = 0; ULONG_PTR k = 0; OVERLAPPED *o = NULL; int gotc = 0, gotr = 0, gotw = 0, started = 0;
    /* an already-connected ConnectNamedPipe may or may not queue a packet: tolerate it */
    if (e == ERROR_PIPE_CONNECTED) gotc = 1;
    for (int i = 0; i < 6 && !(gotc && gotr && gotw); i++) {
        if (gotc && !started) { started = 1; r = U(ReadFile)(s, b, 10, NULL, &ro); CHECK(r || GetLastError() == ERROR_IO_PENDING); }
        o = NULL;
        if (!U(GetQueuedCompletionStatus)(port, &n, &k, &o, 10000)) { CHECK(0); break; }
        EXPECT_EQ(k, 0x99);
        if (o == &co) gotc = 1;
        else if (o == &ro) { gotr = 1; EXPECT_EQ(n, 10); CHECK(!memcmp(b, "from-child", 10));
            r = U(WriteFile)(s, "reply", 5, NULL, &wo); CHECK(r || GetLastError() == ERROR_IO_PENDING); }
        else if (o == &wo) { gotw = 1; EXPECT_EQ(n, 5); }
        if (gotc && !started) { started = 1; r = U(ReadFile)(s, b, 10, NULL, &ro); CHECK(r || GetLastError() == ERROR_IO_PENDING); }
    }
    CHECK(gotc && gotr && gotw);
    DWORD w = WaitForSingleObject(proc, 10000);
    if (w != WAIT_OBJECT_0) TerminateProcess(proc, 1);
    DWORD code = 99; GetExitCodeProcess(proc, &code); EXPECT_EQ(w, WAIT_OBJECT_0); EXPECT_EQ(code, 0);
    CloseHandle(pi.hThread); CloseHandle(proc); CloseHandle(s); CloseHandle(port);
}

/* ---- I/O priority hint (informational) ---- */
TEST(aio_xt_io_priority_hint_note) {
    REQUIRE(U(SetFileInformationByHandle)); REQUIRE(U(ReadFile));
    char p[MAX_PATH]; mkpath(p, "prio"); make_file(p, 4096);
    HANDLE h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    CHECK(h != INVALID_HANDLE_VALUE); if (h == INVALID_HANDLE_VALUE) return;
    for (DWORD hint = 0; hint <= 3; hint++) {
        DWORD v = hint; SET_ERR(0);
        BOOL r = U(SetFileInformationByHandle)(h, (FILE_INFO_BY_HANDLE_CLASS)12 /*FileIoPriorityHintInfo*/, &v, sizeof v);
        t_note("SetFileInformationByHandle(IoPriorityHint=%lu): %d err %lu", hint, r, r ? 0 : GetLastError());
    }
    HANDLE ev = mkevent(); OVERLAPPED ov; ovinit(&ov, 0, ev); BYTE b[100];
    iores x = do_read(h, b, 100, &ov); CHECK(x.ok); EXPECT_EQ(x.n, 100);
    CloseHandle(ev); CloseHandle(h);
}

/* ---- low-bit event with GetOverlappedResult wait ---- */
TEST(aio_xt_lowbit_event_gor_wait_true) {
    REQUIRE(U(ReadFile)); REQUIRE(U(GetOverlappedResult));
    char p[MAX_PATH]; mkpath(p, "lbg"); make_file(p, 4096);
    HANDLE h = open_ov(p, GENERIC_READ, 0); HANDLE ev = mkevent();
    OVERLAPPED ov; ovinit(&ov, 0, (HANDLE)((ULONG_PTR)ev | 1)); BYTE b[100];
    BOOL r = U(ReadFile)(h, b, 100, NULL, &ov); CHECK(r || GetLastError() == ERROR_IO_PENDING);
    DWORD n = 0; CHECK(U(GetOverlappedResult)(h, &ov, &n, TRUE)); EXPECT_EQ(n, 100);
    CloseHandle(ev); CloseHandle(h);
}
