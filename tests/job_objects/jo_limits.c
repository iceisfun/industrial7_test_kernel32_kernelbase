#include "jc.h"

#define MB (1u << 20)
static BOOL ext_set(HANDLE j, DWORD flags, void (*fill)(JOBOBJECT_EXTENDED_LIMIT_INFORMATION *, void *), void *ctx) {
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION e; memset(&e, 0, sizeof e);
    e.BasicLimitInformation.LimitFlags = flags;
    if (fill) fill(&e, ctx);
    return jc_set_ext(j, &e);
}
static void fill_procmem(JOBOBJECT_EXTENDED_LIMIT_INFORMATION *e, void *c) { e->ProcessMemoryLimit = *(SIZE_T *)c; }
static void fill_jobmem(JOBOBJECT_EXTENDED_LIMIT_INFORMATION *e, void *c) { e->JobMemoryLimit = *(SIZE_T *)c; }
static void fill_active(JOBOBJECT_EXTENDED_LIMIT_INFORMATION *e, void *c) { e->BasicLimitInformation.ActiveProcessLimit = *(DWORD *)c; }
static void fill_ptime(JOBOBJECT_EXTENDED_LIMIT_INFORMATION *e, void *c) { e->BasicLimitInformation.PerProcessUserTimeLimit.QuadPart = *(LONGLONG *)c; }
static void fill_jtime(JOBOBJECT_EXTENDED_LIMIT_INFORMATION *e, void *c) { e->BasicLimitInformation.PerJobUserTimeLimit.QuadPart = *(LONGLONG *)c; }
static void fill_prio(JOBOBJECT_EXTENDED_LIMIT_INFORMATION *e, void *c) { e->BasicLimitInformation.PriorityClass = *(DWORD *)c; }
static void fill_aff(JOBOBJECT_EXTENDED_LIMIT_INFORMATION *e, void *c) { e->BasicLimitInformation.Affinity = *(DWORD_PTR *)c; }

static BOOL alloc_err_ok(DWORD e) { return e == ERROR_COMMITMENT_LIMIT || e == ERROR_NOT_ENOUGH_MEMORY || e == ERROR_NOT_ENOUGH_QUOTA || e == ERROR_OUTOFMEMORY; }

/* ---------- KILL_ON_JOB_CLOSE ---------- */
TEST(joblimit_kill_on_close_terminates_child) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    CHECK(jc_set_flags(j, JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE));
    CHECK(jc_spawn_in_job(j, "jo_sleep", "20000", &pi));
    if (!pi.hProcess) { CloseHandle(j); return; }
    EXPECT_EQ(WaitForSingleObject(pi.hProcess, 300), WAIT_TIMEOUT);
    CloseHandle(j);
    DWORD c = jc_wait_exit(pi.hProcess, 8000);
    CHECK(c != JC_TIMEOUT);
    t_note("exit code of child killed by job close: 0x%lx", c);
    jc_kill(&pi);
}
TEST(joblimit_kill_on_close_second_handle_keeps_alive) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    CHECK(jc_set_flags(j, JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE));
    CHECK(jc_spawn_in_job(j, "jo_sleep", "20000", &pi));
    HANDLE d = jc_dup(j, JOB_OBJECT_ALL_ACCESS); CHECK(d);
    CloseHandle(j);
    EXPECT_EQ(WaitForSingleObject(pi.hProcess, 700), WAIT_TIMEOUT);
    CloseHandle(d);
    CHECK(jc_wait_exit(pi.hProcess, 8000) != JC_TIMEOUT);
    jc_kill(&pi);
}
TEST(joblimit_no_kill_without_flag) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    CHECK(jc_spawn_in_job(j, "jo_sleep", "20000", &pi));
    CloseHandle(j);
    EXPECT_EQ(WaitForSingleObject(pi.hProcess, 800), WAIT_TIMEOUT);
    BOOL in = FALSE; CHECK(U(IsProcessInJob)(pi.hProcess, NULL, &in)); CHECK(in);   /* still a job member */
    jc_kill(&pi);
}
TEST(joblimit_kill_on_close_kills_grandchildren) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    CHECK(jc_set_flags(j, JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE));
    CHECK(jc_spawn_in_job(j, "jo_spawn", "0 2 1", &pi));
    CHECK(jc_wait_active(j, 3, 15000));
    DWORD ids[8]; int n = jc_pids(j, ids, 8, NULL); HANDLE h[8] = {0};
    for (int i = 0; i < n; i++) h[i] = OpenProcess(SYNCHRONIZE, FALSE, ids[i]);
    CloseHandle(j);
    for (int i = 0; i < n; i++) if (h[i]) { EXPECT_EQ(WaitForSingleObject(h[i], 8000), WAIT_OBJECT_0); CloseHandle(h[i]); }
    jc_kill(&pi);
}
TEST(joblimit_kill_on_close_cleared_before_close) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    CHECK(jc_set_flags(j, JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE));
    CHECK(jc_spawn_in_job(j, "jo_sleep", "20000", &pi));
    CHECK(jc_set_flags(j, 0));
    CloseHandle(j);
    EXPECT_EQ(WaitForSingleObject(pi.hProcess, 800), WAIT_TIMEOUT);
    jc_kill(&pi);
}
TEST(joblimit_kill_on_close_unnamed_dup_into_child_irrelevant) {
    /* closing the creator's handle while the child itself holds an inheritable copy keeps the job alive */
    REQUIRE(U(SetInformationJobObject)); REQUIRE(U(CreateJobObjectW));
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE j = U(CreateJobObjectW)(&sa, NULL); CHECK(j); if (!j) return;
    CHECK(jc_set_flags(j, JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE));
    PROCESS_INFORMATION pi; char args[64]; snprintf(args, sizeof args, "%llx", (unsigned long long)(ULONG_PTR)j);
    HANDLE p = t_spawn_child("jo_sleep", "6000", CREATE_SUSPENDED, NULL, NULL, NULL, TRUE, NULL, &pi); CHECK(p);
    if (p) {
        CHECK(U(AssignProcessToJobObject)(j, p)); ResumeThread(pi.hThread);
        CloseHandle(j);   /* child holds an inherited copy of the handle */
        EXPECT_EQ(WaitForSingleObject(p, 800), WAIT_TIMEOUT);
        TerminateProcess(p, 1); jc_kill(&pi);
    } else CloseHandle(j);
}

