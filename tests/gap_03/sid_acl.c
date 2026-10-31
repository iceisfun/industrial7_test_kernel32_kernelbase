/* gap_03: EqualSid / EqualPrefixSid / EqualDomainSid / FindFirstFreeAce / FreeSid */
#include "t.h"
#include <stdarg.h>

typedef struct { BYTE rev, n; BYTE auth[6]; DWORD sub[15]; } G3SID;

static void g3_mk(G3SID *s, BYTE auth5, int n, ...) {
    va_list ap; memset(s, 0, sizeof *s);
    s->rev = 1; s->n = (BYTE)n; s->auth[5] = auth5;
    va_start(ap, n);
    for (int i = 0; i < n; i++) s->sub[i] = va_arg(ap, DWORD);
    va_end(ap);
}
static DWORD g3_len(const G3SID *s) { return 8 + 4 * s->n; }

/* ---- EqualSid ---- */
TEST(gap3_equalsid_identical_copies) {
    REQUIRE(U(EqualSid));
    G3SID a, b;
    g3_mk(&a, 5, 5, 21, 1, 2, 3, 500);
    memcpy(&b, &a, sizeof a);
    BOOL (WINAPI *f)(PSID, PSID) = U(EqualSid);
    SetLastError(0x1234);
    CHECK(f(&a, &b));
    EXPECT_ERR(0);           /* kernelbase clears last error unconditionally */
}
TEST(gap3_equalsid_same_pointer) {
    REQUIRE(U(EqualSid));
    G3SID a; g3_mk(&a, 5, 1, 18);
    CHECK(U(EqualSid)(&a, &a));
}
TEST(gap3_equalsid_clears_error_on_false) {
    REQUIRE(U(EqualSid));
    G3SID a, b; g3_mk(&a, 5, 1, 18); g3_mk(&b, 5, 1, 19);
    BOOL (WINAPI *f)(PSID, PSID) = U(EqualSid);
    SetLastError(0xBEEF);
    BOOL r = f(&a, &b);
    CHECK(r == FALSE);
    EXPECT_ERR(0);
}
TEST(gap3_equalsid_diff_last_subauth) {
    REQUIRE(U(EqualSid));
    G3SID a, b;
    g3_mk(&a, 5, 5, 21, 1, 2, 3, 500);
    g3_mk(&b, 5, 5, 21, 1, 2, 3, 501);
    CHECK(!U(EqualSid)(&a, &b));
    CHECK(!U(EqualSid)(&b, &a));
}
TEST(gap3_equalsid_diff_each_subauth_position) {
    REQUIRE(U(EqualSid));
    for (int pos = 0; pos < 5; pos++) {
        G3SID a, b;
        g3_mk(&a, 5, 5, 21, 1, 2, 3, 500);
        g3_mk(&b, 5, 5, 21, 1, 2, 3, 500);
        b.sub[pos] ^= 0x80000000u;
        CHECKF(!U(EqualSid)(&a, &b), "pos %d", pos);
    }
}
TEST(gap3_equalsid_diff_count) {
    REQUIRE(U(EqualSid));
    G3SID a, b;
    g3_mk(&a, 5, 2, 32, 544);
    g3_mk(&b, 5, 3, 32, 544, 0);
    CHECK(!U(EqualSid)(&a, &b));
    CHECK(!U(EqualSid)(&b, &a));
}
TEST(gap3_equalsid_diff_authority) {
    REQUIRE(U(EqualSid));
    G3SID a, b;
    g3_mk(&a, 1, 1, 0);   /* S-1-1-0 Everyone */
    g3_mk(&b, 5, 1, 0);
    CHECK(!U(EqualSid)(&a, &b));
    for (int i = 0; i < 6; i++) {
        G3SID c; g3_mk(&c, 5, 1, 18); G3SID d = c; d.auth[i] ^= 0x01;
        CHECKF(!U(EqualSid)(&c, &d), "auth byte %d", i);
    }
}
TEST(gap3_equalsid_wellknown_vs_created) {
    REQUIRE(U(EqualSid));
    REQUIRE(U(CreateWellKnownSid));
    BYTE b1[SECURITY_MAX_SID_SIZE], b2[SECURITY_MAX_SID_SIZE];
    DWORD s1 = sizeof b1, s2 = sizeof b2;
    CHECK(U(CreateWellKnownSid)(WinLocalSystemSid, NULL, b1, &s1));
    CHECK(U(CreateWellKnownSid)(WinLocalSystemSid, NULL, b2, &s2));
    CHECK(s1 == s2);
    CHECK(U(EqualSid)(b1, b2));
    G3SID sys; g3_mk(&sys, 5, 1, 18);
    CHECK(U(EqualSid)(b1, &sys));
    s2 = sizeof b2;
    CHECK(U(CreateWellKnownSid)(WinWorldSid, NULL, b2, &s2));
    CHECK(!U(EqualSid)(b1, b2));
}
TEST(gap3_equalsid_ignores_trailing_garbage) {
    REQUIRE(U(EqualSid));
    G3SID a, b; g3_mk(&a, 5, 2, 32, 544); b = a;
    memset(&b.sub[2], 0xAB, 12);           /* beyond SubAuthorityCount */
    memset(&a.sub[2], 0xCD, 12);
    CHECK(U(EqualSid)(&a, &b));
}
TEST(gap3_equalsid_max_subauthorities) {
    REQUIRE(U(EqualSid));
    G3SID a, b; g3_mk(&a, 5, 15, 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15); b = a;
    CHECK(U(EqualSid)(&a, &b));
    b.sub[14] = 99;
    CHECK(!U(EqualSid)(&a, &b));
}
TEST(gap3_equalsid_allocated_sid) {
    REQUIRE(U(EqualSid)); REQUIRE(U(FreeSid));
    SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
    PSID p1 = NULL, p2 = NULL;
    CHECK(AllocateAndInitializeSid(&nt, 2, 32, 544, 0,0,0,0,0,0, &p1));
    CHECK(AllocateAndInitializeSid(&nt, 2, 32, 544, 0,0,0,0,0,0, &p2));
    if (p1 && p2) {
        CHECK(p1 != p2);
        CHECK(U(EqualSid)(p1, p2));
        EXPECT_EQ(GetLengthSid(p1), 16);
    }
    if (p1) { PVOID r = U(FreeSid)(p1); CHECK(r == NULL); }
    if (p2) { PVOID r = U(FreeSid)(p2); CHECK(r == NULL); }
}

