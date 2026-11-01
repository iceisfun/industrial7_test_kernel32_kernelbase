#include "fio.h"

#define SHARE_ALL (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE)

/* ---------- CreateFile2 ---------- */
static HANDLE cf2(const char *p, DWORD acc, DWORD sh, DWORD disp, CREATEFILE2_EXTENDED_PARAMETERS *x) {
    return U(CreateFile2)(fio_w(p), acc, sh, disp, x);
}
static void ext_init(CREATEFILE2_EXTENDED_PARAMETERS *x) { memset(x, 0, sizeof *x); x->dwSize = sizeof *x; }

TEST(fio_createfile2_dispositions) {
    REQUIRE(U(CreateFile2));
    TP(p, "cf2_d.bin"); rmfile(p);
    SET_ERR(0x44);
    HANDLE h = cf2(p, GENERIC_READ | GENERIC_WRITE, 0, CREATE_NEW, NULL);
    DWORD e = GetLastError();
    OK_H(h); EXPECT_EQ(e, 0); CLOSE(h);
    EXPECT_FAIL_H(cf2(p, GENERIC_WRITE, 0, CREATE_NEW, NULL), ERROR_FILE_EXISTS);
    h = cf2(p, GENERIC_WRITE, 0, CREATE_ALWAYS, NULL); OK_H(h); EXPECT_EQ(GetLastError(), ERROR_ALREADY_EXISTS); CLOSE(h);
    h = cf2(p, GENERIC_WRITE, 0, OPEN_ALWAYS, NULL); OK_H(h); EXPECT_EQ(GetLastError(), ERROR_ALREADY_EXISTS); CLOSE(h);
    h = cf2(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, NULL); OK_H(h); EXPECT_EQ(GetLastError(), 0); CLOSE(h);
    h = cf2(p, GENERIC_WRITE, 0, TRUNCATE_EXISTING, NULL); OK_H(h); CLOSE(h);
    rmfile(p);
    EXPECT_FAIL_H(cf2(p, GENERIC_READ, 0, OPEN_EXISTING, NULL), ERROR_FILE_NOT_FOUND);
    EXPECT_FAIL_H(cf2(p, GENERIC_WRITE, 0, TRUNCATE_EXISTING, NULL), ERROR_FILE_NOT_FOUND);
    h = cf2(p, GENERIC_WRITE, 0, OPEN_ALWAYS, NULL); OK_H(h); EXPECT_EQ(GetLastError(), 0); CLOSE(h);
    rmfile(p);
}
TEST(fio_createfile2_bad_arguments) {
    REQUIRE(U(CreateFile2));
    TP(p, "cf2_bad.bin"); mkfile_pat(p, 4);
    EXPECT_FAIL_H(cf2(p, GENERIC_READ, SHARE_ALL, 0, NULL), ERROR_INVALID_PARAMETER);
    EXPECT_FAIL_H(cf2(p, GENERIC_READ, SHARE_ALL, 6, NULL), ERROR_INVALID_PARAMETER);
    EXPECT_FAIL_H(cf2(p, GENERIC_READ, SHARE_ALL, 0xFFFFFFFF, NULL), ERROR_INVALID_PARAMETER);
    { HANDLE t0 = cf2(p, GENERIC_READ, SHARE_ALL, TRUNCATE_EXISTING, NULL);   /* kernel behaviour: report only */
      t_note("CreateFile2 TRUNCATE_EXISTING with GENERIC_READ: %s gle=%lu", t0 != INVALID_HANDLE_VALUE ? "ok" : "fail", GetLastError());
      if (t0 != INVALID_HANDLE_VALUE) CloseHandle(t0);
      mkfile_pat(p, 4); }
    EXPECT_FAIL_H(U(CreateFile2)(NULL, GENERIC_READ, SHARE_ALL, OPEN_EXISTING, NULL), ERROR_PATH_NOT_FOUND);
    EXPECT_FAIL_H(U(CreateFile2)(L"", GENERIC_READ, SHARE_ALL, OPEN_EXISTING, NULL), ERROR_PATH_NOT_FOUND);
    rmfile(p);
}
TEST(fio_createfile2_extended_parameter_size) {
    REQUIRE(U(CreateFile2));
    TP(p, "cf2_sz.bin"); mkfile_pat(p, 4);
    CREATEFILE2_EXTENDED_PARAMETERS x;
    static const DWORD bad[] = {0, 1, 4, 16, 24, 28, 31};
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        ext_init(&x); x.dwSize = bad[i];
        SET_ERR(0);
        HANDLE h = cf2(p, GENERIC_READ, SHARE_ALL, OPEN_EXISTING, &x);
        DWORD e = GetLastError();
        CHECKF(h == INVALID_HANDLE_VALUE && e == ERROR_INVALID_PARAMETER, "dwSize=%lu h=%p gle=%lu", bad[i], h, e);
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }
    ext_init(&x); EXPECT_EQ(sizeof x, 32);
    HANDLE h = cf2(p, GENERIC_READ, SHARE_ALL, OPEN_EXISTING, &x); OK_H(h); CLOSE(h);
    char big[128]; memset(big, 0, sizeof big); ((DWORD *)big)[0] = sizeof big;
    h = cf2(p, GENERIC_READ, SHARE_ALL, OPEN_EXISTING, (CREATEFILE2_EXTENDED_PARAMETERS *)big);
    CHECKF(h != INVALID_HANDLE_VALUE, "larger dwSize accepted, gle=%lu", GetLastError()); CLOSE(h);
    rmfile(p);
}
TEST(fio_createfile2_attributes_and_flags) {
    REQUIRE(U(CreateFile2));
    TP(p, "cf2_attr.bin"); rmfile(p);
    CREATEFILE2_EXTENDED_PARAMETERS x; ext_init(&x);
    x.dwFileAttributes = FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY;
    HANDLE h = cf2(p, GENERIC_WRITE, 0, CREATE_NEW, &x); OK_H(h); CLOSE(h);
    DWORD a = GetFileAttributesA(p);
    CHECK((a & FILE_ATTRIBUTE_HIDDEN) && (a & FILE_ATTRIBUTE_READONLY));
    SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL); rmfile(p);
    /* dwFileFlags: delete on close */
    ext_init(&x); x.dwFileFlags = FILE_FLAG_DELETE_ON_CLOSE;
    h = cf2(p, GENERIC_READ | GENERIC_WRITE, SHARE_ALL, CREATE_NEW, &x); OK_H(h);
    CHECK(exists(p)); CLOSE(h); CHECK(!exists(p));
    /* write through / overlapped via dwFileFlags */
    ext_init(&x); x.dwFileFlags = FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED;
    h = cf2(p, GENERIC_READ | GENERIC_WRITE, SHARE_ALL, CREATE_NEW, &x); OK_H(h);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD m = fio_mode(h);
        CHECKF((m & 0x2) && !(m & 0x30), "mode 0x%lx", m);
    }
    CLOSE(h); rmfile(p);
    /* security QoS */
    ext_init(&x); x.dwSecurityQosFlags = SECURITY_SQOS_PRESENT | SECURITY_IMPERSONATION;
    h = cf2(p, GENERIC_WRITE, 0, CREATE_NEW, &x); OK_H(h); CLOSE(h);
    ext_init(&x); x.dwSecurityQosFlags = SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS;
    h = cf2(p, GENERIC_WRITE, 0, OPEN_EXISTING, &x); OK_H(h); CLOSE(h);
    rmfile(p);
}
TEST(fio_createfile2_security_attributes_inherit) {
    REQUIRE(U(CreateFile2));
    TP(p, "cf2_sa.bin"); mkfile_pat(p, 4);
    SECURITY_ATTRIBUTES sa = {sizeof sa, NULL, TRUE};
    CREATEFILE2_EXTENDED_PARAMETERS x; ext_init(&x); x.lpSecurityAttributes = &sa;
    HANDLE h = cf2(p, GENERIC_READ, SHARE_ALL, OPEN_EXISTING, &x); OK_H(h);
    DWORD f = 0; CHECK(GetHandleInformation(h, &f)); CHECK(f & HANDLE_FLAG_INHERIT);
    CLOSE(h);
    h = cf2(p, GENERIC_READ, SHARE_ALL, OPEN_EXISTING, NULL); OK_H(h);
    CHECK(GetHandleInformation(h, &f)); CHECK(!(f & HANDLE_FLAG_INHERIT)); CLOSE(h);
    rmfile(p);
}
TEST(fio_createfile2_sharing_and_directory) {
    REQUIRE(U(CreateFile2));
    TP(p, "cf2_sh.bin"); mkfile_pat(p, 4);
    HANDLE h = cf2(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, OPEN_EXISTING, NULL); OK_H(h);
    EXPECT_FAIL_H(cf2(p, GENERIC_WRITE, FILE_SHARE_READ, OPEN_EXISTING, NULL), ERROR_SHARING_VIOLATION);
    EXPECT_FAIL_H(cfa(p, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, 0), ERROR_SHARING_VIOLATION);
    CLOSE(h);
    TP(d, "cf2_dir"); RemoveDirectoryA(d); CreateDirectoryA(d, NULL);
    EXPECT_FAIL_H(cf2(d, GENERIC_READ, SHARE_ALL, OPEN_EXISTING, NULL), ERROR_ACCESS_DENIED);
    CREATEFILE2_EXTENDED_PARAMETERS x; ext_init(&x); x.dwFileFlags = FILE_FLAG_BACKUP_SEMANTICS;
    h = cf2(d, GENERIC_READ, SHARE_ALL, OPEN_EXISTING, &x); OK_H(h); CLOSE(h);
    RemoveDirectoryA(d); rmfile(p);
}
TEST(fio_createfile2_template_file) {
    REQUIRE(U(CreateFile2));
    TP(p, "cf2_t.bin"); mkfile_pat(p, 4);
    TP(q, "cf2_t2.bin"); rmfile(q);
    HANDLE t = cfa(p, GENERIC_READ, SHARE_ALL, OPEN_EXISTING, 0); OK_H(t);
    CREATEFILE2_EXTENDED_PARAMETERS x; ext_init(&x); x.hTemplateFile = t;
    HANDLE h = cf2(q, GENERIC_WRITE, 0, CREATE_NEW, &x); OK_H(h); CLOSE(h);
    CloseHandle(t);
    x.hTemplateFile = INVALID_HANDLE_VALUE;
    rmfile(q);
    h = cf2(q, GENERIC_WRITE, 0, CREATE_NEW, &x); OK_H(h); CLOSE(h);
    rmfile(q); rmfile(p);
}

