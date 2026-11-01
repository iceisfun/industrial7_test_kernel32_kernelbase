#include "g6.h"

typedef BOOL (WINAPI *og_get_fn)(PSECURITY_DESCRIPTOR, PSID *, LPBOOL);
typedef BOOL (WINAPI *og_set_fn)(PSECURITY_DESCRIPTOR, PSID, BOOL);

static void init_sd(SECURITY_DESCRIPTOR *sd) { InitializeSecurityDescriptor(sd, SECURITY_DESCRIPTOR_REVISION); }

/* Shared body for owner (offset 4) and group (offset 8). */
static void og_run(og_get_fn get, og_set_fn set, DWORD reloff, const char *nm) {
    SECURITY_DESCRIPTOR sd; BYTE sb[68], sb2[68];
    PSID sid = g6_sid(sb, 5, 1, 18), sid2 = g6_sid(sb2, 5, 2, 32, 544);
    PSID out; BOOL def;
    (void)nm;

    /* empty descriptor */
    init_sd(&sd);
    out = (PSID)(ULONG_PTR)1; def = 77;
    CHECK(get(&sd, &out, &def));
    EXPECT_EQ(out, NULL);
    EXPECT_EQ(def, 0);

    /* set, not defaulted */
    CHECK(set(&sd, sid, FALSE));
    out = NULL; def = 77;
    CHECK(get(&sd, &out, &def));
    EXPECT_EQ(out, sid);
    EXPECT_EQ(def, 0);

    /* set, defaulted */
    CHECK(set(&sd, sid2, TRUE));
    out = NULL; def = 0;
    CHECK(get(&sd, &out, &def));
    EXPECT_EQ(out, sid2);
    EXPECT_EQ(def, 1);

    /* clear again */
    CHECK(set(&sd, NULL, FALSE));
    out = (PSID)(ULONG_PTR)1; def = 5;
    CHECK(get(&sd, &out, &def));
    EXPECT_EQ(out, NULL);
    EXPECT_EQ(def, 0);

    /* self-relative form: pointer is base + offset */
    CHECK(set(&sd, sid2, TRUE));
    BYTE rel[512]; DWORD sz = sizeof rel;
    if (MakeSelfRelativeSD(&sd, rel, &sz)) {
        out = NULL; def = 0;
        CHECK(get(rel, &out, &def));
        DWORD off = *(DWORD *)(rel + reloff);
        CHECK(off != 0);
        EXPECT_EQ(out, rel + off);
        CHECK(EqualSid(out, sid2));
        EXPECT_EQ(def, 1);
    } else CHECK(0);

    /* self-relative with zero offset -> NULL */
    SECURITY_DESCRIPTOR sd2; init_sd(&sd2);
    BYTE rel2[64]; DWORD sz2 = sizeof rel2;
    CHECK(MakeSelfRelativeSD(&sd2, rel2, &sz2));
    out = (PSID)(ULONG_PTR)1; def = 9;
    CHECK(get(rel2, &out, &def));
    EXPECT_EQ(out, NULL);
    EXPECT_EQ(def, 0);

    /* revision mismatch */
    init_sd(&sd);
    DWORD revs[] = {0, 2, 3, 0x80, 0xFF};
    for (unsigned i = 0; i < 5; i++) {
        init_sd(&sd);
        ((BYTE *)&sd)[0] = (BYTE)revs[i];
        SET_ERR(0);
        BOOL r = get(&sd, &out, &def);
        CHECKF(r == FALSE, "%s revision %lu", nm, revs[i]);
        CHECKF(GetLastError() == ERROR_UNKNOWN_REVISION, "%s rev %lu err %lu", nm, revs[i], GetLastError());
    }
}

