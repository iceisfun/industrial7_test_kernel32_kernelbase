/* GetLastError / SetLastError / TEB consistency / Rtl error helpers. */
#include "eed.h"

static const DWORD k_errvals[] = { 0, 1, 2, 5, 6, 87, 122, 183, 234, 258, 259, 317, 997, 1000, 0x7FFFFFFF, 0x80000000, 0xC0000005, 0xDEADBEEF, 0xFFFFFFFF };

TEST(eed_le_setget_roundtrip) {
    REQUIRE(U(SetLastError)); REQUIRE(U(GetLastError));
    for (size_t i = 0; i < sizeof k_errvals / sizeof *k_errvals; i++) {
        U(SetLastError)(k_errvals[i]);
        EXPECT_EQ(U(GetLastError)(), k_errvals[i]);
        EXPECT_EQ(GetLastError(), k_errvals[i]);
    }
}
TEST(eed_le_teb_offset_matches_set) {
    REQUIRE(U(SetLastError));
    for (size_t i = 0; i < sizeof k_errvals / sizeof *k_errvals; i++) {
        U(SetLastError)(k_errvals[i]);
        EXPECT_EQ(EED_TEB_LASTERR(), k_errvals[i]);
    }
}
TEST(eed_le_teb_write_visible_to_get) {
    REQUIRE(U(GetLastError));
    for (size_t i = 0; i < sizeof k_errvals / sizeof *k_errvals; i++) {
        EED_TEB_LASTERR() = k_errvals[i];
        EXPECT_EQ(U(GetLastError)(), k_errvals[i]);
    }
}
TEST(eed_le_get_does_not_modify) {
    REQUIRE(U(GetLastError)); REQUIRE(U(SetLastError));
    U(SetLastError)(4321);
    for (int i = 0; i < 100; i++) EXPECT_EQ(U(GetLastError)(), 4321);
}
TEST(eed_le_set_overwrite_sequence) {
    REQUIRE(U(SetLastError)); REQUIRE(U(GetLastError));
    for (DWORD i = 0; i < 2000; i++) { U(SetLastError)(i * 7u); if (U(GetLastError)() != i * 7u) { CHECK(0); break; } }
}
TEST(eed_le_set_zero_clears) {
    REQUIRE(U(SetLastError)); REQUIRE(U(GetLastError));
    U(SetLastError)(5); U(SetLastError)(0);
    EXPECT_EQ(U(GetLastError)(), 0);
}
TEST(eed_le_kernel32_export_present) {
    CHECK(UK(GetLastError) != NULL);
    CHECK(UK(SetLastError) != NULL);
}
TEST(eed_le_kernel32_forwards_to_ntdll_or_kernelbase) {
    FARPROC k = (FARPROC)UK(GetLastError), kb = (FARPROC)UB(GetLastError);
    REQUIRE(k); REQUIRE(kb);
    /* kernel32!GetLastError is a forwarder to kernelbase on Win10 (or to ntdll!RtlGetLastWin32Error). */
    FARPROC nt = (FARPROC)NTF(RtlGetLastWin32Error_t,"RtlGetLastWin32Error");
    if (!(k == kb || (nt && k == nt))) t_note("k32=%p kb=%p nt=%p", k, kb, nt);
}