/* ---------- ActiveProcessLimit ---------- */
TEST(joblimit_active_process_child_creation) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(), port = jc_port(); PROCESS_INFORMATION pi; CHECK(j && port); if (!j || !port) return;
    DWORD lim = 2; CHECK(ext_set(j, JOB_OBJECT_LIMIT_ACTIVE_PROCESS, fill_active, &lim));
    CHECK(jc_assoc(j, port, 0x77));
    /* the child counts as one; it tries to make 3 more */
    CHECK(jc_spawn_in_job(j, "jo_spawn", "0 3 1", &pi));
    if (pi.hProcess) {
        DWORD c = jc_wait_exit(pi.hProcess, 15000);
        CHECKF(c != JC_TIMEOUT && c != 0 && c != 0x100, "child result 0x%lx", c);
        t_note("spawn beyond active process limit: result 0x%lx", c);
        DWORD seen = 0; int got = jc_wait_msg(port, JOB_OBJECT_MSG_ACTIVE_PROCESS_LIMIT, 0, 3000, &seen);
        CHECKF(got, "no ACTIVE_PROCESS_LIMIT message, seen mask 0x%lx", seen);
        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION a; CHECK(jc_acct(j, &a));
        CHECKF(a.ActiveProcesses <= 2, "active %lu", a.ActiveProcesses);
        U(TerminateJobObject)(j, 1); jc_kill(&pi);
    }
    CloseHandle(port); CloseHandle(j);
}
TEST(joblimit_active_process_one_blocks_any_child) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    DWORD lim = 1; CHECK(ext_set(j, JOB_OBJECT_LIMIT_ACTIVE_PROCESS, fill_active, &lim));
    CHECK(jc_spawn_in_job(j, "jo_spawn", "0 1 1", &pi));
    if (pi.hProcess) {
        DWORD c = jc_wait_exit(pi.hProcess, 15000);
        CHECKF(c != JC_TIMEOUT && c != 0 && c != 0x100, "child result 0x%lx", c);
        t_note("limit 1 result 0x%lx", c);
        U(TerminateJobObject)(j, 1); jc_kill(&pi);
    }
    CloseHandle(j);
}
TEST(joblimit_active_process_limit_allows_up_to_limit) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    DWORD lim = 3; CHECK(ext_set(j, JOB_OBJECT_LIMIT_ACTIVE_PROCESS, fill_active, &lim));
    CHECK(jc_spawn_in_job(j, "jo_spawn", "0 2 1", &pi));    /* 1 + 2 = 3 */
    if (pi.hProcess) {
        CHECK(jc_wait_active(j, 3, 15000));
        EXPECT_EQ(WaitForSingleObject(pi.hProcess, 300), WAIT_TIMEOUT);
        U(TerminateJobObject)(j, 1); jc_kill(&pi);
    }
    CloseHandle(j);
}
TEST(joblimit_active_process_lowering_does_not_kill) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION p1, p2; CHECK(j); if (!j) return;
    CHECK(jc_spawn_in_job(j, "jo_sleep", "20000", &p1)); CHECK(jc_spawn_in_job(j, "jo_sleep", "20000", &p2));
    DWORD lim = 1; SetLastError(0);
    BOOL r = ext_set(j, JOB_OBJECT_LIMIT_ACTIVE_PROCESS, fill_active, &lim);
    t_note("lower active limit below current count: ret %d err %lu", r, r ? 0 : GetLastError());
    EXPECT_EQ(WaitForSingleObject(p1.hProcess, 400), WAIT_TIMEOUT); EXPECT_EQ(WaitForSingleObject(p2.hProcess, 10), WAIT_TIMEOUT);
    jc_kill(&p1); jc_kill(&p2); CloseHandle(j);
}

