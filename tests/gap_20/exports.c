/* gap20: presence, module placement, ordinal parity and aliasing of internal exports. */
#include "t.h"

#define K32_ONLY(X) X(ActivateActCtxWorker) X(AddLocalAlternateComputerNameA) X(AddLocalAlternateComputerNameW) \
    X(AddRefActCtxWorker) X(AdjustCalendarDate) X(BaseCheckElevation) X(BaseDllReadWriteIniFile) X(BaseFormatTimeOut) \
    X(BaseIsDosApplication) X(BaseQueryModuleData) X(BaseSetLastNTError) X(BaseThreadInitThunk) \
    X(BaseVerifyUnicodeString) X(Basep8BitStringToDynamicUnicodeString) X(BasepAnsiStringToDynamicUnicodeString) \
    X(BasepGetExeArchType) X(BasepMapModuleHandle)
#define BOTH(X) X(AppXGetOSMaxVersionTested) X(BaseCheckAppcompatCache) X(BaseCheckAppcompatCacheEx) \
    X(BaseFormatObjectAttributes) X(BaseGetNamedObjectDirectory)
#define KB_ONLY(X) X(AcquireStateLock) X(AddDependencyToProcessPackageGraph) X(AddExtensionProgId) \
    X(AddPackageToFamilyXref) X(AppContainerDeriveSidFromMoniker) X(AppContainerFreeMemory) \
    X(AppContainerLookupDisplayNameMrtReference) X(AppContainerLookupMoniker) X(AppContainerRegisterSid) \
    X(AppContainerUnregisterSid) X(AppXFreeMemory) X(AppXGetApplicationData) X(AppXGetDevelopmentMode) \
    X(AppXGetOSMinVersion) X(AppXGetPackageCapabilities) X(AppXGetPackageSid) X(AppXLookupDisplayName) \
    X(AppXLookupMoniker) X(AppXPostSuccessExtension) X(AppXPreCreationExtension) X(AppXReleaseAppXContext) \
    X(AppXUpdatePackageCapabilities) X(ApplicationUserModelIdFromProductId) X(BaseMarkFileForDelete) \
    X(BasepCopyFileExW) X(CLOSE_LOCAL_HANDLE_INTERNAL) X(ChrCmpIA) X(ChrCmpIW)

/* Walk the export directory of module m. Returns 1 if exported by name. */
static int find_export(HMODULE m, const char *name, WORD *ord, const char **fwd, FARPROC *addr)
{
    BYTE *base = (BYTE *)m;
    IMAGE_DOS_HEADER *dh = (IMAGE_DOS_HEADER *)base;
    IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(base + dh->e_lfanew);
    IMAGE_DATA_DIRECTORY *dd = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!dd->VirtualAddress) return 0;
    IMAGE_EXPORT_DIRECTORY *ed = (IMAGE_EXPORT_DIRECTORY *)(base + dd->VirtualAddress);
    DWORD *names = (DWORD *)(base + ed->AddressOfNames);
    WORD *ords = (WORD *)(base + ed->AddressOfNameOrdinals);
    DWORD *funcs = (DWORD *)(base + ed->AddressOfFunctions);
    for (DWORD i = 0; i < ed->NumberOfNames; i++) {
        if (strcmp((const char *)(base + names[i]), name)) continue;
        DWORD rva = funcs[ords[i]];
        if (ord) *ord = (WORD)(ords[i] + ed->Base);
        int isfwd = rva >= dd->VirtualAddress && rva < dd->VirtualAddress + dd->Size;
        if (fwd) *fwd = isfwd ? (const char *)(base + rva) : NULL;
        if (addr) *addr = (FARPROC)(base + rva);
        return 1;
    }
    return 0;
}

