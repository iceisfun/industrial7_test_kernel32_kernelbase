/* gap_12: SetSecurityDescriptor{Dacl,Sacl,Owner,Group,Control,RMControl} and SetSecurityAccessMask.
 * These are pure memory operations on an absolute SD (kernelbase wrappers around the Rtl*SecurityDescriptor
 * routines; failures map the NTSTATUS through RtlNtStatusToDosError). */
#include "sdh.h"

/* ---------------- Dacl ---------------- */
TEST(gap12_dacl_basic) {
    REQUIRE(U(SetSecurityDescriptorDacl));
    SECURITY_DESCRIPTOR sd; BYTE ab[128]; PACL acl = mkacl(ab, sizeof ab, sid_world(), GENERIC_ALL);
    sd_init(&sd);
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetSecurityDescriptorDacl)(&sd, TRUE, acl, FALSE), TRUE);
    CHECK(sd_ctl(&sd) & SE_DACL_PRESENT);
    CHECK(!(sd_ctl(&sd) & SE_DACL_DEFAULTED));
    EXPECT_EQ(sd.Dacl, acl);
    EXPECT_EQ(sd.Revision, 1);
}
TEST(gap12_dacl_defaulted_flag) {
    REQUIRE(U(SetSecurityDescriptorDacl));
    SECURITY_DESCRIPTOR sd; BYTE ab[128]; PACL acl = mkacl(ab, sizeof ab, sid_world(), GENERIC_ALL);
    sd_init(&sd);
    CHECK(U(SetSecurityDescriptorDacl)(&sd, TRUE, acl, TRUE));
    CHECK(sd_ctl(&sd) & SE_DACL_DEFAULTED);
    CHECK(sd_ctl(&sd) & SE_DACL_PRESENT);
    /* a second call with defaulted=FALSE clears the bit */
    CHECK(U(SetSecurityDescriptorDacl)(&sd, TRUE, acl, FALSE));
    CHECK(!(sd_ctl(&sd) & SE_DACL_DEFAULTED));
    CHECK(sd_ctl(&sd) & SE_DACL_PRESENT);
}
TEST(gap12_dacl_null_dacl) {
    REQUIRE(U(SetSecurityDescriptorDacl));
    SECURITY_DESCRIPTOR sd; sd_init(&sd);
    CHECK(U(SetSecurityDescriptorDacl)(&sd, TRUE, NULL, FALSE));
    CHECK(sd_ctl(&sd) & SE_DACL_PRESENT);
    EXPECT_EQ(sd.Dacl, NULL);
    int present = 0; BOOL nul = 0;
    sd_dacl_info(&sd, &present, NULL, NULL, &nul);
    CHECK(present); CHECK(nul);
}
TEST(gap12_dacl_not_present_clears_flag) {
    REQUIRE(U(SetSecurityDescriptorDacl));
    SECURITY_DESCRIPTOR sd; BYTE ab[128]; PACL acl = mkacl(ab, sizeof ab, sid_world(), GENERIC_ALL);
    sd_init(&sd);
    CHECK(U(SetSecurityDescriptorDacl)(&sd, TRUE, acl, FALSE));
    CHECK(U(SetSecurityDescriptorDacl)(&sd, FALSE, acl, FALSE));
    CHECK(!(sd_ctl(&sd) & SE_DACL_PRESENT));
    int present = 1; sd_dacl_info(&sd, &present, NULL, NULL, NULL);
    CHECK(!present);
}
TEST(gap12_dacl_not_present_fresh_sd) {
    REQUIRE(U(SetSecurityDescriptorDacl));
    SECURITY_DESCRIPTOR sd; sd_init(&sd);
    SET_ERR(0xBAD);
    CHECK(U(SetSecurityDescriptorDacl)(&sd, FALSE, NULL, FALSE));
    CHECK(!(sd_ctl(&sd) & SE_DACL_PRESENT));
    EXPECT_EQ(sd_ctl(&sd), 0);
}
TEST(gap12_dacl_preserves_other_fields) {
    REQUIRE(U(SetSecurityDescriptorDacl));
    SECURITY_DESCRIPTOR sd; BYTE ab[128]; PACL acl = mkacl(ab, sizeof ab, sid_world(), GENERIC_ALL);
    sd_init(&sd);
    CHECK(SetSecurityDescriptorOwner(&sd, sid_user(), TRUE));
    CHECK(SetSecurityDescriptorGroup(&sd, sid_admin(), FALSE));
    WORD before = sd_ctl(&sd);
    CHECK(U(SetSecurityDescriptorDacl)(&sd, TRUE, acl, FALSE));
    EXPECT_EQ(sd.Owner, sid_user());
    EXPECT_EQ(sd.Group, sid_admin());
    EXPECT_EQ(sd_ctl(&sd), before | SE_DACL_PRESENT);
}
TEST(gap12_dacl_preserves_protected_bits) {
    REQUIRE(U(SetSecurityDescriptorDacl));
    SECURITY_DESCRIPTOR sd; BYTE ab[128]; PACL acl = mkacl(ab, sizeof ab, sid_world(), GENERIC_ALL);
    sd_init(&sd);
    CHECK(SetSecurityDescriptorControl(&sd, SE_DACL_PROTECTED | SE_SACL_PROTECTED, SE_DACL_PROTECTED | SE_SACL_PROTECTED));
    CHECK(U(SetSecurityDescriptorDacl)(&sd, TRUE, acl, FALSE));
    CHECK((sd_ctl(&sd) & (SE_DACL_PROTECTED | SE_SACL_PROTECTED)) == (SE_DACL_PROTECTED | SE_SACL_PROTECTED));
}
TEST(gap12_dacl_bad_revision) {
    REQUIRE(U(SetSecurityDescriptorDacl));
    static const BYTE revs[] = { 0, 2, 3, 0x7F, 0xFF };
    BYTE ab[128]; PACL acl = mkacl(ab, sizeof ab, sid_world(), GENERIC_ALL);
    for (unsigned i = 0; i < sizeof revs; i++) {
        SECURITY_DESCRIPTOR sd; sd_init(&sd); sd.Revision = revs[i];
        SET_ERR(0xBAD);
        EXPECT_EQ(U(SetSecurityDescriptorDacl)(&sd, TRUE, acl, FALSE), FALSE);
        EXPECT_ERR(ERROR_UNKNOWN_REVISION);
        CHECK(!(sd_ctl(&sd) & SE_DACL_PRESENT));
    }
}
TEST(gap12_dacl_self_relative_rejected) {
    REQUIRE(U(SetSecurityDescriptorDacl));
    SECURITY_DESCRIPTOR sd; BYTE ab[128]; PACL acl = mkacl(ab, sizeof ab, sid_world(), GENERIC_ALL);
    sd_init(&sd);
    sd.Control |= SE_SELF_RELATIVE;
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetSecurityDescriptorDacl)(&sd, TRUE, acl, FALSE), FALSE);
    EXPECT_ERR(ERROR_INVALID_SECURITY_DESCR);
    /* present=FALSE is rejected the same way */
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetSecurityDescriptorDacl)(&sd, FALSE, NULL, FALSE), FALSE);
    EXPECT_ERR(ERROR_INVALID_SECURITY_DESCR);
}
TEST(gap12_dacl_real_self_relative_rejected) {
    REQUIRE(U(SetSecurityDescriptorDacl));
    SECURITY_DESCRIPTOR sd; BYTE ab[128], rel[512]; DWORD n = sizeof rel;
    PACL acl = mkacl(ab, sizeof ab, sid_world(), GENERIC_ALL);
    sd_init(&sd);
    CHECK(SetSecurityDescriptorDacl(&sd, TRUE, acl, FALSE));
    if (!MakeSelfRelativeSD(&sd, rel, &n)) { t_skip("MakeSelfRelativeSD failed"); return; }
    CHECK(sd_ctl(rel) & SE_SELF_RELATIVE);
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetSecurityDescriptorDacl)(rel, TRUE, acl, FALSE), FALSE);
    EXPECT_ERR(ERROR_INVALID_SECURITY_DESCR);
}
TEST(gap12_dacl_nonzero_bool_values) {
    REQUIRE(U(SetSecurityDescriptorDacl));
    static const BOOL vals[] = { 1, 2, 0x7F, 0x80, -1 };
    BYTE ab[128]; PACL acl = mkacl(ab, sizeof ab, sid_world(), GENERIC_ALL);
    for (unsigned i = 0; i < 5; i++) {
        SECURITY_DESCRIPTOR sd; sd_init(&sd);
        CHECK(U(SetSecurityDescriptorDacl)(&sd, vals[i], acl, vals[i]));
        CHECKF(sd_ctl(&sd) & SE_DACL_PRESENT, "present BOOL 0x%x", vals[i]);
        CHECKF(sd_ctl(&sd) & SE_DACL_DEFAULTED, "defaulted BOOL 0x%x", vals[i]);
    }
    /* BOOL whose low byte is zero: the Rtl routine takes BOOLEAN */
    SECURITY_DESCRIPTOR sd; sd_init(&sd);
    BOOL r = U(SetSecurityDescriptorDacl)(&sd, 0x100, acl, FALSE);
    t_note("present=0x100: ret=%d control=0x%04x", r, sd_ctl(&sd));
}
TEST(gap12_dacl_repeat_alternate) {
    REQUIRE(U(SetSecurityDescriptorDacl));
    SECURITY_DESCRIPTOR sd; BYTE a1[128], a2[128]; sd_init(&sd);
    PACL p1 = mkacl(a1, sizeof a1, sid_world(), GENERIC_ALL), p2 = mkacl(a2, sizeof a2, sid_admin(), GENERIC_READ);
    for (int i = 0; i < 1000; i++) {
        PACL p = (i & 1) ? p2 : p1;
        if (!U(SetSecurityDescriptorDacl)(&sd, TRUE, p, i & 1)) { CHECKF(0, "iteration %d failed", i); return; }
        if (sd.Dacl != p) { CHECKF(0, "iteration %d wrong pointer", i); return; }
    }
    CHECK(1);
}
TEST(gap12_dacl_empty_acl_valid) {
    REQUIRE(U(SetSecurityDescriptorDacl));
    SECURITY_DESCRIPTOR sd; BYTE ab[64]; PACL acl = mkacl_empty(ab, sizeof ab);
    sd_init(&sd);
    CHECK(U(SetSecurityDescriptorDacl)(&sd, TRUE, acl, FALSE));
    int present = 0; BOOL nul = 1;
    EXPECT_EQ(sd_dacl_info(&sd, &present, NULL, NULL, &nul), 0);
    CHECK(present); CHECK(!nul);
    CHECK(IsValidSecurityDescriptor(&sd));
}
TEST(gap12_dacl_not_validated_acl_pointer) {
    REQUIRE(U(SetSecurityDescriptorDacl));
    /* the routine stores the pointer without looking at it (not dereferenced here) */
    SECURITY_DESCRIPTOR sd; sd_init(&sd);
    PACL fake = (PACL)(ULONG_PTR)0x1000;
    CHECK(U(SetSecurityDescriptorDacl)(&sd, TRUE, fake, FALSE));
    EXPECT_EQ(sd.Dacl, fake);
}
TEST(gap12_dacl_export_resolution) {
    CHECK(UB(SetSecurityDescriptorDacl) != NULL);
    t_note("kernel32 exports SetSecurityDescriptorDacl: %s", UK(SetSecurityDescriptorDacl) ? "yes" : "no");
}

