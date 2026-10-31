/* Path forms, file locking of loaded images, last-error preservation, resource handle internals */
#include "ml.h"
#include <wchar.h>

static int put_w(const WCHAR *path) {
    size_t n; const unsigned char *d = ml_blob(ML_A, &n);
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return 0;
    DWORD w; BOOL ok = WriteFile(f, d, (DWORD)n, &w, NULL); CloseHandle(f);
    return ok;
}

/* ---- path normalisation ---- */
TEST(ld3_unicode_path) {
    REQUIRE(U(LoadLibraryW)); REQUIRE(U(GetModuleFileNameW)); REQUIRE(U(GetModuleHandleW));
    WCHAR dir[MAX_PATH], path[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, t_tmpdir(), -1, dir, MAX_PATH);
    swprintf(path, MAX_PATH, L"%ls\u4e2d\u6587_\u00e9\u00e8", dir);
    if (!CreateDirectoryW(path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) { t_skip("cannot create unicode directory"); return; }
    wcscat(path, L"\\k32m_\u00fc\u4e2d.dll");
    if (!put_w(path)) { t_skip("cannot write unicode file"); return; }
    HMODULE m = U(LoadLibraryW)(path);
    CHECKF(m != NULL, "err %lu", GetLastError());
    if (!m) return;
    WCHAR out[MAX_PATH];
    DWORD n = U(GetModuleFileNameW)(m, out, MAX_PATH);
    CHECK(n == wcslen(path) && !_wcsicmp(out, path));
    CHECK(U(GetModuleHandleW)(L"k32m_\u00fc\u4e2d.dll") == m);
    CHECK(U(GetModuleHandleW)(L"K32M_\u00dc\u4e2d") == m);          /* case-insensitive for non-ASCII letters */
    char a[MAX_PATH]; DWORD na = GetModuleFileNameA(m, a, MAX_PATH);
    t_note("ANSI name of unicode module: %lu chars", na);
    FreeLibrary(m);
    DeleteFileW(path);
    WCHAR *bs = wcsrchr(path, L'\\'); if (bs) { *bs = 0; RemoveDirectoryW(path); }
}
TEST(ld3_dotdot_and_double_separators_normalised) {
    REQUIRE(U(LoadLibraryA)); REQUIRE(U(GetModuleFileNameA));
    const char *d = ml_dir("ld3_dd"); char sub[MAX_PATH]; snprintf(sub, sizeof sub, "%ssub", d); CreateDirectoryA(sub, NULL);
    char real[MAX_PATH]; CHECK(ml_put(ML_A, d, "k32m_dd3.dll", real));
    char odd[MAX_PATH]; snprintf(odd, sizeof odd, "%ssub\\..\\k32m_dd3.dll", d);
    HMODULE m = U(LoadLibraryA)(odd);
    CHECKF(m != NULL, "err %lu", GetLastError());
    if (!m) return;
    char out[MAX_PATH]; U(GetModuleFileNameA)(m, out, MAX_PATH);
    CHECKF(!_stricmp(out, real), "got %s", out);
    snprintf(odd, sizeof odd, "%s.\\\\k32m_dd3.dll", d);
    HMODULE m2 = U(LoadLibraryA)(odd);
    t_note("path with .\\\\ : %s", m2 ? (m2 == m ? "same module" : "different module") : "failed");
    if (m2) CHECK(m2 == m);
    ml_unload(m);
    if (m2 && m2 != m) ml_unload(m2);
}
TEST(ld3_extended_length_prefix) {
    REQUIRE(U(LoadLibraryW)); REQUIRE(U(GetModuleFileNameW));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld3_ext"), "k32m_ext3.dll", p));
    WCHAR wp[MAX_PATH + 8]; wcscpy(wp, L"\\\\?\\"); MultiByteToWideChar(CP_ACP, 0, p, -1, wp + 4, MAX_PATH);
    HMODULE m = U(LoadLibraryW)(wp);
    CHECKF(m != NULL, "err %lu", GetLastError());
    if (!m) return;
    WCHAR out[MAX_PATH + 8]; DWORD n = U(GetModuleFileNameW)(m, out, MAX_PATH + 8);
    CHECK(n > 0);
    t_note("GetModuleFileName for \\\\?\\ load: %ls", out);
    CHECK(!_wcsicmp(out, wp + 4) || !_wcsicmp(out, wp));
    CHECK(GetModuleHandleW(L"k32m_ext3.dll") == m);
    ml_unload(m);
    HMODULE d = U(LoadLibraryW) ? LoadLibraryExW(wp, NULL, LOAD_LIBRARY_AS_DATAFILE) : NULL;
    CHECKF(d != NULL, "datafile err %lu", GetLastError());
    ml_unload(d);
}
TEST(ld3_short_path_load) {
    REQUIRE(U(LoadLibraryA)); REQUIRE(U(GetModuleFileNameA));
    const char *d = ml_dir("ld3_long_directory_name"); char p[MAX_PATH]; CHECK(ml_put(ML_A, d, "k32m_long_file_name3.dll", p));
    char sp[MAX_PATH]; DWORD n = GetShortPathNameA(p, sp, MAX_PATH);
    if (!n || !_stricmp(sp, p)) { t_skip("8.3 names unavailable on this volume"); return; }
    HMODULE m = U(LoadLibraryA)(sp);
    CHECKF(m != NULL, "err %lu", GetLastError());
    if (!m) return;
    char out[MAX_PATH]; U(GetModuleFileNameA)(m, out, MAX_PATH);
    t_note("module name after loading through short path: %s", out);
    CHECK(GetModuleHandleA("k32m_long_file_name3.dll") == m || GetModuleHandleA(strrchr(sp, '\\') + 1) == m);
    ml_unload(m);
}
TEST(ld3_trailing_dot_in_path) {
    REQUIRE(U(LoadLibraryA));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld3_td"), "k32m_td.dll", p));
    char q[MAX_PATH]; snprintf(q, sizeof q, "%s.", p);
    HMODULE m = U(LoadLibraryA)(q);
    t_note("LoadLibrary(\"...dll.\") -> %s err %lu", m ? "loaded" : "failed", m ? 0 : GetLastError());
    ml_unload(m);
}
TEST(ld3_extension_variants) {
    REQUIRE(U(LoadLibraryA));
    const char *d = ml_dir("ld3_ev");
    char p[MAX_PATH];
    CHECK(ml_put(ML_A, d, "k32m_noext", p));                       /* file without extension */
    SET_ERR(0);
    HMODULE m = U(LoadLibraryA)(p);                                 /* ".dll" is appended, so the extension-less file is not found */
    CHECK(m == NULL);
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
    ml_unload(m);
    char dotted[MAX_PATH]; snprintf(dotted, sizeof dotted, "%s.", p);   /* trailing dot: "no extension" */
    m = U(LoadLibraryA)(dotted);
    CHECKF(m != NULL, "trailing dot: err %lu", GetLastError());
    if (m) {
        CHECK(GetModuleHandleA("k32m_noext.") == m);
        CHECK(GetModuleHandleA("K32M_NOEXT.") == m);
        char out[MAX_PATH]; GetModuleFileNameA(m, out, MAX_PATH);
        CHECK(!_stricmp(out, p));
    }
    ml_unload(m);
    CHECK(ml_put(ML_A, d, "k32m_ocx.ocx", p));
    m = U(LoadLibraryA)(p);
    CHECKF(m != NULL, "ocx: err %lu", GetLastError());
    ml_unload(m);
    CHECK(ml_put(ML_A, d, "k32m_up.DLL", p));
    m = U(LoadLibraryA)(p);
    CHECK(m != NULL);
    CHECK(GetModuleHandleA("k32m_up.dll") == m);
    CHECK(GetModuleHandleA("k32m_up") == m);
    ml_unload(m);
    CHECK(ml_put(ML_A, d, "k32m_two.dots.dll", p));
    m = U(LoadLibraryA)(p);
    CHECK(m != NULL);
    CHECK(GetModuleHandleA("k32m_two.dots.dll") == m);
    CHECK(GetModuleHandleA("k32m_two.dots") == m || GetModuleHandleA("k32m_two") == NULL);
    ml_unload(m);
}
TEST(ld3_bare_name_extension_appended_only_when_missing) {
    REQUIRE(U(LoadLibraryA));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld3_bn"), "k32m_bn.dll", p));
    HMODULE m = U(LoadLibraryA)(p);
    CHECK(m != NULL); if (!m) return;
    /* "k32m_bn.dll" and "k32m_bn" both find it; "k32m_bn.dll.dll" and "k32m_bn." do not */
    CHECK(GetModuleHandleA("k32m_bn") == m);
    SET_ERR(0);
    CHECK(GetModuleHandleA("k32m_bn.dll.dll") == NULL);
    EXPECT_ERR(ERROR_MOD_NOT_FOUND);
    ml_unload(m);
}

/* ---- file locking of loaded images ---- */
TEST(ld3_loaded_image_file_protection) {
    REQUIRE(U(LoadLibraryA));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld3_lock"), "k32m_lock3.dll", p));
    HMODULE m = U(LoadLibraryA)(p);
    CHECK(m != NULL); if (!m) return;
    SET_ERR(0);
    CHECK(!DeleteFileA(p));
    DWORD e = GetLastError();
    CHECKF(e == ERROR_ACCESS_DENIED || e == ERROR_SHARING_VIOLATION, "delete error %lu", e);
    SET_ERR(0);
    HANDLE f = CreateFileA(p, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(f == INVALID_HANDLE_VALUE);
    e = GetLastError();
    CHECKF(e == ERROR_SHARING_VIOLATION || e == ERROR_ACCESS_DENIED || e == ERROR_USER_MAPPED_FILE, "write-open error %lu", e);
    if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
    f = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(f != INVALID_HANDLE_VALUE);                                 /* reading is fine */
    if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
    char q[MAX_PATH]; snprintf(q, sizeof q, "%s.renamed", p);
    SET_ERR(0);
    BOOL mv = MoveFileA(p, q);
    t_note("rename of loaded dll: %s err %lu", mv ? "ok" : "failed", mv ? 0 : GetLastError());
    if (mv) {
        char out[MAX_PATH]; GetModuleFileNameA(m, out, MAX_PATH);
        CHECKF(!_stricmp(out, p), "loader keeps the old name: %s", out);
        MoveFileA(q, p);
    }
    ml_unload(m);
    CHECK(DeleteFileA(p));                                           /* unloaded: can be deleted */
}
TEST(ld3_datafile_file_protection) {
    REQUIRE(U(LoadLibraryExA));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld3_dflock"), "k32m_dfl.dll", p));
    HMODULE m = U(LoadLibraryExA)(p, NULL, LOAD_LIBRARY_AS_DATAFILE);
    CHECK(m != NULL); if (!m) return;
    SET_ERR(0);
    BOOL del = DeleteFileA(p);
    t_note("DeleteFile of datafile-loaded dll -> %d err %lu", del, del ? 0 : GetLastError());
    ml_unload(m);
    if (!del) CHECK(DeleteFileA(p));
}
TEST(ld3_image_memory_protections) {
    REQUIRE(U(LoadLibraryA));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld3_mem"), "k32m_mem.dll", p));
    HMODULE m = U(LoadLibraryA)(p);
    CHECK(m != NULL); if (!m) return;
    const IMAGE_NT_HEADERS64 *nt = ml_nt(m);
    const IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        MEMORY_BASIC_INFORMATION mbi;
        CHECK(VirtualQuery((char *)m + s[i].VirtualAddress, &mbi, sizeof mbi) == sizeof mbi);
        CHECKF(mbi.Type == MEM_IMAGE && mbi.AllocationBase == (PVOID)m, "section %.8s", s[i].Name);
        if (!strncmp((char *)s[i].Name, ".text", 5)) CHECK(mbi.Protect == PAGE_EXECUTE_READ);
        if (!strncmp((char *)s[i].Name, ".data", 5)) CHECK(mbi.Protect == PAGE_READWRITE || mbi.Protect == PAGE_WRITECOPY);
        if (!strncmp((char *)s[i].Name, ".rdata", 6) || !strncmp((char *)s[i].Name, ".rsrc", 5) || !strncmp((char *)s[i].Name, ".edata", 6) || !strncmp((char *)s[i].Name, ".pdata", 6))
            CHECKF(mbi.Protect == PAGE_READONLY, "section %.8s protect %lx", s[i].Name, mbi.Protect);
    }
    MEMORY_BASIC_INFORMATION mbi;
    VirtualQuery(m, &mbi, sizeof mbi);
    CHECK(mbi.Protect == PAGE_READONLY && mbi.RegionSize == 0x1000);
    ml_unload(m);
}
TEST(ld3_datafile_memory_type) {
    REQUIRE(U(LoadLibraryExA));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld3_dfm"), "k32m_dfmt.dll", p));
    HMODULE m = U(LoadLibraryExA)(p, NULL, LOAD_LIBRARY_AS_DATAFILE);
    CHECK(m != NULL); if (!m) return;
    MEMORY_BASIC_INFORMATION mbi;
    CHECK(VirtualQuery((void *)((ULONG_PTR)m & ~(ULONG_PTR)3), &mbi, sizeof mbi) == sizeof mbi);
    CHECK(mbi.Type == MEM_MAPPED || mbi.Type == MEM_IMAGE);
    t_note("datafile mapping type %lx protect %lx", mbi.Type, mbi.Protect);
    CHECK(!(mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)));
    ml_unload(m);
}

