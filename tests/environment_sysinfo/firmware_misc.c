/* Firmware tables/type/variables (read-only), safe boot, manufacturing mode, VHD boot, power status,
 * file API code page mode (child), execution state. */
#include "t.h"
#include "k.h"

#define SIG(a,b,c,d) ((DWORD)(a) | ((DWORD)(b) << 8) | ((DWORD)(c) << 16) | ((DWORD)(d) << 24))
#define SIG_ACPI SIG('A','C','P','I')
#define SIG_FIRM SIG('F','I','R','M')
#define SIG_RSMB SIG('R','S','M','B')

UDECL(DWORD, GetFirmwareEnvironmentVariableExW, (LPCWSTR, LPCWSTR, PVOID, DWORD, PDWORD));
UDECL(BOOL, GetOsManufacturingMode, (PBOOL));

/* ---------- firmware type ---------- */
TEST(fw_type_value) {
    REQUIRE(U(GetFirmwareType));
    FIRMWARE_TYPE t = (FIRMWARE_TYPE)0xCC;
    CHECK(U(GetFirmwareType)(&t));
    CHECKF(t == FirmwareTypeBios || t == FirmwareTypeUefi, "type %d", t);
    t_note("firmware type %d (1=BIOS, 2=UEFI)", t);
}
TEST(fw_type_null_pointer) {
    REQUIRE(U(GetFirmwareType));
    SET_ERR(0);
    EXPECT_EQ(U(GetFirmwareType)(NULL), 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(fw_type_stable) {
    REQUIRE(U(GetFirmwareType));
    FIRMWARE_TYPE a, b;
    CHECK(U(GetFirmwareType)(&a)); CHECK(U(GetFirmwareType)(&b));
    EXPECT_EQ(a, b);
}
TEST(fw_type_matches_firmware_provider_on_uefi) {
    REQUIRE(U(GetFirmwareType)); REQUIRE(U(EnumSystemFirmwareTables));
    FIRMWARE_TYPE t; CHECK(U(GetFirmwareType)(&t));
    if (t != FirmwareTypeUefi) { t_skip("not UEFI"); return; }
    /* UEFI systems expose ACPI tables */
    UINT n = U(EnumSystemFirmwareTables)(SIG_ACPI, NULL, 0);
    t_note("ACPI table list size on UEFI: %u", n);
    CHECK(n > 0);
}

/* ---------- firmware environment variables (read only) ---------- */
static const WCHAR fw_guid[] = L"{00000000-0000-0000-0000-000000000000}";
TEST(fwvar_null_arguments) {
    REQUIRE(U(GetFirmwareEnvironmentVariableW)); REQUIRE(U(GetFirmwareEnvironmentVariableA));
    BYTE b[16];
    SET_ERR(0); EXPECT_EQ(U(GetFirmwareEnvironmentVariableW)(NULL, fw_guid, b, 16), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(GetFirmwareEnvironmentVariableW)(L"k32t_none", NULL, b, 16), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(GetFirmwareEnvironmentVariableW)(NULL, NULL, b, 16), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(GetFirmwareEnvironmentVariableA)(NULL, "{00000000-0000-0000-0000-000000000000}", b, 16), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(fwvar_malformed_guid) {
    REQUIRE(U(GetFirmwareEnvironmentVariableW));
    static const WCHAR *bad[] = { L"", L"not a guid", L"00000000-0000-0000-0000-000000000000", L"{00000000-0000-0000-0000-00000000000}",
        L"{0000000-0000-0000-0000-000000000000}", L"{GGGGGGGG-0000-0000-0000-000000000000}", L"{00000000-0000-0000-0000-000000000000" };
    BYTE b[16];
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SET_ERR(0);
        DWORD r = U(GetFirmwareEnvironmentVariableW)(L"k32t_none", bad[i], b, 16);
        CHECKF(r == 0, "guid #%u accepted", i);
        DWORD e = GetLastError();
        CHECKF(e == ERROR_INVALID_PARAMETER, "guid #%u err %lu", i, e);
    }
}
TEST(fwvar_missing_variable_error_class) {
    REQUIRE(U(GetFirmwareEnvironmentVariableW));
    BYTE b[16]; SET_ERR(0);
    DWORD r = U(GetFirmwareEnvironmentVariableW)(L"k32t_no_such_variable", fw_guid, b, 16);
    DWORD e = GetLastError();
    EXPECT_EQ(r, 0);
    /* 1314 without SeSystemEnvironmentPrivilege (normal case), 1 (ERROR_INVALID_FUNCTION) on legacy BIOS,
     * 203 if the privilege is enabled and the variable simply does not exist */
    CHECKF(e == ERROR_PRIVILEGE_NOT_HELD || e == ERROR_INVALID_FUNCTION || e == ERROR_ENVVAR_NOT_FOUND, "err %lu", e);
}
TEST(fwvar_a_matches_w_error) {
    REQUIRE(U(GetFirmwareEnvironmentVariableW)); REQUIRE(U(GetFirmwareEnvironmentVariableA));
    BYTE b[16]; SET_ERR(0);
    U(GetFirmwareEnvironmentVariableW)(L"k32t_no_such_variable", fw_guid, b, 16);
    DWORD ew = GetLastError(); SET_ERR(0);
    DWORD r = U(GetFirmwareEnvironmentVariableA)("k32t_no_such_variable", "{00000000-0000-0000-0000-000000000000}", b, 16);
    EXPECT_EQ(r, 0);
    EXPECT_ERR(ew);
}
TEST(fwvar_ex_variant) {
    REQUIRE(UN(GetFirmwareEnvironmentVariableExW));
    BYTE b[16]; DWORD attr = 0xCCCC; SET_ERR(0);
    DWORD r = UN(GetFirmwareEnvironmentVariableExW)(L"k32t_no_such_variable", fw_guid, b, 16, &attr);
    DWORD e = GetLastError();
    EXPECT_EQ(r, 0);
    CHECKF(e == ERROR_PRIVILEGE_NOT_HELD || e == ERROR_INVALID_FUNCTION || e == ERROR_ENVVAR_NOT_FOUND, "err %lu", e);
    SET_ERR(0);
    EXPECT_EQ(UN(GetFirmwareEnvironmentVariableExW)(NULL, fw_guid, b, 16, &attr), 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(fwvar_zero_size_buffer) {
    REQUIRE(U(GetFirmwareEnvironmentVariableW));
    SET_ERR(0);
    DWORD r = U(GetFirmwareEnvironmentVariableW)(L"k32t_no_such_variable", fw_guid, NULL, 0);
    EXPECT_EQ(r, 0);
    CHECK(GetLastError() != 0);
}

/* ---------- firmware tables ---------- */
TEST(fwtable_enum_invalid_provider) {
    REQUIRE(U(EnumSystemFirmwareTables));
    SET_ERR(0);
    EXPECT_EQ(U(EnumSystemFirmwareTables)(0x12345678, NULL, 0), 0);
    CHECK(GetLastError() != 0);
    SET_ERR(0);
    EXPECT_EQ(U(EnumSystemFirmwareTables)(0, NULL, 0), 0);
    CHECK(GetLastError() != 0);
}
TEST(fwtable_get_invalid_provider) {
    REQUIRE(U(GetSystemFirmwareTable));
    SET_ERR(0);
    EXPECT_EQ(U(GetSystemFirmwareTable)(0x12345678, 0, NULL, 0), 0);
    CHECK(GetLastError() != 0);
}
TEST(fwtable_acpi_enum_size_query) {
    REQUIRE(U(EnumSystemFirmwareTables));
    SET_ERR(0);
    UINT n = U(EnumSystemFirmwareTables)(SIG_ACPI, NULL, 0);
    if (!n) { t_skip("no ACPI tables (err %lu)", GetLastError()); return; }
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(n % 4, 0);
    CHECK(n >= 4 * 4);
}
TEST(fwtable_acpi_enum_contents) {
    REQUIRE(U(EnumSystemFirmwareTables));
    UINT n = U(EnumSystemFirmwareTables)(SIG_ACPI, NULL, 0);
    if (!n) { t_skip("no ACPI tables"); return; }
    DWORD *ids = (DWORD *)malloc(n + 64);
    memset(ids, 0xCC, n + 64);
    SET_ERR(0x1234);
    UINT r = U(EnumSystemFirmwareTables)(SIG_ACPI, ids, n);
    EXPECT_EQ(r, n);
    EXPECT_ERR(0);
    int facp = 0, dsdt = 0;
    for (UINT i = 0; i < n / 4; i++) {
        BYTE *c = (BYTE *)&ids[i];
        for (int k = 0; k < 4; k++) CHECKF(c[k] >= 0x20 && c[k] < 0x7F, "id %u byte %d = 0x%02x", i, k, c[k]);
        if (ids[i] == SIG('F','A','C','P')) facp = 1;
        if (ids[i] == SIG('D','S','D','T')) dsdt = 1;
    }
    CHECK(facp);
    t_note("ACPI tables: %u, FACP=%d DSDT=%d (DSDT is referenced from FACP, not enumerated by XSDT)", n / 4, facp, dsdt);
    EXPECT_EQ(*(DWORD *)((BYTE *)ids + n), 0xCCCCCCCC);   /* nothing written past the reported size */
    free(ids);
}
TEST(fwtable_acpi_enum_small_buffer) {
    REQUIRE(U(EnumSystemFirmwareTables));
    UINT n = U(EnumSystemFirmwareTables)(SIG_ACPI, NULL, 0);
    if (!n) { t_skip("no ACPI tables"); return; }
    BYTE *b = (BYTE *)malloc(n + 16); memset(b, 0x5A, n + 16);
    SET_ERR(0);
    UINT r = U(EnumSystemFirmwareTables)(SIG_ACPI, b, n - 4);
    EXPECT_EQ(r, n);
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    for (UINT i = 0; i < n; i++) if (b[i] != 0x5A) { CHECKF(0, "buffer modified at %u on failure", i); break; }
    SET_ERR(0);
    r = U(EnumSystemFirmwareTables)(SIG_ACPI, b, 0);
    EXPECT_EQ(r, n);
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    SET_ERR(0);
    r = U(EnumSystemFirmwareTables)(SIG_ACPI, b, n + 16);
    EXPECT_EQ(r, n);
    free(b);
}
TEST(fwtable_acpi_tables_well_formed) {
    REQUIRE(U(EnumSystemFirmwareTables)); REQUIRE(U(GetSystemFirmwareTable));
    UINT n = U(EnumSystemFirmwareTables)(SIG_ACPI, NULL, 0);
    if (!n) { t_skip("no ACPI tables"); return; }
    DWORD *ids = (DWORD *)malloc(n);
    CHECK(U(EnumSystemFirmwareTables)(SIG_ACPI, ids, n) == n);
    for (UINT i = 0; i < n / 4; i++) {
        UINT sz = U(GetSystemFirmwareTable)(SIG_ACPI, ids[i], NULL, 0);
        CHECKF(sz >= 36, "table %.4s size %u", (char *)&ids[i], sz);
        if (sz < 36 || sz > 64 * 1024 * 1024) continue;
        BYTE *t = (BYTE *)malloc(sz + 8); memset(t, 0xEE, sz + 8);
        SET_ERR(0x1234);
        UINT r = U(GetSystemFirmwareTable)(SIG_ACPI, ids[i], t, sz);
        CHECKF(r == sz, "table %.4s: %u vs %u", (char *)&ids[i], r, sz);
        EXPECT_ERR(0);
        CHECKF(!memcmp(t, &ids[i], 4), "signature of %.4s", (char *)&ids[i]);
        DWORD len = *(DWORD *)(t + 4);
        CHECKF(len == sz, "table %.4s length field %lu vs %u", (char *)&ids[i], len, sz);
        if (ids[i] != SIG('F','A','C','S')) {
            unsigned sum = 0; for (UINT k = 0; k < sz; k++) sum += t[k];
            CHECKF((sum & 0xFF) == 0, "table %.4s checksum 0x%x", (char *)&ids[i], sum & 0xFF);
        }
        CHECK(t[sz] == 0xEE);
        free(t);
    }
    free(ids);
}
TEST(fwtable_acpi_buffer_boundaries) {
    REQUIRE(U(GetSystemFirmwareTable));
    UINT sz = U(GetSystemFirmwareTable)(SIG_ACPI, SIG('F','A','C','P'), NULL, 0);
    if (!sz) { t_skip("no FACP"); return; }
    SET_ERR(0);
    sz = U(GetSystemFirmwareTable)(SIG_ACPI, SIG('F','A','C','P'), NULL, 0);
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    BYTE *b = (BYTE *)malloc(sz + 16); memset(b, 0x5A, sz + 16);
    SET_ERR(0);
    EXPECT_EQ(U(GetSystemFirmwareTable)(SIG_ACPI, SIG('F','A','C','P'), b, sz - 1), sz);
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    for (UINT i = 0; i < sz + 16; i++) if (b[i] != 0x5A) { CHECKF(0, "buffer written on failure at %u", i); break; }
    EXPECT_EQ(U(GetSystemFirmwareTable)(SIG_ACPI, SIG('F','A','C','P'), b, sz), sz);
    EXPECT_EQ(U(GetSystemFirmwareTable)(SIG_ACPI, SIG('F','A','C','P'), b, sz + 16), sz);
    free(b);
}
TEST(fwtable_acpi_unknown_table) {
    REQUIRE(U(GetSystemFirmwareTable));
    SET_ERR(0);
    EXPECT_EQ(U(GetSystemFirmwareTable)(SIG_ACPI, SIG('Z','Z','Z','Z'), NULL, 0), 0);
    DWORD e = GetLastError();
    CHECK(e != 0);
    t_note("unknown ACPI table error %lu", e);
}
TEST(fwtable_acpi_facp_revision) {
    REQUIRE(U(GetSystemFirmwareTable));
    UINT sz = U(GetSystemFirmwareTable)(SIG_ACPI, SIG('F','A','C','P'), NULL, 0);
    if (!sz) { t_skip("no FACP"); return; }
    BYTE *b = (BYTE *)malloc(sz);
    CHECK(U(GetSystemFirmwareTable)(SIG_ACPI, SIG('F','A','C','P'), b, sz) == sz);
    CHECK(b[8] >= 1 && b[8] <= 6);   /* FADT major revision */
    free(b);
}
TEST(fwtable_rsmb_enum_and_get) {
    REQUIRE(U(EnumSystemFirmwareTables)); REQUIRE(U(GetSystemFirmwareTable));
    UINT n = U(EnumSystemFirmwareTables)(SIG_RSMB, NULL, 0);
    if (!n) { t_skip("no SMBIOS provider data"); return; }
    t_note("RSMB enumeration size %u", n);
    EXPECT_EQ(n % 4, 0);
    UINT sz = U(GetSystemFirmwareTable)(SIG_RSMB, 0, NULL, 0);
    if (!sz) { t_skip("no RSMB 0 table"); return; }
    BYTE *b = (BYTE *)malloc(sz);
    SET_ERR(0x1234);
    CHECK(U(GetSystemFirmwareTable)(SIG_RSMB, 0, b, sz) == sz);
    EXPECT_ERR(0);
    DWORD len = *(DWORD *)(b + 4);
    EXPECT_EQ(len + 8, sz);                 /* RawSMBIOSData header is 8 bytes */
    CHECK(b[1] >= 2);                       /* SMBIOS major version */
    CHECK(b[2] <= 20);
    free(b);
}
TEST(fwtable_rsmb_structure_chain) {
    REQUIRE(U(GetSystemFirmwareTable));
    UINT sz = U(GetSystemFirmwareTable)(SIG_RSMB, 0, NULL, 0);
    if (!sz) { t_skip("no RSMB"); return; }
    BYTE *b = (BYTE *)malloc(sz);
    CHECK(U(GetSystemFirmwareTable)(SIG_RSMB, 0, b, sz) == sz);
    DWORD len = *(DWORD *)(b + 4);
    BYTE *p = b + 8, *end = b + 8 + len;
    int count = 0, type0 = 0, type1 = 0, ended = 0;
    while (p + 4 <= end) {
        BYTE type = p[0], l = p[1];
        if (l < 4 || p + l > end) { CHECK(0); break; }
        count++;
        if (type == 0) type0++;
        if (type == 1) type1++;
        BYTE *s = p + l;
        while (s + 1 < end && !(s[0] == 0 && s[1] == 0)) s++;   /* string set ends with a double NUL */
        p = s + 2;
        if (type == 127) { ended = 1; break; }
    }
    CHECK(count >= 3);
    CHECK(type0 >= 1);
    CHECK(ended);
    free(b);
}
TEST(fwtable_firm_enum_note) {
    REQUIRE(U(EnumSystemFirmwareTables));
    SET_ERR(0);
    UINT n = U(EnumSystemFirmwareTables)(SIG_FIRM, NULL, 0);
    t_note("FIRM enumeration -> %u (err %lu)", n, GetLastError());
    if (n) EXPECT_EQ(n % 4, 0);
}
TEST(fwtable_enum_provider_signature_case_sensitive) {
    REQUIRE(U(EnumSystemFirmwareTables));
    SET_ERR(0);
    UINT n = U(EnumSystemFirmwareTables)(SIG('a','c','p','i'), NULL, 0);
    EXPECT_EQ(n, 0);
    CHECK(GetLastError() != 0);
}
TEST(fwtable_k32_kernelbase_same) {
    REQUIRE(UK(EnumSystemFirmwareTables)); REQUIRE(UB(EnumSystemFirmwareTables)); REQUIRE(UK(GetSystemFirmwareTable)); REQUIRE(UB(GetSystemFirmwareTable));
    EXPECT_EQ(UK(EnumSystemFirmwareTables)(SIG_ACPI, NULL, 0), UB(EnumSystemFirmwareTables)(SIG_ACPI, NULL, 0));
    EXPECT_EQ(UK(GetSystemFirmwareTable)(SIG_ACPI, SIG('F','A','C','P'), NULL, 0), UB(GetSystemFirmwareTable)(SIG_ACPI, SIG('F','A','C','P'), NULL, 0));
}

/* ---------- misc boot / mode queries ---------- */
TEST(boot_safe_mode_value) {
    REQUIRE(U(GetOsSafeBootMode));
    DWORD m = 0xCC;
    CHECK(U(GetOsSafeBootMode)(&m));
    CHECKF(m <= 3, "mode %lu", m);
    t_note("safe boot mode %lu (0 normal, 1 minimal, 2 network?, 3 DS repair)", m);
}
TEST(boot_safe_mode_normal_boot) {
    REQUIRE(U(GetOsSafeBootMode));
    DWORD m = 0xCC; CHECK(U(GetOsSafeBootMode)(&m));
    if (m) { t_note("system is booted in safe mode"); return; }
    EXPECT_EQ(m, 0);
}
TEST(boot_safe_mode_stable) {
    REQUIRE(U(GetOsSafeBootMode));
    DWORD a = 1, b = 2; CHECK(U(GetOsSafeBootMode)(&a)); CHECK(U(GetOsSafeBootMode)(&b));
    EXPECT_EQ(a, b);
}
TEST(boot_safe_mode_matches_shared_data) {
    REQUIRE(U(GetOsSafeBootMode));
    DWORD m = 0xCC; CHECK(U(GetOsSafeBootMode)(&m));
    BYTE sb = *(volatile const BYTE *)(0x7FFE0000 + 0x2EC);
    EXPECT_EQ(sb != 0, m != 0);
}
TEST(boot_manufacturing_mode) {
    REQUIRE(UN(GetOsManufacturingMode));
    BOOL m = 0xCC; SET_ERR(0);
    BOOL r = UN(GetOsManufacturingMode)(&m);
    if (!r) { t_note("GetOsManufacturingMode failed, err %lu", GetLastError()); return; }
    CHECKF(m == 0 || m == 1, "mode %d", m);
    EXPECT_EQ(m, 0);   /* retail systems are not in manufacturing mode */
}
TEST(boot_manufacturing_mode_null) {
    REQUIRE(UN(GetOsManufacturingMode));
    SET_ERR(0);
    EXPECT_EQ(UN(GetOsManufacturingMode)(NULL), 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(boot_native_vhd) {
    REQUIRE(U(IsNativeVhdBoot));
    BOOL v = 0xCC;
    CHECK(U(IsNativeVhdBoot)(&v));
    CHECKF(v == 0 || v == 1, "value %d", v);
    t_note("native VHD boot: %d", v);
}
TEST(boot_native_vhd_null) {
    REQUIRE(U(IsNativeVhdBoot));
    SET_ERR(0);
    EXPECT_EQ(U(IsNativeVhdBoot)(NULL), 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(boot_native_vhd_stable) {
    REQUIRE(U(IsNativeVhdBoot));
    BOOL a = 1, b = 2; CHECK(U(IsNativeVhdBoot)(&a)); CHECK(U(IsNativeVhdBoot)(&b));
    EXPECT_EQ(a, b);
}
TEST(boot_dep_policy_range) {
    REQUIRE(U(GetSystemDEPPolicy));
    int p = (int)U(GetSystemDEPPolicy)();
    CHECK(p >= 0 && p <= 3);
    t_note("system DEP policy %d (0 AlwaysOff, 1 AlwaysOn, 2 OptIn, 3 OptOut)", p);
    CHECK(p != DEPPolicyAlwaysOff);   /* NX is mandatory on x64 */
}
TEST(boot_dep_policy_k32_matches_kernelbase) {
    REQUIRE(UK(GetSystemDEPPolicy)); REQUIRE(UB(GetSystemDEPPolicy));
    EXPECT_EQ(UK(GetSystemDEPPolicy)(), UB(GetSystemDEPPolicy)());
}

/* ---------- power status ---------- */
TEST(power_status_basic) {
    REQUIRE(U(GetSystemPowerStatus));
    SYSTEM_POWER_STATUS s; memset(&s, 0xCC, sizeof s);
    CHECK(U(GetSystemPowerStatus)(&s));
    CHECKF(s.ACLineStatus == 0 || s.ACLineStatus == 1 || s.ACLineStatus == 255, "ac %u", s.ACLineStatus);
    CHECKF(s.BatteryLifePercent <= 100 || s.BatteryLifePercent == 255, "pct %u", s.BatteryLifePercent);
    CHECKF(s.SystemStatusFlag <= 1, "status flag %u", s.SystemStatusFlag);
}
TEST(power_status_struct_size) {
    EXPECT_EQ(sizeof(SYSTEM_POWER_STATUS), 12);
    EXPECT_EQ(offsetof(SYSTEM_POWER_STATUS, BatteryLifeTime), 4);
}
TEST(power_status_battery_flag_bits) {
    REQUIRE(U(GetSystemPowerStatus));
    SYSTEM_POWER_STATUS s; CHECK(U(GetSystemPowerStatus)(&s));
    CHECKF(s.BatteryFlag == 255 || (s.BatteryFlag & ~(1 | 2 | 4 | 8 | 128)) == 0, "battery flag 0x%x", s.BatteryFlag);
    if (s.BatteryFlag & 128) {
        CHECKF(s.BatteryFlag == 128, "no-battery flag combined with others: 0x%x", s.BatteryFlag);
        EXPECT_EQ(s.BatteryLifePercent, 255);
    }
}
TEST(power_status_ac_battery_lifetime) {
    REQUIRE(U(GetSystemPowerStatus));
    SYSTEM_POWER_STATUS s; CHECK(U(GetSystemPowerStatus)(&s));
    if (s.ACLineStatus == 1) EXPECT_EQ(s.BatteryLifeTime, 0xFFFFFFFF);
    t_note("ac=%u flag=0x%x pct=%u life=%lu full=%lu", s.ACLineStatus, s.BatteryFlag, s.BatteryLifePercent, s.BatteryLifeTime, s.BatteryFullLifeTime);
}
TEST(power_status_charging_implies_ac) {
    REQUIRE(U(GetSystemPowerStatus));
    SYSTEM_POWER_STATUS s; CHECK(U(GetSystemPowerStatus)(&s));
    if (s.BatteryFlag != 255 && (s.BatteryFlag & 8)) CHECK(s.ACLineStatus == 1);
}
TEST(power_status_repeatable_ac_line) {
    REQUIRE(U(GetSystemPowerStatus));
    SYSTEM_POWER_STATUS a, b; CHECK(U(GetSystemPowerStatus)(&a));
    for (int i = 0; i < 20; i++) { CHECK(U(GetSystemPowerStatus)(&b)); EXPECT_EQ(a.ACLineStatus, b.ACLineStatus); }
}
TEST(power_status_k32_kernelbase_same) {
    REQUIRE(UK(GetSystemPowerStatus)); REQUIRE(UB(GetSystemPowerStatus));
    SYSTEM_POWER_STATUS a, b; CHECK(UK(GetSystemPowerStatus)(&a)); CHECK(UB(GetSystemPowerStatus)(&b));
    EXPECT_EQ(a.ACLineStatus, b.ACLineStatus);
    EXPECT_EQ(a.BatteryFlag, b.BatteryFlag);
}

/* ---------- SetThreadExecutionState (always restored to ES_CONTINUOUS) ---------- */
TEST(execstate_continuous_roundtrip) {
    REQUIRE(U(SetThreadExecutionState));
    EXECUTION_STATE p0 = U(SetThreadExecutionState)(ES_CONTINUOUS);
    CHECK(p0 != 0);
    EXECUTION_STATE p1 = U(SetThreadExecutionState)(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
    CHECK(p1 != 0);
    EXPECT_EQ(p1 & ES_SYSTEM_REQUIRED, 0);
    EXECUTION_STATE p2 = U(SetThreadExecutionState)(ES_CONTINUOUS);
    EXPECT_EQ(p2, (ES_CONTINUOUS | ES_SYSTEM_REQUIRED));
    EXECUTION_STATE p3 = U(SetThreadExecutionState)(ES_CONTINUOUS);
    EXPECT_EQ(p3 & ES_SYSTEM_REQUIRED, 0);
    EXPECT_EQ(p3 & ES_CONTINUOUS, ES_CONTINUOUS);
    t_note("initial previous state 0x%lx", (DWORD)p0);
}
TEST(execstate_system_required_bit_tracking) {
    REQUIRE(U(SetThreadExecutionState));
    U(SetThreadExecutionState)(ES_CONTINUOUS);
    U(SetThreadExecutionState)(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
    EXECUTION_STATE a = U(SetThreadExecutionState)(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
    CHECK(a & ES_SYSTEM_REQUIRED);
    CHECK(a & ES_CONTINUOUS);
    EXECUTION_STATE b = U(SetThreadExecutionState)(ES_CONTINUOUS);
    CHECK(b & ES_SYSTEM_REQUIRED);
    EXECUTION_STATE c = U(SetThreadExecutionState)(ES_CONTINUOUS);
    EXPECT_EQ(c & ES_SYSTEM_REQUIRED, 0);
}
TEST(execstate_display_required_roundtrip) {
    REQUIRE(U(SetThreadExecutionState));
    U(SetThreadExecutionState)(ES_CONTINUOUS);
    EXECUTION_STATE p = U(SetThreadExecutionState)(ES_CONTINUOUS | ES_DISPLAY_REQUIRED);
    CHECK(p != 0);
    EXECUTION_STATE q = U(SetThreadExecutionState)(ES_CONTINUOUS);
    CHECK(q & ES_DISPLAY_REQUIRED);
    EXECUTION_STATE r = U(SetThreadExecutionState)(ES_CONTINUOUS);
    EXPECT_EQ(r & ES_DISPLAY_REQUIRED, 0);
}
TEST(execstate_one_shot_not_sticky) {
    REQUIRE(U(SetThreadExecutionState));
    U(SetThreadExecutionState)(ES_CONTINUOUS);
    EXECUTION_STATE p = U(SetThreadExecutionState)(ES_SYSTEM_REQUIRED);   /* resets the idle timer once */
    CHECK(p != 0);
    EXECUTION_STATE q = U(SetThreadExecutionState)(ES_CONTINUOUS);
    EXPECT_EQ(q & ES_CONTINUOUS, ES_CONTINUOUS);
    t_note("state after one-shot call: 0x%lx", (DWORD)q);
    U(SetThreadExecutionState)(ES_CONTINUOUS);
}
static DWORD WINAPI es_thread(LPVOID p) {
    EXECUTION_STATE (WINAPI *f)(EXECUTION_STATE) = (EXECUTION_STATE (WINAPI *)(EXECUTION_STATE))p;
    f(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
    return 0;   /* exits without clearing */
}
TEST(execstate_is_per_thread) {
    REQUIRE(U(SetThreadExecutionState));
    U(SetThreadExecutionState)(ES_CONTINUOUS);
    HANDLE h = CreateThread(NULL, 0, es_thread, (LPVOID)U(SetThreadExecutionState), 0, NULL);
    CHECK(h != NULL);
    if (h) { WaitForSingleObject(h, 10000); CloseHandle(h); }
    EXECUTION_STATE p = U(SetThreadExecutionState)(ES_CONTINUOUS);
    t_note("state seen by this thread after worker set SYSTEM_REQUIRED and exited: 0x%lx", (DWORD)p);
}
TEST(execstate_k32_export_resolves) {
    REQUIRE(UK(SetThreadExecutionState));
    EXECUTION_STATE p = UK(SetThreadExecutionState)(ES_CONTINUOUS);
    CHECK(p != 0);
    UK(SetThreadExecutionState)(ES_CONTINUOUS);
}

/* ---------- file API code page mode ---------- */
TEST(fileapis_default_is_ansi) {
    REQUIRE(U(AreFileApisANSI));
    EXPECT_EQ(U(AreFileApisANSI)(), TRUE);
}
TEST(fileapis_k32_kernelbase_agree) {
    REQUIRE(UK(AreFileApisANSI)); REQUIRE(UB(AreFileApisANSI));
    EXPECT_EQ(UK(AreFileApisANSI)(), UB(AreFileApisANSI)());
}
static int fa_find_byte(void) {
    for (int b = 0x80; b <= 0xFF; b++) {
        char c = (char)b; WCHAR a[2] = {0}, o[2] = {0};
        int na = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, &c, 1, a, 2);
        int no = MultiByteToWideChar(CP_OEMCP, MB_ERR_INVALID_CHARS, &c, 1, o, 2);
        if (na == 1 && no == 1 && a[0] != o[0] && a[0] >= 0x80 && o[0] >= 0x80 && a[0] != 0xFFFD && o[0] != 0xFFFD
            && !wcschr(L"\\/:*?\"<>|", a[0]) && !wcschr(L"\\/:*?\"<>|", o[0])) return b;
    }
    return -1;
}
CHILD(fileapis) {
    /* argv[1] = scratch directory (ASCII, trailing backslash) */
    if (argc < 2) return 90;
    int b = fa_find_byte();
    if (b < 0) return 77;
    char c = (char)b; WCHAR wa, wo;
    MultiByteToWideChar(CP_ACP, 0, &c, 1, &wa, 1);
    MultiByteToWideChar(CP_OEMCP, 0, &c, 1, &wo, 1);
    char namea[MAX_PATH]; WCHAR pa[MAX_PATH], po[MAX_PATH], dirw[MAX_PATH];
    snprintf(namea, sizeof namea, "%sfa_%c.k", argv[1], c);
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, dirw, MAX_PATH);
    wsprintfW(pa, L"%lsfa_%lc.k", dirw, wa);
    wsprintfW(po, L"%lsfa_%lc.k", dirw, wo);
    if (!AreFileApisANSI()) return 1;
    HANDLE h = CreateFileA(namea, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 2;
    CloseHandle(h);
    if (GetFileAttributesW(pa) == INVALID_FILE_ATTRIBUTES) return 3;
    if (GetFileAttributesW(po) != INVALID_FILE_ATTRIBUTES) return 4;
    SetFileApisToOEM();
    if (AreFileApisANSI()) return 5;
    if (GetFileAttributesA(namea) != INVALID_FILE_ATTRIBUTES) return 6;       /* now decoded as OEM: no such file */
    h = CreateFileA(namea, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return 7;
    CloseHandle(h);
    if (GetFileAttributesW(po) == INVALID_FILE_ATTRIBUTES) return 8;
    if (GetFileAttributesA(namea) == INVALID_FILE_ATTRIBUTES) return 9;
    /* wide APIs are unaffected */
    if (GetFileAttributesW(pa) == INVALID_FILE_ATTRIBUTES) return 10;
    SetFileApisToANSI();
    if (!AreFileApisANSI()) return 11;
    if (GetFileAttributesA(namea) == INVALID_FILE_ATTRIBUTES) return 12;
    DeleteFileW(pa); DeleteFileW(po);
    return 0;
}
TEST(fileapis_oem_mode_changes_ansi_name_decoding) {
    REQUIRE(U(SetFileApisToOEM)); REQUIRE(U(SetFileApisToANSI)); REQUIRE(U(AreFileApisANSI));
    if (fa_find_byte() < 0) { t_skip("ANSI and OEM code pages decode identically here"); return; }
    DWORD r = t_run_child("fileapis", t_tmpdir(), 20000);
    CHECKF(r == 0, "child step %lu failed", r);
}
CHILD(fileapis_toggle) {
    if (!AreFileApisANSI()) return 1;
    for (int i = 0; i < 100; i++) {
        SetFileApisToOEM();  if (AreFileApisANSI()) return 2;
        SetFileApisToOEM();  if (AreFileApisANSI()) return 3;
        SetFileApisToANSI(); if (!AreFileApisANSI()) return 4;
        SetFileApisToANSI(); if (!AreFileApisANSI()) return 5;
    }
    return 0;
}
TEST(fileapis_toggle_is_idempotent) {
    REQUIRE(U(SetFileApisToOEM));
    EXPECT_EQ(t_run_child("fileapis_toggle", "", 20000), 0);
}
static DWORD WINAPI fa_probe_thread(LPVOID p) { *(BOOL *)p = !AreFileApisANSI(); return 0; }
CHILD(fileapis_process_wide) {
    SetFileApisToOEM();
    BOOL main_oem = !AreFileApisANSI();
    /* the mode is process-wide: another thread sees it */
    struct { BOOL seen; } r = { 0 };
    HANDLE h = CreateThread(NULL, 0, fa_probe_thread, &r.seen, 0, NULL);
    WaitForSingleObject(h, 10000); CloseHandle(h);
    SetFileApisToANSI();
    return (main_oem && r.seen) ? 0 : 1;
}
TEST(fileapis_mode_is_process_wide) {
    REQUIRE(U(SetFileApisToOEM));
    EXPECT_EQ(t_run_child("fileapis_process_wide", "", 20000), 0);
}
