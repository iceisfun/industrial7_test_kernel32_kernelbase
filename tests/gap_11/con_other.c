/* gap_11 console: SetConsoleHistoryInfo, SetConsoleActiveScreenBuffer, SetConsoleDisplayMode,
 * SetConsoleNumberOfCommandsA, ScrollConsoleScreenBufferA/W, SetConsoleCtrlHandler. */
#include "con_h.h"

/* ---------------- SetConsoleHistoryInfo ---------------- */
#define SETHI U(SetConsoleHistoryInfo)

static CONSOLE_HISTORY_INFO hist(UINT size, UINT num, DWORD fl) {
    CONSOLE_HISTORY_INFO h; h.cbSize = sizeof h; h.HistoryBufferSize = size; h.NumberOfHistoryBuffers = num; h.dwFlags = fl;
    return h;
}
static int hist_is(UINT size, UINT num, DWORD fl) {
    CONSOLE_HISTORY_INFO g = hist(0xAAAA, 0xBBBB, 0xCCCC);
    if (!GetConsoleHistoryInfo(&g)) return 0;
    return g.cbSize == sizeof g && g.HistoryBufferSize == size && g.NumberOfHistoryBuffers == num && (g.dwFlags & 1) == fl;   /* only bit 0 is written by conhost; the rest of dwFlags is not initialised */
}

/* GetConsoleHistoryInfo only ORs bit 0 into a stack slot it never initialises (KERNELBASE passes the
 * uninitialised dwFlags through; conhost sets bit 0 but never clears it), so a clear flag cannot be
 * observed reliably after a call that left bit 0 set on the stack: compare size and count only. */
static int hist_sc_is(UINT size, UINT num) {
    CONSOLE_HISTORY_INFO g = hist(0xAAAA, 0xBBBB, 0xCCCC);
    if (!GetConsoleHistoryInfo(&g)) return 0;
    return g.HistoryBufferSize == size && g.NumberOfHistoryBuffers == num;
}

CT(gap11_con_history_struct_size) {
    CEQ(sizeof(CONSOLE_HISTORY_INFO), 16);
}
CT(gap11_con_history_roundtrip) {
    CREQ(SETHI);
    CONSOLE_HISTORY_INFO orig = hist(0, 0, 0); CC(GetConsoleHistoryInfo(&orig)); orig.dwFlags &= 1;
    CONSOLE_HISTORY_INFO cases[] = { hist(50, 4, 0), hist(25, 1, HISTORY_NO_DUP_FLAG), hist(0, 0, 0), hist(1, 1, 0),
                                     hist(999, 16, HISTORY_NO_DUP_FLAG), hist(0x7FFF, 0x7FFF, 0), hist(100, 0x7FFF, HISTORY_NO_DUP_FLAG) };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        SetLastError(0xBAD);
        CCF(SETHI(&cases[i]), "case %zu err=%lu", i, GetLastError());
        CCF(hist_is(cases[i].HistoryBufferSize, cases[i].NumberOfHistoryBuffers, cases[i].dwFlags), "case %zu readback", i);
    }
    CC(SETHI(&orig));
    CC(hist_is(orig.HistoryBufferSize, orig.NumberOfHistoryBuffers, orig.dwFlags));
}
CT(gap11_con_history_bad_cbsize) {
    CREQ(SETHI);
    CONSOLE_HISTORY_INFO orig = hist(0, 0, 0); CC(GetConsoleHistoryInfo(&orig)); orig.dwFlags &= 1;
    CONSOLE_HISTORY_INFO ok = hist(33, 3, 0); CC(SETHI(&ok));
    UINT sizes[] = { 0, 1, 4, 8, 12, 15, 17, 20, 24, 32, 0x100, 0x10000, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        CONSOLE_HISTORY_INFO h = hist(44, 4, HISTORY_NO_DUP_FLAG); h.cbSize = sizes[i];
        SetLastError(0xBAD);
        BOOL r = SETHI(&h);
        CCF(r == FALSE, "cbSize %u", sizes[i]);
        CCF(GetLastError() == ERROR_INVALID_PARAMETER, "cbSize %u err=%lu", sizes[i], GetLastError());
        CCF(hist_is(33, 3, 0), "unchanged after cbSize %u", sizes[i]);
    }
    SETHI(&orig);
}
CT(gap11_con_history_size_limits) {
    CREQ(SETHI);
    CONSOLE_HISTORY_INFO orig = hist(0, 0, 0); CC(GetConsoleHistoryInfo(&orig)); orig.dwFlags &= 1;
    CONSOLE_HISTORY_INFO ok = hist(33, 3, 0); CC(SETHI(&ok));
    CONSOLE_HISTORY_INFO h = hist(0x7FFF, 3, 0); CC(SETHI(&h));
    UINT bad[] = { 0x8000, 0x8001, 0xFFFF, 0x10000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SETHI(&ok);
        h = hist(bad[i], 3, 0);
        SetLastError(0xBAD);
        BOOL r = SETHI(&h);
        CCF(r == FALSE, "history size %u", bad[i]);
        CCF(GetLastError() == ERROR_INVALID_PARAMETER, "history size %u err=%lu", bad[i], GetLastError());
        CCF(hist_is(33, 3, 0), "unchanged after history size %u", bad[i]);
        h = hist(10, bad[i], 0);
        SetLastError(0xBAD);
        r = SETHI(&h);
        CCF(r == FALSE, "buffer count %u", bad[i]);
        CCF(GetLastError() == ERROR_INVALID_PARAMETER, "buffer count %u err=%lu", bad[i], GetLastError());
        CCF(hist_is(33, 3, 0), "unchanged after buffer count %u", bad[i]);
    }
    SETHI(&orig);
}
CT(gap11_con_history_flag_limits) {
    CREQ(SETHI);
    CONSOLE_HISTORY_INFO orig = hist(0, 0, 0); CC(GetConsoleHistoryInfo(&orig)); orig.dwFlags &= 1;
    CONSOLE_HISTORY_INFO ok = hist(33, 3, 0); CC(SETHI(&ok));
    DWORD bad[] = { 2, 3, 4, 0x10, 0x100, 0x80000000, 0xFFFFFFFE, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        CONSOLE_HISTORY_INFO h = hist(10, 2, bad[i]);
        SetLastError(0xBAD);
        BOOL r = SETHI(&h);
        CCF(r == FALSE, "flags 0x%lx", bad[i]);
        CCF(GetLastError() == ERROR_INVALID_PARAMETER, "flags 0x%lx err=%lu", bad[i], GetLastError());
        CCF(hist_sc_is(33, 3), "unchanged after flags 0x%lx", bad[i]);
    }
    SETHI(&orig);
}
CT(gap11_con_history_independent_fields) {
    CREQ(SETHI);
    CONSOLE_HISTORY_INFO orig = hist(0, 0, 0); CC(GetConsoleHistoryInfo(&orig)); orig.dwFlags &= 1;
    CONSOLE_HISTORY_INFO a = hist(10, 20, 0), b = hist(30, 20, HISTORY_NO_DUP_FLAG), c = hist(30, 40, HISTORY_NO_DUP_FLAG), d = hist(30, 40, 0);
    CC(SETHI(&a)); CC(hist_is(10, 20, 0));
    CC(SETHI(&b)); CC(hist_is(30, 20, 1));
    CC(SETHI(&c)); CC(hist_is(30, 40, 1));
    CC(SETHI(&d)); CC(hist_is(30, 40, 0));
    SETHI(&orig);
}
CT(gap11_con_history_repeat) {
    CREQ(SETHI);
    CONSOLE_HISTORY_INFO orig = hist(0, 0, 0); CC(GetConsoleHistoryInfo(&orig)); orig.dwFlags &= 1;
    for (UINT i = 0; i < 50; i++) { CONSOLE_HISTORY_INFO h = hist(i, i + 1, i & 1); CC(SETHI(&h)); }
    CC(hist_is(49, 50, 1));      /* last iteration i=49: i & 1 == 1 */
    SETHI(&orig);
}
CT(gap11_con_history_cbsize_checked_first) {
    CREQ(SETHI);
    CONSOLE_HISTORY_INFO h = hist(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF); h.cbSize = 0;
    FAILS(SETHI(&h), ERROR_INVALID_PARAMETER);
}
CTD(gap11_con_history_no_console) {
    CREQ(SETHI);
    CONSOLE_HISTORY_INFO h = hist(50, 4, 0);
    SetLastError(0xBAD);
    BOOL r = SETHI(&h);
    DWORD le = GetLastError();      /* cnote() resets the last error */
    CEQ(r, FALSE);
    cnote("SetConsoleHistoryInfo without console -> err=%lu", le);
    CCF(le == ERROR_INVALID_HANDLE, "err=%lu", le);
    h.cbSize = 5;
    FAILS(SETHI(&h), ERROR_INVALID_PARAMETER);      /* client-side size check precedes any console access */
}

