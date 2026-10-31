/* FormatMessageA / FormatMessageW. */
#include "eed.h"

#define LANG_US 0x0409
#define FM_SYS  FORMAT_MESSAGE_FROM_SYSTEM
#define FM_IGN  FORMAT_MESSAGE_IGNORE_INSERTS
#define FM_STR  FORMAT_MESSAGE_FROM_STRING
#define FM_ARR  FORMAT_MESSAGE_ARGUMENT_ARRAY

static DWORD g_fmerr;
static DWORD fw(DWORD flags, LPCVOID src, DWORD id, DWORD lang, DWORD_PTR *args, WCHAR *out, DWORD cap) {
    SetLastError(0xCAFE);
    DWORD r = U(FormatMessageW)(flags, src, id, lang, out, cap, (va_list *)args);
    g_fmerr = GetLastError();
    return r;
}
static DWORD fa(DWORD flags, LPCVOID src, DWORD id, DWORD lang, DWORD_PTR *args, char *out, DWORD cap) {
    SetLastError(0xCAFE);
    DWORD r = U(FormatMessageA)(flags, src, id, lang, out, cap, (va_list *)args);
    g_fmerr = GetLastError();
    return r;
}
static void narrow(const WCHAR *w, char *o, size_t cap) {
    size_t i = 0;
    for (; w[i] && i + 1 < cap; i++) o[i] = w[i] < 128 ? (char)w[i] : '?';
    o[i] = 0;
}
/* compare wide result against ascii text */
static int weq(const WCHAR *w, const char *a) {
    size_t i = 0;
    for (; a[i]; i++) if (w[i] != (WCHAR)(unsigned char)a[i]) return 0;
    return w[i] == 0;
}
static void esc(const char *in, char *out, size_t cap) {
    size_t o = 0;
    for (; *in && o + 5 < cap; in++) {
        if (*in == '\r') { out[o++] = '\\'; out[o++] = 'r'; }
        else if (*in == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
        else if (*in == '\t') { out[o++] = '\\'; out[o++] = 't'; }
        else out[o++] = *in;
    }
    out[o] = 0;
}
/* EXPECT string: FROM_STRING style W formatting */
#define FWS(fmt, args, expect) do { \
    WCHAR ow_[1024]; WCHAR fw_[512]; char nb_[1024], eb_[1024]; \
    eed_widen(fmt, fw_, 512); \
    DWORD r_ = fw(FM_STR | FM_ARR, fw_, 0, 0, args, ow_, 1024); \
    narrow(ow_, nb_, sizeof nb_); esc(nb_, eb_, sizeof eb_); \
    t_check(r_ == strlen(expect) && weq(ow_, expect), __FILE__, __LINE__, "FormatMessageW(\"" fmt "\")", "got [%s] n=%lu want [%s] err=%lu", eb_, r_, expect, g_fmerr); \
} while (0)

struct sysmsg { DWORD id; const char *text; };
static void check_sys_table(const struct sysmsg *t, size_t n) {
    for (size_t i = 0; i < n; i++) {
        WCHAR w[512]; char a[512], got[512], want[600], e1[600], e2[600];
        DWORD r = fw(FM_SYS | FM_IGN, NULL, t[i].id, LANG_US, NULL, w, 512);
        snprintf(want, sizeof want, "%s\r\n", t[i].text);
        narrow(w, got, sizeof got); esc(got, e1, sizeof e1); esc(want, e2, sizeof e2);
        if (!eed_en_us() && strcmp(got, want)) { t_note("non-en-US UI: id %lu -> %s", t[i].id, e1); continue; }
        CHECKF(r == strlen(want) && weq(w, want), "id %lu got [%s] n=%lu want [%s] err=%lu", t[i].id, e1, r, e2, g_fmerr);
        /* A parity */
        DWORD ra = fa(FM_SYS | FM_IGN, NULL, t[i].id, LANG_US, NULL, a, 512);
        CHECKF(ra == r && !strcmp(a, got), "A/W mismatch id %lu", t[i].id);
    }
}
#define SYSTEST(name, ...) TEST(name) { REQUIRE(U(FormatMessageW)); REQUIRE(U(FormatMessageA)); \
    static const struct sysmsg t[] = { __VA_ARGS__ }; check_sys_table(t, sizeof t / sizeof *t); }

SYSTEST(eed_fm_sys_0_to_20,
    {0, "The operation completed successfully."}, {1, "Incorrect function."},
    {2, "The system cannot find the file specified."}, {3, "The system cannot find the path specified."},
    {4, "The system cannot open the file."}, {5, "Access is denied."}, {6, "The handle is invalid."},
    {8, "Not enough memory resources are available to process this command."},
    {10, "The environment is incorrect."}, {11, "An attempt was made to load a program with an incorrect format."},
    {12, "The access code is invalid."}, {13, "The data is invalid."},
    {14, "Not enough memory resources are available to complete this operation."},
    {15, "The system cannot find the drive specified."}, {16, "The directory cannot be removed."},
    {17, "The system cannot move the file to a different disk drive."}, {18, "There are no more files."},
    {19, "The media is write protected."}, {20, "The system cannot find the device specified."})
SYSTEST(eed_fm_sys_21_to_50,
    {21, "The device is not ready."}, {22, "The device does not recognize the command."},
    {23, "Data error (cyclic redundancy check)."},
    {24, "The program issued a command but the command length is incorrect."},
    {25, "The drive cannot locate a specific area or track on the disk."},
    {26, "The specified disk or diskette cannot be accessed."}, {27, "The drive cannot find the sector requested."},
    {28, "The printer is out of paper."}, {29, "The system cannot write to the specified device."},
    {30, "The system cannot read from the specified device."}, {31, "A device attached to the system is not functioning."},
    {32, "The process cannot access the file because it is being used by another process."},
    {33, "The process cannot access the file because another process has locked a portion of the file."},
    {36, "Too many files opened for sharing."}, {38, "Reached the end of the file."}, {39, "The disk is full."},
    {50, "The request is not supported."})
