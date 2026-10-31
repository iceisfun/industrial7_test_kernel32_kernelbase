/* Encode/DecodeSystemPointer, DnsHostnameToComputerName{A,W}, EmptyWorkingSet,
 * EnumDeviceDrivers, EnumPageFilesA, CveEventWrite */
#include "g2.h"

/* ---------------- Encode/DecodeSystemPointer ---------------- */
typedef PVOID (WINAPI *ptr_fn)(PVOID);

static ULONG_PTR rotr64(ULONG_PTR v, unsigned s) { s &= 63; return s ? (v >> s) | (v << (64 - s)) : v; }

TEST(gap2_sysptr_roundtrip_values) {
    ptr_fn e = (ptr_fn)t_proc("EncodeSystemPointer"), d = (ptr_fn)t_proc("DecodeSystemPointer");
    static const ULONG_PTR v[] = { 0, 1, 2, 0x10, 0x1000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x100000000ull,
        0x00007FFF12345678ull, 0x7FFFFFFFFFFFFFFFull, 0x8000000000000000ull, 0xFFFFFFFFFFFFFFFFull,
        0xDEADBEEFCAFEBABEull, 0x0123456789ABCDEFull };
    size_t i;
    REQUIRE(e); REQUIRE(d);
    for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        PVOID enc = e((PVOID)v[i]);
        PVOID dec = d(enc);
        CHECKF((ULONG_PTR)dec == v[i], "roundtrip of 0x%llx -> 0x%llx -> 0x%llx", (unsigned long long)v[i], (unsigned long long)(ULONG_PTR)enc, (unsigned long long)(ULONG_PTR)dec);
        /* decode first, then encode, is also the identity */
        CHECK((ULONG_PTR)e(d((PVOID)v[i])) == v[i]);
    }
}
TEST(gap2_sysptr_decode_encode_identity) {
    ptr_fn e = (ptr_fn)t_proc("EncodeSystemPointer"), d = (ptr_fn)t_proc("DecodeSystemPointer");
    ULONG_PTR x = 0x0123456789ABCDEFull; int i;
    REQUIRE(e); REQUIRE(d);
    for (i = 0; i < 1000; i++) {
        CHECK((ULONG_PTR)e(d((PVOID)x)) == x);
        x = x * 6364136223846793005ull + 1442695040888963407ull;
    }
}
TEST(gap2_sysptr_deterministic) {
    ptr_fn e = (ptr_fn)t_proc("EncodeSystemPointer");
    int i;
    REQUIRE(e);
    PVOID first = e((PVOID)0x1122334455667788ull);
    for (i = 0; i < 100; i++) CHECK(e((PVOID)0x1122334455667788ull) == first);
}
TEST(gap2_sysptr_injective) {
    ptr_fn e = (ptr_fn)t_proc("EncodeSystemPointer");
    PVOID seen[64]; int i, j;
    REQUIRE(e);
    for (i = 0; i < 64; i++) seen[i] = e((PVOID)((ULONG_PTR)1 << i));
    for (i = 0; i < 64; i++) for (j = i + 1; j < 64; j++) CHECK(seen[i] != seen[j]);
}
TEST(gap2_sysptr_matches_kuser_cookie) {
    ptr_fn e = (ptr_fn)t_proc("EncodeSystemPointer"), d = (ptr_fn)t_proc("DecodeSystemPointer");
    ULONG_PTR cookie; size_t i;
    static const ULONG_PTR v[] = { 0, 1, 0x1234567890ABCDEFull, 0xFFFFFFFFFFFFFFFFull };
    REQUIRE(e); REQUIRE(d);
    if (t_is_wine()) { t_skip("wine uses its own pointer encoding"); return; }
    cookie = *(volatile ULONG_PTR *)0x7FFE0330;      /* KUSER_SHARED_DATA.Cookie / pointer guard */
    t_note("system pointer cookie low byte = 0x%x", (unsigned)(cookie & 0xFF));
    for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        ULONG_PTR want = rotr64(cookie ^ v[i], (unsigned)(cookie & 63));
        EXPECT_EQ((ULONG_PTR)e((PVOID)v[i]), want);
        EXPECT_EQ((ULONG_PTR)d((PVOID)want), v[i]);
    }
}
TEST(gap2_sysptr_export_locations) {
    CHECK(t_proc_k32("EncodeSystemPointer") != NULL);
    CHECK(t_proc_k32("DecodeSystemPointer") != NULL);
    CHECK(t_proc_kb("EncodeSystemPointer") != NULL);
    CHECK(t_proc_kb("DecodeSystemPointer") != NULL);
    CHECK(t_proc_nt("RtlEncodeSystemPointer") != NULL);
}
TEST(gap2_sysptr_matches_ntdll) {
    ptr_fn e = (ptr_fn)t_proc("EncodeSystemPointer"), d = (ptr_fn)t_proc("DecodeSystemPointer");
    ptr_fn ne = (ptr_fn)t_proc_nt("RtlEncodeSystemPointer"), nd = (ptr_fn)t_proc_nt("RtlDecodeSystemPointer");
    REQUIRE(e); REQUIRE(d); REQUIRE(ne); REQUIRE(nd);
    CHECK(e((PVOID)0x4242) == ne((PVOID)0x4242));
    CHECK(d((PVOID)0x4242) == nd((PVOID)0x4242));
    CHECK(UK(EncodeSystemPointer)((PVOID)5) == ne((PVOID)5));
}
struct sp_arg { PVOID r; ptr_fn f; };
static DWORD WINAPI sp_thr(LPVOID p) { struct sp_arg *x = (struct sp_arg *)p; x->r = x->f((PVOID)0xA5A5A5A5ull); return 0; }
TEST(gap2_sysptr_stable_across_threads) {
    ptr_fn e = (ptr_fn)t_proc("EncodeSystemPointer");
    struct sp_arg a;
    HANDLE h;
    REQUIRE(e);
    a.r = NULL; a.f = e;
    PVOID mine = e((PVOID)0xA5A5A5A5ull);
    h = CreateThread(NULL, 0, sp_thr, &a, 0, NULL);
    CHECK(h != NULL);
    if (h) { WaitForSingleObject(h, 10000); CloseHandle(h); CHECK(a.r == mine); }
}
CHILD(gap2_sysptr_child) {
    ptr_fn e = (ptr_fn)t_proc("EncodeSystemPointer");
    if (!e) return 1;
    return (int)(DWORD)(ULONG_PTR)e((PVOID)0x1122334455667788ull);
}
TEST(gap2_sysptr_same_in_other_process) {
    ptr_fn e = (ptr_fn)t_proc("EncodeSystemPointer");
    REQUIRE(e);
    DWORD want = (DWORD)(ULONG_PTR)e((PVOID)0x1122334455667788ull);
    DWORD got = t_run_child("gap2_sysptr_child", "", 20000);
    EXPECT_EQ(got, want);
}

