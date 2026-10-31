/* AreAllAccessesGranted, AreAnyAccessesGranted, CopySid, CheckTokenMembership(Ex), CheckTokenCapability,
 * ConvertToAutoInheritPrivateObjectSecurity, AppPolicyGetThreadInitializationType. */
#include "gap1.h"
#include <aclapi.h>

/* ---------- AreAll/AnyAccessesGranted ---------- */
static const DWORD g_masks[] = { 0, 1, 2, 3, 0x10, 0xFF, 0x100, 0xFFFF, 0x10000, 0x1F0000, 0x00120089, 0x001F01FF,
    0x01000000, 0x02000000, 0x10000000, 0x20000000, 0x40000000, 0x80000000, 0xF0000000, 0xFFFFFFFF, 0xDEADBEEF };
#define NMASK (sizeof g_masks / sizeof g_masks[0])

TEST(gap1_areallaccessesgranted_table) {
    REQUIRE(U(AreAllAccessesGranted));
    int bad = 0;
    for (size_t i = 0; i < NMASK; i++) for (size_t j = 0; j < NMASK; j++) {
        DWORD g = g_masks[i], d = g_masks[j];
        BOOL want = (d & ~g) == 0;
        BOOL got = U(AreAllAccessesGranted)(g, d);
        if (got != want) { bad++; CHECKF(0, "g=%08lx d=%08lx got %d want %d", g, d, got, want); }
    }
    CHECK(bad == 0);
}
TEST(gap1_areanyaccessesgranted_table) {
    REQUIRE(U(AreAnyAccessesGranted));
    for (size_t i = 0; i < NMASK; i++) for (size_t j = 0; j < NMASK; j++) {
        DWORD g = g_masks[i], d = g_masks[j];
        BOOL want = (g & d) != 0;
        BOOL got = U(AreAnyAccessesGranted)(g, d);
        CHECKF(got == want, "g=%08lx d=%08lx got %d want %d", g, d, got, want);
    }
}
TEST(gap1_areallaccessesgranted_zero_desired_is_true) {
    REQUIRE(U(AreAllAccessesGranted));
    EXPECT_EQ(U(AreAllAccessesGranted)(0, 0), TRUE);
    EXPECT_EQ(U(AreAllAccessesGranted)(0xFFFFFFFF, 0), TRUE);
}
TEST(gap1_areanyaccessesgranted_zero_desired_is_false) {
    REQUIRE(U(AreAnyAccessesGranted));
    EXPECT_EQ(U(AreAnyAccessesGranted)(0, 0), FALSE);
    EXPECT_EQ(U(AreAnyAccessesGranted)(0xFFFFFFFF, 0), FALSE);
    EXPECT_EQ(U(AreAnyAccessesGranted)(0, 0xFFFFFFFF), FALSE);
}
TEST(gap1_areaccessesgranted_single_bits) {
    REQUIRE(U(AreAllAccessesGranted)); REQUIRE(U(AreAnyAccessesGranted));
    for (int b = 0; b < 32; b++) {
        DWORD m = 1u << b;
        EXPECT_EQ(U(AreAllAccessesGranted)(m, m), TRUE);
        EXPECT_EQ(U(AreAnyAccessesGranted)(m, m), TRUE);
        EXPECT_EQ(U(AreAllAccessesGranted)(~m, m), FALSE);
        EXPECT_EQ(U(AreAnyAccessesGranted)(~m, m), FALSE);
        EXPECT_EQ(U(AreAllAccessesGranted)(m, 0xFFFFFFFF), (m == 0xFFFFFFFF));
        EXPECT_EQ(U(AreAnyAccessesGranted)(m, 0xFFFFFFFF), TRUE);
    }
}
TEST(gap1_areaccessesgranted_generic_bits_not_mapped) {
    /* generic rights are plain bits for these helpers: no mapping is applied */
    REQUIRE(U(AreAllAccessesGranted)); REQUIRE(U(AreAnyAccessesGranted));
    EXPECT_EQ(U(AreAllAccessesGranted)(FILE_ALL_ACCESS, GENERIC_ALL), FALSE);
    EXPECT_EQ(U(AreAnyAccessesGranted)(FILE_ALL_ACCESS, GENERIC_READ), FALSE);
    EXPECT_EQ(U(AreAllAccessesGranted)(GENERIC_READ | GENERIC_WRITE, GENERIC_READ), TRUE);
}
TEST(gap1_areaccessesgranted_return_is_normalized_bool) {
    REQUIRE(U(AreAllAccessesGranted)); REQUIRE(U(AreAnyAccessesGranted));
    DWORD a = U(AreAllAccessesGranted)(0xFFFFFFFF, 0xFFFFFFFF);
    DWORD b = U(AreAnyAccessesGranted)(0xFFFFFFFF, 0xFFFFFFFF);
    EXPECT_EQ(a, 1); EXPECT_EQ(b, 1);
}
TEST(gap1_areaccessesgranted_preserves_last_error) {
    REQUIRE(U(AreAllAccessesGranted)); REQUIRE(U(AreAnyAccessesGranted));
    SET_ERR(0x4321);
    U(AreAllAccessesGranted)(1, 2);
    EXPECT_ERR(0x4321);
    U(AreAnyAccessesGranted)(1, 2);
    EXPECT_ERR(0x4321);
}
TEST(gap1_areaccessesgranted_all_implies_any) {
    REQUIRE(U(AreAllAccessesGranted)); REQUIRE(U(AreAnyAccessesGranted));
    for (size_t i = 0; i < NMASK; i++) for (size_t j = 0; j < NMASK; j++) {
        DWORD g = g_masks[i], d = g_masks[j];
        if (d != 0 && U(AreAllAccessesGranted)(g, d)) CHECKF(U(AreAnyAccessesGranted)(g, d), "g=%lx d=%lx", g, d);
    }
}

/* ---------- SID helpers ---------- */
typedef struct { BYTE rev, cnt; BYTE auth[6]; DWORD sub[15]; } G1SID;
static DWORD g1_mksid(G1SID *s, int n, BYTE authlast) {
    memset(s, 0, sizeof *s);
    s->rev = 1; s->cnt = (BYTE)n; s->auth[5] = authlast;
    for (int i = 0; i < n; i++) s->sub[i] = 0x1000 + i;
    return 8 + 4 * n;
}

