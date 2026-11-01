/* DuplicateToken, DuplicateTokenEx, CreateRestrictedToken */
#include "g2.h"

typedef BOOL (WINAPI *dupex_fn)(HANDLE, DWORD, LPSECURITY_ATTRIBUTES, SECURITY_IMPERSONATION_LEVEL, TOKEN_TYPE, PHANDLE);
typedef BOOL (WINAPI *dup_fn)(HANDLE, SECURITY_IMPERSONATION_LEVEL, PHANDLE);
typedef BOOL (WINAPI *crt_fn)(HANDLE, DWORD, DWORD, PSID_AND_ATTRIBUTES, DWORD, PLUID_AND_ATTRIBUTES, DWORD, PSID_AND_ATTRIBUTES, PHANDLE);

static void *tok_info(HANDLE t, TOKEN_INFORMATION_CLASS c)
{
    DWORD n = 0; void *b;
    GetTokenInformation(t, c, NULL, 0, &n);
    if (!n) return NULL;
    b = malloc(n);
    if (b && !GetTokenInformation(t, c, b, n, &n)) { free(b); return NULL; }
    return b;
}
static int tok_type(HANDLE t)
{
    DWORD v = 0, n;
    if (!GetTokenInformation(t, TokenType, &v, sizeof v, &n)) return -1;
    return (int)v;
}
static int tok_level(HANDLE t)
{
    DWORD v = 0xFF, n;
    if (!GetTokenInformation(t, TokenImpersonationLevel, &v, sizeof v, &n)) return -1;
    return (int)v;
}

