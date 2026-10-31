/* gap_03: Event* (ETW provider API) */
#include "t.h"
#include <evntprov.h>

static const GUID G3_PROV = { 0x6b8e3a10, 0x4c2f, 0x4f3a, { 0x9e, 0x11, 0x5a, 0x0d, 0xc7, 0x21, 0x03, 0x01 } };
static const GUID G3_PROV2 = { 0x6b8e3a10, 0x4c2f, 0x4f3a, { 0x9e, 0x11, 0x5a, 0x0d, 0xc7, 0x21, 0x03, 0x02 } };

static REGHANDLE g3_reg(const GUID *g) {
    REGHANDLE h = 0;
    ULONG r = U(EventRegister)(g, NULL, NULL, &h);
    return r == ERROR_SUCCESS ? h : 0;
}

/* ---- EventRegister / EventUnregister ---- */
TEST(gap3_eventregister_basic) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventRegister)); REQUIRE(U(EventUnregister));
    REGHANDLE h = 0xDEAD;
    ULONG r = U(EventRegister)(&G3_PROV, NULL, NULL, &h);
    EXPECT_EQ(r, ERROR_SUCCESS);
    CHECK(h != 0 && h != 0xDEAD);
    r = U(EventUnregister)(h);
    EXPECT_EQ(r, ERROR_SUCCESS);
}
TEST(gap3_eventregister_null_provider) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventRegister));
    REGHANDLE h = 0xDEAD;
    ULONG r = U(EventRegister)(NULL, NULL, NULL, &h);
    EXPECT_EQ(r, ERROR_INVALID_PARAMETER);
    CHECK(h == 0xDEAD || h == 0);
}
TEST(gap3_eventregister_null_reghandle) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventRegister));
    ULONG r = U(EventRegister)(&G3_PROV, NULL, NULL, NULL);
    EXPECT_EQ(r, ERROR_INVALID_PARAMETER);
}
static VOID NTAPI g3_enable_cb(LPCGUID id, ULONG src, UCHAR lvl, ULONGLONG any, ULONGLONG all, PEVENT_FILTER_DESCRIPTOR f, PVOID ctx) {}
TEST(gap3_eventregister_context_without_callback) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventRegister));
    REGHANDLE h = 0;
    ULONG r = U(EventRegister)(&G3_PROV, NULL, (PVOID)0x1234, &h);
    EXPECT_EQ(r, ERROR_INVALID_PARAMETER);       /* ntdll: context requires a callback */
    if (r == 0 && h) U(EventUnregister)(h);
}
TEST(gap3_eventregister_with_callback) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventRegister)); REQUIRE(U(EventUnregister));
    REGHANDLE h = 0;
    ULONG r = U(EventRegister)(&G3_PROV, g3_enable_cb, (PVOID)0x1234, &h);
    EXPECT_EQ(r, ERROR_SUCCESS);
    CHECK(h != 0);
    EXPECT_EQ(U(EventUnregister)(h), ERROR_SUCCESS);
}
TEST(gap3_eventregister_callback_without_context) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventRegister)); REQUIRE(U(EventUnregister));
    REGHANDLE h = 0;
    ULONG r = U(EventRegister)(&G3_PROV, g3_enable_cb, NULL, &h);
    EXPECT_EQ(r, ERROR_SUCCESS);
    if (r == 0) U(EventUnregister)(h);
}
TEST(gap3_eventregister_same_guid_twice_distinct_handles) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventRegister)); REQUIRE(U(EventUnregister));
    REGHANDLE a = g3_reg(&G3_PROV), b = g3_reg(&G3_PROV);
    CHECK(a != 0 && b != 0);
    CHECK(a != b);
    if (a) EXPECT_EQ(U(EventUnregister)(a), ERROR_SUCCESS);
    if (b) EXPECT_EQ(U(EventUnregister)(b), ERROR_SUCCESS);
}
TEST(gap3_eventregister_two_guids) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventRegister)); REQUIRE(U(EventUnregister));
    REGHANDLE a = g3_reg(&G3_PROV), b = g3_reg(&G3_PROV2);
    CHECK(a != 0 && b != 0 && a != b);
    if (a) U(EventUnregister)(a);
    if (b) U(EventUnregister)(b);
}
TEST(gap3_eventregister_zero_guid) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventRegister)); REQUIRE(U(EventUnregister));
    GUID z; memset(&z, 0, sizeof z);
    REGHANDLE h = 0;
    ULONG r = U(EventRegister)(&z, NULL, NULL, &h);
    t_note("zero GUID provider: r=%lu", r);
    if (r == 0 && h) U(EventUnregister)(h);
}
TEST(gap3_eventregister_many_handles) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventRegister)); REQUIRE(U(EventUnregister));
    enum { N = 64 };
    REGHANDLE hs[N]; int ok = 0;
    for (int i = 0; i < N; i++) { hs[i] = g3_reg(&G3_PROV); if (hs[i]) ok++; }
    EXPECT_EQ(ok, N);
    for (int i = 0; i < N; i++) for (int j = i + 1; j < N; j++) if (hs[i] && hs[i] == hs[j]) CHECK(0);
    for (int i = 0; i < N; i++) if (hs[i]) U(EventUnregister)(hs[i]);
}
TEST(gap3_eventregister_cycle) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventRegister)); REQUIRE(U(EventUnregister));
    for (int i = 0; i < 500; i++) {
        REGHANDLE h = g3_reg(&G3_PROV);
        if (!h) { CHECK(0); break; }
        if (U(EventUnregister)(h) != 0) { CHECK(0); break; }
    }
}
TEST(gap3_eventregister_handle_shape) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventRegister)); REQUIRE(U(EventUnregister));
    REGHANDLE h = g3_reg(&G3_PROV);
    CHECK(h != 0);
    if (h) {
        t_note("REGHANDLE 0x%llx", (unsigned long long)h);
        CHECK((h & 1) == 0);           /* low bit is reserved for invalid markers in ntdll */
        CHECK((h >> 48) != 0);         /* generation tag */
        U(EventUnregister)(h);
    }
}
static DWORD WINAPI g3_reg_thread(LPVOID p) {
    for (int i = 0; i < 100; i++) {
        REGHANDLE h = g3_reg(&G3_PROV);
        if (!h) return 1;
        if (U(EventUnregister)(h)) return 2;
    }
    return 0;
}
TEST(gap3_eventregister_threads) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventRegister)); REQUIRE(U(EventUnregister));
    HANDLE th[8];
    for (int i = 0; i < 8; i++) th[i] = CreateThread(NULL, 0, g3_reg_thread, NULL, 0, NULL);
    for (int i = 0; i < 8; i++) {
        WaitForSingleObject(th[i], 20000);
        DWORD c = 99; GetExitCodeThread(th[i], &c);
        EXPECT_EQ(c, 0);
        CloseHandle(th[i]);
    }
}

