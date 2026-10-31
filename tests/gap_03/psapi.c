/* gap_03: EnumProcesses / EnumProcessModules(Ex) / EnumPageFilesW */
#include "t.h"
#include <psapi.h>

CHILD(gap3_sleeper) {
    Sleep(25000);
    return 0;
}

/* ---------------- EnumProcesses ---------------- */
TEST(gap3_enumprocesses_basic) {
    REQUIRE(U(EnumProcesses));
    DWORD ids[4096]; DWORD needed = 0xFFFFFFFF;
    CHECK(U(EnumProcesses)(ids, sizeof ids, &needed));
    CHECK(needed % 4 == 0);
    CHECK(needed >= 8);
    CHECK(needed <= sizeof ids);
    int self = 0;
    for (DWORD i = 0; i < needed / 4; i++) if (ids[i] == GetCurrentProcessId()) self++;
    EXPECT_EQ(self, 1);
}
TEST(gap3_enumprocesses_contains_idle_and_system) {
    REQUIRE(U(EnumProcesses));
    static DWORD ids[8192]; DWORD needed = 0;
    CHECK(U(EnumProcesses)(ids, sizeof ids, &needed));
    int idle = 0, sys = 0;
    for (DWORD i = 0; i < needed / 4; i++) { if (ids[i] == 0) idle++; if (ids[i] == 4) sys++; }
    if (!t_is_wine()) { EXPECT_EQ(idle, 1); EXPECT_EQ(sys, 1); }
    else t_note("idle=%d system=%d", idle, sys);
}
TEST(gap3_enumprocesses_cb_zero) {
    REQUIRE(U(EnumProcesses));
    DWORD needed = 0x1234;
    SetLastError(0x99);
    CHECK(U(EnumProcesses)(NULL, 0, &needed));
    EXPECT_EQ(needed, 0);
}
TEST(gap3_enumprocesses_cb_small_values) {
    REQUIRE(U(EnumProcesses));
    /* cb < 4 stores nothing; cb of 4..7 stores exactly one pid; unaligned cb truncates down */
    DWORD ids[8]; DWORD needed;
    for (DWORD cb = 0; cb < 4; cb++) {
        memset(ids, 0xEE, sizeof ids); needed = 0x55;
        CHECK(U(EnumProcesses)(ids, cb, &needed));
        EXPECT_EQ(needed, 0);
        EXPECT_EQ(ids[0], 0xEEEEEEEE);
    }
    for (DWORD cb = 4; cb < 8; cb++) {
        memset(ids, 0xEE, sizeof ids); needed = 0x55;
        CHECK(U(EnumProcesses)(ids, cb, &needed));
        EXPECT_EQ(needed, 4);
        EXPECT_NE(ids[0], 0xEEEEEEEE);
        EXPECT_EQ(ids[1], 0xEEEEEEEE);
    }
    for (DWORD cb = 8; cb < 12; cb++) {
        memset(ids, 0xEE, sizeof ids); needed = 0x55;
        CHECK(U(EnumProcesses)(ids, cb, &needed));
        EXPECT_EQ(needed, 8);
        EXPECT_EQ(ids[2], 0xEEEEEEEE);
    }
}
TEST(gap3_enumprocesses_buffer_not_overrun) {
    REQUIRE(U(EnumProcesses));
    DWORD ids[16]; DWORD needed = 0;
    for (int i = 0; i < 16; i++) ids[i] = 0xA5A5A5A5;
    CHECK(U(EnumProcesses)(ids + 4, 8 * sizeof(DWORD), &needed));
    if (needed < 32) { t_skip("fewer than 8 processes"); return; }
    EXPECT_EQ(needed, 32);
    for (int i = 0; i < 4; i++) EXPECT_EQ(ids[i], 0xA5A5A5A5);
    for (int i = 12; i < 16; i++) EXPECT_EQ(ids[i], 0xA5A5A5A5);
}
TEST(gap3_enumprocesses_truncated_is_prefix_of_full) {
    REQUIRE(U(EnumProcesses));
    static DWORD full[8192]; DWORD part[3], nf = 0, np = 0;
    CHECK(U(EnumProcesses)(full, sizeof full, &nf));
    CHECK(U(EnumProcesses)(part, sizeof part, &np));
    EXPECT_EQ(np, 12);
    /* the first entries are stable across immediate calls (Idle, System, ...) */
    CHECK(part[0] == full[0]);
}
TEST(gap3_enumprocesses_sees_child) {
    REQUIRE(U(EnumProcesses));
    PROCESS_INFORMATION pi;
    HANDLE h = t_spawn_child("gap3_sleeper", "", 0, NULL, NULL, NULL, FALSE, NULL, &pi);
    CHECK(h != NULL);
    if (!h) return;
    static DWORD ids[8192]; DWORD needed = 0; int found = 0;
    CHECK(U(EnumProcesses)(ids, sizeof ids, &needed));
    for (DWORD i = 0; i < needed / 4; i++) if (ids[i] == pi.dwProcessId) found++;
    EXPECT_EQ(found, 1);
    TerminateProcess(h, 0); WaitForSingleObject(h, 5000);
    CloseHandle(pi.hThread); CloseHandle(h);
}
TEST(gap3_enumprocesses_repeatable_count_close) {
    REQUIRE(U(EnumProcesses));
    static DWORD ids[8192]; DWORD n0 = 0;
    CHECK(U(EnumProcesses)(ids, sizeof ids, &n0));
    for (int i = 0; i < 20; i++) {
        DWORD n = 0;
        CHECK(U(EnumProcesses)(ids, sizeof ids, &n));
        CHECKF((n > n0 ? n - n0 : n0 - n) <= 40 * 4, "count jumped %lu -> %lu", n0 / 4, n / 4);
    }
}
TEST(gap3_enumprocesses_no_duplicate_ids_except_idle) {
    REQUIRE(U(EnumProcesses));
    static DWORD ids[8192]; DWORD n = 0;
    CHECK(U(EnumProcesses)(ids, sizeof ids, &n));
    int dups = 0;
    for (DWORD i = 0; i < n / 4; i++)
        for (DWORD j = i + 1; j < n / 4; j++) if (ids[i] == ids[j]) dups++;
    EXPECT_EQ(dups, 0);
}

