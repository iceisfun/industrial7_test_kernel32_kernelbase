/* Pending overlapped pipe and mailslot operations: ConnectNamedPipe and FSCTL_PIPE_LISTEN through a
 * completion port, TransactNamedPipe, pending writes, mailslot reads; cancel versus close versus
 * peer close. */
#define XC_TU "pp"
#include "xc.h"

#define NEED_IOCP() REQUIRE(U(CreateIoCompletionPort)); REQUIRE(U(GetQueuedCompletionStatus)); REQUIRE(U(CancelIoEx))

typedef struct { BOOL r; DWORD err, n; ULONG_PTR key; OVERLAPPED *ov; } pkt;
static pkt getpkt(HANDLE port, DWORD ms) {
    pkt p; p.n = 0xFFFF; p.key = 0xFFFF; p.ov = (OVERLAPPED *)1; SetLastError(0);
    p.r = U(GetQueuedCompletionStatus)(port, &p.n, &p.key, &p.ov, ms); p.err = p.r ? 0 : GetLastError(); return p;
}
#define CHECK_PKT_ABORTED(p, k, o) do { pkt p_ = (p); \
    t_check(!p_.r && p_.err == ERROR_OPERATION_ABORTED && p_.n == 0 && p_.key == (ULONG_PTR)(k) && p_.ov == (o), __FILE__, __LINE__, \
            "aborted packet " #p, "r=%d err=%lu n=%lu key=0x%llx ov=%p", p_.r, p_.err, p_.n, (unsigned long long)p_.key, p_.ov); } while (0)
#define CHECK_NO_PKT(port) do { pkt q_ = getpkt(port, 50); \
    t_check(!q_.r && q_.err == WAIT_TIMEOUT && q_.ov == NULL, __FILE__, __LINE__, "no further packet", "r=%d err=%lu ov=%p", q_.r, q_.err, q_.ov); } while (0)

static BOOL fsctl_listen(HANDLE s, OVERLAPPED *ov) {
    DWORD n = 0; return U(DeviceIoControl)(s, FSCTL_PIPE_LISTEN, NULL, 0, NULL, 0, &n, ov);
}

/* ---- ConnectNamedPipe / FSCTL_PIPE_LISTEN ---- */
TEST(xcan_pipe_connect_iocp_cancel_packet) {
    NEED_IOCP(); REQUIRE(U(ConnectNamedPipe));
    char name[160]; HANDLE s = xlisten_srv(name, sizeof name, TRUE); CHECK(s != INVALID_HANDLE_VALUE);
    HANDLE port = U(CreateIoCompletionPort)(s, NULL, 0xC0, 0); CHECK(port != NULL);
    OVERLAPPED ov; xov(&ov, 0, NULL);
    CHECK_IOPEND(U(ConnectNamedPipe)(s, &ov));
    CHECK(U(CancelIoEx)(s, &ov));
    CHECK_PKT_ABORTED(getpkt(port, 5000), 0xC0, &ov);
    EXPECT_DW(ovst(&ov), NTS_CANCELLED);
    CHECK_NO_PKT(port);
    CloseHandle(port); xclose(&s);
}

TEST(xcan_pipe_connect_cancel_then_listen_again) {
    NEED_IOCP(); REQUIRE(U(ConnectNamedPipe));
    char name[160]; HANDLE s = xlisten_srv(name, sizeof name, TRUE); CHECK(s != INVALID_HANDLE_VALUE);
    HANDLE port = U(CreateIoCompletionPort)(s, NULL, 0xC1, 0);
    OVERLAPPED ov; xov(&ov, 0, NULL);
    CHECK_IOPEND(U(ConnectNamedPipe)(s, &ov));
    CHECK(U(CancelIoEx)(s, &ov));
    CHECK_PKT_ABORTED(getpkt(port, 5000), 0xC1, &ov);
    xov(&ov, 0, NULL);
    CHECK_IOPEND(U(ConnectNamedPipe)(s, &ov));
    HANDLE c = xclient(name, FALSE); CHECKF(c != INVALID_HANDLE_VALUE, "err %lu", GetLastError());
    pkt p = getpkt(port, 5000);
    CHECKF(p.r && p.key == 0xC1 && p.ov == &ov && p.n == 0, "r=%d err=%lu key=0x%llx", p.r, p.err, (unsigned long long)p.key);
    CHECK_NO_PKT(port);
    xclose(&c); CloseHandle(port); xclose(&s);
}