/* ---- EventEnabled / EventProviderEnabled ---- */
TEST(gap3_eventenabled_no_session) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventEnabled)); REQUIRE(U(EventRegister)); REQUIRE(U(EventUnregister));
    REGHANDLE h = g3_reg(&G3_PROV);
    CHECK(h != 0);
    EVENT_DESCRIPTOR d; memset(&d, 0, sizeof d);
    d.Id = 1; d.Level = 4; d.Keyword = 1;
    CHECK(U(EventEnabled)(h, &d) == FALSE);
    d.Level = 0; d.Keyword = 0;
    CHECK(U(EventEnabled)(h, &d) == FALSE);
    d.Level = 255; d.Keyword = ~0ULL;
    CHECK(U(EventEnabled)(h, &d) == FALSE);
    U(EventUnregister)(h);
}
TEST(gap3_eventenabled_null_descriptor) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventEnabled)); REQUIRE(U(EventRegister)); REQUIRE(U(EventUnregister));
    REGHANDLE h = g3_reg(&G3_PROV);
    CHECK(U(EventEnabled)(h, NULL) == FALSE);
    CHECK(U(EventEnabled)(0, NULL) == FALSE);
    U(EventUnregister)(h);
}
TEST(gap3_eventenabled_invalid_handles) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventEnabled));
    EVENT_DESCRIPTOR d; memset(&d, 0, sizeof d);
    CHECK(U(EventEnabled)(0, &d) == FALSE);
    CHECK(U(EventEnabled)(1, &d) == FALSE);          /* odd values are always rejected */
    CHECK(U(EventEnabled)(0xFFFF000000000001ULL, &d) == FALSE);
    CHECK(U(EventEnabled)(0x0000000000000003ULL, &d) == FALSE);
}
TEST(gap3_eventproviderenabled_no_session) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventProviderEnabled)); REQUIRE(U(EventRegister)); REQUIRE(U(EventUnregister));
    REGHANDLE h = g3_reg(&G3_PROV);
    CHECK(h != 0);
    CHECK(U(EventProviderEnabled)(h, 0, 0) == FALSE);
    CHECK(U(EventProviderEnabled)(h, 5, 0xFF) == FALSE);
    CHECK(U(EventProviderEnabled)(h, 255, ~0ULL) == FALSE);
    U(EventUnregister)(h);
}
TEST(gap3_eventproviderenabled_invalid_handles) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventProviderEnabled));
    CHECK(U(EventProviderEnabled)(0, 0, 0) == FALSE);
    CHECK(U(EventProviderEnabled)(1, 0, 0) == FALSE);
    CHECK(U(EventProviderEnabled)(0x0000000000000003ULL, 4, 1) == FALSE);
}

