/* gap_04: GetBinaryType, GetFileSecurityW, GetCachedSigningLevel, GetAppContainerNamedObjectPath,
 * GetDeviceDriver{Base,File}Name */
#include "t.h"

#define A8 __attribute__((aligned(8)))

static DWORD n2d(LONG st) {
    typeof(&RtlNtStatusToDosError) f = (typeof(&RtlNtStatusToDosError))UNT(RtlNtStatusToDosError);
    return f ? f(st) : 0xFFFFFFFF;
}
static void mkfile(const char *leaf, const void *data, DWORD len, char *out, size_t cap) {
    t_tmpfile(out, cap, leaf);
    HANDLE h = CreateFileA(out, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h != INVALID_HANDLE_VALUE) { DWORD n; if (len) WriteFile(h, data, len, &n, NULL); CloseHandle(h); }
}
static void a2w_(const char *a, WCHAR *w, int cap) { MultiByteToWideChar(CP_ACP, 0, a, -1, w, cap); }
#define SENT 0xA5A5A5A5u

/* ================= GetBinaryType ================= */

TEST(gap4_bintype_self_is_64bit) {
    REQUIRE(UK(GetBinaryTypeA)); REQUIRE(UK(GetBinaryTypeW));
    DWORD t = SENT; WCHAR w[MAX_PATH]; a2w_(t_self_path(), w, MAX_PATH);
    SET_ERR(0xBAD);
    BOOL r = UK(GetBinaryTypeA)(t_self_path(), &t);
    CHECKF(r, "err=%lu", GetLastError());
    EXPECT_EQ(t, SCS_64BIT_BINARY);
    t = SENT;
    r = UK(GetBinaryTypeW)(w, &t);
    CHECKF(r, "err=%lu", GetLastError());
    EXPECT_EQ(t, SCS_64BIT_BINARY);
}

TEST(gap4_bintype_alias_export) {
    FARPROC p = t_proc_k32("GetBinaryType");
    REQUIRE(p);
    DWORD t = SENT;
    CHECK(((BOOL (WINAPI *)(LPCSTR, LPDWORD))p)(t_self_path(), &t));
    EXPECT_EQ(t, SCS_64BIT_BINARY);
}

TEST(gap4_bintype_syswow64_is_32bit) {
    REQUIRE(UK(GetBinaryTypeW));
    char win[MAX_PATH], p[MAX_PATH + 32]; WCHAR w[MAX_PATH + 32];
    if (!GetWindowsDirectoryA(win, sizeof win)) { t_skip("no windows dir"); return; }
    snprintf(p, sizeof p, "%s\\SysWOW64\\cmd.exe", win);
    if (GetFileAttributesA(p) == INVALID_FILE_ATTRIBUTES) { t_skip("no SysWOW64\\cmd.exe"); return; }
    a2w_(p, w, MAX_PATH + 32);
    DWORD t = SENT;
    SET_ERR(0xBAD);
    BOOL r = UK(GetBinaryTypeW)(w, &t);
    CHECKF(r, "err=%lu", GetLastError());
    EXPECT_EQ(t, SCS_32BIT_BINARY);
    t = SENT;
    CHECK(UK(GetBinaryTypeA)(p, &t));
    EXPECT_EQ(t, SCS_32BIT_BINARY);
}

TEST(gap4_bintype_system32_cmd_is_64bit) {
    REQUIRE(UK(GetBinaryTypeA));
    char win[MAX_PATH], p[MAX_PATH + 32]; DWORD t = SENT;
    if (!GetWindowsDirectoryA(win, sizeof win)) { t_skip("no windows dir"); return; }
    snprintf(p, sizeof p, "%s\\System32\\cmd.exe", win);
    if (GetFileAttributesA(p) == INVALID_FILE_ATTRIBUTES) { t_skip("no cmd.exe"); return; }
    CHECK(UK(GetBinaryTypeA)(p, &t));
    EXPECT_EQ(t, SCS_64BIT_BINARY);
}

TEST(gap4_bintype_dll_rejected) {
    REQUIRE(UK(GetBinaryTypeA));
    char win[MAX_PATH], p[MAX_PATH + 32]; DWORD t = SENT;
    if (!GetWindowsDirectoryA(win, sizeof win)) { t_skip("no windows dir"); return; }
    snprintf(p, sizeof p, "%s\\System32\\kernel32.dll", win);
    if (GetFileAttributesA(p) == INVALID_FILE_ATTRIBUTES) { t_skip("no kernel32.dll"); return; }
    SET_ERR(0xBAD);
    BOOL r = UK(GetBinaryTypeA)(p, &t);
    EXPECT_EQ(r, 0);
    EXPECT_ERR(ERROR_BAD_EXE_FORMAT);
    EXPECT_EQ(t, SENT);
}

TEST(gap4_bintype_failure_keeps_output) {
    REQUIRE(UK(GetBinaryTypeW));
    DWORD t = SENT;
    CHECK(!UK(GetBinaryTypeW)(L"C:\\k32t_no_such_dir_zz\\nofile.exe", &t));
    EXPECT_EQ(t, SENT);
    CHECK(!UK(GetBinaryTypeW)(L"", &t));
    EXPECT_EQ(t, SENT);
}

TEST(gap4_bintype_missing_file) {
    REQUIRE(UK(GetBinaryTypeA)); REQUIRE(UK(GetBinaryTypeW));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; DWORD t = SENT;
    t_tmpfile(p, sizeof p, "does_not_exist.exe");
    a2w_(p, w, MAX_PATH);
    SET_ERR(0xBAD);
    EXPECT_EQ(UK(GetBinaryTypeA)(p, &t), 0); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    SET_ERR(0xBAD);
    EXPECT_EQ(UK(GetBinaryTypeW)(w, &t), 0); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}

TEST(gap4_bintype_missing_directory) {
    REQUIRE(UK(GetBinaryTypeA)); REQUIRE(UK(GetBinaryTypeW));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; DWORD t = SENT;
    t_tmpfile(p, sizeof p, "nodir\\sub\\x.exe");
    a2w_(p, w, MAX_PATH);
    SET_ERR(0xBAD);
    EXPECT_EQ(UK(GetBinaryTypeA)(p, &t), 0); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    SET_ERR(0xBAD);
    EXPECT_EQ(UK(GetBinaryTypeW)(w, &t), 0); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}

TEST(gap4_bintype_null_and_empty_name) {
    REQUIRE(UK(GetBinaryTypeA)); REQUIRE(UK(GetBinaryTypeW));
    DWORD t = SENT;
    SET_ERR(0xBAD);
    BOOL r = UK(GetBinaryTypeW)(NULL, &t);
    t_note("W(NULL) -> %d err=%lu", r, GetLastError());
    EXPECT_EQ(r, 0); EXPECT_EQ(t, SENT);
    SET_ERR(0xBAD);
    r = UK(GetBinaryTypeW)(L"", &t);
    t_note("W(\"\") -> %d err=%lu", r, GetLastError());
    EXPECT_EQ(r, 0); EXPECT_EQ(t, SENT);
    SET_ERR(0xBAD);
    r = UK(GetBinaryTypeA)(NULL, &t);
    t_note("A(NULL) -> %d err=%lu", r, GetLastError());
    EXPECT_EQ(r, 0); EXPECT_EQ(t, SENT);
    SET_ERR(0xBAD);
    r = UK(GetBinaryTypeA)("", &t);
    t_note("A(\"\") -> %d err=%lu", r, GetLastError());
    EXPECT_EQ(r, 0); EXPECT_EQ(t, SENT);
}

TEST(gap4_bintype_text_file_bad_exe_format) {
    REQUIRE(UK(GetBinaryTypeA));
    static const char *names[] = { "plain.txt", "plain.dat", "plain", "plain.bat", "plain.cmd", "plain.dll", "plain.sys" };
    for (int i = 0; i < 7; i++) {
        char p[MAX_PATH]; DWORD t = SENT;
        mkfile(names[i], "hello, this is not an executable image at all", 45, p, sizeof p);
        SET_ERR(0xBAD);
        BOOL r = UK(GetBinaryTypeA)(p, &t);
        CHECKF(!r, "%s", names[i]);
        CHECKF(GetLastError() == ERROR_BAD_EXE_FORMAT, "%s err=%lu", names[i], GetLastError());
        EXPECT_EQ(t, SENT);
        DeleteFileA(p);
    }
}

