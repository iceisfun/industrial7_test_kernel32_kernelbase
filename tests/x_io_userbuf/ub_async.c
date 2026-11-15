/* Overlapped I/O, completion ports, ReadFileEx/WriteFileEx, scatter/gather,
 * GetQueuedCompletionStatusEx, GetOverlappedResult, ReadDirectoryChangesW and
 * DeviceIoControl with protected user buffers. */
#define UB_TU "a"
#include "ub.h"
#ifndef FSCTL_PIPE_PEEK
#define FSCTL_PIPE_PEEK CTL_CODE(FILE_DEVICE_NAMED_PIPE, 3, METHOD_BUFFERED, FILE_READ_DATA)
#endif

/* Packets currently queued on the port (drains them). */
static int port_drain(HANDLE port, DWORD first_ms, ULONG_PTR *key_out) {
    int n = 0; DWORD bytes; ULONG_PTR key; OVERLAPPED *o;
    while (GetQueuedCompletionStatus(port, &bytes, &key, &o, n ? 0 : first_ms) || o) { if (key_out) *key_out = key; n++; if (n > 16) break; }
    return n;
}

typedef struct { BOOL r; DWORD e; } ares;
static ares ov_read(HANDLE h, void *b, DWORD len, OVERLAPPED *ov) {
    ares x; SetLastError(0xBEEF); x.r = U(ReadFile)(h, b, len, NULL, ov); x.e = x.r ? 0 : GetLastError(); return x;
}
static ares ov_write(HANDLE h, const void *b, DWORD len, OVERLAPPED *ov) {
    ares x; SetLastError(0xBEEF); x.r = U(WriteFile)(h, b, len, NULL, ov); x.e = x.r ? 0 : GetLastError(); return x;
}
static void ov_settle(HANDLE h, OVERLAPPED *ov, ares x) {
    DWORD n; if (!x.r && x.e == ERROR_IO_PENDING && !GetOverlappedResultEx(h, ov, &n, 5000, FALSE)) { CancelIoEx(h, ov); GetOverlappedResultEx(h, ov, &n, 5000, FALSE); }
}

/* Overlapped read on an IOCP-associated handle: synchronous failure, no packet,
 * the caller's event is left as it was (signaled), Internal still STATUS_PENDING. */
static void iocp_read_bad(BYTE *bad, DWORD len, DWORD flags) {
    char p[MAX_PATH]; ub_path(p, "iocp"); CHECK(ub_mkfile(p, 8192));
    HANDLE h = ub_open(p, GENERIC_READ, FILE_FLAG_OVERLAPPED | flags); CHECK(h != INVALID_HANDLE_VALUE);
    HANDLE port = CreateIoCompletionPort(h, NULL, 7, 0); CHECK(port != NULL);
    HANDLE ev = ub_event(TRUE); OVERLAPPED ov; ub_ov(&ov, 0, ev);
    ares x = ov_read(h, bad, len, &ov);
    ov_settle(h, &ov, x);
    CHECK(!x.r); EXPECT_EQ(x.e, ERROR_NOACCESS);
    EXPECT_EQ((DWORD)ov.Internal, NTS_PENDING); EXPECT_EQ(ov.InternalHigh, 0);
    CHECK(ub_signaled(ev));
    EXPECT_EQ(port_drain(port, 100, NULL), 0);
    /* a valid read through the same handle posts exactly one packet */
    BYTE *ok = ub_va(PG, PAGE_READWRITE); ULONG_PTR key = 0;
    ResetEvent(ev); ub_ov(&ov, 0, ev);
    x = ov_read(h, ok, 4096, &ov);
    CHECK(x.r || x.e == ERROR_IO_PENDING);
    EXPECT_EQ(port_drain(port, 5000, &key), 1); EXPECT_EQ(key, 7);
    CHECK(ub_chk(ok, 0, 4096));
    ub_free(ok); CloseHandle(ev); CloseHandle(port); closeh(&h); ub_rm(p);
}