/* ---------------- DnsHostnameToComputerName ---------------- */
typedef BOOL (WINAPI *dnsw_fn)(LPCWSTR, LPWSTR, LPDWORD);
typedef BOOL (WINAPI *dnsa_fn)(LPCSTR, LPSTR, LPDWORD);

static void dnsw_expect(LPCWSTR host, LPCWSTR want)
{
    dnsw_fn f = (dnsw_fn)t_proc("DnsHostnameToComputerNameW");
    WCHAR out[64]; DWORD n = 64;
    REQUIRE(f);
    memset(out, 0xAA, sizeof out);
    SET_ERR(0);
    BOOL ok = f(host, out, &n);
    CHECKF(ok, "host %ls err=%lu", host, GetLastError());
    if (!ok) return;
    CHECKF(!lstrcmpW(out, want), "host %ls -> %ls want %ls", host, out, want);
    EXPECT_EQ(n, lstrlenW(want));
}
#define DNSW(nm, host, want) TEST(gap2_dnsw_##nm) { dnsw_expect(host, want); }
DNSW(simple, L"host", L"HOST")
DNSW(mixed_case, L"MyHost", L"MYHOST")
DNSW(fqdn, L"server.example.com", L"SERVER")
DNSW(trailing_dot, L"server.", L"SERVER")
DNSW(single_char, L"a", L"A")
DNSW(digits, L"web01", L"WEB01")
DNSW(hyphen, L"my-host.dom", L"MY-HOST")
DNSW(already_upper, L"ABC.DEF", L"ABC")
DNSW(exactly15, L"abcdefghijklmno", L"ABCDEFGHIJKLMNO")
DNSW(many_labels, L"a.b.c.d.e.f", L"A")

