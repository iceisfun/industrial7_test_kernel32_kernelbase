/* GetFileVersionInfo{,Ex}{A,W}, GetFileVersionInfoSize{,Ex}{A,W} */
#include "g5.h"

#define FV_LOCALISED 1
#define FV_NEUTRAL 2

static void sysdll(char *out, size_t cap, const char *name) {
    char d[MAX_PATH]; GetSystemDirectoryA(d, sizeof d);
    snprintf(out, cap, "%s\\%s", d, name);
}
static void sysdllw(WCHAR *out, const char *name) { char a[MAX_PATH]; sysdll(a, sizeof a, name); g5_a2w(a, out, MAX_PATH); }

typedef DWORD (WINAPI *szA_t)(LPCSTR, LPDWORD);
typedef DWORD (WINAPI *szW_t)(LPCWSTR, LPDWORD);
typedef DWORD (WINAPI *szExA_t)(DWORD, LPCSTR, LPDWORD);
typedef DWORD (WINAPI *szExW_t)(DWORD, LPCWSTR, LPDWORD);
typedef BOOL (WINAPI *giA_t)(LPCSTR, DWORD, DWORD, LPVOID);
typedef BOOL (WINAPI *giW_t)(LPCWSTR, DWORD, DWORD, LPVOID);
typedef BOOL (WINAPI *giExA_t)(DWORD, LPCSTR, DWORD, DWORD, LPVOID);
typedef BOOL (WINAPI *giExW_t)(DWORD, LPCWSTR, DWORD, DWORD, LPVOID);
#define P_szA ((szA_t)g5_proc("GetFileVersionInfoSizeA"))
#define P_szW ((szW_t)g5_proc("GetFileVersionInfoSizeW"))
#define P_szExA ((szExA_t)g5_proc("GetFileVersionInfoSizeExA"))
#define P_szExW ((szExW_t)g5_proc("GetFileVersionInfoSizeExW"))
#define P_giA ((giA_t)g5_proc("GetFileVersionInfoA"))
#define P_giW ((giW_t)g5_proc("GetFileVersionInfoW"))
#define P_giExA ((giExA_t)g5_proc("GetFileVersionInfoExA"))
#define P_giExW ((giExW_t)g5_proc("GetFileVersionInfoExW"))

#define NEED_ALL() do { if (!P_szA || !P_szW || !P_szExA || !P_szExW || !P_giA || !P_giW || !P_giExA || !P_giExW) { t_skip("missing version exports"); return; } } while (0)

TEST(gap5_ver_exports_present) {
    static const char *n[] = { "GetFileVersionInfoA", "GetFileVersionInfoW", "GetFileVersionInfoExA", "GetFileVersionInfoExW",
        "GetFileVersionInfoSizeA", "GetFileVersionInfoSizeW", "GetFileVersionInfoSizeExA", "GetFileVersionInfoSizeExW" };
    for (unsigned i = 0; i < sizeof n / sizeof *n; i++) CHECKF(g5_proc(n[i]) != NULL, "%s", n[i]);
    /* On Win10 these live in kernelbase.dll (version.dll forwards to it). */
    if (!t_is_wine()) for (unsigned i = 0; i < sizeof n / sizeof *n; i++) CHECKF(t_proc_kb(n[i]) != NULL, "kernelbase!%s", n[i]);
}

/* Fetch version block of a system DLL into a malloc'd buffer. */
static BYTE *getblock(const char *dll, DWORD *sz) {
    char p[MAX_PATH]; sysdll(p, sizeof p, dll);
    DWORD h = 0xCCCCCCCC;
    DWORD n = P_szA(p, &h);
    if (!n) return NULL;
    BYTE *b = calloc(1, n + 16);
    if (!P_giA(p, 0, n, b)) { free(b); return NULL; }
    *sz = n;
    return b;
}

