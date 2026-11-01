/* console_apis: remaining exports, SGR baseline stability, boundary coordinates */
#include "ch.h"

#define FAILS(call, err)  do { SetLastError(0xBAD); BOOL r_ = (call); DWORD le_ = GetLastError(); CEQ(r_, 0); CERRV(le_, err); } while (0)

CT(con_x_misc_exports_present) {
    CC(t_proc_k32("CtrlRoutine") != NULL);
    CC(t_proc_k32("OpenConsoleW") != NULL);
    CC(t_proc_k32("WTSGetActiveConsoleSessionId") != NULL);
    CC(t_proc_k32("GetConsoleInputWaitHandle") != NULL || t_is_wine());
    CC(t_proc_k32("SetConsoleMaximumWindowSize") != NULL || t_is_wine());
}

CT(con_x_wts_active_console_session) {
    typedef DWORD (WINAPI *f_t)(void);
    f_t f = (f_t)t_proc_k32("WTSGetActiveConsoleSessionId");
    CREQ(f);
    DWORD a = f(), b = f();
    CEQ(a, b);
    CC(a == 0xFFFFFFFF || a < 1000);
    cnote("active console session %lu", a);
}

CT(con_x_sgr_reset_is_idempotent) {
    CREQ(WriteConsoleW);
    HANDLE b = mkbuf_std(); if (!b) return;
    if (!SetConsoleMode(b, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) { CloseHandle(b); return; }
    DWORD n;
    CC(SetConsoleTextAttribute(b, 0x1E));
    WriteConsoleW(b, L"\x1b[0m\x1b[1;1H1", 11, &n, NULL);
    WORD a1 = cellattr(b, 0, 0);
    CC(SetConsoleTextAttribute(b, 0x4A));
    WriteConsoleW(b, L"\x1b[m\x1b[1;2H2", 10, &n, NULL);
    WORD a2 = cellattr(b, 1, 0);
    CEQ(a1, a2);
    CloseHandle(b);
}

CT(con_x_cursor_position_bounds) {
    CREQ(SetConsoleCursorPosition);
    HANDLE b = mkbuf_std(); if (!b) return;
    CC(SetConsoleCursorPosition(b, co(0, 0)));
    CC(SetConsoleCursorPosition(b, co(79, 24)));
    CC(coeq(curpos(b), 79, 24));
    COORD bad[] = { {80, 0}, {0, 25}, {-1, 0}, {0, -1}, {-32768, -32768}, {32767, 32767} };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        FAILS(SetConsoleCursorPosition(b, bad[i]), ERROR_INVALID_PARAMETER);
        CC(coeq(curpos(b), 79, 24));
    }
    CloseHandle(b);
}

CT(con_x_fill_zero_length) {
    CREQ(FillConsoleOutputCharacterW); CREQ(FillConsoleOutputAttribute);
    HANDLE b = mkbuf_std(); if (!b) return;
    DWORD n = 0xAA;
    SetLastError(0xBAD);
    BOOL r = FillConsoleOutputCharacterW(b, L'Q', 0, co(0, 0), &n);
    cnote("Fill length 0 -> %d n=%lu err=%lu", r, n, GetLastError());
    if (r) CEQ(n, 0);
    CEQ(cellch(b, 0, 0), L' ');
    n = 0xAA;
    r = FillConsoleOutputAttribute(b, 0x70, 0, co(0, 0), &n);
    if (r) CEQ(n, 0);
    CEQ(cellattr(b, 0, 0), 7);
    CloseHandle(b);
}

CT(con_x_fill_clamps_to_buffer_end) {
    CREQ(FillConsoleOutputCharacterW);
    HANDLE b = mkbuf_std(); if (!b) return;
    DWORD n = 0;
    CC(FillConsoleOutputCharacterW(b, L'Z', 100000, co(70, 24), &n));
    CEQ(n, 10);
    CEQ(cellch(b, 69, 24), L' ');
    CEQ(cellch(b, 70, 24), L'Z');
    CEQ(cellch(b, 79, 24), L'Z');
    CloseHandle(b);
}

CT(con_x_write_char_clamps_and_counts) {
    CREQ(WriteConsoleOutputCharacterW);
    HANDLE b = mkbuf_std(); if (!b) return;
    WCHAR s[20]; for (int i = 0; i < 20; i++) s[i] = L'a' + i;
    DWORD n = 0;
    CC(WriteConsoleOutputCharacterW(b, s, 20, co(75, 24), &n));
    CEQ(n, 5);
    CEQ(cellch(b, 79, 24), L'e');
    CC(coeq(curpos(b), 0, 0));   /* cursor not moved by this API */
    CloseHandle(b);
}

CT(con_x_title_roundtrip_unicode) {
    CREQ(SetConsoleTitleW); CREQ(GetConsoleTitleW);
    static const WCHAR t[] = { 'k', '3', '2', 't', 0xE9, 0x4E2D, 0 };
    WCHAR g[64];
    CC(SetConsoleTitleW(t));
    DWORD n = GetConsoleTitleW(g, 64);
    CEQ(n, 6);
    CWSTR(g, t);
}
