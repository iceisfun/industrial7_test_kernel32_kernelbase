/* GetFileTime / SetFileTime / CompareFileTime. */
#include "fa.h"

static HANDLE tf(const char *leaf, DWORD access) {
    fa_mkfile_s(leaf, "0123456789");
    return fa_openl(leaf, access, FA_SHARE_ALL, OPEN_EXISTING, 0);
}
static int exact_time_fs(void) { return fa_is_ntfs() && !t_is_wine(); }
#define FT(v) fa_mkft(v)

TEST(ftime_compare_table) {
    REQUIRE(U(CompareFileTime));
    static const struct { unsigned long long a, b; int r; } tb[] = {
        {0, 0, 0}, {1, 0, 1}, {0, 1, -1}, {5, 5, 0}, {FA_FT_2001, FA_FT_2001, 0},
        {FA_FT_2001, FA_FT_2001 + 1, -1}, {FA_FT_2001 + 1, FA_FT_2001, 1},
        {0x00000000FFFFFFFFULL, 0x0000000100000000ULL, -1},
        {0x0000000100000000ULL, 0x00000000FFFFFFFFULL, 1},
        {0x100000000ULL, 0x100000001ULL, -1},
        {0xFFFFFFFFFFFFFFFFULL, 0, 1}, {0, 0xFFFFFFFFFFFFFFFFULL, -1},
        {0x8000000000000000ULL, 1, 1}, {1, 0x8000000000000000ULL, -1},
        {0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL, 0},
        {0x7FFFFFFFFFFFFFFFULL, 0x8000000000000000ULL, -1},
    };
    for (unsigned i = 0; i < sizeof tb / sizeof *tb; i++) {
        FILETIME a = FT(tb[i].a), b = FT(tb[i].b);
        LONG r = U(CompareFileTime)(&a, &b);
        CHECKF(r == tb[i].r, "%llx vs %llx got %ld want %d", tb[i].a, tb[i].b, r, tb[i].r);
        r = U(CompareFileTime)(&b, &a);
        CHECKF(r == -tb[i].r, "swapped %llx vs %llx got %ld want %d", tb[i].a, tb[i].b, r, -tb[i].r);
    }
}
TEST(ftime_compare_same_pointer) {
    REQUIRE(U(CompareFileTime));
    FILETIME a = FT(0x123456789ULL);
    EXPECT_EQ(U(CompareFileTime)(&a, &a), 0);
}
TEST(ftime_compare_returns_only_minus1_0_1) {
    REQUIRE(U(CompareFileTime));
    FILETIME a = FT(1000000000000ULL), b = FT(5);
    LONG r = U(CompareFileTime)(&a, &b);
    EXPECT_EQ(r, 1);
    r = U(CompareFileTime)(&b, &a);
    EXPECT_EQ(r, -1);
}
TEST(ftime_compare_exported_by_kernel32_and_kernelbase) {
    CHECK(UK(CompareFileTime) != NULL);
    if (UB(CompareFileTime)) {
        FILETIME a = FT(2), b = FT(1);
        EXPECT_EQ(UB(CompareFileTime)(&a, &b), 1);
    }
}