TEST(gap4_bintype_text_file_dos_extensions) {
    REQUIRE(UK(GetBinaryTypeA)); REQUIRE(UK(GetBinaryTypeW));
    static const struct { const char *n; DWORD want; } c[] = {
        { "fake.com", SCS_DOS_BINARY }, { "FAKE.COM", SCS_DOS_BINARY }, { "fake.exe", SCS_DOS_BINARY },
        { "FaKe.ExE", SCS_DOS_BINARY }, { "fake.pif", SCS_PIF_BINARY }, { "FAKE.PIF", SCS_PIF_BINARY },
    };
    for (int i = 0; i < 6; i++) {
        char p[MAX_PATH]; WCHAR w[MAX_PATH]; DWORD t = SENT;
        mkfile(c[i].n, "this text is neither MZ nor PE data, just ascii", 48, p, sizeof p);
        a2w_(p, w, MAX_PATH);
        SET_ERR(0xBAD);
        BOOL r = UK(GetBinaryTypeA)(p, &t);
        CHECKF(r, "%s err=%lu", c[i].n, GetLastError());
        CHECKF(t == c[i].want, "%s type %lu want %lu", c[i].n, t, c[i].want);
        t = SENT;
        r = UK(GetBinaryTypeW)(w, &t);
        CHECKF(r && t == c[i].want, "W %s type %lu", c[i].n, t);
        DeleteFileA(p);
    }
}

TEST(gap4_bintype_suffix_only_matters_for_non_mz) {
    REQUIRE(UK(GetBinaryTypeA));
    char p[MAX_PATH]; DWORD t = SENT;
    /* name that merely contains .exe in the middle is not a DOS name */
    mkfile("x.exe.txt", "not an image, plain text data for the loader", 44, p, sizeof p);
    SET_ERR(0xBAD);
    CHECK(!UK(GetBinaryTypeA)(p, &t));
    EXPECT_ERR(ERROR_BAD_EXE_FORMAT);
    DeleteFileA(p);
}

TEST(gap4_bintype_copy_of_self_content_wins) {
    REQUIRE(UK(GetBinaryTypeA));
    static const char *names[] = { "copy.exe", "copy.dat", "copy.com", "copy.pif", "copy" };
    for (int i = 0; i < 5; i++) {
        char p[MAX_PATH]; DWORD t = SENT;
        t_tmpfile(p, sizeof p, names[i]);
        if (!CopyFileA(t_self_path(), p, FALSE)) { t_note("copy failed err=%lu", GetLastError()); return; }
        SET_ERR(0xBAD);
        BOOL r = UK(GetBinaryTypeA)(p, &t);
        CHECKF(r, "%s err=%lu", names[i], GetLastError());
        CHECKF(t == SCS_64BIT_BINARY, "%s type=%lu", names[i], t);
        DeleteFileA(p);
    }
}

TEST(gap4_bintype_forward_slashes_and_case) {
    REQUIRE(UK(GetBinaryTypeA));
    char p[MAX_PATH], q[MAX_PATH]; DWORD t = SENT;
    strcpy(p, t_self_path());
    for (char *c = p; *c; c++) if (*c == '\\') *c = '/';
    SET_ERR(0xBAD);
    CHECKF(UK(GetBinaryTypeA)(p, &t), "err=%lu", GetLastError());
    EXPECT_EQ(t, SCS_64BIT_BINARY);
    strcpy(q, t_self_path());
    CharUpperA(q);
    t = SENT;
    CHECKF(UK(GetBinaryTypeA)(q, &t), "err=%lu", GetLastError());
    EXPECT_EQ(t, SCS_64BIT_BINARY);
}

TEST(gap4_bintype_extended_prefix) {
    REQUIRE(UK(GetBinaryTypeW));
    WCHAR w[MAX_PATH + 8], s[MAX_PATH]; DWORD t = SENT;
    a2w_(t_self_path(), s, MAX_PATH);
    lstrcpyW(w, L"\\\\?\\"); lstrcatW(w, s);
    SET_ERR(0xBAD);
    CHECKF(UK(GetBinaryTypeW)(w, &t), "err=%lu", GetLastError());
    EXPECT_EQ(t, SCS_64BIT_BINARY);
}

TEST(gap4_bintype_directory_fails) {
    REQUIRE(UK(GetBinaryTypeA)); REQUIRE(UNT(RtlNtStatusToDosError));
    DWORD t = SENT;
    SET_ERR(0xBAD);
    BOOL r = UK(GetBinaryTypeA)(t_tmpdir(), &t);
    EXPECT_EQ(r, 0);
    EXPECT_EQ(t, SENT);
    t_note("directory -> err=%lu (file-is-a-directory maps to %lu)", GetLastError(), n2d((LONG)0xC00000BA));
    EXPECT_ERR(n2d((LONG)0xC00000BA));
}

TEST(gap4_bintype_sharing_violation) {
    REQUIRE(UK(GetBinaryTypeA)); REQUIRE(UNT(RtlNtStatusToDosError));
    char p[MAX_PATH]; DWORD t = SENT;
    mkfile("locked.exe", "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", 43, p, sizeof p);
    HANDLE h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    SET_ERR(0xBAD);
    BOOL r = UK(GetBinaryTypeA)(p, &t);
    EXPECT_EQ(r, 0);
    EXPECT_ERR(ERROR_SHARING_VIOLATION);
    CloseHandle(h);
    CHECK(UK(GetBinaryTypeA)(p, &t));
    EXPECT_EQ(t, SCS_DOS_BINARY);
    DeleteFileA(p);
}

TEST(gap4_bintype_empty_file) {
    REQUIRE(UK(GetBinaryTypeA));
    char p[MAX_PATH]; DWORD t = SENT;
    mkfile("empty.exe", NULL, 0, p, sizeof p);
    SET_ERR(0xBAD);
    BOOL r = UK(GetBinaryTypeA)(p, &t);
    t_note("empty .exe -> %d type=%lu err=%lu", r, t, GetLastError());
    if (!r) EXPECT_EQ(t, SENT);
    DeleteFileA(p);
}

TEST(gap4_bintype_mz_stub_only) {
    REQUIRE(UK(GetBinaryTypeA));
    BYTE b[512]; char p[MAX_PATH]; DWORD t = SENT;
    memset(b, 0, sizeof b); b[0] = 'M'; b[1] = 'Z';
    mkfile("stub.exe", b, sizeof b, p, sizeof p);
    SET_ERR(0xBAD);
    BOOL r = UK(GetBinaryTypeA)(p, &t);
    t_note("MZ stub (e_lfanew=0) -> %d type=%lu err=%lu", r, t, GetLastError());
    DeleteFileA(p);
    mkfile("stub.dat", b, sizeof b, p, sizeof p);
    r = UK(GetBinaryTypeA)(p, &t);
    t_note("MZ stub .dat -> %d type=%lu err=%lu", r, t, GetLastError());
    DeleteFileA(p);
}

