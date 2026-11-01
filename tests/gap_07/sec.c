/* gap7: IsValidSid/Acl/SecurityDescriptor, IsWellKnownSid, MapGenericMask, MakeAbsoluteSD, MakeSelfRelativeSD,
 *       IsTokenRestricted, OpenThreadToken, Object*AuditAlarmW */
#include "t.h"
#include <sddl.h>

#ifndef ERROR_INVALID_ACL
#define ERROR_INVALID_ACL 1336
#endif
#ifndef ERROR_INVALID_SID
#define ERROR_INVALID_SID 1337
#endif
#ifndef ERROR_INVALID_SECURITY_DESCR
#define ERROR_INVALID_SECURITY_DESCR 1338
#endif

/* ---------- IsValidSid ---------- */
TEST(gap7_isvalidsid_good) {
    REQUIRE(U(IsValidSid));
    BYTE buf[SECURITY_MAX_SID_SIZE]; DWORD n = sizeof buf;
    CHECK(CreateWellKnownSid(WinWorldSid, NULL, buf, &n));
    SET_ERR(0);
    CHECK(U(IsValidSid)(buf));
    /* hand-built S-1-5-32-544 */
    SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
    PSID admins = NULL;
    CHECK(AllocateAndInitializeSid(&nt, 2, 32, 544, 0, 0, 0, 0, 0, 0, &admins));
    CHECK(U(IsValidSid)(admins));
    FreeSid(admins);
    /* SIDs with 0 and with 15 subauthorities */
    BYTE sid[8 + 4 * 15] = { 1, 0, 0, 0, 0, 0, 0, 5 };
    sid[1] = 0; CHECK(U(IsValidSid)(sid));
    sid[1] = SID_MAX_SUB_AUTHORITIES; CHECK(U(IsValidSid)(sid));
}

TEST(gap7_isvalidsid_bad) {
    REQUIRE(U(IsValidSid));
    BYTE sid[8 + 4 * 16] = { 1, 1, 0, 0, 0, 0, 0, 5, 0x20, 0, 0, 0 };
    CHECK(U(IsValidSid)(sid));
    for (int rev = 0; rev < 256; rev++) {
        sid[0] = (BYTE)rev;
        BOOL r = U(IsValidSid)(sid);
        /* RtlValidSid tests (Revision & 0xf) == 1 */
        CHECKF((r != 0) == ((rev & 0xf) == 1), "revision %d -> %d", rev, r);
    }
    sid[0] = 1;
    sid[1] = SID_MAX_SUB_AUTHORITIES + 1;
    SET_ERR(0);
    CHECK(!U(IsValidSid)(sid));
    EXPECT_ERR(ERROR_INVALID_SID);
    sid[1] = 0xFF;
    CHECK(!U(IsValidSid)(sid));
    sid[0] = 2; sid[1] = 1;
    SET_ERR(0);
    CHECK(!U(IsValidSid)(sid));
    EXPECT_ERR(ERROR_INVALID_SID);
}

TEST(gap7_isvalidsid_null) {
    REQUIRE(U(IsValidSid));
    SET_ERR(0);
    BOOL r = U(IsValidSid)(NULL);
    CHECK(!r);
    t_note("IsValidSid(NULL) last error %lu", GetLastError());
}

/* ---------- IsWellKnownSid ---------- */
TEST(gap7_iswellknownsid_matches_create) {
    REQUIRE(U(IsWellKnownSid));
    int made = 0;
    for (int t = 0; t <= 120; t++) {
        BYTE buf[SECURITY_MAX_SID_SIZE]; DWORD n = sizeof buf;
        if (!CreateWellKnownSid((WELL_KNOWN_SID_TYPE)t, NULL, buf, &n)) continue;
        made++;
        CHECKF(U(IsWellKnownSid)(buf, (WELL_KNOWN_SID_TYPE)t), "type %d", t);
        /* it matches no other type that we can also create */
        for (int u = 0; u <= 120; u++) {
            if (u == t) continue;
            BYTE b2[SECURITY_MAX_SID_SIZE]; DWORD n2 = sizeof b2;
            if (!CreateWellKnownSid((WELL_KNOWN_SID_TYPE)u, NULL, b2, &n2)) continue;
            if (EqualSid(buf, b2)) continue;   /* aliases */
            if (U(IsWellKnownSid)(buf, (WELL_KNOWN_SID_TYPE)u)) CHECKF(0, "type %d claimed as %d", t, u);
        }
    }
    CHECKF(made > 30, "only %d well-known SIDs creatable", made);
}

TEST(gap7_iswellknownsid_common) {
    REQUIRE(U(IsWellKnownSid));
    struct { const char *sddl; WELL_KNOWN_SID_TYPE type; } t[] = {
        { "S-1-1-0",      WinWorldSid },
        { "S-1-5-18",     WinLocalSystemSid },
        { "S-1-5-19",     WinLocalServiceSid },
        { "S-1-5-20",     WinNetworkServiceSid },
        { "S-1-5-32-544", WinBuiltinAdministratorsSid },
        { "S-1-5-32-545", WinBuiltinUsersSid },
        { "S-1-5-32-546", WinBuiltinGuestsSid },
        { "S-1-5-11",     WinAuthenticatedUserSid },
        { "S-1-5-7",      WinAnonymousSid },
        { "S-1-5-2",      WinNetworkSid },
        { "S-1-5-4",      WinInteractiveSid },
        { "S-1-5-6",      WinServiceSid },
        { "S-1-5-1",      WinDialupSid },
        { "S-1-5-9",      WinEnterpriseControllersSid },
        { "S-1-5-10",     WinSelfSid },
        { "S-1-5-12",     WinRestrictedCodeSid },
        { "S-1-3-0",      WinCreatorOwnerSid },
        { "S-1-3-1",      WinCreatorGroupSid },
        { "S-1-0-0",      WinNullSid },
        { "S-1-16-4096",  WinLowLabelSid },
        { "S-1-16-8192",  WinMediumLabelSid },
        { "S-1-16-12288", WinHighLabelSid },
        { "S-1-16-16384", WinSystemLabelSid },
        { "S-1-2-0",      WinLocalSid },
    };
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++) {
        PSID sid = NULL;
        if (!ConvertStringSidToSidA(t[i].sddl, &sid)) { CHECKF(0, "convert %s", t[i].sddl); continue; }
        CHECKF(U(IsWellKnownSid)(sid, t[i].type), "%s", t[i].sddl);
        CHECKF(!U(IsWellKnownSid)(sid, (WELL_KNOWN_SID_TYPE)(t[i].type == WinWorldSid ? WinLocalSystemSid : WinWorldSid)), "%s wrong type", t[i].sddl);
        LocalFree(sid);
    }
}

