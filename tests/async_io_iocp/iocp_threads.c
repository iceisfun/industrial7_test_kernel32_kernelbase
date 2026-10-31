#define AIO_TU "it"
#include "aio.h"

#define SENTINEL ((ULONG_PTR)0xFFFFFFF0u)

typedef struct { HANDLE port; volatile LONG *counts; volatile LONG total; volatile LONG timeouts; volatile LONG wrongret; } drain_ctx;

static DWORD WINAPI drain_thread(void *p) {
    drain_ctx *c = p;
    for (;;) {
        DWORD n = 0; ULONG_PTR key = 0; OVERLAPPED *o = NULL;
        BOOL r = U(GetQueuedCompletionStatus)(c->port, &n, &key, &o, 10000);
        if (!r) { if (o == NULL) InterlockedIncrement(&c->timeouts); else InterlockedIncrement(&c->wrongret); break; }
        if (key == SENTINEL) break;
        if (n != (DWORD)key * 2 + 1) InterlockedIncrement(&c->wrongret);
        InterlockedIncrement(&c->counts[key]); InterlockedIncrement(&c->total);
    }
    return 0;
}

static void run_drain(DWORD concurrency, int nthreads, int npackets) {
    HANDLE port = U(CreateIoCompletionPort)(INVALID_HANDLE_VALUE, NULL, 0, concurrency);
    CHECK(port != NULL); if (!port) return;
    volatile LONG *counts = calloc(npackets, sizeof(LONG));
    drain_ctx c = {port, counts, 0, 0, 0};
    HANDLE th[16]; for (int i = 0; i < nthreads; i++) th[i] = CreateThread(NULL, 0, drain_thread, &c, 0, NULL);
    for (int i = 0; i < npackets; i++) U(PostQueuedCompletionStatus)(port, i * 2 + 1, i, NULL);
    for (int i = 0; i < nthreads; i++) U(PostQueuedCompletionStatus)(port, 0, SENTINEL, NULL);
    CHECK(WaitForMultipleObjects(nthreads, th, TRUE, 60000) == WAIT_OBJECT_0);
    EXPECT_EQ(c.total, npackets); EXPECT_EQ(c.timeouts, 0); EXPECT_EQ(c.wrongret, 0);
    int bad = 0; for (int i = 0; i < npackets; i++) if (counts[i] != 1) bad++;
    EXPECT_EQ(bad, 0);
    for (int i = 0; i < nthreads; i++) CloseHandle(th[i]);
    free((void *)counts); CloseHandle(port);
}

TEST(aio_iocpmt_drain_10000_8_threads) {
    REQUIRE(U(GetQueuedCompletionStatus)); REQUIRE(U(PostQueuedCompletionStatus)); REQUIRE(U(CreateIoCompletionPort));
    run_drain(0, 8, 10000);
}
TEST(aio_iocpmt_drain_10000_concurrency_1) {
    REQUIRE(U(GetQueuedCompletionStatus)); REQUIRE(U(PostQueuedCompletionStatus)); REQUIRE(U(CreateIoCompletionPort));
    run_drain(1, 8, 10000);
}
TEST(aio_iocpmt_drain_5000_concurrency_4) {
    REQUIRE(U(GetQueuedCompletionStatus)); REQUIRE(U(PostQueuedCompletionStatus)); REQUIRE(U(CreateIoCompletionPort));
    run_drain(4, 8, 5000);
}
TEST(aio_iocpmt_drain_single_thread_1000) {
    REQUIRE(U(GetQueuedCompletionStatus)); REQUIRE(U(PostQueuedCompletionStatus)); REQUIRE(U(CreateIoCompletionPort));
    run_drain(0, 1, 1000);
}

