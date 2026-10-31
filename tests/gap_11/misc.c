/* gap_11: SetAclInformation, SetCachedSigningLevel, SetCalendarInfoA/W, SetComputerName*.
 * The state-changing APIs (computer name, calendar user overrides, signing cache) are only ever exercised
 * on argument-validation paths that fail before anything is modified. */
#include "g11.h"
#include <aclapi.h>

/* ---------------- SetAclInformation ---------------- */
#define SAI U(SetAclInformation)

static void mkacl(BYTE *buf, DWORD size, BYTE rev) {
    memset(buf, 0, size);
    InitializeAcl((PACL)buf, size, ACL_REVISION);
    buf[0] = rev;       /* raw revision byte */
}
static BYTE aclrev(BYTE *buf) { return buf[0]; }

TEST(gap11_acl_set_revision_raises) {
    REQUIRE(SAI);
    DWORD ar[2]; BYTE acl[64];
    mkacl(acl, sizeof acl, ACL_REVISION);
    ACL_REVISION_INFORMATION ri; ri.AclRevision = ACL_REVISION_DS;      /* 4 */
    SetLastError(0xBAD);
    EQ(SAI((PACL)acl, &ri, sizeof ri, AclRevisionInformation), TRUE);
    EQ(aclrev(acl), 4);
    ACL_REVISION_INFORMATION q; EQ(GetAclInformation((PACL)acl, &q, sizeof q, AclRevisionInformation), TRUE);
    EQ(q.AclRevision, 4);
}
TEST(gap11_acl_set_revision_same_ok) {
    REQUIRE(SAI);
    BYTE acl[64]; mkacl(acl, sizeof acl, 3);
    ACL_REVISION_INFORMATION ri; ri.AclRevision = 3;
    EQ(SAI((PACL)acl, &ri, sizeof ri, AclRevisionInformation), TRUE);
    EQ(aclrev(acl), 3);
}
TEST(gap11_acl_set_revision_cannot_lower) {
    REQUIRE(SAI);
    BYTE acl[64]; mkacl(acl, sizeof acl, 4);
    ACL_REVISION_INFORMATION ri;
    for (DWORD r = 0; r < 4; r++) {
        ri.AclRevision = r;
        SetLastError(0xBAD);
        EQ(SAI((PACL)acl, &ri, sizeof ri, AclRevisionInformation), FALSE);
        EQ(GetLastError(), ERROR_INVALID_PARAMETER);
        EQ(aclrev(acl), 4);
    }
}
TEST(gap11_acl_set_revision_step_up) {
    REQUIRE(SAI);
    BYTE acl[64]; mkacl(acl, sizeof acl, 2);
    ACL_REVISION_INFORMATION ri;
    ri.AclRevision = 3; EQ(SAI((PACL)acl, &ri, sizeof ri, AclRevisionInformation), TRUE); EQ(aclrev(acl), 3);
    ri.AclRevision = 2; EQ(SAI((PACL)acl, &ri, sizeof ri, AclRevisionInformation), FALSE); EQ(aclrev(acl), 3);
    ri.AclRevision = 4; EQ(SAI((PACL)acl, &ri, sizeof ri, AclRevisionInformation), TRUE); EQ(aclrev(acl), 4);
}
TEST(gap11_acl_set_revision_above_max_note) {
    REQUIRE(SAI);
    BYTE acl[64]; mkacl(acl, sizeof acl, 2);
    ACL_REVISION_INFORMATION ri; ri.AclRevision = 5;
    BOOL r = SAI((PACL)acl, &ri, sizeof ri, AclRevisionInformation);
    t_note("SetAclInformation(revision 5) -> %d err=%lu stored=%u", r, GetLastError(), aclrev(acl));
}
TEST(gap11_acl_set_length_too_small) {
    REQUIRE(SAI);
    BYTE acl[64]; mkacl(acl, sizeof acl, 2);
    ACL_REVISION_INFORMATION ri; ri.AclRevision = 4;
    for (DWORD len = 0; len < 4; len++) {
        SetLastError(0xBAD);
        CHECKF(SAI((PACL)acl, &ri, len, AclRevisionInformation) == FALSE, "len %lu", len);
        CHECKF(GetLastError() == ERROR_INSUFFICIENT_BUFFER, "len %lu err %lu", len, GetLastError());
        EQ(aclrev(acl), 2);
    }
}
TEST(gap11_acl_set_length_larger_ok) {
    REQUIRE(SAI);
    BYTE acl[64]; mkacl(acl, sizeof acl, 2);
    DWORD buf[4] = { 3, 0x11111111, 0x22222222, 0x33333333 };
    EQ(SAI((PACL)acl, buf, 16, AclRevisionInformation), TRUE);
    EQ(aclrev(acl), 3);
    EQ(SAI((PACL)acl, buf, 0xFFFFFFFF, AclRevisionInformation), TRUE);   /* length is only a lower bound */
    EQ(aclrev(acl), 3);
}
TEST(gap11_acl_set_revision_high_bytes_ignored) {
    REQUIRE(SAI);
    BYTE acl[64]; mkacl(acl, sizeof acl, 2);
    DWORD v = 0xABCDEF04;    /* only the low byte is stored */
    EQ(SAI((PACL)acl, &v, 4, AclRevisionInformation), TRUE);
    EQ(aclrev(acl), 4);
}
TEST(gap11_acl_set_wrong_info_class) {
    REQUIRE(SAI);
    BYTE acl[64]; mkacl(acl, sizeof acl, 2);
    ACL_SIZE_INFORMATION si; memset(&si, 0, sizeof si);
    ACL_REVISION_INFORMATION ri; ri.AclRevision = 4;
    DWORD classes[] = { 0, AclSizeInformation, 3, 4, 100, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof classes / sizeof classes[0]; i++) {
        SetLastError(0xBAD);
        BOOL r = SAI((PACL)acl, &ri, sizeof ri, (ACL_INFORMATION_CLASS)classes[i]);
        CHECKF(r == FALSE, "class %lu", classes[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "class %lu err %lu", classes[i], GetLastError());
    }
    SetLastError(0xBAD);
    EQ(SAI((PACL)acl, &si, sizeof si, AclSizeInformation), FALSE);    /* size info is read-only */
    EQ(GetLastError(), ERROR_INVALID_PARAMETER);
    EQ(aclrev(acl), 2);
}
TEST(gap11_acl_set_class_checked_before_length) {
    REQUIRE(SAI);
    BYTE acl[64]; mkacl(acl, sizeof acl, 2);
    DWORD v = 4;
    SetLastError(0xBAD);
    EQ(SAI((PACL)acl, &v, 0, AclSizeInformation), FALSE);
    EQ(GetLastError(), ERROR_INVALID_PARAMETER);    /* class error wins over the length error */
}
TEST(gap11_acl_set_bad_acl_revision) {
    REQUIRE(SAI);
    BYTE bads[] = { 0, 1, 5, 6, 0x7F, 0xFF };
    for (size_t i = 0; i < sizeof bads; i++) {
        BYTE acl[64]; mkacl(acl, sizeof acl, bads[i]);
        ACL_REVISION_INFORMATION ri; ri.AclRevision = 4;
        SetLastError(0xBAD);
        CHECKF(SAI((PACL)acl, &ri, sizeof ri, AclRevisionInformation) == FALSE, "acl rev %u", bads[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "acl rev %u err %lu", bads[i], GetLastError());
        CHECKF(aclrev(acl) == bads[i], "acl rev %u unchanged", bads[i]);
    }
}
TEST(gap11_acl_set_each_valid_acl_revision) {
    REQUIRE(SAI);
    for (BYTE r = 2; r <= 4; r++) {
        BYTE acl[64]; mkacl(acl, sizeof acl, r);
        ACL_REVISION_INFORMATION ri;
        ri.AclRevision = r;     EQ(SAI((PACL)acl, &ri, sizeof ri, AclRevisionInformation), TRUE);
        ri.AclRevision = 4;     EQ(SAI((PACL)acl, &ri, sizeof ri, AclRevisionInformation), TRUE);
        EQ(aclrev(acl), 4);
    }
}
TEST(gap11_acl_set_does_not_touch_rest_of_acl) {
    REQUIRE(SAI);
    BYTE acl[64]; mkacl(acl, sizeof acl, 2);
    BYTE before[64]; memcpy(before, acl, sizeof acl);
    ACL_REVISION_INFORMATION ri; ri.AclRevision = 3;
    EQ(SAI((PACL)acl, &ri, sizeof ri, AclRevisionInformation), TRUE);
    EQ(acl[0], 3);
    CHECK(!memcmp(acl + 1, before + 1, sizeof acl - 1));
    PACL p = (PACL)acl;
    EQ(p->AceCount, 0); EQ(p->AclSize, 64);
}
TEST(gap11_acl_set_with_aces_keeps_aces) {
    REQUIRE(SAI);
    BYTE acl[128]; mkacl(acl, sizeof acl, 2);
    SID_IDENTIFIER_AUTHORITY wa = SECURITY_WORLD_SID_AUTHORITY; PSID sid = NULL;
    AllocateAndInitializeSid(&wa, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &sid);
    BOOL ok = AddAccessAllowedAce((PACL)acl, ACL_REVISION, GENERIC_READ, sid);
    CHECK(ok);
    ACL_REVISION_INFORMATION ri; ri.AclRevision = 4;
    EQ(SAI((PACL)acl, &ri, sizeof ri, AclRevisionInformation), TRUE);
    EQ(((PACL)acl)->AceCount, 1);
    EQ(IsValidAcl((PACL)acl), TRUE);
    if (sid) FreeSid(sid);
}

/* ---------------- SetCachedSigningLevel ---------------- */
UDECL(BOOL, SetCachedSigningLevel, (PHANDLE, ULONG, ULONG, HANDLE));
#define SCSL UN(SetCachedSigningLevel)

TEST(gap11_signing_export_present) {
    REQUIRE(SCSL);
    CHECK(t_proc_k32("SetCachedSigningLevel") != NULL);
    CHECK(t_proc_kb("SetCachedSigningLevel") != NULL);
}
TEST(gap11_signing_invalid_target_handles) {
    REQUIRE(SCSL);
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x1234, (HANDLE)(ULONG_PTR)0xFFFFFFFC };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0xBAD);
        BOOL r = SCSL(NULL, 0, 0, bad[i]);
        CHECKF(r == FALSE, "target %p", bad[i]);
        t_note("SetCachedSigningLevel(NULL,0,0,%p) -> %d err=%lu", bad[i], r, GetLastError());
        CHECKF(GetLastError() != 0xBAD, "target %p: last error set", bad[i]);
    }
}
TEST(gap11_signing_invalid_flags_bad_handle) {
    REQUIRE(SCSL);
    ULONG fl[] = { 0, 1, 2, 4, 8, 0x80, 0x100, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof fl / sizeof fl[0]; i++) {
        SetLastError(0xBAD);
        CHECKF(SCSL(NULL, 0, fl[i], INVALID_HANDLE_VALUE) == FALSE, "flags 0x%lx", fl[i]);
        CHECKF(GetLastError() != 0xBAD && GetLastError() != 0, "flags 0x%lx err", fl[i]);
    }
}
TEST(gap11_signing_bad_source_handles) {
    REQUIRE(SCSL);
    HANDLE src[2] = { INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x4444 };
    SetLastError(0xBAD);
    EQ(SCSL(src, 2, 0, INVALID_HANDLE_VALUE), FALSE);
    NE(GetLastError(), 0xBAD);
    SetLastError(0xBAD);
    EQ(SCSL(src, 1, 0, NULL), FALSE);
}
TEST(gap11_signing_own_file_report) {
    REQUIRE(SCSL);
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "sign.bin");
    HANDLE h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE); if (h == INVALID_HANDLE_VALUE) return;
    DWORD n; WriteFile(h, "MZ", 2, &n, NULL);
    SetLastError(0xBAD);
    BOOL r = SCSL(NULL, 0, 0, h);
    t_note("SetCachedSigningLevel(no sources, flags 0, scratch file) -> %d err=%lu", r, GetLastError());
    CloseHandle(h);
    h = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    SetLastError(0xBAD);
    r = SCSL(&h, 1, 0xFFFFFFFF, h);
    t_note("SetCachedSigningLevel(self source, bogus flags) -> %d err=%lu", r, GetLastError());
    CloseHandle(h);
}
TEST(gap11_signing_closed_handle) {
    REQUIRE(SCSL);
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "sign2.bin");
    HANDLE h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CloseHandle(h);
    SetLastError(0xBAD);
    EQ(SCSL(NULL, 0, 0, h), FALSE);
    NE(GetLastError(), 0xBAD);
}

