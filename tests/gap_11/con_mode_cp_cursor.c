/* gap_11 console: SetConsoleMode, SetConsoleCP, SetConsoleOutputCP, SetConsoleCursorInfo, SetConsoleCursorPosition.
 * Each body runs in a child with a private console (or none: CTD). */
#include "con_h.h"

#define SETMODE U(SetConsoleMode)
#define SETCP U(SetConsoleCP)
#define SETOCP U(SetConsoleOutputCP)
#define SETCI U(SetConsoleCursorInfo)
#define SETCP_POS U(SetConsoleCursorPosition)

/* ---------------- SetConsoleMode ---------------- */
CT(gap11_con_mode_input_roundtrip) {
    CREQ(SETMODE);
    HANDLE hi = hin_(); DWORD orig = 0, m;
    CC(GetConsoleMode(hi, &orig));
    const DWORD pub = 0x1F | 0x200;     /* processed, line, echo, window, mouse, VT input */
    DWORD sets[] = { 0, ENABLE_PROCESSED_INPUT, ENABLE_LINE_INPUT, ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT, 7,
                     ENABLE_WINDOW_INPUT, ENABLE_MOUSE_INPUT, ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT,
                     ENABLE_VIRTUAL_TERMINAL_INPUT, ENABLE_VIRTUAL_TERMINAL_INPUT | ENABLE_PROCESSED_INPUT, 0x1F | 0x200 };
    for (size_t i = 0; i < sizeof sets / sizeof sets[0]; i++) {
        SetLastError(0xBAD);
        BOOL r = SETMODE(hi, sets[i] | ENABLE_EXTENDED_FLAGS);
        CCF(r, "set 0x%lx err=%lu", sets[i], GetLastError());
        m = 0xFFFFFFFF;
        CC(GetConsoleMode(hi, &m));
        CCF((m & pub) == (sets[i] & pub), "set 0x%lx -> get 0x%lx", sets[i], m);
    }
    CC(SETMODE(hi, orig));
}
CT(gap11_con_mode_input_echo_requires_line) {
    CREQ(SETMODE);
    HANDLE hi = hin_(); DWORD orig = 0, m = 0;
    CC(GetConsoleMode(hi, &orig));
    /* ENABLE_ECHO_INPUT without ENABLE_LINE_INPUT is rejected */
    DWORD bad[] = { ENABLE_ECHO_INPUT, ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT, ENABLE_ECHO_INPUT | ENABLE_MOUSE_INPUT };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        FAILS(SETMODE(hi, bad[i]), ERROR_INVALID_PARAMETER);
    }
    CC(SETMODE(hi, orig));
    CC(GetConsoleMode(hi, &m));
    CEQ(m & 0x3FF, orig & 0x3FF);
}
CT(gap11_con_mode_input_undefined_bits) {
    CREQ(SETMODE);
    HANDLE hi = hin_(); DWORD orig = 0;
    CC(GetConsoleMode(hi, &orig));
    DWORD bad[] = { 0x400, 0x800, 0x1000, 0x8000, 0x10000, 0x100000, 0x40000000, 0x80000000, 0xFFFFFC00, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0xBAD);
        BOOL r = SETMODE(hi, bad[i]);
        CCF(r == FALSE, "mode 0x%lx", bad[i]);
        CCF(GetLastError() == ERROR_INVALID_PARAMETER, "mode 0x%lx err=%lu", bad[i], GetLastError());
        SETMODE(hi, orig);
    }
    CC(SETMODE(hi, orig));
}
CT(gap11_con_mode_input_extended_flags) {
    CREQ(SETMODE);
    HANDLE hi = hin_(); DWORD orig = 0, m = 0;
    CC(GetConsoleMode(hi, &orig));
    /* quick edit / insert only take effect together with ENABLE_EXTENDED_FLAGS */
    CC(SETMODE(hi, ENABLE_EXTENDED_FLAGS | ENABLE_QUICK_EDIT_MODE | ENABLE_INSERT_MODE | ENABLE_PROCESSED_INPUT));
    CC(GetConsoleMode(hi, &m));
    CCF((m & ENABLE_QUICK_EDIT_MODE) != 0, "quick edit set: 0x%lx", m);
    CCF((m & ENABLE_INSERT_MODE) != 0, "insert set: 0x%lx", m);
    CC(SETMODE(hi, ENABLE_EXTENDED_FLAGS | ENABLE_PROCESSED_INPUT));
    CC(GetConsoleMode(hi, &m));
    CCF((m & ENABLE_QUICK_EDIT_MODE) == 0, "quick edit cleared: 0x%lx", m);
    CCF((m & ENABLE_INSERT_MODE) == 0, "insert cleared: 0x%lx", m);
    CC(SETMODE(hi, orig | ENABLE_EXTENDED_FLAGS));
    CC(SETMODE(hi, orig));
}
CT(gap11_con_mode_output_roundtrip) {
    CREQ(SETMODE);
    HANDLE b = mkbuf(); if (!b) { cnote("no buffer"); return; }
    DWORD m;
    for (DWORD v = 0; v <= 0x1F; v++) {
        SetLastError(0xBAD);
        CCF(SETMODE(b, v), "set 0x%lx err=%lu", v, GetLastError());
        m = 0xFFFFFFFF;
        CC(GetConsoleMode(b, &m));
        CCF(m == v, "set 0x%lx -> get 0x%lx", v, m);
    }
    CloseHandle(b);
}
CT(gap11_con_mode_output_undefined_bits) {
    CREQ(SETMODE);
    HANDLE b = mkbuf(); if (!b) return;
    DWORD m0 = 0, m1 = 0;
    CC(SETMODE(b, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT));
    CC(GetConsoleMode(b, &m0));
    DWORD bad[] = { 0x20, 0x40, 0x80, 0x100, 0x200, 0x8000, 0x10000, 0x80000000, 0xFFFFFFE0, 0xFFFFFFFF, 0x3F };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0xBAD);
        BOOL r = SETMODE(b, bad[i]);
        CCF(r == FALSE, "mode 0x%lx", bad[i]);
        CCF(GetLastError() == ERROR_INVALID_PARAMETER, "mode 0x%lx err=%lu", bad[i], GetLastError());
        CC(GetConsoleMode(b, &m1));
        CCF(m1 == m0, "mode unchanged after 0x%lx: 0x%lx", bad[i], m1);
    }
    CloseHandle(b);
}
CT(gap11_con_mode_output_is_per_buffer) {
    CREQ(SETMODE);
    HANDLE a = mkbuf(), b = mkbuf(); if (!a || !b) return;
    DWORD ma, mb;
    CC(SETMODE(a, ENABLE_PROCESSED_OUTPUT));
    CC(SETMODE(b, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING));
    CC(GetConsoleMode(a, &ma)); CC(GetConsoleMode(b, &mb));
    CEQ(ma, 1); CEQ(mb, 7);
    CloseHandle(a); CloseHandle(b);
}
CT(gap11_con_mode_output_vt_effect) {
    CREQ(SETMODE); CREQ(U(WriteConsoleW));
    HANDLE b = mkbuf(); if (!b) return;
    DWORD n;
    clear_buf(b);
    CC(SETMODE(b, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING));
    CC(U(WriteConsoleW)(b, L"\x1b[2;3HX", 7, &n, NULL));
    CEQ(cellch(b, 2, 1), L'X');          /* CSI row;col H moved the cursor */
    clear_buf(b);
    CC(SETMODE(b, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT));
    CC(U(WriteConsoleW)(b, L"\x1b[2;3HX", 7, &n, NULL));
    CNE(cellch(b, 2, 1), L'X');          /* without VT processing the sequence is not interpreted */
    CloseHandle(b);
}
CT(gap11_con_mode_output_processed_newline) {
    CREQ(SETMODE); CREQ(U(WriteConsoleW));
    HANDLE b = mkbuf(); if (!b) return;
    DWORD n;
    clear_buf(b);
    CC(SETMODE(b, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT));
    CC(U(WriteConsoleW)(b, L"ab\ncd", 5, &n, NULL));
    CEQ(cellch(b, 0, 1), L'c');          /* LF returns to column 0 under processed output */
    clear_buf(b);
    CC(SETMODE(b, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT | DISABLE_NEWLINE_AUTO_RETURN));
    CC(U(WriteConsoleW)(b, L"ab\ncd", 5, &n, NULL));
    cnote("DISABLE_NEWLINE_AUTO_RETURN: 'c' landed at column %d", 0 + (cellch(b, 2, 1) == L'c' ? 2 : (cellch(b, 0, 1) == L'c' ? 0 : -1)));
    CloseHandle(b);
}
CT(gap11_con_mode_invalid_handles) {
    CREQ(SETMODE);
    HANDLE closed = mkbuf(); if (closed) CloseHandle(closed);
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x1234, (HANDLE)(ULONG_PTR)0xFFFFFFFC, closed };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        if (!bad[i] && i == 4) continue;
        FAILS(SETMODE(bad[i], 0), ERROR_INVALID_HANDLE);
        FAILS(SETMODE(bad[i], ENABLE_PROCESSED_OUTPUT), ERROR_INVALID_HANDLE);
        FAILS(SETMODE(bad[i], ENABLE_PROCESSED_INPUT), ERROR_INVALID_HANDLE);
    }
}
CT(gap11_con_mode_non_console_handles) {
    CREQ(SETMODE);
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "mode.bin");
    HANDLE f = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE hs[] = { f, ev };
    for (int i = 0; i < 2; i++) {
        SetLastError(0xBAD);
        BOOL r = SETMODE(hs[i], ENABLE_PROCESSED_OUTPUT);
        DWORD e = GetLastError();
        CCF(r == FALSE, "handle %d", i);
        cnote("SetConsoleMode(non-console handle %d) err=%lu", i, e);
        CCF(e != 0 && e != 0xBAD, "handle %d error set", i);
    }
    if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
    CloseHandle(ev);
}
CT(gap11_con_mode_conout_handle) {
    CREQ(SETMODE);
    HANDLE co_ = CreateFileA("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    HANDLE ci_ = CreateFileA("CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CNE(co_, INVALID_HANDLE_VALUE); CNE(ci_, INVALID_HANDLE_VALUE);
    if (co_ == INVALID_HANDLE_VALUE || ci_ == INVALID_HANDLE_VALUE) return;
    DWORD oo, oi, m;
    CC(GetConsoleMode(co_, &oo)); CC(GetConsoleMode(ci_, &oi));
    CC(SETMODE(co_, ENABLE_PROCESSED_OUTPUT));
    CC(GetConsoleMode(co_, &m)); CEQ(m, 1);
    CC(SETMODE(co_, oo));
    CC(SETMODE(ci_, ENABLE_PROCESSED_INPUT | ENABLE_EXTENDED_FLAGS));
    CC(GetConsoleMode(ci_, &m)); CEQ(m & 0x3FF & ~0x180 & ~0x40 & ~0x20, ENABLE_PROCESSED_INPUT);
    CC(SETMODE(ci_, oi));
    CloseHandle(co_); CloseHandle(ci_);
}
CT(gap11_con_mode_duplicated_handle_shares_state) {
    CREQ(SETMODE);
    HANDLE b = mkbuf(); if (!b) return;
    HANDLE d = NULL;
    CC(DuplicateHandle(GetCurrentProcess(), b, GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS));
    DWORD m;
    CC(SETMODE(d, ENABLE_PROCESSED_OUTPUT | DISABLE_NEWLINE_AUTO_RETURN));
    CC(GetConsoleMode(b, &m)); CEQ(m, 9);
    CloseHandle(d);
    CC(GetConsoleMode(b, &m));       /* original handle is unaffected by closing the duplicate */
    CloseHandle(b);
}
CTD(gap11_con_mode_no_console) {
    CREQ(SETMODE);
    FAILS(SETMODE(GetStdHandle(STD_OUTPUT_HANDLE), ENABLE_PROCESSED_OUTPUT), ERROR_INVALID_HANDLE);
    FAILS(SETMODE(GetStdHandle(STD_INPUT_HANDLE), 0), ERROR_INVALID_HANDLE);
    FAILS(SETMODE(NULL, 0), ERROR_INVALID_HANDLE);
    FAILS(SETMODE(INVALID_HANDLE_VALUE, 0), ERROR_INVALID_HANDLE);
}

/* ---------------- SetConsoleCP / SetConsoleOutputCP ---------------- */
CT(gap11_con_cp_input_roundtrip) {
    CREQ(SETCP);
    UINT orig = GetConsoleCP();
    UINT cps[] = { 437, 850, 1252, 65001, 437, 20127, 28591, 1250, 866, 1251 };
    for (size_t i = 0; i < sizeof cps / sizeof cps[0]; i++) {
        SetLastError(0xBAD);
        BOOL r = SETCP(cps[i]);
        CCF(r, "cp %u err=%lu", cps[i], GetLastError());
        CCF(GetConsoleCP() == cps[i], "cp %u -> %u", cps[i], GetConsoleCP());
    }
    CC(SETCP(orig));
    CEQ(GetConsoleCP(), orig);
}
CT(gap11_con_cp_output_roundtrip) {
    CREQ(SETOCP);
    UINT orig = GetConsoleOutputCP();
    UINT cps[] = { 437, 850, 1252, 65001, 437, 20127, 28591, 1250, 866, 1251 };
    for (size_t i = 0; i < sizeof cps / sizeof cps[0]; i++) {
        SetLastError(0xBAD);
        BOOL r = SETOCP(cps[i]);
        CCF(r, "cp %u err=%lu", cps[i], GetLastError());
        CCF(GetConsoleOutputCP() == cps[i], "cp %u -> %u", cps[i], GetConsoleOutputCP());
    }
    CC(SETOCP(orig));
    CEQ(GetConsoleOutputCP(), orig);
}
CT(gap11_con_cp_input_output_independent) {
    CREQ(SETCP); CREQ(SETOCP);
    UINT oi = GetConsoleCP(), oo = GetConsoleOutputCP();
    CC(SETCP(437)); CC(SETOCP(1252));
    CEQ(GetConsoleCP(), 437); CEQ(GetConsoleOutputCP(), 1252);
    CC(SETCP(850));
    CEQ(GetConsoleCP(), 850); CEQ(GetConsoleOutputCP(), 1252);
    CC(SETOCP(65001));
    CEQ(GetConsoleCP(), 850); CEQ(GetConsoleOutputCP(), 65001);
    SETCP(oi); SETOCP(oo);
}
CT(gap11_con_cp_invalid_values) {
    CREQ(SETCP); CREQ(SETOCP);
    UINT oi = GetConsoleCP(), oo = GetConsoleOutputCP();
    UINT bad[] = { 0, 1, 2, 99, 12345, 99999, 0x10000, 0x7FFFFFFF, 0xFFFFFFFE, 0xFFFFFFFF, 65535, 54321 };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0xBAD);
        BOOL r = SETCP(bad[i]);
        CCF(r == FALSE, "input cp %u", bad[i]);
        CCF(GetLastError() == ERROR_INVALID_PARAMETER, "input cp %u err=%lu", bad[i], GetLastError());
        SetLastError(0xBAD);
        r = SETOCP(bad[i]);
        CCF(r == FALSE, "output cp %u", bad[i]);
        CCF(GetLastError() == ERROR_INVALID_PARAMETER, "output cp %u err=%lu", bad[i], GetLastError());
    }
    CEQ(GetConsoleCP(), oi); CEQ(GetConsoleOutputCP(), oo);   /* nothing changed */
}
CT(gap11_con_cp_pseudo_and_utf16_values) {
    CREQ(SETCP); CREQ(SETOCP);
    UINT oi = GetConsoleCP(), oo = GetConsoleOutputCP();
    /* CP_ACP/CP_OEMCP/CP_MACCP/CP_THREAD_ACP/CP_SYMBOL/CP_UTF7 and the UTF-16 pages: observed only */
    UINT odd[] = { CP_ACP, CP_OEMCP, CP_MACCP, CP_THREAD_ACP, CP_SYMBOL, CP_UTF7, 1200, 1201, 12000, 12001, 10000 };
    for (size_t i = 0; i < sizeof odd / sizeof odd[0]; i++) {
        SetLastError(0xBAD);
        BOOL r = SETCP(odd[i]);
        cnote("SetConsoleCP(%u) -> %d err=%lu now=%u", odd[i], r, GetLastError(), GetConsoleCP());
        SETCP(oi);
        SetLastError(0xBAD);
        r = SETOCP(odd[i]);
        cnote("SetConsoleOutputCP(%u) -> %d err=%lu now=%u", odd[i], r, GetLastError(), GetConsoleOutputCP());
        SETOCP(oo);
    }
    CEQ(GetConsoleCP(), oi); CEQ(GetConsoleOutputCP(), oo);
}
CT(gap11_con_cp_output_affects_ansi_write) {
    CREQ(SETOCP); CREQ(U(WriteConsoleA));
    HANDLE b = mkbuf(); if (!b) return;
    UINT oo = GetConsoleOutputCP(); DWORD n;
    SetConsoleMode(b, ENABLE_PROCESSED_OUTPUT);
    clear_buf(b);
    CC(SETOCP(1252));
    CC(U(WriteConsoleA)(b, "\xE9", 1, &n, NULL));
    CEQ(cellch(b, 0, 0), 0x00E9);
    clear_buf(b);
    CC(SETOCP(437));
    CC(U(WriteConsoleA)(b, "\x82", 1, &n, NULL));
    CEQ(cellch(b, 0, 0), 0x00E9);        /* 0x82 in CP437 is e-acute */
    clear_buf(b);
    CC(SETOCP(1251));
    CC(U(WriteConsoleA)(b, "\xE0", 1, &n, NULL));
    CEQ(cellch(b, 0, 0), 0x0430);        /* cyrillic a */
    clear_buf(b);
    CC(SETOCP(65001));
    CC(U(WriteConsoleA)(b, "\xC3\xA9", 2, &n, NULL));
    CEQ(cellch(b, 0, 0), 0x00E9);
    SETOCP(oo);
    CloseHandle(b);
}
CT(gap11_con_cp_input_affects_ansi_read_conversion) {
    CREQ(SETCP);
    UINT oi = GetConsoleCP();
    CC(SETCP(932));
    UINT now = GetConsoleCP();
    cnote("after SetConsoleCP(932): %u", now);
    SETCP(1252);
    CEQ(GetConsoleCP(), 1252);
    SETCP(oi);
}
CT(gap11_con_cp_repeat_idempotent) {
    CREQ(SETCP); CREQ(SETOCP);
    UINT oi = GetConsoleCP(), oo = GetConsoleOutputCP();
    for (int i = 0; i < 20; i++) { CC(SETCP(850)); CC(SETOCP(850)); }
    CEQ(GetConsoleCP(), 850); CEQ(GetConsoleOutputCP(), 850);
    SETCP(oi); SETOCP(oo);
}
CTD(gap11_con_cp_no_console) {
    CREQ(SETCP); CREQ(SETOCP);
    FAILS(SETCP(437), ERROR_INVALID_HANDLE);
    FAILS(SETOCP(437), ERROR_INVALID_HANDLE);
    FAILS(SETCP(0), ERROR_INVALID_HANDLE);
    FAILS(SETOCP(0xFFFFFFFF), ERROR_INVALID_HANDLE);
}

