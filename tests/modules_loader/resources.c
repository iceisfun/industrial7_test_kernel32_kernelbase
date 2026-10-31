/* FindResource, LoadResource, LockResource, SizeofResource, FreeResource, EnumResource* */
#include "ml.h"

#define RES_MODES 3
static const char *mode_name[RES_MODES] = { "image", "datafile", "image_resource" };
static const DWORD mode_flags[RES_MODES] = { 0, LOAD_LIBRARY_AS_DATAFILE, LOAD_LIBRARY_AS_IMAGE_RESOURCE };

static HMODULE res_open(int mode, const char *sub, const char *leaf) {
    char p[MAX_PATH];
    if (!ml_put(ML_A, ml_dir(sub), leaf, p)) return NULL;
    return LoadLibraryExA(p, NULL, mode_flags[mode]);
}
static void res_close(HMODULE h) { ml_unload(h); }

#define FOR_MODES(sub) \
    for (int mode = 0; mode < RES_MODES; mode++) \
        if (mode == 2 && t_is_wine()) { t_note("image_resource mode skipped under wine"); } else \
        for (HMODULE h = res_open(mode, sub, "k32m_res.dll"), once = (HMODULE)1; once; once = NULL, res_close(h)) \
            if (!h) { CHECKF(0, "cannot load (%s)", mode_name[mode]); } else

static const void *res_data(HMODULE h, LPCSTR name, LPCSTR type, DWORD *size) {
    HRSRC r = FindResourceA(h, name, type);
    if (!r) return NULL;
    HGLOBAL g = LoadResource(h, r);
    if (!g) return NULL;
    if (size) *size = SizeofResource(h, r);
    return LockResource(g);
}

