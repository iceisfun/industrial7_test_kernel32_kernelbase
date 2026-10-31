/* Named pipe data transfer: message mode, SetNamedPipeHandleState, Transact/Call, overlapped, IOCP, threads. */
#include "pmc.h"

#define PIPE_NAME(n) char n[128]; pmc_pipename(n, sizeof n)
#define MSGMODE (PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT)

static HANDLE mk_msg_pair(char *nm, HANDLE *cli, DWORD srv_access) {
    pmc_pipename(nm, 128);
    HANDLE s = pmc_server(nm, srv_access, MSGMODE, 4, 4096, 4096);
    *cli = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    return s;
}

TEST(pmc_msg_boundaries) {
    REQUIRE(U(CreateNamedPipeA));
    char nm[128]; HANDLE c, s = mk_msg_pair(nm, &c, PIPE_ACCESS_DUPLEX);
    DWORD n; char b[64];
    CHECK(WriteFile(c, "hello", 5, &n, NULL)); CHECK(WriteFile(c, "ab", 2, &n, NULL)); CHECK(WriteFile(c, "wxyz!", 5, &n, NULL));
    CHECK(ReadFile(s, b, 64, &n, NULL)); EXPECT_EQ(n, 5); CHECK(!memcmp(b, "hello", 5));
    CHECK(ReadFile(s, b, 64, &n, NULL)); EXPECT_EQ(n, 2); CHECK(!memcmp(b, "ab", 2));
    CHECK(ReadFile(s, b, 64, &n, NULL)); EXPECT_EQ(n, 5); CHECK(!memcmp(b, "wxyz!", 5));
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_msg_zero_length_message) {
    REQUIRE(U(CreateNamedPipeA));
    char nm[128]; HANDLE c, s = mk_msg_pair(nm, &c, PIPE_ACCESS_DUPLEX);
    DWORD n = 77, av = 0, left = 0; char b[8];
    CHECK(WriteFile(c, "", 0, &n, NULL)); EXPECT_EQ(n, 0);
    CHECK(WriteFile(c, "A", 1, &n, NULL));
    CHECK(ReadFile(s, b, 8, &n, NULL)); EXPECT_EQ(n, 0);
    CHECK(ReadFile(s, b, 8, &n, NULL)); EXPECT_EQ(n, 1); CHECK(b[0] == 'A');
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_msg_partial_read_more_data) {
    REQUIRE(U(CreateNamedPipeA));
    char nm[128]; HANDLE c, s = mk_msg_pair(nm, &c, PIPE_ACCESS_DUPLEX);
    DWORD n = 0; char b[16];
    CHECK(WriteFile(c, "0123456789", 10, &n, NULL));
    SET_ERR(0); CHECK(!ReadFile(s, b, 4, &n, NULL)); EXPECT_ERR(ERROR_MORE_DATA); EXPECT_EQ(n, 4); CHECK(!memcmp(b, "0123", 4));
    SET_ERR(0); CHECK(!ReadFile(s, b, 4, &n, NULL)); EXPECT_ERR(ERROR_MORE_DATA); EXPECT_EQ(n, 4); CHECK(!memcmp(b, "4567", 4));
    CHECK(ReadFile(s, b, 4, &n, NULL)); EXPECT_EQ(n, 2); CHECK(!memcmp(b, "89", 2));
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_msg_read_one_byte_buffer) {
    REQUIRE(U(CreateNamedPipeA));
    char nm[128]; HANDLE c, s = mk_msg_pair(nm, &c, PIPE_ACCESS_DUPLEX);
    DWORD n = 0; char b[4]; int more = 0;
    CHECK(WriteFile(c, "abc", 3, &n, NULL));
    for (int i = 0; i < 3; i++) {
        BOOL ok = ReadFile(s, b, 1, &n, NULL);
        if (!ok) { EXPECT_ERR(ERROR_MORE_DATA); more++; }
        EXPECT_EQ(n, 1); CHECK(b[0] == 'a' + i);
    }
    EXPECT_EQ(more, 2);
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_msg_read_zero_size_buffer) {
    REQUIRE(U(CreateNamedPipeA));
    char nm[128]; HANDLE c, s = mk_msg_pair(nm, &c, PIPE_ACCESS_DUPLEX);
    DWORD n = 55; char b[4];
    CHECK(WriteFile(c, "xyz", 3, &n, NULL));
    SET_ERR(0);
    BOOL ok = ReadFile(s, b, 0, &n, NULL);
    t_note("ReadFile(0 bytes) on message pipe with data: ok=%d err=%lu n=%lu", ok, GetLastError(), n);
    CHECK(ReadFile(s, b, 4, &n, NULL)); EXPECT_EQ(n, 3);
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_msg_peek_sizes) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(PeekNamedPipe));
    char nm[128]; HANDLE c, s = mk_msg_pair(nm, &c, PIPE_ACCESS_DUPLEX);
    DWORD n, rd = 0, av = 0, left = 0; char b[16];
    CHECK(WriteFile(c, "0123456789", 10, &n, NULL));
    CHECK(U(PeekNamedPipe)(s, b, 3, &rd, &av, &left));
    EXPECT_EQ(rd, 3); EXPECT_EQ(av, 10); EXPECT_EQ(left, 7); CHECK(!memcmp(b, "012", 3));   /* kernelbase!PeekNamedPipe: left = MessageLength - bytesRead */
    /* still unread */
    CHECK(U(PeekNamedPipe)(s, NULL, 0, NULL, &av, &left)); EXPECT_EQ(av, 10); EXPECT_EQ(left, 10);
    CHECK(!ReadFile(s, b, 4, &n, NULL)); EXPECT_ERR(ERROR_MORE_DATA);
    CHECK(U(PeekNamedPipe)(s, b, 16, &rd, &av, &left));
    EXPECT_EQ(rd, 6); EXPECT_EQ(av, 6); EXPECT_EQ(left, 0); CHECK(!memcmp(b, "456789", 6));
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_msg_peek_two_messages) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(PeekNamedPipe));
    char nm[128]; HANDLE c, s = mk_msg_pair(nm, &c, PIPE_ACCESS_DUPLEX);
    DWORD n, rd = 0, av = 0, left = 0; char b[16];
    CHECK(WriteFile(c, "aaaa", 4, &n, NULL)); CHECK(WriteFile(c, "bb", 2, &n, NULL));
    CHECK(U(PeekNamedPipe)(s, b, 16, &rd, &av, &left));
    t_note("two messages 4+2: read=%lu avail=%lu left=%lu", rd, av, left);
    CHECK(left == 4u - rd);   /* kernelbase!PeekNamedPipe: left = first MessageLength - bytesRead */
    CHECK(rd >= 4);
    CHECK(av >= 4);
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_msg_peek_empty_message_pipe) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(PeekNamedPipe));
    char nm[128]; HANDLE c, s = mk_msg_pair(nm, &c, PIPE_ACCESS_DUPLEX);
    DWORD rd = 9, av = 9, left = 9; char b[4];
    CHECK(U(PeekNamedPipe)(s, b, 4, &rd, &av, &left));
    EXPECT_EQ(rd, 0); EXPECT_EQ(av, 0); EXPECT_EQ(left, 0);
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_msg_client_default_read_mode_byte) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(GetNamedPipeHandleStateA));
    char nm[128]; HANDLE c, s = mk_msg_pair(nm, &c, PIPE_ACCESS_DUPLEX);
    DWORD st = 0xff, sst = 0xff;
    CHECK(U(GetNamedPipeHandleStateA)(c, &st, NULL, NULL, NULL, NULL, 0));
    CHECK(U(GetNamedPipeHandleStateA)(s, &sst, NULL, NULL, NULL, NULL, 0));
    EXPECT_EQ(sst, PIPE_READMODE_MESSAGE);
    t_note("client initial state 0x%lx (server 0x%lx)", st, sst);
    EXPECT_EQ(st, PIPE_READMODE_BYTE);
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_msg_client_set_readmode_message) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(SetNamedPipeHandleState)); REQUIRE(U(GetNamedPipeHandleStateA));
    char nm[128]; HANDLE c, s = mk_msg_pair(nm, &c, PIPE_ACCESS_DUPLEX);
    DWORD mode = PIPE_READMODE_MESSAGE, st = 0, n; char b[16];
    CHECK(U(SetNamedPipeHandleState)(c, &mode, NULL, NULL));
    CHECK(U(GetNamedPipeHandleStateA)(c, &st, NULL, NULL, NULL, NULL, 0)); EXPECT_EQ(st, PIPE_READMODE_MESSAGE);
    CHECK(WriteFile(s, "one", 3, &n, NULL)); CHECK(WriteFile(s, "two2", 4, &n, NULL));
    CHECK(ReadFile(c, b, 16, &n, NULL)); EXPECT_EQ(n, 3);
    CHECK(ReadFile(c, b, 16, &n, NULL)); EXPECT_EQ(n, 4);
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_msg_client_byte_mode_reads_stream) {
    REQUIRE(U(CreateNamedPipeA));
    char nm[128]; HANDLE c, s = mk_msg_pair(nm, &c, PIPE_ACCESS_DUPLEX);
    DWORD n; char b[16];
    CHECK(WriteFile(s, "one", 3, &n, NULL)); CHECK(WriteFile(s, "two", 3, &n, NULL));
    CHECK(ReadFile(c, b, 2, &n, NULL)); EXPECT_EQ(n, 2); CHECK(!memcmp(b, "on", 2));   /* byte mode: no MORE_DATA */
    CHECK(ReadFile(c, b, 16, &n, NULL));
    t_note("byte-mode client read after partial: %lu bytes", n);
    CHECK(n >= 1);
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_msg_server_switch_to_byte_mode) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(SetNamedPipeHandleState)); REQUIRE(U(GetNamedPipeHandleStateA));
    char nm[128]; HANDLE c, s = mk_msg_pair(nm, &c, PIPE_ACCESS_DUPLEX);
    DWORD mode = PIPE_READMODE_BYTE, st = 9, n; char b[16];
    CHECK(U(SetNamedPipeHandleState)(s, &mode, NULL, NULL));
    CHECK(U(GetNamedPipeHandleStateA)(s, &st, NULL, NULL, NULL, NULL, 0)); EXPECT_EQ(st, 0);
    CHECK(WriteFile(c, "abc", 3, &n, NULL));
    CHECK(ReadFile(s, b, 2, &n, NULL)); EXPECT_EQ(n, 2);
    CHECK(ReadFile(s, b, 2, &n, NULL)); EXPECT_EQ(n, 1);
    mode = PIPE_READMODE_MESSAGE;
    CHECK(U(SetNamedPipeHandleState)(s, &mode, NULL, NULL));
    CHECK(U(GetNamedPipeHandleStateA)(s, &st, NULL, NULL, NULL, NULL, 0)); EXPECT_EQ(st, PIPE_READMODE_MESSAGE);
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_np_setstate_message_on_byte_pipe) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(SetNamedPipeHandleState));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    DWORD mode = PIPE_READMODE_MESSAGE;
    SET_ERR(0);
    CHECK(!U(SetNamedPipeHandleState)(s, &mode, NULL, NULL));
    t_note("message read mode on byte pipe: err=%lu", GetLastError());
    SET_ERR(0);
    CHECK(!U(SetNamedPipeHandleState)(c, &mode, NULL, NULL));
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_np_setstate_all_null) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(SetNamedPipeHandleState));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    CHECK(U(SetNamedPipeHandleState)(s, NULL, NULL, NULL));
    CloseHandle(s);
}
TEST(pmc_np_setstate_nowait_toggle) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(SetNamedPipeHandleState)); REQUIRE(U(GetNamedPipeHandleStateA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    DWORD mode = PIPE_NOWAIT, st = 0, n; char b[4];
    CHECK(U(SetNamedPipeHandleState)(c, &mode, NULL, NULL));
    CHECK(U(GetNamedPipeHandleStateA)(c, &st, NULL, NULL, NULL, NULL, 0)); EXPECT_EQ(st, PIPE_NOWAIT);
    SET_ERR(0); CHECK(!ReadFile(c, b, 4, &n, NULL)); EXPECT_ERR(ERROR_NO_DATA);
    mode = PIPE_WAIT;
    CHECK(U(SetNamedPipeHandleState)(c, &mode, NULL, NULL));
    CHECK(U(GetNamedPipeHandleStateA)(c, &st, NULL, NULL, NULL, NULL, 0)); EXPECT_EQ(st, 0);
    CHECK(WriteFile(s, "k", 1, &n, NULL)); CHECK(ReadFile(c, b, 4, &n, NULL)); EXPECT_EQ(n, 1);
    /* server's own state is independent */
    CHECK(U(GetNamedPipeHandleStateA)(s, &st, NULL, NULL, NULL, NULL, 0)); EXPECT_EQ(st, 0);
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_np_setstate_read_only_access_denied) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(SetNamedPipeHandleState));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, MSGMODE, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ, 0);
    CHECK(c != INVALID_HANDLE_VALUE);
    DWORD mode = PIPE_READMODE_BYTE;
    SET_ERR(0); CHECK(!U(SetNamedPipeHandleState)(c, &mode, NULL, NULL)); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_np_setstate_invalid_handle) {
    REQUIRE(U(SetNamedPipeHandleState));
    DWORD mode = PIPE_NOWAIT;
    SET_ERR(0); CHECK(!U(SetNamedPipeHandleState)(NULL, &mode, NULL, NULL)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(SetNamedPipeHandleState)(INVALID_HANDLE_VALUE, &mode, NULL, NULL)); CHECK(GetLastError() != 0);
}
TEST(pmc_np_setstate_on_file_handle) {
    REQUIRE(U(SetNamedPipeHandleState));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "pmc_snphs.bin");
    HANDLE h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD mode = PIPE_NOWAIT;
    SET_ERR(0); CHECK(!U(SetNamedPipeHandleState)(h, &mode, NULL, NULL)); CHECK(GetLastError() != 0);
    CloseHandle(h); DeleteFileA(p);
}
TEST(pmc_np_setstate_bad_mode_bits) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(SetNamedPipeHandleState));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, MSGMODE, 1, 512, 512);
    DWORD bad[] = { 0x10, 0x100, 0x80000000, PIPE_TYPE_MESSAGE };
    for (int i = 0; i < 4; i++) {
        DWORD m = bad[i]; SET_ERR(0);
        BOOL ok = U(SetNamedPipeHandleState)(s, &m, NULL, NULL);
        t_note("SetNamedPipeHandleState mode 0x%lx -> %d err=%lu", m, ok, ok ? 0 : GetLastError());
    }
    CloseHandle(s);
}
TEST(pmc_np_setstate_collect_params_local) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(SetNamedPipeHandleState)); REQUIRE(U(GetNamedPipeHandleStateA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    DWORD cnt = 100, tmo = 250, gc = 0, gt = 0;
    SET_ERR(0);
    BOOL ok = U(SetNamedPipeHandleState)(c, NULL, &cnt, &tmo);
    t_note("set collect params on local client: ok=%d err=%lu", ok, ok ? 0 : GetLastError());
    if (ok) {
        CHECK(U(GetNamedPipeHandleStateA)(c, NULL, NULL, &gc, &gt, NULL, 0));
        t_note("collect count=%lu timeout=%lu", gc, gt);
    }
    ok = U(SetNamedPipeHandleState)(s, NULL, &cnt, &tmo);
    t_note("set collect params on server end: ok=%d err=%lu", ok, ok ? 0 : GetLastError());
    CloseHandle(c); CloseHandle(s);
}

/* ---- Transact / Call ---- */
typedef struct { HANDLE s; const char *reply; DWORD replylen; char got[256]; DWORD gotlen; BOOL connected; } srv_t;
static DWORD WINAPI echo_srv(void *p) {
    srv_t *x = p; DWORD n;
    BOOL ok = ConnectNamedPipe(x->s, NULL) || GetLastError() == ERROR_PIPE_CONNECTED;
    if (!ok) return 1;
    x->connected = TRUE;
    if (!ReadFile(x->s, x->got, sizeof x->got, &n, NULL)) return 2;
    x->gotlen = n;
    if (!WriteFile(x->s, x->reply, x->replylen, &n, NULL)) return 3;
    FlushFileBuffers(x->s);
    return 0;
}
TEST(pmc_transact_roundtrip) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(TransactNamedPipe)); REQUIRE(U(SetNamedPipeHandleState));
    char nm[128]; HANDLE c, s = mk_msg_pair(nm, &c, PIPE_ACCESS_DUPLEX);
    srv_t x = { s, "REPLY-DATA", 10 };
    HANDLE th = pmc_thread(echo_srv, &x);
    DWORD mode = PIPE_READMODE_MESSAGE, n = 0; char out[64] = {0};
    CHECK(U(SetNamedPipeHandleState)(c, &mode, NULL, NULL));
    CHECK(U(TransactNamedPipe)(c, "REQUEST", 7, out, sizeof out, &n, NULL));
    EXPECT_EQ(n, 10); CHECK(!memcmp(out, "REPLY-DATA", 10));
    pmc_join(th);
    EXPECT_EQ(x.gotlen, 7); CHECK(!memcmp(x.got, "REQUEST", 7));
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_transact_reply_larger_more_data) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(TransactNamedPipe)); REQUIRE(U(SetNamedPipeHandleState));
    char nm[128]; HANDLE c, s = mk_msg_pair(nm, &c, PIPE_ACCESS_DUPLEX);
    srv_t x = { s, "0123456789ABCDEF", 16 };
    HANDLE th = pmc_thread(echo_srv, &x);
    DWORD mode = PIPE_READMODE_MESSAGE, n = 0; char out[64] = {0};
    CHECK(U(SetNamedPipeHandleState)(c, &mode, NULL, NULL));
    SET_ERR(0);
    CHECK(!U(TransactNamedPipe)(c, "Q", 1, out, 8, &n, NULL)); EXPECT_ERR(ERROR_MORE_DATA);
    EXPECT_EQ(n, 8); CHECK(!memcmp(out, "01234567", 8));
    CHECK(ReadFile(c, out, 64, &n, NULL)); EXPECT_EQ(n, 8); CHECK(!memcmp(out, "89ABCDEF", 8));
    pmc_join(th);
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_transact_empty_reply_buffer_zero) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(TransactNamedPipe)); REQUIRE(U(SetNamedPipeHandleState));
    char nm[128]; HANDLE c, s = mk_msg_pair(nm, &c, PIPE_ACCESS_DUPLEX);
    srv_t x = { s, "R", 1 };
    HANDLE th = pmc_thread(echo_srv, &x);
    DWORD mode = PIPE_READMODE_MESSAGE, n = 55; char out[4];
    CHECK(U(SetNamedPipeHandleState)(c, &mode, NULL, NULL));
    SET_ERR(0);
    BOOL ok = U(TransactNamedPipe)(c, "Q", 1, out, 0, &n, NULL);
    t_note("Transact with 0-size out buffer: ok=%d err=%lu n=%lu", ok, ok ? 0 : GetLastError(), n);
    CloseHandle(c);   /* unblock the server thread whatever happened */
    pmc_join(th); CloseHandle(s);
}
TEST(pmc_transact_byte_pipe_bad_pipe) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(TransactNamedPipe));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    DWORD n = 0; char out[8];
    SET_ERR(0);
    CHECK(!U(TransactNamedPipe)(c, "Q", 1, out, 8, &n, NULL));
    t_note("Transact on byte pipe err=%lu", GetLastError());
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_transact_client_not_message_readmode) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(TransactNamedPipe));
    char nm[128]; HANDLE c, s = mk_msg_pair(nm, &c, PIPE_ACCESS_DUPLEX);
    DWORD n = 0; char out[8];
    SET_ERR(0);
    BOOL ok = U(TransactNamedPipe)(c, "Q", 1, out, 8, &n, NULL);
    CHECK(!ok);   /* client still in byte read mode: transceive requires message read mode */
    t_note("Transact on message pipe with byte read mode: err=%lu", GetLastError());
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_transact_invalid_handle) {
    REQUIRE(U(TransactNamedPipe));
    DWORD n; char out[4];
    SET_ERR(0); CHECK(!U(TransactNamedPipe)(NULL, "x", 1, out, 4, &n, NULL)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(TransactNamedPipe)(INVALID_HANDLE_VALUE, "x", 1, out, 4, &n, NULL)); CHECK(GetLastError() != 0);
}
TEST(pmc_transact_on_file_handle) {
    REQUIRE(U(TransactNamedPipe));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "pmc_trans.bin");
    HANDLE h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD n; char out[4];
    SET_ERR(0); CHECK(!U(TransactNamedPipe)(h, "x", 1, out, 4, &n, NULL)); CHECK(GetLastError() != 0);
    CloseHandle(h); DeleteFileA(p);
}
TEST(pmc_transact_overlapped) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(TransactNamedPipe)); REQUIRE(U(SetNamedPipeHandleState));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, MSGMODE, 1, 4096, 4096);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, FILE_FLAG_OVERLAPPED);
    srv_t x = { s, "OVL-REPLY", 9 };
    HANDLE th = pmc_thread(echo_srv, &x);
    DWORD mode = PIPE_READMODE_MESSAGE, n = 0; char out[32] = {0};
    CHECK(U(SetNamedPipeHandleState)(c, &mode, NULL, NULL));
    OVERLAPPED ov = {0}; ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    BOOL ok = U(TransactNamedPipe)(c, "OVL", 3, out, sizeof out, &n, &ov);
    if (!ok) { EXPECT_ERR(ERROR_IO_PENDING); EXPECT_EQ(WaitForSingleObject(ov.hEvent, 5000), WAIT_OBJECT_0); }
    CHECK(GetOverlappedResult(c, &ov, &n, FALSE)); EXPECT_EQ(n, 9); CHECK(!memcmp(out, "OVL-REPLY", 9));
    pmc_join(th); CloseHandle(ov.hEvent); CloseHandle(c); CloseHandle(s);
}
TEST(pmc_call_roundtrip_a) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(CallNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, MSGMODE, 1, 4096, 4096);
    srv_t x = { s, "CALL-REPLY", 10 };
    HANDLE th = pmc_thread(echo_srv, &x);
    char out[64] = {0}; DWORD n = 0;
    CHECK(U(CallNamedPipeA)(nm, "CALL-REQ", 8, out, sizeof out, &n, 5000));
    EXPECT_EQ(n, 10); CHECK(!memcmp(out, "CALL-REPLY", 10));
    pmc_join(th);
    EXPECT_EQ(x.gotlen, 8); CHECK(!memcmp(x.got, "CALL-REQ", 8));
    CloseHandle(s);
}
TEST(pmc_call_roundtrip_w) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(CallNamedPipeW));
    PIPE_NAME(nm);
    WCHAR w[128]; MultiByteToWideChar(CP_ACP, 0, nm, -1, w, 128);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, MSGMODE, 1, 4096, 4096);
    srv_t x = { s, "WIDE", 4 };
    HANDLE th = pmc_thread(echo_srv, &x);
    char out[64] = {0}; DWORD n = 0;
    CHECK(U(CallNamedPipeW)(w, "w-req", 5, out, sizeof out, &n, 5000));
    EXPECT_EQ(n, 4); CHECK(!memcmp(out, "WIDE", 4));
    pmc_join(th); CloseHandle(s);
}
TEST(pmc_call_reply_too_large) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(CallNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, MSGMODE, 1, 4096, 4096);
    srv_t x = { s, "0123456789ABCDEF", 16 };
    HANDLE th = pmc_thread(echo_srv, &x);
    char out[64] = {0}; DWORD n = 0;
    SET_ERR(0);
    CHECK(!U(CallNamedPipeA)(nm, "Q", 1, out, 8, &n, 5000)); EXPECT_ERR(ERROR_MORE_DATA);
    t_note("CallNamedPipe truncated: n=%lu out='%.8s'", n, out);
    CHECK(!memcmp(out, "01234567", 8));
    pmc_join(th); CloseHandle(s);
}
TEST(pmc_call_nonexistent) {
    REQUIRE(U(CallNamedPipeA)); REQUIRE(U(CallNamedPipeW));
    PIPE_NAME(nm); char out[8]; DWORD n = 0;
    SET_ERR(0); CHECK(!U(CallNamedPipeA)(nm, "x", 1, out, 8, &n, 100)); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    WCHAR w[128]; MultiByteToWideChar(CP_ACP, 0, nm, -1, w, 128);
    SET_ERR(0); CHECK(!U(CallNamedPipeW)(w, "x", 1, out, 8, &n, 100)); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    SET_ERR(0); CHECK(!U(CallNamedPipeA)(nm, "x", 1, out, 8, &n, NMPWAIT_NOWAIT)); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(pmc_call_busy_times_out) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(CallNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, MSGMODE, 1, 4096, 4096);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);   /* occupy the only instance */
    char out[8]; DWORD n = 0, t0 = GetTickCount();
    SET_ERR(0);
    CHECK(!U(CallNamedPipeA)(nm, "x", 1, out, 8, &n, 200));
    DWORD e = GetLastError(), dt = GetTickCount() - t0;
    CHECK(e == ERROR_SEM_TIMEOUT || e == ERROR_PIPE_BUSY);
    CHECK(dt < 5000);
    t_note("CallNamedPipe on busy pipe: err=%lu after %lums", e, dt);
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_call_byte_type_pipe) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(CallNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    char out[8]; DWORD n = 0;
    SET_ERR(0);
    CHECK(!U(CallNamedPipeA)(nm, "x", 1, out, 8, &n, 200));
    t_note("CallNamedPipe on byte pipe: err=%lu", GetLastError());
    CloseHandle(s);
}

