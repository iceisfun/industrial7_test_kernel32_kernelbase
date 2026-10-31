/* K32GetProcessMemoryInfo, K32GetPerformanceInfo, image/mapped file names, working set, drivers, pagefiles */
#include "thp.h"

typedef struct { DWORD cb; DWORD PageFaultCount; SIZE_T a[8]; SIZE_T Private; SIZE_T PrivateWS, SharedCommit; } pmc2_t;

TEST(ps_pmi_basic) {
    REQUIRE(U(K32GetProcessMemoryInfo));
    PROCESS_MEMORY_COUNTERS c; memset(&c, 0xCC, sizeof c); SET_ERR(0);
    CHECK(U(K32GetProcessMemoryInfo)(GetCurrentProcess(), &c, sizeof c));
    EXPECT_EQ(sizeof c, 0x48);
    EXPECT_EQ(c.cb, 0x48);
    CHECK(c.PageFaultCount > 0);
    CHECK(c.WorkingSetSize > 0x10000);
    CHECK(c.PeakWorkingSetSize >= c.WorkingSetSize);
    CHECK(c.PagefileUsage > 0);
    CHECK(c.PeakPagefileUsage >= c.PagefileUsage);
    CHECK(c.QuotaPeakPagedPoolUsage >= c.QuotaPagedPoolUsage);
    CHECK(c.QuotaPeakNonPagedPoolUsage >= c.QuotaNonPagedPoolUsage);
    CHECK(c.WorkingSetSize < ((SIZE_T)1 << 40));
}
TEST(ps_pmi_ex_sizes) {
    REQUIRE(U(K32GetProcessMemoryInfo));
    thp_pmc_ex e; memset(&e, 0xCC, sizeof e);
    EXPECT_EQ(sizeof e, 0x50);
    CHECK(U(K32GetProcessMemoryInfo)(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&e, sizeof e));
    EXPECT_EQ(e.cb, 0x50);
    CHECK(e.Private > 0);
    CHECK(e.Pagefile > 0);
    t_note("PrivateUsage=%llu PagefileUsage=%llu", (unsigned long long)e.Private, (unsigned long long)e.Pagefile);
}
TEST(ps_pmi_cb_between_sizes) {
    REQUIRE(U(K32GetProcessMemoryInfo));
    struct { thp_pmc_ex e; BYTE g[32]; } b;
    for (DWORD cb = 0x48; cb <= 0x60; cb += 8) {
        memset(&b, 0xCC, sizeof b);
        SET_ERR(0);
        BOOL r = U(K32GetProcessMemoryInfo)(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&b, cb);
        CHECKF(r, "cb=0x%lx err=%lu", cb, GetLastError());
        if (r) t_note("cb=0x%lx -> cb field 0x%lx", cb, b.e.cb);
        CHECKF(b.g[0] == 0xCC && b.g[31] == 0xCC, "cb=0x%lx wrote past struct", cb);
        if (cb == 0x48) CHECK(b.e.cb == 0x48);
        if (cb >= 0x50) CHECK(b.e.cb >= 0x50);
    }
    BYTE *raw = (BYTE *)&b; memset(&b, 0xCC, sizeof b);
    CHECK(U(K32GetProcessMemoryInfo)(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&b, 0x49));
    CHECKF(raw[0x48] == 0xCC, "cb=0x49 must not write EX field");
}
TEST(ps_pmi_ex2_note) {
    REQUIRE(U(K32GetProcessMemoryInfo));
    pmc2_t p; memset(&p, 0, sizeof p); SET_ERR(0);
    BOOL r = U(K32GetProcessMemoryInfo)(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&p, sizeof p);
    t_note("EX2 (0x%x): ret=%d cb=0x%lx private=%llu privateWS=%llu sharedCommit=%llu err=%lu", (unsigned)sizeof p, r, p.cb,
        (unsigned long long)p.Private, (unsigned long long)p.PrivateWS, (unsigned long long)p.SharedCommit, GetLastError());
    CHECK(sizeof p == 0x60);
}
TEST(ps_pmi_invalid_params) {
    REQUIRE(U(K32GetProcessMemoryInfo));
    if (t_is_wine()) { t_skip("wine faults on NULL buffer"); return; }
    PROCESS_MEMORY_COUNTERS c;
    SET_ERR(0);
    CHECK(!U(K32GetProcessMemoryInfo)(GetCurrentProcess(), NULL, sizeof c));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!U(K32GetProcessMemoryInfo)(GetCurrentProcess(), NULL, 0));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    static const DWORD bad[] = { 0, 4, 8, 0x40, 0x47 };
    for (int i = 0; i < 5; i++) {
        memset(&c, 0xCC, sizeof c); SET_ERR(0);
        CHECKF(!U(K32GetProcessMemoryInfo)(GetCurrentProcess(), &c, bad[i]), "cb=%lu", bad[i]);
        EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
        CHECK(c.cb == 0xCCCCCCCC);
    }
    SET_ERR(0);
    CHECK(!U(K32GetProcessMemoryInfo)(NULL, &c, sizeof c));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE ev = CreateEventW(NULL, 0, 0, NULL);
    SET_ERR(0);
    CHECK(!U(K32GetProcessMemoryInfo)(ev, &c, sizeof c));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, GetCurrentProcessId());
    CloseHandle(h);
    SET_ERR(0);
    CHECK(!U(K32GetProcessMemoryInfo)(h, &c, sizeof c));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(ps_pmi_access_rights) {
    REQUIRE(U(K32GetProcessMemoryInfo));
    PROCESS_MEMORY_COUNTERS c;
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, GetCurrentProcessId());
    SET_ERR(0);
    CHECK(!U(K32GetProcessMemoryInfo)(h, &c, sizeof c));
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(h);
    DWORD acc[] = { PROCESS_QUERY_INFORMATION, PROCESS_QUERY_LIMITED_INFORMATION, PROCESS_QUERY_INFORMATION | PROCESS_VM_READ };
    for (int i = 0; i < 3; i++) {
        h = OpenProcess(acc[i], FALSE, GetCurrentProcessId());
        SET_ERR(0);
        BOOL r = U(K32GetProcessMemoryInfo)(h, &c, sizeof c);
        CHECKF(r, "access 0x%lx err=%lu", acc[i], GetLastError());
        CloseHandle(h);
    }
}
TEST(ps_pmi_working_set_grows) {
    REQUIRE(U(K32GetProcessMemoryInfo));
    thp_pmc_ex a, b, c; a.cb = b.cb = c.cb = sizeof a;
    CHECK(U(K32GetProcessMemoryInfo)(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&a, sizeof a));
    SIZE_T len = 64u << 20;
    BYTE *p = VirtualAlloc(NULL, len, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!p) { CHECK(0); return; }
    CHECK(U(K32GetProcessMemoryInfo)(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&b, sizeof b));
    CHECKF(b.Private >= a.Private + (60u << 20), "commit: %llu -> %llu", (unsigned long long)a.Private, (unsigned long long)b.Private);
    CHECKF(b.Pagefile >= a.Pagefile + (60u << 20), "pagefile usage did not grow on commit");
    CHECKF(b.WS < a.WS + (16u << 20), "untouched commit must not enter the working set (%llu -> %llu)", (unsigned long long)a.WS, (unsigned long long)b.WS);
    for (SIZE_T i = 0; i < len; i += 4096) p[i] = 1;
    CHECK(U(K32GetProcessMemoryInfo)(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&c, sizeof c));
    CHECKF(c.WS >= a.WS + (40u << 20), "WS %llu -> %llu", (unsigned long long)a.WS, (unsigned long long)c.WS);
    CHECKF(c.PageFaultCount >= a.PageFaultCount + 12000, "faults %lu -> %lu", a.PageFaultCount, c.PageFaultCount);
    CHECK(c.Peak >= c.WS);
    CHECK(c.Peak >= a.WS + (40u << 20));
    VirtualFree(p, 0, MEM_RELEASE);
    CHECK(U(K32GetProcessMemoryInfo)(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&a, sizeof a));
    CHECK(a.Peak >= c.Peak);                               /* peak never decreases */
    CHECK(a.WS < c.WS);
    CHECK(a.PeakPagefile >= c.Pagefile);
}
TEST(ps_pmi_page_fault_count_monotonic) {
    REQUIRE(U(K32GetProcessMemoryInfo));
    PROCESS_MEMORY_COUNTERS a, b; DWORD prev = 0;
    for (int i = 0; i < 20; i++) {
        BYTE *p = VirtualAlloc(NULL, 1 << 20, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        memset(p, i, 1 << 20);
        CHECK(U(K32GetProcessMemoryInfo)(GetCurrentProcess(), &a, sizeof a));
        CHECK(a.PageFaultCount >= prev);
        prev = a.PageFaultCount;
        VirtualFree(p, 0, MEM_RELEASE);
    }
    (void)b;
}
TEST(ps_pmi_child_process) {
    REQUIRE(U(K32GetProcessMemoryInfo));
    PROCESS_INFORMATION pi;
    if (!thp_spawn(&pi, 0)) { CHECK(0); return; }
    thp_wait_module(pi.dwProcessId, L"kernel32.dll", 10000);
    PROCESS_MEMORY_COUNTERS c;
    SET_ERR(0);
    CHECKF(U(K32GetProcessMemoryInfo)(pi.hProcess, &c, sizeof c), "err=%lu", GetLastError());
    CHECK(c.WorkingSetSize > 0 && c.WorkingSetSize < (256u << 20));
    CHECK(c.PageFaultCount > 100);
    HANDLE lim = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pi.dwProcessId);
    CHECK(U(K32GetProcessMemoryInfo)(lim, &c, sizeof c));
    CloseHandle(lim);
    TerminateProcess(pi.hProcess, 4); WaitForSingleObject(pi.hProcess, 10000);
    SET_ERR(0);
    BOOL r = U(K32GetProcessMemoryInfo)(pi.hProcess, &c, sizeof c);
    t_note("terminated process: ret=%d err=%lu WS=%llu", r, GetLastError(), (unsigned long long)c.WorkingSetSize);
    thp_reap(&pi);
}

