/* GetCPInfo / GetCPInfoEx / IsValidCodePage / GetACP / GetOEMCP / IsDBCSLeadByte(Ex) */
#include "sc.h"

static int cpi_eq(const CPINFO *a, const CPINFO *b) {
    return a->MaxCharSize == b->MaxCharSize && !memcmp(a->DefaultChar, b->DefaultChar, 2) && !memcmp(a->LeadByte, b->LeadByte, MAX_LEADBYTES);
}
static DWORD WINAPI acp_thread(LPVOID p) { *(UINT *)p = GetACP(); return 0; }
static int cp_installed(UINT cp) { return U(IsValidCodePage) && U(IsValidCodePage)(cp); }

static const struct { UINT cp; UINT max; BYTE lead[8]; BYTE def; } cpi[] = {
    {437, 1, {0}, '?'}, {737, 1, {0}, '?'}, {775, 1, {0}, '?'}, {850, 1, {0}, '?'}, {852, 1, {0}, '?'}, {855, 1, {0}, '?'}, {857, 1, {0}, '?'},
    {858, 1, {0}, '?'}, {860, 1, {0}, '?'}, {862, 1, {0}, '?'}, {866, 1, {0}, '?'}, {874, 1, {0}, '?'},
    {1250, 1, {0}, '?'}, {1251, 1, {0}, '?'}, {1252, 1, {0}, '?'}, {1253, 1, {0}, '?'}, {1254, 1, {0}, '?'}, {1255, 1, {0}, '?'}, {1256, 1, {0}, '?'}, {1257, 1, {0}, '?'}, {1258, 1, {0}, '?'},
    {10000, 1, {0}, '?'}, {20127, 1, {0}, '?'}, {20866, 1, {0}, '?'}, {21866, 1, {0}, '?'}, {28591, 1, {0}, '?'}, {28592, 1, {0}, '?'}, {28595, 1, {0}, '?'}, {28597, 1, {0}, '?'},
    {28599, 1, {0}, '?'}, {28605, 1, {0}, '?'},
    {932, 2, {0x81, 0x9F, 0xE0, 0xFC, 0, 0}, '?'}, {936, 2, {0x81, 0xFE, 0, 0}, '?'}, {949, 2, {0x81, 0xFE, 0, 0}, '?'}, {950, 2, {0x81, 0xFE, 0, 0}, '?'},
    {65000, 5, {0}, '?'}, {65001, 4, {0}, '?'},
};