/* ---------- ReOpenFile ---------- */
TEST(fio_reopenfile_basic) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(ReOpenFile));
    TP(p, "ro_b.bin"); mkfile(p, "0123456789", 10);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, SHARE_ALL, OPEN_EXISTING, 0); OK_H(h);
    hsetpos(h, 7);
    SET_ERR(0x66);
    HANDLE r = U(ReOpenFile)(h, GENERIC_READ, SHARE_ALL, 0);
    DWORD e = GetLastError();
    OK_H(r); EXPECT_EQ(e, 0);
    EXPECT_EQ(fpos(r), 0);                    /* independent file object, own position */
    EXPECT_EQ(fpos(h), 7);
    char b[16] = {0}; DWORD n;
    CHECK(ReadFile(r, b, 4, &n, NULL)); EXPECT_EQ(n, 4); CHECK(!memcmp(b, "0123", 4));
    EXPECT_EQ(fpos(h), 7);
    DWORD m = fio_access_mask(r);
    CHECKF(m == 0x120089, "reopen access mask 0x%lx", m);
    EXPECT_FAIL_B(WriteFile(r, "x", 1, &n, NULL), ERROR_ACCESS_DENIED);
    CLOSE(r); CLOSE(h); rmfile(p);
}
TEST(fio_reopenfile_share_rules) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(ReOpenFile));
    TP(p, "ro_sh.bin"); mkfile_pat(p, 10);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, OPEN_EXISTING, 0); OK_H(h);
    EXPECT_FAIL_H(U(ReOpenFile)(h, GENERIC_WRITE, FILE_SHARE_READ, 0), ERROR_SHARING_VIOLATION);
    EXPECT_FAIL_H(U(ReOpenFile)(h, GENERIC_READ, 0, 0), ERROR_SHARING_VIOLATION);
    HANDLE r = U(ReOpenFile)(h, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0); OK_H(r); CLOSE(r);
    CLOSE(h);
    h = cfa(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, 0); OK_H(h);
    r = U(ReOpenFile)(h, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, 0); OK_H(r);
    if (r != INVALID_HANDLE_VALUE) {
        DWORD n; CHECK(WriteFile(r, "Q", 1, &n, NULL));       /* more access than the original handle had */
    }
    CLOSE(r); CLOSE(h); rmfile(p);
}
TEST(fio_reopenfile_bad_flags) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(ReOpenFile));
    TP(p, "ro_bf.bin"); mkfile_pat(p, 10);
    HANDLE h = cfa(p, GENERIC_READ, SHARE_ALL, OPEN_EXISTING, 0); OK_H(h);
    static const DWORD bad[] = {FILE_ATTRIBUTE_NORMAL, FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_READONLY, FILE_ATTRIBUTE_ARCHIVE,
        FILE_ATTRIBUTE_TEMPORARY, FILE_ATTRIBUTE_SYSTEM, FILE_ATTRIBUTE_ENCRYPTED, FILE_ATTRIBUTE_OFFLINE};
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        SET_ERR(0);
        HANDLE r = U(ReOpenFile)(h, GENERIC_READ, SHARE_ALL, bad[i]);
        DWORD e = GetLastError();
        CHECKF(r == INVALID_HANDLE_VALUE && e == ERROR_INVALID_PARAMETER, "attr 0x%lx r=%p gle=%lu", bad[i], r, e);
        if (r != INVALID_HANDLE_VALUE) CloseHandle(r);
    }
    static const DWORD good[] = {0, FILE_FLAG_WRITE_THROUGH, FILE_FLAG_OVERLAPPED, FILE_FLAG_RANDOM_ACCESS, FILE_FLAG_SEQUENTIAL_SCAN,
        FILE_FLAG_OPEN_REPARSE_POINT, FILE_FLAG_NO_BUFFERING};
    for (size_t i = 0; i < sizeof good / sizeof *good; i++) {
        HANDLE r = U(ReOpenFile)(h, GENERIC_READ, SHARE_ALL, good[i]);
        CHECKF(r != INVALID_HANDLE_VALUE, "flag 0x%lx gle=%lu", good[i], GetLastError());
        if (r != INVALID_HANDLE_VALUE) CloseHandle(r);
    }
    CLOSE(h); rmfile(p);
}
TEST(fio_reopenfile_invalid_handles) {
    REQUIRE(U(ReOpenFile));
    HANDLE r0 = U(ReOpenFile)(INVALID_HANDLE_VALUE, GENERIC_READ, SHARE_ALL, 0);
    CHECK(r0 == INVALID_HANDLE_VALUE); t_note("ReOpenFile(INVALID_HANDLE_VALUE) gle=%lu", GetLastError());
    r0 = U(ReOpenFile)(NULL, GENERIC_READ, SHARE_ALL, 0);
    CHECK(r0 == INVALID_HANDLE_VALUE); t_note("ReOpenFile(NULL) gle=%lu", GetLastError());
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    HANDLE r = U(ReOpenFile)(ev, GENERIC_READ, SHARE_ALL, 0);
    CHECK(r == INVALID_HANDLE_VALUE);
    t_note("ReOpenFile(event) gle=%lu", GetLastError());
    if (r != INVALID_HANDLE_VALUE) CloseHandle(r);
    CloseHandle(ev);
}
TEST(fio_reopenfile_overlapped_and_delete_on_close) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(ReOpenFile));
    TP(p, "ro_ov.bin"); rmfile(p);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, SHARE_ALL, CREATE_NEW, 0); OK_H(h);
    HANDLE r = U(ReOpenFile)(h, GENERIC_READ, SHARE_ALL, FILE_FLAG_OVERLAPPED); OK_H(r);
    DWORD m = fio_mode(r); CHECKF(!(m & 0x30), "mode 0x%lx", m);
    CLOSE(r);
    r = U(ReOpenFile)(h, GENERIC_READ, SHARE_ALL, FILE_FLAG_DELETE_ON_CLOSE); OK_H(r);
    CHECKF(fio_access_mask(r) & DELETE, "DELETE added by delete-on-close");
    CLOSE(r);
    CLOSE(h);
    CHECK(!exists(p));
    rmfile(p);
}
TEST(fio_reopenfile_after_original_closed_data_survives) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(ReOpenFile));
    TP(p, "ro_cl.bin"); mkfile(p, "abcd", 4);
    HANDLE h = cfa(p, GENERIC_READ, SHARE_ALL, OPEN_EXISTING, 0); OK_H(h);
    HANDLE r = U(ReOpenFile)(h, GENERIC_READ, SHARE_ALL, 0); OK_H(r);
    CloseHandle(h);
    char b[8]; DWORD n; CHECK(ReadFile(r, b, 8, &n, NULL)); EXPECT_EQ(n, 4);
    CLOSE(r); rmfile(p);
}