TEST(xcan_pipe_connect_gqcsex_entry_cancelled) {
    NEED_IOCP(); REQUIRE(U(ConnectNamedPipe)); REQUIRE(U(GetQueuedCompletionStatusEx));
    char name[160]; HANDLE s = xlisten_srv(name, sizeof name, TRUE); CHECK(s != INVALID_HANDLE_VALUE);
    HANDLE port = U(CreateIoCompletionPort)(s, NULL, 0xC2, 0);
    OVERLAPPED ov; xov(&ov, 0, NULL);
    CHECK_IOPEND(U(ConnectNamedPipe)(s, &ov));
    CHECK(U(CancelIoEx)(s, NULL));
    OVERLAPPED_ENTRY e[4]; memset(e, 0xCC, sizeof e); ULONG got = 99;
    SetLastError(0); CHECK(U(GetQueuedCompletionStatusEx)(port, e, 4, &got, 5000, FALSE));
    EXPECT_DW(got, 1);
    EXPECT_DW((DWORD)e[0].Internal, NTS_CANCELLED); EXPECT_EQ(e[0].lpCompletionKey, 0xC2);
    EXPECT_EQ(e[0].lpOverlapped, &ov); EXPECT_DW(e[0].dwNumberOfBytesTransferred, 0);
    CloseHandle(port); xclose(&s);
}

TEST(xcan_pipe_connect_cancelio_issuing_thread) {
    REQUIRE(U(ConnectNamedPipe)); REQUIRE(U(CancelIo));
    char name[160]; HANDLE s = xlisten_srv(name, sizeof name, TRUE); CHECK(s != INVALID_HANDLE_VALUE);
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev);
    CHECK_IOPEND(U(ConnectNamedPipe)(s, &ov));
    CHECK(U(CancelIo)(s));
    CHECK(xwait(ev, 5000)); CHECK_ABORTED(xnow(s, &ov));
    CloseHandle(ev); xclose(&s);
}

TEST(xcan_pipe_connect_cancel_client_still_opens) {
    REQUIRE(U(ConnectNamedPipe)); REQUIRE(U(CancelIoEx));
    char name[160]; HANDLE s = xlisten_srv(name, sizeof name, TRUE); CHECK(s != INVALID_HANDLE_VALUE);
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev);
    CHECK_IOPEND(U(ConnectNamedPipe)(s, &ov));
    CHECK(U(CancelIoEx)(s, &ov)); CHECK_ABORTED(xfin(s, &ov, 5000));
    /* cancelling the listen does not leave the listening state */
    HANDLE c = xclient(name, TRUE); CHECKF(c != INVALID_HANDLE_VALUE, "err %lu", GetLastError());
    xov(&ov, 0, ev); ResetEvent(ev);
    CHECK_FAILS(U(ConnectNamedPipe)(s, &ov), ERROR_PIPE_CONNECTED);
    CHECK(xwrite_all(c, "hi", 2));
    xclose(&c); CloseHandle(ev); xclose(&s);
}

/* DeviceIoControl sends codes whose device type is not FILE_DEVICE_FILE_SYSTEM as device controls,
 * so FSCTL_PIPE_LISTEN (device type FILE_DEVICE_NAMED_PIPE) never reaches the listen code: it fails
 * at once and leaves the OVERLAPPED as DeviceIoControl initialised it. */