TEST(gap6_sdowner_semantics) {
    REQUIRE(U(GetSecurityDescriptorOwner)); REQUIRE(U(InitializeSecurityDescriptor));
    og_run(U(GetSecurityDescriptorOwner), SetSecurityDescriptorOwner, 4, "owner");
}
TEST(gap6_sdgroup_semantics) {
    REQUIRE(U(GetSecurityDescriptorGroup)); REQUIRE(U(InitializeSecurityDescriptor));
    og_run(U(GetSecurityDescriptorGroup), SetSecurityDescriptorGroup, 8, "group");
}
TEST(gap6_sdowner_group_independent) {
    REQUIRE(U(GetSecurityDescriptorOwner)); REQUIRE(U(GetSecurityDescriptorGroup));
    SECURITY_DESCRIPTOR sd; BYTE a[68], b[68];
    PSID sa = g6_sid(a, 5, 1, 18), sbb = g6_sid(b, 5, 1, 19);
    init_sd(&sd);
    SetSecurityDescriptorOwner(&sd, sa, FALSE);
    PSID o = NULL, g = (PSID)1; BOOL d;
    CHECK(U(GetSecurityDescriptorOwner)(&sd, &o, &d)); EXPECT_EQ(o, sa);
    CHECK(U(GetSecurityDescriptorGroup)(&sd, &g, &d)); EXPECT_EQ(g, NULL);
    SetSecurityDescriptorGroup(&sd, sbb, TRUE);
    CHECK(U(GetSecurityDescriptorGroup)(&sd, &g, &d)); EXPECT_EQ(g, sbb); EXPECT_EQ(d, 1);
    CHECK(U(GetSecurityDescriptorOwner)(&sd, &o, &d)); EXPECT_EQ(o, sa); EXPECT_EQ(d, 0);
}
TEST(gap6_sdowner_defaulted_control_bit) {
    REQUIRE(U(GetSecurityDescriptorOwner));
    SECURITY_DESCRIPTOR sd; BYTE a[68]; PSID sa = g6_sid(a, 5, 1, 18);
    init_sd(&sd); sd.Owner = sa;
    PSID o; BOOL d = 9;
    sd.Control = 0;
    CHECK(U(GetSecurityDescriptorOwner)(&sd, &o, &d)); EXPECT_EQ(d, 0); EXPECT_EQ(o, sa);
    sd.Control = SE_OWNER_DEFAULTED;
    CHECK(U(GetSecurityDescriptorOwner)(&sd, &o, &d)); EXPECT_EQ(d, 1);
    sd.Control = (SECURITY_DESCRIPTOR_CONTROL)~SE_OWNER_DEFAULTED & ~SE_SELF_RELATIVE;
    CHECK(U(GetSecurityDescriptorOwner)(&sd, &o, &d)); EXPECT_EQ(d, 0);
}
TEST(gap6_sdgroup_defaulted_control_bit) {
    REQUIRE(U(GetSecurityDescriptorGroup));
    SECURITY_DESCRIPTOR sd; BYTE a[68]; PSID sa = g6_sid(a, 5, 1, 18);
    init_sd(&sd); sd.Group = sa;
    PSID o; BOOL d = 9;
    sd.Control = SE_GROUP_DEFAULTED;
    CHECK(U(GetSecurityDescriptorGroup)(&sd, &o, &d)); EXPECT_EQ(d, 1); EXPECT_EQ(o, sa);
    sd.Control = SE_OWNER_DEFAULTED;   /* other bits do not leak */
    CHECK(U(GetSecurityDescriptorGroup)(&sd, &o, &d)); EXPECT_EQ(d, 0);
}
TEST(gap6_sdowner_nonvalid_sid_pointer_returned_raw) {
    REQUIRE(U(GetSecurityDescriptorOwner));
    /* The getter performs no validation of the SID it hands back. */
    SECURITY_DESCRIPTOR sd; init_sd(&sd);
    sd.Owner = (PSID)(ULONG_PTR)0x1000;     /* never dereferenced by the call */
    PSID o = NULL; BOOL d;
    CHECK(U(GetSecurityDescriptorOwner)(&sd, &o, &d));
    EXPECT_EQ(o, (PSID)(ULONG_PTR)0x1000);
}
TEST(gap6_sdowner_repeat) {
    REQUIRE(U(GetSecurityDescriptorOwner));
    SECURITY_DESCRIPTOR sd; BYTE a[68]; PSID sa = g6_sid(a, 5, 1, 18); init_sd(&sd);
    SetSecurityDescriptorOwner(&sd, sa, FALSE);
    int bad = 0;
    for (int i = 0; i < 5000; i++) { PSID o = NULL; BOOL d; if (!U(GetSecurityDescriptorOwner)(&sd, &o, &d) || o != sa) bad++; }
    EXPECT_EQ(bad, 0);
}
TEST(gap6_sdowner_from_real_file_sd) {
    REQUIRE(U(GetSecurityDescriptorOwner)); REQUIRE(U(GetSecurityDescriptorGroup));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "g6_sd.txt");
    HANDLE h = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_skip("create"); return; }
    CloseHandle(h);
    DWORD need = 0;
    GetFileSecurityA(p, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, NULL, 0, &need);
    if (!need) { t_skip("GetFileSecurity size"); DeleteFileA(p); return; }
    BYTE *sd = (BYTE *)LocalAlloc(LPTR, need);
    if (GetFileSecurityA(p, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, sd, need, &need)) {
        PSID o = NULL, g = NULL; BOOL d = 7;
        CHECK(U(GetSecurityDescriptorOwner)(sd, &o, &d));
        CHECK(o != NULL && IsValidSid(o));
        CHECK(d == 0 || d == 1);
        CHECK(U(GetSecurityDescriptorGroup)(sd, &g, &d));
        CHECK(g != NULL && IsValidSid(g));
        CHECK((BYTE *)o >= sd && (BYTE *)o < sd + need);
        CHECK((BYTE *)g >= sd && (BYTE *)g < sd + need);
    } else CHECK(0);
    LocalFree(sd);
    DeleteFileA(p);
}

