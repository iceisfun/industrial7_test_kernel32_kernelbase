/* gap_10: child dispatcher for console tests */
#include "g10.h"

#define G10_MAX 512
static struct { const char *name; g10_fn fn; } g_tab[G10_MAX];
static int g_ntab;
static char g_res[MAX_PATH * 2];
int g10_cpass, g10_cskip;

void g10_reg(const char *name, g10_fn fn) { if (g_ntab < G10_MAX) { g_tab[g_ntab].name = name; g_tab[g_ntab].fn = fn; g_ntab++; } }

void g10_cline(const char *fmt, ...) {
    char buf[1100]; va_list a; DWORD n; HANDLE h; int len;
    if (!g_res[0]) return;
    va_start(a, fmt); len = vsnprintf(buf, sizeof buf - 2, fmt, a); va_end(a);
    if (len < 0) len = 0;
    if (len > (int)sizeof buf - 3) len = sizeof buf - 3;
    buf[len++] = '\n';
    h = CreateFileA(g_res, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    WriteFile(h, buf, len, &n, NULL);
    CloseHandle(h);
}
void g10_cfail(const char *file, int line, const char *expr, const char *fmt, ...) {
    char d[512]; const char *base;
    d[0] = 0;
    if (fmt) { va_list a; va_start(a, fmt); vsnprintf(d, sizeof d, fmt, a); va_end(a); }
    base = strrchr(file, '/'); if (!base) base = strrchr(file, '\\'); base = base ? base + 1 : file;
    g10_cline("FAIL %s:%d: %s%s%s%s", base, line, expr, d[0] ? " (" : "", d, d[0] ? ")" : "");
}
void g10_cnote(const char *fmt, ...) {
    char d[700]; va_list a;
    va_start(a, fmt); vsnprintf(d, sizeof d, fmt, a); va_end(a);
    g10_cline("NOTE %s", d);
}

/* child: exe --child gap10_con <test> <resultfile> */
CHILD(gap10_con) {
    if (argc < 3) return 0xC0DE0003;
    snprintf(g_res, sizeof g_res, "%s", argv[2]);
    for (int i = 0; i < g_ntab; i++) {
        if (!strcmp(g_tab[i].name, argv[1])) {
            g_tab[i].fn();
            g10_cline("DONE %d", g10_cskip ? 0 : g10_cpass);
            return 0;
        }
    }
    return 0xC0DE0002;
}

/* child that just sleeps (a process to query; the parent terminates it) */
CHILD(gap10_sleep) {
    Sleep(argc > 1 ? (DWORD)atoi(argv[1]) : 10000);
    return 0;
}

static void report(const char *res, DWORD code, int timedout, const char *what) {
    char *buf = (char *)calloc(1, 262144);
    DWORD n = 0; int done = 0, npass = 0;
    HANDLE h = CreateFileA(res, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) { ReadFile(h, buf, 262143, &n, NULL); CloseHandle(h); }
    buf[n] = 0;
    for (char *p = buf; *p; ) {
        char *e = strchr(p, '\n'); size_t l;
        if (e) *e = 0;
        l = strlen(p); if (l && p[l - 1] == '\r') p[l - 1] = 0;
        if (!strncmp(p, "FAIL ", 5)) t_check(0, "child", 0, p + 5, NULL);
        else if (!strncmp(p, "NOTE ", 5)) t_note("%s", p + 5);
        else if (!strncmp(p, "SKIP ", 5)) t_skip("%s", p + 5);
        else if (!strncmp(p, "DONE ", 5)) { done = 1; npass = atoi(p + 5); }
        if (!e) break;
        p = e + 1;
    }
    if (timedout) t_check(0, "child", 0, "child finished within timeout", "%s", what);
    else if (!done) t_check(0, "child", 0, "child wrote DONE", "%s exit=0x%lx", what, code);
    else if (code != 0) t_check(0, "child", 0, "child exit code 0", "exit=0x%lx", code);
    for (int i = 0; i < npass; i++) t_check(1, "child", 0, "", NULL);
    free(buf);
}

static int spawn_console(const char *name, DWORD flags, char *res, size_t rescap, PROCESS_INFORMATION *pi) {
    static int seq;
    char cmd[4096]; STARTUPINFOA si;
    snprintf(res, rescap, "%sg10res_%d_%d.txt", t_tmpdir(), (int)GetCurrentProcessId(), ++seq);
    DeleteFileA(res);
    memset(&si, 0, sizeof si); si.cb = sizeof si;
    si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_SHOWMINNOACTIVE;
    snprintf(cmd, sizeof cmd, "\"%s\" --child gap10_con %s \"%s\"", t_self_path(), name, res);
    memset(pi, 0, sizeof *pi);
    return CreateProcessA(NULL, cmd, NULL, NULL, FALSE, flags, NULL, NULL, &si, pi);
}

void g10_run(const char *name, int nowindow) {
    PROCESS_INFORMATION pi; char res[MAX_PATH * 2]; DWORD code = 0xDEAD0000; int to = 0;
    DWORD flags = nowindow ? CREATE_NO_WINDOW : CREATE_NEW_CONSOLE;
    int ok;
    /* the emulator's windowed console host needs a display; its headless host behaves like a console */
    if (t_is_wine() && (flags & CREATE_NEW_CONSOLE)) flags = CREATE_NO_WINDOW;
    ok = spawn_console(name, flags, res, sizeof res, &pi);
    for (int retry = 0; !ok && retry < 3; retry++) { Sleep(300); ok = spawn_console(name, flags, res, sizeof res, &pi); }
    if (!ok) { t_check(0, "parent", 0, "CreateProcess child", "err=%lu", GetLastError()); return; }
    if (WaitForSingleObject(pi.hProcess, 25000) != WAIT_OBJECT_0) { TerminateProcess(pi.hProcess, 1); WaitForSingleObject(pi.hProcess, 5000); to = 1; }
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    report(res, code, to, name);
}
