/* Debugger API tests against our own debuggee processes (re-launches of this exe). */
#include "eed.h"
#include "eed_asm.h"

static volatile DWORD g_hw_var2;
static void WINAPI hw_dummy(void) {}

/* ================= debuggee children ================= */
CHILD(dbg_exit7) { return 7; }
CHILD(dbg_outstr) {
    OutputDebugStringA("k32t-A-string");
    OutputDebugStringW(L"k32t-W-string");
    return 5;
}
static DWORD WINAPI dbg_thr(LPVOID p) { return 3; }
CHILD(dbg_thread) {
    HANDLE h = CreateThread(NULL, 0, dbg_thr, NULL, 0, NULL);
    WaitForSingleObject(h, 10000);
    CloseHandle(h);
    return 8;
}
CHILD(dbg_dll) {
    HMODULE m = LoadLibraryA("dbghelp.dll");
    if (m) FreeLibrary(m);
    return m ? 9 : 10;
}
CHILD(dbg_bp) { DebugBreak(); return 9; }
CHILD(dbg_raise) { ULONG_PTR a[2] = { 0x11, 0x22 }; RaiseException(0xE0A0DB01, 0, 2, a); return 0x21; }
CHILD(dbg_av) { eed_av_write((char *)0); return 0x31; }
CHILD(dbg_idle) { Sleep(25000); return 0; }
CHILD(dbg_hwwrite) { eed_write_dword(&g_hw_var2); return g_hw_var2 == 0x77 ? 0 : 1; }
CHILD(dbg_spawner) {
    PROCESS_INFORMATION pi; STARTUPINFOA si; char cmd[MAX_PATH + 64];
    memset(&si, 0, sizeof si); si.cb = sizeof si;
    snprintf(cmd, sizeof cmd, "\"%s\" --child dbg_exit7", t_self_path());
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) return 100;
    WaitForSingleObject(pi.hProcess, 15000);
    DWORD c = 0; GetExitCodeProcess(pi.hProcess, &c);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return c == 7 ? 0 : 101;
}
/* debugger-in-a-child: debugs an idle grandchild, publishes its pid, lingers, then exits */
CHILD(dbg_parent) {
    const char *pidfile = argv[1]; int kill_on_exit = atoi(argv[2]);
    BOOL (WINAPI *wait)(LPDEBUG_EVENT, DWORD) = (void *)t_proc("WaitForDebugEvent");
    BOOL (WINAPI *cont)(DWORD, DWORD, DWORD) = (void *)t_proc("ContinueDebugEvent");
    BOOL (WINAPI *koe)(BOOL) = (void *)t_proc("DebugSetProcessKillOnExit");
    PROCESS_INFORMATION pi; STARTUPINFOA si; char cmd[MAX_PATH + 64];
    memset(&si, 0, sizeof si); si.cb = sizeof si;
    snprintf(cmd, sizeof cmd, "\"%s\" --child dbg_idle", t_self_path());
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, DEBUG_ONLY_THIS_PROCESS | CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) return 100;
    if (!kill_on_exit) koe(FALSE);
    DWORD t0 = GetTickCount(); int published = 0; DWORD tpub = 0;
    while (GetTickCount() - t0 < 20000) {
        DEBUG_EVENT e;
        if (!wait(&e, 200)) { if (published && GetTickCount() - tpub > 2500) break; continue; }
        DWORD st = DBG_CONTINUE;
        if (e.dwDebugEventCode == EXCEPTION_DEBUG_EVENT && e.u.Exception.ExceptionRecord.ExceptionCode != EXCEPTION_BREAKPOINT) st = DBG_EXCEPTION_NOT_HANDLED;
        if (e.dwDebugEventCode == EXCEPTION_DEBUG_EVENT && e.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_BREAKPOINT && !published) {
            char b[32]; DWORD w; snprintf(b, sizeof b, "%lu", pi.dwProcessId);
            HANDLE f = CreateFileA(pidfile, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
            WriteFile(f, b, (DWORD)strlen(b), &w, NULL); CloseHandle(f);
            published = 1; tpub = GetTickCount();
        }
        cont(e.dwProcessId, e.dwThreadId, st);
    }
    return 0;
}

/* ================= debugger session helper ================= */
typedef struct {
    DWORD code, pid, tid;
    DWORD exc, first, nparams; ULONG_PTR addr, info[4];
    BOOL uni; DWORD slen; char str[260]; WCHAR wstr[260];
    char dll[MAX_PATH]; PVOID base, start; DWORD exitcode; HANDLE hthread;
} ev_t;
struct dsess;
typedef DWORD (*dcb_t)(struct dsess *, DEBUG_EVENT *, ev_t *);   /* returns continue status, or 0 for default */
typedef struct dsess {
    PROCESS_INFORMATION pi; ev_t ev[600]; int n; int exited; int stop; int use_ex;
    dcb_t cb; void *user; PVOID cbase; int bp_count; int ss_count; DWORD hits;
} dsess;

#define NEED_DBG() REQUIRE(U(WaitForDebugEvent)); REQUIRE(U(ContinueDebugEvent)); REQUIRE(U(ReadProcessMemory))
static PVOID child_addr(dsess *s, const void *local) { return (PVOID)((ULONG_PTR)s->cbase + ((ULONG_PTR)local - (ULONG_PTR)GetModuleHandleA(NULL))); }
static int ends_with_i(const char *s, const char *suf) { size_t a = strlen(s), b = strlen(suf); return a >= b && !_stricmp(s + a - b, suf); }

static int ev_find(dsess *s, DWORD code, int from) { for (int i = from; i < s->n; i++) if (s->ev[i].code == code) return i; return -1; }
static int ev_count(dsess *s, DWORD code) { int c = 0; for (int i = 0; i < s->n; i++) if (s->ev[i].code == code) c++; return c; }
static int ev_find_dll(dsess *s, const char *name) { for (int i = 0; i < s->n; i++) if (s->ev[i].code == LOAD_DLL_DEBUG_EVENT && ends_with_i(s->ev[i].dll, name)) return i; return -1; }
static int ev_find_exc(dsess *s, DWORD exc, int first, int from) { for (int i = from; i < s->n; i++) if (s->ev[i].code == EXCEPTION_DEBUG_EVENT && s->ev[i].exc == exc && (int)s->ev[i].first == first) return i; return -1; }