TEST(xcan_pipe_fsctl_listen_devioctl_invalid_function) {
    REQUIRE(U(DeviceIoControl)); REQUIRE(U(CreateIoCompletionPort)); REQUIRE(U(CancelIoEx));
    char name[160]; HANDLE s = xlisten_srv(name, sizeof name, TRUE); CHECK(s != INVALID_HANDLE_VALUE);
    HANDLE port = U(CreateIoCompletionPort)(s, NULL, 0xF4, 0);
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev);
    CHECK_FAILS(fsctl_listen(s, &ov), ERROR_INVALID_FUNCTION);
    EXPECT_DW(ovst(&ov), NTS_PENDING);
    CHECK(!xsig(ev));
    CHECK_FAILS(U(CancelIoEx)(s, &ov), ERROR_NOT_FOUND);
    CHECK_NO_PKT(port);
    /* the instance still listens */
    xov(&ov, 0, ev);
    CHECK_IOPEND(U(ConnectNamedPipe)(s, &ov));
    HANDLE c = xclient(name, FALSE); CHECK(c != INVALID_HANDLE_VALUE);
    pkt p = getpkt(port, 5000); CHECKF(p.r && p.ov == &ov, "r=%d err=%lu", p.r, p.err);
    xclose(&c); CloseHandle(port); CloseHandle(ev); xclose(&s);
}

/* The same control code through NtFsControlFile is the real listen. */
UDECL(NTSTATUS, NtFsControlFile, (HANDLE, HANDLE, PVOID, PVOID, PIO_STATUS_BLOCK, ULONG, PVOID, ULONG, PVOID, ULONG));
static NtFsControlFile_pfn pNtFsControlFile(void) { return (NtFsControlFile_pfn)t_proc_nt("NtFsControlFile"); }
static DWORD nt_listen(HANDLE s, OVERLAPPED *ov) {
    ov->Internal = NTS_PENDING;
    return (DWORD)pNtFsControlFile()(s, ov->hEvent, NULL, ov, (PIO_STATUS_BLOCK)ov, FSCTL_PIPE_LISTEN, NULL, 0, NULL, 0);
}
#define NEED_NTFSC() REQUIRE(pNtFsControlFile()); REQUIRE(U(CancelIoEx))

TEST(xcan_pipe_fsctl_listen_pending) {
    NEED_NTFSC();
    char name[160]; HANDLE s = xlisten_srv(name, sizeof name, TRUE); CHECK(s != INVALID_HANDLE_VALUE);
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev);
    EXPECT_DW(nt_listen(s, &ov), NTS_PENDING);
    CHECK_PENDING(&ov); CHECK(!xsig(ev));
    DWORD n; SetLastError(0); CHECK(!GetOverlappedResultEx(s, &ov, &n, 50, FALSE)); EXPECT_ERR(WAIT_TIMEOUT);
    HANDLE c = xclient(name, FALSE);
    xres x = xfin(s, &ov, 5000); CHECKF(x.ok, "err %lu", x.err); EXPECT_DW(ovst(&ov), 0);
    xclose(&c); CloseHandle(ev); xclose(&s);
}

TEST(xcan_pipe_fsctl_listen_iocp_cancel_packet) {
    NEED_IOCP(); NEED_NTFSC();
    char name[160]; HANDLE s = xlisten_srv(name, sizeof name, TRUE); CHECK(s != INVALID_HANDLE_VALUE);
    HANDLE port = U(CreateIoCompletionPort)(s, NULL, 0xF5, 0);
    OVERLAPPED ov; xov(&ov, 0, NULL);
    EXPECT_DW(nt_listen(s, &ov), NTS_PENDING);
    CHECK(U(CancelIoEx)(s, &ov));
    CHECK_PKT_ABORTED(getpkt(port, 5000), 0xF5, &ov);
    EXPECT_DW(ovst(&ov), NTS_CANCELLED);
    CHECK_NO_PKT(port);
    CloseHandle(port); xclose(&s);
}