/* ---------- Memory limits ---------- */
TEST(joblimit_process_memory_blocks_big_commit) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    SIZE_T lim = 64 * MB; CHECK(ext_set(j, JOB_OBJECT_LIMIT_PROCESS_MEMORY, fill_procmem, &lim));
    CHECK(jc_spawn_in_job(j, "jo_alloc", "134217728", &pi));      /* 128 MB */
    if (pi.hProcess) {
        DWORD c = jc_wait_exit(pi.hProcess, 15000);
        CHECKF(c >= 0x1000 && c < 0x1000 + 0x10000, "child result 0x%lx", c);
        if (c >= 0x1000) CHECKF(alloc_err_ok(c - 0x1000), "alloc error %lu", c - 0x1000);
        t_note("128MB commit with 64MB process limit: error %ld", (long)c - 0x1000);
        jc_kill(&pi);
    }
    CloseHandle(j);
}
TEST(joblimit_process_memory_allows_small_commit) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    SIZE_T lim = 64 * MB; CHECK(ext_set(j, JOB_OBJECT_LIMIT_PROCESS_MEMORY, fill_procmem, &lim));
    CHECK(jc_spawn_in_job(j, "jo_alloc", "16777216", &pi));       /* 16 MB */
    if (pi.hProcess) { EXPECT_EQ(jc_wait_exit(pi.hProcess, 15000), 0); jc_kill(&pi); }
    CloseHandle(j);
}
TEST(joblimit_process_memory_reserve_not_limited) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    SIZE_T lim = 64 * MB; CHECK(ext_set(j, JOB_OBJECT_LIMIT_PROCESS_MEMORY, fill_procmem, &lim));
    CHECK(jc_spawn_in_job(j, "jo_reserve_commit", "536870912 8388608", &pi));   /* reserve 512 MB, commit 8 MB */
    if (pi.hProcess) { EXPECT_EQ(jc_wait_exit(pi.hProcess, 15000), 0); jc_kill(&pi); }
    CloseHandle(j);
}
TEST(joblimit_process_memory_commit_of_reserved_blocked) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    SIZE_T lim = 64 * MB; CHECK(ext_set(j, JOB_OBJECT_LIMIT_PROCESS_MEMORY, fill_procmem, &lim));
    CHECK(jc_spawn_in_job(j, "jo_reserve_commit", "268435456 201326592", &pi));   /* reserve 256 MB, commit 192 MB */
    if (pi.hProcess) {
        DWORD c = jc_wait_exit(pi.hProcess, 15000);
        CHECKF((c >> 16) == 2, "child result 0x%lx", c);
        if ((c >> 16) == 2) CHECKF(alloc_err_ok(c & 0xFFFF), "commit error %lu", c & 0xFFFF);
        jc_kill(&pi);
    }
    CloseHandle(j);
}
TEST(joblimit_no_memory_limit_allows_big_commit) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    CHECK(jc_set_flags(j, JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE));
    CHECK(jc_spawn_in_job(j, "jo_alloc", "134217728", &pi));
    if (pi.hProcess) { DWORD c = jc_wait_exit(pi.hProcess, 20000); CHECKF(c == 0, "result 0x%lx (low-commit host?)", c); jc_kill(&pi); }
    CloseHandle(j);
}
TEST(joblimit_process_memory_applies_per_process) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION p1, p2; CHECK(j); if (!j) return;
    SIZE_T lim = 96 * MB; CHECK(ext_set(j, JOB_OBJECT_LIMIT_PROCESS_MEMORY, fill_procmem, &lim));
    CHECK(jc_spawn_in_job(j, "jo_allochold", "50331648 3000", &p1));     /* 48 MB held */
    CHECK(jc_spawn_in_job(j, "jo_alloc", "50331648", &p2));              /* another 48 MB in a second process */
    if (p1.hProcess && p2.hProcess) { EXPECT_EQ(jc_wait_exit(p2.hProcess, 15000), 0); }
    jc_kill(&p1); jc_kill(&p2); CloseHandle(j);
}
TEST(joblimit_job_memory_blocks_second_process) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION p1, p2; CHECK(j); if (!j) return;
    SIZE_T lim = 96 * MB; CHECK(ext_set(j, JOB_OBJECT_LIMIT_JOB_MEMORY, fill_jobmem, &lim));
    CHECK(jc_spawn_in_job(j, "jo_allochold", "50331648 20000", &p1));    /* 48 MB */
    if (!p1.hProcess) { CloseHandle(j); return; }
    /* wait for the first child to commit */
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION e; DWORD t0 = GetTickCount(); SIZE_T peak = 0;
    while (GetTickCount() - t0 < 10000) {
        if (U(QueryInformationJobObject)(j, JobObjectExtendedLimitInformation, &e, sizeof e, NULL)) peak = e.PeakJobMemoryUsed;
        if (peak >= 48 * MB) break;
        if (WaitForSingleObject(p1.hProcess, 0) == WAIT_OBJECT_0) break;
        Sleep(50);
    }
    CHECKF(peak >= 48 * MB, "peak job memory %llu", (unsigned long long)peak);
    EXPECT_EQ(WaitForSingleObject(p1.hProcess, 0), WAIT_TIMEOUT);
    CHECK(jc_spawn_in_job(j, "jo_alloc", "50331648", &p2));              /* would exceed 96 MB in total */
    if (p2.hProcess) {
        DWORD c = jc_wait_exit(p2.hProcess, 15000);
        CHECKF(c >= 0x1000 && c < 0x20000, "second child result 0x%lx", c);
        if (c >= 0x1000) CHECKF(alloc_err_ok(c - 0x1000), "alloc error %lu", c - 0x1000);
    }
    jc_kill(&p1); jc_kill(&p2); CloseHandle(j);
}
TEST(joblimit_job_memory_peak_tracking) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    SIZE_T lim = 512 * MB; CHECK(ext_set(j, JOB_OBJECT_LIMIT_JOB_MEMORY | JOB_OBJECT_LIMIT_PROCESS_MEMORY, fill_jobmem, &lim));
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION e; memset(&e, 0, sizeof e);
    e.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_JOB_MEMORY | JOB_OBJECT_LIMIT_PROCESS_MEMORY; e.JobMemoryLimit = lim; e.ProcessMemoryLimit = lim;
    CHECK(jc_set_ext(j, &e));
    CHECK(jc_spawn_in_job(j, "jo_alloc", "33554432", &pi));
    if (pi.hProcess) {
        EXPECT_EQ(jc_wait_exit(pi.hProcess, 15000), 0);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION q; memset(&q, 0, sizeof q);
        CHECK(U(QueryInformationJobObject)(j, JobObjectExtendedLimitInformation, &q, sizeof q, NULL));
        CHECKF(q.PeakProcessMemoryUsed >= 32 * MB, "peak process %llu", (unsigned long long)q.PeakProcessMemoryUsed);
        CHECKF(q.PeakJobMemoryUsed >= 32 * MB, "peak job %llu", (unsigned long long)q.PeakJobMemoryUsed);
        CHECK(q.PeakJobMemoryUsed >= q.PeakProcessMemoryUsed || q.PeakJobMemoryUsed >= 32 * MB);
        jc_kill(&pi);
    }
    CloseHandle(j);
}
TEST(joblimit_memory_limit_notification) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(), port = jc_port(); PROCESS_INFORMATION pi; CHECK(j && port); if (!j || !port) return;
    SIZE_T lim = 64 * MB; CHECK(ext_set(j, JOB_OBJECT_LIMIT_PROCESS_MEMORY, fill_procmem, &lim));
    CHECK(jc_assoc(j, port, 0x99));
    CHECK(jc_spawn_in_job(j, "jo_alloc", "268435456", &pi));
    if (pi.hProcess) {
        jc_wait_exit(pi.hProcess, 15000);
        DWORD seen = 0; int got = jc_wait_msg(port, JOB_OBJECT_MSG_PROCESS_MEMORY_LIMIT, pi.dwProcessId, 3000, &seen);
        CHECKF(got, "no PROCESS_MEMORY_LIMIT message; seen mask 0x%lx", seen);
        jc_kill(&pi);
    }
    CloseHandle(port); CloseHandle(j);
}
TEST(joblimit_job_memory_limit_notification) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(), port = jc_port(); PROCESS_INFORMATION pi; CHECK(j && port); if (!j || !port) return;
    SIZE_T lim = 64 * MB; CHECK(ext_set(j, JOB_OBJECT_LIMIT_JOB_MEMORY, fill_jobmem, &lim));
    CHECK(jc_assoc(j, port, 0x98));
    CHECK(jc_spawn_in_job(j, "jo_alloc", "268435456", &pi));
    if (pi.hProcess) {
        jc_wait_exit(pi.hProcess, 15000);
        DWORD seen = 0; int got = jc_wait_msg(port, JOB_OBJECT_MSG_JOB_MEMORY_LIMIT, pi.dwProcessId, 3000, &seen);
        CHECKF(got, "no JOB_MEMORY_LIMIT message; seen mask 0x%lx", seen);
        jc_kill(&pi);
    }
    CloseHandle(port); CloseHandle(j);
}

