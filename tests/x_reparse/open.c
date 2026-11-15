/* Opening through a junction versus opening the junction itself; ids, final and DOS path names. */
#include "xrp.h"

static ULONGLONG hidx(HANDLE h) {
    BY_HANDLE_FILE_INFORMATION bi;
    if (!U(GetFileInformationByHandle)(h, &bi)) return 0;
    return ((ULONGLONG)bi.nFileIndexHigh << 32) | bi.nFileIndexLow;
}

TEST(xrp_open_file_through_reaches_target) {
    BEGIN(xrp_open_file_through_reaches_target);
    NEED(std_layout());
    HANDLE h = U(CreateFileW)(P(L"j\\f.txt"), GENERIC_READ, XSH, NULL, OPEN_EXISTING, 0, NULL);
    OK_H(h);
    char b[64] = {0}; DWORD n = 0;
    CHECK(ReadFile(h, b, sizeof b - 1, &n, NULL));
    EXPECT_STR(b, "target body");
    ULONGLONG i1 = hidx(h);
    CLOSE(h);
    h = U(CreateFileW)(P(L"t\\f.txt"), FILE_READ_ATTRIBUTES, XSH, NULL, OPEN_EXISTING, 0, NULL);
    OK_H(h);
    EXPECT_EQ(hidx(h), i1);
    CLOSE(h);
    /* write through, read directly */
    h = U(CreateFileW)(P(L"j\\new.txt"), GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
    OK_H(h);
    CHECK(WriteFile(h, "via j", 5, &n, NULL));
    CLOSE(h);
    EXPECT_BODY(P(L"t\\new.txt"), "via j");
    xrp_end();
}
TEST(xrp_open_a_variant_through) {
    BEGIN(xrp_open_a_variant_through);
    NEED(std_layout());
    char a[XP]; xrp_acp(a, sizeof a, P(L"j\\f.txt"));
    HANDLE h = U(CreateFileA)(a, GENERIC_READ, XSH, NULL, OPEN_EXISTING, 0, NULL);
    OK_H(h);
    char b[64] = {0}; DWORD n = 0;
    CHECK(ReadFile(h, b, sizeof b - 1, &n, NULL));
    EXPECT_STR(b, "target body");
    CLOSE(h);
    xrp_acp(a, sizeof a, P(L"j"));
    h = U(CreateFileA)(a, FILE_READ_ATTRIBUTES, XSH, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    OK_H(h);
    FILE_ATTRIBUTE_TAG_INFO ti;
    CHECK(GetFileInformationByHandleEx(h, FileAttributeTagInfo, &ti, sizeof ti));
    EXPECT_EQ(ti.ReparseTag, TAG_MP);
    CLOSE(h);
    xrp_end();
}
TEST(xrp_open_dir_through_index_is_target) {
    BEGIN(xrp_open_dir_through_index_is_target);
    NEED(std_layout());
    HANDLE hj = U(CreateFileW)(P(L"j"), FILE_READ_ATTRIBUTES, XSH, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    HANDLE ht = U(CreateFileW)(P(L"t"), FILE_READ_ATTRIBUTES, XSH, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    OK_H(hj); OK_H(ht);
    BY_HANDLE_FILE_INFORMATION a, b;
    CHECK(U(GetFileInformationByHandle)(hj, &a));
    CHECK(U(GetFileInformationByHandle)(ht, &b));
    EXPECT_EQ(a.nFileIndexLow, b.nFileIndexLow);
    EXPECT_EQ(a.nFileIndexHigh, b.nFileIndexHigh);
    EXPECT_EQ(a.dwVolumeSerialNumber, b.dwVolumeSerialNumber);
    EXPECT_EQ(a.dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY);
    CLOSE(hj); CLOSE(ht);
    xrp_end();
}
TEST(xrp_open_reparse_flag_index_differs) {
    BEGIN(xrp_open_reparse_flag_index_differs);
    NEED(std_layout());
    HANDLE hl = U(CreateFileW)(P(L"j"), FILE_READ_ATTRIBUTES, XSH, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    HANDLE ht = open_dir(P(L"t"), FILE_READ_ATTRIBUTES);
    OK_H(hl); OK_H(ht);
    BY_HANDLE_FILE_INFORMATION a, b;
    CHECK(U(GetFileInformationByHandle)(hl, &a));
    CHECK(U(GetFileInformationByHandle)(ht, &b));
    CHECK(a.nFileIndexLow != b.nFileIndexLow || a.nFileIndexHigh != b.nFileIndexHigh);
    EXPECT_EQ(a.dwVolumeSerialNumber, b.dwVolumeSerialNumber);
    EXPECT_EQ(a.dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT);
    CLOSE(hl); CLOSE(ht);
    xrp_end();
}
TEST(xrp_open_file_id_info_link_vs_target) {
    BEGIN(xrp_open_file_id_info_link_vs_target);
    NEED(std_layout());
    FILE_ID_INFO il, ij, it;
    HANDLE hl = open_link(P(L"j"), FILE_READ_ATTRIBUTES), hj = open_dir(P(L"j"), FILE_READ_ATTRIBUTES), ht = open_dir(P(L"t"), FILE_READ_ATTRIBUTES);
    OK_H(hl); OK_H(hj); OK_H(ht);
    CHECK(U(GetFileInformationByHandleEx)(hl, FileIdInfo, &il, sizeof il));
    CHECK(U(GetFileInformationByHandleEx)(hj, FileIdInfo, &ij, sizeof ij));
    CHECK(U(GetFileInformationByHandleEx)(ht, FileIdInfo, &it, sizeof it));
    CHECK(!memcmp(&ij.FileId, &it.FileId, sizeof it.FileId));
    CHECK(memcmp(&il.FileId, &it.FileId, sizeof it.FileId) != 0);
    EXPECT_EQ(il.VolumeSerialNumber, it.VolumeSerialNumber);
    /* on NTFS the low 64 bits of the 128-bit id are the file index */
    ULONGLONG lo; memcpy(&lo, il.FileId.Identifier, 8);
    EXPECT_EQ(lo, hidx(hl));
    CLOSE(hl); CLOSE(hj); CLOSE(ht);
    xrp_end();
}
TEST(xrp_open_without_backup_semantics) {
    BEGIN(xrp_open_without_backup_semantics);
    NEED(std_layout());
    SetLastError(0);
    HANDLE h = U(CreateFileW)(P(L"j"), GENERIC_READ, XSH, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CLOSE(h);
    SetLastError(0);
    h = U(CreateFileW)(P(L"j"), GENERIC_READ, XSH, NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    CHECK(h == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CLOSE(h);
    xrp_end();
}
TEST(xrp_open_dispositions_on_junction) {
    BEGIN(xrp_open_dispositions_on_junction);
    NEED(std_layout());
    SetLastError(0);
    HANDLE h = U(CreateFileW)(P(L"j"), GENERIC_READ, XSH, NULL, CREATE_NEW, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    CHECK(h == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_FILE_EXISTS);
    CLOSE(h);
    h = U(CreateFileW)(P(L"j"), FILE_READ_ATTRIBUTES, XSH, NULL, OPEN_ALWAYS, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    DWORD e = GetLastError();
    OK_H(h);
    EXPECT_EQ(e, ERROR_ALREADY_EXISTS);
    CLOSE(h);
    CHECK(GetFileAttributesW(P(L"j")) & FILE_ATTRIBUTE_REPARSE_POINT);
    xrp_end();
}
TEST(xrp_final_path_through_reports_target) {
    BEGIN(xrp_final_path_through_reports_target);
    NEED(std_layout());
    WCHAR want[XP], got[XP];
    NEED(final_of(P(L"t"), 0, want, XP));
    HANDLE h = open_dir(P(L"j"), FILE_READ_ATTRIBUTES); OK_H(h);
    DWORD n = U(GetFinalPathNameByHandleW)(h, got, XP, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    EXPECT_EQ(n, wcslen(want));
    EXPECT_WEQ(got, want);
    n = U(GetFinalPathNameByHandleW)(h, got, XP, FILE_NAME_OPENED | VOLUME_NAME_DOS);
    EXPECT_WEQ(got, want);
    CHECK(!wcsncmp(got, L"\\\\?\\", 4));
    CLOSE(h);
    /* a file below the junction */
    h = CreateFileW(P(L"j\\f.txt"), FILE_READ_ATTRIBUTES, XSH, NULL, OPEN_EXISTING, 0, NULL); OK_H(h);
    U(GetFinalPathNameByHandleW)(h, got, XP, 0);
    wcscat(want, L"\\f.txt");
    EXPECT_WEQ(got, want);
    CLOSE(h);
    xrp_end();
}
TEST(xrp_final_path_link_handle_reports_link) {
    BEGIN(xrp_final_path_link_handle_reports_link);
    NEED(std_layout());
    WCHAR want[XP], got[XP];
    NEED(final_of(g_xbase, 0, want, XP));
    wcscat(want, L"\\j");
    HANDLE h = open_link(P(L"j"), FILE_READ_ATTRIBUTES); OK_H(h);
    U(GetFinalPathNameByHandleW)(h, got, XP, 0);
    EXPECT_WEQ(got, want);
    CLOSE(h);
    xrp_end();
}
TEST(xrp_final_path_volume_forms_through) {
    BEGIN(xrp_final_path_volume_forms_through);
    NEED(std_layout());
    HANDLE hj = open_dir(P(L"j"), FILE_READ_ATTRIBUTES), ht = open_dir(P(L"t"), FILE_READ_ATTRIBUTES);
    OK_H(hj); OK_H(ht);
    DWORD fl[] = {VOLUME_NAME_NONE, VOLUME_NAME_NT, VOLUME_NAME_GUID, VOLUME_NAME_NONE | FILE_NAME_OPENED};
    for (int i = 0; i < 4; i++) {
        WCHAR a[XP], b[XP]; a[0] = b[0] = 0;
        DWORD na = U(GetFinalPathNameByHandleW)(hj, a, XP, fl[i]);
        DWORD nb = U(GetFinalPathNameByHandleW)(ht, b, XP, fl[i]);
        CHECKF(na && na == nb && !wcscmp(a, b), "flags 0x%lx: %lu vs %lu", fl[i], na, nb);
    }
    WCHAR a[XP];
    U(GetFinalPathNameByHandleW)(hj, a, XP, VOLUME_NAME_NONE);
    size_t L = wcslen(a);
    CHECK(L >= 2 && a[L - 2] == L'\\' && a[L - 1] == L't');
    /* size query reports the length including the terminator */
    WCHAR tiny[2];
    DWORD need = U(GetFinalPathNameByHandleW)(hj, tiny, 2, VOLUME_NAME_NONE);
    EXPECT_EQ(need, L + 1);
    CLOSE(hj); CLOSE(ht);
    xrp_end();
}
TEST(xrp_fullpath_does_not_resolve) {
    BEGIN(xrp_fullpath_does_not_resolve);
    NEED(std_layout());
    WCHAR out[XP], *fp = NULL;
    DWORD n = U(GetFullPathNameW)(P(L"j\\f.txt"), XP, out, &fp);
    EXPECT_WEQ(out, P(L"j\\f.txt"));
    EXPECT_EQ(n, wcslen(P(L"j\\f.txt")));
    CHECK(fp && !wcscmp(fp, L"f.txt"));
    /* ".." after the junction is removed textually: it goes back to the link's parent */
    U(GetFullPathNameW)(P(L"j\\..\\x.txt"), XP, out, NULL);
    EXPECT_WEQ(out, P(L"x.txt"));
    U(GetFullPathNameW)(P(L"j\\.\\sub\\..\\f.txt"), XP, out, NULL);
    EXPECT_WEQ(out, P(L"j\\f.txt"));
    xrp_end();
}
TEST(xrp_dotdot_after_junction_win32_and_nt) {
    BEGIN(xrp_dotdot_after_junction_win32_and_nt);
    /* target is two levels deep, so its parent differs from the link's parent */
    NEED(mkdirw(P(L"a")) && mkdirw(P(L"a\\b")) && mkdirw(P(L"a\\b\\t")) && mkjunc(P(L"j"), P(L"a\\b\\t")));
    HANDLE h = U(CreateFileW)(P(L"j\\..\\x.txt"), GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
    OK_H(h); CLOSE(h);
    CHECK(existsw(P(L"x.txt")));
    CHECK(!existsw(P(L"a\\b\\x.txt")));
    /* with \\?\ no Win32 normalisation happens: the file system sees the ".." */
    WCHAR raw[XP]; snwprintf(raw, XP, L"\\\\?\\%ls", P(L"j\\..\\y.txt"));
    SetLastError(0);
    h = U(CreateFileW)(raw, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
    DWORD e = GetLastError();
    CHECK(h == INVALID_HANDLE_VALUE);
    EXPECT_EQ(e, ERROR_INVALID_NAME);
    CLOSE(h);
    CHECK(!existsw(P(L"y.txt")) && !existsw(P(L"a\\b\\y.txt")));
    xrp_end();
}
TEST(xrp_longpath_does_not_resolve) {
    BEGIN(xrp_longpath_does_not_resolve);
    NEED(std_layout());
    WCHAR out[XP];
    DWORD n = U(GetLongPathNameW)(P(L"j\\f.txt"), out, XP);
    EXPECT_WEQ(out, P(L"j\\f.txt"));
    EXPECT_EQ(n, wcslen(P(L"j\\f.txt")));
    n = U(GetLongPathNameW)(P(L"j\\sub"), out, XP);
    EXPECT_WEQ(out, P(L"j\\sub"));
    SetLastError(0);
    n = U(GetLongPathNameW)(P(L"j\\missing.txt"), out, XP);
    EXPECT_EQ(n, 0);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    xrp_end();
}
TEST(xrp_volume_path_through_junction) {
    BEGIN(xrp_volume_path_through_junction);
    NEED(std_layout());
    WCHAR vb[XP], vj[XP], vf[XP];
    CHECK(U(GetVolumePathNameW)(g_xbase, vb, XP));
    CHECK(U(GetVolumePathNameW)(P(L"j"), vj, XP));
    CHECK(U(GetVolumePathNameW)(P(L"j\\f.txt"), vf, XP));
    EXPECT_WIEQ(vj, vb);
    EXPECT_WIEQ(vf, vb);
    CHECK(wcslen(vb) == 3 && vb[1] == L':' && vb[2] == L'\\');
    xrp_end();
}
TEST(xrp_openbyid_link_with_reparse_flag) {
    BEGIN(xrp_openbyid_link_with_reparse_flag);
    NEED(std_layout());
    HANDLE hint = open_dir(g_xbase, FILE_READ_ATTRIBUTES); OK_H(hint);
    FILE_ID_DESCRIPTOR fid; memset(&fid, 0, sizeof fid);
    fid.dwSize = sizeof fid; fid.Type = FileIdType; fid.FileId.QuadPart = (LONGLONG)file_index(P(L"j"), 1);
    HANDLE h = U(OpenFileById)(hint, &fid, FILE_READ_ATTRIBUTES, XSH, NULL, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT);
    OK_H(h);
    if (h != INVALID_HANDLE_VALUE) {
        FILE_ATTRIBUTE_TAG_INFO ti;
        CHECK(GetFileInformationByHandleEx(h, FileAttributeTagInfo, &ti, sizeof ti));
        EXPECT_EQ(ti.ReparseTag, TAG_MP);
        EXPECT_EQ(hidx(h), (ULONGLONG)fid.FileId.QuadPart);
    }
    CLOSE(h);
    /* the 128-bit form names the same object */
    FILE_ID_INFO ii; HANDLE hl = open_link(P(L"j"), FILE_READ_ATTRIBUTES);
    CHECK(GetFileInformationByHandleEx(hl, FileIdInfo, &ii, sizeof ii));
    CLOSE(hl);
    memset(&fid, 0, sizeof fid); fid.dwSize = sizeof fid; fid.Type = ExtendedFileIdType; fid.ExtendedFileId = ii.FileId;
    h = U(OpenFileById)(hint, &fid, FILE_READ_ATTRIBUTES, XSH, NULL, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT);
    OK_H(h);
    if (h != INVALID_HANDLE_VALUE) { FILE_BASIC_INFO bi; CHECK(GetFileInformationByHandleEx(h, FileBasicInfo, &bi, sizeof bi)); EXPECT_EQ(bi.FileAttributes, FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT); }
    CLOSE(h);
    CLOSE(hint);
    xrp_end();
}
TEST(xrp_openbyid_link_without_reparse_flag) {
    BEGIN(xrp_openbyid_link_without_reparse_flag);
    NEED(std_layout());
    HANDLE hint = open_dir(g_xbase, FILE_READ_ATTRIBUTES); OK_H(hint);
    FILE_ID_DESCRIPTOR fid; memset(&fid, 0, sizeof fid);
    fid.dwSize = sizeof fid; fid.Type = FileIdType; fid.FileId.QuadPart = (LONGLONG)file_index(P(L"j"), 1);
    SetLastError(0);
    HANDLE h = U(OpenFileById)(hint, &fid, FILE_READ_ATTRIBUTES, XSH, NULL, FILE_FLAG_BACKUP_SEMANTICS);
    DWORD e = GetLastError();
    /* an open by id cannot be reparsed (there is no name to substitute into) */
    CHECK(h == INVALID_HANDLE_VALUE);
    EXPECT_EQ(e, ERROR_INVALID_PARAMETER);
    CLOSE(h);
    /* a plain directory's id opens fine without the flag */
    fid.FileId.QuadPart = (LONGLONG)file_index(P(L"t"), 0);
    h = U(OpenFileById)(hint, &fid, FILE_READ_ATTRIBUTES, XSH, NULL, FILE_FLAG_BACKUP_SEMANTICS);
    OK_H(h);
    CLOSE(h);
    CLOSE(hint);
    xrp_end();
}
TEST(xrp_openbyid_target_and_errors) {
    BEGIN(xrp_openbyid_target_and_errors);
    NEED(std_layout());
    HANDLE hint = open_dir(g_xbase, FILE_READ_ATTRIBUTES); OK_H(hint);
    FILE_ID_DESCRIPTOR fid; memset(&fid, 0, sizeof fid);
    fid.dwSize = sizeof fid; fid.Type = FileIdType; fid.FileId.QuadPart = (LONGLONG)file_index(P(L"t"), 0);
    HANDLE h = U(OpenFileById)(hint, &fid, FILE_READ_ATTRIBUTES, XSH, NULL, FILE_FLAG_BACKUP_SEMANTICS);
    OK_H(h);
    if (h != INVALID_HANDLE_VALUE) EXPECT_EQ(hidx(h), file_index(P(L"j"), 0));
    CLOSE(h);
    /* after RemoveDirectory of the link its id no longer opens */
    ULONGLONG lid = file_index(P(L"j"), 1);
    CHECK(RemoveDirectoryW(P(L"j")));
    fid.FileId.QuadPart = (LONGLONG)lid;
    SetLastError(0);
    h = U(OpenFileById)(hint, &fid, FILE_READ_ATTRIBUTES, XSH, NULL, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT);
    DWORD e = GetLastError();
    CHECK(h == INVALID_HANDLE_VALUE);
    CHECKF(e == ERROR_INVALID_PARAMETER, "gle %lu", e);
    CLOSE(h);
    CLOSE(hint);
    xrp_end();
}
TEST(xrp_open_junction_to_file_refused) {
    BEGIN(xrp_open_junction_to_file_refused);
    /* a mount point naming a file can be stored, but a following open does not land on the file */
    NEED(mkfilew(P(L"f.txt"), "file target") && mkjunc(P(L"j"), P(L"f.txt")));
    EXPECT_EQ(GetFileAttributesW(P(L"j")), FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT);
    SetLastError(0);
    HANDLE h = U(CreateFileW)(P(L"j"), GENERIC_READ, XSH, NULL, OPEN_EXISTING, 0, NULL);
    DWORD e = GetLastError();
    CHECK(h == INVALID_HANDLE_VALUE);
    EXPECT_EQ(e, ERROR_ACCESS_DENIED);
    CLOSE(h);
    SetLastError(0);
    h = U(CreateFileW)(P(L"j"), FILE_READ_ATTRIBUTES, XSH, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    e = GetLastError();
    CHECK(h == INVALID_HANDLE_VALUE);
    EXPECT_EQ(e, ERROR_DIRECTORY);
    CLOSE(h);
    EXPECT_BODY(P(L"f.txt"), "file target");
    xrp_end();
}
TEST(xrp_open_print_name_ignored) {
    BEGIN(xrp_open_print_name_ignored);
    NEED(mkdirw(P(L"real")) && mkdirw(P(L"shown")) && mkfilew(P(L"real\\w.txt"), "real"));
    WCHAR s[XP]; snwprintf(s, XP, L"\\??\\%ls", P(L"real"));
    rbuf r; mp_build_raw(&r, s, P(L"shown"), 0);
    NEED(mkdirw(P(L"j")) && set_rp(P(L"j"), &r) == 0);
    EXPECT_BODY(P(L"j\\w.txt"), "real");
    WCHAR want[XP], got[XP];
    NEED(final_of(P(L"real"), 0, want, XP));
    final_of(P(L"j"), 0, got, XP);
    EXPECT_WEQ(got, want);
    /* an empty print name is also fine */
    rbuf r2; mp_build_raw(&r2, s, L"", 0);
    NEED(mkdirw(P(L"j2")) && set_rp(P(L"j2"), &r2) == 0);
    EXPECT_BODY(P(L"j2\\w.txt"), "real");
    xrp_end();
}
TEST(xrp_open_substitute_forms) {
    BEGIN(xrp_open_substitute_forms);
    NEED(mkdirw(P(L"t")) && mkfilew(P(L"t\\z.txt"), "zz"));
    WCHAR s[XP];
    /* trailing backslash on the substitute name */
    snwprintf(s, XP, L"\\??\\%ls\\", P(L"t"));
    rbuf r; mp_build_raw(&r, s, P(L"t"), 0);
    NEED(mkdirw(P(L"jb")) && set_rp(P(L"jb"), &r) == 0);
    EXPECT_BODY(P(L"jb\\z.txt"), "zz");
    /* DOS form without \??\ : stored as given, not resolvable */
    mp_build_raw(&r, P(L"t"), P(L"t"), 0);
    NEED(mkdirw(P(L"jd")) && set_rp(P(L"jd"), &r) == 0);
    SetLastError(0);
    HANDLE h = CreateFileW(P(L"jd\\z.txt"), GENERIC_READ, XSH, NULL, OPEN_EXISTING, 0, NULL);
    DWORD e = GetLastError();
    CHECK(h == INVALID_HANDLE_VALUE);
    EXPECT_EQ(e, ERROR_INVALID_NAME);
    CLOSE(h);
    /* relative substitute name */
    mp_build_raw(&r, L"t", L"t", 0);
    NEED(mkdirw(P(L"jr")) && set_rp(P(L"jr"), &r) == 0);
    SetLastError(0);
    h = CreateFileW(P(L"jr\\z.txt"), GENERIC_READ, XSH, NULL, OPEN_EXISTING, 0, NULL);
    e = GetLastError();
    CHECK(h == INVALID_HANDLE_VALUE);
    EXPECT_EQ(e, ERROR_INVALID_NAME);
    CLOSE(h);
    xrp_end();
}
