/* console_apis: standard handles, console modes, code pages, titles, window, process list */
#include "ch.h"

#define FAILS(call, err)  do { SetLastError(0xBAD); BOOL r_ = (call); DWORD le_ = GetLastError(); CEQ(r_, 0); CERRV(le_, err); } while (0)

UDECL(BOOL, SetStdHandleEx, (DWORD, HANDLE, PHANDLE));
UDECL(BOOL, GetConsoleKeyboardLayoutNameA, (LPSTR));
UDECL(BOOL, GetConsoleKeyboardLayoutNameW, (LPWSTR));

static HANDLE mk_event_handle(void) { return CreateEventA(NULL, TRUE, FALSE, NULL); }

/* ---------------- GetStdHandle / SetStdHandle ---------------- */

CT(con_std_handles_valid) {
    CREQ(GetStdHandle);
    HANDLE h[3]; h[0] = hin_(); h[1] = hout_(); h[2] = herr_();
    for (int i = 0; i < 3; i++) {
        CNE(h[i], NULL);
        CNE(h[i], INVALID_HANDLE_VALUE);
        CEQ(GetFileType(h[i]), FILE_TYPE_CHAR);
    }
    CC(h[0] != h[1]);
}

CT(con_std_handles_repeatable) {
    CREQ(GetStdHandle);
    for (int i = 0; i < 50; i++) {
        CEQ(GetStdHandle(STD_INPUT_HANDLE), hin_());
        CEQ(GetStdHandle(STD_OUTPUT_HANDLE), hout_());
    }
}

CT(con_std_handles_usable_modes) {
    CREQ(GetConsoleMode);
    DWORD m = 0;
    CC(GetConsoleMode(hin_(), &m));
    CC(GetConsoleMode(hout_(), &m));
    CC(GetConsoleMode(herr_(), &m));
}

CT(con_std_getstdhandle_invalid_ids) {
    CREQ(GetStdHandle);
    DWORD ids[] = { 0, 1, 2, 3, (DWORD)-9, (DWORD)-13, (DWORD)-1, (DWORD)-14, 0x80000000, 0x7FFFFFFF, 0xFFFFFFF0, 0xFFFFFFF3, 0xFFFFFFF7, 0x1000 };
    for (size_t i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        SetLastError(0xBAD);
        HANDLE h = GetStdHandle(ids[i]);
        CCF(h == INVALID_HANDLE_VALUE, "id=0x%lx got %p", ids[i], h);
        CERR(ERROR_INVALID_HANDLE);
    }
}

CT(con_std_setstdhandle_invalid_ids) {
    CREQ(SetStdHandle);
    HANDLE oh = hout_();
    DWORD ids[] = { 0, 1, 2, 3, (DWORD)-9, (DWORD)-13, (DWORD)-1, 0x80000000, 0xFFFFFFF0, 0xFFFFFFF3 };
    for (size_t i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        SetLastError(0xBAD);
        BOOL r = SetStdHandle(ids[i], oh);
        CCF(r == FALSE, "id=0x%lx", ids[i]);
        CERR(ERROR_INVALID_HANDLE);
    }
    CEQ(hout_(), oh);
}

CT(con_std_setstdhandle_roundtrip_values) {
    CREQ(SetStdHandle);
    DWORD ids[3] = { STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE };
    HANDLE vals[4] = { NULL, (HANDLE)(ULONG_PTR)0x1234, mk_event_handle(), hout_() };
    for (int k = 0; k < 3; k++) {
        HANDLE save = GetStdHandle(ids[k]);
        for (int j = 0; j < 4; j++) {
            SetLastError(0xBAD);
            CC(SetStdHandle(ids[k], vals[j]));
            CEQ(GetStdHandle(ids[k]), vals[j]);
        }
        /* INVALID_HANDLE_VALUE is accepted too, and read back with the invalid-handle error */
        CC(SetStdHandle(ids[k], INVALID_HANDLE_VALUE));
        SetLastError(0xBAD);
        CEQ(GetStdHandle(ids[k]), INVALID_HANDLE_VALUE);
        CERR(ERROR_INVALID_HANDLE);
        CC(SetStdHandle(ids[k], save));
        CEQ(GetStdHandle(ids[k]), save);
    }
    CloseHandle(vals[2]);
}

CT(con_std_setstdhandle_independent) {
    CREQ(SetStdHandle);
    HANDLE a = hin_(), b = hout_(), c = herr_();
    HANDLE ev = mk_event_handle();
    CC(SetStdHandle(STD_OUTPUT_HANDLE, ev));
    CEQ(hin_(), a); CEQ(hout_(), ev); CEQ(herr_(), c);
    CC(SetStdHandle(STD_OUTPUT_HANDLE, b));
    CC(SetStdHandle(STD_ERROR_HANDLE, ev));
    CEQ(hin_(), a); CEQ(hout_(), b); CEQ(herr_(), ev);
    CC(SetStdHandle(STD_ERROR_HANDLE, c));
    CloseHandle(ev);
}

CT(con_std_setstdhandle_ex) {
    CREQ(SetStdHandle);
    SetStdHandleEx_pfn fn = UN(SetStdHandleEx);
    CREQ(fn);
    HANDLE ev = mk_event_handle(), prev = (HANDLE)(ULONG_PTR)0x99, orig = hout_();
    CC(fn(STD_OUTPUT_HANDLE, ev, &prev));
    CEQ(prev, orig);
    CEQ(hout_(), ev);
    prev = NULL;
    CC(fn(STD_OUTPUT_HANDLE, orig, &prev));
    CEQ(prev, ev);
    CEQ(hout_(), orig);
    /* NULL previous pointer is allowed */
    CC(fn(STD_OUTPUT_HANDLE, orig, NULL));
    SetLastError(0xBAD);
    CEQ(fn(0x1234, orig, &prev), 0);
    CERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
}

CT(con_std_redirect_pipe) {
    CREQ(SetStdHandle); CREQ(WriteConsoleW); CREQ(GetConsoleMode); CREQ(SetConsoleMode);
    HANDLE r, w; DWORD n, m = 0;
    CC(CreatePipe(&r, &w, NULL, 0));
    HANDLE save = hout_();
    CC(SetStdHandle(STD_OUTPUT_HANDLE, w));
    CEQ(GetFileType(hout_()), FILE_TYPE_PIPE);
    CC(WriteFile(hout_(), "pipe-data", 9, &n, NULL)); CEQ(n, 9);
    char buf[32] = {0};
    CC(ReadFile(r, buf, sizeof buf, &n, NULL)); CEQ(n, 9); CSTR(buf, "pipe-data");
    /* console APIs reject the pipe handle */
    FAILS(GetConsoleMode(hout_(), &m), ERROR_INVALID_HANDLE);
    FAILS(SetConsoleMode(hout_(), ENABLE_PROCESSED_OUTPUT), ERROR_INVALID_HANDLE);
    DWORD wr = 0;
    /* P1 EXPECT-FIX: Win10 WriteConsoleW sends the ConDrv ioctl straight to the handle; a pipe answers
       ERROR_INVALID_FUNCTION (baseline 1). */
    FAILS(WriteConsoleW(hout_(), L"x", 1, &wr, NULL), ERROR_INVALID_FUNCTION);
    CC(SetStdHandle(STD_OUTPUT_HANDLE, save));
    CloseHandle(r); CloseHandle(w);
}

