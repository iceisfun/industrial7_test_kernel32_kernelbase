/* console_apis: WriteConsole A/W, processed output, wrapping, VT sequences, wide characters */
#include "ch.h"

#define FAILS(call, err)  do { SetLastError(0xBAD); BOOL r_ = (call); DWORD le_ = GetLastError(); CEQ(r_, 0); CERRV(le_, err); } while (0)

static BOOL wrote_n(HANDLE h, const WCHAR *s, DWORD want_n) {
    DWORD n = 0xFFFF;
    BOOL r = WriteConsoleW(h, s, lstrlenW(s), &n, NULL);
    return r && n == want_n;
}

CT(con_write_w_basic) {
    CREQ(WriteConsoleW);
    HANDLE b = mkbuf_std(); if (!b) return;
    DWORD n = 0xFFFF;
    SetLastError(0xBAD);
    CC(WriteConsoleW(b, L"Hello", 5, &n, NULL));
    CEQ(n, 5);
    CC(rowis(b, 0, 0, L"Hello"));
    CC(coeq(curpos(b), 5, 0));
    CC(WriteConsoleW(b, L", World", 7, &n, NULL));
    CEQ(n, 7);
    CC(rowis(b, 0, 0, L"Hello, World"));
    CC(coeq(curpos(b), 12, 0));
    CloseHandle(b);
}

CT(con_write_a_basic) {
    CREQ(WriteConsoleA);
    HANDLE b = mkbuf_std(); if (!b) return;
    DWORD n = 0xFFFF;
    CC(WriteConsoleA(b, "ansi text", 9, &n, NULL));
    CEQ(n, 9);
    CC(rowis(b, 0, 0, L"ansi text"));
    CC(coeq(curpos(b), 9, 0));
    CloseHandle(b);
}

CT(con_write_partial_length) {
    CREQ(WriteConsoleW); CREQ(WriteConsoleA);
    HANDLE b = mkbuf_std(); if (!b) return;
    DWORD n = 0xFFFF;
    CC(WriteConsoleW(b, L"abcdefgh", 3, &n, NULL)); CEQ(n, 3);
    CC(rowis(b, 0, 0, L"abc")); CEQ(cellch(b, 3, 0), ' ');
    CC(WriteConsoleA(b, "XYZ123", 2, &n, NULL)); CEQ(n, 2);
    CC(rowis(b, 0, 0, L"abcXY"));
    CloseHandle(b);
}

CT(con_write_embedded_nul_counts) {
    CREQ(WriteConsoleW);
    HANDLE b = mkbuf_std(); if (!b) return;
    DWORD n = 0;
    CC(WriteConsoleW(b, L"a\0b", 3, &n, NULL));
    CEQ(n, 3);
    CEQ(cellch(b, 0, 0), 'a');
    cnote("embedded NUL cursor x=%d cell1=0x%x cell2=0x%x", curpos(b).X, cellch(b, 1, 0), cellch(b, 2, 0));
    CloseHandle(b);
}

CT(con_write_zero_length) {
    CREQ(WriteConsoleW); CREQ(WriteConsoleA);
    HANDLE b = mkbuf_std(); if (!b) return;
    DWORD n = 0xFFFF;
    SetLastError(0xBAD);
    CC(WriteConsoleW(b, L"x", 0, &n, NULL)); CEQ(n, 0);
    CC(coeq(curpos(b), 0, 0));
    n = 0xFFFF;
    CC(WriteConsoleA(b, "x", 0, &n, NULL)); CEQ(n, 0);
    CC(coeq(curpos(b), 0, 0));
    CEQ(cellch(b, 0, 0), ' ');
    CloseHandle(b);
}

CT(con_write_invalid_handles) {
    CREQ(WriteConsoleW); CREQ(WriteConsoleA);
    DWORD n = 0xFFFF;
    HANDLE ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x1234, ev, hin_() };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        FAILS(WriteConsoleW(bad[i], L"x", 1, &n, NULL), ERROR_INVALID_HANDLE);
        FAILS(WriteConsoleA(bad[i], "x", 1, &n, NULL), ERROR_INVALID_HANDLE);
    }
    CloseHandle(ev);
}

CT(con_write_file_handle_rejected) {
    CREQ(WriteConsoleW);
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "wc.txt");
    HANDLE f = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD n = 0;
    FAILS(WriteConsoleW(f, L"x", 1, &n, NULL), ERROR_INVALID_HANDLE);
    CloseHandle(f);
}

CT(con_write_closed_handle) {
    CREQ(WriteConsoleW);
    HANDLE b = mkbuf_std(); if (!b) return;
    CloseHandle(b);
    DWORD n;
    FAILS(WriteConsoleW(b, L"x", 1, &n, NULL), ERROR_INVALID_HANDLE);
}

