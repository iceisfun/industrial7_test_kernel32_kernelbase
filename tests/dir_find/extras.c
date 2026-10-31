/* Additional dir_find coverage: struct layout, path normalisation in patterns, data preservation on failure. */
#include "dh.h"
#include <stddef.h>

static char *sub(char *buf, size_t cap, const char *dir, const char *leaf) { dh_path(buf, cap, dir, leaf); return buf; }

_Static_assert(sizeof(WIN32_FIND_DATAW) == 592, "WIN32_FIND_DATAW size");
_Static_assert(sizeof(WIN32_FIND_DATAA) == 320, "WIN32_FIND_DATAA size");
_Static_assert(offsetof(WIN32_FIND_DATAW, cFileName) == 44, "cFileName offset");
_Static_assert(offsetof(WIN32_FIND_DATAW, cAlternateFileName) == 44 + 520, "cAlternateFileName offset");
_Static_assert(offsetof(WIN32_FIND_DATAA, cAlternateFileName) == 44 + 260, "A alt offset");

TEST(dfx_find_data_layout_runtime) {
    REQUIRE(U(FindFirstFileW)); REQUIRE(U(FindClose));
    char *d = dh_dir("dfx_layout"); dh_mkfile(d, "layout.bin", 77);
    WCHAR w[MAX_PATH]; WIN32_FIND_DATAW fd; memset(&fd, 0, sizeof fd);
    dh_w(w, MAX_PATH, d); wcscat(w, L"\\layout.bin");
    HANDLE h = U(FindFirstFileW)(w, &fd);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    EXPECT_EQ(fd.nFileSizeHigh, 0); EXPECT_EQ(fd.nFileSizeLow, 77);
    CHECK(!wcscmp(fd.cFileName, L"layout.bin"));
    CHECK(fd.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE);
    CHECK(!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));
    U(FindClose)(h);
}

TEST(dfx_find_case_preserved) {
    REQUIRE(U(FindFirstFileA)); REQUIRE(U(FindNextFileA)); REQUIRE(U(FindClose));
    char *d = dh_dir("dfx_case"), p[MAX_PATH]; dh_name l[8];
    dh_mkfile(d, "MiXeD_CaSe.TxT", 1);
    sub(p, sizeof p, d, "mixed_case.txt");
    int n = dh_listA(p, l, 8);
    EXPECT_EQ(n, 1);
    if (n == 1) EXPECT_STR(l[0].n, "MiXeD_CaSe.TxT");
    sub(p, sizeof p, d, "MIXED*");
    n = dh_listA(p, l, 8);
    EXPECT_EQ(n, 1);
}

TEST(dfx_find_dotdot_in_directory_part) {
    REQUIRE(U(FindFirstFileA));
    char *d = dh_dir("dfx_dotdot"), p[MAX_PATH * 2]; dh_name l[16];
    dh_mkdir(d, "sub"); dh_mkfile(d, "top1", 1); dh_mkfile(d, "top2", 1);
    snprintf(p, sizeof p, "%s\\sub\\..\\top*", d);
    int n = dh_listA(p, l, 16);
    EXPECT_EQ(n, 2);
    CHECK(dh_has(l, n > 16 ? 16 : n, "top1") && dh_has(l, n > 16 ? 16 : n, "top2"));
    snprintf(p, sizeof p, "%s\\.\\.\\top*", d);
    EXPECT_EQ(dh_listA(p, l, 16), 2);
    snprintf(p, sizeof p, "%s\\sub\\..\\sub\\..\\top1", d);
    EXPECT_EQ(dh_listA(p, l, 16), 1);
}

TEST(dfx_find_forward_slash_and_doubled_sep) {
    REQUIRE(U(FindFirstFileA));
    char *d = dh_dir("dfx_slash"), p[MAX_PATH * 2]; dh_name l[16];
    dh_mkfile(d, "f1", 1); dh_mkfile(d, "f2", 1);
    snprintf(p, sizeof p, "%s/f*", d);
    EXPECT_EQ(dh_listA(p, l, 16), 2);
    snprintf(p, sizeof p, "%s\\\\f*", d);
    EXPECT_EQ(dh_listA(p, l, 16), 2);
}

TEST(dfx_find_via_short_directory_name) {
    REQUIRE(U(FindFirstFileA)); REQUIRE(U(GetShortPathNameA));
    char *d = dh_dir("dfx_shortdir_long_directory_name"), p[MAX_PATH * 2], s[MAX_PATH * 2]; dh_name l[16];
    dh_mkfile(d, "inner_long_file_name.txt", 1);
    DWORD n = U(GetShortPathNameA)(d, s, sizeof s);
    if (!n || n >= sizeof s || !strcmp(s, d)) { t_skip("8.3 names unavailable"); return; }
    snprintf(p, sizeof p, "%s\\*", s);
    int c = dh_listA_nd(p, l, 16);
    EXPECT_EQ(c, 1);
    if (c == 1) EXPECT_STR(l[0].n, "inner_long_file_name.txt");
}

