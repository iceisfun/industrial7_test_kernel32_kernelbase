/* Shared static helpers for the file_attrs_ops area. */
#ifndef FA_H
#define FA_H
#include "t.h"
#include <winioctl.h>
#include <stdlib.h>

#ifndef FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS
#define FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS 0x00400000
#endif


/* ---- scaffolding (plain harness-side calls, not under test) ---- */
static void fa_path(char *out, const char *leaf) { t_tmpfile(out, MAX_PATH * 2, leaf); }

static void fa_wpath(WCHAR *out, const char *leaf) {
    char a[MAX_PATH * 2];
    fa_path(a, leaf);
    MultiByteToWideChar(CP_ACP, 0, a, -1, out, MAX_PATH * 2);
}
static void fa_widen(WCHAR *out, const char *a) { MultiByteToWideChar(CP_ACP, 0, a, -1, out, MAX_PATH * 2); }

static int fa_mkfile(const char *path, const void *data, DWORD len) {
    HANDLE h = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD w = 0;
    if (h == INVALID_HANDLE_VALUE) return 0;
    if (len) WriteFile(h, data, len, &w, NULL);
    CloseHandle(h);
    return w == len;
}
static int fa_mkfile_s(const char *leaf, const char *data) {
    char p[MAX_PATH * 2]; fa_path(p, leaf);
    return fa_mkfile(p, data, (DWORD)strlen(data));
}
static int fa_mkdir(const char *leaf) { char p[MAX_PATH * 2]; fa_path(p, leaf); return CreateDirectoryA(p, NULL) != 0; }
static DWORD fa_attr(const char *path) { return GetFileAttributesA(path); }
static int fa_exists(const char *path) { return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES; }
static void fa_rm(const char *path) {
    DWORD a = GetFileAttributesA(path);
    if (a == INVALID_FILE_ATTRIBUTES) return;
    SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
    if (a & FILE_ATTRIBUTE_DIRECTORY) RemoveDirectoryA(path); else DeleteFileA(path);
}
static void fa_rml(const char *leaf) { char p[MAX_PATH * 2]; fa_path(p, leaf); fa_rm(p); }
static int fa_read(const char *path, char *buf, DWORD cap, DWORD *got) {
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    DWORD n = 0;
    if (h == INVALID_HANDLE_VALUE) return 0;
    ReadFile(h, buf, cap, &n, NULL);
    CloseHandle(h);
    if (got) *got = n;
    return 1;
}
static int fa_content_is(const char *path, const char *want) {
    char b[512]; DWORD n = 0;
    if (!fa_read(path, b, sizeof b - 1, &n)) return 0;
    return n == strlen(want) && !memcmp(b, want, n);
}
static int fa_content_isl(const char *leaf, const char *want) { char p[MAX_PATH * 2]; fa_path(p, leaf); return fa_content_is(p, want); }
static HANDLE fa_open(const char *path, DWORD access, DWORD share, DWORD disp, DWORD flags) {
    return CreateFileA(path, access, share, NULL, disp, flags, NULL);
}
static HANDLE fa_openl(const char *leaf, DWORD access, DWORD share, DWORD disp, DWORD flags) {
    char p[MAX_PATH * 2]; fa_path(p, leaf);
    return CreateFileA(p, access, share, NULL, disp, flags, NULL);
}
#define FA_SHARE_ALL (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE)
static HANDLE fa_openfile(const char *leaf) { return fa_openl(leaf, GENERIC_READ | GENERIC_WRITE, FA_SHARE_ALL, OPEN_EXISTING, 0); }
static HANDLE fa_opendir(const char *leaf, DWORD access) {
    return fa_openl(leaf, access, FA_SHARE_ALL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS);
}

static int fa_is_ntfs(void) {
    static int cached = -1;
    if (cached < 0) {
        char root[MAX_PATH], fs[64] = "";
        char p[MAX_PATH * 2]; fa_path(p, "");
        if (!GetVolumePathNameA(p, root, sizeof root)) strcpy(root, "C:\\");
        GetVolumeInformationA(root, NULL, 0, NULL, NULL, NULL, fs, sizeof fs);
        cached = !_stricmp(fs, "NTFS");
    }
    return cached;
}
/* single-evaluation versions: operands often contain calls with side effects */
#undef EXPECT_EQ
#undef EXPECT_NE
#define EXPECT_EQ(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", (unsigned long long)a_, (unsigned long long)b_); } while (0)
#define EXPECT_NE(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); \
    t_check(a_ != b_, __FILE__, __LINE__, #a " != " #b, "got 0x%llx", (unsigned long long)a_); } while (0)
#define FA_REQUIRE_STREAMS() do { if (!fa_is_ntfs() || t_is_wine()) { t_skip("needs real NTFS"); return; } } while (0)
#define FA_REQUIRE_NTFS() do { if (!fa_is_ntfs()) { t_skip("not NTFS"); return; } } while (0)

static unsigned long long fa_ft64(FILETIME f) { return ((unsigned long long)f.dwHighDateTime << 32) | f.dwLowDateTime; }
static FILETIME fa_mkft(unsigned long long v) { FILETIME f; f.dwLowDateTime = (DWORD)v; f.dwHighDateTime = (DWORD)(v >> 32); return f; }
/* 2001-01-01 00:00:00 UTC in FILETIME ticks */
#define FA_FT_2001 0x01C0D2C000000000ULL
/* NTFS allows 100ns; FAT 2s granularity. FA_TICK_SEC = 1 second in 100ns ticks */
#define FA_SEC 10000000ULL


#endif
