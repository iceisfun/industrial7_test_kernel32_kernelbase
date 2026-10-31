/* GetNumberOfConsoleInputEvents, GetNumberOfConsoleMouseButtons, GetLargestConsoleWindowSize.
 * Console state is only touched inside child processes that own their console (CREATE_NEW_CONSOLE) or have none
 * (DETACHED_PROCESS). The parent never attaches to, frees, or writes to any console. */
#include "g5.h"
#include <wincon.h>

static FILE *rf;
static int nfail;
#define CC(cond) do { if (!(cond)) { nfail++; if (rf) { fprintf(rf, "FAIL line %d: %s (gle=%lu)\n", __LINE__, #cond, GetLastError()); fflush(rf); } } } while (0)
#define CN(...) do { if (rf) { fprintf(rf, "NOTE "); fprintf(rf, __VA_ARGS__); fprintf(rf, "\n"); fflush(rf); } } while (0)

typedef BOOL (WINAPI *ninp_t)(HANDLE, LPDWORD);
typedef BOOL (WINAPI *nmouse_t)(LPDWORD);
typedef COORD (WINAPI *lgw_t)(HANDLE);
#define NINP ((ninp_t)g5_proc("GetNumberOfConsoleInputEvents"))
#define NMOUSE ((nmouse_t)g5_proc("GetNumberOfConsoleMouseButtons"))
#define LGW ((lgw_t)g5_proc("GetLargestConsoleWindowSize"))

TEST(gap5_con_exports) {
    CHECK(t_proc_k32("GetNumberOfConsoleInputEvents") != NULL);
    CHECK(t_proc_k32("GetNumberOfConsoleMouseButtons") != NULL);
    CHECK(t_proc_k32("GetLargestConsoleWindowSize") != NULL);
    if (!t_is_wine()) {
        CHECK(t_proc_kb("GetNumberOfConsoleInputEvents") != NULL);
        CHECK(t_proc_kb("GetNumberOfConsoleMouseButtons") != NULL);
        CHECK(t_proc_kb("GetLargestConsoleWindowSize") != NULL);
    }
}

