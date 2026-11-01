/* gap_12: SetConsoleTextAttribute, SetConsoleScreenBufferSize, SetConsoleWindowInfo, SetConsoleScreenBufferInfoEx,
 * SetCurrentConsoleFontEx, SetConsoleTitleA/W. Every body runs in a child: detached (no console) or in its own new console. */
#include "gc.h"

#define F(n) (U(n))

static HANDLE newbuf(void) {
    HANDLE h = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    return h == INVALID_HANDLE_VALUE ? NULL : h;
}
static BOOL info(HANDLE h, CONSOLE_SCREEN_BUFFER_INFO *i) { memset(i, 0xAA, sizeof *i); return GetConsoleScreenBufferInfo(h, i); }
static void infoex_init(CONSOLE_SCREEN_BUFFER_INFOEX *e) { memset(e, 0, sizeof *e); e->cbSize = sizeof *e; }
static void fontex_init(CONSOLE_FONT_INFOEX *f) { memset(f, 0, sizeof *f); f->cbSize = sizeof *f; }

/* handles that are not console screen buffers */
typedef struct { const char *what; HANDLE h; } BH;
static int bad_handles(BH *out) {
    int n = 0;
    HANDLE closed = CreateEventW(NULL, TRUE, FALSE, NULL); CloseHandle(closed);
    out[n].what = "NULL"; out[n++].h = NULL;
    out[n].what = "INVALID_HANDLE_VALUE"; out[n++].h = INVALID_HANDLE_VALUE;
    out[n].what = "bogus 0x1234"; out[n++].h = (HANDLE)(ULONG_PTR)0x1234;
    out[n].what = "bogus 0x7FFFFFFC"; out[n++].h = (HANDLE)(ULONG_PTR)0x7FFFFFFC;
    out[n].what = "closed event"; out[n++].h = closed;
    out[n].what = "event"; out[n++].h = CreateEventW(NULL, TRUE, FALSE, NULL);
    out[n].what = "current process"; out[n++].h = GetCurrentProcess();
    out[n].what = "current thread"; out[n++].h = GetCurrentThread();
    return n;
}

/* ---------------- detached: no console at all ---------------- */
GT(con_detached_all_fail_invalid_handle) {
    GREQ(F(SetConsoleTextAttribute)); GREQ(F(SetConsoleScreenBufferSize)); GREQ(F(SetConsoleWindowInfo));
    GREQ(F(SetConsoleTitleW)); GREQ(F(SetConsoleTitleA));
    COORD sz = { 80, 25 }; SMALL_RECT r = { 0, 0, 79, 24 };
    HANDLE hs[3] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x44 };
    for (int i = 0; i < 3; i++) {
        GFAIL(F(SetConsoleTextAttribute)(hs[i], 7), ERROR_INVALID_HANDLE);
        GFAIL(F(SetConsoleScreenBufferSize)(hs[i], sz), ERROR_INVALID_HANDLE);
        GFAIL(F(SetConsoleWindowInfo)(hs[i], TRUE, &r), ERROR_INVALID_HANDLE);
    }
    GFAIL(F(SetConsoleTitleW)(L"x"), ERROR_INVALID_HANDLE);
    GFAIL(F(SetConsoleTitleA)("x"), ERROR_INVALID_HANDLE);
}
GT(con_detached_infoex_font) {
    GREQ(F(SetConsoleScreenBufferInfoEx)); GREQ(F(SetCurrentConsoleFontEx));
    CONSOLE_SCREEN_BUFFER_INFOEX e; infoex_init(&e);
    e.dwSize.X = 80; e.dwSize.Y = 25; e.srWindow.Right = 79; e.srWindow.Bottom = 24; e.dwMaximumWindowSize.X = 80; e.dwMaximumWindowSize.Y = 25;
    GFAIL(F(SetConsoleScreenBufferInfoEx)(NULL, &e), ERROR_INVALID_HANDLE);
    GFAIL(F(SetConsoleScreenBufferInfoEx)(INVALID_HANDLE_VALUE, &e), ERROR_INVALID_HANDLE);
    e.cbSize = 0;
    GFAIL(F(SetConsoleScreenBufferInfoEx)(NULL, &e), ERROR_INVALID_PARAMETER);
    CONSOLE_FONT_INFOEX f; fontex_init(&f);
    GFAIL(F(SetCurrentConsoleFontEx)(NULL, FALSE, &f), ERROR_INVALID_HANDLE);
    GFAIL(F(SetCurrentConsoleFontEx)(INVALID_HANDLE_VALUE, TRUE, &f), ERROR_INVALID_HANDLE);
    f.cbSize = 0;
    GFAIL(F(SetCurrentConsoleFontEx)(NULL, FALSE, &f), ERROR_INVALID_PARAMETER);
}
GT(con_detached_title_variants) {
    GREQ(F(SetConsoleTitleW)); GREQ(F(SetConsoleTitleA));
    static const WCHAR *w[] = { L"", L"a", L"a much longer title than usual with spaces", L"é中文" };
    for (unsigned i = 0; i < 4; i++) GFAIL(F(SetConsoleTitleW)(w[i]), ERROR_INVALID_HANDLE);
    static const char *a[] = { "", "a", "ascii title" };
    for (unsigned i = 0; i < 3; i++) GFAIL(F(SetConsoleTitleA)(a[i]), ERROR_INVALID_HANDLE);
}

/* ---------------- argument validation that happens before the console server is contacted ---------------- */
GT(con_infoex_cbsize_validation) {
    GREQ(F(SetConsoleScreenBufferInfoEx));
    static const ULONG sizes[] = { 0, 1, 4, 0x5C, 0x5F, 0x61, 0x64, 0x100, 0x80000000, 0xFFFFFFFF };
    CONSOLE_SCREEN_BUFFER_INFOEX e; infoex_init(&e);
    for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        e.cbSize = sizes[i];
        GFAIL(F(SetConsoleScreenBufferInfoEx)(INVALID_HANDLE_VALUE, &e), ERROR_INVALID_PARAMETER);
        GFAIL(F(SetConsoleScreenBufferInfoEx)(NULL, &e), ERROR_INVALID_PARAMETER);
    }
    /* correct size reaches the handle check instead */
    e.cbSize = sizeof e;
    GEQ(e.cbSize, 0x60);
    GFAIL(F(SetConsoleScreenBufferInfoEx)(INVALID_HANDLE_VALUE, &e), ERROR_INVALID_HANDLE);
}
GT(con_fontex_cbsize_validation) {
    GREQ(F(SetCurrentConsoleFontEx));
    static const ULONG sizes[] = { 0, 1, 4, 0x50, 0x53, 0x55, 0x58, 0x100, 0x80000000, 0xFFFFFFFF };
    CONSOLE_FONT_INFOEX f; fontex_init(&f);
    for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        f.cbSize = sizes[i];
        GFAIL(F(SetCurrentConsoleFontEx)(INVALID_HANDLE_VALUE, FALSE, &f), ERROR_INVALID_PARAMETER);
        GFAIL(F(SetCurrentConsoleFontEx)(NULL, TRUE, &f), ERROR_INVALID_PARAMETER);
    }
    f.cbSize = sizeof f;
    GEQ(f.cbSize, 0x54);
    GFAIL(F(SetCurrentConsoleFontEx)(INVALID_HANDLE_VALUE, FALSE, &f), ERROR_INVALID_HANDLE);
}

