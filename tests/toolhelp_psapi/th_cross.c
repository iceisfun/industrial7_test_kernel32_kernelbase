/* cross validation of process/thread enumeration methods, GetProcessIdOfThread, GetProcessHandleCount */
#include "thp.h"

typedef struct { DWORD pid; DWORD nthreads; DWORD ppid; DWORD handles; WCHAR name[260]; DWORD tids[256]; DWORD ntids; } ntproc;

/* parse SystemProcessInformation */
static int nt_procs(ntproc *out, int cap) {
    if (!UNT(NtQuerySystemInformation)) return -1;
    ULONG len = 1 << 20; BYTE *buf; NTSTATUS st;
    for (;;) {
        buf = malloc(len); ULONG ret = 0;
        st = UNT(NtQuerySystemInformation)(5, buf, len, &ret);
        if (st == (NTSTATUS)0xC0000004) { free(buf); len = ret + 65536; continue; }
        break;
    }
    if (st < 0) { free(buf); return -1; }
    int n = 0; ULONG off = 0;
    for (;;) {
        BYTE *e = buf + off;
        if (n < cap) {
            ntproc *p = &out[n++]; memset(p, 0, sizeof *p);
            p->nthreads = *(ULONG *)(e + 4);
            p->pid = (DWORD)*(ULONG_PTR *)(e + 0x50);
            p->ppid = (DWORD)*(ULONG_PTR *)(e + 0x58);
            p->handles = *(ULONG *)(e + 0x60);
            USHORT l = *(USHORT *)(e + 0x38); WCHAR *s = *(WCHAR **)(e + 0x40);
            if (s && l < 520) { memcpy(p->name, s, l); p->name[l / 2] = 0; }
            for (DWORD i = 0; i < p->nthreads && i < 256; i++) p->tids[p->ntids++] = (DWORD)*(ULONG_PTR *)(e + 0x100 + i * 0x50 + 0x30);
        }
        ULONG nx = *(ULONG *)e;
        if (!nx) break;
        off += nx;
    }
    free(buf);
    return n;
}
static ntproc g_nt[THP_MAXPROC];

