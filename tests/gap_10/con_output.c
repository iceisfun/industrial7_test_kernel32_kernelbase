/* gap_10: ReadConsoleOutput{A,W}, ReadConsoleOutputCharacter{A,W}, ReadConsoleOutputAttribute */
#include "g10.h"

static int W_, H_;   /* screen buffer size of the default output buffer */

/* Clear the first 20 rows and write known text. Returns 0 if the buffer is too small for the tests. */
static int setup(HANDLE h) {
    CONSOLE_SCREEN_BUFFER_INFO i; DWORD n;
    memset(&i, 0, sizeof i);
    if (!GetConsoleScreenBufferInfo(h, &i)) return 0;
    W_ = i.dwSize.X; H_ = i.dwSize.Y;
    if (W_ < 20 || H_ < 24) return 0;
    FillConsoleOutputCharacterW(h, L' ', W_ * 20, gco(0, 0), &n);
    FillConsoleOutputAttribute(h, 7, W_ * 20, gco(0, 0), &n);
    FillConsoleOutputCharacterW(h, L' ', W_, gco(0, H_ - 1), &n);
    FillConsoleOutputAttribute(h, 7, W_, gco(0, H_ - 1), &n);
    return 1;
}
static void g10_putw(HANDLE h, int x, int y, const WCHAR *s) { DWORD n; WriteConsoleOutputCharacterW(h, s, lstrlenW(s), gco(x, y), &n); }
static void g10_puta(HANDLE h, int x, int y, WORD a, int len) { DWORD n; FillConsoleOutputAttribute(h, a, len, gco(x, y), &n); }
#define SETUP() HANDLE h = gout(); do { if (!setup(h)) { g10_cline("SKIP console buffer too small (%dx%d)", W_, H_); g10_cskip = 1; return; } } while (0)

/* ---------- ReadConsoleOutputCharacterW ---------- */