TEST(gap4_bintype_truncated_pe) {
    REQUIRE(UK(GetBinaryTypeA));
    static BYTE b[1024]; char p[MAX_PATH]; DWORD t = SENT, n;
    HANDLE h = CreateFileA(t_self_path(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE); if (h == INVALID_HANDLE_VALUE) return;
    ReadFile(h, b, sizeof b, &n, NULL); CloseHandle(h);
    mkfile("trunc.exe", b, n, p, sizeof p);
    SET_ERR(0xBAD);
    BOOL r = UK(GetBinaryTypeA)(p, &t);
    t_note("PE truncated to %lu bytes -> %d type=%lu err=%lu", n, r, t, GetLastError());
    EXPECT_EQ(r, 0);
    DeleteFileA(p);
}

TEST(gap4_bintype_repeated_calls) {
    REQUIRE(UK(GetBinaryTypeW));
    WCHAR w[MAX_PATH]; a2w_(t_self_path(), w, MAX_PATH);
    for (int i = 0; i < 300; i++) {
        DWORD t = SENT;
        if (!UK(GetBinaryTypeW)(w, &t) || t != SCS_64BIT_BINARY) { CHECKF(0, "iteration %d t=%lu", i, t); break; }
    }
    CHECK(1);
}

TEST(gap4_bintype_no_handle_leak) {
    REQUIRE(UK(GetBinaryTypeW));
    WCHAR w[MAX_PATH]; a2w_(t_self_path(), w, MAX_PATH);
    DWORD before = 0, after = 0, t;
    GetProcessHandleCount(GetCurrentProcess(), &before);
    for (int i = 0; i < 200; i++) UK(GetBinaryTypeW)(w, &t);
    for (int i = 0; i < 200; i++) UK(GetBinaryTypeW)(L"C:\\k32t_missing_\\a.exe", &t);
    GetProcessHandleCount(GetCurrentProcess(), &after);
    CHECKF(after <= before + 2, "handles %lu -> %lu", before, after);
}

/* ================= GetFileSecurityW ================= */

static int mkfile_sd(char *p, size_t cap, const char *leaf) {
    mkfile(leaf, "sd", 2, p, cap);
    return GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES;
}

TEST(gap4_filesd_size_query_null_buffer) {
    REQUIRE(U(GetFileSecurityW));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; DWORD need = 0xFFFFFFFF;
    CHECK(mkfile_sd(p, sizeof p, "sd1.txt")); a2w_(p, w, MAX_PATH);
    SET_ERR(0xBAD);
    BOOL r = U(GetFileSecurityW)(w, DACL_SECURITY_INFORMATION, NULL, 0, &need);
    EXPECT_EQ(r, 0); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECKF(need >= 20, "need=%lu", need);
    DeleteFileA(p);
}

TEST(gap4_filesd_two_step_fetch) {
    REQUIRE(U(GetFileSecurityW)); REQUIRE(U(IsValidSecurityDescriptor)); REQUIRE(U(GetSecurityDescriptorControl));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; DWORD need = 0, need2 = 0;
    CHECK(mkfile_sd(p, sizeof p, "sd2.txt")); a2w_(p, w, MAX_PATH);
    U(GetFileSecurityW)(w, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, NULL, 0, &need);
    BYTE *sd = calloc(1, need + 16);
    SET_ERR(0xBAD);
    BOOL r = U(GetFileSecurityW)(w, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, sd, need, &need2);
    CHECKF(r, "err=%lu", GetLastError());
    EXPECT_EQ(need2, need);
    CHECK(U(IsValidSecurityDescriptor)(sd));
    SECURITY_DESCRIPTOR_CONTROL c = 0; DWORD rev = 0;
    CHECK(U(GetSecurityDescriptorControl)(sd, &c, &rev));
    EXPECT_EQ(rev, 1);
    CHECK(c & SE_SELF_RELATIVE);
    free(sd);
    DeleteFileA(p);
}

TEST(gap4_filesd_length_matches_need) {
    REQUIRE(U(GetFileSecurityW)); REQUIRE(U(GetSecurityDescriptorLength));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; DWORD need = 0;
    CHECK(mkfile_sd(p, sizeof p, "sd3.txt")); a2w_(p, w, MAX_PATH);
    BYTE A8 sd[4096];
    CHECK(U(GetFileSecurityW)(w, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, sd, sizeof sd, &need));
    EXPECT_EQ(U(GetSecurityDescriptorLength)(sd), need);
    DeleteFileA(p);
}

TEST(gap4_filesd_buffer_one_short) {
    REQUIRE(U(GetFileSecurityW));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; DWORD need = 0, n2 = 0;
    CHECK(mkfile_sd(p, sizeof p, "sd4.txt")); a2w_(p, w, MAX_PATH);
    BYTE A8 sd[4096];
    CHECK(U(GetFileSecurityW)(w, DACL_SECURITY_INFORMATION, sd, sizeof sd, &need));
    memset(sd, 0x77, sizeof sd);
    SET_ERR(0xBAD);
    BOOL r = U(GetFileSecurityW)(w, DACL_SECURITY_INFORMATION, sd, need - 1, &n2);
    EXPECT_EQ(r, 0); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(n2, need);
    for (DWORD n = 0; n < need; n += 3) {
        n2 = 0;
        SET_ERR(0xBAD);
        r = U(GetFileSecurityW)(w, DACL_SECURITY_INFORMATION, sd, n, &n2);
        if (r || GetLastError() != ERROR_INSUFFICIENT_BUFFER || n2 != need) { CHECKF(0, "n=%lu r=%d err=%lu need=%lu", n, r, GetLastError(), n2); break; }
    }
    DeleteFileA(p);
}

TEST(gap4_filesd_buffer_larger_than_needed) {
    REQUIRE(U(GetFileSecurityW));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; DWORD need = 0, n2 = 0;
    CHECK(mkfile_sd(p, sizeof p, "sd5.txt")); a2w_(p, w, MAX_PATH);
    BYTE A8 sd[4096];
    CHECK(U(GetFileSecurityW)(w, DACL_SECURITY_INFORMATION, NULL, 0, &need) == 0);
    memset(sd, 0x77, sizeof sd);
    CHECK(U(GetFileSecurityW)(w, DACL_SECURITY_INFORMATION, sd, sizeof sd, &n2));
    EXPECT_EQ(n2, need);
    EXPECT_EQ(sd[need], 0x77);
    EXPECT_EQ(sd[sizeof sd - 1], 0x77);
    DeleteFileA(p);
}

TEST(gap4_filesd_owner_only) {
    REQUIRE(U(GetFileSecurityW)); REQUIRE(U(GetSecurityDescriptorOwner)); REQUIRE(U(GetSecurityDescriptorGroup));
    REQUIRE(U(GetSecurityDescriptorDacl)); REQUIRE(U(IsValidSid));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; DWORD need;
    CHECK(mkfile_sd(p, sizeof p, "sd6.txt")); a2w_(p, w, MAX_PATH);
    BYTE A8 sd[4096];
    SET_ERR(0xBAD);
    CHECKF(U(GetFileSecurityW)(w, OWNER_SECURITY_INFORMATION, sd, sizeof sd, &need), "err=%lu", GetLastError());
    PSID o = NULL, g = NULL; BOOL d = FALSE, present = TRUE; PACL acl = NULL;
    CHECK(U(GetSecurityDescriptorOwner)(sd, &o, &d));
    CHECK(o != NULL);
    if (o) CHECK(U(IsValidSid)(o));
    CHECK(U(GetSecurityDescriptorGroup)(sd, &g, &d));
    CHECK(g == NULL);
    CHECK(U(GetSecurityDescriptorDacl)(sd, &present, &acl, &d));
    CHECK(!present);
    CHECK(need >= 20 + 12);
    DeleteFileA(p);
}

TEST(gap4_filesd_group_only) {
    REQUIRE(U(GetFileSecurityW)); REQUIRE(U(GetSecurityDescriptorOwner)); REQUIRE(U(GetSecurityDescriptorGroup));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; DWORD need;
    CHECK(mkfile_sd(p, sizeof p, "sd7.txt")); a2w_(p, w, MAX_PATH);
    BYTE A8 sd[4096];
    CHECKF(U(GetFileSecurityW)(w, GROUP_SECURITY_INFORMATION, sd, sizeof sd, &need), "err=%lu", GetLastError());
    PSID o = (PSID)1, g = NULL; BOOL d;
    CHECK(U(GetSecurityDescriptorOwner)(sd, &o, &d));
    CHECK(o == NULL);
    CHECK(U(GetSecurityDescriptorGroup)(sd, &g, &d));
    CHECK(g != NULL);
    DeleteFileA(p);
}

TEST(gap4_filesd_dacl_only) {
    REQUIRE(U(GetFileSecurityW)); REQUIRE(U(GetSecurityDescriptorOwner)); REQUIRE(U(GetSecurityDescriptorDacl));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; DWORD need;
    CHECK(mkfile_sd(p, sizeof p, "sd8.txt")); a2w_(p, w, MAX_PATH);
    BYTE A8 sd[4096];
    CHECKF(U(GetFileSecurityW)(w, DACL_SECURITY_INFORMATION, sd, sizeof sd, &need), "err=%lu", GetLastError());
    PSID o = (PSID)1; PACL acl = NULL; BOOL d, present = FALSE;
    CHECK(U(GetSecurityDescriptorOwner)(sd, &o, &d));
    CHECK(o == NULL);
    CHECK(U(GetSecurityDescriptorDacl)(sd, &present, &acl, &d));
    if (!present) t_note("DACL not present on this file system");
    else CHECK(acl != NULL);
    DeleteFileA(p);
}

TEST(gap4_filesd_need_grows_with_more_info) {
    REQUIRE(U(GetFileSecurityW));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; DWORD nd = 0, nod = 0, nogd = 0, no = 0, ng = 0;
    CHECK(mkfile_sd(p, sizeof p, "sd9.txt")); a2w_(p, w, MAX_PATH);
    U(GetFileSecurityW)(w, DACL_SECURITY_INFORMATION, NULL, 0, &nd);
    U(GetFileSecurityW)(w, OWNER_SECURITY_INFORMATION, NULL, 0, &no);
    U(GetFileSecurityW)(w, GROUP_SECURITY_INFORMATION, NULL, 0, &ng);
    U(GetFileSecurityW)(w, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, NULL, 0, &nod);
    U(GetFileSecurityW)(w, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, NULL, 0, &nogd);
    t_note("need: D=%lu O=%lu G=%lu OD=%lu OGD=%lu", nd, no, ng, nod, nogd);
    CHECK(nod > nd);
    CHECK(nod > no);
    CHECK(nogd > nod);
    CHECK(nd >= 20 && no >= 32 && ng >= 32);
    DeleteFileA(p);
}

TEST(gap4_filesd_repeatable_bytes) {
    REQUIRE(U(GetFileSecurityW));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; DWORD n1, n2;
    CHECK(mkfile_sd(p, sizeof p, "sd10.txt")); a2w_(p, w, MAX_PATH);
    BYTE A8 a[4096], b[4096];
    memset(a, 0, sizeof a); memset(b, 0, sizeof b);
    DWORD info = OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION;
    CHECK(U(GetFileSecurityW)(w, info, a, sizeof a, &n1));
    CHECK(U(GetFileSecurityW)(w, info, b, sizeof b, &n2));
    EXPECT_EQ(n1, n2);
    CHECK(!memcmp(a, b, n1));
    DeleteFileA(p);
}

TEST(gap4_filesd_sacl_needs_privilege) {
    REQUIRE(U(GetFileSecurityW));
    if (t_is_wine()) { t_skip("privilege model differs on emulator"); return; }
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; DWORD need = 0xFFFFFFFF;
    CHECK(mkfile_sd(p, sizeof p, "sd11.txt")); a2w_(p, w, MAX_PATH);
    BYTE A8 sd[4096];
    /* SeSecurityPrivilege is never enabled in a fresh process token */
    SET_ERR(0xBAD);
    BOOL r = U(GetFileSecurityW)(w, SACL_SECURITY_INFORMATION, sd, sizeof sd, &need);
    EXPECT_EQ(r, 0); EXPECT_ERR(ERROR_PRIVILEGE_NOT_HELD);
    SET_ERR(0xBAD);
    r = U(GetFileSecurityW)(w, SACL_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION, sd, sizeof sd, &need);
    EXPECT_EQ(r, 0); EXPECT_ERR(ERROR_PRIVILEGE_NOT_HELD);
    DeleteFileA(p);
}

TEST(gap4_filesd_label_info) {
    REQUIRE(U(GetFileSecurityW));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; DWORD need = 0;
    CHECK(mkfile_sd(p, sizeof p, "sd12.txt")); a2w_(p, w, MAX_PATH);
    BYTE A8 sd[4096];
    SET_ERR(0xBAD);
    BOOL r = U(GetFileSecurityW)(w, LABEL_SECURITY_INFORMATION, sd, sizeof sd, &need);
    CHECKF(r, "err=%lu", GetLastError());
    r = U(GetFileSecurityW)(w, ATTRIBUTE_SECURITY_INFORMATION, sd, sizeof sd, &need);
    t_note("ATTRIBUTE_SECURITY_INFORMATION -> %d err=%lu", r, GetLastError());
    r = U(GetFileSecurityW)(w, SCOPE_SECURITY_INFORMATION, sd, sizeof sd, &need);
    t_note("SCOPE_SECURITY_INFORMATION -> %d err=%lu", r, GetLastError());
    DeleteFileA(p);
}

TEST(gap4_filesd_odd_information_values) {
    REQUIRE(U(GetFileSecurityW));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; DWORD need = 0;
    CHECK(mkfile_sd(p, sizeof p, "sd13.txt")); a2w_(p, w, MAX_PATH);
    BYTE A8 sd[4096];
    static const DWORD infos[] = { 0, 0x80, 0x100, 0x10000, 0x80000000, 0xFFFFFFFF, 0x1000 };
    for (int i = 0; i < 7; i++) {
        SET_ERR(0xBAD);
        BOOL r = U(GetFileSecurityW)(w, infos[i], sd, sizeof sd, &need);
        t_note("info 0x%08lx -> %d err=%lu need=%lu", infos[i], r, GetLastError(), need);
    }
    DeleteFileA(p);
}

TEST(gap4_filesd_missing_file_and_dir) {
    REQUIRE(U(GetFileSecurityW));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; DWORD need = 0xFEFEFEFE;
    BYTE A8 sd[256];
    t_tmpfile(p, sizeof p, "nothere.txt"); a2w_(p, w, MAX_PATH);
    SET_ERR(0xBAD);
    EXPECT_EQ(U(GetFileSecurityW)(w, DACL_SECURITY_INFORMATION, sd, sizeof sd, &need), 0);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    EXPECT_EQ(need, 0xFEFEFEFE);
    t_tmpfile(p, sizeof p, "nodir\\x.txt"); a2w_(p, w, MAX_PATH);
    SET_ERR(0xBAD);
    EXPECT_EQ(U(GetFileSecurityW)(w, DACL_SECURITY_INFORMATION, sd, sizeof sd, &need), 0);
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}

TEST(gap4_filesd_null_and_empty_name) {
    REQUIRE(U(GetFileSecurityW));
    DWORD need = 0xFEFEFEFE; BYTE A8 sd[256];
    SET_ERR(0xBAD);
    EXPECT_EQ(U(GetFileSecurityW)(NULL, DACL_SECURITY_INFORMATION, sd, sizeof sd, &need), 0);
    EXPECT_ERR(ERROR_INVALID_NAME);
    SET_ERR(0xBAD);
    EXPECT_EQ(U(GetFileSecurityW)(L"", DACL_SECURITY_INFORMATION, sd, sizeof sd, &need), 0);
    EXPECT_ERR(ERROR_INVALID_NAME);
    EXPECT_EQ(need, 0xFEFEFEFE);
}

TEST(gap4_filesd_exclusively_open_file) {
    REQUIRE(U(GetFileSecurityW));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; DWORD need;
    CHECK(mkfile_sd(p, sizeof p, "sd14.txt")); a2w_(p, w, MAX_PATH);
    HANDLE h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    BYTE A8 sd[4096];
    SET_ERR(0xBAD);
    BOOL r = U(GetFileSecurityW)(w, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, sd, sizeof sd, &need);
    CHECKF(r, "err=%lu", GetLastError());
    CloseHandle(h);
    DeleteFileA(p);
}

TEST(gap4_filesd_directory) {
    REQUIRE(U(GetFileSecurityW));
    WCHAR w[MAX_PATH]; DWORD need = 0;
    a2w_(t_tmpdir(), w, MAX_PATH);
    BYTE A8 sd[4096];
    SET_ERR(0xBAD);
    BOOL r = U(GetFileSecurityW)(w, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, sd, sizeof sd, &need);
    CHECKF(r, "err=%lu", GetLastError());
    /* with and without the trailing backslash */
    size_t l = lstrlenW(w);
    if (l > 3 && w[l - 1] == L'\\') {
        w[l - 1] = 0;
        r = U(GetFileSecurityW)(w, DACL_SECURITY_INFORMATION, sd, sizeof sd, &need);
        CHECKF(r, "no trailing slash err=%lu", GetLastError());
    }
}

TEST(gap4_filesd_extended_prefix_and_slashes) {
    REQUIRE(U(GetFileSecurityW));
    char p[MAX_PATH]; WCHAR w[MAX_PATH + 8], s[MAX_PATH]; DWORD need;
    CHECK(mkfile_sd(p, sizeof p, "sd15.txt")); a2w_(p, s, MAX_PATH);
    BYTE A8 sd[4096];
    lstrcpyW(w, L"\\\\?\\"); lstrcatW(w, s);
    CHECKF(U(GetFileSecurityW)(w, DACL_SECURITY_INFORMATION, sd, sizeof sd, &need), "prefix err=%lu", GetLastError());
    lstrcpyW(w, s);
    for (WCHAR *c = w; *c; c++) if (*c == L'\\') *c = L'/';
    CHECKF(U(GetFileSecurityW)(w, DACL_SECURITY_INFORMATION, sd, sizeof sd, &need), "slashes err=%lu", GetLastError());
    DeleteFileA(p);
}

TEST(gap4_filesd_case_and_trailing_dot) {
    REQUIRE(U(GetFileSecurityW));
    char p[MAX_PATH], q[MAX_PATH + 4]; WCHAR w[MAX_PATH + 4]; DWORD need;
    CHECK(mkfile_sd(p, sizeof p, "sdcase.txt"));
    BYTE A8 sd[4096];
    strcpy(q, p); CharUpperA(q); a2w_(q, w, MAX_PATH);
    CHECKF(U(GetFileSecurityW)(w, DACL_SECURITY_INFORMATION, sd, sizeof sd, &need), "upper err=%lu", GetLastError());
    mkfile("sddot", "x", 1, p, sizeof p);
    strcat(p, ".");
    a2w_(p, w, MAX_PATH);
    BOOL r = U(GetFileSecurityW)(w, DACL_SECURITY_INFORMATION, sd, sizeof sd, &need);
    CHECKF(r, "trailing dot err=%lu", GetLastError());
    t_tmpfile(p, sizeof p, "sddot"); DeleteFileA(p);
    t_tmpfile(p, sizeof p, "sdcase.txt"); DeleteFileA(p);
}

TEST(gap4_filesd_long_path) {
    REQUIRE(U(GetFileSecurityW));
    WCHAR base[MAX_PATH], d1[600], d2[900], f[1000]; DWORD need = 0;
    a2w_(t_tmpdir(), base, MAX_PATH);
    WCHAR seg[201]; for (int i = 0; i < 200; i++) seg[i] = L'q'; seg[200] = 0;
    wsprintfW(d1, L"\\\\?\\%s%s", base, seg);
    wsprintfW(d2, L"%s\\%s", d1, seg);
    wsprintfW(f, L"%s\\leaf.txt", d2);
    BYTE A8 sd[4096];
    if (!CreateDirectoryW(d1, NULL) || !CreateDirectoryW(d2, NULL)) { t_skip("cannot create long dirs err=%lu", GetLastError()); RemoveDirectoryW(d1); return; }
    HANDLE h = CreateFileW(f, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    SET_ERR(0xBAD);
    CHECKF(U(GetFileSecurityW)(f, DACL_SECURITY_INFORMATION, sd, sizeof sd, &need), "long path err=%lu", GetLastError());
    CHECKF(lstrlenW(f) > 260, "path len %d", lstrlenW(f));
    DeleteFileW(f); RemoveDirectoryW(d2); RemoveDirectoryW(d1);
}

TEST(gap4_filesd_invalid_name_chars) {
    REQUIRE(U(GetFileSecurityW));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; DWORD need = 0; BYTE A8 sd[256];
    t_tmpfile(p, sizeof p, "wild*.txt"); a2w_(p, w, MAX_PATH);
    SET_ERR(0xBAD);
    BOOL r = U(GetFileSecurityW)(w, DACL_SECURITY_INFORMATION, sd, sizeof sd, &need);
    EXPECT_EQ(r, 0);
    t_note("wildcard name -> err=%lu", GetLastError());
    t_tmpfile(p, sizeof p, "q?.txt"); a2w_(p, w, MAX_PATH);
    r = U(GetFileSecurityW)(w, DACL_SECURITY_INFORMATION, sd, sizeof sd, &need);
    EXPECT_EQ(r, 0);
    t_note("question mark name -> err=%lu", GetLastError());
}

TEST(gap4_filesd_volume_root) {
    REQUIRE(U(GetFileSecurityW));
    char p[MAX_PATH]; WCHAR w[8]; DWORD need = 0; BYTE A8 sd[4096];
    if (GetWindowsDirectoryA(p, sizeof p) < 3) return;
    w[0] = (WCHAR)p[0]; w[1] = L':'; w[2] = L'\\'; w[3] = 0;
    SET_ERR(0xBAD);
    BOOL r = U(GetFileSecurityW)(w, DACL_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION, sd, sizeof sd, &need);
    CHECKF(r, "root err=%lu", GetLastError());
}

TEST(gap4_filesd_system_file_readable) {
    REQUIRE(U(GetFileSecurityW));
    char win[MAX_PATH], p[MAX_PATH + 32]; WCHAR w[MAX_PATH + 32]; DWORD need = 0; BYTE A8 sd[4096];
    if (!GetWindowsDirectoryA(win, sizeof win)) return;
    snprintf(p, sizeof p, "%s\\System32\\kernel32.dll", win); a2w_(p, w, MAX_PATH + 32);
    SET_ERR(0xBAD);
    BOOL r = U(GetFileSecurityW)(w, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, sd, sizeof sd, &need);
    CHECKF(r, "err=%lu", GetLastError());
}

TEST(gap4_filesd_readonly_file) {
    REQUIRE(U(GetFileSecurityW));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; DWORD need; BYTE A8 sd[4096];
    CHECK(mkfile_sd(p, sizeof p, "sdro.txt")); a2w_(p, w, MAX_PATH);
    SetFileAttributesA(p, FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN);
    CHECKF(U(GetFileSecurityW)(w, DACL_SECURITY_INFORMATION, sd, sizeof sd, &need), "err=%lu", GetLastError());
    SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL);
    DeleteFileA(p);
}

TEST(gap4_filesd_owner_is_valid_sid_and_matches_token_or_admins) {
    REQUIRE(U(GetFileSecurityW)); REQUIRE(U(GetSecurityDescriptorOwner)); REQUIRE(U(IsValidSid)); REQUIRE(U(GetLengthSid));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; DWORD need; BYTE A8 sd[4096];
    CHECK(mkfile_sd(p, sizeof p, "sdown.txt")); a2w_(p, w, MAX_PATH);
    CHECK(U(GetFileSecurityW)(w, OWNER_SECURITY_INFORMATION, sd, sizeof sd, &need));
    PSID o = NULL; BOOL d;
    CHECK(U(GetSecurityDescriptorOwner)(sd, &o, &d));
    if (o) {
        CHECK(U(IsValidSid)(o));
        CHECK(U(GetLengthSid)(o) >= 12);
        CHECK(((BYTE *)o)[0] == 1);
    }
    DeleteFileA(p);
}

TEST(gap4_filesd_stale_need_not_clobbered_on_open_failure) {
    REQUIRE(U(GetFileSecurityW));
    DWORD need = 77; BYTE A8 sd[64];
    CHECK(!U(GetFileSecurityW)(L"Z:\\k32t\\definitely\\missing.txt", DACL_SECURITY_INFORMATION, sd, sizeof sd, &need));
    EXPECT_EQ(need, 77);
}

TEST(gap4_filesd_many_calls) {
    REQUIRE(U(GetFileSecurityW));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; DWORD need; BYTE A8 sd[4096];
    CHECK(mkfile_sd(p, sizeof p, "sdloop.txt")); a2w_(p, w, MAX_PATH);
    DWORD h0 = 0, h1 = 0; GetProcessHandleCount(GetCurrentProcess(), &h0);
    for (int i = 0; i < 500; i++) if (!U(GetFileSecurityW)(w, DACL_SECURITY_INFORMATION, sd, sizeof sd, &need)) { CHECKF(0, "iter %d err=%lu", i, GetLastError()); break; }
    GetProcessHandleCount(GetCurrentProcess(), &h1);
    CHECKF(h1 <= h0 + 2, "handles %lu -> %lu", h0, h1);
    DeleteFileA(p);
}

/* ================= GetCachedSigningLevel ================= */

UDECL(BOOL, GetCachedSigningLevel, (HANDLE, PULONG, PULONG, PUCHAR, PULONG, PULONG));

TEST(gap4_signinglevel_invalid_handles) {
    GetCachedSigningLevel_pfn f = UN(GetCachedSigningLevel);
    REQUIRE(f); REQUIRE(UNT(RtlNtStatusToDosError));
    if (t_is_wine()) { t_skip("not implemented on emulator"); return; }
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x1234, (HANDLE)(ULONG_PTR)0xFFFFFFFC };
    for (int i = 0; i < 4; i++) {
        ULONG fl = 0xAA, lvl = 0xBB, tpsz = 20, alg = 0xCC; UCHAR tp[64];
        SET_ERR(0xBAD);
        BOOL r = f(bad[i], &fl, &lvl, tp, &tpsz, &alg);
        CHECKF(!r, "handle %p", bad[i]);
        CHECKF(GetLastError() != 0xBAD && GetLastError() != 0, "handle %p err=%lu", bad[i], GetLastError());
        if (i == 2) EXPECT_ERR(ERROR_INVALID_HANDLE);
        EXPECT_EQ(lvl, 0xBB);
    }
}

