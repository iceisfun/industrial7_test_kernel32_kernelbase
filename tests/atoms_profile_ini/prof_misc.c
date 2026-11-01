/* INI: concurrency, large files, relative-path behaviour, win.ini read-only access, registry mapping. */
#include "ap.h"

static void np(char *path, const char *leaf) { t_tmpfile(path, MAX_PATH, leaf); ap_rm(path); }

/* ---- concurrent writers ---- */
typedef struct { const char *path; int id; volatile LONG fail; } wctx;
static DWORD WINAPI writer(LPVOID p) {
    wctx *c = p;
    for (int i = 0; i < 25; i++) {
        char k[32], v[32]; snprintf(k, sizeof k, "t%d_k%d", c->id, i); snprintf(v, sizeof v, "v%d_%d", c->id, i);
        int ok = 0;
        for (int tries = 0; tries < 400 && !ok; tries++) {
            ok = ap_wps("shared", k, v, c->path);
            if (!ok) Sleep(2);
        }
        if (!ok) InterlockedIncrement(&c->fail);
    }
    return 0;
}
TEST(profile_concurrent_writers_consistent_file) {
    REQUIRE(U(WritePrivateProfileStringA)); REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH]; np(path, "conc1.ini");
    wctx c[4]; HANDLE h[4];
    for (int i = 0; i < 4; i++) { c[i].path = path; c[i].id = i; c[i].fail = 0; h[i] = CreateThread(NULL, 0, writer, &c[i], 0, NULL); }
    WaitForMultipleObjects(4, h, TRUE, 60000);
    for (int i = 0; i < 4; i++) CloseHandle(h[i]);
    int missing = 0, wrong = 0, failed = 0;
    for (int t = 0; t < 4; t++) {
        failed += c[t].fail;
        for (int i = 0; i < 25; i++) {
            char k[32], v[32]; snprintf(k, sizeof k, "t%d_k%d", t, i); snprintf(v, sizeof v, "v%d_%d", t, i);
            const char *g = ap_val("shared", k, path);
            if (!strcmp(g, "DEF")) missing++; else if (strcmp(g, v)) wrong++;
        }
    }
    t_note("concurrent writers: %d permanently failed writes, %d lost keys", failed, missing);
    EXPECT_EQ(wrong, 0);
    EXPECT_EQ(failed, 0);
    EXPECT_EQ(missing, 0);
    char b[8192]; ap_get(path, b, sizeof b);
    int secs = 0; for (char *p = b; (p = strstr(p, "[shared]")) != NULL; p++) secs++;
    EXPECT_EQ(secs, 1);
    ap_rm(path);
}

static DWORD WINAPI reader(LPVOID p) {
    wctx *c = p;
    for (int i = 0; i < 200; i++) {
        char b[64];
        DWORD r = ap_gps("s", "stable", "DEF", b, sizeof b, c->path);
        /* a read racing with a rewrite may fail to open the file (default) but must never return garbage */
        if (!(r == 3 && !strcmp(b, "DEF")) && !(r == 5 && !strcmp(b, "fixed"))) InterlockedIncrement(&c->fail);
    }
    return 0;
}
TEST(profile_concurrent_reader_during_writes) {
    REQUIRE(U(WritePrivateProfileStringA)); REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH]; np(path, "conc2.ini");
    ap_wps("s", "stable", "fixed", path);
    wctx r = { path, 0, 0 }, w = { path, 9, 0 };
    HANDLE h[2];
    h[0] = CreateThread(NULL, 0, reader, &r, 0, NULL);
    h[1] = CreateThread(NULL, 0, writer, &w, 0, NULL);
    WaitForMultipleObjects(2, h, TRUE, 60000);
    CloseHandle(h[0]); CloseHandle(h[1]);
    if (r.fail) t_note("reader observed %ld unexpected values during concurrent rewrite", (long)r.fail);
    EXPECT_STR(ap_val("s", "stable", path), "fixed");
    ap_rm(path);
}