/* ---------------- SetConsoleCursorInfo ---------------- */
CT(gap11_con_cursorinfo_sizes) {
    CREQ(SETCI);
    HANDLE b = mkbuf(); if (!b) return;
    DWORD sizes[] = { 1, 2, 12, 25, 50, 99, 100, 1 };
    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        CONSOLE_CURSOR_INFO ci = { sizes[i], TRUE }, g = { 0, 0 };
        SetLastError(0xBAD);
        CCF(SETCI(b, &ci), "size %lu err=%lu", sizes[i], GetLastError());
        CC(GetConsoleCursorInfo(b, &g));
        CCF(g.dwSize == sizes[i] && g.bVisible, "size %lu -> %lu vis %d", sizes[i], g.dwSize, g.bVisible);
    }
    CloseHandle(b);
}
CT(gap11_con_cursorinfo_invalid_sizes) {
    CREQ(SETCI);
    HANDLE b = mkbuf(); if (!b) return;
    CONSOLE_CURSOR_INFO good = { 33, TRUE }, g;
    CC(SETCI(b, &good));
    DWORD bad[] = { 0, 101, 102, 200, 255, 256, 1000, 0x8000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        CONSOLE_CURSOR_INFO ci = { bad[i], TRUE };
        SetLastError(0xBAD);
        BOOL r = SETCI(b, &ci);
        CCF(r == FALSE, "size %lu", bad[i]);
        CCF(GetLastError() == ERROR_INVALID_PARAMETER, "size %lu err=%lu", bad[i], GetLastError());
        CC(GetConsoleCursorInfo(b, &g));
        CCF(g.dwSize == 33 && g.bVisible, "unchanged after %lu: %lu", bad[i], g.dwSize);
    }
    CloseHandle(b);
}
CT(gap11_con_cursorinfo_visibility) {
    CREQ(SETCI);
    HANDLE b = mkbuf(); if (!b) return;
    CONSOLE_CURSOR_INFO ci = { 40, FALSE }, g;
    CC(SETCI(b, &ci)); CC(GetConsoleCursorInfo(b, &g));
    CEQ(g.dwSize, 40); CEQ(g.bVisible, FALSE);
    ci.bVisible = TRUE; CC(SETCI(b, &ci)); CC(GetConsoleCursorInfo(b, &g));
    CEQ(g.bVisible, TRUE);
    ci.bVisible = 2; CC(SETCI(b, &ci)); CC(GetConsoleCursorInfo(b, &g));
    cnote("bVisible=2 reads back as %d", g.bVisible);
    CCF(g.bVisible != 0, "non-zero BOOL is visible");
    ci.bVisible = 0x100; CC(SETCI(b, &ci)); CC(GetConsoleCursorInfo(b, &g));
    cnote("bVisible=0x100 (low byte zero) reads back as %d", g.bVisible);
    ci.bVisible = (BOOL)0xFFFFFFFF; CC(SETCI(b, &ci)); CC(GetConsoleCursorInfo(b, &g));
    CCF(g.bVisible != 0, "all-ones BOOL is visible");
    CloseHandle(b);
}
CT(gap11_con_cursorinfo_is_per_buffer) {
    CREQ(SETCI);
    HANDLE a = mkbuf(), b = mkbuf(); if (!a || !b) return;
    CONSOLE_CURSOR_INFO ia = { 10, TRUE }, ib = { 90, FALSE }, g;
    CC(SETCI(a, &ia)); CC(SETCI(b, &ib));
    CC(GetConsoleCursorInfo(a, &g)); CEQ(g.dwSize, 10); CEQ(g.bVisible, TRUE);
    CC(GetConsoleCursorInfo(b, &g)); CEQ(g.dwSize, 90); CEQ(g.bVisible, FALSE);
    CloseHandle(a); CloseHandle(b);
}
CT(gap11_con_cursorinfo_invalid_handles) {
    CREQ(SETCI);
    CONSOLE_CURSOR_INFO ci = { 25, TRUE };
    HANDLE closed = mkbuf(); if (closed) CloseHandle(closed);
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x1234, closed, hin_() };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        if (!bad[i] && i == 3) continue;
        SetLastError(0xBAD);
        BOOL r = SETCI(bad[i], &ci);
        CCF(r == FALSE, "handle #%zu", i);
        CCF(GetLastError() == ERROR_INVALID_HANDLE, "handle #%zu err=%lu", i, GetLastError());
    }
}
CT(gap11_con_cursorinfo_handle_checked_before_size) {
    CREQ(SETCI);
    CONSOLE_CURSOR_INFO ci = { 0, TRUE };
    FAILS(SETCI(INVALID_HANDLE_VALUE, &ci), ERROR_INVALID_HANDLE);
}
CT(gap11_con_cursorinfo_active_buffer) {
    CREQ(SETCI);
    HANDLE ho = hout_();
    CONSOLE_CURSOR_INFO orig, ci = { 77, TRUE }, g;
    CC(GetConsoleCursorInfo(ho, &orig));
    CC(SETCI(ho, &ci));
    CC(GetConsoleCursorInfo(ho, &g)); CEQ(g.dwSize, 77);
    CC(SETCI(ho, &orig));
}
CTD(gap11_con_cursorinfo_no_console) {
    CREQ(SETCI);
    CONSOLE_CURSOR_INFO ci = { 25, TRUE };
    FAILS(SETCI(GetStdHandle(STD_OUTPUT_HANDLE), &ci), ERROR_INVALID_HANDLE);
    FAILS(SETCI(NULL, &ci), ERROR_INVALID_HANDLE);
    FAILS(SETCI(INVALID_HANDLE_VALUE, &ci), ERROR_INVALID_HANDLE);
}