CT(con_write_readonly_handle) {
    CREQ(WriteConsoleW); CREQ(ReadConsoleOutputCharacterW);
    HANDLE b = mkbuf_raw(); CNE(b, NULL);
    HANDLE ro = CreateFileW(L"CONOUT$", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CNE(ro, INVALID_HANDLE_VALUE);
    DWORD n = 0; SetLastError(0xBAD);
    BOOL r = WriteConsoleW(ro, L"x", 1, &n, NULL);
    DWORD e = GetLastError();
    cnote("WriteConsoleW on GENERIC_READ-only CONOUT$ -> %d err=%lu n=%lu", r, e, n);
    CEQ(r, 0);
    WCHAR c; SetLastError(0xBAD);
    r = ReadConsoleOutputCharacterW(ro, &c, 1, co(0, 0), &n);
    CEQ(r, 1);
    CloseHandle(ro); CloseHandle(b);
}

CT(con_write_reserved_param) {
    CREQ(WriteConsoleW);
    HANDLE b = mkbuf_std(); if (!b) return;
    DWORD n = 0; int dummy = 0;
    SetLastError(0xBAD);
    BOOL r = WriteConsoleW(b, L"r", 1, &n, &dummy);
    cnote("WriteConsoleW with non-NULL reserved -> %d err=%lu", r, GetLastError());
    CloseHandle(b);
}

CT(con_write_processed_lf) {
    CREQ(WriteConsoleW);
    HANDLE b = mkbuf_std(); if (!b) return;
    CC(wrote_n(b, L"ab\ncd", 5));
    CC(rowis(b, 0, 0, L"ab"));
    CC(rowis(b, 1, 0, L"cd"));
    CC(coeq(curpos(b), 2, 1));
    CloseHandle(b);
}

CT(con_write_processed_crlf) {
    CREQ(WriteConsoleW);
    HANDLE b = mkbuf_std(); if (!b) return;
    CC(wrote_n(b, L"one\r\ntwo\r\nthree", 15));
    CC(rowis(b, 0, 0, L"one")); CC(rowis(b, 1, 0, L"two")); CC(rowis(b, 2, 0, L"three"));
    CC(coeq(curpos(b), 5, 2));
    CloseHandle(b);
}

CT(con_write_processed_cr) {
    CREQ(WriteConsoleW);
    HANDLE b = mkbuf_std(); if (!b) return;
    CC(wrote_n(b, L"abcdef\rXY", 9));
    CC(rowis(b, 0, 0, L"XYcdef"));
    CC(coeq(curpos(b), 2, 0));
    CloseHandle(b);
}

CT(con_write_processed_backspace) {
    CREQ(WriteConsoleW);
    HANDLE b = mkbuf_std(); if (!b) return;
    CC(wrote_n(b, L"abc\bX", 5));
    CC(rowis(b, 0, 0, L"abX"));
    CC(coeq(curpos(b), 3, 0));
    CC(wrote_n(b, L"\b\b\b\b\b", 5));   /* clamps at column 0 */
    CC(coeq(curpos(b), 0, 0));
    CloseHandle(b);
}

CT(con_write_processed_tab_stops) {
    CREQ(WriteConsoleW);
    HANDLE b = mkbuf_std(); if (!b) return;
    static const struct { const WCHAR *s; int x; } t[] = {
        { L"\t", 8 }, { L"a\t", 8 }, { L"1234567\t", 8 }, { L"12345678\t", 16 }, { L"\t\t", 16 }, { L"\t\t\t", 24 }, { L"abc\tdef\t", 16 },
    };
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++) {
        clear_buf(b);
        CC(wrote_n(b, t[i].s, lstrlenW(t[i].s)));
        CCF(coeq(curpos(b), t[i].x, 0), "case %zu got x=%d want %d", i, curpos(b).X, t[i].x);
    }
    CloseHandle(b);
}

CT(con_write_processed_tab_char_placement) {
    CREQ(WriteConsoleW);
    HANDLE b = mkbuf_std(); if (!b) return;
    CC(wrote_n(b, L"a\tb", 3));
    CEQ(cellch(b, 0, 0), 'a');
    CEQ(cellch(b, 8, 0), 'b');
    CC(coeq(curpos(b), 9, 0));
    CloseHandle(b);
}

CT(con_write_processed_tab_at_eol) {
    CREQ(WriteConsoleW);
    HANDLE b = mkbuf_std(); if (!b) return;
    CC(SetConsoleCursorPosition(b, co(78, 3)));
    CC(wrote_n(b, L"\t", 1));
    COORD c = curpos(b);
    cnote("tab at col 78 -> cursor %d,%d", c.X, c.Y);
    CC(c.X < 80 && c.Y >= 3 && c.Y <= 4);
    CloseHandle(b);
}

CT(con_write_unprocessed_controls_advance) {
    CREQ(WriteConsoleW);
    HANDLE b = mkbuf_std(); if (!b) return;
    CC(SetConsoleMode(b, 0));
    CC(wrote_n(b, L"a\nb\rc\bd", 7));
    COORD c = curpos(b);
    CCF(coeq(c, 7, 0), "cursor %d,%d", c.X, c.Y);
    for (int i = 0; i < 7; i++) cnote("raw cell %d = 0x%x", i, cellch(b, i, 0));
    CloseHandle(b);
}

CT(con_write_unprocessed_lf_no_newline) {
    CREQ(WriteConsoleW);
    HANDLE b = mkbuf_std(); if (!b) return;
    CC(SetConsoleMode(b, ENABLE_WRAP_AT_EOL_OUTPUT));
    CC(wrote_n(b, L"x\ny", 3));
    CC(coeq(curpos(b), 3, 0));
    CloseHandle(b);
}

CT(con_write_bell_not_tested_but_other_ctrl) {
    CREQ(WriteConsoleW);
    HANDLE b = mkbuf_std(); if (!b) return;
    /* no BEL here: it would beep. Other C0 codes in processed mode. */
    CC(wrote_n(b, L"a\x01\x02" L"b", 4));
    COORD c = curpos(b);
    cnote("C0 chars in processed mode: cursor x=%d", c.X);
    CC(c.Y == 0 && c.X >= 2);
    CloseHandle(b);
}

CT(con_write_wrap_on) {
    CREQ(WriteConsoleW);
    HANDLE b = mkbuf_std(); if (!b) return;
    CC(SetConsoleMode(b, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT));
    WCHAR s[96];
    for (int i = 0; i < 85; i++) s[i] = L'a' + (i % 26);
    s[85] = 0;
    CC(wrote_n(b, s, 85));
    CC(rowis(b, 0, 0, L"abcdefghij"));
    CEQ(cellch(b, 79, 0), s[79]);
    CEQ(cellch(b, 0, 1), s[80]);
    CEQ(cellch(b, 4, 1), s[84]);
    CC(coeq(curpos(b), 5, 1));
    CloseHandle(b);
}