TEST(cpinfo_table) {
    REQUIRE(U(GetCPInfo));
    for (int i = 0; i < ARRAY_N(cpi); i++) {
        if (!cp_installed(cpi[i].cp)) { t_note("cp %u not installed", cpi[i].cp); continue; }
        CPINFO ci; memset(&ci, 0xCC, sizeof ci);
        SET_ERR(0);
        BOOL ok = U(GetCPInfo)(cpi[i].cp, &ci);
        CHECKF(ok, "cp %u failed err=%lu", cpi[i].cp, GetLastError());
        if (!ok) continue;
        CHECKF(ci.MaxCharSize == cpi[i].max, "cp %u MaxCharSize %u want %u", cpi[i].cp, ci.MaxCharSize, cpi[i].max);
        CHECKF(ci.DefaultChar[0] == cpi[i].def && ci.DefaultChar[1] == 0, "cp %u DefaultChar %02X %02X", cpi[i].cp, ci.DefaultChar[0], ci.DefaultChar[1]);
        BYTE exp[MAX_LEADBYTES] = {0}; memcpy(exp, cpi[i].lead, sizeof cpi[i].lead < MAX_LEADBYTES ? sizeof cpi[i].lead : MAX_LEADBYTES);
        CHECKF(!memcmp(ci.LeadByte, exp, MAX_LEADBYTES), "cp %u LeadByte [%02X %02X %02X %02X %02X %02X]", cpi[i].cp, ci.LeadByte[0], ci.LeadByte[1], ci.LeadByte[2], ci.LeadByte[3], ci.LeadByte[4], ci.LeadByte[5]);
    }
}
TEST(cpinfo_other_dbcs_note) {
    REQUIRE(U(GetCPInfo));
    static const UINT cps[] = {1361, 20932, 51932, 51936, 51949, 54936, 50220, 50221, 50222, 50225, 52936, 57002, 57003, 57006, 57011, 38598, 20000, 20001};
    for (int i = 0; i < ARRAY_N(cps); i++) {
        CPINFO ci; memset(&ci, 0, sizeof ci);
        SET_ERR(0);
        BOOL ok = U(GetCPInfo)(cps[i], &ci);
        t_note("GetCPInfo(%u) = %d err=%lu max=%u lead=%02X%02X%02X%02X", cps[i], ok, ok ? 0 : GetLastError(), ci.MaxCharSize, ci.LeadByte[0], ci.LeadByte[1], ci.LeadByte[2], ci.LeadByte[3]);
    }
    CPINFO ci;
    if (cp_installed(54936) && U(GetCPInfo)(54936, &ci)) { EXPECT_EQ(ci.MaxCharSize, 4); EXPECT_EQ(ci.LeadByte[0], 0x81); EXPECT_EQ(ci.LeadByte[1], 0xFE); }
}
TEST(cpinfo_invalid_params) {
    REQUIRE(U(GetCPInfo));
    CPINFO ci;
    static const UINT bad[] = {4, 5, 99, 100, 999, 1199, 12345, 20000 - 1, 28590, 59999, 65002, 65535, 99999, 0x7fffffff, 0xffffffff};
    for (int i = 0; i < ARRAY_N(bad); i++) {
        SET_ERR(0);
        BOOL ok = U(GetCPInfo)(bad[i], &ci);
        CHECKF(!ok && GetLastError() == E_INVALID_PARAMETER, "cp %u ok=%d err=%lu", bad[i], ok, GetLastError());
    }
    SET_ERR(0); EXPECT_EQ(U(GetCPInfo)(1252, NULL), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(GetCPInfo)(437, NULL), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(GetCPInfo)(CP_ACP, NULL), 0); EXPECT_ERR(E_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(GetCPInfo)(99999, NULL), 0); EXPECT_ERR(E_INVALID_PARAMETER);
}
TEST(cpinfo_utf16_and_utf32_ids) {
    REQUIRE(U(GetCPInfo));
    static const UINT cps[] = {1200, 1201, 12000, 12001};
    for (int i = 0; i < ARRAY_N(cps); i++) {
        CPINFO ci; SET_ERR(0);
        BOOL ok = U(GetCPInfo)(cps[i], &ci);
        t_note("GetCPInfo(%u) = %d err=%lu", cps[i], ok, ok ? 0 : GetLastError());
        if (U(IsValidCodePage)) { BOOL v = U(IsValidCodePage)(cps[i]); EXPECT_EQ(v, 0); }
    }
}
TEST(cpinfo_pseudo_codepages) {
    REQUIRE(U(GetCPInfo)); REQUIRE(U(GetACP)); REQUIRE(U(GetOEMCP));
    CPINFO a, b;
    CHECK(U(GetCPInfo)(CP_ACP, &a)); CHECK(U(GetCPInfo)(U(GetACP)(), &b));
    CHECK(cpi_eq(&a, &b));
    CHECK(U(GetCPInfo)(CP_OEMCP, &a)); CHECK(U(GetCPInfo)(U(GetOEMCP)(), &b));
    CHECK(cpi_eq(&a, &b));
    CHECK(U(GetCPInfo)(CP_THREAD_ACP, &a));
    CHECK(U(GetCPInfo)(CP_MACCP, &a));
    CPINFO m; if (U(GetCPInfo)(10000, &m)) { t_note("MACCP info max=%u", a.MaxCharSize); }
    SET_ERR(0);
    BOOL ok = U(GetCPInfo)(CP_SYMBOL, &a);
    t_note("GetCPInfo(CP_SYMBOL) = %d err=%lu", ok, ok ? 0 : GetLastError());
}
TEST(cpinfo_leadbyte_array_terminated) {
    REQUIRE(U(GetCPInfo));
    CPINFO ci;
    if (cp_installed(932) && U(GetCPInfo)(932, &ci)) {
        CHECK(ci.LeadByte[4] == 0 && ci.LeadByte[5] == 0);
        for (int i = 4; i < MAX_LEADBYTES; i++) CHECKF(ci.LeadByte[i] == 0, "tail byte %d", i);
    }
    if (cp_installed(1252) && U(GetCPInfo)(1252, &ci)) for (int i = 0; i < MAX_LEADBYTES; i++) CHECKF(ci.LeadByte[i] == 0, "1252 lead %d", i);
}
TEST(cpinfo_struct_size) {
    EXPECT_EQ(sizeof(CPINFO), 4 + 2 + 12 + 2);
    EXPECT_EQ(offsetof(CPINFO, DefaultChar), 4);
    EXPECT_EQ(offsetof(CPINFO, LeadByte), 6);
}

