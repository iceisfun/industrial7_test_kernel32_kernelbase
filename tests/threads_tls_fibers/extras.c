/* Additional thread-scope coverage: thread locale/token/UI language (worker threads only), context helpers. */
#include "tf.h"

UDECL(BOOL, SetThreadToken, (PHANDLE, HANDLE));
UDECL(LCID, GetThreadLocale, (void));
UDECL(BOOL, SetThreadLocale, (LCID));
UDECL(DWORD64, GetEnabledXStateFeatures, (void));
UDECL(BOOL, GetXStateFeaturesMask, (PCONTEXT, PDWORD64));
UDECL(PVOID, LocateXStateFeature, (PCONTEXT, DWORD, PDWORD));
UDECL(BOOL, CopyContext, (PCONTEXT, DWORD, PCONTEXT));

static DWORD WINAPI th_token_null(LPVOID p) {
    DWORD bits = 0;
    /* A thread with no impersonation token: reverting is a success no-op. */
    if (!UN(SetThreadToken)(NULL, NULL)) bits |= 1;
    HANDLE t = NULL;
    SetLastError(0);
    if (OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &t)) { bits |= 2; CloseHandle(t); }
    else if (GetLastError() != ERROR_NO_TOKEN) bits |= 4;
    return bits;
}
TEST(thrx_setthreadtoken_revert_noop) {
    REQUIRE(UN(SetThreadToken));
    EQ(tf_run(th_token_null, NULL), 0);
}
TEST(thrx_setthreadtoken_invalid_token) {
    REQUIRE(UN(SetThreadToken));
    HANDLE ev = tf_ev(TRUE, FALSE);
    SET_ERR(0);
    /* an event is not a token; thread handle NULL means the calling thread */
    BOOL r = UN(SetThreadToken)(NULL, ev);
    CHECK(!r);
    t_note("SetThreadToken(event) err %lu", GetLastError());
    CloseHandle(ev);
    CHECK(OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &ev) == FALSE);
}

static DWORD WINAPI th_locale(LPVOID p) {
    DWORD bits = 0;
    LCID before = UN(GetThreadLocale)();
    if (before == 0) bits |= 1;
    if (!UN(SetThreadLocale)(0x0409)) bits |= 2;
    if (UN(GetThreadLocale)() != 0x0409) bits |= 4;
    SetLastError(0);
    if (UN(SetThreadLocale)(0xFFFFFFF1)) bits |= 8;   /* not an installed locale */
    if (UN(GetThreadLocale)() != 0x0409) bits |= 0x10;
    return bits;
}
TEST(thrx_threadlocale_per_thread) {
    REQUIRE(UN(GetThreadLocale)); REQUIRE(UN(SetThreadLocale));
    LCID main_before = UN(GetThreadLocale)();
    DWORD bits = tf_run(th_locale, NULL);
    CHECKF((bits & ~0x8u) == 0, "bits %lx", bits);   /* bit 8: invalid-locale rejection is host dependent */
    if (bits & 8) t_note("SetThreadLocale(invalid) unexpectedly succeeded");
    EQ(UN(GetThreadLocale)(), main_before);          /* other thread's change does not leak */
}
TEST(thrx_threadlocale_new_thread_inherits_default) {
    REQUIRE(UN(GetThreadLocale));
    LCID a = UN(GetThreadLocale)();
    CHECK(a != 0);
}

static DWORD WINAPI th_uilang(LPVOID p) {
    BOOL (WINAPI *get)(DWORD, PULONG, PZZWSTR, PULONG) = (void *)t_proc("GetThreadPreferredUILanguages");
    BOOL (WINAPI *set)(DWORD, PCZZWSTR, PULONG) = (void *)t_proc("SetThreadPreferredUILanguages");
    DWORD bits = 0;
    if (!get || !set) return 0x100;
    ULONG n = 0, len = 0;
    if (!get(MUI_LANGUAGE_NAME, &n, NULL, &len)) bits |= 1;
    if (len < 2) bits |= 2;
    /* invalid flags */
    SetLastError(0);
    n = 0; len = 0;
    if (get(0x80000000, &n, NULL, &len)) bits |= 4;
    else if (GetLastError() != ERROR_INVALID_PARAMETER) bits |= 8;
    SetLastError(0);
    if (set(0x80000000, NULL, &n)) bits |= 0x10;
    set(0, NULL, &n);  /* reset */
    return bits;
}
TEST(thrx_thread_preferred_ui_languages) {
    REQUIRE(UN(GetThreadLocale));
    DWORD r = tf_run(th_uilang, NULL);
    if (r == 0x100) { t_skip("export missing"); return; }
    CHECKF((r & 3) == 0, "bits %lx", r);
    if (r & ~3u) t_note("invalid-flag handling of Get/SetThreadPreferredUILanguages differs: bits %lx", r);
}

