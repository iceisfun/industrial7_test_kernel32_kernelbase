/* shared helpers for the heap_local_global area (all static inline) */
#ifndef HLG_H
#define HLG_H
#include "t.h"

#define HLG_ERR_SENTINEL 0x5A5A5Au
#define HLG_POISON() SetLastError(HLG_ERR_SENTINEL)

typedef struct { unsigned long long s; } hlg_rng;
static inline void hlg_seed(hlg_rng *r, unsigned long long s) { r->s = s ? s : 1; }
static inline unsigned hlg_next(hlg_rng *r) {
    r->s = r->s * 6364136223846793005ULL + 1442695040888963407ULL;
    return (unsigned)(r->s >> 33);
}
static inline void hlg_fill(void *p, size_t n, unsigned char seed) {
    unsigned char *b = (unsigned char *)p;
    for (size_t i = 0; i < n; i++) b[i] = (unsigned char)(seed + i * 7);
}
static inline int hlg_verify(const void *p, size_t n, unsigned char seed) {
    const unsigned char *b = (const unsigned char *)p;
    for (size_t i = 0; i < n; i++) if (b[i] != (unsigned char)(seed + i * 7)) return 0;
    return 1;
}
static inline int hlg_all(const void *p, size_t n, unsigned char v) {
    const unsigned char *b = (const unsigned char *)p;
    for (size_t i = 0; i < n; i++) if (b[i] != v) return 0;
    return 1;
}
/* process heap straight from the PEB (offset 0x30 on x64) */
static inline HANDLE hlg_peb_heap(void) {
    return *(HANDLE *)((char *)NtCurrentTeb()->ProcessEnvironmentBlock + 0x30);
}
#define HLG_NOT_LOCKED 158
#define HLG_DISCARDED 157
#define HLG_INVALID_HANDLE 6
#define HLG_NOMEM 8
#define HLG_INVPARAM 87
#define HLG_NO_MORE_ITEMS 259
/* a handle-shaped value (bit 3 set) that cannot be in the handle table */
#define HLG_BOGUS_HANDLE ((HANDLE)(ULONG_PTR)0x1008)

typedef struct { DWORD Version; DWORD Flags; } hlg_optres;
#endif