TEST(gap4_signinglevel_wrong_object_type) {
    GetCachedSigningLevel_pfn f = UN(GetCachedSigningLevel);
    REQUIRE(f); REQUIRE(UNT(RtlNtStatusToDosError));
    if (t_is_wine()) { t_skip("not implemented on emulator"); return; }
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    ULONG fl = 0, lvl = 0xBB, tpsz = 20, alg = 0; UCHAR tp[64];
    SET_ERR(0xBAD);
    BOOL r = f(ev, &fl, &lvl, tp, &tpsz, &alg);
    EXPECT_EQ(r, 0);
    EXPECT_ERR(n2d((LONG)0xC0000024));
    EXPECT_EQ(lvl, 0xBB);
    CloseHandle(ev);
}

TEST(gap4_signinglevel_on_system_binary) {
    GetCachedSigningLevel_pfn f = UN(GetCachedSigningLevel);
    REQUIRE(f);
    if (t_is_wine()) { t_skip("not implemented on emulator"); return; }
    char win[MAX_PATH], p[MAX_PATH + 32];
    if (!GetWindowsDirectoryA(win, sizeof win)) return;
    snprintf(p, sizeof p, "%s\\System32\\kernel32.dll", win);
    HANDLE h = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE); if (h == INVALID_HANDLE_VALUE) return;
    ULONG fl = 0xAA, lvl = 0xBB, tpsz = 64, alg = 0xCC; UCHAR tp[64];
    memset(tp, 0xEE, sizeof tp);
    SET_ERR(0xBAD);
    BOOL r = f(h, &fl, &lvl, tp, &tpsz, &alg);
    t_note("kernel32.dll: ret=%d err=%lu flags=0x%lx level=%lu thumbprint size=%lu alg=0x%lx", r, GetLastError(), fl, lvl, tpsz, alg);
    if (r) CHECK(lvl <= 0xF);
    CloseHandle(h);
}

