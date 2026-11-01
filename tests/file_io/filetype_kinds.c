/* GetFileType per handle kind: disk file, directory, anonymous and named pipes, NUL, mailslot, console
 * handles (own child console), invalid and pseudo handles, and inherited standard handles in a child.
 * Evidence: values observed on Windows 10 22H2 (build 19045), standard user and elevated, in a process
 * with a console and redirected. GetFileType is what the C runtime uses to classify std handles
 * (FILE_TYPE_CHAR -> a device, FILE_TYPE_PIPE -> a pipe); UNKNOWN makes it treat the handle as unusable. */
#include "fio.h"

#define FT_CHILD_TIMEOUT 20000

/* ---- in-process kinds ----------------------------------------------------------- */
TEST(fio_gft_kind_disk_file) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(GetFileType));
    TP(p, "gft_file.bin"); rmfile(p);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, 0, CREATE_NEW, 0); OK_H(h);
    SET_ERR(0x4242);
    EXPECT_EQ(U(GetFileType)(h), FILE_TYPE_DISK);
    EXPECT_EQ(GetLastError(), 0x4242);                          /* untouched on success */
    DWORD m = 0; SET_ERR(0); CHECK(!GetConsoleMode(h, &m)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CLOSE(h);
    h = cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, FILE_FLAG_OVERLAPPED); OK_H(h);
    EXPECT_EQ(U(GetFileType)(h), FILE_TYPE_DISK);
    CLOSE(h); rmfile(p);
}
TEST(fio_gft_kind_directory) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(GetFileType));
    TP(d, "gft_dir"); RemoveDirectoryA(d); CreateDirectoryA(d, NULL);
    HANDLE h = cfa(d, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS); OK_H(h);
    EXPECT_EQ(U(GetFileType)(h), FILE_TYPE_DISK);
    CLOSE(h); RemoveDirectoryA(d);
}
TEST(fio_gft_kind_anonymous_pipe) {
    REQUIRE(U(GetFileType)); REQUIRE(U(CreatePipe));
    HANDLE r, w; SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    CHECK(U(CreatePipe)(&r, &w, &sa, 0));
    EXPECT_EQ(U(GetFileType)(r), FILE_TYPE_PIPE); EXPECT_EQ(U(GetFileType)(w), FILE_TYPE_PIPE);
    DWORD m = 0; SET_ERR(0); CHECK(!GetConsoleMode(r, &m)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(r); CloseHandle(w);
    CHECK(U(CreatePipe)(&r, &w, NULL, 65536));
    EXPECT_EQ(U(GetFileType)(r), FILE_TYPE_PIPE); EXPECT_EQ(U(GetFileType)(w), FILE_TYPE_PIPE);
    CloseHandle(r); CloseHandle(w);
}
TEST(fio_gft_kind_named_pipe) {
    REQUIRE(U(GetFileType)); REQUIRE(U(CreateNamedPipeA));
    char n[96]; snprintf(n, sizeof n, "\\\\.\\pipe\\fio_gft_%lu", GetCurrentProcessId());
    HANDLE s = U(CreateNamedPipeA)(n, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, NULL);
    CHECK(s != INVALID_HANDLE_VALUE);
    if (s == INVALID_HANDLE_VALUE) return;
    EXPECT_EQ(U(GetFileType)(s), FILE_TYPE_PIPE);               /* not yet connected */
    HANDLE c = cfa(n, GENERIC_READ | GENERIC_WRITE, 0, OPEN_EXISTING, 0); OK_H(c);
    if (c != INVALID_HANDLE_VALUE) { EXPECT_EQ(U(GetFileType)(c), FILE_TYPE_PIPE); EXPECT_EQ(U(GetFileType)(s), FILE_TYPE_PIPE); CloseHandle(c); }
    CloseHandle(s);
}
static void gft_nul(const char *name, DWORD access, DWORD share, DWORD flags) {
    HANDLE h = cfa(name, access, share, OPEN_EXISTING, flags); OK_H(h);
    if (h == INVALID_HANDLE_VALUE) return;
    EXPECT_EQ(U(GetFileType)(h), FILE_TYPE_CHAR);
    DWORD m = 0; SET_ERR(0); CHECK(!GetConsoleMode(h, &m)); EXPECT_ERR(ERROR_INVALID_HANDLE);   /* a char device, but not a console */
    CLOSE(h);
}
TEST(fio_gft_kind_nul_read_write) { REQUIRE(U(GetFileType)); gft_nul("NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0); }
TEST(fio_gft_kind_nul_read_only) { REQUIRE(U(GetFileType)); gft_nul("NUL", GENERIC_READ, 0, 0); }
TEST(fio_gft_kind_nul_write_only) { REQUIRE(U(GetFileType)); gft_nul("NUL", GENERIC_WRITE, 0, 0); }
TEST(fio_gft_kind_nul_lowercase) { REQUIRE(U(GetFileType)); gft_nul("nul", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0); }
TEST(fio_gft_kind_nul_device_path) { REQUIRE(U(GetFileType)); gft_nul("\\\\.\\NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0); }
TEST(fio_gft_kind_nul_overlapped) { REQUIRE(U(GetFileType)); gft_nul("NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, FILE_FLAG_OVERLAPPED); }
TEST(fio_gft_kind_nul_create_always) {
    REQUIRE(U(GetFileType)); REQUIRE(U(CreateFileA));
    HANDLE h = cfa("NUL", GENERIC_WRITE, FILE_SHARE_WRITE, CREATE_ALWAYS, 0); OK_H(h);
    if (h != INVALID_HANDLE_VALUE) { EXPECT_EQ(U(GetFileType)(h), FILE_TYPE_CHAR); CLOSE(h); }
}
TEST(fio_gft_nul_io_semantics) {
    REQUIRE(U(CreateFileA));
    HANDLE h = cfa("NUL", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, 0); OK_H(h);
    if (h == INVALID_HANDLE_VALUE) return;
    char b[8]; DWORD n = 0x55;
    CHECK(ReadFile(h, b, sizeof b, &n, NULL)); EXPECT_EQ(n, 0);        /* EOF at once, success */
    n = 0x55; CHECK(WriteFile(h, "abcdefgh", 8, &n, NULL)); EXPECT_EQ(n, 8);
    CHECK(SetFilePointerEx(h, (LARGE_INTEGER){ .QuadPart = 0 }, NULL, FILE_BEGIN));
    CLOSE(h);
}
TEST(fio_gft_kind_mailslot_is_unknown_without_error) {
    REQUIRE(U(GetFileType)); REQUIRE(U(CreateMailslotA));
    char n[96]; snprintf(n, sizeof n, "\\\\.\\mailslot\\fio_gft_%lu", GetCurrentProcessId());
    HANDLE s = U(CreateMailslotA)(n, 0, 0, NULL); CHECK(s != INVALID_HANDLE_VALUE);
    if (s == INVALID_HANDLE_VALUE) return;
    SET_ERR(0x4242);
    EXPECT_EQ(U(GetFileType)(s), FILE_TYPE_UNKNOWN);
    EXPECT_ERR(NO_ERROR);                                        /* UNKNOWN with no error: the type is simply not classified */
    HANDLE c = cfa(n, GENERIC_WRITE, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(c);
    if (c != INVALID_HANDLE_VALUE) { SET_ERR(0x4242); EXPECT_EQ(U(GetFileType)(c), FILE_TYPE_UNKNOWN); EXPECT_ERR(NO_ERROR); CloseHandle(c); }
    CloseHandle(s);
}
TEST(fio_gft_invalid_and_pseudo_handles) {
    REQUIRE(U(GetFileType)); REQUIRE(U(CreateFileA));
    TP(p, "gft_inv.bin"); rmfile(p);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, 0, CREATE_NEW, 0); OK_H(h);
    CLOSE(h);
    EXPECT_FAIL_DW(U(GetFileType)(h), FILE_TYPE_UNKNOWN, ERROR_INVALID_HANDLE);                                   /* closed */
    EXPECT_FAIL_DW(U(GetFileType)(NULL), FILE_TYPE_UNKNOWN, ERROR_INVALID_HANDLE);
    EXPECT_FAIL_DW(U(GetFileType)((HANDLE)(ULONG_PTR)0x1234), FILE_TYPE_UNKNOWN, ERROR_INVALID_HANDLE);          /* never opened */
    EXPECT_FAIL_DW(U(GetFileType)((HANDLE)(LONG_PTR)-1), FILE_TYPE_UNKNOWN, ERROR_INVALID_HANDLE);                /* current process pseudo handle */
    EXPECT_FAIL_DW(U(GetFileType)((HANDLE)(LONG_PTR)-2), FILE_TYPE_UNKNOWN, ERROR_INVALID_HANDLE);                /* current thread pseudo handle */
    rmfile(p);
}
TEST(fio_gft_thread_pseudo_handle_write_fails) {
    DWORD n = 0x77; SET_ERR(0); CHECK(!WriteFile((HANDLE)(LONG_PTR)-2, "abc", 3, &n, NULL)); EXPECT_ERR(ERROR_INVALID_HANDLE);
}

/* ---- console handles (own console in a child) ------------------------------------ */
/* exit code: bit0 CONOUT$ CHAR, bit1 CONIN$ CHAR, bit2 GetConsoleMode(CONOUT$) ok, bit3 GetConsoleMode(CONIN$) ok,
 * bits4-5 std in type, bits6-7 std out type, bits8-9 std err type, bit10 GetConsoleMode(std out) ok,
 * bit11 WriteFile(std out) ok with the full count */
CHILD(fio_gft_console) {
    HANDLE co = CreateFileA("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    HANDLE ci = CreateFileA("CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    HANDLE si = GetStdHandle(STD_INPUT_HANDLE), so = GetStdHandle(STD_OUTPUT_HANDLE), se = GetStdHandle(STD_ERROR_HANDLE);
    DWORD m, n = 0, rc = 0;
    if (co != INVALID_HANDLE_VALUE && GetFileType(co) == FILE_TYPE_CHAR) rc |= 1;
    if (ci != INVALID_HANDLE_VALUE && GetFileType(ci) == FILE_TYPE_CHAR) rc |= 2;
    if (co != INVALID_HANDLE_VALUE && GetConsoleMode(co, &m)) rc |= 4;
    if (ci != INVALID_HANDLE_VALUE && GetConsoleMode(ci, &m)) rc |= 8;
    rc |= (GetFileType(si) & 3) << 4; rc |= (GetFileType(so) & 3) << 6; rc |= (GetFileType(se) & 3) << 8;
    if (GetConsoleMode(so, &m)) rc |= 1 << 10;
    if (WriteFile(so, "x", 1, &n, NULL) && n == 1) rc |= 1 << 11;
    return rc;
}
TEST(fio_gft_kind_console_in_own_console) {
    REQUIRE(U(GetFileType));
    PROCESS_INFORMATION pi;
    HANDLE hp = t_spawn_child("fio_gft_console", NULL, CREATE_NEW_CONSOLE, NULL, NULL, NULL, FALSE, NULL, &pi);
    CHECK(hp != NULL);
    if (!hp) return;
    DWORD code = 0xDEAD;
    if (WaitForSingleObject(hp, FT_CHILD_TIMEOUT) == WAIT_OBJECT_0) GetExitCodeProcess(hp, &code); else TerminateProcess(hp, 1);
    CloseHandle(pi.hThread); CloseHandle(hp);
    EXPECT_EQ(code & 0xF, 0xF);                                  /* CONOUT$ and CONIN$ are CHAR and accept GetConsoleMode */
    EXPECT_EQ((code >> 4) & 3, FILE_TYPE_CHAR);                  /* std handles of a new console */
    EXPECT_EQ((code >> 6) & 3, FILE_TYPE_CHAR);
    EXPECT_EQ((code >> 8) & 3, FILE_TYPE_CHAR);
    EXPECT_EQ((code >> 10) & 1, 1);
    EXPECT_EQ((code >> 11) & 1, 1);
}

/* ---- inherited standard handles in a child ----------------------------------------- */
/* exit code: bits0-1 in type, bits2-3 out type, bits4-5 err type, bit6 GetConsoleMode(out) ok,
 * bit7 WriteFile(out) ok with the full count, bit8 ReadFile(in) ok with 0 bytes (EOF) */
CHILD(fio_gft_std) {
    HANDLE si = GetStdHandle(STD_INPUT_HANDLE), so = GetStdHandle(STD_OUTPUT_HANDLE), se = GetStdHandle(STD_ERROR_HANDLE);
    DWORD m, n = 0xCC, rc = 0; char b[4];
    rc |= (GetFileType(si) & 3); rc |= (GetFileType(so) & 3) << 2; rc |= (GetFileType(se) & 3) << 4;
    if (GetConsoleMode(so, &m)) rc |= 1 << 6;
    if (WriteFile(so, "out", 3, &n, NULL) && n == 3) rc |= 1 << 7;
    n = 0xCC;
    if (argc > 1 && argv[1][0] == 'r' && ReadFile(si, b, sizeof b, &n, NULL) && n == 0) rc |= 1 << 8;
    return rc;
}
static DWORD run_std(HANDLE in, HANDLE out, HANDLE err, DWORD flags, const char *arg) {
    PROCESS_INFORMATION pi; DWORD code = 0xDEAD0000;
    HANDLE hp = t_spawn_child("fio_gft_std", arg, flags, in, out, err, TRUE, NULL, &pi);
    if (!hp) return code;
    if (WaitForSingleObject(hp, FT_CHILD_TIMEOUT) == WAIT_OBJECT_0) GetExitCodeProcess(hp, &code); else { TerminateProcess(hp, 1); code = 0xDEAD0001; }
    CloseHandle(pi.hThread); CloseHandle(hp);
    return code;
}
static HANDLE open_inh(const char *name, DWORD acc, DWORD disp) {
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    return CreateFileA(name, acc, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, disp, FILE_ATTRIBUTE_NORMAL, NULL);
}
TEST(fio_gft_std_inherited_nul) {
    REQUIRE(U(GetFileType));
    HANDLE i = open_inh("NUL", GENERIC_READ, OPEN_EXISTING), o = open_inh("NUL", GENERIC_WRITE, OPEN_EXISTING), e = open_inh("NUL", GENERIC_WRITE, OPEN_EXISTING);
    CHECK(i != INVALID_HANDLE_VALUE && o != INVALID_HANDLE_VALUE && e != INVALID_HANDLE_VALUE);
    DWORD code = run_std(i, o, e, CREATE_NEW_CONSOLE, "r");
    EXPECT_EQ(code & 3, FILE_TYPE_CHAR); EXPECT_EQ((code >> 2) & 3, FILE_TYPE_CHAR); EXPECT_EQ((code >> 4) & 3, FILE_TYPE_CHAR);
    EXPECT_EQ((code >> 6) & 1, 0);                               /* NUL is CHAR but not a console */
    EXPECT_EQ((code >> 7) & 1, 1);                               /* writes to NUL succeed in full */
    EXPECT_EQ((code >> 8) & 1, 1);                               /* reads from NUL return EOF */
    CloseHandle(i); CloseHandle(o); CloseHandle(e);
}
TEST(fio_gft_std_inherited_pipes) {
    REQUIRE(U(GetFileType));
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE ir, iw, or_, ow, er, ew;
    CHECK(CreatePipe(&ir, &iw, &sa, 0)); CHECK(CreatePipe(&or_, &ow, &sa, 0)); CHECK(CreatePipe(&er, &ew, &sa, 0));
    SetHandleInformation(iw, HANDLE_FLAG_INHERIT, 0); SetHandleInformation(or_, HANDLE_FLAG_INHERIT, 0); SetHandleInformation(er, HANDLE_FLAG_INHERIT, 0);
    CloseHandle(iw);                                             /* the child's stdin is at EOF at once */
    DWORD code = run_std(ir, ow, ew, 0, "r");
    EXPECT_EQ(code & 3, FILE_TYPE_PIPE); EXPECT_EQ((code >> 2) & 3, FILE_TYPE_PIPE); EXPECT_EQ((code >> 4) & 3, FILE_TYPE_PIPE);
    EXPECT_EQ((code >> 6) & 1, 0);
    EXPECT_EQ((code >> 7) & 1, 1);
    EXPECT_EQ((code >> 8) & 1, 0);                               /* ReadFile on a pipe at EOF fails (ERROR_BROKEN_PIPE) */
    CloseHandle(ow); CloseHandle(ew);
    char b[8] = {0}; DWORD n = 0;
    CHECK(ReadFile(or_, b, sizeof b, &n, NULL)); EXPECT_EQ(n, 3); CHECK(!memcmp(b, "out", 3));
    CloseHandle(ir); CloseHandle(or_); CloseHandle(er);
}
TEST(fio_gft_std_inherited_files) {
    REQUIRE(U(GetFileType));
    TP(pi_, "gft_in.txt"); TP(po, "gft_out.txt"); TP(pe, "gft_err.txt");
    mkfile(pi_, "x", 1);
    HANDLE i = open_inh(pi_, GENERIC_READ, OPEN_EXISTING), o = open_inh(po, GENERIC_WRITE, CREATE_ALWAYS), e = open_inh(pe, GENERIC_WRITE, CREATE_ALWAYS);
    CHECK(i != INVALID_HANDLE_VALUE && o != INVALID_HANDLE_VALUE && e != INVALID_HANDLE_VALUE);
    DWORD code = run_std(i, o, e, CREATE_NEW_CONSOLE, NULL);
    EXPECT_EQ(code & 3, FILE_TYPE_DISK); EXPECT_EQ((code >> 2) & 3, FILE_TYPE_DISK); EXPECT_EQ((code >> 4) & 3, FILE_TYPE_DISK);
    EXPECT_EQ((code >> 6) & 1, 0);
    EXPECT_EQ((code >> 7) & 1, 1);
    CloseHandle(i); CloseHandle(o); CloseHandle(e);
    rmfile(pi_); rmfile(po); rmfile(pe);
}
TEST(fio_gft_std_inherited_console) {
    REQUIRE(U(GetFileType));
    DWORD code = run_std(NULL, NULL, NULL, CREATE_NEW_CONSOLE, NULL);   /* no redirection: the child's console handles */
    EXPECT_EQ(code & 3, FILE_TYPE_CHAR); EXPECT_EQ((code >> 2) & 3, FILE_TYPE_CHAR); EXPECT_EQ((code >> 4) & 3, FILE_TYPE_CHAR);
    EXPECT_EQ((code >> 6) & 1, 1);
    EXPECT_EQ((code >> 7) & 1, 1);
}
TEST(fio_gft_std_mixed_pipe_stdin_nul_stdout) {
    REQUIRE(U(GetFileType));
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE ir, iw; CHECK(CreatePipe(&ir, &iw, &sa, 0)); SetHandleInformation(iw, HANDLE_FLAG_INHERIT, 0);
    HANDLE o = open_inh("NUL", GENERIC_WRITE, OPEN_EXISTING), e = open_inh("NUL", GENERIC_WRITE, OPEN_EXISTING);
    DWORD code = run_std(ir, o, e, CREATE_NEW_CONSOLE, NULL);
    EXPECT_EQ(code & 3, FILE_TYPE_PIPE); EXPECT_EQ((code >> 2) & 3, FILE_TYPE_CHAR); EXPECT_EQ((code >> 4) & 3, FILE_TYPE_CHAR);   /* each handle is classified on its own */
    CloseHandle(ir); CloseHandle(iw); CloseHandle(o); CloseHandle(e);
}
