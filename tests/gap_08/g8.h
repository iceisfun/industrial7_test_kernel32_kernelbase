/* gap_08: shlwapi-style Path* exports living in kernelbase.dll (A variants and a few W). */
#ifndef G8_H
#define G8_H
#include "t.h"
#include <shlwapi.h>

/* Resolve like U(), but under an emulator that keeps these in shlwapi fall back to it. */
static FARPROC g8_proc(const char *n) {
    FARPROC p = t_proc(n);
    if (!p && t_is_wine()) { HMODULE h = LoadLibraryA("shlwapi.dll"); if (h) p = GetProcAddress(h, n); }
    return p;
}
#define P(name) ((__typeof__(&name))g8_proc(#name))

/* The mingw headers declare these as returning void; the DLLs return BOOL. */
typedef BOOL (WINAPI *g8_bool_lpstr_fn)(LPSTR);
#define PQS ((g8_bool_lpstr_fn)g8_proc("PathQuoteSpacesA"))
#define PUQS ((g8_bool_lpstr_fn)g8_proc("PathUnquoteSpacesA"))

#define G8BUF 1024
/* Fill a 1024-byte scratch buffer: whole buffer 0xCC-poisoned then the string copied in. */
static char *g8_set(char *b, const char *s) { memset(b, 0, G8BUF); strcpy(b, s); return b; }
static char *g8_fill(char *b, int len, char c) { memset(b, 0, G8BUF); memset(b, c, len); return b; }
/* Offset of pointer p inside buffer b, or -1 for NULL. */
#define OFF(p, b) ((p) ? (long)((const char *)(p) - (const char *)(b)) : -1L)
#endif
