/* console_apis: screen buffers, cell read/write/fill, cursor, attributes, scrolling, window/buffer size, fonts */
#include "ch.h"

#define FAILS(call, err)  do { SetLastError(0xBAD); BOOL r_ = (call); DWORD le_ = GetLastError(); CEQ(r_, 0); CERRV(le_, err); } while (0)

#define BUF() HANDLE b = mkbuf_std(); if (!b) return

static void fillrow(HANDLE b, int y, WCHAR ch) { DWORD n; FillConsoleOutputCharacterW(b, ch, 80, co(0, y), &n); }
static HANDLE inhandle_bad_list(HANDLE *out) { /* helper: unusable handles for output APIs */
    out[0] = NULL; out[1] = INVALID_HANDLE_VALUE; out[2] = (HANDLE)(ULONG_PTR)0x1234; out[3] = hin_();
    return out[0];
}

/* ---------------- CreateConsoleScreenBuffer ---------------- */

CT(con_csb_basic) {
    CREQ(CreateConsoleScreenBuffer); CREQ(GetConsoleMode);
    SetLastError(0xBAD);
    HANDLE h = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    CNE(h, INVALID_HANDLE_VALUE); CNE(h, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    CEQ(GetFileType(h), FILE_TYPE_CHAR);
    DWORD m; CC(GetConsoleMode(h, &m));
    CC(CloseHandle(h));
}

CT(con_csb_access_variants) {
    CREQ(CreateConsoleScreenBuffer); CREQ(WriteConsoleW);
    DWORD acc[] = { GENERIC_READ | GENERIC_WRITE, GENERIC_WRITE, GENERIC_READ, 0, GENERIC_ALL };
    for (size_t i = 0; i < sizeof acc / sizeof acc[0]; i++) {
        SetLastError(0xBAD);
        HANDLE h = CreateConsoleScreenBuffer(acc[i], FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
        DWORD e_ = GetLastError();
        cnote("CreateConsoleScreenBuffer access=0x%lx -> %p err=%lu", acc[i], h, e_);
        if (i == 0 || i == 1) CCF(h != INVALID_HANDLE_VALUE, "access 0x%lx", acc[i]);
        /* P1 TOLERANCE: GENERIC_ALL is passed unmapped to NtCreateFile on \Device\ConDrv\ScreenBuffer and Win10
           answered ERROR_ACCESS_DENIED; only GENERIC_READ/GENERIC_WRITE are documented. Accept success or access denied. */
        if (i == 4 && h == INVALID_HANDLE_VALUE) CERRV(e_, ERROR_ACCESS_DENIED);
        if (h != INVALID_HANDLE_VALUE) {
            DWORD n = 0;
            BOOL w = WriteConsoleW(h, L"x", 1, &n, NULL);
            cnote("  write via access 0x%lx -> %d err=%lu", acc[i], w, GetLastError());
            if (i == 0 || i == 1 || (i == 4 && h != INVALID_HANDLE_VALUE)) CC(w);
            CloseHandle(h);
        }
    }
}

CT(con_csb_share_variants) {
    CREQ(CreateConsoleScreenBuffer);
    DWORD sh[] = { 0, FILE_SHARE_READ, FILE_SHARE_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE };
    for (size_t i = 0; i < sizeof sh / sizeof sh[0]; i++) {
        HANDLE h = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, sh[i], NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
        CCF(h != INVALID_HANDLE_VALUE, "share 0x%lx err=%lu", sh[i], GetLastError());
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }
}

CT(con_csb_invalid_flags) {
    CREQ(CreateConsoleScreenBuffer);
    DWORD fl[] = { 0, 4, 0x80, 0x100, 0x80000000 };
    for (size_t i = 0; i < sizeof fl / sizeof fl[0]; i++) {
        SetLastError(0xBAD);
        HANDLE h = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, fl[i], NULL);
        DWORD e = GetLastError();
        cnote("CreateConsoleScreenBuffer flags=0x%lx -> %p err=%lu", fl[i], h, e);
        /* P1 EXPECT-FIX: Win10 conhost (ConsoleCreateScreenBuffer) rejects only flags & ~1; flags 0 is accepted
           as a text-mode buffer (observed: valid handle). */
        if (fl[i] == 0) CCF(h != INVALID_HANDLE_VALUE, "flags 0x%lx accepted", fl[i]);
        else CCF(h == INVALID_HANDLE_VALUE, "flags 0x%lx should fail", fl[i]);
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }
}

CT(con_csb_inherit_flag) {
    CREQ(CreateConsoleScreenBuffer);
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE a = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, CONSOLE_TEXTMODE_BUFFER, NULL);
    HANDLE n = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    CNE(a, INVALID_HANDLE_VALUE); CNE(n, INVALID_HANDLE_VALUE);
    DWORD fa = 0, fn = 0;
    CC(GetHandleInformation(a, &fa)); CC(GetHandleInformation(n, &fn));
    CEQ(fa & HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    CEQ(fn & HANDLE_FLAG_INHERIT, 0);
    CloseHandle(a); CloseHandle(n);
}

CT(con_csb_buffers_independent) {
    CREQ(CreateConsoleScreenBuffer); CREQ(WriteConsoleW);
    HANDLE a = mkbuf_std(), c = mkbuf_std(); if (!a || !c) return;
    CC(wrw(a, L"AAAA")); CC(wrw(c, L"cc"));
    CC(rowis(a, 0, 0, L"AAAA")); CC(rowis(c, 0, 0, L"cc  "));
    CC(coeq(curpos(a), 4, 0)); CC(coeq(curpos(c), 2, 0));
    CC(SetConsoleTextAttribute(a, 0x1F));
    CEQ(attrof(c), 7); CEQ(attrof(a), 0x1F);
    CloseHandle(a); CloseHandle(c);
}

CT(con_csb_initial_state) {
    CREQ(CreateConsoleScreenBuffer); CREQ(GetConsoleScreenBufferInfo);
    HANDLE h = mkbuf_raw(); CNE(h, NULL); if (!h) return;
    CONSOLE_SCREEN_BUFFER_INFO i;
    CC(GetConsoleScreenBufferInfo(h, &i));
    CC(coeq(i.dwCursorPosition, 0, 0));
    cnote("new buffer: size %dx%d window %d,%d-%d,%d attr 0x%x max %dx%d", i.dwSize.X, i.dwSize.Y, i.srWindow.Left, i.srWindow.Top, i.srWindow.Right, i.srWindow.Bottom, i.wAttributes, i.dwMaximumWindowSize.X, i.dwMaximumWindowSize.Y);
    CC(i.dwSize.X > 0 && i.dwSize.Y > 0);
    CC(i.srWindow.Left >= 0 && i.srWindow.Top >= 0 && i.srWindow.Right < i.dwSize.X && i.srWindow.Bottom < i.dwSize.Y);
    CEQ(cellch(h, 0, 0), ' ');
    CEQ(cellch(h, i.dwSize.X - 1, i.dwSize.Y - 1), ' ');
    CloseHandle(h);
}

CT(con_csb_many_buffers) {
    CREQ(CreateConsoleScreenBuffer);
    HANDLE h[40]; int n = 0;
    for (int i = 0; i < 40; i++) { h[i] = mkbuf_raw(); if (!h[i]) break; n++; }
    CEQ(n, 40);
    for (int i = 0; i < n; i++) { CC(wrw(h[i], L"x")); }
    for (int i = 0; i < n; i++) CloseHandle(h[i]);
}

CT(con_csb_closed_handle_invalid) {
    CREQ(CreateConsoleScreenBuffer);
    HANDLE h = mkbuf_raw(); CNE(h, NULL);
    CC(CloseHandle(h));
    CONSOLE_SCREEN_BUFFER_INFO i;
    FAILS(GetConsoleScreenBufferInfo(h, &i), ERROR_INVALID_HANDLE);
}

CT(con_csb_data_param_text_mode) {
    CREQ(CreateConsoleScreenBuffer);
    int junk[8] = { 0 };
    SetLastError(0xBAD);
    HANDLE h = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CONSOLE_TEXTMODE_BUFFER, junk);
    cnote("text-mode buffer with non-NULL data -> %p err=%lu", h, GetLastError());
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
}

CT(con_active_set_basic) {
    CREQ(SetConsoleActiveScreenBuffer);
    HANDLE b = mkbuf_raw(); CNE(b, NULL); if (!b) return;
    HANDLE orig = hout_();
    SetLastError(0xBAD);
    CC(SetConsoleActiveScreenBuffer(b));
    CC(SetConsoleActiveScreenBuffer(b));      /* idempotent */
    CC(SetConsoleActiveScreenBuffer(orig));
    CC(SetConsoleActiveScreenBuffer(orig));
    CloseHandle(b);
}

CT(con_active_set_invalid) {
    CREQ(SetConsoleActiveScreenBuffer);
    HANDLE bad[4]; inhandle_bad_list(bad);
    for (int i = 0; i < 4; i++) FAILS(SetConsoleActiveScreenBuffer(bad[i]), ERROR_INVALID_HANDLE);
}

CT(con_active_close_active_buffer) {
    CREQ(SetConsoleActiveScreenBuffer);
    HANDLE b = mkbuf_raw(); CNE(b, NULL); if (!b) return;
    HANDLE orig = hout_();
    CC(SetConsoleActiveScreenBuffer(b));
    CC(CloseHandle(b));
    DWORD m;
    CC(GetConsoleMode(orig, &m));
    CC(wrw(orig, L"still works"));
    HANDLE co_ = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CNE(co_, INVALID_HANDLE_VALUE);
    if (co_ != INVALID_HANDLE_VALUE) CloseHandle(co_);
}

CT(con_active_conout_follows_switch) {
    CREQ(SetConsoleActiveScreenBuffer);
    HANDLE b = mkbuf_std(); if (!b) return;
    HANDLE orig = hout_();
    clear_buf(orig);
    HANDLE c1 = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CC(wrw(c1, L"on-main"));
    CC(rowis(orig, 0, 0, L"on-main"));
    CC(SetConsoleActiveScreenBuffer(b));
    HANDLE c2 = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CC(wrw(c2, L"on-b"));
    CC(rowis(b, 0, 0, L"on-b"));
    CC(SetConsoleActiveScreenBuffer(orig));
    CloseHandle(c1); CloseHandle(c2); CloseHandle(b);
}

/* ---------------- GetConsoleScreenBufferInfo[Ex] ---------------- */

CT(con_sbi_invariants_stdout) {
    CREQ(GetConsoleScreenBufferInfo); CREQ(GetLargestConsoleWindowSize);
    CONSOLE_SCREEN_BUFFER_INFO i; memset(&i, 0xCC, sizeof i);
    CC(GetConsoleScreenBufferInfo(hout_(), &i));
    CC(i.dwSize.X > 0 && i.dwSize.Y > 0);
    CC(i.dwCursorPosition.X >= 0 && i.dwCursorPosition.X < i.dwSize.X);
    CC(i.dwCursorPosition.Y >= 0 && i.dwCursorPosition.Y < i.dwSize.Y);
    CC(i.srWindow.Left >= 0 && i.srWindow.Top >= 0);
    CC(i.srWindow.Right >= i.srWindow.Left && i.srWindow.Bottom >= i.srWindow.Top);
    CC(i.srWindow.Right < i.dwSize.X && i.srWindow.Bottom < i.dwSize.Y);
    CC(i.dwMaximumWindowSize.X >= i.srWindow.Right - i.srWindow.Left + 1);
    CC(i.dwMaximumWindowSize.Y >= i.srWindow.Bottom - i.srWindow.Top + 1);
    COORD lg = GetLargestConsoleWindowSize(hout_());
    CC(i.dwMaximumWindowSize.X <= lg.X + 0 || i.dwMaximumWindowSize.X <= i.dwSize.X);
    cnote("stdout: buffer %dx%d window %d,%d-%d,%d max %dx%d largest %dx%d attr 0x%x", i.dwSize.X, i.dwSize.Y, i.srWindow.Left, i.srWindow.Top, i.srWindow.Right, i.srWindow.Bottom, i.dwMaximumWindowSize.X, i.dwMaximumWindowSize.Y, lg.X, lg.Y, i.wAttributes);
}

CT(con_sbi_invalid_handles) {
    CREQ(GetConsoleScreenBufferInfo);
    CONSOLE_SCREEN_BUFFER_INFO i;
    HANDLE bad[4]; inhandle_bad_list(bad);
    HANDLE ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    for (int k = 0; k < 4; k++) FAILS(GetConsoleScreenBufferInfo(bad[k], &i), ERROR_INVALID_HANDLE);
    FAILS(GetConsoleScreenBufferInfo(ev, &i), ERROR_INVALID_HANDLE);
    CloseHandle(ev);
}

CT(con_sbi_same_for_stdout_and_stderr) {
    CREQ(GetConsoleScreenBufferInfo);
    CONSOLE_SCREEN_BUFFER_INFO a, c;
    CC(GetConsoleScreenBufferInfo(hout_(), &a));
    CC(GetConsoleScreenBufferInfo(herr_(), &c));
    CC(!memcmp(&a, &c, sizeof a));
}

CT(con_sbi_stable_when_idle) {
    CREQ(GetConsoleScreenBufferInfo);
    CONSOLE_SCREEN_BUFFER_INFO a, c;
    CC(GetConsoleScreenBufferInfo(hout_(), &a));
    for (int k = 0; k < 20; k++) { CC(GetConsoleScreenBufferInfo(hout_(), &c)); CC(!memcmp(&a, &c, sizeof a)); }
}

