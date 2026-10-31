#define AIO_TU "ex"
#include "aio.h"

/* ---------- ReadFileEx / WriteFileEx ---------- */
typedef struct { volatile LONG hits; DWORD err[128]; DWORD bytes[128]; OVERLAPPED *ov[128]; DWORD tid[128]; HANDLE ev_seen[128]; } cbrec;
static cbrec g_rec;
#define EQ_CALL(call, want) do { DWORD v_ = (call); EXPECT_EQ(v_, want); } while (0)
static VOID WINAPI ex_cb(DWORD err, DWORD n, LPOVERLAPPED ov) {
    LONG i = g_rec.hits; if (i < 128) { g_rec.err[i] = err; g_rec.bytes[i] = n; g_rec.ov[i] = ov; g_rec.tid[i] = GetCurrentThreadId(); g_rec.ev_seen[i] = ov->hEvent; }
    InterlockedIncrement(&g_rec.hits);
}
static void wait_done(OVERLAPPED *ov) { for (int i = 0; i < 5000 && ov->Internal == NTS_PENDING; i++) Sleep(1); }
static void rec_reset(void) { memset((void *)&g_rec, 0, sizeof g_rec); }

TEST(aio_ex_readfileex_basic) {
    REQUIRE(U(ReadFileEx));
    char p[MAX_PATH]; mkpath(p, "rx"); make_file(p, 16384);
    HANDLE h = open_ov(p, GENERIC_READ, 0); rec_reset();
    OVERLAPPED ov; ovinit(&ov, 512, NULL); BYTE b[2000];
    SET_ERR(0); CHECK(U(ReadFileEx)(h, b, 2000, &ov, ex_cb));
    EXPECT_EQ(g_rec.hits, 0);                 /* not before an alertable wait */
    Sleep(50); EXPECT_EQ(g_rec.hits, 0);
    DWORD r = SleepEx(5000, TRUE);
    EXPECT_EQ(r, WAIT_IO_COMPLETION); EXPECT_EQ(g_rec.hits, 1);
    EXPECT_EQ(g_rec.err[0], 0); EXPECT_EQ(g_rec.bytes[0], 2000); CHECK(g_rec.ov[0] == &ov);
    EXPECT_EQ(g_rec.tid[0], GetCurrentThreadId());
    CHECK(chkp(b, 512, 2000));
    EXPECT_EQ((DWORD)ov.Internal, 0); EXPECT_EQ(ov.InternalHigh, 2000);
    CloseHandle(h);
}

TEST(aio_ex_hevent_is_user_data) {
    REQUIRE(U(ReadFileEx));
    char p[MAX_PATH]; mkpath(p, "rxe"); make_file(p, 4096);
    HANDLE h = open_ov(p, GENERIC_READ, 0); rec_reset();
    OVERLAPPED ov; ovinit(&ov, 0, (HANDLE)(ULONG_PTR)0xC0FFEE00); BYTE b[100];
    CHECK(U(ReadFileEx)(h, b, 100, &ov, ex_cb));
    SleepEx(5000, TRUE);
    EXPECT_EQ(g_rec.hits, 1); CHECK(g_rec.ev_seen[0] == (HANDLE)(ULONG_PTR)0xC0FFEE00); CHECK(ov.hEvent == (HANDLE)(ULONG_PTR)0xC0FFEE00);
    CloseHandle(h);
}

TEST(aio_ex_wait_functions_return_io_completion) {
    REQUIRE(U(ReadFileEx));
    char p[MAX_PATH]; mkpath(p, "rxw"); make_file(p, 4096);
    HANDLE h = open_ov(p, GENERIC_READ, 0); HANDLE ev = mkevent(); BYTE b[100];
    /* WaitForSingleObjectEx */
    rec_reset(); OVERLAPPED ov; ovinit(&ov, 0, NULL);
    CHECK(U(ReadFileEx)(h, b, 100, &ov, ex_cb));
    EQ_CALL(WaitForSingleObjectEx(ev, 5000, TRUE), WAIT_IO_COMPLETION); EXPECT_EQ(g_rec.hits, 1);
    /* WaitForMultipleObjectsEx */
    ovinit(&ov, 0, NULL); CHECK(U(ReadFileEx)(h, b, 100, &ov, ex_cb));
    EQ_CALL(WaitForMultipleObjectsEx(1, &ev, FALSE, 5000, TRUE), WAIT_IO_COMPLETION); EXPECT_EQ(g_rec.hits, 2);
    /* SleepEx(0, TRUE) delivers pending APC */
    ovinit(&ov, 0, NULL); CHECK(U(ReadFileEx)(h, b, 100, &ov, ex_cb)); wait_done(&ov);
    EQ_CALL(SleepEx(0, TRUE), WAIT_IO_COMPLETION); EXPECT_EQ(g_rec.hits, 3);
    /* non-alertable never delivers */
    ovinit(&ov, 0, NULL); CHECK(U(ReadFileEx)(h, b, 100, &ov, ex_cb));
    EQ_CALL(SleepEx(30, FALSE), 0); EXPECT_EQ(WaitForSingleObject(ev, 30), WAIT_TIMEOUT); EXPECT_EQ(g_rec.hits, 3);
    wait_done(&ov);
    EQ_CALL(SleepEx(0, TRUE), WAIT_IO_COMPLETION); EXPECT_EQ(g_rec.hits, 4);
    /* nothing pending: alertable wait times out normally */
    EQ_CALL(SleepEx(20, TRUE), 0);
    CloseHandle(ev); CloseHandle(h);
}

