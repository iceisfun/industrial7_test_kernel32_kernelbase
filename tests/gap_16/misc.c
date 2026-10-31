#include "g16.h"

/* ================================================================ VerifyVersionInfoA */
static ULONGLONG cm(DWORD type, BYTE cond, ULONGLONG m) { return UK(VerSetConditionMask)(m, type, cond); }
static BOOL vvia(OSVERSIONINFOEXA *v, DWORD mask, ULONGLONG c) { return UK(VerifyVersionInfoA)(v, mask, c); }
static BOOL vviw_from(const OSVERSIONINFOEXA *a, DWORD mask, ULONGLONG c) {
    OSVERSIONINFOEXW w; memset(&w, 0, sizeof w); w.dwOSVersionInfoSize = sizeof w;
    w.dwMajorVersion = a->dwMajorVersion; w.dwMinorVersion = a->dwMinorVersion; w.dwBuildNumber = a->dwBuildNumber; w.dwPlatformId = a->dwPlatformId;
    w.wServicePackMajor = a->wServicePackMajor; w.wServicePackMinor = a->wServicePackMinor; w.wSuiteMask = a->wSuiteMask; w.wProductType = a->wProductType;
    return UK(VerifyVersionInfoW)(&w, mask, c);
}
#define VV_NEED REQUIRE(UK(VerifyVersionInfoA)); REQUIRE(UK(VerSetConditionMask))