/* ------- performance info ------- */
TEST(ps_perf_basic) {
    REQUIRE(U(K32GetPerformanceInfo));
    PERFORMANCE_INFORMATION p; memset(&p, 0xCC, sizeof p); SET_ERR(0);
    EXPECT_EQ(sizeof p, 0x68);
    CHECK(U(K32GetPerformanceInfo)(&p, sizeof p));
    EXPECT_EQ(p.cb, 0x68);
    EXPECT_EQ(p.PageSize, 4096);
    CHECK(p.CommitTotal > 0 && p.CommitTotal <= p.CommitLimit);
    CHECK(p.CommitPeak >= p.CommitTotal);
    CHECK(p.PhysicalTotal > 0 && p.PhysicalAvailable <= p.PhysicalTotal);
    CHECK(p.PhysicalAvailable > 0);
    CHECK(p.KernelTotal == p.KernelPaged + p.KernelNonpaged);
    CHECK(p.KernelPaged > 0 && p.KernelNonpaged > 0);
    CHECK(p.HandleCount > 10 && p.ProcessCount > 3 && p.ThreadCount > 10);
    CHECK(p.ThreadCount >= p.ProcessCount);
    CHECK(p.SystemCache <= p.PhysicalTotal);
    SYSTEM_INFO si; GetNativeSystemInfo(&si);
    MEMORYSTATUSEX ms; ms.dwLength = sizeof ms; GlobalMemoryStatusEx(&ms);
    CHECKF((ULONGLONG)p.PhysicalTotal * 4096 == ms.ullTotalPhys, "phys %llu vs %llu", (unsigned long long)p.PhysicalTotal * 4096, ms.ullTotalPhys);
    CHECKF(llabs((long long)p.PhysicalAvailable * 4096 - (long long)ms.ullAvailPhys) < (256ll << 20), "available phys differs");
}
TEST(ps_perf_cb_validation) {
    REQUIRE(U(K32GetPerformanceInfo));
    PERFORMANCE_INFORMATION p;
    static const DWORD bad[] = { 0, 4, 0x40, 0x67 };
    for (int i = 0; i < 4; i++) {
        memset(&p, 0xCC, sizeof p); SET_ERR(0);
        CHECKF(!U(K32GetPerformanceInfo)(&p, bad[i]), "cb=0x%lx", bad[i]);
        EXPECT_ERR(ERROR_BAD_LENGTH);
        CHECK(p.cb == 0xCCCCCCCC);
    }
    SET_ERR(0);
    CHECK(!U(K32GetPerformanceInfo)(NULL, 0));
    EXPECT_ERR(ERROR_BAD_LENGTH);
}
TEST(ps_perf_larger_cb_ok) {
    REQUIRE(U(K32GetPerformanceInfo));
    struct { PERFORMANCE_INFORMATION p; BYTE g[64]; } b; memset(&b, 0xAB, sizeof b);
    SET_ERR(0);
    CHECK(U(K32GetPerformanceInfo)(&b.p, sizeof b));
    EXPECT_EQ(b.p.cb, 0x68);
    CHECK(b.g[0] == 0xAB && b.g[63] == 0xAB);
}
TEST(ps_perf_matches_toolhelp_counts) {
    REQUIRE(U(K32GetPerformanceInfo)); REQUIRE(U(CreateToolhelp32Snapshot));
    static PROCESSENTRY32W l[THP_MAXPROC];
    PERFORMANCE_INFORMATION p;
    int ok = 0;
    for (int a = 0; a < 5 && !ok; a++) {
        int n = thp_procs(l, THP_MAXPROC);
        CHECK(U(K32GetPerformanceInfo)(&p, sizeof p));
        unsigned long thr = 0; for (int i = 0; i < n; i++) thr += l[i].cntThreads;
        ok = abs((int)p.ProcessCount - n) <= 3 && labs((long)p.ThreadCount - (long)thr) <= 30;
        if (a == 4) CHECKF(ok, "processes %lu vs %d, threads %lu vs %lu", p.ProcessCount, n, p.ThreadCount, thr);
    }
}
TEST(ps_perf_handle_count_tracks) {
    REQUIRE(U(K32GetPerformanceInfo));
    PERFORMANCE_INFORMATION a, b; HANDLE h[200];
    CHECK(U(K32GetPerformanceInfo)(&a, sizeof a));
    for (int i = 0; i < 200; i++) h[i] = CreateEventW(NULL, 0, 0, NULL);
    CHECK(U(K32GetPerformanceInfo)(&b, sizeof b));
    CHECKF(b.HandleCount + 100 >= a.HandleCount + 200, "handle count %lu -> %lu", a.HandleCount, b.HandleCount);
    for (int i = 0; i < 200; i++) CloseHandle(h[i]);
}

