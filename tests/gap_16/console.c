#include "g16.h"
/* All console writes run in a child process that owns a brand new console (CREATE_NEW_CONSOLE)
 * or in a detached child with no console at all. The test exe itself never touches its console. */
static int kc_bad;
static HANDLE out, in; static COORD sz;
#define NOCON 0x7001

static void rdw(int x, int y, int n, WCHAR *b) { COORD c = { (SHORT)x, (SHORT)y }; DWORD got = 0; memset(b, 0, n * 2 + 2); ReadConsoleOutputCharacterW(out, b, n, c, &got); }
static int streqw(int x, int y, const WCHAR *w) { WCHAR b[256]; int n = lstrlenW(w); rdw(x, y, n, b); b[n] = 0; return !lstrcmpW(b, w); }
static WORD attr_at(int x, int y) { WORD a = 0xffff; DWORD got; COORD c = { (SHORT)x, (SHORT)y }; ReadConsoleOutputAttribute(out, &a, 1, c, &got); return a; }
static COORD cur(void) { CONSOLE_SCREEN_BUFFER_INFO i; memset(&i, 0, sizeof i); GetConsoleScreenBufferInfo(out, &i); return i.dwCursorPosition; }
static void gotoxy(int x, int y) { COORD c = { (SHORT)x, (SHORT)y }; SetConsoleCursorPosition(out, c); }
static void clear(void) { DWORD n; COORD z = { 0, 0 }; FillConsoleOutputCharacterW(out, L' ', sz.X * 24, z, &n); FillConsoleOutputAttribute(out, 0x07, sz.X * 24, z, &n); gotoxy(0, 0); }
static void setmode(DWORD m) { SetConsoleMode(out, m); }
#define PW (ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT)