/* ---- EqualPrefixSid ---- */
TEST(gap3_equalprefixsid_same_domain_diff_rid) {
    REQUIRE(U(EqualPrefixSid));
    G3SID a, b;
    g3_mk(&a, 5, 5, 21, 1, 2, 3, 500);
    g3_mk(&b, 5, 5, 21, 1, 2, 3, 501);
    BOOL (WINAPI *f)(PSID, PSID) = U(EqualPrefixSid);
    SetLastError(0x77);
    CHECK(f(&a, &b));
    EXPECT_ERR(0);
}
TEST(gap3_equalprefixsid_diff_domain) {
    REQUIRE(U(EqualPrefixSid));
    G3SID a, b;
    g3_mk(&a, 5, 5, 21, 1, 2, 3, 500);
    g3_mk(&b, 5, 5, 21, 1, 2, 4, 500);
    BOOL (WINAPI *f)(PSID, PSID) = U(EqualPrefixSid);
    SetLastError(0x77);
    CHECK(!f(&a, &b));
    EXPECT_ERR(0);
}
TEST(gap3_equalprefixsid_identical) {
    REQUIRE(U(EqualPrefixSid));
    G3SID a; g3_mk(&a, 5, 3, 32, 544, 7);
    CHECK(U(EqualPrefixSid)(&a, &a));
}
TEST(gap3_equalprefixsid_diff_count) {
    REQUIRE(U(EqualPrefixSid));
    G3SID a, b;
    g3_mk(&a, 5, 5, 21, 1, 2, 3, 500);
    g3_mk(&b, 5, 4, 21, 1, 2, 3);
    CHECK(!U(EqualPrefixSid)(&a, &b));
    CHECK(!U(EqualPrefixSid)(&b, &a));
}
TEST(gap3_equalprefixsid_single_subauth) {
    REQUIRE(U(EqualPrefixSid));
    /* S-1-5-18 vs S-1-5-19: the prefix (authority only) is identical */
    G3SID a, b, c;
    g3_mk(&a, 5, 1, 18); g3_mk(&b, 5, 1, 19); g3_mk(&c, 1, 1, 0);
    CHECK(U(EqualPrefixSid)(&a, &b));
    CHECK(!U(EqualPrefixSid)(&a, &c));
}
TEST(gap3_equalprefixsid_diff_each_prefix_position) {
    REQUIRE(U(EqualPrefixSid));
    for (int pos = 0; pos < 4; pos++) {
        G3SID a, b;
        g3_mk(&a, 5, 5, 21, 1, 2, 3, 500); b = a;
        b.sub[pos] += 1;
        CHECKF(!U(EqualPrefixSid)(&a, &b), "pos %d", pos);
    }
}
TEST(gap3_equalprefixsid_vs_equalsid) {
    REQUIRE(U(EqualPrefixSid)); REQUIRE(U(EqualSid));
    G3SID a, b;
    g3_mk(&a, 5, 3, 32, 544, 5); g3_mk(&b, 5, 3, 32, 544, 6);
    CHECK(U(EqualPrefixSid)(&a, &b) && !U(EqualSid)(&a, &b));
    b = a;
    CHECK(U(EqualPrefixSid)(&a, &b) && U(EqualSid)(&a, &b));
}

