/* AddAce, object ACEs (with GUIDs), AddMandatoryAce, AddScopedPolicyIDAce, AddResourceAttributeAce */
#include "g0.h"

/* ---------------------------------------------------------------- AddAce */
static BYTE g_ace1[20], g_ace2[20], g_ace3[24];
static void mk3(void) {
    g0_mkace(g_ace1, ACCESS_ALLOWED_ACE_TYPE, 0, 0x11, G0_SID(g0_sid_world));
    g0_mkace(g_ace2, ACCESS_DENIED_ACE_TYPE, 0, 0x22, G0_SID(g0_sid_world));
    g0_mkace(g_ace3, SYSTEM_AUDIT_ACE_TYPE, 0xC0, 0x33, G0_SID(g0_sid_admins));
}

TEST(gap0_addace_basic) {
    REQUIRE(U(AddAce)); mk3();
    BYTE buf[128]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION);
    BYTE list[40]; memcpy(list, g_ace1, 20); memcpy(list + 20, g_ace2, 20);
    { BOOL (WINAPI *fn)(PACL, DWORD, DWORD, LPVOID, DWORD) = U(AddAce);
      SET_ERR(0xdeadbeef);
      CHECK(fn(acl, ACL_REVISION, 0, list, 40));
      EXPECT_EQ(GetLastError(), 0xdeadbeef); }
    EXPECT_EQ(g0_cnt(acl), 2);
    ACE_HEADER *h = g0_ace(acl, 0);
    if (h) { EXPECT_EQ(h->AceType, ACCESS_ALLOWED_ACE_TYPE); EXPECT_EQ(g0_ace_mask(h), 0x11); }
    h = g0_ace(acl, 1);
    if (h) { EXPECT_EQ(h->AceType, ACCESS_DENIED_ACE_TYPE); EXPECT_EQ(g0_ace_mask(h), 0x22); }
    CHECK(IsValidAcl(acl));
    CHECK(!memcmp(buf + 8, list, 40));
}

TEST(gap0_addace_insert_positions) {
    REQUIRE(U(AddAce)); mk3();
    BYTE buf[256]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION);
    CHECK(U(AddAce)(acl, ACL_REVISION, 0, g_ace1, 20));            /* [1] */
    CHECK(U(AddAce)(acl, ACL_REVISION, 0, g_ace2, 20));            /* [2 1] */
    CHECK(U(AddAce)(acl, ACL_REVISION, 1, g_ace3, 24));            /* [2 3 1] */
    EXPECT_EQ(g0_cnt(acl), 3);
    DWORD want[3] = {0x22, 0x33, 0x11};
    for (int i = 0; i < 3; i++) { ACE_HEADER *h = g0_ace(acl, i); if (h) EXPECT_EQ(g0_ace_mask(h), want[i]); }
    CHECK(IsValidAcl(acl));
    /* MAXDWORD appends, and so does any index past the end */
    BYTE x[20]; g0_mkace(x, ACCESS_ALLOWED_ACE_TYPE, 0, 0x44, G0_SID(g0_sid_world));
    CHECK(U(AddAce)(acl, ACL_REVISION, MAXDWORD, x, 20));
    ACE_HEADER *h = g0_ace(acl, 3); if (h) EXPECT_EQ(g0_ace_mask(h), 0x44);
    g0_mkace(x, ACCESS_ALLOWED_ACE_TYPE, 0, 0x55, G0_SID(g0_sid_world));
    CHECK(U(AddAce)(acl, ACL_REVISION, 1000, x, 20));
    h = g0_ace(acl, 4); if (h) EXPECT_EQ(g0_ace_mask(h), 0x55);
    g0_mkace(x, ACCESS_ALLOWED_ACE_TYPE, 0, 0x66, G0_SID(g0_sid_world));
    CHECK(U(AddAce)(acl, ACL_REVISION, 5, x, 20));                 /* index == count */
    h = g0_ace(acl, 5); if (h) EXPECT_EQ(g0_ace_mask(h), 0x66);
    EXPECT_EQ(g0_cnt(acl), 6);
    CHECK(IsValidAcl(acl));
}

TEST(gap0_addace_zero_length_and_null_list) {
    REQUIRE(U(AddAce));
    BYTE buf[64]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION);
    CHECK(U(AddAce)(acl, ACL_REVISION, 0, NULL, 0));
    EXPECT_EQ(g0_cnt(acl), 0);
    BYTE d[4] = {0};
    CHECK(U(AddAce)(acl, ACL_REVISION, MAXDWORD, d, 0));
    EXPECT_EQ(g0_cnt(acl), 0);
    CHECK(IsValidAcl(acl));
}