GC(rocw_basic) {
    GREQ(U(ReadConsoleOutputCharacterW)); SETUP();
    g10_putw(h, 0, 0, L"ABCDEFGHIJ");
    WCHAR b[16] = {0}; DWORD n = 0xAA;
    SetLastError(0xBAD);
    GCHK(U(ReadConsoleOutputCharacterW)(h, b, 10, gco(0, 0), &n));
    GEQ(n, 10); GWSTR(b, L"ABCDEFGHIJ");
}
GC(rocw_offset_and_length) {
    GREQ(U(ReadConsoleOutputCharacterW)); SETUP();
    g10_putw(h, 0, 2, L"ABCDEFGHIJ");
    WCHAR b[16] = {0}; DWORD n = 0xAA;
    GCHK(U(ReadConsoleOutputCharacterW)(h, b, 4, gco(3, 2), &n));
    GEQ(n, 4); GCHK(!wcsncmp(b, L"DEFG", 4)); GEQ(b[4], 0);
    GCHK(U(ReadConsoleOutputCharacterW)(h, b, 1, gco(9, 2), &n));
    GEQ(n, 1); GEQ(b[0], L'J');
}
GC(rocw_spaces_in_empty_cells) {
    GREQ(U(ReadConsoleOutputCharacterW)); SETUP();
    WCHAR b[8]; DWORD n = 0xAA;
    for (int i = 0; i < 8; i++) b[i] = 0x7777;
    GCHK(U(ReadConsoleOutputCharacterW)(h, b, 8, gco(5, 6), &n));
    GEQ(n, 8);
    for (int i = 0; i < 8; i++) GEQ(b[i], L' ');
}
GC(rocw_wraps_to_next_row) {
    GREQ(U(ReadConsoleOutputCharacterW)); SETUP();
    g10_putw(h, W_ - 3, 0, L"XYZ"); g10_putw(h, 0, 1, L"123");
    WCHAR b[16] = {0}; DWORD n = 0xAA;
    GCHK(U(ReadConsoleOutputCharacterW)(h, b, 6, gco(W_ - 3, 0), &n));
    GEQ(n, 6); GCHK(!wcsncmp(b, L"XYZ123", 6));
}
GC(rocw_spans_many_rows) {
    GREQ(U(ReadConsoleOutputCharacterW)); SETUP();
    for (int y = 0; y < 4; y++) g10_putw(h, 0, y + 3, y == 0 ? L"r0" : y == 1 ? L"r1" : y == 2 ? L"r2" : L"r3");
    static WCHAR b[8192]; DWORD n = 0xAA;
    GCHK(U(ReadConsoleOutputCharacterW)(h, b, W_ * 4, gco(0, 3), &n));
    GEQ(n, W_ * 4);
    GEQ(b[0], L'r'); GEQ(b[1], L'0'); GEQ(b[W_], L'r'); GEQ(b[W_ + 1], L'1');
    GEQ(b[2 * W_ + 1], L'2'); GEQ(b[3 * W_ + 1], L'3');
}
GC(rocw_truncates_at_buffer_end) {
    GREQ(U(ReadConsoleOutputCharacterW)); SETUP();
    WCHAR b[32]; DWORD n = 0xAA;
    for (int i = 0; i < 32; i++) b[i] = 0x7777;
    GCHK(U(ReadConsoleOutputCharacterW)(h, b, 10, gco(W_ - 3, H_ - 1), &n));
    GEQ(n, 3);
    GEQ(b[2], L' '); GCHK(b[3] == 0x7777 || b[3] == 0);   /* element past the returned count is unspecified (Win10 zero-fills the requested length) */
    n = 0xAA;
    GCHK(U(ReadConsoleOutputCharacterW)(h, b, 5, gco(W_ - 1, H_ - 1), &n));
    GEQ(n, 1);
    n = 0xAA;
    GCHK(U(ReadConsoleOutputCharacterW)(h, b, 1, gco(W_ - 1, H_ - 1), &n));
    GEQ(n, 1);
}
GC(rocw_does_not_write_past_length) {
    GREQ(U(ReadConsoleOutputCharacterW)); SETUP();
    g10_putw(h, 0, 4, L"ABCDEFGH");
    WCHAR b[16]; DWORD n = 0xAA;
    for (int i = 0; i < 16; i++) b[i] = 0x7777;
    GCHK(U(ReadConsoleOutputCharacterW)(h, b, 5, gco(0, 4), &n));
    GEQ(n, 5);
    for (int i = 5; i < 16; i++) GEQ(b[i], 0x7777);
}
GC(rocw_zero_length) {
    GREQ(U(ReadConsoleOutputCharacterW)); SETUP();
    WCHAR b[4] = {0x7777}; DWORD n = 0xAA;
    SetLastError(0xBAD);
    BOOL ok = U(ReadConsoleOutputCharacterW)(h, b, 0, gco(0, 0), &n);
    g10_cnote("ReadConsoleOutputCharacterW(len 0) ok=%d n=%lu err=%lu", ok, n, GetLastError());
    GEQ(b[0], 0x7777);
}
GC(rocw_start_out_of_range) {
    GREQ(U(ReadConsoleOutputCharacterW)); SETUP();
    WCHAR b[8]; DWORD n;
    COORD c[] = { gco(W_, 0), gco(0, H_), gco(-1, 0), gco(0, -1), gco(W_ + 100, H_ + 100), gco(0x7FFF, 0x7FFF) };
    for (size_t i = 0; i < sizeof c / sizeof c[0]; i++) {
        n = 0xAA; SetLastError(0xBAD);
        BOOL ok = U(ReadConsoleOutputCharacterW)(h, b, 4, c[i], &n);
        g10_cnote("start (%d,%d): ok=%d n=%lu err=%lu", c[i].X, c[i].Y, ok, n, GetLastError());
        if (ok) GEQ(n, 0);
    }
}
GC(rocw_invalid_handles) {
    GREQ(U(ReadConsoleOutputCharacterW));
    WCHAR b[8]; DWORD n = 0xAA;
    HANDLE cl = CreateEventA(NULL, FALSE, FALSE, NULL); CloseHandle(cl);
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x1234, cl };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) GFAIL(U(ReadConsoleOutputCharacterW)(bad[i], b, 4, gco(0, 0), &n), ERROR_INVALID_HANDLE);
}
GC(rocw_input_handle_rejected) {
    GREQ(U(ReadConsoleOutputCharacterW));
    WCHAR b[8]; DWORD n = 0xAA;
    SetLastError(0xBAD);
    BOOL ok = U(ReadConsoleOutputCharacterW)(gin(), b, 4, gco(0, 0), &n);
    GEQ(ok, 0);
    g10_cnote("ReadConsoleOutputCharacterW(input handle) err=%lu", GetLastError());
}
GC(rocw_second_buffer_independent) {
    GREQ(U(ReadConsoleOutputCharacterW)); SETUP();
    HANDLE b2 = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    if (b2 == INVALID_HANDLE_VALUE) { g10_cnote("no second buffer"); return; }
    g10_putw(h, 0, 0, L"MAIN");
    g10_putw(b2, 0, 0, L"OTHR");
    WCHAR b[8] = {0}; DWORD n = 0xAA;
    GCHK(U(ReadConsoleOutputCharacterW)(b2, b, 4, gco(0, 0), &n)); GEQ(n, 4); GCHK(!wcsncmp(b, L"OTHR", 4));
    GCHK(U(ReadConsoleOutputCharacterW)(h, b, 4, gco(0, 0), &n)); GEQ(n, 4); GCHK(!wcsncmp(b, L"MAIN", 4));
    CloseHandle(b2);
}
GC(rocw_after_close_of_buffer_handle) {
    GREQ(U(ReadConsoleOutputCharacterW)); SETUP();
    HANDLE b2 = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    if (b2 == INVALID_HANDLE_VALUE) { g10_cnote("no second buffer"); return; }
    CloseHandle(b2);
    WCHAR b[4]; DWORD n = 0xAA;
    GFAIL(U(ReadConsoleOutputCharacterW)(b2, b, 2, gco(0, 0), &n), ERROR_INVALID_HANDLE);
}
GC(rocw_repeated_reads_stable) {
    GREQ(U(ReadConsoleOutputCharacterW)); SETUP();
    g10_putw(h, 0, 7, L"stable text");
    for (int k = 0; k < 50; k++) {
        WCHAR b[16] = {0}; DWORD n = 0;
        GCHK(U(ReadConsoleOutputCharacterW)(h, b, 11, gco(0, 7), &n));
        GEQ(n, 11); GCHK(!wcsncmp(b, L"stable text", 11));
    }
}