CT(con_sbiex_basic) {
    CREQ(GetConsoleScreenBufferInfoEx); CREQ(GetConsoleScreenBufferInfo);
    HANDLE b = mkbuf_std(); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFOEX x; memset(&x, 0, sizeof x); x.cbSize = sizeof x;
    CONSOLE_SCREEN_BUFFER_INFO i;
    CC(wrw(b, L"ab"));
    SetConsoleTextAttribute(b, 0x1E);
    CC(GetConsoleScreenBufferInfoEx(b, &x));
    CC(GetConsoleScreenBufferInfo(b, &i));
    CEQ(x.cbSize, 96);
    CC(x.dwSize.X == i.dwSize.X && x.dwSize.Y == i.dwSize.Y);
    CC(coeq(x.dwCursorPosition, 2, 0));
    CEQ(x.wAttributes, 0x1E);
    CC(x.srWindow.Left == i.srWindow.Left && x.srWindow.Right == i.srWindow.Right && x.srWindow.Top == i.srWindow.Top && x.srWindow.Bottom == i.srWindow.Bottom);
    CC(x.dwMaximumWindowSize.X == i.dwMaximumWindowSize.X && x.dwMaximumWindowSize.Y == i.dwMaximumWindowSize.Y);
    for (int k = 0; k < 16; k++) CCF((x.ColorTable[k] & 0xFF000000) == 0, "color %d = 0x%08lx", k, x.ColorTable[k]);
    CC(x.ColorTable[0] != x.ColorTable[15]);
    cnote("popup attr 0x%x fullscreen supported %d color0=0x%06lx color7=0x%06lx", x.wPopupAttributes, x.bFullscreenSupported, x.ColorTable[0], x.ColorTable[7]);
    CloseHandle(b);
}

CT(con_sbiex_bad_cbsize) {
    CREQ(GetConsoleScreenBufferInfoEx); CREQ(SetConsoleScreenBufferInfoEx);
    CONSOLE_SCREEN_BUFFER_INFOEX x; memset(&x, 0, sizeof x);
    DWORD sz[] = { 0, 1, 4, 80, 95, 97, 100, 192, 0x60000000 };
    HANDLE b = mkbuf_raw(); CNE(b, NULL); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFOEX good; memset(&good, 0, sizeof good); good.cbSize = sizeof good;
    CC(GetConsoleScreenBufferInfoEx(b, &good));
    for (size_t k = 0; k < sizeof sz / sizeof sz[0]; k++) {
        x = good; x.cbSize = sz[k];
        FAILS(GetConsoleScreenBufferInfoEx(b, &x), ERROR_INVALID_PARAMETER);
        x = good; x.cbSize = sz[k];
        FAILS(SetConsoleScreenBufferInfoEx(b, &x), ERROR_INVALID_PARAMETER);
    }
    CloseHandle(b);
}

CT(con_sbiex_invalid_handles) {
    CREQ(GetConsoleScreenBufferInfoEx); CREQ(SetConsoleScreenBufferInfoEx);
    CONSOLE_SCREEN_BUFFER_INFOEX x; memset(&x, 0, sizeof x); x.cbSize = sizeof x;
    HANDLE bad[4]; inhandle_bad_list(bad);
    for (int k = 0; k < 4; k++) {
        FAILS(GetConsoleScreenBufferInfoEx(bad[k], &x), ERROR_INVALID_HANDLE);
        FAILS(SetConsoleScreenBufferInfoEx(bad[k], &x), ERROR_INVALID_HANDLE);
    }
}

CT(con_sbiex_color_table_roundtrip) {
    CREQ(GetConsoleScreenBufferInfoEx); CREQ(SetConsoleScreenBufferInfoEx);
    HANDLE b = mkbuf_raw(); CNE(b, NULL); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFOEX x, y, z; memset(&x, 0, sizeof x); x.cbSize = sizeof x;
    CC(GetConsoleScreenBufferInfoEx(b, &x));
    y = x;
    y.ColorTable[5] = RGB(0x12, 0x34, 0x56);
    y.ColorTable[0] = RGB(0x01, 0x02, 0x03);
    y.ColorTable[15] = RGB(0xFE, 0xDC, 0xBA);
    SetLastError(0xBAD);
    CCF(SetConsoleScreenBufferInfoEx(b, &y), "err=%lu", GetLastError());
    memset(&z, 0, sizeof z); z.cbSize = sizeof z;
    CC(GetConsoleScreenBufferInfoEx(b, &z));
    CEQ(z.ColorTable[5], RGB(0x12, 0x34, 0x56));
    CEQ(z.ColorTable[0], RGB(0x01, 0x02, 0x03));
    CEQ(z.ColorTable[15], RGB(0xFE, 0xDC, 0xBA));
    for (int k = 1; k < 15; k++) if (k != 5) CCF(z.ColorTable[k] == x.ColorTable[k], "color %d changed", k);
    cnote("window after round trip: %d,%d-%d,%d (before %d,%d-%d,%d)", z.srWindow.Left, z.srWindow.Top, z.srWindow.Right, z.srWindow.Bottom, x.srWindow.Left, x.srWindow.Top, x.srWindow.Right, x.srWindow.Bottom);
    CloseHandle(b);
}

CT(con_sbiex_color_table_per_buffer) {
    CREQ(GetConsoleScreenBufferInfoEx); CREQ(SetConsoleScreenBufferInfoEx);
    HANDLE a = mkbuf_raw(), c = mkbuf_raw(); if (!a || !c) return;
    CONSOLE_SCREEN_BUFFER_INFOEX x, y; memset(&x, 0, sizeof x); x.cbSize = sizeof x; y = x;
    CC(GetConsoleScreenBufferInfoEx(a, &x));
    CC(GetConsoleScreenBufferInfoEx(c, &y));
    COLORREF before = y.ColorTable[2];
    x.ColorTable[2] = RGB(9, 8, 7);
    CC(SetConsoleScreenBufferInfoEx(a, &x));
    memset(&y, 0, sizeof y); y.cbSize = sizeof y;
    CC(GetConsoleScreenBufferInfoEx(c, &y));
    /* P1 EXPECT-FIX: Win10 conhost keeps ONE console-wide color table (SetConsoleScreenBufferInfoExImpl copies
       the 16 entries into the global settings), so a change through one buffer is visible through every buffer.
       Restore it afterwards because it is console-wide. UNVERIFIED-until-rerun */
    CEQ(y.ColorTable[2], RGB(9, 8, 7));
    x.ColorTable[2] = before;
    CC(SetConsoleScreenBufferInfoEx(a, &x));
    CloseHandle(a); CloseHandle(c);
}

CT(con_sbiex_set_attributes) {
    CREQ(GetConsoleScreenBufferInfoEx); CREQ(SetConsoleScreenBufferInfoEx);
    HANDLE b = mkbuf_raw(); CNE(b, NULL); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFOEX x; memset(&x, 0, sizeof x); x.cbSize = sizeof x;
    CC(GetConsoleScreenBufferInfoEx(b, &x));
    x.wAttributes = 0x2D;
    CC(SetConsoleScreenBufferInfoEx(b, &x));
    CEQ(attrof(b), 0x2D);
    memset(&x, 0, sizeof x); x.cbSize = sizeof x;
    CC(GetConsoleScreenBufferInfoEx(b, &x));
    CEQ(x.wAttributes, 0x2D);
    CloseHandle(b);
}

CT(con_sbiex_set_cursor_and_size) {
    CREQ(GetConsoleScreenBufferInfoEx); CREQ(SetConsoleScreenBufferInfoEx);
    HANDLE b = mkbuf_std(); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFOEX x; memset(&x, 0, sizeof x); x.cbSize = sizeof x;
    CC(GetConsoleScreenBufferInfoEx(b, &x));
    COORD cur0 = x.dwCursorPosition;
    x.dwCursorPosition = co(7, 3);
    x.dwSize.Y = 40;
    SetLastError(0xBAD);
    BOOL ok = SetConsoleScreenBufferInfoEx(b, &x);
    cnote("SetConsoleScreenBufferInfoEx(cursor+height) -> %d err=%lu", ok, GetLastError());
    CC(ok);
    CONSOLE_SCREEN_BUFFER_INFO i;
    CC(GetConsoleScreenBufferInfo(b, &i));
    /* P1 EXPECT-FIX: Win10 SetConsoleScreenBufferInfoExImpl never reads dwCursorPosition (it applies size,
       attributes, window, max size and color table only), so the cursor is not moved.
       Audit: exact form, the cursor stays where it was before the call (growing the height keeps it). */
    CC(coeq(i.dwCursorPosition, cur0.X, cur0.Y));
    CEQ(i.dwSize.Y, 40);
    CloseHandle(b);
}

/* ---------------- SetConsoleScreenBufferSize ---------------- */

CT(con_sbs_grow) {
    CREQ(SetConsoleScreenBufferSize); CREQ(GetConsoleScreenBufferInfo);
    BUF();
    CONSOLE_SCREEN_BUFFER_INFO a, c;
    GetConsoleScreenBufferInfo(b, &a);
    SetLastError(0xBAD);
    CCF(SetConsoleScreenBufferSize(b, co(120, 60)), "err=%lu", GetLastError());
    CC(GetConsoleScreenBufferInfo(b, &c));
    CEQ(c.dwSize.X, 120); CEQ(c.dwSize.Y, 60);
    CC(c.srWindow.Left == a.srWindow.Left && c.srWindow.Right == a.srWindow.Right && c.srWindow.Bottom == a.srWindow.Bottom);
    CloseHandle(b);
}

CT(con_sbs_grow_preserves_content) {
    CREQ(SetConsoleScreenBufferSize);
    BUF();
    CC(wrw(b, L"keep me"));
    CC(SetConsoleCursorPosition(b, co(3, 20)));
    CC(wrw(b, L"low"));
    CC(SetConsoleScreenBufferSize(b, co(100, 50)));
    CC(rowis(b, 0, 0, L"keep me"));
    CC(rowis(b, 20, 3, L"low"));
    CC(coeq(curpos(b), 6, 20));
    CEQ(cellch(b, 99, 49), ' ');
    CloseHandle(b);
}

CT(con_sbs_height_only) {
    CREQ(SetConsoleScreenBufferSize);
    BUF();
    CC(wrw(b, L"top"));
    CC(SetConsoleScreenBufferSize(b, co(80, 300)));
    CONSOLE_SCREEN_BUFFER_INFO c; GetConsoleScreenBufferInfo(b, &c);
    CEQ(c.dwSize.X, 80); CEQ(c.dwSize.Y, 300);
    CC(rowis(b, 0, 0, L"top"));
    CC(SetConsoleCursorPosition(b, co(0, 299)));
    CC(wrw(b, L"deep"));
    CC(rowis(b, 299, 0, L"deep"));
    CloseHandle(b);
}

CT(con_sbs_below_window_fails) {
    CREQ(SetConsoleScreenBufferSize);
    BUF();
    COORD sizes[] = { co(79, 25), co(80, 24), co(10, 10), co(1, 1), co(79, 100), co(200, 24) };
    for (size_t k = 0; k < sizeof sizes / sizeof sizes[0]; k++) {
        FAILS(SetConsoleScreenBufferSize(b, sizes[k]), ERROR_INVALID_PARAMETER);
    }
    CONSOLE_SCREEN_BUFFER_INFO c; GetConsoleScreenBufferInfo(b, &c);
    CEQ(c.dwSize.X, 80); CEQ(c.dwSize.Y, 25);
    CloseHandle(b);
}

CT(con_sbs_nonpositive_fails) {
    CREQ(SetConsoleScreenBufferSize);
    BUF();
    COORD sizes[] = { co(0, 0), co(0, 25), co(80, 0), co(-1, 25), co(80, -1), co(-80, -25), co(-32768, 100) };
    for (size_t k = 0; k < sizeof sizes / sizeof sizes[0]; k++) {
        FAILS(SetConsoleScreenBufferSize(b, sizes[k]), ERROR_INVALID_PARAMETER);
    }
    CloseHandle(b);
}

CT(con_sbs_same_size_ok) {
    CREQ(SetConsoleScreenBufferSize);
    BUF();
    SetLastError(0xBAD);
    CCF(SetConsoleScreenBufferSize(b, co(80, 25)), "err=%lu", GetLastError());
    CloseHandle(b);
}

CT(con_sbs_invalid_handles) {
    CREQ(SetConsoleScreenBufferSize);
    HANDLE bad[4]; inhandle_bad_list(bad);
    for (int k = 0; k < 4; k++) FAILS(SetConsoleScreenBufferSize(bad[k], co(80, 25)), ERROR_INVALID_HANDLE);
}

CT(con_sbs_wide_buffer) {
    CREQ(SetConsoleScreenBufferSize);
    BUF();
    SetLastError(0xBAD);
    BOOL ok = SetConsoleScreenBufferSize(b, co(400, 25));
    CCF(ok, "err=%lu", GetLastError());
    if (ok) {
        CC(SetConsoleCursorPosition(b, co(399, 2)));
        CC(wrw(b, L"E"));
        CEQ(cellch(b, 399, 2), 'E');
    }
    CloseHandle(b);
}

CT(con_sbs_large_height) {
    CREQ(SetConsoleScreenBufferSize);
    BUF();
    SetLastError(0xBAD);
    BOOL ok = SetConsoleScreenBufferSize(b, co(80, 9999));
    CCF(ok, "err=%lu", GetLastError());
    CloseHandle(b);
}

CT(con_sbs_cursor_stays_in_bounds_when_grown) {
    CREQ(SetConsoleScreenBufferSize);
    BUF();
    CC(SetConsoleCursorPosition(b, co(79, 24)));
    CC(SetConsoleScreenBufferSize(b, co(90, 30)));
    { COORD c_ = curpos(b); cnote("cursor after growing 80x25 -> 90x30: %d,%d", c_.X, c_.Y); }
    CC(coeq(curpos(b), 79, 24));   /* P1b UNRESOLVED: baseline cursor is (78,24) after growing, i.e. moved left by one; no decompilation read explains it */
    CloseHandle(b);
}

/* ---------------- SetConsoleWindowInfo ---------------- */

