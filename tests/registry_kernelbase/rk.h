/* Shared helpers for the registry_kernelbase area (scaffolding only; the APIs under test are
 * always called through U()/UK()/UB()/UA() pointers). All writes go below HKCU\Software\k32t_<pid>. */
#ifndef RK_H
#define RK_H
#include "t.h"
#include <winreg.h>
#include <stdlib.h>
#include <wchar.h>

#ifndef RRF_RT_ANY
#define RRF_RT_REG_NONE 0x1
#define RRF_RT_REG_SZ 0x2
#define RRF_RT_REG_EXPAND_SZ 0x4
#define RRF_RT_REG_BINARY 0x8
#define RRF_RT_REG_DWORD 0x10
#define RRF_RT_REG_MULTI_SZ 0x20
#define RRF_RT_REG_QWORD 0x40
#define RRF_RT_DWORD 0x18
#define RRF_RT_QWORD 0x48
#define RRF_RT_ANY 0xffff
#define RRF_NOEXPAND 0x10000000
#define RRF_ZEROONFAILURE 0x20000000
#endif
#ifndef RRF_SUBKEY_WOW6464KEY
#define RRF_SUBKEY_WOW6464KEY 0x10000
#define RRF_SUBKEY_WOW6432KEY 0x20000
#endif

/* single-evaluation equality (EXPECT_EQ re-evaluates its arguments when formatting) */
#define EQ(a, b) do { unsigned long long a_ = (unsigned long long)(ULONG_PTR)(a), b_ = (unsigned long long)(ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", a_, b_); } while (0)
#define WSZ(s) ((DWORD)sizeof(s))            /* bytes incl. NUL for a wide array/literal */
#define WLEN(s) ((DWORD)(sizeof(s) / 2 - 1)) /* chars excl. NUL */

static FARPROC rk_adv(const char *n) {
    FARPROC p = t_proc(n);
    if (!p) { HMODULE m = LoadLibraryA("advapi32.dll"); if (m) p = GetProcAddress(m, n); }
    return p;
}
/* kernelbase/kernel32 first, then advapi32 (legacy exports that never moved) */
#define UA(name) ((__typeof__(&name))rk_adv(#name))

static HKEY g_rk_root;
static char g_rk_root_name[64];
static void rk_cleanup(void) {
    if (g_rk_root) { RegCloseKey(g_rk_root); g_rk_root = NULL; }
    if (g_rk_root_name[0]) RegDeleteTreeA(HKEY_CURRENT_USER, g_rk_root_name);
}
static HKEY rk_root(void) {
    if (!g_rk_root) {
        snprintf(g_rk_root_name, sizeof g_rk_root_name, "Software\\k32t_%lu", GetCurrentProcessId());
        RegDeleteTreeA(HKEY_CURRENT_USER, g_rk_root_name);
        if (RegCreateKeyExA(HKEY_CURRENT_USER, g_rk_root_name, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &g_rk_root, NULL) != 0)
            g_rk_root = NULL;
        atexit(rk_cleanup);
    }
    return g_rk_root;
}
/* Fresh empty subkey of the root, named after the calling test. KEY_ALL_ACCESS. */
static HKEY rk_fresh(const char *name) {
    HKEY r = rk_root(), h = NULL;
    if (!r) return NULL;
    RegDeleteTreeA(r, name);
    if (RegCreateKeyExA(r, name, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &h, NULL) != 0) return NULL;
    return h;
}
#define FRESH() rk_fresh(__func__)
/* Path (relative to HKCU) of a fresh key, for tests that need the name */
static void rk_path(char *out, size_t cap, const char *name) { snprintf(out, cap, "%s\\%s", g_rk_root_name, name); }
static void rk_pathw(WCHAR *out, size_t cap, const char *name) {
    char a[256]; rk_path(a, sizeof a, name); MultiByteToWideChar(CP_ACP, 0, a, -1, out, (int)cap);
}

/* scaffolding setters (direct calls) */
static void sc_sz(HKEY h, const WCHAR *n, const WCHAR *v) { RegSetValueExW(h, n, 0, REG_SZ, (const BYTE *)v, (DWORD)((lstrlenW(v) + 1) * 2)); }
static void sc_dw(HKEY h, const WCHAR *n, DWORD v) { RegSetValueExW(h, n, 0, REG_DWORD, (const BYTE *)&v, 4); }
static void sc_raw(HKEY h, const WCHAR *n, DWORD t, const void *d, DWORD cb) { RegSetValueExW(h, n, 0, t, (const BYTE *)d, cb); }
static HKEY sc_sub(HKEY h, const WCHAR *n) { HKEY k = NULL; RegCreateKeyExW(h, n, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL); return k; }
static void sc_subc(HKEY h, const WCHAR *n) { HKEY k = sc_sub(h, n); if (k) RegCloseKey(k); }

/* NtSetValueKey to store values the Win32 layer would normalise (unterminated strings, ...) */
typedef LONG (NTAPI *rk_NtSetValueKey_t)(HANDLE, PUNICODE_STRING, ULONG, ULONG, PVOID, ULONG);
static LONG rk_ntset(HKEY h, const WCHAR *name, ULONG type, const void *data, ULONG cb) {
    rk_NtSetValueKey_t f = (rk_NtSetValueKey_t)t_proc_nt("NtSetValueKey");
    UNICODE_STRING us; if (!f) return -1;
    us.Buffer = (PWSTR)name; us.Length = (USHORT)(name ? lstrlenW(name) * 2 : 0); us.MaximumLength = us.Length;
    return f((HANDLE)h, &us, 0, type, (PVOID)data, cb);
}
static int rk_exists(HKEY h, const char *sub) { HKEY k; if (RegOpenKeyExA(h, sub, 0, KEY_READ, &k) == 0) { RegCloseKey(k); return 1; } return 0; }
static int rk_existsw(HKEY h, const WCHAR *sub) { HKEY k; if (RegOpenKeyExW(h, sub, 0, KEY_READ, &k) == 0) { RegCloseKey(k); return 1; } return 0; }
static DWORD rk_subcount(HKEY h) { DWORD n = 0; RegQueryInfoKeyW(h, NULL, NULL, NULL, &n, NULL, NULL, NULL, NULL, NULL, NULL, NULL); return n; }
static DWORD rk_valcount(HKEY h) { DWORD n = 0; RegQueryInfoKeyW(h, NULL, NULL, NULL, NULL, NULL, NULL, &n, NULL, NULL, NULL, NULL); return n; }
static void rk_fill_w(WCHAR *b, size_t n, WCHAR c) { for (size_t i = 0; i < n; i++) b[i] = c; b[n] = 0; }
#endif