CT(con_write_wrap_off) {
    CREQ(WriteConsoleW);
    HANDLE b = mkbuf_std(); if (!b) return;
    CC(SetConsoleMode(b, ENABLE_PROCESSED_OUTPUT));
    WCHAR s[96];
    for (int i = 0; i < 85; i++) s[i] = L'a' + (i % 26);
    s[85] = 0;
    CC(wrote_n(b, s, 85));
    CEQ(cellch(b, 78, 0), s[78]);
    /* P1: Win10 conhost drops the characters past the last column when ENABLE_WRAP_AT_EOL_OUTPUT is off;
       the last cell keeps the 80th character (s[79]), it is not overwritten by the final one. UNVERIFIED-until-rerun */
    CEQ(cellch(b, 79, 0), s[79]);
    CEQ(cellch(b, 0, 1), ' ');
    { COORD c_ = curpos(b); cnote("no-wrap cursor after 85 chars: %d,%d", c_.X, c_.Y);
      cnote("no-wrap row0 cols 0..4: %c%c%c%c%c", cellch(b, 0, 0), cellch(b, 1, 0), cellch(b, 2, 0), cellch(b, 3, 0), cellch(b, 4, 0)); }
    CC(coeq(curpos(b), 79, 0));   /* P1b UNRESOLVED: baseline cursor is (5,0) = 85 mod 80, not (79,0); documentation says the last cell is overwritten, observed s[79] kept */
    CloseHandle(b);
}

CT(con_write_wrap_exact_width) {
    CREQ(WriteConsoleW);
    HANDLE b = mkbuf_std(); if (!b) return;
    WCHAR s[96];
    for (int i = 0; i < 80; i++) s[i] = L'0' + (i % 10);
    s[80] = 0;
    CC(wrote_n(b, s, 80));
    COORD c = curpos(b);
    cnote("after exactly 80 chars cursor=%d,%d", c.X, c.Y);
    CC((c.X == 0 && c.Y == 1) || (c.X == 79 && c.Y == 0));
    CC(wrote_n(b, L"Z", 1));
    CEQ(cellch(b, 0, 1), 'Z');
    CloseHandle(b);
}

CT(con_write_scrolls_at_bottom) {
    CREQ(WriteConsoleW);
    HANDLE b = mkbuf_std(); if (!b) return;
    CC(SetConsoleCursorPosition(b, co(0, 24)));
    CC(wrote_n(b, L"bottom\n", 7));
    CC(rowis(b, 23, 0, L"bottom"));
    CC(coeq(curpos(b), 0, 24));
    CEQ(cellch(b, 0, 24), ' ');
    CloseHandle(b);
}

CT(con_write_scrolls_whole_buffer) {
    CREQ(WriteConsoleW);
    HANDLE b = mkbuf_std(); if (!b) return;
    for (int i = 0; i < 40; i++) {
        WCHAR l[32]; wsprintfW(l, L"line%02d\r\n", i);
        CC(wrote_n(b, l, lstrlenW(l)));
    }
    /* 25 rows; last row is the empty cursor row, the 24 rows above hold lines 16..39 */
    CC(rowis(b, 0, 0, L"line16"));
    CC(rowis(b, 23, 0, L"line39"));
    CC(coeq(curpos(b), 0, 24));
    CloseHandle(b);
}

CT(con_write_long_string) {
    CREQ(WriteConsoleW);
    HANDLE b = mkbuf_std(); if (!b) return;
    static WCHAR s[5001];
    for (int i = 0; i < 5000; i++) s[i] = L'A' + (i % 26);
    s[5000] = 0;
    DWORD n = 0;
    CC(WriteConsoleW(b, s, 5000, &n, NULL));
    CEQ(n, 5000);
    /* 5000 = 62 full rows + 40 */
    COORD c = curpos(b);
    CCF(c.Y == 24 && c.X == 40, "cursor %d,%d", c.X, c.Y);
    CloseHandle(b);
}

CT(con_write_large_a_buffer) {
    CREQ(WriteConsoleA);
    HANDLE b = mkbuf_std(); if (!b) return;
    static char s[30001];
    memset(s, 'x', 30000);
    DWORD n = 0;
    CC(WriteConsoleA(b, s, 30000, &n, NULL));
    CCF(n == 30000, "n=%lu", n);
    CloseHandle(b);
}

CT(con_write_uses_current_attribute) {
    CREQ(WriteConsoleW);
    HANDLE b = mkbuf_std(); if (!b) return;
    CC(SetConsoleTextAttribute(b, FOREGROUND_RED | FOREGROUND_INTENSITY | BACKGROUND_BLUE));
    CC(wrote_n(b, L"ab", 2));
    CEQ(cellattr(b, 0, 0), FOREGROUND_RED | FOREGROUND_INTENSITY | BACKGROUND_BLUE);
    CEQ(cellattr(b, 1, 0), FOREGROUND_RED | FOREGROUND_INTENSITY | BACKGROUND_BLUE);
    CEQ(cellattr(b, 2, 0), 7);
    CC(SetConsoleTextAttribute(b, 0x2A));
    CC(wrote_n(b, L"c", 1));
    CEQ(cellattr(b, 2, 0), 0x2A);
    CloseHandle(b);
}

CT(con_write_overwrites_existing) {
    CREQ(WriteConsoleW);
    HANDLE b = mkbuf_std(); if (!b) return;
    CC(wrote_n(b, L"XXXXXXXX", 8));
    CC(SetConsoleCursorPosition(b, co(2, 0)));
    CC(wrote_n(b, L"ab", 2));
    CC(rowis(b, 0, 0, L"XXabXXXX"));
    CloseHandle(b);
}

CT(con_write_unicode_roundtrip) {
    CREQ(WriteConsoleW);
    HANDLE b = mkbuf_std(); if (!b) return;
    static const WCHAR s[] = { 'A', 0xE9, 0x416, 0x3A9, 0x20AC, 'z', 0 };
    CC(wrote_n(b, s, 6));
    WCHAR r[16]; rowtext(b, 0, 0, 6, r);
    CWSTR(r, s);
    CC(coeq(curpos(b), 6, 0));
    CloseHandle(b);
}

