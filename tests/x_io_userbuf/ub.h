/* Shared helpers for the x_io_userbuf area (all static).
 * Invalid or protected user buffers and output pointers handed to file, pipe,
 * directory and completion-port APIs. All bad memory comes from the test's own
 * VirtualAlloc (PAGE_NOACCESS, PAGE_READONLY, MEM_RESERVE, PAGE_GUARD).
 * Calls that may raise a user-mode exception run in a child process whose
 * unhandled-exception filter exits with the exception code. */
#ifndef UB_H
#define UB_H
#include "t.h"
#include <stdlib.h>
#include <winioctl.h>

/* Single-evaluation versions (the harness macros evaluate their arguments several times). */
#undef EXPECT_EQ
#define EXPECT_EQ(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)a_, (unsigned long long)b_); } while (0)

#define PG 4096u
#define NTS_PENDING 0x103u
/* A kernel-mode probe that hits a guard page fails with STATUS_GUARD_PAGE_VIOLATION;
 * Windows 10 passes this warning status through unmapped as the last-error value. */
#define GLE_GUARD 0x80000001u

/* ---- memory (scaffolding, direct calls) ---- */
static BYTE *ub_va(SIZE_T n, DWORD prot) { return (BYTE *)VirtualAlloc(NULL, n, MEM_RESERVE | MEM_COMMIT, prot); }
static BYTE *ub_reserve(SIZE_T n) { return (BYTE *)VirtualAlloc(NULL, n, MEM_RESERVE, PAGE_NOACCESS); }
static void ub_free(void *p) { if (p) VirtualFree(p, 0, MEM_RELEASE); }
static void ub_protect(void *p, SIZE_T n, DWORD prot) { DWORD o; VirtualProtect(p, n, prot, &o); }
/* n pages committed read-write, filled with fill, then protected with prot. */
static BYTE *ub_pages(unsigned n, DWORD prot, BYTE fill) {
    BYTE *b = ub_va((SIZE_T)n * PG, PAGE_READWRITE);
    if (!b) return NULL;
    memset(b, fill, (SIZE_T)n * PG);
    if (prot != PAGE_READWRITE) ub_protect(b, (SIZE_T)n * PG, prot);
    return b;
}
/* Two pages: the first read-write (filled with fill), the second with prot2.
 * prot2 == 0 means the second page is reserved, not committed. */
static BYTE *ub_span(DWORD prot2, BYTE fill) {
    BYTE *b;
    if (prot2 == 0) {
        b = ub_reserve(2 * PG);
        if (!b || !VirtualAlloc(b, PG, MEM_COMMIT, PAGE_READWRITE)) return NULL;
        memset(b, fill, PG);
        return b;
    }
    b = ub_va(2 * PG, PAGE_READWRITE);
    if (!b) return NULL;
    memset(b, fill, 2 * PG);
    ub_protect(b + PG, PG, prot2);
    return b;
}
static DWORD ub_prot(const void *p) { MEMORY_BASIC_INFORMATION m; memset(&m, 0, sizeof m); VirtualQuery(p, &m, sizeof m); return m.Protect; }
static DWORD ub_state(const void *p) { MEMORY_BASIC_INFORMATION m; memset(&m, 0, sizeof m); VirtualQuery(p, &m, sizeof m); return m.State; }
static int ub_all(const BYTE *p, SIZE_T n, BYTE v) { for (SIZE_T i = 0; i < n; i++) if (p[i] != v) return 0; return 1; }

/* ---- data pattern ---- */
static BYTE ub_pat(size_t i) { return (BYTE)((i * 131u + (i >> 8) * 17u + 5u) & 0xff); }
static void ub_fill(void *b, size_t off, size_t n) { BYTE *p = b; for (size_t i = 0; i < n; i++) p[i] = ub_pat(off + i); }
static int ub_chk(const void *b, size_t off, size_t n) { const BYTE *p = b; for (size_t i = 0; i < n; i++) if (p[i] != ub_pat(off + i)) return 0; return 1; }

