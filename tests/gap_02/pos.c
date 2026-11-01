/* CreatePrivateObjectSecurity{,Ex,WithMultipleInheritance}, DestroyPrivateObjectSecurity */
#include "g2.h"

typedef BOOL (WINAPI *cpos_fn)(PSECURITY_DESCRIPTOR, PSECURITY_DESCRIPTOR, PSECURITY_DESCRIPTOR *, BOOL, HANDLE, PGENERIC_MAPPING);
typedef BOOL (WINAPI *cpose_fn)(PSECURITY_DESCRIPTOR, PSECURITY_DESCRIPTOR, PSECURITY_DESCRIPTOR *, GUID *, BOOL, ULONG, HANDLE, PGENERIC_MAPPING);
typedef BOOL (WINAPI *cposm_fn)(PSECURITY_DESCRIPTOR, PSECURITY_DESCRIPTOR, PSECURITY_DESCRIPTOR *, GUID **, ULONG, BOOL, ULONG, HANDLE, PGENERIC_MAPPING);
typedef BOOL (WINAPI *dpos_fn)(PSECURITY_DESCRIPTOR *);

/* The kernel masks every effective ACE with GenericAll, so GenericAll must cover the specific-right bits the tests use */
static GENERIC_MAPPING gm = { 0x1, 0x2, 0x4, 0x1FFFFF };

/* Build an absolute SD: optional owner/group, DACL from up to 4 (mask, flags, sid) entries. */
typedef struct { DWORD mask; BYTE flags; PSID sid; } ace_spec;
typedef struct {
    SECURITY_DESCRIPTOR sd;
    BYTE acl[512];
    BYTE usid[16], ugid[16];
} sdbuf;

static void sd_make(sdbuf *b, PSID owner, PSID group, const ace_spec *a, int n, BOOL have_dacl)
{
    int i;
    memset(b, 0, sizeof *b);
    InitializeSecurityDescriptor(&b->sd, SECURITY_DESCRIPTOR_REVISION);
    if (owner) SetSecurityDescriptorOwner(&b->sd, owner, FALSE);
    if (group) SetSecurityDescriptorGroup(&b->sd, group, FALSE);
    if (have_dacl) {
        InitializeAcl((PACL)b->acl, sizeof b->acl, ACL_REVISION);
        for (i = 0; i < n; i++) AddAccessAllowedAceEx((PACL)b->acl, ACL_REVISION, a[i].flags, a[i].mask, a[i].sid);
        SetSecurityDescriptorDacl(&b->sd, TRUE, (PACL)b->acl, FALSE);
    }
}

static PACL sd_dacl(PSECURITY_DESCRIPTOR sd, BOOL *present)
{
    PACL a = NULL; BOOL def = FALSE; BOOL pr = FALSE;
    if (!GetSecurityDescriptorDacl(sd, &pr, &a, &def)) pr = FALSE;
    if (present) *present = pr;
    return pr ? a : NULL;
}
static HANDLE imp_token(void)
{
    HANDLE t = g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY), n = NULL;
    if (!t) return NULL;
    if (!DuplicateToken(t, SecurityImpersonation, &n)) n = NULL;
    CloseHandle(t);
    return n;
}

static BYTE evid[16], sysid[16], coid[16];
#define EVERYONE g2_mksid(evid, 1, 1, 0u)
#define SYSTEM_SID g2_mksid(sysid, 5, 1, 18u)
#define CREATOR_OWNER g2_mksid(coid, 3, 1, 0u)