CT(con_std_redirect_file) {
    CREQ(SetStdHandle); CREQ(GetConsoleMode); CREQ(SetConsoleMode); CREQ(ReadConsoleW);
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "redir.txt");
    HANDLE f = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, NULL);
    CNE(f, INVALID_HANDLE_VALUE);
    HANDLE so = hout_(), si = hin_();
    CC(SetStdHandle(STD_OUTPUT_HANDLE, f));
    CC(SetStdHandle(STD_INPUT_HANDLE, f));
    CEQ(GetFileType(hout_()), FILE_TYPE_DISK);
    DWORD n, m;
    CC(WriteFile(hout_(), "abcdef", 6, &n, NULL)); CEQ(n, 6);
    SetFilePointer(f, 0, NULL, FILE_BEGIN);
    WCHAR wb[16]; DWORD got = 0;
    FAILS(GetConsoleMode(hin_(), &m), ERROR_INVALID_HANDLE);
    FAILS(SetConsoleMode(hin_(), 0), ERROR_INVALID_HANDLE);
    /* P1 EXPECT-FIX (audit: was a tolerance 6/1/87): a disk-file handle answers the ConDrv ioctl with its own status; baseline 87. */
    FAILS(ReadConsoleW(hin_(), wb, 4, &got, NULL), ERROR_INVALID_PARAMETER);
    CC(SetStdHandle(STD_OUTPUT_HANDLE, so));
    CC(SetStdHandle(STD_INPUT_HANDLE, si));
    CloseHandle(f);
}

CT(con_std_writefile_console) {
    CREQ(WriteConsoleW); CREQ(ReadConsoleOutputCharacterW);
    HANDLE b = mkbuf_std(); if (!b) return;
    DWORD n = 0;
    CC(WriteFile(b, "hello", 5, &n, NULL)); CEQ(n, 5);
    CC(rowis(b, 0, 0, L"hello"));
    CC(coeq(curpos(b), 5, 0));
    CC(WriteFile(b, "\r\nxy", 4, &n, NULL)); CEQ(n, 4);
    CC(rowis(b, 1, 0, L"xy"));
    CC(coeq(curpos(b), 2, 1));
    CC(WriteFile(b, "", 0, &n, NULL));
    CloseHandle(b);
}

CT(con_std_writefile_utf8_cp) {
    CREQ(SetConsoleOutputCP);
    UINT old = GetConsoleOutputCP();
    HANDLE b = mkbuf_std(); if (!b) return;
    CC(SetConsoleOutputCP(65001));
    DWORD n = 0;
    CC(WriteFile(b, "\xC3\xA9z", 3, &n, NULL));
    CCF(n == 3, "n=%lu", n);
    CEQ(cellch(b, 0, 0), 0xE9);
    CEQ(cellch(b, 1, 0), 'z');
    SetConsoleOutputCP(old);
    CloseHandle(b);
}

CT(con_std_readfile_line_mode) {
    CREQ(SetConsoleMode); CREQ(WriteConsoleInputW);
    HANDLE hi = hin_(); DWORD n = 0;
    CC(SetConsoleMode(hi, ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT));
    inp_reset();
    typestr(hi, L"ok\r");
    char buf[32] = {0};
    CC(ReadFile(hi, buf, sizeof buf - 1, &n, NULL));
    CCF(n == 4, "n=%lu", n);
    CC(!memcmp(buf, "ok\r\n", 4));
}

CT(con_std_readfile_raw_mode) {
    CREQ(SetConsoleMode); CREQ(WriteConsoleInputW);
    HANDLE hi = hin_(); DWORD n = 0;
    CC(SetConsoleMode(hi, 0));
    inp_reset();
    typestr(hi, L"xyz");
    char buf[32] = {0};
    CC(ReadFile(hi, buf, 3, &n, NULL));
    CEQ(n, 3);
    CC(!memcmp(buf, "xyz", 3));
}

CT(con_std_file_type_all_handles) {
    CREQ(CreateConsoleScreenBuffer);
    HANDLE b = mkbuf_raw(); CNE(b, NULL);
    CEQ(GetFileType(b), FILE_TYPE_CHAR);
    CloseHandle(b);
    HANDLE ci = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    HANDLE co_ = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CNE(ci, INVALID_HANDLE_VALUE); CNE(co_, INVALID_HANDLE_VALUE);
    CEQ(GetFileType(ci), FILE_TYPE_CHAR);
    CEQ(GetFileType(co_), FILE_TYPE_CHAR);
    CloseHandle(ci); CloseHandle(co_);
}

/* ---------------- GetConsoleMode / SetConsoleMode ---------------- */

CT(con_mode_get_invalid_handles) {
    CREQ(GetConsoleMode);
    DWORD m = 0xAA;
    HANDLE ev = mk_event_handle();
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "gm.txt");
    HANDLE f = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x1234, (HANDLE)(ULONG_PTR)-2, ev, f };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0xBAD);
        BOOL r = GetConsoleMode(bad[i], &m);
        CCF(r == FALSE, "i=%zu", i);
        CERR(ERROR_INVALID_HANDLE);
    }
    CEQ(m, 0xAA);
    CloseHandle(ev); CloseHandle(f);
}

CT(con_mode_set_invalid_handles) {
    CREQ(SetConsoleMode);
    HANDLE ev = mk_event_handle();
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x1234, ev };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        FAILS(SetConsoleMode(bad[i], 0), ERROR_INVALID_HANDLE);
        FAILS(SetConsoleMode(bad[i], ENABLE_PROCESSED_OUTPUT), ERROR_INVALID_HANDLE);
    }
    CloseHandle(ev);
}

CT(con_mode_input_default_bits) {
    CREQ(GetConsoleMode);
    DWORD m = 0;
    CC(GetConsoleMode(hin_(), &m));
    CEQ(m & (ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT), ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
    cnote("initial input mode 0x%lx", m);
}

CT(con_mode_output_default_bits) {
    CREQ(GetConsoleMode);
    DWORD m = 0;
    CC(GetConsoleMode(hout_(), &m));
    CEQ(m & (ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT), ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT);
    cnote("initial output mode 0x%lx", m);
    DWORD me = 0;
    CC(GetConsoleMode(herr_(), &me));
    CEQ(me, m);
}

CT(con_mode_input_roundtrip_table) {
    CREQ(GetConsoleMode); CREQ(SetConsoleMode);
    HANDLE hi = hin_(); DWORD orig = 0, m;
    GetConsoleMode(hi, &orig);
    DWORD tab[] = {
        0, ENABLE_PROCESSED_INPUT, ENABLE_LINE_INPUT, ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT,
        ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT, ENABLE_WINDOW_INPUT, ENABLE_MOUSE_INPUT,
        ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT, ENABLE_VIRTUAL_TERMINAL_INPUT,
        ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT | ENABLE_VIRTUAL_TERMINAL_INPUT,
        ENABLE_EXTENDED_FLAGS, ENABLE_EXTENDED_FLAGS | ENABLE_QUICK_EDIT_MODE, ENABLE_EXTENDED_FLAGS | ENABLE_INSERT_MODE,
        ENABLE_EXTENDED_FLAGS | ENABLE_AUTO_POSITION,
    };
    const DWORD mask = ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT | ENABLE_VIRTUAL_TERMINAL_INPUT;
    for (size_t i = 0; i < sizeof tab / sizeof tab[0]; i++) {
        SetLastError(0xBAD);
        BOOL r = SetConsoleMode(hi, tab[i]);
        CCF(r, "mode 0x%lx err=%lu", tab[i], GetLastError());
        m = 0xFFFFFFFF;
        CC(GetConsoleMode(hi, &m));
        CCF((m & mask) == (tab[i] & mask), "set 0x%lx got 0x%lx", tab[i], m);
    }
    CC(SetConsoleMode(hi, orig));
}

CT(con_mode_input_echo_requires_line) {
    CREQ(GetConsoleMode); CREQ(SetConsoleMode);
    HANDLE hi = hin_(); DWORD base = ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT, m = 0;
    CC(SetConsoleMode(hi, base));
    DWORD bad[] = { ENABLE_ECHO_INPUT, ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT, ENABLE_ECHO_INPUT | ENABLE_WINDOW_INPUT, ENABLE_ECHO_INPUT | ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0xBAD);
        BOOL r = SetConsoleMode(hi, bad[i]);
        CCF(r == FALSE, "mode 0x%lx", bad[i]);
        CERR(ERROR_INVALID_PARAMETER);
        /* conhost ApiRoutines::SetConsoleInputModeImpl stores the mode before validating it (getset.cpp:0x171),
         * so the failed call still leaves the requested bits visible; do not assert on the read-back. */
        CC(GetConsoleMode(hi, &m));
        cnote("mode after failed set 0x%lx: 0x%lx", bad[i], m);
        SetConsoleMode(hi, base);
    }
}

