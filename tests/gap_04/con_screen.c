/* gap_04: console title, cursor, display mode, fonts, code page, screen buffer info, selection */
#include "ch.h"

#define F0(call, err) do { SetLastError(0xBAD); DWORD r_ = (DWORD)(call); CEQ(r_, 0); CERR(err); } while (0)
#define FB(call, err) do { SetLastError(0xBAD); BOOL r_ = (call); CEQ(r_, 0); CERR(err); } while (0)

static DWORD n2d(LONG st) {
    typeof(&RtlNtStatusToDosError) f = (typeof(&RtlNtStatusToDosError))UNT(RtlNtStatusToDosError);
    return f ? f(st) : 0xFFFFFFFF;
}
#define TITLE0 "k32t gap4 start title"

/* ---------------- GetConsoleTitleW / A ---------------- */

CT(gap4_title_w_roundtrip_and_sizes) {
    CREQ(SetConsoleTitleW); CREQ(GetConsoleTitleW);
    CC(SetConsoleTitleW(L"abcdef"));
    WCHAR b[32];
    for (int i = 0; i < 32; i++) b[i] = 0x7777;
    SetLastError(0xBAD);
    CEQ(GetConsoleTitleW(b, 32), 6);
    CWSTR(b, L"abcdef");
    CEQ(b[7], 0x7777);
    /* nSize == len+1 : complete */
    for (int i = 0; i < 32; i++) b[i] = 0x7777;
    CEQ(GetConsoleTitleW(b, 7), 6);
    CWSTR(b, L"abcdef");
    /* nSize == len : last character is replaced by the terminator */
    for (int i = 0; i < 32; i++) b[i] = 0x7777;
    DWORD r = GetConsoleTitleW(b, 6);
    cnote("nSize==len returns %lu", r);
    CCF(r >= 6, "ret %lu", r);
    CWSTR(b, L"abcde");
    /* shorter */
    for (int i = 0; i < 32; i++) b[i] = 0x7777;
    r = GetConsoleTitleW(b, 3);
    CCF(r >= 3, "ret %lu", r);
    CWSTR(b, L"ab");
    CEQ(b[3], 0x7777);
    /* nSize 1: only the terminator */
    for (int i = 0; i < 32; i++) b[i] = 0x7777;
    r = GetConsoleTitleW(b, 1);
    CCF(r >= 1, "ret %lu", r);
    CEQ(b[0], 0); CEQ(b[1], 0x7777);
}

CT(gap4_title_w_size_zero) {
    CREQ(SetConsoleTitleW); CREQ(GetConsoleTitleW);
    CC(SetConsoleTitleW(L"nonempty"));
    WCHAR b[4]; for (int i = 0; i < 4; i++) b[i] = 0x7777;
    SetLastError(0xBAD);
    CEQ(GetConsoleTitleW(b, 0), 0);
    CEQ(b[0], 0x7777);
    CEQ(GetConsoleTitleW(NULL, 0), 0);
}

CT(gap4_title_a_roundtrip_and_sizes) {
    CREQ(SetConsoleTitleA); CREQ(GetConsoleTitleA);
    CC(SetConsoleTitleA("hello title"));
    char b[40]; memset(b, 0x55, sizeof b);
    SetLastError(0xBAD);
    CEQ(GetConsoleTitleA(b, 40), 11);
    CSTR(b, "hello title");
    CEQ((unsigned char)b[12], 0x55);
    memset(b, 0x55, sizeof b);
    CEQ(GetConsoleTitleA(b, 12), 11);
    CSTR(b, "hello title");
    memset(b, 0x55, sizeof b);
    DWORD r = GetConsoleTitleA(b, 5);
    cnote("GetConsoleTitleA(size 5) -> %lu \"%s\"", r, b);
    /* documented: a buffer too small for the title yields 0 (content unspecified) */
    CEQ(r, 0);
    memset(b, 0x55, sizeof b);
    CEQ(GetConsoleTitleA(b, 0), 0);
    CEQ((unsigned char)b[0], 0x55);
}

CT(gap4_title_a_w_parity_ascii) {
    CREQ(SetConsoleTitleW); CREQ(GetConsoleTitleA); CREQ(GetConsoleTitleW); CREQ(SetConsoleTitleA);
    char a[64]; WCHAR w[64];
    CC(SetConsoleTitleW(L"Parity Title 123"));
    DWORD na = GetConsoleTitleA(a, 64), nw = GetConsoleTitleW(w, 64);
    CEQ(na, 16); CEQ(nw, 16);
    CSTR(a, "Parity Title 123"); CWSTR(w, L"Parity Title 123");
    CC(SetConsoleTitleA("From The A Side"));
    nw = GetConsoleTitleW(w, 64);
    CEQ(nw, 15); CWSTR(w, L"From The A Side");
}

CT(gap4_title_long) {
    CREQ(SetConsoleTitleW); CREQ(GetConsoleTitleW);
    static WCHAR t[1100], g[1200];
    for (int i = 0; i < 1000; i++) t[i] = L'a' + (i % 26);
    t[1000] = 0;
    CC(SetConsoleTitleW(t));
    for (int i = 0; i < 1200; i++) g[i] = 0x7777;
    SetLastError(0xBAD);
    CEQ(GetConsoleTitleW(g, 1200), 1000);
    CWSTR(g, t);
    CEQ(g[1001], 0x7777);
    for (int i = 0; i < 1200; i++) g[i] = 0x7777;
    DWORD r = GetConsoleTitleW(g, 500);
    CCF(r >= 500, "ret %lu", r);
    CEQ(lstrlenW(g), 499);
}