static void record(dsess *s, DEBUG_EVENT *e, ev_t *r) {
    memset(r, 0, sizeof *r);
    r->code = e->dwDebugEventCode; r->pid = e->dwProcessId; r->tid = e->dwThreadId;
    switch (e->dwDebugEventCode) {
    case EXCEPTION_DEBUG_EVENT: {
        EXCEPTION_RECORD *x = &e->u.Exception.ExceptionRecord;
        r->exc = x->ExceptionCode; r->first = e->u.Exception.dwFirstChance; r->addr = (ULONG_PTR)x->ExceptionAddress; r->nparams = x->NumberParameters;
        for (int i = 0; i < 4; i++) r->info[i] = x->ExceptionInformation[i];
        break; }
    case CREATE_THREAD_DEBUG_EVENT: r->start = (PVOID)e->u.CreateThread.lpStartAddress; r->hthread = e->u.CreateThread.hThread; break;
    case CREATE_PROCESS_DEBUG_EVENT:
        r->base = e->u.CreateProcessInfo.lpBaseOfImage; r->start = (PVOID)e->u.CreateProcessInfo.lpStartAddress; r->hthread = e->u.CreateProcessInfo.hThread;
        if (e->u.CreateProcessInfo.hFile && e->u.CreateProcessInfo.hFile != INVALID_HANDLE_VALUE) {
            GetFinalPathNameByHandleA(e->u.CreateProcessInfo.hFile, r->dll, MAX_PATH, 0);
            CloseHandle(e->u.CreateProcessInfo.hFile);
        }
        break;
    case EXIT_THREAD_DEBUG_EVENT: r->exitcode = e->u.ExitThread.dwExitCode; break;
    case EXIT_PROCESS_DEBUG_EVENT: r->exitcode = e->u.ExitProcess.dwExitCode; break;
    case LOAD_DLL_DEBUG_EVENT:
        r->base = e->u.LoadDll.lpBaseOfDll;
        if (e->u.LoadDll.hFile && e->u.LoadDll.hFile != INVALID_HANDLE_VALUE) {
            GetFinalPathNameByHandleA(e->u.LoadDll.hFile, r->dll, MAX_PATH, 0);
            CloseHandle(e->u.LoadDll.hFile);
        }
        break;
    case UNLOAD_DLL_DEBUG_EVENT: r->base = e->u.UnloadDll.lpBaseOfDll; break;
    case OUTPUT_DEBUG_STRING_EVENT: {
        r->uni = e->u.DebugString.fUnicode; r->slen = e->u.DebugString.nDebugStringLength;
        SIZE_T n = r->slen * (r->uni ? 2 : 1); if (n > 256) n = 256; SIZE_T got = 0;
        if (r->uni) ReadProcessMemory(s->pi.hProcess, e->u.DebugString.lpDebugStringData, r->wstr, n, &got);
        else ReadProcessMemory(s->pi.hProcess, e->u.DebugString.lpDebugStringData, r->str, n, &got);
        break; }
    }
}
/* Pump events until the debuggee exits, s->stop is set, or the time runs out. */
static int dbg_run(dsess *s, DWORD total_ms) {
    DWORD t0 = GetTickCount();
    while (!s->exited && !s->stop && GetTickCount() - t0 < total_ms) {
        DEBUG_EVENT e; memset(&e, 0, sizeof e);
        BOOL ok = s->use_ex ? U(WaitForDebugEventEx)(&e, 500) : U(WaitForDebugEvent)(&e, 500);
        if (!ok) { if (GetLastError() != ERROR_SEM_TIMEOUT) { t_note("WaitForDebugEvent failed err=%lu", GetLastError()); break; } continue; }
        if (s->n >= 600) { U(ContinueDebugEvent)(e.dwProcessId, e.dwThreadId, DBG_CONTINUE); continue; }
        ev_t *r = &s->ev[s->n++];
        record(s, &e, r);
        if (r->code == CREATE_PROCESS_DEBUG_EVENT && r->pid == s->pi.dwProcessId) s->cbase = r->base;
        DWORD st = DBG_CONTINUE;
        if (e.dwDebugEventCode == EXCEPTION_DEBUG_EVENT) {
            if (e.dwProcessId == s->pi.dwProcessId && r->exc == EXCEPTION_BREAKPOINT && r->first) s->bp_count++;
            st = DBG_EXCEPTION_NOT_HANDLED;
            if (r->exc == EXCEPTION_BREAKPOINT && r->first && s->bp_count == 1) st = DBG_CONTINUE;   /* loader breakpoint */
        }
        if (s->cb) { DWORD x = s->cb(s, &e, r); if (x) st = x; }
        if (e.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT && e.dwProcessId == s->pi.dwProcessId) s->exited = 1;
        U(ContinueDebugEvent)(e.dwProcessId, e.dwThreadId, st);
    }
    return s->exited;
}
static int dbg_start(dsess *s, const char *child, const char *args, DWORD flags, dcb_t cb, int use_ex) {
    memset(s, 0, sizeof *s);
    HANDLE h = t_spawn_child(child, args, flags | CREATE_NO_WINDOW, NULL, NULL, NULL, FALSE, NULL, &s->pi);
    if (!h) return 0;
    s->cb = cb; s->use_ex = use_ex;
    return 1;
}
/* Kill the debuggee and consume every remaining debug event so later tests start clean. */
static void dbg_finish(dsess *s) {
    if (!s->pi.hProcess) return;
    if (WaitForSingleObject(s->pi.hProcess, 0) != WAIT_OBJECT_0) TerminateProcess(s->pi.hProcess, 0xDEAD);
    DWORD t0 = GetTickCount();
    while (GetTickCount() - t0 < 6000) {
        DEBUG_EVENT e;
        if (!U(WaitForDebugEvent)(&e, 700)) { if (WaitForSingleObject(s->pi.hProcess, 0) == WAIT_OBJECT_0) break; continue; }
        if (e.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT && e.u.LoadDll.hFile) CloseHandle(e.u.LoadDll.hFile);
        if (e.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT && e.u.CreateProcessInfo.hFile) CloseHandle(e.u.CreateProcessInfo.hFile);
        U(ContinueDebugEvent)(e.dwProcessId, e.dwThreadId, DBG_CONTINUE);
        if (e.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT && e.dwProcessId == s->pi.dwProcessId) { /* keep draining briefly for other pids */ }
    }
    WaitForSingleObject(s->pi.hProcess, 5000);
    CloseHandle(s->pi.hThread); CloseHandle(s->pi.hProcess);
    memset(&s->pi, 0, sizeof s->pi);
}
#define DBG_FLAGS DEBUG_ONLY_THIS_PROCESS