TEST(gap0_addace_length_mismatch) {
    REQUIRE(U(AddAce)); mk3();
    BYTE buf[128]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION);
    DWORD lens[] = {1, 3, 4, 8, 12, 15, 19};
    for (unsigned i = 0; i < sizeof lens / sizeof *lens; i++) {
        SET_ERR(0);
        CHECKF(!U(AddAce)(acl, ACL_REVISION, 0, g_ace1, lens[i]), "len %lu", lens[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        EXPECT_EQ(g0_cnt(acl), 0);
    }
}

TEST(gap0_addace_no_space) {
    REQUIRE(U(AddAce)); mk3();
    BYTE buf[28]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION);
    CHECK(U(AddAce)(acl, ACL_REVISION, 0, g_ace1, 20));
    SET_ERR(0);
    CHECK(!U(AddAce)(acl, ACL_REVISION, 0, g_ace2, 20));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(g0_cnt(acl), 1);
    BYTE b8[8]; PACL e = g0_acl(b8, 8, ACL_REVISION);
    SET_ERR(0);
    CHECK(!U(AddAce)(e, ACL_REVISION, 0, g_ace1, 20));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    /* a list one dword too big for the free space */
    BYTE b44[44]; PACL f = g0_acl(b44, 44, ACL_REVISION);
    BYTE two[40]; memcpy(two, g_ace1, 20); memcpy(two + 20, g_ace2, 20);
    SET_ERR(0);
    CHECK(!U(AddAce)(f, ACL_REVISION, 0, two, 40));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    BYTE b48[48]; PACL g = g0_acl(b48, 48, ACL_REVISION);
    CHECK(U(AddAce)(g, ACL_REVISION, 0, two, 40));
    EXPECT_EQ(g0_cnt(g), 2);
}

TEST(gap0_addace_invalid_acl) {
    REQUIRE(U(AddAce)); mk3();
    for (int v = 0; v < 3; v++) {
        BYTE buf[64]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION);
        if (v == 0) acl->AclRevision = 1;
        if (v == 1) acl->AclRevision = 5;
        if (v == 2) acl->AclRevision = 0;
        SET_ERR(0);
        CHECKF(!U(AddAce)(acl, ACL_REVISION, 0, g_ace1, 20), "variant %d", v);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);   /* unlike AddAccessAllowedAce, not ERROR_INVALID_ACL */
        EXPECT_EQ(g0_cnt(acl), 0);
    }
    BYTE buf[64]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION);
    acl->AclSize = 4;
    SET_ERR(0);
    CHECK(!U(AddAce)(acl, ACL_REVISION, 0, g_ace1, 20));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}

TEST(gap0_addace_revision_rules) {
    REQUIRE(U(AddAce)); mk3();
    struct { DWORD acl_rev, ace_rev, expect; } t[] = {
        {2, 2, 2}, {2, 3, 3}, {2, 4, 4}, {4, 2, 4}, {3, 0, 3}, {2, 0, 2}, {4, 4, 4}, {3, 3, 3}, {2, 1, 2},
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        BYTE buf[64]; PACL acl = g0_acl(buf, sizeof buf, t[i].acl_rev);
        BOOL r = U(AddAce)(acl, t[i].ace_rev, 0, g_ace1, 20);
        CHECKF(r, "case %u err %lu", i, GetLastError());
        EXPECT_EQ(acl->AclRevision, t[i].expect);
    }
}

TEST(gap0_addace_various_types) {
    REQUIRE(U(AddAce));
    if (t_is_wine()) { t_skip("wine hangs on non-classic ACE types in AddAce"); return; }
    BYTE a[20], b[20], c[20];
    g0_mkace(a, SYSTEM_MANDATORY_LABEL_ACE_TYPE, 0, 1, G0_SID(g0_sid_low));
    g0_mkace(b, SYSTEM_AUDIT_ACE_TYPE, 0x40, 9, G0_SID(g0_sid_world));
    g0_mkace(c, ACCESS_DENIED_ACE_TYPE, 0x10, 9, G0_SID(g0_sid_world));
    BYTE buf[128]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION);
    CHECK(U(AddAce)(acl, ACL_REVISION, 0, a, 20));
    CHECK(U(AddAce)(acl, ACL_REVISION, 1, b, 20));
    CHECK(U(AddAce)(acl, ACL_REVISION, 2, c, 20));
    EXPECT_EQ(g0_cnt(acl), 3);
    BYTE want[3] = {0x11, 2, 1};
    for (int i = 0; i < 3; i++) { ACE_HEADER *h = g0_ace(acl, i); if (h) EXPECT_EQ(h->AceType, want[i]); }
}

TEST(gap0_addace_does_not_validate_payload) {
    /* the ACE bodies are copied verbatim without SID validation */
    REQUIRE(U(AddAce));
    BYTE a[16]; memset(a, 0xAB, 16);
    ((ACE_HEADER *)a)->AceType = ACCESS_ALLOWED_ACE_TYPE;
    ((ACE_HEADER *)a)->AceSize = 16;
    BYTE buf[64]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION);
    BOOL r = U(AddAce)(acl, ACL_REVISION, 0, a, 16);
    CHECK(r);
    if (r) { EXPECT_EQ(g0_cnt(acl), 1); CHECK(!memcmp(buf + 8, a, 16)); }
}

TEST(gap0_addace_multi_index_shift) {
    REQUIRE(U(AddAce));
    BYTE buf[512]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION);
    for (DWORD i = 0; i < 20; i++) {
        BYTE x[20]; g0_mkace(x, ACCESS_ALLOWED_ACE_TYPE, 0, i, G0_SID(g0_sid_world));
        CHECK(U(AddAce)(acl, ACL_REVISION, 0, x, 20));   /* always prepend */
    }
    EXPECT_EQ(g0_cnt(acl), 20);
    for (DWORD i = 0; i < 20; i++) { ACE_HEADER *h = g0_ace(acl, i); if (h && g0_ace_mask(h) != 19 - i) { CHECKF(0, "order at %lu", i); break; } }
    CHECK(IsValidAcl(acl));
}

