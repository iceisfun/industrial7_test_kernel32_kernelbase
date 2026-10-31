/* gap_10: QuerySecurityAccessMask, RevertToSelf, RestoreLastError, QueryProcessAffinityUpdateMode, QueryProtectedPolicy,
 * QueryWorkingSet(Ex), QueryUmsThreadInformation, ReadThreadProfilingData, Register/UnregisterBadMemoryNotification,
 * RemoveSecureMemoryCacheCallback, RegisterTraceGuidsW, RegDisablePredefinedCacheEx */
#include "g10.h"
#include <winternl.h>

/* ---------------- QuerySecurityAccessMask ---------------- */

#define SI_OWNER 0x1
#define SI_GROUP 0x2
#define SI_DACL  0x4
#define SI_SACL  0x8
#define SI_LABEL 0x10
#define SI_ATTRIBUTE 0x20
#define SI_SCOPE 0x40
#define SI_PROCESS_TRUST_LABEL 0x80
#define SI_ACCESS_FILTER 0x100
#define SI_BACKUP 0x10000

static DWORD qsam_model(DWORD si) {
    DWORD m = 0;
    if (si & SI_BACKUP) m = ACCESS_SYSTEM_SECURITY | READ_CONTROL;
    if (si & (SI_OWNER | SI_GROUP | SI_DACL | SI_LABEL | SI_ATTRIBUTE | SI_SCOPE | SI_PROCESS_TRUST_LABEL | SI_ACCESS_FILTER)) m |= READ_CONTROL;
    if (si & SI_SACL) m |= ACCESS_SYSTEM_SECURITY;
    return m;
}
#define WINE_QSAM() do { if (t_is_wine()) { t_skip("QuerySecurityAccessMask is an aborting stub in the emulator"); return; } } while (0)
static DWORD qsam(DWORD si) { DWORD m = 0xDEADBEEF; U(QuerySecurityAccessMask)(si, &m); return m; }

TEST(gap10_qsam_documented_values) {
    REQUIRE(U(QuerySecurityAccessMask)); WINE_QSAM();
    EXPECT_EQ(qsam(0), 0);
    EXPECT_EQ(qsam(OWNER_SECURITY_INFORMATION), READ_CONTROL);
    EXPECT_EQ(qsam(GROUP_SECURITY_INFORMATION), READ_CONTROL);
    EXPECT_EQ(qsam(DACL_SECURITY_INFORMATION), READ_CONTROL);
    EXPECT_EQ(qsam(SACL_SECURITY_INFORMATION), ACCESS_SYSTEM_SECURITY);
    EXPECT_EQ(qsam(LABEL_SECURITY_INFORMATION), READ_CONTROL);
    EXPECT_EQ(qsam(OWNER_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION), READ_CONTROL | ACCESS_SYSTEM_SECURITY);
    EXPECT_EQ(qsam(OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION), READ_CONTROL);
    EXPECT_EQ(qsam(OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION | LABEL_SECURITY_INFORMATION),
              READ_CONTROL | ACCESS_SYSTEM_SECURITY);
}
TEST(gap10_qsam_single_bits) {
    REQUIRE(U(QuerySecurityAccessMask)); WINE_QSAM();
    for (int b = 0; b < 32; b++) {
        DWORD si = 1u << b, got = qsam(si);
        CHECKF(got == qsam_model(si), "bit %d: got 0x%lx want 0x%lx", b, got, qsam_model(si));
    }
}
TEST(gap10_qsam_unprotected_protected_bits_ignored) {
    REQUIRE(U(QuerySecurityAccessMask)); WINE_QSAM();
    EXPECT_EQ(qsam(PROTECTED_DACL_SECURITY_INFORMATION), 0);
    EXPECT_EQ(qsam(PROTECTED_SACL_SECURITY_INFORMATION), 0);
    EXPECT_EQ(qsam(UNPROTECTED_DACL_SECURITY_INFORMATION), 0);
    EXPECT_EQ(qsam(UNPROTECTED_SACL_SECURITY_INFORMATION), 0);
    EXPECT_EQ(qsam(DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION), READ_CONTROL);
    EXPECT_EQ(qsam(0x200 | 0x400 | 0x800 | 0x1000), 0);
}
TEST(gap10_qsam_backup_bit) {
    REQUIRE(U(QuerySecurityAccessMask)); WINE_QSAM();
    EXPECT_EQ(qsam(SI_BACKUP), READ_CONTROL | ACCESS_SYSTEM_SECURITY);
    EXPECT_EQ(qsam(SI_BACKUP | SI_OWNER), READ_CONTROL | ACCESS_SYSTEM_SECURITY);
    EXPECT_EQ(qsam(SI_BACKUP | SI_SACL), READ_CONTROL | ACCESS_SYSTEM_SECURITY);
}
TEST(gap10_qsam_extended_info_classes_need_read_control) {
    REQUIRE(U(QuerySecurityAccessMask)); WINE_QSAM();
    EXPECT_EQ(qsam(SI_ATTRIBUTE), READ_CONTROL);
    EXPECT_EQ(qsam(SI_SCOPE), READ_CONTROL);
    EXPECT_EQ(qsam(SI_PROCESS_TRUST_LABEL), READ_CONTROL);
    EXPECT_EQ(qsam(SI_ACCESS_FILTER), READ_CONTROL);
}
TEST(gap10_qsam_all_combinations_of_low_bits) {
    REQUIRE(U(QuerySecurityAccessMask)); WINE_QSAM();
    int bad = 0;
    for (DWORD si = 0; si < 0x2000; si++) if (qsam(si) != qsam_model(si)) bad++;
    EQ1(bad, 0);
}
TEST(gap10_qsam_pseudo_random_values) {
    REQUIRE(U(QuerySecurityAccessMask)); WINE_QSAM();
    DWORD x = 0x1234567; int bad = 0;
    for (int i = 0; i < 5000; i++) { x = x * 1664525u + 1013904223u; if (qsam(x) != qsam_model(x)) bad++; }
    EQ1(bad, 0);
}
TEST(gap10_qsam_overwrites_output) {
    REQUIRE(U(QuerySecurityAccessMask)); WINE_QSAM();
    DWORD m = 0xFFFFFFFF;
    U(QuerySecurityAccessMask)(0, &m);
    EQ1(m, 0);
    m = 0xFFFFFFFF;
    U(QuerySecurityAccessMask)(OWNER_SECURITY_INFORMATION, &m);
    EQ1(m, READ_CONTROL);
}
TEST(gap10_qsam_does_not_touch_last_error) {
    REQUIRE(U(QuerySecurityAccessMask)); WINE_QSAM();
    DWORD m; SET_ERR(0x4242);
    U(QuerySecurityAccessMask)(DACL_SECURITY_INFORMATION, &m);
    EXPECT_ERR(0x4242);
}
TEST(gap10_qsam_vs_setsecurityaccessmask_differs) {
    REQUIRE(U(QuerySecurityAccessMask)); WINE_QSAM(); REQUIRE(U(SetSecurityAccessMask));
    /* reading needs READ_CONTROL, writing needs WRITE_DAC for the same info */
    DWORD q = 0, s = 0;
    U(QuerySecurityAccessMask)(DACL_SECURITY_INFORMATION, &q);
    U(SetSecurityAccessMask)(DACL_SECURITY_INFORMATION, &s);
    CHECK(q != s);
    EXPECT_EQ(q, READ_CONTROL);
    EXPECT_EQ(s, WRITE_DAC);
}

/* ---------------- RevertToSelf ---------------- */

