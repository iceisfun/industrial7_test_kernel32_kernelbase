/* Shared helpers for the file_io area (all static). */
#ifndef FIO_H
#define FIO_H
#include "t.h"
#include <stdlib.h>

/* Single-evaluation versions (the harness macros evaluate their arguments several times). */
#undef EXPECT_EQ
#define EXPECT_EQ(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)a_, (unsigned long long)b_); } while (0)
#undef EXPECT_ERR
#define EXPECT_ERR(err) do { DWORD e_ = GetLastError(); \
    t_check(e_ == (DWORD)(err), __FILE__, __LINE__, #err, "GetLastError got %lu want %lu", e_, (DWORD)(err)); } while (0)
#define TP(var, leaf) char var[MAX_PATH * 2]; t_tmpfile(var, sizeof var, leaf)
#define NA ((DWORD)-1)

static WCHAR *fio_w(const char *s) {
    static WCHAR ring[4][1200]; static int idx;
    WCHAR *o = ring[idx++ & 3];
    MultiByteToWideChar(CP_ACP, 0, s, -1, o, 1200);
    return o;
}

/* Typed wrappers going through the pointers under test. */
static HANDLE cfa(const char *p, DWORD acc, DWORD sh, DWORD disp, DWORD fl) {
    return U(CreateFileA)(p, acc, sh, NULL, disp, fl, NULL);
}
static HANDLE cfw(const WCHAR *p, DWORD acc, DWORD sh, DWORD disp, DWORD fl) {
    return U(CreateFileW)(p, acc, sh, NULL, disp, fl, NULL);
}

/* Scaffolding with direct (harness-linked) APIs. */
static int mkfile(const char *path, const void *data, DWORD len) {
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD n = 0;
    if (h == INVALID_HANDLE_VALUE) return 0;
    if (len) WriteFile(h, data, len, &n, NULL);
    CloseHandle(h);
    return n == len;
}
static int mkfile_pat(const char *path, DWORD len) {
    char *b = (char *)malloc(len ? len : 1); DWORD i; int r;
    for (i = 0; i < len; i++) b[i] = (char)('A' + (i % 26));
    r = mkfile(path, b, len); free(b); return r;
}
static void rmfile(const char *path) {
    SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
    DeleteFileA(path);
}
static int exists(const char *path) { return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES; }
static LONGLONG fsize_of(const char *path) {
    WIN32_FILE_ATTRIBUTE_DATA d;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &d)) return -1;
    return ((LONGLONG)d.nFileSizeHigh << 32) | d.nFileSizeLow;
}
static LONGLONG fpos(HANDLE h) {
    LARGE_INTEGER z, r; z.QuadPart = 0; r.QuadPart = -1;
    if (!SetFilePointerEx(h, z, &r, FILE_CURRENT)) return -1;
    return r.QuadPart;
}
static LONGLONG hsize(HANDLE h) { LARGE_INTEGER s; if (!GetFileSizeEx(h, &s)) return -1; return s.QuadPart; }
static LONGLONG hsetpos(HANDLE h, LONGLONG p) {
    LARGE_INTEGER z, r; z.QuadPart = p; r.QuadPart = -1;
    if (!SetFilePointerEx(h, z, &r, FILE_BEGIN)) return -1;
    return r.QuadPart;
}

#define EXPECT_FAIL_H(call, err) do { SET_ERR(0xBEEF); HANDLE h_ = (call); DWORD e_ = GetLastError(); \
    t_check(h_ == INVALID_HANDLE_VALUE, __FILE__, __LINE__, #call, "expected INVALID_HANDLE_VALUE got %p", h_); \
    t_check(e_ == (DWORD)(err), __FILE__, __LINE__, #call " last-error", "GLE got %lu want %lu", e_, (DWORD)(err)); \
    if (h_ != INVALID_HANDLE_VALUE) CloseHandle(h_); } while (0)
#define EXPECT_FAIL_B(call, err) do { SET_ERR(0xBEEF); BOOL b_ = (call); DWORD e_ = GetLastError(); \
    t_check(b_ == 0, __FILE__, __LINE__, #call, "expected FALSE"); \
    t_check(e_ == (DWORD)(err), __FILE__, __LINE__, #call " last-error", "GLE got %lu want %lu", e_, (DWORD)(err)); } while (0)
#define EXPECT_FAIL_DW(call, ret, err) do { SET_ERR(0xBEEF); DWORD r_ = (call); DWORD e_ = GetLastError(); \
    t_check(r_ == (DWORD)(ret), __FILE__, __LINE__, #call, "ret got 0x%lx want 0x%lx", r_, (DWORD)(ret)); \
    t_check(e_ == (DWORD)(err), __FILE__, __LINE__, #call " last-error", "GLE got %lu want %lu", e_, (DWORD)(err)); } while (0)
#define OK_H(h) t_check((h) != INVALID_HANDLE_VALUE, __FILE__, __LINE__, #h " valid", "GLE=%lu", GetLastError())
#define CLOSE(h) do { if ((h) != INVALID_HANDLE_VALUE && (h) != NULL) CloseHandle(h); (h) = INVALID_HANDLE_VALUE; } while (0)

/* NtQueryInformationFile helpers: FileAccessInformation=8, FileModeInformation=16 */
static DWORD fio_access_mask(HANDLE h) {
    IO_STATUS_BLOCK iosb; ULONG v = 0xFFFFFFFF; NTSTATUS st;
    typedef NTSTATUS (NTAPI *pfn)(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, ULONG);
    pfn f = (pfn)t_proc_nt("NtQueryInformationFile");
    if (!f) return 0xFFFFFFFF;
    st = f(h, &iosb, &v, sizeof v, 8);
    return st >= 0 ? v : 0xFFFFFFFF;
}
static DWORD fio_mode(HANDLE h) {
    IO_STATUS_BLOCK iosb; ULONG v = 0xFFFFFFFF; NTSTATUS st;
    typedef NTSTATUS (NTAPI *pfn)(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, ULONG);
    pfn f = (pfn)t_proc_nt("NtQueryInformationFile");
    if (!f) return 0xFFFFFFFF;
    st = f(h, &iosb, &v, sizeof v, 16);
    return st >= 0 ? v : 0xFFFFFFFF;
}
#endif