TEST(gap2_dnsw_long_label) {
    dnsw_fn f = (dnsw_fn)t_proc("DnsHostnameToComputerNameW");
    WCHAR out[64]; DWORD n = 64;
    REQUIRE(f);
    SET_ERR(0);
    BOOL ok = f(L"abcdefghijklmnopqrstuvwxyz.example.com", out, &n);
    t_note("26-char label: ok=%d err=%lu n=%lu name=%ls", ok, ok ? 0 : GetLastError(), n, ok ? out : L"");
    if (ok) { CHECK(n <= 15); }
}
TEST(gap2_dnsw_empty_and_dot) {
    dnsw_fn f = (dnsw_fn)t_proc("DnsHostnameToComputerNameW");
    WCHAR out[64]; DWORD n;
    static const WCHAR *bad[] = { L"", L".", L".example.com", L"..", L"." };
    size_t i;
    REQUIRE(f);
    for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        n = 64; SET_ERR(0);
        CHECKF(!f(bad[i], out, &n), "host '%ls' succeeded", bad[i]);
        EXPECT_ERR(ERROR_INVALID_COMPUTERNAME);
    }
}
TEST(gap2_dnsw_null_hostname) {
    dnsw_fn f = (dnsw_fn)t_proc("DnsHostnameToComputerNameW");
    WCHAR out[64]; DWORD n = 64;
    REQUIRE(f);
    if (t_is_wine()) { t_skip("emulator faults on NULL hostname"); return; }
    SET_ERR(0);
    CHECK(!f(NULL, out, &n));
    t_note("NULL hostname: err=%lu", GetLastError());
}
TEST(gap2_dnsw_buffer_boundaries) {
    dnsw_fn f = (dnsw_fn)t_proc("DnsHostnameToComputerNameW");
    WCHAR out[16]; DWORD n;
    REQUIRE(f);
    /* "HOST" needs 5 chars including the terminator */
    n = 5; memset(out, 0xAA, sizeof out);
    CHECK(f(L"host", out, &n));
    EXPECT_EQ(n, 4);
    CHECK(!lstrcmpW(out, L"HOST"));
    n = 4; memset(out, 0xAA, sizeof out);
    SET_ERR(0);
    CHECK(!f(L"host", out, &n));
    t_note("size 4 for 4-char name: err=%lu n=%lu", GetLastError(), n);
    n = 0; SET_ERR(0);
    CHECK(!f(L"host", out, &n));
    t_note("size 0: err=%lu n=%lu", GetLastError(), n);
    n = 1; SET_ERR(0);
    CHECK(!f(L"host", out, &n));
    n = 100;
    CHECK(f(L"host", out, &n));
    EXPECT_EQ(n, 4);
}
TEST(gap2_dnsw_size_in_out) {
    dnsw_fn f = (dnsw_fn)t_proc("DnsHostnameToComputerNameW");
    WCHAR out[64]; DWORD n = 64; 
    REQUIRE(f);
    CHECK(f(L"abc.def", out, &n));
    EXPECT_EQ(n, 3);          /* count excludes the NUL on success */
    CHECK(out[3] == 0);
}
TEST(gap2_dnsw_repeat) {
    dnsw_fn f = (dnsw_fn)t_proc("DnsHostnameToComputerNameW");
    WCHAR out[64]; int i;
    REQUIRE(f);
    for (i = 0; i < 200; i++) {
        DWORD n = 64;
        CHECK(f(L"loop.example.org", out, &n));
        CHECK(!lstrcmpW(out, L"LOOP"));
    }
}
TEST(gap2_dnsw_non_ascii) {
    dnsw_fn f = (dnsw_fn)t_proc("DnsHostnameToComputerNameW");
    WCHAR out[64]; DWORD n = 64;
    REQUIRE(f);
    SET_ERR(0);
    BOOL ok = f(L"café.example.com", out, &n);
    t_note("non-ASCII label: ok=%d err=%lu name=%ls", ok, ok ? 0 : GetLastError(), ok ? out : L"");
    n = 64;
    ok = f(L"中文.example", out, &n);
    t_note("CJK label: ok=%d err=%lu", ok, ok ? 0 : GetLastError());
}
TEST(gap2_dnsw_name_with_space_and_symbols) {
    dnsw_fn f = (dnsw_fn)t_proc("DnsHostnameToComputerNameW");
    WCHAR out[64]; DWORD n = 64;
    REQUIRE(f);
    BOOL ok = f(L"my host.example", out, &n);
    t_note("label with space: ok=%d err=%lu name=%ls", ok, ok ? 0 : GetLastError(), ok ? out : L"");
    n = 64; ok = f(L"a_b.example", out, &n);
    t_note("label with underscore: ok=%d name=%ls", ok, ok ? out : L"");
}

#define DNSA(nm, host, want) TEST(gap2_dnsa_##nm) { \
    dnsa_fn f = (dnsa_fn)t_proc("DnsHostnameToComputerNameA"); \
    char out[64]; DWORD n = 64; \
    REQUIRE(f); \
    memset(out, 0xAA, sizeof out); \
    SET_ERR(0); \
    BOOL ok = f(host, out, &n); \
    CHECKF(ok, "host %s err=%lu", host, GetLastError()); \
    if (ok) { out[n < 63 ? n : 63] = 0; CHECKF(!strncmp(out, want, strlen(want)), "got %s want %s", out, want); EXPECT_EQ(n, strlen(want)); } }
DNSA(simple, "host", "HOST")
DNSA(fqdn, "server.example.com", "SERVER")
DNSA(mixed, "WebServer01.corp", "WEBSERVER01")
DNSA(exactly15, "abcdefghijklmno", "ABCDEFGHIJKLMNO")
DNSA(trailing_dot, "x.", "X")