/* ---------------- SetConsoleActiveScreenBuffer ---------------- */
#define SETACT U(SetConsoleActiveScreenBuffer)

CT(gap11_con_active_switch_and_back) {
    CREQ(SETACT);
    HANDLE orig = hout_(), n = mkbuf(); if (!n) return;
    SetLastError(0xBAD);
    CCF(SETACT(n), "err=%lu", GetLastError());
    CC(SETACT(orig));
    CC(SETACT(n));
    CC(SETACT(n));          /* activating the active buffer again is fine */
    CC(SETACT(orig));
    CloseHandle(n);
}
CT(gap11_con_active_buffer_content_preserved) {
    CREQ(SETACT);
    HANDLE orig = hout_(), n = mkbuf(), m = mkbuf(); if (!n || !m) return;
    clear_buf(n); clear_buf(m);
    g11_putw(n, 0, 0, L"private text");
    g11_putw(m, 0, 5, L"second text");
    CC(SETACT(n));
    CC(rowis(n, 0, 0, L"private text"));
    CC(rowis(m, 5, 0, L"second text"));
    CC(SETACT(m));
    CC(rowis(n, 0, 0, L"private text"));
    CC(SETACT(orig));
    CC(rowis(n, 0, 0, L"private text"));
    CC(rowis(m, 5, 0, L"second text"));
    CloseHandle(n); CloseHandle(m);
}
CT(gap11_con_active_write_to_inactive) {
    CREQ(SETACT); CREQ(U(WriteConsoleW));
    HANDLE orig = hout_(), n = mkbuf(); if (!n) return;
    DWORD w;
    SetConsoleMode(n, ENABLE_PROCESSED_OUTPUT);
    clear_buf(n);
    CC(SETACT(n));
    CC(U(WriteConsoleW)(orig, L"inactive", 8, &w, NULL));    /* writes to a buffer that is not displayed still work */
    CEQ(w, 8);
    CC(SETACT(orig));
    CloseHandle(n);
}
CT(gap11_con_active_close_active_buffer) {
    CREQ(SETACT); CREQ(U(WriteConsoleW));
    HANDLE orig = hout_(), n = mkbuf(); if (!n) return;
    CC(SETACT(n));
    CC(CloseHandle(n));
    DWORD w = 0;
    CC(U(WriteConsoleW)(orig, L"still works\r\n", 13, &w, NULL));
    CEQ(w, 13);
    CC(SETACT(orig));
}
CT(gap11_con_active_many_buffers) {
    CREQ(SETACT);
    HANDLE orig = hout_(), b[8];
    int k = 0;
    for (; k < 8; k++) { b[k] = mkbuf(); if (!b[k]) break; }
    for (int i = 0; i < k; i++) CCF(SETACT(b[i]), "buffer %d", i);
    for (int i = k - 1; i >= 0; i--) CCF(SETACT(b[i]), "buffer %d (back)", i);
    CC(SETACT(orig));
    for (int i = 0; i < k; i++) CloseHandle(b[i]);
}
CT(gap11_con_active_invalid_handles) {
    CREQ(SETACT);
    HANDLE closed = mkbuf(); if (closed) CloseHandle(closed);
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x1234, (HANDLE)(ULONG_PTR)0xFFFFFFFC, closed, hin_() };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        if (!bad[i] && i == 4) continue;
        SetLastError(0xBAD);
        BOOL r = SETACT(bad[i]);
        CCF(r == FALSE, "handle #%zu", i);
        CCF(GetLastError() == ERROR_INVALID_HANDLE, "handle #%zu err=%lu", i, GetLastError());
    }
}
CT(gap11_con_active_non_console_handle) {
    CREQ(SETACT);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SetLastError(0xBAD);
    BOOL r = SETACT(ev);
    CEQ(r, FALSE);
    cnote("SetConsoleActiveScreenBuffer(event) err=%lu", GetLastError());
    CNE(GetLastError(), 0xBAD);
    CloseHandle(ev);
}
CT(gap11_con_active_limited_access_handle) {
    CREQ(SETACT);
    HANDLE n = CreateConsoleScreenBuffer(GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    if (n == INVALID_HANDLE_VALUE) { cnote("write-only buffer not creatable"); return; }
    SetLastError(0xBAD);
    BOOL r = SETACT(n);
    cnote("SetConsoleActiveScreenBuffer(GENERIC_WRITE only handle) -> %d err=%lu", r, GetLastError());
    if (r) SETACT(hout_());
    CloseHandle(n);
}
CT(gap11_con_active_duplicate_handle) {
    CREQ(SETACT);
    HANDLE orig = hout_(), n = mkbuf(); if (!n) return;
    HANDLE d = NULL;
    CC(DuplicateHandle(GetCurrentProcess(), n, GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS));
    CC(SETACT(d));
    CloseHandle(n);              /* the duplicate still keeps the buffer alive */
    CC(SETACT(orig));
    CC(SETACT(d));
    CC(SETACT(orig));
    CloseHandle(d);
}
CTD(gap11_con_active_no_console) {
    CREQ(SETACT);
    FAILS(SETACT(GetStdHandle(STD_OUTPUT_HANDLE)), ERROR_INVALID_HANDLE);
    FAILS(SETACT(NULL), ERROR_INVALID_HANDLE);
    FAILS(SETACT(INVALID_HANDLE_VALUE), ERROR_INVALID_HANDLE);
}

/* ---------------- SetConsoleDisplayMode ---------------- */
#define SETDM U(SetConsoleDisplayMode)
/* In kernelbase on this build the function is a stub: it never reaches the console host. */
CT(gap11_con_displaymode_not_implemented) {
    CREQ(SETDM);
    HANDLE b = mkbuf();
    COORD nd = co(0, 0);
    DWORD modes[] = { 0, CONSOLE_FULLSCREEN_MODE, CONSOLE_WINDOWED_MODE, 3, 4, 0x80000000, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof modes / sizeof modes[0]; i++) {
        FAILS(SETDM(hout_(), modes[i], &nd), ERROR_CALL_NOT_IMPLEMENTED);
        FAILS(SETDM(hout_(), modes[i], NULL), ERROR_CALL_NOT_IMPLEMENTED);
        if (b) FAILS(SETDM(b, modes[i], &nd), ERROR_CALL_NOT_IMPLEMENTED);
    }
    if (b) CloseHandle(b);
}
CT(gap11_con_displaymode_handles_not_validated) {
    CREQ(SETDM);
    COORD nd = co(80, 25);
    HANDLE hs[] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x1234, hin_() };
    for (size_t i = 0; i < 4; i++) {
        SetLastError(0xBAD);
        BOOL r = SETDM(hs[i], CONSOLE_FULLSCREEN_MODE, &nd);
        CCF(r == FALSE, "handle #%zu", i);
        CCF(GetLastError() == ERROR_CALL_NOT_IMPLEMENTED, "handle #%zu err=%lu", i, GetLastError());
    }
}
CT(gap11_con_displaymode_does_not_touch_out_param) {
    CREQ(SETDM);
    COORD nd = co(123, 45);
    SETDM(hout_(), CONSOLE_FULLSCREEN_MODE, &nd);
    CEQ(nd.X, 123); CEQ(nd.Y, 45);
}
CT(gap11_con_displaymode_get_stays_windowed) {
    CREQ(SETDM);
    DWORD m = 0xFFFF;
    SETDM(hout_(), CONSOLE_FULLSCREEN_MODE, NULL);
    CC(GetConsoleDisplayMode(&m));
    CEQ(m, 0);
}
CTD(gap11_con_displaymode_no_console) {
    CREQ(SETDM);
    COORD nd = co(0, 0);
    FAILS(SETDM(GetStdHandle(STD_OUTPUT_HANDLE), CONSOLE_FULLSCREEN_MODE, &nd), ERROR_CALL_NOT_IMPLEMENTED);
    FAILS(SETDM(NULL, 0, NULL), ERROR_CALL_NOT_IMPLEMENTED);
}

