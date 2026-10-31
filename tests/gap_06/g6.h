#ifndef G6_H
#define G6_H
#include "t.h"
#include <stdarg.h>
#include <sddl.h>
#include <aclapi.h>

/* Build a SID in buf: S-1-<auth>-<subs...>. */
static inline PSID g6_sid(BYTE *buf, BYTE auth, int n, ...) {
    va_list ap; int i;
    memset(buf, 0, 8 + 4 * 15);
    buf[0] = 1; buf[1] = (BYTE)n; buf[7] = auth;
    va_start(ap, n);
    for (i = 0; i < n; i++) ((DWORD *)(buf + 8))[i] = va_arg(ap, DWORD);
    va_end(ap);
    return (PSID)buf;
}
#endif
