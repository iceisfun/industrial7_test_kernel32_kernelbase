/* GetModuleHandle{A,W,ExA,ExW}, GetModuleFileName{A,W} */
#include "ml.h"
#include <wchar.h>

static HMODULE exe_base_from_peb(void) {
    PEB *peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    return (HMODULE)peb->Reserved3[1];
}
static int mh_dummy_code(void) { return 42; }
static int mh_dummy_data = 17;

/* ---- GetModuleHandleA/W ---- */
TEST(gmh_null_is_exe) {
    REQUIRE(U(GetModuleHandleA)); REQUIRE(U(GetModuleHandleW));
    HMODULE a = U(GetModuleHandleA)(NULL), w = U(GetModuleHandleW)(NULL);
    CHECK(a != NULL);
    CHECK(a == w);
    CHECK(a == exe_base_from_peb());
    CHECK(ml_nt(a) != NULL);
    CHECK(((ULONG_PTR)a & 0xFFFF) == 0);
    CHECK(a == (HMODULE)GetModuleHandleA(NULL));
}
TEST(gmh_null_clears_nothing_and_keeps_error) {
    REQUIRE(U(GetModuleHandleW));
    SET_ERR(0x1234);
    CHECK(U(GetModuleHandleW)(NULL) != NULL);
    EXPECT_ERR(0x1234);
}
TEST(gmh_nonexistent) {
    REQUIRE(U(GetModuleHandleA)); REQUIRE(U(GetModuleHandleW));
    SET_ERR(0);
    CHECK(U(GetModuleHandleA)("k32t_nosuch_module.dll") == NULL);
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
    SET_ERR(0);
    CHECK(U(GetModuleHandleW)(L"k32t_nosuch_module.dll") == NULL);
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
    SET_ERR(0);
    CHECK(U(GetModuleHandleA)("k32t_nosuch_module") == NULL);
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
}
TEST(gmh_empty_name) {
    REQUIRE(U(GetModuleHandleA)); REQUIRE(U(GetModuleHandleW));
    SET_ERR(0);
    HMODULE h = U(GetModuleHandleA)("");
    CHECK(h == NULL);
    t_note("GetModuleHandleA(\"\") error %lu", GetLastError());
    SET_ERR(0);
    h = U(GetModuleHandleW)(L"");
    CHECK(h == NULL);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_MOD_NOT_FOUND || e == ERROR_INVALID_PARAMETER, "e=%lu", e);
}
TEST(gmh_extension_and_case) {
    REQUIRE(U(GetModuleHandleA)); REQUIRE(U(GetModuleHandleW));
    HMODULE k = t_kernel32();
    static const char *n[] = { "kernel32.dll", "kernel32", "KERNEL32.DLL", "KERNEL32", "Kernel32.Dll", "kErNeL32" };
    for (int i = 0; i < 6; i++) {
        CHECKF(U(GetModuleHandleA)(n[i]) == k, "A %s", n[i]);
        CHECKF(U(GetModuleHandleW)(ml_w(n[i])) == k, "W %s", n[i]);
    }
    static const char *m[] = { "ntdll.dll", "NTDLL", "ntdll", "KernelBase", "kernelbase.DLL" };
    HMODULE exp[] = { t_ntdll(), t_ntdll(), t_ntdll(), t_kernelbase(), t_kernelbase() };
    for (int i = 0; i < 5; i++) CHECKF(U(GetModuleHandleA)(m[i]) == exp[i], "%s", m[i]);
}
TEST(gmh_full_path) {
    REQUIRE(U(GetModuleHandleA)); REQUIRE(U(GetModuleHandleW));
    char p[MAX_PATH]; GetSystemDirectoryA(p, sizeof p); strcat(p, "\\kernel32.dll");
    CHECKF(U(GetModuleHandleA)(p) == t_kernel32(), "err %lu", GetLastError());
    CHECK(U(GetModuleHandleW)(ml_w(p)) == t_kernel32());
    char q[MAX_PATH]; strcpy(q, p); CharUpperA(q);
    CHECK(U(GetModuleHandleA)(q) == t_kernel32());
}
TEST(gmh_wrong_directory_path) {
    REQUIRE(U(GetModuleHandleA));
    char p[MAX_PATH]; snprintf(p, sizeof p, "%skernel32.dll", ml_dir("gmh_wd"));
    SET_ERR(0);
    HMODULE h = U(GetModuleHandleA)(p);
    CHECK(h == NULL);
    if (!h) EXPECT_ERR(ERROR_MOD_NOT_FOUND);
}
TEST(gmh_trailing_dot_and_spaces) {
    REQUIRE(U(GetModuleHandleA));
    HMODULE h = U(GetModuleHandleA)("kernel32.");
    t_note("GetModuleHandle(\"kernel32.\") -> %p (err %lu)", h, h ? 0 : GetLastError());
    h = U(GetModuleHandleA)("kernel32.dll ");
    t_note("GetModuleHandle(\"kernel32.dll \") -> %p", h);
    h = U(GetModuleHandleA)(" kernel32.dll");
    CHECK(h == NULL);
}
TEST(gmh_loaded_image_lifecycle) {
    REQUIRE(U(GetModuleHandleA));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("gmh_lc"), "k32m_gmh.dll", p));
    SET_ERR(0);
    CHECK(U(GetModuleHandleA)("k32m_gmh.dll") == NULL);
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
    HMODULE h = LoadLibraryA(p);
    CHECK(h != NULL); if (!h) return;
    CHECK(U(GetModuleHandleA)("k32m_gmh.dll") == h);
    CHECK(U(GetModuleHandleA)("K32M_GMH") == h);
    CHECK(U(GetModuleHandleA)(p) == h);
    CHECK(U(GetModuleHandleW)(ml_w(p)) == h);
    FreeLibrary(h);
    SET_ERR(0);
    CHECK(U(GetModuleHandleA)("k32m_gmh.dll") == NULL);
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
}
TEST(gmh_does_not_find_datafile_loads) {
    REQUIRE(U(GetModuleHandleA));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("gmh_df"), "k32m_gdf.dll", p));
    HMODULE h = LoadLibraryExA(p, NULL, LOAD_LIBRARY_AS_DATAFILE);
    CHECK(h != NULL); if (!h) return;
    SET_ERR(0);
    CHECK(U(GetModuleHandleA)("k32m_gdf.dll") == NULL);
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
    FreeLibrary(h);
}
TEST(gmh_apiset_name) {
    REQUIRE(U(GetModuleHandleW)); REQUIRE(U(LoadLibraryW));
    HMODULE h = U(LoadLibraryW)(L"api-ms-win-core-processthreads-l1-1-0.dll");
    CHECK(h != NULL);
    if (h) {
        CHECK(h == t_kernel32() || h == t_kernelbase());
        FreeLibrary(h);
    }
    HMODULE g = U(GetModuleHandleW)(L"api-ms-win-core-processthreads-l1-1-0.dll");
    t_note("GetModuleHandle(apiset) -> %p (kernel32=%p kernelbase=%p)", g, t_kernel32(), t_kernelbase());
}
TEST(gmh_apiset_unknown_contract) {
    REQUIRE(U(LoadLibraryW));
    SET_ERR(0);
    HMODULE h = U(LoadLibraryW)(L"api-ms-win-k32t-nosuch-l1-1-0.dll");
    CHECK(h == NULL);
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
}
TEST(gmh_ntdll_and_kernel_distinct) {
    REQUIRE(U(GetModuleHandleA));
    HMODULE a = U(GetModuleHandleA)("ntdll.dll"), b = U(GetModuleHandleA)("kernel32.dll"), c = U(GetModuleHandleA)("kernelbase.dll");
    CHECK(a && b && c && a != b && b != c && a != c);
}
TEST(gmh_repeated_stable) {
    REQUIRE(U(GetModuleHandleA));
    HMODULE first = U(GetModuleHandleA)("kernel32.dll");
    for (int i = 0; i < 1000; i++) if (U(GetModuleHandleA)("kernel32.dll") != first) { CHECK(0); break; }
}
TEST(gmh_very_long_name) {
    REQUIRE(U(GetModuleHandleW));
    static WCHAR big[1200]; for (int i = 0; i < 1100; i++) big[i] = L'a'; big[1100] = 0;
    SET_ERR(0);
    CHECK(U(GetModuleHandleW)(big) == NULL);
    CHECK(GetLastError() != 0);
    static WCHAR path[600]; wcscpy(path, L"C:\\");
    for (int i = 0; i < 3; i++) wcscat(path, L"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\\");
    wcscat(path, L"x.dll");
    SET_ERR(0);
    CHECK(U(GetModuleHandleW)(path) == NULL);
}