/* ---- EventWriteString ---- */
TEST(gap3_eventwritestring_not_enabled_success) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventWriteString)); REQUIRE(U(EventRegister)); REQUIRE(U(EventUnregister));
    REGHANDLE h = g3_reg(&G3_PROV);
    CHECK(h != 0);
    EXPECT_EQ(U(EventWriteString)(h, 0, 0, L"hello"), ERROR_SUCCESS);
    EXPECT_EQ(U(EventWriteString)(h, 4, 1, L""), ERROR_SUCCESS);
    EXPECT_EQ(U(EventWriteString)(h, 255, ~0ULL, L"x"), ERROR_SUCCESS);
    U(EventUnregister)(h);
}
TEST(gap3_eventwritestring_null_string) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventWriteString)); REQUIRE(U(EventRegister)); REQUIRE(U(EventUnregister));
    REGHANDLE h = g3_reg(&G3_PROV);
    CHECK(h != 0);
    EXPECT_EQ(U(EventWriteString)(h, 0, 0, NULL), ERROR_INVALID_PARAMETER);
    U(EventUnregister)(h);
}
TEST(gap3_eventwritestring_invalid_handles) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventWriteString));
    EXPECT_EQ(U(EventWriteString)(0, 0, 0, L"x"), ERROR_INVALID_HANDLE);
    EXPECT_EQ(U(EventWriteString)(1, 0, 0, L"x"), ERROR_INVALID_HANDLE);
    EXPECT_EQ(U(EventWriteString)(0x0000000000000003ULL, 0, 0, L"x"), ERROR_INVALID_HANDLE);
    EXPECT_EQ(U(EventWriteString)(0, 0, 0, NULL), ERROR_INVALID_HANDLE);   /* handle is validated first */
}
TEST(gap3_eventwritestring_wrong_generation) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventWriteString)); REQUIRE(U(EventRegister)); REQUIRE(U(EventUnregister));
    REGHANDLE h = g3_reg(&G3_PROV);
    CHECK(h != 0);
    EXPECT_EQ(U(EventWriteString)(h | 1, 0, 0, L"x"), ERROR_INVALID_HANDLE);
    U(EventUnregister)(h);
}
TEST(gap3_eventwritestring_long_string) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventWriteString)); REQUIRE(U(EventRegister)); REQUIRE(U(EventUnregister));
    static WCHAR big[20000];
    for (int i = 0; i < 19999; i++) big[i] = L'a' + i % 26;
    big[19999] = 0;
    REGHANDLE h = g3_reg(&G3_PROV);
    EXPECT_EQ(U(EventWriteString)(h, 4, 0, big), ERROR_SUCCESS);
    U(EventUnregister)(h);
}