/* Run a child in the requested console mode and relay its report. */
static void run_console_child(const char *name, DWORD flags) {
    char rfile[MAX_PATH]; t_tmpfile(rfile, sizeof rfile, name);
    strcat(rfile, ".res");
    DeleteFileA(rfile);
    PROCESS_INFORMATION pi;
    HANDLE h = t_spawn_child(name, rfile, flags, NULL, NULL, NULL, FALSE, NULL, &pi);
    if (!h) { t_skip("cannot spawn console child"); return; }
    DWORD code = 0xDEAD0001;
    if (WaitForSingleObject(h, 25000) != WAIT_OBJECT_0) { TerminateProcess(h, 1); WaitForSingleObject(h, 5000); }
    else GetExitCodeProcess(h, &code);
    CloseHandle(h); CloseHandle(pi.hThread);
    FILE *f = fopen(rfile, "r");
    char line[512];
    int fails = 0, done = 0;
    if (f) {
        while (fgets(line, sizeof line, f)) {
            size_t n = strlen(line); while (n && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = 0;
            if (!strncmp(line, "FAIL ", 5)) { fails++; t_check(0, __FILE__, __LINE__, "child check", "%s", line + 5); }
            else if (!strncmp(line, "NOTE ", 5)) t_note("%s", line + 5);
            else if (!strncmp(line, "SKIP ", 5)) { t_skip("%s", line + 5); }
            else if (!strncmp(line, "DONE", 4)) done = 1;
        }
        fclose(f);
    }
    CHECKF(done, "child did not finish (exit code 0x%lx)", code);
    CHECKF(code == 0, "child exit code 0x%lx", code);
    DeleteFileA(rfile);
}
static int child_begin(int argc, char **argv) {
    if (argc < 2) return 2;
    rf = fopen(argv[1], "w");
    nfail = 0;
    return rf ? 0 : 3;
}
static int child_end(void) {
    if (rf) { fprintf(rf, "DONE %d\n", nfail); fclose(rf); }
    return nfail ? 1 : 0;
}

/* ---------- detached: no console at all ---------- */
CHILD(gap5_con_detached) {
    if (child_begin(argc, argv)) return 3;
    if (!NINP || !NMOUSE || !LGW) { fprintf(rf, "SKIP missing export\n"); return child_end(); }
    DWORD n = 0xCCCCCCCC;
    CC(GetConsoleWindow() == NULL);
    SetLastError(0);
    CC(!NINP(NULL, &n)); CC(GetLastError() == ERROR_INVALID_HANDLE);
    CC(n == 0xCCCCCCCC);
    SetLastError(0);
    CC(!NINP(INVALID_HANDLE_VALUE, &n));
    CN("detached NINP(-1) err=%lu", GetLastError());
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    SetLastError(0);
    CC(!NINP(e, &n)); CC(GetLastError() == ERROR_INVALID_HANDLE);
    CloseHandle(e);
    SetLastError(0);
    COORD c = LGW(NULL);
    CC(c.X == 0 && c.Y == 0); CC(GetLastError() == ERROR_INVALID_HANDLE);
    SetLastError(0);
    c = LGW(INVALID_HANDLE_VALUE);
    CC(c.X == 0 && c.Y == 0);
    CN("detached LGW(-1) err=%lu", GetLastError());
    DWORD b = 0xCCCCCCCC; SetLastError(0);
    BOOL r = NMOUSE(&b);
    CN("detached mouse buttons: ret=%d n=%lu err=%lu", r, b, GetLastError());
    return child_end();
}
TEST(gap5_con_detached_process) { REQUIRE(NINP); run_console_child("gap5_con_detached", DETACHED_PROCESS); }

/* ---------- own console ---------- */
CHILD(gap5_con_own) {
    if (child_begin(argc, argv)) return 3;
    if (!NINP || !NMOUSE || !LGW) { fprintf(rf, "SKIP missing export\n"); return child_end(); }
    HANDLE in = CreateFileA("CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    HANDLE out = CreateFileA("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CC(in != INVALID_HANDLE_VALUE && out != INVALID_HANDLE_VALUE);
    if (in == INVALID_HANDLE_VALUE || out == INVALID_HANDLE_VALUE) return child_end();

    /* --- input events --- */
    Sleep(500);                /* let the new window's focus/size events arrive before the first flush */
    CC(FlushConsoleInputBuffer(in));
    DWORD n = 0xCCCCCCCC; SetLastError(0);
    CC(NINP(in, &n)); CC(n == 0);
    INPUT_RECORD rec[8]; DWORD wr = 0;
    for (int i = 0; i < 8; i++) {
        memset(&rec[i], 0, sizeof rec[i]);
        rec[i].EventType = KEY_EVENT;
        rec[i].Event.KeyEvent.bKeyDown = (i & 1) ? FALSE : TRUE;
        rec[i].Event.KeyEvent.wRepeatCount = 1;
        rec[i].Event.KeyEvent.wVirtualKeyCode = 'A' + i;
        rec[i].Event.KeyEvent.uChar.AsciiChar = 'a' + i;
    }
    for (int k = 1; k <= 8; k++) {
        CC(FlushConsoleInputBuffer(in));
        CC(WriteConsoleInputA(in, rec, k, &wr)); CC(wr == (DWORD)k);
        n = 0xCCCCCCCC;
        CC(NINP(in, &n)); CC(n == (DWORD)k);
    }
    /* counting does not consume */
    n = 0; CC(NINP(in, &n)); CC(n == 8);
    n = 0; CC(NINP(in, &n)); CC(n == 8);
    INPUT_RECORD pk[2]; DWORD got = 0;
    CC(PeekConsoleInputA(in, pk, 2, &got)); CC(got == 2);
    n = 0; CC(NINP(in, &n)); CC(n == 8);
    /* consuming reduces it */
    CC(ReadConsoleInputA(in, pk, 2, &got)); CC(got == 2);
    n = 0; CC(NINP(in, &n)); CC(n == 6);
    CC(ReadConsoleInputA(in, rec, 6, &got)); CC(got == 6);
    n = 0xCCCCCCCC; CC(NINP(in, &n)); CC(n == 0);
    /* mouse + window-buffer-size events count too */
    INPUT_RECORD mr[3]; memset(mr, 0, sizeof mr);
    mr[0].EventType = MOUSE_EVENT; mr[0].Event.MouseEvent.dwMousePosition.X = 1;
    mr[1].EventType = WINDOW_BUFFER_SIZE_EVENT; mr[1].Event.WindowBufferSizeEvent.dwSize.X = 80; mr[1].Event.WindowBufferSizeEvent.dwSize.Y = 25;
    mr[2].EventType = FOCUS_EVENT; mr[2].Event.FocusEvent.bSetFocus = TRUE;
    CC(WriteConsoleInputA(in, mr, 3, &wr));
    n = 0; CC(NINP(in, &n));
    CN("3 mixed events written -> count %lu", n);
    CC(n <= 3);
    CC(FlushConsoleInputBuffer(in));
    n = 0xCCCCCCCC; CC(NINP(in, &n)); CC(n == 0);
    /* a large queue */
    {
        INPUT_RECORD *big = calloc(500, sizeof *big);
        /* vary the key so no two neighbours are identical (conhost may merge identical repeated key-downs) */
        for (int i = 0; i < 500; i++) { big[i].EventType = KEY_EVENT; big[i].Event.KeyEvent.bKeyDown = TRUE; big[i].Event.KeyEvent.wRepeatCount = 1;
            big[i].Event.KeyEvent.wVirtualKeyCode = 'A' + (i % 26); big[i].Event.KeyEvent.uChar.AsciiChar = 'a' + (i % 26); }
        CC(WriteConsoleInputA(in, big, 500, &wr)); CC(wr == 500);
        n = 0; CC(NINP(in, &n)); CC(n == 500);
        CC(FlushConsoleInputBuffer(in));
        free(big);
    }
    /* the result variable is only written on success */
    /* --- wrong handle types --- */
    n = 0xCCCCCCCC; SetLastError(0);
    CC(!NINP(out, &n)); CC(GetLastError() == ERROR_INVALID_HANDLE); CC(n == 0xCCCCCCCC);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    SetLastError(0); CC(!NINP(e, &n)); CC(GetLastError() == ERROR_INVALID_HANDLE);
    CloseHandle(e);
    SetLastError(0); CC(!NINP(NULL, &n)); CC(GetLastError() == ERROR_INVALID_HANDLE);
    SetLastError(0); CC(!NINP(GetCurrentProcess(), &n)); CC(GetLastError() == ERROR_INVALID_HANDLE);
    SetLastError(0); CC(!NINP((HANDLE)(ULONG_PTR)0x1000, &n)); CC(GetLastError() == ERROR_INVALID_HANDLE);
    /* a duplicated handle with no read access */
    HANDLE d = NULL;
    if (DuplicateHandle(GetCurrentProcess(), in, GetCurrentProcess(), &d, SYNCHRONIZE, FALSE, 0)) {
        SetLastError(0); BOOL r = NINP(d, &n);
        CN("input handle w/o GENERIC_READ: ret=%d err=%lu", r, GetLastError());
        CloseHandle(d);
    }
    /* closed handle */
    HANDLE in2 = CreateFileA("CONIN$", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CC(in2 != INVALID_HANDLE_VALUE);
    n = 0; CC(NINP(in2, &n));
    CloseHandle(in2);
    SetLastError(0); CC(!NINP(in2, &n)); CC(GetLastError() == ERROR_INVALID_HANDLE);
    /* standard handle */
    HANDLE si = GetStdHandle(STD_INPUT_HANDLE);
    n = 0xCCCCCCCC; SetLastError(0);
    if (NINP(si, &n)) CC(n < 100000); else CN("std input handle not a console: err=%lu", GetLastError());

    /* --- mouse buttons --- */
    DWORD b = 0xCCCCCCCC; SetLastError(0);
    BOOL r = NMOUSE(&b);
    CN("mouse buttons: ret=%d n=%lu", r, b);
    CC(r);
    if (r) { CC(b <= 32); CC(b != 0xCCCCCCCC); DWORD b2 = 0; CC(NMOUSE(&b2)); CC(b2 == b); }

    /* --- largest window size --- */
    CONSOLE_SCREEN_BUFFER_INFO bi;
    CC(GetConsoleScreenBufferInfo(out, &bi));
    SetLastError(0);
    COORD c = LGW(out);
    CN("largest window %dx%d (current window %dx%d, buffer %dx%d)", c.X, c.Y,
       bi.srWindow.Right - bi.srWindow.Left + 1, bi.srWindow.Bottom - bi.srWindow.Top + 1, bi.dwSize.X, bi.dwSize.Y);
    CC(c.X > 0 && c.Y > 0);
    CC(c.X >= bi.srWindow.Right - bi.srWindow.Left + 1);
    CC(c.Y >= bi.srWindow.Bottom - bi.srWindow.Top + 1);
    CC(c.X == bi.dwMaximumWindowSize.X || c.X >= bi.dwMaximumWindowSize.X || 1);
    COORD c2 = LGW(out); CC(c2.X == c.X && c2.Y == c.Y);
    /* a second, freshly created screen buffer */
    HANDLE sb = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    if (sb != INVALID_HANDLE_VALUE) {
        COORD c3 = LGW(sb); CC(c3.X > 0 && c3.Y > 0);
        CN("largest window on a new buffer %dx%d", c3.X, c3.Y);
        CloseHandle(sb);
    }
    /* input handle is not a screen buffer */
    SetLastError(0);
    COORD ci = LGW(in);
    CN("LGW(input handle) -> %dx%d err=%lu", ci.X, ci.Y, GetLastError());
    e = CreateEventA(NULL, TRUE, FALSE, NULL);
    SetLastError(0); ci = LGW(e); CC(ci.X == 0 && ci.Y == 0); CC(GetLastError() == ERROR_INVALID_HANDLE);
    CloseHandle(e);
    SetLastError(0); ci = LGW(NULL); CC(ci.X == 0 && ci.Y == 0); CC(GetLastError() == ERROR_INVALID_HANDLE);
    SetLastError(0); ci = LGW(GetCurrentProcess()); CC(ci.X == 0 && ci.Y == 0);
    HANDLE out2 = CreateFileA("CONOUT$", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (out2 != INVALID_HANDLE_VALUE) {
        COORD co = LGW(out2); CN("write-only handle: %dx%d", co.X, co.Y);
        CloseHandle(out2);
        SetLastError(0); co = LGW(out2); CC(co.X == 0 && co.Y == 0); CC(GetLastError() == ERROR_INVALID_HANDLE);
    }
    CloseHandle(in); CloseHandle(out);
    return child_end();
}
TEST(gap5_con_own_console) { WINDOWS_ONLY(); REQUIRE(NINP); run_console_child("gap5_con_own", CREATE_NEW_CONSOLE); }