/* ---- GetSecurityDescriptorSacl ---- */
TEST(gap6_sdsacl_absent) {
    REQUIRE(U(GetSecurityDescriptorSacl));
    SECURITY_DESCRIPTOR sd; init_sd(&sd);
    BOOL present = 77, def = 77; PACL acl = (PACL)1;
    SET_ERR(0);
    CHECK(U(GetSecurityDescriptorSacl)(&sd, &present, &acl, &def));
    EXPECT_EQ(present, 0);
}
TEST(gap6_sdsacl_null_sacl_present) {
    REQUIRE(U(GetSecurityDescriptorSacl));
    SECURITY_DESCRIPTOR sd; init_sd(&sd);
    CHECK(SetSecurityDescriptorSacl(&sd, TRUE, NULL, FALSE));
    BOOL present = 0, def = 77; PACL acl = (PACL)1;
    CHECK(U(GetSecurityDescriptorSacl)(&sd, &present, &acl, &def));
    EXPECT_EQ(present, 1);
    EXPECT_EQ(acl, NULL);
    EXPECT_EQ(def, 0);
}
TEST(gap6_sdsacl_real_acl_defaulted_variants) {
    REQUIRE(U(GetSecurityDescriptorSacl));
    SECURITY_DESCRIPTOR sd; init_sd(&sd);
    BYTE aclbuf[64]; PACL acl = (PACL)aclbuf; InitializeAcl(acl, sizeof aclbuf, ACL_REVISION);
    for (int d = 0; d <= 1; d++) {
        CHECK(SetSecurityDescriptorSacl(&sd, TRUE, acl, d));
        BOOL present = 0, def = 77; PACL got = NULL;
        CHECK(U(GetSecurityDescriptorSacl)(&sd, &present, &got, &def));
        EXPECT_EQ(present, 1); EXPECT_EQ(got, acl); EXPECT_EQ(def, d);
    }
    /* un-present again */
    CHECK(SetSecurityDescriptorSacl(&sd, FALSE, NULL, FALSE));
    BOOL present = 5, def; PACL got = NULL;
    CHECK(U(GetSecurityDescriptorSacl)(&sd, &present, &got, &def));
    EXPECT_EQ(present, 0);
}
TEST(gap6_sdsacl_independent_of_dacl) {
    REQUIRE(U(GetSecurityDescriptorSacl));
    SECURITY_DESCRIPTOR sd; init_sd(&sd);
    CHECK(SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE));
    BOOL present = 5, def; PACL got = NULL;
    CHECK(U(GetSecurityDescriptorSacl)(&sd, &present, &got, &def));
    EXPECT_EQ(present, 0);
}
TEST(gap6_sdsacl_selfrelative) {
    REQUIRE(U(GetSecurityDescriptorSacl));
    SECURITY_DESCRIPTOR sd; init_sd(&sd);
    BYTE aclbuf[32]; PACL acl = (PACL)aclbuf; InitializeAcl(acl, sizeof aclbuf, ACL_REVISION);
    CHECK(SetSecurityDescriptorSacl(&sd, TRUE, acl, TRUE));
    BYTE rel[256]; DWORD sz = sizeof rel;
    CHECK(MakeSelfRelativeSD(&sd, rel, &sz));
    BOOL present = 0, def = 0; PACL got = NULL;
    CHECK(U(GetSecurityDescriptorSacl)(rel, &present, &got, &def));
    EXPECT_EQ(present, 1); EXPECT_EQ(def, 1);
    DWORD off = *(DWORD *)(rel + 12);
    CHECK(off != 0);
    EXPECT_EQ(got, rel + off);
    EXPECT_EQ(got->AclSize, 32);
    EXPECT_EQ(got->AclRevision, ACL_REVISION);
}
TEST(gap6_sdsacl_revision_mismatch) {
    REQUIRE(U(GetSecurityDescriptorSacl));
    SECURITY_DESCRIPTOR sd;
    BYTE revs[] = {0, 2, 0xFF};
    for (unsigned i = 0; i < 3; i++) {
        init_sd(&sd); ((BYTE *)&sd)[0] = revs[i];
        BOOL present, def; PACL got;
        SET_ERR(0);
        CHECK(!U(GetSecurityDescriptorSacl)(&sd, &present, &got, &def));
        EXPECT_ERR(ERROR_UNKNOWN_REVISION);
    }
}
TEST(gap6_sdsacl_control_flag_only) {
    REQUIRE(U(GetSecurityDescriptorSacl));
    SECURITY_DESCRIPTOR sd; init_sd(&sd);
    BYTE aclbuf[16]; PACL acl = (PACL)aclbuf; InitializeAcl(acl, sizeof aclbuf, ACL_REVISION);
    sd.Sacl = acl;
    BOOL present = 5, def; PACL got = NULL;
    sd.Control = 0;
    CHECK(U(GetSecurityDescriptorSacl)(&sd, &present, &got, &def));
    EXPECT_EQ(present, 0);                 /* pointer set but SE_SACL_PRESENT clear */
    sd.Control = SE_SACL_PRESENT;
    CHECK(U(GetSecurityDescriptorSacl)(&sd, &present, &got, &def));
    EXPECT_EQ(present, 1); EXPECT_EQ(got, acl);
    sd.Control = SE_SACL_PRESENT | SE_SACL_DEFAULTED;
    CHECK(U(GetSecurityDescriptorSacl)(&sd, &present, &got, &def));
    EXPECT_EQ(def, 1);
}