/* ================= tests ================= */
TEST(eed_dbg_exports) {
    CHECK(UK(WaitForDebugEvent) != NULL); CHECK(UK(WaitForDebugEventEx) != NULL); CHECK(UK(ContinueDebugEvent) != NULL);
    CHECK(UK(DebugActiveProcess) != NULL); CHECK(UK(DebugActiveProcessStop) != NULL); CHECK(UK(DebugSetProcessKillOnExit) != NULL);
    CHECK(UK(DebugBreakProcess) != NULL);
    CHECK(UB(WaitForDebugEvent) != NULL); CHECK(UB(WaitForDebugEventEx) != NULL);
}
TEST(eed_dbg_event_stream_basic) {
    NEED_DBG();
    dsess s; CHECK(dbg_start(&s, "dbg_exit7", "", DBG_FLAGS, NULL, 0)); if (!s.pi.hProcess) return;
    int done = dbg_run(&s, 20000);
    EXPECT_EQ(done, 1);
    CHECK(s.n >= 5);
    if (s.n) {
        EXPECT_EQ(s.ev[0].code, CREATE_PROCESS_DEBUG_EVENT);
        EXPECT_EQ(s.ev[0].pid, s.pi.dwProcessId);
        EXPECT_EQ(s.ev[0].tid, s.pi.dwThreadId);
        CHECK(s.ev[0].base != NULL);
        CHECK(s.ev[0].start != NULL);
        CHECK(ends_with_i(s.ev[0].dll, ".exe"));
        EXPECT_EQ(s.ev[s.n - 1].code, EXIT_PROCESS_DEBUG_EVENT);
        EXPECT_EQ(s.ev[s.n - 1].exitcode, 7);
    }
    for (int i = 0; i < s.n; i++) if (s.ev[i].pid != s.pi.dwProcessId) { CHECKF(0, "event %d from other pid %lu", i, s.ev[i].pid); break; }
    dbg_finish(&s);
}
TEST(eed_dbg_process_start_address_is_entry_point) {
    NEED_DBG();
    dsess s; dbg_start(&s, "dbg_exit7", "", DBG_FLAGS, NULL, 0); if (!s.pi.hProcess) return;
    dbg_run(&s, 20000);
    if (s.n) {
        PIMAGE_DOS_HEADER dh = (PIMAGE_DOS_HEADER)GetModuleHandleA(NULL);
        PIMAGE_NT_HEADERS nh = (PIMAGE_NT_HEADERS)((char *)dh + dh->e_lfanew);
        ULONG_PTR want = (ULONG_PTR)s.ev[0].base + nh->OptionalHeader.AddressOfEntryPoint;
        EXPECT_EQ((ULONG_PTR)s.ev[0].start, want);
        EXPECT_EQ((ULONG_PTR)s.ev[0].base, (ULONG_PTR)s.ev[0].base & ~(ULONG_PTR)0xFFFF);
    }
    dbg_finish(&s);
}
TEST(eed_dbg_loads_system_dlls_before_loader_breakpoint) {
    NEED_DBG();
    dsess s; dbg_start(&s, "dbg_exit7", "", DBG_FLAGS, NULL, 0); if (!s.pi.hProcess) return;
    dbg_run(&s, 20000);
    int bp = ev_find_exc(&s, EXCEPTION_BREAKPOINT, 1, 0);
    CHECK(bp > 0);
    int nt = ev_find_dll(&s, "ntdll.dll"), k32 = ev_find_dll(&s, "kernel32.dll"), kb = ev_find_dll(&s, "kernelbase.dll");
    CHECK(nt > 0); CHECK(k32 > 0); CHECK(kb > 0);
    CHECK(nt < bp); CHECK(k32 < bp); CHECK(kb < bp);
    if (nt > 0 && k32 > 0) CHECK(nt < k32);
    dbg_finish(&s);
}
TEST(eed_dbg_dll_base_addresses_match_our_process) {
    NEED_DBG();
    dsess s; dbg_start(&s, "dbg_exit7", "", DBG_FLAGS, NULL, 0); if (!s.pi.hProcess) return;
    dbg_run(&s, 20000);
    int nt = ev_find_dll(&s, "ntdll.dll"), k32 = ev_find_dll(&s, "kernel32.dll");
    if (nt >= 0) EXPECT_EQ(s.ev[nt].base, t_ntdll());
    if (k32 >= 0) EXPECT_EQ(s.ev[k32].base, t_kernel32());
    dbg_finish(&s);
}
TEST(eed_dbg_loader_breakpoint) {
    NEED_DBG();
    dsess s; dbg_start(&s, "dbg_exit7", "", DBG_FLAGS, NULL, 0); if (!s.pi.hProcess) return;
    dbg_run(&s, 20000);
    int bp = ev_find_exc(&s, EXCEPTION_BREAKPOINT, 1, 0);
    CHECK(bp >= 0);
    if (bp >= 0) {
        EXPECT_EQ(s.ev[bp].tid, s.pi.dwThreadId);
        CHECKF(s.ev[bp].nparams >= 1, "nparams %lu", s.ev[bp].nparams);
        HMODULE m = NULL;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)s.ev[bp].addr, &m);
        CHECKF(m == t_ntdll(), "loader breakpoint at %p not in ntdll", (void *)s.ev[bp].addr);
    }
    EXPECT_EQ(ev_count(&s, EXCEPTION_DEBUG_EVENT), 1);
    dbg_finish(&s);
}
TEST(eed_dbg_exit_code_and_last_events) {
    NEED_DBG();
    dsess s; dbg_start(&s, "dbg_exit7", "", DBG_FLAGS, NULL, 0); if (!s.pi.hProcess) return;
    dbg_run(&s, 20000);
    EXPECT_EQ(ev_count(&s, EXIT_PROCESS_DEBUG_EVENT), 1);
    EXPECT_EQ(ev_count(&s, CREATE_PROCESS_DEBUG_EVENT), 1);
    int ex = ev_find(&s, EXIT_PROCESS_DEBUG_EVENT, 0);
    if (ex >= 0) { EXPECT_EQ(ex, s.n - 1); EXPECT_EQ(s.ev[ex].tid, s.pi.dwThreadId); }
    DWORD code = 0; WaitForSingleObject(s.pi.hProcess, 5000); GetExitCodeProcess(s.pi.hProcess, &code);
    EXPECT_EQ(code, 7);
    dbg_finish(&s);
}
static DWORD cb_nothing(dsess *s, DEBUG_EVENT *e, ev_t *r) { return 0; }
TEST(eed_dbg_output_debug_string_a) {
    NEED_DBG();
    dsess s; dbg_start(&s, "dbg_outstr", "", DBG_FLAGS, cb_nothing, 0); if (!s.pi.hProcess) return;
    dbg_run(&s, 20000);
    int i = ev_find(&s, OUTPUT_DEBUG_STRING_EVENT, 0);
    CHECK(i >= 0);
    if (i >= 0) {
        EXPECT_EQ(s.ev[i].uni, 0);
        EXPECT_STR(s.ev[i].str, "k32t-A-string");
        EXPECT_EQ(s.ev[i].slen, strlen("k32t-A-string") + 1);
        EXPECT_EQ(s.ev[i].tid, s.pi.dwThreadId);
    }
    int j = i >= 0 ? ev_find(&s, OUTPUT_DEBUG_STRING_EVENT, i + 1) : -1;
    CHECK(j >= 0);
    EXPECT_EQ(ev_count(&s, OUTPUT_DEBUG_STRING_EVENT), 2);
    dbg_finish(&s);
}
TEST(eed_dbg_output_debug_string_w_ansi_conversion) {
    NEED_DBG();
    /* plain WaitForDebugEvent reports OutputDebugStringW text as ANSI */
    dsess s; dbg_start(&s, "dbg_outstr", "", DBG_FLAGS, cb_nothing, 0); if (!s.pi.hProcess) return;
    dbg_run(&s, 20000);
    int i = ev_find(&s, OUTPUT_DEBUG_STRING_EVENT, 0);
    int j = i >= 0 ? ev_find(&s, OUTPUT_DEBUG_STRING_EVENT, i + 1) : -1;
    CHECK(j >= 0);
    if (j >= 0) {
        EXPECT_EQ(s.ev[j].uni, 0);
        EXPECT_STR(s.ev[j].str, "k32t-W-string");
    }
    dbg_finish(&s);
}
TEST(eed_dbg_output_debug_string_wide_with_ex) {
    NEED_DBG(); REQUIRE(U(WaitForDebugEventEx));
    dsess s; dbg_start(&s, "dbg_outstr", "", DBG_FLAGS, cb_nothing, 1); if (!s.pi.hProcess) return;
    dbg_run(&s, 20000);
    int i = ev_find(&s, OUTPUT_DEBUG_STRING_EVENT, 0);
    int j = i >= 0 ? ev_find(&s, OUTPUT_DEBUG_STRING_EVENT, i + 1) : -1;
    CHECK(i >= 0 && j >= 0);
    if (i >= 0) { EXPECT_EQ(s.ev[i].uni, 0); EXPECT_STR(s.ev[i].str, "k32t-A-string"); }
    if (j >= 0) {
        EXPECT_EQ(s.ev[j].uni, 1);
        EXPECT_WSTR(s.ev[j].wstr, L"k32t-W-string");
        EXPECT_EQ(s.ev[j].slen, wcslen(L"k32t-W-string") + 1);
    }
    dbg_finish(&s);
}
TEST(eed_dbg_ex_variant_event_stream_same_as_plain) {
    NEED_DBG(); REQUIRE(U(WaitForDebugEventEx));
    dsess a, b;
    dbg_start(&a, "dbg_exit7", "", DBG_FLAGS, NULL, 0); dbg_run(&a, 20000); dbg_finish(&a);
    dbg_start(&b, "dbg_exit7", "", DBG_FLAGS, NULL, 1); dbg_run(&b, 20000); dbg_finish(&b);
    CHECKF(a.n > 0 && b.n > 0, "n=%d/%d", a.n, b.n);
    EXPECT_EQ(ev_count(&a, LOAD_DLL_DEBUG_EVENT) > 0, ev_count(&b, LOAD_DLL_DEBUG_EVENT) > 0);
    EXPECT_EQ(a.ev[0].code, b.ev[0].code);
    EXPECT_EQ(b.ev[b.n - 1].code, EXIT_PROCESS_DEBUG_EVENT);
}
TEST(eed_dbg_thread_create_and_exit_events) {
    NEED_DBG();
    dsess s; dbg_start(&s, "dbg_thread", "", DBG_FLAGS, cb_nothing, 0); if (!s.pi.hProcess) return;
    dbg_run(&s, 20000);
    /* the loader may create its own worker threads: find ours by start address */
    int c = -1;
    for (int i = 0; i < s.n; i++) if (s.ev[i].code == CREATE_THREAD_DEBUG_EVENT && (ULONG_PTR)s.ev[i].start == (ULONG_PTR)child_addr(&s, (void *)dbg_thr)) c = i;
    CHECKF(c >= 0, "no CREATE_THREAD event with the expected start address (%d create-thread events)", ev_count(&s, CREATE_THREAD_DEBUG_EVENT));
    if (c >= 0) {
        CHECK(s.ev[c].tid != s.pi.dwThreadId);
        CHECK(s.ev[c].hthread != NULL);
        int x = -1;
        for (int i = c + 1; i < s.n; i++) if (s.ev[i].code == EXIT_THREAD_DEBUG_EVENT && s.ev[i].tid == s.ev[c].tid) x = i;
        CHECK(x > c);
        if (x > c) EXPECT_EQ(s.ev[x].exitcode, 3);
    }
    EXPECT_EQ(s.ev[s.n - 1].code, EXIT_PROCESS_DEBUG_EVENT);
    EXPECT_EQ(s.ev[s.n - 1].exitcode, 8);
    dbg_finish(&s);
}
TEST(eed_dbg_dll_load_and_unload_events) {
    NEED_DBG();
    dsess s; dbg_start(&s, "dbg_dll", "", DBG_FLAGS, cb_nothing, 0); if (!s.pi.hProcess) return;
    dbg_run(&s, 20000);
    int l = ev_find_dll(&s, "dbghelp.dll");
    CHECK(l >= 0);
    if (l >= 0) {
        int u = -1;
        for (int i = l + 1; i < s.n; i++) if (s.ev[i].code == UNLOAD_DLL_DEBUG_EVENT && s.ev[i].base == s.ev[l].base) u = i;
        CHECK(u > l);
        CHECK(((ULONG_PTR)s.ev[l].base & 0xFFFF) == 0);
    }
    EXPECT_EQ(s.ev[s.n - 1].exitcode, 9);
    dbg_finish(&s);
}

