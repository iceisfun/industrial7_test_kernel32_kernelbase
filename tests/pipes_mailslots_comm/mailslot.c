/* Mailslots: CreateMailslot, GetMailslotInfo, SetMailslotInfo, client writes. */
#include "pmc.h"

#define SLOT(n) char n[128]; pmc_slotname(n, sizeof n)
static HANDLE slot_client(const char *nm) { return CreateFileA(nm, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL); }

TEST(pmc_ms_create_basic) {
    REQUIRE(U(CreateMailslotA));
    SLOT(nm);
    HANDLE h = U(CreateMailslotA)(nm, 0, MAILSLOT_WAIT_FOREVER, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    EXPECT_EQ(GetFileType(h), FILE_TYPE_UNKNOWN);   /* mailslot device type */
    CloseHandle(h);
}
TEST(pmc_ms_create_wide) {
    REQUIRE(U(CreateMailslotW));
    WCHAR nm[128]; pmc_slotnamew(nm, 128);
    HANDLE h = U(CreateMailslotW)(nm, 0, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    HANDLE c = CreateFileW(nm, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(c != INVALID_HANDLE_VALUE);
    DWORD n; CHECK(WriteFile(c, "w", 1, &n, NULL));
    char b[4]; CHECK(ReadFile(h, b, 4, &n, NULL)); EXPECT_EQ(n, 1);
    CloseHandle(c); CloseHandle(h);
}
TEST(pmc_ms_filetype) {
    REQUIRE(U(CreateMailslotA));
    SLOT(nm);
    HANDLE h = U(CreateMailslotA)(nm, 0, 0, NULL);
    HANDLE c = slot_client(nm);
    DWORD t1 = GetFileType(h), t2 = GetFileType(c);
    t_note("GetFileType mailslot server=%lu client=%lu", t1, t2);
    EXPECT_EQ(t1, t2);
    CloseHandle(c); CloseHandle(h);
}
TEST(pmc_ms_empty_name) {
    REQUIRE(U(CreateMailslotA)); REQUIRE(U(CreateMailslotW));
    SET_ERR(0); CHECK(U(CreateMailslotA)("", 0, 0, NULL) == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    SET_ERR(0); CHECK(U(CreateMailslotW)(L"", 0, 0, NULL) == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(pmc_ms_bad_name_prefix) {
    REQUIRE(U(CreateMailslotA));
    char nm[64]; snprintf(nm, sizeof nm, "k32t_slot_%lu", GetCurrentProcessId());
    SET_ERR(0);
    HANDLE h = U(CreateMailslotA)(nm, 0, 0, NULL);
    CHECK(h == INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) EXPECT_ERR(ERROR_INVALID_NAME); else CloseHandle(h);
    DeleteFileA(nm);
}
TEST(pmc_ms_pipe_prefix_rejected) {
    REQUIRE(U(CreateMailslotA));
    char nm[128]; pmc_pipename(nm, sizeof nm);
    SET_ERR(0);
    HANDLE h = U(CreateMailslotA)(nm, 0, 0, NULL);
    t_note("CreateMailslot on a \\\\.\\pipe\\ name: %s err=%lu", h != INVALID_HANDLE_VALUE ? "ok" : "fail", GetLastError());
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
}
TEST(pmc_ms_duplicate_name) {
    REQUIRE(U(CreateMailslotA));
    SLOT(nm);
    HANDLE a = U(CreateMailslotA)(nm, 0, 0, NULL);
    SET_ERR(0);
    HANDLE b = U(CreateMailslotA)(nm, 0, 0, NULL);
    CHECK(b == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_ALREADY_EXISTS);
    CloseHandle(a);
    b = U(CreateMailslotA)(nm, 0, 0, NULL);
    CHECK(b != INVALID_HANDLE_VALUE);   /* free again after close */
    if (b != INVALID_HANDLE_VALUE) CloseHandle(b);
}
TEST(pmc_ms_name_case_insensitive) {
    REQUIRE(U(CreateMailslotA));
    SLOT(nm);
    HANDLE a = U(CreateMailslotA)(nm, 0, 0, NULL);
    char up[128]; strcpy(up, nm); for (char *p = up; *p; p++) *p = (*p >= 'a' && *p <= 'z') ? *p - 32 : *p;
    SET_ERR(0);
    HANDLE b = U(CreateMailslotA)(up, 0, 0, NULL);
    CHECK(b == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_ALREADY_EXISTS);
    HANDLE c = slot_client(up);
    CHECK(c != INVALID_HANDLE_VALUE);
    if (c != INVALID_HANDLE_VALUE) CloseHandle(c);
    CloseHandle(a);
}
TEST(pmc_ms_info_empty) {
    REQUIRE(U(CreateMailslotA)); REQUIRE(U(GetMailslotInfo));
    SLOT(nm);
    HANDLE h = U(CreateMailslotA)(nm, 100, 250, NULL);
    DWORD mx = 0, next = 0, cnt = 99, to = 0;
    CHECK(U(GetMailslotInfo)(h, &mx, &next, &cnt, &to));
    EXPECT_EQ(mx, 100); EXPECT_EQ(next, MAILSLOT_NO_MESSAGE); EXPECT_EQ(cnt, 0); EXPECT_EQ(to, 250);
    CloseHandle(h);
}
TEST(pmc_ms_info_all_null) {
    REQUIRE(U(CreateMailslotA)); REQUIRE(U(GetMailslotInfo));
    SLOT(nm);
    HANDLE h = U(CreateMailslotA)(nm, 0, 0, NULL);
    CHECK(U(GetMailslotInfo)(h, NULL, NULL, NULL, NULL));
    CloseHandle(h);
}
TEST(pmc_ms_info_each_output_alone) {
    REQUIRE(U(CreateMailslotA)); REQUIRE(U(GetMailslotInfo));
    SLOT(nm);
    HANDLE h = U(CreateMailslotA)(nm, 64, 10, NULL);
    DWORD v = 0;
    CHECK(U(GetMailslotInfo)(h, &v, NULL, NULL, NULL)); EXPECT_EQ(v, 64);
    CHECK(U(GetMailslotInfo)(h, NULL, &v, NULL, NULL)); EXPECT_EQ(v, MAILSLOT_NO_MESSAGE);
    CHECK(U(GetMailslotInfo)(h, NULL, NULL, &v, NULL)); EXPECT_EQ(v, 0);
    CHECK(U(GetMailslotInfo)(h, NULL, NULL, NULL, &v)); EXPECT_EQ(v, 10);
    CloseHandle(h);
}
TEST(pmc_ms_info_unlimited_size) {
    REQUIRE(U(CreateMailslotA)); REQUIRE(U(GetMailslotInfo));
    SLOT(nm);
    HANDLE h = U(CreateMailslotA)(nm, 0, 0, NULL);
    DWORD mx = 77; CHECK(U(GetMailslotInfo)(h, &mx, NULL, NULL, NULL)); EXPECT_EQ(mx, 0);
    CloseHandle(h);
}
TEST(pmc_ms_info_timeouts_roundtrip) {
    REQUIRE(U(CreateMailslotA)); REQUIRE(U(GetMailslotInfo));
    DWORD tv[] = { 0, 1, 100, 5000, MAILSLOT_WAIT_FOREVER };
    for (int i = 0; i < 5; i++) {
        SLOT(nm);
        HANDLE h = U(CreateMailslotA)(nm, 0, tv[i], NULL);
        DWORD to = 77; CHECK(U(GetMailslotInfo)(h, NULL, NULL, NULL, &to));
        CHECKF(to == tv[i], "timeout %lu read back %lu", tv[i], to);
        CloseHandle(h);
    }
}
TEST(pmc_ms_set_timeout) {
    REQUIRE(U(CreateMailslotA)); REQUIRE(U(GetMailslotInfo)); REQUIRE(U(SetMailslotInfo));
    SLOT(nm);
    HANDLE h = U(CreateMailslotA)(nm, 0, 0, NULL);
    DWORD tv[] = { 123, MAILSLOT_WAIT_FOREVER, 0, 1 };
    for (int i = 0; i < 4; i++) {
        CHECK(U(SetMailslotInfo)(h, tv[i]));
        DWORD to = 77; CHECK(U(GetMailslotInfo)(h, NULL, NULL, NULL, &to)); EXPECT_EQ(to, tv[i]);
    }
    CloseHandle(h);
}
TEST(pmc_ms_set_invalid_handle) {
    REQUIRE(U(SetMailslotInfo)); REQUIRE(U(GetMailslotInfo));
    SET_ERR(0); CHECK(!U(SetMailslotInfo)(NULL, 5)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    DWORD v;
    SET_ERR(0); CHECK(!U(GetMailslotInfo)(NULL, &v, NULL, NULL, NULL)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(SetMailslotInfo)(INVALID_HANDLE_VALUE, 5)); CHECK(GetLastError() != 0);
    SET_ERR(0); CHECK(!U(GetMailslotInfo)(INVALID_HANDLE_VALUE, &v, NULL, NULL, NULL)); CHECK(GetLastError() != 0);
}
TEST(pmc_ms_info_on_file_handle) {
    REQUIRE(U(GetMailslotInfo)); REQUIRE(U(SetMailslotInfo));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "pmc_ms.bin");
    HANDLE h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD v;
    SET_ERR(0); CHECK(!U(GetMailslotInfo)(h, &v, NULL, NULL, NULL)); CHECK(GetLastError() != 0);
    SET_ERR(0); CHECK(!U(SetMailslotInfo)(h, 5)); CHECK(GetLastError() != 0);
    CloseHandle(h); DeleteFileA(p);
}
TEST(pmc_ms_info_on_pipe_handle) {
    REQUIRE(U(GetMailslotInfo));
    HANDLE r, w; CreatePipe(&r, &w, NULL, 0);
    DWORD v;
    SET_ERR(0); CHECK(!U(GetMailslotInfo)(r, &v, NULL, NULL, NULL)); CHECK(GetLastError() != 0);
    CloseHandle(r); CloseHandle(w);
}
TEST(pmc_ms_info_after_close) {
    REQUIRE(U(CreateMailslotA)); REQUIRE(U(GetMailslotInfo));
    SLOT(nm);
    HANDLE h = U(CreateMailslotA)(nm, 0, 0, NULL);
    CloseHandle(h);
    DWORD v;
    SET_ERR(0); CHECK(!U(GetMailslotInfo)(h, &v, NULL, NULL, NULL)); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(pmc_ms_message_roundtrip) {
    REQUIRE(U(CreateMailslotA));
    SLOT(nm);
    HANDLE h = U(CreateMailslotA)(nm, 0, 0, NULL);
    HANDLE c = slot_client(nm);
    CHECK(c != INVALID_HANDLE_VALUE);
    DWORD n; char b[64];
    CHECK(WriteFile(c, "message one", 11, &n, NULL)); EXPECT_EQ(n, 11);
    CHECK(ReadFile(h, b, 64, &n, NULL)); EXPECT_EQ(n, 11); CHECK(!memcmp(b, "message one", 11));
    CloseHandle(c); CloseHandle(h);
}
TEST(pmc_ms_one_message_per_read) {
    REQUIRE(U(CreateMailslotA));
    SLOT(nm);
    HANDLE h = U(CreateMailslotA)(nm, 0, 0, NULL);
    HANDLE c = slot_client(nm);
    DWORD n; char b[64];
    CHECK(WriteFile(c, "aaa", 3, &n, NULL)); CHECK(WriteFile(c, "bbbbb", 5, &n, NULL)); CHECK(WriteFile(c, "c", 1, &n, NULL));
    CHECK(ReadFile(h, b, 64, &n, NULL)); EXPECT_EQ(n, 3); CHECK(!memcmp(b, "aaa", 3));
    CHECK(ReadFile(h, b, 64, &n, NULL)); EXPECT_EQ(n, 5); CHECK(!memcmp(b, "bbbbb", 5));
    CHECK(ReadFile(h, b, 64, &n, NULL)); EXPECT_EQ(n, 1); CHECK(b[0] == 'c');
    CloseHandle(c); CloseHandle(h);
}
TEST(pmc_ms_info_tracks_queue) {
    REQUIRE(U(CreateMailslotA)); REQUIRE(U(GetMailslotInfo));
    SLOT(nm);
    HANDLE h = U(CreateMailslotA)(nm, 0, 0, NULL);
    HANDLE c = slot_client(nm);
    DWORD n, next = 0, cnt = 0; char b[64];
    CHECK(WriteFile(c, "12345", 5, &n, NULL)); CHECK(WriteFile(c, "1234567", 7, &n, NULL));
    CHECK(U(GetMailslotInfo)(h, NULL, &next, &cnt, NULL)); EXPECT_EQ(next, 5); EXPECT_EQ(cnt, 2);
    CHECK(ReadFile(h, b, 64, &n, NULL));
    CHECK(U(GetMailslotInfo)(h, NULL, &next, &cnt, NULL)); EXPECT_EQ(next, 7); EXPECT_EQ(cnt, 1);
    CHECK(ReadFile(h, b, 64, &n, NULL));
    CHECK(U(GetMailslotInfo)(h, NULL, &next, &cnt, NULL)); EXPECT_EQ(next, MAILSLOT_NO_MESSAGE); EXPECT_EQ(cnt, 0);
    CloseHandle(c); CloseHandle(h);
}
TEST(pmc_ms_read_buffer_too_small) {
    REQUIRE(U(CreateMailslotA)); REQUIRE(U(GetMailslotInfo));
    SLOT(nm);
    HANDLE h = U(CreateMailslotA)(nm, 0, 0, NULL);
    HANDLE c = slot_client(nm);
    DWORD n = 55, next = 0, cnt = 0; char b[64];
    CHECK(WriteFile(c, "0123456789", 10, &n, NULL));
    SET_ERR(0); CHECK(!ReadFile(h, b, 4, &n, NULL)); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(U(GetMailslotInfo)(h, NULL, &next, &cnt, NULL)); EXPECT_EQ(next, 10); EXPECT_EQ(cnt, 1);   /* message not consumed */
    SET_ERR(0); CHECK(!ReadFile(h, b, 9, &n, NULL)); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(ReadFile(h, b, 10, &n, NULL)); EXPECT_EQ(n, 10);   /* exact size works */
    CloseHandle(c); CloseHandle(h);
}
TEST(pmc_ms_read_zero_buffer) {
    REQUIRE(U(CreateMailslotA));
    SLOT(nm);
    HANDLE h = U(CreateMailslotA)(nm, 0, 0, NULL);
    HANDLE c = slot_client(nm);
    DWORD n = 55; char b[8];
    CHECK(WriteFile(c, "x", 1, &n, NULL));
    SET_ERR(0);
    BOOL ok = ReadFile(h, b, 0, &n, NULL);
    t_note("ReadFile(0) on mailslot with message: ok=%d err=%lu", ok, GetLastError());
    CHECK(ReadFile(h, b, 8, &n, NULL)); EXPECT_EQ(n, 1);
    CloseHandle(c); CloseHandle(h);
}
TEST(pmc_ms_read_timeout_zero_empty) {
    REQUIRE(U(CreateMailslotA));
    SLOT(nm);
    HANDLE h = U(CreateMailslotA)(nm, 0, 0, NULL);
    DWORD n = 55; char b[8], t0 = 0; DWORD s = GetTickCount();
    SET_ERR(0); CHECK(!ReadFile(h, b, 8, &n, NULL)); EXPECT_ERR(ERROR_SEM_TIMEOUT);
    CHECK(GetTickCount() - s < 1000);
    CloseHandle(h);
}
TEST(pmc_ms_read_timeout_ms) {
    REQUIRE(U(CreateMailslotA));
    SLOT(nm);
    HANDLE h = U(CreateMailslotA)(nm, 0, 300, NULL);
    DWORD n = 55; char b[8]; DWORD s = GetTickCount();
    SET_ERR(0); CHECK(!ReadFile(h, b, 8, &n, NULL)); EXPECT_ERR(ERROR_SEM_TIMEOUT);
    DWORD dt = GetTickCount() - s;
    CHECKF(dt >= 250 && dt < 3000, "elapsed %lu", dt);
    CloseHandle(h);
}
static DWORD WINAPI slot_late_writer(void *p) {
    Sleep(150);
    HANDLE c = slot_client((const char *)p); DWORD n;
    if (c != INVALID_HANDLE_VALUE) { WriteFile(c, "late", 4, &n, NULL); CloseHandle(c); }
    return 0;
}
TEST(pmc_ms_read_blocks_until_message) {
    REQUIRE(U(CreateMailslotA));
    SLOT(nm);
    HANDLE h = U(CreateMailslotA)(nm, 0, 5000, NULL);
    HANDLE th = pmc_thread(slot_late_writer, nm);
    DWORD n = 0; char b[8]; DWORD s = GetTickCount();
    CHECK(ReadFile(h, b, 8, &n, NULL)); EXPECT_EQ(n, 4);
    DWORD dt = GetTickCount() - s;
    CHECKF(dt >= 100 && dt < 4000, "elapsed %lu", dt);
    pmc_join(th); CloseHandle(h);
}
TEST(pmc_ms_set_timeout_affects_read) {
    REQUIRE(U(CreateMailslotA)); REQUIRE(U(SetMailslotInfo));
    SLOT(nm);
    HANDLE h = U(CreateMailslotA)(nm, 0, MAILSLOT_WAIT_FOREVER, NULL);
    CHECK(U(SetMailslotInfo)(h, 100));
    DWORD n = 0; char b[8]; DWORD s = GetTickCount();
    SET_ERR(0); CHECK(!ReadFile(h, b, 8, &n, NULL)); EXPECT_ERR(ERROR_SEM_TIMEOUT);
    CHECK(GetTickCount() - s < 3000);
    CloseHandle(h);
}
TEST(pmc_ms_max_message_size) {
    REQUIRE(U(CreateMailslotA)); REQUIRE(U(GetMailslotInfo));
    SLOT(nm);
    HANDLE h = U(CreateMailslotA)(nm, 16, 0, NULL);
    HANDLE c = slot_client(nm);
    DWORD n = 0, next = 0, cnt = 0; char big[64]; memset(big, 'z', sizeof big);
    CHECK(WriteFile(c, big, 16, &n, NULL)); EXPECT_EQ(n, 16);
    SET_ERR(0);
    BOOL ok = WriteFile(c, big, 17, &n, NULL);
    t_note("WriteFile 17 bytes to mailslot with max 16: ok=%d err=%lu", ok, GetLastError());
    CHECK(U(GetMailslotInfo)(h, NULL, &next, &cnt, NULL)); EXPECT_EQ(cnt, ok ? 2 : 1);
    CloseHandle(c); CloseHandle(h);
}
TEST(pmc_ms_large_message) {
    REQUIRE(U(CreateMailslotA));
    SLOT(nm);
    HANDLE h = U(CreateMailslotA)(nm, 0, 0, NULL);
    HANDLE c = slot_client(nm);
    DWORD sizes[] = { 1000, 4000, 20000 };
    for (int i = 0; i < 3; i++) {
        unsigned char *w = malloc(sizes[i]), *r = malloc(sizes[i]); DWORD n = 0;
        pmc_fill(w, sizes[i], i);
        SET_ERR(0);
        BOOL ok = WriteFile(c, w, sizes[i], &n, NULL);
        if (ok) { CHECK(ReadFile(h, r, sizes[i], &n, NULL)); EXPECT_EQ(n, sizes[i]); CHECK(!memcmp(w, r, sizes[i])); }
        else t_note("mailslot write of %lu bytes failed err=%lu", sizes[i], GetLastError());
        free(w); free(r);
    }
    CloseHandle(c); CloseHandle(h);
}
TEST(pmc_ms_zero_length_message) {
    REQUIRE(U(CreateMailslotA)); REQUIRE(U(GetMailslotInfo));
    SLOT(nm);
    HANDLE h = U(CreateMailslotA)(nm, 0, 0, NULL);
    HANDLE c = slot_client(nm);
    DWORD n = 55, next = 0, cnt = 0; char b[4];
    SET_ERR(0);
    BOOL ok = WriteFile(c, "", 0, &n, NULL);
    t_note("zero-length mailslot write: ok=%d err=%lu n=%lu", ok, GetLastError(), n);
    CHECK(U(GetMailslotInfo)(h, NULL, &next, &cnt, NULL));
    t_note("after zero-length write: next=%lu count=%lu", next, cnt);
    if (cnt) { SET_ERR(0); ok = ReadFile(h, b, 4, &n, NULL); t_note("read zero-length: ok=%d n=%lu err=%lu", ok, n, GetLastError()); }
    CloseHandle(c); CloseHandle(h);
}
TEST(pmc_ms_client_nonexistent) {
    SLOT(nm);
    SET_ERR(0);
    HANDLE c = slot_client(nm);
    CHECK(c == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(pmc_ms_client_requires_share_read) {
    REQUIRE(U(CreateMailslotA));
    SLOT(nm);
    HANDLE h = U(CreateMailslotA)(nm, 0, 0, NULL);
    SET_ERR(0);
    HANDLE c = CreateFileA(nm, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(c == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_SHARING_VIOLATION);
    c = CreateFileA(nm, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(c != INVALID_HANDLE_VALUE);
    if (c != INVALID_HANDLE_VALUE) CloseHandle(c);
    CloseHandle(h);
}
TEST(pmc_ms_client_read_access_denied) {
    REQUIRE(U(CreateMailslotA));
    SLOT(nm);
    HANDLE h = U(CreateMailslotA)(nm, 0, 0, NULL);
    SET_ERR(0);
    HANDLE c = CreateFileA(nm, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    t_note("mailslot client with GENERIC_READ: err=%lu", GetLastError());
    if (c != INVALID_HANDLE_VALUE) CloseHandle(c);
    CloseHandle(h);
}
TEST(pmc_ms_client_create_new_fails) {
    REQUIRE(U(CreateMailslotA));
    SLOT(nm);
    HANDLE h = U(CreateMailslotA)(nm, 0, 0, NULL);
    SET_ERR(0);
    HANDLE c = CreateFileA(nm, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, 0, NULL);
    t_note("CREATE_NEW on mailslot: %s err=%lu", c != INVALID_HANDLE_VALUE ? "ok" : "fail", GetLastError());
    if (c != INVALID_HANDLE_VALUE) CloseHandle(c);
    CloseHandle(h);
}
TEST(pmc_ms_client_read_on_write_handle) {
    REQUIRE(U(CreateMailslotA));
    SLOT(nm);
    HANDLE h = U(CreateMailslotA)(nm, 0, 0, NULL);
    HANDLE c = slot_client(nm);
    DWORD n; char b[4];
    SET_ERR(0); CHECK(!ReadFile(c, b, 4, &n, NULL)); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(c); CloseHandle(h);
}
TEST(pmc_ms_multiple_clients) {
    REQUIRE(U(CreateMailslotA)); REQUIRE(U(GetMailslotInfo));
    SLOT(nm);
    HANDLE h = U(CreateMailslotA)(nm, 0, 0, NULL);
    HANDLE c[4]; DWORD n, cnt = 0, total = 0;
    for (int i = 0; i < 4; i++) { c[i] = slot_client(nm); CHECK(c[i] != INVALID_HANDLE_VALUE); }
    for (int i = 0; i < 4; i++) { char m = (char)('0' + i); CHECK(WriteFile(c[i], &m, 1, &n, NULL)); }
    CHECK(U(GetMailslotInfo)(h, NULL, NULL, &cnt, NULL)); EXPECT_EQ(cnt, 4);
    for (int i = 0; i < 4; i++) { char b[4]; CHECK(ReadFile(h, b, 4, &n, NULL)); EXPECT_EQ(n, 1); total += b[0] - '0'; }
    EXPECT_EQ(total, 0 + 1 + 2 + 3);
    for (int i = 0; i < 4; i++) CloseHandle(c[i]);
    CloseHandle(h);
}
TEST(pmc_ms_client_outlives_server) {
    REQUIRE(U(CreateMailslotA));
    SLOT(nm);
    HANDLE h = U(CreateMailslotA)(nm, 0, 0, NULL);
    HANDLE c = slot_client(nm);
    CloseHandle(h);
    DWORD n = 55;
    SET_ERR(0);
    BOOL ok = WriteFile(c, "x", 1, &n, NULL);
    t_note("write to mailslot after server closed: ok=%d err=%lu", ok, GetLastError());
    CloseHandle(c);
}
TEST(pmc_ms_message_dropped_on_server_close) {
    REQUIRE(U(CreateMailslotA));
    SLOT(nm);
    HANDLE h = U(CreateMailslotA)(nm, 0, 0, NULL);
    HANDLE c = slot_client(nm); DWORD n;
    CHECK(WriteFile(c, "pending", 7, &n, NULL));
    CloseHandle(c); CloseHandle(h);
    HANDLE h2 = U(CreateMailslotA)(nm, 0, 0, NULL);
    DWORD cnt = 9;
    CHECK(h2 != INVALID_HANDLE_VALUE);
    CHECK(U(GetMailslotInfo)(h2, NULL, NULL, &cnt, NULL)); EXPECT_EQ(cnt, 0);
    CloseHandle(h2);
}
TEST(pmc_ms_overlapped_read) {
    REQUIRE(U(CreateMailslotA));
    SLOT(nm);
    HANDLE h = U(CreateMailslotA)(nm, 0, MAILSLOT_WAIT_FOREVER, NULL);
    HANDLE c = slot_client(nm);
    HANDLE ho = CreateFileA(nm, GENERIC_READ, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    t_note("reopen mailslot server for read: %s err=%lu", ho != INVALID_HANDLE_VALUE ? "ok" : "fail", GetLastError());
    if (ho != INVALID_HANDLE_VALUE) CloseHandle(ho);
    CloseHandle(c); CloseHandle(h);
}
TEST(pmc_ms_dup_server_handle) {
    REQUIRE(U(CreateMailslotA));
    SLOT(nm);
    HANDLE h = U(CreateMailslotA)(nm, 0, 0, NULL), h2;
    HANDLE c = slot_client(nm); DWORD n; char b[8];
    CHECK(DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(), &h2, 0, FALSE, DUPLICATE_SAME_ACCESS));
    CloseHandle(h);
    CHECK(WriteFile(c, "dup", 3, &n, NULL));
    CHECK(ReadFile(h2, b, 8, &n, NULL)); EXPECT_EQ(n, 3);
    SET_ERR(0);
    HANDLE dup = U(CreateMailslotA)(nm, 0, 0, NULL);
    CHECK(dup == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_ALREADY_EXISTS);
    CloseHandle(c); CloseHandle(h2);
}
TEST(pmc_ms_security_inherit) {
    REQUIRE(U(CreateMailslotA));
    SLOT(nm);
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE h = U(CreateMailslotA)(nm, 0, 0, &sa);
    DWORD f = 0; CHECK(GetHandleInformation(h, &f)); CHECK(f & HANDLE_FLAG_INHERIT);
    CloseHandle(h);
    h = U(CreateMailslotA)(nm, 0, 0, NULL);
    f = 0xff; CHECK(GetHandleInformation(h, &f)); CHECK(!(f & HANDLE_FLAG_INHERIT));
    CloseHandle(h);
}
TEST(pmc_ms_peek_not_supported) {
    REQUIRE(U(CreateMailslotA)); REQUIRE(U(PeekNamedPipe));
    SLOT(nm);
    HANDLE h = U(CreateMailslotA)(nm, 0, 0, NULL);
    DWORD av;
    SET_ERR(0); CHECK(!U(PeekNamedPipe)(h, NULL, 0, NULL, &av, NULL));
    t_note("PeekNamedPipe on mailslot err=%lu", GetLastError());
    CloseHandle(h);
}
TEST(pmc_ms_many_messages) {
    REQUIRE(U(CreateMailslotA)); REQUIRE(U(GetMailslotInfo));
    SLOT(nm);
    HANDLE h = U(CreateMailslotA)(nm, 0, 0, NULL);
    HANDLE c = slot_client(nm);
    DWORD n, cnt = 0;
    for (int i = 0; i < 500; i++) { char m[8]; int l = snprintf(m, sizeof m, "%d", i); CHECK(WriteFile(c, m, l, &n, NULL)); }
    CHECK(U(GetMailslotInfo)(h, NULL, NULL, &cnt, NULL)); EXPECT_EQ(cnt, 500);
    for (int i = 0; i < 500; i++) { char b[16] = {0}, m[8]; int l = snprintf(m, sizeof m, "%d", i); CHECK(ReadFile(h, b, 16, &n, NULL)); if (n != (DWORD)l || memcmp(b, m, l)) { CHECK(0); break; } }
    CloseHandle(c); CloseHandle(h);
}