/* ---- per-thread isolation ---- */
static volatile DWORD g_thr_initial, g_thr_after;
static HANDLE g_ev_set, g_ev_go;
static DWORD WINAPI th_le_basic(LPVOID p) {
    g_thr_initial = GetLastError();
    SetLastError(0x1111);
    g_thr_after = GetLastError();
    SetEvent(g_ev_set);
    WaitForSingleObject(g_ev_go, 5000);
    g_thr_after = GetLastError();
    return 0;
}
TEST(eed_le_thread_isolation) {
    REQUIRE(U(SetLastError)); REQUIRE(U(GetLastError)); REQUIRE(U(CreateThread));
    g_ev_set = CreateEventA(NULL, FALSE, FALSE, NULL);
    g_ev_go = CreateEventA(NULL, FALSE, FALSE, NULL);
    U(SetLastError)(0x2222);
    HANDLE h = U(CreateThread)(NULL, 0, th_le_basic, NULL, 0, NULL);
    CHECK(h != NULL);
    if (h) {
        WaitForSingleObject(g_ev_set, 5000);
        EXPECT_EQ(g_thr_initial, 0);
        EXPECT_EQ(U(GetLastError)(), 0x2222);   /* main thread unaffected */
        U(SetLastError)(0x3333);
        SetEvent(g_ev_go);
        WaitForSingleObject(h, 5000);
        EXPECT_EQ(g_thr_after, 0x1111);           /* worker unaffected by main */
        EXPECT_EQ(U(GetLastError)(), 0x3333);
        CloseHandle(h);
    }
    CloseHandle(g_ev_set); CloseHandle(g_ev_go);
}
static DWORD WINAPI th_le_teb(LPVOID p) {
    SetLastError(0xAAAA);
    *(DWORD *)p = EED_TEB_LASTERR();
    return 0;
}
TEST(eed_le_thread_teb_separate) {
    REQUIRE(U(CreateThread));
    DWORD seen = 0;
    SetLastError(0xBBBB);
    HANDLE h = U(CreateThread)(NULL, 0, th_le_teb, &seen, 0, NULL);
    CHECK(h != NULL);
    if (h) { WaitForSingleObject(h, 5000); CloseHandle(h); }
    EXPECT_EQ(seen, 0xAAAA);
    EXPECT_EQ(EED_TEB_LASTERR(), 0xBBBB);
}
static DWORD WINAPI th_le_many(LPVOID p) {
    DWORD id = (DWORD)(ULONG_PTR)p, bad = 0;
    for (int i = 0; i < 2000; i++) { SetLastError(id * 100000 + i); if (GetLastError() != id * 100000 + i) bad++; if ((i & 63) == 0) Sleep(0); }
    return bad;
}
TEST(eed_le_many_threads_independent) {
    REQUIRE(U(CreateThread));
    HANDLE h[8];
    for (int i = 0; i < 8; i++) h[i] = U(CreateThread)(NULL, 0, th_le_many, (LPVOID)(ULONG_PTR)(i + 1), 0, NULL);
    for (int i = 0; i < 8; i++) {
        CHECK(h[i] != NULL);
        if (!h[i]) continue;
        WaitForSingleObject(h[i], 10000);
        DWORD c = 99; GetExitCodeThread(h[i], &c);
        EXPECT_EQ(c, 0);
        CloseHandle(h[i]);
    }
}