CT(con_write_cjk_two_cells) {
    CREQ(WriteConsoleW);
    HANDLE b = mkbuf_std(); if (!b) return;
    CC(wrote_n(b, L"\x4E2D", 1));
    COORD c = curpos(b);
    CCF(c.X == 2 && c.Y == 0, "cursor %d,%d", c.X, c.Y);
    WORD a0 = cellattr(b, 0, 0), a1 = cellattr(b, 1, 0), a2 = cellattr(b, 2, 0);
    CCF((a0 & COMMON_LVB_LEADING_BYTE) != 0, "attr0=0x%x", a0);
    CCF((a1 & COMMON_LVB_TRAILING_BYTE) != 0, "attr1=0x%x", a1);
    CEQ(a2 & (COMMON_LVB_LEADING_BYTE | COMMON_LVB_TRAILING_BYTE), 0);
    CEQ(a0 & COMMON_LVB_TRAILING_BYTE, 0);
    CEQ(a1 & COMMON_LVB_LEADING_BYTE, 0);
    CEQ(a0 & 0xFF, 7);
    cnote("CJK cell chars: 0x%x 0x%x", cellch(b, 0, 0), cellch(b, 1, 0));
    CloseHandle(b);
}

CT(con_write_cjk_sequence) {
    CREQ(WriteConsoleW);
    HANDLE b = mkbuf_std(); if (!b) return;
    CC(wrote_n(b, L"a\x4E2D" L"b\x6587", 4));
    COORD c = curpos(b);
    CCF(c.X == 6 && c.Y == 0, "cursor %d,%d", c.X, c.Y);
    CEQ(cellch(b, 0, 0), 'a');
    CEQ(cellch(b, 3, 0), 'b');
    CEQ(cellattr(b, 1, 0) & COMMON_LVB_LEADING_BYTE, COMMON_LVB_LEADING_BYTE);
    CEQ(cellattr(b, 2, 0) & COMMON_LVB_TRAILING_BYTE, COMMON_LVB_TRAILING_BYTE);
    CEQ(cellattr(b, 4, 0) & COMMON_LVB_LEADING_BYTE, COMMON_LVB_LEADING_BYTE);
    CEQ(cellattr(b, 5, 0) & COMMON_LVB_TRAILING_BYTE, COMMON_LVB_TRAILING_BYTE);
    CloseHandle(b);
}

CT(con_write_cjk_at_last_column) {
    CREQ(WriteConsoleW);
    HANDLE b = mkbuf_std(); if (!b) return;
    CC(SetConsoleCursorPosition(b, co(79, 0)));
    CC(wrote_n(b, L"\x4E2D", 1));
    COORD c = curpos(b);
    cnote("wide char at last column: cursor %d,%d cell79=0x%x/0x%x cell(0,1)=0x%x", c.X, c.Y, cellch(b, 79, 0), cellattr(b, 79, 0), cellch(b, 0, 1));
    CC(c.Y == 1);
    CloseHandle(b);
}

CT(con_write_surrogate_pair_count) {
    CREQ(WriteConsoleW);
    HANDLE b = mkbuf_std(); if (!b) return;
    DWORD n = 0;
    CC(WriteConsoleW(b, L"\xD83D\xDE00", 2, &n, NULL));
    CEQ(n, 2);
    COORD c = curpos(b);
    cnote("surrogate pair cursor %d,%d cells 0x%x 0x%x", c.X, c.Y, cellch(b, 0, 0), cellch(b, 1, 0));
    CC(c.X >= 1 && c.Y == 0);
    CloseHandle(b);
}

CT(con_write_combining_mark) {
    CREQ(WriteConsoleW);
    HANDLE b = mkbuf_std(); if (!b) return;
    CC(wrote_n(b, L"e\x0301x", 3));
    COORD c = curpos(b);
    cnote("combining mark cursor x=%d", c.X);
    CC(c.Y == 0 && c.X >= 2 && c.X <= 3);
    CloseHandle(b);
}

CT(con_write_cursor_not_moved_on_other_buffer) {
    CREQ(WriteConsoleW);
    HANDLE a = mkbuf_std(), c = mkbuf_raw(); if (!a || !c) return;
    CC(wrote_n(a, L"aaaa", 4));
    CC(coeq(curpos(c), 0, 0));
    CC(wrote_n(c, L"cc", 2));
    CC(coeq(curpos(a), 4, 0));
    CC(coeq(curpos(c), 2, 0));
    CloseHandle(a); CloseHandle(c);
}

CT(con_write_concurrent_handles_same_buffer) {
    CREQ(WriteConsoleW);
    HANDLE a = mkbuf_std(); if (!a) return;
    HANDLE d = NULL;
    CC(DuplicateHandle(GetCurrentProcess(), a, GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS));
    CC(wrote_n(a, L"one", 3));
    CC(wrote_n(d, L"two", 3));
    CC(rowis(a, 0, 0, L"onetwo"));
    CC(coeq(curpos(d), 6, 0));
    CloseHandle(d); CloseHandle(a);
}

CT(con_write_conout_is_active_buffer) {
    CREQ(WriteConsoleW); CREQ(SetConsoleActiveScreenBuffer);
    HANDLE b = mkbuf_std(); if (!b) return;
    HANDLE orig = hout_();
    CC(SetConsoleActiveScreenBuffer(b));
    HANDLE co_ = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CNE(co_, INVALID_HANDLE_VALUE);
    CC(wrote_n(co_, L"viaconout", 9));
    CC(rowis(b, 0, 0, L"viaconout"));
    CC(SetConsoleActiveScreenBuffer(orig));
    CloseHandle(co_);
    CloseHandle(b);
}

CT(con_write_stdout_not_changed_by_active_switch) {
    CREQ(WriteConsoleW); CREQ(SetConsoleActiveScreenBuffer);
    HANDLE b = mkbuf_std(); if (!b) return;
    HANDLE orig = hout_();
    COORD pre = curpos(orig);
    CC(SetConsoleActiveScreenBuffer(b));
    CEQ(hout_(), orig);
    CC(wrote_n(orig, L"q", 1));
    CC(coeq(curpos(b), 0, 0));
    CEQ(curpos(orig).X, pre.X + 1);
    CC(SetConsoleActiveScreenBuffer(orig));
    CloseHandle(b);
}

