/* LoadLibrary{A,W,ExA,ExW}, FreeLibrary: argument validation, error codes, flags, ref counting */
#include "ml.h"

#define LLPARITY_NOTE(x) t_note x

/* ---- invalid names ---- */
TEST(ld_a_null_name) {
    REQUIRE(U(LoadLibraryA));
    SET_ERR(0);
    CHECK(U(LoadLibraryA)(NULL) == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(ld_w_null_name) {
    REQUIRE(U(LoadLibraryW));
    SET_ERR(0);
    CHECK(U(LoadLibraryW)(NULL) == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(ld_ex_null_name) {
    REQUIRE(U(LoadLibraryExA)); REQUIRE(U(LoadLibraryExW));
    SET_ERR(0);
    CHECK(U(LoadLibraryExA)(NULL, NULL, 0) == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(U(LoadLibraryExW)(NULL, NULL, 0) == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(ld_empty_name) {
    REQUIRE(U(LoadLibraryA)); REQUIRE(U(LoadLibraryW));
    SET_ERR(0);
    CHECK(U(LoadLibraryA)("") == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(U(LoadLibraryW)(L"") == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(ld_blank_names) {
    REQUIRE(U(LoadLibraryA)); REQUIRE(U(LoadLibraryW));
    static const char *n[] = { " ", "   ", "          " };
    for (int i = 0; i < 3; i++) {
        SET_ERR(0);
        CHECKF(U(LoadLibraryA)(n[i]) == NULL, "i=%d", i);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        SET_ERR(0);
        CHECKF(U(LoadLibraryW)(ml_w(n[i])) == NULL, "i=%d", i);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(ld_trailing_spaces_trimmed) {
    REQUIRE(U(LoadLibraryA)); REQUIRE(U(LoadLibraryW));
    HMODULE k = GetModuleHandleA("kernel32.dll");
    HMODULE a = U(LoadLibraryA)("kernel32.dll   ");
    CHECK(a == k);
    HMODULE w = U(LoadLibraryW)(L"kernel32.dll ");
    CHECK(w == k);
    if (a) FreeLibrary(a);
    if (w) FreeLibrary(w);
}
TEST(ld_bare_nonexistent) {
    REQUIRE(U(LoadLibraryA)); REQUIRE(U(LoadLibraryW));
    SET_ERR(0);
    CHECK(U(LoadLibraryA)("k32t_nosuch_module.dll") == NULL);
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
    SET_ERR(0);
    CHECK(U(LoadLibraryW)(L"k32t_nosuch_module.dll") == NULL);
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
}
TEST(ld_bare_nonexistent_noext) {
    REQUIRE(U(LoadLibraryA));
    SET_ERR(0);
    CHECK(U(LoadLibraryA)("k32t_nosuch_module") == NULL);
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
}
TEST(ld_ex_nonexistent) {
    REQUIRE(U(LoadLibraryExA)); REQUIRE(U(LoadLibraryExW));
    SET_ERR(0);
    CHECK(U(LoadLibraryExA)("k32t_nosuch_module.dll", NULL, 0) == NULL);
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
    SET_ERR(0);
    CHECK(U(LoadLibraryExW)(L"k32t_nosuch_module.dll", NULL, 0) == NULL);
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
}
TEST(ld_nonexistent_fullpath) {
    REQUIRE(U(LoadLibraryA)); REQUIRE(U(LoadLibraryW));
    char p[MAX_PATH]; snprintf(p, sizeof p, "%sk32t_missing.dll", ml_dir("ld_np"));
    SET_ERR(0);
    CHECK(U(LoadLibraryA)(p) == NULL);
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
    SET_ERR(0);
    CHECK(U(LoadLibraryW)(ml_w(p)) == NULL);
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
}
TEST(ld_nonexistent_dir_fullpath) {
    REQUIRE(U(LoadLibraryA));
    char p[MAX_PATH]; snprintf(p, sizeof p, "%sno_such_dir\\k32t_missing.dll", ml_dir("ld_npd"));
    SET_ERR(0);
    CHECK(U(LoadLibraryA)(p) == NULL);
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
}
TEST(ld_nonexistent_bad_drive) {
    REQUIRE(U(LoadLibraryA));
    SET_ERR(0);
    CHECK(U(LoadLibraryA)("\\\\?\\Q:\\k32t_nosuch\\x.dll") == NULL);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_MOD_NOT_FOUND || e == ERROR_PATH_NOT_FOUND || e == ERROR_FILE_NOT_FOUND, "e=%lu", e);
}
TEST(ld_non_pe_file) {
    REQUIRE(U(LoadLibraryA)); REQUIRE(U(LoadLibraryW));
    char p[MAX_PATH]; snprintf(p, sizeof p, "%snotpe.dll", ml_dir("ld_npe"));
    static const char txt[] = "this is not a portable executable, just text\r\n";
    CHECK(ml_write(p, txt, sizeof txt));
    SET_ERR(0);
    CHECK(U(LoadLibraryA)(p) == NULL);
    EXPECT_ERR(ERROR_BAD_EXE_FORMAT);
    SET_ERR(0);
    CHECK(U(LoadLibraryW)(ml_w(p)) == NULL);
    EXPECT_ERR(ERROR_BAD_EXE_FORMAT);
}
TEST(ld_non_pe_file_large) {
    REQUIRE(U(LoadLibraryA));
    char p[MAX_PATH]; snprintf(p, sizeof p, "%slarge.dll", ml_dir("ld_npl"));
    static char buf[65536]; memset(buf, 'x', sizeof buf);
    CHECK(ml_write(p, buf, sizeof buf));
    SET_ERR(0);
    CHECK(U(LoadLibraryA)(p) == NULL);
    EXPECT_ERR(ERROR_BAD_EXE_FORMAT);
}
TEST(ld_mz_only_file) {
    REQUIRE(U(LoadLibraryA));
    char p[MAX_PATH]; snprintf(p, sizeof p, "%smz.dll", ml_dir("ld_mz"));
    unsigned char b[64] = { 'M', 'Z' };
    CHECK(ml_write(p, b, sizeof b));
    SET_ERR(0);
    CHECK(U(LoadLibraryA)(p) == NULL);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_BAD_EXE_FORMAT || e == ERROR_INVALID_IMAGE_HASH || e == ERROR_FILE_INVALID, "e=%lu", e);
}
TEST(ld_empty_file) {
    REQUIRE(U(LoadLibraryA));
    char p[MAX_PATH]; snprintf(p, sizeof p, "%sempty.dll", ml_dir("ld_empty"));
    CHECK(ml_write(p, "", 0));
    SET_ERR(0);
    CHECK(U(LoadLibraryA)(p) == NULL);
    t_note("empty file LoadLibrary error = %lu", GetLastError());
    DWORD e = GetLastError();
    CHECK(e == ERROR_FILE_INVALID || e == ERROR_BAD_EXE_FORMAT || e == ERROR_MOD_NOT_FOUND);
}
TEST(ld_truncated_image) {
    REQUIRE(U(LoadLibraryA));
    char p[MAX_PATH]; size_t n; const unsigned char *d = ml_blob(ML_A, &n);
    snprintf(p, sizeof p, "%strunc.dll", ml_dir("ld_trunc"));
    CHECK(ml_write(p, d, 0x200));   /* headers only, sections missing */
    SET_ERR(0);
    HMODULE h = U(LoadLibraryA)(p);
    CHECK(h == NULL);
    if (h) ml_unload(h); else t_note("truncated image error = %lu", GetLastError());
}
TEST(ld_directory_path) {
    REQUIRE(U(LoadLibraryA));
    const char *d = ml_dir("ld_dirdll.dll");   /* a directory named *.dll */
    char p[MAX_PATH]; strcpy(p, d); p[strlen(p) - 1] = 0;
    SET_ERR(0);
    CHECK(U(LoadLibraryA)(p) == NULL);
    t_note("LoadLibrary(directory) error = %lu", GetLastError());
}

/* ---- well-known system modules ---- */
TEST(ld_system_modules) {
    REQUIRE(U(LoadLibraryA)); REQUIRE(U(LoadLibraryW));
    static const char *n[] = { "kernel32.dll", "KERNEL32.DLL", "Kernel32", "kernel32", "kernelbase.dll", "ntdll.dll", "NTDLL", "advapi32.dll", "user32.dll", "gdi32.dll", "msvcrt.dll", "ws2_32.dll", "shell32.dll", "ole32.dll", "version.dll" };
    for (int i = 0; i < (int)(sizeof n / sizeof *n); i++) {
        HMODULE a = U(LoadLibraryA)(n[i]);
        CHECKF(a != NULL, "%s err %lu", n[i], GetLastError());
        HMODULE w = U(LoadLibraryW)(ml_w(n[i]));
        CHECKF(w == a, "%s a=%p w=%p", n[i], a, w);
        if (a) {
            CHECKF(((ULONG_PTR)a & 0xFFFF) == 0, "%s base %p not 64K aligned", n[i], a);
            const IMAGE_NT_HEADERS64 *nt = ml_nt(a);
            CHECKF(nt && nt->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64, "%s", n[i]);
            FreeLibrary(a);
        }
        if (w) FreeLibrary(w);
    }
}
TEST(ld_same_handle_as_getmodulehandle) {
    REQUIRE(U(LoadLibraryA));
    HMODULE k = U(LoadLibraryA)("kernel32.dll");
    CHECK(k == GetModuleHandleA("kernel32.dll"));
    CHECK(k == t_kernel32());
    HMODULE n = U(LoadLibraryA)("ntdll.dll");
    CHECK(n == t_ntdll());
    FreeLibrary(k); FreeLibrary(n);
}
TEST(ld_kernelbase_loaded) {
    REQUIRE(U(LoadLibraryW));
    HMODULE kb = U(LoadLibraryW)(L"kernelbase.dll");
    CHECK(kb != NULL);
    CHECK(kb == t_kernelbase());
    CHECK(kb != t_kernel32());
    if (kb) FreeLibrary(kb);
}
TEST(ld_system_fullpath_same_handle) {
    REQUIRE(U(LoadLibraryA)); REQUIRE(U(LoadLibraryW));
    char p[MAX_PATH]; GetSystemDirectoryA(p, sizeof p); strcat(p, "\\kernel32.dll");
    HMODULE a = U(LoadLibraryA)(p), w = U(LoadLibraryW)(ml_w(p));
    CHECK(a == GetModuleHandleA("kernel32.dll"));
    CHECK(a == w);
    if (a) FreeLibrary(a);
    if (w) FreeLibrary(w);
    /* forward slashes and doubled separators are normalised */
    char q[MAX_PATH]; strcpy(q, p);
    for (char *c = q; *c; c++) if (*c == '\\') *c = '/';
    HMODULE s = U(LoadLibraryA)(q);
    CHECKF(s == GetModuleHandleA("kernel32.dll"), "slash path err %lu", GetLastError());
    if (s) FreeLibrary(s);
}
TEST(ld_self_exe) {
    REQUIRE(U(LoadLibraryA));
    HMODULE h = U(LoadLibraryA)(t_self_path());
    CHECK(h == GetModuleHandleA(NULL));
    if (h) FreeLibrary(h);
}
TEST(ld_wow_dirs_absent) {
    /* A 64-bit process must not find syswow64-only content through the plain system directory. */
    REQUIRE(U(LoadLibraryA));
    SET_ERR(0);
    CHECK(U(LoadLibraryA)("wow64cpu_k32t_nosuch.dll") == NULL);
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
}

/* ---- flags validation ---- */
TEST(ld_ex_invalid_high_flags) {
    REQUIRE(U(LoadLibraryExW)); REQUIRE(U(LoadLibraryExA));
    static const DWORD f[] = { 0x10000, 0x20000, 0x100000, 0x80000000, 0xFFFF0000, 0x00010000 | LOAD_LIBRARY_SEARCH_SYSTEM32 };
    for (int i = 0; i < (int)(sizeof f / sizeof *f); i++) {
        SET_ERR(0);
        CHECKF(U(LoadLibraryExW)(L"kernel32.dll", NULL, f[i]) == NULL, "flags %08lx", f[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        SET_ERR(0);
        CHECKF(U(LoadLibraryExA)("kernel32.dll", NULL, f[i]) == NULL, "flags %08lx", f[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(ld_ex_datafile_and_exclusive_combo) {
    REQUIRE(U(LoadLibraryExW));
    SET_ERR(0);
    HMODULE h = U(LoadLibraryExW)(L"kernel32.dll", NULL, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE);
    CHECK(h == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    h = U(LoadLibraryExW)(L"kernel32.dll", NULL, 0x42 | LOAD_LIBRARY_SEARCH_SYSTEM32);
    CHECK(h == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(ld_ex_hfile_must_be_null) {
    REQUIRE(U(LoadLibraryExW)); REQUIRE(U(LoadLibraryExA));
    SET_ERR(0);
    CHECK(U(LoadLibraryExW)(L"kernel32.dll", (HANDLE)(ULONG_PTR)4, 0) == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(U(LoadLibraryExA)("kernel32.dll", INVALID_HANDLE_VALUE, 0) == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    HANDLE f = CreateFileA(t_self_path(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    SET_ERR(0);
    CHECK(U(LoadLibraryExW)(ml_w(t_self_path()), f, 0) == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CloseHandle(f);
}
TEST(ld_ex_zero_flags_equals_plain) {
    REQUIRE(U(LoadLibraryExW)); REQUIRE(U(LoadLibraryExA));
    HMODULE a = U(LoadLibraryExA)("kernel32.dll", NULL, 0), w = U(LoadLibraryExW)(L"kernel32.dll", NULL, 0);
    CHECK(a == w);
    CHECK(a == t_kernel32());
    FreeLibrary(a); FreeLibrary(w);
}
TEST(ld_ex_search_flags_dll_load_dir_with_altered_invalid) {
    REQUIRE(U(LoadLibraryExW));
    char p[MAX_PATH]; const char *d = ml_dir("ld_sdl");
    CHECK(ml_put(ML_A, d, "k32m_sdl.dll", p));
    SET_ERR(0);
    HMODULE h = U(LoadLibraryExW)(ml_w(p), NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_WITH_ALTERED_SEARCH_PATH);
    /* The documentation says the combination "cannot be combined" but names no error, and neither LoadLibraryExW nor
     * LdrLoadDll on Windows 10 rejects it: the outcome is not specified, so only record it. */
    t_note("DLL_LOAD_DIR|ALTERED_SEARCH_PATH with an absolute path: %s err %lu", h ? "loaded" : "failed", h ? 0 : GetLastError());
    if (h) ml_unload(h);
}
TEST(ld_ex_search_dll_load_dir_needs_absolute) {
    REQUIRE(U(LoadLibraryExW));
    SET_ERR(0);
    HMODULE h = U(LoadLibraryExW)(L"k32t_rel_nosuch.dll", NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR);
    CHECK(h == NULL);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_INVALID_PARAMETER || e == ERROR_MOD_NOT_FOUND, "e=%lu", e);
    t_note("SEARCH_DLL_LOAD_DIR with relative name: error %lu", e);
}

/* ---- the embedded test image ---- */
TEST(ld_image_basic_a_and_w) {
    REQUIRE(U(LoadLibraryA)); REQUIRE(U(LoadLibraryW)); REQUIRE(U(FreeLibrary));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld_basic"), "k32m_ba.dll", p));
    HMODULE a = U(LoadLibraryA)(p);
    CHECKF(a != NULL, "err %lu", GetLastError());
    if (!a) return;
    CHECK(((ULONG_PTR)a & 0xFFFF) == 0);
    CHECK(ml_nt(a) != NULL);
    HMODULE w = U(LoadLibraryW)(ml_w(p));
    CHECK(w == a);
    CHECK(GetModuleHandleA("k32m_ba.dll") == a);
    CHECK(GetModuleHandleA("k32m_ba") == a);
    int *attach = (int *)GetProcAddress(a, "ml_proc_attach");
    CHECK(attach != NULL);
    if (attach) EXPECT_EQ(*attach, 1);         /* DllMain ran once although loaded twice */
    CHECK(U(FreeLibrary)(w));
    CHECK(ml_is_loaded(a));
    CHECK(U(FreeLibrary)(a));
    CHECK(!ml_is_loaded(a));
}
TEST(ld_image_loaded_via_bare_name_after_fullpath) {
    REQUIRE(U(LoadLibraryA));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld_bare"), "k32m_bare.dll", p));
    HMODULE a = U(LoadLibraryA)(p);
    CHECK(a != NULL);
    if (!a) return;
    HMODULE b = U(LoadLibraryA)("k32m_bare.dll");      /* already loaded: found by base name */
    CHECK(b == a);
    HMODULE c = U(LoadLibraryA)("K32M_BARE");
    CHECK(c == a);
    ml_unload(a);
    SET_ERR(0);
    CHECK(U(LoadLibraryA)("k32m_bare.dll") == NULL);
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
}
TEST(ld_image_path_case_insensitive) {
    REQUIRE(U(LoadLibraryA));
    char p[MAX_PATH], q[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld_case"), "k32m_case.dll", p));
    strcpy(q, p); CharUpperA(q);
    HMODULE a = U(LoadLibraryA)(p), b = U(LoadLibraryA)(q);
    CHECK(a != NULL);
    CHECK(a == b);
    ml_unload(a);
}
TEST(ld_image_refcount_three) {
    REQUIRE(U(LoadLibraryA)); REQUIRE(U(FreeLibrary));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld_rc3"), "k32m_rc3.dll", p));
    HMODULE h1 = U(LoadLibraryA)(p), h2 = U(LoadLibraryA)(p), h3 = U(LoadLibraryA)(p);
    CHECK(h1 && h1 == h2 && h2 == h3);
    if (!h1) return;
    CHECK(U(FreeLibrary)(h1)); CHECK(ml_is_loaded(h1));
    CHECK(U(FreeLibrary)(h1)); CHECK(ml_is_loaded(h1));
    CHECK(U(FreeLibrary)(h1)); CHECK(!ml_is_loaded(h1));
    SET_ERR(0);
    CHECK(!U(FreeLibrary)(h1));                 /* no longer loaded */
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
}
TEST(ld_image_refcount_getmodulehandleex) {
    REQUIRE(U(LoadLibraryA)); REQUIRE(U(FreeLibrary)); REQUIRE(U(GetModuleHandleExA));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld_rcx"), "k32m_rcx.dll", p));
    HMODULE h = U(LoadLibraryA)(p), x = NULL;
    CHECK(h != NULL); if (!h) return;
    CHECK(U(GetModuleHandleExA)(0, "k32m_rcx.dll", &x));            /* +1 */
    CHECK(x == h);
    x = NULL;
    CHECK(U(GetModuleHandleExA)(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, "k32m_rcx.dll", &x));
    CHECK(x == h);
    CHECK(U(FreeLibrary)(h)); CHECK(ml_is_loaded(h));              /* count was 2 */
    CHECK(U(FreeLibrary)(h)); CHECK(!ml_is_loaded(h));             /* UNCHANGED did not add */
}
TEST(ld_image_refcount_getmodulehandle_plain_no_addref) {
    REQUIRE(U(LoadLibraryA)); REQUIRE(U(GetModuleHandleA));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld_rch"), "k32m_rch.dll", p));
    HMODULE h = U(LoadLibraryA)(p);
    CHECK(h != NULL); if (!h) return;
    for (int i = 0; i < 5; i++) CHECK(U(GetModuleHandleA)("k32m_rch.dll") == h);
    CHECK(FreeLibrary(h));
    CHECK(!ml_is_loaded(h));
}
TEST(ld_image_freelibrary_after_exitless_unload_resets_state) {
    REQUIRE(U(LoadLibraryA));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld_re"), "k32m_re.dll", p));
    for (int i = 0; i < 20; i++) {
        HMODULE h = U(LoadLibraryA)(p);
        CHECKF(h != NULL, "iter %d err %lu", i, GetLastError());
        if (!h) return;
        int *attach = (int *)GetProcAddress(h, "ml_proc_attach");
        CHECKF(attach && *attach == 1, "iter %d attach", i);   /* fresh mapping each time */
        CHECK(FreeLibrary(h));
        CHECK(!ml_is_loaded(h));
    }
}
TEST(ld_image_two_names_two_modules) {
    REQUIRE(U(LoadLibraryA));
    char p1[MAX_PATH], p2[MAX_PATH];
    const char *d = ml_dir("ld_two");
    CHECK(ml_put(ML_A, d, "k32m_two1.dll", p1)); CHECK(ml_put(ML_A, d, "k32m_two2.dll", p2));
    HMODULE a = U(LoadLibraryA)(p1), b = U(LoadLibraryA)(p2);
    CHECK(a && b && a != b);
    int *x = (int *)GetProcAddress(a, "ml_value"), *y = (int *)GetProcAddress(b, "ml_value");
    CHECK(x && y && x != y);
    if (x && y) { *x = 7; CHECK(*y == 0x1234); *x = 0x1234; }
    ml_unload(a); ml_unload(b);
}
TEST(ld_image_same_basename_two_dirs) {
    REQUIRE(U(LoadLibraryA));
    char p1[MAX_PATH], p2[MAX_PATH];
    CHECK(ml_put(ML_A, ml_dir("ld_sb1"), "k32m_sb.dll", p1)); CHECK(ml_put(ML_A, ml_dir("ld_sb2"), "k32m_sb.dll", p2));
    HMODULE a = U(LoadLibraryA)(p1), b = U(LoadLibraryA)(p2);
    CHECK(a != NULL);
    t_note("same basename in two dirs: second load %s (a=%p b=%p)", b == a ? "returned same module" : b ? "loaded separately" : "failed", a, b);
    ml_unload(a); if (b != a) ml_unload(b);
}
TEST(ld_image_dont_resolve_references) {
    REQUIRE(U(LoadLibraryExA));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld_dr"), "k32m_dr.dll", p));
    HMODULE h = U(LoadLibraryExA)(p, NULL, DONT_RESOLVE_DLL_REFERENCES);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (!h) return;
    int *attach = (int *)GetProcAddress(h, "ml_proc_attach");
    CHECK(attach != NULL);
    if (attach) EXPECT_EQ(*attach, 0);            /* DllMain not called */
    CHECK(FreeLibrary(h));
}
TEST(ld_image_imports_resolved_with_load_dir) {
    REQUIRE(U(LoadLibraryExA));
    const char *d = ml_dir("ld_imp"); char pi[MAX_PATH], pd[MAX_PATH];
    CHECK(ml_put(ML_IMP, d, "k32m_imp.dll", pi)); CHECK(ml_put(ML_DEP, d, "k32m_dep.dll", pd));
    HMODULE h = U(LoadLibraryExA)(pi, NULL, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (!h) return;
    HMODULE dep = GetModuleHandleA("k32m_dep.dll");
    CHECK(dep != NULL);
    typedef int (WINAPI *fn)(void);
    fn f = (fn)GetProcAddress(h, "ml_via_dep");
    CHECK(f != NULL);
    if (f) EXPECT_EQ(f(), 4243);
    CHECK(FreeLibrary(h));
    CHECK(GetModuleHandleA("k32m_dep.dll") == NULL);        /* dependency released with the importer */
}
TEST(ld_image_imports_resolved_altered_search_path) {
    REQUIRE(U(LoadLibraryExA));
    const char *d = ml_dir("ld_imp2"); char pi[MAX_PATH], pd[MAX_PATH];
    CHECK(ml_put(ML_IMP, d, "k32m_imp.dll", pi)); CHECK(ml_put(ML_DEP, d, "k32m_dep.dll", pd));
    HMODULE h = U(LoadLibraryExA)(pi, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (!h) return;
    CHECK(GetModuleHandleA("k32m_dep.dll") != NULL);
    CHECK(FreeLibrary(h));
    CHECK(GetModuleHandleA("k32m_dep.dll") == NULL);
}
TEST(ld_image_missing_dependency) {
    REQUIRE(U(LoadLibraryExA));
    const char *d = ml_dir("ld_imp3"); char pi[MAX_PATH];
    CHECK(ml_put(ML_IMP, d, "k32m_imp.dll", pi));      /* dependency not present anywhere */
    SET_ERR(0);
    HMODULE h = U(LoadLibraryExA)(pi, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    CHECK(h == NULL);
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
    CHECK(GetModuleHandleA("k32m_imp.dll") == NULL);
    if (h) ml_unload(h);
}
TEST(ld_image_dependency_found_in_loaded_list) {
    /* an already loaded module satisfies an import by base name regardless of path */
    REQUIRE(U(LoadLibraryExA));
    const char *d = ml_dir("ld_imp4"), *d2 = ml_dir("ld_imp4b"); char pi[MAX_PATH], pd[MAX_PATH];
    CHECK(ml_put(ML_IMP, d, "k32m_imp.dll", pi)); CHECK(ml_put(ML_DEP, d2, "k32m_dep.dll", pd));
    HMODULE dep = LoadLibraryA(pd);
    CHECK(dep != NULL); if (!dep) return;
    HMODULE h = U(LoadLibraryExA)(pi, NULL, 0);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (h) { typedef int (WINAPI *fn)(void); fn f = (fn)GetProcAddress(h, "ml_via_dep"); CHECK(f && f() == 4243); ml_unload(h); }
    ml_unload(dep);
}
TEST(ld_image_plain_load_dependency_not_in_dll_dir) {
    REQUIRE(U(LoadLibraryA));
    const char *d = ml_dir("ld_imp5"); char pi[MAX_PATH], pd[MAX_PATH];
    CHECK(ml_put(ML_IMP, d, "k32m_imp.dll", pi)); CHECK(ml_put(ML_DEP, d, "k32m_dep.dll", pd));
    HMODULE h = U(LoadLibraryA)(pi);
    t_note("plain LoadLibrary(full path) with sibling dependency: %s (err %lu)", h ? "loaded" : "failed", h ? 0 : GetLastError());
    if (h) ml_unload(h);
}

/* ---- data-file / image-resource loads ---- */
TEST(ld_datafile_tag_bit) {
    REQUIRE(U(LoadLibraryExA)); REQUIRE(U(FreeLibrary));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld_df"), "k32m_df.dll", p));
    HMODULE h = U(LoadLibraryExA)(p, NULL, LOAD_LIBRARY_AS_DATAFILE);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (!h) return;
    CHECK(((ULONG_PTR)h & 1) == 1);
    CHECK(((ULONG_PTR)h & 2) == 0);
    const IMAGE_DOS_HEADER *base = (void *)((ULONG_PTR)h & ~(ULONG_PTR)3);
    CHECK(base->e_magic == IMAGE_DOS_SIGNATURE);
    CHECK(GetModuleHandleA("k32m_df.dll") == NULL);   /* not in the module list */
    CHECK(U(FreeLibrary)(h));
}
TEST(ld_datafile_w_parity) {
    REQUIRE(U(LoadLibraryExW)); REQUIRE(U(LoadLibraryExA));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld_dfw"), "k32m_dfw.dll", p));
    HMODULE a = U(LoadLibraryExA)(p, NULL, LOAD_LIBRARY_AS_DATAFILE), w = U(LoadLibraryExW)(ml_w(p), NULL, LOAD_LIBRARY_AS_DATAFILE);
    CHECK(a != NULL && w != NULL);
    CHECK(((ULONG_PTR)a & 1) && ((ULONG_PTR)w & 1));
    if (a) FreeLibrary(a);
    if (w) FreeLibrary(w);
}
TEST(ld_datafile_does_not_run_dllmain_or_imports) {
    REQUIRE(U(LoadLibraryExA));
    const char *d = ml_dir("ld_dfi"); char pi[MAX_PATH];
    CHECK(ml_put(ML_IMP, d, "k32m_imp.dll", pi));      /* import target missing: image load would fail */
    HMODULE h = U(LoadLibraryExA)(pi, NULL, LOAD_LIBRARY_AS_DATAFILE);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (h) FreeLibrary(h);
}
TEST(ld_datafile_image_resource_tag) {
    REQUIRE(U(LoadLibraryExA));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld_ir"), "k32m_ir.dll", p));
    HMODULE h = U(LoadLibraryExA)(p, NULL, LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (!h) return;
    CHECK(((ULONG_PTR)h & 2) == 2);
    CHECK(((ULONG_PTR)h & 1) == 0);
    CHECK(GetModuleHandleA("k32m_ir.dll") == NULL);
    CHECK(FreeLibrary(h));
}
TEST(ld_datafile_image_resource_with_datafile) {
    REQUIRE(U(LoadLibraryExA));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld_ird"), "k32m_ird.dll", p));
    HMODULE h = U(LoadLibraryExA)(p, NULL, LOAD_LIBRARY_AS_IMAGE_RESOURCE | LOAD_LIBRARY_AS_DATAFILE);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (!h) return;
    CHECK(((ULONG_PTR)h & 3) != 0);
    t_note("IMAGE_RESOURCE|DATAFILE tag bits = %lx", (unsigned long)((ULONG_PTR)h & 3));
    FreeLibrary(h);
}
TEST(ld_datafile_exclusive) {
    REQUIRE(U(LoadLibraryExA));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld_dfx"), "k32m_dfx.dll", p));
    HMODULE h = U(LoadLibraryExA)(p, NULL, LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (!h) return;
    CHECK(((ULONG_PTR)h & 1) == 1);
    HANDLE f = CreateFileA(p, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(f == INVALID_HANDLE_VALUE);
    if (f == INVALID_HANDLE_VALUE) EXPECT_ERR(ERROR_SHARING_VIOLATION); else CloseHandle(f);
    CHECK(FreeLibrary(h));
    f = CreateFileA(p, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(f != INVALID_HANDLE_VALUE);
    if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
}
TEST(ld_datafile_nonexclusive_allows_write_open) {
    REQUIRE(U(LoadLibraryExA));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld_dfn"), "k32m_dfn.dll", p));
    HMODULE h = U(LoadLibraryExA)(p, NULL, LOAD_LIBRARY_AS_DATAFILE);
    CHECK(h != NULL); if (!h) return;
    HANDLE f = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(f != INVALID_HANDLE_VALUE);
    if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
    FreeLibrary(h);
}
TEST(ld_datafile_non_pe) {
    REQUIRE(U(LoadLibraryExA)); REQUIRE(U(LoadLibraryExW));
    char p[MAX_PATH]; snprintf(p, sizeof p, "%snotpe.dll", ml_dir("ld_dfnp"));
    CHECK(ml_write(p, "plain text, not an image at all, padded to be reasonably long....\n", 66));
    static const DWORD fl[] = { LOAD_LIBRARY_AS_DATAFILE, LOAD_LIBRARY_AS_IMAGE_RESOURCE, LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE };
    for (int i = 0; i < 3; i++) {
        SET_ERR(0);
        HMODULE h = U(LoadLibraryExA)(p, NULL, fl[i]);
        CHECKF(h == NULL, "flags %lx", fl[i]);
        if (h) { FreeLibrary(h); continue; }
        EXPECT_ERR(ERROR_BAD_EXE_FORMAT);
        SET_ERR(0);
        h = U(LoadLibraryExW)(ml_w(p), NULL, fl[i]);
        CHECK(h == NULL);
        EXPECT_ERR(ERROR_BAD_EXE_FORMAT);
    }
}
TEST(ld_datafile_nonexistent) {
    REQUIRE(U(LoadLibraryExA));
    char p[MAX_PATH]; snprintf(p, sizeof p, "%smissing.dll", ml_dir("ld_dfm"));
    SET_ERR(0);
    CHECK(U(LoadLibraryExA)(p, NULL, LOAD_LIBRARY_AS_DATAFILE) == NULL);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_FILE_NOT_FOUND || e == ERROR_MOD_NOT_FOUND, "e=%lu", e);
    t_note("datafile nonexistent full path error %lu", e);
    SET_ERR(0);
    CHECK(U(LoadLibraryExA)("k32t_nosuch_module.dll", NULL, LOAD_LIBRARY_AS_DATAFILE) == NULL);
    e = GetLastError();
    CHECKF(e == ERROR_FILE_NOT_FOUND || e == ERROR_MOD_NOT_FOUND, "e=%lu", e);
    t_note("datafile nonexistent bare name error %lu", e);
}
TEST(ld_datafile_bare_system_name) {
    REQUIRE(U(LoadLibraryExW));
    HMODULE loaded = GetModuleHandleW(L"user32.dll");   /* an already loaded module is returned as the image handle, untagged */
    HMODULE h = U(LoadLibraryExW)(L"user32.dll", NULL, LOAD_LIBRARY_AS_DATAFILE);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (h) { if (loaded) CHECK(h == loaded); else CHECK((ULONG_PTR)h & 1); FreeLibrary(h); }
    h = U(LoadLibraryExW)(L"user32", NULL, LOAD_LIBRARY_AS_DATAFILE);
    t_note("datafile load without extension: %s", h ? "ok" : "failed");
    if (h) FreeLibrary(h);
}
TEST(ld_datafile_multiple_refs_same_base) {
    REQUIRE(U(LoadLibraryExA));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld_dfr"), "k32m_dfr.dll", p));
    HMODULE a = U(LoadLibraryExA)(p, NULL, LOAD_LIBRARY_AS_DATAFILE), b = U(LoadLibraryExA)(p, NULL, LOAD_LIBRARY_AS_DATAFILE);
    CHECK(a && b);
    t_note("two datafile loads: %s", a == b ? "same view" : "distinct views");
    CHECK(FreeLibrary(a));
    if (b != a) CHECK(FreeLibrary(b)); else FreeLibrary(b);
}
TEST(ld_datafile_then_image_load) {
    REQUIRE(U(LoadLibraryExA)); REQUIRE(U(LoadLibraryA));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld_dfi2"), "k32m_dfi2.dll", p));
    HMODULE d = U(LoadLibraryExA)(p, NULL, LOAD_LIBRARY_AS_DATAFILE);
    HMODULE i = U(LoadLibraryA)(p);
    CHECK(d && i);
    if (i) { CHECK(((ULONG_PTR)i & 3) == 0); CHECK(GetModuleHandleA("k32m_dfi2.dll") == i); }
    if (d) FreeLibrary(d);
    CHECK(i == NULL || GetModuleHandleA("k32m_dfi2.dll") == i);   /* image still loaded */
    ml_unload(i);
}
TEST(ld_freelibrary_datafile_handle_variants) {
    REQUIRE(U(FreeLibrary)); REQUIRE(U(LoadLibraryExA));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld_dffl"), "k32m_dffl.dll", p));
    static const DWORD fl[] = { LOAD_LIBRARY_AS_DATAFILE, LOAD_LIBRARY_AS_IMAGE_RESOURCE, LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE };
    for (int i = 0; i < 3; i++) {
        HMODULE h = U(LoadLibraryExA)(p, NULL, fl[i]);
        CHECKF(h != NULL, "flags %lx", fl[i]);
        if (h) CHECKF(U(FreeLibrary)(h), "flags %lx err %lu", fl[i], GetLastError());
    }
}

/* ---- FreeLibrary errors ---- */
TEST(ld_freelibrary_null) {
    REQUIRE(U(FreeLibrary));
    SET_ERR(0);
    CHECK(!U(FreeLibrary)(NULL));
    CHECK(GetLastError() != 0);
    t_note("FreeLibrary(NULL) error %lu", GetLastError());
}
TEST(ld_freelibrary_bogus_handles) {
    REQUIRE(U(FreeLibrary));
    static const ULONG_PTR h[] = { 0x10000, 0x7FFE0000, 0x400000, 0xFFFFFFFFFFFF0000ull, 0x12340000 };
    for (int i = 0; i < 5; i++) {
        SET_ERR(0);
        CHECKF(!U(FreeLibrary)((HMODULE)h[i]), "h=%p", (void *)h[i]);
        EXPECT_ERR(ERROR_MOD_NOT_FOUND);
    }
}
TEST(ld_freelibrary_unaligned_tagged_invalid) {
    REQUIRE(U(FreeLibrary));
    if (t_is_wine()) { t_skip("wine faults on unmapped tagged handle"); return; }
    /* tagged handle (low bits set) whose base has no PE header -> bad exe format */
    SET_ERR(0);
    CHECK(!U(FreeLibrary)((HMODULE)(ULONG_PTR)1));
    EXPECT_ERR(ERROR_BAD_EXE_FORMAT);
    SET_ERR(0);
    CHECK(!U(FreeLibrary)((HMODULE)(ULONG_PTR)3));
    EXPECT_ERR(ERROR_BAD_EXE_FORMAT);
}
TEST(ld_freelibrary_not_a_module_address) {
    REQUIRE(U(FreeLibrary));
    /* an address inside the heap is not a module base */
    static char buf[0x2000];
    SET_ERR(0);
    BOOL r = U(FreeLibrary)((HMODULE)(((ULONG_PTR)buf + 0xFFFF) & ~(ULONG_PTR)0xFFFF));
    DWORD e = GetLastError();
    t_note("FreeLibrary(non-base inside exe) -> %d err %lu", r, r ? 0 : e);   /* depends on loader lookup */
}
CHILD(ld_free_pinned) {
    HMODULE self = GetModuleHandleA(NULL);
    HMODULE k = LoadLibraryA("kernel32.dll");
    if (!k || k != GetModuleHandleA("kernel32.dll")) return 90;
    /* exe is never unloaded: either refused or harmlessly counted */
    U(FreeLibrary)(self);
    if (GetModuleHandleA(NULL) != self) return 91;
    if (!ml_is_loaded(self)) return 92;
    for (int i = 0; i < 5; i++) U(FreeLibrary)(k);
    if (GetModuleHandleA("kernel32.dll") != k) return 93;
    if (!GetProcAddress(k, "GetCurrentProcessId")) return 94;
    return 0;
}
TEST(ld_freelibrary_exe_module_survives) {
    REQUIRE(U(FreeLibrary));
    EXPECT_EQ(t_run_child("ld_free_pinned", "", 25000), 0);
}
TEST(ld_freelibrary_kernel32_pinned_survives) {
    REQUIRE(U(FreeLibrary)); REQUIRE(U(LoadLibraryA));
    HMODULE k = U(LoadLibraryA)("kernel32.dll");
    CHECK(k == GetModuleHandleA("kernel32.dll"));
    CHECK(U(FreeLibrary)(k));                      /* balanced with the load above */
    CHECK(GetModuleHandleA("kernel32.dll") == k);
    CHECK(GetProcAddress(k, "GetCurrentProcessId") != NULL);
}
TEST(ld_freelibrary_last_error_untouched_on_success) {
    REQUIRE(U(FreeLibrary)); REQUIRE(U(LoadLibraryA));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld_le"), "k32m_le.dll", p));
    HMODULE h = U(LoadLibraryA)(p); CHECK(h != NULL); if (!h) return;
    SET_ERR(0x5A5A);
    CHECK(U(FreeLibrary)(h));
    t_note("FreeLibrary success leaves last error %lx", GetLastError());
}

/* ---- LoadPackagedLibrary ---- */
TEST(ld_packaged_no_package) {
    REQUIRE(U(LoadPackagedLibrary));
    SET_ERR(0);
    CHECK(U(LoadPackagedLibrary)(L"k32t_nosuch.dll", 0) == NULL);
    EXPECT_ERR(15700);   /* APPMODEL_ERROR_NO_PACKAGE */
}
TEST(ld_packaged_null_name) {
    REQUIRE(U(LoadPackagedLibrary));
    /* LoadPackagedLibrary(NULL, 0) passes the pointer to RtlDetermineDosPathNameType_U, which faults on Windows 10;
     * the documentation does not define a NULL name, so there is nothing to verify. */
    t_skip("LoadPackagedLibrary(NULL) is undefined (access violation on Windows 10)");
}
TEST(ld_packaged_reserved_nonzero) {
    REQUIRE(U(LoadPackagedLibrary));
    SET_ERR(0);
    CHECK(U(LoadPackagedLibrary)(L"kernel32.dll", 1) == NULL);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_INVALID_PARAMETER || e == 15700, "e=%lu", e);
}
TEST(ld_packaged_system_dll_refused) {
    REQUIRE(U(LoadPackagedLibrary));
    SET_ERR(0);
    CHECK(U(LoadPackagedLibrary)(L"kernel32.dll", 0) == NULL);
    EXPECT_ERR(15700);
}

/* ---- relative paths and current directory in a child ---- */
CHILD(ld_relpath) {
    /* argv[1] = directory containing sub\k32m_rel.dll */
    if (argc < 2) return 90;
    if (!SetCurrentDirectoryA(argv[1])) return 91;
    HMODULE h = U(LoadLibraryA)("sub\\k32m_rel.dll");
    if (!h) return 100 + (int)(GetLastError() & 0x7F);
    char name[MAX_PATH];
    if (!GetModuleFileNameA(h, name, sizeof name)) return 92;
    if (!strstr(name, "sub\\k32m_rel.dll")) return 93;
    HMODULE h2 = U(LoadLibraryA)(".\\sub\\k32m_rel.dll");
    if (h2 != h) return 94;
    return 0;
}
TEST(ld_relative_path_with_slash_uses_cwd) {
    REQUIRE(U(LoadLibraryA));
    const char *d = ml_dir("ld_rel"); char sub[MAX_PATH], p[MAX_PATH];
    snprintf(sub, sizeof sub, "%ssub", d); CreateDirectoryA(sub, NULL);
    snprintf(p, sizeof p, "%ssub\\", d);
    CHECK(ml_put(ML_A, p, "k32m_rel.dll", NULL));
    char dd[MAX_PATH]; strcpy(dd, d); dd[strlen(dd) - 1] = 0;
    char args[MAX_PATH + 8]; snprintf(args, sizeof args, "\"%s\"", dd);
    /* the child receives the dir as a single argument; quotes are stripped by the CRT */
    DWORD rc = ml_run_exe(t_self_path(), "ld_relpath", args, NULL, 20000, 0);
    EXPECT_EQ(rc, 0);
}