CT(con_mode_input_undefined_bits) {
    CREQ(GetConsoleMode); CREQ(SetConsoleMode);
    HANDLE hi = hin_(); DWORD orig = 0, m = 0;
    GetConsoleMode(hi, &orig);
    DWORD bad[] = { 0x400, 0x800, 0x8000, 0x10000, 0x80000000, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0xBAD);
        BOOL r = SetConsoleMode(hi, bad[i]);
        DWORD e = GetLastError();
        cnote("input SetConsoleMode(0x%lx) -> %d err=%lu", bad[i], r, e);
        if (!r) CEQ(e, ERROR_INVALID_PARAMETER);
        SetConsoleMode(hi, orig);
    }
    CC(GetConsoleMode(hi, &m));
}

CT(con_mode_output_roundtrip_table) {
    CREQ(GetConsoleMode); CREQ(SetConsoleMode);
    HANDLE b = mkbuf_raw(); CNE(b, NULL); if (!b) return;
    DWORD tab[] = { 0, 1, 2, 3, 4, 5, 7, 8, 0xB, 0xF, ENABLE_LVB_GRID_WORLDWIDE, 0x1F, ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN };
    for (size_t i = 0; i < sizeof tab / sizeof tab[0]; i++) {
        DWORD m = 0xFFFFFFFF;
        SetLastError(0xBAD);
        BOOL r = SetConsoleMode(b, tab[i]);
        CCF(r, "mode 0x%lx err=%lu", tab[i], GetLastError());
        CC(GetConsoleMode(b, &m));
        CCF((m & 0x1F) == (tab[i] & 0x1F), "set 0x%lx got 0x%lx", tab[i], m);
    }
    CloseHandle(b);
}

CT(con_mode_output_undefined_bits) {
    CREQ(GetConsoleMode); CREQ(SetConsoleMode);
    HANDLE b = mkbuf_raw(); CNE(b, NULL); if (!b) return;
    DWORD orig = 0, m = 0;
    GetConsoleMode(b, &orig);
    DWORD bad[] = { 0x20, 0x40, 0x100, 0x8000, 0x80000000 };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0xBAD);
        BOOL r = SetConsoleMode(b, bad[i]);
        DWORD e = GetLastError();
        cnote("output SetConsoleMode(0x%lx) -> %d err=%lu", bad[i], r, e);
        if (!r) CEQ(e, ERROR_INVALID_PARAMETER);
        SetConsoleMode(b, orig);
    }
    CC(GetConsoleMode(b, &m));
    CloseHandle(b);
}

CT(con_mode_per_buffer) {
    CREQ(GetConsoleMode); CREQ(SetConsoleMode);
    HANDLE a = mkbuf_raw(), b = mkbuf_raw(); CNE(a, NULL); CNE(b, NULL);
    if (!a || !b) return;
    DWORD m;
    CC(SetConsoleMode(a, ENABLE_PROCESSED_OUTPUT));
    CC(SetConsoleMode(b, 0));
    CC(GetConsoleMode(a, &m)); CEQ(m & 0x1F, ENABLE_PROCESSED_OUTPUT);
    CC(GetConsoleMode(b, &m)); CEQ(m & 0x1F, 0);
    CloseHandle(a); CloseHandle(b);
}

CT(con_mode_input_vs_output_handle) {
    CREQ(GetConsoleMode); CREQ(SetConsoleMode);
    /* input flags are only meaningful on the input handle; output flags on output handles */
    DWORD oi = 0, oo = 0, m;
    GetConsoleMode(hin_(), &oi); GetConsoleMode(hout_(), &oo);
    CC(SetConsoleMode(hout_(), ENABLE_PROCESSED_OUTPUT));
    CC(GetConsoleMode(hin_(), &m)); CEQ(m, oi);
    CC(SetConsoleMode(hin_(), ENABLE_PROCESSED_INPUT));
    CC(GetConsoleMode(hout_(), &m)); CEQ(m & 0x1F, ENABLE_PROCESSED_OUTPUT);
    SetConsoleMode(hin_(), oi); SetConsoleMode(hout_(), oo);
}

CT(con_mode_conin_conout_handles) {
    CREQ(GetConsoleMode); CREQ(SetConsoleMode);
    HANDLE ci = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    HANDLE co_ = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CNE(ci, INVALID_HANDLE_VALUE); CNE(co_, INVALID_HANDLE_VALUE);
    DWORD a = 0, b = 0, c = 0, d = 0;
    CC(GetConsoleMode(ci, &a)); CC(GetConsoleMode(hin_(), &b)); CEQ(a, b);
    CC(GetConsoleMode(co_, &c)); CC(GetConsoleMode(hout_(), &d)); CEQ(c, d);
    CNE(ci, hin_()); CNE(co_, hout_());
    DWORD orig = b;
    CC(SetConsoleMode(ci, ENABLE_PROCESSED_INPUT));
    CC(GetConsoleMode(hin_(), &b)); CEQ(b & 0x1F, ENABLE_PROCESSED_INPUT);
    SetConsoleMode(hin_(), orig);
    CloseHandle(ci); CloseHandle(co_);
}

CT(con_mode_duplicated_handle) {
    CREQ(GetConsoleMode); CREQ(SetConsoleMode);
    HANDLE d = NULL; DWORD m = 0, o = 0;
    CC(DuplicateHandle(GetCurrentProcess(), hout_(), GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS));
    CNE(d, NULL);
    GetConsoleMode(hout_(), &o);
    CC(GetConsoleMode(d, &m)); CEQ(m, o);
    CC(SetConsoleMode(d, ENABLE_PROCESSED_OUTPUT));
    CC(GetConsoleMode(hout_(), &m)); CEQ(m & 0x1F, ENABLE_PROCESSED_OUTPUT);
    SetConsoleMode(hout_(), o);
    CloseHandle(d);
    SetLastError(0xBAD);
    CEQ(GetConsoleMode(d, &m), 0);
    CERR(ERROR_INVALID_HANDLE);
}

CT(con_mode_closed_handle) {
    CREQ(GetConsoleMode); CREQ(CreateConsoleScreenBuffer);
    HANDLE b = mkbuf_raw(); CNE(b, NULL);
    DWORD m;
    CC(GetConsoleMode(b, &m));
    CC(CloseHandle(b));
    FAILS(GetConsoleMode(b, &m), ERROR_INVALID_HANDLE);
}

/* ---------------- code pages ---------------- */

