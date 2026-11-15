/* Shared helpers for the x_io_cancel area (all static).
 * Every blocking worker has a bounded fallback: if a cancel does not end the
 * operation, the helper completes it the natural way (release the lock, connect
 * a client, write a message, touch the watched directory) and finally closes the
 * handle, so a missing cancel fails the test instead of hanging the area. */
#ifndef XC_H
#define XC_H
#include "t.h"
#include <stdlib.h>
#include <winioctl.h>

/* Single-evaluation versions of the harness macros. */
#undef EXPECT_EQ
#define EXPECT_EQ(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)a_, (unsigned long long)b_); } while (0)
#undef EXPECT_ERR
#define EXPECT_ERR(err) do { DWORD e_ = GetLastError(); \
    t_check(e_ == (DWORD)(err), __FILE__, __LINE__, #err, "GetLastError got %lu want %lu", e_, (DWORD)(err)); } while (0)
#define EXPECT_DW(a, b) do { DWORD a_ = (DWORD)(a), b_ = (DWORD)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got %lu (0x%lx) want %lu (0x%lx)", a_, a_, b_, b_); } while (0)

#define NTS_SUCCESS   0x00000000u
#define NTS_PENDING   0x00000103u
#define NTS_CANCELLED 0xC0000120u
#define NTS_NOTIFY_CLEANUP 0x0000010Bu
#ifndef FSCTL_PIPE_LISTEN
#define FSCTL_PIPE_LISTEN 0x00110008

#endif

static DWORD ovst(const OVERLAPPED *ov) { return (DWORD)ov->Internal; }
static HANDLE xev(void) { return CreateEventA(NULL, TRUE, FALSE, NULL); }
static void xov(OVERLAPPED *ov, ULONGLONG off, HANDLE ev) {
    memset(ov, 0, sizeof *ov); ov->Offset = (DWORD)off; ov->OffsetHigh = (DWORD)(off >> 32); ov->hEvent = ev;
}
static int xsig(HANDLE h) { return WaitForSingleObject(h, 0) == WAIT_OBJECT_0; }
static int xwait(HANDLE h, DWORD ms) { return WaitForSingleObject(h, ms) == WAIT_OBJECT_0; }
static void xclose(HANDLE *h) { if (*h && *h != INVALID_HANDLE_VALUE) CloseHandle(*h); *h = INVALID_HANDLE_VALUE; }
/* Wait (bounded) for an overlapped op: returns GetOverlappedResultEx result, error and count. */
typedef struct { BOOL ok; DWORD err; DWORD n; } xres;
static xres xfin(HANDLE h, OVERLAPPED *ov, DWORD ms) {
    xres r; r.n = 0xFFFF; SetLastError(0);
    r.ok = GetOverlappedResultEx(h, ov, &r.n, ms, FALSE); r.err = r.ok ? 0 : GetLastError(); return r;
}
/* Read the result of an op whose event/handle is already signalled (no wait, handle may be closed). */
static xres xnow(HANDLE h, OVERLAPPED *ov) {
    xres r; r.n = 0xFFFF; SetLastError(0);
    r.ok = GetOverlappedResult(h, ov, &r.n, FALSE); r.err = r.ok ? 0 : GetLastError(); return r;
}
/* An op ended as cancelled. */
#define CHECK_ABORTED(x) do { xres x_ = (x); \
    t_check(!x_.ok && x_.err == ERROR_OPERATION_ABORTED && x_.n == 0, __FILE__, __LINE__, "aborted: " #x, \
            "ok=%d err=%lu n=%lu", x_.ok, x_.err, x_.n); } while (0)