CT(con_swi_absolute_shrink) {
    CREQ(SetConsoleWindowInfo); CREQ(GetConsoleScreenBufferInfo);
    BUF();
    SMALL_RECT r = sr(0, 0, 39, 9);
    SetLastError(0xBAD);
    BOOL ok = SetConsoleWindowInfo(b, TRUE, &r);
    CCF(ok, "err=%lu", GetLastError());
    CONSOLE_SCREEN_BUFFER_INFO i; GetConsoleScreenBufferInfo(b, &i);
    cnote("window after shrink to 40x10: %d,%d-%d,%d", i.srWindow.Left, i.srWindow.Top, i.srWindow.Right, i.srWindow.Bottom);
    if (ok) { CEQ(i.srWindow.Right - i.srWindow.Left, 39); CEQ(i.srWindow.Bottom - i.srWindow.Top, 9); }
    CEQ(i.dwSize.X, 80); CEQ(i.dwSize.Y, 25);
    CloseHandle(b);
}

CT(con_swi_invalid_rects) {
    CREQ(SetConsoleWindowInfo);
    BUF();
    SMALL_RECT bad[] = {
        sr(0, 0, 80, 24),      /* beyond buffer width */
        sr(0, 0, 79, 25),      /* beyond buffer height */
        sr(-1, 0, 79, 24),
        sr(0, -1, 79, 24),
        sr(10, 0, 5, 24),      /* right < left */
        sr(0, 10, 79, 5),      /* bottom < top */
        sr(0, 0, -1, -1),
        sr(0, 0, 0x7FFF, 0x7FFF),
        sr(0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF),
    };
    for (size_t k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        SetLastError(0xBAD);
        BOOL r = SetConsoleWindowInfo(b, TRUE, &bad[k]);
        DWORD e_ = GetLastError();
        if (k >= 7) {
            /* P1 EXPECT-FIX (audit: was a tolerance): Win10 SetConsoleWindowInfoImpl validates only right>=left,
               bottom>=top and the window SIZE against the maximum (16-bit arithmetic: 0x7FFF-0+1 wraps negative) and
               never the position, so rects 7 and 8 are accepted (baseline: both succeeded). */
            CCF(r, "rect %zu err=%lu", k, e_);
            continue;
        }
        CCF(r == FALSE, "rect %zu", k);
        CERRV(e_, ERROR_INVALID_PARAMETER);
    }
    CloseHandle(b);
}

CT(con_swi_relative) {
    CREQ(SetConsoleWindowInfo); CREQ(GetConsoleScreenBufferInfo);
    BUF();
    CC(SetConsoleScreenBufferSize(b, co(120, 60)));
    SMALL_RECT r = sr(0, 0, 39, 9);
    CC(SetConsoleWindowInfo(b, TRUE, &r));
    CONSOLE_SCREEN_BUFFER_INFO i0, i1;
    CC(GetConsoleScreenBufferInfo(b, &i0));
    SMALL_RECT d = sr(10, 5, 10, 5);     /* move window by (10,5) */
    SetLastError(0xBAD);
    BOOL ok = SetConsoleWindowInfo(b, FALSE, &d);
    CCF(ok, "err=%lu", GetLastError());
    CC(GetConsoleScreenBufferInfo(b, &i1));
    CC(i1.srWindow.Left == i0.srWindow.Left + 10 && i1.srWindow.Right == i0.srWindow.Right + 10);
    CC(i1.srWindow.Top == i0.srWindow.Top + 5 && i1.srWindow.Bottom == i0.srWindow.Bottom + 5);
    d = sr(-10, -5, -10, -5);
    CC(SetConsoleWindowInfo(b, FALSE, &d));
    CC(GetConsoleScreenBufferInfo(b, &i1));
    CC(i1.srWindow.Left == i0.srWindow.Left && i1.srWindow.Top == i0.srWindow.Top);
    /* relative move off the buffer fails */
    /* P1 EXPECT-FIX (audit: was a tolerance): Win10 does not validate the window position (see
       con_swi_invalid_rects); the size is unchanged, so both relative moves succeed (baseline: both returned TRUE). */
    d = sr(-1, 0, -1, 0);
    SetLastError(0xBAD);
    CCF(SetConsoleWindowInfo(b, FALSE, &d), "err=%lu", GetLastError());
    d = sr(200, 0, 200, 0);
    SetLastError(0xBAD);
    CCF(SetConsoleWindowInfo(b, FALSE, &d), "err=%lu", GetLastError());
    CloseHandle(b);
}

CT(con_swi_scroll_viewport) {
    CREQ(SetConsoleWindowInfo); CREQ(GetConsoleScreenBufferInfo);
    BUF();
    CC(SetConsoleScreenBufferSize(b, co(80, 100)));
    SMALL_RECT r = sr(0, 50, 79, 74);
    SetLastError(0xBAD);
    CCF(SetConsoleWindowInfo(b, TRUE, &r), "err=%lu", GetLastError());
    CONSOLE_SCREEN_BUFFER_INFO i; GetConsoleScreenBufferInfo(b, &i);
    CEQ(i.srWindow.Top, 50); CEQ(i.srWindow.Bottom, 74);
    CC(coeq(i.dwCursorPosition, 0, 0));
    r = sr(0, 75, 79, 99);
    CC(SetConsoleWindowInfo(b, TRUE, &r));
    r = sr(0, 76, 79, 100);
    /* P1 EXPECT-FIX (audit: was a tolerance): position beyond the buffer is not validated by Win10 and the
       size 80x25 fits, so the call succeeds (baseline: TRUE). */
    SetLastError(0xBAD);
    CCF(SetConsoleWindowInfo(b, TRUE, &r), "err=%lu", GetLastError());
    r = sr(0, 0, 79, 24);
    CC(SetConsoleWindowInfo(b, TRUE, &r));
    CloseHandle(b);
}

CT(con_swi_larger_than_maximum) {
    CREQ(SetConsoleWindowInfo); CREQ(GetLargestConsoleWindowSize);
    BUF();
    COORD lg = GetLargestConsoleWindowSize(b);
    if (lg.X <= 0 || lg.X >= 2000) { cnote("largest window %dx%d unusable", lg.X, lg.Y); CloseHandle(b); return; }
    CC(SetConsoleScreenBufferSize(b, co(lg.X + 50, lg.Y + 50)));
    SMALL_RECT r = sr(0, 0, lg.X, 10);     /* width = largest + 1 */
    FAILS(SetConsoleWindowInfo(b, TRUE, &r), ERROR_INVALID_PARAMETER);
    r = sr(0, 0, 10, lg.Y);
    FAILS(SetConsoleWindowInfo(b, TRUE, &r), ERROR_INVALID_PARAMETER);
    CloseHandle(b);
}

CT(con_swi_invalid_handles) {
    CREQ(SetConsoleWindowInfo);
    SMALL_RECT r = sr(0, 0, 10, 10);
    HANDLE bad[4]; inhandle_bad_list(bad);
    for (int k = 0; k < 4; k++) {
        FAILS(SetConsoleWindowInfo(bad[k], TRUE, &r), ERROR_INVALID_HANDLE);
        FAILS(SetConsoleWindowInfo(bad[k], FALSE, &r), ERROR_INVALID_HANDLE);
    }
}

CT(con_swi_bool_nonzero_is_absolute) {
    CREQ(SetConsoleWindowInfo);
    BUF();
    SMALL_RECT r = sr(0, 0, 39, 9);
    BOOL ok = SetConsoleWindowInfo(b, (BOOL)5, &r);
    CCF(ok, "err=%lu", GetLastError());
    CONSOLE_SCREEN_BUFFER_INFO i; GetConsoleScreenBufferInfo(b, &i);
    CEQ(i.srWindow.Right - i.srWindow.Left, 39);
    CloseHandle(b);
}

CT(con_largest_window_size) {
    CREQ(GetLargestConsoleWindowSize);
    COORD c = GetLargestConsoleWindowSize(hout_());
    cnote("largest console window %dx%d", c.X, c.Y);
    CC(c.X > 0 && c.Y > 0);
    CONSOLE_SCREEN_BUFFER_INFO i; GetConsoleScreenBufferInfo(hout_(), &i);
    CC(i.dwMaximumWindowSize.X <= c.X || i.dwMaximumWindowSize.X <= i.dwSize.X);
    COORD d = GetLargestConsoleWindowSize(hout_());
    CC(d.X == c.X && d.Y == c.Y);
}

CT(con_largest_window_size_invalid) {
    CREQ(GetLargestConsoleWindowSize);
    HANDLE bad[3] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x1234 };
    for (int k = 0; k < 3; k++) {
        SetLastError(0xBAD);
        COORD c = GetLargestConsoleWindowSize(bad[k]);
        CC(c.X == 0 && c.Y == 0);
        CERR(ERROR_INVALID_HANDLE);
    }
    SetLastError(0xBAD);
    COORD c = GetLargestConsoleWindowSize(hin_());
    cnote("GetLargestConsoleWindowSize(input) -> %d,%d err=%lu", c.X, c.Y, GetLastError());
}

CT(con_largest_window_matches_across_buffers) {
    CREQ(GetLargestConsoleWindowSize);
    HANDLE a = mkbuf_raw(); if (!a) return;
    COORD x = GetLargestConsoleWindowSize(a), y = GetLargestConsoleWindowSize(hout_());
    CC(x.X == y.X && x.Y == y.Y);
    CloseHandle(a);
}

CT(con_set_max_window_size_stub) {
    typedef BOOL (WINAPI *smws_t)(HANDLE, COORD);
    smws_t f = (smws_t)t_proc_k32("SetConsoleMaximumWindowSize");
    CREQ(f);
    CWINESKIP("emulator forwards this to the console host");
    COORD c = co(80, 25);
    SetLastError(0xBAD);
    CEQ(f(hout_(), c), 1);
    CEQ(f(NULL, c), 1);
    CEQ(f(INVALID_HANDLE_VALUE, co(0, 0)), 1);
}

/* ---------------- cursor ---------------- */

CT(con_cursor_position_set_get) {
    CREQ(SetConsoleCursorPosition);
    BUF();
    COORD t[] = { co(0, 0), co(79, 24), co(5, 0), co(0, 24), co(40, 12), co(79, 0), co(1, 1) };
    for (size_t k = 0; k < sizeof t / sizeof t[0]; k++) {
        SetLastError(0xBAD);
        CCF(SetConsoleCursorPosition(b, t[k]), "k=%zu err=%lu", k, GetLastError());
        CCF(coeq(curpos(b), t[k].X, t[k].Y), "k=%zu got %d,%d", k, curpos(b).X, curpos(b).Y);
    }
    CloseHandle(b);
}

CT(con_cursor_position_out_of_range) {
    CREQ(SetConsoleCursorPosition);
    BUF();
    CC(SetConsoleCursorPosition(b, co(10, 10)));
    COORD bad[] = { co(80, 0), co(0, 25), co(80, 25), co(-1, 0), co(0, -1), co(-1, -1), co(32767, 32767), co(-32768, 0), co(1000, 5) };
    for (size_t k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        FAILS(SetConsoleCursorPosition(b, bad[k]), ERROR_INVALID_PARAMETER);
        CC(coeq(curpos(b), 10, 10));
    }
    CloseHandle(b);
}

CT(con_cursor_position_invalid_handles) {
    CREQ(SetConsoleCursorPosition);
    HANDLE bad[4]; inhandle_bad_list(bad);
    for (int k = 0; k < 4; k++) FAILS(SetConsoleCursorPosition(bad[k], co(0, 0)), ERROR_INVALID_HANDLE);
}

CT(con_cursor_write_after_set) {
    CREQ(SetConsoleCursorPosition);
    BUF();
    CC(SetConsoleCursorPosition(b, co(30, 10)));
    CC(wrw(b, L"here"));
    CC(rowis(b, 10, 30, L"here"));
    CC(coeq(curpos(b), 34, 10));
    CloseHandle(b);
}

CT(con_cursor_per_buffer) {
    CREQ(SetConsoleCursorPosition);
    HANDLE a = mkbuf_std(), c = mkbuf_std(); if (!a || !c) return;
    CC(SetConsoleCursorPosition(a, co(5, 5)));
    CC(coeq(curpos(c), 0, 0));
    CloseHandle(a); CloseHandle(c);
}

CT(con_cursor_info_roundtrip) {
    CREQ(GetConsoleCursorInfo); CREQ(SetConsoleCursorInfo);
    BUF();
    CONSOLE_CURSOR_INFO ci, g;
    DWORD sizes[] = { 1, 2, 25, 50, 99, 100 };
    for (size_t k = 0; k < sizeof sizes / sizeof sizes[0]; k++) {
        for (int vis = 0; vis < 2; vis++) {
            ci.dwSize = sizes[k]; ci.bVisible = vis;
            SetLastError(0xBAD);
            CCF(SetConsoleCursorInfo(b, &ci), "size %lu err=%lu", sizes[k], GetLastError());
            memset(&g, 0xCC, sizeof g);
            CC(GetConsoleCursorInfo(b, &g));
            CCF(g.dwSize == sizes[k], "size %lu got %lu", sizes[k], g.dwSize);
            CCF(g.bVisible == vis, "vis %d got %d", vis, g.bVisible);
        }
    }
    CloseHandle(b);
}

CT(con_cursor_info_invalid_size) {
    CREQ(GetConsoleCursorInfo); CREQ(SetConsoleCursorInfo);
    BUF();
    CONSOLE_CURSOR_INFO ci = { 33, TRUE }, g;
    CC(SetConsoleCursorInfo(b, &ci));
    DWORD bad[] = { 101, 150, 1000, 0x7FFFFFFF, 0xFFFFFFFF };
    for (size_t k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        ci.dwSize = bad[k]; ci.bVisible = TRUE;
        FAILS(SetConsoleCursorInfo(b, &ci), ERROR_INVALID_PARAMETER);
    }
    CC(GetConsoleCursorInfo(b, &g)); CEQ(g.dwSize, 33);
    ci.dwSize = 0;
    SetLastError(0xBAD);
    BOOL ok = SetConsoleCursorInfo(b, &ci);
    cnote("cursor size 0 -> %d err=%lu", ok, GetLastError());
    CloseHandle(b);
}