/* ---------------- EnumProcessModules ---------------- */
TEST(gap3_enummodules_self_basic) {
    REQUIRE(U(EnumProcessModules));
    HMODULE mods[512]; DWORD needed = 0;
    CHECK(U(EnumProcessModules)(GetCurrentProcess(), mods, sizeof mods, &needed));
    CHECK(needed % sizeof(HMODULE) == 0);
    CHECK(needed >= 3 * sizeof(HMODULE));
    CHECK(needed <= sizeof mods);
    CHECK(mods[0] == GetModuleHandleA(NULL));
}
TEST(gap3_enummodules_contains_system_dlls) {
    REQUIRE(U(EnumProcessModules));
    HMODULE mods[512]; DWORD needed = 0;
    CHECK(U(EnumProcessModules)(GetCurrentProcess(), mods, sizeof mods, &needed));
    HMODULE want[3] = { GetModuleHandleA("ntdll.dll"), GetModuleHandleA("kernel32.dll"), GetModuleHandleA("kernelbase.dll") };
    for (int w = 0; w < 3; w++) {
        int hit = 0;
        for (DWORD i = 0; i < needed / 8; i++) if (mods[i] == want[w]) hit++;
        CHECKF(hit == 1, "module %d hits %d", w, hit);
    }
}
TEST(gap3_enummodules_every_entry_resolves) {
    REQUIRE(U(EnumProcessModules));
    HMODULE mods[512]; DWORD needed = 0;
    CHECK(U(EnumProcessModules)(GetCurrentProcess(), mods, sizeof mods, &needed));
    for (DWORD i = 0; i < needed / 8; i++) {
        char name[MAX_PATH];
        CHECKF(mods[i] != NULL, "entry %lu null", i);
        CHECKF(GetModuleFileNameA(mods[i], name, sizeof name) != 0, "entry %lu no name", i);
    }
}
TEST(gap3_enummodules_pseudo_handle_minus_one) {
    REQUIRE(U(EnumProcessModules));
    HMODULE mods[512]; DWORD needed = 0, n2 = 0;
    CHECK(U(EnumProcessModules)((HANDLE)(LONG_PTR)-1, mods, sizeof mods, &needed));
    CHECK(U(EnumProcessModules)(GetCurrentProcess(), mods, sizeof mods, &n2));
    EXPECT_EQ(needed, n2);
}
TEST(gap3_enummodules_real_handle_to_self) {
    REQUIRE(U(EnumProcessModules));
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, GetCurrentProcessId());
    CHECK(h != NULL);
    if (!h) return;
    HMODULE a[512], b[512]; DWORD na = 0, nb = 0;
    CHECK(U(EnumProcessModules)(h, a, sizeof a, &na));
    CHECK(U(EnumProcessModules)(GetCurrentProcess(), b, sizeof b, &nb));
    EXPECT_EQ(na, nb);
    CHECK(memcmp(a, b, na) == 0);
    CloseHandle(h);
}
TEST(gap3_enummodules_size_query_cb_zero) {
    REQUIRE(U(EnumProcessModules));
    HMODULE mods[512]; DWORD full = 0, q = 0;
    CHECK(U(EnumProcessModules)(GetCurrentProcess(), mods, sizeof mods, &full));
    q = 0xFFFF;
    CHECK(U(EnumProcessModules)(GetCurrentProcess(), NULL, 0, &q));
    EXPECT_EQ(q, full);
    HMODULE one[1] = { (HMODULE)0x77 };
    q = 0;
    CHECK(U(EnumProcessModules)(GetCurrentProcess(), one, 0, &q));
    EXPECT_EQ(q, full);
    EXPECT_EQ(one[0], (HMODULE)0x77);
}
TEST(gap3_enummodules_partial_buffer) {
    REQUIRE(U(EnumProcessModules));
    HMODULE full[512]; DWORD nf = 0;
    CHECK(U(EnumProcessModules)(GetCurrentProcess(), full, sizeof full, &nf));
    for (DWORD cb = 1; cb <= 3 * 8; cb++) {
        HMODULE p[5]; DWORD np = 0;
        for (int i = 0; i < 5; i++) p[i] = (HMODULE)0x77;
        BOOL r = U(EnumProcessModules)(GetCurrentProcess(), p, cb, &np);
        CHECKF(r, "cb %lu", cb);
        CHECKF(np == nf, "cb %lu needed %lu vs %lu", cb, np, nf);
        DWORD slots = cb / 8;
        for (DWORD i = 0; i < 5; i++) {
            if (i < slots) CHECKF(p[i] == full[i], "cb %lu slot %lu", cb, i);
            else CHECKF(p[i] == (HMODULE)0x77, "cb %lu slot %lu clobbered", cb, i);
        }
    }
}
TEST(gap3_enummodules_load_free_changes_list) {
    REQUIRE(U(EnumProcessModules));
    if (GetModuleHandleA("version.dll")) { t_skip("version.dll already loaded"); return; }
    HMODULE m[512]; DWORD n0 = 0, n1 = 0, n2 = 0;
    CHECK(U(EnumProcessModules)(GetCurrentProcess(), m, sizeof m, &n0));
    HMODULE h = LoadLibraryA("version.dll");
    CHECK(h != NULL);
    if (!h) return;
    CHECK(U(EnumProcessModules)(GetCurrentProcess(), m, sizeof m, &n1));
    CHECK(n1 > n0);
    int hit = 0;
    for (DWORD i = 0; i < n1 / 8; i++) if (m[i] == h) hit++;
    EXPECT_EQ(hit, 1);
    FreeLibrary(h);
    CHECK(U(EnumProcessModules)(GetCurrentProcess(), m, sizeof m, &n2));
    CHECK(n2 < n1);
}
TEST(gap3_enummodules_null_handle) {
    REQUIRE(U(EnumProcessModules));
    HMODULE m[8]; DWORD n = 0x55;
    SetLastError(0);
    CHECK(U(EnumProcessModules)(NULL, m, sizeof m, &n) == FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap3_enummodules_wrong_handle_type) {
    REQUIRE(U(EnumProcessModules));
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    HMODULE m[8]; DWORD n = 0x55;
    SetLastError(0);
    BOOL r = U(EnumProcessModules)(ev, m, sizeof m, &n);
    DWORD e = GetLastError();
    CHECK(r == FALSE);
    CHECKF(e == ERROR_INVALID_HANDLE, "err %lu", e);
    CloseHandle(ev);
}
TEST(gap3_enummodules_closed_handle) {
    REQUIRE(U(EnumProcessModules));
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, GetCurrentProcessId());
    CHECK(h != NULL);
    CloseHandle(h);
    HMODULE m[8]; DWORD n = 0;
    SetLastError(0);
    CHECK(U(EnumProcessModules)(h, m, sizeof m, &n) == FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap3_enummodules_thread_handle) {
    REQUIRE(U(EnumProcessModules));
    HMODULE m[8]; DWORD n = 0;
    SetLastError(0);
    BOOL r = U(EnumProcessModules)(GetCurrentThread(), m, sizeof m, &n);
    CHECK(r == FALSE);
    t_note("thread pseudo-handle err=%lu", GetLastError());
}
TEST(gap3_enummodules_limited_access_handle) {
    REQUIRE(U(EnumProcessModules));
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    CHECK(h != NULL);
    if (!h) return;
    HMODULE m[64]; DWORD n = 0;
    SetLastError(0);
    BOOL r = U(EnumProcessModules)(h, m, sizeof m, &n);
    CHECK(r == FALSE);          /* needs PROCESS_VM_READ to walk the loader list */
    t_note("limited-access err=%lu", GetLastError());
    CloseHandle(h);
}
TEST(gap3_enummodules_no_vm_read_handle) {
    REQUIRE(U(EnumProcessModules));
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, GetCurrentProcessId());
    CHECK(h != NULL);
    if (!h) return;
    HMODULE m[64]; DWORD n = 0;
    BOOL r = U(EnumProcessModules)(h, m, sizeof m, &n);
    t_note("query-only handle ret=%d err=%lu", r, GetLastError());
    CloseHandle(h);
}
TEST(gap3_enummodules_other_process) {
    REQUIRE(U(EnumProcessModules));
    PROCESS_INFORMATION pi;
    HANDLE h = t_spawn_child("gap3_sleeper", "", 0, NULL, NULL, NULL, FALSE, NULL, &pi);
    CHECK(h != NULL);
    if (!h) return;
    HMODULE m[512]; DWORD n = 0; BOOL ok = FALSE;
    for (int i = 0; i < 100 && !ok; i++) {
        n = 0;
        ok = U(EnumProcessModules)(h, m, sizeof m, &n) && n >= 3 * 8;
        if (!ok) Sleep(50);
    }
    CHECK(ok);
    if (ok) {
        CHECK(n % 8 == 0);
        UDECL(DWORD, K32GetModuleFileNameExA, (HANDLE, HMODULE, LPSTR, DWORD));
        K32GetModuleFileNameExA_pfn gm = UN(K32GetModuleFileNameExA);
        if (gm) {
            char name[MAX_PATH] = "";
            CHECK(gm(h, m[0], name, sizeof name) != 0);
            const char *base = strrchr(t_self_path(), '\\');
            base = base ? base + 1 : t_self_path();
            CHECKF(strstr(name, base) != NULL, "first module %s", name);
        }
        /* the system DLLs map at the same base address across processes in a session */
        HMODULE nt = GetModuleHandleA("ntdll.dll"); int hit = 0;
        for (DWORD i = 0; i < n / 8; i++) if (m[i] == nt) hit++;
        EXPECT_EQ(hit, 1);
    }
    TerminateProcess(h, 0); WaitForSingleObject(h, 5000);
    CloseHandle(pi.hThread); CloseHandle(h);
}
TEST(gap3_enummodules_terminated_process_no_crash) {
    REQUIRE(U(EnumProcessModules));
    PROCESS_INFORMATION pi;
    HANDLE h = t_spawn_child("gap3_sleeper", "", 0, NULL, NULL, NULL, FALSE, NULL, &pi);
    CHECK(h != NULL);
    if (!h) return;
    Sleep(200);
    TerminateProcess(h, 0); WaitForSingleObject(h, 5000);
    CloseHandle(pi.hThread);
    HMODULE m[64]; DWORD n = 0;
    SetLastError(0);
    BOOL r = U(EnumProcessModules)(h, m, sizeof m, &n);
    t_note("terminated process (query handle): ret=%d err=%lu", r, GetLastError());
    CloseHandle(h);
}
TEST(gap3_enummodules_repeated_stable) {
    REQUIRE(U(EnumProcessModules));
    HMODULE a[512], b[512]; DWORD na = 0, nb = 0;
    CHECK(U(EnumProcessModules)(GetCurrentProcess(), a, sizeof a, &na));
    for (int i = 0; i < 200; i++) {
        CHECK(U(EnumProcessModules)(GetCurrentProcess(), b, sizeof b, &nb));
        if (nb != na || memcmp(a, b, na)) { CHECK(0); break; }
    }
}

