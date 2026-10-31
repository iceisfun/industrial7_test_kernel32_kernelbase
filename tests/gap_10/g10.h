/* gap_10: shared scaffolding. Console tests run in a child process that owns a brand new console;
 * the child reports through a result file (FAIL/NOTE/SKIP/DONE lines) that the parent turns into results. */
#ifndef G10_H
#define G10_H
#include "t.h"
#include <wincon.h>
#include <psapi.h>
#include <shlwapi.h>
#include <objbase.h>

typedef void (*g10_fn)(void);
void g10_reg(const char *name, g10_fn fn);
void g10_run(const char *name, int nowindow);
extern int g10_cpass, g10_cskip;
void g10_cline(const char *fmt, ...);
void g10_cfail(const char *file, int line, const char *expr, const char *fmt, ...);
void g10_cnote(const char *fmt, ...);

#define GC(n) \
    static void gcf_##n(void); \
    static void __attribute__((constructor)) gcr_##n(void) { g10_reg(#n, gcf_##n); } \
    TEST(gap10_##n) { g10_run(#n, 0); } \
    static void gcf_##n(void)

#define GCHK(c)   do { if (c) g10_cpass++; else g10_cfail(__FILE__, __LINE__, #c, NULL); } while (0)
#define GEQ(a, b) do { unsigned long long a_ = (unsigned long long)(ULONG_PTR)(a), b_ = (unsigned long long)(ULONG_PTR)(b); \
    if (a_ == b_) g10_cpass++; else g10_cfail(__FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", a_, b_); } while (0)
#define GNE(a, b) do { unsigned long long a_ = (unsigned long long)(ULONG_PTR)(a), b_ = (unsigned long long)(ULONG_PTR)(b); \
    if (a_ != b_) g10_cpass++; else g10_cfail(__FILE__, __LINE__, #a " != " #b, "got 0x%llx", a_); } while (0)
#define GERR(e)   do { DWORD e_ = GetLastError(); if (e_ == (DWORD)(e)) g10_cpass++; else g10_cfail(__FILE__, __LINE__, "GetLastError == " #e, "got %lu want %lu", e_, (DWORD)(e)); } while (0)
#define GREQ(p)   do { if (!(p)) { g10_cline("SKIP missing export: " #p); g10_cskip = 1; return; } } while (0)
#define GWINESKIP(why) do { if (t_is_wine()) { g10_cline("SKIP " why); g10_cskip = 1; return; } } while (0)
#define GWSTR(a, b) do { if (!lstrcmpW((a), (b))) g10_cpass++; else g10_cfail(__FILE__, __LINE__, #a " == " #b, "got \"%ls\" want \"%ls\"", (a), (b)); } while (0)
#define GSTR(a, b)  do { if (!strcmp((a), (b))) g10_cpass++; else g10_cfail(__FILE__, __LINE__, #a " == " #b, "got \"%s\" want \"%s\"", (a), (b)); } while (0)
/* single-evaluation equality for t_check based tests (EXPECT_EQ evaluates its arguments twice) */
#define EQ1(a, b) do { unsigned long long a_ = (unsigned long long)(ULONG_PTR)(a), b_ = (unsigned long long)(ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", a_, b_); } while (0)
/* call that must fail with return 0 and a given last error */
#define GFAIL(call, err) do { SetLastError(0xBAD); BOOL r_ = (BOOL)(call); GEQ(r_, 0); GERR(err); } while (0)

/* common console helpers (static: each TU gets its own copy) */
static HANDLE gin(void)  { return GetStdHandle(STD_INPUT_HANDLE); }
static HANDLE gout(void) { return GetStdHandle(STD_OUTPUT_HANDLE); }
static COORD gco(int x, int y) { COORD c; c.X = (SHORT)x; c.Y = (SHORT)y; return c; }
static SMALL_RECT gsr(int l, int t, int r, int b) { SMALL_RECT s; s.Left = (SHORT)l; s.Top = (SHORT)t; s.Right = (SHORT)r; s.Bottom = (SHORT)b; return s; }
#endif