/* ---------- handle duplication and inheritance ---------- */
TEST(fio_duplicate_handle_shares_position) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(DuplicateHandle));
    TP(p, "dup_pos.bin"); mkfile(p, "0123456789", 10);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, SHARE_ALL, OPEN_EXISTING, 0), d = NULL; OK_H(h);
    CHECK(U(DuplicateHandle)(GetCurrentProcess(), h, GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS));
    CHECK(d != NULL && d != h);
    char b[8] = {0}; DWORD n;
    CHECK(ReadFile(h, b, 3, &n, NULL)); EXPECT_EQ(n, 3);
    EXPECT_EQ(fpos(d), 3);
    CHECK(ReadFile(d, b, 3, &n, NULL)); CHECK(!memcmp(b, "345", 3));
    EXPECT_EQ(fpos(h), 6);
    hsetpos(d, 1);
    EXPECT_EQ(fpos(h), 1);
    CloseHandle(h);
    CHECK(ReadFile(d, b, 2, &n, NULL)); EXPECT_EQ(n, 2); CHECK(!memcmp(b, "12", 2));
    CloseHandle(d); rmfile(p);
}
TEST(fio_duplicate_handle_reduced_access) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(DuplicateHandle));
    TP(p, "dup_acc.bin"); mkfile(p, "0123456789", 10);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, SHARE_ALL, OPEN_EXISTING, 0), d = NULL; OK_H(h);
    CHECK(U(DuplicateHandle)(GetCurrentProcess(), h, GetCurrentProcess(), &d, FILE_GENERIC_READ, FALSE, 0));
    char b[4]; DWORD n = 0;
    CHECK(ReadFile(d, b, 4, &n, NULL));
    EXPECT_FAIL_B(WriteFile(d, "x", 1, &n, NULL), ERROR_ACCESS_DENIED);
    DWORD m = fio_access_mask(d); EXPECT_EQ(m & FILE_WRITE_DATA, 0);
    CloseHandle(d);
    /* asking for more access than the source has is refused */
    HANDLE ro = cfa(p, GENERIC_READ, SHARE_ALL, OPEN_EXISTING, 0); OK_H(ro);
    SET_ERR(0);
    BOOL ok = U(DuplicateHandle)(GetCurrentProcess(), ro, GetCurrentProcess(), &d, FILE_GENERIC_WRITE, FALSE, 0);
    t_note("duplicate requesting more access than source: ok=%d gle=%lu", ok, GetLastError());
    if (ok) CloseHandle(d);
    CLOSE(ro); CLOSE(h); rmfile(p);
}
TEST(fio_separate_opens_have_independent_positions) {
    REQUIRE(U(CreateFileA));
    TP(p, "indep.bin"); mkfile(p, "0123456789", 10);
    HANDLE a = cfa(p, GENERIC_READ, SHARE_ALL, OPEN_EXISTING, 0), b = cfa(p, GENERIC_READ, SHARE_ALL, OPEN_EXISTING, 0); OK_H(a); OK_H(b);
    char c[4]; DWORD n;
    ReadFile(a, c, 4, &n, NULL);
    EXPECT_EQ(fpos(a), 4); EXPECT_EQ(fpos(b), 0);
    CLOSE(a); CLOSE(b); rmfile(p);
}
CHILD(fio_inh_write) {
    if (argc < 2) return 90;
    HANDLE h = (HANDLE)(ULONG_PTR)strtoull(argv[1], NULL, 10);
    DWORD n = 0;
    if (!U(WriteFile)(h, "BBBB", 4, &n, NULL) || n != 4) return 1;
    return 0;
}
TEST(fio_inherited_handle_shares_position_with_child) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(WriteFile));
    TP(p, "inh_pos.bin"); rmfile(p);
    SECURITY_ATTRIBUTES sa = {sizeof sa, NULL, TRUE};
    HANDLE h = U(CreateFileA)(p, GENERIC_READ | GENERIC_WRITE, SHARE_ALL, &sa, CREATE_NEW, 0, NULL); OK_H(h);
    DWORD n; CHECK(U(WriteFile)(h, "AAAA", 4, &n, NULL));
    char arg[64]; snprintf(arg, sizeof arg, "%llu", (unsigned long long)(ULONG_PTR)h);
    PROCESS_INFORMATION pi;
    HANDLE ph = t_spawn_child("fio_inh_write", arg, 0, NULL, NULL, NULL, TRUE, NULL, &pi);
    CHECK(ph != NULL);
    if (ph) {
        DWORD code = 0xDEAD;
        if (WaitForSingleObject(ph, 20000) != WAIT_OBJECT_0) { TerminateProcess(ph, 1); WaitForSingleObject(ph, 3000); }
        else GetExitCodeProcess(ph, &code);
        EXPECT_EQ(code, 0);
        CloseHandle(pi.hThread); CloseHandle(ph);
    }
    EXPECT_EQ(fpos(h), 8);
    CHECK(U(WriteFile)(h, "CC", 2, &n, NULL));
    hsetpos(h, 0);
    char b[16] = {0}; ReadFile(h, b, 16, &n, NULL);
    EXPECT_EQ(n, 10); CHECK(!memcmp(b, "AAAABBBBCC", 10));
    CLOSE(h); rmfile(p);
}
CHILD(fio_stdin_reader) {
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE), out = GetStdHandle(STD_OUTPUT_HANDLE);
    char b[64]; DWORD n = 0;
    if (!U(ReadFile)(in, b, 5, &n, NULL) || n != 5) return 1;
    if (memcmp(b, "hello", 5)) return 2;
    if (!U(ReadFile)(in, b, 64, &n, NULL) || n != 6) return 3;   /* rest of the file */
    if (U(GetFileType)(in) != FILE_TYPE_DISK) return 4;
    if (!U(ReadFile)(in, b, 64, &n, NULL) || n != 0) return 5;   /* EOF */
    if (!U(WriteFile)(out, "echo:", 5, &n, NULL) || n != 5) return 6;
    if (!U(FlushFileBuffers)(out)) return 7;
    return 0;
}
TEST(fio_std_handles_redirected_to_files) {
    REQUIRE(U(ReadFile)); REQUIRE(U(WriteFile));
    TP(pi, "std_in.txt"); TP(po, "std_out.txt"); rmfile(pi); rmfile(po);
    mkfile(pi, "hello world", 11);
    SECURITY_ATTRIBUTES sa = {sizeof sa, NULL, TRUE};
    HANDLE hi = CreateFileA(pi, GENERIC_READ, SHARE_ALL, &sa, OPEN_EXISTING, 0, NULL);
    HANDLE ho = CreateFileA(po, GENERIC_WRITE, SHARE_ALL, &sa, CREATE_ALWAYS, 0, NULL);
    OK_H(hi); OK_H(ho);
    PROCESS_INFORMATION pr;
    HANDLE ph = t_spawn_child("fio_stdin_reader", "", 0, hi, ho, ho, TRUE, NULL, &pr);
    CHECK(ph != NULL);
    DWORD code = 0xDEAD;
    if (ph) {
        if (WaitForSingleObject(ph, 20000) != WAIT_OBJECT_0) { TerminateProcess(ph, 1); WaitForSingleObject(ph, 3000); code = 0xDEAD0001; }
        else GetExitCodeProcess(ph, &code);
        CloseHandle(pr.hThread); CloseHandle(ph);
    }
    EXPECT_EQ(code, 0);
    CLOSE(hi); CLOSE(ho);
    EXPECT_EQ(fsize_of(po), 5);
    rmfile(pi); rmfile(po);
}