TEST(gap7_iswellknownsid_not_wellknown) {
    REQUIRE(U(IsWellKnownSid));
    PSID sid = NULL;
    CHECK(ConvertStringSidToSidA("S-1-5-21-111-222-333-1001", &sid));
    for (int t = 0; t < 100; t++) CHECKF(!U(IsWellKnownSid)(sid, (WELL_KNOWN_SID_TYPE)t), "user sid vs type %d", t);
    LocalFree(sid);
    CHECK(ConvertStringSidToSidA("S-1-5-99-1-2-3", &sid));
    for (int t = 0; t < 100; t++) CHECKF(!U(IsWellKnownSid)(sid, (WELL_KNOWN_SID_TYPE)t), "type %d", t);
    LocalFree(sid);
}

TEST(gap7_iswellknownsid_bad_type) {
    REQUIRE(U(IsWellKnownSid));
    BYTE buf[SECURITY_MAX_SID_SIZE]; DWORD n = sizeof buf;
    CHECK(CreateWellKnownSid(WinWorldSid, NULL, buf, &n));
    CHECK(!U(IsWellKnownSid)(buf, (WELL_KNOWN_SID_TYPE)-1));
    CHECK(!U(IsWellKnownSid)(buf, (WELL_KNOWN_SID_TYPE)1000));
    CHECK(!U(IsWellKnownSid)(buf, (WELL_KNOWN_SID_TYPE)0x7fffffff));
}

TEST(gap7_iswellknownsid_invalid_sid) {
    REQUIRE(U(IsWellKnownSid));
    BYTE sid[16] = { 9, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0 };
    CHECK(!U(IsWellKnownSid)(sid, WinWorldSid));
    sid[0] = 1; sid[1] = 0x20;
    CHECK(!U(IsWellKnownSid)(sid, WinWorldSid));
    CHECK(!U(IsWellKnownSid)(NULL, WinWorldSid));
}

TEST(gap7_iswellknownsid_domain_relative) {
    REQUIRE(U(IsWellKnownSid));
    PSID dom = NULL; PSID adm = NULL;
    CHECK(ConvertStringSidToSidA("S-1-5-21-1000-2000-3000", &dom));
    BYTE buf[SECURITY_MAX_SID_SIZE]; DWORD n = sizeof buf;
    if (CreateWellKnownSid(WinAccountAdministratorSid, dom, buf, &n)) {
        CHECK(U(IsWellKnownSid)(buf, WinAccountAdministratorSid));
        CHECK(!U(IsWellKnownSid)(buf, WinAccountGuestSid));
        CHECK(!U(IsWellKnownSid)(buf, WinBuiltinAdministratorsSid));
    } else CHECK(0);
    n = sizeof buf;
    if (CreateWellKnownSid(WinAccountDomainUsersSid, dom, buf, &n)) {
        CHECK(U(IsWellKnownSid)(buf, WinAccountDomainUsersSid));
        CHECK(!U(IsWellKnownSid)(buf, WinAccountDomainAdminsSid));
    } else CHECK(0);
    LocalFree(dom);
}

/* ---------- IsValidAcl ---------- */
TEST(gap7_isvalidacl_revisions) {
    REQUIRE(U(IsValidAcl));
    BYTE buf[256]; ACL *acl = (ACL *)buf;
    for (int rev = 0; rev < 256; rev++) {
        memset(buf, 0, sizeof buf);
        acl->AclRevision = (BYTE)rev; acl->AclSize = 8; acl->AceCount = 0;
        SET_ERR(0);
        BOOL r = U(IsValidAcl)(acl);
        int want = (rev >= 2 && rev <= 4);
        CHECKF((r != 0) == want, "revision %d -> %d", rev, r);
        if (!r) EXPECT_ERR(ERROR_INVALID_ACL);
    }
}

TEST(gap7_isvalidacl_initialized) {
    REQUIRE(U(IsValidAcl));
    BYTE buf[512]; ACL *acl = (ACL *)buf;
    CHECK(InitializeAcl(acl, sizeof buf, ACL_REVISION));
    CHECK(U(IsValidAcl)(acl));
    PSID everyone = NULL; CHECK(ConvertStringSidToSidA("S-1-1-0", &everyone));
    CHECK(AddAccessAllowedAce(acl, ACL_REVISION, GENERIC_READ, everyone));
    CHECK(U(IsValidAcl)(acl));
    CHECK(AddAccessDeniedAce(acl, ACL_REVISION, GENERIC_WRITE, everyone));
    CHECK(U(IsValidAcl)(acl));
    CHECK(acl->AceCount == 2);
    CHECK(InitializeAcl(acl, sizeof buf, ACL_REVISION_DS));
    CHECK(U(IsValidAcl)(acl));
    LocalFree(everyone);
}