CT(gap4_title_empty_title) {
    CREQ(SetConsoleTitleW); CREQ(GetConsoleTitleW); CREQ(GetConsoleTitleA);
    CC(SetConsoleTitleW(L""));
    WCHAR w[8]; char a[8];
    for (int i = 0; i < 8; i++) { w[i] = 0x7777; }
    memset(a, 0x55, sizeof a);
    CEQ(GetConsoleTitleW(w, 8), 0);
    CEQ(w[0], 0);
    CEQ(GetConsoleTitleA(a, 8), 0);
}

CT(gap4_title_independent_of_original) {
    CREQ(SetConsoleTitleW); CREQ(GetConsoleTitleW); CREQ(GetConsoleOriginalTitleW);
    if (t_is_wine()) { cline("SKIP emulator original title is empty"); return; }
    WCHAR o1[128], o2[128], c[128];
    memset(o1, 0, sizeof o1); memset(o2, 0, sizeof o2); memset(c, 0, sizeof c);
    DWORD n1 = GetConsoleOriginalTitleW(o1, 128);
    CC(SetConsoleTitleW(L"changed title"));
    DWORD n2 = GetConsoleOriginalTitleW(o2, 128);
    CEQ(n1, n2);
    CWSTR(o1, o2);
    GetConsoleTitleW(c, 128);
    CWSTR(c, L"changed title");
    CC(lstrcmpW(o2, L"changed title") != 0);
}

CTF(gap4_title_original_is_startup_title, CREATE_NEW_CONSOLE, TITLE0) {
    CREQ(GetConsoleOriginalTitleW); CREQ(GetConsoleOriginalTitleA); CREQ(SetConsoleTitleA); CREQ(GetConsoleTitleA);
    if (t_is_wine()) { cline("SKIP emulator does not apply STARTUPINFO title to the console"); return; }
    WCHAR w[128]; char a[128];
    memset(a, 0, sizeof a); memset(w, 0, sizeof w);
    SetLastError(0xBAD);
    DWORD nw = GetConsoleOriginalTitleW(w, 128), na = GetConsoleOriginalTitleA(a, 128);
    cnote("original title W=%lu A=%lu", nw, na);
    CEQ(nw, sizeof(TITLE0) - 1);
    CEQ(na, sizeof(TITLE0) - 1);
    CWSTR(w, L"k32t gap4 start title");
    CSTR(a, TITLE0);
    CC(SetConsoleTitleA("temp"));
    memset(a, 0, sizeof a);
    na = GetConsoleOriginalTitleA(a, 128);
    CEQ(na, sizeof(TITLE0) - 1);
    CSTR(a, TITLE0);
    GetConsoleTitleA(a, 128);
    CSTR(a, "temp");
}

CTF(gap4_title_current_initially_startup_title, CREATE_NEW_CONSOLE, TITLE0) {
    CREQ(GetConsoleTitleA); CREQ(GetConsoleTitleW);
    if (t_is_wine()) { cline("SKIP emulator does not apply STARTUPINFO title to the console"); return; }
    char a[128]; WCHAR w[128];
    CEQ(GetConsoleTitleA(a, 128), sizeof(TITLE0) - 1);
    CSTR(a, TITLE0);
    CEQ(GetConsoleTitleW(w, 128), sizeof(TITLE0) - 1);
}

CTF(gap4_title_original_buffer_edges, CREATE_NEW_CONSOLE, TITLE0) {
    CREQ(GetConsoleOriginalTitleW); CREQ(GetConsoleOriginalTitleA);
    if (t_is_wine()) { cline("SKIP emulator does not apply STARTUPINFO title to the console"); return; }
    WCHAR w[64]; char a[64];
    const DWORD L = sizeof(TITLE0) - 1;
    for (int i = 0; i < 64; i++) w[i] = 0x7777;
    SetLastError(0xBAD);
    CEQ(GetConsoleOriginalTitleW(w, 0), 0);
    CEQ(w[0], 0x7777);
    CEQ(GetConsoleOriginalTitleW(NULL, 0), 0);
    memset(a, 0x55, sizeof a);
    CEQ(GetConsoleOriginalTitleA(a, 0), 0);
    CEQ((unsigned char)a[0], 0x55);
    memset(w, 0, sizeof w);
    DWORD r = GetConsoleOriginalTitleW(w, L + 1);
    cnote("original title with nSize=len+1 -> %lu", r);
    CEQ(r, L);
    CWSTR(w, L"k32t gap4 start title");
    for (int i = 0; i < 64; i++) w[i] = 0x7777;
    r = GetConsoleOriginalTitleW(w, 5);
    cnote("original title nSize=5 -> %lu  first=%x", r, w[0]);
    CEQ(w[6], 0x7777);
}