/* ---------------- SetConsoleNumberOfCommands ---------------- */
#define SETNC_A U(SetConsoleNumberOfCommandsA)
CT(gap11_con_numcmds_values_observed) {
    CWINESKIP("emulator aborts in this unimplemented console call");
    CREQ(SETNC_A);
    char exe[] = "gap11_noexe.exe";
    DWORD nums[] = { 0, 1, 50, 500, 0x7FFF, 0x8000, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof nums / sizeof nums[0]; i++) {
        SetLastError(0xBAD);
        BOOL r = SETNC_A(nums[i], exe);
        cnote("SetConsoleNumberOfCommandsA(%lu, \"%s\") -> %d err=%lu", nums[i], exe, r, GetLastError());
        CCF(r == TRUE || r == FALSE, "BOOL result");
    }
}
CT(gap11_con_numcmds_w_values_observed) {
    CWINESKIP("emulator aborts in this unimplemented console call");
    CREQ(U(SetConsoleNumberOfCommandsW));
    WCHAR exe[] = L"gap11_noexe.exe";
    DWORD nums[] = { 0, 1, 50, 0x7FFF, 0x8000, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof nums / sizeof nums[0]; i++) {
        SetLastError(0xBAD);
        BOOL r = U(SetConsoleNumberOfCommandsW)(nums[i], exe);
        cnote("SetConsoleNumberOfCommandsW(%lu) -> %d err=%lu", nums[i], r, GetLastError());
    }
}
CT(gap11_con_numcmds_failure_sets_error) {
    CWINESKIP("emulator aborts in this unimplemented console call");
    CREQ(SETNC_A);
    char empty[] = "";
    SetLastError(0xBAD);
    BOOL r = SETNC_A(10, empty);
    cnote("SetConsoleNumberOfCommandsA(10, \"\") -> %d err=%lu", r, GetLastError());
    if (!r) CCF(GetLastError() != 0xBAD && GetLastError() != 0, "an error code is set on failure");
}
CT(gap11_con_numcmds_long_exe_name) {
    CWINESKIP("emulator aborts in this unimplemented console call");
    CREQ(SETNC_A);
    char exe[600]; memset(exe, 'e', 599); exe[599] = 0;
    SetLastError(0xBAD);
    BOOL r = SETNC_A(10, exe);
    cnote("SetConsoleNumberOfCommandsA(10, 599-char name) -> %d err=%lu", r, GetLastError());
    if (!r) CCF(GetLastError() != 0xBAD, "error set on failure");
}
CT(gap11_con_numcmds_history_length_follows) {
    CWINESKIP("emulator aborts in this unimplemented console call");
    CREQ(SETNC_A); CREQ(U(GetConsoleCommandHistoryLengthA)); CREQ(U(SetConsoleHistoryInfo));
    /* observation only: how the per-exe history size reacts */
    char exe[] = "gap11_hist.exe";
    BOOL r = SETNC_A(7, exe);
    DWORD len = U(GetConsoleCommandHistoryLengthA)(exe);
    cnote("NumberOfCommands(7, %s) -> %d; history length now %lu", exe, r, len);
}
CTD(gap11_con_numcmds_no_console) {
    CWINESKIP("emulator aborts in this unimplemented console call");
    CREQ(SETNC_A);
    char exe[] = "x.exe";
    SetLastError(0xBAD);
    BOOL r = SETNC_A(10, exe);
    CEQ(r, FALSE);
    cnote("SetConsoleNumberOfCommandsA without console err=%lu", GetLastError());
    CNE(GetLastError(), 0xBAD);
}