/* ---------------- DuplicateTokenEx ---------------- */
TEST(gap2_dupex_primary) {
    dupex_fn f = (dupex_fn)t_proc("DuplicateTokenEx");
    HANDLE src, n = NULL;
    REQUIRE(f);
    src = g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY);
    CHECK(src != NULL); if (!src) return;
    CHECK(f(src, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenPrimary, &n));
    CHECK(n != NULL && n != INVALID_HANDLE_VALUE);
    if (n) { EXPECT_EQ(tok_type(n), TokenPrimary); CloseHandle(n); }
    CloseHandle(src);
}
TEST(gap2_dupex_impersonation_levels) {
    dupex_fn f = (dupex_fn)t_proc("DuplicateTokenEx");
    HANDLE src; int lvl;
    REQUIRE(f);
    src = g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY);
    CHECK(src != NULL); if (!src) return;
    for (lvl = SecurityAnonymous; lvl <= SecurityDelegation; lvl++) {
        HANDLE n = NULL;
        BOOL ok = f(src, MAXIMUM_ALLOWED, NULL, (SECURITY_IMPERSONATION_LEVEL)lvl, TokenImpersonation, &n);
        CHECKF(ok, "level %d err %lu", lvl, GetLastError());
        if (ok) {
            EXPECT_EQ(tok_type(n), TokenImpersonation);
            EXPECT_EQ(tok_level(n), lvl);
            CloseHandle(n);
        }
    }
    CloseHandle(src);
}
TEST(gap2_dupex_primary_ignores_level) {
    dupex_fn f = (dupex_fn)t_proc("DuplicateTokenEx");
    HANDLE src, n = NULL;
    REQUIRE(f);
    src = g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY);
    if (!src) { CHECK(0); return; }
    BOOL ok = f(src, MAXIMUM_ALLOWED, NULL, SecurityAnonymous, TokenPrimary, &n);
    CHECK(ok);
    if (ok) { EXPECT_EQ(tok_type(n), TokenPrimary); CloseHandle(n); }
    CloseHandle(src);
}
TEST(gap2_dupex_access_zero_inherits_source_access) {
    dupex_fn f = (dupex_fn)t_proc("DuplicateTokenEx");
    HANDLE src, n = NULL;
    REQUIRE(f);
    src = g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY);
    if (!src) { CHECK(0); return; }
    DWORD sa = g2_granted(src);
    CHECK(f(src, 0, NULL, SecurityImpersonation, TokenImpersonation, &n));
    if (n) { EXPECT_EQ(g2_granted(n), sa); CloseHandle(n); }
    CloseHandle(src);
}
TEST(gap2_dupex_exact_access) {
    dupex_fn f = (dupex_fn)t_proc("DuplicateTokenEx");
    static const DWORD masks[] = { TOKEN_QUERY, TOKEN_QUERY | TOKEN_DUPLICATE, TOKEN_IMPERSONATE,
        TOKEN_QUERY | TOKEN_IMPERSONATE | TOKEN_DUPLICATE, TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
        TOKEN_ASSIGN_PRIMARY | TOKEN_QUERY, READ_CONTROL, TOKEN_READ };
    HANDLE src; size_t i;
    REQUIRE(f);
    src = g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY);
    if (!src) { CHECK(0); return; }
    for (i = 0; i < sizeof masks / sizeof masks[0]; i++) {
        HANDLE n = NULL;
        BOOL ok = f(src, masks[i], NULL, SecurityImpersonation, TokenPrimary, &n);
        CHECKF(ok, "mask 0x%lx err %lu", masks[i], GetLastError());
        if (ok) { EXPECT_EQ(g2_granted(n), masks[i]); CloseHandle(n); }
    }
    CloseHandle(src);
}
TEST(gap2_dupex_max_allowed_has_requested_rights) {
    dupex_fn f = (dupex_fn)t_proc("DuplicateTokenEx");
    HANDLE src, n = NULL;
    REQUIRE(f);
    src = g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY);
    if (!src) { CHECK(0); return; }
    CHECK(f(src, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenImpersonation, &n));
    if (n) {
        DWORD g = g2_granted(n);
        CHECK((g & (TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_IMPERSONATE)) == (TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_IMPERSONATE));
        t_note("MAXIMUM_ALLOWED token access 0x%lx", g);
        CloseHandle(n);
    }
    CloseHandle(src);
}
TEST(gap2_dupex_source_needs_duplicate_right) {
    dupex_fn f = (dupex_fn)t_proc("DuplicateTokenEx");
    HANDLE src, n = (HANDLE)0x1234;
    REQUIRE(f);
    src = g2_open_proc_token(TOKEN_QUERY);
    if (!src) { CHECK(0); return; }
    SET_ERR(0);
    CHECK(!f(src, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenPrimary, &n));
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CHECK(n == (HANDLE)0x1234 || n == NULL);
    CloseHandle(src);
}
TEST(gap2_dupex_invalid_source_handles) {
    dupex_fn f = (dupex_fn)t_proc("DuplicateTokenEx");
    HANDLE n; HANDLE ev, closed;
    REQUIRE(f);
    n = NULL; SET_ERR(0);
    CHECK(!f(NULL, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenPrimary, &n));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CHECK(n == NULL);
    n = NULL; SET_ERR(0);
    CHECK(!f(INVALID_HANDLE_VALUE, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenPrimary, &n));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    n = NULL; SET_ERR(0);
    CHECK(!f((HANDLE)(LONG_PTR)-2, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenPrimary, &n));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    n = NULL; SET_ERR(0);
    CHECK(!f(ev, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenPrimary, &n));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
    closed = g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY);
    CloseHandle(closed);
    n = NULL; SET_ERR(0);
    CHECK(!f(closed, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenPrimary, &n));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    n = NULL; SET_ERR(0);
    CHECK(!f((HANDLE)(ULONG_PTR)0x7FFFFFF0, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenPrimary, &n));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap2_dupex_process_handle_is_wrong_type) {
    dupex_fn f = (dupex_fn)t_proc("DuplicateTokenEx");
    HANDLE n = NULL, p;
    REQUIRE(f);
    p = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, GetCurrentProcessId());
    if (!p) { t_skip("OpenProcess failed"); return; }
    SET_ERR(0);
    CHECK(!f(p, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenPrimary, &n));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(p);
}
TEST(gap2_dupex_invalid_token_type) {
    dupex_fn f = (dupex_fn)t_proc("DuplicateTokenEx");
    static const int types[] = { 0, 3, 4, 100, -1 };
    HANDLE src; size_t i;
    REQUIRE(f);
    src = g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY);
    if (!src) { CHECK(0); return; }
    for (i = 0; i < sizeof types / sizeof types[0]; i++) {
        HANDLE n = NULL; SET_ERR(0);
        BOOL ok = f(src, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, (TOKEN_TYPE)types[i], &n);
        if (ok) t_note("token type %d accepted", types[i]);
        t_note("token type %d: err=%lu", types[i], GetLastError());
        if (ok) CloseHandle(n);
    }
    CloseHandle(src);
}
TEST(gap2_dupex_invalid_impersonation_level) {
    dupex_fn f = (dupex_fn)t_proc("DuplicateTokenEx");
    static const int lv[] = { 4, 5, 100, -1 };
    HANDLE src; size_t i;
    REQUIRE(f);
    src = g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY);
    if (!src) { CHECK(0); return; }
    for (i = 0; i < sizeof lv / sizeof lv[0]; i++) {
        HANDLE n = NULL; SET_ERR(0);
        BOOL ok = f(src, MAXIMUM_ALLOWED, NULL, (SECURITY_IMPERSONATION_LEVEL)lv[i], TokenImpersonation, &n);
        if (ok) t_note("level %d accepted", lv[i]);
        t_note("impersonation level %d: err=%lu", lv[i], GetLastError());
        if (ok) CloseHandle(n);
    }
    CloseHandle(src);
}
TEST(gap2_dupex_inherit_attribute) {
    dupex_fn f = (dupex_fn)t_proc("DuplicateTokenEx");
    SECURITY_ATTRIBUTES sa; HANDLE src, n = NULL; DWORD fl = 0;
    REQUIRE(f);
    src = g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY);
    if (!src) { CHECK(0); return; }
    memset(&sa, 0, sizeof sa); sa.nLength = sizeof sa; sa.bInheritHandle = TRUE;
    CHECK(f(src, MAXIMUM_ALLOWED, &sa, SecurityImpersonation, TokenPrimary, &n));
    if (n) { CHECK(GetHandleInformation(n, &fl)); CHECK(fl & HANDLE_FLAG_INHERIT); CloseHandle(n); }
    n = NULL; sa.bInheritHandle = FALSE;
    CHECK(f(src, MAXIMUM_ALLOWED, &sa, SecurityImpersonation, TokenPrimary, &n));
    if (n) { CHECK(GetHandleInformation(n, &fl)); CHECK(!(fl & HANDLE_FLAG_INHERIT)); CloseHandle(n); }
    n = NULL;
    CHECK(f(src, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenPrimary, &n));
    if (n) { CHECK(GetHandleInformation(n, &fl)); CHECK(!(fl & HANDLE_FLAG_INHERIT)); CloseHandle(n); }
    CloseHandle(src);
}
TEST(gap2_dupex_security_attributes_null_dacl) {
    dupex_fn f = (dupex_fn)t_proc("DuplicateTokenEx");
    SECURITY_ATTRIBUTES sa; SECURITY_DESCRIPTOR sd; HANDLE src, n = NULL;
    REQUIRE(f);
    src = g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY);
    if (!src) { CHECK(0); return; }
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
    memset(&sa, 0, sizeof sa); sa.nLength = sizeof sa; sa.lpSecurityDescriptor = &sd;
    CHECK(f(src, MAXIMUM_ALLOWED, &sa, SecurityImpersonation, TokenPrimary, &n));
    if (n) CloseHandle(n);
    CloseHandle(src);
}
TEST(gap2_dupex_same_user_and_groups) {
    dupex_fn f = (dupex_fn)t_proc("DuplicateTokenEx");
    HANDLE src, n = NULL; TOKEN_USER *u1, *u2; TOKEN_GROUPS *g1, *g2;
    REQUIRE(f);
    src = g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY);
    if (!src) { CHECK(0); return; }
    CHECK(f(src, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenImpersonation, &n));
    if (n) {
        u1 = tok_info(src, TokenUser); u2 = tok_info(n, TokenUser);
        CHECK(u1 && u2 && EqualSid(u1->User.Sid, u2->User.Sid));
        g1 = tok_info(src, TokenGroups); g2 = tok_info(n, TokenGroups);
        CHECK(g1 && g2 && g1->GroupCount == g2->GroupCount);
        free(u1); free(u2); free(g1); free(g2);
        CloseHandle(n);
    }
    CloseHandle(src);
}
TEST(gap2_dupex_same_session_and_authid) {
    dupex_fn f = (dupex_fn)t_proc("DuplicateTokenEx");
    HANDLE src, n = NULL; TOKEN_STATISTICS *s1, *s2; DWORD a = 0, b = 0, r;
    REQUIRE(f);
    src = g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY);
    if (!src) { CHECK(0); return; }
    CHECK(f(src, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenPrimary, &n));
    if (n) {
        s1 = tok_info(src, TokenStatistics); s2 = tok_info(n, TokenStatistics);
        CHECK(s1 && s2);
        if (s1 && s2) {
            CHECK(s1->AuthenticationId.LowPart == s2->AuthenticationId.LowPart && s1->AuthenticationId.HighPart == s2->AuthenticationId.HighPart);
            CHECK(s1->PrivilegeCount == s2->PrivilegeCount);
            CHECK(s1->GroupCount == s2->GroupCount);
            CHECK(s1->TokenId.LowPart != s2->TokenId.LowPart || s1->TokenId.HighPart != s2->TokenId.HighPart);
        }
        CHECK(GetTokenInformation(src, TokenSessionId, &a, sizeof a, &r));
        CHECK(GetTokenInformation(n, TokenSessionId, &b, sizeof b, &r));
        EXPECT_EQ(a, b);
        free(s1); free(s2);
        CloseHandle(n);
    }
    CloseHandle(src);
}
TEST(gap2_dupex_cannot_raise_impersonation_level) {
    dupex_fn f = (dupex_fn)t_proc("DuplicateTokenEx");
    HANDLE src, ident = NULL, n = NULL;
    REQUIRE(f);
    src = g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY);
    if (!src) { CHECK(0); return; }
    CHECK(f(src, MAXIMUM_ALLOWED, NULL, SecurityIdentification, TokenImpersonation, &ident));
    if (ident) {
        SET_ERR(0);
        CHECK(!f(ident, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenImpersonation, &n));
        EXPECT_ERR(ERROR_BAD_IMPERSONATION_LEVEL);
        SET_ERR(0);
        CHECK(!f(ident, MAXIMUM_ALLOWED, NULL, SecurityDelegation, TokenImpersonation, &n));
        EXPECT_ERR(ERROR_BAD_IMPERSONATION_LEVEL);
        /* lowering or keeping is fine */
        n = NULL;
        CHECK(f(ident, MAXIMUM_ALLOWED, NULL, SecurityIdentification, TokenImpersonation, &n));
        if (n) { EXPECT_EQ(tok_level(n), SecurityIdentification); CloseHandle(n); }
        n = NULL;
        CHECK(f(ident, MAXIMUM_ALLOWED, NULL, SecurityAnonymous, TokenImpersonation, &n));
        if (n) { EXPECT_EQ(tok_level(n), SecurityAnonymous); CloseHandle(n); }
        CloseHandle(ident);
    }
    CloseHandle(src);
}
TEST(gap2_dupex_impersonation_to_primary_needs_level) {
    dupex_fn f = (dupex_fn)t_proc("DuplicateTokenEx");
    HANDLE src, imp = NULL, n = NULL; int lvl;
    REQUIRE(f);
    src = g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY);
    if (!src) { CHECK(0); return; }
    for (lvl = SecurityAnonymous; lvl <= SecurityDelegation; lvl++) {
        imp = NULL;
        CHECK(f(src, MAXIMUM_ALLOWED, NULL, (SECURITY_IMPERSONATION_LEVEL)lvl, TokenImpersonation, &imp));
        if (!imp) continue;
        n = NULL; SET_ERR(0);
        BOOL ok = f(imp, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenPrimary, &n);
        if (lvl < SecurityImpersonation) {
            CHECKF(!ok, "level %d -> primary succeeded", lvl);
            EXPECT_ERR(ERROR_BAD_IMPERSONATION_LEVEL);
        } else {
            CHECKF(ok, "level %d -> primary failed err %lu", lvl, GetLastError());
            if (ok) EXPECT_EQ(tok_type(n), TokenPrimary);
        }
        if (n) CloseHandle(n);
        CloseHandle(imp);
    }
    CloseHandle(src);
}
TEST(gap2_dupex_dup_of_dup) {
    dupex_fn f = (dupex_fn)t_proc("DuplicateTokenEx");
    HANDLE src, a = NULL, b = NULL, c = NULL;
    REQUIRE(f);
    src = g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY);
    if (!src) { CHECK(0); return; }
    CHECK(f(src, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenPrimary, &a));
    if (a) CHECK(f(a, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenPrimary, &b));
    if (b) CHECK(f(b, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenImpersonation, &c));
    if (c) { EXPECT_EQ(tok_type(c), TokenImpersonation); CloseHandle(c); }
    if (b) CloseHandle(b);
    if (a) CloseHandle(a);
    CloseHandle(src);
}
TEST(gap2_dupex_independent_handles) {
    dupex_fn f = (dupex_fn)t_proc("DuplicateTokenEx");
    HANDLE src, a = NULL, b = NULL;
    REQUIRE(f);
    src = g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY);
    if (!src) { CHECK(0); return; }
    CHECK(f(src, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenPrimary, &a));
    CHECK(f(src, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenPrimary, &b));
    CHECK(a != b);
    if (a && b) {
        TOKEN_STATISTICS *s1 = tok_info(a, TokenStatistics), *s2 = tok_info(b, TokenStatistics);
        CHECK(s1 && s2 && (s1->TokenId.LowPart != s2->TokenId.LowPart || s1->TokenId.HighPart != s2->TokenId.HighPart));
        free(s1); free(s2);
    }
    if (a) { CHECK(CloseHandle(a)); }
    if (b) { TOKEN_STATISTICS *s = tok_info(b, TokenStatistics); CHECK(s != NULL); free(s); CloseHandle(b); }
    CloseHandle(src);
}
TEST(gap2_dupex_stress_no_leak) {
    dupex_fn f = (dupex_fn)t_proc("DuplicateTokenEx");
    HANDLE src; int i;
    REQUIRE(f);
    src = g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY);
    if (!src) { CHECK(0); return; }
    for (i = 0; i < 500; i++) {
        HANDLE n = NULL;
        if (!f(src, TOKEN_QUERY, NULL, SecurityImpersonation, TokenImpersonation, &n)) { CHECK(0); break; }
        CloseHandle(n);
    }
    CloseHandle(src);
}
TEST(gap2_dupex_usable_as_thread_token) {
    dupex_fn f = (dupex_fn)t_proc("DuplicateTokenEx");
    HANDLE src, imp = NULL, tt = NULL;
    REQUIRE(f);
    src = g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY);
    if (!src) { CHECK(0); return; }
    CHECK(f(src, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenImpersonation, &imp));
    if (imp) {
        CHECK(SetThreadToken(NULL, imp));
        CHECK(OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &tt));
        if (tt) { EXPECT_EQ(tok_type(tt), TokenImpersonation); CloseHandle(tt); }
        CHECK(SetThreadToken(NULL, NULL));
        SET_ERR(0);
        CHECK(!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &tt));
        EXPECT_ERR(ERROR_NO_TOKEN);
        CloseHandle(imp);
    }
    CloseHandle(src);
}
TEST(gap2_dupex_primary_token_cannot_be_thread_token) {
    dupex_fn f = (dupex_fn)t_proc("DuplicateTokenEx");
    HANDLE src, p = NULL;
    REQUIRE(f);
    src = g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY);
    if (!src) { CHECK(0); return; }
    CHECK(f(src, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenPrimary, &p));
    if (p) {
        SET_ERR(0);
        BOOL ok = SetThreadToken(NULL, p);
        if (ok) { SetThreadToken(NULL, NULL); t_note("SetThreadToken accepted a primary token (converted)"); }
        else t_note("SetThreadToken(primary) failed err=%lu", GetLastError());
        CloseHandle(p);
    }
    CloseHandle(src);
}
TEST(gap2_dupex_null_out_handle) {
    dupex_fn f = (dupex_fn)t_proc("DuplicateTokenEx");
    HANDLE src;
    REQUIRE(f);
    src = g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY);
    if (!src) { CHECK(0); return; }
    SET_ERR(0);
    /* the output pointer is probed by the kernel, so this fails cleanly rather than faulting */
    BOOL ok = f(src, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenPrimary, NULL);
    CHECK(!ok);
    t_note("NULL phNewToken: err=%lu", GetLastError());
    CloseHandle(src);
}