CTD(gap4_title_detached) {
    CREQ(GetConsoleTitleW); CREQ(GetConsoleTitleA); CREQ(GetConsoleOriginalTitleW); CREQ(GetConsoleOriginalTitleA);
    WCHAR w[16]; char a[16];
    for (int i = 0; i < 16; i++) { w[i] = 0x7777; }
    memset(a, 0x55, sizeof a);
    SetLastError(0xBAD);
    CEQ(GetConsoleTitleW(w, 16), 0);
    CERR(ERROR_INVALID_HANDLE);
    CEQ(w[0], 0x7777);
    SetLastError(0xBAD);
    CEQ(GetConsoleTitleA(a, 16), 0);
    cnote("detached GetConsoleTitleA err=%lu", GetLastError());
    SetLastError(0xBAD);
    CEQ(GetConsoleOriginalTitleW(w, 16), 0);
    CERR(ERROR_INVALID_HANDLE);
    CEQ(GetConsoleOriginalTitleA(a, 16), 0);
}

/* ---------------- GetConsoleOutputCP ---------------- */

CT(gap4_outputcp_basic_and_roundtrip) {
    CREQ(GetConsoleOutputCP); CREQ(SetConsoleOutputCP);
    UINT orig = GetConsoleOutputCP();
    cnote("initial output code page %u (OEM %u)", orig, GetOEMCP());
    CNE(orig, 0);
    CEQ(GetConsoleOutputCP(), orig);
    SetLastError(0xBAD);
    CCF(SetConsoleOutputCP(65001), "err=%lu", GetLastError());
    CEQ(GetConsoleOutputCP(), 65001);
    if (SetConsoleOutputCP(437)) CEQ(GetConsoleOutputCP(), 437);
    if (SetConsoleOutputCP(850)) CEQ(GetConsoleOutputCP(), 850);
    CC(SetConsoleOutputCP(orig));
    CEQ(GetConsoleOutputCP(), orig);
}

CT(gap4_outputcp_independent_of_input_cp) {
    CREQ(GetConsoleOutputCP); CREQ(SetConsoleOutputCP); CREQ(GetConsoleCP); CREQ(SetConsoleCP);
    UINT in0 = GetConsoleCP(), out0 = GetConsoleOutputCP();
    CNE(in0, 0); CNE(out0, 0);
    CC(SetConsoleOutputCP(65001));
    CEQ(GetConsoleCP(), in0);
    CC(SetConsoleOutputCP(out0));
    CC(SetConsoleCP(65001));
    CEQ(GetConsoleOutputCP(), out0);
    CEQ(GetConsoleCP(), 65001);
    CC(SetConsoleCP(in0));
}

CT(gap4_outputcp_failed_set_leaves_value) {
    CREQ(GetConsoleOutputCP); CREQ(SetConsoleOutputCP);
    UINT orig = GetConsoleOutputCP();
    static const UINT bad[] = { 0, 1, 2, 3, 0xFFFF, 0x12345, 0xFFFFFFFF, 65535 };
    for (int i = 0; i < 8; i++) {
        SetLastError(0xBAD);
        BOOL r = SetConsoleOutputCP(bad[i]);
        CCF(!r, "cp %u accepted", bad[i]);
        CEQ(GetConsoleOutputCP(), orig);
    }
}

CT(gap4_outputcp_repeat_stable) {
    CREQ(GetConsoleOutputCP);
    UINT a = GetConsoleOutputCP();
    for (int i = 0; i < 200; i++) if (GetConsoleOutputCP() != a) { CCF(0, "changed at %d", i); break; }
    CC(1);
}

CTD(gap4_outputcp_detached) {
    CREQ(GetConsoleOutputCP);
    SetLastError(0xBAD);
    CEQ(GetConsoleOutputCP(), 0);
    CERR(ERROR_INVALID_HANDLE);
}

/* ---------------- GetConsoleCursorInfo ---------------- */

CT(gap4_cursor_defaults_new_buffer) {
    CREQ(GetConsoleCursorInfo); CREQ(CreateConsoleScreenBuffer);
    HANDLE b = mkbuf_raw(); CNE(b, NULL); if (!b) return;
    CONSOLE_CURSOR_INFO ci; memset(&ci, 0xCC, sizeof ci);
    SetLastError(0xBAD);
    CCF(GetConsoleCursorInfo(b, &ci), "err=%lu", GetLastError());
    cnote("new buffer cursor: size=%lu visible=%d", ci.dwSize, ci.bVisible);
    CC(ci.dwSize >= 1 && ci.dwSize <= 100);
    CC(ci.bVisible == 0 || ci.bVisible == 1);
    CloseHandle(b);
}

CT(gap4_cursor_active_buffer) {
    CREQ(GetConsoleCursorInfo);
    CONSOLE_CURSOR_INFO ci; memset(&ci, 0xCC, sizeof ci);
    CCF(GetConsoleCursorInfo(hout_(), &ci), "err=%lu", GetLastError());
    CC(ci.dwSize >= 1 && ci.dwSize <= 100);
    CC(ci.bVisible == 0 || ci.bVisible == 1);
}

CT(gap4_cursor_set_get_roundtrip) {
    CREQ(GetConsoleCursorInfo); CREQ(SetConsoleCursorInfo);
    HANDLE b = mkbuf_raw(); CNE(b, NULL); if (!b) return;
    static const DWORD sizes[] = { 1, 2, 25, 50, 99, 100 };
    for (int i = 0; i < 6; i++) { for (int vis = 0; vis < 2; vis++) {
        CONSOLE_CURSOR_INFO s, g; s.dwSize = sizes[i]; s.bVisible = vis; memset(&g, 0xCC, sizeof g);
        CCF(SetConsoleCursorInfo(b, &s), "size %lu err=%lu", sizes[i], GetLastError());
        CC(GetConsoleCursorInfo(b, &g));
        CCF(g.dwSize == sizes[i] && g.bVisible == vis, "size %lu vis %d got %lu/%d", sizes[i], vis, g.dwSize, g.bVisible);
    } }
    CloseHandle(b);
}