/* ---- EventWrite / Transfer / Ex ---- */
TEST(gap3_eventwritetransfer_not_enabled) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventWriteTransfer)); REQUIRE(U(EventRegister)); REQUIRE(U(EventUnregister));
    REGHANDLE h = g3_reg(&G3_PROV);
    EVENT_DESCRIPTOR d; memset(&d, 0, sizeof d); d.Id = 7; d.Level = 4;
    EXPECT_EQ(U(EventWriteTransfer)(h, &d, NULL, NULL, 0, NULL), ERROR_SUCCESS);
    GUID act = { 1, 2, 3, { 4, 5, 6, 7, 8, 9, 10, 11 } };
    EXPECT_EQ(U(EventWriteTransfer)(h, &d, &act, NULL, 0, NULL), ERROR_SUCCESS);
    EXPECT_EQ(U(EventWriteTransfer)(h, &d, &act, &act, 0, NULL), ERROR_SUCCESS);
    U(EventUnregister)(h);
}
TEST(gap3_eventwritetransfer_with_data) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventWriteTransfer)); REQUIRE(U(EventRegister)); REQUIRE(U(EventUnregister));
    REGHANDLE h = g3_reg(&G3_PROV);
    EVENT_DESCRIPTOR d; memset(&d, 0, sizeof d); d.Id = 8;
    DWORD val = 0x1234; WCHAR str[] = L"payload";
    EVENT_DATA_DESCRIPTOR dd[2];
    EventDataDescCreate(&dd[0], &val, sizeof val);
    EventDataDescCreate(&dd[1], str, sizeof str);
    EXPECT_EQ(U(EventWriteTransfer)(h, &d, NULL, NULL, 2, dd), ERROR_SUCCESS);
    U(EventUnregister)(h);
}
TEST(gap3_eventwrite_unregistered_in_child_notes) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    /* behaviour with a zero/odd handle is reported only; see child */
    REQUIRE(U(EventWriteTransfer));
    DWORD code = t_run_child("gap3_etw_bad", "", 20000);
    t_note("child exit code (bad-handle write results encoded) = 0x%lx", code);
    CHECK(code < 0xDEAD0000);
}
CHILD(gap3_etw_bad) {
    if (!U(EventWriteTransfer) || !U(EventWriteEx)) return 3;
    EVENT_DESCRIPTOR d; memset(&d, 0, sizeof d);
    ULONG a = U(EventWriteTransfer)(0, &d, NULL, NULL, 0, NULL);
    ULONG b = U(EventWriteTransfer)(1, &d, NULL, NULL, 0, NULL);
    ULONG c = U(EventWriteEx)(0, &d, 0, 0, NULL, NULL, 0, NULL);
    return (int)((a & 0xFF) | ((b & 0xFF) << 8) | ((c & 0xFF) << 16));
}
TEST(gap3_eventwriteex_not_enabled) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventWriteEx)); REQUIRE(U(EventRegister)); REQUIRE(U(EventUnregister));
    REGHANDLE h = g3_reg(&G3_PROV);
    CHECK(h != 0);
    EVENT_DESCRIPTOR d; memset(&d, 0, sizeof d); d.Id = 9;
    EXPECT_EQ(U(EventWriteEx)(h, &d, 0, 0, NULL, NULL, 0, NULL), ERROR_SUCCESS);
    EXPECT_EQ(U(EventWriteEx)(h, &d, 0, EVENT_FILTER_TYPE_SCHEMATIZED, NULL, NULL, 0, NULL), ERROR_SUCCESS);
    EXPECT_EQ(U(EventWriteEx)(h, &d, 0x10, 0, NULL, NULL, 0, NULL), ERROR_SUCCESS);
    U(EventUnregister)(h);
}
TEST(gap3_eventwrite_family_consistent) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventWriteEx)); REQUIRE(U(EventWriteTransfer)); REQUIRE(U(EventRegister)); REQUIRE(U(EventUnregister));
    REGHANDLE h = g3_reg(&G3_PROV);
    EVENT_DESCRIPTOR d; memset(&d, 0, sizeof d);
    for (int i = 0; i < 1000; i++) {
        d.Id = (USHORT)i;
        if (U(EventWriteTransfer)(h, &d, NULL, NULL, 0, NULL)) { CHECK(0); break; }
        if (U(EventWriteEx)(h, &d, 0, 0, NULL, NULL, 0, NULL)) { CHECK(0); break; }
    }
    U(EventUnregister)(h);
}

