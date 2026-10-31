/* GetBinaryType, volume IOCTLs, argument-validation of mutating volume APIs, error-mode interplay */
#include "vd.h"

/* ---- GetBinaryType ---- */
TEST(vd_binarytype_own_exe_64bit) {
    REQUIRE(U(GetBinaryTypeA)); REQUIRE(U(GetBinaryTypeW));
    DWORD t = 0xFFFF;
    CHECK(U(GetBinaryTypeA)(t_self_path(), &t));
    EXPECT_EQ(t, SCS_64BIT_BINARY);
    WCHAR w[MAX_PATH]; vd_widen(t_self_path(), w, MAX_PATH); t = 0xFFFF;
    CHECK(U(GetBinaryTypeW)(w, &t));
    EXPECT_EQ(t, SCS_64BIT_BINARY);
}
TEST(vd_binarytype_system_exes_64bit) {
    REQUIRE(U(GetBinaryTypeA));
    char d[MAX_PATH], p[MAX_PATH]; GetSystemDirectoryA(d, sizeof d);
    static const char *const n[] = { "cmd.exe", "notepad.exe", "ping.exe" };
    for (size_t i = 0; i < ARRAYSIZE(n); i++) {
        sprintf(p, "%s\\%s", d, n[i]); if (GetFileAttributesA(p) == INVALID_FILE_ATTRIBUTES) continue;
        DWORD t = 0xFFFF; BOOL ok = U(GetBinaryTypeA)(p, &t);
        CHECKF(ok, "%s err %lu", p, GetLastError()); if (ok) CHECKF(t == SCS_64BIT_BINARY, "%s type %lu", p, t);
    }
}
TEST(vd_binarytype_syswow64_exe_32bit) {
    REQUIRE(U(GetBinaryTypeA));
    char d[MAX_PATH], p[MAX_PATH];
    if (!GetSystemWow64DirectoryA(d, sizeof d)) { t_skip("no SysWOW64"); return; }
    sprintf(p, "%s\\cmd.exe", d);
    if (GetFileAttributesA(p) == INVALID_FILE_ATTRIBUTES) { t_skip("no 32-bit cmd.exe"); return; }
    DWORD t = 0xFFFF;
    CHECK(U(GetBinaryTypeA)(p, &t)); EXPECT_EQ(t, SCS_32BIT_BINARY);
}
TEST(vd_binarytype_nonexistent_file) {
    REQUIRE(U(GetBinaryTypeA)); REQUIRE(U(GetBinaryTypeW));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "no_such_binary_k32t.exe"); DWORD t = 0xFFFF; WCHAR w[MAX_PATH]; vd_widen(p, w, MAX_PATH);
    SET_ERR(0); CHECK(!U(GetBinaryTypeA)(p, &t)); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    SET_ERR(0); CHECK(!U(GetBinaryTypeW)(w, &t)); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    EXPECT_EQ(t, 0xFFFF);
}
TEST(vd_binarytype_nonexistent_directory) {
    REQUIRE(U(GetBinaryTypeA));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "no_dir_k32t\\x.exe"); DWORD t = 0xFFFF;
    SET_ERR(0); CHECK(!U(GetBinaryTypeA)(p, &t)); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(vd_binarytype_text_file_bad_format) {
    REQUIRE(U(GetBinaryTypeA));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "vd_text.txt");
    HANDLE h = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); DWORD n;
    WriteFile(h, "this is not an executable image, only text\r\n", 43, &n, NULL); CloseHandle(h);
    DWORD t = 0xFFFF; SET_ERR(0);
    CHECK(!U(GetBinaryTypeA)(p, &t));
    EXPECT_ERR(ERROR_BAD_EXE_FORMAT); EXPECT_EQ(t, 0xFFFF);
    DeleteFileA(p);
}
TEST(vd_binarytype_empty_file) {
    REQUIRE(U(GetBinaryTypeA));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "vd_empty.bin");
    HANDLE h = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); CloseHandle(h);
    DWORD t = 0xFFFF; SET_ERR(0);
    CHECK(!U(GetBinaryTypeA)(p, &t));
    t_note("empty file error %lu", GetLastError());
    DeleteFileA(p);
}
TEST(vd_binarytype_dll_rejected) {
    REQUIRE(U(GetBinaryTypeA));
    char p[MAX_PATH]; GetSystemDirectoryA(p, sizeof p); strcat(p, "\\kernel32.dll"); DWORD t = 0xFFFF; SET_ERR(0);
    BOOL ok = U(GetBinaryTypeA)(p, &t);
    CHECK(!ok); EXPECT_ERR(ERROR_BAD_EXE_FORMAT);
}
TEST(vd_binarytype_directory) {
    REQUIRE(U(GetBinaryTypeA));
    DWORD t = 0xFFFF; SET_ERR(0);
    char d[MAX_PATH]; strcpy(d, t_tmpdir()); d[strlen(d) - 1] = 0;
    CHECK(!U(GetBinaryTypeA)(d, &t));
    EXPECT_ERR(ERROR_ACCESS_DENIED);
}
TEST(vd_binarytype_exe_named_text_note) {
    REQUIRE(U(GetBinaryTypeA));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "vd_fake.exe");
    HANDLE h = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); DWORD n;
    WriteFile(h, "not an image\r\n", 14, &n, NULL); CloseHandle(h);
    DWORD t = 0xFFFF; SET_ERR(0);
    BOOL ok = U(GetBinaryTypeA)(p, &t);
    t_note("text .exe: ok=%d type=%lu err=%lu", ok, t, GetLastError());
    DeleteFileA(p);
}
TEST(vd_binarytype_copy_of_exe_in_tmp) {
    REQUIRE(U(GetBinaryTypeA));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "vd_copy.exe");
    if (!CopyFileA(t_self_path(), p, FALSE)) { t_skip("copy failed"); return; }
    DWORD t = 0xFFFF; CHECK(U(GetBinaryTypeA)(p, &t)); EXPECT_EQ(t, SCS_64BIT_BINARY);
    DeleteFileA(p);
}
TEST(vd_binarytype_relative_name_not_searched_on_path) {
    REQUIRE(U(GetBinaryTypeA));
    DWORD t = 0xFFFF; SET_ERR(0);
    BOOL ok = U(GetBinaryTypeA)("cmd", &t);       /* no extension, no PATH search: relative to cwd */
    if (ok) t_note("cmd resolved in cwd?"); else CHECK(GetLastError() != 0);
}
TEST(vd_binarytype_extended_prefix_path) {
    REQUIRE(U(GetBinaryTypeW));
    WCHAR w[MAX_PATH + 8] = L"\\\\?\\"; vd_widen(t_self_path(), w + 4, MAX_PATH); DWORD t = 0xFFFF;
    CHECK(U(GetBinaryTypeW)(w, &t)); EXPECT_EQ(t, SCS_64BIT_BINARY);
}
TEST(vd_binarytype_file_held_open_shared) {
    REQUIRE(U(GetBinaryTypeA));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "vd_busy.exe");
    if (!CopyFileA(t_self_path(), p, FALSE)) { t_skip("copy"); return; }
    HANDLE h = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    DWORD t = 0xFFFF; CHECK(U(GetBinaryTypeA)(p, &t)); EXPECT_EQ(t, SCS_64BIT_BINARY);
    CloseHandle(h); DeleteFileA(p);
}