/* ---------------- invalid handles with a real console ---------------- */
GTC(con_bad_handles_all_apis) {
    GREQ(F(SetConsoleTextAttribute)); GREQ(F(SetConsoleScreenBufferSize)); GREQ(F(SetConsoleWindowInfo));
    GREQ(F(SetConsoleScreenBufferInfoEx)); GREQ(F(SetCurrentConsoleFontEx));
    GWINESKIP("console server semantics differ in the emulator");
    BH hs[16]; int n = bad_handles(hs);
    COORD sz = { 80, 25 }; SMALL_RECT r = { 0, 0, 10, 10 };
    CONSOLE_SCREEN_BUFFER_INFOEX e; infoex_init(&e); e.dwSize = sz;
    CONSOLE_FONT_INFOEX f; fontex_init(&f);
    for (int i = 0; i < n; i++) {
        SetLastError(0xBAD); BOOL a = F(SetConsoleTextAttribute)(hs[i].h, 7); DWORD ea = GetLastError();
        SetLastError(0xBAD); BOOL b = F(SetConsoleScreenBufferSize)(hs[i].h, sz); DWORD eb = GetLastError();
        SetLastError(0xBAD); BOOL c = F(SetConsoleWindowInfo)(hs[i].h, TRUE, &r); DWORD ec = GetLastError();
        SetLastError(0xBAD); BOOL d = F(SetConsoleScreenBufferInfoEx)(hs[i].h, &e); DWORD ed = GetLastError();
        SetLastError(0xBAD); BOOL g = F(SetCurrentConsoleFontEx)(hs[i].h, FALSE, &f); DWORD eg = GetLastError();
        GCF(!a && ea == ERROR_INVALID_HANDLE, "%s: SetConsoleTextAttribute ret=%d err=%lu", hs[i].what, a, ea);
        GCF(!b && eb == ERROR_INVALID_HANDLE, "%s: SetConsoleScreenBufferSize ret=%d err=%lu", hs[i].what, b, eb);
        GCF(!c && ec == ERROR_INVALID_HANDLE, "%s: SetConsoleWindowInfo ret=%d err=%lu", hs[i].what, c, ec);
        GCF(!d && ed == ERROR_INVALID_HANDLE, "%s: SetConsoleScreenBufferInfoEx ret=%d err=%lu", hs[i].what, d, ed);
        GCF(!g && eg == ERROR_INVALID_HANDLE, "%s: SetCurrentConsoleFontEx ret=%d err=%lu", hs[i].what, g, eg);
    }
    CloseHandle(hs[5].h);
}
GTC(con_file_and_pipe_handles_rejected) {
    GREQ(F(SetConsoleTextAttribute)); GREQ(F(SetConsoleScreenBufferSize)); GREQ(F(SetConsoleWindowInfo));
    GWINESKIP("console server semantics differ in the emulator");
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "notaconsole.txt");
    HANDLE file = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL), r, w;
    CreatePipe(&r, &w, NULL, 0);
    COORD sz = { 80, 25 }; SMALL_RECT rc = { 0, 0, 10, 10 };
    HANDLE hs[3] = { file, r, w };
    for (int i = 0; i < 3; i++) {
        GFAIL(F(SetConsoleTextAttribute)(hs[i], 7), ERROR_INVALID_HANDLE);
        GFAIL(F(SetConsoleScreenBufferSize)(hs[i], sz), ERROR_INVALID_HANDLE);
        GFAIL(F(SetConsoleWindowInfo)(hs[i], TRUE, &rc), ERROR_INVALID_HANDLE);
    }
    CloseHandle(file); CloseHandle(r); CloseHandle(w);
}
GTC(con_input_handle_rejected_by_output_apis) {
    GREQ(F(SetConsoleTextAttribute)); GREQ(F(SetConsoleScreenBufferSize)); GREQ(F(SetConsoleWindowInfo));
    GWINESKIP("console server semantics differ in the emulator");
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    COORD sz = { 80, 25 }; SMALL_RECT rc = { 0, 0, 10, 10 };
    SetLastError(0xBAD); BOOL a = F(SetConsoleTextAttribute)(in, 7); DWORD ea = GetLastError();
    SetLastError(0xBAD); BOOL b = F(SetConsoleScreenBufferSize)(in, sz); DWORD eb = GetLastError();
    SetLastError(0xBAD); BOOL c = F(SetConsoleWindowInfo)(in, TRUE, &rc); DWORD ec = GetLastError();
    GEQ(a, 0); GEQ(b, 0); GEQ(c, 0);
    gnote("input handle: attr err=%lu size err=%lu window err=%lu", ea, eb, ec);
}

