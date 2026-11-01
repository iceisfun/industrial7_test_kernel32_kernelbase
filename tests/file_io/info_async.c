/* file_io: handle information classes, final paths, file times, completion routines, scatter/gather. */
#include "fio.h"

#define FI_SHARE (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE)

static const char *leaf_of(const char *p) { const char *s = strrchr(p, '\\'); return s ? s + 1 : p; }

/* ---------- GetFileInformationByHandleEx ---------- */
TEST(fio_gfibhex_standard_info) {
    REQUIRE(U(GetFileInformationByHandleEx)); REQUIRE(U(CreateFileA));
    TP(p, "gex_std.bin"); mkfile_pat(p, 1000);
    HANDLE h = cfa(p, GENERIC_READ, FI_SHARE, OPEN_EXISTING, 0); OK_H(h);
    FILE_STANDARD_INFO si; memset(&si, 0xCC, sizeof si);
    CHECK(U(GetFileInformationByHandleEx)(h, FileStandardInfo, &si, sizeof si));
    EXPECT_EQ(si.EndOfFile.QuadPart, 1000);
    CHECK(si.AllocationSize.QuadPart >= 1000);
    EXPECT_EQ(si.NumberOfLinks, 1);
    EXPECT_EQ(si.DeletePending, 0);
    EXPECT_EQ(si.Directory, 0);
    CLOSE(h); rmfile(p);
}
TEST(fio_gfibhex_basic_info_matches_attributes) {
    REQUIRE(U(GetFileInformationByHandleEx)); REQUIRE(U(CreateFileA));
    TP(p, "gex_basic.bin"); mkfile_pat(p, 3);
    HANDLE h = cfa(p, GENERIC_READ, FI_SHARE, OPEN_EXISTING, 0); OK_H(h);
    FILE_BASIC_INFO bi; memset(&bi, 0, sizeof bi);
    CHECK(U(GetFileInformationByHandleEx)(h, FileBasicInfo, &bi, sizeof bi));
    EXPECT_EQ(bi.FileAttributes, GetFileAttributesA(p));
    BY_HANDLE_FILE_INFORMATION b2; CHECK(GetFileInformationByHandle(h, &b2));
    CHECK(bi.LastWriteTime.QuadPart == (((LONGLONG)b2.ftLastWriteTime.dwHighDateTime << 32) | b2.ftLastWriteTime.dwLowDateTime));
    CHECK(bi.CreationTime.QuadPart == (((LONGLONG)b2.ftCreationTime.dwHighDateTime << 32) | b2.ftCreationTime.dwLowDateTime));
    CLOSE(h); rmfile(p);
}
TEST(fio_gfibhex_name_info) {
    REQUIRE(U(GetFileInformationByHandleEx)); REQUIRE(U(CreateFileA));
    TP(p, "gex_name.bin"); mkfile_pat(p, 3);
    HANDLE h = cfa(p, GENERIC_READ, FI_SHARE, OPEN_EXISTING, 0); OK_H(h);
    union { FILE_NAME_INFO i; char raw[2200]; } u; memset(&u, 0, sizeof u);
    CHECK(U(GetFileInformationByHandleEx)(h, FileNameInfo, &u, sizeof u));
    DWORD n = u.i.FileNameLength / 2;
    CHECK(n > 0 && u.i.FileName[0] == L'\\');          /* volume-relative path */
    size_t ll = strlen(leaf_of(p));
    CHECK(n > ll);
    if (n > ll) {
        WCHAR *tail = u.i.FileName + n - ll;
        CHECK(!_wcsnicmp(tail, fio_w(leaf_of(p)), ll));
    }
    CLOSE(h); rmfile(p);
}
TEST(fio_gfibhex_attribute_tag_and_compression) {
    REQUIRE(U(GetFileInformationByHandleEx)); REQUIRE(U(CreateFileA));
    TP(p, "gex_tag.bin"); mkfile_pat(p, 3);
    HANDLE h = cfa(p, GENERIC_READ, FI_SHARE, OPEN_EXISTING, 0); OK_H(h);
    FILE_ATTRIBUTE_TAG_INFO ti; memset(&ti, 0xCC, sizeof ti);
    SET_ERR(0);
    BOOL ok = U(GetFileInformationByHandleEx)(h, FileAttributeTagInfo, &ti, sizeof ti);
    if (ok) { EXPECT_EQ(ti.FileAttributes, GetFileAttributesA(p)); EXPECT_EQ(ti.ReparseTag, 0); }
    else t_note("FileAttributeTagInfo gle=%lu", GetLastError());
    CLOSE(h); rmfile(p);
}
TEST(fio_gfibhex_id_info) {
    REQUIRE(U(GetFileInformationByHandleEx)); REQUIRE(U(CreateFileA));
    TP(p, "gex_id.bin"); mkfile_pat(p, 3);
    HANDLE h = cfa(p, GENERIC_READ, FI_SHARE, OPEN_EXISTING, 0); OK_H(h);
    FILE_ID_INFO ii; memset(&ii, 0, sizeof ii);
    SET_ERR(0);
    BOOL ok = U(GetFileInformationByHandleEx)(h, FileIdInfo, &ii, sizeof ii);
    if (!ok) { t_note("FileIdInfo unsupported on this file system gle=%lu", GetLastError()); }
    else {
        int nz = 0; for (int i = 0; i < 16; i++) if (ii.FileId.Identifier[i]) nz = 1;
        CHECK(nz);
        CHECK(ii.VolumeSerialNumber != 0);
    }
    CLOSE(h); rmfile(p);
}
TEST(fio_gfibhex_stream_info) {
    REQUIRE(U(GetFileInformationByHandleEx)); REQUIRE(U(CreateFileA));
    TP(p, "gex_stream.bin"); mkfile_pat(p, 77);
    HANDLE h = cfa(p, GENERIC_READ, FI_SHARE, OPEN_EXISTING, 0); OK_H(h);
    union { FILE_STREAM_INFO i; char raw[1024]; } u; memset(&u, 0, sizeof u);
    SET_ERR(0);
    BOOL ok = U(GetFileInformationByHandleEx)(h, FileStreamInfo, &u, sizeof u);
    if (!ok) t_note("FileStreamInfo gle=%lu (non-NTFS?)", GetLastError());
    else {
        EXPECT_EQ(u.i.StreamSize.QuadPart, 77);
        CHECK(u.i.StreamNameLength == 14 && !wcsncmp(u.i.StreamName, L"::$DATA", 7));
    }
    CLOSE(h); rmfile(p);
}
TEST(fio_gfibhex_invalid_arguments) {
    REQUIRE(U(GetFileInformationByHandleEx)); REQUIRE(U(CreateFileA));
    char buf[256];
    EXPECT_FAIL_B(U(GetFileInformationByHandleEx)(INVALID_HANDLE_VALUE, FileStandardInfo, buf, sizeof buf), ERROR_INVALID_HANDLE);
    EXPECT_FAIL_B(U(GetFileInformationByHandleEx)(NULL, FileStandardInfo, buf, sizeof buf), ERROR_INVALID_HANDLE);
    TP(p, "gex_bad.bin"); mkfile_pat(p, 3);
    HANDLE h = cfa(p, GENERIC_READ, FI_SHARE, OPEN_EXISTING, 0); OK_H(h);
    /* buffer too small for the class */
    SET_ERR(0);
    BOOL ok = U(GetFileInformationByHandleEx)(h, FileStandardInfo, buf, 4);
    DWORD e = GetLastError();
    CHECK(!ok);
    CHECKF(e == ERROR_BAD_LENGTH || e == ERROR_INSUFFICIENT_BUFFER || e == ERROR_INVALID_PARAMETER, "gle=%lu", e);
    /* class value outside of the table */
    SET_ERR(0);
    ok = U(GetFileInformationByHandleEx)(h, (FILE_INFO_BY_HANDLE_CLASS)0x7FFF, buf, sizeof buf);
    CHECK(!ok); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    ok = U(GetFileInformationByHandleEx)(h, (FILE_INFO_BY_HANDLE_CLASS)-1, buf, sizeof buf);
    CHECK(!ok); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CLOSE(h); rmfile(p);
}