TEST(xubuf_iocp_read_noaccess_sync_fail_no_packet) {
    REQUIRE(U(ReadFile));
    BYTE *b = ub_pages(1, PAGE_NOACCESS, 0);
    iocp_read_bad(b, 4096, 0);
    ub_free(b);
}
TEST(xubuf_iocp_read_readonly_sync_fail_no_packet) {
    REQUIRE(U(ReadFile));
    BYTE *b = ub_pages(1, PAGE_READONLY, 0xCC);
    iocp_read_bad(b, 4096, 0);
    CHECK(ub_all(b, PG, 0xCC));
    ub_free(b);
}
TEST(xubuf_iocp_read_span_sync_fail_no_packet) {
    REQUIRE(U(ReadFile));
    BYTE *b = ub_span(PAGE_NOACCESS, 0xCC);
    iocp_read_bad(b, 8192, 0);
    CHECK(ub_all(b, PG, 0xCC));
    ub_free(b);
}
TEST(xubuf_iocp_read_nobuffering_noaccess_no_packet) {
    REQUIRE(U(ReadFile));
    BYTE *b = ub_pages(2, PAGE_NOACCESS, 0);
    iocp_read_bad(b, 8192, FILE_FLAG_NO_BUFFERING);
    ub_free(b);
}

/* Overlapped write from no-access memory on an IOCP handle: the write goes pending and
 * completes with STATUS_INVALID_USER_BUFFER, which does post a packet; size unchanged. */
TEST(xubuf_iocp_write_noaccess_pending_then_packet) {
    REQUIRE(U(WriteFile));
    char p[MAX_PATH]; ub_path(p, "iocpw"); CHECK(ub_mkfile(p, 4096));
    HANDLE h = ub_open(p, GENERIC_READ | GENERIC_WRITE, FILE_FLAG_OVERLAPPED);
    HANDLE port = CreateIoCompletionPort(h, NULL, 9, 0); CHECK(port != NULL);
    HANDLE ev = ub_event(TRUE); OVERLAPPED ov; ub_ov(&ov, 4096, ev);
    BYTE *b = ub_pages(1, PAGE_NOACCESS, 0);
    ares x = ov_write(h, b, 4096, &ov);
    CHECK(!x.r); EXPECT_EQ(x.e, ERROR_IO_PENDING);
    DWORD n = 0x5A5A5A5A; SetLastError(0);
    BOOL g = GetOverlappedResultEx(h, &ov, &n, 5000, FALSE); DWORD ge = g ? 0 : GetLastError();
    if (!g && ge == WAIT_TIMEOUT) { CancelIoEx(h, &ov); GetOverlappedResultEx(h, &ov, &n, 5000, FALSE); }
    CHECK(!g); EXPECT_EQ(ge, ERROR_INVALID_USER_BUFFER); EXPECT_EQ(n, 0);
    EXPECT_EQ((DWORD)ov.Internal, 0xC00000E8u);
    EXPECT_EQ(port_drain(port, 5000, NULL), 1);
    EXPECT_EQ(ub_size(h), 4096);
    ub_free(b); CloseHandle(ev); CloseHandle(port); closeh(&h); ub_rm(p);
}

/* Named pipe overlapped read on an IOCP handle, data present. */
TEST(xubuf_iocp_pipe_read_noaccess_no_packet_data_kept) {
    REQUIRE(U(ReadFile));
    HANDLE s, c; CHECK(ub_npipe(&s, &c, FALSE, TRUE, NULL));
    HANDLE port = CreateIoCompletionPort(c, NULL, 3, 0); CHECK(port != NULL);
    CHECK(ub_pwrite(s, "abcdefgh", 8));
    HANDLE ev = ub_event(TRUE); OVERLAPPED ov; ub_ov(&ov, 0, ev);
    BYTE *b = ub_pages(1, PAGE_NOACCESS, 0);
    ares x = ov_read(c, b, 64, &ov);
    ov_settle(c, &ov, x);
    CHECK(!x.r); EXPECT_EQ(x.e, ERROR_NOACCESS);
    CHECK(ub_signaled(ev));
    EXPECT_EQ(port_drain(port, 100, NULL), 0);
    EXPECT_EQ(ub_avail(c), 8);
    ub_free(b); CloseHandle(ev); CloseHandle(port); closeh(&c); closeh(&s);
}

/* ---- ReadFileEx / WriteFileEx ---- */
static volatile LONG g_apc, g_apc_err, g_apc_n;
static void WINAPI apc_cb(DWORD err, DWORD n, LPOVERLAPPED ov) { (void)ov; g_apc++; g_apc_err = (LONG)err; g_apc_n = (LONG)n; }

/* want_ok: the call returns TRUE (guard page: the warning status is not treated as a
 * failure) but no APC is ever queued. */