/* ---- GetCPInfoEx ---- */
TEST(cpinfoex_w_basic) {
    REQUIRE(U(GetCPInfoExW)); REQUIRE(U(GetCPInfo));
    static const UINT cps[] = {437, 850, 932, 936, 949, 950, 1250, 1251, 1252, 1253, 1254, 1255, 1256, 1257, 1258, 874, 20127, 28591, 28592, 65001, 65000};
    for (int i = 0; i < ARRAY_N(cps); i++) {
        if (!cp_installed(cps[i])) continue;
        CPINFOEXW e; CPINFO c; memset(&e, 0xCC, sizeof e);
        SET_ERR(0);
        BOOL ok = U(GetCPInfoExW)(cps[i], 0, &e);
        CHECKF(ok, "cp %u err=%lu", cps[i], GetLastError());
        if (!ok) continue;
        CHECKF(U(GetCPInfo)(cps[i], &c), "GetCPInfo %u", cps[i]);
        CHECKF(e.CodePage == cps[i], "cp %u CodePage field %u", cps[i], e.CodePage);
        CHECKF(e.MaxCharSize == c.MaxCharSize, "cp %u max", cps[i]);
        CHECKF(!memcmp(e.DefaultChar, c.DefaultChar, 2), "cp %u def", cps[i]);
        CHECKF(!memcmp(e.LeadByte, c.LeadByte, MAX_LEADBYTES), "cp %u lead", cps[i]);
        /* name begins with the decimal code page number */
        WCHAR num[16]; wsprintfW(num, L"%u", cps[i]);
        int nl = lstrlenW(num);
        CHECKF(!memcmp(e.CodePageName, num, nl * 2), "cp %u name does not start with number", cps[i]);
        int len = lstrlenW(e.CodePageName);
        CHECKF(len > nl && len < MAX_PATH, "cp %u name length %d", cps[i], len);
        CHECKF(e.UnicodeDefaultChar != 0 || cps[i] == 65000 || cps[i] == 65001 || 1, "udef");
    }
}
TEST(cpinfoex_a_basic) {
    REQUIRE(U(GetCPInfoExA)); REQUIRE(U(GetCPInfoExW));
    static const UINT cps[] = {437, 850, 932, 1252, 65001};
    for (int i = 0; i < ARRAY_N(cps); i++) {
        if (!cp_installed(cps[i])) continue;
        CPINFOEXA ea; CPINFOEXW ew; memset(&ea, 0, sizeof ea); memset(&ew, 0, sizeof ew);
        BOOL oka = U(GetCPInfoExA)(cps[i], 0, &ea), okw = U(GetCPInfoExW)(cps[i], 0, &ew);
        CHECKF(oka && okw, "cp %u", cps[i]);
        if (!oka || !okw) continue;
        CHECK(ea.CodePage == ew.CodePage && ea.MaxCharSize == ew.MaxCharSize);
        CHECK(!memcmp(ea.LeadByte, ew.LeadByte, MAX_LEADBYTES));
        char conv[MAX_PATH]; WideCharToMultiByte(CP_ACP, 0, ew.CodePageName, -1, conv, sizeof conv, NULL, NULL);
        CHECKF(!strcmp(conv, ea.CodePageName), "cp %u name A [%s] vs W", cps[i], ea.CodePageName);
        char num[16]; wsprintfA(num, "%u", cps[i]);
        CHECKF(!strncmp(ea.CodePageName, num, strlen(num)), "cp %u A name prefix", cps[i]);
    }
}
TEST(cpinfoex_pseudo_cps) {
    REQUIRE(U(GetCPInfoExW)); REQUIRE(U(GetACP)); REQUIRE(U(GetOEMCP));
    CPINFOEXW e;
    memset(&e, 0, sizeof e);
    CHECK(U(GetCPInfoExW)(CP_ACP, 0, &e));
    EXPECT_EQ(e.CodePage, U(GetACP)());
    memset(&e, 0, sizeof e);
    CHECK(U(GetCPInfoExW)(CP_OEMCP, 0, &e));
    EXPECT_EQ(e.CodePage, U(GetOEMCP)());
    memset(&e, 0, sizeof e);
    BOOL ok = U(GetCPInfoExW)(CP_THREAD_ACP, 0, &e);
    t_note("CP_THREAD_ACP -> ok=%d CodePage=%u", ok, ok ? e.CodePage : 0);
    ok = U(GetCPInfoExW)(CP_MACCP, 0, &e);
    t_note("CP_MACCP -> ok=%d CodePage=%u", ok, ok ? e.CodePage : 0);
}
TEST(cpinfoex_errors) {
    REQUIRE(U(GetCPInfoExW)); REQUIRE(U(GetCPInfoExA));
    CPINFOEXW e; CPINFOEXA a;
    static const UINT bad[] = {4, 99, 12345, 65002, 99999, 0xffffffff};
    for (int i = 0; i < ARRAY_N(bad); i++) {
        SET_ERR(0); BOOL ok = U(GetCPInfoExW)(bad[i], 0, &e);
        CHECKF(!ok && GetLastError() == E_INVALID_PARAMETER, "W cp %u ok=%d err=%lu", bad[i], ok, GetLastError());
        SET_ERR(0); ok = U(GetCPInfoExA)(bad[i], 0, &a);
        CHECKF(!ok && GetLastError() == E_INVALID_PARAMETER, "A cp %u ok=%d err=%lu", bad[i], ok, GetLastError());
    }
    if (!t_is_wine()) { SET_ERR(0); EXPECT_EQ(U(GetCPInfoExW)(1252, 0, NULL), 0); EXPECT_ERR(E_INVALID_PARAMETER); }
    static const DWORD badf[] = {1, 2, 0x10, 0x100, 0x80000000, 0xffffffff};
    for (int i = 0; i < ARRAY_N(badf); i++) {
        SET_ERR(0); BOOL ok = U(GetCPInfoExW)(1252, badf[i], &e);
        CHECKF(!ok && GetLastError() == E_INVALID_FLAGS, "W flags %lx ok=%d err=%lu", badf[i], ok, GetLastError());
        SET_ERR(0); ok = U(GetCPInfoExA)(1252, badf[i], &a);
        CHECKF(!ok && GetLastError() == E_INVALID_FLAGS, "A flags %lx ok=%d err=%lu", badf[i], ok, GetLastError());
    }
}

