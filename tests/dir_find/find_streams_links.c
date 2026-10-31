/* FindFirstStreamW/FindNextStreamW and FindFirstFileNameW/FindNextFileNameW */
#include "dh.h"

static char *sub(char *buf, size_t cap, const char *dir, const char *leaf) { dh_path(buf, cap, dir, leaf); return buf; }
#define NEED_REAL() do { if (t_is_wine()) { t_skip("stream/link enumeration is not implemented by the emulator"); return; } } while (0)
#define NEED_NTFS() do { if (!dh_is_ntfs()) { t_skip("scratch volume is not NTFS"); return; } } while (0)

static int mkstream(const char *file, const char *name, const char *data) {
    char p[MAX_PATH * 2]; snprintf(p, sizeof p, "%s:%s", file, name);
    HANDLE h = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    DWORD w; if (data && *data) WriteFile(h, data, (DWORD)strlen(data), &w, NULL);
    CloseHandle(h);
    return 1;
}
typedef struct { WCHAR name[MAX_PATH + 36]; LONGLONG size; } sinfo;
static int list_streams(const WCHAR *path, sinfo *out, int max) {
    WIN32_FIND_STREAM_DATA sd;
    HANDLE h = U(FindFirstStreamW)(path, FindStreamInfoStandard, &sd, 0);
    if (h == INVALID_HANDLE_VALUE) return -1;
    int n = 0;
    do { if (n < max) { wcscpy(out[n].name, sd.cStreamName); out[n].size = sd.StreamSize.QuadPart; } n++; } while (U(FindNextStreamW)(h, &sd));
    DWORD e = GetLastError();
    U(FindClose)(h);
    SetLastError(e);
    return n;
}
static int find_stream(const sinfo *l, int n, const WCHAR *name) { for (int i = 0; i < n; i++) if (!wcscmp(l[i].name, name)) return i; return -1; }