/* ---- GetSecurityDescriptorLength ---- */
static DWORD sdlen(void *sd) { return U(GetSecurityDescriptorLength)(sd); }

TEST(gap6_sdlen_absolute_components) {
    REQUIRE(U(GetSecurityDescriptorLength));
    SECURITY_DESCRIPTOR sd; BYTE a[68], b[68]; init_sd(&sd);
    PSID o = g6_sid(a, 5, 1, 18), g = g6_sid(b, 5, 2, 32, 544);
    DWORD base = sdlen(&sd);
    EXPECT_EQ(base, sizeof(SECURITY_DESCRIPTOR));       /* 40 on x64 for an absolute SD */
    SetSecurityDescriptorOwner(&sd, o, FALSE);
    EXPECT_EQ(sdlen(&sd), base + 12);
    SetSecurityDescriptorGroup(&sd, g, FALSE);
    EXPECT_EQ(sdlen(&sd), base + 12 + 16);
    BYTE d[40]; InitializeAcl((PACL)d, sizeof d, ACL_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, (PACL)d, FALSE);
    EXPECT_EQ(sdlen(&sd), base + 12 + 16 + 40);
    BYTE s[24]; InitializeAcl((PACL)s, sizeof s, ACL_REVISION);
    SetSecurityDescriptorSacl(&sd, TRUE, (PACL)s, FALSE);
    EXPECT_EQ(sdlen(&sd), base + 12 + 16 + 40 + 24);
}
TEST(gap6_sdlen_null_dacl_adds_nothing) {
    REQUIRE(U(GetSecurityDescriptorLength));
    SECURITY_DESCRIPTOR sd; init_sd(&sd);
    DWORD base = sdlen(&sd);
    SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
    EXPECT_EQ(sdlen(&sd), base);
    SetSecurityDescriptorSacl(&sd, TRUE, NULL, FALSE);
    EXPECT_EQ(sdlen(&sd), base);
}
TEST(gap6_sdlen_acl_size_rounded_to_4) {
    REQUIRE(U(GetSecurityDescriptorLength));
    SECURITY_DESCRIPTOR sd; init_sd(&sd);
    DWORD base = sdlen(&sd);
    BYTE d[64]; InitializeAcl((PACL)d, sizeof d, ACL_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, (PACL)d, FALSE);
    for (WORD sz = 8; sz <= 20; sz++) {
        ((PACL)d)->AclSize = sz;
        EXPECT_EQ(sdlen(&sd), base + ((sz + 3) & ~3u));
    }
}
TEST(gap6_sdlen_acl_ignored_without_present_flag) {
    REQUIRE(U(GetSecurityDescriptorLength));
    SECURITY_DESCRIPTOR sd; init_sd(&sd);
    DWORD base = sdlen(&sd);
    BYTE d[32]; InitializeAcl((PACL)d, sizeof d, ACL_REVISION);
    sd.Dacl = (PACL)d; sd.Sacl = (PACL)d;     /* control bits clear */
    EXPECT_EQ(sdlen(&sd), base);
}
TEST(gap6_sdlen_selfrelative_matches_buffer) {
    REQUIRE(U(GetSecurityDescriptorLength));
    SECURITY_DESCRIPTOR sd; BYTE a[68], b[68]; init_sd(&sd);
    SetSecurityDescriptorOwner(&sd, g6_sid(a, 5, 1, 18), FALSE);
    SetSecurityDescriptorGroup(&sd, g6_sid(b, 5, 2, 32, 544), FALSE);
    BYTE d[40]; InitializeAcl((PACL)d, sizeof d, ACL_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, (PACL)d, FALSE);
    BYTE s[24]; InitializeAcl((PACL)s, sizeof s, ACL_REVISION);
    SetSecurityDescriptorSacl(&sd, TRUE, (PACL)s, FALSE);
    DWORD need = 0;
    SET_ERR(0);
    CHECK(!MakeSelfRelativeSD(&sd, NULL, &need));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    BYTE rel[512]; DWORD sz = sizeof rel;
    CHECK(MakeSelfRelativeSD(&sd, rel, &sz));
    EXPECT_EQ(sdlen(rel), need);
    EXPECT_EQ(sdlen(rel), 20 + 12 + 16 + 40 + 24);
    EXPECT_EQ(GetSecurityDescriptorLength(rel), sdlen(rel));
    CHECK(IsValidSecurityDescriptor(rel));
}
TEST(gap6_sdlen_selfrelative_empty) {
    REQUIRE(U(GetSecurityDescriptorLength));
    SECURITY_DESCRIPTOR sd; init_sd(&sd);
    BYTE rel[64]; DWORD sz = sizeof rel;
    CHECK(MakeSelfRelativeSD(&sd, rel, &sz));
    EXPECT_EQ(sdlen(rel), 20);
}
TEST(gap6_sdlen_matches_advapi_on_system_sds) {
    REQUIRE(U(GetSecurityDescriptorLength));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "g6_len.txt");
    HANDLE h = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_skip("create"); return; }
    CloseHandle(h);
    DWORD need = 0;
    GetFileSecurityA(p, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, NULL, 0, &need);
    BYTE *sd = (BYTE *)LocalAlloc(LPTR, need ? need : 1);
    DWORD got = 0;
    if (need && GetFileSecurityA(p, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, sd, need, &got)) {
        CHECK(sdlen(sd) <= need);
        CHECK(sdlen(sd) > 20);
    }
    LocalFree(sd);
    DeleteFileA(p);
}
TEST(gap6_sdlen_ignores_revision) {
    REQUIRE(U(GetSecurityDescriptorLength));
    SECURITY_DESCRIPTOR sd; init_sd(&sd);
    DWORD base = sdlen(&sd);
    ((BYTE *)&sd)[0] = 9;
    EXPECT_EQ(sdlen(&sd), base);
}
TEST(gap6_sdlen_repeat) {
    REQUIRE(U(GetSecurityDescriptorLength));
    SECURITY_DESCRIPTOR sd; init_sd(&sd);
    DWORD base = sdlen(&sd);
    for (int i = 0; i < 2000; i++) if (sdlen(&sd) != base) { CHECK(0); break; }
}