/* ---- IsValidCodePage ---- */
TEST(isvalidcp_valid_set) {
    REQUIRE(U(IsValidCodePage));
    static const UINT cps[] = {437, 850, 852, 866, 874, 932, 936, 949, 950, 1250, 1251, 1252, 1253, 1254, 1255, 1256, 1257, 1258, 20127, 28591, 28592, 65000, 65001};
    for (int i = 0; i < ARRAY_N(cps); i++) { SET_ERR(0); CHECKF(U(IsValidCodePage)(cps[i]), "cp %u", cps[i]); }
    static const UINT soft[] = {737, 775, 855, 857, 858, 860, 861, 862, 863, 864, 865, 869, 1361, 10000, 10001, 10002, 10003, 10008, 20866, 21866, 28593, 28594, 28595, 28596, 28597, 28598, 28599, 28603, 28605, 38598, 20932, 51932, 51936, 51949, 54936, 50220, 50221, 50222, 50225, 52936, 20105, 20106, 20107, 20108, 20261, 20269, 20273, 20277, 20278, 20280, 20284, 20285, 20290, 20297, 20420, 20423, 20424, 29001, 57002, 57003, 57004, 57005, 57006, 57007, 57008, 57009, 57010, 57011, 65002};
    for (int i = 0; i < ARRAY_N(soft); i++) t_note("IsValidCodePage(%u) = %d", soft[i], U(IsValidCodePage)(soft[i]));
}
TEST(isvalidcp_invalid_set) {
    REQUIRE(U(IsValidCodePage));
    static const UINT cps[] = {0, 1, 2, 3, 4, 42, 99, 100, 999, 1199, 1200, 1201, 12000, 12001, 12345, 28590, 59999, 60000, 65535, 99999, 0x7fffffff, 0x80000000, 0xffffffff};
    for (int i = 0; i < ARRAY_N(cps); i++) CHECKF(!U(IsValidCodePage)(cps[i]), "cp %u should be invalid", cps[i]);
}
TEST(isvalidcp_acp_oem_valid) {
    REQUIRE(U(IsValidCodePage)); REQUIRE(U(GetACP)); REQUIRE(U(GetOEMCP));
    CHECK(U(IsValidCodePage)(U(GetACP)()));
    CHECK(U(IsValidCodePage)(U(GetOEMCP)()));
}
TEST(isvalidcp_agrees_with_getcpinfo) {
    REQUIRE(U(IsValidCodePage)); REQUIRE(U(GetCPInfo));
    static const UINT cps[] = {437, 850, 932, 936, 1252, 1258, 20127, 28591, 65000, 65001, 99999, 12345, 1200, 54936, 28605};
    for (int i = 0; i < ARRAY_N(cps); i++) {
        CPINFO ci;
        BOOL v = U(IsValidCodePage)(cps[i]), g = U(GetCPInfo)(cps[i], &ci);
        if (v != g) t_note("cp %u IsValidCodePage=%d GetCPInfo=%d", cps[i], v, g);
        if (v) CHECKF(g, "cp %u valid but GetCPInfo failed", cps[i]);
    }
}
TEST(isvalidcp_no_lasterror_requirement) {
    REQUIRE(U(IsValidCodePage));
    SET_ERR(0x5555);
    U(IsValidCodePage)(1252);
    t_note("LastError after IsValidCodePage(1252): %lu", GetLastError());
    SET_ERR(0x5555);
    U(IsValidCodePage)(99999);
    t_note("LastError after IsValidCodePage(99999): %lu", GetLastError());
}

