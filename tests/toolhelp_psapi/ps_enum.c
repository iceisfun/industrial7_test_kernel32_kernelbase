/* K32EnumProcesses, K32EnumProcessModules(Ex), K32GetModuleBaseName*, K32GetModuleFileNameEx*, K32GetModuleInformation */
#include "thp.h"

static const char *k32names[] = {
    "K32EmptyWorkingSet", "K32EnumDeviceDrivers", "K32EnumPageFilesA", "K32EnumPageFilesW", "K32EnumProcessModules",
    "K32EnumProcessModulesEx", "K32EnumProcesses", "K32GetDeviceDriverBaseNameA", "K32GetDeviceDriverBaseNameW",
    "K32GetDeviceDriverFileNameA", "K32GetDeviceDriverFileNameW", "K32GetMappedFileNameA", "K32GetMappedFileNameW",
    "K32GetModuleBaseNameA", "K32GetModuleBaseNameW", "K32GetModuleFileNameExA", "K32GetModuleFileNameExW",
    "K32GetModuleInformation", "K32GetPerformanceInfo", "K32GetProcessImageFileNameA", "K32GetProcessImageFileNameW",
    "K32GetProcessMemoryInfo", "K32GetWsChanges", "K32GetWsChangesEx", "K32InitializeProcessForWsWatch",
    "K32QueryWorkingSet", "K32QueryWorkingSetEx" };

TEST(ps_exports_in_kernel32) {
    for (unsigned i = 0; i < sizeof k32names / sizeof *k32names; i++)
        CHECKF(t_proc_k32(k32names[i]) != NULL, "kernel32!%s", k32names[i]);
}
TEST(ps_exports_in_kernelbase) {
    for (unsigned i = 0; i < sizeof k32names / sizeof *k32names; i++)
        CHECKF(t_proc_kb(k32names[i]) != NULL, "kernelbase!%s", k32names[i]);
}
TEST(ps_exports_same_target) {
    /* kernel32 forwards the K32* exports to kernelbase */
    for (unsigned i = 0; i < sizeof k32names / sizeof *k32names; i++) {
        FARPROC a = t_proc_k32(k32names[i]), b = t_proc_kb(k32names[i]);
        CHECKF(a == b, "%s: k32=%p kb=%p", k32names[i], a, b);
    }
}
TEST(ps_exports_plain_names_in_kernelbase) {
    static const char *p[] = { "EnumProcesses", "EnumProcessModules", "EnumProcessModulesEx", "GetModuleBaseNameW",
        "GetModuleFileNameExW", "GetModuleInformation", "GetProcessMemoryInfo", "GetPerformanceInfo", "GetMappedFileNameW",
        "GetProcessImageFileNameW", "QueryWorkingSet", "QueryWorkingSetEx", "EnumDeviceDrivers" };
    for (unsigned i = 0; i < sizeof p / sizeof *p; i++) CHECKF(t_proc_kb(p[i]) != NULL, "kernelbase!%s", p[i]);
}

