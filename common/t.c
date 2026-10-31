#include "t.h"
#include <setjmp.h>

static HMODULE g_k32, g_kb, g_nt, g_u32;
static int g_pass, g_fail, g_cur_fail, g_cur_skip;
static const char *g_cur;
static jmp_buf g_jb;
static volatile int g_in_test;

HMODULE t_kernel32(void) { if (!g_k32) g_k32 = LoadLibraryA("kernel32.dll"); return g_k32; }
HMODULE t_kernelbase(void) { if (!g_kb) g_kb = LoadLibraryA("kernelbase.dll"); return g_kb; }
HMODULE t_ntdll(void) { if (!g_nt) g_nt = LoadLibraryA("ntdll.dll"); return g_nt; }
HMODULE t_user32(void) { if (!g_u32) g_u32 = LoadLibraryA("user32.dll"); return g_u32; }
FARPROC t_proc_k32(const char *n) { return GetProcAddress(t_kernel32(), n); }
FARPROC t_proc_kb(const char *n) { HMODULE m = t_kernelbase(); return m ? GetProcAddress(m, n) : NULL; }
FARPROC t_proc_nt(const char *n) { return GetProcAddress(t_ntdll(), n); }
FARPROC t_proc(const char *n) {
    FARPROC p = t_proc_k32(n);
    if (!p) p = t_proc_kb(n);
    if (!p) p = t_proc_nt(n);
    if (!p && g_u32) p = GetProcAddress(g_u32, n);
    return p;
}
int t_is_wine(void) { return GetProcAddress(t_ntdll(), "wine_get_version") != NULL; }

#define MAXT 8192
static struct { const char *n; t_fn f; } g_t[MAXT];
static int g_nt_;
void t_register(const char *n, t_fn f) { if (g_nt_ < MAXT) { g_t[g_nt_].n = n; g_t[g_nt_].f = f; g_nt_++; } }
static struct { const char *n; t_child_fn f; } g_c[512];
static int g_nc;
void t_register_child(const char *n, t_child_fn f) { if (g_nc < 512) { g_c[g_nc].n = n; g_c[g_nc].f = f; g_nc++; } }

void t_check(int ok, const char *file, int line, const char *expr, const char *fmt, ...) {
    if (ok) { g_pass++; return; }
    g_fail++; g_cur_fail++;
    printf("  FAIL %s:%d: %s", file, line, expr);
    if (fmt) { va_list a; va_start(a, fmt); printf(" ("); vprintf(fmt, a); printf(")"); va_end(a); }
    printf("\n");
}
void t_note(const char *fmt, ...) { va_list a; va_start(a, fmt); printf("  NOTE "); vprintf(fmt, a); printf("\n"); va_end(a); }
void t_skip(const char *fmt, ...) {
    va_list a; va_start(a, fmt); printf("  SKIP "); vprintf(fmt, a); printf("\n"); va_end(a);
    g_cur_skip = 1;
}

static LONG CALLBACK veh(PEXCEPTION_POINTERS ep) {
    DWORD c = ep->ExceptionRecord->ExceptionCode;
    if (!g_in_test) return EXCEPTION_CONTINUE_SEARCH;
    if (c == 0x40010006 || c == 0x406D1388 || c == DBG_PRINTEXCEPTION_C || c == 0x4001000A) return EXCEPTION_CONTINUE_SEARCH;
    if ((c & 0xF0000000) != 0xC0000000) return EXCEPTION_CONTINUE_SEARCH;
    /* Tests that deliberately raise/catch exceptions disable this with g_in_test=0 via t_expect_exceptions. */
    printf("  FAIL exception 0x%08lx at %p in %s\n", c, ep->ExceptionRecord->ExceptionAddress, g_cur);
    g_fail++; g_cur_fail++;
    g_in_test = 0;
    longjmp(g_jb, 1);
    return EXCEPTION_CONTINUE_SEARCH;
}

