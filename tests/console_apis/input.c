/* console_apis: console input buffer, INPUT_RECORD, ReadConsole A/W (cooked and raw) */
#include "ch.h"

#define FAILS(call, err)  do { SetLastError(0xBAD); BOOL r_ = (call); DWORD le_ = GetLastError(); CEQ(r_, 0); CERRV(le_, err); } while (0)

UDECL(BOOL, ReadConsoleInputExW, (HANDLE, PINPUT_RECORD, DWORD, LPDWORD, USHORT));
UDECL(BOOL, ReadConsoleInputExA, (HANDLE, PINPUT_RECORD, DWORD, LPDWORD, USHORT));

#ifndef CONSOLE_READ_NOREMOVE
#define CONSOLE_READ_NOREMOVE 0x0001
#define CONSOLE_READ_NOWAIT   0x0002
#endif

static void cooked(void) {
    SetConsoleMode(hin_(), ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
    inp_reset();
}
static void rawmode(DWORD extra) {
    SetConsoleMode(hin_(), extra);
    inp_reset();
}
static INPUT_RECORD mkkey(WCHAR c, BOOL down) { return keyrec(c, vk_of(c), down, 0); }

static DWORD WINAPI late_typer(LPVOID p) {
    Sleep((DWORD)(ULONG_PTR)p);
    typestr(hin_(), L"late\r");
    return 0;
}
static DWORD WINAPI late_event(LPVOID p) {
    Sleep((DWORD)(ULONG_PTR)p);
    INPUT_RECORD r = mkkey(L'q', TRUE); DWORD w;
    WriteConsoleInputW(hin_(), &r, 1, &w);
    return 0;
}

/* ---------------- layout ---------------- */

CT(con_input_record_layout) {
    CEQ(sizeof(INPUT_RECORD), 20);
    CEQ(offsetof(INPUT_RECORD, Event), 4);
    CEQ(sizeof(KEY_EVENT_RECORD), 16);
    CEQ(sizeof(MOUSE_EVENT_RECORD), 16);
    CEQ(sizeof(WINDOW_BUFFER_SIZE_RECORD), 4);
    CEQ(sizeof(MENU_EVENT_RECORD), 4);
    CEQ(sizeof(FOCUS_EVENT_RECORD), 4);
    CEQ(sizeof(CHAR_INFO), 4);
    CEQ(offsetof(CHAR_INFO, Attributes), 2);
    CEQ(sizeof(CONSOLE_SCREEN_BUFFER_INFO), 22);
    CEQ(sizeof(CONSOLE_SCREEN_BUFFER_INFOEX), 96);
    CEQ(sizeof(CONSOLE_FONT_INFOEX), 84);
    CEQ(sizeof(CONSOLE_CURSOR_INFO), 8);
    CEQ(sizeof(CONSOLE_HISTORY_INFO), 16);
    CEQ(sizeof(CONSOLE_SELECTION_INFO), 16);
    CEQ(sizeof(CONSOLE_READCONSOLE_CONTROL), 16);
    CEQ(KEY_EVENT, 1); CEQ(MOUSE_EVENT, 2); CEQ(WINDOW_BUFFER_SIZE_EVENT, 4); CEQ(MENU_EVENT, 8); CEQ(FOCUS_EVENT, 16);
}

/* ---------------- GetNumberOfConsoleInputEvents / Flush ---------------- */

CT(con_input_count_empty_after_flush) {
    CREQ(GetNumberOfConsoleInputEvents); CREQ(FlushConsoleInputBuffer);
    inp_reset();
    DWORD n = 0xFFFF;
    CC(GetNumberOfConsoleInputEvents(hin_(), &n));
    CEQ(n, 0);
}

CT(con_input_count_after_write) {
    CREQ(GetNumberOfConsoleInputEvents); CREQ(WriteConsoleInputW);
    inp_reset();
    INPUT_RECORD r[3] = { mkkey(L'a', TRUE), mkkey(L'a', FALSE), mkkey(L'b', TRUE) };
    DWORD w = 0, n = 0;
    CC(WriteConsoleInputW(hin_(), r, 3, &w)); CEQ(w, 3);
    CC(GetNumberOfConsoleInputEvents(hin_(), &n)); CEQ(n, 3);
    CC(GetNumberOfConsoleInputEvents(hin_(), &n)); CEQ(n, 3);
}

CT(con_input_count_invalid_handles) {
    CREQ(GetNumberOfConsoleInputEvents);
    DWORD n = 0xAA;
    HANDLE ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x1234, ev, hout_() };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) FAILS(GetNumberOfConsoleInputEvents(bad[i], &n), ERROR_INVALID_HANDLE);
    CEQ(n, 0xAA);
    CloseHandle(ev);
}

CT(con_input_flush_clears) {
    CREQ(FlushConsoleInputBuffer);
    inp_reset();
    typestr(hin_(), L"abcdef");
    CEQ(inp_count(), 12);
    SetLastError(0xBAD);
    CC(FlushConsoleInputBuffer(hin_()));
    CEQ(inp_count(), 0);
    CC(FlushConsoleInputBuffer(hin_()));
    CEQ(inp_count(), 0);
}

CT(con_input_flush_invalid_handles) {
    CREQ(FlushConsoleInputBuffer);
    HANDLE ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    HANDLE b = mkbuf_raw();
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x1234, ev, hout_(), b };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) FAILS(FlushConsoleInputBuffer(bad[i]), ERROR_INVALID_HANDLE);
    CloseHandle(ev); if (b) CloseHandle(b);
}

CT(con_input_flushfilebuffers_on_console_input) {
    CREQ(FlushConsoleInputBuffer);
    inp_reset();
    typestr(hin_(), L"xy");
    SetLastError(0xBAD);
    BOOL r = FlushFileBuffers(hin_());
    cnote("FlushFileBuffers(console input) -> %d err=%lu pending=%lu", r, GetLastError(), inp_count());
}

/* ---------------- WriteConsoleInput ---------------- */

CT(con_input_write_returns_written) {
    CREQ(WriteConsoleInputW); CREQ(WriteConsoleInputA);
    inp_reset();
    INPUT_RECORD r[4]; DWORD w = 0xFFFF;
    for (int i = 0; i < 4; i++) r[i] = mkkey(L'a' + i, (i & 1) == 0);
    CC(WriteConsoleInputW(hin_(), r, 4, &w)); CEQ(w, 4);
    w = 0xFFFF;
    CC(WriteConsoleInputA(hin_(), r, 4, &w)); CEQ(w, 4);
    CEQ(inp_count(), 8);
}

CT(con_input_write_zero_records) {
    CREQ(WriteConsoleInputW);
    inp_reset();
    INPUT_RECORD r = mkkey(L'a', TRUE); DWORD w = 0xFFFF;
    SetLastError(0xBAD);
    BOOL ok = WriteConsoleInputW(hin_(), &r, 0, &w);
    cnote("WriteConsoleInputW(len 0) -> %d err=%lu written=%lu", ok, GetLastError(), w);
    CEQ(inp_count(), 0);
}

CT(con_input_write_invalid_handles) {
    CREQ(WriteConsoleInputW); CREQ(WriteConsoleInputA);
    INPUT_RECORD r = mkkey(L'a', TRUE); DWORD w = 0xAA;
    HANDLE ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x1234, ev, hout_() };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        FAILS(WriteConsoleInputW(bad[i], &r, 1, &w), ERROR_INVALID_HANDLE);
        FAILS(WriteConsoleInputA(bad[i], &r, 1, &w), ERROR_INVALID_HANDLE);
    }
    CloseHandle(ev);
}

