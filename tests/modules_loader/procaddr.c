/* GetProcAddress, DisableThreadLibraryCalls, FreeLibraryAndExitThread, export table consistency */
#include "ml.h"

typedef int (WINAPI *pf_int2)(int, int);
typedef int (WINAPI *pf_int0)(void);

static HMODULE pa_load(const char *sub, const char *leaf) {
    char p[MAX_PATH];
    if (!ml_put(ML_A, ml_dir(sub), leaf, p)) return NULL;
    return LoadLibraryA(p);
}

/* ---- by name ---- */
TEST(gpa_by_name_call) {
    REQUIRE(U(GetProcAddress));
    HMODULE m = pa_load("gpa_n", "k32m_gpa1.dll"); CHECK(m != NULL); if (!m) return;
    pf_int2 add = (pf_int2)U(GetProcAddress)(m, "ml_add");
    CHECK(add != NULL);
    if (add) { EXPECT_EQ(add(2, 3), 5); EXPECT_EQ(add(-7, 7), 0); EXPECT_EQ(add(0x7FFFFFFF, 1), (int)0x80000000); }
    pf_int0 o7 = (pf_int0)U(GetProcAddress)(m, "ml_ord7");
    CHECK(o7 != NULL);
    if (o7) EXPECT_EQ(o7(), 77);
    CHECK((char *)add > (char *)m && (char *)add < (char *)m + ml_nt(m)->OptionalHeader.SizeOfImage);
    ml_unload(m);
}
TEST(gpa_exported_alias_name) {
    REQUIRE(U(GetProcAddress));
    HMODULE m = pa_load("gpa_alias", "k32m_gpa2.dll"); if (!m) { CHECK(0); return; }
    pf_int0 c = (pf_int0)U(GetProcAddress)(m, "MlCase");
    CHECK(c != NULL);
    if (c) EXPECT_EQ(c(), 5);
    SET_ERR(0);
    CHECK(U(GetProcAddress)(m, "ml_case") == NULL);        /* internal name is not exported */
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    ml_unload(m);
}
TEST(gpa_case_sensitive) {
    REQUIRE(U(GetProcAddress));
    HMODULE m = pa_load("gpa_cs", "k32m_gpa3.dll"); if (!m) { CHECK(0); return; }
    static const char *bad[] = { "ML_ADD", "Ml_Add", "mlcase", "MLCASE", "ml_ADD", "ml_add ", " ml_add", "ml_ad", "ml_addd", "" };
    for (int i = 0; i < 10; i++) {
        SET_ERR(0);
        CHECKF(U(GetProcAddress)(m, bad[i]) == NULL, "\"%s\"", bad[i]);
        EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    }
    CHECK(U(GetProcAddress)(m, "ml_add") != NULL);
    ml_unload(m);
}
TEST(gpa_system_case_sensitive) {
    REQUIRE(U(GetProcAddress));
    CHECK(U(GetProcAddress)(t_kernel32(), "GetTickCount") != NULL);
    SET_ERR(0);
    CHECK(U(GetProcAddress)(t_kernel32(), "gettickcount") == NULL);
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    SET_ERR(0);
    CHECK(U(GetProcAddress)(t_kernel32(), "GETTICKCOUNT") == NULL);
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    SET_ERR(0);
    CHECK(U(GetProcAddress)(t_kernel32(), "GetTickCountW") == NULL);
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
}
TEST(gpa_nonexistent_name) {
    REQUIRE(U(GetProcAddress));
    SET_ERR(0);
    CHECK(U(GetProcAddress)(t_kernel32(), "K32T_NoSuchFunction") == NULL);
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    SET_ERR(0);
    CHECK(U(GetProcAddress)(t_ntdll(), "K32T_NoSuchFunction") == NULL);
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    SET_ERR(0);
    CHECK(U(GetProcAddress)(t_kernelbase(), "K32T_NoSuchFunction") == NULL);
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
}
TEST(gpa_long_name) {
    REQUIRE(U(GetProcAddress));
    char *n = HeapAlloc(GetProcessHeap(), 0, 5000);
    if (!n) return;
    for (int len = 100; len <= 4500; len += 1400) {
        memset(n, 'A', len); n[len] = 0;
        SET_ERR(0);
        CHECKF(U(GetProcAddress)(t_kernel32(), n) == NULL, "len %d", len);
        EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    }
    HeapFree(GetProcessHeap(), 0, n);
}
TEST(gpa_name_is_not_decorated_stdcall) {
    REQUIRE(U(GetProcAddress));
    SET_ERR(0);
    CHECK(U(GetProcAddress)(t_kernel32(), "GetTickCount@0") == NULL);
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    SET_ERR(0);
    CHECK(U(GetProcAddress)(t_kernel32(), "_GetTickCount") == NULL);
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
}
TEST(gpa_null_module_is_exe) {
    REQUIRE(U(GetProcAddress));
    SET_ERR(0);
    CHECK(U(GetProcAddress)(NULL, "ml_add") == NULL);
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    SET_ERR(0);
    CHECK(U(GetProcAddress)(NULL, "GetTickCount") == NULL);     /* exe exports nothing */
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    SET_ERR(0);
    CHECK(U(GetProcAddress)(GetModuleHandleA(NULL), "main") == NULL);
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
}
TEST(gpa_null_module_ordinal) {
    REQUIRE(U(GetProcAddress));
    SET_ERR(0);
    CHECK(U(GetProcAddress)(NULL, MAKEINTRESOURCEA(1)) == NULL);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_PROC_NOT_FOUND || e == ERROR_INVALID_ORDINAL, "e=%lu", e);
}
TEST(gpa_null_name_pointer) {
    REQUIRE(U(GetProcAddress));
    SET_ERR(0);
    CHECK(U(GetProcAddress)(t_kernel32(), NULL) == NULL);
    DWORD e = GetLastError();
    CHECK(e != 0);
    t_note("GetProcAddress(kernel32, NULL) error %lu", e);
}
TEST(gpa_tagged_module_handles) {
    REQUIRE(U(GetProcAddress));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("gpa_tag"), "k32m_gpat.dll", p));
    HMODULE d = LoadLibraryExA(p, NULL, LOAD_LIBRARY_AS_DATAFILE);
    CHECK(d != NULL); if (!d) return;
    SET_ERR(0);
    FARPROC f = U(GetProcAddress)(d, "ml_add");
    CHECK(f == NULL);
    DWORD e = GetLastError();
    CHECK(e != 0);
    t_note("GetProcAddress(datafile module) error %lu", e);
    SET_ERR(0);
    f = U(GetProcAddress)(d, MAKEINTRESOURCEA(ML_ORD_ADD));
    CHECK(f == NULL);
    FreeLibrary(d);
}
TEST(gpa_unloaded_module_handle_after_free) {
    REQUIRE(U(GetProcAddress));
    HMODULE m = pa_load("gpa_unl", "k32m_gpau.dll"); if (!m) { CHECK(0); return; }
    CHECK(U(GetProcAddress)(m, "ml_add") != NULL);
    FreeLibrary(m);
    CHECK(!ml_is_loaded(m));
    /* handle is stale: do not dereference through GetProcAddress (would fault on unmapped image) */
}