CT(con_cursor_info_bvisible_normalised) {
    CREQ(GetConsoleCursorInfo); CREQ(SetConsoleCursorInfo);
    BUF();
    CONSOLE_CURSOR_INFO ci = { 20, 7 }, g;
    CC(SetConsoleCursorInfo(b, &ci));
    CC(GetConsoleCursorInfo(b, &g));
    cnote("bVisible=7 reads back as %d", g.bVisible);
    CC(g.bVisible != 0);
    ci.bVisible = 0x100;
    CC(SetConsoleCursorInfo(b, &ci));
    CC(GetConsoleCursorInfo(b, &g));
    cnote("bVisible=0x100 reads back as %d", g.bVisible);
    CloseHandle(b);
}

CT(con_cursor_info_invalid_handles) {
    CREQ(GetConsoleCursorInfo); CREQ(SetConsoleCursorInfo);
    CONSOLE_CURSOR_INFO ci = { 25, TRUE };
    HANDLE bad[4]; inhandle_bad_list(bad);
    for (int k = 0; k < 4; k++) {
        FAILS(GetConsoleCursorInfo(bad[k], &ci), ERROR_INVALID_HANDLE);
        FAILS(SetConsoleCursorInfo(bad[k], &ci), ERROR_INVALID_HANDLE);
    }
}

CT(con_cursor_info_per_buffer) {
    CREQ(GetConsoleCursorInfo); CREQ(SetConsoleCursorInfo);
    HANDLE a = mkbuf_raw(), c = mkbuf_raw(); if (!a || !c) return;
    CONSOLE_CURSOR_INFO ci = { 77, FALSE }, g, g2;
    CC(GetConsoleCursorInfo(c, &g2));
    CC(SetConsoleCursorInfo(a, &ci));
    CC(GetConsoleCursorInfo(c, &g));
    CC(g.dwSize == g2.dwSize && g.bVisible == g2.bVisible);
    CloseHandle(a); CloseHandle(c);
}

/* ---------------- text attributes ---------------- */

CT(con_attr_all_low_byte_values) {
    CREQ(SetConsoleTextAttribute);
    BUF();
    for (int a = 0; a < 256; a++) {
        SetLastError(0xBAD);
        CCF(SetConsoleTextAttribute(b, (WORD)a), "attr 0x%x err=%lu", a, GetLastError());
        CCF(attrof(b) == a, "attr 0x%x got 0x%x", a, attrof(b));
    }
    CloseHandle(b);
}

CT(con_attr_lvb_bits) {
    CREQ(SetConsoleTextAttribute);
    BUF();
    WORD v[] = { COMMON_LVB_REVERSE_VIDEO | 7, COMMON_LVB_UNDERSCORE | 7, COMMON_LVB_REVERSE_VIDEO | COMMON_LVB_UNDERSCORE | 0x1F,
        COMMON_LVB_GRID_HORIZONTAL | 7, COMMON_LVB_GRID_LVERTICAL | 7, COMMON_LVB_GRID_RVERTICAL | 7, COMMON_LVB_LEADING_BYTE | 7, COMMON_LVB_TRAILING_BYTE | 7 };
    for (size_t k = 0; k < sizeof v / sizeof v[0]; k++) {
        SetLastError(0xBAD);
        BOOL ok = SetConsoleTextAttribute(b, v[k]);
        WORD got = attrof(b);
        cnote("SetConsoleTextAttribute(0x%x) -> %d got 0x%x", v[k], ok, got);
        CCF(ok, "attr 0x%x err=%lu", v[k], GetLastError());
    }
    CloseHandle(b);
}

CT(con_attr_reverse_underscore_stored_in_cells) {
    CREQ(SetConsoleTextAttribute);
    BUF();
    CC(SetConsoleTextAttribute(b, COMMON_LVB_REVERSE_VIDEO | 0x17));
    CC(wrw(b, L"R"));
    WORD a = cellattr(b, 0, 0);
    cnote("cell attr with reverse = 0x%x", a);
    CEQ(a & 0xFF, 0x17);
    CC(SetConsoleTextAttribute(b, COMMON_LVB_UNDERSCORE | 7));
    CC(wrw(b, L"U"));
    a = cellattr(b, 1, 0);
    cnote("cell attr with underscore = 0x%x", a);
    CEQ(a & 0xFF, 7);
    CloseHandle(b);
}

CT(con_attr_invalid_handles) {
    CREQ(SetConsoleTextAttribute);
    HANDLE bad[4]; inhandle_bad_list(bad);
    for (int k = 0; k < 4; k++) FAILS(SetConsoleTextAttribute(bad[k], 7), ERROR_INVALID_HANDLE);
}

CT(con_attr_high_garbage_bits) {
    CREQ(SetConsoleTextAttribute);
    BUF();
    WORD v[] = { 0x2007, 0xFFFF, 0x0010 | 0x2000, 0x8000 };
    for (size_t k = 0; k < sizeof v / sizeof v[0]; k++) {
        SetLastError(0xBAD);
        BOOL ok = SetConsoleTextAttribute(b, v[k]);
        DWORD e = GetLastError();
        cnote("SetConsoleTextAttribute(0x%x) -> %d err=%lu now=0x%x", v[k], ok, e, attrof(b));
        if (!ok) CEQ(e, ERROR_INVALID_PARAMETER);
        SetConsoleTextAttribute(b, 7);
    }
    CloseHandle(b);
}

CT(con_attr_does_not_recolor_existing_cells) {
    CREQ(SetConsoleTextAttribute);
    BUF();
    CC(wrw(b, L"old"));
    CC(SetConsoleTextAttribute(b, 0x4F));
    CEQ(cellattr(b, 0, 0), 7);
    CC(wrw(b, L"new"));
    CEQ(cellattr(b, 3, 0), 0x4F);
    CEQ(cellattr(b, 0, 0), 7);
    CloseHandle(b);
}

/* ---------------- FillConsoleOutputCharacter / Attribute ---------------- */

CT(con_fill_char_basic) {
    CREQ(FillConsoleOutputCharacterW);
    BUF();
    DWORD n = 0xFFFF;
    SetLastError(0xBAD);
    CC(FillConsoleOutputCharacterW(b, L'x', 10, co(5, 2), &n));
    CEQ(n, 10);
    CC(rowis(b, 2, 5, L"xxxxxxxxxx"));
    CEQ(cellch(b, 4, 2), ' '); CEQ(cellch(b, 15, 2), ' ');
    CC(coeq(curpos(b), 0, 0));
    CloseHandle(b);
}

CT(con_fill_char_a_variant) {
    CREQ(FillConsoleOutputCharacterA); CREQ(SetConsoleOutputCP);
    UINT old = GetConsoleOutputCP();
    BUF();
    DWORD n = 0xFFFF;
    CC(FillConsoleOutputCharacterA(b, 'q', 5, co(0, 0), &n)); CEQ(n, 5);
    CC(rowis(b, 0, 0, L"qqqqq"));
    CC(SetConsoleOutputCP(437));
    CC(FillConsoleOutputCharacterA(b, (CHAR)0x82, 3, co(0, 1), &n)); CEQ(n, 3);
    CEQ(cellch(b, 0, 1), 0xE9); CEQ(cellch(b, 2, 1), 0xE9);
    CC(SetConsoleOutputCP(1252));
    CC(FillConsoleOutputCharacterA(b, (CHAR)0xE9, 3, co(0, 2), &n));
    CEQ(cellch(b, 1, 2), 0xE9);
    SetConsoleOutputCP(old);
    CloseHandle(b);
}

CT(con_fill_char_wraps_rows) {
    CREQ(FillConsoleOutputCharacterW);
    BUF();
    DWORD n = 0;
    CC(FillConsoleOutputCharacterW(b, L'w', 100, co(70, 0), &n)); CEQ(n, 100);
    CEQ(cellch(b, 69, 0), ' '); CEQ(cellch(b, 70, 0), 'w'); CEQ(cellch(b, 79, 0), 'w');
    CEQ(cellch(b, 0, 1), 'w'); CEQ(cellch(b, 79, 1), 'w');
    CEQ(cellch(b, 9, 2), 'w'); CEQ(cellch(b, 10, 2), ' ');
    CloseHandle(b);
}

CT(con_fill_char_clipped_at_end) {
    CREQ(FillConsoleOutputCharacterW);
    BUF();
    DWORD n = 0xFFFF;
    CC(FillConsoleOutputCharacterW(b, L'e', 1000, co(70, 24), &n));
    CEQ(n, 10);
    CEQ(cellch(b, 79, 24), 'e'); CEQ(cellch(b, 69, 24), ' ');
    n = 0xFFFF;
    CC(FillConsoleOutputCharacterW(b, L'f', 0xFFFFFFFF, co(0, 0), &n));
    CCF(n == 2000, "n=%lu", n);
    CEQ(cellch(b, 79, 24), 'f');
    n = 0xFFFF;
    CC(FillConsoleOutputCharacterW(b, L'g', 0x7FFFFFFF, co(0, 24), &n));
    CEQ(n, 80);
    CloseHandle(b);
}

CT(con_fill_char_zero_length) {
    CREQ(FillConsoleOutputCharacterW);
    BUF();
    DWORD n = 0xFFFF;
    SetLastError(0xBAD);
    CC(FillConsoleOutputCharacterW(b, L'z', 0, co(0, 0), &n));
    CEQ(n, 0);
    CEQ(cellch(b, 0, 0), ' ');
    CloseHandle(b);
}

CT(con_fill_char_out_of_range_origin) {
    CREQ(FillConsoleOutputCharacterW);
    BUF();
    COORD bad[] = { co(80, 0), co(0, 25), co(-1, 0), co(0, -1), co(32767, 32767) };
    for (size_t k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        DWORD n = 0xFFFF;
        SetLastError(0xBAD);
        BOOL r = FillConsoleOutputCharacterW(b, L'!', 5, bad[k], &n);
        cnote("fill char origin %d,%d -> %d err=%lu n=%lu", bad[k].X, bad[k].Y, r, GetLastError(), n);
        if (!r) CERR(ERROR_INVALID_PARAMETER); else CCF(n == 0, "n=%lu", n);
        if (!r) CERR(ERROR_INVALID_PARAMETER);
    }
    CEQ(cellch(b, 0, 0), ' ');
    CEQ(cellch(b, 79, 24), ' ');
    CloseHandle(b);
}

CT(con_fill_char_does_not_touch_attrs_or_cursor) {
    CREQ(FillConsoleOutputCharacterW);
    BUF();
    CC(SetConsoleTextAttribute(b, 0x4E));
    CC(SetConsoleCursorPosition(b, co(3, 3)));
    DWORD n;
    CC(FillConsoleOutputCharacterW(b, L'c', 10, co(0, 0), &n));
    CEQ(cellattr(b, 0, 0), 7);
    CC(coeq(curpos(b), 3, 3));
    CEQ(attrof(b), 0x4E);
    CloseHandle(b);
}

CT(con_fill_char_invalid_handles) {
    CREQ(FillConsoleOutputCharacterW); CREQ(FillConsoleOutputCharacterA);
    DWORD n = 0xAA;
    HANDLE bad[4]; inhandle_bad_list(bad);
    for (int k = 0; k < 4; k++) {
        FAILS(FillConsoleOutputCharacterW(bad[k], L'a', 1, co(0, 0), &n), ERROR_INVALID_HANDLE);
        FAILS(FillConsoleOutputCharacterA(bad[k], 'a', 1, co(0, 0), &n), ERROR_INVALID_HANDLE);
    }
}

CT(con_fill_char_unicode) {
    CREQ(FillConsoleOutputCharacterW);
    BUF();
    DWORD n;
    CC(FillConsoleOutputCharacterW(b, 0x416, 4, co(0, 0), &n)); CEQ(n, 4);
    CEQ(cellch(b, 0, 0), 0x416); CEQ(cellch(b, 3, 0), 0x416);
    CC(FillConsoleOutputCharacterW(b, 0x20AC, 2, co(0, 1), &n));
    CEQ(cellch(b, 1, 1), 0x20AC);
    CloseHandle(b);
}

CT(con_fill_char_wide_char) {
    CREQ(FillConsoleOutputCharacterW);
    BUF();
    DWORD n = 0;
    CC(FillConsoleOutputCharacterW(b, 0x4E2D, 3, co(0, 0), &n));
    cnote("fill with CJK x3 -> n=%lu cells: %x %x %x %x %x %x", n, cellch(b, 0, 0), cellch(b, 1, 0), cellch(b, 2, 0), cellch(b, 3, 0), cellch(b, 4, 0), cellch(b, 5, 0));
    CloseHandle(b);
}

CT(con_fill_attr_basic) {
    CREQ(FillConsoleOutputAttribute);
    BUF();
    DWORD n = 0xFFFF;
    SetLastError(0xBAD);
    CC(wrw(b, L"text"));
    CC(FillConsoleOutputAttribute(b, 0x2C, 6, co(0, 0), &n));
    CEQ(n, 6);
    for (int x = 0; x < 6; x++) CEQ(cellattr(b, x, 0), 0x2C);
    CEQ(cellattr(b, 6, 0), 7);
    CC(rowis(b, 0, 0, L"text"));
    CC(coeq(curpos(b), 4, 0));
    CloseHandle(b);
}

