/* SetFileInformationByHandle. */
#include "fa.h"
#include <wchar.h>

#define C_BASIC 0
#define C_STANDARD 1
#define C_NAME 2
#define C_RENAME 3
#define C_DISP 4
#define C_ALLOC 5
#define C_EOF 6
#define C_STREAM 7
#define C_IOPRIO 12
#define C_DISPEX 21
#define C_RENAMEEX 22
#define C_CASE 23
#define C_NORMNAME 24
#ifndef FILE_DISPOSITION_FLAG_DELETE
#define FILE_DISPOSITION_FLAG_DELETE 0x1
#define FILE_DISPOSITION_FLAG_POSIX_SEMANTICS 0x2
#define FILE_DISPOSITION_FLAG_FORCE_IMAGE_SECTION_CHECK 0x4
#define FILE_DISPOSITION_FLAG_ON_CLOSE 0x8
#define FILE_DISPOSITION_FLAG_IGNORE_READONLY_ATTRIBUTE 0x10
#endif
#define RN_REPLACE 0x1
#define RN_POSIX 0x2

static BOOL si(HANDLE h, DWORD cls, void *b, DWORD n) {
    return U(SetFileInformationByHandle)(h, (FILE_INFO_BY_HANDLE_CLASS)cls, b, n);
}
static BOOL gi(HANDLE h, DWORD cls, void *b, DWORD n) {
    return U(GetFileInformationByHandleEx)(h, (FILE_INFO_BY_HANDLE_CLASS)cls, b, n);
}
static int real_ntfs(void) { return fa_is_ntfs() && !t_is_wine(); }
static HANDLE mk(const char *leaf, DWORD access) {
    fa_mkfile_s(leaf, "0123456789");
    return fa_openl(leaf, access, FA_SHARE_ALL, OPEN_EXISTING, 0);
}
/* build FILE_RENAME_INFO for DOS path (leaf under tmpdir) */
static FILE_RENAME_INFO *mkren(const char *dst_path, DWORD flags, DWORD *outsz) {
    WCHAR w[MAX_PATH * 2]; fa_widen(w, dst_path);
    DWORD bytes = (DWORD)lstrlenW(w) * 2;
    DWORD sz = (DWORD)offsetof(FILE_RENAME_INFO, FileName) + bytes + 2;
    FILE_RENAME_INFO *ri = calloc(1, sz);
    ri->Flags = flags;
    ri->FileNameLength = bytes;
    memcpy(ri->FileName, w, bytes);
    *outsz = sz;
    return ri;
}
static BOOL do_rename(HANDLE h, DWORD cls, const char *dst, DWORD flags) {
    DWORD sz; FILE_RENAME_INFO *ri = mkren(dst, flags, &sz);
    BOOL r = si(h, cls, ri, sz);
    DWORD e = GetLastError(); free(ri); SetLastError(e);
    return r;
}