/* ---- EventSetInformation ---- */
TEST(gap3_eventsetinformation_unknown_classes) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventSetInformation)); REQUIRE(U(EventRegister)); REQUIRE(U(EventUnregister));
    REGHANDLE h = g3_reg(&G3_PROV);
    CHECK(h != 0);
    BYTE buf[16] = {0};
    static const int bad[] = { 1, 4, 5, 6, 7, 99, 0x7FFFFFFF, -1 };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        ULONG r = U(EventSetInformation)(h, (EVENT_INFO_CLASS)bad[i], buf, sizeof buf);
        CHECKF(r == ERROR_NOT_SUPPORTED, "class %d -> %lu", bad[i], r);
    }
    U(EventUnregister)(h);
}
TEST(gap3_eventsetinformation_traits_invalid) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventSetInformation)); REQUIRE(U(EventRegister)); REQUIRE(U(EventUnregister));
    REGHANDLE h = g3_reg(&G3_PROV);
    BYTE buf[16] = {0};
    /* EventProviderSetTraits = 2: data must be non-NULL and 3..0xFFFF bytes */
    EXPECT_EQ(U(EventSetInformation)(h, (EVENT_INFO_CLASS)2, NULL, 8), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(U(EventSetInformation)(h, (EVENT_INFO_CLASS)2, buf, 0), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(U(EventSetInformation)(h, (EVENT_INFO_CLASS)2, buf, 1), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(U(EventSetInformation)(h, (EVENT_INFO_CLASS)2, buf, 2), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(U(EventSetInformation)(h, (EVENT_INFO_CLASS)2, buf, 0x10000), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(U(EventSetInformation)(h, (EVENT_INFO_CLASS)2, buf, 0xFFFFFFFF), ERROR_INVALID_PARAMETER);
    U(EventUnregister)(h);
}
TEST(gap3_eventsetinformation_descriptor_type_invalid) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventSetInformation)); REQUIRE(U(EventRegister)); REQUIRE(U(EventUnregister));
    REGHANDLE h = g3_reg(&G3_PROV);
    BYTE buf[16] = {0};
    /* EventProviderUseDescriptorType = 3: one byte of data */
    EXPECT_EQ(U(EventSetInformation)(h, (EVENT_INFO_CLASS)3, NULL, 1), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(U(EventSetInformation)(h, (EVENT_INFO_CLASS)3, buf, 0), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(U(EventSetInformation)(h, (EVENT_INFO_CLASS)3, buf, 2), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(U(EventSetInformation)(h, (EVENT_INFO_CLASS)3, buf, 4), ERROR_INVALID_PARAMETER);
    U(EventUnregister)(h);
}
TEST(gap3_eventsetinformation_binary_track_wrong_size) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventSetInformation)); REQUIRE(U(EventRegister)); REQUIRE(U(EventUnregister));
    REGHANDLE h = g3_reg(&G3_PROV);
    BYTE buf[16] = {0};
    /* EventProviderBinaryTrackInfo = 0 requires a zero length */
    EXPECT_EQ(U(EventSetInformation)(h, (EVENT_INFO_CLASS)0, buf, 4), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(U(EventSetInformation)(h, (EVENT_INFO_CLASS)0, buf, 1), ERROR_INVALID_PARAMETER);
    U(EventUnregister)(h);
}
TEST(gap3_eventsetinformation_binary_track_ok) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventSetInformation)); REQUIRE(U(EventRegister)); REQUIRE(U(EventUnregister));
    REGHANDLE h = g3_reg(&G3_PROV);
    ULONG r = U(EventSetInformation)(h, (EVENT_INFO_CLASS)0, NULL, 0);
    t_note("track binary info: %lu", r);
    U(EventUnregister)(h);
}
TEST(gap3_eventsetinformation_traits_valid_notes) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventSetInformation)); REQUIRE(U(EventRegister)); REQUIRE(U(EventUnregister));
    REGHANDLE h = g3_reg(&G3_PROV);
    /* provider traits: UINT16 size, name\0 */
    BYTE traits[16]; USHORT sz = 8; memcpy(traits, &sz, 2); memcpy(traits + 2, "k32t\0\0", 6);
    ULONG r = U(EventSetInformation)(h, (EVENT_INFO_CLASS)2, traits, 8);
    t_note("set traits: %lu", r);
    BYTE one = 0; r = U(EventSetInformation)(h, (EVENT_INFO_CLASS)3, &one, 1);
    t_note("use descriptor type: %lu", r);
    U(EventUnregister)(h);
}

