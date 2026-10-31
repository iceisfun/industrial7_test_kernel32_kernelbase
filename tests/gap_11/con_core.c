#include "con_h.h"

char g_res[MAX_PATH * 2];
int g_cpass, g_cskip;


void cline(const char *fmt, ...) {
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
void cfail(const char *file, int line, const char *expr, const char *fmt, ...) {
    char d[512]; d[0] = 0;
    if (fmt) { va_list a; va_start(a, fmt); vsnprintf(d, sizeof d, fmt, a); va_end(a); }
    const char *base = strrchr(file, '/'); if (!base) base = strrchr(file, '\\'); base = base ? base + 1 : file;
    cline("FAIL %s:%d: %s%s%s%s", base, line, expr, d[0] ? " (" : "", d, d[0] ? ")" : "");
}
void cnote(const char *fmt, ...) {
    char d[700]; va_list a; va_start(a, fmt); vsnprintf(d, sizeof d, fmt, a); va_end(a);
    cline("NOTE %s", d);
}
/* ---- dispatcher: `--child gap11_con <name> <resultfile>` ---- */
#define MAXCT 256
static struct { const char *n; cfn_t f; } g_ct[MAXCT];
static int g_nct;
void ct_register(const char *n, cfn_t f) { if (g_nct < MAXCT) { g_ct[g_nct].n = n; g_ct[g_nct].f = f; g_nct++; } }

void prun(const char *name, DWORD flags) {
    static int seq;
    char res[MAX_PATH * 2], cmd[4096];
    STARTUPINFOA si; PROCESS_INFORMATION pi; DWORD code = 0xDEAD0000; int to = 0;
    if (t_is_wine() && (flags & CREATE_NEW_CONSOLE)) flags = (flags & ~CREATE_NEW_CONSOLE) | CREATE_NO_WINDOW;
    snprintf(res, sizeof res, "%sg11res_%lu_%d.txt", t_tmpdir(), GetCurrentProcessId(), ++seq);
    DeleteFileA(res);
    memset(&si, 0, sizeof si); si.cb = sizeof si;
    si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_SHOWMINNOACTIVE;
    snprintf(cmd, sizeof cmd, "\"%s\" --child gap11_con %s \"%s\"", t_self_path(), name, res);
    memset(&pi, 0, sizeof pi);
    BOOL ok = CreateProcessA(NULL, cmd, NULL, NULL, FALSE, flags, NULL, NULL, &si, &pi);
    for (int retry = 0; !ok && retry < 3; retry++) { Sleep(300); ok = CreateProcessA(NULL, cmd, NULL, NULL, FALSE, flags, NULL, NULL, &si, &pi); }
    if (!ok) { t_check(0, "parent", 0, "CreateProcess child", "err=%lu", GetLastError()); return; }
    if (WaitForSingleObject(pi.hProcess, 25000) != WAIT_OBJECT_0) { TerminateProcess(pi.hProcess, 1); WaitForSingleObject(pi.hProcess, 5000); to = 1; }
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);

    char *buf = (char *)calloc(1, 262144); DWORD n = 0; int done = 0, npass = 0;
    HANDLE h = CreateFileA(res, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) { ReadFile(h, buf, 262143, &n, NULL); CloseHandle(h); }
    buf[n] = 0;
    for (char *p = buf; *p; ) {
        char *e = strchr(p, '\n'); if (e) *e = 0;
        size_t l = strlen(p); if (l && p[l - 1] == '\r') p[l - 1] = 0;
        if (!strncmp(p, "FAIL ", 5)) t_check(0, "child", 0, p + 5, NULL);
        else if (!strncmp(p, "NOTE ", 5)) t_note("%s", p + 5);
        else if (!strncmp(p, "SKIP ", 5)) t_skip("%s", p + 5);
        else if (!strncmp(p, "DONE ", 5)) { done = 1; npass = atoi(p + 5); }
        if (!e) break;
        p = e + 1;
    }
    if (to) t_check(0, "child", 0, "child finished within timeout", "%s", name);
    else if (!done) t_check(0, "child", 0, "child wrote DONE", "%s exit=0x%lx", name, code);
    else if (code != 0) t_check(0, "child", 0, "child exit code 0", "exit=0x%lx", code);
    for (int i = 0; i < npass; i++) t_check(1, "child", 0, "", NULL);
    free(buf);
}
CHILD(gap11_con) {
    if (argc < 3) return 3;
    snprintf(g_res, sizeof g_res, "%s", argv[2]);
    for (int i = 0; i < g_nct; i++) if (!strcmp(g_ct[i].n, argv[1])) {
        g_ct[i].f();
        cline("DONE %d", g_cskip ? 0 : g_cpass);
        return 0;
    }
    return 4;
}