CT(con_cp_get_nonzero) {
    CREQ(GetConsoleCP); CREQ(GetConsoleOutputCP);
    UINT a = GetConsoleCP(), b = GetConsoleOutputCP();
    CNE(a, 0); CNE(b, 0);
    CC(IsValidCodePage(a)); CC(IsValidCodePage(b));
    cnote("console cp in=%u out=%u acp=%u oemcp=%u", a, b, GetACP(), GetOEMCP());
}

CT(con_cp_set_roundtrip) {
    CREQ(GetConsoleCP); CREQ(SetConsoleCP); CREQ(GetConsoleOutputCP); CREQ(SetConsoleOutputCP);
    UINT oi = GetConsoleCP(), oo = GetConsoleOutputCP();
    UINT cps[] = { 437, 850, 1252, 65001, 437 };
    UINT acp = GetACP();
    for (size_t i = 0; i < sizeof cps / sizeof cps[0]; i++) {
        CCF(SetConsoleCP(cps[i]), "in %u err=%lu", cps[i], GetLastError());
        CEQ(GetConsoleCP(), cps[i]);
        CCF(SetConsoleOutputCP(cps[i]), "out %u err=%lu", cps[i], GetLastError());
        CEQ(GetConsoleOutputCP(), cps[i]);
    }
    CEQ(GetACP(), acp);
    SetConsoleCP(oi); SetConsoleOutputCP(oo);
}

CT(con_cp_input_output_independent) {
    CREQ(SetConsoleCP); CREQ(SetConsoleOutputCP);
    UINT oi = GetConsoleCP(), oo = GetConsoleOutputCP();
    CC(SetConsoleCP(437)); CC(SetConsoleOutputCP(1252));
    CEQ(GetConsoleCP(), 437); CEQ(GetConsoleOutputCP(), 1252);
    CC(SetConsoleCP(1252)); CC(SetConsoleOutputCP(437));
    CEQ(GetConsoleCP(), 1252); CEQ(GetConsoleOutputCP(), 437);
    SetConsoleCP(oi); SetConsoleOutputCP(oo);
}

CT(con_cp_set_invalid) {
    CREQ(SetConsoleCP); CREQ(SetConsoleOutputCP);
    UINT oi = GetConsoleCP(), oo = GetConsoleOutputCP();
    UINT bad[] = { 0, 1, 2, 3, 99, 0xFFFF, 0x10000, 0x7FFFFFFF, 0xFFFFFFFF, 65002, 12345 };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        FAILS(SetConsoleCP(bad[i]), ERROR_INVALID_PARAMETER);
        FAILS(SetConsoleOutputCP(bad[i]), ERROR_INVALID_PARAMETER);
    }
    CEQ(GetConsoleCP(), oi); CEQ(GetConsoleOutputCP(), oo);
}

CT(con_cp_special_values) {
    CREQ(SetConsoleCP); CREQ(SetConsoleOutputCP);
    UINT oi = GetConsoleCP(), oo = GetConsoleOutputCP();
    /* CP_ACP / CP_OEMCP pseudo values and UTF-16: record behaviour only */
    UINT odd[] = { CP_ACP, CP_OEMCP, CP_MACCP, CP_THREAD_ACP, CP_SYMBOL, 1200, 1201, 65000 };
    for (size_t i = 0; i < sizeof odd / sizeof odd[0]; i++) {
        SetLastError(0);
        BOOL r = SetConsoleOutputCP(odd[i]);
        cnote("SetConsoleOutputCP(%u) -> %d err=%lu now=%u", odd[i], r, GetLastError(), GetConsoleOutputCP());
        SetConsoleOutputCP(oo);
    }
    SetConsoleCP(oi); SetConsoleOutputCP(oo);
}

CT(con_cp_setconsolecp_does_not_touch_output) {
    CREQ(SetConsoleCP);
    UINT oo = GetConsoleOutputCP(), oi = GetConsoleCP();
    UINT want = oi == 437 ? 850 : 437;
    CC(SetConsoleCP(want));
    CEQ(GetConsoleOutputCP(), oo);
    SetConsoleCP(oi);
}

/* A/W conversion follows the console code page */
CT(con_cp_write_a_conversion) {
    CREQ(WriteConsoleA); CREQ(SetConsoleOutputCP);
    UINT oo = GetConsoleOutputCP();
    HANDLE b = mkbuf_std(); if (!b) return;
    DWORD n;
    CC(SetConsoleOutputCP(437));
    CC(WriteConsoleA(b, "\x82", 1, &n, NULL));  /* e acute in OEM 437 */
    CEQ(cellch(b, 0, 0), 0xE9);
    CC(SetConsoleOutputCP(1252));
    CC(WriteConsoleA(b, "\xE9", 1, &n, NULL));
    CEQ(cellch(b, 1, 0), 0xE9);
    CC(SetConsoleOutputCP(65001));
    CC(WriteConsoleA(b, "\xC3\xA9", 2, &n, NULL));
    CCF(n == 2, "n=%lu", n);
    CEQ(cellch(b, 2, 0), 0xE9);
    CC(SetConsoleOutputCP(437));
    CC(WriteConsoleA(b, "\xE1", 1, &n, NULL));  /* sharp s in 437 */
    CEQ(cellch(b, 3, 0), 0xDF);
    SetConsoleOutputCP(oo);
    CloseHandle(b);
}

/* ---------------- titles ---------------- */

CT(con_title_set_get_w) {
    CREQ(SetConsoleTitleW); CREQ(GetConsoleTitleW);
    WCHAR buf[128];
    CC(SetConsoleTitleW(L"k32t title one"));
    memset(buf, 0xAA, sizeof buf);
    DWORD n = GetConsoleTitleW(buf, 128);
    CEQ(n, 14);
    CWSTR(buf, L"k32t title one");
}

CT(con_title_set_get_a) {
    CREQ(SetConsoleTitleA); CREQ(GetConsoleTitleA);
    char buf[128];
    CC(SetConsoleTitleA("k32t ansi title"));
    memset(buf, 0x55, sizeof buf);
    DWORD n = GetConsoleTitleA(buf, 128);
    CEQ(n, 15);
    CSTR(buf, "k32t ansi title");
}

CT(con_title_a_w_parity) {
    CREQ(SetConsoleTitleA); CREQ(GetConsoleTitleW); CREQ(SetConsoleTitleW); CREQ(GetConsoleTitleA);
    WCHAR w[64]; char a[64];
    CC(SetConsoleTitleA("parity-A"));
    GetConsoleTitleW(w, 64); CWSTR(w, L"parity-A");
    CC(SetConsoleTitleW(L"parity-W"));
    GetConsoleTitleA(a, 64); CSTR(a, "parity-W");
}

CT(con_title_empty) {
    CREQ(SetConsoleTitleW); CREQ(GetConsoleTitleW);
    WCHAR w[8] = { 'x', 'x', 'x', 'x' };
    SetLastError(0xBAD);
    CC(SetConsoleTitleW(L""));
    DWORD n = GetConsoleTitleW(w, 8);
    CEQ(n, 0);
    CEQ(w[0], 0);
}

CT(con_title_buffer_sizes_w) {
    CREQ(SetConsoleTitleW); CREQ(GetConsoleTitleW);
    CC(SetConsoleTitleW(L"abcdef"));
    static const DWORD sizes[] = { 1, 2, 3, 6, 7, 8, 100 };
    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        WCHAR b[128]; DWORD sz = sizes[i];
        for (int k = 0; k < 128; k++) b[k] = 0x7777;
        DWORD n = GetConsoleTitleW(b, sz);
        if (sz >= 7) {
            CEQ(n, 6); CWSTR(b, L"abcdef");
        } else {
            cnote("GetConsoleTitleW size=%lu returned %lu", sz, n);
            CCF(b[sz - 1] == 0, "size=%lu not NUL terminated", sz);
            WCHAR ex[8]; lstrcpynW(ex, L"abcdef", sz > 0 ? (int)sz : 1);
            for (DWORD k = 0; k + 1 < sz; k++) CCF(b[k] == L"abcdef"[k], "size=%lu idx=%lu", sz, k);
        }
        CEQ(b[sz], 0x7777);   /* nothing written past the buffer */
    }
}