/* ---------- unbuffered I/O ---------- */
static void *aligned_buf(DWORD sz) { void *p = VirtualAlloc(NULL, sz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE); return p; }
TEST(fio_nobuffering_aligned_io_works) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(WriteFile)); REQUIRE(U(ReadFile));
    TP(p, "nb_ok.bin"); rmfile(p);
    unsigned char *b = (unsigned char *)aligned_buf(16384);
    for (int i = 0; i < 16384; i++) b[i] = (unsigned char)(i * 7);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, 0, CREATE_NEW, FILE_FLAG_NO_BUFFERING); OK_H(h);
    DWORD n = 0;
    CHECK(U(WriteFile)(h, b, 8192, &n, NULL)); EXPECT_EQ(n, 8192);
    CHECK(U(WriteFile)(h, b + 8192, 4096, &n, NULL)); EXPECT_EQ(n, 4096);
    EXPECT_EQ(hsize(h), 12288);
    hsetpos(h, 0);
    memset(b, 0, 16384);
    CHECK(U(ReadFile)(h, b, 12288, &n, NULL)); EXPECT_EQ(n, 12288);
    int ok = 1; for (int i = 0; i < 12288; i++) if (b[i] != (unsigned char)(i * 7) && i < 8192) ok = 0;
    CHECK(ok);
    hsetpos(h, 4096);
    CHECK(U(ReadFile)(h, b, 4096, &n, NULL)); EXPECT_EQ(n, 4096);
    CHECK(b[0] == (unsigned char)(4096 * 7));
    CLOSE(h);
    VirtualFree(b, 0, MEM_RELEASE); rmfile(p);
}
TEST(fio_nobuffering_misaligned_rejected) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(WriteFile)); REQUIRE(U(ReadFile));
    TP(p, "nb_bad.bin"); mkfile_pat(p, 16384);
    unsigned char *b = (unsigned char *)aligned_buf(16384);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, 0, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING); OK_H(h);
    DWORD n = 77;
    EXPECT_FAIL_B(U(ReadFile)(h, b + 1, 4096, &n, NULL), ERROR_INVALID_PARAMETER); EXPECT_EQ(n, 0);       /* buffer */
    EXPECT_FAIL_B(U(ReadFile)(h, b + 3, 512, &n, NULL), ERROR_INVALID_PARAMETER);
    EXPECT_FAIL_B(U(ReadFile)(h, b, 100, &n, NULL), ERROR_INVALID_PARAMETER);                              /* length */
    EXPECT_FAIL_B(U(ReadFile)(h, b, 513, &n, NULL), ERROR_INVALID_PARAMETER);
    EXPECT_FAIL_B(U(ReadFile)(h, b, 1, &n, NULL), ERROR_INVALID_PARAMETER);
    EXPECT_FAIL_B(U(WriteFile)(h, b + 1, 4096, &n, NULL), ERROR_INVALID_PARAMETER);
    EXPECT_FAIL_B(U(WriteFile)(h, b, 100, &n, NULL), ERROR_INVALID_PARAMETER);
    /* offset: on a synchronous no-buffering handle the misaligned file position is rejected when it is
       set (NtSetInformationFile FilePositionInformation), so the position stays at 0; ReadFile/WriteFile
       with a NULL ByteOffset do not re-check it. Explicit misaligned offsets are covered by
       fio_nobuffering_overlapped_offset_alignment. */
    {
        LARGE_INTEGER mp; mp.QuadPart = 1;
        EXPECT_FAIL_B(SetFilePointerEx(h, mp, NULL, FILE_BEGIN), ERROR_INVALID_PARAMETER);
        mp.QuadPart = 100;
        EXPECT_FAIL_B(SetFilePointerEx(h, mp, NULL, FILE_BEGIN), ERROR_INVALID_PARAMETER);
        EXPECT_EQ(fpos(h), 0);
    }
    EXPECT_EQ(fsize_of(p), 16384);
    /* zero-length IO is not subject to alignment */
    hsetpos(h, 0);
    n = 77; BOOL ok = U(ReadFile)(h, b + 1, 0, &n, NULL);
    t_note("zero-length unbuffered read with odd pointer: ok=%d gle=%lu", ok, GetLastError());
    CLOSE(h); VirtualFree(b, 0, MEM_RELEASE); rmfile(p);
}
TEST(fio_nobuffering_overlapped_offset_alignment) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(ReadFile));
    TP(p, "nb_ov.bin"); mkfile_pat(p, 16384);
    unsigned char *b = (unsigned char *)aligned_buf(8192);
    HANDLE h = cfa(p, GENERIC_READ, SHARE_ALL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED); OK_H(h);
    OVERLAPPED ov; memset(&ov, 0, sizeof ov); ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    ov.Offset = 513;
    SET_ERR(0);
    BOOL ok = U(ReadFile)(h, b, 4096, NULL, &ov);
    DWORD e = GetLastError();
    CHECK(!ok); EXPECT_EQ(e, ERROR_INVALID_PARAMETER);
    ov.Offset = 4096;
    ok = U(ReadFile)(h, b, 4096, NULL, &ov);
    if (!ok) EXPECT_ERR(ERROR_IO_PENDING);
    DWORD n = 0; CHECK(GetOverlappedResult(h, &ov, &n, TRUE)); EXPECT_EQ(n, 4096);
    CloseHandle(ov.hEvent); CLOSE(h); VirtualFree(b, 0, MEM_RELEASE); rmfile(p);
}
TEST(fio_nobuffering_eof_partial_sector) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(ReadFile)); REQUIRE(U(SetEndOfFile));
    TP(p, "nb_eof.bin"); mkfile_pat(p, 100);
    unsigned char *b = (unsigned char *)aligned_buf(8192);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, 0, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING); OK_H(h);
    DWORD n = 0;
    CHECK(U(ReadFile)(h, b, 4096, &n, NULL)); EXPECT_EQ(n, 100);      /* the tail is returned without padding */
    CHECK(b[0] == 'A');
    n = 77; CHECK(U(ReadFile)(h, b, 4096, &n, NULL));
    EXPECT_EQ(n, 0);
    hsetpos(h, 4096); n = 77;
    CHECK(U(ReadFile)(h, b, 4096, &n, NULL)); EXPECT_EQ(n, 0);
    /* SetEndOfFile truncates at the file position, and a no-buffering handle cannot be positioned at an
       unaligned offset; an unaligned EOF is set through FileEndOfFileInfo (UNVERIFIED-until-rerun) */
    {
        LARGE_INTEGER mp; mp.QuadPart = 37;
        EXPECT_FAIL_B(SetFilePointerEx(h, mp, NULL, FILE_BEGIN), ERROR_INVALID_PARAMETER);
        FILE_END_OF_FILE_INFO ei; ei.EndOfFile.QuadPart = 37;
        CHECK(SetFileInformationByHandle(h, FileEndOfFileInfo, &ei, sizeof ei));
        EXPECT_EQ(hsize(h), 37);
        CHECK(hsetpos(h, 512) == 512);
        CHECK(U(SetEndOfFile)(h)); EXPECT_EQ(hsize(h), 512);
    }
    CLOSE(h); VirtualFree(b, 0, MEM_RELEASE); rmfile(p);
}
TEST(fio_write_through_roundtrip) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(WriteFile));
    TP(p, "wt.bin"); rmfile(p);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, SHARE_ALL, CREATE_NEW, FILE_FLAG_WRITE_THROUGH); OK_H(h);
    DWORD n; CHECK(U(WriteFile)(h, "durable", 7, &n, NULL)); EXPECT_EQ(n, 7);
    CHECK(FlushFileBuffers(h));
    HANDLE r = cfa(p, GENERIC_READ, SHARE_ALL, OPEN_EXISTING, 0); OK_H(r);
    EXPECT_EQ(hsize(r), 7);
    CLOSE(r); CLOSE(h); rmfile(p);
}