/* ---- exceptions and continue statuses ---- */
static DWORD cb_bp_continue(dsess *s, DEBUG_EVENT *e, ev_t *r) {
    return (r->code == EXCEPTION_DEBUG_EVENT && r->exc == EXCEPTION_BREAKPOINT) ? DBG_CONTINUE : 0;
}
TEST(eed_dbg_debugbreak_continue) {
    NEED_DBG();
    dsess s; dbg_start(&s, "dbg_bp", "", DBG_FLAGS, cb_bp_continue, 0); if (!s.pi.hProcess) return;
    dbg_run(&s, 20000);
    EXPECT_EQ(s.bp_count, 2);                      /* loader breakpoint + DebugBreak() */
    int b2 = ev_find_exc(&s, EXCEPTION_BREAKPOINT, 1, 0);
    b2 = b2 >= 0 ? ev_find_exc(&s, EXCEPTION_BREAKPOINT, 1, b2 + 1) : -1;
    CHECK(b2 >= 0);
    EXPECT_EQ(s.ev[s.n - 1].code, EXIT_PROCESS_DEBUG_EVENT);
    EXPECT_EQ(s.ev[s.n - 1].exitcode, 9);
    dbg_finish(&s);
}
TEST(eed_dbg_debugbreak_not_handled_gives_second_chance) {
    NEED_DBG();
    dsess s; dbg_start(&s, "dbg_bp", "", DBG_FLAGS, NULL, 0); if (!s.pi.hProcess) return;
    dbg_run(&s, 20000);
    /* default policy answers DBG_EXCEPTION_NOT_HANDLED to the second breakpoint */
    int b1 = ev_find_exc(&s, EXCEPTION_BREAKPOINT, 1, 0);
    int b2 = b1 >= 0 ? ev_find_exc(&s, EXCEPTION_BREAKPOINT, 1, b1 + 1) : -1;
    CHECK(b2 >= 0);
    int last = ev_find_exc(&s, EXCEPTION_BREAKPOINT, 0, b2 >= 0 ? b2 + 1 : 0);
    CHECK(last > b2);
    EXPECT_EQ(s.ev[s.n - 1].code, EXIT_PROCESS_DEBUG_EVENT);
    EXPECT_EQ(s.ev[s.n - 1].exitcode, 0x80000003);
    dbg_finish(&s);
}
static DWORD cb_not_handled(dsess *s, DEBUG_EVENT *e, ev_t *r) {
    if (r->code == EXCEPTION_DEBUG_EVENT && r->exc == EXCEPTION_BREAKPOINT && s->bp_count >= 2) return DBG_EXCEPTION_NOT_HANDLED;
    return 0;
}
TEST(eed_dbg_exception_not_handled_second_chance_and_exit_code) {
    NEED_DBG();
    dsess s; dbg_start(&s, "dbg_raise", "", DBG_FLAGS, cb_not_handled, 0); if (!s.pi.hProcess) return;
    dbg_run(&s, 20000);
    int f = ev_find_exc(&s, 0xE0A0DB01, 1, 0);
    int l = ev_find_exc(&s, 0xE0A0DB01, 0, 0);
    CHECK(f >= 0); CHECK(l > f);
    if (f >= 0) {
        EXPECT_EQ(s.ev[f].nparams, 2);
        EXPECT_EQ(s.ev[f].info[0], 0x11); EXPECT_EQ(s.ev[f].info[1], 0x22);
        CHECK((ULONG_PTR)s.ev[f].addr > (ULONG_PTR)UB(RaiseException) && (ULONG_PTR)s.ev[f].addr < (ULONG_PTR)UB(RaiseException) + 0x100);   /* inside RaiseException (RtlRaiseException rewrites ExceptionAddress) */
        EXPECT_EQ(s.ev[f].tid, s.pi.dwThreadId);
    }
    EXPECT_EQ(s.ev[s.n - 1].code, EXIT_PROCESS_DEBUG_EVENT);
    EXPECT_EQ(s.ev[s.n - 1].exitcode, 0xE0A0DB01);
    dbg_finish(&s);
}
static DWORD cb_handled(dsess *s, DEBUG_EVENT *e, ev_t *r) {
    if (r->code == EXCEPTION_DEBUG_EVENT && r->exc == 0xE0A0DB01) return DBG_CONTINUE;
    return 0;
}
TEST(eed_dbg_exception_continue_resumes_debuggee) {
    NEED_DBG();
    dsess s; dbg_start(&s, "dbg_raise", "", DBG_FLAGS, cb_handled, 0); if (!s.pi.hProcess) return;
    dbg_run(&s, 20000);
    EXPECT_EQ(ev_count(&s, EXCEPTION_DEBUG_EVENT), 2);          /* loader breakpoint + one first-chance */
    EXPECT_EQ(s.ev[s.n - 1].exitcode, 0x21);
    dbg_finish(&s);
}
static DWORD cb_skip_av(dsess *s, DEBUG_EVENT *e, ev_t *r) {
    if (r->code != EXCEPTION_DEBUG_EVENT || r->exc != EXCEPTION_ACCESS_VIOLATION) return 0;
    CONTEXT c; memset(&c, 0, sizeof c); c.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    s->hits++;
    if (U(GetThreadContext)(s->pi.hThread, &c)) {
        if (c.Rip == (DWORD64)(ULONG_PTR)r->addr) s->ss_count = 1;
        c.Rip = (DWORD64)(ULONG_PTR)child_addr(s, eed_av_write_after);
        U(SetThreadContext)(s->pi.hThread, &c);
    }
    return DBG_CONTINUE;
}
TEST(eed_dbg_av_record_and_setthreadcontext_skip) {
    NEED_DBG(); REQUIRE(U(GetThreadContext)); REQUIRE(U(SetThreadContext));
    dsess s; dbg_start(&s, "dbg_av", "", DBG_FLAGS, cb_skip_av, 0); if (!s.pi.hProcess) return;
    dbg_run(&s, 20000);
    int a = ev_find_exc(&s, EXCEPTION_ACCESS_VIOLATION, 1, 0);
    CHECK(a >= 0);
    if (a >= 0) {
        EXPECT_EQ(s.ev[a].nparams, 2);
        EXPECT_EQ(s.ev[a].info[0], 1); EXPECT_EQ(s.ev[a].info[1], 0);
        EXPECT_EQ((ULONG_PTR)s.ev[a].addr, (ULONG_PTR)child_addr(&s, (void *)eed_av_write));
    }
    EXPECT_EQ(s.hits, 1);
    EXPECT_EQ(s.ss_count, 1);                                  /* context Rip equalled the faulting address */
    EXPECT_EQ(s.ev[s.n - 1].exitcode, 0x31);
    dbg_finish(&s);
}
static DWORD cb_bp_ctx(dsess *s, DEBUG_EVENT *e, ev_t *r) {
    if (r->code != EXCEPTION_DEBUG_EVENT || r->exc != EXCEPTION_BREAKPOINT || s->bp_count != 2) return 0;
    CONTEXT c; memset(&c, 0, sizeof c); c.ContextFlags = CONTEXT_CONTROL;
    if (U(GetThreadContext)(s->pi.hThread, &c)) {
        s->user = (void *)(ULONG_PTR)c.Rip;
        BYTE b = 0; SIZE_T got = 0;
        ReadProcessMemory(s->pi.hProcess, (LPCVOID)r->addr, &b, 1, &got);
        s->hits = b;
    }
    return DBG_CONTINUE;
}
TEST(eed_dbg_breakpoint_context_rip_after_int3) {
    NEED_DBG(); REQUIRE(U(GetThreadContext));
    dsess s; dbg_start(&s, "dbg_bp", "", DBG_FLAGS, cb_bp_ctx, 0); if (!s.pi.hProcess) return;
    dbg_run(&s, 20000);
    int b2 = ev_find_exc(&s, EXCEPTION_BREAKPOINT, 1, 0);
    b2 = b2 >= 0 ? ev_find_exc(&s, EXCEPTION_BREAKPOINT, 1, b2 + 1) : -1;
    CHECK(b2 >= 0);
    if (b2 >= 0) {
        EXPECT_EQ((ULONG_PTR)s.user, (ULONG_PTR)s.ev[b2].addr + 1);
        EXPECT_EQ(s.hits, 0xCC);
        EXPECT_EQ((ULONG_PTR)s.ev[b2].addr, (ULONG_PTR)GetProcAddress(t_ntdll(), "DbgBreakPoint"));
    }
    dbg_finish(&s);
}
static DWORD cb_single_step(dsess *s, DEBUG_EVENT *e, ev_t *r) {
    if (r->code != EXCEPTION_DEBUG_EVENT) return 0;
    CONTEXT c; memset(&c, 0, sizeof c); c.ContextFlags = CONTEXT_CONTROL;
    if (r->exc == EXCEPTION_BREAKPOINT && s->bp_count == 1) {
        if (U(GetThreadContext)(s->pi.hThread, &c)) { c.EFlags |= 0x100; U(SetThreadContext)(s->pi.hThread, &c); }
        return DBG_CONTINUE;
    }
    if (r->exc == EXCEPTION_SINGLE_STEP) {
        s->ss_count++;
        if (U(GetThreadContext)(s->pi.hThread, &c)) {
            if (s->ss_count < 25) c.EFlags |= 0x100; else c.EFlags &= ~0x100u;
            U(SetThreadContext)(s->pi.hThread, &c);
        }
        return DBG_CONTINUE;
    }
    return 0;
}
TEST(eed_dbg_single_step_via_trap_flag) {
    NEED_DBG(); REQUIRE(U(GetThreadContext)); REQUIRE(U(SetThreadContext));
    if (t_is_wine()) { t_skip("trap flag in debuggee unreliable under wine"); return; }
    dsess s; dbg_start(&s, "dbg_exit7", "", DBG_FLAGS, cb_single_step, 0); if (!s.pi.hProcess) return;
    dbg_run(&s, 30000);
    EXPECT_EQ(s.ss_count, 25);
    int n = 0; for (int i = 0; i < s.n; i++) if (s.ev[i].code == EXCEPTION_DEBUG_EVENT && s.ev[i].exc == EXCEPTION_SINGLE_STEP) { n++; CHECK(s.ev[i].first == 1); }
    EXPECT_EQ(n, 25);
    EXPECT_EQ(s.ev[s.n - 1].exitcode, 7);
    dbg_finish(&s);
}
static DWORD cb_hwbp(dsess *s, DEBUG_EVENT *e, ev_t *r) {
    if (r->code != EXCEPTION_DEBUG_EVENT) return 0;
    CONTEXT c; memset(&c, 0, sizeof c);
    if (r->exc == EXCEPTION_BREAKPOINT && s->bp_count == 1) {
        c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        c.Dr0 = (DWORD64)(ULONG_PTR)child_addr(s, (void *)&g_hw_var2);
        c.Dr7 = 1 | (1u << 16) | (3u << 18);
        U(SetThreadContext)(s->pi.hThread, &c);
        return DBG_CONTINUE;
    }
    if (r->exc == EXCEPTION_SINGLE_STEP) {
        s->ss_count++;
        c.ContextFlags = CONTEXT_DEBUG_REGISTERS | CONTEXT_CONTROL;
        if (U(GetThreadContext)(s->pi.hThread, &c)) { s->hits = (DWORD)c.Dr6; c.Dr7 = 0; c.Dr6 = 0; U(SetThreadContext)(s->pi.hThread, &c); }
        return DBG_CONTINUE;
    }
    return 0;
}
TEST(eed_dbg_hardware_data_breakpoint) {
    NEED_DBG(); REQUIRE(U(GetThreadContext)); REQUIRE(U(SetThreadContext));
    if (t_is_wine()) { t_skip("debug registers not supported under wine"); return; }
    dsess s; dbg_start(&s, "dbg_hwwrite", "", DBG_FLAGS, cb_hwbp, 0); if (!s.pi.hProcess) return;
    dbg_run(&s, 30000);
    if (s.ss_count == 0) { t_note("no hardware breakpoint event (debug registers unavailable?)"); dbg_finish(&s); return; }
    EXPECT_EQ(s.ss_count, 1);
    CHECK(s.hits & 1);
    int i = ev_find_exc(&s, EXCEPTION_SINGLE_STEP, 1, 0);
    if (i >= 0) EXPECT_EQ((ULONG_PTR)s.ev[i].addr, (ULONG_PTR)child_addr(&s, eed_write_dword_after));
    EXPECT_EQ(s.ev[s.n - 1].exitcode, 0);
    dbg_finish(&s);
}

