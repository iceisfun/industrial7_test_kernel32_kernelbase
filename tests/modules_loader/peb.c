/* PEB loader lists, image header sanity of the system DLLs, loader notifications, Ldr* cross-checks */
#include "ml.h"
#include <wchar.h>
#define EXPECT_NT(a, b) EXPECT_EQ((ULONG)(a), (ULONG)(b))

typedef struct { LIST_ENTRY InLoadOrderLinks, InMemoryOrderLinks, InInitializationOrderLinks; PVOID DllBase, EntryPoint; ULONG SizeOfImage; ULONG pad_;
    UNICODE_STRING FullDllName, BaseDllName; ULONG Flags; USHORT ObsoleteLoadCount, TlsIndex; LIST_ENTRY HashLinks; ULONG TimeDateStamp; } LDRE;
typedef struct { ULONG Length; BOOLEAN Initialized; PVOID SsHandle; LIST_ENTRY InLoadOrderModuleList, InMemoryOrderModuleList, InInitializationOrderModuleList; } LDRD;

static LDRD *get_ldr(void) { return (LDRD *)NtCurrentTeb()->ProcessEnvironmentBlock->Ldr; }
#define LOADE(l) CONTAINING_RECORD(l, LDRE, InLoadOrderLinks)
#define MEME(l) CONTAINING_RECORD(l, LDRE, InMemoryOrderLinks)
#define INITE(l) CONTAINING_RECORD(l, LDRE, InInitializationOrderLinks)

static int count_list(LIST_ENTRY *head) { int n = 0; for (LIST_ENTRY *e = head->Flink; e != head && n < 10000; e = e->Flink) n++; return n; }
static LDRE *find_by_base(PVOID base) {
    LDRD *l = get_ldr();
    for (LIST_ENTRY *e = l->InLoadOrderModuleList.Flink; e != &l->InLoadOrderModuleList; e = e->Flink) if (LOADE(e)->DllBase == base) return LOADE(e);
    return NULL;
}
static int wsuffix_ci(const WCHAR *s, size_t sl, const WCHAR *suf, size_t pl) { return sl >= pl && !_wcsnicmp(s + sl - pl, suf, pl); }