CT(gap4_cursor_nonzero_visible_normalised) {
    CREQ(GetConsoleCursorInfo); CREQ(SetConsoleCursorInfo);
    HANDLE b = mkbuf_raw(); CNE(b, NULL); if (!b) return;
    CONSOLE_CURSOR_INFO s, g; s.dwSize = 30; s.bVisible = 5; memset(&g, 0xCC, sizeof g);
    CC(SetConsoleCursorInfo(b, &s));
    CC(GetConsoleCursorInfo(b, &g));
    CEQ(g.bVisible, 1);
    CloseHandle(b);
}

CT(gap4_cursor_buffers_independent) {
    CREQ(GetConsoleCursorInfo); CREQ(SetConsoleCursorInfo);
    HANDLE b1 = mkbuf_raw(), b2 = mkbuf_raw(); CNE(b1, NULL); CNE(b2, NULL);
    if (!b1 || !b2) return;
    CONSOLE_CURSOR_INFO s, g;
    s.dwSize = 11; s.bVisible = FALSE; CC(SetConsoleCursorInfo(b1, &s));
    s.dwSize = 77; s.bVisible = TRUE;  CC(SetConsoleCursorInfo(b2, &s));
    CC(GetConsoleCursorInfo(b1, &g)); CEQ(g.dwSize, 11); CEQ(g.bVisible, 0);
    CC(GetConsoleCursorInfo(b2, &g)); CEQ(g.dwSize, 77); CEQ(g.bVisible, 1);
    CloseHandle(b1); CloseHandle(b2);
}

CT(gap4_cursor_invalid_handles) {
    CREQ(GetConsoleCursorInfo);
    CONSOLE_CURSOR_INFO ci; memset(&ci, 0xCC, sizeof ci);
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x1234, (HANDLE)(ULONG_PTR)0xFFFFFFFF };
    for (int i = 0; i < 4; i++) {
        FB(GetConsoleCursorInfo(bad[i], &ci), ERROR_INVALID_HANDLE);
        CEQ(ci.dwSize, 0xCCCCCCCC);
    }
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    FB(GetConsoleCursorInfo(ev, &ci), n2d((LONG)0xC0000024));
    CloseHandle(ev);
}

CT(gap4_cursor_closed_handle) {
    CREQ(GetConsoleCursorInfo);
    HANDLE b = mkbuf_raw(); CNE(b, NULL); if (!b) return;
    CloseHandle(b);
    CONSOLE_CURSOR_INFO ci;
    FB(GetConsoleCursorInfo(b, &ci), ERROR_INVALID_HANDLE);
}

CT(gap4_cursor_input_handle) {
    CREQ(GetConsoleCursorInfo);
    CONSOLE_CURSOR_INFO ci; memset(&ci, 0xCC, sizeof ci);
    SetLastError(0xBAD);
    BOOL r = GetConsoleCursorInfo(hin_(), &ci);
    cnote("cursor info on the input handle -> %d err=%lu", r, GetLastError());
    CEQ(r, 0);
}

CT(gap4_cursor_dup_handle_shares_state) {
    CREQ(GetConsoleCursorInfo); CREQ(SetConsoleCursorInfo);
    HANDLE b = mkbuf_raw(), d = NULL; CNE(b, NULL); if (!b) return;
    CC(DuplicateHandle(GetCurrentProcess(), b, GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS));
    CONSOLE_CURSOR_INFO s = { 42, FALSE }, g;
    CC(SetConsoleCursorInfo(b, &s));
    CC(GetConsoleCursorInfo(d, &g));
    CEQ(g.dwSize, 42); CEQ(g.bVisible, 0);
    CloseHandle(d); CloseHandle(b);
}

/* ---------------- GetConsoleDisplayMode ---------------- */

CT(gap4_displaymode_windowed) {
    CREQ(GetConsoleDisplayMode);
    DWORD f = 0xCCCCCCCC;
    SetLastError(0xBAD);
    BOOL r = GetConsoleDisplayMode(&f);
    cnote("display mode ret=%d flags=0x%lx err=%lu", r, f, GetLastError());
    CCF(r, "err=%lu", GetLastError());
    CCF((f & ~3u) == 0, "flags 0x%lx", f);
}

CT(gap4_displaymode_stable_across_calls) {
    CREQ(GetConsoleDisplayMode);
    DWORD a = 1, b = 2;
    CC(GetConsoleDisplayMode(&a)); CC(GetConsoleDisplayMode(&b));
    CEQ(a, b);
}

CTD(gap4_displaymode_detached) {
    CREQ(GetConsoleDisplayMode);
    DWORD f = 0xCCCCCCCC;
    SetLastError(0xBAD);
    BOOL r = GetConsoleDisplayMode(&f);
    CEQ(r, 0);
    CEQ(f, 0xCCCCCCCC);
    CERR(ERROR_INVALID_HANDLE);
}

/* ---------------- GetConsoleSelectionInfo ---------------- */

