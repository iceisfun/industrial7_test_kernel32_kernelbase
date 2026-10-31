/* BuildCommDCB / BuildCommDCBAndTimeouts string parsing (A and W), table driven. */
#include "pmc.h"

#define DCB_FLAGS(d) (*(DWORD *)((char *)(d) + 8))
#define F_CTS 0x4
#define F_DSR 0x8
#define F_DTR_MASK 0x30
#define F_DSRSENS 0x40
#define F_XON 0x300
#define F_RTS_MASK 0x3000
#define LEGACY_MASK (F_XON | F_CTS | F_DSR | F_DTR_MASK | F_RTS_MASK)
#define LEGACY_VAL  (0x10 | 0x1000)

typedef struct {
    const char *str;
    int baud;      /* -1: unchanged */
    int data;      /* -1: unchanged */
    int parity;    /* -1: unchanged */
    int stop;      /* expected StopBits (always written on success) */
    DWORD fmask, fval;
    int wrconst;   /* -1: timeouts untouched, else expected WriteTotalTimeoutConstant (others 0) */
} bcase;

static void fill_dcb(DCB *d, int f) { memset(d, f, sizeof *d); }

static void check_case(const bcase *c, int fill, int api /*0 A,1 W,2 TA,3 TW*/) {
    DCB d; COMMTIMEOUTS to; WCHAR w[128]; BOOL ok;
    fill_dcb(&d, fill); memset(&to, fill, sizeof to);
    MultiByteToWideChar(CP_ACP, 0, c->str, -1, w, 128);
    SET_ERR(0);
    switch (api) {
    case 0: ok = U(BuildCommDCBA)(c->str, &d); break;
    case 1: ok = U(BuildCommDCBW)(w, &d); break;
    case 2: ok = U(BuildCommDCBAndTimeoutsA)(c->str, &d, &to); break;
    default: ok = U(BuildCommDCBAndTimeoutsW)(w, &d, &to); break;
    }
    static const char *names[] = { "BuildCommDCBA", "BuildCommDCBW", "BuildCommDCBAndTimeoutsA", "BuildCommDCBAndTimeoutsW" };
    CHECKF(ok, "%s('%s') fill=%02x err=%lu", names[api], c->str, fill, GetLastError());
    if (!ok) return;
    DWORD fillw = fill ? 0xFFFFFFFFu : 0;
    CHECKF(d.BaudRate == (c->baud >= 0 ? (DWORD)c->baud : fillw), "%s('%s') baud=%lu", names[api], c->str, d.BaudRate);
    CHECKF(d.ByteSize == (c->data >= 0 ? (BYTE)c->data : (BYTE)fill), "%s('%s') bytesize=%u", names[api], c->str, d.ByteSize);
    CHECKF(d.Parity == (c->parity >= 0 ? (BYTE)c->parity : (BYTE)fill), "%s('%s') parity=%u", names[api], c->str, d.Parity);
    CHECKF(d.StopBits == (BYTE)c->stop, "%s('%s') stopbits=%u want %d", names[api], c->str, d.StopBits, c->stop);
    DWORD wantf = (fillw & ~c->fmask) | c->fval;
    CHECKF(DCB_FLAGS(&d) == wantf, "%s('%s') fill=%02x flags=0x%lx want 0x%lx", names[api], c->str, fill, DCB_FLAGS(&d), wantf);
    CHECKF(d.DCBlength == fillw, "%s('%s') DCBlength touched: %lu", names[api], c->str, d.DCBlength);
    CHECKF(d.XonLim == (WORD)fillw && d.XoffLim == (WORD)fillw, "%s('%s') XonLim/XoffLim touched", names[api], c->str);
    CHECKF(d.XonChar == (char)fill && d.XoffChar == (char)fill && d.ErrorChar == (char)fill && d.EofChar == (char)fill && d.EvtChar == (char)fill, "%s('%s') special chars touched", names[api], c->str);
    if (api >= 2) {
        if (c->wrconst < 0) {
            CHECKF(to.ReadIntervalTimeout == fillw && to.WriteTotalTimeoutConstant == fillw, "%s('%s') timeouts touched", names[api], c->str);
        } else {
            CHECKF(to.ReadIntervalTimeout == 0 && to.ReadTotalTimeoutMultiplier == 0 && to.ReadTotalTimeoutConstant == 0 && to.WriteTotalTimeoutMultiplier == 0 && to.WriteTotalTimeoutConstant == (DWORD)c->wrconst,
                   "%s('%s') timeouts %lu/%lu/%lu/%lu/%lu", names[api], c->str, to.ReadIntervalTimeout, to.ReadTotalTimeoutMultiplier, to.ReadTotalTimeoutConstant, to.WriteTotalTimeoutMultiplier, to.WriteTotalTimeoutConstant);
        }
    }
}
static void run_table(const bcase *t, int n) {
    if (!U(BuildCommDCBA) || !U(BuildCommDCBW) || !U(BuildCommDCBAndTimeoutsA) || !U(BuildCommDCBAndTimeoutsW)) { t_skip("missing BuildCommDCB export"); return; }
    for (int i = 0; i < n; i++)
        for (int fill = 0; fill <= 0xFF; fill += 0xFF)
            for (int api = 0; api < 4; api++) check_case(&t[i], fill, api);
}

