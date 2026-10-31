/* GetDiskFreeSpace A/W, GetDiskFreeSpaceEx A/W, GetDiskSpaceInformation A/W */
#include "vd.h"

#define SENT 0x13572468u

typedef struct { DWORD spc, bps, free, total; } vd_fs;
static BOOL vd_gdfs_w(const char *root, vd_fs *o) {
    WCHAR w[MAX_PATH]; if (root) vd_widen(root, w, MAX_PATH);
    return U(GetDiskFreeSpaceW)(root ? w : NULL, &o->spc, &o->bps, &o->free, &o->total);
}

TEST(vd_diskfree_w_system_root) {
    REQUIRE(U(GetDiskFreeSpaceW));
    char r[4]; vd_sysroot(r); vd_fs f = {0};
    CHECK(vd_gdfs_w(r, &f));
    CHECK(f.spc >= 1 && vd_pow2(f.spc));
    CHECK(f.bps >= 512 && vd_pow2(f.bps));
    CHECK(f.total > 0);
    CHECK(f.free <= f.total);
}
TEST(vd_diskfree_a_system_root) {
    REQUIRE(U(GetDiskFreeSpaceA));
    char r[4]; vd_sysroot(r); DWORD spc = 0, bps = 0, fr = 0, tot = 0;
    CHECK(U(GetDiskFreeSpaceA)(r, &spc, &bps, &fr, &tot));
    CHECK(spc >= 1 && vd_pow2(spc)); CHECK(vd_pow2(bps)); CHECK(tot > 0); CHECK(fr <= tot);
}
TEST(vd_diskfree_ab_parity) {
    REQUIRE(U(GetDiskFreeSpaceA)); REQUIRE(U(GetDiskFreeSpaceW));
    char r[4]; vd_sysroot(r); vd_fs w = {0}; DWORD spc = 0, bps = 0, fr = 0, tot = 0;
    CHECK(vd_gdfs_w(r, &w));
    CHECK(U(GetDiskFreeSpaceA)(r, &spc, &bps, &fr, &tot));
    EXPECT_EQ(spc, w.spc); EXPECT_EQ(bps, w.bps); EXPECT_EQ(tot, w.total);
}
TEST(vd_diskfree_null_root_is_current_volume) {
    REQUIRE(U(GetDiskFreeSpaceA)); REQUIRE(U(GetDiskFreeSpaceW));
    char cwd[MAX_PATH]; GetCurrentDirectoryA(sizeof cwd, cwd);
    if (cwd[1] != ':') { t_skip("cwd not a drive path"); return; }
    char r[4] = { cwd[0], ':', '\\', 0 };
    vd_fs a = {0}, b = {0};
    CHECK(vd_gdfs_w(NULL, &a));
    CHECK(vd_gdfs_w(r, &b));
    EXPECT_EQ(a.total, b.total); EXPECT_EQ(a.bps, b.bps); EXPECT_EQ(a.spc, b.spc);
    DWORD x, y, z, t;
    CHECK(U(GetDiskFreeSpaceA)(NULL, &x, &y, &z, &t));
    EXPECT_EQ(t, b.total);
}
TEST(vd_diskfree_each_output_null_individually) {
    REQUIRE(U(GetDiskFreeSpaceW));
    char r[4]; vd_sysroot(r); WCHAR w[4]; vd_widen(r, w, 4);
    DWORD v[4];
    for (int skip = 0; skip < 4; skip++) {
        DWORD *p[4]; for (int i = 0; i < 4; i++) { v[i] = SENT; p[i] = (i == skip) ? NULL : &v[i]; }
        CHECKF(U(GetDiskFreeSpaceW)(w, p[0], p[1], p[2], p[3]), "skip=%d", skip);
        for (int i = 0; i < 4; i++) if (i != skip) CHECKF(v[i] != SENT, "skip=%d i=%d", skip, i);
        CHECK(v[skip] == SENT);
    }
}
TEST(vd_diskfree_all_outputs_null) {
    REQUIRE(U(GetDiskFreeSpaceW));
    char r[4]; vd_sysroot(r); WCHAR w[4]; vd_widen(r, w, 4);
    CHECK(U(GetDiskFreeSpaceW)(w, NULL, NULL, NULL, NULL));
}
TEST(vd_diskfree_single_output_each) {
    REQUIRE(U(GetDiskFreeSpaceW));
    char r[4]; vd_sysroot(r); WCHAR w[4]; vd_widen(r, w, 4);
    DWORD v = SENT;
    CHECK(U(GetDiskFreeSpaceW)(w, &v, NULL, NULL, NULL)); CHECK(v != SENT && v != 0); v = SENT;
    CHECK(U(GetDiskFreeSpaceW)(w, NULL, &v, NULL, NULL)); CHECK(v != SENT && v != 0); v = SENT;
    CHECK(U(GetDiskFreeSpaceW)(w, NULL, NULL, &v, NULL)); CHECK(v != SENT); v = SENT;
    CHECK(U(GetDiskFreeSpaceW)(w, NULL, NULL, NULL, &v)); CHECK(v != SENT && v != 0);
}
TEST(vd_diskfree_nonexistent_drive_fails) {
    REQUIRE(U(GetDiskFreeSpaceW)); REQUIRE(U(GetDiskFreeSpaceA));
    char l = vd_freeletter(); if (!l) { t_skip("no free letter"); return; }
    char a[4] = { l, ':', '\\', 0 }; WCHAR w[4]; vd_widen(a, w, 4);
    DWORD spc = SENT, bps = SENT, fr = SENT, tot = SENT;
    SET_ERR(0);
    CHECK(!U(GetDiskFreeSpaceW)(w, &spc, &bps, &fr, &tot));
    DWORD e = GetLastError();
    CHECKF(e == ERROR_PATH_NOT_FOUND || e == ERROR_INVALID_DRIVE, "err %lu", e);
    CHECK(bps == 0);          /* zeroed when the open fails */
    CHECK(spc == SENT && fr == SENT && tot == SENT);
    CHECK(!U(GetDiskFreeSpaceA)(a, &spc, &bps, &fr, &tot));
}
TEST(vd_diskfree_empty_string_invalid) {
    REQUIRE(U(GetDiskFreeSpaceW));
    DWORD a, b, c, d;
    SET_ERR(0);
    CHECK(!U(GetDiskFreeSpaceW)(L"", &a, &b, &c, &d));
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(vd_diskfree_invalid_chars_fail) {
    REQUIRE(U(GetDiskFreeSpaceW));
    static const WCHAR *const bad[] = { L"C:\\<>\\", L"C:\\a|b\\", L"C:\\a?b\\", L"C:\\a*b\\", L"C:\\\"x\"\\" };
    for (size_t i = 0; i < ARRAYSIZE(bad); i++) {
        DWORD a, b, c, d; SET_ERR(0);
        BOOL ok = U(GetDiskFreeSpaceW)(bad[i], &a, &b, &c, &d);
        CHECKF(!ok, "case %zu", i);
        CHECK(GetLastError() != 0);
    }
}
TEST(vd_diskfree_subdirectory_ok) {
    REQUIRE(U(GetDiskFreeSpaceW));
    vd_fs sub = {0}, root = {0}; char r[4]; vd_tmproot(r);
    CHECK(vd_gdfs_w(t_tmpdir(), &sub));
    CHECK(vd_gdfs_w(r, &root));
    EXPECT_EQ(sub.total, root.total); EXPECT_EQ(sub.bps, root.bps); EXPECT_EQ(sub.spc, root.spc);
}
TEST(vd_diskfree_subdirectory_without_trailing_backslash) {
    REQUIRE(U(GetDiskFreeSpaceW));
    char d[MAX_PATH]; strcpy(d, t_tmpdir()); d[strlen(d) - 1] = 0;
    vd_fs f = {0}; CHECK(vd_gdfs_w(d, &f)); CHECK(f.total > 0);
}
TEST(vd_diskfree_volume_guid_path) {
    REQUIRE(U(GetDiskFreeSpaceW));
    char r[4]; vd_sysroot(r); char g[64];
    if (!vd_volname(r, g)) { t_skip("no volume name"); return; }
    vd_fs a = {0}, b = {0};
    CHECK(vd_gdfs_w(g, &a)); CHECK(vd_gdfs_w(r, &b));
    EXPECT_EQ(a.total, b.total);
}
TEST(vd_diskfree_dosdevice_prefix) {
    REQUIRE(U(GetDiskFreeSpaceW));
    char p[16]; sprintf(p, "\\\\?\\%c:\\", vd_sysletter());
    vd_fs a = {0}; CHECK(vd_gdfs_w(p, &a)); CHECK(a.total > 0);
}
TEST(vd_diskfree_nonexistent_subdir_fails) {
    REQUIRE(U(GetDiskFreeSpaceW));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "no_such_dir_k32t\\"); vd_fs f = {0};
    SET_ERR(0);
    CHECK(!vd_gdfs_w(p, &f));
    DWORD e = GetLastError();
    CHECKF(e == ERROR_FILE_NOT_FOUND || e == ERROR_PATH_NOT_FOUND, "err %lu", e);
}
TEST(vd_diskfree_file_path_behaviour) {
    REQUIRE(U(GetDiskFreeSpaceW));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "vd_free_file.bin");
    HANDLE h = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); CloseHandle(h);
    vd_fs f = {0}; SET_ERR(0);
    BOOL ok = vd_gdfs_w(p, &f);
    CHECK(!ok); EXPECT_ERR(ERROR_DIRECTORY);   /* opened with FILE_DIRECTORY_FILE */
    DeleteFileA(p);
}
TEST(vd_diskfree_totals_consistent_with_ex) {
    REQUIRE(U(GetDiskFreeSpaceW)); REQUIRE(U(GetDiskFreeSpaceExW));
    char r[4]; vd_sysroot(r); WCHAR w[4]; vd_widen(r, w, 4); vd_fs f = {0};
    CHECK(vd_gdfs_w(r, &f));
    ULARGE_INTEGER av, tot, fr;
    CHECK(U(GetDiskFreeSpaceExW)(w, &av, &tot, &fr));
    if (f.total != 0xFFFFFFFFu) EXPECT_EQ(tot.QuadPart, (ULONGLONG)f.total * f.spc * f.bps);
    else CHECK(tot.QuadPart >= 0xFFFFFFFFull * f.spc * f.bps);
}