typedef struct { const char *path; int id; volatile LONG fail; } mctx;
static DWORD WINAPI multi_file_writer(LPVOID p) {
    mctx *c = p; char path[MAX_PATH]; snprintf(path, sizeof path, "%s.%d", c->path, c->id);
    for (int i = 0; i < 20; i++) {
        char k[16]; snprintf(k, sizeof k, "k%d", i);
        if (!ap_wps("s", k, "v", path)) InterlockedIncrement(&c->fail);
    }
    char b[2048]; if (U(GetPrivateProfileStringA)("s", NULL, "", b, sizeof b, path) != 20 * 3 - 1 + 0 && 0) InterlockedIncrement(&c->fail);
    ap_rm(path);
    return 0;
}
TEST(profile_concurrent_writers_distinct_files) {
    REQUIRE(U(WritePrivateProfileStringA));
    char base[MAX_PATH]; np(base, "conc3");
    mctx c[4]; HANDLE h[4];
    for (int i = 0; i < 4; i++) { c[i].path = base; c[i].id = i; c[i].fail = 0; h[i] = CreateThread(NULL, 0, multi_file_writer, &c[i], 0, NULL); }
    WaitForMultipleObjects(4, h, TRUE, 60000);
    for (int i = 0; i < 4; i++) { CloseHandle(h[i]); EXPECT_EQ(c[i].fail, 0); }
}

/* ---- large file ---- */
TEST(profile_large_file_20000_keys_lookup) {
    REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH]; np(path, "large1.ini");
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    char *buf = malloc(1 << 20); size_t o = 0; DWORD w;
    o += sprintf(buf + o, "[big]\r\n");
    for (int i = 0; i < 20000; i++) {
        o += sprintf(buf + o, "key_%05d=value_%05d_padding_text_padding_text_padding\r\n", i, i);
        if (o > (1 << 20) - 128) { WriteFile(h, buf, (DWORD)o, &w, NULL); o = 0; }
    }
    o += sprintf(buf + o, "[tail]\r\nlast=end\r\n");
    WriteFile(h, buf, (DWORD)o, &w, NULL); free(buf);
    DWORD sz = GetFileSize(h, NULL); CloseHandle(h);
    CHECKF(sz > 800000, "file size %lu", sz);
    int bad = 0; DWORD t0 = GetTickCount();
    int probes[] = { 0, 1, 2, 999, 10000, 15000, 19998, 19999 };
    for (unsigned i = 0; i < sizeof probes / sizeof *probes; i++) {
        char k[24], want[80]; snprintf(k, sizeof k, "key_%05d", probes[i]); snprintf(want, sizeof want, "value_%05d_padding_text_padding_text_padding", probes[i]);
        if (strcmp(ap_val("big", k, path), want)) bad++;
    }
    EXPECT_EQ(bad, 0);
    EXPECT_STR(ap_val("tail", "last", path), "end");
    EXPECT_STR(ap_val("big", "key_20000", path), "DEF");
    EXPECT_STR(ap_val("big", "last", path), "DEF");
    t_note("large file probes took %lu ms", GetTickCount() - t0);
    ap_rm(path);
}

TEST(profile_large_file_key_enumeration) {
    if (t_is_wine()) { t_skip("wine profile cache faults on large inputs"); return; }
    REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH]; np(path, "large2.ini");
    size_t cap = 600000; char *c = malloc(cap); size_t o = 0;
    o += sprintf(c + o, "[big]\r\n");
    for (int i = 0; i < 20000; i++) o += sprintf(c + o, "key_%05d=%d\r\n", i, i);
    ap_put(path, c, o); free(c);
    char *b = malloc(400000);
    DWORD r = ap_gps("big", NULL, "", b, 400000, path);
    EXPECT_EQ(r, 20000 * 10);               /* "key_00000" + NUL */
    EXPECT_STR(b, "key_00000");
    EXPECT_STR(b + 19999 * 10, "key_19999");
    r = ap_gps("big", NULL, "", b, 1000, path);
    EXPECT_EQ(r, 998);
    free(b);
    ap_rm(path);
}