CT(gap4_selection_none) {
    CREQ(GetConsoleSelectionInfo);
    CONSOLE_SELECTION_INFO si; memset(&si, 0xCC, sizeof si);
    SetLastError(0xBAD);
    BOOL r = GetConsoleSelectionInfo(&si);
    cnote("selection info ret=%d err=%lu flags=0x%lx anchor=(%d,%d) rect=(%d,%d,%d,%d)", r, GetLastError(), si.dwFlags,
        si.dwSelectionAnchor.X, si.dwSelectionAnchor.Y, si.srSelection.Left, si.srSelection.Top, si.srSelection.Right, si.srSelection.Bottom);
    if (r) {
        CEQ(si.dwFlags & CONSOLE_SELECTION_IN_PROGRESS, 0);
        CEQ(si.dwFlags & CONSOLE_SELECTION_NOT_EMPTY, 0);
    }
}

CT(gap4_selection_struct_size) {
    CC(sizeof(CONSOLE_SELECTION_INFO) == 16);
    CC(offsetof(CONSOLE_SELECTION_INFO, dwSelectionAnchor) == 4);
    CC(offsetof(CONSOLE_SELECTION_INFO, srSelection) == 8);
}

CTD(gap4_selection_detached) {
    CREQ(GetConsoleSelectionInfo);
    CONSOLE_SELECTION_INFO si; memset(&si, 0xCC, sizeof si);
    SetLastError(0xBAD);
    BOOL r = GetConsoleSelectionInfo(&si);
    CEQ(r, 0);
    CERR(ERROR_INVALID_HANDLE);
    CEQ(si.dwFlags, 0xCCCCCCCC);
}

/* ---------------- fonts ---------------- */

CT(gap4_font_size_current_index) {
    CREQ(GetCurrentConsoleFont); CREQ(GetConsoleFontSize);
    if (t_is_wine()) { cline("SKIP headless emulator console has no font metrics"); return; }
    CONSOLE_FONT_INFO fi; memset(&fi, 0xCC, sizeof fi);
    CCF(GetCurrentConsoleFont(hout_(), FALSE, &fi), "err=%lu", GetLastError());
    COORD c = GetConsoleFontSize(hout_(), fi.nFont);
    cnote("current font index %lu size %dx%d (struct %dx%d)", fi.nFont, c.X, c.Y, fi.dwFontSize.X, fi.dwFontSize.Y);
    CC(c.X > 0 && c.Y > 0);
    if (c.X != fi.dwFontSize.X || c.Y != fi.dwFontSize.Y) cnote("size by index differs from size in the font record");
}

CT(gap4_font_size_bad_handles) {
    CREQ(GetConsoleFontSize);
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x1234, (HANDLE)(ULONG_PTR)0xFFFFFFFF };
    for (int i = 0; i < 4; i++) {
        SetLastError(0xBAD);
        COORD c = GetConsoleFontSize(bad[i], 0);
        CEQ(c.X, 0); CEQ(c.Y, 0);
        CERR(ERROR_INVALID_HANDLE);
    }
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SetLastError(0xBAD);
    COORD c = GetConsoleFontSize(ev, 0);
    CEQ(c.X, 0); CEQ(c.Y, 0);
    CERR(n2d((LONG)0xC0000024));
    CloseHandle(ev);
}

CT(gap4_font_size_out_of_range_index) {
    CREQ(GetConsoleFontSize);
    static const DWORD idx[] = { 0xFFFF, 0x10000, 0x7FFFFFFF, 0xFFFFFFFF };
    for (int i = 0; i < 4; i++) {
        SetLastError(0xBAD);
        COORD c = GetConsoleFontSize(hout_(), idx[i]);
        cnote("font index 0x%lx -> %dx%d err=%lu", idx[i], c.X, c.Y, GetLastError());
    }
}

CT(gap4_font_size_repeat_stable) {
    CREQ(GetConsoleFontSize);
    COORD a = GetConsoleFontSize(hout_(), 0);
    for (int i = 0; i < 100; i++) { COORD b = GetConsoleFontSize(hout_(), 0); if (b.X != a.X || b.Y != a.Y) { CCF(0, "changed at %d", i); break; } }
    CC(1);
}

CT(gap4_font_current_vs_ex_consistent) {
    CREQ(GetCurrentConsoleFont); CREQ(GetCurrentConsoleFontEx);
    CONSOLE_FONT_INFO fi; CONSOLE_FONT_INFOEX fx;
    memset(&fi, 0xCC, sizeof fi); memset(&fx, 0xCC, sizeof fx); fx.cbSize = sizeof fx;
    CCF(GetCurrentConsoleFont(hout_(), FALSE, &fi), "err=%lu", GetLastError());
    CCF(GetCurrentConsoleFontEx(hout_(), FALSE, &fx), "err=%lu", GetLastError());
    CEQ(fi.nFont, fx.nFont);
    CEQ(fi.dwFontSize.X, fx.dwFontSize.X); CEQ(fi.dwFontSize.Y, fx.dwFontSize.Y);
    CEQ(fx.cbSize, sizeof fx);
}

CT(gap4_font_ex_sizeof_and_cbsize) {
    CREQ(GetCurrentConsoleFontEx);
    CC(sizeof(CONSOLE_FONT_INFOEX) == 0x54);
    CONSOLE_FONT_INFOEX fx;
    static const ULONG sizes[] = { 0, 1, 0x50, 0x53, 0x55, 0x58, 0x60, 0x100, 0xFFFFFFFF };
    for (int i = 0; i < 9; i++) {
        memset(&fx, 0xCC, sizeof fx); fx.cbSize = sizes[i];
        FB(GetCurrentConsoleFontEx(hout_(), FALSE, &fx), ERROR_INVALID_PARAMETER);
        FB(GetCurrentConsoleFontEx(hout_(), TRUE, &fx), ERROR_INVALID_PARAMETER);
        CEQ(fx.nFont, 0xCCCCCCCC);
    }
}