/* ---------------- DuplicateToken ---------------- */
TEST(gap2_dup_basic) {
    dup_fn f = (dup_fn)t_proc("DuplicateToken");
    HANDLE src, n = NULL;
    REQUIRE(f);
    src = g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY);
    if (!src) { CHECK(0); return; }
    CHECK(f(src, SecurityImpersonation, &n));
    if (n) {
        EXPECT_EQ(tok_type(n), TokenImpersonation);
        EXPECT_EQ(tok_level(n), SecurityImpersonation);
        EXPECT_EQ(g2_granted(n), TOKEN_QUERY | TOKEN_IMPERSONATE);
        CloseHandle(n);
    }
    CloseHandle(src);
}
TEST(gap2_dup_levels) {
    dup_fn f = (dup_fn)t_proc("DuplicateToken");
    HANDLE src; int lvl;
    REQUIRE(f);
    src = g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY);
    if (!src) { CHECK(0); return; }
    for (lvl = SecurityAnonymous; lvl <= SecurityDelegation; lvl++) {
        HANDLE n = NULL;
        CHECKF(f(src, (SECURITY_IMPERSONATION_LEVEL)lvl, &n), "level %d", lvl);
        if (n) { EXPECT_EQ(tok_level(n), lvl); EXPECT_EQ(tok_type(n), TokenImpersonation); CloseHandle(n); }
    }
    CloseHandle(src);
}
TEST(gap2_dup_invalid_level) {
    dup_fn f = (dup_fn)t_proc("DuplicateToken");
    HANDLE src, n = NULL;
    REQUIRE(f);
    src = g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY);
    if (!src) { CHECK(0); return; }
    SET_ERR(0);
    BOOL ok = f(src, (SECURITY_IMPERSONATION_LEVEL)4, &n);
    t_note("DuplicateToken level 4: ok=%d err=%lu", ok, GetLastError());
    if (ok) { CloseHandle(n); n = NULL; }
    ok = f(src, (SECURITY_IMPERSONATION_LEVEL)-1, &n);
    t_note("DuplicateToken level -1: ok=%d", ok);
    if (ok) CloseHandle(n);
    CloseHandle(src);
}
TEST(gap2_dup_invalid_handles) {
    dup_fn f = (dup_fn)t_proc("DuplicateToken");
    HANDLE n = NULL, ev;
    REQUIRE(f);
    SET_ERR(0);
    CHECK(!f(NULL, SecurityImpersonation, &n));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(!f(INVALID_HANDLE_VALUE, SecurityImpersonation, &n));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    CHECK(!f(ev, SecurityImpersonation, &n));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
    CHECK(n == NULL);
}
TEST(gap2_dup_needs_duplicate_right) {
    dup_fn f = (dup_fn)t_proc("DuplicateToken");
    HANDLE src, n = NULL;
    REQUIRE(f);
    src = g2_open_proc_token(TOKEN_QUERY);
    if (!src) { CHECK(0); return; }
    SET_ERR(0);
    CHECK(!f(src, SecurityImpersonation, &n));
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(src);
}
TEST(gap2_dup_result_cannot_be_duplicated_again) {
    dup_fn f = (dup_fn)t_proc("DuplicateToken");
    HANDLE src, n = NULL, m = NULL;
    REQUIRE(f);
    src = g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY);
    if (!src) { CHECK(0); return; }
    CHECK(f(src, SecurityImpersonation, &n));
    if (n) {
        SET_ERR(0);
        /* the duplicate only holds TOKEN_QUERY | TOKEN_IMPERSONATE */
        CHECK(!f(n, SecurityImpersonation, &m));
        EXPECT_ERR(ERROR_ACCESS_DENIED);
        CloseHandle(n);
    }
    CloseHandle(src);
}
TEST(gap2_dup_from_impersonation_token) {
    dup_fn f = (dup_fn)t_proc("DuplicateToken");
    dupex_fn fx = (dupex_fn)t_proc("DuplicateTokenEx");
    HANDLE src, imp = NULL, n = NULL;
    REQUIRE(f); REQUIRE(fx);
    src = g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY);
    if (!src) { CHECK(0); return; }
    CHECK(fx(src, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenImpersonation, &imp));
    if (imp) {
        CHECK(f(imp, SecurityIdentification, &n));
        if (n) { EXPECT_EQ(tok_level(n), SecurityIdentification); CloseHandle(n); }
        n = NULL; SET_ERR(0);
        CHECK(!f(imp, SecurityDelegation, &n));      /* cannot raise the level above the source */
        EXPECT_ERR(ERROR_BAD_IMPERSONATION_LEVEL);
        if (n) CloseHandle(n);
        CloseHandle(imp);
    }
    CloseHandle(src);
}
TEST(gap2_dup_matches_dupex_equivalent) {
    dup_fn f = (dup_fn)t_proc("DuplicateToken");
    dupex_fn fx = (dupex_fn)t_proc("DuplicateTokenEx");
    HANDLE src, a = NULL, b = NULL;
    REQUIRE(f); REQUIRE(fx);
    src = g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY);
    if (!src) { CHECK(0); return; }
    CHECK(f(src, SecurityIdentification, &a));
    CHECK(fx(src, TOKEN_QUERY | TOKEN_IMPERSONATE, NULL, SecurityIdentification, TokenImpersonation, &b));
    if (a && b) {
        EXPECT_EQ(g2_granted(a), g2_granted(b));
        EXPECT_EQ(tok_type(a), tok_type(b));
        EXPECT_EQ(tok_level(a), tok_level(b));
    }
    if (a) CloseHandle(a);
    if (b) CloseHandle(b);
    CloseHandle(src);
}
TEST(gap2_dup_not_inheritable) {
    dup_fn f = (dup_fn)t_proc("DuplicateToken");
    HANDLE src, n = NULL; DWORD fl = 0xFF;
    REQUIRE(f);
    src = g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY);
    if (!src) { CHECK(0); return; }
    CHECK(f(src, SecurityImpersonation, &n));
    if (n) { CHECK(GetHandleInformation(n, &fl)); CHECK(!(fl & HANDLE_FLAG_INHERIT)); CloseHandle(n); }
    CloseHandle(src);
}