/* Multiple producers */
typedef struct { HANDLE port; int base, count; } prod_ctx;
static DWORD WINAPI producer(void *p) {
    prod_ctx *c = p; for (int i = 0; i < c->count; i++) U(PostQueuedCompletionStatus)(c->port, c->base + i, c->base + i, NULL); return 0;
}
TEST(aio_iocpmt_multiple_producers_exact_count) {
    REQUIRE(U(PostQueuedCompletionStatus)); REQUIRE(U(GetQueuedCompletionStatus));
    HANDLE port = U(CreateIoCompletionPort)(INVALID_HANDLE_VALUE, NULL, 0, 0);
    enum { P = 4, PER = 2500 }; prod_ctx pc[P]; HANDLE th[P];
    for (int i = 0; i < P; i++) { pc[i].port = port; pc[i].base = i * PER; pc[i].count = PER; th[i] = CreateThread(NULL, 0, producer, &pc[i], 0, NULL); }
    CHECK(WaitForMultipleObjects(P, th, TRUE, 30000) == WAIT_OBJECT_0);
    char *seen = calloc(P * PER, 1); int bad = 0, got = 0;
    for (int i = 0; i < P * PER; i++) {
        DWORD n; ULONG_PTR k; OVERLAPPED *o;
        if (!U(GetQueuedCompletionStatus)(port, &n, &k, &o, 2000)) { bad++; break; }
        got++; if (k >= P * PER || seen[k]) bad++; else seen[k] = 1;
    }
    EXPECT_EQ(bad, 0); EXPECT_EQ(got, P * PER);
    DWORD n; ULONG_PTR k; OVERLAPPED *o; CHECK(!U(GetQueuedCompletionStatus)(port, &n, &k, &o, 0));
    for (int i = 0; i < P; i++) CloseHandle(th[i]);
    free(seen); CloseHandle(port);
}

static DWORD WINAPI waiter_thread(void *p) {
    void **a = p; DWORD n = 0; ULONG_PTR k = 0; OVERLAPPED *o = NULL;
    BOOL r = U(GetQueuedCompletionStatus)((HANDLE)a[0], &n, &k, &o, INFINITE);
    return r && k == 0xC0FFEE && n == 42 ? 1 : 0;
}
TEST(aio_iocpmt_blocked_waiter_wakes_on_post) {
    REQUIRE(U(PostQueuedCompletionStatus));
    HANDLE port = U(CreateIoCompletionPort)(INVALID_HANDLE_VALUE, NULL, 0, 0);
    void *a[1] = {port}; HANDLE t = CreateThread(NULL, 0, waiter_thread, a, 0, NULL);
    Sleep(150); CHECK(WaitForSingleObject(t, 0) == WAIT_TIMEOUT);
    CHECK(U(PostQueuedCompletionStatus)(port, 42, 0xC0FFEE, NULL));
    CHECK(WaitForSingleObject(t, 5000) == WAIT_OBJECT_0);
    DWORD rc = 0; GetExitCodeThread(t, &rc); EXPECT_EQ(rc, 1);
    CloseHandle(t); CloseHandle(port);
}

/* waiters released in LIFO order by the kernel; just check each gets exactly one packet */
TEST(aio_iocpmt_n_waiters_n_posts) {
    REQUIRE(U(PostQueuedCompletionStatus));
    HANDLE port = U(CreateIoCompletionPort)(INVALID_HANDLE_VALUE, NULL, 0, 0);
    enum { W = 6 }; void *a[1] = {port}; HANDLE t[W];
    for (int i = 0; i < W; i++) t[i] = CreateThread(NULL, 0, waiter_thread, a, 0, NULL);
    Sleep(150);
    for (int i = 0; i < W; i++) U(PostQueuedCompletionStatus)(port, 42, 0xC0FFEE, NULL);
    CHECK(WaitForMultipleObjects(W, t, TRUE, 10000) == WAIT_OBJECT_0);
    int ok = 0; for (int i = 0; i < W; i++) { DWORD rc = 0; GetExitCodeThread(t[i], &rc); ok += rc; CloseHandle(t[i]); }
    EXPECT_EQ(ok, W); CloseHandle(port);
}

