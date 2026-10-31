/* AdjustTokenPrivileges, AdjustTokenGroups, AllocateAndInitializeSid, AllocateLocallyUniqueId.
 * Token manipulation only ever touches duplicates of the process token. */
#include "g0.h"
#include <stdlib.h>

static HANDLE dup_tok(DWORD access) {
    HANDLE p = NULL, d = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | TOKEN_QUERY, &p)) return NULL;
    if (!DuplicateTokenEx(p, access, NULL, SecurityImpersonation, TokenImpersonation, &d)) d = NULL;
    CloseHandle(p);
    return d;
}
static TOKEN_PRIVILEGES *get_privs(HANDLE t) {
    DWORD n = 0; GetTokenInformation(t, TokenPrivileges, NULL, 0, &n);
    TOKEN_PRIVILEGES *tp = (TOKEN_PRIVILEGES *)malloc(n ? n : 8);
    if (tp && !GetTokenInformation(t, TokenPrivileges, tp, n, &n)) { free(tp); return NULL; }
    return tp;
}
static int find_priv(HANDLE t, LUID l, DWORD *attr) {
    TOKEN_PRIVILEGES *tp = get_privs(t); int r = 0;
    if (!tp) return 0;
    for (DWORD i = 0; i < tp->PrivilegeCount; i++)
        if (tp->Privileges[i].Luid.LowPart == l.LowPart && tp->Privileges[i].Luid.HighPart == l.HighPart) { r = 1; if (attr) *attr = tp->Privileges[i].Attributes; }
    free(tp);
    return r;
}
/* pick a privilege that the token holds; prefer SeChangeNotifyPrivilege */
static int pick_priv(HANDLE t, LUID *out, DWORD *attr) {
    LUID l;
    if (LookupPrivilegeValueA(NULL, "SeChangeNotifyPrivilege", &l) && find_priv(t, l, attr)) { *out = l; return 1; }
    TOKEN_PRIVILEGES *tp = get_privs(t);
    if (!tp || !tp->PrivilegeCount) { free(tp); return 0; }
    *out = tp->Privileges[0].Luid; *attr = tp->Privileges[0].Attributes;
    free(tp);
    return 1;
}
typedef struct { DWORD n; LUID_AND_ATTRIBUTES a[2]; } TP2;

TEST(gap0_atp_disable_enable_roundtrip) {
    REQUIRE(U(AdjustTokenPrivileges));
    HANDLE t = dup_tok(TOKEN_ALL_ACCESS); CHECK(t != NULL); if (!t) return;
    LUID l; DWORD at = 0;
    if (!pick_priv(t, &l, &at)) { t_skip("token holds no privileges"); CloseHandle(t); return; }
    TOKEN_PRIVILEGES ns = {1, {{l, 0}}};
    SET_ERR(0x1234);
    BOOL r = U(AdjustTokenPrivileges)(t, FALSE, &ns, 0, NULL, NULL);
    CHECK(r);
    EXPECT_EQ(GetLastError(), 0);                 /* success explicitly sets last error 0 */
    DWORD now = 0xff; CHECK(find_priv(t, l, &now));
    EXPECT_EQ(now & SE_PRIVILEGE_ENABLED, 0);
    ns.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    SET_ERR(0x1234);
    r = U(AdjustTokenPrivileges)(t, FALSE, &ns, 0, NULL, NULL);
    CHECK(r); EXPECT_EQ(GetLastError(), 0);
    CHECK(find_priv(t, l, &now));
    EXPECT_EQ(now & SE_PRIVILEGE_ENABLED, SE_PRIVILEGE_ENABLED);
    /* enabling twice is fine */
    CHECK(U(AdjustTokenPrivileges)(t, FALSE, &ns, 0, NULL, NULL));
    EXPECT_EQ(GetLastError(), 0);
    CloseHandle(t);
}

