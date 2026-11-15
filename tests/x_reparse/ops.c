/* Namespace operations on and through a junction: remove, delete, move, copy, create, cwd. */
#include "xrp.h"

#define JATTR (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)

TEST(xrp_rmdir_removes_only_link) {
    BEGIN(xrp_rmdir_removes_only_link);
    NEED(std_layout());
    CHECK(U(RemoveDirectoryW)(P(L"j")));
    CHECK(!existsw(P(L"j")));
    EXPECT_BODY(P(L"t\\f.txt"), "target body");
    CHECK(existsw(P(L"t\\sub")));
    xrp_end();
}
TEST(xrp_rmdir_trailing_backslash) {
    BEGIN(xrp_rmdir_trailing_backslash);
    NEED(std_layout());
    WCHAR p[XP]; snwprintf(p, XP, L"%ls\\", P(L"j"));
    CHECK(U(RemoveDirectoryW)(p));
    CHECK(!existsw(P(L"j")));
    EXPECT_BODY(P(L"t\\f.txt"), "target body");
    xrp_end();
}
TEST(xrp_rmdir_through_removes_target_entry) {
    BEGIN(xrp_rmdir_through_removes_target_entry);
    NEED(std_layout());
    CHECK(U(RemoveDirectoryW)(P(L"j\\sub")));
    CHECK(!existsw(P(L"t\\sub")));
    SetLastError(0);
    CHECK(!U(RemoveDirectoryW)(P(L"j\\sub")));
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    CHECK(GetFileAttributesW(P(L"j")) == JATTR);
    xrp_end();
}
TEST(xrp_rmdir_target_leaves_link) {
    BEGIN(xrp_rmdir_target_leaves_link);
    NEED(mkdirw(P(L"t")) && mkjunc(P(L"j"), P(L"t")));
    /* the target can be removed while the junction names it */
    CHECK(U(RemoveDirectoryW)(P(L"t")));
    EXPECT_EQ(GetFileAttributesW(P(L"j")), JATTR);
    xrp_end();
}
TEST(xrp_rmdir_nonempty_plain_vs_link) {
    BEGIN(xrp_rmdir_nonempty_plain_vs_link);
    NEED(std_layout());
    SetLastError(0);
    CHECK(!U(RemoveDirectoryW)(P(L"t")));
    EXPECT_ERR(ERROR_DIR_NOT_EMPTY);
    CHECK(U(RemoveDirectoryW)(P(L"j")));      /* content behind the link does not matter */
    xrp_end();
}
TEST(xrp_rmdir_readonly_link) {
    BEGIN(xrp_rmdir_readonly_link);
    NEED(std_layout());
    CHECK(SetFileAttributesW(P(L"j"), FILE_ATTRIBUTE_READONLY));
    EXPECT_EQ(GetFileAttributesW(P(L"j")), JATTR | FILE_ATTRIBUTE_READONLY);
    SetLastError(0);
    BOOL ok = U(RemoveDirectoryW)(P(L"j"));
    DWORD e = GetLastError();
    CHECK(!ok);
    EXPECT_EQ(e, ERROR_ACCESS_DENIED);
    CHECK(SetFileAttributesW(P(L"j"), FILE_ATTRIBUTE_NORMAL));
    CHECK(U(RemoveDirectoryW)(P(L"j")));
    EXPECT_BODY(P(L"t\\f.txt"), "target body");
    xrp_end();
}
TEST(xrp_deletefile_on_junction_fails) {
    BEGIN(xrp_deletefile_on_junction_fails);
    NEED(std_layout());
    SetLastError(0);
    CHECK(!U(DeleteFileW)(P(L"j")));
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    EXPECT_EQ(GetFileAttributesW(P(L"j")), JATTR);
    EXPECT_BODY(P(L"j\\f.txt"), "target body");
    xrp_end();
}
TEST(xrp_deletefile_through_removes_target_file) {
    BEGIN(xrp_deletefile_through_removes_target_file);
    NEED(std_layout());
    CHECK(U(DeleteFileW)(P(L"j\\f.txt")));
    CHECK(!existsw(P(L"t\\f.txt")));
    SetLastError(0);
    CHECK(!U(DeleteFileW)(P(L"j\\f.txt")));
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    xrp_end();
}
TEST(xrp_move_junction_moves_link) {
    BEGIN(xrp_move_junction_moves_link);
    NEED(std_layout() && mkdirw(P(L"other")));
    ULONGLONG id = file_index(P(L"j"), 1);
    CHECK(U(MoveFileExW)(P(L"j"), P(L"other\\k"), 0));
    CHECK(!existsw(P(L"j")));
    EXPECT_EQ(GetFileAttributesW(P(L"other\\k")), JATTR);
    EXPECT_EQ(file_index(P(L"other\\k"), 1), id);
    EXPECT_BODY(P(L"other\\k\\f.txt"), "target body");
    EXPECT_BODY(P(L"t\\f.txt"), "target body");
    /* the stored target is unchanged by the rename */
    BYTE out[2048]; DWORD n = 0; rbuf r; mp_build(&r, P(L"t"));
    EXPECT_EQ(get_rp(P(L"other\\k"), out, sizeof out, &n), 0);
    CHECK(n == r.len && !memcmp(out, r.b, r.len));
    xrp_end();
}
TEST(xrp_move_copy_allowed_still_renames) {
    BEGIN(xrp_move_copy_allowed_still_renames);
    NEED(std_layout());
    CHECK(U(MoveFileExW)(P(L"j"), P(L"j2"), MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING));
    EXPECT_EQ(GetFileAttributesW(P(L"j2")), JATTR);
    CHECK(!existsw(P(L"j")));
    EXPECT_BODY(P(L"t\\f.txt"), "target body");
    xrp_end();
}
TEST(xrp_move_file_into_junction) {
    BEGIN(xrp_move_file_into_junction);
    NEED(std_layout() && mkfilew(P(L"m.txt"), "moved"));
    CHECK(U(MoveFileExW)(P(L"m.txt"), P(L"j\\m.txt"), 0));
    EXPECT_BODY(P(L"t\\m.txt"), "moved");
    CHECK(!existsw(P(L"m.txt")));
    /* and back out, renaming inside the target through the link path */
    CHECK(U(MoveFileExW)(P(L"j\\m.txt"), P(L"j\\m2.txt"), 0));
    EXPECT_BODY(P(L"t\\m2.txt"), "moved");
    CHECK(U(MoveFileExW)(P(L"j\\m2.txt"), P(L"back.txt"), 0));
    EXPECT_BODY(P(L"back.txt"), "moved");
    xrp_end();
}
TEST(xrp_move_target_away_and_back) {
    BEGIN(xrp_move_target_away_and_back);
    NEED(std_layout());
    CHECK(U(MoveFileExW)(P(L"t"), P(L"t_away"), 0));
    SetLastError(0);
    CHECK(GetFileAttributesW(P(L"j\\f.txt")) == INVALID_FILE_ATTRIBUTES);
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    CHECK(U(MoveFileExW)(P(L"t_away"), P(L"t"), 0));
    EXPECT_BODY(P(L"j\\f.txt"), "target body");
    xrp_end();
}
TEST(xrp_move_replace_onto_junction) {
    BEGIN(xrp_move_replace_onto_junction);
    NEED(std_layout() && mkfilew(P(L"r.txt"), "replacer"));
    SetLastError(0);
    BOOL ok = U(MoveFileExW)(P(L"r.txt"), P(L"j"), MOVEFILE_REPLACE_EXISTING);
    DWORD e = GetLastError();
    CHECK(!ok);
    EXPECT_EQ(e, ERROR_ACCESS_DENIED);
    EXPECT_EQ(GetFileAttributesW(P(L"j")), JATTR);
    EXPECT_BODY(P(L"r.txt"), "replacer");
    /* without REPLACE_EXISTING the name is simply taken */
    SetLastError(0);
    CHECK(!U(MoveFileExW)(P(L"r.txt"), P(L"j"), 0));
    EXPECT_ERR(ERROR_ALREADY_EXISTS);
    xrp_end();
}
static DWORD CALLBACK prog_cb(LARGE_INTEGER total, LARGE_INTEGER done, LARGE_INTEGER st, LARGE_INTEGER sd, DWORD sn, DWORD reason, HANDLE s, HANDLE d, LPVOID ctx) {
    (void)done; (void)st; (void)sd; (void)sn; (void)reason; (void)s; (void)d;
    *(LONGLONG *)ctx = total.QuadPart;
    return PROGRESS_CONTINUE;
}
TEST(xrp_copy_file_through_junction) {
    BEGIN(xrp_copy_file_through_junction);
    NEED(std_layout());
    LONGLONG total = -1;
    CHECK(U(CopyFileExW)(P(L"j\\f.txt"), P(L"c.txt"), prog_cb, &total, NULL, 0));
    EXPECT_BODY(P(L"c.txt"), "target body");
    EXPECT_EQ(total, 11);
    EXPECT_EQ(GetFileAttributesW(P(L"c.txt")), FILE_ATTRIBUTE_ARCHIVE);
    /* into the junction */
    CHECK(U(CopyFileExW)(P(L"c.txt"), P(L"j\\c2.txt"), NULL, NULL, NULL, 0));
    EXPECT_BODY(P(L"t\\c2.txt"), "target body");
    /* COPY_FILE_COPY_SYMLINK on a plain file reached through the junction: an ordinary copy */
    CHECK(U(CopyFileExW)(P(L"j\\f.txt"), P(L"c3.txt"), NULL, NULL, NULL, COPY_FILE_COPY_SYMLINK));
    EXPECT_EQ(GetFileAttributesW(P(L"c3.txt")), FILE_ATTRIBUTE_ARCHIVE);
    EXPECT_BODY(P(L"c3.txt"), "target body");
    xrp_end();
}
TEST(xrp_copy_junction_itself) {
    BEGIN(xrp_copy_junction_itself);
    NEED(std_layout());
    SetLastError(0);
    BOOL ok = U(CopyFileExW)(P(L"j"), P(L"cj"), NULL, NULL, NULL, 0);
    DWORD e = GetLastError();
    CHECK(!ok);
    EXPECT_EQ(e, ERROR_ACCESS_DENIED);
    CHECK(!existsw(P(L"cj")));
    xrp_end();
}
TEST(xrp_copy_symlink_flag_on_junction) {
    BEGIN(xrp_copy_symlink_flag_on_junction);
    NEED(std_layout());
    SetLastError(0);
    BOOL ok = U(CopyFileExW)(P(L"j"), P(L"cj"), NULL, NULL, NULL, COPY_FILE_COPY_SYMLINK);
    DWORD e = GetLastError();
    DWORD a = GetFileAttributesW(P(L"cj"));
    CHECKF(!ok && e == ERROR_ACCESS_DENIED, "ok=%d gle=%lu attr=0x%lx", ok, e, a);
    EXPECT_BODY(P(L"t\\f.txt"), "target body");
    xrp_end();
}
TEST(xrp_copy_fail_if_exists_through) {
    BEGIN(xrp_copy_fail_if_exists_through);
    NEED(std_layout() && mkfilew(P(L"src.txt"), "src"));
    SetLastError(0);
    CHECK(!U(CopyFileExW)(P(L"src.txt"), P(L"j\\f.txt"), NULL, NULL, NULL, COPY_FILE_FAIL_IF_EXISTS));
    EXPECT_ERR(ERROR_FILE_EXISTS);
    EXPECT_BODY(P(L"t\\f.txt"), "target body");
    CHECK(U(CopyFileExW)(P(L"src.txt"), P(L"j\\f.txt"), NULL, NULL, NULL, 0));
    EXPECT_BODY(P(L"t\\f.txt"), "src");
    xrp_end();
}
TEST(xrp_createdir_through_junction) {
    BEGIN(xrp_createdir_through_junction);
    NEED(std_layout());
    CHECK(U(CreateDirectoryW)(P(L"j\\nd"), NULL));
    EXPECT_EQ(GetFileAttributesW(P(L"t\\nd")), FILE_ATTRIBUTE_DIRECTORY);
    CHECK(U(CreateDirectoryW)(P(L"j\\nd\\deeper"), NULL));
    CHECK(existsw(P(L"t\\nd\\deeper")));
    SetLastError(0);
    CHECK(!U(CreateDirectoryW)(P(L"j"), NULL));
    EXPECT_ERR(ERROR_ALREADY_EXISTS);
    SetLastError(0);
    CHECK(!U(CreateDirectoryW)(P(L"j\\sub"), NULL));
    EXPECT_ERR(ERROR_ALREADY_EXISTS);
    xrp_end();
}
TEST(xrp_createdirex_template_junction) {
    BEGIN(xrp_createdirex_template_junction);
    NEED(std_layout());
    /* a junction as template: the new directory becomes a junction to the same target */
    BOOL ok = U(CreateDirectoryExW)(P(L"j"), P(L"n"), NULL);
    DWORD e = ok ? 0 : GetLastError();
    CHECKF(ok, "gle %lu", e);
    EXPECT_EQ(GetFileAttributesW(P(L"n")), JATTR);
    BYTE a[2048], b[2048]; DWORD na = 0, nb = 0;
    EXPECT_EQ(get_rp(P(L"j"), a, sizeof a, &na), 0);
    EXPECT_EQ(get_rp(P(L"n"), b, sizeof b, &nb), 0);
    CHECK(na == nb && !memcmp(a, b, na));
    EXPECT_BODY(P(L"n\\f.txt"), "target body");
    xrp_end();
}
TEST(xrp_createdirex_template_through_junction) {
    BEGIN(xrp_createdirex_template_through_junction);
    NEED(std_layout());
    CHECK(SetFileAttributesW(P(L"t\\sub"), FILE_ATTRIBUTE_HIDDEN));
    /* template below the junction is the plain target subdirectory; new dir also through it */
    CHECK(U(CreateDirectoryExW)(P(L"j\\sub"), P(L"j\\sub2"), NULL));
    EXPECT_EQ(GetFileAttributesW(P(L"t\\sub2")), FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_HIDDEN);
    xrp_end();
}
TEST(xrp_rename_and_delete_inside_through) {
    BEGIN(xrp_rename_and_delete_inside_through);
    NEED(std_layout());
    CHECK(U(MoveFileExW)(P(L"j\\sub"), P(L"j\\sub_renamed"), 0));
    CHECK(existsw(P(L"t\\sub_renamed")) && !existsw(P(L"t\\sub")));
    CHECK(mkfilew(P(L"j\\sub_renamed\\x.txt"), "x"));
    EXPECT_BODY(P(L"t\\sub_renamed\\x.txt"), "x");
    CHECK(U(DeleteFileW)(P(L"j\\sub_renamed\\x.txt")));
    CHECK(U(RemoveDirectoryW)(P(L"j\\sub_renamed")));
    CHECK(!existsw(P(L"t\\sub_renamed")));
    xrp_end();
}