CT(con_title_size_zero) {
    CREQ(SetConsoleTitleW); CREQ(GetConsoleTitleW); CREQ(GetConsoleTitleA);
    CC(SetConsoleTitleW(L"abcdef"));
    WCHAR b[4] = { 0x7777, 0x7777, 0x7777, 0x7777 };
    DWORD n = GetConsoleTitleW(b, 0);
    cnote("GetConsoleTitleW(buf,0) -> %lu", n);
    CEQ(b[0], 0x7777);
    char a[4] = { 0x55, 0x55, 0x55, 0x55 };
    n = GetConsoleTitleA(a, 0);
    cnote("GetConsoleTitleA(buf,0) -> %lu", n);
    CEQ((unsigned char)a[0], 0x55);
}

CT(con_title_buffer_sizes_a) {
    CREQ(SetConsoleTitleA); CREQ(GetConsoleTitleA);
    CC(SetConsoleTitleA("abcdef"));
    static const DWORD sizes[] = { 1, 2, 6, 7, 8, 100 };
    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        char b[128]; DWORD sz = sizes[i];
        memset(b, 0x77, sizeof b);
        DWORD n = GetConsoleTitleA(b, sz);
        if (sz >= 7) { CEQ(n, 6); CSTR(b, "abcdef"); }
        else if (sz < 6) {
            /* P1 EXPECT-FIX: Win10 GetConsoleTitleAImplHelper returns an empty string and 0 when the buffer is
               shorter than the title (b[0] = 0, nothing else written). */
            cnote("GetConsoleTitleA size=%lu returned %lu", sz, n);
            CEQ(n, 0); CEQ((unsigned char)b[0], 0);
        } else {
            /* sz == 6: exact fit; Win10 returns 6 and copies the 6 characters WITHOUT a terminating NUL. */
            cnote("GetConsoleTitleA size=%lu returned %lu", sz, n);
            CEQ(n, 6); CC(!memcmp(b, "abcdef", 6));
        }
        CEQ((unsigned char)b[sz], 0x77);
    }
}

CT(con_title_long_1000) {
    CREQ(SetConsoleTitleW); CREQ(GetConsoleTitleW);
    static WCHAR s[1001], g[2048];
    for (int i = 0; i < 1000; i++) s[i] = L'a' + (i % 26);
    s[1000] = 0;
    CC(SetConsoleTitleW(s));
    DWORD n = GetConsoleTitleW(g, 2048);
    CEQ(n, 1000);
    CWSTR(g, s);
    n = GetConsoleTitleW(g, 10);
    cnote("long title, size 10 -> %lu", n);
    CEQ(g[9], 0);
}

CT(con_title_very_long) {
    CREQ(SetConsoleTitleW); CREQ(GetConsoleTitleW);
    static WCHAR s[40001], g[40001];
    for (int i = 0; i < 40000; i++) s[i] = L'q';
    s[40000] = 0;
    SetLastError(0xBAD);
    BOOL r = SetConsoleTitleW(s);
    DWORD e = GetLastError();
    DWORD n = GetConsoleTitleW(g, 40001);
    cnote("40000-char title: set=%d err=%lu get=%lu", r, e, n);
    CC(r ? (n > 0 && n <= 40000) : 1);
}

CT(con_title_unicode_roundtrip) {
    CREQ(SetConsoleTitleW); CREQ(GetConsoleTitleW);
    static const WCHAR t[] = { 'T', 0xE9, 0x4E2D, 0x0416, 0x20AC, 0 };
    WCHAR g[16];
    CC(SetConsoleTitleW(t));
    DWORD n = GetConsoleTitleW(g, 16);
    CEQ(n, 5);
    CWSTR(g, t);
}

CT(con_title_a_nonascii_cp) {
    CREQ(SetConsoleTitleA); CREQ(GetConsoleTitleW); CREQ(SetConsoleCP); CREQ(SetConsoleOutputCP);
    UINT oi = GetConsoleCP(), oo = GetConsoleOutputCP();
    SetConsoleCP(65001); SetConsoleOutputCP(65001);
    WCHAR g[16];
    CC(SetConsoleTitleA("A\xC3\xA9Z"));
    DWORD n = GetConsoleTitleW(g, 16);
    cnote("utf8 title -> %lu chars, [1]=0x%x", n, g[1]);
    CEQ(n, 3); CEQ(g[0], 'A'); CEQ(g[1], 0xE9); CEQ(g[2], 'Z');
    SetConsoleCP(437); SetConsoleOutputCP(437);
    CC(SetConsoleTitleA("\x82"));
    n = GetConsoleTitleW(g, 16);
    cnote("cp437 title -> %lu chars, [0]=0x%x", n, g[0]);
    SetConsoleCP(oi); SetConsoleOutputCP(oo);
}

CT(con_title_repeated_sets) {
    CREQ(SetConsoleTitleW); CREQ(GetConsoleTitleW);
    WCHAR g[32], e[32];
    for (int i = 0; i < 200; i++) {
        wsprintfW(e, L"title-%d", i);
        CC(SetConsoleTitleW(e));
        GetConsoleTitleW(g, 32);
        CWSTR(g, e);
    }
}

CTF(con_title_original_matches_start_title, CREATE_NEW_CONSOLE, "k32t_title_orig") {
    CREQ(GetConsoleOriginalTitleW); CREQ(GetConsoleTitleW); CREQ(SetConsoleTitleW); CREQ(GetConsoleOriginalTitleA);
    if (t_is_wine()) { cline("SKIP original title not tracked"); g_cskip = 1; return; }
    WCHAR o[64], t[64]; char oa[64];
    DWORD n = GetConsoleOriginalTitleW(o, 64);
    cnote("original title \"%ls\" (%lu)", o, n);
    /* P1b EXPECT-FIX: kernelbase GetConsoleOriginalTitleW is GetConsoleTitleInternal2(buf, nSize*2, 1) and, unlike
       GetConsoleTitleW (which stores L'\0' at buf[n]), never terminates the string; the baseline printed the title
       followed by stale stack text. Compare exactly n characters instead of relying on a terminator. UNVERIFIED-until-rerun */
    cnote("o[n]=0x%x", n < 64 ? o[n] : 0xFFFF);
    /* an elevated console host may prefix the title; accept that */
    CC(n >= 15 && !memcmp(o + n - 15, L"k32t_title_orig", 15 * sizeof(WCHAR)));
    GetConsoleTitleW(t, 64);
    CC(lstrlenW(t) >= 15 && !lstrcmpW(t + lstrlenW(t) - 15, L"k32t_title_orig"));
    CC(SetConsoleTitleW(L"changed"));
    n = GetConsoleOriginalTitleW(o, 64);
    CC(n >= 15 && !memcmp(o + n - 15, L"k32t_title_orig", 15 * sizeof(WCHAR)));
    n = GetConsoleOriginalTitleA(oa, 64);
    CC(n >= 15 && !strcmp(oa + n - 15, "k32t_title_orig"));
}