static void readex_bad(BYTE *bad, DWORD len, BOOL want_ok) {
    char p[MAX_PATH]; ub_path(p, "rex"); CHECK(ub_mkfile(p, 8192));
    HANDLE h = ub_open(p, GENERIC_READ, FILE_FLAG_OVERLAPPED);
    OVERLAPPED ov; ub_ov(&ov, 0, NULL); g_apc = 0;
    SetLastError(0xBEEF);
    BOOL r = U(ReadFileEx)(h, bad, len, &ov, apc_cb); DWORD e = r ? 0 : GetLastError();
    if (want_ok) CHECK(r); else { CHECK(!r); EXPECT_EQ(e, ERROR_NOACCESS); }
    EXPECT_EQ(SleepEx(200, TRUE), 0); EXPECT_EQ(g_apc, 0);
    BYTE *ok = ub_va(PG, PAGE_READWRITE); ub_ov(&ov, 0, NULL);
    r = U(ReadFileEx)(h, ok, 4096, &ov, apc_cb); CHECK(r);
    if (r) { EXPECT_EQ(SleepEx(5000, TRUE), WAIT_IO_COMPLETION); EXPECT_EQ(g_apc, 1); EXPECT_EQ(g_apc_n, 4096); CHECK(ub_chk(ok, 0, 4096)); }
    ub_free(ok); closeh(&h); ub_rm(p);
}

TEST(xubuf_readfileex_noaccess_no_apc) {
    REQUIRE(U(ReadFileEx));
    BYTE *b = ub_pages(1, PAGE_NOACCESS, 0);
    readex_bad(b, 4096, FALSE);
    ub_free(b);
}
TEST(xubuf_readfileex_readonly_no_apc) {
    REQUIRE(U(ReadFileEx));
    BYTE *b = ub_pages(1, PAGE_READONLY, 0);
    readex_bad(b, 4096, FALSE);
    ub_free(b);
}
TEST(xubuf_readfileex_guard_true_but_no_apc) {
    REQUIRE(U(ReadFileEx));
    BYTE *b = ub_va(PG, PAGE_READWRITE | PAGE_GUARD);
    readex_bad(b, 4096, TRUE);
    EXPECT_EQ(ub_prot(b), PAGE_READWRITE);
    ub_free(b);
}

/* The write is accepted; the failure arrives through the completion routine. */
TEST(xubuf_writefileex_noaccess_error_via_apc) {
    REQUIRE(U(WriteFileEx));
    char p[MAX_PATH]; ub_path(p, "wex"); CHECK(ub_mkfile(p, 4096));
    HANDLE h = ub_open(p, GENERIC_READ | GENERIC_WRITE, FILE_FLAG_OVERLAPPED);
    OVERLAPPED ov; ub_ov(&ov, 4096, NULL); g_apc = 0; g_apc_err = -1; g_apc_n = -1;
    BYTE *b = ub_pages(1, PAGE_NOACCESS, 0);
    SetLastError(0xBEEF);
    BOOL r = U(WriteFileEx)(h, b, 4096, &ov, apc_cb); DWORD e = r ? 0 : GetLastError();
    DWORD sl = SleepEx(5000, TRUE);
    CHECK(r);
    EXPECT_EQ(sl, WAIT_IO_COMPLETION); EXPECT_EQ(g_apc, 1);
    EXPECT_EQ(g_apc_err, ERROR_INVALID_USER_BUFFER); EXPECT_EQ(g_apc_n, 0);
    EXPECT_EQ(ub_size(h), 4096);
    ub_free(b); closeh(&h); ub_rm(p);
}

/* ---- ReadFileScatter / WriteFileGather (unbuffered, overlapped) ---- */
static HANDLE sg_open(const char *p) { return ub_open(p, GENERIC_READ | GENERIC_WRITE, FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING); }

