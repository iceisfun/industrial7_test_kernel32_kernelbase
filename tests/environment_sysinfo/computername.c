/* GetComputerName{A,W}, GetComputerNameEx{A,W}, DnsHostnameToComputerNameExW */
#include "t.h"
#include "k.h"

UDECL(BOOL, DnsHostnameToComputerNameExW, (LPCWSTR, LPWSTR, LPDWORD));

#define CN_MAX 15

static int cn_get_w(COMPUTER_NAME_FORMAT f, WCHAR *buf, DWORD cap, DWORD *len) {
    DWORD n = cap; BOOL r = U(GetComputerNameExW)(f, buf, &n);
    if (len) *len = n;
    return r;
}

TEST(cname_w_basic) {
    REQUIRE(U(GetComputerNameW));
    WCHAR b[MAX_COMPUTERNAME_LENGTH + 1]; DWORD n = MAX_COMPUTERNAME_LENGTH + 1;
    memset(b, 0xCC, sizeof b);
    CHECK(U(GetComputerNameW)(b, &n));
    CHECK(n > 0 && n <= MAX_COMPUTERNAME_LENGTH);
    EXPECT_EQ(wcslen(b), n);
    EXPECT_EQ(b[n], 0);
}
TEST(cname_a_basic) {
    REQUIRE(U(GetComputerNameA));
    char b[MAX_COMPUTERNAME_LENGTH + 1]; DWORD n = MAX_COMPUTERNAME_LENGTH + 1;
    memset(b, 0xCC, sizeof b);
    CHECK(U(GetComputerNameA)(b, &n));
    CHECK(n > 0 && n <= MAX_COMPUTERNAME_LENGTH);
    EXPECT_EQ(strlen(b), n);
}
TEST(cname_max_length_constant) {
    EXPECT_EQ(MAX_COMPUTERNAME_LENGTH, 15);
}
TEST(cname_aw_parity) {
    REQUIRE(U(GetComputerNameA)); REQUIRE(U(GetComputerNameW));
    char a[32]; WCHAR w[32], conv[32]; DWORD na = 32, nw = 32;
    CHECK(U(GetComputerNameA)(a, &na));
    CHECK(U(GetComputerNameW)(w, &nw));
    EXPECT_EQ(na, nw);
    MultiByteToWideChar(CP_ACP, 0, a, -1, conv, 32);
    CHECK(!wcscmp(conv, w));
}
TEST(cname_w_size_exact_len) {
    /* nSize == length: too small (no room for the NUL); required size is returned incl. NUL, error 111 */
    REQUIRE(U(GetComputerNameW));
    WCHAR b[32]; DWORD n = 32;
    CHECK(U(GetComputerNameW)(b, &n));
    DWORD len = n;
    WCHAR small[32];
    n = len;
    SET_ERR(0);
    EXPECT_EQ(U(GetComputerNameW)(small, &n), 0);
    EXPECT_ERR(ERROR_BUFFER_OVERFLOW);
    EXPECT_EQ(n, len + 1);
}
TEST(cname_w_size_exact_len_plus_one) {
    REQUIRE(U(GetComputerNameW));
    WCHAR b[32]; DWORD n = 32;
    CHECK(U(GetComputerNameW)(b, &n));
    DWORD len = n;
    n = len + 1;
    WCHAR r[32];
    CHECK(U(GetComputerNameW)(r, &n));
    EXPECT_EQ(n, len);
    CHECK(!wcscmp(r, b));
}
TEST(cname_a_size_exact_len) {
    REQUIRE(U(GetComputerNameA));
    char b[32]; DWORD n = 32;
    CHECK(U(GetComputerNameA)(b, &n));
    DWORD len = n;
    n = len;
    SET_ERR(0);
    EXPECT_EQ(U(GetComputerNameA)(b, &n), 0);
    EXPECT_ERR(ERROR_BUFFER_OVERFLOW);
    EXPECT_EQ(n, len + 1);
}
TEST(cname_w_size_zero_with_buffer) {
    REQUIRE(U(GetComputerNameW));
    WCHAR b[4] = { 0x4141, 0x4141, 0x4141, 0x4141 }; DWORD n = 0;
    SET_ERR(0);
    EXPECT_EQ(U(GetComputerNameW)(b, &n), 0);
    EXPECT_ERR(ERROR_BUFFER_OVERFLOW);
    CHECK(n >= 2);
}
TEST(cname_w_size_one_with_buffer) {
    REQUIRE(U(GetComputerNameW));
    WCHAR b[4] = { 0x4141, 0x4141, 0x4141, 0x4141 }; DWORD n = 1;
    SET_ERR(0);
    EXPECT_EQ(U(GetComputerNameW)(b, &n), 0);
    EXPECT_ERR(ERROR_BUFFER_OVERFLOW);
    CHECK(n >= 2);
}
TEST(cname_w_required_size_stable) {
    REQUIRE(U(GetComputerNameW));
    WCHAR b[4]; DWORD n1 = 1, n2 = 2;
    U(GetComputerNameW)(b, &n1);
    U(GetComputerNameW)(b, &n2);
    EXPECT_EQ(n1, n2);
}
TEST(cname_w_large_buffer_unchanged_tail) {
    REQUIRE(U(GetComputerNameW));
    WCHAR b[64]; DWORD n = 64;
    for (int i = 0; i < 64; i++) b[i] = 0x7777;
    CHECK(U(GetComputerNameW)(b, &n));
    EXPECT_EQ(b[n + 1], 0x7777);
    EXPECT_EQ(b[63], 0x7777);
}
TEST(cname_name_has_no_forbidden_chars) {
    REQUIRE(U(GetComputerNameA));
    char b[32]; DWORD n = 32;
    CHECK(U(GetComputerNameA)(b, &n));
    for (DWORD i = 0; i < n; i++) CHECKF(!strchr("\\/:*?\"<>|,~!@#$%^&'(){}", b[i]) && (unsigned char)b[i] > 0x20, "char %02x at %lu", b[i], i);
}
TEST(cname_matches_registry_active_name) {
    REQUIRE(U(GetComputerNameA));
    HKEY k; char reg[64] = "", api[64]; DWORD n = 64, rl = sizeof reg;
    CHECK(U(GetComputerNameA)(api, &n));
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\ComputerName\\ActiveComputerName", 0, KEY_READ, &k) == 0) {
        if (RegQueryValueExA(k, "ComputerName", NULL, NULL, (BYTE *)reg, &rl) == 0) CHECK(!_stricmp(reg, api));
        else t_note("ActiveComputerName value not readable");
        RegCloseKey(k);
    } else t_note("ActiveComputerName key not readable");
}
TEST(cname_matches_env_variable_when_unclustered) {
    REQUIRE(U(GetComputerNameA)); REQUIRE(U(GetEnvironmentVariableA));
    char env[64], api[64]; DWORD n = 64;
    CHECK(U(GetComputerNameA)(api, &n));
    if (U(GetEnvironmentVariableA)("COMPUTERNAME", env, 64)) CHECK(!_stricmp(env, api));
}