/* ---- ReadProcessMemory in the debuggee ---- */
static DWORD cb_readmem(dsess *s, DEBUG_EVENT *e, ev_t *r) {
    if (r->code == EXCEPTION_DEBUG_EVENT && r->exc == EXCEPTION_BREAKPOINT && s->bp_count == 1) {
        unsigned char hdr[64], mine[64]; SIZE_T got = 0;
        memcpy(mine, GetModuleHandleA(NULL), 64);
        BOOL ok = U(ReadProcessMemory)(s->pi.hProcess, s->cbase, hdr, 64, &got);
        s->hits = ok && got == 64 && !memcmp(hdr, mine, 64);
        unsigned char code[3] = { 0 }; got = 0;
        ok = U(ReadProcessMemory)(s->pi.hProcess, child_addr(s, (void *)eed_av_write), code, 3, &got);
        s->ss_count = ok && got == 3 && !memcmp(code, (void *)eed_av_write, 3);
        char big[16]; got = 77;
        SetLastError(0);
        ok = U(ReadProcessMemory)(s->pi.hProcess, (LPCVOID)0x10, big, 16, &got);
        s->bp_count += 0;
        s->user = (void *)(ULONG_PTR)(ok ? 0 : GetLastError());
    }
    if (r->code == CREATE_PROCESS_DEBUG_EVENT) s->cbase = r->base;
    return 0;
}
TEST(eed_dbg_readprocessmemory_in_debuggee) {
    NEED_DBG();
    dsess s; dbg_start(&s, "dbg_exit7", "", DBG_FLAGS, cb_readmem, 0); if (!s.pi.hProcess) return;
    dbg_run(&s, 20000);
    EXPECT_EQ(s.hits, 1);
    EXPECT_EQ(s.ss_count, 1);
    EXPECT_EQ((ULONG_PTR)s.user, ERROR_PARTIAL_COPY);
    dbg_finish(&s);
}

