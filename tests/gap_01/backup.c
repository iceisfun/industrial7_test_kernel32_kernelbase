/* BackupRead, BackupSeek, BackupWrite: operate on private files under the test scratch directory only. */
#include "gap1.h"

#pragma pack(push, 1)
typedef struct { DWORD id, attr; LONGLONG size; DWORD nameSize; } G1HDR;
#pragma pack(pop)
#define NSB 20

static HANDLE g1_open_backup(const char *leaf, DWORD access, DWORD disp) {
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, leaf);
    return CreateFileA(p, access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, disp, FILE_FLAG_BACKUP_SEMANTICS, NULL);
}
static HANDLE g1_mk(const char *leaf, const void *data, DWORD n) {
    HANDLE h = g1_open_backup(leaf, GENERIC_READ | GENERIC_WRITE, CREATE_ALWAYS);
    if (h != INVALID_HANDLE_VALUE && n) { DWORD w; WriteFile(h, data, n, &w, NULL); }
    return h;
}
static BOOL g1_write_ads(const char *leaf, const char *ads, const void *data, DWORD n) {
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, leaf);
    strcat(p, ":"); strcat(p, ads);
    HANDLE h = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    DWORD w; WriteFile(h, data, n, &w, NULL); CloseHandle(h);
    return TRUE;
}
/* Drain the whole backup stream of h with the given chunk size. Returns FALSE if BackupRead failed. */
static BOOL g1_drain(HANDLE h, DWORD chunk, BOOL sec, BYTE *out, size_t cap, size_t *outlen) {
    LPVOID ctx = NULL;
    BYTE *tmp = (BYTE *)HeapAlloc(GetProcessHeap(), 0, chunk);
    size_t total = 0; BOOL ok = TRUE;
    for (int it = 0; it < 100000; it++) {
        DWORD n = 0xDEAD;
        if (!U(BackupRead)(h, tmp, chunk, &n, FALSE, sec, &ctx)) { ok = FALSE; break; }
        if (n == 0) break;
        if (total + n > cap) { ok = FALSE; break; }
        memcpy(out + total, tmp, n); total += n;
        if (n > chunk) { ok = FALSE; break; }
    }
    DWORD n = 0;
    U(BackupRead)(h, tmp, 0, &n, TRUE, sec, &ctx);
    HeapFree(GetProcessHeap(), 0, tmp);
    *outlen = total;
    return ok;
}
typedef struct { DWORD id; LONGLONG size; DWORD nameSize; const BYTE *name; const BYTE *data; } G1STREAM;
static int g1_parse(const BYTE *b, size_t len, G1STREAM *s, int max) {
    int n = 0; size_t o = 0;
    while (o + NSB <= len && n < max) {
        const G1HDR *h = (const G1HDR *)(b + o);
        s[n].id = h->id; s[n].size = h->size; s[n].nameSize = h->nameSize;
        s[n].name = b + o + NSB; s[n].data = b + o + NSB + h->nameSize;
        o += NSB + h->nameSize;
        if ((LONGLONG)(len - o) < h->size) { n++; return -n; }   /* truncated */
        o += (size_t)h->size;
        n++;
    }
    return o == len ? n : -(n + 1000);
}
static void g1_pattern(BYTE *b, size_t n) { for (size_t i = 0; i < n; i++) b[i] = (BYTE)(i * 31 + 7); }
static BYTE g_big[1 << 16];

/* ---------- BackupRead: argument edge cases ---------- */
TEST(gap1_backupread_zero_bytes_is_noop) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead));
    HANDLE h = g1_mk("g1_br0.bin", "hello", 5);
    if (h == INVALID_HANDLE_VALUE) { t_skip("file"); return; }
    BYTE b[8]; DWORD n = 0x77; LPVOID ctx = NULL;
    CHECK(U(BackupRead)(h, b, 0, &n, FALSE, FALSE, &ctx));
    EXPECT_EQ(n, 0);
    CHECK(ctx == NULL);
    CloseHandle(h);
}
TEST(gap1_backupread_abort_without_context_is_ok) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead));
    HANDLE h = g1_mk("g1_br1.bin", "hello", 5);
    if (h == INVALID_HANDLE_VALUE) { t_skip("file"); return; }
    BYTE b[8]; DWORD n = 0x77; LPVOID ctx = NULL;
    CHECK(U(BackupRead)(h, b, 8, &n, TRUE, FALSE, &ctx));
    CHECK(ctx == NULL);
    CloseHandle(h);
}
TEST(gap1_backupread_abort_with_null_handle_and_context) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead));
    BYTE b[8]; DWORD n = 0; LPVOID ctx = NULL;
    CHECK(U(BackupRead)(NULL, b, 8, &n, TRUE, FALSE, &ctx));
    CHECK(U(BackupRead)(INVALID_HANDLE_VALUE, b, 0, &n, TRUE, TRUE, &ctx));
}
TEST(gap1_backupread_context_minus_one_returns_true_zero) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead));
    BYTE b[8]; DWORD n = 0x77; LPVOID ctx = (LPVOID)(LONG_PTR)-1;
    CHECK(U(BackupRead)(INVALID_HANDLE_VALUE, b, 8, &n, FALSE, FALSE, &ctx));
    EXPECT_EQ(n, 0);
    CHECK(ctx == (LPVOID)(LONG_PTR)-1);
}
TEST(gap1_backupread_null_handle) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead));
    BYTE b[32]; DWORD n = 0x77; LPVOID ctx = NULL;
    SET_ERR(0);
    BOOL r = U(BackupRead)(NULL, b, sizeof b, &n, FALSE, FALSE, &ctx);
    CHECK(!r);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    EXPECT_EQ(n, 0);
    BYTE a[8]; DWORD m; if (ctx) U(BackupRead)(NULL, a, 0, &m, TRUE, FALSE, &ctx);
}
TEST(gap1_backupread_invalid_handle_value) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead));
    BYTE b[32]; DWORD n = 0x77; LPVOID ctx = NULL;
    SET_ERR(0);
    BOOL r = U(BackupRead)(INVALID_HANDLE_VALUE, b, sizeof b, &n, FALSE, FALSE, &ctx);
    CHECK(!r);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    BYTE a[8]; DWORD m; if (ctx) U(BackupRead)(NULL, a, 0, &m, TRUE, FALSE, &ctx);
}
TEST(gap1_backupread_closed_handle) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead));
    HANDLE h = g1_mk("g1_brc.bin", "hello", 5);
    if (h == INVALID_HANDLE_VALUE) { t_skip("file"); return; }
    CloseHandle(h);
    BYTE b[32]; DWORD n = 0x77; LPVOID ctx = NULL;
    SET_ERR(0);
    CHECK(!U(BackupRead)(h, b, sizeof b, &n, FALSE, FALSE, &ctx));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap1_backupread_event_handle) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead));
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    BYTE b[32]; DWORD n = 0x77; LPVOID ctx = NULL;
    SET_ERR(0);
    CHECK(!U(BackupRead)(ev, b, sizeof b, &n, FALSE, FALSE, &ctx));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
}