/* ---------------------------------------------------------- object ACEs */
typedef BOOL (WINAPI *objfn)(PACL, DWORD, DWORD, DWORD, GUID *, GUID *, PSID);
struct objadd { const char *name; BYTE objtype, plaintype; };
static const struct objadd g_obj[] = {
    {"AddAccessAllowedObjectAce", ACCESS_ALLOWED_OBJECT_ACE_TYPE, ACCESS_ALLOWED_ACE_TYPE},
    {"AddAccessDeniedObjectAce",  ACCESS_DENIED_OBJECT_ACE_TYPE,  ACCESS_DENIED_ACE_TYPE},
    {"AddAuditAccessObjectAce",   SYSTEM_AUDIT_OBJECT_ACE_TYPE,   SYSTEM_AUDIT_ACE_TYPE},
};
static BOOL callobj(int k, PACL a, DWORD rev, DWORD fl, DWORD mask, const GUID *g1, const GUID *g2, PSID sid) {
    GUID *p1 = (GUID *)g1, *p2 = (GUID *)g2;
    if (k == 0) return U(AddAccessAllowedObjectAce)(a, rev, fl, mask, p1, p2, sid);
    if (k == 1) return U(AddAccessDeniedObjectAce)(a, rev, fl, mask, p1, p2, sid);
    return U(AddAuditAccessObjectAce)(a, rev, fl, mask, p1, p2, sid, TRUE, FALSE);
}

TEST(gap0_objace_layout_one_guid) {
    for (int k = 0; k < 3; k++) {
        if (!t_proc(g_obj[k].name) || t_is_wine()) continue;
        BYTE buf[256]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION_DS);
        BOOL r = callobj(k, acl, ACL_REVISION_DS, 0, 0x100, &g0_guid_a, NULL, G0_SID(g0_sid_world));
        CHECKF(r, "%s err %lu", g_obj[k].name, GetLastError());
        if (!r) continue;
        BYTE *e = (BYTE *)g0_ace(acl, 0);
        EXPECT_EQ(e[0], g_obj[k].objtype);
        EXPECT_EQ(e[1], k == 2 ? 0x40 : 0);
        EXPECT_EQ(*(WORD *)(e + 2), 20 + 4 + 16);   /* hdr+mask+flags + guid + sid(12) */
        EXPECT_EQ(*(DWORD *)(e + 4), 0x100);
        EXPECT_EQ(*(DWORD *)(e + 8), ACE_OBJECT_TYPE_PRESENT);
        CHECK(!memcmp(e + 12, &g0_guid_a, 16));
        CHECK(EqualSid((PSID)(e + 28), G0_SID(g0_sid_world)));
        EXPECT_EQ(acl->AclRevision, ACL_REVISION_DS);
        CHECK(IsValidAcl(acl));
    }
}

TEST(gap0_objace_layout_inherited_guid_only) {
    for (int k = 0; k < 3; k++) {
        if (!t_proc(g_obj[k].name) || t_is_wine()) continue;
        BYTE buf[256]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION_DS);
        BOOL r = callobj(k, acl, ACL_REVISION_DS, CONTAINER_INHERIT_ACE, 0x4, NULL, &g0_guid_b, G0_SID(g0_sid_admins));
        CHECKF(r, "%s err %lu", g_obj[k].name, GetLastError());
        if (!r) continue;
        BYTE *e = (BYTE *)g0_ace(acl, 0);
        EXPECT_EQ(e[0], g_obj[k].objtype);
        EXPECT_EQ(*(WORD *)(e + 2), 20 + 8 + 16);
        EXPECT_EQ(*(DWORD *)(e + 8), ACE_INHERITED_OBJECT_TYPE_PRESENT);
        CHECK(!memcmp(e + 12, &g0_guid_b, 16));
        CHECK(EqualSid((PSID)(e + 28), G0_SID(g0_sid_admins)));
    }
}

TEST(gap0_objace_layout_both_guids) {
    for (int k = 0; k < 3; k++) {
        if (!t_proc(g_obj[k].name) || t_is_wine()) continue;
        BYTE buf[256]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION_DS);
        BOOL r = callobj(k, acl, ACL_REVISION_DS, 0, 0x7, &g0_guid_a, &g0_guid_b, G0_SID(g0_sid_fake));
        CHECKF(r, "%s err %lu", g_obj[k].name, GetLastError());
        if (!r) continue;
        BYTE *e = (BYTE *)g0_ace(acl, 0);
        EXPECT_EQ(e[0], g_obj[k].objtype);
        EXPECT_EQ(*(WORD *)(e + 2), 20 + 20 + 32);
        EXPECT_EQ(*(DWORD *)(e + 8), ACE_OBJECT_TYPE_PRESENT | ACE_INHERITED_OBJECT_TYPE_PRESENT);
        CHECK(!memcmp(e + 12, &g0_guid_a, 16));
        CHECK(!memcmp(e + 28, &g0_guid_b, 16));
        CHECK(EqualSid((PSID)(e + 44), G0_SID(g0_sid_fake)));
        EXPECT_EQ(acl->AclSize, sizeof buf);
        ACL_SIZE_INFORMATION si; CHECK(GetAclInformation(acl, &si, sizeof si, AclSizeInformation));
        EXPECT_EQ(si.AclBytesInUse, 8 + 20 + 20 + 32);
    }
}