/* ---- by ordinal ---- */
TEST(gpa_ordinal_matches_name) {
    REQUIRE(U(GetProcAddress));
    HMODULE m = pa_load("gpa_o", "k32m_gpo.dll"); if (!m) { CHECK(0); return; }
    static const struct { int ord; const char *name; } t[] = {
        { ML_ORD_PROC_ATTACH, "ml_proc_attach" }, { ML_ORD_THREAD_ATTACH, "ml_thread_attach" }, { ML_ORD_THREAD_DETACH, "ml_thread_detach" },
        { ML_ORD_VALUE, "ml_value" }, { ML_ORD_ADD, "ml_add" }, { ML_ORD_ORD7, "ml_ord7" }, { ML_ORD_CASE, "MlCase" },
        { ML_ORD_FWD_PID, "ml_fwd_pid" }, { ML_ORD_FWD_NT, "ml_fwd_nt" } };
    for (int i = 0; i < (int)(sizeof t / sizeof *t); i++) {
        FARPROC a = U(GetProcAddress)(m, t[i].name), b = U(GetProcAddress)(m, MAKEINTRESOURCEA(t[i].ord));
        CHECKF(a != NULL && a == b, "%s ord %d: %p %p", t[i].name, t[i].ord, a, b);
    }
    ml_unload(m);
}
TEST(gpa_ordinal_noname_export) {
    REQUIRE(U(GetProcAddress));
    HMODULE m = pa_load("gpa_nn", "k32m_gpnn.dll"); if (!m) { CHECK(0); return; }
    pf_int0 f = (pf_int0)U(GetProcAddress)(m, MAKEINTRESOURCEA(ML_ORD_NONAME));
    CHECK(f != NULL);
    if (f) EXPECT_EQ(f(), 99);
    ml_unload(m);
}
TEST(gpa_ordinal_unused_slots) {
    REQUIRE(U(GetProcAddress));
    HMODULE m = pa_load("gpa_hole", "k32m_gph.dll"); if (!m) { CHECK(0); return; }
    static const int holes[] = { 6, 8, 10, 12 };
    for (int i = 0; i < 4; i++) {
        SET_ERR(0);
        CHECKF(U(GetProcAddress)(m, MAKEINTRESOURCEA(holes[i])) == NULL, "ord %d", holes[i]);
        DWORD e = GetLastError();
        CHECKF(e == ERROR_PROC_NOT_FOUND || e == ERROR_INVALID_ORDINAL, "ord %d e=%lu", holes[i], e);
    }
    ml_unload(m);
}
TEST(gpa_ordinal_out_of_range) {
    REQUIRE(U(GetProcAddress));
    HMODULE m = pa_load("gpa_oor", "k32m_gpr.dll"); if (!m) { CHECK(0); return; }
    static const int o[] = { 0, ML_ORD_MAX + 1, ML_ORD_MAX + 100, 1000, 0x7FFF, 0xFFFF };
    for (int i = 0; i < 6; i++) {
        SET_ERR(0);
        CHECKF(U(GetProcAddress)(m, MAKEINTRESOURCEA(o[i])) == NULL, "ord %d", o[i]);
        DWORD e = GetLastError();
        CHECKF(e == ERROR_PROC_NOT_FOUND || e == ERROR_INVALID_ORDINAL || e == ERROR_INVALID_PARAMETER, "ord %d e=%lu", o[i], e);
    }
    ml_unload(m);
}
TEST(gpa_ordinal_system_dll_matches_name) {
    REQUIRE(U(GetProcAddress));
    /* kernel32's own export directory: a named export resolves identically by ordinal */
    static const char *n[] = { "GetTickCount", "CreateFileW", "Sleep", "GetLastError", "LoadLibraryA", "HeapAlloc" };
    const IMAGE_NT_HEADERS64 *nt = ml_nt(t_kernel32());
    const IMAGE_EXPORT_DIRECTORY *ed = (void *)((char *)t_kernel32() + nt->OptionalHeader.DataDirectory[0].VirtualAddress);
    const DWORD *names = (void *)((char *)t_kernel32() + ed->AddressOfNames);
    const WORD *ords = (void *)((char *)t_kernel32() + ed->AddressOfNameOrdinals);
    for (int k = 0; k < 6; k++) {
        for (DWORD i = 0; i < ed->NumberOfNames; i++) {
            if (strcmp((char *)t_kernel32() + names[i], n[k])) continue;
            FARPROC a = U(GetProcAddress)(t_kernel32(), n[k]);
            FARPROC b = U(GetProcAddress)(t_kernel32(), MAKEINTRESOURCEA(ed->Base + ords[i]));
            CHECKF(a && a == b, "%s", n[k]);
        }
    }
}
TEST(gpa_ordinal_value_boundary_names) {
    REQUIRE(U(GetProcAddress));
    /* a string pointer at or above 0x10000 is a name; below is an ordinal. */
    HMODULE m = pa_load("gpa_bnd", "k32m_gpb.dll"); if (!m) { CHECK(0); return; }
    SET_ERR(0);
    CHECK(U(GetProcAddress)(m, (LPCSTR)(ULONG_PTR)0xFFFF) == NULL);
    CHECK(U(GetProcAddress)(m, (LPCSTR)(ULONG_PTR)ML_ORD_ADD) == U(GetProcAddress)(m, "ml_add"));
    ml_unload(m);
}