TEST(gap4_signinglevel_on_unsigned_temp_file) {
    GetCachedSigningLevel_pfn f = UN(GetCachedSigningLevel);
    REQUIRE(f);
    if (t_is_wine()) { t_skip("not implemented on emulator"); return; }
    char p[MAX_PATH];
    mkfile("unsigned.dll", "MZ not really", 13, p, sizeof p);
    HANDLE h = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) {
        ULONG fl = 0xAA, lvl = 0xBB, tpsz = 64, alg = 0xCC; UCHAR tp[64];
        SET_ERR(0xBAD);
        BOOL r = f(h, &fl, &lvl, tp, &tpsz, &alg);
        t_note("unsigned temp file: ret=%d err=%lu flags=0x%lx level=%lu", r, GetLastError(), fl, lvl);
        if (r) EXPECT_EQ(lvl, 0);
        CloseHandle(h);
    }
    DeleteFileA(p);
}

TEST(gap4_signinglevel_forwarded) {
    GetCachedSigningLevel_pfn f = (GetCachedSigningLevel_pfn)t_proc_k32("GetCachedSigningLevel");
    GetCachedSigningLevel_pfn g = (GetCachedSigningLevel_pfn)t_proc_kb("GetCachedSigningLevel");
    t_note("GetCachedSigningLevel kernel32=%p kernelbase=%p", f, g);
    REQUIRE(f || g);
    CHECK(f != NULL);
}