TEST(gap10_revert_without_impersonation) {
    REQUIRE(U(RevertToSelf));
    SET_ERR(0x1357);
    BOOL r = U(RevertToSelf)();
    EXPECT_EQ(r, TRUE);
    EXPECT_ERR(0x1357);
}
TEST(gap10_revert_repeated) {
    REQUIRE(U(RevertToSelf));
    for (int i = 0; i < 20; i++) { BOOL r = U(RevertToSelf)(); EQ1(r, TRUE); }
}
TEST(gap10_revert_after_impersonate_self) {
    REQUIRE(U(RevertToSelf));
    HANDLE tok = NULL;
    SET_ERR(0);
    if (!ImpersonateSelf(SecurityImpersonation)) { t_skip("ImpersonateSelf failed %lu", GetLastError()); return; }
    CHECK(OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &tok));
    if (tok) CloseHandle(tok);
    BOOL r = U(RevertToSelf)();
    EXPECT_EQ(r, TRUE);
    tok = NULL; SET_ERR(0);
    BOOL o = OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &tok);
    EXPECT_EQ(o, FALSE);
    EXPECT_ERR(ERROR_NO_TOKEN);
    if (tok) CloseHandle(tok);
}
TEST(gap10_revert_after_set_thread_token) {
    REQUIRE(U(RevertToSelf));
    HANDLE ptok = NULL, dup = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_IMPERSONATE, &ptok)) { t_skip("no process token"); return; }
    if (!DuplicateTokenEx(ptok, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenImpersonation, &dup)) { CloseHandle(ptok); t_skip("dup failed"); return; }
    CHECK(SetThreadToken(NULL, dup));
    HANDLE t2 = NULL;
    CHECK(OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &t2));
    if (t2) CloseHandle(t2);
    BOOL r = U(RevertToSelf)();
    EXPECT_EQ(r, TRUE);
    t2 = NULL; SET_ERR(0);
    { BOOL o2 = OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &t2); EQ1(o2, FALSE); }
    EXPECT_ERR(ERROR_NO_TOKEN);
    /* the duplicated token stays valid after the revert */
    DWORD len = 0;
    CHECK(GetTokenInformation(dup, TokenType, &len, sizeof len, &len));
    CloseHandle(dup); CloseHandle(ptok);
}
TEST(gap10_revert_is_per_thread) {
    REQUIRE(U(RevertToSelf));
    if (!ImpersonateSelf(SecurityImpersonation)) { t_skip("ImpersonateSelf failed"); return; }
    DWORD WINAPI th(LPVOID p) {
        /* the other thread is not impersonating; reverting there must not affect the caller */
        BOOL (WINAPI *rv)(void) = (BOOL (WINAPI *)(void))p;
        return rv() ? 1 : 0;
    }
    HANDLE t = CreateThread(NULL, 0, th, (LPVOID)U(RevertToSelf), 0, NULL);
    WaitForSingleObject(t, 5000);
    DWORD code = 0; GetExitCodeThread(t, &code); CloseHandle(t);
    EXPECT_EQ(code, 1);
    HANDLE tok = NULL;
    CHECK(OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &tok));
    if (tok) CloseHandle(tok);
    U(RevertToSelf)();
}
TEST(gap10_revert_thread_impersonation_then_exit_clean) {
    REQUIRE(U(RevertToSelf));
    DWORD WINAPI th(LPVOID p) {
        if (!ImpersonateSelf(SecurityImpersonation)) return 2;
        BOOL r = ((BOOL (WINAPI *)(void))p)();
        HANDLE tok = NULL;
        BOOL o = OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &tok);
        if (tok) CloseHandle(tok);
        return (r && !o && GetLastError() == ERROR_NO_TOKEN) ? 1 : 0;
    }
    HANDLE t = CreateThread(NULL, 0, th, (LPVOID)U(RevertToSelf), 0, NULL);
    WaitForSingleObject(t, 5000);
    DWORD code = 0; GetExitCodeThread(t, &code); CloseHandle(t);
    CHECKF(code == 1 || code == 2, "thread result %lu", code);
}

/* ---------------- RestoreLastError ---------------- */

UDECL(void, RestoreLastError_, (DWORD));
TEST(gap10_restorelasterror_values) {
    void (WINAPI *f)(DWORD) = (void (WINAPI *)(DWORD))t_proc_k32("RestoreLastError");
    REQUIRE(f);
    static const DWORD v[] = { 0, 1, 2, 5, 6, 87, 122, 183, 234, 259, 997, 998, 1008, 0x4242, 0xFFFF, 0x10000, 0x7FFFFFFF, 0x80000000, 0xC0000005, 0xDEADBEEF, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof v / sizeof v[0]; i++) {
        SET_ERR(0x31337);
        f(v[i]);
        DWORD e = GetLastError();
        CHECKF(e == v[i], "RestoreLastError(0x%lx) -> GetLastError 0x%lx", v[i], e);
    }
}
TEST(gap10_restorelasterror_is_per_thread) {
    void (WINAPI *f)(DWORD) = (void (WINAPI *)(DWORD))t_proc_k32("RestoreLastError");
    REQUIRE(f);
    static void (WINAPI *g_f)(DWORD);
    g_f = f;
    DWORD WINAPI th(LPVOID p) { SetLastError(0); g_f(0x777); return GetLastError(); }
    SET_ERR(0x111);
    HANDLE t = CreateThread(NULL, 0, th, NULL, 0, NULL);
    WaitForSingleObject(t, 5000);
    DWORD code = 0; GetExitCodeThread(t, &code); CloseHandle(t);
    EXPECT_EQ(code, 0x777);
    EXPECT_ERR(0x111);
}
TEST(gap10_restorelasterror_roundtrip_preserves_error) {
    void (WINAPI *f)(DWORD) = (void (WINAPI *)(DWORD))t_proc_k32("RestoreLastError");
    REQUIRE(f); REQUIRE(U(CloseHandle));
    SET_ERR(0x2468);
    DWORD saved = GetLastError();
    U(CloseHandle)((HANDLE)(ULONG_PTR)0x7ffe0); /* some failing call clobbers it */
    CHECK(GetLastError() != 0x2468);
    f(saved);
    EXPECT_ERR(0x2468);
}
TEST(gap10_restorelasterror_matches_teb) {
    void (WINAPI *f)(DWORD) = (void (WINAPI *)(DWORD))t_proc_k32("RestoreLastError");
    REQUIRE(f);
    f(0x5A5A);
    EXPECT_EQ(*(DWORD *)((BYTE *)NtCurrentTeb() + 0x68), 0x5A5A);
    f(0);
    EXPECT_EQ(*(DWORD *)((BYTE *)NtCurrentTeb() + 0x68), 0);
}
TEST(gap10_restorelasterror_repeated_same_value) {
    void (WINAPI *f)(DWORD) = (void (WINAPI *)(DWORD))t_proc_k32("RestoreLastError");
    REQUIRE(f);
    for (int i = 0; i < 1000; i++) { f(0x42); }
    EXPECT_ERR(0x42);
}

/* ---------------- QueryProcessAffinityUpdateMode ---------------- */

