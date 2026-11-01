/* console_apis: pseudo consoles (ConPTY) and inherited / redirected console state of child processes */
#include "ch.h"

/* All pseudo console tests run inside a console child (the test process itself never attaches to a console),
 * so the parent-style helper macros are mapped onto the child-side reporting. */
#undef CHECK
#undef CHECKF
#undef EXPECT_EQ
#undef t_note
#undef t_skip
#undef t_check
#define CHECK(c)            CC(c)
#define CHECKF(c, ...)      CCF(c, __VA_ARGS__)
#define EXPECT_EQ(a, b)     CEQ(a, b)
#define t_note              cnote
#define t_skip(...)         c_skip(__VA_ARGS__)
#define t_check(ok, f, l, expr, ...) do { if (ok) g_cpass++; else cfail(f, l, expr, __VA_ARGS__); } while (0)
static void c_skip(const char *fmt, ...) {
    char d[400]; va_list a; va_start(a, fmt); vsnprintf(d, sizeof d, fmt, a); va_end(a);
    cline("SKIP %s", d); g_cskip = 1;
}
/* forward the lines a pty grandchild wrote into our own result file */
static void prun_forward(const char *res, DWORD code, int timedout, const char *what) {
    static char buf[262144]; DWORD n = 0; int done = 0, npass = 0;
    HANDLE h = CreateFileA(res, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) { ReadFile(h, buf, sizeof buf - 1, &n, NULL); CloseHandle(h); }
    buf[n] = 0;
    for (char *p = buf; *p; ) {
        char *e = strchr(p, '\n'); if (e) *e = 0;
        size_t l = strlen(p); if (l && p[l - 1] == '\r') p[l - 1] = 0;
        if (!strncmp(p, "FAIL ", 5) || !strncmp(p, "NOTE ", 5)) cline("%s", p);
        else if (!strncmp(p, "DONE ", 5)) { done = 1; npass = atoi(p + 5); }
        if (!e) break;
        p = e + 1;
    }
    if (timedout) cfail("pty", 0, "grandchild finished within timeout", "%s", what);
    else if (!done) cfail("pty", 0, "grandchild wrote DONE", "%s exit=0x%lx", what, code);
    g_cpass += npass;
}

#define FAILS(call, err)  do { SetLastError(0xBAD); BOOL r_ = (call); DWORD le_ = GetLastError(); CEQ(r_, 0); CERRV(le_, err); } while (0)

/* The test process only creates pipes and a pseudo console; it never attaches to any console. */
typedef struct {
    HPCON pc;
    HANDLE inW, outR;
    PROCESS_INFORMATION pi;
    char out[131072];
    size_t olen;
    char res[MAX_PATH * 2];
    int closed;
} PTY;

static HRESULT pty_open(PTY *p, int cols, int rows, DWORD flags) {
    HANDLE inR = NULL, outW = NULL;
    memset(p, 0, sizeof *p);
    if (!CreatePipe(&inR, &p->inW, NULL, 0)) return E_FAIL;
    if (!CreatePipe(&p->outR, &outW, NULL, 0)) return E_FAIL;
    HRESULT hr = CreatePseudoConsole(co(cols, rows), inR, outW, flags, &p->pc);
    CloseHandle(inR); CloseHandle(outW);
    if (FAILED(hr)) { CloseHandle(p->inW); CloseHandle(p->outR); p->inW = p->outR = NULL; p->pc = NULL; }
    return hr;
}

static BOOL pty_spawn(PTY *p, const char *child, const char *extra) {
    static int seq;
    snprintf(p->res, sizeof p->res, "%spty_%lu_%d.txt", t_tmpdir(), GetCurrentProcessId(), ++seq);
    DeleteFileA(p->res);
    char cmd[4096];
    snprintf(cmd, sizeof cmd, "\"%s\" --child con_dispatch %s \"%s\" %s", t_self_path(), child, p->res, extra ? extra : "");
    WCHAR wcmd[4096];
    MultiByteToWideChar(CP_ACP, 0, cmd, -1, wcmd, 4096);
    STARTUPINFOEXW si; SIZE_T sz = 0;
    memset(&si, 0, sizeof si); si.StartupInfo.cb = sizeof si;
    InitializeProcThreadAttributeList(NULL, 1, 0, &sz);
    si.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, sz);
    if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &sz)) return FALSE;
    if (!UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, p->pc, sizeof(HPCON), NULL, NULL)) return FALSE;
    memset(&p->pi, 0, sizeof p->pi);
    BOOL ok = CreateProcessW(NULL, wcmd, NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, &si.StartupInfo, &p->pi);
    DeleteProcThreadAttributeList(si.lpAttributeList);
    HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
    return ok;
}

static void pty_drain(PTY *p, DWORD ms) {
    DWORD end = GetTickCount() + ms;
    do {
        DWORD avail = 0;
        if (!PeekNamedPipe(p->outR, NULL, 0, NULL, &avail, NULL)) return;
        if (avail) {
            DWORD n = 0; char tmp[4096];
            if (ReadFile(p->outR, tmp, avail > sizeof tmp ? sizeof tmp : avail, &n, NULL) && n) {
                size_t room = sizeof p->out - 1 - p->olen;
                if (n > room) n = (DWORD)room;
                memcpy(p->out + p->olen, tmp, n); p->olen += n; p->out[p->olen] = 0;
            }
        } else Sleep(20);
    } while ((LONG)(end - GetTickCount()) > 0);
}
/* pump output until `needle` appears (binary-safe needle via memmem-ish) or timeout */
static const char *pty_find(PTY *p, const char *needle, size_t nl) {
    if (!nl) nl = strlen(needle);
    for (size_t i = 0; i + nl <= p->olen; i++) if (!memcmp(p->out + i, needle, nl)) return p->out + i;
    return NULL;
}
static BOOL pty_wait_for(PTY *p, const char *needle, DWORD ms) {
    DWORD end = GetTickCount() + ms;
    for (;;) {
        if (pty_find(p, needle, 0)) return TRUE;
        if ((LONG)(end - GetTickCount()) <= 0) return FALSE;
        pty_drain(p, 50);
    }
}
static DWORD pty_wait_exit(PTY *p, DWORD ms) {
    DWORD end = GetTickCount() + ms;
    for (;;) {
        if (WaitForSingleObject(p->pi.hProcess, 0) == WAIT_OBJECT_0) { DWORD c = 0; GetExitCodeProcess(p->pi.hProcess, &c); pty_drain(p, 100); return c; }
        if ((LONG)(end - GetTickCount()) <= 0) return 0xDEAD0001;
        pty_drain(p, 50);
    }
}
static DWORD WINAPI close_thread(LPVOID pc) { ClosePseudoConsole((HPCON)pc); return 0; }
static void pty_close(PTY *p) {
    if (p->closed) return;
    p->closed = 1;
    if (p->pc) {
        HANDLE t = CreateThread(NULL, 0, close_thread, p->pc, 0, NULL);
        DWORD end = GetTickCount() + 10000;
        while (WaitForSingleObject(t, 30) == WAIT_TIMEOUT && (LONG)(end - GetTickCount()) > 0) pty_drain(p, 10);
        if (WaitForSingleObject(t, 0) != WAIT_OBJECT_0) t_check(0, "parent", 0, "ClosePseudoConsole returned within 10s", NULL);
        CloseHandle(t);
    }
    if (p->pi.hProcess) {
        if (WaitForSingleObject(p->pi.hProcess, 3000) != WAIT_OBJECT_0) TerminateProcess(p->pi.hProcess, 99);
        CloseHandle(p->pi.hThread); CloseHandle(p->pi.hProcess);
    }
    if (p->inW) CloseHandle(p->inW);
    if (p->outR) CloseHandle(p->outR);
}
static void pty_report(PTY *p, DWORD code, const char *what) { prun_forward(p->res, code, code == 0xDEAD0001, what); }