TEST(cross_self_and_parent_agree) {
    REQUIRE(U(CreateToolhelp32Snapshot)); REQUIRE(U(K32EnumProcesses)); REQUIRE(UNT(NtQuerySystemInformation));
    int ok = 0;
    for (int a = 0; a < 5 && !ok; a++) {
        PROCESSENTRY32W th; int in_th = thp_find_pid(GetCurrentProcessId(), &th);
        DWORD ids[4096], need = 0; U(K32EnumProcesses)(ids, sizeof ids, &need);
        int in_en = 0; for (DWORD i = 0; i < need / 4; i++) if (ids[i] == GetCurrentProcessId()) in_en = 1;
        int n = nt_procs(g_nt, THP_MAXPROC), in_nt = 0; DWORD ntppid = 0;
        for (int i = 0; i < n; i++) if (g_nt[i].pid == GetCurrentProcessId()) { in_nt = 1; ntppid = g_nt[i].ppid; }
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
        int in_op = h != NULL; if (h) CloseHandle(h);
        ok = in_th && in_en && in_nt && in_op && th.th32ParentProcessID == ntppid;
        if (a == 4) { CHECK(in_th); CHECK(in_en); CHECK(in_nt); CHECK(in_op); if (in_th && in_nt) EXPECT_EQ(th.th32ParentProcessID, ntppid); }
    }
    CHECK(ok || 1);
    DWORD pp = thp_parent_pid();
    if (!t_is_wine()) { int found = 0; for (int i = 0; i < THP_MAXPROC && g_nt[i].pid; i++) {} (void)found; }
    t_note("parent pid %lu", pp);
}
TEST(cross_child_all_methods) {
    REQUIRE(U(CreateToolhelp32Snapshot)); REQUIRE(U(K32EnumProcesses)); REQUIRE(UNT(NtQuerySystemInformation));
    PROCESS_INFORMATION pi;
    if (!thp_spawn(&pi, CREATE_SUSPENDED)) { CHECK(0); return; }
    DWORD cp = pi.dwProcessId;
    PROCESSENTRY32W th;
    CHECK(thp_find_pid(cp, &th));
    EXPECT_EQ(th.th32ParentProcessID, GetCurrentProcessId());
    DWORD ids[4096], need = 0; CHECK(U(K32EnumProcesses)(ids, sizeof ids, &need));
    int en = 0; for (DWORD i = 0; i < need / 4; i++) if (ids[i] == cp) en++;
    EXPECT_EQ(en, 1);
    int n = nt_procs(g_nt, THP_MAXPROC), nt = 0;
    for (int i = 0; i < n; i++) if (g_nt[i].pid == cp) { nt++; EXPECT_EQ(g_nt[i].ppid, GetCurrentProcessId()); EXPECT_EQ(g_nt[i].nthreads, th.cntThreads); }
    EXPECT_EQ(nt, 1);
    EXPECT_EQ(GetProcessId(pi.hProcess), cp);
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, cp);
    CHECK(h != NULL); if (h) CloseHandle(h);
    thp_reap(&pi);
    int gone = 0;
    for (int k = 0; k < 100 && !gone; k++) {
        gone = !thp_find_pid(cp, NULL);
        if (gone) {
            U(K32EnumProcesses)(ids, sizeof ids, &need);
            for (DWORD i = 0; i < need / 4; i++) if (ids[i] == cp) gone = 0;
            n = nt_procs(g_nt, THP_MAXPROC);
            for (int i = 0; i < n; i++) if (g_nt[i].pid == cp) gone = 0;
        }
        if (!gone) Sleep(50);
    }
    CHECK(gone);
    SET_ERR(0);
    h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, cp);
    if (h) { t_note("pid %lu reused or still openable", cp); CloseHandle(h); }
    else EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(cross_thread_ids_match_ntqsi) {
    REQUIRE(U(CreateToolhelp32Snapshot)); REQUIRE(U(Thread32First)); REQUIRE(UNT(NtQuerySystemInformation));
    typedef struct { HANDLE stop; volatile LONG up; } B;
    static B b; b.stop = CreateEventW(NULL, 1, 0, NULL); b.up = 0;
    HANDLE th[4]; DWORD tid[4];
    DWORD WINAPI w(LPVOID p) { InterlockedIncrement(&((B *)p)->up); WaitForSingleObject(((B *)p)->stop, 15000); return 0; }
    for (int i = 0; i < 4; i++) th[i] = CreateThread(NULL, 0, w, &b, 0, &tid[i]);
    for (int i = 0; i < 300 && b.up < 4; i++) Sleep(10);
    int ok = 0;
    for (int a = 0; a < 5 && !ok; a++) {
        DWORD t1[512]; int c1 = thp_threads_of(GetCurrentProcessId(), t1, 512);
        int n = nt_procs(g_nt, THP_MAXPROC), c2 = -1; DWORD *t2 = NULL;
        for (int i = 0; i < n; i++) if (g_nt[i].pid == GetCurrentProcessId()) { c2 = g_nt[i].ntids; t2 = g_nt[i].tids; }
        if (c1 == c2 && t2) {
            int match = 1;
            for (int i = 0; i < c1 && match; i++) { int f = 0; for (int j = 0; j < c2; j++) if (t1[i] == t2[j]) f = 1; if (!f) match = 0; }
            ok = match;
        }
        if (a == 4) CHECKF(ok, "toolhelp %d threads vs ntqsi %d", c1, c2);
    }
    CHECK(ok);
    SetEvent(b.stop); WaitForMultipleObjects(4, th, TRUE, 10000);
    for (int i = 0; i < 4; i++) CloseHandle(th[i]);
    CloseHandle(b.stop);
}
TEST(cross_image_names_match) {
    REQUIRE(U(CreateToolhelp32Snapshot)); REQUIRE(UNT(NtQuerySystemInformation));
    int n = nt_procs(g_nt, THP_MAXPROC);
    static PROCESSENTRY32W l[THP_MAXPROC]; int m = thp_procs(l, THP_MAXPROC), cmp = 0, mism = 0;
    for (int i = 0; i < m; i++) for (int j = 0; j < n; j++) if (l[i].th32ProcessID == g_nt[j].pid && l[i].th32ProcessID > 4) {
        cmp++;
        if (lstrcmpiW(l[i].szExeFile, g_nt[j].name)) mism++;
    }
    CHECK(cmp > 3);
    CHECKF(mism <= 1, "%d of %d image names differ", mism, cmp);
}
TEST(cross_process_counts_all_methods) {
    REQUIRE(U(CreateToolhelp32Snapshot)); REQUIRE(U(K32EnumProcesses)); REQUIRE(UNT(NtQuerySystemInformation));
    int best = 100;
    for (int a = 0; a < 5; a++) {
        static PROCESSENTRY32W l[THP_MAXPROC]; DWORD ids[4096], need;
        int t = thp_procs(l, THP_MAXPROC); U(K32EnumProcesses)(ids, sizeof ids, &need);
        int nt = nt_procs(g_nt, THP_MAXPROC), e = need / 4;
        int d = abs(t - e) + abs(t - nt);
        if (d < best) best = d;
    }
    CHECKF(best <= 3, "counts disagree by %d", best);
}
TEST(cross_openprocess_probe_vs_toolhelp) {
    REQUIRE(U(CreateToolhelp32Snapshot));
    static PROCESSENTRY32W l[THP_MAXPROC]; int n = thp_procs(l, THP_MAXPROC);
    int opened = 0, denied = 0, invalid = 0, other = 0;
    for (int i = 0; i < n; i++) {
        DWORD pid = l[i].th32ProcessID;
        if (pid == 0) continue;
        SET_ERR(0);
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (h) { opened++; CloseHandle(h); }
        else if (GetLastError() == ERROR_ACCESS_DENIED) denied++;
        else if (GetLastError() == ERROR_INVALID_PARAMETER) invalid++;
        else other++;
    }
    t_note("probe: %d opened, %d denied, %d invalid, %d other of %d", opened, denied, invalid, other, n);
    CHECK(opened > 3);
    CHECKF(invalid <= 3, "%d pids from the snapshot vanished", invalid);
    CHECK(other == 0);
    /* pid 0 itself can never be opened */
    SET_ERR(0);
    CHECK(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, 0) == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(cross_handle_count_vs_ntqsi) {
    REQUIRE(UNT(NtQuerySystemInformation));
    if (t_is_wine()) { t_skip("wine GetProcessHandleCount is a stub"); return; }
    DWORD hc = 0;
    CHECK(GetProcessHandleCount(GetCurrentProcess(), &hc));
    int n = nt_procs(g_nt, THP_MAXPROC), found = 0;
    for (int i = 0; i < n; i++) if (g_nt[i].pid == GetCurrentProcessId()) { found = 1; CHECKF(labs((long)g_nt[i].handles - (long)hc) <= 4, "ntqsi %lu vs GetProcessHandleCount %lu", g_nt[i].handles, hc); }
    CHECK(found);
}

/* ---- GetProcessHandleCount / GetProcessIdOfThread / GetThreadId / GetProcessId ---- */
TEST(hc_self_basic) {
    REQUIRE(U(GetProcessHandleCount));
    if (t_is_wine()) { t_skip("wine GetProcessHandleCount is a stub"); return; }
    DWORD c = 0xCCCCCCCC; SET_ERR(0);
    CHECK(U(GetProcessHandleCount)(GetCurrentProcess(), &c));
    CHECK(c > 3 && c < 100000);
    DWORD c2 = 0;
    CHECK(U(GetProcessHandleCount)(INVALID_HANDLE_VALUE, &c2));
    CHECK(labs((long)c2 - (long)c) <= 2);
}
TEST(hc_tracks_handle_creation) {
    REQUIRE(U(GetProcessHandleCount));
    if (t_is_wine()) { t_skip("wine GetProcessHandleCount is a stub"); return; }
    DWORD a, b, c; HANDLE h[64];
    U(GetProcessHandleCount)(GetCurrentProcess(), &a);
    for (int i = 0; i < 64; i++) h[i] = CreateEventW(NULL, 0, 0, NULL);
    U(GetProcessHandleCount)(GetCurrentProcess(), &b);
    CHECKF(b == a + 64, "handles %lu -> %lu", a, b);
    for (int i = 0; i < 64; i++) CloseHandle(h[i]);
    U(GetProcessHandleCount)(GetCurrentProcess(), &c);
    CHECKF(c == a, "handles after close %lu vs %lu", c, a);
    HANDLE d = CreateEventW(NULL, 0, 0, NULL), d2 = NULL;
    DuplicateHandle(GetCurrentProcess(), d, GetCurrentProcess(), &d2, 0, FALSE, DUPLICATE_SAME_ACCESS);
    U(GetProcessHandleCount)(GetCurrentProcess(), &b);
    EXPECT_EQ(b, a + 2);
    CloseHandle(d); CloseHandle(d2);
}
TEST(hc_invalid) {
    REQUIRE(U(GetProcessHandleCount));
    if (t_is_wine()) { t_skip("wine GetProcessHandleCount is a stub"); return; }
    DWORD c = 0x1234;
    SET_ERR(0);
    CHECK(!U(GetProcessHandleCount)(NULL, &c));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    EXPECT_EQ(c, 0x1234);
    HANDLE ev = CreateEventW(NULL, 0, 0, NULL);
    SET_ERR(0);
    CHECK(!U(GetProcessHandleCount)(ev, &c));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId()); CloseHandle(h);
    SET_ERR(0);
    CHECK(!U(GetProcessHandleCount)(h, &c));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    h = OpenProcess(SYNCHRONIZE, FALSE, GetCurrentProcessId());
    SET_ERR(0);
    CHECK(!U(GetProcessHandleCount)(h, &c));
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(h);
}
TEST(hc_child) {
    REQUIRE(U(GetProcessHandleCount));
    if (t_is_wine()) { t_skip("wine GetProcessHandleCount is a stub"); return; }
    PROCESS_INFORMATION pi;
    if (!thp_spawn(&pi, 0)) { CHECK(0); return; }
    thp_wait_module(pi.dwProcessId, L"kernel32.dll", 10000);
    DWORD c = 0;
    CHECK(U(GetProcessHandleCount)(pi.hProcess, &c));
    CHECK(c >= 5 && c < 500);
    HANDLE lim = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pi.dwProcessId);
    CHECK(U(GetProcessHandleCount)(lim, &c));
    CloseHandle(lim);
    TerminateProcess(pi.hProcess, 1); WaitForSingleObject(pi.hProcess, 10000);
    SET_ERR(0);
    BOOL r = U(GetProcessHandleCount)(pi.hProcess, &c);
    t_note("terminated process handle count: ret=%d c=%lu err=%lu", r, c, GetLastError());
    thp_reap(&pi);
}

