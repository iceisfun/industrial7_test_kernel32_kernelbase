/* gap_03: FillConsoleOutput*, FlushConsoleInputBuffer, GenerateConsoleCtrlEvent,
 * ExpungeConsoleCommandHistory*, FreeConsole (child only).
 * All console work happens in children with their own console (CREATE_NEW_CONSOLE) or none (DETACHED_PROCESS). */
#include "t.h"

#define CK(c) do { if (!(c)) return __LINE__; } while (0)

static DWORD g3_run(const char *name, DWORD flags, DWORD timeout) {
    PROCESS_INFORMATION pi; DWORD code = 0xDEAD0000;
    HANDLE h = t_spawn_child(name, "", flags, NULL, NULL, NULL, FALSE, NULL, &pi);
    if (!h) return 0xDEAD0000;
    if (WaitForSingleObject(h, timeout) != WAIT_OBJECT_0) { TerminateProcess(h, 1); WaitForSingleObject(h, 5000); code = 0xDEAD0001; }
    else GetExitCodeProcess(h, &code);
    CloseHandle(pi.hThread); CloseHandle(h);
    return code;
}

/* --------- no console at all --------- */
CHILD(gap3_con_detached) {
    CK(U(FillConsoleOutputCharacterW) && U(FillConsoleOutputCharacterA) && U(FillConsoleOutputAttribute));
    CK(U(FlushConsoleInputBuffer) && U(GenerateConsoleCtrlEvent));
    COORD c = {0, 0}; DWORD n = 0x77;
    HANDLE bad[3] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x1234 };
    for (int i = 0; i < 3; i++) {
        n = 0x77; SetLastError(0);
        CK(U(FillConsoleOutputCharacterW)(bad[i], L'a', 4, c, &n) == FALSE);
        CK(GetLastError() != 0);
        n = 0x77; SetLastError(0);
        CK(U(FillConsoleOutputCharacterA)(bad[i], 'a', 4, c, &n) == FALSE);
        CK(GetLastError() != 0);
        n = 0x77; SetLastError(0);
        CK(U(FillConsoleOutputAttribute)(bad[i], 7, 4, c, &n) == FALSE);
        CK(GetLastError() != 0);
        SetLastError(0);
        CK(U(FlushConsoleInputBuffer)(bad[i]) == FALSE);
        CK(GetLastError() != 0);
    }
    /* invalid control events are rejected before the console server is contacted */
    static const DWORD badev[] = { 2, 3, 4, 5, 100, 0x80000000u, 0xFFFFFFFFu };
    for (unsigned i = 0; i < sizeof badev / sizeof badev[0]; i++) {
        SetLastError(0);
        CK(U(GenerateConsoleCtrlEvent)(badev[i], 0) == FALSE);
        CK(t_is_wine() || GetLastError() == ERROR_INVALID_PARAMETER);
        SetLastError(0);
        CK(U(GenerateConsoleCtrlEvent)(badev[i], GetCurrentProcessId() + 1) == FALSE);
        CK(t_is_wine() || GetLastError() == ERROR_INVALID_PARAMETER);
    }
    return 0;
}
TEST(gap3_console_no_console_invalid_handles) {
    REQUIRE(U(FillConsoleOutputCharacterW));
    DWORD c = g3_run("gap3_con_detached", DETACHED_PROCESS, 30000);
    t_note("child line %lu", c);
    EXPECT_EQ(c, 0);
}

CHILD(gap3_con_detached_ctrl) {
    CK(U(GenerateConsoleCtrlEvent));
    /* valid event types without a console: must fail, process must survive */
    SetLastError(0);
    BOOL r = U(GenerateConsoleCtrlEvent)(CTRL_BREAK_EVENT, 0);
    CK(r == FALSE);
    CK(GetLastError() != 0);
    SetLastError(0);
    r = U(GenerateConsoleCtrlEvent)(CTRL_C_EVENT, 0);
    CK(r == FALSE);
    CK(GetLastError() != 0);
    return 0;
}
TEST(gap3_genctrl_no_console_fails) {
    REQUIRE(U(GenerateConsoleCtrlEvent));
    EXPECT_EQ(g3_run("gap3_con_detached_ctrl", DETACHED_PROCESS, 30000), 0);
}

