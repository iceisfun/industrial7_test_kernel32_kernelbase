/* AddAccessAllowedAce/Ex, AddAccessDeniedAce/Ex, AddAuditAccessAce/Ex and the
 * object-less (NULL GUID) path of the *ObjectAce variants. */
#include "g0.h"

static BOOL w_allow_ex(PACL a, DWORD r, DWORD f, DWORD m, PSID s) { return U(AddAccessAllowedAceEx)(a, r, f, m, s); }
static BOOL w_allow(PACL a, DWORD r, DWORD f, DWORD m, PSID s)    { return U(AddAccessAllowedAce)(a, r, m, s); }
static BOOL w_deny_ex(PACL a, DWORD r, DWORD f, DWORD m, PSID s)  { return U(AddAccessDeniedAceEx)(a, r, f, m, s); }
static BOOL w_deny(PACL a, DWORD r, DWORD f, DWORD m, PSID s)     { return U(AddAccessDeniedAce)(a, r, m, s); }
static BOOL w_audit_ex(PACL a, DWORD r, DWORD f, DWORD m, PSID s) { return U(AddAuditAccessAceEx)(a, r, f, m, s, TRUE, TRUE); }
static BOOL w_audit_s(PACL a, DWORD r, DWORD f, DWORD m, PSID s)  { return U(AddAuditAccessAce)(a, r, m, s, TRUE, FALSE); }
static BOOL w_audit_f(PACL a, DWORD r, DWORD f, DWORD m, PSID s)  { return U(AddAuditAccessAce)(a, r, m, s, FALSE, TRUE); }
static BOOL w_allow_o0(PACL a, DWORD r, DWORD f, DWORD m, PSID s) { return U(AddAccessAllowedObjectAce)(a, r, f, m, NULL, NULL, s); }
static BOOL w_deny_o0(PACL a, DWORD r, DWORD f, DWORD m, PSID s)  { return U(AddAccessDeniedObjectAce)(a, r, f, m, NULL, NULL, s); }
static BOOL w_audit_o0(PACL a, DWORD r, DWORD f, DWORD m, PSID s) { return U(AddAuditAccessObjectAce)(a, r, f, m, NULL, NULL, s, TRUE, TRUE); }

struct adder {
    const char *name; g0_adder_fn fn;
    BYTE type;          /* ACE type produced */
    DWORD extra;        /* flag bits OR'ed in by the API itself */
    int takes_flags;    /* AceFlags parameter present */
    DWORD okmask;       /* AceFlags bits that are accepted */
    int ex_errmap;      /* STATUS_INVALID_PARAMETER -> ERROR_INVALID_FLAGS */
};
static const struct adder g_add[] = {
    {"AddAccessAllowedAceEx",      w_allow_ex, ACCESS_ALLOWED_ACE_TYPE, 0,    1, 0x3f, 1},
    {"AddAccessAllowedAce",        w_allow,    ACCESS_ALLOWED_ACE_TYPE, 0,    0, 0,    0},
    {"AddAccessDeniedAceEx",       w_deny_ex,  ACCESS_DENIED_ACE_TYPE,  0,    1, 0x1f, 1},
    {"AddAccessDeniedAce",         w_deny,     ACCESS_DENIED_ACE_TYPE,  0,    0, 0,    0},
    {"AddAuditAccessAceEx",        w_audit_ex, SYSTEM_AUDIT_ACE_TYPE,   0xC0, 1, 0xdf, 1},
    {"AddAuditAccessAce",          w_audit_s,  SYSTEM_AUDIT_ACE_TYPE,   0x40, 0, 0,    0},
    {"AddAuditAccessAce",          w_audit_f,  SYSTEM_AUDIT_ACE_TYPE,   0x80, 0, 0,    0},
    {"AddAccessAllowedObjectAce",  w_allow_o0, ACCESS_ALLOWED_ACE_TYPE, 0,    1, 0x3f, 1},
    {"AddAccessDeniedObjectAce",   w_deny_o0,  ACCESS_DENIED_ACE_TYPE,  0,    1, 0x1f, 1},
    {"AddAuditAccessObjectAce",    w_audit_o0, SYSTEM_AUDIT_ACE_TYPE,   0xC0, 1, 0xdf, 1},
};
#define NADD ((int)(sizeof g_add / sizeof g_add[0]))

