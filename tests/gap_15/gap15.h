/* gap_15 shared helpers (static). */
#ifndef GAP15_H
#define GAP15_H
#include "t.h"
#include <shlwapi.h>
#include <winver.h>
#include <evntrace.h>

#define G_E_POINTER   ((HRESULT)0x80004003L)
#define G_E_INVALIDARG ((HRESULT)0x80070057L)
#define G_E_NOTIMPL   ((HRESULT)0x80004001L)

typedef HRESULT (WINAPI *g_fnA4)(LPCSTR, LPSTR, LPDWORD, DWORD);
typedef HRESULT (WINAPI *g_fnW4)(LPCWSTR, LPWSTR, LPDWORD, DWORD);

static void g_widen(const char *s, WCHAR *w, size_t cap)
{
    size_t i = 0;
    for (; s[i] && i + 1 < cap; i++) w[i] = (unsigned char)s[i];
    w[i] = 0;
}
static int g_waeq(const WCHAR *w, const char *a)
{
    size_t i = 0;
    for (; w[i] || a[i]; i++)
        if (w[i] != (WCHAR)(unsigned char)a[i]) return 0;
    return 1;
}
static void g_narrow(const WCHAR *w, char *a, size_t cap)
{
    size_t i = 0;
    for (; w[i] && i + 1 < cap; i++) a[i] = (char)w[i];
    a[i] = 0;
}

typedef struct {
    HRESULT hrA, hrW;
    DWORD nA, nW;
    char a[2048];
    WCHAR w[2048];
} g_res;

/* Call the (in,out,pcch,flags) A and W entry points with an ASCII input and a
 * buffer of cap characters (cap is the *pcch passed in). */
static void g_run4(g_fnA4 fa, g_fnW4 fw, const char *in, DWORD flags, DWORD cap, g_res *r)
{
    char ain[2048];
    WCHAR win[2048];
    memset(r, 0, sizeof *r);
    memset(r->a, 0x7e, sizeof r->a);
    for (size_t i = 0; i < 2048; i++) r->w[i] = 0x7e7e;
    strncpy(ain, in, sizeof ain - 1); ain[sizeof ain - 1] = 0;
    g_widen(in, win, 2048);
    if (cap > 2040) cap = 2040;
    r->nA = cap;
    r->hrA = fa(ain, r->a, &r->nA, flags);
    r->nW = cap;
    r->hrW = fw(win, r->w, &r->nW, flags);
}

/* Table-driven expectation. hard=0: observation only. */
typedef struct { const char *in; DWORD flags; HRESULT hr; const char *out; int hard; } g_case4;

static void g_check4(const char *fn, g_fnA4 fa, g_fnW4 fw, const g_case4 *c, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        g_res r;
        g_run4(fa, fw, c[i].in, c[i].flags, 1024, &r);
        char wn[2048];
        g_narrow(r.w, wn, sizeof wn);
        if (c[i].hard) {
            CHECKF(r.hrA == c[i].hr, "%sA(\"%s\",0x%lx) hr=0x%lx want 0x%lx", fn, c[i].in, (unsigned long)c[i].flags, (unsigned long)r.hrA, (unsigned long)c[i].hr);
            CHECKF(r.hrW == c[i].hr, "%sW(\"%s\",0x%lx) hr=0x%lx want 0x%lx", fn, c[i].in, (unsigned long)c[i].flags, (unsigned long)r.hrW, (unsigned long)c[i].hr);
            if (c[i].out && SUCCEEDED(c[i].hr)) {
                CHECKF(!strcmp(r.a, c[i].out), "%sA(\"%s\",0x%lx) -> \"%s\" want \"%s\"", fn, c[i].in, (unsigned long)c[i].flags, r.a, c[i].out);
                CHECKF(!strcmp(wn, c[i].out), "%sW(\"%s\",0x%lx) -> \"%s\" want \"%s\"", fn, c[i].in, (unsigned long)c[i].flags, wn, c[i].out);
                CHECKF(r.nA == strlen(c[i].out), "%sA count %lu", fn, (unsigned long)r.nA);
                CHECKF(r.nW == strlen(c[i].out), "%sW count %lu", fn, (unsigned long)r.nW);
            }
        } else {
            t_note("%s(\"%s\",0x%lx): A hr=0x%lx \"%s\"  W hr=0x%lx \"%s\"", fn, c[i].in, (unsigned long)c[i].flags,
                   (unsigned long)r.hrA, SUCCEEDED(r.hrA) ? r.a : "-", (unsigned long)r.hrW, SUCCEEDED(r.hrW) ? wn : "-");
            /* Win10: the A entry points return S_FALSE for an empty result (ShStrA::_SetStr returns 1 on an empty string), W returns S_OK */
            if (c[i].in[0] == 0 && r.hrA == S_FALSE && r.hrW == S_OK) continue;
            CHECKF(r.hrA == r.hrW, "%s A/W hr differ for \"%s\": 0x%lx vs 0x%lx", fn, c[i].in, (unsigned long)r.hrA, (unsigned long)r.hrW);
            if (r.hrA == S_OK && r.hrW == S_OK)
                CHECKF(!strcmp(r.a, wn), "%s A/W output differ for \"%s\": \"%s\" vs \"%s\"", fn, c[i].in, r.a, wn);
        }
    }
}