/* real IO driven through workers */
typedef struct { OVERLAPPED ov; int idx; BYTE *buf; } rio;
typedef struct { HANDLE port; volatile LONG done[128]; volatile LONG bad; } rctx;
static DWORD WINAPI rio_worker(void *p) {
    rctx *c = p;
    for (;;) {
        DWORD n; ULONG_PTR k; OVERLAPPED *o;
        if (!U(GetQueuedCompletionStatus)(c->port, &n, &k, &o, 20000)) { InterlockedIncrement(&c->bad); return 0; }
        if (k == SENTINEL) return 0;
        rio *r = (rio *)o;
        if (k != 7 || n != 16384 || !chkp(r->buf, (size_t)r->idx * 16384, 16384)) InterlockedIncrement(&c->bad);
        InterlockedIncrement(&c->done[r->idx]);
    }
}
TEST(aio_iocpmt_real_reads_4_workers) {
    REQUIRE(U(ReadFile)); REQUIRE(U(CreateIoCompletionPort));
    enum { N = 64, SZ = 16384, W = 4 };
    char p[MAX_PATH]; mkpath(p, "rio"); make_file(p, (size_t)N * SZ);
    HANDLE f = open_ov(p, GENERIC_READ, 0);
    rctx c; memset(&c, 0, sizeof c); c.port = U(CreateIoCompletionPort)(INVALID_HANDLE_VALUE, NULL, 0, 0);
    CHECK(U(CreateIoCompletionPort)(f, c.port, 7, 0) == c.port);
    HANDLE th[W]; for (int i = 0; i < W; i++) th[i] = CreateThread(NULL, 0, rio_worker, &c, 0, NULL);
    rio *ios = calloc(N, sizeof(rio)); BYTE *buf = calloc(N, SZ);
    for (int i = 0; i < N; i++) {
        ios[i].idx = i; ios[i].buf = buf + (size_t)i * SZ; ovinit(&ios[i].ov, (ULONGLONG)i * SZ, NULL);
        BOOL r = U(ReadFile)(f, ios[i].buf, SZ, NULL, &ios[i].ov);
        CHECKF(r || GetLastError() == ERROR_IO_PENDING, "issue %d", i);
    }
    /* wait until all done, then stop workers */
    for (int t = 0; t < 200; t++) { int d = 0; for (int i = 0; i < N; i++) d += c.done[i]; if (d == N) break; Sleep(25); }
    for (int i = 0; i < W; i++) U(PostQueuedCompletionStatus)(c.port, 0, SENTINEL, NULL);
    CHECK(WaitForMultipleObjects(W, th, TRUE, 30000) == WAIT_OBJECT_0);
    int bad = 0; for (int i = 0; i < N; i++) if (c.done[i] != 1) bad++;
    EXPECT_EQ(bad, 0); EXPECT_EQ(c.bad, 0);
    for (int i = 0; i < W; i++) CloseHandle(th[i]);
    free(ios); free(buf); CloseHandle(f); CloseHandle(c.port);
}

TEST(aio_iocpmt_real_writes_then_verify) {
    REQUIRE(U(WriteFile)); REQUIRE(U(GetQueuedCompletionStatusEx));
    enum { N = 32, SZ = 32768 };
    char p[MAX_PATH]; mkpath(p, "rw"); HANDLE f = new_ov_file(p, 0);
    HANDLE port = U(CreateIoCompletionPort)(INVALID_HANDLE_VALUE, NULL, 0, 0);
    U(CreateIoCompletionPort)(f, port, 5, 0);
    OVERLAPPED ov[N]; BYTE *buf = malloc((size_t)N * SZ); fillp(buf, 0, (size_t)N * SZ);
    for (int i = 0; i < N; i++) { ovinit(&ov[i], (ULONGLONG)(N - 1 - i) * SZ, NULL); U(WriteFile)(f, buf + (size_t)(N - 1 - i) * SZ, SZ, NULL, &ov[i]); }
    int got = 0, bad = 0;
    OVERLAPPED_ENTRY e[16];
    while (got < N) {
        ULONG rem = 0;
        if (!U(GetQueuedCompletionStatusEx)(port, e, 16, &rem, 10000, FALSE)) { bad++; break; }
        for (ULONG i = 0; i < rem; i++) {
            if (e[i].lpCompletionKey != 5 || e[i].dwNumberOfBytesTransferred != SZ || e[i].lpOverlapped < &ov[0] || e[i].lpOverlapped >= &ov[N]) bad++;
            got++;
        }
    }
    EXPECT_EQ(got, N); EXPECT_EQ(bad, 0);
    CloseHandle(f); f = open_sync(p, GENERIC_READ);
    BYTE *r = malloc((size_t)N * SZ); DWORD n; CHECK(ReadFile(f, r, N * SZ, &n, NULL)); CHECK(n == N * SZ && !memcmp(r, buf, N * SZ));
    free(r); free(buf); CloseHandle(f); CloseHandle(port);
}

