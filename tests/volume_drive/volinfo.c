/* GetVolumeInformation A/W/ByHandleW */
#include "vd.h"

typedef struct { WCHAR label[MAX_PATH + 1]; WCHAR fs[MAX_PATH + 1]; DWORD serial, maxcomp, flags; } vd_vi;
static BOOL vd_gvi_w(const char *root, vd_vi *v) {
    WCHAR w[MAX_PATH]; if (root) vd_widen(root, w, MAX_PATH);
    return U(GetVolumeInformationW)(root ? w : NULL, v->label, MAX_PATH + 1, &v->serial, &v->maxcomp, &v->flags, v->fs, MAX_PATH + 1);
}
static int vd_is_ntfs(const vd_vi *v) { return !lstrcmpW(v->fs, L"NTFS"); }

TEST(vd_gvi_w_system_root_basic) {
    REQUIRE(U(GetVolumeInformationW));
    char r[4]; vd_sysroot(r); vd_vi v; memset(&v, 0, sizeof v);
    CHECK(vd_gvi_w(r, &v));
    char fs[64]; vd_narrow(v.fs, fs, sizeof fs);
    CHECKF(vd_known_fs(fs), "fs=%s", fs);
    CHECK(v.serial != 0);
    CHECK(v.maxcomp == 255);
    CHECK(v.flags & FILE_CASE_PRESERVED_NAMES);
    CHECK(v.flags & FILE_UNICODE_ON_DISK);
}
TEST(vd_gvi_a_system_root_basic) {
    REQUIRE(U(GetVolumeInformationA));
    char r[4]; vd_sysroot(r); char label[MAX_PATH + 1], fs[MAX_PATH + 1]; DWORD ser = 0, mc = 0, fl = 0;
    CHECK(U(GetVolumeInformationA)(r, label, sizeof label, &ser, &mc, &fl, fs, sizeof fs));
    CHECK(vd_known_fs(fs)); CHECK(mc == 255);
}
TEST(vd_gvi_ab_parity) {
    REQUIRE(U(GetVolumeInformationA)); REQUIRE(U(GetVolumeInformationW));
    char r[4]; vd_sysroot(r); vd_vi v; memset(&v, 0, sizeof v); CHECK(vd_gvi_w(r, &v));
    char label[MAX_PATH + 1], fs[MAX_PATH + 1], wl[MAX_PATH + 1], wf[MAX_PATH + 1]; DWORD ser, mc, fl;
    CHECK(U(GetVolumeInformationA)(r, label, sizeof label, &ser, &mc, &fl, fs, sizeof fs));
    vd_narrow(v.label, wl, sizeof wl); vd_narrow(v.fs, wf, sizeof wf);
    EXPECT_STR(label, wl); EXPECT_STR(fs, wf); EXPECT_EQ(ser, v.serial); EXPECT_EQ(mc, v.maxcomp); EXPECT_EQ(fl, v.flags);
}
TEST(vd_gvi_ntfs_flags) {
    REQUIRE(U(GetVolumeInformationW));
    char r[4]; vd_sysroot(r); vd_vi v; memset(&v, 0, sizeof v); CHECK(vd_gvi_w(r, &v));
    if (!vd_is_ntfs(&v)) { t_skip("system volume is not NTFS"); return; }
    DWORD want = FILE_CASE_SENSITIVE_SEARCH | FILE_CASE_PRESERVED_NAMES | FILE_UNICODE_ON_DISK | FILE_PERSISTENT_ACLS |
                 FILE_SUPPORTS_REPARSE_POINTS | FILE_SUPPORTS_SPARSE_FILES | FILE_SUPPORTS_OBJECT_IDS |
                 FILE_SUPPORTS_HARD_LINKS | FILE_SUPPORTS_EXTENDED_ATTRIBUTES | FILE_SUPPORTS_OPEN_BY_FILE_ID | FILE_SUPPORTS_USN_JOURNAL |
                 FILE_NAMED_STREAMS | FILE_FILE_COMPRESSION;
    CHECKF((v.flags & want) == want, "flags=%08lx missing=%08lx", v.flags, want & ~v.flags);
    CHECK(!(v.flags & FILE_READ_ONLY_VOLUME));
}
TEST(vd_gvi_ntfs_maxcomp_255) {
    REQUIRE(U(GetVolumeInformationW));
    char r[4]; vd_sysroot(r); vd_vi v; memset(&v, 0, sizeof v); CHECK(vd_gvi_w(r, &v));
    EXPECT_EQ(v.maxcomp, 255);
}
TEST(vd_gvi_serial_matches_across_calls) {
    REQUIRE(U(GetVolumeInformationW));
    char r[4]; vd_sysroot(r); vd_vi a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    CHECK(vd_gvi_w(r, &a)); CHECK(vd_gvi_w(r, &b));
    EXPECT_EQ(a.serial, b.serial);
    EXPECT_WSTR(a.label, b.label);
}
TEST(vd_gvi_no_trailing_backslash_invalid_name) {
    REQUIRE(U(GetVolumeInformationW)); REQUIRE(U(GetVolumeInformationA));
    /* Must not be a bare "X:" (resolves via the drive's cwd, possibly the root). Reference analysis: NT path not ending in '\\' -> STATUS_OBJECT_NAME_INVALID -> 123. */
    char s[16] = { vd_sysletter(), ':', '\\', 'W', 'i', 'n', 'd', 'o', 'w', 's', 0 }; WCHAR w[16]; vd_widen(s, w, 16);
    DWORD ser = SENT_U;
    SET_ERR(0);
    CHECK(!U(GetVolumeInformationW)(w, NULL, 0, &ser, NULL, NULL, NULL, 0));
    EXPECT_ERR(VD_ERR_INVALID_NAME);
    CHECK(ser == SENT_U);
    SET_ERR(0);
    CHECK(!U(GetVolumeInformationA)(s, NULL, 0, &ser, NULL, NULL, NULL, 0));
    EXPECT_ERR(VD_ERR_INVALID_NAME);
}
TEST(vd_gvi_directory_not_root) {
    REQUIRE(U(GetVolumeInformationW));
    WCHAR w[MAX_PATH]; vd_widen(t_tmpdir(), w, MAX_PATH); DWORD ser;
    SET_ERR(0);
    CHECK(!U(GetVolumeInformationW)(w, NULL, 0, &ser, NULL, NULL, NULL, 0));
    EXPECT_ERR(ERROR_DIR_NOT_ROOT);
}
TEST(vd_gvi_system_directory_not_root) {
    REQUIRE(U(GetVolumeInformationA));
    char d[MAX_PATH]; GetSystemDirectoryA(d, sizeof d); strcat(d, "\\"); DWORD ser;
    SET_ERR(0);
    CHECK(!U(GetVolumeInformationA)(d, NULL, 0, &ser, NULL, NULL, NULL, 0));
    EXPECT_ERR(ERROR_DIR_NOT_ROOT);
}
TEST(vd_gvi_null_root_uses_current_volume) {
    REQUIRE(U(GetVolumeInformationW));
    char cwd[MAX_PATH]; GetCurrentDirectoryA(sizeof cwd, cwd);
    if (cwd[1] != ':') { t_skip("cwd not drive path"); return; }
    char r[4] = { cwd[0], ':', '\\', 0 };
    vd_vi a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    CHECK(vd_gvi_w(NULL, &a)); CHECK(vd_gvi_w(r, &b));
    EXPECT_EQ(a.serial, b.serial); EXPECT_WSTR(a.fs, b.fs);
}
TEST(vd_gvi_null_root_a) {
    REQUIRE(U(GetVolumeInformationA));
    DWORD ser = 0; CHECK(U(GetVolumeInformationA)(NULL, NULL, 0, &ser, NULL, NULL, NULL, 0)); CHECK(ser != 0);
}
TEST(vd_gvi_nonexistent_drive) {
    REQUIRE(U(GetVolumeInformationW));
    char l = vd_freeletter(); if (!l) { t_skip("no free letter"); return; }
    char a[4] = { l, ':', '\\', 0 }; vd_vi v; memset(&v, 0, sizeof v);
    SET_ERR(0);
    CHECK(!vd_gvi_w(a, &v));
    DWORD e = GetLastError();
    CHECKF(e == ERROR_PATH_NOT_FOUND || e == ERROR_INVALID_DRIVE, "err %lu", e);
}
TEST(vd_gvi_empty_root_string) {
    REQUIRE(U(GetVolumeInformationW));
    SET_ERR(0);
    CHECK(!U(GetVolumeInformationW)(L"", NULL, 0, NULL, NULL, NULL, NULL, 0));
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(vd_gvi_all_outputs_null) {
    REQUIRE(U(GetVolumeInformationW)); REQUIRE(U(GetVolumeInformationA));
    char r[4]; vd_sysroot(r); WCHAR w[4]; vd_widen(r, w, 4);
    CHECK(U(GetVolumeInformationW)(w, NULL, 0, NULL, NULL, NULL, NULL, 0));
    CHECK(U(GetVolumeInformationA)(r, NULL, 0, NULL, NULL, NULL, NULL, 0));
}
TEST(vd_gvi_each_output_alone) {
    REQUIRE(U(GetVolumeInformationW));
    char r[4]; vd_sysroot(r); WCHAR w[4]; vd_widen(r, w, 4);
    DWORD s = SENT_U, m = SENT_U, f = SENT_U; WCHAR lab[MAX_PATH + 1], fs[MAX_PATH + 1]; lab[0] = fs[0] = 0x7777;
    CHECK(U(GetVolumeInformationW)(w, NULL, 0, &s, NULL, NULL, NULL, 0)); CHECK(s != SENT_U);
    CHECK(U(GetVolumeInformationW)(w, NULL, 0, NULL, &m, NULL, NULL, 0)); CHECK(m != SENT_U);
    CHECK(U(GetVolumeInformationW)(w, NULL, 0, NULL, NULL, &f, NULL, 0)); CHECK(f != SENT_U);
    CHECK(U(GetVolumeInformationW)(w, lab, MAX_PATH + 1, NULL, NULL, NULL, NULL, 0)); CHECK(lab[0] != 0x7777);
    CHECK(U(GetVolumeInformationW)(w, NULL, 0, NULL, NULL, NULL, fs, MAX_PATH + 1)); CHECK(fs[0] != 0x7777 && fs[0] != 0);
}
TEST(vd_gvi_label_buffer_size_boundary) {
    REQUIRE(U(GetVolumeInformationW));
    char r[4]; vd_sysroot(r); WCHAR w[4]; vd_widen(r, w, 4); vd_vi v; memset(&v, 0, sizeof v);
    CHECK(vd_gvi_w(r, &v));
    DWORD L = lstrlenW(v.label); WCHAR buf[MAX_PATH + 8];
    for (int i = 0; i < MAX_PATH + 8; i++) buf[i] = 0x7777;
    SET_ERR(0);
    CHECK(U(GetVolumeInformationW)(w, buf, L + 1, NULL, NULL, NULL, NULL, 0));      /* exact fit incl. NUL */
    EXPECT_EQ(lstrlenW(buf), L);
    SET_ERR(0);
    CHECK(!U(GetVolumeInformationW)(w, buf, L, NULL, NULL, NULL, NULL, 0));         /* no room for NUL */
    EXPECT_ERR(ERROR_BAD_LENGTH);
}
TEST(vd_gvi_label_buffer_zero_size_fails_even_when_empty) {
    REQUIRE(U(GetVolumeInformationW));
    char r[4]; vd_sysroot(r); WCHAR w[4]; vd_widen(r, w, 4); WCHAR buf[4] = { 0x7777 };
    SET_ERR(0);
    vd_vi lv; memset(&lv, 0, sizeof lv); CHECK(vd_gvi_w(r, &lv));
    DWORD Ll = lstrlenW(lv.label);
    SET_ERR(0);
    CHECK(!U(GetVolumeInformationW)(w, buf, 0, NULL, NULL, NULL, NULL, 0));
    /* Win10 kernelbase queries FileFsVolumeInformation into a 0x18-byte buffer (0x12 header + 6 label bytes): a
       label longer than 3 chars overflows -> STATUS_BUFFER_OVERFLOW -> ERROR_MORE_DATA; otherwise the size check gives ERROR_BAD_LENGTH. */
    EXPECT_ERR(Ll > 3 ? ERROR_MORE_DATA : ERROR_BAD_LENGTH);
    CHECK(buf[0] == 0x7777);
}
TEST(vd_gvi_fsname_buffer_size_boundary) {
    REQUIRE(U(GetVolumeInformationW));
    char r[4]; vd_sysroot(r); WCHAR w[4]; vd_widen(r, w, 4); vd_vi v; memset(&v, 0, sizeof v);
    CHECK(vd_gvi_w(r, &v));
    DWORD L = lstrlenW(v.fs); WCHAR buf[MAX_PATH + 8];
    for (int i = 0; i < MAX_PATH + 8; i++) buf[i] = 0x7777;
    CHECK(U(GetVolumeInformationW)(w, NULL, 0, NULL, NULL, NULL, buf, L + 1));
    EXPECT_EQ(lstrlenW(buf), L);
    SET_ERR(0);
    CHECK(!U(GetVolumeInformationW)(w, NULL, 0, NULL, NULL, NULL, buf, L));
    EXPECT_ERR(ERROR_BAD_LENGTH);
    SET_ERR(0);
    CHECK(!U(GetVolumeInformationW)(w, NULL, 0, NULL, NULL, NULL, buf, 1));
    /* Win10: the query buffer is 2*size+0x10 bytes for a 12-byte header + 2*L name bytes; overflow (ERROR_MORE_DATA) when size < L-2 */
    EXPECT_ERR(1 < (long)L - 2 ? ERROR_MORE_DATA : ERROR_BAD_LENGTH);
    SET_ERR(0);
    CHECK(!U(GetVolumeInformationW)(w, NULL, 0, NULL, NULL, NULL, buf, 0));
    EXPECT_ERR(0 < (long)L - 2 ? ERROR_MORE_DATA : ERROR_BAD_LENGTH);
}
TEST(vd_gvi_a_fsname_boundary) {
    REQUIRE(U(GetVolumeInformationA));
    char r[4]; vd_sysroot(r); char fs[64]; CHECK(U(GetVolumeInformationA)(r, NULL, 0, NULL, NULL, NULL, fs, sizeof fs));
    DWORD L = (DWORD)strlen(fs); char buf[64]; memset(buf, 0x77, sizeof buf);
    CHECK(U(GetVolumeInformationA)(r, NULL, 0, NULL, NULL, NULL, buf, L + 1));
    SET_ERR(0);
    CHECK(!U(GetVolumeInformationA)(r, NULL, 0, NULL, NULL, NULL, buf, L));
    DWORD e = GetLastError();
    CHECKF(e == ERROR_BAD_LENGTH || e == ERROR_INSUFFICIENT_BUFFER, "err %lu", e);
}
TEST(vd_gvi_volume_guid_root) {
    REQUIRE(U(GetVolumeInformationW));
    char r[4]; vd_sysroot(r); char g[64];
    if (!vd_volname(r, g)) { t_skip("no volume name"); return; }
    vd_vi a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    CHECK(vd_gvi_w(g, &a)); CHECK(vd_gvi_w(r, &b));
    EXPECT_EQ(a.serial, b.serial); EXPECT_WSTR(a.fs, b.fs); EXPECT_EQ(a.flags, b.flags);
}
TEST(vd_gvi_dosdevice_prefix_root) {
    REQUIRE(U(GetVolumeInformationW));
    char p[16]; sprintf(p, "\\\\?\\%c:\\", vd_sysletter()); vd_vi v; memset(&v, 0, sizeof v);
    CHECK(vd_gvi_w(p, &v)); CHECK(v.serial != 0);
}
TEST(vd_gvi_lowercase_drive) {
    REQUIRE(U(GetVolumeInformationW));
    char r[4]; vd_sysroot(r); r[0] = (char)tolower(r[0]); vd_vi v; memset(&v, 0, sizeof v);
    CHECK(vd_gvi_w(r, &v));
}
TEST(vd_gvi_invalid_chars_in_root) {
    REQUIRE(U(GetVolumeInformationW));
    static const WCHAR *const bad[] = { L"C:\\a|b\\", L"C:\\a<b\\", L"C:\\a*b\\" };
    for (size_t i = 0; i < ARRAYSIZE(bad); i++) { SET_ERR(0); CHECKF(!U(GetVolumeInformationW)(bad[i], NULL, 0, NULL, NULL, NULL, NULL, 0), "case %zu", i); CHECK(GetLastError() != 0); }
}
TEST(vd_gvi_tmp_root_consistent_with_byhandle) {
    REQUIRE(U(GetVolumeInformationW)); REQUIRE(U(GetVolumeInformationByHandleW));
    char r[4]; vd_tmproot(r); if (!r[0]) { t_skip("tmp not on a letter"); return; }
    vd_vi a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    CHECK(vd_gvi_w(r, &a));
    char f[MAX_PATH]; t_tmpfile(f, sizeof f, "vd_gvi_probe.bin");
    HANDLE h = CreateFileA(f, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_skip("cannot create"); return; }
    BOOL ok = U(GetVolumeInformationByHandleW)(h, b.label, MAX_PATH + 1, &b.serial, &b.maxcomp, &b.flags, b.fs, MAX_PATH + 1);
    CHECK(ok);
    CloseHandle(h); DeleteFileA(f);
    EXPECT_EQ(a.serial, b.serial); EXPECT_EQ(a.maxcomp, b.maxcomp); EXPECT_EQ(a.flags, b.flags);
    EXPECT_WSTR(a.fs, b.fs); EXPECT_WSTR(a.label, b.label);
}
TEST(vd_gvi_flags_on_system_vs_tmp_volume_same_fs) {
    REQUIRE(U(GetVolumeInformationW));
    char s[4], t[4]; vd_sysroot(s); vd_tmproot(t); if (!t[0]) { t_skip("tmp not on a letter"); return; }
    if (!strcmp(s, t)) { vd_vi a; memset(&a, 0, sizeof a); CHECK(vd_gvi_w(s, &a)); CHECK(a.flags != 0); return; }
    vd_vi a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    CHECK(vd_gvi_w(s, &a)); CHECK(vd_gvi_w(t, &b));
    if (!lstrcmpW(a.fs, b.fs)) EXPECT_EQ(a.maxcomp, b.maxcomp);
}
TEST(vd_gvi_all_fixed_drives) {
    REQUIRE(U(GetVolumeInformationA));
    DWORD m = vd_fixed_mask(); int n = 0;
    for (int i = 0; i < 26; i++) {
        if (!((m >> i) & 1)) continue;
        char r[4] = { (char)('A' + i), ':', '\\', 0 }; char fs[64]; DWORD mc = 0;
        if (!U(GetVolumeInformationA)(r, NULL, 0, NULL, &mc, NULL, fs, sizeof fs)) { t_note("%s failed %lu", r, GetLastError()); continue; }
        n++;
        CHECKF(vd_known_fs(fs), "%s fs=%s", r, fs);
        CHECKF(mc == 255, "%s maxcomp %lu", r, mc);
    }
    CHECK(n >= 1);
}
TEST(vd_gvi_repeated_calls) {
    REQUIRE(U(GetVolumeInformationA));
    char r[4]; vd_sysroot(r);
    for (int i = 0; i < 200; i++) { DWORD s; if (!U(GetVolumeInformationA)(r, NULL, 0, &s, NULL, NULL, NULL, 0)) { CHECK(0); break; } }
}
TEST(vd_gvi_unc_server_only_root_rejected_safely) {
    REQUIRE(U(GetVolumeInformationW));
    /* "\\" alone is not a volume root */
    SET_ERR(0);
    BOOL ok = U(GetVolumeInformationW)(L"\\\\", NULL, 0, NULL, NULL, NULL, NULL, 0);
    CHECK(!ok); CHECK(GetLastError() != 0);
}

/* ---- GetVolumeInformationByHandleW ---- */
static HANDLE vd_open_dir(const char *p) {
    return CreateFileA(p, FILE_READ_ATTRIBUTES | SYNCHRONIZE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
}
TEST(vd_gvibh_directory_handle) {
    REQUIRE(U(GetVolumeInformationByHandleW));
    HANDLE h = vd_open_dir(t_tmpdir()); if (h == INVALID_HANDLE_VALUE) { t_skip("open failed"); return; }
    vd_vi v; memset(&v, 0, sizeof v);
    CHECK(U(GetVolumeInformationByHandleW)(h, v.label, MAX_PATH + 1, &v.serial, &v.maxcomp, &v.flags, v.fs, MAX_PATH + 1));
    CHECK(v.serial != 0); CHECK(v.maxcomp == 255);
    CloseHandle(h);
}
TEST(vd_gvibh_file_handle_matches_dir_handle) {
    REQUIRE(U(GetVolumeInformationByHandleW));
    char f[MAX_PATH]; t_tmpfile(f, sizeof f, "vd_gvibh_file.bin");
    HANDLE hf = CreateFileA(f, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    HANDLE hd = vd_open_dir(t_tmpdir());
    if (hf == INVALID_HANDLE_VALUE || hd == INVALID_HANDLE_VALUE) { t_skip("open failed"); return; }
    DWORD s1 = 0, s2 = 0, fl1 = 0, fl2 = 0;
    CHECK(U(GetVolumeInformationByHandleW)(hf, NULL, 0, &s1, NULL, &fl1, NULL, 0));
    CHECK(U(GetVolumeInformationByHandleW)(hd, NULL, 0, &s2, NULL, &fl2, NULL, 0));
    EXPECT_EQ(s1, s2); EXPECT_EQ(fl1, fl2);
    CloseHandle(hf); CloseHandle(hd); DeleteFileA(f);
}
TEST(vd_gvibh_closed_handle) {
    REQUIRE(U(GetVolumeInformationByHandleW));
    HANDLE h = vd_open_dir(t_tmpdir()); if (h == INVALID_HANDLE_VALUE) { t_skip("open failed"); return; }
    CloseHandle(h);
    DWORD s; SET_ERR(0);
    CHECK(!U(GetVolumeInformationByHandleW)(h, NULL, 0, &s, NULL, NULL, NULL, 0));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(vd_gvibh_invalid_and_null_handle) {
    REQUIRE(U(GetVolumeInformationByHandleW));
    DWORD s; SET_ERR(0);
    CHECK(!U(GetVolumeInformationByHandleW)(INVALID_HANDLE_VALUE, NULL, 0, &s, NULL, NULL, NULL, 0));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(!U(GetVolumeInformationByHandleW)(NULL, NULL, 0, &s, NULL, NULL, NULL, 0));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(vd_gvibh_wrong_object_type) {
    REQUIRE(U(GetVolumeInformationByHandleW));
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL); DWORD s; SET_ERR(0);
    CHECK(!U(GetVolumeInformationByHandleW)(e, NULL, 0, &s, NULL, NULL, NULL, 0));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(e);
}
TEST(vd_gvibh_label_boundary) {
    REQUIRE(U(GetVolumeInformationByHandleW));
    HANDLE h = vd_open_dir(t_tmpdir()); if (h == INVALID_HANDLE_VALUE) { t_skip("open failed"); return; }
    vd_vi v; memset(&v, 0, sizeof v);
    CHECK(U(GetVolumeInformationByHandleW)(h, v.label, MAX_PATH + 1, NULL, NULL, NULL, NULL, 0));
    DWORD L = lstrlenW(v.label); WCHAR b[MAX_PATH + 1];
    SET_ERR(0);
    CHECK(!U(GetVolumeInformationByHandleW)(h, b, L, NULL, NULL, NULL, NULL, 0)); EXPECT_ERR(ERROR_BAD_LENGTH);
    CHECK(U(GetVolumeInformationByHandleW)(h, b, L + 1, NULL, NULL, NULL, NULL, 0));
    EXPECT_WSTR(b, v.label);
    CloseHandle(h);
}
TEST(vd_gvibh_zero_access_volume_handle) {
    REQUIRE(U(GetVolumeInformationByHandleW));
    char p[8]; sprintf(p, "\\\\.\\%c:", vd_sysletter());
    HANDLE h = CreateFileA(p, SYNCHRONIZE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_skip("cannot open %s: %lu", p, GetLastError()); return; }
    /* A zero-data-access open of "\\.\C:" is a direct device open (IopParseDevice, access mask & 0xfee1ff7f == 0):
       the handle targets the volume device, not the mounted file system, so the FS volume query is rejected
       (observed on Windows 10: FALSE, ERROR_INVALID_FUNCTION). */
    vd_vi v; memset(&v, 0, sizeof v); SET_ERR(0);
    BOOL okz = U(GetVolumeInformationByHandleW)(h, v.label, MAX_PATH + 1, &v.serial, &v.maxcomp, &v.flags, v.fs, MAX_PATH + 1);
    DWORD ez = GetLastError();
    t_note("ByHandle(zero-access volume handle) ok=%d GetLastError=%lu", okz, ez);
    CHECK(!okz);
    CloseHandle(h);
}
TEST(vd_gvibh_pipe_handle_rejected) {
    REQUIRE(U(GetVolumeInformationByHandleW));
    HANDLE rd, wr; if (!CreatePipe(&rd, &wr, NULL, 0)) { t_skip("pipe"); return; }
    DWORD s; SET_ERR(0);
    BOOL ok = U(GetVolumeInformationByHandleW)(rd, NULL, 0, &s, NULL, NULL, NULL, 0);
    t_note("ByHandle(pipe) = %d err=%lu", ok, GetLastError());
    CloseHandle(rd); CloseHandle(wr);
}
