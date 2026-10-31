/* Pseudo console (ConPTY) creation and use as a child's console. */
#include "pc.h"

typedef void *PCHPCON;
UDECL(HRESULT, CreatePseudoConsole, (COORD, HANDLE, HANDLE, DWORD, PCHPCON *));
UDECL(HRESULT, ResizePseudoConsole, (PCHPCON, COORD));
UDECL(void, ClosePseudoConsole, (PCHPCON));
#define A_PCON 0x00020016

typedef struct { HANDLE in_r, in_w, out_r, out_w; PCHPCON pc; } PTY;
static int pty_open(PTY *p, SHORT cols, SHORT rows) {
    COORD sz; HRESULT hr; memset(p, 0, sizeof *p);
    if (!CreatePipe(&p->in_r, &p->in_w, NULL, 0) || !CreatePipe(&p->out_r, &p->out_w, NULL, 0)) return 0;
    sz.X = cols; sz.Y = rows;
    hr = UN(CreatePseudoConsole)(sz, p->in_r, p->out_w, 0, &p->pc);
    return hr == S_OK && p->pc != NULL;
}
static DWORD WINAPI close_thread(LPVOID p) { UN(ClosePseudoConsole)((PCHPCON)p); return 0; }
static void pty_close(PTY *p) {
    if (p->pc) {   /* ClosePseudoConsole can block until the output pipe is drained */
        HANDLE t = CreateThread(NULL, 0, close_thread, p->pc, 0, NULL); int i; char b[4096]; DWORD g;
        for (i = 0; i < 100 && WaitForSingleObject(t, 50) == WAIT_TIMEOUT; i++) { DWORD av = 0; if (PeekNamedPipe(p->out_r, NULL, 0, NULL, &av, NULL) && av) ReadFile(p->out_r, b, sizeof b, &g, NULL); }
        CloseHandle(t);
    }
    if (p->in_r) CloseHandle(p->in_r);
    if (p->in_w) CloseHandle(p->in_w);
    if (p->out_r) CloseHandle(p->out_r);
    if (p->out_w) CloseHandle(p->out_w);
}
static int pty_run(PTY *p, const char *child, char *out, size_t cap, DWORD *code) {
    STARTUPINFOEXW sx; pc_opts o; PROCESS_INFORMATION pi; SIZE_T sz = 0; LPPROC_THREAD_ATTRIBUTE_LIST l; size_t n = 0; int i;
    U(InitializeProcThreadAttributeList)(NULL, 1, 0, &sz);
    l = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, sz);
    U(InitializeProcThreadAttributeList)(l, 1, 0, &sz);
    if (!U(UpdateProcThreadAttribute)(l, 0, A_PCON, p->pc, sizeof(PCHPCON), NULL, NULL)) { HeapFree(GetProcessHeap(), 0, l); return 0; }
    memset(&sx, 0, sizeof sx); sx.StartupInfo.cb = sizeof sx; sx.lpAttributeList = l;
    memset(&o, 0, sizeof o); o.child = child; o.si = &sx; o.flags = EXTENDED_STARTUPINFO_PRESENT;
    if (!pc_launch(&o, &pi)) { DWORD e = GetLastError(); U(DeleteProcThreadAttributeList)(l); HeapFree(GetProcessHeap(), 0, l); SetLastError(e); return 0; }
    out[0] = 0;
    for (i = 0; i < 100; i++) {
        DWORD av = 0, g = 0;
        while (PeekNamedPipe(p->out_r, NULL, 0, NULL, &av, NULL) && av && n + 1 < cap) { ReadFile(p->out_r, out + n, (DWORD)(cap - 1 - n), &g, NULL); n += g; out[n] = 0; }
        if (strstr(out, "PTYHELLO") && strchr(strstr(out, "PTYHELLO"), '\n')) break;
        if (WaitForSingleObject(pi.hProcess, 50) == WAIT_OBJECT_0 && !av) { Sleep(100); }
    }
    if (WaitForSingleObject(pi.hProcess, 10000) != WAIT_OBJECT_0) { TerminateProcess(pi.hProcess, 1); *code = 0xDEAD0001; } else GetExitCodeProcess(pi.hProcess, code);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    U(DeleteProcThreadAttributeList)(l); HeapFree(GetProcessHeap(), 0, l);
    return 1;
}