TEST(ftime_get_all_null_succeeds) {
    REQUIRE(U(GetFileTime));
    HANDLE h = tf("ft_null.txt", GENERIC_READ);
    SetLastError(0);
    EXPECT_EQ(U(GetFileTime)(h, NULL, NULL, NULL), TRUE);
    CloseHandle(h); fa_rml("ft_null.txt");
}
TEST(ftime_get_each_pointer_alone) {
    REQUIRE(U(GetFileTime));
    HANDLE h = tf("ft_each.txt", GENERIC_READ);
    FILETIME c = FT(0x77), a = FT(0x77), w = FT(0x77);
    CHECK(U(GetFileTime)(h, &c, NULL, NULL)); CHECK(fa_ft64(c) > FA_FT_2001);
    CHECK(U(GetFileTime)(h, NULL, &a, NULL)); CHECK(fa_ft64(a) > FA_FT_2001);
    CHECK(U(GetFileTime)(h, NULL, NULL, &w)); CHECK(fa_ft64(w) > FA_FT_2001);
    FILETIME c2, a2, w2;
    CHECK(U(GetFileTime)(h, &c2, &a2, &w2));
    EXPECT_EQ(fa_ft64(c), fa_ft64(c2));
    EXPECT_EQ(fa_ft64(w), fa_ft64(w2));
    CloseHandle(h); fa_rml("ft_each.txt");
}
TEST(ftime_get_invalid_handles) {
    REQUIRE(U(GetFileTime));
    FILETIME c;
    SetLastError(0); EXPECT_EQ(U(GetFileTime)(INVALID_HANDLE_VALUE, &c, NULL, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SetLastError(0); EXPECT_EQ(U(GetFileTime)(NULL, &c, NULL, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SetLastError(0); EXPECT_EQ(U(GetFileTime)((HANDLE)(ULONG_PTR)0x7777770, &c, NULL, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(ftime_get_wrong_handle_type) {
    REQUIRE(U(GetFileTime));
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    FILETIME c = FT(0x1234);
    SetLastError(0); EXPECT_EQ(U(GetFileTime)(e, &c, NULL, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    EXPECT_EQ(fa_ft64(c), 0x1234);
    CloseHandle(e);
    HANDLE m = CreateMutexA(NULL, FALSE, NULL);
    SetLastError(0); EXPECT_EQ(U(GetFileTime)(m, &c, NULL, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(m);
}
TEST(ftime_get_pseudo_handles) {
    REQUIRE(U(GetFileTime));
    FILETIME c;
    SetLastError(0); EXPECT_EQ(U(GetFileTime)(GetCurrentProcess(), &c, NULL, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SetLastError(0); EXPECT_EQ(U(GetFileTime)(GetCurrentThread(), &c, NULL, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(ftime_get_after_close_fails) {
    REQUIRE(U(GetFileTime));
    HANDLE h = tf("ft_closed.txt", GENERIC_READ);
    FILETIME c;
    CHECK(U(GetFileTime)(h, &c, NULL, NULL));
    CloseHandle(h);
    SetLastError(0); EXPECT_EQ(U(GetFileTime)(h, &c, NULL, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    fa_rml("ft_closed.txt");
}
TEST(ftime_get_zero_access_handle) {
    REQUIRE(U(GetFileTime));
    fa_mkfile_s("ft_zacc.txt", "x");
    HANDLE h = fa_openl("ft_zacc.txt", 0, FA_SHARE_ALL, OPEN_EXISTING, 0);
    CHECK(h != INVALID_HANDLE_VALUE);
    FILETIME c, w;
    CHECKF(U(GetFileTime)(h, &c, NULL, &w), "err %lu", GetLastError());
    CloseHandle(h); fa_rml("ft_zacc.txt");
}
TEST(ftime_get_directory_handle) {
    REQUIRE(U(GetFileTime));
    fa_mkdir("ft_dir");
    HANDLE h = fa_opendir("ft_dir", GENERIC_READ);
    CHECK(h != INVALID_HANDLE_VALUE);
    FILETIME c, a, w;
    CHECK(U(GetFileTime)(h, &c, &a, &w));
    CHECK(fa_ft64(c) > FA_FT_2001);
    CloseHandle(h); fa_rml("ft_dir");
}
TEST(ftime_get_recent_creation_close_to_now) {
    REQUIRE(U(GetFileTime));
    FILETIME now0, now1, c;
    GetSystemTimeAsFileTime(&now0);
    HANDLE h = tf("ft_now.txt", GENERIC_READ);
    GetSystemTimeAsFileTime(&now1);
    CHECK(U(GetFileTime)(h, &c, NULL, NULL));
    unsigned long long lo = fa_ft64(now0) - 30 * FA_SEC, hi = fa_ft64(now1) + 30 * FA_SEC;
    CHECKF(fa_ft64(c) >= lo && fa_ft64(c) <= hi, "creation %llx not in [%llx,%llx]", fa_ft64(c), lo, hi);
    CloseHandle(h); fa_rml("ft_now.txt");
}
TEST(ftime_get_consistent_with_findfirst) {
    REQUIRE(U(GetFileTime));
    HANDLE h = tf("ft_ff.txt", GENERIC_READ);
    FILETIME c, a, w;
    CHECK(U(GetFileTime)(h, &c, &a, &w));
    char p[MAX_PATH * 2]; fa_path(p, "ft_ff.txt");
    WIN32_FIND_DATAA fd; HANDLE f = FindFirstFileA(p, &fd);
    CHECK(f != INVALID_HANDLE_VALUE);
    if (f != INVALID_HANDLE_VALUE) {
        EXPECT_EQ(fa_ft64(fd.ftCreationTime), fa_ft64(c));
        EXPECT_EQ(fa_ft64(fd.ftLastWriteTime), fa_ft64(w));
        FindClose(f);
    }
    CloseHandle(h); fa_rml("ft_ff.txt");
}

/* ---------------- SetFileTime ---------------- */
TEST(ftime_set_all_null_is_noop) {
    REQUIRE(U(SetFileTime)); REQUIRE(U(GetFileTime));
    HANDLE h = tf("ft_snull.txt", GENERIC_READ | GENERIC_WRITE);
    FILETIME c, a, w, c2, a2, w2;
    U(GetFileTime)(h, &c, &a, &w);
    SetLastError(0);
    EXPECT_EQ(U(SetFileTime)(h, NULL, NULL, NULL), TRUE);
    U(GetFileTime)(h, &c2, &a2, &w2);
    EXPECT_EQ(fa_ft64(c), fa_ft64(c2));
    EXPECT_EQ(fa_ft64(w), fa_ft64(w2));
    CloseHandle(h); fa_rml("ft_snull.txt");
}
TEST(ftime_set_roundtrip_all_three) {
    REQUIRE(U(SetFileTime)); REQUIRE(U(GetFileTime));
    HANDLE h = tf("ft_rt.txt", GENERIC_READ | GENERIC_WRITE);
    FILETIME c = FT(FA_FT_2001 + 12345 * FA_SEC), a = FT(FA_FT_2001 + 22345 * FA_SEC), w = FT(FA_FT_2001 + 32345 * FA_SEC);
    CHECKF(U(SetFileTime)(h, &c, &a, &w), "err %lu", GetLastError());
    FILETIME c2, a2, w2;
    CHECK(U(GetFileTime)(h, &c2, &a2, &w2));
    if (exact_time_fs()) {
        EXPECT_EQ(fa_ft64(c2), fa_ft64(c));
        EXPECT_EQ(fa_ft64(a2), fa_ft64(a));
        EXPECT_EQ(fa_ft64(w2), fa_ft64(w));
    } else {
        CHECK(llabs((long long)(fa_ft64(c2) - fa_ft64(c))) <= 2 * (long long)FA_SEC);
        CHECK(llabs((long long)(fa_ft64(w2) - fa_ft64(w))) <= 2 * (long long)FA_SEC);
    }
    CloseHandle(h); fa_rml("ft_rt.txt");
}
TEST(ftime_set_100ns_resolution_ntfs) {
    REQUIRE(U(SetFileTime)); REQUIRE(U(GetFileTime));
    if (!exact_time_fs()) { t_skip("needs real NTFS"); return; }
    HANDLE h = tf("ft_100.txt", GENERIC_READ | GENERIC_WRITE);
    static const unsigned long long add[] = { 0, 1, 2, 3, 9, 10, 99, 12345, 9999999 };
    for (unsigned i = 0; i < sizeof add / sizeof *add; i++) {
        FILETIME c = FT(FA_FT_2001 + add[i]), w = FT(FA_FT_2001 + 7 + add[i]), c2, w2;
        CHECK(U(SetFileTime)(h, &c, NULL, &w));
        CHECK(U(GetFileTime)(h, &c2, NULL, &w2));
        CHECKF(fa_ft64(c2) == fa_ft64(c) && fa_ft64(w2) == fa_ft64(w), "add %llu: c=%llx/%llx w=%llx/%llx", add[i], fa_ft64(c2), fa_ft64(c), fa_ft64(w2), fa_ft64(w));
    }
    CloseHandle(h); fa_rml("ft_100.txt");
}
TEST(ftime_set_independent_fields) {
    REQUIRE(U(SetFileTime)); REQUIRE(U(GetFileTime));
    HANDLE h = tf("ft_ind.txt", GENERIC_READ | GENERIC_WRITE);
    FILETIME c0, a0, w0;
    CHECK(U(GetFileTime)(h, &c0, &a0, &w0));
    FILETIME nw = FT(FA_FT_2001 + 100 * FA_SEC), c1, a1, w1;
    CHECK(U(SetFileTime)(h, NULL, NULL, &nw));
    CHECK(U(GetFileTime)(h, &c1, &a1, &w1));
    EXPECT_EQ(fa_ft64(c1), fa_ft64(c0));
    if (exact_time_fs()) { EXPECT_EQ(fa_ft64(w1), fa_ft64(nw)); }
    FILETIME nc = FT(FA_FT_2001 + 50 * FA_SEC), w2;
    CHECK(U(SetFileTime)(h, &nc, NULL, NULL));
    CHECK(U(GetFileTime)(h, &c1, NULL, &w2));
    if (exact_time_fs()) { EXPECT_EQ(fa_ft64(c1), fa_ft64(nc)); EXPECT_EQ(fa_ft64(w2), fa_ft64(nw)); }
    FILETIME na = FT(FA_FT_2001 + 70 * FA_SEC), a2, c2;
    CHECK(U(SetFileTime)(h, NULL, &na, NULL));
    CHECK(U(GetFileTime)(h, &c2, &a2, &w2));
    if (exact_time_fs()) { EXPECT_EQ(fa_ft64(a2), fa_ft64(na)); EXPECT_EQ(fa_ft64(c2), fa_ft64(nc)); EXPECT_EQ(fa_ft64(w2), fa_ft64(nw)); }
    CloseHandle(h); fa_rml("ft_ind.txt");
}
TEST(ftime_set_zero_value_means_unchanged) {
    REQUIRE(U(SetFileTime)); REQUIRE(U(GetFileTime));
    HANDLE h = tf("ft_zero.txt", GENERIC_READ | GENERIC_WRITE);
    FILETIME c0, w0, z = FT(0), c1, w1;
    CHECK(U(GetFileTime)(h, &c0, NULL, &w0));
    CHECKF(U(SetFileTime)(h, &z, &z, &z), "err %lu", GetLastError());
    CHECK(U(GetFileTime)(h, &c1, NULL, &w1));
    EXPECT_EQ(fa_ft64(c0), fa_ft64(c1));
    EXPECT_EQ(fa_ft64(w0), fa_ft64(w1));
    CloseHandle(h); fa_rml("ft_zero.txt");
}
TEST(ftime_set_minus1_suspends_write_time_updates) {
    REQUIRE(U(SetFileTime)); REQUIRE(U(GetFileTime));
    if (!exact_time_fs()) { t_skip("needs real NTFS"); return; }
    HANDLE h = tf("ft_m1.txt", GENERIC_READ | GENERIC_WRITE);
    FILETIME t1 = FT(FA_FT_2001 + 5 * FA_SEC), m1 = FT(0xFFFFFFFFFFFFFFFFULL), w;
    CHECK(U(SetFileTime)(h, NULL, NULL, &t1));
    CHECKF(U(SetFileTime)(h, NULL, NULL, &m1), "err %lu", GetLastError());
    Sleep(30);
    DWORD n; CHECK(WriteFile(h, "abc", 3, &n, NULL));
    CHECK(U(GetFileTime)(h, NULL, NULL, &w));
    EXPECT_EQ(fa_ft64(w), fa_ft64(t1));
    /* a second handle is not affected by the first handle's suppression */
    HANDLE h2 = fa_openfile("ft_m1.txt");
    Sleep(30);
    CHECK(WriteFile(h2, "d", 1, &n, NULL));
    CloseHandle(h2);
    CHECK(U(GetFileTime)(h, NULL, NULL, &w));
    t_note("write time after write from other handle = %llx (t1=%llx)", fa_ft64(w), fa_ft64(t1));
    CloseHandle(h); fa_rml("ft_m1.txt");
}
TEST(ftime_set_without_minus1_write_updates_time) {
    REQUIRE(U(SetFileTime)); REQUIRE(U(GetFileTime));
    if (!exact_time_fs()) { t_skip("needs real NTFS"); return; }
    HANDLE h = tf("ft_upd.txt", GENERIC_READ | GENERIC_WRITE);
    FILETIME t1 = FT(FA_FT_2001 + 5 * FA_SEC), w;
    CHECK(U(SetFileTime)(h, NULL, NULL, &t1));
    DWORD n; CHECK(WriteFile(h, "abc", 3, &n, NULL));
    CHECK(CloseHandle(h));
    h = fa_openl("ft_upd.txt", GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0);
    CHECK(U(GetFileTime)(h, NULL, NULL, &w));
    CHECKF(fa_ft64(w) > fa_ft64(t1) + 1000 * FA_SEC, "write time %llx not updated", fa_ft64(w));
    CloseHandle(h); fa_rml("ft_upd.txt");
}
TEST(ftime_set_explicit_time_survives_close_and_write) {
    REQUIRE(U(SetFileTime)); REQUIRE(U(GetFileTime));
    if (!exact_time_fs()) { t_skip("needs real NTFS"); return; }
    /* setting a write time on a handle stops further automatic write-time updates through that handle */
    HANDLE h = tf("ft_surv.txt", GENERIC_READ | GENERIC_WRITE);
    DWORD n; WriteFile(h, "xyz", 3, &n, NULL);
    FILETIME t1 = FT(FA_FT_2001 + 9 * FA_SEC), w;
    CHECK(U(SetFileTime)(h, NULL, NULL, &t1));
    WriteFile(h, "more", 4, &n, NULL);
    CloseHandle(h);
    h = fa_openl("ft_surv.txt", GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0);
    U(GetFileTime)(h, NULL, NULL, &w);
    t_note("write time after set+write+close = %llx (set %llx)", fa_ft64(w), fa_ft64(t1));
    CloseHandle(h); fa_rml("ft_surv.txt");
}
TEST(ftime_set_requires_write_attributes) {
    REQUIRE(U(SetFileTime));
    HANDLE h = tf("ft_acc.txt", GENERIC_READ);
    FILETIME t = FT(FA_FT_2001);
    SetLastError(0); EXPECT_EQ(U(SetFileTime)(h, &t, NULL, NULL), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(h);
    h = fa_openl("ft_acc.txt", 0, FA_SHARE_ALL, OPEN_EXISTING, 0);
    SetLastError(0); EXPECT_EQ(U(SetFileTime)(h, &t, NULL, NULL), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(h);
    h = fa_openl("ft_acc.txt", FILE_WRITE_ATTRIBUTES, FA_SHARE_ALL, OPEN_EXISTING, 0);
    CHECK(h != INVALID_HANDLE_VALUE);
    SetLastError(0); EXPECT_EQ(U(SetFileTime)(h, &t, NULL, NULL), TRUE);
    CloseHandle(h);
    h = fa_openl("ft_acc.txt", FILE_WRITE_DATA, FA_SHARE_ALL, OPEN_EXISTING, 0);
    SetLastError(0); EXPECT_EQ(U(SetFileTime)(h, &t, NULL, NULL), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(h);
    fa_rml("ft_acc.txt");
}
TEST(ftime_set_invalid_handles) {
    REQUIRE(U(SetFileTime));
    FILETIME t = FT(FA_FT_2001);
    SetLastError(0); EXPECT_EQ(U(SetFileTime)(INVALID_HANDLE_VALUE, &t, &t, &t), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SetLastError(0); EXPECT_EQ(U(SetFileTime)(NULL, &t, &t, &t), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE e = CreateEventA(NULL, FALSE, FALSE, NULL);
    SetLastError(0); EXPECT_EQ(U(SetFileTime)(e, &t, &t, &t), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(e);
}
TEST(ftime_set_out_of_range_values) {
    REQUIRE(U(SetFileTime));
    HANDLE h = tf("ft_range.txt", GENERIC_READ | GENERIC_WRITE);
    static const unsigned long long v[] = { 0x8000000000000000ULL, 0x8000000000000001ULL, 0xFFFFFFFFFFFFFFFEULL, 0x7FFFFFFFFFFFFFFFULL };
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++) {
        FILETIME t = FT(v[i]);
        SetLastError(0);
        BOOL r = U(SetFileTime)(h, NULL, NULL, &t);
        t_note("SetFileTime(write=%llx) -> %d err=%lu", v[i], r, r ? 0 : GetLastError());
        if (!r) CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "err %lu", GetLastError());
    }
    /* high-bit-set values other than -1 are negative LARGE_INTEGERs and must be rejected by NTFS */
    FILETIME neg = FT(0x8000000000000000ULL);
    if (exact_time_fs()) { SetLastError(0); EXPECT_EQ(U(SetFileTime)(h, &neg, NULL, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
    CloseHandle(h); fa_rml("ft_range.txt");
}
TEST(ftime_set_on_readonly_file) {
    REQUIRE(U(SetFileTime)); REQUIRE(U(GetFileTime));
    fa_mkfile_s("ft_ro.txt", "x");
    char p[MAX_PATH * 2]; fa_path(p, "ft_ro.txt"); SetFileAttributesA(p, FILE_ATTRIBUTE_READONLY);
    HANDLE h = fa_openl("ft_ro.txt", FILE_WRITE_ATTRIBUTES | FILE_READ_ATTRIBUTES, FA_SHARE_ALL, OPEN_EXISTING, 0);
    CHECK(h != INVALID_HANDLE_VALUE);
    FILETIME t = FT(FA_FT_2001 + 77 * FA_SEC), w;
    CHECK(U(SetFileTime)(h, NULL, NULL, &t));
    CHECK(U(GetFileTime)(h, NULL, NULL, &w));
    if (exact_time_fs()) EXPECT_EQ(fa_ft64(w), fa_ft64(t));
    CloseHandle(h);
    fa_rm(p);
}
TEST(ftime_set_on_directory) {
    REQUIRE(U(SetFileTime)); REQUIRE(U(GetFileTime));
    fa_mkdir("ft_sdir");
    HANDLE h = fa_opendir("ft_sdir", FILE_WRITE_ATTRIBUTES | FILE_READ_ATTRIBUTES);
    CHECK(h != INVALID_HANDLE_VALUE);
    FILETIME t = FT(FA_FT_2001 + 1000 * FA_SEC), c, w;
    CHECKF(U(SetFileTime)(h, &t, NULL, &t), "err %lu", GetLastError());
    CHECK(U(GetFileTime)(h, &c, NULL, &w));
    if (exact_time_fs()) { EXPECT_EQ(fa_ft64(c), fa_ft64(t)); EXPECT_EQ(fa_ft64(w), fa_ft64(t)); }
    CloseHandle(h); fa_rml("ft_sdir");
}
TEST(ftime_set_visible_through_other_apis) {
    REQUIRE(U(SetFileTime));
    HANDLE h = tf("ft_vis.txt", GENERIC_READ | GENERIC_WRITE);
    FILETIME t = FT(FA_FT_2001 + 424242 * FA_SEC);
    CHECK(U(SetFileTime)(h, &t, &t, &t));
    CloseHandle(h);
    char p[MAX_PATH * 2]; fa_path(p, "ft_vis.txt");
    WIN32_FILE_ATTRIBUTE_DATA d; CHECK(GetFileAttributesExA(p, GetFileExInfoStandard, &d));
    WIN32_FIND_DATAA fd; HANDLE f = FindFirstFileA(p, &fd);
    if (exact_time_fs()) {
        EXPECT_EQ(fa_ft64(d.ftCreationTime), fa_ft64(t));
        EXPECT_EQ(fa_ft64(d.ftLastWriteTime), fa_ft64(t));
        if (f != INVALID_HANDLE_VALUE) EXPECT_EQ(fa_ft64(fd.ftLastWriteTime), fa_ft64(t));
    }
    if (f != INVALID_HANDLE_VALUE) FindClose(f);
    fa_rm(p);
}
TEST(ftime_set_does_not_change_size_or_attrs) {
    REQUIRE(U(SetFileTime));
    HANDLE h = tf("ft_nochg.txt", GENERIC_READ | GENERIC_WRITE);
    char p[MAX_PATH * 2]; fa_path(p, "ft_nochg.txt");
    DWORD a0 = GetFileAttributesA(p);
    FILETIME t = FT(FA_FT_2001 + 1);
    CHECK(U(SetFileTime)(h, &t, &t, &t));
    EXPECT_EQ(GetFileSize(h, NULL), 10);
    CloseHandle(h);
    EXPECT_EQ(GetFileAttributesA(p), a0);
    fa_rm(p);
}
TEST(ftime_set_future_dates) {
    REQUIRE(U(SetFileTime)); REQUIRE(U(GetFileTime));
    if (!exact_time_fs()) { t_skip("needs real NTFS"); return; }
    HANDLE h = tf("ft_fut.txt", GENERIC_READ | GENERIC_WRITE);
    FILETIME t = FT(FA_FT_2001 + 100ULL * 365 * 86400 * FA_SEC), r;   /* ~2101 */
    CHECK(U(SetFileTime)(h, &t, &t, &t));
    CHECK(U(GetFileTime)(h, &r, NULL, NULL));
    EXPECT_EQ(fa_ft64(r), fa_ft64(t));
    CloseHandle(h); fa_rml("ft_fut.txt");
}
TEST(ftime_set_early_dates) {
    REQUIRE(U(SetFileTime)); REQUIRE(U(GetFileTime));
    if (!exact_time_fs()) { t_skip("needs real NTFS"); return; }
    HANDLE h = tf("ft_early.txt", GENERIC_READ | GENERIC_WRITE);
    FILETIME t = FT(1), r;     /* 1601-01-01 00:00:00.0000001 */
    CHECKF(U(SetFileTime)(h, &t, &t, &t), "err %lu", GetLastError());
    CHECK(U(GetFileTime)(h, &r, NULL, NULL));
    EXPECT_EQ(fa_ft64(r), 1);
    CloseHandle(h); fa_rml("ft_early.txt");
}
TEST(ftime_write_updates_write_time) {
    REQUIRE(U(GetFileTime)); REQUIRE(U(SetFileTime));
    if (!exact_time_fs()) { t_skip("needs real NTFS"); return; }
    HANDLE h = tf("ft_wr.txt", GENERIC_READ | GENERIC_WRITE);
    FILETIME old = FT(FA_FT_2001 + 3 * FA_SEC), w;
    CHECK(U(SetFileTime)(h, &old, &old, &old));
    CloseHandle(h);
    h = fa_openfile("ft_wr.txt");
    DWORD n; CHECK(WriteFile(h, "a", 1, &n, NULL));
    FlushFileBuffers(h);
    CloseHandle(h);
    h = fa_openl("ft_wr.txt", GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0);
    CHECK(U(GetFileTime)(h, NULL, NULL, &w));
    CHECKF(fa_ft64(w) > fa_ft64(old) + 1000 * FA_SEC, "w=%llx", fa_ft64(w));
    FILETIME c; U(GetFileTime)(h, &c, NULL, NULL);
    EXPECT_EQ(fa_ft64(c), fa_ft64(old));
    CloseHandle(h); fa_rml("ft_wr.txt");
}
TEST(ftime_read_does_not_change_write_time) {
    REQUIRE(U(GetFileTime));
    HANDLE h = tf("ft_rd.txt", GENERIC_READ);
    FILETIME w0, w1; char b[16]; DWORD n;
    U(GetFileTime)(h, NULL, NULL, &w0);
    Sleep(20);
    ReadFile(h, b, sizeof b, &n, NULL);
    U(GetFileTime)(h, NULL, NULL, &w1);
    EXPECT_EQ(fa_ft64(w0), fa_ft64(w1));
    CloseHandle(h); fa_rml("ft_rd.txt");
}
TEST(ftime_setendoffile_updates_write_time) {
    REQUIRE(U(GetFileTime)); REQUIRE(U(SetFileTime));
    if (!exact_time_fs()) { t_skip("needs real NTFS"); return; }
    HANDLE h = tf("ft_eof.txt", GENERIC_READ | GENERIC_WRITE);
    FILETIME old = FT(FA_FT_2001 + 3 * FA_SEC), w;
    CHECK(U(SetFileTime)(h, NULL, NULL, &old));
    CloseHandle(h);
    h = fa_openfile("ft_eof.txt");
    SetFilePointer(h, 4, NULL, FILE_BEGIN); SetEndOfFile(h);
    CloseHandle(h);
    h = fa_openl("ft_eof.txt", GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0);
    U(GetFileTime)(h, NULL, NULL, &w);
    CHECKF(fa_ft64(w) > fa_ft64(old) + 1000 * FA_SEC, "w=%llx", fa_ft64(w));
    CloseHandle(h); fa_rml("ft_eof.txt");
}
TEST(ftime_setfileattributes_keeps_creation_and_write) {
    REQUIRE(U(GetFileTime)); REQUIRE(U(SetFileTime));
    HANDLE h = tf("ft_attr.txt", GENERIC_READ | GENERIC_WRITE);
    FILETIME t = FT(FA_FT_2001 + 11 * FA_SEC), c, w;
    U(SetFileTime)(h, &t, NULL, &t);
    char p[MAX_PATH * 2]; fa_path(p, "ft_attr.txt");
    SetFileAttributesA(p, FILE_ATTRIBUTE_HIDDEN);
    U(GetFileTime)(h, &c, NULL, &w);
    if (exact_time_fs()) { EXPECT_EQ(fa_ft64(c), fa_ft64(t)); EXPECT_EQ(fa_ft64(w), fa_ft64(t)); }
    CloseHandle(h);
    SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL); fa_rm(p);
}
TEST(ftime_rename_keeps_times) {
    REQUIRE(U(GetFileTime)); REQUIRE(U(SetFileTime));
    HANDLE h = tf("ft_ren1.txt", GENERIC_READ | GENERIC_WRITE);
    FILETIME t = FT(FA_FT_2001 + 13 * FA_SEC), c, w;
    U(SetFileTime)(h, &t, NULL, &t);
    CloseHandle(h);
    char p[MAX_PATH * 2], q[MAX_PATH * 2]; fa_path(p, "ft_ren1.txt"); fa_path(q, "ft_ren2.txt");
    CHECK(MoveFileA(p, q));
    h = fa_openl("ft_ren2.txt", GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0);
    U(GetFileTime)(h, &c, NULL, &w);
    if (exact_time_fs()) { EXPECT_EQ(fa_ft64(c), fa_ft64(t)); EXPECT_EQ(fa_ft64(w), fa_ft64(t)); }
    CloseHandle(h); fa_rm(q);
}
TEST(ftime_cross_handle_visibility) {
    REQUIRE(U(GetFileTime)); REQUIRE(U(SetFileTime));
    HANDLE a = tf("ft_xh.txt", GENERIC_READ | GENERIC_WRITE);
    HANDLE b = fa_openl("ft_xh.txt", GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0);
    FILETIME t = FT(FA_FT_2001 + 21 * FA_SEC), w;
    CHECK(U(SetFileTime)(a, NULL, NULL, &t));
    CHECK(U(GetFileTime)(b, NULL, NULL, &w));
    if (exact_time_fs()) EXPECT_EQ(fa_ft64(w), fa_ft64(t));
    CloseHandle(a); CloseHandle(b); fa_rml("ft_xh.txt");
}
TEST(ftime_set_on_pipe_and_console_like_handles) {
    REQUIRE(U(GetFileTime)); REQUIRE(U(SetFileTime));
    HANDLE r, w;
    if (!CreatePipe(&r, &w, NULL, 0)) { t_skip("no pipe"); return; }
    FILETIME t = FT(FA_FT_2001), o;
    SetLastError(0); BOOL g = U(GetFileTime)(r, &o, NULL, NULL);
    t_note("GetFileTime(pipe) = %d err=%lu", g, g ? 0 : GetLastError());
    SetLastError(0); BOOL s = U(SetFileTime)(w, &t, NULL, NULL);
    t_note("SetFileTime(pipe) = %d err=%lu", s, s ? 0 : GetLastError());
    CHECK(!s);
    CloseHandle(r); CloseHandle(w);
}
TEST(ftime_stress_set_get_loop) {
    REQUIRE(U(GetFileTime)); REQUIRE(U(SetFileTime));
    HANDLE h = tf("ft_loop.txt", GENERIC_READ | GENERIC_WRITE);
    int bad = 0;
    for (int i = 0; i < 500; i++) {
        FILETIME t = FT(FA_FT_2001 + (unsigned long long)i * 12345 + 17), r;
        if (!U(SetFileTime)(h, NULL, NULL, &t) || !U(GetFileTime)(h, NULL, NULL, &r)) { bad++; continue; }
        if (exact_time_fs() && fa_ft64(r) != fa_ft64(t)) bad++;
    }
    EXPECT_EQ(bad, 0);
    CloseHandle(h); fa_rml("ft_loop.txt");
}