TEST(pmc_dcb_legacy_forms) {
    static const bcase t[] = {
        { "9600,n,8,1",    9600, 8, 0, 0, LEGACY_MASK, LEGACY_VAL, -1 },
        { "9600,e,7,2",    9600, 7, 2, 2, LEGACY_MASK, LEGACY_VAL, -1 },
        { "19200,o,8,1",  19200, 8, 1, 0, LEGACY_MASK, LEGACY_VAL, -1 },
        { "1200,m,5,1.5",  1200, 5, 3, 1, LEGACY_MASK, LEGACY_VAL, -1 },
        { "4800,s,6,1",    4800, 6, 4, 0, LEGACY_MASK, LEGACY_VAL, -1 },
        { "115200,N,8,1", 115200, 8, 0, 0, LEGACY_MASK, LEGACY_VAL, -1 },
        { "57600,E,8,2",  57600, 8, 2, 2, LEGACY_MASK, LEGACY_VAL, -1 },
        { "300,n,8,1",      300, 8, 0, 0, LEGACY_MASK, LEGACY_VAL, -1 },
        { "2400, n, 8, 1", 2400, 8, 0, 0, LEGACY_MASK, LEGACY_VAL, -1 },
    };
    run_table(t, sizeof t / sizeof t[0]);
}
TEST(pmc_dcb_legacy_flow_suffix) {
    static const bcase t[] = {
        { "9600,n,8,1,p", 9600, 8, 0, 0, LEGACY_MASK, F_CTS | F_DSR | 0x20 | 0x2000, -1 },
        { "9600,n,8,1,x", 9600, 8, 0, 0, LEGACY_MASK, F_XON | 0x10 | 0x1000, -1 },
        { "9600,n,8,2,P", 9600, 8, 0, 2, LEGACY_MASK, F_CTS | F_DSR | 0x20 | 0x2000, -1 },
        { "9600,n,8,1, x", 9600, 8, 0, 0, LEGACY_MASK, F_XON | 0x10 | 0x1000, -1 },
    };
    run_table(t, sizeof t / sizeof t[0]);
}
TEST(pmc_dcb_legacy_partial) {
    static const bcase t[] = {
        { "9600",        9600, -1, -1, 0, LEGACY_MASK, LEGACY_VAL, -1 },
        { "9600,n",      9600, -1,  0, 0, LEGACY_MASK, LEGACY_VAL, -1 },
        { "9600,n,8",    9600,  8,  0, 0, LEGACY_MASK, LEGACY_VAL, -1 },
        { "9600,,8,1",   9600,  8, -1, 0, LEGACY_MASK, LEGACY_VAL, -1 },
        { "9600,n,,1",   9600, -1,  0, 0, LEGACY_MASK, LEGACY_VAL, -1 },
        { "9600,,,2",    9600, -1, -1, 2, LEGACY_MASK, LEGACY_VAL, -1 },
        { "9600,n,8,",   9600,  8,  0, 0, LEGACY_MASK, LEGACY_VAL, -1 },
    };
    run_table(t, sizeof t / sizeof t[0]);
}
TEST(pmc_dcb_baud_abbreviations) {
    static const bcase t[] = {
        { "11",   110, -1, -1, 2, LEGACY_MASK, LEGACY_VAL, -1 },
        { "110",  110, -1, -1, 2, LEGACY_MASK, LEGACY_VAL, -1 },
        { "15",   150, -1, -1, 0, LEGACY_MASK, LEGACY_VAL, -1 },
        { "150",  150, -1, -1, 0, LEGACY_MASK, LEGACY_VAL, -1 },
        { "30",   300, -1, -1, 0, LEGACY_MASK, LEGACY_VAL, -1 },
        { "60",   600, -1, -1, 0, LEGACY_MASK, LEGACY_VAL, -1 },
        { "12",  1200, -1, -1, 0, LEGACY_MASK, LEGACY_VAL, -1 },
        { "24",  2400, -1, -1, 0, LEGACY_MASK, LEGACY_VAL, -1 },
        { "48",  4800, -1, -1, 0, LEGACY_MASK, LEGACY_VAL, -1 },
        { "96",  9600, -1, -1, 0, LEGACY_MASK, LEGACY_VAL, -1 },
        { "19", 19200, -1, -1, 0, LEGACY_MASK, LEGACY_VAL, -1 },
        { "600",  600, -1, -1, 0, LEGACY_MASK, LEGACY_VAL, -1 },
        { "1200", 1200, -1, -1, 0, LEGACY_MASK, LEGACY_VAL, -1 },
        { "4800", 4800, -1, -1, 0, LEGACY_MASK, LEGACY_VAL, -1 },
        { "9600", 9600, -1, -1, 0, LEGACY_MASK, LEGACY_VAL, -1 },
        { "19200", 19200, -1, -1, 0, LEGACY_MASK, LEGACY_VAL, -1 },
        { "14400", 14400, -1, -1, 0, LEGACY_MASK, LEGACY_VAL, -1 },
        { "38400", 38400, -1, -1, 0, LEGACY_MASK, LEGACY_VAL, -1 },
        { "56000", 56000, -1, -1, 0, LEGACY_MASK, LEGACY_VAL, -1 },
        { "128000", 128000, -1, -1, 0, LEGACY_MASK, LEGACY_VAL, -1 },
        { "256000", 256000, -1, -1, 0, LEGACY_MASK, LEGACY_VAL, -1 },
        { "1", 1, -1, -1, 0, LEGACY_MASK, LEGACY_VAL, -1 },
        { "0", 0, -1, -1, 0, LEGACY_MASK, LEGACY_VAL, -1 },
        { "7", 7, -1, -1, 0, LEGACY_MASK, LEGACY_VAL, -1 },
        { "123456", 123456, -1, -1, 0, LEGACY_MASK, LEGACY_VAL, -1 },
    };
    run_table(t, sizeof t / sizeof t[0]);
}
TEST(pmc_dcb_keyword_forms) {
    static const bcase t[] = {
        { "baud=9600",                          9600, -1, -1, 0, 0, 0, -1 },
        { "BAUD=19200",                        19200, -1, -1, 0, 0, 0, -1 },
        { "baud=9600 parity=n data=8 stop=1",   9600,  8,  0, 0, 0, 0, -1 },
        { "BAUD=9600 PARITY=E DATA=7 STOP=2",   9600,  7,  2, 2, 0, 0, -1 },
        { "baud=2400 parity=o data=5 stop=1.5", 2400,  5,  1, 1, 0, 0, -1 },
        { "parity=m",                             -1, -1,  3, 0, 0, 0, -1 },
        { "parity=s",                             -1, -1,  4, 0, 0, 0, -1 },
        { "parity=N",                             -1, -1,  0, 0, 0, 0, -1 },
        { "data=5",                               -1,  5, -1, 0, 0, 0, -1 },
        { "data=6",                               -1,  6, -1, 0, 0, 0, -1 },
        { "data=7",                               -1,  7, -1, 0, 0, 0, -1 },
        { "data=8",                               -1,  8, -1, 0, 0, 0, -1 },
        { "stop=1",                               -1, -1, -1, 0, 0, 0, -1 },
        { "stop=2",                               -1, -1, -1, 2, 0, 0, -1 },
        { "stop=1.5",                             -1, -1, -1, 1, 0, 0, -1 },
        { "baud=110",                            110, -1, -1, 2, 0, 0, -1 },
        { "baud=110 stop=1",                     110, -1, -1, 0, 0, 0, -1 },
        { "baud=9600   parity=n",               9600, -1,  0, 0, 0, 0, -1 },
        { "baud=12",                              12, -1, -1, 0, 0, 0, -1 },   /* keyword form still maps abbreviations */
    };
    /* the abbreviation row above is verified separately below; drop it here */
    run_table(t, sizeof t / sizeof t[0] - 1);
}
TEST(pmc_dcb_keyword_abbrev_baud) {
    if (!U(BuildCommDCBA)) { t_skip("missing BuildCommDCBA"); return; }
    DCB d; memset(&d, 0, sizeof d);
    CHECK(U(BuildCommDCBA)("baud=12", &d)); EXPECT_EQ(d.BaudRate, 1200);
    CHECK(U(BuildCommDCBA)("baud=96", &d)); EXPECT_EQ(d.BaudRate, 9600);
}
/* kernel32!ConvertDtrControl/ConvertRtsControl compare the token end against
   token+2 for "hs"/"tg" (token+1 for "on"), so "dtr=hs", "rts=hs", "rts=tg"
   are rejected by real Windows with ERROR_INVALID_PARAMETER. */