/* ---- GetSecurityDescriptorRMControl ---- */
static DWORD set_rm(SECURITY_DESCRIPTOR *sd, UCHAR *v) {
    if (v) { sd->Control |= 0x4000; sd->Sbz1 = *v; } else { sd->Control &= ~0x4000; sd->Sbz1 = 0; }
    return 0;
}
TEST(gap6_sdrm_not_set) {
    REQUIRE(U(GetSecurityDescriptorRMControl)); if (t_is_wine()) { t_skip("GetSecurityDescriptorRMControl unimplemented in wine"); return; }
    SECURITY_DESCRIPTOR sd; init_sd(&sd);
    UCHAR rm = 0xFF;
    DWORD r = U(GetSecurityDescriptorRMControl)(&sd, &rm);
    EXPECT_EQ(r, ERROR_INVALID_DATA);
    EXPECT_EQ(rm, 0);
}
TEST(gap6_sdrm_set_values) {
    REQUIRE(U(GetSecurityDescriptorRMControl)); if (t_is_wine()) { t_skip("GetSecurityDescriptorRMControl unimplemented in wine"); return; }
    SECURITY_DESCRIPTOR sd; init_sd(&sd);
    UCHAR vals[] = {0, 1, 0x5A, 0x80, 0xFF};
    for (unsigned i = 0; i < sizeof vals; i++) {
        UCHAR v = vals[i];
        DWORD sr = set_rm(&sd, &v);
        EXPECT_EQ(sr, ERROR_SUCCESS);
        UCHAR rm = 0xEE;
        DWORD r = U(GetSecurityDescriptorRMControl)(&sd, &rm);
        CHECKF(r == ERROR_SUCCESS, "val %u", v);
        CHECKF(rm == v, "val %u got %u", v, rm);
        CHECK(sd.Control & 0x4000);
    }
}
TEST(gap6_sdrm_cleared_by_null) {
    REQUIRE(U(GetSecurityDescriptorRMControl)); if (t_is_wine()) { t_skip("GetSecurityDescriptorRMControl unimplemented in wine"); return; }
    SECURITY_DESCRIPTOR sd; init_sd(&sd);
    UCHAR v = 0x33;
    set_rm(&sd, &v);
    set_rm(&sd, NULL);
    UCHAR rm = 0xEE;
    EXPECT_EQ(U(GetSecurityDescriptorRMControl)(&sd, &rm), ERROR_INVALID_DATA);
    EXPECT_EQ(rm, 0);
}
TEST(gap6_sdrm_control_bit_direct) {
    REQUIRE(U(GetSecurityDescriptorRMControl)); if (t_is_wine()) { t_skip("GetSecurityDescriptorRMControl unimplemented in wine"); return; }
    SECURITY_DESCRIPTOR sd; init_sd(&sd);
    sd.Sbz1 = 0x42;
    UCHAR rm = 0xEE;
    sd.Control = 0;
    EXPECT_EQ(U(GetSecurityDescriptorRMControl)(&sd, &rm), ERROR_INVALID_DATA);
    EXPECT_EQ(rm, 0);
    sd.Control = 0x4000;
    EXPECT_EQ(U(GetSecurityDescriptorRMControl)(&sd, &rm), ERROR_SUCCESS);
    EXPECT_EQ(rm, 0x42);
    sd.Control = 0x4000 | SE_DACL_PRESENT;
    EXPECT_EQ(U(GetSecurityDescriptorRMControl)(&sd, &rm), ERROR_SUCCESS);
}
TEST(gap6_sdrm_does_not_set_lasterror) {
    REQUIRE(U(GetSecurityDescriptorRMControl)); if (t_is_wine()) { t_skip("GetSecurityDescriptorRMControl unimplemented in wine"); return; }
    SECURITY_DESCRIPTOR sd; init_sd(&sd);
    UCHAR rm;
    /* resolve first: the GetProcAddress miss in kernel32 would itself set last error 127 */
    DWORD (WINAPI *pfn)(PSECURITY_DESCRIPTOR, PUCHAR) = (void *)U(GetSecurityDescriptorRMControl);
    SET_ERR(4242);
    pfn(&sd, &rm);
    EXPECT_ERR(4242);                      /* the failure is the return value, not last error */
}
TEST(gap6_sdrm_survives_selfrelative) {
    REQUIRE(U(GetSecurityDescriptorRMControl)); if (t_is_wine()) { t_skip("GetSecurityDescriptorRMControl unimplemented in wine"); return; }
    SECURITY_DESCRIPTOR sd; init_sd(&sd);
    UCHAR v = 0x77; set_rm(&sd, &v);
    BYTE rel[64]; DWORD sz = sizeof rel;
    if (MakeSelfRelativeSD(&sd, rel, &sz)) {
        UCHAR rm = 0;
        EXPECT_EQ(U(GetSecurityDescriptorRMControl)(rel, &rm), ERROR_SUCCESS);
        EXPECT_EQ(rm, 0x77);
    } else CHECK(0);
}