/* ---------- Priority / affinity ---------- */
TEST(joblimit_priority_class_applied) {
    REQUIRE(U(SetInformationJobObject));
    static const DWORD pc[] = { IDLE_PRIORITY_CLASS, BELOW_NORMAL_PRIORITY_CLASS, ABOVE_NORMAL_PRIORITY_CLASS };
    for (unsigned i = 0; i < sizeof pc / sizeof *pc; i++) {
        HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
        DWORD v = pc[i]; CHECK(ext_set(j, JOB_OBJECT_LIMIT_PRIORITY_CLASS, fill_prio, &v));
        CHECK(jc_spawn_in_job(j, "jo_prio", "", &pi));
        if (pi.hProcess) {
            DWORD c = jc_wait_exit(pi.hProcess, 15000);
            CHECKF((c >> 16) == pc[i], "class 0x%lx: child reported initial 0x%lx", pc[i], c >> 16);
            jc_kill(&pi);
        }
        CloseHandle(j);
    }
}
TEST(joblimit_priority_class_cannot_be_raised) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    DWORD v = BELOW_NORMAL_PRIORITY_CLASS; CHECK(ext_set(j, JOB_OBJECT_LIMIT_PRIORITY_CLASS, fill_prio, &v));
    char a[32]; snprintf(a, sizeof a, "%u", HIGH_PRIORITY_CLASS);
    CHECK(jc_spawn_in_job(j, "jo_prio", a, &pi));
    if (pi.hProcess) {
        DWORD c = jc_wait_exit(pi.hProcess, 15000);
        EXPECT_EQ(c >> 16, BELOW_NORMAL_PRIORITY_CLASS);
        CHECKF((c & 0xFFFF) != HIGH_PRIORITY_CLASS, "child raised its priority to HIGH inside the job (0x%lx)", c & 0xFFFF);
        t_note("priority after attempt to raise: 0x%lx", c & 0xFFFF);
        jc_kill(&pi);
    }
    CloseHandle(j);
}
TEST(joblimit_no_priority_limit_leaves_normal) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    CHECK(jc_spawn_in_job(j, "jo_prio", "", &pi));
    if (pi.hProcess) {
        DWORD c = jc_wait_exit(pi.hProcess, 15000);
        EXPECT_EQ(c >> 16, GetPriorityClass(GetCurrentProcess()));
        jc_kill(&pi);
    }
    CloseHandle(j);
}
TEST(joblimit_affinity_applied) {
    REQUIRE(U(SetInformationJobObject)); REQUIRE(U(GetProcessAffinityMask));
    DWORD_PTR pm = 0, sm = 0; CHECK(U(GetProcessAffinityMask)(GetCurrentProcess(), &pm, &sm));
    DWORD_PTR one = pm & (~pm + 1);
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    CHECK(ext_set(j, JOB_OBJECT_LIMIT_AFFINITY, fill_aff, &one));
    CHECK(jc_spawn_in_job(j, "jo_aff", "", &pi));
    if (pi.hProcess) {
        DWORD c = jc_wait_exit(pi.hProcess, 15000);
        EXPECT_EQ(c & 0xFFFF, (DWORD)one & 0xFFFF);
        EXPECT_EQ(c >> 16, (DWORD)one & 0xFFFF);      /* cannot widen */
        jc_kill(&pi);
    }
    CloseHandle(j);
}