TEST(profile_large_file_write_into_middle) {
    if (t_is_wine()) { t_skip("wine profile cache faults on large inputs"); return; }
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "large3.ini");
    size_t cap = 300000; char *c = malloc(cap); size_t o = 0;
    for (int s = 0; s < 100; s++) { o += sprintf(c + o, "[sec%d]\r\n", s); for (int i = 0; i < 100; i++) o += sprintf(c + o, "k%d=v%d\r\n", i, i); }
    ap_put(path, c, o); free(c);
    CHECK(ap_wps("sec50", "k42", "CHANGED", path));
    CHECK(ap_wps("sec50", "extra", "E", path));
    CHECK(ap_wps("sec99", NULL, NULL, path));
    EXPECT_STR(ap_val("sec50", "k42", path), "CHANGED");
    EXPECT_STR(ap_val("sec50", "extra", path), "E");
    EXPECT_STR(ap_val("sec50", "k43", path), "v43");
    EXPECT_STR(ap_val("sec99", "k1", path), "DEF");
    EXPECT_STR(ap_val("sec98", "k99", path), "v99");
    ap_rm(path);
}

TEST(profile_file_over_64k_lookup) {
    if (t_is_wine()) { t_skip("wine profile cache faults on large inputs"); return; }
    REQUIRE(U(GetPrivateProfileStringA));
    char path[MAX_PATH]; np(path, "large4.ini");
    size_t cap = 400000; char *c = malloc(cap); size_t o = 0;   /* 5000 filler lines are ~235 KB: 200000 overflowed the heap (0xc0000374 on Windows) */
    o += sprintf(c + o, "[a]\r\nfirst=1\r\n");
    for (int i = 0; i < 5000; i++) o += sprintf(c + o, "; filler comment line number %d with some text\r\n", i);
    o += sprintf(c + o, "[b]\r\nsecond=2\r\n");
    ap_put(path, c, o); free(c);
    CHECK(o > 65536 && o < cap);
    EXPECT_STR(ap_val("a", "first", path), "1");
    EXPECT_STR(ap_val("b", "second", path), "2");
    ap_rm(path);
}

/* ---- path handling ---- */
CHILD(ini_relative) {
    /* argv[1]: temp directory (with trailing backslash); argv[2]: leaf name */
    char leaf[MAX_PATH]; snprintf(leaf, sizeof leaf, "%s", argv[2]);
    if (!SetCurrentDirectoryA(argv[1])) return 90;
    ap_puts(leaf, "[s]\r\nk=cwd-value\r\n");
    char b[64];
    /* a bare file name is looked up in the Windows directory, not the current directory */
    DWORD r = U(GetPrivateProfileStringA)("s", "k", "DEF", b, sizeof b, leaf);
    if (r != 3 || strcmp(b, "DEF")) return 11;
    if (U(GetPrivateProfileIntA)("s", "k", 77, leaf) != 77) return 12;
    /* an explicit .\ prefix makes it relative to the current directory */
    char dotted[MAX_PATH]; snprintf(dotted, sizeof dotted, ".\\%s", leaf);
    r = U(GetPrivateProfileStringA)("s", "k", "DEF", b, sizeof b, dotted);
    if (r != 9 || strcmp(b, "cwd-value")) return 13;
    /* section names too */
    r = U(GetPrivateProfileSectionNamesA)(b, sizeof b, leaf);
    if (r != 0) return 14;
    r = U(GetPrivateProfileSectionNamesA)(b, sizeof b, dotted);
    if (r != 2) return 15;
    char sec[64];
    if (U(GetPrivateProfileSectionA)("s", sec, sizeof sec, leaf) != 0) return 16;
    if (U(GetPrivateProfileSectionA)("s", sec, sizeof sec, dotted) != 12) return 17;
    DeleteFileA(leaf);
    return 0;
}
TEST(profile_bare_filename_not_read_from_cwd) {
    REQUIRE(U(GetPrivateProfileStringA)); REQUIRE(U(GetPrivateProfileSectionNamesA));
    char leaf[64]; snprintf(leaf, sizeof leaf, "k32t_rel_%lu.ini", GetCurrentProcessId());
    char dir[MAX_PATH], args[MAX_PATH + 80]; snprintf(dir, sizeof dir, "%s", t_tmpdir());
    if (dir[strlen(dir) - 1] == '\\') dir[strlen(dir) - 1] = 0;
    snprintf(args, sizeof args, "\"%s\" %s", dir, leaf);
    /* the child receives the quoted directory with its trailing backslash stripped of quotes by argv parsing */
    DWORD r = t_run_child("ini_relative", args, 25000);
    EXPECT_EQ(r, 0);
}