static void scatter_bad(DWORD prot2) {
    REQUIRE(U(ReadFileScatter));
    char p[MAX_PATH]; ub_path(p, "sc"); CHECK(ub_mkfile(p, 8192));
    HANDLE h = sg_open(p); CHECK(h != INVALID_HANDLE_VALUE);
    BYTE *good = ub_pages(1, PAGE_READWRITE, 0xCC), *bad = prot2 ? ub_pages(1, prot2, 0xCC) : ub_reserve(PG);
    FILE_SEGMENT_ELEMENT seg[3]; memset(seg, 0, sizeof seg);
    seg[0].Buffer = PtrToPtr64(good); seg[1].Buffer = PtrToPtr64(bad);
    HANDLE ev = ub_event(TRUE); OVERLAPPED ov; ub_ov(&ov, 0, ev);
    SetLastError(0xBEEF);
    BOOL r = U(ReadFileScatter)(h, seg, 8192, NULL, &ov); DWORD e = r ? 0 : GetLastError();
    ov_settle(h, &ov, (ares){r, e});
    CHECK(!r); EXPECT_EQ(e, ERROR_NOACCESS);
    CHECK(ub_all(good, PG, 0xCC));
    /* unlike ReadFile's up-front probe, the segment pages fail after the event was reset */
    CHECK(!ub_signaled(ev));
    ub_free(good); ub_free(bad); CloseHandle(ev); closeh(&h); ub_rm(p);
}
TEST(xubuf_scatter_segment_noaccess) { scatter_bad(PAGE_NOACCESS); }
TEST(xubuf_scatter_segment_readonly) { scatter_bad(PAGE_READONLY); }
TEST(xubuf_scatter_segment_reserved) { scatter_bad(0); }

TEST(xubuf_gather_segment_noaccess_file_unchanged) {
    REQUIRE(U(WriteFileGather));
    char p[MAX_PATH]; ub_path(p, "ga"); CHECK(ub_mkfile(p, 8192));
    HANDLE h = sg_open(p);
    BYTE *good = ub_pages(1, PAGE_READWRITE, 0x11), *bad = ub_pages(1, PAGE_NOACCESS, 0x22);
    FILE_SEGMENT_ELEMENT seg[3]; memset(seg, 0, sizeof seg);
    seg[0].Buffer = PtrToPtr64(good); seg[1].Buffer = PtrToPtr64(bad);
    HANDLE ev = ub_event(TRUE); OVERLAPPED ov; ub_ov(&ov, 0, ev);
    SetLastError(0xBEEF);
    BOOL r = U(WriteFileGather)(h, seg, 8192, NULL, &ov); DWORD e = r ? 0 : GetLastError();
    ov_settle(h, &ov, (ares){r, e});
    CHECK(!r); EXPECT_EQ(e, ERROR_NOACCESS);
    closeh(&h); CHECK(ub_file_pat(p, 0, 8192));
    ub_free(good); ub_free(bad); CloseHandle(ev); ub_rm(p);
}

TEST(xubuf_gather_segment_readonly_ok) {
    REQUIRE(U(WriteFileGather));
    char p[MAX_PATH]; ub_path(p, "gro"); CHECK(ub_mkfile(p, 8192));
    HANDLE h = sg_open(p);
    BYTE *a = ub_va(PG, PAGE_READWRITE), *b = ub_va(PG, PAGE_READWRITE);
    memset(a, 0x41, PG); memset(b, 0x42, PG); ub_protect(a, PG, PAGE_READONLY); ub_protect(b, PG, PAGE_READONLY);
    FILE_SEGMENT_ELEMENT seg[3]; memset(seg, 0, sizeof seg);
    seg[0].Buffer = PtrToPtr64(a); seg[1].Buffer = PtrToPtr64(b);
    HANDLE ev = ub_event(FALSE); OVERLAPPED ov; ub_ov(&ov, 0, ev);
    BOOL r = U(WriteFileGather)(h, seg, 8192, NULL, &ov); DWORD e = r ? 0 : GetLastError();
    CHECK(r || e == ERROR_IO_PENDING);
    DWORD n = 0; CHECK(GetOverlappedResultEx(h, &ov, &n, 5000, FALSE)); EXPECT_EQ(n, 8192);
    closeh(&h);
    HANDLE hr = ub_open(p, GENERIC_READ, 0); BYTE *chk = ub_va(2 * PG, PAGE_READWRITE); DWORD got = 0;
    CHECK(ReadFile(hr, chk, 2 * PG, &got, NULL) && got == 2 * PG);
    CHECK(ub_all(chk, PG, 0x41)); CHECK(ub_all(chk + PG, PG, 0x42));
    ub_free(chk); closeh(&hr); ub_free(a); ub_free(b); CloseHandle(ev); ub_rm(p);
}