/* ---- EqualDomainSid ---- */
TEST(gap3_equaldomainsid_same_domain) {
    REQUIRE(U(EqualDomainSid));
    G3SID a, b; BOOL eq = 0xFF;
    g3_mk(&a, 5, 5, 21, 1, 2, 3, 500);
    g3_mk(&b, 5, 5, 21, 1, 2, 3, 513);
    CHECK(U(EqualDomainSid)(&a, &b, &eq));
    CHECK(eq == TRUE);
}
TEST(gap3_equaldomainsid_diff_domain) {
    REQUIRE(U(EqualDomainSid));
    G3SID a, b; BOOL eq = 0xFF;
    g3_mk(&a, 5, 5, 21, 1, 2, 3, 500);
    g3_mk(&b, 5, 5, 21, 1, 2, 4, 500);
    CHECK(U(EqualDomainSid)(&a, &b, &eq));
    CHECK(eq == FALSE);
}
TEST(gap3_equaldomainsid_symmetry) {
    REQUIRE(U(EqualDomainSid));
    G3SID a, b;
    g3_mk(&a, 5, 5, 21, 10, 20, 30, 1000);
    g3_mk(&b, 5, 5, 21, 10, 20, 30, 1001);
    BOOL e1 = 0xFF, e2 = 0xFF;
    CHECK(U(EqualDomainSid)(&a, &b, &e1));
    CHECK(U(EqualDomainSid)(&b, &a, &e2));
    CHECK(e1 == e2 && e1 == TRUE);
}
TEST(gap3_equaldomainsid_self) {
    REQUIRE(U(EqualDomainSid));
    G3SID a; BOOL eq = 0xFF;
    g3_mk(&a, 5, 5, 21, 7, 8, 9, 1234);
    CHECK(U(EqualDomainSid)(&a, &a, &eq));
    CHECK(eq == TRUE);
}
static void g3_invalid_sid_case(G3SID *bad, const char *what) {
    G3SID good; BOOL eq = 0x55;
    g3_mk(&good, 5, 5, 21, 1, 2, 3, 500);
    BOOL (WINAPI *f)(PSID, PSID, BOOL *) = U(EqualDomainSid);
    SetLastError(0);
    BOOL r = f(&good, bad, &eq);
    CHECKF(r == FALSE, "%s second", what);
    EXPECT_ERR(ERROR_INVALID_SID);
    SetLastError(0);
    r = f(bad, &good, &eq);
    CHECKF(r == FALSE, "%s first", what);
    EXPECT_ERR(ERROR_INVALID_SID);
}
TEST(gap3_equaldomainsid_invalid_revision) {
    REQUIRE(U(EqualDomainSid));
    G3SID bad; g3_mk(&bad, 5, 5, 21, 1, 2, 3, 500); bad.rev = 2;
    g3_invalid_sid_case(&bad, "rev2");
    bad.rev = 0; g3_invalid_sid_case(&bad, "rev0");
}
TEST(gap3_equaldomainsid_invalid_subauth_count) {
    REQUIRE(U(EqualDomainSid));
    G3SID bad; g3_mk(&bad, 5, 5, 21, 1, 2, 3, 500);
    bad.n = 16;                       /* SID_MAX_SUB_AUTHORITIES is 15 */
    /* buffer is only 15 dwords; RtlValidSid rejects on the count alone */
    g3_invalid_sid_case(&bad, "count16");
    bad.n = 255; g3_invalid_sid_case(&bad, "count255");
}
TEST(gap3_equaldomainsid_nondomain_notes) {
    REQUIRE(U(EqualDomainSid));
    G3SID a, b; BOOL eq = 0x55;
    g3_mk(&a, 5, 2, 32, 544); g3_mk(&b, 5, 2, 32, 545);
    SetLastError(0);
    BOOL r = U(EqualDomainSid)(&a, &b, &eq);
    t_note("builtin SIDs: ret=%d eq=%d err=%lu", r, eq, GetLastError());
    g3_mk(&a, 1, 1, 0); g3_mk(&b, 5, 1, 18); eq = 0x55; SetLastError(0);
    r = U(EqualDomainSid)(&a, &b, &eq);
    t_note("everyone vs system: ret=%d eq=%d err=%lu", r, eq, GetLastError());
}