static HANDLE open_sleeper(PROCESS_INFORMATION *pi) {
    return t_spawn_child("gap10_sleep", "20000", 0, NULL, NULL, NULL, FALSE, NULL, pi);
}
static void kill_sleeper(PROCESS_INFORMATION *pi) {
    TerminateProcess(pi->hProcess, 0);
    WaitForSingleObject(pi->hProcess, 5000);
    CloseHandle(pi->hProcess);
}
TEST(gap10_qpaum_current_process) {
    if (t_is_wine()) { t_skip("QueryProcessAffinityUpdateMode is an aborting stub in the emulator"); return; }
    REQUIRE(U(QueryProcessAffinityUpdateMode));
    DWORD f = 0xFFFFFFFF; SET_ERR(0xBAD);
    BOOL r = U(QueryProcessAffinityUpdateMode)(GetCurrentProcess(), &f);
    EXPECT_EQ(r, TRUE);
    CHECKF(f <= 1, "flags 0x%lx", f);
    t_note("affinity update mode: %lu", f);
}
TEST(gap10_qpaum_real_handle_and_pseudo_handle_agree) {
    if (t_is_wine()) { t_skip("QueryProcessAffinityUpdateMode is an aborting stub in the emulator"); return; }
    REQUIRE(U(QueryProcessAffinityUpdateMode));
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, GetCurrentProcessId());
    REQUIRE(h);
    DWORD a = 0xAA, b = 0xBB;
    EQ1(U(QueryProcessAffinityUpdateMode)(h, &a), TRUE);
    EQ1(U(QueryProcessAffinityUpdateMode)(GetCurrentProcess(), &b), TRUE);
    EQ1(a, b);
    CloseHandle(h);
}
TEST(gap10_qpaum_null_flags_pointer_ok) {
    if (t_is_wine()) { t_skip("QueryProcessAffinityUpdateMode is an aborting stub in the emulator"); return; }
    REQUIRE(U(QueryProcessAffinityUpdateMode));
    SET_ERR(0xBAD);
    BOOL r = U(QueryProcessAffinityUpdateMode)(GetCurrentProcess(), NULL);
    EXPECT_EQ(r, TRUE);
}
TEST(gap10_qpaum_invalid_handles) {
    if (t_is_wine()) { t_skip("QueryProcessAffinityUpdateMode is an aborting stub in the emulator"); return; }
    REQUIRE(U(QueryProcessAffinityUpdateMode));
    DWORD f = 0x1234;
    SET_ERR(0xBAD); BOOL r = U(QueryProcessAffinityUpdateMode)(NULL, &f);
    EQ1(r, 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE cl = CreateEventA(NULL, FALSE, FALSE, NULL); CloseHandle(cl);
    SET_ERR(0xBAD); r = U(QueryProcessAffinityUpdateMode)(cl, &f);
    EQ1(r, 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0xBAD); r = U(QueryProcessAffinityUpdateMode)((HANDLE)(ULONG_PTR)0x12344, &f);
    EQ1(r, 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    EXPECT_EQ(f, 0x1234);   /* untouched on failure */
}
TEST(gap10_qpaum_wrong_handle_type) {
    if (t_is_wine()) { t_skip("QueryProcessAffinityUpdateMode is an aborting stub in the emulator"); return; }
    REQUIRE(U(QueryProcessAffinityUpdateMode));
    HANDLE ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    DWORD f = 0x1234;
    SET_ERR(0xBAD); BOOL r = U(QueryProcessAffinityUpdateMode)(ev, &f);
    EQ1(r, 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0xBAD); r = U(QueryProcessAffinityUpdateMode)(GetCurrentThread(), &f);
    EQ1(r, 0);
    t_note("thread pseudo-handle as process: err=%lu", GetLastError());
    CloseHandle(ev);
}
TEST(gap10_qpaum_access_rights) {
    if (t_is_wine()) { t_skip("QueryProcessAffinityUpdateMode is an aborting stub in the emulator"); return; }
    REQUIRE(U(QueryProcessAffinityUpdateMode));
    PROCESS_INFORMATION pi;
    if (!open_sleeper(&pi)) { t_skip("cannot spawn child"); return; }
    HANDLE sync = OpenProcess(SYNCHRONIZE, FALSE, pi.dwProcessId);
    HANDLE lim = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pi.dwProcessId);
    HANDLE qi = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pi.dwProcessId);
    DWORD f = 0x1234;
    if (sync) { SET_ERR(0xBAD); BOOL r = U(QueryProcessAffinityUpdateMode)(sync, &f); EQ1(r, 0); EXPECT_ERR(ERROR_ACCESS_DENIED); EXPECT_EQ(f, 0x1234); CloseHandle(sync); }
    if (lim) { SET_ERR(0xBAD); BOOL r = U(QueryProcessAffinityUpdateMode)(lim, &f); t_note("PROCESS_QUERY_LIMITED_INFORMATION: ret=%d err=%lu", r, r ? 0 : GetLastError()); CloseHandle(lim); }
    if (qi) { f = 0xFF; BOOL r = U(QueryProcessAffinityUpdateMode)(qi, &f); EQ1(r, TRUE); CHECK(f <= 1); CloseHandle(qi); }
    kill_sleeper(&pi);
}
TEST(gap10_qpaum_after_process_exit) {
    if (t_is_wine()) { t_skip("QueryProcessAffinityUpdateMode is an aborting stub in the emulator"); return; }
    REQUIRE(U(QueryProcessAffinityUpdateMode));
    PROCESS_INFORMATION pi;
    HANDLE h = t_spawn_child("echo", "0", 0, NULL, NULL, NULL, FALSE, NULL, &pi);
    if (!h) { t_skip("cannot spawn child"); return; }
    WaitForSingleObject(h, 10000);
    DWORD f = 0x55; SET_ERR(0xBAD);
    BOOL r = U(QueryProcessAffinityUpdateMode)(h, &f);
    t_note("exited process: ret=%d err=%lu flags=%lu", r, r ? 0 : GetLastError(), f);
    CloseHandle(pi.hThread); CloseHandle(h);
}
UDECL(BOOL, SetProcessAffinityUpdateMode, (HANDLE, DWORD));
CHILD(gap10_affupd) {
    SetProcessAffinityUpdateMode_pfn setf = UN(SetProcessAffinityUpdateMode);
    if (!setf || !U(QueryProcessAffinityUpdateMode)) return 3;
    DWORD f = 0xFF;
    if (!U(QueryProcessAffinityUpdateMode)(GetCurrentProcess(), &f)) return 4;
    if (f != 0) return 9;                       /* disabled by default; anything else is only noted */
    if (!setf(GetCurrentProcess(), 1)) return 6;
    f = 0xFF;
    if (!U(QueryProcessAffinityUpdateMode)(GetCurrentProcess(), &f)) return 7;
    if (f != 1) return 8;
    return 0;
}
TEST(gap10_qpaum_reflects_set_in_child) {
    if (t_is_wine()) { t_skip("QueryProcessAffinityUpdateMode is an aborting stub in the emulator"); return; }
    REQUIRE(U(QueryProcessAffinityUpdateMode)); REQUIRE(UN(SetProcessAffinityUpdateMode));
    DWORD c = t_run_child("gap10_affupd", NULL, 20000);
    if (c == 6) { t_note("SetProcessAffinityUpdateMode(ENABLE) failed in child"); return; }
    if (c == 9) { t_note("affinity update mode already enabled at start in child"); return; }
    CHECKF(c == 0, "child exit %lu (3 missing,4 query,5 default!=0,7 query2,8 flag not set)", c);
}

/* ---------------- QueryProtectedPolicy ---------------- */

UDECL(BOOL, QueryProtectedPolicy, (const GUID *, ULONG_PTR *));
UDECL(BOOL, SetProtectedPolicy, (const GUID *, ULONG_PTR, ULONG_PTR *));
static const GUID g_pg1 = { 0x6b32c0de, 0xaaaa, 0x4321, { 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78 } };
static const GUID g_pg2 = { 0x6b32c0de, 0xbbbb, 0x4321, { 0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78 } };
TEST(gap10_qpp_unknown_guid) {
    QueryProtectedPolicy_pfn f = UN(QueryProtectedPolicy);
    REQUIRE(f);
    ULONG_PTR v = 0x5151; SET_ERR(0xBAD);
    BOOL r = f(&g_pg1, &v);
    EQ1(r, 0); EXPECT_ERR(ERROR_NOT_FOUND);
    EXPECT_EQ(v, 0x5151);
}
TEST(gap10_qpp_unknown_guid_repeated_stable) {
    QueryProtectedPolicy_pfn f = UN(QueryProtectedPolicy);
    REQUIRE(f);
    for (int i = 0; i < 20; i++) { ULONG_PTR v = 7; SET_ERR(0); BOOL r = f(&g_pg2, &v); EQ1(r, 0); EXPECT_ERR(ERROR_NOT_FOUND); }
}
TEST(gap10_qpp_zero_guid) {
    QueryProtectedPolicy_pfn f = UN(QueryProtectedPolicy);
    REQUIRE(f);
    GUID z; memset(&z, 0, sizeof z);
    ULONG_PTR v = 7; SET_ERR(0xBAD);
    BOOL r = f(&z, &v);
    EQ1(r, 0); EXPECT_ERR(ERROR_NOT_FOUND);
}
CHILD(gap10_protpol) {
    QueryProtectedPolicy_pfn q = UN(QueryProtectedPolicy);
    SetProtectedPolicy_pfn s = UN(SetProtectedPolicy);
    if (!q || !s) return 3;
    ULONG_PTR old = 0x99, v = 0;
    if (!s(&g_pg1, 0x1234, &old)) return 10 + (int)GetLastError() % 100;
    if (!q(&g_pg1, &v) || v != 0x1234) return 4;
    if (q(&g_pg2, &v)) return 5;
    if (GetLastError() != ERROR_NOT_FOUND) return 6;
    if (!s(&g_pg2, 0x5678, &old)) return 7;
    if (!q(&g_pg2, &v) || v != 0x5678) return 8;
    if (!q(&g_pg1, &v) || v != 0x1234) return 9;
    return 0;
}
TEST(gap10_qpp_set_then_query_in_child) {
    REQUIRE(UN(QueryProtectedPolicy)); REQUIRE(UN(SetProtectedPolicy));
    DWORD c = t_run_child("gap10_protpol", NULL, 20000);
    if (c >= 10 && c < 110) { t_note("SetProtectedPolicy failed in child (err %lu)", c - 10); return; }
    CHECKF(c == 0, "child exit %lu", c);
}