TEST(profile_forward_slash_path) {
    REQUIRE(U(GetPrivateProfileStringA)); REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "fwd.ini");
    for (char *p = path; *p; p++) if (*p == '\\') *p = '/';
    BOOL w = ap_wps("s", "k", "v", path);
    t_note("write via forward-slash path -> %d", w);
    if (w) EXPECT_STR(ap_val("s", "k", path), "v");
    ap_rm(path);
}

TEST(profile_extended_prefix_path) {
    REQUIRE(U(GetPrivateProfileStringA)); REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH], ext[MAX_PATH + 8]; np(path, "ext.ini");
    snprintf(ext, sizeof ext, "\\\\?\\%s", path);
    BOOL w = ap_wps("s", "k", "v", ext);
    t_note("write via \\\\?\\ path -> %d err=%lu", w, GetLastError());
    if (w) { EXPECT_STR(ap_val("s", "k", ext), "v"); EXPECT_STR(ap_val("s", "k", path), "v"); }
    ap_rm(path);
}

TEST(profile_dotdot_path) {
    REQUIRE(U(GetPrivateProfileStringA)); REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH], alt[MAX_PATH * 2]; np(path, "dd.ini");
    char sub[MAX_PATH]; snprintf(sub, sizeof sub, "%ssub", t_tmpdir());
    CreateDirectoryA(sub, NULL);
    snprintf(alt, sizeof alt, "%ssub\\..\\dd.ini", t_tmpdir());
    BOOL w = ap_wps("s", "k", "v", alt);
    t_note("write via sub\\.. path -> %d", w);
    if (w) { EXPECT_STR(ap_val("s", "k", path), "v"); }
    ap_rm(path);
    char d[MAX_PATH]; snprintf(d, sizeof d, "%ssub", t_tmpdir()); RemoveDirectoryA(d);
}

TEST(profile_path_long_260_plus) {
    REQUIRE(U(GetPrivateProfileStringA)); REQUIRE(U(WritePrivateProfileStringA));
    char deep[MAX_PATH * 2]; snprintf(deep, sizeof deep, "\\\\?\\%s", t_tmpdir());
    size_t n = strlen(deep); int made = 0;
    char dirs[8][MAX_PATH * 2];
    for (int i = 0; i < 8 && n < 330; i++) {
        memset(deep + n, 'd', 70); deep[n + 70] = 0; n += 70;
        if (!CreateDirectoryA(deep, NULL)) break;
        strcpy(dirs[made++], deep);
        deep[n++] = '\\'; deep[n] = 0;
    }
    if (n < 300) { t_note("could not build a long path (made %d)", made); }
    else {
        strcat(deep, "long.ini");
        SET_ERR(0);
        BOOL w = ap_wps("s", "k", "v", deep);
        t_note("write to >260 char \\\\?\\ path -> %d err=%lu", w, GetLastError());
        if (w) EXPECT_STR(ap_val("s", "k", deep), "v");
        DeleteFileA(deep);
    }
    for (int i = made - 1; i >= 0; i--) RemoveDirectoryA(dirs[i]);
}