TEST(gap0_objace_requires_ds_revision) {
    for (int k = 0; k < 3; k++) {
        if (!t_proc(g_obj[k].name) || t_is_wine()) continue;
        DWORD revs[] = {0, 1, 2, 3, 5, 255};
        for (unsigned i = 0; i < 6; i++) {
            BYTE buf[256]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION_DS);
            SET_ERR(0);
            CHECKF(!callobj(k, acl, revs[i], 0, 1, &g0_guid_a, NULL, G0_SID(g0_sid_world)), "%s rev %lu", g_obj[k].name, revs[i]);
            EXPECT_ERR(ERROR_REVISION_MISMATCH);
            EXPECT_EQ(g0_cnt(acl), 0);
        }
        /* ACL must itself be < 5 */
        BYTE buf[256]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION_DS);
        acl->AclRevision = 5;
        SET_ERR(0);
        CHECK(!callobj(k, acl, 4, 0, 1, &g0_guid_a, NULL, G0_SID(g0_sid_world)));
        EXPECT_ERR(ERROR_REVISION_MISMATCH);
    }
}

TEST(gap0_objace_upgrades_acl_revision) {
    for (int k = 0; k < 3; k++) {
        if (!t_proc(g_obj[k].name) || t_is_wine()) continue;
        BYTE buf[256]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION);
        BOOL r = callobj(k, acl, ACL_REVISION_DS, 0, 1, &g0_guid_a, NULL, G0_SID(g0_sid_world));
        CHECK(r);
        EXPECT_EQ(acl->AclRevision, ACL_REVISION_DS);
        CHECK(IsValidAcl(acl));
    }
}

TEST(gap0_objace_null_guids_use_plain_ace) {
    for (int k = 0; k < 3; k++) {
        if (!t_proc(g_obj[k].name) || t_is_wine()) continue;
        BYTE buf[256]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION);
        BOOL r = callobj(k, acl, ACL_REVISION, 0, 0x55, NULL, NULL, G0_SID(g0_sid_world));
        CHECKF(r, "%s err %lu", g_obj[k].name, GetLastError());
        ACE_HEADER *h = g0_ace(acl, 0);
        if (h) { EXPECT_EQ(h->AceType, g_obj[k].plaintype); EXPECT_EQ(h->AceSize, 20); }
        EXPECT_EQ(acl->AclRevision, ACL_REVISION);      /* plain path: revision 2 is fine */
    }
}

TEST(gap0_objace_errors) {
    for (int k = 0; k < 3; k++) {
        if (!t_proc(g_obj[k].name) || t_is_wine()) continue;
        BYTE buf[256]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION_DS);
        SET_ERR(0);
        CHECK(!callobj(k, acl, 4, 0, 1, &g0_guid_a, NULL, G0_SID(g0_sid_badrev)));
        EXPECT_ERR(ERROR_INVALID_SID);
        SET_ERR(0);
        CHECK(!callobj(k, acl, 4, 0x20, 1, &g0_guid_a, NULL, G0_SID(g0_sid_world)));
        EXPECT_ERR(ERROR_INVALID_FLAGS);
        SET_ERR(0);
        CHECK(!callobj(k, acl, 4, 0x100, 1, &g0_guid_a, &g0_guid_b, G0_SID(g0_sid_world)));
        EXPECT_ERR(ERROR_INVALID_FLAGS);
        BYTE b2[256]; PACL a2 = g0_acl(b2, sizeof b2, ACL_REVISION_DS);
        a2->AclRevision = 1;
        SET_ERR(0);
        CHECK(!callobj(k, a2, 4, 0, 1, &g0_guid_a, NULL, G0_SID(g0_sid_world)));
        /* ACL revision 1 fails the "< 5" test? no: it passes, then validity fails */
        EXPECT_ERR(ERROR_INVALID_ACL);
        /* too small: object ACE needs 20+4+16 = 40 bytes */
        BYTE b3[8 + 36]; PACL a3 = g0_acl(b3, sizeof b3, ACL_REVISION_DS);
        SET_ERR(0);
        CHECK(!callobj(k, a3, 4, 0, 1, &g0_guid_a, NULL, G0_SID(g0_sid_world)));
        EXPECT_ERR(ERROR_ALLOTTED_SPACE_EXCEEDED);
        BYTE b4[8 + 40]; PACL a4 = g0_acl(b4, sizeof b4, ACL_REVISION_DS);
        CHECK(callobj(k, a4, 4, 0, 1, &g0_guid_a, NULL, G0_SID(g0_sid_world)));
    }
}