/* ---------------- Sacl ---------------- */
TEST(gap12_sacl_basic) {
    REQUIRE(U(SetSecurityDescriptorSacl));
    SECURITY_DESCRIPTOR sd; BYTE ab[128]; PACL acl = mkacl_empty(ab, sizeof ab);
    sd_init(&sd);
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetSecurityDescriptorSacl)(&sd, TRUE, acl, FALSE), TRUE);
    CHECK(sd_ctl(&sd) & SE_SACL_PRESENT);
    CHECK(!(sd_ctl(&sd) & SE_SACL_DEFAULTED));
    EXPECT_EQ(sd.Sacl, acl);
    CHECK(!(sd_ctl(&sd) & SE_DACL_PRESENT));
}
TEST(gap12_sacl_defaulted_flag) {
    REQUIRE(U(SetSecurityDescriptorSacl));
    SECURITY_DESCRIPTOR sd; BYTE ab[128]; PACL acl = mkacl_empty(ab, sizeof ab);
    sd_init(&sd);
    CHECK(U(SetSecurityDescriptorSacl)(&sd, TRUE, acl, TRUE));
    CHECK(sd_ctl(&sd) & SE_SACL_DEFAULTED);
    CHECK(U(SetSecurityDescriptorSacl)(&sd, TRUE, acl, FALSE));
    CHECK(!(sd_ctl(&sd) & SE_SACL_DEFAULTED));
}
TEST(gap12_sacl_null_sacl) {
    REQUIRE(U(SetSecurityDescriptorSacl));
    SECURITY_DESCRIPTOR sd; sd_init(&sd);
    CHECK(U(SetSecurityDescriptorSacl)(&sd, TRUE, NULL, FALSE));
    CHECK(sd_ctl(&sd) & SE_SACL_PRESENT);
    EXPECT_EQ(sd.Sacl, NULL);
}
TEST(gap12_sacl_not_present) {
    REQUIRE(U(SetSecurityDescriptorSacl));
    SECURITY_DESCRIPTOR sd; BYTE ab[128]; PACL acl = mkacl_empty(ab, sizeof ab);
    sd_init(&sd);
    CHECK(U(SetSecurityDescriptorSacl)(&sd, TRUE, acl, FALSE));
    CHECK(U(SetSecurityDescriptorSacl)(&sd, FALSE, acl, FALSE));
    CHECK(!(sd_ctl(&sd) & SE_SACL_PRESENT));
    BOOL p = 1, d = 0; PACL a = NULL;
    CHECK(GetSecurityDescriptorSacl(&sd, &p, &a, &d));
    CHECK(!p);
}
TEST(gap12_sacl_bad_revision) {
    REQUIRE(U(SetSecurityDescriptorSacl));
    static const BYTE revs[] = { 0, 2, 0xFF };
    BYTE ab[128]; PACL acl = mkacl_empty(ab, sizeof ab);
    for (unsigned i = 0; i < sizeof revs; i++) {
        SECURITY_DESCRIPTOR sd; sd_init(&sd); sd.Revision = revs[i];
        SET_ERR(0xBAD);
        EXPECT_EQ(U(SetSecurityDescriptorSacl)(&sd, TRUE, acl, FALSE), FALSE);
        EXPECT_ERR(ERROR_UNKNOWN_REVISION);
    }
}
TEST(gap12_sacl_self_relative_rejected) {
    REQUIRE(U(SetSecurityDescriptorSacl));
    SECURITY_DESCRIPTOR sd; sd_init(&sd); sd.Control |= SE_SELF_RELATIVE;
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetSecurityDescriptorSacl)(&sd, TRUE, NULL, FALSE), FALSE);
    EXPECT_ERR(ERROR_INVALID_SECURITY_DESCR);
}
TEST(gap12_sacl_independent_of_dacl) {
    REQUIRE(U(SetSecurityDescriptorSacl)); REQUIRE(U(SetSecurityDescriptorDacl));
    SECURITY_DESCRIPTOR sd; BYTE a1[128], a2[128]; sd_init(&sd);
    PACL d = mkacl(a1, sizeof a1, sid_world(), GENERIC_ALL), s = mkacl_empty(a2, sizeof a2);
    CHECK(U(SetSecurityDescriptorDacl)(&sd, TRUE, d, TRUE));
    CHECK(U(SetSecurityDescriptorSacl)(&sd, TRUE, s, FALSE));
    CHECK((sd_ctl(&sd) & (SE_DACL_PRESENT | SE_DACL_DEFAULTED | SE_SACL_PRESENT)) == (SE_DACL_PRESENT | SE_DACL_DEFAULTED | SE_SACL_PRESENT));
    CHECK(!(sd_ctl(&sd) & SE_SACL_DEFAULTED));
    EXPECT_EQ(sd.Dacl, d); EXPECT_EQ(sd.Sacl, s);
    CHECK(U(SetSecurityDescriptorSacl)(&sd, FALSE, NULL, FALSE));
    CHECK(sd_ctl(&sd) & SE_DACL_PRESENT);
    EXPECT_EQ(sd.Dacl, d);
}

