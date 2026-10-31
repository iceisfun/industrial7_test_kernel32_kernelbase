/* gap_10: ReadConsoleInputA/W and ReadConsoleA/W in a private console (child process) */
#include "g10.h"

static WORD vk_of(WCHAR c) {
    if (c >= 'a' && c <= 'z') return (WORD)(c - 'a' + 'A');
    if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return (WORD)c;
    if (c == ' ') return VK_SPACE;
    if (c == '\r') return VK_RETURN;
    if (c == '\t') return VK_TAB;
    if (c == 8) return VK_BACK;
    return 0;
}
static INPUT_RECORD keyrec(WCHAR c, WORD vk, BOOL down, DWORD ctl, WORD rep) {
    INPUT_RECORD r; memset(&r, 0, sizeof r);
    r.EventType = KEY_EVENT;
    r.Event.KeyEvent.bKeyDown = down;
    r.Event.KeyEvent.wRepeatCount = rep;
    r.Event.KeyEvent.wVirtualKeyCode = vk;
    r.Event.KeyEvent.wVirtualScanCode = vk ? (WORD)MapVirtualKeyA(vk, 0) : 0;
    r.Event.KeyEvent.uChar.UnicodeChar = c;
    r.Event.KeyEvent.dwControlKeyState = ctl;
    return r;
}
static DWORD inj(const INPUT_RECORD *r, DWORD n) { DWORD w = 0; WriteConsoleInputW(gin(), r, n, &w); return w; }
static DWORD typestr(const WCHAR *s) {
    INPUT_RECORD r[256]; int n = 0;
    for (; *s && n < 254; s++) {
        r[n++] = keyrec(*s, vk_of(*s), TRUE, 0, 1);
        r[n++] = keyrec(*s, vk_of(*s), FALSE, 0, 1);
    }
    return inj(r, n);
}
static DWORD qcount(void) { DWORD n = 0xFFFFFFFF; GetNumberOfConsoleInputEvents(gin(), &n); return n; }
static void reset_in(DWORD mode) {
    FlushConsoleInputBuffer(gin());
    SetConsoleMode(gin(), mode);
}
#define COOKED (ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT)

/* ---------- ReadConsoleInputW ---------- */