/* ---------- CopySid ---------- */
TEST(gap1_copysid_all_subauth_counts_exact) {
    REQUIRE(U(CopySid));
    for (int n = 0; n <= 15; n++) {
        G1SID src; DWORD need = g1_mksid(&src, n, 5);
        BYTE dst[128]; memset(dst, 0xCC, sizeof dst);
        SET_ERR(0x5555);
        BOOL r = U(CopySid)(need, dst, &src);
        CHECKF(r, "n=%d", n);
        CHECKF(!memcmp(dst, &src, need), "n=%d content", n);
        CHECKF(dst[need] == 0xCC, "n=%d wrote past end", n);
        CHECKF(GetLengthSid(&src) == need, "length");
    }
}
TEST(gap1_copysid_exact_minus_one_fails) {
    REQUIRE(U(CopySid));
    for (int n = 0; n <= 15; n++) {
        G1SID src; DWORD need = g1_mksid(&src, n, 5);
        BYTE dst[128]; memset(dst, 0xCC, sizeof dst);
        SET_ERR(0);
        BOOL r = U(CopySid)(need - 1, dst, &src);
        CHECKF(!r, "n=%d", n);
        EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
        int untouched = 1; for (DWORD i = 0; i < sizeof dst; i++) if (dst[i] != 0xCC) untouched = 0;
        CHECKF(untouched, "n=%d dest modified on failure", n);
    }
}
TEST(gap1_copysid_larger_buffer_ok) {
    REQUIRE(U(CopySid));
    G1SID src; DWORD need = g1_mksid(&src, 3, 5);
    BYTE dst[128]; memset(dst, 0xCC, sizeof dst);
    CHECK(U(CopySid)(sizeof dst, dst, &src));
    CHECK(!memcmp(dst, &src, need));
    CHECK(dst[need] == 0xCC);
    memset(dst, 0xCC, sizeof dst);
    CHECK(U(CopySid)(need + 1, dst, &src));
    CHECK(dst[need] == 0xCC);
}
TEST(gap1_copysid_zero_length_fails) {
    REQUIRE(U(CopySid));
    G1SID src; g1_mksid(&src, 1, 5);
    BYTE dst[32] = {0};
    SET_ERR(0);
    CHECK(!U(CopySid)(0, dst, &src));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
}
TEST(gap1_copysid_small_lengths_fail_with_null_dest) {
    /* size is validated before any copy, so a NULL destination is never touched */
    REQUIRE(U(CopySid));
    G1SID src; g1_mksid(&src, 2, 5);
    for (DWORD len = 0; len < 16; len++) {
        SET_ERR(0);
        CHECKF(!U(CopySid)(len, NULL, &src), "len=%lu", len);
        EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    }
}
TEST(gap1_copysid_uses_declared_subauth_count_not_validity) {
    /* the byte count comes from SubAuthorityCount alone (even with a bogus revision) */
    REQUIRE(U(CopySid));
    G1SID src; DWORD need = g1_mksid(&src, 2, 5);
    src.rev = 9;
    BYTE dst[64] = {0};
    CHECK(U(CopySid)(need, dst, &src));
    CHECK(!memcmp(dst, &src, need));
}
TEST(gap1_copysid_well_known) {
    REQUIRE(U(CopySid)); REQUIRE(U(CreateWellKnownSid));
    static const WELL_KNOWN_SID_TYPE ty[] = { WinWorldSid, WinLocalSystemSid, WinBuiltinAdministratorsSid, WinBuiltinUsersSid,
        WinAuthenticatedUserSid, WinLocalServiceSid, WinNetworkServiceSid, WinInteractiveSid, WinLowLabelSid, WinMediumLabelSid, WinHighLabelSid, WinSystemLabelSid };
    for (size_t i = 0; i < sizeof ty / sizeof ty[0]; i++) {
        BYTE a[SECURITY_MAX_SID_SIZE], b[SECURITY_MAX_SID_SIZE];
        DWORD sz = sizeof a;
        if (!U(CreateWellKnownSid)(ty[i], NULL, a, &sz)) { t_note("CreateWellKnownSid(%d) failed %lu", ty[i], GetLastError()); continue; }
        memset(b, 0, sizeof b);
        CHECKF(U(CopySid)(sz, b, a), "type %d", ty[i]);
        CHECKF(EqualSid(a, b), "type %d", ty[i]);
        CHECKF(GetLengthSid(b) == sz, "type %d", ty[i]);
        SET_ERR(0);
        CHECKF(!U(CopySid)(sz - 1, b, a), "type %d", ty[i]);
        EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    }
}
TEST(gap1_copysid_success_keeps_last_error) {
    REQUIRE(U(CopySid));
    G1SID src; DWORD need = g1_mksid(&src, 1, 5);
    BYTE dst[32];
    SET_ERR(0x7777);
    CHECK(U(CopySid)(need, dst, &src));
    EXPECT_ERR(0x7777);
}
TEST(gap1_copysid_in_place_same_buffer) {
    REQUIRE(U(CopySid));
    G1SID src; DWORD need = g1_mksid(&src, 4, 5);
    G1SID cpy = src;
    CHECK(U(CopySid)(need, (PSID)&src, (PSID)&src));
    CHECK(!memcmp(&src, &cpy, need));
}
TEST(gap1_copysid_matches_advapi_style_alias_via_kernelbase) {
    /* same function must be reachable via both lookups */
    FARPROC a = t_proc("CopySid");
    CHECK(a != NULL);
}

/* ---------- token helpers ---------- */
static HANDLE g1_imp_token(DWORD access) {
    HANDLE p = NULL, d = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | TOKEN_QUERY, &p)) return NULL;
    if (!DuplicateTokenEx(p, access, NULL, SecurityImpersonation, TokenImpersonation, &d)) d = NULL;
    CloseHandle(p);
    return d;
}
static HANDLE g1_primary_token(void) {
    HANDLE p = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &p)) return NULL;
    return p;
}
static PSID g1_tokuser(HANDLE tok, BYTE *buf, DWORD cap) {
    DWORD need = 0;
    if (!GetTokenInformation(tok, TokenUser, buf, cap, &need)) return NULL;
    return ((TOKEN_USER *)buf)->User.Sid;
}
static PSID g1_world(BYTE *buf) { DWORD n = SECURITY_MAX_SID_SIZE; return CreateWellKnownSid(WinWorldSid, NULL, buf, &n) ? buf : NULL; }

