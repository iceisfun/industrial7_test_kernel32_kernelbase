/* Shared helpers for the process_create area (all static). */
#ifndef PC_H
#define PC_H
#include "t.h"
#include <stdlib.h>
#include <ctype.h>

UDECL(LONG, NtQueryInformationProcess, (HANDLE, ULONG, PVOID, ULONG, PULONG));
UDECL(BOOL, GetSystemCpuSetInformation, (PVOID, ULONG, PULONG, HANDLE, ULONG));
UDECL(BOOL, GetProcessDefaultCpuSets, (HANDLE, PULONG, ULONG, PULONG));
UDECL(BOOL, SetProcessDefaultCpuSets, (HANDLE, const ULONG *, ULONG));
UDECL(LONG, NtQueryObject, (HANDLE, ULONG, PVOID, ULONG, PULONG));

/* single-evaluation equality check (EXPECT_EQ evaluates its arguments twice) */
#define EQV(a, b) do { unsigned long long a_ = (unsigned long long)(ULONG_PTR)(a), b_ = (unsigned long long)(ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", a_, b_); } while (0)
typedef struct { ULONG Attributes, GrantedAccess, HandleCount, PointerCount, PagedPool, NonPagedPool, Res[3], NameInfoSize, TypeInfoSize, SecDescSize; LARGE_INTEGER CreationTime; } PC_OBI;
#define PC_NOTSET ((HANDLE)0)
#define PC_TIMEOUT 25000

typedef struct {
    const WCHAR *app;        /* lpApplicationName (wide) */
    const WCHAR *exe;        /* exe path used when composing the command line (default: self) */
    const char *child, *args;/* "<exe>" --child <child> <args> */
    const WCHAR *raw_cmd;    /* command line used verbatim when non-NULL */
    DWORD flags;
    void *env;
    const WCHAR *cwd;
    STARTUPINFOEXW *si;      /* optional custom startup info */
    BOOL inherit;
    HANDLE hin, hout, herr;
    SECURITY_ATTRIBUTES *psa, *tsa;
    int ansi;                /* use CreateProcessA */
    const char *in_text;     /* capture: text written to child's stdin */
    int sep_err;             /* capture: separate pipe for stderr */
} pc_opts;

static const WCHAR *pc_selfw(void) {
    static WCHAR p[MAX_PATH * 2];
    if (!p[0]) GetModuleFileNameW(NULL, p, MAX_PATH * 2);
    return p;
}

static void pc_widen(WCHAR *dst, size_t cap, const char *s) { MultiByteToWideChar(CP_ACP, 0, s, -1, dst, (int)cap); }