/* ---- FindFirstFreeAce ---- */
static ACL *g3_acl(BYTE *buf, DWORD size) {
    memset(buf, 0xCC, size);
    ACL *a = (ACL *)buf;
    if (!InitializeAcl(a, size, ACL_REVISION)) return NULL;
    return a;
}
TEST(gap3_ffa_empty_acl) {
    REQUIRE(U(FindFirstFreeAce));
    BYTE buf[128] __attribute__((aligned(8)));
    ACL *a = g3_acl(buf, sizeof buf);
    CHECK(a != NULL);
    LPVOID p = (LPVOID)1;
    BOOL (WINAPI *f)(PACL, LPVOID *) = U(FindFirstFreeAce);
    SetLastError(0x4321);
    CHECK(f(a, &p));
    CHECK(p == (LPVOID)(buf + sizeof(ACL)));
    EXPECT_ERR(0x4321);         /* success path does not touch last error */
}
TEST(gap3_ffa_after_one_ace) {
    REQUIRE(U(FindFirstFreeAce));
    BYTE buf[256] __attribute__((aligned(8)));
    ACL *a = g3_acl(buf, sizeof buf);
    G3SID s; g3_mk(&s, 1, 1, 0);
    CHECK(AddAccessAllowedAce(a, ACL_REVISION, GENERIC_ALL, &s));
    LPVOID p = NULL;
    CHECK(U(FindFirstFreeAce)(a, &p));
    DWORD aceSize = 8 + g3_len(&s) - 4 + 0;   /* ACE header + mask + SID */
    aceSize = 4 + 4 + g3_len(&s);
    CHECK(p == (LPVOID)(buf + 8 + aceSize));
    /* walking via GetAce agrees */
    LPVOID ace0 = NULL;
    CHECK(GetAce(a, 0, &ace0));
    CHECK((BYTE *)p == (BYTE *)ace0 + ((ACE_HEADER *)ace0)->AceSize);
}
TEST(gap3_ffa_after_many_aces) {
    REQUIRE(U(FindFirstFreeAce));
    BYTE buf[1024] __attribute__((aligned(8)));
    ACL *a = g3_acl(buf, sizeof buf);
    G3SID s;
    for (int i = 0; i < 10; i++) {
        g3_mk(&s, 5, 2, 32, 500 + i);
        CHECK(AddAccessAllowedAce(a, ACL_REVISION, 1u << i, &s));
        LPVOID p = NULL;
        CHECK(U(FindFirstFreeAce)(a, &p));
        LPVOID last = NULL;
        CHECK(GetAce(a, i, &last));
        CHECK((BYTE *)p == (BYTE *)last + ((ACE_HEADER *)last)->AceSize);
    }
}
TEST(gap3_ffa_full_acl_points_to_end) {
    REQUIRE(U(FindFirstFreeAce));
    G3SID s; g3_mk(&s, 1, 1, 0);
    DWORD need = 8 + 8 + g3_len(&s);
    BYTE buf[64] __attribute__((aligned(8)));
    ACL *a = g3_acl(buf, need);
    CHECK(AddAccessAllowedAce(a, ACL_REVISION, 1, &s));
    LPVOID p = NULL;
    CHECK(U(FindFirstFreeAce)(a, &p));
    CHECK(p == (LPVOID)(buf + need));
}
TEST(gap3_ffa_header_only_acl) {
    REQUIRE(U(FindFirstFreeAce));
    BYTE buf[16] __attribute__((aligned(8)));
    ACL *a = g3_acl(buf, 8);                   /* exactly the header */
    CHECK(a != NULL);
    LPVOID p = NULL;
    CHECK(U(FindFirstFreeAce)(a, &p));
    CHECK(p == (LPVOID)(buf + 8));
}
TEST(gap3_ffa_ace_count_exceeds_size_fails) {
    REQUIRE(U(FindFirstFreeAce));
    BYTE buf[64] __attribute__((aligned(8))); memset(buf, 0, sizeof buf);
    ACL *a = (ACL *)buf;
    a->AclRevision = ACL_REVISION; a->AclSize = 8; a->AceCount = 1;   /* claims an ACE but has no room */
    LPVOID p = (LPVOID)0x1234;
    BOOL (WINAPI *f)(PACL, LPVOID *) = U(FindFirstFreeAce);
    SetLastError(0);
    BOOL r = f(a, &p);
    CHECK(r == FALSE);
    EXPECT_ERR(ERROR_INVALID_ACL);
    CHECK(p == NULL);
}
TEST(gap3_ffa_ace_overruns_acl) {
    REQUIRE(U(FindFirstFreeAce));
    BYTE buf[64] __attribute__((aligned(8))); memset(buf, 0, sizeof buf);
    ACL *a = (ACL *)buf;
    a->AclRevision = ACL_REVISION; a->AclSize = 16; a->AceCount = 1;
    ACE_HEADER *h = (ACE_HEADER *)(buf + 8);
    h->AceType = ACCESS_ALLOWED_ACE_TYPE; h->AceSize = 0x20;   /* extends past AclSize */
    LPVOID p = (LPVOID)0x1234;
    SetLastError(0);
    BOOL r = U(FindFirstFreeAce)(a, &p);
    CHECK(r == TRUE);
    CHECK(p == NULL);
}
TEST(gap3_ffa_second_ace_beyond_end_fails) {
    REQUIRE(U(FindFirstFreeAce));
    BYTE buf[64] __attribute__((aligned(8))); memset(buf, 0, sizeof buf);
    ACL *a = (ACL *)buf;
    a->AclRevision = ACL_REVISION; a->AclSize = 16; a->AceCount = 3;
    ACE_HEADER *h = (ACE_HEADER *)(buf + 8);
    h->AceType = ACCESS_ALLOWED_ACE_TYPE; h->AceSize = 8;      /* second ACE starts at AclSize */
    LPVOID p = (LPVOID)0x1234;
    BOOL (WINAPI *f)(PACL, LPVOID *) = U(FindFirstFreeAce);
    SetLastError(0);
    BOOL r = f(a, &p);
    CHECK(r == FALSE);
    EXPECT_ERR(ERROR_INVALID_ACL);
}
TEST(gap3_ffa_zero_count_ignores_revision) {
    REQUIRE(U(FindFirstFreeAce));
    BYTE buf[32] __attribute__((aligned(8))); memset(buf, 0, sizeof buf);
    ACL *a = (ACL *)buf; a->AclRevision = 0x7F; a->AclSize = 32; a->AceCount = 0;
    LPVOID p = NULL;
    t_note("bad revision walk: %d", U(FindFirstFreeAce)(a, &p));
}
TEST(gap3_ffa_repeatable) {
    REQUIRE(U(FindFirstFreeAce));
    BYTE buf[256] __attribute__((aligned(8)));
    ACL *a = g3_acl(buf, sizeof buf);
    G3SID s; g3_mk(&s, 1, 1, 0);
    AddAccessAllowedAce(a, ACL_REVISION, 1, &s);
    LPVOID first = NULL;
    CHECK(U(FindFirstFreeAce)(a, &first));
    for (int i = 0; i < 100; i++) {
        LPVOID p = NULL;
        CHECK(U(FindFirstFreeAce)(a, &p));
        CHECK(p == first);
    }
}
TEST(gap3_ffa_deny_and_audit_aces) {
    REQUIRE(U(FindFirstFreeAce));
    BYTE buf[512] __attribute__((aligned(8)));
    ACL *a = g3_acl(buf, sizeof buf);
    G3SID s; g3_mk(&s, 5, 1, 18);
    CHECK(AddAccessDeniedAce(a, ACL_REVISION, 1, &s));
    CHECK(AddAccessAllowedAceEx(a, ACL_REVISION, CONTAINER_INHERIT_ACE, 2, &s));
    LPVOID p = NULL, last = NULL;
    CHECK(U(FindFirstFreeAce)(a, &p));
    CHECK(GetAce(a, 1, &last));
    CHECK((BYTE *)p == (BYTE *)last + ((ACE_HEADER *)last)->AceSize);
    CHECK(a->AceCount == 2);
}

/* ---- FreeSid ---- */
TEST(gap3_freesid_returns_null) {
    REQUIRE(U(FreeSid));
    SID_IDENTIFIER_AUTHORITY wa = SECURITY_WORLD_SID_AUTHORITY;
    for (int i = 0; i < 200; i++) {
        PSID p = NULL;
        CHECK(AllocateAndInitializeSid(&wa, 1, 0, 0,0,0,0,0,0,0, &p));
        PVOID r = U(FreeSid)(p);
        CHECK(r == NULL);
    }
}
TEST(gap3_freesid_null_is_harmless) {
    REQUIRE(U(FreeSid));
    PVOID r = U(FreeSid)(NULL);
    CHECK(r == NULL);
}
TEST(gap3_freesid_all_subauth_counts) {
    REQUIRE(U(FreeSid));
    SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
    for (BYTE n = 1; n <= 8; n++) {
        PSID p = NULL;
        CHECK(AllocateAndInitializeSid(&nt, n, 1,2,3,4,5,6,7,8, &p));
        if (p) {
            CHECK(*GetSidSubAuthorityCount(p) == n);
            CHECK(GetLengthSid(p) == 8u + 4u * n);
            CHECK(U(FreeSid)(p) == NULL);
        }
    }
}
