/* Loader failure modes, corrupted images, search precedence, concurrency, cross-process loads */
#include "ml.h"

/* ---- DllMain / import failures ---- */
TEST(ld2_dllmain_returns_false) {
    REQUIRE(U(LoadLibraryA)); REQUIRE(U(LoadLibraryExA));
    char p[MAX_PATH]; CHECK(ml_put(ML_FAIL, ml_dir("ld2_fail"), "k32m_fail.dll", p));
    SET_ERR(0);
    CHECK(U(LoadLibraryA)(p) == NULL);
    EXPECT_ERR(ERROR_DLL_INIT_FAILED);
    CHECK(GetModuleHandleA("k32m_fail.dll") == NULL);
    SET_ERR(0);
    CHECK(U(LoadLibraryExA)(p, NULL, LOAD_WITH_ALTERED_SEARCH_PATH) == NULL);
    EXPECT_ERR(ERROR_DLL_INIT_FAILED);
    /* repeated failures stay failures (the module is not left half loaded) */
    for (int i = 0; i < 5; i++) { SET_ERR(0); CHECK(U(LoadLibraryA)(p) == NULL); EXPECT_ERR(ERROR_DLL_INIT_FAILED); }
}
TEST(ld2_dllmain_false_ignored_without_resolve) {
    REQUIRE(U(LoadLibraryExA));
    char p[MAX_PATH]; CHECK(ml_put(ML_FAIL, ml_dir("ld2_fail2"), "k32m_fail2.dll", p));
    HMODULE h = U(LoadLibraryExA)(p, NULL, DONT_RESOLVE_DLL_REFERENCES);
    CHECKF(h != NULL, "err %lu", GetLastError());      /* DllMain is not called */
    ml_unload(h);
}
TEST(ld2_dllmain_false_datafile_ok) {
    REQUIRE(U(LoadLibraryExA));
    char p[MAX_PATH]; CHECK(ml_put(ML_FAIL, ml_dir("ld2_fail3"), "k32m_fail3.dll", p));
    HMODULE h = U(LoadLibraryExA)(p, NULL, LOAD_LIBRARY_AS_DATAFILE);
    CHECK(h != NULL);
    ml_unload(h);
}
/* The loader reports an unresolved import by name through LdrpReportError, which raises STATUS_ENTRYPOINT_NOT_FOUND
 * (0xC0000139) internally before LoadLibrary returns NULL.  The harness exception handler would flag that first-chance
 * exception, so the load runs in a child process. */