/* ---------------- Owner / Group ---------------- */
#define OG_TESTS(NAME, FN, FIELD, DEFBIT, OTHERFIELD) \
TEST(gap12_##NAME##_basic) { \
    REQUIRE(U(FN)); SECURITY_DESCRIPTOR sd; sd_init(&sd); \
    SET_ERR(0xBAD); EXPECT_EQ(U(FN)(&sd, sid_user(), FALSE), TRUE); \
    EXPECT_EQ(sd.FIELD, sid_user()); CHECK(!(sd_ctl(&sd) & DEFBIT)); \
    EXPECT_EQ(sd.OTHERFIELD, NULL); } \
TEST(gap12_##NAME##_defaulted) { \
    REQUIRE(U(FN)); SECURITY_DESCRIPTOR sd; sd_init(&sd); \
    CHECK(U(FN)(&sd, sid_admin(), TRUE)); CHECK(sd_ctl(&sd) & DEFBIT); EXPECT_EQ(sd.FIELD, sid_admin()); \
    CHECK(U(FN)(&sd, sid_admin(), FALSE)); CHECK(!(sd_ctl(&sd) & DEFBIT)); } \
TEST(gap12_##NAME##_null_sid) { \
    REQUIRE(U(FN)); SECURITY_DESCRIPTOR sd; sd_init(&sd); \
    CHECK(U(FN)(&sd, sid_user(), TRUE)); \
    SET_ERR(0xBAD); EXPECT_EQ(U(FN)(&sd, NULL, FALSE), TRUE); \
    EXPECT_EQ(sd.FIELD, NULL); CHECK(!(sd_ctl(&sd) & DEFBIT)); } \