TEST(aio_ex_multiple_completions_one_wait) {
    REQUIRE(U(ReadFileEx));
    char p[MAX_PATH]; mkpath(p, "rxm"); make_file(p, 65536);
    HANDLE h = open_ov(p, GENERIC_READ, 0); rec_reset();
    OVERLAPPED ov[8]; BYTE b[8][1000];
    for (int i = 0; i < 8; i++) { ovinit(&ov[i], i * 1000, NULL); CHECK(U(ReadFileEx)(h, b[i], 1000, &ov[i], ex_cb)); }
    for (int t = 0; t < 20 && g_rec.hits < 8; t++) SleepEx(500, TRUE);
    EXPECT_EQ(g_rec.hits, 8);
    int bad = 0;
    for (int i = 0; i < 8; i++) { if (g_rec.err[i] || g_rec.bytes[i] != 1000 || !chkp(b[i], i * 1000, 1000)) bad++; }
    EXPECT_EQ(bad, 0);
    CloseHandle(h);
}

TEST(aio_ex_readfileex_eof_fails_immediately) {
    REQUIRE(U(ReadFileEx));
    char p[MAX_PATH]; mkpath(p, "rxeof"); make_file(p, 1000);
    HANDLE h = open_ov(p, GENERIC_READ, 0); rec_reset();
    OVERLAPPED ov; ovinit(&ov, 1000, NULL); BYTE b[16];
    SET_ERR(0); BOOL r = U(ReadFileEx)(h, b, 16, &ov, ex_cb);
    CHECK(!r); EXPECT_ERR(ERROR_HANDLE_EOF);
    EQ_CALL(SleepEx(50, TRUE), 0); EXPECT_EQ(g_rec.hits, 0);
    CloseHandle(h);
}

TEST(aio_ex_readfileex_partial_last) {
    REQUIRE(U(ReadFileEx));
    char p[MAX_PATH]; mkpath(p, "rxp"); make_file(p, 1000);
    HANDLE h = open_ov(p, GENERIC_READ, 0); rec_reset();
    OVERLAPPED ov; ovinit(&ov, 900, NULL); BYTE b[200];
    CHECK(U(ReadFileEx)(h, b, 200, &ov, ex_cb)); SleepEx(5000, TRUE);
    EXPECT_EQ(g_rec.hits, 1); EXPECT_EQ(g_rec.err[0], 0); EXPECT_EQ(g_rec.bytes[0], 100);
    CloseHandle(h);
}

TEST(aio_ex_writefileex_basic) {
    REQUIRE(U(WriteFileEx));
    char p[MAX_PATH]; mkpath(p, "wx"); HANDLE h = new_ov_file(p, 0); rec_reset();
    OVERLAPPED ov; ovinit(&ov, 256, NULL); BYTE b[3000]; fillp(b, 256, 3000);
    SET_ERR(0); CHECK(U(WriteFileEx)(h, b, 3000, &ov, ex_cb));
    EQ_CALL(SleepEx(5000, TRUE), WAIT_IO_COMPLETION);
    EXPECT_EQ(g_rec.hits, 1); EXPECT_EQ(g_rec.err[0], 0); EXPECT_EQ(g_rec.bytes[0], 3000); CHECK(g_rec.ov[0] == &ov);
    CloseHandle(h);
    h = open_sync(p, GENERIC_READ); EXPECT_EQ(GetFileSize(h, NULL), 3256);
    BYTE r[3256]; DWORD n; CHECK(ReadFile(h, r, 3256, &n, NULL)); CHECK(!memcmp(r + 256, b, 3000));
    CloseHandle(h);
}