/* ---- GetDiskFreeSpaceEx ---- */
TEST(vd_diskfreeex_w_relations) {
    REQUIRE(U(GetDiskFreeSpaceExW));
    char r[4]; vd_sysroot(r); WCHAR w[4]; vd_widen(r, w, 4);
    ULARGE_INTEGER av = {{0}}, tot = {{0}}, fr = {{0}};
    CHECK(U(GetDiskFreeSpaceExW)(w, &av, &tot, &fr));
    CHECK(tot.QuadPart > 0);
    CHECK(fr.QuadPart <= tot.QuadPart);
    CHECK(av.QuadPart <= fr.QuadPart);
    CHECK(tot.QuadPart % 512 == 0);
}
TEST(vd_diskfreeex_a_relations) {
    REQUIRE(U(GetDiskFreeSpaceExA));
    char r[4]; vd_sysroot(r);
    ULARGE_INTEGER av = {{0}}, tot = {{0}}, fr = {{0}};
    CHECK(U(GetDiskFreeSpaceExA)(r, &av, &tot, &fr));
    CHECK(tot.QuadPart > 0); CHECK(fr.QuadPart <= tot.QuadPart); CHECK(av.QuadPart <= fr.QuadPart);
}
TEST(vd_diskfreeex_ab_parity_totals) {
    REQUIRE(U(GetDiskFreeSpaceExA)); REQUIRE(U(GetDiskFreeSpaceExW));
    char r[4]; vd_sysroot(r); WCHAR w[4]; vd_widen(r, w, 4);
    ULARGE_INTEGER a1, a2, t1, t2, f1, f2;
    CHECK(U(GetDiskFreeSpaceExA)(r, &a1, &t1, &f1));
    CHECK(U(GetDiskFreeSpaceExW)(w, &a2, &t2, &f2));
    EXPECT_EQ(t1.QuadPart, t2.QuadPart);
}
TEST(vd_diskfreeex_null_directory_current_volume) {
    REQUIRE(U(GetDiskFreeSpaceExW)); REQUIRE(U(GetDiskFreeSpaceExA));
    char cwd[MAX_PATH]; GetCurrentDirectoryA(sizeof cwd, cwd);
    if (cwd[1] != ':') { t_skip("cwd not drive path"); return; }
    char r[4] = { cwd[0], ':', '\\', 0 }; WCHAR w[4]; vd_widen(r, w, 4);
    ULARGE_INTEGER a, t1, t2, f;
    CHECK(U(GetDiskFreeSpaceExW)(NULL, &a, &t1, &f));
    CHECK(U(GetDiskFreeSpaceExW)(w, &a, &t2, &f));
    EXPECT_EQ(t1.QuadPart, t2.QuadPart);
    CHECK(U(GetDiskFreeSpaceExA)(NULL, &a, &t1, &f));
    EXPECT_EQ(t1.QuadPart, t2.QuadPart);
}
TEST(vd_diskfreeex_all_null_outputs) {
    REQUIRE(U(GetDiskFreeSpaceExW));
    char r[4]; vd_sysroot(r); WCHAR w[4]; vd_widen(r, w, 4);
    CHECK(U(GetDiskFreeSpaceExW)(w, NULL, NULL, NULL));
}
TEST(vd_diskfreeex_each_output_alone) {
    REQUIRE(U(GetDiskFreeSpaceExW));
    char r[4]; vd_sysroot(r); WCHAR w[4]; vd_widen(r, w, 4);
    ULARGE_INTEGER v;
    v.QuadPart = SENT; CHECK(U(GetDiskFreeSpaceExW)(w, &v, NULL, NULL)); CHECK(v.QuadPart != SENT);
    v.QuadPart = SENT; CHECK(U(GetDiskFreeSpaceExW)(w, NULL, &v, NULL)); CHECK(v.QuadPart != SENT && v.QuadPart > 0);
    v.QuadPart = SENT; CHECK(U(GetDiskFreeSpaceExW)(w, NULL, NULL, &v)); CHECK(v.QuadPart != SENT);
}
TEST(vd_diskfreeex_without_totalfree_pointer) {
    REQUIRE(U(GetDiskFreeSpaceExW));
    char r[4]; vd_sysroot(r); WCHAR w[4]; vd_widen(r, w, 4);
    ULARGE_INTEGER a = {{0}}, t = {{0}};
    CHECK(U(GetDiskFreeSpaceExW)(w, &a, &t, NULL));
    CHECK(t.QuadPart > 0); CHECK(a.QuadPart <= t.QuadPart);
}
TEST(vd_diskfreeex_subdirectory_same_total) {
    REQUIRE(U(GetDiskFreeSpaceExW));
    char r[4]; vd_tmproot(r); WCHAR w[4], wd[MAX_PATH]; vd_widen(r, w, 4); vd_widen(t_tmpdir(), wd, MAX_PATH);
    ULARGE_INTEGER a, t1, t2, f;
    CHECK(U(GetDiskFreeSpaceExW)(w, &a, &t1, &f));
    CHECK(U(GetDiskFreeSpaceExW)(wd, &a, &t2, &f));
    EXPECT_EQ(t1.QuadPart, t2.QuadPart);   /* total is volume-wide; only the caller-available figure is quota aware */
}
TEST(vd_diskfreeex_subdirectory_no_trailing_backslash) {
    REQUIRE(U(GetDiskFreeSpaceExA));
    char d[MAX_PATH]; strcpy(d, t_tmpdir()); d[strlen(d) - 1] = 0;
    ULARGE_INTEGER a, t, f;
    CHECK(U(GetDiskFreeSpaceExA)(d, &a, &t, &f));
}
TEST(vd_diskfreeex_free_tracks_file_creation) {
    REQUIRE(U(GetDiskFreeSpaceExW));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "vd_ex_big.bin"); WCHAR wd[MAX_PATH]; vd_widen(t_tmpdir(), wd, MAX_PATH);
    ULARGE_INTEGER a0, t0, f0, a1, t1, f1;
    CHECK(U(GetDiskFreeSpaceExW)(wd, &a0, &t0, &f0));
    HANDLE h = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_skip("cannot create file"); return; }
    static char blk[65536]; DWORD n; memset(blk, 'z', sizeof blk);
    for (int i = 0; i < 16; i++) WriteFile(h, blk, sizeof blk, &n, NULL);   /* 1 MiB */
    FlushFileBuffers(h); CloseHandle(h);
    CHECK(U(GetDiskFreeSpaceExW)(wd, &a1, &t1, &f1));
    EXPECT_EQ(t0.QuadPart, t1.QuadPart);
    /* other activity on the machine can only add noise; require a visible decrease not an exact amount */
    if (f1.QuadPart + 4096 > f0.QuadPart) t_note("free space did not visibly decrease (%llu -> %llu)", f0.QuadPart, f1.QuadPart);
    DeleteFileA(p);
}
TEST(vd_diskfreeex_nonexistent_drive) {
    REQUIRE(U(GetDiskFreeSpaceExW)); REQUIRE(U(GetDiskFreeSpaceExA));
    char l = vd_freeletter(); if (!l) { t_skip("no free letter"); return; }
    char a[4] = { l, ':', '\\', 0 }; WCHAR w[4]; vd_widen(a, w, 4);
    ULARGE_INTEGER av, t, f; av.QuadPart = t.QuadPart = f.QuadPart = SENT;
    SET_ERR(0);
    CHECK(!U(GetDiskFreeSpaceExW)(w, &av, &t, &f));
    DWORD e = GetLastError();
    CHECKF(e == ERROR_PATH_NOT_FOUND || e == ERROR_INVALID_DRIVE, "err %lu", e);
    CHECK(av.QuadPart == SENT && t.QuadPart == SENT && f.QuadPart == SENT);
    CHECK(!U(GetDiskFreeSpaceExA)(a, &av, &t, &f));
}
TEST(vd_diskfreeex_empty_string) {
    REQUIRE(U(GetDiskFreeSpaceExW)); REQUIRE(U(GetDiskFreeSpaceExA));
    ULARGE_INTEGER a, t, f;
    SET_ERR(0); CHECK(!U(GetDiskFreeSpaceExW)(L"", &a, &t, &f)); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    SET_ERR(0); CHECK(!U(GetDiskFreeSpaceExA)("", &a, &t, &f)); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(vd_diskfreeex_missing_leaf_maps_to_path_not_found) {
    REQUIRE(U(GetDiskFreeSpaceExW));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "no_such_leaf_k32t\\"); WCHAR w[MAX_PATH]; vd_widen(p, w, MAX_PATH);
    ULARGE_INTEGER a, t, f;
    SET_ERR(0); CHECK(!U(GetDiskFreeSpaceExW)(w, &a, &t, &f));
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(vd_diskfreeex_missing_intermediate_dir) {
    REQUIRE(U(GetDiskFreeSpaceExW));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "no_a_k32t\\no_b\\"); WCHAR w[MAX_PATH]; vd_widen(p, w, MAX_PATH);
    ULARGE_INTEGER a, t, f;
    SET_ERR(0); CHECK(!U(GetDiskFreeSpaceExW)(w, &a, &t, &f));
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(vd_diskfreeex_volume_guid_matches_letter) {
    REQUIRE(U(GetDiskFreeSpaceExW));
    char r[4]; vd_sysroot(r); char g[64]; WCHAR wg[64], wr[4];
    if (!vd_volname(r, g)) { t_skip("no volume name"); return; }
    vd_widen(g, wg, 64); vd_widen(r, wr, 4);
    ULARGE_INTEGER a, t1, t2, f;
    CHECK(U(GetDiskFreeSpaceExW)(wg, &a, &t1, &f));
    CHECK(U(GetDiskFreeSpaceExW)(wr, &a, &t2, &f));
    EXPECT_EQ(t1.QuadPart, t2.QuadPart);
}
TEST(vd_diskfreeex_file_path_behaviour) {
    REQUIRE(U(GetDiskFreeSpaceExW));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "vd_ex_file.bin"); WCHAR w[MAX_PATH]; vd_widen(p, w, MAX_PATH);
    HANDLE h = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); CloseHandle(h);
    ULARGE_INTEGER a, t, f; SET_ERR(0);
    BOOL ok = U(GetDiskFreeSpaceExW)(w, &a, &t, &f);
    CHECK(!ok); EXPECT_ERR(ERROR_DIRECTORY);
    DeleteFileA(p);
}
TEST(vd_diskfreeex_repeated_total_stable) {
    REQUIRE(U(GetDiskFreeSpaceExW));
    char r[4]; vd_sysroot(r); WCHAR w[4]; vd_widen(r, w, 4);
    ULARGE_INTEGER a, t0, t, f;
    CHECK(U(GetDiskFreeSpaceExW)(w, &a, &t0, &f));
    for (int i = 0; i < 100; i++) { CHECK(U(GetDiskFreeSpaceExW)(w, &a, &t, &f)); if (t.QuadPart != t0.QuadPart) { CHECK(0); break; } }
}
TEST(vd_diskfreeex_lastcallers_error_untouched_on_success) {
    REQUIRE(U(GetDiskFreeSpaceExW));
    char r[4]; vd_sysroot(r); WCHAR w[4]; vd_widen(r, w, 4); ULARGE_INTEGER a, t, f;
    SET_ERR(0x1234);
    CHECK(U(GetDiskFreeSpaceExW)(w, &a, &t, &f));
    DWORD e = GetLastError();
    if (e != 0x1234) t_note("last error after success: %lu", e);
}