/* ---- port closed while a thread waits: child process, bounded ---- */
static HANDLE g_cp; static volatile LONG g_wstate; static DWORD g_werr; static BOOL g_wret; static OVERLAPPED *g_wov; static ULONG g_wrem;
static DWORD WINAPI cw_thread(void *p) {
    DWORD n; ULONG_PTR k; OVERLAPPED *o = (OVERLAPPED *)(ULONG_PTR)1; g_wstate = 1;
    g_wret = GetQueuedCompletionStatus(g_cp, &n, &k, &o, INFINITE);
    g_werr = GetLastError(); g_wov = o; g_wstate = 2; return 0;
}
static DWORD WINAPI cwex_thread(void *p) {
    OVERLAPPED_ENTRY e[2]; g_wstate = 1;
    g_wret = GetQueuedCompletionStatusEx(g_cp, e, 2, &g_wrem, INFINITE, FALSE);
    g_werr = GetLastError(); g_wstate = 2; return 0;
}
CHILD(aio_iocp_close_waiter) {
    int ex = argc > 1 && !strcmp(argv[1], "ex");
    g_cp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    HANDLE t = CreateThread(NULL, 0, ex ? cwex_thread : cw_thread, NULL, 0, NULL);
    Sleep(300);
    CloseHandle(g_cp);
    if (WaitForSingleObject(t, 3000) != WAIT_OBJECT_0) return 2;
    if (g_wret) return 3;
    if (g_werr != 735) return 4;
    if (!ex && g_wov != NULL) return 5;
    return 0;
}
TEST(aio_iocpmt_close_port_wakes_gqcs_waiter) {
    DWORD rc = t_run_child("aio_iocp_close_waiter", "", 20000);
    CHECKF(rc == 0, "child rc %lu (2=not woken 3=ret TRUE 4=err!=735 5=ov!=NULL)", rc);
}
TEST(aio_iocpmt_close_port_wakes_gqcsex_waiter) {
    DWORD rc = t_run_child("aio_iocp_close_waiter", "ex", 20000);
    CHECKF(rc == 0, "child rc %lu (2=not woken 3=ret TRUE 4=err!=735)", rc);
}

/* ---- BindIoCompletionCallback ---- */
static volatile LONG g_cb_hits; static DWORD g_cb_err, g_cb_bytes; static OVERLAPPED *g_cb_ov; static HANDLE g_cb_ev; static DWORD g_cb_tid;
static VOID WINAPI bind_cb(DWORD err, DWORD bytes, LPOVERLAPPED ov) {
    g_cb_err = err; g_cb_bytes = bytes; g_cb_ov = ov; g_cb_tid = GetCurrentThreadId(); InterlockedIncrement(&g_cb_hits); SetEvent(g_cb_ev);
}
TEST(aio_bindcb_read_completion) {
    REQUIRE(U(BindIoCompletionCallback)); REQUIRE(U(ReadFile));
    if (t_is_wine()) { t_skip("BindIoCompletionCallback unsupported under emulator"); return; }
    char p[MAX_PATH]; mkpath(p, "bind"); make_file(p, 8192);
    HANDLE f = open_ov(p, GENERIC_READ, 0); g_cb_ev = mkevent(); g_cb_hits = 0;
    SET_ERR(0); CHECK(U(BindIoCompletionCallback)(f, bind_cb, 0));
    OVERLAPPED ov; ovinit(&ov, 100, NULL); BYTE b[500];
    BOOL r = U(ReadFile)(f, b, 500, NULL, &ov); CHECK(r || GetLastError() == ERROR_IO_PENDING);
    CHECK(WaitForSingleObject(g_cb_ev, 5000) == WAIT_OBJECT_0);
    EXPECT_EQ(g_cb_hits, 1); EXPECT_EQ(g_cb_err, 0); EXPECT_EQ(g_cb_bytes, 500); CHECK(g_cb_ov == &ov);
    CHECK(g_cb_tid != GetCurrentThreadId()); CHECK(chkp(b, 100, 500));
    CloseHandle(f); CloseHandle(g_cb_ev);
}
TEST(aio_bindcb_error_completion) {
    REQUIRE(U(BindIoCompletionCallback)); REQUIRE(U(ReadFile));
    if (t_is_wine()) { t_skip("BindIoCompletionCallback unsupported under emulator"); return; }
    char p[MAX_PATH]; mkpath(p, "bind2"); make_file(p, 100);
    HANDLE f = open_ov(p, GENERIC_READ, 0); g_cb_ev = mkevent(); g_cb_hits = 0;
    CHECK(U(BindIoCompletionCallback)(f, bind_cb, 0));
    OVERLAPPED ov; ovinit(&ov, 100, NULL); BYTE b[16];
    SET_ERR(0); BOOL r = U(ReadFile)(f, b, 16, NULL, &ov);
    CHECK(!r);
    DWORD w = WaitForSingleObject(g_cb_ev, 1000);
    t_note("bound-handle EOF read: ReadFile err %lu, callback %s err=%lu", GetLastError(), w == WAIT_OBJECT_0 ? "invoked" : "not invoked", g_cb_err);
    CloseHandle(f); CloseHandle(g_cb_ev);
}
TEST(aio_bindcb_invalid_cases) {
    REQUIRE(U(BindIoCompletionCallback));
    if (t_is_wine()) { t_skip("BindIoCompletionCallback unsupported under emulator"); return; }
    char p[MAX_PATH]; mkpath(p, "bind3"); make_file(p, 100);
    HANDLE s = open_sync(p, GENERIC_READ);
    SET_ERR(0); CHECK(!U(BindIoCompletionCallback)(s, bind_cb, 0)); t_note("bind sync handle: err %lu", GetLastError());
    SET_ERR(0); CHECK(!U(BindIoCompletionCallback)(INVALID_HANDLE_VALUE, bind_cb, 0)); t_note("bind INVALID_HANDLE_VALUE: err %lu", GetLastError());
    SET_ERR(0); CHECK(!U(BindIoCompletionCallback)(NULL, bind_cb, 0)); t_note("bind NULL: err %lu", GetLastError());
    HANDLE f = open_ov(p, GENERIC_READ, 0);
    CHECK(U(BindIoCompletionCallback)(f, bind_cb, 0));
    SET_ERR(0); BOOL r = U(BindIoCompletionCallback)(f, bind_cb, 0);
    t_note("rebind same handle: %d err %lu", r, r ? 0 : GetLastError());
    CloseHandle(s); CloseHandle(f);
}