/* ---------------- QueryWorkingSet / QueryWorkingSetEx ---------------- */

static PSAPI_WORKING_SET_INFORMATION *qws_alloc(HANDLE h, DWORD *cbout) {
    DWORD n = 1024;
    for (int tries = 0; tries < 16; tries++) {
        DWORD cb = (DWORD)(sizeof(ULONG_PTR) + n * sizeof(PSAPI_WORKING_SET_BLOCK));
        PSAPI_WORKING_SET_INFORMATION *p = (PSAPI_WORKING_SET_INFORMATION *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cb);
        if (!p) return NULL;
        if (U(QueryWorkingSet)(h, p, cb)) { *cbout = cb; return p; }
        DWORD e = GetLastError();
        ULONG_PTR need = p->NumberOfEntries;
        HeapFree(GetProcessHeap(), 0, p);
        if (e != ERROR_BAD_LENGTH) return NULL;
        n = (DWORD)need + 256 * (tries + 1);
    }
    return NULL;
}
TEST(gap10_qws_too_small_reports_needed) {
    if (t_is_wine()) { t_skip("QueryWorkingSet is not backed by NtQueryVirtualMemory in the emulator"); return; }
    REQUIRE(U(QueryWorkingSet));
    PSAPI_WORKING_SET_INFORMATION hdr; memset(&hdr, 0, sizeof hdr);
    SET_ERR(0xBAD);
    BOOL r = U(QueryWorkingSet)(GetCurrentProcess(), &hdr, sizeof(ULONG_PTR));
    EQ1(r, 0); EXPECT_ERR(ERROR_BAD_LENGTH);
    CHECKF(hdr.NumberOfEntries > 10, "NumberOfEntries %llu", (unsigned long long)hdr.NumberOfEntries);
}
TEST(gap10_qws_one_entry_buffer_still_too_small) {
    if (t_is_wine()) { t_skip("QueryWorkingSet is not backed by NtQueryVirtualMemory in the emulator"); return; }
    REQUIRE(U(QueryWorkingSet));
    PSAPI_WORKING_SET_INFORMATION *p = (PSAPI_WORKING_SET_INFORMATION *)calloc(1, sizeof(PSAPI_WORKING_SET_INFORMATION));
    SET_ERR(0xBAD);
    BOOL r = U(QueryWorkingSet)(GetCurrentProcess(), p, sizeof(PSAPI_WORKING_SET_INFORMATION));
    EQ1(r, 0); EXPECT_ERR(ERROR_BAD_LENGTH);
    CHECK(p->NumberOfEntries > 1);
    free(p);
}
TEST(gap10_qws_tiny_cb_values) {
    if (t_is_wine()) { t_skip("QueryWorkingSet is not backed by NtQueryVirtualMemory in the emulator"); return; }
    REQUIRE(U(QueryWorkingSet));
    char buf[64]; memset(buf, 0, sizeof buf);
    static const DWORD cbs[] = { 0, 1, 4, 7 };
    for (size_t i = 0; i < 4; i++) {
        SET_ERR(0xBAD);
        BOOL r = U(QueryWorkingSet)(GetCurrentProcess(), buf, cbs[i]);
        EQ1(r, 0); EXPECT_ERR(ERROR_BAD_LENGTH);
    }
}
TEST(gap10_qws_full_query_succeeds) {
    if (t_is_wine()) { t_skip("QueryWorkingSet is not backed by NtQueryVirtualMemory in the emulator"); return; }
    REQUIRE(U(QueryWorkingSet));
    DWORD cb = 0;
    PSAPI_WORKING_SET_INFORMATION *p = qws_alloc(GetCurrentProcess(), &cb);
    CHECK(p != NULL);
    if (!p) return;
    CHECK(p->NumberOfEntries > 10);
    CHECK(sizeof(ULONG_PTR) + p->NumberOfEntries * sizeof(PSAPI_WORKING_SET_BLOCK) <= cb);
    int zero = 0;
    for (ULONG_PTR i = 0; i < p->NumberOfEntries; i++) if (p->WorkingSetInfo[i].VirtualPage == 0) zero++;
    CHECKF(zero <= 1, "entries with page 0: %d", zero);
    HeapFree(GetProcessHeap(), 0, p);
}
TEST(gap10_qws_finds_locked_page) {
    if (t_is_wine()) { t_skip("QueryWorkingSet is not backed by NtQueryVirtualMemory in the emulator"); return; }
    REQUIRE(U(QueryWorkingSet)); REQUIRE(U(VirtualLock)); REQUIRE(U(VirtualUnlock));
    BYTE *page = (BYTE *)VirtualAlloc(NULL, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    REQUIRE(page);
    page[0] = 1;
    BOOL locked = U(VirtualLock)(page, 4096);
    if (!locked) { t_note("VirtualLock failed %lu", GetLastError()); VirtualFree(page, 0, MEM_RELEASE); return; }
    DWORD cb = 0;
    PSAPI_WORKING_SET_INFORMATION *p = qws_alloc(GetCurrentProcess(), &cb);
    CHECK(p != NULL);
    int found = 0;
    if (p) for (ULONG_PTR i = 0; i < p->NumberOfEntries; i++) if (p->WorkingSetInfo[i].VirtualPage == ((ULONG_PTR)page >> 12)) found++;
    EQ1(found, 1);
    if (p) HeapFree(GetProcessHeap(), 0, p);
    U(VirtualUnlock)(page, 4096);
    VirtualFree(page, 0, MEM_RELEASE);
}
TEST(gap10_qws_reserved_page_not_listed) {
    if (t_is_wine()) { t_skip("QueryWorkingSet is not backed by NtQueryVirtualMemory in the emulator"); return; }
    REQUIRE(U(QueryWorkingSet));
    BYTE *page = (BYTE *)VirtualAlloc(NULL, 4096, MEM_RESERVE, PAGE_READWRITE);
    REQUIRE(page);
    DWORD cb = 0;
    PSAPI_WORKING_SET_INFORMATION *p = qws_alloc(GetCurrentProcess(), &cb);
    int found = 0;
    if (p) for (ULONG_PTR i = 0; i < p->NumberOfEntries; i++) if (p->WorkingSetInfo[i].VirtualPage == ((ULONG_PTR)page >> 12)) found++;
    EQ1(found, 0);
    if (p) HeapFree(GetProcessHeap(), 0, p);
    VirtualFree(page, 0, MEM_RELEASE);
}
TEST(gap10_qws_invalid_handles) {
    if (t_is_wine()) { t_skip("QueryWorkingSet is not backed by NtQueryVirtualMemory in the emulator"); return; }
    REQUIRE(U(QueryWorkingSet));
    char buf[256];
    HANDLE cl = CreateEventA(NULL, FALSE, FALSE, NULL); CloseHandle(cl);
    HANDLE bad[] = { NULL, cl, (HANDLE)(ULONG_PTR)0x12344 };
    for (size_t i = 0; i < 3; i++) { SET_ERR(0xBAD); BOOL r = U(QueryWorkingSet)(bad[i], buf, sizeof buf); EQ1(r, 0); EXPECT_ERR(ERROR_INVALID_HANDLE); }
    HANDLE ev = CreateEventA(NULL, FALSE, FALSE, NULL);
    SET_ERR(0xBAD); BOOL r = U(QueryWorkingSet)(ev, buf, sizeof buf); EQ1(r, 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
}
TEST(gap10_qws_access_rights) {
    if (t_is_wine()) { t_skip("QueryWorkingSet is not backed by NtQueryVirtualMemory in the emulator"); return; }
    REQUIRE(U(QueryWorkingSet));
    PROCESS_INFORMATION pi;
    if (!open_sleeper(&pi)) { t_skip("cannot spawn child"); return; }
    HANDLE lim = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pi.dwProcessId);
    HANDLE full = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pi.dwProcessId);
    HANDLE qionly = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pi.dwProcessId);
    char small[8] = {0};
    if (lim) { SET_ERR(0xBAD); BOOL r = U(QueryWorkingSet)(lim, small, 8); EQ1(r, 0); EXPECT_ERR(ERROR_ACCESS_DENIED); CloseHandle(lim); }
    if (full) {
        DWORD cb = 0;
        PSAPI_WORKING_SET_INFORMATION *p = qws_alloc(full, &cb);
        CHECK(p != NULL);
        if (p) { CHECK(p->NumberOfEntries > 10); HeapFree(GetProcessHeap(), 0, p); }
        CloseHandle(full);
    }
    if (qionly) {
        SET_ERR(0xBAD); BOOL r = U(QueryWorkingSet)(qionly, small, 8);
        t_note("QUERY_INFORMATION only: ret=%d err=%lu", r, GetLastError());
        CloseHandle(qionly);
    }
    kill_sleeper(&pi);
}
TEST(gap10_qws_other_process_differs_from_self) {
    if (t_is_wine()) { t_skip("QueryWorkingSet is not backed by NtQueryVirtualMemory in the emulator"); return; }
    REQUIRE(U(QueryWorkingSet));
    PROCESS_INFORMATION pi;
    if (!open_sleeper(&pi)) { t_skip("cannot spawn child"); return; }
    HANDLE full = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pi.dwProcessId);
    if (full) {
        DWORD cb1 = 0, cb2 = 0;
        PSAPI_WORKING_SET_INFORMATION *a = qws_alloc(full, &cb1), *b = qws_alloc(GetCurrentProcess(), &cb2);
        CHECK(a && b);
        if (a && b) {
            /* the test image page of this process is not in the child's mapping with the same state, but both have entries */
            CHECK(a->NumberOfEntries > 5); CHECK(b->NumberOfEntries > 5);
        }
        if (a) HeapFree(GetProcessHeap(), 0, a);
        if (b) HeapFree(GetProcessHeap(), 0, b);
        CloseHandle(full);
    }
    kill_sleeper(&pi);
}
TEST(gap10_qws_repeated_consistent) {
    if (t_is_wine()) { t_skip("QueryWorkingSet is not backed by NtQueryVirtualMemory in the emulator"); return; }
    REQUIRE(U(QueryWorkingSet));
    for (int i = 0; i < 20; i++) {
        DWORD cb = 0;
        PSAPI_WORKING_SET_INFORMATION *p = qws_alloc(GetCurrentProcess(), &cb);
        CHECK(p != NULL);
        if (p) HeapFree(GetProcessHeap(), 0, p);
    }
}