/* The segment array itself is inaccessible: run in a child. */
CHILD(xubuf_c_scatter_array_noaccess) {
    ub_child_init();
    HANDLE h = sg_open(argv[1]); if (h == INVALID_HANDLE_VALUE) return R_SETUP;
    FILE_SEGMENT_ELEMENT *seg = (FILE_SEGMENT_ELEMENT *)ub_pages(1, PAGE_NOACCESS, 0);
    HANDLE ev = ub_event(FALSE); OVERLAPPED ov; ub_ov(&ov, 0, ev);
    SetLastError(0xBEEF);
    BOOL r = U(ReadFileScatter)(h, seg, 4096, NULL, &ov);
    return (int)R_BOOL(r);
}
TEST(xubuf_scatter_segment_array_noaccess) {
    REQUIRE(U(ReadFileScatter));
    char p[MAX_PATH]; ub_path(p, "sca"); CHECK(ub_mkfile(p, 8192));
    EXPECT_EQ(ub_child("xubuf_c_scatter_array_noaccess", p), R_ERR(ERROR_NOACCESS));
    ub_rm(p);
}

/* ---- GetQueuedCompletionStatusEx (children) ---- */
#define R_KEEP 0x00800000u
/* After the call under test: does a valid dequeue still return the posted packet? */
static DWORD gq_kept(HANDLE port) {
    OVERLAPPED_ENTRY e[4]; ULONG n = 0; memset(e, 0, sizeof e);
    if (GetQueuedCompletionStatusEx(port, e, 4, &n, 0, FALSE) && n == 1 && e[0].lpCompletionKey == 0x55 && e[0].dwNumberOfBytesTransferred == 33) return R_KEEP;
    return 0;
}
static DWORD gq_case(OVERLAPPED_ENTRY *ent, ULONG *removed, BOOL post) {
    HANDLE port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
    if (!port) return R_SETUP;
    if (post) PostQueuedCompletionStatus(port, 33, 0x55, NULL);
    SetLastError(0xBEEF);
    BOOL r = U(GetQueuedCompletionStatusEx)(port, ent, 4, removed, 0, FALSE);
    DWORD res = R_BOOL(r);
    return res | (post ? gq_kept(port) : 0);
}
CHILD(xubuf_c_gqcsex_entries_readonly) { ub_child_init(); ULONG n = 0; return (int)gq_case((OVERLAPPED_ENTRY *)ub_pages(1, PAGE_READONLY, 0), &n, TRUE); }
CHILD(xubuf_c_gqcsex_entries_noaccess) { ub_child_init(); ULONG n = 0; return (int)gq_case((OVERLAPPED_ENTRY *)ub_pages(1, PAGE_NOACCESS, 0), &n, TRUE); }
CHILD(xubuf_c_gqcsex_entries_noaccess_empty) { ub_child_init(); ULONG n = 0; return (int)gq_case((OVERLAPPED_ENTRY *)ub_pages(1, PAGE_NOACCESS, 0), &n, FALSE); }
CHILD(xubuf_c_gqcsex_removed_readonly) { ub_child_init(); OVERLAPPED_ENTRY e[4]; return (int)gq_case(e, (ULONG *)ub_pages(1, PAGE_READONLY, 0), TRUE); }
CHILD(xubuf_c_gqcsex_entries_span) {
    ub_child_init();
    BYTE *b = ub_span(PAGE_NOACCESS, 0); ULONG n = 0;
    /* first entry fits in the valid page, the remaining three do not */
    return (int)gq_case((OVERLAPPED_ENTRY *)(b + PG - sizeof(OVERLAPPED_ENTRY)), &n, TRUE);
}

TEST(xubuf_gqcsex_entries_readonly_packet_kept) {
    REQUIRE(U(GetQueuedCompletionStatusEx));
    EXPECT_EQ(ub_child("xubuf_c_gqcsex_entries_readonly", ""), R_ERR(ERROR_NOACCESS) | R_KEEP);
}
TEST(xubuf_gqcsex_entries_noaccess_packet_kept) {
    REQUIRE(U(GetQueuedCompletionStatusEx));
    EXPECT_EQ(ub_child("xubuf_c_gqcsex_entries_noaccess", ""), R_ERR(ERROR_NOACCESS) | R_KEEP);
}
TEST(xubuf_gqcsex_entries_noaccess_empty_port) {
    REQUIRE(U(GetQueuedCompletionStatusEx));
    EXPECT_EQ(ub_child("xubuf_c_gqcsex_entries_noaccess_empty", ""), R_ERR(ERROR_NOACCESS));
}
TEST(xubuf_gqcsex_removed_count_readonly) {
    REQUIRE(U(GetQueuedCompletionStatusEx));
    EXPECT_EQ(ub_child("xubuf_c_gqcsex_removed_readonly", ""), R_ERR(ERROR_NOACCESS) | R_KEEP);
}
TEST(xubuf_gqcsex_entries_span_whole_array_probed) {
    REQUIRE(U(GetQueuedCompletionStatusEx));
    EXPECT_EQ(ub_child("xubuf_c_gqcsex_entries_span", ""), R_ERR(ERROR_NOACCESS) | R_KEEP);
}