CT(con_fill_attr_wraps_and_clips) {
    CREQ(FillConsoleOutputAttribute);
    BUF();
    DWORD n = 0;
    CC(FillConsoleOutputAttribute(b, 0x30, 60, co(75, 0), &n)); CEQ(n, 60);
    CEQ(cellattr(b, 74, 0), 7); CEQ(cellattr(b, 75, 0), 0x30); CEQ(cellattr(b, 4, 1), 0x30); CEQ(cellattr(b, 54, 1), 0x30); CEQ(cellattr(b, 55, 1), 7);
    n = 0xFFFF;
    CC(FillConsoleOutputAttribute(b, 0x50, 5000, co(60, 24), &n)); CEQ(n, 20);
    CEQ(cellattr(b, 79, 24), 0x50);
    CloseHandle(b);
}

CT(con_fill_attr_zero_length_and_range) {
    CREQ(FillConsoleOutputAttribute);
    BUF();
    DWORD n = 0xFFFF;
    CC(FillConsoleOutputAttribute(b, 0x70, 0, co(0, 0), &n)); CEQ(n, 0);
    COORD bad[] = { co(80, 0), co(0, 25), co(-1, 0), co(0, -1) };
    for (size_t k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        n = 0xFFFF; SetLastError(0xBAD);
        BOOL r = FillConsoleOutputAttribute(b, 0x70, 5, bad[k], &n);
        cnote("fill attr origin %d,%d -> %d err=%lu n=%lu", bad[k].X, bad[k].Y, r, GetLastError(), n);
        if (!r) CERR(ERROR_INVALID_PARAMETER); else CCF(n == 0, "n=%lu", n);
    }
    CEQ(cellattr(b, 0, 0), 7);
    CloseHandle(b);
}

CT(con_fill_attr_invalid_handles) {
    CREQ(FillConsoleOutputAttribute);
    DWORD n = 0xAA;
    HANDLE bad[4]; inhandle_bad_list(bad);
    for (int k = 0; k < 4; k++) FAILS(FillConsoleOutputAttribute(bad[k], 7, 1, co(0, 0), &n), ERROR_INVALID_HANDLE);
}

CT(con_fill_attr_full_word_stored) {
    CREQ(FillConsoleOutputAttribute);
    BUF();
    DWORD n;
    WORD v[] = { 0x00FF, 0x4007, 0x8007, COMMON_LVB_GRID_HORIZONTAL | 7 };
    for (size_t k = 0; k < sizeof v / sizeof v[0]; k++) {
        CC(FillConsoleOutputAttribute(b, v[k], 1, co((int)k, 0), &n));
        cnote("fill attr 0x%x reads back 0x%x", v[k], cellattr(b, (int)k, 0));
    }
    CEQ(cellattr(b, 0, 0), 0xFF);
    CloseHandle(b);
}

/* ---------------- WriteConsoleOutputCharacter / Attribute ---------------- */

CT(con_wcoc_basic) {
    CREQ(WriteConsoleOutputCharacterW);
    BUF();
    DWORD n = 0xFFFF;
    CC(SetConsoleCursorPosition(b, co(9, 9)));
    SetLastError(0xBAD);
    CC(WriteConsoleOutputCharacterW(b, L"abc", 3, co(3, 3), &n));
    CEQ(n, 3);
    CC(rowis(b, 3, 3, L"abc"));
    CC(coeq(curpos(b), 9, 9));
    CEQ(cellattr(b, 3, 3), 7);
    CloseHandle(b);
}

CT(con_wcoc_keeps_attributes) {
    CREQ(WriteConsoleOutputCharacterW);
    BUF();
    DWORD n;
    CC(FillConsoleOutputAttribute(b, 0x2A, 5, co(0, 0), &n));
    CC(WriteConsoleOutputCharacterW(b, L"hello", 5, co(0, 0), &n));
    CEQ(cellattr(b, 0, 0), 0x2A); CEQ(cellattr(b, 4, 0), 0x2A);
    CC(rowis(b, 0, 0, L"hello"));
    CloseHandle(b);
}

CT(con_wcoc_wraps_and_clips) {
    CREQ(WriteConsoleOutputCharacterW);
    BUF();
    DWORD n = 0;
    CC(WriteConsoleOutputCharacterW(b, L"abcd", 4, co(78, 0), &n)); CEQ(n, 4);
    CEQ(cellch(b, 78, 0), 'a'); CEQ(cellch(b, 79, 0), 'b'); CEQ(cellch(b, 0, 1), 'c'); CEQ(cellch(b, 1, 1), 'd');
    n = 0xFFFF;
    CC(WriteConsoleOutputCharacterW(b, L"WXYZ", 4, co(78, 24), &n));
    CEQ(n, 2);
    CEQ(cellch(b, 78, 24), 'W'); CEQ(cellch(b, 79, 24), 'X');
    CloseHandle(b);
}

CT(con_wcoc_control_chars_not_interpreted) {
    CREQ(WriteConsoleOutputCharacterW);
    BUF();
    DWORD n = 0;
    CC(WriteConsoleOutputCharacterW(b, L"a\nb\rc", 5, co(0, 0), &n));
    CEQ(n, 5);
    CEQ(cellch(b, 0, 0), 'a'); CEQ(cellch(b, 2, 0), 'b'); CEQ(cellch(b, 4, 0), 'c');
    cnote("cells 1 and 3: 0x%x 0x%x", cellch(b, 1, 0), cellch(b, 3, 0));
    CC(coeq(curpos(b), 0, 0));
    CEQ(cellch(b, 0, 1), ' ');
    CloseHandle(b);
}

CT(con_wcoc_zero_length_and_range) {
    CREQ(WriteConsoleOutputCharacterW);
    BUF();
    DWORD n = 0xFFFF;
    CC(WriteConsoleOutputCharacterW(b, L"x", 0, co(0, 0), &n)); CEQ(n, 0);
    COORD bad[] = { co(80, 0), co(0, 25), co(-1, 0), co(0, -1) };
    for (size_t k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        n = 0xFFFF; SetLastError(0xBAD);
        BOOL r = WriteConsoleOutputCharacterW(b, L"xyz", 3, bad[k], &n);
        DWORD e_ = GetLastError();   /* P1 GUARD: capture before cnote, whose printf clobbers the last error */
        cnote("wcoc origin %d,%d -> %d err=%lu n=%lu", bad[k].X, bad[k].Y, r, e_, n);
        if (!r) CERRV(e_, ERROR_INVALID_PARAMETER); else CCF(n == 0, "n=%lu", n);
    }
    CEQ(cellch(b, 0, 0), ' ');
    CloseHandle(b);
}

CT(con_wcoc_a_variant) {
    CREQ(WriteConsoleOutputCharacterA); CREQ(SetConsoleOutputCP);
    UINT old = GetConsoleOutputCP();
    BUF();
    DWORD n = 0;
    CC(WriteConsoleOutputCharacterA(b, "ansi", 4, co(0, 0), &n)); CEQ(n, 4);
    CC(rowis(b, 0, 0, L"ansi"));
    CC(SetConsoleOutputCP(437));
    CC(WriteConsoleOutputCharacterA(b, "\x82\x81", 2, co(0, 1), &n)); CEQ(n, 2);
    CEQ(cellch(b, 0, 1), 0xE9); CEQ(cellch(b, 1, 1), 0xFC);
    SetConsoleOutputCP(old);
    CloseHandle(b);
}

CT(con_wcoc_invalid_handles) {
    CREQ(WriteConsoleOutputCharacterW); CREQ(WriteConsoleOutputCharacterA);
    DWORD n = 0xAA;
    HANDLE bad[4]; inhandle_bad_list(bad);
    for (int k = 0; k < 4; k++) {
        FAILS(WriteConsoleOutputCharacterW(bad[k], L"a", 1, co(0, 0), &n), ERROR_INVALID_HANDLE);
        FAILS(WriteConsoleOutputCharacterA(bad[k], "a", 1, co(0, 0), &n), ERROR_INVALID_HANDLE);
    }
}

CT(con_wcoc_unicode) {
    CREQ(WriteConsoleOutputCharacterW);
    BUF();
    DWORD n = 0;
    static const WCHAR s[] = { 0xE9, 0x416, 0x3A9, 0x20AC, 0 };
    CC(WriteConsoleOutputCharacterW(b, s, 4, co(10, 10), &n)); CEQ(n, 4);
    WCHAR r[8]; rowtext(b, 10, 10, 4, r);
    CWSTR(r, s);
    CloseHandle(b);
}

CT(con_wcoa_basic) {
    CREQ(WriteConsoleOutputAttribute);
    BUF();
    WORD a[4] = { 0x1F, 0x2E, 0x3D, 0x4C };
    DWORD n = 0xFFFF;
    CC(wrw(b, L"abcd"));
    CC(SetConsoleCursorPosition(b, co(9, 9)));
    SetLastError(0xBAD);
    CC(WriteConsoleOutputAttribute(b, a, 4, co(0, 0), &n)); CEQ(n, 4);
    for (int i = 0; i < 4; i++) CEQ(cellattr(b, i, 0), a[i]);
    CC(rowis(b, 0, 0, L"abcd"));
    CC(coeq(curpos(b), 9, 9));
    CloseHandle(b);
}

CT(con_wcoa_wraps_and_clips) {
    CREQ(WriteConsoleOutputAttribute);
    BUF();
    WORD a[6] = { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 };
    DWORD n = 0;
    CC(WriteConsoleOutputAttribute(b, a, 6, co(77, 0), &n)); CEQ(n, 6);
    CEQ(cellattr(b, 79, 0), 0x30); CEQ(cellattr(b, 0, 1), 0x40); CEQ(cellattr(b, 2, 1), 0x60);
    n = 0xFFFF;
    CC(WriteConsoleOutputAttribute(b, a, 6, co(78, 24), &n)); CEQ(n, 2);
    CEQ(cellattr(b, 79, 24), 0x20);
    CloseHandle(b);
}

CT(con_wcoa_zero_and_invalid) {
    CREQ(WriteConsoleOutputAttribute);
    BUF();
    WORD a[2] = { 0x70, 0x70 }; DWORD n = 0xFFFF;
    CC(WriteConsoleOutputAttribute(b, a, 0, co(0, 0), &n)); CEQ(n, 0);
    COORD bad[] = { co(80, 0), co(0, 25), co(-1, 0), co(0, -1) };
    for (size_t k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        n = 0xFFFF; SetLastError(0xBAD);
        BOOL r = WriteConsoleOutputAttribute(b, a, 2, bad[k], &n);
        DWORD e_ = GetLastError();   /* P1 GUARD: capture before cnote, whose printf clobbers the last error */
        cnote("wcoa origin %d,%d -> %d err=%lu n=%lu", bad[k].X, bad[k].Y, r, e_, n);
        if (!r) CERRV(e_, ERROR_INVALID_PARAMETER); else CCF(n == 0, "n=%lu", n);
    }
    HANDLE hb[4]; inhandle_bad_list(hb);
    for (int k = 0; k < 4; k++) FAILS(WriteConsoleOutputAttribute(hb[k], a, 2, co(0, 0), &n), ERROR_INVALID_HANDLE);
    CloseHandle(b);
}

/* ---------------- ReadConsoleOutputCharacter / Attribute ---------------- */

CT(con_rcoc_basic) {
    CREQ(ReadConsoleOutputCharacterW); CREQ(ReadConsoleOutputCharacterA);
    BUF();
    CC(wrw(b, L"readme"));
    WCHAR w[16]; char a[16]; DWORD n = 0xFFFF;
    memset(w, 0xAA, sizeof w);
    CC(ReadConsoleOutputCharacterW(b, w, 6, co(0, 0), &n)); CEQ(n, 6);
    CC(!memcmp(w, L"readme", 12));
    CEQ(w[6], 0xAAAA);   /* no terminator written */
    memset(a, 0x55, sizeof a);
    CC(ReadConsoleOutputCharacterA(b, a, 6, co(0, 0), &n)); CEQ(n, 6);
    CC(!memcmp(a, "readme", 6));
    CEQ((unsigned char)a[6], 0x55);
    CC(ReadConsoleOutputCharacterW(b, w, 4, co(2, 0), &n)); CEQ(n, 4);
    CC(!memcmp(w, L"adme", 8));
    CloseHandle(b);
}

CT(con_rcoc_wraps_rows) {
    CREQ(ReadConsoleOutputCharacterW);
    BUF();
    DWORD n;
    FillConsoleOutputCharacterW(b, L'1', 80, co(0, 0), &n);
    FillConsoleOutputCharacterW(b, L'2', 80, co(0, 1), &n);
    WCHAR w[200];
    CC(ReadConsoleOutputCharacterW(b, w, 20, co(70, 0), &n)); CEQ(n, 20);
    for (int i = 0; i < 10; i++) CEQ(w[i], '1');
    for (int i = 10; i < 20; i++) CEQ(w[i], '2');
    CC(ReadConsoleOutputCharacterW(b, w, 160, co(0, 0), &n)); CEQ(n, 160);
    CEQ(w[79], '1'); CEQ(w[80], '2');
    CloseHandle(b);
}

CT(con_rcoc_clipped_at_end) {
    CREQ(ReadConsoleOutputCharacterW);
    BUF();
    static WCHAR w[2100];
    DWORD n = 0xFFFF;
    CC(ReadConsoleOutputCharacterW(b, w, 2100, co(0, 0), &n)); CEQ(n, 2000);
    CC(ReadConsoleOutputCharacterW(b, w, 50, co(70, 24), &n)); CEQ(n, 10);
    CC(ReadConsoleOutputCharacterW(b, w, 1, co(79, 24), &n)); CEQ(n, 1);
    CloseHandle(b);
}

CT(con_rcoc_zero_and_range) {
    CREQ(ReadConsoleOutputCharacterW);
    BUF();
    WCHAR w[4]; DWORD n = 0xFFFF;
    CC(ReadConsoleOutputCharacterW(b, w, 0, co(0, 0), &n)); CEQ(n, 0);
    COORD bad[] = { co(80, 0), co(0, 25), co(-1, 0), co(0, -1) };
    for (size_t k = 0; k < sizeof bad / sizeof bad[0]; k++) {
        n = 0xFFFF; SetLastError(0xBAD);
        BOOL r = ReadConsoleOutputCharacterW(b, w, 4, bad[k], &n);
        cnote("rcoc origin %d,%d -> %d err=%lu n=%lu", bad[k].X, bad[k].Y, r, GetLastError(), n);
        if (!r) CERR(ERROR_INVALID_PARAMETER); else CCF(n == 0, "n=%lu", n);
    }
    CloseHandle(b);
}

