/* gap24: LZ expand API (LZOpenFile/LZInit/LZRead/LZSeek/LZCopy/LZClose...). */
#include "t.h"
#include <lzexpand.h>
#define EQ(a, b) do { long long v_ = (long long)(a); t_check(v_ == (long long)(b), __FILE__, __LINE__, #a " == " #b, "got %lld want %lld", v_, (long long)(b)); } while (0)

static int wr(const char *path, const void *d, DWORD n) {
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    DWORD w = 0; BOOL ok = n ? WriteFile(h, d, n, &w, NULL) : TRUE; CloseHandle(h);
    return ok && w == n;
}
static int rd(const char *path, void *d, DWORD cap, DWORD *n) {
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    BOOL ok = ReadFile(h, d, cap, n, NULL); CloseHandle(h); return ok;
}
/* SZDD container, mode 'A', body made only of literal groups (control byte 0xFF + 8 literals). */
static DWORD make_szdd(BYTE *out, const char *text, DWORD len, BYTE mode) {
    static const BYTE sig[8] = {'S','Z','D','D',0x88,0xF0,0x27,0x33};
    DWORD o = 0; memcpy(out, sig, 8); o = 8; out[o++] = mode; out[o++] = 0;
    memcpy(out + o, &len, 4); o += 4;
    for (DWORD i = 0; i < len; i += 8) {
        out[o++] = 0xFF;
        for (DWORD j = 0; j < 8; j++) out[o++] = (i + j < len) ? (BYTE)text[i + j] : 0;
    }
    return o;
}
static const char TXT[] = "0123456789ABCDEFghijklmnopqrstuv"; /* 32 chars */