/* ---------- Ex ---------- */
static const char *cn_fmt_name[] = { "NetBIOS", "DnsHostname", "DnsDomain", "DnsFullyQualified",
    "PhysicalNetBIOS", "PhysicalDnsHostname", "PhysicalDnsDomain", "PhysicalDnsFullyQualified" };

#define CN_FMT_TEST(idx) \
TEST(cnameex_format_##idx) { \
    REQUIRE(U(GetComputerNameExW)); REQUIRE(U(GetComputerNameExA)); \
    WCHAR w[512]; char a[512]; DWORD nw = 512, na = 512; \
    for (int i = 0; i < 512; i++) w[i] = 0x7777; \
    BOOL rw = U(GetComputerNameExW)(idx, w, &nw); \
    BOOL ra = U(GetComputerNameExA)(idx, a, &na); \
    CHECKF(rw, "%s W failed err %lu", cn_fmt_name[idx], GetLastError()); \
    CHECKF(ra, "%s A failed err %lu", cn_fmt_name[idx], GetLastError()); \
    if (rw) { EXPECT_EQ(wcslen(w), nw); EXPECT_EQ(w[nw], 0); } \
    if (rw && ra) { \
        EXPECT_EQ(na, nw); EXPECT_EQ(strlen(a), na); \
        WCHAR conv[512]; MultiByteToWideChar(CP_ACP, 0, a, -1, conv, 512); CHECK(!wcscmp(conv, w)); \
    } \
    if (idx == 0 || idx == 4) { CHECK(nw > 0 && nw <= CN_MAX); } \
    if (idx == 1 || idx == 5) { CHECK(nw > 0 && nw <= 63); } \
    t_note("%s = \"%ls\"", cn_fmt_name[idx], w); \
}
CN_FMT_TEST(0) CN_FMT_TEST(1) CN_FMT_TEST(2) CN_FMT_TEST(3)
CN_FMT_TEST(4) CN_FMT_TEST(5) CN_FMT_TEST(6) CN_FMT_TEST(7)