TEST(gap0_known_success_layout) {
    for (int i = 0; i < NADD; i++) {
        const struct adder *a = &g_add[i];
        if (!t_proc(a->name) || (t_is_wine() && i >= 7)) { t_note("skipping %s", a->name); continue; }
        BYTE buf[256]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION);
        CHECK(acl != NULL);
        DWORD fl = a->takes_flags ? 0x3 : 0;
        BOOL r = a->fn(acl, ACL_REVISION, fl, 0x00120089, G0_SID(g0_sid_world));
        CHECKF(r, "%s idx %d err %lu", a->name, i, GetLastError());
        EXPECT_EQ(g0_cnt(acl), 1);
        ACE_HEADER *h = g0_ace(acl, 0);
        if (!h) continue;
        EXPECT_EQ(h->AceType, a->type);
        EXPECT_EQ(h->AceFlags, (fl | a->extra));
        EXPECT_EQ(h->AceSize, 8 + 12);
        EXPECT_EQ(g0_ace_mask(h), 0x00120089);
        CHECK(EqualSid(g0_ace_sid(h), G0_SID(g0_sid_world)));
        EXPECT_EQ(acl->AclRevision, ACL_REVISION);
        CHECK(IsValidAcl(acl));
        /* second ACE with a longer SID is appended after the first */
        r = a->fn(acl, ACL_REVISION, fl, 0x1, G0_SID(g0_sid_admins));
        CHECK(r);
        EXPECT_EQ(g0_cnt(acl), 2);
        h = g0_ace(acl, 1);
        if (h) {
            EXPECT_EQ(h->AceSize, 8 + 16);
            EXPECT_EQ(g0_ace_mask(h), 1);
            CHECK(EqualSid(g0_ace_sid(h), G0_SID(g0_sid_admins)));
            EXPECT_EQ((BYTE *)h, (BYTE *)acl + 8 + 20);
        }
        ACL_SIZE_INFORMATION si;
        CHECK(GetAclInformation(acl, &si, sizeof si, AclSizeInformation));
        EXPECT_EQ(si.AceCount, 2);
        EXPECT_EQ(si.AclBytesInUse, 8 + 20 + 24);
        EXPECT_EQ(si.AclBytesFree, sizeof buf - (8 + 20 + 24));
        EXPECT_EQ(acl->AclSize, sizeof buf);
    }
}

TEST(gap0_known_mask_passthrough) {
    DWORD masks[] = {0, 1, 0x80000000, 0xFFFFFFFF, GENERIC_ALL, MAXIMUM_ALLOWED, 0x12345678, ACCESS_SYSTEM_SECURITY};
    for (int i = 0; i < NADD; i++) {
        if (!t_proc(g_add[i].name) || (t_is_wine() && i >= 7)) continue;
        for (unsigned m = 0; m < sizeof masks / sizeof *masks; m++) {
            BYTE buf[64]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION);
            BOOL r = g_add[i].fn(acl, ACL_REVISION, 0, masks[m], G0_SID(g0_sid_world));
            CHECKF(r, "%s mask %lx", g_add[i].name, masks[m]);
            ACE_HEADER *h = g0_ace(acl, 0);
            if (h) EXPECT_EQ(g0_ace_mask(h), masks[m]);
        }
    }
}

TEST(gap0_known_sid_sizes) {
    for (int i = 0; i < NADD; i++) {
        if (!t_proc(g_add[i].name) || (t_is_wine() && i >= 7)) continue;
        for (int n = 0; n <= 15; n++) {
            BYTE sid[8 + 15 * 4] = {1, (BYTE)n, 0, 0, 0, 0, 0, 5};
            for (int k = 0; k < n; k++) *(DWORD *)(sid + 8 + 4 * k) = 100 + k;
            BYTE buf[256]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION);
            BOOL r = g_add[i].fn(acl, ACL_REVISION, 0, 7, G0_SID(sid));
            CHECKF(r, "%s n=%d err %lu", g_add[i].name, n, GetLastError());
            ACE_HEADER *h = g0_ace(acl, 0);
            if (r && h) {
                EXPECT_EQ(h->AceSize, (n + 4) * 4);
                CHECK(!memcmp(g0_ace_sid(h), sid, 8 + 4 * n));
            }
        }
    }
}