static BOOL pc_launch(const pc_opts *o, PROCESS_INFORMATION *pi) {
    static WCHAR cmdw[40000];
    static char cmda[40000], appa[2000], cwda[2000], titlea[600];
    STARTUPINFOEXW sx; BOOL r; DWORD e;
    memset(&sx, 0, sizeof sx);
    if (o->si) sx = *o->si; else sx.StartupInfo.cb = (o->flags & EXTENDED_STARTUPINFO_PRESENT) ? sizeof(STARTUPINFOEXW) : sizeof(STARTUPINFOW);
    if (o->hin || o->hout || o->herr) {
        sx.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
        sx.StartupInfo.hStdInput = o->hin; sx.StartupInfo.hStdOutput = o->hout; sx.StartupInfo.hStdError = o->herr;
    }
    memset(pi, 0, sizeof *pi);
    if (o->raw_cmd) lstrcpynW(cmdw, o->raw_cmd, 40000);
    else {
        WCHAR a[8192], c[256];
        pc_widen(c, 256, o->child ? o->child : "");
        pc_widen(a, 8192, o->args ? o->args : "");
        wsprintfW(cmdw, L"\"%s\" --child %s %s", o->exe ? o->exe : pc_selfw(), c, a);
    }
    if (!o->ansi) {
        r = UK(CreateProcessW)(o->app, cmdw, o->psa, o->tsa, o->inherit, o->flags, o->env, o->cwd, &sx.StartupInfo, pi);
    } else {
        STARTUPINFOEXA xa; memset(&xa, 0, sizeof xa);
        xa.StartupInfo.dwX = sx.StartupInfo.dwX; xa.StartupInfo.dwY = sx.StartupInfo.dwY;
        xa.StartupInfo.dwXSize = sx.StartupInfo.dwXSize; xa.StartupInfo.dwYSize = sx.StartupInfo.dwYSize;
        xa.StartupInfo.dwXCountChars = sx.StartupInfo.dwXCountChars; xa.StartupInfo.dwYCountChars = sx.StartupInfo.dwYCountChars;
        xa.StartupInfo.dwFillAttribute = sx.StartupInfo.dwFillAttribute;
        xa.StartupInfo.cb = (sx.StartupInfo.cb == sizeof(STARTUPINFOEXW)) ? sizeof(STARTUPINFOEXA) : sizeof(STARTUPINFOA);
        xa.StartupInfo.dwFlags = sx.StartupInfo.dwFlags; xa.StartupInfo.wShowWindow = sx.StartupInfo.wShowWindow;
        xa.StartupInfo.cbReserved2 = sx.StartupInfo.cbReserved2; xa.StartupInfo.lpReserved2 = sx.StartupInfo.lpReserved2;
        xa.StartupInfo.hStdInput = sx.StartupInfo.hStdInput; xa.StartupInfo.hStdOutput = sx.StartupInfo.hStdOutput; xa.StartupInfo.hStdError = sx.StartupInfo.hStdError;
        if (sx.StartupInfo.lpTitle) { WideCharToMultiByte(CP_ACP, 0, sx.StartupInfo.lpTitle, -1, titlea, sizeof titlea, NULL, NULL); xa.StartupInfo.lpTitle = titlea; }
        xa.lpAttributeList = sx.lpAttributeList;
        WideCharToMultiByte(CP_ACP, 0, cmdw, -1, cmda, sizeof cmda, NULL, NULL);
        if (o->app) WideCharToMultiByte(CP_ACP, 0, o->app, -1, appa, sizeof appa, NULL, NULL);
        if (o->cwd) WideCharToMultiByte(CP_ACP, 0, o->cwd, -1, cwda, sizeof cwda, NULL, NULL);
        r = UK(CreateProcessA)(o->app ? appa : NULL, cmda, o->psa, o->tsa, o->inherit, o->flags, o->env, o->cwd ? cwda : NULL, &xa.StartupInfo, pi);
    }
    e = GetLastError();
    SetLastError(e);
    return r;
}

/* Spawn child, wait (bounded), return exit code; 0xDEAD0000 on spawn failure (error preserved), 0xDEAD0001 timeout. */
static DWORD pc_run(const pc_opts *o, DWORD timeout) {
    PROCESS_INFORMATION pi; DWORD code = 0xDEAD0000, e;
    if (!pc_launch(o, &pi)) return 0xDEAD0000;
    if (WaitForSingleObject(pi.hProcess, timeout) != WAIT_OBJECT_0) { TerminateProcess(pi.hProcess, 1); WaitForSingleObject(pi.hProcess, 5000); code = 0xDEAD0001; }
    else GetExitCodeProcess(pi.hProcess, &code);
    e = GetLastError();
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    SetLastError(e);
    return code;
}

static void pc_drain(HANDLE h, char *out, size_t cap) {
    size_t n = 0; DWORD avail, got;
    while (n + 1 < cap && PeekNamedPipe(h, NULL, 0, NULL, &avail, NULL) && avail) {
        if (avail > cap - 1 - n) avail = (DWORD)(cap - 1 - n);
        if (!ReadFile(h, out + n, avail, &got, NULL) || !got) break;
        n += got;
    }
    out[n] = 0;
}