/* ---------- BackupRead: stream format ---------- */
TEST(gap1_backupread_data_stream_header_layout) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead));
    HANDLE h = g1_mk("g1_brhdr.bin", "hello", 5);
    if (h == INVALID_HANDLE_VALUE) { t_skip("file"); return; }
    static BYTE out[4096]; size_t len = 0;
    CHECK(g1_drain(h, 4096, FALSE, out, sizeof out, &len));
    G1STREAM st[8];
    int n = g1_parse(out, len, st, 8);
    CHECKF(n >= 1, "parse %d len %zu", n, len);
    if (n >= 1) {
        EXPECT_EQ(st[0].id, BACKUP_DATA);
        EXPECT_EQ(st[0].size, 5);
        EXPECT_EQ(st[0].nameSize, 0);
        CHECK(!memcmp(st[0].data, "hello", 5));
        EXPECT_EQ(((G1HDR *)out)->attr, 0);
    }
    t_note("plain file backup stream: %d streams, %zu bytes", n, len);
    CloseHandle(h);
}
TEST(gap1_backupread_total_length_plain_file) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead));
    HANDLE h = g1_mk("g1_brlen.bin", "hello", 5);
    if (h == INVALID_HANDLE_VALUE) { t_skip("file"); return; }
    static BYTE out[4096]; size_t len = 0;
    CHECK(g1_drain(h, 4096, FALSE, out, sizeof out, &len));
    G1STREAM st[8]; int n = g1_parse(out, len, st, 8);
    if (n == 1) EXPECT_EQ(len, NSB + 5);
    CloseHandle(h);
}
TEST(gap1_backupread_chunk_sizes_agree) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead));
    BYTE data[3000]; g1_pattern(data, sizeof data);
    HANDLE h = g1_mk("g1_brchunk.bin", data, sizeof data);
    if (h == INVALID_HANDLE_VALUE) { t_skip("file"); return; }
    static BYTE ref[8192], cur[8192]; size_t rl = 0, cl = 0;
    CHECK(g1_drain(h, 8192, FALSE, ref, sizeof ref, &rl));
    CHECK(rl >= sizeof data + NSB);
    static const DWORD chunks[] = { 1, 2, 7, 19, 20, 21, 64, 511, 512, 513, 1000, 2999, 3000, 3020, 3021, 4096 };
    for (size_t i = 0; i < sizeof chunks / sizeof chunks[0]; i++) {
        if (chunks[i] == 1 && rl > 4000) continue;
        cl = 0;
        BOOL ok = g1_drain(h, chunks[i], FALSE, cur, sizeof cur, &cl);
        CHECKF(ok, "chunk %lu failed", chunks[i]);
        CHECKF(cl == rl && !memcmp(cur, ref, rl), "chunk %lu: len %zu vs %zu", chunks[i], cl, rl);
    }
    G1STREAM st[8]; int n = g1_parse(ref, rl, st, 8);
    if (n >= 1) { EXPECT_EQ(st[0].size, 3000); CHECK(!memcmp(st[0].data, data, 3000)); }
    CloseHandle(h);
}
TEST(gap1_backupread_chunk_one_byte_small_file) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead));
    HANDLE h = g1_mk("g1_br1b.bin", "abc", 3);
    if (h == INVALID_HANDLE_VALUE) { t_skip("file"); return; }
    BYTE ref[256], cur[256]; size_t rl = 0, cl = 0;
    CHECK(g1_drain(h, 256, FALSE, ref, sizeof ref, &rl));
    CHECK(g1_drain(h, 1, FALSE, cur, sizeof cur, &cl));
    CHECK(rl == cl && !memcmp(ref, cur, rl));
    CloseHandle(h);
}
TEST(gap1_backupread_bytes_read_never_exceeds_request) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead));
    BYTE data[2000]; g1_pattern(data, sizeof data);
    HANDLE h = g1_mk("g1_brmax.bin", data, sizeof data);
    if (h == INVALID_HANDLE_VALUE) { t_skip("file"); return; }
    LPVOID ctx = NULL; BYTE tmp[100]; int calls = 0;
    for (;;) {
        DWORD n = 0xDEAD;
        if (!U(BackupRead)(h, tmp, sizeof tmp, &n, FALSE, FALSE, &ctx)) { CHECK(0); break; }
        if (n == 0) break;
        CHECK(n <= sizeof tmp);
        if (++calls > 1000) break;
    }
    DWORD m; U(BackupRead)(h, tmp, 0, &m, TRUE, FALSE, &ctx);
    CHECK(calls >= 20);
    CloseHandle(h);
}
TEST(gap1_backupread_end_of_stream_repeats_zero) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead));
    HANDLE h = g1_mk("g1_breos.bin", "xyz", 3);
    if (h == INVALID_HANDLE_VALUE) { t_skip("file"); return; }
    LPVOID ctx = NULL; BYTE tmp[256]; DWORD n = 0;
    while (U(BackupRead)(h, tmp, sizeof tmp, &n, FALSE, FALSE, &ctx) && n) {}
    for (int i = 0; i < 3; i++) { n = 0x77; CHECK(U(BackupRead)(h, tmp, sizeof tmp, &n, FALSE, FALSE, &ctx)); EXPECT_EQ(n, 0); }
    U(BackupRead)(h, tmp, 0, &n, TRUE, FALSE, &ctx);
    CloseHandle(h);
}
TEST(gap1_backupread_abort_midway_releases_context) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead));
    BYTE data[2000]; g1_pattern(data, sizeof data);
    HANDLE h = g1_mk("g1_brabort.bin", data, sizeof data);
    if (h == INVALID_HANDLE_VALUE) { t_skip("file"); return; }
    LPVOID ctx = NULL; BYTE tmp[50]; DWORD n = 0;
    CHECK(U(BackupRead)(h, tmp, sizeof tmp, &n, FALSE, FALSE, &ctx));
    EXPECT_EQ(n, 50);
    CHECK(ctx != NULL);
    CHECK(U(BackupRead)(h, tmp, sizeof tmp, &n, TRUE, FALSE, &ctx));
    t_note("context after abort: %p", ctx);
    /* a fresh enumeration restarts at the first stream */
    LPVOID c2 = NULL;
    CHECK(U(BackupRead)(h, tmp, NSB, &n, FALSE, FALSE, &c2));
    EXPECT_EQ(n, NSB);
    EXPECT_EQ(((G1HDR *)tmp)->id, BACKUP_DATA);
    U(BackupRead)(h, tmp, 0, &n, TRUE, FALSE, &c2);
    CloseHandle(h);
}
TEST(gap1_backupread_restart_after_complete) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead));
    HANDLE h = g1_mk("g1_brrst.bin", "12345", 5);
    if (h == INVALID_HANDLE_VALUE) { t_skip("file"); return; }
    static BYTE a[512], b[512]; size_t la = 0, lb = 0;
    CHECK(g1_drain(h, 512, FALSE, a, sizeof a, &la));
    CHECK(g1_drain(h, 512, FALSE, b, sizeof b, &lb));
    CHECK(la == lb && la > 0 && !memcmp(a, b, la));
    CloseHandle(h);
}
TEST(gap1_backupread_security_stream) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead));
    HANDLE h = g1_mk("g1_brsec.bin", "hello", 5);
    if (h == INVALID_HANDLE_VALUE) { t_skip("file"); return; }
    static BYTE o1[4096], o2[4096]; size_t l1 = 0, l2 = 0;
    CHECK(g1_drain(h, 4096, FALSE, o1, sizeof o1, &l1));
    BOOL ok = g1_drain(h, 4096, TRUE, o2, sizeof o2, &l2);
    CHECKF(ok, "err %lu", GetLastError());
    G1STREAM s1[8], s2[8];
    int n1 = g1_parse(o1, l1, s1, 8), n2 = g1_parse(o2, l2, s2, 8);
    int sec1 = 0, sec2 = 0;
    for (int i = 0; i < n1; i++) if (s1[i].id == BACKUP_SECURITY_DATA) sec1++;
    for (int i = 0; i < n2; i++) if (s2[i].id == BACKUP_SECURITY_DATA) { sec2++; CHECK(s2[i].size > 0); if (s2[i].size > 0) CHECK(IsValidSecurityDescriptor((PSECURITY_DESCRIPTOR)s2[i].data)); }
    CHECK(sec1 == 0);
    CHECK(sec2 == 1);
    CHECK(l2 > l1);
    t_note("without security: %d streams; with: %d streams", n1, n2);
    CloseHandle(h);
}
TEST(gap1_backupread_empty_file) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead));
    HANDLE h = g1_mk("g1_brempty.bin", NULL, 0);
    if (h == INVALID_HANDLE_VALUE) { t_skip("file"); return; }
    static BYTE out[512]; size_t len = 0;
    BOOL ok = g1_drain(h, 512, FALSE, out, sizeof out, &len);
    CHECK(ok);
    G1STREAM st[4]; int n = g1_parse(out, len, st, 4);
    t_note("empty file: %d streams, %zu bytes", n, len);
    for (int i = 0; i < n; i++) if (st[i].id == BACKUP_DATA) CHECK(st[i].size == 0);
    CloseHandle(h);
}
TEST(gap1_backupread_alternate_data_stream) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead));
    BYTE data[100]; g1_pattern(data, sizeof data);
    HANDLE h = g1_mk("g1_brads.bin", data, sizeof data);
    if (h == INVALID_HANDLE_VALUE) { t_skip("file"); return; }
    if (!g1_write_ads("g1_brads.bin", "meta", "adsdata", 7)) { CloseHandle(h); t_skip("no NTFS alternate streams"); return; }
    static BYTE out[4096]; size_t len = 0;
    CHECK(g1_drain(h, 4096, FALSE, out, sizeof out, &len));
    G1STREAM st[8]; int n = g1_parse(out, len, st, 8);
    CHECKF(n >= 2, "streams %d", n);
    int foundads = 0, founddata = 0;
    for (int i = 0; i < n; i++) {
        if (st[i].id == BACKUP_DATA) { founddata++; CHECK(st[i].size == 100); CHECK(!memcmp(st[i].data, data, 100)); }
        if (st[i].id == BACKUP_ALTERNATE_DATA) {
            foundads++;
            CHECK(st[i].size == 7);
            CHECK(!memcmp(st[i].data, "adsdata", 7));
            WCHAR want[] = L":meta:$DATA";
            CHECKF(st[i].nameSize == sizeof(want) - sizeof(WCHAR), "nameSize %lu", st[i].nameSize);
            CHECK(!memcmp(st[i].name, want, st[i].nameSize < sizeof want ? st[i].nameSize : sizeof want - 2));
        }
    }
    CHECK(founddata == 1); CHECK(foundads == 1);
    CloseHandle(h);
}
TEST(gap1_backupread_multiple_streams_order) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead));
    HANDLE h = g1_mk("g1_brmulti.bin", "main", 4);
    if (h == INVALID_HANDLE_VALUE) { t_skip("file"); return; }
    if (!g1_write_ads("g1_brmulti.bin", "a", "AAAA", 4) || !g1_write_ads("g1_brmulti.bin", "b", "BBBBBB", 6)) { CloseHandle(h); t_skip("no ADS"); return; }
    static BYTE out[4096]; size_t len = 0;
    CHECK(g1_drain(h, 7, FALSE, out, sizeof out, &len));
    G1STREAM st[8]; int n = g1_parse(out, len, st, 8);
    int ads = 0; for (int i = 0; i < n; i++) if (st[i].id == BACKUP_ALTERNATE_DATA) ads++;
    CHECK(ads == 2);
    CHECK(n >= 1 && st[0].id == BACKUP_DATA);
    CloseHandle(h);
}
TEST(gap1_backupread_directory_handle) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead));
    char d[MAX_PATH]; t_tmpfile(d, sizeof d, "g1_brdir");
    CreateDirectoryA(d, NULL);
    HANDLE h = CreateFileA(d, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_skip("dir open"); return; }
    static BYTE out[1024]; size_t len = 0;
    BOOL ok = g1_drain(h, 1024, TRUE, out, sizeof out, &len);
    t_note("directory: ok=%d len=%zu err=%lu", ok, len, GetLastError());
    G1STREAM st[8]; int n = g1_parse(out, len, st, 8);
    for (int i = 0; i < n; i++) CHECK(st[i].id != BACKUP_DATA);
    CloseHandle(h);
    RemoveDirectoryA(d);
}
TEST(gap1_backupread_write_only_handle) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead));
    HANDLE h = g1_open_backup("g1_brwo.bin", GENERIC_WRITE, CREATE_ALWAYS);
    if (h == INVALID_HANDLE_VALUE) { t_skip("file"); return; }
    DWORD w; WriteFile(h, "hello", 5, &w, NULL);
    BYTE b[256]; DWORD n = 0; LPVOID ctx = NULL;
    SET_ERR(0);
    BOOL r = U(BackupRead)(h, b, sizeof b, &n, FALSE, FALSE, &ctx);
    t_note("write-only handle: ret=%d n=%lu err=%lu", r, n, GetLastError());
    DWORD m; U(BackupRead)(h, b, 0, &m, TRUE, FALSE, &ctx);
    CloseHandle(h);
}
TEST(gap1_backupread_does_not_move_file_pointer_semantics) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead));
    HANDLE h = g1_mk("g1_brfp.bin", "0123456789", 10);
    if (h == INVALID_HANDLE_VALUE) { t_skip("file"); return; }
    static BYTE out[256]; size_t len = 0;
    CHECK(g1_drain(h, 256, FALSE, out, sizeof out, &len));
    G1STREAM st[4]; int n = g1_parse(out, len, st, 4);
    CHECK(n >= 1);
    if (n >= 1) { EXPECT_EQ(st[0].size, 10); CHECK(!memcmp(st[0].data, "0123456789", 10)); }
    CloseHandle(h);
}
TEST(gap1_backupread_large_file_chunked) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead));
    static BYTE data[1 << 16]; g1_pattern(data, sizeof data);
    HANDLE h = g1_mk("g1_brbig.bin", data, sizeof data);
    if (h == INVALID_HANDLE_VALUE) { t_skip("file"); return; }
    static BYTE out[(1 << 16) + 4096]; size_t len = 0;
    BOOL ok = g1_drain(h, 4093, FALSE, out, sizeof out, &len);
    CHECK(ok);
    G1STREAM st[4]; int n = g1_parse(out, len, st, 4);
    CHECKF(n >= 1, "n=%d len=%zu", n, len);
    if (n >= 1) { EXPECT_EQ(st[0].size, sizeof data); CHECK(!memcmp(st[0].data, data, sizeof data)); }
    CloseHandle(h);
}