/* ---------------- SetConsoleCursorPosition ---------------- */
CT(gap11_con_cursorpos_valid) {
    CREQ(SETCP_POS);
    HANDLE b = mkbuf(); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFO i = binfo(b);
    int W = i.dwSize.X, H = i.dwSize.Y;
    int pts[][2] = { {0, 0}, {1, 0}, {0, 1}, {5, 3}, {W - 1, 0}, {0, 3}, {W - 1, 3}, {10, 2}, {0, 0} };
    for (size_t k = 0; k < sizeof pts / sizeof pts[0]; k++) {
        SetLastError(0xBAD);
        CCF(SETCP_POS(b, co(pts[k][0], pts[k][1])), "(%d,%d) err=%lu", pts[k][0], pts[k][1], GetLastError());
        CONSOLE_SCREEN_BUFFER_INFO g = binfo(b);
        CCF(g.dwCursorPosition.X == pts[k][0] && g.dwCursorPosition.Y == pts[k][1], "(%d,%d) -> (%d,%d)", pts[k][0], pts[k][1], g.dwCursorPosition.X, g.dwCursorPosition.Y);
    }
    (void)H;
    CloseHandle(b);
}
CT(gap11_con_cursorpos_last_cell) {
    CREQ(SETCP_POS);
    HANDLE b = mkbuf(); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFO i = binfo(b);
    int W = i.dwSize.X, H = i.dwSize.Y;
    CC(SETCP_POS(b, co(W - 1, H - 1)));
    CONSOLE_SCREEN_BUFFER_INFO g = binfo(b);
    CEQ(g.dwCursorPosition.X, W - 1); CEQ(g.dwCursorPosition.Y, H - 1);
    /* the last cell is inside the buffer, but the viewport must now show it */
    CC(g.srWindow.Top <= H - 1 && g.srWindow.Bottom >= H - 1);
    CloseHandle(b);
}
CT(gap11_con_cursorpos_out_of_range) {
    CREQ(SETCP_POS);
    HANDLE b = mkbuf(); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFO i = binfo(b);
    int W = i.dwSize.X, H = i.dwSize.Y;
    CC(SETCP_POS(b, co(3, 2)));
    int bad[][2] = { {-1, 0}, {0, -1}, {-1, -1}, {W, 0}, {0, H}, {W, H}, {W + 1, 0}, {0, H + 1}, {32767, 0}, {0, 32767},
                     {32767, 32767}, {-32768, 0}, {0, -32768}, {-32768, -32768}, {W, H - 1}, {W - 1, H} };
    for (size_t k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        SetLastError(0xBAD);
        BOOL r = SETCP_POS(b, co(bad[k][0], bad[k][1]));
        CCF(r == FALSE, "(%d,%d)", bad[k][0], bad[k][1]);
        CCF(GetLastError() == ERROR_INVALID_PARAMETER, "(%d,%d) err=%lu", bad[k][0], bad[k][1], GetLastError());
        CONSOLE_SCREEN_BUFFER_INFO g = binfo(b);
        CCF(g.dwCursorPosition.X == 3 && g.dwCursorPosition.Y == 2, "unchanged after (%d,%d)", bad[k][0], bad[k][1]);
    }
    CloseHandle(b);
}
CT(gap11_con_cursorpos_invalid_handles) {
    CREQ(SETCP_POS);
    HANDLE closed = mkbuf(); if (closed) CloseHandle(closed);
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x1234, closed, hin_() };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        if (!bad[i] && i == 3) continue;
        SetLastError(0xBAD);
        BOOL r = SETCP_POS(bad[i], co(0, 0));
        CCF(r == FALSE, "handle #%zu", i);
        CCF(GetLastError() == ERROR_INVALID_HANDLE, "handle #%zu err=%lu", i, GetLastError());
    }
}
CT(gap11_con_cursorpos_scrolls_viewport) {
    CREQ(SETCP_POS); CREQ(U(SetConsoleScreenBufferSize));
    HANDLE b = mkbuf(); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFO i = binfo(b);
    int wh = i.srWindow.Bottom - i.srWindow.Top + 1;
    COORD sz = co(i.dwSize.X, 200);
    if (!U(SetConsoleScreenBufferSize)(b, sz)) { cnote("could not grow buffer"); CloseHandle(b); return; }
    i = binfo(b);
    CC(SETCP_POS(b, co(0, 150)));
    CONSOLE_SCREEN_BUFFER_INFO g = binfo(b);
    CCF(g.srWindow.Top <= 150 && g.srWindow.Bottom >= 150, "window %d..%d must contain row 150", g.srWindow.Top, g.srWindow.Bottom);
    CEQ(g.srWindow.Bottom - g.srWindow.Top + 1, wh);        /* window size is preserved */
    CC(SETCP_POS(b, co(0, 0)));
    g = binfo(b);
    CCF(g.srWindow.Top == 0, "window back at top: %d", g.srWindow.Top);
    CloseHandle(b);
}
CT(gap11_con_cursorpos_independent_of_write_buffers) {
    CREQ(SETCP_POS);
    HANDLE a = mkbuf(), b = mkbuf(); if (!a || !b) return;
    CC(SETCP_POS(a, co(7, 1))); CC(SETCP_POS(b, co(2, 3)));
    CONSOLE_SCREEN_BUFFER_INFO ga = binfo(a), gb = binfo(b);
    CEQ(ga.dwCursorPosition.X, 7); CEQ(ga.dwCursorPosition.Y, 1);
    CEQ(gb.dwCursorPosition.X, 2); CEQ(gb.dwCursorPosition.Y, 3);
    CloseHandle(a); CloseHandle(b);
}
CT(gap11_con_cursorpos_write_continues_from_position) {
    CREQ(SETCP_POS); CREQ(U(WriteConsoleW));
    HANDLE b = mkbuf(); if (!b) return;
    DWORD n;
    SetConsoleMode(b, ENABLE_PROCESSED_OUTPUT);
    clear_buf(b);
    CC(SETCP_POS(b, co(4, 2)));
    CC(U(WriteConsoleW)(b, L"xyz", 3, &n, NULL));
    CC(rowis(b, 2, 4, L"xyz"));
    CONSOLE_SCREEN_BUFFER_INFO g = binfo(b);
    CEQ(g.dwCursorPosition.X, 7); CEQ(g.dwCursorPosition.Y, 2);
    CloseHandle(b);
}
CTD(gap11_con_cursorpos_no_console) {
    CREQ(SETCP_POS);
    FAILS(SETCP_POS(GetStdHandle(STD_OUTPUT_HANDLE), co(0, 0)), ERROR_INVALID_HANDLE);
    FAILS(SETCP_POS(NULL, co(0, 0)), ERROR_INVALID_HANDLE);
    FAILS(SETCP_POS(INVALID_HANDLE_VALUE, co(1, 1)), ERROR_INVALID_HANDLE);
}