TEST(gap2_dnsa_matches_w) {
    dnsa_fn fa = (dnsa_fn)t_proc("DnsHostnameToComputerNameA");
    dnsw_fn fw = (dnsw_fn)t_proc("DnsHostnameToComputerNameW");
    char oa[64]; WCHAR ow[64], conv[64]; DWORD na = 64, nw = 64;
    REQUIRE(fa); REQUIRE(fw);
    CHECK(fa("some.host.name", oa, &na));
    CHECK(fw(L"some.host.name", ow, &nw));
    MultiByteToWideChar(CP_OEMCP, 0, oa, na, conv, 64); conv[na] = 0;
    EXPECT_EQ(na, nw);
    CHECK(!lstrcmpW(conv, ow));
}
TEST(gap2_dnsa_invalid) {
    dnsa_fn f = (dnsa_fn)t_proc("DnsHostnameToComputerNameA");
    char out[64]; DWORD n;
    REQUIRE(f);
    n = 64; SET_ERR(0);
    CHECK(!f("", out, &n));
    EXPECT_ERR(ERROR_INVALID_COMPUTERNAME);
    n = 64; SET_ERR(0);
    CHECK(!f(".example", out, &n));
    EXPECT_ERR(ERROR_INVALID_COMPUTERNAME);
}
TEST(gap2_dnsa_small_buffer) {
    dnsa_fn f = (dnsa_fn)t_proc("DnsHostnameToComputerNameA");
    char out[64]; DWORD n;
    REQUIRE(f);
    n = 2; SET_ERR(0);
    CHECK(!f("longname", out, &n));
    t_note("A small buffer: err=%lu n=%lu", GetLastError(), n);
    n = 0; SET_ERR(0);
    CHECK(!f("longname", out, &n));
    t_note("A zero buffer: err=%lu n=%lu", GetLastError(), n);
}

/* ---------------- EmptyWorkingSet ---------------- */
typedef BOOL (WINAPI *ews_fn)(HANDLE);