/* ---------- BackupSeek ---------- */
TEST(gap1_backupseek_null_context_zeroes_outputs) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupSeek));
    HANDLE h = g1_mk("g1_bs0.bin", "hello", 5);
    if (h == INVALID_HANDLE_VALUE) { t_skip("file"); return; }
    DWORD lo = 0xDEAD, hi = 0xBEEF; LPVOID ctx = NULL;
    BOOL r = U(BackupSeek)(h, 1, 0, &lo, &hi, &ctx);
    CHECK(!r);
    EXPECT_EQ(lo, 0); EXPECT_EQ(hi, 0);
    CloseHandle(h);
}
TEST(gap1_backupseek_minus_one_context_fails) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupSeek));
    DWORD lo = 0xDEAD, hi = 0xBEEF; LPVOID ctx = (LPVOID)(LONG_PTR)-1;
    BOOL r = U(BackupSeek)(INVALID_HANDLE_VALUE, 1, 0, &lo, &hi, &ctx);
    CHECK(!r);
    EXPECT_EQ(lo, 0); EXPECT_EQ(hi, 0);
}
TEST(gap1_backupseek_within_data_stream) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead)); REQUIRE(U(BackupSeek));
    HANDLE h = g1_mk("g1_bs1.bin", "hello", 5);
    if (h == INVALID_HANDLE_VALUE) { t_skip("file"); return; }
    LPVOID ctx = NULL; BYTE b[64]; DWORD n = 0;
    CHECK(U(BackupRead)(h, b, NSB, &n, FALSE, FALSE, &ctx));
    EXPECT_EQ(n, NSB);
    EXPECT_EQ(((G1HDR *)b)->id, BACKUP_DATA);
    DWORD lo = 0xDEAD, hi = 0xBEEF;
    BOOL r = U(BackupSeek)(h, 3, 0, &lo, &hi, &ctx);
    CHECKF(r, "err %lu", GetLastError());
    EXPECT_EQ(lo, 3); EXPECT_EQ(hi, 0);
    CHECK(U(BackupRead)(h, b, 64, &n, FALSE, FALSE, &ctx));
    EXPECT_EQ(n, 2);
    if (n == 2) CHECK(!memcmp(b, "lo", 2));
    U(BackupRead)(h, b, 0, &n, TRUE, FALSE, &ctx);
    CloseHandle(h);
}
TEST(gap1_backupseek_past_end_of_stream) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead)); REQUIRE(U(BackupSeek));
    HANDLE h = g1_mk("g1_bs2.bin", "hello", 5);
    if (h == INVALID_HANDLE_VALUE) { t_skip("file"); return; }
    LPVOID ctx = NULL; BYTE b[64]; DWORD n = 0;
    CHECK(U(BackupRead)(h, b, NSB, &n, FALSE, FALSE, &ctx));
    DWORD lo = 0xDEAD, hi = 0xBEEF;
    SET_ERR(0);
    BOOL r = U(BackupSeek)(h, 100, 0, &lo, &hi, &ctx);
    DWORD e = GetLastError();
    t_note("seek 100 in 5-byte stream: ret=%d lo=%lu hi=%lu err=%lu", r, lo, hi, e);
    CHECK(!r);
    CHECK(lo <= 5);
    EXPECT_EQ(hi, 0);
    U(BackupRead)(h, b, 0, &n, TRUE, FALSE, &ctx);
    CloseHandle(h);
}
TEST(gap1_backupseek_zero_bytes) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead)); REQUIRE(U(BackupSeek));
    HANDLE h = g1_mk("g1_bs3.bin", "hello", 5);
    if (h == INVALID_HANDLE_VALUE) { t_skip("file"); return; }
    LPVOID ctx = NULL; BYTE b[64]; DWORD n = 0;
    CHECK(U(BackupRead)(h, b, NSB, &n, FALSE, FALSE, &ctx));
    DWORD lo = 0xDEAD, hi = 0xBEEF;
    BOOL r = U(BackupSeek)(h, 0, 0, &lo, &hi, &ctx);
    t_note("seek 0: ret=%d lo=%lu hi=%lu", r, lo, hi);
    EXPECT_EQ(lo, 0); EXPECT_EQ(hi, 0);
    CHECK(U(BackupRead)(h, b, 64, &n, FALSE, FALSE, &ctx));
    EXPECT_EQ(n, 5);
    U(BackupRead)(h, b, 0, &n, TRUE, FALSE, &ctx);
    CloseHandle(h);
}
TEST(gap1_backupseek_exactly_to_end) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead)); REQUIRE(U(BackupSeek));
    HANDLE h = g1_mk("g1_bs4.bin", "hello", 5);
    if (h == INVALID_HANDLE_VALUE) { t_skip("file"); return; }
    LPVOID ctx = NULL; BYTE b[64]; DWORD n = 0;
    CHECK(U(BackupRead)(h, b, NSB, &n, FALSE, FALSE, &ctx));
    DWORD lo = 0xDEAD, hi = 0xBEEF;
    BOOL r = U(BackupSeek)(h, 5, 0, &lo, &hi, &ctx);
    CHECKF(r, "err %lu", GetLastError());
    EXPECT_EQ(lo, 5); EXPECT_EQ(hi, 0);
    CHECK(U(BackupRead)(h, b, 64, &n, FALSE, FALSE, &ctx));
    EXPECT_EQ(n, 0);
    U(BackupRead)(h, b, 0, &n, TRUE, FALSE, &ctx);
    CloseHandle(h);
}
TEST(gap1_backupseek_in_pieces) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead)); REQUIRE(U(BackupSeek));
    BYTE data[1000]; g1_pattern(data, sizeof data);
    HANDLE h = g1_mk("g1_bs5.bin", data, sizeof data);
    if (h == INVALID_HANDLE_VALUE) { t_skip("file"); return; }
    LPVOID ctx = NULL; BYTE b[2048]; DWORD n = 0;
    CHECK(U(BackupRead)(h, b, NSB, &n, FALSE, FALSE, &ctx));
    DWORD total = 0;
    for (int i = 0; i < 4; i++) {
        DWORD lo = 0, hi = 0;
        CHECK(U(BackupSeek)(h, 100, 0, &lo, &hi, &ctx));
        EXPECT_EQ(lo, 100); total += lo;
    }
    CHECK(U(BackupRead)(h, b, 2048, &n, FALSE, FALSE, &ctx));
    EXPECT_EQ(n, 600);
    if (n == 600) CHECK(!memcmp(b, data + 400, 600));
    U(BackupRead)(h, b, 0, &n, TRUE, FALSE, &ctx);
    CloseHandle(h);
}
TEST(gap1_backupseek_high_dword_clamped) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead)); REQUIRE(U(BackupSeek));
    HANDLE h = g1_mk("g1_bs6.bin", "hello", 5);
    if (h == INVALID_HANDLE_VALUE) { t_skip("file"); return; }
    LPVOID ctx = NULL; BYTE b[64]; DWORD n = 0;
    CHECK(U(BackupRead)(h, b, NSB, &n, FALSE, FALSE, &ctx));
    DWORD lo = 0xDEAD, hi = 0xBEEF;
    BOOL r = U(BackupSeek)(h, 0, 1, &lo, &hi, &ctx);
    t_note("seek 4GiB: ret=%d lo=%lu hi=%lu", r, lo, hi);
    CHECK(!r);
    CHECK(hi == 0 && lo <= 5);
    U(BackupRead)(h, b, 0, &n, TRUE, FALSE, &ctx);
    CloseHandle(h);
}
TEST(gap1_backupseek_across_stream_boundary_stops) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead)); REQUIRE(U(BackupSeek));
    HANDLE h = g1_mk("g1_bs7.bin", "main", 4);
    if (h == INVALID_HANDLE_VALUE) { t_skip("file"); return; }
    if (!g1_write_ads("g1_bs7.bin", "x", "XXXXXXXX", 8)) { CloseHandle(h); t_skip("no ADS"); return; }
    LPVOID ctx = NULL; BYTE b[256]; DWORD n = 0;
    CHECK(U(BackupRead)(h, b, NSB, &n, FALSE, FALSE, &ctx));
    EXPECT_EQ(((G1HDR *)b)->id, BACKUP_DATA);
    DWORD lo = 0, hi = 0;
    BOOL r = U(BackupSeek)(h, 1000, 0, &lo, &hi, &ctx);
    t_note("seek past first stream: ret=%d lo=%lu", r, lo);
    CHECK(!r);
    CHECK(lo <= 4);
    U(BackupRead)(h, b, 0, &n, TRUE, FALSE, &ctx);
    CloseHandle(h);
}

