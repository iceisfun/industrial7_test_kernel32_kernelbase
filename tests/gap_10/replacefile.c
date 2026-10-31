/* gap_10: ReplaceFile (kernel32 export), ReplaceFileA, ReplaceFileW */
#include "g10.h"

typedef BOOL (WINAPI *replacew_t)(LPCWSTR, LPCWSTR, LPCWSTR, DWORD, LPVOID, LPVOID);
static replacew_t rf_export(void) { return (replacew_t)t_proc_k32("ReplaceFile"); }

static void wfile(const char *leaf, const void *data, DWORD n) {
    char p[MAX_PATH]; DWORD w; t_tmpfile(p, sizeof p, leaf);
    HANDLE h = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) { WriteFile(h, data, n, &w, NULL); CloseHandle(h); }
}
static int rfile(const char *leaf, char *out, int cap) {
    char p[MAX_PATH]; DWORD r = 0; t_tmpfile(p, sizeof p, leaf);
    HANDLE h = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return -1;
    ReadFile(h, out, cap - 1, &r, NULL); CloseHandle(h);
    out[r] = 0;
    return (int)r;
}
static int exists(const char *leaf) { char p[MAX_PATH]; t_tmpfile(p, sizeof p, leaf); return GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES; }
static void wp(const char *leaf, WCHAR *out) { char p[MAX_PATH]; t_tmpfile(p, sizeof p, leaf); MultiByteToWideChar(CP_ACP, 0, p, -1, out, MAX_PATH); }
static void ap(const char *leaf, char *out) { t_tmpfile(out, MAX_PATH, leaf); }
static int content_is(const char *leaf, const char *want) { char b[256]; return rfile(leaf, b, sizeof b) >= 0 && !strcmp(b, want); }
static void cleanup(const char *a, const char *b, const char *c) {
    char p[MAX_PATH];
    const char *l[] = { a, b, c };
    for (int i = 0; i < 3; i++) if (l[i]) { t_tmpfile(p, sizeof p, l[i]); SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL); DeleteFileA(p); }
}