static void note_stream(PTY *p) {
    char esc[200]; size_t j = 0;
    for (size_t i = 0; i < p->olen && i < 60 && j < sizeof esc - 8; i++) {
        unsigned char c = (unsigned char)p->out[i];
        if (c >= 32 && c < 127) esc[j++] = (char)c;
        else j += snprintf(esc + j, 8, "\\x%02x", c);
    }
    esc[j] = 0;
    t_note("pty stream (%zu bytes) starts: %s", p->olen, esc);
}

#define PTY_NEED(p) do { if (!(p)->pc) return; } while (0)

static int pty_up(PTY *p, int cols, int rows, DWORD flags) {
    HRESULT hr = pty_open(p, cols, rows, flags);
    if (FAILED(hr)) {
        if (hr == HRESULT_FROM_WIN32(ERROR_CALL_NOT_IMPLEMENTED)) { t_skip("CreatePseudoConsole not supported (hr=0x%lx)", hr); return 0; }
        t_check(0, "parent", 0, "CreatePseudoConsole succeeds", "hr=0x%lx", hr);
        return 0;
    }
    return 1;
}

/* ---------------- child side (run attached to the pseudo console) ---------------- */

static void ptyc_done(void) { cline("DONE %d", g_cpass); }

CG(pty_hello) {
    GSETUP();
    HANDLE o = GetStdHandle(STD_OUTPUT_HANDLE); DWORD n;
    WriteFile(o, "HELLO_PTY\r\n", 11, &n, NULL);
    CEQ(n, 11);
    ptyc_done();
    return 0;
}

CG(pty_state) {
    GSETUP();
    int cols = atoi(argv[2]), rows = atoi(argv[3]);
    HANDLE h[3]; h[0] = GetStdHandle(STD_INPUT_HANDLE); h[1] = GetStdHandle(STD_OUTPUT_HANDLE); h[2] = GetStdHandle(STD_ERROR_HANDLE);
    for (int i = 0; i < 3; i++) {
        CNE(h[i], NULL); CNE(h[i], INVALID_HANDLE_VALUE);
        CEQ(GetFileType(h[i]), FILE_TYPE_CHAR);
        DWORD m; CCF(GetConsoleMode(h[i], &m), "mode of std handle %d", i);
        if (i == 0) cnote("pty input mode 0x%lx", m); else cnote("pty output mode 0x%lx", m);
    }
    CNE(GetConsoleCP(), 0); CNE(GetConsoleOutputCP(), 0);
    CONSOLE_SCREEN_BUFFER_INFO i;
    CC(GetConsoleScreenBufferInfo(h[1], &i));
    cnote("pty child: buffer %dx%d window %d,%d-%d,%d cursor %d,%d attr 0x%x", i.dwSize.X, i.dwSize.Y, i.srWindow.Left, i.srWindow.Top, i.srWindow.Right, i.srWindow.Bottom, i.dwCursorPosition.X, i.dwCursorPosition.Y, i.wAttributes);
    CEQ(i.dwSize.X, cols);
    CEQ(i.srWindow.Right - i.srWindow.Left + 1, cols);
    CEQ(i.srWindow.Bottom - i.srWindow.Top + 1, rows);
    CC(i.dwSize.Y >= rows);
    CC(i.dwMaximumWindowSize.X >= cols);
    DWORD list[4]; DWORD np = GetConsoleProcessList(list, 4);
    cnote("pty process list count %lu", np);
    CEQ(np, 1);
    cnote("pty GetConsoleWindow = %p", GetConsoleWindow());
    COORD big = GetLargestConsoleWindowSize(h[1]);
    cnote("pty largest window %dx%d", big.X, big.Y);
    CONSOLE_CURSOR_INFO ci; CC(GetConsoleCursorInfo(h[1], &ci));
    ptyc_done();
    return 0;
}

CG(pty_resize_wait) {
    GSETUP();
    int cols = atoi(argv[2]);
    HANDLE o = GetStdHandle(STD_OUTPUT_HANDLE); DWORD n;
    CONSOLE_SCREEN_BUFFER_INFO i;
    GetConsoleScreenBufferInfo(o, &i);
    int first = i.dwSize.X;
    cnote("initial buffer width %d", first);
    WriteFile(o, "READY\r\n", 7, &n, NULL);
    for (int k = 0; k < 150; k++) {
        GetConsoleScreenBufferInfo(o, &i);
        if (i.dwSize.X != first) break;
        Sleep(100);
    }
    cnote("after resize: buffer %dx%d window %d,%d-%d,%d", i.dwSize.X, i.dwSize.Y, i.srWindow.Left, i.srWindow.Top, i.srWindow.Right, i.srWindow.Bottom);
    CEQ(i.dwSize.X, cols);
    CEQ(i.srWindow.Right - i.srWindow.Left + 1, cols);
    CEQ(i.srWindow.Bottom - i.srWindow.Top + 1, atoi(argv[3]));
    ptyc_done();
    return 0;
}

