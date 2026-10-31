/* GetLogicalDrives / GetLogicalDriveStrings / GetDriveType */
#include "vd.h"

/* ---- GetLogicalDrives ---- */
TEST(vd_logicaldrives_nonzero) {
    REQUIRE(U(GetLogicalDrives));
    DWORD m = U(GetLogicalDrives)();
    CHECK(m != 0);
}
TEST(vd_logicaldrives_system_drive_bit) {
    REQUIRE(U(GetLogicalDrives));
    DWORD m = U(GetLogicalDrives)();
    CHECK((m >> (vd_sysletter() - 'A')) & 1);
}
TEST(vd_logicaldrives_tmp_drive_bit) {
    REQUIRE(U(GetLogicalDrives));
    char l = vd_tmpletter();
    if (!l) { t_skip("temp dir is not a drive-letter path"); return; }
    CHECK((U(GetLogicalDrives)() >> (l - 'A')) & 1);
}
TEST(vd_logicaldrives_high_bits_clear) {
    REQUIRE(U(GetLogicalDrives));
    CHECK((U(GetLogicalDrives)() >> 26) == 0);
}
TEST(vd_logicaldrives_stable_repeated) {
    REQUIRE(U(GetLogicalDrives));
    DWORD a = U(GetLogicalDrives)();
    for (int i = 0; i < 50; i++) {
        DWORD b = U(GetLogicalDrives)();
        if (b != a) { t_note("drive mask changed during test (%08lx -> %08lx)", a, b); break; }
    }
    CHECK(a != 0);
}
TEST(vd_logicaldrives_kernel32_export) {
    REQUIRE(UK(GetLogicalDrives));
    CHECK(UK(GetLogicalDrives)() != 0);
}
TEST(vd_logicaldrives_set_bits_have_drivetype) {
    REQUIRE(U(GetLogicalDrives)); REQUIRE(U(GetDriveTypeA));
    DWORD m = U(GetLogicalDrives)();
    for (int i = 0; i < 26; i++) {
        if (!((m >> i) & 1)) continue;
        char root[4] = { (char)('A' + i), ':', '\\', 0 };
        UINT t = U(GetDriveTypeA)(root);
        CHECKF(t >= DRIVE_REMOVABLE && t <= DRIVE_RAMDISK, "drive %s type %u", root, t);
    }
}
TEST(vd_logicaldrives_clear_bits_no_root_dir) {
    REQUIRE(U(GetLogicalDrives)); REQUIRE(U(GetDriveTypeA));
    DWORD m = U(GetLogicalDrives)();
    int bad = 0;
    for (int i = 0; i < 26; i++) {
        if ((m >> i) & 1) continue;
        char root[4] = { (char)('A' + i), ':', '\\', 0 };
        UINT t = U(GetDriveTypeA)(root);
        if (t != DRIVE_NO_ROOT_DIR) { bad++; t_note("unmapped %s type %u", root, t); }
    }
    CHECK(bad == 0);
}

/* ---- GetLogicalDriveStrings ---- */
static int vd_popcount(DWORD m) { int n = 0; while (m) { n += m & 1; m >>= 1; } return n; }