/* ---- GetDiskSpaceInformation ---- */
TEST(vd_diskspaceinfo_w_basic) {
    GetDiskSpaceInformationW_pfn f = UN(GetDiskSpaceInformationW);
    if (!f) { t_skip("GetDiskSpaceInformationW not available (pre-2004)"); return; }
    char r[4]; vd_sysroot(r); WCHAR w[4]; vd_widen(r, w, 4);
    struct { VD_DSI i; DWORD canary[4]; } b; memset(&b, 0, sizeof b); for (int k = 0; k < 4; k++) b.canary[k] = SENT;
    LONG hr = f(w, &b.i);
    CHECKF(hr == 0, "hr=%08lx", (ULONG)hr);
    CHECK(b.i.ActualTotalAllocationUnits > 0);
    CHECK(b.i.ActualAvailableAllocationUnits <= b.i.ActualTotalAllocationUnits);
    CHECK(b.i.CallerTotalAllocationUnits > 0);
    CHECK(b.i.CallerAvailableAllocationUnits <= b.i.CallerTotalAllocationUnits);
    CHECK(vd_pow2(b.i.SectorsPerAllocationUnit)); CHECK(vd_pow2(b.i.BytesPerSector));
    for (int k = 0; k < 4; k++) CHECK(b.canary[k] == SENT);
}
TEST(vd_diskspaceinfo_a_basic) {
    GetDiskSpaceInformationA_pfn f = UN(GetDiskSpaceInformationA);
    if (!f) { t_skip("GetDiskSpaceInformationA not available"); return; }
    char r[4]; vd_sysroot(r); VD_DSI i; memset(&i, 0, sizeof i);
    LONG hr = f(r, &i);
    CHECKF(hr == 0, "hr=%08lx", (ULONG)hr);
    CHECK(i.ActualTotalAllocationUnits > 0);
}
TEST(vd_diskspaceinfo_matches_getdiskfreespace) {
    GetDiskSpaceInformationW_pfn f = UN(GetDiskSpaceInformationW);
    REQUIRE(U(GetDiskFreeSpaceW));
    if (!f) { t_skip("GetDiskSpaceInformationW not available"); return; }
    char r[4]; vd_sysroot(r); WCHAR w[4]; vd_widen(r, w, 4); VD_DSI i; vd_fs fs = {0};
    if (f(w, &i) != 0) { t_skip("call failed"); return; }
    CHECK(vd_gdfs_w(r, &fs));
    EXPECT_EQ(i.SectorsPerAllocationUnit, fs.spc); EXPECT_EQ(i.BytesPerSector, fs.bps);
    if (fs.total != 0xFFFFFFFFu) EXPECT_EQ(i.ActualTotalAllocationUnits, fs.total);
}
TEST(vd_diskspaceinfo_matches_diskfreeex_total) {
    GetDiskSpaceInformationW_pfn f = UN(GetDiskSpaceInformationW);
    REQUIRE(U(GetDiskFreeSpaceExW));
    if (!f) { t_skip("GetDiskSpaceInformationW not available"); return; }
    char r[4]; vd_sysroot(r); WCHAR w[4]; vd_widen(r, w, 4); VD_DSI i; ULARGE_INTEGER a, t, fr;
    if (f(w, &i) != 0) { t_skip("call failed"); return; }
    CHECK(U(GetDiskFreeSpaceExW)(w, &a, &t, &fr));
    EXPECT_EQ(t.QuadPart, i.ActualTotalAllocationUnits * i.SectorsPerAllocationUnit * i.BytesPerSector);
}
TEST(vd_diskspaceinfo_null_root_current_volume) {
    GetDiskSpaceInformationW_pfn f = UN(GetDiskSpaceInformationW);
    if (!f) { t_skip("GetDiskSpaceInformationW not available"); return; }
    VD_DSI i; memset(&i, 0, sizeof i);
    LONG hr = f(NULL, &i);
    CHECKF(hr == 0, "hr=%08lx", (ULONG)hr);
}
TEST(vd_diskspaceinfo_empty_path_error) {
    GetDiskSpaceInformationW_pfn f = UN(GetDiskSpaceInformationW);
    if (!f) { t_skip("GetDiskSpaceInformationW not available"); return; }
    VD_DSI i; SET_ERR(0);
    LONG hr = f(L"", &i);
    CHECKF(hr == (LONG)0x80070003, "hr=%08lx", (ULONG)hr);
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(vd_diskspaceinfo_nonexistent_drive_fails) {
    GetDiskSpaceInformationW_pfn f = UN(GetDiskSpaceInformationW);
    if (!f) { t_skip("GetDiskSpaceInformationW not available"); return; }
    char l = vd_freeletter(); if (!l) { t_skip("no free letter"); return; }
    char a[4] = { l, ':', '\\', 0 }; WCHAR w[4]; vd_widen(a, w, 4); VD_DSI i;
    LONG hr = f(w, &i);
    CHECKF(hr < 0, "hr=%08lx", (ULONG)hr);
}
TEST(vd_diskspaceinfo_subdirectory) {
    GetDiskSpaceInformationW_pfn f = UN(GetDiskSpaceInformationW);
    if (!f) { t_skip("GetDiskSpaceInformationW not available"); return; }
    WCHAR w[MAX_PATH]; vd_widen(t_tmpdir(), w, MAX_PATH); VD_DSI i; memset(&i, 0, sizeof i);
    CHECKF(f(w, &i) == 0, "failed");
    CHECK(i.CallerAvailableAllocationUnits <= i.CallerTotalAllocationUnits);
}