CG(pty_read_input) {
    GSETUP();
    HANDLE hi = GetStdHandle(STD_INPUT_HANDLE), o = GetStdHandle(STD_OUTPUT_HANDLE); DWORD n;
    SetConsoleMode(hi, 0);
    WriteFile(o, "READY\r\n", 7, &n, NULL);
    char buf[32] = { 0 }; DWORD got = 0;
    DWORD end = GetTickCount() + 10000;
    while (got < 5 && (LONG)(end - GetTickCount()) > 0) {
        if (WaitForSingleObject(hi, 200) == WAIT_OBJECT_0) {
            DWORD r = 0;
            if (ReadFile(hi, buf + got, 5 - got, &r, NULL)) got += r;
        }
    }
    cnote("pty child read %lu bytes: %s", got, buf);
    CEQ(got, 5);
    CC(!memcmp(buf, "hello", 5));
    ptyc_done();
    return 0;
}

CG(pty_read_keys) {
    GSETUP();
    HANDLE hi = GetStdHandle(STD_INPUT_HANDLE), o = GetStdHandle(STD_OUTPUT_HANDLE); DWORD n;
    SetConsoleMode(hi, ENABLE_WINDOW_INPUT);
    WriteFile(o, "READY\r\n", 7, &n, NULL);
    WCHAR got[8] = { 0 }; int ng = 0;
    DWORD end = GetTickCount() + 10000;
    while (ng < 3 && (LONG)(end - GetTickCount()) > 0) {
        if (WaitForSingleObject(hi, 200) != WAIT_OBJECT_0) continue;
        INPUT_RECORD r[16]; DWORD k = 0;
        if (!ReadConsoleInputW(hi, r, 16, &k)) break;
        for (DWORD j = 0; j < k; j++) {
            if (r[j].EventType == KEY_EVENT && r[j].Event.KeyEvent.bKeyDown && r[j].Event.KeyEvent.uChar.UnicodeChar && ng < 7)
                got[ng++] = r[j].Event.KeyEvent.uChar.UnicodeChar;
        }
    }
    cnote("pty key chars: \"%ls\"", got);
    CEQ(ng, 3);
    CC(got[0] == L'a' && got[1] == L'b' && got[2] == L'c');
    ptyc_done();
    return 0;
}

static volatile LONG g_pty_c;
static BOOL WINAPI pty_ctrl(DWORD t) { if (t == CTRL_C_EVENT) g_pty_c = 1; return TRUE; }
CG(pty_ctrlc) {
    GSETUP();
    HANDLE o = GetStdHandle(STD_OUTPUT_HANDLE); DWORD n;
    SetConsoleCtrlHandler(pty_ctrl, TRUE);
    DWORD m; GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &m);
    SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), m | ENABLE_PROCESSED_INPUT);
    WriteFile(o, "READY\r\n", 7, &n, NULL);
    for (int k = 0; k < 100 && !g_pty_c; k++) Sleep(100);
    CC(g_pty_c);
    ptyc_done();
    return 0;
}