/* Run child with stdout (and optionally stderr / stdin) redirected through pipes. Returns 1 when it was created. */
static int pc_capture(pc_opts *o, char *out, size_t cap, char *errout, size_t ecap, DWORD *code, DWORD timeout) {
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE ro = NULL, wo = NULL, re = NULL, we = NULL, ri = NULL, wi = NULL;
    PROCESS_INFORMATION pi; DWORD c = 0xDEAD0000;
    pc_opts l = *o;
    out[0] = 0; if (errout) errout[0] = 0;
    CreatePipe(&ro, &wo, &sa, 65536); SetHandleInformation(ro, HANDLE_FLAG_INHERIT, 0);
    if (errout) { CreatePipe(&re, &we, &sa, 65536); SetHandleInformation(re, HANDLE_FLAG_INHERIT, 0); }
    if (o->in_text) { CreatePipe(&ri, &wi, &sa, 65536); SetHandleInformation(wi, HANDLE_FLAG_INHERIT, 0); }
    l.hout = wo; l.herr = errout ? we : wo; l.hin = o->in_text ? ri : NULL; l.inherit = TRUE;
    if (!l.hin) { /* give the child a valid stdin so its handle table is predictable */ l.hin = GetStdHandle(STD_INPUT_HANDLE); if (l.hin == INVALID_HANDLE_VALUE) l.hin = NULL; }
    if (l.hin && !o->in_text) { /* only use it if inheritable, else leave NULL */
        DWORD fl = 0; if (!GetHandleInformation(l.hin, &fl) || !(fl & HANDLE_FLAG_INHERIT)) l.hin = NULL;
    }
    if (!pc_launch(&l, &pi)) { DWORD e = GetLastError(); CloseHandle(ro); CloseHandle(wo); if (re) { CloseHandle(re); CloseHandle(we); } if (ri) { CloseHandle(ri); CloseHandle(wi); } if (code) *code = 0xDEAD0000; SetLastError(e); return 0; }
    CloseHandle(wo); if (we) CloseHandle(we); if (ri) CloseHandle(ri);
    if (wi) { DWORD w; WriteFile(wi, o->in_text, (DWORD)strlen(o->in_text), &w, NULL); CloseHandle(wi); }
    if (WaitForSingleObject(pi.hProcess, timeout) != WAIT_OBJECT_0) { TerminateProcess(pi.hProcess, 1); WaitForSingleObject(pi.hProcess, 5000); c = 0xDEAD0001; }
    else GetExitCodeProcess(pi.hProcess, &c);
    pc_drain(ro, out, cap); if (errout) pc_drain(re, errout, ecap);
    CloseHandle(ro); if (re) CloseHandle(re);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    if (code) *code = c;
    return 1;
}

/* Convenience: run a child by name, capture stdout. */
static DWORD pc_cap(const char *child, const char *args, DWORD flags, char *out, size_t cap) {
    pc_opts o; DWORD code = 0xDEAD0000;
    memset(&o, 0, sizeof o); o.child = child; o.args = args; o.flags = flags;
    pc_capture(&o, out, cap, NULL, 0, &code, PC_TIMEOUT);
    return code;
}

/* key=value line lookup in captured output */
static const char *pc_val(const char *out, const char *key) {
    size_t kl = strlen(key); const char *p = out;
    while (p && *p) {
        if (!strncmp(p, key, kl) && p[kl] == '=') return p + kl + 1;
        p = strchr(p, '\n'); if (p) p++;
    }
    return NULL;
}
static long long pc_num(const char *out, const char *key, long long def) {
    const char *v = pc_val(out, key); if (!v) return def;
    return strtoll(v, NULL, 0);
}
static unsigned long long pc_unum(const char *out, const char *key, unsigned long long def) {
    const char *v = pc_val(out, key); if (!v) return def;
    return strtoull(v, NULL, 0);
}
static int pc_streq(const char *out, const char *key, const char *want) {
    const char *v = pc_val(out, key); size_t n = strlen(want);
    return v && !strncmp(v, want, n) && (v[n] == '\n' || v[n] == '\r' || v[n] == 0);
}
static void pc_line(const char *out, const char *key, char *dst, size_t cap) {
    const char *v = pc_val(out, key); size_t n = 0;
    if (v) while (v[n] && v[n] != '\n' && v[n] != '\r' && n + 1 < cap) { dst[n] = v[n]; n++; }
    dst[n] = 0;
}