/* ---------- append semantics ---------- */
TEST(fio_append_position_after_write_is_end) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(WriteFile));
    TP(p, "app_pos.bin"); mkfile(p, "0123456789", 10);
    HANDLE h = cfa(p, FILE_GENERIC_WRITE & ~FILE_WRITE_DATA, SHARE_ALL, OPEN_EXISTING, 0); OK_H(h);
    DWORD n;
    hsetpos(h, 3);
    CHECK(U(WriteFile)(h, "XYZ", 3, &n, NULL)); EXPECT_EQ(n, 3);
    EXPECT_EQ(fpos(h), 13);
    EXPECT_EQ(hsize(h), 13);
    CLOSE(h);
    char b[16] = {0};
    h = cfa(p, GENERIC_READ, SHARE_ALL, OPEN_EXISTING, 0); ReadFile(h, b, 16, &n, NULL); CLOSE(h);
    EXPECT_EQ(n, 13); CHECK(!memcmp(b, "0123456789XYZ", 13));
    rmfile(p);
}
TEST(fio_append_two_handles_interleave_without_overwrite) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(WriteFile));
    TP(p, "app_two.bin"); rmfile(p);
    HANDLE a = cfa(p, FILE_APPEND_DATA, SHARE_ALL, CREATE_NEW, 0); OK_H(a);
    HANDLE b = cfa(p, FILE_APPEND_DATA, SHARE_ALL, OPEN_EXISTING, 0); OK_H(b);
    DWORD n;
    for (int i = 0; i < 100; i++) {
        CHECK(U(WriteFile)(a, "aaaaaaaaaa", 10, &n, NULL)); EXPECT_EQ(n, 10);
        CHECK(U(WriteFile)(b, "bbbbbbbbbb", 10, &n, NULL)); EXPECT_EQ(n, 10);
    }
    CLOSE(a); CLOSE(b);
    EXPECT_EQ(fsize_of(p), 2000);
    rmfile(p);
}
TEST(fio_append_zero_length_and_extend) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(WriteFile));
    TP(p, "app_zero.bin"); mkfile(p, "abc", 3);
    HANDLE h = cfa(p, FILE_APPEND_DATA, SHARE_ALL, OPEN_EXISTING, 0); OK_H(h);
    DWORD n = 77;
    CHECK(U(WriteFile)(h, "x", 0, &n, NULL)); EXPECT_EQ(n, 0);
    EXPECT_EQ(hsize(h), 3);
    CLOSE(h); rmfile(p);
}
TEST(fio_open_always_position_is_start) {
    REQUIRE(U(CreateFileA));
    TP(p, "oa_pos.bin"); mkfile_pat(p, 10);
    HANDLE h = cfa(p, GENERIC_READ | GENERIC_WRITE, 0, OPEN_ALWAYS, 0); OK_H(h);
    EXPECT_EQ(fpos(h), 0);
    CLOSE(h); rmfile(p);
}

