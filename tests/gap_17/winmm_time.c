#include "t.h"
/* gap17: timeBeginPeriod/timeEndPeriod/timeGetDevCaps/timeGetSystemTime/timeGetTime
 * as exported by kernel32.dll (Win10 moved these into kernel32). Error codes:
 * TIMERR_NOCANDO 0x61 (97), TIMERR_STRUCT 0x81 (129). */

typedef struct { UINT wPeriodMin, wPeriodMax; } G17_TIMECAPS;
typedef struct { UINT wType; union { DWORD ms; DWORD sample; DWORD cb; DWORD ticks; } u; BYTE pad[16]; } G17_MMTIME;
typedef UINT (WINAPI *tbp_pfn)(UINT);
typedef UINT (WINAPI *tgdc_pfn)(G17_TIMECAPS *, UINT);
typedef UINT (WINAPI *tgst_pfn)(G17_MMTIME *, UINT);
typedef DWORD (WINAPI *tgt_pfn)(void);

#define TNOCANDO 0x61
#define TSTRUCT 0x81
#define TIME_MS 1

#define WINE_SKIP(why) do { if (t_is_wine()) { t_skip(why); return; } } while (0)

static tbp_pfn BP(void) { return (tbp_pfn)t_proc_k32("timeBeginPeriod"); }
static tbp_pfn EP(void) { return (tbp_pfn)t_proc_k32("timeEndPeriod"); }
static tgdc_pfn DC(void) { return (tgdc_pfn)t_proc_k32("timeGetDevCaps"); }
static tgst_pfn ST(void) { return (tgst_pfn)t_proc_k32("timeGetSystemTime"); }
static tgt_pfn GT(void) { return (tgt_pfn)t_proc_k32("timeGetTime"); }

TEST(gap17_timeGetDevCaps_basic) {
    WINE_SKIP("wine reports different wPeriodMax");
    tgdc_pfn f = DC(); REQUIRE(f);
    G17_TIMECAPS c = { 0xdead, 0xbeef };
    EXPECT_EQ(f(&c, 8), 0);
    CHECK(c.wPeriodMin >= 1 && c.wPeriodMin <= 20);
    EXPECT_EQ(c.wPeriodMax, 1000000);
    CHECK(c.wPeriodMin < c.wPeriodMax);
    t_note("wPeriodMin=%u", c.wPeriodMin);
}
TEST(gap17_timeGetDevCaps_sizes) {
    WINE_SKIP("wine reports different wPeriodMax");
    tgdc_pfn f = DC(); REQUIRE(f);
    G17_TIMECAPS big[4];
    memset(big, 0x77, sizeof big);
    EXPECT_EQ(f((G17_TIMECAPS *)big, 8), 0);
    EXPECT_EQ(big[1].wPeriodMin, 0x77777777);   /* only 8 bytes written */
    memset(big, 0x77, sizeof big);
    EXPECT_EQ(f(big, 64), 0);                   /* larger cb accepted */
    EXPECT_EQ(big[0].wPeriodMax, 1000000);
    EXPECT_EQ(big[1].wPeriodMin, 0x77777777);
    G17_TIMECAPS c = { 0x11, 0x22 };
    for (UINT cb = 0; cb < 8; cb++) {
        c.wPeriodMin = 0x11; c.wPeriodMax = 0x22;
        CHECKF(f(&c, cb) == TNOCANDO, "cb=%u", cb);
        CHECKF(c.wPeriodMin == 0x11 && c.wPeriodMax == 0x22, "cb=%u untouched", cb);
    }
}
TEST(gap17_timeGetDevCaps_null) {
    tgdc_pfn f = DC(); REQUIRE(f);
    EXPECT_EQ(f(NULL, 8), TNOCANDO);
    EXPECT_EQ(f(NULL, 0), TNOCANDO);
    EXPECT_EQ(f(NULL, 0xffffffff), TNOCANDO);
}
TEST(gap17_timeGetDevCaps_no_lasterror) {
    tgdc_pfn f = DC(); REQUIRE(f);
    G17_TIMECAPS c;
    SET_ERR(0x999);
    f(&c, 8);
    EXPECT_ERR(0x999);
    SET_ERR(0x999);
    f(NULL, 8);
    EXPECT_ERR(0x999);   /* MMRESULT-style: last error not used */
}
TEST(gap17_timeGetDevCaps_repeatable) {
    tgdc_pfn f = DC(); REQUIRE(f);
    G17_TIMECAPS a, b;
    EXPECT_EQ(f(&a, 8), 0);
    for (int i = 0; i < 100; i++) { EXPECT_EQ(f(&b, 8), 0); CHECK(a.wPeriodMin == b.wPeriodMin && a.wPeriodMax == b.wPeriodMax); }
}