/* Child: argv[1] = junction path, argv[2] = target path (ACP). */
CHILD(xrp_cwd) {
    if (argc < 3) return 90;
    WCHAR j[XP], t[XP], cur[XP], full[XP], want[XP];
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, j, XP);
    MultiByteToWideChar(CP_ACP, 0, argv[2], -1, t, XP);
    if (!U(SetCurrentDirectoryW)(j)) return 1;
    DWORD n = GetCurrentDirectoryW(XP, cur);
    if (!n || wcscmp(cur, j)) return 2;                       /* the junction path is kept */
    HANDLE h = CreateFileW(L"cwd.txt", GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 3;
    CloseHandle(h);
    snwprintf(want, XP, L"%ls\\cwd.txt", t);
    if (GetFileAttributesW(want) == INVALID_FILE_ATTRIBUTES) return 4;   /* landed in the target */
    if (!U(GetFullPathNameW)(L"cwd.txt", XP, full, NULL)) return 5;
    snwprintf(want, XP, L"%ls\\cwd.txt", j);
    if (wcscmp(full, want)) return 6;
    /* ".." from the junction goes to the link's parent */
    if (!U(SetCurrentDirectoryW)(L"..")) return 7;
    GetCurrentDirectoryW(XP, cur);
    wcscpy(want, j); *wcsrchr(want, L'\\') = 0;
    if (wcscmp(cur, want)) return 8;
    /* a subdirectory of the target through the junction */
    snwprintf(want, XP, L"%ls\\sub", j);
    if (!U(SetCurrentDirectoryW)(want)) return 9;
    GetCurrentDirectoryW(XP, cur);
    if (wcscmp(cur, want)) return 10;
    if (!DeleteFileW(L"..\\cwd.txt")) return 11;               /* ".." is textual: j\cwd.txt */
    SetCurrentDirectoryW(L"\\");
    return 0;
}
TEST(xrp_cwd_child_keeps_junction_path) {
    BEGIN(xrp_cwd_child_keeps_junction_path);
    NEED(std_layout());
    char args[2 * XP + 8], j[XP], t[XP];
    xrp_acp(j, sizeof j, P(L"j")); xrp_acp(t, sizeof t, P(L"t"));
    snprintf(args, sizeof args, "\"%s\" \"%s\"", j, t);
    DWORD rc = t_run_child("xrp_cwd", args, 20000);
    EXPECT_EQ(rc, 0);
    CHECK(!existsw(P(L"t\\cwd.txt")));
    xrp_end();
}
CHILD(xrp_cwd_dangling) {
    if (argc < 2) return 90;
    WCHAR j[XP];
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, j, XP);
    SetLastError(0);
    if (U(SetCurrentDirectoryW)(j)) return 1;
    return (int)GetLastError();
}
TEST(xrp_cwd_child_dangling_refused) {
    BEGIN(xrp_cwd_child_dangling_refused);
    NEED(mkdirw(P(L"t")) && mkjunc(P(L"j"), P(L"t")) && RemoveDirectoryW(P(L"t")));
    char args[XP + 8], j[XP];
    xrp_acp(j, sizeof j, P(L"j"));
    snprintf(args, sizeof args, "\"%s\"", j);
    EXPECT_EQ(t_run_child("xrp_cwd_dangling", args, 20000), ERROR_FILE_NOT_FOUND);
    xrp_end();
}
