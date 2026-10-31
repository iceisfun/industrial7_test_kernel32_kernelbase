#include "jc.h"

static BOOL self_in_job(void) { BOOL in = FALSE; U(IsProcessInJob)(GetCurrentProcess(), NULL, &in); return in; }

/* ---------- AssignProcessToJobObject ---------- */
TEST(jobassign_suspended_child) {
    REQUIRE(U(AssignProcessToJobObject)); REQUIRE(U(CreateJobObjectW));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    PROCESS_INFORMATION pi; HANDLE p = jc_spawn("jo_sleep", "20000", 0, &pi); CHECK(p);
    if (p) {
        SetLastError(0);
        CHECK(U(AssignProcessToJobObject)(j, p));
        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION a; CHECK(jc_acct(j, &a));
        EXPECT_EQ(a.TotalProcesses, 1); EXPECT_EQ(a.ActiveProcesses, 1);
        { DWORD sc = ResumeThread(pi.hThread); EXPECT_EQ(sc, 1); }
        CHECK(U(TerminateJobObject)(j, 0x42));
        EXPECT_EQ(jc_wait_exit(p, 8000), 0x42);
        CHECK(jc_wait_active(j, 0, 3000));
        jc_kill(&pi);
    }
    CloseHandle(j);
}
TEST(jobassign_running_child) {
    REQUIRE(U(AssignProcessToJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    PROCESS_INFORMATION pi; HANDLE p = jc_spawn("jo_sleep", "20000", 0, &pi); CHECK(p);
    if (p) {
        ResumeThread(pi.hThread); Sleep(200);
        CHECK(U(AssignProcessToJobObject)(j, p));
        CHECK(jc_pid_in(j, pi.dwProcessId));
        U(TerminateJobObject)(j, 1);
        EXPECT_EQ(jc_wait_exit(p, 8000), 1);
        jc_kill(&pi);
    }
    CloseHandle(j);
}
TEST(jobassign_lasterror_untouched_on_success_irrelevant) {
    REQUIRE(U(AssignProcessToJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; HANDLE p = jc_spawn("jo_sleep", "5000", 0, &pi);
    CHECK(j && p);
    if (j && p) { BOOL r = U(AssignProcessToJobObject)(j, p); EXPECT_EQ(r, 1); }
    if (p) jc_kill(&pi); if (j) CloseHandle(j);
}
TEST(jobassign_null_job) {
    REQUIRE(U(AssignProcessToJobObject));
    PROCESS_INFORMATION pi; HANDLE p = jc_spawn("jo_sleep", "5000", 0, &pi); CHECK(p); if (!p) return;
    SetLastError(0);
    CHECK(!U(AssignProcessToJobObject)(NULL, p)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    jc_kill(&pi);
}
TEST(jobassign_null_process) {
    REQUIRE(U(AssignProcessToJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    SetLastError(0);
    CHECK(!U(AssignProcessToJobObject)(j, NULL)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(j);
}
TEST(jobassign_both_null) {
    REQUIRE(U(AssignProcessToJobObject));
    SetLastError(0);
    CHECK(!U(AssignProcessToJobObject)(NULL, NULL)); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(jobassign_job_is_invalid_handle_value) {
    REQUIRE(U(AssignProcessToJobObject));
    PROCESS_INFORMATION pi; HANDLE p = jc_spawn("jo_sleep", "5000", 0, &pi); CHECK(p); if (!p) return;
    SetLastError(0);
    CHECK(!U(AssignProcessToJobObject)(INVALID_HANDLE_VALUE, p)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    jc_kill(&pi);
}
TEST(jobassign_job_is_event) {
    REQUIRE(U(AssignProcessToJobObject));
    PROCESS_INFORMATION pi; HANDLE p = jc_spawn("jo_sleep", "5000", 0, &pi); CHECK(p); if (!p) return;
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SetLastError(0);
    CHECK(!U(AssignProcessToJobObject)(ev, p)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev); jc_kill(&pi);
}
TEST(jobassign_process_is_event) {
    REQUIRE(U(AssignProcessToJobObject));
    HANDLE j = jc_job(); HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SetLastError(0);
    CHECK(!U(AssignProcessToJobObject)(j, ev)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev); if (j) CloseHandle(j);
}
TEST(jobassign_process_is_thread_handle) {
    REQUIRE(U(AssignProcessToJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; HANDLE p = jc_spawn("jo_sleep", "5000", 0, &pi); CHECK(j && p);
    if (j && p) {
        SetLastError(0);
        CHECK(!U(AssignProcessToJobObject)(j, pi.hThread)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
    if (p) jc_kill(&pi); if (j) CloseHandle(j);
}
TEST(jobassign_job_handle_closed) {
    REQUIRE(U(AssignProcessToJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; HANDLE p = jc_spawn("jo_sleep", "5000", 0, &pi); CHECK(j && p);
    if (j && p) {
        CloseHandle(j);
        SetLastError(0);
        CHECK(!U(AssignProcessToJobObject)(j, p)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
    if (p) jc_kill(&pi);
}
TEST(jobassign_process_handle_closed) {
    REQUIRE(U(AssignProcessToJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; HANDLE p = jc_spawn("jo_sleep", "5000", 0, &pi); CHECK(j && p);
    if (j && p) {
        HANDLE keep = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pi.dwProcessId);
        jc_kill(&pi);
        SetLastError(0);
        CHECK(!U(AssignProcessToJobObject)(j, p)); EXPECT_ERR(ERROR_INVALID_HANDLE);
        if (keep) CloseHandle(keep);
    }
    if (j) CloseHandle(j);
}
TEST(jobassign_process_needs_set_quota_and_terminate) {
    REQUIRE(U(AssignProcessToJobObject));
    static const struct { DWORD acc; int ok; } t[] = {
        { PROCESS_QUERY_LIMITED_INFORMATION, 0 }, { PROCESS_SET_QUOTA, 0 }, { PROCESS_TERMINATE, 0 },
        { PROCESS_QUERY_INFORMATION | SYNCHRONIZE, 0 }, { PROCESS_SET_QUOTA | PROCESS_TERMINATE, 1 },
        { PROCESS_SET_QUOTA | PROCESS_TERMINATE | PROCESS_QUERY_INFORMATION, 1 } };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        HANDLE j = jc_job(); PROCESS_INFORMATION pi; HANDLE p = jc_spawn("jo_sleep", "5000", 0, &pi); CHECK(j && p);
        if (j && p) {
            HANDLE op = OpenProcess(t[i].acc, FALSE, pi.dwProcessId); CHECK(op);
            if (op) {
                SetLastError(0);
                BOOL r = U(AssignProcessToJobObject)(j, op);
                DWORD e = GetLastError();
                CHECKF(!!r == t[i].ok, "access 0x%lx result %d err %lu", t[i].acc, r, e);
                if (!t[i].ok && !r) CHECKF(e == ERROR_ACCESS_DENIED, "access 0x%lx err %lu", t[i].acc, e);
                CloseHandle(op);
            }
        }
        if (p) jc_kill(&pi); if (j) CloseHandle(j);
    }
}
TEST(jobassign_terminated_process) {
    REQUIRE(U(AssignProcessToJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; HANDLE p = t_spawn_child("jo_exit", "0", 0, NULL, NULL, NULL, FALSE, NULL, &pi); CHECK(j && p);
    if (j && p) {
        EXPECT_EQ(jc_wait_exit(p, 8000), 0);
        SetLastError(0);
        BOOL r = U(AssignProcessToJobObject)(j, p);
        DWORD e = GetLastError();
        CHECK(!r);
        if (!r) { t_note("assign of an already terminated process: error %lu (expected ACCESS_DENIED from STATUS_PROCESS_IS_TERMINATING)", e); CHECK(e != 0); }
    }
    if (p) { CloseHandle(pi.hThread); CloseHandle(p); } if (j) CloseHandle(j);
}
TEST(jobassign_same_job_twice) {
    REQUIRE(U(AssignProcessToJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; HANDLE p = jc_spawn("jo_sleep", "5000", 0, &pi); CHECK(j && p);
    if (j && p) {
        CHECK(U(AssignProcessToJobObject)(j, p));
        SetLastError(0);
        BOOL r = U(AssignProcessToJobObject)(j, p);
        t_note("assign to same job twice: %d err %lu", r, GetLastError());
        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION a; CHECK(jc_acct(j, &a));
        EXPECT_EQ(a.ActiveProcesses, 1);
    }
    if (p) jc_kill(&pi); if (j) CloseHandle(j);
}
TEST(jobassign_nested_second_job_ok) {
    REQUIRE(U(AssignProcessToJobObject));
    HANDLE a = jc_job(), b = jc_job(); PROCESS_INFORMATION pi; HANDLE p = jc_spawn("jo_sleep", "8000", 0, &pi); CHECK(a && b && p);
    if (a && b && p) {
        CHECK(U(AssignProcessToJobObject)(a, p));
        SetLastError(0);
        CHECK(U(AssignProcessToJobObject)(b, p));      /* Win8+: nested jobs */
        BOOL in = FALSE;
        CHECK(U(IsProcessInJob)(p, a, &in)); CHECK(in);
        in = FALSE; CHECK(U(IsProcessInJob)(p, b, &in)); CHECK(in);
        CHECK(jc_pid_in(a, pi.dwProcessId)); CHECK(jc_pid_in(b, pi.dwProcessId));
        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION x; CHECK(jc_acct(b, &x)); EXPECT_EQ(x.ActiveProcesses, 1);
    }
    if (p) jc_kill(&pi); if (a) CloseHandle(a); if (b) CloseHandle(b);
}
TEST(jobassign_nested_three_levels) {
    REQUIRE(U(AssignProcessToJobObject));
    HANDLE a = jc_job(), b = jc_job(), c = jc_job(); PROCESS_INFORMATION pi; HANDLE p = jc_spawn("jo_sleep", "8000", 0, &pi); CHECK(a && b && c && p);
    if (a && b && c && p) {
        CHECK(U(AssignProcessToJobObject)(a, p)); CHECK(U(AssignProcessToJobObject)(b, p)); CHECK(U(AssignProcessToJobObject)(c, p));
        BOOL in; HANDLE js[3] = { a, b, c };
        for (int i = 0; i < 3; i++) { in = FALSE; CHECK(U(IsProcessInJob)(p, js[i], &in)); CHECK(in); }
        CHECK(U(TerminateJobObject)(a, 77));
        EXPECT_EQ(jc_wait_exit(p, 8000), 77);
    }
    if (p) jc_kill(&pi); if (a) CloseHandle(a); if (b) CloseHandle(b); if (c) CloseHandle(c);
}
TEST(jobassign_unrelated_job_denied) {
    REQUIRE(U(AssignProcessToJobObject));
    HANDLE a = jc_job(), x = jc_job(); PROCESS_INFORMATION p1, p2;
    HANDLE h1 = jc_spawn("jo_sleep", "8000", 0, &p1), h2 = jc_spawn("jo_sleep", "8000", 0, &p2);
    CHECK(a && x && h1 && h2);
    if (a && x && h1 && h2) {
        CHECK(U(AssignProcessToJobObject)(a, h1));
        CHECK(U(AssignProcessToJobObject)(x, h2));
        SetLastError(0);
        CHECK(!U(AssignProcessToJobObject)(a, h2)); EXPECT_ERR(ERROR_ACCESS_DENIED);
        SetLastError(0);
        CHECK(!U(AssignProcessToJobObject)(x, h1)); EXPECT_ERR(ERROR_ACCESS_DENIED);
        BOOL in = TRUE; CHECK(U(IsProcessInJob)(h2, a, &in)); CHECK(!in);
    }
    if (h1) jc_kill(&p1); if (h2) jc_kill(&p2); if (a) CloseHandle(a); if (x) CloseHandle(x);
}
TEST(jobassign_active_process_limit_exceeded) {
    REQUIRE(U(AssignProcessToJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION e; memset(&e, 0, sizeof e);
    e.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_ACTIVE_PROCESS; e.BasicLimitInformation.ActiveProcessLimit = 1;
    CHECK(jc_set_ext(j, &e));
    PROCESS_INFORMATION p1, p2; HANDLE h1 = jc_spawn("jo_sleep", "8000", 0, &p1), h2 = jc_spawn("jo_sleep", "8000", 0, &p2);
    CHECK(h1 && h2);
    if (h1 && h2) {
        CHECK(U(AssignProcessToJobObject)(j, h1));
        SetLastError(0);
        CHECK(!U(AssignProcessToJobObject)(j, h2));
        { DWORD qe = GetLastError(); t_note("assign beyond active process limit: error %lu (1816 expected)", qe); CHECK(qe == ERROR_NOT_ENOUGH_QUOTA || qe == ERROR_ACCESS_DENIED); }
        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION a; CHECK(jc_acct(j, &a)); EXPECT_EQ(a.ActiveProcesses, 1);
    }
    if (h1) jc_kill(&p1); if (h2) jc_kill(&p2); CloseHandle(j);
}
TEST(jobassign_active_limit_frees_slot_on_exit) {
    REQUIRE(U(AssignProcessToJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION e; memset(&e, 0, sizeof e);
    e.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_ACTIVE_PROCESS; e.BasicLimitInformation.ActiveProcessLimit = 1;
    CHECK(jc_set_ext(j, &e));
    PROCESS_INFORMATION p1, p2; HANDLE h1 = jc_spawn("jo_sleep", "8000", 0, &p1), h2 = jc_spawn("jo_sleep", "8000", 0, &p2);
    CHECK(h1 && h2);
    if (h1 && h2) {
        CHECK(U(AssignProcessToJobObject)(j, h1));
        TerminateProcess(h1, 5); WaitForSingleObject(h1, 5000);
        CHECK(jc_wait_active(j, 0, 3000));
        CHECK(U(AssignProcessToJobObject)(j, h2));
    }
    if (h1) jc_kill(&p1); if (h2) jc_kill(&p2); CloseHandle(j);
}
TEST(jobassign_child_inherits_membership) {
    REQUIRE(U(AssignProcessToJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    PROCESS_INFORMATION pi;
    CHECK(jc_spawn_in_job(j, "jo_isjob", "", &pi));
    if (pi.hProcess) { EXPECT_EQ(jc_wait_exit(pi.hProcess, 10000), 1); jc_kill(&pi); }
    /* an unassigned sibling reports the test process's own state */
    HANDLE p = t_spawn_child("jo_isjob", "", 0, NULL, NULL, NULL, FALSE, NULL, &pi);
    if (p) { DWORD c = jc_wait_exit(p, 10000); if (self_in_job()) t_note("sibling in-job=%lu while test process is in a job", c); else EXPECT_EQ(c, 0); CloseHandle(pi.hThread); CloseHandle(p); }
    CloseHandle(j);
}
TEST(jobassign_grandchild_joins_job) {
    REQUIRE(U(AssignProcessToJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    PROCESS_INFORMATION pi;
    CHECK(jc_spawn_in_job(j, "jo_spawn", "0 1 0", &pi));
    if (pi.hProcess) { EXPECT_EQ(jc_wait_exit(pi.hProcess, 15000), 0x101); jc_kill(&pi); }
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION a; CHECK(jc_acct(j, &a));
    EXPECT_EQ(a.TotalProcesses, 2);
    CloseHandle(j);
}
TEST(jobassign_grandchild_counts_and_pidlist) {
    REQUIRE(U(AssignProcessToJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    PROCESS_INFORMATION pi;
    CHECK(jc_spawn_in_job(j, "jo_spawn", "0 3 1", &pi));
    if (pi.hProcess) {
        CHECK(jc_wait_active(j, 4, 15000));
        DWORD ids[16], asg = 0; int n = jc_pids(j, ids, 16, &asg);
        EXPECT_EQ(n, 4); EXPECT_EQ(asg, 4);
        CHECK(jc_pid_in(j, pi.dwProcessId));
        U(TerminateJobObject)(j, 9);
        EXPECT_EQ(jc_wait_exit(pi.hProcess, 8000), 9);
        CHECK(jc_wait_active(j, 0, 5000));
        jc_kill(&pi);
    }
    CloseHandle(j);
}
TEST(jobassign_many_children) {
    REQUIRE(U(AssignProcessToJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    enum { N = 12 }; PROCESS_INFORMATION pi[N]; int ok = 0;
    for (int i = 0; i < N; i++) { memset(&pi[i], 0, sizeof pi[i]); if (jc_spawn_in_job(j, "jo_sleep", "20000", &pi[i])) ok++; }
    EXPECT_EQ(ok, N);
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION a; CHECK(jc_acct(j, &a));
    EXPECT_EQ(a.ActiveProcesses, ok); EXPECT_EQ(a.TotalProcesses, ok);
    DWORD ids[32], asg; EXPECT_EQ(jc_pids(j, ids, 32, &asg), ok);
    CHECK(U(TerminateJobObject)(j, 31));
    for (int i = 0; i < N; i++) if (pi[i].hProcess) { EXPECT_EQ(jc_wait_exit(pi[i].hProcess, 8000), 31); jc_kill(&pi[i]); }
    CloseHandle(j);
}

/* ---------- IsProcessInJob ---------- */
TEST(jobisin_basic_true_false) {
    REQUIRE(U(IsProcessInJob));
    HANDLE j = jc_job(), k = jc_job(); CHECK(j && k); if (!j || !k) return;
    PROCESS_INFORMATION pi; CHECK(jc_spawn_in_job(j, "jo_sleep", "20000", &pi));
    if (pi.hProcess) {
        BOOL in = 0xAAAA; SetLastError(0);
        CHECK(U(IsProcessInJob)(pi.hProcess, j, &in)); EXPECT_EQ(in, 1);
        in = 0xAAAA; CHECK(U(IsProcessInJob)(pi.hProcess, NULL, &in)); EXPECT_EQ(in, 1);
        in = 0xAAAA; CHECK(U(IsProcessInJob)(pi.hProcess, k, &in)); EXPECT_EQ(in, 0);
        jc_kill(&pi);
    }
    CloseHandle(j); CloseHandle(k);
}
TEST(jobisin_unassigned_child_null_job) {
    REQUIRE(U(IsProcessInJob));
    PROCESS_INFORMATION pi; HANDLE p = jc_spawn("jo_sleep", "5000", 0, &pi); CHECK(p); if (!p) return;
    BOOL in = 0xAAAA, self = FALSE;
    CHECK(U(IsProcessInJob)(GetCurrentProcess(), NULL, &self));
    CHECK(U(IsProcessInJob)(p, NULL, &in));
    if (self) t_note("test process is itself in a job; inheritance depends on that job's breakaway flags (child in=%d)", in);
    else EXPECT_EQ(in, 0);
    HANDLE j = jc_job(); in = 0xAAAA;
    CHECK(U(IsProcessInJob)(p, j, &in)); EXPECT_EQ(in, 0);
    CloseHandle(j); jc_kill(&pi);
}
TEST(jobisin_pseudo_handle_specific_job) {
    REQUIRE(U(IsProcessInJob));
    HANDLE j = jc_job(); BOOL in = 0xAAAA; SetLastError(0);
    CHECK(U(IsProcessInJob)(GetCurrentProcess(), j, &in)); EXPECT_EQ(in, 0);
    in = 0xAAAA; CHECK(U(IsProcessInJob)((HANDLE)(LONG_PTR)-1, NULL, &in));
    BOOL self = FALSE; HANDLE me = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, GetCurrentProcessId());
    CHECK(me && U(IsProcessInJob)(me, NULL, &self));
    EXPECT_EQ(in, self); if (me) CloseHandle(me);
    CloseHandle(j);
}
TEST(jobisin_real_self_handle_matches_pseudo) {
    REQUIRE(U(IsProcessInJob));
    HANDLE me = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, GetCurrentProcessId()); CHECK(me); if (!me) return;
    BOOL a = 7, b = 7;
    CHECK(U(IsProcessInJob)(me, NULL, &a)); CHECK(U(IsProcessInJob)(GetCurrentProcess(), NULL, &b));
    EXPECT_EQ(a, b); CHECK(a == 0 || a == 1);
    CloseHandle(me);
}
TEST(jobisin_null_process) {
    REQUIRE(U(IsProcessInJob));
    HANDLE j = jc_job(); BOOL in = 0xAAAA; SetLastError(0);
    CHECK(!U(IsProcessInJob)(NULL, j, &in)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SetLastError(0); CHECK(!U(IsProcessInJob)(NULL, NULL, &in)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(j);
}
TEST(jobisin_bad_job_handle) {
    REQUIRE(U(IsProcessInJob));
    PROCESS_INFORMATION pi; HANDLE p = jc_spawn("jo_sleep", "5000", 0, &pi); CHECK(p); if (!p) return;
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL); BOOL in = 0xAAAA;
    SetLastError(0); CHECK(!U(IsProcessInJob)(p, ev, &in)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SetLastError(0); CHECK(!U(IsProcessInJob)(p, INVALID_HANDLE_VALUE, &in)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SetLastError(0); CHECK(!U(IsProcessInJob)(p, (HANDLE)(ULONG_PTR)0x7777FC, &in)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CHECK(in == 0xAAAA);
    CloseHandle(ev); jc_kill(&pi);
}
TEST(jobisin_bad_process_handle) {
    REQUIRE(U(IsProcessInJob));
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL); HANDLE j = jc_job(); BOOL in = 0xAAAA;
    SetLastError(0); CHECK(!U(IsProcessInJob)(ev, j, &in)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SetLastError(0); CHECK(!U(IsProcessInJob)(ev, NULL, &in)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev); CloseHandle(j);
}
TEST(jobisin_job_handle_needs_query) {
    REQUIRE(U(IsProcessInJob));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j && jc_spawn_in_job(j, "jo_sleep", "8000", &pi)); if (!j) return;
    BOOL in = 0xAAAA;
    HANDLE noq = jc_dup(j, JOB_OBJECT_ASSIGN_PROCESS | JOB_OBJECT_TERMINATE | SYNCHRONIZE);
    if (noq) { SetLastError(0); CHECK(!U(IsProcessInJob)(pi.hProcess, noq, &in)); EXPECT_ERR(ERROR_ACCESS_DENIED); CloseHandle(noq); }
    HANDLE q = jc_dup(j, JOB_OBJECT_QUERY);
    if (q) { in = 0; CHECK(U(IsProcessInJob)(pi.hProcess, q, &in)); EXPECT_EQ(in, 1); CloseHandle(q); }
    jc_kill(&pi); CloseHandle(j);
}
TEST(jobisin_process_handle_rights) {
    REQUIRE(U(IsProcessInJob));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j && jc_spawn_in_job(j, "jo_sleep", "8000", &pi)); if (!j) return;
    BOOL in = 0xAAAA;
    HANDLE none = OpenProcess(SYNCHRONIZE, FALSE, pi.dwProcessId);
    if (none) { SetLastError(0); CHECK(!U(IsProcessInJob)(none, NULL, &in)); EXPECT_ERR(ERROR_ACCESS_DENIED); CloseHandle(none); }
    HANDLE qi = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pi.dwProcessId);
    if (qi) { in = 0; CHECK(U(IsProcessInJob)(qi, j, &in)); EXPECT_EQ(in, 1); CloseHandle(qi); }
    HANDLE ql = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pi.dwProcessId);
    if (ql) { in = 7; BOOL r = U(IsProcessInJob)(ql, j, &in); t_note("limited-query handle: ret %d err %lu in=%d", r, r ? 0 : GetLastError(), in); CloseHandle(ql); }
    jc_kill(&pi); CloseHandle(j);
}
TEST(jobisin_after_process_exit) {
    REQUIRE(U(IsProcessInJob));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j && jc_spawn_in_job(j, "jo_exit", "0", &pi)); if (!j) return;
    EXPECT_EQ(jc_wait_exit(pi.hProcess, 8000), 0);
    BOOL in = 0xAAAA; SetLastError(0);
    BOOL r = U(IsProcessInJob)(pi.hProcess, j, &in);
    t_note("exited process in job: ret %d in %d err %lu", r, in, r ? 0 : GetLastError());
    jc_kill(&pi); CloseHandle(j);
}
TEST(jobisin_nested_ancestor_true) {
    REQUIRE(U(IsProcessInJob));
    HANDLE a = jc_job(), b = jc_job(); PROCESS_INFORMATION pi; HANDLE p = jc_spawn("jo_sleep", "8000", 0, &pi); CHECK(a && b && p);
    if (a && b && p) {
        U(AssignProcessToJobObject)(a, p); U(AssignProcessToJobObject)(b, p);
        BOOL in = 0; CHECK(U(IsProcessInJob)(p, a, &in)); CHECK(in);
    }
    if (p) jc_kill(&pi); if (a) CloseHandle(a); if (b) CloseHandle(b);
}
TEST(jobisin_result_normalized_bool) {
    REQUIRE(U(IsProcessInJob));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j && jc_spawn_in_job(j, "jo_sleep", "8000", &pi)); if (!j) return;
    BOOL in = 0x55555555; CHECK(U(IsProcessInJob)(pi.hProcess, j, &in)); CHECK(in == 1);
    BOOL out = 0x55555555; HANDLE k = jc_job(); CHECK(U(IsProcessInJob)(pi.hProcess, k, &out)); CHECK(out == 0);
    CloseHandle(k); jc_kill(&pi); CloseHandle(j);
}
TEST(jobisin_kernelbase_export_matches) {
    REQUIRE(UB(IsProcessInJob)); REQUIRE(UK(IsProcessInJob));
    BOOL a = 5, b = 6;
    CHECK(UB(IsProcessInJob)(GetCurrentProcess(), NULL, &a)); CHECK(UK(IsProcessInJob)(GetCurrentProcess(), NULL, &b));
    EXPECT_EQ(a, b);
    CHECK(UK(IsProcessInJob) == UB(IsProcessInJob) || UK(IsProcessInJob) != NULL);
}

/* ---------- TerminateJobObject ---------- */
TEST(jobterm_exit_code_values) {
    REQUIRE(U(TerminateJobObject));
    static const UINT codes[] = { 0, 1, 0x7F, 0xFF, 0x1234, 0xC0000005, 0xC000013A, 0x80000000, 0xFFFFFFFF, 259 };
    for (unsigned i = 0; i < sizeof codes / sizeof *codes; i++) {
        HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j && jc_spawn_in_job(j, "jo_sleep", "20000", &pi)); if (!j) continue;
        SetLastError(0);
        CHECK(U(TerminateJobObject)(j, codes[i]));
        DWORD c = jc_wait_exit(pi.hProcess, 8000);
        CHECKF(c == codes[i], "code 0x%x got 0x%lx", codes[i], c);
        jc_kill(&pi); CloseHandle(j);
    }
}
TEST(jobterm_empty_job) {
    REQUIRE(U(TerminateJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    SetLastError(0);
    BOOL r = U(TerminateJobObject)(j, 5);
    t_note("terminate empty job: ret %d err %lu", r, r ? 0 : GetLastError());
    CloseHandle(j);
}
TEST(jobterm_twice) {
    REQUIRE(U(TerminateJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j && jc_spawn_in_job(j, "jo_sleep", "20000", &pi)); if (!j) return;
    CHECK(U(TerminateJobObject)(j, 11));
    EXPECT_EQ(jc_wait_exit(pi.hProcess, 8000), 11);
    SetLastError(0);
    BOOL r = U(TerminateJobObject)(j, 12);
    t_note("terminate again: ret %d err %lu", r, r ? 0 : GetLastError());
    EXPECT_EQ(jc_wait_exit(pi.hProcess, 100), 11);   /* exit code unchanged */
    jc_kill(&pi); CloseHandle(j);
}
TEST(jobterm_invalid_handles) {
    REQUIRE(U(TerminateJobObject));
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL), j = jc_job();
    SetLastError(0); CHECK(!U(TerminateJobObject)(NULL, 1)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SetLastError(0); CHECK(!U(TerminateJobObject)(INVALID_HANDLE_VALUE, 1)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SetLastError(0); CHECK(!U(TerminateJobObject)(ev, 1)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(j); SetLastError(0); CHECK(!U(TerminateJobObject)(j, 1)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
}
TEST(jobterm_does_not_kill_caller_or_unrelated) {
    REQUIRE(U(TerminateJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi, other; CHECK(j && jc_spawn_in_job(j, "jo_sleep", "20000", &pi)); if (!j) return;
    HANDLE o = jc_spawn("jo_sleep", "20000", 0, &other); ResumeThread(other.hThread);
    CHECK(U(TerminateJobObject)(j, 2));
    EXPECT_EQ(jc_wait_exit(pi.hProcess, 8000), 2);
    EXPECT_EQ(WaitForSingleObject(o, 300), WAIT_TIMEOUT);
    jc_kill(&other); jc_kill(&pi); CloseHandle(j);
}
TEST(jobterm_kills_grandchildren) {
    REQUIRE(U(TerminateJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j && jc_spawn_in_job(j, "jo_spawn", "0 2 1", &pi)); if (!j) return;
    CHECK(jc_wait_active(j, 3, 15000));
    DWORD ids[8]; int n = jc_pids(j, ids, 8, NULL);
    HANDLE gh[8] = {0};
    for (int i = 0; i < n; i++) gh[i] = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, ids[i]);
    CHECK(U(TerminateJobObject)(j, 0x55));
    for (int i = 0; i < n; i++) if (gh[i]) {
        EXPECT_EQ(WaitForSingleObject(gh[i], 8000), WAIT_OBJECT_0);
        DWORD c = 0; GetExitCodeProcess(gh[i], &c); EXPECT_EQ(c, 0x55);
        CloseHandle(gh[i]);
    }
    CHECK(jc_wait_active(j, 0, 5000));
    jc_kill(&pi); CloseHandle(j);
}
TEST(jobterm_accounting_after) {
    REQUIRE(U(TerminateJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j && jc_spawn_in_job(j, "jo_sleep", "20000", &pi)); if (!j) return;
    U(TerminateJobObject)(j, 3);
    jc_wait_exit(pi.hProcess, 8000);
    CHECK(jc_wait_active(j, 0, 5000));
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION a; CHECK(jc_acct(j, &a));
    EXPECT_EQ(a.TotalProcesses, 1); EXPECT_EQ(a.ActiveProcesses, 0);
    t_note("TotalTerminatedProcesses after TerminateJobObject = %lu", a.TotalTerminatedProcesses);
    DWORD ids[4], asg = 9; EXPECT_EQ(jc_pids(j, ids, 4, &asg), 0); EXPECT_EQ(asg, 0);
    jc_kill(&pi); CloseHandle(j);
}
TEST(jobterm_job_signaled_state) {
    REQUIRE(U(TerminateJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j && jc_spawn_in_job(j, "jo_sleep", "20000", &pi)); if (!j) return;
    EXPECT_EQ(WaitForSingleObject(j, 100), WAIT_TIMEOUT);
    U(TerminateJobObject)(j, 3);
    jc_wait_exit(pi.hProcess, 8000);
    DWORD w = WaitForSingleObject(j, 500);
    t_note("job wait after TerminateJobObject: %lu (0=signaled)", w);
    jc_kill(&pi); CloseHandle(j);
}
TEST(jobterm_kills_nested_job_members) {
    REQUIRE(U(TerminateJobObject));
    HANDLE a = jc_job(); PROCESS_INFORMATION pi; CHECK(a); if (!a) return;
    char nm[96]; jc_name(nm, sizeof nm, "nestterm"); char args[200]; snprintf(args, sizeof args, "%s 0 0 30000", nm);
    CHECK(jc_spawn_in_job(a, "jo_nest", args, &pi));
    if (pi.hProcess) {
        WCHAR w[96]; for (int i = 0;; i++) { w[i] = nm[i]; if (!nm[i]) break; }
        HANDLE b = NULL; DWORD t0 = GetTickCount();
        while (!(b = U(OpenJobObjectW)(JOB_OBJECT_ALL_ACCESS, FALSE, w)) && GetTickCount() - t0 < 8000) Sleep(30);
        CHECK(b);
        if (b) {
            CHECK(jc_wait_active(b, 1, 5000));
            CHECK(U(TerminateJobObject)(a, 0x66));
            EXPECT_EQ(jc_wait_exit(pi.hProcess, 8000), 0x66);
            CloseHandle(b);
        }
        jc_kill(&pi);
    }
    CloseHandle(a);
}