static void check_present(HMODULE m, const char *mod, const char *name, int expect)
{
    WORD ord = 0; const char *fwd = NULL; FARPROC a = NULL;
    int f = find_export(m, name, &ord, &fwd, &a);
    if (t_is_wine()) { t_note("%s!%s present=%d (emulator, not enforced)", mod, name, f); return; }
    if (expect) {
        CHECKF(f, "%s must export %s", mod, name);
        if (!f) return;
        FARPROC byname = GetProcAddress(m, name);
        FARPROC byord = GetProcAddress(m, (LPCSTR)(ULONG_PTR)ord);
        CHECK(byname != NULL);
        CHECK(byname == byord);
        CHECK(ord != 0);
        if (fwd) t_note("%s!%s forwards to %s", mod, name, fwd);
        else CHECK(a == byname);
    } else {
        CHECKF(!f, "%s must not export %s", mod, name);
        CHECK(GetProcAddress(m, name) == NULL);
    }
}

#define GEN_K(n) TEST(gap20_present_k32_##n) { check_present(t_kernel32(), "kernel32", #n, 1); \
    FARPROC p = t_proc_k32(#n); if (!t_is_wine()) CHECK(p != NULL); }
#define GEN_B(n) TEST(gap20_present_both_##n) { check_present(t_kernel32(), "kernel32", #n, 1); \
    check_present(t_kernelbase(), "kernelbase", #n, 1); }
#define GEN_KB(n) TEST(gap20_present_kb_##n) { check_present(t_kernelbase(), "kernelbase", #n, 1); \
    check_present(t_kernel32(), "kernel32", #n, 0); \
    FARPROC p = t_proc_kb(#n); if (!t_is_wine()) CHECK(p != NULL); \
    /* generic lookup order falls through kernel32 to kernelbase */ \
    if (!t_is_wine()) CHECK(t_proc(#n) == p); }
K32_ONLY(GEN_K)
BOTH(GEN_B)
KB_ONLY(GEN_KB)

/* The kernel32-only names must not be exported by kernelbase. */
#define GEN_NOTKB(n) check_present(t_kernelbase(), "kernelbase", #n, 0);
TEST(gap20_k32_only_absent_from_kernelbase) { K32_ONLY(GEN_NOTKB) }

/* Names resolved via the generic lookup must be identical to the owning-module lookup. */
#define GEN_RES_K(n) if (!t_is_wine()) CHECK(t_proc(#n) == t_proc_k32(#n));
TEST(gap20_generic_lookup_prefers_kernel32) { K32_ONLY(GEN_RES_K) BOTH(GEN_RES_K) }

/* Aliased exports share one body in kernelbase (same RVA in the real image). */
TEST(gap20_alias_appx_free_memory) {
    FARPROC a = t_proc_kb("AppXFreeMemory"), b = t_proc_kb("AppContainerFreeMemory");
    REQUIRE(a); REQUIRE(b);
    if (t_is_wine()) { t_note("alias not enforced on emulator"); return; }
    CHECK(a == b);
}
TEST(gap20_alias_osminversion_updatecaps) {
    FARPROC a = t_proc_kb("AppXGetOSMinVersion"), b = t_proc_kb("AppXUpdatePackageCapabilities");
    REQUIRE(a); REQUIRE(b);
    if (t_is_wine()) { t_note("alias not enforced on emulator"); return; }
    CHECK(a == b);
}
TEST(gap20_alias_productid_family) {
    const char *n[] = { "ApplicationUserModelIdFromProductId", "PackageFullNameFromProductId", "PackageIdFromProductId",
                        "PackageRelativeApplicationIdFromProductId", "ProductIdFromPackageFamilyName" };
    FARPROC base = t_proc_kb(n[0]);
    REQUIRE(base);
    if (t_is_wine()) { t_note("alias not enforced on emulator"); return; }
    for (int i = 1; i < 5; i++) {
        FARPROC p = t_proc_kb(n[i]);
        CHECKF(p == base, "%s should alias %s", n[i], n[0]);
    }
}
TEST(gap20_exports_inside_module_image) {
    HMODULE ms[2] = { t_kernel32(), t_kernelbase() };
    const char *nm[] = { "BaseSetLastNTError", "BaseFormatTimeOut", "BasepMapModuleHandle", "ChrCmpIA", "ChrCmpIW", "BaseMarkFileForDelete" };
    for (int i = 0; i < 6; i++) {
        FARPROC p = t_proc(nm[i]);
        if (!p) continue;
        HMODULE owner = NULL;
        CHECK(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &owner));
        if (!t_is_wine()) CHECK(owner == ms[0] || owner == ms[1]);
    }
}