/* ---------- CPU time limits ---------- */
TEST(joblimit_process_time_kills_spinner) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(), port = jc_port(); PROCESS_INFORMATION pi; CHECK(j && port); if (!j || !port) return;
    LONGLONG lim = 3000000;   /* 300 ms */
    CHECK(ext_set(j, JOB_OBJECT_LIMIT_PROCESS_TIME, fill_ptime, &lim));
    CHECK(jc_assoc(j, port, 0x55));
    CHECK(jc_spawn_in_job(j, "jo_spin", "30000", &pi));
    if (pi.hProcess) {
        DWORD c = jc_wait_exit(pi.hProcess, 14000);
        CHECKF(c != JC_TIMEOUT && c != 0, "spinner not killed (result 0x%lx)", c);
        t_note("process killed by time limit, exit code 0x%lx", c);
        DWORD seen = 0; int got = jc_wait_msg(port, JOB_OBJECT_MSG_END_OF_PROCESS_TIME, pi.dwProcessId, 3000, &seen);
        CHECKF(got, "no END_OF_PROCESS_TIME message; seen mask 0x%lx", seen);
        jc_kill(&pi);
    }
    CloseHandle(port); CloseHandle(j);
}
TEST(joblimit_job_time_terminates_and_signals) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    LONGLONG lim = 3000000;
    CHECK(ext_set(j, JOB_OBJECT_LIMIT_JOB_TIME, fill_jtime, &lim));
    EXPECT_EQ(WaitForSingleObject(j, 0), WAIT_TIMEOUT);
    CHECK(jc_spawn_in_job(j, "jo_spin", "30000", &pi));
    if (pi.hProcess) {
        DWORD c = jc_wait_exit(pi.hProcess, 14000);
        CHECKF(c != JC_TIMEOUT && c != 0, "spinner not killed (result 0x%lx)", c);
        t_note("process killed by job time limit, exit code 0x%lx", c);
        DWORD w = WaitForSingleObject(j, 3000);
        CHECKF(w == WAIT_OBJECT_0, "job not signaled after job time limit (wait=%lu)", w);
        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION a; CHECK(jc_acct(j, &a));
        EXPECT_EQ(a.ActiveProcesses, 0);
        jc_kill(&pi);
    }
    CloseHandle(j);
}
TEST(joblimit_job_time_post_does_not_terminate) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(), port = jc_port(); PROCESS_INFORMATION pi; CHECK(j && port); if (!j || !port) return;
    LONGLONG lim = 3000000;
    CHECK(ext_set(j, JOB_OBJECT_LIMIT_JOB_TIME, fill_jtime, &lim));
    JOBOBJECT_END_OF_JOB_TIME_INFORMATION t = { JOB_OBJECT_POST_AT_END_OF_JOB };
    CHECK(U(SetInformationJobObject)(j, JobObjectEndOfJobTimeInformation, &t, sizeof t));
    CHECK(jc_assoc(j, port, 0x56));
    CHECK(jc_spawn_in_job(j, "jo_spin", "9000", &pi));
    if (pi.hProcess) {
        DWORD seen = 0; int got = jc_wait_msg(port, JOB_OBJECT_MSG_END_OF_JOB_TIME, 0, 12000, &seen);
        CHECKF(got, "no END_OF_JOB_TIME message; seen mask 0x%lx", seen);
        if (got) EXPECT_EQ(WaitForSingleObject(pi.hProcess, 200), WAIT_TIMEOUT);   /* still running */
        U(TerminateJobObject)(j, 3); jc_kill(&pi);
    }
    CloseHandle(port); CloseHandle(j);
}
TEST(joblimit_time_limits_do_not_hit_sleepers) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    LONGLONG lim = 3000000;
    CHECK(ext_set(j, JOB_OBJECT_LIMIT_PROCESS_TIME | JOB_OBJECT_LIMIT_JOB_TIME, fill_jtime, &lim));
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION e; memset(&e, 0, sizeof e);
    e.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_TIME | JOB_OBJECT_LIMIT_JOB_TIME;
    e.BasicLimitInformation.PerProcessUserTimeLimit.QuadPart = lim; e.BasicLimitInformation.PerJobUserTimeLimit.QuadPart = lim;
    CHECK(jc_set_ext(j, &e));
    CHECK(jc_spawn_in_job(j, "jo_sleep", "1500", &pi));
    if (pi.hProcess) { EXPECT_EQ(jc_wait_exit(pi.hProcess, 12000), 0); jc_kill(&pi); }
    CloseHandle(j);
}