TEST(gap12_##NAME##_null_sid_keeps_default_flag) { \
    REQUIRE(U(FN)); SECURITY_DESCRIPTOR sd; sd_init(&sd); \
    CHECK(U(FN)(&sd, NULL, TRUE)); EXPECT_EQ(sd.FIELD, NULL); CHECK(sd_ctl(&sd) & DEFBIT); } \
TEST(gap12_##NAME##_bad_revision) { \
    REQUIRE(U(FN)); static const BYTE revs[] = { 0, 2, 0xFF }; \
    for (unsigned i = 0; i < sizeof revs; i++) { SECURITY_DESCRIPTOR sd; sd_init(&sd); sd.Revision = revs[i]; \
        SET_ERR(0xBAD); EXPECT_EQ(U(FN)(&sd, sid_user(), FALSE), FALSE); EXPECT_ERR(ERROR_UNKNOWN_REVISION); \
        EXPECT_EQ(sd.FIELD, NULL); } } \
TEST(gap12_##NAME##_self_relative_rejected) { \
    REQUIRE(U(FN)); SECURITY_DESCRIPTOR sd; sd_init(&sd); sd.Control |= SE_SELF_RELATIVE; \
    SET_ERR(0xBAD); EXPECT_EQ(U(FN)(&sd, sid_user(), FALSE), FALSE); EXPECT_ERR(ERROR_INVALID_SECURITY_DESCR); } \
TEST(gap12_##NAME##_not_validated_pointer) { \
    REQUIRE(U(FN)); SECURITY_DESCRIPTOR sd; sd_init(&sd); \
    PSID fake = (PSID)(ULONG_PTR)0x1000; CHECK(U(FN)(&sd, fake, FALSE)); EXPECT_EQ(sd.FIELD, fake); } \
TEST(gap12_##NAME##_preserves_dacl_state) { \
    REQUIRE(U(FN)); SECURITY_DESCRIPTOR sd; BYTE ab[128]; PACL acl = mkacl(ab, sizeof ab, sid_world(), GENERIC_ALL); sd_init(&sd); \
    CHECK(SetSecurityDescriptorDacl(&sd, TRUE, acl, TRUE)); \
    CHECK(U(FN)(&sd, sid_user(), TRUE)); \
    CHECK((sd_ctl(&sd) & (SE_DACL_PRESENT | SE_DACL_DEFAULTED)) == (SE_DACL_PRESENT | SE_DACL_DEFAULTED)); EXPECT_EQ(sd.Dacl, acl); } \
TEST(gap12_##NAME##_repeat) { \
    REQUIRE(U(FN)); SECURITY_DESCRIPTOR sd; sd_init(&sd); \
    for (int i = 0; i < 500; i++) { PSID s = (i & 1) ? sid_admin() : sid_user(); \
        if (!U(FN)(&sd, s, i & 1) || sd.FIELD != s) { CHECKF(0, "iteration %d", i); return; } } CHECK(1); }

OG_TESTS(owner, SetSecurityDescriptorOwner, Owner, SE_OWNER_DEFAULTED, Group)
OG_TESTS(group, SetSecurityDescriptorGroup, Group, SE_GROUP_DEFAULTED, Owner)

TEST(gap12_owner_group_independent) {
    REQUIRE(U(SetSecurityDescriptorOwner)); REQUIRE(U(SetSecurityDescriptorGroup));
    SECURITY_DESCRIPTOR sd; sd_init(&sd);
    CHECK(U(SetSecurityDescriptorOwner)(&sd, sid_user(), TRUE));
    CHECK(U(SetSecurityDescriptorGroup)(&sd, sid_admin(), FALSE));
    CHECK(sd_ctl(&sd) & SE_OWNER_DEFAULTED);
    CHECK(!(sd_ctl(&sd) & SE_GROUP_DEFAULTED));
    CHECK(U(SetSecurityDescriptorGroup)(&sd, sid_admin(), TRUE));
    CHECK(sd_ctl(&sd) & SE_OWNER_DEFAULTED);
    CHECK(sd_ctl(&sd) & SE_GROUP_DEFAULTED);
    EXPECT_EQ(sd.Owner, sid_user());
}
TEST(gap12_owner_roundtrip_getter) {
    REQUIRE(U(SetSecurityDescriptorOwner));
    SECURITY_DESCRIPTOR sd; PSID o = NULL; BOOL d = 1; sd_init(&sd);
    CHECK(U(SetSecurityDescriptorOwner)(&sd, sid_user(), FALSE));
    CHECK(GetSecurityDescriptorOwner(&sd, &o, &d));
    EXPECT_EQ(o, sid_user()); CHECK(!d);
    CHECK(U(SetSecurityDescriptorOwner)(&sd, sid_user(), TRUE));
    CHECK(GetSecurityDescriptorOwner(&sd, &o, &d));
    CHECK(d);
}
TEST(gap12_group_roundtrip_getter) {
    REQUIRE(U(SetSecurityDescriptorGroup));
    SECURITY_DESCRIPTOR sd; PSID o = NULL; BOOL d = 1; sd_init(&sd);
    CHECK(U(SetSecurityDescriptorGroup)(&sd, sid_admin(), FALSE));
    CHECK(GetSecurityDescriptorGroup(&sd, &o, &d));
    EXPECT_EQ(o, sid_admin()); CHECK(!d);
}
TEST(gap12_full_descriptor_valid_and_convertible) {
    REQUIRE(U(SetSecurityDescriptorOwner)); REQUIRE(U(SetSecurityDescriptorGroup));
    REQUIRE(U(SetSecurityDescriptorDacl)); REQUIRE(U(SetSecurityDescriptorSacl));
    SECURITY_DESCRIPTOR sd; BYTE a1[128], a2[128], rel[1024]; DWORD n = sizeof rel;
    sd_init(&sd);
    PACL d = mkacl(a1, sizeof a1, sid_world(), GENERIC_READ), s = mkacl_empty(a2, sizeof a2);
    CHECK(U(SetSecurityDescriptorOwner)(&sd, sid_user(), FALSE));
    CHECK(U(SetSecurityDescriptorGroup)(&sd, sid_admin(), FALSE));
    CHECK(U(SetSecurityDescriptorDacl)(&sd, TRUE, d, FALSE));
    CHECK(U(SetSecurityDescriptorSacl)(&sd, TRUE, s, FALSE));
    CHECK(IsValidSecurityDescriptor(&sd));
    CHECK(MakeSelfRelativeSD(&sd, rel, &n));
    CHECK(IsValidSecurityDescriptor(rel));
    CHECK(sd_ctl(rel) & SE_SELF_RELATIVE);
    int present = 0; PSID sid = NULL; DWORD mask = 0;
    EXPECT_EQ(sd_dacl_info(rel, &present, &sid, &mask, NULL), 1);
    EXPECT_EQ(mask, GENERIC_READ);
    CHECK(EqualSid(sid, sid_world()));
    LPWSTR str = NULL;
    if (ConvertSecurityDescriptorToStringSecurityDescriptorW(&sd, 1, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, &str, NULL)) {
        CHECK(wcsstr(str, L"O:") != NULL); CHECK(wcsstr(str, L"G:BA") != NULL); CHECK(wcsstr(str, L"D:") != NULL);
        LocalFree(str);
    } else t_note("SDDL conversion failed %lu", GetLastError());
}