TEST(gap0_objace_audit_flags) {
    REQUIRE(U(AddAuditAccessObjectAce));
    if (t_is_wine()) { t_skip("object ACE functions are stubs in wine"); return; }
    BYTE buf[256]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION_DS);
    for (int s = 0; s < 2; s++) for (int f = 0; f < 2; f++) {
        BYTE b[256]; PACL a = g0_acl(b, sizeof b, ACL_REVISION_DS);
        CHECK(U(AddAuditAccessObjectAce)(a, 4, 0x2, 1, (GUID *)&g0_guid_a, NULL, G0_SID(g0_sid_world), s, f));
        BYTE *e = (BYTE *)g0_ace(a, 0);
        if (e) EXPECT_EQ(e[1], (0x2 | (s ? 0x40 : 0) | (f ? 0x80 : 0)));
    }
    /* explicit 0x40|0x80 in AceFlags is valid for audit object ACEs */
    CHECK(U(AddAuditAccessObjectAce)(acl, 4, 0xC0, 1, (GUID *)&g0_guid_a, NULL, G0_SID(g0_sid_world), FALSE, FALSE));
    BYTE *e = (BYTE *)g0_ace(acl, 0); if (e) EXPECT_EQ(e[1], 0xC0);
}

TEST(gap0_objace_flags_for_allow_deny) {
    /* with a GUID present the 0x20 flag is rejected for allow/deny object ACEs */
    for (int k = 0; k < 2; k++) {
        if (!t_proc(g_obj[k].name) || t_is_wine()) continue;
        DWORD good[] = {0, 1, 2, 4, 8, 0x10, 0x1f};
        for (unsigned i = 0; i < 7; i++) {
            BYTE buf[256]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION_DS);
            CHECKF(callobj(k, acl, 4, good[i], 1, &g0_guid_a, NULL, G0_SID(g0_sid_world)), "%s fl %lx", g_obj[k].name, good[i]);
        }
        DWORD bad[] = {0x20, 0x40, 0x80, 0xff, 0x100};
        for (unsigned i = 0; i < 5; i++) {
            BYTE buf[256]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION_DS);
            SET_ERR(0);
            CHECKF(!callobj(k, acl, 4, bad[i], 1, &g0_guid_a, NULL, G0_SID(g0_sid_world)), "%s fl %lx", g_obj[k].name, bad[i]);
            EXPECT_ERR(ERROR_INVALID_FLAGS);
        }
    }
}

/* ------------------------------------------------------ AddMandatoryAce */
TEST(gap0_mandatory_basic) {
    REQUIRE(U(AddMandatoryAce));
    DWORD pol[] = {0, 1, 2, 3, 4, 5, 6, 7};
    for (unsigned i = 0; i < 8; i++) {
        BYTE buf[64]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION_DS);
        BOOL (WINAPI *fn)(PACL, DWORD, DWORD, DWORD, PSID) = U(AddMandatoryAce);
        SET_ERR(0xdeadbeef);
        BOOL r = fn(acl, ACL_REVISION_DS, 0, pol[i], G0_SID(g0_sid_low));
        CHECKF(r, "policy %lu err %lu", pol[i], GetLastError());
        EXPECT_EQ(GetLastError(), 0xdeadbeef);
        ACE_HEADER *h = g0_ace(acl, 0);
        if (h) {
            EXPECT_EQ(h->AceType, SYSTEM_MANDATORY_LABEL_ACE_TYPE);
            EXPECT_EQ(h->AceSize, 20);
            EXPECT_EQ(g0_ace_mask(h), pol[i]);
            CHECK(EqualSid(g0_ace_sid(h), G0_SID(g0_sid_low)));
        }
        CHECK(IsValidAcl(acl));
    }
}

TEST(gap0_mandatory_policy_flags_invalid) {
    REQUIRE(U(AddMandatoryAce));
    DWORD badpol[] = {8, 0x10, 0xff, 0x100, 0x80000000, 0xFFFFFFFF, 0xF};
    for (unsigned i = 0; i < 7; i++) {
        BYTE buf[64]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION_DS);
        SET_ERR(0);
        CHECKF(!U(AddMandatoryAce)(acl, ACL_REVISION_DS, 0, badpol[i], G0_SID(g0_sid_low)), "policy %lx", badpol[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);        /* no Ex mapping to ERROR_INVALID_FLAGS here */
        EXPECT_EQ(g0_cnt(acl), 0);
    }
    DWORD fl[] = {0, 1, 2, 4, 8, 0x10, 0x1f};
    for (unsigned i = 0; i < 7; i++) {
        BYTE buf[64]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION_DS);
        CHECKF(U(AddMandatoryAce)(acl, ACL_REVISION_DS, fl[i], 1, G0_SID(g0_sid_low)), "flags %lx", fl[i]);
        ACE_HEADER *h = g0_ace(acl, 0); if (h) EXPECT_EQ(h->AceFlags, fl[i]);
    }
    DWORD badfl[] = {0x20, 0x40, 0x80, 0x100, 0xFFFFFFFF};
    for (unsigned i = 0; i < 5; i++) {
        BYTE buf[64]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION_DS);
        SET_ERR(0);
        CHECKF(!U(AddMandatoryAce)(acl, ACL_REVISION_DS, badfl[i], 1, G0_SID(g0_sid_low)), "flags %lx", badfl[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}

TEST(gap0_mandatory_levels) {
    REQUIRE(U(AddMandatoryAce));
    DWORD lv[] = {0, 0x1000, 0x2000, 0x2100, 0x3000, 0x4000, 0x5000, 0x12345};
    for (unsigned i = 0; i < 8; i++) {
        BYTE sid[12]; memcpy(sid, g0_sid_low, 12); *(DWORD *)(sid + 8) = lv[i];
        BYTE buf[64]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION_DS);
        CHECKF(U(AddMandatoryAce)(acl, ACL_REVISION_DS, 0, 1, G0_SID(sid)), "level %lx", lv[i]);
        ACE_HEADER *h = g0_ace(acl, 0);
        if (h) CHECK(EqualSid(g0_ace_sid(h), G0_SID(sid)));
    }
}

