/* Additional volume/mount point checks (read-only; mutators only on paths that fail first). */
#include "vd.h"

TEST(vd_extra_findmp_w_enumeration_matches_a) {
    REQUIRE(U(FindFirstVolumeMountPointW)); REQUIRE(U(FindNextVolumeMountPointW));
    REQUIRE(U(FindFirstVolumeMountPointA)); REQUIRE(U(FindNextVolumeMountPointA)); REQUIRE(U(FindVolumeMountPointClose));
    if (t_is_wine()) { t_skip("not implemented under wine"); return; }
    char r[4]; vd_sysroot(r); WCHAR w[4]; vd_widen(r, w, 4);
    WCHAR bw[MAX_PATH]; char ba[MAX_PATH]; int nw = 0, na = 0;
    SET_ERR(0);
    HANDLE h = U(FindFirstVolumeMountPointW)(w, bw, MAX_PATH);
    if (h == INVALID_HANDLE_VALUE) { t_note("no W mount points under %s (err %lu)", r, GetLastError()); return; }
    do { nw++; } while (nw < 1000 && U(FindNextVolumeMountPointW)(h, bw, MAX_PATH));
    EXPECT_ERR(ERROR_NO_MORE_FILES);
    CHECK(U(FindVolumeMountPointClose)(h));
    h = U(FindFirstVolumeMountPointA)(r, ba, sizeof ba);
    if (h != INVALID_HANDLE_VALUE) {
        do { na++; } while (na < 1000 && U(FindNextVolumeMountPointA)(h, ba, sizeof ba));
        CHECK(U(FindVolumeMountPointClose)(h));
    }
    EXPECT_EQ(nw, na);
}

TEST(vd_extra_setmountpoint_a_existing_rejected) {
    REQUIRE(U(SetVolumeMountPointA)); REQUIRE(U(GetVolumeNameForVolumeMountPointA));
    if (t_is_wine()) { t_skip("SetVolumeMountPoint not safe under wine"); return; }
    char r[4]; vd_sysroot(r); char g[64];
    /* only call when the root is already a mount point: the call then fails with
       ERROR_DIR_NOT_EMPTY before changing anything (reference analysis) */
    if (!U(GetVolumeNameForVolumeMountPointA)(r, g, 64)) { t_skip("precondition: %s is not a mount point", r); return; }
    SET_ERR(0);
    CHECK(!U(SetVolumeMountPointA)(r, g));
    EXPECT_ERR(ERROR_DIR_NOT_EMPTY);
}

TEST(vd_extra_volume_guid_roundtrip_every_fixed_drive) {
    REQUIRE(U(GetLogicalDriveStringsW)); REQUIRE(U(GetVolumeNameForVolumeMountPointW)); REQUIRE(U(GetVolumeInformationW));
    WCHAR all[256]; DWORD n = U(GetLogicalDriveStringsW)(256, all);
    CHECK(n > 0 && n < 256);
    for (const WCHAR *p = all; *p; p += lstrlenW(p) + 1) {
        if (GetDriveTypeW(p) != DRIVE_FIXED) continue;
        WCHAR g[64]; DWORD ser1 = 0, ser2 = 0;
        if (!U(GetVolumeNameForVolumeMountPointW)(p, g, 64)) continue;
        CHECK(lstrlenW(g) == VD_GUID_LEN);
        if (U(GetVolumeInformationW)(p, NULL, 0, &ser1, NULL, NULL, NULL, 0) &&
            U(GetVolumeInformationW)(g, NULL, 0, &ser2, NULL, NULL, NULL, 0))
            CHECK(ser1 == ser2);
    }
}

TEST(vd_extra_volumeinfo_serial_stable) {
    REQUIRE(U(GetVolumeInformationA));
    char r[4]; vd_sysroot(r); DWORD a = 1, b = 2;
    CHECK(U(GetVolumeInformationA)(r, NULL, 0, &a, NULL, NULL, NULL, 0));
    CHECK(U(GetVolumeInformationA)(r, NULL, 0, &b, NULL, NULL, NULL, 0));
    EXPECT_EQ(a, b);
}
