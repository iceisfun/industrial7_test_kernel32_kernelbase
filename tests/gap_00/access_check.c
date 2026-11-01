/* AccessCheck, AccessCheckByType, AccessCheckByTypeResultList and the *AndAuditAlarm family */
#include "g0.h"

static GENERIC_MAPPING g_gm = {0x1, 0x2, 0x4, 0x7};

struct sdctx {
    SECURITY_DESCRIPTOR sd;
    BYTE acl[512] __attribute__((aligned(8)));
};

/* mode: 0 = DACL present with ACEs (allow world mask), 1 = empty DACL, 2 = NULL DACL, 3 = no DACL at all */
static int sd_build(struct sdctx *c, int mode, PSID owner, PSID group, DWORD worldmask) {
    memset(c, 0, sizeof *c);
    if (!InitializeSecurityDescriptor(&c->sd, SECURITY_DESCRIPTOR_REVISION)) return 0;
    if (owner && !SetSecurityDescriptorOwner(&c->sd, owner, FALSE)) return 0;
    if (group && !SetSecurityDescriptorGroup(&c->sd, group, FALSE)) return 0;
    if (mode == 3) return 1;
    PACL acl = NULL;
    if (mode == 0 || mode == 1) {
        acl = g0_acl(c->acl, sizeof c->acl, ACL_REVISION);
        if (mode == 0 && !U(AddAccessAllowedAce)(acl, ACL_REVISION, worldmask, G0_SID(g0_sid_world))) return 0;
    }
    return SetSecurityDescriptorDacl(&c->sd, TRUE, acl, FALSE);
}

static HANDLE imp_token(SECURITY_IMPERSONATION_LEVEL lvl, DWORD access) {
    HANDLE p = NULL, d = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | TOKEN_QUERY, &p)) return NULL;
    if (!DuplicateTokenEx(p, access, NULL, lvl, TokenImpersonation, &d)) d = NULL;
    CloseHandle(p);
    return d;
}
static HANDLE prim_token(void) {
    HANDLE p = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &p)) return NULL;
    return p;
}
static int priv_enabled(HANDLE tok, const char *name) {
    LUID l; if (!LookupPrivilegeValueA(NULL, name, &l)) return 0;
    DWORD n = 0; GetTokenInformation(tok, TokenPrivileges, NULL, 0, &n);
    TOKEN_PRIVILEGES *tp = (TOKEN_PRIVILEGES *)malloc(n ? n : 1);
    int r = 0;
    if (tp && GetTokenInformation(tok, TokenPrivileges, tp, n, &n)) {
        for (DWORD i = 0; i < tp->PrivilegeCount; i++)
            if (tp->Privileges[i].Luid.LowPart == l.LowPart && tp->Privileges[i].Luid.HighPart == l.HighPart)
                r = (tp->Privileges[i].Attributes & SE_PRIVILEGE_ENABLED) != 0;
    }
    free(tp);
    return r;
}
static PSID user_sid(HANDLE tok, BYTE *buf, DWORD cap) {
    DWORD n = 0;
    if (!GetTokenInformation(tok, TokenUser, buf, cap, &n)) return NULL;
    return ((TOKEN_USER *)buf)->User.Sid;
}

typedef BOOL (WINAPI *ac_fn)(PSECURITY_DESCRIPTOR, HANDLE, DWORD, PGENERIC_MAPPING, PPRIVILEGE_SET, LPDWORD, LPDWORD, LPBOOL);

#define AC_CALL(sd, tok, want, gm, granted, status) \
    U(AccessCheck)((sd), (tok), (want), (gm), &ps.p, &pslen, &(granted), &(status))
#define PSDECL() union { PRIVILEGE_SET p; BYTE b[256]; } ps; DWORD pslen = sizeof ps; memset(&ps, 0, sizeof ps)

TEST(gap0_ac_grant_deny_basic) {
    REQUIRE(U(AccessCheck)); REQUIRE(U(AddAccessAllowedAce));
    HANDLE tok = imp_token(SecurityImpersonation, TOKEN_ALL_ACCESS); CHECK(tok != NULL); if (!tok) return;
    struct sdctx c; CHECK(sd_build(&c, 0, G0_SID(g0_sid_fake), G0_SID(g0_sid_fake), 0x3));
    struct { DWORD want; int ok; DWORD granted; } t[] = {
        {0x1, 1, 0x1}, {0x2, 1, 0x2}, {0x3, 1, 0x3}, {0x4, 0, 0}, {0x5, 0, 0}, {0x7, 0, 0},
        {0x8, 0, 0}, {DELETE, 0, 0}, {0x00010000 | 1, 0, 0}, {0x100, 0, 0}, {0x80, 0, 0},
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        PSDECL(); DWORD granted = 0xdead; BOOL st = 0x55;
        SET_ERR(0);
        BOOL r = AC_CALL(&c.sd, tok, t[i].want, &g_gm, granted, st);
        CHECKF(r, "want %lx err %lu", t[i].want, GetLastError());
        CHECKF(!!st == t[i].ok, "want %lx status %d", t[i].want, st);
        EXPECT_EQ(granted, t[i].granted);
        if (!t[i].ok) EXPECT_ERR(ERROR_ACCESS_DENIED);
    }
    CloseHandle(tok);
}

TEST(gap0_ac_maximum_allowed) {
    REQUIRE(U(AccessCheck)); REQUIRE(U(AddAccessAllowedAce));
    HANDLE tok = imp_token(SecurityImpersonation, TOKEN_ALL_ACCESS); if (!tok) { CHECK(0); return; }
    struct sdctx c; CHECK(sd_build(&c, 0, G0_SID(g0_sid_fake), G0_SID(g0_sid_fake), 0x3));
    PSDECL(); DWORD granted = 0; BOOL st = 0;
    BOOL r = AC_CALL(&c.sd, tok, MAXIMUM_ALLOWED, &g_gm, granted, st);
    CHECK(r); CHECK(st);
    CHECK((granted & 3) == 3);
    EXPECT_EQ(granted & ~0x3u & ~(DWORD)(READ_CONTROL | WRITE_DAC | WRITE_OWNER | ACCESS_SYSTEM_SECURITY), 0);
    if (granted != 3) t_note("MAXIMUM_ALLOWED granted 0x%lx (extra rights from token state)", granted);
    /* empty DACL: nothing for MAXIMUM_ALLOWED */
    CHECK(sd_build(&c, 1, G0_SID(g0_sid_fake), G0_SID(g0_sid_fake), 0));
    granted = 0xdead; st = 1;
    SET_ERR(0);
    r = AC_CALL(&c.sd, tok, MAXIMUM_ALLOWED, &g_gm, granted, st);
    CHECK(r); CHECK(!st);
    EXPECT_EQ(granted, 0);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(tok);
}

