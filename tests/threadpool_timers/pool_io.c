/* Threadpool I/O completion objects (files and named pipes). */
#include "tp.h"

#define NEED_IO() REQUIRE(U(CreateThreadpoolIo)); REQUIRE(U(StartThreadpoolIo)); REQUIRE(U(CancelThreadpoolIo)); \
    REQUIRE(U(WaitForThreadpoolIoCallbacks)); REQUIRE(U(CloseThreadpoolIo))

typedef struct {
    volatile LONG n;
    PVOID ov[32]; ULONG res[32]; ULONG_PTR bytes[32]; PVOID io[32]; PVOID ctx[32]; DWORD tid[32];
    HANDLE ev;
} io_rec;

static VOID NTAPI io_cb(PTP_CALLBACK_INSTANCE inst, PVOID ctx, PVOID ov, ULONG res, ULONG_PTR bytes, PTP_IO io) {
    io_rec *r = (io_rec *)ctx;
    LONG k = InterlockedIncrement(&r->n) - 1;
    if (k < 32) { r->ov[k] = ov; r->res[k] = res; r->bytes[k] = bytes; r->io[k] = io; r->ctx[k] = ctx; r->tid[k] = GetCurrentThreadId(); }
    SetEvent(r->ev);
}
static void io_rec_init(io_rec *r) { memset(r, 0, sizeof *r); r->ev = tp_ev(FALSE); }
static void io_rec_free(io_rec *r) { CloseHandle(r->ev); }

static HANDLE open_overlapped_file(const char *leaf) {
    char p[MAX_PATH];
    t_tmpfile(p, sizeof p, leaf);
    return CreateFileA(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS,
                       FILE_FLAG_OVERLAPPED, NULL);
}
static HANDLE open_sync_file(const char *leaf) {
    char p[MAX_PATH];
    t_tmpfile(p, sizeof p, leaf);
    return CreateFileA(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, NULL);
}
/* Issue an overlapped write; true when pending or completed. */
static BOOL do_write(HANDLE h, OVERLAPPED *ov, const void *buf, DWORD n, ULONGLONG off) {
    memset(ov, 0, sizeof *ov);
    ov->Offset = (DWORD)off; ov->OffsetHigh = (DWORD)(off >> 32);
    DWORD got = 0;
    if (WriteFile(h, buf, n, &got, ov)) return TRUE;
    return GetLastError() == ERROR_IO_PENDING;
}
static BOOL do_read(HANDLE h, OVERLAPPED *ov, void *buf, DWORD n, ULONGLONG off) {
    memset(ov, 0, sizeof *ov);
    ov->Offset = (DWORD)off; ov->OffsetHigh = (DWORD)(off >> 32);
    DWORD got = 0;
    if (ReadFile(h, buf, n, &got, ov)) return TRUE;
    return GetLastError() == ERROR_IO_PENDING;
}

