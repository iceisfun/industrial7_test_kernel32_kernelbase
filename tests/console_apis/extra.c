/* console_apis: legacy / undocumented exports, device names, handle lifetime, mode bit tables, threads */
#include "ch.h"

#define FAILS(call, err)  do { SetLastError(0xBAD); BOOL r_ = (call); CEQ(r_, 0); CERR(err); } while (0)

/* ---------------- OpenConsoleW (CreateFileW with OPEN_EXISTING) ---------------- */

CT(con_openconsole_conout) {
    typedef HANDLE (WINAPI *oc_t)(LPCWSTR, DWORD, BOOL, DWORD);
    oc_t oc = (oc_t)t_proc_k32("OpenConsoleW");
    CREQ(oc); CREQ(GetConsoleMode);
    SetLastError(0xBAD);
    HANDLE h = oc(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FALSE, FILE_SHARE_READ | FILE_SHARE_WRITE);
    CNE(h, INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD m; CC(GetConsoleMode(h, &m));
    CEQ(GetFileType(h), FILE_TYPE_CHAR);
    DWORD fl = 0; CC(GetHandleInformation(h, &fl));
    CEQ(fl & HANDLE_FLAG_INHERIT, 0);
    CloseHandle(h);
    h = oc(L"CONIN$", GENERIC_READ | GENERIC_WRITE, TRUE, FILE_SHARE_READ | FILE_SHARE_WRITE);
    CNE(h, INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) {
        CC(GetHandleInformation(h, &fl));
        CEQ(fl & HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        CloseHandle(h);
    }
}

CT(con_openconsole_non_console_name) {
    typedef HANDLE (WINAPI *oc_t)(LPCWSTR, DWORD, BOOL, DWORD);
    oc_t oc = (oc_t)t_proc_k32("OpenConsoleW");
    CREQ(oc);
    SetLastError(0xBAD);
    HANDLE h = oc(L"k32t_no_such_dir\\nothing.txt", GENERIC_READ, FALSE, FILE_SHARE_READ);
    CEQ(h, INVALID_HANDLE_VALUE);
    DWORD e = GetLastError();
    CC(e == ERROR_PATH_NOT_FOUND || e == ERROR_FILE_NOT_FOUND);
}

/* ---------------- device-name opens ---------------- */

CT(con_device_name_variants) {
    CREQ(GetConsoleMode);
    static const WCHAR *names[] = { L"CONIN$", L"conin$", L"CONOUT$", L"conout$", L"\\\\.\\CONOUT$", L"\\\\.\\CONIN$" };
    for (size_t k = 0; k < sizeof names / sizeof names[0]; k++) {
        SetLastError(0xBAD);
        HANDLE h = CreateFileW(names[k], GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        CCF(h != INVALID_HANDLE_VALUE, "%ls err=%lu", names[k], GetLastError());
        if (h != INVALID_HANDLE_VALUE) {
            DWORD m; CCF(GetConsoleMode(h, &m), "%ls not a console handle", names[k]);
            CloseHandle(h);
        }
    }
}

CT(con_device_conin_access_variants) {
    DWORD acc[] = { GENERIC_READ, GENERIC_WRITE, GENERIC_READ | GENERIC_WRITE, 0 };
    DWORD sh[] = { FILE_SHARE_READ, FILE_SHARE_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0 };
    for (size_t i = 0; i < 4; i++) for (size_t j = 0; j < 4; j++) {
        SetLastError(0xBAD);
        HANDLE h = CreateFileW(L"CONIN$", acc[i], sh[j], NULL, OPEN_EXISTING, 0, NULL);
        cnote("CONIN$ access=0x%lx share=0x%lx -> %s (err %lu)", acc[i], sh[j], h == INVALID_HANDLE_VALUE ? "fail" : "ok", GetLastError());
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }
    HANDLE h = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CNE(h, INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
}

CT(con_device_conout_dispositions) {
    DWORD disp[] = { CREATE_NEW, CREATE_ALWAYS, OPEN_EXISTING, OPEN_ALWAYS, TRUNCATE_EXISTING };
    for (size_t k = 0; k < sizeof disp / sizeof disp[0]; k++) {
        SetLastError(0xBAD);
        HANDLE h = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, disp[k], 0, NULL);
        cnote("CONOUT$ disposition %lu -> %s (err %lu)", disp[k], h == INVALID_HANDLE_VALUE ? "fail" : "ok", GetLastError());
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }
    HANDLE h = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CNE(h, INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
}

CT(con_device_nul_is_not_console) {
    CREQ(GetConsoleMode); CREQ(SetConsoleMode);
    HANDLE h = CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CNE(h, INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    CEQ(GetFileType(h), FILE_TYPE_CHAR);
    DWORD m;
    FAILS(GetConsoleMode(h, &m), ERROR_INVALID_HANDLE);
    FAILS(SetConsoleMode(h, 0), ERROR_INVALID_HANDLE);
    CloseHandle(h);
}

CT(con_device_inheritable_console_handle_flag) {
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE h = CreateFileW(L"CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
    CNE(h, INVALID_HANDLE_VALUE);
    DWORD fl = 0;
    CC(GetHandleInformation(h, &fl));
    CEQ(fl & HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    CC(SetHandleInformation(h, HANDLE_FLAG_INHERIT, 0));
    CC(GetHandleInformation(h, &fl));
    CEQ(fl & HANDLE_FLAG_INHERIT, 0);
    CloseHandle(h);
}

/* ---------------- handle lifetime ---------------- */

CT(con_handle_close_dup_keeps_buffer) {
    CREQ(GetConsoleMode); CREQ(WriteConsoleW);
    HANDLE b = mkbuf_std(); if (!b) return;
    HANDLE d = NULL;
    CC(DuplicateHandle(GetCurrentProcess(), b, GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS));
    CC(wrw(b, L"before"));
    CC(CloseHandle(b));
    CC(rowis(d, 0, 0, L"before"));
    CC(wrw(d, L"+after"));
    CC(rowis(d, 0, 0, L"before+after"));
    CloseHandle(d);
}

CT(con_handle_duplicate_reduced_access) {
    CREQ(WriteConsoleW); CREQ(ReadConsoleOutputCharacterW);
    HANDLE b = mkbuf_std(); if (!b) return;
    HANDLE d = NULL;
    BOOL ok = DuplicateHandle(GetCurrentProcess(), b, GetCurrentProcess(), &d, GENERIC_READ, FALSE, 0);
    cnote("DuplicateHandle(GENERIC_READ) of a screen buffer -> %d err=%lu", ok, GetLastError());
    if (ok) {
        DWORD n = 0;
        SetLastError(0xBAD);
        BOOL w = WriteConsoleW(d, L"x", 1, &n, NULL);
        cnote("write through read-only duplicate -> %d err=%lu", w, GetLastError());
        CEQ(w, 0);
        WCHAR c; CC(ReadConsoleOutputCharacterW(d, &c, 1, co(0, 0), &n));
        CloseHandle(d);
    }
    CloseHandle(b);
}

CT(con_handle_close_std_output) {
    CREQ(GetConsoleMode);
    HANDLE dup = NULL;
    CC(DuplicateHandle(GetCurrentProcess(), hout_(), GetCurrentProcess(), &dup, 0, FALSE, DUPLICATE_SAME_ACCESS));
    HANDLE o = hout_();
    CC(CloseHandle(o));
    DWORD m;
    FAILS(GetConsoleMode(o, &m), ERROR_INVALID_HANDLE);
    CC(GetConsoleMode(dup, &m));
    SetLastError(0xBAD);
    BOOL again = CloseHandle(o);
    cnote("second CloseHandle(std out) -> %d err=%lu", again, GetLastError());
    CloseHandle(dup);
}

CT(con_handle_screen_buffer_waitable) {
    HANDLE b = mkbuf_raw(); CNE(b, NULL); if (!b) return;
    SetLastError(0xBAD);
    DWORD w = WaitForSingleObject(b, 0);
    cnote("WaitForSingleObject(screen buffer, 0) -> %lu err=%lu", w, GetLastError());
    CloseHandle(b);
}

CT(con_handle_console_input_handle_reopen_shares_queue) {
    CREQ(FlushConsoleInputBuffer); CREQ(GetNumberOfConsoleInputEvents);
    HANDLE a = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    HANDLE b = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CNE(a, INVALID_HANDLE_VALUE); CNE(b, INVALID_HANDLE_VALUE);
    inp_reset();
    typestr(a, L"xy");
    DWORD n = 0;
    CC(GetNumberOfConsoleInputEvents(b, &n)); CEQ(n, 4);
    CC(GetNumberOfConsoleInputEvents(hin_(), &n)); CEQ(n, 4);
    CC(FlushConsoleInputBuffer(b));
    CC(GetNumberOfConsoleInputEvents(a, &n)); CEQ(n, 0);
    CloseHandle(a); CloseHandle(b);
}

/* ---------------- mode bit tables ---------------- */

CT(con_mode_input_single_bits) {
    CREQ(GetConsoleMode); CREQ(SetConsoleMode);
    HANDLE hi = hin_(); DWORD orig = 0, m;
    GetConsoleMode(hi, &orig);
    static const struct { DWORD bit; int ok; } t[] = {
        { ENABLE_PROCESSED_INPUT, 1 }, { ENABLE_LINE_INPUT, 1 }, { ENABLE_ECHO_INPUT, 0 }, { ENABLE_WINDOW_INPUT, 1 },
        { ENABLE_MOUSE_INPUT, 1 }, { ENABLE_INSERT_MODE, 1 }, { ENABLE_QUICK_EDIT_MODE, 1 }, { ENABLE_EXTENDED_FLAGS, 1 },
        { ENABLE_AUTO_POSITION, 1 }, { ENABLE_VIRTUAL_TERMINAL_INPUT, 1 },
    };
    for (size_t k = 0; k < sizeof t / sizeof t[0]; k++) {
        SetLastError(0xBAD);
        BOOL r = SetConsoleMode(hi, t[k].bit);
        CCF((r != 0) == t[k].ok, "bit 0x%lx -> %d (err %lu)", t[k].bit, r, GetLastError());
        if (r) {
            CC(GetConsoleMode(hi, &m));
            DWORD stable = ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT | ENABLE_VIRTUAL_TERMINAL_INPUT;
            CCF((m & stable) == (t[k].bit & stable), "bit 0x%lx read back 0x%lx", t[k].bit, m);
        } else CERR(ERROR_INVALID_PARAMETER);
    }
    SetConsoleMode(hi, orig);
}

CT(con_mode_output_single_bits) {
    CREQ(GetConsoleMode); CREQ(SetConsoleMode);
    HANDLE b = mkbuf_raw(); CNE(b, NULL); if (!b) return;
    DWORD bits[] = { ENABLE_PROCESSED_OUTPUT, ENABLE_WRAP_AT_EOL_OUTPUT, ENABLE_VIRTUAL_TERMINAL_PROCESSING, DISABLE_NEWLINE_AUTO_RETURN, ENABLE_LVB_GRID_WORLDWIDE };
    for (size_t k = 0; k < sizeof bits / sizeof bits[0]; k++) {
        DWORD m = 0xFFFF;
        SetLastError(0xBAD);
        CCF(SetConsoleMode(b, bits[k]), "bit 0x%lx err=%lu", bits[k], GetLastError());
        CC(GetConsoleMode(b, &m));
        CCF((m & 0x1F) == bits[k], "bit 0x%lx read back 0x%lx", bits[k], m);
    }
    CloseHandle(b);
}

CT(con_mode_input_all_combinations_of_basic_bits) {
    CREQ(GetConsoleMode); CREQ(SetConsoleMode);
    HANDLE hi = hin_(); DWORD orig = 0, m;
    GetConsoleMode(hi, &orig);
    int okc = 0, badc = 0;
    for (DWORD v = 0; v < 32; v++) {
        DWORD mode = (v & 1 ? ENABLE_PROCESSED_INPUT : 0) | (v & 2 ? ENABLE_LINE_INPUT : 0) | (v & 4 ? ENABLE_ECHO_INPUT : 0) | (v & 8 ? ENABLE_WINDOW_INPUT : 0) | (v & 16 ? ENABLE_MOUSE_INPUT : 0);
        int expect_ok = !((mode & ENABLE_ECHO_INPUT) && !(mode & ENABLE_LINE_INPUT));
        SetLastError(0xBAD);
        BOOL r = SetConsoleMode(hi, mode);
        CCF((r != 0) == expect_ok, "mode 0x%lx -> %d", mode, r);
        if (r) { okc++; CC(GetConsoleMode(hi, &m)); CCF((m & 0x1F) == mode, "mode 0x%lx read back 0x%lx", mode, m); }
        else { badc++; CERR(ERROR_INVALID_PARAMETER); }
    }
    CEQ(okc, 28); CEQ(badc, 4);
    SetConsoleMode(hi, orig);
}

CT(con_mode_output_all_combinations) {
    CREQ(GetConsoleMode); CREQ(SetConsoleMode);
    HANDLE b = mkbuf_raw(); CNE(b, NULL); if (!b) return;
    for (DWORD v = 0; v < 32; v++) {
        DWORD m = 0xFFFF;
        SetLastError(0xBAD);
        CCF(SetConsoleMode(b, v), "mode 0x%lx err=%lu", v, GetLastError());
        CC(GetConsoleMode(b, &m));
        CCF((m & 0x1F) == v, "mode 0x%lx read back 0x%lx", v, m);
    }
    CloseHandle(b);
}

CT(con_mode_toggle_stress) {
    CREQ(GetConsoleMode); CREQ(SetConsoleMode);
    HANDLE b = mkbuf_raw(); CNE(b, NULL); if (!b) return;
    int bad = 0;
    for (int i = 0; i < 2000; i++) {
        DWORD want = (DWORD)(i % 32), got = 0xFFFF;
        if (!SetConsoleMode(b, want) || !GetConsoleMode(b, &got) || (got & 0x1F) != want) bad++;
    }
    CEQ(bad, 0);
    CloseHandle(b);
}

CT(con_mode_input_output_same_value_different_meaning) {
    CREQ(GetConsoleMode); CREQ(SetConsoleMode);
    HANDLE hi = hin_(), ho = hout_();
    DWORD oi = 0, oo = 0, m;
    GetConsoleMode(hi, &oi); GetConsoleMode(ho, &oo);
    CC(SetConsoleMode(hi, 0x7));            /* processed | line | echo */
    CC(SetConsoleMode(ho, 0x7));            /* processed | wrap | vt */
    CC(GetConsoleMode(hi, &m)); CEQ(m & 0x7, 7);
    CC(GetConsoleMode(ho, &m)); CEQ(m & 0x7, 7);
    CC(SetConsoleMode(ho, 0x3));
    CC(GetConsoleMode(hi, &m)); CEQ(m & 0x7, 7);
    SetConsoleMode(hi, oi); SetConsoleMode(ho, oo);
}

/* ---------------- legacy server calls (observed only) ---------------- */

CT(con_legacy_cursor_mode_calls) {
    CWINESKIP("legacy server calls are unimplemented stubs in the emulator");
    typedef BOOL (WINAPI *g_t)(HANDLE, PUINT, PUINT);
    typedef BOOL (WINAPI *s_t)(HANDLE, BOOL, BOOL);
    g_t g = (g_t)t_proc_k32("GetConsoleCursorMode");
    s_t s = (s_t)t_proc_k32("SetConsoleCursorMode");
    CREQ(g); CREQ(s);
    UINT a = 7, b = 7;
    SetLastError(0xBAD);
    BOOL r = g(hout_(), &a, &b);
    cnote("GetConsoleCursorMode -> %d err=%lu a=%u b=%u", r, GetLastError(), a, b);
    SetLastError(0xBAD);
    r = g(INVALID_HANDLE_VALUE, &a, &b);
    cnote("GetConsoleCursorMode(invalid) -> %d err=%lu", r, GetLastError());
    CEQ(r, 0);
    SetLastError(0xBAD);
    r = s(INVALID_HANDLE_VALUE, FALSE, FALSE);
    CEQ(r, 0);
}

CT(con_legacy_nls_calls) {
    CWINESKIP("legacy server calls are unimplemented stubs in the emulator");
    typedef BOOL (WINAPI *g_t)(HANDLE, PDWORD);
    g_t g = (g_t)t_proc_k32("GetConsoleNlsMode");
    CREQ(g);
    DWORD m = 0xAA;
    SetLastError(0xBAD);
    BOOL r = g(hin_(), &m);
    cnote("GetConsoleNlsMode -> %d err=%lu mode=0x%lx", r, GetLastError(), m);
    SetLastError(0xBAD);
    r = g(INVALID_HANDLE_VALUE, &m);
    CEQ(r, 0);
}

CT(con_legacy_char_type_call) {
    CWINESKIP("legacy server calls are unimplemented stubs in the emulator");
    typedef BOOL (WINAPI *g_t)(HANDLE, COORD, PDWORD);
    g_t g = (g_t)t_proc_k32("GetConsoleCharType");
    CREQ(g);
    HANDLE b = mkbuf_std(); if (!b) return;
    CC(wrw(b, L"a"));
    DWORD t = 0xAA;
    SetLastError(0xBAD);
    BOOL r = g(b, co(0, 0), &t);
    cnote("GetConsoleCharType -> %d err=%lu type=0x%lx", r, GetLastError(), t);
    SetLastError(0xBAD);
    r = g(INVALID_HANDLE_VALUE, co(0, 0), &t);
    CEQ(r, 0);
    CloseHandle(b);
}

CT(con_legacy_hardware_state_calls) {
    CWINESKIP("legacy server calls are unimplemented stubs in the emulator");
    typedef BOOL (WINAPI *g_t)(HANDLE, PDWORD, PDWORD);
    g_t g = (g_t)t_proc_k32("GetConsoleHardwareState");
    CREQ(g);
    DWORD a = 0, b = 0;
    SetLastError(0xBAD);
    BOOL r = g(hout_(), &a, &b);
    cnote("GetConsoleHardwareState -> %d err=%lu", r, GetLastError());
    SetLastError(0xBAD);
    r = g(INVALID_HANDLE_VALUE, &a, &b);
    CEQ(r, 0);
}

CT(con_legacy_menu_close) {
    CWINESKIP("legacy server calls are unimplemented stubs in the emulator");
    typedef BOOL (WINAPI *s_t)(BOOL);
    s_t s = (s_t)t_proc_k32("SetConsoleMenuClose");
    CREQ(s);
    SetLastError(0xBAD);
    BOOL r = s(TRUE);
    cnote("SetConsoleMenuClose(TRUE) -> %d err=%lu", r, GetLastError());
    r = s(TRUE);
    cnote("SetConsoleMenuClose(TRUE) again -> %d", r);
}

CT(con_legacy_last_event_active_does_not_crash) {
    CWINESKIP("legacy server calls are unimplemented stubs in the emulator");
    typedef void (WINAPI *f_t)(void);
    f_t f = (f_t)t_proc_k32("SetLastConsoleEventActive");
    CREQ(f);
    f();
    CC(1);
}

/* ---------------- threads ---------------- */

typedef struct { HANDLE h; WCHAR ch; int ok; } WARG;
static DWORD WINAPI writer(LPVOID p) {
    WARG *a = (WARG *)p;
    for (int i = 0; i < 200; i++) {
        DWORD n = 0;
        if (!WriteConsoleW(a->h, &a->ch, 1, &n, NULL) || n != 1) return 1;
    }
    a->ok = 1;
    return 0;
}

CT(con_threads_concurrent_writes) {
    CREQ(WriteConsoleW);
    HANDLE b = mkbuf_std(); if (!b) return;
    CC(SetConsoleMode(b, ENABLE_WRAP_AT_EOL_OUTPUT));
    WARG a[4]; HANDLE t[4];
    for (int i = 0; i < 4; i++) { a[i].h = b; a[i].ch = L'A' + i; a[i].ok = 0; t[i] = CreateThread(NULL, 0, writer, &a[i], 0, NULL); }
    for (int i = 0; i < 4; i++) { CEQ(WaitForSingleObject(t[i], 20000), WAIT_OBJECT_0); CloseHandle(t[i]); CC(a[i].ok); }
    /* 800 characters at 80 columns without scrolling: 10 full rows, each cell written by exactly one thread */
    int counts[4] = { 0 };
    for (int y = 0; y < 10; y++) for (int x = 0; x < 80; x++) {
        WCHAR c = cellch(b, x, y);
        if (c >= L'A' && c <= L'D') counts[c - L'A']++;
    }
    for (int i = 0; i < 4; i++) CCF(counts[i] == 200, "thread %d wrote %d cells", i, counts[i]);
    CloseHandle(b);
}

static DWORD WINAPI cursor_mover(LPVOID p) {
    HANDLE b = (HANDLE)p;
    for (int i = 0; i < 300; i++) {
        if (!SetConsoleCursorPosition(b, co(i % 80, i % 25))) return 1;
        CONSOLE_SCREEN_BUFFER_INFO s;
        if (!GetConsoleScreenBufferInfo(b, &s)) return 2;
    }
    return 0;
}
CT(con_threads_concurrent_cursor_and_info) {
    CREQ(SetConsoleCursorPosition);
    HANDLE b = mkbuf_std(); if (!b) return;
    HANDLE t[3];
    for (int i = 0; i < 3; i++) t[i] = CreateThread(NULL, 0, cursor_mover, b, 0, NULL);
    for (int i = 0; i < 3; i++) { CEQ(WaitForSingleObject(t[i], 20000), WAIT_OBJECT_0); DWORD c = 9; GetExitCodeThread(t[i], &c); CEQ(c, 0); CloseHandle(t[i]); }
    CloseHandle(b);
}

static DWORD WINAPI input_writer(LPVOID p) {
    for (int i = 0; i < 100; i++) {
        INPUT_RECORD r = keyrec(L'a' + (i % 26), 'A' + (i % 26), (i & 1) == 0, 0); DWORD n = 0;
        if (!WriteConsoleInputW(hin_(), &r, 1, &n)) return 1;
    }
    return 0;
}
CT(con_threads_input_writers_and_reader) {
    CREQ(WriteConsoleInputW); CREQ(ReadConsoleInputW);
    SetConsoleMode(hin_(), ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS);
    inp_reset();
    HANDLE t[3];
    for (int i = 0; i < 3; i++) t[i] = CreateThread(NULL, 0, input_writer, NULL, 0, NULL);
    for (int i = 0; i < 3; i++) { CEQ(WaitForSingleObject(t[i], 20000), WAIT_OBJECT_0); CloseHandle(t[i]); }
    DWORD pending = inp_count();
    cnote("300 events written by 3 threads, pending %lu", pending);
    CC(pending > 0 && pending <= 300);
    static INPUT_RECORD o[400]; DWORD n = 0;
    CC(ReadConsoleInputW(hin_(), o, 400, &n));
    CEQ(n, pending);
}

static DWORD WINAPI blocked_reader(LPVOID p) {
    INPUT_RECORD r; DWORD n = 0;
    BOOL ok = ReadConsoleInputW(hin_(), &r, 1, &n);
    return ok ? (100 + (r.Event.KeyEvent.uChar.UnicodeChar & 0x7F)) : 1;
}
CT(con_threads_blocked_reader_woken_by_other_thread) {
    CREQ(ReadConsoleInputW); CREQ(WriteConsoleInputW);
    inp_reset();
    HANDLE t = CreateThread(NULL, 0, blocked_reader, NULL, 0, NULL);
    Sleep(300);
    CEQ(WaitForSingleObject(t, 0), WAIT_TIMEOUT);
    INPUT_RECORD r = keyrec(L'Z', 'Z', TRUE, 0); DWORD n = 0;
    CC(WriteConsoleInputW(hin_(), &r, 1, &n));
    CEQ(WaitForSingleObject(t, 5000), WAIT_OBJECT_0);
    DWORD c = 0; GetExitCodeThread(t, &c);
    CEQ(c, 100 + 'Z');
    CloseHandle(t);
}

CT(con_threads_flush_does_not_wake_reader) {
    CREQ(ReadConsoleInputW); CREQ(FlushConsoleInputBuffer);
    inp_reset();
    HANDLE t = CreateThread(NULL, 0, blocked_reader, NULL, 0, NULL);
    Sleep(200);
    CC(FlushConsoleInputBuffer(hin_()));
    CEQ(WaitForSingleObject(t, 300), WAIT_TIMEOUT);
    INPUT_RECORD r = keyrec(L'F', 'F', TRUE, 0); DWORD n = 0;
    WriteConsoleInputW(hin_(), &r, 1, &n);
    CEQ(WaitForSingleObject(t, 5000), WAIT_OBJECT_0);
    CloseHandle(t);
}

static DWORD WINAPI title_reader(LPVOID p) {
    WCHAR g[64];
    GetConsoleTitleW(g, 64);
    return lstrcmpW(g, L"main-thread-title") == 0 ? 0 : 1;
}
CT(con_threads_title_from_thread) {
    CREQ(SetConsoleTitleW); CREQ(GetConsoleTitleW);
    CC(SetConsoleTitleW(L"main-thread-title"));
    WCHAR b[64];
    HANDLE t = CreateThread(NULL, 0, title_reader, NULL, 0, NULL);
    CEQ(WaitForSingleObject(t, 5000), WAIT_OBJECT_0);
    DWORD c = 9; GetExitCodeThread(t, &c); CEQ(c, 0);
    CloseHandle(t);
    GetConsoleTitleW(b, 64);
    CWSTR(b, L"main-thread-title");
}
