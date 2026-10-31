#define AIO_TU "ip"
#include "aio.h"

static void CALLBACK noop_apc_ip(ULONG_PTR p) { (*(volatile LONG *)p)++; }
static HANDLE new_port(DWORD conc) { return U(CreateIoCompletionPort)(INVALID_HANDLE_VALUE, NULL, 0, conc); }
/* dequeue helper: returns GQCS result */
typedef struct { BOOL r; DWORD err; DWORD n; ULONG_PTR key; OVERLAPPED *ov; } gq;
static gq getq(HANDLE port, DWORD tmo) {
    gq g; g.n = 0xAAAA5555; g.key = 0x5A5A5A5A; g.ov = (OVERLAPPED *)(ULONG_PTR)0x1234;
    SET_ERR(0); g.r = U(GetQueuedCompletionStatus)(port, &g.n, &g.key, &g.ov, tmo); g.err = g.r ? 0 : GetLastError(); return g;
}
static HANDLE pending_pipe_read(HANDLE s, OVERLAPPED *ov, HANDLE ev, BYTE *b, DWORD n) {
    ovinit(ov, 0, ev); SET_ERR(0); BOOL r = U(ReadFile)(s, b, n, NULL, ov); return (!r && GetLastError() == ERROR_IO_PENDING) ? s : NULL;
}

TEST(aio_iocp_create_new_port) {
    REQUIRE(U(CreateIoCompletionPort));
    HANDLE p1 = new_port(0), p2 = new_port(0);
    CHECK(p1 != NULL && p1 != INVALID_HANDLE_VALUE); CHECK(p2 != NULL); CHECK(p1 != p2);
    DWORD flags = 0; CHECK(GetHandleInformation(p1, &flags));
    CHECK(CloseHandle(p1)); CHECK(CloseHandle(p2));
    SET_ERR(0); CHECK(!CloseHandle(p1)); EXPECT_ERR(ERROR_INVALID_HANDLE);
}

TEST(aio_iocp_create_concurrency_values) {
    REQUIRE(U(CreateIoCompletionPort));
    static const DWORD v[] = {0, 1, 2, 4, 64, 1000};
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) {
        HANDLE p = new_port(v[i]); CHECKF(p != NULL, "conc %lu", v[i]); if (p) CloseHandle(p);
    }
}