TEST(vd_drivestrings_w_format) {
    REQUIRE(U(GetLogicalDriveStringsW)); REQUIRE(U(GetLogicalDrives));
    WCHAR buf[26 * 4 + 8]; memset(buf, 0xAA, sizeof buf);
    DWORD n = U(GetLogicalDriveStringsW)(ARRAYSIZE(buf), buf);
    CHECK(n >= 4 && n % 4 == 0);
    for (DWORD i = 0; i + 3 < n; i += 4) {
        CHECKF(buf[i] >= L'A' && buf[i] <= L'Z', "i=%lu c=%04x", i, buf[i]);
        CHECK(buf[i + 1] == L':'); CHECK(buf[i + 2] == L'\\'); CHECK(buf[i + 3] == 0);
    }
    CHECK(buf[n] == 0);   /* terminating second NUL */
}
TEST(vd_drivestrings_a_format) {
    REQUIRE(U(GetLogicalDriveStringsA));
    char buf[26 * 4 + 8]; memset(buf, 0x55, sizeof buf);
    DWORD n = U(GetLogicalDriveStringsA)(sizeof buf, buf);
    CHECK(n >= 4 && n % 4 == 0);
    for (DWORD i = 0; i + 3 < n; i += 4) {
        CHECK(buf[i] >= 'A' && buf[i] <= 'Z'); CHECK(buf[i + 1] == ':'); CHECK(buf[i + 2] == '\\'); CHECK(buf[i + 3] == 0);
    }
    CHECK(buf[n] == 0);
}
TEST(vd_drivestrings_count_matches_mask) {
    REQUIRE(U(GetLogicalDriveStringsW)); REQUIRE(U(GetLogicalDrives));
    DWORD m = U(GetLogicalDrives)();
    WCHAR buf[200];
    DWORD n = U(GetLogicalDriveStringsW)(ARRAYSIZE(buf), buf);
    CHECKF(n == (DWORD)vd_popcount(m) * 4, "n=%lu mask=%08lx", n, m);
}
TEST(vd_drivestrings_letters_match_mask_ascending) {
    REQUIRE(U(GetLogicalDriveStringsW)); REQUIRE(U(GetLogicalDrives));
    DWORD m = U(GetLogicalDrives)();
    WCHAR buf[200];
    DWORD n = U(GetLogicalDriveStringsW)(ARRAYSIZE(buf), buf);
    DWORD seen = 0; WCHAR prev = 0;
    for (DWORD i = 0; i + 3 < n; i += 4) {
        CHECK(buf[i] > prev); prev = buf[i];
        if (buf[i] >= L'A' && buf[i] <= L'Z') seen |= 1u << (buf[i] - L'A');
    }
    CHECKF(seen == m, "seen=%08lx mask=%08lx", seen, m);
}
TEST(vd_drivestrings_ab_parity) {
    REQUIRE(U(GetLogicalDriveStringsW)); REQUIRE(U(GetLogicalDriveStringsA));
    WCHAR w[200]; char a[200];
    DWORD nw = U(GetLogicalDriveStringsW)(ARRAYSIZE(w), w);
    DWORD na = U(GetLogicalDriveStringsA)(sizeof a, a);
    EXPECT_EQ(nw, na);
    for (DWORD i = 0; i < nw && i < na; i++) CHECK((WCHAR)(unsigned char)a[i] == w[i]);
}
TEST(vd_drivestrings_system_drive_listed) {
    REQUIRE(U(GetLogicalDriveStringsA));
    char buf[200]; DWORD n = U(GetLogicalDriveStringsA)(sizeof buf, buf);
    char want[4]; vd_sysroot(want);
    int found = 0;
    for (const char *p = buf; *p && p < buf + n; p += strlen(p) + 1) if (!vd_stricmp(p, want)) found = 1;
    CHECK(found);
}
TEST(vd_drivestrings_w_exact_fit_excludes_final_nul_in_count) {
    REQUIRE(U(GetLogicalDriveStringsW)); REQUIRE(U(GetLogicalDrives));
    DWORD k = vd_popcount(U(GetLogicalDrives)());
    WCHAR buf[200]; for (int i = 0; i < 200; i++) buf[i] = 0x7777;
    DWORD n = U(GetLogicalDriveStringsW)(4 * k + 1, buf);
    EXPECT_EQ(n, 4 * k);
    CHECK(buf[4 * k] == 0);
    CHECK(buf[4 * k + 1] == 0x7777);   /* nothing written past the buffer length */
}
TEST(vd_drivestrings_a_exact_fit) {
    REQUIRE(U(GetLogicalDriveStringsA)); REQUIRE(U(GetLogicalDrives));
    DWORD k = vd_popcount(U(GetLogicalDrives)());
    char buf[200]; memset(buf, 0x77, sizeof buf);
    DWORD n = U(GetLogicalDriveStringsA)(4 * k + 1, buf);
    EXPECT_EQ(n, 4 * k);
    CHECK(buf[4 * k] == 0);
    CHECK(buf[4 * k + 1] == 0x77);
}
TEST(vd_drivestrings_w_one_short_returns_required) {
    REQUIRE(U(GetLogicalDriveStringsW)); REQUIRE(U(GetLogicalDrives));
    DWORD k = vd_popcount(U(GetLogicalDrives)());
    WCHAR buf[200]; for (int i = 0; i < 200; i++) buf[i] = 0x7777;
    DWORD n = U(GetLogicalDriveStringsW)(4 * k, buf);
    EXPECT_EQ(n, 4 * k + 1);   /* required size including final NUL */
    CHECK(buf[4 * k] == 0x7777);
}
TEST(vd_drivestrings_a_one_short_returns_required) {
    REQUIRE(U(GetLogicalDriveStringsA)); REQUIRE(U(GetLogicalDrives));
    DWORD k = vd_popcount(U(GetLogicalDrives)());
    char buf[200]; memset(buf, 0x77, sizeof buf);
    DWORD n = U(GetLogicalDriveStringsA)(4 * k, buf);
    EXPECT_EQ(n, 4 * k + 1);
}
TEST(vd_drivestrings_w_size_query_null_buffer) {
    REQUIRE(U(GetLogicalDriveStringsW)); REQUIRE(U(GetLogicalDrives));
    DWORD k = vd_popcount(U(GetLogicalDrives)());
    EXPECT_EQ(U(GetLogicalDriveStringsW)(0, NULL), 4 * k + 1);
}
TEST(vd_drivestrings_a_size_query_null_buffer) {
    REQUIRE(U(GetLogicalDriveStringsA)); REQUIRE(U(GetLogicalDrives));
    DWORD k = vd_popcount(U(GetLogicalDrives)());
    EXPECT_EQ(U(GetLogicalDriveStringsA)(0, NULL), 4 * k + 1);
}
TEST(vd_drivestrings_w_zero_len_buffer_untouched) {
    REQUIRE(U(GetLogicalDriveStringsW));
    WCHAR buf[8]; for (int i = 0; i < 8; i++) buf[i] = 0x7777;
    U(GetLogicalDriveStringsW)(0, buf);
    for (int i = 0; i < 8; i++) CHECK(buf[i] == 0x7777);
}
TEST(vd_drivestrings_w_tiny_buffers_untouched) {
    REQUIRE(U(GetLogicalDriveStringsW)); REQUIRE(U(GetLogicalDrives));
    DWORD k = vd_popcount(U(GetLogicalDrives)());
    for (DWORD len = 1; len <= 4; len++) {   /* too small for even one entry + final NUL */
        WCHAR buf[8]; for (int i = 0; i < 8; i++) buf[i] = 0x7777;
        EXPECT_EQ(U(GetLogicalDriveStringsW)(len, buf), 4 * k + 1);
        for (int i = 0; i < 8; i++) CHECKF(buf[i] == 0x7777, "len=%lu i=%d", len, i);
    }
}
TEST(vd_drivestrings_w_partial_fill) {
    REQUIRE(U(GetLogicalDriveStringsW)); REQUIRE(U(GetLogicalDrives));
    DWORD k = vd_popcount(U(GetLogicalDrives)());
    if (k < 2) { t_skip("needs two drives"); return; }
    WCHAR buf[16]; for (int i = 0; i < 16; i++) buf[i] = 0x7777;
    DWORD n = U(GetLogicalDriveStringsW)(5, buf);     /* room for one entry + final NUL only */
    EXPECT_EQ(n, 4 * k + 1);
    CHECK(buf[0] >= L'A' && buf[0] <= L'Z');
    CHECK(buf[1] == L':'); CHECK(buf[2] == L'\\'); CHECK(buf[3] == 0);
}
TEST(vd_drivestrings_repeated_identical) {
    REQUIRE(U(GetLogicalDriveStringsW));
    WCHAR a[200], b[200];
    DWORD na = U(GetLogicalDriveStringsW)(200, a);
    for (int i = 0; i < 20; i++) {
        DWORD nb = U(GetLogicalDriveStringsW)(200, b);
        if (nb != na) { t_note("changed"); return; }
        CHECK(!memcmp(a, b, (na + 1) * 2));
    }
}
/* ---- GetDriveType ---- */
TEST(vd_drivetype_system_drive_fixed) {
    REQUIRE(U(GetDriveTypeA));
    char r[4]; vd_sysroot(r);
    EXPECT_EQ(U(GetDriveTypeA)(r), DRIVE_FIXED);
}
TEST(vd_drivetype_system_drive_w) {
    REQUIRE(U(GetDriveTypeW));
    char r[4]; WCHAR w[4]; vd_sysroot(r); vd_widen(r, w, 4);
    EXPECT_EQ(U(GetDriveTypeW)(w), DRIVE_FIXED);
}
TEST(vd_drivetype_ab_parity_all_letters) {
    REQUIRE(U(GetDriveTypeA)); REQUIRE(U(GetDriveTypeW));
    for (int i = 0; i < 26; i++) {
        char a[4] = { (char)('A' + i), ':', '\\', 0 }; WCHAR w[4] = { (WCHAR)('A' + i), L':', L'\\', 0 };
        UINT ta = U(GetDriveTypeA)(a), tw = U(GetDriveTypeW)(w);
        CHECKF(ta == tw, "%s a=%u w=%u", a, ta, tw);
        CHECK(ta <= DRIVE_RAMDISK);
    }
}
TEST(vd_drivetype_no_trailing_backslash_accepted) {
    REQUIRE(U(GetDriveTypeA));
    char r[4], s[3]; vd_sysroot(r); s[0] = r[0]; s[1] = ':'; s[2] = 0;
    EXPECT_EQ(U(GetDriveTypeA)(s), U(GetDriveTypeA)(r));
}
TEST(vd_drivetype_lowercase_letter) {
    REQUIRE(U(GetDriveTypeA));
    char r[4]; vd_sysroot(r); char l[4] = { (char)tolower(r[0]), ':', '\\', 0 };
    EXPECT_EQ(U(GetDriveTypeA)(l), U(GetDriveTypeA)(r));
}
TEST(vd_drivetype_null_is_current_dir) {
    REQUIRE(U(GetDriveTypeA)); REQUIRE(U(GetDriveTypeW));
    char cwd[MAX_PATH]; GetCurrentDirectoryA(sizeof cwd, cwd);
    if (cwd[1] != ':') { t_skip("cwd is not a drive path"); return; }
    char r[4] = { cwd[0], ':', '\\', 0 };
    UINT t = U(GetDriveTypeA)(NULL);
    EXPECT_EQ(t, U(GetDriveTypeA)(r));
    EXPECT_EQ(U(GetDriveTypeW)(NULL), t);
}
TEST(vd_drivetype_nonexistent_drive_no_root_dir) {
    REQUIRE(U(GetDriveTypeA)); REQUIRE(U(GetDriveTypeW));
    char l = vd_freeletter(); if (!l) { t_skip("no free letter"); return; }
    char a[4] = { l, ':', '\\', 0 }; WCHAR w[4] = { (WCHAR)l, L':', L'\\', 0 };
    EXPECT_EQ(U(GetDriveTypeA)(a), DRIVE_NO_ROOT_DIR);
    EXPECT_EQ(U(GetDriveTypeW)(w), DRIVE_NO_ROOT_DIR);
    a[2] = 0; EXPECT_EQ(U(GetDriveTypeA)(a), DRIVE_NO_ROOT_DIR);
}
TEST(vd_drivetype_empty_string_no_root_dir) {
    REQUIRE(U(GetDriveTypeA)); REQUIRE(U(GetDriveTypeW));
    EXPECT_EQ(U(GetDriveTypeA)(""), DRIVE_NO_ROOT_DIR);
    EXPECT_EQ(U(GetDriveTypeW)(L""), DRIVE_NO_ROOT_DIR);
}
TEST(vd_drivetype_minus_one_w_is_unknown) {
    REQUIRE(U(GetDriveTypeW));
    if (t_is_wine()) { t_skip("faults under wine"); return; }
    EXPECT_EQ(U(GetDriveTypeW)((LPCWSTR)(ULONG_PTR)-1), DRIVE_UNKNOWN);
}
TEST(vd_drivetype_nonroot_without_backslash_no_root_dir) {
    REQUIRE(U(GetDriveTypeA));
    char d[MAX_PATH]; GetSystemDirectoryA(d, sizeof d);   /* e.g. C:\Windows\System32 */
    EXPECT_EQ(U(GetDriveTypeA)(d), DRIVE_NO_ROOT_DIR);
}
TEST(vd_drivetype_nonroot_with_backslash) {
    REQUIRE(U(GetDriveTypeA));
    char d[MAX_PATH]; GetSystemDirectoryA(d, sizeof d); strcat(d, "\\");
    UINT t = U(GetDriveTypeA)(d);
    t_note("GetDriveType(\"%s\") = %u", d, t);
    CHECK(t <= DRIVE_RAMDISK);
}
TEST(vd_drivetype_tmp_root_matches_letter) {
    REQUIRE(U(GetDriveTypeA));
    char l = vd_tmpletter(); if (!l) { t_skip("no letter"); return; }
    char r[4]; vd_tmproot(r);
    UINT t = U(GetDriveTypeA)(r);
    CHECK(t >= DRIVE_REMOVABLE && t <= DRIVE_RAMDISK);
}
TEST(vd_drivetype_guid_path_matches_letter) {
    REQUIRE(U(GetDriveTypeA));
    char r[4]; vd_sysroot(r); char g[64];
    if (!vd_volname(r, g)) { t_skip("no volume name"); return; }
    EXPECT_EQ(U(GetDriveTypeA)(g), U(GetDriveTypeA)(r));
    EXPECT_EQ(U(GetDriveTypeA)(g), DRIVE_FIXED);
}
TEST(vd_drivetype_guid_path_w) {
    REQUIRE(U(GetDriveTypeW));
    char r[4]; vd_sysroot(r); char g[64]; WCHAR w[64];
    if (!vd_volname(r, g)) { t_skip("no volume name"); return; }
    vd_widen(g, w, 64);
    EXPECT_EQ(U(GetDriveTypeW)(w), DRIVE_FIXED);
}
TEST(vd_drivetype_dos_device_prefix_forms) {
    REQUIRE(U(GetDriveTypeA));
    char l = vd_sysletter(); char a[16], b[16], c[16];
    sprintf(a, "\\\\?\\%c:\\", l); sprintf(b, "\\\\.\\%c:\\", l); sprintf(c, "%c:\\", l);
    UINT t = U(GetDriveTypeA)(c);
    EXPECT_EQ(U(GetDriveTypeA)(a), t);
    UINT tb = U(GetDriveTypeA)(b);
    if (tb != t) t_note("\\\\.\\ form returned %u (plain %u)", tb, t);
}
TEST(vd_drivetype_relative_names_do_not_crash) {
    REQUIRE(U(GetDriveTypeA));
    static const char *const names[] = { "\\", ".", "..", "Windows", "\\\\", "\\\\?\\", "1:\\", "::\\", "CC:\\", "C:\\\\", "C:/", "C:\\.\\", " C:\\" };
    for (size_t i = 0; i < ARRAYSIZE(names); i++) {
        UINT t = U(GetDriveTypeA)(names[i]);
        t_note("GetDriveType(\"%s\") = %u", names[i], t);
        CHECK(t <= DRIVE_RAMDISK);
    }
}
TEST(vd_drivetype_repeated_stable) {
    REQUIRE(U(GetDriveTypeA));
    char r[4]; vd_sysroot(r);
    for (int i = 0; i < 200; i++) if (U(GetDriveTypeA)(r) != DRIVE_FIXED) { CHECK(0); break; }
}
TEST(vd_drivetype_fixed_mask_all_fixed_have_guid) {
    REQUIRE(U(GetDriveTypeA));
    DWORD m = vd_fixed_mask(); CHECK(m != 0);
}
TEST(vd_drivetype_long_path_nonexistent) {
    REQUIRE(U(GetDriveTypeA));
    char p[400]; strcpy(p, "C:\\"); for (int i = 0; i < 40; i++) strcat(p, "abcdefgh\\");
    UINT t = U(GetDriveTypeA)(p);
    CHECK(t <= DRIVE_RAMDISK);
}
