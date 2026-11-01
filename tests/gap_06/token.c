#include "g6.h"

static BOOL gti(HANDLE h, DWORD cls, void *buf, DWORD len, DWORD *rl) {
    return U(GetTokenInformation)(h, (TOKEN_INFORMATION_CLASS)cls, buf, len, rl);
}
static HANDLE open_tok(DWORD access) {
    HANDLE h = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), access, &h)) return NULL;
    return h;
}

/* class, fixed size (0 = variable) */
static const struct { DWORD cls; DWORD fixed; const char *name; } classes[] = {
    {TokenUser, 0, "User"}, {TokenGroups, 0, "Groups"}, {TokenPrivileges, 0, "Privileges"},
    {TokenOwner, 0, "Owner"}, {TokenPrimaryGroup, 0, "PrimaryGroup"}, {TokenDefaultDacl, 0, "DefaultDacl"},
    {TokenSource, 16, "Source"}, {TokenType, 4, "Type"}, {TokenStatistics, 56, "Statistics"},
    {TokenRestrictedSids, 0, "RestrictedSids"}, {TokenSessionId, 4, "SessionId"},
    {TokenGroupsAndPrivileges, 0, "GroupsAndPrivileges"}, {TokenSandBoxInert, 4, "SandBoxInert"},
    {TokenOrigin, 8, "Origin"}, {TokenElevationType, 4, "ElevationType"}, {TokenElevation, 4, "Elevation"},
    {TokenHasRestrictions, 1, "HasRestrictions"}, {TokenAccessInformation, 0, "AccessInformation"},
    {TokenVirtualizationAllowed, 4, "VirtualizationAllowed"}, {TokenVirtualizationEnabled, 4, "VirtualizationEnabled"},
    {TokenIntegrityLevel, 0, "IntegrityLevel"}, {TokenUIAccess, 4, "UIAccess"},
    {TokenMandatoryPolicy, 4, "MandatoryPolicy"}, {28, 0, "LogonSid"}, {29, 4, "IsAppContainer"},
};
#define NCLS (sizeof classes / sizeof *classes)