TEST(gap17_timeGetTime_monotonic) {
    tgt_pfn f = GT(); REQUIRE(f);
    DWORD prev = f();
    for (int i = 0; i < 2000; i++) {
        DWORD now = f();
        if ((int)(now - prev) < 0) { CHECKF(0, "went backwards %lu -> %lu", prev, now); break; }
        prev = now;
    }
}
TEST(gap17_timeGetTime_advances) {
    tgt_pfn f = GT(); REQUIRE(f);
    DWORD a = f();
    Sleep(120);
    DWORD b = f();
    DWORD d = b - a;
    CHECKF(d >= 90 && d < 3000, "delta=%lu", d);
}
TEST(gap17_timeGetTime_tracks_gettickcount) {
    tgt_pfn f = GT(); REQUIRE(f);
    /* Both are millisecond clocks; they differ by a constant offset up to the
     * boot-relative base. Deltas must agree closely. */
    DWORD t0 = f(), k0 = GetTickCount();
    Sleep(200);
    DWORD t1 = f(), k1 = GetTickCount();
    int dd = (int)((t1 - t0) - (k1 - k0));
    CHECKF(dd > -60 && dd < 60, "delta diff %d", dd);
}
TEST(gap17_timeGetTime_no_lasterror) {
    tgt_pfn f = GT(); REQUIRE(f);
    SET_ERR(0x4242);
    f();
    EXPECT_ERR(0x4242);
}
TEST(gap17_timeGetTime_cross_thread) {
    tgt_pfn f = GT(); REQUIRE(f);
    struct S { tgt_pfn f; DWORD v; };
    struct S s = { f, 0 };
    DWORD WINAPI th(LPVOID p) { struct S *x = p; x->v = x->f(); return 0; }
    DWORD before = f();
    HANDLE h = CreateThread(NULL, 0, th, &s, 0, NULL);
    if (!h) return;
    WaitForSingleObject(h, 5000);
    CloseHandle(h);
    DWORD after = f();
    CHECKF((int)(s.v - before) >= 0 && (int)(after - s.v) >= 0, "before=%lu thread=%lu after=%lu", before, s.v, after);
}

TEST(gap17_timeGetSystemTime_basic) {
    tgst_pfn f = ST(); REQUIRE(f); REQUIRE(GT());
    G17_MMTIME t; memset(&t, 0xcc, sizeof t);
    DWORD before = GT()();
    EXPECT_EQ(f(&t, 12), 0);
    DWORD after = GT()();
    EXPECT_EQ(t.wType, TIME_MS);
    CHECKF((int)(t.u.ms - before) >= 0 && (int)(after - t.u.ms) >= 0, "ms=%lu [%lu,%lu]", t.u.ms, before, after);
}
TEST(gap17_timeGetSystemTime_sizes) {
    WINE_SKIP("wine timeGetSystemTime faults/differs on invalid args");
    tgst_pfn f = ST(); REQUIRE(f);
    G17_MMTIME t;
    for (UINT cb = 0; cb < 12; cb++) {
        memset(&t, 0xcc, sizeof t);
        CHECKF(f(&t, cb) == TSTRUCT, "cb=%u", cb);
        CHECKF(t.wType == 0xcccccccc, "cb=%u untouched", cb);
    }
    memset(&t, 0xcc, sizeof t);
    EXPECT_EQ(f(&t, 12), 0);
    EXPECT_EQ(t.wType, TIME_MS);
    memset(&t, 0xcc, sizeof t);
    EXPECT_EQ(f(&t, sizeof t), 0);
    EXPECT_EQ(t.wType, TIME_MS);
    EXPECT_EQ(t.pad[8], 0xcc);   /* only the first 12 bytes written */
    memset(&t, 0xcc, sizeof t);
    EXPECT_EQ(f(&t, 0xffffffff), 0);
}
TEST(gap17_timeGetSystemTime_null) {
    WINE_SKIP("wine timeGetSystemTime faults/differs on invalid args");
    tgst_pfn f = ST(); REQUIRE(f);
    EXPECT_EQ(f(NULL, 12), TSTRUCT);
    EXPECT_EQ(f(NULL, 0), TSTRUCT);
    EXPECT_EQ(f(NULL, 0xffffffff), TSTRUCT);
}
TEST(gap17_timeGetSystemTime_no_lasterror) {
    WINE_SKIP("wine timeGetSystemTime faults/differs on invalid args");
    tgst_pfn f = ST(); REQUIRE(f);
    G17_MMTIME t;
    SET_ERR(0x31);
    f(&t, 12); EXPECT_ERR(0x31);
    f(NULL, 12); EXPECT_ERR(0x31);
}