TEST(gap7_isvalidacl_corrupt) {
    REQUIRE(U(IsValidAcl));
    BYTE buf[512]; ACL *acl = (ACL *)buf;
    PSID everyone = NULL; CHECK(ConvertStringSidToSidA("S-1-1-0", &everyone));
    CHECK(InitializeAcl(acl, sizeof buf, ACL_REVISION));
    CHECK(AddAccessAllowedAce(acl, ACL_REVISION, GENERIC_READ, everyone));
    CHECK(U(IsValidAcl)(acl));
    WORD size = acl->AclSize, cnt = acl->AceCount;
    /* size smaller than the header */
    acl->AclSize = 4; SET_ERR(0); CHECK(!U(IsValidAcl)(acl)); EXPECT_ERR(ERROR_INVALID_ACL);
    acl->AclSize = 0; CHECK(!U(IsValidAcl)(acl));
    /* size smaller than the ACE that is claimed */
    acl->AclSize = 8 + 4; SET_ERR(0); CHECK(!U(IsValidAcl)(acl)); EXPECT_ERR(ERROR_INVALID_ACL);
    acl->AclSize = size;
    /* ACE count larger than what fits */
    acl->AceCount = 200; SET_ERR(0); CHECK(!U(IsValidAcl)(acl)); EXPECT_ERR(ERROR_INVALID_ACL);
    acl->AceCount = cnt;
    CHECK(U(IsValidAcl)(acl));
    /* ACE header size smaller than header / beyond the ACL */
    ACE_HEADER *ah = (ACE_HEADER *)(buf + sizeof(ACL));
    WORD as = ah->AceSize;
    ah->AceSize = 2; CHECK(!U(IsValidAcl)(acl));
    ah->AceSize = 0x400; CHECK(!U(IsValidAcl)(acl));
    ah->AceSize = as;
    CHECK(U(IsValidAcl)(acl));
    LocalFree(everyone);
}

/* NULL-pointer probes run in a child process: on Windows 10 these fault inside the callee
 * (undocumented behaviour), so the fault must not take down the test run. */
static LONG WINAPI g7_null_filter(PEXCEPTION_POINTERS ep) { (void)ep; ExitProcess(0xE0000001); return EXCEPTION_EXECUTE_HANDLER; }
CHILD(g7_isvalidacl_null) { SetErrorMode(SEM_NOGPFAULTERRORBOX); SetUnhandledExceptionFilter(g7_null_filter); U(IsValidAcl)(NULL); return 0; }
CHILD(g7_isvalidsd_null) { SetErrorMode(SEM_NOGPFAULTERRORBOX); SetUnhandledExceptionFilter(g7_null_filter); U(IsValidSecurityDescriptor)(NULL); return 0; }

TEST(gap7_isvalidacl_null) {
    REQUIRE(U(IsValidAcl));
    DWORD rc = t_run_child("g7_isvalidacl_null", NULL, 15000);
    CHECK(rc != 0xDEAD0000 && rc != 0xDEAD0001);
    t_note("IsValidAcl(NULL) child exit 0x%08lx (0xE0000001 = faulted)", rc);
}

/* ---------- IsValidSecurityDescriptor ---------- */
TEST(gap7_isvalidsd_basic) {
    REQUIRE(U(IsValidSecurityDescriptor));
    SECURITY_DESCRIPTOR sd;
    CHECK(InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION));
    CHECK(U(IsValidSecurityDescriptor)(&sd));
    CHECK(SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE));
    CHECK(U(IsValidSecurityDescriptor)(&sd));
    PSID sid = NULL; CHECK(ConvertStringSidToSidA("S-1-5-32-544", &sid));
    CHECK(SetSecurityDescriptorOwner(&sd, sid, FALSE));
    CHECK(SetSecurityDescriptorGroup(&sd, sid, FALSE));
    CHECK(U(IsValidSecurityDescriptor)(&sd));
    BYTE aclb[128];
    CHECK(InitializeAcl((PACL)aclb, sizeof aclb, ACL_REVISION));
    CHECK(SetSecurityDescriptorDacl(&sd, TRUE, (PACL)aclb, FALSE));
    CHECK(SetSecurityDescriptorSacl(&sd, TRUE, (PACL)aclb, FALSE));
    CHECK(U(IsValidSecurityDescriptor)(&sd));
    LocalFree(sid);
}

TEST(gap7_isvalidsd_revision) {
    REQUIRE(U(IsValidSecurityDescriptor));
    SECURITY_DESCRIPTOR sd;
    for (int rev = 0; rev < 256; rev++) {
        memset(&sd, 0, sizeof sd);
        sd.Revision = (BYTE)rev;
        SET_ERR(0);
        BOOL r = U(IsValidSecurityDescriptor)(&sd);
        CHECKF((r != 0) == (rev == 1), "revision %d -> %d", rev, r);
        if (!r) EXPECT_ERR(ERROR_INVALID_SECURITY_DESCR);
    }
}

TEST(gap7_isvalidsd_self_relative) {
    REQUIRE(U(IsValidSecurityDescriptor));
    PSECURITY_DESCRIPTOR sd = NULL;
    CHECK(ConvertStringSecurityDescriptorToSecurityDescriptorA("O:BAG:SYD:(A;;GA;;;WD)(D;;GW;;;BG)", SDDL_REVISION_1, &sd, NULL));
    if (!sd) return;
    CHECK(U(IsValidSecurityDescriptor)(sd));
    SECURITY_DESCRIPTOR_CONTROL c; DWORD rev;
    CHECK(GetSecurityDescriptorControl(sd, &c, &rev));
    CHECK(c & SE_SELF_RELATIVE);
    CHECK(rev == 1);
    LocalFree(sd);
}

TEST(gap7_isvalidsd_null) {
    REQUIRE(U(IsValidSecurityDescriptor));
    DWORD rc = t_run_child("g7_isvalidsd_null", NULL, 15000);
    CHECK(rc != 0xDEAD0000 && rc != 0xDEAD0001);
    t_note("IsValidSecurityDescriptor(NULL) child exit 0x%08lx (0xE0000001 = faulted)", rc);
}

