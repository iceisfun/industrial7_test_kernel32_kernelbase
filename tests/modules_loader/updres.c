/* BeginUpdateResource / UpdateResource / EndUpdateResource on private copies of images */
#include "ml.h"

static char g_upd[MAX_PATH];

static int upd_fresh(const char *sub, const char *leaf) { return ml_put(ML_A, ml_dir(sub), leaf, g_upd); }

/* Load as data file and fetch a resource copy; returns malloc'd buffer or NULL (sets *err). */
static void *upd_get(const char *path, LPCSTR type, LPCSTR name, WORD lang, DWORD *size, DWORD *err) {
    HMODULE h = LoadLibraryExA(path, NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (!h) { if (err) *err = GetLastError(); return NULL; }
    HRSRC r = FindResourceExA(h, type, name, lang);
    void *out = NULL;
    if (!r) { if (err) *err = GetLastError(); }
    else {
        DWORD sz = SizeofResource(h, r); const void *p = LockResource(LoadResource(h, r));
        out = malloc(sz ? sz : 1); if (out && p) memcpy(out, p, sz);
        if (size) *size = sz;
    }
    FreeLibrary(h);
    return out;
}
static void *slurp(const char *path, DWORD *n) {
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return NULL;
    DWORD sz = GetFileSize(f, NULL), rd = 0; void *b = malloc(sz ? sz : 1);
    if (b) ReadFile(f, b, sz, &rd, NULL);
    CloseHandle(f);
    if (n) *n = sz;
    return b;
}
static int still_loadable_and_working(const char *path) {
    HMODULE m = LoadLibraryA(path);
    if (!m) return 0;
    typedef int (WINAPI *add_fn)(int, int);
    add_fn f = (add_fn)GetProcAddress(m, "ml_add");
    int ok = f && f(20, 22) == 42;
    ml_unload(m);
    return ok;
}
static BOOL CALLBACK lcb(HMODULE hm, LPCSTR t2, LPCSTR n, WORD l, LONG_PTR p) { (*(int *)p)++; return TRUE; }
typedef struct { int n_types; int has_id[64]; int ids[64]; int n_names; } enumst;
static BOOL CALLBACK en_types(HMODULE h, LPSTR t, LONG_PTR l) { enumst *e = (enumst *)l; if ((ULONG_PTR)t <= 0xFFFF && e->n_types < 64) e->ids[e->n_types] = (int)(ULONG_PTR)t; e->n_types++; return TRUE; }
static BOOL CALLBACK en_names(HMODULE h, LPCSTR t, LPSTR n, LONG_PTR l) { ((enumst *)l)->n_names++; return TRUE; }
static int type_present(enumst *e, int id) { for (int i = 0; i < e->n_types && i < 64; i++) if (e->ids[i] == id) return 1; return 0; }

/* ---- Begin: argument errors ---- */
TEST(upd_begin_null_or_empty_name) {
    if (t_is_wine()) { t_skip("emulator faults on NULL arguments"); return; }
    REQUIRE(U(BeginUpdateResourceA)); REQUIRE(U(BeginUpdateResourceW));
    SET_ERR(0);
    CHECK(U(BeginUpdateResourceA)(NULL, FALSE) == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(U(BeginUpdateResourceW)(NULL, TRUE) == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(U(BeginUpdateResourceA)("", FALSE) == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(U(BeginUpdateResourceW)(L"", TRUE) == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(upd_begin_nonexistent_file) {
    REQUIRE(U(BeginUpdateResourceA)); REQUIRE(U(BeginUpdateResourceW));
    char p[MAX_PATH]; snprintf(p, sizeof p, "%smissing.dll", ml_dir("upd_ne"));
    SET_ERR(0);
    CHECK(U(BeginUpdateResourceA)(p, FALSE) == NULL);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    SET_ERR(0);
    CHECK(U(BeginUpdateResourceW)(ml_w(p), TRUE) == NULL);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    snprintf(p, sizeof p, "%sno_such_dir\\x.dll", ml_dir("upd_ne2"));
    SET_ERR(0);
    CHECK(U(BeginUpdateResourceA)(p, FALSE) == NULL);
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(upd_begin_non_pe_file) {
    REQUIRE(U(BeginUpdateResourceA));
    char p[MAX_PATH]; snprintf(p, sizeof p, "%snotpe.exe", ml_dir("upd_npe"));
    CHECK(ml_write(p, "plain text file, not an executable image.........................\n", 64));
    SET_ERR(0);
    CHECK(U(BeginUpdateResourceA)(p, FALSE) == NULL);
    EXPECT_ERR(ERROR_BAD_EXE_FORMAT);
    SET_ERR(0);
    CHECK(U(BeginUpdateResourceA)(p, TRUE) == NULL);
    EXPECT_ERR(ERROR_BAD_EXE_FORMAT);
}
TEST(upd_begin_empty_file) {
    REQUIRE(U(BeginUpdateResourceA));
    char p[MAX_PATH]; snprintf(p, sizeof p, "%sempty.exe", ml_dir("upd_emp"));
    CHECK(ml_write(p, "", 0));
    SET_ERR(0);
    CHECK(U(BeginUpdateResourceA)(p, FALSE) == NULL);
    CHECK(GetLastError() != 0);
    t_note("BeginUpdateResource(empty file) error %lu", GetLastError());
}
TEST(upd_begin_readonly_hidden_system_directory) {
    REQUIRE(U(BeginUpdateResourceA));
    CHECK(upd_fresh("upd_ro", "k32m_ro.dll"));
    static const DWORD attr[] = { FILE_ATTRIBUTE_READONLY, FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_SYSTEM };
    for (int i = 0; i < 3; i++) {
        SetFileAttributesA(g_upd, attr[i]);
        SET_ERR(0);
        HANDLE h = U(BeginUpdateResourceA)(g_upd, FALSE);
        CHECKF(h == NULL, "attr %lx", attr[i]);
        EXPECT_ERR(ERROR_WRITE_PROTECT);
        if (h) U(EndUpdateResourceA)(h, TRUE);
    }
    SetFileAttributesA(g_upd, FILE_ATTRIBUTE_NORMAL);
    const char *d = ml_dir("upd_dir"); char dd[MAX_PATH]; strcpy(dd, d); dd[strlen(dd) - 1] = 0;
    SET_ERR(0);
    CHECK(U(BeginUpdateResourceA)(dd, FALSE) == NULL);
    EXPECT_ERR(ERROR_WRITE_PROTECT);
}
TEST(upd_begin_returns_handle_and_end_discard) {
    REQUIRE(U(BeginUpdateResourceA)); REQUIRE(U(EndUpdateResourceA));
    CHECK(upd_fresh("upd_bd", "k32m_bd.dll"));
    HANDLE h = U(BeginUpdateResourceA)(g_upd, FALSE);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (!h) return;
    SET_ERR(0x1234);
    CHECK(U(EndUpdateResourceA)(h, TRUE));
    EXPECT_ERR(0);
}
TEST(upd_begin_w_variant) {
    REQUIRE(U(BeginUpdateResourceW)); REQUIRE(U(EndUpdateResourceW));
    CHECK(upd_fresh("upd_bw", "k32m_bw.dll"));
    HANDLE h = U(BeginUpdateResourceW)(ml_w(g_upd), TRUE);
    CHECK(h != NULL);
    if (h) CHECK(U(EndUpdateResourceW)(h, TRUE));
}

/* ---- Update / End argument errors ---- */
TEST(upd_update_null_handle) {
    REQUIRE(U(UpdateResourceA)); REQUIRE(U(UpdateResourceW));
    static char data[4] = "abc";
    SET_ERR(0);
    CHECK(!U(UpdateResourceA)(NULL, RT_RCDATA, MAKEINTRESOURCEA(1), 0, data, 4));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!U(UpdateResourceW)(NULL, (LPWSTR)RT_RCDATA, MAKEINTRESOURCEW(1), 0, data, 4));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(upd_end_null_handle) {
    REQUIRE(U(EndUpdateResourceA)); REQUIRE(U(EndUpdateResourceW));
    SET_ERR(0);
    CHECK(!U(EndUpdateResourceA)(NULL, FALSE));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!U(EndUpdateResourceW)(NULL, TRUE));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(upd_update_data_size_mismatch) {
    REQUIRE(U(BeginUpdateResourceA)); REQUIRE(U(UpdateResourceA)); REQUIRE(U(EndUpdateResourceA));
    CHECK(upd_fresh("upd_mm", "k32m_mm.dll"));
    HANDLE h = U(BeginUpdateResourceA)(g_upd, FALSE);
    CHECK(h != NULL); if (!h) return;
    static char data[4] = "abc";
    SET_ERR(0);
    CHECK(!U(UpdateResourceA)(h, RT_RCDATA, MAKEINTRESOURCEA(500), 0x409, data, 0));       /* data with zero size */
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!U(UpdateResourceA)(h, RT_RCDATA, MAKEINTRESOURCEA(500), 0x409, NULL, 4));       /* size without data */
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CHECK(U(EndUpdateResourceA)(h, TRUE));
}

/* ---- discard keeps file untouched ---- */
TEST(upd_discard_leaves_file_unchanged) {
    REQUIRE(U(BeginUpdateResourceA)); REQUIRE(U(UpdateResourceA)); REQUIRE(U(EndUpdateResourceA));
    CHECK(upd_fresh("upd_disc", "k32m_disc.dll"));
    DWORD n0; void *before = slurp(g_upd, &n0);
    HANDLE h = U(BeginUpdateResourceA)(g_upd, FALSE);
    CHECK(h != NULL); if (!h) { free(before); return; }
    static const char payload[] = "payload-disc";
    CHECK(U(UpdateResourceA)(h, RT_RCDATA, MAKEINTRESOURCEA(500), 0x409, (LPVOID)payload, sizeof payload));
    DWORD n1; void *mid = slurp(g_upd, &n1);
    CHECK(n1 == n0 && mid && before && !memcmp(mid, before, n0));        /* nothing written before End */
    CHECK(U(EndUpdateResourceA)(h, TRUE));
    DWORD n2; void *after = slurp(g_upd, &n2);
    CHECK(n2 == n0 && after && before && !memcmp(after, before, n0));
    DWORD err = 0;
    CHECK(upd_get(g_upd, RT_RCDATA, MAKEINTRESOURCEA(500), 0x409, NULL, &err) == NULL);
    CHECK(err == ERROR_RESOURCE_NAME_NOT_FOUND);
    free(before); free(mid); free(after);
}
TEST(upd_begin_end_no_changes) {
    REQUIRE(U(BeginUpdateResourceA)); REQUIRE(U(EndUpdateResourceA));
    CHECK(upd_fresh("upd_nc", "k32m_nc.dll"));
    HANDLE h = U(BeginUpdateResourceA)(g_upd, FALSE);
    CHECK(h != NULL); if (!h) return;
    CHECK(U(EndUpdateResourceA)(h, FALSE));
    CHECK(still_loadable_and_working(g_upd));
    DWORD sz; void *d = upd_get(g_upd, RT_RCDATA, MAKEINTRESOURCEA(1), 0x409, &sz, NULL);
    CHECK(d && sz == 8 && !memcmp(d, "ABCDEFGH", 8));
    free(d);
}

/* ---- successful modifications ---- */
TEST(upd_add_rcdata_a) {
    REQUIRE(U(BeginUpdateResourceA)); REQUIRE(U(UpdateResourceA)); REQUIRE(U(EndUpdateResourceA));
    CHECK(upd_fresh("upd_add", "k32m_add.dll"));
    HANDLE h = U(BeginUpdateResourceA)(g_upd, FALSE);
    CHECK(h != NULL); if (!h) return;
    static const char payload[] = "hello-update";
    SET_ERR(0x555);
    CHECK(U(UpdateResourceA)(h, RT_RCDATA, MAKEINTRESOURCEA(500), 0x409, (LPVOID)payload, sizeof payload));
    EXPECT_ERR(0);
    CHECKF(U(EndUpdateResourceA)(h, FALSE), "err %lu", GetLastError());
    DWORD sz = 0; void *d = upd_get(g_upd, RT_RCDATA, MAKEINTRESOURCEA(500), 0x409, &sz, NULL);
    CHECK(d != NULL);
    if (d) { EXPECT_EQ(sz, sizeof payload); CHECK(!memcmp(d, payload, sizeof payload)); free(d); }
    CHECK(still_loadable_and_working(g_upd));
    /* original resources survive when bDeleteExistingResources is FALSE */
    d = upd_get(g_upd, RT_RCDATA, MAKEINTRESOURCEA(1), 0x409, &sz, NULL);
    CHECK(d && sz == 8 && !memcmp(d, "ABCDEFGH", 8));
    free(d);
    d = upd_get(g_upd, RT_VERSION, MAKEINTRESOURCEA(1), 0, &sz, NULL);
    CHECK(d && sz == 332);
    free(d);
}
TEST(upd_add_rcdata_w_with_string_name_and_type) {
    REQUIRE(U(BeginUpdateResourceW)); REQUIRE(U(UpdateResourceW)); REQUIRE(U(EndUpdateResourceW));
    CHECK(upd_fresh("upd_addw", "k32m_addw.dll"));
    HANDLE h = U(BeginUpdateResourceW)(ml_w(g_upd), FALSE);
    CHECK(h != NULL); if (!h) return;
    static const char payload[] = "w-payload";
    CHECK(U(UpdateResourceW)(h, L"MyType", L"MyName", 0x409, (LPVOID)payload, sizeof payload));
    CHECK(U(UpdateResourceW)(h, (LPWSTR)RT_RCDATA, L"AnotherName", 0, (LPVOID)payload, 4));
    CHECKF(U(EndUpdateResourceW)(h, FALSE), "err %lu", GetLastError());
    DWORD sz = 0; void *d = upd_get(g_upd, "MYTYPE", "MYNAME", 0x409, &sz, NULL);
    CHECK(d && sz == sizeof payload && !memcmp(d, payload, sz)); free(d);
    d = upd_get(g_upd, "mytype", "myname", 0x409, &sz, NULL);        /* case-insensitive */
    CHECK(d != NULL); free(d);
    d = upd_get(g_upd, RT_RCDATA, "ANOTHERNAME", 0, &sz, NULL);
    CHECK(d && sz == 4 && !memcmp(d, "w-pa", 4)); free(d);
    CHECK(still_loadable_and_working(g_upd));
}
TEST(upd_replace_existing_resource) {
    REQUIRE(U(BeginUpdateResourceA)); REQUIRE(U(UpdateResourceA)); REQUIRE(U(EndUpdateResourceA));
    CHECK(upd_fresh("upd_rep", "k32m_rep.dll"));
    HANDLE h = U(BeginUpdateResourceA)(g_upd, FALSE);
    CHECK(h != NULL); if (!h) return;
    CHECK(U(UpdateResourceA)(h, RT_RCDATA, MAKEINTRESOURCEA(1), 0x409, "XYZ", 3));
    CHECK(U(UpdateResourceA)(h, RT_RCDATA, MAKEINTRESOURCEA(1), 0x409, "QRSTU", 5));    /* later update wins */
    CHECK(U(EndUpdateResourceA)(h, FALSE));
    DWORD sz = 0; void *d = upd_get(g_upd, RT_RCDATA, MAKEINTRESOURCEA(1), 0x409, &sz, NULL);
    CHECK(d && sz == 5 && !memcmp(d, "QRSTU", 5)); free(d);
    d = upd_get(g_upd, RT_RCDATA, MAKEINTRESOURCEA(7), 0x407, &sz, NULL);
    CHECK(d && sz == 8 && !memcmp(d, "DEUTSCH!", 8)); free(d);
    CHECK(still_loadable_and_working(g_upd));
}
TEST(upd_delete_single_resource) {
    REQUIRE(U(BeginUpdateResourceA)); REQUIRE(U(UpdateResourceA)); REQUIRE(U(EndUpdateResourceA));
    CHECK(upd_fresh("upd_del1", "k32m_del1.dll"));
    HANDLE h = U(BeginUpdateResourceA)(g_upd, FALSE);
    CHECK(h != NULL); if (!h) return;
    CHECKF(U(UpdateResourceA)(h, RT_RCDATA, MAKEINTRESOURCEA(1), 0x409, NULL, 0), "err %lu", GetLastError());   /* NULL data deletes */
    CHECK(U(EndUpdateResourceA)(h, FALSE));
    DWORD err = 0;
    CHECK(upd_get(g_upd, RT_RCDATA, MAKEINTRESOURCEA(1), 0x409, NULL, &err) == NULL);
    CHECK(err == ERROR_RESOURCE_NAME_NOT_FOUND);
    DWORD sz; void *d = upd_get(g_upd, RT_RCDATA, MAKEINTRESOURCEA(7), 0x409, &sz, NULL);
    CHECK(d && sz == 2); free(d);
    CHECK(still_loadable_and_working(g_upd));
}
TEST(upd_delete_existing_resources_flag) {
    REQUIRE(U(BeginUpdateResourceA)); REQUIRE(U(UpdateResourceA)); REQUIRE(U(EndUpdateResourceA));
    CHECK(upd_fresh("upd_delall", "k32m_delall.dll"));
    HANDLE h = U(BeginUpdateResourceA)(g_upd, TRUE);
    CHECK(h != NULL); if (!h) return;
    CHECK(U(UpdateResourceA)(h, RT_RCDATA, MAKEINTRESOURCEA(900), 0x409, "only", 4));
    CHECKF(U(EndUpdateResourceA)(h, FALSE), "err %lu", GetLastError());
    HMODULE m = LoadLibraryExA(g_upd, NULL, LOAD_LIBRARY_AS_DATAFILE);
    CHECK(m != NULL); if (m) {
        enumst e; memset(&e, 0, sizeof e);
        CHECK(EnumResourceTypesA(m, en_types, (LONG_PTR)&e));
        EXPECT_EQ(e.n_types, 1);
        CHECK(type_present(&e, 10));
        enumst nm; memset(&nm, 0, sizeof nm);
        CHECK(EnumResourceNamesA(m, RT_RCDATA, en_names, (LONG_PTR)&nm));
        EXPECT_EQ(nm.n_names, 1);
        FreeLibrary(m);
    }
    CHECK(still_loadable_and_working(g_upd));
}
TEST(upd_delete_all_existing_then_nothing_added) {
    REQUIRE(U(BeginUpdateResourceA)); REQUIRE(U(EndUpdateResourceA));
    CHECK(upd_fresh("upd_delnone", "k32m_dn.dll"));
    HANDLE h = U(BeginUpdateResourceA)(g_upd, TRUE);
    CHECK(h != NULL); if (!h) return;
    CHECKF(U(EndUpdateResourceA)(h, FALSE), "err %lu", GetLastError());
    SET_ERR(0);
    HMODULE m = LoadLibraryExA(g_upd, NULL, LOAD_LIBRARY_AS_DATAFILE);
    CHECK(m != NULL);
    if (m) {
        SET_ERR(0);
        CHECK(FindResourceA(m, MAKEINTRESOURCEA(1), RT_RCDATA) == NULL);
        DWORD e = GetLastError();
        CHECKF(e == ERROR_RESOURCE_DATA_NOT_FOUND || e == ERROR_RESOURCE_TYPE_NOT_FOUND, "e=%lu", e);
        FreeLibrary(m);
    }
}
TEST(upd_multiple_languages_same_id) {
    REQUIRE(U(BeginUpdateResourceA)); REQUIRE(U(UpdateResourceA)); REQUIRE(U(EndUpdateResourceA));
    CHECK(upd_fresh("upd_ml", "k32m_ml.dll"));
    HANDLE h = U(BeginUpdateResourceA)(g_upd, TRUE);
    CHECK(h != NULL); if (!h) return;
    CHECK(U(UpdateResourceA)(h, RT_RCDATA, MAKEINTRESOURCEA(5), 0x409, "english", 7));
    CHECK(U(UpdateResourceA)(h, RT_RCDATA, MAKEINTRESOURCEA(5), 0x407, "deutsch", 7));
    CHECK(U(UpdateResourceA)(h, RT_RCDATA, MAKEINTRESOURCEA(5), 0x40C, "francais", 8));
    CHECK(U(UpdateResourceA)(h, RT_RCDATA, MAKEINTRESOURCEA(5), 0, "neutral", 7));
    CHECK(U(EndUpdateResourceA)(h, FALSE));
    static const struct { WORD l; const char *s; } t[] = { { 0x409, "english" }, { 0x407, "deutsch" }, { 0x40C, "francais" }, { 0, "neutral" } };
    for (int i = 0; i < 4; i++) {
        DWORD sz = 0; void *d = upd_get(g_upd, RT_RCDATA, MAKEINTRESOURCEA(5), t[i].l, &sz, NULL);
        CHECKF(d && sz == strlen(t[i].s) && !memcmp(d, t[i].s, sz), "lang %04x", t[i].l);
        free(d);
    }
    HMODULE m = LoadLibraryExA(g_upd, NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (m) {
        enumst e; memset(&e, 0, sizeof e);
        int cnt = 0; 
        CHECK(EnumResourceLanguagesA(m, RT_RCDATA, MAKEINTRESOURCEA(5), lcb, (LONG_PTR)&cnt));
        EXPECT_EQ(cnt, 4);
        FreeLibrary(m);
    }
}
TEST(upd_many_resources_one_session) {
    REQUIRE(U(BeginUpdateResourceA)); REQUIRE(U(UpdateResourceA)); REQUIRE(U(EndUpdateResourceA));
    CHECK(upd_fresh("upd_many", "k32m_many.dll"));
    HANDLE h = U(BeginUpdateResourceA)(g_upd, TRUE);
    CHECK(h != NULL); if (!h) return;
    for (int i = 1; i <= 200; i++) {
        char buf[32]; int n = snprintf(buf, sizeof buf, "item-%d", i);
        CHECKF(U(UpdateResourceA)(h, RT_RCDATA, MAKEINTRESOURCEA(i), 0x409, buf, n), "i=%d", i);
    }
    CHECKF(U(EndUpdateResourceA)(h, FALSE), "err %lu", GetLastError());
    HMODULE m = LoadLibraryExA(g_upd, NULL, LOAD_LIBRARY_AS_DATAFILE);
    CHECK(m != NULL);
    if (m) {
        enumst nm; memset(&nm, 0, sizeof nm);
        CHECK(EnumResourceNamesA(m, RT_RCDATA, en_names, (LONG_PTR)&nm));
        EXPECT_EQ(nm.n_names, 200);
        for (int i = 1; i <= 200; i += 37) {
            HRSRC r = FindResourceA(m, MAKEINTRESOURCEA(i), RT_RCDATA);
            char buf[32]; int n = snprintf(buf, sizeof buf, "item-%d", i);
            CHECKF(r && SizeofResource(m, r) == (DWORD)n && !memcmp(LockResource(LoadResource(m, r)), buf, n), "i=%d", i);
        }
        FreeLibrary(m);
    }
    CHECK(still_loadable_and_working(g_upd));
}
TEST(upd_large_resource) {
    REQUIRE(U(BeginUpdateResourceA)); REQUIRE(U(UpdateResourceA)); REQUIRE(U(EndUpdateResourceA));
    CHECK(upd_fresh("upd_big", "k32m_big.dll"));
    const DWORD N = 600 * 1024;
    unsigned char *b = malloc(N); for (DWORD i = 0; i < N; i++) b[i] = (unsigned char)(i * 7 + (i >> 8));
    HANDLE h = U(BeginUpdateResourceA)(g_upd, FALSE);
    CHECK(h != NULL); if (!h) { free(b); return; }
    CHECK(U(UpdateResourceA)(h, RT_RCDATA, MAKEINTRESOURCEA(777), 0x409, b, N));
    CHECKF(U(EndUpdateResourceA)(h, FALSE), "err %lu", GetLastError());
    DWORD sz = 0; void *d = upd_get(g_upd, RT_RCDATA, MAKEINTRESOURCEA(777), 0x409, &sz, NULL);
    CHECK(d && sz == N && !memcmp(d, b, N));
    free(d); free(b);
    CHECK(still_loadable_and_working(g_upd));
}
TEST(upd_string_and_version_resources) {
    REQUIRE(U(BeginUpdateResourceA)); REQUIRE(U(UpdateResourceA)); REQUIRE(U(EndUpdateResourceA));
    CHECK(upd_fresh("upd_str", "k32m_str.dll"));
    HANDLE h = U(BeginUpdateResourceA)(g_upd, FALSE);
    CHECK(h != NULL); if (!h) return;
    /* a string table block: id 3 holds strings 32..47; string index 0 = "zz" */
    WORD blk[16 + 2]; memset(blk, 0, sizeof blk); blk[0] = 2; blk[1] = 'z'; blk[2] = 'z';
    DWORD blksz = sizeof(WORD) * (3 + 15);
    CHECK(U(UpdateResourceA)(h, RT_STRING, MAKEINTRESOURCEA(3), 0x409, blk, blksz));
    CHECK(U(EndUpdateResourceA)(h, FALSE));
    DWORD sz = 0; WORD *d = upd_get(g_upd, RT_STRING, MAKEINTRESOURCEA(3), 0x409, &sz, NULL);
    CHECK(d && sz == blksz && d[0] == 2 && d[1] == 'z'); free(d);
    CHECK(still_loadable_and_working(g_upd));
}
TEST(upd_file_not_modified_until_end) {
    REQUIRE(U(BeginUpdateResourceA)); REQUIRE(U(UpdateResourceA)); REQUIRE(U(EndUpdateResourceA));
    CHECK(upd_fresh("upd_nm", "k32m_nm.dll"));
    WIN32_FILE_ATTRIBUTE_DATA a0, a1;
    GetFileAttributesExA(g_upd, GetFileExInfoStandard, &a0);
    DWORD nb0; void *bytes0 = slurp(g_upd, &nb0);
    HANDLE h = U(BeginUpdateResourceA)(g_upd, FALSE);
    CHECK(h != NULL); if (!h) return;
    CHECK(U(UpdateResourceA)(h, RT_RCDATA, MAKEINTRESOURCEA(42), 0x409, "x", 1));
    GetFileAttributesExA(g_upd, GetFileExInfoStandard, &a1);
    CHECK(a0.nFileSizeLow == a1.nFileSizeLow && !memcmp(&a0.ftLastWriteTime, &a1.ftLastWriteTime, sizeof(FILETIME)));
    CHECK(U(EndUpdateResourceA)(h, FALSE));
    GetFileAttributesExA(g_upd, GetFileExInfoStandard, &a1);
    DWORD nb1; void *bytes1 = slurp(g_upd, &nb1);
    CHECK(nb0 != nb1 || memcmp(bytes0, bytes1, nb0));
    free(bytes0); free(bytes1);
}
TEST(upd_end_leaves_no_temp_file) {
    REQUIRE(U(BeginUpdateResourceA)); REQUIRE(U(UpdateResourceA)); REQUIRE(U(EndUpdateResourceA));
    const char *d = ml_dir("upd_tmp"); char p[MAX_PATH];
    CHECK(ml_put(ML_A, d, "k32m_tmp.dll", p));
    HANDLE h = U(BeginUpdateResourceA)(p, FALSE);
    CHECK(h != NULL); if (!h) return;
    CHECK(U(UpdateResourceA)(h, RT_RCDATA, MAKEINTRESOURCEA(42), 0x409, "x", 1));
    CHECK(U(EndUpdateResourceA)(h, FALSE));
    char pat[MAX_PATH]; snprintf(pat, sizeof pat, "%s*", d);
    WIN32_FIND_DATAA fd; int n = 0;
    HANDLE f = FindFirstFileA(pat, &fd);
    if (f != INVALID_HANDLE_VALUE) { do { if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) { n++; if (strcmp(fd.cFileName, "k32m_tmp.dll")) t_note("leftover %s", fd.cFileName); } } while (FindNextFileA(f, &fd)); FindClose(f); }
    EXPECT_EQ(n, 1);
}
TEST(upd_end_fails_when_target_locked) {
    REQUIRE(U(BeginUpdateResourceA)); REQUIRE(U(UpdateResourceA)); REQUIRE(U(EndUpdateResourceA));
    CHECK(upd_fresh("upd_lock", "k32m_lock.dll"));
    HANDLE h = U(BeginUpdateResourceA)(g_upd, FALSE);
    CHECK(h != NULL); if (!h) return;
    CHECK(U(UpdateResourceA)(h, RT_RCDATA, MAKEINTRESOURCEA(42), 0x409, "x", 1));
    HMODULE m = LoadLibraryA(g_upd);            /* image mapping prevents replacing the file */
    CHECK(m != NULL);
    SET_ERR(0);
    BOOL ok = U(EndUpdateResourceA)(h, FALSE);
    CHECK(!ok);
    if (!ok) { DWORD e = GetLastError(); CHECKF(e == ERROR_ACCESS_DENIED || e == ERROR_SHARING_VIOLATION || e == ERROR_USER_MAPPED_FILE, "e=%lu", e); }
    ml_unload(m);
    DWORD err = 0;
    CHECK(upd_get(g_upd, RT_RCDATA, MAKEINTRESOURCEA(42), 0x409, NULL, &err) == NULL);
}
TEST(upd_begin_loaded_image) {
    REQUIRE(U(BeginUpdateResourceA)); REQUIRE(U(EndUpdateResourceA));
    CHECK(upd_fresh("upd_ld", "k32m_ld.dll"));
    HMODULE m = LoadLibraryA(g_upd);
    CHECK(m != NULL); if (!m) return;
    HANDLE h = U(BeginUpdateResourceA)(g_upd, FALSE);       /* a data-file open of a loaded image is allowed */
    CHECK(h != NULL);
    if (h) CHECK(U(EndUpdateResourceA)(h, TRUE));
    ml_unload(m);
}
TEST(upd_system_binary_copy) {
    REQUIRE(U(BeginUpdateResourceA)); REQUIRE(U(UpdateResourceA)); REQUIRE(U(EndUpdateResourceA));
    if (t_is_wine()) { t_skip("system binaries are stubs under wine"); return; }
    const char *d = ml_dir("upd_sys"); char p[MAX_PATH];
    if (!ml_copy_sys("notepad.exe", d, "np_copy.exe", p)) { t_skip("notepad.exe not available"); return; }
    SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL);
    HMODULE m0 = LoadLibraryExA(p, NULL, LOAD_LIBRARY_AS_DATAFILE);
    CHECK(m0 != NULL);
    enumst before; memset(&before, 0, sizeof before);
    if (m0) { EnumResourceTypesA(m0, en_types, (LONG_PTR)&before); FreeLibrary(m0); }
    HANDLE h = U(BeginUpdateResourceA)(p, FALSE);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (!h) return;
    static const char payload[] = "k32t-update";
    CHECK(U(UpdateResourceA)(h, RT_RCDATA, MAKEINTRESOURCEA(31337), 0x409, (LPVOID)payload, sizeof payload));
    CHECKF(U(EndUpdateResourceA)(h, FALSE), "err %lu", GetLastError());
    DWORD sz = 0; void *r = upd_get(p, RT_RCDATA, MAKEINTRESOURCEA(31337), 0x409, &sz, NULL);
    CHECK(r && sz == sizeof payload && !memcmp(r, payload, sz)); free(r);
    m0 = LoadLibraryExA(p, NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (m0) {
        enumst after; memset(&after, 0, sizeof after);
        EnumResourceTypesA(m0, en_types, (LONG_PTR)&after);
        CHECK(after.n_types >= before.n_types);
        CHECK(type_present(&after, 16) == type_present(&before, 16));       /* RT_VERSION preserved */
        FreeLibrary(m0);
    }
}
TEST(upd_test_exe_copy_without_rsrc_section) {
    REQUIRE(U(BeginUpdateResourceA)); REQUIRE(U(UpdateResourceA)); REQUIRE(U(EndUpdateResourceA));
    const char *d = ml_dir("upd_exe"); char p[MAX_PATH];
    if (!ml_copy_self(d, "self_copy.exe", p)) { t_skip("copy failed"); return; }
    SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL);   /* CopyFile keeps the source attributes; a read-only copy gives ERROR_WRITE_PROTECT */
    HANDLE h = U(BeginUpdateResourceA)(p, FALSE);
    CHECKF(h != NULL, "err %lu", GetLastError());
    if (!h) return;
    CHECK(U(UpdateResourceA)(h, RT_RCDATA, MAKEINTRESOURCEA(7), 0x409, "selfcopy", 8));
    BOOL ok = U(EndUpdateResourceA)(h, FALSE);
    t_note("EndUpdateResource on exe without .rsrc: %d err %lu", ok, ok ? 0 : GetLastError());
    if (ok) {
        DWORD sz = 0; void *r = upd_get(p, RT_RCDATA, MAKEINTRESOURCEA(7), 0x409, &sz, NULL);
        CHECK(r && sz == 8 && !memcmp(r, "selfcopy", 8)); free(r);
    }
}
TEST(upd_interleaved_sessions_on_two_files) {
    REQUIRE(U(BeginUpdateResourceA)); REQUIRE(U(UpdateResourceA)); REQUIRE(U(EndUpdateResourceA));
    char p1[MAX_PATH], p2[MAX_PATH]; const char *d = ml_dir("upd_two");
    CHECK(ml_put(ML_A, d, "k32m_t1.dll", p1)); CHECK(ml_put(ML_A, d, "k32m_t2.dll", p2));
    HANDLE h1 = U(BeginUpdateResourceA)(p1, FALSE), h2 = U(BeginUpdateResourceA)(p2, TRUE);
    CHECK(h1 && h2 && h1 != h2);
    if (!h1 || !h2) return;
    CHECK(U(UpdateResourceA)(h1, RT_RCDATA, MAKEINTRESOURCEA(11), 0x409, "one", 3));
    CHECK(U(UpdateResourceA)(h2, RT_RCDATA, MAKEINTRESOURCEA(22), 0x409, "two", 3));
    CHECK(U(EndUpdateResourceA)(h2, FALSE));
    CHECK(U(EndUpdateResourceA)(h1, FALSE));
    DWORD sz; void *a = upd_get(p1, RT_RCDATA, MAKEINTRESOURCEA(11), 0x409, &sz, NULL); CHECK(a && sz == 3 && !memcmp(a, "one", 3)); free(a);
    DWORD e = 0;
    CHECK(upd_get(p1, RT_RCDATA, MAKEINTRESOURCEA(22), 0x409, NULL, &e) == NULL);
    a = upd_get(p2, RT_RCDATA, MAKEINTRESOURCEA(22), 0x409, &sz, NULL); CHECK(a && sz == 3 && !memcmp(a, "two", 3)); free(a);
    CHECK(upd_get(p2, RT_RCDATA, MAKEINTRESOURCEA(1), 0x409, NULL, &e) == NULL);
}