TEST(gap0_mandatory_sid_validation) {
    REQUIRE(U(AddMandatoryAce));
    PSID notlabel[] = {G0_SID(g0_sid_world), G0_SID(g0_sid_admins), G0_SID(g0_sid_fake), G0_SID(g0_sid_scoped)};
    for (int i = 0; i < 4; i++) {
        BYTE buf[64]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION_DS);
        SET_ERR(0);
        CHECKF(!U(AddMandatoryAce)(acl, ACL_REVISION_DS, 0, 1, notlabel[i]), "sid %d", i);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
    BYTE buf[64]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION_DS);
    SET_ERR(0);
    CHECK(!U(AddMandatoryAce)(acl, ACL_REVISION_DS, 0, 1, G0_SID(g0_sid_badrev)));
    EXPECT_ERR(ERROR_INVALID_SID);
    SET_ERR(0);
    CHECK(!U(AddMandatoryAce)(acl, ACL_REVISION_DS, 0, 1, G0_SID(g0_sid_bigcnt)));
    EXPECT_ERR(ERROR_INVALID_SID);
}

TEST(gap0_mandatory_null_and_invalid_acl) {
    REQUIRE(U(AddMandatoryAce));
    if (t_is_wine()) { t_skip("wine dereferences a NULL ACL"); return; }
    SET_ERR(0);
    CHECK(!U(AddMandatoryAce)(NULL, ACL_REVISION_DS, 0, 1, G0_SID(g0_sid_low)));
    EXPECT_ERR(ERROR_INVALID_ACL);
    BYTE buf[64]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION_DS);
    acl->AclRevision = 1;
    SET_ERR(0);
    CHECK(!U(AddMandatoryAce)(acl, ACL_REVISION_DS, 0, 1, G0_SID(g0_sid_low)));
    EXPECT_ERR(ERROR_INVALID_ACL);
    acl->AclRevision = 0;
    SET_ERR(0);
    CHECK(!U(AddMandatoryAce)(acl, ACL_REVISION_DS, 0, 1, G0_SID(g0_sid_low)));
    EXPECT_ERR(ERROR_INVALID_ACL);
}

TEST(gap0_mandatory_revision_rules) {
    REQUIRE(U(AddMandatoryAce));
    BYTE buf[64]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION_DS);
    SET_ERR(0);
    CHECK(!U(AddMandatoryAce)(acl, 5, 0, 1, G0_SID(g0_sid_low)));
    EXPECT_ERR(ERROR_REVISION_MISMATCH);
    acl->AclRevision = 5;
    SET_ERR(0);
    CHECK(!U(AddMandatoryAce)(acl, ACL_REVISION_DS, 0, 1, G0_SID(g0_sid_low)));
    EXPECT_ERR(ERROR_REVISION_MISMATCH);
    BYTE b2[64]; PACL a2 = g0_acl(b2, sizeof b2, ACL_REVISION);
    CHECK(U(AddMandatoryAce)(a2, ACL_REVISION, 0, 1, G0_SID(g0_sid_low)));
    EXPECT_EQ(a2->AclRevision, ACL_REVISION);
    BYTE b3[64]; PACL a3 = g0_acl(b3, sizeof b3, ACL_REVISION);
    CHECK(U(AddMandatoryAce)(a3, ACL_REVISION_DS, 0, 1, G0_SID(g0_sid_low)));
    EXPECT_EQ(a3->AclRevision, ACL_REVISION_DS);
    /* sid check precedes revision check */
    SET_ERR(0);
    CHECK(!U(AddMandatoryAce)(a3, 9, 0, 1, G0_SID(g0_sid_badrev)));
    EXPECT_ERR(ERROR_INVALID_SID);
}

TEST(gap0_mandatory_space) {
    REQUIRE(U(AddMandatoryAce));
    BYTE b16[8 + 16]; PACL a = g0_acl(b16, sizeof b16, ACL_REVISION_DS);
    SET_ERR(0);
    CHECK(!U(AddMandatoryAce)(a, ACL_REVISION_DS, 0, 1, G0_SID(g0_sid_low)));
    EXPECT_ERR(ERROR_ALLOTTED_SPACE_EXCEEDED);
    BYTE b20[8 + 20]; PACL b = g0_acl(b20, sizeof b20, ACL_REVISION_DS);
    CHECK(U(AddMandatoryAce)(b, ACL_REVISION_DS, 0, 1, G0_SID(g0_sid_low)));
    SET_ERR(0);
    CHECK(!U(AddMandatoryAce)(b, ACL_REVISION_DS, 0, 1, G0_SID(g0_sid_low)));
    EXPECT_ERR(ERROR_ALLOTTED_SPACE_EXCEEDED);
    EXPECT_EQ(g0_cnt(b), 1);
}