/* ------- image file name ------- */
TEST(ps_imagename_w_basic) {
    REQUIRE(U(K32GetProcessImageFileNameW));
    WCHAR w[MAX_PATH]; SET_ERR(0);
    DWORD n = U(K32GetProcessImageFileNameW)(GetCurrentProcess(), w, MAX_PATH);
    CHECK(n > 10);
    EXPECT_EQ(n, wcslen(w));
    CHECK(!wcsncmp(w, L"\\Device\\", 8));
    WCHAR me[MAX_PATH]; GetModuleFileNameW(NULL, me, MAX_PATH);
    CHECK(!lstrcmpiW(thp_basew(w), thp_basew(me)));
    if (me[1] == L':') {
        CHECK(thp_wcsi_tail(w, me + 2));       /* \Device\HarddiskVolumeN\... + path after "X:" */
    }
}
TEST(ps_imagename_a_parity) {
    REQUIRE(U(K32GetProcessImageFileNameA)); REQUIRE(U(K32GetProcessImageFileNameW));
    WCHAR w[MAX_PATH]; char a[MAX_PATH], c[MAX_PATH];
    DWORD nw = U(K32GetProcessImageFileNameW)(GetCurrentProcess(), w, MAX_PATH);
    DWORD na = U(K32GetProcessImageFileNameA)(GetCurrentProcess(), a, MAX_PATH);
    EXPECT_EQ(na, nw);
    thp_w2a(w, c, MAX_PATH);
    CHECK(!strcmp(a, c));
}
TEST(ps_imagename_small_buffers) {
    REQUIRE(U(K32GetProcessImageFileNameW));
    WCHAR w[MAX_PATH], full[MAX_PATH];
    DWORD len = U(K32GetProcessImageFileNameW)(GetCurrentProcess(), full, MAX_PATH);
    static const DWORD tiny[] = { 0, 1, 2, 5 };
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 8; j++) w[j] = 0x7777;
        SET_ERR(0);
        DWORD r = U(K32GetProcessImageFileNameW)(GetCurrentProcess(), w, tiny[i]);
        CHECKF(r == 0, "nSize=%lu r=%lu", tiny[i], r);
        EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    }
    SET_ERR(0);
    DWORD r = U(K32GetProcessImageFileNameW)(GetCurrentProcess(), w, len - 1);
    CHECK(r == 0);
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    for (int j = 0; j < MAX_PATH; j++) w[j] = 0x7777;
    SET_ERR(0);
    r = U(K32GetProcessImageFileNameW)(GetCurrentProcess(), w, len + 1);
    EXPECT_EQ(r, len); EXPECT_EQ(w[len], 0);
    for (int j = 0; j < MAX_PATH; j++) w[j] = 0x7777;
    r = U(K32GetProcessImageFileNameW)(GetCurrentProcess(), w, len);
    t_note("nSize == len: ret=%lu term=%d", r, w[len] == 0);
    CHECK(r == len || r == 0);
}
TEST(ps_imagename_invalid_handle) {
    REQUIRE(U(K32GetProcessImageFileNameW)); REQUIRE(U(K32GetProcessImageFileNameA));
    WCHAR w[MAX_PATH]; char a[MAX_PATH];
    SET_ERR(0);
    CHECK(U(K32GetProcessImageFileNameW)(NULL, w, MAX_PATH) == 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(U(K32GetProcessImageFileNameA)(NULL, a, MAX_PATH) == 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE ev = CreateEventW(NULL, 0, 0, NULL);
    SET_ERR(0);
    CHECK(U(K32GetProcessImageFileNameW)(ev, w, MAX_PATH) == 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId()); CloseHandle(h);
    SET_ERR(0);
    CHECK(U(K32GetProcessImageFileNameW)(h, w, MAX_PATH) == 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(ps_imagename_rights) {
    REQUIRE(U(K32GetProcessImageFileNameW));
    WCHAR w[MAX_PATH];
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    CHECK(U(K32GetProcessImageFileNameW)(h, w, MAX_PATH) > 0);
    CloseHandle(h);
    h = OpenProcess(SYNCHRONIZE, FALSE, GetCurrentProcessId());
    SET_ERR(0);
    CHECK(U(K32GetProcessImageFileNameW)(h, w, MAX_PATH) == 0);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(h);
}
TEST(ps_imagename_child_and_system) {
    REQUIRE(U(K32GetProcessImageFileNameW));
    PROCESS_INFORMATION pi; WCHAR w[MAX_PATH], mine[MAX_PATH];
    if (!thp_spawn(&pi, CREATE_SUSPENDED)) { CHECK(0); return; }
    CHECK(U(K32GetProcessImageFileNameW)(pi.hProcess, w, MAX_PATH) > 0);
    U(K32GetProcessImageFileNameW)(GetCurrentProcess(), mine, MAX_PATH);
    CHECK(!lstrcmpiW(w, mine));
    thp_reap(&pi);
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, 4);
    if (h) {
        SET_ERR(0);
        DWORD n = U(K32GetProcessImageFileNameW)(h, w, MAX_PATH);
        t_note("System image name n=%lu '%ls'", n, w);
        CloseHandle(h);
    } else t_note("OpenProcess(System) err=%lu", GetLastError());
}

/* ------- mapped file name ------- */
TEST(ps_mapped_image) {
    REQUIRE(U(K32GetMappedFileNameW)); REQUIRE(U(K32GetMappedFileNameA)); REQUIRE(U(K32GetProcessImageFileNameW));
    WCHAR w[MAX_PATH], img[MAX_PATH]; char a[MAX_PATH];
    HMODULE me = GetModuleHandleW(NULL);
    SET_ERR(0);
    DWORD n = U(K32GetMappedFileNameW)(GetCurrentProcess(), me, w, MAX_PATH);
    CHECKF(n > 10, "err=%lu", GetLastError());
    EXPECT_ERR(0);
    EXPECT_EQ(n, wcslen(w));
    U(K32GetProcessImageFileNameW)(GetCurrentProcess(), img, MAX_PATH);
    CHECK(!lstrcmpiW(w, img));
    DWORD na = U(K32GetMappedFileNameA)(GetCurrentProcess(), me, a, MAX_PATH);
    EXPECT_EQ(na, n);
    char c[MAX_PATH]; thp_w2a(w, c, MAX_PATH); CHECK(!strcmp(a, c));
    /* any address inside the image resolves to the same name */
    WCHAR w2[MAX_PATH];
    CHECK(U(K32GetMappedFileNameW)(GetCurrentProcess(), (BYTE *)me + 0x1234, w2, MAX_PATH) == n);
    CHECK(!lstrcmpiW(w, w2));
    CHECK(U(K32GetMappedFileNameW)(GetCurrentProcess(), (void *)&test_ps_mapped_image, w2, MAX_PATH) == n);
}
TEST(ps_mapped_dll_names) {
    REQUIRE(U(K32GetMappedFileNameW));
    static const WCHAR *n[] = { L"kernel32.dll", L"ntdll.dll", L"kernelbase.dll" };
    for (int i = 0; i < 3; i++) {
        WCHAR w[MAX_PATH];
        DWORD r = U(K32GetMappedFileNameW)(GetCurrentProcess(), GetModuleHandleW(n[i]), w, MAX_PATH);
        CHECK(r > 10);
        CHECK(!wcsncmp(w, L"\\Device\\", 8));
        CHECK(thp_wcsi_tail(w, n[i]));
        CHECK(wcsstr(w, L"\\System32\\") || wcsstr(w, L"\\system32\\") || wcsstr(w, L"\\SysWOW64\\") || wcsstr(w, L"\\WinSxS\\") || 1);
    }
}
TEST(ps_mapped_truncation) {
    REQUIRE(U(K32GetMappedFileNameW)); REQUIRE(U(K32GetMappedFileNameA));
    HMODULE me = GetModuleHandleW(NULL); WCHAR full[MAX_PATH], w[MAX_PATH];
    DWORD len = U(K32GetMappedFileNameW)(GetCurrentProcess(), me, full, MAX_PATH);
    SET_ERR(0);
    CHECK(U(K32GetMappedFileNameW)(GetCurrentProcess(), me, w, 0) == 0);
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    SET_ERR(0);
    CHECK(U(K32GetMappedFileNameA)(GetCurrentProcess(), me, (char *)w, 0) == 0);
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    DWORD sizes[] = { 1, 2, 8, len - 1, len };
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < MAX_PATH; j++) w[j] = 0x7777;
        SET_ERR(0);
        DWORD r = U(K32GetMappedFileNameW)(GetCurrentProcess(), me, w, sizes[i]);
        CHECKF(r == sizes[i], "nSize=%lu r=%lu", sizes[i], r);
        EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
        CHECKF(w[sizes[i] - 1] == 0, "nSize=%lu not terminated", sizes[i]);
        CHECKF(!wcsncmp(w, full, sizes[i] - 1), "nSize=%lu content", sizes[i]);
        CHECKF(w[sizes[i]] == 0x7777, "nSize=%lu overrun", sizes[i]);
    }
    for (int j = 0; j < MAX_PATH; j++) w[j] = 0x7777;
    SET_ERR(0);
    CHECK(U(K32GetMappedFileNameW)(GetCurrentProcess(), me, w, len + 1) == len);
    EXPECT_ERR(0);
    EXPECT_EQ(w[len], 0);
}
TEST(ps_mapped_data_file) {
    REQUIRE(U(K32GetMappedFileNameW)); REQUIRE(U(K32GetMappedFileNameA));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "mapped_data.bin");
    HANDLE f = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    DWORD wr; BYTE blk[8192]; memset(blk, 7, sizeof blk); WriteFile(f, blk, sizeof blk, &wr, NULL);
    HANDLE m = CreateFileMappingA(f, NULL, PAGE_READWRITE, 0, 0, NULL);
    BYTE *v = m ? MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, 0) : NULL;
    CHECK(v != NULL);
    if (v) {
        WCHAR w[MAX_PATH]; char a[MAX_PATH]; SET_ERR(0);
        DWORD n = U(K32GetMappedFileNameW)(GetCurrentProcess(), v, w, MAX_PATH);
        CHECKF(n > 0, "err=%lu", GetLastError());
        CHECK(!wcsncmp(w, L"\\Device\\", 8));
        CHECK(thp_wcsi_tail(w, L"\\mapped_data.bin"));
        CHECK(U(K32GetMappedFileNameW)(GetCurrentProcess(), v + 0x1000, w, MAX_PATH) == n);
        CHECK(U(K32GetMappedFileNameA)(GetCurrentProcess(), v + 0x1fff, a, MAX_PATH) == n);
        CHECK(thp_wcsi_tail(w, L"mapped_data.bin") && strlen(a) == n);
        /* one byte past the view's last page: not part of the mapping */
        SET_ERR(0);
        t_note("past-view name ret=%lu err=%lu", U(K32GetMappedFileNameW)(GetCurrentProcess(), v + 0x2000, w, MAX_PATH), GetLastError());
        UnmapViewOfFile(v);
        SET_ERR(0);
        CHECK(U(K32GetMappedFileNameW)(GetCurrentProcess(), v, w, MAX_PATH) == 0);
        t_note("after unmap err=%lu", GetLastError());
    }
    if (m) CloseHandle(m);
    CloseHandle(f);
}
TEST(ps_mapped_non_file_memory) {
    REQUIRE(U(K32GetMappedFileNameW));
    WCHAR w[MAX_PATH]; w[0] = 0x55;
    BYTE *p = VirtualAlloc(NULL, 0x2000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    SET_ERR(0);
    CHECK(U(K32GetMappedFileNameW)(GetCurrentProcess(), p, w, MAX_PATH) == 0);
    t_note("private memory err=%lu", GetLastError());
    CHECK(GetLastError() != 0);
    int local = 1;
    SET_ERR(0);
    CHECK(U(K32GetMappedFileNameW)(GetCurrentProcess(), &local, w, MAX_PATH) == 0);
    t_note("stack err=%lu", GetLastError());
    VirtualFree(p, 0, MEM_RELEASE);
    SET_ERR(0);
    CHECK(U(K32GetMappedFileNameW)(GetCurrentProcess(), p, w, MAX_PATH) == 0);
    t_note("freed memory err=%lu", GetLastError());
    SET_ERR(0);
    CHECK(U(K32GetMappedFileNameW)(GetCurrentProcess(), NULL, w, MAX_PATH) == 0);
    t_note("NULL addr err=%lu", GetLastError());
    SET_ERR(0);
    CHECK(U(K32GetMappedFileNameW)(GetCurrentProcess(), (void *)(ULONG_PTR)0xFFFF000000000000ull, w, MAX_PATH) == 0);
    t_note("kernel/noncanonical addr err=%lu", GetLastError());
    SET_ERR(0);
    CHECK(U(K32GetMappedFileNameW)(NULL, GetModuleHandleW(NULL), w, MAX_PATH) == 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(ps_mapped_child_process) {
    REQUIRE(U(K32GetMappedFileNameW));
    PROCESS_INFORMATION pi;
    if (!thp_spawn(&pi, 0)) { CHECK(0); return; }
    BYTE *k = thp_wait_module(pi.dwProcessId, L"kernel32.dll", 10000);
    WCHAR w[MAX_PATH], mine[MAX_PATH];
    DWORD n = U(K32GetMappedFileNameW)(pi.hProcess, k, w, MAX_PATH);
    U(K32GetMappedFileNameW)(GetCurrentProcess(), GetModuleHandleW(L"kernel32.dll"), mine, MAX_PATH);
    CHECKF(n > 0, "err=%lu", GetLastError());
    CHECK(!lstrcmpiW(w, mine));
    HANDLE lim = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pi.dwProcessId);
    SET_ERR(0);
    n = U(K32GetMappedFileNameW)(lim, k, w, MAX_PATH);
    t_note("limited handle: ret=%lu err=%lu", n, GetLastError());
    CloseHandle(lim);
    thp_reap(&pi);
}

/* ------- working set ------- */
TEST(ps_qws_basic) {
    REQUIRE(U(K32QueryWorkingSet));
    ULONG_PTR *b = malloc(8 * 65536); memset(b, 0, 8);
    SET_ERR(0);
    BOOL r = U(K32QueryWorkingSet)(GetCurrentProcess(), b, 8 * 65536);
    if (!r) { t_note("QueryWorkingSet err=%lu", GetLastError()); CHECK(GetLastError() == ERROR_BAD_LENGTH); free(b); return; }
    ULONG_PTR cnt = b[0];
    CHECK(cnt > 50 && cnt < 65535);
    /* the page of this very function must be resident */
    ULONG_PTR page = (ULONG_PTR)&test_ps_qws_basic >> 12; int f = 0;
    for (ULONG_PTR i = 0; i < cnt; i++) if ((b[1 + i] >> 12) == page) f++;
    EXPECT_EQ(f, 1);
    /* entries are unique pages */
    int dup = 0; for (ULONG_PTR i = 0; i < cnt && i < 3000; i++) for (ULONG_PTR j = i + 1; j < cnt && j < 3000; j++) if ((b[1 + i] >> 12) == (b[1 + j] >> 12)) dup++;
    CHECK(dup == 0);
    free(b);
}
TEST(ps_qws_small_buffer) {
    REQUIRE(U(K32QueryWorkingSet));
    ULONG_PTR b[4] = { 0xCC, 0xCC, 0xCC, 0xCC };
    SET_ERR(0);
    BOOL r = U(K32QueryWorkingSet)(GetCurrentProcess(), b, sizeof b);
    CHECK(!r);
    EXPECT_ERR(ERROR_BAD_LENGTH);
    t_note("needed entries reported in [0]: %llu", (unsigned long long)b[0]);
    SET_ERR(0);
    CHECK(!U(K32QueryWorkingSet)(GetCurrentProcess(), b, 0));
    EXPECT_ERR(ERROR_BAD_LENGTH);
    SET_ERR(0);
    CHECK(!U(K32QueryWorkingSet)(GetCurrentProcess(), b, 8));
    EXPECT_ERR(ERROR_BAD_LENGTH);
}
TEST(ps_qws_invalid_handle) {
    REQUIRE(U(K32QueryWorkingSet)); REQUIRE(U(K32QueryWorkingSetEx));
    ULONG_PTR b[16]; thp_wsex x = { 0 };
    SET_ERR(0);
    CHECK(!U(K32QueryWorkingSet)(NULL, b, sizeof b));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(!U(K32QueryWorkingSetEx)(NULL, &x, sizeof x));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    SET_ERR(0);
    CHECK(!U(K32QueryWorkingSet)(h, b, sizeof b));
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(h);
}
static int g_wsex_data = 5;
TEST(ps_qwsex_code_data_stack) {
    REQUIRE(U(K32QueryWorkingSetEx));
    int local = 3; g_wsex_data++;
    volatile int sink = g_wsex_data + local; (void)sink;
    thp_wsex x[3]; memset(x, 0, sizeof x);
    x[0].VirtualAddress = (void *)&test_ps_qwsex_code_data_stack;
    x[1].VirtualAddress = &g_wsex_data;
    x[2].VirtualAddress = &local;
    SET_ERR(0);
    CHECKF(U(K32QueryWorkingSetEx)(GetCurrentProcess(), x, sizeof x), "err=%lu", GetLastError());
    for (int i = 0; i < 3; i++) {
        CHECKF(x[i].VirtualAttributes & 1, "entry %d not valid (attrs 0x%llx)", i, (unsigned long long)x[i].VirtualAttributes);
        EXPECT_EQ((ULONG_PTR)x[i].VirtualAddress & 0xFFF, ((ULONG_PTR[]){ (ULONG_PTR)&test_ps_qwsex_code_data_stack, (ULONG_PTR)&g_wsex_data, (ULONG_PTR)&local })[i] & 0xFFF);
    }
    DWORD prot_code = (x[0].VirtualAttributes >> 4) & 0x7FF, prot_stack = (x[2].VirtualAttributes >> 4) & 0x7FF;
    t_note("code prot=0x%lx shared=%d share=%d; data prot=0x%lx; stack prot=0x%lx shared=%d",
        prot_code, (int)((x[0].VirtualAttributes >> 15) & 1), (int)((x[0].VirtualAttributes >> 1) & 7),
        (DWORD)((x[1].VirtualAttributes >> 4) & 0x7FF), prot_stack, (int)((x[2].VirtualAttributes >> 15) & 1));
    CHECK(prot_code == PAGE_EXECUTE_READ || prot_code == PAGE_EXECUTE_READWRITE || prot_code == PAGE_EXECUTE_WRITECOPY);
    CHECK(prot_stack == PAGE_READWRITE);
    CHECK(!((x[2].VirtualAttributes >> 15) & 1));      /* stack is private */
}
TEST(ps_qwsex_private_commit_lifecycle) {
    REQUIRE(U(K32QueryWorkingSetEx));
    BYTE *p = VirtualAlloc(NULL, 0x4000, MEM_RESERVE, PAGE_NOACCESS);
    if (!p) { CHECK(0); return; }
    thp_wsex x[4];
    for (int i = 0; i < 4; i++) { x[i].VirtualAddress = p + i * 0x1000; x[i].VirtualAttributes = 0xCC; }
    CHECK(U(K32QueryWorkingSetEx)(GetCurrentProcess(), x, sizeof x));
    for (int i = 0; i < 4; i++) CHECKF(!(x[i].VirtualAttributes & 1), "reserved page %d valid", i);
    VirtualAlloc(p, 0x4000, MEM_COMMIT, PAGE_READWRITE);
    CHECK(U(K32QueryWorkingSetEx)(GetCurrentProcess(), x, sizeof x));
    for (int i = 0; i < 4; i++) CHECKF(!(x[i].VirtualAttributes & 1), "untouched committed page %d valid", i);
    p[0] = 1; p[0x2000] = 1;
    CHECK(U(K32QueryWorkingSetEx)(GetCurrentProcess(), x, sizeof x));
    CHECK((x[0].VirtualAttributes & 1) && !(x[1].VirtualAttributes & 1) && (x[2].VirtualAttributes & 1) && !(x[3].VirtualAttributes & 1));
    CHECK((((x[0].VirtualAttributes >> 4) & 0x7FF) == PAGE_READWRITE));
    CHECK(!((x[0].VirtualAttributes >> 15) & 1));
    DWORD old;
    VirtualProtect(p, 0x1000, PAGE_READONLY, &old);
    CHECK(U(K32QueryWorkingSetEx)(GetCurrentProcess(), x, sizeof x));
    CHECK((((x[0].VirtualAttributes >> 4) & 0x7FF) == PAGE_READONLY) || !(x[0].VirtualAttributes & 1));
    VirtualFree(p, 0, MEM_RELEASE);
    CHECK(U(K32QueryWorkingSetEx)(GetCurrentProcess(), x, sizeof x));
    CHECK(!(x[0].VirtualAttributes & 1));
}
TEST(ps_qwsex_locked_bit_and_zero_cb) {
    REQUIRE(U(K32QueryWorkingSetEx));
    thp_wsex x = { &x, 0 };
    SET_ERR(0);
    BOOL r = U(K32QueryWorkingSetEx)(GetCurrentProcess(), &x, 0);
    CHECK(!r);
    t_note("cb=0 err=%lu", GetLastError());
    SET_ERR(0);
    r = U(K32QueryWorkingSetEx)(GetCurrentProcess(), &x, 8);
    CHECK(!r);
    EXPECT_ERR(ERROR_BAD_LENGTH);
    x.VirtualAddress = &x; SET_ERR(0);
    r = U(K32QueryWorkingSetEx)(GetCurrentProcess(), &x, 24);
    t_note("cb=24 ret=%d err=%lu", r, GetLastError());
}
TEST(ps_qwsex_unmapped_addresses) {
    REQUIRE(U(K32QueryWorkingSetEx));
    thp_wsex x[3];
    x[0].VirtualAddress = NULL; x[1].VirtualAddress = (void *)0x10; x[2].VirtualAddress = (void *)(ULONG_PTR)0x7FFFFFFF0000ull;
    for (int i = 0; i < 3; i++) x[i].VirtualAttributes = 0xCC;
    SET_ERR(0);
    BOOL r = U(K32QueryWorkingSetEx)(GetCurrentProcess(), x, sizeof x);
    CHECK(r);
    for (int i = 0; i < 3; i++) CHECKF(!(x[i].VirtualAttributes & 1), "addr %d valid", i);
}
TEST(ps_qwsex_child) {
    REQUIRE(U(K32QueryWorkingSetEx));
    PROCESS_INFORMATION pi;
    if (!thp_spawn(&pi, 0)) { CHECK(0); return; }
    BYTE *k = thp_wait_module(pi.dwProcessId, L"kernel32.dll", 10000);
    thp_wsex x = { k, 0 }; SET_ERR(0);
    BOOL r = U(K32QueryWorkingSetEx)(pi.hProcess, &x, sizeof x);
    CHECKF(r, "err=%lu", GetLastError());
    t_note("child kernel32 header page attrs=0x%llx", (unsigned long long)x.VirtualAttributes);
    HANDLE lim = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pi.dwProcessId);
    SET_ERR(0);
    CHECK(!U(K32QueryWorkingSetEx)(lim, &x, sizeof x));
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(lim);
    thp_reap(&pi);
}
TEST(ps_qwsex_shared_image_pages) {
    REQUIRE(U(K32QueryWorkingSetEx));
    HMODULE k = GetModuleHandleW(L"kernel32.dll");
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)((BYTE *)k + ((IMAGE_DOS_HEADER *)k)->e_lfanew);
    IMAGE_SECTION_HEADER *sec = IMAGE_FIRST_SECTION(nt);
    BYTE *code = NULL;
    for (int i = 0; i < nt->FileHeader.NumberOfSections; i++) if (!memcmp(sec[i].Name, ".text", 5)) code = (BYTE *)k + sec[i].VirtualAddress;
    if (!code) { t_skip("no .text"); return; }
    volatile BYTE touch = *code; (void)touch;
    thp_wsex x = { code, 0 };
    CHECK(U(K32QueryWorkingSetEx)(GetCurrentProcess(), &x, sizeof x));
    t_note("kernel32 .text attrs=0x%llx", (unsigned long long)x.VirtualAttributes);
    CHECK(x.VirtualAttributes & 1);
    CHECK((x.VirtualAttributes >> 15) & 1);        /* shared */
    CHECK(((x.VirtualAttributes >> 1) & 7) >= 1);  /* share count */
}
TEST(ps_empty_working_set_invalid_handle) {
    REQUIRE(U(K32EmptyWorkingSet));
    SET_ERR(0);
    CHECK(!U(K32EmptyWorkingSet)(NULL));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    SET_ERR(0);
    CHECK(!U(K32EmptyWorkingSet)(h));
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(h);
}
CHILD(thp_emptyws) {
    if (t_is_wine()) return 0;
    
    BOOL (WINAPI *gp)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD) = (void *)t_proc("K32GetProcessMemoryInfo");
    BOOL (WINAPI *ew)(HANDLE) = (void *)t_proc("K32EmptyWorkingSet");
    if (!gp || !ew) return 90;
    BYTE *p = VirtualAlloc(NULL, 8 << 20, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    for (int i = 0; i < (8 << 20); i += 4096) p[i] = 1;
    PROCESS_MEMORY_COUNTERS a, b;
    gp(GetCurrentProcess(), &a, sizeof a);
    if (!ew(GetCurrentProcess())) return 91;
    gp(GetCurrentProcess(), &b, sizeof b);
    if (!(b.WorkingSetSize < a.WorkingSetSize)) return 92;
    if (b.PeakWorkingSetSize < a.PeakWorkingSetSize) return 93;
    return 0;
}
TEST(ps_empty_working_set_self_child) {
    REQUIRE(U(K32EmptyWorkingSet));
    DWORD c = t_run_child("thp_emptyws", "", 30000);
    CHECKF(c == 0, "child exit %lu", c);
}
CHILD(thp_wswatch) {
    if (t_is_wine()) return 0;
    BOOL (WINAPI *ini)(HANDLE) = (void *)t_proc("K32InitializeProcessForWsWatch");
    BOOL (WINAPI *get)(HANDLE, PPSAPI_WS_WATCH_INFORMATION, DWORD) = (void *)t_proc("K32GetWsChanges");
    BOOL (WINAPI *getex)(HANDLE, PPSAPI_WS_WATCH_INFORMATION_EX, DWORD) = (void *)t_proc("K32GetWsChangesEx");
    if (!ini || !get || !getex) return 90;
    PSAPI_WS_WATCH_INFORMATION w[16];
    /* before init: expect failure */
    SetLastError(0);
    BOOL pre = get(GetCurrentProcess(), w, sizeof w);
    DWORD preerr = GetLastError();
    printf("  NOTE wswatch before init: ret=%d err=%lu\n", pre, preerr);
    SetLastError(0);
    if (!ini(GetCurrentProcess())) return 91;
    if (!ini(GetCurrentProcess())) return 92;           /* second init also succeeds */
    SIZE_T len = 256 * 4096;
    BYTE *p = VirtualAlloc(NULL, len, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    for (SIZE_T i = 0; i < len; i += 4096) p[i] = 1;
    PSAPI_WS_WATCH_INFORMATION big[4096];
    memset(big, 0, sizeof big);
    SetLastError(0);
    if (!get(GetCurrentProcess(), big, sizeof big)) { printf("  NOTE GetWsChanges err=%lu\n", GetLastError()); return 93; }
    int hits = 0, total = 0;
    for (int i = 0; i < 4096 && big[i].FaultingPc; i++) {
        total++;
        ULONG_PTR va = (ULONG_PTR)big[i].FaultingVa & ~(ULONG_PTR)0xFFF;
        if (va >= (ULONG_PTR)p && va < (ULONG_PTR)p + len) hits++;
    }
    printf("  NOTE wswatch entries=%d in-region=%d\n", total, hits);
    if (hits < 200) return 94;
    /* buffer is drained: immediately asking again with a tiny window */
    SetLastError(0);
    BOOL again = get(GetCurrentProcess(), w, sizeof w);
    printf("  NOTE wswatch second call: ret=%d err=%lu\n", again, GetLastError());
    /* Ex variant */
    for (SIZE_T i = 0; i < len; i += 4096) p[i + 1] = 2;
    VirtualFree(p, 0, MEM_DECOMMIT);
    VirtualAlloc(p, len, MEM_COMMIT, PAGE_READWRITE);
    for (SIZE_T i = 0; i < len; i += 4096) p[i] = 3;
    static PSAPI_WS_WATCH_INFORMATION_EX ex[4096];
    SetLastError(0);
    if (!getex(GetCurrentProcess(), ex, sizeof ex)) { printf("  NOTE GetWsChangesEx err=%lu\n", GetLastError()); return 95; }
    int exhits = 0;
    for (int i = 0; i < 4096 && ex[i].BasicInfo.FaultingPc; i++) {
        ULONG_PTR va = (ULONG_PTR)ex[i].BasicInfo.FaultingVa & ~(ULONG_PTR)0xFFF;
        if (va >= (ULONG_PTR)p && va < (ULONG_PTR)p + len) exhits++;
    }
    printf("  NOTE wswatch ex in-region=%d\n", exhits);
    if (exhits < 100) return 96;
    /* cb too small for one entry */
    SetLastError(0);
    BOOL tiny = get(GetCurrentProcess(), w, 4);
    printf("  NOTE tiny cb: ret=%d err=%lu\n", tiny, GetLastError());
    return 0;
}
TEST(ps_wswatch_self_in_child) {
    REQUIRE(U(K32InitializeProcessForWsWatch)); REQUIRE(U(K32GetWsChanges)); REQUIRE(U(K32GetWsChangesEx));
    DWORD c = t_run_child("thp_wswatch", "", 30000);
    CHECKF(c == 0, "child exit %lu", c);
}
TEST(ps_wswatch_invalid_handle) {
    REQUIRE(U(K32InitializeProcessForWsWatch)); REQUIRE(U(K32GetWsChanges));
    PSAPI_WS_WATCH_INFORMATION w[4];
    SET_ERR(0);
    CHECK(!U(K32InitializeProcessForWsWatch)(NULL));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    CHECK(!U(K32GetWsChanges)(NULL, w, sizeof w));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    SET_ERR(0);
    CHECK(!U(K32InitializeProcessForWsWatch)(h));
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(h);
}

/* ------- device drivers ------- */
TEST(ps_drivers_enum_basic) {
    if (t_is_wine()) { t_skip("wine fakes driver/pagefile data"); return; }
    REQUIRE(U(K32EnumDeviceDrivers));
    LPVOID b[2048]; DWORD need = 0xCC; SET_ERR(0);
    if (!U(K32EnumDeviceDrivers)(b, sizeof b, &need)) { t_note("EnumDeviceDrivers failed err=%lu", GetLastError()); return; }
    CHECK(need >= 8 * 20 && need % 8 == 0);
    CHECK(need < sizeof b);
    int n = need / 8;
    if (b[0] == NULL) { t_note("driver bases are zeroed (restricted caller)"); return; }
    CHECK((ULONG_PTR)b[0] >= 0xFFFF800000000000ull);
    for (int i = 0; i < n; i++) CHECKF((ULONG_PTR)b[i] >= 0xFFFF800000000000ull, "driver %d base %p", i, b[i]);
    for (int i = 1; i < n && i < 50; i++) CHECK(b[i] != b[0]);
}
TEST(ps_drivers_enum_cb_semantics) {
    if (t_is_wine()) { t_skip("wine fakes driver/pagefile data"); return; }
    REQUIRE(U(K32EnumDeviceDrivers));
    LPVOID full[2048], b[4]; DWORD nf = 0, need = 0xCC;
    if (!U(K32EnumDeviceDrivers)(full, sizeof full, &nf)) { t_note("failed err=%lu", GetLastError()); return; }
    for (int i = 0; i < 4; i++) b[i] = (LPVOID)0x1111;
    CHECK(U(K32EnumDeviceDrivers)(NULL, 0, &need));
    EXPECT_EQ(need, nf);
    need = 0xCC;
    CHECK(U(K32EnumDeviceDrivers)(b, 16, &need));
    EXPECT_EQ(need, nf);
    EXPECT_EQ(b[0], full[0]); EXPECT_EQ(b[1], full[1]);
    EXPECT_EQ(b[2], 0x1111);
    CHECK(U(K32EnumDeviceDrivers)(b, 15, &need));
    EXPECT_EQ(b[1], full[1]); EXPECT_EQ(b[2], 0x1111);
    CHECK(U(K32EnumDeviceDrivers)(b, 7, &need));
}
TEST(ps_drivers_names_ntoskrnl) {
    if (t_is_wine()) { t_skip("wine fakes driver/pagefile data"); return; }
    REQUIRE(U(K32EnumDeviceDrivers)); REQUIRE(U(K32GetDeviceDriverBaseNameA)); REQUIRE(U(K32GetDeviceDriverBaseNameW));
    REQUIRE(U(K32GetDeviceDriverFileNameA)); REQUIRE(U(K32GetDeviceDriverFileNameW));
    LPVOID b[2048]; DWORD need = 0;
    if (!U(K32EnumDeviceDrivers)(b, sizeof b, &need) || !b[0]) { t_skip("driver list unavailable"); return; }
    char a[MAX_PATH]; WCHAR w[MAX_PATH];
    SET_ERR(0);
    DWORD n = U(K32GetDeviceDriverBaseNameA)(b[0], a, MAX_PATH);
    CHECKF(n > 0, "err=%lu", GetLastError());
    EXPECT_EQ(n, strlen(a));
    CHECK(!lstrcmpiA(a, "ntoskrnl.exe"));
    DWORD nw = U(K32GetDeviceDriverBaseNameW)(b[0], w, MAX_PATH);
    EXPECT_EQ(nw, n);
    CHECK(!lstrcmpiW(w, L"ntoskrnl.exe"));
    n = U(K32GetDeviceDriverFileNameA)(b[0], a, MAX_PATH);
    CHECK(n > 0 && strstr(a, "ntoskrnl.exe") != NULL);
    t_note("ntoskrnl file name: %s", a);
    nw = U(K32GetDeviceDriverFileNameW)(b[0], w, MAX_PATH);
    EXPECT_EQ(nw, n);
    char c[MAX_PATH]; thp_w2a(w, c, MAX_PATH); CHECK(!lstrcmpiA(a, c));
}
TEST(ps_drivers_name_truncation_and_bad) {
    if (t_is_wine()) { t_skip("wine fakes driver/pagefile data"); return; }
    REQUIRE(U(K32EnumDeviceDrivers)); REQUIRE(U(K32GetDeviceDriverBaseNameA)); REQUIRE(U(K32GetDeviceDriverBaseNameW));
    LPVOID b[2048]; DWORD need = 0;
    if (!U(K32EnumDeviceDrivers)(b, sizeof b, &need) || !b[0]) { t_skip("driver list unavailable"); return; }
    char a[32]; WCHAR w[32];
    SET_ERR(0);
    CHECK(U(K32GetDeviceDriverBaseNameA)(b[0], a, 0) == 0);
    t_note("nSize=0 A err=%lu", GetLastError());
    for (DWORD ns = 1; ns <= 12; ns++) {
        memset(a, 0x77, sizeof a);
        DWORD r = U(K32GetDeviceDriverBaseNameA)(b[0], a, ns);
        CHECKF(r <= ns, "A ns=%lu r=%lu", ns, r);
        CHECKF(a[ns] == 0x77, "A ns=%lu overrun", ns);
        for (int i = 0; i < 32; i++) w[i] = 0x7777;
        r = U(K32GetDeviceDriverBaseNameW)(b[0], w, ns);
        CHECKF(r <= ns, "W ns=%lu r=%lu", ns, r);
        CHECKF(w[ns] == 0x7777, "W ns=%lu overrun", ns);
    }
    SET_ERR(0);
    CHECK(U(K32GetDeviceDriverBaseNameA)((LPVOID)0x12340000, a, sizeof a) == 0);
    t_note("bad base A err=%lu", GetLastError());
    SET_ERR(0);
    CHECK(U(K32GetDeviceDriverBaseNameW)((LPVOID)0x12340000, w, 32) == 0);
    CHECK(U(K32GetDeviceDriverBaseNameA)(NULL, a, sizeof a) == 0);
    CHECK(U(K32GetDeviceDriverFileNameA)(NULL, a, sizeof a) == 0);
    CHECK(U(K32GetDeviceDriverFileNameW)(NULL, w, 32) == 0);
}
TEST(ps_drivers_all_have_names) {
    if (t_is_wine()) { t_skip("wine fakes driver/pagefile data"); return; }
    REQUIRE(U(K32EnumDeviceDrivers)); REQUIRE(U(K32GetDeviceDriverBaseNameW));
    LPVOID b[2048]; DWORD need = 0;
    if (!U(K32EnumDeviceDrivers)(b, sizeof b, &need) || !b[0]) { t_skip("driver list unavailable"); return; }
    int missing = 0;
    for (DWORD i = 0; i < need / 8 && i < 400; i++) {
        WCHAR w[MAX_PATH];
        if (U(K32GetDeviceDriverBaseNameW)(b[i], w, MAX_PATH) == 0) missing++;
    }
    CHECKF(missing <= 2, "%d drivers without base name", missing);
}

/* ------- page files ------- */
typedef struct { int n; int bad; int unicode; } pf_ctx;
static BOOL CALLBACK pf_cbw(LPVOID ctx, PENUM_PAGE_FILE_INFORMATION i, LPCWSTR name) {
    pf_ctx *c = ctx; c->n++;
    if (i->cb != 0x20 || i->TotalInUse > i->TotalSize || i->PeakUsage < i->TotalInUse || i->TotalSize == 0) c->bad++;
    if (!name || name[1] != L':' || name[2] != L'\\') c->bad++;
    return TRUE;
}
static BOOL CALLBACK pf_cba(LPVOID ctx, PENUM_PAGE_FILE_INFORMATION i, LPCSTR name) {
    pf_ctx *c = ctx; c->n++;
    if (i->cb != 0x20 || i->TotalInUse > i->TotalSize) c->bad++;
    if (!name || name[1] != ':' || name[2] != '\\') c->bad++;
    return TRUE;
}
static BOOL CALLBACK pf_stopw(LPVOID ctx, PENUM_PAGE_FILE_INFORMATION i, LPCWSTR name) { ((pf_ctx *)ctx)->n++; return FALSE; }
TEST(ps_pagefiles_w_a) {
    if (t_is_wine()) { t_skip("wine fakes driver/pagefile data"); return; }
    REQUIRE(U(K32EnumPageFilesW)); REQUIRE(U(K32EnumPageFilesA));
    pf_ctx w = { 0 }, a = { 0 };
    SET_ERR(0);
    CHECK(U(K32EnumPageFilesW)(pf_cbw, &w));
    CHECK(U(K32EnumPageFilesA)(pf_cba, &a));
    t_note("page files: %d (W) %d (A)", w.n, a.n);
    EXPECT_EQ(w.n, a.n);
    CHECK(w.bad == 0 && a.bad == 0);
    PERFORMANCE_INFORMATION p;
    if (U(K32GetPerformanceInfo) && U(K32GetPerformanceInfo)(&p, sizeof p) && w.n > 0) CHECK(p.CommitLimit > p.PhysicalTotal / 2);
}
TEST(ps_pagefiles_callback_return_value) {
    if (t_is_wine()) { t_skip("wine fakes driver/pagefile data"); return; }
    REQUIRE(U(K32EnumPageFilesW));
    pf_ctx all = { 0 }, stop = { 0 };
    CHECK(U(K32EnumPageFilesW)(pf_cbw, &all));
    SET_ERR(0);
    BOOL r = U(K32EnumPageFilesW)(pf_stopw, &stop);
    t_note("returning FALSE from callback: ret=%d calls=%d (all=%d)", r, stop.n, all.n);
    CHECK(stop.n >= 0 && stop.n <= all.n + 1);
}
TEST(ps_pagefiles_context_passthrough) {
    if (t_is_wine()) { t_skip("wine fakes driver/pagefile data"); return; }
    REQUIRE(U(K32EnumPageFilesW));
    pf_ctx c = { 0 };
    CHECK(U(K32EnumPageFilesW)(pf_cbw, &c));
    CHECK(c.n >= 0);
}