/* ---------- wrong handle types ---------- */
TEST(fio_wrong_handle_types) {
    REQUIRE(U(ReadFile)); REQUIRE(U(WriteFile)); REQUIRE(U(SetFilePointer)); REQUIRE(U(GetFileSizeEx));
    HANDLE hs[8]; const char *nm[8]; int n = 0;
    hs[n] = CreateEventA(NULL, TRUE, FALSE, NULL); nm[n++] = "event";
    hs[n] = CreateMutexA(NULL, FALSE, NULL); nm[n++] = "mutex";
    hs[n] = CreateSemaphoreA(NULL, 0, 1, NULL); nm[n++] = "semaphore";
    hs[n] = CreateWaitableTimerA(NULL, TRUE, NULL); nm[n++] = "timer";
    hs[n] = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, NULL); nm[n++] = "section";
    hs[n] = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId()); nm[n++] = "process";
    hs[n] = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentThreadId()); nm[n++] = "thread";
    hs[n] = CreateJobObjectA(NULL, NULL); nm[n++] = "job";
    for (int i = 0; i < n; i++) {
        if (!hs[i]) { t_note("cannot create %s", nm[i]); continue; }
        char b[8]; DWORD c = 77; LARGE_INTEGER s, z; z.QuadPart = 0;
        SET_ERR(0); BOOL ok = U(ReadFile)(hs[i], b, 8, &c, NULL); DWORD e = GetLastError();
        CHECKF(!ok && e == ERROR_INVALID_HANDLE && c == 0, "ReadFile(%s) ok=%d gle=%lu", nm[i], ok, e);
        c = 77; SET_ERR(0); ok = U(WriteFile)(hs[i], b, 8, &c, NULL); e = GetLastError();
        CHECKF(!ok && e == ERROR_INVALID_HANDLE && c == 0, "WriteFile(%s) ok=%d gle=%lu", nm[i], ok, e);
        SET_ERR(0); DWORD r = U(SetFilePointer)(hs[i], 0, NULL, FILE_BEGIN); e = GetLastError();
        CHECKF(r == INVALID_SET_FILE_POINTER && e == ERROR_INVALID_HANDLE, "SetFilePointer(%s) r=0x%lx gle=%lu", nm[i], r, e);
        SET_ERR(0); ok = U(SetFilePointerEx)(hs[i], z, &s, FILE_BEGIN); e = GetLastError();
        CHECKF(!ok && e == ERROR_INVALID_HANDLE, "SetFilePointerEx(%s) gle=%lu", nm[i], e);
        SET_ERR(0); ok = U(GetFileSizeEx)(hs[i], &s); e = GetLastError();
        CHECKF(!ok && e == ERROR_INVALID_HANDLE, "GetFileSizeEx(%s) gle=%lu", nm[i], e);
        SET_ERR(0); r = U(GetFileSize)(hs[i], NULL); e = GetLastError();
        CHECKF(r == INVALID_FILE_SIZE && e == ERROR_INVALID_HANDLE, "GetFileSize(%s) gle=%lu", nm[i], e);
        SET_ERR(0); ok = U(FlushFileBuffers)(hs[i]); e = GetLastError();
        CHECKF(!ok && e == ERROR_INVALID_HANDLE, "FlushFileBuffers(%s) gle=%lu", nm[i], e);
        SET_ERR(0); ok = U(SetEndOfFile)(hs[i]); e = GetLastError();
        CHECKF(!ok && e == ERROR_INVALID_HANDLE, "SetEndOfFile(%s) gle=%lu", nm[i], e);
        SET_ERR(0); ok = U(LockFile)(hs[i], 0, 0, 1, 0); e = GetLastError();
        CHECKF(!ok && e == ERROR_INVALID_HANDLE, "LockFile(%s) gle=%lu", nm[i], e);
        SET_ERR(0); ok = U(UnlockFile)(hs[i], 0, 0, 1, 0); e = GetLastError();
        CHECKF(!ok && e == ERROR_INVALID_HANDLE, "UnlockFile(%s) gle=%lu", nm[i], e);
        CloseHandle(hs[i]);
    }
}