TEST(gap0_atp_previous_state) {
    REQUIRE(U(AdjustTokenPrivileges));
    HANDLE t = dup_tok(TOKEN_ALL_ACCESS); if (!t) { CHECK(0); return; }
    LUID l; DWORD at = 0;
    if (!pick_priv(t, &l, &at)) { t_skip("token holds no privileges"); CloseHandle(t); return; }
    /* make sure it is enabled first */
    TOKEN_PRIVILEGES en = {1, {{l, SE_PRIVILEGE_ENABLED}}};
    CHECK(U(AdjustTokenPrivileges)(t, FALSE, &en, 0, NULL, NULL));
    TOKEN_PRIVILEGES dis = {1, {{l, 0}}};
    union { TOKEN_PRIVILEGES p; BYTE b[128]; } prev; memset(&prev, 0xCC, sizeof prev);
    DWORD rl = 0xdead;
    SET_ERR(0x1234);
    BOOL r = U(AdjustTokenPrivileges)(t, FALSE, &dis, sizeof prev, &prev.p, &rl);
    CHECK(r); EXPECT_EQ(GetLastError(), 0);
    EXPECT_EQ(prev.p.PrivilegeCount, 1);
    EXPECT_EQ(rl, sizeof(DWORD) + sizeof(LUID_AND_ATTRIBUTES));
    EXPECT_EQ(prev.p.Privileges[0].Luid.LowPart, l.LowPart);
    EXPECT_EQ(prev.p.Privileges[0].Attributes & SE_PRIVILEGE_ENABLED, SE_PRIVILEGE_ENABLED);
    /* the previous state can be fed straight back in */
    CHECK(U(AdjustTokenPrivileges)(t, FALSE, &prev.p, 0, NULL, NULL));
    DWORD now = 0; CHECK(find_priv(t, l, &now));
    EXPECT_EQ(now & SE_PRIVILEGE_ENABLED, SE_PRIVILEGE_ENABLED);
    /* nothing changes: previous state reports an empty change set */
    memset(&prev, 0xCC, sizeof prev); rl = 0xdead;
    CHECK(U(AdjustTokenPrivileges)(t, FALSE, &en, sizeof prev, &prev.p, &rl));
    t_note("no-op adjust: PreviousState.PrivilegeCount=%lu ReturnLength=%lu", prev.p.PrivilegeCount, rl);
    CloseHandle(t);
}

TEST(gap0_atp_previous_buffer_too_small) {
    REQUIRE(U(AdjustTokenPrivileges));
    HANDLE t = dup_tok(TOKEN_ALL_ACCESS); if (!t) { CHECK(0); return; }
    LUID l; DWORD at = 0;
    if (!pick_priv(t, &l, &at)) { t_skip("token holds no privileges"); CloseHandle(t); return; }
    TOKEN_PRIVILEGES en = {1, {{l, SE_PRIVILEGE_ENABLED}}};
    CHECK(U(AdjustTokenPrivileges)(t, FALSE, &en, 0, NULL, NULL));
    TOKEN_PRIVILEGES dis = {1, {{l, 0}}};
    BYTE small[4]; DWORD rl = 0;
    SET_ERR(0);
    BOOL r = U(AdjustTokenPrivileges)(t, FALSE, &dis, 0, (PTOKEN_PRIVILEGES)small, &rl);
    CHECK(!r);
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(rl, 16);
    DWORD now = 0; CHECK(find_priv(t, l, &now));
    t_note("after too-small buffer the privilege attributes are 0x%lx", now);
    CloseHandle(t);
}

TEST(gap0_atp_disable_all) {
    REQUIRE(U(AdjustTokenPrivileges));
    HANDLE t = dup_tok(TOKEN_ALL_ACCESS); if (!t) { CHECK(0); return; }
    SET_ERR(0x1234);
    BOOL r = U(AdjustTokenPrivileges)(t, TRUE, NULL, 0, NULL, NULL);
    CHECK(r); EXPECT_EQ(GetLastError(), 0);
    TOKEN_PRIVILEGES *tp = get_privs(t);
    CHECK(tp != NULL);
    if (tp) {
        for (DWORD i = 0; i < tp->PrivilegeCount; i++)
            if (tp->Privileges[i].Attributes & SE_PRIVILEGE_ENABLED) { CHECKF(0, "privilege %lu still enabled", i); break; }
        free(tp);
    }
    /* NewState is ignored when DisableAllPrivileges is TRUE */
    LUID l; DWORD at;
    if (pick_priv(t, &l, &at)) {
        TOKEN_PRIVILEGES en = {1, {{l, SE_PRIVILEGE_ENABLED}}};
        CHECK(U(AdjustTokenPrivileges)(t, TRUE, &en, 0, NULL, NULL));
        DWORD now = 0xff; CHECK(find_priv(t, l, &now));
        EXPECT_EQ(now & SE_PRIVILEGE_ENABLED, 0);
        /* and now enable it for real */
        CHECK(U(AdjustTokenPrivileges)(t, FALSE, &en, 0, NULL, NULL));
        CHECK(find_priv(t, l, &now));
        EXPECT_EQ(now & SE_PRIVILEGE_ENABLED, SE_PRIVILEGE_ENABLED);
    }
    CloseHandle(t);
}

