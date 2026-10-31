/* Shared helpers for the strings_codepages area (static only). */
#ifndef SC_H
#define SC_H
#include "t.h"
#include <winnls.h>

#define ARRAY_N(a) ((int)(sizeof(a) / sizeof((a)[0])))
#define W_(...) { __VA_ARGS__ }
#ifndef CP_UTF7
#define CP_UTF7 65000
#endif
#ifndef CP_UTF8
#define CP_UTF8 65001
#endif
#ifndef CP_SYMBOL
#define CP_SYMBOL 42
#endif
#ifndef MB_ERR_INVALID_CHARS
#define MB_ERR_INVALID_CHARS 8
#endif
#ifndef WC_ERR_INVALID_CHARS
#define WC_ERR_INVALID_CHARS 0x80
#endif
#ifndef WC_NO_BEST_FIT_CHARS
#define WC_NO_BEST_FIT_CHARS 0x400
#endif
#ifndef LCMAP_BYTEREV
#define LCMAP_BYTEREV 0x800
#endif
#ifndef LINGUISTIC_IGNORECASE
#define LINGUISTIC_IGNORECASE 0x10
#define LINGUISTIC_IGNOREDIACRITIC 0x20
#endif
#ifndef SORT_DIGITSASNUMBERS
#define SORT_DIGITSASNUMBERS 8
#endif
#ifndef NORM_LINGUISTIC_CASING
#define NORM_LINGUISTIC_CASING 0x08000000
#endif

#define E_INVALID_PARAMETER 87
#define E_INSUFFICIENT_BUFFER 122
#define E_INVALID_FLAGS 1004
#define E_NO_UNICODE_TRANSLATION 1113

/* wide compare: n WCHARs */
static inline int sc_weq(const WCHAR *a, const WCHAR *b, int n) {
    for (int i = 0; i < n; i++) if (a[i] != b[i]) return 0;
    return 1;
}
static inline void sc_wdump(char *out, size_t cap, const WCHAR *w, int n) {
    size_t o = 0; out[0] = 0;
    for (int i = 0; i < n && o + 8 < cap; i++) o += snprintf(out + o, cap - o, "%04X ", (unsigned)w[i]);
}
static inline void sc_bdump(char *out, size_t cap, const unsigned char *b, int n) {
    size_t o = 0; out[0] = 0;
    for (int i = 0; i < n && o + 6 < cap; i++) o += snprintf(out + o, cap - o, "%02X ", (unsigned)b[i]);
}
/* check wide buffer equals expected */
#define CHECK_WEQ(got, exp, n) do { char g_[256], e_[256]; sc_wdump(g_, sizeof g_, (got), (n)); sc_wdump(e_, sizeof e_, (exp), (n)); \
    t_check(sc_weq((got), (exp), (n)), __FILE__, __LINE__, #got " == " #exp, "got [%s] want [%s]", g_, e_); } while (0)
#define CHECK_BEQ(got, exp, n) do { char g_[256], e_[256]; sc_bdump(g_, sizeof g_, (const unsigned char *)(got), (n)); sc_bdump(e_, sizeof e_, (const unsigned char *)(exp), (n)); \
    t_check(!memcmp((got), (exp), (n)), __FILE__, __LINE__, #got " == " #exp, "got [%s] want [%s]", g_, e_); } while (0)

/* Fixed-address page helpers for IsBad* tests. */
static inline void *sc_alloc_pages(size_t n, DWORD prot) {
    return VirtualAlloc(NULL, n, MEM_COMMIT | MEM_RESERVE, prot);
}
#endif
