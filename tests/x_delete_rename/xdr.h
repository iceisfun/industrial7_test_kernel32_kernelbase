/* Shared static helpers for the x_delete_rename area: delete and rename semantics on NTFS scratch. */
#ifndef XDR_H
#define XDR_H
#include "t.h"
#include <stdlib.h>
#include <wchar.h>

#ifndef FILE_DISPOSITION_FLAG_DELETE
#define FILE_DISPOSITION_FLAG_DO_NOT_DELETE 0x0
#define FILE_DISPOSITION_FLAG_DELETE 0x1
#define FILE_DISPOSITION_FLAG_POSIX_SEMANTICS 0x2
#define FILE_DISPOSITION_FLAG_FORCE_IMAGE_SECTION_CHECK 0x4
#define FILE_DISPOSITION_FLAG_ON_CLOSE 0x8
#define FILE_DISPOSITION_FLAG_IGNORE_READONLY_ATTRIBUTE 0x10
#endif
#define XD_DEL   0x1
#define XD_POSIX 0x2
#define XD_IMG   0x4
#define XD_CLOSE 0x8
#define XD_IGNRO 0x10

#define XR_REPLACE 0x1
#define XR_POSIX   0x2
#define XR_SUPPRESS_PIN 0x4
#define XR_SUPPRESS_SR  0x8
#define XR_IGNRO   0x40

#define XC_STANDARD 1
#define XC_RENAME 3
#define XC_DISP 4
#define XC_DISPEX 21
#define XC_RENAMEEX 22

#define XSHARE_ALL (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE)
#define XSHARE_RW  (FILE_SHARE_READ | FILE_SHARE_WRITE)
#define XP(var, leaf) WCHAR var[MAX_PATH * 2]; xp(var, leaf)