/* ---- overlapped ---- */
TEST(pmc_ovl_read_pending_then_complete) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    OVERLAPPED ov = {0}; ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    char b[16]; DWORD n = 0;
    SET_ERR(0);
    CHECK(!ReadFile(s, b, 16, NULL, &ov)); EXPECT_ERR(ERROR_IO_PENDING);
    CHECK(!HasOverlappedIoCompleted(&ov));
    SET_ERR(0); CHECK(!GetOverlappedResult(s, &ov, &n, FALSE)); EXPECT_ERR(ERROR_IO_INCOMPLETE);
    CHECK(WriteFile(c, "async", 5, &n, NULL));
    EXPECT_EQ(WaitForSingleObject(ov.hEvent, 5000), WAIT_OBJECT_0);
    CHECK(GetOverlappedResult(s, &ov, &n, FALSE)); EXPECT_EQ(n, 5); CHECK(!memcmp(b, "async", 5));
    CloseHandle(ov.hEvent); CloseHandle(c); CloseHandle(s);
}
TEST(pmc_ovl_read_data_ready_completes_inline) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    OVERLAPPED ov = {0}; ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    char b[16]; DWORD n = 0;
    CHECK(WriteFile(c, "ready", 5, &n, NULL));
    BOOL ok = ReadFile(s, b, 16, &n, &ov);
    if (!ok) { EXPECT_ERR(ERROR_IO_PENDING); }
    CHECK(GetOverlappedResult(s, &ov, &n, TRUE)); EXPECT_EQ(n, 5);
    CloseHandle(ov.hEvent); CloseHandle(c); CloseHandle(s);
}
TEST(pmc_ovl_cancelioex_read) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(CancelIoEx));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    OVERLAPPED ov = {0}; ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    char b[16]; DWORD n = 0;
    SET_ERR(0); CHECK(!ReadFile(s, b, 16, NULL, &ov)); EXPECT_ERR(ERROR_IO_PENDING);
    CHECK(U(CancelIoEx)(s, &ov));
    SET_ERR(0); CHECK(!GetOverlappedResult(s, &ov, &n, TRUE)); EXPECT_ERR(ERROR_OPERATION_ABORTED);
    CloseHandle(ov.hEvent); CloseHandle(c); CloseHandle(s);
}
TEST(pmc_ovl_cancelio_all) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(CancelIo));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    OVERLAPPED ov = {0}; ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    char b[16]; DWORD n = 0;
    SET_ERR(0); CHECK(!ReadFile(s, b, 16, NULL, &ov)); EXPECT_ERR(ERROR_IO_PENDING);
    CHECK(U(CancelIo)(s));
    SET_ERR(0); CHECK(!GetOverlappedResult(s, &ov, &n, TRUE)); EXPECT_ERR(ERROR_OPERATION_ABORTED);
    CloseHandle(ov.hEvent); CloseHandle(c); CloseHandle(s);
}
TEST(pmc_ovl_read_broken_on_client_close) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    OVERLAPPED ov = {0}; ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    char b[16]; DWORD n = 0;
    SET_ERR(0); CHECK(!ReadFile(s, b, 16, NULL, &ov)); EXPECT_ERR(ERROR_IO_PENDING);
    CloseHandle(c);
    EXPECT_EQ(WaitForSingleObject(ov.hEvent, 5000), WAIT_OBJECT_0);
    SET_ERR(0); CHECK(!GetOverlappedResult(s, &ov, &n, FALSE)); EXPECT_ERR(ERROR_BROKEN_PIPE);
    CloseHandle(ov.hEvent); CloseHandle(s);
}
TEST(pmc_ovl_message_more_data) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, MSGMODE, 1, 4096, 4096);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    OVERLAPPED ov = {0}; ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    char b[16]; DWORD n = 0;
    CHECK(WriteFile(c, "0123456789", 10, &n, NULL));
    BOOL ok = ReadFile(s, b, 4, NULL, &ov);
    if (!ok && GetLastError() == ERROR_IO_PENDING) { WaitForSingleObject(ov.hEvent, 5000); }
    SET_ERR(0); ok = GetOverlappedResult(s, &ov, &n, TRUE);
    CHECK(!ok); EXPECT_ERR(ERROR_MORE_DATA); EXPECT_EQ(n, 4);
    CloseHandle(ov.hEvent); CloseHandle(c); CloseHandle(s);
}
TEST(pmc_ovl_client_overlapped_read) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, FILE_FLAG_OVERLAPPED);
    CHECK(c != INVALID_HANDLE_VALUE);
    OVERLAPPED ov = {0}; ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    char b[16]; DWORD n = 0;
    SET_ERR(0); CHECK(!ReadFile(c, b, 16, NULL, &ov)); EXPECT_ERR(ERROR_IO_PENDING);
    CHECK(WriteFile(s, "fromsrv", 7, &n, NULL));
    CHECK(GetOverlappedResult(c, &ov, &n, TRUE)); EXPECT_EQ(n, 7);
    CloseHandle(ov.hEvent); CloseHandle(c); CloseHandle(s);
}
TEST(pmc_ovl_write_completes) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    OVERLAPPED ov = {0}; ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    DWORD n = 0; char b[16];
    BOOL ok = WriteFile(s, "owrite", 6, NULL, &ov);
    if (!ok) EXPECT_ERR(ERROR_IO_PENDING);
    CHECK(GetOverlappedResult(s, &ov, &n, TRUE)); EXPECT_EQ(n, 6);
    CHECK(ReadFile(c, b, 16, &n, NULL)); EXPECT_EQ(n, 6);
    CloseHandle(ov.hEvent); CloseHandle(c); CloseHandle(s);
}
TEST(pmc_ovl_write_large_blocks_until_read) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    OVERLAPPED ov = {0}; ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    static unsigned char big[262144]; pmc_fill(big, sizeof big, 11);
    DWORD n = 0;
    BOOL ok = WriteFile(s, big, sizeof big, NULL, &ov);
    CHECK(!ok); EXPECT_ERR(ERROR_IO_PENDING);
    EXPECT_EQ(WaitForSingleObject(ov.hEvent, 100), WAIT_TIMEOUT);
    static unsigned char rb[262144]; size_t got = 0;
    while (got < sizeof big) { if (!ReadFile(c, rb + got, (DWORD)(sizeof big - got), &n, NULL)) break; got += n; }
    EXPECT_EQ(got, sizeof big);
    CHECK(GetOverlappedResult(s, &ov, &n, TRUE)); EXPECT_EQ(n, sizeof big);
    CHECK(!memcmp(rb, big, sizeof big));
    CloseHandle(ov.hEvent); CloseHandle(c); CloseHandle(s);
}
TEST(pmc_ovl_event_low_bit_suppresses_iocp) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(CreateIoCompletionPort)); REQUIRE(U(GetQueuedCompletionStatus));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    HANDLE port = U(CreateIoCompletionPort)(s, NULL, 5, 0);
    CHECK(port != NULL);
    OVERLAPPED ov = {0}; ov.hEvent = (HANDLE)((ULONG_PTR)CreateEventA(NULL, TRUE, FALSE, NULL) | 1);
    char b[8]; DWORD n = 0; ULONG_PTR key; LPOVERLAPPED pov;
    SET_ERR(0); CHECK(!ReadFile(s, b, 8, NULL, &ov)); EXPECT_ERR(ERROR_IO_PENDING);
    CHECK(WriteFile(c, "x", 1, &n, NULL));
    Sleep(100);
    SET_ERR(0);
    BOOL ok = U(GetQueuedCompletionStatus)(port, &n, &key, &pov, 200);
    CHECK(!ok); EXPECT_ERR(WAIT_TIMEOUT);
    CloseHandle((HANDLE)((ULONG_PTR)ov.hEvent & ~(ULONG_PTR)1)); CloseHandle(port); CloseHandle(c); CloseHandle(s);
}
TEST(pmc_iocp_read_completion) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(CreateIoCompletionPort)); REQUIRE(U(GetQueuedCompletionStatus));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    HANDLE port = U(CreateIoCompletionPort)(s, NULL, 0x1234, 0);
    CHECK(port != NULL);
    OVERLAPPED ov = {0};
    char b[8]; DWORD n = 0; ULONG_PTR key = 0; LPOVERLAPPED pov = NULL;
    SET_ERR(0); CHECK(!ReadFile(s, b, 8, NULL, &ov)); EXPECT_ERR(ERROR_IO_PENDING);
    CHECK(WriteFile(c, "iocp", 4, &n, NULL));
    CHECK(U(GetQueuedCompletionStatus)(port, &n, &key, &pov, 5000));
    EXPECT_EQ(n, 4); EXPECT_EQ(key, 0x1234); CHECK(pov == &ov); CHECK(!memcmp(b, "iocp", 4));
    CloseHandle(port); CloseHandle(c); CloseHandle(s);
}
TEST(pmc_iocp_connect_completion) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(CreateIoCompletionPort)); REQUIRE(U(GetQueuedCompletionStatus)); REQUIRE(U(ConnectNamedPipe));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE port = U(CreateIoCompletionPort)(s, NULL, 77, 0);
    OVERLAPPED ov = {0}; DWORD n = 0; ULONG_PTR key = 0; LPOVERLAPPED pov = NULL;
    SET_ERR(0); CHECK(!U(ConnectNamedPipe)(s, &ov)); EXPECT_ERR(ERROR_IO_PENDING);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    CHECK(U(GetQueuedCompletionStatus)(port, &n, &key, &pov, 5000));
    EXPECT_EQ(key, 77); CHECK(pov == &ov);
    CloseHandle(port); CloseHandle(c); CloseHandle(s);
}
TEST(pmc_iocp_broken_pipe_completion_error) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(CreateIoCompletionPort)); REQUIRE(U(GetQueuedCompletionStatus));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    HANDLE port = U(CreateIoCompletionPort)(s, NULL, 9, 0);
    OVERLAPPED ov = {0}; char b[4]; DWORD n = 0; ULONG_PTR key = 0; LPOVERLAPPED pov = NULL;
    SET_ERR(0); CHECK(!ReadFile(s, b, 4, NULL, &ov)); EXPECT_ERR(ERROR_IO_PENDING);
    CloseHandle(c);
    SET_ERR(0);
    BOOL ok = U(GetQueuedCompletionStatus)(port, &n, &key, &pov, 5000);
    CHECK(!ok); CHECK(pov == &ov); EXPECT_ERR(ERROR_BROKEN_PIPE);
    CloseHandle(port); CloseHandle(s);
}
TEST(pmc_iocp_second_association_fails) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(CreateIoCompletionPort));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE port = U(CreateIoCompletionPort)(s, NULL, 1, 0);
    HANDLE port2 = U(CreateIoCompletionPort)(INVALID_HANDLE_VALUE, NULL, 0, 0);
    SET_ERR(0);
    HANDLE r = U(CreateIoCompletionPort)(s, port2, 2, 0);
    CHECK(r == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CloseHandle(port2); CloseHandle(port); CloseHandle(s);
}
TEST(pmc_iocp_sync_handle_association_fails) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(CreateIoCompletionPort));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    SET_ERR(0);
    HANDLE port = U(CreateIoCompletionPort)(s, NULL, 1, 0);
    t_note("IOCP association of synchronous pipe handle: %s err=%lu", port ? "ok" : "fail", GetLastError());
    if (port) CloseHandle(port);
    CloseHandle(s);
}
TEST(pmc_ovl_getoverlappedresult_wait_event) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    OVERLAPPED ov = {0}; ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    char b[8]; DWORD n = 0;
    SET_ERR(0); CHECK(!ReadFile(s, b, 8, NULL, &ov)); EXPECT_ERR(ERROR_IO_PENDING);
    CHECK(WriteFile(c, "w", 1, &n, NULL));
    CHECK(GetOverlappedResult(s, &ov, &n, TRUE)); EXPECT_EQ(n, 1);
    CloseHandle(ov.hEvent); CloseHandle(c); CloseHandle(s);
}