/* ---------- MapGenericMask ---------- */
TEST(gap7_mapgenericmask_table) {
    REQUIRE(U(MapGenericMask));
    GENERIC_MAPPING gm = { 0x00000011, 0x00000022, 0x00000044, 0x00000F00 };
    struct { DWORD in, out; } t[] = {
        { 0, 0 },
        { GENERIC_READ, 0x11 },
        { GENERIC_WRITE, 0x22 },
        { GENERIC_EXECUTE, 0x44 },
        { GENERIC_ALL, 0xF00 },
        { GENERIC_READ | GENERIC_WRITE, 0x33 },
        { GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE, 0x77 },
        { GENERIC_READ | GENERIC_ALL, 0xF11 },
        { 0x00000001, 0x00000001 },
        { 0x12345678 & ~0xF0000000, 0x12345678 & ~0xF0000000 },
        { DELETE | SYNCHRONIZE, DELETE | SYNCHRONIZE },
        { ACCESS_SYSTEM_SECURITY, ACCESS_SYSTEM_SECURITY },
        { MAXIMUM_ALLOWED, MAXIMUM_ALLOWED },
        { GENERIC_READ | ACCESS_SYSTEM_SECURITY, 0x11 | ACCESS_SYSTEM_SECURITY },
        { GENERIC_READ | MAXIMUM_ALLOWED, 0x11 | MAXIMUM_ALLOWED },
        { GENERIC_READ | 0x1, 0x11 },
        { 0xF0000000, 0x11 | 0x22 | 0x44 | 0xF00 },
        { 0x0F000000, 0x0F000000 },
    };
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++) {
        DWORD m = t[i].in;
        U(MapGenericMask)(&m, &gm);
        CHECKF(m == t[i].out, "in=%08lx got=%08lx want=%08lx", t[i].in, m, t[i].out);
    }
}

TEST(gap7_mapgenericmask_file_mapping) {
    REQUIRE(U(MapGenericMask));
    GENERIC_MAPPING gm = { FILE_GENERIC_READ, FILE_GENERIC_WRITE, FILE_GENERIC_EXECUTE, FILE_ALL_ACCESS };
    DWORD m = GENERIC_READ;       U(MapGenericMask)(&m, &gm); CHECK(m == FILE_GENERIC_READ);
    m = GENERIC_WRITE;            U(MapGenericMask)(&m, &gm); CHECK(m == FILE_GENERIC_WRITE);
    m = GENERIC_EXECUTE;          U(MapGenericMask)(&m, &gm); CHECK(m == FILE_GENERIC_EXECUTE);
    m = GENERIC_ALL;              U(MapGenericMask)(&m, &gm); CHECK(m == FILE_ALL_ACCESS);
    m = GENERIC_READ | GENERIC_WRITE; U(MapGenericMask)(&m, &gm); CHECK(m == (FILE_GENERIC_READ | FILE_GENERIC_WRITE));
}

TEST(gap7_mapgenericmask_idempotent) {
    REQUIRE(U(MapGenericMask));
    GENERIC_MAPPING gm = { 1, 2, 4, 8 };
    DWORD m = GENERIC_ALL | GENERIC_READ;
    U(MapGenericMask)(&m, &gm);
    DWORD first = m;
    U(MapGenericMask)(&m, &gm);
    CHECK(m == first);
    CHECK(!(m & 0xF0000000));
}

/* ---------- MakeSelfRelativeSD / MakeAbsoluteSD ---------- */
static void build_abs_sd(SECURITY_DESCRIPTOR *sd, BYTE *dacl, BYTE *sacl, PSID *owner, PSID *group) {
    InitializeSecurityDescriptor(sd, SECURITY_DESCRIPTOR_REVISION);
    ConvertStringSidToSidA("S-1-5-32-544", owner);
    ConvertStringSidToSidA("S-1-5-18", group);
    InitializeAcl((PACL)dacl, 128, ACL_REVISION);
    PSID world = NULL; ConvertStringSidToSidA("S-1-1-0", &world);
    AddAccessAllowedAce((PACL)dacl, ACL_REVISION, GENERIC_READ, world);
    LocalFree(world);
    InitializeAcl((PACL)sacl, 64, ACL_REVISION);
    SetSecurityDescriptorOwner(sd, *owner, FALSE);
    SetSecurityDescriptorGroup(sd, *group, TRUE);
    SetSecurityDescriptorDacl(sd, TRUE, (PACL)dacl, FALSE);
    SetSecurityDescriptorSacl(sd, TRUE, (PACL)sacl, FALSE);
}

TEST(gap7_makeselfrelative_size_query) {
    REQUIRE(U(MakeSelfRelativeSD));
    SECURITY_DESCRIPTOR sd; BYTE dacl[128], sacl[64]; PSID o = NULL, g = NULL;
    build_abs_sd(&sd, dacl, sacl, &o, &g);
    DWORD len = 0;
    SET_ERR(0);
    CHECK(!U(MakeSelfRelativeSD)(&sd, NULL, &len));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(len > sizeof(SECURITY_DESCRIPTOR_RELATIVE));
    DWORD need = len;
    /* exact-1 */
    BYTE out[512];
    len = need - 1;
    SET_ERR(0);
    CHECK(!U(MakeSelfRelativeSD)(&sd, out, &len));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(len == need);
    /* zero length with a buffer */
    len = 0;
    SET_ERR(0);
    CHECK(!U(MakeSelfRelativeSD)(&sd, out, &len));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(len == need);
    /* exact */
    memset(out, 0xEE, sizeof out);
    len = need;
    CHECK(U(MakeSelfRelativeSD)(&sd, out, &len));
    CHECK(len == need);
    CHECK(IsValidSecurityDescriptor(out));
    CHECK(GetSecurityDescriptorLength(out) == need);
    SECURITY_DESCRIPTOR_CONTROL c; DWORD rev;
    CHECK(GetSecurityDescriptorControl(out, &c, &rev));
    CHECK(c & SE_SELF_RELATIVE);
    CHECK(c & SE_DACL_PRESENT);
    CHECK(c & SE_SACL_PRESENT);
    CHECK(c & SE_GROUP_DEFAULTED);
    CHECK(!(c & SE_OWNER_DEFAULTED));
    /* larger buffer */
    len = sizeof out;
    memset(out, 0xEE, sizeof out);
    CHECK(U(MakeSelfRelativeSD)(&sd, out, &len));
    t_note("MakeSelfRelativeSD larger buffer: len after success = %lu (need %lu)", len, need);
    LocalFree(o); LocalFree(g);
}