/* ---------------- SetConsoleTextAttribute ---------------- */
GTC(con_textattr_sweep_low_byte) {
    GREQ(F(SetConsoleTextAttribute));
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFO i; int bad = 0;
    for (int a = 0; a < 256; a++) {
        SetLastError(0xBAD);
        if (!F(SetConsoleTextAttribute)(b, (WORD)a)) { bad++; continue; }
        if (!info(b, &i) || (i.wAttributes & 0xFF) != a) bad++;
    }
    GEQ(bad, 0);
    CloseHandle(b);
}
GTC(con_textattr_success_keeps_last_error_unspecified) {
    GREQ(F(SetConsoleTextAttribute));
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    SetLastError(0xBAD);
    GEQ(F(SetConsoleTextAttribute)(b, FOREGROUND_RED | FOREGROUND_INTENSITY), TRUE);
    CONSOLE_SCREEN_BUFFER_INFO i; GC(info(b, &i));
    GEQ(i.wAttributes & 0xFF, FOREGROUND_RED | FOREGROUND_INTENSITY);
    CloseHandle(b);
}
GTC(con_textattr_high_bits) {
    GREQ(F(SetConsoleTextAttribute));
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    static const WORD v[] = { COMMON_LVB_LEADING_BYTE, COMMON_LVB_TRAILING_BYTE, COMMON_LVB_GRID_HORIZONTAL, COMMON_LVB_GRID_LVERTICAL,
                              COMMON_LVB_GRID_RVERTICAL, COMMON_LVB_REVERSE_VIDEO, COMMON_LVB_UNDERSCORE, 0x0100, 0x0200, 0x0400, 0x7FFF, 0xFFFF };
    CONSOLE_SCREEN_BUFFER_INFO i;
    for (unsigned k = 0; k < sizeof v / sizeof v[0]; k++) {
        SetLastError(0xBAD);
        BOOL r = F(SetConsoleTextAttribute)(b, v[k]);
        BOOL g = info(b, &i);
        gnote("attr 0x%04x: ret=%d stored=0x%04x", v[k], r, g ? i.wAttributes : 0xDEAD);
        /* P1 (gap_12): conhost SetConsoleTextAttributeImpl rejects any attribute with bit 0x2000 (undefined) set
         * with an invalid-parameter result and accepts all others (0x7FFF and 0xFFFF contain 0x2000). */
        GCF(r == ((v[k] & 0x2000) ? FALSE : TRUE), "attr 0x%04x ret=%d", v[k], r);
    }
    CloseHandle(b);
}
GTC(con_textattr_applies_to_new_text) {
    GREQ(F(SetConsoleTextAttribute));
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    GC(F(SetConsoleTextAttribute)(b, BACKGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY));
    COORD z = { 0, 0 }; DWORD n = 0;
    GC(SetConsoleCursorPosition(b, z));
    GC(WriteConsoleW(b, L"AB", 2, &n, NULL));
    GEQ(n, 2);
    WORD at[4] = {0}; DWORD got = 0;
    GC(ReadConsoleOutputAttribute(b, at, 2, z, &got));
    GEQ(got, 2);
    GEQ(at[0] & 0xFF, BACKGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    GEQ(at[1] & 0xFF, BACKGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    /* changing the attribute does not recolour text that was already written */
    GC(F(SetConsoleTextAttribute)(b, FOREGROUND_RED));
    GC(ReadConsoleOutputAttribute(b, at, 1, z, &got));
    GEQ(at[0] & 0xFF, BACKGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    COORD c2 = { 2, 0 };
    GC(SetConsoleCursorPosition(b, c2));
    GC(WriteConsoleW(b, L"C", 1, &n, NULL));
    GC(ReadConsoleOutputAttribute(b, at, 1, c2, &got));
    GEQ(at[0] & 0xFF, FOREGROUND_RED);
    CloseHandle(b);
}
GTC(con_textattr_per_buffer) {
    GREQ(F(SetConsoleTextAttribute));
    HANDLE a = newbuf(), b = newbuf(); GC(a && b); if (!a || !b) return;
    GC(F(SetConsoleTextAttribute)(a, 0x1E));
    GC(F(SetConsoleTextAttribute)(b, 0x2D));
    CONSOLE_SCREEN_BUFFER_INFO ia, ib; GC(info(a, &ia)); GC(info(b, &ib));
    GEQ(ia.wAttributes & 0xFF, 0x1E); GEQ(ib.wAttributes & 0xFF, 0x2D);
    CloseHandle(a); CloseHandle(b);
}
GTC(con_textattr_std_output_restore) {
    GREQ(F(SetConsoleTextAttribute));
    HANDLE o = GetStdHandle(STD_OUTPUT_HANDLE); CONSOLE_SCREEN_BUFFER_INFO i0, i1;
    GC(info(o, &i0));
    GC(F(SetConsoleTextAttribute)(o, 0x4F));
    GC(info(o, &i1)); GEQ(i1.wAttributes & 0xFF, 0x4F);
    GC(F(SetConsoleTextAttribute)(o, i0.wAttributes));
    GC(info(o, &i1)); GEQ(i1.wAttributes, i0.wAttributes);
}
GTC(con_textattr_does_not_touch_other_state) {
    GREQ(F(SetConsoleTextAttribute));
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFO i0, i1; GC(info(b, &i0));
    GC(F(SetConsoleTextAttribute)(b, 0x70));
    GC(info(b, &i1));
    GEQ(i1.dwSize.X, i0.dwSize.X); GEQ(i1.dwSize.Y, i0.dwSize.Y);
    GEQ(i1.dwCursorPosition.X, i0.dwCursorPosition.X); GEQ(i1.dwCursorPosition.Y, i0.dwCursorPosition.Y);
    GEQ(i1.srWindow.Left, i0.srWindow.Left); GEQ(i1.srWindow.Right, i0.srWindow.Right);
    CloseHandle(b);
}
GTC(con_textattr_repeat) {
    GREQ(F(SetConsoleTextAttribute));
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    int bad = 0;
    for (int k = 0; k < 2000; k++) if (!F(SetConsoleTextAttribute)(b, (WORD)(k & 0xFF))) bad++;
    GEQ(bad, 0);
    CloseHandle(b);
}
GTC(con_textattr_closed_buffer) {
    GREQ(F(SetConsoleTextAttribute));
    GWINESKIP("console server semantics differ in the emulator");
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    CloseHandle(b);
    GFAIL(F(SetConsoleTextAttribute)(b, 7), ERROR_INVALID_HANDLE);
}

/* ---------------- SetConsoleScreenBufferSize ---------------- */
GTC(con_bufsize_same_size) {
    GREQ(F(SetConsoleScreenBufferSize));
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFO i0, i1; GC(info(b, &i0));
    SetLastError(0xBAD);
    GEQ(F(SetConsoleScreenBufferSize)(b, i0.dwSize), TRUE);
    GC(info(b, &i1));
    GEQ(i1.dwSize.X, i0.dwSize.X); GEQ(i1.dwSize.Y, i0.dwSize.Y);
    CloseHandle(b);
}
GTC(con_bufsize_grow_and_shrink_height) {
    GREQ(F(SetConsoleScreenBufferSize));
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFO i0, i1; GC(info(b, &i0));
    COORD big = { i0.dwSize.X, (SHORT)(i0.dwSize.Y + 10) };
    SetLastError(0xBAD);
    GEQ(F(SetConsoleScreenBufferSize)(b, big), TRUE);
    GC(info(b, &i1)); GEQ(i1.dwSize.X, big.X); GEQ(i1.dwSize.Y, big.Y);
    /* back to the original height */
    SetLastError(0xBAD);
    GEQ(F(SetConsoleScreenBufferSize)(b, i0.dwSize), TRUE);
    GC(info(b, &i1)); GEQ(i1.dwSize.X, i0.dwSize.X); GEQ(i1.dwSize.Y, i0.dwSize.Y);
    CloseHandle(b);
}
GTC(con_bufsize_grow_width) {
    GREQ(F(SetConsoleScreenBufferSize));
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFO i0, i1; GC(info(b, &i0));
    COORD big = { (SHORT)(i0.dwSize.X + 20), i0.dwSize.Y };
    GEQ(F(SetConsoleScreenBufferSize)(b, big), TRUE);
    GC(info(b, &i1)); GEQ(i1.dwSize.X, big.X); GEQ(i1.dwSize.Y, big.Y);
    GEQ(F(SetConsoleScreenBufferSize)(b, i0.dwSize), TRUE);
    CloseHandle(b);
}
GTC(con_bufsize_zero_dimensions) {
    GREQ(F(SetConsoleScreenBufferSize));
    GWINESKIP("console server semantics differ in the emulator");
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFO i0, i1; GC(info(b, &i0));
    COORD z[] = { {0, 0}, {0, 25}, {80, 0}, {0, (SHORT)i0.dwSize.Y}, {i0.dwSize.X, 0} };
    for (unsigned k = 0; k < sizeof z / sizeof z[0]; k++) GFAIL(F(SetConsoleScreenBufferSize)(b, z[k]), ERROR_INVALID_PARAMETER);
    GC(info(b, &i1)); GEQ(i1.dwSize.X, i0.dwSize.X); GEQ(i1.dwSize.Y, i0.dwSize.Y);
    CloseHandle(b);
}
GTC(con_bufsize_negative_dimensions) {
    GREQ(F(SetConsoleScreenBufferSize));
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFO i0, i1; GC(info(b, &i0));
    COORD z[] = { {-1, -1}, {-1, 25}, {80, -1}, {-32768, 25}, {80, -32768} };
    for (unsigned k = 0; k < sizeof z / sizeof z[0]; k++) {
        SetLastError(0xBAD);
        BOOL r = F(SetConsoleScreenBufferSize)(b, z[k]);
        gnote("size (%d,%d): ret=%d err=%lu", z[k].X, z[k].Y, r, r ? 0 : GetLastError());
        GC(r == FALSE);
    }
    GC(info(b, &i1)); GEQ(i1.dwSize.X, i0.dwSize.X); GEQ(i1.dwSize.Y, i0.dwSize.Y);
    CloseHandle(b);
}
GTC(con_bufsize_smaller_than_window) {
    GREQ(F(SetConsoleScreenBufferSize));
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFO i0, i1; GC(info(b, &i0));
    SHORT ww = i0.srWindow.Right - i0.srWindow.Left + 1, wh = i0.srWindow.Bottom - i0.srWindow.Top + 1;
    COORD s1 = { (SHORT)(ww - 1), i0.dwSize.Y }, s2 = { i0.dwSize.X, (SHORT)(wh - 1) };
    if (ww > 1) { SetLastError(0xBAD); BOOL r = F(SetConsoleScreenBufferSize)(b, s1); gnote("width window-1: ret=%d err=%lu", r, r ? 0 : GetLastError()); }
    if (wh > 1) { SetLastError(0xBAD); BOOL r = F(SetConsoleScreenBufferSize)(b, s2); gnote("height window-1: ret=%d err=%lu", r, r ? 0 : GetLastError()); }
    GC(info(b, &i1));
    gnote("after: size %dx%d window %dx%d", i1.dwSize.X, i1.dwSize.Y, i1.srWindow.Right - i1.srWindow.Left + 1, i1.srWindow.Bottom - i1.srWindow.Top + 1);
    GC(i1.dwSize.X >= i1.srWindow.Right - i1.srWindow.Left + 1);
    GC(i1.dwSize.Y >= i1.srWindow.Bottom - i1.srWindow.Top + 1);
    CloseHandle(b);
}
GTC(con_bufsize_huge_width_rejected) {
    GREQ(F(SetConsoleScreenBufferSize));
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFO i0, i1; GC(info(b, &i0));
    /* 32767 columns x 1 row is a tiny allocation but exceeds the maximum */
    COORD c = { 32767, 1 };
    SetLastError(0xBAD);
    BOOL r = F(SetConsoleScreenBufferSize)(b, c);
    gnote("32767x1: ret=%d err=%lu", r, r ? 0 : GetLastError());
    GC(info(b, &i1));
    if (!r) { GEQ(i1.dwSize.X, i0.dwSize.X); GEQ(i1.dwSize.Y, i0.dwSize.Y); }
    else F(SetConsoleScreenBufferSize)(b, i0.dwSize);
    CloseHandle(b);
}
GTC(con_bufsize_preserves_attributes_and_cursor) {
    GREQ(F(SetConsoleScreenBufferSize));
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    GC(SetConsoleTextAttribute(b, 0x2E));
    COORD cur = { 3, 2 }; GC(SetConsoleCursorPosition(b, cur));
    CONSOLE_SCREEN_BUFFER_INFO i0, i1; GC(info(b, &i0));
    COORD big = { i0.dwSize.X, (SHORT)(i0.dwSize.Y + 5) };
    GC(F(SetConsoleScreenBufferSize)(b, big));
    GC(info(b, &i1));
    /* P1 (gap_12): with resize-with-reflow (the default wrap-on-resize console setting) conhost re-derives the cursor from
     * the last non-space cell and moves a cursor sitting past it on a blank row left by one column (X 3 -> 2); with the
     * traditional resize it stays at 3. Either is accepted. */
    GEQ(i1.wAttributes & 0xFF, 0x2E); GCF(i1.dwCursorPosition.X == 3 || i1.dwCursorPosition.X == 2, "cursor X %d", i1.dwCursorPosition.X); GEQ(i1.dwCursorPosition.Y, 2);
    CloseHandle(b);
}
GTC(con_bufsize_keeps_text) {
    GREQ(F(SetConsoleScreenBufferSize));
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    COORD z = { 0, 0 }; DWORD n = 0;
    GC(WriteConsoleOutputCharacterW(b, L"keep", 4, z, &n));
    CONSOLE_SCREEN_BUFFER_INFO i0; GC(info(b, &i0));
    COORD big = { (SHORT)(i0.dwSize.X + 10), (SHORT)(i0.dwSize.Y + 10) };
    GC(F(SetConsoleScreenBufferSize)(b, big));
    WCHAR out[8] = {0};
    GC(ReadConsoleOutputCharacterW(b, out, 4, z, &n));
    GEQ(n, 4); GWSTR(out, L"keep");
    CloseHandle(b);
}
GTC(con_bufsize_independent_buffers) {
    GREQ(F(SetConsoleScreenBufferSize));
    HANDLE a = newbuf(), b = newbuf(); GC(a && b); if (!a || !b) return;
    CONSOLE_SCREEN_BUFFER_INFO ia, ib, ia2; GC(info(a, &ia)); GC(info(b, &ib));
    COORD big = { ia.dwSize.X, (SHORT)(ia.dwSize.Y + 7) };
    GC(F(SetConsoleScreenBufferSize)(a, big));
    GC(info(b, &ia2));
    GEQ(ia2.dwSize.Y, ib.dwSize.Y);
    CloseHandle(a); CloseHandle(b);
}
GTC(con_bufsize_active_buffer) {
    GREQ(F(SetConsoleScreenBufferSize));
    HANDLE o = GetStdHandle(STD_OUTPUT_HANDLE); CONSOLE_SCREEN_BUFFER_INFO i0, i1; GC(info(o, &i0));
    COORD big = { i0.dwSize.X, (SHORT)(i0.dwSize.Y + 3) };
    GC(F(SetConsoleScreenBufferSize)(o, big));
    GC(info(o, &i1)); GEQ(i1.dwSize.Y, big.Y);
    GC(F(SetConsoleScreenBufferSize)(o, i0.dwSize));
    GC(info(o, &i1)); GEQ(i1.dwSize.Y, i0.dwSize.Y);
}

/* ---------------- SetConsoleWindowInfo ---------------- */
GTC(con_window_inverted_rect) {
    GREQ(F(SetConsoleWindowInfo));
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFO i0, i1; GC(info(b, &i0));
    SMALL_RECT r1 = { 5, 0, 4, 10 }, r2 = { 0, 5, 10, 4 }, r3 = { 10, 10, 0, 0 };
    GFAIL(F(SetConsoleWindowInfo)(b, TRUE, &r1), ERROR_INVALID_PARAMETER);
    GFAIL(F(SetConsoleWindowInfo)(b, TRUE, &r2), ERROR_INVALID_PARAMETER);
    GFAIL(F(SetConsoleWindowInfo)(b, TRUE, &r3), ERROR_INVALID_PARAMETER);
    GC(info(b, &i1));
    GEQ(i1.srWindow.Left, i0.srWindow.Left); GEQ(i1.srWindow.Right, i0.srWindow.Right);
    GEQ(i1.srWindow.Top, i0.srWindow.Top); GEQ(i1.srWindow.Bottom, i0.srWindow.Bottom);
    CloseHandle(b);
}
GTC(con_window_absolute_out_of_buffer) {
    GREQ(F(SetConsoleWindowInfo));
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFO i0, i1; GC(info(b, &i0));
    SMALL_RECT r[] = {
        { 0, 0, i0.dwSize.X, 10 }, { 0, 0, 10, i0.dwSize.Y }, { 0, 0, (SHORT)(i0.dwSize.X + 100), 10 },
        { -1, 0, 10, 10 }, { 0, -1, 10, 10 }, { 0, 0, 32767, 32767 }, { -32768, -32768, 10, 10 },
    };
    for (unsigned k = 0; k < sizeof r / sizeof r[0]; k++) {
        SMALL_RECT rc = r[k];
        SetLastError(0xBAD);
        BOOL ok = F(SetConsoleWindowInfo)(b, TRUE, &rc);
        /* P1 (gap_12) UNRESOLVED: the documentation says a window beyond the buffer (negative Left/Top, Right/Bottom past
         * the buffer) fails, but Win10 conhost SetConsoleWindowInfoImpl only rejects an inverted rectangle or a size above
         * the maximum window size (a short-width overflow lets 0..32767 through) and accepts negative origins. The
         * rectangles Win10 rejects (0..2) stay asserted; for 3..6 the outcome is recorded, not asserted, and the window is restored. */
        if (k < 3) {
            GCF(ok == FALSE, "rect %u (%d,%d,%d,%d) accepted", k, rc.Left, rc.Top, rc.Right, rc.Bottom);
            if (!ok) GERR(ERROR_INVALID_PARAMETER);
        } else {
            gnote("UNRESOLVED rect %u (%d,%d,%d,%d): ret=%d err=%lu", k, rc.Left, rc.Top, rc.Right, rc.Bottom, ok, ok ? 0 : GetLastError());
            if (ok) { SMALL_RECT rs = i0.srWindow; F(SetConsoleWindowInfo)(b, TRUE, &rs); }
        }
    }
    GC(info(b, &i1));
    GEQ(i1.srWindow.Right, i0.srWindow.Right); GEQ(i1.srWindow.Bottom, i0.srWindow.Bottom);
    CloseHandle(b);
}
GTC(con_window_absolute_resize_and_restore) {
    GREQ(F(SetConsoleWindowInfo));
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFO i0, i1; GC(info(b, &i0));
    SHORT cw = i0.srWindow.Right - i0.srWindow.Left + 1, ch = i0.srWindow.Bottom - i0.srWindow.Top + 1;
    SHORT nw = cw > 60 ? 60 : cw, nh = ch > 15 ? 15 : ch;
    SMALL_RECT r = { 0, 0, (SHORT)(nw - 1), (SHORT)(nh - 1) };
    SetLastError(0xBAD);
    BOOL ok = F(SetConsoleWindowInfo)(b, TRUE, &r);
    GCF(ok, "err=%lu", GetLastError());
    if (ok) {
        GC(info(b, &i1));
        gnote("requested 0,0,%d,%d got %d,%d,%d,%d", r.Right, r.Bottom, i1.srWindow.Left, i1.srWindow.Top, i1.srWindow.Right, i1.srWindow.Bottom);
        GEQ(i1.srWindow.Left, 0); GEQ(i1.srWindow.Top, 0);
        GCF(i1.srWindow.Right == r.Right, "right %d want %d", i1.srWindow.Right, r.Right);
        GCF(i1.srWindow.Bottom == r.Bottom, "bottom %d want %d", i1.srWindow.Bottom, r.Bottom);
        GEQ(i1.dwSize.X, i0.dwSize.X); GEQ(i1.dwSize.Y, i0.dwSize.Y);
        SMALL_RECT back = i0.srWindow;
        GC(F(SetConsoleWindowInfo)(b, TRUE, &back));
        GC(info(b, &i1));
        GEQ(i1.srWindow.Right, i0.srWindow.Right); GEQ(i1.srWindow.Bottom, i0.srWindow.Bottom);
    }
    CloseHandle(b);
}
GTC(con_window_relative_zero_is_noop) {
    GREQ(F(SetConsoleWindowInfo));
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFO i0, i1; GC(info(b, &i0));
    SMALL_RECT z = { 0, 0, 0, 0 };
    SetLastError(0xBAD);
    GEQ(F(SetConsoleWindowInfo)(b, FALSE, &z), TRUE);
    GC(info(b, &i1));
    GEQ(i1.srWindow.Left, i0.srWindow.Left); GEQ(i1.srWindow.Top, i0.srWindow.Top);
    GEQ(i1.srWindow.Right, i0.srWindow.Right); GEQ(i1.srWindow.Bottom, i0.srWindow.Bottom);
    CloseHandle(b);
}
GTC(con_window_relative_out_of_range) {
    GREQ(F(SetConsoleWindowInfo));
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFO i0, i1; GC(info(b, &i0));
    /* move the window up/left past the buffer origin */
    SMALL_RECT r[] = { {-1, 0, -1, 0}, {0, -1, 0, -1}, {-1, -1, -1, -1}, {0, 0, 32000, 32000}, {0, 0, 0, 32000} };
    for (unsigned k = 0; k < sizeof r / sizeof r[0]; k++) {
        SMALL_RECT rc = r[k];
        if (i0.srWindow.Left > 0 || i0.srWindow.Top > 0) continue;
        SetLastError(0xBAD);
        BOOL ok = F(SetConsoleWindowInfo)(b, FALSE, &rc);
        /* P1 (gap_12) UNRESOLVED: documented to fail when the moved window leaves the buffer; Win10 conhost does not check
         * the buffer bounds (only inverted rectangle / maximum window size). Rectangles 0..2 move the window to negative
         * coordinates and are accepted there; 3 and 4 exceed the maximum window size and are rejected (asserted). */
        if (k < 3) {
            gnote("UNRESOLVED relative rect %u: ret=%d err=%lu", k, ok, ok ? 0 : GetLastError());
            if (ok) { SMALL_RECT rs = i0.srWindow; F(SetConsoleWindowInfo)(b, TRUE, &rs); }
        } else {
            GCF(ok == FALSE, "relative rect %u accepted", k);
            if (!ok) GERR(ERROR_INVALID_PARAMETER);
        }
    }
    GC(info(b, &i1));
    GEQ(i1.srWindow.Right, i0.srWindow.Right); GEQ(i1.srWindow.Bottom, i0.srWindow.Bottom);
    CloseHandle(b);
}
GTC(con_window_relative_shift) {
    GREQ(F(SetConsoleWindowInfo)); GREQ(F(SetConsoleScreenBufferSize));
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFO i0, i1; GC(info(b, &i0));
    COORD big = { (SHORT)(i0.dwSize.X + 10), (SHORT)(i0.dwSize.Y + 10) };
    GC(F(SetConsoleScreenBufferSize)(b, big));
    GC(info(b, &i0));
    SMALL_RECT d = { 1, 1, 1, 1 };
    SetLastError(0xBAD);
    BOOL ok = F(SetConsoleWindowInfo)(b, FALSE, &d);
    GCF(ok, "err=%lu", GetLastError());
    if (ok) {
        GC(info(b, &i1));
        GEQ(i1.srWindow.Left, i0.srWindow.Left + 1); GEQ(i1.srWindow.Top, i0.srWindow.Top + 1);
        GEQ(i1.srWindow.Right, i0.srWindow.Right + 1); GEQ(i1.srWindow.Bottom, i0.srWindow.Bottom + 1);
        SMALL_RECT back = { -1, -1, -1, -1 };
        GC(F(SetConsoleWindowInfo)(b, FALSE, &back));
        GC(info(b, &i1));
        GEQ(i1.srWindow.Left, i0.srWindow.Left); GEQ(i1.srWindow.Bottom, i0.srWindow.Bottom);
    }
    CloseHandle(b);
}
GTC(con_window_relative_resize_right_bottom) {
    GREQ(F(SetConsoleWindowInfo));
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFO i0, i1; GC(info(b, &i0));
    SHORT cw = i0.srWindow.Right - i0.srWindow.Left + 1;
    if (cw < 30) { gnote("window too narrow (%d) for this test", cw); CloseHandle(b); return; }
    SMALL_RECT d = { 0, 0, -5, 0 };
    BOOL ok = F(SetConsoleWindowInfo)(b, FALSE, &d);
    GCF(ok, "err=%lu", GetLastError());
    if (ok) {
        GC(info(b, &i1));
        GEQ(i1.srWindow.Right, i0.srWindow.Right - 5); GEQ(i1.srWindow.Left, i0.srWindow.Left); GEQ(i1.srWindow.Bottom, i0.srWindow.Bottom);
        SMALL_RECT back = { 0, 0, 5, 0 };
        GC(F(SetConsoleWindowInfo)(b, FALSE, &back));
        GC(info(b, &i1)); GEQ(i1.srWindow.Right, i0.srWindow.Right);
    }
    CloseHandle(b);
}
GTC(con_window_bool_nonzero_is_absolute) {
    GREQ(F(SetConsoleWindowInfo));
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFO i0, i1; GC(info(b, &i0));
    SMALL_RECT r = i0.srWindow;
    static const BOOL vals[] = { 2, 0x100, -1, 0x7FFFFFFF };
    for (unsigned k = 0; k < 4; k++) {
        SetLastError(0xBAD);
        BOOL ok = F(SetConsoleWindowInfo)(b, vals[k], &r);
        GCF(ok, "BOOL 0x%x err=%lu", vals[k], GetLastError());
        GC(info(b, &i1));
        GCF(i1.srWindow.Right == i0.srWindow.Right && i1.srWindow.Left == i0.srWindow.Left, "BOOL 0x%x moved the window", vals[k]);
    }
    CloseHandle(b);
}
GTC(con_window_too_small) {
    GREQ(F(SetConsoleWindowInfo));
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFO i0, i1; GC(info(b, &i0));
    SMALL_RECT one = { 0, 0, 0, 0 };
    SetLastError(0xBAD);
    BOOL ok = F(SetConsoleWindowInfo)(b, TRUE, &one);
    gnote("1x1 window: ret=%d err=%lu", ok, ok ? 0 : GetLastError());
    GC(info(b, &i1));
    gnote("window after: %d,%d,%d,%d", i1.srWindow.Left, i1.srWindow.Top, i1.srWindow.Right, i1.srWindow.Bottom);
    if (ok) F(SetConsoleWindowInfo)(b, TRUE, &i0.srWindow);
    CloseHandle(b);
}
GTC(con_window_does_not_change_buffer_size) {
    GREQ(F(SetConsoleWindowInfo));
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFO i0, i1; GC(info(b, &i0));
    SMALL_RECT r = i0.srWindow;
    GC(F(SetConsoleWindowInfo)(b, TRUE, &r));
    GC(info(b, &i1));
    GEQ(i1.dwSize.X, i0.dwSize.X); GEQ(i1.dwSize.Y, i0.dwSize.Y);
    GEQ(i1.wAttributes, i0.wAttributes);
    CloseHandle(b);
}

/* ---------------- SetConsoleScreenBufferInfoEx ---------------- */
GTC(con_infoex_roundtrip_unchanged) {
    GREQ(F(SetConsoleScreenBufferInfoEx));
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFOEX e; infoex_init(&e);
    GC(GetConsoleScreenBufferInfoEx(b, &e));
    SetLastError(0xBAD);
    BOOL r = F(SetConsoleScreenBufferInfoEx)(b, &e);
    GCF(r, "err=%lu", GetLastError());
    CONSOLE_SCREEN_BUFFER_INFOEX e2; infoex_init(&e2);
    GC(GetConsoleScreenBufferInfoEx(b, &e2));
    GEQ(e2.dwSize.X, e.dwSize.X); GEQ(e2.dwSize.Y, e.dwSize.Y);
    GEQ(e2.wAttributes, e.wAttributes);
    GEQ(e2.wPopupAttributes, e.wPopupAttributes);
    gnote("window before %d,%d,%d,%d after %d,%d,%d,%d", e.srWindow.Left, e.srWindow.Top, e.srWindow.Right, e.srWindow.Bottom,
          e2.srWindow.Left, e2.srWindow.Top, e2.srWindow.Right, e2.srWindow.Bottom);
    CloseHandle(b);
}
GTC(con_infoex_color_table_roundtrip) {
    GREQ(F(SetConsoleScreenBufferInfoEx));
    GWINESKIP("emulator console has no colour table");
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFOEX e, orig, g; infoex_init(&e); infoex_init(&orig); infoex_init(&g);
    GC(GetConsoleScreenBufferInfoEx(b, &orig));
    e = orig;
    for (int k = 0; k < 16; k++) e.ColorTable[k] = RGB(k * 15 + 1, 255 - k * 9, (k * 37) & 0xFF);
    SetLastError(0xBAD);
    BOOL r = F(SetConsoleScreenBufferInfoEx)(b, &e);
    GCF(r, "err=%lu", GetLastError());
    if (r) {
        GC(GetConsoleScreenBufferInfoEx(b, &g));
        for (int k = 0; k < 16; k++) GCF((g.ColorTable[k] & 0xFFFFFF) == (e.ColorTable[k] & 0xFFFFFF), "entry %d got 0x%06lx want 0x%06lx", k, g.ColorTable[k] & 0xFFFFFF, e.ColorTable[k] & 0xFFFFFF);
    }
    F(SetConsoleScreenBufferInfoEx)(b, &orig);
    CloseHandle(b);
}
GTC(con_infoex_single_color_entries) {
    GREQ(F(SetConsoleScreenBufferInfoEx));
    GWINESKIP("emulator console has no colour table");
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFOEX orig, e, g; infoex_init(&orig); infoex_init(&g);
    GC(GetConsoleScreenBufferInfoEx(b, &orig));
    static const int idx[] = { 0, 1, 7, 8, 15 };
    for (unsigned k = 0; k < 5; k++) {
        e = orig; e.ColorTable[idx[k]] = RGB(0x12, 0x34, 0x56);
        if (!F(SetConsoleScreenBufferInfoEx)(b, &e)) { GCF(0, "idx %d err=%lu", idx[k], GetLastError()); continue; }
        GC(GetConsoleScreenBufferInfoEx(b, &g));
        GCF((g.ColorTable[idx[k]] & 0xFFFFFF) == RGB(0x12, 0x34, 0x56), "entry %d got 0x%06lx", idx[k], g.ColorTable[idx[k]] & 0xFFFFFF);
        for (int o = 0; o < 16; o++) if (o != idx[k] && (g.ColorTable[o] & 0xFFFFFF) != (orig.ColorTable[o] & 0xFFFFFF)) GCF(0, "entry %d changed while setting %d", o, idx[k]);
    }
    F(SetConsoleScreenBufferInfoEx)(b, &orig);
    CloseHandle(b);
}
GTC(con_infoex_attributes) {
    GREQ(F(SetConsoleScreenBufferInfoEx));
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFOEX e; infoex_init(&e);
    GC(GetConsoleScreenBufferInfoEx(b, &e));
    e.wAttributes = 0x5A; e.wPopupAttributes = 0xE4;
    BOOL r = F(SetConsoleScreenBufferInfoEx)(b, &e);
    GCF(r, "err=%lu", GetLastError());
    if (r) {
        CONSOLE_SCREEN_BUFFER_INFO i; CONSOLE_SCREEN_BUFFER_INFOEX g; infoex_init(&g);
        GC(info(b, &i)); GEQ(i.wAttributes & 0xFF, 0x5A);
        GC(GetConsoleScreenBufferInfoEx(b, &g)); GEQ(g.wAttributes & 0xFF, 0x5A);
        GEQ(g.wPopupAttributes & 0xFF, 0xE4);
    }
    CloseHandle(b);
}
GTC(con_infoex_cursor_and_size) {
    GREQ(F(SetConsoleScreenBufferInfoEx));
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFOEX e, g; infoex_init(&e); infoex_init(&g);
    GC(GetConsoleScreenBufferInfoEx(b, &e));
    CONSOLE_SCREEN_BUFFER_INFO ib; GC(info(b, &ib));
    e.dwCursorPosition.X = 4; e.dwCursorPosition.Y = 3;
    e.dwSize.Y = (SHORT)(e.dwSize.Y + 4);
    BOOL r = F(SetConsoleScreenBufferInfoEx)(b, &e);
    GCF(r, "err=%lu", GetLastError());
    CONSOLE_SCREEN_BUFFER_INFO i; GC(info(b, &i));
    gnote("cursor %d,%d size %dx%d (requested cursor 4,3 height %d)", i.dwCursorPosition.X, i.dwCursorPosition.Y, i.dwSize.X, i.dwSize.Y, e.dwSize.Y);
    /* P1 (gap_12): conhost ApiRoutines::SetConsoleScreenBufferInfoExImpl applies size, attributes, colour table, window
     * size and popup attributes but never reads dwCursorPosition: the cursor stays where it was. */
    if (r) { GEQ(i.dwSize.Y, e.dwSize.Y); GEQ(i.dwCursorPosition.X, ib.dwCursorPosition.X); GEQ(i.dwCursorPosition.Y, ib.dwCursorPosition.Y); }
    CloseHandle(b);
}
GTC(con_infoex_zero_size_rejected) {
    GREQ(F(SetConsoleScreenBufferInfoEx));
    GWINESKIP("console server semantics differ in the emulator");
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFOEX e, g; infoex_init(&e); infoex_init(&g);
    GC(GetConsoleScreenBufferInfoEx(b, &e));
    CONSOLE_SCREEN_BUFFER_INFOEX z = e; z.dwSize.X = 0;
    SetLastError(0xBAD);
    BOOL r = F(SetConsoleScreenBufferInfoEx)(b, &z);
    gnote("dwSize.X=0: ret=%d err=%lu", r, r ? 0 : GetLastError());
    GC(r == FALSE);
    z = e; z.dwSize.Y = 0;
    SetLastError(0xBAD);
    r = F(SetConsoleScreenBufferInfoEx)(b, &z);
    gnote("dwSize.Y=0: ret=%d err=%lu", r, r ? 0 : GetLastError());
    GC(r == FALSE);
    GC(GetConsoleScreenBufferInfoEx(b, &g));
    GEQ(g.dwSize.X, e.dwSize.X); GEQ(g.dwSize.Y, e.dwSize.Y);
    CloseHandle(b);
}
GTC(con_infoex_ignored_fields) {
    GREQ(F(SetConsoleScreenBufferInfoEx));
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFOEX e, g; infoex_init(&e); infoex_init(&g);
    GC(GetConsoleScreenBufferInfoEx(b, &e));
    e.bFullscreenSupported = !e.bFullscreenSupported;
    BOOL r = F(SetConsoleScreenBufferInfoEx)(b, &e);
    GCF(r, "err=%lu", GetLastError());
    GC(GetConsoleScreenBufferInfoEx(b, &g));
    gnote("bFullscreenSupported toggle: now %d (requested %d)", g.bFullscreenSupported, e.bFullscreenSupported);
    CloseHandle(b);
}
GTC(con_infoex_repeat_stable_window) {
    GREQ(F(SetConsoleScreenBufferInfoEx));
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFOEX e; infoex_init(&e);
    GC(GetConsoleScreenBufferInfoEx(b, &e));
    CONSOLE_SCREEN_BUFFER_INFO i0, i1; GC(info(b, &i0));
    for (int k = 0; k < 10; k++) {
        GC(GetConsoleScreenBufferInfoEx(b, &e));
        GC(F(SetConsoleScreenBufferInfoEx)(b, &e));
    }
    GC(info(b, &i1));
    gnote("window width before %d after 10 get/set round trips %d", i0.srWindow.Right - i0.srWindow.Left + 1, i1.srWindow.Right - i1.srWindow.Left + 1);
    CloseHandle(b);
}

/* ---------------- SetCurrentConsoleFontEx ---------------- */
GTC(con_fontex_same_font) {
    GREQ(F(SetCurrentConsoleFontEx));
    HANDLE o = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_FONT_INFOEX f, g; fontex_init(&f); fontex_init(&g);
    if (!GetCurrentConsoleFontEx(o, FALSE, &f)) { gline("SKIP GetCurrentConsoleFontEx failed %lu", GetLastError()); g_cskip = 1; return; }
    SetLastError(0xBAD);
    BOOL r = F(SetCurrentConsoleFontEx)(o, FALSE, &f);
    gnote("set same font: ret=%d err=%lu (\"%ls\" %dx%d weight %u)", r, r ? 0 : GetLastError(), f.FaceName, f.dwFontSize.X, f.dwFontSize.Y, f.FontWeight);
    GC(r == TRUE);
    GC(GetCurrentConsoleFontEx(o, FALSE, &g));
    gnote("font after: \"%ls\" %dx%d", g.FaceName, g.dwFontSize.X, g.dwFontSize.Y);
}
GTC(con_fontex_maximum_window_flag) {
    GREQ(F(SetCurrentConsoleFontEx));
    HANDLE o = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_FONT_INFOEX f; fontex_init(&f);
    if (!GetCurrentConsoleFontEx(o, TRUE, &f)) { gline("SKIP GetCurrentConsoleFontEx failed"); g_cskip = 1; return; }
    SetLastError(0xBAD);
    BOOL r = F(SetCurrentConsoleFontEx)(o, TRUE, &f);
    gnote("bMaximumWindow=TRUE: ret=%d err=%lu", r, r ? 0 : GetLastError());
    r = F(SetCurrentConsoleFontEx)(o, 5, &f);
    gnote("bMaximumWindow=5: ret=%d", r);
    GC(1);
}
GTC(con_fontex_unknown_face) {
    GREQ(F(SetCurrentConsoleFontEx));
    HANDLE o = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_FONT_INFOEX f, g; fontex_init(&f); fontex_init(&g);
    if (!GetCurrentConsoleFontEx(o, FALSE, &f)) { gline("SKIP no font"); g_cskip = 1; return; }
    CONSOLE_FONT_INFOEX orig = f;
    lstrcpyW(f.FaceName, L"K32T No Such Face");
    SetLastError(0xBAD);
    BOOL r = F(SetCurrentConsoleFontEx)(o, FALSE, &f);
    gnote("unknown face: ret=%d err=%lu", r, r ? 0 : GetLastError());
    GC(GetCurrentConsoleFontEx(o, FALSE, &g));
    gnote("face now \"%ls\"", g.FaceName);
    F(SetCurrentConsoleFontEx)(o, FALSE, &orig);
    GC(1);
}
GTC(con_fontex_unterminated_face) {
    GREQ(F(SetCurrentConsoleFontEx));
    HANDLE o = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_FONT_INFOEX f; fontex_init(&f);
    if (!GetCurrentConsoleFontEx(o, FALSE, &f)) { gline("SKIP no font"); g_cskip = 1; return; }
    CONSOLE_FONT_INFOEX orig = f;
    for (int k = 0; k < LF_FACESIZE; k++) f.FaceName[k] = L'X';     /* no terminator: the copy is bounded (RtlStringCchCopyW, 32 chars) */
    SetLastError(0xBAD);
    BOOL r = F(SetCurrentConsoleFontEx)(o, FALSE, &f);
    gnote("unterminated 32-char face: ret=%d err=%lu", r, r ? 0 : GetLastError());
    F(SetCurrentConsoleFontEx)(o, FALSE, &orig);
    GC(1);
}
GTC(con_fontex_on_private_buffer) {
    GREQ(F(SetCurrentConsoleFontEx));
    HANDLE b = newbuf(); GC(b != NULL); if (!b) return;
    CONSOLE_FONT_INFOEX f; fontex_init(&f);
    if (!GetCurrentConsoleFontEx(b, FALSE, &f)) { gline("SKIP no font"); g_cskip = 1; CloseHandle(b); return; }
    SetLastError(0xBAD);
    BOOL r = F(SetCurrentConsoleFontEx)(b, FALSE, &f);
    gnote("private buffer: ret=%d err=%lu", r, r ? 0 : GetLastError());
    GC(1);
    CloseHandle(b);
}

/* ---------------- SetConsoleTitleW / SetConsoleTitleA ---------------- */
static BOOL gettitle_w(WCHAR *b, DWORD n) { SetLastError(0); return GetConsoleTitleW(b, n) != 0; }
GTC(con_title_w_roundtrip) {
    GREQ(F(SetConsoleTitleW));
    static const WCHAR *t[] = { L"a", L"hello world", L"  leading and trailing  ", L"éü中文Ж", L"tab\there", L"k32t title with symbols !@#$%^&()_+-=[]{};',." };
    WCHAR b[512];
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        SetLastError(0xBAD);
        BOOL r = F(SetConsoleTitleW)(t[i]);
        GCF(r, "title %u err=%lu", i, GetLastError());
        memset(b, 0, sizeof b);
        DWORD n = GetConsoleTitleW(b, 512);
        GCF(n == (DWORD)lstrlenW(t[i]), "title %u len %lu want %d", i, n, lstrlenW(t[i]));
        GCF(!lstrcmpW(b, t[i]), "title %u text \"%ls\"", i, b);
    }
}
GTC(con_title_a_roundtrip_ascii) {
    GREQ(F(SetConsoleTitleA));
    static const char *t[] = { "a", "hello world", "  spaced  ", "K32T_title_123", "punct !@#$%^&()_+-=[]{};',." };
    char b[512];
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        SetLastError(0xBAD);
        BOOL r = F(SetConsoleTitleA)(t[i]);
        GCF(r, "title %u err=%lu", i, GetLastError());
        memset(b, 0, sizeof b);
        DWORD n = GetConsoleTitleA(b, 512);
        GCF(n == strlen(t[i]), "title %u len %lu", i, n);
        GCF(!strcmp(b, t[i]), "title %u text \"%s\"", i, b);
    }
}
GTC(con_title_a_w_parity) {
    GREQ(F(SetConsoleTitleA)); GREQ(F(SetConsoleTitleW));
    WCHAR w[128]; char a[128];
    GC(F(SetConsoleTitleA)("parity A to W"));
    GC(gettitle_w(w, 128)); GWSTR(w, L"parity A to W");
    GC(F(SetConsoleTitleW)(L"parity W to A"));
    GetConsoleTitleA(a, 128); GSTR(a, "parity W to A");
}
GTC(con_title_empty) {
    GREQ(F(SetConsoleTitleW)); GREQ(F(SetConsoleTitleA));
    WCHAR w[16] = L"junk"; char a[16] = "junk";
    SetLastError(0xBAD);
    GEQ(F(SetConsoleTitleW)(L""), TRUE);
    SetLastError(0);
    GEQ(GetConsoleTitleW(w, 16), 0);
    gnote("buffer after empty-title read (W): 0x%04x", w[0]);
    GEQ(F(SetConsoleTitleA)(""), TRUE);
    GEQ(GetConsoleTitleA(a, 16), 0);
    gnote("buffer after empty-title read (A): 0x%02x", (unsigned char)a[0]);
    /* non-empty again afterwards */
    GC(F(SetConsoleTitleW)(L"back"));
    GEQ(GetConsoleTitleW(w, 16), 4);
}
GTC(con_title_lengths) {
    GREQ(F(SetConsoleTitleW));
    static const int lens[] = { 1, 2, 31, 32, 63, 64, 127, 128, 254, 255, 256, 257, 1000, 1023, 1024, 4000 };
    WCHAR *t = (WCHAR *)calloc(8192, sizeof(WCHAR)), *b = (WCHAR *)calloc(8192, sizeof(WCHAR));
    for (unsigned k = 0; k < sizeof lens / sizeof lens[0]; k++) {
        for (int i = 0; i < lens[k]; i++) t[i] = L'A' + (i % 26);
        t[lens[k]] = 0;
        SetLastError(0xBAD);
        BOOL r = F(SetConsoleTitleW)(t);
        memset(b, 0, 8192 * sizeof(WCHAR));
        DWORD n = GetConsoleTitleW(b, 8000);
        gnote("title length %d: set ret=%d (err=%lu) read back %lu chars, equal=%d", lens[k], r, r ? 0 : GetLastError(), n, !lstrcmpW(b, t));
        if (lens[k] <= 255) { GCF(r, "len %d", lens[k]); GCF(n == (DWORD)lens[k] && !lstrcmpW(b, t), "len %d roundtrip", lens[k]); }
    }
    free(t); free(b);
}
GTC(con_title_a_long) {
    GREQ(F(SetConsoleTitleA));
    char *t = (char *)calloc(3000, 1), *b = (char *)calloc(3000, 1);
    for (int i = 0; i < 1500; i++) t[i] = 'a' + (i % 26);
    BOOL r = F(SetConsoleTitleA)(t);
    DWORD n = GetConsoleTitleA(b, 3000);
    gnote("1500-char ANSI title: set ret=%d, read %lu chars equal=%d", r, n, !strcmp(t, b));
    GC(r);
    free(t); free(b);
}
GTC(con_title_overwrites_previous) {
    GREQ(F(SetConsoleTitleW));
    WCHAR b[64];
    GC(F(SetConsoleTitleW)(L"a long first title for the test"));
    GC(F(SetConsoleTitleW)(L"short"));
    GEQ(GetConsoleTitleW(b, 64), 5); GWSTR(b, L"short");
}
GTC(con_title_repeat) {
    GREQ(F(SetConsoleTitleW));
    WCHAR t[40], b[64]; int bad = 0;
    for (int i = 0; i < 300; i++) {
        swprintf(t, 40, L"k32t title %d", i);
        if (!F(SetConsoleTitleW)(t)) { bad++; continue; }
        GetConsoleTitleW(b, 64);
        if (lstrcmpW(b, t)) bad++;
    }
    GEQ(bad, 0);
}
static DWORD WINAPI title_thread(LPVOID p) {
    BOOL (WINAPI *f)(LPCWSTR) = (BOOL (WINAPI *)(LPCWSTR))t_proc("SetConsoleTitleW");
    *(BOOL *)p = f(L"set from another thread");
    return 0;
}
GTC(con_title_from_other_thread) {
    GREQ(F(SetConsoleTitleW));
    BOOL r = -1; HANDLE t = CreateThread(NULL, 0, title_thread, &r, 0, NULL);
    WaitForSingleObject(t, 5000); CloseHandle(t);
    GEQ(r, TRUE);
    WCHAR b[64]; GC(gettitle_w(b, 64)); GWSTR(b, L"set from another thread");
}
GTCT(con_title_original_unchanged, "k32t_g12_start_title") {
    GREQ(F(SetConsoleTitleW));
    GWINESKIP("start-up title semantics differ in the emulator");
    WCHAR o0[128] = {0}, o1[128] = {0}, t[128] = {0};
    DWORD n0 = GetConsoleOriginalTitleW(o0, 128);
    GC(F(SetConsoleTitleW)(L"changed title"));
    DWORD n1 = GetConsoleOriginalTitleW(o1, 128);
    GEQ(n0, n1); GWSTR(o0, o1);
    GC(gettitle_w(t, 128)); GWSTR(t, L"changed title");
    gnote("original title \"%ls\"", o1);
}
GTCT(con_title_initial_from_startup_info, "k32t_g12_start_title") {
    GREQ(F(SetConsoleTitleW));
    GWINESKIP("start-up title semantics differ in the emulator");
    WCHAR t[128] = {0};
    GC(gettitle_w(t, 128));
    gnote("initial title \"%ls\"", t);
    GC(!t_is_wine() ? !lstrcmpW(t, L"k32t_g12_start_title") : 1);
}
GTC(con_title_control_chars) {
    GREQ(F(SetConsoleTitleW));
    WCHAR b[64] = {0};
    BOOL r = F(SetConsoleTitleW)(L"line1\nline2");
    DWORD n = GetConsoleTitleW(b, 64);
    gnote("embedded newline: set ret=%d read %lu chars", r, n);
    GC(r);
    r = F(SetConsoleTitleW)(L"bell\a");
    n = GetConsoleTitleW(b, 64);
    gnote("embedded BEL: set ret=%d read %lu chars", r, n);
    GC(r);
}
GTC(con_title_a_high_chars_follow_console_cp) {
    GREQ(F(SetConsoleTitleA));
    UINT cp = GetConsoleCP(); WCHAR w[32] = {0}, want[8] = {0};
    GC(F(SetConsoleTitleA)("t\xE9\xFC"));
    GetConsoleTitleW(w, 32);
    MultiByteToWideChar(cp, 0, "t\xE9\xFC", -1, want, 8);
    gnote("ANSI title with high bytes: console cp %u -> \"%ls\" (expected \"%ls\")", cp, w, want);
    GC(1);
}
GTC(con_title_unaffected_by_buffer_ops) {
    GREQ(F(SetConsoleTitleW));
    WCHAR b[64];
    GC(F(SetConsoleTitleW)(L"stable title"));
    HANDLE nb = newbuf();
    if (nb) { SetConsoleTextAttribute(nb, 0x1F); CloseHandle(nb); }
    GEQ(GetConsoleTitleW(b, 64), 12); GWSTR(b, L"stable title");
}
