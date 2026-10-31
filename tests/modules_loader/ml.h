/* Shared helpers for the modules_loader area (all static / area-local). */
#ifndef ML_H
#define ML_H
#include "t.h"

enum { ML_A = 1, ML_DEP = 2, ML_IMP = 3 };
const unsigned char *ml_blob(int which, size_t *n);

/* Exports of the embedded k32m_a image */
#define ML_ORD_PROC_ATTACH 1
#define ML_ORD_THREAD_ATTACH 2
#define ML_ORD_THREAD_DETACH 3
#define ML_ORD_VALUE 4
#define ML_ORD_ADD 5
#define ML_ORD_ORD7 7
#define ML_ORD_CASE 9
#define ML_ORD_NONAME 11
#define ML_ORD_FWD_PID 13
#define ML_ORD_FWD_NT 14
#define ML_ORD_FWD_BAD 15
#define ML_ORD_MAX 15

#ifndef LOAD_LIBRARY_REQUIRE_SIGNED_TARGET
#define LOAD_LIBRARY_REQUIRE_SIGNED_TARGET 0x80
#endif
#ifndef LOAD_LIBRARY_SAFE_CURRENT_DIRS
#define LOAD_LIBRARY_SAFE_CURRENT_DIRS 0x2000
#endif
#ifndef LOAD_LIBRARY_OS_INTEGRITY_CONTINUITY
#define LOAD_LIBRARY_OS_INTEGRITY_CONTINUITY 0x8000
#endif
#ifndef LOAD_LIBRARY_AS_IMAGE_RESOURCE
#define LOAD_LIBRARY_AS_IMAGE_RESOURCE 0x20
#endif
#ifndef LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE
#define LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE 0x40
#endif
#ifndef LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR
#define LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR 0x100
#define LOAD_LIBRARY_SEARCH_APPLICATION_DIR 0x200
#define LOAD_LIBRARY_SEARCH_USER_DIRS 0x400
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x800
#define LOAD_LIBRARY_SEARCH_DEFAULT_DIRS 0x1000
#endif

/* Create a fresh sub directory of the scratch dir; returns "<tmp>\sub\" (static ring buffer). */
static inline const char *ml_dir(const char *sub) {
    static char ring[8][MAX_PATH]; static int i;
    char *b = ring[i++ & 7];
    snprintf(b, MAX_PATH, "%s%s", t_tmpdir(), sub);
    CreateDirectoryA(b, NULL);
    strcat(b, "\\");
    return b;
}
/* Write file contents; returns 1 on success. */
static inline int ml_write(const char *path, const void *d, size_t n) {
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    DWORD w = 0; BOOL ok = WriteFile(h, d, (DWORD)n, &w, NULL);
    CloseHandle(h);
    return ok && w == n;
}
/* Write one of the embedded images to dir+leaf. */
static inline int ml_put(int which, const char *dir, const char *leaf, char *out /*MAX_PATH or NULL*/) {
    char tmp[MAX_PATH]; size_t n; const unsigned char *d = ml_blob(which, &n);
    snprintf(tmp, sizeof tmp, "%s%s", dir, leaf);
    if (out) strcpy(out, tmp);
    return ml_write(tmp, d, n);
}
/* Copy a system32 file to dir+leaf. */
static inline int ml_copy_sys(const char *sysleaf, const char *dir, const char *leaf, char *out) {
    char src[MAX_PATH], dst[MAX_PATH];
    GetSystemDirectoryA(src, sizeof src); strcat(src, "\\"); strcat(src, sysleaf);
    snprintf(dst, sizeof dst, "%s%s", dir, leaf);
    if (out) strcpy(out, dst);
    return CopyFileA(src, dst, FALSE) != 0;
}
static inline WCHAR *ml_w(const char *s) {
    static WCHAR ring[8][1024]; static int i;
    WCHAR *b = ring[i++ & 7];
    MultiByteToWideChar(CP_ACP, 0, s, -1, b, 1024);
    return b;
}
static inline char *ml_a(const WCHAR *s) {
    static char ring[8][1024]; static int i;
    char *b = ring[i++ & 7];
    WideCharToMultiByte(CP_ACP, 0, s, -1, b, 1024, NULL, NULL);
    return b;
}
/* Fully unload a module whatever its refcount (bounded; scaffolding, not under test). */
static inline void ml_unload(HMODULE h) {
    if (!h) return;
    if ((ULONG_PTR)h & 3) { FreeLibrary(h); return; }      /* data-file / image-resource views: one release only */
    for (int i = 0; i < 64; i++) if (!FreeLibrary(h)) break;
}
static inline HMODULE ml_load(const char *path) { return LoadLibraryA(path); }
/* Is `m` currently loaded (by exact handle)? */
static inline int ml_is_loaded(HMODULE m) {
    HMODULE h = NULL;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)m, &h)) return 0;
    return h == m;
}
/* Parse helper: NT headers of a mapped/loaded image or NULL. */
static inline const IMAGE_NT_HEADERS64 *ml_nt(const void *base) {
    const IMAGE_DOS_HEADER *d = base;
    if (!base || d->e_magic != IMAGE_DOS_SIGNATURE) return NULL;
    const IMAGE_NT_HEADERS64 *n = (const void *)((const char *)base + d->e_lfanew);
    return n->Signature == IMAGE_NT_SIGNATURE ? n : NULL;
}
static inline void *ml_sys_module(const char *name) { return GetModuleHandleA(name); }
#endif

/* Run a copy of the test exe (any path) as a child; cwd may be NULL. Hard timeout, killed on expiry. */
static inline DWORD ml_run_exe(const char *exe, const char *child, const char *args, const char *cwd, DWORD timeout_ms, DWORD flags) {
    char cmd[4096]; STARTUPINFOA si; PROCESS_INFORMATION pi; DWORD code = 0xDEAD0000;
    memset(&si, 0, sizeof si); si.cb = sizeof si; memset(&pi, 0, sizeof pi);
    snprintf(cmd, sizeof cmd, "\"%s\" --child %s %s", exe, child, args ? args : "");
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, flags, NULL, cwd, &si, &pi)) return 0xDEAD0000;
    if (WaitForSingleObject(pi.hProcess, timeout_ms) != WAIT_OBJECT_0) { TerminateProcess(pi.hProcess, 1); WaitForSingleObject(pi.hProcess, 5000); code = 0xDEAD0001; }
    else GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return code;
}
/* Copy the test exe into dir as leaf; returns 1 on success. */
static inline int ml_copy_self(const char *dir, const char *leaf, char *out) {
    char dst[MAX_PATH]; snprintf(dst, sizeof dst, "%s%s", dir, leaf);
    if (out) strcpy(out, dst);
    return CopyFileA(t_self_path(), dst, FALSE) != 0;
}