/* ---------- SetFileInformationByHandle ---------- */
TEST(fio_sfibh_end_of_file_info) {
    REQUIRE(U(SetFileInformationByHandle)); REQUIRE(U(CreateFileA));
    TP(p, "sfi_eof.bin"); mkfile_pat(p, 10);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, FI_SHARE, OPEN_EXISTING, 0); OK_H(h);
    FILE_END_OF_FILE_INFO ei; ei.EndOfFile.QuadPart = 5000;
    CHECK(U(SetFileInformationByHandle)(h, FileEndOfFileInfo, &ei, sizeof ei));
    EXPECT_EQ(hsize(h), 5000);
    ei.EndOfFile.QuadPart = 3;
    CHECK(U(SetFileInformationByHandle)(h, FileEndOfFileInfo, &ei, sizeof ei));
    EXPECT_EQ(hsize(h), 3);
    EXPECT_EQ(fpos(h), 0);                                   /* position untouched */
    FILE_ALLOCATION_INFO ai; ai.AllocationSize.QuadPart = 8192;
    BOOL ok = U(SetFileInformationByHandle)(h, FileAllocationInfo, &ai, sizeof ai);
    t_note("FileAllocationInfo = %d gle=%lu", ok, GetLastError());
    EXPECT_EQ(hsize(h), 3);
    CLOSE(h);
    h = cfa(p, GENERIC_READ, FI_SHARE, OPEN_EXISTING, 0); OK_H(h);
    ei.EndOfFile.QuadPart = 1;
    EXPECT_FAIL_B(U(SetFileInformationByHandle)(h, FileEndOfFileInfo, &ei, sizeof ei), ERROR_ACCESS_DENIED);
    CLOSE(h); rmfile(p);
}
TEST(fio_sfibh_disposition_info) {
    REQUIRE(U(SetFileInformationByHandle)); REQUIRE(U(CreateFileA));
    TP(p, "sfi_disp.bin"); mkfile_pat(p, 10);
    HANDLE h = cfa(p, GENERIC_READ | DELETE, FI_SHARE, OPEN_EXISTING, 0); OK_H(h);
    FILE_DISPOSITION_INFO di; di.DeleteFile = TRUE;
    CHECK(U(SetFileInformationByHandle)(h, FileDispositionInfo, &di, sizeof di));
    FILE_STANDARD_INFO si; CHECK(GetFileInformationByHandleEx(h, FileStandardInfo, &si, sizeof si));
    EXPECT_EQ(si.DeletePending, 1);
    di.DeleteFile = FALSE;                                    /* can be revoked while the handle is open */
    CHECK(U(SetFileInformationByHandle)(h, FileDispositionInfo, &di, sizeof di));
    CHECK(GetFileInformationByHandleEx(h, FileStandardInfo, &si, sizeof si));
    EXPECT_EQ(si.DeletePending, 0);
    di.DeleteFile = TRUE;
    CHECK(U(SetFileInformationByHandle)(h, FileDispositionInfo, &di, sizeof di));
    CLOSE(h);
    CHECK(!exists(p));
    mkfile_pat(p, 10);
    h = cfa(p, GENERIC_READ, FI_SHARE, OPEN_EXISTING, 0); OK_H(h);    /* no DELETE access */
    di.DeleteFile = TRUE;
    EXPECT_FAIL_B(U(SetFileInformationByHandle)(h, FileDispositionInfo, &di, sizeof di), ERROR_ACCESS_DENIED);
    CLOSE(h); CHECK(exists(p)); rmfile(p);
}
TEST(fio_sfibh_basic_info_attributes_and_times) {
    REQUIRE(U(SetFileInformationByHandle)); REQUIRE(U(CreateFileA));
    TP(p, "sfi_basic.bin"); mkfile_pat(p, 10);
    HANDLE h = cfa(p, FILE_WRITE_ATTRIBUTES | FILE_READ_ATTRIBUTES, FI_SHARE, OPEN_EXISTING, 0); OK_H(h);
    FILE_BASIC_INFO bi; memset(&bi, 0, sizeof bi);
    bi.FileAttributes = FILE_ATTRIBUTE_HIDDEN;
    bi.LastWriteTime.QuadPart = 132223104000000000LL;        /* 2020-01-01 00:00:00 UTC */
    CHECK(U(SetFileInformationByHandle)(h, FileBasicInfo, &bi, sizeof bi));
    CHECK(GetFileAttributesA(p) & FILE_ATTRIBUTE_HIDDEN);
    FILE_BASIC_INFO g; memset(&g, 0, sizeof g);
    CHECK(GetFileInformationByHandleEx(h, FileBasicInfo, &g, sizeof g));
    EXPECT_EQ(g.LastWriteTime.QuadPart, 132223104000000000LL);
    bi.FileAttributes = FILE_ATTRIBUTE_NORMAL; memset(&bi.LastWriteTime, 0, sizeof bi.LastWriteTime);
    CHECK(U(SetFileInformationByHandle)(h, FileBasicInfo, &bi, sizeof bi));
    CHECK(!(GetFileAttributesA(p) & FILE_ATTRIBUTE_HIDDEN));
    CLOSE(h); rmfile(p);
}
TEST(fio_sfibh_invalid_arguments) {
    REQUIRE(U(SetFileInformationByHandle)); REQUIRE(U(CreateFileA));
    FILE_END_OF_FILE_INFO ei; ei.EndOfFile.QuadPart = 0;
    EXPECT_FAIL_B(U(SetFileInformationByHandle)(INVALID_HANDLE_VALUE, FileEndOfFileInfo, &ei, sizeof ei), ERROR_INVALID_HANDLE);
    EXPECT_FAIL_B(U(SetFileInformationByHandle)(NULL, FileEndOfFileInfo, &ei, sizeof ei), ERROR_INVALID_HANDLE);
    TP(p, "sfi_bad.bin"); mkfile_pat(p, 10);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, FI_SHARE, OPEN_EXISTING, 0); OK_H(h);
    SET_ERR(0);
    CHECK(!U(SetFileInformationByHandle)(h, (FILE_INFO_BY_HANDLE_CLASS)0x7FFF, &ei, sizeof ei)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!U(SetFileInformationByHandle)(h, FileStandardInfo, &ei, sizeof ei));   /* query-only class */
    CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "gle=%lu", GetLastError());
    EXPECT_EQ(hsize(h), 10);
    CLOSE(h); rmfile(p);
}
TEST(fio_sfibh_rename_info) {
    REQUIRE(U(SetFileInformationByHandle)); REQUIRE(U(CreateFileA));
    TP(p, "sfi_ren_a.bin"); TP(q, "sfi_ren_b.bin"); TP(r, "sfi_ren_c.bin");
    rmfile(p); rmfile(q); rmfile(r);
    mkfile_pat(p, 10); mkfile_pat(q, 20);
    HANDLE h = cfa(p, GENERIC_READ | DELETE, FI_SHARE, OPEN_EXISTING, 0); OK_H(h);
    /* NT-style destination path is required (\??\ + dos path) */
    char nt[MAX_PATH * 2]; snprintf(nt, sizeof nt, "\\??\\%s", r);
    WCHAR *wn = fio_w(nt); DWORD bytes = (DWORD)(lstrlenW(wn) * 2);
    union { FILE_RENAME_INFO i; char raw[sizeof(FILE_RENAME_INFO) + 1200 * 2]; } u; memset(&u, 0, sizeof u);
    u.i.ReplaceIfExists = FALSE; u.i.RootDirectory = NULL; u.i.FileNameLength = bytes;
    memcpy(u.i.FileName, wn, bytes);
    DWORD total = (DWORD)(offsetof(FILE_RENAME_INFO, FileName) + bytes + 2);
    CHECK(U(SetFileInformationByHandle)(h, FileRenameInfo, &u, total));
    CHECK(!exists(p)); CHECK(exists(r)); EXPECT_EQ(fsize_of(r), 10);
    /* target exists, no replace */
    snprintf(nt, sizeof nt, "\\??\\%s", q);
    wn = fio_w(nt); bytes = (DWORD)(lstrlenW(wn) * 2);
    memset(&u, 0, sizeof u); u.i.FileNameLength = bytes; memcpy(u.i.FileName, wn, bytes);
    total = (DWORD)(offsetof(FILE_RENAME_INFO, FileName) + bytes + 2);
    SET_ERR(0);
    CHECK(!U(SetFileInformationByHandle)(h, FileRenameInfo, &u, total));
    DWORD e = GetLastError();
    CHECKF(e == ERROR_ALREADY_EXISTS || e == ERROR_ACCESS_DENIED || e == ERROR_FILE_EXISTS, "gle=%lu", e);
    EXPECT_EQ(fsize_of(q), 20);
    u.i.ReplaceIfExists = TRUE;
    CHECK(U(SetFileInformationByHandle)(h, FileRenameInfo, &u, total));
    EXPECT_EQ(fsize_of(q), 10); CHECK(!exists(r));
    CLOSE(h); rmfile(q); rmfile(r); rmfile(p);
}