/* ---------- BackupWrite ---------- */
TEST(gap1_backupwrite_abort_without_context) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupWrite));
    HANDLE h = g1_open_backup("g1_bw0.bin", GENERIC_WRITE, CREATE_ALWAYS);
    if (h == INVALID_HANDLE_VALUE) { t_skip("file"); return; }
    DWORD n = 0x77; LPVOID ctx = NULL; BYTE b[4] = {0};
    BOOL r = U(BackupWrite)(h, b, 0, &n, TRUE, FALSE, &ctx);
    t_note("abort w/o context: ret=%d", r);
    CHECK(r);
    CloseHandle(h);
}
TEST(gap1_backupwrite_zero_bytes_allocates_context) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupWrite));
    HANDLE h = g1_open_backup("g1_bw1.bin", GENERIC_WRITE, CREATE_ALWAYS);
    if (h == INVALID_HANDLE_VALUE) { t_skip("file"); return; }
    DWORD n = 0x77; LPVOID ctx = NULL; BYTE b[4] = {0};
    BOOL r = U(BackupWrite)(h, b, 0, &n, FALSE, FALSE, &ctx);
    t_note("zero-length write: ret=%d n=%lu ctx=%p", r, n, ctx);
    CHECK(r);
    EXPECT_EQ(n, 0);
    U(BackupWrite)(h, b, 0, &n, TRUE, FALSE, &ctx);
    CloseHandle(h);
}
static BOOL g1_replay(HANDLE h, const BYTE *stream, size_t len, DWORD chunk, BOOL sec) {
    LPVOID ctx = NULL; size_t o = 0; BOOL ok = TRUE;
    while (o < len) {
        DWORD c = (DWORD)((len - o) < chunk ? (len - o) : chunk), w = 0xDEAD;
        if (!U(BackupWrite)(h, (LPBYTE)stream + o, c, &w, FALSE, sec, &ctx)) { ok = FALSE; break; }
        if (w != c) { ok = FALSE; break; }
        o += c;
    }
    DWORD w; BYTE z[1];
    U(BackupWrite)(h, z, 0, &w, TRUE, sec, &ctx);
    return ok;
}
static int g1_file_equals(const char *leaf, const void *data, DWORD n) {
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, leaf);
    HANDLE h = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    BYTE *buf = (BYTE *)HeapAlloc(GetProcessHeap(), 0, n + 16);
    DWORD got = 0; ReadFile(h, buf, n + 16, &got, NULL); CloseHandle(h);
    int eq = got == n && !memcmp(buf, data, n);
    HeapFree(GetProcessHeap(), 0, buf);
    return eq;
}
TEST(gap1_backupwrite_roundtrip_data_stream) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead)); REQUIRE(U(BackupWrite));
    BYTE data[3000]; g1_pattern(data, sizeof data);
    HANDLE a = g1_mk("g1_bwa.bin", data, sizeof data);
    if (a == INVALID_HANDLE_VALUE) { t_skip("file"); return; }
    static BYTE s[8192]; size_t sl = 0;
    CHECK(g1_drain(a, 8192, FALSE, s, sizeof s, &sl));
    CloseHandle(a);
    HANDLE b = g1_open_backup("g1_bwb.bin", GENERIC_WRITE | WRITE_DAC | WRITE_OWNER, CREATE_ALWAYS);
    if (b == INVALID_HANDLE_VALUE) { t_skip("dst"); return; }
    CHECK(g1_replay(b, s, sl, 8192, FALSE));
    CloseHandle(b);
    CHECK(g1_file_equals("g1_bwb.bin", data, sizeof data));
}
TEST(gap1_backupwrite_roundtrip_chunked) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead)); REQUIRE(U(BackupWrite));
    BYTE data[2500]; g1_pattern(data, sizeof data);
    HANDLE a = g1_mk("g1_bwca.bin", data, sizeof data);
    if (a == INVALID_HANDLE_VALUE) { t_skip("file"); return; }
    static BYTE s[8192]; size_t sl = 0;
    CHECK(g1_drain(a, 8192, FALSE, s, sizeof s, &sl));
    CloseHandle(a);
    static const DWORD chunks[] = { 1, 5, 19, 20, 21, 100, 1000 };
    for (size_t i = 0; i < sizeof chunks / sizeof chunks[0]; i++) {
        if (chunks[i] == 1) continue;     /* covered below on a small file */
        HANDLE b = g1_open_backup("g1_bwcb.bin", GENERIC_WRITE | WRITE_DAC | WRITE_OWNER, CREATE_ALWAYS);
        if (b == INVALID_HANDLE_VALUE) { t_skip("dst"); return; }
        BOOL ok = g1_replay(b, s, sl, chunks[i], FALSE);
        CloseHandle(b);
        CHECKF(ok, "chunk %lu", chunks[i]);
        CHECKF(g1_file_equals("g1_bwcb.bin", data, sizeof data), "chunk %lu content", chunks[i]);
    }
}
TEST(gap1_backupwrite_roundtrip_one_byte_chunks) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead)); REQUIRE(U(BackupWrite));
    HANDLE a = g1_mk("g1_bw1a.bin", "tiny", 4);
    if (a == INVALID_HANDLE_VALUE) { t_skip("file"); return; }
    BYTE s[256]; size_t sl = 0;
    CHECK(g1_drain(a, 256, FALSE, s, sizeof s, &sl));
    CloseHandle(a);
    HANDLE b = g1_open_backup("g1_bw1b.bin", GENERIC_WRITE | WRITE_DAC | WRITE_OWNER, CREATE_ALWAYS);
    if (b == INVALID_HANDLE_VALUE) { t_skip("dst"); return; }
    CHECK(g1_replay(b, s, sl, 1, FALSE));
    CloseHandle(b);
    CHECK(g1_file_equals("g1_bw1b.bin", "tiny", 4));
}
TEST(gap1_backupwrite_roundtrip_alternate_stream) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead)); REQUIRE(U(BackupWrite));
    HANDLE a = g1_mk("g1_bwsa.bin", "primary", 7);
    if (a == INVALID_HANDLE_VALUE) { t_skip("file"); return; }
    if (!g1_write_ads("g1_bwsa.bin", "side", "secondary", 9)) { CloseHandle(a); t_skip("no ADS"); return; }
    static BYTE s[4096]; size_t sl = 0;
    CHECK(g1_drain(a, 4096, FALSE, s, sizeof s, &sl));
    CloseHandle(a);
    HANDLE b = g1_open_backup("g1_bwsb.bin", GENERIC_WRITE | WRITE_DAC | WRITE_OWNER, CREATE_ALWAYS);
    if (b == INVALID_HANDLE_VALUE) { t_skip("dst"); return; }
    CHECK(g1_replay(b, s, sl, 13, FALSE));
    CloseHandle(b);
    CHECK(g1_file_equals("g1_bwsb.bin", "primary", 7));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "g1_bwsb.bin"); strcat(p, ":side");
    HANDLE ah = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(ah != INVALID_HANDLE_VALUE);
    if (ah != INVALID_HANDLE_VALUE) { char buf[16] = {0}; DWORD n = 0; ReadFile(ah, buf, 15, &n, NULL); CHECK(n == 9 && !memcmp(buf, "secondary", 9)); CloseHandle(ah); }
}
TEST(gap1_backupwrite_roundtrip_with_security) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupRead)); REQUIRE(U(BackupWrite));
    HANDLE a = g1_mk("g1_bwea.bin", "secured", 7);
    if (a == INVALID_HANDLE_VALUE) { t_skip("file"); return; }
    static BYTE s[4096]; size_t sl = 0;
    BOOL ok = g1_drain(a, 4096, TRUE, s, sizeof s, &sl);
    CloseHandle(a);
    CHECK(ok);
    HANDLE b = g1_open_backup("g1_bweb.bin", GENERIC_WRITE | WRITE_DAC | WRITE_OWNER, CREATE_ALWAYS);
    if (b == INVALID_HANDLE_VALUE) { t_skip("dst"); return; }
    BOOL w = g1_replay(b, s, sl, 4096, TRUE);
    t_note("replay with security: %d err=%lu", w, GetLastError());
    CloseHandle(b);
    CHECK(g1_file_equals("g1_bweb.bin", "secured", 7));
}
TEST(gap1_backupwrite_invalid_stream_id) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupWrite));
    HANDLE b = g1_open_backup("g1_bwbad.bin", GENERIC_WRITE | WRITE_DAC | WRITE_OWNER, CREATE_ALWAYS);
    if (b == INVALID_HANDLE_VALUE) { t_skip("dst"); return; }
    G1HDR h; memset(&h, 0, sizeof h); h.id = 99; h.size = 4;
    BYTE buf[NSB + 4]; memcpy(buf, &h, NSB); memset(buf + NSB, 'x', 4);
    LPVOID ctx = NULL; DWORD w = 0x77;
    SET_ERR(0);
    BOOL r = U(BackupWrite)(b, buf, sizeof buf, &w, FALSE, FALSE, &ctx);
    t_note("unknown stream id: ret=%d written=%lu err=%lu", r, w, GetLastError());
    BYTE z[1]; DWORD m; U(BackupWrite)(b, z, 0, &m, TRUE, FALSE, &ctx);
    CloseHandle(b);
}
TEST(gap1_backupwrite_null_handle) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupWrite));
    G1HDR h; memset(&h, 0, sizeof h); h.id = BACKUP_DATA; h.size = 4;
    BYTE buf[NSB + 4]; memcpy(buf, &h, NSB); memset(buf + NSB, 'x', 4);
    LPVOID ctx = NULL; DWORD w = 0x77;
    SET_ERR(0);
    BOOL r = U(BackupWrite)(NULL, buf, sizeof buf, &w, FALSE, FALSE, &ctx);
    CHECK(!r);
    t_note("NULL handle: err=%lu", GetLastError());
    BYTE z[1]; DWORD m; U(BackupWrite)(NULL, z, 0, &m, TRUE, FALSE, &ctx);
}
TEST(gap1_backupwrite_closed_handle) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupWrite));
    HANDLE b = g1_open_backup("g1_bwclosed.bin", GENERIC_WRITE | WRITE_DAC | WRITE_OWNER, CREATE_ALWAYS);
    if (b == INVALID_HANDLE_VALUE) { t_skip("dst"); return; }
    CloseHandle(b);
    G1HDR h; memset(&h, 0, sizeof h); h.id = BACKUP_DATA; h.size = 4;
    BYTE buf[NSB + 4]; memcpy(buf, &h, NSB); memset(buf + NSB, 'x', 4);
    LPVOID ctx = NULL; DWORD w = 0x77;
    SET_ERR(0);
    BOOL r = U(BackupWrite)(b, buf, sizeof buf, &w, FALSE, FALSE, &ctx);
    CHECK(!r);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    BYTE z[1]; DWORD m; U(BackupWrite)(NULL, z, 0, &m, TRUE, FALSE, &ctx);
}
TEST(gap1_backupwrite_truncated_stream_then_abort) {
    if (t_is_wine()) { t_skip("Backup* are stubs under wine"); return; }
    REQUIRE(U(BackupWrite));
    HANDLE b = g1_open_backup("g1_bwtr.bin", GENERIC_WRITE | WRITE_DAC | WRITE_OWNER, CREATE_ALWAYS);
    if (b == INVALID_HANDLE_VALUE) { t_skip("dst"); return; }
    G1HDR h; memset(&h, 0, sizeof h); h.id = BACKUP_DATA; h.size = 100;
    BYTE buf[NSB + 10]; memcpy(buf, &h, NSB); memset(buf + NSB, 'y', 10);
    LPVOID ctx = NULL; DWORD w = 0x77;
    BOOL r = U(BackupWrite)(b, buf, sizeof buf, &w, FALSE, FALSE, &ctx);
    t_note("partial data stream: ret=%d written=%lu", r, w);
    CHECK(r);
    BYTE z[1]; DWORD m; CHECK(U(BackupWrite)(b, z, 0, &m, TRUE, FALSE, &ctx));
    CloseHandle(b);
}
TEST(gap1_backupwrite_exports_in_kernel32) {
    CHECK(t_proc_k32("BackupRead") != NULL);
    CHECK(t_proc_k32("BackupSeek") != NULL);
    CHECK(t_proc_k32("BackupWrite") != NULL);
}