/* ---------- Accounting growth ---------- */
TEST(joballocount_user_time_grows) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    CHECK(jc_spawn_in_job(j, "jo_spin", "600", &pi));
    if (pi.hProcess) {
        EXPECT_EQ(jc_wait_exit(pi.hProcess, 15000), 0);
        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION a; CHECK(jc_acct(j, &a));
        CHECKF(a.TotalUserTime.QuadPart > 0, "user time %lld", a.TotalUserTime.QuadPart);
        CHECK(a.TotalPageFaultCount > 0);
        EXPECT_EQ(a.TotalProcesses, 1); EXPECT_EQ(a.ActiveProcesses, 0);
        CHECK(a.ThisPeriodTotalUserTime.QuadPart <= a.TotalUserTime.QuadPart);
        t_note("user %lld kernel %lld faults %lu", a.TotalUserTime.QuadPart, a.TotalKernelTime.QuadPart, a.TotalPageFaultCount);
        FILETIME c, e, k, u; GetProcessTimes(pi.hProcess, &c, &e, &k, &u);
        ULARGE_INTEGER uu; uu.LowPart = u.dwLowDateTime; uu.HighPart = u.dwHighDateTime;
        CHECKF((ULONGLONG)a.TotalUserTime.QuadPart >= uu.QuadPart, "job user %lld < process user %llu", a.TotalUserTime.QuadPart, uu.QuadPart);
        jc_kill(&pi);
    }
    CloseHandle(j);
}
TEST(joballocount_monotonic_while_running) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    CHECK(jc_spawn_in_job(j, "jo_spin", "1500", &pi));
    if (pi.hProcess) {
        LONGLONG prev = 0; int regress = 0;
        for (int i = 0; i < 12; i++) {
            JOBOBJECT_BASIC_ACCOUNTING_INFORMATION a; CHECK(jc_acct(j, &a));
            if (a.TotalUserTime.QuadPart < prev) regress++;
            prev = a.TotalUserTime.QuadPart; Sleep(100);
        }
        EXPECT_EQ(regress, 0);
        CHECK(prev > 0);
        jc_wait_exit(pi.hProcess, 8000); jc_kill(&pi);
    }
    CloseHandle(j);
}
TEST(joballocount_basic_and_io_matches_basic) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    CHECK(jc_spawn_in_job(j, "jo_exit", "0", &pi));
    if (pi.hProcess) {
        jc_wait_exit(pi.hProcess, 8000);
        JOBOBJECT_BASIC_AND_IO_ACCOUNTING_INFORMATION x; DWORD rl = 0;
        CHECK(U(QueryInformationJobObject)(j, JobObjectBasicAndIoAccountingInformation, &x, sizeof x, &rl));
        EXPECT_EQ(rl, sizeof x);
        EXPECT_EQ(x.BasicInfo.TotalProcesses, 1); EXPECT_EQ(x.BasicInfo.ActiveProcesses, 0);
        t_note("job io: reads %llu writes %llu other %llu", (unsigned long long)x.IoInfo.ReadOperationCount, (unsigned long long)x.IoInfo.WriteOperationCount, (unsigned long long)x.IoInfo.OtherOperationCount);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION e; CHECK(U(QueryInformationJobObject)(j, JobObjectExtendedLimitInformation, &e, sizeof e, NULL));
        EXPECT_EQ(e.IoInfo.ReadOperationCount, x.IoInfo.ReadOperationCount);
        jc_kill(&pi);
    }
    CloseHandle(j);
}
TEST(joballocount_io_counts_file_write) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    CHECK(jc_spawn_in_job(j, "jo_spin", "200", &pi));
    if (pi.hProcess) {
        jc_wait_exit(pi.hProcess, 10000);
        JOBOBJECT_BASIC_AND_IO_ACCOUNTING_INFORMATION x;
        CHECK(U(QueryInformationJobObject)(j, JobObjectBasicAndIoAccountingInformation, &x, sizeof x, NULL));
        { ULONGLONG tot = x.IoInfo.OtherOperationCount + x.IoInfo.ReadOperationCount + x.IoInfo.WriteOperationCount;
          CHECKF(tot > 0, "a process that started and ran has zero accounted I/O operations"); }
        jc_kill(&pi);
    }
    CloseHandle(j);
}
TEST(joballocount_total_counts_survive_exit) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    enum { N = 4 }; PROCESS_INFORMATION pi[N];
    for (int i = 0; i < N; i++) CHECK(jc_spawn_in_job(j, "jo_exit", "0", &pi[i]));
    for (int i = 0; i < N; i++) if (pi[i].hProcess) { jc_wait_exit(pi[i].hProcess, 8000); jc_kill(&pi[i]); }
    CHECK(jc_wait_active(j, 0, 3000));
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION a; CHECK(jc_acct(j, &a));
    EXPECT_EQ(a.TotalProcesses, N); EXPECT_EQ(a.ActiveProcesses, 0);
    CloseHandle(j);
}
TEST(joballocount_terminated_counts_limit_kills) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    LONGLONG lim = 3000000; CHECK(ext_set(j, JOB_OBJECT_LIMIT_PROCESS_TIME, fill_ptime, &lim));
    CHECK(jc_spawn_in_job(j, "jo_spin", "30000", &pi));
    if (pi.hProcess) {
        jc_wait_exit(pi.hProcess, 14000);
        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION a; CHECK(jc_acct(j, &a));
        EXPECT_EQ(a.TotalTerminatedProcesses, 1);
        jc_kill(&pi);
    }
    CloseHandle(j);
}
TEST(joballocount_this_period_reset_via_job_time_set) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    CHECK(jc_spawn_in_job(j, "jo_spin", "500", &pi));
    if (pi.hProcess) {
        jc_wait_exit(pi.hProcess, 10000);
        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION a, b; CHECK(jc_acct(j, &a));
        LONGLONG lim = 600000000; CHECK(ext_set(j, JOB_OBJECT_LIMIT_JOB_TIME, fill_jtime, &lim));
        CHECK(jc_acct(j, &b));
        t_note("this-period user time before %lld after setting job time limit %lld (total %lld)", a.ThisPeriodTotalUserTime.QuadPart, b.ThisPeriodTotalUserTime.QuadPart, b.TotalUserTime.QuadPart);
        EXPECT_EQ(b.TotalUserTime.QuadPart, a.TotalUserTime.QuadPart);
        jc_kill(&pi);
    }
    CloseHandle(j);
}

/* ---------- Breakaway ---------- */
#define CREATE_BREAKAWAY 0x01000000
TEST(jobbreak_denied_without_flag) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    CHECK(jc_set_flags(j, JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE));
    CHECK(jc_spawn_in_job(j, "jo_spawn", "0x01000000 1 0", &pi));
    if (pi.hProcess) { EXPECT_EQ(jc_wait_exit(pi.hProcess, 15000), 0x1000 + ERROR_ACCESS_DENIED); jc_kill(&pi); }
    CloseHandle(j);
}
TEST(jobbreak_allowed_with_breakaway_ok) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    CHECK(jc_set_flags(j, JOB_OBJECT_LIMIT_BREAKAWAY_OK | JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE));
    CHECK(jc_spawn_in_job(j, "jo_spawn", "0x01000000 1 0", &pi));
    if (pi.hProcess) {
        DWORD c = jc_wait_exit(pi.hProcess, 15000);
        BOOL self = FALSE; U(IsProcessInJob)(GetCurrentProcess(), NULL, &self);
        if (self) t_note("test process already in a job; breakaway result 0x%lx", c); else EXPECT_EQ(c, 0x100);   /* created, not in any job */
        jc_kill(&pi);
    }
    CloseHandle(j);
}
TEST(jobbreak_default_child_stays_in_job_with_breakaway_ok) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    CHECK(jc_set_flags(j, JOB_OBJECT_LIMIT_BREAKAWAY_OK));
    CHECK(jc_spawn_in_job(j, "jo_spawn", "0 1 0", &pi));
    if (pi.hProcess) { EXPECT_EQ(jc_wait_exit(pi.hProcess, 15000), 0x101); jc_kill(&pi); }
    CloseHandle(j);
}
TEST(jobbreak_silent_breakaway_ok) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    CHECK(jc_set_flags(j, JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK));
    CHECK(jc_spawn_in_job(j, "jo_spawn", "0 1 0", &pi));
    if (pi.hProcess) {
        DWORD c = jc_wait_exit(pi.hProcess, 15000);
        BOOL self = FALSE; U(IsProcessInJob)(GetCurrentProcess(), NULL, &self);
        if (self) t_note("test process already in a job; silent breakaway result 0x%lx", c); else EXPECT_EQ(c, 0x100);
        jc_kill(&pi);
    }
    CloseHandle(j);
}
TEST(jobbreak_silent_with_explicit_flag_observe) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    CHECK(jc_set_flags(j, JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK));
    CHECK(jc_spawn_in_job(j, "jo_spawn", "0x01000000 1 0", &pi));
    if (pi.hProcess) { t_note("explicit breakaway with only SILENT_BREAKAWAY_OK: result 0x%lx", jc_wait_exit(pi.hProcess, 15000)); jc_kill(&pi); }
    CloseHandle(j);
}
TEST(jobbreak_breakaway_job_membership_counts) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    CHECK(jc_set_flags(j, JOB_OBJECT_LIMIT_BREAKAWAY_OK));
    CHECK(jc_spawn_in_job(j, "jo_spawn", "0x01000000 1 1", &pi));
    if (pi.hProcess) {
        Sleep(1500);
        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION a; CHECK(jc_acct(j, &a));
        EXPECT_EQ(a.ActiveProcesses, 1); EXPECT_EQ(a.TotalProcesses, 1);
        U(TerminateJobObject)(j, 1); jc_kill(&pi);
    }
    CloseHandle(j);
}
TEST(jobbreak_flag_without_job_is_harmless) {
    PROCESS_INFORMATION pi;
    HANDLE p = t_spawn_child("jo_spawn", "0x01000000 1 0", 0, NULL, NULL, NULL, FALSE, NULL, &pi);
    CHECK(p);
    if (p) {
        BOOL self = FALSE; U(IsProcessInJob)(GetCurrentProcess(), NULL, &self);
        DWORD c = jc_wait_exit(p, 15000);
        if (self) t_note("in job: result 0x%lx", c); else EXPECT_EQ(c, 0x100);
        CloseHandle(pi.hThread); CloseHandle(p);
    }
}

