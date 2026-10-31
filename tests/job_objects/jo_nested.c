#include "jc.h"

static void jname_to_w(const char *a, WCHAR *w) { int i = 0; for (; a[i]; i++) w[i] = a[i]; w[i] = 0; }
static HANDLE open_wait(const char *nm, DWORD access, DWORD ms) {
    WCHAR w[128]; jname_to_w(nm, w); HANDLE b = NULL; DWORD t0 = GetTickCount();
    while (!(b = U(OpenJobObjectW)(access, FALSE, w)) && GetTickCount() - t0 < ms) Sleep(30);
    return b;
}
static BOOL set_procmem(HANDLE j, SIZE_T lim) {
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION e; memset(&e, 0, sizeof e);
    e.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY; e.ProcessMemoryLimit = lim;
    return jc_set_ext(j, &e);
}

TEST(jobnest_child_creates_and_joins_own_job) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE a = jc_job(); PROCESS_INFORMATION pi; CHECK(a); if (!a) return;
    CHECK(jc_spawn_in_job(a, "jo_nest2", "", &pi));
    if (pi.hProcess) { EXPECT_EQ(jc_wait_exit(pi.hProcess, 15000), 0); jc_kill(&pi); }
    CloseHandle(a);
}
TEST(jobnest_parent_job_sees_member_of_nested_job) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE a = jc_job(); PROCESS_INFORMATION pi; CHECK(a); if (!a) return;
    char nm[96]; jc_name(nm, sizeof nm, "nsee"); char args[200]; snprintf(args, sizeof args, "%s 0 0 30000", nm);
    CHECK(jc_spawn_in_job(a, "jo_nest", args, &pi));
    if (pi.hProcess) {
        HANDLE b = open_wait(nm, JOB_OBJECT_ALL_ACCESS, 10000); CHECK(b);
        if (b) {
            CHECK(jc_wait_active(b, 1, 5000));
            CHECK(jc_pid_in(b, pi.dwProcessId)); CHECK(jc_pid_in(a, pi.dwProcessId));
            BOOL in = FALSE; CHECK(U(IsProcessInJob)(pi.hProcess, b, &in)); CHECK(in);
            in = FALSE; CHECK(U(IsProcessInJob)(pi.hProcess, a, &in)); CHECK(in);
            JOBOBJECT_BASIC_ACCOUNTING_INFORMATION x; CHECK(jc_acct(a, &x)); EXPECT_EQ(x.ActiveProcesses, 1);
            CHECK(jc_acct(b, &x)); EXPECT_EQ(x.ActiveProcesses, 1); EXPECT_EQ(x.TotalProcesses, 1);
            CloseHandle(b);
        }
        U(TerminateJobObject)(a, 1); jc_kill(&pi);
    }
    CloseHandle(a);
}
TEST(jobnest_terminate_child_job_only) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE a = jc_job(); PROCESS_INFORMATION pi, other; CHECK(a); if (!a) return;
    char nm[96]; jc_name(nm, sizeof nm, "nterm"); char args[200]; snprintf(args, sizeof args, "%s 0 0 30000", nm);
    CHECK(jc_spawn_in_job(a, "jo_nest", args, &pi));
    CHECK(jc_spawn_in_job(a, "jo_sleep", "30000", &other));
    if (pi.hProcess && other.hProcess) {
        HANDLE b = open_wait(nm, JOB_OBJECT_ALL_ACCESS, 10000); CHECK(b);
        if (b) {
            CHECK(jc_wait_active(b, 1, 5000));
            CHECK(U(TerminateJobObject)(b, 0x21));
            EXPECT_EQ(jc_wait_exit(pi.hProcess, 8000), 0x21);
            EXPECT_EQ(WaitForSingleObject(other.hProcess, 300), WAIT_TIMEOUT);   /* sibling in parent job unaffected */
            CHECK(jc_wait_active(a, 1, 3000));
            CloseHandle(b);
        }
    }
    U(TerminateJobObject)(a, 1); jc_kill(&pi); jc_kill(&other); CloseHandle(a);
}
TEST(jobnest_kill_on_close_of_ancestor) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE a = jc_job(); PROCESS_INFORMATION pi; CHECK(a); if (!a) return;
    CHECK(jc_set_flags(a, JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE));
    char nm[96]; jc_name(nm, sizeof nm, "nkoc"); char args[200]; snprintf(args, sizeof args, "%s 0 0 30000", nm);
    CHECK(jc_spawn_in_job(a, "jo_nest", args, &pi));
    if (pi.hProcess) {
        HANDLE b = open_wait(nm, JOB_OBJECT_QUERY, 10000); CHECK(b);
        if (b) { CHECK(jc_wait_active(b, 1, 5000)); CloseHandle(b); }
        CloseHandle(a);
        CHECK(jc_wait_exit(pi.hProcess, 8000) != JC_TIMEOUT);
        jc_kill(&pi);
    } else CloseHandle(a);
}
TEST(jobnest_memory_limit_parent_stricter) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE a = jc_job(); PROCESS_INFORMATION pi; CHECK(a); if (!a) return;
    CHECK(set_procmem(a, 64u << 20));
    char nm[96]; jc_name(nm, sizeof nm, "nmem1"); char args[200];
    snprintf(args, sizeof args, "%s 536870912 134217728 0", nm);    /* child job allows 512 MB, ask for 128 MB */
    CHECK(jc_spawn_in_job(a, "jo_nest", args, &pi));
    if (pi.hProcess) {
        DWORD c = jc_wait_exit(pi.hProcess, 15000);
        CHECKF((c & 0x100000) != 0, "allocation beyond the ancestor's limit succeeded (result 0x%lx)", c);
        jc_kill(&pi);
    }
    CloseHandle(a);
}
TEST(jobnest_memory_limit_child_stricter) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE a = jc_job(); PROCESS_INFORMATION pi; CHECK(a); if (!a) return;
    CHECK(set_procmem(a, 512u << 20));
    char nm[96]; jc_name(nm, sizeof nm, "nmem2"); char args[200];
    snprintf(args, sizeof args, "%s 67108864 134217728 0", nm);
    CHECK(jc_spawn_in_job(a, "jo_nest", args, &pi));
    if (pi.hProcess) {
        DWORD c = jc_wait_exit(pi.hProcess, 15000);
        CHECKF((c & 0x100000) != 0, "allocation beyond the nested job's limit succeeded (result 0x%lx)", c);
        jc_kill(&pi);
    }
    CloseHandle(a);
}
TEST(jobnest_memory_limit_both_allow) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE a = jc_job(); PROCESS_INFORMATION pi; CHECK(a); if (!a) return;
    CHECK(set_procmem(a, 512u << 20));
    char nm[96]; jc_name(nm, sizeof nm, "nmem3"); char args[200];
    snprintf(args, sizeof args, "%s 268435456 16777216 0", nm);
    CHECK(jc_spawn_in_job(a, "jo_nest", args, &pi));
    if (pi.hProcess) { EXPECT_EQ(jc_wait_exit(pi.hProcess, 15000), 0); jc_kill(&pi); }
    CloseHandle(a);
}
TEST(jobnest_second_process_joins_existing_child_job) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE a = jc_job(), b = jc_job(); PROCESS_INFORMATION p1, p2; CHECK(a && b); if (!a || !b) return;
    CHECK(jc_spawn_in_job(a, "jo_sleep", "30000", &p1)); CHECK(jc_spawn_in_job(a, "jo_sleep", "30000", &p2));
    CHECK(U(AssignProcessToJobObject)(b, p1.hProcess));      /* b becomes a child of a */
    SetLastError(0);
    CHECK(U(AssignProcessToJobObject)(b, p2.hProcess));      /* p2 is in a; b is a descendant of a */
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION x; CHECK(jc_acct(b, &x)); EXPECT_EQ(x.ActiveProcesses, 2);
    CHECK(jc_acct(a, &x)); EXPECT_EQ(x.ActiveProcesses, 2);
    U(TerminateJobObject)(a, 1); jc_kill(&p1); jc_kill(&p2); CloseHandle(a); CloseHandle(b);
}
TEST(jobnest_active_limit_hierarchical) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE a = jc_job(), b = jc_job(); PROCESS_INFORMATION p1, p2, p3; CHECK(a && b); if (!a || !b) return;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION e; memset(&e, 0, sizeof e);
    e.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_ACTIVE_PROCESS; e.BasicLimitInformation.ActiveProcessLimit = 2;
    CHECK(jc_set_ext(a, &e));
    CHECK(jc_spawn_in_job(a, "jo_sleep", "30000", &p1)); CHECK(jc_spawn_in_job(a, "jo_sleep", "30000", &p2));
    CHECK(U(AssignProcessToJobObject)(b, p1.hProcess));
    HANDLE h3 = jc_spawn("jo_sleep", "30000", 0, &p3);
    if (h3) {
        SetLastError(0);
        CHECK(!U(AssignProcessToJobObject)(b, h3));      /* third member would exceed a's limit */
        t_note("assign into nested job beyond ancestor active limit: err %lu", GetLastError());
        jc_kill(&p3);
    }
    U(TerminateJobObject)(a, 1); jc_kill(&p1); jc_kill(&p2); CloseHandle(a); CloseHandle(b);
}
TEST(jobnest_unrelated_job_with_process_denied) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE a = jc_job(), b = jc_job(), x = jc_job(); PROCESS_INFORMATION p1, p2; CHECK(a && b && x); if (!a || !b || !x) return;
    CHECK(jc_spawn_in_job(a, "jo_sleep", "30000", &p1)); CHECK(jc_spawn_in_job(x, "jo_sleep", "30000", &p2));
    CHECK(U(AssignProcessToJobObject)(b, p1.hProcess));
    SetLastError(0);
    CHECK(!U(AssignProcessToJobObject)(x, p1.hProcess)); EXPECT_ERR(ERROR_ACCESS_DENIED);   /* x already has an unrelated member */
    SetLastError(0);
    CHECK(!U(AssignProcessToJobObject)(b, p2.hProcess)); EXPECT_ERR(ERROR_ACCESS_DENIED);
    U(TerminateJobObject)(a, 1); U(TerminateJobObject)(x, 1); jc_kill(&p1); jc_kill(&p2); CloseHandle(a); CloseHandle(b); CloseHandle(x);
}
TEST(jobnest_empty_unrelated_job_can_join_chain) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE a = jc_job(), b = jc_job(), c = jc_job(); PROCESS_INFORMATION p1; CHECK(a && b && c); if (!a || !b || !c) return;
    CHECK(jc_spawn_in_job(a, "jo_sleep", "30000", &p1));
    CHECK(U(AssignProcessToJobObject)(b, p1.hProcess));
    SetLastError(0);
    BOOL r = U(AssignProcessToJobObject)(c, p1.hProcess);
    t_note("third empty job joins chain: ret %d err %lu", r, r ? 0 : GetLastError());
    CHECK(r);
    U(TerminateJobObject)(a, 1); jc_kill(&p1); CloseHandle(a); CloseHandle(b); CloseHandle(c);
}
TEST(jobnest_accounting_aggregates_to_ancestor) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE a = jc_job(), b = jc_job(); PROCESS_INFORMATION p1; CHECK(a && b); if (!a || !b) return;
    CHECK(jc_spawn_in_job(a, "jo_spin", "500", &p1));
    CHECK(U(AssignProcessToJobObject)(b, p1.hProcess));
    jc_wait_exit(p1.hProcess, 10000);
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION xa, xb; CHECK(jc_acct(a, &xa)); CHECK(jc_acct(b, &xb));
    CHECK(xa.TotalUserTime.QuadPart >= xb.TotalUserTime.QuadPart);
    CHECK(xb.TotalUserTime.QuadPart > 0);
    EXPECT_EQ(xa.TotalProcesses, 1); EXPECT_EQ(xb.TotalProcesses, 1);
    jc_kill(&p1); CloseHandle(a); CloseHandle(b);
}
TEST(jobnest_priority_limit_most_restrictive) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE a = jc_job(), b = jc_job(); PROCESS_INFORMATION p1; CHECK(a && b); if (!a || !b) return;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION e; memset(&e, 0, sizeof e);
    e.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PRIORITY_CLASS; e.BasicLimitInformation.PriorityClass = IDLE_PRIORITY_CLASS;
    CHECK(jc_set_ext(a, &e));
    e.BasicLimitInformation.PriorityClass = ABOVE_NORMAL_PRIORITY_CLASS; CHECK(jc_set_ext(b, &e));
    CHECK(jc_spawn_in_job(a, "jo_prio", "", &p1));
    if (p1.hProcess) { DWORD c = jc_wait_exit(p1.hProcess, 15000); t_note("initial priority under IDLE parent: 0x%lx", c >> 16); CHECK((c >> 16) == IDLE_PRIORITY_CLASS); jc_kill(&p1); }
    CloseHandle(a); CloseHandle(b);
}
TEST(jobnest_count_job_via_open_in_child) {
    REQUIRE(U(SetInformationJobObject));
    char nm[96]; jc_name(nm, sizeof nm, "ncnt"); WCHAR w[96]; jname_to_w(nm, w);
    HANDLE a = U(CreateJobObjectW)(NULL, w); PROCESS_INFORMATION pi; CHECK(a); if (!a) return;
    CHECK(jc_spawn_in_job(a, "jo_countjob", nm, &pi));
    if (pi.hProcess) { EXPECT_EQ(jc_wait_exit(pi.hProcess, 15000), 1); jc_kill(&pi); }
    CloseHandle(a);
}