CT(con_rcoc_invalid_handles) {
    CREQ(ReadConsoleOutputCharacterW); CREQ(ReadConsoleOutputCharacterA); CREQ(ReadConsoleOutputAttribute);
    WCHAR w[4]; char a[4]; WORD at[4]; DWORD n = 0xAA;
    HANDLE bad[4]; inhandle_bad_list(bad);
    for (int k = 0; k < 4; k++) {
        FAILS(ReadConsoleOutputCharacterW(bad[k], w, 4, co(0, 0), &n), ERROR_INVALID_HANDLE);
        FAILS(ReadConsoleOutputCharacterA(bad[k], a, 4, co(0, 0), &n), ERROR_INVALID_HANDLE);
        FAILS(ReadConsoleOutputAttribute(bad[k], at, 4, co(0, 0), &n), ERROR_INVALID_HANDLE);
    }
}

CT(con_rcoc_a_codepage) {
    CREQ(ReadConsoleOutputCharacterA); CREQ(SetConsoleOutputCP);
    UINT old = GetConsoleOutputCP();
    BUF();
    DWORD n;
    FillConsoleOutputCharacterW(b, 0xE9, 2, co(0, 0), &n);
    char a[4] = { 0 };
    CC(SetConsoleOutputCP(437));
    CC(ReadConsoleOutputCharacterA(b, a, 2, co(0, 0), &n)); CEQ(n, 2);
    CEQ((unsigned char)a[0], 0x82);
    CC(SetConsoleOutputCP(1252));
    CC(ReadConsoleOutputCharacterA(b, a, 2, co(0, 0), &n));
    CEQ((unsigned char)a[0], 0xE9);
    CC(SetConsoleOutputCP(65001));
    char u[8] = { 0 };
    CC(ReadConsoleOutputCharacterA(b, u, 2, co(0, 0), &n));
    cnote("UTF-8 ReadConsoleOutputCharacterA: n=%lu bytes %02x %02x %02x %02x", n, (unsigned char)u[0], (unsigned char)u[1], (unsigned char)u[2], (unsigned char)u[3]);
    SetConsoleOutputCP(old);
    CloseHandle(b);
}

CT(con_rcoa_basic) {
    CREQ(ReadConsoleOutputAttribute);
    BUF();
    DWORD n;
    FillConsoleOutputAttribute(b, 0x2B, 3, co(4, 4), &n);
    WORD at[8]; memset(at, 0xAA, sizeof at);
    n = 0xFFFF;
    CC(ReadConsoleOutputAttribute(b, at, 8, co(2, 4), &n)); CEQ(n, 8);
    CEQ(at[0], 7); CEQ(at[1], 7); CEQ(at[2], 0x2B); CEQ(at[4], 0x2B); CEQ(at[5], 7);
    CloseHandle(b);
}

CT(con_rcoa_wraps_and_clips) {
    CREQ(ReadConsoleOutputAttribute);
    BUF();
    DWORD n;
    FillConsoleOutputAttribute(b, 0x11, 80, co(0, 0), &n);
    FillConsoleOutputAttribute(b, 0x22, 80, co(0, 1), &n);
    WORD at[200];
    CC(ReadConsoleOutputAttribute(b, at, 20, co(70, 0), &n)); CEQ(n, 20);
    CEQ(at[9], 0x11); CEQ(at[10], 0x22);
    CC(ReadConsoleOutputAttribute(b, at, 200, co(40, 24), &n)); CEQ(n, 40);
    CC(ReadConsoleOutputAttribute(b, at, 0, co(0, 0), &n)); CEQ(n, 0);
    CloseHandle(b);
}

/* ---------------- WriteConsoleOutput / ReadConsoleOutput ---------------- */

static CHAR_INFO ci(WCHAR c, WORD a) { CHAR_INFO x; x.Char.UnicodeChar = c; x.Attributes = a; return x; }

CT(con_wco_basic_rect) {
    CREQ(WriteConsoleOutputW);
    BUF();
    CHAR_INFO buf[6];
    for (int i = 0; i < 6; i++) buf[i] = ci(L'a' + i, 0x10 + i);
    SMALL_RECT r = sr(2, 3, 4, 4);   /* 3 x 2 */
    SetLastError(0xBAD);
    CCF(WriteConsoleOutputW(b, buf, co(3, 2), co(0, 0), &r), "err=%lu", GetLastError());
    CC(r.Left == 2 && r.Top == 3 && r.Right == 4 && r.Bottom == 4);
    CC(rowis(b, 3, 2, L"abc"));
    CC(rowis(b, 4, 2, L"def"));
    CEQ(cellattr(b, 2, 3), 0x10); CEQ(cellattr(b, 4, 3), 0x12); CEQ(cellattr(b, 2, 4), 0x13); CEQ(cellattr(b, 4, 4), 0x15);
    CEQ(cellch(b, 1, 3), ' '); CEQ(cellch(b, 5, 3), ' ');
    CC(coeq(curpos(b), 0, 0));
    CloseHandle(b);
}

CT(con_wco_clipping_returns_rect) {
    CREQ(WriteConsoleOutputW);
    BUF();
    CHAR_INFO buf[16];
    for (int i = 0; i < 16; i++) buf[i] = ci(L'A' + i, 0x1F);
    SMALL_RECT r = sr(78, 23, 81, 26);   /* 4 x 4, buffer is 80 x 25 */
    CC(WriteConsoleOutputW(b, buf, co(4, 4), co(0, 0), &r));
    CCF(r.Left == 78 && r.Top == 23 && r.Right == 79 && r.Bottom == 24, "rect %d,%d-%d,%d", r.Left, r.Top, r.Right, r.Bottom);
    CEQ(cellch(b, 78, 23), 'A'); CEQ(cellch(b, 79, 23), 'B'); CEQ(cellch(b, 78, 24), 'E'); CEQ(cellch(b, 79, 24), 'F');
    CloseHandle(b);
}

CT(con_wco_buffer_coord_offset) {
    CREQ(WriteConsoleOutputW);
    BUF();
    CHAR_INFO buf[9];
    for (int i = 0; i < 9; i++) buf[i] = ci(L'a' + i, 7);   /* 3x3: abc / def / ghi */
    SMALL_RECT r = sr(10, 10, 11, 11);    /* 2x2 from source (1,1) */
    CC(WriteConsoleOutputW(b, buf, co(3, 3), co(1, 1), &r));
    CC(rowis(b, 10, 10, L"ef"));
    CC(rowis(b, 11, 10, L"hi"));
    CC(r.Left == 10 && r.Right == 11 && r.Top == 10 && r.Bottom == 11);
    CloseHandle(b);
}

CT(con_wco_outside_buffer) {
    CREQ(WriteConsoleOutputW);
    BUF();
    CHAR_INFO buf[4];
    for (int i = 0; i < 4; i++) buf[i] = ci(L'x', 7);
    SMALL_RECT r = sr(100, 100, 101, 101);
    SetLastError(0xBAD);
    BOOL ok = WriteConsoleOutputW(b, buf, co(2, 2), co(0, 0), &r);
    DWORD e = GetLastError();
    cnote("rect outside buffer -> %d err=%lu out %d,%d-%d,%d", ok, e, r.Left, r.Top, r.Right, r.Bottom);
    if (!ok) CEQ(e, ERROR_INVALID_PARAMETER);
    CEQ(cellch(b, 79, 24), ' ');
    CloseHandle(b);
}

CT(con_wco_inverted_rect) {
    CREQ(WriteConsoleOutputW);
    BUF();
    CHAR_INFO buf[4];
    for (int i = 0; i < 4; i++) buf[i] = ci(L'x', 7);
    SMALL_RECT r = sr(5, 5, 3, 3);
    SetLastError(0xBAD);
    BOOL ok = WriteConsoleOutputW(b, buf, co(2, 2), co(0, 0), &r);
    DWORD e = GetLastError();
    cnote("inverted rect -> %d err=%lu", ok, e);
    CEQ(cellch(b, 3, 3), ' '); CEQ(cellch(b, 5, 5), ' ');
    CloseHandle(b);
}

CT(con_wco_a_variant) {
    CREQ(WriteConsoleOutputA); CREQ(SetConsoleOutputCP);
    UINT old = GetConsoleOutputCP();
    BUF();
    CHAR_INFO buf[3];
    buf[0].Char.AsciiChar = 'A'; buf[0].Attributes = 0x1E;
    buf[1].Char.AsciiChar = (CHAR)0x82; buf[1].Attributes = 0x2D;
    buf[2].Char.AsciiChar = 'Z'; buf[2].Attributes = 0x3C;
    SMALL_RECT r = sr(0, 0, 2, 0);
    CC(SetConsoleOutputCP(437));
    CC(WriteConsoleOutputA(b, buf, co(3, 1), co(0, 0), &r));
    CEQ(cellch(b, 0, 0), 'A'); CEQ(cellch(b, 1, 0), 0xE9); CEQ(cellch(b, 2, 0), 'Z');
    CEQ(cellattr(b, 1, 0), 0x2D);
    SetConsoleOutputCP(old);
    CloseHandle(b);
}

CT(con_wco_invalid_handles) {
    CREQ(WriteConsoleOutputW); CREQ(WriteConsoleOutputA); CREQ(ReadConsoleOutputW); CREQ(ReadConsoleOutputA);
    CHAR_INFO buf[4]; memset(buf, 0, sizeof buf);
    HANDLE bad[4]; inhandle_bad_list(bad);
    for (int k = 0; k < 4; k++) {
        SMALL_RECT r = sr(0, 0, 1, 1);
        /* P1b EXPECT-FIX: lpWriteRegion is in/out (WriteConsoleOutput documentation); kernelbase
           WriteConsoleOutputInternal stores the server's reply rectangle back into it even when the call failed, so
           a failed call leaves an unspecified rectangle (baseline: 87 on the next call for handles 0 and 0x44, whose
           failure comes from the server). Reinitialise the rectangle before every call. UNVERIFIED-until-rerun */
        { r = sr(0, 0, 1, 1); SetLastError(0xBAD); BOOL w_ = WriteConsoleOutputA(bad[k], buf, co(2, 2), co(0, 0), &r); DWORD e_ = GetLastError();
          cnote("WriteConsoleOutputA bad[%d]=%p -> %d err=%lu", k, bad[k], w_, e_); }
        r = sr(0, 0, 1, 1); FAILS(WriteConsoleOutputW(bad[k], buf, co(2, 2), co(0, 0), &r), ERROR_INVALID_HANDLE);
        r = sr(0, 0, 1, 1); FAILS(WriteConsoleOutputA(bad[k], buf, co(2, 2), co(0, 0), &r), ERROR_INVALID_HANDLE);
        r = sr(0, 0, 1, 1); FAILS(ReadConsoleOutputW(bad[k], buf, co(2, 2), co(0, 0), &r), ERROR_INVALID_HANDLE);
        r = sr(0, 0, 1, 1);
        FAILS(ReadConsoleOutputA(bad[k], buf, co(2, 2), co(0, 0), &r), ERROR_INVALID_HANDLE);
    }
}

CT(con_wco_single_cell) {
    CREQ(WriteConsoleOutputW);
    BUF();
    CHAR_INFO c = ci(L'#', 0x4F);
    SMALL_RECT r = sr(79, 24, 79, 24);
    CC(WriteConsoleOutputW(b, &c, co(1, 1), co(0, 0), &r));
    CEQ(cellch(b, 79, 24), '#'); CEQ(cellattr(b, 79, 24), 0x4F);
    CC(r.Left == 79 && r.Top == 24 && r.Right == 79 && r.Bottom == 24);
    CloseHandle(b);
}

CT(con_wco_full_screen) {
    CREQ(WriteConsoleOutputW); CREQ(ReadConsoleOutputW);
    BUF();
    static CHAR_INFO buf[2000], rd[2000];
    for (int i = 0; i < 2000; i++) buf[i] = ci(L'a' + (i % 26), (WORD)(i % 256));
    SMALL_RECT r = sr(0, 0, 79, 24);
    CC(WriteConsoleOutputW(b, buf, co(80, 25), co(0, 0), &r));
    SMALL_RECT r2 = sr(0, 0, 79, 24);
    CC(ReadConsoleOutputW(b, rd, co(80, 25), co(0, 0), &r2));
    int bad = 0;
    for (int i = 0; i < 2000; i++) if (rd[i].Char.UnicodeChar != buf[i].Char.UnicodeChar || rd[i].Attributes != buf[i].Attributes) bad++;
    CCF(bad == 0, "%d mismatching cells", bad);
    CloseHandle(b);
}

CT(con_wco_keeps_cursor_and_default_attr) {
    CREQ(WriteConsoleOutputW);
    BUF();
    CC(SetConsoleCursorPosition(b, co(7, 7)));
    CC(SetConsoleTextAttribute(b, 0x2F));
    CHAR_INFO c = ci(L'k', 0x13);
    SMALL_RECT r = sr(0, 0, 0, 0);
    CC(WriteConsoleOutputW(b, &c, co(1, 1), co(0, 0), &r));
    CC(coeq(curpos(b), 7, 7));
    CEQ(attrof(b), 0x2F);
    CloseHandle(b);
}

