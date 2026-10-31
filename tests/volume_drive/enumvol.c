/* FindFirstVolume/FindNextVolume/FindVolumeClose, volume mount point enumeration, QueryDosDevice */
#include "vd.h"

#define VD_MAXVOL 128
typedef struct { char n[VD_MAXVOL][64]; int c; } vd_vols;

static int vd_enum_a(vd_vols *v) {
    HANDLE h = U(FindFirstVolumeA)(v->n[0], 64);
    if (h == INVALID_HANDLE_VALUE) return 0;
    v->c = 1;
    while (v->c < VD_MAXVOL && U(FindNextVolumeA)(h, v->n[v->c], 64)) v->c++;
    U(FindVolumeClose)(h);
    return 1;
}

TEST(vd_findvol_w_first_name_format) {
    REQUIRE(U(FindFirstVolumeW)); REQUIRE(U(FindVolumeClose));
    WCHAR b[64]; for (int i = 0; i < 64; i++) b[i] = 0x7777;
    HANDLE h = U(FindFirstVolumeW)(b, 50);
    if (h == INVALID_HANDLE_VALUE) { CHECKF(0, "FindFirstVolumeW err %lu", GetLastError()); return; }
    CHECK(vd_is_guid_name_w(b)); EXPECT_EQ(lstrlenW(b), VD_GUID_LEN);
    CHECK(U(FindVolumeClose)(h));
}
TEST(vd_findvol_a_first_name_format) {
    REQUIRE(U(FindFirstVolumeA)); REQUIRE(U(FindVolumeClose));
    char b[64]; HANDLE h = U(FindFirstVolumeA)(b, 50);
    if (h == INVALID_HANDLE_VALUE) { CHECKF(0, "err %lu", GetLastError()); return; }
    CHECKF(vd_is_guid_name_a(b), "name \"%s\"", b);
    CHECK(U(FindVolumeClose)(h));
}
TEST(vd_findvol_enumeration_terminates_with_no_more_files) {
    REQUIRE(U(FindFirstVolumeA)); REQUIRE(U(FindNextVolumeA)); REQUIRE(U(FindVolumeClose));
    char b[64]; HANDLE h = U(FindFirstVolumeA)(b, 64); if (h == INVALID_HANDLE_VALUE) { t_skip("no volumes"); return; }
    int n = 1;
    SET_ERR(0);
    while (n < 1000 && U(FindNextVolumeA)(h, b, 64)) n++;
    EXPECT_ERR(ERROR_NO_MORE_FILES);
    CHECK(n >= 1 && n < 1000);
    SET_ERR(0);
    CHECK(!U(FindNextVolumeA)(h, b, 64)); EXPECT_ERR(ERROR_NO_MORE_FILES);   /* stays exhausted */
    CHECK(U(FindVolumeClose)(h));
}
TEST(vd_findvol_ab_same_set) {
    REQUIRE(U(FindFirstVolumeA)); REQUIRE(U(FindFirstVolumeW)); REQUIRE(U(FindNextVolumeW)); REQUIRE(U(FindNextVolumeA));
    vd_vols a; memset(&a, 0, sizeof a); if (!vd_enum_a(&a)) { t_skip("none"); return; }
    WCHAR w[64]; char n[64]; int c = 0;
    HANDLE h = U(FindFirstVolumeW)(w, 64);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) {
        do { vd_narrow(w, n, 64); CHECKF(c < a.c && !strcmp(n, a.n[c]), "idx %d %s", c, n); c++; } while (U(FindNextVolumeW)(h, w, 64));
        EXPECT_EQ(c, a.c);
        U(FindVolumeClose)(h);
    }
}
TEST(vd_findvol_all_names_wellformed_unique) {
    REQUIRE(U(FindFirstVolumeA));
    vd_vols v; memset(&v, 0, sizeof v); if (!vd_enum_a(&v)) { t_skip("none"); return; }
    for (int i = 0; i < v.c; i++) {
        CHECKF(vd_is_guid_name_a(v.n[i]), "name %d \"%s\"", i, v.n[i]);
        for (int j = i + 1; j < v.c; j++) CHECKF(strcmp(v.n[i], v.n[j]) != 0, "dup %d/%d", i, j);
    }
}
TEST(vd_findvol_contains_system_volume) {
    REQUIRE(U(FindFirstVolumeA));
    char r[4]; vd_sysroot(r); char g[64]; if (!vd_volname(r, g)) { t_skip("no volume name"); return; }
    vd_vols v; memset(&v, 0, sizeof v); if (!vd_enum_a(&v)) { t_skip("none"); return; }
    int f = 0; for (int i = 0; i < v.c; i++) if (!vd_stricmp(v.n[i], g)) f = 1;
    CHECK(f);
}
TEST(vd_findvol_contains_every_fixed_letter_volume) {
    REQUIRE(U(FindFirstVolumeA));
    vd_vols v; memset(&v, 0, sizeof v); if (!vd_enum_a(&v)) { t_skip("none"); return; }
    DWORD m = vd_fixed_mask();
    for (int i = 0; i < 26; i++) {
        if (!((m >> i) & 1)) continue;
        char r[4] = { (char)('A' + i), ':', '\\', 0 }, g[64]; if (!vd_volname(r, g)) continue;
        int f = 0; for (int k = 0; k < v.c; k++) if (!vd_stricmp(v.n[k], g)) f = 1;
        CHECKF(f, "%s -> %s not enumerated", r, g);
    }
}
TEST(vd_findvol_names_usable_for_pathnames) {
    REQUIRE(U(FindFirstVolumeA)); REQUIRE(U(GetVolumePathNamesForVolumeNameA));
    vd_vols v; memset(&v, 0, sizeof v); if (!vd_enum_a(&v)) { t_skip("none"); return; }
    for (int i = 0; i < v.c; i++) {
        char buf[2048]; DWORD ret = 0; SET_ERR(0);
        BOOL ok = U(GetVolumePathNamesForVolumeNameA)(v.n[i], buf, sizeof buf, &ret);
        CHECKF(ok, "%s err %lu", v.n[i], GetLastError());
        if (ok) { CHECK(ret >= 1); CHECK(buf[ret - 1] == 0); }
    }
}
TEST(vd_findvol_names_drivetype_valid) {
    REQUIRE(U(FindFirstVolumeA)); REQUIRE(U(GetDriveTypeA));
    vd_vols v; memset(&v, 0, sizeof v); if (!vd_enum_a(&v)) { t_skip("none"); return; }
    for (int i = 0; i < v.c; i++) { UINT t = U(GetDriveTypeA)(v.n[i]); CHECKF(t != DRIVE_NO_ROOT_DIR && t <= DRIVE_RAMDISK, "%s type %u", v.n[i], t); }
}
TEST(vd_findvol_buffer_exactly_50_ok_49_fails) {
    REQUIRE(U(FindFirstVolumeA)); REQUIRE(U(FindFirstVolumeW)); REQUIRE(U(FindVolumeClose));
    char a[64]; WCHAR w[64]; HANDLE h;
    SET_ERR(0);
    h = U(FindFirstVolumeW)(w, 49);
    CHECK(h == INVALID_HANDLE_VALUE); t_note("FindFirstVolumeW(49) err %lu", GetLastError());
    if (h != INVALID_HANDLE_VALUE) U(FindVolumeClose)(h);
    SET_ERR(0);
    h = U(FindFirstVolumeA)(a, 49);
    CHECK(h == INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) U(FindVolumeClose)(h);
    h = U(FindFirstVolumeA)(a, 50); CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) U(FindVolumeClose)(h);
}
TEST(vd_findvol_small_buffer_error_code) {
    REQUIRE(U(FindFirstVolumeW));
    WCHAR w[64]; SET_ERR(0);
    HANDLE h = U(FindFirstVolumeW)(w, 10);
    CHECK(h == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_FILENAME_EXCED_RANGE);
}
TEST(vd_findvol_zero_buffer) {
    REQUIRE(U(FindFirstVolumeW));
    WCHAR w[4]; SET_ERR(0);
    HANDLE h = U(FindFirstVolumeW)(w, 0);
    CHECK(h == INVALID_HANDLE_VALUE);
    CHECK(GetLastError() != 0);
}
TEST(vd_findvol_next_small_buffer_keeps_position) {
    REQUIRE(U(FindFirstVolumeA)); REQUIRE(U(FindNextVolumeA)); REQUIRE(U(FindVolumeClose));
    char a[64]; HANDLE h = U(FindFirstVolumeA)(a, 64); if (h == INVALID_HANDLE_VALUE) { t_skip("none"); return; }
    char b[64];
    if (U(FindNextVolumeA)(h, b, 64)) {
        /* now fail a next with a tiny buffer, the entry must not be consumed */
        char c[64];
        if (U(FindNextVolumeA)(h, c, 10)) { t_note("tiny buffer accepted"); }
        else { EXPECT_ERR(ERROR_FILENAME_EXCED_RANGE); }
    } else t_note("only one volume");
    U(FindVolumeClose)(h);
}
TEST(vd_findvol_two_concurrent_enumerations) {
    REQUIRE(U(FindFirstVolumeA)); REQUIRE(U(FindNextVolumeA)); REQUIRE(U(FindVolumeClose));
    char a[64], b[64]; HANDLE h1 = U(FindFirstVolumeA)(a, 64), h2 = U(FindFirstVolumeA)(b, 64);
    if (h1 == INVALID_HANDLE_VALUE || h2 == INVALID_HANDLE_VALUE) { t_skip("none"); return; }
    CHECK(h1 != h2);
    EXPECT_STR(a, b);
    int n1 = 1, n2 = 1; while (U(FindNextVolumeA)(h1, a, 64)) n1++; while (U(FindNextVolumeA)(h2, b, 64)) n2++;
    EXPECT_EQ(n1, n2);
    CHECK(U(FindVolumeClose)(h1)); CHECK(U(FindVolumeClose)(h2));
}
TEST(vd_findvol_repeated_open_close) {
    REQUIRE(U(FindFirstVolumeA)); REQUIRE(U(FindVolumeClose));
    char a[64];
    for (int i = 0; i < 200; i++) { HANDLE h = U(FindFirstVolumeA)(a, 64); if (h == INVALID_HANDLE_VALUE) { CHECK(0); break; } U(FindVolumeClose)(h); }
}
TEST(vd_findvol_handle_is_not_kernel_handle) {
    REQUIRE(U(FindFirstVolumeA)); REQUIRE(U(FindVolumeClose));
    char a[64]; HANDLE h = U(FindFirstVolumeA)(a, 64); if (h == INVALID_HANDLE_VALUE) { t_skip("none"); return; }
    DWORD fl = 0;
    BOOL ok = GetHandleInformation(h, &fl);     /* a heap block, not a kernel handle */
    CHECK(!ok);
    U(FindVolumeClose)(h);
}
TEST(vd_findvol_lasterror_roundtrip_nothing) {
    REQUIRE(U(FindFirstVolumeA));
    char a[64]; SET_ERR(0);
    HANDLE h = U(FindFirstVolumeA)(a, 64);
    if (h != INVALID_HANDLE_VALUE) U(FindVolumeClose)(h);
    CHECK(h != INVALID_HANDLE_VALUE);
}