CT(con_input_write_requires_write_access) {
    CREQ(WriteConsoleInputW);
    HANDLE ro = CreateFileW(L"CONIN$", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CNE(ro, INVALID_HANDLE_VALUE);
    INPUT_RECORD r = mkkey(L'a', TRUE); DWORD w = 0;
    inp_reset();
    SetLastError(0xBAD);
    BOOL ok = WriteConsoleInputW(ro, &r, 1, &w);
    cnote("WriteConsoleInputW on read-only CONIN$ -> %d err=%lu", ok, GetLastError());
    CEQ(ok, 0);
    CEQ(inp_count(), 0);
    CloseHandle(ro);
}

CT(con_input_read_requires_read_access) {
    CREQ(ReadConsoleInputW); CREQ(PeekConsoleInputW); CREQ(GetNumberOfConsoleInputEvents);
    HANDLE wo = CreateFileW(L"CONIN$", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CNE(wo, INVALID_HANDLE_VALUE);
    inp_reset();
    INPUT_RECORD r = mkkey(L'a', TRUE); DWORD n = 0;
    CC(WriteConsoleInputW(wo, &r, 1, &n)); CEQ(n, 1);
    SetLastError(0xBAD);
    BOOL ok = PeekConsoleInputW(wo, &r, 1, &n);
    cnote("PeekConsoleInputW on write-only CONIN$ -> %d err=%lu", ok, GetLastError());
    CEQ(ok, 0);
    SetLastError(0xBAD);
    ok = GetNumberOfConsoleInputEvents(wo, &n);
    cnote("GetNumberOfConsoleInputEvents on write-only CONIN$ -> %d err=%lu", ok, GetLastError());
    CloseHandle(wo);
}

CT(con_input_write_via_conin_read_via_std) {
    CREQ(WriteConsoleInputW); CREQ(ReadConsoleInputW);
    HANDLE ci = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CNE(ci, INVALID_HANDLE_VALUE);
    inp_reset();
    INPUT_RECORD r = mkkey(L'z', TRUE), o; DWORD n = 0;
    CC(WriteConsoleInputW(ci, &r, 1, &n)); CEQ(n, 1);
    CEQ(inp_count(), 1);
    CC(ReadConsoleInputW(hin_(), &o, 1, &n)); CEQ(n, 1);
    CEQ(o.EventType, KEY_EVENT);
    CEQ(o.Event.KeyEvent.uChar.UnicodeChar, 'z');
    CloseHandle(ci);
}

/* ---------------- read / peek ---------------- */

CT(con_input_key_record_roundtrip) {
    CREQ(WriteConsoleInputW); CREQ(ReadConsoleInputW);
    inp_reset();
    INPUT_RECORD r, o; DWORD n = 0;
    memset(&r, 0, sizeof r);
    r.EventType = KEY_EVENT;
    r.Event.KeyEvent.bKeyDown = TRUE;
    r.Event.KeyEvent.wRepeatCount = 1;
    r.Event.KeyEvent.wVirtualKeyCode = 'A';
    r.Event.KeyEvent.wVirtualScanCode = 0x1E;
    r.Event.KeyEvent.uChar.UnicodeChar = L'A';
    r.Event.KeyEvent.dwControlKeyState = SHIFT_PRESSED | CAPSLOCK_ON;
    CC(WriteConsoleInputW(hin_(), &r, 1, &n)); CEQ(n, 1);
    CC(ReadConsoleInputW(hin_(), &o, 1, &n)); CEQ(n, 1);
    CEQ(o.EventType, KEY_EVENT);
    CEQ(o.Event.KeyEvent.bKeyDown, TRUE);
    CEQ(o.Event.KeyEvent.wRepeatCount, 1);
    CEQ(o.Event.KeyEvent.wVirtualKeyCode, 'A');
    CEQ(o.Event.KeyEvent.wVirtualScanCode, 0x1E);
    CEQ(o.Event.KeyEvent.uChar.UnicodeChar, L'A');
    CEQ(o.Event.KeyEvent.dwControlKeyState, SHIFT_PRESSED | CAPSLOCK_ON);
}

CT(con_input_key_up_roundtrip) {
    CREQ(WriteConsoleInputW); CREQ(ReadConsoleInputW);
    inp_reset();
    INPUT_RECORD r = keyrec(L'k', 'K', FALSE, LEFT_CTRL_PRESSED | ENHANCED_KEY), o; DWORD n = 0;
    CC(WriteConsoleInputW(hin_(), &r, 1, &n));
    CC(ReadConsoleInputW(hin_(), &o, 1, &n)); CEQ(n, 1);
    CEQ(o.Event.KeyEvent.bKeyDown, FALSE);
    CEQ(o.Event.KeyEvent.dwControlKeyState, LEFT_CTRL_PRESSED | ENHANCED_KEY);
    CEQ(o.Event.KeyEvent.uChar.UnicodeChar, L'k');
}

CT(con_input_all_control_key_flags_roundtrip) {
    CREQ(WriteConsoleInputW); CREQ(ReadConsoleInputW);
    inp_reset();
    DWORD flags[] = { RIGHT_ALT_PRESSED, LEFT_ALT_PRESSED, RIGHT_CTRL_PRESSED, LEFT_CTRL_PRESSED, SHIFT_PRESSED, NUMLOCK_ON, SCROLLLOCK_ON, CAPSLOCK_ON, ENHANCED_KEY,
        RIGHT_ALT_PRESSED | LEFT_CTRL_PRESSED | SHIFT_PRESSED | NUMLOCK_ON, 0 };
    for (size_t i = 0; i < sizeof flags / sizeof flags[0]; i++) {
        INPUT_RECORD r = keyrec(L'x', 'X', TRUE, flags[i]), o; DWORD n = 0;
        CC(WriteConsoleInputW(hin_(), &r, 1, &n));
        CC(ReadConsoleInputW(hin_(), &o, 1, &n)); CEQ(n, 1);
        CCF(o.Event.KeyEvent.dwControlKeyState == flags[i], "flags 0x%lx got 0x%lx", flags[i], o.Event.KeyEvent.dwControlKeyState);
    }
}

CT(con_input_repeat_count_roundtrip) {
    CREQ(WriteConsoleInputW); CREQ(ReadConsoleInputW);
    inp_reset();
    WORD counts[] = { 1, 2, 5, 100, 0xFFFF };
    for (size_t i = 0; i < sizeof counts / sizeof counts[0]; i++) {
        INPUT_RECORD r = mkkey(L'r', TRUE), o; DWORD n = 0;
        r.Event.KeyEvent.wRepeatCount = counts[i];
        CC(WriteConsoleInputW(hin_(), &r, 1, &n));
        CC(ReadConsoleInputW(hin_(), &o, 1, &n)); CEQ(n, 1);
        CCF(o.Event.KeyEvent.wRepeatCount == counts[i], "repeat %u got %u", counts[i], o.Event.KeyEvent.wRepeatCount);
    }
}

CT(con_input_repeat_count_zero) {
    CREQ(WriteConsoleInputW); CREQ(ReadConsoleInputW);
    inp_reset();
    INPUT_RECORD r = mkkey(L'r', TRUE), o; DWORD n = 0;
    r.Event.KeyEvent.wRepeatCount = 0;
    BOOL ok = WriteConsoleInputW(hin_(), &r, 1, &n);
    cnote("repeat count 0 write -> %d n=%lu pending=%lu", ok, n, inp_count());
    if (inp_count()) { ReadConsoleInputW(hin_(), &o, 1, &n); cnote("read back repeat=%u", o.Event.KeyEvent.wRepeatCount); }
}

CT(con_input_read_order_fifo) {
    CREQ(WriteConsoleInputW); CREQ(ReadConsoleInputW);
    inp_reset();
    INPUT_RECORD r[10], o[10]; DWORD n = 0;
    for (int i = 0; i < 10; i++) r[i] = mkkey(L'a' + i, (i & 1) == 0);
    CC(WriteConsoleInputW(hin_(), r, 10, &n)); CEQ(n, 10);
    CC(ReadConsoleInputW(hin_(), o, 10, &n)); CEQ(n, 10);
    for (int i = 0; i < 10; i++) {
        CCF(o[i].Event.KeyEvent.uChar.UnicodeChar == L'a' + i, "idx %d got 0x%x", i, o[i].Event.KeyEvent.uChar.UnicodeChar);
        CCF(o[i].Event.KeyEvent.bKeyDown == ((i & 1) == 0), "idx %d down", i);
    }
    CEQ(inp_count(), 0);
}

CT(con_input_read_partial_buffer) {
    CREQ(WriteConsoleInputW); CREQ(ReadConsoleInputW);
    inp_reset();
    typestr(hin_(), L"abc");   /* 6 events */
    INPUT_RECORD o[16]; DWORD n = 0;
    CC(ReadConsoleInputW(hin_(), o, 4, &n)); CEQ(n, 4);
    CEQ(inp_count(), 2);
    CC(ReadConsoleInputW(hin_(), o, 16, &n)); CEQ(n, 2);   /* returns what is available, does not wait for 16 */
    CEQ(inp_count(), 0);
}

CT(con_input_read_one_at_a_time) {
    CREQ(WriteConsoleInputW); CREQ(ReadConsoleInputW);
    inp_reset();
    typestr(hin_(), L"xyz");
    for (int i = 0; i < 6; i++) {
        INPUT_RECORD o; DWORD n = 0;
        CC(ReadConsoleInputW(hin_(), &o, 1, &n)); CEQ(n, 1);
        CCF(o.Event.KeyEvent.uChar.UnicodeChar == L"xxyyzz"[i], "i=%d", i);
        CEQ(o.Event.KeyEvent.bKeyDown, (i & 1) == 0);
    }
    CEQ(inp_count(), 0);
}

CT(con_input_peek_empty_returns_immediately) {
    CREQ(PeekConsoleInputW); CREQ(PeekConsoleInputA);
    inp_reset();
    INPUT_RECORD o[4]; DWORD n = 0xFFFF;
    DWORD t0 = GetTickCount();
    CC(PeekConsoleInputW(hin_(), o, 4, &n)); CEQ(n, 0);
    n = 0xFFFF;
    CC(PeekConsoleInputA(hin_(), o, 4, &n)); CEQ(n, 0);
    CC(GetTickCount() - t0 < 2000);
}

CT(con_input_peek_does_not_remove) {
    CREQ(PeekConsoleInputW); CREQ(WriteConsoleInputW);
    inp_reset();
    typestr(hin_(), L"hi");
    INPUT_RECORD a[8], b[8]; DWORD n1 = 0, n2 = 0;
    CC(PeekConsoleInputW(hin_(), a, 8, &n1)); CEQ(n1, 4);
    CC(PeekConsoleInputW(hin_(), b, 8, &n2)); CEQ(n2, 4);
    CC(!memcmp(a, b, sizeof(INPUT_RECORD) * 4));
    CEQ(inp_count(), 4);
    CEQ(a[0].Event.KeyEvent.uChar.UnicodeChar, 'h');
    CEQ(a[2].Event.KeyEvent.uChar.UnicodeChar, 'i');
}

CT(con_input_peek_limited_length) {
    CREQ(PeekConsoleInputW);
    inp_reset();
    typestr(hin_(), L"abcd");
    INPUT_RECORD o[8]; DWORD n = 0;
    CC(PeekConsoleInputW(hin_(), o, 3, &n)); CEQ(n, 3);
    CEQ(o[2].Event.KeyEvent.uChar.UnicodeChar, 'b');
    CC(PeekConsoleInputW(hin_(), o, 1, &n)); CEQ(n, 1);
    CC(PeekConsoleInputW(hin_(), o, 8, &n)); CEQ(n, 8);
    CEQ(inp_count(), 8);
}

CT(con_input_peek_invalid_handles) {
    CREQ(PeekConsoleInputW); CREQ(PeekConsoleInputA);
    INPUT_RECORD o[2]; DWORD n = 0xAA;
    HANDLE ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x1234, ev, hout_() };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        FAILS(PeekConsoleInputW(bad[i], o, 2, &n), ERROR_INVALID_HANDLE);
        FAILS(PeekConsoleInputA(bad[i], o, 2, &n), ERROR_INVALID_HANDLE);
    }
    CloseHandle(ev);
}