CTF(con_title_original_buffer_sizes, CREATE_NEW_CONSOLE, "k32t_title_orig") {
    CREQ(GetConsoleOriginalTitleW);
    if (t_is_wine()) { cline("SKIP original title not tracked"); g_cskip = 1; return; }
    WCHAR b[32];
    for (int k = 0; k < 32; k++) b[k] = 0x7777;
    DWORD n = GetConsoleOriginalTitleW(b, 4);
    cnote("original title size 4 -> %lu", n);
    CEQ(b[3], 0);
    CEQ(b[4], 0x7777);
    n = GetConsoleOriginalTitleW(b, 16);
    CEQ(n, 15);
    /* P1b EXPECT-FIX: GetConsoleOriginalTitleW does not store a terminator when the whole title fits (kernelbase
       GetConsoleTitleInternal2 returns the server count and writes nothing, unlike GetConsoleTitleW which stores
       buf[n] = 0 itself because the server reply carries no terminator); baseline b[15] stayed 0x7777.
       Audit: the slot after the title is asserted untouched rather than left unchecked. UNVERIFIED-until-rerun */
    CC(!memcmp(b, L"k32t_title_orig", 15 * sizeof(WCHAR)));
    cnote("b[15]=0x%x after size 16", b[15]);
    CEQ(b[15], 0x7777);
    n = GetConsoleOriginalTitleW(b, 15);
    cnote("original title size 15 -> %lu", n);
    CEQ(b[14], 0);
}

/* ---------------- GetConsoleProcessList ---------------- */

CT(con_proclist_invalid_args) {
    CREQ(GetConsoleProcessList);
    DWORD list[4];
    SetLastError(0xBAD);
    CEQ(GetConsoleProcessList(list, 0), 0); CERR(ERROR_INVALID_PARAMETER);
    SetLastError(0xBAD);
    CEQ(GetConsoleProcessList(NULL, 4), 0); CERR(ERROR_INVALID_PARAMETER);
    SetLastError(0xBAD);
    CEQ(GetConsoleProcessList(NULL, 0), 0); CERR(ERROR_INVALID_PARAMETER);
}

CT(con_proclist_self_only) {
    CREQ(GetConsoleProcessList);
    DWORD list[8] = { 0 };
    DWORD n = GetConsoleProcessList(list, 8);
    CEQ(n, 1);
    CEQ(list[0], GetCurrentProcessId());
    DWORD one = 0;
    n = GetConsoleProcessList(&one, 1);
    CEQ(n, 1); CEQ(one, GetCurrentProcessId());
}

CG(con_sleep) {
    GSETUP();
    HANDLE ev = OpenEventA(SYNCHRONIZE, FALSE, argv[2]);
    if (!ev) { cline("FAIL g_con_sleep OpenEvent %lu", GetLastError()); return 11; }
    WaitForSingleObject(ev, 20000);
    return 0;
}

CT(con_proclist_two_processes) {
    CREQ(GetConsoleProcessList);
    char name[64]; snprintf(name, sizeof name, "Local\\k32t_pl_%lu", GetCurrentProcessId());
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, name);
    PROCESS_INFORMATION pi;
    HANDLE g = gspawn("con_sleep", name, 0, NULL, NULL, NULL, FALSE, &pi);
    CNE(g, NULL); if (!g) return;
    Sleep(500);
    DWORD list[8] = { 0 };
    DWORD n = GetConsoleProcessList(list, 8);
    CEQ(n, 2);
    int me = 0, ch = 0;
    for (DWORD i = 0; i < 2 && i < 8; i++) { if (list[i] == GetCurrentProcessId()) me++; if (list[i] == pi.dwProcessId) ch++; }
    CEQ(me, 1); CEQ(ch, 1);
    /* too small: returns the required count, array contents unspecified */
    DWORD one[1] = { 0xDEAD };
    n = GetConsoleProcessList(one, 1);
    CEQ(n, 2);
    SetEvent(ev);
    CEQ(gwait(g, 10000), 0);
    CloseHandle(pi.hThread); CloseHandle(g);
    Sleep(200);
    n = GetConsoleProcessList(list, 8);
    CEQ(n, 1);
    CloseHandle(ev);
}

/* ---------------- GetConsoleWindow ---------------- */

CT(con_window_valid) {
    CREQ(GetConsoleWindow);
    if (t_is_wine()) { cline("SKIP headless console has no window"); g_cskip = 1; return; }
    HWND h = GetConsoleWindow();
    CNE(h, NULL);
    FARPROC isw = GetProcAddress(t_user32(), "IsWindow");
    if (isw) { CC(((BOOL (WINAPI *)(HWND))isw)(h)); }
    CEQ(GetConsoleWindow(), h);
}

CT(con_window_class_and_owner) {
    CREQ(GetConsoleWindow);
    if (t_is_wine()) { cline("SKIP headless console has no window"); g_cskip = 1; return; }
    HWND h = GetConsoleWindow();
    CNE(h, NULL);
    typedef int (WINAPI *gcn_t)(HWND, LPWSTR, int);
    typedef DWORD (WINAPI *gwtp_t)(HWND, LPDWORD);
    gcn_t gcn = (gcn_t)GetProcAddress(t_user32(), "GetClassNameW");
    gwtp_t gw = (gwtp_t)GetProcAddress(t_user32(), "GetWindowThreadProcessId");
    WCHAR cls[64] = { 0 };
    if (gcn) { gcn(h, cls, 64); cnote("console window class \"%ls\"", cls); }
    DWORD pid = 0;
    if (gw) { gw(h, &pid); cnote("console window owner pid %lu (self %lu)", pid, GetCurrentProcessId()); }
}

CT(con_window_no_arg_stability) {
    CREQ(GetConsoleWindow);
    HWND h = GetConsoleWindow();
    for (int i = 0; i < 100; i++) CEQ(GetConsoleWindow(), h);
}

CTD(con_detached_no_window) {
    CREQ(GetConsoleWindow); CREQ(GetConsoleMode);
    CEQ(GetConsoleWindow(), NULL);
    HANDLE h[3]; h[0] = hin_(); h[1] = hout_(); h[2] = herr_();
    for (int i = 0; i < 3; i++) {
        cnote("detached std handle %d = %p", i, h[i]);
        CC(h[i] == NULL || h[i] == INVALID_HANDLE_VALUE);
        DWORD m;
        CEQ(GetConsoleMode(h[i], &m), 0);
    }
}

CTD(con_detached_apis_fail) {
    CREQ(GetConsoleCP); CREQ(GetConsoleOutputCP); CREQ(GetConsoleTitleW); CREQ(GetConsoleProcessList);
    CREQ(WriteConsoleW); CREQ(FlushConsoleInputBuffer); CREQ(GetNumberOfConsoleInputEvents);
    SetLastError(0xBAD);
    CEQ(GetConsoleCP(), 0); cnote("detached GetConsoleCP err=%lu", GetLastError());
    SetLastError(0xBAD);
    CEQ(GetConsoleOutputCP(), 0); cnote("detached GetConsoleOutputCP err=%lu", GetLastError());
    WCHAR t[16] = { 1 };
    SetLastError(0xBAD);
    CEQ(GetConsoleTitleW(t, 16), 0); cnote("detached GetConsoleTitleW err=%lu", GetLastError());
    DWORD list[4];
    SetLastError(0xBAD);
    CEQ(GetConsoleProcessList(list, 4), 0); cnote("detached GetConsoleProcessList err=%lu", GetLastError());
    DWORD n;
    FAILS(WriteConsoleW(NULL, L"x", 1, &n, NULL), ERROR_INVALID_HANDLE);
    FAILS(FlushConsoleInputBuffer(NULL), ERROR_INVALID_HANDLE);
    FAILS(GetNumberOfConsoleInputEvents(NULL, &n), ERROR_INVALID_HANDLE);
    HANDLE c = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    cnote("detached open CONOUT$ -> %p err=%lu", c, GetLastError());
    CEQ(c, INVALID_HANDLE_VALUE);
}