TEST(gap0_known_invalid_sid) {
    PSID bad[] = {G0_SID(g0_sid_badrev), G0_SID(g0_sid_bigcnt)};
    for (int i = 0; i < NADD; i++) {
        if (!t_proc(g_add[i].name) || (t_is_wine() && i >= 7)) continue;
        for (int s = 0; s < 2; s++) {
            BYTE buf[128]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION);
            SET_ERR(0);
            CHECKF(!g_add[i].fn(acl, ACL_REVISION, 0, 7, bad[s]), "%s sid %d", g_add[i].name, s);
            EXPECT_ERR(ERROR_INVALID_SID);
            EXPECT_EQ(g0_cnt(acl), 0);
        }
        /* SID validity is checked before ACL validity and revision */
        BYTE buf[128]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION);
        acl->AclRevision = 1;
        SET_ERR(0);
        CHECK(!g_add[i].fn(acl, 9, 0, 7, G0_SID(g0_sid_badrev)));
        EXPECT_ERR(ERROR_INVALID_SID);
    }
}

TEST(gap0_known_revision_matrix) {
    for (int i = 0; i < NADD; i++) {
        if (!t_proc(g_add[i].name) || (t_is_wine() && i >= 7)) continue;
        DWORD bad[] = {5, 6, 7, 100, 255};
        for (unsigned k = 0; k < 5; k++) {
            BYTE buf[128]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION);
            SET_ERR(0);
            CHECKF(!g_add[i].fn(acl, bad[k], 0, 7, G0_SID(g0_sid_world)), "%s rev %lu", g_add[i].name, bad[k]);
            EXPECT_ERR(ERROR_REVISION_MISMATCH);
            EXPECT_EQ(g0_cnt(acl), 0);
        }
        /* revisions 0..4 are all accepted; the ACL takes the larger of the two */
        for (DWORD rv = 0; rv <= 4; rv++) {
            BYTE buf[128]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION);
            BOOL r = g_add[i].fn(acl, rv, 0, 7, G0_SID(g0_sid_world));
            CHECKF(r, "%s rev %lu err %lu", g_add[i].name, rv, GetLastError());
            EXPECT_EQ(acl->AclRevision, rv > 2 ? rv : 2);
        }
        /* an ACL whose own revision byte is >= 5 is also a revision mismatch */
        BYTE buf[128]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION);
        acl->AclRevision = 5;
        SET_ERR(0);
        CHECK(!g_add[i].fn(acl, ACL_REVISION, 0, 7, G0_SID(g0_sid_world)));
        EXPECT_ERR(ERROR_REVISION_MISMATCH);
    }
}

TEST(gap0_known_revision_ds_acl_kept) {
    for (int i = 0; i < NADD; i++) {
        if (!t_proc(g_add[i].name) || (t_is_wine() && i >= 7)) continue;
        BYTE buf[128]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION_DS);
        CHECK(g_add[i].fn(acl, ACL_REVISION, 0, 7, G0_SID(g0_sid_world)));
        EXPECT_EQ(acl->AclRevision, ACL_REVISION_DS);
    }
}

TEST(gap0_known_invalid_acl) {
    for (int i = 0; i < NADD; i++) {
        if (!t_proc(g_add[i].name) || (t_is_wine() && i >= 7)) continue;
        BYTE buf[128]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION);
        acl->AclRevision = 1;
        SET_ERR(0);
        CHECKF(!g_add[i].fn(acl, ACL_REVISION, 0, 7, G0_SID(g0_sid_world)), "%s", g_add[i].name);
        EXPECT_ERR(ERROR_INVALID_ACL);
        acl->AclRevision = 0;
        SET_ERR(0);
        CHECK(!g_add[i].fn(acl, ACL_REVISION, 0, 7, G0_SID(g0_sid_world)));
        EXPECT_ERR(ERROR_INVALID_ACL);
        acl->AclRevision = 2; acl->AclSize = 4;
        SET_ERR(0);
        CHECK(!g_add[i].fn(acl, ACL_REVISION, 0, 7, G0_SID(g0_sid_world)));
        EXPECT_ERR(ERROR_INVALID_ACL);
    }
}