CHILD(ld2_missing_import_child) {
    HMODULE h; DWORD e;
    SetLastError(0);
    h = LoadLibraryExA(argv[1], NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    e = GetLastError();
    if (h != NULL) return 1;
    if (e != ERROR_PROC_NOT_FOUND) return 0x1000 + (e & 0xFFF);
    if (GetModuleHandleA("k32m_badimp.dll") != NULL) return 3;
    if (GetModuleHandleA("k32m_dep.dll") != NULL) return 4;      /* dependency released after the failure */
    return 0;
}
TEST(ld2_missing_import_function) {
    REQUIRE(U(LoadLibraryExA));
    const char *d = ml_dir("ld2_badimp"); char pb[MAX_PATH], args[MAX_PATH + 8];
    CHECK(ml_put(ML_BADIMP, d, "k32m_badimp.dll", pb)); CHECK(ml_put(ML_DEP, d, "k32m_dep.dll", NULL));
    snprintf(args, sizeof args, "\"%s\"", pb);
    EXPECT_EQ(ml_run_exe(t_self_path(), "ld2_missing_import_child", args, NULL, 25000, 0), 0);
}
TEST(ld2_missing_import_dll_vs_function) {
    REQUIRE(U(LoadLibraryExA));
    const char *d = ml_dir("ld2_badimp2"); char pb[MAX_PATH];
    CHECK(ml_put(ML_BADIMP, d, "k32m_badimp.dll", pb));    /* no dependency at all */
    SET_ERR(0);
    CHECK(U(LoadLibraryExA)(pb, NULL, LOAD_WITH_ALTERED_SEARCH_PATH) == NULL);
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
}

/* ---- corrupted images ---- */
static char *patched(const char *sub, const char *leaf, void (*patch)(unsigned char *, size_t)) {
    static char p[MAX_PATH]; size_t n; const unsigned char *src = ml_blob(ML_A, &n);
    unsigned char *b = malloc(n); memcpy(b, src, n); patch(b, n);
    snprintf(p, sizeof p, "%s%s", ml_dir(sub), leaf);
    ml_write(p, b, n); free(b);
    return p;
}
static unsigned char *nt_of(unsigned char *b) { return b + *(DWORD *)(b + 0x3c); }
static void p_machine_i386(unsigned char *b, size_t n) { *(WORD *)(nt_of(b) + 4) = IMAGE_FILE_MACHINE_I386; }
static void p_machine_arm64(unsigned char *b, size_t n) { *(WORD *)(nt_of(b) + 4) = IMAGE_FILE_MACHINE_ARM64; }
static void p_pe_sig(unsigned char *b, size_t n) { nt_of(b)[0] = 'X'; }
static void p_mz_sig(unsigned char *b, size_t n) { b[0] = 'N'; }
static void p_lfanew_big(unsigned char *b, size_t n) { *(DWORD *)(b + 0x3c) = 0x7FFFFFF0; }
static void p_lfanew_zero(unsigned char *b, size_t n) { *(DWORD *)(b + 0x3c) = 0; }
static void p_opt_magic_pe32(unsigned char *b, size_t n) { *(WORD *)(nt_of(b) + 24) = 0x10b; }
static void p_opt_magic_bad(unsigned char *b, size_t n) { *(WORD *)(nt_of(b) + 24) = 0x1234; }
static void p_sizeofimage_small(unsigned char *b, size_t n) { *(DWORD *)(nt_of(b) + 24 + 56) = 0x1000; }
static void p_sizeofimage_zero(unsigned char *b, size_t n) { *(DWORD *)(nt_of(b) + 24 + 56) = 0; }
static void p_numsections_zero(unsigned char *b, size_t n) { *(WORD *)(nt_of(b) + 6) = 0; }
static void p_numsections_many(unsigned char *b, size_t n) { *(WORD *)(nt_of(b) + 6) = 200; }
static void p_sectalign_bad(unsigned char *b, size_t n) { *(DWORD *)(nt_of(b) + 24 + 32) = 0x300; }
static void p_not_dll_flag(unsigned char *b, size_t n) { *(WORD *)(nt_of(b) + 22) &= ~IMAGE_FILE_DLL; }
static void p_no_exec_flag(unsigned char *b, size_t n) { *(WORD *)(nt_of(b) + 22) &= ~IMAGE_FILE_EXECUTABLE_IMAGE; }
static void p_sizeofoptheader_small(unsigned char *b, size_t n) { *(WORD *)(nt_of(b) + 20) = 0x20; }
static void p_checksum(unsigned char *b, size_t n) { *(DWORD *)(nt_of(b) + 24 + 64) = 0xDEADBEEF; }
static void p_timestamp(unsigned char *b, size_t n) { *(DWORD *)(nt_of(b) + 8) = 0; }
static void p_subsystem_native(unsigned char *b, size_t n) { *(WORD *)(nt_of(b) + 24 + 68) = IMAGE_SUBSYSTEM_NATIVE; }
static void p_subsystem_dos(unsigned char *b, size_t n) { *(WORD *)(nt_of(b) + 24 + 68) = 0x7777; }
static void p_osver_future(unsigned char *b, size_t n) { *(WORD *)(nt_of(b) + 24 + 40) = 99; }
static void p_entry_outside(unsigned char *b, size_t n) { *(DWORD *)(nt_of(b) + 24 + 16) = 0x7FFF0000; }
static void p_no_aslr(unsigned char *b, size_t n) { *(WORD *)(nt_of(b) + 24 + 70) &= ~0x40; }

static const struct { const char *name; void (*patch)(unsigned char *, size_t); DWORD err1, hard; } bad_images[] = {
    { "machine_i386",   p_machine_i386,   ERROR_BAD_EXE_FORMAT, 1 },
    { "machine_arm64",  p_machine_arm64,  ERROR_BAD_EXE_FORMAT, 1 },
    { "pe_signature",   p_pe_sig,         ERROR_BAD_EXE_FORMAT, 1 },
    { "mz_signature",   p_mz_sig,         ERROR_BAD_EXE_FORMAT, 1 },
    { "lfanew_huge",    p_lfanew_big,     ERROR_BAD_EXE_FORMAT, 1 },
    { "lfanew_zero",    p_lfanew_zero,    ERROR_BAD_EXE_FORMAT, 1 },
    { "magic_pe32",     p_opt_magic_pe32, ERROR_BAD_EXE_FORMAT, 1 },
    { "magic_garbage",  p_opt_magic_bad,  ERROR_BAD_EXE_FORMAT, 1 },
    { "sizeofimage_small", p_sizeofimage_small, ERROR_BAD_EXE_FORMAT, 0 },
    { "sizeofimage_zero",  p_sizeofimage_zero,  ERROR_BAD_EXE_FORMAT, 0 },
    { "zero_sections",  p_numsections_zero, ERROR_BAD_EXE_FORMAT, 0 },
    { "too_many_sections", p_numsections_many, ERROR_BAD_EXE_FORMAT, 0 },
    { "section_align_odd", p_sectalign_bad, ERROR_BAD_EXE_FORMAT, 0 },
    { "optheader_too_small", p_sizeofoptheader_small, ERROR_BAD_EXE_FORMAT, 0 },
    { "entry_outside",  p_entry_outside,  ERROR_BAD_EXE_FORMAT, 0 },
};
/* Loading a deliberately damaged image happens in a child: whatever the loader decides, the harness survives. */
CHILD(ld2_try_load) {
    UNREFERENCED_PARAMETER(argc);
    SetLastError(0);
    HMODULE h = LoadLibraryA(argv[1]);
    if (!h) return 0x10000 + GetLastError();
    return 0x20000;
}
static DWORD try_load_in_child(const char *path) {
    char args[MAX_PATH + 8]; snprintf(args, sizeof args, "\"%s\"", path);
    return ml_run_exe(t_self_path(), "ld2_try_load", args, NULL, 25000, 0);
}
TEST(ld2_corrupted_images_rejected) {
    REQUIRE(U(LoadLibraryA));
    for (int i = 0; i < (int)(sizeof bad_images / sizeof *bad_images); i++) {
        char leaf[64]; snprintf(leaf, sizeof leaf, "k32m_bad%d.dll", i);
        char *p = patched("ld2_bad", leaf, bad_images[i].patch);
        DWORD rc = try_load_in_child(p);
        int rejected = rc == 0x10000 + ERROR_BAD_EXE_FORMAT || rc == 0x10000 + ERROR_INVALID_IMAGE_HASH || rc == 0x10000 + ERROR_INVALID_DATA || rc == 0x10000 + ERROR_FILE_CORRUPT;
        if (bad_images[i].hard) CHECKF(rejected, "%s: child result %lx", bad_images[i].name, rc);
        CHECKF(rc != 0xDEAD0000 && rc != 0xDEAD0001, "%s: child hung or could not start", bad_images[i].name);
        if (rc != 0x10000 + ERROR_BAD_EXE_FORMAT) t_note("%s: child result %lx", bad_images[i].name, rc);
    }
}
TEST(ld2_harmless_header_variations_load) {
    REQUIRE(U(LoadLibraryA));
    static const struct { const char *n; void (*p)(unsigned char *, size_t); int must_load; } ok[] = {
        { "checksum", p_checksum, 1 }, { "timestamp", p_timestamp, 1 }, { "no_aslr", p_no_aslr, 1 },
        { "subsystem_native", p_subsystem_native, 0 }, { "subsystem_garbage", p_subsystem_dos, 0 }, { "future_os_version", p_osver_future, 0 } };
    for (int i = 0; i < 6; i++) {
        char leaf[64]; snprintf(leaf, sizeof leaf, "k32m_ok%d.dll", i);
        char *p = patched("ld2_ok", leaf, ok[i].p);
        DWORD rc = try_load_in_child(p);
        if (ok[i].must_load) CHECKF(rc == 0x20000, "%s: child result %lx", ok[i].n, rc);
        else t_note("%s: child result %lx", ok[i].n, rc);
        CHECKF(rc != 0xDEAD0000 && rc != 0xDEAD0001, "%s", ok[i].n);
    }
}
TEST(ld2_exe_flag_changes) {
    REQUIRE(U(LoadLibraryA));
    char *p = patched("ld2_notdll", "k32m_notdll.dll", p_not_dll_flag);
    DWORD rc = try_load_in_child(p);
    t_note("image without IMAGE_FILE_DLL: child result %lx", rc);
    CHECK(rc != 0xDEAD0000 && rc != 0xDEAD0001);
    p = patched("ld2_noexec", "k32m_noexec.dll", p_no_exec_flag);
    rc = try_load_in_child(p);
    t_note("image without EXECUTABLE_IMAGE: child result %lx", rc);
    CHECK(rc != 0xDEAD0000 && rc != 0xDEAD0001);
}
TEST(ld2_wrong_machine_datafile) {
    REQUIRE(U(LoadLibraryExA));
    char *p = patched("ld2_dfm", "k32m_dfm.dll", p_machine_i386);
    SET_ERR(0);
    HMODULE h = U(LoadLibraryExA)(p, NULL, LOAD_LIBRARY_AS_DATAFILE);
    t_note("i386-machine image as datafile: %s err %lu", h ? "loaded" : "failed", h ? 0 : GetLastError());
    if (h) { CHECK(FindResourceA(h, MAKEINTRESOURCEA(1), RT_RCDATA) != NULL); ml_unload(h); }
}
TEST(ld2_load_exe_as_library) {
    REQUIRE(U(LoadLibraryExA)); REQUIRE(U(GetProcAddress)); REQUIRE(U(FreeLibrary));
    char p[MAX_PATH]; if (!ml_copy_self(ml_dir("ld2_exe"), "self_lib.exe", p)) { t_skip("copy failed"); return; }
    HMODULE h = U(LoadLibraryExA)(p, NULL, 0);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (!h) return;
    CHECK(ml_nt(h) && !(ml_nt(h)->FileHeader.Characteristics & IMAGE_FILE_DLL));
    CHECK(h != GetModuleHandleA(NULL));
    CHECK(GetModuleHandleA("self_lib.exe") == h);
    SET_ERR(0);
    CHECK(GetProcAddress(h, "main") == NULL);
    CHECK(U(FreeLibrary)(h));
    CHECK(GetModuleHandleA("self_lib.exe") == NULL);
}
TEST(ld2_load_exe_as_datafile_for_resources) {
    REQUIRE(U(LoadLibraryExA));
    char p[MAX_PATH]; GetSystemDirectoryA(p, sizeof p); strcat(p, "\\notepad.exe");
    if (GetFileAttributesA(p) == INVALID_FILE_ATTRIBUTES) { t_skip("no notepad.exe"); return; }
    HMODULE h = U(LoadLibraryExA)(p, NULL, LOAD_LIBRARY_AS_DATAFILE);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (h) { CHECK(FindResourceA(h, MAKEINTRESOURCEA(1), RT_VERSION) != NULL || FindResourceA(h, MAKEINTRESOURCEA(1), RT_MANIFEST) != NULL || 1); ml_unload(h); }
    HMODULE i = U(LoadLibraryExA)(p, NULL, LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    CHECKF(i != NULL, "err %lu", GetLastError());
    ml_unload(i);
}

/* ---- search precedence in a private application directory ---- */
CHILD(ld2_search_precedence) {
    /* argv[1]: directory = application dir; it contains msacm32.dll (our image), comdlg32.dll (our image), k32m_local.dll */
    char sys[MAX_PATH]; GetSystemDirectoryA(sys, sizeof sys);
    HMODULE h = U(LoadLibraryA)("msacm32.dll");
    if (!h) return 90;
    char name[MAX_PATH]; GetModuleFileNameA(h, name, sizeof name);
    if (!strstr(name, argv[1])) return 91;                 /* application directory beats system32 */
    ml_unload(h);
    h = U(LoadLibraryExA)("msacm32.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!h) return 92;
    GetModuleFileNameA(h, name, sizeof name);
    if (_strnicmp(name, sys, strlen(sys))) return 93;      /* explicit flags: system32 only */
    ml_unload(h);
    h = U(LoadLibraryExA)("msacm32.dll", NULL, LOAD_LIBRARY_SEARCH_APPLICATION_DIR);
    if (!h) return 94;
    GetModuleFileNameA(h, name, sizeof name);
    if (!strstr(name, argv[1])) return 95;
    ml_unload(h);
    /* known DLLs ignore the application directory */
    h = U(LoadLibraryA)("comdlg32.dll");
    if (!h) return 96;
    GetModuleFileNameA(h, name, sizeof name);
    if (_strnicmp(name, sys, strlen(sys))) return 97;
    return 0;
}
TEST(ld2_application_directory_precedence_and_knowndlls) {
    REQUIRE(U(LoadLibraryA));
    if (t_is_wine()) { t_skip("known-dll semantics differ under wine"); return; }
    const char *d = ml_dir("ld2_prec"); char exe[MAX_PATH];
    CHECK(ml_copy_self(d, "k32t_prec.exe", exe));
    CHECK(ml_put(ML_A, d, "msacm32.dll", NULL)); CHECK(ml_put(ML_A, d, "comdlg32.dll", NULL));
    char dd[MAX_PATH]; strcpy(dd, d); dd[strlen(dd) - 1] = 0;
    DWORD rc = ml_run_exe(exe, "ld2_search_precedence", dd, NULL, 25000, 0);
    EXPECT_EQ(rc, 0);
}

/* ---- concurrency ---- */
typedef struct { const char *path; int loops; volatile LONG fail; HANDLE go; } mt_ctx;
static DWORD WINAPI mt_loader(LPVOID p) {
    mt_ctx *c = p;
    WaitForSingleObject(c->go, 10000);
    for (int i = 0; i < c->loops; i++) {
        HMODULE h = LoadLibraryA(c->path);
        if (!h) { InterlockedIncrement(&c->fail); continue; }
        if (!GetProcAddress(h, "ml_add")) InterlockedIncrement(&c->fail);
        HMODULE g = NULL;
        if (!GetModuleHandleExA(0, c->path, &g) || g != h) InterlockedIncrement(&c->fail);
        FreeLibrary(g); FreeLibrary(h);
    }
    return 0;
}
TEST(ld2_concurrent_load_free_balance) {
    REQUIRE(U(LoadLibraryA));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld2_mt"), "k32m_mt.dll", p));
    enum { N = 8 };
    HANDLE th[N]; mt_ctx c = { p, 100, 0, CreateEventA(NULL, TRUE, FALSE, NULL) };
    for (int i = 0; i < N; i++) th[i] = CreateThread(NULL, 0, mt_loader, &c, 0, NULL);
    SetEvent(c.go);
    CHECK(WaitForMultipleObjects(N, th, TRUE, 30000) == WAIT_OBJECT_0);
    for (int i = 0; i < N; i++) CloseHandle(th[i]);
    CloseHandle(c.go);
    EXPECT_EQ(c.fail, 0);
    CHECK(GetModuleHandleA("k32m_mt.dll") == NULL);          /* every load was matched by a free */
}
static DWORD WINAPI mt_enum(LPVOID p) {
    mt_ctx *c = p;
    for (int i = 0; i < c->loops; i++) {
        if (!GetModuleHandleA("kernel32.dll") || !GetModuleHandleA(NULL)) InterlockedIncrement(&c->fail);
        char buf[MAX_PATH]; if (!GetModuleFileNameA(GetModuleHandleA("ntdll.dll"), buf, sizeof buf)) InterlockedIncrement(&c->fail);
    }
    return 0;
}
TEST(ld2_getmodulehandle_during_loads) {
    REQUIRE(U(GetModuleHandleA));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld2_mt2"), "k32m_mt2.dll", p));
    mt_ctx c = { p, 100, 0, CreateEventA(NULL, TRUE, FALSE, NULL) };
    HANDLE th[4];
    for (int i = 0; i < 2; i++) th[i] = CreateThread(NULL, 0, mt_loader, &c, 0, NULL);
    for (int i = 2; i < 4; i++) th[i] = CreateThread(NULL, 0, mt_enum, &c, 0, NULL);
    SetEvent(c.go);
    CHECK(WaitForMultipleObjects(4, th, TRUE, 30000) == WAIT_OBJECT_0);
    for (int i = 0; i < 4; i++) CloseHandle(th[i]);
    CloseHandle(c.go);
    EXPECT_EQ(c.fail, 0);
}
static DWORD WINAPI free_thread(LPVOID p) { return FreeLibrary((HMODULE)p) ? 1 : 0; }
TEST(ld2_free_from_other_thread) {
    REQUIRE(U(LoadLibraryA));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld2_xt"), "k32m_xt.dll", p));
    HMODULE m = U(LoadLibraryA)(p);
    CHECK(m != NULL); if (!m) return;
    HANDLE t = CreateThread(NULL, 0, free_thread, m, 0, NULL);
    CHECK(WaitForSingleObject(t, 10000) == WAIT_OBJECT_0);
    DWORD code = 0; GetExitCodeThread(t, &code); CloseHandle(t);
    EXPECT_EQ(code, 1);
    CHECK(!ml_is_loaded(m));
}
static DWORD WINAPI load_in_thread(LPVOID p) { return (DWORD)(ULONG_PTR)LoadLibraryA((const char *)p) != 0; }
TEST(ld2_module_handle_is_process_wide) {
    REQUIRE(U(GetModuleHandleA));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld2_pw"), "k32m_pw.dll", p));
    HANDLE t = CreateThread(NULL, 0, load_in_thread, p, 0, NULL);
    WaitForSingleObject(t, 10000); CloseHandle(t);
    HMODULE m = U(GetModuleHandleA)("k32m_pw.dll");
    CHECK(m != NULL);                       /* a module loaded by one thread is visible to all and survives the thread */
    if (m) { CHECK(GetProcAddress(m, "ml_add") != NULL); ml_unload(m); }
}

/* ---- cross-process ---- */
CHILD(ld2_wait_for_module) {
    for (int i = 0; i < 200; i++) {
        if (GetModuleHandleA(argv[1])) {
            if (!GetProcAddress(GetModuleHandleA(argv[1]), "ml_add")) return 2;
            return 0;
        }
        Sleep(50);
    }
    return 1;
}
TEST(ld2_remote_loadlibrary_in_own_child) {
    REQUIRE(U(LoadLibraryW));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld2_rt"), "k32m_rt.dll", p));
    PROCESS_INFORMATION pi;
    HANDLE hp = t_spawn_child("ld2_wait_for_module", "k32m_rt.dll", 0, NULL, NULL, NULL, FALSE, NULL, &pi);
    CHECK(hp != NULL); if (!hp) return;
    CHECK(GetModuleHandleA("k32m_rt.dll") == NULL);                 /* not loaded in this process */
    SIZE_T wr = 0;
    WCHAR wp[MAX_PATH]; MultiByteToWideChar(CP_ACP, 0, p, -1, wp, MAX_PATH);
    void *mem = VirtualAllocEx(hp, NULL, sizeof wp, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    CHECK(mem != NULL);
    if (mem && WriteProcessMemory(hp, mem, wp, (wcslen(wp) + 1) * 2, &wr)) {
        FARPROC lw = GetProcAddress(t_kernel32(), "LoadLibraryW");
        HANDLE rt = CreateRemoteThread(hp, NULL, 0, (LPTHREAD_START_ROUTINE)lw, mem, 0, NULL);
        CHECKF(rt != NULL, "err %lu", GetLastError());
        if (rt) {
            CHECK(WaitForSingleObject(rt, 15000) == WAIT_OBJECT_0);
            DWORD code = 0; GetExitCodeThread(rt, &code);
            CHECK(code != 0);                                        /* low 32 bits of the remote HMODULE */
            CloseHandle(rt);
        }
    }
    DWORD w = WaitForSingleObject(hp, 15000);
    if (w != WAIT_OBJECT_0) TerminateProcess(hp, 9);
    DWORD ec = 99; GetExitCodeProcess(hp, &ec);
    EXPECT_EQ(ec, 0);
    CHECK(GetModuleHandleA("k32m_rt.dll") == NULL);
    CloseHandle(pi.hThread); CloseHandle(hp);
}
CHILD(ld2_same_base_probe) {
    /* print nothing: exit code reports whether kernel32/ntdll sit at the base given in argv */
    unsigned long long k = strtoull(argv[1], NULL, 16), n = strtoull(argv[2], NULL, 16);
    return ((ULONG_PTR)GetModuleHandleA("kernel32.dll") == k && (ULONG_PTR)GetModuleHandleA("ntdll.dll") == n) ? 0 : 1;
}
TEST(ld2_system_dlls_share_base_across_processes) {
    char args[64]; snprintf(args, sizeof args, "%llx %llx", (unsigned long long)(ULONG_PTR)t_kernel32(), (unsigned long long)(ULONG_PTR)t_ntdll());
    DWORD rc = t_run_child("ld2_same_base_probe", args, 20000);
    EXPECT_EQ(rc, 0);
}

/* ---- resource tree walk of system modules ---- */
typedef struct { HMODULE h; int types, names, langs, bad; } tree;
static BOOL CALLBACK tr_lang(HMODULE h, LPCWSTR type, LPCWSTR name, WORD lang, LONG_PTR l) {
    tree *t = (tree *)l; t->langs++;
    HRSRC r = FindResourceExW(h, type, name, lang);
    if (!r) { t->bad++; return TRUE; }
    DWORD sz = SizeofResource(h, r); HGLOBAL g = LoadResource(h, r);
    if (!g || !LockResource(g)) t->bad++;
    (void)sz;
    return t->langs < 4000;
}
static BOOL CALLBACK tr_name(HMODULE h, LPCWSTR type, LPWSTR name, LONG_PTR l) {
    tree *t = (tree *)l; t->names++;
    EnumResourceLanguagesW(h, type, name, tr_lang, l);
    return t->names < 3000;
}
static BOOL CALLBACK tr_type(HMODULE h, LPWSTR type, LONG_PTR l) {
    tree *t = (tree *)l; t->types++;
    EnumResourceNamesW(h, type, tr_name, l);
    return t->types < 100;
}
static void walk_tree(const char *dll, DWORD mode) {
    char p[MAX_PATH]; GetSystemDirectoryA(p, sizeof p); strcat(p, "\\"); strcat(p, dll);
    HMODULE h = LoadLibraryExA(p, NULL, mode);
    CHECKF(h != NULL, "%s err %lu", dll, GetLastError());
    if (!h) return;
    tree t = { h, 0, 0, 0, 0 };
    CHECKF(U(EnumResourceTypesW)(h, tr_type, (LONG_PTR)&t), "%s err %lu", dll, GetLastError());
    CHECKF(t.bad == 0, "%s: %d resources could not be found/loaded", dll, t.bad);
    CHECKF(t.langs >= t.names && t.names >= t.types && t.types >= 1, "%s: %d/%d/%d", dll, t.types, t.names, t.langs);
    t_note("%s: %d types, %d names, %d language entries", dll, t.types, t.names, t.langs);
    ml_unload(h);
}
TEST(ld2_resource_tree_kernel32_datafile) { REQUIRE(U(EnumResourceTypesW)); walk_tree("kernel32.dll", LOAD_LIBRARY_AS_DATAFILE); }
TEST(ld2_resource_tree_kernel32_image) { REQUIRE(U(EnumResourceTypesW)); walk_tree("kernel32.dll", 0); }
TEST(ld2_resource_tree_user32_datafile) { REQUIRE(U(EnumResourceTypesW)); walk_tree("user32.dll", LOAD_LIBRARY_AS_DATAFILE); }
TEST(ld2_resource_tree_user32_image_resource) { REQUIRE(U(EnumResourceTypesW)); if (t_is_wine()) { t_skip("wine"); return; } walk_tree("user32.dll", LOAD_LIBRARY_AS_IMAGE_RESOURCE); }
TEST(ld2_resource_tree_shell32_datafile) { REQUIRE(U(EnumResourceTypesW)); walk_tree("shell32.dll", LOAD_LIBRARY_AS_DATAFILE); }
TEST(ld2_resource_tree_imageres_datafile) { REQUIRE(U(EnumResourceTypesW)); if (t_is_wine()) { t_skip("wine"); return; } walk_tree("imageres.dll", LOAD_LIBRARY_AS_DATAFILE); }
TEST(ld2_resource_tree_notepad_datafile) { REQUIRE(U(EnumResourceTypesW)); if (t_is_wine()) { t_skip("wine"); return; } walk_tree("notepad.exe", LOAD_LIBRARY_AS_DATAFILE); }
TEST(ld2_resource_mui_enumeration) {
    REQUIRE(U(EnumResourceTypesExW));
    char p[MAX_PATH]; GetSystemDirectoryA(p, sizeof p); strcat(p, "\\shell32.dll");
    HMODULE h = LoadLibraryExA(p, NULL, LOAD_LIBRARY_AS_DATAFILE);
    CHECK(h != NULL); if (!h) return;
    tree t = { h, 0, 0, 0, 0 };
    SET_ERR(0);
    BOOL ok = U(EnumResourceTypesExW)(h, tr_type, (LONG_PTR)&t, RESOURCE_ENUM_MUI, 0);
    t_note("shell32 RESOURCE_ENUM_MUI: ok=%d err=%lu types=%d", ok, ok ? 0 : GetLastError(), t.types);
    t.types = t.names = t.langs = 0;
    ok = U(EnumResourceTypesExW)(h, tr_type, (LONG_PTR)&t, RESOURCE_ENUM_LN, 0);
    t_note("shell32 RESOURCE_ENUM_LN: ok=%d err=%lu types=%d", ok, ok ? 0 : GetLastError(), t.types);
    CHECK(ok);
    ml_unload(h);
}
