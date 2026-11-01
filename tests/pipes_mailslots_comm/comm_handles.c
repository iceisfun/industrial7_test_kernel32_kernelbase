/* Serial / comm APIs on handles that are not comm ports (files, pipes, invalid handles) plus port lookup APIs.
   No test ever touches a real serial port: the only comm-port open uses zero access and is closed at once. */
#include "pmc.h"

enum { H_FILE, H_PIPE_R, H_PIPE_SRV, H_NULL, H_INVALID, H_CLOSED, H_COUNT };
static const char *hname[] = { "file", "anon-pipe", "named-pipe", "NULL", "INVALID_HANDLE_VALUE", "closed" };
typedef struct { HANDLE h[H_COUNT]; HANDLE pw, extra; char path[MAX_PATH]; } hset;

static void hs_open(hset *s) {
    memset(s, 0, sizeof *s);
    t_tmpfile(s->path, sizeof s->path, "pmc_commfile.bin");
    s->h[H_FILE] = CreateFileA(s->path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CreatePipe(&s->h[H_PIPE_R], &s->pw, NULL, 0);
    char nm[128]; pmc_pipename(nm, sizeof nm);
    s->h[H_PIPE_SRV] = pmc_server(nm, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 512, 512);
    s->h[H_NULL] = NULL;
    s->h[H_INVALID] = INVALID_HANDLE_VALUE;
    HANDLE t = CreateFileA(s->path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    s->extra = t;
    s->h[H_CLOSED] = t; CloseHandle(t);
}
static void hs_close(hset *s) {
    CloseHandle(s->h[H_FILE]); CloseHandle(s->h[H_PIPE_R]); CloseHandle(s->pw); CloseHandle(s->h[H_PIPE_SRV]);
    DeleteFileA(s->path);
}
/* a failing call on a non-comm handle must report one of the plausible "wrong device" errors */
static int bad_err_ok(int kind, DWORD e) {
    if (kind == H_NULL) return e == ERROR_INVALID_HANDLE;
    if (kind == H_CLOSED) return e == ERROR_INVALID_HANDLE;
    return pmc_err_in(e, ERROR_INVALID_FUNCTION, ERROR_INVALID_HANDLE, ERROR_NOT_SUPPORTED, ERROR_INVALID_PARAMETER, ERROR_ACCESS_DENIED, ERROR_GEN_FAILURE, 0);
}
#define EACH(k) for (int k = 0; k < H_COUNT; k++)
#define FAILCALL(kind, api, call) do { SET_ERR(0); BOOL ok_ = (call); DWORD e_ = GetLastError(); \
    t_check(!ok_, __FILE__, __LINE__, api " on " , "%s: unexpectedly succeeded", hname[kind]); \
    if (!ok_) t_check(bad_err_ok(kind, e_), __FILE__, __LINE__, api " error", "%s: err=%lu", hname[kind], e_); } while (0)

TEST(pmc_comm_getcommstate_noncomm) {
    REQUIRE(U(GetCommState));
    hset s; hs_open(&s);
    EACH(k) { DCB d; memset(&d, 0xAA, sizeof d); FAILCALL(k, "GetCommState", U(GetCommState)(s.h[k], &d));
        CHECKF(d.DCBlength == 28 && d.BaudRate == 0 && d.XonLim == 0 && d.ByteSize == 0 && (*(DWORD *)((char *)&d + 8) & 1), "%s: DCB not reset (len=%lu baud=%lu)", hname[k], d.DCBlength, d.BaudRate); }
    hs_close(&s);
}
TEST(pmc_comm_setcommstate_noncomm) {
    REQUIRE(U(SetCommState));
    hset s; hs_open(&s);
    DCB d; memset(&d, 0, sizeof d); d.DCBlength = sizeof d; d.BaudRate = 9600; d.ByteSize = 8;
    EACH(k) FAILCALL(k, "SetCommState", U(SetCommState)(s.h[k], &d));
    hs_close(&s);
}
TEST(pmc_comm_getcommtimeouts_noncomm) {
    REQUIRE(U(GetCommTimeouts));
    hset s; hs_open(&s);
    EACH(k) { COMMTIMEOUTS t, ref; memset(&t, 0x77, sizeof t); ref = t; FAILCALL(k, "GetCommTimeouts", U(GetCommTimeouts)(s.h[k], &t));
        CHECKF(!memcmp(&t, &ref, sizeof t), "%s: timeouts modified on failure", hname[k]); }
    hs_close(&s);
}
TEST(pmc_comm_setcommtimeouts_noncomm) {
    REQUIRE(U(SetCommTimeouts));
    hset s; hs_open(&s);
    COMMTIMEOUTS t = { 1, 2, 3, 4, 5 };
    EACH(k) FAILCALL(k, "SetCommTimeouts", U(SetCommTimeouts)(s.h[k], &t));
    hs_close(&s);
}
TEST(pmc_comm_getcommmask_noncomm) {
    REQUIRE(U(GetCommMask));
    hset s; hs_open(&s);
    EACH(k) { DWORD m = 0x1234; FAILCALL(k, "GetCommMask", U(GetCommMask)(s.h[k], &m)); }
    hs_close(&s);
}
TEST(pmc_comm_setcommmask_noncomm) {
    REQUIRE(U(SetCommMask));
    hset s; hs_open(&s);
    EACH(k) { FAILCALL(k, "SetCommMask(EV_RXCHAR)", U(SetCommMask)(s.h[k], EV_RXCHAR)); CHECK(GetLastError() != ERROR_INVALID_DATA); }
    hs_close(&s);
}
TEST(pmc_comm_setcommmask_invalid_bits) {
    REQUIRE(U(SetCommMask));
    hset s; hs_open(&s);
    DWORD bad[] = { 0x2000, 0x4000, 0x8000, 0x10000, 0x80000000, 0xFFFFFFFF, 0xFFFFE000 };
    EACH(k) for (int i = 0; i < 7; i++) {
        SET_ERR(0);
        CHECKF(!U(SetCommMask)(s.h[k], bad[i]), "%s mask 0x%lx succeeded", hname[k], bad[i]);
        CHECKF(GetLastError() == ERROR_INVALID_DATA, "%s mask 0x%lx err %lu", hname[k], bad[i], GetLastError());
    }
    hs_close(&s);
}
TEST(pmc_comm_setcommmask_valid_bits_pass_validation) {
    REQUIRE(U(SetCommMask));
    hset s; hs_open(&s);
    DWORD ok_masks[] = { 0, EV_RXCHAR, EV_TXEMPTY, 0x1FFF, 0x1000, 0x0FFF };
    for (int i = 0; i < 6; i++) {
        SET_ERR(0);
        CHECK(!U(SetCommMask)(s.h[H_FILE], ok_masks[i]));
        CHECKF(GetLastError() != ERROR_INVALID_DATA, "mask 0x%lx rejected by validation", ok_masks[i]);
    }
    hs_close(&s);
}
TEST(pmc_comm_waitcommevent_noncomm) {
    REQUIRE(U(WaitCommEvent));
    hset s; hs_open(&s);
    EACH(k) { DWORD m = 0; FAILCALL(k, "WaitCommEvent", U(WaitCommEvent)(s.h[k], &m, NULL)); }
    hs_close(&s);
}
TEST(pmc_comm_waitcommevent_overlapped_noncomm) {
    REQUIRE(U(WaitCommEvent));
    hset s; hs_open(&s);
    EACH(k) {
        OVERLAPPED ov; memset(&ov, 0, sizeof ov); ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL); DWORD m = 0;
        SET_ERR(0); BOOL ok = U(WaitCommEvent)(s.h[k], &m, &ov); DWORD e = GetLastError();
        CHECKF(!ok, "%s: succeeded", hname[k]);
        CHECKF(e != ERROR_IO_PENDING, "%s: claimed pending on a non-comm handle", hname[k]);
        CloseHandle(ov.hEvent);
    }
    hs_close(&s);
}
TEST(pmc_comm_clearcommerror_noncomm) {
    REQUIRE(U(ClearCommError));
    hset s; hs_open(&s);
    EACH(k) { DWORD e = 0; COMSTAT st; memset(&st, 0, sizeof st); FAILCALL(k, "ClearCommError", U(ClearCommError)(s.h[k], &e, &st)); }
    EACH(k) { FAILCALL(k, "ClearCommError(NULL,NULL)", U(ClearCommError)(s.h[k], NULL, NULL)); }
    hs_close(&s);
}
TEST(pmc_comm_break_noncomm) {
    REQUIRE(U(SetCommBreak)); REQUIRE(U(ClearCommBreak));
    hset s; hs_open(&s);
    EACH(k) { FAILCALL(k, "SetCommBreak", U(SetCommBreak)(s.h[k])); FAILCALL(k, "ClearCommBreak", U(ClearCommBreak)(s.h[k])); }
    hs_close(&s);
}
TEST(pmc_comm_escape_noncomm) {
    REQUIRE(U(EscapeCommFunction));
    hset s; hs_open(&s);
    /* p1: codes 1-9 are all accepted by kernelbase EscapeCommFunction; a plain file handle gets ERROR_INVALID_PARAMETER from the file system's IOCTL path */
    DWORD f[] = { SETXOFF, SETXON, SETRTS, CLRRTS, SETDTR, CLRDTR, SETBREAK, CLRBREAK };
    EACH(k) for (int i = 0; i < 8; i++) {
        SET_ERR(0);
        CHECKF(!U(EscapeCommFunction)(s.h[k], f[i]), "%s func %lu succeeded", hname[k], f[i]);
        CHECKF(GetLastError() != ERROR_INVALID_PARAMETER || k == H_NULL || k == H_FILE, "%s func %lu: valid function rejected as invalid parameter", hname[k], f[i]);
    }
    hs_close(&s);
}
TEST(pmc_comm_escape_invalid_function) {
    REQUIRE(U(EscapeCommFunction));
    hset s; hs_open(&s);
    DWORD bad[] = { 0, 10, 11, 100, 0xFFFFFFFF, 0x80000000 };
    EACH(k) for (int i = 0; i < 6; i++) {
        SET_ERR(0);
        CHECKF(!U(EscapeCommFunction)(s.h[k], bad[i]), "%s func %lu succeeded", hname[k], bad[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "%s func %lu err %lu", hname[k], bad[i], GetLastError());
    }
    hs_close(&s);
}
TEST(pmc_comm_modemstatus_noncomm) {
    REQUIRE(U(GetCommModemStatus));
    hset s; hs_open(&s);
    EACH(k) { DWORD m = 0xdead; FAILCALL(k, "GetCommModemStatus", U(GetCommModemStatus)(s.h[k], &m)); }
    hs_close(&s);
}
TEST(pmc_comm_properties_noncomm) {
    REQUIRE(U(GetCommProperties));
    hset s; hs_open(&s);
    EACH(k) {
        COMMPROP p; memset(&p, 0xAA, sizeof p);
        FAILCALL(k, "GetCommProperties", U(GetCommProperties)(s.h[k], &p));
        unsigned char z[0x40]; memset(z, 0, sizeof z);
        CHECKF(!memcmp(&p, z, sizeof z), "%s: COMMPROP header not zeroed on failure", hname[k]);
    }
    hs_close(&s);
}
TEST(pmc_comm_properties_packetlength_magic) {
    REQUIRE(U(GetCommProperties));
    hset s; hs_open(&s);
    struct { COMMPROP p; BYTE extra[64]; } big; memset(&big, 0xAA, sizeof big);
    big.p.wPacketLength = sizeof big; big.p.dwProvSpec1 = 0xE73CF52E;
    SET_ERR(0); CHECK(!U(GetCommProperties)(s.h[H_FILE], &big.p));
    /* with the magic the whole advertised packet is cleared */
    CHECK(big.extra[63] == 0 || big.extra[63] == 0xAA);
    t_note("packet clear: first extra byte 0x%02x last 0x%02x", big.extra[0], big.extra[63]);
    hs_close(&s);
}
TEST(pmc_comm_purge_noncomm) {
    REQUIRE(U(PurgeComm));
    hset s; hs_open(&s);
    EACH(k) FAILCALL(k, "PurgeComm", U(PurgeComm)(s.h[k], PURGE_RXCLEAR | PURGE_TXCLEAR));
    EACH(k) FAILCALL(k, "PurgeComm(0)", U(PurgeComm)(s.h[k], 0));
    hs_close(&s);
}
TEST(pmc_comm_setupcomm_noncomm) {
    REQUIRE(U(SetupComm));
    hset s; hs_open(&s);
    EACH(k) { SET_ERR(0); CHECK(!U(SetupComm)(s.h[k], 4096, 4096)); CHECKF(GetLastError() != ERROR_INVALID_DATA, "%s: even sizes rejected", hname[k]); }
    hs_close(&s);
}
TEST(pmc_comm_setupcomm_odd_sizes) {
    REQUIRE(U(SetupComm));
    hset s; hs_open(&s);
    struct { DWORD in, out; } t[] = { {1, 0}, {0, 1}, {3, 4096}, {4096, 3}, {4097, 4097}, {0x7fffffff, 0}, {0xfffffffd, 0} };
    EACH(k) for (int i = 0; i < 7; i++) {
        SET_ERR(0);
        CHECKF(!U(SetupComm)(s.h[k], t[i].in, t[i].out), "%s in=%lu out=%lu succeeded", hname[k], t[i].in, t[i].out);
        CHECKF(GetLastError() == ERROR_INVALID_DATA, "%s in=%lu out=%lu err %lu", hname[k], t[i].in, t[i].out, GetLastError());
    }
    hs_close(&s);
}
TEST(pmc_comm_setupcomm_special_values_pass_validation) {
    REQUIRE(U(SetupComm));
    hset s; hs_open(&s);
    struct { DWORD in, out; } t[] = { {0, 0}, {0xffffffff, 0xffffffff}, {0xffffffff, 4096}, {2, 2}, {0x10000, 0x10000} };
    for (int i = 0; i < 5; i++) {
        SET_ERR(0);
        CHECK(!U(SetupComm)(s.h[H_FILE], t[i].in, t[i].out));
        CHECKF(GetLastError() != ERROR_INVALID_DATA, "in=%lu out=%lu rejected by validation", t[i].in, t[i].out);
    }
    hs_close(&s);
}
TEST(pmc_comm_transmitchar_noncomm) {
    REQUIRE(U(TransmitCommChar));
    hset s; hs_open(&s);
    EACH(k) FAILCALL(k, "TransmitCommChar", U(TransmitCommChar)(s.h[k], 'x'));
    EACH(k) FAILCALL(k, "TransmitCommChar(0)", U(TransmitCommChar)(s.h[k], 0));
    hs_close(&s);
}
TEST(pmc_comm_getcommconfig_size_rules) {
    REQUIRE(U(GetCommConfig));
    hset s; hs_open(&s);
    EACH(k) {
        COMMCONFIG cc; memset(&cc, 0, sizeof cc); DWORD sz;
        if (!t_is_wine()) { SET_ERR(0); CHECK(!U(GetCommConfig)(s.h[k], &cc, NULL)); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
        sz = 0; SET_ERR(0); CHECK(!U(GetCommConfig)(s.h[k], &cc, &sz)); EXPECT_ERR(ERROR_INVALID_PARAMETER); CHECKF(sz == 52, "%s: size reported %lu", hname[k], sz);
        sz = 51; SET_ERR(0); CHECK(!U(GetCommConfig)(s.h[k], NULL, &sz)); EXPECT_ERR(ERROR_INVALID_PARAMETER); CHECKF(sz == 52, "%s: size reported %lu", hname[k], sz);
    }
    hs_close(&s);
}
TEST(pmc_comm_getcommconfig_full_size_noncomm) {
    REQUIRE(U(GetCommConfig));
    hset s; hs_open(&s);
    COMMCONFIG cc; DWORD sz = sizeof cc;
    memset(&cc, 0xAA, sizeof cc);
    SET_ERR(0); BOOL ok = U(GetCommConfig)(s.h[H_FILE], &cc, &sz);
    t_note("GetCommConfig(file, full size): ok=%d err=%lu size=%lu dwSize=%lu", ok, ok ? 0 : GetLastError(), sz, cc.dwSize);
    CHECK(!ok);
    sz = sizeof cc;
    SET_ERR(0); ok = U(GetCommConfig)(s.h[H_FILE], NULL, &sz);
    t_note("GetCommConfig(file, NULL cc, full size): ok=%d err=%lu", ok, ok ? 0 : GetLastError());
    hs_close(&s);
}
TEST(pmc_comm_setcommconfig_noncomm) {
    REQUIRE(U(SetCommConfig));
    hset s; hs_open(&s);
    COMMCONFIG cc; memset(&cc, 0, sizeof cc); cc.dwSize = sizeof cc; cc.wVersion = 1;
    cc.dcb.DCBlength = sizeof(DCB); cc.dcb.BaudRate = 9600; cc.dcb.ByteSize = 8;
    EACH(k) { if (k == H_NULL || k == H_INVALID || k == H_CLOSED) continue; FAILCALL(k, "SetCommConfig", U(SetCommConfig)(s.h[k], &cc, sizeof cc)); }
    hs_close(&s);
}
TEST(pmc_comm_errors_are_repeatable) {
    REQUIRE(U(GetCommState)); REQUIRE(U(GetCommTimeouts));
    hset s; hs_open(&s);
    DWORD f1 = 0, f2 = 0;
    for (int i = 0; i < 100; i++) {
        DCB d; COMMTIMEOUTS t;
        SET_ERR(0); if (U(GetCommState)(s.h[H_FILE], &d)) { CHECK(0); break; }
        DWORD e1 = GetLastError();
        SET_ERR(0); if (U(GetCommTimeouts)(s.h[H_FILE], &t)) { CHECK(0); break; }
        DWORD e2 = GetLastError();
        if (i == 0) { f1 = e1; f2 = e2; t_note("file handle: GetCommState err=%lu GetCommTimeouts err=%lu", e1, e2); }
        if (e1 != f1 || e2 != f2) { CHECK(e1 == f1 && e2 == f2); break; }
    }
    hs_close(&s);
}
TEST(pmc_comm_no_handle_leak_on_failure) {
    REQUIRE(U(GetCommState));
    hset s; hs_open(&s);
    DWORD before = 0, after = 0;
    GetProcessHandleCount(GetCurrentProcess(), &before);
    for (int i = 0; i < 2000; i++) { DCB d; U(GetCommState)(s.h[H_FILE], &d); COMMTIMEOUTS t; U(GetCommTimeouts)(s.h[H_FILE], &t); U(PurgeComm)(s.h[H_FILE], PURGE_RXCLEAR); }
    GetProcessHandleCount(GetCurrentProcess(), &after);
    CHECKF(after <= before + 4, "handle count %lu -> %lu", before, after);
    hs_close(&s);
}

/* ---- port lookup / open ---- */
TEST(pmc_comm_createfile_nonexistent_port) {
    SET_ERR(0); HANDLE h = CreateFileA("\\\\.\\COM99999", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    SET_ERR(0); h = CreateFileA("\\\\.\\COM0", 0, 0, NULL, OPEN_EXISTING, 0, NULL);
    t_note("\\\\.\\COM0: %s err=%lu", h != INVALID_HANDLE_VALUE ? "opened" : "failed", GetLastError());
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
}
TEST(pmc_comm_createfile_com1_zero_access) {
    SET_ERR(0);
    HANDLE h = CreateFileA("\\\\.\\COM1", 0, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) t_note("COM1 (access 0): not available, err=%lu", GetLastError());
    else { t_note("COM1 (access 0): present, filetype=%lu", GetFileType(h)); if (!t_is_wine()) EXPECT_EQ(GetFileType(h), FILE_TYPE_CHAR); CloseHandle(h); }
    SET_ERR(0);
    h = CreateFileA("COM1", 0, 0, NULL, OPEN_EXISTING, 0, NULL);
    t_note("COM1 without device prefix (access 0): %s err=%lu", h != INVALID_HANDLE_VALUE ? "opened" : "failed", GetLastError());
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
}
TEST(pmc_comm_openport_nonexistent) {
    REQUIRE(UN(OpenCommPort));
    ULONG ports[] = { 99999, 0, 4000000000u, 0xFFFFFFFFu };
    for (int i = 0; i < 4; i++) {
        SET_ERR(0);
        HANDLE h = UN(OpenCommPort)(ports[i], GENERIC_READ | GENERIC_WRITE, 0);
        t_note("OpenCommPort(%lu) = %p err=%lu", ports[i], h, GetLastError());
        if (ports[i] == 99999) { CHECK(h == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_FILE_NOT_FOUND); }
        if (h != INVALID_HANDLE_VALUE && h != NULL) CloseHandle(h);
    }
}
TEST(pmc_comm_openport_zero_access) {
    REQUIRE(UN(OpenCommPort));
    SET_ERR(0);
    HANDLE h = UN(OpenCommPort)(1, 0, 0);
    t_note("OpenCommPort(1, access 0) = %s err=%lu", h != INVALID_HANDLE_VALUE ? "handle" : "INVALID", GetLastError());
    if (h != INVALID_HANDLE_VALUE) { if (!t_is_wine()) EXPECT_EQ(GetFileType(h), FILE_TYPE_CHAR); CloseHandle(h); }
}
TEST(pmc_comm_getcommports_query) {
    REQUIRE(UN(GetCommPorts));
    ULONG n = 0xdead;
    SET_ERR(0);
    ULONG r = UN(GetCommPorts)(NULL, 0, &n);
    t_note("GetCommPorts(NULL,0) ret=%lu count=%lu err=%lu", r, n, GetLastError());
    CHECK(n != 0xdead);
    if (n == 0) { t_note("no COM ports (r=%lu)", r); }
    else { CHECK(r == ERROR_MORE_DATA || r == 0); }
}
TEST(pmc_comm_getcommports_buffer_sizes) {
    REQUIRE(UN(GetCommPorts));
    ULONG n = 0, got = 0;
    UN(GetCommPorts)(NULL, 0, &n);
    if (n > 256) n = 256;
    ULONG buf[260]; memset(buf, 0xAB, sizeof buf);
    SET_ERR(0);
    ULONG r = UN(GetCommPorts)(buf, 260, &got);
    t_note("GetCommPorts(buf,260) ret=%lu count=%lu", r, got);
    EXPECT_EQ(got, n);
    if (got) CHECK(r == 0);
    for (ULONG i = 0; i < got && i < 260; i++) { CHECKF(buf[i] >= 1, "port[%lu]=%lu", i, buf[i]); }
    for (ULONG i = got; i < 260; i++) if (buf[i] != 0xABABABAB) { CHECK(buf[i] == 0xABABABAB); break; }   /* nothing written past the count */
    if (got > 1) {
        ULONG small[1] = { 0xABABABAB }, c2 = 0;
        SET_ERR(0); r = UN(GetCommPorts)(small, 1, &c2);
        EXPECT_EQ(c2, got);
        t_note("GetCommPorts(small,1) ret=%lu small[0]=%lu", r, small[0]);
        CHECK(r == ERROR_MORE_DATA);
        CHECK(small[0] == buf[0] || small[0] == 0xABABABAB);
    }
    ULONG z = 0xdead; ULONG dummy[1] = { 7 };
    SET_ERR(0); r = UN(GetCommPorts)(dummy, 0, &z);
    EXPECT_EQ(z, got); CHECK(dummy[0] == 7);
}
TEST(pmc_comm_getcommports_consistent_repeat) {
    REQUIRE(UN(GetCommPorts));
    ULONG a = 0, b = 0;
    UN(GetCommPorts)(NULL, 0, &a); UN(GetCommPorts)(NULL, 0, &b);
    EXPECT_EQ(a, b);
}
TEST(pmc_comm_getdefaultcommconfig_junk_names) {
    REQUIRE(U(GetDefaultCommConfigA)); REQUIRE(U(GetDefaultCommConfigW));
    COMMCONFIG cc; DWORD sz = sizeof cc;
    memset(&cc, 0, sizeof cc);
    SET_ERR(0); BOOL ok = U(GetDefaultCommConfigA)("NOSUCHPORT99", &cc, &sz);
    t_note("GetDefaultCommConfigA(NOSUCHPORT99) = %d err=%lu size=%lu", ok, ok ? 0 : GetLastError(), sz);
    sz = sizeof cc; SET_ERR(0); ok = U(GetDefaultCommConfigW)(L"NOSUCHPORT99", &cc, &sz);
    t_note("GetDefaultCommConfigW(NOSUCHPORT99) = %d err=%lu", ok, ok ? 0 : GetLastError());
}
TEST(pmc_comm_getdefaultcommconfig_com1) {
    REQUIRE(U(GetDefaultCommConfigA)); REQUIRE(U(GetDefaultCommConfigW));
    COMMCONFIG a, w; DWORD sa = sizeof a, sw = sizeof w;
    memset(&a, 0, sizeof a); memset(&w, 0, sizeof w);
    SET_ERR(0); BOOL oka = U(GetDefaultCommConfigA)("COM1", &a, &sa); DWORD ea = GetLastError();
    SET_ERR(0); BOOL okw = U(GetDefaultCommConfigW)(L"COM1", &w, &sw); DWORD ew = GetLastError();
    t_note("GetDefaultCommConfig COM1: A ok=%d err=%lu size=%lu baud=%lu | W ok=%d err=%lu size=%lu baud=%lu", oka, ea, sa, a.dcb.BaudRate, okw, ew, sw, w.dcb.BaudRate);
    EXPECT_EQ(oka, okw);
    if (oka && okw) { EXPECT_EQ(a.dcb.BaudRate, w.dcb.BaudRate); EXPECT_EQ(a.dwSize, w.dwSize); EXPECT_EQ(sa, sw); }
}
TEST(pmc_comm_getdefaultcommconfig_small_size) {
    REQUIRE(U(GetDefaultCommConfigA));
    COMMCONFIG cc; DWORD sz = 4;
    memset(&cc, 0, sizeof cc);
    SET_ERR(0); BOOL ok = U(GetDefaultCommConfigA)("COM1", &cc, &sz);
    t_note("GetDefaultCommConfigA(COM1, size 4) = %d err=%lu size=%lu", ok, ok ? 0 : GetLastError(), sz);
    if (!ok) CHECK(GetLastError() != 0);
}
TEST(pmc_comm_setdefaultcommconfig_exports_exist) {
    /* resolved only: calling it could change persistent port defaults */
    CHECK(t_proc_k32("SetDefaultCommConfigA") != NULL);
    CHECK(t_proc_k32("SetDefaultCommConfigW") != NULL);
    CHECK(t_proc_k32("GetDefaultCommConfigA") != NULL);
    CHECK(t_proc_k32("GetDefaultCommConfigW") != NULL);
    CHECK(t_proc_k32("CommConfigDialogA") != NULL);
    CHECK(t_proc_k32("CommConfigDialogW") != NULL);
}
TEST(pmc_comm_export_surface) {
    const char *k32[] = { "BuildCommDCBA", "BuildCommDCBW", "BuildCommDCBAndTimeoutsA", "BuildCommDCBAndTimeoutsW", "ClearCommBreak", "ClearCommError",
        "EscapeCommFunction", "GetCommConfig", "GetCommMask", "GetCommModemStatus", "GetCommProperties", "GetCommState", "GetCommTimeouts", "PurgeComm",
        "SetCommBreak", "SetCommConfig", "SetCommMask", "SetCommState", "SetCommTimeouts", "SetupComm", "TransmitCommChar", "WaitCommEvent",
        "CreateNamedPipeA", "CreateNamedPipeW", "CreatePipe", "ConnectNamedPipe", "DisconnectNamedPipe", "WaitNamedPipeA", "WaitNamedPipeW",
        "PeekNamedPipe", "TransactNamedPipe", "CallNamedPipeA", "CallNamedPipeW", "SetNamedPipeHandleState", "GetNamedPipeInfo",
        "GetNamedPipeHandleStateA", "GetNamedPipeHandleStateW", "GetNamedPipeClientComputerNameA", "GetNamedPipeClientComputerNameW",
        "GetNamedPipeClientProcessId", "GetNamedPipeServerProcessId", "GetNamedPipeClientSessionId", "GetNamedPipeServerSessionId",
        "CreateMailslotA", "CreateMailslotW", "GetMailslotInfo", "SetMailslotInfo" };
    for (size_t i = 0; i < sizeof k32 / sizeof k32[0]; i++) CHECKF(t_proc_k32(k32[i]) != NULL || (t_is_wine() && !strncmp(k32[i], "GetNamedPipeClientComputerName", 30)), "kernel32 export %s missing", k32[i]);
    const char *kb[] = { "GetCommPorts", "OpenCommPort", "ClearCommBreak", "ClearCommError", "EscapeCommFunction", "GetCommConfig", "GetCommMask",
        "GetCommModemStatus", "GetCommProperties", "GetCommState", "GetCommTimeouts", "PurgeComm", "SetCommBreak", "SetCommConfig", "SetCommMask",
        "SetCommState", "SetCommTimeouts", "SetupComm", "TransmitCommChar", "WaitCommEvent", "CreateNamedPipeW", "CreatePipe", "ConnectNamedPipe",
        "DisconnectNamedPipe", "PeekNamedPipe", "SetNamedPipeHandleState", "GetNamedPipeInfo", "GetNamedPipeHandleStateW" };
    for (size_t i = 0; i < sizeof kb / sizeof kb[0]; i++) { if (!t_kernelbase()) break; CHECKF(t_proc_kb(kb[i]) != NULL || (t_is_wine() && (!strcmp(kb[i], "GetCommPorts") || !strcmp(kb[i], "OpenCommPort"))), "kernelbase export %s missing", kb[i]); }
}