/* ---- SetFileIoOverlappedRange ---- */
TEST(aio_overlapped_range_validation) {
    REQUIRE(U(SetFileIoOverlappedRange));
    char p[MAX_PATH]; mkpath(p, "range"); make_file(p, 100);
    HANDLE f = open_ov(p, GENERIC_READ, 0);
    BYTE *m = VirtualAlloc(NULL, 65536, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    for (ULONG len = 0; len < 0x20; len += 7) {
        SET_ERR(0); CHECKF(!U(SetFileIoOverlappedRange)(f, m, len), "len %lu", len); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
    SET_ERR(0); CHECK(!U(SetFileIoOverlappedRange)(f, m, 0x1F)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!U(SetFileIoOverlappedRange)(INVALID_HANDLE_VALUE, m, 0x10)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); BOOL r = U(SetFileIoOverlappedRange)(f, m, 65536);
    t_note("SetFileIoOverlappedRange(valid): %d err %lu (needs SeLockMemoryPrivilege)", r, r ? 0 : GetLastError());
    CloseHandle(f); VirtualFree(m, 0, MEM_RELEASE);
}
TEST(aio_overlapped_range_invalid_handle) {
    REQUIRE(U(SetFileIoOverlappedRange));
    BYTE *m = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    SET_ERR(0); BOOL r = U(SetFileIoOverlappedRange)(INVALID_HANDLE_VALUE, m, 4096);
    CHECK(!r); t_note("range with INVALID_HANDLE_VALUE: err %lu", GetLastError());
    SET_ERR(0); r = U(SetFileIoOverlappedRange)(NULL, m, 4096);
    CHECK(!r); t_note("range with NULL: err %lu", GetLastError());
    VirtualFree(m, 0, MEM_RELEASE);
}

/* ---- connect pending cancelled by close ---- */
TEST(aio_pipe_close_cancels_pending_listen) {
    REQUIRE(U(ConnectNamedPipe));
    char name[128]; snprintf(name, sizeof name, "\\\\.\\pipe\\k32t_aio_cl_%lu", GetCurrentProcessId());
    HANDLE s = CreateNamedPipeA(name, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, NULL);
    CHECK(s != INVALID_HANDLE_VALUE); if (s == INVALID_HANDLE_VALUE) return;
    HANDLE ev = mkevent(); OVERLAPPED ov; ovinit(&ov, 0, ev);
    CHECK(!U(ConnectNamedPipe)(s, &ov)); EXPECT_ERR(ERROR_IO_PENDING);
    CloseHandle(s);
    CHECK(WaitForSingleObject(ev, 5000) == WAIT_OBJECT_0);
    DWORD n; SET_ERR(0); CHECK(!GetOverlappedResult(s, &ov, &n, FALSE));
    DWORD e = GetLastError(); CHECKF(e != 0 && e != ERROR_IO_INCOMPLETE, "err %lu", e);
    t_note("pending ConnectNamedPipe when server handle closed: err %lu (Internal 0x%lx)", e, (DWORD)ov.Internal);
    CloseHandle(ev);
}