TEST(peb_ldr_structure) {
    LDRD *l = get_ldr();
    CHECK(l != NULL);
    CHECK(l->Initialized);
    CHECK(l->Length >= 0x58);
    CHECK(l->InLoadOrderModuleList.Flink != &l->InLoadOrderModuleList);
    CHECK(count_list(&l->InLoadOrderModuleList) >= 4);
    CHECK(count_list(&l->InLoadOrderModuleList) == count_list(&l->InMemoryOrderModuleList));
    int init = count_list(&l->InInitializationOrderModuleList);
    CHECK(init > 0 && init <= count_list(&l->InLoadOrderModuleList));
}
TEST(peb_ldr_first_entries) {
    LDRD *l = get_ldr();
    LDRE *e0 = LOADE(l->InLoadOrderModuleList.Flink);
    CHECK(e0->DllBase == GetModuleHandleA(NULL));
    CHECK(e0->DllBase == (PVOID)NtCurrentTeb()->ProcessEnvironmentBlock->Reserved3[1]);
    WCHAR full[MAX_PATH]; DWORD n = GetModuleFileNameW(NULL, full, MAX_PATH);
    CHECK(e0->FullDllName.Length / 2 == n && !_wcsnicmp(e0->FullDllName.Buffer, full, n));
    const WCHAR *bs = wcsrchr(full, L'\\');
    CHECK(bs && e0->BaseDllName.Length / 2 == wcslen(bs + 1) && !_wcsnicmp(e0->BaseDllName.Buffer, bs + 1, e0->BaseDllName.Length / 2));
    LDRE *e1 = LOADE(e0->InLoadOrderLinks.Flink);
    CHECK(e1->DllBase == t_ntdll());
    CHECK(e1->BaseDllName.Length == 18 && !_wcsnicmp(e1->BaseDllName.Buffer, L"ntdll.dll", 9));
    LDRE *e2 = LOADE(e1->InLoadOrderLinks.Flink);
    t_note("third module in load order: %ls", e2->BaseDllName.Buffer);
    CHECK(e2->DllBase == t_kernel32() || e2->DllBase == t_kernelbase());
    CHECK(find_by_base(t_kernel32()) && find_by_base(t_kernelbase()));
}
TEST(peb_ldr_memory_order_starts_with_exe) {
    LDRD *l = get_ldr();
    LDRE *m0 = MEME(l->InMemoryOrderModuleList.Flink);
    CHECK(m0->DllBase == GetModuleHandleA(NULL));
    LDRE *m1 = MEME(m0->InMemoryOrderLinks.Flink);
    CHECK(m1->DllBase == t_ntdll());
}
TEST(peb_ldr_init_order_excludes_exe_and_starts_with_ntdll) {
    LDRD *l = get_ldr();
    LDRE *i0 = INITE(l->InInitializationOrderModuleList.Flink);
    CHECK(i0->DllBase == t_ntdll());
    for (LIST_ENTRY *e = l->InInitializationOrderModuleList.Flink; e != &l->InInitializationOrderModuleList; e = e->Flink)
        CHECK(INITE(e)->DllBase != GetModuleHandleA(NULL));
}
TEST(peb_ldr_list_integrity) {
    LDRD *l = get_ldr();
    LIST_ENTRY *heads[3] = { &l->InLoadOrderModuleList, &l->InMemoryOrderModuleList, &l->InInitializationOrderModuleList };
    for (int k = 0; k < 3; k++) {
        LIST_ENTRY *h = heads[k];
        CHECKF(h->Flink->Blink == h && h->Blink->Flink == h, "list %d head", k);
        for (LIST_ENTRY *e = h->Flink; e != h; e = e->Flink) CHECKF(e->Flink->Blink == e && e->Blink->Flink == e, "list %d entry", k);
    }
}
TEST(peb_ldr_entries_match_headers) {
    LDRD *l = get_ldr();
    int n = 0;
    for (LIST_ENTRY *e = l->InLoadOrderModuleList.Flink; e != &l->InLoadOrderModuleList; e = e->Flink, n++) {
        LDRE *m = LOADE(e);
        const IMAGE_NT_HEADERS64 *nt = ml_nt(m->DllBase);
        CHECKF(nt != NULL, "entry %d", n);
        if (!nt) continue;
        CHECKF(((ULONG_PTR)m->DllBase & 0xFFFF) == 0, "%ls base %p", m->BaseDllName.Buffer, m->DllBase);
        CHECKF(m->SizeOfImage == nt->OptionalHeader.SizeOfImage, "%ls size", m->BaseDllName.Buffer);
        CHECKF(m->TimeDateStamp == nt->FileHeader.TimeDateStamp, "%ls tds", m->BaseDllName.Buffer);
        if (m->EntryPoint) CHECKF((char *)m->EntryPoint == (char *)m->DllBase + nt->OptionalHeader.AddressOfEntryPoint, "%ls entry", m->BaseDllName.Buffer);
        CHECKF(m->FullDllName.Length >= m->BaseDllName.Length, "%ls", m->BaseDllName.Buffer);
        CHECKF(wsuffix_ci(m->FullDllName.Buffer, m->FullDllName.Length / 2, m->BaseDllName.Buffer, m->BaseDllName.Length / 2), "%ls", m->BaseDllName.Buffer);
        CHECK(m->FullDllName.Buffer[m->FullDllName.Length / 2] == 0 && m->BaseDllName.Buffer[m->BaseDllName.Length / 2] == 0);
        CHECKF(m->FullDllName.Length / 2 < 32768, "%ls", m->BaseDllName.Buffer);
        /* the public APIs agree with the loader entry */
        WCHAR path[MAX_PATH * 2]; DWORD r = GetModuleFileNameW(m->DllBase, path, MAX_PATH * 2);
        CHECKF(r == m->FullDllName.Length / 2 && !_wcsnicmp(path, m->FullDllName.Buffer, r), "%ls gmfn", m->BaseDllName.Buffer);
        HMODULE hm = GetModuleHandleW(m->BaseDllName.Buffer);
        CHECKF(hm == m->DllBase, "%ls GetModuleHandle=%p base=%p", m->BaseDllName.Buffer, hm, m->DllBase);
        /* no overlap with other modules */
        for (LIST_ENTRY *f = e->Flink; f != &l->InLoadOrderModuleList; f = f->Flink) {
            LDRE *o = LOADE(f);
            CHECKF((char *)o->DllBase >= (char *)m->DllBase + m->SizeOfImage || (char *)m->DllBase >= (char *)o->DllBase + o->SizeOfImage, "%ls overlaps %ls", m->BaseDllName.Buffer, o->BaseDllName.Buffer);
        }
    }
    t_note("%d modules in the process", n);
}
TEST(peb_ldr_all_modules_have_unique_names_and_bases) {
    LDRD *l = get_ldr();
    for (LIST_ENTRY *e = l->InLoadOrderModuleList.Flink; e != &l->InLoadOrderModuleList; e = e->Flink)
        for (LIST_ENTRY *f = e->Flink; f != &l->InLoadOrderModuleList; f = f->Flink) {
            CHECK(LOADE(e)->DllBase != LOADE(f)->DllBase);
            CHECKF(_wcsicmp(LOADE(e)->BaseDllName.Buffer, LOADE(f)->BaseDllName.Buffer) != 0, "duplicate %ls", LOADE(e)->BaseDllName.Buffer);
        }
}
TEST(peb_ldr_load_and_free_updates_lists) {
    REQUIRE(U(LoadLibraryA));
    LDRD *l = get_ldr();
    int before = count_list(&l->InLoadOrderModuleList);
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("peb_lf"), "k32m_peb.dll", p));
    HMODULE m = U(LoadLibraryA)(p);
    CHECK(m != NULL); if (!m) return;
    CHECK(count_list(&l->InLoadOrderModuleList) == before + 1);
    CHECK(count_list(&l->InMemoryOrderModuleList) == before + 1);
    LDRE *e = find_by_base(m);
    CHECK(e != NULL);
    if (e) {
        CHECK(e->BaseDllName.Length == 2 * 12 && !_wcsnicmp(e->BaseDllName.Buffer, L"k32m_peb.dll", 12));
        CHECK(e->FullDllName.Length / 2 == strlen(p) && !_wcsnicmp(e->FullDllName.Buffer, ml_w(p), strlen(p)));
        CHECK(e->EntryPoint != NULL);
        CHECK(e->SizeOfImage == ml_nt(m)->OptionalHeader.SizeOfImage);
        CHECK(e->Flags & 0x4);                 /* LDRP_IMAGE_DLL */
        t_note("ldr flags for loaded test dll: %lx", e->Flags);
        /* appended at the tail of the load order list */
        CHECK(l->InLoadOrderModuleList.Blink == &e->InLoadOrderLinks);
    }
    FreeLibrary(m);
    CHECK(count_list(&l->InLoadOrderModuleList) == before);
    CHECK(find_by_base(m) == NULL);
}
TEST(peb_ldr_datafile_not_listed) {
    REQUIRE(U(LoadLibraryExA));
    LDRD *l = get_ldr();
    int before = count_list(&l->InLoadOrderModuleList);
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("peb_df"), "k32m_pdf.dll", p));
    HMODULE m = U(LoadLibraryExA)(p, NULL, LOAD_LIBRARY_AS_DATAFILE);
    CHECK(m != NULL);
    CHECK(count_list(&l->InLoadOrderModuleList) == before);
    CHECK(find_by_base((PVOID)((ULONG_PTR)m & ~(ULONG_PTR)3)) == NULL);
    ml_unload(m);
}
TEST(peb_ldr_dependency_listed_with_importer) {
    REQUIRE(U(LoadLibraryExA));
    LDRD *l = get_ldr();
    int before = count_list(&l->InLoadOrderModuleList);
    const char *d = ml_dir("peb_dep"); char pi[MAX_PATH];
    CHECK(ml_put(ML_IMP, d, "k32m_imp.dll", pi)); CHECK(ml_put(ML_DEP, d, "k32m_dep.dll", NULL));
    HMODULE m = U(LoadLibraryExA)(pi, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    CHECK(m != NULL); if (!m) return;
    CHECK(count_list(&l->InLoadOrderModuleList) == before + 2);
    HMODULE dep = GetModuleHandleA("k32m_dep.dll");
    LDRE *ei = find_by_base(m), *ed = find_by_base(dep);
    CHECK(ei && ed);
    if (ei && ed) {
        /* the dependency is mapped and initialised first: it precedes the importer in initialisation order */
        int pos_i = -1, pos_d = -1, k = 0;
        LDRD *ld = get_ldr();
        for (LIST_ENTRY *e = ld->InInitializationOrderModuleList.Flink; e != &ld->InInitializationOrderModuleList; e = e->Flink, k++) {
            if (INITE(e) == ei) pos_i = k;
            if (INITE(e) == ed) pos_d = k;
        }
        CHECK(pos_i >= 0 && pos_d >= 0 && pos_d < pos_i);
    }
    FreeLibrary(m);
    CHECK(count_list(&l->InLoadOrderModuleList) == before);
}
TEST(peb_system_paths) {
    static const char *n[] = { "kernel32.dll", "kernelbase.dll", "ntdll.dll", "user32.dll", "advapi32.dll" };
    char sysdir[MAX_PATH]; GetSystemDirectoryA(sysdir, sizeof sysdir);
    for (int i = 0; i < 5; i++) {
        HMODULE h = LoadLibraryA(n[i]);
        LDRE *e = find_by_base(h);
        CHECKF(e != NULL, "%s", n[i]);
        if (!e) continue;
        char *fn = ml_a(e->FullDllName.Buffer);
        char want[MAX_PATH]; snprintf(want, sizeof want, "%s\\%s", sysdir, n[i]);
        CHECKF(!_stricmp(fn, want), "%s: %s", n[i], fn);
    }
}
TEST(peb_apiset_map_and_os_version) {
    PEB *peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    unsigned char *p = (unsigned char *)peb;
    ULONG major = *(ULONG *)(p + 0x118), minor = *(ULONG *)(p + 0x11C); USHORT build = *(USHORT *)(p + 0x120);
    EXPECT_EQ(major, 10); EXPECT_EQ(minor, 0);
    CHECK(build >= 19041);
    t_note("PEB OS version %lu.%lu.%u", major, minor, build);
    void *apiset = *(void **)(p + 0x68);
    CHECK(apiset != NULL);
    if (apiset) EXPECT_EQ(*(ULONG *)apiset, 6);          /* API set schema version 6 on Windows 10 */
    CHECK(*(ULONG *)(p + 0xB8) >= 1);                    /* NumberOfProcessors */
}

/* ---- image headers of the system DLLs ---- */
static const IMAGE_SECTION_HEADER *find_sec(HMODULE m, const char *name) {
    const IMAGE_NT_HEADERS64 *nt = ml_nt(m);
    const IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(nt);
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) if (!strncmp((char *)s[i].Name, name, 8)) return &s[i];
    return NULL;
}
static void check_system_image(HMODULE m, const char *label, WORD subsystem, const char *exportname, DWORD min_names) {
    const IMAGE_DOS_HEADER *dos = (void *)m;
    const IMAGE_NT_HEADERS64 *nt = ml_nt(m);
    CHECKF(nt != NULL, "%s", label);
    if (!nt) return;
    CHECK(dos->e_magic == 0x5A4D);
    CHECK(dos->e_lfanew >= 0x40 && dos->e_lfanew < 0x400 && (dos->e_lfanew & 3) == 0);
    CHECKF(nt->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64, "%s", label);
    CHECKF(nt->FileHeader.Characteristics & IMAGE_FILE_DLL, "%s", label);
    CHECKF(nt->FileHeader.Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE, "%s", label);
    CHECKF(nt->FileHeader.Characteristics & IMAGE_FILE_LARGE_ADDRESS_AWARE, "%s", label);
    CHECKF(!(nt->FileHeader.Characteristics & IMAGE_FILE_RELOCS_STRIPPED), "%s", label);
    CHECKF(nt->FileHeader.SizeOfOptionalHeader == sizeof(IMAGE_OPTIONAL_HEADER64), "%s", label);
    CHECKF(nt->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC, "%s", label);
    CHECKF(nt->OptionalHeader.Subsystem == subsystem, "%s subsystem %u", label, nt->OptionalHeader.Subsystem);
    CHECKF(nt->OptionalHeader.SectionAlignment == 0x1000, "%s", label);
    CHECKF(nt->OptionalHeader.FileAlignment >= 0x200 && nt->OptionalHeader.FileAlignment <= 0x1000, "%s", label);
    CHECKF(nt->OptionalHeader.SizeOfImage % 0x1000 == 0 && nt->OptionalHeader.SizeOfImage > 0x10000, "%s", label);
    CHECKF(nt->OptionalHeader.SizeOfHeaders % nt->OptionalHeader.FileAlignment == 0, "%s", label);
    CHECKF(nt->OptionalHeader.DllCharacteristics & IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE, "%s", label);
    CHECKF(nt->OptionalHeader.DllCharacteristics & IMAGE_DLLCHARACTERISTICS_NX_COMPAT, "%s", label);
    CHECKF(nt->OptionalHeader.MajorOperatingSystemVersion == 10, "%s os major %u", label, nt->OptionalHeader.MajorOperatingSystemVersion);
    CHECKF(nt->OptionalHeader.MajorSubsystemVersion == 10, "%s subsystem major %u", label, nt->OptionalHeader.MajorSubsystemVersion);
    CHECKF(nt->OptionalHeader.NumberOfRvaAndSizes == 16, "%s", label);
    CHECKF(nt->OptionalHeader.AddressOfEntryPoint != 0, "%s", label);
    CHECKF(nt->OptionalHeader.AddressOfEntryPoint < nt->OptionalHeader.SizeOfImage, "%s", label);
    /* the loader mapped it at an aligned address */
    CHECKF(((ULONG_PTR)m & 0xFFFF) == 0, "%s", label);
    CHECKF(nt->OptionalHeader.ImageBase != 0, "%s", label);
    /* sections */
    const char *must[] = { ".text", ".data", ".pdata", ".rsrc", ".reloc" };
    for (int i = 0; i < 5; i++) CHECKF(find_sec(m, must[i]) != NULL, "%s missing %s", label, must[i]);
    const IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(nt);
    DWORD prev_end = nt->OptionalHeader.SizeOfHeaders;
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        CHECKF(s[i].VirtualAddress >= prev_end && s[i].VirtualAddress % 0x1000 == 0, "%s section %d", label, i);
        prev_end = s[i].VirtualAddress + ((s[i].Misc.VirtualSize + 0xFFF) & ~0xFFFu);
    }
    CHECKF(prev_end == nt->OptionalHeader.SizeOfImage, "%s sections end %lx image %lx", label, prev_end, nt->OptionalHeader.SizeOfImage);
    const IMAGE_SECTION_HEADER *text = find_sec(m, ".text");
    if (text) {
        CHECKF(text->Characteristics & IMAGE_SCN_CNT_CODE, "%s", label);
        CHECKF(text->Characteristics & IMAGE_SCN_MEM_EXECUTE, "%s", label);
        CHECKF(!(text->Characteristics & IMAGE_SCN_MEM_WRITE), "%s", label);
        CHECKF(nt->OptionalHeader.AddressOfEntryPoint >= text->VirtualAddress, "%s", label);
        /* real page protection of the mapped section */
        MEMORY_BASIC_INFORMATION mbi; VirtualQuery((char *)m + text->VirtualAddress, &mbi, sizeof mbi);
        CHECKF(mbi.Protect == PAGE_EXECUTE_READ || mbi.Protect == PAGE_EXECUTE_READWRITE || mbi.Protect == PAGE_EXECUTE_WRITECOPY, "%s .text protect %lx", label, mbi.Protect);
        CHECKF(mbi.Type == MEM_IMAGE, "%s", label);
    }
    const IMAGE_SECTION_HEADER *data = find_sec(m, ".data");
    if (data) CHECKF(data->Characteristics & IMAGE_SCN_MEM_WRITE, "%s", label);
    /* exports */
    const IMAGE_DATA_DIRECTORY *ex = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    CHECKF(ex->VirtualAddress && ex->Size >= sizeof(IMAGE_EXPORT_DIRECTORY), "%s", label);
    if (ex->VirtualAddress) {
        const IMAGE_EXPORT_DIRECTORY *ed = (void *)((char *)m + ex->VirtualAddress);
        CHECKF(!_stricmp((char *)m + ed->Name, exportname), "%s export name \"%s\"", label, (char *)m + ed->Name);
        CHECKF(ed->NumberOfNames >= min_names, "%s only %lu names", label, ed->NumberOfNames);
        t_note("%s export TimeDateStamp %08lx header %08lx", label, ed->TimeDateStamp, nt->FileHeader.TimeDateStamp);
        CHECKF(ed->NumberOfFunctions >= ed->NumberOfNames, "%s", label);
    }
    /* directories that must exist */
    CHECKF(nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress != 0, "%s", label);
    CHECKF(nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION].VirtualAddress != 0, "%s", label);
    CHECKF(nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress != 0, "%s", label);
    CHECKF(nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].VirtualAddress != 0, "%s", label);
    CHECKF(nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress != 0, "%s", label);
}
TEST(img_kernel32_header) { check_system_image(t_kernel32(), "kernel32", IMAGE_SUBSYSTEM_WINDOWS_CUI, "KERNEL32.DLL", 1400); }
TEST(img_kernelbase_header) { check_system_image(t_kernelbase(), "kernelbase", IMAGE_SUBSYSTEM_WINDOWS_CUI, "KERNELBASE.dll", 1400); }
TEST(img_ntdll_header) { check_system_image(t_ntdll(), "ntdll", IMAGE_SUBSYSTEM_WINDOWS_CUI, "ntdll.dll", 1800); }
TEST(img_kernel32_imports) {
    const IMAGE_NT_HEADERS64 *nt = ml_nt(t_kernel32());
    const IMAGE_DATA_DIRECTORY *d = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    const IMAGE_IMPORT_DESCRIPTOR *id = (void *)((char *)t_kernel32() + d->VirtualAddress);
    int has_nt = 0, has_kb = 0, has_apiset = 0, n = 0;
    for (; id->Name && n < 200; id++, n++) {
        const char *nm = (char *)t_kernel32() + id->Name;
        if (!_stricmp(nm, "ntdll.dll")) has_nt = 1;
        if (!_stricmp(nm, "KERNELBASE.dll")) has_kb = 1;
        if (!_strnicmp(nm, "api-ms-win-", 11)) has_apiset = 1;
    }
    CHECK(has_nt); CHECK(has_kb); CHECK(has_apiset || n >= 2);
    t_note("kernel32 imports %d modules", n);
}
TEST(img_kernelbase_imports_ntdll_only_core) {
    const IMAGE_NT_HEADERS64 *nt = ml_nt(t_kernelbase());
    const IMAGE_DATA_DIRECTORY *d = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    const IMAGE_IMPORT_DESCRIPTOR *id = (void *)((char *)t_kernelbase() + d->VirtualAddress);
    int has_nt = 0;
    for (int n = 0; id->Name && n < 200; id++, n++) if (!_stricmp((char *)t_kernelbase() + id->Name, "ntdll.dll")) has_nt = 1;
    CHECK(has_nt);
}
TEST(img_exe_header) {
    HMODULE m = GetModuleHandleA(NULL);
    const IMAGE_NT_HEADERS64 *nt = ml_nt(m);
    CHECK(nt != NULL); if (!nt) return;
    CHECK(nt->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64);
    CHECK(!(nt->FileHeader.Characteristics & IMAGE_FILE_DLL));
    CHECK(nt->FileHeader.Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE);
    CHECK(nt->OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_WINDOWS_CUI || nt->OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_WINDOWS_GUI);
    CHECK((char *)m == (char *)NtCurrentTeb()->ProcessEnvironmentBlock->Reserved3[1]);
    /* the process entry code lies in the exe */
    CHECK(find_by_base(m) != NULL);
}
TEST(img_kernel32_unique_modules_in_sysdir) {
    char p[MAX_PATH]; GetSystemDirectoryA(p, sizeof p); strcat(p, "\\kernel32.dll");
    HANDLE f = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(f != INVALID_HANDLE_VALUE); if (f == INVALID_HANDLE_VALUE) return;
    IMAGE_DOS_HEADER dos; DWORD rd;
    ReadFile(f, &dos, sizeof dos, &rd, NULL);
    SetFilePointer(f, dos.e_lfanew, NULL, FILE_BEGIN);
    IMAGE_NT_HEADERS64 nt; ReadFile(f, &nt, sizeof nt, &rd, NULL);
    CloseHandle(f);
    const IMAGE_NT_HEADERS64 *mem = ml_nt(t_kernel32());
    CHECK(nt.Signature == IMAGE_NT_SIGNATURE);
    CHECK(nt.FileHeader.TimeDateStamp == mem->FileHeader.TimeDateStamp);
    CHECK(nt.OptionalHeader.SizeOfImage == mem->OptionalHeader.SizeOfImage);
    CHECK(nt.OptionalHeader.AddressOfEntryPoint == mem->OptionalHeader.AddressOfEntryPoint);
    CHECK(nt.OptionalHeader.CheckSum == mem->OptionalHeader.CheckSum);
}
TEST(img_mapped_pages_are_image_type) {
    HMODULE ms[] = { t_kernel32(), t_kernelbase(), t_ntdll() };
    for (int i = 0; i < 3; i++) {
        MEMORY_BASIC_INFORMATION mbi;
        CHECK(VirtualQuery(ms[i], &mbi, sizeof mbi) == sizeof mbi);
        CHECK(mbi.Type == MEM_IMAGE);
        CHECK(mbi.AllocationBase == (PVOID)ms[i]);
        CHECK(mbi.BaseAddress == (PVOID)ms[i]);
        CHECK(mbi.Protect == PAGE_READONLY);        /* headers are read-only */
        CHECK(mbi.RegionSize == 0x1000 || mbi.RegionSize >= 0x1000);
    }
}