/* ---- files (scaffolding, direct calls) ---- */
static int g_ub_seq;
static void ub_path(char *out, const char *stem) {
    char leaf[96]; snprintf(leaf, sizeof leaf, "xubuf_%s_%s_%d.bin", UB_TU, stem, ++g_ub_seq); t_tmpfile(out, MAX_PATH, leaf);
}
static BOOL ub_mkfile(const char *path, DWORD size) {
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    BYTE *b; DWORD w = 0; BOOL ok = TRUE;
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    if (size) {
        b = (BYTE *)malloc(size); ub_fill(b, 0, size);
        ok = WriteFile(h, b, size, &w, NULL) && w == size;
        free(b);
    }
    CloseHandle(h); return ok;
}
static HANDLE ub_open(const char *path, DWORD access, DWORD flags) {
    return CreateFileA(path, access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, flags, NULL);
}
static void ub_rm(const char *path) { SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL); DeleteFileA(path); }
static LONGLONG ub_pos(HANDLE h) { LARGE_INTEGER z, r; z.QuadPart = 0; r.QuadPart = -1; if (!SetFilePointerEx(h, z, &r, FILE_CURRENT)) return -1; return r.QuadPart; }
static void ub_seek(HANDLE h, LONGLONG p) { LARGE_INTEGER z; z.QuadPart = p; SetFilePointerEx(h, z, NULL, FILE_BEGIN); }
static LONGLONG ub_size(HANDLE h) { LARGE_INTEGER s; if (!GetFileSizeEx(h, &s)) return -1; return s.QuadPart; }
/* Content of the file at offset off, read through a fresh handle, compared with the pattern. */
static int ub_file_pat(const char *path, DWORD off, DWORD n) {
    HANDLE h = ub_open(path, GENERIC_READ, 0); BYTE *b; DWORD r = 0; int ok;
    if (h == INVALID_HANDLE_VALUE) return 0;
    b = (BYTE *)malloc(n ? n : 1); ub_seek(h, off);
    ok = ReadFile(h, b, n, &r, NULL) && r == n && ub_chk(b, off, n);
    free(b); CloseHandle(h); return ok;
}
static void closeh(HANDLE *h) { if (*h && *h != INVALID_HANDLE_VALUE) CloseHandle(*h); *h = INVALID_HANDLE_VALUE; }
static HANDLE ub_event(BOOL signaled) { return CreateEventA(NULL, TRUE, signaled, NULL); }
static int ub_signaled(HANDLE h) { return WaitForSingleObject(h, 0) == WAIT_OBJECT_0; }
static void ub_ov(OVERLAPPED *ov, ULONGLONG off, HANDLE ev) {
    memset(ov, 0, sizeof *ov); ov->Offset = (DWORD)off; ov->OffsetHigh = (DWORD)(off >> 32); ov->hEvent = ev;
}

/* ---- pipes (scaffolding, direct calls) ---- */
static int g_ub_pseq;
static void ub_pipename(char *out, size_t cap) {
    snprintf(out, cap, "\\\\.\\pipe\\k32t_xubuf_%s_%lu_%d", UB_TU, GetCurrentProcessId(), ++g_ub_pseq);
}
/* Connected named pipe pair. msg: message type and read mode on both ends.
 * ovl: both ends opened for overlapped I/O. */
