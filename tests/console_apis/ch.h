/* Shared scaffolding for the console_apis area.
 *
 * Every console test runs inside a CHILD process that owns a brand new console
 * (CREATE_NEW_CONSOLE, minimised and not activated), or a detached / no-window /
 * pseudo-console process where that is the thing under test. The parent never
 * attaches to any console. The child reports through a result file:
 *   FAIL <text>   one failed check
 *   NOTE <text>   observed behaviour
 *   SKIP <text>   test could not run (missing export ...)
 *   DONE <n>      finished, n checks passed
 * and the parent converts those lines into harness results.
 */
#ifndef CH_H
#define CH_H
#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x0A000006
#endif
#include "t.h"
#include <wincon.h>

/* Route every console API used in this area through GetProcAddress. Use the plain names; never wrap in U(). */
#define AddConsoleAliasA (U(AddConsoleAliasA))
#define AddConsoleAliasW (U(AddConsoleAliasW))
#define AllocConsole (U(AllocConsole))
#define AttachConsole (U(AttachConsole))
#define CreateConsoleScreenBuffer (U(CreateConsoleScreenBuffer))
#define ClosePseudoConsole (U(ClosePseudoConsole))
#define CreatePseudoConsole (U(CreatePseudoConsole))
#define ResizePseudoConsole (U(ResizePseudoConsole))
#define ExpungeConsoleCommandHistoryA (U(ExpungeConsoleCommandHistoryA))
#define ExpungeConsoleCommandHistoryW (U(ExpungeConsoleCommandHistoryW))
#define FillConsoleOutputAttribute (U(FillConsoleOutputAttribute))
#define FillConsoleOutputCharacterA (U(FillConsoleOutputCharacterA))
#define FillConsoleOutputCharacterW (U(FillConsoleOutputCharacterW))
#define FlushConsoleInputBuffer (U(FlushConsoleInputBuffer))
#define FreeConsole (U(FreeConsole))
#define GenerateConsoleCtrlEvent (U(GenerateConsoleCtrlEvent))
#define GetConsoleAliasA (U(GetConsoleAliasA))
#define GetConsoleAliasW (U(GetConsoleAliasW))
#define GetConsoleAliasesA (U(GetConsoleAliasesA))
#define GetConsoleAliasesW (U(GetConsoleAliasesW))
#define GetConsoleAliasesLengthA (U(GetConsoleAliasesLengthA))
#define GetConsoleAliasesLengthW (U(GetConsoleAliasesLengthW))
#define GetConsoleAliasExesA (U(GetConsoleAliasExesA))
#define GetConsoleAliasExesW (U(GetConsoleAliasExesW))
#define GetConsoleAliasExesLengthA (U(GetConsoleAliasExesLengthA))
#define GetConsoleAliasExesLengthW (U(GetConsoleAliasExesLengthW))
#define GetConsoleCP (U(GetConsoleCP))
#define GetConsoleCommandHistoryA (U(GetConsoleCommandHistoryA))
#define GetConsoleCommandHistoryW (U(GetConsoleCommandHistoryW))
#define GetConsoleCommandHistoryLengthA (U(GetConsoleCommandHistoryLengthA))
#define GetConsoleCommandHistoryLengthW (U(GetConsoleCommandHistoryLengthW))
#define GetConsoleCursorInfo (U(GetConsoleCursorInfo))
#define GetConsoleDisplayMode (U(GetConsoleDisplayMode))
#define GetConsoleFontSize (U(GetConsoleFontSize))
#define GetConsoleHistoryInfo (U(GetConsoleHistoryInfo))
#define GetConsoleMode (U(GetConsoleMode))
#define GetConsoleOriginalTitleA (U(GetConsoleOriginalTitleA))
#define GetConsoleOriginalTitleW (U(GetConsoleOriginalTitleW))
#define GetConsoleOutputCP (U(GetConsoleOutputCP))
#define GetConsoleProcessList (U(GetConsoleProcessList))
#define GetConsoleScreenBufferInfo (U(GetConsoleScreenBufferInfo))
#define GetConsoleScreenBufferInfoEx (U(GetConsoleScreenBufferInfoEx))
#define GetConsoleSelectionInfo (U(GetConsoleSelectionInfo))
#define GetConsoleTitleA (U(GetConsoleTitleA))
#define GetConsoleTitleW (U(GetConsoleTitleW))
#define GetConsoleWindow (U(GetConsoleWindow))
#define GetCurrentConsoleFont (U(GetCurrentConsoleFont))
#define GetCurrentConsoleFontEx (U(GetCurrentConsoleFontEx))
#define GetLargestConsoleWindowSize (U(GetLargestConsoleWindowSize))
#define GetNumberOfConsoleInputEvents (U(GetNumberOfConsoleInputEvents))
#define GetNumberOfConsoleMouseButtons (U(GetNumberOfConsoleMouseButtons))
#define PeekConsoleInputA (U(PeekConsoleInputA))
#define PeekConsoleInputW (U(PeekConsoleInputW))
#define ReadConsoleA (U(ReadConsoleA))
#define ReadConsoleW (U(ReadConsoleW))
#define ReadConsoleInputA (U(ReadConsoleInputA))
#define ReadConsoleInputW (U(ReadConsoleInputW))
#define ReadConsoleOutputA (U(ReadConsoleOutputA))
#define ReadConsoleOutputW (U(ReadConsoleOutputW))
#define ReadConsoleOutputAttribute (U(ReadConsoleOutputAttribute))
#define ReadConsoleOutputCharacterA (U(ReadConsoleOutputCharacterA))
#define ReadConsoleOutputCharacterW (U(ReadConsoleOutputCharacterW))
#define ScrollConsoleScreenBufferA (U(ScrollConsoleScreenBufferA))
#define ScrollConsoleScreenBufferW (U(ScrollConsoleScreenBufferW))
#define SetConsoleActiveScreenBuffer (U(SetConsoleActiveScreenBuffer))
#define SetConsoleCP (U(SetConsoleCP))
#define SetConsoleCtrlHandler (U(SetConsoleCtrlHandler))
#define SetConsoleCursorInfo (U(SetConsoleCursorInfo))
#define SetConsoleCursorPosition (U(SetConsoleCursorPosition))
#define SetConsoleDisplayMode (U(SetConsoleDisplayMode))
#define SetConsoleHistoryInfo (U(SetConsoleHistoryInfo))
#define SetConsoleMode (U(SetConsoleMode))
#define SetConsoleNumberOfCommandsA (U(SetConsoleNumberOfCommandsA))
#define SetConsoleNumberOfCommandsW (U(SetConsoleNumberOfCommandsW))
#define SetConsoleOutputCP (U(SetConsoleOutputCP))
#define SetConsoleScreenBufferInfoEx (U(SetConsoleScreenBufferInfoEx))
#define SetConsoleScreenBufferSize (U(SetConsoleScreenBufferSize))
#define SetConsoleTextAttribute (U(SetConsoleTextAttribute))
#define SetConsoleTitleA (U(SetConsoleTitleA))
#define SetConsoleTitleW (U(SetConsoleTitleW))
#define SetConsoleWindowInfo (U(SetConsoleWindowInfo))
#define SetCurrentConsoleFontEx (U(SetCurrentConsoleFontEx))
#define WriteConsoleA (U(WriteConsoleA))
#define WriteConsoleW (U(WriteConsoleW))
#define WriteConsoleInputA (U(WriteConsoleInputA))
#define WriteConsoleInputW (U(WriteConsoleInputW))
#define WriteConsoleOutputA (U(WriteConsoleOutputA))
#define WriteConsoleOutputW (U(WriteConsoleOutputW))
#define WriteConsoleOutputAttribute (U(WriteConsoleOutputAttribute))
#define WriteConsoleOutputCharacterA (U(WriteConsoleOutputCharacterA))
#define WriteConsoleOutputCharacterW (U(WriteConsoleOutputCharacterW))
#define GetStdHandle (U(GetStdHandle))
#define SetStdHandle (U(SetStdHandle))