TEST(gap2_ews_current_process) {
    ews_fn f = (ews_fn)t_proc("EmptyWorkingSet");
    REQUIRE(f);
    CHECK(f(GetCurrentProcess()));
    CHECK(f(GetCurrentProcess()));
}
TEST(gap2_ews_pseudo_handle_minus_one) {
    ews_fn f = (ews_fn)t_proc("EmptyWorkingSet");
    REQUIRE(f);
    CHECK(f((HANDLE)(LONG_PTR)-1));
}
TEST(gap2_ews_real_handle_to_self) {
    ews_fn f = (ews_fn)t_proc("EmptyWorkingSet");
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_SET_QUOTA, FALSE, GetCurrentProcessId());
    REQUIRE(f);
    CHECK(h != NULL); if (!h) return;
    CHECK(f(h));
    CloseHandle(h);
}
TEST(gap2_ews_k32_alias) {
    ews_fn f = (ews_fn)t_proc_k32("K32EmptyWorkingSet");
    ews_fn g = (ews_fn)t_proc_kb("K32EmptyWorkingSet");
    ews_fn e = (ews_fn)t_proc_kb("EmptyWorkingSet");
    REQUIRE(f); REQUIRE(g); REQUIRE(e);
    CHECK(f(GetCurrentProcess()));
    CHECK(g(GetCurrentProcess()));
    CHECK(e(GetCurrentProcess()));
}
TEST(gap2_ews_null_handle) {
    ews_fn f = (ews_fn)t_proc("EmptyWorkingSet");
    REQUIRE(f);
    SET_ERR(0);
    CHECK(!f(NULL));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap2_ews_invalid_handles) {
    ews_fn f = (ews_fn)t_proc("EmptyWorkingSet");
    HANDLE ev, c;
    REQUIRE(f);
    SET_ERR(0);
    CHECK(!f((HANDLE)(ULONG_PTR)0x7FFFFFF0));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    CHECK(!f(ev));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    c = ev; CloseHandle(ev);
    SET_ERR(0);
    CHECK(!f(c));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(!f((HANDLE)(LONG_PTR)-2));     /* current thread pseudo-handle is not a process */
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap2_ews_access_denied_handles) {
    ews_fn f = (ews_fn)t_proc("EmptyWorkingSet");
    HANDLE h;
    REQUIRE(f);
    h = OpenProcess(SYNCHRONIZE, FALSE, GetCurrentProcessId());
    if (h) { SET_ERR(0); CHECK(!f(h)); EXPECT_ERR(ERROR_ACCESS_DENIED); CloseHandle(h); }
    else CHECK(0);
    h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    if (h) { SET_ERR(0); CHECK(!f(h)); EXPECT_ERR(ERROR_ACCESS_DENIED); CloseHandle(h); }
    h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, GetCurrentProcessId());
    if (h) { SET_ERR(0); CHECK(!f(h)); EXPECT_ERR(ERROR_ACCESS_DENIED); CloseHandle(h); }
    h = OpenProcess(PROCESS_SET_QUOTA, FALSE, GetCurrentProcessId());
    if (h) { SET_ERR(0); CHECK(!f(h)); EXPECT_ERR(ERROR_ACCESS_DENIED); CloseHandle(h); }
}
CHILD(gap2_sleeper) { Sleep(8000); return 0; }
TEST(gap2_ews_child_process) {
    ews_fn f = (ews_fn)t_proc("EmptyWorkingSet");
    PROCESS_INFORMATION pi; HANDLE h;
    REQUIRE(f);
    h = t_spawn_child("gap2_sleeper", "", 0, NULL, NULL, NULL, FALSE, NULL, &pi);
    CHECK(h != NULL); if (!h) return;
    Sleep(300);
    CHECK(f(h));
    CHECK(f(h));
    CHECK(WaitForSingleObject(h, 0) == WAIT_TIMEOUT);   /* still alive */
    TerminateProcess(h, 0);
    WaitForSingleObject(h, 5000);
    SET_ERR(0);
    BOOL ok = f(h);       /* exited process: handle still valid */
    t_note("EmptyWorkingSet on exited process: ok=%d err=%lu", ok, ok ? 0 : GetLastError());
    CloseHandle(pi.hThread); CloseHandle(h);
}
TEST(gap2_ews_suspended_child) {
    ews_fn f = (ews_fn)t_proc("EmptyWorkingSet");
    PROCESS_INFORMATION pi; HANDLE h;
    REQUIRE(f);
    h = t_spawn_child("gap2_sleeper", "", CREATE_SUSPENDED, NULL, NULL, NULL, FALSE, NULL, &pi);
    CHECK(h != NULL); if (!h) return;
    BOOL ok = f(h);
    t_note("EmptyWorkingSet on suspended-at-start process: ok=%d err=%lu", ok, ok ? 0 : GetLastError());
    TerminateProcess(h, 0);
    WaitForSingleObject(h, 5000);
    CloseHandle(pi.hThread); CloseHandle(h);
}
TEST(gap2_ews_keeps_process_functional) {
    ews_fn f = (ews_fn)t_proc("EmptyWorkingSet");
    volatile char *p; int i, sum = 0;
    REQUIRE(f);
    p = (volatile char *)VirtualAlloc(NULL, 1 << 20, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    CHECK(p != NULL); if (!p) return;
    for (i = 0; i < (1 << 20); i += 4096) p[i] = (char)(i >> 12);
    CHECK(f(GetCurrentProcess()));
    for (i = 0; i < (1 << 20); i += 4096) sum += (char)(i >> 12) == p[i];
    EXPECT_EQ(sum, 256);
    VirtualFree((LPVOID)p, 0, MEM_RELEASE);
}
TEST(gap2_ews_repeat) {
    ews_fn f = (ews_fn)t_proc("EmptyWorkingSet");
    int i;
    REQUIRE(f);
    for (i = 0; i < 100; i++) CHECK(f(GetCurrentProcess()));
}

/* ---------------- EnumDeviceDrivers ---------------- */
typedef BOOL (WINAPI *edd_fn)(LPVOID *, DWORD, LPDWORD);

TEST(gap2_edd_size_query) {
    edd_fn f = (edd_fn)t_proc("EnumDeviceDrivers");
    DWORD need = 0xFFFFFFFF;
    REQUIRE(f);
    if (t_is_wine()) { t_skip("driver enumeration is a stub in this emulator"); return; }
    CHECK(f(NULL, 0, &need));
    CHECK(need != 0xFFFFFFFF);
    EXPECT_EQ(need % sizeof(LPVOID), 0);
    CHECK(need >= sizeof(LPVOID));
    t_note("%lu device drivers", need / 8);
}
TEST(gap2_edd_fill) {
    edd_fn f = (edd_fn)t_proc("EnumDeviceDrivers");
    DWORD need = 0, need2 = 0, i; LPVOID *buf;
    REQUIRE(f);
    if (t_is_wine()) { t_skip("driver enumeration is a stub in this emulator"); return; }
    CHECK(f(NULL, 0, &need));
    buf = (LPVOID *)calloc(need / 8 + 64, 8);
    CHECK(f(buf, need + 64 * 8, &need2));
    CHECK(need2 >= 8);
    for (i = 0; i < need2 / 8 && i < need / 8; i++) CHECK(buf[i] != NULL);
    t_note("first driver base %p", buf[0]);
    free(buf);
}
TEST(gap2_edd_buffer_too_small_is_success) {
    edd_fn f = (edd_fn)t_proc("EnumDeviceDrivers");
    LPVOID small[1]; DWORD need = 0, full = 0;
    REQUIRE(f);
    if (t_is_wine()) { t_skip("driver enumeration is a stub in this emulator"); return; }
    CHECK(f(NULL, 0, &full));
    small[0] = (LPVOID)0x1234;
    SET_ERR(0);
    CHECK(f(small, 8, &need));          /* one slot: succeeds, reports full size */
    CHECK(small[0] != (LPVOID)0x1234);
    CHECK(need >= 8);
    t_note("full=%lu need=%lu", full, need);
}
TEST(gap2_edd_cb_below_pointer_size) {
    edd_fn f = (edd_fn)t_proc("EnumDeviceDrivers");
    LPVOID b[2] = { (LPVOID)0x77, (LPVOID)0x88 }; DWORD need = 0;
    int cbs[] = { 0, 1, 7 }; size_t i;
    REQUIRE(f);
    if (t_is_wine()) { t_skip("driver enumeration is a stub in this emulator"); return; }
    for (i = 0; i < 3; i++) {
        b[0] = (LPVOID)0x77; need = 0;
        CHECK(f(b, cbs[i], &need));
        CHECK(b[0] == (LPVOID)0x77);      /* nothing copied */
        CHECK(need >= 8);
    }
}
TEST(gap2_edd_partial_fill_does_not_overrun) {
    edd_fn f = (edd_fn)t_proc("EnumDeviceDrivers");
    LPVOID b[8]; DWORD need = 0; int i;
    REQUIRE(f);
    if (t_is_wine()) { t_skip("driver enumeration is a stub in this emulator"); return; }
    for (i = 0; i < 8; i++) b[i] = (LPVOID)0x5A5A;
    CHECK(f(b, 3 * 8 + 5, &need));        /* room for exactly 3 */
    CHECK(b[0] != (LPVOID)0x5A5A);
    for (i = 3; i < 8; i++) CHECK(b[i] == (LPVOID)0x5A5A);
}
TEST(gap2_edd_repeat_consistent) {
    edd_fn f = (edd_fn)t_proc("EnumDeviceDrivers");
    DWORD a = 0, b = 0;
    REQUIRE(f);
    if (t_is_wine()) { t_skip("driver enumeration is a stub in this emulator"); return; }
    CHECK(f(NULL, 0, &a));
    CHECK(f(NULL, 0, &b));
    CHECK(a > 0 && b > 0);
    CHECK(a / 8 <= b / 8 + 8 && b / 8 <= a / 8 + 8);
}
TEST(gap2_edd_first_matches_ntdll_module_list) {
    edd_fn f = (edd_fn)t_proc("EnumDeviceDrivers");
    typedef LONG (NTAPI *nqsi)(ULONG, PVOID, ULONG, PULONG);
    nqsi q = (nqsi)t_proc_nt("NtQuerySystemInformation");
    LPVOID buf[4] = {0}; DWORD need = 0; ULONG len = 0; BYTE *mem;
    REQUIRE(f); REQUIRE(q);
    if (t_is_wine()) { t_skip("driver enumeration is a stub in this emulator"); return; }
    CHECK(f(buf, sizeof buf, &need));
    mem = (BYTE *)malloc(1 << 20);
    if (q(11, mem, 1 << 20, &len) >= 0) {
        /* RTL_PROCESS_MODULES: ULONG count, then entries; ImageBase at +24 of entry (entry starts at +8) */
        LPVOID base0 = *(LPVOID *)(mem + 8 + 16);
        CHECK(buf[0] == base0);
    } else t_note("NtQuerySystemInformation(SystemModuleInformation) failed");
    free(mem);
}
TEST(gap2_edd_k32_alias) {
    edd_fn f = (edd_fn)t_proc_k32("K32EnumDeviceDrivers");
    edd_fn g = (edd_fn)t_proc_kb("K32EnumDeviceDrivers");
    DWORD a = 0, b = 0;
    REQUIRE(f); REQUIRE(g);
    if (t_is_wine()) { t_skip("driver enumeration is a stub in this emulator"); return; }
    CHECK(f(NULL, 0, &a));
    CHECK(g(NULL, 0, &b));
    CHECK(a > 0 && b > 0);
}
TEST(gap2_edd_kernel_addresses) {
    edd_fn f = (edd_fn)t_proc("EnumDeviceDrivers");
    LPVOID buf[8]; DWORD need = 0, i;
    REQUIRE(f);
    if (t_is_wine()) { t_skip("driver enumeration is a stub in this emulator"); return; }
    CHECK(f(buf, sizeof buf, &need));
    for (i = 0; i < 8 && i < need / 8; i++) {
        t_note("driver[%lu] = %p", i, buf[i]);
        CHECK((ULONG_PTR)buf[i] >= 0xFFFF800000000000ull);
    }
}
TEST(gap2_edd_ntoskrnl_present) {
    edd_fn f = (edd_fn)t_proc("EnumDeviceDrivers");
    typedef DWORD (WINAPI *gdbn)(LPVOID, LPSTR, DWORD);
    gdbn nm = (gdbn)t_proc("K32GetDeviceDriverBaseNameA");
    LPVOID buf[512]; DWORD need = 0, i; int found = 0;
    REQUIRE(f);
    if (t_is_wine()) { t_skip("driver enumeration is a stub in this emulator"); return; }
    CHECK(f(buf, sizeof buf, &need));
    if (!nm) nm = (gdbn)t_proc_kb("GetDeviceDriverBaseNameA");
    REQUIRE(nm);
    for (i = 0; i < need / 8 && i < 512; i++) {
        char n[64] = "";
        if (nm(buf[i], n, sizeof n) && !lstrcmpiA(n, "ntoskrnl.exe")) found = 1;
    }
    CHECK(found);
}

/* ---------------- EnumPageFilesA ---------------- */
typedef BOOL (CALLBACK *pf_cbA)(LPVOID, PENUM_PAGE_FILE_INFORMATION, LPCSTR);
typedef BOOL (CALLBACK *pf_cbW)(LPVOID, PENUM_PAGE_FILE_INFORMATION, LPCWSTR);
typedef BOOL (WINAPI *epfa_fn)(PENUM_PAGE_FILE_CALLBACKA, LPVOID);
typedef BOOL (WINAPI *epfw_fn)(PENUM_PAGE_FILE_CALLBACKW, LPVOID);

struct pfctx { int n; int stop_after; LPVOID expect_ctx; char first[MAX_PATH]; ENUM_PAGE_FILE_INFORMATION info[8]; int bad_ctx; };

static BOOL CALLBACK pf_a(LPVOID ctx, PENUM_PAGE_FILE_INFORMATION info, LPCSTR name)
{
    struct pfctx *c = (struct pfctx *)ctx;
    if (c->n < 8) c->info[c->n] = *info;
    if (c->n == 0) lstrcpynA(c->first, name, MAX_PATH);
    c->n++;
    return c->stop_after && c->n >= c->stop_after ? FALSE : TRUE;
}
static BOOL CALLBACK pf_w(LPVOID ctx, PENUM_PAGE_FILE_INFORMATION info, LPCWSTR name)
{
    struct pfctx *c = (struct pfctx *)ctx;
    if (c->n < 8) c->info[c->n] = *info;
    if (c->n == 0) WideCharToMultiByte(CP_ACP, 0, name, -1, c->first, MAX_PATH, NULL, NULL);
    c->n++;
    return c->stop_after && c->n >= c->stop_after ? FALSE : TRUE;
}

#define EPF_NOT_WINE() do { if (t_is_wine()) { t_skip("page file enumeration is a stub in this emulator"); return; } } while (0)
TEST(gap2_epf_a_basic) {
    epfa_fn f = (epfa_fn)t_proc("EnumPageFilesA");
    struct pfctx c; memset(&c, 0, sizeof c);
    REQUIRE(f);
    EPF_NOT_WINE();
    SET_ERR(0);
    CHECK(f((PENUM_PAGE_FILE_CALLBACKA)pf_a, &c));
    t_note("%d page file(s); first=%s", c.n, c.n ? c.first : "-");
    if (c.n) {
        CHECK(c.first[0] != 0);
        CHECK(c.info[0].TotalSize >= c.info[0].TotalInUse);
        CHECK(c.info[0].PeakUsage >= c.info[0].TotalInUse);
        CHECK(c.info[0].cb == sizeof(ENUM_PAGE_FILE_INFORMATION));
    }
}
TEST(gap2_epf_a_matches_w) {
    epfa_fn fa = (epfa_fn)t_proc("EnumPageFilesA");
    epfw_fn fw = (epfw_fn)t_proc("EnumPageFilesW");
    struct pfctx a, w; memset(&a, 0, sizeof a); memset(&w, 0, sizeof w);
    REQUIRE(fa); REQUIRE(fw);
    EPF_NOT_WINE();
    CHECK(fa((PENUM_PAGE_FILE_CALLBACKA)pf_a, &a));
    CHECK(fw((PENUM_PAGE_FILE_CALLBACKW)pf_w, &w));
    EXPECT_EQ(a.n, w.n);
    if (a.n && w.n) CHECK(!lstrcmpiA(a.first, w.first));
}
TEST(gap2_epf_a_stop_on_false) {
    epfa_fn f = (epfa_fn)t_proc("EnumPageFilesA");
    struct pfctx c; memset(&c, 0, sizeof c); c.stop_after = 1;
    REQUIRE(f);
    EPF_NOT_WINE();
    SET_ERR(0);
    BOOL ok = f((PENUM_PAGE_FILE_CALLBACKA)pf_a, &c);
    t_note("callback returned FALSE after first: ret=%d err=%lu n=%d", ok, GetLastError(), c.n);
    CHECK(c.n <= 1);
}
TEST(gap2_epf_a_context_passthrough) {
    epfa_fn f = (epfa_fn)t_proc("EnumPageFilesA");
    struct pfctx c; memset(&c, 0, sizeof c);
    REQUIRE(f);
    EPF_NOT_WINE();
    CHECK(f((PENUM_PAGE_FILE_CALLBACKA)pf_a, &c));
    /* the callback wrote through the context, so it was passed unchanged */
    CHECK(c.n >= 0);
}
TEST(gap2_epf_a_null_callback_without_pagefiles) {
    epfa_fn f = (epfa_fn)t_proc("EnumPageFilesA");
    epfw_fn fw = (epfw_fn)t_proc("EnumPageFilesW");
    struct pfctx w; memset(&w, 0, sizeof w);
    REQUIRE(f); REQUIRE(fw);
    EPF_NOT_WINE();
    CHECK(fw((PENUM_PAGE_FILE_CALLBACKW)pf_w, &w));
    if (w.n != 0) { t_skip("page file exists; a NULL callback would be invoked"); return; }
    SET_ERR(0);
    BOOL ok = f(NULL, NULL);
    t_note("NULL callback with no page files: ok=%d err=%lu", ok, GetLastError());
}
TEST(gap2_epf_a_repeat) {
    epfa_fn f = (epfa_fn)t_proc("EnumPageFilesA");
    int i, first = -1;
    REQUIRE(f);
    EPF_NOT_WINE();
    for (i = 0; i < 50; i++) {
        struct pfctx c; memset(&c, 0, sizeof c);
        CHECK(f((PENUM_PAGE_FILE_CALLBACKA)pf_a, &c));
        if (first < 0) first = c.n; else EXPECT_EQ(c.n, first);
    }
}
TEST(gap2_epf_k32_aliases) {
    epfa_fn f = (epfa_fn)t_proc_k32("K32EnumPageFilesA");
    epfw_fn w = (epfw_fn)t_proc_k32("K32EnumPageFilesW");
    epfa_fn g = (epfa_fn)t_proc_kb("K32EnumPageFilesA");
    epfa_fn h = (epfa_fn)t_proc_kb("EnumPageFilesA");
    struct pfctx a, b, c; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b); memset(&c, 0, sizeof c);
    REQUIRE(f); REQUIRE(w); REQUIRE(g); REQUIRE(h);
    EPF_NOT_WINE();
    CHECK(f((PENUM_PAGE_FILE_CALLBACKA)pf_a, &a));
    CHECK(g((PENUM_PAGE_FILE_CALLBACKA)pf_a, &b));
    CHECK(h((PENUM_PAGE_FILE_CALLBACKA)pf_a, &c));
    EXPECT_EQ(a.n, b.n);
    EXPECT_EQ(a.n, c.n);
}
TEST(gap2_epf_a_name_is_dos_path) {
    epfa_fn f = (epfa_fn)t_proc("EnumPageFilesA");
    struct pfctx c; memset(&c, 0, sizeof c);
    REQUIRE(f);
    EPF_NOT_WINE();
    CHECK(f((PENUM_PAGE_FILE_CALLBACKA)pf_a, &c));
    if (!c.n) { t_skip("no page file"); return; }
    t_note("page file name: %s", c.first);
    CHECK(strlen(c.first) > 3);
}