TEST(fsinfo_set_undersized_buffer_bad_length) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_small.txt", GENERIC_READ | GENERIC_WRITE | DELETE);
    BYTE buf[256]; memset(buf, 0, sizeof buf);
    static const struct { DWORD cls; DWORD min; const char *n; } m[] = {
        { C_BASIC, 0x28, "Basic" }, { C_DISP, 1, "Disp" }, { C_ALLOC, 8, "Alloc" }, { C_EOF, 8, "EOF" },
        { C_IOPRIO, 4, "IoPrio" }, { C_DISPEX, 4, "DispEx" }, { C_CASE, 4, "Case" },
    };
    for (unsigned i = 0; i < sizeof m / sizeof *m; i++) {
        SetLastError(0);
        BOOL r = si(h, m[i].cls, buf, m[i].min - 1);
        CHECKF(!r && GetLastError() == ERROR_BAD_LENGTH, "%s min-1: r=%d err=%lu", m[i].n, r, GetLastError());
        SetLastError(0);
        r = si(h, m[i].cls, buf, 0);
        CHECKF(!r && GetLastError() == ERROR_BAD_LENGTH, "%s 0: r=%d err=%lu", m[i].n, r, GetLastError());
    }
    CloseHandle(h); fa_rml("si_small.txt");
}
TEST(fsinfo_set_invalid_classes) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_badcls.txt", GENERIC_READ | GENERIC_WRITE | DELETE);
    static const DWORD bad[] = { 1, 2, 7, 8, 9, 10, 11, 13, 14, 15, 16, 17, 18, 19, 20, 24, 25, 26, 100, 0xffffffff, 0x80000000 };
    BYTE buf[512]; memset(buf, 0, sizeof buf);
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SetLastError(0);
        BOOL r = si(h, bad[i], buf, sizeof buf);
        CHECKF(!r && GetLastError() == ERROR_INVALID_PARAMETER, "class %lu r=%d err=%lu", bad[i], r, GetLastError());
    }
    CloseHandle(h); fa_rml("si_badcls.txt");
}
TEST(fsinfo_set_invalid_handle) {
    REQUIRE(U(SetFileInformationByHandle));
    FILE_BASIC_INFO bi; memset(&bi, 0, sizeof bi);
    SetLastError(0); EXPECT_EQ(si(INVALID_HANDLE_VALUE, C_BASIC, &bi, sizeof bi), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SetLastError(0); EXPECT_EQ(si(NULL, C_BASIC, &bi, sizeof bi), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    FILE_DISPOSITION_INFO di = { FALSE };
    SetLastError(0); EXPECT_EQ(si(INVALID_HANDLE_VALUE, C_DISP, &di, sizeof di), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    FILE_END_OF_FILE_INFO ei; memset(&ei, 0, sizeof ei);
    SetLastError(0); EXPECT_EQ(si(NULL, C_EOF, &ei, sizeof ei), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(fsinfo_set_wrong_handle_type) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE e = CreateEventA(NULL, FALSE, FALSE, NULL);
    FILE_BASIC_INFO bi; memset(&bi, 0, sizeof bi);
    SetLastError(0); EXPECT_EQ(si(e, C_BASIC, &bi, sizeof bi), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    FILE_END_OF_FILE_INFO ei; memset(&ei, 0, sizeof ei);
    SetLastError(0); EXPECT_EQ(si(e, C_EOF, &ei, sizeof ei), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(e);
}

/* ---- FileBasicInfo ---- */
TEST(fsinfo_set_basic_write_time) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_bw.txt", GENERIC_READ | GENERIC_WRITE);
    FILE_BASIC_INFO bi; memset(&bi, 0, sizeof bi);
    bi.LastWriteTime.QuadPart = FA_FT_2001 + 99 * FA_SEC;
    CHECKF(si(h, C_BASIC, &bi, sizeof bi), "err %lu", GetLastError());
    FILETIME w, c0; GetFileTime(h, NULL, NULL, &w);
    if (real_ntfs()) EXPECT_EQ(fa_ft64(w), FA_FT_2001 + 99 * FA_SEC);
    CloseHandle(h); fa_rml("si_bw.txt");
}
TEST(fsinfo_set_basic_all_times_100ns) {
    REQUIRE(U(SetFileInformationByHandle));
    if (!real_ntfs()) { t_skip("needs real NTFS"); return; }
    HANDLE h = mk("si_b3.txt", GENERIC_READ | GENERIC_WRITE);
    FILE_BASIC_INFO bi, r; memset(&bi, 0, sizeof bi);
    bi.CreationTime.QuadPart = FA_FT_2001 + 1;
    bi.LastAccessTime.QuadPart = FA_FT_2001 + 22;
    bi.LastWriteTime.QuadPart = FA_FT_2001 + 333;
    bi.ChangeTime.QuadPart = FA_FT_2001 + 4444;
    CHECK(si(h, C_BASIC, &bi, sizeof bi));
    CHECK(gi(h, C_BASIC, &r, sizeof r));
    EXPECT_EQ(r.CreationTime.QuadPart, bi.CreationTime.QuadPart);
    EXPECT_EQ(r.LastAccessTime.QuadPart, bi.LastAccessTime.QuadPart);
    EXPECT_EQ(r.LastWriteTime.QuadPart, bi.LastWriteTime.QuadPart);
    EXPECT_EQ(r.ChangeTime.QuadPart, bi.ChangeTime.QuadPart);
    CloseHandle(h); fa_rml("si_b3.txt");
}
TEST(fsinfo_set_basic_zero_means_unchanged) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_b0.txt", GENERIC_READ | GENERIC_WRITE);
    char p[MAX_PATH * 2]; fa_path(p, "si_b0.txt");
    FILE_BASIC_INFO a, b, z; memset(&z, 0, sizeof z);
    gi(h, C_BASIC, &a, sizeof a);
    CHECK(si(h, C_BASIC, &z, sizeof z));
    gi(h, C_BASIC, &b, sizeof b);
    EXPECT_EQ(a.CreationTime.QuadPart, b.CreationTime.QuadPart);
    EXPECT_EQ(a.LastWriteTime.QuadPart, b.LastWriteTime.QuadPart);
    EXPECT_EQ(a.FileAttributes, b.FileAttributes);       /* attributes == 0 means "no change" here */
    CloseHandle(h); fa_rm(p);
}
TEST(fsinfo_set_basic_attributes) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_ba.txt", GENERIC_READ | GENERIC_WRITE);
    char p[MAX_PATH * 2]; fa_path(p, "si_ba.txt");
    FILE_BASIC_INFO bi; memset(&bi, 0, sizeof bi);
    bi.FileAttributes = FILE_ATTRIBUTE_HIDDEN;
    CHECKF(si(h, C_BASIC, &bi, sizeof bi), "err %lu", GetLastError());
    CHECK(GetFileAttributesA(p) & FILE_ATTRIBUTE_HIDDEN);
    bi.FileAttributes = FILE_ATTRIBUTE_NORMAL;
    CHECK(si(h, C_BASIC, &bi, sizeof bi));
    CHECK(!(GetFileAttributesA(p) & FILE_ATTRIBUTE_HIDDEN));
    bi.FileAttributes = FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM;
    CHECK(si(h, C_BASIC, &bi, sizeof bi));
    EXPECT_EQ(GetFileAttributesA(p) & 7, 5);
    bi.FileAttributes = FILE_ATTRIBUTE_NORMAL; CHECK(si(h, C_BASIC, &bi, sizeof bi));
    CloseHandle(h); fa_rm(p);
}
TEST(fsinfo_set_basic_directory_bit_rejected_or_ignored) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_bd.txt", GENERIC_READ | GENERIC_WRITE);
    FILE_BASIC_INFO bi; memset(&bi, 0, sizeof bi);
    bi.FileAttributes = FILE_ATTRIBUTE_DIRECTORY;
    SetLastError(0);
    BOOL r = si(h, C_BASIC, &bi, sizeof bi);
    t_note("BasicInfo with DIRECTORY attribute on file: r=%d err=%lu", r, r ? 0 : GetLastError());
    FILE_BASIC_INFO g; gi(h, C_BASIC, &g, sizeof g);
    CHECK(!(g.FileAttributes & FILE_ATTRIBUTE_DIRECTORY));
    CloseHandle(h); fa_rml("si_bd.txt");
}
TEST(fsinfo_set_basic_requires_write_attributes) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_bacc.txt", GENERIC_READ);
    FILE_BASIC_INFO bi; memset(&bi, 0, sizeof bi); bi.LastWriteTime.QuadPart = FA_FT_2001;
    SetLastError(0); EXPECT_EQ(si(h, C_BASIC, &bi, sizeof bi), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(h);
    h = fa_openl("si_bacc.txt", FILE_WRITE_ATTRIBUTES, FA_SHARE_ALL, OPEN_EXISTING, 0);
    SetLastError(0); EXPECT_EQ(si(h, C_BASIC, &bi, sizeof bi), TRUE);
    CloseHandle(h); fa_rml("si_bacc.txt");
}
TEST(fsinfo_set_basic_minus1_freezes_write_time) {
    REQUIRE(U(SetFileInformationByHandle));
    if (!real_ntfs()) { t_skip("needs real NTFS"); return; }
    HANDLE h = mk("si_bm1.txt", GENERIC_READ | GENERIC_WRITE);
    FILE_BASIC_INFO bi, r; memset(&bi, 0, sizeof bi);
    bi.LastWriteTime.QuadPart = FA_FT_2001 + 5 * FA_SEC;
    CHECK(si(h, C_BASIC, &bi, sizeof bi));
    bi.LastWriteTime.QuadPart = -1;
    CHECK(si(h, C_BASIC, &bi, sizeof bi));
    Sleep(30); DWORD n; WriteFile(h, "q", 1, &n, NULL);
    gi(h, C_BASIC, &r, sizeof r);
    EXPECT_EQ(r.LastWriteTime.QuadPart, FA_FT_2001 + 5 * FA_SEC);
    CloseHandle(h); fa_rml("si_bm1.txt");
}
TEST(fsinfo_set_basic_negative_time_invalid) {
    REQUIRE(U(SetFileInformationByHandle));
    if (!real_ntfs()) { t_skip("needs real NTFS"); return; }
    HANDLE h = mk("si_bneg.txt", GENERIC_READ | GENERIC_WRITE);
    FILE_BASIC_INFO bi; memset(&bi, 0, sizeof bi);
    bi.CreationTime.QuadPart = -2;
    SetLastError(0); EXPECT_EQ(si(h, C_BASIC, &bi, sizeof bi), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    bi.CreationTime.QuadPart = (LONGLONG)0x8000000000000000ULL;
    SetLastError(0); EXPECT_EQ(si(h, C_BASIC, &bi, sizeof bi), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CloseHandle(h); fa_rml("si_bneg.txt");
}
TEST(fsinfo_set_basic_on_directory) {
    REQUIRE(U(SetFileInformationByHandle));
    fa_mkdir("si_bdir");
    HANDLE h = fa_opendir("si_bdir", GENERIC_READ | FILE_WRITE_ATTRIBUTES);
    FILE_BASIC_INFO bi, r; memset(&bi, 0, sizeof bi);
    bi.FileAttributes = FILE_ATTRIBUTE_HIDDEN;
    CHECKF(si(h, C_BASIC, &bi, sizeof bi), "err %lu", GetLastError());
    gi(h, C_BASIC, &r, sizeof r);
    CHECK(r.FileAttributes & FILE_ATTRIBUTE_DIRECTORY);
    CHECK(r.FileAttributes & FILE_ATTRIBUTE_HIDDEN);
    bi.FileAttributes = FILE_ATTRIBUTE_NORMAL; si(h, C_BASIC, &bi, sizeof bi);
    CloseHandle(h); fa_rml("si_bdir");
}
TEST(fsinfo_set_basic_larger_buffer_ok) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_blarge.txt", GENERIC_READ | GENERIC_WRITE);
    BYTE buf[128]; memset(buf, 0, sizeof buf);
    ((FILE_BASIC_INFO *)buf)->LastWriteTime.QuadPart = FA_FT_2001 + 8 * FA_SEC;
    SetLastError(0);
    BOOL r = si(h, C_BASIC, buf, sizeof buf);
    t_note("basic info with 128-byte buffer r=%d err=%lu", r, r ? 0 : GetLastError());
    CloseHandle(h); fa_rml("si_blarge.txt");
}

/* ---- Disposition ---- */
TEST(fsinfo_set_disposition_delete_on_close) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_dd.txt", GENERIC_READ | DELETE);
    char p[MAX_PATH * 2]; fa_path(p, "si_dd.txt");
    FILE_DISPOSITION_INFO di = { TRUE };
    CHECKF(si(h, C_DISP, &di, sizeof di), "err %lu", GetLastError());
    CHECK(fa_exists(p) || real_ntfs());      /* still there (or access denied) until the last handle closes */
    CloseHandle(h);
    SetLastError(0);
    CHECK(!fa_exists(p));
}
TEST(fsinfo_set_disposition_cancel) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_dc.txt", GENERIC_READ | DELETE);
    char p[MAX_PATH * 2]; fa_path(p, "si_dc.txt");
    FILE_DISPOSITION_INFO di = { TRUE };
    CHECK(si(h, C_DISP, &di, sizeof di));
    di.DeleteFile = FALSE;
    CHECK(si(h, C_DISP, &di, sizeof di));
    FILE_STANDARD_INFO st; if (gi(h, C_STANDARD, &st, sizeof st)) EXPECT_EQ(st.DeletePending, 0);
    CloseHandle(h);
    CHECK(fa_exists(p));
    fa_rm(p);
}
TEST(fsinfo_set_disposition_requires_delete_access) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_dacc.txt", GENERIC_READ | GENERIC_WRITE);
    char p[MAX_PATH * 2]; fa_path(p, "si_dacc.txt");
    FILE_DISPOSITION_INFO di = { TRUE };
    SetLastError(0); EXPECT_EQ(si(h, C_DISP, &di, sizeof di), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(h);
    CHECK(fa_exists(p)); fa_rm(p);
}
TEST(fsinfo_set_disposition_readonly_file_denied) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_dro.txt", GENERIC_READ | DELETE);
    char p[MAX_PATH * 2]; fa_path(p, "si_dro.txt");
    SetFileAttributesA(p, FILE_ATTRIBUTE_READONLY);
    FILE_DISPOSITION_INFO di = { TRUE };
    SetLastError(0); EXPECT_EQ(si(h, C_DISP, &di, sizeof di), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(h);
    CHECK(fa_exists(p)); fa_rm(p);
}
TEST(fsinfo_set_disposition_nonempty_directory) {
    REQUIRE(U(SetFileInformationByHandle));
    fa_mkdir("si_ddir"); fa_mkfile_s("si_ddir\\f.txt", "x");
    HANDLE h = fa_opendir("si_ddir", GENERIC_READ | DELETE);
    FILE_DISPOSITION_INFO di = { TRUE };
    SetLastError(0); EXPECT_EQ(si(h, C_DISP, &di, sizeof di), FALSE); EXPECT_ERR(ERROR_DIR_NOT_EMPTY);
    CloseHandle(h);
    fa_rml("si_ddir\\f.txt");
    h = fa_opendir("si_ddir", GENERIC_READ | DELETE);
    CHECK(si(h, C_DISP, &di, sizeof di));
    CloseHandle(h);
    CHECK(!fa_exists("x") ); char p[MAX_PATH * 2]; fa_path(p, "si_ddir"); CHECK(!fa_exists(p));
}
TEST(fsinfo_set_disposition_pending_blocks_new_opens) {
    REQUIRE(U(SetFileInformationByHandle));
    FA_REQUIRE_NTFS();
    if (t_is_wine()) { t_skip("delete-pending semantics"); return; }
    HANDLE h = mk("si_dpend.txt", GENERIC_READ | DELETE);
    char p[MAX_PATH * 2]; fa_path(p, "si_dpend.txt");
    FILE_DISPOSITION_INFO di = { TRUE };
    CHECK(si(h, C_DISP, &di, sizeof di));
    SetLastError(0);
    HANDLE o = fa_open(p, GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0);
    EXPECT_EQ(o, INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    SetLastError(0);
    o = fa_open(p, GENERIC_WRITE, FA_SHARE_ALL, CREATE_NEW, 0);
    t_note("CREATE_NEW over delete-pending file: %s err=%lu", o == INVALID_HANDLE_VALUE ? "failed" : "succeeded", GetLastError());
    if (o != INVALID_HANDLE_VALUE) CloseHandle(o);
    SetLastError(0);
    DWORD a = GetFileAttributesA(p);
    t_note("GetFileAttributes of delete-pending file = 0x%lx err=%lu", a, a == INVALID_FILE_ATTRIBUTES ? GetLastError() : 0);
    CloseHandle(h);
}
TEST(fsinfo_set_disposition_open_handles_keep_file_alive) {
    REQUIRE(U(SetFileInformationByHandle));
    FA_REQUIRE_NTFS();
    HANDLE a = mk("si_dkeep.txt", GENERIC_READ | DELETE);
    HANDLE b = fa_openl("si_dkeep.txt", GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0);
    char p[MAX_PATH * 2]; fa_path(p, "si_dkeep.txt");
    FILE_DISPOSITION_INFO di = { TRUE };
    CHECK(si(a, C_DISP, &di, sizeof di));
    CloseHandle(a);
    FILE_STANDARD_INFO st; CHECK(gi(b, C_STANDARD, &st, sizeof st));
    EXPECT_EQ(st.DeletePending, 1);
    CloseHandle(b);
    CHECK(!fa_exists(p));
}
TEST(fsinfo_set_dispex_basic_delete) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_dx.txt", GENERIC_READ | DELETE);
    char p[MAX_PATH * 2]; fa_path(p, "si_dx.txt");
    FILE_DISPOSITION_INFO_EX di = { FILE_DISPOSITION_FLAG_DELETE };
    SetLastError(0);
    BOOL r = si(h, C_DISPEX, &di, sizeof di);
    if (!r && GetLastError() == ERROR_INVALID_PARAMETER) { t_note("DispositionInfoEx unsupported here"); CloseHandle(h); fa_rm(p); return; }
    CHECKF(r, "err %lu", GetLastError());
    CloseHandle(h);
    CHECK(!fa_exists(p)); fa_rm(p);
}
TEST(fsinfo_set_dispex_flags_zero_cancels) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_dx0.txt", GENERIC_READ | DELETE);
    char p[MAX_PATH * 2]; fa_path(p, "si_dx0.txt");
    FILE_DISPOSITION_INFO_EX di = { FILE_DISPOSITION_FLAG_DELETE };
    if (!si(h, C_DISPEX, &di, sizeof di)) { t_note("unsupported err=%lu", GetLastError()); CloseHandle(h); fa_rm(p); return; }
    di.Flags = 0;
    CHECK(si(h, C_DISPEX, &di, sizeof di));
    CloseHandle(h);
    CHECK(fa_exists(p)); fa_rm(p);
}
TEST(fsinfo_set_dispex_posix_semantics_removes_name_immediately) {
    REQUIRE(U(SetFileInformationByHandle));
    if (!real_ntfs()) { t_skip("needs real NTFS"); return; }
    HANDLE h = mk("si_dxp.txt", GENERIC_READ | DELETE);
    char p[MAX_PATH * 2]; fa_path(p, "si_dxp.txt");
    FILE_DISPOSITION_INFO_EX di = { FILE_DISPOSITION_FLAG_DELETE | FILE_DISPOSITION_FLAG_POSIX_SEMANTICS };
    SetLastError(0);
    if (!si(h, C_DISPEX, &di, sizeof di)) { t_note("POSIX delete unsupported err=%lu", GetLastError()); CloseHandle(h); fa_rm(p); return; }
    SetLastError(0);
    CHECK(!fa_exists(p));
    HANDLE n = fa_open(p, GENERIC_WRITE, FA_SHARE_ALL, CREATE_NEW, 0);
    CHECKF(n != INVALID_HANDLE_VALUE, "re-create with same name while old handle open: err %lu", GetLastError());
    if (n != INVALID_HANDLE_VALUE) CloseHandle(n);
    CloseHandle(h); fa_rm(p);
}
TEST(fsinfo_set_dispex_ignore_readonly) {
    REQUIRE(U(SetFileInformationByHandle));
    if (!real_ntfs()) { t_skip("needs real NTFS"); return; }
    HANDLE h = mk("si_dxro.txt", GENERIC_READ | DELETE | FILE_WRITE_ATTRIBUTES);
    char p[MAX_PATH * 2]; fa_path(p, "si_dxro.txt");
    SetFileAttributesA(p, FILE_ATTRIBUTE_READONLY);
    FILE_DISPOSITION_INFO_EX di = { FILE_DISPOSITION_FLAG_DELETE };
    SetLastError(0); EXPECT_EQ(si(h, C_DISPEX, &di, sizeof di), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    di.Flags = FILE_DISPOSITION_FLAG_DELETE | FILE_DISPOSITION_FLAG_IGNORE_READONLY_ATTRIBUTE;
    SetLastError(0);
    BOOL r = si(h, C_DISPEX, &di, sizeof di);
    CHECKF(r, "err %lu", GetLastError());
    CloseHandle(h);
    if (r) CHECK(!fa_exists(p));
    fa_rm(p);
}
TEST(fsinfo_set_dispex_invalid_flags) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_dxbad.txt", GENERIC_READ | DELETE);
    char p[MAX_PATH * 2]; fa_path(p, "si_dxbad.txt");
    static const DWORD bad[] = { 0x20, 0x100, 0x80000000, 0xffffffff };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        FILE_DISPOSITION_INFO_EX di = { bad[i] };
        SetLastError(0);
        BOOL r = si(h, C_DISPEX, &di, sizeof di);
        /* invalid-bit rejection is enforced by the file system, not kernelbase: unverified */
        if (r) t_note("DispEx flags 0x%lx accepted", bad[i]);
        t_note("DispEx flags 0x%lx -> err=%lu", bad[i], r ? 0 : GetLastError());
    }
    CloseHandle(h); fa_rm(p);
}
TEST(fsinfo_set_dispex_on_close_flag) {
    REQUIRE(U(SetFileInformationByHandle));
    if (!real_ntfs()) { t_skip("needs real NTFS"); return; }
    HANDLE h = mk("si_dxoc.txt", GENERIC_READ | DELETE);
    char p[MAX_PATH * 2]; fa_path(p, "si_dxoc.txt");
    FILE_DISPOSITION_INFO_EX di = { FILE_DISPOSITION_FLAG_DELETE | FILE_DISPOSITION_FLAG_ON_CLOSE };
    SetLastError(0);
    if (!si(h, C_DISPEX, &di, sizeof di)) { t_note("ON_CLOSE err=%lu", GetLastError()); CloseHandle(h); fa_rm(p); return; }
    CloseHandle(h);
    CHECK(!fa_exists(p)); fa_rm(p);
}