/* ---------- GetFinalPathNameByHandle ---------- */
TEST(fio_finalpath_flags) {
    REQUIRE(U(GetFinalPathNameByHandleA)); REQUIRE(U(CreateFileA));
    TP(p, "final_path.bin"); mkfile_pat(p, 3);
    HANDLE h = cfa(p, GENERIC_READ, FI_SHARE, OPEN_EXISTING, 0); OK_H(h);
    char b[1024]; DWORD n;
    n = U(GetFinalPathNameByHandleA)(h, b, sizeof b, VOLUME_NAME_DOS | FILE_NAME_NORMALIZED);
    CHECK(n > 0 && n < sizeof b);
    CHECK(!strncmp(b, "\\\\?\\", 4) || !strncmp(b, "\\\\?\\UNC\\", 8));
    if (n > 0 && n < sizeof b) {
        EXPECT_EQ(strlen(b), n);
        CHECK(!_stricmp(b + n - strlen(leaf_of(p)), leaf_of(p)));
        if (isalpha((unsigned char)p[0]) && p[1] == ':') CHECK(b[5] == ':' && toupper(b[4]) == toupper(p[0]));
    }
    n = U(GetFinalPathNameByHandleA)(h, b, sizeof b, VOLUME_NAME_NT);
    CHECK(n > 0 && !strncmp(b, "\\Device\\", 8));
    n = U(GetFinalPathNameByHandleA)(h, b, sizeof b, VOLUME_NAME_NONE);
    CHECK(n > 0 && b[0] == '\\' && b[1] != '\\');
    n = U(GetFinalPathNameByHandleA)(h, b, sizeof b, VOLUME_NAME_GUID);
    if (n > 0) CHECK(!strncmp(b, "\\\\?\\Volume{", 11)); else t_note("VOLUME_NAME_GUID gle=%lu", GetLastError());
    n = U(GetFinalPathNameByHandleA)(h, b, sizeof b, VOLUME_NAME_DOS | FILE_NAME_OPENED);
    CHECK(n > 0);
    CLOSE(h); rmfile(p);
}
TEST(fio_finalpath_buffer_sizes) {
    REQUIRE(U(GetFinalPathNameByHandleW)); REQUIRE(U(CreateFileA));
    TP(p, "final_buf.bin"); mkfile_pat(p, 3);
    HANDLE h = cfa(p, GENERIC_READ, FI_SHARE, OPEN_EXISTING, 0); OK_H(h);
    WCHAR b[1024];
    DWORD need = U(GetFinalPathNameByHandleW)(h, NULL, 0, VOLUME_NAME_DOS);
    CHECK(need > 4 && need < 1024);            /* required size includes the terminator */
    DWORD n = U(GetFinalPathNameByHandleW)(h, b, 1024, VOLUME_NAME_DOS);
    EXPECT_EQ(n, need - 1);
    EXPECT_EQ(lstrlenW(b), n);
    WCHAR s[1024]; memset(s, 0x55, sizeof s);
    DWORD t = U(GetFinalPathNameByHandleW)(h, s, need - 1, VOLUME_NAME_DOS);   /* one too small: needs the NUL */
    EXPECT_EQ(t, need);
    t = U(GetFinalPathNameByHandleW)(h, s, need, VOLUME_NAME_DOS);
    EXPECT_EQ(t, need - 1);
    t = U(GetFinalPathNameByHandleW)(h, s, 1, VOLUME_NAME_DOS);
    EXPECT_EQ(t, need);
    CLOSE(h); rmfile(p);
}
TEST(fio_finalpath_errors) {
    REQUIRE(U(GetFinalPathNameByHandleW));
    WCHAR b[64];
    SET_ERR(0);
    DWORD n = U(GetFinalPathNameByHandleW)(INVALID_HANDLE_VALUE, b, 64, VOLUME_NAME_DOS);
    CHECK(n == 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    n = U(GetFinalPathNameByHandleW)(NULL, b, 64, VOLUME_NAME_DOS);
    CHECK(n == 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
}

/* ---------- Get/SetFileTime on handles ---------- */
TEST(fio_filetime_roundtrip) {
    REQUIRE(U(SetFileTime)); REQUIRE(U(GetFileTime)); REQUIRE(U(CreateFileA));
    TP(p, "ft_rt.bin"); mkfile_pat(p, 5);
    HANDLE h = cfa(p, GENERIC_READ | FILE_WRITE_ATTRIBUTES, FI_SHARE, OPEN_EXISTING, 0); OK_H(h);
    FILETIME c, a, w, c2, a2, w2;
    ULARGE_INTEGER v;
    v.QuadPart = 132223104000000000ULL; c.dwLowDateTime = v.LowPart; c.dwHighDateTime = v.HighPart;
    v.QuadPart = 132223104400000000ULL; a.dwLowDateTime = v.LowPart; a.dwHighDateTime = v.HighPart;
    v.QuadPart = 132223105800000000ULL; w.dwLowDateTime = v.LowPart; w.dwHighDateTime = v.HighPart;
    CHECK(U(SetFileTime)(h, &c, &a, &w));
    CHECK(U(GetFileTime)(h, &c2, &a2, &w2));
    CHECK(!memcmp(&c, &c2, sizeof c)); CHECK(!memcmp(&w, &w2, sizeof w));
    CHECKF(a2.dwHighDateTime == a.dwHighDateTime, "access time high differs");
    /* NULL pointers leave the value alone */
    CHECK(U(SetFileTime)(h, NULL, NULL, NULL));
    CHECK(U(GetFileTime)(h, &c2, NULL, NULL)); CHECK(!memcmp(&c, &c2, sizeof c));
    CHECK(U(GetFileTime)(h, NULL, NULL, &w2)); CHECK(!memcmp(&w, &w2, sizeof w));
    CHECK(U(GetFileTime)(h, NULL, NULL, NULL));
    CLOSE(h);
    h = cfa(p, GENERIC_READ, FI_SHARE, OPEN_EXISTING, 0); OK_H(h);
    EXPECT_FAIL_B(U(SetFileTime)(h, &c, NULL, NULL), ERROR_ACCESS_DENIED);
    CLOSE(h); rmfile(p);
}
TEST(fio_filetime_invalid_handle) {
    REQUIRE(U(SetFileTime)); REQUIRE(U(GetFileTime));
    FILETIME f; memset(&f, 0, sizeof f);
    EXPECT_FAIL_B(U(GetFileTime)(INVALID_HANDLE_VALUE, &f, NULL, NULL), ERROR_INVALID_HANDLE);
    EXPECT_FAIL_B(U(SetFileTime)(INVALID_HANDLE_VALUE, &f, NULL, NULL), ERROR_INVALID_HANDLE);
    EXPECT_FAIL_B(U(GetFileTime)(NULL, &f, NULL, NULL), ERROR_INVALID_HANDLE);
}
TEST(fio_filetime_write_updates_last_write) {
    REQUIRE(U(GetFileTime)); REQUIRE(U(WriteFile)); REQUIRE(U(CreateFileA));
    TP(p, "ft_write.bin"); mkfile_pat(p, 5);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE | FILE_WRITE_ATTRIBUTES, FI_SHARE, OPEN_EXISTING, 0); OK_H(h);
    FILETIME old; ULARGE_INTEGER v; v.QuadPart = 132223104000000000ULL; old.dwLowDateTime = v.LowPart; old.dwHighDateTime = v.HighPart;
    CHECK(SetFileTime(h, NULL, NULL, &old));
    /* a timestamp set explicitly through a handle is not updated by writes through that same handle, so
       write through a second handle and close it before reading the time back */
    DWORD n;
    HANDLE h2 = cfa(p, GENERIC_WRITE, FI_SHARE, OPEN_EXISTING, 0); OK_H(h2);
    CHECK(U(WriteFile)(h2, "x", 1, &n, NULL));
    CLOSE(h2);
    FILETIME w; CHECK(U(GetFileTime)(h, NULL, NULL, &w));
    CHECK(CompareFileTime(&w, &old) > 0);              /* write moved it (unless the time was frozen) */
    CLOSE(h); rmfile(p);
}

/* ---------- OpenFileById ---------- */
TEST(fio_openfilebyid_roundtrip) {
    REQUIRE(U(OpenFileById)); REQUIRE(U(CreateFileA));
    TP(p, "byid.bin"); mkfile_pat(p, 12);
    HANDLE h = cfa(p, GENERIC_READ, FI_SHARE, OPEN_EXISTING, 0); OK_H(h);
    BY_HANDLE_FILE_INFORMATION bi; CHECK(GetFileInformationByHandle(h, &bi));
    FILE_ID_DESCRIPTOR d; memset(&d, 0, sizeof d);
    d.dwSize = sizeof d; d.Type = FileIdType;
    d.FileId.LowPart = bi.nFileIndexLow; d.FileId.HighPart = (LONG)bi.nFileIndexHigh;
    SET_ERR(0);
    HANDLE r = U(OpenFileById)(h, &d, GENERIC_READ, FI_SHARE, NULL, 0);
    if (r == INVALID_HANDLE_VALUE) t_note("OpenFileById gle=%lu", GetLastError());
    else {
        EXPECT_EQ(hsize(r), 12);
        BY_HANDLE_FILE_INFORMATION b2; CHECK(GetFileInformationByHandle(r, &b2));
        EXPECT_EQ(b2.nFileIndexLow, bi.nFileIndexLow);
        CloseHandle(r);
    }
    /* bad descriptor size */
    d.dwSize = 3;
    r = U(OpenFileById)(h, &d, GENERIC_READ, FI_SHARE, NULL, 0);
    CHECK(r == INVALID_HANDLE_VALUE); if (r != INVALID_HANDLE_VALUE) CloseHandle(r);
    d.dwSize = sizeof d;
    r = U(OpenFileById)(INVALID_HANDLE_VALUE, &d, GENERIC_READ, FI_SHARE, NULL, 0);
    CHECK(r == INVALID_HANDLE_VALUE); if (r != INVALID_HANDLE_VALUE) CloseHandle(r);
    CLOSE(h); rmfile(p);
}

/* ---------- completion routines / overlapped helpers ---------- */
typedef struct { DWORD calls, err, bytes; LPOVERLAPPED ov; } CR;
static CR g_cr;
static VOID WINAPI cr_routine(DWORD err, DWORD bytes, LPOVERLAPPED ov) { g_cr.calls++; g_cr.err = err; g_cr.bytes = bytes; g_cr.ov = ov; }

TEST(fio_writefileex_readfileex_completion) {
    REQUIRE(U(WriteFileEx)); REQUIRE(U(ReadFileEx)); REQUIRE(U(CreateFileA));
    TP(p, "ex_rw.bin"); rmfile(p);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, FI_SHARE, CREATE_NEW, FILE_FLAG_OVERLAPPED); OK_H(h);
    OVERLAPPED ov; memset(&ov, 0, sizeof ov);
    memset(&g_cr, 0, sizeof g_cr);
    CHECK(U(WriteFileEx)(h, "abcdefgh", 8, &ov, cr_routine));
    for (int i = 0; i < 100 && !g_cr.calls; i++) SleepEx(50, TRUE);
    EXPECT_EQ(g_cr.calls, 1); EXPECT_EQ(g_cr.err, 0); EXPECT_EQ(g_cr.bytes, 8); CHECK(g_cr.ov == &ov);
    EXPECT_EQ(hsize(h), 8);
    char b[16] = {0}; memset(&ov, 0, sizeof ov); ov.Offset = 2; memset(&g_cr, 0, sizeof g_cr);
    CHECK(U(ReadFileEx)(h, b, 6, &ov, cr_routine));
    for (int i = 0; i < 100 && !g_cr.calls; i++) SleepEx(50, TRUE);
    EXPECT_EQ(g_cr.calls, 1); EXPECT_EQ(g_cr.bytes, 6); CHECK(!memcmp(b, "cdefgh", 6));
    /* read at EOF reports ERROR_HANDLE_EOF in the completion routine */
    memset(&ov, 0, sizeof ov); ov.Offset = 8; memset(&g_cr, 0, sizeof g_cr);
    BOOL ok = U(ReadFileEx)(h, b, 4, &ov, cr_routine);
    if (ok) { for (int i = 0; i < 100 && !g_cr.calls; i++) SleepEx(50, TRUE); EXPECT_EQ(g_cr.err, ERROR_HANDLE_EOF); EXPECT_EQ(g_cr.bytes, 0); }
    else EXPECT_ERR(ERROR_HANDLE_EOF);
    CLOSE(h); rmfile(p);
}
TEST(fio_readfileex_argument_errors) {
    REQUIRE(U(ReadFileEx)); REQUIRE(U(WriteFileEx)); REQUIRE(U(CreateFileA));
    TP(p, "ex_err.bin"); mkfile_pat(p, 16);
    char b[8]; OVERLAPPED ov; memset(&ov, 0, sizeof ov);
    EXPECT_FAIL_B(U(ReadFileEx)(INVALID_HANDLE_VALUE, b, 4, &ov, cr_routine), ERROR_INVALID_HANDLE);
    EXPECT_FAIL_B(U(WriteFileEx)(INVALID_HANDLE_VALUE, b, 4, &ov, cr_routine), ERROR_INVALID_HANDLE);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, FI_SHARE, OPEN_EXISTING, FILE_FLAG_OVERLAPPED); OK_H(h);
    SET_ERR(0);
    /* OVERLAPPED is mandatory, but kernelbase ReadFileEx/WriteFileEx dereference it unconditionally
       (access violation on NULL), so passing NULL is not a testable error path */
    EXPECT_EQ(fsize_of(p), 16);
    CLOSE(h); rmfile(p);
}
TEST(fio_getoverlappedresult_semantics) {
    REQUIRE(U(GetOverlappedResult)); REQUIRE(U(ReadFile)); REQUIRE(U(CreateFileA));
    TP(p, "gor.bin"); mkfile_pat(p, 8192);
    HANDLE h = cfa(p, GENERIC_READ, FI_SHARE, OPEN_EXISTING, FILE_FLAG_OVERLAPPED); OK_H(h);
    OVERLAPPED ov; memset(&ov, 0, sizeof ov); ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    char b[4096]; DWORD n = 0;
    BOOL ok = U(ReadFile)(h, b, sizeof b, NULL, &ov);
    if (!ok) EXPECT_ERR(ERROR_IO_PENDING);
    CHECK(U(GetOverlappedResult)(h, &ov, &n, TRUE)); EXPECT_EQ(n, 4096);
    EXPECT_EQ(ov.InternalHigh, 4096);
    n = 99; CHECK(U(GetOverlappedResult)(h, &ov, &n, FALSE)); EXPECT_EQ(n, 4096);   /* repeatable */
    /* Offset past EOF */
    memset(&ov.Internal, 0, 2 * sizeof(ULONG_PTR)); ResetEvent(ov.hEvent);
    ov.Offset = 8192; ov.OffsetHigh = 0;
    ok = U(ReadFile)(h, b, 16, NULL, &ov);
    DWORD e = GetLastError();
    if (!ok && e == ERROR_IO_PENDING) { ok = U(GetOverlappedResult)(h, &ov, &n, TRUE); e = GetLastError(); }
    CHECK(!ok); EXPECT_EQ(e, ERROR_HANDLE_EOF);
    CloseHandle(ov.hEvent); CLOSE(h); rmfile(p);
}
TEST(fio_getoverlappedresultex_timeout) {
    REQUIRE(U(GetOverlappedResultEx)); REQUIRE(U(CreateNamedPipeA));
    char name[128]; snprintf(name, sizeof name, "\\\\.\\pipe\\k32t_fio_gore_%lu", GetCurrentProcessId());
    HANDLE srv = U(CreateNamedPipeA)(name, PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 256, 256, 0, NULL);
    if (srv == INVALID_HANDLE_VALUE) { t_skip("CreateNamedPipe gle=%lu", GetLastError()); return; }
    HANDLE cli = cfa(name, GENERIC_WRITE, 0, OPEN_EXISTING, 0); OK_H(cli);
    OVERLAPPED ov; memset(&ov, 0, sizeof ov); ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    char b[16]; DWORD n = 0;
    BOOL ok = ReadFile(srv, b, sizeof b, NULL, &ov);
    CHECK(!ok); EXPECT_ERR(ERROR_IO_PENDING);
    SET_ERR(0);
    ok = U(GetOverlappedResultEx)(srv, &ov, &n, 0, FALSE);
    CHECK(!ok); EXPECT_ERR(ERROR_IO_INCOMPLETE);
    SET_ERR(0);
    ok = U(GetOverlappedResultEx)(srv, &ov, &n, 50, FALSE);
    CHECK(!ok); EXPECT_ERR(WAIT_TIMEOUT);
    DWORD w; CHECK(WriteFile(cli, "hi", 2, &w, NULL));
    CHECK(U(GetOverlappedResultEx)(srv, &ov, &n, 5000, FALSE)); EXPECT_EQ(n, 2);
    CloseHandle(ov.hEvent); CloseHandle(cli); CloseHandle(srv);
}
TEST(fio_cancelioex_no_pending) {
    REQUIRE(U(CancelIoEx)); REQUIRE(U(CancelIo)); REQUIRE(U(CreateFileA));
    TP(p, "cancel.bin"); mkfile_pat(p, 8);
    HANDLE h = cfa(p, GENERIC_READ, FI_SHARE, OPEN_EXISTING, FILE_FLAG_OVERLAPPED); OK_H(h);
    SET_ERR(0);
    CHECK(!U(CancelIoEx)(h, NULL)); EXPECT_ERR(ERROR_NOT_FOUND);
    OVERLAPPED ov; memset(&ov, 0, sizeof ov);
    SET_ERR(0);
    CHECK(!U(CancelIoEx)(h, &ov)); EXPECT_ERR(ERROR_NOT_FOUND);
    CHECK(U(CancelIo)(h));
    CLOSE(h); rmfile(p);
    EXPECT_FAIL_B(U(CancelIoEx)(INVALID_HANDLE_VALUE, NULL), ERROR_INVALID_HANDLE);
    EXPECT_FAIL_B(U(CancelIo)(INVALID_HANDLE_VALUE), ERROR_INVALID_HANDLE);
    EXPECT_FAIL_B(U(CancelIo)(NULL), ERROR_INVALID_HANDLE);
}
TEST(fio_cancelioex_cancels_pending_pipe_read) {
    REQUIRE(U(CancelIoEx)); REQUIRE(U(GetOverlappedResult)); REQUIRE(U(CreateNamedPipeA));
    char name[128]; snprintf(name, sizeof name, "\\\\.\\pipe\\k32t_fio_cancel_%lu", GetCurrentProcessId());
    HANDLE srv = U(CreateNamedPipeA)(name, PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 256, 256, 0, NULL);
    if (srv == INVALID_HANDLE_VALUE) { t_skip("CreateNamedPipe gle=%lu", GetLastError()); return; }
    HANDLE cli = cfa(name, GENERIC_WRITE, 0, OPEN_EXISTING, 0); OK_H(cli);
    OVERLAPPED ov; memset(&ov, 0, sizeof ov); ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    char b[16]; DWORD n = 0;
    BOOL ok = ReadFile(srv, b, sizeof b, NULL, &ov);
    CHECK(!ok); EXPECT_ERR(ERROR_IO_PENDING);
    CHECK(U(CancelIoEx)(srv, &ov));
    SET_ERR(0);
    ok = U(GetOverlappedResult)(srv, &ov, &n, TRUE);
    CHECK(!ok); EXPECT_ERR(ERROR_OPERATION_ABORTED);
    CloseHandle(ov.hEvent); CloseHandle(cli); CloseHandle(srv);
}

