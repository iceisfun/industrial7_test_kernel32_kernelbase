#include "g6.h"

static BOOL thread_has_token(void) {
    HANDLE t = NULL;
    if (OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &t)) { CloseHandle(t); return TRUE; }
    return FALSE;
}
static HANDLE open_tok(DWORD access) { HANDLE h = NULL; return OpenProcessToken(GetCurrentProcess(), access, &h) ? h : NULL; }

/* ---- ImpersonateSelf ---- */
TEST(gap6_impself_levels) {
    REQUIRE(U(ImpersonateSelf));
    SECURITY_IMPERSONATION_LEVEL lv[] = {SecurityAnonymous, SecurityIdentification, SecurityImpersonation, SecurityDelegation};
    for (int i = 0; i < 4; i++) {
        SET_ERR(0);
        BOOL r = U(ImpersonateSelf)(lv[i]);
        if (!r) { DWORD e = GetLastError(); CHECKF(i == 3, "level %d failed err %lu", i, e); t_note("level %d err %lu", i, e); continue; }
        HANDLE t = NULL; DWORD got = 99, rl = 0, ty = 0;
        BOOL ok = OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &t);
        BOOL g1 = ok && GetTokenInformation(t, TokenImpersonationLevel, &got, 4, &rl);
        BOOL g2 = ok && GetTokenInformation(t, TokenType, &ty, 4, &rl);
        DWORD oe = GetLastError();
        RevertToSelf();
        if (lv[i] == SecurityAnonymous) {
            /* NtOpenThreadToken refuses anonymous-level tokens (STATUS_CANT_OPEN_ANONYMOUS) */
            if (!ok) CHECKF(oe == ERROR_CANT_OPEN_ANONYMOUS, "anonymous OpenThreadToken err %lu", oe);
            else t_note("anonymous-level thread token opened");
            if (t) CloseHandle(t);
            CHECKF(!thread_has_token(), "revert level %d", i);
            continue;
        }
        CHECKF(ok, "level %d OpenThreadToken", i);
        CHECKF(g1 && g2, "level %d query", i);
        CHECKF(ty == TokenImpersonation, "level %d type %lu", i, ty);
        CHECKF(got == (DWORD)lv[i] || (lv[i] == SecurityDelegation && got == SecurityImpersonation), "level %d got %lu", i, got);
        if (t) CloseHandle(t);
        CHECKF(!thread_has_token(), "revert level %d", i);
    }
}
TEST(gap6_impself_invalid_levels) {
    REQUIRE(U(ImpersonateSelf));
    DWORD bad[] = {4, 5, 0x100, 0x7FFFFFFF, 0xFFFFFFFF};
    for (unsigned i = 0; i < 5; i++) {
        SET_ERR(0);
        BOOL r = U(ImpersonateSelf)((SECURITY_IMPERSONATION_LEVEL)bad[i]);
        DWORD e = GetLastError();
        if (r) { RevertToSelf(); }
        CHECKF(!r, "level %lx", bad[i]);
        CHECKF(e == ERROR_BAD_IMPERSONATION_LEVEL || e == ERROR_INVALID_PARAMETER, "level %lx err %lu", bad[i], e);
        t_note("ImpersonateSelf(%lx) err %lu", bad[i], e);
        CHECK(!thread_has_token());
    }
}
TEST(gap6_impself_revert_clears) {
    REQUIRE(U(ImpersonateSelf));
    CHECK(!thread_has_token());
    CHECK(U(ImpersonateSelf)(SecurityImpersonation));
    CHECK(thread_has_token());
    CHECK(RevertToSelf());
    CHECK(!thread_has_token());
    SET_ERR(0);
    CHECK(!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &(HANDLE){0}));
    EXPECT_ERR(ERROR_NO_TOKEN);
}
TEST(gap6_impself_twice_replaces) {
    REQUIRE(U(ImpersonateSelf));
    CHECK(U(ImpersonateSelf)(SecurityIdentification));
    CHECK(U(ImpersonateSelf)(SecurityImpersonation));
    HANDLE t = NULL; DWORD lv = 0, rl;
    BOOL ok = OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &t);
    BOOL g = ok && GetTokenInformation(t, TokenImpersonationLevel, &lv, 4, &rl);
    RevertToSelf();
    CHECK(ok && g);
    /* second call impersonates the (impersonating) thread's process again: level is the new one */
    EXPECT_EQ(lv, SecurityImpersonation);
    if (t) CloseHandle(t);
}
TEST(gap6_impself_same_user_as_process) {
    REQUIRE(U(ImpersonateSelf));
    HANDLE pt = open_tok(TOKEN_QUERY); if (!pt) { t_skip("token"); return; }
    BYTE a[256], b[256]; DWORD rl;
    CHECK(GetTokenInformation(pt, TokenUser, a, sizeof a, &rl));
    CHECK(U(ImpersonateSelf)(SecurityImpersonation));
    HANDLE tt = NULL; BOOL ok = OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &tt);
    BOOL g = ok && GetTokenInformation(tt, TokenUser, b, sizeof b, &rl);
    RevertToSelf();
    CHECK(ok && g);
    if (g) CHECK(EqualSid(((TOKEN_USER *)a)->User.Sid, ((TOKEN_USER *)b)->User.Sid));
    if (tt) CloseHandle(tt);
    CloseHandle(pt);
}
static DWORD WINAPI self_thread(LPVOID p) {
    /* impersonation is per-thread */
    DWORD *res = (DWORD *)p;
    BOOL r = U(ImpersonateSelf)(SecurityImpersonation);
    HANDLE t = NULL;
    BOOL has = OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &t);
    if (t) CloseHandle(t);
    RevertToSelf();
    *res = (r ? 1 : 0) | (has ? 2 : 0);
    return 0;
}
TEST(gap6_impself_per_thread) {
    REQUIRE(U(ImpersonateSelf));
    DWORD res = 0;
    HANDLE th = CreateThread(NULL, 0, self_thread, &res, 0, NULL);
    if (!th) { t_skip("thread"); return; }
    WaitForSingleObject(th, 10000);
    CloseHandle(th);
    EXPECT_EQ(res, 3);
    CHECK(!thread_has_token());
}