TEST(gap10_qwsex_locked_page_valid) {
    REQUIRE(U(QueryWorkingSetEx)); REQUIRE(U(VirtualLock)); REQUIRE(U(VirtualUnlock));
    BYTE *page = (BYTE *)VirtualAlloc(NULL, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    REQUIRE(page);
    page[0] = 1;
    if (!U(VirtualLock)(page, 4096)) { t_note("VirtualLock failed %lu", GetLastError()); VirtualFree(page, 0, MEM_RELEASE); return; }
    PSAPI_WORKING_SET_EX_INFORMATION e; memset(&e, 0, sizeof e);
    e.VirtualAddress = page;
    SET_ERR(0xBAD);
    BOOL r = U(QueryWorkingSetEx)(GetCurrentProcess(), &e, sizeof e);
    EQ1(r, TRUE);
    EQ1(e.VirtualAttributes.Valid, 1);
    EQ1(e.VirtualAttributes.Win32Protection, PAGE_READWRITE);
    EQ1(e.VirtualAttributes.Shared, 0);
    EQ1(e.VirtualAttributes.Bad, 0);
    t_note("locked page attrs: 0x%llx locked=%u sharecount=%u node=%u", (unsigned long long)e.VirtualAttributes.Flags, e.VirtualAttributes.Locked, e.VirtualAttributes.ShareCount, e.VirtualAttributes.Node);
    U(VirtualUnlock)(page, 4096);
    VirtualFree(page, 0, MEM_RELEASE);
}
TEST(gap10_qwsex_reserved_and_committed_untouched) {
    REQUIRE(U(QueryWorkingSetEx));
    BYTE *res = (BYTE *)VirtualAlloc(NULL, 4096, MEM_RESERVE, PAGE_READWRITE);
    REQUIRE(res);
    PSAPI_WORKING_SET_EX_INFORMATION e; memset(&e, 0xFF, sizeof e);
    e.VirtualAddress = res;
    BOOL r = U(QueryWorkingSetEx)(GetCurrentProcess(), &e, sizeof e);
    EQ1(r, TRUE);
    EQ1(e.VirtualAttributes.Valid, 0);
    EQ1(e.VirtualAddress, res);
    VirtualFree(res, 0, MEM_RELEASE);
}
TEST(gap10_qwsex_free_address_not_valid) {
    REQUIRE(U(QueryWorkingSetEx));
    PSAPI_WORKING_SET_EX_INFORMATION e; memset(&e, 0xFF, sizeof e);
    e.VirtualAddress = (PVOID)(ULONG_PTR)0x10000;
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(e.VirtualAddress, &mbi, sizeof mbi) && mbi.State != MEM_FREE) { t_skip("0x10000 in use"); return; }
    BOOL r = U(QueryWorkingSetEx)(GetCurrentProcess(), &e, sizeof e);
    EQ1(r, TRUE);
    EQ1(e.VirtualAttributes.Valid, 0);
}
TEST(gap10_qwsex_null_address) {
    REQUIRE(U(QueryWorkingSetEx));
    PSAPI_WORKING_SET_EX_INFORMATION e; memset(&e, 0xFF, sizeof e);
    e.VirtualAddress = NULL;
    BOOL r = U(QueryWorkingSetEx)(GetCurrentProcess(), &e, sizeof e);
    EQ1(r, TRUE);
    EQ1(e.VirtualAttributes.Valid, 0);
}
TEST(gap10_qwsex_multiple_pages) {
    REQUIRE(U(QueryWorkingSetEx));
    enum { N = 8 };
    BYTE *base = (BYTE *)VirtualAlloc(NULL, N * 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    REQUIRE(base);
    for (int i = 0; i < N; i += 2) base[i * 4096] = 1;   /* touch even pages only */
    DWORD old;
    VirtualProtect(base + 4 * 4096, 4096, PAGE_READONLY, &old);
    PSAPI_WORKING_SET_EX_INFORMATION e[N]; memset(e, 0, sizeof e);
    for (int i = 0; i < N; i++) e[i].VirtualAddress = base + i * 4096;
    BOOL r = U(QueryWorkingSetEx)(GetCurrentProcess(), e, sizeof e);
    EQ1(r, TRUE);
    for (int i = 0; i < N; i++) EQ1(e[i].VirtualAddress, base + i * 4096);
    /* never-touched pages are demand-zero and not resident */
    for (int i = 1; i < N; i += 2) EQ1(e[i].VirtualAttributes.Valid, 0);
    t_note("page4 (readonly, touched) valid=%u win32prot=0x%x", e[4].VirtualAttributes.Valid, e[4].VirtualAttributes.Win32Protection);
    if (e[4].VirtualAttributes.Valid) EQ1(e[4].VirtualAttributes.Win32Protection, PAGE_READONLY);
    VirtualFree(base, 0, MEM_RELEASE);
}
TEST(gap10_qwsex_length_not_multiple) {
    if (t_is_wine()) { t_skip("emulator error mapping differs"); return; }
    REQUIRE(U(QueryWorkingSetEx));
    PSAPI_WORKING_SET_EX_INFORMATION e[2]; memset(e, 0, sizeof e);
    e[0].VirtualAddress = e; e[1].VirtualAddress = e;
    static const DWORD cbs[] = { 1, 8, 15 };
    for (size_t i = 0; i < sizeof cbs / sizeof cbs[0]; i++) {
        SET_ERR(0xBAD);
        BOOL r = U(QueryWorkingSetEx)(GetCurrentProcess(), e, cbs[i]);
        CHECKF(r == 0 && GetLastError() == ERROR_BAD_LENGTH, "cb=%lu ret=%d err=%lu", cbs[i], r, GetLastError());
    }
}
TEST(gap10_qwsex_length_larger_not_multiple) {
    REQUIRE(U(QueryWorkingSetEx));
    if (t_is_wine()) { t_skip("emulator differs"); return; }
    PSAPI_WORKING_SET_EX_INFORMATION e[2]; memset(e, 0, sizeof e);
    e[0].VirtualAddress = e; e[1].VirtualAddress = e;
    static const DWORD cbs[] = { 17, 24, 31 };
    for (size_t i = 0; i < 3; i++) { SET_ERR(0xBAD); BOOL r = U(QueryWorkingSetEx)(GetCurrentProcess(), e, cbs[i]); t_note("QueryWorkingSetEx(cb %lu): ret=%d err=%lu", cbs[i], r, r ? 0 : GetLastError()); }
}
TEST(gap10_qwsex_zero_length) {
    REQUIRE(U(QueryWorkingSetEx));
    PSAPI_WORKING_SET_EX_INFORMATION e; memset(&e, 0, sizeof e);
    SET_ERR(0xBAD);
    BOOL r = U(QueryWorkingSetEx)(GetCurrentProcess(), &e, 0);
    t_note("QueryWorkingSetEx(cb 0): ret=%d err=%lu", r, GetLastError());
}
TEST(gap10_qwsex_invalid_handles) {
    if (t_is_wine()) { t_skip("emulator error mapping differs"); return; }
    REQUIRE(U(QueryWorkingSetEx));
    PSAPI_WORKING_SET_EX_INFORMATION e; memset(&e, 0, sizeof e);
    HANDLE cl = CreateEventA(NULL, FALSE, FALSE, NULL); CloseHandle(cl);
    HANDLE bad[] = { NULL, cl, (HANDLE)(ULONG_PTR)0x12344 };
    for (size_t i = 0; i < 3; i++) { SET_ERR(0xBAD); BOOL r = U(QueryWorkingSetEx)(bad[i], &e, sizeof e); EQ1(r, 0); EXPECT_ERR(ERROR_INVALID_HANDLE); }
}
TEST(gap10_qwsex_access_denied_limited) {
    if (t_is_wine()) { t_skip("emulator error mapping differs"); return; }
    REQUIRE(U(QueryWorkingSetEx));
    PROCESS_INFORMATION pi;
    if (!open_sleeper(&pi)) { t_skip("cannot spawn child"); return; }
    HANDLE lim = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pi.dwProcessId);
    PSAPI_WORKING_SET_EX_INFORMATION e; memset(&e, 0, sizeof e);
    e.VirtualAddress = &e;
    if (lim) { SET_ERR(0xBAD); BOOL r = U(QueryWorkingSetEx)(lim, &e, sizeof e); EQ1(r, 0); EXPECT_ERR(ERROR_ACCESS_DENIED); CloseHandle(lim); }
    kill_sleeper(&pi);
}
TEST(gap10_qwsex_stack_variable_is_resident) {
    REQUIRE(U(QueryWorkingSetEx));
    volatile int local = 5; (void)local;
    PSAPI_WORKING_SET_EX_INFORMATION e; memset(&e, 0, sizeof e);
    e.VirtualAddress = (PVOID)&local;
    BOOL r = U(QueryWorkingSetEx)(GetCurrentProcess(), &e, sizeof e);
    EQ1(r, TRUE);
    EQ1(e.VirtualAttributes.Valid, 1);
}
TEST(gap10_qwsex_image_page_is_shared_or_valid) {
    REQUIRE(U(QueryWorkingSetEx));
    PSAPI_WORKING_SET_EX_INFORMATION e; memset(&e, 0, sizeof e);
    PVOID pc; __asm__ volatile("lea (%%rip), %0" : "=r"(pc));
    e.VirtualAddress = pc;
    BOOL r = U(QueryWorkingSetEx)(GetCurrentProcess(), &e, sizeof e);
    EQ1(r, TRUE);
    EQ1(e.VirtualAttributes.Valid, 1);
    t_note("test image code page: shared=%u protection=0x%x sharecount=%u", e.VirtualAttributes.Shared, e.VirtualAttributes.Win32Protection, e.VirtualAttributes.ShareCount);
    EQ1(e.VirtualAttributes.Win32Protection, PAGE_EXECUTE_READ);
}
TEST(gap10_qwsex_many_pages_one_call) {
    REQUIRE(U(QueryWorkingSetEx));
    enum { N = 1024 };
    PSAPI_WORKING_SET_EX_INFORMATION *e = (PSAPI_WORKING_SET_EX_INFORMATION *)calloc(N, sizeof *e);
    BYTE *base = (BYTE *)VirtualAlloc(NULL, N * 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!e || !base) { t_skip("alloc"); free(e); return; }
    for (int i = 0; i < N; i++) e[i].VirtualAddress = base + (size_t)i * 4096;
    BOOL r = U(QueryWorkingSetEx)(GetCurrentProcess(), e, N * sizeof *e);
    EQ1(r, TRUE);
    int bad = 0; for (int i = 0; i < N; i++) if (e[i].VirtualAttributes.Valid) bad++;
    EQ1(bad, 0);
    VirtualFree(base, 0, MEM_RELEASE); free(e);
}