GC(rci_w_key_roundtrip) {
    GREQ(U(ReadConsoleInputW));
    reset_in(0);
    INPUT_RECORD w = keyrec(L'q', 'Q', TRUE, SHIFT_PRESSED | LEFT_CTRL_PRESSED, 1), o; DWORD n = 0xAA;
    GEQ(inj(&w, 1), 1);
    SetLastError(0xBAD);
    GCHK(U(ReadConsoleInputW)(gin(), &o, 1, &n));
    GEQ(n, 1);
    GEQ(o.EventType, KEY_EVENT);
    GEQ(o.Event.KeyEvent.bKeyDown, 1);
    GEQ(o.Event.KeyEvent.wVirtualKeyCode, 'Q');
    GEQ(o.Event.KeyEvent.uChar.UnicodeChar, L'q');
    GEQ(o.Event.KeyEvent.wVirtualScanCode, w.Event.KeyEvent.wVirtualScanCode);
    GEQ(o.Event.KeyEvent.dwControlKeyState, SHIFT_PRESSED | LEFT_CTRL_PRESSED);
    GEQ(o.Event.KeyEvent.wRepeatCount, 1);
}
GC(rci_w_key_up_event) {
    GREQ(U(ReadConsoleInputW));
    reset_in(0);
    INPUT_RECORD w = keyrec(L'z', 'Z', FALSE, 0, 1), o; DWORD n = 0;
    inj(&w, 1);
    GCHK(U(ReadConsoleInputW)(gin(), &o, 1, &n));
    GEQ(n, 1); GEQ(o.EventType, KEY_EVENT); GEQ(o.Event.KeyEvent.bKeyDown, 0);
    GEQ(o.Event.KeyEvent.wVirtualKeyCode, 'Z');
}
GC(rci_w_control_key_state_preserved) {
    GREQ(U(ReadConsoleInputW));
    static const DWORD st[] = { 0, SHIFT_PRESSED, LEFT_CTRL_PRESSED, RIGHT_CTRL_PRESSED, LEFT_ALT_PRESSED, RIGHT_ALT_PRESSED,
        ENHANCED_KEY, CAPSLOCK_ON, NUMLOCK_ON, SCROLLLOCK_ON, SHIFT_PRESSED | LEFT_ALT_PRESSED | ENHANCED_KEY | NUMLOCK_ON };
    for (size_t i = 0; i < sizeof st / sizeof st[0]; i++) {
        reset_in(0);
        INPUT_RECORD w = keyrec(L'a', 'A', TRUE, st[i], 1), o; DWORD n = 0;
        inj(&w, 1);
        if (!qcount()) { g10_cnote("state 0x%lx not queued", st[i]); continue; }
        GCHK(U(ReadConsoleInputW)(gin(), &o, 1, &n));
        GEQ(n, 1);
        GEQ(o.Event.KeyEvent.dwControlKeyState, st[i]);
    }
}
GC(rci_w_limited_length_leaves_rest) {
    GREQ(U(ReadConsoleInputW));
    reset_in(0);
    INPUT_RECORD w[5], o[5]; DWORD n = 0;
    for (int i = 0; i < 5; i++) w[i] = keyrec(L'a' + i, 'A' + i, TRUE, 0, 1);
    GEQ(inj(w, 5), 5);
    GEQ(qcount(), 5);
    GCHK(U(ReadConsoleInputW)(gin(), o, 2, &n)); GEQ(n, 2);
    GEQ(o[0].Event.KeyEvent.uChar.UnicodeChar, L'a'); GEQ(o[1].Event.KeyEvent.uChar.UnicodeChar, L'b');
    GEQ(qcount(), 3);
    GCHK(U(ReadConsoleInputW)(gin(), o, 2, &n)); GEQ(n, 2);
    GEQ(o[0].Event.KeyEvent.uChar.UnicodeChar, L'c'); GEQ(o[1].Event.KeyEvent.uChar.UnicodeChar, L'd');
    GCHK(U(ReadConsoleInputW)(gin(), o, 2, &n)); GEQ(n, 1);
    GEQ(o[0].Event.KeyEvent.uChar.UnicodeChar, L'e');
    GEQ(qcount(), 0);
}
GC(rci_w_returns_available_not_requested) {
    GREQ(U(ReadConsoleInputW));
    reset_in(0);
    INPUT_RECORD w[3], o[64]; DWORD n = 0;
    for (int i = 0; i < 3; i++) w[i] = keyrec(L'x', 'X', i & 1 ? FALSE : TRUE, 0, 1);
    inj(w, 3);
    GCHK(U(ReadConsoleInputW)(gin(), o, 64, &n));
    GEQ(n, 3);
    GEQ(qcount(), 0);
}
GC(rci_w_fifo_order_100) {
    GREQ(U(ReadConsoleInputW));
    reset_in(0);
    INPUT_RECORD w[100], o[100]; DWORD n = 0;
    for (int i = 0; i < 100; i++) w[i] = keyrec(L'a' + (i % 26), 'A' + (i % 26), (i / 26) & 1, (DWORD)(i & 1) * SHIFT_PRESSED, 1);
    GEQ(inj(w, 100), 100);
    GCHK(U(ReadConsoleInputW)(gin(), o, 100, &n));
    GEQ(n, 100);
    int bad = 0;
    for (int i = 0; i < 100 && i < (int)n; i++)
        if (o[i].EventType != KEY_EVENT || o[i].Event.KeyEvent.uChar.UnicodeChar != w[i].Event.KeyEvent.uChar.UnicodeChar ||
            o[i].Event.KeyEvent.bKeyDown != w[i].Event.KeyEvent.bKeyDown) bad++;
    GEQ(bad, 0);
}
GC(rci_w_big_batch_1000) {
    GREQ(U(ReadConsoleInputW));
    reset_in(0);
    static INPUT_RECORD w[1000], o[1000]; DWORD n = 0;
    for (int i = 0; i < 1000; i++) w[i] = keyrec(L'a' + (i % 26), 'A' + (i % 26), TRUE, 0, (WORD)(1 + i % 7));
    DWORD wr = inj(w, 1000);
    GEQ(wr, 1000);
    GEQ(qcount(), 1000);
    GCHK(U(ReadConsoleInputW)(gin(), o, 1000, &n));
    GEQ(n, 1000);
    int bad = 0;
    for (int i = 0; i < 1000 && i < (int)n; i++) if (o[i].Event.KeyEvent.uChar.UnicodeChar != w[i].Event.KeyEvent.uChar.UnicodeChar) bad++;
    GEQ(bad, 0);
}
GC(rci_w_unicode_chars_preserved) {
    GREQ(U(ReadConsoleInputW));
    reset_in(0);
    static const WCHAR cs[] = { 0x00E9, 0x20AC, 0x4E2D, 0x0416, 0xFFFD, 0x00FF, 0x0100, 0xD7FF };
    for (size_t i = 0; i < sizeof cs / sizeof cs[0]; i++) {
        INPUT_RECORD w = keyrec(cs[i], 0, TRUE, 0, 1), o; DWORD n = 0;
        inj(&w, 1);
        GCHK(U(ReadConsoleInputW)(gin(), &o, 1, &n));
        GEQ(n, 1);
        GEQ(o.Event.KeyEvent.uChar.UnicodeChar, cs[i]);
    }
}
GC(rci_w_nonkey_events_roundtrip) {
    GREQ(U(ReadConsoleInputW));
    reset_in(0);
    INPUT_RECORD w, o; DWORD n = 0;
    memset(&w, 0, sizeof w);
    w.EventType = FOCUS_EVENT; w.Event.FocusEvent.bSetFocus = TRUE;
    inj(&w, 1);
    if (qcount()) {
        GCHK(U(ReadConsoleInputW)(gin(), &o, 1, &n)); GEQ(n, 1);
        GEQ(o.EventType, FOCUS_EVENT); GEQ(o.Event.FocusEvent.bSetFocus, 1);
    } else g10_cnote("focus event not queued");
    memset(&w, 0, sizeof w);
    w.EventType = MENU_EVENT; w.Event.MenuEvent.dwCommandId = 0x1234;
    inj(&w, 1);
    if (qcount()) {
        GCHK(U(ReadConsoleInputW)(gin(), &o, 1, &n)); GEQ(n, 1);
        GEQ(o.EventType, MENU_EVENT); GEQ(o.Event.MenuEvent.dwCommandId, 0x1234);
    } else g10_cnote("menu event not queued");
    memset(&w, 0, sizeof w);
    w.EventType = WINDOW_BUFFER_SIZE_EVENT; w.Event.WindowBufferSizeEvent.dwSize = gco(77, 33);
    inj(&w, 1);
    if (qcount()) {
        GCHK(U(ReadConsoleInputW)(gin(), &o, 1, &n)); GEQ(n, 1);
        GEQ(o.EventType, WINDOW_BUFFER_SIZE_EVENT);
        GEQ(o.Event.WindowBufferSizeEvent.dwSize.X, 77); GEQ(o.Event.WindowBufferSizeEvent.dwSize.Y, 33);
    } else g10_cnote("buffer size event not queued");
    memset(&w, 0, sizeof w);
    w.EventType = MOUSE_EVENT; w.Event.MouseEvent.dwMousePosition = gco(5, 6);
    w.Event.MouseEvent.dwButtonState = FROM_LEFT_1ST_BUTTON_PRESSED; w.Event.MouseEvent.dwEventFlags = DOUBLE_CLICK;
    inj(&w, 1);
    if (qcount()) {
        GCHK(U(ReadConsoleInputW)(gin(), &o, 1, &n)); GEQ(n, 1);
        GEQ(o.EventType, MOUSE_EVENT);
        GEQ(o.Event.MouseEvent.dwMousePosition.X, 5); GEQ(o.Event.MouseEvent.dwMousePosition.Y, 6);
        GEQ(o.Event.MouseEvent.dwButtonState, FROM_LEFT_1ST_BUTTON_PRESSED);
        GEQ(o.Event.MouseEvent.dwEventFlags, DOUBLE_CLICK);
    } else g10_cnote("mouse event not queued");
}
GC(rci_w_mixed_event_order) {
    GREQ(U(ReadConsoleInputW));
    reset_in(0);
    INPUT_RECORD w[3], o[3]; DWORD n = 0;
    memset(w, 0, sizeof w);
    w[0] = keyrec(L'k', 'K', TRUE, 0, 1);
    w[1].EventType = FOCUS_EVENT; w[1].Event.FocusEvent.bSetFocus = FALSE;
    w[2] = keyrec(L'l', 'L', TRUE, 0, 1);
    DWORD wr = inj(w, 3);
    GEQ(wr, 3);
    if (qcount() == 3) {
        GCHK(U(ReadConsoleInputW)(gin(), o, 3, &n)); GEQ(n, 3);
        GEQ(o[0].EventType, KEY_EVENT); GEQ(o[1].EventType, FOCUS_EVENT); GEQ(o[2].EventType, KEY_EVENT);
        GEQ(o[2].Event.KeyEvent.uChar.UnicodeChar, L'l');
    } else g10_cnote("queued %lu of 3", qcount());
}
GC(rci_w_consumes_events) {
    GREQ(U(ReadConsoleInputW));
    reset_in(0);
    INPUT_RECORD w[4], o; DWORD n = 0;
    for (int i = 0; i < 4; i++) w[i] = keyrec(L'a' + i, 'A' + i, TRUE, 0, 1);
    inj(w, 4);
    for (int i = 3; i >= 0; i--) {
        GCHK(U(ReadConsoleInputW)(gin(), &o, 1, &n));
        GEQ(qcount(), (DWORD)i);
    }
}
GC(rci_w_input_handle_signalled) {
    GREQ(U(ReadConsoleInputW));
    reset_in(0);
    GEQ(WaitForSingleObject(gin(), 0), WAIT_TIMEOUT);
    INPUT_RECORD w = keyrec(L'a', 'A', TRUE, 0, 1), o; DWORD n = 0;
    inj(&w, 1);
    GEQ(WaitForSingleObject(gin(), 0), WAIT_OBJECT_0);
    GCHK(U(ReadConsoleInputW)(gin(), &o, 1, &n));
    GEQ(WaitForSingleObject(gin(), 0), WAIT_TIMEOUT);
}
GC(rci_w_after_flush_empty) {
    GREQ(U(ReadConsoleInputW));
    reset_in(0);
    INPUT_RECORD w[3];
    for (int i = 0; i < 3; i++) w[i] = keyrec(L'a', 'A', TRUE, 0, 1);
    inj(w, 3);
    GEQ(qcount(), 3);
    FlushConsoleInputBuffer(gin());
    GEQ(qcount(), 0);
    GEQ(WaitForSingleObject(gin(), 0), WAIT_TIMEOUT);
}
GC(rci_w_repeat_count) {
    GREQ(U(ReadConsoleInputW));
    reset_in(0);
    static const WORD reps[] = { 0, 1, 2, 5, 100, 0xFFFF };
    for (size_t i = 0; i < sizeof reps / sizeof reps[0]; i++) {
        INPUT_RECORD w = keyrec(L'r', 'R', TRUE, 0, reps[i]), o; DWORD n = 0;
        inj(&w, 1);
        if (!qcount()) { g10_cnote("repeat %u not queued", reps[i]); continue; }
        GCHK(U(ReadConsoleInputW)(gin(), &o, 1, &n)); GEQ(n, 1);
        if (reps[i] != 0) GEQ(o.Event.KeyEvent.wRepeatCount, reps[i]);
        else g10_cnote("repeat 0 read back as %u", o.Event.KeyEvent.wRepeatCount);
    }
}
GC(rci_w_blocks_until_input) {
    GREQ(U(ReadConsoleInputW));
    reset_in(0);
    struct { int dummy; } unused; (void)unused;
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    DWORD WINAPI th(LPVOID p) {
        Sleep(250);
        INPUT_RECORD w = keyrec(L'w', 'W', TRUE, 0, 1);
        inj(&w, 1);
        SetEvent((HANDLE)p);
        return 0;
    }
    HANDLE t = CreateThread(NULL, 0, th, ev, 0, NULL);
    INPUT_RECORD o; DWORD n = 0;
    DWORD t0 = GetTickCount();
    BOOL ok = U(ReadConsoleInputW)(gin(), &o, 1, &n);
    DWORD dt = GetTickCount() - t0;
    GCHK(ok); GEQ(n, 1);
    GCHK(dt >= 150);
    GEQ(WaitForSingleObject(ev, 5000), WAIT_OBJECT_0);
    GEQ(o.Event.KeyEvent.uChar.UnicodeChar, L'w');
    WaitForSingleObject(t, 5000); CloseHandle(t); CloseHandle(ev);
}
GC(rci_w_invalid_handles) {
    GREQ(U(ReadConsoleInputW));
    reset_in(0);
    INPUT_RECORD o[2]; DWORD n = 0xAA;
    HANDLE ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    HANDLE cl = CreateEventA(NULL, FALSE, FALSE, NULL); CloseHandle(cl);
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x1234, ev, cl, (HANDLE)(LONG_PTR)-2 };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        n = 0xAA;
        GFAIL(U(ReadConsoleInputW)(bad[i], o, 2, &n), ERROR_INVALID_HANDLE);
    }
    CloseHandle(ev);
}
GC(rci_w_output_handle_rejected) {
    GREQ(U(ReadConsoleInputW));
    reset_in(0);
    INPUT_RECORD o[2]; DWORD n = 0xAA;
    SetLastError(0xBAD);
    BOOL ok = U(ReadConsoleInputW)(gout(), o, 2, &n);
    GEQ(ok, 0);
    g10_cnote("ReadConsoleInputW(output handle) err=%lu", GetLastError());
}
GC(rci_w_file_handle_rejected) {
    GREQ(U(ReadConsoleInputW));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "g10_rci.bin");
    HANDLE f = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    GNE(f, INVALID_HANDLE_VALUE);
    INPUT_RECORD o[2]; DWORD n = 0xAA;
    SetLastError(0xBAD);
    BOOL ok = U(ReadConsoleInputW)(f, o, 2, &n);
    GEQ(ok, 0);
    g10_cnote("ReadConsoleInputW(file handle) err=%lu", GetLastError());
    CloseHandle(f);
}
GC(rci_w_zero_length) {
    GREQ(U(ReadConsoleInputW));
    reset_in(0);
    INPUT_RECORD w = keyrec(L'a', 'A', TRUE, 0, 1), o; DWORD n = 0xAA;
    inj(&w, 1);
    SetLastError(0xBAD);
    BOOL ok = U(ReadConsoleInputW)(gin(), &o, 0, &n);
    g10_cnote("ReadConsoleInputW(len 0) ok=%d n=%lu err=%lu queue=%lu", ok, n, GetLastError(), qcount());
    FlushConsoleInputBuffer(gin());
}
GC(rci_w_write_input_multi_type_count) {
    GREQ(U(ReadConsoleInputW));
    reset_in(0);
    INPUT_RECORD w[8], o[8]; DWORD n = 0;
    for (int i = 0; i < 8; i++) w[i] = keyrec(L'0' + i, '0' + i, TRUE, 0, 1);
    inj(w, 8);
    GCHK(U(ReadConsoleInputW)(gin(), o, 8, &n));
    GEQ(n, 8);
    for (int i = 0; i < 8; i++) GEQ(o[i].Event.KeyEvent.wVirtualKeyCode, '0' + i);
}