/* ================= GetAppContainerNamedObjectPath ================= */

UDECL(BOOL, GetAppContainerNamedObjectPath, (HANDLE, PSID, ULONG, LPWSTR, PULONG));

static const BYTE SID_APPC[8 + 8 * 4] = { 1, 8, 0, 0, 0, 0, 0, 15,
    2, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 3, 0, 0, 0, 4, 0, 0, 0, 5, 0, 0, 0, 6, 0, 0, 0, 7, 0, 0, 0 };
#define APPC_STR L"S-1-15-2-1-2-3-4-5-6-7"

TEST(gap4_acnp_null_return_length) {
    GetAppContainerNamedObjectPath_pfn f = UN(GetAppContainerNamedObjectPath);
    REQUIRE(f); REQUIRE(UNT(RtlNtStatusToDosError));
    WCHAR b[512];
    SET_ERR(0xBAD);
    EXPECT_EQ(f(NULL, (PSID)SID_APPC, 512, b, NULL), 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0xBAD);
    EXPECT_EQ(f(NULL, (PSID)SID_APPC, 0, NULL, NULL), 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}

TEST(gap4_acnp_null_buffer_nonzero_length) {
    GetAppContainerNamedObjectPath_pfn f = UN(GetAppContainerNamedObjectPath);
    REQUIRE(f); REQUIRE(UNT(RtlNtStatusToDosError));
    ULONG need = 0x1234;
    SET_ERR(0xBAD);
    EXPECT_EQ(f(NULL, (PSID)SID_APPC, 16, NULL, &need), 0);
    EXPECT_ERR(n2d((LONG)0xC0000030));
    EXPECT_EQ(need, 0x1234);
}

TEST(gap4_acnp_length_query_with_sid) {
    GetAppContainerNamedObjectPath_pfn f = UN(GetAppContainerNamedObjectPath);
    REQUIRE(f);
    if (t_is_wine()) { t_skip("not implemented on emulator"); return; }
    ULONG need = 0x1234; WCHAR dummy[2] = { 0x7777, 0x7777 };
    SET_ERR(0xBAD);
    BOOL r = f(NULL, (PSID)SID_APPC, 0, NULL, &need);
    t_note("length query -> %d err=%lu need=%lu", r, GetLastError(), need);
    if (need != 0x1234) {
        EXPECT_EQ(r, 0);
        EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
        CHECK(need > 22);
    }
    need = 0x1234;
    SET_ERR(0xBAD);
    r = f(NULL, (PSID)SID_APPC, 0, dummy, &need);
    if (need != 0x1234) { EXPECT_EQ(r, 0); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER); }
    EXPECT_EQ(dummy[0], 0x7777);
}

TEST(gap4_acnp_fetch_with_sid) {
    GetAppContainerNamedObjectPath_pfn f = UN(GetAppContainerNamedObjectPath);
    REQUIRE(f); REQUIRE(UNT(RtlNtStatusToDosError));
    if (t_is_wine()) { t_skip("not implemented on emulator"); return; }
    ULONG need = 0;
    f(NULL, (PSID)SID_APPC, 0, NULL, &need);
    if (!need) { t_note("could not obtain length"); return; }
    WCHAR *b = calloc(need + 8, sizeof(WCHAR));
    for (ULONG i = 0; i < need + 8; i++) b[i] = 0x7777;
    ULONG got = 0;
    SET_ERR(0xBAD);
    BOOL r = f(NULL, (PSID)SID_APPC, need + 1, b, &got);
    CHECKF(r, "err=%lu", GetLastError());
    if (r) {
        EXPECT_EQ(got, need);
        EXPECT_EQ(lstrlenW(b), need - 1); /* *needed counts the terminating NUL */
        CHECK(wcsstr(b, APPC_STR) != NULL);
        CHECK(wcsstr(b, L"AppContainerNamedObjects") != NULL);
        EXPECT_EQ(b[need + 1], 0x7777);
        t_note("path: %ls", b);
    }
    /* buffer of exactly *needed characters */
    for (ULONG i = 0; i < need + 8; i++) b[i] = 0x7777;
    got = 0;
    SET_ERR(0xBAD);
    r = f(NULL, (PSID)SID_APPC, need, b, &got);
    EXPECT_EQ(r, 1); /* need already includes the NUL, so this buffer fits */
    EXPECT_EQ(got, need);
    /* one char fewer than *needed: too small */
    SET_ERR(0xBAD);
    r = f(NULL, (PSID)SID_APPC, need - 1, b, &got);
    EXPECT_EQ(r, 0);
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    free(b);
}

TEST(gap4_acnp_bad_token_handles) {
    GetAppContainerNamedObjectPath_pfn f = UN(GetAppContainerNamedObjectPath);
    REQUIRE(f); REQUIRE(UNT(RtlNtStatusToDosError));
    if (t_is_wine()) { t_skip("not implemented on emulator"); return; }
    WCHAR b[512]; ULONG need = 0x1234;
    SET_ERR(0xBAD);
    EXPECT_EQ(f((HANDLE)(ULONG_PTR)0x1234, NULL, 512, b, &need), 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    EXPECT_EQ(need, 0x1234);
    SET_ERR(0xBAD);
    EXPECT_EQ(f(INVALID_HANDLE_VALUE, NULL, 512, b, &need), 0);
    t_note("INVALID_HANDLE_VALUE token -> err=%lu", GetLastError());
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0xBAD);
    EXPECT_EQ(f(ev, NULL, 512, b, &need), 0);
    EXPECT_ERR(n2d((LONG)0xC0000024));
    CloseHandle(ev);
}