#define CHECK_PENDING(ov) do { DWORD s_ = ovst(ov); \
    t_check(s_ == NTS_PENDING, __FILE__, __LINE__, "pending: " #ov, "Internal=0x%lx", s_); } while (0)
/* Call returned FALSE/ERROR_IO_PENDING. */
#define CHECK_IOPEND(call) do { SetLastError(0xBEEF); BOOL r_ = (call); DWORD e_ = GetLastError(); \
    t_check(!r_ && e_ == ERROR_IO_PENDING, __FILE__, __LINE__, #call, "ret=%d err=%lu", r_, e_); } while (0)
#define CHECK_FAILS(call, err) do { SetLastError(0xBEEF); BOOL r_ = (call); DWORD e_ = GetLastError(); \
    t_check(!r_ && e_ == (DWORD)(err), __FILE__, __LINE__, #call, "ret=%d err=%lu want %lu", r_, e_, (DWORD)(err)); } while (0)

/* ---- names ---- */
static int g_xseq;
static void xpipename(char *out, size_t cap) { snprintf(out, cap, "\\\\.\\pipe\\k32t_%lu_xcan_%s_%d", GetCurrentProcessId(), XC_TU, ++g_xseq); }
static void xslotname(char *out, size_t cap) { snprintf(out, cap, "\\\\.\\mailslot\\k32t_%lu_xcan_%s_%d", GetCurrentProcessId(), XC_TU, ++g_xseq); }
static void xpath(char *out, const char *stem) { char leaf[96]; snprintf(leaf, sizeof leaf, "xcan_%s_%s_%d", XC_TU, stem, ++g_xseq); t_tmpfile(out, MAX_PATH, leaf); }
static WCHAR *xw(const char *s) { static WCHAR ring[4][400]; static int i; WCHAR *o = ring[i++ & 3]; MultiByteToWideChar(CP_ACP, 0, s, -1, o, 400); return o; }

/* ---- files ---- */
static HANDLE xopen(const char *p, BOOL ov, DWORD disp) {
    return CreateFileA(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, disp,
                       ov ? FILE_FLAG_OVERLAPPED : FILE_ATTRIBUTE_NORMAL, NULL);
}
/* A scratch file of 4096 bytes plus two handles: holder (sync) and waiter (overlapped unless sync_waiter). */
typedef struct { char path[MAX_PATH]; HANDLE hold, wait; } xlockf;
static BOOL xlock_open(xlockf *f, BOOL sync_waiter) {
    xpath(f->path, "lk");
    f->hold = xopen(f->path, FALSE, CREATE_ALWAYS);
    if (f->hold == INVALID_HANDLE_VALUE) { f->wait = INVALID_HANDLE_VALUE; return FALSE; }
    BYTE z[4096]; DWORD n; memset(z, 0x5a, sizeof z); WriteFile(f->hold, z, sizeof z, &n, NULL);
    f->wait = xopen(f->path, !sync_waiter, OPEN_EXISTING);
    return f->wait != INVALID_HANDLE_VALUE;
}
/* Holder takes an exclusive fail-immediately lock of [off, off+len). */
static BOOL xlock_hold(HANDLE h, DWORD off, DWORD len) {
    OVERLAPPED ov; xov(&ov, off, NULL);
    return LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, len, 0, &ov);
}
static BOOL xunlock(HANDLE h, DWORD off, DWORD len) { OVERLAPPED ov; xov(&ov, off, NULL); return UnlockFileEx(h, 0, len, 0, &ov); }
/* A fresh handle can take an exclusive fail-immediately lock of the range (then releases it). */
static BOOL xrange_free(const char *path, DWORD off, DWORD len) {
    HANDLE h = xopen(path, FALSE, OPEN_EXISTING); if (h == INVALID_HANDLE_VALUE) return FALSE;
    BOOL r = xlock_hold(h, off, len); if (r) xunlock(h, off, len); CloseHandle(h); return r;
}
static void xlock_close(xlockf *f) { xclose(&f->wait); xclose(&f->hold); DeleteFileA(f->path); }

/* ---- pipes ---- */
/* Overlapped duplex byte pipe, connected; srv/cli overlapped unless the flags say otherwise. */
static BOOL xpipe_ex(HANDLE *srv, HANDLE *cli, DWORD bufsz, DWORD mode, BOOL srv_ov, BOOL cli_ov) {
    char name[160]; xpipename(name, sizeof name);
    *cli = INVALID_HANDLE_VALUE;
    *srv = CreateNamedPipeA(name, PIPE_ACCESS_DUPLEX | (srv_ov ? FILE_FLAG_OVERLAPPED : 0), mode | PIPE_WAIT, 1, bufsz, bufsz, 0, NULL);
    if (*srv == INVALID_HANDLE_VALUE) return FALSE;
    *cli = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, cli_ov ? FILE_FLAG_OVERLAPPED : 0, NULL);
    if (*cli == INVALID_HANDLE_VALUE) { xclose(srv); return FALSE; }
    if (mode & PIPE_READMODE_MESSAGE) { DWORD m = PIPE_READMODE_MESSAGE; SetNamedPipeHandleState(*cli, &m, NULL, NULL); }
    return TRUE;
}
static BOOL xpipe(HANDLE *srv, HANDLE *cli) { return xpipe_ex(srv, cli, 4096, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE, TRUE, TRUE); }
/* Listening (unconnected) server instance; name returned. */
static HANDLE xlisten_srv(char *name, size_t cap, BOOL ov) {
    xpipename(name, cap);
    return CreateNamedPipeA(name, PIPE_ACCESS_DUPLEX | (ov ? FILE_FLAG_OVERLAPPED : 0), PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, NULL);
}
static HANDLE xclient(const char *name, BOOL ov) {
    return CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, ov ? FILE_FLAG_OVERLAPPED : 0, NULL);
}
/* Start a pending overlapped read; TRUE when FALSE/ERROR_IO_PENDING. */
static BOOL xread_pend(HANDLE h, void *b, DWORD n, OVERLAPPED *ov) {
    SetLastError(0); BOOL r = U(ReadFile)(h, b, n, NULL, ov); return !r && GetLastError() == ERROR_IO_PENDING;
}
/* Write a few bytes through a (possibly overlapped) handle and wait for it. */
static BOOL xwrite_all(HANDLE h, const void *b, DWORD n) {
    OVERLAPPED ov; HANDLE ev = xev(); xov(&ov, 0, ev); DWORD w = 0;
    BOOL r = WriteFile(h, b, n, &w, &ov);
    if (!r && GetLastError() == ERROR_IO_PENDING) r = GetOverlappedResultEx(h, &ov, &w, 5000, FALSE);
    CloseHandle(ev); return r && w == n;
}