#define CN_SIZE_TEST(idx) \
TEST(cnameex_sizes_format_##idx) { \
    REQUIRE(U(GetComputerNameExW)); \
    WCHAR full[512], tmp[512]; DWORD len = 0, n; \
    if (!cn_get_w(idx, full, 512, &len)) { CHECK(0); return; } \
    /* NULL buffer, size 0: ERROR_MORE_DATA and required size incl. NUL (only when a name exists) */ \
    n = 0; SET_ERR(0); \
    BOOL r = U(GetComputerNameExW)(idx, NULL, &n); \
    if (len == 0 && r) { EXPECT_EQ(n, 0); } \
    else { EXPECT_EQ(r, 0); EXPECT_ERR(ERROR_MORE_DATA); EXPECT_EQ(n, len + 1); } \
    if (len > 0) { \
        n = len; SET_ERR(0); \
        EXPECT_EQ(U(GetComputerNameExW)(idx, tmp, &n), 0); EXPECT_ERR(ERROR_MORE_DATA); EXPECT_EQ(n, len + 1); \
        n = len + 1; \
        CHECK(U(GetComputerNameExW)(idx, tmp, &n)); EXPECT_EQ(n, len); CHECK(!wcscmp(tmp, full)); \
        n = 0; SET_ERR(0); \
        EXPECT_EQ(U(GetComputerNameExW)(idx, tmp, &n), 0); EXPECT_ERR(ERROR_MORE_DATA); EXPECT_EQ(n, len + 1); \
    } \
}
CN_SIZE_TEST(0) CN_SIZE_TEST(1) CN_SIZE_TEST(2) CN_SIZE_TEST(3)
CN_SIZE_TEST(4) CN_SIZE_TEST(5) CN_SIZE_TEST(6) CN_SIZE_TEST(7)