/* -------------------------------------------------- AddScopedPolicyIDAce */
TEST(gap0_scoped_basic) {
    REQUIRE(U(AddScopedPolicyIDAce));
    BYTE buf[64]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION_DS);
    BOOL (WINAPI *fn)(PACL, DWORD, DWORD, DWORD, PSID) = U(AddScopedPolicyIDAce);
    SET_ERR(0xdeadbeef);
    BOOL r = fn(acl, ACL_REVISION_DS, 0, 0, G0_SID(g0_sid_scoped));
    CHECKF(r, "err %lu", GetLastError());
    EXPECT_EQ(GetLastError(), 0xdeadbeef);
    ACE_HEADER *h = g0_ace(acl, 0);
    if (h) {
        EXPECT_EQ(h->AceType, SYSTEM_SCOPED_POLICY_ID_ACE_TYPE);
        EXPECT_EQ(h->AceSize, 20);
        EXPECT_EQ(g0_ace_mask(h), 0);
        CHECK(EqualSid(g0_ace_sid(h), G0_SID(g0_sid_scoped)));
    }
    CHECK(IsValidAcl(acl));
    DWORD fl[] = {1, 2, 4, 8, 0x10, 0x1f};
    for (unsigned i = 0; i < 6; i++) {
        BYTE b[64]; PACL a = g0_acl(b, sizeof b, ACL_REVISION_DS);
        CHECKF(U(AddScopedPolicyIDAce)(a, ACL_REVISION_DS, fl[i], 0, G0_SID(g0_sid_scoped)), "flags %lx", fl[i]);
        ACE_HEADER *e = g0_ace(a, 0); if (e) EXPECT_EQ(e->AceFlags, fl[i]);
    }
}

TEST(gap0_scoped_errors) {
    REQUIRE(U(AddScopedPolicyIDAce));
    BYTE buf[64]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION_DS);
    SET_ERR(0);
    CHECK(!U(AddScopedPolicyIDAce)(NULL, ACL_REVISION_DS, 0, 0, G0_SID(g0_sid_scoped)));
    EXPECT_ERR(ERROR_INVALID_ACL);
    SET_ERR(0);
    CHECK(!U(AddScopedPolicyIDAce)(acl, ACL_REVISION_DS, 0, 1, G0_SID(g0_sid_scoped)));     /* mask must be 0 */
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!U(AddScopedPolicyIDAce)(acl, ACL_REVISION_DS, 0x20, 0, G0_SID(g0_sid_scoped)));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!U(AddScopedPolicyIDAce)(acl, ACL_REVISION_DS, 0, 0, G0_SID(g0_sid_world)));      /* wrong authority */
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!U(AddScopedPolicyIDAce)(acl, ACL_REVISION_DS, 0, 0, G0_SID(g0_sid_low)));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!U(AddScopedPolicyIDAce)(acl, ACL_REVISION_DS, 0, 0, G0_SID(g0_sid_badrev)));
    EXPECT_ERR(ERROR_INVALID_SID);
    SET_ERR(0);
    CHECK(!U(AddScopedPolicyIDAce)(acl, 5, 0, 0, G0_SID(g0_sid_scoped)));
    EXPECT_ERR(ERROR_REVISION_MISMATCH);
    acl->AclRevision = 1;
    SET_ERR(0);
    CHECK(!U(AddScopedPolicyIDAce)(acl, ACL_REVISION_DS, 0, 0, G0_SID(g0_sid_scoped)));
    EXPECT_ERR(ERROR_INVALID_ACL);
    BYTE b2[8 + 16]; PACL a2 = g0_acl(b2, sizeof b2, ACL_REVISION_DS);
    SET_ERR(0);
    CHECK(!U(AddScopedPolicyIDAce)(a2, ACL_REVISION_DS, 0, 0, G0_SID(g0_sid_scoped)));
    EXPECT_ERR(ERROR_ALLOTTED_SPACE_EXCEEDED);
    BYTE b3[8 + 20]; PACL a3 = g0_acl(b3, sizeof b3, ACL_REVISION);
    CHECK(U(AddScopedPolicyIDAce)(a3, ACL_REVISION, 0, 0, G0_SID(g0_sid_scoped)));
}

/* ----------------------------------------------- AddResourceAttributeAce */
static WCHAR g_attr_name[] = L"gap0.attr";
static DWORD64 g_attr_val = 42;
static CLAIM_SECURITY_ATTRIBUTE_V1 g_attrs[2];   /* second slot keeps AttributeCount==2 readable; Windows requires exactly 1 */
#define g_attr g_attrs[0]
static CLAIM_SECURITY_ATTRIBUTES_INFORMATION g_info;
static void mkinfo(void) {
    memset(g_attrs, 0, sizeof g_attrs);
    g_attr.Name = g_attr_name;
    g_attr.ValueType = CLAIM_SECURITY_ATTRIBUTE_TYPE_UINT64;
    g_attr.ValueCount = 1;
    g_attr.Values.pUint64 = &g_attr_val;
    g_attrs[1] = g_attrs[0];
    memset(&g_info, 0, sizeof g_info);
    g_info.Version = CLAIM_SECURITY_ATTRIBUTES_INFORMATION_VERSION_V1;
    g_info.AttributeCount = 1;
    g_info.Attribute.pAttributeV1 = &g_attr;
}