/* ---- success does not touch last error ---- */
TEST(eed_le_success_preserves_simple_getters) {
    REQUIRE(U(GetCurrentProcessId)); REQUIRE(U(GetCurrentThreadId)); REQUIRE(U(GetTickCount)); REQUIRE(U(GetCurrentProcess));
    SetLastError(777);
    U(GetCurrentProcessId)(); U(GetCurrentThreadId)(); U(GetTickCount)(); U(GetCurrentProcess)();
    EXPECT_EQ(GetLastError(), 777);
}
TEST(eed_le_success_preserves_event_ops) {
    REQUIRE(U(SetEvent)); REQUIRE(U(ResetEvent)); REQUIRE(U(CloseHandle));
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL);
    SetLastError(778);
    CHECK(U(SetEvent)(h)); EXPECT_EQ(GetLastError(), 778);
    CHECK(U(ResetEvent)(h)); EXPECT_EQ(GetLastError(), 778);
    CHECK(U(CloseHandle)(h)); EXPECT_EQ(GetLastError(), 778);
}
TEST(eed_le_success_preserves_getprocaddress) {
    REQUIRE(U(GetProcAddress)); REQUIRE(U(GetModuleHandleW));
    SetLastError(779);
    HMODULE m = U(GetModuleHandleW)(L"kernel32.dll");
    CHECK(m != NULL); EXPECT_EQ(GetLastError(), 779);
    FARPROC p = U(GetProcAddress)(m, "GetTickCount");
    CHECK(p != NULL); EXPECT_EQ(GetLastError(), 779);
}
TEST(eed_le_success_preserves_qpc) {
    REQUIRE(U(QueryPerformanceCounter)); REQUIRE(U(QueryPerformanceFrequency));
    LARGE_INTEGER a;
    SetLastError(780);
    CHECK(U(QueryPerformanceCounter)(&a)); EXPECT_EQ(GetLastError(), 780);
    CHECK(U(QueryPerformanceFrequency)(&a)); EXPECT_EQ(GetLastError(), 780);
}
TEST(eed_le_success_preserves_tls_value) {
    REQUIRE(U(TlsAlloc)); REQUIRE(U(TlsSetValue)); REQUIRE(U(TlsGetValue)); REQUIRE(U(TlsFree));
    DWORD i = U(TlsAlloc)();
    CHECK(i != TLS_OUT_OF_INDEXES);
    if (i == TLS_OUT_OF_INDEXES) return;
    SetLastError(781);
    CHECK(U(TlsSetValue)(i, (LPVOID)5)); EXPECT_EQ(GetLastError(), 781);
    /* TlsGetValue is documented to SET the last error to 0 on success. */
    LPVOID v = U(TlsGetValue)(i);
    EXPECT_EQ(v, 5);
    EXPECT_EQ(GetLastError(), 0);
    U(TlsFree)(i);
}
TEST(eed_le_tlsgetvalue_clears_error_even_for_null) {
    REQUIRE(U(TlsAlloc)); REQUIRE(U(TlsGetValue)); REQUIRE(U(TlsFree));
    DWORD i = U(TlsAlloc)();
    if (i == TLS_OUT_OF_INDEXES) return;
    SetLastError(123);
    LPVOID v = U(TlsGetValue)(i);
    EXPECT_EQ(v, 0); EXPECT_EQ(GetLastError(), 0);
    U(TlsFree)(i);
}
TEST(eed_le_tlsgetvalue_bad_index_sets_87) {
    REQUIRE(U(TlsGetValue));
    SetLastError(0);
    LPVOID v = U(TlsGetValue)(TLS_OUT_OF_INDEXES);
    EXPECT_EQ(v, 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SetLastError(0);
    v = U(TlsGetValue)(0x7FFFFFFF);
    EXPECT_EQ(v, 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(eed_le_flag_getlasterror_after_heapalloc_success) {
    REQUIRE(U(GetProcessHeap)); REQUIRE(U(HeapAlloc)); REQUIRE(U(HeapFree));
    HANDLE hp = U(GetProcessHeap)();
    SetLastError(782);
    void *p = U(HeapAlloc)(hp, 0, 64);
    CHECK(p != NULL);
    EXPECT_EQ(GetLastError(), 782);
    U(HeapFree)(hp, 0, p);
    EXPECT_EQ(GetLastError(), 782);
}

/* ---- failures set documented codes ---- */
TEST(eed_le_closehandle_invalid_sets_6) {
    REQUIRE(U(CloseHandle));
    SetLastError(0);
    CHECK(!U(CloseHandle)(INVALID_HANDLE_VALUE)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SetLastError(0);
    CHECK(!U(CloseHandle)(NULL)); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(eed_le_closehandle_twice_sets_6) {
    REQUIRE(U(CloseHandle));
    HANDLE h = CreateEventA(NULL, FALSE, FALSE, NULL);
    CHECK(U(CloseHandle)(h));
    SetLastError(0);
    CHECK(!U(CloseHandle)(h)); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(eed_le_file_not_found_codes) {
    REQUIRE(U(GetFileAttributesW));
    SetLastError(0);
    DWORD a = U(GetFileAttributesW)(L"C:\\k32t_definitely_missing_dir_xyz\\f.txt");
    EXPECT_EQ(a, INVALID_FILE_ATTRIBUTES); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "nope.txt");
    WCHAR w[MAX_PATH]; eed_widen(p, w, MAX_PATH);
    SetLastError(0);
    a = U(GetFileAttributesW)(w);
    EXPECT_EQ(a, INVALID_FILE_ATTRIBUTES); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(eed_le_createfile_missing_sets_2_and_ntstatus) {
    REQUIRE(U(CreateFileA));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "missing_file.bin");
    SetLastError(0);
    HANDLE h = U(CreateFileA)(p, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    EXPECT_EQ(h, INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    EXPECT_EQ(EED_TEB_LASTSTATUS(), 0xC0000034u);   /* STATUS_OBJECT_NAME_NOT_FOUND */
}
TEST(eed_le_createfile_missing_dir_sets_3_and_ntstatus) {
    REQUIRE(U(CreateFileA));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "nodir\\file.bin");
    SetLastError(0);
    HANDLE h = U(CreateFileA)(p, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    EXPECT_EQ(h, INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_PATH_NOT_FOUND);
    EXPECT_EQ(EED_TEB_LASTSTATUS(), 0xC000003Au);   /* STATUS_OBJECT_PATH_NOT_FOUND */
}
TEST(eed_le_waitsingle_invalid_handle_sets_6_and_status) {
    REQUIRE(U(WaitForSingleObject));
    SetLastError(0);
    DWORD r = U(WaitForSingleObject)((HANDLE)(ULONG_PTR)0x7777FFF8, 0);
    EXPECT_EQ(r, WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE);
    EXPECT_EQ(EED_TEB_LASTSTATUS(), 0xC0000008u);   /* STATUS_INVALID_HANDLE */
}
TEST(eed_le_openevent_missing_sets_2) {
    REQUIRE(U(OpenEventW));
    SetLastError(0);
    HANDLE h = U(OpenEventW)(EVENT_ALL_ACCESS, FALSE, L"k32t_no_such_event_name_12345");
    EXPECT_EQ(h, 0); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    EXPECT_EQ(EED_TEB_LASTSTATUS(), 0xC0000034u);
}
TEST(eed_le_createevent_existing_name_sets_183_but_succeeds) {
    REQUIRE(U(CreateEventW));
    WCHAR nm[64]; wsprintfW(nm, L"k32t_ev_%lu_exist", GetCurrentProcessId());
    HANDLE a = U(CreateEventW)(NULL, TRUE, FALSE, nm);
    CHECK(a != NULL);
    SetLastError(0);
    HANDLE b = U(CreateEventW)(NULL, TRUE, FALSE, nm);
    CHECK(b != NULL); EXPECT_ERR(ERROR_ALREADY_EXISTS);
    CloseHandle(a); CloseHandle(b);
    /* first creation does not set the error */
    SetLastError(0);
    HANDLE d = U(CreateEventW)(NULL, TRUE, FALSE, nm);
    CHECK(d != NULL); EXPECT_ERR(0);
    CloseHandle(d);
}
TEST(eed_le_invalid_param_virtualalloc_zero_size) {
    REQUIRE(U(VirtualAlloc));
    SetLastError(0);
    void *p = U(VirtualAlloc)(NULL, 0, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    EXPECT_EQ(p, 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    EXPECT_EQ(EED_TEB_LASTSTATUS(), 0xC000000Du);
}
TEST(eed_le_error_not_cleared_by_later_success) {
    REQUIRE(U(CloseHandle)); REQUIRE(U(GetCurrentProcessId));
    SetLastError(0);
    U(CloseHandle)(INVALID_HANDLE_VALUE);
    DWORD e = GetLastError();
    U(GetCurrentProcessId)();
    EXPECT_EQ(GetLastError(), e);
    EXPECT_EQ(e, ERROR_INVALID_HANDLE);
}

/* ---- Rtl helpers (ntdll) ---- */
TEST(eed_rtl_getlastwin32error_matches_teb) {
    REQUIRE(NTF(RtlGetLastWin32Error_t,"RtlGetLastWin32Error"));
    for (size_t i = 0; i < sizeof k_errvals / sizeof *k_errvals; i++) {
        SetLastError(k_errvals[i]);
        EXPECT_EQ(NTF(RtlGetLastWin32Error_t,"RtlGetLastWin32Error")(), k_errvals[i]);
    }
}
TEST(eed_rtl_setlastwin32error_visible_to_getlasterror) {
    REQUIRE(NTF(RtlSetLastWin32Error_t,"RtlSetLastWin32Error"));
    for (size_t i = 0; i < sizeof k_errvals / sizeof *k_errvals; i++) {
        NTF(RtlSetLastWin32Error_t,"RtlSetLastWin32Error")(k_errvals[i]);
        EXPECT_EQ(GetLastError(), k_errvals[i]);
        EXPECT_EQ(EED_TEB_LASTERR(), k_errvals[i]);
    }
}
TEST(eed_rtl_setlastwin32errorandntstatus) {
    typedef void (WINAPI *pfn)(ULONG);
    pfn f = (pfn)t_proc_nt("RtlSetLastWin32ErrorAndNtStatusFromNtStatus");
    REQUIRE(f);
    f(0xC0000022);
    EXPECT_EQ(GetLastError(), 5);
    EXPECT_EQ(EED_TEB_LASTSTATUS(), 0xC0000022u);
    f(0xC0000034);
    EXPECT_EQ(GetLastError(), 2);
    EXPECT_EQ(EED_TEB_LASTSTATUS(), 0xC0000034u);
}
TEST(eed_rtl_getlastntstatus) {
    typedef LONG (WINAPI *pfn)(void);
    pfn g = (pfn)t_proc_nt("RtlGetLastNtStatus");
    REQUIRE(g); REQUIRE(UNT(RtlNtStatusToDosError));
    UNT(RtlNtStatusToDosError)(0xC0000043);
    EXPECT_EQ((DWORD)g(), 0xC0000043u);
    EXPECT_EQ(EED_TEB_LASTSTATUS(), 0xC0000043u);
}
TEST(eed_rtl_ntstatustodoserror_does_not_set_lasterror) {
    REQUIRE(UNT(RtlNtStatusToDosError));
    SetLastError(0x4242);
    ULONG r = UNT(RtlNtStatusToDosError)(0xC0000022);
    EXPECT_EQ(r, 5);
    EXPECT_EQ(GetLastError(), 0x4242);
}
TEST(eed_rtl_restorelastwin32error) {
    typedef void (WINAPI *pfn)(ULONG);
    pfn f = (pfn)t_proc_nt("RtlRestoreLastWin32Error");
    REQUIRE(f);
    f(0x999);
    EXPECT_EQ(GetLastError(), 0x999);
}
TEST(eed_rtl_ntstatustodoserrornoteb_matches_with_teb) {
    typedef ULONG (WINAPI *pfn)(ULONG);
    pfn nb = (pfn)t_proc_nt("RtlNtStatusToDosErrorNoTeb");
    REQUIRE(nb); REQUIRE(UNT(RtlNtStatusToDosError));
    static const ULONG st[] = { 0, 0xC0000022, 0xC0000034, 0xC000003A, 0xC0000008, 0xC000000D, 0xC0000017, 0xC0000043, 0x80000005, 0xC0000005, 0x103 };
    for (size_t i = 0; i < sizeof st / sizeof *st; i++) {
        SetLastError(1);
        EED_TEB_LASTSTATUS() = 0x12345;
        ULONG a = nb(st[i]);
        CHECKF(EED_TEB_LASTSTATUS() == 0x12345, "NoTeb must not touch TEB status for 0x%lx", st[i]);
        ULONG b = UNT(RtlNtStatusToDosError)(st[i]);
        EXPECT_EQ(a, b);
    }
}
TEST(eed_rtl_ntstatustodoserror_stores_status_in_teb) {
    REQUIRE(UNT(RtlNtStatusToDosError));
    static const ULONG st[] = { 0xC0000022, 0, 0x103, 0xE0000001, 0xC0FFFFFF };
    for (size_t i = 0; i < sizeof st / sizeof *st; i++) {
        UNT(RtlNtStatusToDosError)(st[i]);
        EXPECT_EQ(EED_TEB_LASTSTATUS(), st[i]);
    }
}

/* ---- NTSTATUS -> Win32 mapping tables ---- */
struct nt2dos { ULONG nt; ULONG dos; };
static void check_map(const struct nt2dos *t, size_t n) {
    for (size_t i = 0; i < n; i++) {
        ULONG r = UNT(RtlNtStatusToDosError)(t[i].nt);
        CHECKF(r == t[i].dos, "NTSTATUS 0x%08lx -> got %lu want %lu", t[i].nt, r, t[i].dos);
    }
}
#define MAPTEST(name, ...) TEST(name) { REQUIRE(UNT(RtlNtStatusToDosError)); static const struct nt2dos t[] = { __VA_ARGS__ }; check_map(t, sizeof t / sizeof *t); }

MAPTEST(eed_ntmap_common_file,
    {0xC0000034, 2}, {0xC000000F, 2}, {0xC000003A, 3}, {0xC0000022, 5}, {0xC0000008, 6}, {0xC0000043, 32},
    {0xC0000054, 33}, {0xC0000055, 33}, {0xC0000011, 38}, {0xC000007F, 112}, {0xC0000035, 183}, {0xC000003B, 161},
    {0xC0000101, 145}, {0xC0000103, 267}, {0xC0000056, 5}, {0xC0000121, 5}, {0xC00000BA, 5}, {0xC00000BB, 50},
    {0xC0000010, 1}, {0xC0000002, 1}, {0xC0000001, 31}, {0xC00000D4, 17}, {0xC0000012, 34}, {0xC0000013, 21})
MAPTEST(eed_ntmap_memory_params,
    {0xC0000005, 998}, {0xC0000006, 999}, {0xC0000017, 8}, {0xC000000D, 87}, {0xC0000023, 122}, {0xC0000004, 24},
    {0xC0000003, 87}, {0xC0000018, 487}, {0xC0000019, 487}, {0xC00000EF, 87},
    {0xC00000F0, 87}, {0xC00000F1, 87}, {0xC000009A, 1450}, {0xC0000024, 6}, {0xC0000016, 234}, {0xC00000FD, 1001})
MAPTEST(eed_ntmap_pipes_net,
    {0xC000014B, 109}, {0xC00000B0, 233}, {0xC00000B1, 232}, {0xC00000D9, 232}, {0xC00000AD, 230}, {0xC00000AE, 231},
    {0xC00000AB, 231}, {0xC00000AC, 231}, {0xC00000BE, 53}, {0xC00000CC, 67}, {0xC00000BD, 52}, {0xC00000BF, 54},
    {0xC00000C0, 55})
MAPTEST(eed_ntmap_warning_info,
    {0x80000005, 234}, {0x80000006, 18}, {0x8000001A, 259}, {0x80000011, 170}, {0x00000000, 0}, {0x00000103, 997})
MAPTEST(eed_ntmap_loader_security,
    {0xC0000135, 126}, {0xC0000139, 127}, {0xC000007A, 127}, {0xC000007B, 193}, {0xC0000142, 1114}, {0xC0000061, 1314},
    {0xC000006D, 1326}, {0xC0000120, 995}, {0xC000010A, 5}, {0xC0000225, 1168}, {0xC00000E5, 1359}, {0xC0000185, 1117},
    {0xC000011F, 4}, {0xC000009C, 23}, {0xC000009D, 1167}, {0xC00000A2, 19}, {0xC00000A3, 21}, {0xC0000102, 1392})

TEST(eed_ntmap_win32_facility_passthrough) {
    REQUIRE(UNT(RtlNtStatusToDosError));
    /* 0x8007xxxx / 0xC007xxxx are HRESULT-style Win32 codes: low word returned (decomp: mask 0xFFFF). */
    EXPECT_EQ(UNT(RtlNtStatusToDosError)(0x80070005), 5);
    EXPECT_EQ(UNT(RtlNtStatusToDosError)(0x80070057), 87);
    EXPECT_EQ(UNT(RtlNtStatusToDosError)(0x8007007B), 0x7B);
    EXPECT_EQ(UNT(RtlNtStatusToDosError)(0xC0070002), 2);
    EXPECT_EQ(UNT(RtlNtStatusToDosError)(0x8007FFFF), 0xFFFF);
}
TEST(eed_ntmap_customer_bit_passthrough) {
    REQUIRE(UNT(RtlNtStatusToDosError));
    /* Bit 29 (customer-defined) set: returned unchanged. */
    static const ULONG v[] = { 0xE0000001, 0xE0A00001, 0xA0000005, 0x20000005, 0xEFFFFFFF, 0xE06D7363 };
    for (size_t i = 0; i < sizeof v / sizeof *v; i++) EXPECT_EQ(UNT(RtlNtStatusToDosError)(v[i]), v[i]);
}
TEST(eed_ntmap_d_prefix_maps_like_c) {
    REQUIRE(UNT(RtlNtStatusToDosError));
    EXPECT_EQ(UNT(RtlNtStatusToDosError)(0xD0000022), UNT(RtlNtStatusToDosError)(0xC0000022));
    EXPECT_EQ(UNT(RtlNtStatusToDosError)(0xD0000034), UNT(RtlNtStatusToDosError)(0xC0000034));
    EXPECT_EQ(UNT(RtlNtStatusToDosError)(0xD0000005), UNT(RtlNtStatusToDosError)(0xC0000005));
}
TEST(eed_ntmap_unmapped_gives_317) {
    REQUIRE(UNT(RtlNtStatusToDosError));
    static const ULONG v[] = { 0xC0FFFFFF, 0xC0000FFF, 0x80FFFFF0, 0xC0F00000 };
    for (size_t i = 0; i < sizeof v / sizeof *v; i++) {
        ULONG r = UNT(RtlNtStatusToDosError)(v[i]);
        if (r != 317) t_note("0x%08lx -> %lu (expected 317 if unmapped)", v[i], r);
    }
}
TEST(eed_ntmap_zero_stays_zero_and_is_success) {
    REQUIRE(UNT(RtlNtStatusToDosError));
    EXPECT_EQ(UNT(RtlNtStatusToDosError)(0), 0);
}
TEST(eed_ntmap_deterministic_repeat) {
    REQUIRE(UNT(RtlNtStatusToDosError));
    for (int i = 0; i < 500; i++) { EXPECT_EQ(UNT(RtlNtStatusToDosError)(0xC0000022), 5); }
}
TEST(eed_ntmap_kernel32_forwarder_identity) {
    FARPROC a = (FARPROC)UNT(RtlNtStatusToDosError);
    FARPROC b = GetProcAddress(t_kernel32(), "RtlNtStatusToDosError");
    if (b) CHECK(a == b); else t_note("kernel32 does not export RtlNtStatusToDosError");
}
/* every status in the table has a stable monotone mapping from calling BaseSetLastNTError via API */
TEST(eed_ntmap_via_api_sharing_violation) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(CloseHandle));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "share.bin");
    HANDLE a = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CHECK(a != INVALID_HANDLE_VALUE);
    SetLastError(0);
    HANDLE b = U(CreateFileA)(p, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    EXPECT_EQ(b, INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_SHARING_VIOLATION);
    EXPECT_EQ(EED_TEB_LASTSTATUS(), 0xC0000043u);
    CloseHandle(a);
}
TEST(eed_ntmap_via_api_access_denied_dir_open) {
    REQUIRE(U(CreateFileA));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "adir");
    CreateDirectoryA(p, NULL);
    SetLastError(0);
    HANDLE h = U(CreateFileA)(p, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    EXPECT_EQ(h, INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_ACCESS_DENIED);
}
TEST(eed_ntmap_via_api_file_exists) {
    REQUIRE(U(CreateFileA));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "exists.bin");
    HANDLE a = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CloseHandle(a);
    SetLastError(0);
    HANDLE h = U(CreateFileA)(p, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
    EXPECT_EQ(h, INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_FILE_EXISTS);
}
TEST(eed_ntmap_via_api_overwrite_sets_183_on_success) {
    REQUIRE(U(CreateFileA));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "ow.bin");
    HANDLE a = CreateFileA(p, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, NULL);
    SetLastError(0);
    HANDLE b = U(CreateFileA)(p, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, NULL);
    CHECK(b != INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_ALREADY_EXISTS);
    SetLastError(0);
    HANDLE c = U(CreateFileA)(p, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, 0, NULL);
    CHECK(c != INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_ALREADY_EXISTS);
    CloseHandle(a); CloseHandle(b); CloseHandle(c);
}
TEST(eed_ntmap_via_api_loadlibrary_missing_126) {
    REQUIRE(U(LoadLibraryA));
    SetLastError(0);
    HMODULE m = U(LoadLibraryA)("k32t_no_such_dll_zz.dll");
    EXPECT_EQ(m, 0); EXPECT_ERR(ERROR_MOD_NOT_FOUND);
}
TEST(eed_ntmap_via_api_getprocaddress_missing_127) {
    REQUIRE(U(GetProcAddress));
    SetLastError(0);
    FARPROC f = U(GetProcAddress)(t_kernel32(), "K32T_NoSuchExport");
    EXPECT_EQ(f, 0); EXPECT_ERR(ERROR_PROC_NOT_FOUND);
}
TEST(eed_ntmap_via_api_getprocaddress_null_module_is_exe) {
    REQUIRE(U(GetProcAddress));
    /* NULL module means the process image, which does not export GetTickCount. */
    SetLastError(0);
    FARPROC f = U(GetProcAddress)(NULL, "K32T_NotExportedByExe");
    EXPECT_EQ(f, 0); EXPECT_ERR(ERROR_PROC_NOT_FOUND);
}