TEST(streams_default_stream_only) {
    NEED_REAL();
    REQUIRE(U(FindFirstStreamW)); REQUIRE(U(FindNextStreamW)); REQUIRE(U(FindClose));
    NEED_NTFS();
    char *d = dh_dir("st_def"), p[MAX_PATH]; WCHAR w[MAX_PATH]; WIN32_FIND_STREAM_DATA sd;
    dh_mkfile(d, "f", 10); sub(p, sizeof p, d, "f"); dh_w(w, MAX_PATH, p);
    memset(&sd, 0xCD, sizeof sd);
    HANDLE h = U(FindFirstStreamW)(w, FindStreamInfoStandard, &sd, 0);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    CHECK(!wcscmp(sd.cStreamName, L"::$DATA"));
    EXPECT_EQ(sd.StreamSize.QuadPart, 10);
    SET_ERR(0);
    CHECK(!U(FindNextStreamW)(h, &sd));
    EXPECT_ERR(ERROR_HANDLE_EOF);
    SET_ERR(0);
    CHECK(!U(FindNextStreamW)(h, &sd));
    EXPECT_ERR(ERROR_HANDLE_EOF);
    CHECK(U(FindClose)(h));
}
TEST(streams_empty_file) {
    NEED_REAL();
    REQUIRE(U(FindFirstStreamW));
    NEED_NTFS();
    char *d = dh_dir("st_empty"), p[MAX_PATH]; WCHAR w[MAX_PATH]; sinfo l[4];
    dh_mkfile(d, "f", 0); sub(p, sizeof p, d, "f"); dh_w(w, MAX_PATH, p);
    int n = list_streams(w, l, 4);
    EXPECT_EQ(n, 1);
    if (n >= 1) { CHECK(!wcscmp(l[0].name, L"::$DATA")); EXPECT_EQ(l[0].size, 0); }
}
TEST(streams_named_streams_listed) {
    NEED_REAL();
    REQUIRE(U(FindFirstStreamW)); REQUIRE(U(FindNextStreamW));
    NEED_NTFS();
    char *d = dh_dir("st_named"), p[MAX_PATH]; WCHAR w[MAX_PATH]; sinfo l[8];
    dh_mkfile(d, "f", 3); sub(p, sizeof p, d, "f");
    CHECK(mkstream(p, "alpha", "12345")); CHECK(mkstream(p, "beta", "1234567")); CHECK(mkstream(p, "empty", ""));
    dh_w(w, MAX_PATH, p);
    int n = list_streams(w, l, 8);
    EXPECT_EQ(n, 4);
    int a = find_stream(l, n, L":alpha:$DATA"), b = find_stream(l, n, L":beta:$DATA"), e = find_stream(l, n, L":empty:$DATA"), dflt = find_stream(l, n, L"::$DATA");
    CHECK(a >= 0 && b >= 0 && e >= 0 && dflt >= 0);
    if (a >= 0) EXPECT_EQ(l[a].size, 5);
    if (b >= 0) EXPECT_EQ(l[b].size, 7);
    if (e >= 0) EXPECT_EQ(l[e].size, 0);
    if (dflt >= 0) { EXPECT_EQ(l[dflt].size, 3); EXPECT_EQ(dflt, 0); }   /* default stream first on NTFS */
    t_note("stream order: %ls %ls %ls %ls", n > 0 ? l[0].name : L"", n > 1 ? l[1].name : L"", n > 2 ? l[2].name : L"", n > 3 ? l[3].name : L"");
}
TEST(streams_default_stream_always_listed_for_files_with_ads) {
    NEED_REAL();
    REQUIRE(U(FindFirstStreamW));
    NEED_NTFS();
    char *d = dh_dir("st_dflt2"), p[MAX_PATH]; WCHAR w[MAX_PATH]; sinfo l[4];
    dh_mkfile(d, "f", 0); sub(p, sizeof p, d, "f"); mkstream(p, "only", "x"); dh_w(w, MAX_PATH, p);
    int n = list_streams(w, l, 4);
    EXPECT_EQ(n, 2);
    CHECK(find_stream(l, n, L"::$DATA") >= 0);
}
TEST(streams_directory_without_streams) {
    NEED_REAL();
    REQUIRE(U(FindFirstStreamW));
    NEED_NTFS();
    char *d = dh_dir("st_dir"), p[MAX_PATH]; WCHAR w[MAX_PATH]; WIN32_FIND_STREAM_DATA sd;
    dh_mkdir(d, "plain"); sub(p, sizeof p, d, "plain"); dh_w(w, MAX_PATH, p);
    SET_ERR(0);
    CHECK(U(FindFirstStreamW)(w, FindStreamInfoStandard, &sd, 0) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_HANDLE_EOF);
}
TEST(streams_directory_with_ads) {
    NEED_REAL();
    REQUIRE(U(FindFirstStreamW));
    NEED_NTFS();
    char *d = dh_dir("st_dirads"), p[MAX_PATH]; WCHAR w[MAX_PATH]; sinfo l[4];
    dh_mkdir(d, "dd"); sub(p, sizeof p, d, "dd");
    if (!mkstream(p, "meta", "abc")) { t_skip("cannot create stream on directory"); return; }
    dh_w(w, MAX_PATH, p);
    int n = list_streams(w, l, 4);
    EXPECT_EQ(n, 1);
    if (n == 1) { CHECK(!wcscmp(l[0].name, L":meta:$DATA")); EXPECT_EQ(l[0].size, 3); }
}
TEST(streams_missing_file) {
    NEED_REAL();
    REQUIRE(U(FindFirstStreamW));
    char *d = dh_dir("st_missing"), p[MAX_PATH]; WCHAR w[MAX_PATH]; WIN32_FIND_STREAM_DATA sd;
    sub(p, sizeof p, d, "nosuch"); dh_w(w, MAX_PATH, p);
    SET_ERR(0);
    CHECK(U(FindFirstStreamW)(w, FindStreamInfoStandard, &sd, 0) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    sub(p, sizeof p, d, "nodir\\nosuch"); dh_w(w, MAX_PATH, p);
    CHECK(U(FindFirstStreamW)(w, FindStreamInfoStandard, &sd, 0) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(streams_invalid_level_and_flags) {
    NEED_REAL();
    REQUIRE(U(FindFirstStreamW));
    char *d = dh_dir("st_inv"), p[MAX_PATH]; WCHAR w[MAX_PATH]; WIN32_FIND_STREAM_DATA sd;
    dh_mkfile(d, "f", 1); sub(p, sizeof p, d, "f"); dh_w(w, MAX_PATH, p);
    SET_ERR(0);
    CHECK(U(FindFirstStreamW)(w, (STREAM_INFO_LEVELS)1, &sd, 0) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CHECK(U(FindFirstStreamW)(w, (STREAM_INFO_LEVELS)7, &sd, 0) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CHECK(U(FindFirstStreamW)(w, FindStreamInfoStandard, &sd, 1) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CHECK(U(FindFirstStreamW)(w, FindStreamInfoStandard, &sd, 0x80000000) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(streams_empty_name) {
    NEED_REAL();
    REQUIRE(U(FindFirstStreamW));
    WIN32_FIND_STREAM_DATA sd;
    SET_ERR(0);
    CHECK(U(FindFirstStreamW)(L"", FindStreamInfoStandard, &sd, 0) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(streams_invalid_input_leaves_data_alone) {
    NEED_REAL();
    REQUIRE(U(FindFirstStreamW));
    WIN32_FIND_STREAM_DATA sd; memset(&sd, 0xCD, sizeof sd);
    U(FindFirstStreamW)(L"C:\\k32t_never_exists\\x", (STREAM_INFO_LEVELS)3, &sd, 0);
    CHECK(sd.StreamSize.QuadPart == (LONGLONG)0xCDCDCDCDCDCDCDCDLL);
}
TEST(streams_open_exclusive_file_still_listable) {
    NEED_REAL();
    REQUIRE(U(FindFirstStreamW));
    NEED_NTFS();
    char *d = dh_dir("st_excl"), p[MAX_PATH]; WCHAR w[MAX_PATH]; sinfo l[4];
    dh_mkfile(d, "f", 5); sub(p, sizeof p, d, "f"); dh_w(w, MAX_PATH, p);
    HANDLE h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    int n = list_streams(w, l, 4);
    EXPECT_EQ(n, 1);     /* attribute-only open does not conflict with exclusive sharing */
    CloseHandle(h);
}
TEST(streams_size_tracks_writes) {
    NEED_REAL();
    REQUIRE(U(FindFirstStreamW));
    NEED_NTFS();
    char *d = dh_dir("st_grow"), p[MAX_PATH]; WCHAR w[MAX_PATH]; sinfo l[4];
    dh_mkfile(d, "f", 0); sub(p, sizeof p, d, "f"); dh_w(w, MAX_PATH, p);
    mkstream(p, "g", "");
    static const int sizes[] = { 1, 8, 64, 512, 4095, 3, 0 };
    for (unsigned k = 0; k < sizeof sizes / sizeof *sizes; k++) {
        int sz = sizes[k];
        char buf[4096]; memset(buf, 'g', sizeof buf);
        char sp[MAX_PATH * 2]; snprintf(sp, sizeof sp, "%s:g", p);
        HANDLE h = CreateFileA(sp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); DWORD wr = 0;
        if (sz) WriteFile(h, buf, sz, &wr, NULL);
        CloseHandle(h);
        int n = list_streams(w, l, 4); int i = find_stream(l, n, L":g:$DATA");
        CHECK(i >= 0);
        if (i >= 0) CHECKF(l[i].size == sz, "size %lld want %d", l[i].size, sz);
    }
}
TEST(streams_many_streams) {
    NEED_REAL();
    REQUIRE(U(FindFirstStreamW));
    NEED_NTFS();
    char *d = dh_dir("st_many"), p[MAX_PATH]; WCHAR w[MAX_PATH]; sinfo *l = malloc(sizeof(sinfo) * 128);
    dh_mkfile(d, "f", 1); sub(p, sizeof p, d, "f");
    for (int i = 0; i < 60; i++) { char n[16]; snprintf(n, sizeof n, "s%02d", i); if (!mkstream(p, n, "z")) { CHECKF(0, "stream %d err %lu", i, GetLastError()); break; } }
    dh_w(w, MAX_PATH, p);
    int n = list_streams(w, l, 128);
    EXPECT_EQ(n, 61);
    for (int i = 0; i < 60; i++) { WCHAR nm[32]; snwprintf(nm, 32, L":s%02d:$DATA", i); CHECKF(find_stream(l, n < 128 ? n : 128, nm) >= 0, "missing s%02d", i); }
    free(l);
}
TEST(streams_unicode_stream_name) {
    NEED_REAL();
    REQUIRE(U(FindFirstStreamW));
    NEED_NTFS();
    char *d = dh_dir("st_uni"), p[MAX_PATH]; WCHAR w[MAX_PATH], sp[MAX_PATH + 32]; sinfo l[4];
    dh_mkfile(d, "f", 0); sub(p, sizeof p, d, "f"); dh_w(w, MAX_PATH, p);
    snwprintf(sp, MAX_PATH + 32, L"%ls:caf\u00e9\u65e5", w);
    HANDLE h = CreateFileW(sp, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    CloseHandle(h);
    int n = list_streams(w, l, 4);
    EXPECT_EQ(n, 2);
    CHECK(find_stream(l, n, L":caf\u00e9\u65e5:$DATA") >= 0);
    DeleteFileW(w);
}
TEST(streams_name_with_type_suffix) {
    NEED_REAL();
    REQUIRE(U(FindFirstStreamW));
    NEED_NTFS();
    char *d = dh_dir("st_suffix"), p[MAX_PATH], sp[MAX_PATH * 2]; WCHAR w[MAX_PATH]; sinfo l[4];
    dh_mkfile(d, "f", 0); sub(p, sizeof p, d, "f");
    snprintf(sp, sizeof sp, "%s:typed:$DATA", p);
    HANDLE h = CreateFileA(sp, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    dh_w(w, MAX_PATH, p);
    int n = list_streams(w, l, 4);
    EXPECT_EQ(n, 2);
    CHECK(find_stream(l, n, L":typed:$DATA") >= 0);
}
TEST(streams_query_via_stream_path) {
    NEED_REAL();
    REQUIRE(U(FindFirstStreamW));
    NEED_NTFS();
    char *d = dh_dir("st_viapath"), p[MAX_PATH], sp[MAX_PATH * 2]; WCHAR w[MAX_PATH]; sinfo l[8];
    dh_mkfile(d, "f", 2); sub(p, sizeof p, d, "f"); mkstream(p, "x", "123");
    snprintf(sp, sizeof sp, "%s:x", p); dh_w(w, MAX_PATH, sp);
    int n = list_streams(w, l, 8);
    t_note("FindFirstStreamW on 'file:stream' path: %d streams", n);
    CHECK(n == 2 || n == 1 || n < 0);
}
TEST(streams_deleted_stream_disappears) {
    NEED_REAL();
    REQUIRE(U(FindFirstStreamW));
    NEED_NTFS();
    char *d = dh_dir("st_del"), p[MAX_PATH], sp[MAX_PATH * 2]; WCHAR w[MAX_PATH]; sinfo l[4];
    dh_mkfile(d, "f", 0); sub(p, sizeof p, d, "f"); mkstream(p, "tmp", "q");
    dh_w(w, MAX_PATH, p);
    EXPECT_EQ(list_streams(w, l, 4), 2);
    snprintf(sp, sizeof sp, "%s:tmp", p);
    CHECK(DeleteFileA(sp));
    EXPECT_EQ(list_streams(w, l, 4), 1);
}
TEST(streams_relative_to_raw_prefix) {
    NEED_REAL();
    REQUIRE(U(FindFirstStreamW));
    NEED_NTFS();
    char *d = dh_dir("st_raw"), p[MAX_PATH]; WCHAR w[MAX_PATH], r[MAX_PATH + 8]; sinfo l[4];
    dh_mkfile(d, "f", 4); sub(p, sizeof p, d, "f"); dh_w(w, MAX_PATH, p);
    snwprintf(r, MAX_PATH + 8, L"\\\\?\\%ls", w);
    EXPECT_EQ(list_streams(r, l, 4), 1);
}
TEST(streams_wildcard_name_rejected) {
    NEED_REAL();
    REQUIRE(U(FindFirstStreamW));
    char *d = dh_dir("st_wild"), p[MAX_PATH]; WCHAR w[MAX_PATH]; WIN32_FIND_STREAM_DATA sd;
    dh_mkfile(d, "f", 1); sub(p, sizeof p, d, "f*"); dh_w(w, MAX_PATH, p);
    SET_ERR(0);
    CHECK(U(FindFirstStreamW)(w, FindStreamInfoStandard, &sd, 0) == INVALID_HANDLE_VALUE);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_INVALID_NAME || e == ERROR_FILE_NOT_FOUND, "err %lu", e);
}
TEST(streams_device_name) {
    NEED_REAL();
    REQUIRE(U(FindFirstStreamW));
    WIN32_FIND_STREAM_DATA sd;
    SET_ERR(0);
    HANDLE h = U(FindFirstStreamW)(L"NUL", FindStreamInfoStandard, &sd, 0);
    t_note("FindFirstStreamW(NUL): %s err=%lu", h != INVALID_HANDLE_VALUE ? "handle" : "failed", GetLastError());
    if (h != INVALID_HANDLE_VALUE) U(FindClose)(h);
}

/* ---- hard-link names ---- */
static void volrel(char *out, size_t cap, const char *full) { snprintf(out, cap, "%s", full[1] == ':' ? full + 2 : full); }

TEST(links_single_name) {
    NEED_REAL();
    REQUIRE(U(FindFirstFileNameW)); REQUIRE(U(FindNextFileNameW)); REQUIRE(U(FindClose));
    NEED_NTFS();
    char *d = dh_dir("ln_one"), p[MAX_PATH], want[MAX_PATH]; WCHAR w[MAX_PATH], out[MAX_PATH], ww[MAX_PATH];
    dh_mkfile(d, "only", 1); sub(p, sizeof p, d, "only"); dh_w(w, MAX_PATH, p);
    volrel(want, sizeof want, p); dh_w(ww, MAX_PATH, want);
    DWORD len = MAX_PATH;
    HANDLE h = U(FindFirstFileNameW)(w, 0, &len, out);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    CHECK(!_wcsicmp(out, ww));
    EXPECT_EQ(len, wcslen(ww) + 1);
    len = MAX_PATH;
    SET_ERR(0);
    CHECK(!U(FindNextFileNameW)(h, &len, out));
    EXPECT_ERR(ERROR_HANDLE_EOF);
    CHECK(U(FindClose)(h));
}
TEST(links_multiple_names) {
    NEED_REAL();
    REQUIRE(U(FindFirstFileNameW)); REQUIRE(U(FindNextFileNameW)); REQUIRE(U(FindClose));
    NEED_NTFS();
    char *d = dh_dir("ln_multi"), p[MAX_PATH], q[MAX_PATH], r[MAX_PATH]; WCHAR w[MAX_PATH], out[MAX_PATH];
    dh_mkfile(d, "orig", 1); dh_mkdir(d, "sub");
    sub(p, sizeof p, d, "orig"); sub(q, sizeof q, d, "second"); sub(r, sizeof r, d, "sub\\third");
    if (!CreateHardLinkA(q, p, NULL) || !CreateHardLinkA(r, p, NULL)) { t_skip("cannot create hard links"); return; }
    char *exp[3] = { p, q, r }; int seen[3] = {0};
    dh_w(w, MAX_PATH, p);
    DWORD len = MAX_PATH; int n = 0;
    HANDLE h = U(FindFirstFileNameW)(w, 0, &len, out);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        n++;
        for (int i = 0; i < 3; i++) { char v[MAX_PATH]; WCHAR vw[MAX_PATH]; volrel(v, sizeof v, exp[i]); dh_w(vw, MAX_PATH, v); if (!_wcsicmp(vw, out)) seen[i]++; }
        len = MAX_PATH;
    } while (U(FindNextFileNameW)(h, &len, out));
    EXPECT_ERR(ERROR_HANDLE_EOF);
    U(FindClose)(h);
    EXPECT_EQ(n, 3);
    for (int i = 0; i < 3; i++) EXPECT_EQ(seen[i], 1);
}
TEST(links_query_via_any_name) {
    NEED_REAL();
    REQUIRE(U(FindFirstFileNameW)); REQUIRE(U(FindNextFileNameW)); REQUIRE(U(FindClose));
    NEED_NTFS();
    char *d = dh_dir("ln_any"), p[MAX_PATH], q[MAX_PATH]; WCHAR w[MAX_PATH], out[MAX_PATH];
    dh_mkfile(d, "orig", 1); sub(p, sizeof p, d, "orig"); sub(q, sizeof q, d, "alias");
    if (!CreateHardLinkA(q, p, NULL)) { t_skip("no hard links"); return; }
    dh_w(w, MAX_PATH, q);
    DWORD len = MAX_PATH; int n = 0;
    HANDLE h = U(FindFirstFileNameW)(w, 0, &len, out);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    do { n++; len = MAX_PATH; } while (U(FindNextFileNameW)(h, &len, out));
    U(FindClose)(h);
    EXPECT_EQ(n, 2);
}
TEST(links_buffer_too_small_first) {
    NEED_REAL();
    REQUIRE(U(FindFirstFileNameW));
    NEED_NTFS();
    char *d = dh_dir("ln_small"), p[MAX_PATH], want[MAX_PATH]; WCHAR w[MAX_PATH], out[MAX_PATH];
    dh_mkfile(d, "f", 1); sub(p, sizeof p, d, "f"); dh_w(w, MAX_PATH, p);
    volrel(want, sizeof want, p);
    DWORD need = (DWORD)strlen(want) + 1, len = 1;
    SET_ERR(0);
    CHECK(U(FindFirstFileNameW)(w, 0, &len, out) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_MORE_DATA);
    EXPECT_EQ(len, need);
    len = need - 1;
    CHECK(U(FindFirstFileNameW)(w, 0, &len, out) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_MORE_DATA);
    EXPECT_EQ(len, need);
    len = 0;
    CHECK(U(FindFirstFileNameW)(w, 0, &len, NULL) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_MORE_DATA);
    EXPECT_EQ(len, need);
}
TEST(links_exact_buffer_ok) {
    NEED_REAL();
    REQUIRE(U(FindFirstFileNameW)); REQUIRE(U(FindClose));
    NEED_NTFS();
    char *d = dh_dir("ln_exact"), p[MAX_PATH], want[MAX_PATH]; WCHAR w[MAX_PATH], out[MAX_PATH];
    dh_mkfile(d, "f", 1); sub(p, sizeof p, d, "f"); dh_w(w, MAX_PATH, p);
    volrel(want, sizeof want, p);
    DWORD len = (DWORD)strlen(want) + 1;
    HANDLE h = U(FindFirstFileNameW)(w, 0, &len, out);
    CHECKF(h != INVALID_HANDLE_VALUE, "err %lu", GetLastError());
    if (h != INVALID_HANDLE_VALUE) { EXPECT_EQ(len, strlen(want) + 1); U(FindClose)(h); }
}
TEST(links_next_buffer_too_small_then_retry) {
    NEED_REAL();
    REQUIRE(U(FindFirstFileNameW)); REQUIRE(U(FindNextFileNameW)); REQUIRE(U(FindClose));
    NEED_NTFS();
    char *d = dh_dir("ln_retry"), p[MAX_PATH], q[MAX_PATH]; WCHAR w[MAX_PATH], out[MAX_PATH];
    dh_mkfile(d, "orig", 1); sub(p, sizeof p, d, "orig"); sub(q, sizeof q, d, "a_much_longer_alias_name");
    if (!CreateHardLinkA(q, p, NULL)) { t_skip("no hard links"); return; }
    dh_w(w, MAX_PATH, p);
    DWORD len = MAX_PATH;
    HANDLE h = U(FindFirstFileNameW)(w, 0, &len, out);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    len = 1;
    SET_ERR(0);
    CHECK(!U(FindNextFileNameW)(h, &len, out));
    EXPECT_ERR(ERROR_MORE_DATA);
    CHECK(len > 1);
    DWORD need = len;
    len = MAX_PATH;
    CHECK(U(FindNextFileNameW)(h, &len, out));       /* position not consumed by the failed attempt */
    EXPECT_EQ(len, need);
    len = MAX_PATH;
    CHECK(!U(FindNextFileNameW)(h, &len, out));
    EXPECT_ERR(ERROR_HANDLE_EOF);
    U(FindClose)(h);
}
TEST(links_invalid_flags) {
    NEED_REAL();
    REQUIRE(U(FindFirstFileNameW));
    char *d = dh_dir("ln_flags"), p[MAX_PATH]; WCHAR w[MAX_PATH], out[MAX_PATH];
    dh_mkfile(d, "f", 1); sub(p, sizeof p, d, "f"); dh_w(w, MAX_PATH, p);
    DWORD len = MAX_PATH;
    SET_ERR(0);
    CHECK(U(FindFirstFileNameW)(w, 1, &len, out) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CHECK(U(FindFirstFileNameW)(w, 0x80000000, &len, out) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(links_missing_file) {
    NEED_REAL();
    REQUIRE(U(FindFirstFileNameW));
    char *d = dh_dir("ln_missing"), p[MAX_PATH]; WCHAR w[MAX_PATH], out[MAX_PATH]; DWORD len = MAX_PATH;
    sub(p, sizeof p, d, "nosuch"); dh_w(w, MAX_PATH, p);
    SET_ERR(0);
    CHECK(U(FindFirstFileNameW)(w, 0, &len, out) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    sub(p, sizeof p, d, "nodir\\nosuch"); dh_w(w, MAX_PATH, p);
    CHECK(U(FindFirstFileNameW)(w, 0, &len, out) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(links_empty_name) {
    NEED_REAL();
    REQUIRE(U(FindFirstFileNameW));
    WCHAR out[MAX_PATH]; DWORD len = MAX_PATH;
    SET_ERR(0);
    CHECK(U(FindFirstFileNameW)(L"", 0, &len, out) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(links_directory_name) {
    NEED_REAL();
    REQUIRE(U(FindFirstFileNameW));
    NEED_NTFS();
    char *d = dh_dir("ln_dir"), p[MAX_PATH]; WCHAR w[MAX_PATH], out[MAX_PATH]; DWORD len = MAX_PATH;
    dh_mkdir(d, "dd"); sub(p, sizeof p, d, "dd"); dh_w(w, MAX_PATH, p);
    SET_ERR(0);
    HANDLE h = U(FindFirstFileNameW)(w, 0, &len, out);
    t_note("FindFirstFileNameW on a directory: %s err=%lu", h != INVALID_HANDLE_VALUE ? "ok" : "failed", GetLastError());
    if (h != INVALID_HANDLE_VALUE) U(FindClose)(h);
}
TEST(links_name_starts_with_backslash_no_drive) {
    NEED_REAL();
    REQUIRE(U(FindFirstFileNameW)); REQUIRE(U(FindClose));
    NEED_NTFS();
    char *d = dh_dir("ln_form"), p[MAX_PATH]; WCHAR w[MAX_PATH], out[MAX_PATH]; DWORD len = MAX_PATH;
    dh_mkfile(d, "f", 1); sub(p, sizeof p, d, "f"); dh_w(w, MAX_PATH, p);
    HANDLE h = U(FindFirstFileNameW)(w, 0, &len, out);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) { CHECK(out[0] == L'\\' && out[1] != L':'); U(FindClose)(h); }
}
TEST(links_unlink_updates_enumeration) {
    NEED_REAL();
    REQUIRE(U(FindFirstFileNameW)); REQUIRE(U(FindNextFileNameW)); REQUIRE(U(FindClose));
    NEED_NTFS();
    char *d = dh_dir("ln_unlink"), p[MAX_PATH], q[MAX_PATH]; WCHAR w[MAX_PATH], out[MAX_PATH]; DWORD len = MAX_PATH;
    dh_mkfile(d, "orig", 1); sub(p, sizeof p, d, "orig"); sub(q, sizeof q, d, "alias");
    if (!CreateHardLinkA(q, p, NULL)) { t_skip("no hard links"); return; }
    DeleteFileA(q);
    dh_w(w, MAX_PATH, p);
    HANDLE h = U(FindFirstFileNameW)(w, 0, &len, out);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    len = MAX_PATH;
    CHECK(!U(FindNextFileNameW)(h, &len, out));
    U(FindClose)(h);
}
TEST(links_unicode_name) {
    NEED_REAL();
    REQUIRE(U(FindFirstFileNameW)); REQUIRE(U(FindClose));
    NEED_NTFS();
    char *d = dh_dir("ln_uni"); WCHAR dw[MAX_PATH], w[MAX_PATH], out[MAX_PATH]; DWORD len = MAX_PATH;
    dh_w(dw, MAX_PATH, d);
    dh_mkfileW(dw, L"\u65e5\u672c.txt", 1);
    snwprintf(w, MAX_PATH, L"%ls\\\u65e5\u672c.txt", dw);
    HANDLE h = U(FindFirstFileNameW)(w, 0, &len, out);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) { CHECK(wcsstr(out, L"\u65e5\u672c.txt") != NULL); U(FindClose)(h); }
    dh_rmtreeW(dw);
}
TEST(links_stream_path_input) {
    NEED_REAL();
    REQUIRE(U(FindFirstFileNameW));
    NEED_NTFS();
    char *d = dh_dir("ln_stream"), p[MAX_PATH], sp[MAX_PATH * 2]; WCHAR w[MAX_PATH], out[MAX_PATH]; DWORD len = MAX_PATH;
    dh_mkfile(d, "f", 1); sub(p, sizeof p, d, "f"); mkstream(p, "s", "x");
    snprintf(sp, sizeof sp, "%s:s", p); dh_w(w, MAX_PATH, sp);
    HANDLE h = U(FindFirstFileNameW)(w, 0, &len, out);
    t_note("FindFirstFileNameW on 'file:stream': %s err=%lu name=%ls", h != INVALID_HANDLE_VALUE ? "ok" : "failed", GetLastError(), h != INVALID_HANDLE_VALUE ? out : L"-");
    if (h != INVALID_HANDLE_VALUE) U(FindClose)(h);
}
TEST(links_many_names) {
    NEED_REAL();
    REQUIRE(U(FindFirstFileNameW)); REQUIRE(U(FindNextFileNameW)); REQUIRE(U(FindClose));
    NEED_NTFS();
    char *d = dh_dir("ln_many"), p[MAX_PATH], q[MAX_PATH]; WCHAR w[MAX_PATH], out[MAX_PATH]; DWORD len = MAX_PATH;
    dh_mkfile(d, "base", 1); sub(p, sizeof p, d, "base");
    int made = 1;
    for (int i = 0; i < 40; i++) { char n[16]; snprintf(n, sizeof n, "l%02d", i); sub(q, sizeof q, d, n); if (CreateHardLinkA(q, p, NULL)) made++; }
    if (made < 41) { t_skip("hard links unavailable"); return; }
    dh_w(w, MAX_PATH, p);
    HANDLE h = U(FindFirstFileNameW)(w, 0, &len, out);
    CHECK(h != INVALID_HANDLE_VALUE);
    int n = 0;
    if (h != INVALID_HANDLE_VALUE) { do { n++; len = MAX_PATH; } while (U(FindNextFileNameW)(h, &len, out)); U(FindClose)(h); }
    EXPECT_EQ(n, 41);
}