/* ---- ImpersonateLoggedOnUser ---- */
TEST(gap6_impuser_primary_token) {
    REQUIRE(U(ImpersonateLoggedOnUser));
    HANDLE t = open_tok(TOKEN_QUERY | TOKEN_DUPLICATE); if (!t) { t_skip("token"); return; }
    SET_ERR(0);
    BOOL r = U(ImpersonateLoggedOnUser)(t);
    DWORD e = GetLastError();
    BOOL has = r && thread_has_token();
    DWORD ty = 0, lv = 0, rl; HANDLE tt = NULL;
    if (r && OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &tt)) {
        GetTokenInformation(tt, TokenType, &ty, 4, &rl);
        GetTokenInformation(tt, TokenImpersonationLevel, &lv, 4, &rl);
        CloseHandle(tt);
    }
    RevertToSelf();
    CHECKF(r, "err %lu", e);
    CHECK(has);
    EXPECT_EQ(ty, TokenImpersonation);
    EXPECT_EQ(lv, SecurityImpersonation);   /* primary tokens are duplicated at impersonation level */
    CHECK(!thread_has_token());
    CloseHandle(t);
}
TEST(gap6_impuser_primary_needs_duplicate) {
    REQUIRE(U(ImpersonateLoggedOnUser));
    HANDLE t = open_tok(TOKEN_QUERY); if (!t) { t_skip("token"); return; }
    SET_ERR(0);
    BOOL r = U(ImpersonateLoggedOnUser)(t);
    DWORD e = GetLastError();
    if (r) RevertToSelf();
    CHECK(!r);
    EXPECT_EQ(e, ERROR_ACCESS_DENIED);
    CHECK(!thread_has_token());
    CloseHandle(t);
}
TEST(gap6_impuser_primary_needs_query) {
    REQUIRE(U(ImpersonateLoggedOnUser));
    HANDLE t = open_tok(TOKEN_DUPLICATE); if (!t) { t_skip("token"); return; }
    SET_ERR(0);
    BOOL r = U(ImpersonateLoggedOnUser)(t);
    DWORD e = GetLastError();
    if (r) RevertToSelf();
    CHECK(!r);
    EXPECT_EQ(e, ERROR_ACCESS_DENIED);
    CloseHandle(t);
}
TEST(gap6_impuser_impersonation_token) {
    REQUIRE(U(ImpersonateLoggedOnUser));
    HANDLE t = open_tok(TOKEN_DUPLICATE | TOKEN_QUERY); if (!t) { t_skip("token"); return; }
    HANDLE d = NULL;
    if (!DuplicateTokenEx(t, TOKEN_QUERY | TOKEN_IMPERSONATE, NULL, SecurityImpersonation, TokenImpersonation, &d)) { t_skip("dup"); CloseHandle(t); return; }
    SET_ERR(0);
    BOOL r = U(ImpersonateLoggedOnUser)(d);
    DWORD e = GetLastError();
    BOOL has = r && thread_has_token();
    RevertToSelf();
    CHECKF(r, "err %lu", e);
    CHECK(has);
    CloseHandle(d);
    /* token without TOKEN_IMPERSONATE cannot be installed on a thread */
    if (DuplicateTokenEx(t, TOKEN_QUERY, NULL, SecurityImpersonation, TokenImpersonation, &d)) {
        SET_ERR(0);
        r = U(ImpersonateLoggedOnUser)(d);
        e = GetLastError();
        if (r) RevertToSelf();
        CHECK(!r);
        t_note("impersonation token without TOKEN_IMPERSONATE: err %lu", e);
        CloseHandle(d);
    }
    CloseHandle(t);
}
TEST(gap6_impuser_identification_level_token) {
    REQUIRE(U(ImpersonateLoggedOnUser));
    HANDLE t = open_tok(TOKEN_DUPLICATE | TOKEN_QUERY); if (!t) { t_skip("token"); return; }
    HANDLE d = NULL;
    if (!DuplicateTokenEx(t, TOKEN_QUERY | TOKEN_IMPERSONATE, NULL, SecurityIdentification, TokenImpersonation, &d)) { t_skip("dup"); CloseHandle(t); return; }
    BOOL r = U(ImpersonateLoggedOnUser)(d);
    DWORD e = GetLastError();
    BOOL has = r && thread_has_token();
    RevertToSelf();
    CHECKF(r, "err %lu", e);
    CHECK(has);
    CloseHandle(d); CloseHandle(t);
}
TEST(gap6_impuser_invalid_handles) {
    REQUIRE(U(ImpersonateLoggedOnUser));
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE hs[] = {NULL, INVALID_HANDLE_VALUE, (HANDLE)(LONG_PTR)-2, (HANDLE)0xDEADBEE4, ev, GetCurrentProcess()};
    for (unsigned i = 0; i < sizeof hs / sizeof *hs; i++) {
        SET_ERR(0);
        BOOL r = U(ImpersonateLoggedOnUser)(hs[i]);
        DWORD e = GetLastError();
        if (r) RevertToSelf();
        CHECKF(!r, "handle %u", i);
        CHECKF(e == ERROR_INVALID_HANDLE, "handle %u err %lu", i, e);
        CHECKF(!thread_has_token(), "handle %u left token", i);
    }
    CloseHandle(ev);
}
TEST(gap6_impuser_closed_handle) {
    REQUIRE(U(ImpersonateLoggedOnUser));
    HANDLE t = open_tok(TOKEN_QUERY | TOKEN_DUPLICATE); if (!t) { t_skip("token"); return; }
    CloseHandle(t);
    SET_ERR(0);
    CHECK(!U(ImpersonateLoggedOnUser)(t));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap6_impuser_token_stays_valid_after_revert) {
    REQUIRE(U(ImpersonateLoggedOnUser));
    HANDLE t = open_tok(TOKEN_QUERY | TOKEN_DUPLICATE); if (!t) { t_skip("token"); return; }
    for (int i = 0; i < 20; i++) {
        if (!U(ImpersonateLoggedOnUser)(t)) { CHECKF(0, "iter %d", i); break; }
        RevertToSelf();
    }
    DWORD ty, rl;
    CHECK(GetTokenInformation(t, TokenType, &ty, 4, &rl));
    CloseHandle(t);
}