TEST(gap4_acnp_process_token_not_appcontainer) {
    GetAppContainerNamedObjectPath_pfn f = UN(GetAppContainerNamedObjectPath);
    REQUIRE(f); REQUIRE(UNT(RtlNtStatusToDosError));
    if (t_is_wine()) { t_skip("not implemented on emulator"); return; }
    HANDLE tok = NULL;
    CHECK(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok));
    DWORD isac = 0, rl = 0;
    BOOL have = GetTokenInformation(tok, (TOKEN_INFORMATION_CLASS)29, &isac, sizeof isac, &rl);
    WCHAR b[512]; ULONG need = 0x1234;
    SET_ERR(0xBAD);
    BOOL r = f(tok, NULL, 512, b, &need);
    t_note("process token (IsAppContainer=%lu) -> %d err=%lu", isac, r, GetLastError());
    if (have && !isac) {
        EXPECT_EQ(r, 0);
        t_note("non-appcontainer token: err=%lu (STATUS_NOT_APPCONTAINER maps to %lu; unverified)", GetLastError(), n2d((LONG)0xC000A200));
        EXPECT_EQ(need, 0x1234);
    }
    CloseHandle(tok);
}

TEST(gap4_acnp_no_token_no_sid) {
    GetAppContainerNamedObjectPath_pfn f = UN(GetAppContainerNamedObjectPath);
    REQUIRE(f);
    if (t_is_wine()) { t_skip("not implemented on emulator"); return; }
    WCHAR b[512]; ULONG need = 0x1234;
    SET_ERR(0xBAD);
    BOOL r = f(NULL, NULL, 512, b, &need);
    t_note("NULL,NULL -> %d err=%lu need=%lu", r, GetLastError(), need);
}

TEST(gap4_acnp_token_and_sid_together) {
    GetAppContainerNamedObjectPath_pfn f = UN(GetAppContainerNamedObjectPath);
    REQUIRE(f);
    if (t_is_wine()) { t_skip("not implemented on emulator"); return; }
    HANDLE tok = NULL; WCHAR b[512]; ULONG need = 0x1234;
    CHECK(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok));
    SET_ERR(0xBAD);
    BOOL r = f(tok, (PSID)SID_APPC, 512, b, &need);
    t_note("token + sid -> %d err=%lu need=%lu", r, GetLastError(), need);
    EXPECT_EQ(r, 0);
    CloseHandle(tok);
}

TEST(gap4_acnp_non_appcontainer_sid) {
    GetAppContainerNamedObjectPath_pfn f = UN(GetAppContainerNamedObjectPath);
    REQUIRE(f);
    if (t_is_wine()) { t_skip("not implemented on emulator"); return; }
    static const BYTE world[12] = { 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0 };
    WCHAR b[512]; ULONG need = 0x1234;
    SET_ERR(0xBAD);
    BOOL r = f(NULL, (PSID)world, 512, b, &need);
    t_note("S-1-1-0 as container sid -> %d err=%lu need=%lu", r, GetLastError(), need);
}

/* ================= GetDeviceDriver{Base,File}Name ================= */
UDECL(DWORD, GetDeviceDriverBaseNameA, (LPVOID, LPSTR, DWORD));
UDECL(DWORD, GetDeviceDriverBaseNameW, (LPVOID, LPWSTR, DWORD));
UDECL(DWORD, GetDeviceDriverFileNameA, (LPVOID, LPSTR, DWORD));
UDECL(DWORD, GetDeviceDriverFileNameW, (LPVOID, LPWSTR, DWORD));
UDECL(BOOL, EnumDeviceDrivers, (LPVOID *, DWORD, LPDWORD));

static LPVOID g_drv;
static int find_driver(void) {
    EnumDeviceDrivers_pfn en = UN(EnumDeviceDrivers);
    if (!en) return 0;
    LPVOID arr[1024]; DWORD need = 0;
    if (!en(arr, sizeof arr, &need) || need < sizeof(LPVOID)) return 0;
    g_drv = arr[0];
    return g_drv != NULL;
}

TEST(gap4_driver_basename_a_full) {
    REQUIRE(UN(GetDeviceDriverBaseNameA));
    if (!find_driver()) { t_skip("no driver list"); return; }
    char b[260]; memset(b, 0x55, sizeof b);
    SET_ERR(0xBAD);
    DWORD n = UN(GetDeviceDriverBaseNameA)(g_drv, b, sizeof b);
    CHECKF(n != 0, "err=%lu", GetLastError());
    EXPECT_EQ(n, strlen(b));
    CHECK(n > 3 && !strchr(b, '\\'));
    t_note("first driver %p base name \"%s\"", g_drv, b);
    EXPECT_EQ((unsigned char)b[n + 1], 0x55);
}

TEST(gap4_driver_first_is_kernel) {
    REQUIRE(UN(GetDeviceDriverBaseNameA));
    if (!find_driver()) { t_skip("no driver list"); return; }
    char b[260];
    CHECK(UN(GetDeviceDriverBaseNameA)(g_drv, b, sizeof b));
    CHECKF(!lstrcmpiA(b, "ntoskrnl.exe") || !lstrcmpiA(b, "ntkrnlmp.exe") || !lstrcmpiA(b, "ntkrnlpa.exe") || !lstrcmpiA(b, "ntkrpamp.exe"), "got %s", b);
}

TEST(gap4_driver_basename_truncation_a) {
    REQUIRE(UN(GetDeviceDriverBaseNameA));
    if (!find_driver()) { t_skip("no driver list"); return; }
    char full[260], b[260];
    DWORD L = UN(GetDeviceDriverBaseNameA)(g_drv, full, sizeof full);
    CHECK(L > 0); if (!L) return;
    /* nSize == L+1: complete with NUL */
    memset(b, 0x55, sizeof b);
    EXPECT_EQ(UN(GetDeviceDriverBaseNameA)(g_drv, b, L + 1), L);
    EXPECT_EQ(b[L], 0); CHECK(!strcmp(b, full));
    /* nSize == L: returns L, no terminator written */
    memset(b, 0x55, sizeof b);
    EXPECT_EQ(UN(GetDeviceDriverBaseNameA)(g_drv, b, L), L);
    CHECK(!memcmp(b, full, L)); EXPECT_EQ((unsigned char)b[L], 0x55);
    /* smaller */
    for (DWORD n = 1; n < L; n++) {
        memset(b, 0x55, sizeof b);
        DWORD r = UN(GetDeviceDriverBaseNameA)(g_drv, b, n);
        if (r != n || memcmp(b, full, n) || (unsigned char)b[n] != 0x55) { CHECKF(0, "nSize=%lu ret=%lu", n, r); break; }
    }
    /* larger */
    memset(b, 0x55, sizeof b);
    EXPECT_EQ(UN(GetDeviceDriverBaseNameA)(g_drv, b, 259), L);
    EXPECT_EQ((unsigned char)b[L + 1], 0x55);
}

TEST(gap4_driver_basename_size_zero) {
    REQUIRE(UN(GetDeviceDriverBaseNameA)); REQUIRE(UN(GetDeviceDriverBaseNameW));
    if (!find_driver()) { t_skip("no driver list"); return; }
    char b[8]; WCHAR w[8]; memset(b, 0x55, sizeof b);
    EXPECT_EQ(UN(GetDeviceDriverBaseNameA)(g_drv, b, 0), 0);
    EXPECT_EQ((unsigned char)b[0], 0x55);
    for (int i = 0; i < 8; i++) w[i] = 0x7777;
    EXPECT_EQ(UN(GetDeviceDriverBaseNameW)(g_drv, w, 0), 0);
    EXPECT_EQ(w[0], 0x7777);
}

TEST(gap4_driver_basename_w_matches_a) {
    REQUIRE(UN(GetDeviceDriverBaseNameA)); REQUIRE(UN(GetDeviceDriverBaseNameW));
    if (!find_driver()) { t_skip("no driver list"); return; }
    char a[260]; WCHAR w[260], w2[260];
    DWORD na = UN(GetDeviceDriverBaseNameA)(g_drv, a, sizeof a);
    for (int i = 0; i < 260; i++) w[i] = 0x7777;
    SET_ERR(0xBAD);
    DWORD nw = UN(GetDeviceDriverBaseNameW)(g_drv, w, 260);
    CHECKF(nw != 0, "err=%lu", GetLastError());
    EXPECT_EQ(na, nw);
    MultiByteToWideChar(CP_ACP, 0, a, -1, w2, 260);
    CHECK(!lstrcmpW(w, w2));
    EXPECT_EQ(w[nw + 1], 0x7777);
}