/* ---------- ReadConsoleOutputCharacterA ---------- */

GC(roca_basic) {
    GREQ(U(ReadConsoleOutputCharacterA)); SETUP();
    g10_putw(h, 0, 0, L"hello, world");
    char b[32] = {0}; DWORD n = 0xAA;
    GCHK(U(ReadConsoleOutputCharacterA)(h, b, 12, gco(0, 0), &n));
    GEQ(n, 12); GCHK(!memcmp(b, "hello, world", 12)); GEQ(b[12], 0);
}
GC(roca_matches_wide_for_ascii) {
    GREQ(U(ReadConsoleOutputCharacterA)); GREQ(U(ReadConsoleOutputCharacterW)); SETUP();
    WCHAR txt[96]; for (int i = 0; i < 95; i++) txt[i] = (WCHAR)(32 + i); txt[95] = 0;
    g10_putw(h, 0, 8, txt);
    char a[96] = {0}; WCHAR w[96] = {0}; DWORD na = 0, nw = 0;
    GCHK(U(ReadConsoleOutputCharacterA)(h, a, 95, gco(0, 8), &na));
    GCHK(U(ReadConsoleOutputCharacterW)(h, w, 95, gco(0, 8), &nw));
    GEQ(na, 95); GEQ(nw, 95);
    int bad = 0; for (int i = 0; i < 95; i++) if ((WCHAR)(unsigned char)a[i] != w[i]) bad++;
    GEQ(bad, 0);
}
GC(roca_truncates_at_buffer_end) {
    GREQ(U(ReadConsoleOutputCharacterA)); SETUP();
    char b[32]; DWORD n = 0xAA; memset(b, 0x77, sizeof b);
    GCHK(U(ReadConsoleOutputCharacterA)(h, b, 10, gco(W_ - 4, H_ - 1), &n));
    GEQ(n, 4); GCHK(b[4] == 0x77 || b[4] == 0);   /* byte past the returned count is unspecified (Win10 zero-fills the requested length) */
}
GC(roca_cp1252_high_char) {
    GREQ(U(ReadConsoleOutputCharacterA)); SETUP();
    UINT ocp = GetConsoleOutputCP();
    if (!SetConsoleOutputCP(1252)) { g10_cnote("SetConsoleOutputCP(1252) failed"); return; }
    WCHAR s[] = { 0x00E9, 0x00FC, 0x20AC, 0 };
    g10_putw(h, 0, 9, s);
    unsigned char b[8] = {0}; DWORD n = 0xAA;
    GCHK(U(ReadConsoleOutputCharacterA)(h, (char*)b, 3, gco(0, 9), &n));
    GEQ(n, 3); GEQ(b[0], 0xE9); GEQ(b[1], 0xFC); GEQ(b[2], 0x80);
    SetConsoleOutputCP(ocp);
}
GC(roca_cp437_high_char) {
    GREQ(U(ReadConsoleOutputCharacterA)); SETUP();
    UINT ocp = GetConsoleOutputCP();
    if (!SetConsoleOutputCP(437)) { g10_cnote("SetConsoleOutputCP(437) failed"); return; }
    WCHAR s[] = { 0x00E9, 0 };
    g10_putw(h, 0, 9, s);
    unsigned char b[4] = {0}; DWORD n = 0xAA;
    GCHK(U(ReadConsoleOutputCharacterA)(h, (char*)b, 1, gco(0, 9), &n));
    GEQ(n, 1); GEQ(b[0], 0x82);
    SetConsoleOutputCP(ocp);
}
GC(roca_zero_length_and_sentinel) {
    GREQ(U(ReadConsoleOutputCharacterA)); SETUP();
    char b[8]; DWORD n = 0xAA; memset(b, 0x77, sizeof b);
    SetLastError(0xBAD);
    BOOL ok = U(ReadConsoleOutputCharacterA)(h, b, 0, gco(0, 0), &n);
    g10_cnote("ReadConsoleOutputCharacterA(len 0) ok=%d n=%lu err=%lu", ok, n, GetLastError());
    GEQ(b[0], 0x77);
}
GC(roca_invalid_handles) {
    GREQ(U(ReadConsoleOutputCharacterA));
    char b[8]; DWORD n = 0xAA;
    HANDLE cl = CreateEventA(NULL, FALSE, FALSE, NULL); CloseHandle(cl);
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x1234, cl };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) GFAIL(U(ReadConsoleOutputCharacterA)(bad[i], b, 4, gco(0, 0), &n), ERROR_INVALID_HANDLE);
}
GC(roca_start_out_of_range) {
    GREQ(U(ReadConsoleOutputCharacterA)); SETUP();
    char b[8]; DWORD n;
    COORD c[] = { gco(W_, 0), gco(0, H_), gco(-1, -1) };
    for (size_t i = 0; i < sizeof c / sizeof c[0]; i++) {
        n = 0xAA; SetLastError(0xBAD);
        BOOL ok = U(ReadConsoleOutputCharacterA)(h, b, 4, c[i], &n);
        g10_cnote("A start (%d,%d): ok=%d n=%lu err=%lu", c[i].X, c[i].Y, ok, n, GetLastError());
    }
}