/* ---- GetOverlappedResult ---- */
/* After completion the OVERLAPPED is only read: a read-only page is fine. */
TEST(xubuf_gor_overlapped_readonly_after_completion) {
    REQUIRE(U(GetOverlappedResult));
    char p[MAX_PATH]; ub_path(p, "gor"); CHECK(ub_mkfile(p, 8192));
    HANDLE h = ub_open(p, GENERIC_READ, FILE_FLAG_OVERLAPPED);
    BYTE *page = ub_va(PG, PAGE_READWRITE); OVERLAPPED *ov = (OVERLAPPED *)page;
    HANDLE ev = ub_event(FALSE); ub_ov(ov, 0, ev);
    BYTE *b = ub_va(PG, PAGE_READWRITE);
    BOOL r = ReadFile(h, b, 4096, NULL, ov);
    CHECK(r || GetLastError() == ERROR_IO_PENDING);
    CHECK(WaitForSingleObject(ev, 5000) == WAIT_OBJECT_0);
    ub_protect(page, PG, PAGE_READONLY);
    DWORD n = 0; SetLastError(0xBEEF);
    CHECK(U(GetOverlappedResult)(h, ov, &n, FALSE)); EXPECT_EQ(n, 4096);
    n = 0; CHECK(U(GetOverlappedResult)(h, ov, &n, TRUE)); EXPECT_EQ(n, 4096);
    ub_free(b); ub_free(page); CloseHandle(ev); closeh(&h); ub_rm(p);
}

CHILD(xubuf_c_gor_bytes_readonly) {
    ub_child_init();
    HANDLE h = ub_open(argv[1], GENERIC_READ, FILE_FLAG_OVERLAPPED); if (h == INVALID_HANDLE_VALUE) return R_SETUP;
    HANDLE ev = ub_event(FALSE); OVERLAPPED ov; ub_ov(&ov, 0, ev); BYTE b[512];
    if (!ReadFile(h, b, sizeof b, NULL, &ov) && GetLastError() != ERROR_IO_PENDING) return R_SETUP;
    if (WaitForSingleObject(ev, 5000) != WAIT_OBJECT_0) return R_SETUP;
    DWORD *n = (DWORD *)ub_pages(1, PAGE_READONLY, 0);
    SetLastError(0xBEEF);
    return (int)R_BOOL(U(GetOverlappedResult)(h, &ov, n, FALSE));
}
TEST(xubuf_gor_bytes_out_readonly_faults) {
    REQUIRE(U(GetOverlappedResult));
    char p[MAX_PATH]; ub_path(p, "gorb"); CHECK(ub_mkfile(p, 4096));
    EXPECT_EQ(ub_child("xubuf_c_gor_bytes_readonly", p), EXC_AV);
    ub_rm(p);
}