/* ---- FindResource / Load / Lock / Sizeof ---- */
TEST(res_find_rcdata_by_id_a) {
    REQUIRE(U(FindResourceA)); REQUIRE(U(LoadResource)); REQUIRE(U(LockResource)); REQUIRE(U(SizeofResource));
    FOR_MODES("res_a") {
        HRSRC r = U(FindResourceA)(h, MAKEINTRESOURCEA(1), RT_RCDATA);
        CHECKF(r != NULL, "%s err %lu", mode_name[mode], GetLastError());
        if (!r) continue;
        EXPECT_EQ(U(SizeofResource)(h, r), 8);
        HGLOBAL g = U(LoadResource)(h, r);
        CHECK(g != NULL);
        const char *d = g ? U(LockResource)(g) : NULL;
        CHECK(d != NULL);
        if (d) CHECKF(!memcmp(d, "ABCDEFGH", 8), "%s", mode_name[mode]);
    }
}
TEST(res_find_rcdata_by_id_w) {
    REQUIRE(U(FindResourceW)); REQUIRE(U(LoadResource)); REQUIRE(U(SizeofResource));
    FOR_MODES("res_w") {
        HRSRC a = FindResourceA(h, MAKEINTRESOURCEA(1), RT_RCDATA);
        HRSRC w = U(FindResourceW)(h, MAKEINTRESOURCEW(1), (LPWSTR)RT_RCDATA);
        CHECKF(w != NULL, "%s", mode_name[mode]);
        CHECK(a == w);
        EXPECT_EQ(U(SizeofResource)(h, w), 8);
    }
}
TEST(res_find_ex_languages) {
    REQUIRE(U(FindResourceExA)); REQUIRE(U(FindResourceExW)); REQUIRE(U(LoadResource)); REQUIRE(U(SizeofResource));
    FOR_MODES("res_lang") {
        HRSRC en = U(FindResourceExA)(h, RT_RCDATA, MAKEINTRESOURCEA(7), 0x409);
        HRSRC de = U(FindResourceExA)(h, RT_RCDATA, MAKEINTRESOURCEA(7), 0x407);
        CHECK(en != NULL && de != NULL && en != de);
        if (!en || !de) continue;
        EXPECT_EQ(U(SizeofResource)(h, en), 2);
        EXPECT_EQ(U(SizeofResource)(h, de), 8);
        CHECK(!memcmp(LockResource(LoadResource(h, en)), "EN", 2));
        CHECK(!memcmp(LockResource(LoadResource(h, de)), "DEUTSCH!", 8));
        HRSRC enw = U(FindResourceExW)(h, (LPWSTR)RT_RCDATA, MAKEINTRESOURCEW(7), 0x409);
        CHECK(enw == en);
        HRSRC dew = U(FindResourceExW)(h, (LPWSTR)RT_RCDATA, MAKEINTRESOURCEW(7), 0x407);
        CHECK(dew == de);
    }
}
TEST(res_find_ex_neutral_language_lookup) {
    REQUIRE(U(FindResourceExA)); REQUIRE(U(SizeofResource));
    FOR_MODES("res_neu") {
        HRSRC r = U(FindResourceExA)(h, RT_RCDATA, MAKEINTRESOURCEA(7), 0);
        CHECKF(r != NULL, "%s err %lu", mode_name[mode], GetLastError());
        if (r) { DWORD sz = U(SizeofResource)(h, r); CHECK(sz == 2 || sz == 8); t_note("neutral lookup of multi-language resource picked %s", sz == 2 ? "en-US" : "de-DE"); }
        HRSRC plain = FindResourceA(h, MAKEINTRESOURCEA(7), RT_RCDATA);
        CHECK(plain == r);                       /* FindResource == FindResourceEx(..., 0) */
    }
}
TEST(res_find_ex_language_neutral_fallback) {
    REQUIRE(U(FindResourceExA)); REQUIRE(U(SizeofResource));
    FOR_MODES("res_fb") {
        /* the version resource and string tables are language-neutral: any request language finds them */
        static const WORD lang[] = { 0, 0x409, 0x407, 0x0C0A, 0x0411, 0x0800 };
        for (int i = 0; i < 6; i++) {
            HRSRC r = U(FindResourceExA)(h, RT_VERSION, MAKEINTRESOURCEA(1), lang[i]);
            CHECKF(r != NULL, "%s lang %04x err %lu", mode_name[mode], lang[i], GetLastError());
            if (r) EXPECT_EQ(U(SizeofResource)(h, r), 332);
        }
    }
}
TEST(res_find_ex_missing_language) {
    REQUIRE(U(FindResourceExA));
    FOR_MODES("res_ml") {
        SET_ERR(0);
        HRSRC r = U(FindResourceExA)(h, RT_RCDATA, MAKEINTRESOURCEA(1), 0x040C);   /* only en-US exists */
        t_note("%s: FindResourceEx(res with only en-US, fr-FR) -> %p err %lu", mode_name[mode], r, r ? 0 : GetLastError());
        if (!r) { DWORD e = GetLastError(); CHECK(e == ERROR_RESOURCE_LANG_NOT_FOUND); }
        r = U(FindResourceExA)(h, RT_RCDATA, MAKEINTRESOURCEA(1), 0x0409);
        CHECK(r != NULL);
    }
}
TEST(res_find_type_not_found) {
    REQUIRE(U(FindResourceA)); REQUIRE(U(FindResourceW)); REQUIRE(U(FindResourceExA)); REQUIRE(U(FindResourceExW));
    FOR_MODES("res_tnf") {
        SET_ERR(0);
        CHECK(U(FindResourceA)(h, MAKEINTRESOURCEA(1), MAKEINTRESOURCEA(9999)) == NULL);
        EXPECT_ERR(ERROR_RESOURCE_TYPE_NOT_FOUND);
        SET_ERR(0);
        CHECK(U(FindResourceW)(h, MAKEINTRESOURCEW(1), MAKEINTRESOURCEW(9999)) == NULL);
        EXPECT_ERR(ERROR_RESOURCE_TYPE_NOT_FOUND);
        SET_ERR(0);
        CHECK(U(FindResourceExA)(h, "NOSUCHTYPE", MAKEINTRESOURCEA(1), 0) == NULL);
        EXPECT_ERR(ERROR_RESOURCE_TYPE_NOT_FOUND);
        SET_ERR(0);
        CHECK(U(FindResourceExW)(h, L"NOSUCHTYPE", MAKEINTRESOURCEW(1), 0x409) == NULL);
        EXPECT_ERR(ERROR_RESOURCE_TYPE_NOT_FOUND);
        SET_ERR(0);
        CHECK(U(FindResourceA)(h, MAKEINTRESOURCEA(1), RT_ICON) == NULL);
        EXPECT_ERR(ERROR_RESOURCE_TYPE_NOT_FOUND);
        SET_ERR(0);
        CHECK(U(FindResourceA)(h, MAKEINTRESOURCEA(1), RT_MANIFEST) == NULL);
        EXPECT_ERR(ERROR_RESOURCE_TYPE_NOT_FOUND);
    }
}
TEST(res_find_name_not_found) {
    REQUIRE(U(FindResourceA)); REQUIRE(U(FindResourceW));
    FOR_MODES("res_nnf") {
        SET_ERR(0);
        CHECK(U(FindResourceA)(h, MAKEINTRESOURCEA(9999), RT_RCDATA) == NULL);
        EXPECT_ERR(ERROR_RESOURCE_NAME_NOT_FOUND);
        SET_ERR(0);
        CHECK(U(FindResourceA)(h, "NOSUCHNAME", RT_RCDATA) == NULL);
        EXPECT_ERR(ERROR_RESOURCE_NAME_NOT_FOUND);
        SET_ERR(0);
        CHECK(U(FindResourceW)(h, L"NOSUCHNAME", (LPWSTR)RT_RCDATA) == NULL);
        EXPECT_ERR(ERROR_RESOURCE_NAME_NOT_FOUND);
        SET_ERR(0);
        CHECK(U(FindResourceA)(h, MAKEINTRESOURCEA(2), RT_VERSION) == NULL);
        EXPECT_ERR(ERROR_RESOURCE_NAME_NOT_FOUND);
        SET_ERR(0);
        CHECK(U(FindResourceA)(h, "NAMED", RT_VERSION) == NULL);
        EXPECT_ERR(ERROR_RESOURCE_NAME_NOT_FOUND);
    }
}
TEST(res_find_string_names_case_insensitive) {
    REQUIRE(U(FindResourceA)); REQUIRE(U(FindResourceW)); REQUIRE(U(SizeofResource));
    FOR_MODES("res_nm") {
        HRSRC a = U(FindResourceA)(h, "NAMED", RT_RCDATA);
        CHECKF(a != NULL, "%s err %lu", mode_name[mode], GetLastError());
        if (!a) continue;
        EXPECT_EQ(U(SizeofResource)(h, a), 14);
        CHECK(U(FindResourceA)(h, "named", RT_RCDATA) == a);
        CHECK(U(FindResourceA)(h, "NaMeD", RT_RCDATA) == a);
        CHECK(U(FindResourceW)(h, L"named", (LPWSTR)RT_RCDATA) == a);
        CHECK(U(FindResourceW)(h, L"NAMED", (LPWSTR)RT_RCDATA) == a);
        SET_ERR(0);
        CHECK(U(FindResourceA)(h, "NAME", RT_RCDATA) == NULL);       /* no prefix match */
        EXPECT_ERR(ERROR_RESOURCE_NAME_NOT_FOUND);
        SET_ERR(0);
        CHECK(U(FindResourceA)(h, "NAMED ", RT_RCDATA) == NULL);
        EXPECT_ERR(ERROR_RESOURCE_NAME_NOT_FOUND);
    }
}
TEST(res_find_custom_type) {
    REQUIRE(U(FindResourceA)); REQUIRE(U(FindResourceW)); REQUIRE(U(SizeofResource));
    FOR_MODES("res_ct") {
        HRSRC a = U(FindResourceA)(h, MAKEINTRESOURCEA(200), "CUSTOMTYPE");
        CHECKF(a != NULL, "%s err %lu", mode_name[mode], GetLastError());
        if (a) EXPECT_EQ(U(SizeofResource)(h, a), 6);
        CHECK(U(FindResourceA)(h, MAKEINTRESOURCEA(200), "customtype") == a);
        CHECK(U(FindResourceW)(h, MAKEINTRESOURCEW(200), L"CustomType") == a);
        HRSRC b = U(FindResourceA)(h, "CT2", "CUSTOMTYPE");
        CHECK(b != NULL && b != a);
        if (b) EXPECT_EQ(U(SizeofResource)(h, b), 2);
        const void *d = res_data(h, "CT2", "CUSTOMTYPE", NULL);
        CHECK(d && !memcmp(d, "c2", 2));
    }
}
TEST(res_find_hash_ids) {
    REQUIRE(U(FindResourceA)); REQUIRE(U(FindResourceW));
    FOR_MODES("res_hash") {
        HRSRC id = U(FindResourceA)(h, MAKEINTRESOURCEA(1), RT_RCDATA);
        CHECK(id != NULL);
        CHECK(U(FindResourceA)(h, "#1", RT_RCDATA) == id);
        CHECK(U(FindResourceA)(h, "#1", "#10") == id);
        CHECK(U(FindResourceW)(h, L"#1", L"#10") == id);
        CHECK(U(FindResourceA)(h, MAKEINTRESOURCEA(1), "#10") == id);
        HRSRC v = U(FindResourceA)(h, "#1", "#16");
        CHECK(v == U(FindResourceA)(h, MAKEINTRESOURCEA(1), RT_VERSION));
        CHECK(v != NULL);
        SET_ERR(0);
        CHECK(U(FindResourceA)(h, "#9999", RT_RCDATA) == NULL);
        EXPECT_ERR(ERROR_RESOURCE_NAME_NOT_FOUND);
        SET_ERR(0);
        CHECK(U(FindResourceA)(h, MAKEINTRESOURCEA(1), "#9999") == NULL);
        EXPECT_ERR(ERROR_RESOURCE_TYPE_NOT_FOUND);
    }
}
TEST(res_find_hash_malformed) {
    REQUIRE(U(FindResourceA)); REQUIRE(U(FindResourceW));
    FOR_MODES("res_hashbad") {
        /* ids above 0xFFFF (or negative) cannot be represented */
        static const char *bad[] = { "#65536", "#-1", "#70000" };
        for (int i = 0; i < 3; i++) {
            SET_ERR(0);
            CHECKF(U(FindResourceA)(h, bad[i], RT_RCDATA) == NULL, "%s", bad[i]);
            DWORD e = GetLastError();
            CHECKF(e == ERROR_INVALID_PARAMETER, "%s e=%lu", bad[i], e);
            SET_ERR(0);
            CHECKF(U(FindResourceA)(h, MAKEINTRESOURCEA(1), bad[i]) == NULL, "type %s", bad[i]);
            CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "type %s e=%lu", bad[i], GetLastError());
            SET_ERR(0);
            CHECKF(U(FindResourceW)(h, ml_w(bad[i]), (LPWSTR)RT_RCDATA) == NULL, "W %s", bad[i]);
            CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "W %s e=%lu", bad[i], GetLastError());
        }
        /* trailing garbage / padding is tolerated by the integer parser: record only */
        static const char *odd[] = { "#1 ", "# 1", "#12x", "#x", "#", "#0" };
        for (int i = 0; i < 6; i++) {
            SET_ERR(0);
            HRSRC r = U(FindResourceA)(h, odd[i], RT_RCDATA);
            t_note("%s: FindResource(\"%s\") -> %s err %lu", mode_name[mode], odd[i], r ? "found" : "NULL", r ? 0 : GetLastError());
        }
    }
}
TEST(res_find_a_w_parity_all_resources) {
    REQUIRE(U(FindResourceA)); REQUIRE(U(FindResourceW));
    FOR_MODES("res_par") {
        static const struct { LPCSTR n; LPCSTR t; } t[] = { { MAKEINTRESOURCEA(1), RT_RCDATA }, { MAKEINTRESOURCEA(7), RT_RCDATA }, { "NAMED", RT_RCDATA },
            { MAKEINTRESOURCEA(1), RT_STRING }, { MAKEINTRESOURCEA(2), RT_STRING }, { MAKEINTRESOURCEA(1), RT_VERSION }, { "CT2", "CUSTOMTYPE" }, { MAKEINTRESOURCEA(200), "CUSTOMTYPE" } };
        for (int i = 0; i < 8; i++) {
            HRSRC a = U(FindResourceA)(h, t[i].n, t[i].t);
            WCHAR wn[32], wt[32]; LPCWSTR pn, pt;
            if ((ULONG_PTR)t[i].n > 0xFFFF) { MultiByteToWideChar(CP_ACP, 0, t[i].n, -1, wn, 32); pn = wn; } else pn = (LPCWSTR)t[i].n;
            if ((ULONG_PTR)t[i].t > 0xFFFF) { MultiByteToWideChar(CP_ACP, 0, t[i].t, -1, wt, 32); pt = wt; } else pt = (LPCWSTR)t[i].t;
            HRSRC w = U(FindResourceW)(h, pn, pt);
            CHECKF(a != NULL && a == w, "%s item %d", mode_name[mode], i);
        }
    }
}
TEST(res_find_null_module_is_exe) {
    REQUIRE(U(FindResourceA)); REQUIRE(U(FindResourceW));
    SET_ERR(0);
    HRSRC r = U(FindResourceA)(NULL, MAKEINTRESOURCEA(1), RT_RCDATA);
    CHECK(r == NULL);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_RESOURCE_DATA_NOT_FOUND || e == ERROR_RESOURCE_TYPE_NOT_FOUND || e == ERROR_RESOURCE_NAME_NOT_FOUND, "e=%lu", e);
    SET_ERR(0);
    r = U(FindResourceW)(GetModuleHandleA(NULL), MAKEINTRESOURCEW(1), (LPWSTR)RT_VERSION);
    if (!r) { e = GetLastError(); CHECKF(e == ERROR_RESOURCE_DATA_NOT_FOUND || e == ERROR_RESOURCE_TYPE_NOT_FOUND, "e=%lu", e); }
}
TEST(res_find_exe_has_no_manifest_by_default) {
    REQUIRE(U(FindResourceA));
    SET_ERR(0);
    HRSRC r = U(FindResourceA)(NULL, MAKEINTRESOURCEA(1), RT_MANIFEST);
    if (r) t_note("test exe carries an embedded manifest");
    else { DWORD e = GetLastError(); CHECKF(e == ERROR_RESOURCE_DATA_NOT_FOUND || e == ERROR_RESOURCE_TYPE_NOT_FOUND, "e=%lu", e); }
}
TEST(res_find_ex_null_type_name) {
    REQUIRE(U(FindResourceExA));
    HMODULE h = res_open(0, "res_nul", "k32m_res.dll"); if (!h) { CHECK(0); return; }
    SET_ERR(0);
    HRSRC r = U(FindResourceExA)(h, NULL, MAKEINTRESOURCEA(1), 0);
    CHECK(r == NULL);
    t_note("FindResourceEx(type NULL) error %lu", GetLastError());
    SET_ERR(0);
    r = U(FindResourceExA)(h, RT_RCDATA, NULL, 0);
    CHECK(r == NULL);
    t_note("FindResourceEx(name NULL) error %lu", GetLastError());
    res_close(h);
}
TEST(res_load_null_info) {
    REQUIRE(U(LoadResource)); REQUIRE(U(SizeofResource));
    FOR_MODES("res_lni") {
        SET_ERR(0);
        CHECK(U(LoadResource)(h, NULL) == NULL);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        SET_ERR(0);
        CHECK(U(SizeofResource)(h, NULL) == 0);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
    SET_ERR(0);
    CHECK(U(LoadResource)(NULL, NULL) == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(U(SizeofResource)(NULL, NULL) == 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(res_lockresource_is_identity) {
    REQUIRE(U(LockResource));
    CHECK(U(LockResource)(NULL) == NULL);
    CHECK(U(LockResource)((HGLOBAL)(ULONG_PTR)0x12340) == (LPVOID)(ULONG_PTR)0x12340);
    int x; CHECK(U(LockResource)((HGLOBAL)&x) == (LPVOID)&x);
}
TEST(res_load_returns_pointer_into_module) {
    REQUIRE(U(LoadResource)); REQUIRE(U(LockResource));
    FOR_MODES("res_ptr") {
        HRSRC r = FindResourceA(h, MAKEINTRESOURCEA(1), RT_RCDATA);
        HGLOBAL g = U(LoadResource)(h, r);
        CHECK(g != NULL);
        char *base = (char *)((ULONG_PTR)h & ~(ULONG_PTR)3);
        DWORD sz = ml_nt(base)->OptionalHeader.SizeOfImage;
        CHECKF((char *)g >= base && (char *)g < base + sz, "%s g=%p base=%p", mode_name[mode], g, base);
        /* loading twice is stable */
        CHECK(U(LoadResource)(h, r) == g);
        CHECK(U(LockResource)(g) == g);
    }
}
TEST(res_sizeof_all_known) {
    REQUIRE(U(SizeofResource));
    FOR_MODES("res_sz") {
        static const struct { LPCSTR n; LPCSTR t; DWORD sz; } t[] = { { MAKEINTRESOURCEA(1), RT_RCDATA, 8 }, { "NAMED", RT_RCDATA, 14 },
            { MAKEINTRESOURCEA(1), RT_STRING, 44 }, { MAKEINTRESOURCEA(2), RT_STRING, 46 }, { MAKEINTRESOURCEA(1), RT_VERSION, 332 },
            { MAKEINTRESOURCEA(200), "CUSTOMTYPE", 6 }, { "CT2", "CUSTOMTYPE", 2 } };
        for (int i = 0; i < 7; i++) {
            HRSRC r = FindResourceA(h, t[i].n, t[i].t);
            CHECKF(r != NULL && U(SizeofResource)(h, r) == t[i].sz, "%s item %d", mode_name[mode], i);
        }
    }
}
TEST(res_string_table_layout) {
    REQUIRE(U(FindResourceA));
    FOR_MODES("res_str") {
        DWORD sz;
        const WORD *b1 = res_data(h, MAKEINTRESOURCEA(1), RT_STRING, &sz);
        CHECK(b1 && sz == 44);
        if (b1) {
            CHECK(b1[0] == 0);                       /* id 0 empty */
            CHECK(b1[1] == 3 && !memcmp(b1 + 2, L"one", 6));
            CHECK(b1[5] == 3 && !memcmp(b1 + 6, L"two", 6));
            CHECK(b1[9] == 0);
        }
        const WORD *b2 = res_data(h, MAKEINTRESOURCEA(2), RT_STRING, &sz);
        CHECK(b2 && sz == 46);
        if (b2) CHECK(b2[0] == 7 && !memcmp(b2 + 1, L"sixteen", 14));
    }
}
TEST(res_version_resource_fixed_info) {
    REQUIRE(U(FindResourceA));
    FOR_MODES("res_ver") {
        DWORD sz;
        const WORD *v = res_data(h, MAKEINTRESOURCEA(1), RT_VERSION, &sz);
        CHECK(v != NULL && sz == 332);
        if (!v) continue;
        CHECK(v[0] == sz);                           /* wLength */
        CHECK(v[1] == sizeof(VS_FIXEDFILEINFO));     /* wValueLength */
        CHECK(v[2] == 0);                            /* binary */
        CHECK(!lstrcmpW((const WCHAR *)(v + 3), L"VS_VERSION_INFO"));
        const VS_FIXEDFILEINFO *ff = (const void *)((const char *)v + 40);
        CHECK(ff->dwSignature == 0xFEEF04BD);
        CHECK(ff->dwFileVersionMS == 0x00010002 && ff->dwFileVersionLS == 0x00030004);
        CHECK(ff->dwProductVersionMS == 0x00050006 && ff->dwProductVersionLS == 0x00070008);
        CHECK(ff->dwFileType == VFT_DLL);
        CHECK(ff->dwFileOS == VOS_NT_WINDOWS32);
    }
}
TEST(res_freeresource) {
    REQUIRE(U(FreeResource)); REQUIRE(U(LoadResource));
    FOR_MODES("res_free") {
        HRSRC r = FindResourceA(h, MAKEINTRESOURCEA(1), RT_RCDATA);
        HGLOBAL g = U(LoadResource)(h, r);
        CHECK(g != NULL);
        SET_ERR(0);
        BOOL f = U(FreeResource)(g);
        CHECKF(f == 0, "FreeResource returned %d", f);
        /* data stays valid: resources are not heap blocks */
        CHECK(!memcmp(LockResource(g), "ABCDEFGH", 8));
        CHECK(U(LoadResource)(h, r) == g);
    }
}
TEST(res_freeresource_other_values) {
    REQUIRE(U(FreeResource));
    BOOL r = U(FreeResource)(NULL);
    t_note("FreeResource(NULL) -> %d", r);
}
TEST(res_system_dll_version_resources) {
    REQUIRE(U(FindResourceA));
    static const char *n[] = { "kernel32.dll", "kernelbase.dll", "ntdll.dll", "user32.dll", "advapi32.dll" };
    for (int i = 0; i < 5; i++) {
        HMODULE h = LoadLibraryA(n[i]);
        HRSRC r = U(FindResourceA)(h, MAKEINTRESOURCEA(1), RT_VERSION);
        CHECKF(r != NULL, "%s err %lu", n[i], GetLastError());
        if (!r) continue;
        DWORD sz = SizeofResource(h, r);
        CHECK(sz > 100 && sz < 4096);
        const WORD *v = LockResource(LoadResource(h, r));
        CHECK(v && v[0] <= sz && v[1] == sizeof(VS_FIXEDFILEINFO));
        if (v && v[1] == sizeof(VS_FIXEDFILEINFO)) {
            const VS_FIXEDFILEINFO *ff = (const void *)((const char *)v + 40);
            CHECKF(ff->dwSignature == 0xFEEF04BD, "%s", n[i]);
            CHECKF(HIWORD(ff->dwFileVersionMS) == 10, "%s file version %lx", n[i], ff->dwFileVersionMS);
            CHECKF(ff->dwFileType == VFT_DLL, "%s", n[i]);
            CHECK(ff->dwFileOS == VOS_NT_WINDOWS32 || ff->dwFileOS == 0x00040004);
            t_note("%s version %lu.%lu.%lu.%lu", n[i], HIWORD(ff->dwFileVersionMS), LOWORD(ff->dwFileVersionMS), HIWORD(ff->dwFileVersionLS), LOWORD(ff->dwFileVersionLS));
        }
        FreeLibrary(h);
    }
}
TEST(res_system_dll_image_vs_datafile_equal) {
    REQUIRE(U(FindResourceA));
    char p[MAX_PATH]; GetSystemDirectoryA(p, sizeof p); strcat(p, "\\user32.dll");
    HMODULE img = LoadLibraryA("user32.dll"), df = LoadLibraryExA(p, NULL, LOAD_LIBRARY_AS_DATAFILE);
    CHECK(img && df); if (!img || !df) return;
    HRSRC a = U(FindResourceA)(img, MAKEINTRESOURCEA(1), RT_VERSION), b = U(FindResourceA)(df, MAKEINTRESOURCEA(1), RT_VERSION);
    CHECK(a && b);
    if (a && b) {
        DWORD sa = SizeofResource(img, a), sb = SizeofResource(df, b);
        CHECK(sa == sb && sa > 0);
        CHECK(!memcmp(LockResource(LoadResource(img, a)), LockResource(LoadResource(df, b)), sa));
    }
    FreeLibrary(df); FreeLibrary(img);
}
TEST(res_system_dll_message_table_and_strings) {
    REQUIRE(U(FindResourceExW));
    HMODULE k = t_kernelbase();
    /* kernelbase carries the system message table or MUI redirection; at least the version exists */
    HRSRC r = U(FindResourceExW)(k, (LPWSTR)RT_VERSION, MAKEINTRESOURCEW(1), 0);
    CHECK(r != NULL);
    HMODULE u = LoadLibraryA("user32.dll");
    SET_ERR(0);
    r = U(FindResourceExW)(u, (LPWSTR)RT_STRING, MAKEINTRESOURCEW(0x7FFF), 0);
    CHECK(r == NULL);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_RESOURCE_NAME_NOT_FOUND || e == ERROR_RESOURCE_TYPE_NOT_FOUND, "e=%lu", e);
    FreeLibrary(u);
}
TEST(res_load_after_free_image_not_dereferenced) {
    REQUIRE(U(FindResourceA));
    HMODULE h = res_open(0, "res_af", "k32m_res.dll"); if (!h) { CHECK(0); return; }
    HRSRC r = U(FindResourceA)(h, MAKEINTRESOURCEA(1), RT_RCDATA);
    CHECK(r != NULL);
    res_close(h);
    CHECK(!ml_is_loaded(h));
}

/* ---- enumeration ---- */
typedef struct { int n; int is_str[32]; ULONG_PTR id[32]; char s[32][40]; WORD lang[32]; HMODULE seen_h; LONG_PTR seen_l; int stop_at; int wide; } coll;
static void coll_add(coll *c, const void *item, int wide) {
    if (c->n >= 32) return;
    ULONG_PTR v = (ULONG_PTR)item;
    if (v > 0xFFFF) {
        c->is_str[c->n] = 1;
        if (wide) WideCharToMultiByte(CP_ACP, 0, item, -1, c->s[c->n], 40, NULL, NULL); else strncpy(c->s[c->n], item, 39);
    } else c->id[c->n] = v;
    c->n++;
}
static int coll_has_id(coll *c, ULONG_PTR id) { for (int i = 0; i < c->n; i++) if (!c->is_str[i] && c->id[i] == id) return 1; return 0; }
static int coll_has_str(coll *c, const char *s) { for (int i = 0; i < c->n; i++) if (c->is_str[i] && !_stricmp(c->s[i], s)) return 1; return 0; }
static BOOL CALLBACK type_cbA(HMODULE h, LPSTR type, LONG_PTR l) { coll *c = (coll *)l; c->seen_h = h; c->seen_l = l; coll_add(c, type, 0); return c->stop_at && c->n >= c->stop_at ? FALSE : TRUE; }
static BOOL CALLBACK type_cbW(HMODULE h, LPWSTR type, LONG_PTR l) { coll *c = (coll *)l; c->seen_h = h; coll_add(c, type, 1); return c->stop_at && c->n >= c->stop_at ? FALSE : TRUE; }
static BOOL CALLBACK name_cbA(HMODULE h, LPCSTR type, LPSTR name, LONG_PTR l) { coll *c = (coll *)l; c->seen_h = h; coll_add(c, name, 0); return c->stop_at && c->n >= c->stop_at ? FALSE : TRUE; }
static BOOL CALLBACK name_cbW(HMODULE h, LPCWSTR type, LPWSTR name, LONG_PTR l) { coll *c = (coll *)l; c->seen_h = h; coll_add(c, name, 1); return c->stop_at && c->n >= c->stop_at ? FALSE : TRUE; }
static BOOL CALLBACK lang_cbA(HMODULE h, LPCSTR type, LPCSTR name, WORD lang, LONG_PTR l) { coll *c = (coll *)l; c->seen_h = h; if (c->n < 32) c->lang[c->n++] = lang; return c->stop_at && c->n >= c->stop_at ? FALSE : TRUE; }
static BOOL CALLBACK lang_cbW(HMODULE h, LPCWSTR type, LPCWSTR name, WORD lang, LONG_PTR l) { coll *c = (coll *)l; c->seen_h = h; if (c->n < 32) c->lang[c->n++] = lang; return c->stop_at && c->n >= c->stop_at ? FALSE : TRUE; }

TEST(res_enum_types_a) {
    REQUIRE(U(EnumResourceTypesA));
    FOR_MODES("res_et") {
        coll c; memset(&c, 0, sizeof c);
        SET_ERR(0);
        CHECKF(U(EnumResourceTypesA)(h, type_cbA, (LONG_PTR)&c), "%s err %lu", mode_name[mode], GetLastError());
        EXPECT_EQ(c.n, 4);
        CHECK(coll_has_id(&c, 6) && coll_has_id(&c, 10) && coll_has_id(&c, 16));
        CHECK(coll_has_str(&c, "CUSTOMTYPE"));
        CHECK(c.seen_h == h);
        CHECK(c.n == 4 && c.is_str[0] == 1);                    /* named entries are listed first */
        if (c.n == 4) CHECK(c.id[1] == 6 && c.id[2] == 10 && c.id[3] == 16);   /* then ids ascending */
    }
}
TEST(res_enum_types_w) {
    REQUIRE(U(EnumResourceTypesW));
    FOR_MODES("res_etw") {
        coll c; memset(&c, 0, sizeof c);
        CHECK(U(EnumResourceTypesW)(h, type_cbW, (LONG_PTR)&c));
        EXPECT_EQ(c.n, 4);
        CHECK(coll_has_id(&c, 6) && coll_has_id(&c, 10) && coll_has_id(&c, 16) && coll_has_str(&c, "CUSTOMTYPE"));
        CHECK(c.seen_h == h);
    }
}
TEST(res_enum_types_ex) {
    REQUIRE(U(EnumResourceTypesExA)); REQUIRE(U(EnumResourceTypesExW));
    FOR_MODES("res_etx") {
        coll a, w; memset(&a, 0, sizeof a); memset(&w, 0, sizeof w);
        CHECK(U(EnumResourceTypesExA)(h, type_cbA, (LONG_PTR)&a, 0, 0));
        CHECK(U(EnumResourceTypesExW)(h, type_cbW, (LONG_PTR)&w, 0, 0));
        EXPECT_EQ(a.n, 4); EXPECT_EQ(w.n, 4);
        CHECK(U(EnumResourceTypesExA)(h, type_cbA, (LONG_PTR)&a, RESOURCE_ENUM_LN, 0));
        a.n = 0;
        CHECK(U(EnumResourceTypesExA)(h, type_cbA, (LONG_PTR)&a, RESOURCE_ENUM_LN, 0));
        EXPECT_EQ(a.n, 4);
        a.n = 0;
        CHECK(U(EnumResourceTypesExA)(h, type_cbA, (LONG_PTR)&a, RESOURCE_ENUM_LN | RESOURCE_ENUM_MUI, 0));
        t_note("%s: ENUM_LN|ENUM_MUI on image without .mui -> %d items", mode_name[mode], a.n);
        a.n = 0;
        BOOL ok = U(EnumResourceTypesExA)(h, type_cbA, (LONG_PTR)&a, RESOURCE_ENUM_MUI, 0);
        t_note("%s: ENUM_MUI only -> %d err %lu items %d", mode_name[mode], ok, ok ? 0 : GetLastError(), a.n);
    }
}
TEST(res_enum_types_early_stop) {
    REQUIRE(U(EnumResourceTypesA)); REQUIRE(U(EnumResourceTypesW));
    FOR_MODES("res_ets") {
        coll c; memset(&c, 0, sizeof c); c.stop_at = 1;
        SET_ERR(0);
        CHECK(!U(EnumResourceTypesA)(h, type_cbA, (LONG_PTR)&c));
        EXPECT_ERR(ERROR_RESOURCE_ENUM_USER_STOP);
        EXPECT_EQ(c.n, 1);
        memset(&c, 0, sizeof c); c.stop_at = 3;
        SET_ERR(0);
        CHECK(!U(EnumResourceTypesW)(h, type_cbW, (LONG_PTR)&c));
        EXPECT_ERR(ERROR_RESOURCE_ENUM_USER_STOP);
        EXPECT_EQ(c.n, 3);
    }
}
TEST(res_enum_names_a_w) {
    REQUIRE(U(EnumResourceNamesA)); REQUIRE(U(EnumResourceNamesW));
    FOR_MODES("res_en") {
        coll a, w; memset(&a, 0, sizeof a); memset(&w, 0, sizeof w);
        SET_ERR(0);
        CHECKF(U(EnumResourceNamesA)(h, RT_RCDATA, name_cbA, (LONG_PTR)&a), "%s err %lu", mode_name[mode], GetLastError());
        CHECK(U(EnumResourceNamesW)(h, (LPWSTR)RT_RCDATA, name_cbW, (LONG_PTR)&w));
        EXPECT_EQ(a.n, 3); EXPECT_EQ(w.n, 3);
        CHECK(coll_has_str(&a, "NAMED") && coll_has_id(&a, 1) && coll_has_id(&a, 7));
        CHECK(coll_has_str(&w, "NAMED") && coll_has_id(&w, 1) && coll_has_id(&w, 7));
        CHECK(a.seen_h == h && w.seen_h == h);
        if (a.n == 3) CHECK(a.is_str[0] && a.id[1] == 1 && a.id[2] == 7);
        coll s; memset(&s, 0, sizeof s);
        CHECK(U(EnumResourceNamesA)(h, RT_STRING, name_cbA, (LONG_PTR)&s));
        EXPECT_EQ(s.n, 2); CHECK(coll_has_id(&s, 1) && coll_has_id(&s, 2));
        memset(&s, 0, sizeof s);
        CHECK(U(EnumResourceNamesA)(h, "CUSTOMTYPE", name_cbA, (LONG_PTR)&s));
        EXPECT_EQ(s.n, 2); CHECK(coll_has_str(&s, "CT2") && coll_has_id(&s, 200));
        memset(&s, 0, sizeof s);
        CHECK(U(EnumResourceNamesW)(h, L"customtype", name_cbW, (LONG_PTR)&s));
        EXPECT_EQ(s.n, 2);
        memset(&s, 0, sizeof s);
        CHECK(U(EnumResourceNamesA)(h, "#16", name_cbA, (LONG_PTR)&s));
        EXPECT_EQ(s.n, 1);
    }
}
TEST(res_enum_names_ex) {
    REQUIRE(U(EnumResourceNamesExA)); REQUIRE(U(EnumResourceNamesExW));
    FOR_MODES("res_enx") {
        coll a, w; memset(&a, 0, sizeof a); memset(&w, 0, sizeof w);
        CHECK(U(EnumResourceNamesExA)(h, RT_RCDATA, name_cbA, (LONG_PTR)&a, 0, 0));
        CHECK(U(EnumResourceNamesExW)(h, (LPWSTR)RT_RCDATA, name_cbW, (LONG_PTR)&w, RESOURCE_ENUM_LN, 0));
        EXPECT_EQ(a.n, 3); EXPECT_EQ(w.n, 3);
    }
}
TEST(res_enum_names_errors) {
    REQUIRE(U(EnumResourceNamesA)); REQUIRE(U(EnumResourceNamesW));
    FOR_MODES("res_enerr") {
        coll c; memset(&c, 0, sizeof c);
        SET_ERR(0);
        CHECK(!U(EnumResourceNamesA)(h, MAKEINTRESOURCEA(9999), name_cbA, (LONG_PTR)&c));
        EXPECT_ERR(ERROR_RESOURCE_TYPE_NOT_FOUND);
        SET_ERR(0);
        CHECK(!U(EnumResourceNamesW)(h, L"NOSUCHTYPE", name_cbW, (LONG_PTR)&c));
        EXPECT_ERR(ERROR_RESOURCE_TYPE_NOT_FOUND);
        SET_ERR(0);
        CHECK(!U(EnumResourceNamesA)(h, RT_ICON, name_cbA, (LONG_PTR)&c));
        EXPECT_ERR(ERROR_RESOURCE_TYPE_NOT_FOUND);
        EXPECT_EQ(c.n, 0);
    }
}
TEST(res_enum_names_early_stop) {
    REQUIRE(U(EnumResourceNamesA));
    FOR_MODES("res_enstop") {
        coll c; memset(&c, 0, sizeof c); c.stop_at = 2;
        SET_ERR(0);
        CHECK(!U(EnumResourceNamesA)(h, RT_RCDATA, name_cbA, (LONG_PTR)&c));
        EXPECT_ERR(ERROR_RESOURCE_ENUM_USER_STOP);
        EXPECT_EQ(c.n, 2);
    }
}
TEST(res_enum_languages) {
    REQUIRE(U(EnumResourceLanguagesA)); REQUIRE(U(EnumResourceLanguagesW));
    FOR_MODES("res_el") {
        coll a, w; memset(&a, 0, sizeof a); memset(&w, 0, sizeof w);
        SET_ERR(0);
        CHECKF(U(EnumResourceLanguagesA)(h, RT_RCDATA, MAKEINTRESOURCEA(7), lang_cbA, (LONG_PTR)&a), "%s err %lu", mode_name[mode], GetLastError());
        CHECK(U(EnumResourceLanguagesW)(h, (LPWSTR)RT_RCDATA, MAKEINTRESOURCEW(7), lang_cbW, (LONG_PTR)&w));
        EXPECT_EQ(a.n, 2); EXPECT_EQ(w.n, 2);
        if (a.n == 2) { CHECK(a.lang[0] == 0x407 && a.lang[1] == 0x409); }
        if (w.n == 2) { CHECK(w.lang[0] == 0x407 && w.lang[1] == 0x409); }
        CHECK(a.seen_h == h);
        coll n; memset(&n, 0, sizeof n);
        CHECK(U(EnumResourceLanguagesA)(h, RT_RCDATA, MAKEINTRESOURCEA(1), lang_cbA, (LONG_PTR)&n));
        EXPECT_EQ(n.n, 1); CHECK(n.lang[0] == 0x409);
        memset(&n, 0, sizeof n);
        CHECK(U(EnumResourceLanguagesA)(h, RT_VERSION, MAKEINTRESOURCEA(1), lang_cbA, (LONG_PTR)&n));
        EXPECT_EQ(n.n, 1); CHECK(n.lang[0] == 0);                 /* neutral */
        memset(&n, 0, sizeof n);
        CHECK(U(EnumResourceLanguagesA)(h, "CUSTOMTYPE", "ct2", lang_cbA, (LONG_PTR)&n));
        EXPECT_EQ(n.n, 1);
        memset(&n, 0, sizeof n);
        CHECK(U(EnumResourceLanguagesW)(h, (LPWSTR)RT_RCDATA, L"named", lang_cbW, (LONG_PTR)&n));
        EXPECT_EQ(n.n, 1);
    }
}
TEST(res_enum_languages_ex) {
    REQUIRE(U(EnumResourceLanguagesExA)); REQUIRE(U(EnumResourceLanguagesExW));
    FOR_MODES("res_elx") {
        coll a, w; memset(&a, 0, sizeof a); memset(&w, 0, sizeof w);
        CHECK(U(EnumResourceLanguagesExA)(h, RT_RCDATA, MAKEINTRESOURCEA(7), lang_cbA, (LONG_PTR)&a, 0, 0));
        CHECK(U(EnumResourceLanguagesExW)(h, (LPWSTR)RT_RCDATA, MAKEINTRESOURCEW(7), lang_cbW, (LONG_PTR)&w, RESOURCE_ENUM_LN, 0));
        EXPECT_EQ(a.n, 2); EXPECT_EQ(w.n, 2);
    }
}
TEST(res_enum_languages_errors) {
    REQUIRE(U(EnumResourceLanguagesA)); REQUIRE(U(EnumResourceLanguagesW));
    FOR_MODES("res_elerr") {
        coll c; memset(&c, 0, sizeof c);
        SET_ERR(0);
        CHECK(!U(EnumResourceLanguagesA)(h, MAKEINTRESOURCEA(9999), MAKEINTRESOURCEA(1), lang_cbA, (LONG_PTR)&c));
        EXPECT_ERR(ERROR_RESOURCE_TYPE_NOT_FOUND);
        SET_ERR(0);
        CHECK(!U(EnumResourceLanguagesA)(h, RT_RCDATA, MAKEINTRESOURCEA(9999), lang_cbA, (LONG_PTR)&c));
        EXPECT_ERR(ERROR_RESOURCE_NAME_NOT_FOUND);
        SET_ERR(0);
        CHECK(!U(EnumResourceLanguagesW)(h, (LPWSTR)RT_RCDATA, L"NOSUCH", lang_cbW, (LONG_PTR)&c));
        EXPECT_ERR(ERROR_RESOURCE_NAME_NOT_FOUND);
        EXPECT_EQ(c.n, 0);
    }
}
TEST(res_enum_languages_early_stop) {
    REQUIRE(U(EnumResourceLanguagesA));
    FOR_MODES("res_elstop") {
        coll c; memset(&c, 0, sizeof c); c.stop_at = 1;
        SET_ERR(0);
        CHECK(!U(EnumResourceLanguagesA)(h, RT_RCDATA, MAKEINTRESOURCEA(7), lang_cbA, (LONG_PTR)&c));
        EXPECT_ERR(ERROR_RESOURCE_ENUM_USER_STOP);
        EXPECT_EQ(c.n, 1);
    }
}
TEST(res_enum_lparam_passthrough) {
    REQUIRE(U(EnumResourceTypesA));
    HMODULE h = res_open(0, "res_lp", "k32m_res.dll"); if (!h) { CHECK(0); return; }
    coll c; memset(&c, 0, sizeof c);
    U(EnumResourceTypesA)(h, type_cbA, (LONG_PTR)&c);
    CHECK(c.seen_l == (LONG_PTR)&c);
    res_close(h);
}
TEST(res_enum_exe_without_resources) {
    REQUIRE(U(EnumResourceTypesA));
    coll c; memset(&c, 0, sizeof c);
    SET_ERR(0);
    BOOL ok = U(EnumResourceTypesA)(NULL, type_cbA, (LONG_PTR)&c);
    if (!ok) { DWORD e = GetLastError(); CHECKF(e == ERROR_RESOURCE_DATA_NOT_FOUND, "e=%lu", e); EXPECT_EQ(c.n, 0); }
    else t_note("test exe has %d resource types", c.n);
}
TEST(res_enum_system_dll_types) {
    REQUIRE(U(EnumResourceTypesW)); REQUIRE(U(EnumResourceNamesW));
    HMODULE k = t_kernel32();
    coll c; memset(&c, 0, sizeof c);
    SET_ERR(0);
    CHECK(U(EnumResourceTypesW)(k, type_cbW, (LONG_PTR)&c));
    CHECK(c.n > 0);
    CHECK(coll_has_id(&c, 16));                      /* RT_VERSION */
    coll n; memset(&n, 0, sizeof n);
    CHECK(U(EnumResourceNamesW)(k, (LPWSTR)RT_VERSION, name_cbW, (LONG_PTR)&n));
    CHECK(n.n == 1 && coll_has_id(&n, 1));
    coll l; memset(&l, 0, sizeof l);
    CHECK(U(EnumResourceLanguagesW)(k, (LPWSTR)RT_VERSION, MAKEINTRESOURCEW(1), lang_cbW, (LONG_PTR)&l));
    CHECK(l.n >= 1);
}
TEST(res_enum_datafile_equals_image_for_system_dll) {
    REQUIRE(U(EnumResourceTypesW));
    char p[MAX_PATH]; GetSystemDirectoryA(p, sizeof p); strcat(p, "\\kernel32.dll");
    HMODULE df = LoadLibraryExA(p, NULL, LOAD_LIBRARY_AS_DATAFILE);
    CHECK(df != NULL); if (!df) return;
    coll a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    CHECK(U(EnumResourceTypesW)(t_kernel32(), type_cbW, (LONG_PTR)&a));
    CHECK(U(EnumResourceTypesW)(df, type_cbW, (LONG_PTR)&b));
    EXPECT_EQ(a.n, b.n);
    for (int i = 0; i < a.n && i < b.n; i++) CHECK(a.is_str[i] == b.is_str[i] && a.id[i] == b.id[i] && !strcmp(a.s[i], b.s[i]));
    CHECK(b.seen_h == df);
    FreeLibrary(df);
}