TEST(gap7_makeselfrelative_contents) {
    REQUIRE(U(MakeSelfRelativeSD));
    SECURITY_DESCRIPTOR sd; BYTE dacl[128], sacl[64]; PSID o = NULL, g = NULL;
    build_abs_sd(&sd, dacl, sacl, &o, &g);
    DWORD len = 0; U(MakeSelfRelativeSD)(&sd, NULL, &len);
    BYTE out[512]; DWORD l2 = sizeof out;
    CHECK(U(MakeSelfRelativeSD)(&sd, out, &l2));
    PSID ro = NULL, rg = NULL; BOOL def, pres; PACL rd = NULL, rs = NULL;
    CHECK(GetSecurityDescriptorOwner(out, &ro, &def)); CHECK(EqualSid(ro, o)); CHECK(!def);
    CHECK(GetSecurityDescriptorGroup(out, &rg, &def)); CHECK(EqualSid(rg, g)); CHECK(def);
    CHECK(GetSecurityDescriptorDacl(out, &pres, &rd, &def)); CHECK(pres && rd && rd->AceCount == 1);
    CHECK(GetSecurityDescriptorSacl(out, &pres, &rs, &def)); CHECK(pres && rs && rs->AceCount == 0);
    /* pointers inside the buffer, not into the source */
    CHECK((BYTE *)ro >= out && (BYTE *)ro < out + l2);
    CHECK((BYTE *)rd >= out && (BYTE *)rd < out + l2);
    LocalFree(o); LocalFree(g);
}

TEST(gap7_makeselfrelative_minimal_and_null) {
    REQUIRE(U(MakeSelfRelativeSD));
    SECURITY_DESCRIPTOR sd; InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    DWORD len = 0;
    SET_ERR(0);
    CHECK(!U(MakeSelfRelativeSD)(&sd, NULL, &len));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(len == sizeof(SECURITY_DESCRIPTOR_RELATIVE));
    BYTE out[64]; len = sizeof out;
    CHECK(U(MakeSelfRelativeSD)(&sd, out, &len));
    t_note("MakeSelfRelativeSD minimal: len after success = %lu", len);
    /* NULL source */
    len = 100;
    SET_ERR(0);
    CHECK(!U(MakeSelfRelativeSD)(NULL, out, &len));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}

TEST(gap7_makeselfrelative_already_relative) {
    REQUIRE(U(MakeSelfRelativeSD));
    PSECURITY_DESCRIPTOR sd = NULL;
    CHECK(ConvertStringSecurityDescriptorToSecurityDescriptorA("O:BAD:(A;;GA;;;WD)", SDDL_REVISION_1, &sd, NULL));
    if (!sd) return;
    BYTE out[512]; DWORD len = sizeof out;
    SET_ERR(0);
    BOOL r = U(MakeSelfRelativeSD)(sd, out, &len);
    CHECK(!r);
    /* RtlAbsoluteToSelfRelativeSD: SE_SELF_RELATIVE set -> STATUS_BAD_DESCRIPTOR_FORMAT */
    EXPECT_ERR(ERROR_BAD_DESCRIPTOR_FORMAT);
    LocalFree(sd);
}

TEST(gap7_makeabsolute_size_query) {
    REQUIRE(U(MakeAbsoluteSD)); REQUIRE(U(MakeSelfRelativeSD));
    SECURITY_DESCRIPTOR sd; BYTE dacl[128], sacl[64]; PSID o = NULL, g = NULL;
    build_abs_sd(&sd, dacl, sacl, &o, &g);
    BYTE rel[512]; DWORD rl = sizeof rel;
    CHECK(MakeSelfRelativeSD(&sd, rel, &rl));
    DWORD sdSize = 0, daclSize = 0, saclSize = 0, ownSize = 0, grpSize = 0;
    SET_ERR(0);
    CHECK(!U(MakeAbsoluteSD)(rel, NULL, &sdSize, NULL, &daclSize, NULL, &saclSize, NULL, &ownSize, NULL, &grpSize));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(sdSize >= sizeof(SECURITY_DESCRIPTOR));
    CHECK(daclSize >= 8 + 4);
    CHECK(saclSize >= 8);
    CHECK(ownSize == 16);   /* S-1-5-32-544: 8 + 2*4 */
    CHECK(grpSize == 12);   /* S-1-5-18: 8 + 4 */
    CHECK(daclSize == ((PACL)dacl)->AclSize || daclSize == (DWORD)((PACL)dacl)->AclSize || daclSize <= 128);

    /* each undersized buffer individually is rejected, and sizes reported */
    BYTE abs_[sizeof(SECURITY_DESCRIPTOR) + 16], ndacl[128], nsacl[64], nown[32], ngrp[32];
    DWORD a = sdSize, d = daclSize, s = saclSize, ow = ownSize, gr = grpSize;
    SET_ERR(0);
    d = daclSize - 1;
    CHECK(!U(MakeAbsoluteSD)(rel, abs_, &a, (PACL)ndacl, &d, (PACL)nsacl, &s, nown, &ow, ngrp, &gr));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(d == daclSize);
    a = sdSize; d = daclSize; s = saclSize; ow = ownSize - 1; gr = grpSize;
    SET_ERR(0);
    CHECK(!U(MakeAbsoluteSD)(rel, abs_, &a, (PACL)ndacl, &d, (PACL)nsacl, &s, nown, &ow, ngrp, &gr));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(ow == ownSize);
    a = sdSize; d = daclSize; s = saclSize; ow = ownSize; gr = grpSize;
    SET_ERR(0);
    CHECK(U(MakeAbsoluteSD)(rel, abs_, &a, (PACL)ndacl, &d, (PACL)nsacl, &s, nown, &ow, ngrp, &gr));
    LocalFree(o); LocalFree(g);
}