static BOOL ub_npipe(HANDLE *srv, HANDLE *cli, BOOL msg, BOOL ovl, char *name_out) {
    char name[160]; OVERLAPPED ov; HANDLE ev; BOOL r;
    DWORD mode = msg ? (PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE) : (PIPE_TYPE_BYTE | PIPE_READMODE_BYTE);
    ub_pipename(name, sizeof name);
    if (name_out) strcpy(name_out, name);
    *srv = CreateNamedPipeA(name, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, mode | PIPE_WAIT, 1, 65536, 65536, 0, NULL);
    *cli = INVALID_HANDLE_VALUE;
    if (*srv == INVALID_HANDLE_VALUE) return FALSE;
    ev = ub_event(FALSE); ub_ov(&ov, 0, ev);
    r = ConnectNamedPipe(*srv, &ov);
    if (!r && GetLastError() != ERROR_IO_PENDING && GetLastError() != ERROR_PIPE_CONNECTED) { CloseHandle(ev); closeh(srv); return FALSE; }
    *cli = CreateFileA(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, ovl ? FILE_FLAG_OVERLAPPED : 0, NULL);
    if (*cli == INVALID_HANDLE_VALUE) { CancelIo(*srv); CloseHandle(ev); closeh(srv); return FALSE; }
    WaitForSingleObject(ev, 5000);
    CloseHandle(ev);
    if (msg) { DWORD m = PIPE_READMODE_MESSAGE; SetNamedPipeHandleState(*cli, &m, NULL, NULL); }
    return TRUE;
}
/* Synchronous-looking write/read on a possibly overlapped handle (bounded). */
static BOOL ub_pwrite(HANDLE h, const void *b, DWORD n) {
    OVERLAPPED ov; HANDLE ev = ub_event(FALSE); DWORD w = 0; BOOL r;
    ub_ov(&ov, 0, ev);
    r = WriteFile(h, b, n, &w, &ov);
    if (!r && GetLastError() == ERROR_IO_PENDING) r = GetOverlappedResultEx(h, &ov, &w, 5000, FALSE);
    if (!r) { CancelIoEx(h, &ov); GetOverlappedResultEx(h, &ov, &w, 5000, FALSE); }
    CloseHandle(ev);
    return r && w == n;
}
/* Returns bytes read, (DWORD)-1 on failure; *err gets the error. */
static DWORD ub_pread(HANDLE h, void *b, DWORD n, DWORD *err) {
    OVERLAPPED ov; HANDLE ev = ub_event(FALSE); DWORD got = 0; BOOL r;
    ub_ov(&ov, 0, ev);
    r = ReadFile(h, b, n, &got, &ov);
    if (!r && GetLastError() == ERROR_IO_PENDING) {
        r = GetOverlappedResultEx(h, &ov, &got, 2000, FALSE);
        if (!r && GetLastError() == WAIT_TIMEOUT) {
            CancelIoEx(h, &ov); GetOverlappedResultEx(h, &ov, &got, 5000, FALSE);
            CloseHandle(ev); if (err) *err = WAIT_TIMEOUT; return (DWORD)-1;
        }
    }
    if (err) *err = r ? 0 : GetLastError();
    CloseHandle(ev);
    if (!r && err && *err == ERROR_MORE_DATA) return got;
    return r ? got : (DWORD)-1;
}
/* Bytes available on a pipe (direct PeekNamedPipe into valid memory), -1 on failure. */
static LONG ub_avail(HANDLE h) { DWORD a = 0; if (!PeekNamedPipe(h, NULL, 0, NULL, &a, NULL)) return -1; return (LONG)a; }
static LONG ub_msgleft(HANDLE h) { DWORD a = 0, l = 0; if (!PeekNamedPipe(h, NULL, 0, NULL, &a, &l)) return -1; return (LONG)l; }

/* ---- child processes ---- */
/* Exit codes of the child bodies: an API that returned success, an API that
 * failed with a last-error value, or the code of an unhandled exception. */
#define R_OK(x)  (0x10000000u | ((DWORD)(x) & 0xFFFFFFu))
#define R_ERR(e) (0x20000000u | ((DWORD)(e) & 0xFFFFFFu))
#define R_SETUP  0x30000000u
#define R_BOOL(r) ((r) ? R_OK(0) : R_ERR(GetLastError()))
static LONG WINAPI ub_uef(EXCEPTION_POINTERS *ep) {
    TerminateProcess(GetCurrentProcess(), ep->ExceptionRecord->ExceptionCode);
    return EXCEPTION_EXECUTE_HANDLER;
}
static void ub_child_init(void) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    SetUnhandledExceptionFilter(ub_uef);
}
/* Run child <name> with one quoted argument (a path in the parent's scratch dir or ""). */
static DWORD ub_child(const char *name, const char *arg) {
    char a[MAX_PATH * 2 + 8];
    snprintf(a, sizeof a, "\"%s\"", arg ? arg : "");
    return t_run_child(name, a, 20000);
}
#define EXC_AV 0xC0000005u
#endif