CTD(con_detached_alloc_free_cycle) {
    CREQ(AllocConsole); CREQ(FreeConsole); CREQ(GetConsoleWindow); CREQ(GetConsoleCP);
    if (t_is_wine()) { cline("SKIP windowed AllocConsole needs a display"); g_cskip = 1; return; }
    CEQ(GetConsoleWindow(), NULL);
    SetLastError(0xBAD);
    CC(AllocConsole());
    CNE(GetConsoleWindow(), NULL);
    CNE(GetConsoleCP(), 0);
    HANDLE o = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CNE(o, INVALID_HANDLE_VALUE);
    /* second AllocConsole fails: the process already has a console */
    SetLastError(0xBAD);
    CEQ(AllocConsole(), 0); CERR(ERROR_ACCESS_DENIED);
    CC(wrw(o, L"alloc"));
    CC(rowis(o, 0, 0, L"alloc"));
    CloseHandle(o);
    CC(FreeConsole());
    CEQ(GetConsoleWindow(), NULL);
    SetLastError(0xBAD);
    CEQ(GetConsoleCP(), 0);
    CC(FreeConsole());   /* always succeeds, even without a console */
    CC(AllocConsole());
    CNE(GetConsoleWindow(), NULL);
    CC(FreeConsole());
}

CTD(con_detached_alloc_std_handles) {
    CREQ(AllocConsole); CREQ(FreeConsole); CREQ(GetStdHandle); CREQ(GetConsoleMode);
    if (t_is_wine()) { cline("SKIP windowed AllocConsole needs a display"); g_cskip = 1; return; }
    cnote("before alloc: in=%p out=%p err=%p", hin_(), hout_(), herr_());
    CC(AllocConsole());
    HANDLE h[3]; h[0] = hin_(); h[1] = hout_(); h[2] = herr_();
    for (int i = 0; i < 3; i++) {
        cnote("after alloc std %d = %p type=%lu", i, h[i], h[i] && h[i] != INVALID_HANDLE_VALUE ? GetFileType(h[i]) : 0);
        DWORD m;
        CCF(GetConsoleMode(h[i], &m), "std handle %d usable after AllocConsole", i);
    }
    FreeConsole();
}

CTD(con_detached_free_when_none) {
    CREQ(FreeConsole);
    for (int i = 0; i < 5; i++) { SetLastError(0xBAD); CC(FreeConsole()); }
}

CT(con_free_alloc_in_console_process) {
    CREQ(FreeConsole); CREQ(AllocConsole); CREQ(GetConsoleWindow); CREQ(GetConsoleMode);
    if (t_is_wine()) { cline("SKIP headless console has no window"); g_cskip = 1; return; }
    HWND h0 = GetConsoleWindow();
    CNE(h0, NULL);
    HANDLE oldout = hout_();
    CC(FreeConsole());
    CEQ(GetConsoleWindow(), NULL);
    DWORD m;
    SetLastError(0xBAD);
    CEQ(GetConsoleMode(oldout, &m), 0);
    cnote("GetConsoleMode(stale std handle) err=%lu", GetLastError());
    CC(AllocConsole());
    HWND h1 = GetConsoleWindow();
    CNE(h1, NULL);
    cnote("console window changed: %d", h0 != h1);
    CC(FreeConsole());
}

/* ---------------- AttachConsole ---------------- */

CT(con_attach_while_attached_denied) {
    CREQ(AttachConsole);
    SetLastError(0xBAD);
    CEQ(AttachConsole(GetCurrentProcessId()), 0); CERR(ERROR_ACCESS_DENIED);
    SetLastError(0xBAD);
    CEQ(AttachConsole(ATTACH_PARENT_PROCESS), 0); CERR(ERROR_ACCESS_DENIED);
    SetLastError(0xBAD);
    CEQ(AttachConsole(0x7FFFFFF0), 0); CERR(ERROR_ACCESS_DENIED);
    SetLastError(0xBAD);
    CEQ(AllocConsole(), 0); CERR(ERROR_ACCESS_DENIED);
}

CG(con_attach_parent) {
    GSETUP();
    CREQG(AttachConsole); CREQG(FreeConsole); CREQG(GetConsoleWindow); CREQG(GetConsoleProcessList); CREQG(AllocConsole);
    HWND want = (HWND)(ULONG_PTR)_strtoui64(argv[2], NULL, 16);
    DWORD ppid = (DWORD)atoi(argv[3]);
    CEQ(GetConsoleWindow(), NULL);
    SetLastError(0xBAD);
    CC(AttachConsole(ATTACH_PARENT_PROCESS));
    CEQ(GetConsoleWindow(), want);
    DWORD list[8]; DWORD n = GetConsoleProcessList(list, 8);
    CEQ(n, 2);
    SetLastError(0xBAD);
    CEQ(AttachConsole(ATTACH_PARENT_PROCESS), 0); CERR(ERROR_ACCESS_DENIED);
    CC(FreeConsole());
    CEQ(GetConsoleWindow(), NULL);
    /* attach by explicit pid */
    SetLastError(0xBAD);
    CC(AttachConsole(ppid));
    CEQ(GetConsoleWindow(), want);
    CC(FreeConsole());
    /* attach to own pid: this process has no console to share */
    SetLastError(0xBAD);
    BOOL r = AttachConsole(GetCurrentProcessId());
    cnote("AttachConsole(self, detached) -> %d err=%lu", r, GetLastError());
    CEQ(r, 0);
    /* nonexistent pid */
    SetLastError(0xBAD);
    r = AttachConsole(0x7FFFFFF0);
    cnote("AttachConsole(bogus pid) -> %d err=%lu", r, GetLastError());
    CEQ(r, 0);
    SetLastError(0xBAD);
    r = AttachConsole(0);
    cnote("AttachConsole(0) -> %d err=%lu", r, GetLastError());
    if (r) FreeConsole();
    return 0;
}

CT(con_attach_detached_child_to_parent) {
    CREQ(GetConsoleWindow); CREQ(AttachConsole);
    char args[128];
    snprintf(args, sizeof args, "%llx %lu", (unsigned long long)(ULONG_PTR)GetConsoleWindow(), GetCurrentProcessId());
    PROCESS_INFORMATION pi;
    HANDLE g = gspawn("con_attach_parent", args, DETACHED_PROCESS, NULL, NULL, NULL, FALSE, &pi);
    CNE(g, NULL); if (!g) return;
    CEQ(gwait(g, 15000), 0);
    CloseHandle(pi.hThread); CloseHandle(g);
}

CG(con_nowindow_probe) {
    GSETUP();
    CEQ(GetConsoleWindow(), NULL);
    return 0;
}

CTN(con_nowindow_console_state) {
    CREQ(GetConsoleWindow); CREQ(GetConsoleCP); CREQ(GetConsoleMode); CREQ(GetConsoleProcessList);
    CEQ(GetConsoleWindow(), NULL);
    DWORD m = 0, list[4];
    UINT cp = GetConsoleCP();
    DWORD n = GetConsoleProcessList(list, 4);
    cnote("CREATE_NO_WINDOW: cp=%u proclist=%lu in=%p out=%p", cp, n, hin_(), hout_());
    BOOL mi = GetConsoleMode(hin_(), &m);
    BOOL mo = GetConsoleMode(hout_(), &m);
    cnote("CREATE_NO_WINDOW: GetConsoleMode in=%d out=%d type(out)=%lu", mi, mo, hout_() && hout_() != INVALID_HANDLE_VALUE ? GetFileType(hout_()) : 0);
    CNE(cp, 0);
    CEQ(n, 1);
}