#ifndef CTRL_C_EVENT
#define CTRL_C_EVENT 0
#endif

/* ---------------- child side ---------------- */
static char g_res[MAX_PATH * 2];
static int g_cpass;
static int g_cskip;

static void cline(const char *fmt, ...) {
    char buf[1024]; va_list a; DWORD n; HANDLE h;
    int len;
    if (!g_res[0]) return;
    va_start(a, fmt); len = vsnprintf(buf, sizeof buf - 2, fmt, a); va_end(a);
    if (len < 0) len = 0;
    if (len > (int)sizeof buf - 3) len = sizeof buf - 3;
    buf[len++] = '\n';
    h = CreateFileA(g_res, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    WriteFile(h, buf, len, &n, NULL);
    CloseHandle(h);
}
static void cfail(const char *file, int line, const char *expr, const char *fmt, ...) {
    char d[512]; d[0] = 0;
    if (fmt) { va_list a; va_start(a, fmt); vsnprintf(d, sizeof d, fmt, a); va_end(a); }
    const char *base = strrchr(file, '/'); if (!base) base = strrchr(file, '\\'); base = base ? base + 1 : file;
    cline("FAIL %s:%d: %s%s%s%s", base, line, expr, d[0] ? " (" : "", d, d[0] ? ")" : "");
}
static void cnote(const char *fmt, ...) {
    char d[700]; va_list a; va_start(a, fmt); vsnprintf(d, sizeof d, fmt, a); va_end(a);
    cline("NOTE %s", d);
}
#define CC(c)        do { if (c) g_cpass++; else cfail(__FILE__, __LINE__, #c, NULL); } while (0)
#define CCF(c, ...)  do { if (c) g_cpass++; else cfail(__FILE__, __LINE__, #c, __VA_ARGS__); } while (0)
#define CEQ(a, b)    do { unsigned long long a_ = (unsigned long long)(ULONG_PTR)(a), b_ = (unsigned long long)(ULONG_PTR)(b); \
    if (a_ == b_) g_cpass++; else cfail(__FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", a_, b_); } while (0)
#define CNE(a, b)    do { unsigned long long a_ = (unsigned long long)(ULONG_PTR)(a), b_ = (unsigned long long)(ULONG_PTR)(b); \
    if (a_ != b_) g_cpass++; else cfail(__FILE__, __LINE__, #a " != " #b, "got 0x%llx", a_); } while (0)
/* GetLastError right after the failing call */
#define CERR(e)      do { DWORD e_ = GetLastError(); if (e_ == (DWORD)(e)) g_cpass++; else cfail(__FILE__, __LINE__, "GetLastError == " #e, "got %lu want %lu", e_, (DWORD)(e)); } while (0)
#define CREQ(p)      do { if (!(p)) { cline("SKIP missing export: " #p); g_cskip = 1; return; } } while (0)
#define CREQG(p)     do { if (!(p)) { cline("SKIP missing export: " #p); return 0; } } while (0)
#define CWINESKIP(why) do { if (t_is_wine()) { cline("SKIP " why); g_cskip = 1; return; } } while (0)
#define CWSTR(a, b)  do { if (!lstrcmpW((a), (b))) g_cpass++; else cfail(__FILE__, __LINE__, #a " == " #b, "got \"%ls\" want \"%ls\"", (a), (b)); } while (0)
#define CSTR(a, b)   do { if (!strcmp((a), (b))) g_cpass++; else cfail(__FILE__, __LINE__, #a " == " #b, "got \"%s\" want \"%s\"", (a), (b)); } while (0)

static int cwrap(int argc, char **argv, void (*f)(void)) {
    if (argc < 2) return 3;
    snprintf(g_res, sizeof g_res, "%s", argv[1]);
    f();
    if (g_cskip) { cline("DONE 0"); return 0; }
    cline("DONE %d", g_cpass);
    return 0;
}

/* ---------------- parent side ---------------- */
static void prun_report(const char *res, DWORD code, int timedout, const char *what) {
    char *buf = (char *)calloc(1, 262144);
    DWORD n = 0; int done = 0, npass = 0, skipped = 0;
    HANDLE h = CreateFileA(res, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) { ReadFile(h, buf, 262143, &n, NULL); CloseHandle(h); }
    buf[n] = 0;
    for (char *p = buf; *p; ) {
        char *e = strchr(p, '\n'); if (e) *e = 0;
        size_t l = strlen(p); if (l && p[l - 1] == '\r') p[l - 1] = 0;
        if (!strncmp(p, "FAIL ", 5)) t_check(0, "child", 0, p + 5, NULL);
        else if (!strncmp(p, "NOTE ", 5)) t_note("%s", p + 5);
        else if (!strncmp(p, "SKIP ", 5)) { t_skip("%s", p + 5); skipped = 1; }
        else if (!strncmp(p, "DONE ", 5)) { done = 1; npass = atoi(p + 5); }
        if (!e) break;
        p = e + 1;
    }
    if (timedout) t_check(0, "child", 0, "child finished within timeout", "%s", what);
    else if (!done) t_check(0, "child", 0, "child wrote DONE", "%s exit=0x%lx", what, code);
    else if (code != 0) t_check(0, "child", 0, "child exit code 0", "exit=0x%lx", code);
    for (int i = 0; i < npass; i++) t_check(1, "child", 0, "", NULL);
    free(buf);
}

/* Spawn this exe as `--child <cname> "<resultfile>" [extra]` with the given creation flags. */
static HANDLE pspawn(const char *cname, const char *extra, DWORD flags, const char *title, char *resout, size_t rescap, PROCESS_INFORMATION *pi) {
    static int seq;
    char res[MAX_PATH * 2], cmd[4096];
    STARTUPINFOA si;
    snprintf(res, sizeof res, "%sres_%d_%d.txt", t_tmpdir(), (int)GetCurrentProcessId(), ++seq);
    DeleteFileA(res);
    if (resout) snprintf(resout, rescap, "%s", res);
    memset(&si, 0, sizeof si); si.cb = sizeof si;
    si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_SHOWMINNOACTIVE;
    if (title) si.lpTitle = (LPSTR)title;
    snprintf(cmd, sizeof cmd, "\"%s\" --child %s \"%s\" %s", t_self_path(), cname, res, extra ? extra : "");
    memset(pi, 0, sizeof *pi);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, flags, NULL, NULL, &si, pi)) return NULL;
    return pi->hProcess;
}

static void prun_ex(const char *cname, DWORD flags, const char *title) {
    PROCESS_INFORMATION pi; char res[MAX_PATH * 2]; DWORD code = 0xDEAD0000; int to = 0;
    /* the emulator's windowed console host needs a working display; its headless host behaves like a console */
    if (t_is_wine() && (flags & CREATE_NEW_CONSOLE)) flags = (flags & ~CREATE_NEW_CONSOLE) | CREATE_NO_WINDOW;
    HANDLE h = pspawn(cname, NULL, flags, title, res, sizeof res, &pi);
    for (int retry = 0; !h && retry < 3; retry++) { Sleep(300); h = pspawn(cname, NULL, flags, title, res, sizeof res, &pi); }
    if (!h) { t_check(0, "parent", 0, "CreateProcess child", "err=%lu", GetLastError()); return; }
    if (WaitForSingleObject(h, 25000) != WAIT_OBJECT_0) { TerminateProcess(h, 1); WaitForSingleObject(h, 5000); to = 1; }
    GetExitCodeProcess(h, &code);
    CloseHandle(pi.hThread); CloseHandle(h);
    prun_report(res, code, to, cname);
}

/* CT(name) { child body }: TEST(name) runs the body in a fresh console.
 * CTF: explicit creation flags (DETACHED_PROCESS, CREATE_NO_WINDOW, ...). CTT: with a start title. */
#define CTF(n, fl, ttl) \
    static void ct_##n(void); \
    CHILD(c_##n) { return cwrap(argc, argv, ct_##n); } \
    TEST(n) { prun_ex("c_" #n, (fl), (ttl)); } \
    static void ct_##n(void)
#define CT(n) CTF(n, CREATE_NEW_CONSOLE, NULL)
#define CTD(n) CTF(n, DETACHED_PROCESS, NULL)
#define CTN(n) CTF(n, CREATE_NO_WINDOW, NULL)

/* Grandchild (spawned by a console child): CG(name) { GSETUP(); ...; return code; } */
#define CG(n) CHILD(g_##n)
#define GSETUP() do { if (argc > 1) snprintf(g_res, sizeof g_res, "%s", argv[1]); } while (0)

/* Child-side: spawn a grandchild g_<name>, same result file. */
static HANDLE gspawn(const char *name, const char *extra, DWORD flags, HANDLE hin, HANDLE hout, HANDLE herr, BOOL inherit, PROCESS_INFORMATION *pi) {
    char cmd[4096]; STARTUPINFOA si;
    memset(&si, 0, sizeof si); si.cb = sizeof si;
    if (hin || hout || herr) { si.dwFlags |= STARTF_USESTDHANDLES; si.hStdInput = hin; si.hStdOutput = hout; si.hStdError = herr; }
    snprintf(cmd, sizeof cmd, "\"%s\" --child g_%s \"%s\" %s", t_self_path(), name, g_res, extra ? extra : "");
    memset(pi, 0, sizeof *pi);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, inherit, flags, NULL, NULL, &si, pi)) return NULL;
    return pi->hProcess;
}
static DWORD gwait(HANDLE h, DWORD ms) {
    DWORD code = 0xDEAD0001;
    if (WaitForSingleObject(h, ms) != WAIT_OBJECT_0) { TerminateProcess(h, 99); WaitForSingleObject(h, 3000); return 0xDEAD0001; }
    GetExitCodeProcess(h, &code);
    return code;
}

/* ---------------- common helpers ---------------- */
static HANDLE hin_(void)  { return GetStdHandle(STD_INPUT_HANDLE); }
static HANDLE hout_(void) { return GetStdHandle(STD_OUTPUT_HANDLE); }
static HANDLE herr_(void) { return GetStdHandle(STD_ERROR_HANDLE); }

/* New private text-mode screen buffer, optionally sized (w,h > 0). NULL on failure. */
static HANDLE mkbuf_raw(void) {
    HANDLE h = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    return h == INVALID_HANDLE_VALUE ? NULL : h;
}
static HANDLE mkbuf(SHORT w, SHORT h) {
    HANDLE b = mkbuf_raw();
    CONSOLE_SCREEN_BUFFER_INFO i; SMALL_RECT r; COORD c;
    if (!b) return NULL;
    if (!GetConsoleScreenBufferInfo(b, &i)) { CloseHandle(b); return NULL; }
    if (i.dwSize.X != w || i.dwSize.Y != h) {
        SHORT ww = i.srWindow.Right - i.srWindow.Left + 1, wh = i.srWindow.Bottom - i.srWindow.Top + 1;
        SHORT nw = ww < w ? ww : w, nh = wh < h ? wh : h;
        r.Left = 0; r.Top = 0; r.Right = nw - 1; r.Bottom = nh - 1;
        SetConsoleWindowInfo(b, TRUE, &r);
        c.X = w; c.Y = h;
        SetConsoleScreenBufferSize(b, c);
        r.Right = (w < ww ? w : ww) - 1; r.Bottom = (h < wh ? h : wh) - 1;
        SetConsoleWindowInfo(b, TRUE, &r);
        GetConsoleScreenBufferInfo(b, &i);
        if (i.dwSize.X != w || i.dwSize.Y != h) { CloseHandle(b); return NULL; }
    }
    return b;
}
/* Prepare an 80x25 buffer in processed mode with default attributes; cnote+NULL if impossible. */
static HANDLE mkbuf_std(void) {
    HANDLE b = mkbuf(80, 25);
    if (!b) { cnote("could not create 80x25 buffer; test skipped"); g_cskip = 1; cline("SKIP no 80x25 buffer"); return NULL; }
    SetConsoleTextAttribute(b, 7);
    SetConsoleMode(b, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT);
    return b;
}
static COORD co(int x, int y) { COORD c; c.X = (SHORT)x; c.Y = (SHORT)y; return c; }
static SMALL_RECT sr(int l, int t, int r, int b) { SMALL_RECT s; s.Left = (SHORT)l; s.Top = (SHORT)t; s.Right = (SHORT)r; s.Bottom = (SHORT)b; return s; }
static int coeq(COORD a, int x, int y) { return a.X == x && a.Y == y; }
static COORD curpos(HANDLE h) { CONSOLE_SCREEN_BUFFER_INFO i; memset(&i, 0, sizeof i); GetConsoleScreenBufferInfo(h, &i); return i.dwCursorPosition; }
static WORD attrof(HANDLE h) { CONSOLE_SCREEN_BUFFER_INFO i; memset(&i, 0, sizeof i); GetConsoleScreenBufferInfo(h, &i); return i.wAttributes; }
/* write wide string via kernel32 under test */
static BOOL wrw(HANDLE h, const WCHAR *s) {
    DWORD n = 0;
    return WriteConsoleW(h, s, lstrlenW(s), &n, NULL) && n == (DWORD)lstrlenW(s);
}
static WCHAR cellch(HANDLE h, int x, int y) {
    WCHAR c = 0; DWORD n = 0;
    ReadConsoleOutputCharacterW(h, &c, 1, co(x, y), &n);
    return n == 1 ? c : 0xFFFF;
}
static WORD cellattr(HANDLE h, int x, int y) {
    WORD a = 0xFFFF; DWORD n = 0;
    ReadConsoleOutputAttribute(h, &a, 1, co(x, y), &n);
    return n == 1 ? a : 0xFFFF;
}
static void rowtext(HANDLE h, int y, int x0, int len, WCHAR *out) {
    DWORD n = 0; out[0] = 0;
    if (ReadConsoleOutputCharacterW(h, out, len, co(x0, y), &n)) out[n] = 0;
}
static int rowis(HANDLE h, int y, int x0, const WCHAR *want) {
    WCHAR buf[512]; int l = lstrlenW(want);
    if (l > 500) return 0;
    rowtext(h, y, x0, l, buf);
    return !lstrcmpW(buf, want);
}
static void clear_buf(HANDLE h) {
    CONSOLE_SCREEN_BUFFER_INFO i; DWORD n;
    GetConsoleScreenBufferInfo(h, &i);
    FillConsoleOutputCharacterW(h, L' ', i.dwSize.X * i.dwSize.Y, co(0, 0), &n);
    FillConsoleOutputAttribute(h, 7, i.dwSize.X * i.dwSize.Y, co(0, 0), &n);
    SetConsoleCursorPosition(h, co(0, 0));
}

/* Input injection ------------------------------------------------ */
static WORD vk_of(WCHAR c) {
    if (c >= 'a' && c <= 'z') return (WORD)(c - 'a' + 'A');
    if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return (WORD)c;
    if (c == ' ') return VK_SPACE;
    if (c == '\r') return VK_RETURN;
    if (c == '\t') return VK_TAB;
    if (c == 8) return VK_BACK;
    if (c == 27) return VK_ESCAPE;
    return 0;
}
static INPUT_RECORD keyrec(WCHAR c, WORD vk, BOOL down, DWORD ctl) {
    INPUT_RECORD r; memset(&r, 0, sizeof r);
    r.EventType = KEY_EVENT;
    r.Event.KeyEvent.bKeyDown = down;
    r.Event.KeyEvent.wRepeatCount = 1;
    r.Event.KeyEvent.wVirtualKeyCode = vk;
    r.Event.KeyEvent.wVirtualScanCode = vk ? (WORD)MapVirtualKeyA(vk, 0) : 0;
    r.Event.KeyEvent.uChar.UnicodeChar = c;
    r.Event.KeyEvent.dwControlKeyState = ctl;
    return r;
}
/* type string as down+up pairs; returns events written */
static DWORD typestr(HANDLE hi, const WCHAR *s) {
    INPUT_RECORD r[512]; int n = 0; DWORD w = 0;
    for (; *s && n < 510; s++) {
        r[n++] = keyrec(*s, vk_of(*s), TRUE, 0);
        r[n++] = keyrec(*s, vk_of(*s), FALSE, 0);
    }
    WriteConsoleInputW(hi, r, n, &w);
    return w;
}
static void inp_reset(void) {
    Sleep(30);
    FlushConsoleInputBuffer(hin_());
}
static DWORD inp_count(void) { DWORD n = 0xFFFFFFFF; GetNumberOfConsoleInputEvents(hin_(), &n); return n; }
/* wait until handle h signalled or ms elapsed */
static int waitsig(HANDLE h, DWORD ms) { return WaitForSingleObject(h, ms) == WAIT_OBJECT_0; }

/* A/W helpers */
static void a2w(const char *a, WCHAR *w, int cap) { MultiByteToWideChar(CP_ACP, 0, a, -1, w, cap); }

static const WCHAR *exe_basename_w(void) {
    static WCHAR b[MAX_PATH];
    if (!b[0]) {
        WCHAR full[MAX_PATH]; GetModuleFileNameW(NULL, full, MAX_PATH);
        WCHAR *s = wcsrchr(full, L'\\'); lstrcpyW(b, s ? s + 1 : full);
    }
    return b;
}

#endif