/* Standard argument validation: all of these must be E_INVALIDARG. */
static void g_args4(const char *fn, g_fnA4 fa, g_fnW4 fw, const char *good, DWORD flags)
{
    char out[64]; WCHAR wout[64]; DWORD n; WCHAR win[256]; char ain[256];
    strcpy(ain, good); g_widen(good, win, 256);
    n = 64; CHECKF(fa(NULL, out, &n, flags) == G_E_INVALIDARG, "%sA NULL in", fn);
    n = 64; CHECKF(fw(NULL, wout, &n, flags) == G_E_INVALIDARG, "%sW NULL in", fn);
    n = 64; CHECKF(fa(ain, NULL, &n, flags) == G_E_INVALIDARG, "%sA NULL out", fn);
    n = 64; CHECKF(fw(win, NULL, &n, flags) == G_E_INVALIDARG, "%sW NULL out", fn);
    CHECKF(fa(ain, out, NULL, flags) == G_E_INVALIDARG, "%sA NULL pcch", fn);
    CHECKF(fw(win, wout, NULL, flags) == G_E_INVALIDARG, "%sW NULL pcch", fn);
    n = 0; CHECKF(fa(ain, out, &n, flags) == G_E_INVALIDARG, "%sA zero pcch", fn);
    CHECKF(n == 0, "%sA zero pcch unchanged (%lu)", fn, (unsigned long)n);
    n = 0; CHECKF(fw(win, wout, &n, flags) == G_E_INVALIDARG, "%sW zero pcch", fn);
    CHECKF(n == 0, "%sW zero pcch unchanged (%lu)", fn, (unsigned long)n);
}

/* Truncation semantics: with an exact-size buffer success and count==len; with len
 * (one short) E_POINTER and *pcch = len+1 and (for W) output untouched or empty. */
static void g_trunc4(const char *fn, g_fnA4 fa, g_fnW4 fw, const char *in, DWORD flags, const char *expect)
{
    size_t len = strlen(expect);
    g_res r;
    g_run4(fa, fw, in, flags, (DWORD)len + 1, &r);
    CHECKF(r.hrA == S_OK && r.hrW == S_OK, "%s exact fit hr A=0x%lx W=0x%lx", fn, (unsigned long)r.hrA, (unsigned long)r.hrW);
    CHECKF(r.nA == len && r.nW == len, "%s exact fit count %lu/%lu want %lu", fn, (unsigned long)r.nA, (unsigned long)r.nW, (unsigned long)len);
    CHECKF(!strcmp(r.a, expect), "%s exact fit A \"%s\"", fn, r.a);
    CHECKF(g_waeq(r.w, expect), "%s exact fit W", fn);
    g_run4(fa, fw, in, flags, (DWORD)len, &r);
    CHECKF(r.hrA == G_E_POINTER, "%sA short hr=0x%lx", fn, (unsigned long)r.hrA);
    CHECKF(r.hrW == G_E_POINTER, "%sW short hr=0x%lx", fn, (unsigned long)r.hrW);
    CHECKF(r.nA == len + 1, "%sA short needed %lu want %lu", fn, (unsigned long)r.nA, (unsigned long)len + 1);
    CHECKF(r.nW == len + 1, "%sW short needed %lu want %lu", fn, (unsigned long)r.nW, (unsigned long)len + 1);
    g_run4(fa, fw, in, flags, 1, &r);
    CHECKF(r.hrA == G_E_POINTER && r.hrW == G_E_POINTER, "%s cap=1 hr", fn);
    CHECKF(r.nA == len + 1 && r.nW == len + 1, "%s cap=1 needed", fn);
    /* query needed size, then retry with exactly that */
    g_run4(fa, fw, in, flags, 1, &r);
    DWORD need = r.nW;
    g_run4(fa, fw, in, flags, need, &r);
    CHECKF(r.hrA == S_OK && r.hrW == S_OK && !strcmp(r.a, expect), "%s retry with reported size", fn);
}
#endif