CT(con_input_read_invalid_handles) {
    CREQ(ReadConsoleInputW); CREQ(ReadConsoleInputA);
    INPUT_RECORD o[2]; DWORD n = 0xAA;
    HANDLE ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x1234, ev, hout_() };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        FAILS(ReadConsoleInputW(bad[i], o, 2, &n), ERROR_INVALID_HANDLE);
        FAILS(ReadConsoleInputA(bad[i], o, 2, &n), ERROR_INVALID_HANDLE);
    }
    CloseHandle(ev);
}

CT(con_input_read_blocks_until_event) {
    CREQ(ReadConsoleInputW);
    inp_reset();
    HANDLE t = CreateThread(NULL, 0, late_event, (LPVOID)(ULONG_PTR)400, 0, NULL);
    INPUT_RECORD o; DWORD n = 0;
    DWORD t0 = GetTickCount();
    CC(ReadConsoleInputW(hin_(), &o, 1, &n));
    DWORD el = GetTickCount() - t0;
    CEQ(n, 1);
    CCF(el >= 250, "returned after only %lu ms", el);
    CEQ(o.Event.KeyEvent.uChar.UnicodeChar, 'q');
    WaitForSingleObject(t, 5000); CloseHandle(t);
}

CT(con_input_wait_handle_signalled_state) {
    CREQ(WriteConsoleInputW);
    inp_reset();
    CEQ(WaitForSingleObject(hin_(), 0), WAIT_TIMEOUT);
    typestr(hin_(), L"a");
    CEQ(WaitForSingleObject(hin_(), 0), WAIT_OBJECT_0);
    CEQ(WaitForSingleObject(hin_(), 0), WAIT_OBJECT_0);   /* manual-reset style: stays signalled */
    FlushConsoleInputBuffer(hin_());
    CEQ(WaitForSingleObject(hin_(), 0), WAIT_TIMEOUT);
}

CT(con_input_wait_handle_wakes_waiter) {
    CREQ(WriteConsoleInputW);
    inp_reset();
    HANDLE t = CreateThread(NULL, 0, late_event, (LPVOID)(ULONG_PTR)300, 0, NULL);
    DWORD t0 = GetTickCount();
    DWORD r = WaitForSingleObject(hin_(), 5000);
    CEQ(r, WAIT_OBJECT_0);
    CC(GetTickCount() - t0 >= 200);
    WaitForSingleObject(t, 5000); CloseHandle(t);
}