/* ---------- scatter / gather ---------- */
TEST(fio_scatter_gather_roundtrip) {
    REQUIRE(U(WriteFileGather)); REQUIRE(U(ReadFileScatter)); REQUIRE(U(CreateFileA)); REQUIRE(U(GetOverlappedResult));
    TP(p, "sg.bin"); rmfile(p);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, 0, CREATE_NEW, FILE_FLAG_OVERLAPPED | FILE_FLAG_NO_BUFFERING); OK_H(h);
    if (h == INVALID_HANDLE_VALUE) return;
    SYSTEM_INFO si; GetSystemInfo(&si);
    DWORD pg = si.dwPageSize;
    char *a = (char *)VirtualAlloc(NULL, pg, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    char *b = (char *)VirtualAlloc(NULL, pg, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    memset(a, 'A', pg); memset(b, 'B', pg);
    FILE_SEGMENT_ELEMENT seg[3]; memset(seg, 0, sizeof seg);
    seg[0].Buffer = PtrToPtr64(a); seg[1].Buffer = PtrToPtr64(b);
    OVERLAPPED ov; memset(&ov, 0, sizeof ov); ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    BOOL ok = U(WriteFileGather)(h, seg, 2 * pg, NULL, &ov);
    if (!ok && GetLastError() != ERROR_IO_PENDING) { t_note("WriteFileGather gle=%lu", GetLastError()); goto out; }
    DWORD n = 0; CHECK(U(GetOverlappedResult)(h, &ov, &n, TRUE)); EXPECT_EQ(n, 2 * pg);
    EXPECT_EQ(fsize_of(p), 2 * pg);
    char *r1 = (char *)VirtualAlloc(NULL, pg, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    char *r2 = (char *)VirtualAlloc(NULL, pg, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    memset(seg, 0, sizeof seg); seg[0].Buffer = PtrToPtr64(r1); seg[1].Buffer = PtrToPtr64(r2);
    memset(&ov.Internal, 0, 2 * sizeof(ULONG_PTR)); ov.Offset = 0; ResetEvent(ov.hEvent);
    ok = U(ReadFileScatter)(h, seg, 2 * pg, NULL, &ov);
    CHECK(ok || GetLastError() == ERROR_IO_PENDING);
    CHECK(U(GetOverlappedResult)(h, &ov, &n, TRUE)); EXPECT_EQ(n, 2 * pg);
    CHECK(r1[0] == 'A' && r1[pg - 1] == 'A' && r2[0] == 'B' && r2[pg - 1] == 'B');
    /* byte count must be a page multiple */
    memset(&ov.Internal, 0, 2 * sizeof(ULONG_PTR));
    SET_ERR(0);
    ok = U(ReadFileScatter)(h, seg, pg + 1, NULL, &ov);
    t_note("ReadFileScatter(size not page multiple) = %d gle=%lu", ok, GetLastError());
    if (!ok && GetLastError() == ERROR_IO_PENDING) GetOverlappedResult(h, &ov, &n, TRUE);
    /* reserved must be NULL */
    DWORD rsv = 1;
    SET_ERR(0);
    ok = U(ReadFileScatter)(h, seg, pg, &rsv, &ov);
    t_note("ReadFileScatter(reserved != NULL) = %d gle=%lu", ok, GetLastError());
    if (!ok && GetLastError() == ERROR_IO_PENDING) GetOverlappedResult(h, &ov, &n, TRUE);
    VirtualFree(r1, 0, MEM_RELEASE); VirtualFree(r2, 0, MEM_RELEASE);
out:
    CloseHandle(ov.hEvent); CLOSE(h);
    VirtualFree(a, 0, MEM_RELEASE); VirtualFree(b, 0, MEM_RELEASE); rmfile(p);
}
TEST(fio_scatter_requires_overlapped_handle) {
    REQUIRE(U(ReadFileScatter)); REQUIRE(U(WriteFileGather)); REQUIRE(U(CreateFileA));
    TP(p, "sg_sync.bin"); mkfile_pat(p, 8192);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, 0, OPEN_EXISTING, 0); OK_H(h);
    char *pgbuf = (char *)VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    FILE_SEGMENT_ELEMENT seg[2]; memset(seg, 0, sizeof seg); seg[0].Buffer = PtrToPtr64(pgbuf);
    OVERLAPPED ov; memset(&ov, 0, sizeof ov);
    SET_ERR(0);
    BOOL ok = U(ReadFileScatter)(h, seg, 4096, NULL, &ov);
    t_note("ReadFileScatter(sync handle) = %d gle=%lu", ok, GetLastError()); CHECK(!ok);
    memset(&ov, 0, sizeof ov);
    SET_ERR(0);
    ok = U(WriteFileGather)(h, seg, 4096, NULL, &ov);
    t_note("WriteFileGather(sync handle) = %d gle=%lu", ok, GetLastError()); CHECK(!ok);
    VirtualFree(pgbuf, 0, MEM_RELEASE);
    EXPECT_EQ(fsize_of(p), 8192);
    CLOSE(h); rmfile(p);
}

/* ---------- misc missed edges ---------- */
TEST(fio_createfile_empty_write_does_not_alter_timestamps_or_size) {
    REQUIRE(U(WriteFile)); REQUIRE(U(CreateFileA));
    TP(p, "zero_w.bin"); mkfile_pat(p, 10);
    HANDLE h = cfa(p, GENERIC_WRITE, FI_SHARE, OPEN_EXISTING, 0); OK_H(h);
    DWORD n = 77; CHECK(U(WriteFile)(h, "x", 0, &n, NULL)); EXPECT_EQ(n, 0);
    EXPECT_EQ(hsize(h), 10); EXPECT_EQ(fpos(h), 0);
    hsetpos(h, 4);
    CHECK(U(WriteFile)(h, NULL, 0, &n, NULL)); EXPECT_EQ(n, 0); EXPECT_EQ(fpos(h), 4);
    CLOSE(h); rmfile(p);
}
TEST(fio_getfilesize_directory_handle_and_pipe) {
    REQUIRE(U(GetFileSizeEx)); REQUIRE(U(CreateFileA));
    HANDLE h = cfa(t_tmpdir(), GENERIC_READ, FI_SHARE, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS); OK_H(h);
    LARGE_INTEGER s; s.QuadPart = -1;
    BOOL ok = U(GetFileSizeEx)(h, &s);
    t_note("GetFileSizeEx(dir) = %d size=%lld gle=%lu", ok, s.QuadPart, GetLastError());
    CLOSE(h);
    HANDLE r, w; CHECK(CreatePipe(&r, &w, NULL, 0));
    SET_ERR(0);
    ok = U(GetFileSizeEx)(r, &s);
    t_note("GetFileSizeEx(pipe) = %d gle=%lu", ok, GetLastError());
    CloseHandle(r); CloseHandle(w);
}
TEST(fio_setfilepointerex_null_new_position) {
    REQUIRE(U(SetFilePointerEx)); REQUIRE(U(CreateFileA));
    TP(p, "sfpex_null.bin"); mkfile_pat(p, 20);
    HANDLE h = cfa(p, GENERIC_READ, FI_SHARE, OPEN_EXISTING, 0); OK_H(h);
    LARGE_INTEGER d; d.QuadPart = 7;
    CHECK(U(SetFilePointerEx)(h, d, NULL, FILE_BEGIN)); EXPECT_EQ(fpos(h), 7);
    d.QuadPart = -3;
    CHECK(U(SetFilePointerEx)(h, d, NULL, FILE_CURRENT)); EXPECT_EQ(fpos(h), 4);
    d.QuadPart = -4;
    CHECK(U(SetFilePointerEx)(h, d, NULL, FILE_END)); EXPECT_EQ(fpos(h), 16);
    d.QuadPart = -5;
    SET_ERR(0);
    CHECK(!U(SetFilePointerEx)(h, d, NULL, FILE_BEGIN)); EXPECT_ERR(ERROR_NEGATIVE_SEEK);
    EXPECT_EQ(fpos(h), 16);
    CLOSE(h); rmfile(p);
}