/* ---------------- ScrollConsoleScreenBuffer ---------------- */
#define SCRW U(ScrollConsoleScreenBufferW)
#define SCRA U(ScrollConsoleScreenBufferA)

static void labels(HANDLE b, int n) {
    for (int y = 0; y < n; y++) { WCHAR l[16]; wsprintfW(l, L"row%02d-abcdef", y); g11_putw(b, 0, y, l); }
}
static HANDLE sbuf(void) {
    HANDLE b = mkbuf(); if (!b) return NULL;
    SetConsoleMode(b, ENABLE_PROCESSED_OUTPUT);
    SetConsoleTextAttribute(b, 7);
    clear_buf(b);
    return b;
}

CT(gap11_con_scroll_w_up_one_row) {
    CREQ(SCRW);
    HANDLE b = sbuf(); if (!b) return;
    labels(b, 6);
    SMALL_RECT s = sr(0, 1, 9, 3); CHAR_INFO f = ciw(L'.', 0x1E);
    SetLastError(0xBAD);
    CCF(SCRW(b, &s, NULL, co(0, 0), &f), "err=%lu", GetLastError());
    CC(rowis(b, 0, 0, L"row01-abcd"));
    CC(rowis(b, 1, 0, L"row02-abcd"));
    CC(rowis(b, 2, 0, L"row03-abcd"));
    CC(rowis(b, 3, 0, L".........."));
    CC(rowis(b, 3, 10, L"ef"));        /* outside the source rectangle: untouched */
    CC(rowis(b, 4, 0, L"row04-abcd"));
    CC(rowis(b, 5, 0, L"row05-abcd"));
    CEQ(cellat(b, 0, 3), 0x1E); CEQ(cellat(b, 9, 3), 0x1E);
    CEQ(cellat(b, 10, 3), 7);
    CloseHandle(b);
}
CT(gap11_con_scroll_w_down_one_row) {
    CREQ(SCRW);
    HANDLE b = sbuf(); if (!b) return;
    labels(b, 6);
    SMALL_RECT s = sr(0, 1, 9, 3); CHAR_INFO f = ciw(L'#', 0x2F);
    CC(SCRW(b, &s, NULL, co(0, 2), &f));
    CC(rowis(b, 0, 0, L"row00-abcd"));
    CC(rowis(b, 1, 0, L"##########"));
    CC(rowis(b, 2, 0, L"row01-abcd"));
    CC(rowis(b, 3, 0, L"row02-abcd"));
    CC(rowis(b, 4, 0, L"row03-abcd"));
    CC(rowis(b, 5, 0, L"row05-abcd"));
    CEQ(cellat(b, 0, 1), 0x2F);
    CloseHandle(b);
}
CT(gap11_con_scroll_w_horizontal) {
    CREQ(SCRW);
    HANDLE b = sbuf(); if (!b) return;
    g11_putw(b, 0, 0, L"0123456789");
    SMALL_RECT s = sr(0, 0, 4, 0); CHAR_INFO f = ciw(L'.', 7);
    CC(SCRW(b, &s, NULL, co(3, 0), &f));
    CC(rowis(b, 0, 0, L"...0123489"));     /* overlapping move: no smearing, 0..2 refilled */
    g11_putw(b, 0, 1, L"0123456789");
    s = sr(5, 1, 9, 1);
    CC(SCRW(b, &s, NULL, co(2, 1), &f));
    CC(rowis(b, 1, 0, L"0156789..."));
    CloseHandle(b);
}
CT(gap11_con_scroll_w_dest_equals_source) {
    CREQ(SCRW);
    HANDLE b = sbuf(); if (!b) return;
    labels(b, 4);
    SMALL_RECT s = sr(0, 0, 9, 3); CHAR_INFO f = ciw(L'!', 0x4E);
    CC(SCRW(b, &s, NULL, co(0, 0), &f));
    CC(rowis(b, 0, 0, L"row00-abcd"));
    CC(rowis(b, 3, 0, L"row03-abcd"));
    CEQ(cellat(b, 0, 0), 7);
    CloseHandle(b);
}
CT(gap11_con_scroll_w_clip_rect) {
    CREQ(SCRW);
    HANDLE b = sbuf(); if (!b) return;
    labels(b, 7);
    SMALL_RECT s = sr(0, 0, 9, 4), clip = sr(0, 0, 9, 3); CHAR_INFO f = ciw(L'~', 0x1C);
    CC(SCRW(b, &s, &clip, co(0, 2), &f));
    CC(rowis(b, 0, 0, L"~~~~~~~~~~"));
    CC(rowis(b, 1, 0, L"~~~~~~~~~~"));
    CC(rowis(b, 2, 0, L"row00-abcd"));
    CC(rowis(b, 3, 0, L"row01-abcd"));
    CC(rowis(b, 4, 0, L"row04-abcd"));     /* outside the clip rectangle */
    CC(rowis(b, 5, 0, L"row05-abcd"));
    CloseHandle(b);
}
CT(gap11_con_scroll_w_clip_disjoint) {
    CREQ(SCRW);
    HANDLE b = sbuf(); if (!b) return;
    labels(b, 6);
    SMALL_RECT s = sr(0, 0, 9, 4), clip = sr(40, 15, 50, 18); CHAR_INFO f = ciw(L'~', 0x1C);
    SetLastError(0xBAD);
    CCF(SCRW(b, &s, &clip, co(0, 1), &f), "err=%lu", GetLastError());
    for (int y = 0; y < 6; y++) { WCHAR l[16]; wsprintfW(l, L"row%02d-abcd", y); CCF(rowis(b, y, 0, l), "row %d unchanged", y); }
    CloseHandle(b);
}
CT(gap11_con_scroll_w_clip_narrow) {
    CREQ(SCRW);
    HANDLE b = sbuf(); if (!b) return;
    g11_putw(b, 0, 0, L"ABCDEFGHIJ");
    SMALL_RECT s = sr(0, 0, 9, 0), clip = sr(0, 0, 5, 0); CHAR_INFO f = ciw(L'-', 7);
    CC(SCRW(b, &s, &clip, co(2, 0), &f));
    CC(rowis(b, 0, 0, L"--ABCDGHIJ"));      /* only columns 0..5 may change */
    CloseHandle(b);
}
CT(gap11_con_scroll_w_attributes_move) {
    CREQ(SCRW);
    HANDLE b = sbuf(); if (!b) return;
    DWORD n;
    g11_putw(b, 0, 0, L"AAAA"); g11_putw(b, 0, 1, L"BBBB");
    FillConsoleOutputAttribute(b, 0x12, 4, co(0, 0), &n);
    FillConsoleOutputAttribute(b, 0x34, 4, co(0, 1), &n);
    SMALL_RECT s = sr(0, 0, 3, 1); CHAR_INFO f = ciw(L' ', 0x56);
    CC(SCRW(b, &s, NULL, co(0, 1), &f));
    CEQ(cellch(b, 0, 0), L' '); CEQ(cellat(b, 0, 0), 0x56);
    CEQ(cellch(b, 0, 1), L'A'); CEQ(cellat(b, 0, 1), 0x12);
    CEQ(cellch(b, 3, 2), L'B'); CEQ(cellat(b, 3, 2), 0x34);
    CloseHandle(b);
}
CT(gap11_con_scroll_w_single_cell) {
    CREQ(SCRW);
    HANDLE b = sbuf(); if (!b) return;
    g11_putw(b, 0, 0, L"Q");
    SMALL_RECT s = sr(0, 0, 0, 0); CHAR_INFO f = ciw(L'f', 0x0A);
    CC(SCRW(b, &s, NULL, co(7, 4), &f));
    CEQ(cellch(b, 7, 4), L'Q'); CEQ(cellch(b, 0, 0), L'f'); CEQ(cellat(b, 0, 0), 0x0A);
    CloseHandle(b);
}
CT(gap11_con_scroll_w_does_not_move_cursor) {
    CREQ(SCRW);
    HANDLE b = sbuf(); if (!b) return;
    labels(b, 4);
    SetConsoleCursorPosition(b, co(12, 3));
    SMALL_RECT s = sr(0, 0, 9, 3); CHAR_INFO f = ciw(L' ', 7);
    CC(SCRW(b, &s, NULL, co(0, 1), &f));
    CONSOLE_SCREEN_BUFFER_INFO g = binfo(b);
    CEQ(g.dwCursorPosition.X, 12); CEQ(g.dwCursorPosition.Y, 3);
    CloseHandle(b);
}
CT(gap11_con_scroll_w_dest_clipped_to_buffer) {
    CREQ(SCRW);
    HANDLE b = sbuf(); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFO i = binfo(b); int W = i.dwSize.X;
    g11_putw(b, 0, 0, L"WXYZ");
    SMALL_RECT s = sr(0, 0, 3, 0); CHAR_INFO f = ciw(L'.', 7);
    SetLastError(0xBAD);
    BOOL r = SCRW(b, &s, NULL, co(W - 2, 0), &f);
    cnote("scroll with destination hanging off the right edge -> %d err=%lu", r, GetLastError());
    if (r) { CEQ(cellch(b, W - 2, 0), L'W'); CEQ(cellch(b, W - 1, 0), L'X'); CEQ(cellch(b, 0, 0), L'.'); }
    CloseHandle(b);
}
CT(gap11_con_scroll_w_odd_rectangles_observed) {
    CREQ(SCRW);
    HANDLE b = sbuf(); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFO i = binfo(b); int W = i.dwSize.X, H = i.dwSize.Y;
    labels(b, 3);
    CHAR_INFO f = ciw(L'!', 7);
    SMALL_RECT rects[] = { sr(9, 3, 0, 0), sr(100, 100, 110, 110), sr(-5, -5, 3, 3), sr(0, 0, W + 10, 2), sr(W, 0, W + 4, 2), sr(0, H, 5, H + 3), sr(2, 2, 1, 1) };
    for (size_t k = 0; k < sizeof rects / sizeof rects[0]; k++) {
        SetLastError(0xBAD);
        BOOL r = SCRW(b, &rects[k], NULL, co(0, 5), &f);
        cnote("scroll rect #%zu (%d,%d)-(%d,%d) -> %d err=%lu", k, rects[k].Left, rects[k].Top, rects[k].Right, rects[k].Bottom, r, GetLastError());
    }
    CloseHandle(b);
}
CT(gap11_con_scroll_w_repeated_rotation) {
    CREQ(SCRW);
    HANDLE b = sbuf(); if (!b) return;
    labels(b, 5);
    SMALL_RECT s = sr(0, 0, 9, 4); CHAR_INFO f = ciw(L' ', 7);
    for (int k = 0; k < 5; k++) CC(SCRW(b, &s, NULL, co(0, 0), &f));      /* no-op moves */
    CC(rowis(b, 4, 0, L"row04-abcd"));
    s = sr(0, 1, 9, 4);
    for (int k = 0; k < 4; k++) CC(SCRW(b, &s, NULL, co(0, 0), &f));      /* scroll rows 1..4 up four times */
    CC(rowis(b, 0, 0, L"row04-abcd"));
    CC(rowis(b, 1, 0, L"          "));
    CC(rowis(b, 4, 0, L"          "));
    CloseHandle(b);
}
CT(gap11_con_scroll_a_basic) {
    CREQ(SCRA);
    HANDLE b = sbuf(); if (!b) return;
    labels(b, 5);
    SMALL_RECT s = sr(0, 1, 9, 3); CHAR_INFO f = cia('.', 0x1E);
    SetLastError(0xBAD);
    CCF(SCRA(b, &s, NULL, co(0, 0), &f), "err=%lu", GetLastError());
    CC(rowis(b, 0, 0, L"row01-abcd"));
    CC(rowis(b, 2, 0, L"row03-abcd"));
    CC(rowis(b, 3, 0, L".........."));
    CEQ(cellat(b, 0, 3), 0x1E);
    CC(rowis(b, 4, 0, L"row04-abcd"));
    CloseHandle(b);
}
CT(gap11_con_scroll_a_clip) {
    CREQ(SCRA);
    HANDLE b = sbuf(); if (!b) return;
    labels(b, 6);
    SMALL_RECT s = sr(0, 0, 9, 4), clip = sr(0, 0, 9, 3); CHAR_INFO f = cia('~', 0x1C);
    CC(SCRA(b, &s, &clip, co(0, 2), &f));
    CC(rowis(b, 0, 0, L"~~~~~~~~~~"));
    CC(rowis(b, 2, 0, L"row00-abcd"));
    CC(rowis(b, 4, 0, L"row04-abcd"));
    CloseHandle(b);
}
CT(gap11_con_scroll_a_fill_uses_output_cp) {
    CREQ(SCRA); CREQ(U(SetConsoleOutputCP));
    HANDLE b = sbuf(); if (!b) return;
    UINT oo = GetConsoleOutputCP();
    SMALL_RECT s = sr(0, 0, 3, 0);
    CC(U(SetConsoleOutputCP)(437));
    CHAR_INFO f = cia((char)0x82, 7);
    CC(SCRA(b, &s, NULL, co(0, 1), &f));
    CEQ(cellch(b, 0, 0), 0x00E9);
    CC(U(SetConsoleOutputCP)(1251));
    f = cia((char)0xE0, 7);
    CC(SCRA(b, &s, NULL, co(0, 1), &f));
    CEQ(cellch(b, 0, 0), 0x0430);
    CC(U(SetConsoleOutputCP)(1252));
    f = cia((char)0xE9, 7);
    CC(SCRA(b, &s, NULL, co(0, 1), &f));
    CEQ(cellch(b, 0, 0), 0x00E9);
    CC(U(SetConsoleOutputCP)(65001));
    f = cia('Z', 7);
    CC(SCRA(b, &s, NULL, co(0, 1), &f));
    CEQ(cellch(b, 0, 0), L'Z');
    U(SetConsoleOutputCP)(oo);
    CloseHandle(b);
}
CT(gap11_con_scroll_w_unicode_fill) {
    CREQ(SCRW);
    HANDLE b = sbuf(); if (!b) return;
    SMALL_RECT s = sr(0, 0, 2, 0); CHAR_INFO f = ciw(0x263A, 0x0F);
    CC(SCRW(b, &s, NULL, co(0, 1), &f));
    CEQ(cellch(b, 0, 0), 0x263A); CEQ(cellch(b, 2, 0), 0x263A); CEQ(cellat(b, 1, 0), 0x0F);
    f = ciw(0x20AC, 0x07);
    CC(SCRW(b, &s, NULL, co(0, 1), &f));
    CEQ(cellch(b, 1, 0), 0x20AC);
    CloseHandle(b);
}
CT(gap11_con_scroll_aw_parity) {
    CREQ(SCRW); CREQ(SCRA);
    HANDLE b = sbuf(); if (!b) return;
    labels(b, 4);
    SMALL_RECT s = sr(0, 0, 9, 2); CHAR_INFO fw = ciw(L'x', 0x1A), fa = cia('x', 0x1A);
    CC(SCRW(b, &s, NULL, co(2, 1), &fw));
    WCHAR r1[12], r2[12]; DWORD n;
    ReadConsoleOutputCharacterW(b, r1, 12, co(0, 0), &n); r1[n] = 0;
    labels(b, 4); clear_buf(b); labels(b, 4);
    CC(SCRA(b, &s, NULL, co(2, 1), &fa));
    ReadConsoleOutputCharacterW(b, r2, 12, co(0, 0), &n); r2[n] = 0;
    CC(!lstrcmpW(r1, r2));
    CloseHandle(b);
}
CT(gap11_con_scroll_invalid_handles) {
    CREQ(SCRW); CREQ(SCRA);
    SMALL_RECT s = sr(0, 0, 3, 3); CHAR_INFO fw = ciw(L' ', 7), fa = cia(' ', 7);
    HANDLE closed = mkbuf(); if (closed) CloseHandle(closed);
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x1234, closed, hin_() };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        if (!bad[i] && i == 3) continue;
        FAILS(SCRW(bad[i], &s, NULL, co(0, 1), &fw), ERROR_INVALID_HANDLE);
        FAILS(SCRA(bad[i], &s, NULL, co(0, 1), &fa), ERROR_INVALID_HANDLE);
    }
}
CT(gap11_con_scroll_handle_checked_before_rect) {
    CREQ(SCRW);
    SMALL_RECT s = sr(9, 9, 0, 0); CHAR_INFO f = ciw(L' ', 7);
    FAILS(SCRW(INVALID_HANDLE_VALUE, &s, NULL, co(-5, -5), &f), ERROR_INVALID_HANDLE);
}
CT(gap11_con_scroll_on_active_main_buffer) {
    CREQ(SCRW);
    HANDLE ho = hout_();
    SMALL_RECT s = sr(0, 0, 3, 0); CHAR_INFO f = ciw(L' ', 7);
    SetLastError(0xBAD);
    CCF(SCRW(ho, &s, NULL, co(0, 0), &f), "err=%lu", GetLastError());       /* no-op scroll of the main buffer */
}
CTD(gap11_con_scroll_no_console) {
    CREQ(SCRW); CREQ(SCRA);
    SMALL_RECT s = sr(0, 0, 3, 3); CHAR_INFO fw = ciw(L' ', 7), fa = cia(' ', 7);
    FAILS(SCRW(GetStdHandle(STD_OUTPUT_HANDLE), &s, NULL, co(0, 1), &fw), ERROR_INVALID_HANDLE);
    FAILS(SCRA(GetStdHandle(STD_OUTPUT_HANDLE), &s, NULL, co(0, 1), &fa), ERROR_INVALID_HANDLE);
    FAILS(SCRW(NULL, &s, NULL, co(0, 1), &fw), ERROR_INVALID_HANDLE);
}