TEST(xcan_pipe_fsctl_listen_again_after_cancel) {
    NEED_IOCP(); NEED_NTFSC();
    char name[160]; HANDLE s = xlisten_srv(name, sizeof name, TRUE); CHECK(s != INVALID_HANDLE_VALUE);
    HANDLE port = U(CreateIoCompletionPort)(s, NULL, 0xF6, 0);
    OVERLAPPED ov; xov(&ov, 0, NULL);
    EXPECT_DW(nt_listen(s, &ov), NTS_PENDING);
    CHECK(U(CancelIoEx)(s, &ov));
    CHECK_PKT_ABORTED(getpkt(port, 5000), 0xF6, &ov);
    xov(&ov, 0, NULL);
    EXPECT_DW(nt_listen(s, &ov), NTS_PENDING);
    HANDLE c = xclient(name, FALSE); CHECK(c != INVALID_HANDLE_VALUE);
    pkt p = getpkt(port, 5000); CHECKF(p.r && p.ov == &ov && p.key == 0xF6, "r=%d err=%lu", p.r, p.err);
    /* connected now: a further listen is refused at once and queues nothing */
    xov(&ov, 0, NULL);
    EXPECT_DW(nt_listen(s, &ov), 0xC00000B2u /* STATUS_PIPE_CONNECTED */);
    CHECK_FAILS(U(ConnectNamedPipe)(s, &ov), ERROR_PIPE_CONNECTED);
    CHECK_NO_PKT(port);
    xclose(&c); CloseHandle(port); xclose(&s);
}

TEST(xcan_pipe_fsctl_listen_cancel_via_dup_handle) {
    NEED_NTFSC(); REQUIRE(U(DuplicateHandle));
    char name[160]; HANDLE s = xlisten_srv(name, sizeof name, TRUE); CHECK(s != INVALID_HANDLE_VALUE);
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev);
    EXPECT_DW(nt_listen(s, &ov), NTS_PENDING);
    HANDLE d = NULL; CHECK(U(DuplicateHandle)(GetCurrentProcess(), s, GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS));
    CHECK(U(CancelIoEx)(d, &ov));
    CHECK_ABORTED(xfin(s, &ov, 5000));
    xclose(&d); CloseHandle(ev); xclose(&s);
}

/* ---- closing instead of cancelling ---- */
TEST(xcan_pipe_close_server_with_pending_listen) {
    REQUIRE(U(ConnectNamedPipe));
    char name[160]; HANDLE s = xlisten_srv(name, sizeof name, TRUE); CHECK(s != INVALID_HANDLE_VALUE);
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev);
    CHECK_IOPEND(U(ConnectNamedPipe)(s, &ov));
    HANDLE stale = s; xclose(&s);
    CHECK(xwait(ev, 5000));
    xres x = xnow(stale, &ov);
    CHECKF(!x.ok && x.err == ERROR_BROKEN_PIPE && x.n == 0, "ok=%d err=%lu n=%lu Internal=0x%lx", x.ok, x.err, x.n, ovst(&ov));
    CloseHandle(ev);
}

TEST(xcan_pipe_close_server_with_pending_read) {
    REQUIRE(U(ReadFile));
    HANDLE s, c; CHECK(xpipe(&s, &c));
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev); BYTE b[16];
    CHECK(xread_pend(s, b, sizeof b, &ov));
    HANDLE stale = s; xclose(&s);
    CHECK(xwait(ev, 5000));
    xres x = xnow(stale, &ov);
    CHECKF(!x.ok && x.err == ERROR_BROKEN_PIPE && x.n == 0, "ok=%d err=%lu n=%lu Internal=0x%lx", x.ok, x.err, x.n, ovst(&ov));
    xclose(&c); CloseHandle(ev);
}

TEST(xcan_pipe_close_client_with_pending_read) {
    REQUIRE(U(ReadFile));
    HANDLE s, c; CHECK(xpipe(&s, &c));
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev); BYTE b[16];
    CHECK(xread_pend(c, b, sizeof b, &ov));
    HANDLE stale = c; xclose(&c);
    CHECK(xwait(ev, 5000));
    xres x = xnow(stale, &ov);
    CHECKF(!x.ok && x.err == ERROR_BROKEN_PIPE && x.n == 0, "ok=%d err=%lu n=%lu Internal=0x%lx", x.ok, x.err, x.n, ovst(&ov));
    xclose(&s); CloseHandle(ev);
}

