/* gap_11 console scaffolding. Every console test body runs in a CHILD process that owns a private console
 * (CREATE_NEW_CONSOLE, minimised, not activated) or has none at all (DETACHED_PROCESS). The parent never
 * attaches to a console. The child reports through a result file (FAIL/NOTE/SKIP/DONE lines). */
#ifndef CON_H_H
#define CON_H_H
#include "g11.h"

extern char g_res[MAX_PATH * 2];
extern int g_cpass, g_cskip;
void cline(const char *fmt, ...);
void cfail(const char *file, int line, const char *expr, const char *fmt, ...);
void cnote(const char *fmt, ...);
typedef void (*cfn_t)(void);
void ct_register(const char *n, cfn_t f);
void prun(const char *name, DWORD flags);

#define CC(c)        do { if (c) g_cpass++; else cfail(__FILE__, __LINE__, #c, NULL); } while (0)
#define CCF(c, ...)  do { if (c) g_cpass++; else cfail(__FILE__, __LINE__, #c, __VA_ARGS__); } while (0)
#define CEQ(a, b)    do { unsigned long long a_ = (unsigned long long)(ULONG_PTR)(a), b_ = (unsigned long long)(ULONG_PTR)(b); \
    if (a_ == b_) g_cpass++; else cfail(__FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", a_, b_); } while (0)
#define CNE(a, b)    do { unsigned long long a_ = (unsigned long long)(ULONG_PTR)(a), b_ = (unsigned long long)(ULONG_PTR)(b); \
    if (a_ != b_) g_cpass++; else cfail(__FILE__, __LINE__, #a " != " #b, "got 0x%llx", a_); } while (0)
#define CERR(e)      do { DWORD e_ = GetLastError(); if (e_ == (DWORD)(e)) g_cpass++; else cfail(__FILE__, __LINE__, "GetLastError == " #e, "got %lu want %lu", e_, (DWORD)(e)); } while (0)
#define CREQ(p)      do { if (!(p)) { cline("SKIP missing export: " #p); g_cskip = 1; return; } } while (0)
#define CWINESKIP(why) do { if (t_is_wine()) { cline("SKIP " why); g_cskip = 1; return; } } while (0)
/* a call that must fail with the given last error */
#define FAILS(call, err) do { SetLastError(0xBAD); BOOL r_ = (call); CEQ(r_, 0); CERR(err); } while (0)

#define CTF(n, fl) \
    static void ct_##n(void); \
    static void __attribute__((constructor)) ctreg_##n(void) { ct_register(#n, ct_##n); } \
    TEST(n) { prun(#n, (fl)); } \
    static void ct_##n(void)
#define CT(n)  CTF(n, CREATE_NEW_CONSOLE)
#define CTD(n) CTF(n, DETACHED_PROCESS)

/* ---- helpers ---- */
static HANDLE hin_(void)  { return GetStdHandle(STD_INPUT_HANDLE); }
static HANDLE hout_(void) { return GetStdHandle(STD_OUTPUT_HANDLE); }
static COORD co(int x, int y) { COORD c; c.X = (SHORT)x; c.Y = (SHORT)y; return c; }
static SMALL_RECT sr(int l, int t, int r, int b) { SMALL_RECT s; s.Left = (SHORT)l; s.Top = (SHORT)t; s.Right = (SHORT)r; s.Bottom = (SHORT)b; return s; }
static CHAR_INFO ciw(WCHAR ch, WORD at) { CHAR_INFO c; memset(&c, 0, sizeof c); c.Char.UnicodeChar = ch; c.Attributes = at; return c; }
static CHAR_INFO cia(char ch, WORD at) { CHAR_INFO c; memset(&c, 0, sizeof c); c.Char.AsciiChar = ch; c.Attributes = at; return c; }
static HANDLE mkbuf(void) {
    HANDLE h = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    return h == INVALID_HANDLE_VALUE ? NULL : h;
}
static CONSOLE_SCREEN_BUFFER_INFO binfo(HANDLE h) { CONSOLE_SCREEN_BUFFER_INFO i; memset(&i, 0, sizeof i); GetConsoleScreenBufferInfo(h, &i); return i; }
static void clear_buf(HANDLE h) {
    CONSOLE_SCREEN_BUFFER_INFO i = binfo(h); DWORD n;
    FillConsoleOutputCharacterW(h, L' ', i.dwSize.X * i.dwSize.Y, co(0, 0), &n);
    FillConsoleOutputAttribute(h, 7, i.dwSize.X * i.dwSize.Y, co(0, 0), &n);
    SetConsoleCursorPosition(h, co(0, 0));
}
static void g11_putw(HANDLE h, int x, int y, const WCHAR *s) { DWORD n; WriteConsoleOutputCharacterW(h, s, lstrlenW(s), co(x, y), &n); }
static int rowis(HANDLE h, int y, int x0, const WCHAR *want) {
    WCHAR buf[256]; DWORD n = 0; int l = lstrlenW(want);
    if (l > 250) return 0;
    if (!ReadConsoleOutputCharacterW(h, buf, l, co(x0, y), &n) || (int)n != l) return 0;
    buf[l] = 0;
    return !lstrcmpW(buf, want);
}
static WCHAR cellch(HANDLE h, int x, int y) { WCHAR c = 0; DWORD n = 0; ReadConsoleOutputCharacterW(h, &c, 1, co(x, y), &n); return n == 1 ? c : 0xFFFF; }
static WORD cellat(HANDLE h, int x, int y) { WORD a = 0xFFFF; DWORD n = 0; ReadConsoleOutputAttribute(h, &a, 1, co(x, y), &n); return n == 1 ? a : 0xFFFF; }
#endif
