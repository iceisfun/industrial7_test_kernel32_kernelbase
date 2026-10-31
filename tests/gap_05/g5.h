/* Shared helpers for the gap_05 area (static, header-only). */
#ifndef G5_H
#define G5_H
#include "t.h"
#include <psapi.h>
#include <winver.h>
#include <sddl.h>
#include <ctype.h>

/* Resolve through kernel32 -> kernelbase -> ntdll, then the K32 alias, then the classic
 * helper DLLs (only matters on systems whose kernelbase lacks the plain names). */
static FARPROC g5_proc(const char *name) {
    FARPROC p = t_proc(name);
    if (p) return p;
    char k[128]; snprintf(k, sizeof k, "K32%s", name);
    p = t_proc(k);
    if (p) return p;
    static const char *libs[] = { "psapi.dll", "version.dll", NULL };
    for (int i = 0; libs[i]; i++) {
        HMODULE m = LoadLibraryA(libs[i]);
        if (m && (p = GetProcAddress(m, name))) return p;
    }
    return NULL;
}
#define G(name) ((__typeof__(&name))g5_proc(#name))

/* Last path component of a path. */
static const char *g5_base(const char *p) {
    const char *s = strrchr(p, '\\');
    return s ? s + 1 : p;
}
static void g5_self_base(char *out, size_t cap) {
    char p[MAX_PATH]; GetModuleFileNameA(NULL, p, sizeof p);
    snprintf(out, cap, "%s", g5_base(p));
}
static int g5_w2a(const WCHAR *w, char *a, int cap) {
    int n = WideCharToMultiByte(CP_ACP, 0, w, -1, a, cap, NULL, NULL);
    if (!n && cap) a[0] = 0;
    return n;
}
static void g5_a2w(const char *a, WCHAR *w, int cap) { MultiByteToWideChar(CP_ACP, 0, a, -1, w, cap); }

/* A helper child that publishes "ready" and then waits for "quit" (both named events). */
static HANDLE g5_spawn_waiter(HANDLE *ready, HANDLE *quit, DWORD *pid) {
    char n1[64], n2[64], args[200];
    static int ctr;
    int id = ++ctr;
    snprintf(n1, sizeof n1, "k32t_g5r_%lu_%d", GetCurrentProcessId(), id);
    snprintf(n2, sizeof n2, "k32t_g5q_%lu_%d", GetCurrentProcessId(), id);
    *ready = CreateEventA(NULL, TRUE, FALSE, n1);
    *quit = CreateEventA(NULL, TRUE, FALSE, n2);
    snprintf(args, sizeof args, "%s %s", n1, n2);
    PROCESS_INFORMATION pi;
    HANDLE h = t_spawn_child("gap5_waiter", args, 0, NULL, NULL, NULL, FALSE, NULL, &pi);
    if (!h) { CloseHandle(*ready); CloseHandle(*quit); *ready = *quit = NULL; return NULL; }
    *pid = pi.dwProcessId;
    CloseHandle(pi.hThread);
    if (WaitForSingleObject(*ready, 15000) != WAIT_OBJECT_0) {
        TerminateProcess(h, 1); CloseHandle(h); CloseHandle(*ready); CloseHandle(*quit);
        *ready = *quit = NULL; return NULL;
    }
    return h;
}
static void g5_end_waiter(HANDLE h, HANDLE ready, HANDLE quit) {
    SetEvent(quit);
    if (WaitForSingleObject(h, 10000) != WAIT_OBJECT_0) TerminateProcess(h, 1);
    CloseHandle(h); CloseHandle(ready); CloseHandle(quit);
}
#define WINDOWS_ONLY() do { if (t_is_wine()) { t_skip("emulator differs; windows-specific expectations"); return; } } while (0)
#endif