TEST(cnameex_a_size_semantics_netbios) {
    REQUIRE(U(GetComputerNameExA));
    char b[64]; DWORD n = 64;
    CHECK(U(GetComputerNameExA)(ComputerNameNetBIOS, b, &n));
    DWORD len = n;
    n = len; SET_ERR(0);
    EXPECT_EQ(U(GetComputerNameExA)(ComputerNameNetBIOS, b, &n), 0);
    EXPECT_ERR(ERROR_MORE_DATA);
    EXPECT_EQ(n, len + 1);
    n = 0; SET_ERR(0);
    EXPECT_EQ(U(GetComputerNameExA)(ComputerNameNetBIOS, NULL, &n), 0);
    EXPECT_ERR(ERROR_MORE_DATA);
    EXPECT_EQ(n, len + 1);
}
TEST(cnameex_netbios_matches_getcomputername) {
    REQUIRE(U(GetComputerNameExW)); REQUIRE(U(GetComputerNameW));
    WCHAR a[64], b[64]; DWORD na = 64, nb = 64;
    CHECK(U(GetComputerNameExW)(ComputerNameNetBIOS, a, &na));
    CHECK(U(GetComputerNameW)(b, &nb));
    CHECK(!wcscmp(a, b)); EXPECT_EQ(na, nb);
}
TEST(cnameex_physical_netbios_equals_netbios_unclustered) {
    REQUIRE(U(GetComputerNameExW));
    WCHAR a[64], b[64]; DWORD na = 64, nb = 64;
    CHECK(U(GetComputerNameExW)(ComputerNameNetBIOS, a, &na));
    CHECK(U(GetComputerNameExW)(ComputerNamePhysicalNetBIOS, b, &nb));
    CHECK(!_wcsicmp(a, b));
}
TEST(cnameex_physical_dns_equals_dns_unclustered) {
    REQUIRE(U(GetComputerNameExW));
    WCHAR a[300], b[300]; DWORD na = 300, nb = 300;
    CHECK(U(GetComputerNameExW)(ComputerNameDnsHostname, a, &na));
    CHECK(U(GetComputerNameExW)(ComputerNamePhysicalDnsHostname, b, &nb));
    CHECK(!_wcsicmp(a, b));
    na = nb = 300;
    CHECK(U(GetComputerNameExW)(ComputerNameDnsDomain, a, &na));
    CHECK(U(GetComputerNameExW)(ComputerNamePhysicalDnsDomain, b, &nb));
    CHECK(!_wcsicmp(a, b));
}
TEST(cnameex_netbios_prefix_of_dns_hostname) {
    REQUIRE(U(GetComputerNameExW));
    WCHAR nb[64], dh[300]; DWORD n1 = 64, n2 = 300;
    CHECK(U(GetComputerNameExW)(ComputerNameNetBIOS, nb, &n1));
    CHECK(U(GetComputerNameExW)(ComputerNameDnsHostname, dh, &n2));
    /* the NetBIOS name is the (up to 15 chars, uppercased) DNS host label */
    CHECK(!_wcsnicmp(nb, dh, n1 < n2 ? n1 : n2));
}
TEST(cnameex_fqdn_is_host_dot_domain) {
    REQUIRE(U(GetComputerNameExW));
    WCHAR h[300], d[300], f[600], expect[700]; DWORD nh = 300, nd = 300, nf = 600;
    CHECK(U(GetComputerNameExW)(ComputerNameDnsHostname, h, &nh));
    CHECK(U(GetComputerNameExW)(ComputerNameDnsDomain, d, &nd));
    CHECK(U(GetComputerNameExW)(ComputerNameDnsFullyQualified, f, &nf));
    if (nd) { wcscpy(expect, h); wcscat(expect, L"."); wcscat(expect, d); }
    else wcscpy(expect, h);
    CHECK(!wcscmp(f, expect));
    EXPECT_EQ(nf, wcslen(expect));
}
TEST(cnameex_physical_fqdn_is_host_dot_domain) {
    REQUIRE(U(GetComputerNameExW));
    WCHAR h[300], d[300], f[600], expect[700]; DWORD nh = 300, nd = 300, nf = 600;
    CHECK(U(GetComputerNameExW)(ComputerNamePhysicalDnsHostname, h, &nh));
    CHECK(U(GetComputerNameExW)(ComputerNamePhysicalDnsDomain, d, &nd));
    CHECK(U(GetComputerNameExW)(ComputerNamePhysicalDnsFullyQualified, f, &nf));
    if (nd) { wcscpy(expect, h); wcscat(expect, L"."); wcscat(expect, d); }
    else wcscpy(expect, h);
    CHECK(!wcscmp(f, expect));
}
TEST(cnameex_fqdn_required_size_with_null_buffer) {
    REQUIRE(U(GetComputerNameExW));
    WCHAR f[600]; DWORD nf = 600, n = 0;
    CHECK(U(GetComputerNameExW)(ComputerNameDnsFullyQualified, f, &nf));
    SET_ERR(0);
    EXPECT_EQ(U(GetComputerNameExW)(ComputerNameDnsFullyQualified, NULL, &n), 0);
    EXPECT_ERR(ERROR_MORE_DATA);
    EXPECT_EQ(n, nf + 1);
}
TEST(cnameex_dns_hostname_matches_env_when_set_by_os) {
    REQUIRE(U(GetComputerNameExW)); REQUIRE(U(GetEnvironmentVariableW));
    WCHAR h[300], e[300]; DWORD nh = 300;
    CHECK(U(GetComputerNameExW)(ComputerNameDnsHostname, h, &nh));
    if (U(GetEnvironmentVariableW)(L"COMPUTERNAME", e, 300)) CHECK(!_wcsnicmp(h, e, 15));
}
TEST(cnameex_invalid_format_values) {
    REQUIRE(U(GetComputerNameExW));
    static const int bad[] = { 8, 9, 10, 100, 0x7fffffff };
    WCHAR b[64];
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        DWORD n = 64; SET_ERR(0);
        EXPECT_EQ(U(GetComputerNameExW)((COMPUTER_NAME_FORMAT)bad[i], b, &n), 0);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(cnameex_a_invalid_format_values) {
    REQUIRE(U(GetComputerNameExA));
    static const int bad[] = { 8, 9, 50 };
    char b[64];
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        DWORD n = 64; SET_ERR(0);
        EXPECT_EQ(U(GetComputerNameExA)((COMPUTER_NAME_FORMAT)bad[i], b, &n), 0);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(cnameex_negative_format) {
    REQUIRE(U(GetComputerNameExW));
    WCHAR b[64]; DWORD n = 64;
    SET_ERR(0);
    EXPECT_EQ(U(GetComputerNameExW)((COMPUTER_NAME_FORMAT)-1, b, &n), 0);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_INVALID_PARAMETER, "err %lu", e);
}
TEST(cnameex_null_size) {
    REQUIRE(U(GetComputerNameExW)); REQUIRE(U(GetComputerNameExA));
    if (t_is_wine()) { t_skip("emulator faults on NULL size"); return; }
    WCHAR w[8]; char a[8];
    for (int f = 0; f < 8; f++) {
        SET_ERR(0);
        EXPECT_EQ(U(GetComputerNameExW)((COMPUTER_NAME_FORMAT)f, w, NULL), 0);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        SET_ERR(0);
        EXPECT_EQ(U(GetComputerNameExA)((COMPUTER_NAME_FORMAT)f, a, NULL), 0);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(cnameex_null_buffer_nonzero_size) {
    REQUIRE(U(GetComputerNameExW)); REQUIRE(U(GetComputerNameExA));
    for (int f = 0; f < 8; f++) {
        DWORD n = 16; SET_ERR(0);
        EXPECT_EQ(U(GetComputerNameExW)((COMPUTER_NAME_FORMAT)f, NULL, &n), 0);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        n = 16; SET_ERR(0);
        EXPECT_EQ(U(GetComputerNameExA)((COMPUTER_NAME_FORMAT)f, NULL, &n), 0);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(cnameex_dns_domain_may_be_empty) {
    REQUIRE(U(GetComputerNameExW));
    WCHAR d[300] = L"x"; DWORD n = 300;
    CHECK(U(GetComputerNameExW)(ComputerNameDnsDomain, d, &n));
    if (n == 0) EXPECT_EQ(d[0], 0);
    EXPECT_EQ(wcslen(d), n);
}
TEST(cnameex_empty_result_size_query) {
    /* a format whose value is empty (workgroup machines have no DNS domain): size 0 with a NULL buffer succeeds */
    REQUIRE(U(GetComputerNameExW));
    WCHAR d[300]; DWORD n = 300;
    CHECK(U(GetComputerNameExW)(ComputerNameDnsDomain, d, &n));
    if (n != 0) { t_note("machine has DNS domain; empty-name path not exercised"); return; }
    n = 0; SET_ERR(0);
    CHECK(U(GetComputerNameExW)(ComputerNameDnsDomain, NULL, &n));
    EXPECT_EQ(n, 0);
}
TEST(cnameex_repeat_calls_consistent) {
    REQUIRE(U(GetComputerNameExW));
    WCHAR a[300], b[300];
    for (int f = 0; f < 8; f++) {
        DWORD na = 300, nb = 300;
        BOOL ra = U(GetComputerNameExW)((COMPUTER_NAME_FORMAT)f, a, &na);
        BOOL rb = U(GetComputerNameExW)((COMPUTER_NAME_FORMAT)f, b, &nb);
        EXPECT_EQ(ra, rb); EXPECT_EQ(na, nb);
        CHECK(!wcscmp(a, b));
    }
}
TEST(cnameex_ascii_hostname_chars) {
    REQUIRE(U(GetComputerNameExA));
    char b[300]; DWORD n = 300;
    CHECK(U(GetComputerNameExA)(ComputerNameDnsHostname, b, &n));
    for (DWORD i = 0; i < n; i++) CHECKF(isalnum((unsigned char)b[i]) || b[i] == '-' || b[i] == '_' || (unsigned char)b[i] > 0x7f, "hostname char %02x", b[i]);
}

/* ---------- cluster override via environment (child) ---------- */
CHILD(cname_cluster) {
    SetEnvironmentVariableA("_CLUSTER_NETWORK_NAME_", "K32TCLUSTER");
    char a[64]; WCHAR w[64]; DWORD n = 64;
    if (!GetComputerNameA(a, &n) || n != 11 || strcmp(a, "K32TCLUSTER")) return 1;
    n = 64;
    if (!GetComputerNameW(w, &n) || n != 11) return 2;
    n = 11; SetLastError(0);
    if (GetComputerNameA(a, &n) || GetLastError() != ERROR_BUFFER_OVERFLOW || n != 12) return 3;
    n = 12;
    if (!GetComputerNameA(a, &n) || n != 11) return 4;
    return 0;
}
TEST(cname_cluster_env_override_child) {
    REQUIRE(U(GetComputerNameA));
    EXPECT_EQ(t_run_child("cname_cluster", "", 20000), 0);
}

/* ---------- DnsHostnameToComputerNameExW ---------- */
TEST(dnshost_simple_uppercased) {
    REQUIRE(UN(DnsHostnameToComputerNameExW));
    WCHAR b[32]; DWORD n = 32;
    CHECK(UN(DnsHostnameToComputerNameExW)(L"myhost", b, &n));
    EXPECT_EQ(n, 6);
    CHECK(!wcscmp(b, L"MYHOST"));
}
TEST(dnshost_mixed_case_and_digits) {
    REQUIRE(UN(DnsHostnameToComputerNameExW));
    WCHAR b[32]; DWORD n = 32;
    CHECK(UN(DnsHostnameToComputerNameExW)(L"Web-Server01", b, &n));
    CHECK(!wcscmp(b, L"WEB-SERVER01"));
    EXPECT_EQ(n, 12);
}
TEST(dnshost_truncated_to_15) {
    REQUIRE(UN(DnsHostnameToComputerNameExW));
    WCHAR b[32]; DWORD n = 32;
    CHECK(UN(DnsHostnameToComputerNameExW)(L"averyveryverylonghostname", b, &n));
    EXPECT_EQ(n, 15);
    CHECK(!wcscmp(b, L"AVERYVERYVERYLO"));
}
TEST(dnshost_exactly_15) {
    REQUIRE(UN(DnsHostnameToComputerNameExW));
    WCHAR b[32]; DWORD n = 32;
    CHECK(UN(DnsHostnameToComputerNameExW)(L"abcdefghijklmno", b, &n));
    EXPECT_EQ(n, 15);
    CHECK(!wcscmp(b, L"ABCDEFGHIJKLMNO"));
}
TEST(dnshost_buffer_too_small) {
    REQUIRE(UN(DnsHostnameToComputerNameExW));
    WCHAR b[32]; DWORD n = 3;
    SET_ERR(0);
    EXPECT_EQ(UN(DnsHostnameToComputerNameExW)(L"myhost", b, &n), 0);
    EXPECT_ERR(ERROR_MORE_DATA);
}
TEST(dnshost_roundtrip_with_own_dns_hostname) {
    REQUIRE(UN(DnsHostnameToComputerNameExW)); REQUIRE(U(GetComputerNameExW));
    WCHAR dns[300], nb[64], conv[64]; DWORD n1 = 300, n2 = 64, n3 = 64;
    CHECK(U(GetComputerNameExW)(ComputerNameDnsHostname, dns, &n1));
    CHECK(U(GetComputerNameExW)(ComputerNameNetBIOS, nb, &n2));
    CHECK(UN(DnsHostnameToComputerNameExW)(dns, conv, &n3));
    CHECK(!_wcsicmp(conv, nb));
}
TEST(dnshost_dotted_name_note) {
    REQUIRE(UN(DnsHostnameToComputerNameExW));
    WCHAR b[32]; DWORD n = 32;
    BOOL r = UN(DnsHostnameToComputerNameExW)(L"host.example.com", b, &n);
    t_note("dotted name -> %d \"%ls\" n=%lu err=%lu", r, b, n, GetLastError());
}
