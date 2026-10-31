/* GetFileInformationByHandleEx for every FILE_INFO_BY_HANDLE_CLASS. */
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
#define C_COMPRESSION 8
#define C_ATTRTAG 9
#define C_IDBOTH 10
#define C_IDBOTH_R 11
#define C_IOPRIO 12
#define C_REMOTE 13
#define C_FULLDIR 14
#define C_FULLDIR_R 15
#define C_STORAGE 16
#define C_ALIGN 17
#define C_ID 18
#define C_EXTD 19
#define C_EXTD_R 20
#define C_DISPEX 21
#define C_RENAMEEX 22
#define C_CASE 23
#define C_NORMNAME 24

static BOOL gi(HANDLE h, DWORD cls, void *b, DWORD n) {
    return U(GetFileInformationByHandleEx)(h, (FILE_INFO_BY_HANDLE_CLASS)cls, b, n);
}
static HANDLE mkf(const char *leaf) { fa_mkfile_s(leaf, "0123456789"); return fa_openl(leaf, GENERIC_READ | GENERIC_WRITE, FA_SHARE_ALL, OPEN_EXISTING, 0); }

/* minimum buffer sizes per class (Win10 kernelbase) */
static const struct { DWORD cls; DWORD min; const char *n; } MINSZ[] = {
    { C_BASIC, 0x28, "Basic" }, { C_STANDARD, 0x18, "Standard" }, { C_NAME, 8, "Name" }, { C_STREAM, 0x20, "Stream" },
    { C_COMPRESSION, 0x10, "Compression" }, { C_ATTRTAG, 8, "AttrTag" }, { C_IDBOTH, 0x70, "IdBoth" }, { C_IDBOTH_R, 0x70, "IdBothR" },
    { C_REMOTE, 0x74, "Remote" }, { C_FULLDIR, 0x48, "FullDir" }, { C_FULLDIR_R, 0x48, "FullDirR" }, { C_STORAGE, 0x1c, "Storage" },
    { C_ALIGN, 4, "Align" }, { C_ID, 0x18, "Id" }, { C_EXTD, 0x60, "Extd" }, { C_EXTD_R, 0x60, "ExtdR" },
    { C_CASE, 4, "Case" }, { C_NORMNAME, 8, "NormName" },
};