/* ---- EventActivityIdControl ---- */
static GUID g3_zero(void) { GUID z; memset(&z, 0, sizeof z); return z; }
TEST(gap3_eventactivityid_null_guid_all_codes) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventActivityIdControl));
    for (ULONG c = 0; c < 8; c++)
        EXPECT_EQ(U(EventActivityIdControl)(c, NULL), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(U(EventActivityIdControl)(0xFFFFFFFF, NULL), ERROR_INVALID_PARAMETER);
}
TEST(gap3_eventactivityid_invalid_codes) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventActivityIdControl));
    GUID g = g3_zero();
    static const ULONG bad[] = { 0, 6, 7, 100, 0x80000000u, 0xFFFFFFFFu };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0);
        ULONG r = U(EventActivityIdControl)(bad[i], &g);
        CHECKF(r == ERROR_INVALID_PARAMETER, "code %lu -> %lu", bad[i], r);
    }
}
TEST(gap3_eventactivityid_set_get_roundtrip) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventActivityIdControl));
    GUID orig, mine = { 0x11223344, 0x5566, 0x7788, { 1, 2, 3, 4, 5, 6, 7, 8 } }, got;
    EXPECT_EQ(U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_GET_ID, &orig), ERROR_SUCCESS);
    EXPECT_EQ(U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_SET_ID, &mine), ERROR_SUCCESS);
    memset(&got, 0xEE, sizeof got);
    EXPECT_EQ(U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_GET_ID, &got), ERROR_SUCCESS);
    CHECK(!memcmp(&got, &mine, sizeof got));
    EXPECT_EQ(U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_SET_ID, &orig), ERROR_SUCCESS);
    EXPECT_EQ(U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_GET_ID, &got), ERROR_SUCCESS);
    CHECK(!memcmp(&got, &orig, sizeof got));
}
TEST(gap3_eventactivityid_get_set_swaps) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventActivityIdControl));
    GUID orig, a = { 1, 2, 3, { 1, 1, 1, 1, 1, 1, 1, 1 } }, b = { 4, 5, 6, { 2, 2, 2, 2, 2, 2, 2, 2 } }, g;
    U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_GET_ID, &orig);
    U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_SET_ID, &a);
    g = b;
    EXPECT_EQ(U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_GET_SET_ID, &g), ERROR_SUCCESS);
    CHECK(!memcmp(&g, &a, sizeof g));            /* returns previous */
    GUID cur;
    U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_GET_ID, &cur);
    CHECK(!memcmp(&cur, &b, sizeof cur));        /* installed new */
    U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_SET_ID, &orig);
}
TEST(gap3_eventactivityid_create_does_not_set) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventActivityIdControl));
    GUID orig, before, after, c1, c2, z = g3_zero();
    U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_GET_ID, &orig);
    U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_SET_ID, &z);
    U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_GET_ID, &before);
    c1 = z; c2 = z;
    EXPECT_EQ(U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_CREATE_ID, &c1), ERROR_SUCCESS);
    EXPECT_EQ(U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_CREATE_ID, &c2), ERROR_SUCCESS);
    CHECK(memcmp(&c1, &z, sizeof c1) != 0);
    CHECK(memcmp(&c1, &c2, sizeof c1) != 0);
    U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_GET_ID, &after);
    CHECK(!memcmp(&before, &after, sizeof after));
    U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_SET_ID, &orig);
}
TEST(gap3_eventactivityid_create_set_installs) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventActivityIdControl));
    GUID orig, z = g3_zero(), n, cur;
    U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_GET_ID, &orig);
    U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_SET_ID, &z);
    n = z;
    EXPECT_EQ(U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_CREATE_SET_ID, &n), ERROR_SUCCESS);
    U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_GET_ID, &cur);
    t_note("CREATE_SET_ID: current %s output", !memcmp(&cur, &n, sizeof cur) ? "equals" : "differs from");
    t_note("CREATE_SET_ID output %s previous", !memcmp(&n, &z, sizeof n) ? "equals" : "differs from");
    U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_SET_ID, &orig);
}
static DWORD WINAPI g3_act_thread(LPVOID p) {
    GUID g = { 0xAAAA, 0xBBBB, 0xCCCC, { 9, 9, 9, 9, 9, 9, 9, 9 } }, z = g3_zero(), got;
    U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_GET_ID, &got);
    DWORD ok = memcmp(&got, &z, sizeof got) == 0;     /* new threads start with a zero id */
    U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_SET_ID, &g);
    return ok ? 0 : 1;
}
TEST(gap3_eventactivityid_per_thread) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventActivityIdControl));
    GUID mine = { 0x1, 0x2, 0x3, { 0, 0, 0, 0, 0, 0, 0, 7 } }, orig, got;
    U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_GET_ID, &orig);
    U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_SET_ID, &mine);
    HANDLE t = CreateThread(NULL, 0, g3_act_thread, NULL, 0, NULL);
    WaitForSingleObject(t, 10000);
    DWORD c = 9; GetExitCodeThread(t, &c); CloseHandle(t);
    EXPECT_EQ(c, 0);
    U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_GET_ID, &got);
    CHECK(!memcmp(&got, &mine, sizeof got));      /* other thread did not disturb ours */
    U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_SET_ID, &orig);
}
TEST(gap3_eventactivityid_set_zero) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventActivityIdControl));
    GUID orig, z = g3_zero(), got;
    U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_GET_ID, &orig);
    GUID dirty = { 5, 6, 7, { 1, 1, 1, 1, 1, 1, 1, 1 } };
    U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_SET_ID, &dirty);
    EXPECT_EQ(U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_SET_ID, &z), ERROR_SUCCESS);
    U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_GET_ID, &got);
    CHECK(!memcmp(&got, &z, sizeof got));
    U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_SET_ID, &orig);
}
TEST(gap3_eventactivityid_get_is_idempotent) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventActivityIdControl));
    GUID a, b;
    for (int i = 0; i < 50; i++) {
        EXPECT_EQ(U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_GET_ID, &a), 0);
        EXPECT_EQ(U(EventActivityIdControl)(EVENT_ACTIVITY_CTRL_GET_ID, &b), 0);
        CHECK(!memcmp(&a, &b, sizeof a));
    }
}
TEST(gap3_eventactivityid_get_does_not_touch_lasterror) {
    if (t_is_wine()) { t_skip("wine: ETW stubbed"); return; }
    REQUIRE(U(EventActivityIdControl));
    GUID a;
    ULONG (WINAPI *f)(ULONG, LPGUID) = U(EventActivityIdControl);
    SetLastError(0x5151);
    f(EVENT_ACTIVITY_CTRL_GET_ID, &a);
    EXPECT_ERR(0x5151);
}