TEST(dfx_find_failure_leaves_data_untouched) {
    REQUIRE(U(FindFirstFileExW)); REQUIRE(U(FindFirstFileA));
    char *d = dh_dir("dfx_untouched"), p[MAX_PATH]; WCHAR w[MAX_PATH]; WIN32_FIND_DATAW fw; WIN32_FIND_DATAA fa;
    sub(p, sizeof p, d, "nomatch*"); dh_w(w, MAX_PATH, p);
    memset(&fw, 0xEE, sizeof fw);
    CHECK(U(FindFirstFileExW)(w, FindExInfoStandard, &fw, FindExSearchNameMatch, NULL, 0) == INVALID_HANDLE_VALUE);
    CHECK(fw.dwFileAttributes == 0xEEEEEEEE && fw.cFileName[0] == 0xEEEE);
    sub(p, sizeof p, d, "missing_dir\\x");
    memset(&fa, 0xEE, sizeof fa);
    CHECK(U(FindFirstFileA)(p, &fa) == INVALID_HANDLE_VALUE);
    CHECK(fa.dwFileAttributes == 0xEEEEEEEE && (unsigned char)fa.cFileName[0] == 0xEE);
}

TEST(dfx_find_two_interleaved_enumerations) {
    REQUIRE(U(FindFirstFileA)); REQUIRE(U(FindNextFileA)); REQUIRE(U(FindClose));
    char *d = dh_dir("dfx_interleave"), p[MAX_PATH], n[16]; WIN32_FIND_DATAA f1, f2;
    for (int i = 0; i < 10; i++) { snprintf(n, sizeof n, "i%02d", i); dh_mkfile(d, n, 1); }
    sub(p, sizeof p, d, "i*");
    HANDLE h1 = U(FindFirstFileA)(p, &f1), h2 = U(FindFirstFileA)(p, &f2);
    CHECK(h1 != INVALID_HANDLE_VALUE && h2 != INVALID_HANDLE_VALUE && h1 != h2);
    if (h1 == INVALID_HANDLE_VALUE || h2 == INVALID_HANDLE_VALUE) { if (h1 != INVALID_HANDLE_VALUE) U(FindClose)(h1); if (h2 != INVALID_HANDLE_VALUE) U(FindClose)(h2); return; }
    int c1 = 1, c2 = 1, same = !strcmp(f1.cFileName, f2.cFileName);
    for (;;) {
        BOOL a = U(FindNextFileA)(h1, &f1), b = U(FindNextFileA)(h2, &f2);
        CHECK(a == b);
        if (!a || !b) break;
        c1++; c2++;
        same &= !strcmp(f1.cFileName, f2.cFileName);
    }
    EXPECT_EQ(c1, 10); EXPECT_EQ(c2, 10); CHECK(same);
    U(FindClose)(h1); U(FindClose)(h2);
}

TEST(dfx_dir_times_are_current) {
    REQUIRE(U(CreateDirectoryA)); REQUIRE(U(FindFirstFileA)); REQUIRE(U(FindClose));
    char *d = dh_dir("dfx_times"), p[MAX_PATH]; WIN32_FIND_DATAA fd;
    FILETIME now; GetSystemTimeAsFileTime(&now);
    ULARGE_INTEGER n; n.LowPart = now.dwLowDateTime; n.HighPart = now.dwHighDateTime;
    sub(p, sizeof p, d, "child");
    CHECK(U(CreateDirectoryA)(p, NULL));
    HANDLE h = U(FindFirstFileA)(p, &fd);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    ULARGE_INTEGER c; c.LowPart = fd.ftCreationTime.dwLowDateTime; c.HighPart = fd.ftCreationTime.dwHighDateTime;
    long long diff = (long long)c.QuadPart - (long long)n.QuadPart;
    if (diff < 0) diff = -diff;
    CHECKF(diff < 10LL * 60 * 10000000, "creation time differs from now by %lld ticks", diff);
    CHECK(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
    EXPECT_EQ(fd.nFileSizeLow, 0); EXPECT_EQ(fd.nFileSizeHigh, 0);
    U(FindClose)(h);
}

TEST(dfx_find_names_with_multiple_dots) {
    REQUIRE(U(FindFirstFileA));
    char *d = dh_dir("dfx_dots"), p[MAX_PATH]; dh_name l[16];
    dh_mkfile(d, "a..b", 1); dh_mkfile(d, "a.b.c.d", 1); dh_mkfile(d, "..lead", 1);
    sub(p, sizeof p, d, "a..*"); EXPECT_EQ(dh_listA_nd(p, l, 16), 1);
    sub(p, sizeof p, d, "*.d"); EXPECT_EQ(dh_listA_nd(p, l, 16), 1);
    sub(p, sizeof p, d, "a.b.*"); EXPECT_EQ(dh_listA_nd(p, l, 16), 1);
    sub(p, sizeof p, d, "..*"); EXPECT_EQ(dh_listA_nd(p, l, 16), 1);
    sub(p, sizeof p, d, "*"); EXPECT_EQ(dh_listA_nd(p, l, 16), 3);
}

TEST(dfx_cwd_get_set_main_unchanged_by_children) {
    REQUIRE(U(GetCurrentDirectoryA));
    char a[MAX_PATH * 2], b[MAX_PATH * 2];
    DWORD n1 = U(GetCurrentDirectoryA)(sizeof a, a);
    char *d = dh_dir("dfx_cwdchild"); (void)d;
    DWORD n2 = U(GetCurrentDirectoryA)(sizeof b, b);
    EXPECT_EQ(n1, n2); CHECK(!strcmp(a, b));
}