/* ---------- ReadConsoleOutputAttribute ---------- */

GC(roa_basic) {
    GREQ(U(ReadConsoleOutputAttribute)); SETUP();
    g10_puta(h, 0, 5, 0x1E, 4);
    WORD a[8]; DWORD n = 0xAA;
    GCHK(U(ReadConsoleOutputAttribute)(h, a, 6, gco(0, 5), &n));
    GEQ(n, 6);
    for (int i = 0; i < 4; i++) GEQ(a[i], 0x1E);
    GEQ(a[4], 7); GEQ(a[5], 7);
}
GC(roa_every_foreground_background) {
    GREQ(U(ReadConsoleOutputAttribute)); SETUP();
    for (int i = 0; i < 256; i++) g10_puta(h, i % W_, 10 + i / W_, (WORD)i, 1);
    int bad = 0;
    for (int i = 0; i < 256; i++) {
        WORD a = 0xFFFF; DWORD n = 0;
        if (!U(ReadConsoleOutputAttribute)(h, &a, 1, gco(i % W_, 10 + i / W_), &n) || n != 1 || (a & 0xFF) != i) bad++;
    }
    GEQ(bad, 0);
}
GC(roa_wraps_rows) {
    GREQ(U(ReadConsoleOutputAttribute)); SETUP();
    g10_puta(h, W_ - 2, 3, 0x2F, 2); g10_puta(h, 0, 4, 0x4F, 2);
    WORD a[8]; DWORD n = 0xAA;
    GCHK(U(ReadConsoleOutputAttribute)(h, a, 5, gco(W_ - 2, 3), &n));
    GEQ(n, 5); GEQ(a[0], 0x2F); GEQ(a[1], 0x2F); GEQ(a[2], 0x4F); GEQ(a[3], 0x4F); GEQ(a[4], 7);
}
GC(roa_truncates_at_buffer_end) {
    GREQ(U(ReadConsoleOutputAttribute)); SETUP();
    WORD a[16]; DWORD n = 0xAA; for (int i = 0; i < 16; i++) a[i] = 0x7777;
    GCHK(U(ReadConsoleOutputAttribute)(h, a, 10, gco(W_ - 2, H_ - 1), &n));
    GEQ(n, 2); GCHK(a[2] == 0x7777 || a[2] == 0);   /* element past the returned count is unspecified (Win10 zero-fills the requested length) */
}
GC(roa_does_not_write_past_length) {
    GREQ(U(ReadConsoleOutputAttribute)); SETUP();
    WORD a[16]; DWORD n = 0xAA; for (int i = 0; i < 16; i++) a[i] = 0x7777;
    GCHK(U(ReadConsoleOutputAttribute)(h, a, 3, gco(0, 0), &n));
    GEQ(n, 3);
    for (int i = 3; i < 16; i++) GEQ(a[i], 0x7777);
}
GC(roa_lvb_flags) {
    GREQ(U(ReadConsoleOutputAttribute)); SETUP();
    static const WORD fl[] = { COMMON_LVB_GRID_HORIZONTAL, COMMON_LVB_GRID_LVERTICAL, COMMON_LVB_GRID_RVERTICAL,
        COMMON_LVB_REVERSE_VIDEO, COMMON_LVB_UNDERSCORE };
    for (size_t i = 0; i < sizeof fl / sizeof fl[0]; i++) {
        WORD want = 0x07 | fl[i], a = 0; DWORD n = 0;
        g10_puta(h, (int)i, 12, want, 1);
        GCHK(U(ReadConsoleOutputAttribute)(h, &a, 1, gco((int)i, 12), &n));
        GEQ(n, 1);
        if (a != want) g10_cnote("lvb flag 0x%x read back as 0x%x", fl[i], a);
    }
}
GC(roa_zero_length) {
    GREQ(U(ReadConsoleOutputAttribute)); SETUP();
    WORD a[2] = {0x7777}; DWORD n = 0xAA;
    SetLastError(0xBAD);
    BOOL ok = U(ReadConsoleOutputAttribute)(h, a, 0, gco(0, 0), &n);
    g10_cnote("ReadConsoleOutputAttribute(len 0) ok=%d n=%lu err=%lu", ok, n, GetLastError());
    GEQ(a[0], 0x7777);
}
GC(roa_start_out_of_range) {
    GREQ(U(ReadConsoleOutputAttribute)); SETUP();
    WORD a[4]; DWORD n;
    COORD c[] = { gco(W_, 0), gco(0, H_), gco(-1, 0), gco(0, -1) };
    for (size_t i = 0; i < sizeof c / sizeof c[0]; i++) {
        n = 0xAA; SetLastError(0xBAD);
        BOOL ok = U(ReadConsoleOutputAttribute)(h, a, 2, c[i], &n);
        g10_cnote("attr start (%d,%d): ok=%d n=%lu err=%lu", c[i].X, c[i].Y, ok, n, GetLastError());
    }
}
GC(roa_invalid_handles) {
    GREQ(U(ReadConsoleOutputAttribute));
    WORD a[4]; DWORD n = 0xAA;
    HANDLE cl = CreateEventA(NULL, FALSE, FALSE, NULL); CloseHandle(cl);
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x1234, cl };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) GFAIL(U(ReadConsoleOutputAttribute)(bad[i], a, 2, gco(0, 0), &n), ERROR_INVALID_HANDLE);
}
GC(roa_second_buffer_default_attrs) {
    GREQ(U(ReadConsoleOutputAttribute)); SETUP();
    HANDLE b2 = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    if (b2 == INVALID_HANDLE_VALUE) { g10_cnote("no second buffer"); return; }
    DWORD n; FillConsoleOutputAttribute(b2, 0x5A, 3, gco(0, 0), &n);
    WORD a[4] = {0}; n = 0;
    GCHK(U(ReadConsoleOutputAttribute)(b2, a, 3, gco(0, 0), &n));
    GEQ(n, 3); GEQ(a[0], 0x5A); GEQ(a[2], 0x5A);
    GCHK(U(ReadConsoleOutputAttribute)(h, a, 1, gco(0, 0), &n));
    GEQ(a[0], 7);
    CloseHandle(b2);
}