TEST(gap5_ver_size_basic) {
    NEED_ALL();
    char p[MAX_PATH]; sysdll(p, sizeof p, "kernel32.dll");
    DWORD h = 0xCCCCCCCC; SET_ERR(0xAAAA);
    DWORD n = P_szA(p, &h);
    CHECK(n >= 0x5c * 2 + 4);
    CHECK(n <= 0x8000 * 2 + 4);
    EXPECT_EQ(h, 0);                       /* handle is always zeroed */
    EXPECT_EQ(n % 4, 0);                   /* aligned*2+4 */
    EXPECT_ERR(0);                         /* success resets last error */
}
TEST(gap5_ver_size_null_handle_ptr) {
    NEED_ALL();
    char p[MAX_PATH]; sysdll(p, sizeof p, "kernel32.dll");
    CHECK(P_szA(p, NULL) > 0);
    WCHAR w[MAX_PATH]; sysdllw(w, "kernel32.dll");
    CHECK(P_szW(w, NULL) > 0);
    EXPECT_EQ(P_szA(p, NULL), P_szW(w, NULL));
}
TEST(gap5_ver_size_handle_zeroed_on_failure) {
    NEED_ALL();
    DWORD h = 0x12345678; SET_ERR(0);
    CHECK(P_szA("C:\\k32t_no_such_dir_\\nofile.dll", &h) == 0);
    EXPECT_EQ(h, 0);
    h = 0x12345678;
    CHECK(P_szW(L"C:\\k32t_no_such_dir_\\nofile.dll", &h) == 0);
    EXPECT_EQ(h, 0);
}
TEST(gap5_ver_size_a_w_ex_parity) {
    WINDOWS_ONLY();
    NEED_ALL();
    static const char *dlls[] = { "kernel32.dll", "ntdll.dll", "kernelbase.dll", "advapi32.dll", "cmd.exe", "notepad.exe" };
    for (unsigned i = 0; i < sizeof dlls / sizeof *dlls; i++) {
        char p[MAX_PATH]; WCHAR w[MAX_PATH];
        sysdll(p, sizeof p, dlls[i]); g5_a2w(p, w, MAX_PATH);
        DWORD a = P_szA(p, NULL), b = P_szW(w, NULL), c = P_szExA(FV_LOCALISED, p, NULL), d = P_szExW(FV_LOCALISED, w, NULL);
        if (!a && GetFileAttributesA(p) == INVALID_FILE_ATTRIBUTES) continue;
        CHECKF(a > 0, "%s", dlls[i]);
        CHECKF(a == b && b == c && c == d, "%s: %lu %lu %lu %lu", dlls[i], a, b, c, d);
    }
}
TEST(gap5_ver_size_flags) {
    NEED_ALL();
    WCHAR w[MAX_PATH]; sysdllw(w, "kernel32.dll");
    DWORD l = P_szExW(FV_LOCALISED, w, NULL), nu = P_szExW(FV_NEUTRAL, w, NULL), z = P_szExW(0, w, NULL), both = P_szExW(3, w, NULL);
    CHECK(l > 0 && nu > 0 && z > 0 && both > 0);
    /* only bit 0 is consulted by the size query */
    EXPECT_EQ(nu, z);
    EXPECT_EQ(both, l);
    EXPECT_EQ(P_szExW(0xFFFFFFFE, w, NULL), z);
    EXPECT_EQ(P_szExW(0xFFFFFFFF, w, NULL), l);
    EXPECT_EQ(P_szExW(0x4, w, NULL), z);
    EXPECT_EQ(P_szExW(0x5, w, NULL), l);
    EXPECT_EQ(P_szW(w, NULL), l);   /* the plain query is the localized one */
    if (l != nu) t_note("localized size %lu vs neutral %lu", l, nu);
}
TEST(gap5_ver_size_nonexistent) {
    NEED_ALL();
    DWORD h = 1;
    SET_ERR(0); EXPECT_EQ(P_szA("C:\\Windows\\k32t_nonexistent_file.dll", &h), 0); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    SET_ERR(0); EXPECT_EQ(P_szW(L"C:\\Windows\\k32t_nonexistent_file.dll", &h), 0); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    SET_ERR(0); EXPECT_EQ(P_szExA(FV_NEUTRAL, "C:\\Windows\\k32t_nonexistent_file.dll", &h), 0); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    SET_ERR(0); EXPECT_EQ(P_szExW(FV_NEUTRAL, L"C:\\Windows\\k32t_nonexistent_file.dll", &h), 0); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    SET_ERR(0); EXPECT_EQ(P_szW(L"k32t_nonexistent_file.dll", &h), 0); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(gap5_ver_size_missing_directory) {
    NEED_ALL();
    SET_ERR(0);
    DWORD r = P_szW(L"C:\\k32t_no_such_dir_\\sub\\x.dll", NULL);
    EXPECT_EQ(r, 0);
    t_note("size on missing directory -> error %lu", GetLastError());
}
TEST(gap5_ver_size_empty_and_null_name) {
    NEED_ALL();
    SET_ERR(0); EXPECT_EQ(P_szA("", NULL), 0); t_note("size(\"\") err=%lu", GetLastError());
    SET_ERR(0); EXPECT_EQ(P_szW(L"", NULL), 0); t_note("size(L\"\") err=%lu", GetLastError());
    SET_ERR(0); EXPECT_EQ(P_szA(NULL, NULL), 0); t_note("sizeA(NULL) err=%lu", GetLastError());
}
TEST(gap5_ver_size_directory) {
    NEED_ALL();
    char d[MAX_PATH]; GetSystemDirectoryA(d, sizeof d);
    SET_ERR(0);
    EXPECT_EQ(P_szA(d, NULL), 0);
    t_note("size(directory) err=%lu", GetLastError());
}
TEST(gap5_ver_size_text_file) {
    NEED_ALL();
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "gap5_text.dll");
    HANDLE f = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) { t_skip("cannot create"); return; }
    DWORD w; char junk[4096]; memset(junk, 'x', sizeof junk); WriteFile(f, junk, sizeof junk, &w, NULL); CloseHandle(f);
    DWORD h = 99; SET_ERR(0);
    EXPECT_EQ(P_szA(p, &h), 0);
    EXPECT_EQ(h, 0);
    t_note("size(non-PE file) err=%lu", GetLastError());
    BYTE buf[512]; SET_ERR(0);
    EXPECT_EQ(P_giA(p, 0, sizeof buf, buf), 0);
    DeleteFileA(p);
}
TEST(gap5_ver_size_empty_file) {
    NEED_ALL();
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "gap5_empty.dll");
    HANDLE f = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) { t_skip("cannot create"); return; }
    CloseHandle(f);
    SET_ERR(0);
    EXPECT_EQ(P_szA(p, NULL), 0);
    t_note("size(empty file) err=%lu", GetLastError());
    DeleteFileA(p);
}
TEST(gap5_ver_size_no_version_resource) {
    WINDOWS_ONLY();
    NEED_ALL();
    /* The test executable (mingw) normally carries no VERSIONINFO. If it does, skip. */
    DWORD h = 5; SET_ERR(0);
    DWORD n = P_szA(t_self_path(), &h);
    if (n) { t_skip("test exe has a version resource"); return; }
    EXPECT_EQ(h, 0);
    EXPECT_ERR(ERROR_RESOURCE_TYPE_NOT_FOUND);
    BYTE buf[512]; SET_ERR(0);
    EXPECT_EQ(P_giA(t_self_path(), 0, sizeof buf, buf), 0);
}
TEST(gap5_ver_size_copy_in_tmp) {
    NEED_ALL();
    char src[MAX_PATH], dst[MAX_PATH]; sysdll(src, sizeof src, "kernel32.dll"); t_tmpfile(dst, sizeof dst, "gap5_copy.dll");
    if (!CopyFileA(src, dst, FALSE)) { t_skip("copy failed"); return; }
    CHECK(P_szA(dst, NULL) > 0);
    EXPECT_EQ(P_szA(dst, NULL), P_szA(src, NULL));
    DeleteFileA(dst);
}
TEST(gap5_ver_size_extended_prefix) {
    NEED_ALL();
    char src[MAX_PATH], pre[MAX_PATH + 8]; sysdll(src, sizeof src, "kernel32.dll");
    snprintf(pre, sizeof pre, "\\\\?\\%s", src);
    DWORD a = P_szA(pre, NULL);
    if (!a) t_note("size(\\\\?\\ path) failed err=%lu", GetLastError());
    else EXPECT_EQ(a, P_szA(src, NULL));
}
TEST(gap5_ver_size_case_and_slashes) {
    NEED_ALL();
    char src[MAX_PATH]; sysdll(src, sizeof src, "kernel32.dll");
    char up[MAX_PATH]; strcpy(up, src); for (char *c = up; *c; c++) *c = toupper(*c);
    EXPECT_EQ(P_szA(up, NULL), P_szA(src, NULL));
    char sl[MAX_PATH]; strcpy(sl, src); for (char *c = sl; *c; c++) if (*c == '\\') *c = '/';
    DWORD s = P_szA(sl, NULL);
    t_note("size with forward slashes: %lu (backslash: %lu)", s, P_szA(src, NULL));
}
TEST(gap5_ver_size_bare_name_searched) {
    NEED_ALL();
    DWORD n = P_szA("kernel32.dll", NULL);
    t_note("size(\"kernel32.dll\") bare name -> %lu err=%lu", n, GetLastError());
}
TEST(gap5_ver_size_repeatable) {
    NEED_ALL();
    WCHAR w[MAX_PATH]; sysdllw(w, "kernel32.dll");
    DWORD first = P_szW(w, NULL);
    for (int i = 0; i < 200; i++) if (P_szW(w, NULL) != first) { CHECK(0); break; }
}
TEST(gap5_ver_size_does_not_leak_module) {
    NEED_ALL();
    /* The call loads the file as a datafile and frees it: a name that is not otherwise loaded must not be loaded afterwards. */
    char p[MAX_PATH]; sysdll(p, sizeof p, "wmp.dll");
    if (GetFileAttributesA(p) == INVALID_FILE_ATTRIBUTES) sysdll(p, sizeof p, "mstscax.dll");
    if (GetFileAttributesA(p) == INVALID_FILE_ATTRIBUTES) { t_skip("no candidate dll"); return; }
    HMODULE before = GetModuleHandleA(g5_base(p));
    P_szA(p, NULL);
    HMODULE after = GetModuleHandleA(g5_base(p));
    EXPECT_EQ(before == NULL, after == NULL);
}