TEST(gap0_known_acl_full) {
    for (int i = 0; i < NADD; i++) {
        if (!t_proc(g_add[i].name) || (t_is_wine() && i >= 7)) continue;
        BYTE buf[8 + 20 * 3]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION);
        for (int k = 0; k < 3; k++) {
            CHECKF(g_add[i].fn(acl, ACL_REVISION, 0, 1u << k, G0_SID(g0_sid_world)), "%s add %d", g_add[i].name, k);
        }
        EXPECT_EQ(g0_cnt(acl), 3);
        SET_ERR(0);
        CHECKF(!g_add[i].fn(acl, ACL_REVISION, 0, 8, G0_SID(g0_sid_world)), "%s", g_add[i].name);
        EXPECT_ERR(ERROR_ALLOTTED_SPACE_EXCEEDED);
        EXPECT_EQ(g0_cnt(acl), 3);
        /* header-only ACL */
        BYTE b8[8]; PACL e = g0_acl(b8, 8, ACL_REVISION);
        SET_ERR(0);
        CHECK(!g_add[i].fn(e, ACL_REVISION, 0, 1, G0_SID(g0_sid_world)));
        EXPECT_ERR(ERROR_ALLOTTED_SPACE_EXCEEDED);
        /* one dword short of a 20-byte ACE */
        BYTE b20[24]; PACL s = g0_acl(b20, 24, ACL_REVISION);
        SET_ERR(0);
        CHECK(!g_add[i].fn(s, ACL_REVISION, 0, 1, G0_SID(g0_sid_world)));
        EXPECT_ERR(ERROR_ALLOTTED_SPACE_EXCEEDED);
        /* exactly fitting */
        BYTE b24[28]; PACL x = g0_acl(b24, 28, ACL_REVISION);
        CHECK(g_add[i].fn(x, ACL_REVISION, 0, 1, G0_SID(g0_sid_world)));
        CHECK(IsValidAcl(x));
    }
}

TEST(gap0_known_fill_until_full) {
    for (int i = 0; i < NADD; i += 3) {
        if (!t_proc(g_add[i].name) || (t_is_wine() && i >= 7)) continue;
        BYTE buf[1024]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION);
        int n = 0;
        while (g_add[i].fn(acl, ACL_REVISION, 0, n + 1, G0_SID(g0_sid_world)) && n < 200) n++;
        EXPECT_EQ(n, (1024 - 8) / 20);
        EXPECT_EQ(g0_cnt(acl), n);
        EXPECT_ERR(ERROR_ALLOTTED_SPACE_EXCEEDED);
        CHECK(IsValidAcl(acl));
        for (int k = 0; k < n; k++) {
            ACE_HEADER *h = g0_ace(acl, k);
            if (!h) { CHECK(0); break; }
            if (g0_ace_mask(h) != (DWORD)(k + 1)) { CHECKF(0, "ace %d mask", k); break; }
        }
    }
}

/* ntdll!RtlpAddKnownAce: allowed rejects flags & ~0x3f, denied & ~0x1f, audit & ~0xdf (STATUS_INVALID_PARAMETER -> 1004 in kernelbase *Ex wrappers) */
TEST(gap0_known_flag_matrix) {
    DWORD flags[] = {0, 1, 2, 3, 4, 8, 0x10, 0x1f, 0x20, 0x21, 0x3f, 0x40, 0x80, 0xc0, 0xdf, 0xe0, 0x100, 0x200, 0xff, 0x1000, 0x80000000, 0xFFFFFFFF};
    for (int i = 0; i < NADD; i++) {
        const struct adder *a = &g_add[i];
        if (!a->takes_flags || !t_proc(a->name) || (t_is_wine() && i >= 7)) continue;
        for (unsigned f = 0; f < sizeof flags / sizeof *flags; f++) {
            BYTE buf[64]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION);
            int ok = (flags[f] & ~a->okmask) == 0;
            SET_ERR(0);
            BOOL r = a->fn(acl, ACL_REVISION, flags[f], 1, G0_SID(g0_sid_world));
            CHECKF(!!r == ok, "%s flags 0x%lx want %d got %d", a->name, flags[f], ok, r);
            if (!ok) {
                EXPECT_ERR(ERROR_INVALID_FLAGS);
                EXPECT_EQ(g0_cnt(acl), 0);
            } else {
                ACE_HEADER *h = g0_ace(acl, 0);
                if (h) EXPECT_EQ(h->AceFlags, (flags[f] | a->extra));
            }
        }
    }
}