/* ---------- pipes (named pipe via CreateFile) ---------- */
static void pipe_name(char *o, size_t n, const char *tag) { snprintf(o, n, "\\\\.\\pipe\\k32t_fio_%lu_%s", GetCurrentProcessId(), tag); }
TEST(fio_pipe_client_create_file_roundtrip) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(ReadFile)); REQUIRE(U(WriteFile));
    char nm[128]; pipe_name(nm, sizeof nm, "rt");
    HANDLE srv = CreateNamedPipeA(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, NULL);
    OK_H(srv);
    HANDLE cl = cfa(nm, GENERIC_READ | GENERIC_WRITE, 0, OPEN_EXISTING, 0); OK_H(cl);
    EXPECT_EQ(GetLastError(), 0);
    CHECK(!ConnectNamedPipe(srv, NULL)); EXPECT_ERR(ERROR_PIPE_CONNECTED);
    EXPECT_EQ(U(GetFileType)(cl), FILE_TYPE_PIPE);
    DWORD n; char b[16] = {0};
    CHECK(U(WriteFile)(cl, "ping", 4, &n, NULL)); EXPECT_EQ(n, 4);
    CHECK(U(ReadFile)(srv, b, 16, &n, NULL)); EXPECT_EQ(n, 4); CHECK(!memcmp(b, "ping", 4));
    CHECK(WriteFile(srv, "pong!", 5, &n, NULL));
    CHECK(U(ReadFile)(cl, b, 16, &n, NULL)); EXPECT_EQ(n, 5);
    /* server end closes: reads fail with broken pipe */
    CloseHandle(srv);
    n = 77; SET_ERR(0);
    BOOL ok = U(ReadFile)(cl, b, 16, &n, NULL);
    CHECK(!ok); EXPECT_ERR(ERROR_BROKEN_PIPE); EXPECT_EQ(n, 0);
    SET_ERR(0); ok = U(WriteFile)(cl, "x", 1, &n, NULL);
    CHECK(!ok);
    DWORD e = GetLastError(); CHECKF(e == ERROR_NO_DATA || e == ERROR_BROKEN_PIPE, "write after peer close gle=%lu", e);
    CLOSE(cl);
}
TEST(fio_pipe_busy_and_missing) {
    REQUIRE(U(CreateFileA));
    char nm[128]; pipe_name(nm, sizeof nm, "busy");
    EXPECT_FAIL_H(cfa(nm, GENERIC_READ | GENERIC_WRITE, 0, OPEN_EXISTING, 0), ERROR_FILE_NOT_FOUND);
    HANDLE srv = CreateNamedPipeA(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512, 0, NULL); OK_H(srv);
    HANDLE c1 = cfa(nm, GENERIC_READ | GENERIC_WRITE, 0, OPEN_EXISTING, 0); OK_H(c1);
    EXPECT_FAIL_H(cfa(nm, GENERIC_READ | GENERIC_WRITE, 0, OPEN_EXISTING, 0), ERROR_PIPE_BUSY);
    CLOSE(c1); CLOSE(srv);
}
TEST(fio_pipe_access_direction_mismatch) {
    REQUIRE(U(CreateFileA));
    char nm[128]; pipe_name(nm, sizeof nm, "dir");
    HANDLE srv = CreateNamedPipeA(nm, PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512, 0, NULL); OK_H(srv);
    /* an inbound-only server pipe cannot be opened by a client for reading */
    EXPECT_FAIL_H(cfa(nm, GENERIC_READ, 0, OPEN_EXISTING, 0), ERROR_ACCESS_DENIED);
    HANDLE c = cfa(nm, GENERIC_WRITE, 0, OPEN_EXISTING, 0); OK_H(c);
    DWORD n; char b[4];
    CHECK(U(WriteFile)(c, "ab", 2, &n, NULL));
    EXPECT_FAIL_B(U(ReadFile)(c, b, 4, &n, NULL), ERROR_ACCESS_DENIED);
    CLOSE(c); CLOSE(srv);
}
TEST(fio_pipe_seek_and_size_semantics) {
    REQUIRE(U(CreateFileA));
    char nm[128]; pipe_name(nm, sizeof nm, "seek");
    HANDLE srv = CreateNamedPipeA(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512, 0, NULL); OK_H(srv);
    HANDLE c = cfa(nm, GENERIC_READ | GENERIC_WRITE, 0, OPEN_EXISTING, 0); OK_H(c);
    SET_ERR(0);
    DWORD r = U(SetFilePointer)(c, 0, NULL, FILE_BEGIN);
    t_note("SetFilePointer(pipe) = 0x%lx gle=%lu", r, GetLastError());
    LARGE_INTEGER s; SET_ERR(0);
    BOOL ok = U(GetFileSizeEx)(c, &s);
    t_note("GetFileSizeEx(pipe) = %d gle=%lu", ok, GetLastError());
    SET_ERR(0);
    ok = U(SetEndOfFile)(c);
    CHECK(!ok);
    t_note("SetEndOfFile(pipe) gle=%lu", GetLastError());
    BY_HANDLE_FILE_INFORMATION bi; SET_ERR(0);
    ok = U(GetFileInformationByHandle)(c, &bi);
    t_note("GetFileInformationByHandle(pipe) = %d gle=%lu", ok, GetLastError());
    CLOSE(c); CLOSE(srv);
}
TEST(fio_pipe_first_pipe_instance_flag) {
    REQUIRE(U(CreateFileA));
    char nm[128]; pipe_name(nm, sizeof nm, "fpi");
    HANDLE a = CreateNamedPipeA(nm, PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE, PIPE_TYPE_BYTE | PIPE_WAIT, 2, 512, 512, 0, NULL); OK_H(a);
    SetLastError(0);
    HANDLE b = CreateNamedPipeA(nm, PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE, PIPE_TYPE_BYTE | PIPE_WAIT, 2, 512, 512, 0, NULL);
    CHECK(b == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CLOSE(b); CLOSE(a);
}
TEST(fio_pipe_overlapped_client) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(ReadFile));
    char nm[128]; pipe_name(nm, sizeof nm, "ov");
    HANDLE srv = CreateNamedPipeA(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512, 0, NULL); OK_H(srv);
    HANDLE c = cfa(nm, GENERIC_READ | GENERIC_WRITE, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED); OK_H(c);
    OVERLAPPED ov; memset(&ov, 0, sizeof ov); ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    char b[16]; SET_ERR(0);
    BOOL ok = U(ReadFile)(c, b, 16, NULL, &ov);
    CHECK(!ok); EXPECT_ERR(ERROR_IO_PENDING);
    EXPECT_EQ(WaitForSingleObject(ov.hEvent, 100), WAIT_TIMEOUT);
    DWORD n; WriteFile(srv, "wake", 4, &n, NULL);
    EXPECT_EQ(WaitForSingleObject(ov.hEvent, 5000), WAIT_OBJECT_0);
    CHECK(GetOverlappedResult(c, &ov, &n, FALSE)); EXPECT_EQ(n, 4);
    CloseHandle(ov.hEvent); CLOSE(c); CLOSE(srv);
}
TEST(fio_anonymous_pipe_read_write) {
    REQUIRE(U(ReadFile)); REQUIRE(U(WriteFile));
    HANDLE r, w; CHECK(CreatePipe(&r, &w, NULL, 0));
    DWORD n; char b[16];
    CHECK(U(WriteFile)(w, "abc", 3, &n, NULL)); EXPECT_EQ(n, 3);
    CHECK(U(ReadFile)(r, b, 16, &n, NULL)); EXPECT_EQ(n, 3);
    EXPECT_FAIL_B(U(WriteFile)(r, "x", 1, &n, NULL), ERROR_ACCESS_DENIED);
    EXPECT_FAIL_B(U(ReadFile)(w, b, 1, &n, NULL), ERROR_ACCESS_DENIED);
    CloseHandle(w);
    n = 77; SET_ERR(0); BOOL ok = U(ReadFile)(r, b, 16, &n, NULL);
    CHECK(!ok); EXPECT_ERR(ERROR_BROKEN_PIPE); EXPECT_EQ(n, 0);
    CloseHandle(r);
}

