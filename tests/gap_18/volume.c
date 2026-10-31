#include "g18.h"
/* gap18: DefineDosDeviceW, Set/DeleteVolumeMountPointA, FindNextVolumeW, FindNextVolumeMountPointW,
 * GetDiskSpaceInformationA. Only provably side-effect-free error paths are used for mount points
 * and device definitions, except one self-cleaning define/remove of a uniquely named device. */

static WCHAR *g18_wide_tmp(WCHAR *w, int cap, const char *leaf) { char p[MAX_PATH]; g18_path(p, MAX_PATH, leaf); g18_widen(w, cap, p); return w; }

/* ------------------------- DefineDosDeviceW ------------------------- */
#define DDN L"K32T_GAP18_DDD"
#define DDC_REMOVE (DDD_REMOVE_DEFINITION | DDD_EXACT_MATCH_ON_REMOVE)
TEST(gap18_ddd_reject_unknown_flags) {
    REQUIRE(U(DefineDosDeviceW));
    static const DWORD bad[] = { 0x20, 0x40, 0x80, 0x100, 0x10000, 0x80000000, 0xFFFFFFE0, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SET_ERR(0);
        BOOL r = U(DefineDosDeviceW)(bad[i], DDN, L"C:\\");
        CHECKF(!r, "flags 0x%lx accepted", bad[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        if (r) U(DefineDosDeviceW)(DDC_REMOVE, DDN, NULL);
    }
}
TEST(gap18_ddd_exact_match_requires_remove) {
    REQUIRE(U(DefineDosDeviceW));
    SET_ERR(0);
    CHECK(!U(DefineDosDeviceW)(DDD_EXACT_MATCH_ON_REMOVE, DDN, L"C:\\"));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!U(DefineDosDeviceW)(DDD_EXACT_MATCH_ON_REMOVE | DDD_RAW_TARGET_PATH, DDN, L"\\Device\\Null"));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!U(DefineDosDeviceW)(DDD_EXACT_MATCH_ON_REMOVE | DDD_NO_BROADCAST_SYSTEM, DDN, L"C:\\"));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap18_ddd_null_target_without_remove) {
    REQUIRE(U(DefineDosDeviceW));
    static const DWORD fl[] = { 0, DDD_RAW_TARGET_PATH, DDD_NO_BROADCAST_SYSTEM, DDD_RAW_TARGET_PATH | DDD_NO_BROADCAST_SYSTEM };
    for (unsigned i = 0; i < 4; i++) {
        SET_ERR(0);
        CHECKF(!U(DefineDosDeviceW)(fl[i], DDN, NULL), "flags 0x%lx", fl[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(gap18_ddd_luid_broadcast_flag_combinations) {
    REQUIRE(U(DefineDosDeviceW));
    if (t_is_wine()) { t_skip("wine faults on NULL device name"); return; }
    /* DDD_LUID_BROADCAST_DRIVE (0x10) is only valid with a name, no target, and none of RAW/REMOVE/EXACT/NO_BROADCAST */
    SET_ERR(0); CHECK(!U(DefineDosDeviceW)(0x10, NULL, NULL)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!U(DefineDosDeviceW)(0x10, L"X:", L"C:\\")); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!U(DefineDosDeviceW)(0x11, L"X:", NULL)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!U(DefineDosDeviceW)(0x18, L"X:", NULL)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!U(DefineDosDeviceW)(0x14, L"X:", NULL)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!U(DefineDosDeviceW)(0x10 | 2, NULL, NULL)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap18_ddd_overlong_names) {
    REQUIRE(U(DefineDosDeviceW));
    static WCHAR big[40001];
    for (int i = 0; i < 40000; i++) big[i] = L'A';
    big[40000] = 0;
    SET_ERR(0);
    CHECK(!U(DefineDosDeviceW)(DDD_NO_BROADCAST_SYSTEM, big, L"C:\\"));
    EXPECT_ERR(ERROR_FILENAME_EXCED_RANGE);
    SET_ERR(0);
    CHECK(!U(DefineDosDeviceW)(DDD_NO_BROADCAST_SYSTEM | DDD_RAW_TARGET_PATH, DDN, big));
    EXPECT_ERR(ERROR_FILENAME_EXCED_RANGE);
}
TEST(gap18_ddd_invalid_target_path) {
    REQUIRE(U(DefineDosDeviceW)); REQUIRE(U(QueryDosDeviceW));
    SET_ERR(0);
    BOOL r = U(DefineDosDeviceW)(DDD_NO_BROADCAST_SYSTEM, DDN L"_EP", L"");
    DWORD e = GetLastError();
    if (r) { U(DefineDosDeviceW)(DDC_REMOVE | DDD_NO_BROADCAST_SYSTEM, DDN L"_EP", NULL); t_note("empty target unexpectedly defined a device"); }
    else { CHECK(e == ERROR_INVALID_NAME || e == ERROR_BAD_PATHNAME); t_note("empty target err %lu", e); }
    WCHAR b[64];
    SET_ERR(0);
    CHECK(U(QueryDosDeviceW)(DDN L"_EP", b, 64) == 0);       /* nothing was created */
}
TEST(gap18_ddd_define_query_remove_cycle) {
    REQUIRE(U(DefineDosDeviceW)); REQUIRE(U(QueryDosDeviceW));
    WCHAR name[64]; wsprintfW(name, L"K32T_G18_%lu", GetCurrentProcessId());
    WCHAR tgt[MAX_PATH]; g18_wide_tmp(tgt, MAX_PATH, "");
    size_t l = lstrlenW(tgt); if (l > 3 && tgt[l - 1] == L'\\') tgt[l - 1] = 0;
    WCHAR q[MAX_PATH + 16];
    SET_ERR(0);
    if (!U(DefineDosDeviceW)(DDD_NO_BROADCAST_SYSTEM, name, tgt)) { t_skip("define refused (err %lu)", GetLastError()); return; }
    DWORD n = U(QueryDosDeviceW)(name, q, MAX_PATH + 16);
    CHECK(n > 0);
    if (n) { CHECK(q[0] == L'\\' && q[1] == L'?' && q[2] == L'?' && q[3] == L'\\'); CHECK(!lstrcmpiW(q + 4, tgt)); }
    /* removing with the wrong target and EXACT_MATCH does not remove */
    SET_ERR(0);
    U(DefineDosDeviceW)(DDC_REMOVE | DDD_NO_BROADCAST_SYSTEM, name, L"C:\\k32t_not_this");
    CHECK(U(QueryDosDeviceW)(name, q, MAX_PATH + 16) > 0);
    CHECK(U(DefineDosDeviceW)(DDC_REMOVE | DDD_NO_BROADCAST_SYSTEM, name, tgt));
    SET_ERR(0);
    CHECK(U(QueryDosDeviceW)(name, q, MAX_PATH + 16) == 0);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    U(DefineDosDeviceW)(DDD_REMOVE_DEFINITION | DDD_NO_BROADCAST_SYSTEM, name, NULL);   /* safety net */
}
TEST(gap18_ddd_define_raw_target_cycle) {
    REQUIRE(U(DefineDosDeviceW)); REQUIRE(U(QueryDosDeviceW));
    WCHAR name[64]; wsprintfW(name, L"K32T_G18R_%lu", GetCurrentProcessId());
    WCHAR q[128];
    if (!U(DefineDosDeviceW)(DDD_NO_BROADCAST_SYSTEM | DDD_RAW_TARGET_PATH, name, L"\\Device\\Null")) { t_skip("define refused (err %lu)", GetLastError()); return; }
    DWORD n = U(QueryDosDeviceW)(name, q, 128);
    CHECK(n > 0);
    if (n) CHECK(!lstrcmpW(q, L"\\Device\\Null"));
    CHECK(U(DefineDosDeviceW)(DDD_REMOVE_DEFINITION | DDD_EXACT_MATCH_ON_REMOVE | DDD_NO_BROADCAST_SYSTEM | DDD_RAW_TARGET_PATH, name, L"\\Device\\Null"));
    SET_ERR(0);
    CHECK(U(QueryDosDeviceW)(name, q, 128) == 0);
    U(DefineDosDeviceW)(DDD_REMOVE_DEFINITION | DDD_NO_BROADCAST_SYSTEM, name, NULL);
}
TEST(gap18_ddd_kernel32_export) { REQUIRE(UK(DefineDosDeviceW)); CHECK(1); }

/* ------------------------- Delete/SetVolumeMountPoint ------------------------- */
TEST(gap18_delvmp_requires_trailing_backslash) {
    REQUIRE(U(DeleteVolumeMountPointA));
    char d[MAX_PATH]; g18_path(d, MAX_PATH, "vmp_dir"); CreateDirectoryA(d, NULL);
    SET_ERR(0);
    CHECK(!U(DeleteVolumeMountPointA)(d));
    EXPECT_ERR(ERROR_INVALID_NAME);
    CHECK(!(GetFileAttributesA(d) & FILE_ATTRIBUTE_REPARSE_POINT));
    g18_rmdir(d);
    SET_ERR(0); CHECK(!U(DeleteVolumeMountPointA)("C:")); EXPECT_ERR(ERROR_INVALID_NAME);
    SET_ERR(0); CHECK(!U(DeleteVolumeMountPointA)("a")); EXPECT_ERR(ERROR_INVALID_NAME);
    SET_ERR(0); CHECK(!U(DeleteVolumeMountPointA)("\\\\?\\Volume{00000000-0000-0000-0000-000000000000}")); EXPECT_ERR(ERROR_INVALID_NAME);
}
TEST(gap18_delvmp_w_null) {
    REQUIRE(U(DeleteVolumeMountPointW));
    SET_ERR(0);
    CHECK(!U(DeleteVolumeMountPointW)(NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap18_delvmp_plain_directory_is_not_a_mount_point) {
    REQUIRE(U(DeleteVolumeMountPointA));
    char d[MAX_PATH + 4]; g18_path(d, MAX_PATH, "vmp_plain"); CreateDirectoryA(d, NULL);
    strcat(d, "\\");
    SET_ERR(0);
    CHECK(!U(DeleteVolumeMountPointA)(d));
    t_note("DeleteVolumeMountPoint(plain dir) err %lu", GetLastError());
    d[strlen(d) - 1] = 0;
    CHECK(GetFileAttributesA(d) != INVALID_FILE_ATTRIBUTES);
    CHECK(!(GetFileAttributesA(d) & FILE_ATTRIBUTE_REPARSE_POINT));
    g18_rmdir(d);
}
TEST(gap18_delvmp_nonexistent_directory) {
    REQUIRE(U(DeleteVolumeMountPointA));
    char d[MAX_PATH + 4]; g18_path(d, MAX_PATH, "vmp_missing_zz\\");
    SET_ERR(0);
    CHECK(!U(DeleteVolumeMountPointA)(d));
    t_note("DeleteVolumeMountPoint(missing dir) err %lu", GetLastError());
}
TEST(gap18_delvmp_regular_file_with_slash) {
    REQUIRE(U(DeleteVolumeMountPointA));
    char f[MAX_PATH + 4]; g18_path(f, MAX_PATH, "vmp_file.txt"); g18_put(f, "keep", 4);
    strcat(f, "\\");
    SET_ERR(0);
    CHECK(!U(DeleteVolumeMountPointA)(f));
    f[strlen(f) - 1] = 0;
    CHECK(g18_content_is(f, "keep"));
    g18_rm(f);
}
TEST(gap18_setvmp_garbage_volume_name_rejected) {
    REQUIRE(U(SetVolumeMountPointA));
    char d[MAX_PATH + 4]; g18_path(d, MAX_PATH, "vmp_set"); CreateDirectoryA(d, NULL); strcat(d, "\\");
    static const char *names[] = { "", "x", "C:\\", "\\\\?\\Volume{}\\", "\\\\?\\Volume{zz}", "garbage\\", "\\\\?\\Volume{00000000-0000-0000-0000-000000000000}" };
    for (unsigned i = 0; i < sizeof names / sizeof names[0]; i++) {
        SET_ERR(0);
        CHECKF(!U(SetVolumeMountPointA)(d, names[i]), "volume name '%s' accepted", names[i]);
        CHECKF(GetLastError() != 0, "volume name '%s': no error", names[i]);
    }
    d[strlen(d) - 1] = 0;
    CHECK(!(GetFileAttributesA(d) & FILE_ATTRIBUTE_REPARSE_POINT));
    g18_rmdir(d);
}
TEST(gap18_setvmp_nonexistent_volume_guid) {
    REQUIRE(U(SetVolumeMountPointA));
    char d[MAX_PATH + 4]; g18_path(d, MAX_PATH, "vmp_set2"); CreateDirectoryA(d, NULL); strcat(d, "\\");
    SET_ERR(0);
    CHECK(!U(SetVolumeMountPointA)(d, "\\\\?\\Volume{00000000-0000-0000-0000-000000000000}\\"));
    t_note("SetVolumeMountPoint(bogus GUID) err %lu", GetLastError());
    d[strlen(d) - 1] = 0;
    CHECK(!(GetFileAttributesA(d) & FILE_ATTRIBUTE_REPARSE_POINT));
    g18_rmdir(d);
}
TEST(gap18_setvmp_no_trailing_backslash_mount_point) {
    REQUIRE(U(SetVolumeMountPointA));
    char d[MAX_PATH]; g18_path(d, MAX_PATH, "vmp_set3"); CreateDirectoryA(d, NULL);
    SET_ERR(0);
    CHECK(!U(SetVolumeMountPointA)(d, "\\\\?\\Volume{00000000-0000-0000-0000-000000000000}\\"));
    t_note("SetVolumeMountPoint(no trailing slash) err %lu", GetLastError());
    CHECK(!(GetFileAttributesA(d) & FILE_ATTRIBUTE_REPARSE_POINT));
    g18_rmdir(d);
}
TEST(gap18_setvmp_missing_mount_dir) {
    REQUIRE(U(SetVolumeMountPointA));
    char d[MAX_PATH + 4]; g18_path(d, MAX_PATH, "vmp_nodir_zz\\");
    SET_ERR(0);
    CHECK(!U(SetVolumeMountPointA)(d, "\\\\?\\Volume{00000000-0000-0000-0000-000000000000}\\"));
    CHECK(GetLastError() != 0);
    CHECK(GetFileAttributesA(d) == INVALID_FILE_ATTRIBUTES);
}
TEST(gap18_setvmp_w_null_args) {
    REQUIRE(U(SetVolumeMountPointW));
    SET_ERR(0); CHECK(!U(SetVolumeMountPointW)(NULL, L"\\\\?\\Volume{00000000-0000-0000-0000-000000000000}\\")); t_note("NULL mount point err %lu", GetLastError());
    SET_ERR(0); CHECK(!U(SetVolumeMountPointW)(L"C:\\k32t_nowhere\\", NULL)); t_note("NULL volume err %lu", GetLastError());
    SET_ERR(0); CHECK(!U(SetVolumeMountPointW)(NULL, NULL)); t_note("both NULL err %lu", GetLastError());
}
TEST(gap18_setvmp_existing_mount_point_refused) {
    REQUIRE(U(SetVolumeMountPointA)); REQUIRE(U(GetVolumeNameForVolumeMountPointA));
    char win[MAX_PATH]; GetWindowsDirectoryA(win, MAX_PATH);
    char root[4] = { win[0], ':', '\\', 0 }, vol[64];
    if (!U(GetVolumeNameForVolumeMountPointA)(root, vol, sizeof vol)) { t_skip("no volume name for %s", root); return; }
    /* a path that already is a mount point cannot be re-pointed; the check precedes any change.
       The bogus GUID would be rejected anyway, so nothing can be altered. */
    SET_ERR(0);
    CHECK(!U(SetVolumeMountPointA)(root, "\\\\?\\Volume{00000000-0000-0000-0000-000000000000}\\"));
    DWORD e = GetLastError();
    t_note("SetVolumeMountPoint(existing root) err %lu", e);
    CHECK(e == ERROR_DIR_NOT_EMPTY || e != 0);
    char vol2[64]; CHECK(U(GetVolumeNameForVolumeMountPointA)(root, vol2, sizeof vol2)); CHECK(!strcmp(vol, vol2));
}
TEST(gap18_vmp_kernel32_exports) { REQUIRE(UK(SetVolumeMountPointA)); REQUIRE(UK(DeleteVolumeMountPointA)); CHECK(1); }

/* ------------------------- FindNextVolumeW ------------------------- */
static int is_vol_name(const WCHAR *s) {
    if (lstrlenW(s) != 49) return 0;
    if (memcmp(s, L"\\\\?\\Volume{", 11 * 2)) return 0;
    if (s[19] != L'-' || s[24] != L'-' || s[29] != L'-' || s[34] != L'-' || s[47] != L'}' || s[48] != L'\\') return 0;
    return 1;
}
TEST(gap18_findnextvolume_enumerate_all) {
    REQUIRE(U(FindFirstVolumeW)); REQUIRE(U(FindNextVolumeW)); REQUIRE(U(FindVolumeClose));
    WCHAR v[MAX_PATH]; static WCHAR all[64][50]; int n = 0;
    HANDLE h = U(FindFirstVolumeW)(v, MAX_PATH);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        CHECKF(is_vol_name(v), "bad volume name format");
        if (n < 64) { lstrcpyW(all[n], v); for (int j = 0; j < n; j++) CHECK(lstrcmpiW(all[j], v)); n++; }
        SET_ERR(0);
    } while (U(FindNextVolumeW)(h, v, MAX_PATH) && n < 64);
    EXPECT_ERR(ERROR_NO_MORE_FILES);
    SET_ERR(0);
    CHECK(!U(FindNextVolumeW)(h, v, MAX_PATH));        /* stays exhausted */
    t_note("exhausted-handle repeat err %lu", GetLastError());
    CHECK(U(FindVolumeClose)(h));
    CHECK(n >= 1);
    t_note("%d volumes", n);
}
TEST(gap18_findnextvolume_contains_system_volume) {
    REQUIRE(U(FindFirstVolumeW)); REQUIRE(U(FindNextVolumeW)); REQUIRE(U(FindVolumeClose)); REQUIRE(U(GetVolumeNameForVolumeMountPointW));
    WCHAR win[MAX_PATH]; GetWindowsDirectoryW(win, MAX_PATH);
    WCHAR root[4] = { win[0], L':', L'\\', 0 }, sysvol[64];
    if (!U(GetVolumeNameForVolumeMountPointW)(root, sysvol, 64)) { t_skip("no system volume name"); return; }
    WCHAR v[MAX_PATH]; int found = 0;
    HANDLE h = U(FindFirstVolumeW)(v, MAX_PATH);
    if (h == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    do { if (!lstrcmpiW(v, sysvol)) found = 1; } while (U(FindNextVolumeW)(h, v, MAX_PATH));
    U(FindVolumeClose)(h);
    CHECK(found);
}
TEST(gap18_findnextvolume_buffer_boundaries) {
    REQUIRE(U(FindFirstVolumeW)); REQUIRE(U(FindNextVolumeW)); REQUIRE(U(FindVolumeClose));
    WCHAR v[MAX_PATH];
    HANDLE h = U(FindFirstVolumeW)(v, MAX_PATH);
    if (h == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    /* the second volume name needs 50 characters including the terminator; 49 is too small */
    WCHAR small[80]; for (int i = 0; i < 80; i++) small[i] = 0x7777;
    SET_ERR(0);
    BOOL r49 = U(FindNextVolumeW)(h, small, 49);
    DWORD e49 = GetLastError();
    if (r49) { t_note("only-one-volume or implementation accepted 49"); }
    else { CHECK(e49 != 0); t_note("cch=49 err %lu", e49); CHECK(small[49] == 0x7777); }
    U(FindVolumeClose)(h);
    h = U(FindFirstVolumeW)(v, MAX_PATH);
    for (int i = 0; i < 80; i++) small[i] = 0x7777;
    SET_ERR(0);
    BOOL r50 = U(FindNextVolumeW)(h, small, 50);
    if (r50) { CHECK(is_vol_name(small)); }
    else { EXPECT_ERR(ERROR_NO_MORE_FILES); t_note("single-volume system"); }
    CHECK(small[50] == 0x7777);          /* never writes past cch */
    U(FindVolumeClose)(h);
}
TEST(gap18_findnextvolume_cch_zero_and_one) {
    REQUIRE(U(FindFirstVolumeW)); REQUIRE(U(FindNextVolumeW)); REQUIRE(U(FindVolumeClose));
    WCHAR v[MAX_PATH]; HANDLE h = U(FindFirstVolumeW)(v, MAX_PATH);
    if (h == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    WCHAR b[4] = { 0x7777, 0x7777, 0x7777, 0x7777 };
    SET_ERR(0); CHECK(!U(FindNextVolumeW)(h, b, 0)); t_note("cch=0 err %lu", GetLastError());
    SET_ERR(0); CHECK(!U(FindNextVolumeW)(h, b, 1)); t_note("cch=1 err %lu", GetLastError());
    CHECK(b[1] == 0x7777 && b[2] == 0x7777 && b[3] == 0x7777);
    U(FindVolumeClose)(h);
}
TEST(gap18_findnextvolume_two_enumerations_agree) {
    REQUIRE(U(FindFirstVolumeW)); REQUIRE(U(FindNextVolumeW)); REQUIRE(U(FindVolumeClose));
    WCHAR a[MAX_PATH], b[MAX_PATH];
    HANDLE h1 = U(FindFirstVolumeW)(a, MAX_PATH), h2 = U(FindFirstVolumeW)(b, MAX_PATH);
    CHECK(h1 != INVALID_HANDLE_VALUE && h2 != INVALID_HANDLE_VALUE && h1 != h2);
    if (h1 == INVALID_HANDLE_VALUE || h2 == INVALID_HANDLE_VALUE) return;
    int i = 0;
    for (;;) {
        CHECKF(!lstrcmpiW(a, b), "enumeration %d differs", i);
        BOOL r1 = U(FindNextVolumeW)(h1, a, MAX_PATH), r2 = U(FindNextVolumeW)(h2, b, MAX_PATH);
        CHECK(r1 == r2);
        if (!r1 || !r2 || ++i > 64) break;
    }
    U(FindVolumeClose)(h1); U(FindVolumeClose)(h2);
}
TEST(gap18_findnextvolume_k32_forwarder) { REQUIRE(UK(FindNextVolumeW)); REQUIRE(UB(FindNextVolumeW)); REQUIRE(UK(FindFirstVolumeW)); CHECK(1); }

/* ------------------------- FindNextVolumeMountPointW ------------------------- */
TEST(gap18_findnextvmp_invalid_handles) {
    if (t_is_wine()) { t_skip("unimplemented under wine"); return; }
    REQUIRE(U(FindNextVolumeMountPointW));
    WCHAR b[MAX_PATH];
    SET_ERR(0); CHECK(!U(FindNextVolumeMountPointW)(NULL, b, MAX_PATH)); CHECK(GetLastError() != 0);
    SET_ERR(0); CHECK(!U(FindNextVolumeMountPointW)(INVALID_HANDLE_VALUE, b, MAX_PATH)); CHECK(GetLastError() != 0);
    t_note("NULL handle err above; INVALID_HANDLE_VALUE err %lu", GetLastError());
}
TEST(gap18_findfirstvmp_bad_roots) {
    if (t_is_wine()) { t_skip("unimplemented under wine"); return; }
    REQUIRE(U(FindFirstVolumeMountPointW));
    WCHAR b[MAX_PATH];
    SET_ERR(0);
    HANDLE h = U(FindFirstVolumeMountPointW)(NULL, b, MAX_PATH);
    CHECK(h == INVALID_HANDLE_VALUE);
    CHECK(GetLastError() != 0);
    t_note("NULL root err %lu", GetLastError());
    SET_ERR(0);
    h = U(FindFirstVolumeMountPointW)(L"C:", b, MAX_PATH);          /* missing trailing backslash */
    CHECK(h == INVALID_HANDLE_VALUE);
    CHECK(GetLastError() != 0);
    t_note("no trailing backslash err %lu", GetLastError());
    SET_ERR(0);
    h = U(FindFirstVolumeMountPointW)(L"", b, MAX_PATH);
    t_note("empty root -> %p err %lu", h, GetLastError());
    if (h != INVALID_HANDLE_VALUE && U(FindVolumeMountPointClose)) U(FindVolumeMountPointClose)(h);
}
TEST(gap18_findnextvmp_enumerate_volume_root) {
    if (t_is_wine()) { t_skip("unimplemented under wine"); return; }
    REQUIRE(U(FindFirstVolumeMountPointW)); REQUIRE(U(FindNextVolumeMountPointW)); REQUIRE(U(FindVolumeMountPointClose));
    WCHAR win[MAX_PATH]; GetWindowsDirectoryW(win, MAX_PATH);
    WCHAR root[4] = { win[0], L':', L'\\', 0 }, b[MAX_PATH];
    SET_ERR(0);
    HANDLE h = U(FindFirstVolumeMountPointW)(root, b, MAX_PATH);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD e = GetLastError();
        t_note("FindFirstVolumeMountPoint(%ls) failed %lu (no mount points / not elevated)", root, e);
        CHECK(e == ERROR_NO_MORE_FILES || e == ERROR_ACCESS_DENIED || e == ERROR_PATH_NOT_FOUND || e == ERROR_FILE_NOT_FOUND || e == ERROR_INVALID_PARAMETER);
        return;
    }
    int n = 1;
    CHECK(b[0] != 0);
    size_t l = lstrlenW(b); CHECK(b[l - 1] == L'\\');          /* mount point names are returned with trailing backslash */
    SET_ERR(0);
    while (U(FindNextVolumeMountPointW)(h, b, MAX_PATH) && n < 10000) n++;
    EXPECT_ERR(ERROR_NO_MORE_FILES);
    CHECK(U(FindVolumeMountPointClose)(h));
    t_note("%d mount points under %ls", n, root);
}
TEST(gap18_findnextvmp_small_buffer) {
    if (t_is_wine()) { t_skip("unimplemented under wine"); return; }
    REQUIRE(U(FindFirstVolumeMountPointW)); REQUIRE(U(FindNextVolumeMountPointW)); REQUIRE(U(FindVolumeMountPointClose));
    WCHAR win[MAX_PATH]; GetWindowsDirectoryW(win, MAX_PATH);
    WCHAR root[4] = { win[0], L':', L'\\', 0 }, b[MAX_PATH];
    HANDLE h = U(FindFirstVolumeMountPointW)(root, b, MAX_PATH);
    if (h == INVALID_HANDLE_VALUE) { t_skip("no mount points on %ls", root); return; }
    WCHAR s[2] = { 0x7777, 0x7777 };
    SET_ERR(0);
    BOOL r = U(FindNextVolumeMountPointW)(h, s, 1);
    t_note("small-buffer next -> %d err %lu", r, GetLastError());
    U(FindVolumeMountPointClose)(h);
}

/* ------------------------- GetDiskSpaceInformationA ------------------------- */
typedef struct { ULONGLONG ActualTotal, ActualAvail, ActualPoolUnavail, CallerTotal, CallerAvail, CallerPoolUnavail, Used, TotalReserved, VolumeStorageReserve, AvailableCommitted, PoolAvailable; DWORD SectorsPerAU, BytesPerSector; } g18_dsi;
typedef HRESULT (WINAPI *gdsi_a_t)(LPCSTR, g18_dsi *);
typedef HRESULT (WINAPI *gdsi_w_t)(LPCWSTR, g18_dsi *);

TEST(gap18_gdsia_basic_tmpdir) {
    gdsi_a_t f = (gdsi_a_t)t_proc("GetDiskSpaceInformationA"); REQUIRE(f);
    g18_dsi d; memset(&d, 0xCC, sizeof d);
    SET_ERR(0);
    HRESULT hr = f(t_tmpdir(), &d);
    EQ(hr, 0);
    CHECK(d.SectorsPerAU > 0);
    CHECK(d.BytesPerSector >= 512 && (d.BytesPerSector & (d.BytesPerSector - 1)) == 0);
    CHECK(d.ActualTotal > 0);
    CHECK(d.ActualAvail <= d.ActualTotal);
    CHECK(d.CallerTotal <= d.ActualTotal);
    CHECK(d.CallerAvail <= d.CallerTotal);
    CHECK(d.CallerAvail <= d.ActualAvail);
    if (d.Used > d.ActualTotal) t_note("Used %llu > ActualTotal %llu", d.Used, d.ActualTotal);
}
TEST(gap18_gdsia_null_path_uses_current_drive) {
    gdsi_a_t f = (gdsi_a_t)t_proc("GetDiskSpaceInformationA"); REQUIRE(f);
    g18_dsi d; memset(&d, 0, sizeof d);
    SET_ERR(0);
    HRESULT hr = f(NULL, &d);
    EQ(hr, 0);
    CHECK(d.ActualTotal > 0);
}
TEST(gap18_gdsia_null_buffer) {
    gdsi_a_t f = (gdsi_a_t)t_proc("GetDiskSpaceInformationA"); REQUIRE(f);
    SET_ERR(0);
    HRESULT hr = f(t_tmpdir(), NULL);
    EQ((DWORD)hr, 0xD000000Du);
    EXPECT_ERR(0xC000000Du);        /* the raw NTSTATUS is stored as the last error */
}
TEST(gap18_gdsia_empty_path) {
    gdsi_a_t f = (gdsi_a_t)t_proc("GetDiskSpaceInformationA"); REQUIRE(f);
    g18_dsi d; memset(&d, 0, sizeof d);
    SET_ERR(0);
    HRESULT hr = f("", &d);
    EQ((DWORD)hr, 0x80070003u);
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(gap18_gdsia_missing_directory) {
    gdsi_a_t f = (gdsi_a_t)t_proc("GetDiskSpaceInformationA"); REQUIRE(f);
    char p[MAX_PATH]; g18_path(p, MAX_PATH, "dsi_missing_dir\\");
    g18_dsi d; memset(&d, 0xCC, sizeof d);
    SET_ERR(0);
    HRESULT hr = f(p, &d);
    CHECK(((DWORD)hr & 0xF0000000u) == 0xD0000000u);
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);       /* file-not-found is promoted to path-not-found */
    unsigned char *b = (unsigned char *)&d; int touched = 0; for (size_t i = 0; i < sizeof d; i++) touched |= (b[i] != 0xCC);
    CHECK(!touched);
}
TEST(gap18_gdsia_path_is_a_file) {
    gdsi_a_t f = (gdsi_a_t)t_proc("GetDiskSpaceInformationA"); REQUIRE(f);
    char p[MAX_PATH]; g18_path(p, MAX_PATH, "dsi_file.txt"); g18_put(p, "x", 1);
    g18_dsi d; memset(&d, 0, sizeof d);
    SET_ERR(0);
    HRESULT hr = f(p, &d);
    CHECK(((DWORD)hr & 0xF0000000u) == 0xD0000000u);
    t_note("file path hr 0x%lx err %lu", (DWORD)hr, GetLastError());
    g18_rm(p);
}
TEST(gap18_gdsia_subdirectory_equals_root) {
    gdsi_a_t f = (gdsi_a_t)t_proc("GetDiskSpaceInformationA"); REQUIRE(f);
    char sub[MAX_PATH], root[4]; g18_path(sub, MAX_PATH, "dsi_sub"); CreateDirectoryA(sub, NULL);
    root[0] = t_tmpdir()[0]; root[1] = ':'; root[2] = '\\'; root[3] = 0;
    g18_dsi a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    EQ(f(sub, &a), 0); EQ(f(root, &b), 0);
    EQ(a.ActualTotal, b.ActualTotal);
    EQ(a.SectorsPerAU, b.SectorsPerAU); EQ(a.BytesPerSector, b.BytesPerSector);
    g18_rmdir(sub);
}
TEST(gap18_gdsia_trailing_backslash_irrelevant) {
    gdsi_a_t f = (gdsi_a_t)t_proc("GetDiskSpaceInformationA"); REQUIRE(f);
    char p[MAX_PATH]; strcpy(p, t_tmpdir()); size_t l = strlen(p);
    g18_dsi a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    EQ(f(p, &a), 0);
    if (p[l - 1] == '\\') p[l - 1] = 0;
    EQ(f(p, &b), 0);
    EQ(a.ActualTotal, b.ActualTotal);
}
TEST(gap18_gdsia_wide_parity) {
    gdsi_a_t fa = (gdsi_a_t)t_proc("GetDiskSpaceInformationA"); gdsi_w_t fw = (gdsi_w_t)t_proc("GetDiskSpaceInformationW"); REQUIRE(fa); REQUIRE(fw);
    WCHAR w[MAX_PATH]; g18_widen(w, MAX_PATH, t_tmpdir());
    g18_dsi a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    EQ(fa(t_tmpdir(), &a), 0); EQ(fw(w, &b), 0);
    EQ(a.ActualTotal, b.ActualTotal); EQ(a.SectorsPerAU, b.SectorsPerAU); EQ(a.BytesPerSector, b.BytesPerSector);
    EQ(a.CallerTotal, b.CallerTotal);
    /* failure parity */
    char bad[MAX_PATH]; g18_path(bad, MAX_PATH, "dsi_nope\\"); WCHAR wb[MAX_PATH]; g18_widen(wb, MAX_PATH, bad);
    SET_ERR(0); HRESULT h1 = fa(bad, &a); DWORD e1 = GetLastError();
    SET_ERR(0); HRESULT h2 = fw(wb, &b); DWORD e2 = GetLastError();
    EQ(h1, h2); EQ(e1, e2);
}
TEST(gap18_gdsia_matches_getdiskfreespaceex) {
    gdsi_a_t f = (gdsi_a_t)t_proc("GetDiskSpaceInformationA"); REQUIRE(f); REQUIRE(U(GetDiskFreeSpaceExA)); REQUIRE(U(GetDiskFreeSpaceA));
    g18_dsi d; memset(&d, 0, sizeof d);
    EQ(f(t_tmpdir(), &d), 0);
    ULARGE_INTEGER avail, total, fr;
    CHECK(U(GetDiskFreeSpaceExA)(t_tmpdir(), &avail, &total, &fr));
    unsigned long long unit = (unsigned long long)d.SectorsPerAU * d.BytesPerSector;
    t_note("Ex total %llu vs info caller total %llu", total.QuadPart, d.CallerTotal * unit);
    CHECK(d.CallerAvail * unit <= total.QuadPart);
    char root[4] = { t_tmpdir()[0], ':', '\\', 0 };
    DWORD spc, bps, nf, tc;
    if (U(GetDiskFreeSpaceA)(root, &spc, &bps, &nf, &tc)) { EQ(spc, d.SectorsPerAU); EQ(bps, d.BytesPerSector); }
}
TEST(gap18_gdsia_long_prefix) {
    gdsi_a_t f = (gdsi_a_t)t_proc("GetDiskSpaceInformationA"); REQUIRE(f);
    char p[MAX_PATH + 8]; strcpy(p, "\\\\?\\"); strcat(p, t_tmpdir());
    g18_dsi d; memset(&d, 0, sizeof d);
    EQ(f(p, &d), 0);
    CHECK(d.ActualTotal > 0);
}
TEST(gap18_gdsia_wildcard_path) {
    gdsi_a_t f = (gdsi_a_t)t_proc("GetDiskSpaceInformationA"); REQUIRE(f);
    char p[MAX_PATH]; g18_path(p, MAX_PATH, "dsi*");
    g18_dsi d; memset(&d, 0, sizeof d);
    SET_ERR(0);
    HRESULT hr = f(p, &d);
    CHECK(((DWORD)hr & 0xF0000000u) == 0xD0000000u);
    t_note("wildcard path hr 0x%lx err %lu", (DWORD)hr, GetLastError());
}
TEST(gap18_gdsia_repeat) {
    gdsi_a_t f = (gdsi_a_t)t_proc("GetDiskSpaceInformationA"); REQUIRE(f);
    g18_dsi d;
    for (int i = 0; i < 500; i++) { memset(&d, 0, sizeof d); if (f(t_tmpdir(), &d) != 0) { CHECKF(0, "iter %d", i); break; } }
}