CT(gap4_font_ex_cbsize_checked_before_handle) {
    CREQ(GetCurrentConsoleFontEx);
    CONSOLE_FONT_INFOEX fx; memset(&fx, 0, sizeof fx); fx.cbSize = 7;
    FB(GetCurrentConsoleFontEx(INVALID_HANDLE_VALUE, FALSE, &fx), ERROR_INVALID_PARAMETER);
    FB(GetCurrentConsoleFontEx(NULL, TRUE, &fx), ERROR_INVALID_PARAMETER);
}

CT(gap4_font_ex_contents) {
    CREQ(GetCurrentConsoleFontEx);
    CONSOLE_FONT_INFOEX fx; memset(&fx, 0xCC, sizeof fx); fx.cbSize = sizeof fx;
    CCF(GetCurrentConsoleFontEx(hout_(), FALSE, &fx), "err=%lu", GetLastError());
    int term = 0; for (int i = 0; i < 32; i++) if (!fx.FaceName[i]) { term = 1; break; }
    CCF(term, "FaceName not terminated");
    cnote("font: index=%lu size=%dx%d family=0x%x weight=%u face=\"%ls\"", fx.nFont, fx.dwFontSize.X, fx.dwFontSize.Y, fx.FontFamily, fx.FontWeight, fx.FaceName);
    CC(fx.FontWeight <= 1000);
    if (!t_is_wine()) CC(fx.dwFontSize.Y > 0);
}

CT(gap4_font_ex_maximum_window_flag) {
    CREQ(GetCurrentConsoleFontEx);
    CONSOLE_FONT_INFOEX a, b;
    memset(&a, 0, sizeof a); memset(&b, 0, sizeof b); a.cbSize = b.cbSize = sizeof a;
    CC(GetCurrentConsoleFontEx(hout_(), FALSE, &a));
    CC(GetCurrentConsoleFontEx(hout_(), TRUE, &b));
    cnote("font size normal=%dx%d maxwindow=%dx%d", a.dwFontSize.X, a.dwFontSize.Y, b.dwFontSize.X, b.dwFontSize.Y);
    CC(b.cbSize == sizeof b);
    int term = 0; for (int i = 0; i < 32; i++) if (!b.FaceName[i]) { term = 1; break; }
    CC(term);
}

CT(gap4_font_ex_bad_handles) {
    CREQ(GetCurrentConsoleFontEx); CREQ(GetCurrentConsoleFont);
    CONSOLE_FONT_INFOEX fx; CONSOLE_FONT_INFO fi;
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x1234 };
    for (int i = 0; i < 3; i++) {
        memset(&fx, 0xCC, sizeof fx); fx.cbSize = sizeof fx; memset(&fi, 0xCC, sizeof fi);
        FB(GetCurrentConsoleFontEx(bad[i], FALSE, &fx), ERROR_INVALID_HANDLE);
        FB(GetCurrentConsoleFont(bad[i], FALSE, &fi), ERROR_INVALID_HANDLE);
        CEQ(fi.nFont, 0xCCCCCCCC);
        CEQ(fx.FontWeight, 0xCCCCCCCC);
    }
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    memset(&fx, 0, sizeof fx); fx.cbSize = sizeof fx;
    FB(GetCurrentConsoleFontEx(ev, FALSE, &fx), n2d((LONG)0xC0000024));
    FB(GetCurrentConsoleFont(ev, TRUE, &fi), n2d((LONG)0xC0000024));
    CloseHandle(ev);
}

CT(gap4_font_new_buffer) {
    CREQ(GetCurrentConsoleFontEx); CREQ(GetCurrentConsoleFont);
    HANDLE b = mkbuf_raw(); CNE(b, NULL); if (!b) return;
    CONSOLE_FONT_INFOEX f0, f1; CONSOLE_FONT_INFO fi;
    memset(&f0, 0, sizeof f0); memset(&f1, 0, sizeof f1); f0.cbSize = f1.cbSize = sizeof f0;
    CCF(GetCurrentConsoleFontEx(b, FALSE, &f1), "err=%lu", GetLastError());
    CC(GetCurrentConsoleFontEx(hout_(), FALSE, &f0));
    cnote("new buffer font %dx%d, std buffer font %dx%d", f1.dwFontSize.X, f1.dwFontSize.Y, f0.dwFontSize.X, f0.dwFontSize.Y);
    CCF(GetCurrentConsoleFont(b, FALSE, &fi), "err=%lu", GetLastError());
    CEQ(fi.nFont, f1.nFont);
    CloseHandle(b);
}

CT(gap4_font_closed_handle) {
    CREQ(GetCurrentConsoleFont); CREQ(GetConsoleFontSize);
    HANDLE b = mkbuf_raw(); CNE(b, NULL); if (!b) return;
    CloseHandle(b);
    CONSOLE_FONT_INFO fi;
    FB(GetCurrentConsoleFont(b, FALSE, &fi), ERROR_INVALID_HANDLE);
    SetLastError(0xBAD);
    COORD c = GetConsoleFontSize(b, 0);
    CEQ(c.X, 0); CEQ(c.Y, 0); CERR(ERROR_INVALID_HANDLE);
}

