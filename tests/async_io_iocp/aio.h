/* Shared helpers for the async_io_iocp area (all static). */
#ifndef AIO_H
#define AIO_H
#include "t.h"
#include <stdlib.h>
#include <winioctl.h>

typedef struct { DWORD NamedPipeState; DWORD ReadDataAvailable; DWORD NumberOfMessages; DWORD MessageLength; CHAR Data[1]; } FILE_PIPE_PEEK_BUFFER_;
#define NTS_PENDING   0x103u
#define NTS_CANCELLED 0xC0000120u
#define NTS_EOF       0xC0000011u
#define NTS_BROKEN    0xC000014Bu

static BYTE pat(size_t i) { return (BYTE)((i * 131u + (i >> 8) * 17u + 5u) & 0xff); }
static void fillp(void *b, size_t off, size_t n) { BYTE *p = b; for (size_t i = 0; i < n; i++) p[i] = pat(off + i); }
static int chkp(const void *b, size_t off, size_t n) { const BYTE *p = b; for (size_t i = 0; i < n; i++) if (p[i] != pat(off + i)) return 0; return 1; }

static int g_seq_;
static void mkpath(char *out, const char *stem) {
    char leaf[96]; snprintf(leaf, sizeof leaf, "%s_%s_%d.bin", AIO_TU, stem, ++g_seq_); t_tmpfile(out, MAX_PATH, leaf);
}
/* Create (sync) a file filled with the pattern. */
static BOOL make_file(const char *path, size_t size) {
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    size_t done = 0; BYTE *b = malloc(65536); BOOL ok = TRUE;
    while (done < size && ok) {
        size_t c = size - done < 65536 ? size - done : 65536; DWORD w;
        fillp(b, done, c); ok = WriteFile(h, b, (DWORD)c, &w, NULL) && w == c; done += c;
    }
    free(b); CloseHandle(h); return ok;
}
static HANDLE open_ov(const char *path, DWORD access, DWORD extra) {
    return CreateFileA(path, access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | extra, NULL);
}
static HANDLE open_sync(const char *path, DWORD access) {
    return CreateFileA(path, access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
}
static HANDLE new_ov_file(const char *path, DWORD extra) {
    return CreateFileA(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, CREATE_ALWAYS, FILE_FLAG_OVERLAPPED | extra, NULL);
}
static HANDLE mkevent(void) { return CreateEventA(NULL, TRUE, FALSE, NULL); }
static void ovinit(OVERLAPPED *ov, ULONGLONG off, HANDLE ev) {
    memset(ov, 0, sizeof *ov); ov->Offset = (DWORD)off; ov->OffsetHigh = (DWORD)(off >> 32); ov->hEvent = ev;
}
static int signaled(HANDLE h) { return WaitForSingleObject(h, 0) == WAIT_OBJECT_0; }
static int not_signaled(HANDLE h) { return WaitForSingleObject(h, 0) == WAIT_TIMEOUT; }

/* result of an operation driven to completion */
typedef struct { BOOL r; DWORD e0; BOOL ok; DWORD err; DWORD n; } iores;
static iores io_fin(HANDLE h, OVERLAPPED *ov, BOOL r, DWORD e0) {
    iores x; memset(&x, 0, sizeof x); x.r = r; x.e0 = e0;
    if (!r && e0 != ERROR_IO_PENDING) { x.err = e0; return x; }
    x.ok = GetOverlappedResultEx(h, ov, &x.n, 10000, FALSE);
    x.err = x.ok ? 0 : GetLastError();
    return x;
}
static iores do_read(HANDLE h, void *b, DWORD n, OVERLAPPED *ov) {
    SetLastError(0); BOOL r = U(ReadFile)(h, b, n, NULL, ov); DWORD e = r ? 0 : GetLastError(); return io_fin(h, ov, r, e);
}
static iores do_write(HANDLE h, const void *b, DWORD n, OVERLAPPED *ov) {
    SetLastError(0); BOOL r = U(WriteFile)(h, b, n, NULL, ov); DWORD e = r ? 0 : GetLastError(); return io_fin(h, ov, r, e);
}

static int g_pseq_;
static BOOL make_pipe_sz(HANDLE *srv, HANDLE *cli, DWORD bufsz) {
    char name[160]; snprintf(name, sizeof name, "\\\\.\\pipe\\k32t_aio_%s_%lu_%d", AIO_TU, GetCurrentProcessId(), ++g_pseq_);
    *srv = CreateNamedPipeA(name, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, bufsz, bufsz, 0, NULL);
    if (*srv == INVALID_HANDLE_VALUE) return FALSE;
    *cli = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (*cli == INVALID_HANDLE_VALUE) { CloseHandle(*srv); *srv = INVALID_HANDLE_VALUE; return FALSE; }
    OVERLAPPED ov; HANDLE ev = mkevent(); ovinit(&ov, 0, ev);
    BOOL r = ConnectNamedPipe(*srv, &ov);
    if (!r && GetLastError() == ERROR_IO_PENDING) WaitForSingleObject(ev, 5000);
    CloseHandle(ev);
    return TRUE;
}
static BOOL make_pipe(HANDLE *srv, HANDLE *cli) { return make_pipe_sz(srv, cli, 65536); }
static void closeh(HANDLE *h) { if (*h && *h != INVALID_HANDLE_VALUE) CloseHandle(*h); *h = INVALID_HANDLE_VALUE; }

/* thread that writes to a handle after a delay (sync-ish, via its own OVERLAPPED) */
typedef struct { HANDLE h; DWORD delay; const void *data; DWORD len; } delaywr;
static DWORD WINAPI delaywr_thread(void *p) {
    delaywr *d = p; Sleep(d->delay);
    OVERLAPPED ov; HANDLE ev = mkevent(); ovinit(&ov, 0, ev); DWORD n;
    BOOL r = WriteFile(d->h, d->data, d->len, &n, &ov);
    if (!r && GetLastError() == ERROR_IO_PENDING) GetOverlappedResultEx(d->h, &ov, &n, 5000, FALSE);
    CloseHandle(ev); return 0;
}
#endif
