/* FSCTL_SET_REPARSE_POINT: success, target state, buffer validation, access, tags. */
#include "xrp.h"

/* set buffer r on a fresh empty dir base\d through U(DeviceIoControl); return GLE */
static DWORD set_on_new_dir(const rbuf *r) {
    mkdirw(P(L"d"));
    return set_rp(P(L"d"), r);
}

TEST(xrp_set_empty_dir_succeeds) {
    BEGIN(xrp_set_empty_dir_succeeds);
    NEED(mkdirw(P(L"t")) && mkdirw(P(L"d")));
    rbuf r; mp_build(&r, P(L"t"));
    HANDLE h = open_link(P(L"d"), GENERIC_WRITE);
    OK_H(h);
    DWORD n = 0xDEADBEEF, e = 0;
    BOOL ok = xio(h, XFSCTL_SET, r.b, r.len, NULL, 0, &n, &e);
    CHECKF(ok, "gle %lu", e);
    EXPECT_EQ(n, 0);
    CLOSE(h);
    EXPECT_EQ(GetFileAttributesW(P(L"d")), FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT);
    EXPECT_EQ(GetFileAttributesW(P(L"t")), FILE_ATTRIBUTE_DIRECTORY);
    xrp_end();
}
TEST(xrp_set_with_output_buffer_refused) {
    BEGIN(xrp_set_with_output_buffer_refused);
    NEED(mkdirw(P(L"t")) && mkdirw(P(L"d")));
    rbuf r; mp_build(&r, P(L"t"));
    HANDLE h = open_link(P(L"d"), GENERIC_WRITE);
    OK_H(h);
    /* SET takes no output: a non-empty output buffer is a parameter error and nothing is stored */
    BYTE out[16]; DWORD n = 0xDEADBEEF, e = 0;
    CHECK(!xio(h, XFSCTL_SET, r.b, r.len, out, sizeof out, &n, &e));
    EXPECT_EQ(e, ERROR_INVALID_PARAMETER);
    EXPECT_EQ(n, 0xDEADBEEF);                         /* BytesReturned is not written on failure */
    EXPECT_EQ(GetFileAttributesW(P(L"d")), FILE_ATTRIBUTE_DIRECTORY);
    /* an output pointer with zero length is accepted */
    CHECK(xio(h, XFSCTL_SET, r.b, r.len, out, 0, &n, &e));
    EXPECT_EQ(e, 0);
    CLOSE(h);
    EXPECT_EQ(GetFileAttributesW(P(L"d")), FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT);
    xrp_end();
}
TEST(xrp_set_nonempty_dir_with_file) {
    BEGIN(xrp_set_nonempty_dir_with_file);
    NEED(mkdirw(P(L"t")) && mkdirw(P(L"d")) && mkfilew(P(L"d\\x.txt"), "x"));
    rbuf r; mp_build(&r, P(L"t"));
    EXPECT_EQ(set_rp(P(L"d"), &r), ERROR_DIR_NOT_EMPTY);
    EXPECT_EQ(GetFileAttributesW(P(L"d")), FILE_ATTRIBUTE_DIRECTORY);
    EXPECT_BODY(P(L"d\\x.txt"), "x");
    xrp_end();
}
TEST(xrp_set_nonempty_dir_with_subdir) {
    BEGIN(xrp_set_nonempty_dir_with_subdir);
    NEED(mkdirw(P(L"t")) && mkdirw(P(L"d")) && mkdirw(P(L"d\\s")));
    rbuf r; mp_build(&r, P(L"t"));
    EXPECT_EQ(set_rp(P(L"d"), &r), ERROR_DIR_NOT_EMPTY);
    CHECK(existsw(P(L"d\\s")));
    xrp_end();
}
TEST(xrp_set_regular_file_refused) {
    BEGIN(xrp_set_regular_file_refused);
    NEED(mkdirw(P(L"t")) && mkfilew(P(L"f.txt"), "file"));
    rbuf r; mp_build(&r, P(L"t"));
    HANDLE h = CreateFileW(P(L"f.txt"), GENERIC_READ | GENERIC_WRITE, XSH, NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    OK_H(h);
    DWORD n, e = 0;
    CHECK(!xio(h, XFSCTL_SET, r.b, r.len, NULL, 0, &n, &e));
    EXPECT_EQ(e, ERROR_DIRECTORY);                    /* a mount point needs a directory */
    CLOSE(h);
    EXPECT_EQ(GetFileAttributesW(P(L"f.txt")), FILE_ATTRIBUTE_ARCHIVE);
    EXPECT_BODY(P(L"f.txt"), "file");
    xrp_end();
}
TEST(xrp_set_datalength_too_small) {
    BEGIN(xrp_set_datalength_too_small);
    rbuf r; mp_build(&r, P(L"t"));
    wr16(r.b + 4, (USHORT)(rd16(r.b + 4) - 2));      /* header says 2 bytes less than supplied */
    EXPECT_EQ(set_on_new_dir(&r), ERROR_INVALID_REPARSE_DATA);
    EXPECT_EQ(GetFileAttributesW(P(L"d")), FILE_ATTRIBUTE_DIRECTORY);
    xrp_end();
}
TEST(xrp_set_datalength_too_large) {
    BEGIN(xrp_set_datalength_too_large);
    rbuf r; mp_build(&r, P(L"t"));
    wr16(r.b + 4, (USHORT)(rd16(r.b + 4) + 2));      /* header claims more than supplied */
    EXPECT_EQ(set_on_new_dir(&r), ERROR_INVALID_REPARSE_DATA);
    xrp_end();
}
TEST(xrp_set_input_shorter_than_header) {
    BEGIN(xrp_set_input_shorter_than_header);
    rbuf r; mp_build(&r, P(L"t"));
    mkdirw(P(L"d"));
    for (DWORD len = 1; len < XHDR; len += 3) {
        DWORD e = set_rp_len(P(L"d"), r.b, len);
        CHECKF(e == ERROR_INVALID_REPARSE_DATA, "len=%lu gle=%lu", len, e);
    }
    EXPECT_EQ(GetFileAttributesW(P(L"d")), FILE_ATTRIBUTE_DIRECTORY);
    xrp_end();
}
TEST(xrp_set_null_input) {
    BEGIN(xrp_set_null_input);
    mkdirw(P(L"d"));
    EXPECT_EQ(set_rp_len(P(L"d"), NULL, 0), ERROR_INVALID_USER_BUFFER);
    EXPECT_EQ(set_rp_len(P(L"d"), NULL, 64), ERROR_INVALID_USER_BUFFER);
    EXPECT_EQ(GetFileAttributesW(P(L"d")), FILE_ATTRIBUTE_DIRECTORY);
    xrp_end();
}
TEST(xrp_set_header_only_mount_point) {
    BEGIN(xrp_set_header_only_mount_point);
    BYTE b[XHDR]; memset(b, 0, sizeof b); wr32(b, TAG_MP);   /* ReparseDataLength 0 */
    mkdirw(P(L"d"));
    EXPECT_EQ(set_rp_len(P(L"d"), b, sizeof b), ERROR_INVALID_REPARSE_DATA);
    xrp_end();
}
TEST(xrp_set_mount_point_fields_truncated) {
    BEGIN(xrp_set_mount_point_fields_truncated);
    /* ReparseDataLength 4: not even the four name fields */
    BYTE b[12]; memset(b, 0, sizeof b); wr32(b, TAG_MP); wr16(b + 4, 4);
    mkdirw(P(L"d"));
    EXPECT_EQ(set_rp_len(P(L"d"), b, sizeof b), ERROR_INVALID_REPARSE_DATA);
    xrp_end();
}
TEST(xrp_set_subst_offset_out_of_range) {
    BEGIN(xrp_set_subst_offset_out_of_range);
    rbuf r; mp_build(&r, P(L"t"));
    wr16(r.b + 8, (USHORT)(rd16(r.b + 4)));           /* SubstituteNameOffset past the data */
    EXPECT_EQ(set_on_new_dir(&r), ERROR_INVALID_REPARSE_DATA);
    xrp_end();
}
TEST(xrp_set_subst_length_out_of_range) {
    BEGIN(xrp_set_subst_length_out_of_range);
    rbuf r; mp_build(&r, P(L"t"));
    wr16(r.b + 10, (USHORT)(rd16(r.b + 4)));          /* SubstituteNameLength longer than the data */
    EXPECT_EQ(set_on_new_dir(&r), ERROR_INVALID_REPARSE_DATA);
    xrp_end();
}
TEST(xrp_set_print_offset_out_of_range) {
    BEGIN(xrp_set_print_offset_out_of_range);
    rbuf r; mp_build(&r, P(L"t"));
    wr16(r.b + 12, 0xFFF0);
    EXPECT_EQ(set_on_new_dir(&r), ERROR_INVALID_REPARSE_DATA);
    xrp_end();
}
TEST(xrp_set_print_length_out_of_range) {
    BEGIN(xrp_set_print_length_out_of_range);
    rbuf r; mp_build(&r, P(L"t"));
    wr16(r.b + 14, (USHORT)(rd16(r.b + 14) + 4));     /* PrintName runs 4 bytes past the end */
    EXPECT_EQ(set_on_new_dir(&r), ERROR_INVALID_REPARSE_DATA);
    xrp_end();
}
TEST(xrp_set_tag_zero_and_one_invalid) {
    BEGIN(xrp_set_tag_zero_and_one_invalid);
    rbuf r; mp_build(&r, P(L"t"));
    mkdirw(P(L"d"));
    wr32(r.b, 0); EXPECT_EQ(set_rp(P(L"d"), &r), ERROR_REPARSE_TAG_INVALID);
    wr32(r.b, 1); EXPECT_EQ(set_rp(P(L"d"), &r), ERROR_REPARSE_TAG_INVALID);
    EXPECT_EQ(GetFileAttributesW(P(L"d")), FILE_ATTRIBUTE_DIRECTORY);
    xrp_end();
}
TEST(xrp_set_nonms_tag_without_guid) {
    BEGIN(xrp_set_nonms_tag_without_guid);
    /* 8-byte header + 4 data bytes: shorter than the GUID header a non-Microsoft tag needs */
    BYTE b[12]; memset(b, 0, sizeof b); wr32(b, TAG_3P); wr16(b + 4, 4);
    mkdirw(P(L"d"));
    EXPECT_EQ(set_rp_len(P(L"d"), b, sizeof b), ERROR_INVALID_REPARSE_DATA);
    /* same tag, GUID-sized buffer whose ReparseDataLength counts the GUID as data */
    rbuf r; g3_build(&r, TAG_3P, &xrp_guid, 8); wr16(r.b + 4, 8 + 16);
    EXPECT_EQ(set_rp(P(L"d"), &r), ERROR_INVALID_REPARSE_DATA);
    EXPECT_EQ(GetFileAttributesW(P(L"d")), FILE_ATTRIBUTE_DIRECTORY);
    xrp_end();
}
TEST(xrp_set_nonms_tag_with_guid) {
    BEGIN(xrp_set_nonms_tag_with_guid);
    rbuf r; g3_build(&r, TAG_3P, &xrp_guid, 16);
    EXPECT_EQ(set_on_new_dir(&r), 0);
    EXPECT_EQ(GetFileAttributesW(P(L"d")), FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT);
    BYTE out[256]; DWORD n = 0;
    EXPECT_EQ(get_rp(P(L"d"), out, sizeof out, &n), 0);
    EXPECT_EQ(n, r.len);
    CHECK(n == r.len && !memcmp(out, r.b, r.len));
    EXPECT_EQ(del_rp_tag(P(L"d"), TAG_3P), ERROR_INVALID_REPARSE_DATA);  /* 8-byte header is not enough for a GUID tag */
    xrp_end();
}
TEST(xrp_set_over_16k_refused) {
    BEGIN(xrp_set_over_16k_refused);
    rbuf r; mp_build(&r, P(L"t"));
    /* pad the data up to 16 KiB + 8: consistent lengths, only the size is wrong */
    DWORD total = XMAXRP + 8;
    wr16(r.b + 4, (USHORT)(total - 8));
    r.len = total;
    EXPECT_EQ(set_on_new_dir(&r), ERROR_INVALID_REPARSE_DATA);
    EXPECT_EQ(GetFileAttributesW(P(L"d")), FILE_ATTRIBUTE_DIRECTORY);
    xrp_end();
}
/* mount point with a PrintName of `pl` bytes of filler and a real SubstituteName */
static DWORD mp_long_print(rbuf *r, const WCHAR *target, DWORD pl) {
    WCHAR s[XP]; snwprintf(s, XP, L"\\??\\%ls", target);
    USHORT S = (USHORT)(wcslen(s) * 2);
    memset(r, 0, sizeof *r);
    wr32(r->b, TAG_MP); wr16(r->b + 4, (USHORT)(8 + S + 2 + pl + 2));
    wr16(r->b + 8, 0); wr16(r->b + 10, S); wr16(r->b + 12, S + 2); wr16(r->b + 14, (USHORT)pl);
    memcpy(r->b + 16, s, S);
    for (DWORD i = 0; i < pl / 2; i++) wr16(r->b + 16 + S + 2 + 2 * i, (USHORT)(L'a' + i % 26));
    r->len = 16 + S + 2 + pl + 2;
    return S;
}
TEST(xrp_set_exactly_16k_accepted) {
    BEGIN(xrp_set_exactly_16k_accepted);
    NEED(mkdirw(P(L"t")));
    rbuf r;
    DWORD S = mp_long_print(&r, P(L"t"), 0);
    DWORD pl = XMAXRP - 16 - S - 4;                    /* total exactly 16 KiB */
    mp_long_print(&r, P(L"t"), pl);
    EXPECT_EQ(r.len, XMAXRP);
    EXPECT_EQ(set_on_new_dir(&r), 0);
    static BYTE out[XMAXRP + 64]; DWORD n = 0;
    EXPECT_EQ(get_rp(P(L"d"), out, sizeof out, &n), 0);
    EXPECT_EQ(n, XMAXRP);
    CHECK(!memcmp(out, r.b, XMAXRP));
    CHECK(mkfilew(P(L"t\\v.txt"), "via"));
    EXPECT_BODY(P(L"d\\v.txt"), "via");          /* a long print name does not disturb traversal */
    /* one more character crosses the limit */
    mp_long_print(&r, P(L"t"), pl + 2);
    mkdirw(P(L"d2"));
    EXPECT_EQ(set_rp(P(L"d2"), &r), ERROR_INVALID_REPARSE_DATA);
    xrp_end();
}
TEST(xrp_set_access_read_attributes_denied) {
    BEGIN(xrp_set_access_read_attributes_denied);
    NEED(mkdirw(P(L"t")) && mkdirw(P(L"d")));
    rbuf r; mp_build(&r, P(L"t"));
    HANDLE h = open_link(P(L"d"), FILE_READ_ATTRIBUTES); OK_H(h);
    DWORD n, e = 0;
    CHECK(!xio(h, XFSCTL_SET, r.b, r.len, NULL, 0, &n, &e));
    EXPECT_EQ(e, ERROR_ACCESS_DENIED);
    CLOSE(h);
    h = open_link(P(L"d"), GENERIC_READ); OK_H(h);
    CHECK(!xio(h, XFSCTL_SET, r.b, r.len, NULL, 0, &n, &e));
    EXPECT_EQ(e, ERROR_ACCESS_DENIED);
    CLOSE(h);
    EXPECT_EQ(GetFileAttributesW(P(L"d")), FILE_ATTRIBUTE_DIRECTORY);
    xrp_end();
}
TEST(xrp_set_access_write_attributes_only) {
    BEGIN(xrp_set_access_write_attributes_only);
    NEED(mkdirw(P(L"t")) && mkdirw(P(L"d")));
    rbuf r; mp_build(&r, P(L"t"));
    HANDLE h = open_link(P(L"d"), FILE_WRITE_ATTRIBUTES); OK_H(h);
    DWORD n, e = 0;
    BOOL ok = xio(h, XFSCTL_SET, r.b, r.len, NULL, 0, &n, &e);
    CHECKF(ok, "gle=%lu", e);                        /* FILE_WRITE_ATTRIBUTES is enough */
    CLOSE(h);
    EXPECT_EQ(GetFileAttributesW(P(L"d")), FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT);
    xrp_end();
}
TEST(xrp_set_access_write_data_only) {
    BEGIN(xrp_set_access_write_data_only);
    NEED(mkdirw(P(L"t")) && mkdirw(P(L"d")));
    rbuf r; mp_build(&r, P(L"t"));
    HANDLE h = open_link(P(L"d"), FILE_WRITE_DATA); OK_H(h);
    DWORD n, e = 0;
    { BOOL ok_ = xio(h, XFSCTL_SET, r.b, r.len, NULL, 0, &n, &e); CHECKF(ok_, "gle %lu", e); }
    CLOSE(h);
    CHECK(GetFileAttributesW(P(L"d")) & FILE_ATTRIBUTE_REPARSE_POINT);
    xrp_end();
}
TEST(xrp_set_replace_same_tag) {
    BEGIN(xrp_set_replace_same_tag);
    NEED(mkdirw(P(L"t1")) && mkdirw(P(L"t2")) && mkfilew(P(L"t1\\a.txt"), "one") && mkfilew(P(L"t2\\a.txt"), "two"));
    NEED(mkjunc(P(L"j"), P(L"t1")));
    EXPECT_BODY(P(L"j\\a.txt"), "one");
    rbuf r; mp_build(&r, P(L"t2"));
    EXPECT_EQ(set_rp(P(L"j"), &r), 0);
    EXPECT_BODY(P(L"j\\a.txt"), "two");
    BYTE out[2048]; DWORD n = 0;
    EXPECT_EQ(get_rp(P(L"j"), out, sizeof out, &n), 0);
    CHECK(n == r.len && !memcmp(out, r.b, r.len));
    xrp_end();
}
TEST(xrp_set_different_tag_on_junction) {
    BEGIN(xrp_set_different_tag_on_junction);
    NEED(mkdirw(P(L"t")) && mkjunc(P(L"j"), P(L"t")));
    rbuf r; g3_build(&r, TAG_3P, &xrp_guid, 8);
    EXPECT_EQ(set_rp(P(L"j"), &r), ERROR_REPARSE_TAG_MISMATCH);
    FILE_ATTRIBUTE_TAG_INFO ti; memset(&ti, 0, sizeof ti);
    HANDLE h = open_link(P(L"j"), FILE_READ_ATTRIBUTES); OK_H(h);
    CHECK(GetFileInformationByHandleEx(h, FileAttributeTagInfo, &ti, sizeof ti));
    EXPECT_EQ(ti.ReparseTag, TAG_MP);
    CLOSE(h);
    xrp_end();
}
TEST(xrp_set_symlink_tag_dir_privilege) {
    BEGIN(xrp_set_symlink_tag_dir_privilege);
    NEED(mkdirw(P(L"t")) && mkdirw(P(L"d")));
    /* symbolic-link buffer: mount-point fields + Flags (0 = absolute) */
    WCHAR s[XP]; snwprintf(s, XP, L"\\??\\%ls", P(L"t"));
    const WCHAR *pr = P(L"t");
    USHORT S = (USHORT)(wcslen(s) * 2), Pn = (USHORT)(wcslen(pr) * 2);
    rbuf r; memset(&r, 0, sizeof r);
    wr32(r.b, TAG_SL); wr16(r.b + 4, (USHORT)(12 + S + 2 + Pn + 2));
    wr16(r.b + 8, 0); wr16(r.b + 10, S); wr16(r.b + 12, S + 2); wr16(r.b + 14, Pn); wr32(r.b + 16, 0);
    memcpy(r.b + 20, s, S); memcpy(r.b + 20 + S + 2, pr, Pn);
    r.len = 8 + 12 + S + 2 + Pn + 2;
    DWORD e = set_rp(P(L"d"), &r);
    EXPECT_EQ(e, ERROR_PRIVILEGE_NOT_HELD);
    EXPECT_EQ(GetFileAttributesW(P(L"d")), FILE_ATTRIBUTE_DIRECTORY);
    /* the same buffer on a file */
    NEED(mkfilew(P(L"f.txt"), "f"));
    HANDLE h = CreateFileW(P(L"f.txt"), GENERIC_READ | GENERIC_WRITE, XSH, NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    OK_H(h);
    DWORD n; e = 0;
    CHECK(!xio(h, XFSCTL_SET, r.b, r.len, NULL, 0, &n, &e));
    EXPECT_EQ(e, ERROR_PRIVILEGE_NOT_HELD);
    CLOSE(h);
    EXPECT_BODY(P(L"f.txt"), "f");
    xrp_end();
}
TEST(xrp_set_reserved_field_nonzero) {
    BEGIN(xrp_set_reserved_field_nonzero);
    NEED(mkdirw(P(L"t")));
    rbuf r; mp_build(&r, P(L"t"));
    wr16(r.b + 6, 0x5A5A);
    EXPECT_EQ(set_on_new_dir(&r), 0);
    BYTE out[2048]; DWORD n = 0;
    EXPECT_EQ(get_rp(P(L"d"), out, sizeof out, &n), 0);
    EXPECT_EQ(n, r.len);
    EXPECT_EQ(rd16(out + 6), 0x5A5A);
    xrp_end();
}
TEST(xrp_set_odd_name_length) {
    BEGIN(xrp_set_odd_name_length);
    NEED(mkdirw(P(L"t")));
    rbuf r; mp_build(&r, P(L"t"));
    wr16(r.b + 14, (USHORT)(rd16(r.b + 14) - 1));     /* odd PrintNameLength, still inside the data */
    EXPECT_EQ(set_on_new_dir(&r), ERROR_INVALID_REPARSE_DATA);
    mp_build(&r, P(L"t"));
    wr16(r.b + 10, (USHORT)(rd16(r.b + 10) - 1));     /* odd SubstituteNameLength */
    EXPECT_EQ(set_rp(P(L"d"), &r), ERROR_INVALID_REPARSE_DATA);
    EXPECT_EQ(GetFileAttributesW(P(L"d")), FILE_ATTRIBUTE_DIRECTORY);
    xrp_end();
}
TEST(xrp_set_empty_names) {
    BEGIN(xrp_set_empty_names);
    rbuf r; mp_build_raw(&r, L"", L"", 0);             /* both names empty (two NULs of data) */
    EXPECT_EQ(set_on_new_dir(&r), 0);
    EXPECT_EQ(GetFileAttributesW(P(L"d")), FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT);
    HANDLE h = open_dir(P(L"d"), FILE_READ_ATTRIBUTES);
    DWORD e = GetLastError();
    CHECK(h == INVALID_HANDLE_VALUE);
    EXPECT_EQ(e, ERROR_INVALID_NAME);                 /* stored, but an empty substitute name cannot be opened */
    CLOSE(h);
    xrp_end();
}
/* NTFS accepts only the canonical layout: SubstituteName at offset 0, its NUL, PrintName, its NUL,
 * and ReparseDataLength covering exactly that. */
TEST(xrp_set_print_name_first_layout_refused) {
    BEGIN(xrp_set_print_name_first_layout_refused);
    NEED(mkdirw(P(L"t")));
    WCHAR s[XP]; snwprintf(s, XP, L"\\??\\%ls", P(L"t"));
    rbuf r; mp_build_raw(&r, s, P(L"t"), 1);
    EXPECT_EQ(set_on_new_dir(&r), ERROR_INVALID_REPARSE_DATA);
    EXPECT_EQ(GetFileAttributesW(P(L"d")), FILE_ATTRIBUTE_DIRECTORY);
    xrp_end();
}
TEST(xrp_set_noncanonical_layouts_refused) {
    BEGIN(xrp_set_noncanonical_layouts_refused);
    NEED(mkdirw(P(L"t")) && mkdirw(P(L"d")));
    WCHAR s[XP]; snwprintf(s, XP, L"\\??\\%ls", P(L"t"));
    const WCHAR *pr = P(L"t");
    USHORT S = (USHORT)(wcslen(s) * 2), Pn = (USHORT)(wcslen(pr) * 2);
    rbuf r; DWORD e;
    /* trailing padding after the PrintName NUL (2 and 4 bytes), lengths consistent */
    for (int pad = 2; pad <= 4; pad += 2) {
        mp_build(&r, P(L"t")); wr16(r.b + 4, (USHORT)(rd16(r.b + 4) + pad)); r.len += pad;
        e = set_rp(P(L"d"), &r);
        CHECKF(e == ERROR_INVALID_REPARSE_DATA, "pad %d: gle %lu", pad, e);
    }
    /* names without NUL terminators */
    memset(&r, 0, sizeof r); wr32(r.b, TAG_MP); wr16(r.b + 4, (USHORT)(8 + S + Pn));
    wr16(r.b + 10, S); wr16(r.b + 12, S); wr16(r.b + 14, Pn);
    memcpy(r.b + 16, s, S); memcpy(r.b + 16 + S, pr, Pn); r.len = 16 + S + Pn;
    EXPECT_EQ(set_rp(P(L"d"), &r), ERROR_INVALID_REPARSE_DATA);
    /* only the SubstituteName terminated */
    memset(&r, 0, sizeof r); wr32(r.b, TAG_MP); wr16(r.b + 4, (USHORT)(8 + S + 2 + Pn));
    wr16(r.b + 10, S); wr16(r.b + 12, S + 2); wr16(r.b + 14, Pn);
    memcpy(r.b + 16, s, S); memcpy(r.b + 16 + S + 2, pr, Pn); r.len = 16 + S + 2 + Pn;
    EXPECT_EQ(set_rp(P(L"d"), &r), ERROR_INVALID_REPARSE_DATA);
    /* a 2-byte gap between the names */
    memset(&r, 0, sizeof r); wr32(r.b, TAG_MP); wr16(r.b + 4, (USHORT)(8 + S + 4 + Pn + 2));
    wr16(r.b + 10, S); wr16(r.b + 12, S + 4); wr16(r.b + 14, Pn);
    memcpy(r.b + 16, s, S); memcpy(r.b + 16 + S + 4, pr, Pn); r.len = 16 + S + 4 + Pn + 2;
    EXPECT_EQ(set_rp(P(L"d"), &r), ERROR_INVALID_REPARSE_DATA);
    /* SubstituteName not at offset 0 */
    memset(&r, 0, sizeof r); wr32(r.b, TAG_MP); wr16(r.b + 4, (USHORT)(8 + 2 + S + 2 + Pn + 2));
    wr16(r.b + 8, 2); wr16(r.b + 10, S); wr16(r.b + 12, 2 + S + 2); wr16(r.b + 14, Pn);
    memcpy(r.b + 18, s, S); memcpy(r.b + 18 + S + 2, pr, Pn); r.len = 16 + 2 + S + 2 + Pn + 2;
    EXPECT_EQ(set_rp(P(L"d"), &r), ERROR_INVALID_REPARSE_DATA);
    EXPECT_EQ(GetFileAttributesW(P(L"d")), FILE_ATTRIBUTE_DIRECTORY);
    xrp_end();
}
TEST(xrp_set_empty_print_name_accepted) {
    BEGIN(xrp_set_empty_print_name_accepted);
    NEED(mkdirw(P(L"t")) && mkfilew(P(L"t\\a.txt"), "ep"));
    WCHAR s[XP]; snwprintf(s, XP, L"\\??\\%ls", P(L"t"));
    rbuf r; mp_build_raw(&r, s, L"", 0);
    EXPECT_EQ(set_on_new_dir(&r), 0);
    EXPECT_BODY(P(L"d\\a.txt"), "ep");
    BYTE out[2048]; DWORD n = 0;
    EXPECT_EQ(get_rp(P(L"d"), out, sizeof out, &n), 0);
    CHECK(n == r.len && !memcmp(out, r.b, r.len));
    EXPECT_EQ(rd16(out + 14), 0);
    xrp_end();
}