SYSTEST(eed_fm_sys_51_to_125,
    {53, "The network path was not found."}, {54, "The network is busy."},
    {55, "The specified network resource or device is no longer available."},
    {64, "The specified network name is no longer available."}, {65, "Network access is denied."},
    {66, "The network resource type is not correct."}, {67, "The network name cannot be found."},
    {80, "The file exists."}, {82, "The directory or file cannot be created."},
    {85, "The local device name is already in use."}, {86, "The specified network password is not correct."},
    {87, "The parameter is incorrect."}, {108, "The disk is in use or locked by another process."},
    {109, "The pipe has been ended."}, {110, "The system cannot open the device or file specified."},
    {111, "The file name is too long."}, {112, "There is not enough space on the disk."},
    {120, "This function is not supported on this system."}, {121, "The semaphore timeout period has expired."},
    {122, "The data area passed to a system call is too small."},
    {123, "The filename, directory name, or volume label syntax is incorrect."},
    {124, "The system call level is not correct."}, {125, "The disk has no volume label."})
SYSTEST(eed_fm_sys_126_and_up,
    {126, "The specified module could not be found."}, {127, "The specified procedure could not be found."},
    {128, "There are no child processes to wait for."},
    {131, "An attempt was made to move the file pointer before the beginning of the file."},
    {145, "The directory is not empty."}, {161, "The specified path is invalid."}, {170, "The requested resource is in use."},
    {183, "Cannot create a file when that file already exists."}, {193, "%1 is not a valid Win32 application."},
    {206, "The filename or extension is too long."}, {232, "The pipe is being closed."},
    {233, "No process is on the other end of the pipe."}, {234, "More data is available."},
    {258, "The wait operation timed out."}, {259, "No more data is available."}, {267, "The directory name is invalid."})
SYSTEST(eed_fm_sys_high_codes,
    {317, "The system cannot find message text for message number 0x%1 in the message file for %2."},
    {997, "Overlapped I/O operation is in progress."}, {998, "Invalid access to memory location."},
    {1223, "The operation was canceled by the user."}, {1314, "A required privilege is not held by the client."},
    {1400, "Invalid window handle."}, {1450, "Insufficient system resources exist to complete the requested service."},
    {1460, "This operation returned because the timeout period expired."}, {1722, "The RPC server is unavailable."})