TEST(gap10_replace_w_basic) {
    REQUIRE(U(ReplaceFileW));
    wfile("rp_a.txt", "orig", 4); wfile("rp_b.txt", "newer", 5);
    WCHAR a[MAX_PATH], b[MAX_PATH]; wp("rp_a.txt", a); wp("rp_b.txt", b);
    SET_ERR(0xBAD);
    BOOL r = U(ReplaceFileW)(a, b, NULL, 0, NULL, NULL);
    EQ1(r, TRUE);
    CHECK(content_is("rp_a.txt", "newer"));
    CHECK(!exists("rp_b.txt"));
    cleanup("rp_a.txt", "rp_b.txt", NULL);
}
TEST(gap10_replace_a_basic) {
    REQUIRE(U(ReplaceFileA));
    wfile("rp_a.txt", "orig", 4); wfile("rp_b.txt", "newer", 5);
    char a[MAX_PATH], b[MAX_PATH]; ap("rp_a.txt", a); ap("rp_b.txt", b);
    BOOL r = U(ReplaceFileA)(a, b, NULL, 0, NULL, NULL);
    EQ1(r, TRUE);
    CHECK(content_is("rp_a.txt", "newer"));
    CHECK(!exists("rp_b.txt"));
    cleanup("rp_a.txt", "rp_b.txt", NULL);
}
TEST(gap10_replace_export_is_w_variant) {
    replacew_t f = rf_export();
    REQUIRE(f);
    if (!t_is_wine()) CHECK((void *)f == (void *)t_proc_k32("ReplaceFileW"));
    wfile("rp_a.txt", "orig", 4); wfile("rp_b.txt", "viaexport", 9);
    WCHAR a[MAX_PATH], b[MAX_PATH]; wp("rp_a.txt", a); wp("rp_b.txt", b);
    BOOL r = f(a, b, NULL, 0, NULL, NULL);
    EQ1(r, TRUE);
    CHECK(content_is("rp_a.txt", "viaexport"));
    cleanup("rp_a.txt", "rp_b.txt", NULL);
}
TEST(gap10_replace_with_backup) {
    REQUIRE(U(ReplaceFileW));
    wfile("rp_a.txt", "orig", 4); wfile("rp_b.txt", "newer", 5);
    WCHAR a[MAX_PATH], b[MAX_PATH], k[MAX_PATH]; wp("rp_a.txt", a); wp("rp_b.txt", b); wp("rp_k.txt", k);
    BOOL r = U(ReplaceFileW)(a, b, k, 0, NULL, NULL);
    EQ1(r, TRUE);
    CHECK(content_is("rp_a.txt", "newer"));
    CHECK(content_is("rp_k.txt", "orig"));
    CHECK(!exists("rp_b.txt"));
    cleanup("rp_a.txt", "rp_b.txt", "rp_k.txt");
}
TEST(gap10_replace_a_with_backup) {
    REQUIRE(U(ReplaceFileA));
    wfile("rp_a.txt", "orig", 4); wfile("rp_b.txt", "newer", 5);
    char a[MAX_PATH], b[MAX_PATH], k[MAX_PATH]; ap("rp_a.txt", a); ap("rp_b.txt", b); ap("rp_k.txt", k);
    BOOL r = U(ReplaceFileA)(a, b, k, 0, NULL, NULL);
    EQ1(r, TRUE);
    CHECK(content_is("rp_a.txt", "newer"));
    CHECK(content_is("rp_k.txt", "orig"));
    cleanup("rp_a.txt", "rp_b.txt", "rp_k.txt");
}
TEST(gap10_replace_existing_backup_overwritten) {
    REQUIRE(U(ReplaceFileW));
    wfile("rp_a.txt", "orig", 4); wfile("rp_b.txt", "newer", 5); wfile("rp_k.txt", "oldbackup", 9);
    WCHAR a[MAX_PATH], b[MAX_PATH], k[MAX_PATH]; wp("rp_a.txt", a); wp("rp_b.txt", b); wp("rp_k.txt", k);
    BOOL r = U(ReplaceFileW)(a, b, k, 0, NULL, NULL);
    EQ1(r, TRUE);
    CHECK(content_is("rp_k.txt", "orig"));
    CHECK(content_is("rp_a.txt", "newer"));
    cleanup("rp_a.txt", "rp_b.txt", "rp_k.txt");
}
TEST(gap10_replace_valid_flag_combinations) {
    REQUIRE(U(ReplaceFileW));
    for (DWORD fl = 0; fl < 8; fl++) {
        wfile("rp_a.txt", "orig", 4); wfile("rp_b.txt", "newer", 5);
        WCHAR a[MAX_PATH], b[MAX_PATH]; wp("rp_a.txt", a); wp("rp_b.txt", b);
        BOOL r = U(ReplaceFileW)(a, b, NULL, fl, NULL, NULL);
        CHECKF(r, "flags %lu failed err=%lu", fl, GetLastError());
        CHECK(content_is("rp_a.txt", "newer"));
        cleanup("rp_a.txt", "rp_b.txt", NULL);
    }
}
TEST(gap10_replace_a_valid_flag_combinations) {
    REQUIRE(U(ReplaceFileA));
    for (DWORD fl = 0; fl < 8; fl++) {
        wfile("rp_a.txt", "orig", 4); wfile("rp_b.txt", "newer", 5);
        char a[MAX_PATH], b[MAX_PATH]; ap("rp_a.txt", a); ap("rp_b.txt", b);
        BOOL r = U(ReplaceFileA)(a, b, NULL, fl, NULL, NULL);
        CHECKF(r, "flags %lu failed err=%lu", fl, GetLastError());
        cleanup("rp_a.txt", "rp_b.txt", NULL);
    }
}
/* decomp ReplaceFileExInternal: (flags & ~7) != 0 -> ERROR_INVALID_PARAMETER before any file access */
TEST(gap10_replace_invalid_flags) {
    REQUIRE(U(ReplaceFileW)); REQUIRE(U(ReplaceFileA));
    static const DWORD bad[] = { 8, 0x10, 0x100, 0x80000000, 0xFFFFFFF8, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        wfile("rp_a.txt", "orig", 4); wfile("rp_b.txt", "newer", 5);
        WCHAR a[MAX_PATH], b[MAX_PATH]; char aa[MAX_PATH], ba[MAX_PATH];
        wp("rp_a.txt", a); wp("rp_b.txt", b); ap("rp_a.txt", aa); ap("rp_b.txt", ba);
        SET_ERR(0xBAD); BOOL r = U(ReplaceFileW)(a, b, NULL, bad[i], NULL, NULL);
        EQ1(r, 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
        SET_ERR(0xBAD); r = U(ReplaceFileA)(aa, ba, NULL, bad[i], NULL, NULL);
        EQ1(r, 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
        CHECK(content_is("rp_a.txt", "orig")); CHECK(content_is("rp_b.txt", "newer"));
        cleanup("rp_a.txt", "rp_b.txt", NULL);
    }
}
TEST(gap10_replace_null_names) {
    REQUIRE(U(ReplaceFileW)); REQUIRE(U(ReplaceFileA));
    wfile("rp_a.txt", "orig", 4);
    WCHAR a[MAX_PATH]; char aa[MAX_PATH]; wp("rp_a.txt", a); ap("rp_a.txt", aa);
    SET_ERR(0xBAD); BOOL r = U(ReplaceFileW)(NULL, a, NULL, 0, NULL, NULL); EQ1(r, 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0xBAD); r = U(ReplaceFileW)(a, NULL, NULL, 0, NULL, NULL); EQ1(r, 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0xBAD); r = U(ReplaceFileW)(NULL, NULL, NULL, 0, NULL, NULL); EQ1(r, 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0xBAD); r = U(ReplaceFileA)(NULL, aa, NULL, 0, NULL, NULL); EQ1(r, 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0xBAD); r = U(ReplaceFileA)(aa, NULL, NULL, 0, NULL, NULL); EQ1(r, 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0xBAD); r = U(ReplaceFileA)(NULL, NULL, NULL, 0, NULL, NULL); EQ1(r, 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CHECK(content_is("rp_a.txt", "orig"));
    cleanup("rp_a.txt", NULL, NULL);
}
TEST(gap10_replace_reserved_params_must_be_null) {
    REQUIRE(U(ReplaceFileW)); REQUIRE(U(ReplaceFileA));
    wfile("rp_a.txt", "orig", 4); wfile("rp_b.txt", "newer", 5);
    WCHAR a[MAX_PATH], b[MAX_PATH]; char aa[MAX_PATH], ba[MAX_PATH];
    wp("rp_a.txt", a); wp("rp_b.txt", b); ap("rp_a.txt", aa); ap("rp_b.txt", ba);
    SET_ERR(0xBAD); BOOL r = U(ReplaceFileW)(a, b, NULL, 0, (LPVOID)(ULONG_PTR)1, NULL); EQ1(r, 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0xBAD); r = U(ReplaceFileW)(a, b, NULL, 0, NULL, (LPVOID)(ULONG_PTR)1); EQ1(r, 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0xBAD); r = U(ReplaceFileA)(aa, ba, NULL, 0, (LPVOID)(ULONG_PTR)1, NULL); EQ1(r, 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0xBAD); r = U(ReplaceFileA)(aa, ba, NULL, 0, NULL, (LPVOID)(ULONG_PTR)1); EQ1(r, 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CHECK(content_is("rp_a.txt", "orig")); CHECK(content_is("rp_b.txt", "newer"));
    cleanup("rp_a.txt", "rp_b.txt", NULL);
}
TEST(gap10_replace_missing_replaced) {
    REQUIRE(U(ReplaceFileW)); REQUIRE(U(ReplaceFileA));
    wfile("rp_b.txt", "newer", 5);
    WCHAR a[MAX_PATH], b[MAX_PATH]; char aa[MAX_PATH], ba[MAX_PATH];
    wp("rp_missing.txt", a); wp("rp_b.txt", b); ap("rp_missing.txt", aa); ap("rp_b.txt", ba);
    SET_ERR(0xBAD); BOOL r = U(ReplaceFileW)(a, b, NULL, 0, NULL, NULL); EQ1(r, 0); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    SET_ERR(0xBAD); r = U(ReplaceFileA)(aa, ba, NULL, 0, NULL, NULL); EQ1(r, 0); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    CHECK(content_is("rp_b.txt", "newer")); CHECK(!exists("rp_missing.txt"));
    cleanup("rp_b.txt", NULL, NULL);
}
TEST(gap10_replace_missing_replacement) {
    REQUIRE(U(ReplaceFileW)); REQUIRE(U(ReplaceFileA));
    wfile("rp_a.txt", "orig", 4);
    WCHAR a[MAX_PATH], b[MAX_PATH]; char aa[MAX_PATH], ba[MAX_PATH];
    wp("rp_a.txt", a); wp("rp_missing.txt", b); ap("rp_a.txt", aa); ap("rp_missing.txt", ba);
    SET_ERR(0xBAD); BOOL r = U(ReplaceFileW)(a, b, NULL, 0, NULL, NULL); EQ1(r, 0);
    t_note("missing replacement (W): err=%lu", GetLastError());
    CHECK(GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_UNABLE_TO_MOVE_REPLACEMENT || GetLastError() == ERROR_UNABLE_TO_MOVE_REPLACEMENT_2);
    SET_ERR(0xBAD); r = U(ReplaceFileA)(aa, ba, NULL, 0, NULL, NULL); EQ1(r, 0);
    CHECK(content_is("rp_a.txt", "orig"));
    cleanup("rp_a.txt", NULL, NULL);
}
TEST(gap10_replace_both_missing) {
    REQUIRE(U(ReplaceFileW));
    WCHAR a[MAX_PATH], b[MAX_PATH]; wp("rp_m1.txt", a); wp("rp_m2.txt", b);
    SET_ERR(0xBAD); BOOL r = U(ReplaceFileW)(a, b, NULL, 0, NULL, NULL);
    EQ1(r, 0); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(gap10_replace_missing_directory) {
    REQUIRE(U(ReplaceFileW));
    wfile("rp_b.txt", "newer", 5);
    WCHAR a[MAX_PATH], b[MAX_PATH]; wp("nodir\\x.txt", a); wp("rp_b.txt", b);
    SET_ERR(0xBAD); BOOL r = U(ReplaceFileW)(a, b, NULL, 0, NULL, NULL);
    EQ1(r, 0); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    CHECK(content_is("rp_b.txt", "newer"));
    cleanup("rp_b.txt", NULL, NULL);
}
TEST(gap10_replace_empty_names) {
    REQUIRE(U(ReplaceFileW));
    wfile("rp_a.txt", "orig", 4);
    WCHAR a[MAX_PATH]; wp("rp_a.txt", a);
    SET_ERR(0xBAD); BOOL r = U(ReplaceFileW)(L"", a, NULL, 0, NULL, NULL); EQ1(r, 0);
    t_note("empty replaced name: err=%lu", GetLastError());
    SET_ERR(0xBAD); r = U(ReplaceFileW)(a, L"", NULL, 0, NULL, NULL); EQ1(r, 0);
    t_note("empty replacement name: err=%lu", GetLastError());
    CHECK(content_is("rp_a.txt", "orig"));
    cleanup("rp_a.txt", NULL, NULL);
}
TEST(gap10_replace_readonly_replaced_is_access_denied) {
    REQUIRE(U(ReplaceFileW));
    wfile("rp_a.txt", "orig", 4); wfile("rp_b.txt", "newer", 5);
    char pa[MAX_PATH]; ap("rp_a.txt", pa);
    SetFileAttributesA(pa, FILE_ATTRIBUTE_READONLY);
    WCHAR a[MAX_PATH], b[MAX_PATH]; wp("rp_a.txt", a); wp("rp_b.txt", b);
    SET_ERR(0xBAD); BOOL r = U(ReplaceFileW)(a, b, NULL, 0, NULL, NULL);
    EQ1(r, 0); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CHECK(content_is("rp_a.txt", "orig")); CHECK(content_is("rp_b.txt", "newer"));
    cleanup("rp_a.txt", "rp_b.txt", NULL);
}
TEST(gap10_replace_readonly_replaced_with_ignore_flags) {
    REQUIRE(U(ReplaceFileW));
    wfile("rp_a.txt", "orig", 4); wfile("rp_b.txt", "newer", 5);
    char pa[MAX_PATH]; ap("rp_a.txt", pa);
    SetFileAttributesA(pa, FILE_ATTRIBUTE_READONLY);
    WCHAR a[MAX_PATH], b[MAX_PATH]; wp("rp_a.txt", a); wp("rp_b.txt", b);
    SET_ERR(0xBAD); BOOL r = U(ReplaceFileW)(a, b, NULL, REPLACEFILE_IGNORE_MERGE_ERRORS | REPLACEFILE_IGNORE_ACL_ERRORS, NULL, NULL);
    t_note("readonly replaced with ignore flags: ret=%d err=%lu", r, r ? 0 : GetLastError());
    cleanup("rp_a.txt", "rp_b.txt", NULL);
}
TEST(gap10_replace_readonly_replacement) {
    REQUIRE(U(ReplaceFileW));
    wfile("rp_a.txt", "orig", 4); wfile("rp_b.txt", "newer", 5);
    char pb[MAX_PATH]; ap("rp_b.txt", pb);
    SetFileAttributesA(pb, FILE_ATTRIBUTE_READONLY);
    WCHAR a[MAX_PATH], b[MAX_PATH]; wp("rp_a.txt", a); wp("rp_b.txt", b);
    SET_ERR(0xBAD); BOOL r = U(ReplaceFileW)(a, b, NULL, 0, NULL, NULL);
    t_note("readonly replacement: ret=%d err=%lu", r, r ? 0 : GetLastError());
    if (r) CHECK(content_is("rp_a.txt", "newer"));
    cleanup("rp_a.txt", "rp_b.txt", NULL);
}
TEST(gap10_replace_takes_attributes_of_replaced) {
    REQUIRE(U(ReplaceFileW));
    wfile("rp_a.txt", "orig", 4); Sleep(30); wfile("rp_b.txt", "newer", 5);
    char pa[MAX_PATH], pb[MAX_PATH]; ap("rp_a.txt", pa); ap("rp_b.txt", pb);
    SetFileAttributesA(pa, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_ARCHIVE);
    WIN32_FILE_ATTRIBUTE_DATA before; GetFileAttributesExA(pa, GetFileExInfoStandard, &before);
    WCHAR a[MAX_PATH], b[MAX_PATH]; wp("rp_a.txt", a); wp("rp_b.txt", b);
    BOOL r = U(ReplaceFileW)(a, b, NULL, 0, NULL, NULL);
    EQ1(r, TRUE);
    DWORD at = GetFileAttributesA(pa);
    CHECK(at != INVALID_FILE_ATTRIBUTES && (at & FILE_ATTRIBUTE_HIDDEN));
    WIN32_FILE_ATTRIBUTE_DATA after; memset(&after, 0, sizeof after);
    GetFileAttributesExA(pa, GetFileExInfoStandard, &after);
    CHECK(after.ftCreationTime.dwLowDateTime == before.ftCreationTime.dwLowDateTime && after.ftCreationTime.dwHighDateTime == before.ftCreationTime.dwHighDateTime);
    cleanup("rp_a.txt", "rp_b.txt", NULL);
}
TEST(gap10_replace_backup_keeps_original_attributes) {
    REQUIRE(U(ReplaceFileW));
    wfile("rp_a.txt", "orig", 4); wfile("rp_b.txt", "newer", 5);
    char pa[MAX_PATH], pk[MAX_PATH]; ap("rp_a.txt", pa); ap("rp_k.txt", pk);
    SetFileAttributesA(pa, FILE_ATTRIBUTE_HIDDEN);
    WCHAR a[MAX_PATH], b[MAX_PATH], k[MAX_PATH]; wp("rp_a.txt", a); wp("rp_b.txt", b); wp("rp_k.txt", k);
    BOOL r = U(ReplaceFileW)(a, b, k, 0, NULL, NULL);
    EQ1(r, TRUE);
    DWORD at = GetFileAttributesA(pk);
    CHECK(at != INVALID_FILE_ATTRIBUTES && (at & FILE_ATTRIBUTE_HIDDEN));
    cleanup("rp_a.txt", "rp_b.txt", "rp_k.txt");
}
/* decomp ReplaceFileExInternal: failed rename of replaced to backup -> 0x497 (ERROR_UNABLE_TO_REMOVE_REPLACED) */
TEST(gap10_replace_backup_in_missing_directory) {
    REQUIRE(U(ReplaceFileW));
    wfile("rp_a.txt", "orig", 4); wfile("rp_b.txt", "newer", 5);
    WCHAR a[MAX_PATH], b[MAX_PATH], k[MAX_PATH]; wp("rp_a.txt", a); wp("rp_b.txt", b); wp("nodir\\k.txt", k);
    SET_ERR(0xBAD); BOOL r = U(ReplaceFileW)(a, b, k, 0, NULL, NULL);
    EQ1(r, 0); EXPECT_ERR(ERROR_UNABLE_TO_REMOVE_REPLACED);
    CHECK(content_is("rp_a.txt", "orig")); CHECK(content_is("rp_b.txt", "newer"));
    cleanup("rp_a.txt", "rp_b.txt", NULL);
}
TEST(gap10_replace_replaced_open_without_delete_sharing) {
    REQUIRE(U(ReplaceFileW));
    wfile("rp_a.txt", "orig", 4); wfile("rp_b.txt", "newer", 5);
    char pa[MAX_PATH]; ap("rp_a.txt", pa);
    HANDLE h = CreateFileA(pa, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    WCHAR a[MAX_PATH], b[MAX_PATH]; wp("rp_a.txt", a); wp("rp_b.txt", b);
    SET_ERR(0xBAD); BOOL r = U(ReplaceFileW)(a, b, NULL, 0, NULL, NULL);
    EQ1(r, 0); EXPECT_ERR(ERROR_SHARING_VIOLATION);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    CHECK(content_is("rp_a.txt", "orig")); CHECK(content_is("rp_b.txt", "newer"));
    cleanup("rp_a.txt", "rp_b.txt", NULL);
}
TEST(gap10_replace_replaced_open_with_delete_sharing) {
    REQUIRE(U(ReplaceFileW));
    wfile("rp_a.txt", "orig", 4); wfile("rp_b.txt", "newer", 5);
    char pa[MAX_PATH]; ap("rp_a.txt", pa);
    HANDLE h = CreateFileA(pa, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    WCHAR a[MAX_PATH], b[MAX_PATH]; wp("rp_a.txt", a); wp("rp_b.txt", b);
    SET_ERR(0xBAD); BOOL r = U(ReplaceFileW)(a, b, NULL, 0, NULL, NULL);
    EQ1(r, TRUE);
    CHECK(content_is("rp_a.txt", "newer"));
    if (h != INVALID_HANDLE_VALUE) {
        /* the open handle follows the original data, which was unlinked from the name */
        char buf[16] = {0}; DWORD n = 0;
        ReadFile(h, buf, 15, &n, NULL);
        t_note("open handle on replaced file now reads \"%s\"", buf);
        CloseHandle(h);
    }
    cleanup("rp_a.txt", "rp_b.txt", NULL);
}
TEST(gap10_replace_replacement_open_without_delete_sharing) {
    REQUIRE(U(ReplaceFileW));
    wfile("rp_a.txt", "orig", 4); wfile("rp_b.txt", "newer", 5);
    char pb[MAX_PATH]; ap("rp_b.txt", pb);
    HANDLE h = CreateFileA(pb, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    WCHAR a[MAX_PATH], b[MAX_PATH]; wp("rp_a.txt", a); wp("rp_b.txt", b);
    SET_ERR(0xBAD); BOOL r = U(ReplaceFileW)(a, b, NULL, 0, NULL, NULL);
    t_note("replacement open w/o delete sharing: ret=%d err=%lu", r, r ? 0 : GetLastError());
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    cleanup("rp_a.txt", "rp_b.txt", NULL);
}
TEST(gap10_replace_large_content) {
    REQUIRE(U(ReplaceFileW));
    char p[MAX_PATH]; DWORD n, sz = 1 << 20;
    unsigned char *buf = (unsigned char *)malloc(sz), *chk = (unsigned char *)malloc(sz);
    if (!buf || !chk) { t_skip("alloc"); return; }
    for (DWORD i = 0; i < sz; i++) buf[i] = (unsigned char)(i * 31 + (i >> 8));
    wfile("rp_a.txt", "small", 5); wfile("rp_b.txt", buf, sz);
    WCHAR a[MAX_PATH], b[MAX_PATH]; wp("rp_a.txt", a); wp("rp_b.txt", b);
    BOOL r = U(ReplaceFileW)(a, b, NULL, 0, NULL, NULL);
    EQ1(r, TRUE);
    ap("rp_a.txt", p);
    HANDLE h = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) {
        n = 0; ReadFile(h, chk, sz, &n, NULL);
        EQ1(n, sz); CHECK(!memcmp(buf, chk, sz));
        EQ1(GetFileSize(h, NULL), sz);
        CloseHandle(h);
    }
    free(buf); free(chk);
    cleanup("rp_a.txt", "rp_b.txt", NULL);
}
TEST(gap10_replace_empty_replacement) {
    REQUIRE(U(ReplaceFileW));
    wfile("rp_a.txt", "orig", 4); wfile("rp_b.txt", "", 0);
    WCHAR a[MAX_PATH], b[MAX_PATH]; wp("rp_a.txt", a); wp("rp_b.txt", b);
    BOOL r = U(ReplaceFileW)(a, b, NULL, 0, NULL, NULL);
    EQ1(r, TRUE);
    char buf[16]; EQ1(rfile("rp_a.txt", buf, sizeof buf), 0);
    cleanup("rp_a.txt", "rp_b.txt", NULL);
}
TEST(gap10_replace_repeated_chain) {
    REQUIRE(U(ReplaceFileW));
    wfile("rp_a.txt", "v0", 2);
    for (int i = 1; i <= 8; i++) {
        char c[8]; snprintf(c, sizeof c, "v%d", i);
        wfile("rp_b.txt", c, (DWORD)strlen(c));
        WCHAR a[MAX_PATH], b[MAX_PATH]; wp("rp_a.txt", a); wp("rp_b.txt", b);
        BOOL r = U(ReplaceFileW)(a, b, NULL, 0, NULL, NULL);
        CHECKF(r, "iteration %d err=%lu", i, GetLastError());
        CHECK(content_is("rp_a.txt", c));
    }
    cleanup("rp_a.txt", "rp_b.txt", NULL);
}
TEST(gap10_replace_backup_chain_rotates) {
    REQUIRE(U(ReplaceFileW));
    wfile("rp_a.txt", "gen0", 4);
    for (int i = 1; i <= 4; i++) {
        char c[8], prev[8]; snprintf(c, sizeof c, "gen%d", i); snprintf(prev, sizeof prev, "gen%d", i - 1);
        wfile("rp_b.txt", c, 4);
        WCHAR a[MAX_PATH], b[MAX_PATH], k[MAX_PATH]; wp("rp_a.txt", a); wp("rp_b.txt", b); wp("rp_k.txt", k);
        BOOL r = U(ReplaceFileW)(a, b, k, 0, NULL, NULL);
        CHECKF(r, "iteration %d err=%lu", i, GetLastError());
        CHECK(content_is("rp_a.txt", c)); CHECK(content_is("rp_k.txt", prev));
    }
    cleanup("rp_a.txt", "rp_b.txt", "rp_k.txt");
}
TEST(gap10_replace_into_subdirectory) {
    REQUIRE(U(ReplaceFileW));
    char d[MAX_PATH]; ap("rp_sub", d); CreateDirectoryA(d, NULL);
    wfile("rp_sub\\x.txt", "orig", 4); wfile("rp_sub\\y.txt", "newer", 5);
    WCHAR a[MAX_PATH], b[MAX_PATH]; wp("rp_sub\\x.txt", a); wp("rp_sub\\y.txt", b);
    BOOL r = U(ReplaceFileW)(a, b, NULL, 0, NULL, NULL);
    EQ1(r, TRUE);
    CHECK(content_is("rp_sub\\x.txt", "newer")); CHECK(!exists("rp_sub\\y.txt"));
    cleanup("rp_sub\\x.txt", "rp_sub\\y.txt", NULL);
    RemoveDirectoryA(d);
}
TEST(gap10_replace_across_directories) {
    REQUIRE(U(ReplaceFileW));
    char d1[MAX_PATH], d2[MAX_PATH]; ap("rp_d1", d1); ap("rp_d2", d2); CreateDirectoryA(d1, NULL); CreateDirectoryA(d2, NULL);
    wfile("rp_d1\\x.txt", "orig", 4); wfile("rp_d2\\y.txt", "newer", 5);
    WCHAR a[MAX_PATH], b[MAX_PATH], k[MAX_PATH]; wp("rp_d1\\x.txt", a); wp("rp_d2\\y.txt", b); wp("rp_d2\\k.txt", k);
    BOOL r = U(ReplaceFileW)(a, b, k, 0, NULL, NULL);
    EQ1(r, TRUE);
    CHECK(content_is("rp_d1\\x.txt", "newer")); CHECK(content_is("rp_d2\\k.txt", "orig")); CHECK(!exists("rp_d2\\y.txt"));
    cleanup("rp_d1\\x.txt", "rp_d2\\k.txt", NULL);
    RemoveDirectoryA(d1); RemoveDirectoryA(d2);
}
TEST(gap10_replace_relative_path_names) {
    REQUIRE(U(ReplaceFileW));
    WCHAR cwd[MAX_PATH]; GetCurrentDirectoryW(MAX_PATH, cwd);
    char d[MAX_PATH]; ap("", d);
    SetCurrentDirectoryA(d);
    wfile("rp_a.txt", "orig", 4); wfile("rp_b.txt", "newer", 5);
    BOOL r = U(ReplaceFileW)(L"rp_a.txt", L"rp_b.txt", NULL, 0, NULL, NULL);
    DWORD e = GetLastError();
    SetCurrentDirectoryW(cwd);
    CHECKF(r, "relative names failed err=%lu", e);
    CHECK(content_is("rp_a.txt", "newer"));
    cleanup("rp_a.txt", "rp_b.txt", NULL);
}
TEST(gap10_replace_a_and_w_same_result) {
    REQUIRE(U(ReplaceFileA)); REQUIRE(U(ReplaceFileW));
    wfile("rp_a.txt", "orig", 4);
    WCHAR a[MAX_PATH], b[MAX_PATH]; char aa[MAX_PATH], ba[MAX_PATH];
    wp("rp_a.txt", a); wp("rp_nob.txt", b); ap("rp_a.txt", aa); ap("rp_nob.txt", ba);
    SET_ERR(0xBAD); BOOL rw = U(ReplaceFileW)(a, b, NULL, 0, NULL, NULL); DWORD ew = GetLastError();
    SET_ERR(0xBAD); BOOL ra = U(ReplaceFileA)(aa, ba, NULL, 0, NULL, NULL); DWORD ea = GetLastError();
    EQ1(rw, ra); EQ1(ew, ea);
    cleanup("rp_a.txt", NULL, NULL);
}
TEST(gap10_replace_directory_as_replacement) {
    REQUIRE(U(ReplaceFileW));
    wfile("rp_a.txt", "orig", 4);
    char d[MAX_PATH], pa[MAX_PATH]; ap("rp_dir", d); ap("rp_a.txt", pa); CreateDirectoryA(d, NULL);
    WCHAR a[MAX_PATH], b[MAX_PATH]; wp("rp_a.txt", a); wp("rp_dir", b);
    SET_ERR(0xBAD); BOOL r = U(ReplaceFileW)(a, b, NULL, 0, NULL, NULL);
    t_note("directory as replacement: ret=%d err=%lu", r, r ? 0 : GetLastError());
    DWORD at = GetFileAttributesA(pa);
    if (at != INVALID_FILE_ATTRIBUTES && (at & FILE_ATTRIBUTE_DIRECTORY)) RemoveDirectoryA(pa);
    else { SetFileAttributesA(pa, FILE_ATTRIBUTE_NORMAL); DeleteFileA(pa); }
    RemoveDirectoryA(d);
}
TEST(gap10_replace_returns_to_same_state_on_failure) {
    REQUIRE(U(ReplaceFileW));
    wfile("rp_a.txt", "orig", 4);
    WCHAR a[MAX_PATH], b[MAX_PATH]; wp("rp_a.txt", a); wp("rp_nob.txt", b);
    for (int i = 0; i < 20; i++) {
        BOOL r = U(ReplaceFileW)(a, b, NULL, 0, NULL, NULL);
        CHECK(r == 0);
    }
    CHECK(content_is("rp_a.txt", "orig"));
    /* no temporary leftovers named ~RF*.TMP */
    char pat[MAX_PATH]; ap("~RF*.TMP", pat);
    WIN32_FIND_DATAA fd; HANDLE f = FindFirstFileA(pat, &fd);
    CHECK(f == INVALID_HANDLE_VALUE);
    if (f != INVALID_HANDLE_VALUE) FindClose(f);
    cleanup("rp_a.txt", NULL, NULL);
}