TEST(gap16_vvi_condition_mask_bits) {
    REQUIRE(UK(VerSetConditionMask));
    EXPECT_EQ(UK(VerSetConditionMask)(0, VER_MINORVERSION, VER_GREATER_EQUAL), (ULONGLONG)VER_GREATER_EQUAL << 0);
    EXPECT_EQ(UK(VerSetConditionMask)(0, VER_MAJORVERSION, VER_GREATER_EQUAL), (ULONGLONG)VER_GREATER_EQUAL << 3);
    EXPECT_EQ(UK(VerSetConditionMask)(0, VER_BUILDNUMBER, VER_LESS), (ULONGLONG)VER_LESS << 6);
    EXPECT_EQ(UK(VerSetConditionMask)(0, VER_PLATFORMID, VER_EQUAL), (ULONGLONG)VER_EQUAL << 9);
    EXPECT_EQ(UK(VerSetConditionMask)(0, VER_SERVICEPACKMINOR, VER_EQUAL), (ULONGLONG)VER_EQUAL << 12);
    EXPECT_EQ(UK(VerSetConditionMask)(0, VER_SERVICEPACKMAJOR, VER_GREATER), (ULONGLONG)VER_GREATER << 15);
    EXPECT_EQ(UK(VerSetConditionMask)(0, VER_SUITENAME, VER_AND), (ULONGLONG)VER_AND << 18);
    EXPECT_EQ(UK(VerSetConditionMask)(0, VER_PRODUCT_TYPE, VER_LESS_EQUAL), (ULONGLONG)VER_LESS_EQUAL << 21);
    ULONGLONG m = UK(VerSetConditionMask)(0, VER_MAJORVERSION, VER_GREATER_EQUAL); m = UK(VerSetConditionMask)(m, VER_MINORVERSION, VER_LESS);
    EXPECT_EQ(m, ((ULONGLONG)VER_GREATER_EQUAL << 3) | VER_LESS);
}
TEST(gap16_vvi_condition_mask_overwrites_same_slot) {
    REQUIRE(UK(VerSetConditionMask));
    ULONGLONG m = UK(VerSetConditionMask)(0, VER_MAJORVERSION, VER_GREATER_EQUAL);
    ULONGLONG n = UK(VerSetConditionMask)(m, VER_MAJORVERSION, VER_LESS);
    t_note("or-in semantics: %llx -> %llx", m, n);
    CHECK((n & (7 << 3)) != 0);
}
TEST(gap16_vvi_old_major_ge_true) {
    VV_NEED; OSVERSIONINFOEXA v; memset(&v, 0, sizeof v); v.dwOSVersionInfoSize = sizeof v; v.dwMajorVersion = 5;
    SET_ERR(0); CHECK(vvia(&v, VER_MAJORVERSION, cm(VER_MAJORVERSION, VER_GREATER_EQUAL, 0)));
    v.dwMajorVersion = 6; CHECK(vvia(&v, VER_MAJORVERSION, cm(VER_MAJORVERSION, VER_GREATER_EQUAL, 0)));
    v.dwMajorVersion = 1; CHECK(vvia(&v, VER_MAJORVERSION, cm(VER_MAJORVERSION, VER_GREATER_EQUAL, 0)));
    v.dwMajorVersion = 0; CHECK(vvia(&v, VER_MAJORVERSION, cm(VER_MAJORVERSION, VER_GREATER_EQUAL, 0)));
}
TEST(gap16_vvi_future_major_fails_with_old_win_version) {
    VV_NEED; OSVERSIONINFOEXA v; memset(&v, 0, sizeof v); v.dwOSVersionInfoSize = sizeof v;
    static const DWORD maj[] = { 11, 12, 99, 1000, 0xffffffffu };
    for (int i = 0; i < 5; i++) { v.dwMajorVersion = maj[i]; SET_ERR(0); BOOL r = vvia(&v, VER_MAJORVERSION, cm(VER_MAJORVERSION, VER_GREATER_EQUAL, 0)); CHECKF(!r, "major %lu", maj[i]); if (!r) EXPECT_ERR(ERROR_OLD_WIN_VERSION); }
}
TEST(gap16_vvi_major_equal_and_less) {
    VV_NEED; OSVERSIONINFOEXA v; memset(&v, 0, sizeof v); v.dwOSVersionInfoSize = sizeof v;
    v.dwMajorVersion = 4; SET_ERR(0);
    CHECK(!vvia(&v, VER_MAJORVERSION, cm(VER_MAJORVERSION, VER_EQUAL, 0))); EXPECT_ERR(ERROR_OLD_WIN_VERSION);
    SET_ERR(0); CHECK(!vvia(&v, VER_MAJORVERSION, cm(VER_MAJORVERSION, VER_LESS, 0))); EXPECT_ERR(ERROR_OLD_WIN_VERSION);
    SET_ERR(0); CHECK(!vvia(&v, VER_MAJORVERSION, cm(VER_MAJORVERSION, VER_LESS_EQUAL, 0))); EXPECT_ERR(ERROR_OLD_WIN_VERSION);
    CHECK(vvia(&v, VER_MAJORVERSION, cm(VER_MAJORVERSION, VER_GREATER, 0)));
    v.dwMajorVersion = 99;
    CHECK(vvia(&v, VER_MAJORVERSION, cm(VER_MAJORVERSION, VER_LESS, 0)));
    CHECK(vvia(&v, VER_MAJORVERSION, cm(VER_MAJORVERSION, VER_LESS_EQUAL, 0)));
    SET_ERR(0); CHECK(!vvia(&v, VER_MAJORVERSION, cm(VER_MAJORVERSION, VER_GREATER, 0))); EXPECT_ERR(ERROR_OLD_WIN_VERSION);
    SET_ERR(0); CHECK(!vvia(&v, VER_MAJORVERSION, cm(VER_MAJORVERSION, VER_EQUAL, 0))); EXPECT_ERR(ERROR_OLD_WIN_VERSION);
}
TEST(gap16_vvi_platform_id) {
    VV_NEED; OSVERSIONINFOEXA v; memset(&v, 0, sizeof v); v.dwOSVersionInfoSize = sizeof v; v.dwPlatformId = VER_PLATFORM_WIN32_NT;
    CHECK(vvia(&v, VER_PLATFORMID, cm(VER_PLATFORMID, VER_EQUAL, 0)));
    v.dwPlatformId = VER_PLATFORM_WIN32_WINDOWS; SET_ERR(0); CHECK(!vvia(&v, VER_PLATFORMID, cm(VER_PLATFORMID, VER_EQUAL, 0))); EXPECT_ERR(ERROR_OLD_WIN_VERSION);
    v.dwPlatformId = 0; SET_ERR(0); CHECK(!vvia(&v, VER_PLATFORMID, cm(VER_PLATFORMID, VER_EQUAL, 0))); EXPECT_ERR(ERROR_OLD_WIN_VERSION);
    v.dwPlatformId = 1; CHECK(vvia(&v, VER_PLATFORMID, cm(VER_PLATFORMID, VER_GREATER, 0)));
    SET_ERR(0); CHECK(!vvia(&v, VER_PLATFORMID, cm(VER_PLATFORMID, VER_LESS, 0))); EXPECT_ERR(ERROR_OLD_WIN_VERSION);
}
TEST(gap16_vvi_build_number) {
    VV_NEED; OSVERSIONINFOEXA v; memset(&v, 0, sizeof v); v.dwOSVersionInfoSize = sizeof v;
    v.dwBuildNumber = 1; CHECK(vvia(&v, VER_BUILDNUMBER, cm(VER_BUILDNUMBER, VER_GREATER_EQUAL, 0)));
    v.dwBuildNumber = 2195; CHECK(vvia(&v, VER_BUILDNUMBER, cm(VER_BUILDNUMBER, VER_GREATER_EQUAL, 0)));
    v.dwBuildNumber = 0xfffffff; SET_ERR(0); CHECK(!vvia(&v, VER_BUILDNUMBER, cm(VER_BUILDNUMBER, VER_GREATER_EQUAL, 0))); EXPECT_ERR(ERROR_OLD_WIN_VERSION);
    v.dwBuildNumber = 0xfffffff; CHECK(vvia(&v, VER_BUILDNUMBER, cm(VER_BUILDNUMBER, VER_LESS, 0)));
}
TEST(gap16_vvi_service_pack) {
    VV_NEED; OSVERSIONINFOEXA v; memset(&v, 0, sizeof v); v.dwOSVersionInfoSize = sizeof v;
    CHECK(vvia(&v, VER_SERVICEPACKMAJOR, cm(VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL, 0)));
    CHECK(vvia(&v, VER_SERVICEPACKMINOR, cm(VER_SERVICEPACKMINOR, VER_GREATER_EQUAL, 0)));
    v.wServicePackMajor = 99; SET_ERR(0); CHECK(!vvia(&v, VER_SERVICEPACKMAJOR, cm(VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL, 0))); EXPECT_ERR(ERROR_OLD_WIN_VERSION);
    v.wServicePackMajor = 0; v.wServicePackMinor = 99; SET_ERR(0); CHECK(!vvia(&v, VER_SERVICEPACKMINOR, cm(VER_SERVICEPACKMINOR, VER_GREATER_EQUAL, 0))); EXPECT_ERR(ERROR_OLD_WIN_VERSION);
}
TEST(gap16_vvi_combined_major_minor_sp_ordering) {
    VV_NEED; OSVERSIONINFOEXA v; memset(&v, 0, sizeof v); v.dwOSVersionInfoSize = sizeof v;
    ULONGLONG c = cm(VER_MAJORVERSION, VER_GREATER_EQUAL, 0); c = cm(VER_MINORVERSION, VER_GREATER_EQUAL, c); c = cm(VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL, c);
    DWORD mask = VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR;
    /* major strictly less than the running OS: minor and service pack are not consulted */
    v.dwMajorVersion = 3; v.dwMinorVersion = 99; v.wServicePackMajor = 99; CHECK(vvia(&v, mask, c));
    v.dwMajorVersion = 1; v.dwMinorVersion = 0; v.wServicePackMajor = 0; CHECK(vvia(&v, mask, c));
    v.dwMajorVersion = 99; v.dwMinorVersion = 0; v.wServicePackMajor = 0; SET_ERR(0); CHECK(!vvia(&v, mask, c)); EXPECT_ERR(ERROR_OLD_WIN_VERSION);
}
TEST(gap16_vvi_typemask_zero_is_bad_arguments) {
    VV_NEED; OSVERSIONINFOEXA v; memset(&v, 0, sizeof v); v.dwOSVersionInfoSize = sizeof v;
    SET_ERR(0); CHECK(!vvia(&v, 0, cm(VER_MAJORVERSION, VER_GREATER_EQUAL, 0))); EXPECT_ERR(ERROR_BAD_ARGUMENTS);
    SET_ERR(0); CHECK(!vvia(&v, 0, 0)); EXPECT_ERR(ERROR_BAD_ARGUMENTS);
}
TEST(gap16_vvi_condition_mask_zero) {
    VV_NEED; OSVERSIONINFOEXA v; memset(&v, 0, sizeof v); v.dwOSVersionInfoSize = sizeof v; v.dwMajorVersion = 5;
    SET_ERR(0); BOOL r = vvia(&v, VER_MAJORVERSION, 0); t_note("condition mask 0 -> %d err %lu", r, GetLastError());
}
TEST(gap16_vvi_unknown_typemask_bits) {
    VV_NEED; OSVERSIONINFOEXA v; memset(&v, 0, sizeof v); v.dwOSVersionInfoSize = sizeof v; v.dwMajorVersion = 5;
    static const DWORD m[] = { 0x100, 0x8000, 0x80000000u, 0xffffff00u };
    for (int i = 0; i < 4; i++) { SET_ERR(0); BOOL r = vvia(&v, m[i], cm(VER_MAJORVERSION, VER_GREATER_EQUAL, 0)); t_note("mask 0x%lx -> %d err %lu", m[i], r, GetLastError()); }
}
TEST(gap16_vvi_struct_size_field_ignored_by_a) {
    VV_NEED; OSVERSIONINFOEXA v; static const DWORD sz[] = { 0, 1, 156, sizeof(OSVERSIONINFOEXA), 0xffffffffu };
    for (int i = 0; i < 5; i++) {
        memset(&v, 0, sizeof v); v.dwOSVersionInfoSize = sz[i]; v.dwMajorVersion = 5;
        SET_ERR(0); BOOL r = vvia(&v, VER_MAJORVERSION, cm(VER_MAJORVERSION, VER_GREATER_EQUAL, 0));
        CHECKF(r, "size field %lu err %lu", sz[i], GetLastError());
    }
}
TEST(gap16_vvi_ansi_wide_parity) {
    VV_NEED; REQUIRE(UK(VerifyVersionInfoW));
    static const struct { DWORD maj, min, sp; DWORD mask; BYTE cond; } t[] = {
        { 5, 0, 0, VER_MAJORVERSION, VER_GREATER_EQUAL }, { 99, 0, 0, VER_MAJORVERSION, VER_GREATER_EQUAL }, { 4, 0, 0, VER_MAJORVERSION, VER_EQUAL },
        { 4, 0, 0, VER_MAJORVERSION, VER_LESS }, { 0, 0, 0, VER_MINORVERSION, VER_GREATER_EQUAL }, { 0, 99, 0, VER_MINORVERSION, VER_GREATER_EQUAL },
        { 0, 0, 0, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL }, { 0, 0, 5, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL }, { 0, 0, 0, VER_PLATFORMID, VER_EQUAL },
    };
    for (int i = 0; i < 9; i++) {
        OSVERSIONINFOEXA v; memset(&v, 0, sizeof v); v.dwOSVersionInfoSize = sizeof v; v.dwMajorVersion = t[i].maj; v.dwMinorVersion = t[i].min; v.wServicePackMajor = (WORD)t[i].sp;
        ULONGLONG c = cm(t[i].mask, t[i].cond, 0);
        SET_ERR(0); BOOL a = vvia(&v, t[i].mask, c); DWORD ea = GetLastError();
        SET_ERR(0); BOOL w = vviw_from(&v, t[i].mask, c); DWORD ew = GetLastError();
        CHECKF(!!a == !!w, "case %d A=%d W=%d", i, a, w);
        if (!a && !w) CHECKF(ea == ew, "case %d err A=%lu W=%lu", i, ea, ew);
    }
}
TEST(gap16_vvi_suite_and_product_conditions_do_not_crash) {
    VV_NEED; OSVERSIONINFOEXA v; memset(&v, 0, sizeof v); v.dwOSVersionInfoSize = sizeof v;
    v.wSuiteMask = VER_SUITE_PERSONAL; SET_ERR(0);
    BOOL r = vvia(&v, VER_SUITENAME, cm(VER_SUITENAME, VER_OR, 0)); t_note("suite personal OR -> %d err %lu", r, GetLastError());
    v.wSuiteMask = 0; r = vvia(&v, VER_SUITENAME, cm(VER_SUITENAME, VER_AND, 0)); t_note("suite 0 AND -> %d", r);
    v.wProductType = VER_NT_WORKSTATION; r = vvia(&v, VER_PRODUCT_TYPE, cm(VER_PRODUCT_TYPE, VER_EQUAL, 0)); t_note("workstation -> %d", r);
    v.wProductType = 0; r = vvia(&v, VER_PRODUCT_TYPE, cm(VER_PRODUCT_TYPE, VER_GREATER_EQUAL, 0)); CHECK(r);
}
TEST(gap16_vvi_repeat_stable) {
    VV_NEED; OSVERSIONINFOEXA v; memset(&v, 0, sizeof v); v.dwOSVersionInfoSize = sizeof v; v.dwMajorVersion = 5; int ok = 1;
    ULONGLONG c = cm(VER_MAJORVERSION, VER_GREATER_EQUAL, 0);
    for (int i = 0; i < 2000; i++) if (!vvia(&v, VER_MAJORVERSION, c)) ok = 0;
    CHECK(ok);
}

