/* GetProcessMemoryInfo, GetPerformanceInfo, GetProcessImageFileName{A,W}, GetMappedFileName{A,W} */
#include "g5.h"

typedef BOOL (WINAPI *pmi_t)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD);
typedef BOOL (WINAPI *perf_t)(PPERFORMANCE_INFORMATION, DWORD);
typedef DWORD (WINAPI *imgW_t)(HANDLE, LPWSTR, DWORD);
typedef DWORD (WINAPI *imgA_t)(HANDLE, LPSTR, DWORD);
typedef DWORD (WINAPI *mapW_t)(HANDLE, LPVOID, LPWSTR, DWORD);
typedef DWORD (WINAPI *mapA_t)(HANDLE, LPVOID, LPSTR, DWORD);
#define PMI ((pmi_t)g5_proc("GetProcessMemoryInfo"))
#define PERF ((perf_t)g5_proc("GetPerformanceInfo"))
#define IMGW ((imgW_t)g5_proc("GetProcessImageFileNameW"))
#define IMGA ((imgA_t)g5_proc("GetProcessImageFileNameA"))
#define MAPW ((mapW_t)g5_proc("GetMappedFileNameW"))
#define MAPA ((mapA_t)g5_proc("GetMappedFileNameA"))

TEST(gap5_pm_exports) {
    static const char *n[] = { "GetProcessMemoryInfo", "GetPerformanceInfo", "GetProcessImageFileNameA", "GetProcessImageFileNameW",
        "GetMappedFileNameA", "GetMappedFileNameW" };
    for (unsigned i = 0; i < 6; i++) {
        CHECKF(g5_proc(n[i]) != NULL, "%s", n[i]);
        if (!t_is_wine()) { char k[64]; snprintf(k, sizeof k, "K32%s", n[i]);
            CHECKF(t_proc_kb(n[i]) != NULL, "kernelbase!%s", n[i]);
            CHECKF(t_proc_k32(k) != NULL, "kernel32!%s", k); }
    }
}