TEST(gap0_ac_dacl_modes) {
    REQUIRE(U(AccessCheck)); REQUIRE(U(AddAccessAllowedAce));
    HANDLE tok = imp_token(SecurityImpersonation, TOKEN_ALL_ACCESS); if (!tok) { CHECK(0); return; }
    struct sdctx c;
    struct { int mode; int ok; } t[] = {{0, 0}, {1, 0}, {2, 1}, {3, 1}};
    for (int i = 0; i < 4; i++) {
        CHECK(sd_build(&c, t[i].mode, G0_SID(g0_sid_fake), G0_SID(g0_sid_fake), 0x2));
        PSDECL(); DWORD granted = 0; BOOL st = 0;
        BOOL r = AC_CALL(&c.sd, tok, 0x1, &g_gm, granted, st);   /* world has only 0x2 in mode 0 */
        CHECKF(r, "mode %d err %lu", t[i].mode, GetLastError());
        CHECKF(!!st == t[i].ok, "mode %d status %d", t[i].mode, st);
        EXPECT_EQ(granted, t[i].ok ? 1 : 0);
    }
    CloseHandle(tok);
}

TEST(gap0_ac_ace_ordering_and_flags) {
    REQUIRE(U(AccessCheck)); REQUIRE(U(AddAccessAllowedAceEx)); REQUIRE(U(AddAccessDeniedAceEx));
    HANDLE tok = imp_token(SecurityImpersonation, TOKEN_ALL_ACCESS); if (!tok) { CHECK(0); return; }
    struct sdctx c;
    memset(&c, 0, sizeof c);
    InitializeSecurityDescriptor(&c.sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorOwner(&c.sd, G0_SID(g0_sid_fake), FALSE);
    SetSecurityDescriptorGroup(&c.sd, G0_SID(g0_sid_fake), FALSE);
    PACL acl = g0_acl(c.acl, sizeof c.acl, ACL_REVISION);
    /* deny 1 first, then allow 3 */
    CHECK(U(AddAccessDeniedAceEx)(acl, ACL_REVISION, 0, 0x1, G0_SID(g0_sid_world)));
    CHECK(U(AddAccessAllowedAceEx)(acl, ACL_REVISION, 0, 0x3, G0_SID(g0_sid_world)));
    SetSecurityDescriptorDacl(&c.sd, TRUE, acl, FALSE);
    PSDECL(); DWORD granted; BOOL st;
    granted = 0; st = 1; CHECK(AC_CALL(&c.sd, tok, 0x1, &g_gm, granted, st)); CHECK(!st); EXPECT_EQ(granted, 0);
    granted = 0; st = 0; CHECK(AC_CALL(&c.sd, tok, 0x2, &g_gm, granted, st)); CHECK(st); EXPECT_EQ(granted, 2);
    granted = 0; st = 1; CHECK(AC_CALL(&c.sd, tok, 0x3, &g_gm, granted, st)); CHECK(!st);
    /* allow 3 first then deny 1: everything wanted was already granted */
    acl = g0_acl(c.acl, sizeof c.acl, ACL_REVISION);
    CHECK(U(AddAccessAllowedAceEx)(acl, ACL_REVISION, 0, 0x3, G0_SID(g0_sid_world)));
    CHECK(U(AddAccessDeniedAceEx)(acl, ACL_REVISION, 0, 0x1, G0_SID(g0_sid_world)));
    SetSecurityDescriptorDacl(&c.sd, TRUE, acl, FALSE);
    granted = 0; st = 0; CHECK(AC_CALL(&c.sd, tok, 0x3, &g_gm, granted, st)); CHECK(st); EXPECT_EQ(granted, 3);
    /* inherit-only ACEs do not apply to the object itself */
    acl = g0_acl(c.acl, sizeof c.acl, ACL_REVISION);
    CHECK(U(AddAccessAllowedAceEx)(acl, ACL_REVISION, INHERIT_ONLY_ACE | OBJECT_INHERIT_ACE, 0x3, G0_SID(g0_sid_world)));
    SetSecurityDescriptorDacl(&c.sd, TRUE, acl, FALSE);
    granted = 0; st = 1; CHECK(AC_CALL(&c.sd, tok, 0x1, &g_gm, granted, st)); CHECK(!st); EXPECT_EQ(granted, 0);
    /* an ACE for a SID the token does not hold is ignored (allow and deny) */
    acl = g0_acl(c.acl, sizeof c.acl, ACL_REVISION);
    CHECK(U(AddAccessDeniedAceEx)(acl, ACL_REVISION, 0, 0x3, G0_SID(g0_sid_fake)));
    CHECK(U(AddAccessAllowedAceEx)(acl, ACL_REVISION, 0, 0x3, G0_SID(g0_sid_world)));
    SetSecurityDescriptorDacl(&c.sd, TRUE, acl, FALSE);
    granted = 0; st = 0; CHECK(AC_CALL(&c.sd, tok, 0x3, &g_gm, granted, st)); CHECK(st); EXPECT_EQ(granted, 3);
    CloseHandle(tok);
}

TEST(gap0_ac_token_user_ace) {
    REQUIRE(U(AccessCheck)); REQUIRE(U(AddAccessAllowedAce));
    HANDLE tok = imp_token(SecurityImpersonation, TOKEN_ALL_ACCESS); if (!tok) { CHECK(0); return; }
    BYTE ub[256]; PSID me = user_sid(tok, ub, sizeof ub); CHECK(me != NULL); if (!me) return;
    struct sdctx c; memset(&c, 0, sizeof c);
    InitializeSecurityDescriptor(&c.sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorOwner(&c.sd, G0_SID(g0_sid_fake), FALSE);
    SetSecurityDescriptorGroup(&c.sd, G0_SID(g0_sid_fake), FALSE);
    PACL acl = g0_acl(c.acl, sizeof c.acl, ACL_REVISION);
    CHECK(U(AddAccessAllowedAce)(acl, ACL_REVISION, 0x4, me));
    SetSecurityDescriptorDacl(&c.sd, TRUE, acl, FALSE);
    PSDECL(); DWORD granted = 0; BOOL st = 0;
    CHECK(AC_CALL(&c.sd, tok, 0x4, &g_gm, granted, st)); CHECK(st); EXPECT_EQ(granted, 4);
    granted = 0; st = 1;
    CHECK(AC_CALL(&c.sd, tok, 0x1, &g_gm, granted, st)); CHECK(!st);
    CloseHandle(tok);
}

TEST(gap0_ac_owner_implicit_rights) {
    REQUIRE(U(AccessCheck));
    HANDLE tok = imp_token(SecurityImpersonation, TOKEN_ALL_ACCESS); if (!tok) { CHECK(0); return; }
    BYTE ub[256]; PSID me = user_sid(tok, ub, sizeof ub); if (!me) { CHECK(0); return; }
    struct sdctx c;
    PSDECL(); DWORD granted = 0; BOOL st = 0;
    /* owner == token user: READ_CONTROL and WRITE_DAC are implicit even with an empty DACL */
    CHECK(sd_build(&c, 1, me, me, 0));
    CHECK(AC_CALL(&c.sd, tok, READ_CONTROL | WRITE_DAC, &g_gm, granted, st));
    CHECK(st); EXPECT_EQ(granted, READ_CONTROL | WRITE_DAC);
    granted = 0; st = 1;
    CHECK(AC_CALL(&c.sd, tok, DELETE, &g_gm, granted, st));
    CHECK(!st);
    /* owner is somebody else: denied */
    CHECK(sd_build(&c, 1, G0_SID(g0_sid_fake), G0_SID(g0_sid_fake), 0));
    granted = 0; st = 1;
    SET_ERR(0);
    CHECK(AC_CALL(&c.sd, tok, READ_CONTROL, &g_gm, granted, st));
    CHECK(!st); EXPECT_EQ(granted, 0);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(tok);
}

TEST(gap0_ac_generic_not_mapped) {
    REQUIRE(U(AccessCheck)); REQUIRE(U(AddAccessAllowedAce));
    HANDLE tok = imp_token(SecurityImpersonation, TOKEN_ALL_ACCESS); if (!tok) { CHECK(0); return; }
    struct sdctx c; CHECK(sd_build(&c, 0, G0_SID(g0_sid_fake), G0_SID(g0_sid_fake), 0x7));
    DWORD gens[] = {GENERIC_READ, GENERIC_WRITE, GENERIC_EXECUTE, GENERIC_ALL, GENERIC_READ | 1};
    for (unsigned i = 0; i < 5; i++) {
        PSDECL(); DWORD granted = 0xdead; BOOL st = 0x55;
        SET_ERR(0);
        BOOL r = AC_CALL(&c.sd, tok, gens[i], &g_gm, granted, st);
        CHECKF(!r, "want %lx", gens[i]);
        EXPECT_ERR(ERROR_GENERIC_NOT_MAPPED);
    }
    /* once mapped, it works */
    DWORD want = GENERIC_READ; MapGenericMask(&want, &g_gm);
    EXPECT_EQ(want, 1);
    PSDECL(); DWORD granted = 0; BOOL st = 0;
    CHECK(AC_CALL(&c.sd, tok, want, &g_gm, granted, st)); CHECK(st); EXPECT_EQ(granted, 1);
    /* a mapping that is itself consistent for all-generic */
    GENERIC_MAPPING gm2 = {1, 2, 4, 7};
    want = GENERIC_ALL; MapGenericMask(&want, &gm2);
    EXPECT_EQ(want, 7);
    CloseHandle(tok);
}

TEST(gap0_ac_token_errors) {
    REQUIRE(U(AccessCheck)); REQUIRE(U(AddAccessAllowedAce));
    struct sdctx c; CHECK(sd_build(&c, 0, G0_SID(g0_sid_fake), G0_SID(g0_sid_fake), 0x3));
    PSDECL(); DWORD granted = 0xdead; BOOL st = 0x55;
    /* primary token */
    HANDLE pt = prim_token(); CHECK(pt != NULL);
    if (pt) {
        SET_ERR(0);
        CHECK(!AC_CALL(&c.sd, pt, 1, &g_gm, granted, st));
        EXPECT_ERR(ERROR_NO_IMPERSONATION_TOKEN);
        CloseHandle(pt);
    }
    /* anonymous-level impersonation token */
    HANDLE an = imp_token(SecurityAnonymous, TOKEN_ALL_ACCESS);
    if (an) {
        SET_ERR(0);
        CHECK(!AC_CALL(&c.sd, an, 1, &g_gm, granted, st));
        EXPECT_ERR(ERROR_BAD_IMPERSONATION_LEVEL);
        CloseHandle(an);
    } else t_note("could not create an anonymous token");
    HANDLE idt = imp_token(SecurityIdentification, TOKEN_ALL_ACCESS);
    if (idt) {
        st = 0;
        CHECK(AC_CALL(&c.sd, idt, 1, &g_gm, granted, st));
        CHECK(st);
        CloseHandle(idt);
    }
    /* handles that are not tokens */
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE bad[] = {NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x7777, ev, GetCurrentProcess(), GetCurrentThread()};
    for (unsigned i = 0; i < 6; i++) {
        SET_ERR(0);
        CHECKF(!AC_CALL(&c.sd, bad[i], 1, &g_gm, granted, st), "handle %u", i);
        EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
    /* closed token */
    HANDLE ct = imp_token(SecurityImpersonation, TOKEN_ALL_ACCESS);
    if (ct) {
        CloseHandle(ct);
        SET_ERR(0);
        CHECK(!AC_CALL(&c.sd, ct, 1, &g_gm, granted, st));
        EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
    CloseHandle(ev);
}

TEST(gap0_ac_token_access_rights) {
    REQUIRE(U(AccessCheck)); REQUIRE(U(AddAccessAllowedAce));
    struct sdctx c; CHECK(sd_build(&c, 0, G0_SID(g0_sid_fake), G0_SID(g0_sid_fake), 0x3));
    HANDLE t = imp_token(SecurityImpersonation, TOKEN_IMPERSONATE);
    if (!t) { t_note("could not create limited token"); return; }
    PSDECL(); DWORD granted = 0xdead; BOOL st = 0x55;
    SET_ERR(0);
    BOOL r = AC_CALL(&c.sd, t, 1, &g_gm, granted, st);
    CHECK(!r);
    EXPECT_ERR(ERROR_ACCESS_DENIED);   /* TOKEN_QUERY is required */
    CloseHandle(t);
    t = imp_token(SecurityImpersonation, TOKEN_QUERY);
    if (t) { st = 0; CHECK(AC_CALL(&c.sd, t, 1, &g_gm, granted, st)); CHECK(st); CloseHandle(t); }
}

TEST(gap0_ac_sd_errors) {
    REQUIRE(U(AccessCheck)); REQUIRE(U(AddAccessAllowedAce));
    HANDLE tok = imp_token(SecurityImpersonation, TOKEN_ALL_ACCESS); if (!tok) { CHECK(0); return; }
    struct sdctx c;
    PSDECL(); DWORD granted = 0xdead; BOOL st = 0x55;
    CHECK(sd_build(&c, 0, NULL, G0_SID(g0_sid_fake), 0x3));       /* no owner */
    SET_ERR(0);
    CHECK(!AC_CALL(&c.sd, tok, 1, &g_gm, granted, st));
    EXPECT_ERR(ERROR_INVALID_SECURITY_DESCR);
    CHECK(sd_build(&c, 0, G0_SID(g0_sid_fake), NULL, 0x3));       /* no group */
    SET_ERR(0);
    CHECK(!AC_CALL(&c.sd, tok, 1, &g_gm, granted, st));
    EXPECT_ERR(ERROR_INVALID_SECURITY_DESCR);
    CHECK(sd_build(&c, 0, NULL, NULL, 0x3));
    SET_ERR(0);
    CHECK(!AC_CALL(&c.sd, tok, 1, &g_gm, granted, st));
    EXPECT_ERR(ERROR_INVALID_SECURITY_DESCR);
    CHECK(sd_build(&c, 0, G0_SID(g0_sid_fake), G0_SID(g0_sid_fake), 0x3));
    c.sd.Revision = 7;
    SET_ERR(0);
    CHECK(!AC_CALL(&c.sd, tok, 1, &g_gm, granted, st));
    DWORD e = GetLastError();
    CHECKF(e == ERROR_UNKNOWN_REVISION || e == ERROR_INVALID_SECURITY_DESCR || e == ERROR_INVALID_PARAMETER, "err %lu", e);
    t_note("bad SD revision -> %lu", e);
    c.sd.Revision = 1;
    CHECK(sd_build(&c, 0, G0_SID(g0_sid_badrev), G0_SID(g0_sid_fake), 0x3));
    SET_ERR(0);
    CHECK(!AC_CALL(&c.sd, tok, 1, &g_gm, granted, st));
    t_note("invalid owner SID -> %lu", GetLastError());
    CloseHandle(tok);
}

TEST(gap0_ac_null_params) {
    REQUIRE(U(AccessCheck)); REQUIRE(U(AddAccessAllowedAce));
    HANDLE tok = imp_token(SecurityImpersonation, TOKEN_ALL_ACCESS); if (!tok) { CHECK(0); return; }
    PSDECL(); DWORD granted = 0xdead; BOOL st = 0x55;
    SET_ERR(0);
    BOOL r = U(AccessCheck)(NULL, tok, 1, &g_gm, &ps.p, &pslen, &granted, &st);
    CHECK(!r);
    t_note("NULL SD -> %lu", GetLastError());
    t_note("NULL SD: granted 0x%lx status 0x%x", granted, (unsigned)st);   /* kernel may or may not touch outputs on failure */
    CloseHandle(tok);
}

TEST(gap0_ac_system_security_needs_privilege) {
    REQUIRE(U(AccessCheck)); REQUIRE(U(AddAccessAllowedAce));
    HANDLE tok = imp_token(SecurityImpersonation, TOKEN_ALL_ACCESS); if (!tok) { CHECK(0); return; }
    if (priv_enabled(tok, "SeSecurityPrivilege")) { t_skip("SeSecurityPrivilege enabled"); CloseHandle(tok); return; }
    struct sdctx c; CHECK(sd_build(&c, 2, G0_SID(g0_sid_fake), G0_SID(g0_sid_fake), 0));
    PSDECL(); DWORD granted = 0xdead; BOOL st = 0x55;
    SET_ERR(0);
    BOOL r = AC_CALL(&c.sd, tok, ACCESS_SYSTEM_SECURITY, &g_gm, granted, st);
    CHECK(r);      /* Win10 AccessCheck succeeds; the privilege failure is reported via AccessStatus + last error */
    CHECK(!st);
    EXPECT_ERR(ERROR_PRIVILEGE_NOT_HELD);
    CloseHandle(tok);
}

TEST(gap0_ac_privilege_set_untouched_without_privs) {
    REQUIRE(U(AccessCheck)); REQUIRE(U(AddAccessAllowedAce));
    HANDLE tok = imp_token(SecurityImpersonation, TOKEN_ALL_ACCESS); if (!tok) { CHECK(0); return; }
    struct sdctx c; CHECK(sd_build(&c, 0, G0_SID(g0_sid_fake), G0_SID(g0_sid_fake), 0x3));
    PSDECL(); ps.p.PrivilegeCount = 99; DWORD granted = 0; BOOL st = 0;
    DWORD len = sizeof ps;
    BOOL r = U(AccessCheck)(&c.sd, tok, 1, &g_gm, &ps.p, &len, &granted, &st);
    CHECK(r); CHECK(st);
    t_note("PrivilegeCount after = %lu, length = %lu", ps.p.PrivilegeCount, len);
    EXPECT_EQ(ps.p.PrivilegeCount, 0);
    CloseHandle(tok);
}

TEST(gap0_ac_repeated_calls_stable) {
    REQUIRE(U(AccessCheck)); REQUIRE(U(AddAccessAllowedAce));
    HANDLE tok = imp_token(SecurityImpersonation, TOKEN_ALL_ACCESS); if (!tok) { CHECK(0); return; }
    struct sdctx c; CHECK(sd_build(&c, 0, G0_SID(g0_sid_fake), G0_SID(g0_sid_fake), 0x3));
    for (int i = 0; i < 500; i++) {
        PSDECL(); DWORD granted = 0; BOOL st = 0;
        if (!AC_CALL(&c.sd, tok, 0x2, &g_gm, granted, st) || !st || granted != 2) { CHECKF(0, "iteration %d", i); break; }
    }
    CHECK(1);
    CloseHandle(tok);
}

TEST(gap0_ac_self_relative_sd) {
    REQUIRE(U(AccessCheck)); REQUIRE(U(AddAccessAllowedAce));
    HANDLE tok = imp_token(SecurityImpersonation, TOKEN_ALL_ACCESS); if (!tok) { CHECK(0); return; }
    struct sdctx c; CHECK(sd_build(&c, 0, G0_SID(g0_sid_fake), G0_SID(g0_sid_fake), 0x3));
    BYTE rel[1024]; DWORD n = sizeof rel;
    CHECK(MakeSelfRelativeSD(&c.sd, rel, &n));
    PSDECL(); DWORD granted = 0; BOOL st = 0;
    CHECK(AC_CALL(rel, tok, 0x3, &g_gm, granted, st)); CHECK(st); EXPECT_EQ(granted, 3);
    granted = 0; st = 1;
    SET_ERR(0);
    CHECK(AC_CALL(rel, tok, 0x4, &g_gm, granted, st)); CHECK(!st); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(tok);
}

/* --------------------------------------------------------- ByType */
static GUID g_root = {0x10000000,1,1,{1,1,1,1,1,1,1,1}};
static GUID g_ch1  = {0x20000000,2,2,{2,2,2,2,2,2,2,2}};
static GUID g_ch2  = {0x30000000,3,3,{3,3,3,3,3,3,3,3}};
static GUID g_gch  = {0x40000000,4,4,{4,4,4,4,4,4,4,4}};

#define BT_CALL(sd, self, tok, want, otl, n, granted, status) \
    U(AccessCheckByType)((sd), (self), (tok), (want), (otl), (n), &g_gm, &ps.p, &pslen, &(granted), &(status))

TEST(gap0_acbt_without_type_list_matches_accesscheck) {
    REQUIRE(U(AccessCheckByType)); REQUIRE(U(AddAccessAllowedAce));
    if (t_is_wine()) { t_skip("AccessCheckByType is not implemented by wine"); return; }
    HANDLE tok = imp_token(SecurityImpersonation, TOKEN_ALL_ACCESS); if (!tok) { CHECK(0); return; }
    struct sdctx c; CHECK(sd_build(&c, 0, G0_SID(g0_sid_fake), G0_SID(g0_sid_fake), 0x3));
    DWORD wants[] = {1, 2, 3, 4, 5, 0};
    for (int i = 0; i < 5; i++) {
        PSDECL(); DWORD granted = 0xdead; BOOL st = 0x55;
        SET_ERR(0);
        BOOL r = BT_CALL(&c.sd, NULL, tok, wants[i], NULL, 0, granted, st);
        CHECKF(r, "want %lx err %lu", wants[i], GetLastError());
        int ok = (wants[i] & ~3u) == 0;
        CHECKF(!!st == ok, "want %lx status %d", wants[i], st);
        EXPECT_EQ(granted, ok ? wants[i] : 0);
        if (!ok) EXPECT_ERR(ERROR_ACCESS_DENIED);
    }
    CloseHandle(tok);
}

TEST(gap0_acbt_token_and_generic_errors) {
    REQUIRE(U(AccessCheckByType)); REQUIRE(U(AddAccessAllowedAce));
    if (t_is_wine()) { t_skip("AccessCheckByType is not implemented by wine"); return; }
    struct sdctx c; CHECK(sd_build(&c, 0, G0_SID(g0_sid_fake), G0_SID(g0_sid_fake), 0x3));
    PSDECL(); DWORD granted = 0xdead; BOOL st = 0x55;
    HANDLE pt = prim_token();
    if (pt) { SET_ERR(0); CHECK(!BT_CALL(&c.sd, NULL, pt, 1, NULL, 0, granted, st)); EXPECT_ERR(ERROR_NO_IMPERSONATION_TOKEN); CloseHandle(pt); }
    HANDLE an = imp_token(SecurityAnonymous, TOKEN_ALL_ACCESS);
    if (an) { SET_ERR(0); CHECK(!BT_CALL(&c.sd, NULL, an, 1, NULL, 0, granted, st)); EXPECT_ERR(ERROR_BAD_IMPERSONATION_LEVEL); CloseHandle(an); }
    HANDLE bad[] = {NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x7777};
    for (int i = 0; i < 3; i++) { SET_ERR(0); CHECK(!BT_CALL(&c.sd, NULL, bad[i], 1, NULL, 0, granted, st)); EXPECT_ERR(ERROR_INVALID_HANDLE); }
    HANDLE tok = imp_token(SecurityImpersonation, TOKEN_ALL_ACCESS);
    if (tok) {
        SET_ERR(0);
        CHECK(!BT_CALL(&c.sd, NULL, tok, GENERIC_READ, NULL, 0, granted, st));
        EXPECT_ERR(ERROR_GENERIC_NOT_MAPPED);
        CHECK(sd_build(&c, 0, NULL, NULL, 0x3));
        SET_ERR(0);
        CHECK(!BT_CALL(&c.sd, NULL, tok, 1, NULL, 0, granted, st));
        EXPECT_ERR(ERROR_INVALID_SECURITY_DESCR);
        CloseHandle(tok);
    }
}

TEST(gap0_acbt_principal_self) {
    REQUIRE(U(AccessCheckByType)); REQUIRE(U(AddAccessAllowedAce));
    if (t_is_wine()) { t_skip("AccessCheckByType is not implemented by wine"); return; }
    HANDLE tok = imp_token(SecurityImpersonation, TOKEN_ALL_ACCESS); if (!tok) { CHECK(0); return; }
    BYTE ub[256]; PSID me = user_sid(tok, ub, sizeof ub); if (!me) { CHECK(0); return; }
    BYTE self_sid[12] = {1,1,0,0,0,0,0,5, 10,0,0,0};          /* S-1-5-10 PRINCIPAL_SELF */
    struct sdctx c; memset(&c, 0, sizeof c);
    InitializeSecurityDescriptor(&c.sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorOwner(&c.sd, G0_SID(g0_sid_fake), FALSE);
    SetSecurityDescriptorGroup(&c.sd, G0_SID(g0_sid_fake), FALSE);
    PACL acl = g0_acl(c.acl, sizeof c.acl, ACL_REVISION);
    CHECK(U(AddAccessAllowedAce)(acl, ACL_REVISION, 0x2, G0_SID(self_sid)));
    SetSecurityDescriptorDacl(&c.sd, TRUE, acl, FALSE);
    PSDECL(); DWORD granted = 0; BOOL st = 0;
    CHECK(BT_CALL(&c.sd, me, tok, 0x2, NULL, 0, granted, st));
    CHECK(st); EXPECT_EQ(granted, 2);
    granted = 0xdead; st = 1;
    SET_ERR(0);
    CHECK(BT_CALL(&c.sd, NULL, tok, 0x2, NULL, 0, granted, st));
    CHECK(!st); EXPECT_EQ(granted, 0); EXPECT_ERR(ERROR_ACCESS_DENIED);
    /* a self SID that is a different principal does not match the token user either: still granted since ACE maps to the SID given */
    granted = 0; st = 0;
    CHECK(BT_CALL(&c.sd, G0_SID(g0_sid_fake), tok, 0x2, NULL, 0, granted, st));
    t_note("PRINCIPAL_SELF mapped to foreign SID -> status %d granted 0x%lx", st, granted);
    CloseHandle(tok);
}

TEST(gap0_acbt_object_type_list_validation) {
    REQUIRE(U(AccessCheckByType)); REQUIRE(U(AddAccessAllowedAce));
    if (t_is_wine()) { t_skip("AccessCheckByType is not implemented by wine"); return; }
    HANDLE tok = imp_token(SecurityImpersonation, TOKEN_ALL_ACCESS); if (!tok) { CHECK(0); return; }
    struct sdctx c; CHECK(sd_build(&c, 0, G0_SID(g0_sid_fake), G0_SID(g0_sid_fake), 0x3));
    PSDECL(); DWORD granted = 0xdead; BOOL st = 0x55;
    OBJECT_TYPE_LIST ok2[2] = {{0, 0, &g_root}, {1, 0, &g_ch1}};
    SET_ERR(0);
    BOOL r = BT_CALL(&c.sd, NULL, tok, 1, ok2, 2, granted, st);
    CHECKF(r, "valid list err %lu", GetLastError());
    CHECK(st); EXPECT_EQ(granted, 1);
    /* first element must be level 0 */
    OBJECT_TYPE_LIST b1[2] = {{1, 0, &g_root}, {2, 0, &g_ch1}};
    SET_ERR(0); granted = 0xdead; st = 0x55;
    CHECK(!BT_CALL(&c.sd, NULL, tok, 1, b1, 2, granted, st));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    /* two roots */
    OBJECT_TYPE_LIST b2[2] = {{0, 0, &g_root}, {0, 0, &g_ch1}};
    SET_ERR(0);
    CHECK(!BT_CALL(&c.sd, NULL, tok, 1, b2, 2, granted, st));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    /* level skips */
    OBJECT_TYPE_LIST b3[2] = {{0, 0, &g_root}, {2, 0, &g_ch1}};
    SET_ERR(0);
    CHECK(!BT_CALL(&c.sd, NULL, tok, 1, b3, 2, granted, st));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    /* list pointer without length / length without pointer */
    SET_ERR(0);
    r = BT_CALL(&c.sd, NULL, tok, 1, NULL, 2, granted, st);
    CHECK(!r);
    t_note("NULL list with length 2 -> %lu", GetLastError());
    /* well formed three-level tree */
    OBJECT_TYPE_LIST tree[4] = {{0,0,&g_root},{1,0,&g_ch1},{2,0,&g_gch},{1,0,&g_ch2}};
    SET_ERR(0); st = 0;
    r = BT_CALL(&c.sd, NULL, tok, 1, tree, 4, granted, st);
    CHECKF(r, "tree err %lu", GetLastError());
    CHECK(st);
    CloseHandle(tok);
}

TEST(gap0_acbt_object_ace_scopes_to_child) {
    REQUIRE(U(AccessCheckByType)); REQUIRE(U(AddAccessAllowedObjectAce));
    if (t_is_wine()) { t_skip("AccessCheckByType is not implemented by wine"); return; }
    HANDLE tok = imp_token(SecurityImpersonation, TOKEN_ALL_ACCESS); if (!tok) { CHECK(0); return; }
    struct sdctx c; memset(&c, 0, sizeof c);
    InitializeSecurityDescriptor(&c.sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorOwner(&c.sd, G0_SID(g0_sid_fake), FALSE);
    SetSecurityDescriptorGroup(&c.sd, G0_SID(g0_sid_fake), FALSE);
    PACL acl = g0_acl(c.acl, sizeof c.acl, ACL_REVISION_DS);
    CHECK(U(AddAccessAllowedObjectAce)(acl, ACL_REVISION_DS, 0, 0x1, &g_ch1, NULL, G0_SID(g0_sid_world)));
    SetSecurityDescriptorDacl(&c.sd, TRUE, acl, FALSE);
    OBJECT_TYPE_LIST l[3] = {{0,0,&g_root},{1,0,&g_ch1},{1,0,&g_ch2}};
    PSDECL(); DWORD granted = 0xdead; BOOL st = 0x55;
    SET_ERR(0);
    BOOL r = BT_CALL(&c.sd, NULL, tok, 0x1, l, 3, granted, st);
    CHECKF(r, "err %lu", GetLastError());
    /* the root is not granted by an ACE that only names a child */
    CHECK(!st); EXPECT_EQ(granted, 0);
    /* asking only about the child subtree-less list: a one-node tree rooted at the child GUID is granted */
    OBJECT_TYPE_LIST one[1] = {{0,0,&g_ch1}};
    granted = 0; st = 0;
    CHECK(BT_CALL(&c.sd, NULL, tok, 0x1, one, 1, granted, st));
    CHECK(st); EXPECT_EQ(granted, 1);
    OBJECT_TYPE_LIST other[1] = {{0,0,&g_ch2}};
    granted = 0xdead; st = 1;
    CHECK(BT_CALL(&c.sd, NULL, tok, 0x1, other, 1, granted, st));
    CHECK(!st); EXPECT_EQ(granted, 0);
    CloseHandle(tok);
}

/* ------------------------------------------------------ ResultList */
#define RL_CALL(sd, self, tok, want, otl, n, gl, sl) \
    U(AccessCheckByTypeResultList)((sd), (self), (tok), (want), (otl), (n), &g_gm, &ps.p, &pslen, (gl), (sl))

TEST(gap0_acrl_all_granted_and_all_denied) {
    REQUIRE(U(AccessCheckByTypeResultList)); REQUIRE(U(AddAccessAllowedAce));
    if (t_is_wine()) { t_skip("unimplemented stub aborts under wine"); return; }
    HANDLE tok = imp_token(SecurityImpersonation, TOKEN_ALL_ACCESS); if (!tok) { CHECK(0); return; }
    struct sdctx c; CHECK(sd_build(&c, 0, G0_SID(g0_sid_fake), G0_SID(g0_sid_fake), 0x3));
    OBJECT_TYPE_LIST tree[4] = {{0,0,&g_root},{1,0,&g_ch1},{2,0,&g_gch},{1,0,&g_ch2}};
    PSDECL(); DWORD gl[4], sl[4];
    for (int i = 0; i < 4; i++) { gl[i] = 0xdead; sl[i] = 0x55; }
    SET_ERR(0);
    BOOL r = RL_CALL(&c.sd, NULL, tok, 0x1, tree, 4, gl, sl);
    CHECKF(r, "err %lu", GetLastError());
    for (int i = 0; i < 4; i++) { EXPECT_EQ(gl[i], 1); EXPECT_EQ(sl[i], 0); }
    for (int i = 0; i < 4; i++) { gl[i] = 0xdead; sl[i] = 0x55; }
    r = RL_CALL(&c.sd, NULL, tok, 0x4, tree, 4, gl, sl);
    CHECKF(r, "denied-case err %lu", GetLastError());
    for (int i = 0; i < 4; i++) { EXPECT_EQ(gl[i], 0); EXPECT_EQ(sl[i], ERROR_ACCESS_DENIED); }
    /* partially granted request: 0x5 asks for 1 (ok) and 4 (not) -> whole request is denied per node */
    for (int i = 0; i < 4; i++) { gl[i] = 0xdead; sl[i] = 0x55; }
    r = RL_CALL(&c.sd, NULL, tok, 0x5, tree, 4, gl, sl);
    CHECK(r);
    for (int i = 0; i < 4; i++) { EXPECT_EQ(sl[i], ERROR_ACCESS_DENIED); if (gl[i]) t_note("partial request granted list[%d]=0x%lx", i, gl[i]); }
    CloseHandle(tok);
}

TEST(gap0_acrl_single_node_list) {
    REQUIRE(U(AccessCheckByTypeResultList)); REQUIRE(U(AddAccessAllowedAce));
    if (t_is_wine()) { t_skip("unimplemented stub aborts under wine"); return; }
    HANDLE tok = imp_token(SecurityImpersonation, TOKEN_ALL_ACCESS); if (!tok) { CHECK(0); return; }
    struct sdctx c; CHECK(sd_build(&c, 0, G0_SID(g0_sid_fake), G0_SID(g0_sid_fake), 0x3));
    OBJECT_TYPE_LIST one[1] = {{0,0,&g_root}};
    PSDECL(); DWORD gl = 0xdead, sl = 0x55;
    CHECK(RL_CALL(&c.sd, NULL, tok, 0x2, one, 1, &gl, &sl));
    EXPECT_EQ(gl, 2); EXPECT_EQ(sl, 0);
    gl = 0xdead; sl = 0;
    CHECK(RL_CALL(&c.sd, NULL, tok, 0x8, one, 1, &gl, &sl));
    EXPECT_EQ(gl, 0); EXPECT_EQ(sl, ERROR_ACCESS_DENIED);
    CloseHandle(tok);
}

TEST(gap0_acrl_object_ace_per_node) {
    REQUIRE(U(AccessCheckByTypeResultList)); REQUIRE(U(AddAccessAllowedObjectAce));
    if (t_is_wine()) { t_skip("unimplemented stub aborts under wine"); return; }
    HANDLE tok = imp_token(SecurityImpersonation, TOKEN_ALL_ACCESS); if (!tok) { CHECK(0); return; }
    struct sdctx c; memset(&c, 0, sizeof c);
    InitializeSecurityDescriptor(&c.sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorOwner(&c.sd, G0_SID(g0_sid_fake), FALSE);
    SetSecurityDescriptorGroup(&c.sd, G0_SID(g0_sid_fake), FALSE);
    PACL acl = g0_acl(c.acl, sizeof c.acl, ACL_REVISION_DS);
    CHECK(U(AddAccessAllowedObjectAce)(acl, ACL_REVISION_DS, 0, 0x1, &g_ch1, NULL, G0_SID(g0_sid_world)));
    SetSecurityDescriptorDacl(&c.sd, TRUE, acl, FALSE);
    OBJECT_TYPE_LIST l[3] = {{0,0,&g_root},{1,0,&g_ch1},{1,0,&g_ch2}};
    PSDECL(); DWORD gl[3] = {0xdead,0xdead,0xdead}, sl[3] = {0x55,0x55,0x55};
    BOOL r = RL_CALL(&c.sd, NULL, tok, 0x1, l, 3, gl, sl);
    CHECKF(r, "err %lu", GetLastError());
    EXPECT_EQ(gl[1], 1); EXPECT_EQ(sl[1], 0);          /* the child named by the ACE */
    EXPECT_EQ(gl[2], 0); EXPECT_EQ(sl[2], ERROR_ACCESS_DENIED);
    t_note("root node: granted 0x%lx status %lu", gl[0], sl[0]);
    CloseHandle(tok);
}

TEST(gap0_acrl_errors) {
    REQUIRE(U(AccessCheckByTypeResultList)); REQUIRE(U(AddAccessAllowedAce));
    if (t_is_wine()) { t_skip("unimplemented stub aborts under wine"); return; }
    struct sdctx c; CHECK(sd_build(&c, 0, G0_SID(g0_sid_fake), G0_SID(g0_sid_fake), 0x3));
    OBJECT_TYPE_LIST one[1] = {{0,0,&g_root}};
    PSDECL(); DWORD gl[2] = {0xdead,0xdead}, sl[2] = {0x55,0x55};
    HANDLE pt = prim_token();
    if (pt) { SET_ERR(0); CHECK(!RL_CALL(&c.sd, NULL, pt, 1, one, 1, gl, sl)); EXPECT_ERR(ERROR_NO_IMPERSONATION_TOKEN); CloseHandle(pt); }
    HANDLE bad[] = {NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x7777};
    for (int i = 0; i < 3; i++) { SET_ERR(0); CHECK(!RL_CALL(&c.sd, NULL, bad[i], 1, one, 1, gl, sl)); EXPECT_ERR(ERROR_INVALID_HANDLE); }
    t_note("result list after handle failures: 0x%lx", gl[0]);
    HANDLE tok = imp_token(SecurityImpersonation, TOKEN_ALL_ACCESS);
    if (tok) {
        SET_ERR(0);
        CHECK(!RL_CALL(&c.sd, NULL, tok, GENERIC_READ, one, 1, gl, sl));
        EXPECT_ERR(ERROR_GENERIC_NOT_MAPPED);
        OBJECT_TYPE_LIST b1[2] = {{1, 0, &g_root}, {2, 0, &g_ch1}};
        SET_ERR(0);
        CHECK(!RL_CALL(&c.sd, NULL, tok, 1, b1, 2, gl, sl));
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        SET_ERR(0);
        BOOL r = RL_CALL(&c.sd, NULL, tok, 1, NULL, 0, gl, sl);
        t_note("ResultList with no object type list -> %d / %lu", r, GetLastError());
        CloseHandle(tok);
    }
}

/* ------------------------------------------------------ audit alarm */
static int audit_priv_enabled(void) {
    HANDLE t = prim_token(); int r = 0;
    if (t) { r = priv_enabled(t, "SeAuditPrivilege"); CloseHandle(t); }
    return r;
}

TEST(gap0_audit_accesscheck_alarm_requires_privilege) {
    REQUIRE(U(AccessCheckAndAuditAlarmW)); REQUIRE(U(AddAccessAllowedAce));
    if (t_is_wine()) { t_skip("unimplemented stub aborts under wine"); return; }
    if (audit_priv_enabled()) { t_skip("SeAuditPrivilege is enabled; would emit audit records"); return; }
    struct sdctx c; CHECK(sd_build(&c, 0, G0_SID(g0_sid_fake), G0_SID(g0_sid_fake), 0x3));
    WCHAR sub[] = L"gap0", typ[] = L"Thing", name[] = L"Obj";
    DWORD granted = 0xdead; BOOL st = 0x55, gen = 0x77;
    SET_ERR(0);
    BOOL r = U(AccessCheckAndAuditAlarmW)(sub, NULL, typ, name, &c.sd, 1, &g_gm, FALSE, &granted, &st, &gen);
    CHECK(!r);
    EXPECT_ERR(ERROR_NO_IMPERSONATION_TOKEN);   /* Win10: thread not impersonating -> bail before the SeAuditPrivilege check */
    EXPECT_EQ(gen, 0);                 /* always written by the user-mode wrapper */
}

TEST(gap0_audit_bytype_alarm_requires_privilege) {
    REQUIRE(U(AccessCheckByTypeAndAuditAlarmW)); REQUIRE(U(AddAccessAllowedAce));
    if (t_is_wine()) { t_skip("unimplemented stub aborts under wine"); return; }
    if (audit_priv_enabled()) { t_skip("SeAuditPrivilege is enabled"); return; }
    struct sdctx c; CHECK(sd_build(&c, 0, G0_SID(g0_sid_fake), G0_SID(g0_sid_fake), 0x3));
    DWORD granted = 0xdead; BOOL st = 0x55, gen = 0x77;
    SET_ERR(0);
    BOOL r = U(AccessCheckByTypeAndAuditAlarmW)(L"gap0", NULL, L"Thing", L"Obj", &c.sd, NULL, 1, AuditEventObjectAccess, 0, NULL, 0, &g_gm, FALSE, &granted, &st, &gen);
    CHECK(!r);
    EXPECT_ERR(ERROR_NO_IMPERSONATION_TOKEN);   /* Win10: thread not impersonating -> bail before the SeAuditPrivilege check */
    EXPECT_EQ(gen, 0);
    OBJECT_TYPE_LIST one[1] = {{0,0,&g_root}};
    gen = 0x77; SET_ERR(0);
    r = U(AccessCheckByTypeAndAuditAlarmW)(L"gap0", NULL, L"Thing", L"Obj", &c.sd, NULL, 1, AuditEventDirectoryServiceAccess, 0, one, 1, &g_gm, TRUE, &granted, &st, &gen);
    CHECK(!r);
    EXPECT_ERR(ERROR_NO_IMPERSONATION_TOKEN);   /* Win10: thread not impersonating -> bail before the SeAuditPrivilege check */
    EXPECT_EQ(gen, 0);
}

TEST(gap0_audit_resultlist_alarm_requires_privilege) {
    REQUIRE(U(AccessCheckByTypeResultListAndAuditAlarmW)); REQUIRE(U(AddAccessAllowedAce));
    if (t_is_wine()) { t_skip("unimplemented stub aborts under wine"); return; }
    if (audit_priv_enabled()) { t_skip("SeAuditPrivilege is enabled"); return; }
    struct sdctx c; CHECK(sd_build(&c, 0, G0_SID(g0_sid_fake), G0_SID(g0_sid_fake), 0x3));
    OBJECT_TYPE_LIST one[1] = {{0,0,&g_root}};
    DWORD gl[1] = {0xdead}, sl[1] = {0x55}; BOOL gen = 0x77;
    SET_ERR(0);
    BOOL r = U(AccessCheckByTypeResultListAndAuditAlarmW)(L"gap0", NULL, L"Thing", L"Obj", &c.sd, NULL, 1, AuditEventObjectAccess, 0, one, 1, &g_gm, FALSE, gl, sl, &gen);
    CHECK(!r);
    EXPECT_ERR(ERROR_NO_IMPERSONATION_TOKEN);   /* Win10: thread not impersonating -> bail before the SeAuditPrivilege check */
    EXPECT_EQ(gen, 0);
}

TEST(gap0_audit_resultlist_byhandle_alarm_requires_privilege) {
    REQUIRE(U(AccessCheckByTypeResultListAndAuditAlarmByHandleW)); REQUIRE(U(AddAccessAllowedAce));
    if (t_is_wine()) { t_skip("unimplemented stub aborts under wine"); return; }
    if (audit_priv_enabled()) { t_skip("SeAuditPrivilege is enabled"); return; }
    HANDLE tok = imp_token(SecurityImpersonation, TOKEN_ALL_ACCESS); if (!tok) { CHECK(0); return; }
    struct sdctx c; CHECK(sd_build(&c, 0, G0_SID(g0_sid_fake), G0_SID(g0_sid_fake), 0x3));
    OBJECT_TYPE_LIST one[1] = {{0,0,&g_root}};
    DWORD gl[1] = {0xdead}, sl[1] = {0x55}; BOOL gen = 0x77;
    SET_ERR(0);
    BOOL r = U(AccessCheckByTypeResultListAndAuditAlarmByHandleW)(L"gap0", NULL, tok, L"Thing", L"Obj", &c.sd, NULL, 1, AuditEventObjectAccess, 0, one, 1, &g_gm, FALSE, gl, sl, &gen);
    CHECK(!r);
    EXPECT_ERR(ERROR_PRIVILEGE_NOT_HELD);
    EXPECT_EQ(gen, 0);
    /* an invalid handle: privilege or handle error, but never success */
    gen = 0x77; SET_ERR(0);
    r = U(AccessCheckByTypeResultListAndAuditAlarmByHandleW)(L"gap0", NULL, INVALID_HANDLE_VALUE, L"Thing", L"Obj", &c.sd, NULL, 1, AuditEventObjectAccess, 0, one, 1, &g_gm, FALSE, gl, sl, &gen);
    CHECK(!r);
    t_note("ByHandle with invalid token and no privilege -> %lu", GetLastError());
    EXPECT_EQ(gen, 0);
    CloseHandle(tok);
}