TEST(gap0_atp_disable_all_previous_state) {
    REQUIRE(U(AdjustTokenPrivileges));
    HANDLE t = dup_tok(TOKEN_ALL_ACCESS); if (!t) { CHECK(0); return; }
    TOKEN_PRIVILEGES *before = get_privs(t); if (!before) { CHECK(0); return; }
    DWORD nen = 0;
    for (DWORD i = 0; i < before->PrivilegeCount; i++) if (before->Privileges[i].Attributes & SE_PRIVILEGE_ENABLED) nen++;
    DWORD cap = 4 + 12 * (before->PrivilegeCount + 1);
    TOKEN_PRIVILEGES *prev = (TOKEN_PRIVILEGES *)calloc(1, cap);
    DWORD rl = 0;
    BOOL r = U(AdjustTokenPrivileges)(t, TRUE, NULL, cap, prev, &rl);
    CHECK(r);
    t_note("disable-all with PreviousState: enabled before=%lu previous.count=%lu returnlen=%lu", nen, prev->PrivilegeCount, rl);
    EXPECT_EQ(prev->PrivilegeCount, nen);
    EXPECT_EQ(rl, 4 + 12 * nen);
    /* restoring brings back exactly the privileges that were enabled */
    CHECK(U(AdjustTokenPrivileges)(t, FALSE, prev, 0, NULL, NULL));
    TOKEN_PRIVILEGES *after = get_privs(t);
    if (after) {
        DWORD nen2 = 0;
        for (DWORD i = 0; i < after->PrivilegeCount; i++) if (after->Privileges[i].Attributes & SE_PRIVILEGE_ENABLED) nen2++;
        EXPECT_EQ(nen2, nen);
        free(after);
    }
    free(prev); free(before);
    CloseHandle(t);
}

TEST(gap0_atp_not_all_assigned) {
    REQUIRE(U(AdjustTokenPrivileges));
    HANDLE t = dup_tok(TOKEN_ALL_ACCESS); if (!t) { CHECK(0); return; }
    LUID create;
    if (!LookupPrivilegeValueA(NULL, "SeCreateTokenPrivilege", &create)) { CHECK(0); return; }
    DWORD at;
    if (find_priv(t, create, &at)) { t_skip("token holds SeCreateTokenPrivilege"); CloseHandle(t); return; }
    TOKEN_PRIVILEGES ns = {1, {{create, SE_PRIVILEGE_ENABLED}}};
    SET_ERR(0);
    BOOL r = U(AdjustTokenPrivileges)(t, FALSE, &ns, 0, NULL, NULL);
    CHECK(r);                                  /* succeeds, but warns */
    EXPECT_ERR(ERROR_NOT_ALL_ASSIGNED);
    /* mixed: one held, one not */
    LUID l; DWORD a2;
    if (pick_priv(t, &l, &a2)) {
        TP2 m = {2, {{l, 0}, {create, SE_PRIVILEGE_ENABLED}}};
        SET_ERR(0);
        r = U(AdjustTokenPrivileges)(t, FALSE, (PTOKEN_PRIVILEGES)&m, 0, NULL, NULL);
        CHECK(r); EXPECT_ERR(ERROR_NOT_ALL_ASSIGNED);
        DWORD now = 0xff; CHECK(find_priv(t, l, &now));
        EXPECT_EQ(now & SE_PRIVILEGE_ENABLED, 0);      /* the held one was applied */
    }
    CloseHandle(t);
}

TEST(gap0_atp_unknown_luids) {
    REQUIRE(U(AdjustTokenPrivileges));
    HANDLE t = dup_tok(TOKEN_ALL_ACCESS); if (!t) { CHECK(0); return; }
    LUID bogus[] = {{0, 0}, {0xFFFFFF, 0}, {1, 1}, {0xFFFFFFFF, 0x7FFFFFFF}};
    for (unsigned i = 0; i < 4; i++) {
        TOKEN_PRIVILEGES ns = {1, {{bogus[i], SE_PRIVILEGE_ENABLED}}};
        SET_ERR(0);
        BOOL r = U(AdjustTokenPrivileges)(t, FALSE, &ns, 0, NULL, NULL);
        t_note("bogus luid %u -> ret %d err %lu", i, r, GetLastError());
        if (r) EXPECT_ERR(ERROR_NOT_ALL_ASSIGNED);
    }
    CloseHandle(t);
}

TEST(gap0_atp_remove_privilege) {
    REQUIRE(U(AdjustTokenPrivileges));
    HANDLE t = dup_tok(TOKEN_ALL_ACCESS); if (!t) { CHECK(0); return; }
    LUID l; DWORD at;
    if (!pick_priv(t, &l, &at)) { t_skip("token holds no privileges"); CloseHandle(t); return; }
    TOKEN_PRIVILEGES ns = {1, {{l, SE_PRIVILEGE_REMOVED}}};
    SET_ERR(0x1234);
    BOOL r = U(AdjustTokenPrivileges)(t, FALSE, &ns, 0, NULL, NULL);
    CHECK(r); EXPECT_EQ(GetLastError(), 0);
    CHECK(!find_priv(t, l, NULL));
    /* removing again: no longer held */
    SET_ERR(0);
    r = U(AdjustTokenPrivileges)(t, FALSE, &ns, 0, NULL, NULL);
    CHECK(r); EXPECT_ERR(ERROR_NOT_ALL_ASSIGNED);
    /* cannot be re-enabled */
    ns.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    SET_ERR(0);
    r = U(AdjustTokenPrivileges)(t, FALSE, &ns, 0, NULL, NULL);
    CHECK(r); EXPECT_ERR(ERROR_NOT_ALL_ASSIGNED);
    CloseHandle(t);
}