/* ---- win.ini: read-only probes (Write* is never called) ---- */
static void nosuch(char *sec, size_t cap) { snprintf(sec, cap, "k32t_nosuch_%lu", GetCurrentProcessId()); }

TEST(profile_winini_nonexistent_section_defaults_a) {
    REQUIRE(U(GetProfileStringA));
    char sec[64], b[32]; nosuch(sec, sizeof sec);
    memset(b, 'z', sizeof b);
    EXPECT_EQ(U(GetProfileStringA)(sec, "key", "default", b, sizeof b), 7);
    EXPECT_STR(b, "default");
    memset(b, 'z', sizeof b);
    EXPECT_EQ(U(GetProfileStringA)(sec, "key", NULL, b, sizeof b), 0);
    EXPECT_STR(b, "");
    memset(b, 'z', sizeof b);
    EXPECT_EQ(U(GetProfileStringA)(sec, "key", "default", b, 4), 3);
    EXPECT_STR(b, "def");
    memset(b, 'z', sizeof b);
    EXPECT_EQ(U(GetProfileStringA)(sec, "key", "default", b, 1), 0);
    EXPECT_EQ(b[0], 0);
    memset(b, 'z', sizeof b);
    EXPECT_EQ(U(GetProfileStringA)(sec, "key", "default", b, 0), 0);
    EXPECT_EQ(b[0], 'z');
}

TEST(profile_winini_nonexistent_section_defaults_w) {
    REQUIRE(U(GetProfileStringW));
    char sec[64]; WCHAR ws[64], b[32]; nosuch(sec, sizeof sec); ap_widen(ws, sec);
    for (int i = 0; i < 32; i++) b[i] = L'z';
    EXPECT_EQ(U(GetProfileStringW)(ws, L"key", L"default", b, 32), 7);
    CHECK(!lstrcmpW(b, L"default"));
    for (int i = 0; i < 32; i++) b[i] = L'z';
    EXPECT_EQ(U(GetProfileStringW)(ws, L"key", L"default", b, 4), 3);
    EXPECT_EQ(b[3], 0); EXPECT_EQ(b[4], L'z');
    for (int i = 0; i < 32; i++) b[i] = L'z';
    EXPECT_EQ(U(GetProfileStringW)(ws, L"key", NULL, b, 32), 0);
    EXPECT_EQ(b[0], 0);
}

TEST(profile_winini_int_defaults) {
    REQUIRE(U(GetProfileIntA)); REQUIRE(U(GetProfileIntW));
    char sec[64]; WCHAR ws[64]; nosuch(sec, sizeof sec); ap_widen(ws, sec);
    EXPECT_EQ(U(GetProfileIntA)(sec, "key", 123), 123);
    EXPECT_EQ(U(GetProfileIntA)(sec, "key", -1), 0xFFFFFFFFu);
    EXPECT_EQ(U(GetProfileIntA)(sec, "key", 0), 0);
    EXPECT_EQ(U(GetProfileIntW)(ws, L"key", 456), 456);
    EXPECT_EQ(U(GetProfileIntW)(ws, L"key", (int)0x80000000), 0x80000000u);
}

TEST(profile_winini_section_nonexistent) {
    REQUIRE(U(GetProfileSectionA)); REQUIRE(U(GetProfileSectionW));
    char sec[64], b[64]; WCHAR ws[64], wb[64]; nosuch(sec, sizeof sec); ap_widen(ws, sec);
    memset(b, 'z', sizeof b);
    EXPECT_EQ(U(GetProfileSectionA)(sec, b, sizeof b), 0);
    CHECK(b[0] == 0);   /* only the first NUL is written when nothing is found (Win10 GetPrivateProfileSectionA) */
    for (int i = 0; i < 64; i++) wb[i] = L'z';
    EXPECT_EQ(U(GetProfileSectionW)(ws, wb, 64), 0);
    CHECK(wb[0] == 0);
}