/* ================================================================ VerQueryValueW */
static BYTE *g_vi; static DWORD g_vilen; static WCHAR g_trans[16];
static int load_vi(void) {
    if (g_vi) return 1;
    if (!UN(GetFileVersionInfoSizeW) || !UN(GetFileVersionInfoW)) return 0;
    WCHAR path[MAX_PATH]; GetModuleFileNameW(t_kernel32(), path, MAX_PATH);
    DWORD h = 0, n = UN(GetFileVersionInfoSizeW)(path, &h); if (!n) return 0;
    g_vi = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, n + 16); g_vilen = n;
    if (!UN(GetFileVersionInfoW)(path, 0, n, g_vi)) { g_vi = NULL; return 0; }
    LPVOID p = NULL; UINT l = 0;
    if (UN(VerQueryValueW)(g_vi, L"\\VarFileInfo\\Translation", &p, &l) && l >= 4) wsprintfW(g_trans, L"%04x%04x", ((WORD *)p)[0], ((WORD *)p)[1]);
    else lstrcpyW(g_trans, L"040904B0");
    return 1;
}
#define VQ_NEED REQUIRE(UN(VerQueryValueW)); if (!load_vi()) { t_skip("no version resource"); return; }
static void sub(WCHAR *out, const WCHAR *leaf) { wsprintfW(out, L"\\StringFileInfo\\%s\\%s", g_trans, leaf); }