TEST(gap7_makeabsolute_roundtrip) {
    REQUIRE(U(MakeAbsoluteSD)); REQUIRE(U(MakeSelfRelativeSD));
    SECURITY_DESCRIPTOR sd; BYTE dacl[128], sacl[64]; PSID o = NULL, g = NULL;
    build_abs_sd(&sd, dacl, sacl, &o, &g);
    BYTE rel[512]; DWORD rl = sizeof rel;
    CHECK(MakeSelfRelativeSD(&sd, rel, &rl));
    DWORD a = 0, d = 0, s = 0, ow = 0, gr = 0;
    U(MakeAbsoluteSD)(rel, NULL, &a, NULL, &d, NULL, &s, NULL, &ow, NULL, &gr);
    SECURITY_DESCRIPTOR abs_; BYTE ndacl[256], nsacl[256], nown[64], ngrp[64];
    if (a > sizeof abs_ || d > sizeof ndacl || s > sizeof nsacl || ow > sizeof nown || gr > sizeof ngrp) { CHECK(0); return; }
    CHECK(U(MakeAbsoluteSD)(rel, &abs_, &a, (PACL)ndacl, &d, (PACL)nsacl, &s, nown, &ow, ngrp, &gr));
    SECURITY_DESCRIPTOR_CONTROL c; DWORD rev;
    CHECK(GetSecurityDescriptorControl(&abs_, &c, &rev));
    CHECK(!(c & SE_SELF_RELATIVE));
    CHECK(c & SE_DACL_PRESENT);
    CHECK(c & SE_SACL_PRESENT);
    CHECK(IsValidSecurityDescriptor(&abs_));
    PSID po = NULL, pg = NULL; BOOL def, pres; PACL pd = NULL, ps = NULL;
    CHECK(GetSecurityDescriptorOwner(&abs_, &po, &def)); CHECK(EqualSid(po, o));
    CHECK((BYTE *)po == nown);
    CHECK(GetSecurityDescriptorGroup(&abs_, &pg, &def)); CHECK(EqualSid(pg, g)); CHECK(def);
    CHECK((BYTE *)pg == ngrp);
    CHECK(GetSecurityDescriptorDacl(&abs_, &pres, &pd, &def)); CHECK(pres); CHECK((BYTE *)pd == ndacl);
    CHECK(pd->AceCount == 1);
    CHECK(GetSecurityDescriptorSacl(&abs_, &pres, &ps, &def)); CHECK(pres); CHECK((BYTE *)ps == nsacl);
    LocalFree(o); LocalFree(g);
}

TEST(gap7_makeabsolute_minimal_sd_sizes) {
    REQUIRE(U(MakeAbsoluteSD));
    SECURITY_DESCRIPTOR sd; InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    BYTE rel[64]; DWORD rl = sizeof rel;
    CHECK(MakeSelfRelativeSD(&sd, rel, &rl));
    DWORD a = 0, d = 0, s = 0, ow = 0, gr = 0;
    SET_ERR(0);
    BOOL r = U(MakeAbsoluteSD)(rel, NULL, &a, NULL, &d, NULL, &s, NULL, &ow, NULL, &gr);
    CHECK(!r);
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(a == sizeof(SECURITY_DESCRIPTOR));
    CHECK(d == 0 && s == 0 && ow == 0 && gr == 0);
    SECURITY_DESCRIPTOR out;
    a = sizeof out;
    CHECK(U(MakeAbsoluteSD)(rel, &out, &a, NULL, &d, NULL, &s, NULL, &ow, NULL, &gr));
    CHECK(IsValidSecurityDescriptor(&out));
}

TEST(gap7_makeabsolute_null_and_wrongformat) {
    REQUIRE(U(MakeAbsoluteSD));
    DWORD a = 0, d = 0, s = 0, ow = 0, gr = 0;
    SET_ERR(0);
    CHECK(!U(MakeAbsoluteSD)(NULL, NULL, &a, NULL, &d, NULL, &s, NULL, &ow, NULL, &gr));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    /* absolute input is not a self-relative SD */
    SECURITY_DESCRIPTOR sd; InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SET_ERR(0);
    BOOL r = U(MakeAbsoluteSD)(&sd, NULL, &a, NULL, &d, NULL, &s, NULL, &ow, NULL, &gr);
    CHECK(!r);
    t_note("MakeAbsoluteSD(absolute input) -> err %lu", GetLastError());
}

/* ---------- tokens ---------- */
TEST(gap7_istokenrestricted_normal_and_restricted) {
    REQUIRE(U(IsTokenRestricted));
    HANDLE tok = NULL;
    CHECK(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &tok));
    if (!tok) return;
    BOOL base = U(IsTokenRestricted)(tok);
    t_note("process token restricted=%d", base);
    if (!base) {
        PSID world = NULL; ConvertStringSidToSidA("S-1-1-0", &world);
        SID_AND_ATTRIBUTES rs; rs.Sid = world; rs.Attributes = 0;
        HANDLE nt = NULL;
        if (CreateRestrictedToken(tok, 0, 0, NULL, 0, NULL, 1, &rs, &nt)) {
            CHECK(U(IsTokenRestricted)(nt));
            CloseHandle(nt);
        } else t_note("CreateRestrictedToken failed %lu", GetLastError());
        HANDLE nt2 = NULL;
        if (CreateRestrictedToken(tok, DISABLE_MAX_PRIVILEGE, 0, NULL, 0, NULL, 0, NULL, &nt2)) {
            CHECK(!U(IsTokenRestricted)(nt2));     /* no restricting SIDs */
            CloseHandle(nt2);
        }
        LocalFree(world);
    }
    CloseHandle(tok);
}