CT(con_input_large_batch) {
    CREQ(WriteConsoleInputW); CREQ(ReadConsoleInputW);
    inp_reset();
    static INPUT_RECORD r[500], o[500];
    for (int i = 0; i < 500; i++) r[i] = mkkey(L'a' + (i / 2) % 26, (i & 1) == 0);
    DWORD n = 0;
    CC(WriteConsoleInputW(hin_(), r, 500, &n)); CEQ(n, 500);
    CEQ(inp_count(), 500);
    CC(ReadConsoleInputW(hin_(), o, 500, &n)); CEQ(n, 500);
    CC(!memcmp(r, o, sizeof r));
}

CT(con_input_many_batches) {
    CREQ(WriteConsoleInputW);
    inp_reset();
    for (int i = 0; i < 1000; i++) {
        INPUT_RECORD r = mkkey(L'a' + (i % 26), (i & 1) == 0); DWORD n = 0;
        CC(WriteConsoleInputW(hin_(), &r, 1, &n));
    }
    DWORD c = inp_count();
    CCF(c == 1000, "pending=%lu", c);
    FlushConsoleInputBuffer(hin_());
}

CT(con_input_coalescing_observation) {
    CREQ(WriteConsoleInputW);
    inp_reset();
    INPUT_RECORD r[3]; DWORD n = 0;
    for (int i = 0; i < 3; i++) r[i] = mkkey(L'a', TRUE);
    CC(WriteConsoleInputW(hin_(), r, 3, &n)); CEQ(n, 3);
    cnote("3 identical key-down events written in one call: pending=%lu", inp_count());
    inp_reset();
    for (int i = 0; i < 3; i++) WriteConsoleInputW(hin_(), &r[i], 1, &n);
    cnote("3 identical key-down events written singly: pending=%lu", inp_count());
}

CT(con_input_a_variant_ascii) {
    CREQ(WriteConsoleInputA); CREQ(ReadConsoleInputW); CREQ(ReadConsoleInputA); CREQ(WriteConsoleInputW);
    inp_reset();
    INPUT_RECORD r = mkkey(0, TRUE), o; DWORD n = 0;
    r.Event.KeyEvent.uChar.AsciiChar = 'x';
    r.Event.KeyEvent.wVirtualKeyCode = 'X';
    CC(WriteConsoleInputA(hin_(), &r, 1, &n)); CEQ(n, 1);
    CC(ReadConsoleInputW(hin_(), &o, 1, &n)); CEQ(n, 1);
    CEQ(o.Event.KeyEvent.uChar.UnicodeChar, 'x');
    r = mkkey(L'y', TRUE);
    CC(WriteConsoleInputW(hin_(), &r, 1, &n));
    CC(ReadConsoleInputA(hin_(), &o, 1, &n)); CEQ(n, 1);
    CEQ(o.Event.KeyEvent.uChar.AsciiChar, 'y');
}

CT(con_input_a_variant_codepage) {
    CREQ(WriteConsoleInputW); CREQ(ReadConsoleInputA); CREQ(PeekConsoleInputA); CREQ(SetConsoleCP);
    UINT old = GetConsoleCP();
    INPUT_RECORD r = mkkey(0xE9, TRUE), o; DWORD n = 0;
    CC(SetConsoleCP(437));
    inp_reset();
    CC(WriteConsoleInputW(hin_(), &r, 1, &n));
    CC(PeekConsoleInputA(hin_(), &o, 1, &n)); CEQ(n, 1);
    CEQ((unsigned char)o.Event.KeyEvent.uChar.AsciiChar, 0x82);
    CC(ReadConsoleInputA(hin_(), &o, 1, &n));
    CEQ((unsigned char)o.Event.KeyEvent.uChar.AsciiChar, 0x82);
    CC(SetConsoleCP(1252));
    CC(WriteConsoleInputW(hin_(), &r, 1, &n));
    CC(ReadConsoleInputA(hin_(), &o, 1, &n));
    CEQ((unsigned char)o.Event.KeyEvent.uChar.AsciiChar, 0xE9);
    SetConsoleCP(old);
}

CT(con_input_a_write_cp_conversion) {
    CREQ(WriteConsoleInputA); CREQ(ReadConsoleInputW); CREQ(SetConsoleCP);
    UINT old = GetConsoleCP();
    INPUT_RECORD r = mkkey(0, TRUE), o; DWORD n = 0;
    r.Event.KeyEvent.uChar.AsciiChar = (char)0x82;
    CC(SetConsoleCP(437));
    inp_reset();
    CC(WriteConsoleInputA(hin_(), &r, 1, &n));
    CC(ReadConsoleInputW(hin_(), &o, 1, &n)); CEQ(n, 1);
    CEQ(o.Event.KeyEvent.uChar.UnicodeChar, 0xE9);
    CC(SetConsoleCP(1252));
    r.Event.KeyEvent.uChar.AsciiChar = (char)0xE9;
    CC(WriteConsoleInputA(hin_(), &r, 1, &n));
    CC(ReadConsoleInputW(hin_(), &o, 1, &n));
    CEQ(o.Event.KeyEvent.uChar.UnicodeChar, 0xE9);
    SetConsoleCP(old);
}

CT(con_input_mouse_record_roundtrip) {
    CREQ(WriteConsoleInputW); CREQ(ReadConsoleInputW); CREQ(SetConsoleMode);
    rawmode(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS);
    INPUT_RECORD r, o; DWORD n = 0;
    memset(&r, 0, sizeof r);
    r.EventType = MOUSE_EVENT;
    r.Event.MouseEvent.dwMousePosition = co(5, 6);
    r.Event.MouseEvent.dwButtonState = FROM_LEFT_1ST_BUTTON_PRESSED | RIGHTMOST_BUTTON_PRESSED;
    r.Event.MouseEvent.dwControlKeyState = SHIFT_PRESSED;
    r.Event.MouseEvent.dwEventFlags = DOUBLE_CLICK;
    CC(WriteConsoleInputW(hin_(), &r, 1, &n)); CEQ(n, 1);
    CC(ReadConsoleInputW(hin_(), &o, 1, &n)); CEQ(n, 1);
    CEQ(o.EventType, MOUSE_EVENT);
    CC(coeq(o.Event.MouseEvent.dwMousePosition, 5, 6));
    CEQ(o.Event.MouseEvent.dwButtonState, FROM_LEFT_1ST_BUTTON_PRESSED | RIGHTMOST_BUTTON_PRESSED);
    CEQ(o.Event.MouseEvent.dwControlKeyState, SHIFT_PRESSED);
    CEQ(o.Event.MouseEvent.dwEventFlags, DOUBLE_CLICK);
}

CT(con_input_mouse_flag_variants) {
    CREQ(WriteConsoleInputW); CREQ(ReadConsoleInputW);
    rawmode(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS);
    DWORD flags[] = { 0, MOUSE_MOVED, DOUBLE_CLICK, MOUSE_WHEELED, MOUSE_HWHEELED };
    for (size_t i = 0; i < sizeof flags / sizeof flags[0]; i++) {
        INPUT_RECORD r, o; DWORD n = 0;
        memset(&r, 0, sizeof r);
        r.EventType = MOUSE_EVENT;
        r.Event.MouseEvent.dwMousePosition = co((int)i, (int)i + 1);
        r.Event.MouseEvent.dwEventFlags = flags[i];
        r.Event.MouseEvent.dwButtonState = flags[i] == MOUSE_WHEELED ? 0x00780000 : 0;
        CC(WriteConsoleInputW(hin_(), &r, 1, &n));
        CC(ReadConsoleInputW(hin_(), &o, 1, &n)); CEQ(n, 1);
        CCF(o.EventType == MOUSE_EVENT && o.Event.MouseEvent.dwEventFlags == flags[i], "flags 0x%lx", flags[i]);
        CCF(o.Event.MouseEvent.dwButtonState == r.Event.MouseEvent.dwButtonState, "buttons 0x%lx", o.Event.MouseEvent.dwButtonState);
    }
}