/* Single-evaluation checks (operands often contain calls). */
#undef EXPECT_EQ
#define EXPECT_EQ(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)a_, (unsigned long long)b_); } while (0)
/* call must return FALSE with the given last error */
#define XFAIL(call, err) do { SetLastError(0xBEEF); BOOL b_ = (call); DWORD e_ = GetLastError(); \
    t_check(!b_ && e_ == (DWORD)(err), __FILE__, __LINE__, #call, "ret %d GLE %lu want FALSE/%lu", b_, b_ ? 0 : e_, (DWORD)(err)); } while (0)
/* call must succeed */
#define XOK(call) do { SetLastError(0xBEEF); BOOL b_ = (call); DWORD e_ = GetLastError(); \
    t_check(b_ != 0, __FILE__, __LINE__, #call, "failed GLE %lu", e_); } while (0)
#define XOKH(h) t_check((h) != INVALID_HANDLE_VALUE, __FILE__, __LINE__, #h " valid", "GLE %lu", GetLastError())
#define XCLOSE(h) do { if ((h) != INVALID_HANDLE_VALUE && (h) != NULL) CloseHandle(h); (h) = INVALID_HANDLE_VALUE; } while (0)

static int xdr_ntfs(void) {
    static int cached = -1;
    if (cached < 0) {
        char root[MAX_PATH], fs[64] = "";
        if (!GetVolumePathNameA(t_tmpdir(), root, sizeof root)) strcpy(root, "C:\\");
        if (!GetVolumeInformationA(root, NULL, 0, NULL, NULL, NULL, fs, sizeof fs)) fs[0] = 0;
        cached = !_stricmp(fs, "NTFS") && !t_is_wine();
    }
    return cached;
}
#define XDR_NEED_NTFS() do { if (!xdr_ntfs()) { t_skip("scratch volume is not NTFS"); return; } } while (0)

/* ---- scaffolding (harness-linked calls, not under test) ---- */
static void xp(WCHAR *out, const char *leaf) {
    char a[MAX_PATH * 2];
    t_tmpfile(a, sizeof a, leaf);
    MultiByteToWideChar(CP_ACP, 0, a, -1, out, MAX_PATH * 2);
}
static void xpa(char *out, const char *leaf) { t_tmpfile(out, MAX_PATH * 2, leaf); }
static int xmk(const WCHAR *p, const char *data) {
    HANDLE h = CreateFileW(p, GENERIC_WRITE, XSHARE_ALL, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD n = 0, len = (DWORD)strlen(data);
    if (h == INVALID_HANDLE_VALUE) return 0;
    if (len) WriteFile(h, data, len, &n, NULL);
    CloseHandle(h);
    return n == len;
}
static int xmkdir(const WCHAR *p) { return CreateDirectoryW(p, NULL) != 0; }
static int xexists(const WCHAR *p) { return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES; }
static void xrm(const WCHAR *p) {
    DWORD a = GetFileAttributesW(p);
    if (a == INVALID_FILE_ATTRIBUTES) return;
    SetFileAttributesW(p, FILE_ATTRIBUTE_NORMAL);
    if (a & FILE_ATTRIBUTE_DIRECTORY) RemoveDirectoryW(p); else DeleteFileW(p);
}
static int xread_all(const WCHAR *p, char *buf, DWORD cap, DWORD *got) {
    HANDLE h = CreateFileW(p, GENERIC_READ, XSHARE_ALL, NULL, OPEN_EXISTING, 0, NULL);
    DWORD n = 0;
    *got = 0;
    if (h == INVALID_HANDLE_VALUE) return 0;
    ReadFile(h, buf, cap, &n, NULL);
    CloseHandle(h);
    *got = n;
    return 1;
}
/* file at p holds exactly want */
static int xcontent(const WCHAR *p, const char *want) {
    char b[256]; DWORD n = 0;
    if (!xread_all(p, b, sizeof b, &n)) return 0;
    return n == strlen(want) && !memcmp(b, want, n);
}
/* read from the start through an existing handle */
static int xhcontent(HANDLE h, const char *want) {
    char b[256]; DWORD n = 0; LARGE_INTEGER z; z.QuadPart = 0;
    if (!SetFilePointerEx(h, z, NULL, FILE_BEGIN)) return 0;
    if (!ReadFile(h, b, sizeof b, &n, NULL)) return 0;
    return n == strlen(want) && !memcmp(b, want, n);
}

/* ---- wrappers through the pointers under test ---- */
static HANDLE xopen(const WCHAR *p, DWORD acc, DWORD sh, DWORD disp, DWORD fl) {
    return U(CreateFileW)(p, acc, sh, NULL, disp, fl, NULL);
}
static HANDLE xopendir(const WCHAR *p, DWORD acc, DWORD sh) {
    return U(CreateFileW)(p, acc, sh, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
}
static BOOL xsi(HANDLE h, DWORD cls, void *b, DWORD n) {
    return U(SetFileInformationByHandle)(h, (FILE_INFO_BY_HANDLE_CLASS)cls, b, n);
}
static BOOL xgi(HANDLE h, DWORD cls, void *b, DWORD n) {
    return U(GetFileInformationByHandleEx)(h, (FILE_INFO_BY_HANDLE_CLASS)cls, b, n);
}
static BOOL xdisp(HANDLE h, DWORD flags) {
    FILE_DISPOSITION_INFO_EX d; d.Flags = flags;
    return xsi(h, XC_DISPEX, &d, sizeof d);
}
static BOOL xdisp_legacy(HANDLE h, BYTE v) {
    FILE_DISPOSITION_INFO d; memset(&d, 0, sizeof d); *(BYTE *)&d.DeleteFile = v;
    return xsi(h, XC_DISP, &d, sizeof d);
}
/* FILE_RENAME_INFO with Flags (or ReplaceIfExists as the low byte for the legacy class), optional root */
static BOOL xren(HANDLE h, DWORD cls, const WCHAR *dst, DWORD flags, HANDLE root) {
    DWORD bytes = (DWORD)wcslen(dst) * sizeof(WCHAR);
    DWORD sz = (DWORD)offsetof(FILE_RENAME_INFO, FileName) + bytes + sizeof(WCHAR);
    FILE_RENAME_INFO *ri = (FILE_RENAME_INFO *)calloc(1, sz);
    BOOL r; DWORD e;
    ri->Flags = flags;
    ri->RootDirectory = root;
    ri->FileNameLength = bytes;
    memcpy(ri->FileName, dst, bytes);
    r = xsi(h, cls, ri, sz);
    e = GetLastError(); free(ri); SetLastError(e);
    return r;
}
static int xstd(HANDLE h, FILE_STANDARD_INFO *st) {
    memset(st, 0xCC, sizeof *st);
    return xgi(h, XC_STANDARD, st, sizeof *st) != 0;
}
/* GetFileAttributesW result plus its error */
static DWORD xattr_err(const WCHAR *p) {
    SetLastError(0);
    DWORD a = U(GetFileAttributesW)(p);
    return a == INVALID_FILE_ATTRIBUTES ? GetLastError() : 0;
}
/* how many entries of dir (no trailing backslash) carry this leaf name */
static int xlisted(const WCHAR *dir, const WCHAR *leaf) {
    WCHAR pat[MAX_PATH * 2]; WIN32_FIND_DATAW fd; int n = 0;
    swprintf(pat, MAX_PATH * 2, L"%ls\\*", dir);
    HANDLE f = U(FindFirstFileW)(pat, &fd);
    if (f == INVALID_HANDLE_VALUE) return -1;
    do { if (!_wcsicmp(fd.cFileName, leaf)) n++; } while (FindNextFileW(f, &fd));
    FindClose(f);
    return n;
}
/* scratch dir without the trailing backslash */
static void xroot(WCHAR *out) {
    xp(out, "");
    size_t n = wcslen(out);
    if (n && out[n - 1] == L'\\') out[n - 1] = 0;
}
static DWORD xlinks_by_name(const WCHAR *p) {
    BY_HANDLE_FILE_INFORMATION bi;
    HANDLE h = CreateFileW(p, FILE_READ_ATTRIBUTES, XSHARE_ALL, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    DWORD n = 0xFFFFFFFF;
    if (h == INVALID_HANDLE_VALUE) return n;
    if (GetFileInformationByHandle(h, &bi)) n = bi.nNumberOfLinks;
    CloseHandle(h);
    return n;
}
#endif