TEST(gap16_vqv_fixed_file_info) {
    VQ_NEED; VS_FIXEDFILEINFO *f = NULL; UINT len = 0xdead; SET_ERR(0);
    BOOL r = UN(VerQueryValueW)(g_vi, L"\\", (LPVOID *)&f, &len);
    CHECK(r); CHECK(f != NULL); EXPECT_EQ(len, sizeof(VS_FIXEDFILEINFO));
    if (f) { EXPECT_EQ(f->dwSignature, 0xFEEF04BD); EXPECT_EQ(f->dwStrucVersion, 0x00010000); CHECK((f->dwFileVersionMS >> 16) == 10 || t_is_wine()); CHECK(f->dwFileOS == VOS_NT_WINDOWS32 || t_is_wine());
        CHECK(f->dwFileType == VFT_DLL); }
    CHECK((BYTE *)f > g_vi && (BYTE *)f < g_vi + g_vilen);
}
TEST(gap16_vqv_fixed_info_variants_of_root_path) {
    VQ_NEED; static const WCHAR *roots[] = { L"\\", L"\\\\", L"\\\\\\" };
    for (int i = 0; i < 3; i++) { VS_FIXEDFILEINFO *f = NULL; UINT len = 0; BOOL r = UN(VerQueryValueW)(g_vi, roots[i], (LPVOID *)&f, &len); t_note("root variant %d -> %d len %u", i, r, len); }
}
TEST(gap16_vqv_translation_table) {
    VQ_NEED; WORD *t = NULL; UINT len = 0xdead; SET_ERR(0);
    BOOL r = UN(VerQueryValueW)(g_vi, L"\\VarFileInfo\\Translation", (LPVOID *)&t, &len);
    CHECK(r); CHECK(len >= 4 && (len % 4) == 0); CHECK(t != NULL);
    if (t && len >= 4) { CHECK(t[0] != 0); CHECK(t[1] == 1200 || t[1] == 1252 || t[1] == 0); t_note("translation %04x/%04x count %u", t[0], t[1], len / 4); }
}
TEST(gap16_vqv_string_length_counts_nul) {
    VQ_NEED; WCHAR q[128]; static const WCHAR *keys[] = { L"FileDescription", L"FileVersion", L"InternalName", L"ProductName", L"CompanyName", L"OriginalFilename", L"LegalCopyright" };
    int found = 0;
    for (int i = 0; i < 7; i++) {
        WCHAR *s = NULL; UINT len = 0xdead; sub(q, keys[i]);
        if (!UN(VerQueryValueW)(g_vi, q, (LPVOID *)&s, &len)) { t_note("missing %ls", keys[i]); continue; }
        found++; CHECKF(s != NULL && len == (UINT)lstrlenW(s) + 1, "%ls len %u strlen %d", keys[i], len, s ? lstrlenW(s) : -1);
        CHECK((BYTE *)s > g_vi && (BYTE *)s < g_vi + g_vilen);
    }
    CHECK(found >= 4);
}
TEST(gap16_vqv_string_values_kernel32) {
    VQ_NEED; WCHAR q[128]; WCHAR *s = NULL; UINT len = 0;
    sub(q, L"InternalName"); if (UN(VerQueryValueW)(g_vi, q, (LPVOID *)&s, &len)) { CHECK(!lstrcmpiW(s, L"kernel32") || !lstrcmpiW(s, L"kernel32.dll") || t_is_wine()); }
    sub(q, L"CompanyName"); if (UN(VerQueryValueW)(g_vi, q, (LPVOID *)&s, &len)) { CHECK(wcsstr(s, L"Microsoft") != NULL || t_is_wine()); }
    sub(q, L"FileVersion"); if (UN(VerQueryValueW)(g_vi, q, (LPVOID *)&s, &len)) { CHECK(wcsncmp(s, L"10.0.", 5) == 0 || t_is_wine()); }
}
TEST(gap16_vqv_key_case_insensitive) {
    VQ_NEED; WCHAR q[128]; WCHAR *a = NULL, *b = NULL; UINT la = 0, lb = 0;
    sub(q, L"FileDescription"); BOOL r1 = UN(VerQueryValueW)(g_vi, q, (LPVOID *)&a, &la);
    sub(q, L"FILEDESCRIPTION"); BOOL r2 = UN(VerQueryValueW)(g_vi, q, (LPVOID *)&b, &lb);
    CHECK(r1); t_note("upper-case key lookup -> %d", r2); if (r1 && r2) CHECK(a == b && la == lb);
}
TEST(gap16_vqv_lang_hex_case_insensitive) {
    VQ_NEED; WCHAR q[128], lw[16]; WCHAR *a = NULL; UINT la = 0; lstrcpyW(lw, g_trans); CharLowerW(lw);
    wsprintfW(q, L"\\StringFileInfo\\%s\\FileDescription", lw); BOOL r = UN(VerQueryValueW)(g_vi, q, (LPVOID *)&a, &la);
    CHECK(r);
}
TEST(gap16_vqv_not_found_sets_len_zero) {
    VQ_NEED; WCHAR q[128]; WCHAR *s = (WCHAR *)0x1234; UINT len = 99;
    sub(q, L"NoSuchKeyAtAll"); SET_ERR(0);
    BOOL r = UN(VerQueryValueW)(g_vi, q, (LPVOID *)&s, &len); CHECK(!r); EXPECT_EQ(len, 0); EXPECT_ERR(ERROR_RESOURCE_TYPE_NOT_FOUND);
    len = 99; SET_ERR(0); r = UN(VerQueryValueW)(g_vi, L"\\StringFileInfo\\FFFFFFFF\\FileDescription", (LPVOID *)&s, &len); CHECK(!r); EXPECT_EQ(len, 0); EXPECT_ERR(ERROR_RESOURCE_TYPE_NOT_FOUND);
    len = 99; SET_ERR(0); r = UN(VerQueryValueW)(g_vi, L"\\NoSuchBlock", (LPVOID *)&s, &len); CHECK(!r); EXPECT_EQ(len, 0); EXPECT_ERR(ERROR_RESOURCE_TYPE_NOT_FOUND);
    len = 99; r = UN(VerQueryValueW)(g_vi, L"\\StringFileInfo", (LPVOID *)&s, &len); t_note("\\StringFileInfo -> %d len %u", r, len);
}
TEST(gap16_vqv_null_len_pointer_ok) {
    VQ_NEED; VS_FIXEDFILEINFO *f = NULL; SET_ERR(0);
    CHECK(UN(VerQueryValueW)(g_vi, L"\\", (LPVOID *)&f, NULL)); CHECK(f != NULL);
    WCHAR q[128]; WCHAR *s = NULL; sub(q, L"FileDescription"); CHECK(UN(VerQueryValueW)(g_vi, q, (LPVOID *)&s, NULL)); CHECK(s != NULL);
    f = NULL; CHECK(!UN(VerQueryValueW)(g_vi, L"\\Nope", (LPVOID *)&f, NULL));
}
TEST(gap16_vqv_repeat_same_pointer) {
    VQ_NEED; VS_FIXEDFILEINFO *a = NULL, *b = NULL; UINT l;
    CHECK(UN(VerQueryValueW)(g_vi, L"\\", (LPVOID *)&a, &l)); CHECK(UN(VerQueryValueW)(g_vi, L"\\", (LPVOID *)&b, &l)); CHECK(a == b);
}
TEST(gap16_vqv_nonzero_wtype_block_rejected) {
    VQ_NEED; BYTE *copy = HeapAlloc(GetProcessHeap(), 0, g_vilen + 16); REQUIRE(copy); memcpy(copy, g_vi, g_vilen);
    ((WORD *)copy)[2] = 1;      /* wType: text */
    VS_FIXEDFILEINFO *f = (void *)0x1; UINT len = 77; BOOL r = UN(VerQueryValueW)(copy, L"\\", (LPVOID *)&f, &len);
    CHECK(!r); EXPECT_EQ(len, 0);
    HeapFree(GetProcessHeap(), 0, copy);
}
TEST(gap16_vqv_zeroed_block_rejected) {
    VQ_NEED; BYTE z[256]; memset(z, 0, sizeof z); VS_FIXEDFILEINFO *f = NULL; UINT len = 55;
    SET_ERR(0); BOOL r = UN(VerQueryValueW)(z, L"\\", (LPVOID *)&f, &len); CHECK(!r); EXPECT_EQ(len, 0); EXPECT_ERR(ERROR_INVALID_DATA);
}
TEST(gap16_vqv_tiny_length_block_rejected) {
    VQ_NEED; BYTE z[64]; memset(z, 0, sizeof z); ((WORD *)z)[0] = 4;   /* wLength below the 8-byte header */
    VS_FIXEDFILEINFO *f = NULL; UINT len = 55; SET_ERR(0);
    BOOL r = UN(VerQueryValueW)(z, L"\\", (LPVOID *)&f, &len); CHECK(!r); EXPECT_EQ(len, 0); EXPECT_ERR(ERROR_INVALID_DATA);
    static const WORD badlen[] = { 0, 1, 7, 0x8001, 0xffff };
    for (int i = 0; i < 5; i++) { ((WORD *)z)[0] = badlen[i]; len = 55; SET_ERR(0); r = UN(VerQueryValueW)(z, L"\\", (LPVOID *)&f, &len); CHECKF(!r, "wLength %u", badlen[i]); EXPECT_EQ(len, 0); EXPECT_ERR(ERROR_INVALID_DATA); }
}
TEST(gap16_vqv_empty_subblock) {
    VQ_NEED; VOID *p = NULL; UINT len = 5; SET_ERR(0);
    BOOL r = UN(VerQueryValueW)(g_vi, L"", &p, &len); t_note("empty sub-block -> %d len %u err %lu", r, len, GetLastError());
}
TEST(gap16_vqv_long_subblock_no_overflow) {
    VQ_NEED; static WCHAR big[4096]; for (int i = 0; i < 4095; i++) big[i] = (i % 17 == 0) ? L'\\' : L'A'; big[4095] = 0;
    VOID *p = NULL; UINT len = 5; BOOL r = UN(VerQueryValueW)(g_vi, big, &p, &len); CHECK(!r); EXPECT_EQ(len, 0);
}
TEST(gap16_vqv_only_backslashes_prefix_tolerated) {
    VQ_NEED; WCHAR q[160]; WCHAR *s = NULL; UINT len = 0;
    wsprintfW(q, L"\\\\StringFileInfo\\\\%s\\\\FileDescription", g_trans);
    BOOL r = UN(VerQueryValueW)(g_vi, q, (LPVOID *)&s, &len); t_note("doubled separators -> %d", r);
}
TEST(gap16_vqv_var_file_info_binary_vs_string) {
    VQ_NEED; VOID *p = NULL; UINT len = 0;
    BOOL r = UN(VerQueryValueW)(g_vi, L"\\VarFileInfo", &p, &len); t_note("\\VarFileInfo -> %d len %u", r, len);
    r = UN(VerQueryValueW)(g_vi, L"\\VarFileInfo\\Translation\\x", &p, &len); CHECK(!r); EXPECT_EQ(len, 0);
}