/* ---------- ReadConsoleInputA ---------- */

GC(rci_a_ascii_matches_wide) {
    GREQ(U(ReadConsoleInputA));
    reset_in(0);
    static const char txt[] = "Az09 ~!";
    for (size_t i = 0; txt[i]; i++) {
        INPUT_RECORD w = keyrec((WCHAR)txt[i], vk_of((WCHAR)txt[i]), TRUE, 0, 1), o; DWORD n = 0;
        inj(&w, 1);
        GCHK(U(ReadConsoleInputA)(gin(), &o, 1, &n));
        GEQ(n, 1); GEQ(o.EventType, KEY_EVENT);
        GEQ((unsigned char)o.Event.KeyEvent.uChar.AsciiChar, (unsigned char)txt[i]);
        GEQ(o.Event.KeyEvent.wVirtualKeyCode, vk_of((WCHAR)txt[i]));
    }
}
GC(rci_a_high_char_cp1252) {
    GREQ(U(ReadConsoleInputA));
    UINT cp = GetConsoleCP();
    if (!SetConsoleCP(1252)) { g10_cnote("SetConsoleCP(1252) failed %lu", GetLastError()); return; }
    reset_in(0);
    INPUT_RECORD w = keyrec(0x00E9, 0, TRUE, 0, 1), o; DWORD n = 0;
    inj(&w, 1);
    GCHK(U(ReadConsoleInputA)(gin(), &o, 1, &n));
    GEQ(n, 1);
    GEQ((unsigned char)o.Event.KeyEvent.uChar.AsciiChar, 0xE9);
    w = keyrec(0x20AC, 0, TRUE, 0, 1);
    inj(&w, 1);
    GCHK(U(ReadConsoleInputA)(gin(), &o, 1, &n));
    GEQ((unsigned char)o.Event.KeyEvent.uChar.AsciiChar, 0x80);
    SetConsoleCP(cp);
}
GC(rci_a_high_char_cp437) {
    GREQ(U(ReadConsoleInputA));
    UINT cp = GetConsoleCP();
    if (!SetConsoleCP(437)) { g10_cnote("SetConsoleCP(437) failed %lu", GetLastError()); return; }
    reset_in(0);
    INPUT_RECORD w = keyrec(0x00E9, 0, TRUE, 0, 1), o; DWORD n = 0;
    inj(&w, 1);
    GCHK(U(ReadConsoleInputA)(gin(), &o, 1, &n));
    GEQ((unsigned char)o.Event.KeyEvent.uChar.AsciiChar, 0x82);
    SetConsoleCP(cp);
}
GC(rci_a_limited_length) {
    GREQ(U(ReadConsoleInputA));
    reset_in(0);
    INPUT_RECORD w[3], o[3]; DWORD n = 0;
    for (int i = 0; i < 3; i++) w[i] = keyrec(L'a' + i, 'A' + i, TRUE, 0, 1);
    inj(w, 3);
    GCHK(U(ReadConsoleInputA)(gin(), o, 1, &n)); GEQ(n, 1);
    GEQ(qcount(), 2);
    GCHK(U(ReadConsoleInputA)(gin(), o, 3, &n)); GEQ(n, 2);
    GEQ(o[1].Event.KeyEvent.uChar.AsciiChar, 'c');
}
GC(rci_a_blocks_until_input) {
    GREQ(U(ReadConsoleInputA));
    reset_in(0);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    DWORD WINAPI th(LPVOID p) {
        Sleep(250);
        INPUT_RECORD w = keyrec(L'v', 'V', TRUE, 0, 1);
        inj(&w, 1);
        SetEvent((HANDLE)p);
        return 0;
    }
    HANDLE t = CreateThread(NULL, 0, th, ev, 0, NULL);
    INPUT_RECORD o; DWORD n = 0;
    DWORD t0 = GetTickCount();
    BOOL ok = U(ReadConsoleInputA)(gin(), &o, 1, &n);
    DWORD dt = GetTickCount() - t0;
    GCHK(ok); GEQ(n, 1); GCHK(dt >= 150);
    GEQ(o.Event.KeyEvent.uChar.AsciiChar, 'v');
    WaitForSingleObject(t, 5000); CloseHandle(t); CloseHandle(ev);
}
GC(rci_a_invalid_handles) {
    GREQ(U(ReadConsoleInputA));
    INPUT_RECORD o[2]; DWORD n = 0xAA;
    HANDLE cl = CreateEventA(NULL, FALSE, FALSE, NULL); CloseHandle(cl);
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x1234, cl };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) GFAIL(U(ReadConsoleInputA)(bad[i], o, 2, &n), ERROR_INVALID_HANDLE);
}
GC(rci_a_nonkey_events) {
    GREQ(U(ReadConsoleInputA));
    reset_in(0);
    INPUT_RECORD w, o; DWORD n = 0;
    memset(&w, 0, sizeof w);
    w.EventType = WINDOW_BUFFER_SIZE_EVENT; w.Event.WindowBufferSizeEvent.dwSize = gco(11, 12);
    inj(&w, 1);
    if (qcount()) {
        GCHK(U(ReadConsoleInputA)(gin(), &o, 1, &n)); GEQ(n, 1);
        GEQ(o.EventType, WINDOW_BUFFER_SIZE_EVENT); GEQ(o.Event.WindowBufferSizeEvent.dwSize.X, 11);
    }
}