/* ============ GetProcessMemoryInfo ============ */
TEST(gap5_pmi_basic) {
    WINDOWS_ONLY();
    REQUIRE(PMI);
    PROCESS_MEMORY_COUNTERS c; memset(&c, 0xCC, sizeof c);
    SET_ERR(0);
    CHECK(PMI(GetCurrentProcess(), &c, sizeof c));
    EXPECT_EQ(sizeof c, 0x48);
    EXPECT_EQ(c.cb, 0x48);
    CHECK(c.PageFaultCount > 0);
    CHECK(c.WorkingSetSize > 0);
    CHECK(c.PeakWorkingSetSize >= c.WorkingSetSize);
    CHECK(c.PagefileUsage > 0);
    CHECK(c.PeakPagefileUsage >= c.PagefileUsage);
    CHECK(c.QuotaPeakPagedPoolUsage >= c.QuotaPagedPoolUsage);
    CHECK(c.QuotaPeakNonPagedPoolUsage >= c.QuotaNonPagedPoolUsage);
    CHECK(c.WorkingSetSize < ((SIZE_T)1 << 40));
}
TEST(gap5_pmi_ex_struct) {
    WINDOWS_ONLY();
    REQUIRE(PMI);
    PROCESS_MEMORY_COUNTERS_EX c; memset(&c, 0xCC, sizeof c);
    EXPECT_EQ(sizeof c, 0x50);
    CHECK(PMI(GetCurrentProcess(), (PPROCESS_MEMORY_COUNTERS)&c, sizeof c));
    EXPECT_EQ(c.cb, 0x50);
    CHECK(c.PrivateUsage > 0);
    if (c.PrivateUsage != c.PagefileUsage) t_note("PrivateUsage %zu != PagefileUsage %zu", (size_t)c.PrivateUsage, (size_t)c.PagefileUsage);
    CHECK(c.PrivateUsage < ((SIZE_T)1 << 40));
}
TEST(gap5_pmi_cb_boundaries) {
    WINDOWS_ONLY();
    REQUIRE(PMI);
    unsigned char buf[0x80];
    for (DWORD cb = 0; cb < 0x48; cb += (cb < 8 ? 1 : 0x10)) {
        memset(buf, 0xCC, sizeof buf);
        SET_ERR(0);
        CHECKF(!PMI(GetCurrentProcess(), (PPROCESS_MEMORY_COUNTERS)buf, cb), "cb=%lu", cb);
        EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
        CHECKF(buf[0] == 0xCC, "written on failure cb=%lu", cb);
    }
    memset(buf, 0xCC, sizeof buf); SET_ERR(0);
    CHECK(!PMI(GetCurrentProcess(), (PPROCESS_MEMORY_COUNTERS)buf, 0x47)); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    /* cb in [0x48,0x4f]: legacy layout, cb field becomes 0x48 and nothing past it is touched */
    for (DWORD cb = 0x48; cb < 0x50; cb++) {
        memset(buf, 0xCC, sizeof buf);
        CHECKF(PMI(GetCurrentProcess(), (PPROCESS_MEMORY_COUNTERS)buf, cb), "cb=%lu", cb);
        EXPECT_EQ(((PROCESS_MEMORY_COUNTERS *)buf)->cb, 0x48);
        EXPECT_EQ(buf[0x48], 0xCC);
    }
    for (DWORD cb = 0x50; cb <= 0x70; cb += 8) {
        memset(buf, 0xCC, sizeof buf);
        CHECKF(PMI(GetCurrentProcess(), (PPROCESS_MEMORY_COUNTERS)buf, cb), "cb=%lu", cb);
        EXPECT_EQ(((PROCESS_MEMORY_COUNTERS *)buf)->cb, 0x50);
        EXPECT_EQ(buf[0x50], 0xCC);
    }
    memset(buf, 0xCC, sizeof buf);
    CHECK(PMI(GetCurrentProcess(), (PPROCESS_MEMORY_COUNTERS)buf, 0xFFFFFFFF));
    EXPECT_EQ(((PROCESS_MEMORY_COUNTERS *)buf)->cb, 0x50);
}
TEST(gap5_pmi_null_buffer) {
    WINDOWS_ONLY();
    REQUIRE(PMI);
    SET_ERR(0); EXPECT_EQ(PMI(GetCurrentProcess(), NULL, 0x48), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(PMI(GetCurrentProcess(), NULL, 0), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);   /* null check precedes size check */
    SET_ERR(0); EXPECT_EQ(PMI(NULL, NULL, 0x48), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap5_pmi_bad_handles) {
    REQUIRE(PMI);
    PROCESS_MEMORY_COUNTERS c;
    SET_ERR(0); EXPECT_EQ(PMI(NULL, &c, sizeof c), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0); EXPECT_EQ(PMI(e, &c, sizeof c), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(e);
    SET_ERR(0); EXPECT_EQ(PMI(e, &c, sizeof c), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(PMI((HANDLE)(ULONG_PTR)0xFFFFFF00, &c, sizeof c), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE t = GetCurrentThread();
    SET_ERR(0); EXPECT_EQ(PMI(t, &c, sizeof c), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap5_pmi_access_rights) {
    REQUIRE(PMI);
    PROCESS_MEMORY_COUNTERS c;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    if (h) { memset(&c, 0, sizeof c); CHECK(PMI(h, &c, sizeof c)); CHECK(c.WorkingSetSize > 0); CloseHandle(h); }
    h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, GetCurrentProcessId());
    if (h) { CHECK(PMI(h, &c, sizeof c)); CloseHandle(h); }
    h = OpenProcess(SYNCHRONIZE, FALSE, GetCurrentProcessId());
    if (h) { SET_ERR(0); EXPECT_EQ(PMI(h, &c, sizeof c), 0); EXPECT_ERR(ERROR_ACCESS_DENIED); CloseHandle(h); }
    h = OpenProcess(PROCESS_VM_READ, FALSE, GetCurrentProcessId());
    if (h) { SET_ERR(0); EXPECT_EQ(PMI(h, &c, sizeof c), 0); EXPECT_ERR(ERROR_ACCESS_DENIED); CloseHandle(h); }
}
TEST(gap5_pmi_pseudo_and_real_agree) {
    REQUIRE(PMI);
    PROCESS_MEMORY_COUNTERS a, b;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    if (!h) { t_skip("open failed"); return; }
    CHECK(PMI(GetCurrentProcess(), &a, sizeof a));
    CHECK(PMI(h, &b, sizeof b));
    CHECK(b.PeakWorkingSetSize >= a.PeakWorkingSetSize);
    CHECK(PMI((HANDLE)-1, &a, sizeof a));
    CloseHandle(h);
}
TEST(gap5_pmi_tracks_allocation) {
    WINDOWS_ONLY();
    REQUIRE(PMI);
    PROCESS_MEMORY_COUNTERS_EX a, b, d;
    CHECK(PMI(GetCurrentProcess(), (PPROCESS_MEMORY_COUNTERS)&a, sizeof a));
    const SIZE_T sz = 16 << 20;
    char *p = VirtualAlloc(NULL, sz, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!p) { t_skip("alloc failed"); return; }
    CHECK(PMI(GetCurrentProcess(), (PPROCESS_MEMORY_COUNTERS)&b, sizeof b));
    CHECK(b.PrivateUsage >= a.PrivateUsage + sz - (1 << 20));       /* commit is charged immediately */
    for (SIZE_T i = 0; i < sz; i += 4096) p[i] = 1;
    CHECK(PMI(GetCurrentProcess(), (PPROCESS_MEMORY_COUNTERS)&d, sizeof d));
    if (d.PageFaultCount < b.PageFaultCount + (sz / 4096) / 2) t_note("page faults grew less than expected: %lu -> %lu", (unsigned long)b.PageFaultCount, (unsigned long)d.PageFaultCount);
    if (d.WorkingSetSize < b.WorkingSetSize + sz / 2) t_note("working set grew less than expected: %zu -> %zu", (size_t)b.WorkingSetSize, (size_t)d.WorkingSetSize);
    CHECK(d.PeakPagefileUsage >= b.PrivateUsage);
    VirtualFree(p, 0, MEM_RELEASE);
    PROCESS_MEMORY_COUNTERS_EX e;
    CHECK(PMI(GetCurrentProcess(), (PPROCESS_MEMORY_COUNTERS)&e, sizeof e));
    CHECK(e.PrivateUsage + sz / 2 <= d.PrivateUsage);               /* decommitted */
    CHECK(e.PeakPagefileUsage >= d.PeakPagefileUsage);              /* peaks do not shrink */
}
TEST(gap5_pmi_child_process) {
    WINDOWS_ONLY();
    REQUIRE(PMI);
    HANDLE ready, quit; DWORD pid;
    HANDLE c = g5_spawn_waiter(&ready, &quit, &pid);
    if (!c) { t_skip("child spawn failed"); return; }
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    CHECK(h != NULL);
    if (h) {
        PROCESS_MEMORY_COUNTERS_EX m; memset(&m, 0, sizeof m);
        CHECK(PMI(h, (PPROCESS_MEMORY_COUNTERS)&m, sizeof m));
        CHECK(m.WorkingSetSize > 0 && m.PrivateUsage > 0);
        CHECK(m.WorkingSetSize < (256u << 20));
        CloseHandle(h);
    }
    g5_end_waiter(c, ready, quit);
}
TEST(gap5_pmi_exited_process) {
    REQUIRE(PMI);
    HANDLE ready, quit; DWORD pid;
    HANDLE c = g5_spawn_waiter(&ready, &quit, &pid);
    if (!c) { t_skip("child spawn failed"); return; }
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid);
    g5_end_waiter(c, ready, quit);
    if (!h) { t_skip("open failed"); return; }
    WaitForSingleObject(h, 5000);
    PROCESS_MEMORY_COUNTERS m; SET_ERR(0);
    BOOL r = PMI(h, &m, sizeof m);
    t_note("exited process: ret=%d err=%lu ws=%zu", r, GetLastError(), (size_t)m.WorkingSetSize);
    CloseHandle(h);
}
TEST(gap5_pmi_repeated_monotonic_peaks) {
    REQUIRE(PMI);
    PROCESS_MEMORY_COUNTERS prev, cur; memset(&prev, 0, sizeof prev);
    for (int i = 0; i < 50; i++) {
        CHECK(PMI(GetCurrentProcess(), &cur, sizeof cur));
        CHECK(cur.PeakWorkingSetSize >= prev.PeakWorkingSetSize);
        CHECK(cur.PeakPagefileUsage >= prev.PeakPagefileUsage);
        CHECK(cur.PageFaultCount >= prev.PageFaultCount);
        prev = cur;
    }
}

/* ============ GetPerformanceInfo ============ */
TEST(gap5_perf_basic) {
    WINDOWS_ONLY();
    REQUIRE(PERF);
    PERFORMANCE_INFORMATION p; memset(&p, 0xCC, sizeof p);
    EXPECT_EQ(sizeof p, 0x68);
    SET_ERR(0);
    CHECK(PERF(&p, sizeof p));
    EXPECT_EQ(p.cb, 0x68);
    EXPECT_EQ(p.PageSize, 4096);
    CHECK(p.PhysicalTotal > 0);
    CHECK(p.PhysicalAvailable > 0 && p.PhysicalAvailable <= p.PhysicalTotal);
    CHECK(p.CommitLimit >= p.CommitTotal);
    CHECK(p.CommitPeak >= p.CommitTotal);
    CHECK(p.CommitTotal > 0);
    if (p.SystemCache > p.PhysicalTotal) t_note("SystemCache %zu > PhysicalTotal %zu", (size_t)p.SystemCache, (size_t)p.PhysicalTotal);
    EXPECT_EQ(p.KernelTotal, p.KernelPaged + p.KernelNonpaged);
    CHECK(p.ProcessCount >= 2);
    CHECK(p.ThreadCount >= p.ProcessCount);
    CHECK(p.HandleCount >= 10);
}
TEST(gap5_perf_cb_boundaries) {
    REQUIRE(PERF);
    unsigned char buf[0x100];
    DWORD bad[] = { 0, 1, 4, 0x18, 0x60, 0x67 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        memset(buf, 0xCC, sizeof buf); SET_ERR(0);
        CHECKF(!PERF((PPERFORMANCE_INFORMATION)buf, bad[i]), "cb=0x%lx", bad[i]);
        EXPECT_ERR(ERROR_BAD_LENGTH);
        CHECKF(buf[0] == 0xCC, "touched cb=0x%lx", bad[i]);
    }
    DWORD good[] = { 0x68, 0x69, 0x70, 0x100, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof good / sizeof *good; i++) {
        memset(buf, 0xCC, sizeof buf); SET_ERR(0);
        CHECKF(PERF((PPERFORMANCE_INFORMATION)buf, good[i]), "cb=0x%lx", good[i]);
        EXPECT_EQ(((PERFORMANCE_INFORMATION *)buf)->cb, 0x68);
        EXPECT_EQ(buf[0x68], 0xCC);
    }
}
TEST(gap5_perf_counts_move_with_activity) {
    REQUIRE(PERF);
    PERFORMANCE_INFORMATION a, b;
    CHECK(PERF(&a, sizeof a));
    HANDLE hs[64]; int n = 0;
    for (int i = 0; i < 64; i++) { hs[i] = CreateEventA(NULL, FALSE, FALSE, NULL); if (hs[i]) n++; }
    CHECK(PERF(&b, sizeof b));
    /* system-wide counters are noisy; only a loose lower bound on the handle delta applies */
    CHECK(b.HandleCount + 200 >= a.HandleCount + n);
    for (int i = 0; i < 64; i++) if (hs[i]) CloseHandle(hs[i]);
}
TEST(gap5_perf_vs_globalmemorystatus) {
    REQUIRE(PERF);
    PERFORMANCE_INFORMATION p; MEMORYSTATUSEX m; m.dwLength = sizeof m;
    CHECK(PERF(&p, sizeof p)); CHECK(GlobalMemoryStatusEx(&m));
    unsigned long long phys = (unsigned long long)p.PhysicalTotal * p.PageSize;
    CHECK(phys >= m.ullTotalPhys / 2 && phys <= m.ullTotalPhys * 2);
    t_note("PhysicalTotal=%llu bytes, GlobalMemoryStatusEx.TotalPhys=%llu", phys, (unsigned long long)m.ullTotalPhys);
}
TEST(gap5_perf_k32_alias) {
    if (t_is_wine()) { t_skip("alias check is windows-specific"); return; }
    perf_t k = (perf_t)t_proc_k32("K32GetPerformanceInfo");
    REQUIRE(k);
    PERFORMANCE_INFORMATION p;
    CHECK(k(&p, sizeof p));
    EXPECT_EQ(p.cb, 0x68);
    SET_ERR(0); CHECK(!k(&p, 0x10)); EXPECT_ERR(ERROR_BAD_LENGTH);
}

/* ============ GetProcessImageFileName ============ */
static int ends_with_ci(const char *s, const char *suffix) {
    size_t a = strlen(s), b = strlen(suffix);
    return a >= b && !lstrcmpiA(s + a - b, suffix);
}
TEST(gap5_imgname_self) {
    REQUIRE(IMGW); REQUIRE(IMGA);
    char self[MAX_PATH]; GetModuleFileNameA(NULL, self, sizeof self);
    WCHAR w[MAX_PATH * 2]; char a[MAX_PATH * 2], b[MAX_PATH * 2];
    SET_ERR(0);
    DWORD n = IMGW(GetCurrentProcess(), w, MAX_PATH * 2);
    CHECK(n > 0);
    EXPECT_EQ(w[n], 0);
    g5_w2a(w, a, sizeof a);
    CHECKF(strlen(a) == n, "len %zu n %lu", strlen(a), n);
    CHECKF(!strncmp(a, "\\Device\\", 8), "not a device path: %s", a);
    { char suf[MAX_PATH + 2]; snprintf(suf, sizeof suf, "\\%s", g5_base(self));      /* compare the leaf: the DOS path may carry 8.3 components */
      CHECKF(ends_with_ci(a, suf), "device path %s vs dos path %s", a, self); }
    DWORD m = IMGA(GetCurrentProcess(), b, sizeof b);
    EXPECT_EQ(m, n);
    CHECK(!strcmp(a, b));
    n = IMGW((HANDLE)-1, w, MAX_PATH * 2); CHECK(n > 0);
}
TEST(gap5_imgname_buffer_sizes) {
    REQUIRE(IMGW); REQUIRE(IMGA);
    WCHAR full[MAX_PATH * 2]; DWORD len = IMGW(GetCurrentProcess(), full, MAX_PATH * 2);
    CHECK(len > 0);
    WCHAR w[MAX_PATH * 2]; char a[MAX_PATH * 2];
    for (int i = 0; i < MAX_PATH * 2; i++) w[i] = 0x7777;
    SET_ERR(0);
    DWORD n = IMGW(GetCurrentProcess(), w, len + 1);
    EXPECT_EQ(n, len); EXPECT_EQ(w[len], 0); EXPECT_EQ(w[len + 1], 0x7777);
    for (int i = 0; i < MAX_PATH * 2; i++) w[i] = 0x7777;
    n = IMGW(GetCurrentProcess(), w, len + 100);
    EXPECT_EQ(n, len); EXPECT_EQ(w[len], 0);
    /* too-small buffers fail with ERROR_INSUFFICIENT_BUFFER and return 0 */
    DWORD small[] = { 0, 1, 2, len / 2, len - 1 };
    for (unsigned i = 0; i < sizeof small / sizeof *small; i++) {
        memset(w, 0x77, 64);
        SET_ERR(0);
        n = IMGW(GetCurrentProcess(), w, small[i]);
        if (small[i] + 1 < len) { CHECKF(n == 0, "nSize=%lu ret %lu", small[i], n); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER); }
        else t_note("nSize=%lu (len %lu) -> ret %lu err %lu", small[i], len, n, GetLastError());
        SET_ERR(0);
        DWORD na = IMGA(GetCurrentProcess(), a, small[i]);
        if (small[i] + 1 < len) { CHECKF(na == 0, "A nSize=%lu ret %lu", small[i], na); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER); }
    }
    memset(a, 0x77, sizeof a);
    n = IMGA(GetCurrentProcess(), a, len + 1);
    EXPECT_EQ(n, len); EXPECT_EQ(a[len], 0); EXPECT_EQ((unsigned char)a[len + 1], 0x77);
    memset(a, 0x77, sizeof a);
    n = IMGA(GetCurrentProcess(), a, len);
    t_note("A nSize==len -> ret %lu a[len-1]=%d", n, a[len - 1]);
}
TEST(gap5_imgname_bad_handles) {
    REQUIRE(IMGW); REQUIRE(IMGA);
    WCHAR w[MAX_PATH]; char a[MAX_PATH];
    SET_ERR(0); EXPECT_EQ(IMGW(NULL, w, MAX_PATH), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(IMGA(NULL, a, MAX_PATH), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0); EXPECT_EQ(IMGW(e, w, MAX_PATH), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(IMGA(e, a, MAX_PATH), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(e);
    SET_ERR(0); EXPECT_EQ(IMGW(e, w, MAX_PATH), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(IMGW(GetCurrentThread(), w, MAX_PATH), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap5_imgname_access_rights) {
    REQUIRE(IMGW);
    WCHAR w[MAX_PATH];
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    if (h) { CHECK(IMGW(h, w, MAX_PATH) > 0); CloseHandle(h); }
    h = OpenProcess(SYNCHRONIZE, FALSE, GetCurrentProcessId());
    if (h) { SET_ERR(0); EXPECT_EQ(IMGW(h, w, MAX_PATH), 0); EXPECT_ERR(ERROR_ACCESS_DENIED); CloseHandle(h); }
    h = OpenProcess(PROCESS_VM_READ, FALSE, GetCurrentProcessId());
    if (h) { SET_ERR(0); EXPECT_EQ(IMGW(h, w, MAX_PATH), 0); EXPECT_ERR(ERROR_ACCESS_DENIED); CloseHandle(h); }
}
TEST(gap5_imgname_child_and_exited) {
    REQUIRE(IMGW); REQUIRE(IMGA);
    HANDLE ready, quit; DWORD pid;
    HANDLE c = g5_spawn_waiter(&ready, &quit, &pid);
    if (!c) { t_skip("child spawn failed"); return; }
    WCHAR mine[MAX_PATH * 2], theirs[MAX_PATH * 2];
    DWORD n1 = IMGW(GetCurrentProcess(), mine, MAX_PATH * 2);
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid);
    CHECK(h != NULL);
    if (h) {
        DWORD n2 = IMGW(h, theirs, MAX_PATH * 2);
        EXPECT_EQ(n1, n2);
        CHECK(!lstrcmpW(mine, theirs));
        g5_end_waiter(c, ready, quit);
        WaitForSingleObject(h, 5000);
        SET_ERR(0);
        DWORD n3 = IMGW(h, theirs, MAX_PATH * 2);
        t_note("image name of exited process: ret %lu err %lu", n3, GetLastError());
        CloseHandle(h);
    } else g5_end_waiter(c, ready, quit);
}

/* ============ GetMappedFileName ============ */
TEST(gap5_mapped_exe_image) {
    WINDOWS_ONLY();
    REQUIRE(MAPW); REQUIRE(MAPA); REQUIRE(IMGW);
    WCHAR w[MAX_PATH * 2], img[MAX_PATH * 2]; char a[MAX_PATH * 2], ia[MAX_PATH * 2];
    void *base = GetModuleHandleA(NULL);
    SET_ERR(0xBEEF);
    DWORD n = MAPW(GetCurrentProcess(), base, w, MAX_PATH * 2);
    CHECK(n > 0);
    EXPECT_ERR(0);                       /* success explicitly clears the last error */
    EXPECT_EQ(w[n], 0);
    DWORD m = IMGW(GetCurrentProcess(), img, MAX_PATH * 2);
    EXPECT_EQ(n, m);
    CHECK(!lstrcmpiW(w, img));           /* same device path as the image file name */
    DWORD na = MAPA(GetCurrentProcess(), base, a, sizeof a);
    EXPECT_EQ(na, n);
    g5_w2a(img, ia, sizeof ia); CHECK(!lstrcmpiA(a, ia));
    /* any address within the image resolves to the same name */
    n = MAPW(GetCurrentProcess(), (char *)base + 0x1000, w, MAX_PATH * 2);
    EXPECT_EQ(n, m); CHECK(!lstrcmpiW(w, img));
    n = MAPW((HANDLE)-1, (char *)base + 1, w, MAX_PATH * 2);
    EXPECT_EQ(n, m);
}
TEST(gap5_mapped_system_dll) {
    REQUIRE(MAPA);
    HMODULE k = GetModuleHandleA("kernel32.dll");
    char a[MAX_PATH * 2];
    DWORD n = MAPA(GetCurrentProcess(), k, a, sizeof a);
    CHECK(n > 0);
    CHECKF(ends_with_ci(a, "\\System32\\kernel32.dll") || ends_with_ci(a, "\\SysWOW64\\kernel32.dll") || t_is_wine(), "%s", a);
    CHECK(!strncmp(a, "\\Device\\", 8) || t_is_wine());
}
TEST(gap5_mapped_buffer_sizes_w) {
    WINDOWS_ONLY();
    REQUIRE(MAPW);
    WCHAR full[MAX_PATH * 2]; void *base = GetModuleHandleA(NULL);
    DWORD len = MAPW(GetCurrentProcess(), base, full, MAX_PATH * 2);
    CHECK(len > 0);
    WCHAR w[MAX_PATH * 2];
    for (int i = 0; i < MAX_PATH * 2; i++) w[i] = 0x7777;
    SET_ERR(0);
    DWORD n = MAPW(GetCurrentProcess(), base, w, len + 1);
    EXPECT_EQ(n, len); EXPECT_ERR(0); EXPECT_EQ(w[len], 0); EXPECT_EQ(w[len + 1], 0x7777);
    for (int i = 0; i < MAX_PATH * 2; i++) w[i] = 0x7777;
    SET_ERR(0);
    n = MAPW(GetCurrentProcess(), base, w, len);          /* truncated */
    EXPECT_EQ(n, len); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(w[len - 1], 0); EXPECT_EQ(w[len], 0x7777);
    CHECK(!wcsncmp(w, full, len - 1));
    for (int i = 0; i < 8; i++) w[i] = 0x7777;
    SET_ERR(0);
    n = MAPW(GetCurrentProcess(), base, w, 1);
    EXPECT_EQ(n, 1); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER); EXPECT_EQ(w[0], 0); EXPECT_EQ(w[1], 0x7777);
    for (int i = 0; i < 8; i++) w[i] = 0x7777;
    SET_ERR(0);
    n = MAPW(GetCurrentProcess(), base, w, 0);
    EXPECT_EQ(n, 0); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER); EXPECT_EQ(w[0], 0x7777);
}
TEST(gap5_mapped_buffer_sizes_a) {
    REQUIRE(MAPA);
    char full[MAX_PATH * 2]; void *base = GetModuleHandleA(NULL);
    DWORD len = MAPA(GetCurrentProcess(), base, full, sizeof full);
    CHECK(len > 0);
    char a[MAX_PATH * 2];
    memset(a, 0x77, sizeof a);
    DWORD n = MAPA(GetCurrentProcess(), base, a, len + 1);
    EXPECT_EQ(n, len); EXPECT_EQ(a[len], 0); EXPECT_EQ((unsigned char)a[len + 1], 0x77);
    memset(a, 0x77, sizeof a);
    SET_ERR(0);
    n = MAPA(GetCurrentProcess(), base, a, len);
    EXPECT_EQ(n, len); EXPECT_EQ(a[len - 1], 0);
    CHECK(!strncmp(a, full, len - 1));
    memset(a, 0x77, sizeof a);
    SET_ERR(0);
    EXPECT_EQ(MAPA(GetCurrentProcess(), base, a, 0), 0);
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ((unsigned char)a[0], 0x77);
}
TEST(gap5_mapped_data_file_view) {
    WINDOWS_ONLY();
    REQUIRE(MAPW); REQUIRE(MAPA);
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "gap5_map.bin");
    HANDLE f = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) { t_skip("create failed"); return; }
    DWORD w; char z[8192] = { 1 }; WriteFile(f, z, sizeof z, &w, NULL);
    HANDLE m = CreateFileMappingA(f, NULL, PAGE_READWRITE, 0, 0, NULL);
    char *v = m ? MapViewOfFile(m, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0) : NULL;
    if (!v) { t_skip("mapping failed"); CloseHandle(f); if (m) CloseHandle(m); DeleteFileA(p); return; }
    char a[MAX_PATH * 2]; WCHAR wn[MAX_PATH * 2];
    SET_ERR(0);
    DWORD n = MAPA(GetCurrentProcess(), v, a, sizeof a);
    CHECK(n > 0);
    { char suf[MAX_PATH + 2]; snprintf(suf, sizeof suf, "\\%s", g5_base(p));          /* leaf only: %TEMP% may be an 8.3 path */
      CHECKF(ends_with_ci(a, suf), "%s vs %s", a, p); }
    CHECK(!strncmp(a, "\\Device\\", 8));
    DWORD n2 = MAPA(GetCurrentProcess(), v + 4096, a, sizeof a);       /* second page of the view */
    EXPECT_EQ(n2, n);
    n2 = MAPW(GetCurrentProcess(), v + 8191, wn, MAX_PATH * 2);
    EXPECT_EQ(n2, n);
    n2 = MAPW(GetCurrentProcess(), v, wn, MAX_PATH * 2);
    EXPECT_EQ(n2, n);
    /* past the view */
    SET_ERR(0);
    n2 = MAPW(GetCurrentProcess(), v + 8192, wn, MAX_PATH * 2);
    t_note("one page past a 2-page view: ret=%lu err=%lu", n2, GetLastError());
    UnmapViewOfFile(v);
    SET_ERR(0);
    EXPECT_EQ(MAPW(GetCurrentProcess(), v, wn, MAX_PATH * 2), 0);
    t_note("after unmap: err=%lu", GetLastError());
    CloseHandle(m); CloseHandle(f); DeleteFileA(p);
}
TEST(gap5_mapped_not_a_file_mapping) {
    REQUIRE(MAPW);
    WCHAR w[MAX_PATH]; 
    char *priv = VirtualAlloc(NULL, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (priv) {
        SET_ERR(0); EXPECT_EQ(MAPW(GetCurrentProcess(), priv, w, MAX_PATH), 0);
        t_note("private memory: err=%lu", GetLastError());
        VirtualFree(priv, 0, MEM_RELEASE);
    }
    int local = 0;
    SET_ERR(0); EXPECT_EQ(MAPW(GetCurrentProcess(), &local, w, MAX_PATH), 0);             /* stack */
    HANDLE m = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 8192, NULL);   /* pagefile-backed */
    char *v = m ? MapViewOfFile(m, FILE_MAP_WRITE, 0, 0, 0) : NULL;
    if (v) {
        SET_ERR(0); EXPECT_EQ(MAPW(GetCurrentProcess(), v, w, MAX_PATH), 0);
        t_note("pagefile-backed section: err=%lu", GetLastError());
        UnmapViewOfFile(v);
    }
    if (m) CloseHandle(m);
    /* reserved-but-uncommitted, and free address space */
    char *r = VirtualAlloc(NULL, 65536, MEM_RESERVE, PAGE_NOACCESS);
    if (r) { SET_ERR(0); EXPECT_EQ(MAPW(GetCurrentProcess(), r, w, MAX_PATH), 0); VirtualFree(r, 0, MEM_RELEASE); }
    SET_ERR(0); EXPECT_EQ(MAPW(GetCurrentProcess(), NULL, w, MAX_PATH), 0);
    t_note("NULL address: err=%lu", GetLastError());
}
TEST(gap5_mapped_bad_process) {
    REQUIRE(MAPW); REQUIRE(MAPA);
    WCHAR w[MAX_PATH]; char a[MAX_PATH]; void *base = GetModuleHandleA(NULL);
    SET_ERR(0); EXPECT_EQ(MAPW(NULL, base, w, MAX_PATH), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(MAPA(NULL, base, a, MAX_PATH), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0); EXPECT_EQ(MAPW(e, base, w, MAX_PATH), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(e);
    SET_ERR(0); EXPECT_EQ(MAPW(e, base, w, MAX_PATH), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    if (h) { SET_ERR(0); EXPECT_EQ(MAPW(h, base, w, MAX_PATH), 0); EXPECT_ERR(ERROR_ACCESS_DENIED); CloseHandle(h); }
    h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, GetCurrentProcessId());
    if (h) { CHECK(MAPW(h, base, w, MAX_PATH) > 0); CloseHandle(h); }
}
TEST(gap5_mapped_child_process) {
    WINDOWS_ONLY();
    REQUIRE(MAPW); REQUIRE(IMGW);
    HANDLE ready, quit; DWORD pid;
    HANDLE c = g5_spawn_waiter(&ready, &quit, &pid);
    if (!c) { t_skip("child spawn failed"); return; }
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    CHECK(h != NULL);
    if (h) {
        WCHAR w[MAX_PATH * 2], img[MAX_PATH * 2];
        /* ntdll and the exe are mapped at the same base in a child spawned from us only if ASLR collides; use ntdll (system-wide base) */
        void *nt = GetModuleHandleA("ntdll.dll");
        DWORD n = MAPW(h, nt, w, MAX_PATH * 2);
        CHECK(n > 0);
        char a[MAX_PATH * 2]; g5_w2a(w, a, sizeof a);
        CHECKF(ends_with_ci(a, "\\System32\\ntdll.dll") || t_is_wine(), "%s", a);
        n = MAPW(h, nt, img, 0);
        EXPECT_EQ(n, 0);
        CloseHandle(h);
    }
    g5_end_waiter(c, ready, quit);
}