/* ---------------- CreateRestrictedToken ---------------- */
static HANDLE rt_src(void) { return g2_open_proc_token(TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_ASSIGN_PRIMARY); }

static BOOL has_priv(HANDLE t, DWORD low, DWORD *attr)
{
    TOKEN_PRIVILEGES *p = tok_info(t, TokenPrivileges); DWORD i; BOOL r = FALSE;
    if (!p) return FALSE;
    for (i = 0; i < p->PrivilegeCount; i++)
        if (p->Privileges[i].Luid.LowPart == low && p->Privileges[i].Luid.HighPart == 0) { r = TRUE; if (attr) *attr = p->Privileges[i].Attributes; }
    free(p);
    return r;
}

TEST(gap2_rtok_no_restrictions) {
    crt_fn f = (crt_fn)t_proc("CreateRestrictedToken");
    HANDLE src, n = NULL; TOKEN_USER *u1, *u2;
    REQUIRE(f);
    src = rt_src(); if (!src) { CHECK(0); return; }
    CHECK(f(src, 0, 0, NULL, 0, NULL, 0, NULL, &n));
    if (n) {
        EXPECT_EQ(tok_type(n), TokenPrimary);
        CHECK(!IsTokenRestricted(n));
        u1 = tok_info(src, TokenUser); u2 = tok_info(n, TokenUser);
        CHECK(u1 && u2 && EqualSid(u1->User.Sid, u2->User.Sid));
        free(u1); free(u2);
        CloseHandle(n);
    }
    CloseHandle(src);
}
TEST(gap2_rtok_disable_max_privilege) {
    crt_fn f = (crt_fn)t_proc("CreateRestrictedToken");
    HANDLE src, n = NULL; TOKEN_PRIVILEGES *p; DWORD i;
    REQUIRE(f);
    src = rt_src(); if (!src) { CHECK(0); return; }
    CHECK(f(src, DISABLE_MAX_PRIVILEGE, 0, NULL, 0, NULL, 0, NULL, &n));
    if (n) {
        p = tok_info(n, TokenPrivileges);
        CHECK(p != NULL);
        if (p) {
            for (i = 0; i < p->PrivilegeCount; i++) {
                DWORD low = p->Privileges[i].Luid.LowPart;
                if (low != 23)   /* SeChangeNotifyPrivilege is kept */
                    CHECKF(0, "privilege %lu still present in token", low);
            }
            CHECK(p->PrivilegeCount <= 1);
            free(p);
        }
        CloseHandle(n);
    }
    CloseHandle(src);
}
TEST(gap2_rtok_sandbox_inert) {
    crt_fn f = (crt_fn)t_proc("CreateRestrictedToken");
    HANDLE src, n = NULL; DWORD v = 0, r;
    REQUIRE(f);
    src = rt_src(); if (!src) { CHECK(0); return; }
    CHECK(f(src, SANDBOX_INERT, 0, NULL, 0, NULL, 0, NULL, &n));
    if (n) {
        /* the kernel (SepFilterToken) only sets the flag when the caller runs as LocalSystem */
        TOKEN_USER *tu = tok_info(src, TokenUser); BYTE sysb[16]; int is_sys = 0;
        if (tu) { is_sys = EqualSid(tu->User.Sid, g2_mksid(sysb, 5, 1, 18u)); free(tu); }
        CHECK(GetTokenInformation(n, (TOKEN_INFORMATION_CLASS)15 /* TokenSandBoxInert */, &v, sizeof v, &r));
        CHECK(is_sys ? v != 0 : v == 0);
        CloseHandle(n);
    }
    n = NULL; v = 0;
    CHECK(f(src, 0, 0, NULL, 0, NULL, 0, NULL, &n));
    if (n) {
        CHECK(GetTokenInformation(n, (TOKEN_INFORMATION_CLASS)15, &v, sizeof v, &r));
        CHECK(v == 0);
        CloseHandle(n);
    }
    CloseHandle(src);
}
TEST(gap2_rtok_lua_token) {
    crt_fn f = (crt_fn)t_proc("CreateRestrictedToken");
    HANDLE src, n = NULL;
    REQUIRE(f);
    src = rt_src(); if (!src) { CHECK(0); return; }
    BOOL ok = f(src, LUA_TOKEN, 0, NULL, 0, NULL, 0, NULL, &n);
    t_note("LUA_TOKEN: ok=%d err=%lu", ok, ok ? 0 : GetLastError());
    CHECK(ok);
    if (n) CloseHandle(n);
    CloseHandle(src);
}
TEST(gap2_rtok_restricting_sid) {
    crt_fn f = (crt_fn)t_proc("CreateRestrictedToken");
    HANDLE src, n = NULL; BYTE sidb[16]; SID_AND_ATTRIBUTES sa; TOKEN_GROUPS *rg;
    REQUIRE(f);
    src = rt_src(); if (!src) { CHECK(0); return; }
    sa.Sid = g2_mksid(sidb, 1, 1, 0u); sa.Attributes = 0;
    CHECK(f(src, 0, 0, NULL, 0, NULL, 1, &sa, &n));
    if (n) {
        CHECK(IsTokenRestricted(n));
        rg = tok_info(n, TokenRestrictedSids);
        CHECK(rg != NULL);
        if (rg) { EXPECT_EQ(rg->GroupCount, 1); if (rg->GroupCount) CHECK(EqualSid(rg->Groups[0].Sid, sidb)); free(rg); }
        EXPECT_EQ(tok_type(n), TokenPrimary);
        CloseHandle(n);
    }
    CHECK(!IsTokenRestricted(src));
    CloseHandle(src);
}
TEST(gap2_rtok_several_restricting_sids) {
    crt_fn f = (crt_fn)t_proc("CreateRestrictedToken");
    HANDLE src, n = NULL; BYTE b1[16], b2[16], b3[16]; SID_AND_ATTRIBUTES sa[3]; TOKEN_GROUPS *rg;
    REQUIRE(f);
    src = rt_src(); if (!src) { CHECK(0); return; }
    sa[0].Sid = g2_mksid(b1, 1, 1, 0u);          sa[0].Attributes = 0;
    sa[1].Sid = g2_mksid(b2, 5, 1, 11u);         sa[1].Attributes = 0;
    sa[2].Sid = g2_mksid(b3, 5, 2, 32u, 545u);   sa[2].Attributes = 0;
    CHECK(f(src, 0, 0, NULL, 0, NULL, 3, sa, &n));
    if (n) {
        rg = tok_info(n, TokenRestrictedSids);
        CHECK(rg != NULL);
        if (rg) { EXPECT_EQ(rg->GroupCount, 3); free(rg); }
        CloseHandle(n);
    }
    CloseHandle(src);
}
TEST(gap2_rtok_write_restricted) {
    crt_fn f = (crt_fn)t_proc("CreateRestrictedToken");
    HANDLE src, n = NULL; BYTE sidb[16]; SID_AND_ATTRIBUTES sa;
    REQUIRE(f);
    src = rt_src(); if (!src) { CHECK(0); return; }
    sa.Sid = g2_mksid(sidb, 1, 1, 0u); sa.Attributes = 0;
    CHECK(f(src, WRITE_RESTRICTED, 0, NULL, 0, NULL, 1, &sa, &n));
    if (n) { CHECK(IsTokenRestricted(n)); CloseHandle(n); }
    CloseHandle(src);
}
TEST(gap2_rtok_deny_only_everyone) {
    crt_fn f = (crt_fn)t_proc("CreateRestrictedToken");
    HANDLE src, n = NULL; BYTE sidb[16]; SID_AND_ATTRIBUTES sa; TOKEN_GROUPS *g; DWORD i; int found = 0;
    REQUIRE(f);
    src = rt_src(); if (!src) { CHECK(0); return; }
    sa.Sid = g2_mksid(sidb, 1, 1, 0u); sa.Attributes = 0;
    CHECK(f(src, 0, 1, &sa, 0, NULL, 0, NULL, &n));
    if (n) {
        g = tok_info(n, TokenGroups);
        CHECK(g != NULL);
        for (i = 0; g && i < g->GroupCount; i++)
            if (EqualSid(g->Groups[i].Sid, sidb)) {
                found = 1;
                CHECK(g->Groups[i].Attributes & SE_GROUP_USE_FOR_DENY_ONLY);
                CHECK(!(g->Groups[i].Attributes & SE_GROUP_ENABLED));
            }
        if (!found) t_note("Everyone not present in token groups");
        free(g);
        CloseHandle(n);
    }
    CloseHandle(src);
}
TEST(gap2_rtok_deny_only_is_not_in_source) {
    crt_fn f = (crt_fn)t_proc("CreateRestrictedToken");
    HANDLE src, n = NULL; BYTE sidb[16]; SID_AND_ATTRIBUTES sa;
    REQUIRE(f);
    src = rt_src(); if (!src) { CHECK(0); return; }
    sa.Sid = g2_mksid(sidb, 1, 1, 0u); sa.Attributes = 0;
    CHECK(f(src, 0, 1, &sa, 0, NULL, 0, NULL, &n));
    /* the source token is left unchanged */
    TOKEN_GROUPS *g = tok_info(src, TokenGroups); DWORD i;
    for (i = 0; g && i < g->GroupCount; i++)
        if (EqualSid(g->Groups[i].Sid, sidb)) CHECK(!(g->Groups[i].Attributes & SE_GROUP_USE_FOR_DENY_ONLY));
    free(g);
    if (n) CloseHandle(n);
    CloseHandle(src);
}
TEST(gap2_rtok_delete_privilege) {
    crt_fn f = (crt_fn)t_proc("CreateRestrictedToken");
    HANDLE src, n = NULL; LUID_AND_ATTRIBUTES la;
    REQUIRE(f);
    src = rt_src(); if (!src) { CHECK(0); return; }
    if (!has_priv(src, 23, NULL)) { CloseHandle(src); t_skip("SeChangeNotifyPrivilege missing from token"); return; }
    la.Luid.LowPart = 23; la.Luid.HighPart = 0; la.Attributes = 0;
    CHECK(f(src, 0, 0, NULL, 1, &la, 0, NULL, &n));
    if (n) { CHECK(!has_priv(n, 23, NULL)); CloseHandle(n); }
    CHECK(has_priv(src, 23, NULL));
    CloseHandle(src);
}
TEST(gap2_rtok_delete_unknown_privilege) {
    crt_fn f = (crt_fn)t_proc("CreateRestrictedToken");
    HANDLE src, n = NULL; LUID_AND_ATTRIBUTES la;
    REQUIRE(f);
    src = rt_src(); if (!src) { CHECK(0); return; }
    la.Luid.LowPart = 0x7777; la.Luid.HighPart = 0; la.Attributes = 0;
    SET_ERR(0);
    BOOL ok = f(src, 0, 0, NULL, 1, &la, 0, NULL, &n);
    t_note("delete nonexistent privilege: ok=%d err=%lu", ok, ok ? 0 : GetLastError());
    if (n) CloseHandle(n);
    CloseHandle(src);
}
TEST(gap2_rtok_disable_unknown_sid) {
    crt_fn f = (crt_fn)t_proc("CreateRestrictedToken");
    HANDLE src, n = NULL; BYTE sidb[32]; SID_AND_ATTRIBUTES sa;
    REQUIRE(f);
    src = rt_src(); if (!src) { CHECK(0); return; }
    sa.Sid = g2_mksid(sidb, 5, 3, 21u, 0x1234u, 0x5678u); sa.Attributes = 0;
    SET_ERR(0);
    BOOL ok = f(src, 0, 1, &sa, 0, NULL, 0, NULL, &n);
    t_note("disable SID not in token: ok=%d err=%lu", ok, ok ? 0 : GetLastError());
    if (n) CloseHandle(n);
    CloseHandle(src);
}
TEST(gap2_rtok_combined) {
    crt_fn f = (crt_fn)t_proc("CreateRestrictedToken");
    HANDLE src, n = NULL; BYTE b1[16], b2[16]; SID_AND_ATTRIBUTES d, r; LUID_AND_ATTRIBUTES la;
    REQUIRE(f);
    src = rt_src(); if (!src) { CHECK(0); return; }
    d.Sid = g2_mksid(b1, 1, 1, 0u); d.Attributes = 0;
    r.Sid = g2_mksid(b2, 5, 1, 11u); r.Attributes = 0;
    la.Luid.LowPart = 23; la.Luid.HighPart = 0; la.Attributes = 0;
    CHECK(f(src, DISABLE_MAX_PRIVILEGE | SANDBOX_INERT | WRITE_RESTRICTED, 1, &d, 1, &la, 1, &r, &n));
    if (n) {
        CHECK(IsTokenRestricted(n));
        /* DISABLE_MAX_PRIVILEGE ignores the delete list, so SeChangeNotifyPrivilege (23) stays */
        CHECK(has_priv(n, 23, NULL));
        CloseHandle(n);
    }
    CloseHandle(src);
}
TEST(gap2_rtok_from_impersonation_token) {
    crt_fn f = (crt_fn)t_proc("CreateRestrictedToken");
    dupex_fn fx = (dupex_fn)t_proc("DuplicateTokenEx");
    HANDLE src, imp = NULL, n = NULL;
    REQUIRE(f); REQUIRE(fx);
    src = rt_src(); if (!src) { CHECK(0); return; }
    CHECK(fx(src, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenImpersonation, &imp));
    if (imp) {
        CHECK(f(imp, DISABLE_MAX_PRIVILEGE, 0, NULL, 0, NULL, 0, NULL, &n));
        if (n) { EXPECT_EQ(tok_type(n), TokenImpersonation); CloseHandle(n); }
        CloseHandle(imp);
    }
    CloseHandle(src);
}
TEST(gap2_rtok_chain) {
    crt_fn f = (crt_fn)t_proc("CreateRestrictedToken");
    HANDLE src, a = NULL, b = NULL; BYTE sidb[16]; SID_AND_ATTRIBUTES sa;
    REQUIRE(f);
    src = rt_src(); if (!src) { CHECK(0); return; }
    sa.Sid = g2_mksid(sidb, 1, 1, 0u); sa.Attributes = 0;
    CHECK(f(src, 0, 0, NULL, 0, NULL, 1, &sa, &a));
    if (a) {
        /* A restricted source filtered without any restricting SID yields a token with no
           restricting SIDs; SepFilterToken rejects that (STATUS_INVALID_PARAMETER). */
        SET_ERR(0);
        CHECK(!f(a, DISABLE_MAX_PRIVILEGE, 0, NULL, 0, NULL, 0, NULL, &b));
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        if (b) { CloseHandle(b); b = NULL; }
        /* Re-supplying a restricting SID that is already in the source's restricting set works */
        SET_ERR(0);
        BOOL okb = f(a, DISABLE_MAX_PRIVILEGE, 0, NULL, 0, NULL, 1, &sa, &b);
        if (!okb) t_note("restricted-of-restricted failed: err=%lu, source handle access 0x%lx", GetLastError(), g2_granted(a));
        CHECK(okb);
        if (b) { CHECK(IsTokenRestricted(b)); CloseHandle(b); }
        CloseHandle(a);
    }
    CloseHandle(src);
}
TEST(gap2_rtok_null_arrays_with_counts) {
    crt_fn f = (crt_fn)t_proc("CreateRestrictedToken");
    HANDLE src, n = NULL;
    REQUIRE(f);
    if (t_is_wine()) { t_skip("emulator faults on NULL arrays"); return; }
    src = rt_src(); if (!src) { CHECK(0); return; }
    SET_ERR(0);
    CHECK(!f(src, 0, 1, NULL, 0, NULL, 0, NULL, &n));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!f(src, 0, 0, NULL, 1, NULL, 0, NULL, &n));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!f(src, 0, 0, NULL, 0, NULL, 1, NULL, &n));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!f(src, 0, 5, NULL, 5, NULL, 5, NULL, &n));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CHECK(n == NULL);
    CloseHandle(src);
}
TEST(gap2_rtok_count_overflow) {
    crt_fn f = (crt_fn)t_proc("CreateRestrictedToken");
    HANDLE src, n = NULL; SID_AND_ATTRIBUTES sa; LUID_AND_ATTRIBUTES la; BYTE sidb[16];
    static const DWORD counts[] = { 0xFFFFFFFF, 0x80000000, 0x10000001 };
    size_t i;
    REQUIRE(f);
    if (t_is_wine()) { t_skip("emulator reads the arrays"); return; }
    src = rt_src(); if (!src) { CHECK(0); return; }
    sa.Sid = g2_mksid(sidb, 1, 1, 0u); sa.Attributes = 0;
    la.Luid.LowPart = 23; la.Luid.HighPart = 0; la.Attributes = 0;
    for (i = 0; i < sizeof counts / sizeof counts[0]; i++) {
        /* counts too large to describe in a 32-bit allocation: rejected before any copy */
        SET_ERR(0);
        CHECKF(!f(src, 0, counts[i], &sa, 0, NULL, 0, NULL, &n), "disable count 0x%lx", counts[i]);
        EXPECT_ERR(ERROR_ARITHMETIC_OVERFLOW);
        SET_ERR(0);
        CHECKF(!f(src, 0, 0, NULL, 0, NULL, counts[i], &sa, &n), "restrict count 0x%lx", counts[i]);
        EXPECT_ERR(ERROR_ARITHMETIC_OVERFLOW);
        /* delete array size is (count-1)*12: 0x10000001 still fits in 32 bits (3.2 GB alloc + copy), so skip it */
        if (counts[i] == 0x10000001) continue;
        SET_ERR(0);
        CHECKF(!f(src, 0, 0, NULL, counts[i], &la, 0, NULL, &n), "delete count 0x%lx", counts[i]);
        EXPECT_ERR(ERROR_ARITHMETIC_OVERFLOW);
    }
    CHECK(n == NULL);
    CloseHandle(src);
}
TEST(gap2_rtok_invalid_source_handles) {
    crt_fn f = (crt_fn)t_proc("CreateRestrictedToken");
    HANDLE n = NULL, ev;
    REQUIRE(f);
    SET_ERR(0);
    CHECK(!f(NULL, 0, 0, NULL, 0, NULL, 0, NULL, &n));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(!f(INVALID_HANDLE_VALUE, 0, 0, NULL, 0, NULL, 0, NULL, &n));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    CHECK(!f(ev, 0, 0, NULL, 0, NULL, 0, NULL, &n));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
    CHECK(n == NULL);
}
TEST(gap2_rtok_source_needs_duplicate_right) {
    crt_fn f = (crt_fn)t_proc("CreateRestrictedToken");
    HANDLE src, n = NULL;
    REQUIRE(f);
    src = g2_open_proc_token(TOKEN_QUERY);
    if (!src) { CHECK(0); return; }
    SET_ERR(0);
    CHECK(!f(src, 0, 0, NULL, 0, NULL, 0, NULL, &n));
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(src);
}
TEST(gap2_rtok_invalid_flags) {
    crt_fn f = (crt_fn)t_proc("CreateRestrictedToken");
    static const DWORD fl[] = { 0x10, 0x20, 0x100, 0x80000000, 0xFFFFFFFF };
    HANDLE src; size_t i;
    REQUIRE(f);
    src = rt_src(); if (!src) { CHECK(0); return; }
    for (i = 0; i < sizeof fl / sizeof fl[0]; i++) {
        HANDLE n = NULL; SET_ERR(0);
        BOOL ok = f(src, fl[i], 0, NULL, 0, NULL, 0, NULL, &n);
        t_note("flags 0x%lx: ok=%d err=%lu", fl[i], ok, ok ? 0 : GetLastError());
        if (n) CloseHandle(n);
    }
    CloseHandle(src);
}
TEST(gap2_rtok_null_out_handle) {
    crt_fn f = (crt_fn)t_proc("CreateRestrictedToken");
    HANDLE src;
    REQUIRE(f);
    src = rt_src(); if (!src) { CHECK(0); return; }
    SET_ERR(0);
    BOOL ok = f(src, 0, 0, NULL, 0, NULL, 0, NULL, NULL);
    CHECK(!ok);
    t_note("NULL NewTokenHandle: err=%lu", GetLastError());
    CloseHandle(src);
}
TEST(gap2_rtok_new_token_not_inheritable) {
    crt_fn f = (crt_fn)t_proc("CreateRestrictedToken");
    HANDLE src, n = NULL; DWORD fl = 0xFF;
    REQUIRE(f);
    src = rt_src(); if (!src) { CHECK(0); return; }
    CHECK(f(src, 0, 0, NULL, 0, NULL, 0, NULL, &n));
    if (n) { CHECK(GetHandleInformation(n, &fl)); CHECK(!(fl & HANDLE_FLAG_INHERIT)); CloseHandle(n); }
    CloseHandle(src);
}
TEST(gap2_rtok_repeat) {
    crt_fn f = (crt_fn)t_proc("CreateRestrictedToken");
    HANDLE src; int i;
    REQUIRE(f);
    src = rt_src(); if (!src) { CHECK(0); return; }
    for (i = 0; i < 200; i++) {
        HANDLE n = NULL;
        if (!f(src, DISABLE_MAX_PRIVILEGE, 0, NULL, 0, NULL, 0, NULL, &n)) { CHECK(0); break; }
        CloseHandle(n);
    }
    CloseHandle(src);
}
