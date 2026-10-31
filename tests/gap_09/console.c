/* gap9: PeekConsoleInputA/W. All console work happens in a child process that owns a fresh console. */
#include "g9.h"

static char g_clog[MAX_PATH]; static int g_cfail;
static void cc(int ok, int line, const char *expr) {
    if (ok) return;
    g_cfail++;
    HANDLE f = CreateFileA(g_clog, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, 0, NULL);
    if (f != INVALID_HANDLE_VALUE) {
        char b[600]; int n = snprintf(b, sizeof b, "line %d: %s (err %lu)\r\n", line, expr, GetLastError());
        DWORD w; WriteFile(f, b, n, &w, NULL); CloseHandle(f);
    }
}
static void cnote(const char *fmt, ...) {
    HANDLE f = CreateFileA(g_clog, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    char b[600]; va_list a; va_start(a, fmt); int n = vsnprintf(b, sizeof b, fmt, a); va_end(a);
    DWORD w; WriteFile(f, "NOTE ", 5, &w, NULL); WriteFile(f, b, n, &w, NULL); WriteFile(f, "\r\n", 2, &w, NULL); CloseHandle(f);
}
#define CC(x) cc(!!(x), __LINE__, #x)

typedef BOOL (WINAPI *peek_t)(HANDLE, PINPUT_RECORD, DWORD, LPDWORD);
static peek_t PkA, PkW;

static INPUT_RECORD kev(WCHAR ch, WORD vk, BOOL down) {
    INPUT_RECORD r; memset(&r, 0, sizeof r);
    r.EventType = KEY_EVENT; r.Event.KeyEvent.bKeyDown = down; r.Event.KeyEvent.wRepeatCount = 1;
    r.Event.KeyEvent.wVirtualKeyCode = vk; r.Event.KeyEvent.wVirtualScanCode = 0;
    r.Event.KeyEvent.uChar.UnicodeChar = ch; r.Event.KeyEvent.dwControlKeyState = 0;
    return r;
}
static HANDLE open_in(DWORD access) { return CreateFileA("CONIN$", access, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL); }
static HANDLE open_out(DWORD access) { return CreateFileA("CONOUT$", access, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL); }
static int prep(HANDLE in) {   /* quiet input buffer: no window/mouse events, empty queue */
    SetConsoleMode(in, ENABLE_EXTENDED_FLAGS);
    return FlushConsoleInputBuffer(in);
}
static DWORD pending(HANDLE in) { DWORD n = 0; GetNumberOfConsoleInputEvents(in, &n); return n; }

static int sc_basic(void) {
    HANDLE in = open_in(GENERIC_READ | GENERIC_WRITE); CC(in != INVALID_HANDLE_VALUE);
    if (in == INVALID_HANDLE_VALUE) return 1;
    CC(prep(in));
    INPUT_RECORD rec[16], out[16]; DWORD w = 0, n;
    /* empty queue */
    memset(out, 0, sizeof out); n = 0x55;
    CC(PkW(in, out, 16, &n)); CC(n == 0);
    n = 0x55; CC(PkA(in, out, 16, &n)); CC(n == 0);
    rec[0] = kev(L'a', 'A', TRUE); rec[1] = kev(L'b', 'B', TRUE); rec[2] = kev(L'c', 'C', TRUE);
    CC(WriteConsoleInputW(in, rec, 3, &w)); CC(w == 3);
    CC(pending(in) == 3);
    /* large buffer: exactly the queued events are returned */
    memset(out, 0xcc, sizeof out); n = 0;
    CC(PkW(in, out, 16, &n)); CC(n == 3);
    for (int i = 0; i < 3; i++) {
        CC(out[i].EventType == KEY_EVENT);
        CC(out[i].Event.KeyEvent.uChar.UnicodeChar == L'a' + i);
        CC(out[i].Event.KeyEvent.wVirtualKeyCode == 'A' + i);
        CC(out[i].Event.KeyEvent.bKeyDown == TRUE);
        CC(out[i].Event.KeyEvent.wRepeatCount == 1);
    }
    CC(((BYTE *)&out[3])[0] == 0xcc && ((BYTE *)&out[3])[sizeof(INPUT_RECORD) - 1] == 0xcc);   /* tail untouched */
    CC(pending(in) == 3);                                   /* peek does not consume */
    n = 0; CC(PkW(in, out, 16, &n)); CC(n == 3);           /* peek twice */
    /* partial buffers */
    n = 0; memset(out, 0, sizeof out); CC(PkW(in, out, 1, &n)); CC(n == 1); CC(out[0].Event.KeyEvent.uChar.UnicodeChar == L'a');
    n = 0; CC(PkW(in, out, 2, &n)); CC(n == 2); CC(out[1].Event.KeyEvent.uChar.UnicodeChar == L'b');
    n = 0; CC(PkW(in, out, 3, &n)); CC(n == 3);
    n = 0; CC(PkW(in, out, 4, &n)); CC(n == 3);
    CC(pending(in) == 3);
    /* zero length */
    n = 0x55; BOOL r0 = PkW(in, out, 0, &n);
    cnote("Peek length 0: rc %d n %lu err %lu", r0, n, GetLastError());
    if (r0) CC(n == 0);
    /* consume one with a real read, peek again sees the rest */
    INPUT_RECORD one; CC(ReadConsoleInputW(in, &one, 1, &w)); CC(w == 1);
    CC(one.Event.KeyEvent.uChar.UnicodeChar == L'a');
    n = 0; CC(PkW(in, out, 16, &n)); CC(n == 2); CC(out[0].Event.KeyEvent.uChar.UnicodeChar == L'b');
    /* flush empties */
    CC(FlushConsoleInputBuffer(in));
    n = 0x55; CC(PkW(in, out, 16, &n)); CC(n == 0);
    CloseHandle(in);
    return 0;
}

static int sc_ansi(void) {
    HANDLE in = open_in(GENERIC_READ | GENERIC_WRITE); CC(in != INVALID_HANDLE_VALUE);
    if (in == INVALID_HANDLE_VALUE) return 1;
    CC(prep(in));
    UINT oldcp = GetConsoleCP();
    INPUT_RECORD rec[4], o[4]; DWORD w, n;
    /* ASCII round trip through the W writer */
    rec[0] = kev(L'q', 'Q', TRUE); rec[1] = kev(L'q', 'Q', FALSE);
    CC(WriteConsoleInputW(in, rec, 2, &w));
    memset(o, 0, sizeof o); n = 0;
    CC(PkA(in, o, 4, &n)); CC(n == 2);
    CC(o[0].EventType == KEY_EVENT); CC(o[0].Event.KeyEvent.uChar.AsciiChar == 'q'); CC(o[0].Event.KeyEvent.bKeyDown == TRUE);
    CC(o[1].Event.KeyEvent.uChar.AsciiChar == 'q'); CC(o[1].Event.KeyEvent.bKeyDown == FALSE);
    memset(o, 0, sizeof o); n = 0;
    CC(PkW(in, o, 4, &n)); CC(n == 2); CC(o[0].Event.KeyEvent.uChar.UnicodeChar == L'q');
    CC(FlushConsoleInputBuffer(in));
    /* ANSI writer, both peeks */
    rec[0] = kev(0, 'Z', TRUE); rec[0].Event.KeyEvent.uChar.AsciiChar = 'z';
    CC(WriteConsoleInputA(in, rec, 1, &w)); CC(w == 1);
    memset(o, 0, sizeof o); n = 0; CC(PkW(in, o, 4, &n)); CC(n == 1); CC(o[0].Event.KeyEvent.uChar.UnicodeChar == L'z');
    memset(o, 0, sizeof o); n = 0; CC(PkA(in, o, 4, &n)); CC(n == 1); CC(o[0].Event.KeyEvent.uChar.AsciiChar == 'z');
    CC(FlushConsoleInputBuffer(in));
    /* code page dependent narrowing */
    if (SetConsoleCP(1252)) {
        rec[0] = kev(0x00e9, 0, TRUE); rec[1] = kev(0x20ac, 0, TRUE);
        CC(WriteConsoleInputW(in, rec, 2, &w)); CC(w == 2);
        memset(o, 0, sizeof o); n = 0; CC(PkA(in, o, 4, &n)); CC(n == 2);
        CC((unsigned char)o[0].Event.KeyEvent.uChar.AsciiChar == 0xe9);
        CC((unsigned char)o[1].Event.KeyEvent.uChar.AsciiChar == 0x80);
        memset(o, 0, sizeof o); n = 0; CC(PkW(in, o, 4, &n)); CC(n == 2);
        CC(o[0].Event.KeyEvent.uChar.UnicodeChar == 0x00e9); CC(o[1].Event.KeyEvent.uChar.UnicodeChar == 0x20ac);
        CC(FlushConsoleInputBuffer(in));
    } else cnote("SetConsoleCP(1252) failed: %lu", GetLastError());
    if (SetConsoleCP(437)) {
        rec[0] = kev(0x00e9, 0, TRUE);
        CC(WriteConsoleInputW(in, rec, 1, &w));
        memset(o, 0, sizeof o); n = 0; CC(PkA(in, o, 4, &n)); CC(n == 1);
        CC((unsigned char)o[0].Event.KeyEvent.uChar.AsciiChar == 0x82);
        CC(FlushConsoleInputBuffer(in));
    }
    SetConsoleCP(oldcp);
    CloseHandle(in);
    return 0;
}

static int sc_errors(void) {
    HANDLE in = open_in(GENERIC_READ | GENERIC_WRITE); CC(in != INVALID_HANDLE_VALUE);
    HANDLE out = open_out(GENERIC_READ | GENERIC_WRITE); CC(out != INVALID_HANDLE_VALUE);
    if (in == INVALID_HANDLE_VALUE) return 1;
    prep(in);
    INPUT_RECORD buf[4], rec = kev(L'x', 'X', TRUE); DWORD n, w;
    WriteConsoleInputW(in, &rec, 1, &w);
    peek_t fn[2] = { PkA, PkW };
    for (int k = 0; k < 2; k++) {
        peek_t P = fn[k];
        n = 0x55; SetLastError(0); CC(!P(NULL, buf, 4, &n)); CC(GetLastError() == ERROR_INVALID_HANDLE); CC(n == 0);
        n = 0x55; SetLastError(0); CC(!P(INVALID_HANDLE_VALUE, buf, 4, &n)); cnote("INVALID_HANDLE_VALUE: err %lu", GetLastError()); CC(n == 0);
        n = 0x55; SetLastError(0); CC(!P((HANDLE)(ULONG_PTR)0x7ffffff0, buf, 4, &n)); CC(GetLastError() == ERROR_INVALID_HANDLE); CC(n == 0);
        HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
        n = 0x55; SetLastError(0); CC(!P(ev, buf, 4, &n)); CC(GetLastError() == ERROR_INVALID_HANDLE); CC(n == 0);
        CloseHandle(ev);
        n = 0x55; SetLastError(0); CC(!P(ev, buf, 4, &n)); CC(GetLastError() == ERROR_INVALID_HANDLE); CC(n == 0);
        if (out != INVALID_HANDLE_VALUE) {
            n = 0x55; SetLastError(0); CC(!P(out, buf, 4, &n)); CC(GetLastError() == ERROR_INVALID_HANDLE); CC(n == 0);
        }
        /* a plain file */
        char path[MAX_PATH]; GetTempPathA(sizeof path, path); strcat(path, "k32t_gap9_peek.tmp");
        HANDLE f = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
        if (f != INVALID_HANDLE_VALUE) {
            n = 0x55; SetLastError(0); BOOL r = P(f, buf, 4, &n); DWORD e = GetLastError();
            CC(!r); CC(n == 0); cnote("peek on a disk file: err %lu", e);
            CloseHandle(f);
        }
    }
    /* handle without read access */
    HANDLE wo = open_in(GENERIC_WRITE);
    if (wo != INVALID_HANDLE_VALUE) {
        n = 0x55; SetLastError(0); BOOL r = PkW(wo, buf, 4, &n); DWORD e = GetLastError();
        cnote("write-only CONIN$ handle: rc %d err %lu n %lu", r, e, n);
        CC(!r);
        CloseHandle(wo);
    } else cnote("CONIN$ GENERIC_WRITE open failed: %lu", GetLastError());
    /* the failures above left the queue untouched */
    n = 0; CC(PkW(in, buf, 4, &n)); CC(n == 1);
    if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
    CloseHandle(in);
    return 0;
}

static int sc_kinds(void) {
    HANDLE in = open_in(GENERIC_READ | GENERIC_WRITE); CC(in != INVALID_HANDLE_VALUE);
    if (in == INVALID_HANDLE_VALUE) return 1;
    SetConsoleMode(in, ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT);
    CC(FlushConsoleInputBuffer(in));
    INPUT_RECORD rec[4], o[8]; DWORD w, n;
    memset(rec, 0, sizeof rec);
    rec[0].EventType = MOUSE_EVENT; rec[0].Event.MouseEvent.dwMousePosition.X = 5; rec[0].Event.MouseEvent.dwMousePosition.Y = 7;
    rec[0].Event.MouseEvent.dwButtonState = FROM_LEFT_1ST_BUTTON_PRESSED; rec[0].Event.MouseEvent.dwEventFlags = 0;
    rec[1].EventType = WINDOW_BUFFER_SIZE_EVENT; rec[1].Event.WindowBufferSizeEvent.dwSize.X = 80; rec[1].Event.WindowBufferSizeEvent.dwSize.Y = 25;
    rec[2].EventType = MENU_EVENT; rec[2].Event.MenuEvent.dwCommandId = 0x1234;
    rec[3] = kev(L'k', 'K', TRUE);
    BOOL wr = WriteConsoleInputW(in, rec, 4, &w);
    cnote("write 4 mixed events: rc %d w %lu", wr, w);
    CC(wr && w == 4);
    memset(o, 0, sizeof o); n = 0;
    CC(PkW(in, o, 8, &n));
    cnote("peeked %lu records", n);
    CC(n >= 4 && n <= 8);
    /* our four records must appear in order (real mouse movement could add extra records) */
    int found[4] = { -1, -1, -1, -1 }, k = 0;
    for (DWORD i = 0; i < n && k < 4; i++) {
        WORD t = o[i].EventType;
        if (k == 0 && t == MOUSE_EVENT && o[i].Event.MouseEvent.dwMousePosition.X == 5 && o[i].Event.MouseEvent.dwMousePosition.Y == 7
            && o[i].Event.MouseEvent.dwButtonState == FROM_LEFT_1ST_BUTTON_PRESSED) found[k++] = i;
        else if (k == 1 && t == WINDOW_BUFFER_SIZE_EVENT && o[i].Event.WindowBufferSizeEvent.dwSize.X == 80 && o[i].Event.WindowBufferSizeEvent.dwSize.Y == 25) found[k++] = i;
        else if (k == 2 && t == MENU_EVENT && o[i].Event.MenuEvent.dwCommandId == 0x1234) found[k++] = i;
        else if (k == 3 && t == KEY_EVENT && o[i].Event.KeyEvent.uChar.UnicodeChar == L'k') found[k++] = i;
    }
    CC(k == 4);
    memset(o, 0, sizeof o); n = 0;
    CC(PkA(in, o, 8, &n)); CC(n >= 4);
    int ka = 0; for (DWORD i = 0; i < n; i++) if (o[i].EventType == KEY_EVENT && o[i].Event.KeyEvent.uChar.AsciiChar == 'k') ka++;
    CC(ka == 1);
    CC(FlushConsoleInputBuffer(in));
    CloseHandle(in);
    return 0;
}

static int sc_many(void) {
    HANDLE in = open_in(GENERIC_READ | GENERIC_WRITE); CC(in != INVALID_HANDLE_VALUE);
    if (in == INVALID_HANDLE_VALUE) return 1;
    prep(in);
    enum { N = 100 };
    static INPUT_RECORD rec[N], out[N + 50];
    for (int i = 0; i < N; i++) rec[i] = kev(L'A' + (i % 26), 0x41 + (i % 26), (i / 26) & 1 ? FALSE : TRUE);
    DWORD w = 0, n = 0;
    CC(WriteConsoleInputW(in, rec, N, &w)); CC(w == N);
    DWORD pend = pending(in);
    cnote("queued %lu of %d", pend, N);
    CC(pend == N);
    memset(out, 0, sizeof out); CC(PkW(in, out, N + 50, &n)); CC(n == N);
    for (DWORD i = 0; i < n && i < N; i++) if (out[i].EventType != KEY_EVENT || out[i].Event.KeyEvent.uChar.UnicodeChar != rec[i].Event.KeyEvent.uChar.UnicodeChar || out[i].Event.KeyEvent.bKeyDown != rec[i].Event.KeyEvent.bKeyDown || out[i].Event.KeyEvent.wVirtualKeyCode != rec[i].Event.KeyEvent.wVirtualKeyCode) { cc(0, __LINE__, "record order/content"); cnote("mismatch at %lu", i); break; }
    n = 0; CC(PkW(in, out, 50, &n)); CC(n == 50);
    n = 0; CC(PkW(in, out, N, &n)); CC(n == N);
    n = 0; CC(PkW(in, out, 1, &n)); CC(n == 1);
    /* large claimed length with real storage */
    static INPUT_RECORD big[1000]; n = 0; CC(PkW(in, big, 1000, &n)); CC(n == N);
    CC(pending(in) == N);
    CC(FlushConsoleInputBuffer(in));
    CloseHandle(in);
    return 0;
}

static int sc_dupe_and_inherit(void) {
    /* peeks through a duplicated handle see the same queue */
    HANDLE in = open_in(GENERIC_READ | GENERIC_WRITE), d = NULL; CC(in != INVALID_HANDLE_VALUE);
    if (in == INVALID_HANDLE_VALUE) return 1;
    prep(in);
    CC(DuplicateHandle(GetCurrentProcess(), in, GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS));
    INPUT_RECORD rec = kev(L'd', 'D', TRUE), o[2]; DWORD w, n = 0;
    CC(WriteConsoleInputW(in, &rec, 1, &w));
    CC(PkW(d, o, 2, &n)); CC(n == 1);
    /* the process standard input handle (when it is the console) also works */
    HANDLE si = GetStdHandle(STD_INPUT_HANDLE); DWORD mode;
    if (si && si != INVALID_HANDLE_VALUE && GetConsoleMode(si, &mode)) { n = 0; CC(PkW(si, o, 2, &n)); CC(n == 1); }
    /* closing the original does not invalidate the duplicate */
    CloseHandle(in);
    n = 0; CC(PkW(d, o, 2, &n)); CC(n == 1);
    CloseHandle(d);
    /* ... but closed handles fail */
    n = 0x55; SetLastError(0); CC(!PkW(d, o, 2, &n)); CC(GetLastError() == ERROR_INVALID_HANDLE); CC(n == 0);
    return 0;
}

static struct { const char *n; int (*f)(void); } g_sc[] = {
    { "basic", sc_basic }, { "ansi", sc_ansi }, { "errors", sc_errors }, { "kinds", sc_kinds }, { "many", sc_many }, { "dupe", sc_dupe_and_inherit },
};

CHILD(g9_peekcon) {
    if (argc < 3) return 0xC0DE0003;
    strncpy(g_clog, argv[2], sizeof g_clog - 1);
    PkA = (peek_t)t_proc("PeekConsoleInputA"); PkW = (peek_t)t_proc("PeekConsoleInputW");
    if (!PkA || !PkW) return 0x7701;
    HANDLE probe = open_in(GENERIC_READ | GENERIC_WRITE);
    if (probe == INVALID_HANDLE_VALUE) return 0x7702;   /* no console available */
    CloseHandle(probe);
    for (size_t i = 0; i < sizeof g_sc / sizeof g_sc[0]; i++)
        if (!strcmp(g_sc[i].n, argv[1])) { g_sc[i].f(); return g_cfail > 200 ? 200 : g_cfail; }
    return 0x7703;
}

static void run_scenario(const char *name) {
    if (t_is_wine()) { t_skip("console input injection is not available headless under wine"); return; }
    REQUIRE(t_proc("PeekConsoleInputA")); REQUIRE(t_proc("PeekConsoleInputW"));
    char log[MAX_PATH], args[MAX_PATH + 64]; snprintf(args, sizeof args, "%s ", name);
    char leaf[64]; snprintf(leaf, sizeof leaf, "gap9_peek_%s.log", name); t_tmpfile(log, sizeof log, leaf);
    DeleteFileA(log);
    strcat(args, "\""); strcat(args, log); strcat(args, "\"");
    /* child receives the log path without quotes via argv */
    PROCESS_INFORMATION pi; HANDLE h = t_spawn_child("g9_peekcon", args, CREATE_NEW_CONSOLE, NULL, NULL, NULL, FALSE, NULL, &pi);
    if (!h) { t_skip("could not spawn console child"); return; }
    DWORD code = 0xDEAD0001;
    if (WaitForSingleObject(h, 30000) != WAIT_OBJECT_0) { TerminateProcess(h, 1); WaitForSingleObject(h, 5000); code = 0xDEAD0001; }
    else GetExitCodeProcess(h, &code);
    CloseHandle(pi.hThread); CloseHandle(h);
    HANDLE f = CreateFileA(log, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (f != INVALID_HANDLE_VALUE) {
        char buf[4096]; DWORD n = 0; ReadFile(f, buf, sizeof buf - 1, &n, NULL); buf[n] = 0; CloseHandle(f);
        for (char *p = strtok(buf, "\r\n"); p; p = strtok(NULL, "\r\n")) {
            if (!strncmp(p, "NOTE ", 5)) t_note("child: %s", p + 5); else CHECKF(0, "child: %s", p);
        }
    }
    if (code == 0x7702 || code == 0x7701) { t_skip("child has no console / exports (code 0x%lx)", code); return; }
    CHECKF(code == 0, "console child '%s' exit 0x%lx", name, code);
}

TEST(gap9_peekcon_basic_queue_semantics) { run_scenario("basic"); }
TEST(gap9_peekcon_ansi_unicode_parity) { run_scenario("ansi"); }
TEST(gap9_peekcon_invalid_handles) { run_scenario("errors"); }
TEST(gap9_peekcon_event_kinds) { run_scenario("kinds"); }
TEST(gap9_peekcon_many_events) { run_scenario("many"); }
TEST(gap9_peekcon_duplicated_handle) { run_scenario("dupe"); }