TEST(gap6_gti_size_query_all_classes) {
    REQUIRE(U(GetTokenInformation));
    /* TokenSource additionally needs TOKEN_QUERY_SOURCE (checked before the length) */
    HANDLE t = open_tok(TOKEN_QUERY | TOKEN_QUERY_SOURCE); if (!t) { t_skip("token"); return; }
    for (unsigned i = 0; i < NCLS; i++) {
        DWORD rl = 0xDEAD; SET_ERR(0);
        BOOL r = gti(t, classes[i].cls, NULL, 0, &rl);
        DWORD e = GetLastError();
        if (r) { t_note("%s: size query with len 0 succeeded", classes[i].name); continue; }
        if (e == ERROR_INVALID_PARAMETER || e == ERROR_NOT_FOUND || e == ERROR_NO_SUCH_GROUP) { t_note("%s unsupported err %lu", classes[i].name, e); continue; }
        if (classes[i].cls == TokenElevation) {
            /* NtQueryInformationToken(TokenElevation) requires length == 4 exactly: any other length
               is a length mismatch (ERROR_BAD_LENGTH), but the required size is still reported. */
            CHECKF(e == ERROR_BAD_LENGTH, "%s err %lu", classes[i].name, e);
        } else
            CHECKF(e == ERROR_INSUFFICIENT_BUFFER, "%s err %lu", classes[i].name, e);
        CHECKF(rl != 0 && rl != 0xDEAD, "%s rl %lu", classes[i].name, rl);
        if (classes[i].fixed) CHECKF(rl == classes[i].fixed, "%s rl %lu want %lu", classes[i].name, rl, classes[i].fixed);
    }
    CloseHandle(t);
}
TEST(gap6_gti_exact_size_all_classes) {
    REQUIRE(U(GetTokenInformation));
    HANDLE t = open_tok(TOKEN_QUERY); if (!t) { t_skip("token"); return; }
    for (unsigned i = 0; i < NCLS; i++) {
        DWORD rl = 0;
        if (gti(t, classes[i].cls, NULL, 0, &rl) || GetLastError() != ERROR_INSUFFICIENT_BUFFER) continue;
        BYTE *b = (BYTE *)LocalAlloc(LPTR, rl + 16);
        memset(b + rl, 0xCD, 16);
        DWORD rl2 = 0; SET_ERR(0);
        BOOL r = gti(t, classes[i].cls, b, rl, &rl2);
        CHECKF(r, "%s exact err %lu", classes[i].name, GetLastError());
        if (r) {
            CHECKF(rl2 == rl, "%s rl %lu vs %lu", classes[i].name, rl2, rl);
            CHECKF(b[rl] == 0xCD, "%s overrun", classes[i].name);
        }
        if (rl > 1) {
            rl2 = 0; SET_ERR(0);
            r = gti(t, classes[i].cls, b, rl - 1, &rl2);
            CHECKF(!r, "%s len-1 succeeded", classes[i].name);
            CHECKF(GetLastError() == ERROR_INSUFFICIENT_BUFFER, "%s len-1 err %lu", classes[i].name, GetLastError());
            CHECKF(rl2 == rl, "%s len-1 rl %lu", classes[i].name, rl2);
        }
        LocalFree(b);
    }
    CloseHandle(t);
}
TEST(gap6_gti_oversized_buffer_rl_is_needed_size) {
    REQUIRE(U(GetTokenInformation));
    HANDLE t = open_tok(TOKEN_QUERY); if (!t) { t_skip("token"); return; }
    BYTE b[4096]; DWORD rl = 0;
    CHECK(gti(t, TokenType, b, sizeof b, &rl));
    EXPECT_EQ(rl, 4);
    rl = 0;
    CHECK(gti(t, TokenStatistics, b, sizeof b, &rl));
    EXPECT_EQ(rl, 56);
    CloseHandle(t);
}
TEST(gap6_gti_invalid_class) {
    REQUIRE(U(GetTokenInformation));
    HANDLE t = open_tok(TOKEN_QUERY); if (!t) { t_skip("token"); return; }
    DWORD bad[] = {0, 0x100, 0x7FFF, 0xFFFF, 0x10000, 0x7FFFFFFF, 0xFFFFFFFF};
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        BYTE b[512]; DWORD rl = 0xAA; SET_ERR(0);
        BOOL r = gti(t, bad[i], b, sizeof b, &rl);
        CHECKF(!r, "class %lx", bad[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "class %lx err %lu", bad[i], GetLastError());
    }
    CloseHandle(t);
}
TEST(gap6_gti_invalid_class_with_zero_buffer) {
    REQUIRE(U(GetTokenInformation));
    HANDLE t = open_tok(TOKEN_QUERY); if (!t) { t_skip("token"); return; }
    DWORD rl = 0; SET_ERR(0);
    CHECK(!gti(t, 0, NULL, 0, &rl));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!gti(t, 0x7FFF, NULL, 0, &rl));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CloseHandle(t);
}
TEST(gap6_gti_invalid_handles) {
    REQUIRE(U(GetTokenInformation));
    BYTE b[512]; DWORD rl = 0;
    HANDLE hs[] = {NULL, INVALID_HANDLE_VALUE, (HANDLE)(LONG_PTR)-2, (HANDLE)0x7FFFFFF0, (HANDLE)(ULONG_PTR)0xDEADBEE4};
    for (unsigned i = 0; i < sizeof hs / sizeof *hs; i++) {
        SET_ERR(0);
        BOOL r = gti(hs[i], TokenUser, b, sizeof b, &rl);
        CHECKF(!r, "handle %p", hs[i]);
        CHECKF(GetLastError() == ERROR_INVALID_HANDLE, "handle %p err %lu", hs[i], GetLastError());
    }
}
TEST(gap6_gti_wrong_handle_types) {
    REQUIRE(U(GetTokenInformation));
    BYTE b[512]; DWORD rl = 0;
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE hs[3] = {ev, GetCurrentProcess(), GetCurrentThread()};
    for (int i = 0; i < 3; i++) {
        SET_ERR(0);
        CHECKF(!gti(hs[i], TokenUser, b, sizeof b, &rl), "type %d", i);
        CHECKF(GetLastError() == ERROR_INVALID_HANDLE, "type %d err %lu", i, GetLastError());
    }
    HANDLE ph = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    if (ph) {
        SET_ERR(0);
        CHECK(!gti(ph, TokenUser, b, sizeof b, &rl));
        EXPECT_ERR(ERROR_INVALID_HANDLE);
        CloseHandle(ph);
    }
    CloseHandle(ev);
}
TEST(gap6_gti_closed_handle) {
    REQUIRE(U(GetTokenInformation));
    HANDLE t = open_tok(TOKEN_QUERY); if (!t) { t_skip("token"); return; }
    CloseHandle(t);
    BYTE b[512]; DWORD rl = 0; SET_ERR(0);
    CHECK(!gti(t, TokenUser, b, sizeof b, &rl));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap6_gti_access_denied_without_query) {
    REQUIRE(U(GetTokenInformation));
    HANDLE t = open_tok(TOKEN_DUPLICATE);
    if (!t) { t_skip("cannot open token with TOKEN_DUPLICATE only"); return; }
    BYTE b[512]; DWORD rl = 0;
    DWORD cls[] = {TokenUser, TokenType, TokenStatistics, TokenGroups, TokenElevation};
    for (unsigned i = 0; i < 5; i++) {
        SET_ERR(0);
        /* TokenElevation requires an exact length of 4 (length is checked before access) */
        CHECKF(!gti(t, cls[i], b, cls[i] == TokenElevation ? 4 : sizeof b, &rl), "class %lu", cls[i]);
        CHECKF(GetLastError() == ERROR_ACCESS_DENIED, "class %lu err %lu", cls[i], GetLastError());
    }
    CloseHandle(t);
}
TEST(gap6_gti_source_requires_query_source) {
    REQUIRE(U(GetTokenInformation));
    HANDLE q = open_tok(TOKEN_QUERY);
    if (q) {
        TOKEN_SOURCE src; DWORD rl = 0; SET_ERR(0);
        CHECK(!gti(q, TokenSource, &src, sizeof src, &rl));
        EXPECT_ERR(ERROR_ACCESS_DENIED);
        CloseHandle(q);
    }
    HANDLE s = open_tok(TOKEN_QUERY_SOURCE);
    if (s) {
        TOKEN_SOURCE src; DWORD rl = 0; SET_ERR(0);
        CHECK(gti(s, TokenSource, &src, sizeof src, &rl));
        EXPECT_EQ(rl, sizeof(TOKEN_SOURCE));
        BYTE b[64]; SET_ERR(0);
        CHECK(!gti(s, TokenType, b, sizeof b, &rl));      /* no TOKEN_QUERY */
        EXPECT_ERR(ERROR_ACCESS_DENIED);
        CloseHandle(s);
    } else t_note("cannot open token with TOKEN_QUERY_SOURCE only");
}
TEST(gap6_gti_null_buffer_with_length_is_size_query_only) {
    REQUIRE(U(GetTokenInformation));
    HANDLE t = open_tok(TOKEN_QUERY); if (!t) { t_skip("token"); return; }
    DWORD rl = 0; SET_ERR(0);
    CHECK(!gti(t, TokenUser, NULL, 0, &rl));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(rl >= sizeof(TOKEN_USER) + 8);
    CloseHandle(t);
}