TEST(gap7_istokenrestricted_invalid_handles) {
    REQUIRE(U(IsTokenRestricted));
    SET_ERR(0);
    CHECK(!U(IsTokenRestricted)(NULL));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(!U(IsTokenRestricted)(INVALID_HANDLE_VALUE));
    DWORD e = GetLastError();
    CHECKF(e == ERROR_INVALID_HANDLE || e == ERROR_INVALID_PARAMETER, "err %lu", e);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    CHECK(!U(IsTokenRestricted)(ev));
    e = GetLastError();
    CHECKF(e == ERROR_INVALID_HANDLE, "event handle err %lu", e);
    CloseHandle(ev);
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL); CloseHandle(h);
    SET_ERR(0);
    CHECK(!U(IsTokenRestricted)(h));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}

TEST(gap7_istokenrestricted_no_query_access) {
    REQUIRE(U(IsTokenRestricted));
    HANDLE tok = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE, &tok)) { t_skip("cannot open token"); return; }
    SET_ERR(0);
    CHECK(!U(IsTokenRestricted)(tok));
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(tok);
}

TEST(gap7_openthreadtoken_no_token) {
    REQUIRE(U(OpenThreadToken));
    HANDLE t = (HANDLE)0x1234;
    SET_ERR(0);
    CHECK(!U(OpenThreadToken)(GetCurrentThread(), TOKEN_QUERY, TRUE, &t));
    EXPECT_ERR(ERROR_NO_TOKEN);
    CHECK(t == (HANDLE)0x1234 || t == NULL);
    SET_ERR(0);
    CHECK(!U(OpenThreadToken)(GetCurrentThread(), TOKEN_QUERY, FALSE, &t));
    EXPECT_ERR(ERROR_NO_TOKEN);
}

TEST(gap7_openthreadtoken_invalid_handle) {
    REQUIRE(U(OpenThreadToken));
    HANDLE t = NULL;
    SET_ERR(0);
    CHECK(!U(OpenThreadToken)(NULL, TOKEN_QUERY, TRUE, &t));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(!U(OpenThreadToken)(INVALID_HANDLE_VALUE, TOKEN_QUERY, TRUE, &t));
    DWORD e = GetLastError();
    CHECKF(e == ERROR_INVALID_HANDLE, "INVALID_HANDLE_VALUE err %lu", e);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    CHECK(!U(OpenThreadToken)(ev, TOKEN_QUERY, TRUE, &t));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
    /* the process pseudo handle is not a thread */
    SET_ERR(0);
    CHECK(!U(OpenThreadToken)(GetCurrentProcess(), TOKEN_QUERY, TRUE, &t));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE closed = CreateEventA(NULL, TRUE, FALSE, NULL); CloseHandle(closed);
    SET_ERR(0);
    CHECK(!U(OpenThreadToken)(closed, TOKEN_QUERY, TRUE, &t));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}

TEST(gap7_openthreadtoken_insufficient_thread_access) {
    REQUIRE(U(OpenThreadToken));
    HANDLE th = OpenThread(SYNCHRONIZE, FALSE, GetCurrentThreadId());
    CHECK(th != NULL);
    if (!th) return;
    HANDLE t = NULL;
    SET_ERR(0);
    CHECK(!U(OpenThreadToken)(th, TOKEN_QUERY, TRUE, &t));
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(th);
}

struct imp_ctx { int ok_self, ok_thread, got_type, type_is_imp, level, no_token_after, query_ok; DWORD err_after; };
static DWORD WINAPI imp_thread(LPVOID p) {
    struct imp_ctx *c = p;
    if (!ImpersonateSelf(SecurityImpersonation)) return 1;
    HANDLE t = NULL;
    c->ok_self = U(OpenThreadToken)(GetCurrentThread(), TOKEN_QUERY | TOKEN_DUPLICATE, TRUE, &t);
    if (c->ok_self && t) {
        TOKEN_TYPE tt; DWORD n;
        if (GetTokenInformation(t, TokenType, &tt, sizeof tt, &n)) { c->got_type = 1; c->type_is_imp = (tt == TokenImpersonation); }
        SECURITY_IMPERSONATION_LEVEL lv;
        if (GetTokenInformation(t, TokenImpersonationLevel, &lv, sizeof lv, &n)) c->level = lv;
        CloseHandle(t);
    }
    HANDLE t2 = NULL;
    c->ok_thread = U(OpenThreadToken)(GetCurrentThread(), TOKEN_QUERY, FALSE, &t2);
    if (t2) CloseHandle(t2);
    RevertToSelf();
    HANDLE t3 = NULL;
    SET_ERR(0);
    c->no_token_after = !U(OpenThreadToken)(GetCurrentThread(), TOKEN_QUERY, TRUE, &t3);
    c->err_after = GetLastError();
    if (t3) CloseHandle(t3);
    return 0;
}

TEST(gap7_openthreadtoken_impersonating) {
    REQUIRE(U(OpenThreadToken));
    struct imp_ctx c; memset(&c, 0, sizeof c);
    HANDLE th = CreateThread(NULL, 0, imp_thread, &c, 0, NULL);
    CHECK(th != NULL);
    if (!th) return;
    CHECK(WaitForSingleObject(th, 10000) == WAIT_OBJECT_0);
    DWORD ec = 99; GetExitCodeThread(th, &ec);
    CloseHandle(th);
    CHECK(ec == 0);
    CHECK(c.ok_self);
    CHECK(c.got_type && c.type_is_imp);
    CHECK(c.level == SecurityImpersonation);
    CHECK(c.ok_thread);     /* OpenAsSelf FALSE: access checked against the impersonation token itself, which we own */
    CHECK(c.no_token_after);
    CHECK(c.err_after == ERROR_NO_TOKEN);
}

