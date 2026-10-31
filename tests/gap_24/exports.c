/* gap24: presence, ordinals, forwarders and trivial pure helpers of non-public exports. */
#include "t.h"
#include <sddl.h>

#define PK(name) ((name##_pfn)t_proc_k32(#name))
#define PB(name) ((name##_pfn)t_proc_kb(#name))

static const struct { const char *n; int k32, kb; } ex24[] = {
  {"IsCalendarLeapDay",1,0},{"IsCalendarLeapMonth",1,0},{"IsCalendarLeapYear",1,0},
  {"IsCharBlankW",0,1},{"IsCharCntrlW",0,1},{"IsCharDigitW",0,1},{"IsCharPunctW",0,1},{"IsCharXDigitW",0,1},
  {"IsDeveloperModeEnabled",0,1},{"IsDeveloperModePolicyApplied",0,1},{"IsMrtResourceRedirectionEnabled",0,1},
  {"IsSideloadingEnabled",0,1},{"IsSideloadingPolicyApplied",0,1},{"IsTimeZoneRedirectionEnabled",0,1},
  {"IsValidCalDateTime",1,0},{"IsValidRelativeSecurityDescriptor",0,1},{"KernelBaseGetGlobalData",0,1},
  {"LZClose",1,0},{"LZCloseFile",1,0},{"LZCopy",1,0},{"LZCreateFileW",1,0},{"LZDone",1,0},{"LZInit",1,0},
  {"LZOpenFileA",1,0},{"LZOpenFileW",1,0},{"LZRead",1,0},{"LZSeek",1,0},{"LZStart",1,0},
  {"LoadStringBaseExW",1,1},{"LoadStringBaseW",1,0},{"LocalFileTimeToLocalSystemTime",1,1},
  {"LocalSystemTimeToLocalFileTime",1,1},{"MakeAbsoluteSD2",0,1},{"NamedPipeEventEnum",0,1},
  {"NamedPipeEventSelect",0,1},{"NlsGetACPFromLocale",0,1},{"NlsGetCacheUpdateCount",1,1},
  {"NlsIsUserDefaultLocale",0,1},{"NlsUpdateLocale",1,1},{"NlsUpdateSystemLocale",1,1},
  {"NlsValidateLocale",0,1},{"NotifyMountMgr",1,1},{"OOBEComplete",1,0},
  {"OpenGlobalizationUserSettingsKey",0,1},{"OpenPackageInfoByFullNameForMachine",0,1},
  {"OpenPackageInfoByFullNameForUser",0,1},{"OpenProfileUserMapping",1,0},{"OpenRegKey",0,1},
  {"OpenState",1,1},{"OpenStateAtom",0,1},
};
#define NEX24 (sizeof ex24 / sizeof ex24[0])

/* Look up an export in a module's export directory: ordinal and forwarder string. */
static int find_export(HMODULE m, const char *name, DWORD *ord, const char **fwd) {
    BYTE *b = (BYTE *)m;
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)b;
    IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(b + dos->e_lfanew);
    IMAGE_DATA_DIRECTORY dd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!dd.VirtualAddress) return 0;
    IMAGE_EXPORT_DIRECTORY *ed = (IMAGE_EXPORT_DIRECTORY *)(b + dd.VirtualAddress);
    DWORD *names = (DWORD *)(b + ed->AddressOfNames), *funcs = (DWORD *)(b + ed->AddressOfFunctions);
    WORD *ords = (WORD *)(b + ed->AddressOfNameOrdinals);
    for (DWORD i = 0; i < ed->NumberOfNames; i++) {
        if (strcmp((char *)(b + names[i]), name)) continue;
        DWORD rva = funcs[ords[i]];
        if (ord) *ord = ed->Base + ords[i];
        if (fwd) *fwd = (rva >= dd.VirtualAddress && rva < dd.VirtualAddress + dd.Size) ? (const char *)(b + rva) : NULL;
        return 1;
    }
    return 0;
}