TEST(gap0_known_flags_stored_verbatim) {
    /* every valid single flag round-trips unchanged */
    DWORD single[] = {OBJECT_INHERIT_ACE, CONTAINER_INHERIT_ACE, NO_PROPAGATE_INHERIT_ACE, INHERIT_ONLY_ACE, INHERITED_ACE};
    for (int i = 0; i < NADD; i++) {
        const struct adder *a = &g_add[i];
        if (!a->takes_flags || !t_proc(a->name) || (t_is_wine() && i >= 7)) continue;
        for (int f = 0; f < 5; f++) {
            BYTE buf[64]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION);
            CHECK(a->fn(acl, ACL_REVISION, single[f], 1, G0_SID(g0_sid_world)));
            ACE_HEADER *h = g0_ace(acl, 0);
            if (h) EXPECT_EQ(h->AceFlags, (single[f] | a->extra));
        }
    }
}

TEST(gap0_known_error_priority) {
    /* SID check, then revision, then flags, then ACL validity, then space */
    BYTE buf[8]; PACL acl = g0_acl(buf, 8, ACL_REVISION);
    SET_ERR(0);
    CHECK(!w_allow_ex(acl, 9, 0x100, 7, G0_SID(g0_sid_world)));
    EXPECT_ERR(ERROR_REVISION_MISMATCH);
    SET_ERR(0);
    CHECK(!w_allow_ex(acl, 2, 0x100, 7, G0_SID(g0_sid_world)));
    EXPECT_ERR(ERROR_INVALID_FLAGS);
    acl->AclRevision = 1;
    SET_ERR(0);
    CHECK(!w_allow_ex(acl, 2, 0x100, 7, G0_SID(g0_sid_world)));
    EXPECT_ERR(ERROR_INVALID_FLAGS);
    SET_ERR(0);
    CHECK(!w_allow_ex(acl, 2, 0, 7, G0_SID(g0_sid_world)));
    EXPECT_ERR(ERROR_INVALID_ACL);
    acl->AclRevision = 2;
    SET_ERR(0);
    CHECK(!w_allow_ex(acl, 2, 0, 7, G0_SID(g0_sid_world)));
    EXPECT_ERR(ERROR_ALLOTTED_SPACE_EXCEEDED);
}

TEST(gap0_known_audit_success_failure_bits) {
    REQUIRE(U(AddAuditAccessAceEx)); REQUIRE(U(AddAuditAccessAce));
    for (int s = 0; s < 2; s++) for (int f = 0; f < 2; f++) {
        BYTE buf[64]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION);
        BOOL r = U(AddAuditAccessAceEx)(acl, ACL_REVISION, CONTAINER_INHERIT_ACE, 5, G0_SID(g0_sid_world), s, f);
        CHECKF(r, "s=%d f=%d err %lu", s, f, GetLastError());
        ACE_HEADER *h = g0_ace(acl, 0);
        if (h) EXPECT_EQ(h->AceFlags, (CONTAINER_INHERIT_ACE | (s ? SUCCESSFUL_ACCESS_ACE_FLAG : 0) | (f ? FAILED_ACCESS_ACE_FLAG : 0)));
        BYTE b2[64]; PACL a2 = g0_acl(b2, sizeof b2, ACL_REVISION);
        r = U(AddAuditAccessAce)(a2, ACL_REVISION, 5, G0_SID(g0_sid_world), s, f);
        CHECKF(r, "plain s=%d f=%d err %lu", s, f, GetLastError());
        h = g0_ace(a2, 0);
        if (h) EXPECT_EQ(h->AceFlags, ((s ? SUCCESSFUL_ACCESS_ACE_FLAG : 0) | (f ? FAILED_ACCESS_ACE_FLAG : 0)));
    }
    /* Win10 RtlAddAuditAccessAceEx takes the BOOLs as 'char' and tests only the low byte:
       0x100 (low byte 0) is FALSE, 0x7fffffff (low byte 0xff) is TRUE */
    BYTE buf[64]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION);
    CHECK(U(AddAuditAccessAceEx)(acl, ACL_REVISION, 0, 5, G0_SID(g0_sid_world), 0x100, 0x7fffffff));
    ACE_HEADER *h = g0_ace(acl, 0);
    if (h) EXPECT_EQ(h->AceFlags, 0x80);
    BYTE buf2[64]; PACL acl2 = g0_acl(buf2, sizeof buf2, ACL_REVISION);
    CHECK(U(AddAuditAccessAceEx)(acl2, ACL_REVISION, 0, 5, G0_SID(g0_sid_world), 0x101, 0x7fffffff));
    h = g0_ace(acl2, 0);
    if (h) EXPECT_EQ(h->AceFlags, 0xC0);
}