/* ---- volume mount point enumeration ---- */
TEST(vd_findmp_null_root_invalid_parameter) {
    REQUIRE(U(FindFirstVolumeMountPointW));
    if (t_is_wine()) { t_skip("not implemented under wine"); return; }
    WCHAR b[MAX_PATH]; SET_ERR(0);
    CHECK(U(FindFirstVolumeMountPointW)(NULL, b, MAX_PATH) == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(vd_findmp_no_trailing_backslash_fails) {
    REQUIRE(U(FindFirstVolumeMountPointW)); REQUIRE(U(FindFirstVolumeMountPointA));
    char s[3] = { vd_sysletter(), ':', 0 }; WCHAR w[3], b[MAX_PATH]; char a[MAX_PATH]; vd_widen(s, w, 3);
    SET_ERR(0);
    CHECK(U(FindFirstVolumeMountPointW)(w, b, MAX_PATH) == INVALID_HANDLE_VALUE);
    CHECK(GetLastError() != 0);
    CHECK(U(FindFirstVolumeMountPointA)(s, a, MAX_PATH) == INVALID_HANDLE_VALUE);
}
TEST(vd_findmp_system_root_enumeration) {
    REQUIRE(U(FindFirstVolumeMountPointA)); REQUIRE(U(FindNextVolumeMountPointA)); REQUIRE(U(FindVolumeMountPointClose));
    if (t_is_wine()) { t_skip("not implemented under wine"); return; }
    char r[4]; vd_sysroot(r); char b[MAX_PATH]; SET_ERR(0);
    HANDLE h = U(FindFirstVolumeMountPointA)(r, b, sizeof b);
    if (h == INVALID_HANDLE_VALUE) {
        DWORD e = GetLastError();
        CHECKF(e == ERROR_NO_MORE_FILES || e == ERROR_ACCESS_DENIED || e == ERROR_PRIVILEGE_NOT_HELD || e == ERROR_INVALID_FUNCTION || e == ERROR_FILE_NOT_FOUND, "err %lu", e);
        t_note("no mount points under %s (err %lu)", r, e); return;
    }
    int n = 0;
    do {
        n++; CHECKF(b[0] != 0 && b[strlen(b) - 1] == '\\', "mount point \"%s\"", b);
    } while (n < 1000 && U(FindNextVolumeMountPointA)(h, b, sizeof b));
    EXPECT_ERR(ERROR_NO_MORE_FILES);
    CHECK(U(FindVolumeMountPointClose)(h));
}
TEST(vd_findmp_nonexistent_drive) {
    REQUIRE(U(FindFirstVolumeMountPointA));
    char l = vd_freeletter(); if (!l) { t_skip("no free letter"); return; }
    char p[4] = { l, ':', '\\', 0 }, b[MAX_PATH]; SET_ERR(0);
    CHECK(U(FindFirstVolumeMountPointA)(p, b, sizeof b) == INVALID_HANDLE_VALUE);
    CHECK(GetLastError() != 0);
}
TEST(vd_findmp_empty_string) {
    REQUIRE(U(FindFirstVolumeMountPointW));
    if (t_is_wine()) { t_skip("empty string handling faults under wine"); return; }
    WCHAR b[MAX_PATH]; SET_ERR(0);
    CHECK(U(FindFirstVolumeMountPointW)(L"", b, MAX_PATH) == INVALID_HANDLE_VALUE);
    CHECK(GetLastError() != 0);
}

/* ---- QueryDosDevice ---- */
static void vd_letter_dev(char *dev) { dev[0] = vd_sysletter(); dev[1] = ':'; dev[2] = 0; }

TEST(vd_qdd_system_drive_target) {
    REQUIRE(U(QueryDosDeviceA));
    char d[3]; vd_letter_dev(d); char t[512]; memset(t, 0x77, sizeof t);
    DWORD n = U(QueryDosDeviceA)(d, t, sizeof t);
    CHECK(n > 0);
    CHECKF(!strncmp(t, "\\Device\\", 8), "target \"%s\"", t);
    t_note("QueryDosDevice(%s) = %s", d, t);
}
TEST(vd_qdd_return_counts_both_nuls) {
    REQUIRE(U(QueryDosDeviceA));
    char d[3]; vd_letter_dev(d); char t[512]; memset(t, 0x77, sizeof t);
    DWORD n = U(QueryDosDeviceA)(d, t, sizeof t);
    DWORD total = 0; for (const char *p = t; *p; p += strlen(p) + 1) total += (DWORD)strlen(p) + 1;
    EXPECT_EQ(n, total + 1);
}
TEST(vd_qdd_w_matches_a) {
    REQUIRE(U(QueryDosDeviceA)); REQUIRE(U(QueryDosDeviceW));
    char d[3]; vd_letter_dev(d); WCHAR wd[3]; vd_widen(d, wd, 3); char t[512]; WCHAR w[512];
    DWORD na = U(QueryDosDeviceA)(d, t, sizeof t), nw = U(QueryDosDeviceW)(wd, w, 512);
    EXPECT_EQ(na, nw);
    char n[512]; vd_narrow(w, n, sizeof n); EXPECT_STR(t, n);
}
TEST(vd_qdd_case_insensitive_name) {
    REQUIRE(U(QueryDosDeviceA));
    char d[3]; vd_letter_dev(d); char l[3] = { (char)tolower(d[0]), ':', 0 }, a[512], b[512];
    CHECK(U(QueryDosDeviceA)(d, a, sizeof a)); CHECK(U(QueryDosDeviceA)(l, b, sizeof b));
    EXPECT_STR(a, b);
}
TEST(vd_qdd_nul_device) {
    REQUIRE(U(QueryDosDeviceA));
    char t[256]; CHECK(U(QueryDosDeviceA)("NUL", t, sizeof t)); EXPECT_EQ(vd_stricmp(t, "\\Device\\Null"), 0);
    CHECK(U(QueryDosDeviceA)("nul", t, sizeof t)); EXPECT_EQ(vd_stricmp(t, "\\Device\\Null"), 0);
}
TEST(vd_qdd_named_devices) {
    REQUIRE(U(QueryDosDeviceA));
    static const struct { const char *n, *t; } tab[] = { { "PIPE", "\\Device\\NamedPipe" }, { "MAILSLOT", "\\Device\\Mailslot" }, { "UNC", "\\Device\\Mup" } };
    for (size_t i = 0; i < ARRAYSIZE(tab); i++) {
        char t[256] = ""; SET_ERR(0);
        DWORD n = U(QueryDosDeviceA)(tab[i].n, t, sizeof t);
        if (n) CHECKF(!vd_stricmp(t, tab[i].t), "%s -> %s", tab[i].n, t); else t_note("%s: err %lu", tab[i].n, GetLastError());
    }
}
TEST(vd_qdd_nonexistent_device) {
    REQUIRE(U(QueryDosDeviceA)); REQUIRE(U(QueryDosDeviceW));
    char t[256]; WCHAR w[256]; memset(t, 0x77, sizeof t);
    SET_ERR(0); EXPECT_EQ(U(QueryDosDeviceA)("K32T_NoSuchDevice", t, sizeof t), 0); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    SET_ERR(0); EXPECT_EQ(U(QueryDosDeviceW)(L"K32T_NoSuchDevice", w, 256), 0); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(vd_qdd_free_drive_letter_not_found) {
    REQUIRE(U(QueryDosDeviceA));
    char l = vd_freeletter(); if (!l) { t_skip("no free letter"); return; }
    char d[3] = { l, ':', 0 }, t[256]; SET_ERR(0);
    EXPECT_EQ(U(QueryDosDeviceA)(d, t, sizeof t), 0); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(vd_qdd_buffer_too_small) {
    REQUIRE(U(QueryDosDeviceA)); REQUIRE(U(QueryDosDeviceW));
    char d[3]; vd_letter_dev(d); WCHAR wd[3]; vd_widen(d, wd, 3); char t[512]; WCHAR w[512];
    DWORD need = U(QueryDosDeviceA)(d, t, sizeof t); CHECK(need > 2);
    for (DWORD n = 1; n <= 8; n++) {
        SET_ERR(0); EXPECT_EQ(U(QueryDosDeviceW)(wd, w, n), 0); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    }
    SET_ERR(0); EXPECT_EQ(U(QueryDosDeviceA)(d, t, 1), 0); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    SET_ERR(0); EXPECT_EQ(U(QueryDosDeviceW)(wd, w, need - 2), 0); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);   /* no room for the terminating NULs */
    SET_ERR(0); CHECK(U(QueryDosDeviceW)(wd, w, need) == need);
}
TEST(vd_qdd_zero_size_null_buffer) {
    REQUIRE(U(QueryDosDeviceW));
    char d[3]; vd_letter_dev(d); WCHAR wd[3]; vd_widen(d, wd, 3);
    SET_ERR(0); EXPECT_EQ(U(QueryDosDeviceW)(wd, NULL, 0), 0); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
}
TEST(vd_qdd_null_name_lists_all) {
    REQUIRE(U(QueryDosDeviceW));
    DWORD cap = 65536; WCHAR *b = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, cap * 2);
    if (!b) { t_skip("alloc"); return; }
    SET_ERR(0);
    DWORD n = U(QueryDosDeviceW)(NULL, b, cap);
    CHECKF(n > 2, "n=%lu err=%lu", n, GetLastError());
    int found_c = 0, found_nul = 0, found_pipe = 0; char want[3]; vd_letter_dev(want); WCHAR ww[3]; vd_widen(want, ww, 3);
    DWORD total = 0;
    for (const WCHAR *p = b; *p && total < n; p += lstrlenW(p) + 1) {
        total += lstrlenW(p) + 1;
        if (!lstrcmpiW(p, ww)) found_c = 1;
        if (!lstrcmpiW(p, L"NUL")) found_nul = 1;
        if (!lstrcmpiW(p, L"PIPE")) found_pipe = 1;
    }
    CHECK(found_c); CHECK(found_nul); CHECK(found_pipe);
    EXPECT_EQ(total + 1, n);
    HeapFree(GetProcessHeap(), 0, b);
}
TEST(vd_qdd_null_name_a_matches_w_count) {
    REQUIRE(U(QueryDosDeviceA)); REQUIRE(U(QueryDosDeviceW));
    DWORD cap = 65536; WCHAR *w = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, cap * 2); char *a = (char *)HeapAlloc(GetProcessHeap(), 0, cap);
    if (!w || !a) { t_skip("alloc"); return; }
    DWORD nw = U(QueryDosDeviceW)(NULL, w, cap), na = U(QueryDosDeviceA)(NULL, a, cap);
    CHECK(nw > 0 && na > 0);
    if (nw != na) t_note("device list size changed or differs: w=%lu a=%lu", nw, na);
    HeapFree(GetProcessHeap(), 0, w); HeapFree(GetProcessHeap(), 0, a);
}
TEST(vd_qdd_null_name_small_buffer) {
    REQUIRE(U(QueryDosDeviceW));
    WCHAR b[16]; SET_ERR(0);
    EXPECT_EQ(U(QueryDosDeviceW)(NULL, b, 16), 0);
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
}
TEST(vd_qdd_volume_guid_matches_letter) {
    REQUIRE(U(QueryDosDeviceA));
    char r[4]; vd_sysroot(r); char g[64]; if (!vd_volname(r, g)) { t_skip("no volume name"); return; }
    char d[3]; vd_letter_dev(d); char name[64]; strcpy(name, g + 4); name[strlen(name) - 1] = 0;   /* Volume{GUID} */
    char a[512], b[512];
    CHECK(U(QueryDosDeviceA)(d, a, sizeof a));
    if (U(QueryDosDeviceA)(name, b, sizeof b)) EXPECT_STR(a, b); else t_note("QueryDosDevice(%s) failed %lu", name, GetLastError());
}
TEST(vd_qdd_trailing_backslash_name) {
    REQUIRE(U(QueryDosDeviceA));
    char d[4] = { vd_sysletter(), ':', '\\', 0 }, t[256]; SET_ERR(0);
    DWORD n = U(QueryDosDeviceA)(d, t, sizeof t);
    t_note("QueryDosDevice(\"%s\") = %lu err %lu", d, n, GetLastError());
}
TEST(vd_qdd_empty_name) {
    REQUIRE(U(QueryDosDeviceA));
    char t[256]; SET_ERR(0);
    DWORD n = U(QueryDosDeviceA)("", t, sizeof t);
    t_note("QueryDosDevice(\"\") = %lu err %lu", n, GetLastError());
}
TEST(vd_qdd_physicaldrive_note) {
    REQUIRE(U(QueryDosDeviceA));
    char t[256]; SET_ERR(0);
    DWORD n = U(QueryDosDeviceA)("PhysicalDrive0", t, sizeof t);
    if (n) { CHECK(!strncmp(t, "\\Device\\Harddisk", 15)); t_note("PhysicalDrive0 -> %s", t); } else t_note("PhysicalDrive0 err %lu", GetLastError());
}
TEST(vd_qdd_all_logical_drives_resolve) {
    REQUIRE(U(QueryDosDeviceA)); REQUIRE(U(GetLogicalDrives));
    DWORD m = U(GetLogicalDrives)(); int bad = 0;
    for (int i = 0; i < 26; i++) {
        if (!((m >> i) & 1)) continue;
        char d[3] = { (char)('A' + i), ':', 0 }, t[1024];
        if (!U(QueryDosDeviceA)(d, t, sizeof t)) { bad++; t_note("%s unresolved err %lu", d, GetLastError()); }
    }
    CHECK(bad == 0);
}
TEST(vd_qdd_repeated_stable) {
    REQUIRE(U(QueryDosDeviceA));
    char d[3]; vd_letter_dev(d); char a[512], b[512]; CHECK(U(QueryDosDeviceA)(d, a, sizeof a));
    for (int i = 0; i < 100; i++) { U(QueryDosDeviceA)(d, b, sizeof b); if (strcmp(a, b)) { CHECK(0); break; } }
}
TEST(vd_qdd_target_openable_as_nt_path_exists_in_list) {
    REQUIRE(U(QueryDosDeviceA)); REQUIRE(U(GetVolumeNameForVolumeMountPointA));
    char r[4]; vd_sysroot(r); char d[3]; vd_letter_dev(d); char t[512];
    CHECK(U(QueryDosDeviceA)(d, t, sizeof t));
    CHECK(strlen(t) > 8);
}