/* ---------------- VT sequences ---------------- */

static HANDLE vtbuf(void) {
    HANDLE b = mkbuf_std(); if (!b) return NULL;
    DWORD m = ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetLastError(0xBAD);
    if (!SetConsoleMode(b, m)) {
        cnote("VT processing not supported: err=%lu", GetLastError());
        CloseHandle(b); return NULL;
    }
    return b;
}

/* P1: Win10 conhost routes IL/DL/tab VT operations through the *active* output buffer
   (DoSrvPrivateModifyLinesImpl uses the active buffer), so the buffer under test must be active. */
static HANDLE vt_activate(HANDLE b) {
    HANDLE orig = hout_();
    SetConsoleActiveScreenBuffer(b);
    return orig;
}
static void vt_deactivate(HANDLE orig) { SetConsoleActiveScreenBuffer(orig); }

/* attributes in effect after SGR 0 (the console's default colours, which a user may have customised) */
static WORD vt_dflt(HANDLE b) {
    DWORD n_ = 0; WriteConsoleW(b, L"\x1b[0m\x1b[1;1H ", 11, &n_, NULL);
    WORD a = cellattr(b, 0, 0);
    WriteConsoleW(b, L"\x1b[1;1H", 6, &n_, NULL);
    return a;
}
#define VTW(h, s) do { DWORD n_ = 0; BOOL r_ = WriteConsoleW((h), (s), lstrlenW(s), &n_, NULL); CC(r_); CCF(n_ == (DWORD)lstrlenW(s), "wrote %lu", n_); } while (0)

CT(con_vt_mode_settable_and_readable) {
    CREQ(SetConsoleMode); CREQ(GetConsoleMode);
    HANDLE b = mkbuf_raw(); if (!b) return;
    DWORD m = 0;
    CC(SetConsoleMode(b, ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING));
    CC(GetConsoleMode(b, &m));
    CEQ(m & ENABLE_VIRTUAL_TERMINAL_PROCESSING, ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    CC(SetConsoleMode(b, ENABLE_PROCESSED_OUTPUT));
    CC(GetConsoleMode(b, &m));
    CEQ(m & ENABLE_VIRTUAL_TERMINAL_PROCESSING, 0);
    CloseHandle(b);
}

CT(con_vt_cup_absolute) {
    CREQ(WriteConsoleW);
    HANDLE b = vtbuf(); if (!b) return;
    VTW(b, L"\x1b[3;5H");
    CC(coeq(curpos(b), 4, 2));
    VTW(b, L"\x1b[H");
    CC(coeq(curpos(b), 0, 0));
    VTW(b, L"\x1b[10;20f");
    CC(coeq(curpos(b), 19, 9));
    VTW(b, L"\x1b[7H");
    CC(coeq(curpos(b), 0, 6));
    VTW(b, L"X");
    CEQ(cellch(b, 0, 6), 'X');
    CloseHandle(b);
}

CT(con_vt_cursor_relative) {
    CREQ(WriteConsoleW);
    HANDLE b = vtbuf(); if (!b) return;
    VTW(b, L"\x1b[6;11H");   /* (10,5) */
    VTW(b, L"\x1b[2A"); CC(coeq(curpos(b), 10, 3));
    VTW(b, L"\x1b[3B"); CC(coeq(curpos(b), 10, 6));
    VTW(b, L"\x1b[4C"); CC(coeq(curpos(b), 14, 6));
    VTW(b, L"\x1b[2D"); CC(coeq(curpos(b), 12, 6));
    VTW(b, L"\x1b[A");  CC(coeq(curpos(b), 12, 5));
    VTW(b, L"\x1b[B");  CC(coeq(curpos(b), 12, 6));
    VTW(b, L"\x1b[C");  CC(coeq(curpos(b), 13, 6));
    VTW(b, L"\x1b[D");  CC(coeq(curpos(b), 12, 6));
    VTW(b, L"\x1b[100A"); CC(coeq(curpos(b), 12, 0));
    VTW(b, L"\x1b[100C"); CC(coeq(curpos(b), 79, 0));
    VTW(b, L"\x1b[100B"); CC(coeq(curpos(b), 79, 24));
    VTW(b, L"\x1b[200D"); CC(coeq(curpos(b), 0, 24));
    CloseHandle(b);
}

CT(con_vt_cursor_column_line) {
    CREQ(WriteConsoleW);
    HANDLE b = vtbuf(); if (!b) return;
    VTW(b, L"\x1b[5;5H");
    VTW(b, L"\x1b[10G"); CC(coeq(curpos(b), 9, 4));
    VTW(b, L"\x1b[8d");  CC(coeq(curpos(b), 9, 7));
    VTW(b, L"\x1b[2E");  CC(coeq(curpos(b), 0, 9));
    VTW(b, L"\x1b[3F");  CC(coeq(curpos(b), 0, 6));
    CloseHandle(b);
}

CT(con_vt_save_restore_cursor) {
    CREQ(WriteConsoleW);
    HANDLE b = vtbuf(); if (!b) return;
    VTW(b, L"\x1b[4;9H\x1b[s\x1b[10;10H");
    CC(coeq(curpos(b), 9, 9));
    VTW(b, L"\x1b[u");
    CC(coeq(curpos(b), 8, 3));
    VTW(b, L"\x1b" L"7\x1b[20;20H\x1b" L"8");
    CC(coeq(curpos(b), 8, 3));
    CloseHandle(b);
}

CT(con_vt_sgr_foreground) {
    CREQ(WriteConsoleW);
    HANDLE b = vtbuf(); if (!b) return;
    static const struct { const WCHAR *seq; WORD fg; } t[] = {
        { L"\x1b[30m", 0 }, { L"\x1b[31m", FOREGROUND_RED }, { L"\x1b[32m", FOREGROUND_GREEN }, { L"\x1b[33m", FOREGROUND_RED | FOREGROUND_GREEN },
        { L"\x1b[34m", FOREGROUND_BLUE }, { L"\x1b[35m", FOREGROUND_RED | FOREGROUND_BLUE }, { L"\x1b[36m", FOREGROUND_GREEN | FOREGROUND_BLUE },
        { L"\x1b[37m", 7 }, { L"\x1b[90m", FOREGROUND_INTENSITY }, { L"\x1b[91m", FOREGROUND_RED | FOREGROUND_INTENSITY },
        { L"\x1b[92m", FOREGROUND_GREEN | FOREGROUND_INTENSITY }, { L"\x1b[93m", 14 }, { L"\x1b[94m", FOREGROUND_BLUE | FOREGROUND_INTENSITY },
        { L"\x1b[95m", 13 }, { L"\x1b[96m", 11 }, { L"\x1b[97m", 15 },
    };
    WORD dfl = vt_dflt(b);
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++) {
        clear_buf(b);
        VTW(b, L"\x1b[0m");
        VTW(b, t[i].seq);
        VTW(b, L"X");
        WORD a = cellattr(b, 0, 0);
        CCF((a & 0xF) == t[i].fg, "case %zu attr=0x%x want fg 0x%x", i, a, t[i].fg);
        CCF((a & 0xF0) == (dfl & 0xF0), "case %zu bg changed attr=0x%x", i, a);
        VTW(b, L"\x1b[0m");
    }
    CloseHandle(b);
}