/* ---------- Unhandled exception policy ---------- */
TEST(jobexc_crash_exit_code_baseline) {
    PROCESS_INFORMATION pi; HANDLE p = t_spawn_child("jo_crash", "q", 0, NULL, NULL, NULL, FALSE, NULL, &pi); CHECK(p);
    if (p) { EXPECT_EQ(jc_wait_exit(p, 20000), 0xC0000005); CloseHandle(pi.hThread); CloseHandle(p); }
}
TEST(jobexc_die_on_unhandled_exception) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    CHECK(jc_set_flags(j, JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION));
    CHECK(jc_spawn_in_job(j, "jo_crash", "", &pi));
    if (pi.hProcess) { EXPECT_EQ(jc_wait_exit(pi.hProcess, 20000), 0xC0000005); jc_kill(&pi); }
    CloseHandle(j);
}
TEST(jobexc_abnormal_exit_notification) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(), port = jc_port(); PROCESS_INFORMATION pi; CHECK(j && port); if (!j || !port) return;
    CHECK(jc_assoc(j, port, 0x31));
    CHECK(jc_spawn_in_job(j, "jo_crash", "q", &pi));
    if (pi.hProcess) {
        jc_wait_exit(pi.hProcess, 20000);
        DWORD seen = 0; int got = jc_wait_msg(port, JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS, pi.dwProcessId, 3000, &seen);
        CHECKF(got, "no ABNORMAL_EXIT_PROCESS; seen mask 0x%lx", seen);
        jc_kill(&pi);
    }
    CloseHandle(port); CloseHandle(j);
}