/* ---------------- EnumProcessModulesEx ---------------- */
TEST(gap3_enummodulesex_default_equals_plain) {
    REQUIRE(U(EnumProcessModulesEx)); REQUIRE(U(EnumProcessModules));
    HMODULE a[512], b[512]; DWORD na = 0, nb = 0;
    CHECK(U(EnumProcessModules)(GetCurrentProcess(), a, sizeof a, &na));
    CHECK(U(EnumProcessModulesEx)(GetCurrentProcess(), b, sizeof b, &nb, LIST_MODULES_DEFAULT));
    EXPECT_EQ(na, nb);
    CHECK(memcmp(a, b, na) == 0);
}
TEST(gap3_enummodulesex_64bit_filter) {
    REQUIRE(U(EnumProcessModulesEx));
    HMODULE a[512], b[512]; DWORD na = 0, nb = 0;
    CHECK(U(EnumProcessModulesEx)(GetCurrentProcess(), a, sizeof a, &na, LIST_MODULES_DEFAULT));
    CHECK(U(EnumProcessModulesEx)(GetCurrentProcess(), b, sizeof b, &nb, LIST_MODULES_64BIT));
    EXPECT_EQ(na, nb);
    CHECK(memcmp(a, b, na) == 0);
}
TEST(gap3_enummodulesex_all_filter_native) {
    REQUIRE(U(EnumProcessModulesEx));
    HMODULE a[512], b[512]; DWORD na = 0, nb = 0;
    CHECK(U(EnumProcessModulesEx)(GetCurrentProcess(), a, sizeof a, &na, LIST_MODULES_DEFAULT));
    SetLastError(0);
    BOOL r = U(EnumProcessModulesEx)(GetCurrentProcess(), b, sizeof b, &nb, LIST_MODULES_ALL);
    if (r) { CHECK(nb >= na); t_note("LIST_MODULES_ALL: needed %lu vs default %lu", nb, na); }
    else t_note("LIST_MODULES_ALL failed err=%lu", GetLastError());
}
TEST(gap3_enummodulesex_32bit_filter_native_process) {
    REQUIRE(U(EnumProcessModulesEx));
    HMODULE b[512]; DWORD nb = 0xAA;
    SetLastError(0);
    BOOL r = U(EnumProcessModulesEx)(GetCurrentProcess(), b, sizeof b, &nb, LIST_MODULES_32BIT);
    t_note("LIST_MODULES_32BIT in 64-bit process: ret=%d needed=%lu err=%lu", r, nb, GetLastError());
}
TEST(gap3_enummodulesex_invalid_filters) {
    REQUIRE(U(EnumProcessModulesEx));
    static const DWORD bad[] = { 4, 8, 0x10, 0x80, 0x100, 0x80000000u, 0xFFFFFFFCu };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        HMODULE m[64]; DWORD n = 0x55;
        SetLastError(0);
        BOOL r = U(EnumProcessModulesEx)(GetCurrentProcess(), m, sizeof m, &n, bad[i]);
        CHECKF(r == FALSE, "filter 0x%lx", bad[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        EXPECT_EQ(n, 0x55);
    }
}
TEST(gap3_enummodulesex_invalid_filter_checked_before_handle) {
    REQUIRE(U(EnumProcessModulesEx));
    HMODULE m[8]; DWORD n = 0;
    SetLastError(0);
    CHECK(U(EnumProcessModulesEx)(NULL, m, sizeof m, &n, 4) == FALSE);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap3_enummodulesex_null_handle_default) {
    REQUIRE(U(EnumProcessModulesEx));
    HMODULE m[8]; DWORD n = 0;
    SetLastError(0);
    CHECK(U(EnumProcessModulesEx)(NULL, m, sizeof m, &n, LIST_MODULES_DEFAULT) == FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap3_enummodulesex_null_handle_64bit) {
    REQUIRE(U(EnumProcessModulesEx));
    HMODULE m[8]; DWORD n = 0;
    SetLastError(0);
    CHECK(U(EnumProcessModulesEx)(NULL, m, sizeof m, &n, LIST_MODULES_64BIT) == FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap3_enummodulesex_size_query) {
    REQUIRE(U(EnumProcessModulesEx));
    HMODULE m[512]; DWORD full = 0, q = 0xFFFF;
    CHECK(U(EnumProcessModulesEx)(GetCurrentProcess(), m, sizeof m, &full, LIST_MODULES_64BIT));
    CHECK(U(EnumProcessModulesEx)(GetCurrentProcess(), NULL, 0, &q, LIST_MODULES_64BIT));
    EXPECT_EQ(q, full);
}
TEST(gap3_enummodulesex_partial_buffer) {
    REQUIRE(U(EnumProcessModulesEx));
    HMODULE full[512]; DWORD nf = 0;
    CHECK(U(EnumProcessModulesEx)(GetCurrentProcess(), full, sizeof full, &nf, LIST_MODULES_64BIT));
    HMODULE p[4] = { (HMODULE)7, (HMODULE)7, (HMODULE)7, (HMODULE)7 }; DWORD np = 0;
    CHECK(U(EnumProcessModulesEx)(GetCurrentProcess(), p, 16, &np, LIST_MODULES_64BIT));
    EXPECT_EQ(np, nf);
    CHECK(p[0] == full[0] && p[1] == full[1]);
    CHECK(p[2] == (HMODULE)7 && p[3] == (HMODULE)7);
}
TEST(gap3_enummodulesex_other_process_filters) {
    REQUIRE(U(EnumProcessModulesEx));
    PROCESS_INFORMATION pi;
    HANDLE h = t_spawn_child("gap3_sleeper", "", 0, NULL, NULL, NULL, FALSE, NULL, &pi);
    CHECK(h != NULL);
    if (!h) return;
    HMODULE m[512]; DWORD n = 0; BOOL ok = FALSE;
    for (int i = 0; i < 100 && !ok; i++) {
        n = 0;
        ok = U(EnumProcessModulesEx)(h, m, sizeof m, &n, LIST_MODULES_64BIT) && n >= 24;
        if (!ok) Sleep(50);
    }
    CHECK(ok);
    HMODULE m2[512]; DWORD n2 = 0;
    CHECK(U(EnumProcessModulesEx)(h, m2, sizeof m2, &n2, LIST_MODULES_DEFAULT));
    if (ok) { EXPECT_EQ(n, n2); }
    TerminateProcess(h, 0); WaitForSingleObject(h, 5000);
    CloseHandle(pi.hThread); CloseHandle(h);
}

/* ---------------- EnumPageFilesW ---------------- */
typedef struct { int count; int bad; int stop_after; } PFCTX;
static BOOL CALLBACK g3_pf_cb(LPVOID ctx, PENUM_PAGE_FILE_INFORMATION info, LPCWSTR name) {
    PFCTX *c = (PFCTX *)ctx;
    c->count++;
    if (!info || !name) c->bad++;
    else {
        if (*(DWORD *)info != sizeof(ENUM_PAGE_FILE_INFORMATION)) c->bad++;
        if (info->TotalInUse > info->TotalSize) c->bad++;
        if (info->PeakUsage < info->TotalInUse) c->bad++;
        if (!(name[0] && name[1] == L':')) c->bad++;
    }
    return c->stop_after && c->count >= c->stop_after ? FALSE : TRUE;
}
TEST(gap3_enumpagefiles_runs) {
    REQUIRE(U(EnumPageFilesW));
    if (t_is_wine()) { t_skip("wine: EnumPageFilesW unsupported"); return; }
    PFCTX c = {0};
    SetLastError(0);
    BOOL r = U(EnumPageFilesW)(g3_pf_cb, &c);
    CHECK(r == TRUE);
    CHECK(c.bad == 0);
    t_note("page files: %d", c.count);
}
TEST(gap3_enumpagefiles_repeatable) {
    REQUIRE(U(EnumPageFilesW));
    if (t_is_wine()) { t_skip("wine"); return; }
    PFCTX a = {0}, b = {0};
    CHECK(U(EnumPageFilesW)(g3_pf_cb, &a));
    for (int i = 0; i < 20; i++) { b.count = 0; CHECK(U(EnumPageFilesW)(g3_pf_cb, &b)); EXPECT_EQ(a.count, b.count); }
}
static BOOL CALLBACK g3_pf_count(LPVOID ctx, PENUM_PAGE_FILE_INFORMATION info, LPCWSTR name) { (*(int *)ctx)++; return TRUE; }
TEST(gap3_enumpagefiles_context_passthrough) {
    REQUIRE(U(EnumPageFilesW));
    if (t_is_wine()) { t_skip("wine"); return; }
    int n = 0;
    CHECK(U(EnumPageFilesW)(g3_pf_count, &n));
    PFCTX c = {0};
    CHECK(U(EnumPageFilesW)(g3_pf_cb, &c));
    EXPECT_EQ(n, c.count);
}
static BOOL CALLBACK g3_pf_ignore(LPVOID ctx, PENUM_PAGE_FILE_INFORMATION info, LPCWSTR name) { return TRUE; }
TEST(gap3_enumpagefiles_null_context) {
    REQUIRE(U(EnumPageFilesW));
    if (t_is_wine()) { t_skip("wine"); return; }
    CHECK(U(EnumPageFilesW)(g3_pf_ignore, NULL));
}
TEST(gap3_enumpagefiles_callback_stop) {
    REQUIRE(U(EnumPageFilesW));
    if (t_is_wine()) { t_skip("wine"); return; }
    /* kernelbase EnumPageFilesW ignores the callback's return value and keeps walking every entry */
    PFCTX all = {0};
    CHECK(U(EnumPageFilesW)(g3_pf_cb, &all));
    PFCTX c = {0}; c.stop_after = 1;
    SetLastError(0);
    BOOL r = U(EnumPageFilesW)(g3_pf_cb, &c);
    t_note("callback returning FALSE after first: ret=%d count=%d (all=%d)", r, c.count, all.count);
    CHECK(r == TRUE);
    CHECK(c.count == all.count);
}