TEST(aio_iocp_create_invalid_with_existing_port) {
    REQUIRE(U(CreateIoCompletionPort));
    HANDLE p = new_port(0);
    SET_ERR(0); HANDLE r = U(CreateIoCompletionPort)(INVALID_HANDLE_VALUE, p, 1, 0);
    CHECK(r == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); r = U(CreateIoCompletionPort)(INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x12345678, 1, 0);
    CHECK(r == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CloseHandle(p);
}

TEST(aio_iocp_associate_returns_port) {
    REQUIRE(U(CreateIoCompletionPort));
    char p1[MAX_PATH], p2[MAX_PATH]; mkpath(p1, "a1"); mkpath(p2, "a2"); make_file(p1, 4096); make_file(p2, 4096);
    HANDLE f1 = open_ov(p1, GENERIC_READ, 0), f2 = open_ov(p2, GENERIC_READ, 0);
    HANDLE port = new_port(0);
    HANDLE r1 = U(CreateIoCompletionPort)(f1, port, 11, 0); CHECK(r1 == port);
    HANDLE r2 = U(CreateIoCompletionPort)(f2, port, 22, 7); CHECK(r2 == port);
    /* associate with a fresh port created on the fly: returns a new handle */
    char p3[MAX_PATH]; mkpath(p3, "a3"); make_file(p3, 4096); HANDLE f3 = open_ov(p3, GENERIC_READ, 0);
    HANDLE r3 = U(CreateIoCompletionPort)(f3, NULL, 33, 0); CHECK(r3 != NULL && r3 != port);
    if (r3) CloseHandle(r3);
    CloseHandle(f3); CloseHandle(f2); CloseHandle(f1); CloseHandle(port);
}

TEST(aio_iocp_associate_sync_handle_fails) {
    REQUIRE(U(CreateIoCompletionPort));
    char p[MAX_PATH]; mkpath(p, "sync"); make_file(p, 4096);
    HANDLE f = open_sync(p, GENERIC_READ); HANDLE port = new_port(0);
    SET_ERR(0); HANDLE r = U(CreateIoCompletionPort)(f, port, 1, 0);
    CHECK(r == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); r = U(CreateIoCompletionPort)(f, NULL, 1, 0);
    CHECK(r == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CloseHandle(f); CloseHandle(port);
}

TEST(aio_iocp_reassociate_fails) {
    REQUIRE(U(CreateIoCompletionPort));
    char p[MAX_PATH]; mkpath(p, "re"); make_file(p, 4096);
    HANDLE f = open_ov(p, GENERIC_READ, 0); HANDLE a = new_port(0), b = new_port(0);
    CHECK(U(CreateIoCompletionPort)(f, a, 1, 0) == a);
    SET_ERR(0); HANDLE r = U(CreateIoCompletionPort)(f, b, 2, 0);
    CHECK(r == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); r = U(CreateIoCompletionPort)(f, a, 3, 0);
    CHECK(r == NULL); t_note("reassociate same port: err %lu", GetLastError());
    SET_ERR(0); r = U(CreateIoCompletionPort)(f, NULL, 3, 0);
    CHECK(r == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CloseHandle(f); CloseHandle(a); CloseHandle(b);
}

TEST(aio_iocp_associate_bad_file_handles) {
    REQUIRE(U(CreateIoCompletionPort));
    HANDLE port = new_port(0);
    SET_ERR(0); CHECK(U(CreateIoCompletionPort)(NULL, port, 1, 0) == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(U(CreateIoCompletionPort)(NULL, NULL, 1, 0) == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE);
    char p[MAX_PATH]; mkpath(p, "closed"); make_file(p, 100);
    HANDLE f = open_ov(p, GENERIC_READ, 0); CloseHandle(f);
    SET_ERR(0); CHECK(U(CreateIoCompletionPort)(f, port, 1, 0) == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE ev = mkevent();
    SET_ERR(0); HANDLE r = U(CreateIoCompletionPort)(ev, port, 1, 0);
    CHECK(r == NULL); t_note("associate event handle: err %lu", GetLastError());
    CloseHandle(ev); CloseHandle(port);
}

TEST(aio_iocp_associate_with_wrong_port_object) {
    REQUIRE(U(CreateIoCompletionPort));
    char p[MAX_PATH]; mkpath(p, "wrongport"); make_file(p, 100);
    HANDLE f = open_ov(p, GENERIC_READ, 0); HANDLE ev = mkevent();
    SET_ERR(0); HANDLE r = U(CreateIoCompletionPort)(f, ev, 1, 0);
    CHECK(r == NULL); t_note("associate with event as port: err %lu", GetLastError());
    HANDLE stale = new_port(0); CloseHandle(stale);
    SET_ERR(0); r = U(CreateIoCompletionPort)(f, stale, 1, 0);
    CHECK(r == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev); CloseHandle(f);
}

TEST(aio_iocp_gqcs_timeout_empty) {
    REQUIRE(U(GetQueuedCompletionStatus));
    HANDLE port = new_port(0);
    gq g = getq(port, 0);
    CHECK(!g.r); EXPECT_EQ(g.err, WAIT_TIMEOUT); CHECK(g.ov == NULL);
    DWORD t0 = GetTickCount(); g = getq(port, 120); DWORD dt = GetTickCount() - t0;
    CHECK(!g.r); EXPECT_EQ(g.err, WAIT_TIMEOUT); CHECK(g.ov == NULL); CHECKF(dt >= 100 && dt < 3000, "dt=%lu", dt);
    CloseHandle(port);
}

TEST(aio_iocp_gqcs_invalid_handles) {
    REQUIRE(U(GetQueuedCompletionStatus));
    HANDLE ev = mkevent(); HANDLE stale = new_port(0); CloseHandle(stale);
    HANDLE bad[4] = {INVALID_HANDLE_VALUE, NULL, stale, ev};
    for (int i = 0; i < 4; i++) {
        gq g = getq(bad[i], 0); CHECKF(!g.r, "case %d", i); EXPECT_EQ(g.err, ERROR_INVALID_HANDLE); CHECK(g.ov == NULL);
    }
    CloseHandle(ev);
}

TEST(aio_iocp_post_invalid_handles) {
    REQUIRE(U(PostQueuedCompletionStatus));
    HANDLE ev = mkevent(); HANDLE stale = new_port(0); CloseHandle(stale);
    HANDLE bad[4] = {INVALID_HANDLE_VALUE, NULL, stale, ev};
    for (int i = 0; i < 4; i++) {
        SET_ERR(0); CHECKF(!U(PostQueuedCompletionStatus)(bad[i], 1, 2, NULL), "case %d", i); EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
    CloseHandle(ev);
}

TEST(aio_iocp_post_roundtrip_table) {
    REQUIRE(U(PostQueuedCompletionStatus));
    HANDLE port = new_port(0); OVERLAPPED local; memset(&local, 0x11, sizeof local);
    struct { DWORD n; ULONG_PTR key; OVERLAPPED *ov; } t[] = {
        {0, 0, NULL}, {1, 1, (OVERLAPPED *)(ULONG_PTR)1}, {0xFFFFFFFF, (ULONG_PTR)-1, (OVERLAPPED *)(ULONG_PTR)-1},
        {123, 0x123456789ULL, &local}, {0x80000000, 0x8000000000000000ULL, (OVERLAPPED *)(ULONG_PTR)0xdeadbeef00ULL}};
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        SET_ERR(0); CHECK(U(PostQueuedCompletionStatus)(port, t[i].n, t[i].key, t[i].ov));
        gq g = getq(port, 2000);
        CHECKF(g.r, "case %u err %lu", i, g.err); EXPECT_EQ(g.n, t[i].n); EXPECT_EQ(g.key, t[i].key); CHECK(g.ov == t[i].ov);
    }
    unsigned char c[sizeof local]; memset(c, 0x11, sizeof c); CHECK(!memcmp(&local, c, sizeof c));   /* never touched */
    gq g = getq(port, 0); CHECK(!g.r); EXPECT_EQ(g.err, WAIT_TIMEOUT);
    CloseHandle(port);
}

TEST(aio_iocp_post_null_overlapped_is_success) {
    REQUIRE(U(PostQueuedCompletionStatus));
    HANDLE port = new_port(0);
    CHECK(U(PostQueuedCompletionStatus)(port, 5, 6, NULL));
    gq g = getq(port, 1000); CHECK(g.r); CHECK(g.ov == NULL); EXPECT_EQ(g.n, 5); EXPECT_EQ(g.key, 6);
    CloseHandle(port);
}

TEST(aio_iocp_post_fifo_order) {
    REQUIRE(U(PostQueuedCompletionStatus));
    HANDLE port = new_port(0); const int N = 2000;
    for (int i = 0; i < N; i++) CHECK(U(PostQueuedCompletionStatus)(port, i, (ULONG_PTR)i * 3, NULL));
    int bad = 0;
    for (int i = 0; i < N; i++) { gq g = getq(port, 1000); if (!g.r || g.n != (DWORD)i || g.key != (ULONG_PTR)i * 3) bad++; }
    EXPECT_EQ(bad, 0);
    gq g = getq(port, 0); CHECK(!g.r); EXPECT_EQ(g.err, WAIT_TIMEOUT);
    CloseHandle(port);
}

TEST(aio_iocp_post_via_duplicate_handle) {
    REQUIRE(U(PostQueuedCompletionStatus));
    HANDLE port = new_port(0), d = NULL;
    CHECK(DuplicateHandle(GetCurrentProcess(), port, GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS));
    CHECK(U(PostQueuedCompletionStatus)(d, 9, 8, NULL));
    gq g = getq(port, 1000); CHECK(g.r); EXPECT_EQ(g.n, 9); EXPECT_EQ(g.key, 8);
    CloseHandle(port);   /* d still keeps the port alive */
    CHECK(U(PostQueuedCompletionStatus)(d, 1, 1, NULL));
    g = getq(d, 1000); CHECK(g.r); EXPECT_EQ(g.n, 1);
    CloseHandle(d);
}

TEST(aio_iocp_access_rights_enforced) {
    REQUIRE(U(PostQueuedCompletionStatus));
    HANDLE port = new_port(0), d = NULL;
    CHECK(DuplicateHandle(GetCurrentProcess(), port, GetCurrentProcess(), &d, SYNCHRONIZE, FALSE, 0));
    SET_ERR(0); CHECK(!U(PostQueuedCompletionStatus)(d, 1, 1, NULL)); EXPECT_ERR(ERROR_ACCESS_DENIED);
    gq g = getq(d, 0); CHECK(!g.r); EXPECT_EQ(g.err, ERROR_ACCESS_DENIED);
    CloseHandle(d);
    CHECK(DuplicateHandle(GetCurrentProcess(), port, GetCurrentProcess(), &d, 2 /*IO_COMPLETION_MODIFY_STATE*/, FALSE, 0));
    CHECK(U(PostQueuedCompletionStatus)(d, 1, 1, NULL));
    g = getq(d, 1000); CHECK(g.r);
    CloseHandle(d); CloseHandle(port);
}

TEST(aio_iocp_file_read_completion_packet) {
    REQUIRE(U(ReadFile));
    char p[MAX_PATH]; mkpath(p, "pk"); make_file(p, 16384);
    HANDLE f = open_ov(p, GENERIC_READ, 0); HANDLE port = new_port(0);
    CHECK(U(CreateIoCompletionPort)(f, port, 0x55, 0) == port);
    OVERLAPPED ov; ovinit(&ov, 4096, NULL); BYTE b[1000];
    BOOL r = U(ReadFile)(f, b, 1000, NULL, &ov);
    if (!r) { EXPECT_ERR(ERROR_IO_PENDING); }
    gq g = getq(port, 5000);
    CHECKF(g.r, "err %lu", g.err); CHECK(g.ov == &ov); EXPECT_EQ(g.key, 0x55); EXPECT_EQ(g.n, 1000);
    CHECK(chkp(b, 4096, 1000));
    g = getq(port, 50); CHECK(!g.r); EXPECT_EQ(g.err, WAIT_TIMEOUT);
    CloseHandle(f); CloseHandle(port);
}

TEST(aio_iocp_file_write_completion_packet) {
    REQUIRE(U(WriteFile));
    char p[MAX_PATH]; mkpath(p, "pkw"); HANDLE f = new_ov_file(p, 0); HANDLE port = new_port(0);
    CHECK(U(CreateIoCompletionPort)(f, port, 0x66, 0) == port);
    OVERLAPPED ov; ovinit(&ov, 0, NULL); BYTE b[777]; fillp(b, 0, 777);
    BOOL r = U(WriteFile)(f, b, 777, NULL, &ov);
    if (!r) { EXPECT_ERR(ERROR_IO_PENDING); }
    gq g = getq(port, 5000); CHECK(g.r); CHECK(g.ov == &ov); EXPECT_EQ(g.key, 0x66); EXPECT_EQ(g.n, 777);
    EXPECT_EQ((DWORD)ov.Internal, 0); EXPECT_EQ(ov.InternalHigh, 777);
    CloseHandle(f); CloseHandle(port);
}

TEST(aio_iocp_zero_length_read_packet) {
    REQUIRE(U(ReadFile));
    char p[MAX_PATH]; mkpath(p, "pk0"); make_file(p, 4096);
    HANDLE f = open_ov(p, GENERIC_READ, 0); HANDLE port = new_port(0);
    U(CreateIoCompletionPort)(f, port, 4, 0);
    OVERLAPPED ov; ovinit(&ov, 0, NULL); BYTE b[4];
    BOOL r = U(ReadFile)(f, b, 0, NULL, &ov);
    if (!r) { EXPECT_ERR(ERROR_IO_PENDING); }
    gq g = getq(port, 5000); CHECK(g.r); CHECK(g.ov == &ov); EXPECT_EQ(g.n, 0);
    CloseHandle(f); CloseHandle(port);
}

TEST(aio_iocp_eof_read_observed) {
    REQUIRE(U(ReadFile));
    char p[MAX_PATH]; mkpath(p, "pkeof"); make_file(p, 4096);
    HANDLE f = open_ov(p, GENERIC_READ, 0); HANDLE port = new_port(0);
    U(CreateIoCompletionPort)(f, port, 4, 0);
    OVERLAPPED ov; ovinit(&ov, 4096, NULL); BYTE b[4];
    SET_ERR(0); BOOL r = U(ReadFile)(f, b, 4, NULL, &ov);
    CHECK(!r); EXPECT_ERR(ERROR_HANDLE_EOF);
    gq g = getq(port, 300);
    t_note("EOF read on IOCP handle: packet %s (err %lu)", g.r || g.ov ? "queued" : "not queued", g.err);
    CloseHandle(f); CloseHandle(port);
}

TEST(aio_iocp_failed_io_returns_false_with_overlapped) {
    REQUIRE(U(ReadFile)); REQUIRE(U(CancelIoEx));
    HANDLE s, c; CHECK(make_pipe(&s, &c)); HANDLE port = new_port(0);
    CHECK(U(CreateIoCompletionPort)(s, port, 0x77, 0) == port);
    OVERLAPPED ov; BYTE b[16]; CHECK(pending_pipe_read(s, &ov, NULL, b, 16));
    gq g = getq(port, 50); CHECK(!g.r); EXPECT_EQ(g.err, WAIT_TIMEOUT); CHECK(g.ov == NULL);
    CHECK(U(CancelIoEx)(s, &ov));
    g = getq(port, 5000);
    CHECK(!g.r); EXPECT_EQ(g.err, ERROR_OPERATION_ABORTED); CHECK(g.ov == &ov); EXPECT_EQ(g.key, 0x77); EXPECT_EQ(g.n, 0);
    EXPECT_EQ((DWORD)ov.Internal, NTS_CANCELLED);
    closeh(&s); closeh(&c); CloseHandle(port);
}

TEST(aio_iocp_broken_pipe_packet) {
    REQUIRE(U(ReadFile));
    HANDLE s, c; CHECK(make_pipe(&s, &c)); HANDLE port = new_port(0);
    U(CreateIoCompletionPort)(s, port, 5, 0);
    OVERLAPPED ov; BYTE b[16]; CHECK(pending_pipe_read(s, &ov, NULL, b, 16));
    closeh(&c);
    gq g = getq(port, 5000); CHECK(!g.r); EXPECT_EQ(g.err, ERROR_BROKEN_PIPE); CHECK(g.ov == &ov); EXPECT_EQ(g.key, 5);
    closeh(&s); CloseHandle(port);
}

TEST(aio_iocp_close_handle_pending_io_packet) {
    REQUIRE(U(ReadFile));
    HANDLE s, c; CHECK(make_pipe(&s, &c)); HANDLE port = new_port(0);
    U(CreateIoCompletionPort)(s, port, 6, 0);
    OVERLAPPED ov; BYTE b[16]; CHECK(pending_pipe_read(s, &ov, NULL, b, 16));
    closeh(&s);
    gq g = getq(port, 5000); CHECK(!g.r); EXPECT_EQ(g.err, ERROR_OPERATION_ABORTED); CHECK(g.ov == &ov); EXPECT_EQ(g.key, 6);
    closeh(&c); CloseHandle(port);
}

TEST(aio_iocp_packets_survive_handle_close) {
    REQUIRE(U(ReadFile));
    char p[MAX_PATH]; mkpath(p, "surv"); make_file(p, 4096);
    HANDLE f = open_ov(p, GENERIC_READ, 0); HANDLE port = new_port(0);
    U(CreateIoCompletionPort)(f, port, 9, 0);
    OVERLAPPED ov; ovinit(&ov, 0, NULL); BYTE b[100];
    BOOL r = U(ReadFile)(f, b, 100, NULL, &ov); CHECK(r || GetLastError() == ERROR_IO_PENDING);
    DWORD n; GetOverlappedResult(f, &ov, &n, TRUE);
    CloseHandle(f);
    gq g = getq(port, 2000); CHECK(g.r); CHECK(g.ov == &ov); EXPECT_EQ(g.n, 100); EXPECT_EQ(g.key, 9);
    CloseHandle(port);
}

TEST(aio_iocp_two_handles_distinct_keys) {
    REQUIRE(U(ReadFile));
    char p1[MAX_PATH], p2[MAX_PATH]; mkpath(p1, "k1"); mkpath(p2, "k2"); make_file(p1, 4096); make_file(p2, 4096);
    HANDLE f1 = open_ov(p1, GENERIC_READ, 0), f2 = open_ov(p2, GENERIC_READ, 0); HANDLE port = new_port(0);
    U(CreateIoCompletionPort)(f1, port, 0xA1, 0); U(CreateIoCompletionPort)(f2, port, 0xB2, 0);
    OVERLAPPED o1, o2; ovinit(&o1, 0, NULL); ovinit(&o2, 0, NULL); BYTE b1[10], b2[20];
    U(ReadFile)(f1, b1, 10, NULL, &o1); U(ReadFile)(f2, b2, 20, NULL, &o2);
    int seen1 = 0, seen2 = 0;
    for (int i = 0; i < 2; i++) {
        gq g = getq(port, 5000); CHECK(g.r);
        if (g.ov == &o1) { seen1++; EXPECT_EQ(g.key, 0xA1); EXPECT_EQ(g.n, 10); }
        else if (g.ov == &o2) { seen2++; EXPECT_EQ(g.key, 0xB2); EXPECT_EQ(g.n, 20); }
    }
    EXPECT_EQ(seen1, 1); EXPECT_EQ(seen2, 1);
    CloseHandle(f1); CloseHandle(f2); CloseHandle(port);
}

TEST(aio_iocp_lowbit_event_suppresses_packet) {
    REQUIRE(U(ReadFile));
    HANDLE s, c; CHECK(make_pipe(&s, &c)); HANDLE port = new_port(0), ev = mkevent();
    U(CreateIoCompletionPort)(s, port, 1, 0);
    OVERLAPPED ov; BYTE b[16];
    ovinit(&ov, 0, (HANDLE)((ULONG_PTR)ev | 1));
    SET_ERR(0); BOOL r = U(ReadFile)(s, b, 16, NULL, &ov); CHECK(!r); EXPECT_ERR(ERROR_IO_PENDING);
    OVERLAPPED wo; HANDLE we = mkevent(); ovinit(&wo, 0, we); CHECK(do_write(c, "zzzz", 4, &wo).ok);
    CHECK(WaitForSingleObject(ev, 5000) == WAIT_OBJECT_0);
    DWORD n; CHECK(GetOverlappedResult(s, &ov, &n, FALSE)); EXPECT_EQ(n, 4);
    /* the write on the same handle? (write was on c, not associated). nothing queued for the read */
    gq g = getq(port, 100); CHECK(!g.r); EXPECT_EQ(g.err, WAIT_TIMEOUT); CHECK(g.ov == NULL);
    CloseHandle(we); CloseHandle(ev); closeh(&s); closeh(&c); CloseHandle(port);
}

TEST(aio_iocp_event_and_packet_both) {
    REQUIRE(U(ReadFile));
    HANDLE s, c; CHECK(make_pipe(&s, &c)); HANDLE port = new_port(0), ev = mkevent();
    U(CreateIoCompletionPort)(s, port, 1, 0);
    OVERLAPPED ov; BYTE b[16]; CHECK(pending_pipe_read(s, &ov, ev, b, 16));
    OVERLAPPED wo; HANDLE we = mkevent(); ovinit(&wo, 0, we); CHECK(do_write(c, "zzzz", 4, &wo).ok);
    CHECK(WaitForSingleObject(ev, 5000) == WAIT_OBJECT_0);
    gq g = getq(port, 2000); CHECK(g.r); CHECK(g.ov == &ov); EXPECT_EQ(g.n, 4);
    CloseHandle(we); CloseHandle(ev); closeh(&s); closeh(&c); CloseHandle(port);
}

TEST(aio_iocp_write_on_associated_handle_lowbit) {
    REQUIRE(U(WriteFile));
    char p[MAX_PATH]; mkpath(p, "lbw"); HANDLE f = new_ov_file(p, 0); HANDLE port = new_port(0), ev = mkevent();
    U(CreateIoCompletionPort)(f, port, 1, 0);
    OVERLAPPED ov; ovinit(&ov, 0, (HANDLE)((ULONG_PTR)ev | 1)); BYTE b[64] = {1};
    BOOL r = U(WriteFile)(f, b, 64, NULL, &ov); CHECK(r || GetLastError() == ERROR_IO_PENDING);
    CHECK(WaitForSingleObject(ev, 5000) == WAIT_OBJECT_0);
    gq g = getq(port, 100); CHECK(!g.r); EXPECT_EQ(g.err, WAIT_TIMEOUT);
    CloseHandle(ev); CloseHandle(f); CloseHandle(port);
}

TEST(aio_iocp_skip_on_success_file) {
    REQUIRE(U(SetFileCompletionNotificationModes)); REQUIRE(U(ReadFile));
    char p[MAX_PATH]; mkpath(p, "skip"); make_file(p, 65536);
    HANDLE f = open_ov(p, GENERIC_READ, 0); HANDLE port = new_port(0);
    U(CreateIoCompletionPort)(f, port, 3, 0);
    SET_ERR(0); CHECK(U(SetFileCompletionNotificationModes)(f, FILE_SKIP_COMPLETION_PORT_ON_SUCCESS));
    int immediate = 0, deferred = 0;
    for (int i = 0; i < 50; i++) {
        OVERLAPPED ov; ovinit(&ov, (ULONGLONG)i * 1024, NULL); BYTE b[1024];
        SET_ERR(0); BOOL r = U(ReadFile)(f, b, 1024, NULL, &ov);
        if (r) {
            immediate++;
            gq g = getq(port, 0); CHECKF(!g.r && g.err == WAIT_TIMEOUT, "iteration %d: packet for immediate success", i);
        } else {
            CHECK(GetLastError() == ERROR_IO_PENDING); deferred++;
            gq g = getq(port, 5000); CHECK(g.r); CHECK(g.ov == &ov);
        }
    }
    t_note("skip-on-success file reads: %d immediate, %d deferred", immediate, deferred);
    CloseHandle(f); CloseHandle(port);
}

TEST(aio_iocp_no_skip_always_packet) {
    REQUIRE(U(ReadFile));
    char p[MAX_PATH]; mkpath(p, "noskip"); make_file(p, 65536);
    HANDLE f = open_ov(p, GENERIC_READ, 0); HANDLE port = new_port(0);
    U(CreateIoCompletionPort)(f, port, 3, 0);
    for (int i = 0; i < 20; i++) {
        OVERLAPPED ov; ovinit(&ov, (ULONGLONG)i * 1024, NULL); BYTE b[1024];
        BOOL r = U(ReadFile)(f, b, 1024, NULL, &ov); CHECK(r || GetLastError() == ERROR_IO_PENDING);
        gq g = getq(port, 5000); CHECKF(g.r && g.ov == &ov, "iteration %d", i);
    }
    CloseHandle(f); CloseHandle(port);
}

TEST(aio_iocp_skip_on_success_pipe_pending_still_queued) {
    REQUIRE(U(SetFileCompletionNotificationModes)); REQUIRE(U(ReadFile));
    HANDLE s, c; CHECK(make_pipe(&s, &c)); HANDLE port = new_port(0);
    U(CreateIoCompletionPort)(s, port, 8, 0);
    CHECK(U(SetFileCompletionNotificationModes)(s, FILE_SKIP_COMPLETION_PORT_ON_SUCCESS));
    OVERLAPPED ov; BYTE b[16]; CHECK(pending_pipe_read(s, &ov, NULL, b, 16));
    OVERLAPPED wo; HANDLE we = mkevent(); ovinit(&wo, 0, we); CHECK(do_write(c, "ab", 2, &wo).ok);
    gq g = getq(port, 5000); CHECK(g.r); CHECK(g.ov == &ov); EXPECT_EQ(g.n, 2);
    /* immediate write success on the associated handle: no packet */
    OVERLAPPED wo2; ovinit(&wo2, 0, NULL);
    BOOL r = U(WriteFile)(s, "q", 1, NULL, &wo2);
    if (r) { g = getq(port, 100); CHECK(!g.r); EXPECT_EQ(g.err, WAIT_TIMEOUT); }
    else { g = getq(port, 5000); CHECK(g.r); }
    CloseHandle(we); closeh(&s); closeh(&c); CloseHandle(port);
}

TEST(aio_iocp_skip_set_event_on_handle) {
    REQUIRE(U(SetFileCompletionNotificationModes)); REQUIRE(U(ReadFile));
    char p[MAX_PATH]; mkpath(p, "skipev"); make_file(p, 8192);
    HANDLE f1 = open_ov(p, GENERIC_READ, 0), f2 = open_ov(p, GENERIC_READ, 0); HANDLE port = new_port(0);
    U(CreateIoCompletionPort)(f1, port, 1, 0); U(CreateIoCompletionPort)(f2, port, 2, 0);
    CHECK(U(SetFileCompletionNotificationModes)(f2, FILE_SKIP_SET_EVENT_ON_HANDLE));
    OVERLAPPED o1, o2; BYTE b[100];
    ovinit(&o1, 0, NULL); ovinit(&o2, 0, NULL);
    U(ReadFile)(f1, b, 100, NULL, &o1); gq g = getq(port, 5000); CHECK(g.r);
    U(ReadFile)(f2, b, 100, NULL, &o2); g = getq(port, 5000); CHECK(g.r);
    CHECK(signaled(f1));
    CHECK(not_signaled(f2));
    CloseHandle(f1); CloseHandle(f2); CloseHandle(port);
}

TEST(aio_iocp_setmodes_validation) {
    REQUIRE(U(SetFileCompletionNotificationModes));
    char p[MAX_PATH]; mkpath(p, "modes"); make_file(p, 100);
    HANDLE f = open_ov(p, GENERIC_READ, 0);
    static const UCHAR badf[] = {4, 8, 0x10, 0x80, 0xFC, 0xFF, 5, 7};
    for (unsigned i = 0; i < sizeof badf; i++) {
        SET_ERR(0); CHECKF(!U(SetFileCompletionNotificationModes)(f, badf[i]), "flags %x", badf[i]); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
    CHECK(U(SetFileCompletionNotificationModes)(f, 0));
    CHECK(U(SetFileCompletionNotificationModes)(INVALID_HANDLE_VALUE, 0));   /* flags==0 never reaches the kernel */
    CHECK(U(SetFileCompletionNotificationModes)(NULL, 0));
    SET_ERR(0); CHECK(!U(SetFileCompletionNotificationModes)(INVALID_HANDLE_VALUE, 1)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(SetFileCompletionNotificationModes)(NULL, 2)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE port = new_port(0);
    U(CreateIoCompletionPort)(f, port, 1, 0);
    CHECK(U(SetFileCompletionNotificationModes)(f, 3));
    CHECK(U(SetFileCompletionNotificationModes)(f, 1));
    CHECK(U(SetFileCompletionNotificationModes)(f, 2));
    CloseHandle(f);
    SET_ERR(0); CHECK(!U(SetFileCompletionNotificationModes)(f, 1)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(port);
}

TEST(aio_iocp_setmodes_unassociated_and_sync_note) {
    REQUIRE(U(SetFileCompletionNotificationModes));
    char p[MAX_PATH]; mkpath(p, "modes2"); make_file(p, 100);
    HANDLE f = open_ov(p, GENERIC_READ, 0), s = open_sync(p, GENERIC_READ);
    SET_ERR(0); BOOL r = U(SetFileCompletionNotificationModes)(f, 1);
    t_note("modes on overlapped, not yet associated handle: %d err %lu", r, r ? 0 : GetLastError());
    SET_ERR(0); r = U(SetFileCompletionNotificationModes)(s, 1);
    t_note("modes on synchronous handle: %d err %lu", r, r ? 0 : GetLastError());
    CloseHandle(f); CloseHandle(s);
}

/* ---- GetQueuedCompletionStatusEx ---- */
TEST(aio_iocp_gqcsex_invalid_args) {
    REQUIRE(U(GetQueuedCompletionStatusEx));
    HANDLE port = new_port(0); OVERLAPPED_ENTRY e[2]; ULONG rem = 77;
    SET_ERR(0); CHECK(!U(GetQueuedCompletionStatusEx)(port, NULL, 2, &rem, 0, FALSE)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!U(GetQueuedCompletionStatusEx)(port, e, 0, &rem, 0, FALSE)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!U(GetQueuedCompletionStatusEx)(port, e, 2, NULL, 0, FALSE)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    HANDLE ev = mkevent();
    HANDLE bad[3] = {INVALID_HANDLE_VALUE, NULL, ev};
    for (int i = 0; i < 3; i++) { SET_ERR(0); CHECK(!U(GetQueuedCompletionStatusEx)(bad[i], e, 2, &rem, 0, FALSE)); EXPECT_ERR(ERROR_INVALID_HANDLE); }
    CloseHandle(ev); CloseHandle(port);
}

TEST(aio_iocp_gqcsex_timeout) {
    REQUIRE(U(GetQueuedCompletionStatusEx));
    HANDLE port = new_port(0); OVERLAPPED_ENTRY e[4]; ULONG rem = 0;
    SET_ERR(0); CHECK(!U(GetQueuedCompletionStatusEx)(port, e, 4, &rem, 0, FALSE)); EXPECT_ERR(WAIT_TIMEOUT);
    DWORD t0 = GetTickCount();
    SET_ERR(0); CHECK(!U(GetQueuedCompletionStatusEx)(port, e, 4, &rem, 120, FALSE)); EXPECT_ERR(WAIT_TIMEOUT);
    DWORD dt = GetTickCount() - t0; CHECKF(dt >= 100 && dt < 3000, "dt=%lu", dt);
    SET_ERR(0); CHECK(!U(GetQueuedCompletionStatusEx)(port, e, 4, &rem, 50, TRUE)); EXPECT_ERR(WAIT_TIMEOUT);
    CloseHandle(port);
}

TEST(aio_iocp_gqcsex_bulk_dequeue) {
    REQUIRE(U(GetQueuedCompletionStatusEx)); REQUIRE(U(PostQueuedCompletionStatus));
    HANDLE port = new_port(0); OVERLAPPED_ENTRY e[100]; ULONG rem = 0;
    for (int i = 0; i < 10; i++) U(PostQueuedCompletionStatus)(port, 100 + i, 1000 + i, (OVERLAPPED *)(ULONG_PTR)(0x10000 + i * 16));
    SET_ERR(0); BOOL r = U(GetQueuedCompletionStatusEx)(port, e, 4, &rem, 1000, FALSE);
    CHECK(r); EXPECT_EQ(rem, 4);
    for (int i = 0; i < 4; i++) { EXPECT_EQ(e[i].lpCompletionKey, 1000 + i); EXPECT_EQ(e[i].dwNumberOfBytesTransferred, 100 + i); CHECK(e[i].lpOverlapped == (OVERLAPPED *)(ULONG_PTR)(0x10000 + i * 16)); }
    memset(e, 0, sizeof e);
    r = U(GetQueuedCompletionStatusEx)(port, e, 100, &rem, 1000, FALSE);
    CHECK(r); EXPECT_EQ(rem, 6);
    for (int i = 0; i < 6; i++) { EXPECT_EQ(e[i].lpCompletionKey, 1004 + i); EXPECT_EQ(e[i].dwNumberOfBytesTransferred, 104 + i); }
    SET_ERR(0); CHECK(!U(GetQueuedCompletionStatusEx)(port, e, 100, &rem, 0, FALSE)); EXPECT_ERR(WAIT_TIMEOUT);
    CloseHandle(port);
}

TEST(aio_iocp_gqcsex_count_one) {
    REQUIRE(U(GetQueuedCompletionStatusEx));
    HANDLE port = new_port(0); OVERLAPPED_ENTRY e[1]; ULONG rem = 0;
    for (int i = 0; i < 3; i++) U(PostQueuedCompletionStatus)(port, i, i + 1, NULL);
    for (int i = 0; i < 3; i++) { CHECK(U(GetQueuedCompletionStatusEx)(port, e, 1, &rem, 100, FALSE)); EXPECT_EQ(rem, 1); EXPECT_EQ(e[0].lpCompletionKey, i + 1); }
    CloseHandle(port);
}

TEST(aio_iocp_gqcsex_failed_io_entry) {
    REQUIRE(U(GetQueuedCompletionStatusEx)); REQUIRE(U(ReadFile)); REQUIRE(U(CancelIoEx));
    HANDLE s, c; CHECK(make_pipe(&s, &c)); HANDLE port = new_port(0);
    U(CreateIoCompletionPort)(s, port, 0x33, 0);
    OVERLAPPED ov; BYTE b[16]; CHECK(pending_pipe_read(s, &ov, NULL, b, 16));
    U(CancelIoEx)(s, &ov);
    OVERLAPPED_ENTRY e[2]; ULONG rem = 0;
    BOOL r = U(GetQueuedCompletionStatusEx)(port, e, 2, &rem, 5000, FALSE);
    /* Ex reports failed IO as a successful dequeue; status is in OVERLAPPED.Internal */
    CHECK(r); EXPECT_EQ(rem, 1); CHECK(e[0].lpOverlapped == &ov); EXPECT_EQ(e[0].lpCompletionKey, 0x33);
    EXPECT_EQ((DWORD)ov.Internal, NTS_CANCELLED);
    closeh(&s); closeh(&c); CloseHandle(port);
}

TEST(aio_iocp_gqcsex_alertable_apc) {
    REQUIRE(U(GetQueuedCompletionStatusEx)); REQUIRE(U(QueueUserAPC));
    HANDLE port = new_port(0); OVERLAPPED_ENTRY e[2]; ULONG rem = 0; volatile LONG hit = 0;
    CHECK(U(QueueUserAPC)(noop_apc_ip, GetCurrentThread(), (ULONG_PTR)&hit));
    SET_ERR(0); CHECK(!U(GetQueuedCompletionStatusEx)(port, e, 2, &rem, 5000, TRUE)); EXPECT_ERR(WAIT_IO_COMPLETION);
    EXPECT_EQ(hit, 1);
    CloseHandle(port);
}