TEST(tp_io_create_close) {
    NEED_IO();
    io_rec r; io_rec_init(&r);
    HANDLE h = open_overlapped_file("io_cc.bin");
    SET_ERR(0);
    PTP_IO io = U(CreateThreadpoolIo)(h, io_cb, &r, NULL);
    CHECK(io != NULL);
    U(CloseThreadpoolIo)(io);
    CloseHandle(h); io_rec_free(&r);
}
TEST(tp_io_write_completion_callback) {
    NEED_IO();
    io_rec r; io_rec_init(&r);
    HANDLE h = open_overlapped_file("io_w.bin");
    PTP_IO io = U(CreateThreadpoolIo)(h, io_cb, &r, NULL);
    CHECK(io != NULL);
    static char buf[4096];
    memset(buf, 'Q', sizeof buf);
    OVERLAPPED ov;
    U(StartThreadpoolIo)(io);
    BOOL ok = do_write(h, &ov, buf, sizeof buf, 0);
    CHECK(ok);
    if (!ok) U(CancelThreadpoolIo)(io);
    CHECK(tp_signaled(r.ev, TP_WAIT_MS));
    U(WaitForThreadpoolIoCallbacks)(io, FALSE);
    EXPECT_EQ(r.n, 1);
    CHECK(r.ov[0] == (PVOID)&ov);
    EXPECT_EQ(r.res[0], NO_ERROR);
    EXPECT_EQ(r.bytes[0], sizeof buf);
    CHECK(r.io[0] == (PVOID)io);
    CHECK(r.ctx[0] == (PVOID)&r);
    CHECK(r.tid[0] != GetCurrentThreadId());
    U(CloseThreadpoolIo)(io);
    CloseHandle(h); io_rec_free(&r);
}
TEST(tp_io_read_completion_and_data) {
    NEED_IO();
    io_rec r; io_rec_init(&r);
    HANDLE h = open_overlapped_file("io_r.bin");
    PTP_IO io = U(CreateThreadpoolIo)(h, io_cb, &r, NULL);
    static char out[1000], in[1000];
    for (int i = 0; i < 1000; i++) out[i] = (char)(i * 7 + 3);
    OVERLAPPED ov;
    U(StartThreadpoolIo)(io);
    CHECK(do_write(h, &ov, out, sizeof out, 0));
    CHECK(tp_signaled(r.ev, TP_WAIT_MS));
    U(StartThreadpoolIo)(io);
    CHECK(do_read(h, &ov, in, sizeof in, 0));
    CHECK(tp_wait_count(&r.n, 2, TP_WAIT_MS));
    U(WaitForThreadpoolIoCallbacks)(io, FALSE);
    EXPECT_EQ(r.res[1], NO_ERROR);
    EXPECT_EQ(r.bytes[1], sizeof in);
    CHECK(memcmp(in, out, sizeof in) == 0);
    U(CloseThreadpoolIo)(io);
    CloseHandle(h); io_rec_free(&r);
}
TEST(tp_io_many_concurrent_operations) {
    NEED_IO();
    io_rec r; io_rec_init(&r);
    HANDLE h = open_overlapped_file("io_many.bin");
    PTP_IO io = U(CreateThreadpoolIo)(h, io_cb, &r, NULL);
    enum { N = 16 };
    static char buf[N][512];
    OVERLAPPED ov[N];
    for (int i = 0; i < N; i++) {
        memset(buf[i], 'A' + i, 512);
        U(StartThreadpoolIo)(io);
        if (!do_write(h, &ov[i], buf[i], 512, (ULONGLONG)i * 512)) { U(CancelThreadpoolIo)(io); CHECKF(0, "write %d", i); }
    }
    CHECK(tp_wait_count(&r.n, N, TP_WAIT_MS));
    U(WaitForThreadpoolIoCallbacks)(io, FALSE);
    EXPECT_EQ(r.n, N);
    ULONG_PTR total = 0; int seen[N] = { 0 };
    for (int i = 0; i < N && i < 32; i++) {
        total += r.bytes[i];
        EXPECT_EQ(r.res[i], NO_ERROR);
        for (int j = 0; j < N; j++) if (r.ov[i] == (PVOID)&ov[j]) seen[j]++;
    }
    EXPECT_EQ(total, N * 512);
    for (int j = 0; j < N; j++) CHECKF(seen[j] == 1, "overlapped %d reported %d times", j, seen[j]);
    U(CloseThreadpoolIo)(io);
    CloseHandle(h); io_rec_free(&r);
}