/* ------------- CreatePrivateObjectSecurity ------------- */
TEST(gap2_cpos_defaults_null_token) {
    cpos_fn f = (cpos_fn)t_proc("CreatePrivateObjectSecurity");
    PSECURITY_DESCRIPTOR sd = NULL;
    REQUIRE(f);
    /* a NULL token supplies no default owner/group: the call fails with STATUS_NO_TOKEN */
    SET_ERR(0);
    CHECK(!f(NULL, NULL, &sd, FALSE, NULL, &gm));
    EXPECT_ERR(ERROR_NO_TOKEN);
    CHECK(sd == NULL);
}
TEST(gap2_cpos_owner_matches_token) {
    cpos_fn f = (cpos_fn)t_proc("CreatePrivateObjectSecurity");
    dpos_fn d = (dpos_fn)t_proc("DestroyPrivateObjectSecurity");
    PSECURITY_DESCRIPTOR sd = NULL; PSID o = NULL, g = NULL; BOOL def; HANDLE t;
    DWORD n; TOKEN_OWNER *to; TOKEN_PRIMARY_GROUP *tg;
    REQUIRE(f); REQUIRE(d);
    t = imp_token(); if (!t) { CHECK(0); return; }
    CHECK(f(NULL, NULL, &sd, FALSE, t, &gm));
    if (sd) {
        GetSecurityDescriptorOwner(sd, &o, &def);
        GetSecurityDescriptorGroup(sd, &g, &def);
        n = 0; GetTokenInformation(t, TokenOwner, NULL, 0, &n);
        to = malloc(n); GetTokenInformation(t, TokenOwner, to, n, &n);
        n = 0; GetTokenInformation(t, TokenPrimaryGroup, NULL, 0, &n);
        tg = malloc(n); GetTokenInformation(t, TokenPrimaryGroup, tg, n, &n);
        CHECK(o && EqualSid(o, to->Owner));
        CHECK(g && EqualSid(g, tg->PrimaryGroup));
        free(to); free(tg);
        d(&sd);
    }
    CloseHandle(t);
}
TEST(gap2_cpos_default_dacl_from_token) {
    cpos_fn f = (cpos_fn)t_proc("CreatePrivateObjectSecurity");
    dpos_fn d = (dpos_fn)t_proc("DestroyPrivateObjectSecurity");
    PSECURITY_DESCRIPTOR sd = NULL; BOOL pr; HANDLE t; DWORD n; TOKEN_DEFAULT_DACL *td;
    REQUIRE(f); REQUIRE(d);
    t = imp_token(); if (!t) { CHECK(0); return; }
    CHECK(f(NULL, NULL, &sd, FALSE, t, &gm));
    if (sd) {
        PACL a = sd_dacl(sd, &pr);
        n = 0; GetTokenInformation(t, TokenDefaultDacl, NULL, 0, &n);
        td = malloc(n); GetTokenInformation(t, TokenDefaultDacl, td, n, &n);
        t_note("token default DACL %s, object DACL present=%d", td->DefaultDacl ? "present" : "absent", pr);
        if (td->DefaultDacl) { CHECK(pr); CHECK(a && g2_ace_count(a) == g2_ace_count(td->DefaultDacl)); }
        free(td);
        d(&sd);
    }
    CloseHandle(t);
}
TEST(gap2_cpos_primary_token_accepted) {
    cpos_fn f = (cpos_fn)t_proc("CreatePrivateObjectSecurity");
    dpos_fn d = (dpos_fn)t_proc("DestroyPrivateObjectSecurity");
    PSECURITY_DESCRIPTOR sd = NULL; HANDLE t = g2_open_proc_token(TOKEN_QUERY);
    REQUIRE(f); REQUIRE(d);
    if (!t) { CHECK(0); return; }
    BOOL ok = f(NULL, NULL, &sd, FALSE, t, &gm);
    t_note("primary token handle: ok=%d err=%lu", ok, ok ? 0 : GetLastError());
    if (ok) d(&sd);
    CloseHandle(t);
}
TEST(gap2_cpos_token_needs_query) {
    cpos_fn f = (cpos_fn)t_proc("CreatePrivateObjectSecurity");
    dpos_fn d = (dpos_fn)t_proc("DestroyPrivateObjectSecurity");
    PSECURITY_DESCRIPTOR sd = NULL; HANDLE t = g2_open_proc_token(TOKEN_DUPLICATE);
    REQUIRE(f); REQUIRE(d);
    if (!t) { CHECK(0); return; }
    SET_ERR(0);
    BOOL ok = f(NULL, NULL, &sd, FALSE, t, &gm);
    CHECK(!ok);
    t_note("token without TOKEN_QUERY: err=%lu", GetLastError());
    if (ok) d(&sd);
    CloseHandle(t);
}
TEST(gap2_cpos_invalid_token_handles) {
    cpos_fn f = (cpos_fn)t_proc("CreatePrivateObjectSecurity");
    PSECURITY_DESCRIPTOR sd = NULL; HANDLE ev;
    REQUIRE(f);
    SET_ERR(0);
    CHECK(!f(NULL, NULL, &sd, FALSE, (HANDLE)(ULONG_PTR)0x7FFFFFF0, &gm));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CHECK(sd == NULL);
    ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    CHECK(!f(NULL, NULL, &sd, FALSE, ev, &gm));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
    SET_ERR(0);
    BOOL ok = f(NULL, NULL, &sd, FALSE, INVALID_HANDLE_VALUE, &gm);
    t_note("INVALID_HANDLE_VALUE token: ok=%d err=%lu", ok, ok ? 0 : GetLastError());
    if (ok) LocalFree(sd);
}
TEST(gap2_cpos_creator_dacl_kept) {
    cpos_fn f = (cpos_fn)t_proc("CreatePrivateObjectSecurity");
    dpos_fn d = (dpos_fn)t_proc("DestroyPrivateObjectSecurity");
    PSECURITY_DESCRIPTOR sd = NULL; sdbuf cr; ace_spec a[1]; HANDLE t; BOOL pr;
    REQUIRE(f); REQUIRE(d);
    a[0].mask = 0x1234; a[0].flags = 0; a[0].sid = EVERYONE;
    sd_make(&cr, NULL, NULL, a, 1, TRUE);
    t = imp_token(); if (!t) { CHECK(0); return; }
    CHECK(f(NULL, &cr.sd, &sd, FALSE, t, &gm));
    if (sd) {
        PACL acl = sd_dacl(sd, &pr);
        CHECK(pr && acl);
        EXPECT_EQ(g2_ace_count(acl), 1);
        ACE_HEADER *h = g2_find_ace(acl, 0x1234);
        CHECK(h != NULL);
        if (h) CHECK(EqualSid(g2_ace_sid(h), evid));
        d(&sd);
    }
    CloseHandle(t);
}
TEST(gap2_cpos_creator_null_dacl_kept) {
    cpos_fn f = (cpos_fn)t_proc("CreatePrivateObjectSecurity");
    dpos_fn d = (dpos_fn)t_proc("DestroyPrivateObjectSecurity");
    PSECURITY_DESCRIPTOR sd = NULL; sdbuf cr; HANDLE t; BOOL pr = FALSE, def; PACL acl = (PACL)1;
    REQUIRE(f); REQUIRE(d);
    sd_make(&cr, NULL, NULL, NULL, 0, FALSE);
    SetSecurityDescriptorDacl(&cr.sd, TRUE, NULL, FALSE);    /* NULL DACL = everyone full access */
    t = imp_token(); if (!t) { CHECK(0); return; }
    CHECK(f(NULL, &cr.sd, &sd, FALSE, t, &gm));
    if (sd) {
        CHECK(GetSecurityDescriptorDacl(sd, &pr, &acl, &def));
        CHECK(pr);
        CHECK(acl == NULL);
        d(&sd);
    }
    CloseHandle(t);
}
TEST(gap2_cpos_creator_empty_dacl_kept) {
    cpos_fn f = (cpos_fn)t_proc("CreatePrivateObjectSecurity");
    dpos_fn d = (dpos_fn)t_proc("DestroyPrivateObjectSecurity");
    PSECURITY_DESCRIPTOR sd = NULL; sdbuf cr; HANDLE t; BOOL pr;
    REQUIRE(f); REQUIRE(d);
    sd_make(&cr, NULL, NULL, NULL, 0, TRUE);
    t = imp_token(); if (!t) { CHECK(0); return; }
    CHECK(f(NULL, &cr.sd, &sd, FALSE, t, &gm));
    if (sd) {
        PACL acl = sd_dacl(sd, &pr);
        CHECK(pr && acl);
        EXPECT_EQ(g2_ace_count(acl), 0);
        d(&sd);
    }
    CloseHandle(t);
}
TEST(gap2_cpos_creator_owner_group_note) {
    cpos_fn f = (cpos_fn)t_proc("CreatePrivateObjectSecurity");
    dpos_fn d = (dpos_fn)t_proc("DestroyPrivateObjectSecurity");
    PSECURITY_DESCRIPTOR sd = NULL; sdbuf cr; HANDLE t; PSID o = NULL, g = NULL; BOOL def;
    BYTE b1[16], b2[16];
    REQUIRE(f); REQUIRE(d);
    sd_make(&cr, g2_mksid(b1, 1, 1, 0u), g2_mksid(b2, 5, 1, 18u), NULL, 0, FALSE);
    t = imp_token(); if (!t) { CHECK(0); return; }
    BOOL ok = f(NULL, &cr.sd, &sd, FALSE, t, &gm);
    t_note("creator-specified owner Everyone: ok=%d err=%lu", ok, ok ? 0 : GetLastError());
    if (ok) {
        GetSecurityDescriptorOwner(sd, &o, &def); GetSecurityDescriptorGroup(sd, &g, &def);
        t_note("owner==Everyone: %d group==System: %d", o && EqualSid(o, b1), g && EqualSid(g, b2));
        d(&sd);
    }
    CloseHandle(t);
}
TEST(gap2_cpos_inherit_container) {
    cpos_fn f = (cpos_fn)t_proc("CreatePrivateObjectSecurity");
    dpos_fn d = (dpos_fn)t_proc("DestroyPrivateObjectSecurity");
    PSECURITY_DESCRIPTOR sd = NULL; sdbuf par; ace_spec a[1]; HANDLE t; BOOL pr;
    REQUIRE(f); REQUIRE(d);
    a[0].mask = 0x1234; a[0].flags = CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE; a[0].sid = EVERYONE;
    sd_make(&par, SYSTEM_SID, SYSTEM_SID, a, 1, TRUE);
    t = imp_token(); if (!t) { CHECK(0); return; }
    CHECK(f(&par.sd, NULL, &sd, TRUE, t, &gm));
    if (sd) {
        PACL acl = sd_dacl(sd, &pr);
        /* a container child gets an effective ACE and an inherit-only copy; look for the effective one */
        ACE_HEADER *h = NULL; DWORD i, n = acl ? g2_ace_count(acl) : 0;
        for (i = 0; i < n; i++) {
            void *ace; GetAce(acl, i, &ace);
            if (((ACCESS_ALLOWED_ACE *)ace)->Mask == 0x1234 && !(((ACE_HEADER *)ace)->AceFlags & INHERIT_ONLY_ACE)) { h = (ACE_HEADER *)ace; break; }
        }
        CHECK(h != NULL);
        if (h) {
            CHECK(EqualSid(g2_ace_sid(h), evid));
            t_note("container child ACE flags 0x%x", h->AceFlags);
        }
        d(&sd);
    }
    CloseHandle(t);
}
TEST(gap2_cpos_inherit_leaf_object) {
    cpos_fn f = (cpos_fn)t_proc("CreatePrivateObjectSecurity");
    dpos_fn d = (dpos_fn)t_proc("DestroyPrivateObjectSecurity");
    PSECURITY_DESCRIPTOR sd = NULL; sdbuf par; ace_spec a[1]; HANDLE t; BOOL pr;
    REQUIRE(f); REQUIRE(d);
    a[0].mask = 0x1234; a[0].flags = OBJECT_INHERIT_ACE; a[0].sid = EVERYONE;
    sd_make(&par, SYSTEM_SID, SYSTEM_SID, a, 1, TRUE);
    t = imp_token(); if (!t) { CHECK(0); return; }
    CHECK(f(&par.sd, NULL, &sd, FALSE, t, &gm));
    if (sd) {
        PACL acl = sd_dacl(sd, &pr);
        ACE_HEADER *h = g2_find_ace(acl, 0x1234);
        CHECK(h != NULL);
        if (h) CHECK(!(h->AceFlags & (OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE | INHERIT_ONLY_ACE | NO_PROPAGATE_INHERIT_ACE)));
        d(&sd);
    }
    CloseHandle(t);
}
TEST(gap2_cpos_container_only_ace_not_inherited_by_leaf) {
    cpos_fn f = (cpos_fn)t_proc("CreatePrivateObjectSecurity");
    dpos_fn d = (dpos_fn)t_proc("DestroyPrivateObjectSecurity");
    PSECURITY_DESCRIPTOR sd = NULL; sdbuf par; ace_spec a[1]; HANDLE t; BOOL pr;
    REQUIRE(f); REQUIRE(d);
    a[0].mask = 0x1234; a[0].flags = CONTAINER_INHERIT_ACE; a[0].sid = EVERYONE;
    sd_make(&par, SYSTEM_SID, SYSTEM_SID, a, 1, TRUE);
    t = imp_token(); if (!t) { CHECK(0); return; }
    CHECK(f(&par.sd, NULL, &sd, FALSE, t, &gm));
    if (sd) {
        PACL acl = sd_dacl(sd, &pr);
        CHECK(g2_find_ace(acl, 0x1234) == NULL);
        d(&sd);
    }
    CloseHandle(t);
}
TEST(gap2_cpos_object_only_ace_container_inherit_only) {
    cpos_fn f = (cpos_fn)t_proc("CreatePrivateObjectSecurity");
    dpos_fn d = (dpos_fn)t_proc("DestroyPrivateObjectSecurity");
    PSECURITY_DESCRIPTOR sd = NULL; sdbuf par; ace_spec a[1]; HANDLE t; BOOL pr;
    REQUIRE(f); REQUIRE(d);
    a[0].mask = 0x1234; a[0].flags = OBJECT_INHERIT_ACE; a[0].sid = EVERYONE;
    sd_make(&par, SYSTEM_SID, SYSTEM_SID, a, 1, TRUE);
    t = imp_token(); if (!t) { CHECK(0); return; }
    CHECK(f(&par.sd, NULL, &sd, TRUE, t, &gm));
    if (sd) {
        PACL acl = sd_dacl(sd, &pr);
        ACE_HEADER *h = g2_find_ace(acl, 0x1234);
        CHECK(h != NULL);
        if (h) {
            CHECK(h->AceFlags & INHERIT_ONLY_ACE);
            CHECK(h->AceFlags & OBJECT_INHERIT_ACE);
        }
        d(&sd);
    }
    CloseHandle(t);
}
TEST(gap2_cpos_no_inherit_flags_not_inherited) {
    cpos_fn f = (cpos_fn)t_proc("CreatePrivateObjectSecurity");
    dpos_fn d = (dpos_fn)t_proc("DestroyPrivateObjectSecurity");
    PSECURITY_DESCRIPTOR sd = NULL; sdbuf par; ace_spec a[1]; HANDLE t; BOOL pr; int isdir;
    REQUIRE(f); REQUIRE(d);
    a[0].mask = 0x1234; a[0].flags = 0; a[0].sid = EVERYONE;
    sd_make(&par, SYSTEM_SID, SYSTEM_SID, a, 1, TRUE);
    t = imp_token(); if (!t) { CHECK(0); return; }
    for (isdir = 0; isdir < 2; isdir++) {
        sd = NULL;
        CHECK(f(&par.sd, NULL, &sd, isdir, t, &gm));
        if (sd) { CHECK(g2_find_ace(sd_dacl(sd, &pr), 0x1234) == NULL); d(&sd); }
    }
    CloseHandle(t);
}
TEST(gap2_cpos_no_propagate_inherit) {
    cpos_fn f = (cpos_fn)t_proc("CreatePrivateObjectSecurity");
    dpos_fn d = (dpos_fn)t_proc("DestroyPrivateObjectSecurity");
    PSECURITY_DESCRIPTOR sd = NULL; sdbuf par; ace_spec a[1]; HANDLE t; BOOL pr;
    REQUIRE(f); REQUIRE(d);
    a[0].mask = 0x1234; a[0].flags = CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE | NO_PROPAGATE_INHERIT_ACE; a[0].sid = EVERYONE;
    sd_make(&par, SYSTEM_SID, SYSTEM_SID, a, 1, TRUE);
    t = imp_token(); if (!t) { CHECK(0); return; }
    CHECK(f(&par.sd, NULL, &sd, TRUE, t, &gm));
    if (sd) {
        PACL acl = sd_dacl(sd, &pr);
        ACE_HEADER *h = g2_find_ace(acl, 0x1234);
        CHECK(h != NULL);
        if (h) CHECK(!(h->AceFlags & (OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE | INHERIT_ONLY_ACE | NO_PROPAGATE_INHERIT_ACE)));
        d(&sd);
    }
    CloseHandle(t);
}
TEST(gap2_cpos_creator_owner_substitution) {
    cpos_fn f = (cpos_fn)t_proc("CreatePrivateObjectSecurity");
    dpos_fn d = (dpos_fn)t_proc("DestroyPrivateObjectSecurity");
    PSECURITY_DESCRIPTOR sd = NULL; sdbuf par; ace_spec a[1]; HANDLE t; BOOL pr, def; PSID o = NULL;
    REQUIRE(f); REQUIRE(d);
    a[0].mask = 0x2345; a[0].flags = CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE; a[0].sid = CREATOR_OWNER;
    sd_make(&par, SYSTEM_SID, SYSTEM_SID, a, 1, TRUE);
    t = imp_token(); if (!t) { CHECK(0); return; }
    CHECK(f(&par.sd, NULL, &sd, TRUE, t, &gm));
    if (sd) {
        PACL acl = sd_dacl(sd, &pr);
        GetSecurityDescriptorOwner(sd, &o, &def);
        DWORD i, n = acl ? g2_ace_count(acl) : 0; int effective = 0;
        CHECK(acl != NULL);
        for (i = 0; i < n; i++) {
            void *ace; GetAce(acl, i, &ace);
            if (((ACCESS_ALLOWED_ACE *)ace)->Mask == 0x2345 && !(((ACE_HEADER *)ace)->AceFlags & INHERIT_ONLY_ACE)) {
                effective++;
                CHECK(o && EqualSid(g2_ace_sid((ACE_HEADER *)ace), o));
            }
        }
        CHECK(effective >= 1);
        d(&sd);
    }
    CloseHandle(t);
}
TEST(gap2_cpos_owner_from_token_not_parent) {
    cpos_fn f = (cpos_fn)t_proc("CreatePrivateObjectSecurity");
    dpos_fn d = (dpos_fn)t_proc("DestroyPrivateObjectSecurity");
    PSECURITY_DESCRIPTOR sd = NULL; sdbuf par; HANDLE t; PSID o = NULL; BOOL def;
    REQUIRE(f); REQUIRE(d);
    sd_make(&par, SYSTEM_SID, SYSTEM_SID, NULL, 0, FALSE);
    t = imp_token(); if (!t) { CHECK(0); return; }
    CHECK(f(&par.sd, NULL, &sd, FALSE, t, &gm));
    if (sd) {
        GetSecurityDescriptorOwner(sd, &o, &def);
        t_note("owner with SYSTEM parent equals S-1-5-18: %d", o && EqualSid(o, sysid));
        CHECK(o != NULL);
        d(&sd);
    }
    CloseHandle(t);
}
TEST(gap2_cpos_generic_mapping_applied) {
    cpos_fn f = (cpos_fn)t_proc("CreatePrivateObjectSecurity");
    dpos_fn d = (dpos_fn)t_proc("DestroyPrivateObjectSecurity");
    PSECURITY_DESCRIPTOR sd = NULL; sdbuf cr; ace_spec a[2]; HANDLE t; BOOL pr;
    REQUIRE(f); REQUIRE(d);
    a[0].mask = GENERIC_ALL; a[0].flags = 0; a[0].sid = EVERYONE;
    a[1].mask = GENERIC_READ; a[1].flags = 0; a[1].sid = SYSTEM_SID;
    sd_make(&cr, NULL, NULL, a, 2, TRUE);
    t = imp_token(); if (!t) { CHECK(0); return; }
    CHECK(f(NULL, &cr.sd, &sd, FALSE, t, &gm));
    if (sd) {
        PACL acl = sd_dacl(sd, &pr);
        DWORD i; 
        for (i = 0; acl && i < g2_ace_count(acl); i++) {
            void *ace; GetAce(acl, i, &ace);
            t_note("ACE %lu mask 0x%lx", i, ((ACCESS_ALLOWED_ACE *)ace)->Mask);
        }
        d(&sd);
    }
    CloseHandle(t);
}
TEST(gap2_cpos_directory_flag_with_creator_dacl) {
    cpos_fn f = (cpos_fn)t_proc("CreatePrivateObjectSecurity");
    dpos_fn d = (dpos_fn)t_proc("DestroyPrivateObjectSecurity");
    PSECURITY_DESCRIPTOR sd = NULL; HANDLE t; int isdir;
    REQUIRE(f); REQUIRE(d);
    t = imp_token(); if (!t) { CHECK(0); return; }
    for (isdir = 0; isdir <= 1; isdir++) {
        sd = NULL;
        CHECK(f(NULL, NULL, &sd, isdir ? TRUE : FALSE, t, &gm));
        if (sd) { CHECK(IsValidSecurityDescriptor(sd)); d(&sd); }
    }
    CloseHandle(t);
}
TEST(gap2_cpos_results_are_independent) {
    cpos_fn f = (cpos_fn)t_proc("CreatePrivateObjectSecurity");
    dpos_fn d = (dpos_fn)t_proc("DestroyPrivateObjectSecurity");
    PSECURITY_DESCRIPTOR a = NULL, b = NULL; HANDLE t;
    REQUIRE(f); REQUIRE(d);
    t = imp_token(); if (!t) { CHECK(0); return; }
    CHECK(f(NULL, NULL, &a, FALSE, t, &gm));
    CHECK(f(NULL, NULL, &b, FALSE, t, &gm));
    CHECK(a != b);
    if (a && b) {
        CHECK(GetSecurityDescriptorLength(a) == GetSecurityDescriptorLength(b));
        CHECK(d(&a));
        CHECK(IsValidSecurityDescriptor(b));
        CHECK(d(&b));
    }
    CloseHandle(t);
}
TEST(gap2_cpos_stress) {
    cpos_fn f = (cpos_fn)t_proc("CreatePrivateObjectSecurity");
    dpos_fn d = (dpos_fn)t_proc("DestroyPrivateObjectSecurity");
    HANDLE t; int i;
    REQUIRE(f); REQUIRE(d);
    t = imp_token(); if (!t) { CHECK(0); return; }
    for (i = 0; i < 1000; i++) {
        PSECURITY_DESCRIPTOR sd = NULL;
        if (!f(NULL, NULL, &sd, (i & 1) != 0, t, &gm)) { CHECK(0); break; }
        d(&sd);
    }
    CloseHandle(t);
}