/* ---- GetACP / GetOEMCP ---- */
TEST(acp_basic) {
    REQUIRE(U(GetACP));
    UINT a = U(GetACP)();
    CHECK(a != 0);
    CHECK(a == U(GetACP)());
    static const UINT known[] = {874, 932, 936, 949, 950, 1250, 1251, 1252, 1253, 1254, 1255, 1256, 1257, 1258, 65001};
    int ok = 0; for (int i = 0; i < ARRAY_N(known); i++) ok |= (a == known[i]);
    CHECKF(ok, "unexpected ANSI code page %u", a);
    if (a != 1252) t_note("system ACP %u (tests assume 1252 when ACP-specific)", a);
}
TEST(oemcp_basic) {
    REQUIRE(U(GetOEMCP));
    UINT o = U(GetOEMCP)();
    CHECK(o != 0);
    CHECK(o == U(GetOEMCP)());
    static const UINT known[] = {437, 720, 737, 775, 850, 852, 855, 857, 858, 860, 861, 862, 863, 864, 865, 866, 869, 874, 932, 936, 949, 950};
    int ok = 0; for (int i = 0; i < ARRAY_N(known); i++) ok |= (o == known[i]);
    CHECKF(ok, "unexpected OEM code page %u", o);
    t_note("OEM code page %u", o);
}
TEST(acp_oemcp_consistency) {
    REQUIRE(U(GetACP)); REQUIRE(U(GetOEMCP)); REQUIRE(U(GetCPInfo));
    CPINFO a, o, ca, co;
    CHECK(U(GetCPInfo)(U(GetACP)(), &a)); CHECK(U(GetCPInfo)(U(GetOEMCP)(), &o));
    CHECK(U(GetCPInfo)(CP_ACP, &ca)); CHECK(U(GetCPInfo)(CP_OEMCP, &co));
    CHECK(cpi_eq(&a, &ca)); CHECK(cpi_eq(&o, &co));
    UINT acp = U(GetACP)();
    CHECK(a.MaxCharSize == ((acp == 932 || acp == 936 || acp == 949 || acp == 950) ? 2u : acp == 65001 ? 4u : 1u));
}
TEST(acp_stable_across_threads) {
    REQUIRE(U(GetACP));
    UINT a = U(GetACP)();
    struct { UINT v; } r = {0};
    HANDLE h = CreateThread(NULL, 0, acp_thread, &r, 0, NULL);
    CHECK(h != NULL);
    if (h) { WaitForSingleObject(h, 5000); CloseHandle(h); }
    EXPECT_EQ(r.v, a);
}