/* ---------------- Control ---------------- */
static const struct { WORD bit; const char *name; } k_ctl_ok[] = {
    { SE_DACL_AUTO_INHERIT_REQ, "DACL_AUTO_INHERIT_REQ" }, { SE_SACL_AUTO_INHERIT_REQ, "SACL_AUTO_INHERIT_REQ" },
    { SE_DACL_AUTO_INHERITED, "DACL_AUTO_INHERITED" },     { SE_SACL_AUTO_INHERITED, "SACL_AUTO_INHERITED" },
    { SE_DACL_PROTECTED, "DACL_PROTECTED" },               { SE_SACL_PROTECTED, "SACL_PROTECTED" },
};
TEST(gap12_control_set_each_bit) {
    REQUIRE(U(SetSecurityDescriptorControl));
    for (unsigned i = 0; i < 6; i++) {
        SECURITY_DESCRIPTOR sd; sd_init(&sd);
        SET_ERR(0xBAD);
        BOOL r = U(SetSecurityDescriptorControl)(&sd, k_ctl_ok[i].bit, k_ctl_ok[i].bit);
        CHECKF(r, "set %s err=%lu", k_ctl_ok[i].name, GetLastError());
        CHECKF(sd_ctl(&sd) == k_ctl_ok[i].bit, "%s control=0x%04x", k_ctl_ok[i].name, sd_ctl(&sd));
        r = U(SetSecurityDescriptorControl)(&sd, k_ctl_ok[i].bit, 0);
        CHECKF(r, "clear %s", k_ctl_ok[i].name);
        CHECKF(sd_ctl(&sd) == 0, "%s cleared control=0x%04x", k_ctl_ok[i].name, sd_ctl(&sd));
    }
}
TEST(gap12_control_matches_getter) {
    REQUIRE(U(SetSecurityDescriptorControl));
    SECURITY_DESCRIPTOR sd; SECURITY_DESCRIPTOR_CONTROL c = 0; DWORD rev = 0; sd_init(&sd);
    CHECK(U(SetSecurityDescriptorControl)(&sd, SE_DACL_PROTECTED, SE_DACL_PROTECTED));
    CHECK(GetSecurityDescriptorControl(&sd, &c, &rev));
    EXPECT_EQ(c, SE_DACL_PROTECTED); EXPECT_EQ(rev, 1);
}
TEST(gap12_control_interest_masks_set) {
    REQUIRE(U(SetSecurityDescriptorControl));
    SECURITY_DESCRIPTOR sd; sd_init(&sd);
    /* P1 (gap_12): ToSet bits outside BitsOfInterest are not ignored: ntdll!RtlSetControlSecurityDescriptor rejects
     * the call with STATUS_INVALID_PARAMETER when (ToSet & ~Interest) != 0 and leaves the control word unchanged. */
    SET_ERR(0xBAD);
    CHECK(!U(SetSecurityDescriptorControl)(&sd, SE_DACL_PROTECTED, SE_DACL_PROTECTED | SE_SACL_PROTECTED));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    EXPECT_EQ(sd_ctl(&sd), 0);
    CHECK(U(SetSecurityDescriptorControl)(&sd, SE_DACL_PROTECTED, SE_DACL_PROTECTED));
    EXPECT_EQ(sd_ctl(&sd), SE_DACL_PROTECTED);
    SET_ERR(0xBAD);
    CHECK(!U(SetSecurityDescriptorControl)(&sd, 0, SE_SACL_PROTECTED | SE_DACL_AUTO_INHERITED));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    EXPECT_EQ(sd_ctl(&sd), SE_DACL_PROTECTED);
    /* interest=SACL_PROTECTED, set=0 only clears that one */
    CHECK(U(SetSecurityDescriptorControl)(&sd, SE_SACL_PROTECTED, SE_SACL_PROTECTED));
    CHECK(U(SetSecurityDescriptorControl)(&sd, SE_SACL_PROTECTED, 0));
    EXPECT_EQ(sd_ctl(&sd), SE_DACL_PROTECTED);
}
TEST(gap12_control_zero_zero_noop) {
    REQUIRE(U(SetSecurityDescriptorControl));
    SECURITY_DESCRIPTOR sd; sd_init(&sd);
    CHECK(U(SetSecurityDescriptorControl)(&sd, SE_DACL_PROTECTED, SE_DACL_PROTECTED));
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetSecurityDescriptorControl)(&sd, 0, 0), TRUE);
    EXPECT_EQ(sd_ctl(&sd), SE_DACL_PROTECTED);
}
TEST(gap12_control_all_valid_at_once) {
    REQUIRE(U(SetSecurityDescriptorControl));
    SECURITY_DESCRIPTOR sd; sd_init(&sd);
    WORD all = SE_DACL_AUTO_INHERIT_REQ | SE_SACL_AUTO_INHERIT_REQ | SE_DACL_AUTO_INHERITED | SE_SACL_AUTO_INHERITED | SE_DACL_PROTECTED | SE_SACL_PROTECTED;
    CHECK(U(SetSecurityDescriptorControl)(&sd, all, all));
    EXPECT_EQ(sd_ctl(&sd), all);
    CHECK(U(SetSecurityDescriptorControl)(&sd, all, 0));
    EXPECT_EQ(sd_ctl(&sd), 0);
}
TEST(gap12_control_preserves_present_bits) {
    REQUIRE(U(SetSecurityDescriptorControl));
    SECURITY_DESCRIPTOR sd; BYTE ab[128]; PACL acl = mkacl(ab, sizeof ab, sid_world(), GENERIC_ALL); sd_init(&sd);
    CHECK(SetSecurityDescriptorDacl(&sd, TRUE, acl, TRUE));
    CHECK(SetSecurityDescriptorOwner(&sd, sid_user(), TRUE));
    CHECK(U(SetSecurityDescriptorControl)(&sd, SE_DACL_PROTECTED | SE_DACL_AUTO_INHERITED, SE_DACL_PROTECTED));
    CHECK((sd_ctl(&sd) & (SE_DACL_PRESENT | SE_DACL_DEFAULTED | SE_OWNER_DEFAULTED)) == (SE_DACL_PRESENT | SE_DACL_DEFAULTED | SE_OWNER_DEFAULTED));
    CHECK(sd_ctl(&sd) & SE_DACL_PROTECTED);
    CHECK(!(sd_ctl(&sd) & SE_DACL_AUTO_INHERITED));
}
TEST(gap12_control_immutable_bits_rejected) {
    REQUIRE(U(SetSecurityDescriptorControl));
    static const WORD bad[] = { SE_OWNER_DEFAULTED, SE_GROUP_DEFAULTED, SE_DACL_PRESENT, SE_DACL_DEFAULTED, SE_SACL_PRESENT, SE_SACL_DEFAULTED, SE_RM_CONTROL_VALID, SE_SELF_RELATIVE };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        SECURITY_DESCRIPTOR sd; sd_init(&sd);
        SET_ERR(0xBAD);
        BOOL r = U(SetSecurityDescriptorControl)(&sd, bad[i], bad[i]);
        CHECKF(r == FALSE, "interest 0x%04x accepted", bad[i]);
        if (!r) EXPECT_ERR(ERROR_INVALID_PARAMETER);
        CHECKF(sd_ctl(&sd) == 0, "control changed 0x%04x for bit 0x%04x", sd_ctl(&sd), bad[i]);
    }
}
TEST(gap12_control_immutable_in_toset_only) {
    REQUIRE(U(SetSecurityDescriptorControl));
    SECURITY_DESCRIPTOR sd; sd_init(&sd);
    SET_ERR(0xBAD);
    BOOL r = U(SetSecurityDescriptorControl)(&sd, SE_DACL_PROTECTED, SE_DACL_PROTECTED | SE_DACL_PRESENT);
    t_note("toset has DACL_PRESENT: ret=%d err=%lu control=0x%04x", r, GetLastError(), sd_ctl(&sd));
}
TEST(gap12_control_untrusted_server_bits) {
    REQUIRE(U(SetSecurityDescriptorControl));
    SECURITY_DESCRIPTOR sd; sd_init(&sd);
    SET_ERR(0xBAD);
    BOOL r = U(SetSecurityDescriptorControl)(&sd, SE_DACL_UNTRUSTED | SE_SERVER_SECURITY, SE_DACL_UNTRUSTED | SE_SERVER_SECURITY);
    t_note("DACL_UNTRUSTED|SERVER_SECURITY: ret=%d err=%lu control=0x%04x", r, GetLastError(), sd_ctl(&sd));
}
TEST(gap12_control_bad_revision) {
    REQUIRE(U(SetSecurityDescriptorControl));
    SECURITY_DESCRIPTOR sd; sd_init(&sd); sd.Revision = 9;
    SET_ERR(0xBAD);
    BOOL r = U(SetSecurityDescriptorControl)(&sd, SE_DACL_PROTECTED, SE_DACL_PROTECTED);
    t_note("revision 9: ret=%d err=%lu control=0x%04x", r, GetLastError(), sd_ctl(&sd));
}
TEST(gap12_control_self_relative_sd) {
    REQUIRE(U(SetSecurityDescriptorControl));
    SECURITY_DESCRIPTOR sd; BYTE ab[128], rel[512]; DWORD n = sizeof rel; PACL acl = mkacl(ab, sizeof ab, sid_world(), GENERIC_ALL);
    sd_init(&sd); SetSecurityDescriptorDacl(&sd, TRUE, acl, FALSE);
    if (!MakeSelfRelativeSD(&sd, rel, &n)) { t_skip("MakeSelfRelativeSD"); return; }
    SET_ERR(0xBAD);
    BOOL r = U(SetSecurityDescriptorControl)(rel, SE_DACL_PROTECTED, SE_DACL_PROTECTED);
    t_note("self-relative SD: ret=%d err=%lu control=0x%04x", r, GetLastError(), sd_ctl(rel));
}
TEST(gap12_control_protected_flag_survives_conversion) {
    REQUIRE(U(SetSecurityDescriptorControl));
    SECURITY_DESCRIPTOR sd; BYTE ab[128], rel[512]; DWORD n = sizeof rel; PACL acl = mkacl(ab, sizeof ab, sid_world(), GENERIC_ALL);
    sd_init(&sd); SetSecurityDescriptorDacl(&sd, TRUE, acl, FALSE);
    CHECK(U(SetSecurityDescriptorControl)(&sd, SE_DACL_PROTECTED | SE_DACL_AUTO_INHERITED, SE_DACL_PROTECTED | SE_DACL_AUTO_INHERITED));
    CHECK(MakeSelfRelativeSD(&sd, rel, &n));
    CHECK((sd_ctl(rel) & (SE_DACL_PROTECTED | SE_DACL_AUTO_INHERITED)) == (SE_DACL_PROTECTED | SE_DACL_AUTO_INHERITED));
}