TEST(eed_fm_sys_hresult_win32_facility) {
    REQUIRE(U(FormatMessageW));
    WCHAR a[256], b[256];
    DWORD ra = fw(FM_SYS | FM_IGN, NULL, 5, LANG_US, NULL, a, 256);
    DWORD rb = fw(FM_SYS | FM_IGN, NULL, 0x80070005, LANG_US, NULL, b, 256);
    CHECK(ra > 0); EXPECT_EQ(ra, rb); CHECK(!lstrcmpW(a, b));
    ra = fw(FM_SYS | FM_IGN, NULL, 2, LANG_US, NULL, a, 256);
    rb = fw(FM_SYS | FM_IGN, NULL, 0x80070002, LANG_US, NULL, b, 256);
    CHECK(ra > 0); EXPECT_EQ(ra, rb); CHECK(!lstrcmpW(a, b));
    rb = fw(FM_SYS | FM_IGN, NULL, 0x80070057, LANG_US, NULL, b, 256);
    CHECK(rb > 0);
    if (eed_en_us()) CHECK(weq(b, "The parameter is incorrect.\r\n"));
}
TEST(eed_fm_sys_lang_neutral_works) {
    REQUIRE(U(FormatMessageW));
    WCHAR w[256];
    DWORD r = fw(FM_SYS | FM_IGN, NULL, 5, 0, NULL, w, 256);
    CHECK(r > 0);
    if (eed_en_us()) CHECK(weq(w, "Access is denied.\r\n"));
}
TEST(eed_fm_sys_lang_primary_english_neutral) {
    REQUIRE(U(FormatMessageW));
    WCHAR w[256];
    DWORD r = fw(FM_SYS | FM_IGN, NULL, 5, MAKELANGID(LANG_ENGLISH, SUBLANG_NEUTRAL), NULL, w, 256);
    if (!r) t_note("LANG_ENGLISH/SUBLANG_NEUTRAL: err %lu", g_fmerr); else CHECK(r > 0);
}
TEST(eed_fm_sys_lang_other_languages_note) {
    REQUIRE(U(FormatMessageW));
    static const WORD lids[] = { 0x0809, 0x0407, 0x040C, 0x0411, 0x0436, 0x07FF };
    for (size_t i = 0; i < sizeof lids / sizeof *lids; i++) {
        WCHAR w[256]; char n[256];
        DWORD r = fw(FM_SYS | FM_IGN, NULL, 5, lids[i], NULL, w, 256);
        narrow(w, n, sizeof n);
        t_note("lang 0x%04x id 5 -> r=%lu err=%lu [%.40s]", lids[i], r, r ? 0 : g_fmerr, n);
    }
}
TEST(eed_fm_sys_unknown_ids_317) {
    REQUIRE(U(FormatMessageW)); REQUIRE(U(FormatMessageA));
    static const DWORD ids[] = { 0x00FFFFFF, 0x7FFFFFFF, 0x12345678, 0xBFFFFFFF, 0x0000FFFF };
    for (size_t i = 0; i < sizeof ids / sizeof *ids; i++) {
        WCHAR w[256]; char a[256];
        w[0] = 0x7777;
        DWORD r = fw(FM_SYS | FM_IGN, NULL, ids[i], LANG_US, NULL, w, 256);
        CHECKF(r == 0, "id 0x%lx returned %lu", ids[i], r);
        CHECKF(g_fmerr == ERROR_MR_MID_NOT_FOUND, "id 0x%lx err %lu", ids[i], g_fmerr);
        r = fa(FM_SYS | FM_IGN, NULL, ids[i], LANG_US, NULL, a, 256);
        CHECK(r == 0); CHECK(g_fmerr == ERROR_MR_MID_NOT_FOUND);
    }
}
TEST(eed_fm_sys_with_insert_array) {
    REQUIRE(U(FormatMessageW));
    WCHAR w[512]; DWORD_PTR args[1] = { (DWORD_PTR)L"foo.exe" };
    DWORD r = fw(FM_SYS | FM_ARR, NULL, 193, LANG_US, args, w, 512);
    CHECK(r > 0);
    if (eed_en_us()) CHECK(weq(w, "foo.exe is not a valid Win32 application.\r\n"));
}
TEST(eed_fm_sys_ignore_inserts_keeps_percent1) {
    REQUIRE(U(FormatMessageW));
    WCHAR w[512];
    DWORD r = fw(FM_SYS | FM_IGN, NULL, 193, LANG_US, NULL, w, 512);
    CHECK(r > 0);
    CHECK(wcsstr(w, L"%1") != NULL);
}
TEST(eed_fm_sys_mid_not_found_text_has_inserts) {
    REQUIRE(U(FormatMessageW));
    WCHAR w[512];
    DWORD r = fw(FM_SYS | FM_IGN, NULL, ERROR_MR_MID_NOT_FOUND, LANG_US, NULL, w, 512);
    CHECK(r > 0);
    CHECK(wcsstr(w, L"%1") != NULL);
    CHECK(wcsstr(w, L"%2") != NULL);
}
TEST(eed_fm_sys_width_mask_strips_crlf) {
    REQUIRE(U(FormatMessageW));
    WCHAR w[512]; char n[512];
    DWORD r = fw(FM_SYS | FM_IGN | FORMAT_MESSAGE_MAX_WIDTH_MASK, NULL, 2, LANG_US, NULL, w, 512);
    CHECK(r > 0);
    narrow(w, n, sizeof n);
    CHECK(strchr(n, '\r') == NULL); CHECK(strchr(n, '\n') == NULL);
    if (eed_en_us()) { /* possibly one trailing space */
        char e[64]; size_t l = strlen(n); while (l && n[l - 1] == ' ') n[--l] = 0;
        (void)e; EXPECT_STR(n, "The system cannot find the file specified.");
    }
    EXPECT_EQ(r, wcslen(w));
}
TEST(eed_fm_sys_width_mask_various_ids_have_no_linebreaks) {
    REQUIRE(U(FormatMessageA));
    static const DWORD ids[] = { 1, 5, 6, 8, 32, 33, 87, 122, 126, 183, 997 };
    for (size_t i = 0; i < sizeof ids / sizeof *ids; i++) {
        char a[512];
        DWORD r = fa(FM_SYS | FM_IGN | FORMAT_MESSAGE_MAX_WIDTH_MASK, NULL, ids[i], LANG_US, NULL, a, 512);
        CHECK(r > 0); CHECK(!strchr(a, '\n')); CHECK(!strchr(a, '\r'));
        EXPECT_EQ(r, strlen(a));
    }
}
TEST(eed_fm_sys_width_limit_wraps_lines) {
    REQUIRE(U(FormatMessageA));
    char a[1024];
    DWORD r = fa(FM_SYS | FM_IGN | 20, NULL, 32, LANG_US, NULL, a, 1024);
    CHECK(r > 0);
    /* every line is at most 20 characters (excluding CRLF) except possibly an unbreakable word */
    int ok = 1; const char *p = a;
    while (*p) {
        const char *e = p; while (*e && *e != '\r' && *e != '\n') e++;
        if (e - p > 20) { ok = 0; t_note("long line %d", (int)(e - p)); }
        p = e; while (*p == '\r' || *p == '\n') p++;
    }
    CHECK(ok);
    CHECK(strchr(a, '\n') != NULL);
}

