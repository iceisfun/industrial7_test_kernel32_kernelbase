/* Dangling junctions, loops, chains, unhandled tags, symbolic-link error paths. */
#include "xrp.h"

#define JATTR (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)

static int dangling(void) {
    return mkdirw(P(L"t")) && mkjunc(P(L"j"), P(L"t")) && RemoveDirectoryW(P(L"t"));
}

TEST(xrp_dangling_attributes_still_work) {
    BEGIN(xrp_dangling_attributes_still_work);
    NEED(dangling());
    EXPECT_EQ(U(GetFileAttributesW)(P(L"j")), JATTR);
    char a[XP]; xrp_acp(a, sizeof a, P(L"j"));
    EXPECT_EQ(U(GetFileAttributesA)(a), JATTR);
    WIN32_FILE_ATTRIBUTE_DATA d;
    CHECK(U(GetFileAttributesExW)(P(L"j"), GetFileExInfoStandard, &d));
    EXPECT_EQ(d.dwFileAttributes, JATTR);
    WIN32_FIND_DATAW fd;
    HANDLE h = U(FindFirstFileW)(P(L"j"), &fd);
    OK_H(h);
    if (h != INVALID_HANDLE_VALUE) { EXPECT_EQ(fd.dwReserved0, TAG_MP); FindClose(h); }
    xrp_end();
}
TEST(xrp_dangling_open_fails) {
    BEGIN(xrp_dangling_open_fails);
    NEED(dangling());
    SetLastError(0);
    HANDLE h = U(CreateFileW)(P(L"j"), FILE_READ_ATTRIBUTES, XSH, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    CHECK(h == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    CLOSE(h);
    SetLastError(0);
    h = U(CreateFileW)(P(L"j\\x.txt"), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CHECK(h == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    CLOSE(h);
    SetLastError(0);
    CHECK(GetFileAttributesW(P(L"j\\x.txt")) == INVALID_FILE_ATTRIBUTES);
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    /* the link itself still opens */
    h = U(CreateFileW)(P(L"j"), FILE_READ_ATTRIBUTES, XSH, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    OK_H(h);
    CLOSE(h);
    xrp_end();
}
TEST(xrp_dangling_find_children) {
    BEGIN(xrp_dangling_find_children);
    NEED(dangling());
    WIN32_FIND_DATAW fd;
    SetLastError(0);
    HANDLE h = U(FindFirstFileW)(P(L"j\\*"), &fd);
    DWORD e = GetLastError();
    CHECK(h == INVALID_HANDLE_VALUE);
    EXPECT_EQ(e, ERROR_PATH_NOT_FOUND);
    if (h != INVALID_HANDLE_VALUE) FindClose(h);
    SetLastError(0);
    CHECK(!U(CreateDirectoryW)(P(L"j\\nd"), NULL));
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    xrp_end();
}
TEST(xrp_dangling_get_and_remove) {
    BEGIN(xrp_dangling_get_and_remove);
    NEED(dangling());
    rbuf r; mp_build(&r, P(L"t"));
    BYTE out[2048]; DWORD n = 0;
    EXPECT_EQ(get_rp(P(L"j"), out, sizeof out, &n), 0);
    CHECK(n == r.len && !memcmp(out, r.b, r.len));
    WCHAR want[XP], got[XP];
    NEED(final_of(g_xbase, 0, want, XP));
    wcscat(want, L"\\j");
    final_of(P(L"j"), 1, got, XP);
    EXPECT_WEQ(got, want);
    CHECK(U(RemoveDirectoryW)(P(L"j")));
    CHECK(!existsw(P(L"j")));
    xrp_end();
}
TEST(xrp_dangling_revives_when_target_returns) {
    BEGIN(xrp_dangling_revives_when_target_returns);
    NEED(dangling());
    CHECK(mkdirw(P(L"t")) && mkfilew(P(L"t\\back.txt"), "again"));
    EXPECT_BODY(P(L"j\\back.txt"), "again");
    /* a different directory under the same name: the link follows the name, not the id */
    CHECK(DeleteFileW(P(L"t\\back.txt")) && RemoveDirectoryW(P(L"t")));
    CHECK(mkdirw(P(L"t")) && mkfilew(P(L"t\\new.txt"), "fresh"));
    EXPECT_BODY(P(L"j\\new.txt"), "fresh");
    xrp_end();
}
TEST(xrp_dangling_move_and_copy) {
    BEGIN(xrp_dangling_move_and_copy);
    NEED(dangling() && mkfilew(P(L"m.txt"), "m"));
    SetLastError(0);
    CHECK(!U(MoveFileExW)(P(L"m.txt"), P(L"j\\m.txt"), 0));
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    SetLastError(0);
    CHECK(!U(CopyFileExW)(P(L"m.txt"), P(L"j\\m.txt"), NULL, NULL, NULL, 0));
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    /* the dangling link itself can be renamed */
    CHECK(U(MoveFileExW)(P(L"j"), P(L"j2"), 0));
    EXPECT_EQ(GetFileAttributesW(P(L"j2")), JATTR);
    xrp_end();
}
TEST(xrp_loop_self) {
    BEGIN(xrp_loop_self);
    NEED(mkdirw(P(L"a")));
    rbuf r; mp_build(&r, P(L"a"));
    NEED(set_rp(P(L"a"), &r) == 0);
    SetLastError(0);
    HANDLE h = U(CreateFileW)(P(L"a"), FILE_READ_ATTRIBUTES, XSH, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    CHECK(h == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_CANT_RESOLVE_FILENAME);
    CLOSE(h);
    EXPECT_EQ(GetFileAttributesW(P(L"a")), JATTR);
    CHECK(U(RemoveDirectoryW)(P(L"a")));
    xrp_end();
}
TEST(xrp_loop_two_links) {
    BEGIN(xrp_loop_two_links);
    NEED(mkdirw(P(L"a")) && mkjunc(P(L"b"), P(L"a")));
    rbuf r; mp_build(&r, P(L"b"));
    NEED(set_rp(P(L"a"), &r) == 0);                    /* a -> b -> a */
    SetLastError(0);
    HANDLE h = U(CreateFileW)(P(L"a\\x.txt"), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CHECK(h == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_CANT_RESOLVE_FILENAME);
    CLOSE(h);
    /* GetFileAttributes reports an unresolvable path as not found */
    SetLastError(0);
    CHECK(U(GetFileAttributesW)(P(L"b\\x.txt")) == INVALID_FILE_ATTRIBUTES);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    WIN32_FIND_DATAW fd;
    SetLastError(0);
    h = U(FindFirstFileW)(P(L"a\\*"), &fd);
    DWORD e = GetLastError();
    CHECK(h == INVALID_HANDLE_VALUE);
    EXPECT_EQ(e, ERROR_CANT_RESOLVE_FILENAME);
    if (h != INVALID_HANDLE_VALUE) FindClose(h);
    SetLastError(0);
    CHECK(!U(CreateDirectoryW)(P(L"a\\d"), NULL));
    EXPECT_ERR(ERROR_CANT_RESOLVE_FILENAME);
    xrp_end();
}
TEST(xrp_loop_links_still_manageable) {
    BEGIN(xrp_loop_links_still_manageable);
    NEED(mkdirw(P(L"a")) && mkjunc(P(L"b"), P(L"a")));
    rbuf r; mp_build(&r, P(L"b"));
    NEED(set_rp(P(L"a"), &r) == 0);
    EXPECT_EQ(GetFileAttributesW(P(L"a")), JATTR);
    EXPECT_EQ(GetFileAttributesW(P(L"b")), JATTR);
    WCHAR want[XP], got[XP];
    NEED(final_of(g_xbase, 0, want, XP));
    wcscat(want, L"\\a");
    final_of(P(L"a"), 1, got, XP);
    EXPECT_WEQ(got, want);
    /* breaking the loop by deleting one reparse point makes the other resolvable */
    EXPECT_EQ(del_rp_tag(P(L"a"), TAG_MP), 0);
    CHECK(mkfilew(P(L"a\\in.txt"), "in a"));
    EXPECT_BODY(P(L"b\\in.txt"), "in a");
    CHECK(U(RemoveDirectoryW)(P(L"b")));
    xrp_end();
}

/* chain c0 -> c1 -> ... -> c<n-1> -> t; returns 1 on success */
static int mkchain(int n) {
    WCHAR l[64], nx[64];
    if (!mkdirw(P(L"t")) || !mkfilew(P(L"t\\end.txt"), "end")) return 0;
    for (int i = n - 1; i >= 0; i--) {
        snwprintf(l, 64, L"c%d", i);
        if (i == n - 1) wcscpy(nx, L"t"); else snwprintf(nx, 64, L"c%d", i + 1);
        if (!mkjunc(P(l), P(nx))) return 0;
    }
    return 1;
}
TEST(xrp_chain_short_resolves) {
    BEGIN(xrp_chain_short_resolves);
    NEED(mkchain(8));
    EXPECT_BODY(P(L"c0\\end.txt"), "end");
    WCHAR want[XP], got[XP];
    NEED(final_of(P(L"t"), 0, want, XP));
    final_of(P(L"c0"), 0, got, XP);
    EXPECT_WEQ(got, want);
    EXPECT_EQ(file_index(P(L"c0"), 0), file_index(P(L"t"), 0));
    xrp_end();
}
/* number of hops of the longest chain that still resolves (searching 1..80): 31 on Windows 10 */
TEST(xrp_chain_limit) {
    BEGIN(xrp_chain_limit);
    NEED(mkchain(80));
    int ok_max = 0; DWORD efirst = 0;
    WCHAR l[64];
    for (int start = 79; start >= 0; start--) {         /* c<start> is 80-start hops from t */
        snwprintf(l, 64, L"c%d\\end.txt", start);
        HANDLE h = CreateFileW(P(l), GENERIC_READ, XSH, NULL, OPEN_EXISTING, 0, NULL);
        if (h == INVALID_HANDLE_VALUE) { efirst = GetLastError(); break; }
        CloseHandle(h);
        ok_max = 80 - start;
    }
    t_note("longest resolvable junction chain: %d hops, next fails with %lu", ok_max, efirst);
    EXPECT_EQ(ok_max, 31);
    EXPECT_EQ(efirst, ERROR_CANT_RESOLVE_FILENAME);
    /* attribute queries on the links themselves are unaffected */
    EXPECT_EQ(GetFileAttributesW(P(L"c0")), JATTR);
    xrp_end();
}
TEST(xrp_chain_deep_fails_everywhere) {
    BEGIN(xrp_chain_deep_fails_everywhere);
    NEED(mkchain(70));
    SetLastError(0);
    HANDLE h = U(CreateFileW)(P(L"c0"), FILE_READ_ATTRIBUTES, XSH, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    CHECK(h == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_CANT_RESOLVE_FILENAME);
    CLOSE(h);
    SetLastError(0);
    CHECK(U(GetFileAttributesW)(P(L"c0\\end.txt")) == INVALID_FILE_ATTRIBUTES);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    SetLastError(0);
    CHECK(!U(CopyFileExW)(P(L"c0\\end.txt"), P(L"copy.txt"), NULL, NULL, NULL, 0));
    EXPECT_ERR(ERROR_CANT_RESOLVE_FILENAME);
    /* the far end of the chain still works */
    EXPECT_BODY(P(L"c69\\end.txt"), "end");
    EXPECT_BODY(P(L"c39\\end.txt"), "end");        /* exactly at the limit */
    SetLastError(0);
    CHECK(GetFileAttributesW(P(L"c38\\end.txt")) == INVALID_FILE_ATTRIBUTES);
    xrp_end();
}
TEST(xrp_unhandled_tag_open) {
    BEGIN(xrp_unhandled_tag_open);
    NEED(mkdirw(P(L"d")));
    rbuf r; g3_build(&r, TAG_3P, &xrp_guid, 8);
    NEED(set_rp(P(L"d"), &r) == 0);
    SetLastError(0);
    HANDLE h = U(CreateFileW)(P(L"d"), FILE_READ_ATTRIBUTES, XSH, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    CHECK(h == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_CANT_ACCESS_FILE);
    CLOSE(h);
    h = U(CreateFileW)(P(L"d"), FILE_READ_ATTRIBUTES, XSH, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    OK_H(h);
    FILE_ATTRIBUTE_TAG_INFO ti;
    if (h != INVALID_HANDLE_VALUE) { CHECK(GetFileInformationByHandleEx(h, FileAttributeTagInfo, &ti, sizeof ti)); EXPECT_EQ(ti.ReparseTag, TAG_3P); }
    CLOSE(h);
    WIN32_FIND_DATAW fd;
    h = U(FindFirstFileW)(P(L"d"), &fd);
    OK_H(h);
    if (h != INVALID_HANDLE_VALUE) { EXPECT_EQ(fd.dwReserved0, TAG_3P); FindClose(h); }
    CHECK(U(RemoveDirectoryW)(P(L"d")));
    xrp_end();
}
TEST(xrp_unhandled_tag_on_file) {
    BEGIN(xrp_unhandled_tag_on_file);
    NEED(mkfilew(P(L"f.txt"), "payload"));
    rbuf r; g3_build(&r, TAG_3P, &xrp_guid, 8);
    HANDLE h = CreateFileW(P(L"f.txt"), GENERIC_READ | GENERIC_WRITE, XSH, NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    OK_H(h);
    DWORD n, e = 0;
    { BOOL ok_ = xio(h, XFSCTL_SET, r.b, r.len, NULL, 0, &n, &e); CHECKF(ok_, "gle %lu", e); }
    CLOSE(h);
    EXPECT_EQ(GetFileAttributesW(P(L"f.txt")), FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_REPARSE_POINT);
    SetLastError(0);
    h = U(CreateFileW)(P(L"f.txt"), GENERIC_READ, XSH, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_CANT_ACCESS_FILE);
    CLOSE(h);
    /* DeleteFile opens the reparse point itself */
    CHECK(U(DeleteFileW)(P(L"f.txt")));
    CHECK(!existsw(P(L"f.txt")));
    xrp_end();
}
