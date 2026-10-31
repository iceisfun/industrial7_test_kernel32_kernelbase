/* CreateToolhelp32Snapshot, Process32*, Thread32* */
#include "thp.h"

#define NEED_SNAP() REQUIRE(U(CreateToolhelp32Snapshot)); REQUIRE(U(Process32FirstW)); REQUIRE(U(Process32NextW))

static HANDLE snap(DWORD fl, DWORD pid) { return U(CreateToolhelp32Snapshot)(fl, pid); }

TEST(th_layout_sizes) {
    CHECK(sizeof(PROCESSENTRY32W) == PSZ_PROC_W);
    CHECK(sizeof(THREADENTRY32) == PSZ_THREAD);
    CHECK(sizeof(MODULEENTRY32W) == PSZ_MOD_W);
    CHECK(sizeof(HEAPLIST32) == PSZ_HEAPL);
    CHECK(TH32CS_SNAPHEAPLIST == 1 && TH32CS_SNAPPROCESS == 2 && TH32CS_SNAPTHREAD == 4 && TH32CS_SNAPMODULE == 8);
    CHECK(TH32CS_SNAPMODULE32 == 0x10 && TH32CS_INHERIT == 0x80000000u);
}

TEST(th_exports_present) {
    static const char *n[] = { "CreateToolhelp32Snapshot", "Process32First", "Process32FirstW", "Process32Next", "Process32NextW",
        "Thread32First", "Thread32Next", "Module32First", "Module32FirstW", "Module32Next", "Module32NextW",
        "Heap32ListFirst", "Heap32ListNext", "Heap32First", "Heap32Next", "Toolhelp32ReadProcessMemory" };
    for (int i = 0; i < 16; i++) CHECKF(t_proc_k32(n[i]) != NULL, "kernel32 export %s", n[i]);
}