/* ---------------- SetConsoleCtrlHandler ---------------- */
#define SETCH U(SetConsoleCtrlHandler)
static BOOL WINAPI h_nop1(DWORD t) { return FALSE; }
static BOOL WINAPI h_nop2(DWORD t) { return FALSE; }
static BOOL WINAPI h_nop3(DWORD t) { return TRUE; }
static ULONG peb_console_flags(void) {
    BYTE *params = *(BYTE **)((BYTE *)NtCurrentTeb()->ProcessEnvironmentBlock + 0x20);   /* PEB.ProcessParameters */
    return *(ULONG *)(params + 0x18);                                                     /* ConsoleFlags */
}

CT(gap11_con_ctrl_add_remove) {
    CREQ(SETCH);
    SetLastError(0xBAD);
    CCF(SETCH(h_nop1, TRUE), "add err=%lu", GetLastError());
    CCF(SETCH(h_nop1, FALSE), "remove err=%lu", GetLastError());
}
CT(gap11_con_ctrl_remove_not_registered) {
    CREQ(SETCH);
    SetLastError(0xBAD);
    BOOL r = SETCH(h_nop1, FALSE);
    CEQ(r, FALSE);
    cnote("remove of an unregistered handler: err=%lu", GetLastError());
    CNE(GetLastError(), 0xBAD);
    CC(SETCH(h_nop1, TRUE));
    CC(SETCH(h_nop1, FALSE));
    SetLastError(0xBAD);
    CEQ(SETCH(h_nop1, FALSE), FALSE);        /* already removed */
    CC(SETCH(h_nop1, TRUE));
    SetLastError(0xBAD);
    CEQ(SETCH(h_nop2, FALSE), FALSE);   /* list non-empty, but another routine */
    cnote("remove of a different routine: err=%lu", GetLastError());
    CC(SETCH(h_nop1, FALSE));
}
CT(gap11_con_ctrl_duplicates_counted) {
    CREQ(SETCH);
    CC(SETCH(h_nop1, TRUE)); CC(SETCH(h_nop1, TRUE)); CC(SETCH(h_nop1, TRUE));
    CC(SETCH(h_nop1, FALSE)); CC(SETCH(h_nop1, FALSE)); CC(SETCH(h_nop1, FALSE));
    SetLastError(0xBAD);
    CEQ(SETCH(h_nop1, FALSE), FALSE);        /* three adds need three removes */
}
CT(gap11_con_ctrl_many_handlers) {
    CREQ(SETCH);
    for (int i = 0; i < 300; i++) CCF(SETCH(h_nop1, TRUE), "add %d", i);
    CC(SETCH(h_nop2, TRUE)); CC(SETCH(h_nop3, TRUE));
    for (int i = 0; i < 300; i++) CCF(SETCH(h_nop1, FALSE), "remove %d", i);
    CEQ(SETCH(h_nop1, FALSE), FALSE);
    CC(SETCH(h_nop3, FALSE)); CC(SETCH(h_nop2, FALSE));
    CEQ(SETCH(h_nop2, FALSE), FALSE);
}
CT(gap11_con_ctrl_distinct_handlers_independent) {
    CREQ(SETCH);
    CC(SETCH(h_nop1, TRUE)); CC(SETCH(h_nop2, TRUE));
    CC(SETCH(h_nop1, FALSE));
    CEQ(SETCH(h_nop1, FALSE), FALSE);
    CC(SETCH(h_nop2, FALSE));
}
CT(gap11_con_ctrl_null_ignores_ctrl_c) {
    CREQ(SETCH);
    SetLastError(0xBAD);
    CCF(SETCH(NULL, TRUE), "err=%lu", GetLastError());
    CEQ(peb_console_flags() & 1, 1);
    CC(SETCH(NULL, TRUE));                       /* idempotent */
    CEQ(peb_console_flags() & 1, 1);
    CC(SETCH(NULL, FALSE));
    CEQ(peb_console_flags() & 1, 0);
    CC(SETCH(NULL, FALSE));
    CEQ(peb_console_flags() & 1, 0);
}
CT(gap11_con_ctrl_null_does_not_use_handler_list) {
    CREQ(SETCH);
    CC(SETCH(NULL, TRUE));
    SetLastError(0xBAD);
    CEQ(SETCH(h_nop1, FALSE), FALSE);    /* the NULL entry was a flag, not a list item */
    CC(SETCH(h_nop1, TRUE));
    CC(SETCH(NULL, FALSE));
    CEQ(peb_console_flags() & 1, 0);
    CC(SETCH(h_nop1, FALSE));
}
CT(gap11_con_ctrl_add_does_not_touch_ignore_flag) {
    CREQ(SETCH);
    ULONG before = peb_console_flags();
    CC(SETCH(h_nop1, TRUE));
    CEQ(peb_console_flags(), before);
    CC(SETCH(h_nop1, FALSE));
    CEQ(peb_console_flags(), before);
}
CT(gap11_con_ctrl_other_console_flags_preserved) {
    CREQ(SETCH);
    ULONG before = peb_console_flags();
    CC(SETCH(NULL, TRUE));
    CEQ(peb_console_flags() & ~1u, before & ~1u);
    CC(SETCH(NULL, FALSE));
    CEQ(peb_console_flags() & ~1u, before & ~1u);
}
static DWORD WINAPI ctrl_th(LPVOID p) { *(BOOL *)p = U(SetConsoleCtrlHandler)(h_nop1, FALSE); return 0; }
CT(gap11_con_ctrl_thread_independent) {
    CREQ(SETCH);
    /* the handler list is process wide: a handler added on one thread can be removed from another */
    CC(SETCH(h_nop1, TRUE));
    BOOL res = FALSE;
    HANDLE t = CreateThread(NULL, 0, ctrl_th, &res, 0, NULL);
    WaitForSingleObject(t, 5000); CloseHandle(t);
    CEQ(res, TRUE);
    CEQ(SETCH(h_nop1, FALSE), FALSE);
}
CTD(gap11_con_ctrl_works_without_console) {
    CREQ(SETCH);
    CC(SETCH(h_nop1, TRUE));
    CC(SETCH(h_nop1, FALSE));
    CC(SETCH(NULL, TRUE));
    CC(SETCH(NULL, FALSE));
    CEQ(SETCH(h_nop1, FALSE), FALSE);
}