CT(con_vt_sgr_background) {
    CREQ(WriteConsoleW);
    HANDLE b = vtbuf(); if (!b) return;
    static const struct { const WCHAR *seq; WORD bg; } t[] = {
        { L"\x1b[40m", 0 }, { L"\x1b[41m", BACKGROUND_RED }, { L"\x1b[42m", BACKGROUND_GREEN }, { L"\x1b[43m", BACKGROUND_RED | BACKGROUND_GREEN },
        { L"\x1b[44m", BACKGROUND_BLUE }, { L"\x1b[45m", BACKGROUND_RED | BACKGROUND_BLUE }, { L"\x1b[46m", BACKGROUND_GREEN | BACKGROUND_BLUE },
        { L"\x1b[47m", 0x70 }, { L"\x1b[100m", BACKGROUND_INTENSITY }, { L"\x1b[101m", BACKGROUND_RED | BACKGROUND_INTENSITY },
        { L"\x1b[107m", 0xF0 },
    };
    WORD dfl = vt_dflt(b);
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++) {
        clear_buf(b);
        VTW(b, L"\x1b[0m");
        VTW(b, t[i].seq);
        VTW(b, L"X");
        WORD a = cellattr(b, 0, 0);
        CCF((a & 0xF0) == t[i].bg, "case %zu attr=0x%x want bg 0x%x", i, a, t[i].bg);
        CCF((a & 0xF) == (dfl & 0xF), "case %zu fg changed attr=0x%x", i, a);
        VTW(b, L"\x1b[0m");
    }
    CloseHandle(b);
}

CT(con_vt_sgr_bold_and_reset) {
    CREQ(WriteConsoleW);
    HANDLE b = vtbuf(); if (!b) return;
    WORD dfl = vt_dflt(b) & 0xFF;
    WORD dfg = dfl & 0xF, dbg = dfl & 0xF0;
    VTW(b, L"\x1b[1mA");
    CEQ(cellattr(b, 0, 0) & 0xF, dfg | FOREGROUND_INTENSITY);
    VTW(b, L"\x1b[31mB");
    CEQ(cellattr(b, 1, 0) & 0xF, FOREGROUND_RED | FOREGROUND_INTENSITY); (void)dbg;
    VTW(b, L"\x1b[22mC");
    CEQ(cellattr(b, 2, 0) & FOREGROUND_INTENSITY, 0);
    VTW(b, L"\x1b[44;33mD");
    CEQ(cellattr(b, 3, 0) & 0xFF, BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN);
    VTW(b, L"\x1b[0mE");
    CEQ(cellattr(b, 4, 0) & 0xFF, dfl);
    VTW(b, L"\x1b[1;42;30mF\x1b[mG");
    CEQ(cellattr(b, 5, 0) & 0xFF, BACKGROUND_GREEN | FOREGROUND_INTENSITY);
    CEQ(cellattr(b, 6, 0) & 0xFF, dfl);
    CloseHandle(b);
}

CT(con_vt_sgr_default_colors) {
    CREQ(WriteConsoleW);
    HANDLE b = vtbuf(); if (!b) return;
    CC(SetConsoleTextAttribute(b, 0x1E));
    VTW(b, L"\x1b[39mA");
    CEQ(cellattr(b, 0, 0) & 0xF0, 0x10);
    WORD dflbg = vt_dflt(b) & 0xF0; CC(SetConsoleTextAttribute(b, 0x1E));
    VTW(b, L"\x1b[39mA");
    cnote("39m default fg -> attr 0x%x", cellattr(b, 0, 0));
    VTW(b, L"\x1b[49mB");
    cnote("49m default bg -> attr 0x%x", cellattr(b, 1, 0));
    CC((cellattr(b, 1, 0) & 0xF0) == dflbg);
    CloseHandle(b);
}

CT(con_vt_sgr_reverse_underline) {
    CREQ(WriteConsoleW);
    HANDLE b = vtbuf(); if (!b) return;
    VTW(b, L"\x1b[7mA");
    WORD a = cellattr(b, 0, 0);
    cnote("SGR 7 attr=0x%x", a);
    CC((a & COMMON_LVB_REVERSE_VIDEO) || ((a & 0xFF) == 0x70));
    VTW(b, L"\x1b[27mB");
    a = cellattr(b, 1, 0);
    CEQ(a & COMMON_LVB_REVERSE_VIDEO, 0);
    VTW(b, L"\x1b[4mC");
    a = cellattr(b, 2, 0);
    cnote("SGR 4 attr=0x%x", a);
    CC(a & COMMON_LVB_UNDERSCORE);
    VTW(b, L"\x1b[24mD");
    CEQ(cellattr(b, 3, 0) & COMMON_LVB_UNDERSCORE, 0);
    CloseHandle(b);
}

