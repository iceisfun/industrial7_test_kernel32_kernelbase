/* Module32*, Heap32*, Toolhelp32ReadProcessMemory */
#include "thp.h"

#define NEED_MOD() REQUIRE(U(CreateToolhelp32Snapshot)); REQUIRE(U(Module32FirstW)); REQUIRE(U(Module32NextW))

static int mods_of(DWORD pid, MODULEENTRY32W *out, int cap, DWORD fl) {
    HANDLE s = U(CreateToolhelp32Snapshot)(fl, pid);
    if (s == INVALID_HANDLE_VALUE) return -1;
    int n = 0; MODULEENTRY32W m; m.dwSize = sizeof m;
    BOOL ok = U(Module32FirstW)(s, &m);
    while (ok && n < cap) { out[n++] = m; m.dwSize = sizeof m; ok = U(Module32NextW)(s, &m); }
    CloseHandle(s);
    return n;
}
static MODULEENTRY32W *find_mod(MODULEENTRY32W *l, int n, const WCHAR *name) {
    for (int i = 0; i < n; i++) if (!lstrcmpiW(l[i].szModule, name)) return &l[i];
    return NULL;
}

TEST(th_mod_self_first_is_exe) {
    NEED_MOD();
    HANDLE s = U(CreateToolhelp32Snapshot)(TH32CS_SNAPMODULE, 0);
    if (s == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    MODULEENTRY32W m; memset(&m, 0xCC, sizeof m); m.dwSize = sizeof m;
    SET_ERR(0);
    CHECK(U(Module32FirstW)(s, &m));
    EXPECT_EQ(m.dwSize, PSZ_MOD_W);
    EXPECT_EQ(m.modBaseAddr, GetModuleHandleW(NULL));
    EXPECT_EQ(m.hModule, GetModuleHandleW(NULL));
    EXPECT_EQ(m.th32ProcessID, GetCurrentProcessId());
    WCHAR me[MAX_PATH]; GetModuleFileNameW(NULL, me, MAX_PATH);
    CHECK(!lstrcmpiW(m.szExePath, me));
    CHECK(!lstrcmpiW(m.szModule, thp_basew(me)));
    DWORD sz, ep; thp_chk_pe(GetModuleHandleW(NULL), &sz, &ep);
    EXPECT_EQ(m.modBaseSize, sz);
    CHECK(m.GlblcntUsage != 0);
    CHECK(m.ProccntUsage != 0);
    CloseHandle(s);
}

TEST(th_mod_self_core_modules) {
    NEED_MOD();
    static MODULEENTRY32W l[512];
    int n = mods_of(0, l, 512, TH32CS_SNAPMODULE);
    CHECK(n >= 4);
    static const WCHAR *names[] = { L"ntdll.dll", L"kernel32.dll", L"KERNELBASE.dll" };
    for (int i = 0; i < 3; i++) {
        MODULEENTRY32W *m = find_mod(l, n, names[i]);
        CHECKF(m != NULL, "%ls missing", names[i]);
        if (!m) continue;
        HMODULE h = GetModuleHandleW(names[i]);
        EXPECT_EQ(m->modBaseAddr, h);
        EXPECT_EQ(m->hModule, h);
        DWORD sz, ep; thp_chk_pe(h, &sz, &ep);
        EXPECT_EQ(m->modBaseSize, sz);
        WCHAR p[MAX_PATH]; GetModuleFileNameW(h, p, MAX_PATH);
        CHECK(!lstrcmpiW(m->szExePath, p));
        CHECK(m->ProccntUsage != 0);
    }
}

TEST(th_mod_self_order_exe_then_ntdll) {
    NEED_MOD();
    static MODULEENTRY32W l[512];
    int n = mods_of(0, l, 512, TH32CS_SNAPMODULE);
    if (n < 2) { CHECK(0); return; }
    EXPECT_EQ(l[0].modBaseAddr, GetModuleHandleW(NULL));
    CHECK(!lstrcmpiW(l[1].szModule, L"ntdll.dll"));
}

TEST(th_mod_self_all_match_peb_list) {
    NEED_MOD(); REQUIRE(U(K32EnumProcessModules));
    static MODULEENTRY32W l[512]; HMODULE hm[512]; DWORD need = 0;
    for (int attempt = 0; attempt < 3; attempt++) {
        int n = mods_of(0, l, 512, TH32CS_SNAPMODULE);
        CHECK(U(K32EnumProcessModules)(GetCurrentProcess(), hm, sizeof hm, &need));
        int c = need / sizeof(HMODULE);
        if (c != n) { if (attempt == 2) CHECKF(0, "toolhelp %d vs EnumProcessModules %d", n, c); continue; }
        int bad = 0;
        for (int i = 0; i < n; i++) if (l[i].modBaseAddr != (BYTE *)hm[i]) bad++;
        CHECKF(bad == 0, "%d order/base mismatches", bad);
        return;
    }
}

TEST(th_mod_end_and_restart) {
    NEED_MOD();
    HANDLE s = U(CreateToolhelp32Snapshot)(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (s == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    MODULEENTRY32W m; int n1 = 0, n2 = 0;
    m.dwSize = sizeof m; BOOL ok = U(Module32FirstW)(s, &m);
    while (ok) { n1++; m.dwSize = sizeof m; ok = U(Module32NextW)(s, &m); }
    EXPECT_ERR(ERROR_NO_MORE_FILES);
    m.dwSize = sizeof m; SET_ERR(0);
    CHECK(!U(Module32NextW)(s, &m));
    EXPECT_ERR(ERROR_NO_MORE_FILES);
    m.dwSize = sizeof m; ok = U(Module32FirstW)(s, &m);
    while (ok) { n2++; m.dwSize = sizeof m; ok = U(Module32NextW)(s, &m); }
    EXPECT_EQ(n1, n2);
    CloseHandle(s);
}

TEST(th_mod_bad_dwsize) {
    NEED_MOD();
    HANDLE s = U(CreateToolhelp32Snapshot)(TH32CS_SNAPMODULE, 0);
    if (s == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    static const DWORD bad[] = { 0, 4, 0x224, 0x437, 0x439, 0x1000, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        MODULEENTRY32W m; memset(&m, 0xCC, sizeof m); m.dwSize = bad[i]; SET_ERR(0);
        CHECKF(!U(Module32FirstW)(s, &m), "dwSize 0x%lx", bad[i]);
        t_note("bad dwSize err=%lu", GetLastError());
        m.dwSize = bad[i]; SET_ERR(0);
        CHECKF(!U(Module32NextW)(s, &m), "next dwSize 0x%lx", bad[i]);
        t_note("bad dwSize err=%lu", GetLastError());
    }
    CloseHandle(s);
}

TEST(th_mod_empty_on_wrong_snapshot) {
    NEED_MOD();
    HANDLE s = U(CreateToolhelp32Snapshot)(TH32CS_SNAPHEAPLIST, 0);
    if (s == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    MODULEENTRY32W m; memset(&m, 0xCC, sizeof m); m.dwSize = sizeof m; SET_ERR(0);
    CHECK(!U(Module32FirstW)(s, &m));
    EXPECT_ERR(ERROR_NO_MORE_FILES);
    EXPECT_EQ(m.modBaseAddr, 0);
    CloseHandle(s);
}

TEST(th_mod_a_parity) {
    NEED_MOD(); REQUIRE(UN(Module32First)); REQUIRE(UN(Module32Next));
    HANDLE s = U(CreateToolhelp32Snapshot)(TH32CS_SNAPMODULE, 0), s2 = U(CreateToolhelp32Snapshot)(TH32CS_SNAPMODULE, 0);
    if (s == INVALID_HANDLE_VALUE || s2 == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    MODULEENTRY32 a; MODULEENTRY32W w; int n = 0, mism = 0;
    a.dwSize = sizeof a; w.dwSize = sizeof w;
    BOOL oa = UN(Module32First)(s, &a), ow = U(Module32FirstW)(s2, &w);
    CHECK(oa && ow);
    while (oa && ow) {
        char c1[MAX_PATH], c2[MAX_PATH]; thp_w2a(w.szModule, c1, MAX_PATH); thp_w2a(w.szExePath, c2, MAX_PATH);
        if (a.modBaseAddr != w.modBaseAddr || a.modBaseSize != w.modBaseSize || a.hModule != w.hModule ||
            strcmp(c1, a.szModule) || strcmp(c2, a.szExePath) || a.th32ProcessID != w.th32ProcessID) mism++;
        n++;
        a.dwSize = sizeof a; w.dwSize = sizeof w;
        oa = UN(Module32Next)(s, &a); ow = U(Module32NextW)(s2, &w);
    }
    CHECK(oa == ow);
    EXPECT_ERR(ERROR_NO_MORE_FILES);
    CHECKF(mism == 0, "%d of %d entries differ", mism, n);
    CloseHandle(s); CloseHandle(s2);
}

TEST(th_mod_a_bad_dwsize) {
    NEED_MOD(); REQUIRE(UN(Module32First)); REQUIRE(UN(Module32Next));
    HANDLE s = U(CreateToolhelp32Snapshot)(TH32CS_SNAPMODULE, 0);
    if (s == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    MODULEENTRY32 a;
    DWORD bad[] = { 0, sizeof a - 1, sizeof a + 1, PSZ_MOD_W };
    for (int i = 0; i < 4; i++) {
        a.dwSize = bad[i]; SET_ERR(0);
        CHECKF(!UN(Module32First)(s, &a), "dwSize 0x%lx", bad[i]);
        t_note("bad dwSize err=%lu", GetLastError());
        a.dwSize = bad[i]; SET_ERR(0);
        CHECKF(!UN(Module32Next)(s, &a), "next dwSize 0x%lx", bad[i]);
        t_note("bad dwSize err=%lu", GetLastError());
    }
    CloseHandle(s);
}

TEST(th_mod_loadlibrary_appears_and_vanishes) {
    NEED_MOD();
    if (GetModuleHandleW(L"winmm.dll")) { t_skip("winmm already loaded"); return; }
    HMODULE h = LoadLibraryW(L"winmm.dll");
    if (!h) { t_skip("winmm unavailable"); return; }
    static MODULEENTRY32W l[512];
    int n = mods_of(0, l, 512, TH32CS_SNAPMODULE);
    MODULEENTRY32W *m = find_mod(l, n, L"winmm.dll");
    CHECK(m != NULL);
    if (m) {
        EXPECT_EQ(m->modBaseAddr, h);
        DWORD sz, ep; thp_chk_pe(h, &sz, &ep);
        EXPECT_EQ(m->modBaseSize, sz);
        CHECK(m->GlblcntUsage >= 1);
    }
    FreeLibrary(h);
    n = mods_of(0, l, 512, TH32CS_SNAPMODULE);
    CHECK(find_mod(l, n, L"winmm.dll") == NULL);
}

TEST(th_mod_refcount_usage) {
    NEED_MOD();
    if (GetModuleHandleW(L"winmm.dll")) { t_skip("winmm already loaded"); return; }
    HMODULE h1 = LoadLibraryW(L"winmm.dll"), h2 = LoadLibraryW(L"winmm.dll");
    if (!h1) { t_skip("winmm unavailable"); return; }
    static MODULEENTRY32W l[512];
    int n = mods_of(0, l, 512, TH32CS_SNAPMODULE);
    MODULEENTRY32W *m = find_mod(l, n, L"winmm.dll");
    CHECK(m != NULL);
    if (m) t_note("winmm Glblcnt=%lu Proccnt=%lu after 2 loads", m->GlblcntUsage, m->ProccntUsage);
    FreeLibrary(h1); FreeLibrary(h2);
}

TEST(th_mod_module32_flag_native_process) {
    NEED_MOD();
    static MODULEENTRY32W a[512], b[512];
    int n8 = mods_of(0, a, 512, TH32CS_SNAPMODULE);
    int n18 = mods_of(0, b, 512, TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32);
    t_note("MODULE=%d MODULE|MODULE32=%d", n8, n18);
    int n10 = mods_of(0, b, 512, TH32CS_SNAPMODULE32);
    t_note("MODULE32 only=%d", n10);
    CHECK(n8 > 0 && n18 > 0);
    CHECK(n8 == n18);       /* no 32-bit modules in a native 64-bit process */
}

TEST(th_mod_explicit_self_pid_same_as_zero) {
    NEED_MOD();
    static MODULEENTRY32W a[512], b[512];
    int n0 = mods_of(0, a, 512, TH32CS_SNAPMODULE);
    int n1 = mods_of(GetCurrentProcessId(), b, 512, TH32CS_SNAPMODULE);
    EXPECT_EQ(n0, n1);
    int bad = 0; for (int i = 0; i < n0 && i < n1; i++) if (a[i].modBaseAddr != b[i].modBaseAddr) bad++;
    CHECK(bad == 0);
}

TEST(th_mod_entries_sane) {
    NEED_MOD();
    static MODULEENTRY32W l[512];
    int n = mods_of(0, l, 512, TH32CS_SNAPMODULE);
    for (int i = 0; i < n; i++) {
        CHECKF(l[i].modBaseSize > 0 && ((ULONG_PTR)l[i].modBaseAddr & 0xFFFF) == 0, "entry %d %ls", i, l[i].szModule);
        CHECKF(l[i].szModule[0] && l[i].szExePath[0], "entry %d empty names", i);
        CHECKF(wcslen(l[i].szModule) <= MAX_MODULE_NAME32, "entry %d name length", i);
        CHECK(thp_wcsi_tail(l[i].szExePath, l[i].szModule));
        for (int j = i + 1; j < n; j++) CHECKF(l[i].modBaseAddr != l[j].modBaseAddr, "dup base %p", l[i].modBaseAddr);
    }
}

TEST(th_mod_child_suspended_state) {
    NEED_MOD();
    PROCESS_INFORMATION pi;
    if (!thp_spawn(&pi, CREATE_SUSPENDED)) { CHECK(0); return; }
    static MODULEENTRY32W l[64];
    SET_ERR(0);
    int n = mods_of(pi.dwProcessId, l, 64, TH32CS_SNAPMODULE);
    if (n < 0) {
        DWORD e = GetLastError();
        t_note("suspended-child module snapshot failed err=%lu", e);
        CHECKF(e == ERROR_PARTIAL_COPY || e == ERROR_BAD_LENGTH, "err=%lu", e);
    } else {
        t_note("suspended-child module snapshot: %d modules", n);
        CHECK(n <= 3);
        if (n >= 1) CHECK(thp_wcsi_tail(l[0].szExePath, thp_basew(l[0].szExePath)));
    }
    thp_reap(&pi);
}

TEST(th_mod_child_running) {
    NEED_MOD();
    PROCESS_INFORMATION pi;
    if (!thp_spawn(&pi, 0)) { CHECK(0); return; }
    BYTE *k = thp_wait_module(pi.dwProcessId, L"kernel32.dll", 10000);
    CHECK(k != NULL);
    static MODULEENTRY32W l[512];
    int n = mods_of(pi.dwProcessId, l, 512, TH32CS_SNAPMODULE);
    CHECK(n >= 4);
    if (n >= 1) {
        char ex[MAX_PATH]; thp_w2a(l[0].szExePath, ex, MAX_PATH);
        CHECK(!lstrcmpiA(ex, t_self_path()));
        EXPECT_EQ(l[0].th32ProcessID, pi.dwProcessId);
    }
    MODULEENTRY32W *nt = find_mod(l, n, L"ntdll.dll"), *kb = find_mod(l, n, L"kernelbase.dll");
    CHECK(nt && kb);
    if (nt && !t_is_wine()) EXPECT_EQ(nt->modBaseAddr, GetModuleHandleW(L"ntdll.dll"));
    if (kb && !t_is_wine()) EXPECT_EQ(kb->modBaseAddr, GetModuleHandleW(L"kernelbase.dll"));
    thp_reap(&pi);
}

TEST(th_mod_terminated_child_fails) {
    NEED_MOD();
    PROCESS_INFORMATION pi;
    if (!thp_spawn(&pi, 0)) { CHECK(0); return; }
    DWORD pid = pi.dwProcessId;
    thp_wait_module(pid, L"kernel32.dll", 10000);
    TerminateProcess(pi.hProcess, 3);
    WaitForSingleObject(pi.hProcess, 10000);
    /* handle still open: process object exists but has exited */
    SET_ERR(0);
    HANDLE s = U(CreateToolhelp32Snapshot)(TH32CS_SNAPMODULE, pid);
    if (s != INVALID_HANDLE_VALUE) { t_note("module snapshot of exited process succeeded"); CloseHandle(s); }
    else t_note("module snapshot of exited process err=%lu", GetLastError());
    thp_reap(&pi);
    CHECK(1);
}

/* ---------------- heaps ---------------- */
#define NEED_HEAP() if (t_is_wine()) { t_skip("Heap32* unimplemented under wine"); return; } REQUIRE(U(CreateToolhelp32Snapshot)); REQUIRE(U(Heap32ListFirst)); REQUIRE(U(Heap32ListNext)); REQUIRE(U(Heap32First)); REQUIRE(U(Heap32Next))

TEST(th_heap_list_default_heap) {
    NEED_HEAP();
    HANDLE s = U(CreateToolhelp32Snapshot)(TH32CS_SNAPHEAPLIST, 0);
    if (s == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    HEAPLIST32 h; memset(&h, 0xCC, sizeof h); h.dwSize = sizeof h; SET_ERR(0);
    CHECK(U(Heap32ListFirst)(s, &h));
    EXPECT_EQ(h.dwSize, PSZ_HEAPL);
    int n = 0, foundDef = 0;
    do {
        n++;
        EXPECT_EQ(h.th32ProcessID, GetCurrentProcessId());
        CHECK(h.th32HeapID != 0);
        if (h.th32HeapID == (ULONG_PTR)GetProcessHeap()) { foundDef = 1; t_note("default heap dwFlags=%lu", h.dwFlags); }
        h.dwSize = sizeof h;
    } while (U(Heap32ListNext)(s, &h) && n < 10000);
    EXPECT_ERR(ERROR_NO_MORE_FILES);
    CHECK(foundDef);
    CHECK(n >= 1);
    CloseHandle(s);
}

TEST(th_heap_list_bad_dwsize) {
    NEED_HEAP();
    HANDLE s = U(CreateToolhelp32Snapshot)(TH32CS_SNAPHEAPLIST, 0);
    if (s == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    static const DWORD bad[] = { 0, 8, 0x1f, 0x21, 0x100 };
    for (int i = 0; i < 5; i++) {
        HEAPLIST32 h; memset(&h, 0xCC, sizeof h); h.dwSize = bad[i]; SET_ERR(0);
        CHECKF(!U(Heap32ListFirst)(s, &h), "dwSize 0x%lx", bad[i]);
        t_note("bad dwSize err=%lu", GetLastError());
        h.dwSize = bad[i]; SET_ERR(0);
        CHECKF(!U(Heap32ListNext)(s, &h), "next dwSize 0x%lx", bad[i]);
        t_note("bad dwSize err=%lu", GetLastError());
    }
    SET_ERR(0);
    CHECK(!U(Heap32ListFirst)(s, NULL));
    CloseHandle(s);
}

TEST(th_heap_list_invalid_handle) {
    NEED_HEAP();
    HEAPLIST32 h; h.dwSize = sizeof h; SET_ERR(0);
    CHECK(!U(Heap32ListFirst)(NULL, &h));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    h.dwSize = sizeof h; SET_ERR(0);
    CHECK(!U(Heap32ListNext)(NULL, &h));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}

TEST(th_heap_list_sees_new_heap) {
    NEED_HEAP();
    HANDLE hp = HeapCreate(0, 0x10000, 0);
    if (!hp) { CHECK(0); return; }
    HANDLE s = U(CreateToolhelp32Snapshot)(TH32CS_SNAPHEAPLIST, 0);
    HEAPLIST32 h; h.dwSize = sizeof h; int f = 0;
    BOOL ok = s != INVALID_HANDLE_VALUE && U(Heap32ListFirst)(s, &h);
    int n1 = 0;
    while (ok) { n1++; if (h.th32HeapID == (ULONG_PTR)hp) f = 1; h.dwSize = sizeof h; ok = U(Heap32ListNext)(s, &h); }
    if (s != INVALID_HANDLE_VALUE) CloseHandle(s);
    CHECK(f);
    HeapDestroy(hp);
    s = U(CreateToolhelp32Snapshot)(TH32CS_SNAPHEAPLIST, 0);
    f = 0; int n2 = 0;
    h.dwSize = sizeof h; ok = s != INVALID_HANDLE_VALUE && U(Heap32ListFirst)(s, &h);
    while (ok) { n2++; if (h.th32HeapID == (ULONG_PTR)hp) f = 1; h.dwSize = sizeof h; ok = U(Heap32ListNext)(s, &h); }
    if (s != INVALID_HANDLE_VALUE) CloseHandle(s);
    CHECK(!f);
    CHECK(n2 == n1 - 1);
}

TEST(th_heap_blocks_enumerate) {
    NEED_HEAP();
    HANDLE hp = HeapCreate(HEAP_NO_SERIALIZE, 0x10000, 0);
    if (!hp) { CHECK(0); return; }
    void *p[5]; static const SIZE_T sz[5] = { 1, 24, 100, 1000, 5000 };
    for (int i = 0; i < 5; i++) p[i] = HeapAlloc(hp, 0, sz[i]);
    HEAPENTRY32 he; memset(&he, 0, sizeof he); he.dwSize = sizeof he;
    SET_ERR(0);
    BOOL ok = U(Heap32First)(&he, GetCurrentProcessId(), (ULONG_PTR)hp);
    CHECK(ok);
    int n = 0, found[5] = {0}, freeb = 0, fixed = 0;
    while (ok && n < 100000) {
        n++;
        EXPECT_EQ(he.th32ProcessID, GetCurrentProcessId());
        EXPECT_EQ(he.th32HeapID, (ULONG_PTR)hp);
        if (he.dwFlags & LF32_FREE) freeb++;
        if (he.dwFlags & LF32_FIXED) fixed++;
        for (int i = 0; i < 5; i++) if (he.dwAddress >= (ULONG_PTR)p[i] - 32 && he.dwAddress <= (ULONG_PTR)p[i] && he.dwBlockSize >= sz[i]) found[i] = 1;
        he.dwSize = sizeof he;
        ok = U(Heap32Next)(&he);
    }
    t_note("heap blocks=%d free=%d fixed=%d", n, freeb, fixed);
    CHECK(n >= 5);
    for (int i = 0; i < 5; i++) if (!found[i]) t_note("block %d (size %llu) not matched by dwAddress", i, (unsigned long long)sz[i]);
    CHECK(!ok);
    for (int i = 0; i < 5; i++) HeapFree(hp, 0, p[i]);
    HeapDestroy(hp);
}

TEST(th_heap_first_invalid) {
    NEED_HEAP();
    HEAPENTRY32 he; memset(&he, 0, sizeof he);
    he.dwSize = sizeof he; SET_ERR(0);
    CHECK(!U(Heap32First)(&he, GetCurrentProcessId(), 0x1234));
    t_note("Heap32First bogus heap err=%lu", GetLastError());
    he.dwSize = 0; SET_ERR(0);
    CHECK(!U(Heap32First)(&he, GetCurrentProcessId(), (ULONG_PTR)GetProcessHeap()));
    t_note("Heap32First dwSize=0 err=%lu", GetLastError());
    he.dwSize = sizeof he; SET_ERR(0);
    CHECK(!U(Heap32First)(&he, 0xFFFFFFF0u, (ULONG_PTR)GetProcessHeap()));
    t_note("Heap32First bogus pid err=%lu", GetLastError());
}

/* ---------------- Toolhelp32ReadProcessMemory ---------------- */
static const char g_marker[64] = "k32t-toolhelp-readprocessmemory-marker-0123456789abcdef!!!";

TEST(th_rpm_self_pid_and_zero) {
    REQUIRE(U(Toolhelp32ReadProcessMemory));
    for (int k = 0; k < 2; k++) {
        char buf[64]; SIZE_T got = 77; memset(buf, 0, sizeof buf);
        SET_ERR(0);
        BOOL r = U(Toolhelp32ReadProcessMemory)(k ? GetCurrentProcessId() : 0, g_marker, buf, sizeof buf, &got);
        CHECKF(r, "k=%d err=%lu", k, GetLastError());
        EXPECT_EQ(got, sizeof buf);
        CHECK(!memcmp(buf, g_marker, sizeof buf));
    }
}

TEST(th_rpm_null_count_pointer) {
    REQUIRE(U(Toolhelp32ReadProcessMemory));
    char buf[16];
    CHECK(U(Toolhelp32ReadProcessMemory)(0, g_marker, buf, sizeof buf, NULL));
    CHECK(!memcmp(buf, g_marker, 16));
}

TEST(th_rpm_zero_size) {
    REQUIRE(U(Toolhelp32ReadProcessMemory));
    char buf[4] = { 1, 2, 3, 4 }; SIZE_T got = 5; SET_ERR(0);
    BOOL r = U(Toolhelp32ReadProcessMemory)(0, g_marker, buf, 0, &got);
    t_note("zero-size read: ret=%d got=%llu err=%lu", r, (unsigned long long)got, GetLastError());
    CHECK(buf[0] == 1);
}

TEST(th_rpm_unmapped_address) {
    REQUIRE(U(Toolhelp32ReadProcessMemory));
    char buf[16]; SIZE_T got = 99; SET_ERR(0);
    BOOL r = U(Toolhelp32ReadProcessMemory)(0, (LPCVOID)0x10, buf, sizeof buf, &got);
    CHECK(!r);
    DWORD e = GetLastError();
    t_note("read of 0x10: err=%lu got=%llu", e, (unsigned long long)got);
    CHECKF(e == ERROR_PARTIAL_COPY || e == ERROR_NOACCESS || e == ERROR_INVALID_ADDRESS, "err=%lu", e);
}

TEST(th_rpm_partial_across_boundary) {
    REQUIRE(U(Toolhelp32ReadProcessMemory));
    BYTE *p = VirtualAlloc(NULL, 0x2000, MEM_RESERVE, PAGE_NOACCESS);
    if (!p) { CHECK(0); return; }
    VirtualAlloc(p, 0x1000, MEM_COMMIT, PAGE_READWRITE);
    memset(p, 0x5A, 0x1000);
    char buf[64]; SIZE_T got = 0; memset(buf, 0, sizeof buf);
    SET_ERR(0);
    BOOL r = U(Toolhelp32ReadProcessMemory)(0, p + 0x1000 - 16, buf, 64, &got);
    t_note("boundary read: ret=%d got=%llu err=%lu", r, (unsigned long long)got, GetLastError());
    CHECK(!r);
    CHECK(got <= 16);
    VirtualFree(p, 0, MEM_RELEASE);
}

TEST(th_rpm_child_image_header) {
    REQUIRE(U(Toolhelp32ReadProcessMemory)); REQUIRE(U(CreateToolhelp32Snapshot));
    PROCESS_INFORMATION pi;
    if (!thp_spawn(&pi, 0)) { CHECK(0); return; }
    BYTE *base = thp_wait_module(pi.dwProcessId, L"kernel32.dll", 10000);
    static MODULEENTRY32W l[64];
    int n = mods_of(pi.dwProcessId, l, 64, TH32CS_SNAPMODULE);
    CHECK(n >= 1);
    if (n >= 1) {
        BYTE hdr[0x40]; SIZE_T got = 0; SET_ERR(0);
        BOOL r = U(Toolhelp32ReadProcessMemory)(pi.dwProcessId, l[0].modBaseAddr, hdr, sizeof hdr, &got);
        CHECKF(r, "err=%lu", GetLastError());
        EXPECT_EQ(got, sizeof hdr);
        CHECK(hdr[0] == 'M' && hdr[1] == 'Z');
        SET_ERR(0);
        r = U(Toolhelp32ReadProcessMemory)(pi.dwProcessId, (LPCVOID)0x8, hdr, 4, &got);
        CHECK(!r);
    }
    thp_reap(&pi);
}

TEST(th_rpm_bad_pid) {
    REQUIRE(U(Toolhelp32ReadProcessMemory));
    char buf[8]; SIZE_T got = 5; SET_ERR(0);
    BOOL r = U(Toolhelp32ReadProcessMemory)(0xFFFFFFF0u, g_marker, buf, 8, &got);
    CHECK(!r);
    t_note("bad pid read err=%lu", GetLastError());
}