/* ---- self / child process ---- */
const char *t_self_path(void) {
    static char p[MAX_PATH * 2];
    if (!p[0]) GetModuleFileNameA(NULL, p, sizeof p);
    return p;
}
HANDLE t_spawn_child(const char *name, const char *extra, DWORD flags, HANDLE hin, HANDLE hout, HANDLE herr, BOOL inherit, const char *env, PROCESS_INFORMATION *pi_out) {
    char cmd[4096];
    STARTUPINFOA si; PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof si); si.cb = sizeof si;
    if (hin || hout || herr) { si.dwFlags |= STARTF_USESTDHANDLES; si.hStdInput = hin; si.hStdOutput = hout; si.hStdError = herr; }
    snprintf(cmd, sizeof cmd, "\"%s\" --child %s %s", t_self_path(), name, extra ? extra : "");
    memset(&pi, 0, sizeof pi);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, inherit, flags, (LPVOID)env, NULL, &si, &pi)) return NULL;
    if (pi_out) *pi_out = pi; else CloseHandle(pi.hThread);
    return pi.hProcess;
}
DWORD t_run_child(const char *name, const char *extra, DWORD timeout_ms) {
    PROCESS_INFORMATION pi; DWORD code = 0xDEAD0000;
    HANDLE h = t_spawn_child(name, extra, 0, NULL, NULL, NULL, FALSE, NULL, &pi);
    if (!h) return 0xDEAD0000;
    if (WaitForSingleObject(h, timeout_ms) != WAIT_OBJECT_0) { TerminateProcess(h, 1); WaitForSingleObject(h, 5000); code = 0xDEAD0001; }
    else GetExitCodeProcess(h, &code);
    CloseHandle(pi.hThread); CloseHandle(h);
    return code;
}

/* ---- scratch directory ---- */
static char g_tmp[MAX_PATH];
static void rmtree(const char *dir) {
    char pat[MAX_PATH + 4], sub[MAX_PATH + 300]; WIN32_FIND_DATAA fd;
    snprintf(pat, sizeof pat, "%s\\*", dir);
    HANDLE h = FindFirstFileA(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
            snprintf(sub, sizeof sub, "%s\\%s", dir, fd.cFileName);
            SetFileAttributesA(sub, FILE_ATTRIBUTE_NORMAL);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) rmtree(sub); else DeleteFileA(sub);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryA(dir);
}
static void cleanup_tmp(void) { if (g_tmp[0]) { char d[MAX_PATH]; strcpy(d, g_tmp); d[strlen(d) - 1] = 0; rmtree(d); g_tmp[0] = 0; } }
const char *t_tmpdir(void) {
    if (!g_tmp[0]) {
        char base[MAX_PATH];
        DWORD n = GetTempPathA(sizeof base, base);
        if (!n) strcpy(base, ".\\");
        snprintf(g_tmp, sizeof g_tmp, "%sk32t_%lu\\", base, GetCurrentProcessId());
        CreateDirectoryA(g_tmp, NULL);
        atexit(cleanup_tmp);
    }
    return g_tmp;
}
void t_tmpfile(char *out, size_t cap, const char *leaf) { snprintf(out, cap, "%s%s", t_tmpdir(), leaf); }

int main(int argc, char **argv) {
    const char *filter = argc > 1 ? argv[1] : NULL;
    int ran = 0, tskip = 0, tfail = 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    if (argc > 2 && !strcmp(argv[1], "--child")) {
        for (int i = 0; i < g_nc; i++) if (!strcmp(g_c[i].n, argv[2])) return g_c[i].f(argc - 2, argv + 2);
        return 0xC0DE0001;
    }
    AddVectoredExceptionHandler(1, veh);
    if (!t_kernel32()) { printf("FATAL cannot load kernel32\n"); return 2; }
    for (int i = 0; i < g_nt_; i++) {
        if (filter && !strstr(g_t[i].n, filter)) continue;
        g_cur = g_t[i].n; g_cur_fail = g_cur_skip = 0;
        printf("RUN  %s\n", g_cur);
        SetLastError(0);
        if (!setjmp(g_jb)) { g_in_test = 1; g_t[i].f(); g_in_test = 0; }
        ran++;
        if (g_cur_fail) tfail++;
        else if (g_cur_skip) tskip++;
        printf("%s %s\n", g_cur_fail ? "FAIL" : g_cur_skip ? "SKIP" : "OK  ", g_cur);
    }
    printf("SUMMARY tests=%d failed=%d skipped=%d checks_pass=%d checks_fail=%d\n", ran, tfail, tskip, g_pass, g_fail);
    return tfail ? 1 : 0;
}