/* ---------------- GetConsoleScreenBufferInfo(Ex) ---------------- */

CT(gap4_sbi_invariants) {
    CREQ(GetConsoleScreenBufferInfo);
    CONSOLE_SCREEN_BUFFER_INFO i; memset(&i, 0xCC, sizeof i);
    CCF(GetConsoleScreenBufferInfo(hout_(), &i), "err=%lu", GetLastError());
    CC(i.dwSize.X > 0 && i.dwSize.Y > 0);
    CC(i.srWindow.Left >= 0 && i.srWindow.Top >= 0);
    CC(i.srWindow.Right >= i.srWindow.Left && i.srWindow.Bottom >= i.srWindow.Top);
    CC(i.srWindow.Right < i.dwSize.X && i.srWindow.Bottom < i.dwSize.Y);
    CC(i.dwCursorPosition.X >= 0 && i.dwCursorPosition.X < i.dwSize.X);
    CC(i.dwCursorPosition.Y >= 0 && i.dwCursorPosition.Y < i.dwSize.Y);
    CC(i.dwMaximumWindowSize.X > 0 && i.dwMaximumWindowSize.Y > 0);
    CC((i.srWindow.Right - i.srWindow.Left + 1) <= i.dwMaximumWindowSize.X);
    CC((i.srWindow.Bottom - i.srWindow.Top + 1) <= i.dwMaximumWindowSize.Y);
}

CT(gap4_sbiex_matches_sbi) {
    CREQ(GetConsoleScreenBufferInfo); CREQ(GetConsoleScreenBufferInfoEx);
    HANDLE b = mkbuf_std(); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFO i; CONSOLE_SCREEN_BUFFER_INFOEX x;
    memset(&i, 0, sizeof i); memset(&x, 0xCC, sizeof x); x.cbSize = sizeof x;
    CC(GetConsoleScreenBufferInfo(b, &i));
    SetLastError(0xBAD);
    CCF(GetConsoleScreenBufferInfoEx(b, &x), "err=%lu", GetLastError());
    CEQ(x.cbSize, sizeof x);
    CEQ(x.dwSize.X, i.dwSize.X); CEQ(x.dwSize.Y, i.dwSize.Y);
    CEQ(x.dwCursorPosition.X, i.dwCursorPosition.X); CEQ(x.dwCursorPosition.Y, i.dwCursorPosition.Y);
    CEQ(x.wAttributes, i.wAttributes);
    CEQ(x.srWindow.Left, i.srWindow.Left); CEQ(x.srWindow.Right, i.srWindow.Right);
    CEQ(x.srWindow.Top, i.srWindow.Top); CEQ(x.srWindow.Bottom, i.srWindow.Bottom);
    CEQ(x.dwMaximumWindowSize.X, i.dwMaximumWindowSize.X); CEQ(x.dwMaximumWindowSize.Y, i.dwMaximumWindowSize.Y);
    CloseHandle(b);
}

CT(gap4_sbiex_cbsize_validation) {
    CREQ(GetConsoleScreenBufferInfoEx);
    CC(sizeof(CONSOLE_SCREEN_BUFFER_INFOEX) == 0x60);
    CONSOLE_SCREEN_BUFFER_INFOEX x;
    static const ULONG sizes[] = { 0, 1, 0x58, 0x5C, 0x5F, 0x61, 0x64, 0x68, 0x100, 0xFFFFFFFF };
    for (int i = 0; i < 10; i++) {
        memset(&x, 0xCC, sizeof x); x.cbSize = sizes[i];
        FB(GetConsoleScreenBufferInfoEx(hout_(), &x), ERROR_INVALID_PARAMETER);
        CEQ(x.cbSize, sizes[i]);
        CEQ(x.dwSize.X, (SHORT)0xCCCC);
    }
}

CT(gap4_sbiex_cbsize_checked_before_handle) {
    CREQ(GetConsoleScreenBufferInfoEx);
    CONSOLE_SCREEN_BUFFER_INFOEX x; memset(&x, 0, sizeof x);
    FB(GetConsoleScreenBufferInfoEx(INVALID_HANDLE_VALUE, &x), ERROR_INVALID_PARAMETER);
    FB(GetConsoleScreenBufferInfoEx(NULL, &x), ERROR_INVALID_PARAMETER);
    x.cbSize = sizeof x;
    FB(GetConsoleScreenBufferInfoEx(INVALID_HANDLE_VALUE, &x), ERROR_INVALID_HANDLE);
    FB(GetConsoleScreenBufferInfoEx(NULL, &x), ERROR_INVALID_HANDLE);
    FB(GetConsoleScreenBufferInfoEx((HANDLE)(ULONG_PTR)0x1234, &x), ERROR_INVALID_HANDLE);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    FB(GetConsoleScreenBufferInfoEx(ev, &x), n2d((LONG)0xC0000024));
    CloseHandle(ev);
}

