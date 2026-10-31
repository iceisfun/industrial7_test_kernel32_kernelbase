#include "jc.h"

static BOOL Q(HANDLE j, int cls, void *b, DWORD n, DWORD *rl) { return U(QueryInformationJobObject)(j, (JOBOBJECTINFOCLASS)cls, b, n, rl); }
static BOOL S(HANDLE j, int cls, void *b, DWORD n) { return U(SetInformationJobObject)(j, (JOBOBJECTINFOCLASS)cls, b, n); }
static BOOL badlen_err(DWORD e) { return e == ERROR_BAD_LENGTH || e == ERROR_INVALID_PARAMETER; }

/* ---------- struct layout sanity (x64) ---------- */
TEST(jobinfo_struct_sizes) {
    EXPECT_EQ(sizeof(JOBOBJECT_BASIC_ACCOUNTING_INFORMATION), 48);
    EXPECT_EQ(sizeof(JOBOBJECT_BASIC_LIMIT_INFORMATION), 64);
    EXPECT_EQ(sizeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION), 144);
    EXPECT_EQ(sizeof(JOBOBJECT_BASIC_AND_IO_ACCOUNTING_INFORMATION), 96);
    EXPECT_EQ(sizeof(JOBOBJECT_BASIC_PROCESS_ID_LIST), 16);
    EXPECT_EQ(sizeof(JOBOBJECT_BASIC_UI_RESTRICTIONS), 4);
    EXPECT_EQ(sizeof(JOBOBJECT_ASSOCIATE_COMPLETION_PORT), 16);
    EXPECT_EQ(sizeof(JOBOBJECT_END_OF_JOB_TIME_INFORMATION), 4);
}