/* ---------------- RMControl ---------------- */
TEST(gap12_rmcontrol_set_value) {
    REQUIRE(U(SetSecurityDescriptorRMControl)); if (t_is_wine()) { t_skip("emulator stubs RMControl"); return; }
    SECURITY_DESCRIPTOR sd; UCHAR rm = 0x5A; sd_init(&sd);
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetSecurityDescriptorRMControl)(&sd, &rm), ERROR_SUCCESS);
    CHECK(sd_ctl(&sd) & SE_RM_CONTROL_VALID);
    EXPECT_EQ(sd.Sbz1, 0x5A);
}
TEST(gap12_rmcontrol_values_table) {
    REQUIRE(U(SetSecurityDescriptorRMControl)); if (t_is_wine()) { t_skip("emulator stubs RMControl"); return; }
    static const UCHAR v[] = { 0, 1, 0x7F, 0x80, 0xFF, 0xA5 };
    for (unsigned i = 0; i < sizeof v; i++) {
        SECURITY_DESCRIPTOR sd; UCHAR rm = v[i]; sd_init(&sd);
        EXPECT_EQ(U(SetSecurityDescriptorRMControl)(&sd, &rm), 0);
        CHECKF(sd_ctl(&sd) & SE_RM_CONTROL_VALID, "value 0x%02x", v[i]);
        CHECKF(sd.Sbz1 == v[i], "Sbz1 0x%02x for 0x%02x", sd.Sbz1, v[i]);
    }
}
TEST(gap12_rmcontrol_null_clears) {
    REQUIRE(U(SetSecurityDescriptorRMControl)); if (t_is_wine()) { t_skip("emulator stubs RMControl"); return; }
    SECURITY_DESCRIPTOR sd; UCHAR rm = 0x33; sd_init(&sd);
    CHECK(U(SetSecurityDescriptorRMControl)(&sd, &rm) == 0);
    CHECK(sd_ctl(&sd) & SE_RM_CONTROL_VALID);
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetSecurityDescriptorRMControl)(&sd, NULL), 0);
    CHECK(!(sd_ctl(&sd) & SE_RM_CONTROL_VALID));
    EXPECT_EQ(sd.Sbz1, 0);
}
TEST(gap12_rmcontrol_null_on_fresh) {
    REQUIRE(U(SetSecurityDescriptorRMControl)); if (t_is_wine()) { t_skip("emulator stubs RMControl"); return; }
    SECURITY_DESCRIPTOR sd; sd_init(&sd);
    EXPECT_EQ(U(SetSecurityDescriptorRMControl)(&sd, NULL), 0);
    EXPECT_EQ(sd_ctl(&sd), 0);
}
TEST(gap12_rmcontrol_getter_roundtrip) {
    REQUIRE(U(SetSecurityDescriptorRMControl)); if (t_is_wine()) { t_skip("emulator stubs RMControl"); return; } REQUIRE(U(GetSecurityDescriptorRMControl));
    SECURITY_DESCRIPTOR sd; UCHAR rm = 0xC3, out = 0; sd_init(&sd);
    SET_ERR(0xBAD);
    EXPECT_EQ(U(GetSecurityDescriptorRMControl)(&sd, &out), ERROR_INVALID_DATA);
    CHECK(U(SetSecurityDescriptorRMControl)(&sd, &rm) == 0);
    out = 0;
    EXPECT_EQ(U(GetSecurityDescriptorRMControl)(&sd, &out), ERROR_SUCCESS);
    EXPECT_EQ(out, 0xC3);
    CHECK(U(SetSecurityDescriptorRMControl)(&sd, NULL) == 0);
    EXPECT_EQ(U(GetSecurityDescriptorRMControl)(&sd, &out), ERROR_INVALID_DATA);
}
TEST(gap12_rmcontrol_preserves_control_bits) {
    REQUIRE(U(SetSecurityDescriptorRMControl)); if (t_is_wine()) { t_skip("emulator stubs RMControl"); return; }
    SECURITY_DESCRIPTOR sd; UCHAR rm = 7; BYTE ab[128]; PACL acl = mkacl(ab, sizeof ab, sid_world(), GENERIC_ALL); sd_init(&sd);
    SetSecurityDescriptorDacl(&sd, TRUE, acl, FALSE);
    SetSecurityDescriptorControl(&sd, SE_DACL_PROTECTED, SE_DACL_PROTECTED);
    CHECK(U(SetSecurityDescriptorRMControl)(&sd, &rm) == 0);
    EXPECT_EQ(sd_ctl(&sd), SE_DACL_PRESENT | SE_DACL_PROTECTED | SE_RM_CONTROL_VALID);
    CHECK(U(SetSecurityDescriptorRMControl)(&sd, NULL) == 0);
    EXPECT_EQ(sd_ctl(&sd), SE_DACL_PRESENT | SE_DACL_PROTECTED);
    EXPECT_EQ(sd.Revision, 1);
}
TEST(gap12_rmcontrol_return_ignores_bad_revision) {
    REQUIRE(U(SetSecurityDescriptorRMControl)); if (t_is_wine()) { t_skip("emulator stubs RMControl"); return; }
    SECURITY_DESCRIPTOR sd; UCHAR rm = 1; sd_init(&sd); sd.Revision = 7;
    SET_ERR(0xBAD);
    DWORD r = U(SetSecurityDescriptorRMControl)(&sd, &rm);
    EXPECT_EQ(r, 0);
    t_note("rev 7: control=0x%04x Sbz1=%u", sd_ctl(&sd), sd.Sbz1);
}