TEST(gap0_known_audit_explicit_flag_bits) {
    REQUIRE(U(AddAuditAccessAceEx));
    BYTE buf[64]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION);
    /* success/failure bits supplied in AceFlags are accepted even with both BOOLs FALSE */
    CHECK(U(AddAuditAccessAceEx)(acl, ACL_REVISION, 0xC0, 5, G0_SID(g0_sid_world), FALSE, FALSE));
    ACE_HEADER *h = g0_ace(acl, 0);
    if (h) EXPECT_EQ(h->AceFlags, 0xC0);
    /* 0x20 is not valid for audit ACEs */
    SET_ERR(0);
    CHECK(!U(AddAuditAccessAceEx)(acl, ACL_REVISION, 0x20, 5, G0_SID(g0_sid_world), TRUE, TRUE));
    EXPECT_ERR(ERROR_INVALID_FLAGS);
}

TEST(gap0_known_mixed_order_and_types) {
    REQUIRE(U(AddAccessAllowedAce)); REQUIRE(U(AddAccessDeniedAce)); REQUIRE(U(AddAuditAccessAce));
    BYTE buf[256]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION);
    CHECK(U(AddAccessDeniedAce)(acl, ACL_REVISION, 1, G0_SID(g0_sid_fake)));
    CHECK(U(AddAccessAllowedAce)(acl, ACL_REVISION, 2, G0_SID(g0_sid_world)));
    CHECK(U(AddAuditAccessAce)(acl, ACL_REVISION, 4, G0_SID(g0_sid_admins), TRUE, TRUE));
    EXPECT_EQ(g0_cnt(acl), 3);
    BYTE want[3] = {ACCESS_DENIED_ACE_TYPE, ACCESS_ALLOWED_ACE_TYPE, SYSTEM_AUDIT_ACE_TYPE};
    for (int i = 0; i < 3; i++) { ACE_HEADER *h = g0_ace(acl, i); if (h) { EXPECT_EQ(h->AceType, want[i]); EXPECT_EQ(g0_ace_mask(h), 1u << i); } }
    ACL_REVISION_INFORMATION ri;
    CHECK(GetAclInformation(acl, &ri, sizeof ri, AclRevisionInformation));
    EXPECT_EQ(ri.AclRevision, ACL_REVISION);
}

TEST(gap0_known_does_not_touch_following_memory) {
    for (int i = 0; i < NADD; i++) {
        if (!t_proc(g_add[i].name) || (t_is_wine() && i >= 7)) continue;
        BYTE buf[96]; memset(buf, 0x5A, sizeof buf);
        PACL acl = (PACL)buf; InitializeAcl(acl, 40, ACL_REVISION);
        memset(buf + 8, 0x5A, 32);
        CHECK(g_add[i].fn(acl, ACL_REVISION, 0, 7, G0_SID(g0_sid_world)));
        for (int k = 40; k < 96; k++) if (buf[k] != 0x5A) { CHECKF(0, "%s overran at %d", g_add[i].name, k); break; }
        EXPECT_EQ(acl->AclSize, 40);
    }
}

TEST(gap0_known_success_keeps_last_error) {
    /* pointers are resolved first: a failed lookup in kernel32 would itself set the last error */
    BOOL (WINAPI *f1)(PACL, DWORD, DWORD, DWORD, PSID) = U(AddAccessAllowedAceEx);
    BOOL (WINAPI *f2)(PACL, DWORD, DWORD, PSID) = U(AddAccessDeniedAce);
    BOOL (WINAPI *f3)(PACL, DWORD, DWORD, PSID, BOOL, BOOL) = U(AddAuditAccessAce);
    REQUIRE(f1); REQUIRE(f2); REQUIRE(f3);
    BYTE buf[128]; PACL acl = g0_acl(buf, sizeof buf, ACL_REVISION);
    SET_ERR(0xdeadbeef); CHECK(f1(acl, ACL_REVISION, 0, 1, G0_SID(g0_sid_world))); EXPECT_EQ(GetLastError(), 0xdeadbeef);
    SET_ERR(0xdeadbeef); CHECK(f2(acl, ACL_REVISION, 2, G0_SID(g0_sid_world))); EXPECT_EQ(GetLastError(), 0xdeadbeef);
    SET_ERR(0xdeadbeef); CHECK(f3(acl, ACL_REVISION, 4, G0_SID(g0_sid_world), TRUE, TRUE)); EXPECT_EQ(GetLastError(), 0xdeadbeef);
}