/* ---- volume IOCTLs on zero-access handles ---- */
#define VD_IOCTL_STORAGE_GET_DEVICE_NUMBER 0x002D1080
#define VD_FSCTL_IS_VOLUME_MOUNTED 0x00090028
#define VD_IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS 0x00560000
#define VD_IOCTL_DISK_GET_DRIVE_GEOMETRY 0x00070000
#define VD_IOCTL_DISK_GET_LENGTH_INFO 0x0007405C

static HANDLE vd_open_volume(char letter) {
    char p[8]; sprintf(p, "\\\\.\\%c:", letter);
    return CreateFileA(p, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
}
TEST(vd_ioctl_volume_open_zero_access) {
    HANDLE h = vd_open_volume(vd_sysletter());
    if (h == INVALID_HANDLE_VALUE) { t_skip("cannot open volume with 0 access: %lu", GetLastError()); return; }
    CHECK(GetFileType(h) == FILE_TYPE_DISK);
    CloseHandle(h);
}
TEST(vd_ioctl_is_volume_mounted) {
    REQUIRE(U(DeviceIoControl));
    HANDLE h = vd_open_volume(vd_sysletter()); if (h == INVALID_HANDLE_VALUE) { t_skip("open %lu", GetLastError()); return; }
    DWORD ret = 0xFFFF; SET_ERR(0);
    BOOL ok = U(DeviceIoControl)(h, VD_FSCTL_IS_VOLUME_MOUNTED, NULL, 0, NULL, 0, &ret, NULL);
    CHECKF(ok, "err %lu", GetLastError()); EXPECT_EQ(ret, 0);
    CloseHandle(h);
}
TEST(vd_ioctl_storage_device_number) {
    REQUIRE(U(DeviceIoControl));
    HANDLE h = vd_open_volume(vd_sysletter()); if (h == INVALID_HANDLE_VALUE) { t_skip("open %lu", GetLastError()); return; }
    struct { DWORD DeviceType, DeviceNumber, PartitionNumber; DWORD pad; } s; memset(&s, 0xCC, sizeof s); DWORD ret = 0;
    BOOL ok = U(DeviceIoControl)(h, VD_IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0, &s, 12, &ret, NULL);
    if (!ok) { t_skip("IOCTL failed %lu", GetLastError()); CloseHandle(h); return; }
    EXPECT_EQ(ret, 12); CHECK(s.pad == 0xCCCCCCCC);
    CHECK(s.DeviceType == 7 /* FILE_DEVICE_DISK */ || s.DeviceType == 2 /* CD-ROM */ || s.DeviceType == 0x2d);
    CHECK(s.PartitionNumber < 128 || s.PartitionNumber == 0xFFFFFFFF);
    CloseHandle(h);
}
TEST(vd_ioctl_storage_device_number_small_output) {
    REQUIRE(U(DeviceIoControl));
    HANDLE h = vd_open_volume(vd_sysletter()); if (h == INVALID_HANDLE_VALUE) { t_skip("open %lu", GetLastError()); return; }
    DWORD buf[3] = { 0 }, ret = 0; SET_ERR(0);
    BOOL ok = U(DeviceIoControl)(h, VD_IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0, buf, 4, &ret, NULL);
    CHECK(!ok); t_note("small output error %lu", GetLastError());
    CloseHandle(h);
}
TEST(vd_ioctl_disk_extents) {
    REQUIRE(U(DeviceIoControl));
    HANDLE h = vd_open_volume(vd_sysletter()); if (h == INVALID_HANDLE_VALUE) { t_skip("open %lu", GetLastError()); return; }
    BYTE buf[256]; DWORD ret = 0; SET_ERR(0);
    BOOL ok = U(DeviceIoControl)(h, VD_IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0, buf, sizeof buf, &ret, NULL);
    if (!ok) { t_skip("extents IOCTL failed %lu", GetLastError()); CloseHandle(h); return; }
    DWORD n = *(DWORD *)buf; CHECK(n >= 1 && n < 16); CHECK(ret >= 8 + 24 * n);
    ULONGLONG len = *(ULONGLONG *)(buf + 8 + 8 + 8 + 0 + 0); (void)len;
    SET_ERR(0);
    ok = U(DeviceIoControl)(h, VD_IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0, buf, 4, &ret, NULL);
    CHECK(!ok); EXPECT_ERR(ERROR_MORE_DATA);
    CloseHandle(h);
}
TEST(vd_ioctl_get_length_needs_read_access) {
    REQUIRE(U(DeviceIoControl));
    HANDLE h = vd_open_volume(vd_sysletter()); if (h == INVALID_HANDLE_VALUE) { t_skip("open %lu", GetLastError()); return; }
    ULONGLONG len = 0; DWORD ret = 0; SET_ERR(0);
    BOOL ok = U(DeviceIoControl)(h, VD_IOCTL_DISK_GET_LENGTH_INFO, NULL, 0, &len, 8, &ret, NULL);
    t_note("GET_LENGTH_INFO with no access: ok=%d err=%lu", ok, GetLastError());
    if (!ok) CHECK(GetLastError() == ERROR_ACCESS_DENIED);
    CloseHandle(h);
}
TEST(vd_ioctl_volume_matches_getdiskfreeex_via_geometry_note) {
    REQUIRE(U(DeviceIoControl));
    HANDLE h = vd_open_volume(vd_sysletter()); if (h == INVALID_HANDLE_VALUE) { t_skip("open %lu", GetLastError()); return; }
    BYTE g[64]; DWORD ret = 0; SET_ERR(0);
    BOOL ok = U(DeviceIoControl)(h, VD_IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0, g, sizeof g, &ret, NULL);
    t_note("GET_DRIVE_GEOMETRY ok=%d err=%lu ret=%lu", ok, GetLastError(), ret);
    CloseHandle(h);
}
TEST(vd_ioctl_guid_volume_equals_letter_device) {
    REQUIRE(U(DeviceIoControl));
    char r[4]; vd_sysroot(r); char g[64]; if (!vd_volname(r, g)) { t_skip("no volume name"); return; }
    g[strlen(g) - 1] = 0;     /* drop trailing backslash to open the volume device */
    HANDLE hg = CreateFileA(g, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    HANDLE hl = vd_open_volume(r[0]);
    if (hg == INVALID_HANDLE_VALUE || hl == INVALID_HANDLE_VALUE) { t_skip("open failed %lu", GetLastError()); if (hg != INVALID_HANDLE_VALUE) CloseHandle(hg); if (hl != INVALID_HANDLE_VALUE) CloseHandle(hl); return; }
    DWORD a[3] = {0}, b[3] = {0}, ret;
    if (U(DeviceIoControl)(hg, VD_IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0, a, 12, &ret, NULL) && U(DeviceIoControl)(hl, VD_IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0, b, 12, &ret, NULL))
        CHECK(!memcmp(a, b, 12));
    else t_note("device number IOCTL unavailable");
    CloseHandle(hg); CloseHandle(hl);
}
TEST(vd_ioctl_physicaldrive0_zero_access) {
    REQUIRE(U(DeviceIoControl));
    HANDLE h = CreateFileA("\\\\.\\PhysicalDrive0", 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_skip("PhysicalDrive0 open: %lu", GetLastError()); return; }
    DWORD s[3] = {0}, ret = 0;
    if (U(DeviceIoControl)(h, VD_IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0, s, 12, &ret, NULL)) { EXPECT_EQ(s[1], 0); EXPECT_EQ(s[0], 7); }
    else t_note("device number on PhysicalDrive0: %lu", GetLastError());
    CloseHandle(h);
}
TEST(vd_ioctl_nonexistent_physicaldrive) {
    HANDLE h = CreateFileA("\\\\.\\PhysicalDrive999", 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h == INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
}
TEST(vd_ioctl_invalid_handle) {
    REQUIRE(U(DeviceIoControl));
    DWORD ret = 0; SET_ERR(0);
    CHECK(!U(DeviceIoControl)(INVALID_HANDLE_VALUE, VD_FSCTL_IS_VOLUME_MOUNTED, NULL, 0, NULL, 0, &ret, NULL));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}

/* ---- mutating APIs: only argument errors that fail before any change ---- */
/* If a call that must be rejected is unexpectedly accepted, undo our own K32T: definition. */
static void vd_ddd_undo(BOOL accepted) { if (accepted) { U(DefineDosDeviceW)(DDD_REMOVE_DEFINITION | DDD_NO_BROADCAST_SYSTEM, L"K32T:", NULL); } }
#define VD_DDD_REJECT_W(flags, dev, tgt) do { SET_ERR(0); BOOL r_ = U(DefineDosDeviceW)(flags, dev, tgt); DWORD e_ = GetLastError(); vd_ddd_undo(r_); \
    t_check(!r_, __FILE__, __LINE__, "DefineDosDeviceW rejected", "flags %08lx", (DWORD)(flags)); t_check(e_ == ERROR_INVALID_PARAMETER, __FILE__, __LINE__, "ERROR_INVALID_PARAMETER", "got %lu", e_); } while (0)
#define VD_DDD_REJECT_A(flags, dev, tgt) do { SET_ERR(0); BOOL r_ = U(DefineDosDeviceA)(flags, dev, tgt); DWORD e_ = GetLastError(); vd_ddd_undo(r_); \
    t_check(!r_, __FILE__, __LINE__, "DefineDosDeviceA rejected", "flags %08lx", (DWORD)(flags)); t_check(e_ == ERROR_INVALID_PARAMETER, __FILE__, __LINE__, "ERROR_INVALID_PARAMETER", "got %lu", e_); } while (0)
TEST(vd_defdosdev_invalid_flags_w) {
    REQUIRE(U(DefineDosDeviceW));
    if (t_is_wine()) { t_skip("flags not validated under wine"); return; }
    static const DWORD bad[] = { 0x20, 0x40, 0x100, 0x80000000, 0xFFFFFFE0, 0xFFFFFFFF };
    for (size_t i = 0; i < ARRAYSIZE(bad); i++) VD_DDD_REJECT_W(bad[i], L"K32T:", L"C:\\");
}
TEST(vd_defdosdev_exact_match_without_remove) {
    REQUIRE(U(DefineDosDeviceW)); REQUIRE(U(DefineDosDeviceA));
    if (t_is_wine()) { t_skip("flags not validated under wine"); return; }
    VD_DDD_REJECT_W(DDD_EXACT_MATCH_ON_REMOVE, L"K32T:", L"C:\\");
    VD_DDD_REJECT_A(DDD_EXACT_MATCH_ON_REMOVE, "K32T:", "C:\\");
    VD_DDD_REJECT_W(DDD_EXACT_MATCH_ON_REMOVE | DDD_RAW_TARGET_PATH, L"K32T:", L"\\Device\\Null");
}
TEST(vd_defdosdev_null_target_without_remove) {
    REQUIRE(U(DefineDosDeviceW)); REQUIRE(U(DefineDosDeviceA));
    if (t_is_wine()) { t_skip("flags not validated under wine"); return; }
    VD_DDD_REJECT_W(0, L"K32T:", NULL);
    VD_DDD_REJECT_W(DDD_RAW_TARGET_PATH, L"K32T:", NULL);
    VD_DDD_REJECT_W(DDD_NO_BROADCAST_SYSTEM, L"K32T:", NULL);
    VD_DDD_REJECT_A(0, "K32T:", NULL);
}
TEST(vd_defdosdev_invalid_flags_a) {
    REQUIRE(U(DefineDosDeviceA));
    if (t_is_wine()) { t_skip("flags not validated under wine"); return; }
    VD_DDD_REJECT_A(0x20, "K32T:", "C:\\");
    VD_DDD_REJECT_A(0xFFFFFFFF, "K32T:", "C:\\");
}
TEST(vd_delmountpoint_null_and_no_backslash) {
    REQUIRE(U(DeleteVolumeMountPointW)); REQUIRE(U(DeleteVolumeMountPointA));
    if (t_is_wine()) { t_skip("DeleteVolumeMountPoint not safe under wine"); return; }
    char s[3] = { vd_sysletter(), ':', 0 };
    SET_ERR(0); CHECK(!U(DeleteVolumeMountPointW)(NULL)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!U(DeleteVolumeMountPointA)(s)); EXPECT_ERR(VD_ERR_INVALID_NAME);
    WCHAR w[3]; vd_widen(s, w, 3);
    SET_ERR(0); CHECK(!U(DeleteVolumeMountPointW)(w)); EXPECT_ERR(VD_ERR_INVALID_NAME);
    SET_ERR(0); CHECK(!U(DeleteVolumeMountPointW)(L"relative_k32t")); EXPECT_ERR(VD_ERR_INVALID_NAME);
}
TEST(vd_setmountpoint_existing_mountpoint_rejected) {
    REQUIRE(U(SetVolumeMountPointW)); REQUIRE(U(GetVolumeNameForVolumeMountPointW));
    if (t_is_wine()) { t_skip("SetVolumeMountPoint not safe under wine"); return; }
    char r[4]; vd_sysroot(r); WCHAR w[4], g[64]; vd_widen(r, w, 4);
    if (!U(GetVolumeNameForVolumeMountPointW)(w, g, 64)) { t_skip("precondition: %s is not a mount point", r); return; }
    SET_ERR(0);
    CHECK(!U(SetVolumeMountPointW)(w, g));
    EXPECT_ERR(ERROR_DIR_NOT_EMPTY);   /* the mount point already exists; nothing is changed */
}
TEST(vd_setmountpoint_null_arguments) {
    REQUIRE(U(SetVolumeMountPointW));
    if (t_is_wine()) { t_skip("not safe under wine"); return; }
    SET_ERR(0); CHECK(!U(SetVolumeMountPointW)(NULL, NULL)); CHECK(GetLastError() != 0);
    SET_ERR(0); CHECK(!U(SetVolumeMountPointW)(NULL, L"\\\\?\\Volume{00000000-0000-0000-0000-000000000000}\\")); CHECK(GetLastError() != 0);
}
TEST(vd_setvollabel_unmapped_drive_fails) {
    REQUIRE(U(SetVolumeLabelW)); REQUIRE(U(SetVolumeLabelA)); REQUIRE(U(GetDriveTypeA));
    if (t_is_wine()) { t_skip("not safe under wine"); return; }
    char l = vd_freeletter(); if (!l) { t_skip("no free letter"); return; }
    char root[4] = { l, ':', '\\', 0 }; WCHAR w[4]; vd_widen(root, w, 4);
    if (U(GetDriveTypeA)(root) != DRIVE_NO_ROOT_DIR) { t_skip("letter became used"); return; }
    SET_ERR(0); CHECK(!U(SetVolumeLabelW)(w, L"K32T")); CHECK(GetLastError() != 0);
    SET_ERR(0); CHECK(!U(SetVolumeLabelA)(root, "K32T")); CHECK(GetLastError() != 0);
}

/* ---- error mode interplay, in a child ---- */
CHILD(vd_errormode) {
    __typeof__(&SetErrorMode) sem = U(SetErrorMode); __typeof__(&GetErrorMode) gem = U(GetErrorMode);
    __typeof__(&GetVolumeInformationA) gvi = U(GetVolumeInformationA); __typeof__(&GetDiskFreeSpaceA) gdf = U(GetDiskFreeSpaceA);
    __typeof__(&GetDriveTypeA) gdt = U(GetDriveTypeA);
    if (!sem || !gvi || !gdf || !gdt) return 0;      /* nothing to test */
    sem(0);
    UINT old = sem(SEM_FAILCRITICALERRORS);
    if (old != 0) return 10;
    if (gem && gem() != SEM_FAILCRITICALERRORS) return 11;
    DWORD m = GetLogicalDrives(); int examined = 0;
    for (int i = 0; i < 26; i++) {
        if (!((m >> i) & 1)) continue;
        char r[4] = { (char)('A' + i), ':', '\\', 0 };
        UINT t = gdt(r);
        if (t != DRIVE_REMOVABLE && t != DRIVE_CDROM) continue;      /* only drives that can be empty */
        char fs[64]; DWORD ser; DWORD a, b, c, d;
        SetLastError(0);
        BOOL ok = gvi(r, NULL, 0, &ser, NULL, NULL, fs, sizeof fs);
        if (!ok && GetLastError() == 0) return 20;
        SetLastError(0);
        ok = gdf(r, &a, &b, &c, &d);
        if (!ok && GetLastError() == 0) return 21;
        examined++;
    }
    /* a missing drive must also fail quietly */
    char l = 0; for (char ch = 'Z'; ch >= 'D'; ch--) if (!((m >> (ch - 'A')) & 1)) { l = ch; break; }
    if (l) { char r[4] = { l, ':', '\\', 0 }; DWORD ser; SetLastError(0); if (gvi(r, NULL, 0, &ser, NULL, NULL, NULL, 0)) return 30; if (!GetLastError()) return 31; }
    if (sem(0) != SEM_FAILCRITICALERRORS) return 12;
    return 0;
}
TEST(vd_errormode_failcriticalerrors_empty_drives) {
    REQUIRE(U(SetErrorMode));
    DWORD c = t_run_child("vd_errormode", "", 25000);
    CHECKF(c == 0, "child exit %lu", c);
}
TEST(vd_errormode_harness_default) {
    REQUIRE(U(GetErrorMode)); REQUIRE(U(SetErrorMode));
    UINT cur = U(GetErrorMode)();
    CHECK(cur & SEM_FAILCRITICALERRORS);
    UINT prev = U(SetErrorMode)(cur);
    EXPECT_EQ(prev, cur);
    EXPECT_EQ(U(GetErrorMode)(), cur);
}

/* ---- file system consistency across the temp volume, system drive and current directory ---- */
TEST(vd_consistency_system_directory_volume) {
    REQUIRE(U(GetVolumePathNameA)); REQUIRE(U(GetVolumeInformationA)); REQUIRE(U(GetDriveTypeA));
    char d[MAX_PATH], root[MAX_PATH]; GetSystemDirectoryA(d, sizeof d);
    CHECK(U(GetVolumePathNameA)(d, root, sizeof root));
    char fs[64]; DWORD flags = 0, mc = 0;
    CHECK(U(GetVolumeInformationA)(root, NULL, 0, NULL, &mc, &flags, fs, sizeof fs));
    EXPECT_EQ(U(GetDriveTypeA)(root), DRIVE_FIXED);
    if (!strcmp(fs, "NTFS")) CHECK(flags & FILE_PERSISTENT_ACLS);
}
TEST(vd_consistency_tmp_volume_chain) {
    REQUIRE(U(GetVolumePathNameA)); REQUIRE(U(GetVolumeInformationA)); REQUIRE(U(GetVolumeNameForVolumeMountPointA)); REQUIRE(U(GetDiskFreeSpaceExA));
    char root[MAX_PATH], g[64]; CHECK(U(GetVolumePathNameA)(t_tmpdir(), root, sizeof root));
    char fs[64]; CHECK(U(GetVolumeInformationA)(root, NULL, 0, NULL, NULL, NULL, fs, sizeof fs));
    ULARGE_INTEGER a, t, f; CHECK(U(GetDiskFreeSpaceExA)(root, &a, &t, &f)); CHECK(t.QuadPart > 0);
    if (U(GetVolumeNameForVolumeMountPointA)(root, g, 64)) {
        char fs2[64]; CHECK(U(GetVolumeInformationA)(g, NULL, 0, NULL, NULL, NULL, fs2, sizeof fs2)); EXPECT_STR(fs, fs2);
        ULARGE_INTEGER t2; CHECK(U(GetDiskFreeSpaceExA)(g, &a, &t2, &f)); EXPECT_EQ(t.QuadPart, t2.QuadPart);
    } else t_note("tmp root %s has no volume name: %lu", root, GetLastError());
}
TEST(vd_consistency_logical_drive_strings_vs_getvolumepathnames) {
    REQUIRE(U(GetLogicalDriveStringsA)); REQUIRE(U(GetVolumeNameForVolumeMountPointA)); REQUIRE(U(GetVolumePathNamesForVolumeNameA));
    char all[200]; DWORD n = U(GetLogicalDriveStringsA)(sizeof all, all); (void)n;
    for (const char *p = all; *p; p += strlen(p) + 1) {
        UINT t = GetDriveTypeA(p); if (t != DRIVE_FIXED) continue;
        char g[64], names[1024]; DWORD ret;
        if (!U(GetVolumeNameForVolumeMountPointA)(p, g, 64)) continue;
        CHECK(U(GetVolumePathNamesForVolumeNameA)(g, names, sizeof names, &ret));
        int f = 0; for (const char *q = names; *q; q += strlen(q) + 1) if (!vd_stricmp(q, p)) f = 1;
        CHECKF(f, "%s absent from path names of %s", p, g);
    }
}
TEST(vd_consistency_getvolumepathname_of_every_drive_string) {
    REQUIRE(U(GetLogicalDriveStringsA)); REQUIRE(U(GetVolumePathNameA));
    char all[200]; U(GetLogicalDriveStringsA)(sizeof all, all);
    for (const char *p = all; *p; p += strlen(p) + 1) {
        if (GetDriveTypeA(p) != DRIVE_FIXED) continue;
        char o[MAX_PATH]; CHECK(U(GetVolumePathNameA)(p, o, sizeof o)); CHECKF(!vd_stricmp(o, p), "%s -> %s", p, o);
    }
}