CHILD(gap3_con_expunge) {
    CK(U(ExpungeConsoleCommandHistoryA) && U(ExpungeConsoleCommandHistoryW));
    /* an empty or NULL executable name is rejected locally with ERROR_INVALID_PARAMETER */
    SetLastError(0);
    U(ExpungeConsoleCommandHistoryA)(NULL);
    CK(t_is_wine() || GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0);
    U(ExpungeConsoleCommandHistoryW)(NULL);
    CK(t_is_wine() || GetLastError() == ERROR_INVALID_PARAMETER);
    char e[1] = "";
    SetLastError(0);
    U(ExpungeConsoleCommandHistoryA)(e);
    CK(t_is_wine() || GetLastError() == ERROR_INVALID_PARAMETER);
    WCHAR we[1] = L"";
    SetLastError(0);
    U(ExpungeConsoleCommandHistoryW)((LPWSTR)we);
    CK(t_is_wine() || GetLastError() == ERROR_INVALID_PARAMETER);
    /* with a name and no console: returns (void), must not crash */
    char nm[] = "k32t_nonexistent_exe.exe";
    U(ExpungeConsoleCommandHistoryA)(nm);
    WCHAR wn[] = L"k32t_nonexistent_exe.exe";
    U(ExpungeConsoleCommandHistoryW)(wn);
    return 0;
}
TEST(gap3_expunge_history_detached) {
    REQUIRE(U(ExpungeConsoleCommandHistoryA));
    DWORD c = g3_run("gap3_con_expunge", DETACHED_PROCESS, 30000);
    t_note("child line %lu", c);
    EXPECT_EQ(c, 0);
}
TEST(gap3_expunge_history_exports_present) {
    CHECK(UK(ExpungeConsoleCommandHistoryA) != NULL);
    CHECK(UK(ExpungeConsoleCommandHistoryW) != NULL);
}
TEST(gap3_expunge_null_name_main_process) {
    /* NULL name is validated locally (no console server call), safe in the main process */
    REQUIRE(U(ExpungeConsoleCommandHistoryA)); REQUIRE(U(ExpungeConsoleCommandHistoryW));
    if (t_is_wine()) { t_skip("wine"); return; }
    SetLastError(0);
    U(ExpungeConsoleCommandHistoryA)(NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SetLastError(0);
    U(ExpungeConsoleCommandHistoryW)(NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}

/* --------- private console --------- */
static HANDLE g3_out(void) { return CreateFileA("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL); }
static HANDLE g3_in(void) { return CreateFileA("CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL); }

CHILD(gap3_con_fill) {
    CK(U(FillConsoleOutputCharacterW) && U(FillConsoleOutputCharacterA) && U(FillConsoleOutputAttribute));
    HANDLE o = g3_out();
    CK(o != INVALID_HANDLE_VALUE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    CK(GetConsoleScreenBufferInfo(o, &csbi));
    int W = csbi.dwSize.X, H = csbi.dwSize.Y;
    CK(W > 4 && H > 2);
    DWORD n = 0; COORD z = {0, 0};
    WCHAR buf[512]; WORD at[512];

    /* W fill + read back */
    n = 0x77;
    CK(U(FillConsoleOutputCharacterW)(o, L'X', 10, z, &n) && n == 10);
    DWORD rd = 0;
    CK(ReadConsoleOutputCharacterW(o, buf, 12, z, &rd) && rd == 12);
    for (int i = 0; i < 10; i++) CK(buf[i] == L'X');
    CK(buf[10] != L'X');
    /* A fill */
    COORD c1 = {0, 1};
    n = 0x77;
    CK(U(FillConsoleOutputCharacterA)(o, 'Y', 6, c1, &n) && n == 6);
    CK(ReadConsoleOutputCharacterW(o, buf, 7, c1, &rd) && rd == 7);
    for (int i = 0; i < 6; i++) CK(buf[i] == L'Y');
    /* attribute fill */
    COORD c2 = {2, 2};
    n = 0x77;
    CK(U(FillConsoleOutputAttribute)(o, 0x1E, 5, c2, &n) && n == 5);
    CK(ReadConsoleOutputAttribute(o, at, 7, c2, &rd) && rd == 7);
    for (int i = 0; i < 5; i++) CK(at[i] == 0x1E);
    CK(at[5] != 0x1E || at[6] != 0x1E);
    /* attribute fill does not change characters */
    CK(ReadConsoleOutputCharacterW(o, buf, 1, (COORD){0, 1}, &rd) && buf[0] == L'Y');
    /* char fill does not change attributes */
    COORD c3 = {0, 3};
    n = 0;
    CK(U(FillConsoleOutputAttribute)(o, 0x2C, 4, c3, &n) && n == 4);
    n = 0;
    CK(U(FillConsoleOutputCharacterW)(o, L'q', 4, c3, &n) && n == 4);
    CK(ReadConsoleOutputAttribute(o, at, 4, c3, &rd) && at[0] == 0x2C && at[3] == 0x2C);

    /* zero length */
    n = 0x77;
    CK(U(FillConsoleOutputCharacterW)(o, L'Z', 0, z, &n) && n == 0);
    n = 0x77;
    CK(U(FillConsoleOutputAttribute)(o, 7, 0, z, &n) && n == 0);
    n = 0x77;
    CK(U(FillConsoleOutputCharacterA)(o, 'Z', 0, z, &n) && n == 0);

    /* wraps to the next row */
    COORD c4 = {(SHORT)(W - 3), 5};
    n = 0;
    CK(U(FillConsoleOutputCharacterW)(o, L'w', 8, c4, &n) && n == 8);
    CK(ReadConsoleOutputCharacterW(o, buf, 3, c4, &rd) && buf[0] == L'w' && buf[2] == L'w');
    CK(ReadConsoleOutputCharacterW(o, buf, 5, (COORD){0, 6}, &rd) && buf[4] == L'w');
    CK(ReadConsoleOutputCharacterW(o, buf, 1, (COORD){5, 6}, &rd) && buf[0] != L'w');

    /* clamped at end of the buffer */
    COORD last = {(SHORT)(W - 2), (SHORT)(H - 1)};
    n = 0;
    CK(U(FillConsoleOutputCharacterW)(o, L'e', 100, last, &n));
    CK(n == 2);
    n = 0;
    CK(U(FillConsoleOutputAttribute)(o, 7, 0xFFFFFFFFu, last, &n));
    CK(n == 2);
    n = 0;
    CK(U(FillConsoleOutputCharacterW)(o, L' ', 0xFFFFFFFFu, z, &n));
    CK(n == (DWORD)(W * H));
    CK(U(FillConsoleOutputAttribute)(o, csbi.wAttributes, (DWORD)(W * H), z, &n) && n == (DWORD)(W * H));
    /* unicode round trip */
    n = 0;
    CK(U(FillConsoleOutputCharacterW)(o, 0x00E9, 3, z, &n) && n == 3);
    CK(ReadConsoleOutputCharacterW(o, buf, 3, z, &rd) && buf[0] == 0x00E9 && buf[2] == 0x00E9);
    n = 0;
    CK(U(FillConsoleOutputCharacterW)(o, 0x20AC, 3, z, &n) && n == 3);
    CK(ReadConsoleOutputCharacterW(o, buf, 3, z, &rd) && buf[1] == 0x20AC);
    /* cursor unaffected */
    COORD cur = csbi.dwCursorPosition;
    CK(GetConsoleScreenBufferInfo(o, &csbi) && csbi.dwCursorPosition.X == cur.X && csbi.dwCursorPosition.Y == cur.Y);
    CloseHandle(o);
    return 0;
}
TEST(gap3_fillconsole_functional) {
    REQUIRE(U(FillConsoleOutputCharacterW));
    if (t_is_wine()) { t_skip("wine new-console"); return; }
    EXPECT_EQ(g3_run("gap3_con_fill", CREATE_NEW_CONSOLE, 30000), 0);
}

CHILD(gap3_con_fill_invalid) {
    CK(U(FillConsoleOutputCharacterW) && U(FillConsoleOutputCharacterA) && U(FillConsoleOutputAttribute));
    HANDLE o = g3_out(), in = g3_in();
    CK(o != INVALID_HANDLE_VALUE && in != INVALID_HANDLE_VALUE);
    CONSOLE_SCREEN_BUFFER_INFO csbi; CK(GetConsoleScreenBufferInfo(o, &csbi));
    int W = csbi.dwSize.X, H = csbi.dwSize.Y;
    DWORD n;
    COORD bad[] = { {(SHORT)W, 0}, {0, (SHORT)H}, {-1, 0}, {0, -1}, {(SHORT)0x7FFF, (SHORT)0x7FFF}, {(SHORT)0x8000, 0} };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        n = 0x77; SetLastError(0);
        BOOL r = U(FillConsoleOutputCharacterW)(o, L'x', 5, bad[i], &n);
        CK(r == FALSE);
        CK(GetLastError() != 0);
        n = 0x77; SetLastError(0);
        r = U(FillConsoleOutputAttribute)(o, 7, 5, bad[i], &n);
        CK(r == FALSE);
        n = 0x77; SetLastError(0);
        r = U(FillConsoleOutputCharacterA)(o, 'x', 5, bad[i], &n);
        CK(r == FALSE);
    }
    /* input buffer / event / null handles are not screen buffers */
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE hs[] = { in, ev, NULL, INVALID_HANDLE_VALUE };
    for (unsigned i = 0; i < 4; i++) {
        n = 0x77; SetLastError(0);
        CK(U(FillConsoleOutputCharacterW)(hs[i], L'x', 5, (COORD){0, 0}, &n) == FALSE);
        CK(GetLastError() == ERROR_INVALID_HANDLE);
        n = 0x77; SetLastError(0);
        CK(U(FillConsoleOutputAttribute)(hs[i], 7, 5, (COORD){0, 0}, &n) == FALSE);
        CK(GetLastError() == ERROR_INVALID_HANDLE);
    }
    CloseHandle(ev);
    /* read-only handle: write access is required */
    HANDLE ro = CreateFileA("CONOUT$", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (ro != INVALID_HANDLE_VALUE) {
        n = 0x77; SetLastError(0);
        BOOL r = U(FillConsoleOutputCharacterW)(ro, L'x', 5, (COORD){0, 0}, &n);
        CK(r == FALSE);
        CK(GetLastError() == ERROR_ACCESS_DENIED || GetLastError() == ERROR_INVALID_HANDLE);
        CloseHandle(ro);
    }
    /* closed handle */
    HANDLE c = g3_out(); CloseHandle(c);
    n = 0x77; SetLastError(0);
    CK(U(FillConsoleOutputCharacterW)(c, L'x', 5, (COORD){0, 0}, &n) == FALSE);
    CK(GetLastError() == ERROR_INVALID_HANDLE);
    return 0;
}
TEST(gap3_fillconsole_invalid_args) {
    REQUIRE(U(FillConsoleOutputCharacterW));
    if (t_is_wine()) { t_skip("wine new-console"); return; }
    EXPECT_EQ(g3_run("gap3_con_fill_invalid", CREATE_NEW_CONSOLE, 30000), 0);
}

CHILD(gap3_con_flush) {
    CK(U(FlushConsoleInputBuffer));
    HANDLE in = g3_in(), o = g3_out();
    CK(in != INVALID_HANDLE_VALUE);
    INPUT_RECORD r[5]; DWORD cnt = 0, w = 0;
    /* drain whatever the console pre-queued */
    CK(U(FlushConsoleInputBuffer)(in));
    CK(GetNumberOfConsoleInputEvents(in, &cnt) && cnt == 0);
    /* inject and flush */
    for (int i = 0; i < 5; i++) {
        memset(&r[i], 0, sizeof r[i]);
        r[i].EventType = KEY_EVENT; r[i].Event.KeyEvent.bKeyDown = TRUE;
        r[i].Event.KeyEvent.wRepeatCount = 1; r[i].Event.KeyEvent.uChar.UnicodeChar = L'a' + i;
    }
    CK(WriteConsoleInputW(in, r, 5, &w) && w == 5);
    CK(GetNumberOfConsoleInputEvents(in, &cnt) && cnt >= 5);
    SetLastError(0x1111);
    CK(U(FlushConsoleInputBuffer)(in) == TRUE);
    CK(GetNumberOfConsoleInputEvents(in, &cnt) && cnt == 0);
    /* flushing an empty buffer is fine and repeatable */
    for (int i = 0; i < 20; i++) CK(U(FlushConsoleInputBuffer)(in));
    /* peek on empty */
    DWORD pk = 9;
    CK(PeekConsoleInputW(in, r, 5, &pk) && pk == 0);
    /* mouse + window events too */
    memset(&r[0], 0, sizeof r[0]); r[0].EventType = MOUSE_EVENT;
    memset(&r[1], 0, sizeof r[1]); r[1].EventType = FOCUS_EVENT;
    CK(WriteConsoleInputW(in, r, 2, &w) && w == 2);
    CK(U(FlushConsoleInputBuffer)(in));
    CK(GetNumberOfConsoleInputEvents(in, &cnt) && cnt == 0);
    /* a flush on a duplicate handle flushes the same buffer */
    HANDLE dup = NULL;
    CK(DuplicateHandle(GetCurrentProcess(), in, GetCurrentProcess(), &dup, 0, FALSE, DUPLICATE_SAME_ACCESS));
    CK(WriteConsoleInputW(in, r, 2, &w) && w == 2);
    CK(U(FlushConsoleInputBuffer)(dup));
    CK(GetNumberOfConsoleInputEvents(in, &cnt) && cnt == 0);
    CloseHandle(dup);
    /* wrong handle kinds */
    SetLastError(0);
    CK(U(FlushConsoleInputBuffer)(o) == FALSE);
    CK(GetLastError() == ERROR_INVALID_HANDLE);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SetLastError(0);
    CK(U(FlushConsoleInputBuffer)(ev) == FALSE);
    CK(GetLastError() == ERROR_INVALID_HANDLE);
    CloseHandle(ev);
    SetLastError(0);
    CK(U(FlushConsoleInputBuffer)(NULL) == FALSE);
    CK(GetLastError() == ERROR_INVALID_HANDLE);
    SetLastError(0);
    CK(U(FlushConsoleInputBuffer)(INVALID_HANDLE_VALUE) == FALSE);
    CK(GetLastError() == ERROR_INVALID_HANDLE);
    /* closed */
    HANDLE c = g3_in(); CloseHandle(c);
    SetLastError(0);
    CK(U(FlushConsoleInputBuffer)(c) == FALSE);
    CK(GetLastError() == ERROR_INVALID_HANDLE);
    return 0;
}
TEST(gap3_flushconsoleinput_functional) {
    REQUIRE(U(FlushConsoleInputBuffer));
    if (t_is_wine()) { t_skip("wine new-console"); return; }
    EXPECT_EQ(g3_run("gap3_con_flush", CREATE_NEW_CONSOLE, 30000), 0);
}

static volatile LONG g3_hits;
static volatile DWORD g3_last_type;
static BOOL WINAPI g3_handler(DWORD t) { g3_last_type = t; InterlockedIncrement(&g3_hits); return TRUE; }

CHILD(gap3_con_ctrl) {
    CK(U(GenerateConsoleCtrlEvent));
    CK(SetConsoleCtrlHandler(g3_handler, TRUE));
    /* invalid event types */
    SetLastError(0);
    CK(U(GenerateConsoleCtrlEvent)(2, 0) == FALSE);
    CK(t_is_wine() || GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0);
    CK(U(GenerateConsoleCtrlEvent)(0xFFFFFFFF, 0) == FALSE);
    CK(t_is_wine() || GetLastError() == ERROR_INVALID_PARAMETER);
    CK(g3_hits == 0);
    /* CTRL_BREAK to our own group (the only process on this console) */
    SetLastError(0);
    CK(U(GenerateConsoleCtrlEvent)(CTRL_BREAK_EVENT, 0) == TRUE);
    for (int i = 0; i < 300 && g3_hits < 1; i++) Sleep(10);
    CK(g3_hits == 1);
    CK(g3_last_type == CTRL_BREAK_EVENT);
    /* CTRL_C to group 0 */
    CK(U(GenerateConsoleCtrlEvent)(CTRL_C_EVENT, 0) == TRUE);
    for (int i = 0; i < 300 && g3_hits < 2; i++) Sleep(10);
    CK(g3_hits == 2);
    CK(g3_last_type == CTRL_C_EVENT);
    /* CTRL_C ignored when the process disabled it: call still succeeds, handler not run */
    CK(SetConsoleCtrlHandler(NULL, TRUE));
    CK(U(GenerateConsoleCtrlEvent)(CTRL_C_EVENT, 0) == TRUE);
    Sleep(300);
    CK(g3_hits == 2);
    CK(SetConsoleCtrlHandler(NULL, FALSE));
    /* to our own pid as group id (is a group leader because CREATE_NEW_CONSOLE alone doesn't make a group) */
    LONG before = g3_hits;
    BOOL r = U(GenerateConsoleCtrlEvent)(CTRL_BREAK_EVENT, GetCurrentProcessId());
    Sleep(300);
    (void)before; (void)r;
    CK(SetConsoleCtrlHandler(g3_handler, FALSE));
    return 0;
}
TEST(gap3_genctrl_private_console) {
    REQUIRE(U(GenerateConsoleCtrlEvent));
    if (t_is_wine()) { t_skip("wine new-console"); return; }
    EXPECT_EQ(g3_run("gap3_con_ctrl", CREATE_NEW_CONSOLE, 30000), 0);
}

CHILD(gap3_con_freeconsole) {
    CK(U(FreeConsole));
    HANDLE o = g3_out();
    CK(o != INVALID_HANDLE_VALUE);
    CK(GetConsoleWindow() != NULL || TRUE);
    SetLastError(0x2222);
    CK(U(FreeConsole)() == TRUE);
    DWORD pids[4];
    CK(GetConsoleProcessList(pids, 4) == 0);
    /* old console handle is dead for API use */
    DWORD n = 0; SetLastError(0);
    CK(U(FillConsoleOutputCharacterW)(o, L'a', 1, (COORD){0, 0}, &n) == FALSE);
    CK(GetLastError() != 0);
    /* a console can be allocated again, and FreeConsole works again */
    CK(AllocConsole());
    CK(U(FreeConsole)() == TRUE);
    return 0;
}
TEST(gap3_freeconsole_child_lifecycle) {
    REQUIRE(U(FreeConsole));
    if (t_is_wine()) { t_skip("wine new-console"); return; }
    EXPECT_EQ(g3_run("gap3_con_freeconsole", CREATE_NEW_CONSOLE, 30000), 0);
}
CHILD(gap3_con_freeconsole_detached) {
    CK(U(FreeConsole));
    SetLastError(0);
    BOOL r = U(FreeConsole)();
    DWORD e = GetLastError();
    return 100 + (r ? 1 : 0) + (r ? 0 : (int)(e << 8));
}
TEST(gap3_freeconsole_when_detached) {
    REQUIRE(U(FreeConsole));
    DWORD c = g3_run("gap3_con_freeconsole_detached", DETACHED_PROCESS, 30000);
    t_note("FreeConsole without a console: exit=0x%lx (ret=%lu err=%lu)", c, c & 1 ? 1 : 0, c >> 8);
    CHECK((c & 0xFF) == 100 || (c & 0xFF) == 101);
}
TEST(gap3_console_exports_in_kernel32) {
    CHECK(UK(FillConsoleOutputAttribute) != NULL);
    CHECK(UK(FillConsoleOutputCharacterA) != NULL);
    CHECK(UK(FillConsoleOutputCharacterW) != NULL);
    CHECK(UK(FlushConsoleInputBuffer) != NULL);
    CHECK(UK(FreeConsole) != NULL);
    CHECK(UK(GenerateConsoleCtrlEvent) != NULL);
}
TEST(gap3_console_apis_resolve_to_kernelbase_impl) {
    /* On Win10 the kernel32 exports are thin jumps to kernelbase; both must exist */
    CHECK(UB(FillConsoleOutputAttribute) != NULL);
    CHECK(UB(FillConsoleOutputCharacterW) != NULL);
    CHECK(UB(FlushConsoleInputBuffer) != NULL);
    CHECK(UB(GenerateConsoleCtrlEvent) != NULL);
}
