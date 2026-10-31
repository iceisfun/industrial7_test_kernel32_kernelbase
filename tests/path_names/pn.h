/* Shared helpers for the path_names area (all static). */
#ifndef PN_H
#define PN_H
#include "t.h"
#include <wchar.h>
#include <ctype.h>

#ifndef ERROR_FILENAME_EXCED_RANGE
#define ERROR_FILENAME_EXCED_RANGE 206
#endif
#define HR_W32(e) ((HRESULT)(((e) & 0xFFFF) | 0x80070000))
#define HR_INVALIDARG ((HRESULT)0x80070057)
#define HR_INSUFFICIENT ((HRESULT)0x8007007A)
#define HR_EXCED ((HRESULT)0x800700CE)
#define HR_BADARGS ((HRESULT)0x80070057)

/* Rotating wide-string conversion of an ASCII/UTF-8 literal. */
static inline WCHAR *pn_w(const char *s) {
    static WCHAR bufs[16][1024];
    static int i;
    WCHAR *b = bufs[i++ & 15];
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, b, 1024);
    if (n <= 0) b[0] = 0;
    return b;
}
static inline void pn_a(char *dst, size_t cap, const WCHAR *w) {
    int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, dst, (int)cap, NULL, NULL);
    if (n <= 0) dst[0] = 0;
}
/* Narrow a wide string into a rotating buffer (for messages). */
static inline const char *pn_n(const WCHAR *w) {
    static char bufs[8][1200];
    static int i;
    char *b = bufs[i++ & 7];
    pn_a(b, 1200, w);
    return b;
}
static inline int pn_eq_ci(const WCHAR *a, const WCHAR *b) { return !_wcsicmp(a, b); }
/* Wide compare against ASCII literal (exact). */
static inline int pn_weq(const WCHAR *a, const char *lit) {
    while (*a && *lit) { if (*a != (WCHAR)(unsigned char)*lit) return 0; a++; lit++; }
    return !*a && !*lit;
}
static inline int pn_weq_ci(const WCHAR *a, const char *lit) {
    while (*a && *lit) { if (towlower(*a) != towlower((unsigned char)*lit)) return 0; a++; lit++; }
    return !*a && !*lit;
}
static inline size_t pn_wlen(const WCHAR *w) { size_t n = 0; while (w[n]) n++; return n; }
/* Drive letter of the Windows directory (uppercase). */
static inline char pn_sysdrive(void) {
    char b[MAX_PATH]; GetWindowsDirectoryA(b, sizeof b);
    return (char)toupper((unsigned char)b[0]);
}
static inline int pn_fileexists(const char *p) { return GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES; }
static inline void pn_touch(const char *p) {
    HANDLE h = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) { DWORD n; WriteFile(h, "x", 1, &n, NULL); CloseHandle(h); }
}
/* Fill a wide buffer with 'n' chars of c then NUL. */
static inline void pn_fill(WCHAR *b, int n, WCHAR c) { for (int i = 0; i < n; i++) b[i] = c; b[n] = 0; }

#define SENT 0xBEEF
/* Evaluates the call exactly once (EXPECT_EQ evaluates its arguments twice). */
#define EXPECT_HR(call, want) do { HRESULT h_ = (call); \
    t_check(h_ == (HRESULT)(want), __FILE__, __LINE__, #call " == " #want, "hr 0x%lx want 0x%lx", (unsigned long)h_, (unsigned long)(HRESULT)(want)); } while (0)
#endif