/* ---------------- QueryUmsThreadInformation ---------------- */

TEST(gap10_ums_unsupported_classes_fail_before_use) {
    if (t_is_wine()) { t_skip("UMS is not implemented in the emulator"); return; }
    REQUIRE(U(QueryUmsThreadInformation));
    BYTE buf[16];
    static const DWORD cls[] = { 0, 2, 3, 7, 8, 100, 0x7FFFFFFF, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof cls / sizeof cls[0]; i++) {
        ULONG rl = 0x77; SET_ERR(0xBAD);
        BOOL r = U(QueryUmsThreadInformation)(NULL, (UMS_THREAD_INFO_CLASS)cls[i], buf, 8, &rl);
        CHECKF(r == 0 && GetLastError() == ERROR_NOT_SUPPORTED && rl == 0, "class %lu: ret=%d err=%lu retlen=%lu", cls[i], r, GetLastError(), rl);
    }
}
TEST(gap10_ums_wrong_length_fails_before_use) {
    if (t_is_wine()) { t_skip("UMS is not implemented in the emulator"); return; }
    REQUIRE(U(QueryUmsThreadInformation));
    BYTE buf[32];
    static const struct { DWORD cls, len; } t[] = {
        { 1, 0 }, { 1, 4 }, { 1, 16 }, { 4, 0 }, { 4, 7 }, { 4, 9 }, { 5, 0 }, { 5, 2 }, { 5, 8 }, { 6, 0 }, { 6, 4 }, { 6, 32 } };
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++) {
        ULONG rl = 0x77; SET_ERR(0xBAD);
        BOOL r = U(QueryUmsThreadInformation)(NULL, (UMS_THREAD_INFO_CLASS)t[i].cls, buf, t[i].len, &rl);
        CHECKF(r == 0 && GetLastError() == ERROR_BAD_LENGTH && rl == 0, "class %lu len %lu: ret=%d err=%lu retlen=%lu", t[i].cls, t[i].len, r, GetLastError(), rl);
    }
}
TEST(gap10_ums_null_return_length_ok) {
    if (t_is_wine()) { t_skip("UMS is not implemented in the emulator"); return; }
    REQUIRE(U(QueryUmsThreadInformation));
    BYTE buf[16];
    SET_ERR(0xBAD);
    BOOL r = U(QueryUmsThreadInformation)(NULL, (UMS_THREAD_INFO_CLASS)0, buf, 8, NULL);
    EQ1(r, 0); EXPECT_ERR(ERROR_NOT_SUPPORTED);
    SET_ERR(0xBAD);
    r = U(QueryUmsThreadInformation)(NULL, (UMS_THREAD_INFO_CLASS)5, buf, 4, NULL);
    EQ1(r, 0); EXPECT_ERR(ERROR_BAD_LENGTH);
}
TEST(gap10_ums_valid_context_queries) {
    REQUIRE(U(QueryUmsThreadInformation)); REQUIRE(U(CreateUmsThreadContext)); REQUIRE(U(DeleteUmsThreadContext));
    if (t_is_wine()) { t_skip("UMS not implemented in the emulator"); return; }
    PUMS_CONTEXT ctx = NULL;
    SET_ERR(0xBAD);
    if (!U(CreateUmsThreadContext)(&ctx)) { t_note("CreateUmsThreadContext failed %lu", GetLastError()); return; }
    CHECK(ctx != NULL);
    ULONG rl = 0; BYTE b = 0xAA; PVOID ptr = (PVOID)(ULONG_PTR)0x1234;
    BOOL r = U(QueryUmsThreadInformation)(ctx, (UMS_THREAD_INFO_CLASS)5, &b, 1, &rl);
    EQ1(r, TRUE); EQ1(rl, 1); CHECK(b <= 1);
    t_note("fresh UMS context IsSuspended=%u", b);
    b = 0xAA; rl = 0;
    r = U(QueryUmsThreadInformation)(ctx, (UMS_THREAD_INFO_CLASS)6, &b, 1, &rl);
    EQ1(r, TRUE); EQ1(rl, 1); CHECK(b <= 1);
    t_note("fresh UMS context IsTerminated=%u", b);
    rl = 0;
    r = U(QueryUmsThreadInformation)(ctx, (UMS_THREAD_INFO_CLASS)1, &ptr, sizeof ptr, &rl);
    EQ1(r, TRUE); EQ1(rl, sizeof ptr);
    t_note("fresh UMS context UserContext=%p", ptr);
    rl = 0; ptr = (PVOID)(ULONG_PTR)0x1234;
    r = U(QueryUmsThreadInformation)(ctx, (UMS_THREAD_INFO_CLASS)4, &ptr, sizeof ptr, &rl);
    EQ1(r, TRUE); EQ1(rl, sizeof ptr);
    t_note("fresh UMS context Teb=%p", ptr);
    /* unsupported classes still fail on a valid context */
    SET_ERR(0xBAD);
    r = U(QueryUmsThreadInformation)(ctx, (UMS_THREAD_INFO_CLASS)2, &ptr, sizeof ptr, &rl);
    EQ1(r, 0); EXPECT_ERR(ERROR_NOT_SUPPORTED);
    BOOL d = U(DeleteUmsThreadContext)(ctx);
    t_note("DeleteUmsThreadContext(unused context) -> %d err=%lu", d, d ? 0 : GetLastError());
}