/* ---- timeouts and argument errors ---- */
TEST(eed_dbg_wait_timeout_121) {
    NEED_DBG();
    dsess s; dbg_start(&s, "dbg_idle", "", DBG_FLAGS, NULL, 0); if (!s.pi.hProcess) return;
    s.cb = NULL;
    /* drain the startup events (the loader breakpoint is the last one) */
    DWORD t0 = GetTickCount(); int seen_bp = 0;
    while (!seen_bp && GetTickCount() - t0 < 15000) {
        DEBUG_EVENT e;
        if (!U(WaitForDebugEvent)(&e, 500)) continue;
        if (e.dwDebugEventCode == EXCEPTION_DEBUG_EVENT) seen_bp = 1;
        if (e.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT && e.u.LoadDll.hFile) CloseHandle(e.u.LoadDll.hFile);
        if (e.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT && e.u.CreateProcessInfo.hFile) CloseHandle(e.u.CreateProcessInfo.hFile);
        U(ContinueDebugEvent)(e.dwProcessId, e.dwThreadId, DBG_CONTINUE);
    }
    CHECK(seen_bp);
    DEBUG_EVENT e; memset(&e, 0x5A, sizeof e);
    SetLastError(0);
    DWORD t1 = GetTickCount();
    BOOL ok = U(WaitForDebugEvent)(&e, 300);
    DWORD dt = GetTickCount() - t1;
    EXPECT_EQ(ok, 0); EXPECT_ERR(ERROR_SEM_TIMEOUT);
    CHECKF(dt >= 250 && dt < 3000, "waited %lu ms", dt);
    SetLastError(0);
    t1 = GetTickCount();
    ok = U(WaitForDebugEvent)(&e, 0);
    EXPECT_EQ(ok, 0); EXPECT_ERR(ERROR_SEM_TIMEOUT);
    CHECK(GetTickCount() - t1 < 500);
    if (U(WaitForDebugEventEx)) {
        SetLastError(0);
        ok = U(WaitForDebugEventEx)(&e, 100);
        EXPECT_EQ(ok, 0); EXPECT_ERR(ERROR_SEM_TIMEOUT);
    }
    dbg_finish(&s);
}
static DWORD WINAPI th_wait_nodebug(LPVOID p) {
    DEBUG_EVENT e; memset(&e, 0, sizeof e);
    BOOL ok = WaitForDebugEvent(&e, 0);
    ((DWORD *)p)[0] = ok; ((DWORD *)p)[1] = GetLastError();
    return 0;
}
TEST(eed_dbg_wait_without_debuggee_thread) {
    NEED_DBG();
    DWORD res[2] = { 7, 7 };
    HANDLE h = CreateThread(NULL, 0, th_wait_nodebug, res, 0, NULL);
    WaitForSingleObject(h, 5000); CloseHandle(h);
    EXPECT_EQ(res[0], 0);
    t_note("WaitForDebugEvent on a thread that never debugged: error %lu", res[1]);
    CHECK(res[1] != 0);
}
TEST(eed_dbg_continue_invalid_status_87) {
    NEED_DBG();
    SetLastError(0);
    BOOL ok = U(ContinueDebugEvent)(GetCurrentProcessId(), GetCurrentThreadId(), 0x12345678);
    EXPECT_EQ(ok, 0);
    t_note("ContinueDebugEvent(bad status) error %lu", GetLastError());
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER || GetLastError() == ERROR_INVALID_HANDLE);
}
TEST(eed_dbg_continue_unknown_ids_fail) {
    NEED_DBG();
    static const DWORD ids[][2] = { {0, 0}, {1, 1}, {0xFFFFFFF0, 0xFFFFFFF0}, {4, 8}, {0x7FFFFFFF, 1} };
    for (size_t i = 0; i < sizeof ids / sizeof *ids; i++) {
        SetLastError(0);
        BOOL ok = U(ContinueDebugEvent)(ids[i][0], ids[i][1], DBG_CONTINUE);
        CHECKF(!ok, "ids (%lu,%lu) succeeded", ids[i][0], ids[i][1]);
        t_note("ContinueDebugEvent(%lu,%lu) -> err %lu", ids[i][0], ids[i][1], GetLastError());
        CHECK(GetLastError() != 0);
    }
}
TEST(eed_dbg_continue_unknown_ids_with_active_session) {
    NEED_DBG();
    dsess s; dbg_start(&s, "dbg_idle", "", DBG_FLAGS, NULL, 0); if (!s.pi.hProcess) return;
    DEBUG_EVENT e;
    CHECK(U(WaitForDebugEvent)(&e, 10000));
    CHECK(e.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT);
    if (e.u.CreateProcessInfo.hFile) CloseHandle(e.u.CreateProcessInfo.hFile);
    SetLastError(0);
    CHECK(!U(ContinueDebugEvent)(e.dwProcessId + 4, e.dwThreadId, DBG_CONTINUE));
    t_note("ContinueDebugEvent(wrong pid) err %lu", GetLastError());
    SetLastError(0);
    CHECK(!U(ContinueDebugEvent)(e.dwProcessId, e.dwThreadId + 4, DBG_CONTINUE));
    t_note("ContinueDebugEvent(wrong tid) err %lu", GetLastError());
    SetLastError(0);
    CHECK(!U(ContinueDebugEvent)(e.dwProcessId, e.dwThreadId, 0x12345678));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CHECK(U(ContinueDebugEvent)(e.dwProcessId, e.dwThreadId, DBG_CONTINUE));
    SetLastError(0);
    CHECK(!U(ContinueDebugEvent)(e.dwProcessId, e.dwThreadId, DBG_CONTINUE));     /* already continued */
    t_note("ContinueDebugEvent(twice) err %lu", GetLastError());
    dbg_finish(&s);
}
TEST(eed_dbg_activeprocess_invalid_pids) {
    REQUIRE(U(DebugActiveProcess));
    SetLastError(0);
    CHECK(!U(DebugActiveProcess)(0)); 
    t_note("DebugActiveProcess(0) err %lu", GetLastError());
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER || GetLastError() == ERROR_ACCESS_DENIED);
    SetLastError(0);
    CHECK(!U(DebugActiveProcess)(0xFFFFFFF0));
    t_note("DebugActiveProcess(bogus) err %lu", GetLastError());
    CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0);
    CHECK(!U(DebugActiveProcess)(GetCurrentProcessId()));
    t_note("DebugActiveProcess(self) err %lu", GetLastError());
    CHECK(GetLastError() != 0);
}
TEST(eed_dbg_activeprocess_system_idle_access_denied) {
    REQUIRE(U(DebugActiveProcess));
    /* safety: never attempt to debug a process this test did not create (including System, pid 4) */
    t_skip("not attaching to processes the test did not create");
}
TEST(eed_dbg_stop_not_debugged_access_denied) {
    REQUIRE(U(DebugActiveProcessStop));
    PROCESS_INFORMATION pi;
    HANDLE h = t_spawn_child("dbg_idle", "", CREATE_NO_WINDOW, NULL, NULL, NULL, FALSE, NULL, &pi);
    CHECK(h != NULL); if (!h) return;
    SetLastError(0);
    CHECK(!U(DebugActiveProcessStop)(pi.dwProcessId));
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    SetLastError(0);
    CHECK(!U(DebugActiveProcessStop)(0xFFFFFFF0));
    t_note("DebugActiveProcessStop(bogus) err %lu", GetLastError());
    TerminateProcess(h, 0); WaitForSingleObject(h, 5000);
    CloseHandle(pi.hThread); CloseHandle(h);
}