/* ---------------- SetCalendarInfo ---------------- */
#define SCW U(SetCalendarInfoW)
#define SCA U(SetCalendarInfoA)
#define EN_US 0x0409

TEST(gap11_cal_w_invalid_locale) {
    REQUIRE(SCW);
    LCID bad[] = { 0xDEAD, 0xFFFF, 0x7777, 0x12345678, 0xFFFFFFFF, 0x0000F000 };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0xBAD);
        CHECKF(SCW(bad[i], 1, 0x30, L"abc") == FALSE, "lcid 0x%lx", bad[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "lcid 0x%lx err %lu", bad[i], GetLastError());
    }
}
TEST(gap11_cal_a_invalid_locale) {
    REQUIRE(SCA);
    LCID bad[] = { 0xDEAD, 0xFFFF, 0x7777, 0x12345678 };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0xBAD);
        CHECKF(SCA(bad[i], 1, 0x30, "abc") == FALSE, "lcid 0x%lx", bad[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "lcid 0x%lx err %lu", bad[i], GetLastError());
    }
}
TEST(gap11_cal_w_null_data) {
    REQUIRE(SCW);
    LCID l[] = { EN_US, LOCALE_USER_DEFAULT, LOCALE_SYSTEM_DEFAULT, LOCALE_INVARIANT };
    for (size_t i = 0; i < 4; i++) {
        SetLastError(0xBAD);
        CHECKF(SCW(l[i], 1, 0x30, NULL) == FALSE, "lcid 0x%lx", l[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "lcid 0x%lx err %lu", l[i], GetLastError());
    }
}
TEST(gap11_cal_w_invalid_calendar) {
    REQUIRE(SCW);
    CALID bad[] = { 24, 100, 0x7FFF, 0xFFFF, 0xFFFFFFFF, 0x10000 };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0xBAD);
        CHECKF(SCW(EN_US, bad[i], 0x30, L"abc") == FALSE, "cal %lu", bad[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "cal %lu err %lu", bad[i], GetLastError());
    }
}
TEST(gap11_cal_a_invalid_calendar) {
    REQUIRE(SCA);
    CALID bad[] = { 24, 100, 0xFFFF, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0xBAD);
        CHECKF(SCA(EN_US, bad[i], 0x30, "abc") == FALSE, "cal %lu", bad[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "cal %lu err %lu", bad[i], GetLastError());
    }
}
/* CALTYPEs that are query-only: nothing can be set through them */
static const CALTYPE query_only[] = { 0, 1, 2, 3, 4, 7, 8, 13, 14, 0x15, 0x1B, 0x23, 0x2F, 0x31, 0x32, 0x1000, 0x80000030, 0x20000030, 0x10000030, 0x80000001, 0x20000002 };
TEST(gap11_cal_w_invalid_caltype) {
    REQUIRE(SCW);
    for (size_t i = 0; i < sizeof query_only / sizeof query_only[0]; i++) {
        SetLastError(0xBAD);
        CHECKF(SCW(EN_US, 1, query_only[i], L"x") == FALSE, "caltype 0x%lx", query_only[i]);
        CHECKF(GetLastError() == ERROR_INVALID_FLAGS, "caltype 0x%lx err %lu", query_only[i], GetLastError());
    }
}
TEST(gap11_cal_a_invalid_caltype) {
    REQUIRE(SCA);
    for (size_t i = 0; i < sizeof query_only / sizeof query_only[0]; i++) {
        SetLastError(0xBAD);
        BOOL r = SCA(EN_US, 1, query_only[i], "x");
        DWORD e = GetLastError();
        CHECKF(r == FALSE, "caltype 0x%lx", query_only[i]);
        /* the ANSI wrapper first derives a code page from the locale/CALTYPE */
        CHECKF(e == ERROR_INVALID_FLAGS || e == ERROR_INVALID_PARAMETER, "caltype 0x%lx err %lu", query_only[i], e);
        if (e != ERROR_INVALID_FLAGS) t_note("SetCalendarInfoA caltype 0x%lx -> err %lu", query_only[i], e);
    }
}
TEST(gap11_cal_w_invalid_two_digit_year_data) {
    REQUIRE(SCW);
    /* values outside 99..9999 (or not numbers) are refused before anything is stored */
    const WCHAR *bad[] = { L"abc", L"", L" ", L"0", L"9", L"98", L"-5", L"-100", L"10000", L"123456", L"99999", L"1e3x", L"x100" };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0xBAD);
        CHECKF(SCW(EN_US, 1, 0x30, bad[i]) == FALSE, "data '%ls'", bad[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "data '%ls' err %lu", bad[i], GetLastError());
    }
}
TEST(gap11_cal_a_invalid_two_digit_year_data) {
    REQUIRE(SCA);
    const char *bad[] = { "abc", "", " ", "0", "9", "98", "-5", "10000", "123456", "99999", "x100" };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0xBAD);
        CHECKF(SCA(EN_US, 1, 0x30, bad[i]) == FALSE, "data '%s'", bad[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "data '%s' err %lu", bad[i], GetLastError());
    }
}
TEST(gap11_cal_w_invalid_two_digit_year_other_locales) {
    REQUIRE(SCW);
    LCID l[] = { LOCALE_USER_DEFAULT, LOCALE_SYSTEM_DEFAULT, 0x0407, 0x040C, 0x0411, 0x0401 };
    for (size_t i = 0; i < sizeof l / sizeof l[0]; i++) {
        SetLastError(0xBAD);
        BOOL r = SCW(l[i], 1, 0x30, L"abc");
        t_note("SetCalendarInfoW(lcid 0x%lx, gregorian, 0x30, 'abc') -> %d err=%lu", l[i], r, GetLastError());
        CHECKF(r == FALSE, "lcid 0x%lx", l[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "lcid 0x%lx err %lu", l[i], GetLastError());
    }
}
TEST(gap11_cal_w_generic_calendar_ids_rejected) {
    REQUIRE(SCW);
    /* real calendar ids that en-US does not support are rejected the same way as unknown ones */
    CALID ids[] = { CAL_HIJRI, CAL_THAI, CAL_JAPAN, CAL_TAIWAN, CAL_KOREA };
    for (size_t i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        SetLastError(0xBAD);
        BOOL r = SCW(EN_US, ids[i], 0x30, L"abc");
        CHECKF(r == FALSE, "cal %lu", ids[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "cal %lu err %lu", ids[i], GetLastError());
    }
}
TEST(gap11_cal_w_use_cp_acp_flag_invalid_data) {
    REQUIRE(SCW);
    SetLastError(0xBAD);
    EQ(SCW(EN_US, 1, 0x30 | 0x40000000, L"abc"), FALSE);   /* bit 30 is masked out; data is still invalid */
    EQ(GetLastError(), ERROR_INVALID_PARAMETER);
}
TEST(gap11_cal_a_null_locale_neutral) {
    REQUIRE(SCA);
    SetLastError(0xBAD);
    EQ(SCA(LOCALE_NEUTRAL, 1, 0x30, "abc"), FALSE);
    t_note("SetCalendarInfoA(LOCALE_NEUTRAL) err=%lu", GetLastError());
}

/* ---------------- SetComputerName* (validation failures only) ---------------- */
#define SCNW U(SetComputerNameW)
#define SCNA U(SetComputerNameA)
#define SCNEW U(SetComputerNameExW)
#define SCNEA U(SetComputerNameExA)

static const WCHAR *bad_names_w[] = {
    L"", L" ", L"  ", L" LEADING", L"TRAILING ", L"AAAAAAAAAAAAAAAA", L"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
    L"bad\"name", L"bad/name", L"bad\\name", L"bad[name", L"bad]name", L"bad:name", L"bad|name", L"bad<name", L"bad>name",
    L"bad+name", L"bad=name", L"bad;name", L"bad,name", L"bad?name", L"\x01", L"bad\x01name", L"bad\x1f" L"x", L"bad\tname", L"bad\nname",
    L"/", L"\\", L"?", L"a\"", L"\"a", L"[]",
};
TEST(gap11_cname_w_invalid_names) {
    REQUIRE(SCNW);
    for (size_t i = 0; i < sizeof bad_names_w / sizeof bad_names_w[0]; i++) {
        SetLastError(0xBAD);
        CHECKF(SCNW(bad_names_w[i]) == FALSE, "name #%zu", i);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "name #%zu err %lu", i, GetLastError());
    }
}
TEST(gap11_cname_a_invalid_names) {
    REQUIRE(SCNA);
    const char *bad[] = { "", " ", " LEADING", "TRAILING ", "AAAAAAAAAAAAAAAA", "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
        "bad\"name", "bad/name", "bad\\name", "bad[name", "bad]name", "bad:name", "bad|name", "bad<name", "bad>name",
        "bad+name", "bad=name", "bad;name", "bad,name", "bad?name", "\x01", "bad\x1f" "x", "bad\tname" };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SetLastError(0xBAD);
        CHECKF(SCNA(bad[i]) == FALSE, "name #%zu", i);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "name #%zu err %lu", i, GetLastError());
    }
}
TEST(gap11_cname_a_length_boundary_rejected) {
    REQUIRE(SCNA);
    /* 16 and more characters never pass the A wrapper's length check (1..15) */
    char buf[300];
    for (int len = 16; len <= 256; len += (len < 20 ? 1 : 60)) {
        memset(buf, 'B', len); buf[len] = 0;
        SetLastError(0xBAD);
        CHECKF(SCNA(buf) == FALSE, "len %d", len);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "len %d err %lu", len, GetLastError());
    }
}
TEST(gap11_cname_w_length_boundary_rejected) {
    REQUIRE(SCNW);
    WCHAR buf[300];
    for (int len = 16; len <= 256; len += (len < 20 ? 1 : 60)) {
        for (int i = 0; i < len; i++) buf[i] = L'B';
        buf[len] = 0;
        SetLastError(0xBAD);
        CHECKF(SCNW(buf) == FALSE, "len %d", len);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "len %d err %lu", len, GetLastError());
    }
}
TEST(gap11_cname_w_invalid_char_in_otherwise_valid_name) {
    REQUIRE(SCNW);
    const WCHAR bad[] = L"\"/\\[]:|<>+=;,?";
    for (size_t i = 0; bad[i]; i++) {
        WCHAR n[8] = { L'A', L'B', bad[i], L'C', 0 };
        SetLastError(0xBAD);
        CHECKF(SCNW(n) == FALSE, "char 0x%x", bad[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "char 0x%x err %lu", bad[i], GetLastError());
    }
    for (WCHAR c = 1; c < 0x20; c++) {
        WCHAR n[8] = { L'A', L'B', c, L'C', 0 };
        SetLastError(0xBAD);
        CHECKF(SCNW(n) == FALSE, "control 0x%x", c);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "control 0x%x err %lu", c, GetLastError());
    }
}
TEST(gap11_cname_ex_w_unsupported_name_types) {
    REQUIRE(SCNEW);
    /* only NetBIOS (4), DNS host name (5) and DNS domain (6) of the "physical" types can be set */
    DWORD types[] = { 0, 1, 2, 3, 7, 8, 9, 100, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof types / sizeof types[0]; i++) {
        SetLastError(0xBAD);
        CHECKF(SCNEW((COMPUTER_NAME_FORMAT)types[i], L"K32TNAME") == FALSE, "type %lu", types[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "type %lu err %lu", types[i], GetLastError());
    }
}
TEST(gap11_cname_ex_a_unsupported_name_types) {
    REQUIRE(SCNEA);
    DWORD types[] = { 0, 1, 2, 3, 7, 8, 9, 100, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof types / sizeof types[0]; i++) {
        SetLastError(0xBAD);
        CHECKF(SCNEA((COMPUTER_NAME_FORMAT)types[i], "K32TNAME") == FALSE, "type %lu", types[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "type %lu err %lu", types[i], GetLastError());
    }
}
TEST(gap11_cname_ex_w_null_buffer) {
    REQUIRE(SCNEW);
    if (t_is_wine()) { t_skip("emulator dereferences a NULL name"); return; }
    DWORD types[] = { 0, 3, 4, 5, 6, 7, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof types / sizeof types[0]; i++) {
        SetLastError(0xBAD);
        CHECKF(SCNEW((COMPUTER_NAME_FORMAT)types[i], NULL) == FALSE, "type %lu", types[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "type %lu err %lu", types[i], GetLastError());
    }
}
TEST(gap11_cname_ex_a_null_buffer) {
    REQUIRE(SCNEA);
    /* NULL is treated as an empty string by the ANSI wrapper; unsupported type keeps it harmless */
    SetLastError(0xBAD);
    EQ(SCNEA((COMPUTER_NAME_FORMAT)0, NULL), FALSE);
    t_note("SetComputerNameExA(type 0, NULL) err=%lu", GetLastError());
    SetLastError(0xBAD);
    EQ(SCNEA((COMPUTER_NAME_FORMAT)7, NULL), FALSE);
}
TEST(gap11_cname_ex_w_invalid_chars_all_types) {
    REQUIRE(SCNEW);
    const WCHAR *bad[] = { L"bad/name", L"bad\\name", L"bad[name", L"bad]name", L"bad:name", L"bad|name", L"bad<name", L"bad>name",
        L"bad+name", L"bad=name", L"bad;name", L"bad,name", L"bad?name", L"bad\"name", L"bad\x01name", L" LEADING", L"TRAILING " };
    DWORD types[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
    for (size_t t = 0; t < sizeof types / sizeof types[0]; t++)
        for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
            SetLastError(0xBAD);
            CHECKF(SCNEW((COMPUTER_NAME_FORMAT)types[t], bad[i]) == FALSE, "type %lu name #%zu", types[t], i);
            CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "type %lu name #%zu err %lu", types[t], i, GetLastError());
        }
}
TEST(gap11_cname_ex_a_invalid_chars_all_types) {
    REQUIRE(SCNEA);
    const char *bad[] = { "bad/name", "bad\\name", "bad[name", "bad]name", "bad:name", "bad|name", "bad<name", "bad>name",
        "bad+name", "bad=name", "bad;name", "bad,name", "bad?name", "bad\"name", " LEADING", "TRAILING " };
    DWORD types[] = { 0, 3, 4, 5, 6, 7 };
    for (size_t t = 0; t < sizeof types / sizeof types[0]; t++)
        for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
            SetLastError(0xBAD);
            CHECKF(SCNEA((COMPUTER_NAME_FORMAT)types[t], bad[i]) == FALSE, "type %lu name #%zu", types[t], i);
            CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "type %lu name #%zu err %lu", types[t], i, GetLastError());
        }
}
TEST(gap11_cname_state_unchanged_after_failures) {
    REQUIRE(SCNW); REQUIRE(SCNA); REQUIRE(SCNEW);
    WCHAR before[64], after[64]; DWORD n1 = 64, n2 = 64;
    CHECK(GetComputerNameW(before, &n1));
    SCNW(L"bad/name"); SCNA("bad:name"); SCNEW((COMPUTER_NAME_FORMAT)0, L"K32TNAME"); SCNW(L"");
    CHECK(GetComputerNameW(after, &n2));
    EXPECT_WSTR(before, after);
}