static int setup(void) {
    out = CreateFileA("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    in = CreateFileA("CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (out == INVALID_HANDLE_VALUE || in == INVALID_HANDLE_VALUE) return 0;
    CONSOLE_SCREEN_BUFFER_INFO i; if (!GetConsoleScreenBufferInfo(out, &i)) return 0;
    sz = i.dwSize; if (sz.X < 20) return 0; if (sz.Y < 26) return 0;
    setmode(PW); return 1;
}
#define WCW(s, n, pn) WriteConsoleW_p(out, s, n, pn, NULL)
typedef BOOL (WINAPI *wc_t)(HANDLE, const void *, DWORD, LPDWORD, LPVOID);
#define WCWp ((wc_t)t_proc("WriteConsoleW"))
#define WCAp ((wc_t)t_proc("WriteConsoleA"))
typedef BOOL (WINAPI *wocs_t)(HANDLE, const void *, DWORD, COORD, LPDWORD);
#define WOCW ((wocs_t)t_proc("WriteConsoleOutputCharacterW"))
#define WOCA ((wocs_t)t_proc("WriteConsoleOutputCharacterA"))
#define WOA  ((wocs_t)t_proc("WriteConsoleOutputAttribute"))
typedef BOOL (WINAPI *wob_t)(HANDLE, const CHAR_INFO *, COORD, COORD, PSMALL_RECT);
#define WOW ((wob_t)t_proc("WriteConsoleOutputW"))
#define WOAA ((wob_t)t_proc("WriteConsoleOutputA"))
typedef BOOL (WINAPI *wi_t)(HANDLE, const INPUT_RECORD *, DWORD, LPDWORD);
#define WIW ((wi_t)t_proc("WriteConsoleInputW"))
#define WIA ((wi_t)t_proc("WriteConsoleInputA"))

/* ---- WriteConsoleA / WriteConsoleW ---- */
static DWORD cjkdiag; /* diagnostic: CJK check failure: r<<28 | b[0]<<12 */
static DWORD nwdiag; /* diagnostic: no-wrap cursor Y<<22 | X<<12 when Y != 0 */
static int m_wc(void) {
    DWORD n; BOOL r; WCHAR b[256];
    clear(); n = 99; r = WCWp(out, L"Hello", 5, &n, NULL); KC(r && n == 5); KC(streqw(0, 0, L"Hello")); KC(cur().X == 5 && cur().Y == 0);
    n = 99; r = WCAp(out, "World", 5, &n, NULL); KC(r && n == 5); KC(streqw(0, 0, L"HelloWorld")); KC(cur().X == 10);
    n = 99; r = WCWp(out, L"x", 0, &n, NULL); KC(r && n == 0); KC(cur().X == 10);
    n = 99; r = WCAp(out, "x", 0, &n, NULL); KC(r && n == 0); KC(cur().X == 10);
    r = WCWp(out, L"Q", 1, NULL, NULL); KC(r); KC(streqw(10, 0, L"Q"));
    r = WCAp(out, "R", 1, NULL, NULL); KC(r); KC(streqw(11, 0, L"R"));
    n = 99; r = WCWp(out, L"abcdef", 3, &n, NULL); KC(r && n == 3); KC(streqw(12, 0, L"abc")); rdw(15, 0, 1, b); KC(b[0] == L' ');
    n = 99; r = WCAp(out, "abcdef", 2, &n, NULL); KC(r && n == 2);
    n = 99; r = WCWp(out, NULL, 0, &n, NULL); printf("NULL buffer len 0: %d n=%lu\n", r, n);
    /* control characters */
    clear(); WCWp(out, L"xy\nz", 4, &n, NULL); KC(n == 4); KC(streqw(0, 0, L"xy")); KC(streqw(0, 1, L"z")); KC(cur().X == 1 && cur().Y == 1);
    clear(); WCWp(out, L"ab\rc", 4, &n, NULL); KC(streqw(0, 0, L"cb")); KC(cur().X == 1 && cur().Y == 0);
    clear(); WCWp(out, L"ab\bc", 4, &n, NULL); KC(streqw(0, 0, L"ac")); KC(cur().X == 2);
    clear(); WCWp(out, L"a\tb", 3, &n, NULL); KC(n == 3); rdw(8, 0, 1, b); KC(b[0] == L'b'); KC(cur().X == 9);
    clear(); WCWp(out, L"\n\n\n", 3, &n, NULL); KC(cur().Y == 3 && cur().X == 0);
    clear(); WCAp(out, "ab\r\nc", 5, &n, NULL); KC(n == 5); KC(streqw(0, 0, L"ab")); KC(streqw(0, 1, L"c"));
    /* wrapping */
    clear(); { WCHAR *big = HeapAlloc(GetProcessHeap(), 0, (sz.X + 4) * 2); for (int i = 0; i < sz.X + 3; i++) big[i] = L'a' + (i % 26);
        n = 0; r = WCWp(out, big, sz.X + 3, &n, NULL); KC(r && n == (DWORD)sz.X + 3); KC(cur().X == 3 && cur().Y == 1);
        rdw(0, 1, 3, b); KC(b[0] == big[sz.X] && b[2] == big[sz.X + 2]); HeapFree(GetProcessHeap(), 0, big); }
    /* no wrap: processed output only */
    clear(); setmode(ENABLE_PROCESSED_OUTPUT); gotoxy(sz.X - 2, 0); WCWp(out, L"abcd", 4, &n, NULL); KC(n == 4);
    { COORD cc = cur(); printf("no-wrap cursor after overflow: %d,%d\n", cc.X, cc.Y); KC(cc.Y == 0); if (cc.Y != 0) nwdiag = ((DWORD)(cc.Y & 0x3ff) << 22) | ((DWORD)(cc.X & 0x3ff) << 12); } setmode(PW);
    /* large write */
    clear(); { DWORD big = 20000; WCHAR *p = HeapAlloc(GetProcessHeap(), 0, big * 2); for (DWORD i = 0; i < big; i++) p[i] = L'x'; n = 0; r = WCWp(out, p, big, &n, NULL); KC(r && n == big); HeapFree(GetProcessHeap(), 0, p); }
    clear(); { DWORD big = 70000; char *p = HeapAlloc(GetProcessHeap(), 0, big); memset(p, 'y', big); n = 0; r = WCAp(out, p, big, &n, NULL); printf("70000 A: r=%d n=%lu\n", r, n); KC(r); HeapFree(GetProcessHeap(), 0, p); }
    /* code pages for the ANSI entry */
    clear(); if (SetConsoleOutputCP(1252)) { gotoxy(0, 0); n = 0; r = WCAp(out, "\xe9", 1, &n, NULL); KC(r && n == 1); rdw(0, 0, 1, b); KC(b[0] == 0x00e9); }
    clear(); if (SetConsoleOutputCP(437)) { gotoxy(0, 0); WCAp(out, "\xe9", 1, &n, NULL); rdw(0, 0, 1, b); KC(b[0] == 0x0398); }
    clear(); if (SetConsoleOutputCP(65001)) { gotoxy(0, 0); n = 0; r = WCAp(out, "\xc3\xa9z", 3, &n, NULL); KC(r); printf("utf8 n=%lu\n", n); rdw(0, 0, 2, b); KC(b[0] == 0x00e9 && b[1] == L'z');
        gotoxy(0, 1); r = WCAp(out, "\xe4\xb8\xad", 3, &n, NULL); KC(r); rdw(0, 1, 1, b); KC(b[0] == 0x4e2d); if (!r || b[0] != 0x4e2d) cjkdiag = ((DWORD)(r ? 1 : 0) << 28) | ((DWORD)b[0] << 12); printf("cjk cursor x=%d\n", cur().X);
        gotoxy(0, 2); r = WCAp(out, "\xff", 1, &n, NULL); printf("invalid utf8 -> %d n=%lu\n", r, n); }
    /* unicode through the W entry */
    clear(); SetConsoleOutputCP(437); WCWp(out, L"\x00e9\x20ac\x00df", 3, &n, NULL); KC(n == 3); rdw(0, 0, 3, b); KC(b[0] == 0x00e9 && b[1] == 0x20ac && b[2] == 0x00df);
    /* embedded NUL */
    clear(); n = 0; r = WCWp(out, L"a\0b", 3, &n, NULL); printf("embedded NUL: r=%d n=%lu cur=%d\n", r, n, cur().X); KC(r);
    /* VT sequences */
    clear(); if (SetConsoleMode(out, PW | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
        gotoxy(0, 0); WCWp(out, L"\x1b[3;5H", lstrlenW(L"\x1b[3;5H"), &n, NULL); KC(cur().X == 4 && cur().Y == 2);
        WCAp(out, "\x1b[31mR\x1b[0m", 10, &n, NULL); KC((attr_at(4, 2) & 0x0f) == FOREGROUND_RED);
        WCWp(out, L"\x1b[1;1H\x1b[42mG", 12, &n, NULL); KC((attr_at(0, 0) & 0xf0) == BACKGROUND_GREEN);
        WCWp(out, L"\x1b[2J", 4, &n, NULL); KC(streqw(0, 0, L" "));
        WCWp(out, L"\x1b[10;20H", 8, &n, NULL); KC(cur().X == 19 && cur().Y == 9);
        WCWp(out, L"\x1b[2A\x1b[3C", 8, &n, NULL); KC(cur().X == 22 && cur().Y == 7);
        setmode(PW);
    } else printf("no VT support\n");
    /* disabled processing: control characters are glyphs */
    clear(); setmode(0); gotoxy(0, 0); WCWp(out, L"a\rb", 3, &n, NULL); printf("raw mode CR: cursor x=%d y=%d\n", cur().X, cur().Y); KC(n == 3); KC(cur().Y == 0); setmode(PW);
    return kc_bad;
}
static int m_wc_err(void) {
    DWORD n = 77; BOOL r; DWORD e; HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL), dead = CreateEventA(NULL, TRUE, FALSE, NULL); CloseHandle(dead);
    HANDLE hs[] = { NULL, INVALID_HANDLE_VALUE, dead, ev, (HANDLE)(LONG_PTR)-11 };
    char tmp[MAX_PATH]; GetTempPathA(MAX_PATH, tmp); strcat(tmp, "g16_wc_err.txt");
    HANDLE f = CreateFileA(tmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, NULL);
    for (int i = 0; i < 6; i++) {
        HANDLE h = i < 5 ? hs[i] : f; if (i == 4) continue;
        SetLastError(0); n = 77; r = WCWp(h, L"hi", 2, &n, NULL); e = GetLastError(); KC(!r); KC(e == ERROR_INVALID_HANDLE);
        SetLastError(0); n = 77; r = WCAp(h, "hi", 2, &n, NULL); e = GetLastError(); KC(!r); KC(e == ERROR_INVALID_HANDLE);
    }
    SetLastError(0); r = WCWp(in, L"hi", 2, &n, NULL); e = GetLastError(); KC(!r); printf("write to input buffer handle: err %lu\n", e);
    SetLastError(0); r = WCAp(in, "hi", 2, &n, NULL); KC(!r);
    CloseHandle(ev); CloseHandle(f);
    return kc_bad;
}
/* ---- WriteConsoleOutputCharacter / Attribute ---- */
static int m_woc(void) {
    DWORD n; BOOL r; WCHAR b[256]; COORD c; WORD at[4] = { 0x1E, 0x2F, 0x4A, 0x70 }, back[4];
    clear(); gotoxy(7, 7); c.X = 2; c.Y = 3; n = 99;
    r = WOCW(out, L"Hi!", 3, c, &n); KC(r && n == 3); KC(streqw(2, 3, L"Hi!")); KC(cur().X == 7 && cur().Y == 7);
    c.X = 2; c.Y = 4; n = 99; r = WOCA(out, "abc", 3, c, &n); KC(r && n == 3); KC(streqw(2, 4, L"abc")); KC(cur().X == 7 && cur().Y == 7);
    c.X = 0; c.Y = 5; n = 99; r = WOCW(out, L"zzz", 0, c, &n); KC(r && n == 0); KC(streqw(0, 5, L" "));
    n = 99; r = WOCA(out, "zzz", 0, c, &n); KC(r && n == 0);
    c.X = 0; c.Y = 5; r = WOCA(out, "1234", 2, c, &n); KC(r && n == 2); KC(streqw(0, 5, L"12 "));
    c.X = 0; c.Y = 5; r = WOCW(out, L"WXYZ", 4, c, &n); KC(r && n == 4);   /* lpNumberOfCharsWritten is required: Win10 writes it unconditionally */
    /* clipping at end of buffer */
    c.X = sz.X - 2; c.Y = sz.Y - 1; n = 99; r = WOCW(out, L"0123456789", 10, c, &n); KC(r); printf("end-of-buffer clip n=%lu\n", n); KC(n == 2);
    c.X = sz.X - 2; c.Y = sz.Y - 1; n = 99; r = WOCA(out, "0123456789", 10, c, &n); KC(r && n == 2);
    /* wrap onto next row */
    clear(); c.X = sz.X - 2; c.Y = 0; n = 99; r = WOCW(out, L"ABCDE", 5, c, &n); KC(r && n == 5); KC(streqw(sz.X - 2, 0, L"AB")); KC(streqw(0, 1, L"CDE"));
    /* attributes */
    clear(); c.X = 0; c.Y = 5; WOCW(out, L"text", 4, c, &n); n = 99; r = WOA(out, at, 4, c, &n); KC(r && n == 4);
    for (int i = 0; i < 4; i++) { back[i] = attr_at(i, 5); } KC(!memcmp(back, at, sizeof at)); KC(streqw(0, 5, L"text"));
    c.X = 1; c.Y = 5; n = 99; r = WOA(out, at, 0, c, &n); KC(r && n == 0);
    c.X = 4; c.Y = 5; WORD a2 = 0x0C; WOCW(out, L"M", 1, c, &n); KC(attr_at(4, 5) == 0x07); WOA(out, &a2, 1, c, &n); WOCW(out, L"N", 1, c, &n); KC(attr_at(4, 5) == 0x0C);   /* char writes keep attributes */
    c.X = sz.X - 2; c.Y = sz.Y - 1; { WORD five[5] = { 1, 2, 3, 4, 5 }; n = 99; r = WOA(out, five, 5, c, &n); KC(r && n == 2); }
    c.X = sz.X - 2; c.Y = 6; { WORD five[5] = { 0x11, 0x22, 0x33, 0x44, 0x55 }; n = 99; r = WOA(out, five, 5, c, &n); KC(r && n == 5); KC(attr_at(sz.X - 1, 6) == 0x22); KC(attr_at(0, 7) == 0x33); KC(attr_at(2, 7) == 0x55); }
    { WORD odd = 0xFFFF; c.X = 0; c.Y = 8; n = 0; r = WOA(out, &odd, 1, c, &n); KC(r && n == 1); printf("attr 0xffff reads back 0x%x\n", attr_at(0, 8)); }
    { WORD odd = 0x0100; c.X = 1; c.Y = 8; n = 0; r = WOA(out, &odd, 1, c, &n); KC(r); printf("attr 0x0100 reads back 0x%x\n", attr_at(1, 8)); }
    /* out-of-range coordinates */
    { COORD bad[] = { { (SHORT)sz.X, 0 }, { 0, (SHORT)sz.Y }, { -1, 0 }, { 0, -1 }, { 32767, 32767 }, { -32768, -32768 } };
      for (int i = 0; i < 6; i++) { SetLastError(0); n = 99; r = WOCW(out, L"x", 1, bad[i], &n); DWORD e = GetLastError(); printf("char coord %d,%d -> r=%d n=%lu err=%lu\n", bad[i].X, bad[i].Y, r, n, e);
          if (!r) KC(e == ERROR_INVALID_PARAMETER || e == ERROR_INVALID_HANDLE);
          SetLastError(0); n = 99; r = WOA(out, at, 1, bad[i], &n); e = GetLastError(); printf("attr coord %d,%d -> r=%d n=%lu err=%lu\n", bad[i].X, bad[i].Y, r, n, e); } }
    /* ANSI code page conversion */
    clear(); if (SetConsoleOutputCP(1252)) { c.X = 0; c.Y = 0; n = 0; r = WOCA(out, "\xe9\xfc", 2, c, &n); KC(r && n == 2); rdw(0, 0, 2, b); KC(b[0] == 0x00e9 && b[1] == 0x00fc); }
    clear(); if (SetConsoleOutputCP(437)) { c.X = 0; c.Y = 0; WOCA(out, "\xe9", 1, c, &n); rdw(0, 0, 1, b); KC(b[0] == 0x0398); }
    /* unicode */
    clear(); c.X = 0; c.Y = 0; WOCW(out, L"\x00e9\x20ac", 2, c, &n); KC(n == 2); rdw(0, 0, 2, b); KC(b[0] == 0xe9 && b[1] == 0x20ac);
    /* long run */
    clear(); { WCHAR *p = HeapAlloc(GetProcessHeap(), 0, 2000 * 2); for (int i = 0; i < 2000; i++) p[i] = L'A' + (i % 26); c.X = 0; c.Y = 0; n = 0; r = WOCW(out, p, 2000, c, &n);
        KC(r && n == 2000); HeapFree(GetProcessHeap(), 0, p); }
    return kc_bad;
}
static int m_woc_err(void) {
    DWORD n = 77, e; BOOL r; COORD c = { 0, 0 }; WORD a = 7; HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL), dead = CreateEventA(NULL, TRUE, FALSE, NULL); CloseHandle(dead);
    HANDLE hs[] = { NULL, INVALID_HANDLE_VALUE, dead, ev };
    for (int i = 0; i < 4; i++) {
        SetLastError(0); r = WOCW(hs[i], L"x", 1, c, &n); e = GetLastError(); KC(!r && e == ERROR_INVALID_HANDLE);
        SetLastError(0); r = WOCA(hs[i], "x", 1, c, &n); e = GetLastError(); KC(!r && e == ERROR_INVALID_HANDLE);
        SetLastError(0); r = WOA(hs[i], &a, 1, c, &n); e = GetLastError(); KC(!r && e == ERROR_INVALID_HANDLE);
    }
    SetLastError(0); r = WOCW(in, L"x", 1, c, &n); printf("char write to input handle: r=%d err=%lu\n", r, GetLastError()); KC(!r);
    SetLastError(0); r = WOA(in, &a, 1, c, &n); printf("attr write to input handle: r=%d err=%lu\n", r, GetLastError()); KC(!r);
    CloseHandle(ev); return kc_bad;
}
/* ---- WriteConsoleOutputA / W ---- */
static CHAR_INFO ci(WCHAR ch, WORD at) { CHAR_INFO c; memset(&c, 0, sizeof c); c.Char.UnicodeChar = ch; c.Attributes = at; return c; }
static int m_wob(void) {
    BOOL r; WCHAR b[16]; CHAR_INFO buf[6] = { ci(L'a', 0x1F), ci(L'b', 0x2F), ci(L'c', 0x3F), ci(L'd', 0x4F), ci(L'e', 0x5F), ci(L'f', 0x6F) };
    COORD bs = { 3, 2 }, bc = { 0, 0 }; SMALL_RECT rg = { 10, 5, 12, 6 };
    clear(); gotoxy(1, 1); r = WOW(out, buf, bs, bc, &rg); KC(r); KC(rg.Left == 10 && rg.Top == 5 && rg.Right == 12 && rg.Bottom == 6);
    KC(streqw(10, 5, L"abc")); KC(streqw(10, 6, L"def")); KC(attr_at(10, 5) == 0x1F && attr_at(12, 6) == 0x6F && attr_at(11, 6) == 0x5F); KC(cur().X == 1 && cur().Y == 1);
    /* sub-rectangle of the source */
    clear(); bc.X = 1; bc.Y = 0; rg.Left = 0; rg.Top = 0; rg.Right = 1; rg.Bottom = 1; r = WOW(out, buf, bs, bc, &rg); KC(r); KC(streqw(0, 0, L"bc")); KC(streqw(0, 1, L"ef")); KC(rg.Right == 1 && rg.Bottom == 1);
    clear(); bc.X = 0; bc.Y = 1; rg.Left = 3; rg.Top = 3; rg.Right = 5; rg.Bottom = 3; r = WOW(out, buf, bs, bc, &rg); KC(r); KC(streqw(3, 3, L"def"));
    /* region smaller than the source: only the region is written */
    clear(); bc.X = 0; bc.Y = 0; rg.Left = 0; rg.Top = 0; rg.Right = 0; rg.Bottom = 0; r = WOW(out, buf, bs, bc, &rg); KC(r); KC(streqw(0, 0, L"a ")); 
    /* clipping against the right/bottom edge reports the written rectangle */
    clear(); bc.X = 0; bc.Y = 0; rg.Left = sz.X - 1; rg.Top = 0; rg.Right = sz.X + 1; rg.Bottom = 1; r = WOW(out, buf, bs, bc, &rg); KC(r);
    printf("clipped region %d,%d,%d,%d\n", rg.Left, rg.Top, rg.Right, rg.Bottom); KC(rg.Left == sz.X - 1 && rg.Right == sz.X - 1 && rg.Top == 0 && rg.Bottom == 1); KC(streqw(sz.X - 1, 0, L"a")); KC(streqw(sz.X - 1, 1, L"d"));
    clear(); rg.Left = 2; rg.Top = sz.Y - 1; rg.Right = 4; rg.Bottom = sz.Y; r = WOW(out, buf, bs, bc, &rg); KC(r); KC(rg.Bottom == sz.Y - 1); KC(streqw(2, sz.Y - 1, L"abc"));
    /* negative left/top */
    clear(); rg.Left = -1; rg.Top = 0; rg.Right = 1; rg.Bottom = 0; SetLastError(0); r = WOW(out, buf, bs, bc, &rg); printf("negative left: r=%d err=%lu region %d,%d,%d,%d\n", r, GetLastError(), rg.Left, rg.Top, rg.Right, rg.Bottom);
    /* completely outside */
    rg.Left = sz.X + 5; rg.Top = 0; rg.Right = sz.X + 7; rg.Bottom = 1; SetLastError(0); r = WOW(out, buf, bs, bc, &rg); printf("outside: r=%d err=%lu region %d,%d,%d,%d\n", r, GetLastError(), rg.Left, rg.Top, rg.Right, rg.Bottom);
    /* inverted region */
    rg.Left = 5; rg.Top = 5; rg.Right = 3; rg.Bottom = 5; SetLastError(0); r = WOW(out, buf, bs, bc, &rg); printf("inverted: r=%d err=%lu\n", r, GetLastError());
    /* buffer coordinate / size errors */
    { COORD bad_bc[] = { { 3, 0 }, { 0, 2 }, { 4, 4 }, { -1, 0 } };
      for (int i = 0; i < 4; i++) { if (i == 3 && t_is_wine()) continue; rg.Left = 0; rg.Top = 0; rg.Right = 1; rg.Bottom = 0; SetLastError(0); r = WOW(out, buf, bs, bad_bc[i], &rg); printf("bufcoord %d,%d: r=%d err=%lu\n", bad_bc[i].X, bad_bc[i].Y, r, GetLastError()); } }
    { COORD zero = { 0, 0 }; rg.Left = 0; rg.Top = 0; rg.Right = 1; rg.Bottom = 0; SetLastError(0); r = WOW(out, buf, zero, bc, &rg); printf("zero buffer size: r=%d err=%lu\n", r, GetLastError()); }
    /* single cell, and a full 10x5 block */
    clear(); { CHAR_INFO one = ci(L'Q', 0x4E); COORD one_sz = { 1, 1 }, z = { 0, 0 }; SMALL_RECT g = { 7, 7, 7, 7 }; r = WOW(out, &one, one_sz, z, &g); KC(r); KC(streqw(7, 7, L"Q")); KC(attr_at(7, 7) == 0x4E); }
    clear(); { CHAR_INFO blk[50]; for (int i = 0; i < 50; i++) blk[i] = ci(L'0' + (i % 10), (WORD)(0x10 + (i % 8))); COORD s2 = { 10, 5 }, z = { 0, 0 }; SMALL_RECT g = { 0, 10, 9, 14 };
        r = WOW(out, blk, s2, z, &g); KC(r); for (int y = 0; y < 5; y++) { rdw(0, 10 + y, 10, b); KC(b[0] == L'0' + ((y * 10) % 10) && b[9] == L'9'); } KC(attr_at(3, 12) == (WORD)(0x10 + ((23) % 8))); }
    /* ANSI */
    { CHAR_INFO ab[2]; memset(ab, 0, sizeof ab); ab[0].Char.AsciiChar = (CHAR)0xe9; ab[0].Attributes = 0x1F; ab[1].Char.AsciiChar = 'z'; ab[1].Attributes = 0x2F; COORD s2 = { 2, 1 }, z = { 0, 0 };
      clear(); if (SetConsoleOutputCP(1252)) { SMALL_RECT g = { 0, 0, 1, 0 }; r = WOAA(out, ab, s2, z, &g); KC(r); rdw(0, 0, 2, b); KC(b[0] == 0x00e9 && b[1] == L'z'); KC(attr_at(0, 0) == 0x1F); KC(g.Right == 1); }
      clear(); if (SetConsoleOutputCP(437)) { SMALL_RECT g = { 0, 0, 1, 0 }; r = WOAA(out, ab, s2, z, &g); KC(r); rdw(0, 0, 1, b); KC(b[0] == 0x0398); }
      clear(); { SMALL_RECT g = { 0, 0, 1, 0 }; CHAR_INFO plain[2]; memset(plain, 0, sizeof plain); plain[0].Char.AsciiChar = 'A'; plain[1].Char.AsciiChar = 'B'; plain[0].Attributes = plain[1].Attributes = 7;
        r = WOAA(out, plain, s2, z, &g); KC(r); KC(streqw(0, 0, L"AB")); } }
    return kc_bad;
}
static int m_wob_err(void) {
    CHAR_INFO c1 = ci(L'x', 7); COORD s = { 1, 1 }, z = { 0, 0 }; SMALL_RECT g = { 0, 0, 0, 0 }; DWORD e; BOOL r;
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL), dead = CreateEventA(NULL, TRUE, FALSE, NULL); CloseHandle(dead); HANDLE hs[] = { NULL, INVALID_HANDLE_VALUE, dead, ev };
    for (int i = 0; i < 4; i++) {
        g.Left = g.Top = g.Right = g.Bottom = 0; SetLastError(0); r = WOW(hs[i], &c1, s, z, &g); e = GetLastError(); KC(!r && e == ERROR_INVALID_HANDLE); if (kc_bad) return (e & 0xfff) << 20 | i << 16 | 0x8000 | kc_bad;
        g.Left = g.Top = g.Right = g.Bottom = 0; SetLastError(0); r = WOAA(hs[i], &c1, s, z, &g); e = GetLastError(); KC(!r && e == ERROR_INVALID_HANDLE); if (kc_bad) return (e & 0xfff) << 20 | i << 16 | kc_bad;
    }
    SetLastError(0); r = WOW(in, &c1, s, z, &g); printf("WriteConsoleOutputW to input handle: r=%d err=%lu\n", r, GetLastError()); KC(!r);
    CloseHandle(ev); return kc_bad;
}
/* ---- WriteConsoleInputA / W ---- */
static INPUT_RECORD key(WCHAR ch, BOOL down, WORD vk) { INPUT_RECORD r; memset(&r, 0, sizeof r); r.EventType = KEY_EVENT; r.Event.KeyEvent.bKeyDown = down; r.Event.KeyEvent.wRepeatCount = 1; r.Event.KeyEvent.wVirtualKeyCode = vk; r.Event.KeyEvent.wVirtualScanCode = 0x1e; r.Event.KeyEvent.uChar.UnicodeChar = ch; return r; }
static DWORD qlen(void) { DWORD n = 0xdead; GetNumberOfConsoleInputEvents(in, &n); return n; }
static int m_wi(void) {
    DWORD n; BOOL r; INPUT_RECORD rec[600], pk[600];
    FlushConsoleInputBuffer(in); KC(qlen() == 0);
    rec[0] = key(L'a', TRUE, 'A'); rec[1] = key(L'a', FALSE, 'A'); rec[2] = key(L'b', TRUE, 'B');
    n = 99; r = WIW(in, rec, 3, &n); KC(r && n == 3); KC(qlen() == 3);
    { DWORD got = 0; memset(pk, 0, sizeof pk); PeekConsoleInputW(in, pk, 10, &got); KC(got == 3); KC(!memcmp(pk, rec, 3 * sizeof rec[0]) || (pk[0].EventType == KEY_EVENT && pk[2].Event.KeyEvent.uChar.UnicodeChar == L'b')); KC(pk[0].Event.KeyEvent.bKeyDown == TRUE && pk[1].Event.KeyEvent.bKeyDown == FALSE); }
    FlushConsoleInputBuffer(in); KC(qlen() == 0);
    /* zero length */
    n = 99; r = WIW(in, rec, 0, &n); printf("WriteConsoleInputW len 0: r=%d n=%lu err=%lu\n", r, n, GetLastError()); KC(qlen() == 0);
    n = 99; r = WIA(in, rec, 0, &n); printf("WriteConsoleInputA len 0: r=%d n=%lu\n", r, n); KC(qlen() == 0);
    /* ANSI conversion honours the input code page */
    { INPUT_RECORD a[2]; memset(a, 0, sizeof a); a[0].EventType = KEY_EVENT; a[0].Event.KeyEvent.bKeyDown = TRUE; a[0].Event.KeyEvent.wRepeatCount = 1; a[0].Event.KeyEvent.uChar.AsciiChar = 'z';
      a[1] = a[0]; a[1].Event.KeyEvent.uChar.AsciiChar = (CHAR)0xe9;
      if (SetConsoleCP(1252)) { n = 99; r = WIA(in, a, 2, &n); KC(r && n == 2); DWORD got = 0; PeekConsoleInputW(in, pk, 10, &got); KC(got == 2); KC(pk[0].Event.KeyEvent.uChar.UnicodeChar == L'z'); KC(pk[1].Event.KeyEvent.uChar.UnicodeChar == 0x00e9);
          got = 0; PeekConsoleInputA(in, pk, 10, &got); KC(got == 2 && (BYTE)pk[1].Event.KeyEvent.uChar.AsciiChar == 0xe9); FlushConsoleInputBuffer(in); }
      if (SetConsoleCP(437)) { n = 99; r = WIA(in, a, 2, &n); KC(r); DWORD got = 0; PeekConsoleInputW(in, pk, 10, &got); KC(got == 2 && pk[1].Event.KeyEvent.uChar.UnicodeChar == 0x0398); FlushConsoleInputBuffer(in); } }
    /* unicode characters beyond ANSI */
    rec[0] = key(0x20ac, TRUE, 0); rec[1] = key(0x4e2d, TRUE, 0); n = 0; r = WIW(in, rec, 2, &n); KC(r && n == 2); { DWORD got; PeekConsoleInputW(in, pk, 10, &got); KC(got == 2 && pk[0].Event.KeyEvent.uChar.UnicodeChar == 0x20ac && pk[1].Event.KeyEvent.uChar.UnicodeChar == 0x4e2d); } FlushConsoleInputBuffer(in);
    /* other record types */
    { INPUT_RECORD m[4]; memset(m, 0, sizeof m); m[0].EventType = MOUSE_EVENT; m[0].Event.MouseEvent.dwMousePosition.X = 5; m[0].Event.MouseEvent.dwMousePosition.Y = 6; m[0].Event.MouseEvent.dwButtonState = FROM_LEFT_1ST_BUTTON_PRESSED;
      m[1].EventType = FOCUS_EVENT; m[1].Event.FocusEvent.bSetFocus = TRUE; m[2].EventType = MENU_EVENT; m[2].Event.MenuEvent.dwCommandId = 42; m[3].EventType = WINDOW_BUFFER_SIZE_EVENT; m[3].Event.WindowBufferSizeEvent.dwSize.X = 90; m[3].Event.WindowBufferSizeEvent.dwSize.Y = 33;
      n = 0; r = WIW(in, m, 4, &n); KC(r); printf("mixed record types: n=%lu queue=%lu\n", n, qlen()); DWORD got = 0; PeekConsoleInputW(in, pk, 10, &got);
      if (got >= 1) KC(pk[0].EventType == MOUSE_EVENT && pk[0].Event.MouseEvent.dwMousePosition.X == 5 && pk[0].Event.MouseEvent.dwMousePosition.Y == 6 && pk[0].Event.MouseEvent.dwButtonState == FROM_LEFT_1ST_BUTTON_PRESSED);
      if (got >= 2) KC(pk[1].EventType == FOCUS_EVENT && pk[1].Event.FocusEvent.bSetFocus == TRUE);
      if (got >= 3) KC(pk[2].EventType == MENU_EVENT && pk[2].Event.MenuEvent.dwCommandId == 42);
      if (got >= 4) KC(pk[3].EventType == WINDOW_BUFFER_SIZE_EVENT && pk[3].Event.WindowBufferSizeEvent.dwSize.X == 90);
      FlushConsoleInputBuffer(in); }
    /* append order */
    rec[0] = key(L'1', TRUE, '1'); rec[1] = key(L'2', TRUE, '2'); WIW(in, rec, 2, &n); rec[0] = key(L'3', TRUE, '3'); rec[1] = key(L'4', TRUE, '4'); WIW(in, rec, 2, &n);
    { DWORD got = 0; PeekConsoleInputW(in, pk, 10, &got); KC(got == 4); for (DWORD i = 0; i < got; i++) KC(pk[i].Event.KeyEvent.uChar.UnicodeChar == L'1' + i); } FlushConsoleInputBuffer(in);
    /* many events */
    for (int i = 0; i < 600; i++) rec[i] = key(L'a' + (i % 26), (i & 1) == 0, 'A' + (i % 26));
    n = 0; r = WIW(in, rec, 500, &n); KC(r && n == 500); KC(qlen() == 500); { DWORD got = 0; PeekConsoleInputW(in, pk, 600, &got); KC(got == 500); KC(pk[499].Event.KeyEvent.uChar.UnicodeChar == rec[499].Event.KeyEvent.uChar.UnicodeChar); }
    n = 0; r = WIW(in, rec, 600, &n); KC(r && n == 600); KC(qlen() == 1100);
    FlushConsoleInputBuffer(in); KC(qlen() == 0);
    /* repeat count and control state survive */
    rec[0] = key(L'r', TRUE, 'R'); rec[0].Event.KeyEvent.wRepeatCount = 7; rec[0].Event.KeyEvent.dwControlKeyState = SHIFT_PRESSED | CAPSLOCK_ON; rec[0].Event.KeyEvent.wVirtualScanCode = 0x13;
    WIW(in, rec, 1, &n); { DWORD got = 0; PeekConsoleInputW(in, pk, 4, &got); KC(got == 1 && pk[0].Event.KeyEvent.wRepeatCount == 7 && pk[0].Event.KeyEvent.dwControlKeyState == (SHIFT_PRESSED | CAPSLOCK_ON) && pk[0].Event.KeyEvent.wVirtualScanCode == 0x13); } FlushConsoleInputBuffer(in);
    /* unknown event type */
    { INPUT_RECORD u; memset(&u, 0, sizeof u); u.EventType = 0x7777; SetLastError(0); n = 99; r = WIW(in, &u, 1, &n); printf("unknown event type: r=%d n=%lu err=%lu queue=%lu\n", r, n, GetLastError(), qlen()); FlushConsoleInputBuffer(in); }
    /* output handle is the wrong type */
    SetLastError(0); n = 99; r = WIW(out, rec, 1, &n); printf("WriteConsoleInputW to output handle: r=%d err=%lu\n", r, GetLastError()); KC(!r);
    SetLastError(0); r = WIA(out, rec, 1, &n); KC(!r);
    return kc_bad;
}
static int m_wi_err(void) {
    INPUT_RECORD k = key(L'x', TRUE, 'X'); DWORD n = 77, e; BOOL r; HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL), dead = CreateEventA(NULL, TRUE, FALSE, NULL); CloseHandle(dead); HANDLE hs[] = { NULL, INVALID_HANDLE_VALUE, dead, ev };
    for (int i = 0; i < 4; i++) {
        SetLastError(0); r = WIW(hs[i], &k, 1, &n); e = GetLastError(); KC(!r && e == ERROR_INVALID_HANDLE);
        SetLastError(0); r = WIA(hs[i], &k, 1, &n); e = GetLastError(); KC(!r && e == ERROR_INVALID_HANDLE);
    }
    CloseHandle(ev); return kc_bad;
}
/* ---- no console at all ---- */
static int m_nocon(void) {
    HANDLE nul = CreateFileA("NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    DWORD n = 77, e; BOOL r; COORD c = { 0, 0 }; WORD a = 7; INPUT_RECORD k = key(L'x', TRUE, 'X'); CHAR_INFO ch = ci(L'x', 7); COORD s = { 1, 1 }; SMALL_RECT g = { 0, 0, 0, 0 };
    HANDLE hs[] = { nul, NULL, INVALID_HANDLE_VALUE };
    for (int i = 0; i < 3; i++) {
        SetLastError(0); r = WCWp(hs[i], L"x", 1, &n, NULL); e = GetLastError(); KC(!r && e == ERROR_INVALID_HANDLE);
        SetLastError(0); r = WCAp(hs[i], "x", 1, &n, NULL); e = GetLastError(); KC(!r && e == ERROR_INVALID_HANDLE);
        SetLastError(0); r = WOCW(hs[i], L"x", 1, c, &n); e = GetLastError(); KC(!r && e == ERROR_INVALID_HANDLE);
        SetLastError(0); r = WOCA(hs[i], "x", 1, c, &n); e = GetLastError(); KC(!r && e == ERROR_INVALID_HANDLE);
        SetLastError(0); r = WOA(hs[i], &a, 1, c, &n); e = GetLastError(); KC(!r && e == ERROR_INVALID_HANDLE);
        SetLastError(0); r = WOW(hs[i], &ch, s, c, &g); e = GetLastError(); KC(!r && e == ERROR_INVALID_HANDLE);
        SetLastError(0); r = WOAA(hs[i], &ch, s, c, &g); e = GetLastError(); KC(!r && e == ERROR_INVALID_HANDLE);
        SetLastError(0); r = WIW(hs[i], &k, 1, &n); e = GetLastError(); KC(!r && e == ERROR_INVALID_HANDLE);
        SetLastError(0); r = WIA(hs[i], &k, 1, &n); e = GetLastError(); KC(!r && e == ERROR_INVALID_HANDLE);
    }
    return kc_bad;
}

CHILD(gap16_con) {
    const char *m = argc > 1 ? argv[1] : "";
    if (!strcmp(m, "nocon")) return m_nocon();
    if (!setup()) return NOCON;
    if (!strcmp(m, "wc")) { int rc = m_wc(); kc_dump(); return (int)((DWORD)rc | nwdiag | cjkdiag); }
    if (!strcmp(m, "wcerr")) return m_wc_err();
    if (!strcmp(m, "woc")) { int rc = m_woc(); kc_dump(); return rc; }
    if (!strcmp(m, "wocerr")) return m_woc_err();
    if (!strcmp(m, "wob")) { int rc = m_wob(); kc_dump(); return rc; }
    if (!strcmp(m, "woberr")) return m_wob_err();
    if (!strcmp(m, "wi")) { int rc = m_wi(); kc_dump(); return rc; }
    if (!strcmp(m, "wierr")) return m_wi_err();
    return 0xC0DE;
}
static DWORD run_con(const char *mode, DWORD flags) {
    PROCESS_INFORMATION pi; DWORD c = 0xDEAD0000;
    HANDLE h = t_spawn_child("gap16_con", mode, flags, NULL, NULL, NULL, FALSE, NULL, &pi); if (!h) return 0xDEAD0000;
    if (WaitForSingleObject(h, 25000) != WAIT_OBJECT_0) { TerminateProcess(h, 1); WaitForSingleObject(h, 5000); c = 0xDEAD0001; } else GetExitCodeProcess(h, &c);
    CloseHandle(pi.hThread); CloseHandle(h); return c;
}
static void con_test(const char *mode) {
    DWORD c = run_con(mode, CREATE_NEW_CONSOLE);
    if (c == NOCON || c == 0xDEAD0000 || c == 0xDEAD0001) { if (t_is_wine()) { t_skip("console child unavailable (0x%lx)", c); return; } }
    CHECKF(c == 0, "console child '%s' result %lu (0x%lx, line %lu, no-wrap cursor x=%lu y=%lu if mode wc; cjk diag r=%lu b0=0x%lx if line 74)", mode, c, c, c & 0xfff, (c >> 12) & 0x3ff, c >> 22, (c >> 28) & 1, (c >> 12) & 0xffff);
}
#define CTEST(name, mode, need) TEST(name) { REQUIRE(t_proc(need)); con_test(mode); }
CTEST(gap16_conw_write_console_basic_controls_wrap_vt_codepages, "wc", "WriteConsoleW")
CTEST(gap16_conw_write_console_invalid_handles, "wcerr", "WriteConsoleA")
CTEST(gap16_conw_output_character_and_attribute, "woc", "WriteConsoleOutputCharacterW")
CTEST(gap16_conw_output_character_attribute_invalid_handles, "wocerr", "WriteConsoleOutputAttribute")
CTEST(gap16_conw_write_console_output_rects_and_ansi, "wob", "WriteConsoleOutputW")
CTEST(gap16_conw_write_console_output_invalid_handles, "woberr", "WriteConsoleOutputA")
CTEST(gap16_conw_write_console_input_events, "wi", "WriteConsoleInputW")
CTEST(gap16_conw_write_console_input_invalid_handles, "wierr", "WriteConsoleInputA")
TEST(gap16_conw_all_exports_present) {
    static const char *n[] = { "WriteConsoleA", "WriteConsoleW", "WriteConsoleInputA", "WriteConsoleInputW", "WriteConsoleOutputA", "WriteConsoleOutputW", "WriteConsoleOutputAttribute", "WriteConsoleOutputCharacterA", "WriteConsoleOutputCharacterW" };
    for (int i = 0; i < 9; i++) { CHECKF(t_proc_k32(n[i]) != NULL, "kernel32 %s", n[i]); CHECKF(t_proc_kb(n[i]) != NULL, "kernelbase %s", n[i]); }
}
TEST(gap16_conw_no_console_process_fails_invalid_handle) {
    REQUIRE(t_proc("WriteConsoleW")); DWORD c = run_con("nocon", DETACHED_PROCESS);
    if (c != 0 && t_is_wine()) { t_skip("emulator console differs (line %lu)", c); return; }
    CHECKF(c == 0, "detached child result %lu (line %lu)", c, c);
}