CT(gap4_sbiex_color_table) {
    CREQ(GetConsoleScreenBufferInfoEx);
    CONSOLE_SCREEN_BUFFER_INFOEX x; memset(&x, 0xCC, sizeof x); x.cbSize = sizeof x;
    CCF(GetConsoleScreenBufferInfoEx(hout_(), &x), "err=%lu", GetLastError());
    int bad = 0;
    for (int i = 0; i < 16; i++) if (x.ColorTable[i] & 0xFF000000) bad++;
    CCF(bad == 0, "%d color entries with a nonzero top byte", bad);
    cnote("color table[0]=0x%06lx [7]=0x%06lx [15]=0x%06lx popup=0x%x fullscreen=%d", x.ColorTable[0], x.ColorTable[7], x.ColorTable[15], x.wPopupAttributes, x.bFullscreenSupported);
    CC(x.bFullscreenSupported == 0 || x.bFullscreenSupported == 1);
    if (x.ColorTable[0] == x.ColorTable[15]) cnote("color table entries 0 and 15 are equal");
}

CT(gap4_sbiex_attributes_follow_text_attribute) {
    CREQ(GetConsoleScreenBufferInfoEx); CREQ(SetConsoleTextAttribute);
    HANDLE b = mkbuf_std(); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFOEX x; memset(&x, 0, sizeof x); x.cbSize = sizeof x;
    static const WORD attrs[] = { 0x07, 0x1F, 0xF0, 0x4E, 0x0A };
    for (int i = 0; i < 5; i++) {
        CC(SetConsoleTextAttribute(b, attrs[i]));
        CC(GetConsoleScreenBufferInfoEx(b, &x));
        CEQ(x.wAttributes, attrs[i]);
    }
    CloseHandle(b);
}

CT(gap4_sbiex_color_table_set_get) {
    CREQ(GetConsoleScreenBufferInfoEx); CREQ(SetConsoleScreenBufferInfoEx);
    HANDLE b = mkbuf_std(); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFOEX x, y; memset(&x, 0, sizeof x); x.cbSize = sizeof x;
    CC(GetConsoleScreenBufferInfoEx(b, &x));
    COLORREF old = x.ColorTable[1];
    x.ColorTable[1] = RGB(0x12, 0x34, 0x56);
    /* the window rectangle read back is inclusive; setting needs it exclusive */
    x.srWindow.Right++; x.srWindow.Bottom++;
    SetLastError(0xBAD);
    BOOL r = SetConsoleScreenBufferInfoEx(b, &x);
    cnote("SetConsoleScreenBufferInfoEx -> %d err=%lu", r, GetLastError());
    if (r) {
        memset(&y, 0, sizeof y); y.cbSize = sizeof y;
        CC(GetConsoleScreenBufferInfoEx(b, &y));
        CEQ(y.ColorTable[1], RGB(0x12, 0x34, 0x56));
        CNE(y.ColorTable[1], old);
        CEQ(y.ColorTable[2], x.ColorTable[2]);
    }
    CloseHandle(b);
}

CT(gap4_sbi_new_buffer_cursor_at_origin) {
    CREQ(GetConsoleScreenBufferInfo); CREQ(GetConsoleScreenBufferInfoEx);
    HANDLE b = mkbuf_raw(); CNE(b, NULL); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFO i; CONSOLE_SCREEN_BUFFER_INFOEX x;
    memset(&x, 0, sizeof x); x.cbSize = sizeof x;
    CC(GetConsoleScreenBufferInfo(b, &i));
    CC(GetConsoleScreenBufferInfoEx(b, &x));
    CEQ(i.dwCursorPosition.X, 0); CEQ(i.dwCursorPosition.Y, 0);
    CEQ(i.srWindow.Left, 0); CEQ(i.srWindow.Top, 0);
    CEQ(x.dwCursorPosition.X, 0);
    CloseHandle(b);
}

CT(gap4_sbi_repeat_calls_equal) {
    CREQ(GetConsoleScreenBufferInfo);
    CONSOLE_SCREEN_BUFFER_INFO a, b;
    CC(GetConsoleScreenBufferInfo(hout_(), &a));
    for (int i = 0; i < 100; i++) {
        CC(GetConsoleScreenBufferInfo(hout_(), &b));
        if (memcmp(&a, &b, sizeof a)) { CCF(0, "changed at iteration %d", i); break; }
    }
}

CT(gap4_sbi_closed_and_input_handles) {
    CREQ(GetConsoleScreenBufferInfo); CREQ(GetConsoleScreenBufferInfoEx);
    HANDLE b = mkbuf_raw(); CNE(b, NULL); if (!b) return;
    CloseHandle(b);
    CONSOLE_SCREEN_BUFFER_INFO i; CONSOLE_SCREEN_BUFFER_INFOEX x; memset(&x, 0, sizeof x); x.cbSize = sizeof x;
    FB(GetConsoleScreenBufferInfo(b, &i), ERROR_INVALID_HANDLE);
    FB(GetConsoleScreenBufferInfoEx(b, &x), ERROR_INVALID_HANDLE);
    SetLastError(0xBAD);
    BOOL r = GetConsoleScreenBufferInfo(hin_(), &i);
    cnote("screen buffer info on the input handle -> %d err=%lu", r, GetLastError());
    CEQ(r, 0);
}

CT(gap4_sbi_all_std_handles_after_createbuffer) {
    CREQ(GetConsoleScreenBufferInfo);
    CONSOLE_SCREEN_BUFFER_INFO o, e;
    CC(GetConsoleScreenBufferInfo(hout_(), &o));
    CC(GetConsoleScreenBufferInfo(herr_(), &e));
    CEQ(o.dwSize.X, e.dwSize.X); CEQ(o.dwSize.Y, e.dwSize.Y);
}