TEST(gap0_atp_handle_errors) {
    REQUIRE(U(AdjustTokenPrivileges));
    LUID l; LookupPrivilegeValueA(NULL, "SeChangeNotifyPrivilege", &l);
    TOKEN_PRIVILEGES ns = {1, {{l, 0}}};
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE bad[] = {NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x7777, ev, GetCurrentProcess(), GetCurrentThread()};
    for (unsigned i = 0; i < 6; i++) {
        SET_ERR(0);
        CHECKF(!U(AdjustTokenPrivileges)(bad[i], FALSE, &ns, 0, NULL, NULL), "handle %u", i);
        EXPECT_ERR(ERROR_INVALID_HANDLE);
        SET_ERR(0);
        CHECKF(!U(AdjustTokenPrivileges)(bad[i], TRUE, NULL, 0, NULL, NULL), "handle %u disable-all", i);
        EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
    HANDLE c = dup_tok(TOKEN_ALL_ACCESS);
    if (c) { CloseHandle(c); SET_ERR(0); CHECK(!U(AdjustTokenPrivileges)(c, FALSE, &ns, 0, NULL, NULL)); EXPECT_ERR(ERROR_INVALID_HANDLE); }
    CloseHandle(ev);
}

TEST(gap0_atp_access_rights) {
    REQUIRE(U(AdjustTokenPrivileges));
    LUID l; LookupPrivilegeValueA(NULL, "SeChangeNotifyPrivilege", &l);
    TOKEN_PRIVILEGES ns = {1, {{l, 0}}};
    DWORD rights[] = {TOKEN_QUERY, TOKEN_IMPERSONATE, TOKEN_DUPLICATE, TOKEN_ADJUST_GROUPS, TOKEN_ADJUST_DEFAULT};
    for (unsigned i = 0; i < 5; i++) {
        HANDLE t = dup_tok(rights[i]);
        if (!t) { t_note("cannot create token with rights 0x%lx", rights[i]); continue; }
        SET_ERR(0);
        CHECKF(!U(AdjustTokenPrivileges)(t, FALSE, &ns, 0, NULL, NULL), "rights 0x%lx", rights[i]);
        EXPECT_ERR(ERROR_ACCESS_DENIED);
        SET_ERR(0);
        CHECKF(!U(AdjustTokenPrivileges)(t, TRUE, NULL, 0, NULL, NULL), "rights 0x%lx disable-all", rights[i]);
        EXPECT_ERR(ERROR_ACCESS_DENIED);
        CloseHandle(t);
    }
    /* TOKEN_ADJUST_PRIVILEGES alone is enough when no previous state is requested */
    HANDLE t = dup_tok(TOKEN_ADJUST_PRIVILEGES);
    if (t) { SET_ERR(0x1234); CHECK(U(AdjustTokenPrivileges)(t, FALSE, &ns, 0, NULL, NULL)); EXPECT_EQ(GetLastError(), 0); CloseHandle(t); }
    /* but a previous-state request needs TOKEN_QUERY too */
    t = dup_tok(TOKEN_ADJUST_PRIVILEGES);
    if (t) {
        union { TOKEN_PRIVILEGES p; BYTE b[64]; } prev; DWORD rl = 0;
        TOKEN_PRIVILEGES en = {1, {{l, SE_PRIVILEGE_ENABLED}}};
        SET_ERR(0);
        BOOL r = U(AdjustTokenPrivileges)(t, FALSE, &en, sizeof prev, &prev.p, &rl);
        t_note("ADJUST_PRIVILEGES-only with PreviousState -> %d / %lu", r, GetLastError());
        if (!r) EXPECT_ERR(ERROR_ACCESS_DENIED);
        CloseHandle(t);
    }
}

TEST(gap0_atp_null_newstate) {
    REQUIRE(U(AdjustTokenPrivileges));
    HANDLE t = dup_tok(TOKEN_ALL_ACCESS); if (!t) { CHECK(0); return; }
    SET_ERR(0);
    BOOL r = U(AdjustTokenPrivileges)(t, FALSE, NULL, 0, NULL, NULL);
    t_note("NewState NULL, DisableAll FALSE -> %d / %lu", r, GetLastError());
    TOKEN_PRIVILEGES zero = {0, {{{0, 0}, 0}}};
    SET_ERR(0x1234);
    r = U(AdjustTokenPrivileges)(t, FALSE, &zero, 0, NULL, NULL);
    t_note("empty NewState -> %d / %lu", r, GetLastError());
    CloseHandle(t);
}

TEST(gap0_atp_attribute_bits) {
    REQUIRE(U(AdjustTokenPrivileges));
    HANDLE t = dup_tok(TOKEN_ALL_ACCESS); if (!t) { CHECK(0); return; }
    LUID l; DWORD at;
    if (!pick_priv(t, &l, &at)) { t_skip("no privileges"); CloseHandle(t); return; }
    /* ENABLED_BY_DEFAULT / USED_FOR_ACCESS are not honoured from the caller */
    DWORD variants[] = {SE_PRIVILEGE_ENABLED_BY_DEFAULT, SE_PRIVILEGE_USED_FOR_ACCESS, 0x80, 0x10, 0xFFFFFFF0};
    for (unsigned i = 0; i < 5; i++) {
        TOKEN_PRIVILEGES ns = {1, {{l, variants[i]}}};
        SET_ERR(0x1234);
        BOOL r = U(AdjustTokenPrivileges)(t, FALSE, &ns, 0, NULL, NULL);
        DWORD now = 0xff; find_priv(t, l, &now);
        t_note("attributes 0x%lx -> ret %d err %lu, token attr now 0x%lx", variants[i], r, GetLastError(), now);
        if (r) EXPECT_EQ(now & SE_PRIVILEGE_ENABLED, 0);
    }
    CloseHandle(t);
}

/* ----------------------------------------------------- AdjustTokenGroups */
static TOKEN_GROUPS *get_groups(HANDLE t) {
    DWORD n = 0; GetTokenInformation(t, TokenGroups, NULL, 0, &n);
    TOKEN_GROUPS *g = (TOKEN_GROUPS *)malloc(n ? n : 8);
    if (g && !GetTokenInformation(t, TokenGroups, g, n, &n)) { free(g); return NULL; }
    return g;
}
static DWORD group_attr(HANDLE t, PSID sid) {
    TOKEN_GROUPS *g = get_groups(t); DWORD a = 0xFFFFFFFF;
    if (g) { for (DWORD i = 0; i < g->GroupCount; i++) if (EqualSid(g->Groups[i].Sid, sid)) a = g->Groups[i].Attributes; free(g); }
    return a;
}

TEST(gap0_atg_reset_to_default) {
    REQUIRE(U(AdjustTokenGroups));
    HANDLE t = dup_tok(TOKEN_ALL_ACCESS); if (!t) { CHECK(0); return; }
    SET_ERR(0x1234);
    BOOL r = U(AdjustTokenGroups)(t, TRUE, NULL, 0, NULL, NULL);
    CHECK(r); EXPECT_EQ(GetLastError(), 0);
    CloseHandle(t);
}

TEST(gap0_atg_handle_and_access_errors) {
    REQUIRE(U(AdjustTokenGroups));
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE bad[] = {NULL, INVALID_HANDLE_VALUE, (HANDLE)(ULONG_PTR)0x7777, ev, GetCurrentProcess()};
    for (unsigned i = 0; i < 5; i++) {
        SET_ERR(0);
        CHECKF(!U(AdjustTokenGroups)(bad[i], TRUE, NULL, 0, NULL, NULL), "handle %u", i);
        EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
    CloseHandle(ev);
    DWORD rights[] = {TOKEN_QUERY, TOKEN_ADJUST_PRIVILEGES, TOKEN_IMPERSONATE, TOKEN_DUPLICATE};
    for (unsigned i = 0; i < 4; i++) {
        HANDLE t = dup_tok(rights[i]);
        if (!t) continue;
        SET_ERR(0);
        CHECKF(!U(AdjustTokenGroups)(t, TRUE, NULL, 0, NULL, NULL), "rights 0x%lx", rights[i]);
        EXPECT_ERR(ERROR_ACCESS_DENIED);
        CloseHandle(t);
    }
    HANDLE t = dup_tok(TOKEN_ADJUST_GROUPS);
    if (t) { SET_ERR(0x1234); CHECK(U(AdjustTokenGroups)(t, TRUE, NULL, 0, NULL, NULL)); EXPECT_EQ(GetLastError(), 0); CloseHandle(t); }
}

TEST(gap0_atg_cannot_disable_mandatory) {
    REQUIRE(U(AdjustTokenGroups));
    HANDLE t = dup_tok(TOKEN_ALL_ACCESS); if (!t) { CHECK(0); return; }
    TOKEN_GROUPS *g = get_groups(t); CHECK(g != NULL); if (!g) return;
    int found = 0;
    for (DWORD i = 0; i < g->GroupCount && !found; i++) {
        DWORD a = g->Groups[i].Attributes;
        if ((a & SE_GROUP_MANDATORY) && (a & SE_GROUP_ENABLED) && !(a & (SE_GROUP_LOGON_ID | SE_GROUP_USE_FOR_DENY_ONLY | SE_GROUP_INTEGRITY))) {
            found = 1;
            TOKEN_GROUPS ns; ns.GroupCount = 1; ns.Groups[0].Sid = g->Groups[i].Sid; ns.Groups[0].Attributes = 0;
            SET_ERR(0);
            BOOL r = U(AdjustTokenGroups)(t, FALSE, &ns, 0, NULL, NULL);
            CHECK(!r);
            EXPECT_ERR(ERROR_CANT_DISABLE_MANDATORY);
            EXPECT_EQ(group_attr(t, g->Groups[i].Sid) & SE_GROUP_ENABLED, SE_GROUP_ENABLED);
        }
    }
    if (!found) t_note("no mandatory enabled group in token");
    free(g);
    CloseHandle(t);
}

TEST(gap0_atg_toggle_optional_group) {
    REQUIRE(U(AdjustTokenGroups));
    HANDLE t = dup_tok(TOKEN_ALL_ACCESS); if (!t) { CHECK(0); return; }
    TOKEN_GROUPS *g = get_groups(t); if (!g) { CHECK(0); return; }
    int found = 0;
    for (DWORD i = 0; i < g->GroupCount && !found; i++) {
        DWORD a = g->Groups[i].Attributes;
        if ((a & SE_GROUP_ENABLED) && !(a & (SE_GROUP_MANDATORY | SE_GROUP_LOGON_ID | SE_GROUP_USE_FOR_DENY_ONLY | SE_GROUP_INTEGRITY | SE_GROUP_OWNER))) {
            found = 1;
            TOKEN_GROUPS ns; ns.GroupCount = 1; ns.Groups[0].Sid = g->Groups[i].Sid; ns.Groups[0].Attributes = 0;
            SET_ERR(0x1234);
            BOOL r = U(AdjustTokenGroups)(t, FALSE, &ns, 0, NULL, NULL);
            CHECKF(r, "err %lu", GetLastError());
            EXPECT_EQ(group_attr(t, g->Groups[i].Sid) & SE_GROUP_ENABLED, 0);
            ns.Groups[0].Attributes = SE_GROUP_ENABLED;
            CHECK(U(AdjustTokenGroups)(t, FALSE, &ns, 0, NULL, NULL));
            EXPECT_EQ(group_attr(t, g->Groups[i].Sid) & SE_GROUP_ENABLED, SE_GROUP_ENABLED);
            ns.Groups[0].Attributes = 0;
            CHECK(U(AdjustTokenGroups)(t, FALSE, &ns, 0, NULL, NULL));
            CHECK(U(AdjustTokenGroups)(t, TRUE, NULL, 0, NULL, NULL));   /* reset to default */
            EXPECT_EQ(group_attr(t, g->Groups[i].Sid) & SE_GROUP_ENABLED, SE_GROUP_ENABLED);
        }
    }
    if (!found) t_note("no optional enabled group in token");
    free(g);
    CloseHandle(t);
}

TEST(gap0_atg_unknown_group_and_null_state) {
    REQUIRE(U(AdjustTokenGroups));
    HANDLE t = dup_tok(TOKEN_ALL_ACCESS); if (!t) { CHECK(0); return; }
    TOKEN_GROUPS ns; ns.GroupCount = 1; ns.Groups[0].Sid = G0_SID(g0_sid_fake); ns.Groups[0].Attributes = 0;
    SET_ERR(0);
    BOOL r = U(AdjustTokenGroups)(t, FALSE, &ns, 0, NULL, NULL);
    t_note("unknown group -> %d / %lu", r, GetLastError());
    SET_ERR(0);
    r = U(AdjustTokenGroups)(t, FALSE, NULL, 0, NULL, NULL);
    t_note("NewState NULL, reset FALSE -> %d / %lu", r, GetLastError());
    CloseHandle(t);
}

/* ------------------------------------------ AllocateAndInitializeSid */
TEST(gap0_alloc_sid_counts) {
    REQUIRE(U(AllocateAndInitializeSid));
    SID_IDENTIFIER_AUTHORITY nt = {{0, 0, 0, 0, 0, 5}};
    DWORD sub[8] = {0x11111111, 0x22222222, 0x33333333, 0x44444444, 0x55555555, 0x66666666, 0x77777777, 0x88888888};
    for (int n = 0; n <= 8; n++) {
        PSID sid = (PSID)(ULONG_PTR)0x1111;
        BOOL (WINAPI *fn)(PSID_IDENTIFIER_AUTHORITY, BYTE, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, PSID *) = U(AllocateAndInitializeSid);
        SET_ERR(0x1234);
        BOOL r = fn(&nt, (BYTE)n, sub[0], sub[1], sub[2], sub[3], sub[4], sub[5], sub[6], sub[7], &sid);
        CHECKF(r, "count %d err %lu", n, GetLastError());
        if (!r) continue;
        EXPECT_EQ(GetLastError(), 0x1234);
        CHECK(IsValidSid(sid));
        EXPECT_EQ(GetLengthSid(sid), 8 + 4 * n);
        BYTE *b = (BYTE *)sid;
        EXPECT_EQ(b[0], 1);
        EXPECT_EQ(b[1], n);
        EXPECT_EQ(b[7], 5);
        EXPECT_EQ(*GetSidSubAuthorityCount(sid), n);
        for (int k = 0; k < n; k++) EXPECT_EQ(*GetSidSubAuthority(sid, k), sub[k]);
        FreeSid(sid);
    }
}

TEST(gap0_alloc_sid_authority_bytes) {
    REQUIRE(U(AllocateAndInitializeSid));
    SID_IDENTIFIER_AUTHORITY a = {{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}};
    PSID sid = NULL;
    CHECK(U(AllocateAndInitializeSid)(&a, 2, 1, 2, 0, 0, 0, 0, 0, 0, &sid));
    if (!sid) return;
    CHECK(!memcmp(&GetSidIdentifierAuthority(sid)->Value, a.Value, 6));
    CHECK(IsValidSid(sid));
    FreeSid(sid);
    /* well-known: S-1-5-32-544 equals the built-in administrators SID */
    SID_IDENTIFIER_AUTHORITY nt = {{0, 0, 0, 0, 0, 5}};
    PSID adm = NULL;
    CHECK(U(AllocateAndInitializeSid)(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adm));
    if (adm) { CHECK(EqualSid(adm, G0_SID(g0_sid_admins))); FreeSid(adm); }
    PSID wld = NULL;
    SID_IDENTIFIER_AUTHORITY w = {{0, 0, 0, 0, 0, 1}};
    CHECK(U(AllocateAndInitializeSid)(&w, 1, 0, 0, 0, 0, 0, 0, 0, 0, &wld));
    if (wld) { CHECK(EqualSid(wld, G0_SID(g0_sid_world))); FreeSid(wld); }
    /* the authority is 48-bit: both the high dword and the low word are significant */
    SID_IDENTIFIER_AUTHORITY big = {{0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC}};
    PSID b2 = NULL;
    CHECK(U(AllocateAndInitializeSid)(&big, 1, 7, 0, 0, 0, 0, 0, 0, 0, &b2));
    if (b2) { BYTE *p = (BYTE *)b2; CHECK(!memcmp(p + 2, big.Value, 6)); FreeSid(b2); }
}

TEST(gap0_alloc_sid_invalid_count) {
    REQUIRE(U(AllocateAndInitializeSid));
    SID_IDENTIFIER_AUTHORITY nt = {{0, 0, 0, 0, 0, 5}};
    int counts[] = {9, 10, 15, 16, 17, 100, 127, 128, 200, 255};
    for (unsigned i = 0; i < sizeof counts / sizeof *counts; i++) {
        PSID sid = (PSID)(ULONG_PTR)0x1111;
        SET_ERR(0);
        BOOL r = U(AllocateAndInitializeSid)(&nt, (BYTE)counts[i], 1, 2, 3, 4, 5, 6, 7, 8, &sid);
        CHECKF(!r, "count %d", counts[i]);
        EXPECT_ERR(ERROR_INVALID_SID);
        EXPECT_EQ(sid, 0x1111);                     /* output untouched on failure */
    }
}

TEST(gap0_alloc_sid_ignores_unused_subauthorities) {
    REQUIRE(U(AllocateAndInitializeSid));
    SID_IDENTIFIER_AUTHORITY nt = {{0, 0, 0, 0, 0, 5}};
    PSID s1 = NULL, s2 = NULL;
    CHECK(U(AllocateAndInitializeSid)(&nt, 2, 32, 544, 0, 0, 0, 0, 0, 0, &s1));
    CHECK(U(AllocateAndInitializeSid)(&nt, 2, 32, 544, 9, 9, 9, 9, 9, 9, &s2));
    if (s1 && s2) { CHECK(EqualSid(s1, s2)); EXPECT_NE(s1, s2); EXPECT_EQ(GetLengthSid(s2), 16); }
    if (s1) FreeSid(s1);
    if (s2) FreeSid(s2);
}

TEST(gap0_alloc_sid_extreme_values) {
    REQUIRE(U(AllocateAndInitializeSid));
    SID_IDENTIFIER_AUTHORITY a = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
    PSID sid = NULL;
    CHECK(U(AllocateAndInitializeSid)(&a, 8, 0xFFFFFFFF, 0, 0x80000000, 1, 0x7FFFFFFF, 0xFFFFFFFE, 2, 0xFFFFFFFF, &sid));
    if (!sid) return;
    CHECK(IsValidSid(sid));
    EXPECT_EQ(*GetSidSubAuthority(sid, 0), 0xFFFFFFFF);
    EXPECT_EQ(*GetSidSubAuthority(sid, 2), 0x80000000);
    EXPECT_EQ(*GetSidSubAuthority(sid, 7), 0xFFFFFFFF);
    EXPECT_EQ(GetLengthSid(sid), 40);
    LPSTR str = NULL;
    if (ConvertSidToStringSidA(sid, &str)) { t_note("extreme SID string: %s", str); LocalFree(str); }
    FreeSid(sid);
}

TEST(gap0_alloc_sid_many_distinct) {
    REQUIRE(U(AllocateAndInitializeSid));
    SID_IDENTIFIER_AUTHORITY nt = {{0, 0, 0, 0, 0, 5}};
    PSID s[64];
    for (int i = 0; i < 64; i++) {
        s[i] = NULL;
        CHECK(U(AllocateAndInitializeSid)(&nt, 1, i, 0, 0, 0, 0, 0, 0, 0, &s[i]));
    }
    for (int i = 0; i < 64; i++) for (int j = i + 1; j < 64; j++) if (s[i] && s[j] && s[i] == s[j]) { CHECKF(0, "aliased %d %d", i, j); }
    for (int i = 0; i < 64; i++) if (s[i]) { EXPECT_EQ(*GetSidSubAuthority(s[i], 0), i); FreeSid(s[i]); }
}

TEST(gap0_alloc_sid_memory_is_freeable_with_freesid) {
    REQUIRE(U(AllocateAndInitializeSid));
    SID_IDENTIFIER_AUTHORITY nt = {{0, 0, 0, 0, 0, 5}};
    for (int i = 0; i < 2000; i++) {
        PSID s = NULL;
        if (!U(AllocateAndInitializeSid)(&nt, (BYTE)(i % 9), 1, 2, 3, 4, 5, 6, 7, 8, &s) || !s) { CHECKF(0, "iter %d", i); return; }
        FreeSid(s);
    }
    CHECK(1);
}

/* ------------------------------------------- AllocateLocallyUniqueId */
static LONG64 luid64(LUID l) { return ((LONG64)l.HighPart << 32) | l.LowPart; }

TEST(gap0_luid_monotonic_unique) {
    REQUIRE(U(AllocateLocallyUniqueId));
    LUID prev = {0, 0};
    BOOL (WINAPI *fn)(PLUID) = U(AllocateLocallyUniqueId);
    for (int i = 0; i < 2000; i++) {
        LUID l = {0xdead, 0xbeef};
        SET_ERR(0x1234);
        BOOL r = fn(&l);
        if (!r) { CHECKF(0, "iter %d err %lu", i, GetLastError()); return; }
        if (i == 0) EXPECT_EQ(GetLastError(), 0x1234);
        if (i > 0 && luid64(l) <= luid64(prev)) { CHECKF(0, "not increasing at %d: %llx <= %llx", i, luid64(l), luid64(prev)); return; }
        if (i == 0) { CHECK(luid64(l) > 0x3ea); }                       /* below 1000 are reserved */
        prev = l;
    }
    CHECK(1);
    EXPECT_EQ(prev.HighPart, 0);
}

TEST(gap0_luid_null_pointer) {
    REQUIRE(U(AllocateLocallyUniqueId));
    SET_ERR(0);
    BOOL r = U(AllocateLocallyUniqueId)(NULL);
    CHECK(!r);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_NOACCESS || e == ERROR_INVALID_PARAMETER, "err %lu", e);
    t_note("AllocateLocallyUniqueId(NULL) -> %lu", e);
}

struct luid_thr { LUID l[500]; };
static DWORD WINAPI luid_thread(LPVOID p) {
    struct luid_thr *t = (struct luid_thr *)p;
    for (int i = 0; i < 500; i++) { if (!U(AllocateLocallyUniqueId)(&t->l[i])) return 1; }
    return 0;
}
static int cmp64(const void *a, const void *b) { LONG64 x = *(const LONG64 *)a, y = *(const LONG64 *)b; return x < y ? -1 : x > y; }

TEST(gap0_luid_threads_unique) {
    REQUIRE(U(AllocateLocallyUniqueId));
    enum { NT = 4 };
    static struct luid_thr data[NT];
    HANDLE th[NT];
    for (int i = 0; i < NT; i++) th[i] = CreateThread(NULL, 0, luid_thread, &data[i], 0, NULL);
    for (int i = 0; i < NT; i++) { CHECK(th[i] != NULL); if (th[i]) { EXPECT_EQ(WaitForSingleObject(th[i], 20000), WAIT_OBJECT_0); DWORD ec = 9; GetExitCodeThread(th[i], &ec); EXPECT_EQ(ec, 0); CloseHandle(th[i]); } }
    static LONG64 all[NT * 500];
    for (int i = 0; i < NT; i++) for (int k = 0; k < 500; k++) all[i * 500 + k] = luid64(data[i].l[k]);
    /* per-thread sequences are increasing */
    for (int i = 0; i < NT; i++) for (int k = 1; k < 500; k++) if (all[i * 500 + k] <= all[i * 500 + k - 1]) { CHECKF(0, "thread %d not increasing", i); break; }
    qsort(all, NT * 500, sizeof all[0], cmp64);
    int dup = 0;
    for (int i = 1; i < NT * 500; i++) if (all[i] == all[i - 1]) dup++;
    EXPECT_EQ(dup, 0);
}

TEST(gap0_luid_differs_from_session_luids) {
    REQUIRE(U(AllocateLocallyUniqueId));
    HANDLE p = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &p)) { CHECK(0); return; }
    TOKEN_STATISTICS ts; DWORD n = 0;
    CHECK(GetTokenInformation(p, TokenStatistics, &ts, sizeof ts, &n));
    LUID l; CHECK(U(AllocateLocallyUniqueId)(&l));
    CHECK(luid64(l) != luid64(ts.TokenId));
    CHECK(luid64(l) != luid64(ts.AuthenticationId));
    CloseHandle(p);
}