TEST(aio_ex_invalid_handles_and_access) {
    REQUIRE(U(ReadFileEx)); REQUIRE(U(WriteFileEx));
    BYTE b[16] = {0}; OVERLAPPED ov;
    HANDLE bad[2] = {INVALID_HANDLE_VALUE, NULL};
    for (int i = 0; i < 2; i++) {
        ovinit(&ov, 0, NULL); SET_ERR(0); CHECK(!U(ReadFileEx)(bad[i], b, 16, &ov, ex_cb)); EXPECT_ERR(ERROR_INVALID_HANDLE);
        ovinit(&ov, 0, NULL); SET_ERR(0); CHECK(!U(WriteFileEx)(bad[i], b, 16, &ov, ex_cb)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
    char p[MAX_PATH]; mkpath(p, "rxacc"); make_file(p, 100);
    HANDLE hw = open_ov(p, GENERIC_WRITE, 0), hr = open_ov(p, GENERIC_READ, 0);
    ovinit(&ov, 0, NULL); SET_ERR(0); CHECK(!U(ReadFileEx)(hw, b, 16, &ov, ex_cb)); EXPECT_ERR(ERROR_ACCESS_DENIED);
    ovinit(&ov, 0, NULL); SET_ERR(0); CHECK(!U(WriteFileEx)(hr, b, 16, &ov, ex_cb)); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(hw); CloseHandle(hr);
    ovinit(&ov, 0, NULL); SET_ERR(0); CHECK(!U(ReadFileEx)(hr, b, 16, &ov, ex_cb)); EXPECT_ERR(ERROR_INVALID_HANDLE);
}

TEST(aio_ex_sync_handle_note) {
    REQUIRE(U(ReadFileEx));
    char p[MAX_PATH]; mkpath(p, "rxsync"); make_file(p, 4096);
    HANDLE h = open_sync(p, GENERIC_READ); rec_reset();
    OVERLAPPED ov; ovinit(&ov, 0, NULL); BYTE b[16];
    SET_ERR(0); BOOL r = U(ReadFileEx)(h, b, 16, &ov, ex_cb);
    t_note("ReadFileEx on non-overlapped handle: %d err %lu", r, r ? 0 : GetLastError());
    if (r) SleepEx(100, TRUE);
    CloseHandle(h);
}

TEST(aio_ex_cancelled_pipe_read_gets_aborted_apc) {
    REQUIRE(U(ReadFileEx)); REQUIRE(U(CancelIoEx));
    HANDLE s, c; CHECK(make_pipe(&s, &c)); rec_reset();
    OVERLAPPED ov; ovinit(&ov, 0, NULL); BYTE b[16];
    SET_ERR(0); CHECK(U(ReadFileEx)(s, b, 16, &ov, ex_cb));    /* pending is success */
    EQ_CALL(SleepEx(50, TRUE), 0); EXPECT_EQ(g_rec.hits, 0);
    CHECK(U(CancelIoEx)(s, &ov));
    EQ_CALL(SleepEx(5000, TRUE), WAIT_IO_COMPLETION);
    EXPECT_EQ(g_rec.hits, 1); EXPECT_EQ(g_rec.err[0], ERROR_OPERATION_ABORTED); EXPECT_EQ(g_rec.bytes[0], 0);
    closeh(&s); closeh(&c);
}

TEST(aio_ex_pipe_read_write_roundtrip) {
    REQUIRE(U(ReadFileEx)); REQUIRE(U(WriteFileEx));
    HANDLE s, c; CHECK(make_pipe(&s, &c)); rec_reset();
    OVERLAPPED rov, wov; ovinit(&rov, 0, NULL); ovinit(&wov, 0, NULL); BYTE b[64];
    CHECK(U(ReadFileEx)(s, b, 64, &rov, ex_cb));
    CHECK(U(WriteFileEx)(c, "ping!", 5, &wov, ex_cb));
    for (int t = 0; t < 20 && g_rec.hits < 2; t++) SleepEx(250, TRUE);
    EXPECT_EQ(g_rec.hits, 2);
    int sawr = 0, sawwr = 0;
    for (int i = 0; i < 2; i++) { if (g_rec.ov[i] == &rov) { sawr = 1; EXPECT_EQ(g_rec.bytes[i], 5); } if (g_rec.ov[i] == &wov) { sawwr = 1; EXPECT_EQ(g_rec.bytes[i], 5); } EXPECT_EQ(g_rec.err[i], 0); }
    CHECK(sawr && sawwr); CHECK(!memcmp(b, "ping!", 5));
    closeh(&s); closeh(&c);
}

TEST(aio_ex_pipe_broken_apc_error) {
    REQUIRE(U(ReadFileEx));
    HANDLE s, c; CHECK(make_pipe(&s, &c)); rec_reset();
    OVERLAPPED ov; ovinit(&ov, 0, NULL); BYTE b[16];
    CHECK(U(ReadFileEx)(s, b, 16, &ov, ex_cb));
    closeh(&c);
    EQ_CALL(SleepEx(5000, TRUE), WAIT_IO_COMPLETION);
    EXPECT_EQ(g_rec.hits, 1); EXPECT_EQ(g_rec.err[0], ERROR_BROKEN_PIPE); EXPECT_EQ(g_rec.bytes[0], 0);
    closeh(&s);
}

static volatile LONG g_other_hits; static volatile LONG g_other_ready;
static DWORD WINAPI other_alertable(void *p) { g_other_ready = 1; DWORD r = SleepEx(400, TRUE); return r; }
TEST(aio_ex_apc_delivered_only_to_issuing_thread) {
    REQUIRE(U(ReadFileEx));
    char p[MAX_PATH]; mkpath(p, "rxthr"); make_file(p, 4096);
    HANDLE h = open_ov(p, GENERIC_READ, 0); rec_reset(); g_other_ready = 0;
    HANDLE t = CreateThread(NULL, 0, other_alertable, NULL, 0, NULL);
    while (!g_other_ready) Sleep(1);
    OVERLAPPED ov; ovinit(&ov, 0, NULL); BYTE b[16];
    CHECK(U(ReadFileEx)(h, b, 16, &ov, ex_cb));
    WaitForSingleObject(t, 5000); DWORD rc = 99; GetExitCodeThread(t, &rc);
    EXPECT_EQ(rc, 0); EXPECT_EQ(g_rec.hits, 0);
    wait_done(&ov);
    EQ_CALL(SleepEx(0, TRUE), WAIT_IO_COMPLETION); EXPECT_EQ(g_rec.hits, 1);
    CloseHandle(t); CloseHandle(h);
}

TEST(aio_ex_overlapped_offsets_table) {
    REQUIRE(U(ReadFileEx));
    char p[MAX_PATH]; mkpath(p, "rxoff"); make_file(p, 20000);
    HANDLE h = open_ov(p, GENERIC_READ, 0);
    static const DWORD offs[] = {0, 1, 511, 4096, 9999, 19900};
    for (unsigned i = 0; i < sizeof offs / sizeof offs[0]; i++) {
        rec_reset(); OVERLAPPED ov; ovinit(&ov, offs[i], NULL); BYTE b[100];
        CHECK(U(ReadFileEx)(h, b, 100, &ov, ex_cb)); SleepEx(5000, TRUE);
        CHECKF(g_rec.hits == 1 && g_rec.bytes[0] == 100 && chkp(b, offs[i], 100), "offset %lu", offs[i]);
    }
    CloseHandle(h);
}

/* ---------- ReadFileScatter / WriteFileGather ---------- */
static iores do_scatter(HANDLE h, FILE_SEGMENT_ELEMENT *seg, DWORD bytes, OVERLAPPED *ov) {
    SET_ERR(0); BOOL r = U(ReadFileScatter)(h, seg, bytes, NULL, ov); DWORD e = r ? 0 : GetLastError(); return io_fin(h, ov, r, e);
}
static iores do_gather(HANDLE h, FILE_SEGMENT_ELEMENT *seg, DWORD bytes, OVERLAPPED *ov) {
    SET_ERR(0); BOOL r = U(WriteFileGather)(h, seg, bytes, NULL, ov); DWORD e = r ? 0 : GetLastError(); return io_fin(h, ov, r, e);
}
#define NSEG 4
static void mkseg(FILE_SEGMENT_ELEMENT *seg, BYTE **pages, int n) {
    memset(seg, 0, sizeof(*seg) * (n + 1));
    for (int i = 0; i < n; i++) seg[i].Alignment = (ULONGLONG)(ULONG_PTR)pages[i];
}

TEST(aio_scatter_gather_roundtrip) {
    REQUIRE(U(ReadFileScatter)); REQUIRE(U(WriteFileGather));
    char p[MAX_PATH]; mkpath(p, "sg");
    HANDLE h = new_ov_file(p, FILE_FLAG_NO_BUFFERING); if (h == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    BYTE *w = VirtualAlloc(NULL, NSEG * 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    BYTE *wp[NSEG]; FILE_SEGMENT_ELEMENT seg[NSEG + 1];
    /* non-contiguous order to prove segments are used individually */
    BYTE *base = VirtualAlloc(NULL, 8 * 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    int order[NSEG] = {5, 1, 6, 3};
    for (int i = 0; i < NSEG; i++) { wp[i] = base + order[i] * 4096; fillp(wp[i], (size_t)i * 4096, 4096); }
    mkseg(seg, wp, NSEG);
    HANDLE ev = mkevent(); OVERLAPPED ov; ovinit(&ov, 0, ev);
    iores x = do_gather(h, seg, NSEG * 4096, &ov);
    if (!x.ok && t_is_wine()) { t_skip("scatter/gather unsupported under emulator"); return; }
    CHECK(x.ok); EXPECT_EQ(x.n, NSEG * 4096);
    EXPECT_EQ((DWORD)ov.Internal, 0); EXPECT_EQ(ov.InternalHigh, NSEG * 4096);
    CloseHandle(h);
    h = open_ov(p, GENERIC_READ, FILE_FLAG_NO_BUFFERING);
    BYTE *rb = VirtualAlloc(NULL, 8 * 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); BYTE *rp[NSEG];
    int rorder[NSEG] = {2, 7, 0, 4};
    for (int i = 0; i < NSEG; i++) rp[i] = rb + rorder[i] * 4096;
    mkseg(seg, rp, NSEG); ovinit(&ov, 0, ev);
    x = do_scatter(h, seg, NSEG * 4096, &ov); CHECK(x.ok); EXPECT_EQ(x.n, NSEG * 4096);
    for (int i = 0; i < NSEG; i++) CHECKF(chkp(rp[i], (size_t)i * 4096, 4096), "segment %d", i);
    /* partial: read 2 pages from offset 8192 */
    mkseg(seg, rp, 2); memset(rb, 0, 8 * 4096); ovinit(&ov, 8192, ev);
    x = do_scatter(h, seg, 2 * 4096, &ov); CHECK(x.ok); EXPECT_EQ(x.n, 8192);
    CHECK(chkp(rp[0], 8192, 4096)); CHECK(chkp(rp[1], 12288, 4096));
    VirtualFree(w, 0, MEM_RELEASE); VirtualFree(base, 0, MEM_RELEASE); VirtualFree(rb, 0, MEM_RELEASE);
    CloseHandle(ev); CloseHandle(h);
}

TEST(aio_scatter_invalid_params) {
    REQUIRE(U(ReadFileScatter)); REQUIRE(U(WriteFileGather));
    if (t_is_wine()) { t_skip("scatter/gather parameter validation faults under emulator"); return; }
    char p[MAX_PATH]; mkpath(p, "sginv"); make_file(p, 16384);
    HANDLE h = open_ov(p, GENERIC_READ | GENERIC_WRITE, FILE_FLAG_NO_BUFFERING); if (h == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    BYTE *pg = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); FILE_SEGMENT_ELEMENT seg[2];
    mkseg(seg, &pg, 1); OVERLAPPED ov; HANDLE ev = mkevent(); ovinit(&ov, 0, ev); DWORD rsv = 0;
    SET_ERR(0); CHECK(!U(ReadFileScatter)(h, seg, 4096, NULL, NULL)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!U(WriteFileGather)(h, seg, 4096, NULL, NULL)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!U(ReadFileScatter)(h, seg, 4096, &rsv, &ov)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!U(WriteFileGather)(h, seg, 4096, &rsv, &ov)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    /* not page-multiple length / unaligned offset */
    ovinit(&ov, 0, ev); iores x = do_scatter(h, seg, 100, &ov); t_note("scatter len 100: err %lu", x.err);
    ovinit(&ov, 100, ev); x = do_scatter(h, seg, 4096, &ov); t_note("scatter unaligned offset: err %lu", x.err);
    CloseHandle(h);
    /* handle lacking NO_BUFFERING / lacking OVERLAPPED */
    h = open_ov(p, GENERIC_READ, 0); ovinit(&ov, 0, ev); x = do_scatter(h, seg, 4096, &ov);
    t_note("scatter on buffered overlapped handle: err %lu", x.err); CloseHandle(h);
    h = open_sync(p, GENERIC_READ); ovinit(&ov, 0, ev); x = do_scatter(h, seg, 4096, &ov);
    t_note("scatter on synchronous handle: err %lu", x.err); CloseHandle(h);
    h = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, NULL);
    ovinit(&ov, 0, ev); x = do_scatter(h, seg, 4096, &ov);
    t_note("scatter on unbuffered synchronous handle: err %lu", x.err); CloseHandle(h);
    /* bad handles */
    ovinit(&ov, 0, ev); SET_ERR(0); CHECK(!U(ReadFileScatter)(INVALID_HANDLE_VALUE, seg, 4096, NULL, &ov)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    ovinit(&ov, 0, ev); SET_ERR(0); CHECK(!U(WriteFileGather)(NULL, seg, 4096, NULL, &ov)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    VirtualFree(pg, 0, MEM_RELEASE); CloseHandle(ev);
}

TEST(aio_scatter_eof_and_access) {
    REQUIRE(U(ReadFileScatter));
    char p[MAX_PATH]; mkpath(p, "sgeof"); make_file(p, 8192);
    HANDLE h = open_ov(p, GENERIC_READ, FILE_FLAG_NO_BUFFERING); if (h == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    BYTE *pg = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); FILE_SEGMENT_ELEMENT seg[2]; mkseg(seg, &pg, 1);
    OVERLAPPED ov; HANDLE ev = mkevent(); ovinit(&ov, 8192, ev);
    iores x = do_scatter(h, seg, 4096, &ov);
    if (x.ok && t_is_wine()) { t_skip("scatter unsupported under emulator"); return; }
    CHECK(!x.ok); EXPECT_EQ(x.err, ERROR_HANDLE_EOF);
    CloseHandle(h);
    HANDLE hw = open_ov(p, GENERIC_WRITE, FILE_FLAG_NO_BUFFERING);
    ovinit(&ov, 0, ev); x = do_scatter(hw, seg, 4096, &ov); CHECK(!x.ok); EXPECT_EQ(x.err, ERROR_ACCESS_DENIED);
    CloseHandle(hw);
    VirtualFree(pg, 0, MEM_RELEASE); CloseHandle(ev);
}

TEST(aio_scatter_iocp_packet) {
    REQUIRE(U(ReadFileScatter));
    char p[MAX_PATH]; mkpath(p, "sgiocp"); make_file(p, 8192);
    HANDLE h = open_ov(p, GENERIC_READ, FILE_FLAG_NO_BUFFERING); if (h == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    HANDLE port = U(CreateIoCompletionPort)(INVALID_HANDLE_VALUE, NULL, 0, 0); U(CreateIoCompletionPort)(h, port, 99, 0);
    BYTE *pg[2]; BYTE *m = VirtualAlloc(NULL, 8192, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); pg[0] = m; pg[1] = m + 4096;
    FILE_SEGMENT_ELEMENT seg[3]; mkseg(seg, pg, 2); OVERLAPPED ov; ovinit(&ov, 0, NULL);
    SET_ERR(0); BOOL r = U(ReadFileScatter)(h, seg, 8192, NULL, &ov);
    if (!r && GetLastError() != ERROR_IO_PENDING) { if (t_is_wine()) t_skip("unsupported"); else CHECK(0); return; }
    DWORD n; ULONG_PTR k; OVERLAPPED *o;
    CHECK(U(GetQueuedCompletionStatus)(port, &n, &k, &o, 5000)); EXPECT_EQ(n, 8192); EXPECT_EQ(k, 99); CHECK(o == &ov); CHECK(chkp(m, 0, 8192));
    VirtualFree(m, 0, MEM_RELEASE); CloseHandle(h); CloseHandle(port);
}

/* ---------- DeviceIoControl overlapped ---------- */
static iores do_ioctl(HANDLE h, DWORD code, void *in, DWORD inl, void *out, DWORD outl, OVERLAPPED *ov, DWORD *bytes_ret) {
    SET_ERR(0); DWORD n = 0xFFFF; BOOL r = U(DeviceIoControl)(h, code, in, inl, out, outl, &n, ov); DWORD e = r ? 0 : GetLastError();
    iores x = io_fin(h, ov, r, e); if (bytes_ret) *bytes_ret = n; return x;
}

TEST(aio_ioctl_get_compression_overlapped) {
    REQUIRE(U(DeviceIoControl));
    char p[MAX_PATH]; mkpath(p, "gc"); make_file(p, 4096);
    HANDLE h = open_ov(p, GENERIC_READ, 0); HANDLE ev = mkevent(); OVERLAPPED ov; ovinit(&ov, 0, ev);
    USHORT fmt = 0xFFFF; DWORD br = 0;
    iores x = do_ioctl(h, FSCTL_GET_COMPRESSION, NULL, 0, &fmt, sizeof fmt, &ov, &br);
    if (!x.ok && !t_is_wine() && (x.err == ERROR_INVALID_FUNCTION || x.err == ERROR_NOT_SUPPORTED)) { t_skip("file system lacks compression support"); return; }
    CHECK(x.ok); EXPECT_EQ(x.n, 2); CHECKF(fmt <= 4, "fmt %u", fmt); EXPECT_EQ(ov.InternalHigh, 2);
    if (fmt != 0) t_note("temp directory is compressed (format %u)", fmt);
    /* NULL lpBytesReturned with an OVERLAPPED */
    ovinit(&ov, 0, ev); fmt = 0xFFFF; SET_ERR(0);
    BOOL r = U(DeviceIoControl)(h, FSCTL_GET_COMPRESSION, NULL, 0, &fmt, sizeof fmt, NULL, &ov);
    CHECK(r || GetLastError() == ERROR_IO_PENDING);
    DWORD n; CHECK(GetOverlappedResultEx(h, &ov, &n, 5000, FALSE)); CHECK(fmt <= 4);
    CloseHandle(ev); CloseHandle(h);
}

TEST(aio_ioctl_get_compression_sync_handle_no_overlapped) {
    REQUIRE(U(DeviceIoControl));
    char p[MAX_PATH]; mkpath(p, "gc2"); make_file(p, 4096);
    HANDLE h = open_sync(p, GENERIC_READ); USHORT fmt = 0xFFFF; DWORD br = 0;
    SET_ERR(0); BOOL okc = U(DeviceIoControl)(h, FSCTL_GET_COMPRESSION, NULL, 0, &fmt, 2, &br, NULL);
    if (!okc && !t_is_wine() && (GetLastError() == ERROR_INVALID_FUNCTION || GetLastError() == ERROR_NOT_SUPPORTED)) { CloseHandle(h); t_skip("file system lacks compression support"); return; }
    CHECK(okc); EXPECT_EQ(br, 2); CHECK(fmt <= 4);
    br = 0xFFFF; fmt = 0xFFFF; SET_ERR(0);
    CHECK(!U(DeviceIoControl)(h, FSCTL_GET_COMPRESSION, NULL, 0, &fmt, 0, &br, NULL));
    DWORD e = GetLastError(); CHECKF(e == ERROR_INSUFFICIENT_BUFFER || e == ERROR_INVALID_PARAMETER || e == ERROR_MORE_DATA, "err %lu", e);
    CloseHandle(h);
}

TEST(aio_ioctl_invalid_handle) {
    REQUIRE(U(DeviceIoControl));
    USHORT fmt; DWORD br; OVERLAPPED ov; ovinit(&ov, 0, NULL);
    HANDLE bad[2] = {INVALID_HANDLE_VALUE, NULL};
    for (int i = 0; i < 2; i++) {
        SET_ERR(0); CHECK(!U(DeviceIoControl)(bad[i], FSCTL_GET_COMPRESSION, NULL, 0, &fmt, 2, &br, NULL)); EXPECT_ERR(ERROR_INVALID_HANDLE);
        ovinit(&ov, 0, NULL); SET_ERR(0); CHECK(!U(DeviceIoControl)(bad[i], FSCTL_GET_COMPRESSION, NULL, 0, &fmt, 2, &br, &ov)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
}

TEST(aio_ioctl_set_sparse_and_query_ranges_overlapped) {
    REQUIRE(U(DeviceIoControl));
    char p[MAX_PATH]; mkpath(p, "sp"); make_file(p, 0);
    HANDLE h = new_ov_file(p, 0); HANDLE ev = mkevent(); OVERLAPPED ov; ovinit(&ov, 0, ev); DWORD br = 0;
    iores x = do_ioctl(h, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &ov, &br);
    if (!x.ok) { t_note("FSCTL_SET_SPARSE unsupported here: err %lu", x.err); CloseHandle(h); CloseHandle(ev); return; }
    /* write a block at 1 MB */
    BYTE blk[4096]; memset(blk, 0x42, sizeof blk); ovinit(&ov, 1u << 20, ev); x = do_write(h, blk, sizeof blk, &ov); CHECK(x.ok);
    FILE_ALLOCATED_RANGE_BUFFER in; in.FileOffset.QuadPart = 0; in.Length.QuadPart = (2u << 20);
    FILE_ALLOCATED_RANGE_BUFFER out[8]; ovinit(&ov, 0, ev);
    x = do_ioctl(h, FSCTL_QUERY_ALLOCATED_RANGES, &in, sizeof in, out, sizeof out, &ov, &br);
    if (!x.ok && t_is_wine()) { t_skip("allocated ranges unsupported under emulator"); return; }
    CHECK(x.ok); EXPECT_EQ(x.n % sizeof(FILE_ALLOCATED_RANGE_BUFFER), 0); CHECK(x.n >= sizeof out[0]);
    if (x.n >= sizeof out[0]) { CHECK(out[0].FileOffset.QuadPart <= (1 << 20)); CHECK(out[0].FileOffset.QuadPart + out[0].Length.QuadPart >= (1 << 20) + 4096); }
    CloseHandle(ev); CloseHandle(h);
}

TEST(aio_ioctl_invalid_code_on_file) {
    REQUIRE(U(DeviceIoControl));
    char p[MAX_PATH]; mkpath(p, "badctl"); make_file(p, 100);
    HANDLE h = open_ov(p, GENERIC_READ, 0); HANDLE ev = mkevent(); OVERLAPPED ov; ovinit(&ov, 0, ev); DWORD br;
    iores x = do_ioctl(h, 0x12345678, NULL, 0, NULL, 0, &ov, &br);
    CHECK(!x.ok); t_note("bogus IOCTL on file: err %lu", x.err);
    ovinit(&ov, 0, ev); x = do_ioctl(h, CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 0xFFF, METHOD_BUFFERED, FILE_ANY_ACCESS), NULL, 0, NULL, 0, &ov, &br);
    CHECK(!x.ok); t_note("bogus FSCTL on file: err %lu", x.err);
    CloseHandle(ev); CloseHandle(h);
}

TEST(aio_ioctl_pipe_fsctl_listen_note) {
    REQUIRE(U(DeviceIoControl)); REQUIRE(U(CancelIoEx));
    char name[128]; snprintf(name, sizeof name, "\\\\.\\pipe\\k32t_aio_li_%lu", GetCurrentProcessId());
    HANDLE s = CreateNamedPipeA(name, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, NULL);
    CHECK(s != INVALID_HANDLE_VALUE); if (s == INVALID_HANDLE_VALUE) return;
    HANDLE ev = mkevent(); OVERLAPPED ov; ovinit(&ov, 0, ev); DWORD br = 0;
    SET_ERR(0); BOOL r = U(DeviceIoControl)(s, 0x110008 /*FSCTL_PIPE_LISTEN*/, NULL, 0, NULL, 0, &br, &ov);
    DWORD e = GetLastError();
    t_note("DeviceIoControl(FSCTL_PIPE_LISTEN) overlapped: ret %d err %lu", r, e);
    if (!r && e == ERROR_IO_PENDING) {
        CHECK(U(CancelIoEx)(s, &ov));
        DWORD n; SET_ERR(0); CHECK(!GetOverlappedResultEx(s, &ov, &n, 5000, FALSE)); EXPECT_ERR(ERROR_OPERATION_ABORTED);
    }
    CloseHandle(ev); CloseHandle(s);
}

TEST(aio_ioctl_pipe_wait_missing_pipe) {
    REQUIRE(U(WaitNamedPipeA));
    SET_ERR(0); CHECK(!U(WaitNamedPipeA)("\\\\.\\pipe\\k32t_aio_does_not_exist_zz", 10));
    DWORD e = GetLastError(); CHECKF(e == ERROR_FILE_NOT_FOUND || e == ERROR_SEM_TIMEOUT, "err %lu", e);
}

/* ---------- misc ---------- */
TEST(aio_misc_sequential_ops_1000_iterations_handle_reuse) {
    REQUIRE(U(WriteFile)); REQUIRE(U(ReadFile));
    char p[MAX_PATH]; mkpath(p, "loop"); HANDLE h = new_ov_file(p, 0); HANDLE ev = mkevent();
    int bad = 0;
    for (int i = 0; i < 1000; i++) {
        BYTE w[64], r[64]; OVERLAPPED ov; fillp(w, i * 3, 64);
        ovinit(&ov, (ULONGLONG)(i % 50) * 64, ev); ResetEvent(ev);
        iores x = do_write(h, w, 64, &ov); if (!x.ok || x.n != 64) bad++;
        ovinit(&ov, (ULONGLONG)(i % 50) * 64, ev); ResetEvent(ev);
        x = do_read(h, r, 64, &ov); if (!x.ok || x.n != 64 || memcmp(w, r, 64)) bad++;
    }
    EXPECT_EQ(bad, 0); CloseHandle(ev); CloseHandle(h);
}

TEST(aio_misc_closed_file_handle_ov_ops_fail) {
    REQUIRE(U(ReadFile));
    char p[MAX_PATH]; mkpath(p, "closedf"); make_file(p, 100);
    HANDLE h = open_ov(p, GENERIC_READ, 0); CloseHandle(h);
    OVERLAPPED ov; ovinit(&ov, 0, NULL); BYTE b[4];
    SET_ERR(0); CHECK(!U(ReadFile)(h, b, 4, NULL, &ov)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    DWORD n; SET_ERR(0); CHECK(!U(GetOverlappedResultEx)(h, &ov, &n, 0, FALSE));
}

TEST(aio_misc_getoverlappedresult_internal_error_mapping) {
    REQUIRE(U(GetOverlappedResult)); REQUIRE(U(GetOverlappedResultEx));
    struct { ULONG_PTR st; DWORD err; } t[] = {
        {0xC0000011, ERROR_HANDLE_EOF}, {0xC0000120, ERROR_OPERATION_ABORTED}, {0xC000014B, ERROR_BROKEN_PIPE},
        {0xC0000022, ERROR_ACCESS_DENIED}, {0xC0000008, ERROR_INVALID_HANDLE}, {0xC000000D, ERROR_INVALID_PARAMETER},
        {0xC0000056, ERROR_ACCESS_DENIED}, {0xC000009A, ERROR_NO_SYSTEM_RESOURCES}, {0x80000005, ERROR_MORE_DATA}};
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        OVERLAPPED ov; memset(&ov, 0, sizeof ov); ov.Internal = t[i].st; ov.InternalHigh = 12; DWORD n = 99;
        SET_ERR(0); BOOL r = U(GetOverlappedResult)((HANDLE)(ULONG_PTR)0x1234, &ov, &n, FALSE);
        CHECKF(!r, "status %llx", (unsigned long long)t[i].st);
        DWORD e = GetLastError(); CHECKF(e == t[i].err, "status %llx err %lu want %lu", (unsigned long long)t[i].st, e, t[i].err);
        EXPECT_EQ(n, 12);
        n = 99; SET_ERR(0); r = U(GetOverlappedResultEx)((HANDLE)(ULONG_PTR)0x1234, &ov, &n, 0, FALSE);
        CHECK(!r); EXPECT_ERR(t[i].err); EXPECT_EQ(n, 12);
    }
    /* success statuses (including informational/warning < 0x80000000 are "success") */
    OVERLAPPED ov; memset(&ov, 0, sizeof ov); ov.Internal = 0; ov.InternalHigh = 33; DWORD n = 0;
    CHECK(U(GetOverlappedResult)((HANDLE)(ULONG_PTR)0x1234, &ov, &n, FALSE)); EXPECT_EQ(n, 33);
    ov.Internal = 0x00000103; SET_ERR(0); CHECK(!U(GetOverlappedResult)((HANDLE)(ULONG_PTR)0x1234, &ov, &n, FALSE)); EXPECT_ERR(ERROR_IO_INCOMPLETE);
}