TEST(xcan_pipe_peer_close_is_broken_not_aborted) {
    REQUIRE(U(ReadFile));
    HANDLE s, c; CHECK(xpipe(&s, &c));
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev); BYTE b[16];
    CHECK(xread_pend(s, b, sizeof b, &ov));
    xclose(&c);
    CHECK(xwait(ev, 5000));
    xres x = xnow(s, &ov);
    CHECKF(!x.ok && x.err == ERROR_BROKEN_PIPE, "ok=%d err=%lu", x.ok, x.err);
    CHECK_FAILS(U(CancelIoEx)(s, &ov), ERROR_NOT_FOUND);
    xclose(&s); CloseHandle(ev);
}

TEST(xcan_pipe_disconnect_with_pending_read) {
    REQUIRE(U(ReadFile));
    HANDLE s, c; CHECK(xpipe(&s, &c));
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev); BYTE b[16];
    CHECK(xread_pend(s, b, sizeof b, &ov));
    CHECK(DisconnectNamedPipe(s));
    CHECK(xwait(ev, 5000));
    xres x = xnow(s, &ov);
    CHECKF(!x.ok && x.err == ERROR_PIPE_NOT_CONNECTED && x.n == 0, "ok=%d err=%lu n=%lu Internal=0x%lx", x.ok, x.err, x.n, ovst(&ov));
    xclose(&c); xclose(&s); CloseHandle(ev);
}

/* ---- TransactNamedPipe ---- */
static BOOL msg_pipe(HANDLE *s, HANDLE *c) { return xpipe_ex(s, c, 4096, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE, TRUE, TRUE); }

TEST(xcan_pipe_transact_overlapped_cancelled) {
    REQUIRE(U(TransactNamedPipe)); REQUIRE(U(CancelIoEx));
    HANDLE s, c; CHECK(msg_pipe(&s, &c));
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev); BYTE out[64];
    CHECK_IOPEND(U(TransactNamedPipe)(c, (void *)"question", 8, out, sizeof out, NULL, &ov));
    CHECK_PENDING(&ov);
    CHECK(U(CancelIoEx)(c, &ov));
    CHECK_ABORTED(xfin(c, &ov, 5000));
    /* the request message was written before the read half was cancelled */
    BYTE b[64]; OVERLAPPED ro; HANDLE re = xev(); xov(&ro, 0, re);
    SetLastError(0); BOOL r = U(ReadFile)(s, b, sizeof b, NULL, &ro);
    xres x = (r || GetLastError() == ERROR_IO_PENDING) ? xfin(s, &ro, 1000) : (xres){FALSE, GetLastError(), 0};
    CHECKF(x.ok && x.n == 8 && !memcmp(b, "question", 8), "ok=%d err=%lu n=%lu", x.ok, x.err, x.n);
    if (!x.ok) { U(CancelIoEx)(s, &ro); xfin(s, &ro, 2000); }
    CloseHandle(re); CloseHandle(ev); xclose(&c); xclose(&s);
}

TEST(xcan_pipe_transact_iocp_cancel_packet) {
    NEED_IOCP(); REQUIRE(U(TransactNamedPipe));
    HANDLE s, c; CHECK(msg_pipe(&s, &c));
    HANDLE port = U(CreateIoCompletionPort)(c, NULL, 0x7A, 0);
    OVERLAPPED ov; xov(&ov, 0, NULL); BYTE out[64];
    CHECK_IOPEND(U(TransactNamedPipe)(c, (void *)"q", 1, out, sizeof out, NULL, &ov));
    CHECK(U(CancelIoEx)(c, NULL));
    CHECK_PKT_ABORTED(getpkt(port, 5000), 0x7A, &ov);
    CHECK_NO_PKT(port);
    CloseHandle(port); xclose(&c); xclose(&s);
}