CTN(con_nowindow_screen_buffer_usable) {
    CREQ(CreateConsoleScreenBuffer); CREQ(GetConsoleScreenBufferInfo);
    HANDLE b = mkbuf_raw();
    CNE(b, NULL); if (!b) return;
    CONSOLE_SCREEN_BUFFER_INFO i;
    CC(GetConsoleScreenBufferInfo(b, &i));
    CC(i.dwSize.X > 0 && i.dwSize.Y > 0);
    CC(wrw(b, L"nowin"));
    CC(rowis(b, 0, 0, L"nowin"));
    CloseHandle(b);
}

/* ---------------- history / selection / display / misc ---------------- */

CT(con_history_info_roundtrip) {
    CREQ(GetConsoleHistoryInfo); CREQ(SetConsoleHistoryInfo);
    CONSOLE_HISTORY_INFO hi, orig;
    memset(&orig, 0, sizeof orig); orig.cbSize = sizeof orig;
    CC(GetConsoleHistoryInfo(&orig));
    cnote("history size=%u n=%u flags=0x%x", orig.HistoryBufferSize, orig.NumberOfHistoryBuffers, orig.dwFlags);
    CC(orig.HistoryBufferSize > 0);
    CC(orig.NumberOfHistoryBuffers > 0);
    hi.cbSize = sizeof hi; hi.HistoryBufferSize = 25; hi.NumberOfHistoryBuffers = 3; hi.dwFlags = HISTORY_NO_DUP_FLAG;
    CC(SetConsoleHistoryInfo(&hi));
    memset(&hi, 0, sizeof hi); hi.cbSize = sizeof hi;
    CC(GetConsoleHistoryInfo(&hi));
    CEQ(hi.HistoryBufferSize, 25); CEQ(hi.NumberOfHistoryBuffers, 3); CEQ(hi.dwFlags, HISTORY_NO_DUP_FLAG);
    hi.HistoryBufferSize = 100; hi.NumberOfHistoryBuffers = 7; hi.dwFlags = 0;
    CC(SetConsoleHistoryInfo(&hi));
    memset(&hi, 0, sizeof hi); hi.cbSize = sizeof hi;
    CC(GetConsoleHistoryInfo(&hi));
    CEQ(hi.HistoryBufferSize, 100); CEQ(hi.NumberOfHistoryBuffers, 7); CEQ(hi.dwFlags, 0);
    SetConsoleHistoryInfo(&orig);
}

CT(con_history_info_bad_cbsize) {
    CREQ(GetConsoleHistoryInfo); CREQ(SetConsoleHistoryInfo);
    CONSOLE_HISTORY_INFO hi; memset(&hi, 0, sizeof hi);
    DWORD sizes[] = { 0, 4, 8, 12, 20, 32, 0x10000 };
    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        hi.cbSize = sizes[i];
        SetLastError(0xBAD);
        BOOL r = GetConsoleHistoryInfo(&hi);
        CCF(r == FALSE, "get cbSize=%lu", sizes[i]);
        CERR(ERROR_INVALID_PARAMETER);
        hi.cbSize = sizes[i]; hi.HistoryBufferSize = 50; hi.NumberOfHistoryBuffers = 4;
        SetLastError(0xBAD);
        r = SetConsoleHistoryInfo(&hi);
        CCF(r == FALSE, "set cbSize=%lu", sizes[i]);
        CERR(ERROR_INVALID_PARAMETER);
    }
}

CT(con_history_info_invalid_values) {
    CREQ(GetConsoleHistoryInfo); CREQ(SetConsoleHistoryInfo);
    CONSOLE_HISTORY_INFO orig, hi;
    orig.cbSize = sizeof orig; GetConsoleHistoryInfo(&orig);
    hi.cbSize = sizeof hi; hi.HistoryBufferSize = 50; hi.NumberOfHistoryBuffers = 4; hi.dwFlags = 0xFFFF0000;
    SetLastError(0xBAD);
    BOOL r = SetConsoleHistoryInfo(&hi);
    cnote("history flags 0xFFFF0000 -> %d err=%lu", r, GetLastError());
    hi.dwFlags = 0; hi.HistoryBufferSize = 0xFFFFFFFF;
    SetLastError(0xBAD);
    r = SetConsoleHistoryInfo(&hi);
    cnote("history size huge -> %d err=%lu", r, GetLastError());
    SetConsoleHistoryInfo(&orig);
}

CT(con_selection_info_no_selection) {
    CREQ(GetConsoleSelectionInfo);
    CONSOLE_SELECTION_INFO si; memset(&si, 0xCC, sizeof si);
    SetLastError(0xBAD);
    BOOL r = GetConsoleSelectionInfo(&si);
    CCF(r, "err=%lu", GetLastError());
    CEQ(si.dwFlags & CONSOLE_SELECTION_IN_PROGRESS, 0);
    CEQ(si.dwFlags, CONSOLE_NO_SELECTION);
}

CT(con_display_mode_windowed) {
    CREQ(GetConsoleDisplayMode);
    DWORD f = 0xFFFF;
    CC(GetConsoleDisplayMode(&f));
    CEQ(f, 0);
}

CT(con_display_mode_set) {
    CREQ(SetConsoleDisplayMode);
    /* Win10 kernelbase: unconditional stub, FALSE + ERROR_CALL_NOT_IMPLEMENTED */
    HANDLE b = mkbuf_raw(); CNE(b, NULL);
    COORD nd = { 7, 7 };
    DWORD modes[] = { 0, 1, 2, 3, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof modes / sizeof modes[0]; i++) {
        FAILS(SetConsoleDisplayMode(b, modes[i], &nd), ERROR_CALL_NOT_IMPLEMENTED);
        FAILS(SetConsoleDisplayMode(hout_(), modes[i], &nd), ERROR_CALL_NOT_IMPLEMENTED);
    }
    FAILS(SetConsoleDisplayMode(INVALID_HANDLE_VALUE, 2, &nd), ERROR_CALL_NOT_IMPLEMENTED);
    FAILS(SetConsoleDisplayMode(NULL, 2, NULL), ERROR_CALL_NOT_IMPLEMENTED);
    CEQ(nd.X, 7); CEQ(nd.Y, 7);
    if (b) CloseHandle(b);
}

CT(con_mouse_buttons) {
    CREQ(GetNumberOfConsoleMouseButtons);
    DWORD n = 0xFFFF;
    CC(GetNumberOfConsoleMouseButtons(&n));
    cnote("mouse buttons=%lu", n);
    CC(n < 32);
}

CT(con_keyboard_layout_name) {
    GetConsoleKeyboardLayoutNameA_pfn a = UN(GetConsoleKeyboardLayoutNameA);
    GetConsoleKeyboardLayoutNameW_pfn w = UN(GetConsoleKeyboardLayoutNameW);
    CREQ(a); CREQ(w);
    CWINESKIP("keyboard layout name is not reported by the emulator");
    char ab[16]; WCHAR wb[16];
    memset(ab, 0, sizeof ab); memset(wb, 0, sizeof wb);
    BOOL ra = a(ab), rw = w(wb);
    cnote("layout A=%d \"%s\" W=%d \"%ls\"", ra, ab, rw, wb);
    if (ra) CEQ(strlen(ab), 8);
    if (rw) CEQ(lstrlenW(wb), 8);
}
