/* gap_11 shared helpers (static, scaffolding only). Everything under test goes through U()/UK()/UB(). */
#ifndef G11_H
#define G11_H
#include "t.h"
#include <shlwapi.h>
#include <wincon.h>
#include <stdlib.h>
#include <wchar.h>

/* single-evaluation equality */
#define EQ(a, b) do { unsigned long long a_ = (unsigned long long)(ULONG_PTR)(a), b_ = (unsigned long long)(ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", a_, b_); } while (0)
#define NE(a, b) do { unsigned long long a_ = (unsigned long long)(ULONG_PTR)(a), b_ = (unsigned long long)(ULONG_PTR)(b); \
    t_check(a_ != b_, __FILE__, __LINE__, #a " != " #b, "got 0x%llx", a_); } while (0)
#define WSZ(s) ((DWORD)sizeof(s))

UDECL(LONG, SHRegCloseUSKey, (HUSKEY));
#define pClose ((SHRegCloseUSKey_pfn)t_proc("SHRegCloseUSKey"))

/* private registry root: HKCU\Software\k32t_<pid>_g11 */
static const char *g11_root(void) {
    static char r[96];
    if (!r[0]) snprintf(r, sizeof r, "Software\\k32t_%lu_g11", GetCurrentProcessId());
    return r;
}
static void g11_sub(char *out, size_t cap, const char *leaf) {
    if (leaf && *leaf) snprintf(out, cap, "%s\\%s", g11_root(), leaf); else snprintf(out, cap, "%s", g11_root());
}
static void g11_subw(WCHAR *out, size_t cap, const char *leaf) {
    char a[200]; g11_sub(a, sizeof a, leaf);
    MultiByteToWideChar(CP_ACP, 0, a, -1, out, (int)cap);
}
static void g11_clean(void) { RegDeleteTreeA(HKEY_CURRENT_USER, g11_root()); }
static void g11_w(WCHAR *out, size_t cap, const char *a) { MultiByteToWideChar(CP_ACP, 0, a, -1, out, (int)cap); }
/* scaffolding: create a value with the wide registry API, so the bytes are stored exactly as given */
static LONG g11_mk(const char *leaf, const char *name, DWORD type, const void *data, DWORD cb) {
    WCHAR p[256], n[256]; HKEY k; LONG r;
    g11_subw(p, 256, leaf);
    r = RegCreateKeyExW(HKEY_CURRENT_USER, p, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL);
    if (r) return r;
    if (name) { g11_w(n, 256, name); r = RegSetValueExW(k, n, 0, type, (const BYTE *)data, cb); }
    RegCloseKey(k);
    return r;
}
static LONG g11_mkdw(const char *leaf, const char *name, DWORD v) { return g11_mk(leaf, name, REG_DWORD, &v, 4); }
/* ANSI text stored as a REG_SZ (UTF-16 in the registry) */
static LONG g11_mksz(const char *leaf, const char *name, const char *s) {
    WCHAR w[512]; int n = MultiByteToWideChar(CP_ACP, 0, s, -1, w, 512);
    return g11_mk(leaf, name, REG_SZ, w, (DWORD)n * 2);
}
/* scaffolding: read a raw value via the wide API */
static LONG g11_rd(const char *leaf, const char *name, DWORD *type, void *buf, DWORD *cb) {
    WCHAR p[256], n[256]; HKEY k; LONG r;
    g11_subw(p, 256, leaf); g11_w(n, 256, name);
    r = RegOpenKeyExW(HKEY_CURRENT_USER, p, 0, KEY_READ, &k);
    if (r) return r;
    r = RegQueryValueExW(k, n, NULL, type, (LPBYTE)buf, cb);
    RegCloseKey(k);
    return r;
}
#endif
