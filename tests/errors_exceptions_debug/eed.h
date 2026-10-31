/* Shared helpers for the errors_exceptions_debug area. */
#ifndef EED_H
#define EED_H
#include "t.h"
#include <setjmp.h>
#include <stdlib.h>

#define EED_TEB_LASTERR()    (*(volatile DWORD *)((char *)NtCurrentTeb() + 0x68))
#define EED_TEB_LASTSTATUS() (*(volatile DWORD *)((char *)NtCurrentTeb() + 0x1250))

#ifndef STATUS_NONCONTINUABLE_EXCEPTION
#define STATUS_NONCONTINUABLE_EXCEPTION ((DWORD)0xC0000025)
#endif
#define EED_STATUS_FAIL_FAST_EXCEPTION  0xC0000602u
#define EED_STATUS_STACK_BUFFER_OVERRUN 0xC0000409u
#define EED_STATUS_UNWIND               0xC0000027u

typedef ULONG (WINAPI *RtlGetLastWin32Error_t)(void);
typedef void (WINAPI *RtlSetLastWin32Error_t)(ULONG);
#define NTF(T, name) ((T)t_proc_nt(name))

static inline int eed_en_us(void) { return (GetUserDefaultUILanguage() & 0x3ff) == 9; }

/* Child exit code helper: true when the code is a harness failure (spawn failure / timeout). */
static inline int eed_child_failed(DWORD c) { return c == 0xDEAD0000 || c == 0xDEAD0001; }

/* Convert ascii to wide. */
static inline void eed_widen(const char *s, WCHAR *w, size_t cap) {
    size_t i = 0;
    for (; s[i] && i + 1 < cap; i++) w[i] = (WCHAR)(unsigned char)s[i];
    w[i] = 0;
}
#endif