CT(con_vt_sgr_256_and_rgb) {
    CREQ(WriteConsoleW);
    HANDLE b = vtbuf(); if (!b) return;
    HANDLE orig_ = vt_activate(b);   /* P1: UNRESOLVED, hypothesis: palette-index SGR uses the active buffer */
    { CONSOLE_SCREEN_BUFFER_INFOEX ix_; memset(&ix_, 0, sizeof ix_); ix_.cbSize = sizeof ix_;
      if (GetConsoleScreenBufferInfoEx(b, &ix_)) cnote("P1c palette: [1]=0x%06lx [4]=0x%06lx [9]=0x%06lx [12]=0x%06lx", (unsigned long)ix_.ColorTable[1], (unsigned long)ix_.ColorTable[4], (unsigned long)ix_.ColorTable[9], (unsigned long)ix_.ColorTable[12]); }
    VTW(b, L"\x1b[38;5;1mA");
    CEQ(cellattr(b, 0, 0) & 0xF, FOREGROUND_RED);
    VTW(b, L"\x1b[48;5;4mB");
    CEQ(cellattr(b, 1, 0) & 0xF0, BACKGROUND_BLUE);
    VTW(b, L"\x1b[38;2;255;0;0mC");
    cnote("38;2 red -> attr 0x%x", cellattr(b, 2, 0));
    CC((cellattr(b, 2, 0) & 0xF) != 0);
    vt_deactivate(orig_);
    CloseHandle(b);
}

CT(con_vt_erase_in_line) {
    CREQ(WriteConsoleW);
    HANDLE b = vtbuf(); if (!b) return;
    VTW(b, L"0123456789");
    VTW(b, L"\x1b[1;6H\x1b[K");
    CC(rowis(b, 0, 0, L"01234     "));
    VTW(b, L"\x1b[1;1H0123456789\x1b[1;4H\x1b[1K");
    CC(rowis(b, 0, 0, L"    456789"));
    VTW(b, L"\x1b[1;1H0123456789\x1b[2K");
    CC(rowis(b, 0, 0, L"          "));
    CloseHandle(b);
}

CT(con_vt_erase_in_display) {
    CREQ(WriteConsoleW);
    HANDLE b = vtbuf(); if (!b) return;
    for (int y = 0; y < 5; y++) { VTW(b, L"\x1b[1;1H"); CC(SetConsoleCursorPosition(b, co(0, y))); VTW(b, L"rowXXXXXXXXXX"); }
    VTW(b, L"\x1b[3;4H\x1b[J");
    CC(rowis(b, 1, 0, L"rowXXXXXXXXXX"));
    CC(rowis(b, 2, 0, L"row"));
    CEQ(cellch(b, 3, 2), ' ');
    CEQ(cellch(b, 0, 3), ' ');
    CEQ(cellch(b, 0, 4), ' ');
    VTW(b, L"\x1b[2J");
    CEQ(cellch(b, 0, 0), ' '); CEQ(cellch(b, 0, 1), ' ');
    CloseHandle(b);
}

CT(con_vt_erase_chars_insert_delete) {
    CREQ(WriteConsoleW);
    HANDLE b = vtbuf(); if (!b) return;
    VTW(b, L"abcdefghij\x1b[1;3H\x1b[2X");
    CC(rowis(b, 0, 0, L"ab  efghij"));
    VTW(b, L"\x1b[1;1Habcdefghij\x1b[1;3H\x1b[2P");
    CC(rowis(b, 0, 0, L"abefghij  "));
    VTW(b, L"\x1b[1;1Habcdefghij\x1b[1;3H\x1b[2@");
    CC(rowis(b, 0, 0, L"ab  cdefgh"));
    CloseHandle(b);
}

CT(con_vt_insert_delete_lines) {
    CREQ(WriteConsoleW);
    HANDLE b = vtbuf(); if (!b) return;
    HANDLE orig_ = vt_activate(b);
    for (int y = 0; y < 4; y++) {
        CC(SetConsoleCursorPosition(b, co(0, y)));
        WCHAR t[8]; wsprintfW(t, L"line%d", y); VTW(b, t);
    }
    VTW(b, L"\x1b[2;1H\x1b[L");
    CC(rowis(b, 0, 0, L"line0"));
    CEQ(cellch(b, 0, 1), ' ');
    CC(rowis(b, 2, 0, L"line1"));
    CC(rowis(b, 3, 0, L"line2"));
    VTW(b, L"\x1b[2;1H\x1b[M");
    CC(rowis(b, 1, 0, L"line1"));
    CC(rowis(b, 2, 0, L"line2"));
    vt_deactivate(orig_);
    CloseHandle(b);
}

CT(con_vt_scroll_up_down) {
    CREQ(WriteConsoleW);
    HANDLE b = vtbuf(); if (!b) return;
    for (int y = 0; y < 3; y++) { CC(SetConsoleCursorPosition(b, co(0, y))); WCHAR t[8]; wsprintfW(t, L"r%d", y); VTW(b, t); }
    VTW(b, L"\x1b[S");
    CC(rowis(b, 0, 0, L"r1")); CC(rowis(b, 1, 0, L"r2"));
    VTW(b, L"\x1b[T");
    CC(rowis(b, 1, 0, L"r1")); CC(rowis(b, 2, 0, L"r2"));
    CEQ(cellch(b, 0, 0), ' ');
    CloseHandle(b);
}

CT(con_vt_set_title_osc) {
    CREQ(WriteConsoleW); CREQ(GetConsoleTitleW);
    HANDLE b = vtbuf(); if (!b) return;
    VTW(b, L"\x1b]0;k32t-osc-title\x1b\\");
    WCHAR t[64]; GetConsoleTitleW(t, 64);
    CWSTR(t, L"k32t-osc-title");
    VTW(b, L"\x1b]2;second\x1b\\");
    GetConsoleTitleW(t, 64);
    CWSTR(t, L"second");
    CloseHandle(b);
}