TEST(th_snap_process_basic) {
    NEED_SNAP();
    HANDLE s = snap(TH32CS_SNAPPROCESS, 0);
    CHECK(s != INVALID_HANDLE_VALUE && s != NULL);
    if (s == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32W e; e.dwSize = sizeof e;
    SET_ERR(0);
    CHECK(U(Process32FirstW)(s, &e));
    EXPECT_EQ(e.dwSize, PSZ_PROC_W);
    int n = 1;
    for (;;) {
        e.dwSize = sizeof e;
        if (!U(Process32NextW)(s, &e)) break;
        n++;
        if (n > 100000) break;
    }
    EXPECT_ERR(ERROR_NO_MORE_FILES);
    CHECK(n >= 4);
    /* after exhaustion Next keeps failing */
    e.dwSize = sizeof e; SET_ERR(0);
    CHECK(!U(Process32NextW)(s, &e));
    EXPECT_ERR(ERROR_NO_MORE_FILES);
    CloseHandle(s);
}

TEST(th_snap_first_is_system_process) {
    NEED_SNAP();
    HANDLE s = snap(TH32CS_SNAPPROCESS, 0);
    if (s == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    PROCESSENTRY32W e; e.dwSize = sizeof e;
    CHECK(U(Process32FirstW)(s, &e));
    if (t_is_wine()) { CloseHandle(s); t_skip("no idle process entry under wine"); return; }
    EXPECT_EQ(e.th32ProcessID, 0);
    EXPECT_EQ(e.th32ParentProcessID, 0);
    EXPECT_WSTR(e.szExeFile, L"[System Process]");
    CHECK(e.cntThreads >= 1);
    CloseHandle(s);
}

TEST(th_snap_contains_system_pid4) {
    NEED_SNAP();
    if (t_is_wine()) { t_skip("no System process under wine"); return; }
    PROCESSENTRY32W e;
    CHECK(thp_find_pid(4, &e));
    CHECK(!lstrcmpiW(e.szExeFile, L"System"));
    EXPECT_EQ(e.th32ParentProcessID, 0);
    CHECK(e.cntThreads >= 1);
}

TEST(th_snap_contains_self) {
    NEED_SNAP();
    PROCESSENTRY32W e;
    CHECK(thp_find_pid(GetCurrentProcessId(), &e));
    WCHAR me[MAX_PATH]; GetModuleFileNameW(NULL, me, MAX_PATH);
    CHECK(!lstrcmpiW(e.szExeFile, thp_basew(me)));
    DWORD pp = thp_parent_pid();
    if (!t_is_wine()) EXPECT_EQ(e.th32ParentProcessID, pp);
    CHECK(e.cntThreads >= 1);
    CHECK(e.pcPriClassBase > 0 && e.pcPriClassBase <= 31);
}

TEST(th_snap_self_cntusage_unused) {
    NEED_SNAP();
    PROCESSENTRY32W e;
    CHECK(thp_find_pid(GetCurrentProcessId(), &e));
    t_note("cntUsage=%lu th32ModuleID=%lu th32DefaultHeapID=%llu dwFlags=%lu", e.cntUsage, e.th32ModuleID, (unsigned long long)e.th32DefaultHeapID, e.dwFlags);
}

TEST(th_snap_bad_dwsize_first) {
    NEED_SNAP();
    HANDLE s = snap(TH32CS_SNAPPROCESS, 0);
    if (s == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    static const DWORD bad[] = { 0, 1, 4, 0x128, 0x237, 0x239, 0x240, 0x1000, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        PROCESSENTRY32W e; memset(&e, 0xCC, sizeof e); e.dwSize = bad[i];
        SET_ERR(0);
        BOOL r = U(Process32FirstW)(s, &e);
        CHECKF(!r, "dwSize=0x%lx", bad[i]);
        EXPECT_ERR(ERROR_BAD_LENGTH);
        SET_ERR(0);
        e.dwSize = bad[i];
        r = U(Process32NextW)(s, &e);
        CHECKF(!r, "next dwSize=0x%lx", bad[i]);
        EXPECT_ERR(ERROR_BAD_LENGTH);
    }
    CloseHandle(s);
}

TEST(th_snap_null_entry) {
    NEED_SNAP();
    if (t_is_wine()) { t_skip("wine faults on NULL entry"); return; }
    HANDLE s = snap(TH32CS_SNAPPROCESS, 0);
    if (s == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    SET_ERR(0);
    CHECK(!U(Process32FirstW)(s, NULL));
    t_note("Process32FirstW(NULL entry) err=%lu", GetLastError());
    CHECK(!U(Process32NextW)(s, NULL));
    REQUIRE(U(Thread32First));
    CHECK(!U(Thread32First)(s, NULL));
    CHECK(!U(Thread32Next)(s, NULL));
    CHECK(!U(Module32FirstW)(s, NULL));
    CHECK(!U(Module32NextW)(s, NULL));
    CloseHandle(s);
}

TEST(th_snap_invalid_handles) {
    NEED_SNAP();
    PROCESSENTRY32W e; e.dwSize = sizeof e;
    SET_ERR(0);
    CHECK(!U(Process32FirstW)(NULL, &e));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    e.dwSize = sizeof e; SET_ERR(0);
    CHECK(!U(Process32NextW)(NULL, &e));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    e.dwSize = sizeof e; SET_ERR(0);
    CHECK(!U(Process32FirstW)(INVALID_HANDLE_VALUE, &e));
    t_note("Process32FirstW(INVALID_HANDLE_VALUE) err=%lu", GetLastError());
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    e.dwSize = sizeof e; SET_ERR(0);
    CHECK(!U(Process32FirstW)(ev, &e));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    THREADENTRY32 t; t.dwSize = sizeof t; SET_ERR(0);
    CHECK(!U(Thread32First)(ev, &t));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    MODULEENTRY32W m; m.dwSize = sizeof m; SET_ERR(0);
    CHECK(!U(Module32FirstW)(ev, &m));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
}

TEST(th_snap_closed_handle) {
    NEED_SNAP();
    HANDLE s = snap(TH32CS_SNAPPROCESS, 0);
    if (s == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    CHECK(CloseHandle(s));
    PROCESSENTRY32W e; e.dwSize = sizeof e; SET_ERR(0);
    CHECK(!U(Process32FirstW)(s, &e));
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CHECK(!CloseHandle(s));
}

TEST(th_snap_wrong_entry_type_empty) {
    NEED_SNAP(); REQUIRE(U(Thread32First)); REQUIRE(U(Module32FirstW)); REQUIRE(U(Heap32ListFirst));
    /* a process-only snapshot has no thread/module/heap data */
    HANDLE s = snap(TH32CS_SNAPPROCESS, 0);
    if (s == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    THREADENTRY32 t; t.dwSize = sizeof t; SET_ERR(0);
    CHECK(!U(Thread32First)(s, &t));
    t_note("wrong-type snapshot err=%lu", GetLastError());
    MODULEENTRY32W m; m.dwSize = sizeof m; SET_ERR(0);
    CHECK(!U(Module32FirstW)(s, &m));
    t_note("wrong-type snapshot err=%lu", GetLastError());
    HEAPLIST32 h; h.dwSize = sizeof h; SET_ERR(0);
    CHECK(!U(Heap32ListFirst)(s, &h));
    t_note("wrong-type snapshot err=%lu", GetLastError());
    CloseHandle(s);
    /* a thread-only snapshot has no processes */
    s = snap(TH32CS_SNAPTHREAD, 0);
    if (s == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    PROCESSENTRY32W p; p.dwSize = sizeof p; SET_ERR(0);
    CHECK(!U(Process32FirstW)(s, &p));
    t_note("wrong-type snapshot err=%lu", GetLastError());
    SET_ERR(0);
    CHECK(!U(Process32NextW)(s, &p));
    t_note("wrong-type snapshot err=%lu", GetLastError());
    CloseHandle(s);
}

TEST(th_snap_empty_entry_zeroed) {
    NEED_SNAP();
    HANDLE s = snap(TH32CS_SNAPTHREAD, 0);
    if (s == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    PROCESSENTRY32W p; memset(&p, 0xCC, sizeof p); p.dwSize = sizeof p;
    CHECK(!U(Process32FirstW)(s, &p));
    EXPECT_EQ(p.dwSize, PSZ_PROC_W);
    EXPECT_EQ(p.th32ProcessID, 0);
    EXPECT_EQ(p.szExeFile[0], 0);
    CloseHandle(s);
}

TEST(th_snap_inherit_flag) {
    NEED_SNAP();
    HANDLE s = snap(TH32CS_SNAPPROCESS | TH32CS_INHERIT, 0);
    if (s == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    DWORD fl = 0; CHECK(GetHandleInformation(s, &fl));
    CHECK(fl & HANDLE_FLAG_INHERIT);
    PROCESSENTRY32W e; e.dwSize = sizeof e;
    CHECK(U(Process32FirstW)(s, &e));
    CloseHandle(s);
    s = snap(TH32CS_SNAPPROCESS, 0);
    if (s == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    fl = 0; CHECK(GetHandleInformation(s, &fl));
    CHECK(!(fl & HANDLE_FLAG_INHERIT));
    CloseHandle(s);
}

TEST(th_snap_handle_is_section) {
    NEED_SNAP();
    /* the snapshot handle can be duplicated and used from the duplicate */
    HANDLE s = snap(TH32CS_SNAPPROCESS, 0), d = NULL;
    if (s == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    CHECK(DuplicateHandle(GetCurrentProcess(), s, GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS));
    CloseHandle(s);
    PROCESSENTRY32W e; e.dwSize = sizeof e;
    CHECK(U(Process32FirstW)(d, &e));
    if (!t_is_wine()) EXPECT_EQ(e.th32ProcessID, 0);
    CloseHandle(d);
}

TEST(th_snap_flags_edge) {
    NEED_SNAP();
    static const DWORD fl[] = { 0, 0x20, 0x40, 0x100, 0x7FFFFF00, 0x80000000u, 0xFFFFFFFFu };
    for (unsigned i = 0; i < sizeof fl / sizeof *fl; i++) {
        SET_ERR(0);
        HANDLE s = snap(fl[i], 0);
        if (s == INVALID_HANDLE_VALUE) t_note("flags 0x%lx -> INVALID_HANDLE_VALUE err=%lu", fl[i], GetLastError());
        else { t_note("flags 0x%lx -> ok", fl[i]); CloseHandle(s); }
    }
    CHECK(1);
}

TEST(th_snap_snapall_alias) {
    NEED_SNAP(); REQUIRE(U(Thread32First)); REQUIRE(U(Module32FirstW)); REQUIRE(U(Heap32ListFirst));
    HANDLE s = snap(TH32CS_SNAPALL, GetCurrentProcessId());
    if (s == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    PROCESSENTRY32W p; p.dwSize = sizeof p; CHECK(U(Process32FirstW)(s, &p));
    THREADENTRY32 t; t.dwSize = sizeof t; CHECK(U(Thread32First)(s, &t));
    MODULEENTRY32W m; m.dwSize = sizeof m; CHECK(U(Module32FirstW)(s, &m));
    HEAPLIST32 h; h.dwSize = sizeof h;
    if (!t_is_wine()) CHECK(U(Heap32ListFirst)(s, &h));
    CloseHandle(s);
}

TEST(th_snap_process_pid_ignored) {
    NEED_SNAP();
    static const DWORD pids[] = { 0, 4, 0x12345678, 0xFFFFFFF0u };
    for (int i = 0; i < 4; i++) {
        HANDLE s = snap(TH32CS_SNAPPROCESS, pids[i]);
        CHECKF(s != INVALID_HANDLE_VALUE, "pid %lx", pids[i]);
        if (s == INVALID_HANDLE_VALUE) continue;
        PROCESSENTRY32W e; e.dwSize = sizeof e;
        CHECK(U(Process32FirstW)(s, &e));
        CloseHandle(s);
    }
}

TEST(th_snap_first_restarts_iteration) {
    NEED_SNAP();
    int wn = t_is_wine();
    HANDLE s = snap(TH32CS_SNAPPROCESS, 0);
    if (s == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    PROCESSENTRY32W a, b; int n1 = 0, n2 = 0;
    a.dwSize = sizeof a; CHECK(U(Process32FirstW)(s, &a));
    do { n1++; b.dwSize = sizeof b; } while (U(Process32NextW)(s, &b));
    a.dwSize = sizeof a; CHECK(U(Process32FirstW)(s, &a));
    if (!wn) EXPECT_EQ(a.th32ProcessID, 0);
    do { n2++; b.dwSize = sizeof b; } while (U(Process32NextW)(s, &b));
    EXPECT_EQ(n1, n2);
    /* snapshot is frozen: repeated enumeration gives the same pid list */
    DWORD l1[THP_MAXPROC], l2[THP_MAXPROC]; int c1 = 0, c2 = 0;
    BOOL ok;
    a.dwSize = sizeof a; ok = U(Process32FirstW)(s, &a);
    while (ok && c1 < THP_MAXPROC) { l1[c1++] = a.th32ProcessID; a.dwSize = sizeof a; ok = U(Process32NextW)(s, &a); }
    a.dwSize = sizeof a; ok = U(Process32FirstW)(s, &a);
    while (ok && c2 < THP_MAXPROC) { l2[c2++] = a.th32ProcessID; a.dwSize = sizeof a; ok = U(Process32NextW)(s, &a); }
    CHECK(c1 == c2 && !memcmp(l1, l2, c1 * sizeof(DWORD)));
    CloseHandle(s);
}

TEST(th_snap_two_snapshots_independent) {
    NEED_SNAP();
    HANDLE s1 = snap(TH32CS_SNAPPROCESS, 0), s2 = snap(TH32CS_SNAPPROCESS, 0);
    CHECK(s1 != INVALID_HANDLE_VALUE && s2 != INVALID_HANDLE_VALUE && s1 != s2);
    PROCESSENTRY32W a, b; a.dwSize = b.dwSize = sizeof a;
    CHECK(U(Process32FirstW)(s1, &a)); CHECK(U(Process32NextW)(s1, &a));
    CHECK(U(Process32FirstW)(s2, &b));
    if (!t_is_wine()) EXPECT_EQ(b.th32ProcessID, 0);       /* s2 unaffected by s1 position */
    CloseHandle(s1); CloseHandle(s2);
}

TEST(th_snap_next_continues_after_first) {
    NEED_SNAP();
    HANDLE s = snap(TH32CS_SNAPPROCESS, 0);
    if (s == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    PROCESSENTRY32W a; a.dwSize = sizeof a;
    CHECK(U(Process32FirstW)(s, &a));
    DWORD first = a.th32ProcessID;
    if (!t_is_wine()) EXPECT_EQ(first, 0);
    a.dwSize = sizeof a;
    CHECK(U(Process32NextW)(s, &a));
    CHECK(a.th32ProcessID != first);          /* second entry is not the idle process again */
    CloseHandle(s);
}

TEST(th_proc_entries_sane) {
    NEED_SNAP();
    static PROCESSENTRY32W l[THP_MAXPROC];
    int n = thp_procs(l, THP_MAXPROC);
    CHECK(n > 3);
    int bad = 0, dup = 0, empty = 0, zerothr = 0;
    for (int i = 0; i < n; i++) {
        if (l[i].dwSize != PSZ_PROC_W) bad++;
        if (!l[i].szExeFile[0]) empty++;
        if (l[i].cntThreads == 0) zerothr++;
        for (int j = i + 1; j < n; j++) if (l[i].th32ProcessID == l[j].th32ProcessID) dup++;
        if (l[i].th32ProcessID & 3) { t_note("pid %lu not multiple of 4", l[i].th32ProcessID); }
    }
    CHECK(bad == 0); CHECK(dup == 0); CHECK(empty == 0);
    CHECKF(zerothr == 0, "%d processes with zero threads", zerothr);
}

TEST(th_proc_a_vs_w_parity) {
    NEED_SNAP(); REQUIRE(UN(Process32First)); REQUIRE(UN(Process32Next));
    HANDLE s = snap(TH32CS_SNAPPROCESS, 0), s2 = snap(TH32CS_SNAPPROCESS, 0);   /* the cursor lives in the snapshot, so use one each */
    if (s == INVALID_HANDLE_VALUE || s2 == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    PROCESSENTRY32 a; PROCESSENTRY32W w; int n = 0, mism = 0;
    a.dwSize = sizeof a; w.dwSize = sizeof w;
    BOOL oa = UN(Process32First)(s, &a), ow = U(Process32FirstW)(s2, &w);
    CHECK(oa && ow);
    while (oa && ow && n < 100000) {
        char conv[MAX_PATH]; thp_w2a(w.szExeFile, conv, MAX_PATH);
        if (a.th32ProcessID != w.th32ProcessID || a.th32ParentProcessID != w.th32ParentProcessID ||
            a.cntThreads != w.cntThreads || strcmp(conv, a.szExeFile)) mism++;
        n++;
        a.dwSize = sizeof a; w.dwSize = sizeof w;
        oa = UN(Process32Next)(s, &a); ow = U(Process32NextW)(s2, &w);
    }
    CHECK(oa == ow || n > 0);
    CHECKF(mism <= 2, "%d mismatching entries of %d", mism, n);
    CloseHandle(s); CloseHandle(s2);
}

TEST(th_proc_a_bad_dwsize) {
    NEED_SNAP(); REQUIRE(UN(Process32First)); REQUIRE(UN(Process32Next));
    HANDLE s = snap(TH32CS_SNAPPROCESS, 0);
    if (s == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    PROCESSENTRY32 a;
    DWORD bad[] = { 0, sizeof a - 1, sizeof a + 1, PSZ_PROC_W };
    for (int i = 0; i < 4; i++) {
        a.dwSize = bad[i]; SET_ERR(0);
        CHECKF(!UN(Process32First)(s, &a), "dwSize 0x%lx", bad[i]);
        EXPECT_ERR(ERROR_BAD_LENGTH);
        a.dwSize = bad[i]; SET_ERR(0);
        CHECKF(!UN(Process32Next)(s, &a), "next dwSize 0x%lx", bad[i]);
        EXPECT_ERR(ERROR_BAD_LENGTH);
    }
    PROCESSENTRY32W w; w.dwSize = sizeof(PROCESSENTRY32); SET_ERR(0);
    CHECK(!U(Process32FirstW)(s, &w));
    EXPECT_ERR(ERROR_BAD_LENGTH);
    CloseHandle(s);
}

TEST(th_proc_a_empty_and_end) {
    NEED_SNAP(); REQUIRE(UN(Process32First)); REQUIRE(UN(Process32Next));
    HANDLE s = snap(TH32CS_SNAPTHREAD, 0);
    if (s == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    PROCESSENTRY32 a; a.dwSize = sizeof a; SET_ERR(0);
    CHECK(!UN(Process32First)(s, &a));
    EXPECT_ERR(ERROR_NO_MORE_FILES);
    CloseHandle(s);
    s = snap(TH32CS_SNAPPROCESS, 0);
    a.dwSize = sizeof a; BOOL ok = UN(Process32First)(s, &a); int n = 0;
    while (ok && n++ < 100000) { a.dwSize = sizeof a; ok = UN(Process32Next)(s, &a); }
    EXPECT_ERR(ERROR_NO_MORE_FILES);
    CloseHandle(s);
}

TEST(th_proc_cntthreads_matches_thread_list) {
    NEED_SNAP(); REQUIRE(U(Thread32First)); REQUIRE(U(Thread32Next));
    for (int attempt = 0; attempt < 5; attempt++) {
        HANDLE s = snap(TH32CS_SNAPPROCESS | TH32CS_SNAPTHREAD, 0);
        if (s == INVALID_HANDLE_VALUE) { CHECK(0); return; }
        static PROCESSENTRY32W pl[THP_MAXPROC]; static DWORD owner[20000];
        int np = 0, nt = 0;
        PROCESSENTRY32W p; p.dwSize = sizeof p;
        BOOL ok = U(Process32FirstW)(s, &p);
        while (ok && np < THP_MAXPROC) { pl[np++] = p; p.dwSize = sizeof p; ok = U(Process32NextW)(s, &p); }
        THREADENTRY32 t; t.dwSize = sizeof t;
        ok = U(Thread32First)(s, &t);
        while (ok && nt < 20000) { owner[nt++] = t.th32OwnerProcessID; t.dwSize = sizeof t; ok = U(Thread32Next)(s, &t); }
        CloseHandle(s);
        int mism = 0;
        for (int i = 0; i < np; i++) {
            if (pl[i].th32ProcessID == 0) continue; /* idle threads are listed per-CPU in a version-dependent way */
            int c = 0;
            for (int j = 0; j < nt; j++) if (owner[j] == pl[i].th32ProcessID) c++;
            if (c != (int)pl[i].cntThreads) mism++;
        }
        if (!mism) { CHECK(1); return; }
        if (attempt == 4) CHECKF(0, "%d of %d processes: cntThreads != thread entries", mism, np);
    }
}

typedef struct { HANDLE start, stop; volatile LONG up; } blk_t;
static DWORD WINAPI blocker(LPVOID p) {
    blk_t *b = p;
    InterlockedIncrement(&b->up);
    WaitForSingleObject(b->stop, 15000);
    return 0;
}

TEST(th_proc_cntthreads_grows) {
    NEED_SNAP();
    PROCESSENTRY32W e;
    CHECK(thp_find_pid(GetCurrentProcessId(), &e));
    DWORD base = e.cntThreads;
    blk_t b; b.stop = CreateEventW(NULL, TRUE, FALSE, NULL); b.up = 0;
    HANDLE th[6]; DWORD tid[6];
    for (int i = 0; i < 6; i++) th[i] = CreateThread(NULL, 0, blocker, &b, 0, &tid[i]);
    for (int i = 0; i < 200 && b.up < 6; i++) Sleep(10);
    CHECK(thp_find_pid(GetCurrentProcessId(), &e));
    CHECKF(e.cntThreads >= base + 6, "base %lu now %lu", base, e.cntThreads);
    DWORD ids[512]; int n = thp_threads_of(GetCurrentProcessId(), ids, 512);
    for (int i = 0; i < 6; i++) {
        int f = 0; for (int j = 0; j < n; j++) if (ids[j] == tid[i]) f = 1;
        CHECKF(f, "thread %lu missing", tid[i]);
    }
    SetEvent(b.stop);
    WaitForMultipleObjects(6, th, TRUE, 10000);
    for (int i = 0; i < 6; i++) CloseHandle(th[i]);
    CloseHandle(b.stop);
    /* once exited and closed they disappear */
    int gone = 0;
    for (int k = 0; k < 100 && !gone; k++) {
        n = thp_threads_of(GetCurrentProcessId(), ids, 512);
        gone = 1;
        for (int i = 0; i < 6; i++) for (int j = 0; j < n; j++) if (ids[j] == tid[i]) gone = 0;
        if (!gone) Sleep(20);
    }
    CHECK(gone);
}

TEST(th_proc_child_present_then_absent) {
    NEED_SNAP();
    PROCESS_INFORMATION pi;
    if (!thp_spawn(&pi, 0)) { CHECK(0); return; }
    DWORD cpid = pi.dwProcessId;
    PROCESSENTRY32W e;
    CHECK(thp_find_pid(cpid, &e));
    EXPECT_EQ(e.th32ParentProcessID, GetCurrentProcessId());
    WCHAR me[MAX_PATH]; GetModuleFileNameW(NULL, me, MAX_PATH);
    CHECK(!lstrcmpiW(e.szExeFile, thp_basew(me)));
    CHECK(e.cntThreads >= 1);
    thp_reap(&pi);
    int gone = 0;
    for (int k = 0; k < 100 && !gone; k++) { gone = !thp_find_pid(cpid, NULL); if (!gone) Sleep(50); }
    CHECK(gone);
}

TEST(th_proc_suspended_child_listed) {
    NEED_SNAP();
    PROCESS_INFORMATION pi;
    if (!thp_spawn(&pi, CREATE_SUSPENDED)) { CHECK(0); return; }
    PROCESSENTRY32W e;
    CHECK(thp_find_pid(pi.dwProcessId, &e));
    EXPECT_EQ(e.cntThreads, 1);
    DWORD ids[16]; int n = thp_threads_of(pi.dwProcessId, ids, 16);
    EXPECT_EQ(n, 1);
    if (n == 1) EXPECT_EQ(ids[0], pi.dwThreadId);
    thp_reap(&pi);
}

TEST(th_proc_parent_chain) {
    NEED_SNAP();
    PROCESSENTRY32W e;
    DWORD pp = thp_parent_pid();
    if (pp && thp_find_pid(pp, &e)) { CHECK(e.th32ProcessID == pp); t_note("parent %lu = %ls", pp, e.szExeFile); }
    else t_note("parent %lu not in snapshot (exited)", pp);
}

/* ---------------- threads ---------------- */
TEST(th_thread_basic) {
    NEED_SNAP(); REQUIRE(U(Thread32First)); REQUIRE(U(Thread32Next));
    HANDLE s = snap(TH32CS_SNAPTHREAD, 0);
    if (s == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    THREADENTRY32 t; t.dwSize = sizeof t; SET_ERR(0);
    CHECK(U(Thread32First)(s, &t));
    EXPECT_EQ(t.dwSize, PSZ_THREAD);
    int n = 0, bad = 0;
    do {
        n++;
        if (!t.th32ThreadID) bad++;
        if (t.tpBasePri < 0 || t.tpBasePri > 31) { bad++; }
        t.dwSize = sizeof t;
    } while (U(Thread32Next)(s, &t) && n < 100000);
    EXPECT_ERR(ERROR_NO_MORE_FILES);
    CHECK(n > 10);
    CHECKF(bad == 0, "%d bad thread entries", bad);
    CloseHandle(s);
}

TEST(th_thread_bad_dwsize) {
    NEED_SNAP(); REQUIRE(U(Thread32First)); REQUIRE(U(Thread32Next));
    HANDLE s = snap(TH32CS_SNAPTHREAD, 0);
    if (s == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    static const DWORD bad[] = { 0, 4, 0x1b, 0x1d, 0x20, 0x238, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        THREADENTRY32 t; memset(&t, 0xCC, sizeof t); t.dwSize = bad[i]; SET_ERR(0);
        CHECKF(!U(Thread32First)(s, &t), "dwSize 0x%lx", bad[i]);
        EXPECT_ERR(ERROR_BAD_LENGTH);
        t.dwSize = bad[i]; SET_ERR(0);
        CHECKF(!U(Thread32Next)(s, &t), "next dwSize 0x%lx", bad[i]);
        EXPECT_ERR(ERROR_BAD_LENGTH);
    }
    CloseHandle(s);
}

TEST(th_thread_contains_current) {
    NEED_SNAP(); REQUIRE(U(Thread32First));
    DWORD ids[512]; int n = thp_threads_of(GetCurrentProcessId(), ids, 512);
    int f = 0; for (int i = 0; i < n; i++) if (ids[i] == GetCurrentThreadId()) f = 1;
    CHECK(f);
    PROCESSENTRY32W e; CHECK(thp_find_pid(GetCurrentProcessId(), &e));
    CHECKF(abs((int)e.cntThreads - n) <= 1, "cntThreads %lu thread entries %d", e.cntThreads, n);
}

TEST(th_thread_pid_param_ignored) {
    NEED_SNAP(); REQUIRE(U(Thread32First)); REQUIRE(U(Thread32Next));
    HANDLE s = snap(TH32CS_SNAPTHREAD, 0xFFFFFFF0u);
    CHECK(s != INVALID_HANDLE_VALUE);
    if (s == INVALID_HANDLE_VALUE) return;
    THREADENTRY32 t; t.dwSize = sizeof t; int f = 0;
    BOOL ok = U(Thread32First)(s, &t);
    while (ok) { if (t.th32ThreadID == GetCurrentThreadId()) f = 1; t.dwSize = sizeof t; ok = U(Thread32Next)(s, &t); }
    CHECK(f);
    CloseHandle(s);
}

TEST(th_thread_owner_is_known_process) {
    NEED_SNAP(); REQUIRE(U(Thread32First)); REQUIRE(U(Thread32Next));
    HANDLE s = snap(TH32CS_SNAPTHREAD | TH32CS_SNAPPROCESS, 0);
    if (s == INVALID_HANDLE_VALUE) { CHECK(0); return; }
    static DWORD pids[THP_MAXPROC]; int np = 0;
    PROCESSENTRY32W p; p.dwSize = sizeof p; BOOL ok = U(Process32FirstW)(s, &p);
    while (ok && np < THP_MAXPROC) { pids[np++] = p.th32ProcessID; p.dwSize = sizeof p; ok = U(Process32NextW)(s, &p); }
    THREADENTRY32 t; t.dwSize = sizeof t; int orphans = 0;
    ok = U(Thread32First)(s, &t);
    while (ok) {
        int f = 0; for (int i = 0; i < np; i++) if (pids[i] == t.th32OwnerProcessID) { f = 1; break; }
        if (!f) orphans++;
        t.dwSize = sizeof t; ok = U(Thread32Next)(s, &t);
    }
    CHECKF(orphans == 0, "%d threads with unknown owner in the same snapshot", orphans);
    CloseHandle(s);
}

TEST(th_thread_priority_reflects_set) {
    NEED_SNAP(); REQUIRE(U(Thread32First));
    int old = GetThreadPriority(GetCurrentThread());
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    HANDLE s = snap(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 t; t.dwSize = sizeof t; LONG pri = -999; BOOL ok = s != INVALID_HANDLE_VALUE && U(Thread32First)(s, &t);
    while (ok) { if (t.th32ThreadID == GetCurrentThreadId()) pri = t.tpBasePri; t.dwSize = sizeof t; ok = U(Thread32Next)(s, &t); }
    if (s != INVALID_HANDLE_VALUE) CloseHandle(s);
    SetThreadPriority(GetCurrentThread(), old);
    t_note("tpBasePri with ABOVE_NORMAL = %ld", pri);
    CHECK(pri > 0 && pri <= 31);
}

static volatile LONG g_stop_churn;
static DWORD WINAPI churn_leaf(LPVOID p) { Sleep(1); return 0; }
static DWORD WINAPI churn(LPVOID p) {
    while (!g_stop_churn) {
        HANDLE h[8];
        for (int i = 0; i < 8; i++) h[i] = CreateThread(NULL, 0, churn_leaf, NULL, 0, NULL);
        for (int i = 0; i < 8; i++) { if (h[i]) { WaitForSingleObject(h[i], 2000); CloseHandle(h[i]); } }
    }
    return 0;
}

TEST(th_snap_concurrent_thread_churn) {
    NEED_SNAP(); REQUIRE(U(Thread32First)); REQUIRE(U(Thread32Next));
    g_stop_churn = 0;
    HANDLE ch = CreateThread(NULL, 0, churn, NULL, 0, NULL);
    int consistent = 0, total = 40;
    for (int k = 0; k < total; k++) {
        HANDLE s = snap(TH32CS_SNAPPROCESS | TH32CS_SNAPTHREAD, 0);
        if (s == INVALID_HANDLE_VALUE) { CHECK(0); continue; }
        PROCESSENTRY32W p; p.dwSize = sizeof p; DWORD cnt = 0xFFFFFFFF; DWORD me = GetCurrentProcessId();
        BOOL ok = U(Process32FirstW)(s, &p);
        while (ok) { if (p.th32ProcessID == me) cnt = p.cntThreads; p.dwSize = sizeof p; ok = U(Process32NextW)(s, &p); }
        THREADENTRY32 t; t.dwSize = sizeof t; DWORD n = 0; DWORD mine = 0;
        ok = U(Thread32First)(s, &t);
        while (ok) { if (t.th32OwnerProcessID == me) { n++; if (t.th32ThreadID == GetCurrentThreadId()) mine = 1; } t.dwSize = sizeof t; ok = U(Thread32Next)(s, &t); }
        CloseHandle(s);
        CHECK(mine);
        if (cnt == n) consistent++;
    }
    g_stop_churn = 1;
    WaitForSingleObject(ch, 10000); CloseHandle(ch);
    CHECKF(consistent == total, "only %d of %d snapshots consistent", consistent, total);
}

TEST(th_snap_many_repeated_no_leak) {
    NEED_SNAP();
    DWORD h0 = 0, h1 = 0;
    GetProcessHandleCount(GetCurrentProcess(), &h0);
    for (int i = 0; i < 300; i++) {
        HANDLE s = snap(TH32CS_SNAPPROCESS | TH32CS_SNAPTHREAD, 0);
        CHECK(s != INVALID_HANDLE_VALUE);
        if (s != INVALID_HANDLE_VALUE) CloseHandle(s);
    }
    GetProcessHandleCount(GetCurrentProcess(), &h1);
    CHECKF(h1 <= h0 + 4, "handles %lu -> %lu", h0, h1);
}

TEST(th_snap_invalid_pid_modules) {
    NEED_SNAP();
    static const DWORD pids[] = { 0xFFFFFFF0u, 0x7FFFFFFC, 1, 2, 3 };
    for (int i = 0; i < 5; i++) {
        SET_ERR(0);
        HANDLE s = snap(TH32CS_SNAPMODULE, pids[i]);
        if (s != INVALID_HANDLE_VALUE) { t_note("module snapshot of pid %lu unexpectedly succeeded", pids[i]); CloseHandle(s); }
        else t_note("module snapshot pid %lu err=%lu", pids[i], GetLastError());
        SET_ERR(0);
        s = snap(TH32CS_SNAPHEAPLIST, pids[i]);
        if (s != INVALID_HANDLE_VALUE) { t_note("heap snapshot of pid %lu unexpectedly succeeded", pids[i]); CloseHandle(s); }
        else t_note("heap snapshot pid %lu err=%lu", pids[i], GetLastError());
    }
    CHECK(1);
}

TEST(th_snap_module_of_system_protected) {
    NEED_SNAP();
    for (int k = 0; k < 2; k++) {
        DWORD pid = k ? 4 : 0;
        SET_ERR(0);
        HANDLE s = snap(TH32CS_SNAPMODULE, pid ? pid : 4);
        if (s == INVALID_HANDLE_VALUE) {
            DWORD e = GetLastError();
            t_note("module snapshot of System: err=%lu", e);
            CHECKF(e == ERROR_PARTIAL_COPY || e == ERROR_ACCESS_DENIED || e == ERROR_BAD_LENGTH || e == ERROR_INVALID_PARAMETER || e == ERROR_NOACCESS || e == 1006, "err=%lu", e);
        } else { t_note("module snapshot of System succeeded"); CloseHandle(s); }
        break;
    }
}
