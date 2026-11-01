/* CreateNamedPipe argument handling, instance limits, client open, Connect/Disconnect/Wait. */
#include "pmc.h"

#define PIPE_NAME(n) char n[128]; pmc_pipename(n, sizeof n)

TEST(pmc_np_create_duplex_basic) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096);
    CHECK(s != INVALID_HANDLE_VALUE);
    EXPECT_EQ(GetFileType(s), FILE_TYPE_PIPE);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    CHECK(c != INVALID_HANDLE_VALUE);
    EXPECT_EQ(GetFileType(c), FILE_TYPE_PIPE);
    DWORD n; char b[8];
    CHECK(WriteFile(c, "ping", 4, &n, NULL)); CHECK(ReadFile(s, b, 8, &n, NULL)); EXPECT_EQ(n, 4); CHECK(!memcmp(b, "ping", 4));
    CHECK(WriteFile(s, "pong!", 5, &n, NULL)); CHECK(ReadFile(c, b, 8, &n, NULL)); EXPECT_EQ(n, 5); CHECK(!memcmp(b, "pong!", 5));
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_np_create_wide_basic) {
    REQUIRE(U(CreateNamedPipeW));
    WCHAR nm[128]; pmc_pipenamew(nm, 128);
    HANDLE s = U(CreateNamedPipeW)(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512, 0, NULL);
    CHECK(s != INVALID_HANDLE_VALUE);
    HANDLE c = CreateFileW(nm, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(c != INVALID_HANDLE_VALUE);
    DWORD n; char b[4];
    CHECK(WriteFile(c, "w", 1, &n, NULL)); CHECK(ReadFile(s, b, 4, &n, NULL)); EXPECT_EQ(n, 1);
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_np_inbound_server) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096);
    CHECK(s != INVALID_HANDLE_VALUE);
    SET_ERR(0);
    HANDLE c = pmc_client(nm, GENERIC_READ, 0);
    CHECK(c == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    c = pmc_client(nm, GENERIC_WRITE, 0);
    CHECK(c != INVALID_HANDLE_VALUE);
    DWORD n; char b[4];
    CHECK(WriteFile(c, "in", 2, &n, NULL)); CHECK(ReadFile(s, b, 4, &n, NULL)); EXPECT_EQ(n, 2);
    SET_ERR(0); CHECK(!WriteFile(s, "x", 1, &n, NULL)); EXPECT_ERR(ERROR_ACCESS_DENIED);
    SET_ERR(0); CHECK(!ReadFile(c, b, 1, &n, NULL)); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_np_outbound_server) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_OUTBOUND, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096);
    CHECK(s != INVALID_HANDLE_VALUE);
    SET_ERR(0);
    HANDLE c = pmc_client(nm, GENERIC_WRITE, 0);
    CHECK(c == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    c = pmc_client(nm, GENERIC_READ, 0);
    CHECK(c != INVALID_HANDLE_VALUE);
    DWORD n; char b[4];
    CHECK(WriteFile(s, "out", 3, &n, NULL)); CHECK(ReadFile(c, b, 4, &n, NULL)); EXPECT_EQ(n, 3);
    SET_ERR(0); CHECK(!ReadFile(s, b, 1, &n, NULL)); EXPECT_ERR(ERROR_ACCESS_DENIED);
    SET_ERR(0); CHECK(!WriteFile(c, "x", 1, &n, NULL)); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_np_client_access_combos) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    DWORD acc[] = { GENERIC_READ, GENERIC_WRITE, GENERIC_READ | GENERIC_WRITE, FILE_READ_DATA, FILE_WRITE_DATA, FILE_READ_DATA | FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES, SYNCHRONIZE | FILE_READ_ATTRIBUTES };
    for (int i = 0; i < 7; i++) {
        HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 4, 4096, 4096);
        HANDLE c = pmc_client(nm, acc[i], 0);
        CHECKF(c != INVALID_HANDLE_VALUE, "access 0x%lx err %lu", acc[i], GetLastError());
        if (c != INVALID_HANDLE_VALUE) { EXPECT_EQ(GetFileType(c), FILE_TYPE_PIPE); CloseHandle(c); }
        CloseHandle(s);
    }
}
TEST(pmc_np_invalid_open_mode) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    DWORD bad[] = { 0, 4, 8, 0x10, 0x100, 0x00200000 | 3, 0x04000000 | 3, 0x20000000 | 3, 0x1000 | 3 };
    for (int i = 0; i < 9; i++) {
        SET_ERR(0);
        HANDLE h = pmc_server(nm, bad[i], PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
        CHECKF(h == INVALID_HANDLE_VALUE, "mode 0x%lx unexpectedly succeeded", bad[i]);
        if (h == INVALID_HANDLE_VALUE) { CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "mode 0x%lx err %lu", bad[i], GetLastError()); }
        else CloseHandle(h);
    }
}
TEST(pmc_np_invalid_pipe_mode_bits) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    DWORD bad[] = { 0x10, 0x20, 0x100, 0x80000000, 0xffff };
    for (int i = 0; i < 5; i++) {
        SET_ERR(0);
        HANDLE h = pmc_server(nm, PIPE_ACCESS_DUPLEX, bad[i], 1, 512, 512);
        CHECKF(h == INVALID_HANDLE_VALUE, "pipemode 0x%lx unexpectedly succeeded", bad[i]);
        if (h == INVALID_HANDLE_VALUE) CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "pipemode 0x%lx err %lu", bad[i], GetLastError());
        else CloseHandle(h);
    }
}
TEST(pmc_np_byte_type_message_readmode_rejected) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    SET_ERR(0);
    HANDLE h = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 1, 512, 512);
    CHECK(h == INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) EXPECT_ERR(ERROR_INVALID_PARAMETER); else CloseHandle(h);
}
TEST(pmc_np_max_instances_bounds) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    DWORD bad[] = { 0, 256, 257, 1000, 0x7fffffff, 0xfffffffe };
    for (int i = 0; i < 6; i++) {
        SET_ERR(0);
        HANDLE h = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, bad[i], 512, 512);
        CHECKF(h == INVALID_HANDLE_VALUE, "max %lu succeeded", bad[i]);
        if (h == INVALID_HANDLE_VALUE) CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "max %lu err %lu", bad[i], GetLastError());
        else CloseHandle(h);
    }
    DWORD good[] = { 1, 2, 100, 254, PIPE_UNLIMITED_INSTANCES };
    for (int i = 0; i < 5; i++) {
        PIPE_NAME(n2);
        HANDLE h = pmc_server(n2, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, good[i], 512, 512);
        CHECKF(h != INVALID_HANDLE_VALUE, "max %lu err %lu", good[i], GetLastError());
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }
}
TEST(pmc_np_max_instances_busy) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    HANDLE a = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 2, 512, 512);
    HANDLE b = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 2, 512, 512);
    CHECK(a != INVALID_HANDLE_VALUE); CHECK(b != INVALID_HANDLE_VALUE);
    SET_ERR(0);
    HANDLE c = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 2, 512, 512);
    CHECK(c == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_PIPE_BUSY);
    CloseHandle(b);
    c = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 2, 512, 512);
    CHECK(c != INVALID_HANDLE_VALUE);   /* slot freed */
    if (c != INVALID_HANDLE_VALUE) CloseHandle(c);
    CloseHandle(a);
}
TEST(pmc_np_single_instance_second_busy) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    HANDLE a = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    SET_ERR(0);
    HANDLE b = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    CHECK(b == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_PIPE_BUSY);
    CloseHandle(a);
}
TEST(pmc_np_first_pipe_instance) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    HANDLE a = pmc_server(nm, PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE, PIPE_TYPE_BYTE | PIPE_WAIT, 4, 512, 512);
    CHECK(a != INVALID_HANDLE_VALUE);
    SET_ERR(0);
    HANDLE b = pmc_server(nm, PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE, PIPE_TYPE_BYTE | PIPE_WAIT, 4, 512, 512);
    CHECK(b == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    b = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 4, 512, 512);
    CHECK(b != INVALID_HANDLE_VALUE);   /* without the flag a second instance is fine */
    if (b != INVALID_HANDLE_VALUE) CloseHandle(b);
    CloseHandle(a);
}
TEST(pmc_np_first_pipe_instance_after_close) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    HANDLE a = pmc_server(nm, PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    CHECK(a != INVALID_HANDLE_VALUE);
    CloseHandle(a);
    a = pmc_server(nm, PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    CHECK(a != INVALID_HANDLE_VALUE);
    if (a != INVALID_HANDLE_VALUE) CloseHandle(a);
}
TEST(pmc_np_mismatched_instance_mode) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    HANDLE a = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 4, 512, 512);
    SET_ERR(0);
    HANDLE b = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 4, 512, 512);
    if (b == INVALID_HANDLE_VALUE) t_note("type mismatch err=%lu", GetLastError()); else { t_note("type mismatch accepted"); CloseHandle(b); }
    SET_ERR(0);
    b = pmc_server(nm, PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE | PIPE_WAIT, 4, 512, 512);
    if (b == INVALID_HANDLE_VALUE) t_note("access mismatch err=%lu", GetLastError()); else { t_note("access mismatch accepted"); CloseHandle(b); }
    SET_ERR(0);
    b = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 5, 512, 512);
    if (b == INVALID_HANDLE_VALUE) t_note("maxinst mismatch err=%lu", GetLastError()); else { t_note("maxinst mismatch accepted"); CloseHandle(b); }
    CloseHandle(a);
}
TEST(pmc_np_empty_name) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(CreateNamedPipeW));
    SET_ERR(0);
    HANDLE h = pmc_server("", PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    CHECK(h == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    SET_ERR(0);
    h = U(CreateNamedPipeW)(L"", PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512, 0, NULL);
    CHECK(h == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(pmc_np_name_without_pipe_prefix) {
    REQUIRE(U(CreateNamedPipeA));
    char nm[64]; snprintf(nm, sizeof nm, "k32t_nopipe_%lu", GetCurrentProcessId());
    SET_ERR(0);
    HANDLE h = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    CHECK(h == INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) EXPECT_ERR(ERROR_INVALID_NAME); else CloseHandle(h);
    DeleteFileA(nm);
}
TEST(pmc_np_name_wrong_device_prefix) {
    REQUIRE(U(CreateNamedPipeA));
    char nm[160]; snprintf(nm, sizeof nm, "%sk32t_np_%lu", t_tmpdir(), GetCurrentProcessId());
    SET_ERR(0);
    HANDLE h = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    CHECK(h == INVALID_HANDLE_VALUE);
    if (h == INVALID_HANDLE_VALUE) EXPECT_ERR(ERROR_INVALID_NAME); else CloseHandle(h);
    CHECK(GetFileAttributesA(nm) == INVALID_FILE_ATTRIBUTES);
}
TEST(pmc_np_name_case_insensitive) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 2, 512, 512);
    char up[128]; strcpy(up, nm); for (char *p = up; *p; p++) *p = (*p >= 'a' && *p <= 'z') ? *p - 32 : *p;
    HANDLE c = pmc_client(up, GENERIC_READ | GENERIC_WRITE, 0);
    CHECK(c != INVALID_HANDLE_VALUE);
    /* second instance created with different case joins the same pipe */
    SET_ERR(0);
    HANDLE s2 = pmc_server(up, PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE, PIPE_TYPE_BYTE | PIPE_WAIT, 2, 512, 512);
    CHECK(s2 == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    if (c != INVALID_HANDLE_VALUE) CloseHandle(c);
    CloseHandle(s);
}
TEST(pmc_np_name_forward_slashes) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    char fs[128]; strcpy(fs, nm); for (char *p = fs; *p; p++) if (*p == '\\') *p = '/';
    HANDLE c = CreateFileA(fs, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    t_note("client open with forward slashes: %s err=%lu", c != INVALID_HANDLE_VALUE ? "ok" : "fail", GetLastError());
    if (c != INVALID_HANDLE_VALUE) CloseHandle(c);
    CloseHandle(s);
}
TEST(pmc_np_name_hierarchical) {
    REQUIRE(U(CreateNamedPipeA));
    char nm[160]; snprintf(nm, sizeof nm, "\\\\.\\pipe\\k32t_%lu_dir\\sub\\leaf", GetCurrentProcessId());
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    CHECK(s != INVALID_HANDLE_VALUE);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    CHECK(c != INVALID_HANDLE_VALUE);
    if (c != INVALID_HANDLE_VALUE) CloseHandle(c);
    if (s != INVALID_HANDLE_VALUE) CloseHandle(s);
}
TEST(pmc_np_name_long) {
    REQUIRE(U(CreateNamedPipeA));
    char nm[600]; int k = snprintf(nm, sizeof nm, "\\\\.\\pipe\\k32t_%lu_", GetCurrentProcessId());
    for (int i = 0; i < 200; i++) nm[k++] = 'L';
    nm[k] = 0;
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    CHECK(s != INVALID_HANDLE_VALUE);
    if (s != INVALID_HANDLE_VALUE) { HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0); CHECK(c != INVALID_HANDLE_VALUE); if (c != INVALID_HANDLE_VALUE) CloseHandle(c); CloseHandle(s); }
}
TEST(pmc_np_name_unicode) {
    REQUIRE(U(CreateNamedPipeW));
    WCHAR nm[128]; wsprintfW(nm, L"\\\\.\\pipe\\k32t_%lu_\x00e9\x4e2d\x6587", GetCurrentProcessId());
    HANDLE s = U(CreateNamedPipeW)(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512, 0, NULL);
    CHECK(s != INVALID_HANDLE_VALUE);
    if (s != INVALID_HANDLE_VALUE) {
        HANDLE c = CreateFileW(nm, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        CHECK(c != INVALID_HANDLE_VALUE); if (c != INVALID_HANDLE_VALUE) CloseHandle(c); CloseHandle(s);
    }
}
TEST(pmc_np_buffer_sizes) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(GetNamedPipeInfo));
    DWORD sz[][2] = { {0, 0}, {1, 1}, {0, 4096}, {4096, 0}, {65536, 65536}, {1, 65536} };
    for (int i = 0; i < 6; i++) {
        PIPE_NAME(nm);
        HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, sz[i][0], sz[i][1]);
        CHECKF(s != INVALID_HANDLE_VALUE, "sizes %lu/%lu err %lu", sz[i][0], sz[i][1], GetLastError());
        if (s == INVALID_HANDLE_VALUE) continue;
        HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
        CHECK(c != INVALID_HANDLE_VALUE);   /* no I/O: a write larger than a tiny quota could legitimately block */
        DWORD fl, ob, ib;
        CHECK(U(GetNamedPipeInfo)(s, &fl, &ob, &ib, NULL));
        t_note("requested %lu/%lu -> reported out=%lu in=%lu", sz[i][0], sz[i][1], ob, ib);
        CloseHandle(c); CloseHandle(s);
    }
}
TEST(pmc_np_getinfo_server_client) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(GetNamedPipeInfo));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 5, 4096, 8192);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    DWORD fl = 0xff, ob = 0, ib = 0, mx = 0;
    CHECK(U(GetNamedPipeInfo)(s, &fl, &ob, &ib, &mx));
    EXPECT_EQ(fl, PIPE_SERVER_END | PIPE_TYPE_BYTE); EXPECT_EQ(mx, 5);
    t_note("server out=%lu in=%lu (requested 4096/8192)", ob, ib);
    CHECK(ob >= 4096 || ob == 0); CHECK(ib >= 8192 || ib == 0);
    fl = 0xff; mx = 0;
    CHECK(U(GetNamedPipeInfo)(c, &fl, &ob, &ib, &mx));
    EXPECT_EQ(fl, PIPE_CLIENT_END | PIPE_TYPE_BYTE); EXPECT_EQ(mx, 5);
    t_note("client out=%lu in=%lu", ob, ib);
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_np_getinfo_message_type) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(GetNamedPipeInfo));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 3, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    DWORD fl = 0;
    CHECK(U(GetNamedPipeInfo)(s, &fl, NULL, NULL, NULL)); EXPECT_EQ(fl, PIPE_SERVER_END | PIPE_TYPE_MESSAGE);
    CHECK(U(GetNamedPipeInfo)(c, &fl, NULL, NULL, NULL)); EXPECT_EQ(fl, PIPE_CLIENT_END | PIPE_TYPE_MESSAGE);
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_np_getinfo_unlimited) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(GetNamedPipeInfo));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, 512, 512);
    DWORD mx = 0;
    CHECK(U(GetNamedPipeInfo)(s, NULL, NULL, NULL, &mx)); EXPECT_EQ(mx, PIPE_UNLIMITED_INSTANCES);
    CloseHandle(s);
}
TEST(pmc_np_handlestate_cur_instances) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(GetNamedPipeHandleStateA));
    PIPE_NAME(nm);
    HANDLE s[3]; DWORD cur = 0;
    for (int i = 0; i < 3; i++) {
        s[i] = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 8, 512, 512);
        CHECK(U(GetNamedPipeHandleStateA)(s[0], NULL, &cur, NULL, NULL, NULL, 0));
        EXPECT_EQ(cur, i + 1);
    }
    CloseHandle(s[2]);
    CHECK(U(GetNamedPipeHandleStateA)(s[0], NULL, &cur, NULL, NULL, NULL, 0)); EXPECT_EQ(cur, 2);
    CloseHandle(s[1]); CloseHandle(s[0]);
}
TEST(pmc_np_handlestate_flags) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(GetNamedPipeHandleStateA));
    PIPE_NAME(nm);
    struct { DWORD mode, want; } t[] = {
        { PIPE_TYPE_BYTE | PIPE_WAIT, 0 },
        { PIPE_TYPE_BYTE | PIPE_NOWAIT, PIPE_NOWAIT },
        { PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, PIPE_READMODE_MESSAGE },
        { PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_NOWAIT, PIPE_READMODE_MESSAGE | PIPE_NOWAIT },
        { PIPE_TYPE_MESSAGE | PIPE_READMODE_BYTE | PIPE_WAIT, 0 },
    };
    for (int i = 0; i < 5; i++) {
        HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, t[i].mode, 1, 512, 512);
        DWORD st = 0xff;
        CHECK(U(GetNamedPipeHandleStateA)(s, &st, NULL, NULL, NULL, NULL, 0));
        CHECKF(st == t[i].want, "mode 0x%lx state 0x%lx want 0x%lx", t[i].mode, st, t[i].want);
        CloseHandle(s);
    }
}
TEST(pmc_np_handlestate_wide_parity) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(GetNamedPipeHandleStateA)); REQUIRE(U(GetNamedPipeHandleStateW));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_NOWAIT, 2, 512, 512);
    DWORD a = 1, w = 2, ca = 3, cw = 4;
    /* p1: documented: lpMaxCollectionCount/lpCollectDataTimeout must be NULL for a server-end handle or same-machine client */
    CHECK(U(GetNamedPipeHandleStateA)(s, &a, &ca, NULL, NULL, NULL, 0));
    CHECK(U(GetNamedPipeHandleStateW)(s, &w, &cw, NULL, NULL, NULL, 0));
    EXPECT_EQ(a, w); EXPECT_EQ(ca, cw);
    CloseHandle(s);
}
TEST(pmc_np_handlestate_username) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(GetNamedPipeHandleStateA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    char u[256] = {0};
    BOOL ok = U(GetNamedPipeHandleStateA)(s, NULL, NULL, NULL, NULL, u, sizeof u);
    t_note("server username ok=%d err=%lu '%s'", ok, ok ? 0 : GetLastError(), u);
    if (ok) CHECK(u[0] != 0);
    char tiny[2] = {0};
    SET_ERR(0);
    ok = U(GetNamedPipeHandleStateA)(s, NULL, NULL, NULL, NULL, tiny, 1);
    t_note("server username tiny buffer ok=%d err=%lu", ok, ok ? 0 : GetLastError());
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_np_connect_client_already_connected) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(ConnectNamedPipe));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    CHECK(c != INVALID_HANDLE_VALUE);
    SET_ERR(0);
    CHECK(!U(ConnectNamedPipe)(s, NULL)); EXPECT_ERR(ERROR_PIPE_CONNECTED);
    SET_ERR(0);
    CHECK(!U(ConnectNamedPipe)(s, NULL)); EXPECT_ERR(ERROR_PIPE_CONNECTED);   /* repeatable */
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_np_connect_after_client_closed) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(ConnectNamedPipe));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    CloseHandle(c);
    SET_ERR(0);
    CHECK(!U(ConnectNamedPipe)(s, NULL)); EXPECT_ERR(ERROR_NO_DATA);
    CloseHandle(s);
}
TEST(pmc_np_connect_overlapped_pending) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(ConnectNamedPipe));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    OVERLAPPED ov = {0}; ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    CHECK(!U(ConnectNamedPipe)(s, &ov)); EXPECT_ERR(ERROR_IO_PENDING);
    EXPECT_EQ(WaitForSingleObject(ov.hEvent, 50), WAIT_TIMEOUT);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    CHECK(c != INVALID_HANDLE_VALUE);
    EXPECT_EQ(WaitForSingleObject(ov.hEvent, 5000), WAIT_OBJECT_0);
    DWORD n = 99;
    CHECK(GetOverlappedResult(s, &ov, &n, FALSE));
    CloseHandle(c); CloseHandle(s); CloseHandle(ov.hEvent);
}
TEST(pmc_np_connect_overlapped_client_already_there) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(ConnectNamedPipe));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    OVERLAPPED ov = {0}; ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    CHECK(!U(ConnectNamedPipe)(s, &ov)); EXPECT_ERR(ERROR_PIPE_CONNECTED);
    CloseHandle(c); CloseHandle(s); CloseHandle(ov.hEvent);
}
TEST(pmc_np_connect_nowait_listening) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(ConnectNamedPipe));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_NOWAIT, 1, 512, 512);
    SET_ERR(0);
    CHECK(!U(ConnectNamedPipe)(s, NULL)); EXPECT_ERR(ERROR_PIPE_LISTENING);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    SET_ERR(0);
    CHECK(!U(ConnectNamedPipe)(s, NULL)); EXPECT_ERR(ERROR_PIPE_CONNECTED);
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_np_connect_invalid_handles) {
    REQUIRE(U(ConnectNamedPipe)); REQUIRE(U(DisconnectNamedPipe));
    SET_ERR(0); CHECK(!U(ConnectNamedPipe)(NULL, NULL)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(DisconnectNamedPipe)(NULL)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(ConnectNamedPipe)(INVALID_HANDLE_VALUE, NULL)); CHECK(GetLastError() != 0);
    SET_ERR(0); CHECK(!U(DisconnectNamedPipe)(INVALID_HANDLE_VALUE)); CHECK(GetLastError() != 0);
}
TEST(pmc_np_connect_on_file_handle) {
    REQUIRE(U(ConnectNamedPipe)); REQUIRE(U(DisconnectNamedPipe));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "pmc_conn.bin");
    HANDLE h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    SET_ERR(0); CHECK(!U(ConnectNamedPipe)(h, NULL)); CHECK_ERR_ONE_OF4(ERROR_INVALID_FUNCTION, ERROR_INVALID_HANDLE, ERROR_INVALID_PARAMETER, ERROR_NOT_SUPPORTED);
    SET_ERR(0); CHECK(!U(DisconnectNamedPipe)(h)); CHECK_ERR_ONE_OF4(ERROR_INVALID_FUNCTION, ERROR_INVALID_HANDLE, ERROR_INVALID_PARAMETER, ERROR_NOT_SUPPORTED);
    CloseHandle(h); DeleteFileA(p);
}
TEST(pmc_np_disconnect_client_sees_broken) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(DisconnectNamedPipe));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    DWORD n = 5; char b[4];
    CHECK(U(DisconnectNamedPipe)(s));
    SET_ERR(0); CHECK(!ReadFile(c, b, 4, &n, NULL)); CHECK_ERR_ONE_OF2(ERROR_BROKEN_PIPE, ERROR_PIPE_NOT_CONNECTED);
    SET_ERR(0); CHECK(!WriteFile(c, "x", 1, &n, NULL)); CHECK_ERR_ONE_OF3(ERROR_NO_DATA, ERROR_BROKEN_PIPE, ERROR_PIPE_NOT_CONNECTED);
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_np_disconnect_discards_unread_data) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(DisconnectNamedPipe));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    DWORD n, av = 9;
    CHECK(WriteFile(s, "unread", 6, &n, NULL));
    CHECK(U(DisconnectNamedPipe)(s));
    char b[8];
    SET_ERR(0); CHECK(!ReadFile(c, b, 8, &n, NULL)); CHECK_ERR_ONE_OF2(ERROR_BROKEN_PIPE, ERROR_PIPE_NOT_CONNECTED);
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_np_disconnect_reconnect_cycle) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(DisconnectNamedPipe)); REQUIRE(U(ConnectNamedPipe));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    OVERLAPPED ov = {0}; ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    for (int i = 0; i < 10; i++) {
        ResetEvent(ov.hEvent);
        SET_ERR(0);
        BOOL ok = U(ConnectNamedPipe)(s, &ov);
        CHECK(!ok); EXPECT_ERR(ERROR_IO_PENDING);
        HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
        CHECKF(c != INVALID_HANDLE_VALUE, "iteration %d err %lu", i, GetLastError());
        if (c == INVALID_HANDLE_VALUE) break;
        EXPECT_EQ(WaitForSingleObject(ov.hEvent, 5000), WAIT_OBJECT_0);
        DWORD n; char b[8], m = (char)('A' + i);
        CHECK(WriteFile(c, &m, 1, &n, NULL));
        OVERLAPPED rv = {0}; rv.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
        BOOL r = ReadFile(s, b, 8, &n, &rv);
        if (!r && GetLastError() == ERROR_IO_PENDING) r = GetOverlappedResult(s, &rv, &n, TRUE);
        CHECK(r); EXPECT_EQ(n, 1); CHECK(b[0] == m);
        CloseHandle(rv.hEvent);
        CloseHandle(c);
        CHECK(U(DisconnectNamedPipe)(s));
    }
    CloseHandle(ov.hEvent); CloseHandle(s);
}
TEST(pmc_np_disconnect_without_connection) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(DisconnectNamedPipe));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    SET_ERR(0);
    BOOL ok = U(DisconnectNamedPipe)(s);
    t_note("DisconnectNamedPipe on listening instance: %d err=%lu", ok, ok ? 0 : GetLastError());
    CloseHandle(s);
}
TEST(pmc_np_disconnect_on_client_handle) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(DisconnectNamedPipe));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    SET_ERR(0);
    BOOL ok = U(DisconnectNamedPipe)(c);
    CHECK(!ok);
    t_note("DisconnectNamedPipe on client end err=%lu", GetLastError());
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_np_server_close_client_reads_data_then_broken) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    DWORD n; char b[16];
    CHECK(WriteFile(s, "last words", 10, &n, NULL));
    CloseHandle(s);
    CHECK(ReadFile(c, b, 16, &n, NULL)); EXPECT_EQ(n, 10);
    SET_ERR(0); CHECK(!ReadFile(c, b, 16, &n, NULL)); EXPECT_ERR(ERROR_BROKEN_PIPE);
    CloseHandle(c);
}
TEST(pmc_np_client_close_server_read_broken) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    DWORD n; char b[16];
    CHECK(WriteFile(c, "bye", 3, &n, NULL));
    CloseHandle(c);
    CHECK(ReadFile(s, b, 16, &n, NULL)); EXPECT_EQ(n, 3);
    SET_ERR(0); CHECK(!ReadFile(s, b, 16, &n, NULL)); EXPECT_ERR(ERROR_BROKEN_PIPE);
    SET_ERR(0); CHECK(!WriteFile(s, "x", 1, &n, NULL)); CHECK_ERR_ONE_OF2(ERROR_NO_DATA, ERROR_BROKEN_PIPE);
    CloseHandle(s);
}
TEST(pmc_np_second_client_busy) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    SET_ERR(0);
    HANDLE c2 = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    CHECK(c2 == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_PIPE_BUSY);
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_np_second_client_other_instance) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s1 = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 2, 512, 512);
    HANDLE s2 = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 2, 512, 512);
    HANDLE c1 = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    HANDLE c2 = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    CHECK(c1 != INVALID_HANDLE_VALUE); CHECK(c2 != INVALID_HANDLE_VALUE);
    SET_ERR(0);
    HANDLE c3 = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    CHECK(c3 == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_PIPE_BUSY);
    DWORD n; char b[4];
    CHECK(WriteFile(c1, "1", 1, &n, NULL)); CHECK(WriteFile(c2, "2", 1, &n, NULL));
    /* which instance a client lands on is unspecified; both must see exactly one byte */
    CHECK(ReadFile(s1, b, 4, &n, NULL)); EXPECT_EQ(n, 1);
    CHECK(ReadFile(s2, b, 4, &n, NULL)); EXPECT_EQ(n, 1);
    CloseHandle(c1); CloseHandle(c2); CloseHandle(s1); CloseHandle(s2);
}
TEST(pmc_np_client_nonexistent) {
    PIPE_NAME(nm);
    SET_ERR(0);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    CHECK(c == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(pmc_np_client_create_new_fails) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    SET_ERR(0);
    HANDLE c = CreateFileA(nm, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
    t_note("CREATE_NEW on existing pipe: %s err=%lu", c != INVALID_HANDLE_VALUE ? "ok" : "fail", GetLastError());
    if (c != INVALID_HANDLE_VALUE) CloseHandle(c);
    CloseHandle(s);
}
TEST(pmc_np_client_sqos_flags) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    DWORD fl[] = { SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS, SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, SECURITY_SQOS_PRESENT | SECURITY_IMPERSONATION, SECURITY_SQOS_PRESENT | SECURITY_DELEGATION, SECURITY_SQOS_PRESENT | SECURITY_IMPERSONATION | SECURITY_CONTEXT_TRACKING | SECURITY_EFFECTIVE_ONLY };
    for (int i = 0; i < 5; i++) {
        HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
        HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, fl[i]);
        CHECKF(c != INVALID_HANDLE_VALUE, "sqos 0x%lx err %lu", fl[i], GetLastError());
        if (c != INVALID_HANDLE_VALUE) CloseHandle(c);
        CloseHandle(s);
    }
}
TEST(pmc_np_client_share_modes) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    DWORD sh[] = { 0, FILE_SHARE_READ, FILE_SHARE_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE };
    for (int i = 0; i < 4; i++) {
        HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 2, 512, 512);
        HANDLE c = CreateFileA(nm, GENERIC_READ | GENERIC_WRITE, sh[i], NULL, OPEN_EXISTING, 0, NULL);
        CHECKF(c != INVALID_HANDLE_VALUE, "share 0x%lx err %lu", sh[i], GetLastError());
        if (c != INVALID_HANDLE_VALUE) CloseHandle(c);
        CloseHandle(s);
    }
}
TEST(pmc_np_wait_nonexistent) {
    REQUIRE(U(WaitNamedPipeA)); REQUIRE(U(WaitNamedPipeW));
    PIPE_NAME(nm);
    SET_ERR(0); CHECK(!U(WaitNamedPipeA)(nm, 10)); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    WCHAR w[128]; MultiByteToWideChar(CP_ACP, 0, nm, -1, w, 128);
    SET_ERR(0); CHECK(!U(WaitNamedPipeW)(w, 10)); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    SET_ERR(0); CHECK(!U(WaitNamedPipeA)(nm, NMPWAIT_USE_DEFAULT_WAIT)); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    SET_ERR(0); CHECK(!U(WaitNamedPipeA)(nm, 0)); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(pmc_np_wait_listening_returns_true) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(WaitNamedPipeA)); REQUIRE(U(WaitNamedPipeW));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    CHECK(U(WaitNamedPipeA)(nm, 10));
    CHECK(U(WaitNamedPipeA)(nm, NMPWAIT_USE_DEFAULT_WAIT));
    CHECK(U(WaitNamedPipeA)(nm, NMPWAIT_WAIT_FOREVER));
    WCHAR w[128]; MultiByteToWideChar(CP_ACP, 0, nm, -1, w, 128);
    CHECK(U(WaitNamedPipeW)(w, 10));
    CloseHandle(s);
}
TEST(pmc_np_wait_busy_times_out) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(WaitNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    DWORD t0 = GetTickCount();
    SET_ERR(0); CHECK(!U(WaitNamedPipeA)(nm, 200)); EXPECT_ERR(ERROR_SEM_TIMEOUT);
    DWORD dt = GetTickCount() - t0;
    CHECKF(dt >= 150 && dt < 3000, "elapsed %lu", dt);
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_np_wait_default_timeout_uses_pipe_default) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(WaitNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s = U(CreateNamedPipeA)(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512, 300, NULL);
    CHECK(s != INVALID_HANDLE_VALUE);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    DWORD t0 = GetTickCount();
    SET_ERR(0); CHECK(!U(WaitNamedPipeA)(nm, NMPWAIT_USE_DEFAULT_WAIT)); EXPECT_ERR(ERROR_SEM_TIMEOUT);
    DWORD dt = GetTickCount() - t0;
    CHECKF(dt >= 250 && dt < 3000, "elapsed %lu (default 300)", dt);
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_np_wait_zero_timeout_busy) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(WaitNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s = U(CreateNamedPipeA)(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512, 100, NULL);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    SET_ERR(0); CHECK(!U(WaitNamedPipeA)(nm, 1)); EXPECT_ERR(ERROR_SEM_TIMEOUT);
    CloseHandle(c); CloseHandle(s);
}
static DWORD WINAPI make_instance_later(void *p) {
    Sleep(150);
    HANDLE h = pmc_server((const char *)p, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 2, 512, 512);
    Sleep(1500);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    return 0;
}
TEST(pmc_np_wait_succeeds_when_instance_appears) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(WaitNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 2, 512, 512);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    HANDLE th = pmc_thread(make_instance_later, nm);
    SET_ERR(0);
    BOOL ok = U(WaitNamedPipeA)(nm, 5000);
    CHECK(ok);
    if (ok) {
        HANDLE c2 = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
        CHECK(c2 != INVALID_HANDLE_VALUE);
        if (c2 != INVALID_HANDLE_VALUE) CloseHandle(c2);
    }
    pmc_join(th);
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_np_wait_bad_names) {
    REQUIRE(U(WaitNamedPipeA));
    const char *bad[] = { "k32t_notapipe", "C:\\k32t_notapipe", "\\\\.\\mailslot\\k32t_x", "\\\\.\\nul" };
    for (int i = 0; i < 4; i++) {
        SET_ERR(0);
        CHECKF(!U(WaitNamedPipeA)(bad[i], 10), "name %s", bad[i]);
        t_note("WaitNamedPipeA(%s) err=%lu", bad[i], GetLastError());
    }
}
TEST(pmc_np_wait_unc_localhost_form) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(WaitNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    char unc[160]; snprintf(unc, sizeof unc, "\\\\.\\pipe\\%s", strrchr(nm, '\\') + 1);
    CHECK(U(WaitNamedPipeA)(unc, 10));
    CloseHandle(s);
}
TEST(pmc_np_write_through_create) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX | FILE_FLAG_WRITE_THROUGH, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    CHECK(s != INVALID_HANDLE_VALUE);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, FILE_FLAG_WRITE_THROUGH);
    CHECK(c != INVALID_HANDLE_VALUE);
    DWORD n; char b[8];
    CHECK(WriteFile(c, "wt", 2, &n, NULL)); CHECK(ReadFile(s, b, 8, &n, NULL)); EXPECT_EQ(n, 2);
    CloseHandle(c); CloseHandle(s);
}
TEST(pmc_np_reject_accept_remote_flags) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 2, 512, 512);
    CHECK(s != INVALID_HANDLE_VALUE);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    CHECK(c != INVALID_HANDLE_VALUE);   /* local clients always allowed */
    PIPE_NAME(n2);
    HANDLE s2 = pmc_server(n2, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT | PIPE_ACCEPT_REMOTE_CLIENTS, 1, 512, 512);
    CHECK(s2 != INVALID_HANDLE_VALUE);
    if (c != INVALID_HANDLE_VALUE) CloseHandle(c);
    CloseHandle(s); if (s2 != INVALID_HANDLE_VALUE) CloseHandle(s2);
}
TEST(pmc_np_inheritable_security_attributes) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE s = U(CreateNamedPipeA)(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512, 0, &sa);
    CHECK(s != INVALID_HANDLE_VALUE);
    DWORD f = 0; CHECK(GetHandleInformation(s, &f)); CHECK(f & HANDLE_FLAG_INHERIT);
    CloseHandle(s);
    s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    f = 0xff; CHECK(GetHandleInformation(s, &f)); CHECK(!(f & HANDLE_FLAG_INHERIT));
    CloseHandle(s);
}
TEST(pmc_np_null_dacl_everyone) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    SECURITY_DESCRIPTOR sd; SECURITY_ATTRIBUTES sa = { sizeof sa, &sd, FALSE };
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
    HANDLE s = U(CreateNamedPipeA)(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 2, 512, 512, 0, &sa);
    CHECK(s != INVALID_HANDLE_VALUE);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    CHECK(c != INVALID_HANDLE_VALUE);
    if (c != INVALID_HANDLE_VALUE) CloseHandle(c);
    CloseHandle(s);
}
TEST(pmc_np_empty_dacl_denies_client) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    char aclbuf[64]; PACL acl = (PACL)aclbuf; InitializeAcl(acl, sizeof aclbuf, ACL_REVISION);
    SECURITY_DESCRIPTOR sd; SECURITY_ATTRIBUTES sa = { sizeof sa, &sd, FALSE };
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, acl, FALSE);
    HANDLE s = U(CreateNamedPipeA)(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 2, 512, 512, 0, &sa);
    CHECK(s != INVALID_HANDLE_VALUE);
    SET_ERR(0);
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    t_note("client open against empty DACL: %s err=%lu", c != INVALID_HANDLE_VALUE ? "ok" : "fail", GetLastError());
    if (c != INVALID_HANDLE_VALUE) CloseHandle(c);
    CloseHandle(s);
}
TEST(pmc_np_duplicate_server_handle) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(DisconnectNamedPipe));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512), s2;
    HANDLE c = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    CHECK(DuplicateHandle(GetCurrentProcess(), s, GetCurrentProcess(), &s2, 0, FALSE, DUPLICATE_SAME_ACCESS));
    CloseHandle(s);
    DWORD n; char b[4];
    CHECK(WriteFile(c, "d", 1, &n, NULL)); CHECK(ReadFile(s2, b, 4, &n, NULL)); EXPECT_EQ(n, 1);
    SET_ERR(0);
    HANDLE c2 = pmc_client(nm, GENERIC_READ | GENERIC_WRITE, 0);
    CHECK(c2 == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_PIPE_BUSY);
    CloseHandle(c); CloseHandle(s2);
}
TEST(pmc_np_closed_handle_use) {
    REQUIRE(U(CreateNamedPipeA)); REQUIRE(U(ConnectNamedPipe)); REQUIRE(U(PeekNamedPipe));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    CloseHandle(s);
    DWORD av;
    SET_ERR(0); CHECK(!U(ConnectNamedPipe)(s, NULL)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(PeekNamedPipe)(s, NULL, 0, NULL, &av, NULL)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(DisconnectNamedPipe)(s)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(GetNamedPipeInfo)(s, &av, NULL, NULL, NULL)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    /* name is free again */
    HANDLE s2 = pmc_server(nm, PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    CHECK(s2 != INVALID_HANDLE_VALUE);
    if (s2 != INVALID_HANDLE_VALUE) CloseHandle(s2);
}
TEST(pmc_np_pipe_not_in_filesystem) {
    REQUIRE(U(CreateNamedPipeA));
    PIPE_NAME(nm);
    HANDLE s = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    DWORD a = GetFileAttributesA(nm);
    t_note("GetFileAttributes on pipe: 0x%lx err=%lu", a, GetLastError());
    WIN32_FIND_DATAA fd;
    char pat[160]; snprintf(pat, sizeof pat, "\\\\.\\pipe\\k32t_%lu_*", GetCurrentProcessId());
    HANDLE f = FindFirstFileA(pat, &fd);
    t_note("FindFirstFile on pipe dir: %s", f != INVALID_HANDLE_VALUE ? fd.cFileName : "none");
    if (f != INVALID_HANDLE_VALUE) FindClose(f);
    CloseHandle(s);
}