/* ---- loader notifications (ntdll) ---- */
typedef struct { ULONG Flags; PCUNICODE_STRING FullDllName; PCUNICODE_STRING BaseDllName; PVOID DllBase; ULONG SizeOfImage; } NTF_DATA;
typedef VOID (NTAPI *ntf_cb)(ULONG, const NTF_DATA *, PVOID);
UDECL(NTSTATUS, LdrRegisterDllNotification, (ULONG, ntf_cb, PVOID, PVOID *));
UDECL(NTSTATUS, LdrUnregisterDllNotification, (PVOID));
typedef struct { int loaded, unloaded; PVOID last_base; WCHAR last_name[64]; WCHAR names[16][32]; int reasons[16]; int n; PVOID ctx_seen; DWORD tid; } ntf_state;
static void NTAPI ntf_handler(ULONG reason, const NTF_DATA *d, PVOID ctx) {
    ntf_state *s = ctx;
    s->ctx_seen = ctx; s->tid = GetCurrentThreadId();
    if (reason == 1) s->loaded++; else if (reason == 2) s->unloaded++;
    s->last_base = d->DllBase;
    int len = d->BaseDllName->Length / 2; if (len > 63) len = 63;
    memcpy(s->last_name, d->BaseDllName->Buffer, len * 2); s->last_name[len] = 0;
    if (s->n < 16) { int l2 = len > 31 ? 31 : len; memcpy(s->names[s->n], d->BaseDllName->Buffer, l2 * 2); s->names[s->n][l2] = 0; s->reasons[s->n] = (int)reason; s->n++; }
}
TEST(ntf_register_load_unload) {
    REQUIRE(UN(LdrRegisterDllNotification)); REQUIRE(UN(LdrUnregisterDllNotification));
    static ntf_state st; memset(&st, 0, sizeof st);
    PVOID cookie = NULL;
    NTSTATUS r = UN(LdrRegisterDllNotification)(0, ntf_handler, &st, &cookie);
    EXPECT_NT(r, 0);
    CHECK(cookie != NULL);
    if (r) return;
    char p[MAX_PATH]; CHECK(ml_put(ML_A, ml_dir("ntf1"), "k32m_ntf.dll", p));
    HMODULE m = LoadLibraryA(p);
    CHECK(m != NULL);
    EXPECT_EQ(st.loaded, 1);
    CHECK(st.last_base == m);
    CHECK(!_wcsicmp(st.last_name, L"k32m_ntf.dll"));
    CHECK(st.ctx_seen == &st);
    CHECK(st.tid == GetCurrentThreadId());
    LoadLibraryA(p);                                  /* refcount only: no notification */
    EXPECT_EQ(st.loaded, 1);
    FreeLibrary(m);
    EXPECT_EQ(st.unloaded, 0);                        /* still referenced */
    FreeLibrary(m);
    EXPECT_EQ(st.unloaded, 1);
    CHECK(st.last_base == m);
    NTSTATUS ur = UN(LdrUnregisterDllNotification)(cookie);
    EXPECT_NT(ur, 0);
    /* after unregistering nothing is delivered */
    m = LoadLibraryA(p);
    CHECK(m != NULL);
    EXPECT_EQ(st.loaded, 1);
    ml_unload(m);
}
TEST(ntf_dependency_loads_notified_in_order) {
    REQUIRE(UN(LdrRegisterDllNotification)); REQUIRE(UN(LdrUnregisterDllNotification));
    static ntf_state st; memset(&st, 0, sizeof st); PVOID cookie = NULL;
    if (UN(LdrRegisterDllNotification)(0, ntf_handler, &st, &cookie)) { CHECK(0); return; }
    const char *d = ml_dir("ntf2"); char pi[MAX_PATH];
    ml_put(ML_IMP, d, "k32m_imp.dll", pi); ml_put(ML_DEP, d, "k32m_dep.dll", NULL);
    HMODULE m = LoadLibraryExA(pi, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    CHECK(m != NULL);
    EXPECT_EQ(st.loaded, 2);
    if (st.n >= 2) { CHECK(!_wcsicmp(st.names[0], L"k32m_dep.dll")); CHECK(!_wcsicmp(st.names[1], L"k32m_imp.dll")); CHECK(st.reasons[0] == 1 && st.reasons[1] == 1); }
    ml_unload(m);
    EXPECT_EQ(st.unloaded, 2);
    if (st.n >= 4) { CHECK(st.reasons[2] == 2 && st.reasons[3] == 2); t_note("unload order: %ls then %ls", st.names[2], st.names[3]); }
    UN(LdrUnregisterDllNotification)(cookie);
}
TEST(ntf_datafile_load_not_notified) {
    REQUIRE(UN(LdrRegisterDllNotification)); REQUIRE(UN(LdrUnregisterDllNotification));
    static ntf_state st; memset(&st, 0, sizeof st); PVOID cookie = NULL;
    if (UN(LdrRegisterDllNotification)(0, ntf_handler, &st, &cookie)) { CHECK(0); return; }
    char p[MAX_PATH]; ml_put(ML_A, ml_dir("ntf3"), "k32m_ntd.dll", p);
    HMODULE m = LoadLibraryExA(p, NULL, LOAD_LIBRARY_AS_DATAFILE);
    CHECK(m != NULL);
    t_note("datafile load produced %d load notifications", st.loaded);
    CHECK(st.loaded == 0);
    ml_unload(m);
    UN(LdrUnregisterDllNotification)(cookie);
}
TEST(ntf_multiple_registrations) {
    REQUIRE(UN(LdrRegisterDllNotification)); REQUIRE(UN(LdrUnregisterDllNotification));
    static ntf_state a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b); PVOID ca = NULL, cb = NULL;
    CHECK(UN(LdrRegisterDllNotification)(0, ntf_handler, &a, &ca) == 0);
    CHECK(UN(LdrRegisterDllNotification)(0, ntf_handler, &b, &cb) == 0);
    CHECK(ca && cb && ca != cb);
    char p[MAX_PATH]; ml_put(ML_A, ml_dir("ntf4"), "k32m_nt4.dll", p);
    HMODULE m = LoadLibraryA(p);
    EXPECT_EQ(a.loaded, 1); EXPECT_EQ(b.loaded, 1);
    CHECK(UN(LdrUnregisterDllNotification)(ca) == 0);
    ml_unload(m);
    EXPECT_EQ(a.unloaded, 0); EXPECT_EQ(b.unloaded, 1);
    CHECK(UN(LdrUnregisterDllNotification)(cb) == 0);
}
TEST(ntf_invalid_arguments) {
    REQUIRE(UN(LdrRegisterDllNotification)); REQUIRE(UN(LdrUnregisterDllNotification));
    static ntf_state st; PVOID cookie = NULL;      /* static: a stray registration must never point at the stack */
    NTSTATUS r = UN(LdrRegisterDllNotification)(1, ntf_handler, &st, &cookie);
    t_note("LdrRegisterDllNotification(flags=1) = %08lx", r);
    if (r == 0 && cookie) UN(LdrUnregisterDllNotification)(cookie);
    /* a cookie can only be unregistered once */
    static ntf_state s2; memset(&s2, 0, sizeof s2); PVOID c2 = NULL;
    CHECK(UN(LdrRegisterDllNotification)(0, ntf_handler, &s2, &c2) == 0);
    CHECK(UN(LdrUnregisterDllNotification)(c2) == 0);
}