/* ---- directories ---- */
static void xmkdir(char *out, const char *stem) { xpath(out, stem); CreateDirectoryA(out, NULL); }
static HANDLE xdiropen(const char *d, BOOL ov) {
    return CreateFileA(d, FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
                       FILE_FLAG_BACKUP_SEMANTICS | (ov ? FILE_FLAG_OVERLAPPED : 0), NULL);
}
static void xtouch(const char *dir, const char *leaf) {
    char p[MAX_PATH * 2]; snprintf(p, sizeof p, "%s\\%s", dir, leaf);
    HANDLE h = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
}
static void xrmtree1(const char *dir) {   /* one level */
    char pat[MAX_PATH * 2], sub[MAX_PATH * 2]; WIN32_FIND_DATAA fd; snprintf(pat, sizeof pat, "%s\\*", dir);
    HANDLE h = FindFirstFileA(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do { if (fd.cFileName[0] == '.') continue; snprintf(sub, sizeof sub, "%s\\%s", dir, fd.cFileName); DeleteFileA(sub); } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryA(dir);
}
#define FNI_BUF_DWORDS 1024
/* First record of a FILE_NOTIFY_INFORMATION buffer: action and narrow name. */
static DWORD xfni_first(const void *buf, char *name, size_t cap) {
    const FILE_NOTIFY_INFORMATION *f = buf; int n = (int)(f->FileNameLength / 2); if (n > (int)cap - 1) n = (int)cap - 1;
    for (int i = 0; i < n; i++) name[i] = (char)f->FileName[i];
    name[n] = 0; return f->Action;
}

/* ---- blocking workers ---- */
enum { XS_IDLE = 0, XS_ISSUING = 1, XS_DONE = 2 };
typedef struct xwork xwork;
typedef BOOL (*xop_fn)(xwork *w);
struct xwork {
    xop_fn op; HANDLE h; HANDLE h2; void *p; DWORD a, b; char name[160];
    volatile LONG state; BOOL r; DWORD err; DWORD n; DWORD tid; DWORD t_ms;
    BYTE buf[4096]; int leaked;
};
static DWORD WINAPI xwork_thread(void *p) {
    xwork *w = p; w->tid = GetCurrentThreadId();
    InterlockedExchange(&w->state, XS_ISSUING);
    DWORD t0 = GetTickCount();
    SetLastError(0); w->r = w->op(w); w->err = w->r ? 0 : GetLastError();
    w->t_ms = GetTickCount() - t0;
    InterlockedExchange(&w->state, XS_DONE);
    return 0;
}
static xwork *xwork_new(xop_fn op) { xwork *w = calloc(1, sizeof *w); w->op = op; w->h = w->h2 = INVALID_HANDLE_VALUE; return w; }
/* Frees the record unless a fallback left the worker thread running. */
static void xwork_free(xwork *w) { if (!w->leaked) free(w); }
static HANDLE xwork_start(xwork *w) { w->state = XS_IDLE; w->r = FALSE; w->err = 0xBEEF; return CreateThread(NULL, 0, xwork_thread, w, 0, NULL); }
/* Retry CancelSynchronousIo until it reports TRUE (the worker is blocked in the I/O) or the worker
 * ends or ms elapse. Returns the last result; *last gets the last error of a failed attempt. */
static BOOL xcancel_sync_retry(HANDLE t, xwork *w, DWORD ms, DWORD *last, int *tries) {
    DWORD t0 = GetTickCount(); BOOL got = FALSE; DWORD e = 0; int k = 0;
    while (!got && GetTickCount() - t0 < ms) {
        Sleep(k < 10 ? 2 : 10);
        if (w->state == XS_IDLE) continue;
        if (w->state == XS_DONE) break;
        SetLastError(0); got = U(CancelSynchronousIo)(t); e = got ? 0 : GetLastError(); k++;
    }
    if (last) *last = e;
    if (tries) *tries = k;
    return got;
}
/* Bounded join: wait ms; if the worker is still blocked call unblock(w) (natural completion), wait again,
 * then close w->h as the last resort. Returns TRUE when the worker ended within the first wait. */
static BOOL xwork_join(HANDLE t, xwork *w, DWORD ms, void (*unblock)(xwork *)) {
    if (WaitForSingleObject(t, ms) == WAIT_OBJECT_0) { CloseHandle(t); return TRUE; }
    if (unblock) unblock(w);
    if (WaitForSingleObject(t, 3000) != WAIT_OBJECT_0) {
        HANDLE h = w->h; w->h = INVALID_HANDLE_VALUE; if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h);
        if (WaitForSingleObject(t, 3000) != WAIT_OBJECT_0) { w->leaked = 1; t_note("worker still blocked after fallback (thread leaked)"); }
    }
    CloseHandle(t); return FALSE;
}
/* Worker is still inside its call after ms (blocked). */
static BOOL xstill_blocked(xwork *w, DWORD ms) { Sleep(ms); return w->state == XS_ISSUING; }

/* Watchdog: runs fire(ctx) every ms (at most 20 times) until stopped within ms (guards a call in the test thread that
 * would block if the object turned out not to be asynchronous). */
typedef struct { HANDLE stop, t; void (*fire)(void *); void *ctx; DWORD ms; volatile LONG fired; } xdog;
static DWORD WINAPI xdog_thread(void *p) {
    xdog *d = p;
    for (int i = 0; i < 20 && WaitForSingleObject(d->stop, d->ms) == WAIT_TIMEOUT; i++) { d->fired = 1; d->fire(d->ctx); }
    return 0;
}
static xdog *xdog_start(void (*fire)(void *), void *ctx, DWORD ms) {
    xdog *d = calloc(1, sizeof *d); d->stop = xev(); d->fire = fire; d->ctx = ctx; d->ms = ms;
    d->t = CreateThread(NULL, 0, xdog_thread, d, 0, NULL); return d;
}
/* Stops the watchdog; returns TRUE when it had to fire. */
static BOOL xdog_stop(xdog *d) {
    SetEvent(d->stop); WaitForSingleObject(d->t, 10000); CloseHandle(d->t); CloseHandle(d->stop);
    BOOL f = d->fired; free(d); return f;
}
#endif