CT(con_input_window_buffer_size_record) {
    CREQ(WriteConsoleInputW); CREQ(ReadConsoleInputW);
    rawmode(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS);
    INPUT_RECORD r, o; DWORD n = 0;
    memset(&r, 0, sizeof r);
    r.EventType = WINDOW_BUFFER_SIZE_EVENT;
    r.Event.WindowBufferSizeEvent.dwSize = co(123, 45);
    CC(WriteConsoleInputW(hin_(), &r, 1, &n)); CEQ(n, 1);
    CC(ReadConsoleInputW(hin_(), &o, 1, &n)); CEQ(n, 1);
    CEQ(o.EventType, WINDOW_BUFFER_SIZE_EVENT);
    CC(coeq(o.Event.WindowBufferSizeEvent.dwSize, 123, 45));
}

CT(con_input_focus_record) {
    CREQ(WriteConsoleInputW); CREQ(ReadConsoleInputW);
    rawmode(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS);
    for (int v = 0; v < 2; v++) {
        INPUT_RECORD r, o; DWORD n = 0;
        memset(&r, 0, sizeof r);
        r.EventType = FOCUS_EVENT;
        r.Event.FocusEvent.bSetFocus = v;
        CC(WriteConsoleInputW(hin_(), &r, 1, &n)); CEQ(n, 1);
        CC(ReadConsoleInputW(hin_(), &o, 1, &n)); CEQ(n, 1);
        CEQ(o.EventType, FOCUS_EVENT);
        CEQ(o.Event.FocusEvent.bSetFocus, v);
    }
}

CT(con_input_menu_record) {
    CREQ(WriteConsoleInputW); CREQ(ReadConsoleInputW);
    rawmode(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS);
    INPUT_RECORD r, o; DWORD n = 0;
    memset(&r, 0, sizeof r);
    r.EventType = MENU_EVENT;
    r.Event.MenuEvent.dwCommandId = 0xABCD;
    CC(WriteConsoleInputW(hin_(), &r, 1, &n)); CEQ(n, 1);
    CC(ReadConsoleInputW(hin_(), &o, 1, &n)); CEQ(n, 1);
    CEQ(o.EventType, MENU_EVENT);
    CEQ(o.Event.MenuEvent.dwCommandId, 0xABCD);
}

CT(con_input_mixed_event_types_order) {
    CREQ(WriteConsoleInputW); CREQ(ReadConsoleInputW);
    rawmode(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS);
    INPUT_RECORD r[5], o[5]; DWORD n = 0;
    memset(r, 0, sizeof r);
    r[0] = mkkey(L'a', TRUE);
    r[1].EventType = MOUSE_EVENT; r[1].Event.MouseEvent.dwMousePosition = co(1, 2); r[1].Event.MouseEvent.dwButtonState = 1;
    r[2].EventType = FOCUS_EVENT; r[2].Event.FocusEvent.bSetFocus = 1;
    r[3].EventType = WINDOW_BUFFER_SIZE_EVENT; r[3].Event.WindowBufferSizeEvent.dwSize = co(10, 20);
    r[4].EventType = MENU_EVENT; r[4].Event.MenuEvent.dwCommandId = 7;
    CC(WriteConsoleInputW(hin_(), r, 5, &n)); CEQ(n, 5);
    CEQ(inp_count(), 5);
    CC(ReadConsoleInputW(hin_(), o, 5, &n)); CEQ(n, 5);
    CEQ(o[0].EventType, KEY_EVENT); CEQ(o[1].EventType, MOUSE_EVENT); CEQ(o[2].EventType, FOCUS_EVENT);
    CEQ(o[3].EventType, WINDOW_BUFFER_SIZE_EVENT); CEQ(o[4].EventType, MENU_EVENT);
}

CT(con_input_unknown_event_type) {
    CREQ(WriteConsoleInputW);
    inp_reset();
    INPUT_RECORD r, o; DWORD n = 0;
    memset(&r, 0, sizeof r);
    r.EventType = 0x1234;
    SetLastError(0xBAD);
    BOOL ok = WriteConsoleInputW(hin_(), &r, 1, &n);
    cnote("unknown EventType write -> %d err=%lu n=%lu pending=%lu", ok, GetLastError(), n, inp_count());
    if (inp_count()) { ReadConsoleInputW(hin_(), &o, 1, &n); cnote("read back type=0x%x", o.EventType); }
}

CT(con_input_read_length_zero) {
    CREQ(ReadConsoleInputW); CREQ(PeekConsoleInputW);
    inp_reset();
    typestr(hin_(), L"a");
    INPUT_RECORD o; DWORD n = 0xAA;
    SetLastError(0xBAD);
    BOOL ok = ReadConsoleInputW(hin_(), &o, 0, &n);
    cnote("ReadConsoleInputW(len 0) -> %d err=%lu n=%lu", ok, GetLastError(), n);
    n = 0xAA; SetLastError(0xBAD);
    ok = PeekConsoleInputW(hin_(), &o, 0, &n);
    cnote("PeekConsoleInputW(len 0) -> %d err=%lu n=%lu", ok, GetLastError(), n);
    CEQ(inp_count(), 2);
}

CT(con_input_ex_nowait_empty) {
    ReadConsoleInputExW_pfn fw = UN(ReadConsoleInputExW);
    ReadConsoleInputExA_pfn fa = UN(ReadConsoleInputExA);
    CREQ(fw); CREQ(fa);
    CWINESKIP("ReadConsoleInputEx is an unimplemented stub in the emulator");
    inp_reset();
    INPUT_RECORD o[2]; DWORD n = 0xAA;
    DWORD t0 = GetTickCount();
    SetLastError(0xBAD);
    BOOL ok = fw(hin_(), o, 2, &n, CONSOLE_READ_NOWAIT);
    cnote("ReadConsoleInputExW(NOWAIT, empty) -> %d n=%lu err=%lu", ok, n, GetLastError());
    CC(ok); CEQ(n, 0);
    n = 0xAA;
    ok = fa(hin_(), o, 2, &n, CONSOLE_READ_NOWAIT);
    CC(ok); CEQ(n, 0);
    CC(GetTickCount() - t0 < 2000);
}

CT(con_input_ex_noremove) {
    ReadConsoleInputExW_pfn fw = UN(ReadConsoleInputExW);
    CREQ(fw);
    CWINESKIP("ReadConsoleInputEx is an unimplemented stub in the emulator");
    inp_reset();
    typestr(hin_(), L"ab");
    INPUT_RECORD o[8]; DWORD n = 0;
    CC(fw(hin_(), o, 8, &n, CONSOLE_READ_NOREMOVE)); CEQ(n, 4);
    CEQ(inp_count(), 4);
    CEQ(o[0].Event.KeyEvent.uChar.UnicodeChar, 'a');
    CC(fw(hin_(), o, 8, &n, CONSOLE_READ_NOREMOVE | CONSOLE_READ_NOWAIT)); CEQ(n, 4);
    CEQ(inp_count(), 4);
}