/* ---- attach / detach ---- */
TEST(eed_dbg_attach_events_and_detach_keeps_child_alive) {
    NEED_DBG(); REQUIRE(U(DebugActiveProcess)); REQUIRE(U(DebugActiveProcessStop)); REQUIRE(U(DebugSetProcessKillOnExit)); REQUIRE(U(CheckRemoteDebuggerPresent));
    PROCESS_INFORMATION pi;
    HANDLE h = t_spawn_child("dbg_idle", "", CREATE_NO_WINDOW, NULL, NULL, NULL, FALSE, NULL, &pi);
    CHECK(h != NULL); if (!h) return;
    Sleep(700);                                    /* let it reach Sleep() */
    CHECK(U(DebugSetProcessKillOnExit)(FALSE));
    CHECK(U(DebugActiveProcess)(pi.dwProcessId));
    BOOL dbg = FALSE; CHECK(U(CheckRemoteDebuggerPresent)(h, &dbg)); EXPECT_EQ(dbg, 1);
    DWORD t0 = GetTickCount(); int create = 0, loads = 0, threads = 0, bp = 0; int first_code = -1; DWORD bp_tid = 0; ULONG_PTR bp_addr = 0;
    while (!bp && GetTickCount() - t0 < 15000) {
        DEBUG_EVENT e;
        if (!U(WaitForDebugEvent)(&e, 500)) continue;
        if (first_code < 0) first_code = (int)e.dwDebugEventCode;
        if (e.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT) { create++; if (e.u.CreateProcessInfo.hFile) CloseHandle(e.u.CreateProcessInfo.hFile); }
        if (e.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT) { loads++; if (e.u.LoadDll.hFile) CloseHandle(e.u.LoadDll.hFile); }
        if (e.dwDebugEventCode == CREATE_THREAD_DEBUG_EVENT) threads++;
        DWORD st = DBG_CONTINUE;
        if (e.dwDebugEventCode == EXCEPTION_DEBUG_EVENT) {
            if (e.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_BREAKPOINT) { bp = 1; bp_tid = e.dwThreadId; bp_addr = (ULONG_PTR)e.u.Exception.ExceptionRecord.ExceptionAddress; }
            else st = DBG_EXCEPTION_NOT_HANDLED;
        }
        U(ContinueDebugEvent)(e.dwProcessId, e.dwThreadId, st);
    }
    EXPECT_EQ(first_code, CREATE_PROCESS_DEBUG_EVENT);
    EXPECT_EQ(create, 1);
    CHECK(loads >= 3);
    CHECK(threads >= 1);
    CHECK(bp);
    CHECK(bp_tid != pi.dwThreadId);                 /* injected by a remote breakin thread */
    HMODULE m = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)bp_addr, &m);
    CHECK(m == t_ntdll());
    CHECK(U(DebugActiveProcessStop)(pi.dwProcessId));
    Sleep(300);
    EXPECT_EQ(WaitForSingleObject(h, 500), WAIT_TIMEOUT);
    DWORD code = 0; GetExitCodeProcess(h, &code); EXPECT_EQ(code, STILL_ACTIVE);
    dbg = TRUE; CHECK(U(CheckRemoteDebuggerPresent)(h, &dbg)); EXPECT_EQ(dbg, 0);
    U(DebugSetProcessKillOnExit)(TRUE);
    TerminateProcess(h, 0); WaitForSingleObject(h, 5000);
    CloseHandle(pi.hThread); CloseHandle(h);
}
TEST(eed_dbg_attach_twice_fails) {
    NEED_DBG(); REQUIRE(U(DebugActiveProcess)); REQUIRE(U(DebugActiveProcessStop)); REQUIRE(U(DebugSetProcessKillOnExit));
    PROCESS_INFORMATION pi;
    HANDLE h = t_spawn_child("dbg_idle", "", CREATE_NO_WINDOW, NULL, NULL, NULL, FALSE, NULL, &pi);
    CHECK(h != NULL); if (!h) return;
    Sleep(500);
    U(DebugSetProcessKillOnExit)(FALSE);
    CHECK(U(DebugActiveProcess)(pi.dwProcessId));
    SetLastError(0);
    BOOL again = U(DebugActiveProcess)(pi.dwProcessId);
    t_note("second DebugActiveProcess: %d err %lu", again, GetLastError());
    CHECK(!again);
    /* drain what the attach produced, then detach */
    DWORD t0 = GetTickCount();
    while (GetTickCount() - t0 < 2500) {
        DEBUG_EVENT e;
        if (!U(WaitForDebugEvent)(&e, 400)) continue;
        if (e.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT && e.u.LoadDll.hFile) CloseHandle(e.u.LoadDll.hFile);
        if (e.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT && e.u.CreateProcessInfo.hFile) CloseHandle(e.u.CreateProcessInfo.hFile);
        U(ContinueDebugEvent)(e.dwProcessId, e.dwThreadId, e.dwDebugEventCode == EXCEPTION_DEBUG_EVENT && e.u.Exception.ExceptionRecord.ExceptionCode != EXCEPTION_BREAKPOINT ? DBG_EXCEPTION_NOT_HANDLED : DBG_CONTINUE);
    }
    CHECK(U(DebugActiveProcessStop)(pi.dwProcessId));
    U(DebugSetProcessKillOnExit)(TRUE);
    TerminateProcess(h, 0); WaitForSingleObject(h, 5000);
    CloseHandle(pi.hThread); CloseHandle(h);
}