/* ---- ALLOCATE_BUFFER ---- */
TEST(eed_fm_alloc_basic_w) {
    REQUIRE(U(FormatMessageW)); REQUIRE(U(LocalFree)); REQUIRE(U(LocalSize));
    WCHAR *p = NULL;
    SetLastError(0);
    DWORD r = U(FormatMessageW)(FM_SYS | FM_IGN | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, 5, LANG_US, (LPWSTR)&p, 0, NULL);
    CHECK(r > 0); CHECK(p != NULL);
    if (p) {
        EXPECT_EQ(r, wcslen(p));
        CHECK(LocalSize(p) >= (r + 1) * sizeof(WCHAR));
        CHECK(U(LocalFree)(p) == NULL);
    }
}
TEST(eed_fm_alloc_basic_a) {
    REQUIRE(U(FormatMessageA)); REQUIRE(U(LocalFree)); REQUIRE(U(LocalSize));
    char *p = NULL;
    DWORD r = U(FormatMessageA)(FM_SYS | FM_IGN | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, 2, LANG_US, (LPSTR)&p, 0, NULL);
    CHECK(r > 0); CHECK(p != NULL);
    if (p) {
        EXPECT_EQ(r, strlen(p));
        CHECK(LocalSize(p) >= r + 1);
        if (eed_en_us()) EXPECT_STR(p, "The system cannot find the file specified.\r\n");
        CHECK(U(LocalFree)(p) == NULL);
    }
}
TEST(eed_fm_alloc_min_size) {
    REQUIRE(U(FormatMessageW)); REQUIRE(U(LocalFree)); REQUIRE(U(LocalSize));
    WCHAR *p = NULL;
    DWORD r = U(FormatMessageW)(FM_SYS | FM_IGN | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, 5, LANG_US, (LPWSTR)&p, 1000, NULL);
    CHECK(r > 0 && r < 1000); CHECK(p != NULL);
    if (p) { CHECKF(LocalSize(p) >= 1000 * sizeof(WCHAR), "LocalSize %zu", (size_t)LocalSize(p)); U(LocalFree)(p); }
}
TEST(eed_fm_alloc_min_size_a) {
    REQUIRE(U(FormatMessageA)); REQUIRE(U(LocalFree)); REQUIRE(U(LocalSize));
    char *p = NULL;
    DWORD r = U(FormatMessageA)(FM_SYS | FM_IGN | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, 5, LANG_US, (LPSTR)&p, 700, NULL);
    CHECK(r > 0 && r < 700); CHECK(p != NULL);
    if (p) { CHECKF(LocalSize(p) >= 700, "LocalSize %zu", (size_t)LocalSize(p)); U(LocalFree)(p); }
}
TEST(eed_fm_alloc_from_string_long_insert) {
    REQUIRE(U(FormatMessageW)); REQUIRE(U(LocalFree));
    WCHAR big[3001]; for (int i = 0; i < 3000; i++) big[i] = L'a' + (i % 26); big[3000] = 0;
    DWORD_PTR args[1] = { (DWORD_PTR)big };
    WCHAR *p = NULL;
    DWORD r = U(FormatMessageW)(FM_STR | FM_ARR | FORMAT_MESSAGE_ALLOCATE_BUFFER, L"[%1]", 0, 0, (LPWSTR)&p, 0, (va_list *)args);
    EXPECT_EQ(r, 3002);
    if (p) { CHECK(p[0] == L'[' && p[3001] == L']' && p[3002] == 0); CHECK(!wcsncmp(p + 1, big, 3000)); U(LocalFree)(p); }
}
TEST(eed_fm_alloc_buffer_pointer_zeroed_on_failure_or_null) {
    REQUIRE(U(FormatMessageW));
    WCHAR *p = (WCHAR *)(ULONG_PTR)0x1234;
    DWORD r = U(FormatMessageW)(FM_SYS | FM_IGN | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, 0x7FFFFFFF, LANG_US, (LPWSTR)&p, 0, NULL);
    EXPECT_EQ(r, 0);
    EXPECT_ERR(ERROR_MR_MID_NOT_FOUND);
    EXPECT_EQ(p, 0);   /* decomp: the pointer slot is cleared to NULL up front */
}
TEST(eed_fm_alloc_null_ptr_slot) {
    REQUIRE(U(FormatMessageW));
    SetLastError(0);
    DWORD r = U(FormatMessageW)(FM_SYS | FM_IGN | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, 5, LANG_US, NULL, 0, NULL);
    EXPECT_EQ(r, 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}

/* ---- buffer size semantics ---- */
TEST(eed_fm_size_null_buffer_87) {
    REQUIRE(U(FormatMessageW)); REQUIRE(U(FormatMessageA));
    SetLastError(0);
    EXPECT_EQ(U(FormatMessageW)(FM_SYS | FM_IGN, NULL, 5, LANG_US, NULL, 100, NULL), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SetLastError(0);
    EXPECT_EQ(U(FormatMessageA)(FM_SYS | FM_IGN, NULL, 5, LANG_US, NULL, 100, NULL), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SetLastError(0);
    EXPECT_EQ(U(FormatMessageW)(FM_STR | FM_IGN, L"abc", 0, 0, NULL, 0, NULL), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(eed_fm_size_exact_fit_w) {
    REQUIRE(U(FormatMessageW));
    WCHAR w[256]; WCHAR exact[256];
    DWORD len = fw(FM_STR | FM_IGN, L"hello world", 0, 0, NULL, w, 256);
    EXPECT_EQ(len, 11);
    for (int i = 0; i < 256; i++) exact[i] = 0x7777;
    DWORD r = fw(FM_STR | FM_IGN, L"hello world", 0, 0, NULL, exact, 12);   /* len+1: fits with NUL */
    EXPECT_EQ(r, 11); CHECK(weq(exact, "hello world"));
    EXPECT_EQ(exact[12], 0x7777);   /* nothing written beyond nSize */
}
TEST(eed_fm_size_one_short_w) {
    REQUIRE(U(FormatMessageW));
    WCHAR b[32]; for (int i = 0; i < 32; i++) b[i] = 0x7777;
    DWORD r = fw(FM_STR | FM_IGN, L"hello world", 0, 0, NULL, b, 11);
    EXPECT_EQ(r, 0); EXPECT_EQ(g_fmerr, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(b[10], 0);   /* decomp: buffer[nSize-1] = 0 on truncation */
    EXPECT_EQ(b[11], 0x7777);
}
TEST(eed_fm_size_one_short_a) {
    REQUIRE(U(FormatMessageA));
    char b[32]; memset(b, 0x55, sizeof b);
    DWORD r = fa(FM_STR | FM_IGN, "hello world", 0, 0, NULL, b, 11);
    EXPECT_EQ(r, 0); EXPECT_EQ(g_fmerr, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ((unsigned char)b[11], 0x55);   /* never writes beyond nSize */
}
TEST(eed_fm_size_exact_fit_a) {
    REQUIRE(U(FormatMessageA));
    char b[32]; memset(b, 0x55, sizeof b);
    DWORD r = fa(FM_STR | FM_IGN, "hello world", 0, 0, NULL, b, 12);
    EXPECT_EQ(r, 11); EXPECT_STR(b, "hello world"); EXPECT_EQ((unsigned char)b[12], 0x55);
}
TEST(eed_fm_size_zero_with_buffer_122) {
    REQUIRE(U(FormatMessageW)); REQUIRE(U(FormatMessageA));
    WCHAR w[4] = { 0x7777 }; char a[4] = { 0x55 };
    DWORD r = fw(FM_STR | FM_IGN, L"x", 0, 0, NULL, w, 0);
    EXPECT_EQ(r, 0); EXPECT_EQ(g_fmerr, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(w[0], 0x7777);
    r = fa(FM_STR | FM_IGN, "x", 0, 0, NULL, a, 0);
    EXPECT_EQ(r, 0); EXPECT_EQ(g_fmerr, ERROR_INSUFFICIENT_BUFFER);
}
TEST(eed_fm_size_one_char_buffer) {
    REQUIRE(U(FormatMessageW));
    WCHAR w[4] = { 0x7777, 0x7777 };
    DWORD r = fw(FM_STR | FM_IGN, L"x", 0, 0, NULL, w, 1);
    EXPECT_EQ(r, 0); EXPECT_EQ(g_fmerr, ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(w[0], 0);
    r = fw(FM_STR | FM_IGN, L"x", 0, 0, NULL, w, 2);
    EXPECT_EQ(r, 1); CHECK(weq(w, "x"));
}
TEST(eed_fm_size_system_message_truncation) {
    REQUIRE(U(FormatMessageW));
    WCHAR w[256]; WCHAR b[256];
    DWORD full = fw(FM_SYS | FM_IGN, NULL, 5, LANG_US, NULL, w, 256);
    CHECK(full > 2);
    for (DWORD n = 1; n <= full; n++) {
        for (int i = 0; i < 256; i++) b[i] = 0x7777;
        DWORD r = fw(FM_SYS | FM_IGN, NULL, 5, LANG_US, NULL, b, n);
        if (n < full + 1) { if (r != 0 || g_fmerr != ERROR_INSUFFICIENT_BUFFER) { CHECKF(0, "n=%lu r=%lu err=%lu", n, r, g_fmerr); break; } }
    }
    for (int i = 0; i < 256; i++) b[i] = 0x7777;
    DWORD r = fw(FM_SYS | FM_IGN, NULL, 5, LANG_US, NULL, b, full + 1);
    EXPECT_EQ(r, full); CHECK(!lstrcmpW(b, w));
}
TEST(eed_fm_ret_excludes_nul) {
    REQUIRE(U(FormatMessageW)); REQUIRE(U(FormatMessageA));
    WCHAR w[64]; char a[64];
    EXPECT_EQ(fw(FM_STR | FM_IGN, L"abc", 0, 0, NULL, w, 64), 3);
    EXPECT_EQ(fa(FM_STR | FM_IGN, "abcd", 0, 0, NULL, a, 64), 4);
}

/* ---- flag validation ---- */
TEST(eed_fm_flags_no_source_87) {
    REQUIRE(U(FormatMessageW)); REQUIRE(U(FormatMessageA));
    WCHAR w[64]; char a[64];
    DWORD fl[] = { 0, FM_IGN, FM_ARR, FM_IGN | FM_ARR, FORMAT_MESSAGE_MAX_WIDTH_MASK };
    for (size_t i = 0; i < sizeof fl / sizeof *fl; i++) {
        DWORD r = fw(fl[i], L"abc", 5, 0, NULL, w, 64);
        CHECKF(r == 0 && g_fmerr == ERROR_INVALID_PARAMETER, "flags 0x%lx r=%lu err=%lu", fl[i], r, g_fmerr);
        r = fa(fl[i], "abc", 5, 0, NULL, a, 64);
        CHECKF(r == 0 && g_fmerr == ERROR_INVALID_PARAMETER, "A flags 0x%lx r=%lu err=%lu", fl[i], r, g_fmerr);
    }
}
TEST(eed_fm_flags_string_wins_over_system) {
    REQUIRE(U(FormatMessageW));
    WCHAR w[64];
    DWORD r = fw(FM_STR | FM_SYS | FM_IGN, L"zzz", 5, LANG_US, NULL, w, 64);
    EXPECT_EQ(r, 3); CHECK(weq(w, "zzz"));
}
TEST(eed_fm_flags_hmodule_without_table_fails) {
    REQUIRE(U(FormatMessageW));
    WCHAR w[64];
    DWORD r = fw(FORMAT_MESSAGE_FROM_HMODULE | FM_IGN, GetModuleHandleW(NULL), 5, LANG_US, NULL, w, 64);
    EXPECT_EQ(r, 0);
    t_note("FROM_HMODULE on module without message table: err=%lu", g_fmerr);
    CHECK(g_fmerr != 0 && g_fmerr != 0xCAFE);
}
TEST(eed_fm_flags_hmodule_null_means_exe) {
    REQUIRE(U(FormatMessageW));
    WCHAR w[64];
    DWORD r = fw(FORMAT_MESSAGE_FROM_HMODULE | FM_IGN, NULL, 5, LANG_US, NULL, w, 64);
    EXPECT_EQ(r, 0);
    CHECK(g_fmerr != 0 && g_fmerr != 0xCAFE);
}
TEST(eed_fm_hmodule_ntdll_status_message) {
    REQUIRE(U(FormatMessageW));
    WCHAR w[512]; char n[512];
    DWORD r = fw(FORMAT_MESSAGE_FROM_HMODULE | FM_IGN, t_ntdll(), 0xC0000005, LANG_US, NULL, w, 512);
    CHECKF(r > 0, "err %lu", g_fmerr);
    if (r) { narrow(w, n, sizeof n); CHECK(strstr(n, "memory") != NULL); t_note("ntdll 0xC0000005: %.100s", n); }
}
TEST(eed_fm_hmodule_ntdll_common_statuses_exist) {
    REQUIRE(U(FormatMessageW));
    static const DWORD ids[] = { 0xC0000005, 0xC0000022, 0xC0000034, 0xC000003A, 0xC0000008, 0xC000000D, 0xC0000017, 0xC0000043, 0xC0000094, 0xC000001D, 0xC00000FD, 0xC0000025, 0x80000003, 0x80000004 };
    for (size_t i = 0; i < sizeof ids / sizeof *ids; i++) {
        WCHAR w[1024];
        DWORD r = fw(FORMAT_MESSAGE_FROM_HMODULE | FM_IGN, t_ntdll(), ids[i], LANG_US, NULL, w, 1024);
        CHECKF(r > 0, "ntdll message 0x%08lx missing (err %lu)", ids[i], g_fmerr);
    }
}
TEST(eed_fm_hmodule_plus_system_fallback) {
    REQUIRE(U(FormatMessageW));
    WCHAR w[512]; WCHAR s[512];
    /* id 5 is not in ntdll's NTSTATUS table: falls back to the system table. */
    DWORD r = fw(FORMAT_MESSAGE_FROM_HMODULE | FM_SYS | FM_IGN, t_ntdll(), 5, LANG_US, NULL, w, 512);
    DWORD rs = fw(FM_SYS | FM_IGN, NULL, 5, LANG_US, NULL, s, 512);
    CHECK(r > 0); EXPECT_EQ(r, rs);
    /* and an NTSTATUS id still resolves via the module when both flags are set */
    r = fw(FORMAT_MESSAGE_FROM_HMODULE | FM_SYS | FM_IGN, t_ntdll(), 0xC0000005, LANG_US, NULL, w, 512);
    CHECK(r > 0);
}
TEST(eed_fm_hmodule_kernel32_message_table) {
    REQUIRE(U(FormatMessageW));
    WCHAR w[512];
    DWORD r = fw(FORMAT_MESSAGE_FROM_HMODULE | FM_IGN, t_kernel32(), 5, LANG_US, NULL, w, 512);
    t_note("kernel32 HMODULE id 5: r=%lu err=%lu", r, r ? 0 : g_fmerr);
    r = fw(FORMAT_MESSAGE_FROM_HMODULE | FM_IGN, t_kernelbase(), 5, LANG_US, NULL, w, 512);
    t_note("kernelbase HMODULE id 5: r=%lu err=%lu", r, r ? 0 : g_fmerr);
}
TEST(eed_fm_sys_ntstatus_not_in_win32_table) {
    REQUIRE(U(FormatMessageW));
    WCHAR w[512];
    DWORD r = fw(FM_SYS | FM_IGN, NULL, 0xC0000005, LANG_US, NULL, w, 512);
    t_note("FROM_SYSTEM id 0xC0000005: r=%lu err=%lu", r, r ? 0 : g_fmerr);
    if (!r) CHECK(g_fmerr == ERROR_MR_MID_NOT_FOUND);
}

/* ---- FROM_STRING: inserts and escapes ---- */
TEST(eed_fm_str_plain_text) {
    REQUIRE(U(FormatMessageW));
    FWS("plain text", NULL, "plain text");
    FWS("a b  c", NULL, "a b  c");
    FWS("unicode-free 123", NULL, "unicode-free 123");
}
TEST(eed_fm_str_insert_string) {
    REQUIRE(U(FormatMessageW));
    DWORD_PTR a[2] = { (DWORD_PTR)L"World", (DWORD_PTR)L"again" };
    FWS("Hello %1", a, "Hello World");
    FWS("%1 %2", a, "World again");
    FWS("%2 %1", a, "again World");
    FWS("%1%1%1", a, "WorldWorldWorld");
    FWS("[%2]", a, "[again]");
}
TEST(eed_fm_str_insert_default_is_string) {
    REQUIRE(U(FormatMessageW));
    DWORD_PTR a[1] = { (DWORD_PTR)L"xyz" };
    FWS("%1", a, "xyz");
    FWS("%1!s!", a, "xyz");
    FWS("%1!ls!", a, "xyz");
}
TEST(eed_fm_str_insert_decimal) {
    REQUIRE(U(FormatMessageW));
    DWORD_PTR a[1] = { 42 };
    FWS("%1!d!", a, "42");
    FWS("%1!i!", a, "42");
    FWS("%1!u!", a, "42");
    FWS("%1!5d!", a, "   42");
    FWS("%1!-5d!|", a, "42   |");
    FWS("%1!05d!", a, "00042");
    FWS("%1!+d!", a, "+42");
    FWS("v=%1!d!.", a, "v=42.");
}
TEST(eed_fm_str_insert_negative) {
    REQUIRE(U(FormatMessageW));
    DWORD_PTR a[1] = { (DWORD_PTR)(LONG)-17 };
    FWS("%1!d!", a, "-17");
    FWS("%1!i!", a, "-17");
    FWS("%1!4d!", a, " -17");
}
TEST(eed_fm_str_insert_hex) {
    REQUIRE(U(FormatMessageW));
    DWORD_PTR a[1] = { 255 };
    FWS("%1!x!", a, "ff");
    FWS("%1!X!", a, "FF");
    FWS("%1!08x!", a, "000000ff");
    FWS("%1!#x!", a, "0xff");
    FWS("%1!o!", a, "377");
    FWS("%1!4X!", a, "  FF");
}
TEST(eed_fm_str_insert_char) {
    REQUIRE(U(FormatMessageW));
    DWORD_PTR a[1] = { 'Q' };
    FWS("%1!c!", a, "Q");
    FWS("%1!lc!", a, "Q");
    FWS("<%1!3c!>", a, "<  Q>");
}
TEST(eed_fm_str_insert_string_precision) {
    REQUIRE(U(FormatMessageW));
    DWORD_PTR a[1] = { (DWORD_PTR)L"abcdefgh" };
    FWS("%1!.3s!", a, "abc");
    FWS("%1!10s!", a, "  abcdefgh");
    FWS("%1!-10s!|", a, "abcdefgh  |");
    FWS("%1!10.3s!", a, "       abc");
    FWS("%1!.0s!x", a, "x");
}
TEST(eed_fm_str_insert_star_width_precision) {
    REQUIRE(U(FormatMessageW));
    /* documented example: width 4 / precision 2 taken from the argument array */
    DWORD_PTR a[6] = { 4, 2, (DWORD_PTR)L"Bob", (DWORD_PTR)L"Bill", 6, 7 };
    WCHAR w[128]; char n[128], e[128];
    WCHAR fmt[] = L"%1!*.*s! %4 %5!*d!";
    DWORD r = fw(FM_STR | FM_ARR, fmt, 0, 0, a, w, 128);
    narrow(w, n, sizeof n); esc(n, e, sizeof e);
    CHECKF(r > 0, "err %lu", g_fmerr);
    CHECKF(strncmp(n, "  Bo Bill", 9) == 0, "got [%s]", e);
    CHECK(n[strlen(n) - 1] == '7');
    t_note("star example: [%s]", e);
}
TEST(eed_fm_str_insert_64bit) {
    REQUIRE(U(FormatMessageW));
    DWORD_PTR a[1] = { (DWORD_PTR)0x1122334455667788ULL };
    FWS("%1!I64x!", a, "1122334455667788");
    FWS("%1!I64X!", a, "1122334455667788");
    FWS("%1!llx!", a, "1122334455667788");
    DWORD_PTR b[1] = { (DWORD_PTR)(-5LL) };
    FWS("%1!I64d!", b, "-5");
}
TEST(eed_fm_str_insert_multidigit_index) {
    REQUIRE(U(FormatMessageW));
    DWORD_PTR a[12];
    for (int i = 0; i < 12; i++) a[i] = (DWORD_PTR)(i + 1);
    FWS("%10!d! %11!d! %12!d!", a, "10 11 12");
    FWS("%1!d!%2!d!%3!d!%4!d!%5!d!%6!d!%7!d!%8!d!%9!d!", a, "123456789");
    FWS("%10!d!", a, "10");
}
TEST(eed_fm_str_escapes) {
    REQUIRE(U(FormatMessageW));
    FWS("100%%", NULL, "100%");
    FWS("a%nb", NULL, "a\r\nb");
    FWS("a%rb", NULL, "a\rb");
    FWS("a%tb", NULL, "a\tb");
    FWS("a%!b", NULL, "a!b");
    FWS("a%.b", NULL, "a.b");
    FWS("a% b", NULL, "a b");
    FWS("a%\"b", NULL, "a\"b");
}
TEST(eed_fm_str_percent0_terminates) {
    REQUIRE(U(FormatMessageW));
    FWS("abc%0def", NULL, "abc");
    FWS("done.%0", NULL, "done.");
    FWS("x%0", NULL, "x");
}
TEST(eed_fm_str_raw_linebreaks_kept_without_width) {
    REQUIRE(U(FormatMessageW));
    FWS("one\r\ntwo", NULL, "one\r\ntwo");
}
TEST(eed_fm_str_width_mask_ignores_regular_breaks) {
    REQUIRE(U(FormatMessageW));
    WCHAR w[128], f[] = L"one\r\ntwo"; char n[128];
    DWORD r = fw(FM_STR | FM_ARR | FORMAT_MESSAGE_MAX_WIDTH_MASK, f, 0, 0, NULL, w, 128);
    narrow(w, n, sizeof n);
    CHECK(r > 0); CHECK(!strchr(n, '\r')); CHECK(!strchr(n, '\n'));
    CHECK(strstr(n, "one") && strstr(n, "two"));
}
TEST(eed_fm_str_width_mask_keeps_hard_break) {
    REQUIRE(U(FormatMessageW));
    WCHAR w[128], f[] = L"a%nb"; char n[128];
    DWORD r = fw(FM_STR | FM_ARR | FORMAT_MESSAGE_MAX_WIDTH_MASK, f, 0, 0, NULL, w, 128);
    narrow(w, n, sizeof n);
    CHECK(r > 0); CHECK(strstr(n, "\r\n") != NULL);
}
TEST(eed_fm_str_width_limit_wraps) {
    REQUIRE(U(FormatMessageA));
    char a[256];
    DWORD r = fa(FM_STR | 10, "aaaa bbbb cccc dddd eeee", 0, 0, NULL, a, 256);
    CHECK(r > 0);
    const char *p = a; int ok = 1, lines = 0;
    while (*p) { const char *e = p; while (*e && *e != '\r' && *e != '\n') e++; if (e - p > 10) ok = 0; lines++; p = e; while (*p == '\r' || *p == '\n') p++; }
    CHECK(ok); CHECK(lines >= 2);
}
TEST(eed_fm_str_ignore_inserts_passthrough) {
    REQUIRE(U(FormatMessageW));
    WCHAR w[128], f[] = L"x %1 y %2!d! z"; 
    DWORD r = fw(FM_STR | FM_IGN, f, 0, 0, NULL, w, 128);
    CHECK(r > 0); CHECK(weq(w, "x %1 y %2!d! z"));
    WCHAR f2[] = L"100%% sure";
    r = fw(FM_STR | FM_IGN, f2, 0, 0, NULL, w, 128);
    { char n[128]; narrow(w, n, sizeof n); t_note("IGNORE_INSERTS with %%%%: [%s]", n); }
}
TEST(eed_fm_str_empty_string) {
    REQUIRE(U(FormatMessageW));
    WCHAR w[16] = { 0x7777 };
    DWORD r = fw(FM_STR | FM_IGN, L"", 0, 0, NULL, w, 16);
    EXPECT_EQ(r, 0);
    t_note("empty format: err=%lu w[0]=%x", g_fmerr, w[0]);
}
TEST(eed_fm_str_ansi_variant_inserts) {
    REQUIRE(U(FormatMessageA));
    char a[128];
    DWORD_PTR args[3] = { (DWORD_PTR)"ansi", 77, (DWORD_PTR)'Z' };
    DWORD r = fa(FM_STR | FM_ARR, "%1 %2!d! %3!c!", 0, 0, args, a, 128);
    EXPECT_EQ(r, 9); EXPECT_STR(a, "ansi 77 Z");
    r = fa(FM_STR | FM_ARR, "%1!s!|%2!5.2d!", 0, 0, args, a, 128);
    EXPECT_STR(a, "ansi|   77");
    r = fa(FM_STR | FM_ARR, "%1!hs!", 0, 0, args, a, 128);
    EXPECT_STR(a, "ansi");
}
TEST(eed_fm_str_wide_in_ansi_with_ws) {
    REQUIRE(U(FormatMessageA));
    char a[128];
    DWORD_PTR args[1] = { (DWORD_PTR)L"wide" };
    DWORD r = fa(FM_STR | FM_ARR, "%1!ws!", 0, 0, args, a, 128);
    EXPECT_EQ(r, 4); EXPECT_STR(a, "wide");
    r = fa(FM_STR | FM_ARR, "%1!ls!", 0, 0, args, a, 128);
    EXPECT_STR(a, "wide");
}
TEST(eed_fm_str_ansi_in_wide_with_hs) {
    REQUIRE(U(FormatMessageW));
    DWORD_PTR args[1] = { (DWORD_PTR)"narrow" };
    FWS("%1!hs!", args, "narrow");
}
TEST(eed_fm_str_ansi_format_nonascii_roundtrip) {
    REQUIRE(U(FormatMessageW));
    WCHAR f[] = { 0x00E9, L'%', L'1', 0 }; WCHAR w[16];
    DWORD_PTR args[1] = { (DWORD_PTR)L"x" };
    DWORD r = fw(FM_STR | FM_ARR, f, 0, 0, args, w, 16);
    EXPECT_EQ(r, 2); EXPECT_EQ(w[0], 0x00E9); EXPECT_EQ(w[1], L'x');
}
TEST(eed_fm_str_repeat_stability) {
    REQUIRE(U(FormatMessageW));
    for (int i = 0; i < 300; i++) {
        DWORD_PTR a[1] = { (DWORD_PTR)i };
        WCHAR w[64], f[] = L"n=%1!d!"; char exp[32];
        DWORD r = fw(FM_STR | FM_ARR, f, 0, 0, a, w, 64);
        snprintf(exp, sizeof exp, "n=%d", i);
        if (r != strlen(exp) || !weq(w, exp)) { CHECKF(0, "i=%d r=%lu", i, r); break; }
    }
}
TEST(eed_fm_str_long_literal_fits) {
    REQUIRE(U(FormatMessageA));
    char f[2049], o[4096];
    memset(f, 'k', 2048); f[2048] = 0;
    DWORD r = fa(FM_STR | FM_IGN, f, 0, 0, NULL, o, 4096);
    EXPECT_EQ(r, 2048); CHECK(!strcmp(o, f));
    r = fa(FM_STR | FM_IGN, f, 0, 0, NULL, o, 2048);
    EXPECT_EQ(r, 0); EXPECT_EQ(g_fmerr, ERROR_INSUFFICIENT_BUFFER);
}
TEST(eed_fm_str_message_id_and_lang_ignored) {
    REQUIRE(U(FormatMessageW));
    WCHAR a[64], b[64];
    fw(FM_STR | FM_IGN, L"same", 0, 0, NULL, a, 64);
    fw(FM_STR | FM_IGN, L"same", 0xFFFFFFFF, 0x1234, NULL, b, 64);
    CHECK(!lstrcmpW(a, b));
}
TEST(eed_fm_a_w_parity_system_sweep) {
    REQUIRE(U(FormatMessageW)); REQUIRE(U(FormatMessageA));
    int found = 0;
    for (DWORD id = 0; id < 400; id++) {
        WCHAR w[1024]; char a[1024], nw[1024];
        DWORD rw = fw(FM_SYS | FM_IGN, NULL, id, LANG_US, NULL, w, 1024);
        DWORD ra = fa(FM_SYS | FM_IGN, NULL, id, LANG_US, NULL, a, 1024);
        if (!rw) { if (ra) CHECKF(0, "id %lu: W missing but A present", id); else CHECKF(g_fmerr == ERROR_MR_MID_NOT_FOUND, "id %lu err %lu", id, g_fmerr); continue; }
        found++;
        narrow(w, nw, sizeof nw);
        int ascii = 1; for (DWORD i = 0; i < rw; i++) if (w[i] > 127) ascii = 0;
        if (ascii) CHECKF(ra == rw && !strcmp(a, nw), "id %lu mismatch", id);
        else CHECKF(ra > 0, "id %lu A empty", id);
    }
    CHECKF(found > 100, "only %d system messages in 0..399", found);
}
TEST(eed_fm_sys_every_message_ends_with_crlf_or_text) {
    REQUIRE(U(FormatMessageW));
    for (DWORD id = 0; id < 300; id++) {
        WCHAR w[1024];
        DWORD r = fw(FM_SYS | FM_IGN, NULL, id, LANG_US, NULL, w, 1024);
        if (!r) continue;
        CHECKF(r >= 2 && w[r - 2] == L'\r' && w[r - 1] == L'\n', "id %lu does not end in CRLF", id);
        EXPECT_EQ(r, wcslen(w));
    }
}
TEST(eed_fm_getlasterror_untouched_on_success) {
    REQUIRE(U(FormatMessageW));
    WCHAR w[64];
    SetLastError(31337);
    DWORD r = U(FormatMessageW)(FM_STR | FM_IGN, L"ok", 0, 0, w, 64, NULL);
    EXPECT_EQ(r, 2);
    t_note("FormatMessageW success leaves last error = %lu", GetLastError());
}
TEST(eed_fm_kernel32_forwarders) {
    CHECK(UK(FormatMessageA) != NULL);
    CHECK(UK(FormatMessageW) != NULL);
}