/* ---- InitializeSecurityDescriptor ---- */
TEST(gap6_initsd_basic) {
    REQUIRE(U(InitializeSecurityDescriptor));
    BYTE buf[sizeof(SECURITY_DESCRIPTOR) + 16]; memset(buf, 0xCC, sizeof buf);
    SET_ERR(0);
    CHECK(U(InitializeSecurityDescriptor)(buf, SECURITY_DESCRIPTOR_REVISION));
    SECURITY_DESCRIPTOR *sd = (SECURITY_DESCRIPTOR *)buf;
    EXPECT_EQ(sd->Revision, 1);
    EXPECT_EQ(sd->Sbz1, 0);
    EXPECT_EQ(sd->Control, 0);
    EXPECT_EQ(sd->Owner, NULL); EXPECT_EQ(sd->Group, NULL);
    EXPECT_EQ(sd->Sacl, NULL); EXPECT_EQ(sd->Dacl, NULL);
    int bad = 0;
    for (unsigned i = sizeof(SECURITY_DESCRIPTOR); i < sizeof buf; i++) if (buf[i] != 0xCC) bad++;
    EXPECT_EQ(bad, 0);
    CHECK(IsValidSecurityDescriptor(buf));
}
TEST(gap6_initsd_bad_revisions) {
    REQUIRE(U(InitializeSecurityDescriptor));
    DWORD revs[] = {0, 2, 3, 4, 0x100, 0x101, 0x80000001, 0xFFFFFFFF};
    for (unsigned i = 0; i < sizeof revs / sizeof *revs; i++) {
        BYTE buf[sizeof(SECURITY_DESCRIPTOR)]; memset(buf, 0, sizeof buf);
        SET_ERR(0);
        BOOL r = U(InitializeSecurityDescriptor)(buf, revs[i]);
        CHECKF(r == FALSE, "rev %lx", revs[i]);
        CHECKF(GetLastError() == ERROR_UNKNOWN_REVISION, "rev %lx err %lu", revs[i], GetLastError());
    }
}
TEST(gap6_initsd_resets_existing) {
    REQUIRE(U(InitializeSecurityDescriptor));
    SECURITY_DESCRIPTOR sd; BYTE a[68]; init_sd(&sd);
    SetSecurityDescriptorOwner(&sd, g6_sid(a, 5, 1, 18), TRUE);
    SetSecurityDescriptorDacl(&sd, TRUE, NULL, TRUE);
    CHECK(U(InitializeSecurityDescriptor)(&sd, SECURITY_DESCRIPTOR_REVISION));
    EXPECT_EQ(sd.Control, 0); EXPECT_EQ(sd.Owner, NULL);
}
TEST(gap6_initsd_usable_with_setters) {
    REQUIRE(U(InitializeSecurityDescriptor));
    SECURITY_DESCRIPTOR sd;
    CHECK(U(InitializeSecurityDescriptor)(&sd, 1));
    CHECK(SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE));
    EXPECT_EQ(sd.Control & SE_DACL_PRESENT, SE_DACL_PRESENT);
    SECURITY_DESCRIPTOR_CONTROL c; DWORD rev;
    CHECK(GetSecurityDescriptorControl(&sd, &c, &rev));
    EXPECT_EQ(rev, 1);
}
TEST(gap6_initsd_repeat) {
    REQUIRE(U(InitializeSecurityDescriptor));
    SECURITY_DESCRIPTOR sd;
    for (int i = 0; i < 3000; i++) if (!U(InitializeSecurityDescriptor)(&sd, 1)) { CHECK(0); break; }
}