CT(con_wco_wide_char_cells) {
    CREQ(WriteConsoleOutputW); CREQ(ReadConsoleOutputW);
    BUF();
    CHAR_INFO buf[2];
    buf[0] = ci(0x4E2D, 7 | COMMON_LVB_LEADING_BYTE);
    buf[1] = ci(0x4E2D, 7 | COMMON_LVB_TRAILING_BYTE);
    SMALL_RECT r = sr(0, 0, 1, 0);
    CC(WriteConsoleOutputW(b, buf, co(2, 1), co(0, 0), &r));
    CHAR_INFO rd[2]; SMALL_RECT r2 = sr(0, 0, 1, 0);
    CC(ReadConsoleOutputW(b, rd, co(2, 1), co(0, 0), &r2));
    cnote("wide cells read back: 0x%x/0x%x 0x%x/0x%x", rd[0].Char.UnicodeChar, rd[0].Attributes, rd[1].Char.UnicodeChar, rd[1].Attributes);
    CC((rd[0].Attributes & COMMON_LVB_LEADING_BYTE) != 0);
    CC((rd[1].Attributes & COMMON_LVB_TRAILING_BYTE) != 0);
    CEQ(rd[0].Char.UnicodeChar, 0x4E2D);
    CloseHandle(b);
}

CT(con_rco_basic) {
    CREQ(ReadConsoleOutputW); CREQ(ReadConsoleOutputA);
    BUF();
    CC(wrw(b, L"abcdef"));
    CC(SetConsoleCursorPosition(b, co(0, 1)));
    CC(wrw(b, L"ghijkl"));
    CHAR_INFO rd[8]; memset(rd, 0xAA, sizeof rd);
    SMALL_RECT r = sr(1, 0, 3, 1);
    SetLastError(0xBAD);
    CCF(ReadConsoleOutputW(b, rd, co(3, 2), co(0, 0), &r), "err=%lu", GetLastError());
    CC(r.Left == 1 && r.Top == 0 && r.Right == 3 && r.Bottom == 1);
    CEQ(rd[0].Char.UnicodeChar, 'b'); CEQ(rd[2].Char.UnicodeChar, 'd'); CEQ(rd[3].Char.UnicodeChar, 'h'); CEQ(rd[5].Char.UnicodeChar, 'j');
    CEQ(rd[0].Attributes, 7);
    CEQ(rd[6].Char.UnicodeChar, 0xAAAA);   /* beyond the rectangle untouched */
    CHAR_INFO ra[6]; memset(ra, 0xAA, sizeof ra);
    r = sr(1, 0, 3, 1);
    CC(ReadConsoleOutputA(b, ra, co(3, 2), co(0, 0), &r));
    CEQ(ra[0].Char.AsciiChar, 'b'); CEQ(ra[4].Char.AsciiChar, 'i');
    CloseHandle(b);
}

CT(con_rco_clipping_returns_rect) {
    CREQ(ReadConsoleOutputW);
    BUF();
    static CHAR_INFO rd[16];
    SMALL_RECT r = sr(77, 22, 82, 27);
    CC(ReadConsoleOutputW(b, rd, co(6, 6), co(0, 0), &r));
    CCF(r.Left == 77 && r.Top == 22 && r.Right == 79 && r.Bottom == 24, "rect %d,%d-%d,%d", r.Left, r.Top, r.Right, r.Bottom);
    CloseHandle(b);
}

CT(con_rco_buffer_coord_placement) {
    CREQ(ReadConsoleOutputW);
    BUF();
    CC(wrw(b, L"xy"));
    CHAR_INFO rd[9]; memset(rd, 0, sizeof rd);
    SMALL_RECT r = sr(0, 0, 1, 0);
    CC(ReadConsoleOutputW(b, rd, co(3, 3), co(1, 2), &r));
    CEQ(rd[2 * 3 + 1].Char.UnicodeChar, 'x');
    CEQ(rd[2 * 3 + 2].Char.UnicodeChar, 'y');
    CEQ(rd[0].Char.UnicodeChar, 0);
    CloseHandle(b);
}

CT(con_rco_wide_char_attrs) {
    CREQ(ReadConsoleOutputW);
    BUF();
    CC(wrw(b, L"\x4E2D"));
    CHAR_INFO rd[2]; SMALL_RECT r = sr(0, 0, 1, 0);
    CC(ReadConsoleOutputW(b, rd, co(2, 1), co(0, 0), &r));
    CC(rd[0].Attributes & COMMON_LVB_LEADING_BYTE);
    CC(rd[1].Attributes & COMMON_LVB_TRAILING_BYTE);
    cnote("wide chars: 0x%x 0x%x", rd[0].Char.UnicodeChar, rd[1].Char.UnicodeChar);
    CloseHandle(b);
}

CT(con_rco_a_codepage) {
    CREQ(ReadConsoleOutputA); CREQ(SetConsoleOutputCP);
    UINT old = GetConsoleOutputCP();
    BUF();
    DWORD n;
    FillConsoleOutputCharacterW(b, 0xE9, 1, co(0, 0), &n);
    CHAR_INFO rd[1]; SMALL_RECT r = sr(0, 0, 0, 0);
    CC(SetConsoleOutputCP(437));
    CC(ReadConsoleOutputA(b, rd, co(1, 1), co(0, 0), &r));
    CEQ((unsigned char)rd[0].Char.AsciiChar, 0x82);
    r = sr(0, 0, 0, 0);
    CC(SetConsoleOutputCP(1252));
    CC(ReadConsoleOutputA(b, rd, co(1, 1), co(0, 0), &r));
    CEQ((unsigned char)rd[0].Char.AsciiChar, 0xE9);
    SetConsoleOutputCP(old);
    CloseHandle(b);
}

/* ---------------- ScrollConsoleScreenBuffer ---------------- */

static void rows_with_labels(HANDLE b, int n) {
    for (int y = 0; y < n; y++) {
        WCHAR l[16]; wsprintfW(l, L"row%02d-abcdef", y);
        DWORD w; WriteConsoleOutputCharacterW(b, l, lstrlenW(l), co(0, y), &w);
    }
}

CT(con_scroll_up_one_row) {
    CREQ(ScrollConsoleScreenBufferW);
    BUF();
    rows_with_labels(b, 6);
    SMALL_RECT src = sr(0, 1, 9, 3);
    CHAR_INFO fill = ci(L'.', 0x1E);
    SetLastError(0xBAD);
    CCF(ScrollConsoleScreenBufferW(b, &src, NULL, co(0, 0), &fill), "err=%lu", GetLastError());
    CC(rowis(b, 0, 0, L"row01-abcd"));
    CC(rowis(b, 1, 0, L"row02-abcd"));
    CC(rowis(b, 2, 0, L"row03-abcd"));
    CC(rowis(b, 3, 0, L".........."));
    CEQ(cellattr(b, 0, 3), 0x1E); CEQ(cellattr(b, 9, 3), 0x1E);
    CC(rowis(b, 3, 10, L"ef"));            /* outside src columns untouched */
    CC(rowis(b, 4, 0, L"row04-abcd"));
    CC(coeq(curpos(b), 0, 0));
    CloseHandle(b);
}

CT(con_scroll_down_one_row) {
    CREQ(ScrollConsoleScreenBufferW);
    BUF();
    rows_with_labels(b, 6);
    SMALL_RECT src = sr(0, 1, 9, 3);
    CHAR_INFO fill = ci(L'_', 7);
    CC(ScrollConsoleScreenBufferW(b, &src, NULL, co(0, 2), &fill));
    CC(rowis(b, 0, 0, L"row00-abcd"));
    CC(rowis(b, 1, 0, L"__________"));
    CC(rowis(b, 2, 0, L"row01-abcd"));
    CC(rowis(b, 3, 0, L"row02-abcd"));
    CC(rowis(b, 4, 0, L"row03-abcd"));
    CC(rowis(b, 5, 0, L"row05-abcd"));
    CloseHandle(b);
}

CT(con_scroll_horizontal) {
    CREQ(ScrollConsoleScreenBufferW);
    BUF();
    rows_with_labels(b, 2);
    SMALL_RECT src = sr(0, 0, 9, 0);
    CHAR_INFO fill = ci(L'-', 7);
    CC(ScrollConsoleScreenBufferW(b, &src, NULL, co(3, 0), &fill));
    CC(rowis(b, 0, 0, L"---row00-ab"));
    CC(rowis(b, 1, 0, L"row01-abcd"));
    src = sr(3, 0, 12, 0);
    CC(ScrollConsoleScreenBufferW(b, &src, NULL, co(0, 0), &fill));
    CC(rowis(b, 0, 0, L"row00-ab"));
    CloseHandle(b);
}

CT(con_scroll_clip_rect) {
    CREQ(ScrollConsoleScreenBufferW);
    BUF();
    rows_with_labels(b, 8);
    DWORD w;
    WriteConsoleOutputCharacterW(b, L"ABCDE", 5, co(5, 5), &w);
    SMALL_RECT src = sr(0, 0, 9, 0);
    SMALL_RECT clip = sr(0, 0, 4, 24);
    CHAR_INFO fill = ci(L'?', 7);
    CC(ScrollConsoleScreenBufferW(b, &src, &clip, co(0, 5), &fill));
    CC(rowis(b, 5, 0, L"row00"));          /* inside clip: copied */
    CC(rowis(b, 5, 5, L"ABCDE"));          /* outside clip: untouched */
    CC(rowis(b, 0, 0, L"?????"));          /* vacated source cells inside the clip are filled */
    CC(rowis(b, 0, 5, L"-abcd"));          /* vacated cells outside the clip keep their content */
    CloseHandle(b);
}

CT(con_scroll_clip_removes_dest_only) {
    CREQ(ScrollConsoleScreenBufferW);
    BUF();
    rows_with_labels(b, 4);
    SMALL_RECT src = sr(0, 0, 9, 1);
    SMALL_RECT clip = sr(0, 2, 79, 3);       /* clip excludes the source rows */
    CHAR_INFO fill = ci(L'#', 7);
    CC(ScrollConsoleScreenBufferW(b, &src, &clip, co(0, 2), &fill));
    /* P1 EXPECT-FIX: Win10 ScrollRegion intersects the source with the clip rectangle first and returns when the
       intersection is empty, so nothing is copied to the destination rows. UNVERIFIED-until-rerun */
    CC(rowis(b, 2, 0, L"row02-abcd"));
    CC(rowis(b, 3, 0, L"row03-abcd"));
    CC(rowis(b, 0, 0, L"row00-abcd"));       /* source left as is: outside the clip */
    CC(rowis(b, 1, 0, L"row01-abcd"));
    CloseHandle(b);
}

CT(con_scroll_dest_partially_outside) {
    CREQ(ScrollConsoleScreenBufferW);
    BUF();
    rows_with_labels(b, 2);
    SMALL_RECT src = sr(0, 0, 9, 0);
    CHAR_INFO fill = ci(L' ', 7);
    CC(ScrollConsoleScreenBufferW(b, &src, NULL, co(75, 0), &fill));
    CC(rowis(b, 0, 75, L"row00"));
    CEQ(cellch(b, 74, 0), ' ');
    CC(coeq(curpos(b), 0, 0));
    CloseHandle(b);
}

CT(con_scroll_whole_buffer_negative_dest) {
    CREQ(ScrollConsoleScreenBufferW);
    BUF();
    rows_with_labels(b, 3);
    SMALL_RECT src = sr(0, 0, 79, 24);
    CHAR_INFO fill = ci(L' ', 0x07);
    SetLastError(0xBAD);
    BOOL ok = ScrollConsoleScreenBufferW(b, &src, NULL, co(0, -1), &fill);
    cnote("scroll with negative dest -> %d err=%lu", ok, GetLastError());
    CC(ok);
    CC(rowis(b, 0, 0, L"row01-abcd"));
    CC(rowis(b, 1, 0, L"row02-abcd"));
    CEQ(cellch(b, 0, 2), ' ');
    CloseHandle(b);
}

CT(con_scroll_fill_attr_only_vacated) {
    CREQ(ScrollConsoleScreenBufferW);
    BUF();
    rows_with_labels(b, 4);
    SMALL_RECT src = sr(0, 0, 9, 3);
    CHAR_INFO fill = ci(L'F', 0x5A);
    CC(ScrollConsoleScreenBufferW(b, &src, NULL, co(0, 1), &fill));
    CEQ(cellattr(b, 0, 0), 0x5A); CEQ(cellch(b, 0, 0), 'F');
    CEQ(cellattr(b, 0, 1), 7);
    CC(rowis(b, 1, 0, L"row00-abcd"));
    CloseHandle(b);
}

CT(con_scroll_a_variant) {
    CREQ(ScrollConsoleScreenBufferA);
    BUF();
    rows_with_labels(b, 3);
    SMALL_RECT src = sr(0, 1, 9, 2);
    CHAR_INFO fill; fill.Char.AsciiChar = 'a'; fill.Attributes = 0x2B;
    CC(ScrollConsoleScreenBufferA(b, &src, NULL, co(0, 0), &fill));
    CC(rowis(b, 0, 0, L"row01-abcd"));
    CC(rowis(b, 1, 0, L"row02-abcd"));
    CC(rowis(b, 2, 0, L"aaaaaaaaaa"));
    CEQ(cellattr(b, 0, 2), 0x2B);
    CloseHandle(b);
}

CT(con_scroll_a_variant_codepage_fill) {
    CREQ(ScrollConsoleScreenBufferA); CREQ(SetConsoleOutputCP);
    UINT old = GetConsoleOutputCP();
    BUF();
    SMALL_RECT src = sr(0, 0, 3, 0);
    CHAR_INFO fill; fill.Char.AsciiChar = (CHAR)0x82; fill.Attributes = 7;
    CC(SetConsoleOutputCP(437));
    CC(ScrollConsoleScreenBufferA(b, &src, NULL, co(0, 1), &fill));
    CEQ(cellch(b, 0, 0), 0xE9);
    SetConsoleOutputCP(old);
    CloseHandle(b);
}