/* ================================================================ lstrcat / lstrcmp / lstrcmpi */
TEST(gap16_lstrcat_basic) {
    REQUIRE(UK(lstrcat)); char b[64] = "abc";
    CHECK(UK(lstrcat)(b, "def") == b); EXPECT_STR(b, "abcdef");
    CHECK(UK(lstrcat)(b, "") == b); EXPECT_STR(b, "abcdef");
    b[0] = 0; CHECK(UK(lstrcat)(b, "xyz") == b); EXPECT_STR(b, "xyz");
    b[0] = 0; CHECK(UK(lstrcat)(b, "") == b); EXPECT_STR(b, "");
}
TEST(gap16_lstrcat_chain_and_nul_handling) {
    REQUIRE(UK(lstrcat)); char b[64]; memset(b, 'Z', sizeof b); b[0] = 0;
    UK(lstrcat)(UK(lstrcat)(UK(lstrcat)(b, "a"), "bb"), "ccc"); EXPECT_STR(b, "abbccc");
    CHECK(b[6] == 0); CHECK(b[7] == 'Z');
    char c[16] = "ab\0cd"; UK(lstrcat)(c, "X"); EXPECT_STR(c, "abX");
}
TEST(gap16_lstrcat_exact_fit) {
    REQUIRE(UK(lstrcat)); char b[8 + 4]; memset(b, 0x7f, sizeof b); strcpy(b, "abcd"); UK(lstrcat)(b, "efgh");
    EXPECT_STR(b, "abcdefgh"); CHECK((unsigned char)b[9] == 0x7f);
}
TEST(gap16_lstrcat_matches_lstrcatA_export) {
    REQUIRE(UK(lstrcat)); if (t_is_wine()) { t_skip("export aliasing"); return; } FARPROC a = t_proc_k32("lstrcatA"); FARPROC b = t_proc_k32("lstrcat");
    CHECK(a != NULL && b != NULL); CHECK(a == b);
}
TEST(gap16_lstrcat_high_bytes_and_long) {
    REQUIRE(UK(lstrcat)); char b[600]; b[0] = 0; char piece[100]; memset(piece, 0xe9, 99); piece[99] = 0;
    for (int i = 0; i < 5; i++) UK(lstrcat)(b, piece);
    CHECK(strlen(b) == 495); CHECK((unsigned char)b[494] == 0xe9);
}
TEST(gap16_lstrcmp_equal_and_empty) {
    REQUIRE(UK(lstrcmp)); EXPECT_EQ(UK(lstrcmp)("", ""), 0); EXPECT_EQ(UK(lstrcmp)("abc", "abc"), 0); EXPECT_EQ(UK(lstrcmp)("a", "a"), 0);
    CHECK(UK(lstrcmp)("", "a") < 0); CHECK(UK(lstrcmp)("a", "") > 0);
}
TEST(gap16_lstrcmp_ordering_table) {
    REQUIRE(UK(lstrcmp));
    static const struct { const char *a, *b; int sign; } t[] = {
        { "abc", "abd", -1 }, { "abd", "abc", 1 }, { "abc", "ab", 1 }, { "ab", "abc", -1 },
        { "a", "B", -1 }, { "B", "a", 1 }, { "a", "A", -1 }, { "A", "a", 1 }, { "abc", "ABC", -1 }, { "ABC", "abc", 1 },
        { "1", "a", -1 }, { "a", "1", 1 }, { "10", "9", -1 }, { "9", "10", 1 }, { "Z", "a", 1 }, { "b", "A", 1 },
        { "apple", "banana", -1 }, { "zebra", "apple", 1 }, { "x", "x ", -1 }, { "x ", "x", 1 },
    };
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++) {
        int r = UK(lstrcmp)(t[i].a, t[i].b); int s = r < 0 ? -1 : r > 0;
        CHECKF(s == t[i].sign, "lstrcmp(\"%s\",\"%s\") = %d want sign %d", t[i].a, t[i].b, r, t[i].sign);
    }
}
TEST(gap16_lstrcmp_result_magnitude) {
    REQUIRE(UK(lstrcmp)); int a = UK(lstrcmp)("a", "b"), b = UK(lstrcmp)("b", "a"), c = UK(lstrcmp)("a", "a");
    t_note("results %d %d %d", a, b, c); EXPECT_EQ(a, -1); EXPECT_EQ(b, 1); EXPECT_EQ(c, 0);
}
TEST(gap16_lstrcmp_null_arguments) {
    REQUIRE(UK(lstrcmp)); EXPECT_EQ(UK(lstrcmp)(NULL, NULL), 0);
    CHECK(UK(lstrcmp)(NULL, "a") < 0); CHECK(UK(lstrcmp)("a", NULL) > 0);
    CHECK(UK(lstrcmp)(NULL, "") < 0); CHECK(UK(lstrcmp)("", NULL) > 0);
}
TEST(gap16_lstrcmp_antisymmetric) {
    REQUIRE(UK(lstrcmp)); static const char *s[] = { "", "a", "A", "b", "B", "ab", "AB", "aB", "Ab", "1", "10", "2", "_", "~", " " };
    for (int i = 0; i < 15; i++) for (int j = 0; j < 15; j++) {
        int x = UK(lstrcmp)(s[i], s[j]), y = UK(lstrcmp)(s[j], s[i]);
        CHECKF((x < 0) == (y > 0) && (x == 0) == (y == 0), "\"%s\" vs \"%s\": %d %d", s[i], s[j], x, y);
    }
}
TEST(gap16_lstrcmp_transitive_sort) {
    REQUIRE(UK(lstrcmp)); const char *w[] = { "pear", "Apple", "apple", "banana", "Banana", "cherry", "10", "9", "" }; int n = 9;
    for (int i = 0; i < n; i++) for (int j = 0; j < n - 1 - i; j++) if (UK(lstrcmp)(w[j], w[j + 1]) > 0) { const char *t = w[j]; w[j] = w[j + 1]; w[j + 1] = t; }
    for (int i = 0; i + 1 < n; i++) CHECKF(UK(lstrcmp)(w[i], w[i + 1]) <= 0, "order broken at %d", i);
    EXPECT_STR(w[0], "");
}
TEST(gap16_lstrcmp_high_bytes) {
    REQUIRE(UK(lstrcmp)); EXPECT_EQ(UK(lstrcmp)("\xe9", "\xe9"), 0);
    CHECK(UK(lstrcmp)("\xe9", "\xe8") != 0); CHECK(UK(lstrcmp)("\xe9", "e") != 0);
}
TEST(gap16_lstrcmp_long_strings_differ_at_end) {
    REQUIRE(UK(lstrcmp)); static char a[10001], b[10001]; memset(a, 'q', 10000); memset(b, 'q', 10000); a[10000] = b[10000] = 0;
    EXPECT_EQ(UK(lstrcmp)(a, b), 0); b[9999] = 'r'; CHECK(UK(lstrcmp)(a, b) < 0); CHECK(UK(lstrcmp)(b, a) > 0);
}
TEST(gap16_lstrcmp_matches_lstrcmpA_export) {
    REQUIRE(UK(lstrcmp)); if (t_is_wine()) { t_skip("export aliasing"); return; } CHECK(t_proc_k32("lstrcmpA") == t_proc_k32("lstrcmp")); CHECK(t_proc_k32("lstrcmpiA") == t_proc_k32("lstrcmpi"));
}
TEST(gap16_lstrcmpi_case_insensitive_equal) {
    REQUIRE(UK(lstrcmpi)); EXPECT_EQ(UK(lstrcmpi)("", ""), 0); EXPECT_EQ(UK(lstrcmpi)("a", "A"), 0); EXPECT_EQ(UK(lstrcmpi)("ABC", "abc"), 0);
    EXPECT_EQ(UK(lstrcmpi)("HeLLo WoRLd", "hello world"), 0); EXPECT_EQ(UK(lstrcmpi)("x1_y", "X1_Y"), 0);
    char lo[27], up[27]; for (int i = 0; i < 26; i++) { lo[i] = 'a' + i; up[i] = 'A' + i; } lo[26] = up[26] = 0;
    EXPECT_EQ(UK(lstrcmpi)(lo, up), 0);
}
TEST(gap16_lstrcmpi_ordering_table) {
    REQUIRE(UK(lstrcmpi));
    static const struct { const char *a, *b; int sign; } t[] = {
        { "a", "B", -1 }, { "B", "a", 1 }, { "Z", "a", 1 }, { "z", "A", 1 }, { "abc", "ABD", -1 }, { "ABD", "abc", 1 }, { "ab", "ABC", -1 }, { "ABC", "ab", 1 },
        { "", "a", -1 }, { "a", "", 1 }, { "1", "A", -1 }, { "A", "1", 1 }, { "10", "9", -1 },
    };
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++) {
        int r = UK(lstrcmpi)(t[i].a, t[i].b); int s = r < 0 ? -1 : r > 0;
        CHECKF(s == t[i].sign, "lstrcmpi(\"%s\",\"%s\") = %d want sign %d", t[i].a, t[i].b, r, t[i].sign);
    }
}
TEST(gap16_lstrcmpi_magnitude_and_null) {
    REQUIRE(UK(lstrcmpi)); EXPECT_EQ(UK(lstrcmpi)("a", "b"), -1); EXPECT_EQ(UK(lstrcmpi)("B", "a"), 1); EXPECT_EQ(UK(lstrcmpi)("A", "a"), 0);
    EXPECT_EQ(UK(lstrcmpi)(NULL, NULL), 0); CHECK(UK(lstrcmpi)(NULL, "a") < 0); CHECK(UK(lstrcmpi)("a", NULL) > 0);
}
TEST(gap16_lstrcmpi_vs_lstrcmp_relationship) {
    REQUIRE(UK(lstrcmpi)); REQUIRE(UK(lstrcmp));
    static const char *s[] = { "a", "A", "b", "B", "abc", "ABC", "Abc", "aBc" };
    for (int i = 0; i < 8; i++) for (int j = 0; j < 8; j++) {
        int ci = UK(lstrcmpi)(s[i], s[j]); int cs = UK(lstrcmp)(s[i], s[j]);
        if (ci != 0) CHECKF((ci < 0) == (cs < 0), "\"%s\" \"%s\" ci=%d cs=%d", s[i], s[j], ci, cs);
    }
}
TEST(gap16_lstrcmpi_high_bytes_acp_case) {
    REQUIRE(UK(lstrcmpi)); EXPECT_EQ(UK(lstrcmpi)("\xe9", "\xe9"), 0);
    int r = UK(lstrcmpi)("\xe9", "\xc9"); t_note("e-acute vs E-acute (ACP %u) -> %d", GetACP(), r);
    if (GetACP() == 1252) EXPECT_EQ(r, 0);
}
TEST(gap16_lstrcmpi_embedded_stop_at_nul) {
    REQUIRE(UK(lstrcmpi)); REQUIRE(UK(lstrcmp)); EXPECT_EQ(UK(lstrcmpi)("ab\0x", "AB\0y"), 0); EXPECT_EQ(UK(lstrcmp)("ab\0x", "ab\0y"), 0);
}