/* ---------------- SetSecurityAccessMask ---------------- */
static const struct { DWORD si; DWORD want; } k_sam[] = {
    { 0, 0 },
    { OWNER_SECURITY_INFORMATION, WRITE_OWNER },
    { GROUP_SECURITY_INFORMATION, WRITE_OWNER },
    { DACL_SECURITY_INFORMATION, WRITE_DAC },
    { SACL_SECURITY_INFORMATION, ACCESS_SYSTEM_SECURITY },
    { 0x10 /*LABEL*/, WRITE_OWNER },
    { 0x20 /*ATTRIBUTE*/, WRITE_DAC },
    { 0x40 /*SCOPE*/, ACCESS_SYSTEM_SECURITY },
    { 0x80 /*PROCESS_TRUST_LABEL*/, WRITE_DAC },
    { 0x100 /*ACCESS_FILTER*/, WRITE_DAC },
    { 0x10000 /*BACKUP*/, WRITE_DAC | WRITE_OWNER | ACCESS_SYSTEM_SECURITY },
    { OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION, WRITE_OWNER },
    { OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, WRITE_OWNER | WRITE_DAC },
    { DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION, WRITE_DAC | ACCESS_SYSTEM_SECURITY },
    { OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, WRITE_OWNER | WRITE_DAC },
    { OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION, WRITE_OWNER | WRITE_DAC | ACCESS_SYSTEM_SECURITY },
    { 0x1EC, WRITE_DAC | ACCESS_SYSTEM_SECURITY },
    { PROTECTED_DACL_SECURITY_INFORMATION, 0 },
    { UNPROTECTED_DACL_SECURITY_INFORMATION, 0 },
    { PROTECTED_SACL_SECURITY_INFORMATION, 0 },
    { UNPROTECTED_SACL_SECURITY_INFORMATION, 0 },
    /* P1 (gap_12): kernelbase!SetSecurityAccessMask adds ACCESS_SYSTEM_SECURITY for the PROTECTED/UNPROTECTED_SACL bits
     * (0x50000000) only when a SACL/label/attribute/scope/etc. bit (mask 0x1f8) is also present; DACL (0x4) is not in
     * 0x1f8, so DACL|PROTECTED_SACL needs only WRITE_DAC (consistent with PROTECTED_SACL alone needing 0 above). */
    { DACL_SECURITY_INFORMATION | PROTECTED_SACL_SECURITY_INFORMATION, WRITE_DAC },
    { DACL_SECURITY_INFORMATION | UNPROTECTED_SACL_SECURITY_INFORMATION, WRITE_DAC },
    { DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, WRITE_DAC },
    { OWNER_SECURITY_INFORMATION | PROTECTED_SACL_SECURITY_INFORMATION, WRITE_OWNER },
    { 0xFFFFFFFF, WRITE_DAC | WRITE_OWNER | ACCESS_SYSTEM_SECURITY },
    { 0x00000200, 0 },      /* undefined bit */
    { 0x00020000, 0 },
};
TEST(gap12_accessmask_table) {
    REQUIRE(U(SetSecurityAccessMask)); if (t_is_wine()) { t_skip("emulator lacks SetSecurityAccessMask"); return; }
    for (unsigned i = 0; i < sizeof k_sam / sizeof k_sam[0]; i++) {
        DWORD got = 0xDEADBEEF;
        U(SetSecurityAccessMask)(k_sam[i].si, &got);
        CHECKF(got == k_sam[i].want, "SI=0x%08lx got 0x%08lx want 0x%08lx", k_sam[i].si, got, k_sam[i].want);
    }
}
TEST(gap12_accessmask_always_overwrites_output) {
    REQUIRE(U(SetSecurityAccessMask)); if (t_is_wine()) { t_skip("emulator lacks SetSecurityAccessMask"); return; }
    DWORD v = 0xFFFFFFFF;
    U(SetSecurityAccessMask)(0, &v);
    EXPECT_EQ(v, 0);
    v = 0x12345678;
    U(SetSecurityAccessMask)(DACL_SECURITY_INFORMATION, &v);
    EXPECT_EQ(v, WRITE_DAC);
}
TEST(gap12_accessmask_single_bits_sweep) {
    REQUIRE(U(SetSecurityAccessMask)); if (t_is_wine()) { t_skip("emulator lacks SetSecurityAccessMask"); return; }
    /* output must only ever hold WRITE_DAC / WRITE_OWNER / ACCESS_SYSTEM_SECURITY */
    for (int b = 0; b < 32; b++) {
        DWORD v = 0xFFFFFFFF;
        U(SetSecurityAccessMask)(1u << b, &v);
        CHECKF((v & ~(WRITE_DAC | WRITE_OWNER | ACCESS_SYSTEM_SECURITY)) == 0, "bit %d -> 0x%08lx", b, v);
    }
}
TEST(gap12_accessmask_monotonic_union) {
    REQUIRE(U(SetSecurityAccessMask)); if (t_is_wine()) { t_skip("emulator lacks SetSecurityAccessMask"); return; }
    /* the mask for a union of bits is the union of the masks for each bit, except for the SACL-protection promotion */
    DWORD parts[] = { 1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x10000 };
    DWORD all = 0, u = 0;
    for (unsigned i = 0; i < 10; i++) { DWORD m; U(SetSecurityAccessMask)(parts[i], &m); u |= m; all |= parts[i]; }
    DWORD m; U(SetSecurityAccessMask)(all, &m);
    EXPECT_EQ(m, u);
}
TEST(gap12_accessmask_mask_usable_for_open) {
    REQUIRE(U(SetSecurityAccessMask)); if (t_is_wine()) { t_skip("emulator lacks SetSecurityAccessMask"); return; }
    /* the mask returned for DACL is accepted by CreateEvent-based handle duplication (WRITE_DAC) */
    DWORD m = 0; U(SetSecurityAccessMask)(DACL_SECURITY_INFORMATION, &m);
    HANDLE e = CreateEventW(NULL, TRUE, FALSE, NULL), d = NULL;
    CHECK(e != NULL);
    CHECK(DuplicateHandle(GetCurrentProcess(), e, GetCurrentProcess(), &d, m, FALSE, 0));
    if (d) CloseHandle(d);
    CloseHandle(e);
}