/* ---------- invalid handle / class ---------- */
TEST(jobquery_invalid_handles) {
    REQUIRE(U(QueryInformationJobObject));
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION a; HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL); HANDLE j = jc_job(); CloseHandle(j);
    SetLastError(0); CHECK(!Q(NULL, 1, &a, sizeof a, NULL)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SetLastError(0); CHECK(!Q(INVALID_HANDLE_VALUE, 1, &a, sizeof a, NULL)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SetLastError(0); CHECK(!Q(ev, 1, &a, sizeof a, NULL)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SetLastError(0); CHECK(!Q(j, 1, &a, sizeof a, NULL)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
}
TEST(jobquery_invalid_handles_basic_ext) {
    REQUIRE(U(QueryInformationJobObject));
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION e; JOBOBJECT_BASIC_LIMIT_INFORMATION b;
    SetLastError(0); CHECK(!Q(NULL, JobObjectExtendedLimitInformation, &e, sizeof e, NULL)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SetLastError(0); CHECK(!Q(NULL, JobObjectBasicLimitInformation, &b, sizeof b, NULL)); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(jobset_invalid_handles) {
    REQUIRE(U(SetInformationJobObject));
    JOBOBJECT_BASIC_UI_RESTRICTIONS u = {0}; HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SetLastError(0); CHECK(!S(NULL, JobObjectBasicUIRestrictions, &u, sizeof u)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SetLastError(0); CHECK(!S(INVALID_HANDLE_VALUE, JobObjectBasicUIRestrictions, &u, sizeof u)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SetLastError(0); CHECK(!S(ev, JobObjectBasicUIRestrictions, &u, sizeof u)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION e; memset(&e, 0, sizeof e);
    SetLastError(0); CHECK(!S(NULL, JobObjectExtendedLimitInformation, &e, sizeof e)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SetLastError(0); CHECK(!S(ev, JobObjectExtendedLimitInformation, &e, sizeof e)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
}
TEST(jobquery_invalid_class_values) {
    REQUIRE(U(QueryInformationJobObject));
    HANDLE j = jc_job(); BYTE buf[512]; CHECK(j); if (!j) return;
    static const int cls[] = { 0, 100, 255, 0x1000, 0x7FFFFFFF, -1 };
    for (unsigned i = 0; i < sizeof cls / sizeof *cls; i++) {
        SetLastError(0);
        BOOL r = Q(j, cls[i], buf, sizeof buf, NULL);
        DWORD e = GetLastError();
        CHECKF(!r, "class %d unexpectedly succeeded", cls[i]);
        CHECKF(e == ERROR_INVALID_PARAMETER, "class %d err %lu", cls[i], e);
    }
    CloseHandle(j);
}
TEST(jobset_invalid_class_values) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); BYTE buf[512] = {0}; CHECK(j); if (!j) return;
    static const int cls[] = { 0, 100, 255, 0x7FFFFFFF, -1 };
    for (unsigned i = 0; i < sizeof cls / sizeof *cls; i++) {
        SetLastError(0);
        BOOL r = S(j, cls[i], buf, 64);
        DWORD e = GetLastError();
        CHECKF(!r, "class %d unexpectedly succeeded", cls[i]);
        CHECKF(e == ERROR_INVALID_PARAMETER, "class %d err %lu", cls[i], e);
    }
    CloseHandle(j);
}
TEST(jobquery_class_sweep_notes) {
    REQUIRE(U(QueryInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    char line[600]; int n = 0;
    for (int c = 1; c < 48; c++) {
        BYTE buf[512]; DWORD rl = 0xFFFFFFFF; memset(buf, 0, sizeof buf);
        SetLastError(0);
        BOOL r = Q(j, c, buf, sizeof buf, &rl);
        n += snprintf(line + n, sizeof line - n, "%d:%s%lu ", c, r ? "ok/" : "e", r ? (unsigned long)rl : GetLastError());
        if (n > 500) { t_note("%s", line); n = 0; }
    }
    if (n) t_note("%s", line);
    CloseHandle(j);
}

/* ---------- query: fresh job contents & length rules ---------- */
TEST(jobquery_fresh_accounting_zero) {
    REQUIRE(U(QueryInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION a; memset(&a, 0xCC, sizeof a); DWORD rl = 0xFFFF;
    CHECK(Q(j, JobObjectBasicAccountingInformation, &a, sizeof a, &rl));
    EXPECT_EQ(rl, sizeof a);
    EXPECT_EQ(a.TotalUserTime.QuadPart, 0); EXPECT_EQ(a.TotalKernelTime.QuadPart, 0);
    EXPECT_EQ(a.ThisPeriodTotalUserTime.QuadPart, 0); EXPECT_EQ(a.ThisPeriodTotalKernelTime.QuadPart, 0);
    EXPECT_EQ(a.TotalPageFaultCount, 0); EXPECT_EQ(a.TotalProcesses, 0);
    EXPECT_EQ(a.ActiveProcesses, 0); EXPECT_EQ(a.TotalTerminatedProcesses, 0);
    CloseHandle(j);
}
TEST(jobquery_fresh_basic_and_io_zero) {
    REQUIRE(U(QueryInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_BASIC_AND_IO_ACCOUNTING_INFORMATION a; memset(&a, 0xCC, sizeof a); DWORD rl = 0xFFFF;
    CHECK(Q(j, JobObjectBasicAndIoAccountingInformation, &a, sizeof a, &rl));
    EXPECT_EQ(rl, sizeof a);
    EXPECT_EQ(a.BasicInfo.TotalProcesses, 0); EXPECT_EQ(a.BasicInfo.ActiveProcesses, 0);
    EXPECT_EQ(a.IoInfo.ReadOperationCount, 0); EXPECT_EQ(a.IoInfo.WriteOperationCount, 0); EXPECT_EQ(a.IoInfo.OtherOperationCount, 0);
    EXPECT_EQ(a.IoInfo.ReadTransferCount, 0); EXPECT_EQ(a.IoInfo.WriteTransferCount, 0); EXPECT_EQ(a.IoInfo.OtherTransferCount, 0);
    CloseHandle(j);
}
TEST(jobquery_fresh_basic_limit) {
    REQUIRE(U(QueryInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_BASIC_LIMIT_INFORMATION b; memset(&b, 0xCC, sizeof b); DWORD rl = 0xFFFF;
    CHECK(Q(j, JobObjectBasicLimitInformation, &b, sizeof b, &rl));
    EXPECT_EQ(rl, sizeof b);
    EXPECT_EQ(b.LimitFlags, 0); EXPECT_EQ(b.PerProcessUserTimeLimit.QuadPart, 0); EXPECT_EQ(b.PerJobUserTimeLimit.QuadPart, 0);
    EXPECT_EQ(b.MinimumWorkingSetSize, 0); EXPECT_EQ(b.MaximumWorkingSetSize, 0);
    EXPECT_EQ(b.ActiveProcessLimit, 0); EXPECT_EQ(b.Affinity, 0); EXPECT_EQ(b.SchedulingClass, 0);
    /* user-mode wrapper translates the native priority index; an unset limit reads back as NORMAL_PRIORITY_CLASS */
    EXPECT_EQ(b.PriorityClass, NORMAL_PRIORITY_CLASS);
    CloseHandle(j);
}
TEST(jobquery_fresh_extended_limit) {
    REQUIRE(U(QueryInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION e; memset(&e, 0xCC, sizeof e); DWORD rl = 0xFFFF;
    CHECK(Q(j, JobObjectExtendedLimitInformation, &e, sizeof e, &rl));
    EXPECT_EQ(rl, sizeof e);
    EXPECT_EQ(e.BasicLimitInformation.LimitFlags, 0);
    EXPECT_EQ(e.BasicLimitInformation.PriorityClass, NORMAL_PRIORITY_CLASS);
    EXPECT_EQ(e.ProcessMemoryLimit, 0); EXPECT_EQ(e.JobMemoryLimit, 0);
    EXPECT_EQ(e.PeakProcessMemoryUsed, 0); EXPECT_EQ(e.PeakJobMemoryUsed, 0);
    EXPECT_EQ(e.IoInfo.ReadOperationCount, 0); EXPECT_EQ(e.IoInfo.WriteTransferCount, 0);
    CloseHandle(j);
}
TEST(jobquery_fresh_ui_restrictions) {
    REQUIRE(U(QueryInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_BASIC_UI_RESTRICTIONS u; memset(&u, 0xCC, sizeof u); DWORD rl = 0xFFFF;
    CHECK(Q(j, JobObjectBasicUIRestrictions, &u, sizeof u, &rl));
    EXPECT_EQ(rl, sizeof u); EXPECT_EQ(u.UIRestrictionsClass, 0);
    CloseHandle(j);
}
TEST(jobquery_fresh_pid_list) {
    REQUIRE(U(QueryInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_BASIC_PROCESS_ID_LIST l; memset(&l, 0xCC, sizeof l); DWORD rl = 0xFFFF;
    CHECK(Q(j, JobObjectBasicProcessIdList, &l, sizeof l, &rl));
    EXPECT_EQ(l.NumberOfAssignedProcesses, 0); EXPECT_EQ(l.NumberOfProcessIdsInList, 0);
    t_note("pid list fresh return length %lu", rl);
    CloseHandle(j);
}
TEST(jobquery_fresh_end_of_job_time) {
    REQUIRE(U(QueryInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_END_OF_JOB_TIME_INFORMATION t; t.EndOfJobTimeAction = 0xCC; DWORD rl = 0xFFFF;
    BOOL r = Q(j, JobObjectEndOfJobTimeInformation, &t, sizeof t, &rl);
    if (r) { EXPECT_EQ(t.EndOfJobTimeAction, JOB_OBJECT_TERMINATE_AT_END_OF_JOB); EXPECT_EQ(rl, sizeof t); }
    else t_note("query EndOfJobTime on fresh job failed %lu", GetLastError());
    CloseHandle(j);
}
TEST(jobquery_length_too_small_fixed_classes) {
    REQUIRE(U(QueryInformationJobObject));
    static const struct { int cls; DWORD size; } t[] = { {1, 48}, {4, 4}, {6, 4}, {8, 96} };
    HANDLE j = jc_job(); BYTE buf[256]; CHECK(j); if (!j) return;
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        for (DWORD n = 0; n < 4 && n < t[i].size; n++) {
            SetLastError(0); memset(buf, 0, sizeof buf);
            BOOL r = Q(j, t[i].cls, buf, n, NULL);
            DWORD e = GetLastError();
            CHECKF(!r, "class %d len %lu succeeded", t[i].cls, n);
            CHECKF(e == ERROR_BAD_LENGTH, "class %d len %lu err %lu", t[i].cls, n, e);
        }
        SetLastError(0);
        BOOL r = Q(j, t[i].cls, buf, t[i].size - 1, NULL);
        CHECKF(!r && GetLastError() == ERROR_BAD_LENGTH, "class %d size-1", t[i].cls);
    }
    CloseHandle(j);
}
TEST(jobquery_length_wrong_basic_extended) {
    REQUIRE(U(QueryInformationJobObject));
    HANDLE j = jc_job(); BYTE buf[512]; CHECK(j); if (!j) return;
    static const DWORD bl[] = { 0, 1, 8, 63, 65, 72, 128, 144 };
    for (unsigned i = 0; i < sizeof bl / sizeof *bl; i++) {
        SetLastError(0); BOOL r = Q(j, JobObjectBasicLimitInformation, buf, bl[i], NULL); DWORD e = GetLastError();
        CHECKF(!r, "basic len %lu succeeded", bl[i]); CHECKF(badlen_err(e), "basic len %lu err %lu", bl[i], e);
    }
    static const DWORD el[] = { 0, 1, 64, 143, 145, 152, 200, 256 };
    for (unsigned i = 0; i < sizeof el / sizeof *el; i++) {
        SetLastError(0); BOOL r = Q(j, JobObjectExtendedLimitInformation, buf, el[i], NULL); DWORD e = GetLastError();
        CHECKF(!r, "ext len %lu succeeded", el[i]); CHECKF(badlen_err(e), "ext len %lu err %lu", el[i], e);
    }
    CloseHandle(j);
}
TEST(jobset_length_wrong_basic_extended) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); BYTE buf[512] = {0}; CHECK(j); if (!j) return;
    static const DWORD bl[] = { 0, 1, 8, 63, 65, 72, 128, 144 };
    for (unsigned i = 0; i < sizeof bl / sizeof *bl; i++) {
        SetLastError(0); BOOL r = S(j, JobObjectBasicLimitInformation, buf, bl[i]); DWORD e = GetLastError();
        CHECKF(!r, "basic len %lu succeeded", bl[i]); CHECKF(badlen_err(e), "basic len %lu err %lu", bl[i], e);
    }
    static const DWORD el[] = { 0, 1, 64, 143, 145, 152, 200, 256 };
    for (unsigned i = 0; i < sizeof el / sizeof *el; i++) {
        SetLastError(0); BOOL r = S(j, JobObjectExtendedLimitInformation, buf, el[i]); DWORD e = GetLastError();
        CHECKF(!r, "ext len %lu succeeded", el[i]); CHECKF(badlen_err(e), "ext len %lu err %lu", el[i], e);
    }
    CloseHandle(j);
}
TEST(jobset_length_wrong_fixed_classes) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); BYTE buf[256] = {0}; CHECK(j); if (!j) return;
    static const struct { int cls; DWORD size; } t[] = { {4, 4}, {6, 4}, {7, 16} };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        SetLastError(0); BOOL r = S(j, t[i].cls, buf, t[i].size - 1); DWORD e = GetLastError();
        CHECKF(!r && e == ERROR_BAD_LENGTH, "class %d size-1 -> %d err %lu", t[i].cls, r, e);
        SetLastError(0); r = S(j, t[i].cls, buf, 0); e = GetLastError();
        CHECKF(!r && e == ERROR_BAD_LENGTH, "class %d len0 -> %d err %lu", t[i].cls, r, e);
    }
    CloseHandle(j);
}
TEST(jobquery_larger_length_fixed_classes_notes) {
    REQUIRE(U(QueryInformationJobObject));
    HANDLE j = jc_job(); BYTE buf[512]; CHECK(j); if (!j) return;
    static const struct { int cls; DWORD size; } t[] = { {1, 48}, {4, 4}, {6, 4}, {8, 96} };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        DWORD rl = 0xFFFF; SetLastError(0);
        BOOL r = Q(j, t[i].cls, buf, t[i].size + 8, &rl);
        t_note("class %d len+8: ret %d err %lu rl %lu", t[i].cls, r, r ? 0 : GetLastError(), rl);
    }
    CloseHandle(j);
}
TEST(jobquery_null_buffer_noaccess) {
    REQUIRE(U(QueryInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    SetLastError(0);
    BOOL r = Q(j, JobObjectBasicAccountingInformation, NULL, sizeof(JOBOBJECT_BASIC_ACCOUNTING_INFORMATION), NULL);
    t_note("query accounting NULL buffer: ret %d err %lu", r, GetLastError());
    CHECK(!r);
    SetLastError(0);
    r = Q(j, JobObjectBasicUIRestrictions, NULL, 4, NULL);
    CHECK(!r);
    CloseHandle(j);
}
TEST(jobquery_return_length_optional) {
    REQUIRE(U(QueryInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION a;
    CHECK(Q(j, JobObjectBasicAccountingInformation, &a, sizeof a, NULL));
    DWORD rl = 0xDEAD; CHECK(Q(j, JobObjectBasicAccountingInformation, &a, sizeof a, &rl)); EXPECT_EQ(rl, 48);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION e;
    CHECK(Q(j, JobObjectExtendedLimitInformation, &e, sizeof e, NULL));
    rl = 0xDEAD; CHECK(Q(j, JobObjectExtendedLimitInformation, &e, sizeof e, &rl)); EXPECT_EQ(rl, 144);
    JOBOBJECT_BASIC_LIMIT_INFORMATION b;
    rl = 0xDEAD; CHECK(Q(j, JobObjectBasicLimitInformation, &b, sizeof b, &rl)); EXPECT_EQ(rl, 64);
    CloseHandle(j);
}
TEST(jobquery_repeated_stable) {
    REQUIRE(U(QueryInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION e0, e1;
    CHECK(Q(j, JobObjectExtendedLimitInformation, &e0, sizeof e0, NULL));
    for (int i = 0; i < 200; i++) {
        memset(&e1, 0x77, sizeof e1);
        CHECK(Q(j, JobObjectExtendedLimitInformation, &e1, sizeof e1, NULL));
        if (memcmp(&e0, &e1, sizeof e0)) { CHECKF(0, "iteration %d differs", i); break; }
    }
    CloseHandle(j);
}
TEST(jobquery_buffer_not_written_on_error) {
    REQUIRE(U(QueryInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    BYTE buf[200]; memset(buf, 0x5A, sizeof buf);
    CHECK(!Q(j, JobObjectBasicAccountingInformation, buf, 47, NULL));
    int dirty = 0; for (int i = 0; i < 200; i++) if (buf[i] != 0x5A) dirty++;
    EXPECT_EQ(dirty, 0);
    CloseHandle(j);
}

/* ---------- ProcessIdList ---------- */
TEST(jobpids_overflow_more_data) {
    REQUIRE(U(QueryInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION p1, p2; CHECK(j); if (!j) return;
    CHECK(jc_spawn_in_job(j, "jo_sleep", "20000", &p1)); CHECK(jc_spawn_in_job(j, "jo_sleep", "20000", &p2));
    JOBOBJECT_BASIC_PROCESS_ID_LIST l; memset(&l, 0xCC, sizeof l); DWORD rl = 0;
    SetLastError(0);
    BOOL r = Q(j, JobObjectBasicProcessIdList, &l, sizeof l, &rl);
    CHECK(!r); EXPECT_ERR(ERROR_MORE_DATA);
    EXPECT_EQ(l.NumberOfAssignedProcesses, 2); EXPECT_EQ(l.NumberOfProcessIdsInList, 1);
    CHECK(l.ProcessIdList[0] == p1.dwProcessId || l.ProcessIdList[0] == p2.dwProcessId);
    jc_kill(&p1); jc_kill(&p2); CloseHandle(j);
}
TEST(jobpids_exact_fit) {
    REQUIRE(U(QueryInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION p1, p2; CHECK(j); if (!j) return;
    CHECK(jc_spawn_in_job(j, "jo_sleep", "20000", &p1)); CHECK(jc_spawn_in_job(j, "jo_sleep", "20000", &p2));
    BYTE buf[sizeof(JOBOBJECT_BASIC_PROCESS_ID_LIST) + sizeof(ULONG_PTR)]; memset(buf, 0, sizeof buf);
    JOBOBJECT_BASIC_PROCESS_ID_LIST *l = (void *)buf;
    SetLastError(0);
    CHECK(Q(j, JobObjectBasicProcessIdList, l, sizeof buf, NULL));
    EXPECT_EQ(l->NumberOfAssignedProcesses, 2); EXPECT_EQ(l->NumberOfProcessIdsInList, 2);
    DWORD a = (DWORD)l->ProcessIdList[0], b = (DWORD)l->ProcessIdList[1];
    CHECK((a == p1.dwProcessId && b == p2.dwProcessId) || (a == p2.dwProcessId && b == p1.dwProcessId));
    jc_kill(&p1); jc_kill(&p2); CloseHandle(j);
}
TEST(jobpids_larger_buffer_partial_use) {
    REQUIRE(U(QueryInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION p1; CHECK(j); if (!j) return;
    CHECK(jc_spawn_in_job(j, "jo_sleep", "20000", &p1));
    BYTE buf[256]; memset(buf, 0xEE, sizeof buf); JOBOBJECT_BASIC_PROCESS_ID_LIST *l = (void *)buf; DWORD rl = 0;
    CHECK(Q(j, JobObjectBasicProcessIdList, l, sizeof buf, &rl));
    EXPECT_EQ(l->NumberOfAssignedProcesses, 1); EXPECT_EQ(l->NumberOfProcessIdsInList, 1);
    EXPECT_EQ(l->ProcessIdList[0], p1.dwProcessId);
    t_note("pid list return length %lu", rl);
    jc_kill(&p1); CloseHandle(j);
}
TEST(jobpids_too_small_header) {
    REQUIRE(U(QueryInformationJobObject));
    HANDLE j = jc_job(); BYTE buf[64]; CHECK(j); if (!j) return;
    for (DWORD n = 0; n < 8; n++) {
        SetLastError(0); BOOL r = Q(j, JobObjectBasicProcessIdList, buf, n, NULL); DWORD e = GetLastError();
        CHECKF(!r && e == ERROR_BAD_LENGTH, "len %lu ret %d err %lu", n, r, e);
    }
    SetLastError(0); BOOL r = Q(j, JobObjectBasicProcessIdList, buf, 8, NULL);
    t_note("pid list header-only length 8: ret %d err %lu", r, r ? 0 : GetLastError());
    CloseHandle(j);
}
TEST(jobpids_list_shrinks_after_exit) {
    REQUIRE(U(QueryInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION p1, p2; CHECK(j); if (!j) return;
    CHECK(jc_spawn_in_job(j, "jo_sleep", "20000", &p1)); CHECK(jc_spawn_in_job(j, "jo_sleep", "20000", &p2));
    CHECK(jc_pid_in(j, p1.dwProcessId)); CHECK(jc_pid_in(j, p2.dwProcessId));
    TerminateProcess(p1.hProcess, 4); WaitForSingleObject(p1.hProcess, 5000);
    CHECK(jc_wait_active(j, 1, 3000));
    CHECK(!jc_pid_in(j, p1.dwProcessId)); CHECK(jc_pid_in(j, p2.dwProcessId));
    jc_kill(&p1); jc_kill(&p2); CloseHandle(j);
}

/* ---------- Set/Query round trips ---------- */
TEST(jobset_basic_limit_roundtrip_active_process) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    static const DWORD v[] = { 1, 2, 5, 100, 1000, 0x7FFFFFFF };
    for (unsigned i = 0; i < sizeof v / sizeof *v; i++) {
        JOBOBJECT_BASIC_LIMIT_INFORMATION b; memset(&b, 0, sizeof b);
        b.LimitFlags = JOB_OBJECT_LIMIT_ACTIVE_PROCESS; b.ActiveProcessLimit = v[i];
        CHECKF(S(j, JobObjectBasicLimitInformation, &b, sizeof b), "limit %lu err %lu", v[i], GetLastError());
        JOBOBJECT_BASIC_LIMIT_INFORMATION q; memset(&q, 0xCC, sizeof q);
        CHECK(Q(j, JobObjectBasicLimitInformation, &q, sizeof q, NULL));
        EXPECT_EQ(q.LimitFlags, JOB_OBJECT_LIMIT_ACTIVE_PROCESS); EXPECT_EQ(q.ActiveProcessLimit, v[i]);
    }
    CloseHandle(j);
}
TEST(jobset_basic_limit_clear_flags) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_BASIC_LIMIT_INFORMATION b; memset(&b, 0, sizeof b);
    b.LimitFlags = JOB_OBJECT_LIMIT_ACTIVE_PROCESS; b.ActiveProcessLimit = 9;
    CHECK(S(j, JobObjectBasicLimitInformation, &b, sizeof b));
    memset(&b, 0, sizeof b);
    CHECK(S(j, JobObjectBasicLimitInformation, &b, sizeof b));
    JOBOBJECT_BASIC_LIMIT_INFORMATION q; CHECK(Q(j, JobObjectBasicLimitInformation, &q, sizeof q, NULL));
    EXPECT_EQ(q.LimitFlags, 0); EXPECT_EQ(q.ActiveProcessLimit, 0);
    CloseHandle(j);
}
TEST(jobset_basic_limit_time_limits) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_BASIC_LIMIT_INFORMATION b; memset(&b, 0, sizeof b);
    b.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_TIME | JOB_OBJECT_LIMIT_JOB_TIME;
    b.PerProcessUserTimeLimit.QuadPart = 123456789; b.PerJobUserTimeLimit.QuadPart = 987654321;
    CHECK(S(j, JobObjectBasicLimitInformation, &b, sizeof b));
    JOBOBJECT_BASIC_LIMIT_INFORMATION q; memset(&q, 0, sizeof q); CHECK(Q(j, JobObjectBasicLimitInformation, &q, sizeof q, NULL));
    EXPECT_EQ(q.LimitFlags, b.LimitFlags);
    EXPECT_EQ(q.PerProcessUserTimeLimit.QuadPart, 123456789); EXPECT_EQ(q.PerJobUserTimeLimit.QuadPart, 987654321);
    CloseHandle(j);
}
TEST(jobset_basic_limit_process_time_only) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_BASIC_LIMIT_INFORMATION b; memset(&b, 0, sizeof b);
    b.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_TIME; b.PerProcessUserTimeLimit.QuadPart = 50000000;
    CHECK(S(j, JobObjectBasicLimitInformation, &b, sizeof b));
    JOBOBJECT_BASIC_LIMIT_INFORMATION q; CHECK(Q(j, JobObjectBasicLimitInformation, &q, sizeof q, NULL));
    EXPECT_EQ(q.LimitFlags, JOB_OBJECT_LIMIT_PROCESS_TIME); EXPECT_EQ(q.PerProcessUserTimeLimit.QuadPart, 50000000);
    EXPECT_EQ(q.PerJobUserTimeLimit.QuadPart, 0);
    CloseHandle(j);
}
TEST(jobset_basic_limit_negative_time) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_BASIC_LIMIT_INFORMATION b; memset(&b, 0, sizeof b);
    b.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_TIME; b.PerProcessUserTimeLimit.QuadPart = -1;
    SetLastError(0); BOOL r = S(j, JobObjectBasicLimitInformation, &b, sizeof b);
    t_note("negative PerProcessUserTimeLimit: ret %d err %lu", r, r ? 0 : GetLastError());
    if (!r) CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
    b.PerProcessUserTimeLimit.QuadPart = 0;
    SetLastError(0); r = S(j, JobObjectBasicLimitInformation, &b, sizeof b);
    t_note("zero PerProcessUserTimeLimit with flag: ret %d err %lu", r, r ? 0 : GetLastError());
    CloseHandle(j);
}
TEST(jobset_basic_limit_priority_class_roundtrip) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    static const DWORD pc[] = { IDLE_PRIORITY_CLASS, BELOW_NORMAL_PRIORITY_CLASS, NORMAL_PRIORITY_CLASS, ABOVE_NORMAL_PRIORITY_CLASS, HIGH_PRIORITY_CLASS };
    for (unsigned i = 0; i < sizeof pc / sizeof *pc; i++) {
        JOBOBJECT_BASIC_LIMIT_INFORMATION b; memset(&b, 0, sizeof b);
        b.LimitFlags = JOB_OBJECT_LIMIT_PRIORITY_CLASS; b.PriorityClass = pc[i];
        CHECKF(S(j, JobObjectBasicLimitInformation, &b, sizeof b), "class 0x%lx err %lu", pc[i], GetLastError());
        JOBOBJECT_BASIC_LIMIT_INFORMATION q; memset(&q, 0, sizeof q); CHECK(Q(j, JobObjectBasicLimitInformation, &q, sizeof q, NULL));
        EXPECT_EQ(q.PriorityClass, pc[i]); EXPECT_EQ(q.LimitFlags, JOB_OBJECT_LIMIT_PRIORITY_CLASS);
    }
    JOBOBJECT_BASIC_LIMIT_INFORMATION b; memset(&b, 0, sizeof b);
    b.LimitFlags = JOB_OBJECT_LIMIT_PRIORITY_CLASS; b.PriorityClass = REALTIME_PRIORITY_CLASS;
    SetLastError(0); BOOL r = S(j, JobObjectBasicLimitInformation, &b, sizeof b);
    t_note("REALTIME priority class limit: ret %d err %lu", r, r ? 0 : GetLastError());
    CloseHandle(j);
}
TEST(jobset_basic_limit_priority_class_unknown_maps_normal) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    static const DWORD bogus[] = { 0, 0x10, 0x1234, 0x40000000, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof bogus / sizeof *bogus; i++) {
        JOBOBJECT_BASIC_LIMIT_INFORMATION b; memset(&b, 0, sizeof b);
        b.LimitFlags = JOB_OBJECT_LIMIT_PRIORITY_CLASS; b.PriorityClass = bogus[i];
        SetLastError(0); BOOL r = S(j, JobObjectBasicLimitInformation, &b, sizeof b);
        CHECKF(r, "bogus class 0x%lx err %lu", bogus[i], GetLastError());
        JOBOBJECT_BASIC_LIMIT_INFORMATION q; memset(&q, 0, sizeof q);
        if (r) { CHECK(Q(j, JobObjectBasicLimitInformation, &q, sizeof q, NULL)); EXPECT_EQ(q.PriorityClass, NORMAL_PRIORITY_CLASS); }
    }
    CloseHandle(j);
}
TEST(jobset_basic_limit_priority_ignored_without_flag) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_BASIC_LIMIT_INFORMATION b; memset(&b, 0, sizeof b);
    b.LimitFlags = JOB_OBJECT_LIMIT_ACTIVE_PROCESS; b.ActiveProcessLimit = 3; b.PriorityClass = HIGH_PRIORITY_CLASS;
    SetLastError(0); BOOL r = S(j, JobObjectBasicLimitInformation, &b, sizeof b);
    t_note("priority class value without flag: ret %d err %lu", r, r ? 0 : GetLastError());
    JOBOBJECT_BASIC_LIMIT_INFORMATION q; memset(&q, 0, sizeof q);
    if (r) { CHECK(Q(j, JobObjectBasicLimitInformation, &q, sizeof q, NULL)); EXPECT_EQ(q.LimitFlags, JOB_OBJECT_LIMIT_ACTIVE_PROCESS); EXPECT_EQ(q.PriorityClass, NORMAL_PRIORITY_CLASS); }
    CloseHandle(j);
}
TEST(jobset_basic_limit_scheduling_class) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    for (DWORD sc = 0; sc <= 5; sc++) {
        JOBOBJECT_BASIC_LIMIT_INFORMATION b; memset(&b, 0, sizeof b);
        b.LimitFlags = JOB_OBJECT_LIMIT_SCHEDULING_CLASS; b.SchedulingClass = sc;
        CHECKF(S(j, JobObjectBasicLimitInformation, &b, sizeof b), "sched %lu err %lu", sc, GetLastError());
        JOBOBJECT_BASIC_LIMIT_INFORMATION q; memset(&q, 0, sizeof q); CHECK(Q(j, JobObjectBasicLimitInformation, &q, sizeof q, NULL));
        EXPECT_EQ(q.SchedulingClass, sc); EXPECT_EQ(q.LimitFlags, JOB_OBJECT_LIMIT_SCHEDULING_CLASS);
    }
    for (DWORD sc = 6; sc <= 9; sc++) {
        JOBOBJECT_BASIC_LIMIT_INFORMATION b; memset(&b, 0, sizeof b);
        b.LimitFlags = JOB_OBJECT_LIMIT_SCHEDULING_CLASS; b.SchedulingClass = sc;
        SetLastError(0); BOOL r = S(j, JobObjectBasicLimitInformation, &b, sizeof b);
        CHECKF(r || GetLastError() == ERROR_PRIVILEGE_NOT_HELD, "sched %lu ret %d err %lu", sc, r, GetLastError());
    }
    CloseHandle(j);
}
TEST(jobset_basic_limit_scheduling_class_invalid) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    static const DWORD bad[] = { 10, 11, 100, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        JOBOBJECT_BASIC_LIMIT_INFORMATION b; memset(&b, 0, sizeof b);
        b.LimitFlags = JOB_OBJECT_LIMIT_SCHEDULING_CLASS; b.SchedulingClass = bad[i];
        SetLastError(0); BOOL r = S(j, JobObjectBasicLimitInformation, &b, sizeof b); DWORD e = GetLastError();
        CHECKF(!r && e == ERROR_INVALID_PARAMETER, "sched %lu ret %d err %lu", bad[i], r, e);
    }
    CloseHandle(j);
}
TEST(jobset_basic_limit_affinity_roundtrip) {
    REQUIRE(U(SetInformationJobObject)); REQUIRE(U(GetProcessAffinityMask));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    DWORD_PTR pm = 0, sm = 0; CHECK(U(GetProcessAffinityMask)(GetCurrentProcess(), &pm, &sm));
    DWORD_PTR one = pm & (~pm + 1);
    JOBOBJECT_BASIC_LIMIT_INFORMATION b; memset(&b, 0, sizeof b);
    b.LimitFlags = JOB_OBJECT_LIMIT_AFFINITY; b.Affinity = one;
    CHECK(S(j, JobObjectBasicLimitInformation, &b, sizeof b));
    JOBOBJECT_BASIC_LIMIT_INFORMATION q; memset(&q, 0, sizeof q); CHECK(Q(j, JobObjectBasicLimitInformation, &q, sizeof q, NULL));
    EXPECT_EQ(q.Affinity, one); EXPECT_EQ(q.LimitFlags, JOB_OBJECT_LIMIT_AFFINITY);
    CloseHandle(j);
}
TEST(jobset_basic_limit_affinity_invalid) {
    REQUIRE(U(SetInformationJobObject)); REQUIRE(U(GetProcessAffinityMask));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    DWORD_PTR pm = 0, sm = 0; U(GetProcessAffinityMask)(GetCurrentProcess(), &pm, &sm);
    JOBOBJECT_BASIC_LIMIT_INFORMATION b; memset(&b, 0, sizeof b);
    b.LimitFlags = JOB_OBJECT_LIMIT_AFFINITY; b.Affinity = 0;
    SetLastError(0); BOOL r = S(j, JobObjectBasicLimitInformation, &b, sizeof b);
    CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    b.Affinity = ~sm;     /* only CPUs the system does not have */
    if (~sm) { SetLastError(0); r = S(j, JobObjectBasicLimitInformation, &b, sizeof b); CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
    CloseHandle(j);
}
TEST(jobset_basic_limit_invalid_flag_bits) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    static const DWORD bad[] = { 0x80000000, 0x40000000, 0x00100000, 0x00200000, 0x01000000, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION e; memset(&e, 0, sizeof e);
        e.BasicLimitInformation.LimitFlags = bad[i];
        SetLastError(0); BOOL r = jc_set_ext(j, &e); DWORD er = GetLastError();
        CHECKF(!r && er == ERROR_INVALID_PARAMETER, "ext flags 0x%lx ret %d err %lu", bad[i], r, er);
        JOBOBJECT_BASIC_LIMIT_INFORMATION b; memset(&b, 0, sizeof b); b.LimitFlags = bad[i];
        SetLastError(0); r = S(j, JobObjectBasicLimitInformation, &b, sizeof b); er = GetLastError();
        CHECKF(!r && er == ERROR_INVALID_PARAMETER, "basic flags 0x%lx ret %d err %lu", bad[i], r, er);
    }
    JOBOBJECT_BASIC_LIMIT_INFORMATION q; CHECK(Q(j, JobObjectBasicLimitInformation, &q, sizeof q, NULL)); EXPECT_EQ(q.LimitFlags, 0);
    CloseHandle(j);
}
TEST(jobset_basic_class_rejects_extended_only_flags) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    static const DWORD ext[] = { JOB_OBJECT_LIMIT_PROCESS_MEMORY, JOB_OBJECT_LIMIT_JOB_MEMORY, JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION,
        JOB_OBJECT_LIMIT_BREAKAWAY_OK, JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK, JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE };
    for (unsigned i = 0; i < sizeof ext / sizeof *ext; i++) {
        JOBOBJECT_BASIC_LIMIT_INFORMATION b; memset(&b, 0, sizeof b); b.LimitFlags = ext[i];
        SetLastError(0); BOOL r = S(j, JobObjectBasicLimitInformation, &b, sizeof b); DWORD er = GetLastError();
        CHECKF(!r && er == ERROR_INVALID_PARAMETER, "flag 0x%lx ret %d err %lu", ext[i], r, er);
    }
    CloseHandle(j);
}
TEST(jobset_extended_single_flags_roundtrip) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    static const DWORD f[] = { JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION, JOB_OBJECT_LIMIT_BREAKAWAY_OK, JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK,
        JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE, JOB_OBJECT_LIMIT_BREAKAWAY_OK | JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK,
        JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION | JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_BREAKAWAY_OK, 0 };
    for (unsigned i = 0; i < sizeof f / sizeof *f; i++) {
        CHECKF(jc_set_flags(j, f[i]), "flags 0x%lx err %lu", f[i], GetLastError());
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION q; memset(&q, 0xCC, sizeof q);
        CHECK(Q(j, JobObjectExtendedLimitInformation, &q, sizeof q, NULL));
        EXPECT_EQ(q.BasicLimitInformation.LimitFlags, f[i]);
    }
    CloseHandle(j);
}
TEST(jobset_extended_memory_limits_roundtrip) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION e; memset(&e, 0, sizeof e);
    e.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY | JOB_OBJECT_LIMIT_JOB_MEMORY;
    e.ProcessMemoryLimit = 64 << 20; e.JobMemoryLimit = 256 << 20;
    CHECK(jc_set_ext(j, &e));
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION q; memset(&q, 0, sizeof q); CHECK(Q(j, JobObjectExtendedLimitInformation, &q, sizeof q, NULL));
    EXPECT_EQ(q.ProcessMemoryLimit, 64 << 20); EXPECT_EQ(q.JobMemoryLimit, 256 << 20);
    EXPECT_EQ(q.BasicLimitInformation.LimitFlags, e.BasicLimitInformation.LimitFlags);
    CloseHandle(j);
}
TEST(jobset_extended_memory_value_ignored_without_flag) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION e; memset(&e, 0, sizeof e);
    e.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_ACTIVE_PROCESS; e.BasicLimitInformation.ActiveProcessLimit = 2;
    e.ProcessMemoryLimit = 1 << 20; e.JobMemoryLimit = 2 << 20;
    SetLastError(0); BOOL r = jc_set_ext(j, &e);
    t_note("memory values without flags: ret %d err %lu", r, r ? 0 : GetLastError());
    if (r) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION q; memset(&q, 0, sizeof q); CHECK(Q(j, JobObjectExtendedLimitInformation, &q, sizeof q, NULL));
        EXPECT_EQ(q.ProcessMemoryLimit, 0); EXPECT_EQ(q.JobMemoryLimit, 0);
    }
    CloseHandle(j);
}
TEST(jobset_extended_zero_memory_limit_with_flag) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION e; memset(&e, 0, sizeof e);
    e.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY;
    SetLastError(0); BOOL r = jc_set_ext(j, &e);
    t_note("PROCESS_MEMORY flag with limit 0: ret %d err %lu", r, r ? 0 : GetLastError());
    e.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_JOB_MEMORY;
    SetLastError(0); r = jc_set_ext(j, &e);
    t_note("JOB_MEMORY flag with limit 0: ret %d err %lu", r, r ? 0 : GetLastError());
    CloseHandle(j);
}
TEST(jobset_extended_huge_memory_limit) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION e; memset(&e, 0, sizeof e);
    e.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_PROCESS_MEMORY | JOB_OBJECT_LIMIT_JOB_MEMORY;
    e.ProcessMemoryLimit = (SIZE_T)1 << 46; e.JobMemoryLimit = (SIZE_T)1 << 46;
    SetLastError(0); BOOL r = jc_set_ext(j, &e);
    t_note("huge memory limits: ret %d err %lu", r, r ? 0 : GetLastError());
    e.ProcessMemoryLimit = (SIZE_T)-1; e.JobMemoryLimit = (SIZE_T)-1;
    SetLastError(0); r = jc_set_ext(j, &e);
    t_note("SIZE_T max memory limits: ret %d err %lu", r, r ? 0 : GetLastError());
    CloseHandle(j);
}
TEST(jobset_extended_io_info_ignored) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION e; memset(&e, 0, sizeof e);
    e.IoInfo.ReadOperationCount = 5; e.IoInfo.WriteTransferCount = 99; e.PeakJobMemoryUsed = 77; e.PeakProcessMemoryUsed = 88;
    SetLastError(0); BOOL r = jc_set_ext(j, &e);
    t_note("nonzero IoInfo/Peak fields on set: ret %d err %lu", r, r ? 0 : GetLastError());
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION q; memset(&q, 0xCC, sizeof q); CHECK(Q(j, JobObjectExtendedLimitInformation, &q, sizeof q, NULL));
    EXPECT_EQ(q.IoInfo.ReadOperationCount, 0); EXPECT_EQ(q.IoInfo.WriteTransferCount, 0);
    EXPECT_EQ(q.PeakJobMemoryUsed, 0); EXPECT_EQ(q.PeakProcessMemoryUsed, 0);
    CloseHandle(j);
}
TEST(jobset_extended_matches_basic_view) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION e; memset(&e, 0, sizeof e);
    e.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_ACTIVE_PROCESS | JOB_OBJECT_LIMIT_PRIORITY_CLASS | JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    e.BasicLimitInformation.ActiveProcessLimit = 4; e.BasicLimitInformation.PriorityClass = BELOW_NORMAL_PRIORITY_CLASS;
    CHECK(jc_set_ext(j, &e));
    JOBOBJECT_BASIC_LIMIT_INFORMATION b; memset(&b, 0, sizeof b); CHECK(Q(j, JobObjectBasicLimitInformation, &b, sizeof b, NULL));
    EXPECT_EQ(b.ActiveProcessLimit, 4); EXPECT_EQ(b.PriorityClass, BELOW_NORMAL_PRIORITY_CLASS);
    t_note("basic view of extended flags: 0x%lx", b.LimitFlags);
    CloseHandle(j);
}
TEST(jobset_preserve_job_time_rules) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_BASIC_LIMIT_INFORMATION b; memset(&b, 0, sizeof b);
    b.LimitFlags = JOB_OBJECT_LIMIT_JOB_TIME | JOB_OBJECT_LIMIT_PRESERVE_JOB_TIME; b.PerJobUserTimeLimit.QuadPart = 100000000;
    SetLastError(0); BOOL r = S(j, JobObjectBasicLimitInformation, &b, sizeof b);
    CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    b.LimitFlags = JOB_OBJECT_LIMIT_PRESERVE_JOB_TIME;
    SetLastError(0); r = S(j, JobObjectBasicLimitInformation, &b, sizeof b);
    t_note("PRESERVE_JOB_TIME alone: ret %d err %lu", r, r ? 0 : GetLastError());
    CloseHandle(j);
}
TEST(jobset_working_set_limits) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_BASIC_LIMIT_INFORMATION b; memset(&b, 0, sizeof b);
    b.LimitFlags = JOB_OBJECT_LIMIT_WORKINGSET; b.MinimumWorkingSetSize = 1 << 20; b.MaximumWorkingSetSize = 8 << 20;
    SetLastError(0); BOOL r = S(j, JobObjectBasicLimitInformation, &b, sizeof b);
    DWORD e = GetLastError();
    t_note("working set limit set: ret %d err %lu", r, r ? 0 : e);
    if (r) {
        JOBOBJECT_BASIC_LIMIT_INFORMATION q; memset(&q, 0, sizeof q); CHECK(Q(j, JobObjectBasicLimitInformation, &q, sizeof q, NULL));
        EXPECT_EQ(q.MinimumWorkingSetSize, 1 << 20); EXPECT_EQ(q.MaximumWorkingSetSize, 8 << 20);
    } else CHECK(e == ERROR_PRIVILEGE_NOT_HELD || e == ERROR_ACCESS_DENIED);
    b.MinimumWorkingSetSize = 8 << 20; b.MaximumWorkingSetSize = 1 << 20;
    SetLastError(0); r = S(j, JobObjectBasicLimitInformation, &b, sizeof b);
    CHECK(!r);
    t_note("min>max working set err %lu", GetLastError());
    CloseHandle(j);
}
TEST(jobset_limits_survive_unrelated_set) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION e; memset(&e, 0, sizeof e);
    e.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_ACTIVE_PROCESS; e.BasicLimitInformation.ActiveProcessLimit = 6;
    CHECK(jc_set_ext(j, &e));
    JOBOBJECT_BASIC_UI_RESTRICTIONS u = { JOB_OBJECT_UILIMIT_DESKTOP };
    CHECK(S(j, JobObjectBasicUIRestrictions, &u, sizeof u));
    JOBOBJECT_END_OF_JOB_TIME_INFORMATION t = { JOB_OBJECT_POST_AT_END_OF_JOB };
    S(j, JobObjectEndOfJobTimeInformation, &t, sizeof t);
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION q; CHECK(Q(j, JobObjectExtendedLimitInformation, &q, sizeof q, NULL));
    EXPECT_EQ(q.BasicLimitInformation.ActiveProcessLimit, 6);
    CloseHandle(j);
}
TEST(jobset_extended_replaces_all_limits) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION e; memset(&e, 0, sizeof e);
    e.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_ACTIVE_PROCESS | JOB_OBJECT_LIMIT_PROCESS_MEMORY;
    e.BasicLimitInformation.ActiveProcessLimit = 6; e.ProcessMemoryLimit = 1 << 26;
    CHECK(jc_set_ext(j, &e));
    memset(&e, 0, sizeof e);
    e.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_JOB_MEMORY; e.JobMemoryLimit = 1 << 27;
    CHECK(jc_set_ext(j, &e));
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION q; memset(&q, 0xCC, sizeof q); CHECK(Q(j, JobObjectExtendedLimitInformation, &q, sizeof q, NULL));
    EXPECT_EQ(q.BasicLimitInformation.LimitFlags, JOB_OBJECT_LIMIT_JOB_MEMORY);
    EXPECT_EQ(q.BasicLimitInformation.ActiveProcessLimit, 0); EXPECT_EQ(q.ProcessMemoryLimit, 0); EXPECT_EQ(q.JobMemoryLimit, 1 << 27);
    CloseHandle(j);
}

/* ---------- UI restrictions ---------- */
TEST(jobset_ui_each_bit_roundtrip) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    for (int b = 0; b < 8; b++) {
        JOBOBJECT_BASIC_UI_RESTRICTIONS u = { 1u << b };
        CHECKF(S(j, JobObjectBasicUIRestrictions, &u, sizeof u), "bit %d err %lu", b, GetLastError());
        JOBOBJECT_BASIC_UI_RESTRICTIONS q = { 0xCC }; CHECK(Q(j, JobObjectBasicUIRestrictions, &q, sizeof q, NULL));
        EXPECT_EQ(q.UIRestrictionsClass, 1u << b);
    }
    JOBOBJECT_BASIC_UI_RESTRICTIONS u = { JOB_OBJECT_UILIMIT_ALL };
    CHECK(S(j, JobObjectBasicUIRestrictions, &u, sizeof u));
    JOBOBJECT_BASIC_UI_RESTRICTIONS q = { 0 }; CHECK(Q(j, JobObjectBasicUIRestrictions, &q, sizeof q, NULL));
    EXPECT_EQ(q.UIRestrictionsClass, JOB_OBJECT_UILIMIT_ALL);
    u.UIRestrictionsClass = 0; CHECK(S(j, JobObjectBasicUIRestrictions, &u, sizeof u));
    CHECK(Q(j, JobObjectBasicUIRestrictions, &q, sizeof q, NULL)); EXPECT_EQ(q.UIRestrictionsClass, 0);
    CloseHandle(j);
}
TEST(jobset_ui_invalid_bits) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    static const DWORD bad[] = { 0x100, 0x200, 0x80000000, 0xFFFFFFFF, 0x1FF };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        JOBOBJECT_BASIC_UI_RESTRICTIONS u = { bad[i] };
        SetLastError(0); BOOL r = S(j, JobObjectBasicUIRestrictions, &u, sizeof u); DWORD e = GetLastError();
        t_note("UI restrictions bits 0x%lx: ret %d err %lu", bad[i], r, e);   /* valid-mask not verifiable from user-mode decomp */
        if (r) { JOBOBJECT_BASIC_UI_RESTRICTIONS z = { 0 }; S(j, JobObjectBasicUIRestrictions, &z, sizeof z); }
    }
    CloseHandle(j);
}
TEST(jobset_ui_cumulative_replace) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_BASIC_UI_RESTRICTIONS u = { JOB_OBJECT_UILIMIT_DESKTOP | JOB_OBJECT_UILIMIT_HANDLES };
    CHECK(S(j, JobObjectBasicUIRestrictions, &u, sizeof u));
    u.UIRestrictionsClass = JOB_OBJECT_UILIMIT_GLOBALATOMS;
    CHECK(S(j, JobObjectBasicUIRestrictions, &u, sizeof u));
    JOBOBJECT_BASIC_UI_RESTRICTIONS q = { 0 }; CHECK(Q(j, JobObjectBasicUIRestrictions, &q, sizeof q, NULL));
    t_note("UI restriction set semantics: replaced=%d value=0x%lx", q.UIRestrictionsClass == JOB_OBJECT_UILIMIT_GLOBALATOMS, q.UIRestrictionsClass);
    CHECK((q.UIRestrictionsClass & JOB_OBJECT_UILIMIT_GLOBALATOMS) != 0);
    CloseHandle(j);
}

/* ---------- EndOfJobTime ---------- */
TEST(jobset_end_of_job_time_values) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_END_OF_JOB_TIME_INFORMATION t = { JOB_OBJECT_POST_AT_END_OF_JOB };
    CHECK(S(j, JobObjectEndOfJobTimeInformation, &t, sizeof t));
    JOBOBJECT_END_OF_JOB_TIME_INFORMATION q = { 0xCC }; CHECK(Q(j, JobObjectEndOfJobTimeInformation, &q, sizeof q, NULL));
    EXPECT_EQ(q.EndOfJobTimeAction, JOB_OBJECT_POST_AT_END_OF_JOB);
    t.EndOfJobTimeAction = JOB_OBJECT_TERMINATE_AT_END_OF_JOB;
    CHECK(S(j, JobObjectEndOfJobTimeInformation, &t, sizeof t));
    q.EndOfJobTimeAction = 0xCC; CHECK(Q(j, JobObjectEndOfJobTimeInformation, &q, sizeof q, NULL));
    EXPECT_EQ(q.EndOfJobTimeAction, JOB_OBJECT_TERMINATE_AT_END_OF_JOB);
    CloseHandle(j);
}
TEST(jobset_end_of_job_time_invalid) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    static const DWORD bad[] = { 2, 3, 0x100, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        JOBOBJECT_END_OF_JOB_TIME_INFORMATION t = { bad[i] };
        SetLastError(0); BOOL r = S(j, JobObjectEndOfJobTimeInformation, &t, sizeof t); DWORD e = GetLastError();
        t_note("end-of-job-time action %lu: ret %d err %lu", bad[i], r, e);   /* validation lives in the kernel; unverified */
    }
    CloseHandle(j);
}

/* ---------- Completion port association ---------- */
TEST(jobset_assoc_port_basic) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(), p = jc_port(); CHECK(j && p); if (!j || !p) return;
    CHECK(jc_assoc(j, p, 0x1234));
    JOBOBJECT_ASSOCIATE_COMPLETION_PORT q; memset(&q, 0, sizeof q);
    SetLastError(0); BOOL r = Q(j, JobObjectAssociateCompletionPortInformation, &q, sizeof q, NULL);
    t_note("query associated port: ret %d err %lu key %p port %p", r, r ? 0 : GetLastError(), q.CompletionKey, q.CompletionPort);
    if (r) EXPECT_EQ(q.CompletionKey, 0x1234);
    CloseHandle(p); CloseHandle(j);
}
TEST(jobset_assoc_port_invalid_handles) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(), ev = CreateEventA(NULL, TRUE, FALSE, NULL); CHECK(j); if (!j) return;
    JOBOBJECT_ASSOCIATE_COMPLETION_PORT a;
    a.CompletionKey = (PVOID)1; a.CompletionPort = NULL;
    SetLastError(0); CHECK(!S(j, JobObjectAssociateCompletionPortInformation, &a, sizeof a)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    a.CompletionPort = INVALID_HANDLE_VALUE;
    SetLastError(0); CHECK(!S(j, JobObjectAssociateCompletionPortInformation, &a, sizeof a)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    a.CompletionPort = ev;
    SetLastError(0); CHECK(!S(j, JobObjectAssociateCompletionPortInformation, &a, sizeof a)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    a.CompletionPort = (HANDLE)(ULONG_PTR)0x77FFC;
    SetLastError(0); CHECK(!S(j, JobObjectAssociateCompletionPortInformation, &a, sizeof a)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev); CloseHandle(j);
}
TEST(jobset_assoc_port_twice) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(), p1 = jc_port(), p2 = jc_port(); CHECK(j && p1 && p2); if (!j || !p1 || !p2) return;
    CHECK(jc_assoc(j, p1, 1));
    SetLastError(0); BOOL r = jc_assoc(j, p2, 2);
    t_note("second association with different port: ret %d err %lu", r, r ? 0 : GetLastError());
    SetLastError(0); r = jc_assoc(j, p1, 3);
    t_note("second association with same port: ret %d err %lu", r, r ? 0 : GetLastError());
    CloseHandle(p1); CloseHandle(p2); CloseHandle(j);
}
TEST(jobset_assoc_port_two_jobs_one_port) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE a = jc_job(), b = jc_job(), p = jc_port(); CHECK(a && b && p); if (!a || !b || !p) return;
    CHECK(jc_assoc(a, p, 10)); CHECK(jc_assoc(b, p, 20));
    CloseHandle(p); CloseHandle(a); CloseHandle(b);
}
TEST(jobset_assoc_port_needs_set_attributes) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(), p = jc_port(); CHECK(j && p); if (!j || !p) return;
    HANDLE q = jc_dup(j, JOB_OBJECT_QUERY);
    if (q) { SetLastError(0); CHECK(!jc_assoc(q, p, 1)); EXPECT_ERR(ERROR_ACCESS_DENIED); CloseHandle(q); }
    CloseHandle(p); CloseHandle(j);
}

/* ---------- Security limit / misc classes (observation + error paths) ---------- */
TEST(jobset_security_limit_invalid_flags) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_SECURITY_LIMIT_INFORMATION s; memset(&s, 0, sizeof s);
    s.SecurityLimitFlags = 0x10;
    SetLastError(0); BOOL r = S(j, JobObjectSecurityLimitInformation, &s, sizeof s);
    t_note("security limit invalid flag 0x10: ret %d err %lu", r, r ? 0 : GetLastError());
    CHECK(!r);
    s.SecurityLimitFlags = 0x80000000;
    SetLastError(0); r = S(j, JobObjectSecurityLimitInformation, &s, sizeof s); CHECK(!r);
    SetLastError(0); r = S(j, JobObjectSecurityLimitInformation, &s, sizeof s - 1); CHECK(!r);
    t_note("security limit short length err %lu", GetLastError());
    CloseHandle(j);
}
TEST(jobquery_security_limit_observe) {
    REQUIRE(U(QueryInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    BYTE buf[256]; DWORD rl = 0; SetLastError(0);
    BOOL r = Q(j, JobObjectSecurityLimitInformation, buf, sizeof(JOBOBJECT_SECURITY_LIMIT_INFORMATION), &rl);
    t_note("query security limit: ret %d err %lu rl %lu", r, r ? 0 : GetLastError(), rl);
    CloseHandle(j);
}
TEST(jobset_notification_limit_observe) {
    REQUIRE(U(SetInformationJobObject)); REQUIRE(U(QueryInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_NOTIFICATION_LIMIT_INFORMATION n; memset(&n, 0, sizeof n);
    n.LimitFlags = JOB_OBJECT_LIMIT_JOB_MEMORY; n.JobMemoryLimit = 64 << 20;
    SetLastError(0); BOOL r = S(j, JobObjectNotificationLimitInformation, &n, sizeof n);
    t_note("set notification limit job memory: ret %d err %lu", r, r ? 0 : GetLastError());
    if (r) {
        JOBOBJECT_NOTIFICATION_LIMIT_INFORMATION q; memset(&q, 0, sizeof q);
        CHECK(Q(j, JobObjectNotificationLimitInformation, &q, sizeof q, NULL));
        EXPECT_EQ(q.JobMemoryLimit, 64 << 20); EXPECT_EQ(q.LimitFlags & JOB_OBJECT_LIMIT_JOB_MEMORY, JOB_OBJECT_LIMIT_JOB_MEMORY);
    }
    n.LimitFlags = JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
    SetLastError(0); r = S(j, JobObjectNotificationLimitInformation, &n, sizeof n);
    t_note("notification limit with invalid flag: ret %d err %lu", r, r ? 0 : GetLastError());
    CloseHandle(j);
}
TEST(jobquery_limit_violation_observe) {
    REQUIRE(U(QueryInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_LIMIT_VIOLATION_INFORMATION v; memset(&v, 0xCC, sizeof v); DWORD rl = 0; SetLastError(0);
    BOOL r = Q(j, JobObjectLimitViolationInformation, &v, sizeof v, &rl);
    t_note("query limit violation (fresh): ret %d err %lu rl %lu viol 0x%lx", r, r ? 0 : GetLastError(), rl, r ? v.ViolationLimitFlags : 0);
    if (r) EXPECT_EQ(v.ViolationLimitFlags, 0);
    CloseHandle(j);
}
TEST(jobquery_net_rate_control_observe) {
    REQUIRE(U(QueryInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_NET_RATE_CONTROL_INFORMATION n; memset(&n, 0xCC, sizeof n); DWORD rl = 0; SetLastError(0);
    BOOL r = Q(j, JobObjectNetRateControlInformation, &n, sizeof n, &rl);
    t_note("query net rate control: ret %d err %lu rl %lu flags 0x%x", r, r ? 0 : GetLastError(), rl, r ? n.ControlFlags : 0);
    if (r) EXPECT_EQ(n.ControlFlags & JOB_OBJECT_NET_RATE_CONTROL_ENABLE, 0);
    CloseHandle(j);
}
TEST(jobset_net_rate_control_invalid_flags) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_NET_RATE_CONTROL_INFORMATION n; memset(&n, 0, sizeof n);
    n.ControlFlags = (JOB_OBJECT_NET_RATE_CONTROL_FLAGS)0x80;
    SetLastError(0); BOOL r = S(j, JobObjectNetRateControlInformation, &n, sizeof n);
    CHECK(!r);
    t_note("net rate invalid flag err %lu", GetLastError());
    n.ControlFlags = JOB_OBJECT_NET_RATE_CONTROL_DSCP_TAG; n.DscpTag = 200;   /* > MAX_DSCP_TAG */
    SetLastError(0); r = S(j, JobObjectNetRateControlInformation, &n, sizeof n);
    CHECK(!r);
    t_note("net rate dscp tag 200 err %lu", GetLastError());
    CloseHandle(j);
}
TEST(jobquery_group_info_observe) {
    REQUIRE(U(QueryInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    BYTE buf[256]; DWORD rl = 0; SetLastError(0);
    BOOL r = Q(j, JobObjectGroupInformation, buf, sizeof(USHORT), &rl);
    t_note("query group info len 2: ret %d err %lu rl %lu", r, r ? 0 : GetLastError(), rl);
    r = Q(j, JobObjectGroupInformation, buf, sizeof buf, &rl);
    t_note("query group info len 256: ret %d err %lu rl %lu", r, r ? 0 : GetLastError(), rl);
    r = Q(j, JobObjectGroupInformationEx, buf, sizeof buf, &rl);
    t_note("query group info ex len 256: ret %d err %lu rl %lu", r, r ? 0 : GetLastError(), rl);
    CloseHandle(j);
}
TEST(jobset_group_info_group0) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    USHORT g = 0; SetLastError(0);
    BOOL r = S(j, JobObjectGroupInformation, &g, sizeof g);
    t_note("set group info {0}: ret %d err %lu", r, r ? 0 : GetLastError());
    USHORT bad[1] = { 0xFFFF }; SetLastError(0);
    r = S(j, JobObjectGroupInformation, bad, sizeof bad);
    CHECK(!r);
    t_note("set group info {0xFFFF}: err %lu", GetLastError());
    SetLastError(0);
    r = S(j, JobObjectGroupInformation, &g, 0);
    CHECK(!r);
    t_note("set group info len 0: err %lu", GetLastError());
    CloseHandle(j);
}
TEST(jobquery_silo_classes_on_plain_job) {
    REQUIRE(U(QueryInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    SILOOBJECT_BASIC_INFORMATION s; memset(&s, 0, sizeof s); SetLastError(0);
    BOOL r = Q(j, JobObjectSiloBasicInformation, &s, sizeof s, NULL);
    t_note("silo basic info on plain job: ret %d err %lu", r, r ? 0 : GetLastError());
    CHECK(!r);
    CloseHandle(j);
}
TEST(jobquery_completion_filter_counter_observe) {
    REQUIRE(U(QueryInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    DWORD v = 0xCC, rl = 0; SetLastError(0);
    BOOL r = Q(j, JobObjectCompletionFilter, &v, sizeof v, &rl);
    t_note("completion filter: ret %d err %lu v=0x%lx rl %lu", r, r ? 0 : GetLastError(), v, rl);
    v = 0xCC; SetLastError(0);
    r = Q(j, JobObjectCompletionCounter, &v, sizeof v, &rl);
    t_note("completion counter: ret %d err %lu v=0x%lx rl %lu", r, r ? 0 : GetLastError(), v, rl);
    CloseHandle(j);
}

/* ---------- CPU rate control ---------- */
static BOOL cpu_set(HANDLE j, DWORD flags, DWORD val) { JC_CPU_RATE c = { flags, val }; return S(j, JobObjectCpuRateControlInformation, &c, sizeof c); }
TEST(jobcpu_hard_cap_roundtrip) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    SetLastError(0);
    BOOL r = cpu_set(j, JOB_OBJECT_CPU_RATE_CONTROL_ENABLE | JOB_OBJECT_CPU_RATE_CONTROL_HARD_CAP, 5000);
    CHECKF(r, "err %lu", GetLastError());
    JC_CPU_RATE q = { 0xCC, 0xCC }; DWORD rl = 0;
    if (r) {
        CHECK(Q(j, JobObjectCpuRateControlInformation, &q, sizeof q, &rl));
        EXPECT_EQ(q.ControlFlags, JOB_OBJECT_CPU_RATE_CONTROL_ENABLE | JOB_OBJECT_CPU_RATE_CONTROL_HARD_CAP); EXPECT_EQ(q.Value, 5000);
        EXPECT_EQ(rl, 8);
    }
    CloseHandle(j);
}
TEST(jobcpu_weight_roundtrip) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    for (DWORD w = 1; w <= 9; w++) {
        BOOL r = cpu_set(j, JOB_OBJECT_CPU_RATE_CONTROL_ENABLE | JOB_OBJECT_CPU_RATE_CONTROL_WEIGHT_BASED, w);
        CHECKF(r, "weight %lu err %lu", w, GetLastError());
        JC_CPU_RATE q = { 0xCC, 0xCC };
        if (r) { CHECK(Q(j, JobObjectCpuRateControlInformation, &q, sizeof q, NULL)); EXPECT_EQ(q.Value, w); EXPECT_EQ(q.ControlFlags, JOB_OBJECT_CPU_RATE_CONTROL_ENABLE | JOB_OBJECT_CPU_RATE_CONTROL_WEIGHT_BASED); }
    }
    CloseHandle(j);
}
TEST(jobcpu_hard_cap_rate_bounds) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    static const DWORD ok[] = { 1, 100, 5000, 9999, 10000 };
    for (unsigned i = 0; i < sizeof ok / sizeof *ok; i++) CHECKF(cpu_set(j, 5, ok[i]), "rate %lu err %lu", ok[i], GetLastError());
    static const DWORD bad[] = { 0, 10001, 20000, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SetLastError(0); BOOL r = cpu_set(j, 5, bad[i]); DWORD e = GetLastError();
        CHECKF(!r && e == ERROR_INVALID_PARAMETER, "rate %lu ret %d err %lu", bad[i], r, e);
    }
    CloseHandle(j);
}
TEST(jobcpu_weight_bounds) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    static const DWORD bad[] = { 0, 10, 11, 100, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SetLastError(0); BOOL r = cpu_set(j, 3, bad[i]); DWORD e = GetLastError();
        CHECKF(!r && e == ERROR_INVALID_PARAMETER, "weight %lu ret %d err %lu", bad[i], r, e);
    }
    CloseHandle(j);
}
TEST(jobcpu_invalid_flag_combinations) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    static const DWORD bad[] = {
        JOB_OBJECT_CPU_RATE_CONTROL_ENABLE,                                                              /* neither weight nor hard cap */
        JOB_OBJECT_CPU_RATE_CONTROL_ENABLE | JOB_OBJECT_CPU_RATE_CONTROL_WEIGHT_BASED | JOB_OBJECT_CPU_RATE_CONTROL_HARD_CAP, /* both */
        JOB_OBJECT_CPU_RATE_CONTROL_HARD_CAP,                                                            /* without ENABLE */
        JOB_OBJECT_CPU_RATE_CONTROL_WEIGHT_BASED,
        0x20, 0x80000000, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        SetLastError(0); BOOL r = cpu_set(j, bad[i], 5000); DWORD e = GetLastError();
        if (i < 2) CHECKF(!r && e == ERROR_INVALID_PARAMETER, "flags 0x%lx ret %d err %lu", bad[i], r, e);
        else t_note("cpu rate flags 0x%lx: ret %d err %lu", bad[i], r, e);   /* kernel-side flag validation unverified */
    }
    CloseHandle(j);
}
TEST(jobcpu_disable_with_zero_flags) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    CHECK(cpu_set(j, 5, 3000));
    SetLastError(0); BOOL r = cpu_set(j, 0, 0);
    t_note("disable cpu rate control: ret %d err %lu", r, r ? 0 : GetLastError());
    CloseHandle(j);
}
TEST(jobcpu_notify_and_min_max_observe) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    SetLastError(0); BOOL r = cpu_set(j, JOB_OBJECT_CPU_RATE_CONTROL_ENABLE | JOB_OBJECT_CPU_RATE_CONTROL_HARD_CAP | JOB_OBJECT_CPU_RATE_CONTROL_NOTIFY, 2500);
    t_note("hard cap + notify: ret %d err %lu", r, r ? 0 : GetLastError());
    r = cpu_set(j, JOB_OBJECT_CPU_RATE_CONTROL_ENABLE | JOB_OBJECT_CPU_RATE_CONTROL_MIN_MAX_RATE, (2000u << 16) | 4000u);
    t_note("min/max rate (min 20%% max 40%%): ret %d err %lu", r, r ? 0 : GetLastError());
    CloseHandle(j);
}
TEST(jobcpu_bad_length) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JC_CPU_RATE c = { 5, 5000 };
    SetLastError(0); CHECK(!S(j, JobObjectCpuRateControlInformation, &c, 4)); EXPECT_ERR(ERROR_BAD_LENGTH);
    SetLastError(0); CHECK(!S(j, JobObjectCpuRateControlInformation, &c, 0)); EXPECT_ERR(ERROR_BAD_LENGTH);
    SetLastError(0); BOOL r = S(j, JobObjectCpuRateControlInformation, &c, 12);
    t_note("cpu rate set with length 12: ret %d err %lu", r, r ? 0 : GetLastError());
    BYTE buf[64]; SetLastError(0); CHECK(!Q(j, JobObjectCpuRateControlInformation, buf, 4, NULL)); EXPECT_ERR(ERROR_BAD_LENGTH);
    CloseHandle(j);
}
TEST(jobcpu_query_unset_observe) {
    REQUIRE(U(QueryInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JC_CPU_RATE q = { 0xCC, 0xCC }; DWORD rl = 0; SetLastError(0);
    BOOL r = Q(j, JobObjectCpuRateControlInformation, &q, sizeof q, &rl);
    t_note("cpu rate query unset: ret %d err %lu flags 0x%lx", r, r ? 0 : GetLastError(), q.ControlFlags);
    if (r) EXPECT_EQ(q.ControlFlags & JOB_OBJECT_CPU_RATE_CONTROL_ENABLE, 0);
    CloseHandle(j);
}