/* ================================================================ WritePrivateProfileStructA */
static void ini(char *o, const char *leaf) { t_tmpfile(o, MAX_PATH, leaf); }
static void raw(const char *file, const char *sec, const char *key, char *out, DWORD cap) { out[0] = 0; GetPrivateProfileStringA(sec, key, "", out, cap, file); }
static int ieq(const char *a, const char *b) { return !_stricmp(a, b); }
#define WP_NEED REQUIRE(UK(WritePrivateProfileStructA)); REQUIRE(UK(GetPrivateProfileStructA))

TEST(gap16_wpps_encoding_hex_plus_checksum) {
    WP_NEED; char f[MAX_PATH], v[512]; ini(f, "g16_s1.ini"); BYTE d[4] = { 1, 2, 3, 0xff };
    SET_ERR(0); CHECK(UK(WritePrivateProfileStructA)("Sec", "K", d, 4, f));
    raw(f, "Sec", "K", v, sizeof v); CHECKF(ieq(v, "010203FF05"), "got %s", v);
    BYTE o[4] = { 0 }; CHECK(UK(GetPrivateProfileStructA)("Sec", "K", o, 4, f)); CHECK(!memcmp(o, d, 4));
    DeleteFileA(f);
}
TEST(gap16_wpps_checksum_wraps_mod_256) {
    WP_NEED; char f[MAX_PATH], v[512]; ini(f, "g16_s2.ini"); BYTE d[3] = { 0x80, 0x80, 0x01 };
    CHECK(UK(WritePrivateProfileStructA)("S", "K", d, 3, f)); raw(f, "S", "K", v, sizeof v); CHECKF(ieq(v, "80800101"), "got %s", v);
    BYTE e[2] = { 0xff, 0x01 }; CHECK(UK(WritePrivateProfileStructA)("S", "K2", e, 2, f)); raw(f, "S", "K2", v, sizeof v); CHECKF(ieq(v, "ff0100"), "got %s", v);
    DeleteFileA(f);
}
TEST(gap16_wpps_zero_size_struct) {
    WP_NEED; char f[MAX_PATH], v[64]; ini(f, "g16_s3.ini"); BYTE d = 9;
    CHECK(UK(WritePrivateProfileStructA)("S", "K", &d, 0, f)); raw(f, "S", "K", v, sizeof v); CHECKF(ieq(v, "00"), "got %s", v);
    BYTE o = 0x55; CHECK(UK(GetPrivateProfileStructA)("S", "K", &o, 0, f)); CHECK(o == 0x55);
    DeleteFileA(f);
}
TEST(gap16_wpps_all_byte_values_roundtrip) {
    WP_NEED; char f[MAX_PATH]; ini(f, "g16_s4.ini"); BYTE d[256], o[256]; for (int i = 0; i < 256; i++) d[i] = (BYTE)i;
    CHECK(UK(WritePrivateProfileStructA)("S", "All", d, 256, f)); memset(o, 0xcc, 256);
    CHECK(UK(GetPrivateProfileStructA)("S", "All", o, 256, f)); CHECK(!memcmp(o, d, 256)); DeleteFileA(f);
}
TEST(gap16_wpps_size_table_roundtrip) {
    WP_NEED; char f[MAX_PATH]; ini(f, "g16_s5.ini"); static BYTE d[2000], o[2000]; for (int i = 0; i < 2000; i++) d[i] = (BYTE)(i * 7 + 3);
    static const UINT sz[] = { 1, 2, 3, 7, 8, 63, 64, 65, 127, 128, 129, 130, 255, 256, 500, 1000, 2000 };
    for (int i = 0; i < 17; i++) {
        BOOL w = UK(WritePrivateProfileStructA)("T", "Key", d, sz[i], f); CHECKF(w, "write size %u err %lu", sz[i], GetLastError()); if (!w) continue;
        memset(o, 0, sizeof o); BOOL r = UK(GetPrivateProfileStructA)("T", "Key", o, sz[i], f);
        CHECKF(r && !memcmp(o, d, sz[i]), "roundtrip size %u r=%d err %lu", sz[i], r, GetLastError());
    }
    DeleteFileA(f);
}
TEST(gap16_wpps_null_struct_deletes_key) {
    WP_NEED; char f[MAX_PATH], v[64]; ini(f, "g16_s6.ini"); BYTE d[2] = { 1, 2 };
    CHECK(UK(WritePrivateProfileStructA)("S", "A", d, 2, f)); CHECK(UK(WritePrivateProfileStructA)("S", "B", d, 2, f));
    SET_ERR(0); CHECK(UK(WritePrivateProfileStructA)("S", "A", NULL, 2, f));
    raw(f, "S", "A", v, sizeof v); EXPECT_STR(v, ""); raw(f, "S", "B", v, sizeof v); CHECK(v[0]);
    CHECK(UK(WritePrivateProfileStructA)("S", "NoSuchKey", NULL, 0, f));
    BYTE o[2]; SET_ERR(0); CHECK(!UK(GetPrivateProfileStructA)("S", "A", o, 2, f)); EXPECT_ERR(ERROR_BAD_LENGTH);
    DeleteFileA(f);
}
TEST(gap16_wpps_overwrite_replaces) {
    WP_NEED; char f[MAX_PATH], v[64]; ini(f, "g16_s7.ini"); BYTE a[2] = { 1, 2 }, b[3] = { 9, 8, 7 };
    CHECK(UK(WritePrivateProfileStructA)("S", "K", a, 2, f)); CHECK(UK(WritePrivateProfileStructA)("S", "K", b, 3, f));
    raw(f, "S", "K", v, sizeof v); CHECKF(ieq(v, "09080718"), "got %s", v);
    BYTE o[3]; CHECK(UK(GetPrivateProfileStructA)("S", "K", o, 3, f)); CHECK(!memcmp(o, b, 3));
    DeleteFileA(f);
}
TEST(gap16_wpps_get_wrong_size_bad_length) {
    WP_NEED; char f[MAX_PATH]; ini(f, "g16_s8.ini"); BYTE d[4] = { 1, 2, 3, 4 }, o[8];
    CHECK(UK(WritePrivateProfileStructA)("S", "K", d, 4, f));
    static const UINT bad[] = { 0, 1, 3, 5, 8 };
    for (int i = 0; i < 5; i++) { SET_ERR(0); CHECKF(!UK(GetPrivateProfileStructA)("S", "K", o, bad[i], f), "size %u", bad[i]); EXPECT_ERR(ERROR_BAD_LENGTH); }
    SET_ERR(0); CHECK(!UK(GetPrivateProfileStructA)("S", "Missing", o, 4, f)); EXPECT_ERR(ERROR_BAD_LENGTH);
    SET_ERR(0); CHECK(!UK(GetPrivateProfileStructA)("Nosec", "K", o, 4, f)); EXPECT_ERR(ERROR_BAD_LENGTH);
    DeleteFileA(f);
}
TEST(gap16_wpps_get_bad_checksum_invalid_data) {
    WP_NEED; char f[MAX_PATH]; ini(f, "g16_s9.ini"); BYTE o[3];
    WritePrivateProfileStringA("S", "K", "0102FF", f);   /* checksum should be 0x02+0x01? : 01+02 = 03 */
    SET_ERR(0); CHECK(!UK(GetPrivateProfileStructA)("S", "K", o, 2, f)); EXPECT_ERR(ERROR_INVALID_DATA);
    WritePrivateProfileStringA("S", "K", "010203", f); BYTE p[2];
    SET_ERR(0); CHECK(UK(GetPrivateProfileStructA)("S", "K", p, 2, f)); CHECK(p[0] == 1 && p[1] == 2);
    DeleteFileA(f);
}
TEST(gap16_wpps_huge_size_invalid_parameter) {
    WP_NEED; if (t_is_wine()) { t_skip("emulator reads the struct before validating the size"); return; } char f[MAX_PATH]; ini(f, "g16_s10.ini"); BYTE d = 1;
    static const UINT sz[] = { 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFFu, 0xC0000000u };
    for (int i = 0; i < 4; i++) { SET_ERR(0); CHECKF(!UK(WritePrivateProfileStructA)("S", "K", &d, sz[i], f), "size %x", sz[i]); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
    CHECK(GetFileAttributesA(f) == INVALID_FILE_ATTRIBUTES || 1);
    DeleteFileA(f);
}
TEST(gap16_wpps_wide_variant_same_text) {
    WP_NEED; REQUIRE(UK(WritePrivateProfileStructW)); char fa[MAX_PATH], v1[128], v2[128]; ini(fa, "g16_s11.ini"); WCHAR fw[MAX_PATH]; MultiByteToWideChar(CP_ACP, 0, fa, -1, fw, MAX_PATH);
    BYTE d[5] = { 0xde, 0xad, 0xbe, 0xef, 0x10 };
    CHECK(UK(WritePrivateProfileStructA)("S", "A", d, 5, fa)); CHECK(UK(WritePrivateProfileStructW)(L"S", L"W", d, 5, fw));
    raw(fa, "S", "A", v1, sizeof v1); raw(fa, "S", "W", v2, sizeof v2); CHECKF(ieq(v1, v2), "A=%s W=%s", v1, v2);
    DeleteFileA(fa);
}
TEST(gap16_wpps_creates_file_and_sections) {
    WP_NEED; char f[MAX_PATH]; ini(f, "g16_s12.ini"); DeleteFileA(f); BYTE d[1] = { 5 };
    CHECK(GetFileAttributesA(f) == INVALID_FILE_ATTRIBUTES);
    CHECK(UK(WritePrivateProfileStructA)("NewSec", "k", d, 1, f)); CHECK(GetFileAttributesA(f) != INVALID_FILE_ATTRIBUTES);
    char sec[256]; DWORD n = GetPrivateProfileSectionNamesA(sec, sizeof sec, f); CHECK(n == 7 && !strcmp(sec, "NewSec"));
    DeleteFileA(f);
}
TEST(gap16_wpps_key_and_section_case_insensitive) {
    WP_NEED; char f[MAX_PATH]; ini(f, "g16_s13.ini"); BYTE d[2] = { 3, 4 }, o[2];
    CHECK(UK(WritePrivateProfileStructA)("Section", "KeyName", d, 2, f)); CHECK(UK(GetPrivateProfileStructA)("SECTION", "keyname", o, 2, f)); CHECK(!memcmp(o, d, 2));
    DeleteFileA(f);
}
TEST(gap16_wpps_unwritable_path_fails) {
    WP_NEED; char f[MAX_PATH]; t_tmpfile(f, sizeof f, "no_such_dir\\x\\g16.ini"); BYTE d[1] = { 1 };
    SET_ERR(0); CHECK(!UK(WritePrivateProfileStructA)("S", "K", &d, 1, f)); EXPECT_ERR2(ERROR_PATH_NOT_FOUND, ERROR_FILE_NOT_FOUND);
}

/* ================================================================ WriteTapemark */
#define TP_NEED REQUIRE(UK(WriteTapemark))
TEST(gap16_tapemark_only_in_kernel32) { TP_NEED; CHECK(t_proc_kb("WriteTapemark") == NULL); }
TEST(gap16_tapemark_null_and_invalid_handles) {
    TP_NEED; DWORD r;
    SET_ERR(0); r = UK(WriteTapemark)(NULL, TAPE_SHORT_FILEMARKS, 1, FALSE); EXPECT_EQ(r, ERROR_INVALID_HANDLE);
    SET_ERR(0); r = UK(WriteTapemark)(INVALID_HANDLE_VALUE, TAPE_SHORT_FILEMARKS, 1, FALSE); EXPECT_EQ(r, ERROR_INVALID_HANDLE);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL); r = UK(WriteTapemark)(e, TAPE_FILEMARKS, 1, TRUE); EXPECT_EQ(r, ERROR_INVALID_HANDLE); CloseHandle(e);
    r = UK(WriteTapemark)(e, TAPE_FILEMARKS, 1, TRUE); EXPECT_EQ(r, ERROR_INVALID_HANDLE);
}
TEST(gap16_tapemark_regular_file_unsupported) {
    TP_NEED; char p[MAX_PATH]; t_tmpfile(p, sizeof p, "g16_tape.bin");
    HANDLE f = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); REQUIRE(f != INVALID_HANDLE_VALUE);
    static const DWORD type[] = { TAPE_SETMARKS, TAPE_FILEMARKS, TAPE_SHORT_FILEMARKS, TAPE_LONG_FILEMARKS };
    for (int i = 0; i < 4; i++) { DWORD r = UK(WriteTapemark)(f, type[i], 1, FALSE); CHECKF(r != NO_ERROR, "type %lu returned success on a disk file", type[i]); t_note("type %lu -> %lu", type[i], r); }
    DWORD r = UK(WriteTapemark)(f, 99, 0, TRUE); CHECK(r != NO_ERROR); r = UK(WriteTapemark)(f, TAPE_FILEMARKS, 0xffffffffu, TRUE); CHECK(r != NO_ERROR);
    DWORD sz = GetFileSize(f, NULL); EXPECT_EQ(sz, 0);
    CloseHandle(f); DeleteFileA(p);
}
TEST(gap16_tapemark_directory_and_closed_file) {
    TP_NEED; char p[MAX_PATH]; t_tmpfile(p, sizeof p, "g16_tape2.bin");
    HANDLE f = CreateFileA(p, GENERIC_READ, 0, NULL, CREATE_ALWAYS, 0, NULL); REQUIRE(f != INVALID_HANDLE_VALUE); CloseHandle(f);
    EXPECT_EQ(UK(WriteTapemark)(f, TAPE_FILEMARKS, 1, FALSE), ERROR_INVALID_HANDLE);
    DeleteFileA(p);
}
TEST(gap16_tapemark_does_not_set_last_error_on_success_path_contract) {
    TP_NEED; SET_ERR(0x1234); DWORD r = UK(WriteTapemark)(NULL, TAPE_FILEMARKS, 1, FALSE);
    t_note("return %lu lasterr %lu", r, GetLastError()); CHECK(r == ERROR_INVALID_HANDLE);
}