/* ---------------- ReadThreadProfilingData ---------------- */

typedef struct { WORD Size; BYTE Version; BYTE HwCountersCount; DWORD ContextSwitchCount; DWORD64 WaitReasonBitMap; DWORD64 CycleTime; DWORD RetryCount; DWORD Reserved; BYTE HwCounters[256]; } PD_RAW;

TEST(gap10_rtpd_struct_size_is_0x120) {
    CHECK(sizeof(PD_RAW) == 0x120);
    CHECK(sizeof(PERFORMANCE_DATA) == 0x120);
}
TEST(gap10_rtpd_bad_size) {
    REQUIRE(U(ReadThreadProfilingData));
    if (t_is_wine()) { t_skip("not implemented in the emulator"); return; }
    PD_RAW pd;
    static const WORD sz[] = { 0, 1, 0x11F, 0x121, 0x118, 0x200, 0xFFFF };
    for (size_t i = 0; i < sizeof sz / sizeof sz[0]; i++) {
        memset(&pd, 0, sizeof pd); pd.Size = sz[i]; pd.Version = 1;
        DWORD r = U(ReadThreadProfilingData)(NULL, 0, (PPERFORMANCE_DATA)&pd);
        CHECKF(r != 0, "size 0x%x returned 0", sz[i]);
        if (i == 0) t_note("ReadThreadProfilingData(bad size) -> %lu", r);
    }
}
TEST(gap10_rtpd_bad_version) {
    REQUIRE(U(ReadThreadProfilingData));
    if (t_is_wine()) { t_skip("not implemented in the emulator"); return; }
    PD_RAW pd;
    static const BYTE ver[] = { 0, 2, 3, 0x80, 0xFF };
    for (size_t i = 0; i < sizeof ver; i++) {
        memset(&pd, 0, sizeof pd); pd.Size = 0x120; pd.Version = ver[i];
        DWORD r = U(ReadThreadProfilingData)(NULL, 0, (PPERFORMANCE_DATA)&pd);
        CHECKF(r == ERROR_INVALID_PARAMETER, "version %u returned %lu", ver[i], r);
    }
}
TEST(gap10_rtpd_bad_flags) {
    REQUIRE(U(ReadThreadProfilingData));
    if (t_is_wine()) { t_skip("not implemented in the emulator"); return; }
    PD_RAW pd;
    static const DWORD fl[] = { 4, 8, 0x10, 0x100, 0x80000000, 0xFFFFFFFF, 0xFFFFFFFC };
    for (size_t i = 0; i < sizeof fl / sizeof fl[0]; i++) {
        memset(&pd, 0, sizeof pd); pd.Size = 0x120; pd.Version = 1;
        DWORD r = U(ReadThreadProfilingData)(NULL, fl[i], (PPERFORMANCE_DATA)&pd);
        CHECKF(r == ERROR_INVALID_PARAMETER, "flags 0x%lx returned %lu", fl[i], r);
    }
}
TEST(gap10_rtpd_size_checked_before_version) {
    REQUIRE(U(ReadThreadProfilingData));
    if (t_is_wine()) { t_skip("not implemented in the emulator"); return; }
    PD_RAW pd; memset(&pd, 0, sizeof pd); pd.Size = 0x10; pd.Version = 7;
    DWORD a = U(ReadThreadProfilingData)(NULL, 0xFF, (PPERFORMANCE_DATA)&pd);
    memset(&pd, 0, sizeof pd); pd.Size = 0x120; pd.Version = 7;
    DWORD b = U(ReadThreadProfilingData)(NULL, 0xFF, (PPERFORMANCE_DATA)&pd);
    CHECK(a != 0 && b == ERROR_INVALID_PARAMETER);
    t_note("bad size -> %lu, bad version -> %lu", a, b);
}
TEST(gap10_rtpd_enabled_thread_profiling) {
    REQUIRE(U(ReadThreadProfilingData)); REQUIRE(U(EnableThreadProfiling)); REQUIRE(U(DisableThreadProfiling));
    if (t_is_wine()) { t_skip("not implemented in the emulator"); return; }
    HANDLE h = NULL;
    DWORD e = U(EnableThreadProfiling)(GetCurrentThread(), THREAD_PROFILING_FLAG_DISPATCH, 0, &h);
    if (e != 0 || !h) { t_note("EnableThreadProfiling -> %lu (profiling unavailable)", e); return; }
    PD_RAW pd; memset(&pd, 0, sizeof pd); pd.Size = 0x120; pd.Version = 1;
    DWORD r = U(ReadThreadProfilingData)(h, READ_THREAD_PROFILING_FLAG_DISPATCHING, (PPERFORMANCE_DATA)&pd);
    EQ1(r, 0);
    EQ1(pd.Size, 0x120); EQ1(pd.Version, 1);
    CHECK(pd.CycleTime != 0);
    t_note("profiling data: switches=%lu waitmask=0x%llx cycles=%llu", pd.ContextSwitchCount, (unsigned long long)pd.WaitReasonBitMap, (unsigned long long)pd.CycleTime);
    DWORD64 c1 = pd.CycleTime;
    memset(&pd, 0, sizeof pd); pd.Size = 0x120; pd.Version = 1;
    r = U(ReadThreadProfilingData)(h, 0, (PPERFORMANCE_DATA)&pd);
    EQ1(r, 0);
    CHECK(pd.CycleTime >= c1);
    r = U(DisableThreadProfiling)(h);
    EQ1(r, 0);
}

/* ---------------- Register/UnregisterBadMemoryNotification ---------------- */

static volatile LONG g_bad_calls;
static VOID CALLBACK bad_cb(VOID) { InterlockedIncrement(&g_bad_calls); }
UDECL(PVOID, RegisterBadMemoryNotification_, (PVOID));
TEST(gap10_badmem_register_unregister) {
    REQUIRE(U(RegisterBadMemoryNotification)); REQUIRE(U(UnregisterBadMemoryNotification));
    PVOID h = U(RegisterBadMemoryNotification)(bad_cb);
    if (!h) { t_note("RegisterBadMemoryNotification returned NULL (err %lu)", GetLastError()); return; }
    CHECK(h != INVALID_HANDLE_VALUE);
    BOOL r = U(UnregisterBadMemoryNotification)(h);
    EQ1(r, TRUE);
    EQ1(g_bad_calls, 0);
}
TEST(gap10_badmem_multiple_registrations_distinct) {
    REQUIRE(U(RegisterBadMemoryNotification)); REQUIRE(U(UnregisterBadMemoryNotification));
    PVOID h[4];
    for (int i = 0; i < 4; i++) h[i] = U(RegisterBadMemoryNotification)(bad_cb);
    if (!h[0]) { t_note("registration unavailable"); return; }
    for (int i = 0; i < 4; i++) for (int j = i + 1; j < 4; j++) CHECK(h[i] != h[j]);
    for (int i = 0; i < 4; i++) if (h[i]) EQ1(U(UnregisterBadMemoryNotification)(h[i]), TRUE);
}
TEST(gap10_badmem_register_unregister_loop) {
    REQUIRE(U(RegisterBadMemoryNotification)); REQUIRE(U(UnregisterBadMemoryNotification));
    PVOID first = U(RegisterBadMemoryNotification)(bad_cb);
    if (!first) { t_note("registration unavailable"); return; }
    U(UnregisterBadMemoryNotification)(first);
    for (int i = 0; i < 200; i++) {
        PVOID h = U(RegisterBadMemoryNotification)(bad_cb);
        CHECK(h != NULL);
        if (h) { BOOL r = U(UnregisterBadMemoryNotification)(h); EQ1(r, TRUE); }
    }
}
TEST(gap10_badmem_unregister_null) {
    REQUIRE(U(UnregisterBadMemoryNotification));
    SET_ERR(0xBAD);
    BOOL r = U(UnregisterBadMemoryNotification)(NULL);
    t_note("UnregisterBadMemoryNotification(NULL) -> %d err=%lu", r, GetLastError());
}
TEST(gap10_badmem_callback_not_invoked_spuriously) {
    REQUIRE(U(RegisterBadMemoryNotification)); REQUIRE(U(UnregisterBadMemoryNotification));
    g_bad_calls = 0;
    PVOID h = U(RegisterBadMemoryNotification)(bad_cb);
    if (!h) { t_note("registration unavailable"); return; }
    Sleep(300);
    EQ1(g_bad_calls, 0);
    U(UnregisterBadMemoryNotification)(h);
}