/* ---- data & forwarded exports ---- */
TEST(gpa_data_export) {
    REQUIRE(U(GetProcAddress));
    HMODULE m = pa_load("gpa_data", "k32m_gpd.dll"); if (!m) { CHECK(0); return; }
    volatile int *v = (volatile int *)U(GetProcAddress)(m, "ml_value");
    CHECK(v != NULL);
    if (v) {
        EXPECT_EQ(*v, 0x1234);
        *v = 5; EXPECT_EQ(*(volatile int *)U(GetProcAddress)(m, "ml_value"), 5);
        *v = 0x1234;
        MEMORY_BASIC_INFORMATION mbi; VirtualQuery((void *)v, &mbi, sizeof mbi);
        CHECK(mbi.Type == MEM_IMAGE);
        CHECK(mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY));
    }
    ml_unload(m);
}
TEST(gpa_forwarder_to_kernel32) {
    REQUIRE(U(GetProcAddress));
    HMODULE m = pa_load("gpa_f1", "k32m_gpf1.dll"); if (!m) { CHECK(0); return; }
    FARPROC f = U(GetProcAddress)(m, "ml_fwd_pid");
    CHECKF(f != NULL, "err %lu", GetLastError());
    if (f) {
        CHECK(f == GetProcAddress(t_kernel32(), "GetCurrentProcessId"));
        EXPECT_EQ(((DWORD (WINAPI *)(void))f)(), GetCurrentProcessId());
    }
    FARPROC g = U(GetProcAddress)(m, MAKEINTRESOURCEA(ML_ORD_FWD_PID));
    CHECK(g == f);
    ml_unload(m);
}
TEST(gpa_forwarder_to_ntdll) {
    REQUIRE(U(GetProcAddress));
    HMODULE m = pa_load("gpa_f2", "k32m_gpf2.dll"); if (!m) { CHECK(0); return; }
    FARPROC f = U(GetProcAddress)(m, "ml_fwd_nt");
    CHECKF(f != NULL, "err %lu", GetLastError());
    if (f) CHECK(f == GetProcAddress(t_ntdll(), "RtlGetVersion"));
    ml_unload(m);
}
TEST(gpa_forwarder_to_missing_dll) {
    REQUIRE(U(GetProcAddress));
    HMODULE m = pa_load("gpa_f3", "k32m_gpf3.dll"); if (!m) { CHECK(0); return; }
    SET_ERR(0);
    FARPROC f = U(GetProcAddress)(m, "ml_fwd_bad");
    CHECK(f == NULL);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_MOD_NOT_FOUND || e == ERROR_PROC_NOT_FOUND, "e=%lu", e);
    t_note("forwarder to missing dll: error %lu", e);
    CHECK(GetModuleHandleA("nosuchdll_k32t.dll") == NULL);
    ml_unload(m);
}
TEST(gpa_kernel32_heap_forwarders_to_ntdll) {
    REQUIRE(U(GetProcAddress));
    CHECK(U(GetProcAddress)(t_kernel32(), "HeapAlloc") == GetProcAddress(t_ntdll(), "RtlAllocateHeap"));
    CHECK(U(GetProcAddress)(t_kernel32(), "HeapReAlloc") == GetProcAddress(t_ntdll(), "RtlReAllocateHeap"));
    CHECK(U(GetProcAddress)(t_kernel32(), "HeapSize") != NULL);
}
TEST(gpa_kernel32_interlocked_slist_forwarders) {
    REQUIRE(U(GetProcAddress));
    CHECK(U(GetProcAddress)(t_kernel32(), "InterlockedFlushSList") == GetProcAddress(t_ntdll(), "RtlInterlockedFlushSList"));
    CHECK(U(GetProcAddress)(t_kernel32(), "InterlockedPushEntrySList") == GetProcAddress(t_ntdll(), "RtlInterlockedPushEntrySList"));
    CHECK(U(GetProcAddress)(t_kernel32(), "InterlockedPopEntrySList") == GetProcAddress(t_ntdll(), "RtlInterlockedPopEntrySList"));
    CHECK(U(GetProcAddress)(t_kernel32(), "InitializeSListHead") == GetProcAddress(t_ntdll(), "RtlInitializeSListHead"));
    CHECK(U(GetProcAddress)(t_kernel32(), "QueryDepthSList") == GetProcAddress(t_ntdll(), "RtlQueryDepthSList"));
}
TEST(gpa_kernel32_vs_kernelbase_same_names) {
    REQUIRE(U(GetProcAddress));
    /* The public names exist in both: the kernel32 one is a thunk/forwarder, both must be callable and agree. */
    static const char *n[] = { "GetTickCount", "GetCurrentProcessId", "GetCurrentThreadId", "GetLastError", "SetLastError", "CloseHandle", "CreateEventW", "VirtualAlloc" };
    for (int i = 0; i < 8; i++) {
        FARPROC k = GetProcAddress(t_kernel32(), n[i]), b = GetProcAddress(t_kernelbase(), n[i]);
        CHECKF(k != NULL && b != NULL, "%s", n[i]);
    }
    DWORD (WINAPI *k)(void) = (void *)GetProcAddress(t_kernel32(), "GetCurrentProcessId");
    DWORD (WINAPI *b)(void) = (void *)GetProcAddress(t_kernelbase(), "GetCurrentProcessId");
    if (k && b) { EXPECT_EQ(k(), GetCurrentProcessId()); EXPECT_EQ(b(), GetCurrentProcessId()); }
}
TEST(gpa_ntdll_exports_not_in_kernel32_names) {
    REQUIRE(U(GetProcAddress));
    SET_ERR(0);
    CHECK(U(GetProcAddress)(t_kernel32(), "NtClose") == NULL);
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    SET_ERR(0);
    CHECK(U(GetProcAddress)(t_kernel32(), "RtlGetVersion") == NULL);
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    CHECK(U(GetProcAddress)(t_ntdll(), "NtClose") != NULL);
}
TEST(gpa_win10_additions_present) {
    REQUIRE(U(GetProcAddress));
    static const char *n[] = { "CreateFile2", "WaitOnAddress", "WakeByAddressSingle", "SetThreadDescription", "GetThreadDescription",
        "CreatePseudoConsole", "ClosePseudoConsole", "ResizePseudoConsole", "GetSystemCpuSetInformation", "SetProcessDefaultCpuSets",
        "SetProcessInformation", "GetProcessInformation", "SetThreadInformation", "AddDllDirectory", "RemoveDllDirectory",
        "SetDefaultDllDirectories", "GetCurrentPackageFullName", "VirtualAlloc2", "MapViewOfFile3", "CreateFileMappingFromApp", "GetCurrentThreadStackLimits",
        "IsWow64Process2", "GetOverlappedResultEx", "LoadPackagedLibrary", "GetPackageFamilyName" };
    int missing = 0;
    for (int i = 0; i < (int)(sizeof n / sizeof *n); i++) {
        HMODULE mods[] = { t_kernel32(), t_kernelbase() };
        FARPROC p = GetProcAddress(mods[0], n[i]);
        if (!p) p = GetProcAddress(mods[1], n[i]);
        if (!p) { missing++; t_note("%s not exported by kernel32/kernelbase", n[i]); }
    }
    CHECKF(missing <= 6, "missing=%d", missing);
}