/* ------------- DestroyPrivateObjectSecurity ------------- */
TEST(gap2_dpos_clears_pointer_note) {
    cpos_fn f = (cpos_fn)t_proc("CreatePrivateObjectSecurity");
    dpos_fn d = (dpos_fn)t_proc("DestroyPrivateObjectSecurity");
    PSECURITY_DESCRIPTOR sd = NULL; HANDLE t;
    REQUIRE(f); REQUIRE(d);
    t = imp_token(); if (!t) { CHECK(0); return; }
    CHECK(f(NULL, NULL, &sd, FALSE, t, &gm));
    CHECK(d(&sd));
    t_note("pointer after destroy: %p", sd);
    CloseHandle(t);
}
TEST(gap2_dpos_null_pointer_variable) {
    dpos_fn d = (dpos_fn)t_proc("DestroyPrivateObjectSecurity");
    PSECURITY_DESCRIPTOR sd = NULL;
    REQUIRE(d);
    SET_ERR(0);
    BOOL ok = d(&sd);
    t_note("Destroy(&NULL): ok=%d err=%lu", ok, ok ? 0 : GetLastError());
}

/* ------------- CreatePrivateObjectSecurityEx ------------- */
TEST(gap2_cposex_auto_inherit_flag) {
    cpose_fn f = (cpose_fn)t_proc("CreatePrivateObjectSecurityEx");
    dpos_fn d = (dpos_fn)t_proc("DestroyPrivateObjectSecurity");
    PSECURITY_DESCRIPTOR sd = NULL; sdbuf par; ace_spec a[1]; HANDLE t; BOOL pr; WORD ctl; DWORD rev;
    REQUIRE(f); REQUIRE(d);
    a[0].mask = 0x1234; a[0].flags = CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE; a[0].sid = EVERYONE;
    sd_make(&par, SYSTEM_SID, SYSTEM_SID, a, 1, TRUE);
    t = imp_token(); if (!t) { CHECK(0); return; }
    CHECK(f(&par.sd, NULL, &sd, NULL, TRUE, SEF_DACL_AUTO_INHERIT, t, &gm));
    if (sd) {
        PACL acl = sd_dacl(sd, &pr);
        ACE_HEADER *h = g2_find_ace(acl, 0x1234);
        CHECK(h != NULL);
        if (h) CHECK(h->AceFlags & INHERITED_ACE);
        CHECK(GetSecurityDescriptorControl(sd, &ctl, &rev));
        t_note("control with auto-inherit: 0x%x", ctl);
        d(&sd);
    }
    CloseHandle(t);
}
TEST(gap2_cposex_no_flag_not_marked_inherited) {
    cpose_fn f = (cpose_fn)t_proc("CreatePrivateObjectSecurityEx");
    dpos_fn d = (dpos_fn)t_proc("DestroyPrivateObjectSecurity");
    PSECURITY_DESCRIPTOR sd = NULL; sdbuf par; ace_spec a[1]; HANDLE t; BOOL pr;
    REQUIRE(f); REQUIRE(d);
    a[0].mask = 0x1234; a[0].flags = CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE; a[0].sid = EVERYONE;
    sd_make(&par, SYSTEM_SID, SYSTEM_SID, a, 1, TRUE);
    t = imp_token(); if (!t) { CHECK(0); return; }
    CHECK(f(&par.sd, NULL, &sd, NULL, TRUE, 0, t, &gm));
    if (sd) {
        ACE_HEADER *h = g2_find_ace(sd_dacl(sd, &pr), 0x1234);
        CHECK(h != NULL);
        if (h) t_note("flags without auto-inherit: 0x%x", h->AceFlags);
        d(&sd);
    }
    CloseHandle(t);
}
TEST(gap2_cposex_defaults) {
    cpose_fn f = (cpose_fn)t_proc("CreatePrivateObjectSecurityEx");
    dpos_fn d = (dpos_fn)t_proc("DestroyPrivateObjectSecurity");
    PSECURITY_DESCRIPTOR sd = NULL; HANDLE t; PSID o = NULL; BOOL def; WORD ctl; DWORD rev;
    REQUIRE(f); REQUIRE(d);
    t = imp_token(); if (!t) { CHECK(0); return; }
    CHECK(f(NULL, NULL, &sd, NULL, FALSE, 0, t, &gm));
    if (sd) {
        CHECK(IsValidSecurityDescriptor(sd));
        CHECK(GetSecurityDescriptorOwner(sd, &o, &def) && o);
        CHECK(GetSecurityDescriptorControl(sd, &ctl, &rev) && (ctl & SE_SELF_RELATIVE));
        d(&sd);
    }
    CloseHandle(t);
}
TEST(gap2_cposex_all_flag_combinations) {
    cpose_fn f = (cpose_fn)t_proc("CreatePrivateObjectSecurityEx");
    dpos_fn d = (dpos_fn)t_proc("DestroyPrivateObjectSecurity");
    HANDLE t; DWORD fl; sdbuf par; ace_spec a[1];
    REQUIRE(f); REQUIRE(d);
    a[0].mask = 0x1234; a[0].flags = CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE; a[0].sid = EVERYONE;
    sd_make(&par, SYSTEM_SID, SYSTEM_SID, a, 1, TRUE);
    t = imp_token(); if (!t) { CHECK(0); return; }
    for (fl = 0; fl < 0x80; fl++) {
        PSECURITY_DESCRIPTOR sd = NULL;
        SET_ERR(0);
        BOOL ok = f(&par.sd, NULL, &sd, NULL, TRUE, fl, t, &gm);
        if (!ok) t_note("flags 0x%lx failed err=%lu", fl, GetLastError());
        if (ok) { CHECK(IsValidSecurityDescriptor(sd)); d(&sd); }
    }
    CloseHandle(t);
}
TEST(gap2_cposex_default_owner_group_from_parent) {
    cpose_fn f = (cpose_fn)t_proc("CreatePrivateObjectSecurityEx");
    dpos_fn d = (dpos_fn)t_proc("DestroyPrivateObjectSecurity");
    PSECURITY_DESCRIPTOR sd = NULL; sdbuf par; HANDLE t; PSID o = NULL, g = NULL; BOOL def;
    BYTE b1[16];
    REQUIRE(f); REQUIRE(d);
    sd_make(&par, SYSTEM_SID, g2_mksid(b1, 1, 1, 0u), NULL, 0, FALSE);
    t = imp_token(); if (!t) { CHECK(0); return; }
    BOOL ok = f(&par.sd, NULL, &sd, NULL, FALSE, SEF_DEFAULT_OWNER_FROM_PARENT | SEF_DEFAULT_GROUP_FROM_PARENT | SEF_AVOID_OWNER_CHECK, t, &gm);
    CHECK(ok);
    if (ok) {
        GetSecurityDescriptorOwner(sd, &o, &def); GetSecurityDescriptorGroup(sd, &g, &def);
        CHECK(o && EqualSid(o, sysid));
        CHECK(g && EqualSid(g, b1));
        d(&sd);
    }
    CloseHandle(t);
}
TEST(gap2_cposex_invalid_token) {
    cpose_fn f = (cpose_fn)t_proc("CreatePrivateObjectSecurityEx");
    PSECURITY_DESCRIPTOR sd = NULL;
    REQUIRE(f);
    SET_ERR(0);
    CHECK(!f(NULL, NULL, &sd, NULL, FALSE, 0, (HANDLE)(ULONG_PTR)0x7FFFFFF0, &gm));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CHECK(sd == NULL);
}
TEST(gap2_cposex_object_type_guid) {
    cpose_fn f = (cpose_fn)t_proc("CreatePrivateObjectSecurityEx");
    dpos_fn d = (dpos_fn)t_proc("DestroyPrivateObjectSecurity");
    PSECURITY_DESCRIPTOR sd = NULL; HANDLE t;
    GUID g = { 0x12345678, 0x1234, 0x1234, { 1, 2, 3, 4, 5, 6, 7, 8 } };
    REQUIRE(f); REQUIRE(d);
    t = imp_token(); if (!t) { CHECK(0); return; }
    BOOL ok = f(NULL, NULL, &sd, &g, TRUE, SEF_DACL_AUTO_INHERIT, t, &gm);
    CHECK(ok);
    if (ok) { CHECK(IsValidSecurityDescriptor(sd)); d(&sd); }
    CloseHandle(t);
}

