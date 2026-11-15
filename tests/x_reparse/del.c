/* FSCTL_DELETE_REPARSE_POINT: tag and length rules, resulting directory state. */
#include "xrp.h"

static DWORD del_raw(const WCHAR *p, DWORD acc, const void *in, DWORD inl) {
    HANDLE h = open_link(p, acc);
    if (h == INVALID_HANDLE_VALUE) return 0xFFFF0000u | GetLastError();
    DWORD e = 0, n;
    xio(h, XFSCTL_DEL, in, inl, NULL, 0, &n, &e);
    CloseHandle(h);
    return e;
}
static int dir_is_empty(const WCHAR *d) {
    WCHAR pat[XP]; WIN32_FIND_DATAW fd; int cnt = 0;
    snwprintf(pat, XP, L"%ls\\*", d);
    HANDLE h = FindFirstFileW(pat, &fd);
    if (h == INVALID_HANDLE_VALUE) return -1;
    do { if (wcscmp(fd.cFileName, L".") && wcscmp(fd.cFileName, L"..")) cnt++; } while (FindNextFileW(h, &fd));
    FindClose(h);
    return cnt == 0;
}

TEST(xrp_del_correct_tag_gives_plain_dir) {
    BEGIN(xrp_del_correct_tag_gives_plain_dir);
    NEED(std_layout());
    BYTE hdr[XHDR]; memset(hdr, 0, sizeof hdr); wr32(hdr, TAG_MP);
    HANDLE h = open_link(P(L"j"), GENERIC_WRITE); OK_H(h);
    DWORD n = 0xDEADBEEF, e = 0;
    { BOOL ok_ = xio(h, XFSCTL_DEL, hdr, sizeof hdr, NULL, 0, &n, &e); CHECKF(ok_, "gle %lu", e); }
    EXPECT_EQ(n, 0);
    CLOSE(h);
    EXPECT_EQ(GetFileAttributesW(P(L"j")), FILE_ATTRIBUTE_DIRECTORY);
    EXPECT_EQ(dir_is_empty(P(L"j")), 1);
    CHECK(!existsw(P(L"j\\f.txt")));
    EXPECT_BODY(P(L"t\\f.txt"), "target body");      /* target untouched */
    CHECK(RemoveDirectoryW(P(L"j")));
    xrp_end();
}
TEST(xrp_del_then_get_not_reparse) {
    BEGIN(xrp_del_then_get_not_reparse);
    NEED(mkdirw(P(L"t")) && mkjunc(P(L"j"), P(L"t")));
    EXPECT_EQ(del_rp_tag(P(L"j"), TAG_MP), 0);
    BYTE out[512]; DWORD n;
    EXPECT_EQ(get_rp(P(L"j"), out, sizeof out, &n), ERROR_NOT_A_REPARSE_POINT);
    EXPECT_EQ(del_rp_tag(P(L"j"), TAG_MP), ERROR_NOT_A_REPARSE_POINT);
    xrp_end();
}
TEST(xrp_del_wrong_tag_mismatch) {
    BEGIN(xrp_del_wrong_tag_mismatch);
    NEED(mkdirw(P(L"t")) && mkjunc(P(L"j"), P(L"t")));
    EXPECT_EQ(del_rp_tag(P(L"j"), TAG_SL), ERROR_REPARSE_TAG_MISMATCH);
    EXPECT_EQ(del_rp_tag(P(L"j"), 0x80000017u), ERROR_REPARSE_TAG_MISMATCH);   /* another Microsoft tag (WOF) */
    EXPECT_EQ(GetFileAttributesW(P(L"j")), FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT);
    xrp_end();
}
TEST(xrp_del_invalid_tag_value) {
    BEGIN(xrp_del_invalid_tag_value);
    NEED(mkdirw(P(L"t")) && mkjunc(P(L"j"), P(L"t")));
    EXPECT_EQ(del_rp_tag(P(L"j"), 0), ERROR_REPARSE_TAG_INVALID);
    EXPECT_EQ(del_rp_tag(P(L"j"), 1), ERROR_REPARSE_TAG_INVALID);
    CHECK(GetFileAttributesW(P(L"j")) & FILE_ATTRIBUTE_REPARSE_POINT);
    xrp_end();
}
TEST(xrp_del_nonzero_data_length) {
    BEGIN(xrp_del_nonzero_data_length);
    NEED(mkdirw(P(L"t")) && mkjunc(P(L"j"), P(L"t")));
    BYTE b[XHDR + 8]; memset(b, 0, sizeof b); wr32(b, TAG_MP); wr16(b + 4, 8);
    EXPECT_EQ(del_raw(P(L"j"), GENERIC_WRITE, b, sizeof b), ERROR_INVALID_REPARSE_DATA);
    /* data length non-zero but only the header supplied */
    EXPECT_EQ(del_raw(P(L"j"), GENERIC_WRITE, b, XHDR), ERROR_INVALID_REPARSE_DATA);
    CHECK(GetFileAttributesW(P(L"j")) & FILE_ATTRIBUTE_REPARSE_POINT);
    xrp_end();
}
TEST(xrp_del_full_buffer_as_input) {
    BEGIN(xrp_del_full_buffer_as_input);
    NEED(mkdirw(P(L"t")) && mkjunc(P(L"j"), P(L"t")));
    rbuf r; mp_build(&r, P(L"t"));                    /* what GET returns is not a valid DELETE input */
    EXPECT_EQ(del_raw(P(L"j"), GENERIC_WRITE, r.b, r.len), ERROR_INVALID_REPARSE_DATA);
    CHECK(GetFileAttributesW(P(L"j")) & FILE_ATTRIBUTE_REPARSE_POINT);
    xrp_end();
}
TEST(xrp_del_input_length_rules) {
    BEGIN(xrp_del_input_length_rules);
    NEED(mkdirw(P(L"t")) && mkjunc(P(L"j"), P(L"t")));
    BYTE b[32]; memset(b, 0, sizeof b); wr32(b, TAG_MP);
    EXPECT_EQ(del_raw(P(L"j"), GENERIC_WRITE, NULL, 0), ERROR_INVALID_USER_BUFFER);
    EXPECT_EQ(del_raw(P(L"j"), GENERIC_WRITE, b, 4), ERROR_INVALID_REPARSE_DATA);
    EXPECT_EQ(del_raw(P(L"j"), GENERIC_WRITE, b, XHDR - 1), ERROR_INVALID_REPARSE_DATA);
    /* zero data length but trailing bytes after the header */
    EXPECT_EQ(del_raw(P(L"j"), GENERIC_WRITE, b, XHDR + 4), ERROR_INVALID_REPARSE_DATA);
    CHECK(GetFileAttributesW(P(L"j")) & FILE_ATTRIBUTE_REPARSE_POINT);
    xrp_end();
}
TEST(xrp_del_plain_dir_not_reparse) {
    BEGIN(xrp_del_plain_dir_not_reparse);
    NEED(mkdirw(P(L"d")));
    EXPECT_EQ(del_rp_tag(P(L"d"), TAG_MP), ERROR_NOT_A_REPARSE_POINT);
    xrp_end();
}
TEST(xrp_del_through_junction_handle) {
    BEGIN(xrp_del_through_junction_handle);
    NEED(std_layout());
    BYTE hdr[XHDR]; memset(hdr, 0, sizeof hdr); wr32(hdr, TAG_MP);
    HANDLE h = open_dir(P(L"j"), GENERIC_WRITE); OK_H(h);          /* this is t */
    DWORD n, e = 0;
    CHECK(!xio(h, XFSCTL_DEL, hdr, sizeof hdr, NULL, 0, &n, &e));
    EXPECT_EQ(e, ERROR_NOT_A_REPARSE_POINT);
    CLOSE(h);
    CHECK(GetFileAttributesW(P(L"j")) & FILE_ATTRIBUTE_REPARSE_POINT);
    xrp_end();
}
TEST(xrp_del_then_set_again) {
    BEGIN(xrp_del_then_set_again);
    NEED(mkdirw(P(L"t1")) && mkdirw(P(L"t2")) && mkfilew(P(L"t2\\b.txt"), "second"));
    NEED(mkjunc(P(L"j"), P(L"t1")));
    EXPECT_EQ(del_rp_tag(P(L"j"), TAG_MP), 0);
    rbuf r; mp_build(&r, P(L"t2"));
    EXPECT_EQ(set_rp(P(L"j"), &r), 0);
    EXPECT_BODY(P(L"j\\b.txt"), "second");
    /* a directory that became plain can hold entries again, and then refuses a new reparse point */
    EXPECT_EQ(del_rp_tag(P(L"j"), TAG_MP), 0);
    CHECK(mkfilew(P(L"j\\own.txt"), "own"));
    EXPECT_EQ(set_rp(P(L"j"), &r), ERROR_DIR_NOT_EMPTY);
    xrp_end();
}
TEST(xrp_del_guid_tag_rules) {
    BEGIN(xrp_del_guid_tag_rules);
    NEED(mkdirw(P(L"d")));
    rbuf r; g3_build(&r, TAG_3P, &xrp_guid, 8);
    NEED(set_rp(P(L"d"), &r) == 0);
    BYTE g[XGHDR];
    memset(g, 0, sizeof g); wr32(g, TAG_3P); memcpy(g + 8, &xrp_guid2, 16);
    EXPECT_EQ(del_raw(P(L"d"), GENERIC_WRITE, g, sizeof g), ERROR_REPARSE_ATTRIBUTE_CONFLICT);
    memset(g, 0, sizeof g); wr32(g, TAG_3P + 1); memcpy(g + 8, &xrp_guid, 16);
    EXPECT_EQ(del_raw(P(L"d"), GENERIC_WRITE, g, sizeof g), ERROR_REPARSE_TAG_MISMATCH);
    memset(g, 0, sizeof g); wr32(g, TAG_3P); memcpy(g + 8, &xrp_guid, 16);
    EXPECT_EQ(del_raw(P(L"d"), GENERIC_WRITE, g, sizeof g), 0);
    EXPECT_EQ(GetFileAttributesW(P(L"d")), FILE_ATTRIBUTE_DIRECTORY);
    xrp_end();
}
TEST(xrp_del_access_rules) {
    BEGIN(xrp_del_access_rules);
    NEED(mkdirw(P(L"t")) && mkjunc(P(L"j"), P(L"t")));
    BYTE hdr[XHDR]; memset(hdr, 0, sizeof hdr); wr32(hdr, TAG_MP);
    EXPECT_EQ(del_raw(P(L"j"), FILE_READ_ATTRIBUTES, hdr, sizeof hdr), ERROR_ACCESS_DENIED);
    EXPECT_EQ(del_raw(P(L"j"), GENERIC_READ, hdr, sizeof hdr), ERROR_ACCESS_DENIED);
    CHECK(GetFileAttributesW(P(L"j")) & FILE_ATTRIBUTE_REPARSE_POINT);
    EXPECT_EQ(del_raw(P(L"j"), FILE_WRITE_DATA, hdr, sizeof hdr), 0);
    EXPECT_EQ(GetFileAttributesW(P(L"j")), FILE_ATTRIBUTE_DIRECTORY);
    xrp_end();
}
TEST(xrp_del_keeps_link_attributes) {
    BEGIN(xrp_del_keeps_link_attributes);
    NEED(mkdirw(P(L"t")) && mkjunc(P(L"j"), P(L"t")));
    CHECK(SetFileAttributesW(P(L"j"), FILE_ATTRIBUTE_HIDDEN));
    EXPECT_EQ(GetFileAttributesW(P(L"j")), FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_HIDDEN);
    EXPECT_EQ(del_rp_tag(P(L"j"), TAG_MP), 0);
    EXPECT_EQ(GetFileAttributesW(P(L"j")), FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_HIDDEN);
    EXPECT_EQ(GetFileAttributesW(P(L"t")), FILE_ATTRIBUTE_DIRECTORY);
    xrp_end();
}