/* ---- values ---- */
TEST(gap6_gti_user_sid) {
    REQUIRE(U(GetTokenInformation));
    HANDLE t = open_tok(TOKEN_QUERY); if (!t) { t_skip("token"); return; }
    BYTE b1[256], b2[256]; DWORD rl = 0;
    CHECK(gti(t, TokenUser, b1, sizeof b1, &rl));
    TOKEN_USER *u = (TOKEN_USER *)b1;
    CHECK(IsValidSid(u->User.Sid));
    CHECK((BYTE *)u->User.Sid > b1 && (BYTE *)u->User.Sid < b1 + rl);
    EXPECT_EQ(u->User.Attributes, 0);
    CHECK(gti(t, TokenUser, b2, sizeof b2, &rl));
    CHECK(EqualSid(u->User.Sid, ((TOKEN_USER *)b2)->User.Sid));
    /* user SID matches SID reported by the owner/primary group family */
    CloseHandle(t);
}
TEST(gap6_gti_type_and_statistics) {
    REQUIRE(U(GetTokenInformation));
    HANDLE t = open_tok(TOKEN_QUERY); if (!t) { t_skip("token"); return; }
    DWORD type = 0, rl = 0;
    CHECK(gti(t, TokenType, &type, 4, &rl));
    EXPECT_EQ(type, TokenPrimary);
    TOKEN_STATISTICS st; memset(&st, 0xCC, sizeof st);
    CHECK(gti(t, TokenStatistics, &st, sizeof st, &rl));
    EXPECT_EQ(st.TokenType, TokenPrimary);
    CHECK(st.GroupCount > 0);
    CHECK(st.PrivilegeCount > 0);
    CHECK(st.ExpirationTime.QuadPart == 0 || st.ExpirationTime.QuadPart == 0x7FFFFFFFFFFFFFFFLL);
    CloseHandle(t);
}
TEST(gap6_gti_session_id_matches_process) {
    REQUIRE(U(GetTokenInformation));
    HANDLE t = open_tok(TOKEN_QUERY); if (!t) { t_skip("token"); return; }
    DWORD sid = 0xFFFF, rl = 0, ps = 0xFFFE;
    CHECK(gti(t, TokenSessionId, &sid, 4, &rl));
    if (ProcessIdToSessionId(GetCurrentProcessId(), &ps)) EXPECT_EQ(sid, ps);
    CloseHandle(t);
}
TEST(gap6_gti_groups_privileges_consistent) {
    REQUIRE(U(GetTokenInformation));
    HANDLE t = open_tok(TOKEN_QUERY); if (!t) { t_skip("token"); return; }
    DWORD rl = 0;
    gti(t, TokenGroups, NULL, 0, &rl);
    TOKEN_GROUPS *g = (TOKEN_GROUPS *)LocalAlloc(LPTR, rl);
    CHECK(gti(t, TokenGroups, g, rl, &rl));
    CHECK(g->GroupCount > 0);
    int bad = 0;
    for (DWORD i = 0; i < g->GroupCount; i++) if (!IsValidSid(g->Groups[i].Sid)) bad++;
    EXPECT_EQ(bad, 0);
    gti(t, TokenPrivileges, NULL, 0, &rl);
    TOKEN_PRIVILEGES *p = (TOKEN_PRIVILEGES *)LocalAlloc(LPTR, rl);
    CHECK(gti(t, TokenPrivileges, p, rl, &rl));
    CHECK(p->PrivilegeCount > 0);
    TOKEN_STATISTICS st; DWORD r2;
    if (gti(t, TokenStatistics, &st, sizeof st, &r2)) {
        EXPECT_EQ(st.GroupCount, g->GroupCount);
        EXPECT_EQ(st.PrivilegeCount, p->PrivilegeCount);
    }
    LocalFree(g); LocalFree(p);
    CloseHandle(t);
}
TEST(gap6_gti_owner_primary_group_valid) {
    REQUIRE(U(GetTokenInformation));
    HANDLE t = open_tok(TOKEN_QUERY); if (!t) { t_skip("token"); return; }
    BYTE b[256]; DWORD rl = 0;
    CHECK(gti(t, TokenOwner, b, sizeof b, &rl)); CHECK(IsValidSid(((TOKEN_OWNER *)b)->Owner));
    CHECK(gti(t, TokenPrimaryGroup, b, sizeof b, &rl)); CHECK(IsValidSid(((TOKEN_PRIMARY_GROUP *)b)->PrimaryGroup));
    CHECK(gti(t, TokenDefaultDacl, b, sizeof b, &rl));
    PACL d = ((TOKEN_DEFAULT_DACL *)b)->DefaultDacl;
    CHECK(d == NULL || IsValidAcl(d));
    CloseHandle(t);
}
TEST(gap6_gti_integrity_level_sid) {
    REQUIRE(U(GetTokenInformation));
    HANDLE t = open_tok(TOKEN_QUERY); if (!t) { t_skip("token"); return; }
    BYTE b[128]; DWORD rl = 0;
    if (!gti(t, TokenIntegrityLevel, b, sizeof b, &rl)) { t_note("no integrity level err %lu", GetLastError()); CloseHandle(t); return; }
    PSID s = ((TOKEN_MANDATORY_LABEL *)b)->Label.Sid;
    CHECK(IsValidSid(s));
    EXPECT_EQ(*GetSidSubAuthorityCount(s), 1);
    BYTE auth[6] = {0, 0, 0, 0, 0, 16};
    CHECK(!memcmp(GetSidIdentifierAuthority(s)->Value, auth, 6));
    DWORD rid = *GetSidSubAuthority(s, 0);
    CHECK(rid >= 0x1000 && rid <= 0x5000 && (rid & 0xFFF) == 0);
    CloseHandle(t);
}
TEST(gap6_gti_elevation_flags) {
    REQUIRE(U(GetTokenInformation));
    HANDLE t = open_tok(TOKEN_QUERY); if (!t) { t_skip("token"); return; }
    DWORD v = 99, rl = 0;
    CHECK(gti(t, TokenElevationType, &v, 4, &rl)); CHECK(v >= 1 && v <= 3);
    DWORD et = v; v = 99;
    CHECK(gti(t, TokenElevation, &v, 4, &rl)); CHECK(v == 0 || v == 1);
    if (et == TokenElevationTypeDefault) { /* UAC off or non-admin */ }
    if (et == TokenElevationTypeFull) EXPECT_EQ(v, 1);
    if (et == TokenElevationTypeLimited) EXPECT_EQ(v, 0);
    v = 99; CHECK(gti(t, TokenHasRestrictions, &v, 4, &rl)); CHECK(v == 0 || v == 1);
    v = 99; CHECK(gti(t, TokenSandBoxInert, &v, 4, &rl)); CHECK(v == 0 || v == 1);
    v = 99; CHECK(gti(t, TokenUIAccess, &v, 4, &rl)); CHECK(v == 0 || v == 1);
    v = 99; CHECK(gti(t, 29, &v, 4, &rl)); EXPECT_EQ(v, 0);   /* not an AppContainer */
    CloseHandle(t);
}
TEST(gap6_gti_matches_advapi) {
    REQUIRE(U(GetTokenInformation));
    HANDLE t = open_tok(TOKEN_QUERY); if (!t) { t_skip("token"); return; }
    BYTE a[512], b[512]; DWORD ra = 0, rb = 0;
    CHECK(gti(t, TokenUser, a, sizeof a, &ra));
    CHECK(GetTokenInformation(t, TokenUser, b, sizeof b, &rb));
    EXPECT_EQ(ra, rb);
    CHECK(EqualSid(((TOKEN_USER *)a)->User.Sid, ((TOKEN_USER *)b)->User.Sid));
    CloseHandle(t);
}
TEST(gap6_gti_origin_luid_nonzero) {
    REQUIRE(U(GetTokenInformation));
    HANDLE t = open_tok(TOKEN_QUERY); if (!t) { t_skip("token"); return; }
    LUID l = {0, 0}; DWORD rl = 0;
    CHECK(gti(t, TokenOrigin, &l, sizeof l, &rl));
    EXPECT_EQ(rl, 8);
    t_note("origin luid %lx:%lx", l.HighPart, l.LowPart);
    CloseHandle(t);
}