/* Delivery of a control event: a grandchild is created in its own process group on this console and receives
 * CTRL_BREAK_EVENT addressed to that group only (never group 0). Exit code encodes what the handlers saw. */
static volatile LONG gc_seq_len;
static char gc_seq[16];
static HANDLE gc_done;
static void gc_rec(char c) { LONG i = InterlockedIncrement(&gc_seq_len) - 1; if (i < 15) gc_seq[i] = c; }
static BOOL WINAPI gc_single(DWORD t) { gc_rec((char)('0' + t)); SetEvent(gc_done); return TRUE; }
static BOOL WINAPI gc_first_added(DWORD t) { gc_rec('A'); SetEvent(gc_done); return TRUE; }
static BOOL WINAPI gc_second_added(DWORD t) { gc_rec('B'); return FALSE; }
static BOOL WINAPI gc_third_added_true(DWORD t) { gc_rec('C'); SetEvent(gc_done); return TRUE; }

CHILD(gap11_ctrlgc) {
    int mode = argc > 1 ? atoi(argv[1]) : 0;
    char nm[64];
    snprintf(nm, sizeof nm, "%s", argc > 2 ? argv[2] : "x");
    HANDLE ready = OpenEventA(EVENT_MODIFY_STATE, FALSE, nm);
    gc_done = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!ready || !gc_done) return 90;
    BOOL (WINAPI *set)(PHANDLER_ROUTINE, BOOL) = (void *)t_proc("SetConsoleCtrlHandler");
    if (!set) return 91;
    switch (mode) {
    case 1: set(gc_single, TRUE); break;
    case 2: set(gc_first_added, TRUE); set(gc_second_added, TRUE); break;
    case 3: set(gc_single, TRUE); set(gc_single, FALSE); break;
    case 4: set(gc_first_added, TRUE); set(gc_second_added, TRUE); set(gc_third_added_true, TRUE); break;
    case 5: set(gc_single, TRUE); set(gc_second_added, TRUE); set(gc_second_added, FALSE); break;
    }
    SetEvent(ready);
    if (WaitForSingleObject(gc_done, 12000) != WAIT_OBJECT_0) return 77;
    Sleep(200);
    gc_seq[15] = 0;
    if (mode == 1) return gc_seq_len == 1 && gc_seq[0] == '1' ? 100 : 101;       /* CTRL_BREAK_EVENT == 1 */
    if (mode == 2) return !strcmp(gc_seq, "BA") ? 200 : 201;
    if (mode == 4) return !strcmp(gc_seq, "C") ? 400 : 401;
    if (mode == 5) return (gc_seq[0] == '1' && gc_seq_len == 1) ? 500 : 501;
    return 99;
}
static DWORD run_ctrl_gc(int mode) {
    char evn[64], args[128];
    snprintf(evn, sizeof evn, "g11_ctrl_rdy_%lu_%d", GetCurrentProcessId(), mode);
    HANDLE ready = CreateEventA(NULL, TRUE, FALSE, evn);
    snprintf(args, sizeof args, "%d %s", mode, evn);
    PROCESS_INFORMATION pi;
    HANDLE h = t_spawn_child("gap11_ctrlgc", args, CREATE_NEW_PROCESS_GROUP, NULL, NULL, NULL, FALSE, NULL, &pi);
    if (!h) { CloseHandle(ready); return 0xDEAD0000; }
    DWORD code = 0xDEAD0001;
    if (WaitForSingleObject(ready, 10000) == WAIT_OBJECT_0) {
        Sleep(100);
        if (!GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pi.dwProcessId)) cnote("GenerateConsoleCtrlEvent failed: %lu", GetLastError());
    }
    if (WaitForSingleObject(h, 15000) != WAIT_OBJECT_0) { TerminateProcess(h, 1); WaitForSingleObject(h, 3000); code = 0xDEAD0001; }
    else GetExitCodeProcess(h, &code);
    CloseHandle(pi.hThread); CloseHandle(h); CloseHandle(ready);
    return code;
}
CT(gap11_con_ctrl_event_reaches_handler) {
    CREQ(SETCH); CWINESKIP("console ctrl events are not delivered by the emulator's headless console");
    CEQ(run_ctrl_gc(1), 100);
}
CT(gap11_con_ctrl_handlers_run_newest_first) {
    CREQ(SETCH); CWINESKIP("console ctrl events are not delivered by the emulator's headless console");
    /* A registered first, B second; B (returns FALSE) runs before A (returns TRUE, ends the chain) */
    CEQ(run_ctrl_gc(2), 200);
}
CT(gap11_con_ctrl_true_stops_chain) {
    CREQ(SETCH); CWINESKIP("console ctrl events are not delivered by the emulator's headless console");
    /* C registered last returns TRUE: it alone runs */
    CEQ(run_ctrl_gc(4), 400);
}
CT(gap11_con_ctrl_removed_handler_default_terminates) {
    CREQ(SETCH); CWINESKIP("console ctrl events are not delivered by the emulator's headless console");
    /* with the handler removed the default routine runs and ends the process with STATUS_CONTROL_C_EXIT */
    CEQ(run_ctrl_gc(3), 0xC000013A);
}
CT(gap11_con_ctrl_removed_second_handler_not_called) {
    CREQ(SETCH); CWINESKIP("console ctrl events are not delivered by the emulator's headless console");
    CEQ(run_ctrl_gc(5), 500);
}