/* ---- ImpersonateAnonymousToken ---- */
TEST(gap6_impanon_current_thread) {
    REQUIRE(U(ImpersonateAnonymousToken));
    SET_ERR(0);
    BOOL r = U(ImpersonateAnonymousToken)(GetCurrentThread());
    DWORD e = GetLastError();
    if (!r) { t_note("ImpersonateAnonymousToken failed err %lu", e); CHECK(!thread_has_token()); return; }
    HANDLE t = NULL; BYTE b[256]; DWORD rl = 0; BOOL g = FALSE;
    BOOL ok = OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &t);
    if (ok) g = GetTokenInformation(t, TokenUser, b, sizeof b, &rl);
    RevertToSelf();
    CHECK(ok);
    if (g) {
        PSID anon = NULL;
        if (ConvertStringSidToSidA("S-1-5-7", &anon)) { CHECK(EqualSid(((TOKEN_USER *)b)->User.Sid, anon)); LocalFree(anon); }
    }
    if (t) CloseHandle(t);
    CHECK(!thread_has_token());
}
TEST(gap6_impanon_invalid_handles) {
    REQUIRE(U(ImpersonateAnonymousToken));
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE hs[] = {NULL, INVALID_HANDLE_VALUE, (HANDLE)0xDEADBEE4, ev, GetCurrentProcess()};
    for (unsigned i = 0; i < sizeof hs / sizeof *hs; i++) {
        SET_ERR(0);
        BOOL r = U(ImpersonateAnonymousToken)(hs[i]);
        DWORD e = GetLastError();
        if (r) RevertToSelf();
        CHECKF(!r, "handle %u", i);
        CHECKF(e == ERROR_INVALID_HANDLE, "handle %u err %lu", i, e);
    }
    CloseHandle(ev);
    CHECK(!thread_has_token());
}
TEST(gap6_impanon_insufficient_thread_access) {
    REQUIRE(U(ImpersonateAnonymousToken));
    HANDLE th = OpenThread(THREAD_QUERY_INFORMATION, FALSE, GetCurrentThreadId());
    if (!th) { t_skip("OpenThread"); return; }
    SET_ERR(0);
    BOOL r = U(ImpersonateAnonymousToken)(th);
    DWORD e = GetLastError();
    if (r) RevertToSelf();
    CHECK(!r);
    EXPECT_EQ(e, ERROR_ACCESS_DENIED);
    CloseHandle(th);
}
TEST(gap6_impanon_closed_thread_handle) {
    REQUIRE(U(ImpersonateAnonymousToken));
    HANDLE th = OpenThread(THREAD_ALL_ACCESS, FALSE, GetCurrentThreadId());
    if (!th) { t_skip("OpenThread"); return; }
    CloseHandle(th);
    SET_ERR(0);
    BOOL r = U(ImpersonateAnonymousToken)(th);
    if (r) RevertToSelf();
    CHECK(!r);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap6_impanon_real_thread_handle_with_impersonate_access) {
    REQUIRE(U(ImpersonateAnonymousToken));
    HANDLE th = OpenThread(THREAD_IMPERSONATE | THREAD_QUERY_INFORMATION, FALSE, GetCurrentThreadId());
    if (!th) { t_skip("OpenThread with THREAD_IMPERSONATE"); return; }
    BOOL r = U(ImpersonateAnonymousToken)(th);
    DWORD e = GetLastError();
    if (r) RevertToSelf();
    if (!r) t_note("failed err %lu", e);
    CHECK(!thread_has_token());
    CloseHandle(th);
}