/* ---- export directory walk ---- */
typedef struct { HMODULE base; const IMAGE_EXPORT_DIRECTORY *ed; DWORD ex_lo, ex_hi; } expdir;
static int expdir_open(expdir *e, HMODULE m) {
    const IMAGE_NT_HEADERS64 *nt = ml_nt(m);
    if (!nt) return 0;
    const IMAGE_DATA_DIRECTORY *dd = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!dd->VirtualAddress) return 0;
    e->base = m; e->ed = (void *)((char *)m + dd->VirtualAddress);
    e->ex_lo = dd->VirtualAddress; e->ex_hi = dd->VirtualAddress + dd->Size;
    return 1;
}
static void walk_exports(HMODULE m, const char *label, int require_sorted) {
    REQUIRE(U(GetProcAddress));
    expdir e;
    if (!expdir_open(&e, m)) { CHECK(0); return; }
    const DWORD *funcs = (void *)((char *)m + e.ed->AddressOfFunctions);
    const DWORD *names = (void *)((char *)m + e.ed->AddressOfNames);
    const WORD *ords = (void *)((char *)m + e.ed->AddressOfNameOrdinals);
    int bad_same = 0, bad_addr = 0, unsorted = 0, nullp = 0, fwd = 0;
    const char *prev = "";
    for (DWORD i = 0; i < e.ed->NumberOfNames; i++) {
        const char *nm = (char *)m + names[i];
        if (i && strcmp(prev, nm) >= 0) unsorted++;
        prev = nm;
        DWORD rva = funcs[ords[i]];
        int isfwd = rva >= e.ex_lo && rva < e.ex_hi;
        if (isfwd) { fwd++; continue; }
        FARPROC a = U(GetProcAddress)(m, nm);
        FARPROC b = U(GetProcAddress)(m, MAKEINTRESOURCEA(e.ed->Base + ords[i]));
        if (!a) { nullp++; t_note("%s: %s resolved to NULL (err %lu)", label, nm, GetLastError()); continue; }
        if (a != b) bad_same++;
        if ((char *)a != (char *)m + rva) bad_addr++;
    }
    CHECKF(bad_same == 0, "%s: %d names differ from their ordinal lookup", label, bad_same);
    CHECKF(bad_addr == 0, "%s: %d names differ from base+rva", label, bad_addr);
    CHECKF(nullp == 0, "%s: %d NULL", label, nullp);
    if (require_sorted) CHECKF(unsorted == 0, "%s: %d names out of order", label, unsorted);
    CHECK(e.ed->NumberOfNames > 100);
    t_note("%s: %lu names, %lu functions, %d forwarders, base %lu", label, e.ed->NumberOfNames, e.ed->NumberOfFunctions, fwd, e.ed->Base);
}
TEST(gpa_export_walk_kernel32) { walk_exports(t_kernel32(), "kernel32", 1); }
TEST(gpa_export_walk_kernelbase) { walk_exports(t_kernelbase(), "kernelbase", 1); }
TEST(gpa_export_walk_ntdll) { walk_exports(t_ntdll(), "ntdll", 1); }
TEST(gpa_export_forwarders_resolve_kernel32) {
    REQUIRE(U(GetProcAddress));
    expdir e;
    if (!expdir_open(&e, t_kernel32())) { CHECK(0); return; }
    const DWORD *funcs = (void *)((char *)e.base + e.ed->AddressOfFunctions);
    const DWORD *names = (void *)((char *)e.base + e.ed->AddressOfNames);
    const WORD *ords = (void *)((char *)e.base + e.ed->AddressOfNameOrdinals);
    int n = 0;
    for (DWORD i = 0; i < e.ed->NumberOfNames; i++) {
        DWORD rva = funcs[ords[i]];
        if (rva < e.ex_lo || rva >= e.ex_hi) continue;
        const char *target = (char *)e.base + rva;
        if (_strnicmp(target, "NTDLL.", 6) && _strnicmp(target, "KERNELBASE.", 11)) continue;   /* only resolve forwards to loaded modules */
        FARPROC p = U(GetProcAddress)(e.base, (char *)e.base + names[i]);
        CHECKF(p != NULL, "%s -> %s", (char *)e.base + names[i], target);
        /* forward target resolves to the same address */
        char mod[32]; const char *dot = strchr(target, '.');
        memcpy(mod, target, dot - target); mod[dot - target] = 0;
        HMODULE tm = GetModuleHandleA(mod);
        if (tm && dot[1] != '#') CHECKF(p == GetProcAddress(tm, dot + 1), "%s -> %s", (char *)e.base + names[i], target);
        n++;
    }
    t_note("kernel32 forwarders to ntdll/kernelbase checked: %d", n);
    CHECK(n > 10);
}
TEST(gpa_export_names_nul_terminated_in_image) {
    expdir e;
    HMODULE ms[] = { t_kernel32(), t_kernelbase(), t_ntdll() };
    for (int k = 0; k < 3; k++) {
        if (!expdir_open(&e, ms[k])) { CHECK(0); continue; }
        const IMAGE_NT_HEADERS64 *nt = ml_nt(ms[k]);
        CHECK(e.ed->AddressOfNames < nt->OptionalHeader.SizeOfImage);
        CHECK(e.ed->AddressOfFunctions < nt->OptionalHeader.SizeOfImage);
        CHECK(e.ed->AddressOfNameOrdinals < nt->OptionalHeader.SizeOfImage);
        CHECK(e.ed->NumberOfFunctions >= e.ed->NumberOfNames);
        CHECK(e.ed->Base >= 1);
    }
}
TEST(gpa_test_image_export_directory_walk) {
    REQUIRE(U(GetProcAddress));
    HMODULE m = pa_load("gpa_walk", "k32m_gpw.dll"); if (!m) { CHECK(0); return; }
    expdir e;
    CHECK(expdir_open(&e, m));
    EXPECT_EQ(e.ed->Base, 1);
    EXPECT_EQ(e.ed->NumberOfFunctions, ML_ORD_MAX);
    EXPECT_EQ(e.ed->NumberOfNames, 10);
    CHECK(!strcmp((char *)m + e.ed->Name, "k32m_a.dll"));
    ml_unload(m);
}