/* ------- K32EnumProcesses ------- */
TEST(ps_enumprocesses_basic) {
    REQUIRE(U(K32EnumProcesses));
    DWORD ids[4096], need = 0xCCCCCCCC; SET_ERR(0);
    CHECK(U(K32EnumProcesses)(ids, sizeof ids, &need));
    CHECK(need > 0 && need % 4 == 0 && need < sizeof ids);
    int n = need / 4, self = 0, sys = 0;
    for (int i = 0; i < n; i++) { if (ids[i] == GetCurrentProcessId()) self++; if (ids[i] == 4) sys++; }
    EXPECT_EQ(self, 1);
    if (!t_is_wine()) { EXPECT_EQ(sys, 1); EXPECT_EQ(ids[0], 0); }        /* System Idle Process first */
}
TEST(ps_enumprocesses_no_duplicates) {
    REQUIRE(U(K32EnumProcesses));
    DWORD ids[4096], need = 0;
    CHECK(U(K32EnumProcesses)(ids, sizeof ids, &need));
    int n = need / 4, dup = 0;
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) if (ids[i] == ids[j]) dup++;
    CHECK(dup == 0);
}
TEST(ps_enumprocesses_cb_zero) {
    REQUIRE(U(K32EnumProcesses));
    DWORD need = 0xCCCCCCCC; SET_ERR(0);
    DWORD sentinel = 0xDEADBEEF;
    CHECK(U(K32EnumProcesses)(&sentinel, 0, &need));
    EXPECT_EQ(need, 0);
    EXPECT_EQ(sentinel, 0xDEADBEEF);
    need = 0xCCCCCCCC;
    CHECK(U(K32EnumProcesses)(NULL, 0, &need));
    EXPECT_EQ(need, 0);
}
TEST(ps_enumprocesses_cb_rounding) {
    REQUIRE(U(K32EnumProcesses));
    for (DWORD cb = 1; cb <= 12; cb++) {
        DWORD ids[8]; memset(ids, 0xEE, sizeof ids); DWORD need = 0xCCCCCCCC;
        CHECKF(U(K32EnumProcesses)(ids, cb, &need), "cb=%lu", cb);
        DWORD want = (cb / 4) * 4;
        CHECKF(need == want, "cb=%lu need=%lu want=%lu", cb, need, want);
        for (DWORD i = want / 4; i < 8; i++) CHECKF(ids[i] == 0xEEEEEEEE, "cb=%lu slot %lu clobbered", cb, i);
    }
}
TEST(ps_enumprocesses_truncation_not_signalled) {
    REQUIRE(U(K32EnumProcesses));
    DWORD ids[4096], need = 0, small[2];
    CHECK(U(K32EnumProcesses)(ids, sizeof ids, &need));
    DWORD full = need;
    CHECK(full > 8);
    DWORD n2 = 0;
    SET_ERR(0);
    CHECK(U(K32EnumProcesses)(small, sizeof small, &n2));   /* success even though truncated */
    EXPECT_EQ(n2, sizeof small);                            /* returned size == cb: no "needed" indication */
    if (!t_is_wine()) EXPECT_EQ(small[0], 0);
    CHECK(small[1] == ids[1] || small[1] != 0);
}
TEST(ps_enumprocesses_matches_toolhelp) {
    REQUIRE(U(K32EnumProcesses)); REQUIRE(U(CreateToolhelp32Snapshot));
    static PROCESSENTRY32W l[THP_MAXPROC]; DWORD ids[4096], need;
    int best = 1000;
    for (int a = 0; a < 5 && best; a++) {
        int n = thp_procs(l, THP_MAXPROC);
        CHECK(U(K32EnumProcesses)(ids, sizeof ids, &need));
        int c = need / 4, diff = 0;
        for (int i = 0; i < n; i++) { int f = 0; for (int j = 0; j < c; j++) if (ids[j] == l[i].th32ProcessID) f = 1; if (!f) diff++; }
        for (int j = 0; j < c; j++) { int f = 0; for (int i = 0; i < n; i++) if (ids[j] == l[i].th32ProcessID) f = 1; if (!f) diff++; }
        if (diff < best) best = diff;
    }
    CHECKF(best <= 2, "pid sets differ by %d", best);
}
TEST(ps_enumprocesses_sees_child) {
    REQUIRE(U(K32EnumProcesses));
    PROCESS_INFORMATION pi;
    if (!thp_spawn(&pi, CREATE_SUSPENDED)) { CHECK(0); return; }
    DWORD ids[4096], need = 0, cpid = pi.dwProcessId;
    CHECK(U(K32EnumProcesses)(ids, sizeof ids, &need));
    int f = 0; for (DWORD i = 0; i < need / 4; i++) if (ids[i] == cpid) f++;
    EXPECT_EQ(f, 1);
    thp_reap(&pi);
    int gone = 0;
    for (int k = 0; k < 100 && !gone; k++) {
        CHECK(U(K32EnumProcesses)(ids, sizeof ids, &need));
        gone = 1; for (DWORD i = 0; i < need / 4; i++) if (ids[i] == cpid) gone = 0;
        if (!gone) Sleep(50);
    }
    CHECK(gone);
}
TEST(ps_enumprocesses_ntqsi_agree) {
    REQUIRE(U(K32EnumProcesses)); REQUIRE(UNT(NtQuerySystemInformation));
    ULONG len = 1 << 20; BYTE *buf = NULL; NTSTATUS st;
    for (;;) {
        buf = malloc(len);
        ULONG ret = 0;
        st = UNT(NtQuerySystemInformation)(5, buf, len, &ret);
        if (st == (NTSTATUS)0xC0000004) { free(buf); len = ret + 65536; continue; }
        break;
    }
    CHECK(st >= 0);
    if (st < 0) { free(buf); return; }
    int cnt = 0, self = 0; ULONG off = 0;
    for (;;) {
        cnt++;
        if (*(ULONG_PTR *)(buf + off + 0x50) == GetCurrentProcessId()) self++;
        ULONG nx = *(ULONG *)(buf + off);
        if (!nx) break;
        off += nx;
    }
    free(buf);
    EXPECT_EQ(self, 1);
    DWORD ids[4096], need = 0;
    CHECK(U(K32EnumProcesses)(ids, sizeof ids, &need));
    CHECKF(abs(cnt - (int)(need / 4)) <= 3, "ntqsi %d vs enum %lu", cnt, need / 4);
}

/* ------- K32EnumProcessModules(Ex) ------- */
static HANDLE open_self(DWORD acc) { return OpenProcess(acc, FALSE, GetCurrentProcessId()); }