/* ---- GetModuleHandleEx ---- */
TEST(gmhx_null_name_gives_exe) {
    REQUIRE(U(GetModuleHandleExA)); REQUIRE(U(GetModuleHandleExW));
    HMODULE h = (HMODULE)1;
    CHECK(U(GetModuleHandleExA)(0, NULL, &h));
    CHECK(h == GetModuleHandleA(NULL));
    h = (HMODULE)1;
    CHECK(U(GetModuleHandleExW)(0, NULL, &h));
    CHECK(h == GetModuleHandleA(NULL));
    h = NULL;
    CHECK(U(GetModuleHandleExW)(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, NULL, &h));
    CHECK(h == GetModuleHandleA(NULL));
    h = NULL;
    CHECK(U(GetModuleHandleExW)(GET_MODULE_HANDLE_EX_FLAG_PIN, NULL, &h));
    CHECK(h == GetModuleHandleA(NULL));
}
TEST(gmhx_by_name_a_w) {
    REQUIRE(U(GetModuleHandleExA)); REQUIRE(U(GetModuleHandleExW));
    HMODULE a = NULL, w = NULL;
    CHECK(U(GetModuleHandleExA)(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, "kernel32.dll", &a));
    CHECK(U(GetModuleHandleExW)(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, L"KERNEL32", &w));
    CHECK(a == t_kernel32() && w == a);
    a = w = NULL;
    CHECK(U(GetModuleHandleExA)(0, "kernel32.dll", &a));
    CHECK(a == t_kernel32());
    FreeLibrary(a);
}
TEST(gmhx_nonexistent) {
    REQUIRE(U(GetModuleHandleExA)); REQUIRE(U(GetModuleHandleExW));
    HMODULE h = (HMODULE)0x1234;
    SET_ERR(0);
    CHECK(!U(GetModuleHandleExA)(0, "k32t_nosuch_module.dll", &h));
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
    CHECK(h == NULL);
    h = (HMODULE)0x1234;
    SET_ERR(0);
    CHECK(!U(GetModuleHandleExW)(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, L"k32t_nosuch_module.dll", &h));
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
    CHECK(h == NULL);
}
TEST(gmhx_invalid_flags) {
    REQUIRE(U(GetModuleHandleExW)); REQUIRE(U(GetModuleHandleExA));
    static const DWORD f[] = { 8, 0x10, 0x20, 0x100, 0x80000000, 0xFFFFFFF8, 0xFFFFFFFF,
        GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS };
    for (int i = 0; i < (int)(sizeof f / sizeof *f); i++) {
        HMODULE h = (HMODULE)0x1234;
        SET_ERR(0);
        CHECKF(!U(GetModuleHandleExW)(f[i], L"kernel32.dll", &h), "flags %lx", f[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        CHECKF(h == NULL, "flags %lx h=%p", f[i], h);
        h = (HMODULE)0x1234;
        SET_ERR(0);
        CHECKF(!U(GetModuleHandleExA)(f[i], "kernel32.dll", &h), "flags %lx", f[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        CHECK(h == NULL);
    }
}
TEST(gmhx_invalid_flags_null_name) {
    REQUIRE(U(GetModuleHandleExW));
    HMODULE h = (HMODULE)0x1234;
    SET_ERR(0);
    CHECK(!U(GetModuleHandleExW)(0x8, NULL, &h));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CHECK(h == NULL);
}
TEST(gmhx_from_address_null) {
    REQUIRE(U(GetModuleHandleExW)); REQUIRE(U(GetModuleHandleExA));
    HMODULE h = (HMODULE)0x1234;
    SET_ERR(0);
    CHECK(!U(GetModuleHandleExW)(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, NULL, &h));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CHECK(h == NULL);
    SET_ERR(0);
    CHECK(!U(GetModuleHandleExA)(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, NULL, &h));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gmhx_null_out_pointer) {
    REQUIRE(U(GetModuleHandleExW)); REQUIRE(U(GetModuleHandleExA));
    SET_ERR(0);
    CHECK(!U(GetModuleHandleExW)(0, L"kernel32.dll", NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!U(GetModuleHandleExA)(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, "kernel32.dll", NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!U(GetModuleHandleExW)(0, NULL, NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gmhx_from_address_code_and_data) {
    REQUIRE(U(GetModuleHandleExA)); REQUIRE(U(GetModuleHandleExW));
    const DWORD fl = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
    HMODULE h = NULL;
    CHECK(U(GetModuleHandleExA)(fl, (LPCSTR)mh_dummy_code, &h));
    CHECK(h == GetModuleHandleA(NULL));
    h = NULL;
    CHECK(U(GetModuleHandleExW)(fl, (LPCWSTR)&mh_dummy_data, &h));
    CHECK(h == GetModuleHandleA(NULL));
    h = NULL;
    CHECK(U(GetModuleHandleExA)(fl, (LPCSTR)GetProcAddress(t_ntdll(), "NtClose"), &h));
    CHECK(h == t_ntdll());
    h = NULL;
    CHECK(U(GetModuleHandleExA)(fl, (LPCSTR)((char *)t_kernel32() + 0x1000), &h));
    CHECK(h == t_kernel32());
    h = NULL;
    CHECK(U(GetModuleHandleExA)(fl, (LPCSTR)((char *)t_kernelbase() + 0x10), &h));   /* inside the headers */
    CHECK(h == t_kernelbase());
    h = NULL;
    CHECK(U(GetModuleHandleExA)(fl, (LPCSTR)GetProcAddress(t_kernel32(), "CreateFileW"), &h));
    CHECK(h == t_kernel32() || h == t_kernelbase());
}
TEST(gmhx_from_address_not_a_module) {
    REQUIRE(U(GetModuleHandleExA));
    const DWORD fl = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
    char stackvar; HMODULE h = (HMODULE)0x1234;
    SET_ERR(0);
    CHECK(!U(GetModuleHandleExA)(fl, &stackvar, &h));
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
    CHECK(h == NULL);
    void *heap = HeapAlloc(GetProcessHeap(), 0, 64);
    h = (HMODULE)0x1234;
    SET_ERR(0);
    CHECK(!U(GetModuleHandleExA)(fl, heap, &h));
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
    HeapFree(GetProcessHeap(), 0, heap);
    void *v = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    h = (HMODULE)0x1234;
    SET_ERR(0);
    CHECK(!U(GetModuleHandleExA)(fl, v, &h));
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
    VirtualFree(v, 0, MEM_RELEASE);
    h = (HMODULE)0x1234;
    SET_ERR(0);
    CHECK(!U(GetModuleHandleExA)(fl, (LPCSTR)(ULONG_PTR)0x10, &h));
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
}
TEST(gmhx_from_address_image_lifecycle) {
    REQUIRE(U(GetModuleHandleExA));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("gmhx_fa"), "k32m_gfa.dll", p));
    HMODULE m = LoadLibraryA(p);
    CHECK(m != NULL); if (!m) return;
    void *data = GetProcAddress(m, "ml_value");
    void *code = GetProcAddress(m, "ml_add");
    CHECK(data && code);
    const DWORD fl = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
    HMODULE h = NULL;
    CHECK(U(GetModuleHandleExA)(fl, data, &h)); CHECK(h == m);
    h = NULL;
    CHECK(U(GetModuleHandleExA)(fl, code, &h)); CHECK(h == m);
    /* without UNCHANGED_REFCOUNT the lookup adds a reference */
    h = NULL;
    CHECK(U(GetModuleHandleExA)(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, code, &h));
    CHECK(h == m);
    FreeLibrary(m); CHECK(ml_is_loaded(m));
    FreeLibrary(m); CHECK(!ml_is_loaded(m));
}
TEST(gmhx_addref_by_name_then_two_frees) {
    REQUIRE(U(GetModuleHandleExW));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("gmhx_ar"), "k32m_gar.dll", p));
    HMODULE m = LoadLibraryA(p), h = NULL;
    CHECK(m != NULL); if (!m) return;
    for (int i = 0; i < 3; i++) { h = NULL; CHECK(U(GetModuleHandleExW)(0, ml_w("k32m_gar.dll"), &h)); CHECK(h == m); }
    for (int i = 0; i < 3; i++) { CHECK(FreeLibrary(m)); CHECK(ml_is_loaded(m)); }
    CHECK(FreeLibrary(m));
    CHECK(!ml_is_loaded(m));
}
CHILD(gmhx_pin) {
    /* argv[1] = path of image */
    if (argc < 2) return 90;
    HMODULE m = LoadLibraryA(argv[1]), h = NULL;
    if (!m) return 91;
    if (!U(GetModuleHandleExA)(GET_MODULE_HANDLE_EX_FLAG_PIN, argv[1], &h) || h != m) return 92;
    for (int i = 0; i < 10; i++) FreeLibrary(m);
    HMODULE chk = NULL;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)m, &chk) || chk != m) return 93;
    int *attach = (int *)GetProcAddress(m, "ml_proc_attach");
    if (!attach || *attach != 1) return 94;
    /* pinned: still loadable, still same handle */
    if (LoadLibraryA(argv[1]) != m) return 95;
    return 0;
}
TEST(gmhx_pin_keeps_module_loaded) {
    REQUIRE(U(GetModuleHandleExA));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("gmhx_pin"), "k32m_pin.dll", p));
    char args[MAX_PATH + 4]; snprintf(args, sizeof args, "\"%s\"", p);
    EXPECT_EQ(ml_run_exe(t_self_path(), "gmhx_pin", args, NULL, 20000, 0), 0);
}
TEST(gmhx_pin_by_address) {
    REQUIRE(U(GetModuleHandleExA));
    /* pinning a system module is harmless and idempotent */
    HMODULE h = NULL;
    CHECK(U(GetModuleHandleExA)(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)GetProcAddress(t_ntdll(), "NtClose"), &h));
    CHECK(h == t_ntdll());
}
TEST(gmhx_pin_nonexistent) {
    REQUIRE(U(GetModuleHandleExA));
    HMODULE h = (HMODULE)1;
    SET_ERR(0);
    CHECK(!U(GetModuleHandleExA)(GET_MODULE_HANDLE_EX_FLAG_PIN, "k32t_nosuch_module.dll", &h));
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
    CHECK(h == NULL);
}
TEST(gmhx_a_w_parity_names) {
    REQUIRE(U(GetModuleHandleExA)); REQUIRE(U(GetModuleHandleExW));
    static const char *n[] = { "kernel32", "KERNEL32.DLL", "ntdll.dll", "kernelbase" };
    for (int i = 0; i < 4; i++) {
        HMODULE a = NULL, w = NULL;
        CHECKF(U(GetModuleHandleExA)(2, n[i], &a), "%s", n[i]);
        CHECKF(U(GetModuleHandleExW)(2, ml_w(n[i]), &w), "%s", n[i]);
        CHECKF(a && a == w, "%s", n[i]);
    }
}
TEST(gmhx_last_error_unchanged_on_success) {
    REQUIRE(U(GetModuleHandleExW));
    HMODULE h;
    SET_ERR(0x4321);
    CHECK(U(GetModuleHandleExW)(2, L"kernel32.dll", &h));
    EXPECT_ERR(0x4321);
}