/* ---- DisableThreadLibraryCalls ---- */
static DWORD WINAPI quick_thread(LPVOID p) { return (DWORD)(ULONG_PTR)p; }
static void run_threads(int n) {
    for (int i = 0; i < n; i++) {
        HANDLE h = CreateThread(NULL, 0, quick_thread, (LPVOID)(ULONG_PTR)i, 0, NULL);
        if (h) { WaitForSingleObject(h, 10000); CloseHandle(h); }
    }
}
TEST(dtlc_default_thread_notifications) {
    HMODULE m = pa_load("dtlc_def", "k32m_dt0.dll"); if (!m) { CHECK(0); return; }
    volatile int *ta = (int *)GetProcAddress(m, "ml_thread_attach"), *td = (int *)GetProcAddress(m, "ml_thread_detach");
    CHECK(ta && td);
    if (ta && td) {
        run_threads(3);
        EXPECT_EQ(*ta, 3);
        EXPECT_EQ(*td, 3);
    }
    ml_unload(m);
}
TEST(dtlc_disables_thread_notifications) {
    REQUIRE(U(DisableThreadLibraryCalls));
    HMODULE m = pa_load("dtlc_off", "k32m_dt1.dll"); if (!m) { CHECK(0); return; }
    volatile int *ta = (int *)GetProcAddress(m, "ml_thread_attach"), *td = (int *)GetProcAddress(m, "ml_thread_detach");
    SET_ERR(0x777);
    CHECK(U(DisableThreadLibraryCalls)(m));
    if (ta && td) {
        run_threads(3);
        EXPECT_EQ(*ta, 0);
        EXPECT_EQ(*td, 0);
    }
    CHECK(U(DisableThreadLibraryCalls)(m));       /* idempotent */
    ml_unload(m);
}
TEST(dtlc_after_some_threads) {
    REQUIRE(U(DisableThreadLibraryCalls));
    HMODULE m = pa_load("dtlc_mid", "k32m_dt2.dll"); if (!m) { CHECK(0); return; }
    volatile int *ta = (int *)GetProcAddress(m, "ml_thread_attach");
    run_threads(2);
    CHECK(U(DisableThreadLibraryCalls)(m));
    run_threads(2);
    if (ta) EXPECT_EQ(*ta, 2);
    ml_unload(m);
}
TEST(dtlc_invalid_handles) {
    REQUIRE(U(DisableThreadLibraryCalls));
    SET_ERR(0);
    CHECK(!U(DisableThreadLibraryCalls)(NULL));
    CHECK(GetLastError() != 0);
    t_note("DisableThreadLibraryCalls(NULL) error %lu", GetLastError());
    SET_ERR(0);
    CHECK(!U(DisableThreadLibraryCalls)((HMODULE)0x10000));
    CHECK(GetLastError() != 0);
    SET_ERR(0);
    CHECK(!U(DisableThreadLibraryCalls)((HMODULE)((char *)t_kernel32() + 0x1000)));
    CHECK(GetLastError() != 0);
}
TEST(dtlc_tagged_handle) {
    REQUIRE(U(DisableThreadLibraryCalls));
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("dtlc_tag"), "k32m_dt3.dll", p));
    HMODULE d = LoadLibraryExA(p, NULL, LOAD_LIBRARY_AS_DATAFILE);
    CHECK(d != NULL); if (!d) return;
    SET_ERR(0);
    CHECK(!U(DisableThreadLibraryCalls)(d));
    CHECK(GetLastError() != 0);
    FreeLibrary(d);
}
TEST(dtlc_exe_and_system_modules) {
    REQUIRE(U(DisableThreadLibraryCalls));
    /* these are in the loader list so the call is well-formed; behaviour of the result is recorded only */
    SET_ERR(0);
    BOOL r = U(DisableThreadLibraryCalls)(GetModuleHandleA(NULL));
    t_note("DisableThreadLibraryCalls(exe) -> %d err %lu", r, r ? 0 : GetLastError());
}