TEST(ps_enummodules_self_basic) {
    REQUIRE(U(K32EnumProcessModules));
    HMODULE m[512]; DWORD need = 0xCCCCCCCC; SET_ERR(0);
    CHECK(U(K32EnumProcessModules)(GetCurrentProcess(), m, sizeof m, &need));
    CHECK(need >= 4 * sizeof(HMODULE) && need % sizeof(HMODULE) == 0);
    EXPECT_EQ(m[0], GetModuleHandleW(NULL));
    int k = 0, nt = 0, kb = 0;
    for (DWORD i = 0; i < need / sizeof(HMODULE); i++) {
        if (m[i] == GetModuleHandleW(L"kernel32.dll")) k++;
        if (m[i] == GetModuleHandleW(L"ntdll.dll")) nt++;
        if (m[i] == GetModuleHandleW(L"kernelbase.dll")) kb++;
    }
    EXPECT_EQ(k, 1); EXPECT_EQ(nt, 1); EXPECT_EQ(kb, 1);
}
TEST(ps_enummodules_real_handle_and_pseudo_handles) {
    REQUIRE(U(K32EnumProcessModules));
    HMODULE a[512], b[512], c[512]; DWORD na = 0, nb = 0, nc = 0;
    HANDLE h = open_self(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ);
    CHECK(h != NULL);
    CHECK(U(K32EnumProcessModules)(GetCurrentProcess(), a, sizeof a, &na));
    CHECK(U(K32EnumProcessModules)(h, b, sizeof b, &nb));
    SET_ERR(0);
    CHECK(U(K32EnumProcessModules)(INVALID_HANDLE_VALUE, c, sizeof c, &nc));   /* -1 == current process pseudo handle */
    EXPECT_EQ(na, nb); EXPECT_EQ(na, nc);
    CHECK(!memcmp(a, b, na));
    CloseHandle(h);
}
TEST(ps_enummodules_needed_semantics) {
    REQUIRE(U(K32EnumProcessModules));
    HMODULE all[512]; DWORD full = 0;
    CHECK(U(K32EnumProcessModules)(GetCurrentProcess(), all, sizeof all, &full));
    HMODULE one[2]; one[0] = one[1] = (HMODULE)0x1111; DWORD need = 0; SET_ERR(0);
    BOOL r = U(K32EnumProcessModules)(GetCurrentProcess(), one, sizeof(HMODULE), &need);
    t_note("cb=8: ret=%d need=%lu (full=%lu) err=%lu", r, need, full, GetLastError());
    if (r) { EXPECT_EQ(need, full); EXPECT_EQ(one[0], all[0]); }
    EXPECT_EQ(one[1], 0x1111);
    need = 0xCC; r = U(K32EnumProcessModules)(GetCurrentProcess(), NULL, 0, &need);
    CHECK(r); EXPECT_EQ(need, full);
    one[0] = (HMODULE)0x1111;
    need = 0xCC; r = U(K32EnumProcessModules)(GetCurrentProcess(), one, 0, &need);
    CHECK(r); EXPECT_EQ(need, full);
    EXPECT_EQ(one[0], 0x1111);
    need = 0xCC; r = U(K32EnumProcessModules)(GetCurrentProcess(), one, 7, &need);
    t_note("cb=7: ret=%d need=%lu", r, need);
}
TEST(ps_enummodules_stable_across_calls) {
    REQUIRE(U(K32EnumProcessModules));
    HMODULE a[512], b[512]; DWORD na, nb;
    CHECK(U(K32EnumProcessModules)(GetCurrentProcess(), a, sizeof a, &na));
    CHECK(U(K32EnumProcessModules)(GetCurrentProcess(), b, sizeof b, &nb));
    EXPECT_EQ(na, nb); CHECK(!memcmp(a, b, na));
}
TEST(ps_enummodules_loadlibrary_tracks) {
    REQUIRE(U(K32EnumProcessModules));
    if (GetModuleHandleW(L"winmm.dll")) { t_skip("winmm preloaded"); return; }
    HMODULE a[512], b[512]; DWORD na, nb;
    CHECK(U(K32EnumProcessModules)(GetCurrentProcess(), a, sizeof a, &na));
    HMODULE h = LoadLibraryW(L"winmm.dll");
    if (!h) { t_skip("no winmm"); return; }
    CHECK(U(K32EnumProcessModules)(GetCurrentProcess(), b, sizeof b, &nb));
    CHECK(nb > na);
    int f = 0; for (DWORD i = 0; i < nb / 8; i++) if (b[i] == h) f++;
    EXPECT_EQ(f, 1);
    FreeLibrary(h);
    CHECK(U(K32EnumProcessModules)(GetCurrentProcess(), b, sizeof b, &nb));
    EXPECT_EQ(nb, na);
}
TEST(ps_enummodules_access_rights) {
    REQUIRE(U(K32EnumProcessModules));
    HMODULE m[512]; DWORD need;
    struct { DWORD acc; const char *n; } t[] = {
        { PROCESS_QUERY_LIMITED_INFORMATION, "limited" }, { PROCESS_QUERY_INFORMATION, "query only" },
        { PROCESS_VM_READ, "vm_read only" }, { SYNCHRONIZE, "synchronize" } };
    for (int i = 0; i < 4; i++) {
        HANDLE h = open_self(t[i].acc);
        if (!h) { CHECKF(0, "OpenProcess %s", t[i].n); continue; }
        SET_ERR(0); need = 0xCC;
        BOOL r = U(K32EnumProcessModules)(h, m, sizeof m, &need);
        CHECKF(!r, "%s should fail", t[i].n);
        if (!r) EXPECT_ERR(ERROR_ACCESS_DENIED);
        CloseHandle(h);
    }
}
TEST(ps_enummodules_invalid_handles) {
    REQUIRE(U(K32EnumProcessModules));
    HMODULE m[8]; DWORD need = 0;
    SET_ERR(0);
    CHECK(!U(K32EnumProcessModules)(NULL, m, sizeof m, &need));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE ev = CreateEventW(NULL, 0, 0, NULL);
    SET_ERR(0);
    CHECK(!U(K32EnumProcessModules)(ev, m, sizeof m, &need));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
    HANDLE h = open_self(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ);
    CloseHandle(h);
    SET_ERR(0);
    CHECK(!U(K32EnumProcessModules)(h, m, sizeof m, &need));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(ps_enummodules_suspended_child) {
    REQUIRE(U(K32EnumProcessModules));
    PROCESS_INFORMATION pi;
    if (!thp_spawn(&pi, CREATE_SUSPENDED)) { CHECK(0); return; }
    HMODULE m[64]; DWORD need = 0xCC; SET_ERR(0);
    BOOL r = U(K32EnumProcessModules)(pi.hProcess, m, sizeof m, &need);
    DWORD e = GetLastError();
    t_note("suspended child: ret=%d need=%lu err=%lu", r, need, e);
    if (!r) CHECK(e == ERROR_PARTIAL_COPY);
    thp_reap(&pi);
}
TEST(ps_enummodules_running_child) {
    REQUIRE(U(K32EnumProcessModules));
    PROCESS_INFORMATION pi;
    if (!thp_spawn(&pi, 0)) { CHECK(0); return; }
    BYTE *k = thp_wait_module(pi.dwProcessId, L"kernel32.dll", 10000);
    HMODULE m[256]; DWORD need = 0; SET_ERR(0);
    CHECKF(U(K32EnumProcessModules)(pi.hProcess, m, sizeof m, &need), "err=%lu", GetLastError());
    CHECK(need >= 4 * sizeof(HMODULE));
    char ex[MAX_PATH];
    CHECK(U(K32GetModuleFileNameExA)(pi.hProcess, m[0], ex, MAX_PATH) > 0);
    CHECK(!lstrcmpiA(ex, t_self_path()));
    int f = 0; for (DWORD i = 0; i < need / 8; i++) if ((BYTE *)m[i] == k) f++;
    EXPECT_EQ(f, 1);
    thp_reap(&pi);
}
TEST(ps_enummodules_terminated_child) {
    REQUIRE(U(K32EnumProcessModules));
    PROCESS_INFORMATION pi;
    if (!thp_spawn(&pi, 0)) { CHECK(0); return; }
    thp_wait_module(pi.dwProcessId, L"kernel32.dll", 10000);
    TerminateProcess(pi.hProcess, 9); WaitForSingleObject(pi.hProcess, 10000);
    HMODULE m[64]; DWORD need = 0; SET_ERR(0);
    BOOL r = U(K32EnumProcessModules)(pi.hProcess, m, sizeof m, &need);
    CHECK(!r);
    t_note("terminated child: err=%lu", GetLastError());
    thp_reap(&pi);
}

TEST(ps_enummodulesex_filters_default_equals_64) {
    REQUIRE(U(K32EnumProcessModulesEx));
    HMODULE a[512], b[512], c[512]; DWORD na = 0, nb = 0, nc = 0, nb3 = 0;
    CHECK(U(K32EnumProcessModulesEx)(GetCurrentProcess(), a, sizeof a, &na, LIST_MODULES_DEFAULT));
    CHECK(U(K32EnumProcessModulesEx)(GetCurrentProcess(), b, sizeof b, &nb, LIST_MODULES_64BIT));
    EXPECT_EQ(na, nb); CHECK(!memcmp(a, b, na));
    CHECK(U(K32EnumProcessModules)(GetCurrentProcess(), c, sizeof c, &nc));
    EXPECT_EQ(na, nc);
    SET_ERR(0);
    BOOL r = U(K32EnumProcessModulesEx)(GetCurrentProcess(), c, sizeof c, &nb3, LIST_MODULES_ALL);
    t_note("LIST_MODULES_ALL on native process: ret=%d need=%lu err=%lu", r, nb3, GetLastError());
    SET_ERR(0);
    r = U(K32EnumProcessModulesEx)(GetCurrentProcess(), c, sizeof c, &nb3, LIST_MODULES_32BIT);
    t_note("LIST_MODULES_32BIT on native process: ret=%d need=%lu err=%lu", r, nb3, GetLastError());
}
TEST(ps_enummodulesex_invalid_filter) {
    REQUIRE(U(K32EnumProcessModulesEx));
    HMODULE m[8]; DWORD need = 0xCC;
    static const DWORD bad[] = { 4, 8, 0x10, 0x80, 0x100, 0xFFFFFFFC, 0x80000000u };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SET_ERR(0);
        CHECKF(!U(K32EnumProcessModulesEx)(GetCurrentProcess(), m, sizeof m, &need, bad[i]), "filter 0x%lx", bad[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(ps_enummodulesex_extra_bits_with_valid) {
    REQUIRE(U(K32EnumProcessModulesEx));
    HMODULE a[512], b[512]; DWORD na = 0, nb = 0; SET_ERR(0);
    CHECK(U(K32EnumProcessModulesEx)(GetCurrentProcess(), a, sizeof a, &na, LIST_MODULES_64BIT));
    BOOL r = U(K32EnumProcessModulesEx)(GetCurrentProcess(), b, sizeof b, &nb, LIST_MODULES_64BIT | 0x100);
    t_note("filter 2|0x100: ret=%d", r);
    if (r) EXPECT_EQ(na, nb);
}
TEST(ps_enummodulesex_32bit_flag_on_child_native) {
    REQUIRE(U(K32EnumProcessModulesEx));
    PROCESS_INFORMATION pi;
    if (!thp_spawn(&pi, 0)) { CHECK(0); return; }
    thp_wait_module(pi.dwProcessId, L"kernel32.dll", 10000);
    HMODULE m[256]; DWORD need = 0xCC; SET_ERR(0);
    BOOL r = U(K32EnumProcessModulesEx)(pi.hProcess, m, sizeof m, &need, LIST_MODULES_32BIT);
    t_note("32BIT filter on native child: ret=%d need=%lu err=%lu", r, need, GetLastError());
    r = U(K32EnumProcessModulesEx)(pi.hProcess, m, sizeof m, &need, LIST_MODULES_64BIT);
    CHECK(r); CHECK(need >= 32);
    thp_reap(&pi);
}

/* ------- names ------- */
TEST(ps_modulebasename_exe) {
    REQUIRE(U(K32GetModuleBaseNameW)); REQUIRE(U(K32GetModuleBaseNameA));
    WCHAR full[MAX_PATH], w[MAX_PATH]; char a[MAX_PATH];
    GetModuleFileNameW(NULL, full, MAX_PATH);
    SET_ERR(0);
    DWORD n = U(K32GetModuleBaseNameW)(GetCurrentProcess(), NULL, w, MAX_PATH);
    t_note("GetModuleBaseNameW(NULL module) ret=%lu err=%lu", n, GetLastError());
    n = U(K32GetModuleBaseNameW)(GetCurrentProcess(), GetModuleHandleW(NULL), w, MAX_PATH);
    EXPECT_EQ(n, wcslen(thp_basew(full)));
    CHECK(!lstrcmpiW(w, thp_basew(full)));
    DWORD na = U(K32GetModuleBaseNameA)(GetCurrentProcess(), GetModuleHandleW(NULL), a, MAX_PATH);
    EXPECT_EQ(na, n);
    char fa[MAX_PATH]; thp_w2a(thp_basew(full), fa, MAX_PATH);
    CHECK(!lstrcmpiA(a, fa));
}
TEST(ps_modulebasename_system_dlls) {
    REQUIRE(U(K32GetModuleBaseNameW));
    static const WCHAR *n[] = { L"kernel32.dll", L"ntdll.dll", L"kernelbase.dll" };
    for (int i = 0; i < 3; i++) {
        WCHAR w[MAX_PATH]; char a[MAX_PATH];
        DWORD r = U(K32GetModuleBaseNameW)(GetCurrentProcess(), GetModuleHandleW(n[i]), w, MAX_PATH);
        EXPECT_EQ(r, wcslen(n[i]));
        CHECK(!lstrcmpiW(w, n[i]));
        DWORD ra = U(K32GetModuleBaseNameA)(GetCurrentProcess(), GetModuleHandleW(n[i]), a, MAX_PATH);
        EXPECT_EQ(ra, r);
    }
}
TEST(ps_modulebasename_truncation_w) {
    REQUIRE(U(K32GetModuleBaseNameW));
    HMODULE k = GetModuleHandleW(L"kernel32.dll");
    WCHAR w[32];
    SET_ERR(0);
    CHECK(U(K32GetModuleBaseNameW)(GetCurrentProcess(), k, w, 0) == 0);
    for (DWORD ns = 1; ns <= 12; ns++) {
        for (int i = 0; i < 32; i++) w[i] = 0x7777;
        SET_ERR(0);
        DWORD r = U(K32GetModuleBaseNameW)(GetCurrentProcess(), k, w, ns);
        if (ns <= 12) {
            CHECKF(r == ns || r == ns - 1, "ns=%lu r=%lu", ns, r);
            CHECKF(w[ns - 1] == 0 || (r == ns - 1 && w[r] == 0), "ns=%lu not terminated", ns);
            CHECKF(w[ns] == 0x7777, "ns=%lu wrote past buffer", ns);
        }
    }
    for (int i = 0; i < 32; i++) w[i] = 0x7777;
    DWORD r = U(K32GetModuleBaseNameW)(GetCurrentProcess(), k, w, 13);   /* exact: 12 chars + NUL */
    EXPECT_EQ(r, 12); EXPECT_EQ(w[12], 0);
    for (int i = 0; i < 32; i++) w[i] = 0x7777;
    r = U(K32GetModuleBaseNameW)(GetCurrentProcess(), k, w, 12);        /* exact - 1 */
    CHECKF(r == 11 || r == 12, "r=%lu", r);
    CHECK(w[11] == 0);
}
TEST(ps_modulebasename_truncation_a) {
    REQUIRE(U(K32GetModuleBaseNameA));
    HMODULE k = GetModuleHandleW(L"kernel32.dll");
    char a[32];
    SET_ERR(0);
    CHECK(U(K32GetModuleBaseNameA)(GetCurrentProcess(), k, a, 0) == 0);
    for (DWORD ns = 1; ns <= 12; ns++) {
        memset(a, 0x77, sizeof a);
        DWORD r = U(K32GetModuleBaseNameA)(GetCurrentProcess(), k, a, ns);
        t_note("A trunc ns=%lu r=%lu", ns, r);
        CHECKF(r <= ns, "ns=%lu r=%lu", ns, r);
        CHECKF(a[ns] == 0x77, "ns=%lu wrote past buffer", ns);
    }
    memset(a, 0x77, sizeof a);
    DWORD r = U(K32GetModuleBaseNameA)(GetCurrentProcess(), k, a, 13);
    EXPECT_EQ(r, 12); EXPECT_EQ(a[12], 0);
}
TEST(ps_modulebasename_bad_module) {
    REQUIRE(U(K32GetModuleBaseNameW));
    WCHAR w[MAX_PATH]; w[0] = 0x55;
    HMODULE k = GetModuleHandleW(L"kernel32.dll");
    static const ULONG_PTR bad[] = { 1, 0x1000, 0x10000, 0xFFFFFFFF };
    for (int i = 0; i < 4; i++) {
        SET_ERR(0);
        CHECKF(U(K32GetModuleBaseNameW)(GetCurrentProcess(), (HMODULE)bad[i], w, MAX_PATH) == 0, "module %p", (void *)bad[i]);
        t_note("bad module %p err=%lu", (void *)bad[i], GetLastError());
    }
    SET_ERR(0);
    CHECK(U(K32GetModuleBaseNameW)(GetCurrentProcess(), (HMODULE)((BYTE *)k + 0x1000), w, MAX_PATH) == 0);  /* not a base */
    SET_ERR(0);
    CHECK(U(K32GetModuleBaseNameW)(NULL, k, w, MAX_PATH) == 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(ps_modulebasename_child) {
    REQUIRE(U(K32GetModuleBaseNameW)); REQUIRE(U(K32GetModuleBaseNameA));
    PROCESS_INFORMATION pi;
    if (!thp_spawn(&pi, 0)) { CHECK(0); return; }
    BYTE *k = thp_wait_module(pi.dwProcessId, L"kernel32.dll", 10000);
    WCHAR w[MAX_PATH]; WCHAR me[MAX_PATH]; GetModuleFileNameW(NULL, me, MAX_PATH);
    HMODULE hm[8]; DWORD need;
    CHECK(U(K32EnumProcessModules)(pi.hProcess, hm, sizeof hm, &need));
    DWORD r = U(K32GetModuleBaseNameW)(pi.hProcess, hm[0], w, MAX_PATH);
    EXPECT_EQ(r, wcslen(thp_basew(me)));
    CHECK(!lstrcmpiW(w, thp_basew(me)));
    r = U(K32GetModuleBaseNameW)(pi.hProcess, (HMODULE)k, w, MAX_PATH);
    CHECK(!lstrcmpiW(w, L"kernel32.dll"));
    HANDLE lim = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pi.dwProcessId);
    SET_ERR(0);
    CHECK(U(K32GetModuleBaseNameW)(lim, (HMODULE)k, w, MAX_PATH) == 0);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(lim);
    thp_reap(&pi);
}

TEST(ps_modulefilenameex_basic) {
    REQUIRE(U(K32GetModuleFileNameExW)); REQUIRE(U(K32GetModuleFileNameExA));
    WCHAR a[MAX_PATH], b[MAX_PATH]; char c[MAX_PATH], d[MAX_PATH];
    GetModuleFileNameW(NULL, a, MAX_PATH);
    DWORD n = U(K32GetModuleFileNameExW)(GetCurrentProcess(), NULL, b, MAX_PATH);
    EXPECT_EQ(n, wcslen(a)); CHECK(!lstrcmpiW(a, b));
    n = U(K32GetModuleFileNameExW)(GetCurrentProcess(), GetModuleHandleW(NULL), b, MAX_PATH);
    EXPECT_EQ(n, wcslen(a)); CHECK(!lstrcmpiW(a, b));
    DWORD na = U(K32GetModuleFileNameExA)(GetCurrentProcess(), NULL, c, MAX_PATH);
    EXPECT_EQ(na, n);
    GetModuleFileNameA(NULL, d, MAX_PATH);
    CHECK(!lstrcmpiA(c, d));
    static const WCHAR *mn[] = { L"kernel32.dll", L"ntdll.dll", L"kernelbase.dll" };
    for (int i = 0; i < 3; i++) {
        HMODULE h = GetModuleHandleW(mn[i]);
        GetModuleFileNameW(h, a, MAX_PATH);
        n = U(K32GetModuleFileNameExW)(GetCurrentProcess(), h, b, MAX_PATH);
        EXPECT_EQ(n, wcslen(a)); CHECK(!lstrcmpiW(a, b));
    }
}
TEST(ps_modulefilenameex_nsize_zero) {
    REQUIRE(U(K32GetModuleFileNameExW)); REQUIRE(U(K32GetModuleFileNameExA));
    WCHAR w[4]; char a[4];
    SET_ERR(0);
    CHECK(U(K32GetModuleFileNameExW)(GetCurrentProcess(), NULL, w, 0) == 0);
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    SET_ERR(0);
    CHECK(U(K32GetModuleFileNameExA)(GetCurrentProcess(), NULL, a, 0) == 0);
    t_note("A nSize=0 err=%lu", GetLastError());
}
TEST(ps_modulefilenameex_truncation_null_module) {
    REQUIRE(U(K32GetModuleFileNameExW));
    WCHAR full[MAX_PATH]; GetModuleFileNameW(NULL, full, MAX_PATH);
    DWORD len = wcslen(full);
    WCHAR w[MAX_PATH];
    DWORD sizes[] = { 1, 2, 5, len - 1, len, len + 1 };
    for (int i = 0; i < 6; i++) {
        DWORD ns = sizes[i];
        for (int j = 0; j < MAX_PATH; j++) w[j] = 0x7777;
        SET_ERR(0);
        DWORD r = U(K32GetModuleFileNameExW)(GetCurrentProcess(), NULL, w, ns);
        DWORD want = ns > len ? len : ns - 1;
        CHECKF(r == want, "ns=%lu r=%lu want=%lu", ns, r, want);
        CHECKF(w[r] == 0, "ns=%lu not terminated", ns);
        CHECKF(!wcsncmp(w, full, r), "ns=%lu content", ns);
        CHECKF(w[ns] == 0x7777 || ns > len, "ns=%lu overrun", ns);
    }
}
TEST(ps_modulefilenameex_truncation_module) {
    REQUIRE(U(K32GetModuleFileNameExW));
    HMODULE k = GetModuleHandleW(L"kernel32.dll");
    WCHAR full[MAX_PATH]; GetModuleFileNameW(k, full, MAX_PATH);
    DWORD len = wcslen(full); WCHAR w[MAX_PATH];
    for (DWORD ns = 1; ns <= 12; ns++) {
        for (int j = 0; j < 40; j++) w[j] = 0x7777;
        DWORD r = U(K32GetModuleFileNameExW)(GetCurrentProcess(), k, w, ns);
        CHECKF(r == ns || r == ns - 1, "ns=%lu r=%lu", ns, r);
        CHECKF(w[ns - 1] == 0 || w[r] == 0, "ns=%lu unterminated", ns);
        CHECKF(w[ns] == 0x7777, "ns=%lu overrun", ns);
    }
    for (int j = 0; j < MAX_PATH; j++) w[j] = 0x7777;
    DWORD r = U(K32GetModuleFileNameExW)(GetCurrentProcess(), k, w, len + 1);
    EXPECT_EQ(r, len); EXPECT_EQ(w[len], 0);
}
TEST(ps_modulefilenameex_bad_module_and_handle) {
    REQUIRE(U(K32GetModuleFileNameExW));
    WCHAR w[MAX_PATH]; HMODULE k = GetModuleHandleW(L"kernel32.dll");
    SET_ERR(0);
    CHECK(U(K32GetModuleFileNameExW)(GetCurrentProcess(), (HMODULE)0x1000, w, MAX_PATH) == 0);
    t_note("bad module err=%lu", GetLastError());
    SET_ERR(0);
    CHECK(U(K32GetModuleFileNameExW)(NULL, k, w, MAX_PATH) == 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(U(K32GetModuleFileNameExW)(NULL, NULL, w, MAX_PATH) == 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(ps_modulefilenameex_null_module_limited_handle) {
    REQUIRE(U(K32GetModuleFileNameExW));
    /* hModule NULL goes through a query-only path, so limited access suffices */
    HANDLE h = open_self(PROCESS_QUERY_LIMITED_INFORMATION);
    WCHAR w[MAX_PATH], full[MAX_PATH]; GetModuleFileNameW(NULL, full, MAX_PATH);
    SET_ERR(0);
    DWORD r = U(K32GetModuleFileNameExW)(h, NULL, w, MAX_PATH);
    EXPECT_EQ(r, wcslen(full));
    CHECK(!lstrcmpiW(w, full));
    SET_ERR(0);
    r = U(K32GetModuleFileNameExW)(h, GetModuleHandleW(NULL), w, MAX_PATH);
    CHECK(r == 0);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(h);
}
TEST(ps_modulefilenameex_large_nsize_clamped) {
    REQUIRE(U(K32GetModuleFileNameExW));
    WCHAR *w = malloc(40000 * 2); WCHAR full[MAX_PATH]; GetModuleFileNameW(NULL, full, MAX_PATH);
    DWORD r = U(K32GetModuleFileNameExW)(GetCurrentProcess(), NULL, w, 0x7FFFFFFF);
    EXPECT_EQ(r, wcslen(full));
    free(w);
}

/* ------- K32GetModuleInformation ------- */
TEST(ps_moduleinfo_matches_pe_headers) {
    REQUIRE(U(K32GetModuleInformation));
    static const WCHAR *n[] = { NULL, L"kernel32.dll", L"kernelbase.dll", L"ntdll.dll" };
    for (int i = 0; i < 4; i++) {
        HMODULE h = GetModuleHandleW(n[i]); MODULEINFO mi; memset(&mi, 0xCC, sizeof mi);
        SET_ERR(0);
        CHECKF(U(K32GetModuleInformation)(GetCurrentProcess(), h, &mi, sizeof mi), "i=%d err=%lu", i, GetLastError());
        DWORD sz, ep; thp_chk_pe(h, &sz, &ep);
        EXPECT_EQ(mi.lpBaseOfDll, h);
        EXPECT_EQ(mi.SizeOfImage, sz);
        if (i < 3) EXPECT_EQ(mi.EntryPoint, ep ? (BYTE *)h + ep : NULL);
        else t_note("ntdll entry rva=0x%lx EntryPoint=%p base=%p", ep, mi.EntryPoint, h);
    }
}
TEST(ps_moduleinfo_cb_semantics) {
    REQUIRE(U(K32GetModuleInformation));
    HMODULE h = GetModuleHandleW(L"kernel32.dll");
    EXPECT_EQ(sizeof(MODULEINFO), 24);
    MODULEINFO mi;
    for (DWORD cb = 0; cb < 24; cb += 5) {
        SET_ERR(0);
        CHECKF(!U(K32GetModuleInformation)(GetCurrentProcess(), h, &mi, cb), "cb=%lu", cb);
        EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    }
    SET_ERR(0);
    CHECK(!U(K32GetModuleInformation)(GetCurrentProcess(), h, &mi, 23));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    struct { MODULEINFO m; BYTE guard[16]; } big; memset(&big, 0xAB, sizeof big);
    CHECK(U(K32GetModuleInformation)(GetCurrentProcess(), h, &big.m, sizeof big));
    CHECK(big.guard[0] == 0xAB && big.guard[15] == 0xAB);
    CHECK(U(K32GetModuleInformation)(GetCurrentProcess(), h, &mi, 24));
}
TEST(ps_moduleinfo_bad_module_and_handle) {
    REQUIRE(U(K32GetModuleInformation));
    MODULEINFO mi; HMODULE k = GetModuleHandleW(L"kernel32.dll");
    SET_ERR(0);
    CHECK(!U(K32GetModuleInformation)(GetCurrentProcess(), (HMODULE)0x1000, &mi, sizeof mi));
    t_note("bad module err=%lu", GetLastError());
    SET_ERR(0);
    CHECK(!U(K32GetModuleInformation)(GetCurrentProcess(), (HMODULE)((BYTE *)k + 0x2000), &mi, sizeof mi));
    SET_ERR(0);
    CHECK(!U(K32GetModuleInformation)(NULL, k, &mi, sizeof mi));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(ps_moduleinfo_dll_loaded_dynamically) {
    REQUIRE(U(K32GetModuleInformation));
    if (GetModuleHandleW(L"winmm.dll")) { t_skip("preloaded"); return; }
    HMODULE h = LoadLibraryW(L"winmm.dll");
    if (!h) { t_skip("no winmm"); return; }
    MODULEINFO mi; DWORD sz, ep; thp_chk_pe(h, &sz, &ep);
    CHECK(U(K32GetModuleInformation)(GetCurrentProcess(), h, &mi, sizeof mi));
    EXPECT_EQ(mi.SizeOfImage, sz);
    EXPECT_EQ(mi.EntryPoint, (BYTE *)h + ep);
    FreeLibrary(h);
    SET_ERR(0);
    CHECK(!U(K32GetModuleInformation)(GetCurrentProcess(), h, &mi, sizeof mi));
}
TEST(ps_moduleinfo_child) {
    REQUIRE(U(K32GetModuleInformation)); REQUIRE(U(K32EnumProcessModules));
    PROCESS_INFORMATION pi;
    if (!thp_spawn(&pi, 0)) { CHECK(0); return; }
    thp_wait_module(pi.dwProcessId, L"kernel32.dll", 10000);
    HMODULE m[256]; DWORD need = 0;
    CHECK(U(K32EnumProcessModules)(pi.hProcess, m, sizeof m, &need));
    MODULEINFO mi, mine;
    CHECK(U(K32GetModuleInformation)(pi.hProcess, m[0], &mi, sizeof mi));
    CHECK(U(K32GetModuleInformation)(GetCurrentProcess(), GetModuleHandleW(NULL), &mine, sizeof mine));
    EXPECT_EQ(mi.SizeOfImage, mine.SizeOfImage);
    EXPECT_EQ((BYTE *)mi.EntryPoint - (BYTE *)mi.lpBaseOfDll, (BYTE *)mine.EntryPoint - (BYTE *)mine.lpBaseOfDll);
    thp_reap(&pi);
}