TEST(gap4_driver_basename_w_truncation) {
    REQUIRE(UN(GetDeviceDriverBaseNameW));
    if (!find_driver()) { t_skip("no driver list"); return; }
    WCHAR full[260], w[260];
    DWORD L = UN(GetDeviceDriverBaseNameW)(g_drv, full, 260);
    CHECK(L > 0); if (!L) return;
    for (int i = 0; i < 260; i++) w[i] = 0x7777;
    EXPECT_EQ(UN(GetDeviceDriverBaseNameW)(g_drv, w, L + 1), L);
    EXPECT_EQ(w[L], 0);
    for (int i = 0; i < 260; i++) w[i] = 0x7777;
    DWORD r = UN(GetDeviceDriverBaseNameW)(g_drv, w, L);
    EXPECT_EQ(r, L);
    EXPECT_EQ(w[L], 0x7777);
    CHECK(!memcmp(w, full, L * 2));
    for (int i = 0; i < 260; i++) w[i] = 0x7777;
    r = UN(GetDeviceDriverBaseNameW)(g_drv, w, 3);
    EXPECT_EQ(r, 3);
    CHECK(!memcmp(w, full, 6)); EXPECT_EQ(w[3], 0x7777);
}

TEST(gap4_driver_filename_a_full) {
    REQUIRE(UN(GetDeviceDriverFileNameA)); REQUIRE(UN(GetDeviceDriverBaseNameA));
    if (!find_driver()) { t_skip("no driver list"); return; }
    char f[300], b[260];
    memset(f, 0x55, sizeof f);
    SET_ERR(0xBAD);
    DWORD n = UN(GetDeviceDriverFileNameA)(g_drv, f, sizeof f);
    CHECKF(n != 0, "err=%lu", GetLastError());
    EXPECT_EQ(n, strlen(f));
    t_note("file name \"%s\"", f);
    DWORD nb = UN(GetDeviceDriverBaseNameA)(g_drv, b, sizeof b);
    CHECK(n >= nb);
    CHECK(n >= nb && !lstrcmpiA(f + n - nb, b));
    EXPECT_EQ((unsigned char)f[n + 1], 0x55);
}

TEST(gap4_driver_filename_truncation) {
    REQUIRE(UN(GetDeviceDriverFileNameA));
    if (!find_driver()) { t_skip("no driver list"); return; }
    char full[300], b[300];
    DWORD L = UN(GetDeviceDriverFileNameA)(g_drv, full, sizeof full);
    CHECK(L > 0); if (!L) return;
    memset(b, 0x55, sizeof b);
    EXPECT_EQ(UN(GetDeviceDriverFileNameA)(g_drv, b, L + 1), L);
    EXPECT_EQ(b[L], 0);
    memset(b, 0x55, sizeof b);
    EXPECT_EQ(UN(GetDeviceDriverFileNameA)(g_drv, b, L), L);
    EXPECT_EQ((unsigned char)b[L], 0x55);
    for (DWORD n = 1; n < L; n += 5) {
        memset(b, 0x55, sizeof b);
        DWORD r = UN(GetDeviceDriverFileNameA)(g_drv, b, n);
        if (r != n || memcmp(b, full, n) || (unsigned char)b[n] != 0x55) { CHECKF(0, "nSize=%lu ret=%lu", n, r); break; }
    }
    memset(b, 0x55, sizeof b);
    EXPECT_EQ(UN(GetDeviceDriverFileNameA)(g_drv, b, 0), 0);
    EXPECT_EQ((unsigned char)b[0], 0x55);
}

TEST(gap4_driver_filename_w_matches_a) {
    REQUIRE(UN(GetDeviceDriverFileNameA)); REQUIRE(UN(GetDeviceDriverFileNameW));
    if (!find_driver()) { t_skip("no driver list"); return; }
    char a[300]; WCHAR w[300], w2[300];
    DWORD na = UN(GetDeviceDriverFileNameA)(g_drv, a, sizeof a);
    for (int i = 0; i < 300; i++) w[i] = 0x7777;
    SET_ERR(0xBAD);
    DWORD nw = UN(GetDeviceDriverFileNameW)(g_drv, w, 300);
    CHECKF(nw != 0, "err=%lu", GetLastError());
    EXPECT_EQ(na, nw);
    MultiByteToWideChar(CP_ACP, 0, a, -1, w2, 300);
    CHECK(!lstrcmpW(w, w2));
    for (int i = 0; i < 300; i++) w[i] = 0x7777;
    EXPECT_EQ(UN(GetDeviceDriverFileNameW)(g_drv, w, 0), 0);
    EXPECT_EQ(w[0], 0x7777);
    EXPECT_EQ(UN(GetDeviceDriverFileNameW)(g_drv, w, na), na);
    EXPECT_EQ(w[na], 0x7777);
}

TEST(gap4_driver_invalid_bases) {
    REQUIRE(UN(GetDeviceDriverBaseNameA)); REQUIRE(UN(GetDeviceDriverFileNameA));
    REQUIRE(UN(GetDeviceDriverBaseNameW)); REQUIRE(UN(GetDeviceDriverFileNameW));
    char a[64]; WCHAR w[64];
    LPVOID bad[] = { NULL, (LPVOID)1, (LPVOID)0x1000, (LPVOID)0x7FFE0000, (LPVOID)(ULONG_PTR)-1 };
    for (int i = 0; i < 5; i++) {
        memset(a, 0x55, sizeof a); for (int k = 0; k < 64; k++) w[k] = 0x7777;
        SET_ERR(0);
        EXPECT_EQ(UN(GetDeviceDriverBaseNameA)(bad[i], a, 64), 0);
        t_note("BaseNameA(%p) err=%lu", bad[i], GetLastError());
        EXPECT_EQ(UN(GetDeviceDriverFileNameA)(bad[i], a, 64), 0);
        EXPECT_EQ(UN(GetDeviceDriverBaseNameW)(bad[i], w, 64), 0);
        EXPECT_EQ(UN(GetDeviceDriverFileNameW)(bad[i], w, 64), 0);
        if (!t_is_wine()) {   /* failure path never touches the caller's buffer */
            EXPECT_EQ((unsigned char)a[0], 0x55);
            EXPECT_EQ(w[0], 0x7777);
        }
    }
}

TEST(gap4_driver_interior_pointer_is_not_a_base) {
    REQUIRE(UN(GetDeviceDriverBaseNameA));
    if (!find_driver()) { t_skip("no driver list"); return; }
    char b[64];
    EXPECT_EQ(UN(GetDeviceDriverBaseNameA)((BYTE *)g_drv + 0x10, b, 64), 0);
}

TEST(gap4_driver_psapi_k32_aliases) {
    if (!find_driver()) { t_skip("no driver list"); return; }
    typedef DWORD (WINAPI *fn)(LPVOID, LPSTR, DWORD);
    fn a = (fn)t_proc("K32GetDeviceDriverBaseNameA");
    fn f = (fn)t_proc("K32GetDeviceDriverFileNameA");
    char b1[260], b2[260];
    if (!a || !f) { t_skip("no K32 aliases"); return; }
    REQUIRE(UN(GetDeviceDriverBaseNameA)); REQUIRE(UN(GetDeviceDriverFileNameA));
    DWORD n1 = a(g_drv, b1, 260), n2 = UN(GetDeviceDriverBaseNameA)(g_drv, b2, 260);
    EXPECT_EQ(n1, n2); CHECK(!strcmp(b1, b2));
    n1 = f(g_drv, b1, 260); n2 = UN(GetDeviceDriverFileNameA)(g_drv, b2, 260);
    EXPECT_EQ(n1, n2); CHECK(!strcmp(b1, b2));
}

TEST(gap4_driver_all_enumerated_have_names) {
    EnumDeviceDrivers_pfn en = UN(EnumDeviceDrivers);
    REQUIRE(en); REQUIRE(UN(GetDeviceDriverBaseNameA)); REQUIRE(UN(GetDeviceDriverFileNameA));
    LPVOID arr[1024]; DWORD need = 0;
    if (!en(arr, sizeof arr, &need) || !need) { t_skip("no drivers"); return; }
    DWORD cnt = need / sizeof(LPVOID); if (cnt > 1024) cnt = 1024;
    int bad = 0;
    for (DWORD i = 0; i < cnt && i < 400; i++) {
        char b[260], f[300];
        if (!arr[i]) continue;
        DWORD nb = UN(GetDeviceDriverBaseNameA)(arr[i], b, sizeof b), nf = UN(GetDeviceDriverFileNameA)(arr[i], f, sizeof f);
        if (!nb || !nf || nb != strlen(b) || nf != strlen(f) || nf < nb) bad++;
    }
    CHECKF(bad == 0, "%d drivers with inconsistent names of %lu", bad, cnt);
}