CT(con_input_ex_flags_zero_removes) {
    ReadConsoleInputExW_pfn fw = UN(ReadConsoleInputExW);
    CREQ(fw);
    CWINESKIP("ReadConsoleInputEx is an unimplemented stub in the emulator");
    inp_reset();
    typestr(hin_(), L"ab");
    INPUT_RECORD o[8]; DWORD n = 0;
    CC(fw(hin_(), o, 2, &n, 0)); CEQ(n, 2);
    CEQ(inp_count(), 2);
    CC(fw(hin_(), o, 8, &n, CONSOLE_READ_NOWAIT)); CEQ(n, 2);
    CEQ(inp_count(), 0);
}

CT(con_input_ex_invalid_handle) {
    ReadConsoleInputExW_pfn fw = UN(ReadConsoleInputExW);
    CREQ(fw);
    CWINESKIP("ReadConsoleInputEx is an unimplemented stub in the emulator");
    INPUT_RECORD o[2]; DWORD n = 0;
    FAILS(fw(NULL, o, 2, &n, 0), ERROR_INVALID_HANDLE);
    FAILS(fw(INVALID_HANDLE_VALUE, o, 2, &n, 0), ERROR_INVALID_HANDLE);
    FAILS(fw(hout_(), o, 2, &n, 0), ERROR_INVALID_HANDLE);
}

/* ---------------- ReadConsole cooked ---------------- */

CT(con_readw_line_basic) {
    CREQ(ReadConsoleW);
    cooked();
    typestr(hin_(), L"hello\r");
    WCHAR b[64] = { 0 }; DWORD n = 0;
    CC(ReadConsoleW(hin_(), b, 64, &n, NULL));
    CEQ(n, 7);
    CWSTR(b, L"hello\r\n");
}

CT(con_reada_line_basic) {
    CREQ(ReadConsoleA);
    cooked();
    typestr(hin_(), L"ansi\r");
    char b[64] = { 0 }; DWORD n = 0;
    CC(ReadConsoleA(hin_(), b, 64, &n, NULL));
    CEQ(n, 6);
    CC(!memcmp(b, "ansi\r\n", 6));
}

CT(con_read_empty_line) {
    CREQ(ReadConsoleW);
    cooked();
    typestr(hin_(), L"\r");
    WCHAR b[8] = { 0 }; DWORD n = 0;
    CC(ReadConsoleW(hin_(), b, 8, &n, NULL));
    CEQ(n, 2);
    CC(b[0] == L'\r' && b[1] == L'\n');
}

CT(con_read_line_two_lines_two_calls) {
    CREQ(ReadConsoleW);
    cooked();
    typestr(hin_(), L"first\rsecond\r");
    WCHAR b[64]; DWORD n = 0;
    memset(b, 0, sizeof b);
    CC(ReadConsoleW(hin_(), b, 64, &n, NULL)); CEQ(n, 7); CWSTR(b, L"first\r\n");
    memset(b, 0, sizeof b);
    CC(ReadConsoleW(hin_(), b, 64, &n, NULL)); CEQ(n, 8); CWSTR(b, L"second\r\n");
}

CT(con_read_line_small_buffer_keeps_rest) {
    CREQ(ReadConsoleW);
    cooked();
    typestr(hin_(), L"hello\r");
    WCHAR b[64]; DWORD n = 0;
    memset(b, 0, sizeof b);
    CC(ReadConsoleW(hin_(), b, 3, &n, NULL));
    CEQ(n, 3);
    CC(b[0] == L'h' && b[1] == L'e' && b[2] == L'l');
    CEQ(b[3], 0);
    memset(b, 0, sizeof b);
    CC(ReadConsoleW(hin_(), b, 64, &n, NULL));
    CEQ(n, 4);
    CWSTR(b, L"lo\r\n");
}

CT(con_read_line_buffer_exactly_line) {
    CREQ(ReadConsoleW);
    cooked();
    typestr(hin_(), L"abc\r");
    WCHAR b[16]; DWORD n = 0;
    memset(b, 0, sizeof b);
    CC(ReadConsoleW(hin_(), b, 5, &n, NULL));
    CEQ(n, 5);
    CWSTR(b, L"abc\r\n");
}

CT(con_read_line_backspace) {
    CREQ(ReadConsoleW);
    cooked();
    INPUT_RECORD r[16]; int k = 0; DWORD w = 0;
    const WCHAR seq[] = { 'a', 'b', 'x', 8, 'c', '\r' };
    for (int i = 0; i < 6; i++) { r[k++] = mkkey(seq[i], TRUE); r[k++] = mkkey(seq[i], FALSE); }
    WriteConsoleInputW(hin_(), r, k, &w);
    WCHAR b[32] = { 0 }; DWORD n = 0;
    CC(ReadConsoleW(hin_(), b, 32, &n, NULL));
    CEQ(n, 5);
    CWSTR(b, L"abc\r\n");
}

CT(con_read_line_escape_clears) {
    CREQ(ReadConsoleW);
    cooked();
    INPUT_RECORD r[16]; int k = 0; DWORD w = 0;
    const WCHAR seq[] = { 'a', 'b', 'c', 27, 'x', '\r' };
    for (int i = 0; i < 6; i++) { r[k++] = mkkey(seq[i], TRUE); r[k++] = mkkey(seq[i], FALSE); }
    WriteConsoleInputW(hin_(), r, k, &w);
    WCHAR b[32] = { 0 }; DWORD n = 0;
    CC(ReadConsoleW(hin_(), b, 32, &n, NULL));
    CEQ(n, 3);
    CWSTR(b, L"x\r\n");
}

CT(con_read_line_left_arrow_insert) {
    CREQ(ReadConsoleW);
    cooked();
    INPUT_RECORD r[16]; int k = 0; DWORD w = 0;
    r[k++] = mkkey(L'a', TRUE); r[k++] = mkkey(L'a', FALSE);
    r[k++] = mkkey(L'c', TRUE); r[k++] = mkkey(L'c', FALSE);
    r[k++] = keyrec(0, VK_LEFT, TRUE, ENHANCED_KEY); r[k++] = keyrec(0, VK_LEFT, FALSE, ENHANCED_KEY);
    r[k++] = mkkey(L'b', TRUE); r[k++] = mkkey(L'b', FALSE);
    r[k++] = mkkey(L'\r', TRUE); r[k++] = mkkey(L'\r', FALSE);
    WriteConsoleInputW(hin_(), r, k, &w);
    WCHAR b[32] = { 0 }; DWORD n = 0;
    CC(ReadConsoleW(hin_(), b, 32, &n, NULL));
    cnote("left-arrow edit result: \"%ls\" n=%lu", b, n);
    CC(n >= 3);
}

CT(con_read_line_ignores_key_up_events) {
    CREQ(ReadConsoleW);
    cooked();
    INPUT_RECORD r[8]; int k = 0; DWORD w = 0;
    r[k++] = mkkey(L'u', FALSE); r[k++] = mkkey(L'p', FALSE);
    r[k++] = mkkey(L'o', TRUE);  r[k++] = mkkey(L'k', TRUE);
    r[k++] = mkkey(L'\r', TRUE);
    WriteConsoleInputW(hin_(), r, k, &w);
    WCHAR b[16] = { 0 }; DWORD n = 0;
    CC(ReadConsoleW(hin_(), b, 16, &n, NULL));
    CEQ(n, 4);
    CWSTR(b, L"ok\r\n");
}