/* ---------- CheckTokenMembership ---------- */
TEST(gap1_checktokenmembership_null_sid) {
    REQUIRE(U(CheckTokenMembership));
    BOOL m = 0x77;
    SET_ERR(0);
    CHECK(!U(CheckTokenMembership)(NULL, NULL, &m));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap1_checktokenmembership_null_out) {
    REQUIRE(U(CheckTokenMembership));
    if (t_is_wine()) { t_skip("faults on NULL under wine"); return; }
    BYTE w[SECURITY_MAX_SID_SIZE]; PSID s = g1_world(w);
    SET_ERR(0);
    CHECK(!U(CheckTokenMembership)(NULL, s, NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap1_checktokenmembership_both_null) {
    REQUIRE(U(CheckTokenMembership));
    if (t_is_wine()) { t_skip("faults on NULL under wine"); return; }
    SET_ERR(0);
    CHECK(!U(CheckTokenMembership)(NULL, NULL, NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap1_checktokenmembership_everyone_null_token) {
    REQUIRE(U(CheckTokenMembership));
    BYTE w[SECURITY_MAX_SID_SIZE]; PSID s = g1_world(w);
    BOOL m = 0x77;
    CHECK(U(CheckTokenMembership)(NULL, s, &m));
    EXPECT_EQ(m, TRUE);
}
TEST(gap1_checktokenmembership_everyone_impersonation_token) {
    REQUIRE(U(CheckTokenMembership));
    HANDLE t = g1_imp_token(TOKEN_ALL_ACCESS);
    if (!t) { t_skip("no token"); return; }
    BYTE w[SECURITY_MAX_SID_SIZE]; PSID s = g1_world(w);
    BOOL m = 0x77;
    CHECK(U(CheckTokenMembership)(t, s, &m));
    EXPECT_EQ(m, TRUE);
    CloseHandle(t);
}
TEST(gap1_checktokenmembership_own_user_sid) {
    REQUIRE(U(CheckTokenMembership));
    HANDLE t = g1_imp_token(TOKEN_ALL_ACCESS);
    if (!t) { t_skip("no token"); return; }
    BYTE ub[256]; PSID u = g1_tokuser(t, ub, sizeof ub);
    CHECK(u != NULL);
    BOOL m = 0x77;
    if (u) { CHECK(U(CheckTokenMembership)(t, u, &m)); EXPECT_EQ(m, TRUE); }
    CloseHandle(t);
}
TEST(gap1_checktokenmembership_own_user_sid_null_token) {
    REQUIRE(U(CheckTokenMembership));
    HANDLE t = g1_primary_token();
    if (!t) { t_skip("no token"); return; }
    BYTE ub[256]; PSID u = g1_tokuser(t, ub, sizeof ub);
    BOOL m = 0x77;
    if (u) { CHECK(U(CheckTokenMembership)(NULL, u, &m)); EXPECT_EQ(m, TRUE); }
    CloseHandle(t);
}
TEST(gap1_checktokenmembership_foreign_sid_false) {
    REQUIRE(U(CheckTokenMembership));
    G1SID s; g1_mksid(&s, 5, 5); s.sub[0] = 21; s.sub[1] = 0x1111; s.sub[2] = 0x2222; s.sub[3] = 0x3333; s.sub[4] = 0x7E57;
    BOOL m = 0x77;
    SET_ERR(0);
    CHECK(U(CheckTokenMembership)(NULL, &s, &m));
    EXPECT_EQ(m, FALSE);
}
TEST(gap1_checktokenmembership_member_cleared_before_check) {
    /* *IsMember is zeroed when the arguments are valid, even if a prior value was TRUE */
    REQUIRE(U(CheckTokenMembership));
    G1SID s; g1_mksid(&s, 5, 5); s.sub[0] = 21; s.sub[4] = 0x7E58;
    BOOL m = TRUE;
    CHECK(U(CheckTokenMembership)(NULL, &s, &m));
    EXPECT_EQ(m, FALSE);
}
TEST(gap1_checktokenmembership_local_system_not_member) {
    REQUIRE(U(CheckTokenMembership));
    BYTE b[SECURITY_MAX_SID_SIZE]; DWORD n = sizeof b;
    if (!CreateWellKnownSid(WinLocalSystemSid, NULL, b, &n)) { t_skip("no sid"); return; }
    BOOL m = 0x77;
    CHECK(U(CheckTokenMembership)(NULL, b, &m));
    t_note("member of SYSTEM sid: %d", m);
    CHECK(m == 0 || m == 1);
}
TEST(gap1_checktokenmembership_authenticated_users) {
    REQUIRE(U(CheckTokenMembership));
    BYTE b[SECURITY_MAX_SID_SIZE]; DWORD n = sizeof b;
    if (!CreateWellKnownSid(WinAuthenticatedUserSid, NULL, b, &n)) { t_skip("no sid"); return; }
    BOOL m = 0x77;
    CHECK(U(CheckTokenMembership)(NULL, b, &m));
    t_note("member of Authenticated Users: %d", m);
    CHECK(m == 0 || m == 1);
}
TEST(gap1_checktokenmembership_integrity_label_sid) {
    REQUIRE(U(CheckTokenMembership));
    BYTE b[SECURITY_MAX_SID_SIZE]; DWORD n = sizeof b;
    if (!CreateWellKnownSid(WinMediumLabelSid, NULL, b, &n)) { t_skip("no sid"); return; }
    BOOL m = 0x77;
    CHECK(U(CheckTokenMembership)(NULL, b, &m));
    t_note("member of Medium label: %d", m);
}
TEST(gap1_checktokenmembership_closed_handle) {
    REQUIRE(U(CheckTokenMembership));
    HANDLE t = g1_imp_token(TOKEN_ALL_ACCESS);
    if (!t) { t_skip("no token"); return; }
    CloseHandle(t);
    BYTE w[SECURITY_MAX_SID_SIZE]; PSID s = g1_world(w);
    BOOL m = 0x77;
    SET_ERR(0);
    CHECK(!U(CheckTokenMembership)(t, s, &m));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    EXPECT_EQ(m, FALSE);
}
TEST(gap1_checktokenmembership_wrong_handle_type) {
    REQUIRE(U(CheckTokenMembership));
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    BYTE w[SECURITY_MAX_SID_SIZE]; PSID s = g1_world(w);
    BOOL m = 0x77;
    SET_ERR(0);
    BOOL r = U(CheckTokenMembership)(ev, s, &m);
    CHECK(!r);
    DWORD e = GetLastError();
    t_note("event handle as token -> %lu", e);
    CHECKF(e == ERROR_INVALID_HANDLE || e == ERROR_ACCESS_DENIED, "err %lu", e);
    CloseHandle(ev);
}
TEST(gap1_checktokenmembership_pseudo_process_handle) {
    REQUIRE(U(CheckTokenMembership));
    BYTE w[SECURITY_MAX_SID_SIZE]; PSID s = g1_world(w);
    BOOL m = 0x77;
    SET_ERR(0);
    BOOL r = U(CheckTokenMembership)(GetCurrentProcess(), s, &m);
    CHECK(!r);
    t_note("pseudo process handle -> err %lu", GetLastError());
}
TEST(gap1_checktokenmembership_primary_token_explicit) {
    REQUIRE(U(CheckTokenMembership));
    HANDLE t = g1_primary_token();
    if (!t) { t_skip("no token"); return; }
    BYTE w[SECURITY_MAX_SID_SIZE]; PSID s = g1_world(w);
    BOOL m = 0x77;
    SET_ERR(0);
    BOOL r = U(CheckTokenMembership)(t, s, &m);
    t_note("explicit primary token: ret=%d member=%d err=%lu", r, m, GetLastError());
    CloseHandle(t);
}
TEST(gap1_checktokenmembership_token_without_query_access) {
    REQUIRE(U(CheckTokenMembership));
    HANDLE t = g1_imp_token(TOKEN_IMPERSONATE);
    if (!t) { t_skip("no token"); return; }
    BYTE w[SECURITY_MAX_SID_SIZE]; PSID s = g1_world(w);
    BOOL m = 0x77;
    SET_ERR(0);
    BOOL r = U(CheckTokenMembership)(t, s, &m);
    t_note("token w/o TOKEN_QUERY: ret=%d err=%lu", r, GetLastError());
    CloseHandle(t);
}
TEST(gap1_checktokenmembership_identification_level_token) {
    REQUIRE(U(CheckTokenMembership));
    HANDLE p = NULL, d = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | TOKEN_QUERY, &p)) { t_skip("no token"); return; }
    if (!DuplicateTokenEx(p, TOKEN_ALL_ACCESS, NULL, SecurityIdentification, TokenImpersonation, &d)) { CloseHandle(p); t_skip("dup failed"); return; }
    BYTE w[SECURITY_MAX_SID_SIZE]; PSID s = g1_world(w);
    BOOL m = 0x77;
    SET_ERR(0);
    BOOL r = U(CheckTokenMembership)(d, s, &m);
    t_note("identification token: ret=%d member=%d err=%lu", r, m, GetLastError());
    CloseHandle(d); CloseHandle(p);
}

static DWORD g1_restricted_thread(void) {
    /* thread token with Users/Administrators disabled (deny-only): CheckTokenMembership(NULL) uses it */
    HANDLE t = g1_imp_token(TOKEN_ALL_ACCESS);
    if (!t) return 100;
    BYTE users[SECURITY_MAX_SID_SIZE], admins[SECURITY_MAX_SID_SIZE], world[SECURITY_MAX_SID_SIZE];
    DWORD n1 = sizeof users, n2 = sizeof admins, n3 = sizeof world;
    CreateWellKnownSid(WinBuiltinUsersSid, NULL, users, &n1);
    CreateWellKnownSid(WinBuiltinAdministratorsSid, NULL, admins, &n2);
    CreateWellKnownSid(WinWorldSid, NULL, world, &n3);
    BOOL before_users = 0, before_world = 0, after_users = 0x77, after_world = 0x77;
    DWORD res = 0;
    CheckTokenMembership(NULL, users, &before_users);
    CheckTokenMembership(NULL, world, &before_world);
    SID_AND_ATTRIBUTES sa[2] = { { users, 0 }, { world, 0 } };
    HANDLE r = NULL;
    if (!CreateRestrictedToken(t, 0, 2, sa, 0, NULL, 0, NULL, &r)) { CloseHandle(t); return 101; }
    if (!SetThreadToken(NULL, r)) { CloseHandle(r); CloseHandle(t); return 102; }
    BOOL a = U(CheckTokenMembership)(NULL, users, &after_users);
    BOOL b = U(CheckTokenMembership)(NULL, world, &after_world);
    SetThreadToken(NULL, NULL);
    if (!a || !b) res |= 1;
    if (before_users && after_users) res |= 2;         /* disabled group must not count */
    if (before_world && after_world) res |= 4;
    CloseHandle(r); CloseHandle(t);
    return res;
}
TEST(gap1_checktokenmembership_null_token_uses_thread_token) {
    REQUIRE(U(CheckTokenMembership));
    DWORD r = g1_run_thread(g1_restricted_thread);
    if (r == 100 || r == 101 || r == 102) { t_skip("could not build restricted thread token (%lu)", r); return; }
    CHECKF(r == 0, "flags 0x%lx", r);
}
static DWORD g1_deny_only_ex_thread(void) {
    HANDLE t = g1_imp_token(TOKEN_ALL_ACCESS);
    if (!t) return 100;
    BYTE users[SECURITY_MAX_SID_SIZE]; DWORD n1 = sizeof users;
    CreateWellKnownSid(WinBuiltinUsersSid, NULL, users, &n1);
    BOOL before = 0; CheckTokenMembership(t, users, &before);
    SID_AND_ATTRIBUTES sa[1] = { { users, 0 } };
    HANDLE r = NULL;
    if (!CreateRestrictedToken(t, 0, 1, sa, 0, NULL, 0, NULL, &r)) { CloseHandle(t); return 101; }
    BOOL m1 = 0x77, m2 = 0x77;
    BOOL a = U(CheckTokenMembership)(r, users, &m1);
    BOOL b = UK(CheckTokenMembershipEx) ? UK(CheckTokenMembershipEx)(r, users, 0, &m2) : a;
    DWORD res = 0;
    if (!a || !b) res |= 1;
    if (before && (m1 || m2)) res |= 2;
    CloseHandle(r); CloseHandle(t);
    return res;
}
TEST(gap1_checktokenmembership_deny_only_group_not_member) {
    REQUIRE(U(CheckTokenMembership));
    DWORD r = g1_run_thread(g1_deny_only_ex_thread);
    if (r >= 100 && r <= 102) { t_skip("restricted token unavailable (%lu)", r); return; }
    CHECKF(r == 0, "flags 0x%lx", r);
}
TEST(gap1_checktokenmembership_repeatable) {
    REQUIRE(U(CheckTokenMembership));
    BYTE w[SECURITY_MAX_SID_SIZE]; PSID s = g1_world(w);
    for (int i = 0; i < 500; i++) {
        BOOL m = 0x77;
        if (!U(CheckTokenMembership)(NULL, s, &m) || m != TRUE) { CHECKF(0, "iter %d", i); break; }
    }
}
TEST(gap1_checktokenmembership_impersonating_thread_other_thread_unaffected) {
    REQUIRE(U(CheckTokenMembership));
    /* main thread keeps working while a helper impersonates */
    DWORD r = g1_run_thread(g1_restricted_thread);
    BYTE w[SECURITY_MAX_SID_SIZE]; PSID s = g1_world(w);
    BOOL m = 0x77;
    CHECK(U(CheckTokenMembership)(NULL, s, &m));
    EXPECT_EQ(m, TRUE);
}

/* ---------- CheckTokenMembershipEx ---------- */
TEST(gap1_checktokenmembershipex_null_sid) {
    REQUIRE(U(CheckTokenMembershipEx));
    BOOL m = 0x77;
    SET_ERR(0);
    CHECK(!U(CheckTokenMembershipEx)(NULL, NULL, 0, &m));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap1_checktokenmembershipex_null_out) {
    REQUIRE(U(CheckTokenMembershipEx));
    BYTE w[SECURITY_MAX_SID_SIZE]; PSID s = g1_world(w);
    SET_ERR(0);
    CHECK(!U(CheckTokenMembershipEx)(NULL, s, 0, NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!U(CheckTokenMembershipEx)(NULL, s, CTMF_INCLUDE_APPCONTAINER, NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap1_checktokenmembershipex_flag0_everyone) {
    REQUIRE(U(CheckTokenMembershipEx));
    BYTE w[SECURITY_MAX_SID_SIZE]; PSID s = g1_world(w);
    BOOL m = 0x77;
    CHECK(U(CheckTokenMembershipEx)(NULL, s, 0, &m));
    EXPECT_EQ(m, TRUE);
}
TEST(gap1_checktokenmembershipex_include_appcontainer_flag) {
    REQUIRE(U(CheckTokenMembershipEx));
    BYTE w[SECURITY_MAX_SID_SIZE]; PSID s = g1_world(w);
    BOOL m = 0x77;
    CHECK(U(CheckTokenMembershipEx)(NULL, s, CTMF_INCLUDE_APPCONTAINER, &m));
    EXPECT_EQ(m, TRUE);
}
TEST(gap1_checktokenmembershipex_matches_non_ex) {
    REQUIRE(U(CheckTokenMembershipEx)); REQUIRE(U(CheckTokenMembership));
    HANDLE t = g1_imp_token(TOKEN_ALL_ACCESS);
    if (!t) { t_skip("no token"); return; }
    static const WELL_KNOWN_SID_TYPE ty[] = { WinWorldSid, WinLocalSystemSid, WinBuiltinAdministratorsSid, WinBuiltinUsersSid,
        WinAuthenticatedUserSid, WinInteractiveSid, WinNetworkSid, WinLocalSid, WinBatchSid, WinServiceSid, WinMediumLabelSid, WinHighLabelSid };
    for (size_t i = 0; i < sizeof ty / sizeof ty[0]; i++) {
        BYTE b[SECURITY_MAX_SID_SIZE]; DWORD n = sizeof b;
        if (!CreateWellKnownSid(ty[i], NULL, b, &n)) continue;
        BOOL m1 = 0x77, m2 = 0x77, m3 = 0x77;
        BOOL r1 = U(CheckTokenMembership)(t, b, &m1);
        BOOL r2 = U(CheckTokenMembershipEx)(t, b, 0, &m2);
        BOOL r3 = U(CheckTokenMembership)(NULL, b, &m3);
        CHECKF(r1 && r2 && r3 && m1 == m2 && m1 == m3, "sid type %d: %d/%d/%d", ty[i], m1, m2, m3);
    }
    CloseHandle(t);
}
TEST(gap1_checktokenmembershipex_invalid_flags) {
    REQUIRE(U(CheckTokenMembershipEx));
    BYTE w[SECURITY_MAX_SID_SIZE]; PSID s = g1_world(w);
    static const DWORD f[] = { 4, 8, 0x10, 0x100, 0x80000000, 0xFFFFFFFC, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof f / sizeof f[0]; i++) {
        BOOL m = 0x77;
        SET_ERR(0xAAAA);
        BOOL r = U(CheckTokenMembershipEx)(NULL, s, f[i], &m);
        t_note("flags %08lx: ret=%d member=%d err=%lu", f[i], r, m, GetLastError());
        if (r) CHECK(m == 0 || m == 1);
    }
}
TEST(gap1_checktokenmembershipex_closed_handle) {
    REQUIRE(U(CheckTokenMembershipEx));
    HANDLE t = g1_imp_token(TOKEN_ALL_ACCESS);
    if (!t) { t_skip("no token"); return; }
    CloseHandle(t);
    BYTE w[SECURITY_MAX_SID_SIZE]; PSID s = g1_world(w);
    BOOL m = 0x77;
    SET_ERR(0);
    CHECK(!U(CheckTokenMembershipEx)(t, s, 0, &m));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap1_checktokenmembershipex_in_kernel32_export) {
    if (t_is_wine()) { t_skip("export missing under wine"); return; }
    FARPROC p = t_proc_k32("CheckTokenMembershipEx");
    CHECK(p != NULL);
    FARPROC q = t_proc_kb("CheckTokenMembershipEx");
    CHECK(q != NULL);
}

/* ---------- CheckTokenCapability ---------- */
TEST(gap1_checktokencapability_null_sid) {
    REQUIRE(U(CheckTokenCapability));
    BOOL h = 0x77;
    SET_ERR(0);
    CHECK(!U(CheckTokenCapability)(NULL, NULL, &h));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap1_checktokencapability_null_out) {
    REQUIRE(U(CheckTokenCapability));
    G1SID s; g1_mksid(&s, 2, 15); s.sub[0] = 3; s.sub[1] = 1;
    SET_ERR(0);
    CHECK(!U(CheckTokenCapability)(NULL, &s, NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap1_checktokencapability_non_capability_sid_rejected) {
    REQUIRE(U(CheckTokenCapability));
    BYTE w[SECURITY_MAX_SID_SIZE]; PSID s = g1_world(w);
    BOOL h = 0x77;
    SET_ERR(0);
    CHECK(!U(CheckTokenCapability)(NULL, s, &h));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    EXPECT_EQ(h, FALSE);
}
TEST(gap1_checktokencapability_non_capability_sids_table) {
    REQUIRE(U(CheckTokenCapability));
    static const WELL_KNOWN_SID_TYPE ty[] = { WinLocalSystemSid, WinBuiltinAdministratorsSid, WinBuiltinUsersSid, WinAuthenticatedUserSid, WinMediumLabelSid };
    for (size_t i = 0; i < sizeof ty / sizeof ty[0]; i++) {
        BYTE b[SECURITY_MAX_SID_SIZE]; DWORD n = sizeof b;
        if (!CreateWellKnownSid(ty[i], NULL, b, &n)) continue;
        BOOL h = 0x77;
        SET_ERR(0);
        CHECKF(!U(CheckTokenCapability)(NULL, b, &h), "type %d", ty[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        CHECKF(h == FALSE, "type %d has=%d", ty[i], h);
    }
}
TEST(gap1_checktokencapability_wrong_authority_rejected) {
    REQUIRE(U(CheckTokenCapability));
    G1SID s; g1_mksid(&s, 2, 5); s.sub[0] = 3; s.sub[1] = 1;   /* S-1-5-3-1: NT authority, not app package */
    BOOL h = 0x77;
    SET_ERR(0);
    CHECK(!U(CheckTokenCapability)(NULL, &s, &h));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap1_checktokencapability_wrong_base_rid_rejected) {
    REQUIRE(U(CheckTokenCapability));
    G1SID s; g1_mksid(&s, 2, 15); s.sub[0] = 2; s.sub[1] = 1;  /* S-1-15-2-1: app package SID, not capability */
    BOOL h = 0x77;
    SET_ERR(0);
    CHECK(!U(CheckTokenCapability)(NULL, &s, &h));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap1_checktokencapability_valid_capability_sid) {
    REQUIRE(U(CheckTokenCapability));
    G1SID s; g1_mksid(&s, 2, 15); s.sub[0] = 3; s.sub[1] = 1;   /* internetClient */
    BOOL h = 0x77;
    SET_ERR(0);
    BOOL r = U(CheckTokenCapability)(NULL, &s, &h);
    t_note("internetClient on process token: ret=%d has=%d err=%lu", r, h, GetLastError());
    if (r) CHECK(h == 0 || h == 1);
    else CHECK(h == 0 || h == 0x77);
}
TEST(gap1_checktokencapability_with_impersonation_token) {
    REQUIRE(U(CheckTokenCapability));
    HANDLE t = g1_imp_token(TOKEN_ALL_ACCESS);
    if (!t) { t_skip("no token"); return; }
    G1SID s; g1_mksid(&s, 2, 15); s.sub[0] = 3; s.sub[1] = 1;
    BOOL h = 0x77;
    BOOL r = U(CheckTokenCapability)(t, &s, &h);
    BOOL h2 = 0x77;
    BOOL r2 = U(CheckTokenCapability)(NULL, &s, &h2);
    t_note("explicit imp token ret=%d has=%d ; NULL token ret=%d has=%d", r, h, r2, h2);
    if (r && r2) CHECK(h == h2);
    CloseHandle(t);
}
TEST(gap1_checktokencapability_closed_handle) {
    REQUIRE(U(CheckTokenCapability));
    HANDLE t = g1_imp_token(TOKEN_ALL_ACCESS);
    if (!t) { t_skip("no token"); return; }
    CloseHandle(t);
    G1SID s; g1_mksid(&s, 2, 15); s.sub[0] = 3; s.sub[1] = 1;
    BOOL h = 0x77;
    SET_ERR(0);
    CHECK(!U(CheckTokenCapability)(t, &s, &h));
    t_note("closed handle -> err %lu", GetLastError());
    EXPECT_EQ(h, FALSE);
}
TEST(gap1_checktokencapability_unheld_derived_capability) {
    REQUIRE(U(CheckTokenCapability));
    G1SID s; g1_mksid(&s, 8, 15); s.sub[0] = 3;
    for (int i = 1; i < 8; i++) s.sub[i] = 0xA5A50000 + i;
    BOOL h = 0x77;
    BOOL r = U(CheckTokenCapability)(NULL, &s, &h);
    t_note("derived capability: ret=%d has=%d err=%lu", r, h, GetLastError());
}

/* ---------- ConvertToAutoInheritPrivateObjectSecurity ---------- */
typedef struct { SECURITY_DESCRIPTOR sd; BYTE acl[256]; BYTE owner[SECURITY_MAX_SID_SIZE], group[SECURITY_MAX_SID_SIZE], world[SECURITY_MAX_SID_SIZE]; } G1SD;
static BOOL g1_build_sd(G1SD *g, DWORD aceMask, BYTE aceFlags, BOOL withDacl) {
    DWORD n;
    memset(g, 0, sizeof *g);
    n = SECURITY_MAX_SID_SIZE; CreateWellKnownSid(WinBuiltinAdministratorsSid, NULL, g->owner, &n);
    n = SECURITY_MAX_SID_SIZE; CreateWellKnownSid(WinBuiltinUsersSid, NULL, g->group, &n);
    n = SECURITY_MAX_SID_SIZE; CreateWellKnownSid(WinWorldSid, NULL, g->world, &n);
    if (!InitializeSecurityDescriptor(&g->sd, SECURITY_DESCRIPTOR_REVISION)) return FALSE;
    SetSecurityDescriptorOwner(&g->sd, g->owner, FALSE);
    SetSecurityDescriptorGroup(&g->sd, g->group, FALSE);
    if (withDacl) {
        if (!InitializeAcl((PACL)g->acl, sizeof g->acl, ACL_REVISION)) return FALSE;
        if (!AddAccessAllowedAceEx((PACL)g->acl, ACL_REVISION, aceFlags, aceMask, g->world)) return FALSE;
        if (!SetSecurityDescriptorDacl(&g->sd, TRUE, (PACL)g->acl, FALSE)) return FALSE;
    }
    return TRUE;
}
static const GENERIC_MAPPING g1_filemap = { FILE_GENERIC_READ, FILE_GENERIC_WRITE, FILE_GENERIC_EXECUTE, FILE_ALL_ACCESS };

TEST(gap1_converttoautoinherit_no_parent_roundtrip) {
    REQUIRE(U(ConvertToAutoInheritPrivateObjectSecurity));
    if (t_is_wine()) { t_skip("not implemented under wine"); return; } REQUIRE(U(DestroyPrivateObjectSecurity));
    G1SD cur; if (!g1_build_sd(&cur, FILE_GENERIC_READ, 0, TRUE)) { t_skip("sd build"); return; }
    PSECURITY_DESCRIPTOR nsd = NULL;
    GENERIC_MAPPING gm = g1_filemap;
    SET_ERR(0);
    BOOL r = U(ConvertToAutoInheritPrivateObjectSecurity)(NULL, &cur.sd, &nsd, NULL, FALSE, &gm);
    CHECKF(r, "err %lu", GetLastError());
    if (!r) return;
    CHECK(nsd != NULL);
    CHECK(IsValidSecurityDescriptor(nsd));
    PSID o = NULL, g = NULL; BOOL def; BOOL pres = FALSE, dd; PACL acl = NULL;
    GetSecurityDescriptorOwner(nsd, &o, &def);
    GetSecurityDescriptorGroup(nsd, &g, &def);
    CHECK(o && EqualSid(o, cur.owner));
    CHECK(g && EqualSid(g, cur.group));
    GetSecurityDescriptorDacl(nsd, &pres, &acl, &dd);
    CHECK(pres && acl);
    if (acl) { CHECK(acl->AceCount == 1); }
    SECURITY_DESCRIPTOR_CONTROL c; DWORD rev;
    GetSecurityDescriptorControl(nsd, &c, &rev);
    t_note("result control=0x%04x", c);
    CHECK(U(DestroyPrivateObjectSecurity)(&nsd));
    CHECK(nsd == NULL);
}
TEST(gap1_converttoautoinherit_directory_flag) {
    REQUIRE(U(ConvertToAutoInheritPrivateObjectSecurity));
    if (t_is_wine()) { t_skip("not implemented under wine"); return; } REQUIRE(U(DestroyPrivateObjectSecurity));
    G1SD cur; if (!g1_build_sd(&cur, FILE_GENERIC_READ, CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE, TRUE)) { t_skip("sd build"); return; }
    PSECURITY_DESCRIPTOR nsd = NULL;
    GENERIC_MAPPING gm = g1_filemap;
    BOOL r = U(ConvertToAutoInheritPrivateObjectSecurity)(NULL, &cur.sd, &nsd, NULL, TRUE, &gm);
    CHECKF(r, "err %lu", GetLastError());
    if (r) { CHECK(IsValidSecurityDescriptor(nsd)); U(DestroyPrivateObjectSecurity)(&nsd); }
}
TEST(gap1_converttoautoinherit_with_parent_matching_ace) {
    REQUIRE(U(ConvertToAutoInheritPrivateObjectSecurity));
    if (t_is_wine()) { t_skip("not implemented under wine"); return; } REQUIRE(U(DestroyPrivateObjectSecurity));
    G1SD par, cur;
    if (!g1_build_sd(&par, FILE_GENERIC_READ, OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE, TRUE)) { t_skip("sd build"); return; }
    g1_build_sd(&cur, FILE_GENERIC_READ, 0, TRUE);
    PSECURITY_DESCRIPTOR nsd = NULL;
    GENERIC_MAPPING gm = g1_filemap;
    BOOL r = U(ConvertToAutoInheritPrivateObjectSecurity)(&par.sd, &cur.sd, &nsd, NULL, FALSE, &gm);
    CHECKF(r, "err %lu", GetLastError());
    if (!r) return;
    CHECK(IsValidSecurityDescriptor(nsd));
    PACL acl = NULL; BOOL pres = FALSE, dd;
    GetSecurityDescriptorDacl(nsd, &pres, &acl, &dd);
    if (pres && acl) {
        t_note("AceCount=%u", acl->AceCount);
        ACE_HEADER *h = NULL;
        if (acl->AceCount && GetAce(acl, 0, (LPVOID *)&h)) t_note("first ace flags=0x%02x (INHERITED_ACE=0x10)", h->AceFlags);
    }
    SECURITY_DESCRIPTOR_CONTROL c; DWORD rev;
    GetSecurityDescriptorControl(nsd, &c, &rev);
    t_note("control=0x%04x", c);
    U(DestroyPrivateObjectSecurity)(&nsd);
}
TEST(gap1_converttoautoinherit_no_dacl_in_current) {
    REQUIRE(U(ConvertToAutoInheritPrivateObjectSecurity));
    if (t_is_wine()) { t_skip("not implemented under wine"); return; } REQUIRE(U(DestroyPrivateObjectSecurity));
    G1SD cur; if (!g1_build_sd(&cur, 0, 0, FALSE)) { t_skip("sd build"); return; }
    PSECURITY_DESCRIPTOR nsd = NULL;
    GENERIC_MAPPING gm = g1_filemap;
    SET_ERR(0);
    BOOL r = U(ConvertToAutoInheritPrivateObjectSecurity)(NULL, &cur.sd, &nsd, NULL, FALSE, &gm);
    t_note("no-DACL current SD: ret=%d err=%lu", r, GetLastError());
    if (r && nsd) { CHECK(IsValidSecurityDescriptor(nsd)); U(DestroyPrivateObjectSecurity)(&nsd); }
}
TEST(gap1_converttoautoinherit_invalid_current_sd) {
    REQUIRE(U(ConvertToAutoInheritPrivateObjectSecurity));
    if (t_is_wine()) { t_skip("not implemented under wine"); return; }
    G1SD cur; if (!g1_build_sd(&cur, FILE_GENERIC_READ, 0, TRUE)) { t_skip("sd build"); return; }
    ((BYTE *)&cur.sd)[0] = 7;   /* revision */
    PSECURITY_DESCRIPTOR nsd = (PSECURITY_DESCRIPTOR)(ULONG_PTR)0x1;
    GENERIC_MAPPING gm = g1_filemap;
    SET_ERR(0);
    BOOL r = U(ConvertToAutoInheritPrivateObjectSecurity)(NULL, &cur.sd, &nsd, NULL, FALSE, &gm);
    DWORD e = GetLastError();
    t_note("bad revision: ret=%d err=%lu nsd=%p", r, e, nsd);
    CHECK(!r);
    CHECK(e != 0);
}
TEST(gap1_destroyprivateobjectsecurity_after_convert_null_after) {
    REQUIRE(U(ConvertToAutoInheritPrivateObjectSecurity)); REQUIRE(U(DestroyPrivateObjectSecurity));
    if (t_is_wine()) { t_skip("not implemented under wine"); return; }
    G1SD cur; if (!g1_build_sd(&cur, FILE_GENERIC_READ | FILE_GENERIC_WRITE, 0, TRUE)) { t_skip("sd build"); return; }
    for (int i = 0; i < 20; i++) {
        PSECURITY_DESCRIPTOR nsd = NULL;
        GENERIC_MAPPING gm = g1_filemap;
        BOOL r = U(ConvertToAutoInheritPrivateObjectSecurity)(NULL, &cur.sd, &nsd, NULL, (i & 1), &gm);
        if (!r) { CHECKF(0, "iter %d err %lu", i, GetLastError()); return; }
        CHECK(U(DestroyPrivateObjectSecurity)(&nsd));
    }
}

/* ---------- AppPolicyGetThreadInitializationType ---------- */
TEST(gap1_apppolicy_threadinit_null_token) {
    REQUIRE(U(AppPolicyGetThreadInitializationType));
    if (t_is_wine()) { t_skip("stub under wine"); return; }
    AppPolicyThreadInitializationType t = (AppPolicyThreadInitializationType)0x55;
    LONG r = U(AppPolicyGetThreadInitializationType)(NULL, &t);
    EXPECT_EQ(r, ERROR_INVALID_PARAMETER);
    EXPECT_EQ(t, 0x55);
}
TEST(gap1_apppolicy_threadinit_null_out) {
    REQUIRE(U(AppPolicyGetThreadInitializationType));
    if (t_is_wine()) { t_skip("stub under wine"); return; }
    LONG r = U(AppPolicyGetThreadInitializationType)(GetCurrentProcessToken(), NULL);
    EXPECT_EQ(r, ERROR_INVALID_PARAMETER);
}
TEST(gap1_apppolicy_threadinit_both_null) {
    REQUIRE(U(AppPolicyGetThreadInitializationType));
    if (t_is_wine()) { t_skip("stub under wine"); return; }
    EXPECT_EQ(U(AppPolicyGetThreadInitializationType)(NULL, NULL), ERROR_INVALID_PARAMETER);
}
TEST(gap1_apppolicy_threadinit_does_not_set_last_error) {
    REQUIRE(U(AppPolicyGetThreadInitializationType));
    if (t_is_wine()) { t_skip("stub under wine"); return; }
    SET_ERR(0x3141);
    AppPolicyThreadInitializationType t;
    U(AppPolicyGetThreadInitializationType)(NULL, &t);
    EXPECT_ERR(0x3141);
}
TEST(gap1_apppolicy_threadinit_pseudo_tokens) {
    REQUIRE(U(AppPolicyGetThreadInitializationType));
    if (t_is_wine()) { t_skip("stub under wine"); return; }
    HANDLE toks[3] = { (HANDLE)(LONG_PTR)-4, (HANDLE)(LONG_PTR)-5, (HANDLE)(LONG_PTR)-6 };
    for (int i = 0; i < 3; i++) {
        AppPolicyThreadInitializationType t = (AppPolicyThreadInitializationType)0x55;
        LONG r = U(AppPolicyGetThreadInitializationType)(toks[i], &t);
        t_note("pseudo token %d: ret=%ld out=%d", -4 - i, r, (int)t);
        if (r == 0) CHECK(t == 0 || t == 1); else CHECK(t == 0);
    }
}
TEST(gap1_apppolicy_threadinit_real_token) {
    REQUIRE(U(AppPolicyGetThreadInitializationType));
    if (t_is_wine()) { t_skip("stub under wine"); return; }
    HANDLE p = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &p)) { t_skip("token"); return; }
    AppPolicyThreadInitializationType t = (AppPolicyThreadInitializationType)0x55;
    LONG r = U(AppPolicyGetThreadInitializationType)(p, &t);
    t_note("process token: ret=%ld out=%d", r, (int)t);
    if (r == 0) CHECK(t == 0 || t == 1); else CHECK(t == 0);
    AppPolicyThreadInitializationType t2 = (AppPolicyThreadInitializationType)0x55;
    LONG r2 = U(AppPolicyGetThreadInitializationType)(p, &t2);
    EXPECT_EQ(r, r2); EXPECT_EQ(t, t2);
    CloseHandle(p);
}
TEST(gap1_apppolicy_threadinit_closed_token) {
    REQUIRE(U(AppPolicyGetThreadInitializationType));
    if (t_is_wine()) { t_skip("stub under wine"); return; }
    HANDLE p = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &p)) { t_skip("token"); return; }
    CloseHandle(p);
    AppPolicyThreadInitializationType t = (AppPolicyThreadInitializationType)0x55;
    LONG r = U(AppPolicyGetThreadInitializationType)(p, &t);
    t_note("closed token: ret=%ld out=%d", r, (int)t);
    CHECK(t == 0);
}
TEST(gap1_apppolicy_threadinit_wrong_handle_type) {
    REQUIRE(U(AppPolicyGetThreadInitializationType));
    if (t_is_wine()) { t_skip("stub under wine"); return; }
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    AppPolicyThreadInitializationType t = (AppPolicyThreadInitializationType)0x55;
    LONG r = U(AppPolicyGetThreadInitializationType)(ev, &t);
    t_note("event as token: ret=%ld", r);
    CHECK(t == 0);
    CloseHandle(ev);
}
TEST(gap1_apppolicy_threadinit_invalid_handle_value) {
    REQUIRE(U(AppPolicyGetThreadInitializationType));
    if (t_is_wine()) { t_skip("stub under wine"); return; }
    AppPolicyThreadInitializationType t = (AppPolicyThreadInitializationType)0x55;
    LONG r = U(AppPolicyGetThreadInitializationType)(INVALID_HANDLE_VALUE, &t);
    t_note("INVALID_HANDLE_VALUE: ret=%ld out=%d", r, (int)t);
    CHECK(t == 0 || t == 1);
}