CT(con_scroll_invalid_handles) {
    CREQ(ScrollConsoleScreenBufferW); CREQ(ScrollConsoleScreenBufferA);
    SMALL_RECT src = sr(0, 0, 3, 3);
    CHAR_INFO fill = ci(L' ', 7);
    HANDLE bad[4]; inhandle_bad_list(bad);
    for (int k = 0; k < 4; k++) {
        FAILS(ScrollConsoleScreenBufferW(bad[k], &src, NULL, co(0, 1), &fill), ERROR_INVALID_HANDLE);
        FAILS(ScrollConsoleScreenBufferA(bad[k], &src, NULL, co(0, 1), &fill), ERROR_INVALID_HANDLE);
    }
}

CT(con_scroll_source_outside_buffer) {
    CREQ(ScrollConsoleScreenBufferW);
    BUF();
    rows_with_labels(b, 2);
    SMALL_RECT src = sr(100, 100, 110, 110);
    CHAR_INFO fill = ci(L'!', 7);
    SetLastError(0xBAD);
    BOOL ok = ScrollConsoleScreenBufferW(b, &src, NULL, co(0, 0), &fill);
    DWORD e = GetLastError();
    cnote("scroll source outside -> %d err=%lu", ok, e);
    if (!ok) CEQ(e, ERROR_INVALID_PARAMETER);
    CC(rowis(b, 0, 0, L"row00-abcd"));
    CloseHandle(b);
}

CT(con_scroll_inverted_source) {
    CREQ(ScrollConsoleScreenBufferW);
    BUF();
    rows_with_labels(b, 2);
    SMALL_RECT src = sr(9, 3, 0, 0);
    CHAR_INFO fill = ci(L'!', 7);
    SetLastError(0xBAD);
    BOOL ok = ScrollConsoleScreenBufferW(b, &src, NULL, co(0, 5), &fill);
    DWORD e = GetLastError();
    cnote("scroll inverted source -> %d err=%lu", ok, e);
    if (!ok) CEQ(e, ERROR_INVALID_PARAMETER);
    CC(rowis(b, 0, 0, L"row00-abcd"));
    CloseHandle(b);
}

CT(con_scroll_overlap_copy_semantics) {
    CREQ(ScrollConsoleScreenBufferW);
    BUF();
    DWORD n;
    WriteConsoleOutputCharacterW(b, L"0123456789", 10, co(0, 0), &n);
    SMALL_RECT src = sr(0, 0, 9, 0);
    CHAR_INFO fill = ci(L'.', 7);
    CC(ScrollConsoleScreenBufferW(b, &src, NULL, co(2, 0), &fill));
    CC(rowis(b, 0, 0, L"..01234567"));     /* memmove semantics: no smearing */
    CloseHandle(b);
}

/* ---------------- fonts ---------------- */

CT(con_font_current) {
    CREQ(GetCurrentConsoleFont); CREQ(GetConsoleFontSize);
    CONSOLE_FONT_INFO f; memset(&f, 0xCC, sizeof f);
    SetLastError(0xBAD);
    CCF(GetCurrentConsoleFont(hout_(), FALSE, &f), "err=%lu", GetLastError());
    CC(f.dwFontSize.X > 0 && f.dwFontSize.Y > 0);
    cnote("current font index %lu size %dx%d", f.nFont, f.dwFontSize.X, f.dwFontSize.Y);
    CONSOLE_FONT_INFO m;
    CC(GetCurrentConsoleFont(hout_(), TRUE, &m));
    CC(m.dwFontSize.X > 0 && m.dwFontSize.Y > 0);
    COORD s = GetConsoleFontSize(hout_(), f.nFont);
    cnote("GetConsoleFontSize(index) -> %dx%d", s.X, s.Y);
    CC(s.X > 0 && s.Y > 0);
}

CT(con_font_current_invalid_handles) {
    CREQ(GetCurrentConsoleFont);
    CONSOLE_FONT_INFO f;
    HANDLE bad[4]; inhandle_bad_list(bad);
    for (int k = 0; k < 4; k++) {
        FAILS(GetCurrentConsoleFont(bad[k], FALSE, &f), ERROR_INVALID_HANDLE);
        FAILS(GetCurrentConsoleFont(bad[k], TRUE, &f), ERROR_INVALID_HANDLE);
    }
}

CT(con_font_ex_basic) {
    CREQ(GetCurrentConsoleFontEx);
    CONSOLE_FONT_INFOEX f; memset(&f, 0, sizeof f); f.cbSize = sizeof f;
    SetLastError(0xBAD);
    CCF(GetCurrentConsoleFontEx(hout_(), FALSE, &f), "err=%lu", GetLastError());
    CC(f.dwFontSize.X > 0 && f.dwFontSize.Y > 0);
    CC(f.FaceName[0] != 0);
    CC(f.FontWeight > 0);
    cnote("font ex: idx %lu %dx%d family 0x%x weight %u face \"%ls\"", f.nFont, f.dwFontSize.X, f.dwFontSize.Y, f.FontFamily, f.FontWeight, f.FaceName);
    CONSOLE_FONT_INFOEX m; memset(&m, 0, sizeof m); m.cbSize = sizeof m;
    CC(GetCurrentConsoleFontEx(hout_(), TRUE, &m));
    CC(m.dwFontSize.X > 0);
}

CT(con_font_ex_bad_cbsize) {
    CREQ(GetCurrentConsoleFontEx); CREQ(SetCurrentConsoleFontEx);
    DWORD sz[] = { 0, 1, 80, 83, 85, 100, 0x10000 };
    for (size_t k = 0; k < sizeof sz / sizeof sz[0]; k++) {
        CONSOLE_FONT_INFOEX f; memset(&f, 0, sizeof f); f.cbSize = sz[k];
        FAILS(GetCurrentConsoleFontEx(hout_(), FALSE, &f), ERROR_INVALID_PARAMETER);
        FAILS(GetCurrentConsoleFontEx(hout_(), TRUE, &f), ERROR_INVALID_PARAMETER);
        FAILS(SetCurrentConsoleFontEx(hout_(), FALSE, &f), ERROR_INVALID_PARAMETER);
    }
}

CT(con_font_ex_invalid_handles) {
    CREQ(GetCurrentConsoleFontEx); CREQ(SetCurrentConsoleFontEx);
    CONSOLE_FONT_INFOEX f; memset(&f, 0, sizeof f); f.cbSize = sizeof f;
    HANDLE bad[4]; inhandle_bad_list(bad);
    for (int k = 0; k < 4; k++) {
        FAILS(GetCurrentConsoleFontEx(bad[k], FALSE, &f), ERROR_INVALID_HANDLE);
        FAILS(SetCurrentConsoleFontEx(bad[k], FALSE, &f), ERROR_INVALID_HANDLE);
    }
}

CT(con_font_ex_set_identity) {
    CREQ(GetCurrentConsoleFontEx); CREQ(SetCurrentConsoleFontEx);
    CONSOLE_FONT_INFOEX f, g; memset(&f, 0, sizeof f); f.cbSize = sizeof f;
    CC(GetCurrentConsoleFontEx(hout_(), FALSE, &f));
    SetLastError(0xBAD);
    BOOL ok = SetCurrentConsoleFontEx(hout_(), FALSE, &f);
    cnote("SetCurrentConsoleFontEx(identity) -> %d err=%lu", ok, GetLastError());
    memset(&g, 0, sizeof g); g.cbSize = sizeof g;
    CC(GetCurrentConsoleFontEx(hout_(), FALSE, &g));
    if (ok) { CC(g.dwFontSize.X == f.dwFontSize.X && g.dwFontSize.Y == f.dwFontSize.Y); CC(!lstrcmpW(g.FaceName, f.FaceName)); }
}

CT(con_font_ex_set_size) {
    CREQ(GetCurrentConsoleFontEx); CREQ(SetCurrentConsoleFontEx);
    CONSOLE_FONT_INFOEX f, g; memset(&f, 0, sizeof f); f.cbSize = sizeof f;
    CC(GetCurrentConsoleFontEx(hout_(), FALSE, &f));
    g = f;
    g.dwFontSize.X = 0; g.dwFontSize.Y = 20;
    SetLastError(0xBAD);
    BOOL ok = SetCurrentConsoleFontEx(hout_(), FALSE, &g);
    cnote("SetCurrentConsoleFontEx(0x20) -> %d err=%lu", ok, GetLastError());
    CONSOLE_FONT_INFOEX h; memset(&h, 0, sizeof h); h.cbSize = sizeof h;
    CC(GetCurrentConsoleFontEx(hout_(), FALSE, &h));
    cnote("font after: %dx%d \"%ls\"", h.dwFontSize.X, h.dwFontSize.Y, h.FaceName);
    SetCurrentConsoleFontEx(hout_(), FALSE, &f);
}

CT(con_font_size_query_invalid) {
    CREQ(GetConsoleFontSize);
    HANDLE bad[3] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x1234 };
    for (int k = 0; k < 3; k++) {
        SetLastError(0xBAD);
        COORD c = GetConsoleFontSize(bad[k], 0);
        CC(c.X == 0 && c.Y == 0);
        CERR(ERROR_INVALID_HANDLE);
    }
    SetLastError(0xBAD);
    COORD c = GetConsoleFontSize(hout_(), 0xFFFF);
    cnote("GetConsoleFontSize(index 0xFFFF) -> %dx%d err=%lu", c.X, c.Y, GetLastError());
}

CT(con_font_legacy_calls) {
    typedef DWORD (WINAPI *gnf_t)(void);
    typedef BOOL (WINAPI *gfi_t)(HANDLE, BOOL, DWORD, void *);
    typedef BOOL (WINAPI *sf_t)(HANDLE, DWORD);
    gnf_t gnf = (gnf_t)t_proc_k32("GetNumberOfConsoleFonts");
    gfi_t gfi = (gfi_t)t_proc_k32("GetConsoleFontInfo");
    sf_t sf = (sf_t)t_proc_k32("SetConsoleFont");
    CREQ(gnf); CREQ(gfi); CREQ(sf);
    SetLastError(0xBAD);
    DWORD n = gnf();
    cnote("GetNumberOfConsoleFonts -> %lu err=%lu", n, GetLastError());
    COORD cells[64]; memset(cells, 0, sizeof cells);
    SetLastError(0xBAD);
    BOOL r = gfi(hout_(), FALSE, 8, cells);
    cnote("GetConsoleFontInfo -> %d err=%lu", r, GetLastError());
    SetLastError(0xBAD);
    r = sf(INVALID_HANDLE_VALUE, 0);
    cnote("SetConsoleFont(INVALID,0) -> %d err=%lu", r, GetLastError());
    CEQ(r, 0);
}

CT(con_legacy_exports_present) {
    static const char *names[] = {
        "ShowConsoleCursor", "SetConsoleCursor", "SetConsoleMaximumWindowSize", "SetConsoleKeyShortcuts", "GetConsoleCharType", "SetConsoleNlsMode",
        "GetConsoleNlsMode", "GetConsoleHardwareState", "SetConsoleHardwareState", "SetConsoleIcon", "SetConsolePalette", "SetConsoleMenuClose",
        "ConsoleMenuControl", "GetConsoleInputWaitHandle", "InvalidateConsoleDIBits", "SetLastConsoleEventActive", "CloseConsoleHandle",
        "VerifyConsoleIoHandle", "DuplicateConsoleHandle", "SetConsoleLocalEUDC", "SetConsoleOS2OemFormat", "RegisterConsoleOS2", "RegisterConsoleVDM",
        "RegisterConsoleIME", "UnregisterConsoleIME", "OpenConsoleW", "GetConsoleFontInfo", "GetNumberOfConsoleFonts", "SetConsoleFont",
        "GetConsoleCursorMode", "SetConsoleCursorMode", "WriteConsoleInputVDMA", "WriteConsoleInputVDMW", "VDMConsoleOperation",
        "GetConsoleKeyboardLayoutNameA", "GetConsoleKeyboardLayoutNameW", "SetStdHandleEx", "ReadConsoleInputExA", "ReadConsoleInputExW",
        "GetConsoleInputExeNameA", "GetConsoleInputExeNameW", "SetConsoleInputExeNameA", "SetConsoleInputExeNameW",
        "CreatePseudoConsole", "ResizePseudoConsole", "ClosePseudoConsole", "GetNumberOfConsoleMouseButtons", "GetConsoleSelectionInfo",
        "GetConsoleHistoryInfo", "SetConsoleHistoryInfo", "GetConsoleOriginalTitleA", "GetConsoleOriginalTitleW", "GetConsoleScreenBufferInfoEx",
        "SetConsoleScreenBufferInfoEx", "GetCurrentConsoleFontEx", "SetCurrentConsoleFontEx", "GetConsoleProcessList", "GetConsoleDisplayMode",
        "SetConsoleDisplayMode", "AttachConsole", "GetConsoleAliasExesW", "ExpungeConsoleCommandHistoryW", "SetConsoleNumberOfCommandsW",
        "GetConsoleCommandHistoryLengthW", "GenerateConsoleCtrlEvent", "SetConsoleCtrlHandler",
    };
    for (size_t k = 0; k < sizeof names / sizeof names[0]; k++) {
        FARPROC p = t_proc_k32(names[k]);
        CCF(p != NULL, "kernel32 export %s missing", names[k]);
    }
}

CT(con_input_wait_handle_is_stdin) {
    typedef HANDLE (WINAPI *giwh_t)(void);
    giwh_t f = (giwh_t)t_proc_k32("GetConsoleInputWaitHandle");
    CREQ(f);
    CEQ(f(), hin_());
}

CT(con_verify_io_handle) {
    typedef BOOL (WINAPI *v_t)(HANDLE);
    v_t f = (v_t)t_proc_k32("VerifyConsoleIoHandle");
    CREQ(f);
    BOOL a = f(hin_()), o = f(hout_()), bad = f((HANDLE)(ULONG_PTR)0x1234), nul = f(NULL);
    cnote("VerifyConsoleIoHandle in=%d out=%d bogus=%d null=%d", a, o, bad, nul);
    CEQ(bad, 0);
}