/* ---- DebugBreakProcess ---- */
TEST(eed_dbg_debugbreakprocess_injects_breakpoint) {
    NEED_DBG(); REQUIRE(U(DebugBreakProcess));
    dsess s; dbg_start(&s, "dbg_idle", "", DBG_FLAGS, NULL, 0); if (!s.pi.hProcess) return;
    s.stop = 0;
    /* run until the loader breakpoint has been consumed */
    DWORD t0 = GetTickCount();
    while (s.bp_count < 1 && GetTickCount() - t0 < 15000) {
        DEBUG_EVENT e;
        if (!U(WaitForDebugEvent)(&e, 400)) continue;
        if (e.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT && e.u.LoadDll.hFile) CloseHandle(e.u.LoadDll.hFile);
        if (e.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT && e.u.CreateProcessInfo.hFile) CloseHandle(e.u.CreateProcessInfo.hFile);
        if (e.dwDebugEventCode == EXCEPTION_DEBUG_EVENT) s.bp_count++;
        U(ContinueDebugEvent)(e.dwProcessId, e.dwThreadId, DBG_CONTINUE);
    }
    CHECK(s.bp_count >= 1);
    Sleep(400);
    SetLastError(0);
    BOOL ok = U(DebugBreakProcess)(s.pi.hProcess);
    CHECK(ok);
    int got_thread = 0, got_bp = 0; DWORD bp_tid = 0;
    t0 = GetTickCount();
    while (!got_bp && GetTickCount() - t0 < 10000) {
        DEBUG_EVENT e;
        if (!U(WaitForDebugEvent)(&e, 500)) continue;
        if (e.dwDebugEventCode == CREATE_THREAD_DEBUG_EVENT) got_thread = 1;
        if (e.dwDebugEventCode == EXCEPTION_DEBUG_EVENT && e.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_BREAKPOINT) { got_bp = 1; bp_tid = e.dwThreadId; }
        U(ContinueDebugEvent)(e.dwProcessId, e.dwThreadId, DBG_CONTINUE);
    }
    CHECK(got_thread); CHECK(got_bp);
    CHECK(bp_tid != s.pi.dwThreadId);
    dbg_finish(&s);
}
TEST(eed_dbg_debugbreakprocess_invalid_handle) {
    REQUIRE(U(DebugBreakProcess));
    SetLastError(0);
    CHECK(!U(DebugBreakProcess)(NULL)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SetLastError(0);
    CHECK(!U(DebugBreakProcess)((HANDLE)(ULONG_PTR)0x7777FFF8)); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(eed_dbg_debugbreakprocess_needs_access) {
    REQUIRE(U(DebugBreakProcess)); REQUIRE(U(OpenProcess));
    PROCESS_INFORMATION pi;
    HANDLE h = t_spawn_child("dbg_idle", "", CREATE_NO_WINDOW, NULL, NULL, NULL, FALSE, NULL, &pi);
    CHECK(h != NULL); if (!h) return;
    HANDLE ro = U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pi.dwProcessId);
    CHECK(ro != NULL);
    if (ro) { SetLastError(0); CHECK(!U(DebugBreakProcess)(ro)); EXPECT_ERR(ERROR_ACCESS_DENIED); CloseHandle(ro); }
    TerminateProcess(h, 0); WaitForSingleObject(h, 5000);
    CloseHandle(pi.hThread); CloseHandle(h);
}

/* ---- DEBUG_PROCESS vs DEBUG_ONLY_THIS_PROCESS ---- */
TEST(eed_dbg_only_this_process_ignores_grandchild) {
    NEED_DBG();
    dsess s; dbg_start(&s, "dbg_spawner", "", DEBUG_ONLY_THIS_PROCESS, NULL, 0); if (!s.pi.hProcess) return;
    dbg_run(&s, 30000);
    EXPECT_EQ(ev_count(&s, CREATE_PROCESS_DEBUG_EVENT), 1);
    for (int i = 0; i < s.n; i++) if (s.ev[i].pid != s.pi.dwProcessId) { CHECKF(0, "event %d from pid %lu", i, s.ev[i].pid); break; }
    EXPECT_EQ(s.ev[s.n - 1].exitcode, 0);
    dbg_finish(&s);
}
static int is_our_exe(const char *path) {
    char me[MAX_PATH]; GetModuleFileNameA(NULL, me, sizeof me);
    const char *b = strrchr(me, '\\'); b = b ? b + 1 : me;
    return ends_with_i(path, b);
}
TEST(eed_dbg_debug_process_includes_grandchild) {
    NEED_DBG();
    dsess s; dbg_start(&s, "dbg_spawner", "", DEBUG_PROCESS, NULL, 0); if (!s.pi.hProcess) return;
    dbg_run(&s, 30000);
    int ours = 0; DWORD gpid = 0;
    for (int i = 0; i < s.n; i++) if (s.ev[i].code == CREATE_PROCESS_DEBUG_EVENT && is_our_exe(s.ev[i].dll)) { ours++; if (s.ev[i].pid != s.pi.dwProcessId) gpid = s.ev[i].pid; }
    EXPECT_EQ(ours, 2);
    CHECK(gpid != 0);
    if (gpid) {
        int gx = -1;
        for (int i = 0; i < s.n; i++) if (s.ev[i].code == EXIT_PROCESS_DEBUG_EVENT && s.ev[i].pid == gpid) gx = i;
        CHECK(gx >= 0);
        if (gx >= 0) EXPECT_EQ(s.ev[gx].exitcode, 7);
    }
    EXPECT_EQ(s.ev[s.n - 1].exitcode, 0);
    dbg_finish(&s);
}

/* ---- kill-on-exit: a child process acts as the debugger and exits ---- */
static DWORD read_pid_file(const char *path, DWORD to) {
    DWORD t0 = GetTickCount();
    while (GetTickCount() - t0 < to) {
        char b[32] = { 0 }; DWORD n = 0;
        HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (f != INVALID_HANDLE_VALUE) { ReadFile(f, b, 31, &n, NULL); CloseHandle(f); if (n) return (DWORD)atoi(b); }
        Sleep(100);
    }
    return 0;
}
static void kill_on_exit_case(int kill) {
    REQUIRE(U(DebugSetProcessKillOnExit)); REQUIRE(U(OpenProcess));
    char pf[MAX_PATH], args[MAX_PATH + 16];
    t_tmpfile(pf, sizeof pf, kill ? "koe1.pid" : "koe0.pid"); DeleteFileA(pf);
    snprintf(args, sizeof args, "\"%s\" %d", pf, kill);
    PROCESS_INFORMATION pi;
    HANDLE parent = t_spawn_child("dbg_parent", args, CREATE_NO_WINDOW, NULL, NULL, NULL, FALSE, NULL, &pi);
    CHECK(parent != NULL); if (!parent) return;
    DWORD gpid = read_pid_file(pf, 20000);
    CHECK(gpid != 0);
    HANDLE g = gpid ? U(OpenProcess)(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE, FALSE, gpid) : NULL;
    CHECK(g != NULL);
    if (g) {
        EXPECT_EQ(WaitForSingleObject(parent, 20000), WAIT_OBJECT_0);       /* debugger leaves */
        DWORD w = WaitForSingleObject(g, kill ? 10000 : 2500);
        DWORD code = 0; GetExitCodeProcess(g, &code);
        if (kill) { EXPECT_EQ(w, WAIT_OBJECT_0); t_note("debuggee exit code after debugger exit: 0x%lx", code); }
        else { EXPECT_EQ(w, WAIT_TIMEOUT); EXPECT_EQ(code, STILL_ACTIVE); }
        TerminateProcess(g, 0); WaitForSingleObject(g, 5000);
        CloseHandle(g);
    }
    TerminateProcess(parent, 0);
    CloseHandle(pi.hThread); CloseHandle(parent);
}
TEST(eed_dbg_debugger_exit_kills_debuggee_by_default) { kill_on_exit_case(1); }
TEST(eed_dbg_killonexit_false_leaves_debuggee_alive) { kill_on_exit_case(0); }

/* ---- thread context at process creation ---- */
static DWORD cb_create_ctx(dsess *s, DEBUG_EVENT *e, ev_t *r) {
    if (r->code == CREATE_PROCESS_DEBUG_EVENT) {
        CONTEXT c; memset(&c, 0, sizeof c); c.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
        if (U(GetThreadContext)(s->pi.hThread, &c)) {
            HMODULE m = NULL;
            GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)(ULONG_PTR)c.Rip, &m);
            s->hits = (m == t_ntdll());
            s->user = (void *)(ULONG_PTR)c.Rcx;
        }
    }
    return 0;
}
TEST(eed_dbg_initial_thread_context_in_ntdll) {
    NEED_DBG(); REQUIRE(U(GetThreadContext));
    dsess s; dbg_start(&s, "dbg_exit7", "", DBG_FLAGS, cb_create_ctx, 0); if (!s.pi.hProcess) return;
    dbg_run(&s, 20000);
    EXPECT_EQ(s.hits, 1);
    t_note("initial Rcx %p, entry %p", s.user, s.n ? s.ev[0].start : NULL);
    dbg_finish(&s);
}