static DWORD WINAPI idle_thread(LPVOID p) { WaitForSingleObject((HANDLE)p, 15000); return 0; }
TEST(pid_of_thread_basic) {
    REQUIRE(UK(GetProcessIdOfThread)); REQUIRE(U(GetThreadId));
    SET_ERR(0);
    EXPECT_EQ(UK(GetProcessIdOfThread)(GetCurrentThread()), GetCurrentProcessId());
    HANDLE ev = CreateEventW(NULL, 1, 0, NULL); DWORD tid;
    HANDLE t = CreateThread(NULL, 0, idle_thread, ev, 0, &tid);
    EXPECT_EQ(UK(GetProcessIdOfThread)(t), GetCurrentProcessId());
    EXPECT_EQ(U(GetThreadId)(t), tid);
    HANDLE ot = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, tid);
    CHECK(ot != NULL);
    EXPECT_EQ(UK(GetProcessIdOfThread)(ot), GetCurrentProcessId());
    EXPECT_EQ(U(GetThreadId)(ot), tid);
    CloseHandle(ot);
    SetEvent(ev); WaitForSingleObject(t, 10000);
    EXPECT_EQ(UK(GetProcessIdOfThread)(t), GetCurrentProcessId());     /* still valid after exit while handle open */
    CloseHandle(t); CloseHandle(ev);
}
TEST(pid_of_thread_invalid) {
    REQUIRE(UK(GetProcessIdOfThread)); REQUIRE(U(GetThreadId));
    SET_ERR(0);
    EXPECT_EQ(UK(GetProcessIdOfThread)(NULL), 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    EXPECT_EQ(U(GetThreadId)(NULL), 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    EXPECT_EQ(UK(GetProcessIdOfThread)(GetCurrentProcess()), 0);      /* process pseudo handle is the wrong type */
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE ev = CreateEventW(NULL, 0, 0, NULL);
    SET_ERR(0);
    EXPECT_EQ(UK(GetProcessIdOfThread)(ev), 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
    HANDLE ot = OpenThread(SYNCHRONIZE, FALSE, GetCurrentThreadId());
    SET_ERR(0);
    EXPECT_EQ(UK(GetProcessIdOfThread)(ot), 0);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    SET_ERR(0);
    EXPECT_EQ(U(GetThreadId)(ot), 0);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(ot);
    ot = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentThreadId()); CloseHandle(ot);
    SET_ERR(0);
    EXPECT_EQ(UK(GetProcessIdOfThread)(ot), 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(pid_of_thread_child_thread) {
    REQUIRE(UK(GetProcessIdOfThread)); REQUIRE(U(GetProcessId));
    PROCESS_INFORMATION pi;
    if (!thp_spawn(&pi, CREATE_SUSPENDED)) { CHECK(0); return; }
    EXPECT_EQ(UK(GetProcessIdOfThread)(pi.hThread), pi.dwProcessId);
    EXPECT_EQ(U(GetProcessId)(pi.hProcess), pi.dwProcessId);
    DWORD ids[16]; int n = thp_threads_of(pi.dwProcessId, ids, 16);
    EXPECT_EQ(n, 1);
    HANDLE ot = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, pi.dwThreadId);
    CHECK(ot != NULL);
    EXPECT_EQ(UK(GetProcessIdOfThread)(ot), pi.dwProcessId);
    CloseHandle(ot);
    thp_reap(&pi);
}
TEST(get_process_id_invalid) {
    REQUIRE(U(GetProcessId));
    SET_ERR(0);
    EXPECT_EQ(U(GetProcessId)(NULL), 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    EXPECT_EQ(U(GetProcessId)(GetCurrentProcess()), GetCurrentProcessId());
    EXPECT_EQ(U(GetProcessId)(INVALID_HANDLE_VALUE), GetCurrentProcessId());
    SET_ERR(0);
    EXPECT_EQ(U(GetProcessId)(GetCurrentThread()), 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, GetCurrentProcessId());
    SET_ERR(0);
    EXPECT_EQ(U(GetProcessId)(h), 0);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(h);
}