/* Run a self-checking child scenario: it prints fail=.. / note=.. lines and returns 0 on success. */
static void pc_scenario_opts(pc_opts *o) {
    static char out[16384]; DWORD code;
    int ok = pc_capture(o, out, sizeof out, NULL, 0, &code, PC_TIMEOUT);
    CHECKF(ok, "could not spawn scenario child (err %lu)", GetLastError());
    if (!ok) return;
    const char *p = out;
    while (*p) {
        const char *nl = strchr(p, '\n'); size_t n = nl ? (size_t)(nl - p) : strlen(p);
        char ln[512]; if (n >= sizeof ln) n = sizeof ln - 1; memcpy(ln, p, n); ln[n] = 0;
        if (!strncmp(ln, "fail=", 5)) t_check(0, "child", 0, ln + 5, NULL);
        else if (!strncmp(ln, "note=", 5)) t_note("%s", ln + 5);
        p = nl ? nl + 1 : p + n;
    }
    if (pc_val(out, "skip")) { t_skip("%s", pc_val(out, "skip")); return; }
    CHECKF(code == 0, "scenario child exit code 0x%lx", code);
}
static void pc_scenario(const char *name, const char *args) {
    pc_opts o; memset(&o, 0, sizeof o); o.child = name; o.args = args; pc_scenario_opts(&o);
}

/* Create a copy of the test exe at tmpdir\<rel> (creating directories). Returns wide full path in out. */
static BOOL pc_copy_self(const char *rel, WCHAR *outw, size_t cap) {
    char full[MAX_PATH * 2], dir[MAX_PATH * 2]; char *s;
    snprintf(full, sizeof full, "%s%s", t_tmpdir(), rel);
    strcpy(dir, full);
    for (s = dir + strlen(t_tmpdir()); *s; s++) if (*s == '\\') { *s = 0; CreateDirectoryA(dir, NULL); *s = '\\'; }
    if (!CopyFileA(t_self_path(), full, FALSE)) return FALSE;
    if (outw) MultiByteToWideChar(CP_ACP, 0, full, -1, outw, (int)cap);
    return TRUE;
}

/* Create suspended child running a given child entry; caller owns handles. */
static BOOL pc_suspended(const char *child, const char *args, DWORD extra_flags, PROCESS_INFORMATION *pi) {
    pc_opts o; memset(&o, 0, sizeof o); o.child = child; o.args = args; o.flags = CREATE_SUSPENDED | extra_flags;
    return pc_launch(&o, pi);
}
static void pc_kill(PROCESS_INFORMATION *pi) {
    if (pi->hProcess) { TerminateProcess(pi->hProcess, 0x4B1D); WaitForSingleObject(pi->hProcess, 5000); }
    if (pi->hThread) CloseHandle(pi->hThread);
    if (pi->hProcess) CloseHandle(pi->hProcess);
    memset(pi, 0, sizeof *pi);
}

/* child-side output */
static void pc_out(const char *fmt, ...) {
    char b[8192]; va_list a; DWORD w; int n;
    va_start(a, fmt); n = vsnprintf(b, sizeof b, fmt, a); va_end(a);
    if (n < 0) return;
    if (n >= (int)sizeof b) { n = sizeof b - 1; b[n - 1] = '\n'; }
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), b, n, &w, NULL);
}
static void pc_outw(const char *key, const WCHAR *s) {
    char b[8192]; int n = WideCharToMultiByte(CP_UTF8, 0, s ? s : L"", -1, b, sizeof b, NULL, NULL);
    if (n <= 0) b[0] = 0;
    pc_out("%s=%s\n", key, b);
}

/* shorthand for the very common "spawn and get exit code" */
static DWORD pc_exit_of(const char *child, const char *args, DWORD flags) {
    pc_opts o; memset(&o, 0, sizeof o); o.child = child; o.args = args; o.flags = flags;
    return pc_run(&o, PC_TIMEOUT);
}

#endif