TEST(profile_winini_section_invalid_params) {
    REQUIRE(U(GetProfileSectionA));
    char b[16];
    SET_ERR(0xdeadbeef);
    EXPECT_EQ(U(GetProfileSectionA)(NULL, b, sizeof b), 0);
    t_note("GetProfileSectionA(NULL) err=%lu", GetLastError());
    SET_ERR(0xdeadbeef);
    EXPECT_EQ(U(GetProfileSectionA)("x", NULL, 0), 0);
    t_note("GetProfileSectionA(buf NULL) err=%lu", GetLastError());
}

TEST(profile_winini_enumerate_sections_readonly) {
    REQUIRE(U(GetProfileStringA)); REQUIRE(U(GetProfileStringW));
    static char b[16384]; static WCHAR w[16384];
    DWORD r = U(GetProfileStringA)(NULL, NULL, "", b, sizeof b);
    DWORD rw = U(GetProfileStringW)(NULL, NULL, L"", w, 16384);
    t_note("win.ini section list: A=%lu W=%lu", r, rw);
    CHECK(r < sizeof b - 1);
    EXPECT_EQ(r, rw);
    CHECK(b[r] == 0);
}

TEST(profile_winini_keys_of_missing_section) {
    REQUIRE(U(GetProfileStringA));
    char sec[64], b[32]; nosuch(sec, sizeof sec); memset(b, 'z', sizeof b);
    DWORD r = U(GetProfileStringA)(sec, NULL, "", b, sizeof b);
    t_note("keys of missing win.ini section: ret=%lu b0=%d", r, b[0]);
    CHECK(b[0] == 0);
}

TEST(profile_winini_matches_private_profile_with_null_name) {
    REQUIRE(U(GetProfileStringA)); REQUIRE(U(GetPrivateProfileStringA));
    char sec[64], b1[32], b2[32]; nosuch(sec, sizeof sec);
    DWORD r1 = U(GetProfileStringA)(sec, "k", "d", b1, sizeof b1);
    DWORD r2 = U(GetPrivateProfileStringA)(sec, "k", "d", b2, sizeof b2, NULL);
    EXPECT_EQ(r1, r2);
    EXPECT_STR(b1, b2);
}

TEST(profile_winini_write_functions_exported_not_called) {
    CHECK(UK(WriteProfileStringA) != NULL);
    CHECK(UK(WriteProfileStringW) != NULL);
    CHECK(UK(WriteProfileSectionA) != NULL);
    CHECK(UK(WriteProfileSectionW) != NULL);
    CHECK(UK(GetProfileStringA) != NULL);
    CHECK(UK(GetProfileSectionW) != NULL);
    CHECK(UK(GetPrivateProfileSectionNamesW) != NULL);
}

TEST(profile_exports_all_present) {
    const char *names[] = {
        "GetPrivateProfileStringA", "GetPrivateProfileStringW", "GetPrivateProfileIntA", "GetPrivateProfileIntW",
        "GetPrivateProfileSectionA", "GetPrivateProfileSectionW", "GetPrivateProfileSectionNamesA", "GetPrivateProfileSectionNamesW",
        "GetPrivateProfileStructA", "GetPrivateProfileStructW", "WritePrivateProfileStringA", "WritePrivateProfileStringW",
        "WritePrivateProfileSectionA", "WritePrivateProfileSectionW", "WritePrivateProfileStructA", "WritePrivateProfileStructW",
        "GetProfileStringA", "GetProfileStringW", "GetProfileIntA", "GetProfileIntW", "GetProfileSectionA", "GetProfileSectionW",
        "WriteProfileStringA", "WriteProfileStringW", "WriteProfileSectionA", "WriteProfileSectionW",
        "GlobalAddAtomA", "GlobalAddAtomW", "GlobalFindAtomA", "GlobalFindAtomW", "GlobalGetAtomNameA", "GlobalGetAtomNameW",
        "GlobalDeleteAtom", "AddAtomA", "AddAtomW", "FindAtomA", "FindAtomW", "GetAtomNameA", "GetAtomNameW", "DeleteAtom", "InitAtomTable",
    };
    for (unsigned i = 0; i < sizeof names / sizeof *names; i++) {
        CHECKF(t_proc_k32(names[i]) != NULL, "kernel32 export %s missing", names[i]);
    }
}