TEST(tp_io_start_cancel_without_io) {
    NEED_IO();
    io_rec r; io_rec_init(&r);
    HANDLE h = open_overlapped_file("io_sc.bin");
    PTP_IO io = U(CreateThreadpoolIo)(h, io_cb, &r, NULL);
    for (int i = 0; i < 10; i++) { U(StartThreadpoolIo)(io); U(CancelThreadpoolIo)(io); }
    ULONGLONG t0 = tp_now();
    U(WaitForThreadpoolIoCallbacks)(io, FALSE);   /* nothing outstanding: returns at once */
    CHECK(tp_now() - t0 < 2000);
    EXPECT_EQ(r.n, 0);
    U(CloseThreadpoolIo)(io);
    CloseHandle(h); io_rec_free(&r);
}
TEST(tp_io_cancel_after_failed_issue) {
    NEED_IO();
    io_rec r; io_rec_init(&r);
    HANDLE h = open_overlapped_file("io_eof.bin");
    PTP_IO io = U(CreateThreadpoolIo)(h, io_cb, &r, NULL);
    char b[16];
    OVERLAPPED ov;
    memset(&ov, 0, sizeof ov);
    ov.Offset = 0x1000;                           /* beyond end of the empty file */
    U(StartThreadpoolIo)(io);
    DWORD got = 0;
    BOOL ok = ReadFile(h, b, sizeof b, &got, &ov);
    DWORD e = GetLastError();
    if (!ok && e != ERROR_IO_PENDING) {
        EXPECT_EQ(e, ERROR_HANDLE_EOF);
        U(CancelThreadpoolIo)(io);                /* documented pattern: cancel when the call itself failed */
    }
    ULONGLONG t0 = tp_now();
    U(WaitForThreadpoolIoCallbacks)(io, FALSE);
    CHECK(tp_now() - t0 < 5000);
    t_note("read past EOF: ok=%d err=%lu callbacks=%ld", ok, e, (long)r.n);
    U(CloseThreadpoolIo)(io);
    CloseHandle(h); io_rec_free(&r);
}
TEST(tp_io_wait_blocks_until_pending_io_completes) {
    NEED_IO();
    io_rec r; io_rec_init(&r);
    char name[128];
    snprintf(name, sizeof name, "\\\\.\\pipe\\k32t_tpio_a_%lu", GetCurrentProcessId());
    HANDLE srv = CreateNamedPipeA(name, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, NULL);
    CHECK(srv != INVALID_HANDLE_VALUE);
    if (srv == INVALID_HANDLE_VALUE) { io_rec_free(&r); return; }
    HANDLE cli = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(cli != INVALID_HANDLE_VALUE);
    PTP_IO io = U(CreateThreadpoolIo)(srv, io_cb, &r, NULL);
    CHECK(io != NULL);
    char buf[64]; OVERLAPPED ov;
    U(StartThreadpoolIo)(io);
    CHECK(do_read(srv, &ov, buf, sizeof buf, 0));
    EXPECT_EQ(r.n, 0);
    tp_delay_arg da; HANDLE gate = tp_ev(TRUE); HANDLE th = tp_delayed_set(&da, gate, 250);
    DWORD wrote;
    Sleep(50);
    WriteFile(cli, "hello", 5, &wrote, NULL);     /* completes the pending read */
    ULONGLONG t0 = tp_now();
    U(WaitForThreadpoolIoCallbacks)(io, FALSE);
    CHECK(tp_now() - t0 < 5000);
    EXPECT_EQ(r.n, 1);
    EXPECT_EQ(r.res[0], NO_ERROR);
    EXPECT_EQ(r.bytes[0], 5);
    CHECK(memcmp(buf, "hello", 5) == 0);
    tp_join(th); CloseHandle(gate);
    U(CloseThreadpoolIo)(io);
    CloseHandle(cli); CloseHandle(srv); io_rec_free(&r);
}
TEST(tp_io_pipe_connect_read_and_broken_pipe) {
    NEED_IO();
    io_rec r; io_rec_init(&r);
    char name[128];
    snprintf(name, sizeof name, "\\\\.\\pipe\\k32t_tpio_b_%lu", GetCurrentProcessId());
    HANDLE srv = CreateNamedPipeA(name, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, NULL);
    CHECK(srv != INVALID_HANDLE_VALUE);
    if (srv == INVALID_HANDLE_VALUE) { io_rec_free(&r); return; }
    PTP_IO io = U(CreateThreadpoolIo)(srv, io_cb, &r, NULL);
    CHECK(io != NULL);
    /* 1. ConnectNamedPipe completes through the pool. */
    OVERLAPPED ovc; memset(&ovc, 0, sizeof ovc);
    U(StartThreadpoolIo)(io);
    BOOL c = ConnectNamedPipe(srv, &ovc);
    DWORD ce = GetLastError();
    CHECK(c || ce == ERROR_IO_PENDING || ce == ERROR_PIPE_CONNECTED);
    HANDLE cli = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(cli != INVALID_HANDLE_VALUE);
    if (!c && ce == ERROR_PIPE_CONNECTED) U(CancelThreadpoolIo)(io);   /* nothing was queued */
    else CHECK(tp_wait_count(&r.n, 1, TP_WAIT_MS));
    /* 2. data read */
    char buf[32]; OVERLAPPED ovr;
    LONG base = r.n;
    U(StartThreadpoolIo)(io);
    CHECK(do_read(srv, &ovr, buf, sizeof buf, 0));
    DWORD w; WriteFile(cli, "abc", 3, &w, NULL);
    CHECK(tp_wait_count(&r.n, base + 1, TP_WAIT_MS));
    EXPECT_EQ(r.res[base], NO_ERROR);
    EXPECT_EQ(r.bytes[base], 3);
    CHECK(r.ov[base] == (PVOID)&ovr);
    /* 3. peer closes: a new read finishes with ERROR_BROKEN_PIPE */
    base = r.n;
    CloseHandle(cli);
    U(StartThreadpoolIo)(io);
    memset(&ovr, 0, sizeof ovr);
    DWORD got = 0;
    BOOL rd = ReadFile(srv, buf, sizeof buf, &got, &ovr);
    DWORD re = GetLastError();
    if (rd || re == ERROR_IO_PENDING) {
        CHECK(tp_wait_count(&r.n, base + 1, TP_WAIT_MS));
        EXPECT_EQ(r.res[base], ERROR_BROKEN_PIPE);
        EXPECT_EQ(r.bytes[base], 0);
    } else {
        /* A read on a pipe whose peer has gone fails at once: the documented pattern is to cancel. */
        EXPECT_EQ(re, ERROR_BROKEN_PIPE);
        U(CancelThreadpoolIo)(io);
    }
    U(WaitForThreadpoolIoCallbacks)(io, FALSE);
    U(CloseThreadpoolIo)(io);
    CloseHandle(srv); io_rec_free(&r);
}
TEST(tp_io_cancelled_operation_reports_aborted) {
    NEED_IO();
    io_rec r; io_rec_init(&r);
    char name[128];
    snprintf(name, sizeof name, "\\\\.\\pipe\\k32t_tpio_c_%lu", GetCurrentProcessId());
    HANDLE srv = CreateNamedPipeA(name, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, NULL);
    CHECK(srv != INVALID_HANDLE_VALUE);
    if (srv == INVALID_HANDLE_VALUE) { io_rec_free(&r); return; }
    HANDLE cli = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    PTP_IO io = U(CreateThreadpoolIo)(srv, io_cb, &r, NULL);
    char buf[16]; OVERLAPPED ov;
    U(StartThreadpoolIo)(io);
    CHECK(do_read(srv, &ov, buf, sizeof buf, 0));
    Sleep(50);
    CHECK(CancelIoEx(srv, &ov));
    CHECK(tp_signaled(r.ev, TP_WAIT_MS));
    EXPECT_EQ(r.res[0], ERROR_OPERATION_ABORTED);
    EXPECT_EQ(r.bytes[0], 0);
    U(WaitForThreadpoolIoCallbacks)(io, FALSE);
    U(CloseThreadpoolIo)(io);
    CloseHandle(cli); CloseHandle(srv); io_rec_free(&r);
}
TEST(tp_io_private_pool_callback_thread) {
    NEED_IO(); REQUIRE(U(CreateThreadpool)); REQUIRE(U(CloseThreadpool));
    PTP_POOL p = U(CreateThreadpool)(NULL);
    TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e); SetThreadpoolCallbackPool(&e, p);
    io_rec r; io_rec_init(&r);
    HANDLE h = open_overlapped_file("io_pp.bin");
    PTP_IO io = U(CreateThreadpoolIo)(h, io_cb, &r, &e);
    CHECK(io != NULL);
    char buf[100] = { 1 }; OVERLAPPED ov;
    U(StartThreadpoolIo)(io);
    CHECK(do_write(h, &ov, buf, sizeof buf, 0));
    CHECK(tp_signaled(r.ev, TP_WAIT_MS));
    CHECK(r.tid[0] != GetCurrentThreadId());
    U(WaitForThreadpoolIoCallbacks)(io, FALSE);
    U(CloseThreadpoolIo)(io);
    U(CloseThreadpool)(p);
    CloseHandle(h); io_rec_free(&r);
}
TEST(tp_io_handle_remains_usable_after_close) {
    NEED_IO();
    io_rec r; io_rec_init(&r);
    HANDLE h = open_overlapped_file("io_after.bin");
    PTP_IO io = U(CreateThreadpoolIo)(h, io_cb, &r, NULL);
    char buf[10] = "0123456789"; OVERLAPPED ov;
    U(StartThreadpoolIo)(io);
    CHECK(do_write(h, &ov, buf, 10, 0));
    CHECK(tp_signaled(r.ev, TP_WAIT_MS));
    U(WaitForThreadpoolIoCallbacks)(io, FALSE);
    U(CloseThreadpoolIo)(io);
    CHECK(GetFileSize(h, NULL) == 10);            /* the file handle itself is still valid */
    CloseHandle(h); io_rec_free(&r);
}
TEST(tp_io_create_with_invalid_handles) {
    NEED_IO();
    io_rec r; io_rec_init(&r);
    HANDLE evh = tp_ev(FALSE);
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, evh };
    for (int i = 0; i < 3; i++) {
        SET_ERR(0);
        PTP_IO io = U(CreateThreadpoolIo)(bad[i], io_cb, &r, NULL);
        CHECKF(io == NULL, "handle %p", bad[i]);
        DWORD e = GetLastError();
        CHECKF(e != 0, "handle %p: last error not set", bad[i]);
        t_note("CreateThreadpoolIo(%p) -> last error %lu", bad[i], e);
        if (io) U(CloseThreadpoolIo)(io);
    }
    CloseHandle(evh); io_rec_free(&r);
}
TEST(tp_io_second_association_of_same_handle) {
    NEED_IO();
    io_rec r; io_rec_init(&r);
    HANDLE h = open_overlapped_file("io_dup.bin");
    PTP_IO a = U(CreateThreadpoolIo)(h, io_cb, &r, NULL);
    CHECK(a != NULL);
    SET_ERR(0);
    PTP_IO b = U(CreateThreadpoolIo)(h, io_cb, &r, NULL);
    DWORD e = GetLastError();
    t_note("second CreateThreadpoolIo on the same handle: %p (error %lu)", b, e);
    CHECK(b == NULL);                             /* a handle can be bound to one completion port only */
    if (b) U(CloseThreadpoolIo)(b);
    U(CloseThreadpoolIo)(a);
    CloseHandle(h); io_rec_free(&r);
}
TEST(tp_io_synchronous_handle_association) {
    NEED_IO();
    io_rec r; io_rec_init(&r);
    HANDLE h = open_sync_file("io_sync.bin");
    SET_ERR(0);
    PTP_IO io = U(CreateThreadpoolIo)(h, io_cb, &r, NULL);
    DWORD e = GetLastError();
    t_note("CreateThreadpoolIo on a non-overlapped file handle: %p (error %lu)", io, e);
    if (io) U(CloseThreadpoolIo)(io);
    CloseHandle(h); io_rec_free(&r);
}
TEST(tp_io_create_close_loop_with_new_handles) {
    NEED_IO();
    io_rec r; io_rec_init(&r);
    for (int i = 0; i < 50; i++) {
        HANDLE h = open_overlapped_file("io_loop.bin");
        PTP_IO io = U(CreateThreadpoolIo)(h, io_cb, &r, NULL);
        if (!io) { CHECK(io != NULL); CloseHandle(h); break; }
        U(CloseThreadpoolIo)(io);
        CloseHandle(h);
    }
    EXPECT_EQ(r.n, 0);
    io_rec_free(&r);
}
static VOID NTAPI io_cg_cancel(PVOID obj, PVOID cl) { }
TEST(tp_io_in_cleanup_group) {
    NEED_IO(); REQUIRE(U(CreateThreadpoolCleanupGroup)); REQUIRE(U(CloseThreadpoolCleanupGroupMembers));
    REQUIRE(U(CloseThreadpoolCleanupGroup));
    PTP_CLEANUP_GROUP g = U(CreateThreadpoolCleanupGroup)();
    TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e); SetThreadpoolCallbackCleanupGroup(&e, g, io_cg_cancel);
    io_rec r; io_rec_init(&r);
    HANDLE h = open_overlapped_file("io_cg.bin");
    PTP_IO io = U(CreateThreadpoolIo)(h, io_cb, &r, &e);
    CHECK(io != NULL);
    char buf[8] = { 0 }; OVERLAPPED ov;
    U(StartThreadpoolIo)(io);
    CHECK(do_write(h, &ov, buf, 8, 0));
    CHECK(tp_signaled(r.ev, TP_WAIT_MS));
    U(CloseThreadpoolCleanupGroupMembers)(g, FALSE, NULL);   /* closes the io object */
    EXPECT_EQ(r.n, 1);
    U(CloseThreadpoolCleanupGroup)(g);
    CloseHandle(h); io_rec_free(&r);
}