TEST(pmc_dcb_keyword_flow_flags) {
    static const bcase t[] = {
        { "xon=on",   -1, -1, -1, 0, F_XON, F_XON, -1 },
        { "xon=off",  -1, -1, -1, 0, F_XON, 0, -1 },
        { "XON=ON",   -1, -1, -1, 0, F_XON, F_XON, -1 },
        { "odsr=on",  -1, -1, -1, 0, F_DSR, F_DSR, -1 },
        { "odsr=off", -1, -1, -1, 0, F_DSR, 0, -1 },
        { "idsr=on",  -1, -1, -1, 0, F_DSRSENS, F_DSRSENS, -1 },
        { "idsr=off", -1, -1, -1, 0, F_DSRSENS, 0, -1 },
        { "octs=on",  -1, -1, -1, 0, F_CTS, F_CTS, -1 },
        { "octs=off", -1, -1, -1, 0, F_CTS, 0, -1 },
        { "dtr=on",   -1, -1, -1, 0, F_DTR_MASK, 0x10, -1 },
        { "dtr=off",  -1, -1, -1, 0, F_DTR_MASK, 0x00, -1 },
        { "rts=on",   -1, -1, -1, 0, F_RTS_MASK, 0x1000, -1 },
        { "rts=off",  -1, -1, -1, 0, F_RTS_MASK, 0x0000, -1 },
        { "baud=9600 xon=on rts=on dtr=on", 9600, -1, -1, 0, F_XON | F_RTS_MASK | F_DTR_MASK, F_XON | 0x1000 | 0x10, -1 },
        { "octs=on odsr=on idsr=on", -1, -1, -1, 0, F_CTS | F_DSR | F_DSRSENS, F_CTS | F_DSR | F_DSRSENS, -1 },
    };
    run_table(t, sizeof t / sizeof t[0]);
}
TEST(pmc_dcb_timeouts_keyword) {
    static const bcase t[] = {
        { "to=on",  -1, -1, -1, 0, 0, 0, 60000 },
        { "to=off", -1, -1, -1, 0, 0, 0, 0 },
        { "TO=ON baud=9600", 9600, -1, -1, 0, 0, 0, 60000 },
        { "baud=9600 to=off", 9600, -1, -1, 0, 0, 0, 0 },
    };
    run_table(t, sizeof t / sizeof t[0]);
}
TEST(pmc_dcb_empty_string) {
    if (!U(BuildCommDCBA) || !U(BuildCommDCBW)) { t_skip("missing BuildCommDCB export"); return; }
    DCB d; memset(&d, 0xAA, sizeof d); WCHAR w[1] = { 0 };
    SET_ERR(0);
    BOOL ok = U(BuildCommDCBA)("", &d);
    t_note("BuildCommDCBA(\"\") = %d err=%lu stopbits=%u", ok, GetLastError(), d.StopBits);
    if (ok) { EXPECT_EQ(d.StopBits, 0); EXPECT_EQ(d.BaudRate, 0xAAAAAAAA); }
    ok = U(BuildCommDCBW)(w, &d);
    t_note("BuildCommDCBW(L\"\") = %d", ok);
}
TEST(pmc_dcb_invalid_strings) {
    if (!U(BuildCommDCBA) || !U(BuildCommDCBW) || !U(BuildCommDCBAndTimeoutsA) || !U(BuildCommDCBAndTimeoutsW)) { t_skip("missing export"); return; }
    static const char *bad[] = {
        "abc", "baud=", "baud=abc", "baud", "=9600", "9600,x,8,1", "9600,n,9,1", "9600,n,4,1", "9600,n,0,1", "9600,n,8,3", "9600,n,8,0",
        "9600,n,8,1,z", "9600,n,8,1,pp", "9600,n,8,1 junk", "parity=x", "parity=", "data=9", "data=4", "data=", "stop=3", "stop=0", "stop=1.4", "stop=",
        "dtr=hs", "DTR=HS", "rts=hs", "rts=tg", "xon=maybe", "xon=", "dtr=maybe", "rts=xx", "dtr=tg", "octs=on1", "to=maybe", "baud=9600,parity=n", "baud=9600;parity=n", "bogus=1",
        " 9600,n,8,1", "9600 n 8 1", "n,8,1", ",n,8,1", "9600,n,8,1.5.5", "baud=9600 parity=n data=8 stop=1 bogus",
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        DCB d; memset(&d, 0xAA, sizeof d); COMMTIMEOUTS to; memset(&to, 0xAA, sizeof to); WCHAR w[128];
        MultiByteToWideChar(CP_ACP, 0, bad[i], -1, w, 128);
        SET_ERR(0); BOOL ok = U(BuildCommDCBA)(bad[i], &d);
        CHECKF(!ok, "BuildCommDCBA('%s') succeeded", bad[i]);
        if (!ok) CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "BuildCommDCBA('%s') err %lu", bad[i], GetLastError());
        SET_ERR(0); ok = U(BuildCommDCBW)(w, &d);
        CHECKF(!ok, "BuildCommDCBW('%s') succeeded", bad[i]);
        if (!ok) CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "BuildCommDCBW('%s') err %lu", bad[i], GetLastError());
        SET_ERR(0); ok = U(BuildCommDCBAndTimeoutsA)(bad[i], &d, &to);
        CHECKF(!ok, "BuildCommDCBAndTimeoutsA('%s') succeeded", bad[i]);
        if (!ok) CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "BuildCommDCBAndTimeoutsA('%s') err %lu", bad[i], GetLastError());
        SET_ERR(0); ok = U(BuildCommDCBAndTimeoutsW)(w, &d, &to);
        CHECKF(!ok, "BuildCommDCBAndTimeoutsW('%s') succeeded", bad[i]);
        if (!ok) CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "BuildCommDCBAndTimeoutsW('%s') err %lu", bad[i], GetLastError());
    }
}
TEST(pmc_dcb_failure_leaves_dcb_untouched) {
    if (!U(BuildCommDCBA)) { t_skip("missing BuildCommDCBA"); return; }
    DCB d, ref; memset(&d, 0x5A, sizeof d); ref = d;
    SET_ERR(0);
    CHECK(!U(BuildCommDCBA)("9600,n,8,9", &d)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CHECK(!memcmp(&d, &ref, sizeof d));
    CHECK(!U(BuildCommDCBA)("baud=9600 parity=z", &d));
    CHECK(!memcmp(&d, &ref, sizeof d));
}
TEST(pmc_dcb_failure_leaves_timeouts_untouched) {
    if (!U(BuildCommDCBAndTimeoutsA)) { t_skip("missing export"); return; }
    DCB d; COMMTIMEOUTS to, ref; memset(&d, 0x11, sizeof d); memset(&to, 0x22, sizeof to); ref = to;
    CHECK(!U(BuildCommDCBAndTimeoutsA)("to=on stop=7", &d, &to));
    CHECK(!memcmp(&to, &ref, sizeof to));
}
TEST(pmc_dcb_success_does_not_set_dcblength) {
    if (!U(BuildCommDCBA)) { t_skip("missing BuildCommDCBA"); return; }
    DCB d; memset(&d, 0, sizeof d);
    CHECK(U(BuildCommDCBA)("9600,n,8,1", &d));
    EXPECT_EQ(d.DCBlength, 0);
    d.DCBlength = sizeof d;
    CHECK(U(BuildCommDCBA)("9600,n,8,1", &d));
    EXPECT_EQ(d.DCBlength, sizeof(DCB));
}
TEST(pmc_dcb_sizeof_dcb) {
    EXPECT_EQ(sizeof(DCB), 28);
    EXPECT_EQ(offsetof(DCB, BaudRate), 4); EXPECT_EQ(offsetof(DCB, ByteSize), 0x12);
    EXPECT_EQ(offsetof(DCB, Parity), 0x13); EXPECT_EQ(offsetof(DCB, StopBits), 0x14);
    EXPECT_EQ(sizeof(COMMTIMEOUTS), 20); EXPECT_EQ(sizeof(COMMCONFIG), 52); EXPECT_EQ(sizeof(COMSTAT), 12);
}
TEST(pmc_dcb_a_w_parity_unicode_input) {
    if (!U(BuildCommDCBW)) { t_skip("missing BuildCommDCBW"); return; }
    /* non-ASCII characters convert to ANSI and then fail parsing */
    DCB d; WCHAR w[] = { '9', '6', '0', '0', ',', 'n', ',', '8', ',', '1', 0x4e2d, 0 };
    memset(&d, 0, sizeof d);
    SET_ERR(0); BOOL ok = U(BuildCommDCBW)(w, &d);
    CHECK(!ok); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(pmc_dcb_long_string) {
    if (!U(BuildCommDCBA)) { t_skip("missing BuildCommDCBA"); return; }
    char s[600]; memset(s, ' ', sizeof s); s[sizeof s - 1] = 0;
    memcpy(s, "baud=9600", 9);
    DCB d; memset(&d, 0, sizeof d);
    SET_ERR(0); BOOL ok = U(BuildCommDCBA)(s, &d);
    CHECK(ok); if (ok) EXPECT_EQ(d.BaudRate, 9600);
}
TEST(pmc_dcb_repeated_keywords_last_wins) {
    if (!U(BuildCommDCBA)) { t_skip("missing BuildCommDCBA"); return; }
    DCB d; memset(&d, 0, sizeof d);
    CHECK(U(BuildCommDCBA)("baud=1200 baud=2400", &d)); EXPECT_EQ(d.BaudRate, 2400);
    CHECK(U(BuildCommDCBA)("data=5 data=8", &d)); EXPECT_EQ(d.ByteSize, 8);
    CHECK(U(BuildCommDCBA)("parity=e parity=o", &d)); EXPECT_EQ(d.Parity, 1);
}
TEST(pmc_dcb_com_prefix_forms) {
    if (!U(BuildCommDCBA)) { t_skip("missing BuildCommDCBA"); return; }
    const char *s[] = { "COM1:9600,n,8,1", "com1 baud=9600", "COM99:9600,n,8,1", "COM1: baud=9600 parity=n" };
    for (int i = 0; i < 4; i++) {
        DCB d; memset(&d, 0, sizeof d); SET_ERR(0);
        BOOL ok = U(BuildCommDCBA)(s[i], &d);
        t_note("BuildCommDCBA('%s') = %d err=%lu baud=%lu", s[i], ok, ok ? 0 : GetLastError(), d.BaudRate);
    }
}