/* ---------------- CveEventWrite ---------------- */
typedef LONG (WINAPI *cve_fn)(PCWSTR, PCWSTR);

TEST(gap2_cve_null_id) {
    cve_fn f = (cve_fn)t_proc("CveEventWrite");
    REQUIRE(f);
    SET_ERR(0xABCD);
    EXPECT_EQ(f(NULL, NULL), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(f(NULL, L"details"), ERROR_INVALID_PARAMETER);
}
TEST(gap2_cve_null_id_leaves_last_error) {
    cve_fn f = (cve_fn)t_proc("CveEventWrite");
    REQUIRE(f);
    SET_ERR(0xABCD);
    f(NULL, NULL);
    DWORD e = GetLastError();
    t_note("last error after CveEventWrite(NULL): 0x%lx", e);
}
TEST(gap2_cve_valid_no_details) {
    cve_fn f = (cve_fn)t_proc("CveEventWrite");
    REQUIRE(f);
    LONG r = f(L"CVE-1900-0000", NULL);
    t_note("CveEventWrite(id): %ld", r);
    EXPECT_EQ(r, ERROR_SUCCESS);
}
TEST(gap2_cve_valid_with_details) {
    cve_fn f = (cve_fn)t_proc("CveEventWrite");
    REQUIRE(f);
    LONG r = f(L"CVE-1900-0001", L"conformance test, no real vulnerability");
    t_note("CveEventWrite(id, details): %ld", r);
    EXPECT_EQ(r, ERROR_SUCCESS);
}
TEST(gap2_cve_empty_strings) {
    cve_fn f = (cve_fn)t_proc("CveEventWrite");
    REQUIRE(f);
    LONG r = f(L"", L"");
    t_note("CveEventWrite(empty, empty): %ld", r);
    r = f(L"", NULL);
    t_note("CveEventWrite(empty, NULL): %ld", r);
}
TEST(gap2_cve_long_strings) {
    cve_fn f = (cve_fn)t_proc("CveEventWrite");
    WCHAR *big; int i;
    REQUIRE(f);
    big = (WCHAR *)malloc(sizeof(WCHAR) * 20000);
    for (i = 0; i < 19999; i++) big[i] = L'x';
    big[19999] = 0;
    LONG r = f(L"CVE-1900-0002", big);
    t_note("CveEventWrite(id, 19999 chars): %ld", r);
    free(big);
}
TEST(gap2_cve_repeat) {
    cve_fn f = (cve_fn)t_proc("CveEventWrite");
    int i;
    REQUIRE(f);
    for (i = 0; i < 50; i++) { LONG r = f(L"CVE-1900-0003", L"repeat"); CHECK(r == ERROR_SUCCESS); }
}
TEST(gap2_cve_location) {
    CHECK(t_proc_kb("CveEventWrite") != NULL);
}