/* ---- threaded servers ---- */
typedef struct { HANDLE h; DWORD idx; DWORD result; char name[128]; } inst_t;
static DWORD WINAPI inst_server(void *p) {
    inst_t *x = p; char b[64]; DWORD n;
    if (!ConnectNamedPipe(x->h, NULL) && GetLastError() != ERROR_PIPE_CONNECTED) { x->result = 1; return 1; }
    if (!ReadFile(x->h, b, sizeof b, &n, NULL)) { x->result = 2; return 2; }
    for (DWORD i = 0; i < n; i++) b[i] = (char)(b[i] ^ 0x20);
    if (!WriteFile(x->h, b, n, &n, NULL)) { x->result = 3; return 3; }
    FlushFileBuffers(x->h);
    DisconnectNamedPipe(x->h);
    x->result = 0;
    return 0;
}
static DWORD WINAPI inst_client(void *p) {
    inst_t *x = p; char msg[32], out[64]; DWORD n = 0;
    snprintf(msg, sizeof msg, "client-%lu", x->idx);
    for (int tries = 0; tries < 50; tries++) {
        if (CallNamedPipeA(x->name, msg, (DWORD)strlen(msg), out, sizeof out, &n, 2000)) {
            x->result = 0;
            for (DWORD i = 0; i < n; i++) out[i] = (char)(out[i] ^ 0x20);
            if (n != strlen(msg) || memcmp(out, msg, n)) x->result = 99;
            return 0;
        }
        Sleep(20);
    }
    x->result = 100;
    return 1;
}
TEST(pmc_multi_instances_multi_clients) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    enum { N = 8 };
    inst_t srv[N], cli[N]; HANDLE ts[N], tc[N];
    for (int i = 0; i < N; i++) {
        srv[i].h = pmc_server(nm, PIPE_ACCESS_DUPLEX, MSGMODE, N, 512, 512);
        CHECK(srv[i].h != INVALID_HANDLE_VALUE);
        srv[i].idx = i; srv[i].result = 77;
        ts[i] = pmc_thread(inst_server, &srv[i]);
    }
    for (int i = 0; i < N; i++) { cli[i].idx = i; cli[i].result = 77; strcpy(cli[i].name, nm); tc[i] = pmc_thread(inst_client, &cli[i]); }
    for (int i = 0; i < N; i++) { pmc_join(tc[i]); EXPECT_EQ(cli[i].result, 0); }
    for (int i = 0; i < N; i++) { pmc_join(ts[i]); EXPECT_EQ(srv[i].result, 0); CloseHandle(srv[i].h); }
}
static DWORD WINAPI seq_server(void *p) {
    inst_t *x = p; char b[32]; DWORD n; int served = 0;
    for (int i = 0; i < 12; i++) {
        BOOL ok = ConnectNamedPipe(x->h, NULL) || GetLastError() == ERROR_PIPE_CONNECTED;
        if (!ok) break;
        if (ReadFile(x->h, b, sizeof b, &n, NULL) && n) { WriteFile(x->h, b, n, &n, NULL); served++; }
        FlushFileBuffers(x->h);
        DisconnectNamedPipe(x->h);
    }
    x->result = served;
    return 0;
}
TEST(pmc_sequential_clients_one_instance) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    inst_t srv = {0};
    srv.h = pmc_server(nm, PIPE_ACCESS_DUPLEX, MSGMODE, 1, 512, 512);
    HANDLE th = pmc_thread(seq_server, &srv);
    int good = 0;
    for (int i = 0; i < 12; i++) {
        char msg[16], out[32]; DWORD n = 0; snprintf(msg, sizeof msg, "seq%d", i);
        BOOL ok = FALSE;
        for (int t = 0; t < 100 && !ok; t++) { ok = CallNamedPipeA(nm, msg, (DWORD)strlen(msg), out, sizeof out, &n, 2000); if (!ok) Sleep(10); }
        if (ok && n == strlen(msg) && !memcmp(out, msg, n)) good++;
    }
    EXPECT_EQ(good, 12);
    pmc_join(th); EXPECT_EQ(srv.result, 12);
    CloseHandle(srv.h);
}
static DWORD WINAPI delayed_reader(void *p) { Sleep(200); char b[16]; DWORD k; ReadFile((HANDLE)p, b, 16, &k, NULL); return 0; }
TEST(pmc_flush_waits_for_reader) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(FlushFileBuffers));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    DWORD n;
    CHECK(WriteFile(s, "flushme", 7, &n, NULL));
    /* reader drains after a delay */
    HANDLE th = pmc_thread(delayed_reader, c);
    DWORD t0 = GetTickCount();
    CHECK(U(FlushFileBuffers)(s));
    DWORD dt = GetTickCount() - t0;
    CHECKF(dt >= 100, "flush returned after %lums", dt);
    pmc_join(th);
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_flush_no_data_returns_immediately) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(FlushFileBuffers));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    DWORD t0 = GetTickCount();
    CHECK(U(FlushFileBuffers)(s));
    CHECK(GetTickCount() - t0 < 1000);
    CloseHandle(c); CloseHandle(s);
}