TEST(finfo_get_undersized_buffer_is_bad_length) {
    REQUIRE(U(GetFileInformationByHandleEx));
    HANDLE h = mkf("gi_small.txt");
    BYTE buf[512]; memset(buf, 0xab, sizeof buf);
    for (unsigned i = 0; i < sizeof MINSZ / sizeof *MINSZ; i++) {
        SetLastError(0);
        BOOL r = gi(h, MINSZ[i].cls, buf, MINSZ[i].min - 1);
        CHECKF(!r && GetLastError() == ERROR_BAD_LENGTH, "%s min-1: r=%d err=%lu", MINSZ[i].n, r, GetLastError());
        SetLastError(0);
        r = gi(h, MINSZ[i].cls, buf, 0);
        CHECKF(!r && GetLastError() == ERROR_BAD_LENGTH, "%s size 0: r=%d err=%lu", MINSZ[i].n, r, GetLastError());
        SetLastError(0);
        r = gi(h, MINSZ[i].cls, NULL, 0);
        CHECKF(!r && GetLastError() == ERROR_BAD_LENGTH, "%s NULL/0: r=%d err=%lu", MINSZ[i].n, r, GetLastError());
    }
    CloseHandle(h); fa_rml("gi_small.txt");
}
TEST(finfo_get_size_checked_before_handle) {
    REQUIRE(U(GetFileInformationByHandleEx));
    BYTE buf[256];
    SetLastError(0); EXPECT_EQ(gi(INVALID_HANDLE_VALUE, C_BASIC, buf, 4), FALSE); EXPECT_ERR(ERROR_BAD_LENGTH);
    SetLastError(0); EXPECT_EQ(gi(NULL, C_STANDARD, buf, 1), FALSE); EXPECT_ERR(ERROR_BAD_LENGTH);
}
TEST(finfo_get_invalid_handle) {
    REQUIRE(U(GetFileInformationByHandleEx));
    BYTE buf[512];
    static const DWORD cls[] = { C_BASIC, C_STANDARD, C_NAME, C_STREAM, C_COMPRESSION, C_ATTRTAG, C_ID, C_STORAGE, C_ALIGN };
    for (unsigned i = 0; i < sizeof cls / sizeof *cls; i++) {
        SetLastError(0); EXPECT_EQ(gi(INVALID_HANDLE_VALUE, cls[i], buf, sizeof buf), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
        SetLastError(0); EXPECT_EQ(gi(NULL, cls[i], buf, sizeof buf), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
}
TEST(finfo_get_wrong_handle_type) {
    REQUIRE(U(GetFileInformationByHandleEx));
    HANDLE e = CreateEventA(NULL, FALSE, FALSE, NULL);
    BYTE buf[512];
    SetLastError(0); EXPECT_EQ(gi(e, C_BASIC, buf, sizeof buf), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SetLastError(0); EXPECT_EQ(gi(e, C_STANDARD, buf, sizeof buf), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SetLastError(0); EXPECT_EQ(gi(e, C_NAME, buf, sizeof buf), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(e);
}
TEST(finfo_get_closed_handle) {
    REQUIRE(U(GetFileInformationByHandleEx));
    HANDLE h = mkf("gi_closed.txt"); CloseHandle(h);
    BYTE buf[512];
    SetLastError(0); EXPECT_EQ(gi(h, C_BASIC, buf, sizeof buf), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    fa_rml("gi_closed.txt");
}
TEST(finfo_get_invalid_classes) {
    REQUIRE(U(GetFileInformationByHandleEx));
    HANDLE h = mkf("gi_badcls.txt");
    static const DWORD bad[] = { C_RENAME, C_DISP, C_ALLOC, C_EOF, C_IOPRIO, C_DISPEX, C_RENAMEEX, 25, 26, 27, 100, 0x7fffffff, 0xffffffff, 0x80000000 };
    BYTE buf[1024]; memset(buf, 0xab, sizeof buf);
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SetLastError(0);
        BOOL r = gi(h, bad[i], buf, sizeof buf);
        CHECKF(!r && GetLastError() == ERROR_INVALID_PARAMETER, "class %lu r=%d err=%lu", bad[i], r, GetLastError());
    }
    CloseHandle(h); fa_rml("gi_badcls.txt");
}
TEST(finfo_get_invalid_class_checked_before_buffer) {
    REQUIRE(U(GetFileInformationByHandleEx));
    SetLastError(0); EXPECT_EQ(gi(INVALID_HANDLE_VALUE, C_RENAME, NULL, 0), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SetLastError(0); EXPECT_EQ(gi(INVALID_HANDLE_VALUE, 77, NULL, 0), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}

TEST(finfo_get_basic_file) {
    REQUIRE(U(GetFileInformationByHandleEx));
    HANDLE h = mkf("gi_basic.txt");
    FILE_BASIC_INFO bi; memset(&bi, 0xcc, sizeof bi);
    CHECK(gi(h, C_BASIC, &bi, sizeof bi));
    char p[MAX_PATH * 2]; fa_path(p, "gi_basic.txt");
    EXPECT_EQ(bi.FileAttributes, GetFileAttributesA(p));
    FILETIME c, a, w; GetFileTime(h, &c, &a, &w);
    EXPECT_EQ((ULONGLONG)bi.CreationTime.QuadPart, fa_ft64(c));
    EXPECT_EQ((ULONGLONG)bi.LastAccessTime.QuadPart, fa_ft64(a));
    EXPECT_EQ((ULONGLONG)bi.LastWriteTime.QuadPart, fa_ft64(w));
    CHECK((ULONGLONG)bi.ChangeTime.QuadPart >= (ULONGLONG)bi.LastWriteTime.QuadPart);
    CloseHandle(h); fa_rm(p);
}
TEST(finfo_get_basic_layout) {
    EXPECT_EQ(sizeof(FILE_BASIC_INFO), 40);
    EXPECT_EQ(offsetof(FILE_BASIC_INFO, FileAttributes), 32);
    EXPECT_EQ(sizeof(FILE_STANDARD_INFO), 24);
    EXPECT_EQ(offsetof(FILE_STANDARD_INFO, EndOfFile), 8);
    EXPECT_EQ(offsetof(FILE_STANDARD_INFO, NumberOfLinks), 16);
    EXPECT_EQ(sizeof(FILE_NAME_INFO), 8);
    EXPECT_EQ(sizeof(FILE_COMPRESSION_INFO), 16);
    EXPECT_EQ(sizeof(FILE_ATTRIBUTE_TAG_INFO), 8);
    EXPECT_EQ(sizeof(FILE_ID_INFO), 24);
    EXPECT_EQ(sizeof(FILE_STORAGE_INFO), 28);
    EXPECT_EQ(sizeof(FILE_ALIGNMENT_INFO), 4);
    EXPECT_EQ(sizeof(FILE_ID_BOTH_DIR_INFO), 112);
    EXPECT_EQ(sizeof(FILE_FULL_DIR_INFO), 72);
    EXPECT_EQ(sizeof(FILE_ID_EXTD_DIR_INFO), 96);
    EXPECT_EQ(sizeof(FILE_REMOTE_PROTOCOL_INFO), 116);
    EXPECT_EQ(sizeof(FILE_RENAME_INFO), 24);
    EXPECT_EQ(sizeof(FILE_STREAM_INFO), 32);
}
TEST(finfo_get_basic_directory) {
    REQUIRE(U(GetFileInformationByHandleEx));
    fa_mkdir("gi_bdir");
    HANDLE h = fa_opendir("gi_bdir", GENERIC_READ);
    FILE_BASIC_INFO bi; CHECK(gi(h, C_BASIC, &bi, sizeof bi));
    CHECK(bi.FileAttributes & FILE_ATTRIBUTE_DIRECTORY);
    CloseHandle(h); fa_rml("gi_bdir");
}
TEST(finfo_get_basic_larger_buffer_ok) {
    REQUIRE(U(GetFileInformationByHandleEx));
    HANDLE h = mkf("gi_big.txt");
    BYTE buf[256]; memset(buf, 0xee, sizeof buf);
    CHECK(gi(h, C_BASIC, buf, sizeof buf));
    CHECK(buf[100] == 0xee && buf[255] == 0xee);
    CloseHandle(h); fa_rml("gi_big.txt");
}
TEST(finfo_get_basic_requires_read_attributes) {
    REQUIRE(U(GetFileInformationByHandleEx));
    HANDLE h = mkf("gi_noacc.txt"), d = NULL;
    if (!DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(), &d, SYNCHRONIZE, FALSE, 0)) { t_skip("DuplicateHandle with reduced access failed"); CloseHandle(h); return; }
    FILE_BASIC_INFO bi;
    SetLastError(0); EXPECT_EQ(gi(d, C_BASIC, &bi, sizeof bi), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(d); CloseHandle(h); fa_rml("gi_noacc.txt");
}
TEST(finfo_get_standard_file) {
    REQUIRE(U(GetFileInformationByHandleEx));
    HANDLE h = mkf("gi_std.txt");
    FILE_STANDARD_INFO si; memset(&si, 0xcc, sizeof si);
    CHECK(gi(h, C_STANDARD, &si, sizeof si));
    EXPECT_EQ(si.EndOfFile.QuadPart, 10);
    CHECK(si.AllocationSize.QuadPart >= 10);
    EXPECT_EQ(si.NumberOfLinks, 1);
    EXPECT_EQ(si.DeletePending, 0);
    EXPECT_EQ(si.Directory, 0);
    BY_HANDLE_FILE_INFORMATION bi; GetFileInformationByHandle(h, &bi);
    EXPECT_EQ(bi.nNumberOfLinks, si.NumberOfLinks);
    EXPECT_EQ(bi.nFileSizeLow, 10);
    CloseHandle(h); fa_rml("gi_std.txt");
}
TEST(finfo_get_standard_directory) {
    REQUIRE(U(GetFileInformationByHandleEx));
    fa_mkdir("gi_sdir");
    HANDLE h = fa_opendir("gi_sdir", GENERIC_READ);
    FILE_STANDARD_INFO si; CHECK(gi(h, C_STANDARD, &si, sizeof si));
    EXPECT_EQ(si.Directory, 1);
    EXPECT_EQ(si.EndOfFile.QuadPart, 0);
    CloseHandle(h); fa_rml("gi_sdir");
}
TEST(finfo_get_standard_tracks_writes) {
    REQUIRE(U(GetFileInformationByHandleEx));
    HANDLE h = mkf("gi_trk.txt");
    FILE_STANDARD_INFO si; DWORD n;
    SetFilePointer(h, 0, NULL, FILE_END);
    WriteFile(h, "abcde", 5, &n, NULL);
    CHECK(gi(h, C_STANDARD, &si, sizeof si)); EXPECT_EQ(si.EndOfFile.QuadPart, 15);
    SetFilePointer(h, 3, NULL, FILE_BEGIN); SetEndOfFile(h);
    CHECK(gi(h, C_STANDARD, &si, sizeof si)); EXPECT_EQ(si.EndOfFile.QuadPart, 3);
    CloseHandle(h); fa_rml("gi_trk.txt");
}
TEST(finfo_get_standard_delete_pending_and_links) {
    REQUIRE(U(GetFileInformationByHandleEx)); REQUIRE(U(SetFileInformationByHandle));
    FA_REQUIRE_NTFS();
    fa_mkfile_s("gi_dp.txt", "x");
    HANDLE h = fa_openl("gi_dp.txt", GENERIC_READ | DELETE, FA_SHARE_ALL, OPEN_EXISTING, 0);
    FILE_DISPOSITION_INFO di = { TRUE };
    FILE_STANDARD_INFO si;
    CHECK(U(SetFileInformationByHandle)(h, (FILE_INFO_BY_HANDLE_CLASS)C_DISP, &di, sizeof di));
    CHECK(gi(h, C_STANDARD, &si, sizeof si));
    EXPECT_EQ(si.DeletePending, 1);
    CloseHandle(h);
    fa_rml("gi_dp.txt");
}
TEST(finfo_get_standard_hardlink_count) {
    REQUIRE(U(GetFileInformationByHandleEx));
    FA_REQUIRE_NTFS();
    HANDLE h = mkf("gi_hl1.txt");
    char a[MAX_PATH * 2], b[MAX_PATH * 2]; fa_path(a, "gi_hl1.txt"); fa_path(b, "gi_hl2.txt");
    if (!CreateHardLinkA(b, a, NULL)) { t_skip("hard links unsupported"); CloseHandle(h); fa_rm(a); return; }
    FILE_STANDARD_INFO si; CHECK(gi(h, C_STANDARD, &si, sizeof si));
    EXPECT_EQ(si.NumberOfLinks, 2);
    fa_rm(b);
    CHECK(gi(h, C_STANDARD, &si, sizeof si));
    EXPECT_EQ(si.NumberOfLinks, 1);
    CloseHandle(h); fa_rm(a);
}
TEST(finfo_get_standard_allocation_rounding) {
    REQUIRE(U(GetFileInformationByHandleEx));
    FA_REQUIRE_NTFS();
    char p[MAX_PATH * 2]; fa_path(p, "gi_alloc.bin");
    BYTE *z = calloc(1, 10000); fa_mkfile(p, z, 10000); free(z);
    HANDLE h = fa_open(p, GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0);
    FILE_STANDARD_INFO si; CHECK(gi(h, C_STANDARD, &si, sizeof si));
    EXPECT_EQ(si.EndOfFile.QuadPart, 10000);
    CHECK(si.AllocationSize.QuadPart >= 10000);
    CHECK(si.AllocationSize.QuadPart % 512 == 0);
    CloseHandle(h); fa_rm(p);
}
TEST(finfo_get_name_file) {
    REQUIRE(U(GetFileInformationByHandleEx));
    HANDLE h = mkf("gi_name.txt");
    BYTE buf[2048]; FILE_NAME_INFO *ni = (FILE_NAME_INFO *)buf;
    memset(buf, 0xcc, sizeof buf);
    CHECK(gi(h, C_NAME, buf, sizeof buf));
    CHECK(ni->FileNameLength > 0 && ni->FileNameLength % 2 == 0);
    CHECK(ni->FileName[0] == L'\\');
    WCHAR want[MAX_PATH * 2]; fa_wpath(want, "gi_name.txt");
    /* name is volume-relative: tail of the full path */
    int wl = lstrlenW(want), nl = ni->FileNameLength / 2;
    CHECK(nl <= wl);
    CHECK(!_wcsnicmp(ni->FileName, want + wl - nl, nl));
    CHECK(ni->FileName[nl - 1] == L't');
    CloseHandle(h); fa_rml("gi_name.txt");
}
TEST(finfo_get_name_minimum_buffer_truncates) {
    REQUIRE(U(GetFileInformationByHandleEx));
    HANDLE h = mkf("gi_name_trunc.txt");
    BYTE buf[8]; memset(buf, 0, sizeof buf);
    SetLastError(0);
    BOOL r = gi(h, C_NAME, buf, 8);
    DWORD e = GetLastError();
    FILE_NAME_INFO *ni = (FILE_NAME_INFO *)buf;
    CHECKF(!r && e == ERROR_MORE_DATA, "r=%d err=%lu", r, e);
    t_note("truncated name length reports %lu bytes", ni->FileNameLength);
    CHECK(ni->FileNameLength > 4);
    CloseHandle(h); fa_rml("gi_name_trunc.txt");
}
TEST(finfo_get_name_buffer_exact_size) {
    REQUIRE(U(GetFileInformationByHandleEx));
    HANDLE h = mkf("gi_name_ex.txt");
    BYTE big[2048]; FILE_NAME_INFO *ni = (FILE_NAME_INFO *)big;
    CHECK(gi(h, C_NAME, big, sizeof big));
    DWORD need = 4 + ni->FileNameLength;
    BYTE *b = calloc(1, need);
    SetLastError(0);
    CHECK(gi(h, C_NAME, b, need));
    CHECK(((FILE_NAME_INFO *)b)->FileNameLength == ni->FileNameLength);
    SetLastError(0);
    BOOL r = gi(h, C_NAME, b, need - 2);
    CHECKF(!r && GetLastError() == ERROR_MORE_DATA, "r=%d err=%lu", r, GetLastError());
    free(b);
    CloseHandle(h); fa_rml("gi_name_ex.txt");
}
TEST(finfo_get_name_directory_and_root) {
    REQUIRE(U(GetFileInformationByHandleEx));
    fa_mkdir("gi_ndir");
    HANDLE h = fa_opendir("gi_ndir", GENERIC_READ);
    BYTE buf[2048]; FILE_NAME_INFO *ni = (FILE_NAME_INFO *)buf;
    CHECK(gi(h, C_NAME, buf, sizeof buf));
    CHECK(ni->FileName[0] == L'\\');
    CHECK(ni->FileName[ni->FileNameLength / 2 - 1] == L'r');   /* ...gi_ndir */
    CloseHandle(h); fa_rml("gi_ndir");
    char root[MAX_PATH], vol[MAX_PATH]; fa_path(root, ""); GetVolumePathNameA(root, vol, sizeof vol);
    h = CreateFileA(vol, GENERIC_READ, FA_SHARE_ALL, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        CHECK(gi(h, C_NAME, buf, sizeof buf));
        EXPECT_EQ(ni->FileNameLength, 2);
        EXPECT_EQ(ni->FileName[0], L'\\');
        CloseHandle(h);
    }
}
TEST(finfo_get_name_after_rename) {
    REQUIRE(U(GetFileInformationByHandleEx));
    char a[MAX_PATH * 2], b[MAX_PATH * 2];
    HANDLE h = fa_openl("gi_rn_a.txt", GENERIC_READ, FA_SHARE_ALL, CREATE_ALWAYS, 0);
    CloseHandle(h);
    h = fa_openl("gi_rn_a.txt", GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0);
    fa_path(a, "gi_rn_a.txt"); fa_path(b, "gi_rn_b.txt");
    CHECK(MoveFileA(a, b));
    BYTE buf[2048]; FILE_NAME_INFO *ni = (FILE_NAME_INFO *)buf;
    CHECK(gi(h, C_NAME, buf, sizeof buf));
    CHECK(ni->FileName[ni->FileNameLength / 2 - 5] == L'b');
    CloseHandle(h); fa_rm(b);
}
TEST(finfo_get_stream_main_stream) {
    REQUIRE(U(GetFileInformationByHandleEx));
    FA_REQUIRE_STREAMS();
    HANDLE h = mkf("gi_strm.txt");
    BYTE buf[1024]; FILE_STREAM_INFO *si = (FILE_STREAM_INFO *)buf;
    memset(buf, 0, sizeof buf);
    CHECK(gi(h, C_STREAM, buf, sizeof buf));
    CHECK(si->StreamNameLength == 14);        /* "::$DATA" */
    CHECK(!wcsncmp(si->StreamName, L"::$DATA", 7));
    EXPECT_EQ(si->StreamSize.QuadPart, 10);
    CHECK(si->StreamAllocationSize.QuadPart >= 10);
    EXPECT_EQ(si->NextEntryOffset, 0);
    CloseHandle(h); fa_rml("gi_strm.txt");
}
TEST(finfo_get_stream_with_ads_chain) {
    REQUIRE(U(GetFileInformationByHandleEx));
    FA_REQUIRE_STREAMS();
    HANDLE h = mkf("gi_strm2.txt");
    char p[MAX_PATH * 2], q[MAX_PATH * 2]; fa_path(p, "gi_strm2.txt");
    snprintf(q, sizeof q, "%s:alpha", p); fa_mkfile(q, "12", 2);
    snprintf(q, sizeof q, "%s:beta", p); fa_mkfile(q, "12345", 5);
    BYTE buf[2048]; memset(buf, 0, sizeof buf);
    CHECK(gi(h, C_STREAM, buf, sizeof buf));
    int n = 0, seen_alpha = 0, seen_beta = 0, seen_main = 0; DWORD off = 0;
    for (;;) {
        FILE_STREAM_INFO *si = (FILE_STREAM_INFO *)(buf + off);
        n++;
        if (!wcsncmp(si->StreamName, L"::$DATA", 7)) seen_main = 1;
        if (!wcsncmp(si->StreamName, L":alpha:$DATA", 12)) { seen_alpha = 1; EXPECT_EQ(si->StreamSize.QuadPart, 2); }
        if (!wcsncmp(si->StreamName, L":beta:$DATA", 11)) { seen_beta = 1; EXPECT_EQ(si->StreamSize.QuadPart, 5); }
        if (!si->NextEntryOffset) break;
        off += si->NextEntryOffset;
        if (off > sizeof buf - 32) break;
    }
    EXPECT_EQ(n, 3); CHECK(seen_main && seen_alpha && seen_beta);
    CloseHandle(h); fa_rm(p);
}
TEST(finfo_get_stream_small_buffer) {
    REQUIRE(U(GetFileInformationByHandleEx));
    FA_REQUIRE_STREAMS();
    HANDLE h = mkf("gi_strm3.txt");
    BYTE buf[0x20]; memset(buf, 0, sizeof buf);
    SetLastError(0);
    BOOL r = gi(h, C_STREAM, buf, 0x20);
    t_note("stream info with 0x20 bytes: r=%d err=%lu", r, r ? 0 : GetLastError());
    CHECK(!r);
    if (!r) CHECK(GetLastError() == ERROR_MORE_DATA);
    CloseHandle(h); fa_rml("gi_strm3.txt");
}
TEST(finfo_get_stream_directory_without_streams) {
    REQUIRE(U(GetFileInformationByHandleEx));
    FA_REQUIRE_STREAMS();
    fa_mkdir("gi_sdir2");
    HANDLE h = fa_opendir("gi_sdir2", GENERIC_READ);
    BYTE buf[512];
    SetLastError(0);
    BOOL r = gi(h, C_STREAM, buf, sizeof buf);
    DWORD e = GetLastError();
    CHECKF(!r && e == ERROR_HANDLE_EOF, "r=%d err=%lu", r, e);
    CloseHandle(h); fa_rml("gi_sdir2");
}
TEST(finfo_get_compression_uncompressed) {
    REQUIRE(U(GetFileInformationByHandleEx));
    FA_REQUIRE_NTFS();
    HANDLE h = mkf("gi_comp.txt");
    FILE_COMPRESSION_INFO ci; memset(&ci, 0xcc, sizeof ci);
    CHECK(gi(h, C_COMPRESSION, &ci, sizeof ci));
    EXPECT_EQ(ci.CompressionFormat, 0);
    EXPECT_EQ(ci.CompressionUnitShift, 0);
    EXPECT_EQ(ci.ChunkShift, 0);
    EXPECT_EQ(ci.ClusterShift, 0);
    EXPECT_EQ(ci.CompressedFileSize.QuadPart, 10);
    CloseHandle(h); fa_rml("gi_comp.txt");
}
TEST(finfo_get_compression_directory) {
    REQUIRE(U(GetFileInformationByHandleEx));
    fa_mkdir("gi_cdir");
    HANDLE h = fa_opendir("gi_cdir", GENERIC_READ);
    FILE_COMPRESSION_INFO ci; BOOL r = gi(h, C_COMPRESSION, &ci, sizeof ci);
    t_note("compression info on dir: r=%d err=%lu", r, r ? 0 : GetLastError());
    CloseHandle(h); fa_rml("gi_cdir");
}
TEST(finfo_get_attribute_tag_plain) {
    REQUIRE(U(GetFileInformationByHandleEx));
    HANDLE h = mkf("gi_tag.txt");
    FILE_ATTRIBUTE_TAG_INFO ti; memset(&ti, 0xcc, sizeof ti);
    CHECK(gi(h, C_ATTRTAG, &ti, sizeof ti));
    char p[MAX_PATH * 2]; fa_path(p, "gi_tag.txt");
    EXPECT_EQ(ti.FileAttributes, GetFileAttributesA(p));
    EXPECT_EQ(ti.ReparseTag, 0);
    CloseHandle(h); fa_rm(p);
}
TEST(finfo_get_attribute_tag_dir_flags) {
    REQUIRE(U(GetFileInformationByHandleEx));
    fa_mkdir("gi_tdir");
    HANDLE h = fa_opendir("gi_tdir", GENERIC_READ);
    FILE_ATTRIBUTE_TAG_INFO ti; CHECK(gi(h, C_ATTRTAG, &ti, sizeof ti));
    CHECK(ti.FileAttributes & FILE_ATTRIBUTE_DIRECTORY);
    EXPECT_EQ(ti.ReparseTag, 0);
    CloseHandle(h); fa_rml("gi_tdir");
}
TEST(finfo_get_id_info_matches_byhandle) {
    REQUIRE(U(GetFileInformationByHandleEx));
    FA_REQUIRE_NTFS();
    HANDLE h = mkf("gi_id.txt");
    FILE_ID_INFO id; memset(&id, 0xcc, sizeof id);
    CHECK(gi(h, C_ID, &id, sizeof id));
    BY_HANDLE_FILE_INFORMATION bi; CHECK(GetFileInformationByHandle(h, &bi));
    EXPECT_EQ((DWORD)id.VolumeSerialNumber, bi.dwVolumeSerialNumber);
    ULONGLONG idx = ((ULONGLONG)bi.nFileIndexHigh << 32) | bi.nFileIndexLow;
    ULONGLONG lo; memcpy(&lo, id.FileId.Identifier, 8);
    EXPECT_EQ(lo, idx);
    CHECK(idx != 0);
    CloseHandle(h); fa_rml("gi_id.txt");
}
TEST(finfo_get_id_info_stable_and_unique) {
    REQUIRE(U(GetFileInformationByHandleEx));
    FA_REQUIRE_NTFS();
    HANDLE a = mkf("gi_id1.txt"), b = mkf("gi_id2.txt"), a2 = fa_openfile("gi_id1.txt");
    FILE_ID_INFO ia, ib, ia2;
    CHECK(gi(a, C_ID, &ia, sizeof ia)); CHECK(gi(b, C_ID, &ib, sizeof ib)); CHECK(gi(a2, C_ID, &ia2, sizeof ia2));
    CHECK(!memcmp(&ia, &ia2, sizeof ia));
    CHECK(memcmp(&ia.FileId, &ib.FileId, 16) != 0);
    EXPECT_EQ(ia.VolumeSerialNumber, ib.VolumeSerialNumber);
    CloseHandle(a); CloseHandle(b); CloseHandle(a2); fa_rml("gi_id1.txt"); fa_rml("gi_id2.txt");
}
TEST(finfo_get_storage_info) {
    REQUIRE(U(GetFileInformationByHandleEx));
    HANDLE h = mkf("gi_stor.txt");
    FILE_STORAGE_INFO st; memset(&st, 0xcc, sizeof st);
    BOOL r = gi(h, C_STORAGE, &st, sizeof st);
    if (!r) { t_note("FileStorageInfo failed err=%lu", GetLastError()); CloseHandle(h); return; }
    CHECK(st.LogicalBytesPerSector >= 512 && (st.LogicalBytesPerSector & (st.LogicalBytesPerSector - 1)) == 0);
    CHECK(st.PhysicalBytesPerSectorForAtomicity >= st.LogicalBytesPerSector || st.PhysicalBytesPerSectorForAtomicity == 0);
    CHECK(st.PhysicalBytesPerSectorForPerformance == 0 || (st.PhysicalBytesPerSectorForPerformance & (st.PhysicalBytesPerSectorForPerformance - 1)) == 0);
    t_note("storage: logical=%lu atomic=%lu perf=%lu effective=%lu flags=0x%lx", st.LogicalBytesPerSector, st.PhysicalBytesPerSectorForAtomicity, st.PhysicalBytesPerSectorForPerformance, st.FileSystemEffectivePhysicalBytesPerSectorForAtomicity, st.Flags);
    CloseHandle(h); fa_rml("gi_stor.txt");
}
TEST(finfo_get_alignment_info) {
    REQUIRE(U(GetFileInformationByHandleEx));
    HANDLE h = mkf("gi_align.txt");
    FILE_ALIGNMENT_INFO ai; memset(&ai, 0xcc, sizeof ai);
    CHECK(gi(h, C_ALIGN, &ai, sizeof ai));
    CHECK(ai.AlignmentRequirement <= 0x1ff);
    CloseHandle(h); fa_rml("gi_align.txt");
}
TEST(finfo_get_remote_protocol_local_file) {
    REQUIRE(U(GetFileInformationByHandleEx));
    HANDLE h = mkf("gi_remote.txt");
    FILE_REMOTE_PROTOCOL_INFO ri; memset(&ri, 0, sizeof ri);
    SetLastError(0);
    BOOL r = gi(h, C_REMOTE, &ri, sizeof ri);
    DWORD e = GetLastError();
    t_note("FileRemoteProtocolInfo on local file: r=%d err=%lu", r, r ? 0 : e);
    CHECK(!r);
    SetLastError(0);
    r = gi(h, C_REMOTE, &ri, sizeof ri - 1);
    EXPECT_EQ(r, FALSE); EXPECT_ERR(ERROR_BAD_LENGTH);
    CloseHandle(h); fa_rml("gi_remote.txt");
}
TEST(finfo_get_case_sensitive_info) {
    REQUIRE(U(GetFileInformationByHandleEx));
    fa_mkdir("gi_cs");
    HANDLE h = fa_opendir("gi_cs", GENERIC_READ);
    DWORD flags = 0xcccccccc;
    SetLastError(0);
    BOOL r = gi(h, C_CASE, &flags, sizeof flags);
    t_note("FileCaseSensitiveInfo on dir: r=%d flags=0x%lx err=%lu", r, flags, r ? 0 : GetLastError());
    if (r) EXPECT_EQ(flags & ~1u, 0);
    CloseHandle(h); fa_rml("gi_cs");
    h = mkf("gi_cs_f.txt");
    flags = 0xcccccccc; SetLastError(0);
    r = gi(h, C_CASE, &flags, sizeof flags);
    t_note("FileCaseSensitiveInfo on file: r=%d err=%lu", r, r ? 0 : GetLastError());
    CloseHandle(h); fa_rml("gi_cs_f.txt");
}
TEST(finfo_get_normalized_name) {
    REQUIRE(U(GetFileInformationByHandleEx));
    HANDLE h = mkf("gi_norm_long_name_here.txt");
    BYTE a[2048], b[2048]; FILE_NAME_INFO *na = (FILE_NAME_INFO *)a, *nb = (FILE_NAME_INFO *)b;
    CHECK(gi(h, C_NAME, a, sizeof a));
    SetLastError(0);
    BOOL r = gi(h, C_NORMNAME, b, sizeof b);
    if (!r) { t_note("FileNormalizedNameInfo err=%lu", GetLastError()); CloseHandle(h); fa_rml("gi_norm_long_name_here.txt"); return; }
    EXPECT_EQ(na->FileNameLength, nb->FileNameLength);
    CHECK(!memcmp(na->FileName, nb->FileName, na->FileNameLength));
    CloseHandle(h); fa_rml("gi_norm_long_name_here.txt");
}
TEST(finfo_get_normalized_name_short_form_open) {
    REQUIRE(U(GetFileInformationByHandleEx));
    FA_REQUIRE_NTFS();
    fa_mkfile_s("gi_normalize_me_longname.txt", "x");
    char p[MAX_PATH * 2], sp[MAX_PATH * 2]; fa_path(p, "gi_normalize_me_longname.txt");
    DWORD n = GetShortPathNameA(p, sp, sizeof sp);
    if (!n || !_stricmp(p, sp)) { t_skip("8.3 names unavailable"); fa_rm(p); return; }
    HANDLE h = fa_open(sp, GENERIC_READ, FA_SHARE_ALL, OPEN_EXISTING, 0);
    BYTE buf[2048]; FILE_NAME_INFO *ni = (FILE_NAME_INFO *)buf;
    SetLastError(0);
    if (gi(h, C_NORMNAME, buf, sizeof buf)) {
        CHECK(ni->FileNameLength >= 2 * strlen("gi_normalize_me_longname.txt"));
        CHECK(!_wcsnicmp(ni->FileName + ni->FileNameLength / 2 - 27, L"gi_normalize_me_longname.txt", 27));
    } else t_note("normalized name err=%lu", GetLastError());
    CloseHandle(h); fa_rm(p);
}

/* -------- directory enumeration classes -------- */
static void mkdirfiles(void) {
    fa_mkdir("gd");
    fa_mkfile_s("gd\\alpha.txt", "aa"); fa_mkfile_s("gd\\beta.txt", "bbbb"); fa_mkfile_s("gd\\gamma.txt", "c");
}
static void rmdirfiles(void) { fa_rml("gd\\alpha.txt"); fa_rml("gd\\beta.txt"); fa_rml("gd\\gamma.txt"); fa_rml("gd"); }

typedef struct { int total; int found[3]; int dots; } walk_t;
/* Walks a packed directory-info buffer; kind: 0 = ID_BOTH, 1 = FULL, 2 = EXTD */
static void walk(BYTE *buf, int kind, walk_t *w) {
    DWORD off = 0;
    for (;;) {
        DWORD next, len; WCHAR *name; LARGE_INTEGER eof; DWORD attr;
        if (kind == 0) { FILE_ID_BOTH_DIR_INFO *e = (void *)(buf + off); next = e->NextEntryOffset; len = e->FileNameLength; name = e->FileName; eof = e->EndOfFile; attr = e->FileAttributes; }
        else if (kind == 1) { FILE_FULL_DIR_INFO *e = (void *)(buf + off); next = e->NextEntryOffset; len = e->FileNameLength; name = e->FileName; eof = e->EndOfFile; attr = e->FileAttributes; }
        else { FILE_ID_EXTD_DIR_INFO *e = (void *)(buf + off); next = e->NextEntryOffset; len = e->FileNameLength; name = e->FileName; eof = e->EndOfFile; attr = e->FileAttributes; }
        w->total++;
        if (len == 2 && name[0] == L'.') w->dots++;
        if (len == 4 && name[0] == L'.' && name[1] == L'.') w->dots++;
        if (len == 18 && !_wcsnicmp(name, L"alpha.txt", 9)) { w->found[0] = 1; CHECK(eof.QuadPart == 2); CHECK(!(attr & FILE_ATTRIBUTE_DIRECTORY)); }
        if (len == 16 && !_wcsnicmp(name, L"beta.txt", 8)) { w->found[1] = 1; CHECK(eof.QuadPart == 4); }
        if (len == 18 && !_wcsnicmp(name, L"gamma.txt", 9)) { w->found[2] = 1; CHECK(eof.QuadPart == 1); }
        if (!next) break;
        off += next;
    }
}
static void enum_all(HANDLE h, DWORD first_cls, DWORD cls, int kind, walk_t *w) {
    BYTE *buf = malloc(8192);
    DWORD c = first_cls; int guard = 0;
    memset(w, 0, sizeof *w);
    for (;;) {
        SetLastError(0);
        if (!gi(h, c, buf, 8192)) { CHECKF(GetLastError() == ERROR_NO_MORE_FILES, "enum end err=%lu", GetLastError()); break; }
        walk(buf, kind, w);
        c = cls;
        if (++guard > 50) break;
    }
    free(buf);
}
TEST(finfo_get_idboth_enumerates_directory) {
    REQUIRE(U(GetFileInformationByHandleEx));
    mkdirfiles();
    HANDLE h = fa_opendir("gd", GENERIC_READ);
    walk_t w; enum_all(h, C_IDBOTH_R, C_IDBOTH, 0, &w);
    CHECK(w.found[0] && w.found[1] && w.found[2]);
    EXPECT_EQ(w.total - w.dots, 3);
    CHECK(w.dots == 2 || w.dots == 0);
    CloseHandle(h); rmdirfiles();
}
TEST(finfo_get_idboth_restart_vs_continue) {
    REQUIRE(U(GetFileInformationByHandleEx));
    mkdirfiles();
    HANDLE h = fa_opendir("gd", GENERIC_READ);
    walk_t w1, w2, w3;
    enum_all(h, C_IDBOTH_R, C_IDBOTH, 0, &w1);
    /* cursor is at the end: continuing yields no more files */
    BYTE buf[4096];
    SetLastError(0); EXPECT_EQ(gi(h, C_IDBOTH, buf, sizeof buf), FALSE); EXPECT_ERR(ERROR_NO_MORE_FILES);
    /* restart rewinds */
    enum_all(h, C_IDBOTH_R, C_IDBOTH, 0, &w2);
    EXPECT_EQ(w1.total, w2.total);
    /* the first (non-restart) call on a fresh handle also starts at the beginning */
    CloseHandle(h);
    h = fa_opendir("gd", GENERIC_READ);
    enum_all(h, C_IDBOTH, C_IDBOTH, 0, &w3);
    EXPECT_EQ(w1.total, w3.total);
    CloseHandle(h); rmdirfiles();
}
TEST(finfo_get_idboth_small_buffer_iterates) {
    REQUIRE(U(GetFileInformationByHandleEx));
    mkdirfiles();
    HANDLE h = fa_opendir("gd", GENERIC_READ);
    BYTE buf[0x70 + 64]; int total = 0, guard = 0; DWORD cls = C_IDBOTH_R;
    for (;;) {
        SetLastError(0);
        if (!gi(h, cls, buf, sizeof buf)) { DWORD e = GetLastError(); CHECKF(e == ERROR_NO_MORE_FILES || e == ERROR_MORE_DATA, "err %lu", e); break; }
        DWORD off = 0;
        for (;;) { FILE_ID_BOTH_DIR_INFO *e = (void *)(buf + off); total++; if (!e->NextEntryOffset) break; off += e->NextEntryOffset; }
        cls = C_IDBOTH;
        if (++guard > 40) break;
    }
    CHECKF(total >= 3, "total %d", total);
    CloseHandle(h); rmdirfiles();
}
TEST(finfo_get_idboth_fields) {
    REQUIRE(U(GetFileInformationByHandleEx));
    mkdirfiles();
    HANDLE h = fa_opendir("gd", GENERIC_READ);
    BYTE buf[8192]; CHECK(gi(h, C_IDBOTH_R, buf, sizeof buf));
    DWORD off = 0; int seen = 0;
    for (;;) {
        FILE_ID_BOTH_DIR_INFO *e = (void *)(buf + off);
        if (e->FileNameLength == 18 && !_wcsnicmp(e->FileName, L"alpha.txt", 9)) {
            seen = 1;
            EXPECT_EQ(e->EndOfFile.QuadPart, 2);
            CHECK(e->AllocationSize.QuadPart >= 2);
            CHECK(e->FileAttributes & FILE_ATTRIBUTE_ARCHIVE);
            CHECK(e->ShortNameLength >= 0 && e->ShortNameLength <= 24);
            if (fa_is_ntfs() && !t_is_wine()) CHECK(e->FileId.QuadPart != 0);
            CHECK(e->CreationTime.QuadPart > (LONGLONG)FA_FT_2001);
            CHECK(e->EaSize == 0);
            CHECK(((ULONG_PTR)e & 7) == 0);
        }
        if (!e->NextEntryOffset) break;
        CHECK(e->NextEntryOffset % 8 == 0);
        off += e->NextEntryOffset;
    }
    CHECK(seen);
    CloseHandle(h); rmdirfiles();
}
TEST(finfo_get_idboth_on_file_handle_fails) {
    REQUIRE(U(GetFileInformationByHandleEx));
    HANDLE h = mkf("gi_notdir.txt");
    BYTE buf[1024]; SetLastError(0);
    BOOL r = gi(h, C_IDBOTH_R, buf, sizeof buf);
    DWORD e = GetLastError();
    CHECK(!r);
    CHECKF(e == ERROR_INVALID_PARAMETER || e == ERROR_DIRECTORY || e == ERROR_NOT_SUPPORTED, "err %lu", e);
    t_note("IdBothDirectoryInfo on file err=%lu", e);
    CloseHandle(h); fa_rml("gi_notdir.txt");
}
TEST(finfo_get_idboth_needs_list_directory_access) {
    REQUIRE(U(GetFileInformationByHandleEx));
    mkdirfiles();
    HANDLE h = fa_opendir("gd", GENERIC_READ), d = NULL;
    if (!DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(), &d, SYNCHRONIZE, FALSE, 0)) { t_skip("dup failed"); CloseHandle(h); rmdirfiles(); return; }
    BYTE buf[1024]; SetLastError(0);
    EXPECT_EQ(gi(d, C_IDBOTH_R, buf, sizeof buf), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(d); CloseHandle(h); rmdirfiles();
}
TEST(finfo_get_idboth_empty_directory) {
    REQUIRE(U(GetFileInformationByHandleEx));
    fa_mkdir("gd_empty");
    HANDLE h = fa_opendir("gd_empty", GENERIC_READ);
    BYTE buf[4096]; BOOL r = gi(h, C_IDBOTH_R, buf, sizeof buf);
    int n = 0;
    if (r) { DWORD off = 0; for (;;) { FILE_ID_BOTH_DIR_INFO *e = (void *)(buf + off); n++; if (!e->NextEntryOffset) break; off += e->NextEntryOffset; } }
    CHECKF(n == 0 || n == 2, "entries %d", n);
    SetLastError(0);
    if (!r) EXPECT_ERR(ERROR_NO_MORE_FILES);
    CloseHandle(h); fa_rml("gd_empty");
}
TEST(finfo_get_fulldir_enumerates_directory) {
    REQUIRE(U(GetFileInformationByHandleEx));
    mkdirfiles();
    HANDLE h = fa_opendir("gd", GENERIC_READ);
    walk_t w; enum_all(h, C_FULLDIR_R, C_FULLDIR, 1, &w);
    CHECK(w.found[0] && w.found[1] && w.found[2]);
    EXPECT_EQ(w.total - w.dots, 3);
    CloseHandle(h); rmdirfiles();
}
TEST(finfo_get_fulldir_restart) {
    REQUIRE(U(GetFileInformationByHandleEx));
    mkdirfiles();
    HANDLE h = fa_opendir("gd", GENERIC_READ);
    walk_t a, b; enum_all(h, C_FULLDIR_R, C_FULLDIR, 1, &a);
    BYTE buf[4096]; SetLastError(0); EXPECT_EQ(gi(h, C_FULLDIR, buf, sizeof buf), FALSE); EXPECT_ERR(ERROR_NO_MORE_FILES);
    enum_all(h, C_FULLDIR_R, C_FULLDIR, 1, &b);
    EXPECT_EQ(a.total, b.total);
    CloseHandle(h); rmdirfiles();
}
TEST(finfo_get_extd_enumerates_directory) {
    REQUIRE(U(GetFileInformationByHandleEx));
    mkdirfiles();
    HANDLE h = fa_opendir("gd", GENERIC_READ);
    BYTE probe[0x200]; SetLastError(0);
    if (!gi(h, C_EXTD_R, probe, sizeof probe)) { t_note("ExtdDirectoryInfo unsupported err=%lu", GetLastError()); CloseHandle(h); rmdirfiles(); return; }
    walk_t w; enum_all(h, C_EXTD_R, C_EXTD, 2, &w);
    CHECK(w.found[0] && w.found[1] && w.found[2]);
    EXPECT_EQ(w.total - w.dots, 3);
    CloseHandle(h); rmdirfiles();
}
TEST(finfo_get_extd_fields_and_id) {
    REQUIRE(U(GetFileInformationByHandleEx));
    FA_REQUIRE_NTFS();
    mkdirfiles();
    HANDLE h = fa_opendir("gd", GENERIC_READ);
    BYTE buf[8192]; SetLastError(0);
    if (!gi(h, C_EXTD_R, buf, sizeof buf)) { t_note("unsupported err=%lu", GetLastError()); CloseHandle(h); rmdirfiles(); return; }
    DWORD off = 0;
    HANDLE f = fa_openfile("gd\\alpha.txt"); FILE_ID_INFO id; CHECK(gi(f, C_ID, &id, sizeof id));
    int seen = 0;
    for (;;) {
        FILE_ID_EXTD_DIR_INFO *e = (void *)(buf + off);
        if (e->FileNameLength == 18 && !_wcsnicmp(e->FileName, L"alpha.txt", 9)) {
            seen = 1;
            CHECK(!memcmp(&e->FileId, &id.FileId, 16));
            EXPECT_EQ(e->ReparsePointTag, 0);
            EXPECT_EQ(e->EndOfFile.QuadPart, 2);
        }
        if (!e->NextEntryOffset) break;
        off += e->NextEntryOffset;
    }
    CHECK(seen);
    CloseHandle(f); CloseHandle(h); rmdirfiles();
}
TEST(finfo_get_dir_classes_see_new_and_deleted_entries) {
    REQUIRE(U(GetFileInformationByHandleEx));
    mkdirfiles();
    HANDLE h = fa_opendir("gd", GENERIC_READ);
    walk_t w; enum_all(h, C_IDBOTH_R, C_IDBOTH, 0, &w);
    int before = w.total;
    fa_mkfile_s("gd\\delta.txt", "d");
    enum_all(h, C_IDBOTH_R, C_IDBOTH, 0, &w);
    EXPECT_EQ(w.total, before + 1);
    fa_rml("gd\\delta.txt");
    enum_all(h, C_IDBOTH_R, C_IDBOTH, 0, &w);
    EXPECT_EQ(w.total, before);
    CloseHandle(h); rmdirfiles();
}
TEST(finfo_get_dir_hidden_system_files_are_listed) {
    REQUIRE(U(GetFileInformationByHandleEx));
    mkdirfiles();
    char p[MAX_PATH * 2]; fa_path(p, "gd\\beta.txt");
    SetFileAttributesA(p, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
    HANDLE h = fa_opendir("gd", GENERIC_READ);
    walk_t w; enum_all(h, C_IDBOTH_R, C_IDBOTH, 0, &w);
    CHECK(w.found[1]);
    CloseHandle(h); SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL); rmdirfiles();
}
TEST(finfo_get_basic_after_attribute_change) {
    REQUIRE(U(GetFileInformationByHandleEx));
    HANDLE h = mkf("gi_attrchg.txt");
    char p[MAX_PATH * 2]; fa_path(p, "gi_attrchg.txt");
    SetFileAttributesA(p, FILE_ATTRIBUTE_HIDDEN);
    FILE_BASIC_INFO bi; CHECK(gi(h, C_BASIC, &bi, sizeof bi));
    CHECK(bi.FileAttributes & FILE_ATTRIBUTE_HIDDEN);
    CloseHandle(h); SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL); fa_rm(p);
}
TEST(finfo_get_byhandle_basic_parity) {
    REQUIRE(U(GetFileInformationByHandle));
    HANDLE h = mkf("gi_byh.txt");
    BY_HANDLE_FILE_INFORMATION bi; memset(&bi, 0xcc, sizeof bi);
    CHECK(U(GetFileInformationByHandle)(h, &bi));
    EXPECT_EQ(bi.nFileSizeLow, 10); EXPECT_EQ(bi.nFileSizeHigh, 0); EXPECT_EQ(bi.nNumberOfLinks, 1);
    FILE_STANDARD_INFO si; if (U(GetFileInformationByHandleEx)) { gi(h, C_STANDARD, &si, sizeof si); EXPECT_EQ(si.NumberOfLinks, bi.nNumberOfLinks); }
    SetLastError(0); EXPECT_EQ(U(GetFileInformationByHandle)(INVALID_HANDLE_VALUE, &bi), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(h); fa_rml("gi_byh.txt");
}