TEST(gap24_export_presence) {
    HMODULE k = t_kernel32(), kb = t_kernelbase();
    for (size_t i = 0; i < NEX24; i++) {
        DWORD o; const char *f;
        int ink = find_export(k, ex24[i].n, &o, &f), inb = find_export(kb, ex24[i].n, &o, &f);
        if (ex24[i].k32) { if (!ink && t_is_wine()) t_note("wine lacks kernel32 %s", ex24[i].n); else CHECKF(ink, "kernel32 exports %s", ex24[i].n); }
        if (ex24[i].kb)  { if (!inb && t_is_wine()) t_note("wine lacks kernelbase %s", ex24[i].n); else CHECKF(inb, "kernelbase exports %s", ex24[i].n); }
        if (!ex24[i].k32 && ink) t_note("%s unexpectedly exported by kernel32", ex24[i].n);
        if (!ex24[i].kb && inb) t_note("%s unexpectedly exported by kernelbase", ex24[i].n);
    }
}

TEST(gap24_export_ordinals_unique_and_nonzero) {
    HMODULE k = t_kernel32(), kb = t_kernelbase();
    for (size_t i = 0; i < NEX24; i++) {
        DWORD o = 0; const char *f;
        if (ex24[i].k32 && find_export(k, ex24[i].n, &o, &f)) { CHECKF(o != 0, "k32 ordinal %s", ex24[i].n); t_note("k32 %s ord %lu%s%s", ex24[i].n, o, f ? " fwd " : "", f ? f : ""); }
        if (ex24[i].kb && find_export(kb, ex24[i].n, &o, &f)) { CHECKF(o != 0, "kb ordinal %s", ex24[i].n); CHECKF(f == NULL, "kernelbase %s must not be a forwarder", ex24[i].n); }
    }
}

TEST(gap24_forwarders_resolve_to_kernelbase) {
    HMODULE k = t_kernel32();
    for (size_t i = 0; i < NEX24; i++) {
        if (!(ex24[i].k32 && ex24[i].kb)) continue;
        const char *f = NULL; DWORD o;
        if (!find_export(k, ex24[i].n, &o, &f)) continue;
        FARPROC a = t_proc_k32(ex24[i].n), b = t_proc_kb(ex24[i].n);
        if (f) {
            CHECKF(_strnicmp(f, "KERNELBASE.", 11) == 0, "forwarder of %s is '%s'", ex24[i].n, f);
            CHECKF(a == b, "forwarded %s resolves to the kernelbase address", ex24[i].n);
        } else t_note("kernel32!%s is not a forwarder (local impl %p vs kb %p)", ex24[i].n, (void *)a, (void *)b);
    }
}

TEST(gap24_resolution_stable) {
    for (size_t i = 0; i < NEX24; i++) {
        FARPROC a = t_proc(ex24[i].n), b = t_proc(ex24[i].n);
        CHECKF(a == b, "stable %s", ex24[i].n);
        if (ex24[i].k32) CHECKF(a == t_proc_k32(ex24[i].n) || !t_proc_k32(ex24[i].n), "k32 first %s", ex24[i].n);
    }
}

TEST(gap24_not_exported_by_wrong_module) {
    /* kernelbase-only names must not resolve via kernel32 export table */
    HMODULE k = t_kernel32();
    for (size_t i = 0; i < NEX24; i++) {
        if (ex24[i].k32 || !ex24[i].kb) continue;
        DWORD o; const char *f;
        if (find_export(k, ex24[i].n, &o, &f)) t_note("%s present in kernel32 too", ex24[i].n);
    }
    CHECK(1);
}

/* ---------- IsChar*W ---------- */
UDECL(BOOL, IsCharBlankW, (WCHAR));
UDECL(BOOL, IsCharCntrlW, (WCHAR));
UDECL(BOOL, IsCharDigitW, (WCHAR));
UDECL(BOOL, IsCharPunctW, (WCHAR));
UDECL(BOOL, IsCharXDigitW, (WCHAR));