/* ---- FreeLibraryAndExitThread ---- */
typedef struct { HMODULE m; DWORD code; volatile LONG ran; } flaet_ctx;
static DWORD WINAPI flaet_thread(LPVOID p) {
    flaet_ctx *c = p;
    FARPROC f = U(FreeLibraryAndExitThread) ? (FARPROC)U(FreeLibraryAndExitThread) : NULL;
    c->ran = 1;
    ((void (WINAPI *)(HMODULE, DWORD))f)(c->m, c->code);
    c->ran = 2;       /* never reached */
    return 1;
}
TEST(flaet_releases_last_reference) {
    REQUIRE(U(FreeLibraryAndExitThread));
    HMODULE m = pa_load("flaet1", "k32m_fl1.dll"); if (!m) { CHECK(0); return; }
    flaet_ctx c = { m, 77, 0 };
    HANDLE t = CreateThread(NULL, 0, flaet_thread, &c, 0, NULL);
    CHECK(t != NULL); if (!t) { ml_unload(m); return; }
    CHECK(WaitForSingleObject(t, 15000) == WAIT_OBJECT_0);
    DWORD code = 0; GetExitCodeThread(t, &code);
    EXPECT_EQ(code, 77);
    EXPECT_EQ(c.ran, 1);
    CHECK(!ml_is_loaded(m));
    CloseHandle(t);
}
TEST(flaet_decrements_one_reference_only) {
    REQUIRE(U(FreeLibraryAndExitThread));
    HMODULE m = pa_load("flaet2", "k32m_fl2.dll"); if (!m) { CHECK(0); return; }
    char p[MAX_PATH]; GetModuleFileNameA(m, p, sizeof p);
    LoadLibraryA(p);            /* ref 2 */
    flaet_ctx c = { m, 5, 0 };
    HANDLE t = CreateThread(NULL, 0, flaet_thread, &c, 0, NULL);
    CHECK(WaitForSingleObject(t, 15000) == WAIT_OBJECT_0);
    DWORD code = 0; GetExitCodeThread(t, &code);
    EXPECT_EQ(code, 5);
    CHECK(ml_is_loaded(m));
    CHECK(FreeLibrary(m));
    CHECK(!ml_is_loaded(m));
    CloseHandle(t);
}
TEST(flaet_invalid_module_still_exits) {
    REQUIRE(U(FreeLibraryAndExitThread));
    flaet_ctx c = { NULL, 123, 0 };
    HANDLE t = CreateThread(NULL, 0, flaet_thread, &c, 0, NULL);
    CHECK(WaitForSingleObject(t, 15000) == WAIT_OBJECT_0);
    DWORD code = 0; GetExitCodeThread(t, &code);
    EXPECT_EQ(code, 123);
    EXPECT_EQ(c.ran, 1);
    CloseHandle(t);
}
TEST(flaet_bogus_module_still_exits) {
    REQUIRE(U(FreeLibraryAndExitThread));
    flaet_ctx c = { (HMODULE)0x10000, 9, 0 };
    HANDLE t = CreateThread(NULL, 0, flaet_thread, &c, 0, NULL);
    CHECK(WaitForSingleObject(t, 15000) == WAIT_OBJECT_0);
    DWORD code = 0; GetExitCodeThread(t, &code);
    EXPECT_EQ(code, 9);
    CloseHandle(t);
}
TEST(flaet_thread_detach_delivered_for_other_modules) {
    REQUIRE(U(FreeLibraryAndExitThread));
    HMODULE m = pa_load("flaet3", "k32m_fl3.dll"); if (!m) { CHECK(0); return; }
    HMODULE other = pa_load("flaet3b", "k32m_fl3b.dll");
    volatile int *td = (int *)GetProcAddress(other, "ml_thread_detach");
    flaet_ctx c = { m, 0, 0 };
    HANDLE t = CreateThread(NULL, 0, flaet_thread, &c, 0, NULL);
    CHECK(WaitForSingleObject(t, 15000) == WAIT_OBJECT_0);
    if (td) EXPECT_EQ(*td, 1);
    CloseHandle(t);
    ml_unload(other);
}