TEST(gap7_openthreadtoken_null_output_pointer) {
    REQUIRE(U(OpenThreadToken));
    SET_ERR(0);
    BOOL r = U(OpenThreadToken)(GetCurrentThread(), TOKEN_QUERY, TRUE, NULL);
    CHECK(!r);
    t_note("OpenThreadToken(NULL out) err %lu", GetLastError());
}

TEST(gap7_openthreadtoken_other_thread_handle) {
    REQUIRE(U(OpenThreadToken));
    /* a suspended thread of our own that has no token */
    HANDLE th = CreateThread(NULL, 0, imp_thread, NULL, CREATE_SUSPENDED, NULL);
    if (!th) { CHECK(0); return; }
    HANDLE t = NULL;
    SET_ERR(0);
    CHECK(!U(OpenThreadToken)(th, TOKEN_QUERY, TRUE, &t));
    EXPECT_ERR(ERROR_NO_TOKEN);
    TerminateThread(th, 0);      /* our own, never started, no resources held */
    WaitForSingleObject(th, 5000);
    CloseHandle(th);
}

/* ---------- audit alarms ---------- */
static int have_audit_priv(void) {
    HANDLE tok; int have = 0;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) return 0;
    BYTE buf[4096]; DWORD n;
    if (GetTokenInformation(tok, TokenPrivileges, buf, sizeof buf, &n)) {
        TOKEN_PRIVILEGES *tp = (TOKEN_PRIVILEGES *)buf;
        LUID luid;
        if (LookupPrivilegeValueA(NULL, "SeAuditPrivilege", &luid))
            for (DWORD i = 0; i < tp->PrivilegeCount; i++)
                if (tp->Privileges[i].Luid.LowPart == luid.LowPart && tp->Privileges[i].Luid.HighPart == luid.HighPart) have = 1;
    }
    CloseHandle(tok);
    return have;
}

TEST(gap7_objectclosealarm_requires_audit_privilege) {
    REQUIRE(U(ObjectCloseAuditAlarmW));
    if (have_audit_priv()) { t_skip("SeAuditPrivilege held: not exercised"); return; }
    DWORD id = 0; BOOL r;
    BOOL (WINAPI *pClose)(LPCWSTR, LPVOID, BOOL) = U(ObjectCloseAuditAlarmW);
    /* GenerateOnClose == FALSE: the kernel returns success without the privilege check */
    CHECK(pClose(L"k32t", &id, FALSE));
    SET_ERR(0);
    CHECK(!pClose(L"k32t", &id, TRUE));
    EXPECT_ERR(ERROR_PRIVILEGE_NOT_HELD);
    SET_ERR(0);
    /* NULL HandleId / empty name: kernel probe order not verified from user mode */
    r = pClose(L"", NULL, FALSE);
    t_note("ObjectCloseAuditAlarmW(L\"\", NULL) -> %d err %lu", r, GetLastError());
}

TEST(gap7_objectdeletealarm_requires_audit_privilege) {
    REQUIRE(U(ObjectDeleteAuditAlarmW));
    if (have_audit_priv()) { t_skip("SeAuditPrivilege held: not exercised"); return; }
    DWORD id = 0;
    BOOL (WINAPI *pDel)(LPCWSTR, LPVOID, BOOL) = U(ObjectDeleteAuditAlarmW);
    /* GenerateOnClose == FALSE: the kernel returns success without the privilege check */
    CHECK(pDel(L"k32t", &id, FALSE));
    SET_ERR(0);
    CHECK(!pDel(L"k32t", &id, TRUE));
    EXPECT_ERR(ERROR_PRIVILEGE_NOT_HELD);
}

TEST(gap7_objectopenalarm_unprivileged_fails) {
    REQUIRE(U(ObjectOpenAuditAlarmW));
    if (have_audit_priv()) { t_skip("SeAuditPrivilege held: not exercised"); return; }
    DWORD id = 0; BOOL gen = 0x55;
    SECURITY_DESCRIPTOR sd; InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    HANDLE tok = NULL;
    OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok);
    SET_ERR(0);
    BOOL r = U(ObjectOpenAuditAlarmW)(L"k32t", &id, (LPWSTR)L"File", (LPWSTR)L"k32t-object", &sd, tok,
                                      GENERIC_READ, GENERIC_READ, NULL, FALSE, TRUE, &gen);
    CHECK(!r);
    DWORD e = GetLastError();
    t_note("ObjectOpenAuditAlarmW unprivileged -> err %lu", e);
    CHECKF(e != 0, "error must be set");
    if (tok) CloseHandle(tok);
}

TEST(gap7_objectprivilegealarm_unprivileged_fails) {
    REQUIRE(U(ObjectPrivilegeAuditAlarmW));
    if (have_audit_priv()) { t_skip("SeAuditPrivilege held: not exercised"); return; }
    HANDLE tok = NULL;
    OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok);
    DWORD id = 0;
    PRIVILEGE_SET ps; memset(&ps, 0, sizeof ps); ps.PrivilegeCount = 0;
    SET_ERR(0);
    BOOL r = U(ObjectPrivilegeAuditAlarmW)(L"k32t", &id, tok, GENERIC_READ, &ps, TRUE);
    CHECK(!r);
    DWORD e = GetLastError();
    t_note("ObjectPrivilegeAuditAlarmW unprivileged -> err %lu", e);
    CHECKF(e != 0, "error must be set");
    if (tok) CloseHandle(tok);
}