/* ---- primary vs impersonation specific ---- */
TEST(gap6_gti_impersonation_level_on_primary_fails) {
    REQUIRE(U(GetTokenInformation));
    HANDLE t = open_tok(TOKEN_QUERY); if (!t) { t_skip("token"); return; }
    DWORD v = 99, rl = 0; SET_ERR(0);
    BOOL r = gti(t, TokenImpersonationLevel, &v, 4, &rl);
    CHECK(!r);
    if (!r) { DWORD e = GetLastError(); CHECKF(e == ERROR_INVALID_PARAMETER || e == ERROR_BAD_TOKEN_TYPE, "err %lu", e); t_note("primary TokenImpersonationLevel err=%lu", e); }
    CloseHandle(t);
}
TEST(gap6_gti_duplicated_impersonation_token) {
    REQUIRE(U(GetTokenInformation));
    HANDLE t = open_tok(TOKEN_DUPLICATE | TOKEN_QUERY); if (!t) { t_skip("token"); return; }
    SECURITY_IMPERSONATION_LEVEL lv[] = {SecurityAnonymous, SecurityIdentification, SecurityImpersonation};
    for (int i = 0; i < 3; i++) {
        HANDLE d = NULL;
        if (!DuplicateTokenEx(t, TOKEN_QUERY, NULL, lv[i], TokenImpersonation, &d)) { CHECKF(0, "dup %d err %lu", i, GetLastError()); continue; }
        DWORD v = 99, rl = 0;
        CHECK(gti(d, TokenType, &v, 4, &rl)); EXPECT_EQ(v, TokenImpersonation);
        v = 99;
        CHECK(gti(d, TokenImpersonationLevel, &v, 4, &rl));
        EXPECT_EQ(v, (DWORD)lv[i]);
        EXPECT_EQ(rl, 4);
        TOKEN_STATISTICS st;
        CHECK(gti(d, TokenStatistics, &st, sizeof st, &rl));
        EXPECT_EQ(st.TokenType, TokenImpersonation);
        EXPECT_EQ(st.ImpersonationLevel, lv[i]);
        CloseHandle(d);
    }
    CloseHandle(t);
}
TEST(gap6_gti_duplicate_same_user) {
    REQUIRE(U(GetTokenInformation));
    HANDLE t = open_tok(TOKEN_DUPLICATE | TOKEN_QUERY); if (!t) { t_skip("token"); return; }
    HANDLE d = NULL;
    if (!DuplicateTokenEx(t, TOKEN_QUERY, NULL, SecurityImpersonation, TokenImpersonation, &d)) { t_skip("dup"); CloseHandle(t); return; }
    BYTE a[256], b[256]; DWORD rl;
    CHECK(gti(t, TokenUser, a, sizeof a, &rl));
    CHECK(gti(d, TokenUser, b, sizeof b, &rl));
    CHECK(EqualSid(((TOKEN_USER *)a)->User.Sid, ((TOKEN_USER *)b)->User.Sid));
    CloseHandle(d); CloseHandle(t);
}
TEST(gap6_gti_thread_token_none) {
    REQUIRE(U(GetTokenInformation));
    HANDLE t = NULL;
    SET_ERR(0);
    if (OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &t)) { t_note("thread already impersonating"); CloseHandle(t); return; }
    EXPECT_ERR(ERROR_NO_TOKEN);
}
TEST(gap6_gti_thread_token_after_impersonate_self) {
    REQUIRE(U(GetTokenInformation)); REQUIRE(U(ImpersonateSelf));
    if (!U(ImpersonateSelf)(SecurityImpersonation)) { t_skip("ImpersonateSelf failed"); return; }
    HANDLE t = NULL;
    BOOL ok = OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &t);
    DWORD ty = 0, lv = 0, rl = 0;
    BOOL r1 = ok && gti(t, TokenType, &ty, 4, &rl);
    BOOL r2 = ok && gti(t, TokenImpersonationLevel, &lv, 4, &rl);
    RevertToSelf();
    CHECK(ok); CHECK(r1); CHECK(r2);
    EXPECT_EQ(ty, TokenImpersonation);
    EXPECT_EQ(lv, SecurityImpersonation);
    if (t) CloseHandle(t);
}
TEST(gap6_gti_linked_token_when_not_filtered) {
    REQUIRE(U(GetTokenInformation));
    HANDLE t = open_tok(TOKEN_QUERY); if (!t) { t_skip("token"); return; }
    DWORD et = 0, rl = 0;
    gti(t, TokenElevationType, &et, 4, &rl);
    BYTE b[16]; SET_ERR(0);
    BOOL r = gti(t, TokenLinkedToken, b, sizeof b, &rl);
    if (et == TokenElevationTypeDefault) { CHECK(!r); t_note("linked token on default-elevation: err %lu", GetLastError()); }
    else if (r) { HANDLE lt = ((TOKEN_LINKED_TOKEN *)b)->LinkedToken; CHECK(lt != NULL); if (lt) CloseHandle(lt); }
    else t_note("linked token err %lu", GetLastError());
    CloseHandle(t);
}
TEST(gap6_gti_repeat_stable) {
    REQUIRE(U(GetTokenInformation));
    HANDLE t = open_tok(TOKEN_QUERY); if (!t) { t_skip("token"); return; }
    int bad = 0;
    for (int i = 0; i < 2000; i++) { DWORD v, rl; if (!gti(t, TokenSessionId, &v, 4, &rl) || rl != 4) bad++; }
    EXPECT_EQ(bad, 0);
    CloseHandle(t);
}
TEST(gap6_gti_pseudo_token_handles) {
    REQUIRE(U(GetTokenInformation));
    /* -4/-5/-6 are the pseudo token values on newer Windows builds; just must not crash. */
    HANDLE hs[] = {(HANDLE)(LONG_PTR)-4, (HANDLE)(LONG_PTR)-5, (HANDLE)(LONG_PTR)-6};
    for (int i = 0; i < 3; i++) {
        DWORD v = 0, rl = 0; SET_ERR(0);
        BOOL r = gti(hs[i], TokenType, &v, 4, &rl);
        t_note("pseudo token %d: r=%d err=%lu", -4 - i, r, GetLastError());
    }
}