/* ---- bulk transfers ---- */
typedef struct { HANDLE h; size_t total; DWORD seed; DWORD sum; size_t done; BOOL isread; } bulk_t;
static DWORD WINAPI bulk_thr(void *p) {
    bulk_t *x = p; unsigned char *buf = malloc(32768); size_t pos = 0; DWORD n, sum = 0;
    while (pos < x->total) {
        size_t c = x->total - pos > 32768 ? 32768 : x->total - pos;
        if (x->isread) {
            if (!ReadFile(x->h, buf, (DWORD)c, &n, NULL) || !n) break;
            sum = pmc_sum(buf, n, pos ? sum : 0); pos += n;
        } else {
            pmc_fill(buf, c, x->seed + (DWORD)(pos >> 15));
            if (!WriteFile(x->h, buf, (DWORD)c, &n, NULL) || n != c) break;
            sum = pmc_sum(buf, c, pos ? sum : 0); pos += c;
        }
    }
    x->sum = sum; x->done = pos; free(buf);
    return 0;
}
TEST(pmc_bulk_8mb_both_directions) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(n1); PIPE_NAME(n2);
    HANDLE s1 = pmc_server(n1, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096);
    HANDLE s2 = pmc_server(n2, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096);
    HANDLE c1 = pmc_client(n1, GENERIC_READ | GENERIC_WRITE, 0);
    HANDLE c2 = pmc_client(n2, GENERIC_READ | GENERIC_WRITE, 0);
    size_t total = 8u << 20;
    bulk_t w1 = { c1, total, 1, 0, 0, FALSE }, r1 = { s1, total, 0, 0, 0, TRUE };
    bulk_t w2 = { s2, total, 2, 0, 0, FALSE }, r2 = { c2, total, 0, 0, 0, TRUE };
    HANDLE t[4] = { pmc_thread(bulk_thr, &w1), pmc_thread(bulk_thr, &r1), pmc_thread(bulk_thr, &w2), pmc_thread(bulk_thr, &r2) };
    for (int i = 0; i < 4; i++) pmc_join(t[i]);
    EXPECT_EQ(w1.done, total); EXPECT_EQ(r1.done, total); EXPECT_EQ(w2.done, total); EXPECT_EQ(r2.done, total);
    EXPECT_EQ(w1.sum, r1.sum); EXPECT_EQ(w2.sum, r2.sum);
    CloseHandle(c1); CloseHandle(c2); CloseHandle(s1); CloseHandle(s2);
}
TEST(pmc_bulk_duplex_same_pipe_overlapped) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, FILE_FLAG_OVERLAPPED);
    /* simultaneous 256 KB each way over a single pair using overlapped I/O */
    enum { SZ = 262144 };
    unsigned char *wa = malloc(SZ), *wb = malloc(SZ), *ra = malloc(SZ), *rb = malloc(SZ);
    pmc_fill(wa, SZ, 5); pmc_fill(wb, SZ, 6);
    OVERLAPPED ow1 = {0}, or1 = {0}, ow2 = {0}, or2 = {0};
    ow1.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL); or1.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    ow2.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL); or2.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    DWORD n;
    /* reads first so writes always make progress */
    BOOL a = ReadFile(s, ra, SZ, NULL, &or1); if (!a) EXPECT_ERR(ERROR_IO_PENDING);
    BOOL b = ReadFile(c, rb, SZ, NULL, &or2); if (!b) EXPECT_ERR(ERROR_IO_PENDING);
    WriteFile(c, wa, SZ, NULL, &ow1); WriteFile(s, wb, SZ, NULL, &ow2);
    DWORD tot_s = 0, tot_c = 0;
    /* byte-mode reads can complete partially: keep re-reading until full */
    CHECK(GetOverlappedResult(s, &or1, &n, TRUE)); tot_s = n;
    CHECK(GetOverlappedResult(c, &or2, &n, TRUE)); tot_c = n;
    while (tot_s < SZ) { OVERLAPPED o = {0}; o.hEvent = or1.hEvent; ResetEvent(o.hEvent); BOOL r = ReadFile(s, ra + tot_s, SZ - tot_s, NULL, &o); if (!r && GetLastError() != ERROR_IO_PENDING) break; if (!GetOverlappedResult(s, &o, &n, TRUE)) break; tot_s += n; }
    while (tot_c < SZ) { OVERLAPPED o = {0}; o.hEvent = or2.hEvent; ResetEvent(o.hEvent); BOOL r = ReadFile(c, rb + tot_c, SZ - tot_c, NULL, &o); if (!r && GetLastError() != ERROR_IO_PENDING) break; if (!GetOverlappedResult(c, &o, &n, TRUE)) break; tot_c += n; }
    CHECK(GetOverlappedResult(c, &ow1, &n, TRUE)); CHECK(GetOverlappedResult(s, &ow2, &n, TRUE));
    EXPECT_EQ(tot_s, SZ); EXPECT_EQ(tot_c, SZ);
    CHECK(!memcmp(ra, wa, SZ)); CHECK(!memcmp(rb, wb, SZ));
    CloseHandle(ow1.hEvent); CloseHandle(or1.hEvent); CloseHandle(ow2.hEvent); CloseHandle(or2.hEvent);
    free(wa); free(wb); free(ra); free(rb);
    CloseHandle(c); CloseHandle(s);
}
typedef struct { HANDLE h; int count; int bad; DWORD seed; BOOL isread; } msgbulk_t;
static DWORD msg_len(int i) { return (DWORD)(((i * 7919) % 20000) + (i % 5 == 0 ? 0 : 1)); }
static DWORD WINAPI msgbulk_thr(void *p) {
    msgbulk_t *x = p; unsigned char *buf = malloc(65536); DWORD n;
    for (int i = 0; i < x->count; i++) {
        DWORD len = msg_len(i);
        if (x->isread) {
            if (!ReadFile(x->h, buf, 65536, &n, NULL)) { x->bad++; break; }
            if (n != len) x->bad++;
            else { unsigned char ref[1]; pmc_fill(ref, 1, i + 1); if (n && buf[0] != ref[0]) x->bad++; }
        } else {
            pmc_fill(buf, len ? len : 1, i + 1);
            if (!WriteFile(x->h, buf, len, &n, NULL) || n != len) { x->bad++; break; }
        }
    }
    free(buf);
    return 0;
}
TEST(pmc_bulk_messages_preserve_boundaries) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, MSGMODE, 1, 4096, 4096);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    msgbulk_t w = { c, 300, 0, 1, FALSE }, r = { s, 300, 0, 1, TRUE };
    HANDLE t1 = pmc_thread(msgbulk_thr, &w), t2 = pmc_thread(msgbulk_thr, &r);
    pmc_join(t1); pmc_join(t2);
    EXPECT_EQ(w.bad, 0); EXPECT_EQ(r.bad, 0);
    CloseHandle(c); CloseHandle(s);
}

