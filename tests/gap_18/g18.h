#ifndef G18_H
#define G18_H
#include "t.h"
/* gap18 shared helpers (scaffolding only; code under test is reached via U()). */
static int g18_put(const char *path, const void *data, DWORD n) {
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    DWORD w = 0; BOOL ok = n ? WriteFile(h, data, n, &w, NULL) : TRUE;
    CloseHandle(h);
    return ok && w == n;
}
static int g18_exists(const char *p) { return GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES; }
/* returns size or -1 */
static long long g18_size(const char *p) {
    WIN32_FILE_ATTRIBUTE_DATA d;
    if (!GetFileAttributesExA(p, GetFileExInfoStandard, &d)) return -1;
    return ((long long)d.nFileSizeHigh << 32) | d.nFileSizeLow;
}
static int g18_getn(const char *path, void *buf, DWORD cap, DWORD *got) {
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    DWORD r = 0; BOOL ok = ReadFile(h, buf, cap, &r, NULL);
    CloseHandle(h); if (got) *got = r; return ok;
}
static int g18_content_is(const char *path, const char *s) {
    char b[256]; DWORD n = 0; size_t l = strlen(s);
    if (l >= sizeof b) return 0;
    if (!g18_getn(path, b, sizeof b, &n)) return 0;
    return n == l && !memcmp(b, s, l);
}
static void g18_rm(const char *p) { SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL); DeleteFileA(p); }
static void g18_rmdir(const char *p) { SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL); RemoveDirectoryA(p); }
static void g18_path(char *out, size_t cap, const char *leaf) { t_tmpfile(out, cap, leaf); }
static void g18_widen(WCHAR *w, int cap, const char *a) { MultiByteToWideChar(CP_ACP, 0, a, -1, w, cap); }
static DWORD g18_wait_exit(HANDLE h, DWORD ms) {
    DWORD code = 0xDEAD0001;
    if (WaitForSingleObject(h, ms) != WAIT_OBJECT_0) { TerminateProcess(h, 1); WaitForSingleObject(h, 5000); return code; }
    GetExitCodeProcess(h, &code); return code;
}
/* single-evaluation equality check (EXPECT_EQ evaluates its arguments twice) */
#define EQ(a, b) do { unsigned long long a_ = (unsigned long long)(ULONG_PTR)(a), b_ = (unsigned long long)(ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", a_, b_); } while (0)
#define CK(c) do { if (!(c)) return __LINE__; } while (0)
#endif