TEST(gap24_lz_start_done) {
    INT (WINAPI *st)(VOID) = (void *)t_proc_k32("LZStart"); REQUIRE(st);
    VOID (WINAPI *dn)(VOID) = (void *)t_proc_k32("LZDone"); REQUIRE(dn);
    for (int i = 0; i < 5; i++) { EQ(st(), 1); dn(); }
}
TEST(gap24_lz_open_missing_file) {
    REQUIRE(UK(LZOpenFileA)); REQUIRE(UK(LZOpenFileW));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "missing24.txt");
    OFSTRUCT of; memset(&of, 0, sizeof of);
    INT h = UK(LZOpenFileA)(p, &of, OF_READ);
    EQ(h, -1);
    EQ(of.nErrCode, ERROR_FILE_NOT_FOUND);
    WCHAR w[MAX_PATH]; MultiByteToWideChar(CP_ACP, 0, p, -1, w, MAX_PATH);
    memset(&of, 0, sizeof of);
    h = UK(LZOpenFileW)(w, &of, OF_READ);
    EQ(h, -1);
    EQ(of.nErrCode, ERROR_FILE_NOT_FOUND);
}
TEST(gap24_lz_open_path_too_long) {
    REQUIRE(UK(LZOpenFileA));
    char big[400]; memset(big, 'a', sizeof big - 1); big[sizeof big - 1] = 0;
    OFSTRUCT of; memset(&of, 0, sizeof of);
    INT h = UK(LZOpenFileA)(big, &of, OF_READ);
    EQ(h, -7);   /* LZERROR_BADVALUE: name does not fit the 0x106 buffer */
}
TEST(gap24_lz_plain_file_read_seek_close) {
    REQUIRE(UK(LZOpenFileA)); REQUIRE(UK(LZRead)); REQUIRE(UK(LZSeek)); REQUIRE(UK(LZClose));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "plain24.txt");
    CHECK(wr(p, TXT, 32));
    OFSTRUCT of; memset(&of, 0, sizeof of);
    INT h = UK(LZOpenFileA)(p, &of, OF_READ);
    CHECKF(h >= 0 && h < 0x400, "plain handle %d", h);
    if (h < 0) return;
    CHAR b[64]; memset(b, 0, sizeof b);
    EQ(UK(LZRead)(h, b, 10), 10); CHECK(!memcmp(b, TXT, 10));
    EQ(UK(LZSeek)(h, 0, 1), 10);
    EQ(UK(LZSeek)(h, 4, 0), 4);
    EQ(UK(LZRead)(h, b, 4), 4); CHECK(!memcmp(b, TXT + 4, 4));
    EQ(UK(LZSeek)(h, 0, 2), 32);
    EQ(UK(LZRead)(h, b, 10), 0);
    EQ(UK(LZSeek)(h, -2, 2), 30);
    EQ(UK(LZRead)(h, b, 10), 2);
    EQ(UK(LZRead)(h, b, 0), 0);
    EQ(UK(LZRead)(h, b, -1), -7);
    UK(LZClose)(h);
    CHECK(DeleteFileA(p));   /* handle really released */
}
TEST(gap24_lz_open_empty_and_tiny_files) {
    REQUIRE(UK(LZOpenFileA)); REQUIRE(UK(LZClose));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "tiny24.bin");
    const char *dat[] = {"", "S", "SZDD", "SZDD\x88\xf0\x27\x33", "not a compressed file at all"};
    DWORD len[] = {0, 1, 4, 8, 28};
    for (int i = 0; i < 5; i++) {
        CHECK(wr(p, dat[i], len[i]));
        OFSTRUCT of; INT h = UK(LZOpenFileA)(p, &of, OF_READ);
        CHECKF(h >= 0 && h < 0x400, "tiny file %d -> %d", i, h);
        if (h >= 0) UK(LZClose)(h);
    }
    DeleteFileA(p);
}
TEST(gap24_lz_compressed_stream) {
    REQUIRE(UK(LZOpenFileA)); REQUIRE(UK(LZRead)); REQUIRE(UK(LZSeek)); REQUIRE(UK(LZClose));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "comp24.tx_");
    BYTE buf[128]; DWORD n = make_szdd(buf, TXT, 32, 'A');
    CHECK(wr(p, buf, n));
    OFSTRUCT of; memset(&of, 0, sizeof of);
    INT h = UK(LZOpenFileA)(p, &of, OF_READ);
    CHECKF(h >= 0x400 && h <= 0x40f, "compressed handle %d", h);
    if (h < 0x400) { DeleteFileA(p); return; }
    EQ(UK(LZSeek)(h, 0, 2), 32);
    EQ(UK(LZSeek)(h, 0, 0), 0);
    CHAR o[64]; memset(o, 0, sizeof o);
    EQ(UK(LZRead)(h, o, 32), 32);
    CHECK(!memcmp(o, TXT, 32));
    EQ(UK(LZRead)(h, o, 8), 0);
    EQ(UK(LZSeek)(h, 5, 0), 5);
    memset(o, 0, sizeof o);
    EQ(UK(LZRead)(h, o, 6), 6); CHECK(!memcmp(o, TXT + 5, 6));
    EQ(UK(LZSeek)(h, 0, 1), 11);
    EQ(UK(LZSeek)(h, 3, 1), 14);
    EQ(UK(LZSeek)(h, -14, 1), 0);
    EQ(UK(LZSeek)(h, -1, 0), -7);      /* before start: LZERROR_BADVALUE */
    EQ(UK(LZSeek)(h, 0, 3), -7);       /* bad origin */
    EQ(UK(LZSeek)(h, 0, -1), -7);
    EQ(UK(LZRead)(h, o, -1), -7);
    EQ(UK(LZSeek)(h, 0, 2), 32);
    UK(LZClose)(h);
    CHECK(DeleteFileA(p));
}
TEST(gap24_lz_compressed_open_via_expanded_name) {
    REQUIRE(UK(LZOpenFileA)); REQUIRE(UK(LZClose));
    /* asking for "x.txt" when only "x.tx_" exists falls back to the compressed name */
    char p[MAX_PATH], q[MAX_PATH]; t_tmpfile(p, sizeof p, "fall24.tx_"); t_tmpfile(q, sizeof q, "fall24.txt");
    BYTE buf[64]; DWORD n = make_szdd(buf, TXT, 16, 'A');
    CHECK(wr(p, buf, n));
    OFSTRUCT of; memset(&of, 0, sizeof of);
    INT h = UK(LZOpenFileA)(q, &of, OF_READ);
    CHECKF(h >= 0x400, "fallback handle %d", h);
    if (h >= 0) UK(LZClose)(h);
    DeleteFileA(p);
}
TEST(gap24_lz_unknown_algorithm) {
    REQUIRE(UK(LZOpenFileA));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "alg24.tx_");
    BYTE buf[64]; DWORD n = make_szdd(buf, TXT, 16, 'B');
    CHECK(wr(p, buf, n));
    OFSTRUCT of; memset(&of, 0, sizeof of);
    EQ(UK(LZOpenFileA)(p, &of, OF_READ), -8);   /* LZERROR_UNKNOWNALG */
    CHECK(DeleteFileA(p));                              /* and nothing is left open */
}
TEST(gap24_lz_handle_table_exhaustion) {
    REQUIRE(UK(LZOpenFileA)); REQUIRE(UK(LZClose));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "tab24.tx_");
    BYTE buf[64]; DWORD n = make_szdd(buf, TXT, 16, 'A');
    CHECK(wr(p, buf, n));
    INT hs[16]; int ok = 0; OFSTRUCT of;
    for (int i = 0; i < 16; i++) {
        hs[i] = UK(LZOpenFileA)(p, &of, OF_READ);
        if (hs[i] >= 0x400 && hs[i] <= 0x40f) ok++;
        for (int j = 0; j < i; j++) CHECKF(hs[i] != hs[j], "duplicate handle");
    }
    EQ(ok, 16);
    INT extra = UK(LZOpenFileA)(p, &of, OF_READ);
    EQ(extra, -5);      /* LZERROR_GLOBALLOC: all 16 slots busy */
    for (int i = 0; i < 16; i++) if (hs[i] >= 0x400) UK(LZClose)(hs[i]);
    INT again = UK(LZOpenFileA)(p, &of, OF_READ);
    CHECK(again >= 0x400);
    if (again >= 0) UK(LZClose)(again);
    CHECK(DeleteFileA(p));
}
TEST(gap24_lz_bad_handle_values) {
    REQUIRE(UK(LZSeek)); REQUIRE(UK(LZRead));
    INT bad[] = {0x410, 0x411, 0x1000, 0x7fffffff, -1, -100, (INT)0x80000000};
    CHAR b[8];
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++)
        EQ(UK(LZSeek)(bad[i], 0, 0), -1);
    /* unused-slot reads with a negative count are rejected before the handle is used */
    EQ(UK(LZRead)(3, b, -5), -7);
    EQ(UK(LZRead)(0x405, b, -5), -7);
}
TEST(gap24_lz_copy_plain) {
    REQUIRE(UK(LZOpenFileA)); REQUIRE(UK(LZCopy)); REQUIRE(UK(LZClose));
    char s[MAX_PATH], d[MAX_PATH]; t_tmpfile(s, sizeof s, "csrc24.txt"); t_tmpfile(d, sizeof d, "cdst24.txt");
    CHECK(wr(s, TXT, 32));
    OFSTRUCT a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    INT hs = UK(LZOpenFileA)(s, &a, OF_READ);
    INT hd = UK(LZOpenFileA)(d, &b, OF_CREATE | OF_WRITE);
    CHECKF(hs >= 0 && hd >= 0, "handles %d %d", hs, hd);
    if (hs >= 0 && hd >= 0) {
        EQ(UK(LZCopy)(hs, hd), 32);
    }
    if (hs >= 0) UK(LZClose)(hs);
    if (hd >= 0) UK(LZClose)(hd);
    char got[64]; DWORD n = 0;
    CHECK(rd(d, got, sizeof got, &n)); EQ(n, 32); CHECK(!memcmp(got, TXT, 32));
    DeleteFileA(s); DeleteFileA(d);
}
TEST(gap24_lz_copy_expands_compressed) {
    REQUIRE(UK(LZOpenFileA)); REQUIRE(UK(LZCopy)); REQUIRE(UK(LZClose));
    char s[MAX_PATH], d[MAX_PATH]; t_tmpfile(s, sizeof s, "ecs24.tx_"); t_tmpfile(d, sizeof d, "ecd24.txt");
    BYTE buf[128]; DWORD n = make_szdd(buf, TXT, 32, 'A');
    CHECK(wr(s, buf, n));
    OFSTRUCT a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    INT hs = UK(LZOpenFileA)(s, &a, OF_READ);
    INT hd = UK(LZOpenFileA)(d, &b, OF_CREATE | OF_WRITE);
    CHECKF(hs >= 0x400 && hd >= 0, "handles %d %d", hs, hd);
    if (hs >= 0x400 && hd >= 0) EQ(UK(LZCopy)(hs, hd), 32);
    if (hs >= 0) UK(LZClose)(hs);
    if (hd >= 0) UK(LZClose)(hd);
    char got[64]; DWORD g = 0;
    CHECK(rd(d, got, sizeof got, &g)); EQ(g, 32); CHECK(!memcmp(got, TXT, 32));
    DeleteFileA(s); DeleteFileA(d);
}
TEST(gap24_lz_openfilew_matches_a) {
    REQUIRE(UK(LZOpenFileW)); REQUIRE(UK(LZRead)); REQUIRE(UK(LZClose));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; t_tmpfile(p, sizeof p, "wide24.tx_");
    MultiByteToWideChar(CP_ACP, 0, p, -1, w, MAX_PATH);
    BYTE buf[64]; DWORD n = make_szdd(buf, TXT, 16, 'A'); CHECK(wr(p, buf, n));
    OFSTRUCT of; memset(&of, 0, sizeof of);
    INT h = UK(LZOpenFileW)(w, &of, OF_READ);
    CHECKF(h >= 0x400, "handle %d", h);
    if (h >= 0x400) { CHAR o[32] = {0}; EQ(UK(LZRead)(h, o, 16), 16); CHECK(!memcmp(o, TXT, 16)); UK(LZClose)(h); }
    DeleteFileA(p);
}
TEST(gap24_lz_createfilew) {
    typedef INT (WINAPI *LZCreateFileW_t)(LPCWSTR, DWORD, DWORD, DWORD, LPWSTR);
    LZCreateFileW_t f = (LZCreateFileW_t)t_proc_k32("LZCreateFileW"); REQUIRE(f); REQUIRE(UK(LZClose)); VOID (WINAPI *lzcf)(INT) = (void *)t_proc_k32("LZCloseFile"); REQUIRE(lzcf); REQUIRE(UK(LZRead));
    char p[MAX_PATH]; WCHAR w[MAX_PATH], out[MAX_PATH]; t_tmpfile(p, sizeof p, "lzc24.tx_");
    MultiByteToWideChar(CP_ACP, 0, p, -1, w, MAX_PATH);
    BYTE buf[64]; DWORD n = make_szdd(buf, TXT, 16, 'A'); CHECK(wr(p, buf, n));
    /* OPEN_EXISTING (3) goes through LZInit and reports the opened name */
    memset(out, 0, sizeof out);
    INT h = f(w, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, out);
    CHECKF(h >= 0x400, "handle %d", h);
    if (h >= 0x400) { CHECK(!lstrcmpiW(out, w)); CHAR o[32] = {0}; EQ(UK(LZRead)(h, o, 16), 16); CHECK(!memcmp(o, TXT, 16)); UK(LZClose)(h); }
    h = f(w, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, NULL);
    CHECKF(h >= 0x400, "NULL name buffer handle %d", h);
    if (h >= 0x400) UK(LZClose)(h);
    DeleteFileA(p);
    /* missing file */
    char q[MAX_PATH]; WCHAR wq[MAX_PATH]; t_tmpfile(q, sizeof q, "lznone24.txt"); MultiByteToWideChar(CP_ACP, 0, q, -1, wq, MAX_PATH);
    EQ(f(wq, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, out), -1);
    /* CREATE_ALWAYS yields a plain (small) handle closed by LZCloseFile */
    h = f(wq, GENERIC_WRITE, 0, CREATE_ALWAYS, NULL);
    CHECKF(h >= 0 && h < 0x400, "create handle %d", h);
    if (h >= 0) lzcf(h);
    CHECK(DeleteFileA(q));
}
TEST(gap24_lz_createfilew_overlong_name) {
    typedef INT (WINAPI *LZCreateFileW_t)(LPCWSTR, DWORD, DWORD, DWORD, LPWSTR);
    LZCreateFileW_t f = (LZCreateFileW_t)t_proc_k32("LZCreateFileW"); REQUIRE(f);
    WCHAR big[400]; for (int i = 0; i < 399; i++) big[i] = L'b'; big[399] = 0;
    EQ(f(big, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, NULL), -7);
}