/* ---------------- GetFileVersionInfo ---------------- */
TEST(gap5_ver_info_layout) {
    NEED_ALL();
    DWORD sz = 0; BYTE *b = getblock("kernel32.dll", &sz);
    if (!b) { CHECK(0); return; }
    WORD len = *(WORD *)b, vlen = *(WORD *)(b + 2), type = *(WORD *)(b + 4);
    CHECK(len >= 0x5c && len <= sz);
    EXPECT_EQ(len % 4, 0);
    EXPECT_EQ(vlen, 52);             /* sizeof(VS_FIXEDFILEINFO) */
    EXPECT_EQ(type, 0);              /* binary */
    CHECK(!memcmp(b + 6, L"VS_VERSION_INFO", 32));
    EXPECT_EQ(*(DWORD *)(b + 0x28), 0xFEEF04BD);
    EXPECT_EQ(*(DWORD *)(b + len), 0x58324546);   /* "FE2X" tail marker after the block */
    EXPECT_EQ(sz, (DWORD)len * 2 + 4);            /* size query = wLength*2 + 4 */
    free(b);
}
TEST(gap5_ver_info_fixed_info_values) {
    NEED_ALL();
    if (t_is_wine()) { t_skip("windows-specific version values"); return; }
    DWORD sz = 0; BYTE *b = getblock("kernel32.dll", &sz);
    if (!b) { CHECK(0); return; }
    VS_FIXEDFILEINFO *f = (VS_FIXEDFILEINFO *)(b + 0x28);
    EXPECT_EQ(f->dwSignature, 0xFEEF04BD);
    EXPECT_EQ(f->dwStrucVersion, 0x00010000);
    EXPECT_EQ(HIWORD(f->dwFileVersionMS), 10);
    EXPECT_EQ(HIWORD(f->dwProductVersionMS), 10);
    EXPECT_EQ(f->dwFileType, VFT_DLL);
    EXPECT_EQ(f->dwFileSubtype, 0);
    EXPECT_EQ(f->dwFileOS, VOS_NT_WINDOWS32);
    EXPECT_EQ(f->dwFileFlagsMask, 0x3F);
    free(b);
}
TEST(gap5_ver_info_exe_type) {
    NEED_ALL();
    if (t_is_wine()) { t_skip("windows-specific"); return; }
    DWORD sz = 0; BYTE *b = getblock("cmd.exe", &sz);
    if (!b) { t_skip("no cmd.exe version"); return; }
    VS_FIXEDFILEINFO *f = (VS_FIXEDFILEINFO *)(b + 0x28);
    EXPECT_EQ(f->dwSignature, 0xFEEF04BD);
    EXPECT_EQ(f->dwFileType, VFT_APP);
    free(b);
}
TEST(gap5_ver_info_a_w_parity) {
    NEED_ALL();
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; sysdll(p, sizeof p, "kernel32.dll"); g5_a2w(p, w, MAX_PATH);
    DWORD n = P_szW(w, NULL);
    BYTE *a = calloc(1, n), *b = calloc(1, n), *c = calloc(1, n), *d = calloc(1, n);
    SET_ERR(0xBEEF);
    CHECK(P_giA(p, 0, n, a)); EXPECT_ERR(0);
    CHECK(P_giW(w, 0, n, b));
    CHECK(P_giExA(3, p, 0, n, c));
    CHECK(P_giExW(3, w, 0, n, d));
    CHECK(!memcmp(a, b, n));
    CHECK(!memcmp(a, c, n));
    CHECK(!memcmp(a, d, n));
    free(a); free(b); free(c); free(d);
}
TEST(gap5_ver_info_ignores_handle_param) {
    NEED_ALL();
    WCHAR w[MAX_PATH]; sysdllw(w, "kernel32.dll");
    DWORD n = P_szW(w, NULL);
    BYTE *a = calloc(1, n), *b = calloc(1, n);
    CHECK(P_giW(w, 0, n, a));
    CHECK(P_giW(w, 0xDEADBEEF, n, b));
    CHECK(!memcmp(a, b, n));
    free(a); free(b);
}
TEST(gap5_ver_info_flags) {
    NEED_ALL();
    WCHAR w[MAX_PATH]; sysdllw(w, "kernel32.dll");
    static const DWORD fl[] = { 0, 1, 2, 3, 4, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof fl / sizeof *fl; i++) {
        DWORD n = P_szExW(fl[i] & 1, w, NULL);
        BYTE *b = calloc(1, n + 64);
        BOOL ok = P_giExW(fl[i], w, 0, n, b);
        CHECKF(ok, "flags 0x%lx", fl[i]);
        if (ok) {
            CHECKF(*(DWORD *)(b + 0x28) == 0xFEEF04BD, "flags 0x%lx", fl[i]);
            CHECKF(*(WORD *)b >= 0x5c && *(WORD *)b <= n, "flags 0x%lx", fl[i]);
        }
        free(b);
    }
}
TEST(gap5_ver_info_len_below_minimum) {
    WINDOWS_ONLY();
    NEED_ALL();
    WCHAR w[MAX_PATH]; sysdllw(w, "kernel32.dll"); char p[MAX_PATH]; sysdll(p, sizeof p, "kernel32.dll");
    BYTE buf[0x100]; memset(buf, 0xCC, sizeof buf);
    DWORD lens[] = { 0, 1, 4, 0x20, 0x47 };
    for (unsigned i = 0; i < sizeof lens / sizeof *lens; i++) {
        SET_ERR(0); CHECKF(!P_giW(w, 0, lens[i], buf), "W len %lu", lens[i]); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
        SET_ERR(0); CHECKF(!P_giA(p, 0, lens[i], buf), "A len %lu", lens[i]); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
        SET_ERR(0); CHECKF(!P_giExW(0, w, 0, lens[i], buf), "ExW len %lu", lens[i]); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    }
    /* the length check precedes everything else: even a missing file reports it */
    SET_ERR(0); CHECK(!P_giW(L"C:\\k32t_nonexist\\x.dll", 0, 0x10, buf)); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    SET_ERR(0); CHECK(!P_giW(L"", 0, 0x10, buf)); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    SET_ERR(0); CHECK(!P_giW(NULL, 0, 0x47, NULL)); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    for (unsigned i = 0; i < sizeof buf; i++) if (buf[i] != 0xCC) { CHECK(0); break; }   /* buffer untouched */
}
TEST(gap5_ver_info_len_small_valid_range) {
    NEED_ALL();
    WCHAR w[MAX_PATH]; sysdllw(w, "kernel32.dll");
    /* 0x48..0x5b is accepted by the length check; outcome of the rest is observed only. */
    for (DWORD len = 0x48; len < 0x5c; len += 4) {
        BYTE buf[0x60]; memset(buf, 0, sizeof buf);
        SET_ERR(0);
        BOOL r = P_giW(w, 0, len, buf);
        t_note("giW len=0x%lx -> %d err=%lu", len, r, GetLastError());
    }
}
TEST(gap5_ver_info_truncated_buffer) {
    WINDOWS_ONLY();
    NEED_ALL();
    WCHAR w[MAX_PATH]; sysdllw(w, "kernel32.dll");
    DWORD n = P_szW(w, NULL);
    DWORD lens[] = { 0x5c, 0x60, 0x100, n / 2, n - 4 };
    for (unsigned i = 0; i < sizeof lens / sizeof *lens; i++) {
        DWORD len = lens[i];
        BYTE *b = calloc(1, len + 32);
        memset(b + len, 0x5A, 32);       /* guard bytes after the buffer */
        SET_ERR(0);
        BOOL r = P_giW(w, 0, len, b);
        int guard = 1; for (int k = 0; k < 32; k++) if (b[len + k] != 0x5A) guard = 0;
        CHECKF(guard, "overrun with len=%lu", len);
        if (r) {
            WORD wl = *(WORD *)b;
            CHECKF(wl + 4 <= len, "len=%lu wLength=%u", len, wl);   /* the tail marker fits */
            CHECKF(wl % 4 == 0, "len=%lu", len);
        } else t_note("truncated len=%lu failed err=%lu", len, GetLastError());
        free(b);
    }
}
TEST(gap5_ver_info_exact_size) {
    NEED_ALL();
    WCHAR w[MAX_PATH]; sysdllw(w, "kernel32.dll");
    DWORD n = P_szW(w, NULL);
    BYTE *b = calloc(1, n + 32); memset(b + n, 0x5A, 32);
    CHECK(P_giW(w, 0, n, b));
    for (int k = 0; k < 32; k++) if (b[n + k] != 0x5A) { CHECK(0); break; }
    WORD wl = *(WORD *)b;
    EXPECT_EQ(n, (DWORD)wl * 2 + 4);
    free(b);
}
TEST(gap5_ver_info_oversized_buffer) {
    NEED_ALL();
    WCHAR w[MAX_PATH]; sysdllw(w, "kernel32.dll");
    DWORD n = P_szW(w, NULL);
    DWORD big = n * 4;
    BYTE *b = calloc(1, big), *e = calloc(1, n);
    CHECK(P_giW(w, 0, big, b));
    CHECK(P_giW(w, 0, n, e));
    CHECK(!memcmp(b, e, n));
    free(b); free(e);
}
TEST(gap5_ver_info_nonexistent) {
    NEED_ALL();
    BYTE buf[0x400];
    SET_ERR(0); CHECK(!P_giA("C:\\Windows\\k32t_nonexistent.dll", 0, sizeof buf, buf)); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    SET_ERR(0); CHECK(!P_giW(L"C:\\Windows\\k32t_nonexistent.dll", 0, sizeof buf, buf)); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    SET_ERR(0); CHECK(!P_giExA(1, "C:\\Windows\\k32t_nonexistent.dll", 0, sizeof buf, buf)); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    SET_ERR(0); CHECK(!P_giExW(2, L"C:\\Windows\\k32t_nonexistent.dll", 0, sizeof buf, buf)); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(gap5_ver_info_repeat_and_copy_consistency) {
    NEED_ALL();
    char src[MAX_PATH], dst[MAX_PATH]; sysdll(src, sizeof src, "kernel32.dll"); t_tmpfile(dst, sizeof dst, "gap5_copy2.dll");
    if (!CopyFileA(src, dst, FALSE)) { t_skip("copy failed"); return; }
    DWORD n = P_szA(src, NULL);
    BYTE *a = calloc(1, n), *b = calloc(1, n), *c = calloc(1, n);
    CHECK(P_giA(src, 0, n, a));
    CHECK(P_giA(src, 0, n, b));
    CHECK(!memcmp(a, b, n));
    CHECK(P_giA(dst, 0, n, c));
    /* the leading header and the fixed-info signature agree; the quirk fix-ups for OS-owned files may alter later fields */
    CHECK(!memcmp(a, c, 0x30));
    if (memcmp(a, c, n)) t_note("copy differs from original in fixed-info fields (OS-owned file fix-up)");
    free(a); free(b); free(c);
    DeleteFileA(dst);
}
TEST(gap5_ver_info_many_system_files) {
    WINDOWS_ONLY();
    NEED_ALL();
    static const char *f[] = { "kernel32.dll", "ntdll.dll", "kernelbase.dll", "user32.dll", "gdi32.dll", "advapi32.dll", "shell32.dll",
        "ole32.dll", "rpcrt4.dll", "msvcrt.dll", "ws2_32.dll", "cmd.exe", "notepad.exe", "ping.exe", "conhost.exe" };
    int ok = 0;
    for (unsigned i = 0; i < sizeof f / sizeof *f; i++) {
        char p[MAX_PATH]; sysdll(p, sizeof p, f[i]);
        if (GetFileAttributesA(p) == INVALID_FILE_ATTRIBUTES) continue;
        DWORD sz = 0; BYTE *b = getblock(f[i], &sz);
        if (!b) { CHECKF(0, "%s: no version info (err %lu)", f[i], GetLastError()); continue; }
        CHECKF(*(DWORD *)(b + 0x28) == 0xFEEF04BD, "%s", f[i]);
        CHECKF(*(DWORD *)(b + *(WORD *)b) == 0x58324546, "%s tail marker", f[i]);
        ok++; free(b);
    }
    CHECK(ok > 0);
}
TEST(gap5_ver_info_unicode_path_a_fails_w_works) {
    NEED_ALL();
    /* A file whose name needs a non-ANSI char: only exercise the W path (A form would be lossy). */
    char src[MAX_PATH], dst[MAX_PATH]; sysdll(src, sizeof src, "kernel32.dll"); t_tmpfile(dst, sizeof dst, "gap5_uni.dll");
    WCHAR wd[MAX_PATH]; g5_a2w(dst, wd, MAX_PATH);
    WCHAR *dot = wcsrchr(wd, L'.'); WCHAR tail[8] = { 0x20AC, L'.', L'd', L'l', L'l', 0 };
    wcscpy(dot, tail);
    WCHAR ws[MAX_PATH]; g5_a2w(src, ws, MAX_PATH);
    if (!CopyFileW(ws, wd, FALSE)) { t_skip("copy failed"); return; }
    DWORD n = P_szW(wd, NULL);
    CHECK(n > 0);
    BYTE *b = calloc(1, n ? n : 1);
    CHECK(P_giW(wd, 0, n, b));
    CHECK(*(DWORD *)(b + 0x28) == 0xFEEF04BD);
    free(b);
    DeleteFileW(wd);
}
TEST(gap5_ver_size_last_error_cleared_on_success) {
    NEED_ALL();
    WCHAR w[MAX_PATH]; sysdllw(w, "kernel32.dll");
    SET_ERR(ERROR_ACCESS_DENIED);
    CHECK(P_szW(w, NULL) > 0);
    EXPECT_ERR(0);
    SET_ERR(ERROR_ACCESS_DENIED);
    CHECK(P_szExW(FV_NEUTRAL, w, NULL) > 0);
    EXPECT_ERR(0);
}