/* ---------------- Add/RemoveSecureMemoryCacheCallback ---------------- */

static BOOLEAN NTAPI smc_a(PVOID a, SIZE_T s) { return TRUE; }
static BOOLEAN NTAPI smc_b(PVOID a, SIZE_T s) { return TRUE; }
TEST(gap10_smc_remove_unregistered) {
    REQUIRE(U(RemoveSecureMemoryCacheCallback));
    BOOL r = U(RemoveSecureMemoryCacheCallback)(smc_a);
    EQ1(r, 0);
}
TEST(gap10_smc_remove_null) {
    REQUIRE(U(RemoveSecureMemoryCacheCallback));
    BOOL r = U(RemoveSecureMemoryCacheCallback)(NULL);
    EQ1(r, 0);
}
TEST(gap10_smc_add_then_remove) {
    REQUIRE(U(RemoveSecureMemoryCacheCallback)); REQUIRE(U(AddSecureMemoryCacheCallback));
    BOOL a = U(AddSecureMemoryCacheCallback)(smc_a);
    EQ1(a, TRUE);
    EQ1(U(RemoveSecureMemoryCacheCallback)(smc_a), TRUE);
    EQ1(U(RemoveSecureMemoryCacheCallback)(smc_a), FALSE);
}
TEST(gap10_smc_two_distinct_callbacks) {
    REQUIRE(U(RemoveSecureMemoryCacheCallback)); REQUIRE(U(AddSecureMemoryCacheCallback));
    EQ1(U(AddSecureMemoryCacheCallback)(smc_a), TRUE);
    EQ1(U(AddSecureMemoryCacheCallback)(smc_b), TRUE);
    EQ1(U(RemoveSecureMemoryCacheCallback)(smc_b), TRUE);
    EQ1(U(RemoveSecureMemoryCacheCallback)(smc_b), FALSE);
    EQ1(U(RemoveSecureMemoryCacheCallback)(smc_a), TRUE);
    EQ1(U(RemoveSecureMemoryCacheCallback)(smc_a), FALSE);
}
TEST(gap10_smc_add_twice_remove_count) {
    REQUIRE(U(RemoveSecureMemoryCacheCallback)); REQUIRE(U(AddSecureMemoryCacheCallback));
    BOOL a1 = U(AddSecureMemoryCacheCallback)(smc_a), a2 = U(AddSecureMemoryCacheCallback)(smc_a);
    EQ1(a1, TRUE); t_note("second add of the same callback -> %d", a2);
    int removed = 0;
    for (int i = 0; i < 4; i++) if (U(RemoveSecureMemoryCacheCallback)(smc_a)) removed++;
    t_note("same callback removable %d times", removed);
    CHECK(removed >= 1 && removed <= 2);
}
TEST(gap10_smc_loop) {
    REQUIRE(U(RemoveSecureMemoryCacheCallback)); REQUIRE(U(AddSecureMemoryCacheCallback));
    for (int i = 0; i < 300; i++) {
        BOOL a = U(AddSecureMemoryCacheCallback)(i & 1 ? smc_a : smc_b);
        CHECK(a);
        BOOL r = U(RemoveSecureMemoryCacheCallback)(i & 1 ? smc_a : smc_b);
        CHECK(r);
    }
}

/* ---------------- RegisterTraceGuidsW (kernelbase ETW stub) ---------------- */

TEST(gap10_rtg_w_stub_result) {
    ULONG (WINAPI *f)(PVOID, PVOID, LPCGUID, ULONG, PVOID, LPCWSTR, LPCWSTR, PVOID) = (void *)t_proc_kb("RegisterTraceGuidsW");
    REQUIRE(f);
    if (t_is_wine()) { t_skip("forwarded to a real implementation in the emulator"); return; }
    SET_ERR(0xBAD);
    ULONG r = f(NULL, NULL, NULL, 0, NULL, NULL, NULL, NULL);
    t_note("kernelbase RegisterTraceGuidsW(all NULL) -> %lu (last error %lu)", r, GetLastError());
    CHECK(r != ERROR_SUCCESS);
    CHECK(r == ERROR_PROC_NOT_FOUND);
    EXPECT_ERR(ERROR_PROC_NOT_FOUND);
}
TEST(gap10_rtg_w_not_exported_by_kernel32) {
    CHECK(t_proc_kb("RegisterTraceGuidsW") != NULL || t_is_wine());
    t_note("kernel32 RegisterTraceGuidsW export: %s", t_proc_k32("RegisterTraceGuidsW") ? "present" : "absent");
}
TEST(gap10_rtg_w_same_stub_as_other_etw_functions) {
    void *a = (void *)t_proc_kb("RegisterTraceGuidsW"), *b = (void *)t_proc_kb("CloseTrace"), *c = (void *)t_proc_kb("UnregisterTraceGuids");
    if (!a || !b) { t_skip("exports missing"); return; }
    if (t_is_wine()) { t_skip("not stubs in the emulator"); return; }
    CHECK(a == b);
    if (c) CHECK(a == c);
}

/* ---------------- RegDisablePredefinedCacheEx ---------------- */

CHILD(gap10_regpredef) {
    LSTATUS (WINAPI *ex)(void) = (LSTATUS (WINAPI *)(void))t_proc("RegDisablePredefinedCacheEx");
    if (!ex) return 3;
    LSTATUS r1 = ex();
    if (r1 != 0) return 4;
    LSTATUS r2 = ex();
    if (r2 != 0) return 5;
    HKEY k = NULL;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software", 0, KEY_READ, &k) != 0) return 6;
    RegCloseKey(k);
    k = NULL;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software", 0, KEY_READ, &k) != 0) return 7;
    RegCloseKey(k);
    k = NULL;
    if (RegOpenKeyExA(HKEY_CLASSES_ROOT, "CLSID", 0, KEY_READ, &k) != 0) return 8;
    RegCloseKey(k);
    if (ex() != 0) return 9;
    return 0;
}
TEST(gap10_regdisablepredef_ex_in_child) {
    REQUIRE(U(RegDisablePredefinedCacheEx));
    DWORD c = t_run_child("gap10_regpredef", NULL, 20000);
    CHECKF(c == 0, "child exit %lu (3 missing, 4/5/9 returned error, 6/7/8 open failed)", c);
}
CHILD(gap10_regpredef_old) {
    LSTATUS (WINAPI *ex)(void) = (LSTATUS (WINAPI *)(void))t_proc("RegDisablePredefinedCacheEx");
    LSTATUS (WINAPI *old)(void) = (LSTATUS (WINAPI *)(void))t_proc("RegDisablePredefinedCache");
    if (!ex || !old) return 3;
    if (old() != 0) return 4;
    if (ex() != 0) return 5;
    if (old() != 0) return 6;
    HKEY k = NULL;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software", 0, KEY_READ, &k) != 0) return 7;
    RegCloseKey(k);
    return 0;
}
TEST(gap10_regdisablepredef_ex_and_legacy_combo_in_child) {
    REQUIRE(U(RegDisablePredefinedCacheEx)); REQUIRE(U(RegDisablePredefinedCache));
    DWORD c = t_run_child("gap10_regpredef_old", NULL, 20000);
    CHECKF(c == 0, "child exit %lu", c);
}