TEST(gap24_ischar_blank_ascii) {
    IsCharBlankW_pfn f = PB(IsCharBlankW); REQUIRE(f);
    for (int c = 0; c < 256; c++) {
        int want = (c == 9 || c == 0x20 || c == 0xa0);
        CHECKF((f((WCHAR)c) != 0) == want, "IsCharBlankW(0x%x)", c);
    }
    CHECK(f(0x3000)); CHECK(f(0xFEFF));
    CHECK(!f(0x0a)); CHECK(!f(0x0d)); CHECK(!f(0x0b)); CHECK(!f(0x1680 - 1));
    CHECK(!f(0x7028));
    CHECK(!f(0x3001)); CHECK(!f(0xFFFF)); CHECK(!f(0xFEFE));
}
TEST(gap24_ischar_cntrl) {
    IsCharCntrlW_pfn f = PB(IsCharCntrlW); REQUIRE(f);
    for (int c = 0; c < 256; c++) {
        int want = (c <= 0x1f) || (c >= 0x7f && c <= 0x9f);
        CHECKF((f((WCHAR)c) != 0) == want, "IsCharCntrlW(0x%x)", c);
    }
    CHECK(!f(0x100)); CHECK(!f(0x3000)); CHECK(!f(0xFFFF)); CHECK(!f(0x7028));
}
TEST(gap24_ischar_digit_ascii) {
    IsCharDigitW_pfn f = PB(IsCharDigitW); REQUIRE(f);
    for (int c = 0; c < 128; c++) CHECKF((f((WCHAR)c) != 0) == (c >= '0' && c <= '9'), "IsCharDigitW(0x%x)", c);
    CHECK(!f(0xFFFF));
}
TEST(gap24_ischar_xdigit_ascii) {
    IsCharXDigitW_pfn f = PB(IsCharXDigitW); REQUIRE(f);
    for (int c = 0; c < 256; c++) {
        int want = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        CHECKF((f((WCHAR)c) != 0) == want, "IsCharXDigitW(0x%x)", c);
    }
    CHECK(!f(0x0100)); CHECK(!f(0x4130)); CHECK(!f(0xFFFF)); CHECK(!f(0xFF00));
    /* fullwidth forms: record only */
    t_note("xdigit(U+FF10)=%d U+FF21=%d U+FF41=%d U+FF47=%d", f(0xFF10) != 0, f(0xFF21) != 0, f(0xFF41) != 0, f(0xFF47) != 0);
}
TEST(gap24_ischar_punct_ascii) {
    IsCharPunctW_pfn f = PB(IsCharPunctW); REQUIRE(f);
    const char *sure = "!\"#%&'()*,-./:;?@[\\]_{}";
    for (const char *p = sure; *p; p++) CHECKF(f((WCHAR)*p) != 0, "IsCharPunctW('%c')", *p);
    for (int c = 0; c < 128; c++) {
        if (strchr(sure, c) && c) continue;
        if (strchr("$+<=>^`|~", c) && c) { t_note("IsCharPunctW('%c')=%d", c, f((WCHAR)c) != 0); continue; }
        CHECKF(!f((WCHAR)c), "IsCharPunctW(0x%x) must be false", c);
    }
    CHECK(!f(0xFFFF));
}
TEST(gap24_ischar_class_exclusive) {
    IsCharBlankW_pfn bl = PB(IsCharBlankW); IsCharCntrlW_pfn cn = PB(IsCharCntrlW);
    IsCharDigitW_pfn dg = PB(IsCharDigitW); IsCharPunctW_pfn pu = PB(IsCharPunctW); IsCharXDigitW_pfn xd = PB(IsCharXDigitW);
    REQUIRE(bl); REQUIRE(cn); REQUIRE(dg); REQUIRE(pu); REQUIRE(xd);
    for (int c = 0; c < 128; c++) {
        CHECKF(!(dg((WCHAR)c) && pu((WCHAR)c)), "digit&punct 0x%x", c);
        CHECKF(!(cn((WCHAR)c) && pu((WCHAR)c)), "cntrl&punct 0x%x", c);
        if (dg((WCHAR)c)) CHECKF(xd((WCHAR)c), "digit implies xdigit 0x%x", c);
        if (bl((WCHAR)c) && c != 9) CHECKF(!xd((WCHAR)c), "blank not xdigit 0x%x", c);
    }
}
TEST(gap24_ischar_full_range_boolean) {
    IsCharBlankW_pfn bl = PB(IsCharBlankW); IsCharCntrlW_pfn cn = PB(IsCharCntrlW);
    IsCharDigitW_pfn dg = PB(IsCharDigitW); IsCharPunctW_pfn pu = PB(IsCharPunctW); IsCharXDigitW_pfn xd = PB(IsCharXDigitW);
    REQUIRE(bl); REQUIRE(cn); REQUIRE(dg); REQUIRE(pu); REQUIRE(xd);
    int bad = 0;
    for (unsigned c = 0; c < 0x10000; c++) {
        UINT r1 = (UINT)bl((WCHAR)c), r2 = (UINT)cn((WCHAR)c), r3 = (UINT)dg((WCHAR)c), r4 = (UINT)pu((WCHAR)c), r5 = (UINT)xd((WCHAR)c);
        if (r1 > 1 || r2 > 1 || r3 > 1 || r4 > 1 || r5 > 1) bad++;
    }
    CHECKF(bad == 0, "%d non-boolean results", bad);
}
TEST(gap24_ischar_parity_with_getstringtypew) {
    IsCharDigitW_pfn dg = PB(IsCharDigitW); IsCharXDigitW_pfn xd = PB(IsCharXDigitW);
    IsCharCntrlW_pfn cn = PB(IsCharCntrlW); REQUIRE(dg); REQUIRE(xd); REQUIRE(cn);
    int dd = 0, xx = 0, cc = 0;
    for (WCHAR c = 0; c < 0x100; c++) {
        WORD t = 0;
        if (!GetStringTypeW(CT_CTYPE1, &c, 1, &t)) continue;
        if (((t & C1_DIGIT) != 0) != (dg(c) != 0)) dd++;
        if (((t & C1_XDIGIT) != 0) != (xd(c) != 0)) xx++;
        if (((t & C1_CNTRL) != 0) != (cn(c) != 0)) cc++;
    }
    t_note("Latin-1 mismatches vs GetStringTypeW: digit %d xdigit %d cntrl %d", dd, xx, cc);
    CHECK(1);
}