TEST(pty_child_output_arrives_on_pipe) {
    REQUIRE(UN(CreatePseudoConsole)); REQUIRE(UN(ClosePseudoConsole));
    PTY p; char out[16384]; DWORD code = 0;
    CHECK(pty_open(&p, 100, 30)); if (!p.pc) { pty_close(&p); return; }
    CHECK(pty_run(&p, "pc_pty", out, sizeof out, &code)); EQV(code, 0);
    CHECK(strstr(out, "PTYHELLO") != NULL);
    pty_close(&p);
}
TEST(pty_child_sees_a_console) {
    REQUIRE(UN(CreatePseudoConsole)); REQUIRE(UN(ClosePseudoConsole));
    PTY p; char out[16384]; DWORD code = 0; const char *s;
    CHECK(pty_open(&p, 100, 30)); if (!p.pc) { pty_close(&p); return; }
    CHECK(pty_run(&p, "pc_pty", out, sizeof out, &code));
    s = strstr(out, "PTYHELLO");
    CHECK(s != NULL);
    if (s) { CHECK(strstr(s, "outcon=1") != NULL); CHECK(strstr(s, "incon=1") != NULL); CHECK(strstr(s, "sb=1") != NULL); }
    pty_close(&p);
}
TEST(pty_buffer_size_is_reported_to_child) {
    REQUIRE(UN(CreatePseudoConsole)); REQUIRE(UN(ClosePseudoConsole));
    PTY p; char out[16384]; DWORD code = 0; const char *s;
    CHECK(pty_open(&p, 87, 21)); if (!p.pc) { pty_close(&p); return; }
    CHECK(pty_run(&p, "pc_pty", out, sizeof out, &code));
    s = strstr(out, "PTYHELLO"); CHECK(s != NULL);
    if (s) { CHECK(strstr(s, "cols=87") != NULL); CHECK(strstr(s, "rows=21") != NULL); }
    pty_close(&p);
}
TEST(pty_resize_succeeds) {
    REQUIRE(UN(CreatePseudoConsole)); REQUIRE(UN(ResizePseudoConsole));
    PTY p; COORD c; HRESULT hr;
    CHECK(pty_open(&p, 80, 25)); if (!p.pc) { pty_close(&p); return; }
    c.X = 120; c.Y = 40; hr = UN(ResizePseudoConsole)(p.pc, c); EQV(hr, S_OK);
    c.X = 20; c.Y = 5; hr = UN(ResizePseudoConsole)(p.pc, c); EQV(hr, S_OK);
    pty_close(&p);
}
TEST(pty_create_argument_errors_observed) {
    REQUIRE(UN(CreatePseudoConsole));
    PTY p; COORD c; HRESULT hr; PCHPCON pc = NULL; memset(&p, 0, sizeof p);
    CHECK(CreatePipe(&p.in_r, &p.in_w, NULL, 0)); CHECK(CreatePipe(&p.out_r, &p.out_w, NULL, 0));
    c.X = 0; c.Y = 0; hr = UN(CreatePseudoConsole)(c, p.in_r, p.out_w, 0, &pc); t_note("CreatePseudoConsole(0x0) -> 0x%lx", (unsigned long)hr); if (hr == S_OK && pc) UN(ClosePseudoConsole)(pc);
    pc = NULL; c.X = 80; c.Y = 25;
    hr = UN(CreatePseudoConsole)(c, NULL, p.out_w, 0, &pc); t_note("CreatePseudoConsole(NULL input) -> 0x%lx", (unsigned long)hr); if (hr == S_OK && pc) UN(ClosePseudoConsole)(pc);
    pc = NULL;
    hr = UN(CreatePseudoConsole)(c, p.in_r, NULL, 0, &pc); t_note("CreatePseudoConsole(NULL output) -> 0x%lx", (unsigned long)hr); if (hr == S_OK && pc) UN(ClosePseudoConsole)(pc);
    pc = NULL;
    hr = UN(CreatePseudoConsole)(c, p.in_r, p.out_w, 0x80, &pc); t_note("CreatePseudoConsole(bad flags) -> 0x%lx", (unsigned long)hr); if (hr == S_OK && pc) UN(ClosePseudoConsole)(pc);
    pty_close(&p);
}
