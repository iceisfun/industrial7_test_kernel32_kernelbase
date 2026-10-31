/* Windows 8/10 file additions: CreateFile2, CopyFile2, DeviceIoControl error paths. */
#include "t.h"
#include <winioctl.h>

typedef struct { DWORD dwSize, dwCopyFlags; BOOL *pfCancel; void *pProgress; void *pvCtx; } K_COPYFILE2_PARAMS;
typedef struct { DWORD Type, Pad; BYTE rest[256]; } K_COPYFILE2_MSG;
typedef DWORD (CALLBACK *K_PROGRESS)(const K_COPYFILE2_MSG *, void *);
#define K_HR_INVALIDARG 0x80070057UL
#define K_HR(e) (0x80070000UL | (e))

static void kf_write(const char *path, const void *data, DWORD len) {
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD w; WriteFile(h, data, len, &w, NULL);
    CloseHandle(h);
}
static DWORD kf_size(const char *path) {
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0xFFFFFFFF;
    DWORD s = GetFileSize(h, NULL);
    CloseHandle(h);
    return s;
}
static void kf_wpath(WCHAR *out, size_t cap, const char *leaf) {
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, leaf);
    MultiByteToWideChar(CP_ACP, 0, p, -1, out, (int)cap);
}
static void kf_cleanup(const char *leaf) { char p[MAX_PATH]; t_tmpfile(p, sizeof p, leaf); SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL); DeleteFileA(p); }