/* ---------- UI restrictions enforced ---------- */
TEST(jobui_global_atoms_private_table) {
    REQUIRE(U(SetInformationJobObject)); REQUIRE(U(GlobalFindAtomA)); REQUIRE(U(GlobalDeleteAtom));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    JOBOBJECT_BASIC_UI_RESTRICTIONS u = { JOB_OBJECT_UILIMIT_GLOBALATOMS };
    CHECK(U(SetInformationJobObject)(j, JobObjectBasicUIRestrictions, &u, sizeof u));
    char atom[64], ready[96], go[96]; jc_name(atom, sizeof atom, "atom"); jc_name(ready, sizeof ready, "rdy"); jc_name(go, sizeof go, "go");
    HANDLE er = CreateEventA(NULL, TRUE, FALSE, ready), eg = CreateEventA(NULL, TRUE, FALSE, go);
    char args[300]; snprintf(args, sizeof args, "%s %s %s", atom, ready, go);
    CHECK(jc_spawn_in_job(j, "jo_atom", args, &pi));
    if (pi.hProcess) {
        DWORD w = WaitForSingleObject(er, 10000); CHECK(w == WAIT_OBJECT_0);
        if (w == WAIT_OBJECT_0) {
            ATOM a = U(GlobalFindAtomA)(atom);
            CHECKF(a == 0, "atom added by restricted child is visible outside the job (atom 0x%x)", a);
            if (a) U(GlobalDeleteAtom)(a);
        }
        SetEvent(eg);
        EXPECT_EQ(jc_wait_exit(pi.hProcess, 10000), 0);
        jc_kill(&pi);
    }
    CloseHandle(er); CloseHandle(eg); CloseHandle(j);
}
TEST(jobui_desktop_creation_blocked) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    JOBOBJECT_BASIC_UI_RESTRICTIONS u = { JOB_OBJECT_UILIMIT_DESKTOP };
    CHECK(U(SetInformationJobObject)(j, JobObjectBasicUIRestrictions, &u, sizeof u));
    CHECK(jc_spawn_in_job(j, "jo_desktop", "", &pi));
    if (pi.hProcess) {
        DWORD c = jc_wait_exit(pi.hProcess, 15000);
        CHECKF(c >= 0x1000 && c != JC_TIMEOUT, "CreateDesktop in restricted job result 0x%lx", c);
        if (c >= 0x1000 && c < 0x2000) t_note("CreateDesktop restricted error %lu", c - 0x1000);
        jc_kill(&pi);
    }
    CloseHandle(j);
}
TEST(jobui_desktop_creation_unrestricted_observe) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    CHECK(jc_spawn_in_job(j, "jo_desktop", "", &pi));
    if (pi.hProcess) { t_note("CreateDesktop unrestricted result 0x%lx", jc_wait_exit(pi.hProcess, 15000)); jc_kill(&pi); }
    CloseHandle(j);
}
TEST(jobui_system_parameters_blocked) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    JOBOBJECT_BASIC_UI_RESTRICTIONS u = { JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS };
    CHECK(U(SetInformationJobObject)(j, JobObjectBasicUIRestrictions, &u, sizeof u));
    CHECK(jc_spawn_in_job(j, "jo_spi", "", &pi));
    if (pi.hProcess) {
        DWORD c = jc_wait_exit(pi.hProcess, 15000);
        CHECKF(c == 0x1000 + ERROR_ACCESS_DENIED, "SPI_SETBEEP in restricted job result 0x%lx", c);
        jc_kill(&pi);
    }
    CloseHandle(j);
}
TEST(jobui_system_parameters_unrestricted_observe) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    CHECK(jc_spawn_in_job(j, "jo_spi", "", &pi));
    if (pi.hProcess) { t_note("SPI_SETBEEP(current) unrestricted result 0x%lx", jc_wait_exit(pi.hProcess, 15000)); jc_kill(&pi); }
    CloseHandle(j);
}
TEST(jobui_display_settings_test_observe) {
    REQUIRE(U(SetInformationJobObject));
    for (int restricted = 0; restricted < 2; restricted++) {
        HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
        JOBOBJECT_BASIC_UI_RESTRICTIONS u = { restricted ? JOB_OBJECT_UILIMIT_DISPLAYSETTINGS : 0 };
        CHECK(U(SetInformationJobObject)(j, JobObjectBasicUIRestrictions, &u, sizeof u));
        CHECK(jc_spawn_in_job(j, "jo_display", "", &pi));
        if (pi.hProcess) { t_note("CDS_TEST current mode, restricted=%d: result %ld", restricted, (long)jc_wait_exit(pi.hProcess, 15000) - 100); jc_kill(&pi); }
        CloseHandle(j);
    }
}
TEST(jobui_handles_foreign_window_observe) {
    REQUIRE(U(SetInformationJobObject)); REQUIRE(U(CreateWindowExA));
    HWND w = U(CreateWindowExA)(0, "STATIC", "k32t_jo", WS_POPUP, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    if (!w) { t_note("no window station for window creation (%lu)", GetLastError()); return; }
    for (int restricted = 0; restricted < 2; restricted++) {
        HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) break;
        JOBOBJECT_BASIC_UI_RESTRICTIONS u = { restricted ? JOB_OBJECT_UILIMIT_HANDLES : 0 };
        CHECK(U(SetInformationJobObject)(j, JobObjectBasicUIRestrictions, &u, sizeof u));
        char a[32]; snprintf(a, sizeof a, "%llx", (unsigned long long)(ULONG_PTR)w);
        CHECK(jc_spawn_in_job(j, "jo_hwnd", a, &pi));
        if (pi.hProcess) { t_note("GetWindowThreadProcessId(foreign hwnd), UILIMIT_HANDLES=%d: result 0x%lx", restricted, jc_wait_exit(pi.hProcess, 15000)); jc_kill(&pi); }
        CloseHandle(j);
    }
    U(DestroyWindow)(w);
}

/* ---------- Handle inheritance ---------- */
TEST(jobinherit_inheritable_handle_usable_in_child) {
    REQUIRE(U(CreateJobObjectW));
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE j = U(CreateJobObjectW)(&sa, NULL); CHECK(j); if (!j) return;
    char a[32]; snprintf(a, sizeof a, "%llx", (unsigned long long)(ULONG_PTR)j);
    PROCESS_INFORMATION pi;
    HANDLE p = t_spawn_child("jo_inherit", a, 0, NULL, NULL, NULL, TRUE, NULL, &pi); CHECK(p);
    if (p) { EXPECT_EQ(jc_wait_exit(p, 15000), 0); CloseHandle(pi.hThread); CloseHandle(p); }
    CloseHandle(j);
}
TEST(jobinherit_non_inheritable_not_in_child) {
    REQUIRE(U(CreateJobObjectW));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    char a[32]; snprintf(a, sizeof a, "%llx", (unsigned long long)(ULONG_PTR)j);
    PROCESS_INFORMATION pi;
    HANDLE p = t_spawn_child("jo_inherit", a, 0, NULL, NULL, NULL, TRUE, NULL, &pi); CHECK(p);
    if (p) { DWORD c = jc_wait_exit(p, 15000); CHECKF(c != 0 && c != JC_TIMEOUT, "child reached non-inheritable job via value (0x%lx)", c); CloseHandle(pi.hThread); CloseHandle(p); }
    CloseHandle(j);
}
TEST(jobinherit_inherit_flag_ignored_when_bInheritHandles_false) {
    REQUIRE(U(CreateJobObjectW));
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE j = U(CreateJobObjectW)(&sa, NULL); CHECK(j); if (!j) return;
    char a[32]; snprintf(a, sizeof a, "%llx", (unsigned long long)(ULONG_PTR)j);
    PROCESS_INFORMATION pi;
    HANDLE p = t_spawn_child("jo_inherit", a, 0, NULL, NULL, NULL, FALSE, NULL, &pi); CHECK(p);
    if (p) { DWORD c = jc_wait_exit(p, 15000); CHECKF(c != 0 && c != JC_TIMEOUT, "result 0x%lx", c); CloseHandle(pi.hThread); CloseHandle(p); }
    CloseHandle(j);
}
TEST(jobinherit_set_handle_information_toggles) {
    REQUIRE(U(CreateJobObjectW));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    CHECK(SetHandleInformation(j, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT));
    char a[32]; snprintf(a, sizeof a, "%llx", (unsigned long long)(ULONG_PTR)j);
    PROCESS_INFORMATION pi;
    HANDLE p = t_spawn_child("jo_inherit", a, 0, NULL, NULL, NULL, TRUE, NULL, &pi); CHECK(p);
    if (p) { EXPECT_EQ(jc_wait_exit(p, 15000), 0); CloseHandle(pi.hThread); CloseHandle(p); }
    CloseHandle(j);
}
