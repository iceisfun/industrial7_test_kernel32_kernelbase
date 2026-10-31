/* Additional environment / sysinfo edge cases: concurrency, name edge cases, expand limits. */
#include "t.h"
#include "k.h"

typedef struct { int id; volatile LONG bad; } ex_worker;
static DWORD WINAPI ex_env_thread(LPVOID p) {
    ex_worker *w = (ex_worker *)p;
    BOOL (WINAPI *setv)(LPCWSTR, LPCWSTR) = (BOOL (WINAPI *)(LPCWSTR, LPCWSTR))GetProcAddress(GetModuleHandleA("kernelbase.dll"), "SetEnvironmentVariableW");
    DWORD (WINAPI *getv)(LPCWSTR, LPWSTR, DWORD) = (DWORD (WINAPI *)(LPCWSTR, LPWSTR, DWORD))GetProcAddress(GetModuleHandleA("kernelbase.dll"), "GetEnvironmentVariableW");
    if (!setv || !getv) { w->bad = 1; return 0; }
    WCHAR name[40], val[40], out[64];
    wsprintfW(name, L"K32T_XTH_%d", w->id);
    for (int i = 0; i < 300; i++) {
        wsprintfW(val, L"v%d_%d", w->id, i);
        if (!setv(name, val)) { w->bad++; continue; }
        DWORD n = getv(name, out, 64);
        if (n != lstrlenW(val) || lstrcmpW(out, val)) w->bad++;
    }
    setv(name, NULL);
    return 0;
}
TEST(extra_env_concurrent_distinct_vars) {
    REQUIRE(U(SetEnvironmentVariableW)); REQUIRE(U(GetEnvironmentVariableW));
    ex_worker w[8]; HANDLE h[8];
    for (int i = 0; i < 8; i++) { w[i].id = i; w[i].bad = 0; h[i] = CreateThread(NULL, 0, ex_env_thread, &w[i], 0, NULL); CHECK(h[i] != NULL); }
    for (int i = 0; i < 8; i++) if (h[i]) { EXPECT_EQ(WaitForSingleObject(h[i], 20000), WAIT_OBJECT_0); CloseHandle(h[i]); EXPECT_EQ(w[i].bad, 0); }
    WCHAR b[8]; SET_ERR(0);
    EXPECT_EQ(U(GetEnvironmentVariableW)(L"K32T_XTH_3", b, 8), 0);
    EXPECT_ERR(ERROR_ENVVAR_NOT_FOUND);
}
TEST(extra_env_trailing_equals_name_lookup) {
    REQUIRE(U(GetEnvironmentVariableW));
    WCHAR b[8]; SET_ERR(0);
    EXPECT_EQ(U(GetEnvironmentVariableW)(L"K32T_NEVER_SET=", b, 8), 0);
    EXPECT_ERR(ERROR_ENVVAR_NOT_FOUND);
}
TEST(extra_env_get_name_does_not_match_suffix) {
    REQUIRE(U(GetEnvironmentVariableA)); REQUIRE(U(SetEnvironmentVariableA));
    U(SetEnvironmentVariableA)("K32T_SUFFIXTEST", "1");
    char b[8]; SET_ERR(0);
    EXPECT_EQ(U(GetEnvironmentVariableA)("SUFFIXTEST", b, 8), 0);
    EXPECT_ERR(ERROR_ENVVAR_NOT_FOUND);
    SET_ERR(0);
    EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_SUFFIXTES", b, 8), 0);
    EXPECT_ERR(ERROR_ENVVAR_NOT_FOUND);
    EXPECT_EQ(U(GetEnvironmentVariableA)("K32T_SUFFIXTEST", b, 8), 1);
    U(SetEnvironmentVariableA)("K32T_SUFFIXTEST", NULL);
}
TEST(extra_env_set_success_clears_nothing_but_returns_true) {
    REQUIRE(U(SetEnvironmentVariableW));
    SET_ERR(0x1234);
    EXPECT_EQ(U(SetEnvironmentVariableW)(L"K32T_XSET", L"a"), 1);
    t_note("last error after successful set: 0x%lx", GetLastError());
    U(SetEnvironmentVariableW)(L"K32T_XSET", NULL);
}
TEST(extra_env_get_success_does_not_need_buffer_when_size_exact) {
    REQUIRE(U(GetEnvironmentVariableW)); REQUIRE(U(SetEnvironmentVariableW));
    U(SetEnvironmentVariableW)(L"K32T_XEX", L"abc");
    WCHAR b[5]; for (int i = 0; i < 5; i++) b[i] = 0x7777;
    EXPECT_EQ(U(GetEnvironmentVariableW)(L"K32T_XEX", b, 4), 3);
    EXPECT_EQ(b[3], 0);
    EXPECT_EQ(b[4], 0x7777);
    for (int i = 0; i < 5; i++) b[i] = 0x7777;
    EXPECT_EQ(U(GetEnvironmentVariableW)(L"K32T_XEX", b, 3), 4);
    EXPECT_EQ(b[0], 0);
    EXPECT_EQ(b[3], 0x7777);
    U(SetEnvironmentVariableW)(L"K32T_XEX", NULL);
}
TEST(extra_env_set_w_then_a_get_nonascii_name_parity) {
    REQUIRE(U(SetEnvironmentVariableW)); REQUIRE(U(GetEnvironmentVariableW));
    U(SetEnvironmentVariableW)(L"K32T_X\x00C9" L"NAME", L"v");
    WCHAR b[4];
    EXPECT_EQ(U(GetEnvironmentVariableW)(L"k32t_x\x00E9name", b, 4), 1);
    U(SetEnvironmentVariableW)(L"K32T_X\x00C9" L"NAME", NULL);
}
TEST(extra_expand_empty_source) {
    REQUIRE(U(ExpandEnvironmentStringsA)); REQUIRE(U(ExpandEnvironmentStringsW));
    char a[4] = "xyz"; WCHAR w[4] = L"xyz";
    EXPECT_EQ(U(ExpandEnvironmentStringsA)("", a, 4), 1);
    EXPECT_EQ(a[0], 0);
    EXPECT_EQ(U(ExpandEnvironmentStringsW)(L"", w, 4), 1);
    EXPECT_EQ(w[0], 0);
    EXPECT_EQ(U(ExpandEnvironmentStringsW)(L"", NULL, 0), 1);
}
TEST(extra_expand_w_huge_nsize_small_result) {
    REQUIRE(U(ExpandEnvironmentStringsW)); REQUIRE(U(SetEnvironmentVariableW));
    U(SetEnvironmentVariableW)(L"K32T_XH", L"zz");
    WCHAR b[16] = {0};
    EXPECT_EQ(U(ExpandEnvironmentStringsW)(L"%K32T_XH%", b, 16), 3);
    EXPECT_EQ(lstrcmpW(b, L"zz"), 0);
    U(SetEnvironmentVariableW)(L"K32T_XH", NULL);
}
TEST(extra_expand_case_insensitive_name) {
    REQUIRE(U(ExpandEnvironmentStringsW)); REQUIRE(U(SetEnvironmentVariableW));
    U(SetEnvironmentVariableW)(L"K32T_XCASE", L"v");
    WCHAR b[16] = {0};
    EXPECT_EQ(U(ExpandEnvironmentStringsW)(L"%k32t_xCASE%", b, 16), 2);
    EXPECT_EQ(lstrcmpW(b, L"v"), 0);
    U(SetEnvironmentVariableW)(L"K32T_XCASE", NULL);
}
TEST(extra_envstrings_block_double_nul_terminated_a) {
    REQUIRE(U(GetEnvironmentStringsA)); REQUIRE(U(FreeEnvironmentStringsA));
    LPCH p = U(GetEnvironmentStringsA)();
    CHECK(p != NULL);
    if (!p) return;
    size_t total = 0; int n = 0;
    while (p[total]) { total += strlen(p + total) + 1; n++; if (n > 100000) break; }
    CHECK(n > 0);
    EXPECT_EQ(p[total], 0);
    CHECK(U(FreeEnvironmentStringsA)(p));
}
TEST(extra_envstrings_free_returns_true_w) {
    REQUIRE(U(GetEnvironmentStringsW)); REQUIRE(U(FreeEnvironmentStringsW));
    LPWCH p = U(GetEnvironmentStringsW)();
    CHECK(p != NULL);
    if (p) EXPECT_EQ(U(FreeEnvironmentStringsW)(p), 1);
}
TEST(extra_getversion_high_bit_and_build_word) {
    REQUIRE(U(GetVersion));
    DWORD v = U(GetVersion)();
    EXPECT_EQ(v >> 31, 0);          /* NT family: bit 31 clear */
    CHECK((v & 0xFF) >= 6);
    CHECK((v >> 16) != 0);
}
TEST(extra_computername_not_empty_and_cnameex_netbios_len) {
    REQUIRE(U(GetComputerNameW));
    WCHAR b[MAX_COMPUTERNAME_LENGTH + 1]; DWORD n = MAX_COMPUTERNAME_LENGTH + 1;
    CHECK(U(GetComputerNameW)(b, &n));
    CHECK(n >= 1 && n <= MAX_COMPUTERNAME_LENGTH);
    EXPECT_EQ(lstrlenW(b), n);
}
TEST(extra_procfeature_fastfail_and_cmpxchg) {
    REQUIRE(U(IsProcessorFeaturePresent));
    CHECK(U(IsProcessorFeaturePresent)(PF_COMPARE_EXCHANGE_DOUBLE));
    CHECK(U(IsProcessorFeaturePresent)(PF_XMMI64_INSTRUCTIONS_AVAILABLE));
    CHECK(U(IsProcessorFeaturePresent)(PF_NX_ENABLED));
    CHECK(!U(IsProcessorFeaturePresent)(PF_FLOATING_POINT_EMULATED));
    CHECK(!U(IsProcessorFeaturePresent)(0xFFFFFFFF));
}
TEST(extra_systeminfo_page_size_power_of_two) {
    SYSTEM_INFO si; GetSystemInfo(&si);
    CHECK(si.dwPageSize && !(si.dwPageSize & (si.dwPageSize - 1)));
    CHECK(si.dwAllocationGranularity >= si.dwPageSize && !(si.dwAllocationGranularity & (si.dwAllocationGranularity - 1)));
}