/* ---------- ReadConsoleOutputW / ReadConsoleOutputA ---------- */

static void fill_block(HANDLE h) {   /* rows 14..17: "abcdef" with attr = 0x10 + x + 0x10*y... */
    for (int y = 0; y < 4; y++) {
        WCHAR s[8]; for (int x = 0; x < 6; x++) s[x] = (WCHAR)(L'a' + x + y * 6); s[6] = 0;
        g10_putw(h, 0, 14 + y, s);
        for (int x = 0; x < 6; x++) g10_puta(h, x, 14 + y, (WORD)(0x10 * (y + 1) + x + 1), 1);
    }
}
GC(rcow_basic_block) {
    GREQ(U(ReadConsoleOutputW)); SETUP(); fill_block(h);
    CHAR_INFO ci[6]; memset(ci, 0, sizeof ci);
    SMALL_RECT r = gsr(1, 14, 3, 15);
    SetLastError(0xBAD);
    GCHK(U(ReadConsoleOutputW)(h, ci, gco(3, 2), gco(0, 0), &r));
    GEQ(r.Left, 1); GEQ(r.Top, 14); GEQ(r.Right, 3); GEQ(r.Bottom, 15);
    for (int y = 0; y < 2; y++) for (int x = 0; x < 3; x++) {
        CHAR_INFO c = ci[y * 3 + x];
        GEQ(c.Char.UnicodeChar, L'a' + (x + 1) + y * 6);
        GEQ(c.Attributes, 0x10 * (y + 1) + (x + 1) + 1);
    }
}
GC(rcow_single_cell) {
    GREQ(U(ReadConsoleOutputW)); SETUP(); fill_block(h);
    CHAR_INFO ci; memset(&ci, 0, sizeof ci);
    SMALL_RECT r = gsr(2, 16, 2, 16);
    GCHK(U(ReadConsoleOutputW)(h, &ci, gco(1, 1), gco(0, 0), &r));
    GEQ(ci.Char.UnicodeChar, L'a' + 2 + 12); GEQ(ci.Attributes, 0x30 + 3);
    GEQ(r.Left, 2); GEQ(r.Right, 2); GEQ(r.Top, 16); GEQ(r.Bottom, 16);
}
GC(rcow_dest_offset_preserves_rest) {
    GREQ(U(ReadConsoleOutputW)); SETUP(); fill_block(h);
    CHAR_INFO ci[20]; for (int i = 0; i < 20; i++) { ci[i].Char.UnicodeChar = 0x7777; ci[i].Attributes = 0x7777; }
    SMALL_RECT r = gsr(0, 14, 2, 15);   /* 3x2 */
    GCHK(U(ReadConsoleOutputW)(h, ci, gco(5, 4), gco(1, 2), &r));
    for (int y = 0; y < 4; y++) for (int x = 0; x < 5; x++) {
        CHAR_INFO c = ci[y * 5 + x];
        if (y >= 2 && y < 4 && x >= 1 && x < 4) {
            GEQ(c.Char.UnicodeChar, L'a' + (x - 1) + (y - 2) * 6);
        } else {
            GEQ(c.Char.UnicodeChar, 0x7777);
        }
    }
}
GC(rcow_region_clipped_to_buffer) {
    GREQ(U(ReadConsoleOutputW)); SETUP();
    static CHAR_INFO ci[16 * 16];
    SMALL_RECT r = gsr(W_ - 2, H_ - 2, W_ + 4, H_ + 4);
    SetLastError(0xBAD);
    BOOL ok = U(ReadConsoleOutputW)(h, ci, gco(16, 16), gco(0, 0), &r);
    GCHK(ok);
    if (ok) { GEQ(r.Left, W_ - 2); GEQ(r.Top, H_ - 2); GEQ(r.Right, W_ - 1); GEQ(r.Bottom, H_ - 1); }
    else g10_cnote("clipped region read failed err=%lu", GetLastError());
}
GC(rcow_region_negative_clipped) {
    GREQ(U(ReadConsoleOutputW)); SETUP(); fill_block(h);
    static CHAR_INFO ci[16 * 16];
    SMALL_RECT r = gsr(-3, -3, 2, 2);
    SetLastError(0xBAD);
    BOOL ok = U(ReadConsoleOutputW)(h, ci, gco(16, 16), gco(0, 0), &r);
    g10_cnote("negative-origin region: ok=%d err=%lu -> (%d,%d)-(%d,%d)", ok, GetLastError(), r.Left, r.Top, r.Right, r.Bottom);
    if (ok) { GEQ(r.Left, 0); GEQ(r.Top, 0); GEQ(r.Right, 2); GEQ(r.Bottom, 2); }
}
GC(rcow_region_fully_outside) {
    GREQ(U(ReadConsoleOutputW)); SETUP();
    CHAR_INFO ci[4]; SMALL_RECT r = gsr(W_ + 1, 0, W_ + 3, 1);
    SetLastError(0xBAD);
    BOOL ok = U(ReadConsoleOutputW)(h, ci, gco(2, 2), gco(0, 0), &r);
    g10_cnote("outside region: ok=%d err=%lu -> (%d,%d)-(%d,%d)", ok, GetLastError(), r.Left, r.Top, r.Right, r.Bottom);
    if (!ok) { SetLastError(0xBAD); }
}
GC(rcow_region_inverted) {
    GREQ(U(ReadConsoleOutputW)); SETUP();
    CHAR_INFO ci[16]; SMALL_RECT r = gsr(5, 5, 2, 2);
    SetLastError(0xBAD);
    BOOL ok = U(ReadConsoleOutputW)(h, ci, gco(4, 4), gco(0, 0), &r);
    g10_cnote("inverted region: ok=%d err=%lu", ok, GetLastError());
}
GC(rcow_region_larger_than_dest) {
    GREQ(U(ReadConsoleOutputW)); SETUP(); fill_block(h);
    CHAR_INFO ci[4]; for (int i = 0; i < 4; i++) ci[i].Char.UnicodeChar = 0x7777, ci[i].Attributes = 0x7777;
    SMALL_RECT r = gsr(0, 14, 3, 17);
    SetLastError(0xBAD);
    BOOL ok = U(ReadConsoleOutputW)(h, ci, gco(2, 2), gco(0, 0), &r);
    g10_cnote("region larger than dest: ok=%d err=%lu -> (%d,%d)-(%d,%d)", ok, GetLastError(), r.Left, r.Top, r.Right, r.Bottom);
    if (ok) {
        GCHK(r.Right - r.Left + 1 <= 2); GCHK(r.Bottom - r.Top + 1 <= 2);
        GEQ(ci[0].Char.UnicodeChar, L'a');
    }
}
GC(rcow_dest_coord_out_of_buffer) {
    GREQ(U(ReadConsoleOutputW)); SETUP();
    CHAR_INFO ci[4]; SMALL_RECT r = gsr(0, 0, 1, 1);
    SetLastError(0xBAD);
    BOOL ok = U(ReadConsoleOutputW)(h, ci, gco(2, 2), gco(5, 5), &r);
    g10_cnote("dest coord outside: ok=%d err=%lu", ok, GetLastError());
}
GC(rcow_zero_size_dest) {
    GREQ(U(ReadConsoleOutputW)); SETUP();
    CHAR_INFO ci[4]; SMALL_RECT r = gsr(0, 0, 1, 1);
    SetLastError(0xBAD);
    BOOL ok = U(ReadConsoleOutputW)(h, ci, gco(0, 0), gco(0, 0), &r);
    g10_cnote("zero size dest: ok=%d err=%lu", ok, GetLastError());
}
GC(rcow_invalid_handles) {
    GREQ(U(ReadConsoleOutputW));
    CHAR_INFO ci[4]; SMALL_RECT r;
    HANDLE cl = CreateEventA(NULL, FALSE, FALSE, NULL); CloseHandle(cl);
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x1234, cl };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) { r = gsr(0, 0, 1, 1); GFAIL(U(ReadConsoleOutputW)(bad[i], ci, gco(2, 2), gco(0, 0), &r), ERROR_INVALID_HANDLE); }
}
GC(rcow_input_handle_rejected) {
    GREQ(U(ReadConsoleOutputW));
    CHAR_INFO ci[4]; SMALL_RECT r = gsr(0, 0, 1, 1);
    SetLastError(0xBAD);
    BOOL ok = U(ReadConsoleOutputW)(gin(), ci, gco(2, 2), gco(0, 0), &r);
    GEQ(ok, 0);
    g10_cnote("ReadConsoleOutputW(input handle) err=%lu", GetLastError());
}
GC(rcow_roundtrip_with_writeoutput) {
    GREQ(U(ReadConsoleOutputW)); SETUP();
    CHAR_INFO in[12], out[12]; memset(out, 0, sizeof out);
    for (int i = 0; i < 12; i++) { in[i].Char.UnicodeChar = (WCHAR)(L'A' + i); in[i].Attributes = (WORD)(0x0F + i * 0x10); }
    SMALL_RECT wr = gsr(4, 18, 7, 20);
    WriteConsoleOutputW(h, in, gco(4, 3), gco(0, 0), &wr);
    SMALL_RECT r = gsr(4, 18, 7, 20);
    GCHK(U(ReadConsoleOutputW)(h, out, gco(4, 3), gco(0, 0), &r));
    GCHK(!memcmp(in, out, sizeof in));
}
GC(rcow_unicode_cells) {
    GREQ(U(ReadConsoleOutputW)); SETUP();
    WCHAR s[] = { 0x00E9, 0x20AC, 0x0416, 0x03A9, 0 };
    g10_putw(h, 0, 19, s);
    CHAR_INFO ci[4]; SMALL_RECT r = gsr(0, 19, 3, 19);
    GCHK(U(ReadConsoleOutputW)(h, ci, gco(4, 1), gco(0, 0), &r));
    for (int i = 0; i < 4; i++) GEQ(ci[i].Char.UnicodeChar, s[i]);
}
GC(rcoa_basic_block) {
    GREQ(U(ReadConsoleOutputA)); SETUP(); fill_block(h);
    CHAR_INFO ci[6]; memset(ci, 0, sizeof ci);
    SMALL_RECT r = gsr(1, 14, 3, 15);
    GCHK(U(ReadConsoleOutputA)(h, ci, gco(3, 2), gco(0, 0), &r));
    GEQ(r.Left, 1); GEQ(r.Right, 3); GEQ(r.Top, 14); GEQ(r.Bottom, 15);
    for (int y = 0; y < 2; y++) for (int x = 0; x < 3; x++) {
        CHAR_INFO c = ci[y * 3 + x];
        GEQ((unsigned char)c.Char.AsciiChar, 'a' + (x + 1) + y * 6);
        GEQ(c.Attributes, 0x10 * (y + 1) + (x + 1) + 1);
    }
}
GC(rcoa_matches_wide_ascii) {
    GREQ(U(ReadConsoleOutputA)); GREQ(U(ReadConsoleOutputW)); SETUP(); fill_block(h);
    CHAR_INFO a[24], w[24]; memset(a, 0, sizeof a); memset(w, 0, sizeof w);
    SMALL_RECT ra = gsr(0, 14, 5, 17), rw = ra;
    GCHK(U(ReadConsoleOutputA)(h, a, gco(6, 4), gco(0, 0), &ra));
    GCHK(U(ReadConsoleOutputW)(h, w, gco(6, 4), gco(0, 0), &rw));
    int bad = 0;
    for (int i = 0; i < 24; i++) if ((WCHAR)(unsigned char)a[i].Char.AsciiChar != w[i].Char.UnicodeChar || a[i].Attributes != w[i].Attributes) bad++;
    GEQ(bad, 0);
    GEQ(ra.Right, rw.Right); GEQ(ra.Bottom, rw.Bottom);
}
GC(rcoa_cp1252_high_char) {
    GREQ(U(ReadConsoleOutputA)); SETUP();
    UINT ocp = GetConsoleOutputCP();
    if (!SetConsoleOutputCP(1252)) { g10_cnote("SetConsoleOutputCP(1252) failed"); return; }
    WCHAR s[] = { 0x00E9, 0x00FC, 0 };
    g10_putw(h, 0, 19, s);
    CHAR_INFO ci[2]; SMALL_RECT r = gsr(0, 19, 1, 19);
    GCHK(U(ReadConsoleOutputA)(h, ci, gco(2, 1), gco(0, 0), &r));
    GEQ((unsigned char)ci[0].Char.AsciiChar, 0xE9); GEQ((unsigned char)ci[1].Char.AsciiChar, 0xFC);
    SetConsoleOutputCP(ocp);
}
GC(rcoa_region_clipped_to_buffer) {
    GREQ(U(ReadConsoleOutputA)); SETUP();
    static CHAR_INFO ci[16 * 16];
    SMALL_RECT r = gsr(W_ - 3, H_ - 3, W_ + 9, H_ + 9);
    BOOL ok = U(ReadConsoleOutputA)(h, ci, gco(16, 16), gco(0, 0), &r);
    GCHK(ok);
    if (ok) { GEQ(r.Right, W_ - 1); GEQ(r.Bottom, H_ - 1); GEQ(r.Left, W_ - 3); GEQ(r.Top, H_ - 3); }
}
GC(rcoa_invalid_handles) {
    GREQ(U(ReadConsoleOutputA));
    CHAR_INFO ci[4]; SMALL_RECT r;
    HANDLE cl = CreateEventA(NULL, FALSE, FALSE, NULL); CloseHandle(cl);
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x1234, cl };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) { r = gsr(0, 0, 1, 1); GFAIL(U(ReadConsoleOutputA)(bad[i], ci, gco(2, 2), gco(0, 0), &r), ERROR_INVALID_HANDLE); }
}
GC(rcoa_second_buffer) {
    GREQ(U(ReadConsoleOutputA)); SETUP();
    HANDLE b2 = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    if (b2 == INVALID_HANDLE_VALUE) { g10_cnote("no second buffer"); return; }
    g10_putw(b2, 0, 0, L"QR");
    CHAR_INFO ci[2]; SMALL_RECT r = gsr(0, 0, 1, 0);
    GCHK(U(ReadConsoleOutputA)(b2, ci, gco(2, 1), gco(0, 0), &r));
    GEQ(ci[0].Char.AsciiChar, 'Q'); GEQ(ci[1].Char.AsciiChar, 'R');
    CloseHandle(b2);
}
GC(rcow_large_region) {
    GREQ(U(ReadConsoleOutputW)); SETUP();
    static CHAR_INFO ci[20 * 20];
    int w = W_ < 80 ? W_ : 80;
    SMALL_RECT r = gsr(0, 0, w - 1, 19);
    static CHAR_INFO big[80 * 20];
    GCHK(U(ReadConsoleOutputW)(h, big, gco(w, 20), gco(0, 0), &r));
    GEQ(r.Right, w - 1); GEQ(r.Bottom, 19);
    GEQ(big[0].Char.UnicodeChar, L' ');
}