TEST(xcan_pipe_transact_works_after_cancel) {
    REQUIRE(U(TransactNamedPipe)); REQUIRE(U(CancelIoEx));
    HANDLE s, c; CHECK(msg_pipe(&s, &c));
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev); BYTE out[64], b[64];
    CHECK_IOPEND(U(TransactNamedPipe)(c, (void *)"one", 3, out, sizeof out, NULL, &ov));
    CHECK(U(CancelIoEx)(c, &ov)); CHECK_ABORTED(xfin(c, &ov, 5000));
    /* drain the orphaned request, then a full transaction round trip */
    OVERLAPPED ro; HANDLE re = xev(); xov(&ro, 0, re);
    U(ReadFile)(s, b, sizeof b, NULL, &ro); xfin(s, &ro, 2000);
    xov(&ov, 0, ev); ResetEvent(ev);
    SetLastError(0); BOOL r = U(TransactNamedPipe)(c, (void *)"two", 3, out, sizeof out, NULL, &ov);
    CHECKF(!r && GetLastError() == ERROR_IO_PENDING, "r=%d err=%lu", r, GetLastError());
    xov(&ro, 0, re); ResetEvent(re); memset(b, 0, sizeof b);
    U(ReadFile)(s, b, sizeof b, NULL, &ro); xres y = xfin(s, &ro, 2000);
    CHECK(y.ok && y.n == 3 && !memcmp(b, "two", 3));
    CHECK(xwrite_all(s, "answer", 6));
    xres x = xfin(c, &ov, 5000);
    CHECKF(x.ok && x.n == 6 && !memcmp(out, "answer", 6), "ok=%d err=%lu n=%lu", x.ok, x.err, x.n);
    CloseHandle(re); CloseHandle(ev); xclose(&c); xclose(&s);
}

/* ---- pending pipe write ---- */
TEST(xcan_pipe_pending_write_cancelled) {
    REQUIRE(U(WriteFile)); REQUIRE(U(CancelIoEx));
    HANDLE s, c; CHECK(xpipe_ex(&s, &c, 1024, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE, TRUE, TRUE));
    static BYTE big[64 * 1024]; memset(big, 'w', sizeof big);
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev);
    SetLastError(0); BOOL r = U(WriteFile)(c, big, sizeof big, NULL, &ov);
    CHECKF(!r && GetLastError() == ERROR_IO_PENDING, "r=%d err=%lu", r, GetLastError());
    CHECK(U(CancelIoEx)(c, &ov));
    xres x = xfin(c, &ov, 5000);
    CHECKF(!x.ok && x.err == ERROR_OPERATION_ABORTED, "ok=%d err=%lu n=%lu", x.ok, x.err, x.n);
    EXPECT_DW(x.n, 0);
    CloseHandle(ev); xclose(&c); xclose(&s);
}

/* ---- mailslot overlapped reads ---- */
static HANDLE slot_new(char *name, size_t cap, DWORD timeout) { xslotname(name, cap); return CreateMailslotA(name, 0, timeout, NULL); }
static BOOL slot_put(const char *name, const char *msg) {
    HANDLE c = CreateFileA(name, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (c == INVALID_HANDLE_VALUE) return FALSE;
    DWORD n = 0; BOOL r = WriteFile(c, msg, (DWORD)strlen(msg), &n, NULL); CloseHandle(c); return r;
}
/* Watchdog action: a message completes a read that unexpectedly blocks the test thread. */
static void slot_dog(void *name) { slot_put((const char *)name, "dog"); }
#define SLOT_GUARD(name) xdog *dog_ = xdog_start(slot_dog, (void *)(name), 4000)
#define SLOT_UNGUARD() CHECKF(!xdog_stop(dog_), "a mailslot read blocked the calling thread")

TEST(xcan_slot_overlapped_read_pending_cancelled) {
    REQUIRE(U(ReadFile)); REQUIRE(U(CancelIoEx));
    char name[160]; HANDLE h = slot_new(name, sizeof name, MAILSLOT_WAIT_FOREVER); CHECK(h != INVALID_HANDLE_VALUE);
    SLOT_GUARD(name);
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev); BYTE b[64];
    CHECK(xread_pend(h, b, sizeof b, &ov));
    CHECK_PENDING(&ov);
    CHECK(U(CancelIoEx)(h, &ov));
    CHECK_ABORTED(xfin(h, &ov, 5000)); EXPECT_DW(ovst(&ov), NTS_CANCELLED);
    SLOT_UNGUARD(); CloseHandle(ev); xclose(&h);
}