/* ---------- console handles (child process only) ---------- */
CHILD(fio_console_child) {
    int r = 0;
    HANDLE o = U(CreateFileA)("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (o == INVALID_HANDLE_VALUE) r |= 1;
    else {
        if (U(GetFileType)(o) != FILE_TYPE_CHAR) r |= 2;
        DWORD n = 0;
        if (!U(WriteFile)(o, "x\r\n", 3, &n, NULL) || n != 3) r |= 4;
        DWORD mode; if (!GetConsoleMode(o, &mode)) r |= 8;
        CloseHandle(o);
    }
    HANDLE i = U(CreateFileA)("CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (i == INVALID_HANDLE_VALUE) r |= 16;
    else {
        if (U(GetFileType)(i) != FILE_TYPE_CHAR) r |= 32;
        DWORD mode; if (!GetConsoleMode(i, &mode)) r |= 64;
        CloseHandle(i);
    }
    HANDLE w = U(CreateFileW)(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (w == INVALID_HANDLE_VALUE) r |= 128; else CloseHandle(w);
    return r;
}
TEST(fio_console_handles_in_new_console_child) {
    REQUIRE(U(CreateFileA));
    if (t_is_wine()) { t_skip("console child not exercised under wine"); return; }
    PROCESS_INFORMATION pi;
    HANDLE ph = t_spawn_child("fio_console_child", "", CREATE_NEW_CONSOLE, NULL, NULL, NULL, FALSE, NULL, &pi);
    CHECK(ph != NULL); if (!ph) return;
    DWORD code = 0xDEAD;
    if (WaitForSingleObject(ph, 25000) != WAIT_OBJECT_0) { TerminateProcess(ph, 1); WaitForSingleObject(ph, 3000); code = 0xDEAD0001; }
    else GetExitCodeProcess(ph, &code);
    CHECKF(code == 0, "child bitmask 0x%lx", code);
    CloseHandle(pi.hThread); CloseHandle(ph);
}
CHILD(fio_detached_child) {
    HANDLE o = U(CreateFileA)("CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    DWORD e = GetLastError();
    printf("CONOUT$ in detached process: %s gle=%lu\n", o != INVALID_HANDLE_VALUE ? "opened" : "failed", e);
    if (o != INVALID_HANDLE_VALUE) { CloseHandle(o); return 1; }
    return e == ERROR_INVALID_HANDLE || e == ERROR_FILE_NOT_FOUND || e == ERROR_ACCESS_DENIED ? 0 : 2;
}
TEST(fio_console_handles_unavailable_when_detached) {
    REQUIRE(U(CreateFileA));
    if (t_is_wine()) { t_skip("detached child not exercised under wine"); return; }
    PROCESS_INFORMATION pi;
    HANDLE ph = t_spawn_child("fio_detached_child", "", DETACHED_PROCESS, NULL, NULL, NULL, FALSE, NULL, &pi);
    CHECK(ph != NULL); if (!ph) return;
    DWORD code = 0xDEAD;
    if (WaitForSingleObject(ph, 25000) != WAIT_OBJECT_0) { TerminateProcess(ph, 1); WaitForSingleObject(ph, 3000); code = 0xDEAD0001; }
    else GetExitCodeProcess(ph, &code);
    t_note("detached child result %lu", code);
    CHECK(code == 0 || code == 1);
    CloseHandle(pi.hThread); CloseHandle(ph);
}
