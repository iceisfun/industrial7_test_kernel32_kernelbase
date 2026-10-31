/* gap_12 scaffolding: child-process test bodies that report through a result file.
 *
 * GT(name)  { body }   TEST gap12_<name> that runs body in a detached child (no console at all)
 * GTC(name) { body }   same, but the child owns a brand new console (CREATE_NEW_CONSOLE)
 * GTCT(name, "title") same as GTC with a start-up title
 *
 * The child writes lines to a result file: FAIL / NOTE / SKIP / DONE <n>. The parent folds them into
 * harness results. Child-side checks use the G* macros (they never touch the parent's counters).
 */
#ifndef GC_H
#define GC_H
#ifndef NTDDI_VERSION
#define NTDDI_VERSION 0x0A000006
#endif
#include "t.h"
#include <wincon.h>
#include <sddl.h>
#include <stdlib.h>

static char g_res[MAX_PATH * 2];
static int g_cpass, g_cskip;

static void gline(const char *fmt, ...) {
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
static void gfail(const char *file, int line, const char *expr, const char *fmt, ...) {
    char d[512]; const char *base;
    d[0] = 0;
    if (fmt) { va_list a; va_start(a, fmt); vsnprintf(d, sizeof d, fmt, a); va_end(a); }
    base = strrchr(file, '/'); if (!base) base = strrchr(file, '\\'); base = base ? base + 1 : file;
    gline("FAIL %s:%d: %s%s%s%s", base, line, expr, d[0] ? " (" : "", d, d[0] ? ")" : "");
}
static void gnote(const char *fmt, ...) {
    char d[700]; va_list a; va_start(a, fmt); vsnprintf(d, sizeof d, fmt, a); va_end(a);
    gline("NOTE %s", d);
}
#define GC(c)        do { if (c) g_cpass++; else gfail(__FILE__, __LINE__, #c, NULL); } while (0)
#define GCF(c, ...)  do { if (c) g_cpass++; else gfail(__FILE__, __LINE__, #c, __VA_ARGS__); } while (0)
#define GEQ(a, b)    do { unsigned long long a_ = (unsigned long long)(ULONG_PTR)(a), b_ = (unsigned long long)(ULONG_PTR)(b); \
    if (a_ == b_) g_cpass++; else gfail(__FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", a_, b_); } while (0)
#define GNE(a, b)    do { unsigned long long a_ = (unsigned long long)(ULONG_PTR)(a), b_ = (unsigned long long)(ULONG_PTR)(b); \
    if (a_ != b_) g_cpass++; else gfail(__FILE__, __LINE__, #a " != " #b, "got 0x%llx", a_); } while (0)
#define GERR(e)      do { DWORD e_ = GetLastError(); if (e_ == (DWORD)(e)) g_cpass++; else gfail(__FILE__, __LINE__, "GetLastError == " #e, "got %lu want %lu", e_, (DWORD)(e)); } while (0)
#define GREQ(p)      do { if (!(p)) { gline("SKIP missing export: " #p); g_cskip = 1; return; } } while (0)
#define GWINESKIP(why) do { if (t_is_wine()) { gline("SKIP " why); g_cskip = 1; return; } } while (0)
#define GSTR(a, b)   do { if (!strcmp((a), (b))) g_cpass++; else gfail(__FILE__, __LINE__, #a " == " #b, "got \"%s\" want \"%s\"", (a), (b)); } while (0)
#define GWSTR(a, b)  do { if (!lstrcmpW((a), (b))) g_cpass++; else gfail(__FILE__, __LINE__, #a " == " #b, "got \"%ls\" want \"%ls\"", (a), (b)); } while (0)
/* call must fail with ret 0 and the given last error */
#define GFAIL(call, err) do { SetLastError(0xBAD); BOOL r_ = (BOOL)(call); GEQ(r_, 0); GERR(err); } while (0)

static int gwrap(int argc, char **argv, void (*f)(void)) {
    if (argc < 2) return 3;
    snprintf(g_res, sizeof g_res, "%s", argv[1]);
    f();
    if (g_cskip) { gline("DONE 0"); return 0; }
    gline("DONE %d", g_cpass);
    return 0;
}

static void grun_report(const char *res, DWORD code, int timedout, const char *what) {
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

/* Spawn this exe as `--child <name> "<resultfile>"` and fold the result into the current test. */
static void grun(const char *cname, DWORD flags, const char *title) {
    static int seq;
    char res[MAX_PATH * 2], cmd[4096];
    STARTUPINFOA si; PROCESS_INFORMATION pi; DWORD code = 0xDEAD0000; int to = 0, ok = 0;
    if (t_is_wine() && (flags & CREATE_NEW_CONSOLE)) flags = (flags & ~CREATE_NEW_CONSOLE) | CREATE_NO_WINDOW;
    snprintf(res, sizeof res, "%sg12_%d_%d.txt", t_tmpdir(), (int)GetCurrentProcessId(), ++seq);
    DeleteFileA(res);
    memset(&si, 0, sizeof si); si.cb = sizeof si;
    si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_SHOWMINNOACTIVE;
    if (title) si.lpTitle = (LPSTR)title;
    snprintf(cmd, sizeof cmd, "\"%s\" --child %s \"%s\"", t_self_path(), cname, res);
    for (int retry = 0; retry < 3 && !ok; retry++) {
        memset(&pi, 0, sizeof pi);
        ok = CreateProcessA(NULL, cmd, NULL, NULL, FALSE, flags, NULL, NULL, &si, &pi);
        if (!ok) Sleep(300);
    }
    if (!ok) { t_check(0, "parent", 0, "CreateProcess child", "err=%lu", GetLastError()); return; }
    if (WaitForSingleObject(pi.hProcess, 25000) != WAIT_OBJECT_0) { TerminateProcess(pi.hProcess, 1); WaitForSingleObject(pi.hProcess, 5000); to = 1; }
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    grun_report(res, code, to, cname);
}

#define GTF(n, fl, ttl) \
    static void gb_##n(void); \
    CHILD(gap12_##n) { return gwrap(argc, argv, gb_##n); } \
    TEST(gap12_##n) { grun("gap12_" #n, (fl), (ttl)); } \
    static void gb_##n(void)
#define GT(n)  GTF(n, DETACHED_PROCESS, NULL)
#define GTC(n) GTF(n, CREATE_NEW_CONSOLE, NULL)
#define GTCT(n, ttl) GTF(n, CREATE_NEW_CONSOLE, ttl)

#endif