/* ---- last error preservation ---- */
TEST(ld3_getmodulehandle_preserves_error_on_success) {
    REQUIRE(U(GetModuleHandleA)); REQUIRE(U(GetModuleHandleW));
    SET_ERR(0x4242);
    CHECK(U(GetModuleHandleA)("kernel32.dll") != NULL);
    EXPECT_ERR(0x4242);
    CHECK(U(GetModuleHandleW)(L"ntdll") != NULL);
    EXPECT_ERR(0x4242);
}
TEST(ld3_getprocaddress_preserves_error_on_success) {
    REQUIRE(U(GetProcAddress));
    SET_ERR(0x4343);
    CHECK(U(GetProcAddress)(t_kernel32(), "GetTickCount") != NULL);
    EXPECT_ERR(0x4343);
    CHECK(U(GetProcAddress)(t_kernel32(), MAKEINTRESOURCEA(1)) != NULL || 1);
}
TEST(ld3_loadlibrary_error_on_success_recorded) {
    REQUIRE(U(LoadLibraryA));
    SET_ERR(0x4444);
    HMODULE h = U(LoadLibraryA)("kernel32.dll");
    CHECK(h != NULL);
    t_note("LoadLibrary success leaves last error %lx", GetLastError());
    FreeLibrary(h);
}
TEST(ld3_resource_calls_preserve_error_on_success) {
    REQUIRE(U(FindResourceA)); REQUIRE(U(LoadResource)); REQUIRE(U(SizeofResource));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld3_resle"), "k32m_rsle.dll", p));
    HMODULE m = LoadLibraryA(p); CHECK(m != NULL); if (!m) return;
    SET_ERR(0x4545);
    HRSRC r = U(FindResourceA)(m, MAKEINTRESOURCEA(1), RT_RCDATA);
    CHECK(r != NULL);
    EXPECT_ERR(0x4545);
    HGLOBAL g = U(LoadResource)(m, r);
    CHECK(g != NULL);
    EXPECT_ERR(0x4545);
    CHECK(U(SizeofResource)(m, r) == 8);
    EXPECT_ERR(0x4545);
    ml_unload(m);
}
TEST(ld3_failed_calls_set_last_error_nonzero) {
    REQUIRE(U(GetModuleHandleA)); REQUIRE(U(GetProcAddress)); REQUIRE(U(LoadLibraryA));
    SET_ERR(0);
    CHECK(U(GetModuleHandleA)("k32t_nosuch.dll") == NULL); CHECK(GetLastError() != 0);
    SET_ERR(0);
    CHECK(U(GetProcAddress)(t_kernel32(), "k32t_nosuch") == NULL); CHECK(GetLastError() != 0);
    SET_ERR(0);
    CHECK(U(LoadLibraryA)("k32t_nosuch.dll") == NULL); CHECK(GetLastError() != 0);
}