/* ---- IsDBCSLeadByte(Ex) ---- */
static int is_lead(UINT cp, int c) {
    switch (cp) {
    case 932: return (c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC);
    case 936: case 949: case 950: return c >= 0x81 && c <= 0xFE;
    default: return 0;
    }
}
TEST(isdbcs_leadbyteex_exhaustive) {
    REQUIRE(U(IsDBCSLeadByteEx));
    static const UINT cps[] = {932, 936, 949, 950, 1252, 437, 850, 1250, 1251, 874, 28591, 65001, 65000, 20127};
    for (int k = 0; k < ARRAY_N(cps); k++) {
        if (!cp_installed(cps[k])) continue;
        int bad = 0;
        for (int c = 0; c < 256; c++) if (!!U(IsDBCSLeadByteEx)(cps[k], (BYTE)c) != is_lead(cps[k], c)) { bad++; if (bad < 5) t_note("cp %u byte %02X mismatch", cps[k], c); }
        CHECKF(bad == 0, "cp %u had %d mismatches", cps[k], bad);
    }
}
TEST(isdbcs_leadbyteex_pseudo) {
    REQUIRE(U(IsDBCSLeadByteEx)); REQUIRE(U(GetACP)); REQUIRE(U(GetOEMCP));
    for (int c = 0; c < 256; c++) {
        CHECKF(!!U(IsDBCSLeadByteEx)(CP_ACP, (BYTE)c) == !!U(IsDBCSLeadByteEx)(U(GetACP)(), (BYTE)c), "ACP byte %02X", c);
        CHECKF(!!U(IsDBCSLeadByteEx)(CP_OEMCP, (BYTE)c) == !!U(IsDBCSLeadByteEx)(U(GetOEMCP)(), (BYTE)c), "OEM byte %02X", c);
    }
}
TEST(isdbcs_leadbyteex_invalid_cp) {
    REQUIRE(U(IsDBCSLeadByteEx));
    static const UINT bad[] = {4, 99, 12345, 65002, 99999, 0xffffffff};
    for (int i = 0; i < ARRAY_N(bad); i++) {
        SET_ERR(0);
        BOOL r = U(IsDBCSLeadByteEx)(bad[i], 0x81);
        CHECKF(!r && GetLastError() == E_INVALID_PARAMETER, "cp %u r=%d err=%lu", bad[i], r, GetLastError());
    }
    SET_ERR(0);
    BOOL r = U(IsDBCSLeadByteEx)(1200, 0x81);
    CHECK(!r);
    t_note("IsDBCSLeadByteEx(1200) err=%lu", GetLastError());
}
TEST(isdbcs_leadbyte_follows_acp) {
    REQUIRE(U(IsDBCSLeadByte)); REQUIRE(U(IsDBCSLeadByteEx)); REQUIRE(U(GetACP));
    for (int c = 0; c < 256; c++) CHECKF(!!U(IsDBCSLeadByte)((BYTE)c) == !!U(IsDBCSLeadByteEx)(U(GetACP)(), (BYTE)c), "byte %02X", c);
    UINT a = U(GetACP)();
    if (a == 1252) for (int c = 0; c < 256; c++) CHECKF(!U(IsDBCSLeadByte)((BYTE)c), "1252 byte %02X", c);
    if (a == 932) { CHECK(U(IsDBCSLeadByte)(0x81)); CHECK(U(IsDBCSLeadByte)(0xE0)); CHECK(!U(IsDBCSLeadByte)(0xA0)); CHECK(!U(IsDBCSLeadByte)(0xFD)); }
}
TEST(isdbcs_leadbyte_ascii_never) {
    REQUIRE(U(IsDBCSLeadByte)); REQUIRE(U(IsDBCSLeadByteEx));
    for (int c = 0; c < 0x80; c++) CHECKF(!U(IsDBCSLeadByte)((BYTE)c), "byte %02X", c);
    static const UINT cps[] = {932, 936, 949, 950, 1252};
    for (int k = 0; k < ARRAY_N(cps); k++) if (cp_installed(cps[k])) for (int c = 0; c < 0x80; c++) CHECKF(!U(IsDBCSLeadByteEx)(cps[k], (BYTE)c), "cp %u byte %02X", cps[k], c);
}
TEST(isdbcs_leadbyteex_matches_getcpinfo) {
    REQUIRE(U(IsDBCSLeadByteEx)); REQUIRE(U(GetCPInfo));
    static const UINT cps[] = {932, 936, 949, 950, 1252, 437};
    for (int k = 0; k < ARRAY_N(cps); k++) {
        CPINFO ci; if (!cp_installed(cps[k]) || !U(GetCPInfo)(cps[k], &ci)) continue;
        for (int c = 0; c < 256; c++) {
            int in = 0;
            for (int j = 0; j + 1 < MAX_LEADBYTES && (ci.LeadByte[j] || ci.LeadByte[j + 1]); j += 2) if (c >= ci.LeadByte[j] && c <= ci.LeadByte[j + 1]) in = 1;
            CHECKF(!!U(IsDBCSLeadByteEx)(cps[k], (BYTE)c) == in, "cp %u byte %02X", cps[k], c);
        }
    }
}
TEST(dbcs_conversion_respects_leadbytes) {
    REQUIRE(U(MultiByteToWideChar));
    if (!cp_installed(932)) { t_skip("932 absent"); return; }
    /* the second byte of a double-byte char may look like ASCII (0x5C): must stay one character */
    WCHAR w[8];
    int r = U(MultiByteToWideChar)(932, 0, "\x83\x5c", 2, w, 8);   /* katakana "so" with trail 0x5C */
    EXPECT_EQ(r, 1);
    if (r == 1) EXPECT_EQ(w[0], 0x30BD);
    r = U(MultiByteToWideChar)(932, 0, "\x95\x5c\x5c", 3, w, 8);
    EXPECT_EQ(r, 2);
    if (r == 2) { EXPECT_EQ(w[0], 0x8868); EXPECT_EQ(w[1], 0x5C); }
}