/* ---------- KernelBaseGetGlobalData ---------- */
UDECL(void *, KernelBaseGetGlobalData, (void));
TEST(gap24_kernelbase_global_data) {
    KernelBaseGetGlobalData_pfn f = PB(KernelBaseGetGlobalData); REQUIRE(f);
    void *p = f(); CHECK(p != NULL);
    for (int i = 0; i < 10; i++) CHECK(f() == p);
    BYTE *b = (BYTE *)t_kernelbase();
    IMAGE_NT_HEADERS64 *nt = (IMAGE_NT_HEADERS64 *)(b + ((IMAGE_DOS_HEADER *)b)->e_lfanew);
    if (!t_is_wine()) {
        CHECK((BYTE *)p >= b && (BYTE *)p < b + nt->OptionalHeader.SizeOfImage);
        CHECK(((ULONG_PTR)p & 3) == 0);
    }
    volatile DWORD d = *(volatile DWORD *)p; (void)d;   /* readable */
}

/* ---------- IsTimeZoneRedirectionEnabled / Nls cache ---------- */
UDECL(BOOLEAN, IsTimeZoneRedirectionEnabled, (void));
TEST(gap24_timezone_redirection_stable) {
    IsTimeZoneRedirectionEnabled_pfn f = PB(IsTimeZoneRedirectionEnabled); REQUIRE(f);
    if (t_is_wine()) { t_skip("IsTimeZoneRedirectionEnabled is a wine stub"); return; }
    BOOLEAN a = f();
    CHECK(a == 0 || a == 1);
    for (int i = 0; i < 20; i++) CHECK(f() == a);
    t_note("IsTimeZoneRedirectionEnabled=%d", a);
}
UDECL(DWORD, NlsGetCacheUpdateCount, (void));
TEST(gap24_nls_cache_update_count) {
    NlsGetCacheUpdateCount_pfn f = PB(NlsGetCacheUpdateCount); REQUIRE(f);
    if (t_is_wine()) { t_skip("NlsGetCacheUpdateCount is a wine stub"); return; }
    DWORD a = f(), b = f();
    CHECK(b >= a);
    t_note("NlsGetCacheUpdateCount=%lu", a);
    if (PK(NlsGetCacheUpdateCount)) CHECK(PK(NlsGetCacheUpdateCount)() >= a);
}
UDECL(DWORD, NlsUpdateLocale, (const void *, DWORD));
TEST(gap24_nls_update_locale_error_paths) {
    NlsUpdateLocale_pfn f = PB(NlsUpdateLocale); REQUIRE(f);
    if (t_is_wine()) { t_skip("NlsUpdateLocale is a wine stub"); return; }
    WCHAR nm[] = L"en-US";
    EXPECT_EQ(f(NULL, 1), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(f(NULL, 0), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(f(NULL, 0xFFFFFFFF), ERROR_INVALID_PARAMETER);
    DWORD bad[] = {0, 8, 0x10, 0x80, 0x100, 0x80000000, 0xFFFFFFF8, 0xFFFFFFFF};
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++)
        CHECKF(f(nm, bad[i]) == ERROR_INVALID_FLAGS, "NlsUpdateLocale flags 0x%lx", bad[i]);
}
UDECL(DWORD, NlsUpdateSystemLocale, (LPCWSTR, int));
TEST(gap24_nls_update_system_locale_error_paths) {
    NlsUpdateSystemLocale_pfn f = PB(NlsUpdateSystemLocale); REQUIRE(f);
    if (t_is_wine()) { t_skip("NlsUpdateSystemLocale is a wine stub"); return; }
    int bad[] = {0, 2, 3, 4, 7, 9, 16, -1, 0x100, 0x7fffffff};
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++)
        CHECKF(f(L"en-US", bad[i]) == ERROR_INVALID_FLAGS, "NlsUpdateSystemLocale flag %d", bad[i]);
    /* bad flag is diagnosed before the locale name is examined */
    CHECK(f(L"", 0) == ERROR_INVALID_FLAGS);
}
UDECL(void *, NlsGetACPFromLocale, (LCID, DWORD));
TEST(gap24_nls_get_acp_from_locale) {
    NlsGetACPFromLocale_pfn f = PB(NlsGetACPFromLocale); REQUIRE(f);
    if (t_is_wine()) { t_skip("NlsGetACPFromLocale is a wine stub"); return; }
    /* flags 0x20000000/0x40000000 bypass lookup: both return the same global node */
    void *a = f(0x409, 0x40000000), *b = f(0x409, 0x20000000), *c = f(0x407, 0x60000000);
    CHECK(a == b); CHECK(b == c);
    void *d = f(0x409, 0); void *e = f(0x409, 0);
    CHECK(d == e);
    t_note("ACP node en-US=%p global=%p", d, a);
    /* LCID 0x800 skips the lookup entirely and yields the global node */
    CHECK(f(0x800, 0) == a);
    SetLastError(0);
    void *z = f(0xFFFFFFFF, 0);
    t_note("ACP node for invalid LCID = %p err %lu", z, GetLastError());
}