/* Begin/End period. All pairs are balanced so the process timer resolution is restored. */
TEST(gap17_timeBeginPeriod_invalid) {
    tbp_pfn b = BP(); tbp_pfn e = EP(); tgdc_pfn dc = DC();
    REQUIRE(b); REQUIRE(e); REQUIRE(dc);
    G17_TIMECAPS c; dc(&c, 8);
    EXPECT_EQ(b(0), TNOCANDO);
    if (c.wPeriodMin > 1) EXPECT_EQ(b(c.wPeriodMin - 1), TNOCANDO);
    EXPECT_EQ(e(0), TNOCANDO);
}
TEST(gap17_timeBeginPeriod_valid_pairs) {
    tbp_pfn b = BP(); tbp_pfn e = EP(); tgdc_pfn dc = DC();
    REQUIRE(b); REQUIRE(e); REQUIRE(dc);
    G17_TIMECAPS c; dc(&c, 8);
    UINT periods[] = { c.wPeriodMin, c.wPeriodMin + 1, 5, 10, 15, 16, 20 };
    for (unsigned i = 0; i < sizeof periods / sizeof *periods; i++) {
        UINT p = periods[i];
        if (p < c.wPeriodMin) continue;
        CHECKF(b(p) == 0, "begin %u", p);
        CHECKF(e(p) == 0, "end %u", p);
    }
}
TEST(gap17_timeEndPeriod_unmatched) {
    WINE_SKIP("wine does not refcount timeEndPeriod");
    tbp_pfn b = BP(); tbp_pfn e = EP(); tgdc_pfn dc = DC();
    REQUIRE(b); REQUIRE(e); REQUIRE(dc);
    G17_TIMECAPS c; dc(&c, 8);
    UINT p = c.wPeriodMin < 7 ? 7 : c.wPeriodMin;
    EXPECT_EQ(e(p), TNOCANDO);          /* never begun: refcount 0 */
    EXPECT_EQ(b(p), 0);
    EXPECT_EQ(e(p), 0);
    EXPECT_EQ(e(p), TNOCANDO);          /* second end underflows */
}
TEST(gap17_timeBeginPeriod_refcount) {
    WINE_SKIP("wine does not refcount timeEndPeriod");
    tbp_pfn b = BP(); tbp_pfn e = EP(); tgdc_pfn dc = DC();
    REQUIRE(b); REQUIRE(e); REQUIRE(dc);
    G17_TIMECAPS c; dc(&c, 8);
    UINT p = c.wPeriodMin < 3 ? 3 : c.wPeriodMin;
    for (int i = 0; i < 50; i++) CHECK(b(p) == 0);
    for (int i = 0; i < 50; i++) CHECK(e(p) == 0);
    EXPECT_EQ(e(p), TNOCANDO);
}
TEST(gap17_timeBeginPeriod_independent_periods) {
    WINE_SKIP("wine does not refcount timeEndPeriod");
    tbp_pfn b = BP(); tbp_pfn e = EP(); tgdc_pfn dc = DC();
    REQUIRE(b); REQUIRE(e); REQUIRE(dc);
    G17_TIMECAPS c; dc(&c, 8);
    UINT p1 = c.wPeriodMin + 3, p2 = c.wPeriodMin + 5;
    EXPECT_EQ(b(p1), 0);
    EXPECT_EQ(e(p2), TNOCANDO);   /* p2 has no outstanding begin */
    EXPECT_EQ(b(p2), 0);
    EXPECT_EQ(e(p1), 0);
    EXPECT_EQ(e(p2), 0);
}
TEST(gap17_timeBeginPeriod_above_max_noop) {
    tbp_pfn b = BP(); tbp_pfn e = EP(); tgdc_pfn dc = DC();
    REQUIRE(b); REQUIRE(e); REQUIRE(dc);
    G17_TIMECAPS c; dc(&c, 8);
    /* decomp: periods at/above an internal maximum succeed without tracking; the
     * exact threshold is internal, so only observe. */
    t_note("begin(max)=%u begin(max+1)=%u begin(~0)=%u", b(c.wPeriodMax), b(c.wPeriodMax + 1), b(0xffffffff));
    t_note("end(max)=%u end(~0)=%u", e(c.wPeriodMax), e(0xffffffff));
}
TEST(gap17_timeBeginPeriod_affects_sleep_granularity) {
    tbp_pfn b = BP(); tbp_pfn e = EP(); tgt_pfn t = GT();
    REQUIRE(b); REQUIRE(e); REQUIRE(t);
    if (b(1) != 0) { t_note("timeBeginPeriod(1) failed"); return; }
    DWORD s = t();
    for (int i = 0; i < 20; i++) Sleep(1);
    DWORD d = t() - s;
    EXPECT_EQ(e(1), 0);
    CHECKF(d < 600, "20x Sleep(1) took %lu ms", d);
}
TEST(gap17_timeBeginPeriod_no_lasterror) {
    tbp_pfn b = BP(); tbp_pfn e = EP(); tgdc_pfn dc = DC();
    REQUIRE(b); REQUIRE(e); REQUIRE(dc);
    G17_TIMECAPS c; dc(&c, 8);
    UINT p = c.wPeriodMin < 4 ? 4 : c.wPeriodMin;
    SET_ERR(0x55);
    b(p); EXPECT_ERR(0x55);
    e(p); EXPECT_ERR(0x55);
    e(p); EXPECT_ERR(0x55);
}