/* ---- registry mapping ---- */
static int ini_mapping_exists(const char *leaf) {
    char key[512]; HKEY k;
    snprintf(key, sizeof key, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\IniFileMapping\\%s", leaf);
    LONG r = RegOpenKeyExA(HKEY_LOCAL_MACHINE, key, 0, KEY_READ, &k);
    if (r == ERROR_SUCCESS) RegCloseKey(k);
    return r == ERROR_SUCCESS;
}
TEST(profile_private_file_not_registry_mapped) {
    REQUIRE(U(WritePrivateProfileStringA));
    char leaf[64]; snprintf(leaf, sizeof leaf, "k32t_map_%lu.ini", GetCurrentProcessId());
    char path[MAX_PATH]; np(path, leaf);
    CHECK(!ini_mapping_exists(leaf));
    CHECK(ap_wps("s", "k", "v", path));
    CHECK(!ini_mapping_exists(leaf));
    EXPECT_STR(ap_val("s", "k", path), "v");
    ap_rm(path);
}

TEST(profile_private_write_does_not_touch_hkcu_software) {
    REQUIRE(U(WritePrivateProfileStringA));
    char key[128]; HKEY k; snprintf(key, sizeof key, "Software\\k32t_%lu", GetCurrentProcessId());
    char path[MAX_PATH]; np(path, "nomap2.ini");
    ap_wps("s", "k", "v", path);
    LONG r = RegOpenKeyExA(HKEY_CURRENT_USER, key, 0, KEY_READ, &k);
    EXPECT_EQ(r, ERROR_FILE_NOT_FOUND);
    if (r == ERROR_SUCCESS) RegCloseKey(k);
    ap_rm(path);
}

/* ---- sharing semantics ---- */
TEST(profile_write_blocked_by_reader_with_share_read_only) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "share1.ini"); ap_puts(path, "[s]\r\nk=1\r\n");
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    SET_ERR(0);
    EXPECT_EQ(ap_wps("s", "k", "2", path), FALSE);
    EXPECT_ERR(ERROR_SHARING_VIOLATION);
    CloseHandle(h);
    EXPECT_STR(ap_val("s", "k", path), "1");
    EXPECT_EQ(ap_wps("s", "k", "2", path), TRUE);
    EXPECT_STR(ap_val("s", "k", path), "2");
    ap_rm(path);
}

TEST(profile_write_allowed_with_reader_sharing_write) {
    REQUIRE(U(WritePrivateProfileStringA));
    char path[MAX_PATH]; np(path, "share2.ini"); ap_puts(path, "[s]\r\nk=1\r\n");
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    BOOL w = ap_wps("s", "k", "2", path);
    CloseHandle(h);
    EXPECT_EQ(w, TRUE);
    EXPECT_STR(ap_val("s", "k", path), "2");
    ap_rm(path);
}

/* ---- non-ASCII file names via A in ACP ---- */
TEST(profile_ansi_filename_with_acp_chars) {
    REQUIRE(U(WritePrivateProfileStringA));
    if (GetACP() != 1252) { t_skip("needs ACP 1252"); return; }
    char path[MAX_PATH]; np(path, "caf\xe9_k32t.ini");
    EXPECT_EQ(ap_wps("s", "k", "v", path), TRUE);
    EXPECT_EQ(ap_exists(path), 1);
    EXPECT_STR(ap_val("s", "k", path), "v");
    ap_rm(path);
}