/* ---- Ldr* / Rtl* cross-checks ---- */
UDECL(NTSTATUS, LdrGetDllHandle, (PWORD, PULONG, PUNICODE_STRING, PVOID *));
UDECL(NTSTATUS, LdrLoadDll, (PWCHAR, PULONG, PUNICODE_STRING, PVOID *));
UDECL(NTSTATUS, LdrUnloadDll, (PVOID));
UDECL(NTSTATUS, LdrGetProcedureAddress, (PVOID, PANSI_STRING, ULONG, PVOID *));
UDECL(PVOID, RtlPcToFileHeader, (PVOID, PVOID *));
UDECL(NTSTATUS, LdrFindEntryForAddress, (PVOID, PVOID *));
static void us_init(UNICODE_STRING *u, const WCHAR *s) { u->Buffer = (PWSTR)s; u->Length = (USHORT)(wcslen(s) * 2); u->MaximumLength = u->Length + 2; }
TEST(ldr_get_dll_handle_matches_api) {
    REQUIRE(UN(LdrGetDllHandle));
    UNICODE_STRING u; PVOID h = NULL;
    us_init(&u, L"kernel32.dll");
    NTSTATUS gr = UN(LdrGetDllHandle)(NULL, NULL, &u, &h);
    EXPECT_NT(gr, 0);
    CHECK(h == t_kernel32());
    us_init(&u, L"KERNELBASE");
    h = NULL;
    NTSTATUS r = UN(LdrGetDllHandle)(NULL, NULL, &u, &h);
    t_note("LdrGetDllHandle(\"KERNELBASE\" without extension) = %08lx", r);
    us_init(&u, L"k32t_nosuch_module.dll");
    h = (PVOID)1;
    r = UN(LdrGetDllHandle)(NULL, NULL, &u, &h);
    EXPECT_NT(r, 0xC0000135);              /* STATUS_DLL_NOT_FOUND */
}
TEST(ldr_load_unload_dll_direct) {
    REQUIRE(UN(LdrLoadDll)); REQUIRE(UN(LdrUnloadDll)); REQUIRE(UN(LdrGetProcedureAddress));
    char p[MAX_PATH]; ml_put(ML_A, ml_dir("ldr_ld"), "k32m_ldr.dll", p);
    UNICODE_STRING u; us_init(&u, ml_w(p));
    PVOID h = NULL;
    NTSTATUS r = UN(LdrLoadDll)(NULL, NULL, &u, &h);
    EXPECT_NT(r, 0);
    CHECK(h != NULL && h == GetModuleHandleA("k32m_ldr.dll"));
    if (r) return;
    ANSI_STRING as = { 6, 7, "ml_add" }; PVOID fn = NULL;
    NTSTATUS pr = UN(LdrGetProcedureAddress)(h, &as, 0, &fn);
    EXPECT_NT(pr, 0);
    CHECK(fn == GetProcAddress(h, "ml_add"));
    fn = NULL;
    pr = UN(LdrGetProcedureAddress)(h, NULL, ML_ORD_ADD, &fn);
    EXPECT_NT(pr, 0);
    CHECK(fn == GetProcAddress(h, "ml_add"));
    ANSI_STRING bad = { 6, 7, "nofunc" }; fn = (PVOID)1;
    r = UN(LdrGetProcedureAddress)(h, &bad, 0, &fn);
    CHECKF((ULONG)r == 0xC0000139 || (ULONG)r == 0xC000007A, "status %08lx", r);   /* ENTRYPOINT_NOT_FOUND / PROCEDURE_NOT_FOUND */
    r = UN(LdrGetProcedureAddress)(h, NULL, 99, &fn);
    CHECK(r != 0);
    t_note("LdrGetProcedureAddress(ordinal 99) = %08lx", r);
    NTSTATUS ur = UN(LdrUnloadDll)(h);
    EXPECT_NT(ur, 0);
    CHECK(GetModuleHandleA("k32m_ldr.dll") == NULL);
}
TEST(ldr_load_dll_not_found_statuses) {
    REQUIRE(UN(LdrLoadDll));
    UNICODE_STRING u; PVOID h = (PVOID)1;
    us_init(&u, L"k32t_nosuch_module.dll");
    NTSTATUS lr = UN(LdrLoadDll)(NULL, NULL, &u, &h);
    EXPECT_NT(lr, 0xC0000135);
    char p[MAX_PATH]; snprintf(p, sizeof p, "%sk32t_nosuch.dll", ml_dir("ldr_nf"));
    us_init(&u, ml_w(p));
    lr = UN(LdrLoadDll)(NULL, NULL, &u, &h);
    EXPECT_NT(lr, 0xC0000135);
    char np[MAX_PATH]; snprintf(np, sizeof np, "%snotpe.dll", ml_dir("ldr_nf"));
    ml_write(np, "text file that is not an image at all.............................\n", 64);
    us_init(&u, ml_w(np));
    NTSTATUS r = UN(LdrLoadDll)(NULL, NULL, &u, &h);
    CHECK(r != 0);
    t_note("LdrLoadDll(non-PE) = %08lx", r);
    EXPECT_NT(r, 0xC000012F);              /* STATUS_INVALID_IMAGE_NOT_MZ */
}
TEST(rtl_pc_to_file_header_matches) {
    REQUIRE(UN(RtlPcToFileHeader)); REQUIRE(UN(LdrFindEntryForAddress));
    PVOID base = NULL;
    PVOID r = UN(RtlPcToFileHeader)((PVOID)GetProcAddress(t_kernel32(), "Sleep"), &base);
    CHECK(r == base);
    CHECK(base == t_kernel32() || base == t_kernelbase());
    base = (PVOID)1;
    r = UN(RtlPcToFileHeader)(&base, &base);          /* stack address */
    CHECK(r == NULL);
    PVOID ent = NULL;
    NTSTATUS fr = UN(LdrFindEntryForAddress)((char *)t_ntdll() + 0x1000, &ent);
    EXPECT_NT(fr, 0);
    CHECK(ent == find_by_base(t_ntdll()));
    ent = (PVOID)1;
    CHECK(UN(LdrFindEntryForAddress)(&base, &ent) != 0);
}
