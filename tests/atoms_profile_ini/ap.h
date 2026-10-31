/* Shared helpers for the atoms / INI profile area. */
#ifndef AP_H
#define AP_H
#include "t.h"
#include <stdlib.h>

#ifndef ERROR_BAD_LENGTH
#define ERROR_BAD_LENGTH 24
#endif

/* ---- atom scope wrappers: g=1 -> Global*, g=0 -> local table ---- */
static ATOM ap_add_a(int g, const char *s)  { return g ? U(GlobalAddAtomA)(s) : U(AddAtomA)(s); }
static ATOM ap_add_w(int g, const WCHAR *s) { return g ? U(GlobalAddAtomW)(s) : U(AddAtomW)(s); }
static ATOM ap_find_a(int g, const char *s)  { return g ? U(GlobalFindAtomA)(s) : U(FindAtomA)(s); }
static ATOM ap_find_w(int g, const WCHAR *s) { return g ? U(GlobalFindAtomW)(s) : U(FindAtomW)(s); }
static ATOM ap_del(int g, ATOM a) { return g ? U(GlobalDeleteAtom)(a) : U(DeleteAtom)(a); }
static UINT ap_name_a(int g, ATOM a, char *b, int n)  { return g ? U(GlobalGetAtomNameA)(a, b, n) : U(GetAtomNameA)(a, b, n); }
static UINT ap_name_w(int g, ATOM a, WCHAR *b, int n) { return g ? U(GlobalGetAtomNameW)(a, b, n) : U(GetAtomNameW)(a, b, n); }

static void ap_widen(WCHAR *d, const char *s) { while ((*d++ = (unsigned char)*s++)) ; }
static int ap_wlen(const WCHAR *s) { int n = 0; while (s[n]) n++; return n; }

/* Unique ascii name per process/scope/tag. */
static void ap_aname(char *out, size_t cap, int g, const char *tag) {
    snprintf(out, cap, "k32t%lu_%c_%s", GetCurrentProcessId(), g ? 'G' : 'L', tag);
}

#define SCOPED(n) \
    static void n##_body(int g); \
    TEST(atoms_global_##n) { REQUIRE(U(GlobalAddAtomA)); REQUIRE(U(GlobalAddAtomW)); REQUIRE(U(GlobalFindAtomA)); REQUIRE(U(GlobalFindAtomW)); \
        REQUIRE(U(GlobalDeleteAtom)); REQUIRE(U(GlobalGetAtomNameA)); REQUIRE(U(GlobalGetAtomNameW)); n##_body(1); } \
    TEST(atoms_local_##n) { REQUIRE(U(AddAtomA)); REQUIRE(U(AddAtomW)); REQUIRE(U(FindAtomA)); REQUIRE(U(FindAtomW)); \
        REQUIRE(U(DeleteAtom)); REQUIRE(U(GetAtomNameA)); REQUIRE(U(GetAtomNameW)); n##_body(0); } \
    static void n##_body(int g)

/* ---- files ---- */
static void ap_put(const char *path, const void *d, size_t n) {
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD w;
    if (h == INVALID_HANDLE_VALUE) return;
    if (n) WriteFile(h, d, (DWORD)n, &w, NULL);
    CloseHandle(h);
}
static void ap_puts(const char *path, const char *s) { ap_put(path, s, strlen(s)); }
/* returns file size or -1; reads up to cap-1 bytes and NUL terminates */
static long ap_get(const char *path, char *buf, size_t cap) {
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD n = 0;
    if (h == INVALID_HANDLE_VALUE) return -1;
    if (cap > 1) ReadFile(h, buf, (DWORD)cap - 1, &n, NULL);
    CloseHandle(h);
    if (cap) buf[n] = 0;
    return (long)n;
}
static int ap_exists(const char *path) { return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES; }
static void ap_rm(const char *path) { DeleteFileA(path); }
static void ap_wpath(WCHAR *out, const char *path) { ap_widen(out, path); }

/* New ini path for a test (removes any stale file). */
static void ap_ini(char *path, const char *leaf, const char *content) {
    t_tmpfile(path, MAX_PATH, leaf);
    ap_rm(path);
    if (content) ap_puts(path, content);
}

/* Render a multi-sz block as "a|b|c" (stops at the double NUL or max). */
static void ap_flat(const char *b, size_t max, char *out, size_t cap) {
    size_t o = 0, i = 0;
    while (i < max && o + 2 < cap) {
        if (!b[i]) { if (i + 1 >= max || !b[i + 1]) break; out[o++] = '|'; i++; continue; }
        out[o++] = b[i++];
    }
    out[o] = 0;
}
static void ap_flatw(const WCHAR *b, size_t max, char *out, size_t cap) {
    size_t o = 0, i = 0;
    while (i < max && o + 2 < cap) {
        if (!b[i]) { if (i + 1 >= max || !b[i + 1]) break; out[o++] = '|'; i++; continue; }
        out[o++] = b[i] < 128 ? (char)b[i] : '?'; i++;
    }
    out[o] = 0;
}

/* ---- profile wrappers ---- */
static DWORD ap_gps(const char *s, const char *k, const char *d, char *b, DWORD n, const char *f) {
    return U(GetPrivateProfileStringA)(s, k, d, b, n, f);
}
static BOOL ap_wps(const char *s, const char *k, const char *v, const char *f) {
    return U(WritePrivateProfileStringA)(s, k, v, f);
}
/* Value lookup returning an owned static buffer ("<none>" convention not used: default is "DEF"). */
static const char *ap_val(const char *s, const char *k, const char *f) {
    static char bufs[8][1024]; static int idx;
    char *b = bufs[idx++ & 7];
    ap_gps(s, k, "DEF", b, 1024, f);
    return b;
}
#endif
