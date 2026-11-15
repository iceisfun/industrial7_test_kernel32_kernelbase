/* Attributes and enumeration of a junction: GetFileAttributes(Ex)[A/W], FindFirstFile*, handle info classes. */
#include "xrp.h"

#define JATTR (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)

TEST(xrp_attr_getfileattributes_w) {
    BEGIN(xrp_attr_getfileattributes_w);
    NEED(std_layout());
    EXPECT_EQ(U(GetFileAttributesW)(P(L"j")), JATTR);
    EXPECT_EQ(U(GetFileAttributesW)(P(L"t")), FILE_ATTRIBUTE_DIRECTORY);
    /* entries below the junction are the target's */
    EXPECT_EQ(U(GetFileAttributesW)(P(L"j\\sub")), FILE_ATTRIBUTE_DIRECTORY);
    EXPECT_EQ(U(GetFileAttributesW)(P(L"j\\f.txt")), FILE_ATTRIBUTE_ARCHIVE);
    SetLastError(0);
    EXPECT_EQ(U(GetFileAttributesW)(P(L"j\\nope.txt")), INVALID_FILE_ATTRIBUTES);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    xrp_end();
}
TEST(xrp_attr_a_and_w_agree) {
    BEGIN(xrp_attr_a_and_w_agree);
    NEED(std_layout());
    char a[XP];
    const WCHAR *names[] = {L"j", L"j\\f.txt", L"j\\sub", L"t"};
    for (int i = 0; i < 4; i++) {
        xrp_acp(a, sizeof a, P(names[i]));
        DWORD wa = U(GetFileAttributesW)(P(names[i])), aa = U(GetFileAttributesA)(a);
        CHECKF(wa == aa && wa != INVALID_FILE_ATTRIBUTES, "entry %d: W=0x%lx A=0x%lx", i, wa, aa);
    }
    WIN32_FIND_DATAW fw; WIN32_FIND_DATAA fa;
    xrp_acp(a, sizeof a, P(L"j"));
    HANDLE h1 = U(FindFirstFileW)(P(L"j"), &fw), h2 = U(FindFirstFileA)(a, &fa);
    CHECK(h1 != INVALID_HANDLE_VALUE && h2 != INVALID_HANDLE_VALUE);
    if (h1 != INVALID_HANDLE_VALUE && h2 != INVALID_HANDLE_VALUE) {
        EXPECT_EQ(fw.dwFileAttributes, fa.dwFileAttributes);
        EXPECT_EQ(fw.dwReserved0, fa.dwReserved0);
        EXPECT_EQ(fa.dwReserved0, TAG_MP);
        EXPECT_STR(fa.cFileName, "j");
    }
    if (h1 != INVALID_HANDLE_VALUE) FindClose(h1);
    if (h2 != INVALID_HANDLE_VALUE) FindClose(h2);
    xrp_end();
}
TEST(xrp_attr_getfileattributesex) {
    BEGIN(xrp_attr_getfileattributesex);
    NEED(std_layout());
    WIN32_FILE_ATTRIBUTE_DATA d; memset(&d, 0xAB, sizeof d);
    CHECK(U(GetFileAttributesExW)(P(L"j"), GetFileExInfoStandard, &d));
    EXPECT_EQ(d.dwFileAttributes, JATTR);
    EXPECT_EQ(d.nFileSizeHigh, 0);
    EXPECT_EQ(d.nFileSizeLow, 0);
    /* times are the link's own, not the target's: give t fixed old times (2001-01-01) and
     * check that the link does not report them */
    FILETIME old; old.dwLowDateTime = 0xC89DC000u; old.dwHighDateTime = 0x01C07385u;
    HANDLE ht = open_dir(P(L"t"), FILE_WRITE_ATTRIBUTES); OK_H(ht);
    CHECK(SetFileTime(ht, &old, &old, &old));
    CLOSE(ht);
    WIN32_FILE_ATTRIBUTE_DATA dt;
    CHECK(GetFileAttributesExW(P(L"t"), GetFileExInfoStandard, &dt));
    EXPECT_EQ(CompareFileTime(&dt.ftCreationTime, &old), 0);
    EXPECT_EQ(CompareFileTime(&dt.ftLastWriteTime, &old), 0);
    CHECK(U(GetFileAttributesExW)(P(L"j"), GetFileExInfoStandard, &d));
    CHECK(CompareFileTime(&d.ftCreationTime, &old) > 0);
    CHECK(CompareFileTime(&d.ftLastWriteTime, &old) > 0);
    /* a file through the junction reports the target file's size */
    CHECK(U(GetFileAttributesExW)(P(L"j\\f.txt"), GetFileExInfoStandard, &d));
    EXPECT_EQ(d.nFileSizeLow, 11);
    EXPECT_EQ(d.dwFileAttributes, FILE_ATTRIBUTE_ARCHIVE);
    xrp_end();
}
TEST(xrp_attr_find_single_entry_tag) {
    BEGIN(xrp_attr_find_single_entry_tag);
    NEED(std_layout());
    WIN32_FIND_DATAW fd; memset(&fd, 0, sizeof fd);
    HANDLE h = U(FindFirstFileW)(P(L"j"), &fd);
    OK_H(h);
    if (h != INVALID_HANDLE_VALUE) {
        EXPECT_EQ(fd.dwFileAttributes, JATTR);
        EXPECT_EQ(fd.dwReserved0, TAG_MP);
        EXPECT_WEQ(fd.cFileName, L"j");
        SetLastError(0);
        CHECK(!U(FindNextFileW)(h, &fd));
        EXPECT_ERR(ERROR_NO_MORE_FILES);
        FindClose(h);
    }
    /* the target entry has no reparse attribute (dwReserved0 is only defined when it has) */
    h = U(FindFirstFileW)(P(L"t"), &fd);
    OK_H(h);
    if (h != INVALID_HANDLE_VALUE) { EXPECT_EQ(fd.dwFileAttributes, FILE_ATTRIBUTE_DIRECTORY); FindClose(h); }
    xrp_end();
}
TEST(xrp_attr_findex_basic_and_large_fetch) {
    BEGIN(xrp_attr_findex_basic_and_large_fetch);
    NEED(std_layout());
    WIN32_FIND_DATAW fd;
    FINDEX_INFO_LEVELS lv[] = {FindExInfoStandard, FindExInfoBasic};
    for (int i = 0; i < 2; i++) {
        memset(&fd, 0, sizeof fd);
        HANDLE h = U(FindFirstFileExW)(P(L"j"), lv[i], &fd, FindExSearchNameMatch, NULL, i ? FIND_FIRST_EX_LARGE_FETCH : 0);
        OK_H(h);
        if (h != INVALID_HANDLE_VALUE) {
            EXPECT_EQ(fd.dwReserved0, TAG_MP);
            EXPECT_EQ(fd.dwFileAttributes, JATTR);
            if (lv[i] == FindExInfoBasic) EXPECT_EQ(fd.cAlternateFileName[0], 0);
            FindClose(h);
        }
    }
    /* FindExSearchLimitToDirectories still returns the junction (it is a directory) */
    memset(&fd, 0, sizeof fd);
    HANDLE h = U(FindFirstFileExW)(P(L"j*"), FindExInfoBasic, &fd, FindExSearchLimitToDirectories, NULL, 0);
    OK_H(h);
    if (h != INVALID_HANDLE_VALUE) { EXPECT_WEQ(fd.cFileName, L"j"); EXPECT_EQ(fd.dwReserved0, TAG_MP); FindClose(h); }
    xrp_end();
}
/* collect names of dir\* (skipping . and ..) into a '|'-joined sorted-as-returned string */
static void list_names(const WCHAR *pat, WCHAR *out, DWORD cap, DWORD *tagj) {
    WIN32_FIND_DATAW fd; out[0] = 0;
    HANDLE h = U(FindFirstFileW)(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) { snwprintf(out, cap, L"<err %lu>", GetLastError()); return; }
    do {
        if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
        if (tagj && !wcscmp(fd.cFileName, L"j")) *tagj = fd.dwReserved0;
        wcscat(out, fd.cFileName); wcscat(out, L"|");
    } while (U(FindNextFileW)(h, &fd));
    FindClose(h);
}
TEST(xrp_attr_find_children_lists_target) {
    BEGIN(xrp_attr_find_children_lists_target);
    NEED(std_layout() && mkfilew(P(L"t\\g.txt"), "g"));
    WCHAR a[XP], b[XP];
    list_names(P(L"j\\*"), a, XP, NULL);
    list_names(P(L"t\\*"), b, XP, NULL);
    EXPECT_WEQ(a, L"f.txt|g.txt|sub|");
    EXPECT_WEQ(a, b);
    /* pattern filter through the junction */
    list_names(P(L"j\\*.txt"), a, XP, NULL);
    EXPECT_WEQ(a, L"f.txt|g.txt|");
    /* the parent lists j with its tag */
    DWORD tag = 0;
    list_names(P(L"*"), a, XP, &tag);
    EXPECT_WEQ(a, L"j|t|");
    EXPECT_EQ(tag, TAG_MP);
    xrp_end();
}
TEST(xrp_attr_find_dot_entries_through) {
    BEGIN(xrp_attr_find_dot_entries_through);
    NEED(std_layout());
    WIN32_FIND_DATAW fd;
    HANDLE h = U(FindFirstFileW)(P(L"j\\*"), &fd);
    OK_H(h);
    int dots = 0;
    if (h != INVALID_HANDLE_VALUE) {
        do { if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) { dots++; CHECK(!(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)); } }
        while (U(FindNextFileW)(h, &fd));
        FindClose(h);
    }
    EXPECT_EQ(dots, 2);
    xrp_end();
}
TEST(xrp_attr_tag_info_link_and_target) {
    BEGIN(xrp_attr_tag_info_link_and_target);
    NEED(std_layout());
    FILE_ATTRIBUTE_TAG_INFO ti;
    HANDLE h = open_link(P(L"j"), FILE_READ_ATTRIBUTES); OK_H(h);
    memset(&ti, 0xAB, sizeof ti);
    CHECK(U(GetFileInformationByHandleEx)(h, FileAttributeTagInfo, &ti, sizeof ti));
    EXPECT_EQ(ti.FileAttributes, JATTR);
    EXPECT_EQ(ti.ReparseTag, TAG_MP);
    CLOSE(h);
    h = open_dir(P(L"j"), FILE_READ_ATTRIBUTES); OK_H(h);
    memset(&ti, 0xAB, sizeof ti);
    CHECK(U(GetFileInformationByHandleEx)(h, FileAttributeTagInfo, &ti, sizeof ti));
    EXPECT_EQ(ti.FileAttributes, FILE_ATTRIBUTE_DIRECTORY);
    EXPECT_EQ(ti.ReparseTag, 0);
    CLOSE(h);
    /* short buffer */
    h = open_link(P(L"j"), FILE_READ_ATTRIBUTES); OK_H(h);
    SetLastError(0);
    CHECK(!U(GetFileInformationByHandleEx)(h, FileAttributeTagInfo, &ti, sizeof ti - 1));
    EXPECT_ERR(ERROR_BAD_LENGTH);
    CLOSE(h);
    xrp_end();
}
TEST(xrp_attr_basic_and_standard_info) {
    BEGIN(xrp_attr_basic_and_standard_info);
    NEED(std_layout());
    FILE_BASIC_INFO bi; FILE_STANDARD_INFO si;
    HANDLE h = open_link(P(L"j"), FILE_READ_ATTRIBUTES); OK_H(h);
    CHECK(U(GetFileInformationByHandleEx)(h, FileBasicInfo, &bi, sizeof bi));
    EXPECT_EQ(bi.FileAttributes, JATTR);
    CHECK(U(GetFileInformationByHandleEx)(h, FileStandardInfo, &si, sizeof si));
    EXPECT_EQ(si.Directory, TRUE);
    EXPECT_EQ(si.EndOfFile.QuadPart, 0);
    EXPECT_EQ(si.NumberOfLinks, 1);
    BY_HANDLE_FILE_INFORMATION hi;
    CHECK(U(GetFileInformationByHandle)(h, &hi));
    EXPECT_EQ(hi.dwFileAttributes, JATTR);
    EXPECT_EQ(hi.nNumberOfLinks, 1);
    CLOSE(h);
    h = open_dir(P(L"j"), FILE_READ_ATTRIBUTES); OK_H(h);
    CHECK(U(GetFileInformationByHandleEx)(h, FileBasicInfo, &bi, sizeof bi));
    EXPECT_EQ(bi.FileAttributes, FILE_ATTRIBUTE_DIRECTORY);
    CLOSE(h);
    xrp_end();
}
TEST(xrp_attr_dir_info_ea_size_is_tag) {
    BEGIN(xrp_attr_dir_info_ea_size_is_tag);
    NEED(std_layout());
    /* FILE_ID_BOTH_DIR_INFO of the parent: for a reparse point EaSize carries the tag */
    HANDLE h = open_dir(g_xbase, FILE_LIST_DIRECTORY | SYNCHRONIZE); OK_H(h);
    static BYTE buf[8192];
    int seen_j = 0, seen_t = 0;
    if (U(GetFileInformationByHandleEx)(h, FileIdBothDirectoryRestartInfo, buf, sizeof buf)) {
        BYTE *p = buf;
        for (;;) {
            FILE_ID_BOTH_DIR_INFO *e = (FILE_ID_BOTH_DIR_INFO *)p;
            if (e->FileNameLength == 2 && e->FileName[0] == L'j') {
                seen_j = 1; EXPECT_EQ(e->EaSize, TAG_MP); EXPECT_EQ(e->FileAttributes, JATTR);
                EXPECT_EQ((ULONGLONG)e->FileId.QuadPart, file_index(P(L"j"), 1));
            }
            if (e->FileNameLength == 2 && e->FileName[0] == L't') { seen_t = 1; EXPECT_EQ(e->EaSize, 0); }
            if (!e->NextEntryOffset) break;
            p += e->NextEntryOffset;
        }
    } else CHECKF(0, "GetFileInformationByHandleEx(FileIdBothDirectoryRestartInfo) gle %lu", GetLastError());
    CHECK(seen_j && seen_t);
    CLOSE(h);
    xrp_end();
}
TEST(xrp_attr_dir_info_through_handle) {
    BEGIN(xrp_attr_dir_info_through_handle);
    NEED(std_layout());
    /* enumerating a handle opened through j lists t's entries */
    HANDLE h = open_dir(P(L"j"), FILE_LIST_DIRECTORY | SYNCHRONIZE); OK_H(h);
    static BYTE buf[8192];
    int n = 0, f = 0;
    if (U(GetFileInformationByHandleEx)(h, FileIdBothDirectoryRestartInfo, buf, sizeof buf)) {
        BYTE *p = buf;
        for (;;) {
            FILE_ID_BOTH_DIR_INFO *e = (FILE_ID_BOTH_DIR_INFO *)p;
            n++;
            if (e->FileNameLength == 10 && !memcmp(e->FileName, L"f.txt", 10)) { f = 1; EXPECT_EQ(e->EndOfFile.QuadPart, 11); }
            if (!e->NextEntryOffset) break;
            p += e->NextEntryOffset;
        }
    }
    EXPECT_EQ(n, 4);                      /* . .. f.txt sub */
    CHECK(f);
    CLOSE(h);
    /* enumerating the link itself (OPEN_REPARSE_POINT) shows an empty directory */
    h = open_link(P(L"j"), FILE_LIST_DIRECTORY | SYNCHRONIZE); OK_H(h);
    n = 0;
    if (U(GetFileInformationByHandleEx)(h, FileIdBothDirectoryRestartInfo, buf, sizeof buf)) {
        BYTE *p = buf;
        for (;;) { FILE_ID_BOTH_DIR_INFO *e = (FILE_ID_BOTH_DIR_INFO *)p; n++; if (!e->NextEntryOffset) break; p += e->NextEntryOffset; }
    }
    EXPECT_EQ(n, 2);                      /* . .. */
    CLOSE(h);
    xrp_end();
}
TEST(xrp_attr_trailing_backslash) {
    BEGIN(xrp_attr_trailing_backslash);
    NEED(std_layout());
    WCHAR p[XP]; snwprintf(p, XP, L"%ls\\", P(L"j"));
    EXPECT_EQ(U(GetFileAttributesW)(p), JATTR);
    WIN32_FILE_ATTRIBUTE_DATA d;
    CHECK(U(GetFileAttributesExW)(p, GetFileExInfoStandard, &d));
    EXPECT_EQ(d.dwFileAttributes, JATTR);
    xrp_end();
}
TEST(xrp_attr_set_hidden_applies_to_link) {
    BEGIN(xrp_attr_set_hidden_applies_to_link);
    NEED(std_layout());
    CHECK(U(SetFileAttributesW)(P(L"j"), FILE_ATTRIBUTE_HIDDEN));
    EXPECT_EQ(GetFileAttributesW(P(L"j")), JATTR | FILE_ATTRIBUTE_HIDDEN);
    EXPECT_EQ(GetFileAttributesW(P(L"t")), FILE_ATTRIBUTE_DIRECTORY);
    /* a path below the junction sets the target entry */
    CHECK(U(SetFileAttributesW)(P(L"j\\f.txt"), FILE_ATTRIBUTE_READONLY));
    EXPECT_EQ(GetFileAttributesW(P(L"t\\f.txt")), FILE_ATTRIBUTE_READONLY);
    CHECK(U(SetFileAttributesW)(P(L"j\\f.txt"), FILE_ATTRIBUTE_NORMAL));
    xrp_end();
}
TEST(xrp_attr_set_normal_keeps_reparse_bit) {
    BEGIN(xrp_attr_set_normal_keeps_reparse_bit);
    NEED(std_layout());
    CHECK(U(SetFileAttributesW)(P(L"j"), FILE_ATTRIBUTE_NORMAL));
    EXPECT_EQ(GetFileAttributesW(P(L"j")), JATTR);
    CHECK(U(SetFileAttributesW)(P(L"j"), 0));
    EXPECT_EQ(GetFileAttributesW(P(L"j")), JATTR);
    /* REPARSE_POINT and DIRECTORY in the request are ignored on a junction */
    CHECK(U(SetFileAttributesW)(P(L"j"), JATTR | FILE_ATTRIBUTE_ARCHIVE));
    EXPECT_EQ(GetFileAttributesW(P(L"j")), JATTR | FILE_ATTRIBUTE_ARCHIVE);
    /* and cannot be added to a plain directory */
    CHECK(U(SetFileAttributesW)(P(L"t"), FILE_ATTRIBUTE_REPARSE_POINT));
    EXPECT_EQ(GetFileAttributesW(P(L"t")), FILE_ATTRIBUTE_DIRECTORY);
    xrp_end();
}