/* ======================= CreateFile2 ======================= */
TEST(cf2_basic_create_write_read) {
    REQUIRE(U(CreateFile2));
    WCHAR w[MAX_PATH]; kf_wpath(w, MAX_PATH, "cf2_basic.bin");
    SET_ERR(0xdead);
    HANDLE h = U(CreateFile2)(w, GENERIC_READ | GENERIC_WRITE, 0, CREATE_NEW, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    EXPECT_EQ(GetLastError(), 0);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD n; CHECK(WriteFile(h, "abcdef", 6, &n, NULL) && n == 6);
    SetFilePointer(h, 0, NULL, FILE_BEGIN);
    char b[8] = {0}; CHECK(ReadFile(h, b, 6, &n, NULL) && n == 6);
    EXPECT_STR(b, "abcdef");
    CloseHandle(h);
    kf_cleanup("cf2_basic.bin");
}
TEST(cf2_dispositions_table) {
    REQUIRE(U(CreateFile2));
    WCHAR w[MAX_PATH]; kf_wpath(w, MAX_PATH, "cf2_disp.bin");
    char a[MAX_PATH]; t_tmpfile(a, sizeof a, "cf2_disp.bin");
    DeleteFileA(a);
    HANDLE h;
    /* OPEN_EXISTING on missing */
    h = U(CreateFile2)(w, GENERIC_READ, 0, OPEN_EXISTING, NULL);
    CHECK(h == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    /* TRUNCATE_EXISTING on missing */
    h = U(CreateFile2)(w, GENERIC_WRITE, 0, TRUNCATE_EXISTING, NULL);
    CHECK(h == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    /* OPEN_ALWAYS on missing: created, last error 0 */
    SET_ERR(0xdead);
    h = U(CreateFile2)(w, GENERIC_WRITE, 0, OPEN_ALWAYS, NULL);
    CHECK(h != INVALID_HANDLE_VALUE); EXPECT_EQ(GetLastError(), 0);
    if (h != INVALID_HANDLE_VALUE) { DWORD n; WriteFile(h, "12345", 5, &n, NULL); CloseHandle(h); }
    EXPECT_EQ(kf_size(a), 5);
    /* OPEN_ALWAYS on existing: ERROR_ALREADY_EXISTS but success */
    h = U(CreateFile2)(w, GENERIC_READ, FILE_SHARE_READ, OPEN_ALWAYS, NULL);
    CHECK(h != INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_ALREADY_EXISTS);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    EXPECT_EQ(kf_size(a), 5);
    /* CREATE_NEW on existing */
    h = U(CreateFile2)(w, GENERIC_WRITE, 0, CREATE_NEW, NULL);
    CHECK(h == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_FILE_EXISTS);
    /* CREATE_ALWAYS on existing: truncates, ERROR_ALREADY_EXISTS */
    h = U(CreateFile2)(w, GENERIC_WRITE, 0, CREATE_ALWAYS, NULL);
    CHECK(h != INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_ALREADY_EXISTS);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    EXPECT_EQ(kf_size(a), 0);
    kf_write(a, "xyz", 3);
    /* TRUNCATE_EXISTING with write access */
    h = U(CreateFile2)(w, GENERIC_WRITE, 0, TRUNCATE_EXISTING, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    EXPECT_EQ(kf_size(a), 0);
    kf_cleanup("cf2_disp.bin");
}
TEST(cf2_truncate_requires_write) {
    REQUIRE(U(CreateFile2));
    WCHAR w[MAX_PATH]; kf_wpath(w, MAX_PATH, "cf2_trunc.bin");
    char a[MAX_PATH]; t_tmpfile(a, sizeof a, "cf2_trunc.bin");
    kf_write(a, "keepme", 6);
    HANDLE h = U(CreateFile2)(w, GENERIC_READ, FILE_SHARE_READ, TRUNCATE_EXISTING, NULL);
    CHECK(h == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    h = U(CreateFile2)(w, 0, FILE_SHARE_READ, TRUNCATE_EXISTING, NULL);
    CHECK(h == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    EXPECT_EQ(kf_size(a), 6);
    kf_cleanup("cf2_trunc.bin");
}
TEST(cf2_invalid_dispositions) {
    REQUIRE(U(CreateFile2));
    WCHAR w[MAX_PATH]; kf_wpath(w, MAX_PATH, "cf2_baddisp.bin");
    char a[MAX_PATH]; t_tmpfile(a, sizeof a, "cf2_baddisp.bin");
    static const DWORD bad[] = { 0, 6, 7, 8, 100, 0x80000000, 0xFFFFFFFF, 0x10000001 };
    for (size_t i = 0; i < ARRAYSIZE(bad); i++) {
        SET_ERR(0);
        HANDLE h = U(CreateFile2)(w, GENERIC_WRITE, 0, bad[i], NULL);
        CHECKF(h == INVALID_HANDLE_VALUE, "disp 0x%lx", bad[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }
    EXPECT_EQ(GetFileAttributesA(a), INVALID_FILE_ATTRIBUTES);
}
TEST(cf2_extended_params_dwsize) {
    REQUIRE(U(CreateFile2));
    WCHAR w[MAX_PATH]; kf_wpath(w, MAX_PATH, "cf2_dwsize.bin");
    char a[MAX_PATH]; t_tmpfile(a, sizeof a, "cf2_dwsize.bin");
    static const DWORD bad[] = { 0, 1, 4, 8, 0x10, 0x1c, 0x1f };
    for (size_t i = 0; i < ARRAYSIZE(bad); i++) {
        CREATEFILE2_EXTENDED_PARAMETERS ep; memset(&ep, 0, sizeof ep);
        ep.dwSize = bad[i];
        ep.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
        SET_ERR(0);
        HANDLE h = U(CreateFile2)(w, GENERIC_WRITE, 0, CREATE_ALWAYS, &ep);
        CHECKF(h == INVALID_HANDLE_VALUE, "dwSize %lu accepted", bad[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }
    EXPECT_EQ(GetFileAttributesA(a), INVALID_FILE_ATTRIBUTES);
    static const DWORD good[] = { 0x20, sizeof(CREATEFILE2_EXTENDED_PARAMETERS), 0x30, 0x100 };
    for (size_t i = 0; i < ARRAYSIZE(good); i++) {
        CREATEFILE2_EXTENDED_PARAMETERS ep; memset(&ep, 0, sizeof ep);
        ep.dwSize = good[i];
        ep.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
        HANDLE h = U(CreateFile2)(w, GENERIC_WRITE, 0, CREATE_ALWAYS, &ep);
        CHECKF(h != INVALID_HANDLE_VALUE, "dwSize %lu rejected (err %lu)", good[i], GetLastError());
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }
    kf_cleanup("cf2_dwsize.bin");
}
TEST(cf2_names_invalid) {
    REQUIRE(U(CreateFile2));
    HANDLE h = U(CreateFile2)(NULL, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, NULL);
    CHECK(h == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    h = U(CreateFile2)(L"", GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, NULL);
    CHECK(h == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    h = U(CreateFile2)(L"Z:\\k32t_no_such_dir_\\x\\file.bin", GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, NULL);
    CHECK(h == INVALID_HANDLE_VALUE);
    WCHAR w[MAX_PATH + 40]; kf_wpath(w, MAX_PATH, "nodir_cf2\\file.bin");
    h = U(CreateFile2)(w, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, NULL);
    CHECK(h == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    h = U(CreateFile2)(w, GENERIC_WRITE, 0, CREATE_ALWAYS, NULL);
    CHECK(h == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    kf_wpath(w, MAX_PATH, "missing_cf2.bin");
    h = U(CreateFile2)(w, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, NULL);
    CHECK(h == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(cf2_illegal_chars) {
    REQUIRE(U(CreateFile2));
    static const WCHAR *bad[] = { L"?", L"a*b", L"a<b", L"a>b", L"a|b", L"a\"b" };
    for (size_t i = 0; i < ARRAYSIZE(bad); i++) {
        WCHAR w[MAX_PATH]; char tmp[MAX_PATH]; t_tmpfile(tmp, sizeof tmp, "");
        MultiByteToWideChar(CP_ACP, 0, tmp, -1, w, MAX_PATH);
        wcscat(w, bad[i]);
        HANDLE h = U(CreateFile2)(w, GENERIC_WRITE, 0, CREATE_ALWAYS, NULL);
        CHECKF(h == INVALID_HANDLE_VALUE, "case %zu", i);
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }
}
TEST(cf2_sharing_violation) {
    REQUIRE(U(CreateFile2));
    WCHAR w[MAX_PATH]; kf_wpath(w, MAX_PATH, "cf2_share.bin");
    HANDLE h1 = U(CreateFile2)(w, GENERIC_READ | GENERIC_WRITE, 0, CREATE_ALWAYS, NULL);
    CHECK(h1 != INVALID_HANDLE_VALUE);
    HANDLE h2 = U(CreateFile2)(w, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, NULL);
    CHECK(h2 == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_SHARING_VIOLATION);
    if (h2 != INVALID_HANDLE_VALUE) CloseHandle(h2);
    if (h1 != INVALID_HANDLE_VALUE) CloseHandle(h1);
    h1 = U(CreateFile2)(w, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, NULL);
    CHECK(h1 != INVALID_HANDLE_VALUE);
    h2 = U(CreateFile2)(w, GENERIC_WRITE, FILE_SHARE_READ, OPEN_EXISTING, NULL);
    CHECK(h2 == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_SHARING_VIOLATION);
    if (h2 != INVALID_HANDLE_VALUE) CloseHandle(h2);
    h2 = U(CreateFile2)(w, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, NULL);
    CHECK(h2 != INVALID_HANDLE_VALUE);
    if (h2 != INVALID_HANDLE_VALUE) CloseHandle(h2);
    if (h1 != INVALID_HANDLE_VALUE) CloseHandle(h1);
    kf_cleanup("cf2_share.bin");
}
TEST(cf2_directory_open) {
    REQUIRE(U(CreateFile2));
    WCHAR w[MAX_PATH]; char dir[MAX_PATH]; t_tmpfile(dir, sizeof dir, "cf2_dir");
    CreateDirectoryA(dir, NULL);
    kf_wpath(w, MAX_PATH, "cf2_dir");
    HANDLE h = U(CreateFile2)(w, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, NULL);
    CHECK(h == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CREATEFILE2_EXTENDED_PARAMETERS ep; memset(&ep, 0, sizeof ep);
    ep.dwSize = sizeof ep; ep.dwFileFlags = FILE_FLAG_BACKUP_SEMANTICS;
    h = U(CreateFile2)(w, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, &ep);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) {
        BY_HANDLE_FILE_INFORMATION bi;
        CHECK(GetFileInformationByHandle(h, &bi));
        CHECK(bi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
        CloseHandle(h);
    }
    /* writing to a directory name */
    h = U(CreateFile2)(w, GENERIC_WRITE, 0, CREATE_ALWAYS, NULL);
    CHECK(h == INVALID_HANDLE_VALUE);
    RemoveDirectoryA(dir);
}
TEST(cf2_attributes_readonly) {
    REQUIRE(U(CreateFile2));
    WCHAR w[MAX_PATH]; kf_wpath(w, MAX_PATH, "cf2_ro.bin");
    char a[MAX_PATH]; t_tmpfile(a, sizeof a, "cf2_ro.bin");
    CREATEFILE2_EXTENDED_PARAMETERS ep; memset(&ep, 0, sizeof ep);
    ep.dwSize = sizeof ep; ep.dwFileAttributes = FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN;
    HANDLE h = U(CreateFile2)(w, GENERIC_READ | GENERIC_WRITE, 0, CREATE_NEW, &ep);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) { DWORD n; CHECK(WriteFile(h, "q", 1, &n, NULL)); CloseHandle(h); }
    DWORD at = GetFileAttributesA(a);
    CHECK(at != INVALID_FILE_ATTRIBUTES);
    CHECK(at & FILE_ATTRIBUTE_READONLY);
    CHECK(at & FILE_ATTRIBUTE_HIDDEN);
    h = U(CreateFile2)(w, GENERIC_WRITE, 0, OPEN_EXISTING, NULL);
    CHECK(h == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    h = U(CreateFile2)(w, GENERIC_WRITE, 0, CREATE_ALWAYS, NULL);
    CHECK(h == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    h = U(CreateFile2)(w, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    kf_cleanup("cf2_ro.bin");
}
TEST(cf2_delete_on_close) {
    REQUIRE(U(CreateFile2));
    WCHAR w[MAX_PATH]; kf_wpath(w, MAX_PATH, "cf2_doc.bin");
    char a[MAX_PATH]; t_tmpfile(a, sizeof a, "cf2_doc.bin");
    CREATEFILE2_EXTENDED_PARAMETERS ep; memset(&ep, 0, sizeof ep);
    ep.dwSize = sizeof ep; ep.dwFileFlags = FILE_FLAG_DELETE_ON_CLOSE;
    HANDLE h = U(CreateFile2)(w, GENERIC_READ | GENERIC_WRITE | DELETE, FILE_SHARE_DELETE | FILE_SHARE_READ, CREATE_NEW, &ep);
    CHECK(h != INVALID_HANDLE_VALUE);
    CHECK(GetFileAttributesA(a) != INVALID_FILE_ATTRIBUTES);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    EXPECT_EQ(GetFileAttributesA(a), INVALID_FILE_ATTRIBUTES);
    kf_cleanup("cf2_doc.bin");
}
TEST(cf2_inheritable_handle) {
    REQUIRE(U(CreateFile2));
    WCHAR w[MAX_PATH]; kf_wpath(w, MAX_PATH, "cf2_inh.bin");
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    CREATEFILE2_EXTENDED_PARAMETERS ep; memset(&ep, 0, sizeof ep);
    ep.dwSize = sizeof ep; ep.lpSecurityAttributes = &sa;
    HANDLE h = U(CreateFile2)(w, GENERIC_WRITE, 0, CREATE_ALWAYS, &ep);
    CHECK(h != INVALID_HANDLE_VALUE);
    DWORD fl = 0;
    if (h != INVALID_HANDLE_VALUE) { CHECK(GetHandleInformation(h, &fl)); CHECK(fl & HANDLE_FLAG_INHERIT); CloseHandle(h); }
    h = U(CreateFile2)(w, GENERIC_WRITE, 0, CREATE_ALWAYS, NULL);
    if (h != INVALID_HANDLE_VALUE) { CHECK(GetHandleInformation(h, &fl)); CHECK(!(fl & HANDLE_FLAG_INHERIT)); CloseHandle(h); }
    kf_cleanup("cf2_inh.bin");
}
TEST(cf2_overlapped_flag) {
    REQUIRE(U(CreateFile2));
    WCHAR w[MAX_PATH]; kf_wpath(w, MAX_PATH, "cf2_ovl.bin");
    CREATEFILE2_EXTENDED_PARAMETERS ep; memset(&ep, 0, sizeof ep);
    ep.dwSize = sizeof ep; ep.dwFileFlags = FILE_FLAG_OVERLAPPED;
    HANDLE h = U(CreateFile2)(w, GENERIC_READ | GENERIC_WRITE, 0, CREATE_ALWAYS, &ep);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    OVERLAPPED ov; memset(&ov, 0, sizeof ov);
    ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    BOOL r = WriteFile(h, "overlapped", 10, NULL, &ov);
    if (!r) { EXPECT_EQ(GetLastError(), ERROR_IO_PENDING); }
    DWORD n = 0;
    CHECK(GetOverlappedResult(h, &ov, &n, TRUE));
    EXPECT_EQ(n, 10);
    CloseHandle(ov.hEvent);
    CloseHandle(h);
    kf_cleanup("cf2_ovl.bin");
}
TEST(cf2_access_zero_and_attrs_query) {
    REQUIRE(U(CreateFile2));
    WCHAR w[MAX_PATH]; kf_wpath(w, MAX_PATH, "cf2_zero.bin");
    char a[MAX_PATH]; t_tmpfile(a, sizeof a, "cf2_zero.bin");
    kf_write(a, "zz", 2);
    HANDLE h = U(CreateFile2)(w, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, OPEN_EXISTING, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) {
        BY_HANDLE_FILE_INFORMATION bi;
        CHECK(GetFileInformationByHandle(h, &bi));
        EXPECT_EQ(bi.nFileSizeLow, 2);
        char b[2]; DWORD n;
        CHECK(!ReadFile(h, b, 2, &n, NULL));
        EXPECT_ERR(ERROR_ACCESS_DENIED);
        CloseHandle(h);
    }
    kf_cleanup("cf2_zero.bin");
}
TEST(cf2_generic_rights_mapping) {
    REQUIRE(U(CreateFile2));
    WCHAR w[MAX_PATH]; kf_wpath(w, MAX_PATH, "cf2_gen.bin");
    char a[MAX_PATH]; t_tmpfile(a, sizeof a, "cf2_gen.bin");
    kf_write(a, "data", 4);
    HANDLE h = U(CreateFile2)(w, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD n;
        CHECK(!WriteFile(h, "x", 1, &n, NULL));
        EXPECT_ERR(ERROR_ACCESS_DENIED);
        CloseHandle(h);
    }
    h = U(CreateFile2)(w, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) {
        char b[1]; DWORD n;
        CHECK(!ReadFile(h, b, 1, &n, NULL));
        EXPECT_ERR(ERROR_ACCESS_DENIED);
        CloseHandle(h);
    }
    kf_cleanup("cf2_gen.bin");
}
TEST(cf2_device_names) {
    REQUIRE(U(CreateFile2));
    HANDLE h = U(CreateFile2)(L"NUL", GENERIC_WRITE, FILE_SHARE_WRITE, OPEN_EXISTING, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) { DWORD n; CHECK(WriteFile(h, "x", 1, &n, NULL)); EXPECT_EQ(n, 1); EXPECT_EQ(GetFileType(h), FILE_TYPE_CHAR); CloseHandle(h); }
    h = U(CreateFile2)(L"\\\\.\\NUL", GENERIC_WRITE, FILE_SHARE_WRITE, OPEN_EXISTING, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    h = U(CreateFile2)(L"\\\\.\\PhysicalDrive0", 0, FILE_SHARE_READ | FILE_SHARE_WRITE, OPEN_EXISTING, NULL);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    else t_note("PhysicalDrive0 query-open failed: %lu", GetLastError());
}
TEST(cf2_longpath_prefix) {
    REQUIRE(U(CreateFile2));
    WCHAR dir[MAX_PATH + 400], file[MAX_PATH + 800];
    char tmp[MAX_PATH]; t_tmpfile(tmp, sizeof tmp, "");
    WCHAR wt[MAX_PATH]; MultiByteToWideChar(CP_ACP, 0, tmp, -1, wt, MAX_PATH);
    wcscpy(dir, L"\\\\?\\"); wcscat(dir, wt);
    size_t n = wcslen(dir);
    for (int i = 0; i < 200; i++) dir[n++] = L'd';
    dir[n] = 0;
    BOOL made = CreateDirectoryW(dir, NULL);
    CHECK(made);
    if (!made) return;
    wcscpy(file, dir); n = wcslen(file); file[n++] = L'\\';
    for (int i = 0; i < 100; i++) file[n++] = L'f';
    file[n] = 0;
    CHECK(wcslen(file) > MAX_PATH);
    HANDLE h = U(CreateFile2)(file, GENERIC_WRITE, 0, CREATE_NEW, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) { DWORD w; CHECK(WriteFile(h, "L", 1, &w, NULL)); CloseHandle(h); }
    /* the same path without the prefix is over MAX_PATH; a manifest-less exe is not long-path aware */
    WCHAR *plain = file + 4;
    HANDLE h2 = U(CreateFile2)(plain, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, NULL);
    if (h2 != INVALID_HANDLE_VALUE) { t_note("over-MAX_PATH path without prefix opened (long paths enabled)"); CloseHandle(h2); }
    else t_note("over-MAX_PATH path without prefix failed with %lu", GetLastError());
    DeleteFileW(file);
    RemoveDirectoryW(dir);
}
TEST(cf2_lasterror_on_success) {
    REQUIRE(U(CreateFile2));
    WCHAR w[MAX_PATH]; kf_wpath(w, MAX_PATH, "cf2_le.bin");
    SET_ERR(0x1234);
    HANDLE h = U(CreateFile2)(w, GENERIC_WRITE, 0, CREATE_NEW, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    EXPECT_EQ(GetLastError(), 0);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    kf_cleanup("cf2_le.bin");
}
TEST(cf2_matches_createfilew) {
    REQUIRE(U(CreateFile2));
    REQUIRE(U(CreateFileW));
    WCHAR w[MAX_PATH]; kf_wpath(w, MAX_PATH, "cf2_cmp.bin");
    char ap[MAX_PATH]; t_tmpfile(ap, sizeof ap, "cf2_cmp.bin");
    for (int pass = 0; pass < 2; pass++) {
        DeleteFileA(ap); if (pass) kf_write(ap, "x", 1);
        HANDLE a = U(CreateFileW)(w, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        DWORD ea = GetLastError();
        if (a != INVALID_HANDLE_VALUE) CloseHandle(a);
        DeleteFileA(ap); if (pass) kf_write(ap, "x", 1);
        HANDLE b = U(CreateFile2)(w, GENERIC_WRITE, 0, CREATE_ALWAYS, NULL);
        DWORD eb = GetLastError();
        if (b != INVALID_HANDLE_VALUE) CloseHandle(b);
        CHECK((a == INVALID_HANDLE_VALUE) == (b == INVALID_HANDLE_VALUE));
        EXPECT_EQ(ea, eb);
    }
    kf_cleanup("cf2_cmp.bin");
}

/* ======================= CopyFile2 ======================= */
static LONG g_cf2_calls, g_cf2_types[16];
static DWORD CALLBACK cf2_progress(const K_COPYFILE2_MSG *m, void *ctx) {
    LONG i = InterlockedIncrement(&g_cf2_calls) - 1;
    if (i < 16) g_cf2_types[i] = (LONG)m->Type;
    return ctx ? *(DWORD *)ctx : 0;   /* 0 = COPYFILE2_PROGRESS_CONTINUE */
}
TEST(cp2_basic) {
    REQUIRE(U(CopyFile2));
    WCHAR s[MAX_PATH], d[MAX_PATH]; kf_wpath(s, MAX_PATH, "cp2_src.bin"); kf_wpath(d, MAX_PATH, "cp2_dst.bin");
    char sa[MAX_PATH], da[MAX_PATH]; t_tmpfile(sa, sizeof sa, "cp2_src.bin"); t_tmpfile(da, sizeof da, "cp2_dst.bin");
    kf_write(sa, "copy-me-please", 14); DeleteFileA(da);
    HRESULT hr = U(CopyFile2)(s, d, NULL);
    EXPECT_EQ(hr, 0);
    EXPECT_EQ(kf_size(da), 14);
    char b[16] = {0}; HANDLE h = CreateFileA(da, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    DWORD n; if (h != INVALID_HANDLE_VALUE) { ReadFile(h, b, 14, &n, NULL); CloseHandle(h); }
    EXPECT_STR(b, "copy-me-please");
    kf_cleanup("cp2_src.bin"); kf_cleanup("cp2_dst.bin");
}
TEST(cp2_extended_param_validation) {
    REQUIRE(U(CopyFile2));
    WCHAR s[MAX_PATH], d[MAX_PATH]; kf_wpath(s, MAX_PATH, "cp2_v_src.bin"); kf_wpath(d, MAX_PATH, "cp2_v_dst.bin");
    char sa[MAX_PATH], da[MAX_PATH]; t_tmpfile(sa, sizeof sa, "cp2_v_src.bin"); t_tmpfile(da, sizeof da, "cp2_v_dst.bin");
    kf_write(sa, "v", 1); DeleteFileA(da);
    static const DWORD sizes[] = { 0, 1, 8, 0x1f, 0x21, 0x28, 0x40, 0xFFFFFFFF };
    for (size_t i = 0; i < ARRAYSIZE(sizes); i++) {
        K_COPYFILE2_PARAMS p; memset(&p, 0, sizeof p); p.dwSize = sizes[i];
        HRESULT hr = U(CopyFile2)(s, d, (COPYFILE2_EXTENDED_PARAMETERS *)&p);
        CHECKF((ULONG)hr == K_HR_INVALIDARG, "dwSize 0x%lx -> 0x%lx", sizes[i], (ULONG)hr);
    }
    EXPECT_EQ(GetFileAttributesA(da), INVALID_FILE_ATTRIBUTES);
    K_COPYFILE2_PARAMS p; memset(&p, 0, sizeof p); p.dwSize = 0x20;
    EXPECT_EQ(U(CopyFile2)(s, d, (COPYFILE2_EXTENDED_PARAMETERS *)&p), 0);
    EXPECT_EQ(kf_size(da), 1);
    kf_cleanup("cp2_v_src.bin"); kf_cleanup("cp2_v_dst.bin");
}
TEST(cp2_flag_validation) {
    REQUIRE(U(CopyFile2));
    WCHAR s[MAX_PATH], d[MAX_PATH]; kf_wpath(s, MAX_PATH, "cp2_f_src.bin"); kf_wpath(d, MAX_PATH, "cp2_f_dst.bin");
    char sa[MAX_PATH], da[MAX_PATH]; t_tmpfile(sa, sizeof sa, "cp2_f_src.bin"); t_tmpfile(da, sizeof da, "cp2_f_dst.bin");
    kf_write(sa, "f", 1); DeleteFileA(da);
    static const DWORD bad[] = { 0x80000000, 0x40000000, 0x20000000, 0xE0000000, 0x08000002, 0x08004000 };
    for (size_t i = 0; i < ARRAYSIZE(bad); i++) {
        K_COPYFILE2_PARAMS p; memset(&p, 0, sizeof p); p.dwSize = 0x20; p.dwCopyFlags = bad[i];
        HRESULT hr = U(CopyFile2)(s, d, (COPYFILE2_EXTENDED_PARAMETERS *)&p);
        CHECKF((ULONG)hr == K_HR_INVALIDARG, "flags 0x%lx -> 0x%lx", bad[i], (ULONG)hr);
    }
    EXPECT_EQ(GetFileAttributesA(da), INVALID_FILE_ATTRIBUTES);
    kf_cleanup("cp2_f_src.bin");
}
TEST(cp2_fail_if_exists) {
    REQUIRE(U(CopyFile2));
    WCHAR s[MAX_PATH], d[MAX_PATH]; kf_wpath(s, MAX_PATH, "cp2_e_src.bin"); kf_wpath(d, MAX_PATH, "cp2_e_dst.bin");
    char sa[MAX_PATH], da[MAX_PATH]; t_tmpfile(sa, sizeof sa, "cp2_e_src.bin"); t_tmpfile(da, sizeof da, "cp2_e_dst.bin");
    kf_write(sa, "NEW", 3); kf_write(da, "OLDOLD", 6);
    K_COPYFILE2_PARAMS p; memset(&p, 0, sizeof p); p.dwSize = 0x20; p.dwCopyFlags = COPY_FILE_FAIL_IF_EXISTS;
    HRESULT hr = U(CopyFile2)(s, d, (COPYFILE2_EXTENDED_PARAMETERS *)&p);
    EXPECT_EQ((ULONG)hr, K_HR(ERROR_FILE_EXISTS));
    EXPECT_EQ(kf_size(da), 6);
    hr = U(CopyFile2)(s, d, NULL);   /* overwrite by default */
    EXPECT_EQ(hr, 0);
    EXPECT_EQ(kf_size(da), 3);
    kf_cleanup("cp2_e_src.bin"); kf_cleanup("cp2_e_dst.bin");
}
TEST(cp2_missing_source_and_dir) {
    REQUIRE(U(CopyFile2));
    WCHAR s[MAX_PATH], d[MAX_PATH]; kf_wpath(s, MAX_PATH, "cp2_nosrc.bin"); kf_wpath(d, MAX_PATH, "cp2_nosrc_dst.bin");
    EXPECT_EQ((ULONG)U(CopyFile2)(s, d, NULL), K_HR(ERROR_FILE_NOT_FOUND));
    char sa[MAX_PATH]; t_tmpfile(sa, sizeof sa, "cp2_ok_src.bin"); kf_write(sa, "x", 1);
    kf_wpath(s, MAX_PATH, "cp2_ok_src.bin"); kf_wpath(d, MAX_PATH, "nodir_cp2\\dst.bin");
    EXPECT_EQ((ULONG)U(CopyFile2)(s, d, NULL), K_HR(ERROR_PATH_NOT_FOUND));
    kf_cleanup("cp2_ok_src.bin");
}
TEST(cp2_null_names) {
    REQUIRE(U(CopyFile2));
    WCHAR d[MAX_PATH]; kf_wpath(d, MAX_PATH, "cp2_null_dst.bin");
    HRESULT hr = U(CopyFile2)(NULL, d, NULL);
    CHECK(FAILED(hr)); t_note("CopyFile2(NULL src) -> 0x%lx", (ULONG)hr);
    hr = U(CopyFile2)(L"", d, NULL);
    CHECK(FAILED(hr));
    WCHAR s[MAX_PATH]; kf_wpath(s, MAX_PATH, "cp2_null_src.bin");
    char sa[MAX_PATH]; t_tmpfile(sa, sizeof sa, "cp2_null_src.bin"); kf_write(sa, "x", 1);
    hr = U(CopyFile2)(s, NULL, NULL);
    CHECK(FAILED(hr)); t_note("CopyFile2(NULL dst) -> 0x%lx", (ULONG)hr);
    hr = U(CopyFile2)(s, L"", NULL);
    CHECK(FAILED(hr));
    kf_cleanup("cp2_null_src.bin");
}
TEST(cp2_readonly_destination) {
    REQUIRE(U(CopyFile2));
    WCHAR s[MAX_PATH], d[MAX_PATH]; kf_wpath(s, MAX_PATH, "cp2_r_src.bin"); kf_wpath(d, MAX_PATH, "cp2_r_dst.bin");
    char sa[MAX_PATH], da[MAX_PATH]; t_tmpfile(sa, sizeof sa, "cp2_r_src.bin"); t_tmpfile(da, sizeof da, "cp2_r_dst.bin");
    kf_write(sa, "src", 3); kf_write(da, "dst-ro", 6);
    SetFileAttributesA(da, FILE_ATTRIBUTE_READONLY);
    EXPECT_EQ((ULONG)U(CopyFile2)(s, d, NULL), K_HR(ERROR_ACCESS_DENIED));
    EXPECT_EQ(kf_size(da), 6);
    kf_cleanup("cp2_r_src.bin"); kf_cleanup("cp2_r_dst.bin");
}
TEST(cp2_source_directory) {
    REQUIRE(U(CopyFile2));
    char dir[MAX_PATH]; t_tmpfile(dir, sizeof dir, "cp2_srcdir"); CreateDirectoryA(dir, NULL);
    WCHAR s[MAX_PATH], d[MAX_PATH]; kf_wpath(s, MAX_PATH, "cp2_srcdir"); kf_wpath(d, MAX_PATH, "cp2_srcdir_dst.bin");
    HRESULT hr = U(CopyFile2)(s, d, NULL);
    CHECK(FAILED(hr)); t_note("CopyFile2(directory source) -> 0x%lx", (ULONG)hr);
    kf_cleanup("cp2_srcdir_dst.bin"); RemoveDirectoryA(dir);
}
TEST(cp2_progress_callback) {
    REQUIRE(U(CopyFile2));
    WCHAR s[MAX_PATH], d[MAX_PATH]; kf_wpath(s, MAX_PATH, "cp2_p_src.bin"); kf_wpath(d, MAX_PATH, "cp2_p_dst.bin");
    char sa[MAX_PATH], da[MAX_PATH]; t_tmpfile(sa, sizeof sa, "cp2_p_src.bin"); t_tmpfile(da, sizeof da, "cp2_p_dst.bin");
    BYTE *buf = (BYTE *)HeapAlloc(GetProcessHeap(), 0, 3 << 20);
    if (!buf) { CHECK(0); return; }
    memset(buf, 0x5a, 3 << 20);
    kf_write(sa, buf, 3 << 20); DeleteFileA(da);
    HeapFree(GetProcessHeap(), 0, buf);
    g_cf2_calls = 0; memset(g_cf2_types, 0, sizeof g_cf2_types);
    K_COPYFILE2_PARAMS p; memset(&p, 0, sizeof p); p.dwSize = 0x20; p.pProgress = (void *)cf2_progress;
    HRESULT hr = U(CopyFile2)(s, d, (COPYFILE2_EXTENDED_PARAMETERS *)&p);
    EXPECT_EQ(hr, 0);
    CHECK(g_cf2_calls > 0);
    t_note("progress calls: %ld first types: %ld %ld %ld %ld", g_cf2_calls, g_cf2_types[0], g_cf2_types[1], g_cf2_types[2], g_cf2_types[3]);
    for (int i = 0; i < g_cf2_calls && i < 16; i++) CHECKF(g_cf2_types[i] >= 1 && g_cf2_types[i] <= 6, "type %ld", g_cf2_types[i]);
    EXPECT_EQ(kf_size(da), 3 << 20);
    kf_cleanup("cp2_p_src.bin"); kf_cleanup("cp2_p_dst.bin");
}
TEST(cp2_cancel_pointer) {
    REQUIRE(U(CopyFile2));
    WCHAR s[MAX_PATH], d[MAX_PATH]; kf_wpath(s, MAX_PATH, "cp2_c_src.bin"); kf_wpath(d, MAX_PATH, "cp2_c_dst.bin");
    char sa[MAX_PATH], da[MAX_PATH]; t_tmpfile(sa, sizeof sa, "cp2_c_src.bin"); t_tmpfile(da, sizeof da, "cp2_c_dst.bin");
    kf_write(sa, "cancel", 6); DeleteFileA(da);
    BOOL cancel = TRUE;
    K_COPYFILE2_PARAMS p; memset(&p, 0, sizeof p); p.dwSize = 0x20; p.pfCancel = &cancel;
    HRESULT hr = U(CopyFile2)(s, d, (COPYFILE2_EXTENDED_PARAMETERS *)&p);
    t_note("CopyFile2 with *pfCancel=TRUE -> 0x%lx; dst exists=%d", (ULONG)hr, GetFileAttributesA(da) != INVALID_FILE_ATTRIBUTES);
    cancel = FALSE;
    hr = U(CopyFile2)(s, d, (COPYFILE2_EXTENDED_PARAMETERS *)&p);
    EXPECT_EQ(hr, 0);
    kf_cleanup("cp2_c_src.bin"); kf_cleanup("cp2_c_dst.bin");
}
TEST(cp2_callback_cancel) {
    REQUIRE(U(CopyFile2));
    WCHAR s[MAX_PATH], d[MAX_PATH]; kf_wpath(s, MAX_PATH, "cp2_cb_src.bin"); kf_wpath(d, MAX_PATH, "cp2_cb_dst.bin");
    char sa[MAX_PATH], da[MAX_PATH]; t_tmpfile(sa, sizeof sa, "cp2_cb_src.bin"); t_tmpfile(da, sizeof da, "cp2_cb_dst.bin");
    kf_write(sa, "cb", 2); DeleteFileA(da);
    DWORD action = 1;   /* COPYFILE2_PROGRESS_CANCEL */
    g_cf2_calls = 0;
    K_COPYFILE2_PARAMS p; memset(&p, 0, sizeof p); p.dwSize = 0x20; p.pProgress = (void *)cf2_progress; p.pvCtx = &action;
    HRESULT hr = U(CopyFile2)(s, d, (COPYFILE2_EXTENDED_PARAMETERS *)&p);
    CHECK(FAILED(hr));
    CHECK(g_cf2_calls >= 1);
    t_note("CopyFile2 callback CANCEL -> 0x%lx", (ULONG)hr);
    kf_cleanup("cp2_cb_src.bin"); kf_cleanup("cp2_cb_dst.bin");
}
TEST(cp2_same_file) {
    REQUIRE(U(CopyFile2));
    WCHAR s[MAX_PATH]; kf_wpath(s, MAX_PATH, "cp2_same.bin");
    char sa[MAX_PATH]; t_tmpfile(sa, sizeof sa, "cp2_same.bin"); kf_write(sa, "same", 4);
    HRESULT hr = U(CopyFile2)(s, s, NULL);
    CHECK(FAILED(hr)); t_note("CopyFile2(self->self) -> 0x%lx", (ULONG)hr);
    EXPECT_EQ(kf_size(sa), 4);
    kf_cleanup("cp2_same.bin");
}
TEST(cp2_preserves_attributes_and_times) {
    REQUIRE(U(CopyFile2));
    WCHAR s[MAX_PATH], d[MAX_PATH]; kf_wpath(s, MAX_PATH, "cp2_a_src.bin"); kf_wpath(d, MAX_PATH, "cp2_a_dst.bin");
    char sa[MAX_PATH], da[MAX_PATH]; t_tmpfile(sa, sizeof sa, "cp2_a_src.bin"); t_tmpfile(da, sizeof da, "cp2_a_dst.bin");
    kf_write(sa, "attr", 4); DeleteFileA(da);
    SetFileAttributesA(sa, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_ARCHIVE);
    FILETIME ft = { 0x12345678, 0x01D00000 };
    HANDLE h = CreateFileA(sa, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) { SetFileTime(h, NULL, NULL, &ft); CloseHandle(h); }
    EXPECT_EQ(U(CopyFile2)(s, d, NULL), 0);
    DWORD at = GetFileAttributesA(da);
    CHECK(at != INVALID_FILE_ATTRIBUTES && (at & FILE_ATTRIBUTE_HIDDEN));
    h = CreateFileA(da, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        FILETIME w; CHECK(GetFileTime(h, NULL, NULL, &w));
        CHECK(w.dwLowDateTime == ft.dwLowDateTime && w.dwHighDateTime == ft.dwHighDateTime);
        CloseHandle(h);
    }
    SetFileAttributesA(sa, FILE_ATTRIBUTE_NORMAL); SetFileAttributesA(da, FILE_ATTRIBUTE_NORMAL);
    kf_cleanup("cp2_a_src.bin"); kf_cleanup("cp2_a_dst.bin");
}
TEST(cp2_zero_length) {
    REQUIRE(U(CopyFile2));
    WCHAR s[MAX_PATH], d[MAX_PATH]; kf_wpath(s, MAX_PATH, "cp2_z_src.bin"); kf_wpath(d, MAX_PATH, "cp2_z_dst.bin");
    char sa[MAX_PATH], da[MAX_PATH]; t_tmpfile(sa, sizeof sa, "cp2_z_src.bin"); t_tmpfile(da, sizeof da, "cp2_z_dst.bin");
    kf_write(sa, "", 0); DeleteFileA(da);
    EXPECT_EQ(U(CopyFile2)(s, d, NULL), 0);
    EXPECT_EQ(kf_size(da), 0);
    kf_cleanup("cp2_z_src.bin"); kf_cleanup("cp2_z_dst.bin");
}
TEST(cp2_lasterror_not_required) {
    REQUIRE(U(CopyFile2));
    WCHAR s[MAX_PATH], d[MAX_PATH]; kf_wpath(s, MAX_PATH, "cp2_le_nosrc.bin"); kf_wpath(d, MAX_PATH, "cp2_le_dst.bin");
    SET_ERR(0x4444);
    HRESULT hr = U(CopyFile2)(s, d, NULL);
    EXPECT_EQ((ULONG)hr, K_HR(ERROR_FILE_NOT_FOUND));
    t_note("GetLastError after failed CopyFile2: %lu", GetLastError());
}

/* ======================= DeviceIoControl misc ======================= */
TEST(dioc_invalid_handles) {
    REQUIRE(U(DeviceIoControl));
    DWORD ret = 0x77; char out[16];
    BOOL r = U(DeviceIoControl)(INVALID_HANDLE_VALUE, FSCTL_GET_COMPRESSION, NULL, 0, out, sizeof out, &ret, NULL);
    CHECK(!r); EXPECT_ERR(ERROR_INVALID_HANDLE);
    r = U(DeviceIoControl)(NULL, FSCTL_GET_COMPRESSION, NULL, 0, out, sizeof out, &ret, NULL);
    CHECK(!r); EXPECT_ERR(ERROR_INVALID_HANDLE);
    r = U(DeviceIoControl)((HANDLE)(ULONG_PTR)0x7FFFFFF0, FSCTL_GET_COMPRESSION, NULL, 0, out, sizeof out, &ret, NULL);
    CHECK(!r); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(dioc_wrong_object_type) {
    REQUIRE(U(DeviceIoControl));
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    DWORD ret = 0x77; char out[16];
    BOOL r = U(DeviceIoControl)(ev, FSCTL_GET_COMPRESSION, NULL, 0, out, sizeof out, &ret, NULL);
    CHECK(!r); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE mtx = CreateMutexA(NULL, FALSE, NULL);
    r = U(DeviceIoControl)(mtx, 0x12345678, NULL, 0, out, sizeof out, &ret, NULL);
    CHECK(!r); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(mtx); CloseHandle(ev);
}
TEST(dioc_closed_handle) {
    REQUIRE(U(DeviceIoControl));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "dioc_closed.bin");
    HANDLE h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CloseHandle(h);
    DWORD ret = 0; char out[16];
    BOOL r = U(DeviceIoControl)(h, FSCTL_GET_COMPRESSION, NULL, 0, out, sizeof out, &ret, NULL);
    CHECK(!r); EXPECT_ERR(ERROR_INVALID_HANDLE);
    kf_cleanup("dioc_closed.bin");
}
TEST(dioc_get_compression) {
    REQUIRE(U(DeviceIoControl));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "dioc_comp.bin");
    HANDLE h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    USHORT fmt = 0x7777; DWORD ret = 0;
    BOOL r = U(DeviceIoControl)(h, FSCTL_GET_COMPRESSION, NULL, 0, &fmt, sizeof fmt, &ret, NULL);
    if (r) { EXPECT_EQ(ret, 2); t_note("compression format %u", fmt); CHECK(fmt == 0 || fmt == 2); }
    else t_note("FSCTL_GET_COMPRESSION unsupported (%lu)", GetLastError());
    /* output buffer too small */
    char one = 0x55; ret = 0xdead;
    SET_ERR(0);
    r = U(DeviceIoControl)(h, FSCTL_GET_COMPRESSION, NULL, 0, &one, 1, &ret, NULL);
    if (!r) t_note("one-byte output buffer -> err %lu", GetLastError());
    CHECK(!r);
    r = U(DeviceIoControl)(h, FSCTL_GET_COMPRESSION, NULL, 0, NULL, 0, &ret, NULL);
    CHECK(!r);
    CloseHandle(h);
    kf_cleanup("dioc_comp.bin");
}
TEST(dioc_not_a_reparse_point) {
    REQUIRE(U(DeviceIoControl));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "dioc_rp.bin");
    kf_write(p, "plain", 5);
    HANDLE h = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) return;
    BYTE buf[1024]; DWORD ret = 0;
    BOOL r = U(DeviceIoControl)(h, FSCTL_GET_REPARSE_POINT, NULL, 0, buf, sizeof buf, &ret, NULL);
    CHECK(!r);
    EXPECT_ERR(ERROR_NOT_A_REPARSE_POINT);
    CloseHandle(h);
    kf_cleanup("dioc_rp.bin");
}
TEST(dioc_bogus_ioctl_on_file) {
    REQUIRE(U(DeviceIoControl));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "dioc_bogus.bin");
    kf_write(p, "plain", 5);
    HANDLE h = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    DWORD ret = 0; BYTE buf[16];
    BOOL r = U(DeviceIoControl)(h, 0x00000000, NULL, 0, buf, sizeof buf, &ret, NULL);
    CHECK(!r); t_note("ioctl 0 on file -> %lu", GetLastError());
    r = U(DeviceIoControl)(h, CTL_CODE(FILE_DEVICE_UNKNOWN, 0xABC, METHOD_BUFFERED, FILE_ANY_ACCESS), NULL, 0, buf, sizeof buf, &ret, NULL);
    CHECK(!r); t_note("unknown ioctl on file -> %lu", GetLastError());
    CloseHandle(h);
    kf_cleanup("dioc_bogus.bin");
}
TEST(dioc_set_sparse_and_query_alloc) {
    REQUIRE(U(DeviceIoControl));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "dioc_sparse.bin");
    HANDLE h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    DWORD ret = 0;
    BOOL r = U(DeviceIoControl)(h, FSCTL_SET_SPARSE, NULL, 0, NULL, 0, &ret, NULL);
    if (!r) { t_note("FSCTL_SET_SPARSE unsupported (%lu)", GetLastError()); CloseHandle(h); kf_cleanup("dioc_sparse.bin"); return; }
    BY_HANDLE_FILE_INFORMATION bi;
    CHECK(GetFileInformationByHandle(h, &bi));
    CHECK(bi.dwFileAttributes & FILE_ATTRIBUTE_SPARSE_FILE);
    SetFilePointer(h, 1 << 20, NULL, FILE_BEGIN); SetEndOfFile(h);
    FILE_ALLOCATED_RANGE_BUFFER q; memset(&q, 0, sizeof q); q.Length.QuadPart = 1 << 20;
    FILE_ALLOCATED_RANGE_BUFFER out[4];
    r = U(DeviceIoControl)(h, FSCTL_QUERY_ALLOCATED_RANGES, &q, sizeof q, out, sizeof out, &ret, NULL);
    CHECK(r);
    EXPECT_EQ(ret, 0);   /* nothing is allocated in an all-hole file */
    CloseHandle(h);
    kf_cleanup("dioc_sparse.bin");
}
TEST(dioc_bytes_returned_null_sync) {
    REQUIRE(U(DeviceIoControl));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "dioc_null.bin");
    HANDLE h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    USHORT fmt = 0; DWORD ret = 0;
    /* lpBytesReturned must be non-NULL when lpOverlapped is NULL; test the documented valid shape only */
    BOOL r = U(DeviceIoControl)(h, FSCTL_GET_COMPRESSION, NULL, 0, &fmt, sizeof fmt, &ret, NULL);
    CHECK(r || GetLastError() != 0);
    CloseHandle(h);
    kf_cleanup("dioc_null.bin");
}
TEST(dioc_overlapped_on_async_handle) {
    REQUIRE(U(DeviceIoControl));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "dioc_ovl.bin");
    HANDLE h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_OVERLAPPED, NULL);
    if (h == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    OVERLAPPED ov; memset(&ov, 0, sizeof ov);
    ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    USHORT fmt = 0x7777; DWORD ret = 0;
    BOOL r = U(DeviceIoControl)(h, FSCTL_GET_COMPRESSION, NULL, 0, &fmt, sizeof fmt, NULL, &ov);
    if (!r && GetLastError() == ERROR_IO_PENDING) r = GetOverlappedResult(h, &ov, &ret, TRUE);
    else if (r) GetOverlappedResult(h, &ov, &ret, TRUE);
    CHECK(r);
    EXPECT_EQ(ret, 2);
    CloseHandle(ov.hEvent);
    CloseHandle(h);
    kf_cleanup("dioc_ovl.bin");
}