CG(pty_vt_out) {
    GSETUP();
    HANDLE o = GetStdHandle(STD_OUTPUT_HANDLE); DWORD n;
    DWORD m; GetConsoleMode(o, &m);
    SetConsoleMode(o, m | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    WriteConsoleW(o, L"\x1b[31mRED\x1b[0m\x1b[3;7Hxyz", 21, &n, NULL);
    ptyc_done();
    return 0;
}

CG(pty_unicode_out) {
    GSETUP();
    HANDLE o = GetStdHandle(STD_OUTPUT_HANDLE); DWORD n;
    WriteConsoleW(o, L"\xE9|\x4E2D|\x416", 5, &n, NULL);
    CEQ(n, 5);
    ptyc_done();
    return 0;
}

CG(pty_lines_out) {
    GSETUP();
    HANDLE o = GetStdHandle(STD_OUTPUT_HANDLE); DWORD n;
    WriteConsoleA(o, "AAA\nBBB\r\nCCC", 12, &n, NULL);
    ptyc_done();
    return 0;
}

CG(pty_exit_code) {
    GSETUP();
    ptyc_done();
    return 42;
}

CG(pty_title) {
    GSETUP();
    HANDLE o = GetStdHandle(STD_OUTPUT_HANDLE); DWORD n;
    CC(SetConsoleTitleW(L"PTYTITLE_K32T"));
    WCHAR t[64]; GetConsoleTitleW(t, 64);
    CWSTR(t, L"PTYTITLE_K32T");
    WriteFile(o, "T\r\n", 3, &n, NULL);
    ptyc_done();
    return 0;
}

CG(pty_cursor) {
    GSETUP();
    HANDLE o = GetStdHandle(STD_OUTPUT_HANDLE); DWORD n;
    WriteFile(o, "ab", 2, &n, NULL);
    CONSOLE_SCREEN_BUFFER_INFO i; GetConsoleScreenBufferInfo(o, &i);
    cnote("pty cursor after 'ab': %d,%d", i.dwCursorPosition.X, i.dwCursorPosition.Y);
    CEQ(i.dwCursorPosition.X, 2);
    WCHAR w[4]; DWORD r = 0;
    CC(ReadConsoleOutputCharacterW(o, w, 2, co(0, i.dwCursorPosition.Y), &r));
    CEQ(r, 2);
    CC(w[0] == L'a' && w[1] == L'b');
    CC(SetConsoleCursorPosition(o, co(5, 3)));
    GetConsoleScreenBufferInfo(o, &i);
    CC(coeq(i.dwCursorPosition, 5, 3));
    ptyc_done();
    return 0;
}

CG(pty_sleep) {
    GSETUP();
    HANDLE o = GetStdHandle(STD_OUTPUT_HANDLE); DWORD n;
    WriteFile(o, "ALIVE\r\n", 7, &n, NULL);
    Sleep(30000);
    return 5;
}

CG(pty_screen_ops) {
    GSETUP();
    HANDLE o = GetStdHandle(STD_OUTPUT_HANDLE); DWORD n;
    DWORD w;
    CC(FillConsoleOutputCharacterW(o, L'#', 5, co(0, 2), &w)); CEQ(w, 5);
    CC(FillConsoleOutputAttribute(o, 0x4E, 5, co(0, 2), &w)); CEQ(w, 5);
    WCHAR c[6] = { 0 }; WORD a[5];
    CC(ReadConsoleOutputCharacterW(o, c, 5, co(0, 2), &n)); CEQ(n, 5);
    CC(!lstrcmpW(c, L"#####"));
    CC(ReadConsoleOutputAttribute(o, a, 5, co(0, 2), &n)); CEQ(n, 5);
    CEQ(a[0], 0x4E);
    CHAR_INFO ci[2]; ci[0].Char.UnicodeChar = L'X'; ci[0].Attributes = 0x1F; ci[1] = ci[0];
    SMALL_RECT r = sr(10, 10, 11, 10);
    CC(WriteConsoleOutputW(o, ci, co(2, 1), co(0, 0), &r));
    CHAR_INFO rd[2]; r = sr(10, 10, 11, 10);
    CC(ReadConsoleOutputW(o, rd, co(2, 1), co(0, 0), &r));
    CEQ(rd[1].Char.UnicodeChar, 'X'); CEQ(rd[1].Attributes, 0x1F);
    SMALL_RECT src = sr(0, 2, 4, 2); CHAR_INFO fill; fill.Char.UnicodeChar = L'.'; fill.Attributes = 7;
    CC(ScrollConsoleScreenBufferW(o, &src, NULL, co(0, 3), &fill));
    CC(ReadConsoleOutputCharacterW(o, c, 5, co(0, 3), &n));
    CC(!lstrcmpW(c, L"#####"));
    CC(ReadConsoleOutputCharacterW(o, c, 5, co(0, 2), &n));
    CC(!lstrcmpW(c, L"....."));
    HANDLE b = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    cnote("CreateConsoleScreenBuffer inside pty -> %p err=%lu", b, GetLastError());
    if (b != INVALID_HANDLE_VALUE) CloseHandle(b);
    ptyc_done();
    return 0;
}

CG(pty_inherit_cursor) {
    GSETUP();
    HANDLE o = GetStdHandle(STD_OUTPUT_HANDLE); DWORD n;
    WriteFile(o, "HELLO_PTY\r\n", 11, &n, NULL);
    ptyc_done();
    return 0;
}

/* ---------------- tests ---------------- */

CT(con_pty_create_close_no_process) {
    CREQ(CreatePseudoConsole); CREQ(ClosePseudoConsole);
    PTY p;
    if (!pty_up(&p, 80, 25, 0)) return;
    CHECK(p.pc != NULL);
    pty_close(&p);
}

CT(con_pty_create_many_sequential) {
    CREQ(CreatePseudoConsole); CREQ(ClosePseudoConsole);
    for (int i = 0; i < 10; i++) {
        PTY p;
        if (!pty_up(&p, 80 + i, 25, 0)) return;
        pty_close(&p);
    }
}

CT(con_pty_create_invalid_size) {
    CREQ(CreatePseudoConsole); CREQ(ClosePseudoConsole);
    COORD bad[] = { co(0, 0), co(0, 25), co(80, 0), co(-1, -1), co(-80, 25), co(80, -25) };
    for (size_t k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        HANDLE inR, inW, outR, outW; HPCON pc = NULL;
        CreatePipe(&inR, &inW, NULL, 0); CreatePipe(&outR, &outW, NULL, 0);
        HRESULT hr = CreatePseudoConsole(bad[k], inR, outW, 0, &pc);
        t_note("CreatePseudoConsole(%d,%d) -> hr=0x%08lx", bad[k].X, bad[k].Y, hr);
        if (bad[k].X == 0 || bad[k].Y == 0) CHECKF(FAILED(hr), "size %d,%d should fail (hr=0x%lx)", bad[k].X, bad[k].Y, hr);
        if (SUCCEEDED(hr) && pc) {
            HANDLE t = CreateThread(NULL, 0, close_thread, pc, 0, NULL);
            WaitForSingleObject(t, 10000); CloseHandle(t);
        }
        CloseHandle(inR); CloseHandle(inW); CloseHandle(outR); CloseHandle(outW);
    }
}

CT(con_pty_create_invalid_pipes) {
    CREQ(CreatePseudoConsole); CREQ(ClosePseudoConsole);
    HANDLE inR, inW, outR, outW; HPCON pc = NULL;
    CreatePipe(&inR, &inW, NULL, 0); CreatePipe(&outR, &outW, NULL, 0);
    HRESULT hr = CreatePseudoConsole(co(80, 25), INVALID_HANDLE_VALUE, outW, 0, &pc);
    t_note("invalid input handle -> hr=0x%08lx", hr);
    if (SUCCEEDED(hr) && pc) { HANDLE t = CreateThread(NULL, 0, close_thread, pc, 0, NULL); WaitForSingleObject(t, 10000); CloseHandle(t); }
    pc = NULL;
    hr = CreatePseudoConsole(co(80, 25), inR, INVALID_HANDLE_VALUE, 0, &pc);
    t_note("invalid output handle -> hr=0x%08lx", hr);
    if (SUCCEEDED(hr) && pc) { HANDLE t = CreateThread(NULL, 0, close_thread, pc, 0, NULL); WaitForSingleObject(t, 10000); CloseHandle(t); }
    pc = NULL;
    hr = CreatePseudoConsole(co(80, 25), (HANDLE)(ULONG_PTR)0x12345, outW, 0, &pc);
    t_note("garbage input handle -> hr=0x%08lx", hr);
    CHECK(FAILED(hr));
    CloseHandle(inR); CloseHandle(inW); CloseHandle(outR); CloseHandle(outW);
}

CT(con_pty_create_unknown_flags) {
    CREQ(CreatePseudoConsole); CREQ(ClosePseudoConsole);
    DWORD flags[] = { 0x80000000, 0x100, 0xFFFFFFF8 };
    for (size_t k = 0; k < sizeof flags / sizeof flags[0]; k++) {
        PTY p;
        HRESULT hr = pty_open(&p, 80, 25, flags[k]);
        t_note("CreatePseudoConsole flags=0x%lx -> hr=0x%08lx", flags[k], hr);
        if (SUCCEEDED(hr)) pty_close(&p);
    }
}

CT(con_pty_known_flags_accepted) {
    CREQ(CreatePseudoConsole); CREQ(ClosePseudoConsole);
    DWORD flags[] = { 0, 1 /* INHERIT_CURSOR */, 2 /* RESIZE_QUIRK */, 4 /* WIN32_INPUT_MODE */ };
    for (size_t k = 0; k < sizeof flags / sizeof flags[0]; k++) {
        PTY p;
        HRESULT hr = pty_open(&p, 80, 25, flags[k]);
        t_note("CreatePseudoConsole flags=0x%lx -> hr=0x%08lx", flags[k], hr);
        if (flags[k] == 0) CHECK(SUCCEEDED(hr));
        if (SUCCEEDED(hr)) pty_close(&p);
    }
}

CT(con_pty_child_output_reaches_pipe) {
    CREQ(CreatePseudoConsole);
    PTY p;
    if (!pty_up(&p, 80, 25, 0)) return;
    if (!pty_spawn(&p, "g_pty_hello", NULL)) { CHECK(0); pty_close(&p); return; }
    CHECK(pty_wait_for(&p, "HELLO_PTY", 10000));
    DWORD code = pty_wait_exit(&p, 10000);
    EXPECT_EQ(code, 0);
    note_stream(&p);
    t_note("stream has ESC[?9001h=%d ESC[?1004h=%d ESC[ =%d", pty_find(&p, "\x1b[?9001h", 0) != NULL, pty_find(&p, "\x1b[?1004h", 0) != NULL, pty_find(&p, "\x1b[", 0) != NULL);
    pty_report(&p, code, "pty_hello");
    pty_close(&p);
}

CT(con_pty_child_console_state) {
    CREQ(CreatePseudoConsole);
    PTY p;
    if (!pty_up(&p, 100, 30, 0)) return;
    if (!pty_spawn(&p, "g_pty_state", "100 30")) { CHECK(0); pty_close(&p); return; }
    DWORD code = pty_wait_exit(&p, 15000);
    pty_report(&p, code, "pty_state");
    pty_close(&p);
}

CT(con_pty_size_variants) {
    CREQ(CreatePseudoConsole);
    static const int sz[][2] = { { 20, 5 }, { 132, 50 }, { 200, 60 }, { 80, 24 }, { 40, 40 } };
    for (size_t k = 0; k < sizeof sz / sizeof sz[0]; k++) {
        PTY p;
        if (!pty_up(&p, sz[k][0], sz[k][1], 0)) return;
        char args[32]; snprintf(args, sizeof args, "%d %d", sz[k][0], sz[k][1]);
        if (!pty_spawn(&p, "g_pty_state", args)) { CHECK(0); pty_close(&p); return; }
        DWORD code = pty_wait_exit(&p, 15000);
        char what[40]; snprintf(what, sizeof what, "pty_state %s", args);
        pty_report(&p, code, what);
        pty_close(&p);
    }
}

CT(con_pty_two_sessions_concurrent) {
    CREQ(CreatePseudoConsole);
    PTY a, b;
    if (!pty_up(&a, 90, 20, 0)) return;
    if (!pty_up(&b, 110, 33, 0)) { pty_close(&a); return; }
    BOOL oa = pty_spawn(&a, "g_pty_state", "90 20"), ob = pty_spawn(&b, "g_pty_state", "110 33");
    CHECK(oa); CHECK(ob);
    if (oa && ob) {
        DWORD ca = pty_wait_exit(&a, 15000), cb = pty_wait_exit(&b, 15000);
        pty_report(&a, ca, "pty a"); pty_report(&b, cb, "pty b");
    }
    pty_close(&a); pty_close(&b);
}

CT(con_pty_resize) {
    CWINESKIP("ResizePseudoConsole is not implemented in the emulator");
    CREQ(CreatePseudoConsole); CREQ(ResizePseudoConsole);
    PTY p;
    if (!pty_up(&p, 80, 25, 0)) return;
    if (!pty_spawn(&p, "g_pty_resize_wait", "120 40")) { CHECK(0); pty_close(&p); return; }
    CHECK(pty_wait_for(&p, "READY", 10000));
    HRESULT hr = ResizePseudoConsole(p.pc, co(120, 40));
    EXPECT_EQ(hr, S_OK);
    DWORD code = pty_wait_exit(&p, 20000);
    pty_report(&p, code, "pty_resize_wait");
    pty_close(&p);
}

CT(con_pty_resize_invalid_size) {
    CWINESKIP("ResizePseudoConsole is not implemented in the emulator");
    CREQ(CreatePseudoConsole); CREQ(ResizePseudoConsole);
    PTY p;
    if (!pty_up(&p, 80, 25, 0)) return;
    COORD bad[] = { co(0, 0), co(0, 25), co(80, 0), co(-1, -1) };
    for (size_t k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        HRESULT hr = ResizePseudoConsole(p.pc, bad[k]);
        t_note("ResizePseudoConsole(%d,%d) -> 0x%08lx", bad[k].X, bad[k].Y, hr);
        /* P1 EXPECT-FIX: Win10 kernelbase ResizePseudoConsole only rejects a NULL handle (E_INVALIDARG) and
           otherwise writes a 6-byte resize message to the pty signal pipe; no size validation is done by the
           caller, so every size returns S_OK (observed 0). UNVERIFIED-until-rerun */
        CHECKF(hr == S_OK, "size %d,%d", bad[k].X, bad[k].Y);
    }
    EXPECT_EQ(ResizePseudoConsole(NULL, co(80, 25)), E_INVALIDARG);
    EXPECT_EQ(ResizePseudoConsole(p.pc, co(100, 30)), S_OK);
    EXPECT_EQ(ResizePseudoConsole(p.pc, co(100, 30)), S_OK);
    pty_close(&p);
}

CT(con_pty_input_bytes_reach_child) {
    CREQ(CreatePseudoConsole);
    PTY p;
    if (!pty_up(&p, 80, 25, 0)) return;
    if (!pty_spawn(&p, "g_pty_read_input", NULL)) { CHECK(0); pty_close(&p); return; }
    CHECK(pty_wait_for(&p, "READY", 10000));
    DWORD n = 0;
    CHECK(WriteFile(p.inW, "hello", 5, &n, NULL)); EXPECT_EQ(n, 5);
    DWORD code = pty_wait_exit(&p, 20000);
    pty_report(&p, code, "pty_read_input");
    pty_close(&p);
}

CT(con_pty_input_becomes_key_events) {
    CREQ(CreatePseudoConsole);
    PTY p;
    if (!pty_up(&p, 80, 25, 0)) return;
    if (!pty_spawn(&p, "g_pty_read_keys", NULL)) { CHECK(0); pty_close(&p); return; }
    CHECK(pty_wait_for(&p, "READY", 10000));
    DWORD n = 0;
    CHECK(WriteFile(p.inW, "abc", 3, &n, NULL));
    DWORD code = pty_wait_exit(&p, 20000);
    pty_report(&p, code, "pty_read_keys");
    pty_close(&p);
}

CT(con_pty_ctrl_c_byte_signals_child) {
    CREQ(CreatePseudoConsole);
    PTY p;
    if (!pty_up(&p, 80, 25, 0)) return;
    if (!pty_spawn(&p, "g_pty_ctrlc", NULL)) { CHECK(0); pty_close(&p); return; }
    CHECK(pty_wait_for(&p, "READY", 10000));
    DWORD n = 0;
    CHECK(WriteFile(p.inW, "\x03", 1, &n, NULL));
    DWORD code = pty_wait_exit(&p, 20000);
    pty_report(&p, code, "pty_ctrlc");
    pty_close(&p);
}

CT(con_pty_vt_output_forwarded) {
    CREQ(CreatePseudoConsole);
    PTY p;
    if (!pty_up(&p, 80, 25, 0)) return;
    if (!pty_spawn(&p, "g_pty_vt_out", NULL)) { CHECK(0); pty_close(&p); return; }
    DWORD code = pty_wait_exit(&p, 15000);
    CHECK(pty_wait_for(&p, "RED", 5000));
    CHECK(pty_wait_for(&p, "xyz", 5000));
    note_stream(&p);
    t_note("stream contains ESC[31m=%d ESC[3;7H=%d", pty_find(&p, "\x1b[31m", 0) != NULL, pty_find(&p, "\x1b[3;7H", 0) != NULL);
    pty_report(&p, code, "pty_vt_out");
    pty_close(&p);
}

CT(con_pty_unicode_output_is_utf8) {
    CREQ(CreatePseudoConsole);
    PTY p;
    if (!pty_up(&p, 80, 25, 0)) return;
    if (!pty_spawn(&p, "g_pty_unicode_out", NULL)) { CHECK(0); pty_close(&p); return; }
    DWORD code = pty_wait_exit(&p, 15000);
    pty_wait_for(&p, "\xD0\x96", 3000);
    CHECK(pty_find(&p, "\xC3\xA9", 2) != NULL);
    CHECK(pty_find(&p, "\xE4\xB8\xAD", 3) != NULL);
    CHECK(pty_find(&p, "\xD0\x96", 2) != NULL);
    pty_report(&p, code, "pty_unicode_out");
    pty_close(&p);
}

CT(con_pty_newlines_in_stream) {
    CREQ(CreatePseudoConsole);
    PTY p;
    if (!pty_up(&p, 80, 25, 0)) return;
    if (!pty_spawn(&p, "g_pty_lines_out", NULL)) { CHECK(0); pty_close(&p); return; }
    DWORD code = pty_wait_exit(&p, 15000);
    pty_wait_for(&p, "CCC", 5000);
    CHECK(pty_find(&p, "AAA", 0) != NULL);
    CHECK(pty_find(&p, "BBB", 0) != NULL);
    CHECK(pty_find(&p, "CCC", 0) != NULL);
    const char *a = pty_find(&p, "AAA", 0), *b = pty_find(&p, "BBB", 0), *c = pty_find(&p, "CCC", 0);
    if (a && b && c) CHECK(a < b && b < c);
    note_stream(&p);
    pty_report(&p, code, "pty_lines_out");
    pty_close(&p);
}

CT(con_pty_exit_code_propagates) {
    CREQ(CreatePseudoConsole);
    PTY p;
    if (!pty_up(&p, 80, 25, 0)) return;
    if (!pty_spawn(&p, "g_pty_exit_code", NULL)) { CHECK(0); pty_close(&p); return; }
    DWORD code = pty_wait_exit(&p, 15000);
    EXPECT_EQ(code, 42);
    prun_forward(p.res, 0, 0, "pty_exit_code");
    pty_close(&p);
}

CT(con_pty_title_forwarded) {
    CREQ(CreatePseudoConsole);
    PTY p;
    if (!pty_up(&p, 80, 25, 0)) return;
    if (!pty_spawn(&p, "g_pty_title", NULL)) { CHECK(0); pty_close(&p); return; }
    DWORD code = pty_wait_exit(&p, 15000);
    CHECK(pty_wait_for(&p, "PTYTITLE_K32T", 5000));
    note_stream(&p);
    pty_report(&p, code, "pty_title");
    pty_close(&p);
}

CT(con_pty_cursor_and_readback) {
    CREQ(CreatePseudoConsole);
    PTY p;
    if (!pty_up(&p, 80, 25, 0)) return;
    if (!pty_spawn(&p, "g_pty_cursor", NULL)) { CHECK(0); pty_close(&p); return; }
    DWORD code = pty_wait_exit(&p, 15000);
    pty_report(&p, code, "pty_cursor");
    pty_close(&p);
}

CT(con_pty_screen_apis_in_child) {
    CREQ(CreatePseudoConsole);
    PTY p;
    if (!pty_up(&p, 80, 25, 0)) return;
    if (!pty_spawn(&p, "g_pty_screen_ops", NULL)) { CHECK(0); pty_close(&p); return; }
    DWORD code = pty_wait_exit(&p, 15000);
    pty_report(&p, code, "pty_screen_ops");
    pty_close(&p);
}

CT(con_pty_close_terminates_attached_child) {
    CREQ(CreatePseudoConsole);
    PTY p;
    if (!pty_up(&p, 80, 25, 0)) return;
    if (!pty_spawn(&p, "g_pty_sleep", NULL)) { CHECK(0); pty_close(&p); return; }
    CHECK(pty_wait_for(&p, "ALIVE", 10000));
    HANDLE t = CreateThread(NULL, 0, close_thread, p.pc, 0, NULL);
    DWORD end = GetTickCount() + 12000;
    while (WaitForSingleObject(t, 30) == WAIT_TIMEOUT && (LONG)(end - GetTickCount()) > 0) pty_drain(&p, 10);
    CHECK(WaitForSingleObject(t, 0) == WAIT_OBJECT_0);
    CloseHandle(t);
    p.closed = 1;
    DWORD w = WaitForSingleObject(p.pi.hProcess, 8000);
    t_note("child after ClosePseudoConsole: %s", w == WAIT_OBJECT_0 ? "exited" : "still running");
    CHECK(w == WAIT_OBJECT_0);
    DWORD code = 0; GetExitCodeProcess(p.pi.hProcess, &code);
    t_note("child exit code after pseudoconsole close: 0x%lx", code);
    if (w != WAIT_OBJECT_0) TerminateProcess(p.pi.hProcess, 99);
    CloseHandle(p.pi.hThread); CloseHandle(p.pi.hProcess);
    CloseHandle(p.inW); CloseHandle(p.outR);
}

CT(con_pty_inherit_cursor_flag) {
    CREQ(CreatePseudoConsole);
    PTY p;
    if (!pty_up(&p, 80, 25, 1 /* PSEUDOCONSOLE_INHERIT_CURSOR */)) return;
    if (!pty_spawn(&p, "g_pty_inherit_cursor", NULL)) { CHECK(0); pty_close(&p); return; }
    BOOL got6n = pty_wait_for(&p, "\x1b[6n", 3000);
    t_note("cursor position request seen: %d", got6n);
    DWORD n = 0;
    WriteFile(p.inW, "\x1b[1;1R", 6, &n, NULL);      /* answer the DSR query like a terminal would */
    BOOL hello = pty_wait_for(&p, "HELLO_PTY", 10000);
    if (got6n) CHECK(hello); else t_note("no cursor query seen; output %s", hello ? "flowed" : "did not flow");
    DWORD code = pty_wait_exit(&p, hello ? 15000 : 2000);
    if (hello) pty_report(&p, code, "pty_inherit_cursor");
    pty_close(&p);
}

CT(con_pty_parent_not_attached) {
    CREQ(CreatePseudoConsole);
    /* creating and closing a pseudo console must not give the test process a console of its own */
    HWND before = GetConsoleWindow();
    DWORD cpbefore = GetConsoleCP();
    PTY p;
    if (!pty_up(&p, 80, 25, 0)) return;
    EXPECT_EQ(GetConsoleWindow(), before);
    EXPECT_EQ(GetConsoleCP(), cpbefore);
    pty_close(&p);
    EXPECT_EQ(GetConsoleWindow(), before);
}

/* ---------------- inherited / redirected stdio in console children ---------------- */

CG(redir_probe) {
    GSETUP();
    /* argv[2]: which standard handles are redirected: bitmask 1=in 2=out 4=err */
    int mask = atoi(argv[2]);
    HANDLE h[3]; h[0] = GetStdHandle(STD_INPUT_HANDLE); h[1] = GetStdHandle(STD_OUTPUT_HANDLE); h[2] = GetStdHandle(STD_ERROR_HANDLE);
    for (int i = 0; i < 3; i++) {
        DWORD m = 0; BOOL ok = GetConsoleMode(h[i], &m);
        DWORD ft = GetFileType(h[i]);
        if (mask & (1 << i)) {
            CCF(!ok, "std handle %d is redirected: GetConsoleMode must fail", i);
            if (!ok) CERR(ERROR_INVALID_HANDLE);
            CCF(ft == FILE_TYPE_PIPE || ft == FILE_TYPE_DISK, "type %lu", ft);
        } else {
            CCF(ok, "std handle %d is the console", i);
            CEQ(ft, FILE_TYPE_CHAR);
        }
    }
    if (mask & 2) {
        DWORD n = 0;
        CC(WriteFile(h[1], "to-pipe", 7, &n, NULL)); CEQ(n, 7);
        SetLastError(0xBAD);
        BOOL r = WriteConsoleW(h[1], L"x", 1, &n, NULL);
        /* P1 EXPECT-FIX: Win10 kernelbase WriteConsoleW issues the ConDrv ioctl directly on the handle; a pipe
           answers ERROR_INVALID_FUNCTION (observed 1), not ERROR_INVALID_HANDLE. UNVERIFIED-until-rerun */
        /* P1b: kernelbase maps STATUS_INVALID_PARAMETER from the ioctl to ERROR_INVALID_HANDLE (WriteConsoleW reference analysis:
           status 0xC000000D -> 0xC0000008); the baseline shows a disk file answers that way (6) while a pipe answers
           ERROR_INVALID_FUNCTION (1). UNVERIFIED-until-rerun */
        { DWORD e_ = GetLastError(); CEQ(r, 0); CERRV(e_, GetFileType(h[1]) == FILE_TYPE_DISK ? ERROR_INVALID_HANDLE : ERROR_INVALID_FUNCTION); }
        SetLastError(0xBAD);
        r = SetConsoleMode(h[1], 3);
        CEQ(r, 0); CERR(ERROR_INVALID_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO i;
        SetLastError(0xBAD);
        r = GetConsoleScreenBufferInfo(h[1], &i);
        CEQ(r, 0); CERR(ERROR_INVALID_HANDLE);
    }
    if (mask & 1) {
        char b[16] = { 0 }; DWORD n = 0;
        CC(ReadFile(h[0], b, 6, &n, NULL)); CEQ(n, 6);
        CC(!memcmp(b, "inpipe", 6));
        WCHAR w[4]; SetLastError(0xBAD);
        BOOL r = ReadConsoleW(h[0], w, 4, &n, NULL);
        /* P1 EXPECT-FIX: same as WriteConsoleW above: pipe -> ERROR_INVALID_FUNCTION on Win10. */
        { DWORD e_ = GetLastError(); CEQ(r, 0); CERRV(e_, GetFileType(h[0]) == FILE_TYPE_DISK ? ERROR_INVALID_PARAMETER : ERROR_INVALID_FUNCTION); }
        INPUT_RECORD ir; SetLastError(0xBAD);
        r = PeekConsoleInputW(h[0], &ir, 1, &n);
        CEQ(r, 0); CERR(ERROR_INVALID_HANDLE);
    }
    /* the console itself is still there */
    CNE(GetConsoleCP(), 0);
    CONSOLE_SCREEN_BUFFER_INFO i2;
    HANDLE co_ = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CNE(co_, INVALID_HANDLE_VALUE);
    CC(GetConsoleScreenBufferInfo(co_, &i2));
    CloseHandle(co_);
    return 0;
}

CT(con_inherit_stdout_pipe_in_console_child) {
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE r, w;
    CC(CreatePipe(&r, &w, &sa, 0));
    PROCESS_INFORMATION pi;
    HANDLE g = gspawn("redir_probe", "2", 0, hin_(), w, herr_(), TRUE, &pi);
    CNE(g, NULL); if (!g) return;
    CloseHandle(w);
    char b[32] = { 0 }; DWORD n = 0;
    DWORD end = GetTickCount() + 10000;
    while ((LONG)(end - GetTickCount()) > 0) {
        DWORD av = 0;
        if (!PeekNamedPipe(r, NULL, 0, NULL, &av, NULL)) break;
        if (av) { ReadFile(r, b, sizeof b - 1, &n, NULL); break; }
        if (WaitForSingleObject(g, 0) == WAIT_OBJECT_0) { PeekNamedPipe(r, NULL, 0, NULL, &av, NULL); if (av) { ReadFile(r, b, sizeof b - 1, &n, NULL); } break; }
        Sleep(20);
    }
    CEQ(gwait(g, 10000), 0);
    CSTR(b, "to-pipe");
    CloseHandle(pi.hThread); CloseHandle(g); CloseHandle(r);
}

CT(con_inherit_stdin_pipe_in_console_child) {
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE r, w;
    CC(CreatePipe(&r, &w, &sa, 0));
    DWORD n;
    CC(WriteFile(w, "inpipe", 6, &n, NULL));
    PROCESS_INFORMATION pi;
    HANDLE g = gspawn("redir_probe", "1", 0, r, hout_(), herr_(), TRUE, &pi);
    CNE(g, NULL); if (!g) return;
    CEQ(gwait(g, 10000), 0);
    CloseHandle(pi.hThread); CloseHandle(g); CloseHandle(r); CloseHandle(w);
}

CT(con_inherit_stdout_file_in_console_child) {
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "redir_out.txt");
    HANDLE f = CreateFileA(p, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CREATE_ALWAYS, 0, NULL);
    CNE(f, INVALID_HANDLE_VALUE);
    PROCESS_INFORMATION pi;
    HANDLE g = gspawn("redir_probe", "2", 0, hin_(), f, herr_(), TRUE, &pi);
    CNE(g, NULL); if (!g) return;
    CEQ(gwait(g, 10000), 0);
    CloseHandle(pi.hThread); CloseHandle(g);
    char b[16] = { 0 }; DWORD n = 0;
    SetFilePointer(f, 0, NULL, FILE_BEGIN);
    CC(ReadFile(f, b, 15, &n, NULL));
    CEQ(n, 7);
    CC(!memcmp(b, "to-pipe", 7));
    CloseHandle(f);
}

CT(con_inherit_all_std_handles_redirected_console_still_present) {
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE ir, iw, orr, ow, er, ew; DWORD n;
    CC(CreatePipe(&ir, &iw, &sa, 0)); CC(CreatePipe(&orr, &ow, &sa, 0)); CC(CreatePipe(&er, &ew, &sa, 0));
    WriteFile(iw, "inpipe", 6, &n, NULL);
    PROCESS_INFORMATION pi;
    HANDLE g = gspawn("redir_probe", "7", 0, ir, ow, ew, TRUE, &pi);
    CNE(g, NULL); if (!g) return;
    CEQ(gwait(g, 10000), 0);
    CloseHandle(pi.hThread); CloseHandle(g);
    CloseHandle(ir); CloseHandle(iw); CloseHandle(orr); CloseHandle(ow); CloseHandle(er); CloseHandle(ew);
}

CG(inherit_console_probe) {
    GSETUP();
    int expect_same = atoi(argv[2]);
    HWND want = (HWND)(ULONG_PTR)_strtoui64(argv[3], NULL, 16);
    HWND got = GetConsoleWindow();
    if (expect_same) { CEQ(got, want); }
    else { CNE(got, want); }
    DWORD list[8]; DWORD np = GetConsoleProcessList(list, 8);
    cnote("probe: window %p want %p processes %lu", got, want, np);
    if (expect_same) CC(np >= 2);
    return 0;
}

CT(con_inherit_console_default) {
    char a[64]; snprintf(a, sizeof a, "1 %llx", (unsigned long long)(ULONG_PTR)GetConsoleWindow());
    PROCESS_INFORMATION pi;
    HANDLE g = gspawn("inherit_console_probe", a, 0, NULL, NULL, NULL, FALSE, &pi);
    CNE(g, NULL); if (!g) return;
    CEQ(gwait(g, 10000), 0);
    CloseHandle(pi.hThread); CloseHandle(g);
}

CT(con_inherit_console_new_console_flag) {
    CWINESKIP("windowed consoles need a display in the emulator");
    char a[64]; snprintf(a, sizeof a, "0 %llx", (unsigned long long)(ULONG_PTR)GetConsoleWindow());
    PROCESS_INFORMATION pi;
    HANDLE g = gspawn("inherit_console_probe", a, CREATE_NEW_CONSOLE, NULL, NULL, NULL, FALSE, &pi);
    CNE(g, NULL); if (!g) return;
    CEQ(gwait(g, 10000), 0);
    CloseHandle(pi.hThread); CloseHandle(g);
}

CG(nowindow_probe) {
    GSETUP();
    CEQ(GetConsoleWindow(), NULL);
    CNE(GetConsoleCP(), 0);
    HANDLE o = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD m = 0;
    BOOL ok = GetConsoleMode(o, &m);
    cnote("CREATE_NO_WINDOW child: stdout=%p type=%lu GetConsoleMode=%d", o, o ? GetFileType(o) : 0, ok);
    HANDLE b = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    CNE(b, INVALID_HANDLE_VALUE);
    if (b != INVALID_HANDLE_VALUE) CloseHandle(b);
    return 0;
}

CT(con_inherit_create_no_window_flag) {
    PROCESS_INFORMATION pi;
    HANDLE g = gspawn("nowindow_probe", "", CREATE_NO_WINDOW, NULL, NULL, NULL, FALSE, &pi);
    CNE(g, NULL); if (!g) return;
    CEQ(gwait(g, 10000), 0);
    CloseHandle(pi.hThread); CloseHandle(g);
}

CG(detached_probe) {
    GSETUP();
    CEQ(GetConsoleWindow(), NULL);
    CEQ(GetConsoleCP(), 0);
    HANDLE o = GetStdHandle(STD_OUTPUT_HANDLE);
    CC(o == NULL || o == INVALID_HANDLE_VALUE);
    return 0;
}

CT(con_inherit_detached_flag) {
    PROCESS_INFORMATION pi;
    HANDLE g = gspawn("detached_probe", "", DETACHED_PROCESS, NULL, NULL, NULL, FALSE, &pi);
    CNE(g, NULL); if (!g) return;
    CEQ(gwait(g, 10000), 0);
    CloseHandle(pi.hThread); CloseHandle(g);
}

CG(detached_redir_probe) {
    GSETUP();
    CEQ(GetConsoleWindow(), NULL);
    CEQ(GetConsoleCP(), 0);
    HANDLE o = GetStdHandle(STD_OUTPUT_HANDLE);
    CEQ(GetFileType(o), FILE_TYPE_PIPE);
    DWORD n = 0;
    CC(WriteFile(o, "detached-out", 12, &n, NULL)); CEQ(n, 12);
    DWORD m;
    SetLastError(0xBAD);
    CEQ(GetConsoleMode(o, &m), 0);
    CERR(ERROR_INVALID_HANDLE);
    return 0;
}

CT(con_inherit_detached_with_redirected_stdout) {
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE r, w;
    CC(CreatePipe(&r, &w, &sa, 0));
    PROCESS_INFORMATION pi;
    HANDLE g = gspawn("detached_redir_probe", "", DETACHED_PROCESS, NULL, w, NULL, TRUE, &pi);
    CNE(g, NULL); if (!g) return;
    CloseHandle(w);
    CEQ(gwait(g, 10000), 0);
    char b[32] = { 0 }; DWORD n = 0;
    CC(ReadFile(r, b, sizeof b - 1, &n, NULL));
    CEQ(n, 12);
    CSTR(b, "detached-out");
    CloseHandle(pi.hThread); CloseHandle(g); CloseHandle(r);
}
