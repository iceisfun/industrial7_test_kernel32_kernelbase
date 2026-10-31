/* Shared helpers for the dir_find area (static; scaffolding only). */
#ifndef DH_H
#define DH_H
#include "t.h"

#ifndef FIND_FIRST_EX_CASE_SENSITIVE
#define FIND_FIRST_EX_CASE_SENSITIVE 1
#endif
#ifndef FIND_FIRST_EX_LARGE_FETCH
#define FIND_FIRST_EX_LARGE_FETCH 2
#endif
#ifndef FIND_FIRST_EX_ON_DISK_ENTRIES_ONLY
#define FIND_FIRST_EX_ON_DISK_ENTRIES_ONLY 4
#endif

#define DH_NAME 300
typedef struct { char n[DH_NAME]; } dh_name;

/* Fresh sub directory of the private scratch dir; returns path without trailing backslash. */
static char *dh_dir(const char *name) {
    static char ring[16][MAX_PATH];
    static int idx;
    char *p = ring[idx++ & 15];
    snprintf(p, MAX_PATH, "%s%s", t_tmpdir(), name);
    CreateDirectoryA(p, NULL);
    return p;
}
static void dh_path(char *out, size_t cap, const char *dir, const char *leaf) {
    snprintf(out, cap, "%s\\%s", dir, leaf);
}
static void dh_w(WCHAR *o, size_t cap, const char *a) {
    MultiByteToWideChar(CP_ACP, 0, a, -1, o, (int)cap);
}
static void dh_u8(WCHAR *o, size_t cap, const char *a) {
    MultiByteToWideChar(CP_UTF8, 0, a, -1, o, (int)cap);
}
static int dh_exists(const char *p) { return GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES; }
static int dh_isdir(const char *p) { DWORD a = GetFileAttributesA(p); return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY); }
static int dh_wexists(const WCHAR *p) { return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES; }
static int dh_mkfile(const char *dir, const char *leaf, DWORD size) {
    char p[MAX_PATH * 2];
    dh_path(p, sizeof p, dir, leaf);
    HANDLE h = CreateFileA(p, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    char buf[512]; memset(buf, 'x', sizeof buf);
    DWORD w;
    while (size) { DWORD n = size > sizeof buf ? (DWORD)sizeof buf : size; WriteFile(h, buf, n, &w, NULL); size -= n; }
    CloseHandle(h);
    return 1;
}
static int dh_mkdir(const char *dir, const char *leaf) {
    char p[MAX_PATH * 2];
    dh_path(p, sizeof p, dir, leaf);
    return CreateDirectoryA(p, NULL) != 0;
}
static int dh_mkfileW(const WCHAR *dir, const WCHAR *leaf, DWORD size) {
    WCHAR p[MAX_PATH * 2];
    snwprintf(p, MAX_PATH * 2, L"%ls\\%ls", dir, leaf);
    HANDLE h = CreateFileW(p, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    char buf[64]; memset(buf, 'y', sizeof buf); DWORD w;
    while (size) { DWORD n = size > sizeof buf ? (DWORD)sizeof buf : size; WriteFile(h, buf, n, &w, NULL); size -= n; }
    CloseHandle(h);
    return 1;
}
static void dh_rm(const char *dir, const char *leaf) {
    char p[MAX_PATH * 2]; dh_path(p, sizeof p, dir, leaf);
    SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL);
    if (!DeleteFileA(p)) RemoveDirectoryA(p);
}
/* Delete via \\?\ (no normalisation) - for names with trailing dots/spaces. */
static void dh_rm_raw(const char *full) {
    WCHAR w[MAX_PATH * 2], p[MAX_PATH * 2];
    dh_w(w, MAX_PATH * 2, full);
    snwprintf(p, MAX_PATH * 2, L"\\\\?\\%ls", w);
    if (!DeleteFileW(p)) RemoveDirectoryW(p);
}

/* Enumerate with the function pointers under test (FindFirstFileA/FindNextFileA/FindClose).
 * Returns number of names (including . and ..), or -1 if FindFirstFile failed (last error preserved). */
static int dh_listA(const char *pattern, dh_name *out, int max) {
    WIN32_FIND_DATAA fd;
    HANDLE h = U(FindFirstFileA)(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return -1;
    int n = 0;
    do { if (n < max) { strncpy(out[n].n, fd.cFileName, DH_NAME - 1); out[n].n[DH_NAME - 1] = 0; } n++; }
    while (U(FindNextFileA)(h, &fd));
    DWORD e = GetLastError();
    U(FindClose)(h);
    SetLastError(e);
    return n;
}
/* Like dh_listA but skips "." and ".." */
static int dh_listA_nd(const char *pattern, dh_name *out, int max) {
    int cap = 20000;
    dh_name *tmp = (dh_name *)malloc(sizeof(dh_name) * cap);
    int n = dh_listA(pattern, tmp, cap);
    if (n < 0) { free(tmp); return n; }
    int k = 0;
    for (int i = 0; i < n && i < cap; i++) {
        if (!strcmp(tmp[i].n, ".") || !strcmp(tmp[i].n, "..")) continue;
        if (k < max) out[k] = tmp[i];
        k++;
    }
    free(tmp);
    return k;
}
static int dh_has(const dh_name *l, int n, const char *name) {
    for (int i = 0; i < n; i++) if (!strcmp(l[i].n, name)) return 1;
    return 0;
}
static int dh_hasi(const dh_name *l, int n, const char *name) {
    for (int i = 0; i < n; i++) if (!lstrcmpiA(l[i].n, name)) return 1;
    return 0;
}
/* true if volume of the scratch dir is NTFS */
static int dh_is_ntfs(void) {
    char root[8] = "C:\\", fs[64] = "";
    const char *t = t_tmpdir();
    if (t[1] == ':') { root[0] = t[0]; }
    GetVolumeInformationA(root, NULL, 0, NULL, NULL, NULL, fs, sizeof fs);
    return !lstrcmpiA(fs, "NTFS");
}

/* Junction (mount point reparse) creation; link must be an existing empty directory. */
static int dh_mkjunction(const char *link, const char *target) {
    WCHAR lw[MAX_PATH], tw[MAX_PATH], full[MAX_PATH], sub[MAX_PATH + 8];
    dh_w(lw, MAX_PATH, link); dh_w(tw, MAX_PATH, target);
    if (!GetFullPathNameW(tw, MAX_PATH, full, NULL)) return 0;
    snwprintf(sub, MAX_PATH + 8, L"\\??\\%ls", full);
    USHORT S = (USHORT)(wcslen(sub) * 2), P = (USHORT)(wcslen(full) * 2);
    static BYTE buf[2048]; memset(buf, 0, sizeof buf);
    *(DWORD *)buf = 0xA0000003;
    USHORT rdl = 8 + S + 2 + P + 2;
    *(USHORT *)(buf + 4) = rdl;
    *(USHORT *)(buf + 8) = 0; *(USHORT *)(buf + 10) = S;
    *(USHORT *)(buf + 12) = S + 2; *(USHORT *)(buf + 14) = P;
    memcpy(buf + 16, sub, S); memcpy(buf + 16 + S + 2, full, P);
    HANDLE h = CreateFileW(lw, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    DWORD ret;
    BOOL ok = DeviceIoControl(h, 0x900A4, buf, rdl + 8, NULL, 0, &ret, NULL);
    CloseHandle(h);
    return ok;
}
static int dh_mksymlink(const char *link, const char *target, int isdir) {
    typedef BOOLEAN (WINAPI *fn_t)(LPCSTR, LPCSTR, DWORD);
    fn_t f = (fn_t)GetProcAddress(GetModuleHandleA("kernel32.dll"), "CreateSymbolicLinkA");
    if (!f) return 0;
    return f(link, target, (isdir ? 1 : 0) | 2) != 0;
}

/* Recursive delete using wide APIs and \\?\ paths (handles unicode, trailing dots, long paths). dirw is a plain or \\?\ path. */
static void dh_rmtreeW(const WCHAR *dirw) {
    WCHAR raw[32768 / 8], pat[32768 / 8], sub[32768 / 8];
    if (!wcsncmp(dirw, L"\\\\?\\", 4)) wcscpy(raw, dirw); else snwprintf(raw, 4096, L"\\\\?\\%ls", dirw);
    snwprintf(pat, 4096, L"%ls\\*", raw);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
            snwprintf(sub, 4096, L"%ls\\%ls", raw, fd.cFileName);
            SetFileAttributesW(sub, FILE_ATTRIBUTE_NORMAL | ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? FILE_ATTRIBUTE_DIRECTORY : 0));
            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && !(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) dh_rmtreeW(sub);
            else if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) RemoveDirectoryW(sub);
            else DeleteFileW(sub);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryW(raw);
}
static void dh_rmtree(const char *dir) { WCHAR w[MAX_PATH]; dh_w(w, MAX_PATH, dir); dh_rmtreeW(w); }
#define DH_CHILD_CHECK(c) do { if (!(c)) return __LINE__; } while (0)
#endif