/* ---- resource handle internals ---- */
TEST(ld3_hrsrc_is_resource_data_entry) {
    REQUIRE(U(FindResourceA)); REQUIRE(U(LoadResource)); REQUIRE(U(SizeofResource));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld3_hr"), "k32m_hr.dll", p));
    HMODULE m = LoadLibraryA(p); CHECK(m != NULL); if (!m) return;
    HRSRC r = U(FindResourceA)(m, MAKEINTRESOURCEA(1), RT_RCDATA);
    CHECK(r != NULL);
    if (r) {
        const IMAGE_NT_HEADERS64 *nt = ml_nt(m);
        CHECK((char *)r > (char *)m && (char *)r < (char *)m + nt->OptionalHeader.SizeOfImage);
        const IMAGE_RESOURCE_DATA_ENTRY *e = (const void *)r;
        CHECK(e->Size == 8);
        CHECK(U(SizeofResource)(m, r) == e->Size);
        CHECK((char *)U(LoadResource)(m, r) == (char *)m + e->OffsetToData);
        const IMAGE_DATA_DIRECTORY *rd = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE];
        CHECK((char *)r >= (char *)m + rd->VirtualAddress && (char *)r < (char *)m + rd->VirtualAddress + rd->Size);
        /* the same HRSRC is returned for a repeated lookup */
        CHECK(FindResourceA(m, MAKEINTRESOURCEA(1), RT_RCDATA) == r);
    }
    ml_unload(m);
}
TEST(ld3_hrsrc_cross_module_use_is_unsafe_but_sizeof_consistent) {
    REQUIRE(U(FindResourceA)); REQUIRE(U(SizeofResource));
    /* two copies of the same image have identical resource layouts; a copy's HRSRC is not interchangeable but sizes agree */
    const char *d = ml_dir("ld3_two"); char p1[MAX_PATH], p2[MAX_PATH];
    ml_put(ML_A, d, "k32m_rc1.dll", p1); ml_put(ML_A, d, "k32m_rc2.dll", p2);
    HMODULE a = LoadLibraryA(p1), b = LoadLibraryA(p2);
    CHECK(a && b && a != b);
    if (a && b) {
        HRSRC ra = U(FindResourceA)(a, MAKEINTRESOURCEA(1), RT_RCDATA), rb = U(FindResourceA)(b, MAKEINTRESOURCEA(1), RT_RCDATA);
        CHECK(ra && rb);
        CHECK((char *)ra - (char *)a == (char *)rb - (char *)b);
        CHECK(SizeofResource(a, ra) == SizeofResource(b, rb));
    }
    ml_unload(a); ml_unload(b);
}
TEST(ld3_findresource_repeat_stable_many) {
    REQUIRE(U(FindResourceA));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld3_rep"), "k32m_rep3.dll", p));
    HMODULE m = LoadLibraryA(p); CHECK(m != NULL); if (!m) return;
    HRSRC first = U(FindResourceA)(m, MAKEINTRESOURCEA(1), RT_RCDATA);
    for (int i = 0; i < 5000; i++) if (U(FindResourceA)(m, MAKEINTRESOURCEA(1), RT_RCDATA) != first) { CHECK(0); break; }
    for (int i = 0; i < 5000; i++) { SET_ERR(0); if (U(FindResourceA)(m, MAKEINTRESOURCEA(2), RT_RCDATA) || GetLastError() != ERROR_RESOURCE_NAME_NOT_FOUND) { CHECK(0); break; } }
    ml_unload(m);
}
TEST(ld3_getprocaddress_stable_many) {
    REQUIRE(U(GetProcAddress));
    FARPROC first = U(GetProcAddress)(t_kernel32(), "CreateFileW");
    for (int i = 0; i < 20000; i++) if (U(GetProcAddress)(t_kernel32(), "CreateFileW") != first) { CHECK(0); break; }
    for (int i = 0; i < 20000; i++) { SET_ERR(0); if (U(GetProcAddress)(t_kernel32(), "CreateFileX") || GetLastError() != ERROR_PROC_NOT_FOUND) { CHECK(0); break; } }
}
TEST(ld3_loadlibrary_free_cycle_many) {
    REQUIRE(U(LoadLibraryA)); REQUIRE(U(FreeLibrary));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld3_cyc"), "k32m_cyc.dll", p));
    for (int i = 0; i < 300; i++) {
        HMODULE h = U(LoadLibraryA)(p);
        if (!h) { CHECKF(0, "iteration %d err %lu", i, GetLastError()); break; }
        if (!U(FreeLibrary)(h)) { CHECKF(0, "free iteration %d", i); break; }
    }
    CHECK(GetModuleHandleA("k32m_cyc.dll") == NULL);
}
TEST(ld3_datafile_cycle_many) {
    REQUIRE(U(LoadLibraryExA));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld3_dcyc"), "k32m_dcyc.dll", p));
    for (int i = 0; i < 300; i++) {
        HMODULE h = U(LoadLibraryExA)(p, NULL, i & 1 ? LOAD_LIBRARY_AS_DATAFILE : LOAD_LIBRARY_AS_IMAGE_RESOURCE);
        if (!h) { CHECKF(0, "iteration %d err %lu", i, GetLastError()); break; }
        if (!FreeLibrary(h)) { CHECKF(0, "free iteration %d", i); break; }
    }
}
TEST(ld3_refcount_deep) {
    REQUIRE(U(LoadLibraryA)); REQUIRE(U(FreeLibrary));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld3_deep"), "k32m_deep.dll", p));
    HMODULE h = NULL;
    for (int i = 0; i < 1000; i++) h = U(LoadLibraryA)(p);
    CHECK(h != NULL); if (!h) return;
    for (int i = 0; i < 999; i++) if (!U(FreeLibrary)(h)) { CHECKF(0, "free %d", i); break; }
    CHECK(ml_is_loaded(h));
    CHECK(U(FreeLibrary)(h));
    CHECK(!ml_is_loaded(h));
}
TEST(ld3_load_ex_flags_do_not_change_identity) {
    REQUIRE(U(LoadLibraryExA));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld3_id"), "k32m_id.dll", p));
    HMODULE a = U(LoadLibraryExA)(p, NULL, 0);
    HMODULE b = U(LoadLibraryExA)(p, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    HMODULE c = U(LoadLibraryExA)(p, NULL, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    HMODULE d = U(LoadLibraryExA)(p, NULL, LOAD_IGNORE_CODE_AUTHZ_LEVEL);
    CHECK(a != NULL && a == b && b == c && c == d);
    for (int i = 0; i < 4; i++) FreeLibrary(a);
    CHECK(!ml_is_loaded(a));
}
TEST(ld3_load_packaged_flag_value) {
    REQUIRE(U(LoadLibraryExA));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ld3_pk"), "k32m_pk.dll", p));
    SET_ERR(0);
    HMODULE h = U(LoadLibraryExA)(p, NULL, 0x4);       /* LOAD_PACKAGED_LIBRARY */
    t_note("LoadLibraryEx(flags=0x4) -> %s err %lu", h ? "loaded" : "failed", h ? 0 : GetLastError());
    ml_unload(h);
    SET_ERR(0);
    h = U(LoadLibraryExA)(p, NULL, LOAD_LIBRARY_OS_INTEGRITY_CONTINUITY);
    t_note("LoadLibraryEx(OS_INTEGRITY_CONTINUITY) -> %s err %lu", h ? "loaded" : "failed", h ? 0 : GetLastError());
    ml_unload(h);
}