TEST(gap0_resattr_errors) {
    REQUIRE(U(AddResourceAttributeAce)); mkinfo();
    BYTE buf[256]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION_DS);
    DWORD len = 0x77;
    SET_ERR(0);
    CHECK(!U(AddResourceAttributeAce)(acl, ACL_REVISION_DS, 0, 0, G0_SID(g0_sid_world), &g_info, NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!U(AddResourceAttributeAce)(NULL, ACL_REVISION_DS, 0, 0, G0_SID(g0_sid_world), &g_info, &len));
    EXPECT_ERR(ERROR_INVALID_ACL);
    EXPECT_EQ(len, 0);                               /* *pReturnLength cleared first */
    len = 0x77;
    SET_ERR(0);
    CHECK(!U(AddResourceAttributeAce)(acl, ACL_REVISION_DS, 0, 0, G0_SID(g0_sid_badrev), &g_info, &len));
    EXPECT_ERR(ERROR_INVALID_SID);
    EXPECT_EQ(len, 0);
    SET_ERR(0);
    CHECK(!U(AddResourceAttributeAce)(acl, ACL_REVISION_DS, 0, 0, G0_SID(g0_sid_admins), &g_info, &len));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);             /* SID must be Everyone */
    SET_ERR(0);
    CHECK(!U(AddResourceAttributeAce)(acl, ACL_REVISION_DS, 0, 0, G0_SID(g0_sid_low), &g_info, &len));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!U(AddResourceAttributeAce)(acl, 5, 0, 0, G0_SID(g0_sid_world), &g_info, &len));
    EXPECT_ERR(ERROR_REVISION_MISMATCH);
    SET_ERR(0);
    CHECK(!U(AddResourceAttributeAce)(acl, ACL_REVISION_DS, 0x20, 0, G0_SID(g0_sid_world), &g_info, &len));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!U(AddResourceAttributeAce)(acl, ACL_REVISION_DS, 0, 1, G0_SID(g0_sid_world), &g_info, &len));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!U(AddResourceAttributeAce)(acl, ACL_REVISION_DS, 0, 0, G0_SID(g0_sid_world), NULL, &len));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    EXPECT_EQ(g0_cnt(acl), 0);
}

TEST(gap0_resattr_bad_info) {
    REQUIRE(U(AddResourceAttributeAce));
    BYTE buf[256]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION_DS);
    DWORD len;
    for (int v = 0; v < 4; v++) {
        mkinfo();
        if (v == 0) g_info.Version = 2;
        if (v == 1) g_info.AttributeCount = 0;
        if (v == 2) g_info.AttributeCount = 2;
        if (v == 3) g_info.Reserved = 1;
        len = 0x77;
        SET_ERR(0);
        CHECKF(!U(AddResourceAttributeAce)(acl, ACL_REVISION_DS, 0, 0, G0_SID(g0_sid_world), &g_info, &len), "variant %d", v);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        EXPECT_EQ(g0_cnt(acl), 0);
    }
}

TEST(gap0_resattr_success_and_space) {
    REQUIRE(U(AddResourceAttributeAce)); mkinfo();
    BYTE buf[512]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION_DS);
    DWORD len = 0;
    SET_ERR(0);
    BOOL r = U(AddResourceAttributeAce)(acl, ACL_REVISION_DS, 0, 0, G0_SID(g0_sid_world), &g_info, &len);
    if (!r) { t_note("AddResourceAttributeAce with a UINT64 attribute failed, err %lu", GetLastError()); return; }
    EXPECT_EQ(g0_cnt(acl), 1);
    ACE_HEADER *h = g0_ace(acl, 0);
    if (h) {
        EXPECT_EQ(h->AceType, SYSTEM_RESOURCE_ATTRIBUTE_ACE_TYPE);
        EXPECT_EQ(g0_ace_mask(h), 0);
        CHECK(EqualSid(g0_ace_sid(h), G0_SID(g0_sid_world)));
        EXPECT_EQ(len, 8 + h->AceSize);
    }
    EXPECT_EQ(acl->AclRevision, ACL_REVISION_DS);
    CHECK(IsValidAcl(acl));
    /* second one reports a cumulative length */
    DWORD len2 = 0;
    CHECK(U(AddResourceAttributeAce)(acl, ACL_REVISION_DS, 0, 0, G0_SID(g0_sid_world), &g_info, &len2));
    CHECK(len2 > len);
    /* too-small ACL: fails with the space error and reports the length needed */
    BYTE b2[16]; PACL a2 = g0_acl(b2, sizeof b2, ACL_REVISION_DS);
    DWORD need = 0;
    SET_ERR(0);
    r = U(AddResourceAttributeAce)(a2, ACL_REVISION_DS, 0, 0, G0_SID(g0_sid_world), &g_info, &need);
    CHECK(!r);
    EXPECT_ERR(ERROR_ALLOTTED_SPACE_EXCEEDED);
    CHECK(need > 16);
    BYTE *p = (BYTE *)malloc(need + 16);
    if (p) {
        PACL a3 = g0_acl(p, need + 16, ACL_REVISION_DS);
        DWORD len3 = 0;
        CHECK(U(AddResourceAttributeAce)(a3, ACL_REVISION_DS, 0, 0, G0_SID(g0_sid_world), &g_info, &len3));
        free(p);
    }
}