/* ------------- CreatePrivateObjectSecurityWithMultipleInheritance ------------- */
TEST(gap2_cposm_no_guids_matches_ex) {
    cposm_fn f = (cposm_fn)t_proc("CreatePrivateObjectSecurityWithMultipleInheritance");
    cpose_fn fe = (cpose_fn)t_proc("CreatePrivateObjectSecurityEx");
    dpos_fn d = (dpos_fn)t_proc("DestroyPrivateObjectSecurity");
    PSECURITY_DESCRIPTOR a = NULL, b = NULL; sdbuf par; ace_spec sp[1]; HANDLE t;
    REQUIRE(f); REQUIRE(fe); REQUIRE(d);
    sp[0].mask = 0x1234; sp[0].flags = CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE; sp[0].sid = EVERYONE;
    sd_make(&par, SYSTEM_SID, SYSTEM_SID, sp, 1, TRUE);
    t = imp_token(); if (!t) { CHECK(0); return; }
    CHECK(f(&par.sd, NULL, &a, NULL, 0, TRUE, SEF_DACL_AUTO_INHERIT, t, &gm));
    CHECK(fe(&par.sd, NULL, &b, NULL, TRUE, SEF_DACL_AUTO_INHERIT, t, &gm));
    if (a && b) {
        BOOL pr;
        PACL aa = sd_dacl(a, &pr), ab = sd_dacl(b, &pr);
        EXPECT_EQ(g2_ace_count(aa), g2_ace_count(ab));
        EXPECT_EQ(GetSecurityDescriptorLength(a), GetSecurityDescriptorLength(b));
    }
    if (a) d(&a);
    if (b) d(&b);
    CloseHandle(t);
}
TEST(gap2_cposm_with_guid_list) {
    cposm_fn f = (cposm_fn)t_proc("CreatePrivateObjectSecurityWithMultipleInheritance");
    dpos_fn d = (dpos_fn)t_proc("DestroyPrivateObjectSecurity");
    PSECURITY_DESCRIPTOR sd = NULL; HANDLE t;
    GUID g1 = { 0x11111111, 0x1111, 0x1111, { 1, 1, 1, 1, 1, 1, 1, 1 } };
    GUID g2 = { 0x22222222, 0x2222, 0x2222, { 2, 2, 2, 2, 2, 2, 2, 2 } };
    GUID *list[2] = { &g1, &g2 };
    REQUIRE(f); REQUIRE(d);
    t = imp_token(); if (!t) { CHECK(0); return; }
    BOOL ok = f(NULL, NULL, &sd, list, 2, TRUE, SEF_DACL_AUTO_INHERIT, t, &gm);
    CHECK(ok);
    if (ok) { CHECK(IsValidSecurityDescriptor(sd)); d(&sd); }
    CloseHandle(t);
}
TEST(gap2_cposm_invalid_token) {
    cposm_fn f = (cposm_fn)t_proc("CreatePrivateObjectSecurityWithMultipleInheritance");
    PSECURITY_DESCRIPTOR sd = NULL;
    REQUIRE(f);
    SET_ERR(0);
    CHECK(!f(NULL, NULL, &sd, NULL, 0, FALSE, 0, (HANDLE)(ULONG_PTR)0x7FFFFFF0, &gm));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CHECK(sd == NULL);
}
TEST(gap2_cposm_inherit_with_guid_acl) {
    cposm_fn f = (cposm_fn)t_proc("CreatePrivateObjectSecurityWithMultipleInheritance");
    dpos_fn d = (dpos_fn)t_proc("DestroyPrivateObjectSecurity");
    PSECURITY_DESCRIPTOR sd = NULL; sdbuf par; ace_spec sp[1]; HANDLE t; BOOL pr;
    GUID g1 = { 0x11111111, 0x1111, 0x1111, { 1, 1, 1, 1, 1, 1, 1, 1 } };
    GUID *list[1] = { &g1 };
    REQUIRE(f); REQUIRE(d);
    sp[0].mask = 0x1234; sp[0].flags = CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE; sp[0].sid = EVERYONE;
    sd_make(&par, SYSTEM_SID, SYSTEM_SID, sp, 1, TRUE);
    t = imp_token(); if (!t) { CHECK(0); return; }
    CHECK(f(&par.sd, NULL, &sd, list, 1, TRUE, SEF_DACL_AUTO_INHERIT, t, &gm));
    if (sd) {
        ACE_HEADER *h = g2_find_ace(sd_dacl(sd, &pr), 0x1234);
        CHECK(h != NULL);
        d(&sd);
    }
    CloseHandle(t);
}