CT(con_read_line_unicode_chars) {
    CREQ(ReadConsoleW);
    cooked();
    static const WCHAR in[] = { 0xE9, 0x4E2D, 0x416, '\r', 0 };
    typestr(hin_(), in);
    WCHAR b[16] = { 0 }; DWORD n = 0;
    CC(ReadConsoleW(hin_(), b, 16, &n, NULL));
    CEQ(n, 5);
    CEQ(b[0], 0xE9); CEQ(b[1], 0x4E2D); CEQ(b[2], 0x416); CEQ(b[3], '\r'); CEQ(b[4], '\n');
}

CT(con_reada_cp437_char) {
    CREQ(ReadConsoleA); CREQ(SetConsoleCP);
    UINT old = GetConsoleCP();
    CC(SetConsoleCP(437));
    cooked();
    static const WCHAR in[] = { 0xE9, '\r', 0 };
    typestr(hin_(), in);
    unsigned char b[16] = { 0 }; DWORD n = 0;
    CC(ReadConsoleA(hin_(), b, 16, &n, NULL));
    CEQ(n, 3);
    CEQ(b[0], 0x82);
    SetConsoleCP(old);
}

CT(con_reada_cp1252_char) {
    CREQ(ReadConsoleA); CREQ(SetConsoleCP);
    UINT old = GetConsoleCP();
    CC(SetConsoleCP(1252));
    cooked();
    static const WCHAR in[] = { 0xE9, '\r', 0 };
    typestr(hin_(), in);
    unsigned char b[16] = { 0 }; DWORD n = 0;
    CC(ReadConsoleA(hin_(), b, 16, &n, NULL));
    CEQ(n, 3);
    CEQ(b[0], 0xE9);
    SetConsoleCP(old);
}

CT(con_reada_utf8_cp) {
    CREQ(ReadConsoleA); CREQ(SetConsoleCP);
    UINT old = GetConsoleCP();
    CC(SetConsoleCP(65001));
    cooked();
    static const WCHAR in[] = { 0xE9, '\r', 0 };
    typestr(hin_(), in);
    unsigned char b[16] = { 0 }; DWORD n = 0;
    CC(ReadConsoleA(hin_(), b, 16, &n, NULL));
    cnote("UTF-8 ReadConsoleA n=%lu bytes %02x %02x %02x", n, b[0], b[1], b[2]);
    /* P1 UNRESOLVED: baseline returned 00 0d 0a (the e-acute became NUL) with code page 65001; cause not explained. */
    CEQ(b[0], 0xC3); CEQ(b[1], 0xA9);
    SetConsoleCP(old);
}

CT(con_read_echo_on_shows_text) {
    CREQ(ReadConsoleW); CREQ(SetConsoleCursorPosition);
    HANDLE ho = hout_();
    clear_buf(ho);
    CC(SetConsoleCursorPosition(ho, co(0, 5)));
    SetConsoleMode(hin_(), ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
    inp_reset();
    typestr(hin_(), L"echoed\r");
    WCHAR b[32]; DWORD n = 0;
    CC(ReadConsoleW(hin_(), b, 32, &n, NULL));
    CC(rowis(ho, 5, 0, L"echoed"));
    COORD c = curpos(ho);
    CCF(c.X == 0 && c.Y == 6, "cursor %d,%d", c.X, c.Y);
}

CT(con_read_echo_off_hides_text) {
    CREQ(ReadConsoleW);
    HANDLE ho = hout_();
    clear_buf(ho);
    CC(SetConsoleCursorPosition(ho, co(0, 5)));
    SetConsoleMode(hin_(), ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);
    inp_reset();
    typestr(hin_(), L"secret\r");
    WCHAR b[32] = { 0 }; DWORD n = 0;   /* P1 GUARD: ReadConsoleW does not NUL-terminate; CWSTR needs a zeroed buffer */
    CC(ReadConsoleW(hin_(), b, 32, &n, NULL));
    CEQ(n, 8);
    CWSTR(b, L"secret\r\n");
    CC(rowis(ho, 5, 0, L"      "));
}

CT(con_read_blocks_until_line) {
    CREQ(ReadConsoleW);
    cooked();
    HANDLE t = CreateThread(NULL, 0, late_typer, (LPVOID)(ULONG_PTR)400, 0, NULL);
    WCHAR b[32] = { 0 }; DWORD n = 0;
    DWORD t0 = GetTickCount();
    CC(ReadConsoleW(hin_(), b, 32, &n, NULL));
    CCF(GetTickCount() - t0 >= 250, "no wait");
    CWSTR(b, L"late\r\n");
    WaitForSingleObject(t, 5000); CloseHandle(t);
}

static DWORD WINAPI late_cr(LPVOID p) {
    Sleep((DWORD)(ULONG_PTR)p);
    INPUT_RECORD cr = mkkey(L'\r', TRUE); DWORD w;
    WriteConsoleInputW(hin_(), &cr, 1, &w);
    return 0;
}

CT(con_read_line_partial_input_blocks) {
    CREQ(ReadConsoleW);
    cooked();
    typestr(hin_(), L"nocr");
    HANDLE t = CreateThread(NULL, 0, late_cr, (LPVOID)(ULONG_PTR)500, 0, NULL);
    WCHAR b[32] = { 0 }; DWORD n = 0;
    DWORD t0 = GetTickCount();
    CC(ReadConsoleW(hin_(), b, 32, &n, NULL));
    CCF(GetTickCount() - t0 >= 300, "line read returned before CR (%lu ms)", GetTickCount() - t0);
    CWSTR(b, L"nocr\r\n");
    WaitForSingleObject(t, 5000); CloseHandle(t);
}

CT(con_read_raw_available_chars) {
    CREQ(ReadConsoleW);
    rawmode(0);
    typestr(hin_(), L"abc");
    WCHAR b[16] = { 0 }; DWORD n = 0;
    CC(ReadConsoleW(hin_(), b, 16, &n, NULL));
    CEQ(n, 3);
    CC(b[0] == L'a' && b[1] == L'b' && b[2] == L'c');
}

CT(con_read_raw_small_buffer) {
    CREQ(ReadConsoleW);
    rawmode(0);
    typestr(hin_(), L"abc");
    WCHAR b[16] = { 0 }; DWORD n = 0;
    CC(ReadConsoleW(hin_(), b, 2, &n, NULL));
    CEQ(n, 2);
    CC(b[0] == L'a' && b[1] == L'b');
    CEQ(b[2], 0);
}

CT(con_read_raw_cr_not_translated) {
    CREQ(ReadConsoleW);
    rawmode(0);
    typestr(hin_(), L"x\r");
    WCHAR b[16] = { 0 }; DWORD n = 0;
    CC(ReadConsoleW(hin_(), b, 16, &n, NULL));
    CEQ(n, 2);
    CC(b[0] == L'x' && b[1] == L'\r');
}

CT(con_read_raw_a_variant) {
    CREQ(ReadConsoleA);
    rawmode(0);
    typestr(hin_(), L"AB");
    char b[16] = { 0 }; DWORD n = 0;
    CC(ReadConsoleA(hin_(), b, 16, &n, NULL));
    CEQ(n, 2);
    CC(b[0] == 'A' && b[1] == 'B');
}

CT(con_read_raw_skips_key_up) {
    CREQ(ReadConsoleW);
    rawmode(0);
    INPUT_RECORD r[3]; DWORD w;
    r[0] = mkkey(L'a', FALSE); r[1] = mkkey(L'b', TRUE); r[2] = mkkey(L'b', FALSE);
    WriteConsoleInputW(hin_(), r, 3, &w);
    WCHAR b[8] = { 0 }; DWORD n = 0;
    CC(ReadConsoleW(hin_(), b, 8, &n, NULL));
    CEQ(n, 1);
    CEQ(b[0], 'b');
}

CT(con_read_raw_repeat_count) {
    CREQ(ReadConsoleW);
    rawmode(0);
    INPUT_RECORD r = mkkey(L'z', TRUE); DWORD w;
    r.Event.KeyEvent.wRepeatCount = 3;
    WriteConsoleInputW(hin_(), &r, 1, &w);
    WCHAR b[8] = { 0 }; DWORD n = 0;
    CC(ReadConsoleW(hin_(), b, 8, &n, NULL));
    cnote("repeat count 3 -> ReadConsoleW returned %lu chars", n);
    CC(n >= 1 && b[0] == L'z');
}

CT(con_read_raw_skips_non_key_events) {
    CREQ(ReadConsoleW);
    rawmode(ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS);
    INPUT_RECORD r[3]; DWORD w;
    memset(r, 0, sizeof r);
    r[0].EventType = FOCUS_EVENT; r[0].Event.FocusEvent.bSetFocus = 1;
    r[1].EventType = MOUSE_EVENT;
    r[2] = mkkey(L'm', TRUE);
    WriteConsoleInputW(hin_(), r, 3, &w);
    WCHAR b[8] = { 0 }; DWORD n = 0;
    CC(ReadConsoleW(hin_(), b, 8, &n, NULL));
    CEQ(n, 1);
    CEQ(b[0], 'm');
}

CT(con_read_invalid_handles) {
    CREQ(ReadConsoleW); CREQ(ReadConsoleA);
    WCHAR b[8]; char a[8]; DWORD n = 0xAA;
    HANDLE ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "rc.txt");
    HANDLE f = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x1234, ev, hout_(), f };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        if (bad[i] == f) {
            /* P1 EXPECT-FIX (audit: was a tolerance 6/1/87): kernelbase issues the ConDrv ioctl on the handle, a plain
               disk file answers with its own status: ERROR_INVALID_PARAMETER (87) for both W and A in the baseline. */
            SetLastError(0xBAD); BOOL r1 = ReadConsoleW(bad[i], b, 8, &n, NULL); DWORD e1 = GetLastError();
            SetLastError(0xBAD); BOOL r2 = ReadConsoleA(bad[i], a, 8, &n, NULL); DWORD e2 = GetLastError();
            CEQ(r1, 0); CERRV(e1, ERROR_INVALID_PARAMETER); CEQ(r2, 0); CERRV(e2, ERROR_INVALID_PARAMETER);
            continue;
        }
        FAILS(ReadConsoleW(bad[i], b, 8, &n, NULL), ERROR_INVALID_HANDLE);
        FAILS(ReadConsoleA(bad[i], a, 8, &n, NULL), ERROR_INVALID_HANDLE);
    }
    CloseHandle(ev); CloseHandle(f);
}