CT(con_vt_cursor_visibility_mode) {
    CREQ(WriteConsoleW); CREQ(GetConsoleCursorInfo);
    HANDLE b = vtbuf(); if (!b) return;
    CONSOLE_CURSOR_INFO ci;
    VTW(b, L"\x1b[?25l");
    CC(GetConsoleCursorInfo(b, &ci)); CEQ(ci.bVisible, 0);
    VTW(b, L"\x1b[?25h");
    CC(GetConsoleCursorInfo(b, &ci)); CEQ(ci.bVisible, 1);
    CloseHandle(b);
}

CT(con_vt_alternate_buffer_does_not_crash) {
    CREQ(WriteConsoleW);
    HANDLE b = vtbuf(); if (!b) return;
    VTW(b, L"main");
    VTW(b, L"\x1b[?1049h");
    VTW(b, L"alt");
    VTW(b, L"\x1b[?1049l");
    cnote("after alt buffer round trip row0=\"%c%c%c%c\"", cellch(b, 0, 0), cellch(b, 1, 0), cellch(b, 2, 0), cellch(b, 3, 0));
    CloseHandle(b);
}

CT(con_vt_off_literal_escape) {
    CREQ(WriteConsoleW);
    HANDLE b = mkbuf_std(); if (!b) return;
    CC(SetConsoleMode(b, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT));
    CC(wrote_n(b, L"\x1b[31mX", 6));
    COORD c = curpos(b);
    CCF(c.X == 6 && c.Y == 0, "cursor %d,%d", c.X, c.Y);
    CEQ(cellattr(b, 5, 0) & 0xF, 7);
    CloseHandle(b);
}

CT(con_vt_incomplete_sequence_split_across_writes) {
    CREQ(WriteConsoleW);
    HANDLE b = vtbuf(); if (!b) return;
    VTW(b, L"\x1b[");
    VTW(b, L"3;4H");
    COORD c = curpos(b);
    cnote("split CSI -> cursor %d,%d", c.X, c.Y);
    CloseHandle(b);
}

CT(con_vt_disable_newline_auto_return) {
    CREQ(WriteConsoleW);
    HANDLE b = mkbuf_std(); if (!b) return;
    CC(SetConsoleMode(b, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN));
    CC(wrote_n(b, L"ab\ncd", 5));
    CC(rowis(b, 0, 0, L"ab"));
    CC(rowis(b, 1, 2, L"cd"));
    CC(coeq(curpos(b), 4, 1));
    CC(wrote_n(b, L"\r\nef", 4));
    CC(rowis(b, 2, 0, L"ef"));
    CloseHandle(b);
}

CT(con_vt_newline_auto_return_default) {
    CREQ(WriteConsoleW);
    HANDLE b = vtbuf(); if (!b) return;
    VTW(b, L"ab\ncd");
    CC(rowis(b, 1, 0, L"cd"));
    CloseHandle(b);
}

CT(con_vt_device_attributes_no_hang) {
    CREQ(WriteConsoleW);
    HANDLE b = vtbuf(); if (!b) return;
    VTW(b, L"\x1b[c");
    VTW(b, L"\x1b[6n");
    cnote("DA/DSR queries accepted; input events pending: %lu", inp_count());
    CloseHandle(b);
}

CT(con_vt_dec_line_drawing_charset) {
    CREQ(WriteConsoleW);
    HANDLE b = vtbuf(); if (!b) return;
    VTW(b, L"\x1b(0lqk\x1b(B");
    cnote("line drawing cells: 0x%x 0x%x 0x%x", cellch(b, 0, 0), cellch(b, 1, 0), cellch(b, 2, 0));
    CEQ(cellch(b, 0, 0), 0x250C);
    CEQ(cellch(b, 1, 0), 0x2500);
    CEQ(cellch(b, 2, 0), 0x2510);
    VTW(b, L"q");
    CEQ(cellch(b, 3, 0), 'q');
    CloseHandle(b);
}

CT(con_vt_scroll_region) {
    CREQ(WriteConsoleW);
    HANDLE b = vtbuf(); if (!b) return;
    for (int y = 0; y < 6; y++) { CC(SetConsoleCursorPosition(b, co(0, y))); WCHAR t[8]; wsprintfW(t, L"R%d", y); VTW(b, t); }
    VTW(b, L"\x1b[2;4r");   /* region rows 1..3 (0-based) */
    VTW(b, L"\x1b[4;1H\n");
    CC(rowis(b, 0, 0, L"R0"));
    cnote("scroll region rows: %c%c %c%c %c%c %c%c", cellch(b, 0, 1), cellch(b, 1, 1), cellch(b, 0, 2), cellch(b, 1, 2), cellch(b, 0, 3), cellch(b, 1, 3), cellch(b, 0, 4), cellch(b, 1, 4));
    CC(rowis(b, 4, 0, L"R4"));
    VTW(b, L"\x1b[r");
    CloseHandle(b);
}

CT(con_vt_repeat_and_tab_controls) {
    CREQ(WriteConsoleW);
    HANDLE b = vtbuf(); if (!b) return;
    HANDLE orig_ = vt_activate(b);
    VTW(b, L"a\x1b[3b");
    cnote("REP result: %c%c%c%c", cellch(b, 0, 0), cellch(b, 1, 0), cellch(b, 2, 0), cellch(b, 3, 0));
    /* P1: Win10 conhost has no default VT tab stops (GetForwardTab with an empty list jumps to the
       end of the row), so define stops at columns 8 and 16 explicitly with HTS. UNVERIFIED-until-rerun */
    VTW(b, L"\x1b[3g\x1b[1;9H\x1bH\x1b[1;17H\x1bH");
    VTW(b, L"\x1b[1;1H\x1b[2I");
    COORD c_ = curpos(b); cnote("after CHT x2 cursor %d,%d", c_.X, c_.Y);
    CC(coeq(curpos(b), 16, 0));
    VTW(b, L"\x1b[Z");
    c_ = curpos(b); cnote("after CBT cursor %d,%d", c_.X, c_.Y);
    CC(coeq(curpos(b), 8, 0));
    vt_deactivate(orig_);
    CloseHandle(b);
}