/* ---- ImpersonateNamedPipeClient ---- */
static void pipename(char *out, size_t cap, const char *tag) {
    _snprintf(out, cap, "\\\\.\\pipe\\k32t_g6_%lu_%s", GetCurrentProcessId(), tag);
}
TEST(gap6_imppipe_invalid_handles) {
    REQUIRE(U(ImpersonateNamedPipeClient));
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE hs[] = {NULL, INVALID_HANDLE_VALUE, (HANDLE)0xDEADBEE4, ev, GetCurrentProcess()};
    for (unsigned i = 0; i < sizeof hs / sizeof *hs; i++) {
        SET_ERR(0);
        BOOL r = U(ImpersonateNamedPipeClient)(hs[i]);
        DWORD e = GetLastError();
        if (r) RevertToSelf();
        CHECKF(!r, "handle %u", i);
        CHECKF(e == ERROR_INVALID_HANDLE, "handle %u err %lu", i, e);
    }
    CloseHandle(ev);
}
TEST(gap6_imppipe_regular_file) {
    REQUIRE(U(ImpersonateNamedPipeClient));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "g6_imp.txt");
    HANDLE f = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) { t_skip("create"); return; }
    SET_ERR(0);
    BOOL r = U(ImpersonateNamedPipeClient)(f);
    DWORD e = GetLastError();
    if (r) RevertToSelf();
    CHECK(!r);
    CHECKF(e == ERROR_INVALID_FUNCTION || e == ERROR_INVALID_HANDLE || e == ERROR_NOT_SUPPORTED, "err %lu", e);
    t_note("ImpersonateNamedPipeClient(file) err %lu", e);
    CloseHandle(f); DeleteFileA(p);
}
TEST(gap6_imppipe_closed_handle) {
    REQUIRE(U(ImpersonateNamedPipeClient));
    char n[128]; pipename(n, sizeof n, "closed");
    HANDLE s = CreateNamedPipeA(n, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 256, 256, 0, NULL);
    if (s == INVALID_HANDLE_VALUE) { t_skip("pipe"); return; }
    CloseHandle(s);
    SET_ERR(0);
    CHECK(!U(ImpersonateNamedPipeClient)(s));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap6_imppipe_listening_pipe_no_client) {
    REQUIRE(U(ImpersonateNamedPipeClient));
    char n[128]; pipename(n, sizeof n, "listen");
    HANDLE s = CreateNamedPipeA(n, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 256, 256, 0, NULL);
    if (s == INVALID_HANDLE_VALUE) { t_skip("pipe"); return; }
    SET_ERR(0);
    BOOL r = U(ImpersonateNamedPipeClient)(s);
    DWORD e = GetLastError();
    if (r) RevertToSelf();
    CHECK(!r);
    t_note("no client connected: err %lu", e);
    CHECK(!thread_has_token());
    CloseHandle(s);
}
TEST(gap6_imppipe_before_read_fails) {
    REQUIRE(U(ImpersonateNamedPipeClient));
    char n[128]; pipename(n, sizeof n, "noread");
    HANDLE s = CreateNamedPipeA(n, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 256, 256, 0, NULL);
    if (s == INVALID_HANDLE_VALUE) { t_skip("pipe"); return; }
    HANDLE c = CreateFileA(n, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, SECURITY_SQOS_PRESENT | SECURITY_IMPERSONATION, NULL);
    if (c == INVALID_HANDLE_VALUE) { t_skip("client"); CloseHandle(s); return; }
    ConnectNamedPipe(s, NULL);
    SET_ERR(0);
    BOOL r = U(ImpersonateNamedPipeClient)(s);
    DWORD e = GetLastError();
    if (r) RevertToSelf();
    CHECK(!r);
    t_note("before any data read: err %lu (documented ERROR_CANNOT_IMPERSONATE=1368)", e);
    CHECK(!thread_has_token());
    CloseHandle(c); CloseHandle(s);
}
TEST(gap6_imppipe_full_cycle) {
    REQUIRE(U(ImpersonateNamedPipeClient));
    char n[128]; pipename(n, sizeof n, "cycle");
    HANDLE s = CreateNamedPipeA(n, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 256, 256, 0, NULL);
    if (s == INVALID_HANDLE_VALUE) { t_skip("pipe"); return; }
    HANDLE c = CreateFileA(n, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, SECURITY_SQOS_PRESENT | SECURITY_IMPERSONATION, NULL);
    if (c == INVALID_HANDLE_VALUE) { t_skip("client"); CloseHandle(s); return; }
    ConnectNamedPipe(s, NULL);
    DWORD n1 = 0; char ch = 'x';
    CHECK(WriteFile(c, &ch, 1, &n1, NULL));
    char rb = 0;
    CHECK(ReadFile(s, &rb, 1, &n1, NULL));
    SET_ERR(0);
    BOOL r = U(ImpersonateNamedPipeClient)(s);
    DWORD e = GetLastError();
    BOOL has = r && thread_has_token();
    DWORD lv = 0, rl; HANDLE tt = NULL; BYTE ub[256], pb[256]; BOOL same = FALSE;
    if (r && OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &tt)) {
        GetTokenInformation(tt, TokenImpersonationLevel, &lv, 4, &rl);
        if (GetTokenInformation(tt, TokenUser, ub, sizeof ub, &rl)) {
            HANDLE pt = open_tok(TOKEN_QUERY);
            if (pt && GetTokenInformation(pt, TokenUser, pb, sizeof pb, &rl)) same = EqualSid(((TOKEN_USER *)ub)->User.Sid, ((TOKEN_USER *)pb)->User.Sid);
            if (pt) CloseHandle(pt);
        }
        CloseHandle(tt);
    }
    RevertToSelf();
    CHECKF(r, "err %lu", e);
    CHECK(has);
    EXPECT_EQ(lv, SecurityImpersonation);
    CHECK(same);
    CHECK(!thread_has_token());
    CloseHandle(c); CloseHandle(s);
}
TEST(gap6_imppipe_client_anonymous_level) {
    REQUIRE(U(ImpersonateNamedPipeClient));
    char n[128]; pipename(n, sizeof n, "anon");
    HANDLE s = CreateNamedPipeA(n, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 256, 256, 0, NULL);
    if (s == INVALID_HANDLE_VALUE) { t_skip("pipe"); return; }
    HANDLE c = CreateFileA(n, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS, NULL);
    if (c == INVALID_HANDLE_VALUE) { t_skip("client"); CloseHandle(s); return; }
    ConnectNamedPipe(s, NULL);
    DWORD n1; char ch = 'x', rb;
    WriteFile(c, &ch, 1, &n1, NULL); ReadFile(s, &rb, 1, &n1, NULL);
    BOOL r = U(ImpersonateNamedPipeClient)(s);
    DWORD e = GetLastError();
    DWORD lv = 99, rl; HANDLE tt = NULL;
    if (r && OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &tt)) { GetTokenInformation(tt, TokenImpersonationLevel, &lv, 4, &rl); CloseHandle(tt); }
    RevertToSelf();
    t_note("anonymous-level client: r=%d err=%lu level=%lu", r, e, lv);
    if (r && lv != 99) CHECK(lv == SecurityAnonymous);
    CloseHandle(c); CloseHandle(s);
}
TEST(gap6_imppipe_disconnected_client) {
    REQUIRE(U(ImpersonateNamedPipeClient));
    char n[128]; pipename(n, sizeof n, "gone");
    HANDLE s = CreateNamedPipeA(n, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 256, 256, 0, NULL);
    if (s == INVALID_HANDLE_VALUE) { t_skip("pipe"); return; }
    HANDLE c = CreateFileA(n, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, SECURITY_SQOS_PRESENT | SECURITY_IMPERSONATION, NULL);
    if (c == INVALID_HANDLE_VALUE) { t_skip("client"); CloseHandle(s); return; }
    ConnectNamedPipe(s, NULL);
    DWORD n1; char ch = 'x', rb;
    WriteFile(c, &ch, 1, &n1, NULL); ReadFile(s, &rb, 1, &n1, NULL);
    CloseHandle(c);
    SET_ERR(0);
    BOOL r = U(ImpersonateNamedPipeClient)(s);
    DWORD e = GetLastError();
    if (r) RevertToSelf();
    t_note("client closed: r=%d err=%lu", r, e);
    CHECK(!thread_has_token());
    CloseHandle(s);
}
TEST(gap6_imppipe_client_end_handle) {
    REQUIRE(U(ImpersonateNamedPipeClient));
    char n[128]; pipename(n, sizeof n, "clientend");
    HANDLE s = CreateNamedPipeA(n, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_WAIT, 1, 256, 256, 0, NULL);
    if (s == INVALID_HANDLE_VALUE) { t_skip("pipe"); return; }
    HANDLE c = CreateFileA(n, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, SECURITY_SQOS_PRESENT | SECURITY_IMPERSONATION, NULL);
    if (c == INVALID_HANDLE_VALUE) { t_skip("client"); CloseHandle(s); return; }
    ConnectNamedPipe(s, NULL);
    SET_ERR(0);
    BOOL r = U(ImpersonateNamedPipeClient)(c);
    DWORD e = GetLastError();
    if (r) RevertToSelf();
    CHECK(!r);
    t_note("client-end handle: err %lu", e);
    CHECK(!thread_has_token());
    CloseHandle(c); CloseHandle(s);
}