/* ---- ReadDirectoryChangesW ---- */
static void rdcw_bad(BYTE *bad) {
    REQUIRE(U(ReadDirectoryChangesW));
    char d[MAX_PATH], f[MAX_PATH + 32]; ub_path(d, "dir"); CHECK(CreateDirectoryA(d, NULL));
    HANDLE hd = CreateFileA(d, FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
    CHECK(hd != INVALID_HANDLE_VALUE);
    HANDLE ev = ub_event(TRUE); OVERLAPPED ov; ub_ov(&ov, 0, ev);
    SetLastError(0xBEEF);
    BOOL r = U(ReadDirectoryChangesW)(hd, bad, 4096, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, NULL, &ov, NULL);
    DWORD e = r ? 0 : GetLastError();
    snprintf(f, sizeof f, "%s\\a.txt", d);
    if (r) {
        /* issued: provoke a change and see how the completion reports the bad buffer */
        DWORD n = 0; ResetEvent(ev); ub_mkfile(f, 1);
        BOOL g = GetOverlappedResultEx(hd, &ov, &n, 3000, FALSE); DWORD ge = g ? 0 : GetLastError();
        t_note("RDCW issued; completion g=%d e=%lu n=%lu", g, ge, n);
        if (!g && ge == WAIT_TIMEOUT) { CancelIoEx(hd, &ov); GetOverlappedResultEx(hd, &ov, &n, 5000, FALSE); }
        ub_rm(f);
    }
    CHECK(!r); EXPECT_EQ(e, ERROR_NOACCESS);
    CHECK(ub_signaled(ev));
    if (!r) {
        /* the directory can still be watched with a valid buffer */
        DWORD *ok = (DWORD *)ub_va(PG, PAGE_READWRITE); DWORD n = 0;
        ResetEvent(ev); ub_ov(&ov, 0, ev);
        CHECK(U(ReadDirectoryChangesW)(hd, ok, 4096, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, NULL, &ov, NULL));
        ub_mkfile(f, 1);
        BOOL g = GetOverlappedResultEx(hd, &ov, &n, 5000, FALSE);
        CHECK(g); CHECK(n >= sizeof(FILE_NOTIFY_INFORMATION));
        if (g && n) { FILE_NOTIFY_INFORMATION *fi = (FILE_NOTIFY_INFORMATION *)ok; EXPECT_EQ(fi->Action, FILE_ACTION_ADDED); }
        else { CancelIoEx(hd, &ov); GetOverlappedResultEx(hd, &ov, &n, 5000, FALSE); }
        ub_rm(f); ub_free(ok);
    }
    CloseHandle(ev); closeh(&hd); RemoveDirectoryA(d);
}
TEST(xubuf_rdcw_readonly_buffer_fails_at_issue) {
    BYTE *b = ub_pages(1, PAGE_READONLY, 0); rdcw_bad(b); ub_free(b);
}
TEST(xubuf_rdcw_noaccess_buffer_fails_at_issue) {
    BYTE *b = ub_pages(1, PAGE_NOACCESS, 0); rdcw_bad(b); ub_free(b);
}
TEST(xubuf_rdcw_span_buffer_fails_at_issue) {
    BYTE *b = ub_span(PAGE_NOACCESS, 0); rdcw_bad(b + PG - 2048); ub_free(b);
}

/* ---- DeviceIoControl, METHOD_BUFFERED ---- */
typedef struct { BOOL r; DWORD e; DWORD n; } dres;
static dres dioc(HANDLE h, DWORD code, void *in, DWORD nin, void *out, DWORD nout) {
    dres x; x.n = 0x5A5A5A5A; SetLastError(0xBEEF);
    x.r = U(DeviceIoControl)(h, code, in, nin, out, nout, &x.n, NULL); x.e = x.r ? 0 : GetLastError(); return x;
}

/* The output buffer is checked before the file system sees the request:
 * ERROR_NOACCESS, not ERROR_NOT_A_REPARSE_POINT. */
static void reparse_out_bad(BYTE *bad) {
    REQUIRE(U(DeviceIoControl));
    char p[MAX_PATH]; ub_path(p, "rp"); CHECK(ub_mkfile(p, 10));
    HANDLE h = ub_open(p, GENERIC_READ, 0);
    dres x = dioc(h, FSCTL_GET_REPARSE_POINT, NULL, 0, bad, 1024);
    CHECK(!x.r); EXPECT_EQ(x.e, ERROR_NOACCESS);
    BYTE ok[1024];
    x = dioc(h, FSCTL_GET_REPARSE_POINT, NULL, 0, ok, sizeof ok);
    CHECK(!x.r); EXPECT_EQ(x.e, ERROR_NOT_A_REPARSE_POINT);
    closeh(&h); ub_rm(p);
}
TEST(xubuf_dioctl_get_reparse_out_noaccess) { BYTE *b = ub_pages(1, PAGE_NOACCESS, 0); reparse_out_bad(b); ub_free(b); }
TEST(xubuf_dioctl_get_reparse_out_readonly) { BYTE *b = ub_pages(1, PAGE_READONLY, 0); reparse_out_bad(b); ub_free(b); }

TEST(xubuf_dioctl_zero_data_in_noaccess_file_unchanged) {
    REQUIRE(U(DeviceIoControl));
    char p[MAX_PATH]; ub_path(p, "zd"); CHECK(ub_mkfile(p, 8192));
    HANDLE h = ub_open(p, GENERIC_READ | GENERIC_WRITE, 0);
    BYTE *b = ub_pages(1, PAGE_NOACCESS, 0);
    dres x = dioc(h, FSCTL_SET_ZERO_DATA, b, sizeof(FILE_ZERO_DATA_INFORMATION), NULL, 0);
    CHECK(!x.r); EXPECT_EQ(x.e, ERROR_NOACCESS);
    closeh(&h); CHECK(ub_file_pat(p, 0, 8192));
    ub_free(b); ub_rm(p);
}

TEST(xubuf_dioctl_zero_data_in_readonly_ok) {
    REQUIRE(U(DeviceIoControl));
    char p[MAX_PATH]; ub_path(p, "zdr"); CHECK(ub_mkfile(p, 8192));
    HANDLE h = ub_open(p, GENERIC_READ | GENERIC_WRITE, 0);
    FILE_ZERO_DATA_INFORMATION *z = (FILE_ZERO_DATA_INFORMATION *)ub_va(PG, PAGE_READWRITE);
    z->FileOffset.QuadPart = 0; z->BeyondFinalZero.QuadPart = 4096; ub_protect(z, PG, PAGE_READONLY);
    dres x = dioc(h, FSCTL_SET_ZERO_DATA, z, sizeof *z, NULL, 0);
    CHECK(x.r); EXPECT_EQ(x.n, 0);
    closeh(&h);
    HANDLE hr = ub_open(p, GENERIC_READ, 0); BYTE b[8192]; DWORD got = 0;
    CHECK(ReadFile(hr, b, sizeof b, &got, NULL) && got == 8192);
    CHECK(ub_all(b, 4096, 0)); CHECK(ub_chk(b + 4096, 4096, 4096));
    closeh(&hr); ub_free(z); ub_rm(p);
}

/* FSCTL_PIPE_PEEK is METHOD_BUFFERED: a no-access output buffer fails and the data stays. */
TEST(xubuf_dioctl_pipe_peek_out_noaccess) {
    REQUIRE(U(DeviceIoControl));
    HANDLE s, c; CHECK(ub_npipe(&s, &c, FALSE, FALSE, NULL));
    CHECK(ub_pwrite(s, "0123456789", 10));
    BYTE *b = ub_pages(1, PAGE_NOACCESS, 0);
    dres x = dioc(c, FSCTL_PIPE_PEEK, NULL, 0, b, 64);
    CHECK(!x.r); EXPECT_EQ(x.e, ERROR_NOACCESS);
    EXPECT_EQ(ub_avail(c), 10);
    ub_free(b); closeh(&c); closeh(&s);
}

TEST(xubuf_dioctl_iocp_out_noaccess_no_packet) {
    REQUIRE(U(DeviceIoControl));
    char p[MAX_PATH]; ub_path(p, "dio"); CHECK(ub_mkfile(p, 10));
    HANDLE h = ub_open(p, GENERIC_READ, FILE_FLAG_OVERLAPPED);
    HANDLE port = CreateIoCompletionPort(h, NULL, 4, 0); CHECK(port != NULL);
    HANDLE ev = ub_event(TRUE); OVERLAPPED ov; ub_ov(&ov, 0, ev);
    BYTE *b = ub_pages(1, PAGE_NOACCESS, 0); DWORD n = 0x77;
    SetLastError(0xBEEF);
    BOOL r = U(DeviceIoControl)(h, FSCTL_GET_COMPRESSION, NULL, 0, b, sizeof(USHORT), &n, &ov);
    DWORD e = r ? 0 : GetLastError();
    ov_settle(h, &ov, (ares){r, e});
    CHECK(!r); EXPECT_EQ(e, ERROR_NOACCESS);
    CHECK(ub_signaled(ev));
    EXPECT_EQ(port_drain(port, 100, NULL), 0);
    ub_free(b); CloseHandle(ev); CloseHandle(port); closeh(&h); ub_rm(p);
}

CHILD(xubuf_c_dioctl_bytes_readonly) {
    ub_child_init();
    HANDLE h = ub_open(argv[1], GENERIC_READ, 0); if (h == INVALID_HANDLE_VALUE) return R_SETUP;
    USHORT comp = 0x7777; DWORD *n = (DWORD *)ub_pages(1, PAGE_READONLY, 0);
    SetLastError(0xBEEF);
    BOOL r = U(DeviceIoControl)(h, FSCTL_GET_COMPRESSION, NULL, 0, &comp, sizeof comp, n, NULL);
    return (int)R_BOOL(r);
}
TEST(xubuf_dioctl_bytes_returned_readonly) {
    REQUIRE(U(DeviceIoControl));
    char p[MAX_PATH]; ub_path(p, "diob"); CHECK(ub_mkfile(p, 10));
    EXPECT_EQ(ub_child("xubuf_c_dioctl_bytes_readonly", p), EXC_AV);
    ub_rm(p);
}