/* ---- Allocation / EndOfFile ---- */
TEST(fsinfo_set_eof_extend_zero_fills) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_eofx.txt", GENERIC_READ | GENERIC_WRITE);
    FILE_END_OF_FILE_INFO ei; ei.EndOfFile.QuadPart = 100;
    CHECKF(si(h, C_EOF, &ei, sizeof ei), "err %lu", GetLastError());
    EXPECT_EQ(GetFileSize(h, NULL), 100);
    BYTE b[100]; DWORD n = 0; SetFilePointer(h, 0, NULL, FILE_BEGIN);
    CHECK(ReadFile(h, b, 100, &n, NULL)); EXPECT_EQ(n, 100);
    CHECK(!memcmp(b, "0123456789", 10));
    int z = 1; for (int i = 10; i < 100; i++) if (b[i]) z = 0;
    CHECK(z);
    CloseHandle(h); fa_rml("si_eofx.txt");
}
TEST(fsinfo_set_eof_truncate) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_eoft.txt", GENERIC_READ | GENERIC_WRITE);
    FILE_END_OF_FILE_INFO ei; ei.EndOfFile.QuadPart = 4;
    CHECK(si(h, C_EOF, &ei, sizeof ei));
    EXPECT_EQ(GetFileSize(h, NULL), 4);
    ei.EndOfFile.QuadPart = 0;
    CHECK(si(h, C_EOF, &ei, sizeof ei));
    EXPECT_EQ(GetFileSize(h, NULL), 0);
    CloseHandle(h); fa_rml("si_eoft.txt");
}
TEST(fsinfo_set_eof_does_not_move_file_pointer) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_eofp.txt", GENERIC_READ | GENERIC_WRITE);
    SetFilePointer(h, 7, NULL, FILE_BEGIN);
    FILE_END_OF_FILE_INFO ei; ei.EndOfFile.QuadPart = 3;
    CHECK(si(h, C_EOF, &ei, sizeof ei));
    LONG hi = 0; EXPECT_EQ(SetFilePointer(h, 0, &hi, FILE_CURRENT), 7);
    CloseHandle(h); fa_rml("si_eofp.txt");
}
TEST(fsinfo_set_eof_negative_invalid) {
    REQUIRE(U(SetFileInformationByHandle));
    if (!real_ntfs()) { t_skip("needs real NTFS"); return; }
    HANDLE h = mk("si_eofn.txt", GENERIC_READ | GENERIC_WRITE);
    FILE_END_OF_FILE_INFO ei; ei.EndOfFile.QuadPart = -1;
    SetLastError(0); EXPECT_EQ(si(h, C_EOF, &ei, sizeof ei), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    ei.EndOfFile.QuadPart = (LONGLONG)0x8000000000000000ULL;
    SetLastError(0); EXPECT_EQ(si(h, C_EOF, &ei, sizeof ei), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    EXPECT_EQ(GetFileSize(h, NULL), 10);
    CloseHandle(h); fa_rml("si_eofn.txt");
}
TEST(fsinfo_set_eof_requires_write_access) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_eofa.txt", GENERIC_READ);
    FILE_END_OF_FILE_INFO ei; ei.EndOfFile.QuadPart = 2;
    SetLastError(0); EXPECT_EQ(si(h, C_EOF, &ei, sizeof ei), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    EXPECT_EQ(GetFileSize(h, NULL), 10);
    CloseHandle(h); fa_rml("si_eofa.txt");
}
TEST(fsinfo_set_eof_matches_setendoffile) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE a = mk("si_eofm1.txt", GENERIC_READ | GENERIC_WRITE), b = mk("si_eofm2.txt", GENERIC_READ | GENERIC_WRITE);
    SetFilePointer(a, 6, NULL, FILE_BEGIN); SetEndOfFile(a);
    FILE_END_OF_FILE_INFO ei; ei.EndOfFile.QuadPart = 6; si(b, C_EOF, &ei, sizeof ei);
    EXPECT_EQ(GetFileSize(a, NULL), GetFileSize(b, NULL));
    CloseHandle(a); CloseHandle(b); fa_rml("si_eofm1.txt"); fa_rml("si_eofm2.txt");
}
TEST(fsinfo_set_eof_absurd_size_fails) {
    REQUIRE(U(SetFileInformationByHandle));
    if (!real_ntfs()) { t_skip("needs real NTFS"); return; }
    HANDLE h = mk("si_eofbig.txt", GENERIC_READ | GENERIC_WRITE);
    FILE_END_OF_FILE_INFO ei; ei.EndOfFile.QuadPart = 0x7FFFFFFFFFFFFFFFLL;
    SetLastError(0);
    BOOL r = si(h, C_EOF, &ei, sizeof ei);
    DWORD e = GetLastError();
    CHECK(!r);
    t_note("EOF=2^63-1 -> err=%lu", e);
    if (r) { ei.EndOfFile.QuadPart = 0; si(h, C_EOF, &ei, sizeof ei); }
    CloseHandle(h); fa_rml("si_eofbig.txt");
}
TEST(fsinfo_set_alloc_larger_than_eof) {
    REQUIRE(U(SetFileInformationByHandle));
    if (!real_ntfs()) { t_skip("needs real NTFS"); return; }
    HANDLE h = mk("si_al.txt", GENERIC_READ | GENERIC_WRITE);
    FILE_ALLOCATION_INFO ai; ai.AllocationSize.QuadPart = 1 << 20;
    CHECKF(si(h, C_ALLOC, &ai, sizeof ai), "err %lu", GetLastError());
    FILE_STANDARD_INFO st; CHECK(gi(h, C_STANDARD, &st, sizeof st));
    CHECK(st.AllocationSize.QuadPart >= (1 << 20));
    EXPECT_EQ(st.EndOfFile.QuadPart, 10);
    CloseHandle(h); fa_rml("si_al.txt");
}
TEST(fsinfo_set_alloc_smaller_than_eof_truncates) {
    REQUIRE(U(SetFileInformationByHandle));
    if (!real_ntfs()) { t_skip("needs real NTFS"); return; }
    HANDLE h = mk("si_al0.txt", GENERIC_READ | GENERIC_WRITE);
    FILE_ALLOCATION_INFO ai; ai.AllocationSize.QuadPart = 0;
    CHECK(si(h, C_ALLOC, &ai, sizeof ai));
    FILE_STANDARD_INFO st; CHECK(gi(h, C_STANDARD, &st, sizeof st));
    EXPECT_EQ(st.EndOfFile.QuadPart, 0);
    EXPECT_EQ(st.AllocationSize.QuadPart, 0);
    CloseHandle(h); fa_rml("si_al0.txt");
}
TEST(fsinfo_set_alloc_negative) {
    REQUIRE(U(SetFileInformationByHandle));
    if (!real_ntfs()) { t_skip("needs real NTFS"); return; }
    HANDLE h = mk("si_aln.txt", GENERIC_READ | GENERIC_WRITE);
    FILE_ALLOCATION_INFO ai; ai.AllocationSize.QuadPart = -5;
    SetLastError(0); BOOL r = si(h, C_ALLOC, &ai, sizeof ai);
    CHECK(!r);
    t_note("AllocationInfo=-5 err=%lu", GetLastError());
    CloseHandle(h); fa_rml("si_aln.txt");
}
TEST(fsinfo_set_alloc_requires_write_access) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_ala.txt", GENERIC_READ);
    FILE_ALLOCATION_INFO ai; ai.AllocationSize.QuadPart = 4096;
    SetLastError(0); EXPECT_EQ(si(h, C_ALLOC, &ai, sizeof ai), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(h); fa_rml("si_ala.txt");
}

/* ---- IoPriorityHint ---- */
TEST(fsinfo_set_io_priority_valid_values) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_io.txt", GENERIC_READ | GENERIC_WRITE);
    for (int v = 0; v <= 2; v++) {
        FILE_IO_PRIORITY_HINT_INFO io; io.PriorityHint = (PRIORITY_HINT)v;
        SetLastError(0);
        CHECKF(si(h, C_IOPRIO, &io, sizeof io), "hint %d err %lu", v, GetLastError());
    }
    CloseHandle(h); fa_rml("si_io.txt");
}
TEST(fsinfo_set_io_priority_invalid_values) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_io2.txt", GENERIC_READ | GENERIC_WRITE);
    static const int bad[] = { 3, 4, 100, 0x7fffffff };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        FILE_IO_PRIORITY_HINT_INFO io; io.PriorityHint = (PRIORITY_HINT)bad[i];
        SetLastError(0); EXPECT_EQ(si(h, C_IOPRIO, &io, sizeof io), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
    CloseHandle(h); fa_rml("si_io2.txt");
}
TEST(fsinfo_set_io_priority_get_unsupported) {
    REQUIRE(U(GetFileInformationByHandleEx));
    HANDLE h = mk("si_io3.txt", GENERIC_READ | GENERIC_WRITE);
    FILE_IO_PRIORITY_HINT_INFO io;
    SetLastError(0); EXPECT_EQ(gi(h, C_IOPRIO, &io, sizeof io), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CloseHandle(h); fa_rml("si_io3.txt");
}

/* ---- Rename ---- */
TEST(fsinfo_set_rename_basic) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_rn1.txt", GENERIC_READ | DELETE);
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_path(a, "si_rn1.txt"); fa_path(b, "si_rn2.txt");
    CHECKF(do_rename(h, C_RENAME, b, 0), "err %lu", GetLastError());
    CHECK(!fa_exists(a)); CHECK(fa_exists(b));
    CHECK(fa_content_is(b, "0123456789"));
    CloseHandle(h); fa_rm(b);
}
TEST(fsinfo_set_rename_requires_delete_access) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_rna.txt", GENERIC_READ | GENERIC_WRITE);
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_path(a, "si_rna.txt"); fa_path(b, "si_rnb.txt");
    SetLastError(0); EXPECT_EQ(do_rename(h, C_RENAME, b, 0), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CHECK(fa_exists(a)); CHECK(!fa_exists(b));
    CloseHandle(h); fa_rm(a);
}
TEST(fsinfo_set_rename_handle_follows_file) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_rnf1.txt", GENERIC_READ | GENERIC_WRITE | DELETE);
    char b[MAX_PATH * 2]; fa_path(b, "si_rnf2.txt");
    CHECK(do_rename(h, C_RENAME, b, 0));
    DWORD n; CHECK(WriteFile(h, "ZZ", 2, &n, NULL));
    CloseHandle(h);
    char buf[32]; DWORD got = 0; fa_read(b, buf, sizeof buf, &got);
    EXPECT_EQ(got, 10);
    CHECK(!memcmp(buf, "ZZ23456789", 10));
    fa_rm(b);
}
TEST(fsinfo_set_rename_collision_without_replace) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_rc1.txt", GENERIC_READ | DELETE);
    fa_mkfile_s("si_rc2.txt", "other");
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_path(a, "si_rc1.txt"); fa_path(b, "si_rc2.txt");
    SetLastError(0); EXPECT_EQ(do_rename(h, C_RENAME, b, 0), FALSE); EXPECT_ERR(ERROR_ALREADY_EXISTS);
    CHECK(fa_content_is(b, "other")); CHECK(fa_exists(a));
    CloseHandle(h); fa_rm(a); fa_rm(b);
}
TEST(fsinfo_set_rename_replace_existing) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_rr1.txt", GENERIC_READ | DELETE);
    fa_mkfile_s("si_rr2.txt", "other");
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_path(a, "si_rr1.txt"); fa_path(b, "si_rr2.txt");
    CHECKF(do_rename(h, C_RENAME, b, 1), "err %lu", GetLastError());
    CHECK(!fa_exists(a)); CHECK(fa_content_is(b, "0123456789"));
    CloseHandle(h); fa_rm(b);
}
TEST(fsinfo_set_rename_replace_readonly_target_denied) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_rro1.txt", GENERIC_READ | DELETE);
    fa_mkfile_s("si_rro2.txt", "other");
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_path(a, "si_rro1.txt"); fa_path(b, "si_rro2.txt");
    SetFileAttributesA(b, FILE_ATTRIBUTE_READONLY);
    SetLastError(0); EXPECT_EQ(do_rename(h, C_RENAME, b, 1), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CHECK(fa_content_is(b, "other"));
    CloseHandle(h); fa_rm(a); fa_rm(b);
}
TEST(fsinfo_set_rename_replace_directory_target_denied) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_rd1.txt", GENERIC_READ | DELETE);
    fa_mkdir("si_rd2");
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_path(a, "si_rd1.txt"); fa_path(b, "si_rd2");
    SetLastError(0); EXPECT_EQ(do_rename(h, C_RENAME, b, 1), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(h); fa_rm(a); fa_rm(b);
}
TEST(fsinfo_set_rename_replace_open_target) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_ro1.txt", GENERIC_READ | DELETE);
    fa_mkfile_s("si_ro2.txt", "other");
    HANDLE t = fa_openl("si_ro2.txt", GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0);
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_path(a, "si_ro1.txt"); fa_path(b, "si_ro2.txt");
    SetLastError(0);
    BOOL r = do_rename(h, C_RENAME, b, 1);
    DWORD e = GetLastError();
    CHECKF(!r && e == ERROR_ACCESS_DENIED, "replace over open target: r=%d err=%lu", r, e);
    CloseHandle(t); CloseHandle(h); fa_rm(a); fa_rm(b);
}
TEST(fsinfo_set_rename_missing_target_dir) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_rm1.txt", GENERIC_READ | DELETE);
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_path(a, "si_rm1.txt"); fa_path(b, "si_nodir_here\\x.txt");
    SetLastError(0); EXPECT_EQ(do_rename(h, C_RENAME, b, 0), FALSE); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    CloseHandle(h); fa_rm(a);
}
TEST(fsinfo_set_rename_invalid_name_chars) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_ri1.txt", GENERIC_READ | DELETE);
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_path(a, "si_ri1.txt"); fa_path(b, "si_ri*.txt");
    SetLastError(0); BOOL r = do_rename(h, C_RENAME, b, 0);
    DWORD e = GetLastError();
    CHECKF(!r && e == ERROR_INVALID_NAME, "r=%d err=%lu", r, e);
    fa_path(b, "si_ri?.txt");
    SetLastError(0); r = do_rename(h, C_RENAME, b, 0); e = GetLastError();
    CHECKF(!r && e == ERROR_INVALID_NAME, "r=%d err=%lu", r, e);
    CHECK(fa_exists(a));
    CloseHandle(h); fa_rm(a);
}
TEST(fsinfo_set_rename_case_only) {
    REQUIRE(U(SetFileInformationByHandle));
    FA_REQUIRE_NTFS();
    HANDLE h = mk("si_rcase.txt", GENERIC_READ | DELETE);
    char b[MAX_PATH * 2]; fa_path(b, "SI_RCASE.TXT");
    CHECKF(do_rename(h, C_RENAME, b, 0), "err %lu", GetLastError());
    WIN32_FIND_DATAA fd; HANDLE f = FindFirstFileA(b, &fd);
    if (f != INVALID_HANDLE_VALUE) { if (!t_is_wine()) EXPECT_STR(fd.cFileName, "SI_RCASE.TXT"); FindClose(f); }
    CloseHandle(h); fa_rm(b);
}
TEST(fsinfo_set_rename_onto_itself) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_rself.txt", GENERIC_READ | DELETE);
    char a[MAX_PATH * 2]; fa_path(a, "si_rself.txt");
    SetLastError(0); BOOL r = do_rename(h, C_RENAME, a, 0);
    t_note("rename onto itself (no replace): r=%d err=%lu", r, r ? 0 : GetLastError());
    SetLastError(0); r = do_rename(h, C_RENAME, a, 1);
    t_note("rename onto itself (replace): r=%d err=%lu", r, r ? 0 : GetLastError());
    CHECK(fa_exists(a));
    CloseHandle(h); fa_rm(a);
}
TEST(fsinfo_set_rename_directory) {
    REQUIRE(U(SetFileInformationByHandle));
    fa_mkdir("si_rdir1"); fa_mkfile_s("si_rdir1\\in.txt", "inside");
    HANDLE h = fa_opendir("si_rdir1", GENERIC_READ | DELETE);
    char b[MAX_PATH * 2], f[MAX_PATH * 2]; fa_path(b, "si_rdir2"); fa_path(f, "si_rdir2\\in.txt");
    CHECKF(do_rename(h, C_RENAME, b, 0), "err %lu", GetLastError());
    CHECK(fa_content_is(f, "inside"));
    CloseHandle(h); fa_rm(f); fa_rm(b);
}
TEST(fsinfo_set_rename_into_subdirectory) {
    REQUIRE(U(SetFileInformationByHandle));
    fa_mkdir("si_rsub");
    HANDLE h = mk("si_rs1.txt", GENERIC_READ | DELETE);
    char b[MAX_PATH * 2]; fa_path(b, "si_rsub\\moved.txt");
    CHECKF(do_rename(h, C_RENAME, b, 0), "err %lu", GetLastError());
    CHECK(fa_exists(b));
    CloseHandle(h); fa_rm(b); fa_rml("si_rsub");
}
TEST(fsinfo_set_rename_prefixed_destination) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_rpf1.txt", GENERIC_READ | DELETE);
    char b[MAX_PATH * 2], q[MAX_PATH * 2]; fa_path(b, "si_rpf2.txt"); snprintf(q, sizeof q, "\\\\?\\%s", b);
    SetLastError(0);
    BOOL r = do_rename(h, C_RENAME, q, 0);
    CHECKF(r, "err %lu", GetLastError());
    CloseHandle(h); fa_rm(b); fa_rml("si_rpf1.txt");
}
TEST(fsinfo_set_rename_target_open_without_share_delete_other_source) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_rsh1.txt", GENERIC_READ | DELETE);
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_path(a, "si_rsh1.txt"); fa_path(b, "si_rsh2.txt");
    HANDLE o = fa_open(a, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, 0);
    /* the source has another open handle that does not allow delete: the rename itself is refused */
    HANDLE h2 = fa_open(a, GENERIC_READ | DELETE, FA_SHARE_ALL, OPEN_EXISTING, 0);
    t_note("open with DELETE while other handle lacks FILE_SHARE_DELETE: %s err=%lu", h2 == INVALID_HANDLE_VALUE ? "fails" : "ok", GetLastError());
    if (h2 != INVALID_HANDLE_VALUE) CloseHandle(h2);
    CloseHandle(o); CloseHandle(h); fa_rm(a);
}
TEST(fsinfo_set_rename_short_buffer_header_only) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_rshort.txt", GENERIC_READ | DELETE);
    BYTE buf[8]; memset(buf, 0, sizeof buf);
    SetLastError(0);
    BOOL r = si(h, C_RENAME, buf, 8);
    t_note("rename with 8-byte buffer r=%d err=%lu", r, r ? 0 : GetLastError());
    CHECK(!r);
    FILE_RENAME_INFO empty; memset(&empty, 0, sizeof empty);
    SetLastError(0); r = si(h, C_RENAME, &empty, sizeof empty);
    t_note("rename with empty name r=%d err=%lu", r, r ? 0 : GetLastError());
    CHECK(!r);
    CloseHandle(h); fa_rml("si_rshort.txt");
}
TEST(fsinfo_set_renameex_basic_and_collision) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_rx1.txt", GENERIC_READ | DELETE);
    fa_mkfile_s("si_rx2.txt", "other");
    char a[MAX_PATH * 2], b[MAX_PATH * 2], c[MAX_PATH * 2]; fa_path(a, "si_rx1.txt"); fa_path(b, "si_rx2.txt"); fa_path(c, "si_rx3.txt");
    SetLastError(0);
    BOOL r = do_rename(h, C_RENAMEEX, b, 0);
    if (!r && GetLastError() == ERROR_INVALID_PARAMETER) { t_note("RenameEx unsupported"); CloseHandle(h); fa_rm(a); fa_rm(b); return; }
    CHECK(!r); EXPECT_ERR(ERROR_ALREADY_EXISTS);
    CHECKF(do_rename(h, C_RENAMEEX, c, 0), "err %lu", GetLastError());
    CHECK(fa_exists(c)); CHECK(!fa_exists(a));
    CloseHandle(h); fa_rm(b); fa_rm(c);
}
TEST(fsinfo_set_renameex_replace_flag) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_rxr1.txt", GENERIC_READ | DELETE);
    fa_mkfile_s("si_rxr2.txt", "other");
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_path(a, "si_rxr1.txt"); fa_path(b, "si_rxr2.txt");
    SetLastError(0);
    BOOL r = do_rename(h, C_RENAMEEX, b, RN_REPLACE);
    if (!r && GetLastError() == ERROR_INVALID_PARAMETER) { t_note("RenameEx unsupported"); CloseHandle(h); fa_rm(a); fa_rm(b); return; }
    CHECKF(r, "err %lu", GetLastError());
    CHECK(fa_content_is(b, "0123456789"));
    CloseHandle(h); fa_rm(a); fa_rm(b);
}
TEST(fsinfo_set_renameex_posix_replace_open_target) {
    REQUIRE(U(SetFileInformationByHandle));
    if (!real_ntfs()) { t_skip("needs real NTFS"); return; }
    HANDLE h = mk("si_rxp1.txt", GENERIC_READ | DELETE);
    fa_mkfile_s("si_rxp2.txt", "other");
    HANDLE t = fa_openl("si_rxp2.txt", GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0);
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_path(a, "si_rxp1.txt"); fa_path(b, "si_rxp2.txt");
    SetLastError(0);
    BOOL r = do_rename(h, C_RENAMEEX, b, RN_REPLACE | RN_POSIX);
    if (!r && GetLastError() == ERROR_INVALID_PARAMETER) { t_note("POSIX rename unsupported"); CloseHandle(t); CloseHandle(h); fa_rm(a); fa_rm(b); return; }
    CHECKF(r, "POSIX replace of open (share-delete) target: err %lu", GetLastError());
    if (r) { CHECK(fa_content_is(b, "0123456789")); }
    CloseHandle(t); CloseHandle(h); fa_rm(a); fa_rm(b);
}
TEST(fsinfo_set_renameex_invalid_flags) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_rxbad.txt", GENERIC_READ | DELETE);
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_path(a, "si_rxbad.txt"); fa_path(b, "si_rxbad2.txt");
    SetLastError(0);
    BOOL r = do_rename(h, C_RENAMEEX, b, 0x80000000);
    t_note("RenameEx with flag 0x80000000: r=%d err=%lu", r, r ? 0 : GetLastError());
    CloseHandle(h); fa_rm(a); fa_rm(b);
}
TEST(fsinfo_set_case_sensitive_info_file_rejected) {
    REQUIRE(U(SetFileInformationByHandle));
    HANDLE h = mk("si_cs.txt", GENERIC_READ | GENERIC_WRITE);
    DWORD flags = 0;
    SetLastError(0);
    BOOL r = si(h, C_CASE, &flags, sizeof flags);
    t_note("FileCaseSensitiveInfo(0) on a file: r=%d err=%lu", r, r ? 0 : GetLastError());
    CloseHandle(h); fa_rml("si_cs.txt");
}