/* ---- GetModuleFileName ---- */
static void expect_sys_path(const char *got, const char *leaf, const char *what) {
    char want[MAX_PATH]; GetSystemDirectoryA(want, sizeof want); strcat(want, "\\"); strcat(want, leaf);
    CHECKF(!_stricmp(got, want), "%s: got \"%s\" want \"%s\"", what, got, want);
}
TEST(gmfn_exe_path_a_w_equal) {
    REQUIRE(U(GetModuleFileNameA)); REQUIRE(U(GetModuleFileNameW));
    char a[MAX_PATH]; WCHAR w[MAX_PATH];
    DWORD na = U(GetModuleFileNameA)(NULL, a, MAX_PATH), nw = U(GetModuleFileNameW)(NULL, w, MAX_PATH);
    CHECK(na > 0 && na == nw);
    CHECK(na == strlen(a));
    CHECK(nw == lstrlenW(w));
    CHECK(!strcmp(a, ml_a(w)));
    CHECK(strstr(a, "modules_loader") != NULL || strstr(a, ".exe") != NULL);
    CHECK((a[1] == ':' && a[2] == '\\') || (a[0] == '\\' && a[1] == '\\'));
    HMODULE self = GetModuleHandleA(NULL);
    char b[MAX_PATH];
    CHECK(U(GetModuleFileNameA)(self, b, MAX_PATH) == na);
    CHECK(!strcmp(a, b));
}
TEST(gmfn_system_modules) {
    REQUIRE(U(GetModuleFileNameA)); REQUIRE(U(GetModuleFileNameW));
    static const char *n[] = { "kernel32.dll", "kernelbase.dll", "ntdll.dll", "user32.dll" };
    for (int i = 0; i < 4; i++) {
        HMODULE h = GetModuleHandleA(n[i]);
        if (!h) h = LoadLibraryA(n[i]);
        char a[MAX_PATH]; WCHAR w[MAX_PATH];
        SET_ERR(0x777);
        DWORD na = U(GetModuleFileNameA)(h, a, MAX_PATH);
        CHECKF(na != 0, "%s", n[i]);
        EXPECT_ERR(0);
        SET_ERR(0x777);
        DWORD nw = U(GetModuleFileNameW)(h, w, MAX_PATH);
        EXPECT_ERR(0);
        CHECKF(na == nw, "%s", n[i]);
        expect_sys_path(a, n[i], n[i]);
        CHECKF(!strcmp(a, ml_a(w)), "%s", n[i]);
    }
}
TEST(gmfn_w_truncation_sweep) {
    REQUIRE(U(GetModuleFileNameW));
    HMODULE k = t_kernel32();
    WCHAR full[MAX_PATH]; DWORD L = U(GetModuleFileNameW)(k, full, MAX_PATH);
    CHECK(L > 3 && L < MAX_PATH);
    for (DWORD n = 1; n <= L + 2; n++) {
        WCHAR buf[MAX_PATH + 8]; for (int i = 0; i < MAX_PATH + 8; i++) buf[i] = 0x5A5A;
        SET_ERR(0);
        DWORD r = U(GetModuleFileNameW)(k, buf, n);
        if (n > L) {
            CHECKF(r == L, "n=%lu r=%lu", n, r);
            CHECKF(buf[L] == 0 && !lstrcmpW(buf, full), "n=%lu", n);
            EXPECT_ERR(0);
        } else {
            CHECKF(r == n, "n=%lu r=%lu L=%lu", n, r, L);
            CHECKF(GetLastError() == ERROR_INSUFFICIENT_BUFFER, "n=%lu err %lu", n, GetLastError());
            CHECKF(buf[n - 1] == 0, "n=%lu not terminated", n);
            CHECKF(!wmemcmp(buf, full, n - 1), "n=%lu prefix", n);
            CHECKF(buf[n] == 0x5A5A, "n=%lu wrote past buffer", n);
        }
    }
}
TEST(gmfn_a_truncation_sweep) {
    REQUIRE(U(GetModuleFileNameA));
    HMODULE k = t_kernel32();
    char full[MAX_PATH]; DWORD L = U(GetModuleFileNameA)(k, full, MAX_PATH);
    CHECK(L > 3 && L < MAX_PATH);
    for (DWORD n = 1; n <= L + 2; n++) {
        char buf[MAX_PATH + 8]; memset(buf, 0x5A, sizeof buf);
        SET_ERR(0);
        DWORD r = U(GetModuleFileNameA)(k, buf, n);
        if (n > L) {
            CHECKF(r == L, "n=%lu r=%lu", n, r);
            CHECKF(!strcmp(buf, full), "n=%lu", n);
        } else {
            CHECKF(r == n, "n=%lu r=%lu L=%lu", n, r, L);
            CHECKF(GetLastError() == ERROR_INSUFFICIENT_BUFFER, "n=%lu err %lu", n, GetLastError());
            CHECKF(buf[n - 1] == 0, "n=%lu not terminated", n);
            CHECKF(!memcmp(buf, full, n - 1), "n=%lu prefix", n);
        }
    }
}
TEST(gmfn_size_zero) {
    REQUIRE(U(GetModuleFileNameA)); REQUIRE(U(GetModuleFileNameW));
    char a[4] = "xyz"; WCHAR w[4] = L"xyz";
    SET_ERR(0);
    CHECK(U(GetModuleFileNameA)(NULL, a, 0) == 0);
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(!strcmp(a, "xyz"));
    SET_ERR(0);
    CHECK(U(GetModuleFileNameW)(NULL, w, 0) == 0);
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(!lstrcmpW(w, L"xyz"));
    SET_ERR(0);
    CHECK(U(GetModuleFileNameW)(t_kernel32(), NULL, 0) == 0);
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    SET_ERR(0);
    CHECK(U(GetModuleFileNameA)(t_kernel32(), NULL, 0) == 0);
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
}
TEST(gmfn_size_one) {
    REQUIRE(U(GetModuleFileNameA)); REQUIRE(U(GetModuleFileNameW));
    char a[4] = "xyz"; WCHAR w[4] = L"xyz";
    SET_ERR(0);
    CHECK(U(GetModuleFileNameW)(NULL, w, 1) == 1);
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(w[0] == 0 && w[1] == L'y');
    SET_ERR(0);
    CHECK(U(GetModuleFileNameA)(NULL, a, 1) == 1);
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(a[0] == 0);
}
TEST(gmfn_tagged_handles_fail) {
    REQUIRE(U(GetModuleFileNameA)); REQUIRE(U(GetModuleFileNameW));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("gmfn_df"), "k32m_gmfn.dll", p));
    HMODULE h = LoadLibraryExA(p, NULL, LOAD_LIBRARY_AS_DATAFILE);
    CHECK(h != NULL); if (!h) return;
    char a[MAX_PATH]; WCHAR w[MAX_PATH];
    SET_ERR(0);
    CHECK(U(GetModuleFileNameA)(h, a, MAX_PATH) == 0);
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
    SET_ERR(0);
    CHECK(U(GetModuleFileNameW)(h, w, MAX_PATH) == 0);
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
    FreeLibrary(h);
    h = LoadLibraryExA(p, NULL, LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    CHECK(h != NULL); if (!h) return;
    SET_ERR(0);
    CHECK(U(GetModuleFileNameW)(h, w, MAX_PATH) == 0);
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
    FreeLibrary(h);
}
TEST(gmfn_bogus_module) {
    REQUIRE(U(GetModuleFileNameA)); REQUIRE(U(GetModuleFileNameW));
    char a[MAX_PATH] = "zz"; WCHAR w[MAX_PATH] = L"zz";
    SET_ERR(0);
    CHECK(U(GetModuleFileNameW)((HMODULE)0x10000, w, MAX_PATH) == 0);
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
    SET_ERR(0);
    CHECK(U(GetModuleFileNameA)((HMODULE)0x10000, a, MAX_PATH) == 0);
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
    SET_ERR(0);
    CHECK(U(GetModuleFileNameW)((HMODULE)((char *)t_kernel32() + 0x1000), w, MAX_PATH) == 0);
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
}
TEST(gmfn_image_path_matches_load_path) {
    REQUIRE(U(GetModuleFileNameA)); REQUIRE(U(GetModuleFileNameW));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("gmfn_img"), "k32m_gmi.dll", p));
    HMODULE m = LoadLibraryA(p);
    CHECK(m != NULL); if (!m) return;
    char a[MAX_PATH]; WCHAR w[MAX_PATH];
    CHECK(U(GetModuleFileNameA)(m, a, MAX_PATH) == strlen(p));
    CHECK(!_stricmp(a, p));
    CHECK(U(GetModuleFileNameW)(m, w, MAX_PATH) == strlen(p));
    CHECK(!_stricmp(ml_a(w), p));
    FreeLibrary(m);
}
TEST(gmfn_forward_slash_path_normalised) {
    REQUIRE(U(GetModuleFileNameA));
    char p[MAX_PATH], q[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("gmfn_sl"), "k32m_gsl.dll", p));
    strcpy(q, p); for (char *c = q; *c; c++) if (*c == '\\') *c = '/';
    HMODULE m = LoadLibraryA(q);
    CHECKF(m != NULL, "err %lu", GetLastError()); if (!m) return;
    char a[MAX_PATH];
    CHECK(U(GetModuleFileNameA)(m, a, MAX_PATH) > 0);
    CHECKF(strchr(a, '/') == NULL, "got %s", a);
    CHECK(!_stricmp(a, p));
    FreeLibrary(m);
}
TEST(gmfn_unloaded_module_handle) {
    REQUIRE(U(GetModuleFileNameW));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("gmfn_ul"), "k32m_gul.dll", p));
    HMODULE m = LoadLibraryA(p);
    CHECK(m != NULL); if (!m) return;
    FreeLibrary(m);
    WCHAR w[MAX_PATH];
    SET_ERR(0);
    CHECK(U(GetModuleFileNameW)(m, w, MAX_PATH) == 0);
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
}
TEST(gmfn_large_size_clamped) {
    REQUIRE(U(GetModuleFileNameW)); REQUIRE(U(GetModuleFileNameA));
    WCHAR *w = HeapAlloc(GetProcessHeap(), 0, 70000 * sizeof(WCHAR));
    char *a = HeapAlloc(GetProcessHeap(), 0, 70000);
    if (!w || !a) { t_skip("alloc"); return; }
    DWORD n = U(GetModuleFileNameW)(NULL, w, 65000);
    CHECK(n > 0 && n < MAX_PATH);
    n = U(GetModuleFileNameA)(NULL, a, 65000);
    CHECK(n > 0 && n < MAX_PATH);
    n = U(GetModuleFileNameW)(NULL, w, 0xFFFFFFFF);
    CHECK(n > 0 && n < MAX_PATH);
    HeapFree(GetProcessHeap(), 0, w); HeapFree(GetProcessHeap(), 0, a);
}
TEST(gmfn_long_path_module) {
    REQUIRE(U(GetModuleFileNameW)); REQUIRE(U(LoadLibraryW));
    WCHAR dir[1000]; MultiByteToWideChar(CP_ACP, 0, t_tmpdir(), -1, dir, 1000);
    WCHAR full[1000]; wcscpy(full, L"\\\\?\\"); wcscat(full, dir);
    for (int i = 0; i < 4; i++) {
        wcscat(full, L"long_directory_name_to_exceed_max_path_aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
        if (!CreateDirectoryW(full, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) { t_skip("cannot create long dir (err %lu)", GetLastError()); return; }
        wcscat(full, L"\\");
    }
    wcscat(full, L"k32m_long.dll");
    size_t n; const unsigned char *d = ml_blob(ML_A, &n);
    HANDLE f = CreateFileW(full, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) { t_skip("cannot create long file"); return; }
    DWORD wr; WriteFile(f, d, (DWORD)n, &wr, NULL); CloseHandle(f);
    CHECK(lstrlenW(full) > MAX_PATH);
    HMODULE m = U(LoadLibraryW)(full);
    if (!m) { t_note("long path LoadLibraryW failed err %lu", GetLastError()); return; }
    WCHAR out[1200];
    DWORD r = U(GetModuleFileNameW)(m, out, 1200);
    CHECK(r > MAX_PATH);
    CHECK(r == (DWORD)lstrlenW(out));
    CHECK(r >= 13 && !lstrcmpiW(out + r - 13, L"k32m_long.dll"));
    WCHAR small[MAX_PATH]; SET_ERR(0);
    CHECK(U(GetModuleFileNameW)(m, small, MAX_PATH) == MAX_PATH);
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    FreeLibrary(m);
    /* remove the deep tree (the harness cleanup cannot handle > MAX_PATH) */
    DeleteFileW(full);
    for (int i = 0; i < 4; i++) {
        WCHAR *bs = wcsrchr(full, L'\\'); if (!bs) break;
        *bs = 0; RemoveDirectoryW(full);
    }
}
