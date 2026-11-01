#include "jc.h"

/* Collect messages for up to ms; returns count and fills arrays. */
typedef struct { DWORD msg; ULONG_PTR key, pid; } JMSG;
static int collect(HANDLE port, JMSG *out, int cap, DWORD ms, DWORD idle_ms) {
    int n = 0; DWORD t0 = GetTickCount();
    while (n < cap) {
        DWORD el = GetTickCount() - t0; if (el >= ms) break;
        DWORD w = idle_ms < ms - el ? idle_ms : ms - el;
        if (!jc_getmsg(port, w, &out[n].msg, &out[n].key, &out[n].pid)) break;
        n++;
    }
    return n;
}
static int count_msg(JMSG *m, int n, DWORD id, ULONG_PTR pid) { int c = 0; for (int i = 0; i < n; i++) if (m[i].msg == id && (!pid || m[i].pid == pid)) c++; return c; }
static int index_msg(JMSG *m, int n, DWORD id, ULONG_PTR pid) { for (int i = 0; i < n; i++) if (m[i].msg == id && (!pid || m[i].pid == pid)) return i; return -1; }

TEST(jobnotify_new_process_on_assign) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(), port = jc_port(); PROCESS_INFORMATION pi; CHECK(j && port); if (!j || !port) return;
    CHECK(jc_assoc(j, port, 0xABCD));
    HANDLE p = jc_spawn("jo_sleep", "20000", 0, &pi); CHECK(p);
    if (p) {
        CHECK(U(AssignProcessToJobObject)(j, p));
        DWORD msg = 0; ULONG_PTR key = 0, pid = 0;
        int got = jc_getmsg(port, 5000, &msg, &key, &pid); CHECK(got);
        if (got) { EXPECT_EQ(msg, JOB_OBJECT_MSG_NEW_PROCESS); EXPECT_EQ(key, 0xABCD); EXPECT_EQ(pid, pi.dwProcessId); }
        U(TerminateJobObject)(j, 1); jc_kill(&pi);
    }
    CloseHandle(port); CloseHandle(j);
}
TEST(jobnotify_lifecycle_order) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(), port = jc_port(); PROCESS_INFORMATION pi; CHECK(j && port); if (!j || !port) return;
    CHECK(jc_assoc(j, port, 0x10));
    CHECK(jc_spawn_in_job(j, "jo_exit", "0", &pi));
    if (pi.hProcess) {
        jc_wait_exit(pi.hProcess, 10000);
        JMSG m[16]; int n = collect(port, m, 16, 6000, 1500);
        int inew = index_msg(m, n, JOB_OBJECT_MSG_NEW_PROCESS, pi.dwProcessId);
        int iex = index_msg(m, n, JOB_OBJECT_MSG_EXIT_PROCESS, pi.dwProcessId);
        int izero = index_msg(m, n, JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO, 0);
        CHECKF(inew >= 0 && iex >= 0 && izero >= 0, "messages: new %d exit %d zero %d (n=%d)", inew, iex, izero, n);
        CHECK(inew < iex); CHECK(iex < izero);
        for (int i = 0; i < n; i++) EXPECT_EQ(m[i].key, 0x10);
        if (izero >= 0) EXPECT_EQ(m[izero].pid, 0);
        jc_kill(&pi);
    }
    CloseHandle(port); CloseHandle(j);
}
TEST(jobnotify_exit_nonzero_code_is_normal_exit) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(), port = jc_port(); PROCESS_INFORMATION pi; CHECK(j && port); if (!j || !port) return;
    CHECK(jc_assoc(j, port, 1));
    CHECK(jc_spawn_in_job(j, "jo_exit", "3", &pi));
    if (pi.hProcess) {
        jc_wait_exit(pi.hProcess, 10000);
        JMSG m[16]; int n = collect(port, m, 16, 6000, 1500);
        CHECK(count_msg(m, n, JOB_OBJECT_MSG_EXIT_PROCESS, pi.dwProcessId) == 1);
        EXPECT_EQ(count_msg(m, n, JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS, pi.dwProcessId), 0);
        jc_kill(&pi);
    }
    CloseHandle(port); CloseHandle(j);
}
TEST(jobnotify_abnormal_exit_codes) {
    REQUIRE(U(SetInformationJobObject));
    static const DWORD codes[] = { 0xC0000005, 0xC000001D, 0xC0000094, 0xC00000FD, 0x80000003 };
    for (unsigned i = 0; i < sizeof codes / sizeof *codes; i++) {
        HANDLE j = jc_job(), port = jc_port(); PROCESS_INFORMATION pi; CHECK(j && port); if (!j || !port) return;
        CHECK(jc_assoc(j, port, 2));
        char a[32]; snprintf(a, sizeof a, "%lu", codes[i]);
        CHECK(jc_spawn_in_job(j, "jo_exit", a, &pi));
        if (pi.hProcess) {
            jc_wait_exit(pi.hProcess, 10000);
            JMSG m[16]; int n = collect(port, m, 16, 6000, 1200);
            int ab = count_msg(m, n, JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS, pi.dwProcessId), ex = count_msg(m, n, JOB_OBJECT_MSG_EXIT_PROCESS, pi.dwProcessId);
            CHECKF(ab == 1, "exit 0x%lx: abnormal msgs %d (exit msgs %d)", codes[i], ab, ex);
            t_note("exit 0x%lx: abnormal=%d exit=%d", codes[i], ab, ex);
            jc_kill(&pi);
        }
        CloseHandle(port); CloseHandle(j);
    }
}
TEST(jobnotify_multiple_children_counts) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(), port = jc_port(); CHECK(j && port); if (!j || !port) return;
    CHECK(jc_assoc(j, port, 3));
    enum { N = 3 }; PROCESS_INFORMATION pi[N];
    for (int i = 0; i < N; i++) CHECK(jc_spawn_in_job(j, "jo_sleep", "20000", &pi[i]));
    U(TerminateJobObject)(j, 8);
    for (int i = 0; i < N; i++) if (pi[i].hProcess) jc_wait_exit(pi[i].hProcess, 8000);
    JMSG m[32]; int n = collect(port, m, 32, 8000, 1500);
    EXPECT_EQ(count_msg(m, n, JOB_OBJECT_MSG_NEW_PROCESS, 0), N);
    /* processes removed by TerminateJobObject post no per-process exit message (Win10: the terminate path removes members without a notification); ACTIVE_PROCESS_ZERO is still posted */
    EXPECT_EQ(count_msg(m, n, JOB_OBJECT_MSG_EXIT_PROCESS, 0) + count_msg(m, n, JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS, 0), 0);
    EXPECT_EQ(count_msg(m, n, JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO, 0), 1);
    for (int i = 0; i < N; i++) if (pi[i].hProcess) { CHECK(count_msg(m, n, JOB_OBJECT_MSG_NEW_PROCESS, pi[i].dwProcessId) == 1); jc_kill(&pi[i]); }
    CloseHandle(port); CloseHandle(j);
}
TEST(jobnotify_active_zero_only_when_last_exits) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(), port = jc_port(); PROCESS_INFORMATION p1, p2; CHECK(j && port); if (!j || !port) return;
    CHECK(jc_assoc(j, port, 4));
    CHECK(jc_spawn_in_job(j, "jo_sleep", "20000", &p1)); CHECK(jc_spawn_in_job(j, "jo_sleep", "20000", &p2));
    TerminateProcess(p1.hProcess, 1); WaitForSingleObject(p1.hProcess, 5000);
    JMSG m[16]; int n = collect(port, m, 16, 2500, 800);
    EXPECT_EQ(count_msg(m, n, JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO, 0), 0);
    CHECK(count_msg(m, n, JOB_OBJECT_MSG_EXIT_PROCESS, p1.dwProcessId) + count_msg(m, n, JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS, p1.dwProcessId) >= 1);
    TerminateProcess(p2.hProcess, 1); WaitForSingleObject(p2.hProcess, 5000);
    n = collect(port, m, 16, 6000, 1500);
    EXPECT_EQ(count_msg(m, n, JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO, 0), 1);
    jc_kill(&p1); jc_kill(&p2); CloseHandle(port); CloseHandle(j);
}
TEST(jobnotify_grandchild_new_process) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(), port = jc_port(); PROCESS_INFORMATION pi; CHECK(j && port); if (!j || !port) return;
    CHECK(jc_assoc(j, port, 5));
    CHECK(jc_spawn_in_job(j, "jo_spawn", "0 1 1", &pi));
    if (pi.hProcess) {
        CHECK(jc_wait_active(j, 2, 15000));
        JMSG m[16]; int n = collect(port, m, 16, 3000, 800);
        EXPECT_EQ(count_msg(m, n, JOB_OBJECT_MSG_NEW_PROCESS, 0), 2);
        EXPECT_EQ(count_msg(m, n, JOB_OBJECT_MSG_NEW_PROCESS, pi.dwProcessId), 1);
        U(TerminateJobObject)(j, 1); jc_kill(&pi);
    }
    CloseHandle(port); CloseHandle(j);
}
TEST(jobnotify_no_messages_for_idle_job) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(), port = jc_port(); CHECK(j && port); if (!j || !port) return;
    CHECK(jc_assoc(j, port, 6));
    JMSG m[4]; EXPECT_EQ(collect(port, m, 4, 400, 400), 0);
    CloseHandle(port); CloseHandle(j);
}
TEST(jobnotify_associate_after_assign) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(), port = jc_port(); PROCESS_INFORMATION pi; CHECK(j && port); if (!j || !port) return;
    CHECK(jc_spawn_in_job(j, "jo_sleep", "20000", &pi));
    CHECK(jc_assoc(j, port, 7));
    JMSG m[8]; EXPECT_EQ(collect(port, m, 8, 300, 300), 0);     /* nothing replayed for the existing member */
    U(TerminateJobObject)(j, 2);
    jc_wait_exit(pi.hProcess, 8000);
    int n = collect(port, m, 8, 5000, 1200);
    /* member killed by TerminateJobObject: no per-process exit message on Win10 (see jobnotify_multiple_children_counts) */
    CHECK(count_msg(m, n, JOB_OBJECT_MSG_EXIT_PROCESS, pi.dwProcessId) + count_msg(m, n, JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS, pi.dwProcessId) == 0);
    t_note("ACTIVE_PROCESS_ZERO after late association: %d", count_msg(m, n, JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO, 0));
    jc_kill(&pi); CloseHandle(port); CloseHandle(j);
}
TEST(jobnotify_two_jobs_distinct_keys) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE a = jc_job(), b = jc_job(), port = jc_port(); PROCESS_INFORMATION pa, pb; CHECK(a && b && port); if (!a || !b || !port) return;
    CHECK(jc_assoc(a, port, 0xA0)); CHECK(jc_assoc(b, port, 0xB0));
    CHECK(jc_spawn_in_job(a, "jo_exit", "0", &pa)); CHECK(jc_spawn_in_job(b, "jo_exit", "0", &pb));
    jc_wait_exit(pa.hProcess, 10000); jc_wait_exit(pb.hProcess, 10000);
    JMSG m[32]; int n = collect(port, m, 32, 6000, 1500);
    for (int i = 0; i < n; i++) if (m[i].msg == JOB_OBJECT_MSG_NEW_PROCESS || m[i].msg == JOB_OBJECT_MSG_EXIT_PROCESS) {
        if (m[i].pid == pa.dwProcessId) EXPECT_EQ(m[i].key, 0xA0);
        if (m[i].pid == pb.dwProcessId) EXPECT_EQ(m[i].key, 0xB0);
    }
    EXPECT_EQ(count_msg(m, n, JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO, 0), 2);
    jc_kill(&pa); jc_kill(&pb); CloseHandle(port); CloseHandle(a); CloseHandle(b);
}
TEST(jobnotify_port_closed_before_events) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(), port = jc_port(); PROCESS_INFORMATION pi; CHECK(j && port); if (!j || !port) return;
    CHECK(jc_assoc(j, port, 8));
    CloseHandle(port);
    CHECK(jc_spawn_in_job(j, "jo_exit", "0", &pi));
    if (pi.hProcess) { EXPECT_EQ(jc_wait_exit(pi.hProcess, 10000), 0); jc_kill(&pi); }
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION a; CHECK(jc_acct(j, &a));
    CloseHandle(j);
}
TEST(jobnotify_active_process_limit_message) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(), port = jc_port(); PROCESS_INFORMATION p1, p2; CHECK(j && port); if (!j || !port) return;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION e; memset(&e, 0, sizeof e);
    e.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_ACTIVE_PROCESS; e.BasicLimitInformation.ActiveProcessLimit = 1;
    CHECK(jc_set_ext(j, &e)); CHECK(jc_assoc(j, port, 9));
    CHECK(jc_spawn_in_job(j, "jo_sleep", "20000", &p1));
    HANDLE h2 = jc_spawn("jo_sleep", "20000", 0, &p2);
    if (h2) {
        CHECK(!U(AssignProcessToJobObject)(j, h2));
        JMSG m[16]; int n = collect(port, m, 16, 3000, 800);
        t_note("ACTIVE_PROCESS_LIMIT messages on failed assign: %d", count_msg(m, n, JOB_OBJECT_MSG_ACTIVE_PROCESS_LIMIT, 0));
        jc_kill(&p2);
    }
    U(TerminateJobObject)(j, 1); jc_kill(&p1); CloseHandle(port); CloseHandle(j);
}
TEST(jobnotify_message_ids_constants) {
    EXPECT_EQ(JOB_OBJECT_MSG_END_OF_JOB_TIME, 1); EXPECT_EQ(JOB_OBJECT_MSG_END_OF_PROCESS_TIME, 2);
    EXPECT_EQ(JOB_OBJECT_MSG_ACTIVE_PROCESS_LIMIT, 3); EXPECT_EQ(JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO, 4);
    EXPECT_EQ(JOB_OBJECT_MSG_NEW_PROCESS, 6); EXPECT_EQ(JOB_OBJECT_MSG_EXIT_PROCESS, 7);
    EXPECT_EQ(JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS, 8); EXPECT_EQ(JOB_OBJECT_MSG_PROCESS_MEMORY_LIMIT, 9);
    EXPECT_EQ(JOB_OBJECT_MSG_JOB_MEMORY_LIMIT, 10);
}
TEST(jobnotify_kill_on_close_messages_not_required) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(), port = jc_port(); PROCESS_INFORMATION pi; CHECK(j && port); if (!j || !port) return;
    CHECK(jc_set_flags(j, JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE)); CHECK(jc_assoc(j, port, 11));
    CHECK(jc_spawn_in_job(j, "jo_sleep", "20000", &pi));
    JMSG m[4]; collect(port, m, 4, 500, 500);
    CloseHandle(j);
    CHECK(jc_wait_exit(pi.hProcess, 8000) != JC_TIMEOUT);
    int n = collect(port, m, 4, 1500, 700);
    t_note("messages after kill-on-close: %d", n);
    jc_kill(&pi); CloseHandle(port);
}
TEST(jobnotify_dequeued_values_via_overlapped_entries) {
    /* GetQueuedCompletionStatus returns TRUE with lpOverlapped holding the PID (not a pointer) */
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(), port = jc_port(); PROCESS_INFORMATION pi; CHECK(j && port); if (!j || !port) return;
    CHECK(jc_assoc(j, port, 0x1000));
    HANDLE p = jc_spawn("jo_sleep", "20000", 0, &pi);
    CHECK(U(AssignProcessToJobObject)(j, p));
    DWORD bytes = 0; ULONG_PTR key = 0; OVERLAPPED *ov = (OVERLAPPED *)(ULONG_PTR)0xDEAD;
    BOOL r = GetQueuedCompletionStatus(port, &bytes, &key, &ov, 5000);
    CHECK(r); EXPECT_EQ(bytes, JOB_OBJECT_MSG_NEW_PROCESS); EXPECT_EQ(key, 0x1000); EXPECT_EQ((ULONG_PTR)ov, pi.dwProcessId);
    U(TerminateJobObject)(j, 1); jc_kill(&pi); CloseHandle(port); CloseHandle(j);
}