TEST(xcan_slot_read_again_after_cancel) {
    REQUIRE(U(ReadFile)); REQUIRE(U(CancelIoEx));
    char name[160]; HANDLE h = slot_new(name, sizeof name, MAILSLOT_WAIT_FOREVER); CHECK(h != INVALID_HANDLE_VALUE);
    SLOT_GUARD(name);
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev); BYTE b[64];
    CHECK(xread_pend(h, b, sizeof b, &ov));
    CHECK(U(CancelIoEx)(h, &ov)); CHECK_ABORTED(xfin(h, &ov, 5000));
    xov(&ov, 0, ev); ResetEvent(ev);
    CHECK(xread_pend(h, b, sizeof b, &ov));
    CHECK(slot_put(name, "slotmsg"));
    xres x = xfin(h, &ov, 5000); CHECKF(x.ok && x.n == 7 && !memcmp(b, "slotmsg", 7), "ok=%d err=%lu n=%lu", x.ok, x.err, x.n);
    SLOT_UNGUARD(); CloseHandle(ev); xclose(&h);
}

TEST(xcan_slot_iocp_cancel_packet) {
    NEED_IOCP(); REQUIRE(U(ReadFile));
    char name[160]; HANDLE h = slot_new(name, sizeof name, MAILSLOT_WAIT_FOREVER); CHECK(h != INVALID_HANDLE_VALUE);
    SLOT_GUARD(name);
    HANDLE port = U(CreateIoCompletionPort)(h, NULL, 0x5107, 0); CHECK(port != NULL);
    OVERLAPPED ov; xov(&ov, 0, NULL); BYTE b[64];
    CHECK(xread_pend(h, b, sizeof b, &ov));
    CHECK(U(CancelIoEx)(h, &ov));
    CHECK_PKT_ABORTED(getpkt(port, 5000), 0x5107, &ov);
    CHECK_NO_PKT(port);
    SLOT_UNGUARD(); CloseHandle(port); xclose(&h);
}

TEST(xcan_slot_timeout_read_overlapped_completes_sem_timeout) {
    REQUIRE(U(ReadFile));
    char name[160]; HANDLE h = slot_new(name, sizeof name, 150); CHECK(h != INVALID_HANDLE_VALUE);
    SLOT_GUARD(name);
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev); BYTE b[64];
    SetLastError(0); BOOL r = U(ReadFile)(h, b, sizeof b, NULL, &ov); DWORD e = GetLastError();
    CHECKF(!r && e == ERROR_IO_PENDING, "r=%d err=%lu", r, e);
    xres x = xfin(h, &ov, 5000);
    CHECKF(!x.ok && x.err == ERROR_SEM_TIMEOUT, "ok=%d err=%lu", x.ok, x.err);
    CHECK_FAILS(U(CancelIoEx)(h, &ov), ERROR_NOT_FOUND);
    SLOT_UNGUARD(); CloseHandle(ev); xclose(&h);
}

TEST(xcan_slot_close_with_pending_read) {
    REQUIRE(U(ReadFile));
    char name[160]; HANDLE h = slot_new(name, sizeof name, MAILSLOT_WAIT_FOREVER); CHECK(h != INVALID_HANDLE_VALUE);
    SLOT_GUARD(name);
    HANDLE ev = xev(); OVERLAPPED ov; xov(&ov, 0, ev); BYTE b[64];
    CHECK(xread_pend(h, b, sizeof b, &ov));
    HANDLE stale = h; xclose(&h);
    CHECK(xwait(ev, 5000));
    xres x = xnow(stale, &ov);
    /* STATUS_FILE_FORCED_CLOSED, reported as ERROR_HANDLE_EOF */
    CHECKF(!x.ok && x.err == ERROR_HANDLE_EOF && x.n == 0, "ok=%d err=%lu n=%lu Internal=0x%lx", x.ok, x.err, x.n, ovst(&ov));
    EXPECT_DW(ovst(&ov), 0xC00000B6u);
    SLOT_UNGUARD(); CloseHandle(ev);
}