CT(con_read_ctrl_wakeup_tab) {
    CREQ(ReadConsoleW);
    cooked();
    typestr(hin_(), L"ab\tcd\r");
    WCHAR b[32] = { 0 }; DWORD n = 0;
    CONSOLE_READCONSOLE_CONTROL ctl;
    memset(&ctl, 0, sizeof ctl);
    ctl.nLength = sizeof ctl;
    ctl.dwCtrlWakeupMask = 1u << 9;
    SetLastError(0xBAD);
    BOOL ok = ReadConsoleW(hin_(), b, 32, &n, &ctl);
    cnote("wakeup read: ok=%d err=%lu n=%lu ctlstate=0x%lx", ok, GetLastError(), n, ctl.dwControlKeyState);
    CC(ok);
    CC(n >= 3);
    CC(b[0] == L'a' && b[1] == L'b');
    CC(n >= 3 && b[n - 1] == L'\t');
}

CT(con_read_ctrl_no_mask_ignored) {
    CREQ(ReadConsoleW);
    cooked();
    typestr(hin_(), L"xy\r");
    WCHAR b[32] = { 0 }; DWORD n = 0;
    CONSOLE_READCONSOLE_CONTROL ctl;
    memset(&ctl, 0, sizeof ctl);
    ctl.nLength = sizeof ctl;
    CC(ReadConsoleW(hin_(), b, 32, &n, &ctl));
    CEQ(n, 4);
    CWSTR(b, L"xy\r\n");
}

CT(con_read_ctrl_initial_chars) {
    CREQ(ReadConsoleW);
    cooked();
    typestr(hin_(), L"cd\r");
    WCHAR b[32] = { L'a', L'b', 0 }; DWORD n = 0;
    CONSOLE_READCONSOLE_CONTROL ctl;
    memset(&ctl, 0, sizeof ctl);
    ctl.nLength = sizeof ctl;
    ctl.nInitialChars = 2;
    BOOL ok = ReadConsoleW(hin_(), b, 32, &n, &ctl);
    cnote("initial chars read: ok=%d n=%lu text=\"%ls\"", ok, n, b);
    CC(ok);
}

CT(con_read_ctrl_bad_length) {
    CREQ(ReadConsoleW);
    cooked();
    typestr(hin_(), L"q\r");
    WCHAR b[32] = { 0 }; DWORD n = 0;
    CONSOLE_READCONSOLE_CONTROL ctl;
    memset(&ctl, 0, sizeof ctl);
    ctl.nLength = 4;
    SetLastError(0xBAD);
    BOOL ok = ReadConsoleW(hin_(), b, 32, &n, &ctl);
    cnote("ReadConsoleW with nLength=4 -> %d err=%lu n=%lu", ok, GetLastError(), n);
    FlushConsoleInputBuffer(hin_());
}

CT(con_read_processed_ctrl_c_key_record_raw) {
    CREQ(ReadConsoleInputW);
    /* with processed input disabled Ctrl+C is an ordinary key record */
    rawmode(0);
    INPUT_RECORD r = keyrec(0x03, 'C', TRUE, LEFT_CTRL_PRESSED), o; DWORD n = 0;
    CC(WriteConsoleInputW(hin_(), &r, 1, &n));
    CC(ReadConsoleInputW(hin_(), &o, 1, &n)); CEQ(n, 1);
    CEQ(o.Event.KeyEvent.uChar.UnicodeChar, 3);
    CEQ(o.Event.KeyEvent.wVirtualKeyCode, 'C');
}

CT(con_readfile_after_console_input_events) {
    CREQ(ReadConsoleInputW);
    rawmode(0);
    typestr(hin_(), L"fz");
    char b[8] = { 0 }; DWORD n = 0;
    CC(ReadFile(hin_(), b, 8, &n, NULL));
    CEQ(n, 2);
    CC(b[0] == 'f' && b[1] == 'z');
    CEQ(inp_count(), 0);
}

CT(con_peek_after_readfile_partial) {
    CREQ(PeekConsoleInputW);
    rawmode(0);
    typestr(hin_(), L"abc");
    char b[8] = { 0 }; DWORD n = 0;
    CC(ReadFile(hin_(), b, 1, &n, NULL));
    CEQ(n, 1);
    cnote("events left after 1-byte ReadFile: %lu", inp_count());
    FlushConsoleInputBuffer(hin_());
}