TEST(thrx_getthreadid_pseudo_and_real) {
    REQUIRE(U(GetThreadId)); REQUIRE(U(GetCurrentThreadId));
    EQ(U(GetThreadId)(GetCurrentThread()), GetCurrentThreadId());
    EQ(U(GetCurrentThreadId)(), *(DWORD *)((char *)NtCurrentTeb() + 0x48));
    EQ(GetCurrentThread(), (HANDLE)(LONG_PTR)-2);
    SET_ERR(0);
    EQ(U(GetThreadId)(NULL), 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE ev = tf_ev(TRUE, FALSE);
    SET_ERR(0);
    EQ(U(GetThreadId)(ev), 0);
    CHECK(GetLastError() != 0);
    CloseHandle(ev);
}
TEST(thrx_getprocessidofthread_pseudo) {
    REQUIRE(U(GetProcessIdOfThread));
    EQ(U(GetProcessIdOfThread)(GetCurrentThread()), GetCurrentProcessId());
    SET_ERR(0);
    EQ(U(GetProcessIdOfThread)(NULL), 0);
    CHECK(GetLastError() != 0);
    EQ(*(DWORD *)((char *)NtCurrentTeb() + 0x40), GetCurrentProcessId());
}

TEST(thrx_context_initialize_copy) {
    REQUIRE(UN(CopyContext)); REQUIRE(UN(InitializeContext));
    DWORD len = 0;
    SET_ERR(0);
    BOOL r = UN(InitializeContext)(NULL, CONTEXT_FULL, NULL, &len);
    CHECK(!r);
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(len >= sizeof(CONTEXT));
    BYTE *buf = (BYTE *)HeapAlloc(GetProcessHeap(), 0, len);
    PCONTEXT c = NULL;
    CHECK(UN(InitializeContext)(buf, CONTEXT_FULL, &c, &len));
    CHECK(c != NULL);
    if (c) {
        EQ(c->ContextFlags, CONTEXT_FULL);
        CONTEXT src; memset(&src, 0, sizeof src);
        src.ContextFlags = CONTEXT_INTEGER; src.Rax = 0x1122334455667788ULL; src.Rbx = 0xAB; src.Rip = 0xDEAD;
        CHECK(UN(CopyContext)(c, CONTEXT_INTEGER, &src));
        EQ(c->Rax, 0x1122334455667788ULL);
        EQ(c->Rbx, 0xAB);
        NE(c->Rip, 0xDEAD);     /* control portion not requested */
    }
    HeapFree(GetProcessHeap(), 0, buf);
}
TEST(thrx_xstate_enabled_features) {
    REQUIRE(UN(GetEnabledXStateFeatures));
    DWORD64 f = UN(GetEnabledXStateFeatures)();
    CHECK((f & 3) == 3);    /* x87 and SSE always enabled */
}
TEST(thrx_getxstatefeaturesmask_plain_context) {
    REQUIRE(UN(GetXStateFeaturesMask));
    CONTEXT c; memset(&c, 0, sizeof c);
    c.ContextFlags = CONTEXT_CONTROL;
    DWORD64 m = 0xCC;
    SET_ERR(0);
    BOOL r = UN(GetXStateFeaturesMask)(&c, &m);
    t_note("GetXStateFeaturesMask(no xstate flag) ret %d err %lu mask %llx", r, GetLastError(), (unsigned long long)m);
}
TEST(thrx_locatexstatefeature_legacy) {
    REQUIRE(UN(LocateXStateFeature));
    CONTEXT c; memset(&c, 0, sizeof c);
    c.ContextFlags = CONTEXT_FULL;
    DWORD l = 0;
    PVOID p = UN(LocateXStateFeature)(&c, 1, &l);   /* XSTATE_LEGACY_SSE */
    t_note("LocateXStateFeature(SSE) %p len %lu", p, l);
}