/* ---- InitializeAcl ---- */
TEST(gap6_initacl_revisions) {
    REQUIRE(U(InitializeAcl));
    for (DWORD rev = 0; rev <= 6; rev++) {
        BYTE buf[64]; memset(buf, 0xCC, sizeof buf);
        SET_ERR(0);
        BOOL r = U(InitializeAcl)((PACL)buf, sizeof buf, rev);
        if (rev >= 2 && rev <= 4) {
            CHECKF(r, "rev %lu", rev);
            PACL a = (PACL)buf;
            EXPECT_EQ(a->AclRevision, rev); EXPECT_EQ(a->Sbz1, 0);
            EXPECT_EQ(a->AclSize, 64); EXPECT_EQ(a->AceCount, 0); EXPECT_EQ(a->Sbz2, 0);
            EXPECT_EQ(buf[8], 0xCC);
        } else {
            CHECKF(!r, "rev %lu", rev);
            CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "rev %lu err %lu", rev, GetLastError());
            CHECKF(buf[0] == 0xCC, "modified on failure");
        }
    }
}
TEST(gap6_initacl_bad_big_revisions) {
    REQUIRE(U(InitializeAcl));
    DWORD revs[] = {0x100, 0x102, 0x80000002, 0xFFFFFFFF, 0xFFFFFFFE};
    for (unsigned i = 0; i < 5; i++) {
        BYTE buf[64];
        SET_ERR(0);
        CHECKF(!U(InitializeAcl)((PACL)buf, sizeof buf, revs[i]), "rev %lx", revs[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "rev %lx", revs[i]);
    }
}
TEST(gap6_initacl_too_small) {
    REQUIRE(U(InitializeAcl));
    for (DWORD len = 0; len < 8; len++) {
        BYTE buf[16]; memset(buf, 0xCC, sizeof buf);
        SET_ERR(0);
        BOOL r = U(InitializeAcl)((PACL)buf, len, ACL_REVISION);
        CHECKF(!r, "len %lu", len);
        CHECKF(GetLastError() == ERROR_INSUFFICIENT_BUFFER, "len %lu err %lu", len, GetLastError());
        CHECKF(buf[0] == 0xCC, "modified");
    }
}
TEST(gap6_initacl_length_checked_before_revision) {
    REQUIRE(U(InitializeAcl));
    BYTE buf[16];
    SET_ERR(0);
    CHECK(!U(InitializeAcl)((PACL)buf, 4, 1));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    SET_ERR(0);
    CHECK(!U(InitializeAcl)((PACL)buf, 0, 0xFFFFFFFF));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
}
TEST(gap6_initacl_null_pointer_error_paths) {
    REQUIRE(U(InitializeAcl));
    /* Both failures are decided before the buffer is touched. */
    SET_ERR(0);
    CHECK(!U(InitializeAcl)(NULL, 4, ACL_REVISION));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    SET_ERR(0);
    CHECK(!U(InitializeAcl)(NULL, 64, 1));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!U(InitializeAcl)(NULL, 0x10000, ACL_REVISION));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap6_initacl_size_rounding) {
    REQUIRE(U(InitializeAcl));
    for (DWORD len = 8; len <= 80; len++) {
        BYTE buf[96]; memset(buf, 0xCC, sizeof buf);
        CHECKF(U(InitializeAcl)((PACL)buf, len, ACL_REVISION), "len %lu", len);
        PACL a = (PACL)buf;
        CHECKF(a->AclSize == (len & ~3u), "len %lu AclSize %u", len, a->AclSize);
    }
}
TEST(gap6_initacl_large_sizes) {
    REQUIRE(U(InitializeAcl));
    BYTE *big = (BYTE *)LocalAlloc(LPTR, 0x10010);
    if (!big) { t_skip("alloc"); return; }
    SET_ERR(0);
    CHECK(U(InitializeAcl)((PACL)big, 0xFFFC, ACL_REVISION));
    EXPECT_EQ(((PACL)big)->AclSize, 0xFFFC);
    CHECK(U(InitializeAcl)((PACL)big, 0xFFFC + 0, ACL_REVISION_DS));
    DWORD bad[] = {0xFFFD, 0xFFFF, 0x10000, 0x10001, 0x7FFFFFFF, 0xFFFFFFFF};
    for (unsigned i = 0; i < 6; i++) {
        memset(big, 0xCC, 32);
        SET_ERR(0);
        CHECKF(!U(InitializeAcl)((PACL)big, bad[i], ACL_REVISION), "len %lx", bad[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "len %lx err %lu", bad[i], GetLastError());
        CHECKF(big[0] == 0xCC, "modified");
    }
    LocalFree(big);
}
TEST(gap6_initacl_usable_by_addace) {
    REQUIRE(U(InitializeAcl));
    BYTE buf[128]; BYTE sb[68]; PSID s = g6_sid(sb, 1, 1, 0);
    CHECK(U(InitializeAcl)((PACL)buf, sizeof buf, ACL_REVISION));
    CHECK(IsValidAcl((PACL)buf));
    CHECK(AddAccessAllowedAce((PACL)buf, ACL_REVISION, GENERIC_READ, s));
    EXPECT_EQ(((PACL)buf)->AceCount, 1);
    /* re-initialising discards the ACE */
    CHECK(U(InitializeAcl)((PACL)buf, sizeof buf, ACL_REVISION));
    EXPECT_EQ(((PACL)buf)->AceCount, 0);
}
TEST(gap6_initacl_exact_minimum) {
    REQUIRE(U(InitializeAcl));
    BYTE buf[8];
    CHECK(U(InitializeAcl)((PACL)buf, 8, ACL_REVISION));
    CHECK(IsValidAcl((PACL)buf));
    BYTE sb[68]; PSID s = g6_sid(sb, 1, 1, 0);
    SET_ERR(0);
    CHECK(!AddAccessAllowedAce((PACL)buf, ACL_REVISION, GENERIC_READ, s));
    EXPECT_ERR(ERROR_ALLOTTED_SPACE_EXCEEDED);
}
TEST(gap6_initacl_repeat) {
    REQUIRE(U(InitializeAcl));
    BYTE buf[32];
    for (int i = 0; i < 3000; i++) if (!U(InitializeAcl)((PACL)buf, sizeof buf, ACL_REVISION)) { CHECK(0); break; }
}