/* ---- ids / names ---- */
TEST(pmc_ids_in_process) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(GetNamedPipeClientProcessId)); REQUIRE(U(GetNamedPipeServerProcessId));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    ULONG pid = 0;
    CHECK(U(GetNamedPipeClientProcessId)(s, &pid)); EXPECT_EQ(pid, GetCurrentProcessId());
    pid = 0; CHECK(U(GetNamedPipeServerProcessId)(c, &pid)); EXPECT_EQ(pid, GetCurrentProcessId());
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_ids_sessions_in_process) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(GetNamedPipeClientSessionId)); REQUIRE(U(GetNamedPipeServerSessionId));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    DWORD mysess = 0xffff; ProcessIdToSessionId(GetCurrentProcessId(), &mysess);
    ULONG sid = 0xffff;
    CHECK(U(GetNamedPipeClientSessionId)(s, &sid)); EXPECT_EQ(sid, mysess);
    sid = 0xffff; CHECK(U(GetNamedPipeServerSessionId)(c, &sid)); EXPECT_EQ(sid, mysess);
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_ids_wrong_end_and_invalid) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(GetNamedPipeClientProcessId)); REQUIRE(U(GetNamedPipeServerProcessId));
    REQUIRE(U(GetNamedPipeClientSessionId)); REQUIRE(U(GetNamedPipeServerSessionId));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    ULONG v = 0;
    SET_ERR(0); BOOL ok = U(GetNamedPipeClientProcessId)(c, &v); t_note("ClientProcessId on client end: %d err=%lu v=%lu", ok, ok ? 0 : GetLastError(), v);
    SET_ERR(0); ok = U(GetNamedPipeServerProcessId)(s, &v); t_note("ServerProcessId on server end: %d err=%lu v=%lu", ok, ok ? 0 : GetLastError(), v);
    SET_ERR(0); CHECK(!U(GetNamedPipeClientProcessId)(NULL, &v)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(GetNamedPipeServerProcessId)(NULL, &v)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(GetNamedPipeClientSessionId)(NULL, &v)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(GetNamedPipeServerSessionId)(NULL, &v)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_ids_on_non_pipe) {
    REQUIRE(U(GetNamedPipeClientProcessId));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "pmc_ids.bin");
    HANDLE h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    ULONG v;
    SET_ERR(0); CHECK(!U(GetNamedPipeClientProcessId)(h, &v)); CHECK(GetLastError() != 0);
    CloseHandle(h); DeleteFileA(p);
}
TEST(pmc_ids_not_connected_server) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(GetNamedPipeClientProcessId));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    ULONG v = 0;
    SET_ERR(0); BOOL ok = U(GetNamedPipeClientProcessId)(s, &v);
    t_note("ClientProcessId with no client: %d err=%lu v=%lu", ok, ok ? 0 : GetLastError(), v);
    CloseHandle(s);
}
TEST(pmc_client_computer_name) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(GetNamedPipeClientComputerNameA)); REQUIRE(U(GetNamedPipeClientComputerNameW));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    char a[MAX_COMPUTERNAME_LENGTH + 2] = {0}; WCHAR w[MAX_COMPUTERNAME_LENGTH + 2] = {0};
    SET_ERR(0); BOOL oka = U(GetNamedPipeClientComputerNameA)(s, a, sizeof a); DWORD ea = GetLastError();
    SET_ERR(0); BOOL okw = U(GetNamedPipeClientComputerNameW)(s, w, MAX_COMPUTERNAME_LENGTH + 2); DWORD ew = GetLastError();
    t_note("ClientComputerNameA ok=%d err=%lu '%s'; W ok=%d err=%lu", oka, ea, a, okw, ew);
    EXPECT_EQ(oka, okw);
    if (!oka) EXPECT_EQ(ea, ew);
    SET_ERR(0); CHECK(!U(GetNamedPipeClientComputerNameA)(NULL, a, sizeof a)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_client_computer_name_small_buffer) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(GetNamedPipeClientComputerNameA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    char a[2];
    SET_ERR(0); BOOL ok = U(GetNamedPipeClientComputerNameA)(s, a, 1);
    t_note("ClientComputerNameA with 1-byte buffer: ok=%d err=%lu", ok, ok ? 0 : GetLastError());
    CloseHandle(c); CloseHandle(s);
}