/* ---------- ReadConsoleW / ReadConsoleA ---------- */

GC(rc_w_cooked_line) {
    GREQ(U(ReadConsoleW));
    reset_in(COOKED);
    WCHAR b[64] = {0}; DWORD n = 0xAA;
    typestr(L"abc\r");
    SetLastError(0xBAD);
    GCHK(U(ReadConsoleW)(gin(), b, 64, &n, NULL));
    GEQ(n, 5); GWSTR(b, L"abc\r\n");
}
GC(rc_a_cooked_line) {
    GREQ(U(ReadConsoleA));
    reset_in(COOKED);
    char b[64] = {0}; DWORD n = 0xAA;
    typestr(L"abc\r");
    GCHK(U(ReadConsoleA)(gin(), b, 64, &n, NULL));
    GEQ(n, 5); GCHK(!memcmp(b, "abc\r\n", 5));
}
GC(rc_w_cooked_empty_line) {
    GREQ(U(ReadConsoleW));
    reset_in(COOKED);
    WCHAR b[16] = {0}; DWORD n = 0xAA;
    typestr(L"\r");
    GCHK(U(ReadConsoleW)(gin(), b, 16, &n, NULL));
    GEQ(n, 2); GWSTR(b, L"\r\n");
}
GC(rc_w_cooked_backspace) {
    GREQ(U(ReadConsoleW));
    reset_in(COOKED);
    WCHAR b[32] = {0}; DWORD n = 0xAA;
    typestr(L"ab\x08" L"c\r");
    GCHK(U(ReadConsoleW)(gin(), b, 32, &n, NULL));
    GEQ(n, 4); GWSTR(b, L"ac\r\n");
}
GC(rc_w_cooked_two_lines) {
    GREQ(U(ReadConsoleW));
    reset_in(COOKED);
    WCHAR b[32]; DWORD n = 0xAA;
    typestr(L"one\rtwo\r");
    memset(b, 0, sizeof b);
    GCHK(U(ReadConsoleW)(gin(), b, 32, &n, NULL)); GEQ(n, 5); GWSTR(b, L"one\r\n");
    memset(b, 0, sizeof b);
    GCHK(U(ReadConsoleW)(gin(), b, 32, &n, NULL)); GEQ(n, 5); GWSTR(b, L"two\r\n");
}
GC(rc_w_cooked_short_buffer_keeps_rest) {
    GREQ(U(ReadConsoleW));
    reset_in(COOKED);
    WCHAR b[32] = {0}; DWORD n = 0xAA;
    typestr(L"hello\r");
    GCHK(U(ReadConsoleW)(gin(), b, 3, &n, NULL));
    GEQ(n, 3); GCHK(!wcsncmp(b, L"hel", 3));
    memset(b, 0, sizeof b);
    GCHK(U(ReadConsoleW)(gin(), b, 32, &n, NULL));
    GEQ(n, 4); GWSTR(b, L"lo\r\n");
}
GC(rc_w_cooked_buffer_exact_line) {
    GREQ(U(ReadConsoleW));
    reset_in(COOKED);
    WCHAR b[32] = {0}; DWORD n = 0xAA;
    typestr(L"hello\r");
    GCHK(U(ReadConsoleW)(gin(), b, 7, &n, NULL));
    GEQ(n, 7); GCHK(!wcsncmp(b, L"hello\r\n", 7));
}
GC(rc_w_cooked_one_char_at_a_time) {
    GREQ(U(ReadConsoleW));
    reset_in(COOKED);
    WCHAR all[16] = {0}, b[4]; DWORD n = 0xAA; int k = 0;
    typestr(L"hi\r");
    for (int i = 0; i < 4 && k < 8; i++) {
        n = 0xAA; b[0] = 0;
        GCHK(U(ReadConsoleW)(gin(), b, 1, &n, NULL));
        GEQ(n, 1);
        if (n == 1) all[k++] = b[0];
    }
    all[k] = 0;
    GWSTR(all, L"hi\r\n");
}
GC(rc_a_cooked_short_buffer_keeps_rest) {
    GREQ(U(ReadConsoleA));
    reset_in(COOKED);
    char b[32] = {0}; DWORD n = 0xAA;
    typestr(L"world\r");
    GCHK(U(ReadConsoleA)(gin(), b, 2, &n, NULL));
    GEQ(n, 2); GCHK(!memcmp(b, "wo", 2));
    memset(b, 0, sizeof b);
    GCHK(U(ReadConsoleA)(gin(), b, 32, &n, NULL));
    GEQ(n, 5); GCHK(!memcmp(b, "rld\r\n", 5));
}
GC(rc_w_line_input_without_echo) {
    GREQ(U(ReadConsoleW));
    reset_in(ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);
    WCHAR b[32] = {0}; DWORD n = 0xAA;
    typestr(L"xy\r");
    GCHK(U(ReadConsoleW)(gin(), b, 32, &n, NULL));
    GEQ(n, 4); GWSTR(b, L"xy\r\n");
}
GC(rc_w_raw_chars) {
    GREQ(U(ReadConsoleW));
    reset_in(0);
    WCHAR b[16] = {0}; DWORD n = 0xAA;
    typestr(L"abc");
    GCHK(U(ReadConsoleW)(gin(), b, 16, &n, NULL));
    GEQ(n, 3); GWSTR(b, L"abc");
}
GC(rc_w_raw_enter_is_cr_only) {
    GREQ(U(ReadConsoleW));
    reset_in(0);
    WCHAR b[16] = {0}; DWORD n = 0xAA;
    typestr(L"a\r");
    GCHK(U(ReadConsoleW)(gin(), b, 16, &n, NULL));
    GEQ(n, 2); GEQ(b[0], L'a'); GEQ(b[1], L'\r');
}
GC(rc_w_raw_partial_reads) {
    GREQ(U(ReadConsoleW));
    reset_in(0);
    WCHAR b[16] = {0}; DWORD n = 0xAA;
    typestr(L"abcd");
    GCHK(U(ReadConsoleW)(gin(), b, 2, &n, NULL)); GEQ(n, 2); GCHK(!wcsncmp(b, L"ab", 2));
    GCHK(U(ReadConsoleW)(gin(), b, 2, &n, NULL)); GEQ(n, 2); GCHK(!wcsncmp(b, L"cd", 2));
}
GC(rc_a_raw_chars) {
    GREQ(U(ReadConsoleA));
    reset_in(0);
    char b[16] = {0}; DWORD n = 0xAA;
    typestr(L"xyz");
    GCHK(U(ReadConsoleA)(gin(), b, 16, &n, NULL));
    GEQ(n, 3); GSTR(b, "xyz");
}
GC(rc_w_raw_ignores_key_up) {
    GREQ(U(ReadConsoleW));
    reset_in(0);
    INPUT_RECORD w[4]; WCHAR b[8] = {0}; DWORD n = 0xAA;
    w[0] = keyrec(L'p', 'P', FALSE, 0, 1);
    w[1] = keyrec(L'q', 'Q', FALSE, 0, 1);
    w[2] = keyrec(L'x', 'X', TRUE, 0, 1);
    w[3] = keyrec(L'x', 'X', FALSE, 0, 1);
    inj(w, 4);
    GCHK(U(ReadConsoleW)(gin(), b, 8, &n, NULL));
    GEQ(n, 1); GEQ(b[0], L'x');
}
GC(rc_w_raw_ignores_nonchar_keys) {
    GREQ(U(ReadConsoleW));
    reset_in(0);
    INPUT_RECORD w[3]; WCHAR b[8] = {0}; DWORD n = 0xAA;
    w[0] = keyrec(0, VK_SHIFT, TRUE, SHIFT_PRESSED, 1);
    w[1] = keyrec(0, VK_F5, TRUE, 0, 1);
    w[2] = keyrec(L'm', 'M', TRUE, 0, 1);
    inj(w, 3);
    GCHK(U(ReadConsoleW)(gin(), b, 8, &n, NULL));
    GEQ(n, 1); GEQ(b[0], L'm');
}
GC(rc_w_raw_repeat_count) {
    GREQ(U(ReadConsoleW));
    reset_in(0);
    INPUT_RECORD w = keyrec(L'a', 'A', TRUE, 0, 3); WCHAR b[8] = {0}; DWORD n = 0xAA;
    inj(&w, 1);
    GCHK(U(ReadConsoleW)(gin(), b, 8, &n, NULL));
    g10_cnote("repeat 3 -> %lu chars", n);
    GCHK(n >= 1 && n <= 3);
}
GC(rc_w_raw_unicode) {
    GREQ(U(ReadConsoleW));
    reset_in(0);
    INPUT_RECORD w[3]; WCHAR b[8] = {0}; DWORD n = 0xAA;
    w[0] = keyrec(0x20AC, 0, TRUE, 0, 1);
    w[1] = keyrec(0x4E2D, 0, TRUE, 0, 1);
    w[2] = keyrec(0x00E9, 0, TRUE, 0, 1);
    inj(w, 3);
    GCHK(U(ReadConsoleW)(gin(), b, 8, &n, NULL));
    GEQ(n, 3); GEQ(b[0], 0x20AC); GEQ(b[1], 0x4E2D); GEQ(b[2], 0x00E9);
}
GC(rc_w_raw_surrogate_pair) {
    GREQ(U(ReadConsoleW));
    reset_in(0);
    INPUT_RECORD w[2]; WCHAR b[8] = {0}; DWORD n = 0xAA;
    w[0] = keyrec(0xD83D, 0, TRUE, 0, 1);
    w[1] = keyrec(0xDE00, 0, TRUE, 0, 1);
    inj(w, 2);
    GCHK(U(ReadConsoleW)(gin(), b, 8, &n, NULL));
    g10_cnote("surrogate pair read n=%lu %04x %04x", n, b[0], b[1]);
    GCHK(n >= 1);
}
GC(rc_a_raw_cp1252) {
    GREQ(U(ReadConsoleA));
    UINT cp = GetConsoleCP();
    if (!SetConsoleCP(1252)) { g10_cnote("SetConsoleCP(1252) failed"); return; }
    reset_in(0);
    INPUT_RECORD w = keyrec(0x00E9, 0, TRUE, 0, 1); char b[8] = {0}; DWORD n = 0xAA;
    inj(&w, 1);
    GCHK(U(ReadConsoleA)(gin(), b, 8, &n, NULL));
    GEQ(n, 1); GEQ((unsigned char)b[0], 0xE9);
    SetConsoleCP(cp);
}
GC(rc_a_raw_utf8_cp) {
    GREQ(U(ReadConsoleA));
    UINT cp = GetConsoleCP();
    if (!SetConsoleCP(65001)) { g10_cnote("SetConsoleCP(65001) failed %lu", GetLastError()); return; }
    reset_in(0);
    INPUT_RECORD w = keyrec(0x00E9, 0, TRUE, 0, 1); unsigned char b[8] = {0}; DWORD n = 0xAA;
    inj(&w, 1);
    BOOL ok = U(ReadConsoleA)(gin(), b, 8, &n, NULL);
    g10_cnote("utf8 cp: ok=%d n=%lu bytes %02x %02x", ok, n, b[0], b[1]);
    if (ok && n == 2) { GEQ(b[0], 0xC3); GEQ(b[1], 0xA9); }
    SetConsoleCP(cp);
}
GC(rc_w_blocks_until_input) {
    GREQ(U(ReadConsoleW));
    reset_in(0);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    DWORD WINAPI th(LPVOID p) {
        Sleep(250);
        typestr(L"t");
        SetEvent((HANDLE)p);
        return 0;
    }
    HANDLE t = CreateThread(NULL, 0, th, ev, 0, NULL);
    WCHAR b[4] = {0}; DWORD n = 0;
    DWORD t0 = GetTickCount();
    BOOL ok = U(ReadConsoleW)(gin(), b, 4, &n, NULL);
    DWORD dt = GetTickCount() - t0;
    GCHK(ok); GEQ(n, 1); GEQ(b[0], L't'); GCHK(dt >= 150);
    WaitForSingleObject(t, 5000); CloseHandle(t); CloseHandle(ev);
}
GC(rc_w_wakeup_mask_tab) {
    GREQ(U(ReadConsoleW));
    reset_in(COOKED);
    CONSOLE_READCONSOLE_CONTROL ctl; WCHAR b[32] = {0}; DWORD n = 0xAA;
    memset(&ctl, 0, sizeof ctl);
    ctl.nLength = sizeof ctl; ctl.nInitialChars = 0; ctl.dwCtrlWakeupMask = 1 << 9;
    typestr(L"ab\tcd\r");
    SetLastError(0xBAD);
    BOOL ok = U(ReadConsoleW)(gin(), b, 32, &n, &ctl);
    GCHK(ok);
    g10_cnote("wakeup mask tab: n=%lu b[2]=0x%x ctlstate=0x%lx", n, b[2], ctl.dwControlKeyState);
    GEQ(n, 3);
    GEQ(b[0], L'a'); GEQ(b[1], L'b'); GEQ(b[2], L'\t');
}
GC(rc_w_control_zero_mask) {
    GREQ(U(ReadConsoleW));
    reset_in(COOKED);
    CONSOLE_READCONSOLE_CONTROL ctl; WCHAR b[32] = {0}; DWORD n = 0xAA;
    memset(&ctl, 0, sizeof ctl);
    ctl.nLength = sizeof ctl;
    typestr(L"ok\r");
    GCHK(U(ReadConsoleW)(gin(), b, 32, &n, &ctl));
    GEQ(n, 4); GWSTR(b, L"ok\r\n");
}
GC(rc_w_control_initial_chars) {
    GREQ(U(ReadConsoleW));
    reset_in(COOKED);
    CONSOLE_READCONSOLE_CONTROL ctl; WCHAR b[32]; DWORD n = 0xAA;
    memset(&ctl, 0, sizeof ctl);
    ctl.nLength = sizeof ctl; ctl.nInitialChars = 2;
    for (int i = 0; i < 32; i++) b[i] = 0;
    b[0] = L'p'; b[1] = L'q';
    typestr(L"r\r");
    GCHK(U(ReadConsoleW)(gin(), b, 32, &n, &ctl));
    g10_cnote("initial chars: n=%lu buf=%ls", n, b);
    GEQ(n, 5); GWSTR(b, L"pqr\r\n");
}
GC(rc_w_control_bad_length) {
    GREQ(U(ReadConsoleW));
    reset_in(COOKED);
    CONSOLE_READCONSOLE_CONTROL ctl; WCHAR b[8] = {0}; DWORD n = 0xAA;
    memset(&ctl, 0, sizeof ctl);
    ctl.nLength = sizeof ctl - 4;
    typestr(L"a\r");
    SetLastError(0xBAD);
    BOOL ok = U(ReadConsoleW)(gin(), b, 8, &n, &ctl);
    g10_cnote("ReadConsoleW(bad control length): ok=%d err=%lu n=%lu", ok, GetLastError(), n);
    FlushConsoleInputBuffer(gin());
}
GC(rc_w_invalid_handles) {
    GREQ(U(ReadConsoleW));
    WCHAR b[8]; DWORD n = 0xAA;
    HANDLE ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    HANDLE cl = CreateEventA(NULL, FALSE, FALSE, NULL); CloseHandle(cl);
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x1234, ev, cl };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) GFAIL(U(ReadConsoleW)(bad[i], b, 8, &n, NULL), ERROR_INVALID_HANDLE);
    CloseHandle(ev);
}
GC(rc_a_invalid_handles) {
    GREQ(U(ReadConsoleA));
    char b[8]; DWORD n = 0xAA;
    HANDLE ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    HANDLE cl = CreateEventA(NULL, FALSE, FALSE, NULL); CloseHandle(cl);
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x1234, ev, cl };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) GFAIL(U(ReadConsoleA)(bad[i], b, 8, &n, NULL), ERROR_INVALID_HANDLE);
    CloseHandle(ev);
}
GC(rc_w_output_handle_rejected) {
    GREQ(U(ReadConsoleW));
    reset_in(0);
    WCHAR b[8]; DWORD n = 0xAA;
    SetLastError(0xBAD);
    BOOL ok = U(ReadConsoleW)(gout(), b, 8, &n, NULL);
    GEQ(ok, 0);
    g10_cnote("ReadConsoleW(output handle) err=%lu", GetLastError());
}
GC(rc_w_zero_chars_requested) {
    GREQ(U(ReadConsoleW));
    reset_in(0);
    typestr(L"a");
    WCHAR b[4] = {0}; DWORD n = 0xAA;
    SetLastError(0xBAD);
    BOOL ok = U(ReadConsoleW)(gin(), b, 0, &n, NULL);
    g10_cnote("ReadConsoleW(0 chars) ok=%d n=%lu err=%lu queue=%lu", ok, n, GetLastError(), qcount());
    FlushConsoleInputBuffer(gin());
}
GC(rc_w_file_handle_reads_file) {
    /* a non-console handle is not accepted by ReadConsole */
    GREQ(U(ReadConsoleW));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "g10_rc.bin");
    HANDLE f = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    GNE(f, INVALID_HANDLE_VALUE);
    DWORD w; WriteFile(f, "abcd", 4, &w, NULL); SetFilePointer(f, 0, NULL, FILE_BEGIN);
    char b[8] = {0}; DWORD n = 0xAA;
    SetLastError(0xBAD);
    BOOL ok = U(ReadConsoleA)(f, b, 4, &n, NULL);
    GEQ(ok, 0);
    g10_cnote("ReadConsoleA(file) err=%lu", GetLastError());
    CloseHandle(f);
}
GC(rc_w_cooked_utf16_char_roundtrip) {
    GREQ(U(ReadConsoleW));
    reset_in(COOKED);
    INPUT_RECORD w[4]; WCHAR b[16] = {0}; DWORD n = 0xAA;
    w[0] = keyrec(0x00E9, 0, TRUE, 0, 1);
    w[1] = keyrec(0x00E9, 0, FALSE, 0, 1);
    w[2] = keyrec(L'\r', VK_RETURN, TRUE, 0, 1);
    w[3] = keyrec(L'\r', VK_RETURN, FALSE, 0, 1);
    inj(w, 4);
    GCHK(U(ReadConsoleW)(gin(), b, 16, &n, NULL));
    GEQ(n, 3); GEQ(b[0], 0x00E9); GEQ(b[1], L'\r'); GEQ(b[2], L'\n');
}
