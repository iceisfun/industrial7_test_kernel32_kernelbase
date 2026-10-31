#include "jc.h"

UDECL(BOOL, UserHandleGrantAccess, (HANDLE, HANDLE, BOOL));

/* ---------- UserHandleGrantAccess (user32) ---------- */
TEST(jobmisc_usergrant_invalid_job) {
    REQUIRE(UN(UserHandleGrantAccess));
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SetLastError(0); BOOL r = UN(UserHandleGrantAccess)(ev, NULL, TRUE);
    CHECK(!r); t_note("grant(user=event, job=NULL): err %lu", GetLastError());
    SetLastError(0); r = UN(UserHandleGrantAccess)(ev, ev, TRUE);
    CHECK(!r); t_note("grant(user=event, job=event): err %lu", GetLastError());
    SetLastError(0); r = UN(UserHandleGrantAccess)(NULL, NULL, FALSE);
    CHECK(!r); t_note("grant(NULL, NULL): err %lu", GetLastError());
    CloseHandle(ev);
}
TEST(jobmisc_usergrant_bad_user_handle) {
    REQUIRE(UN(UserHandleGrantAccess));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    SetLastError(0); BOOL r = UN(UserHandleGrantAccess)(NULL, j, TRUE);
    CHECK(!r); t_note("grant(NULL user handle, job): err %lu", GetLastError());
    SetLastError(0); r = UN(UserHandleGrantAccess)((HANDLE)(ULONG_PTR)0x7FFFFFF0, j, TRUE);
    CHECK(!r); t_note("grant(bogus user handle, job): err %lu", GetLastError());
    SetLastError(0); r = UN(UserHandleGrantAccess)((HANDLE)(ULONG_PTR)0x10001, j, FALSE);
    CHECK(!r);
    CloseHandle(j);
}
TEST(jobmisc_usergrant_real_window_requires_ui_limit) {
    REQUIRE(UN(UserHandleGrantAccess)); REQUIRE(U(CreateWindowExA));
    HWND w = U(CreateWindowExA)(0, "STATIC", "k32t_jo_grant", WS_POPUP, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    if (!w) { t_note("no window available (%lu)", GetLastError()); return; }
    HANDLE j = jc_job(); CHECK(j);
    if (j) {
        SetLastError(0); BOOL r = UN(UserHandleGrantAccess)((HANDLE)w, j, TRUE);
        t_note("grant window to job without UILIMIT_HANDLES: ret %d err %lu", r, r ? 0 : GetLastError());
        JOBOBJECT_BASIC_UI_RESTRICTIONS u = { JOB_OBJECT_UILIMIT_HANDLES };
        CHECK(U(SetInformationJobObject)(j, JobObjectBasicUIRestrictions, &u, sizeof u));
        SetLastError(0); r = UN(UserHandleGrantAccess)((HANDLE)w, j, TRUE);
        t_note("grant window to job with UILIMIT_HANDLES: ret %d err %lu", r, r ? 0 : GetLastError());
        SetLastError(0); r = UN(UserHandleGrantAccess)((HANDLE)w, j, FALSE);
        t_note("revoke: ret %d err %lu", r, r ? 0 : GetLastError());
        CloseHandle(j);
    }
    U(DestroyWindow)(w);
}
TEST(jobmisc_usergrant_job_needs_set_attributes) {
    REQUIRE(UN(UserHandleGrantAccess)); REQUIRE(U(CreateWindowExA));
    HWND w = U(CreateWindowExA)(0, "STATIC", "k32t_jo_grant2", WS_POPUP, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
    if (!w) return;
    HANDLE j = jc_job(); HANDLE q = j ? jc_dup(j, JOB_OBJECT_QUERY) : NULL;
    if (q) { SetLastError(0); BOOL r = UN(UserHandleGrantAccess)((HANDLE)w, q, TRUE); CHECK(!r); t_note("grant with query-only job handle: err %lu", GetLastError()); CloseHandle(q); }
    if (j) CloseHandle(j);
    U(DestroyWindow)(w);
}

/* ---------- CreateJobSet ---------- */
TEST(jobmisc_createjobset_zero_jobs) {
    REQUIRE(U(CreateJobSet));
    SetLastError(0); BOOL r = U(CreateJobSet)(0, NULL, 0);
    CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(jobmisc_createjobset_nonzero_flags) {
    REQUIRE(U(CreateJobSet));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOB_SET_ARRAY a[1]; a[0].JobHandle = j; a[0].MemberLevel = 0; a[0].Flags = 0;
    SetLastError(0); BOOL r = U(CreateJobSet)(1, a, 1);
    CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SetLastError(0); r = U(CreateJobSet)(1, a, 0x80000000);
    CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CloseHandle(j);
}
TEST(jobmisc_createjobset_null_array) {
    REQUIRE(U(CreateJobSet));
    SetLastError(0); BOOL r = U(CreateJobSet)(1, NULL, 0);
    CHECK(!r); t_note("CreateJobSet(1, NULL, 0): err %lu", GetLastError());
    SetLastError(0); r = U(CreateJobSet)(2, NULL, 0);
    CHECK(!r); t_note("CreateJobSet(2, NULL, 0): err %lu", GetLastError());
}
TEST(jobmisc_createjobset_invalid_job_handles) {
    REQUIRE(U(CreateJobSet));
    JOB_SET_ARRAY a[2]; memset(a, 0, sizeof a);
    SetLastError(0); BOOL r = U(CreateJobSet)(1, a, 0);
    CHECK(!r); t_note("CreateJobSet(job NULL): err %lu", GetLastError());
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL); a[0].JobHandle = ev; a[1].JobHandle = ev;
    SetLastError(0); r = U(CreateJobSet)(2, a, 0);
    CHECK(!r); t_note("CreateJobSet(event handles): err %lu", GetLastError());
    CloseHandle(ev);
}
TEST(jobmisc_createjobset_single_job_observe) {
    REQUIRE(U(CreateJobSet));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOB_SET_ARRAY a[1]; a[0].JobHandle = j; a[0].MemberLevel = 0; a[0].Flags = 0;
    SetLastError(0); BOOL r = U(CreateJobSet)(1, a, 0);
    t_note("CreateJobSet(1 valid job, level 0): ret %d err %lu", r, r ? 0 : GetLastError());
    CloseHandle(j);
}
TEST(jobmisc_createjobset_two_jobs_same_level_observe) {
    REQUIRE(U(CreateJobSet));
    HANDLE a = jc_job(), b = jc_job(); CHECK(a && b); if (!a || !b) return;
    JOB_SET_ARRAY s[2]; s[0].JobHandle = a; s[0].MemberLevel = 0; s[0].Flags = 0; s[1].JobHandle = b; s[1].MemberLevel = 0; s[1].Flags = 0;
    SetLastError(0); BOOL r = U(CreateJobSet)(2, s, 0);
    t_note("CreateJobSet(2 jobs, level 0): ret %d err %lu", r, r ? 0 : GetLastError());
    CloseHandle(a); CloseHandle(b);
}

/* ---------- Io rate control ---------- */
TEST(jobmisc_ioratequery_null_job) {
    REQUIRE(JC_QueryIoRate);
    JC_IO_RATE_INFO *info = NULL; ULONG cnt = 0xFF;
    SetLastError(0); DWORD r = JC_QueryIoRate(NULL, NULL, &info, &cnt);
    t_note("QueryIoRate(NULL job): ret %lu err %lu", r, GetLastError());
    CHECK(r == 0 || GetLastError() != 0);
    if (r && info) JC_FreeMemoryJobObject(info);
}
TEST(jobmisc_ioratequery_fresh_job) {
    REQUIRE(JC_QueryIoRate);
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JC_IO_RATE_INFO *info = NULL; ULONG cnt = 0xFF;
    SetLastError(0); DWORD r = JC_QueryIoRate(j, NULL, &info, &cnt);
    DWORD e = GetLastError();
    t_note("QueryIoRate(fresh job, all volumes): ret %lu err %lu count %lu info %p", r, e, cnt, (void *)info);
    if (r && info) {
        for (ULONG i = 0; i < cnt && i < 4; i++) t_note("  entry %lu: maxiops %lld maxbw %lld flags 0x%lx", i, info[i].MaxIops, info[i].MaxBandwidth, info[i].ControlFlags);
        JC_FreeMemoryJobObject(info);
    }
    CloseHandle(j);
}
TEST(jobmisc_ioratequery_bad_volume_name) {
    REQUIRE(JC_QueryIoRate);
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JC_IO_RATE_INFO *info = NULL; ULONG cnt = 0xFF;
    SetLastError(0); DWORD r = JC_QueryIoRate(j, L"Z:\\definitely\\not\\a\\volume", &info, &cnt);
    t_note("QueryIoRate(bad volume): ret %lu err %lu count %lu", r, GetLastError(), cnt);
    if (r && info) JC_FreeMemoryJobObject(info);
    CloseHandle(j);
}
TEST(jobmisc_ioratequery_wrong_handle_type) {
    REQUIRE(JC_QueryIoRate);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    JC_IO_RATE_INFO *info = NULL; ULONG cnt = 0xFF;
    SetLastError(0); DWORD r = JC_QueryIoRate(ev, NULL, &info, &cnt);
    t_note("QueryIoRate(event): ret %lu err %lu", r, GetLastError());
    if (r && info) JC_FreeMemoryJobObject(info);
    CloseHandle(ev);
}
TEST(jobmisc_iorateset_invalid_job) {
    REQUIRE(JC_SetIoRate);
    JC_IO_RATE_INFO i; memset(&i, 0, sizeof i); i.MaxIops = 100; i.ControlFlags = JOB_OBJECT_IO_RATE_CONTROL_ENABLE;
    SetLastError(0); DWORD r = JC_SetIoRate(NULL, &i);
    CHECK(r == 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SetLastError(0); r = JC_SetIoRate(ev, &i);
    CHECK(r == 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
}
TEST(jobmisc_iorateset_invalid_parameters) {
    REQUIRE(JC_SetIoRate);
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JC_IO_RATE_INFO i; memset(&i, 0, sizeof i);
    SetLastError(0); DWORD r = JC_SetIoRate(j, &i);
    t_note("set io rate all zero: ret %lu err %lu", r, GetLastError());
    i.ControlFlags = 0x80000000; i.MaxIops = 10;
    SetLastError(0); r = JC_SetIoRate(j, &i);
    CHECK(r == 0); t_note("set io rate invalid flags: err %lu", GetLastError());
    i.ControlFlags = JOB_OBJECT_IO_RATE_CONTROL_ENABLE; i.MaxIops = -5; i.MaxBandwidth = -5;
    SetLastError(0); r = JC_SetIoRate(j, &i);
    t_note("set io rate negative: ret %lu err %lu", r, GetLastError());
    CloseHandle(j);
}
TEST(jobmisc_iorateset_volume_name_too_long) {
    REQUIRE(JC_SetIoRate);
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    enum { N = 0x8100 };
    WCHAR *v = malloc(N * sizeof(WCHAR)); for (int k = 0; k < N - 1; k++) v[k] = L'a'; v[N - 1] = 0;
    JC_IO_RATE_INFO i; memset(&i, 0, sizeof i); i.VolumeName = v; i.MaxIops = 10; i.ControlFlags = JOB_OBJECT_IO_RATE_CONTROL_ENABLE;
    SetLastError(0); DWORD r = JC_SetIoRate(j, &i);
    CHECK(r == 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);        /* >= 0x7fff characters is rejected before the kernel call */
    free(v); CloseHandle(j);
}
TEST(jobmisc_iorateset_unknown_volume) {
    REQUIRE(JC_SetIoRate);
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JC_IO_RATE_INFO i; memset(&i, 0, sizeof i); i.VolumeName = L"\\\\?\\Volume{00000000-0000-0000-0000-000000000000}\\";
    i.MaxIops = 10; i.ControlFlags = JOB_OBJECT_IO_RATE_CONTROL_ENABLE; i.BaseIoSize = 8192;
    SetLastError(0); DWORD r = JC_SetIoRate(j, &i);
    CHECK(r == 0); t_note("set io rate on unknown volume: err %lu", GetLastError());
    CloseHandle(j);
}

/* ---------- FreeMemoryJobObject ---------- */
TEST(jobmisc_freememory_null) {
    REQUIRE(JC_FreeMemoryJobObject);
    /* HeapFree(heap, 0, NULL) is a no-op */
    JC_FreeMemoryJobObject(NULL);
    CHECK(1);
}
TEST(jobmisc_freememory_process_heap_block) {
    REQUIRE(JC_FreeMemoryJobObject);
    void *p = HeapAlloc(GetProcessHeap(), 0, 64); CHECK(p);
    if (p) { JC_FreeMemoryJobObject(p); CHECK(1); }
}

/* ---------- Misc cross checks ---------- */
TEST(jobmisc_forwarders_kernel32_vs_kernelbase) {
    CHECK(UK(IsProcessInJob) != NULL);
    CHECK(UB(CreateJobObjectW) == NULL || UB(CreateJobObjectW) != UK(CreateJobObjectW));
    CHECK(UK(QueryInformationJobObject) != NULL);
}
TEST(jobmisc_job_handle_dup_and_close_order) {
    REQUIRE(U(CreateJobObjectW));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    HANDLE d1 = jc_dup(j, JOB_OBJECT_ALL_ACCESS), d2 = jc_dup(j, JOB_OBJECT_QUERY); CHECK(d1 && d2);
    CloseHandle(j);
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION a;
    if (d1) CHECK(jc_acct(d1, &a));
    if (d2) CHECK(jc_acct(d2, &a));
    if (d1) CloseHandle(d1);
    if (d2) CHECK(jc_acct(d2, &a));
    if (d2) CloseHandle(d2);
}
TEST(jobmisc_job_handle_in_wait_multiple) {
    REQUIRE(U(CreateJobObjectW));
    HANDLE j = jc_job(), k = jc_job(); CHECK(j && k); if (!j || !k) return;
    HANDLE h[2] = { j, k };
    EXPECT_EQ(WaitForMultipleObjects(2, h, FALSE, 50), WAIT_TIMEOUT);
    CloseHandle(j); CloseHandle(k);
}
TEST(jobmisc_job_not_signaled_by_member_exit) {
    REQUIRE(U(CreateJobObjectW));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j && jc_spawn_in_job(j, "jo_exit", "0", &pi)); if (!j) return;
    jc_wait_exit(pi.hProcess, 8000);
    CHECK(jc_wait_active(j, 0, 3000));
    DWORD w = WaitForSingleObject(j, 200);
    t_note("job wait after all members exited normally: %lu (0=signaled, 258=timeout)", w);
    jc_kill(&pi); CloseHandle(j);
}
TEST(jobmisc_reuse_job_after_members_exit) {
    REQUIRE(U(CreateJobObjectW));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    for (int i = 0; i < 3; i++) {
        PROCESS_INFORMATION pi; CHECK(jc_spawn_in_job(j, "jo_exit", "0", &pi));
        if (pi.hProcess) { jc_wait_exit(pi.hProcess, 8000); jc_kill(&pi); }
    }
    CHECK(jc_wait_active(j, 0, 3000));
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION a; CHECK(jc_acct(j, &a)); EXPECT_EQ(a.TotalProcesses, 3);
    CloseHandle(j);
}
TEST(jobmisc_assign_after_terminate_job) {
    REQUIRE(U(CreateJobObjectW));
    HANDLE j = jc_job(); PROCESS_INFORMATION p1, p2; CHECK(j); if (!j) return;
    CHECK(jc_spawn_in_job(j, "jo_sleep", "20000", &p1));
    CHECK(U(TerminateJobObject)(j, 1)); jc_wait_exit(p1.hProcess, 8000);
    HANDLE h2 = jc_spawn("jo_sleep", "20000", 0, &p2);
    if (h2) {
        SetLastError(0); BOOL r = U(AssignProcessToJobObject)(j, h2);
        t_note("assign to a job after TerminateJobObject: ret %d err %lu", r, r ? 0 : GetLastError());
        jc_kill(&p2);
    }
    jc_kill(&p1); CloseHandle(j);
}
TEST(jobmisc_stress_create_close_loop) {
    REQUIRE(U(CreateJobObjectW));
    int ok = 0;
    for (int i = 0; i < 2000; i++) { HANDLE j = jc_job(); if (j) { ok++; CloseHandle(j); } }
    EXPECT_EQ(ok, 2000);
}
TEST(jobmisc_stress_query_loop) {
    REQUIRE(U(QueryInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    int ok = 0; JOBOBJECT_BASIC_AND_IO_ACCOUNTING_INFORMATION a;
    for (int i = 0; i < 5000; i++) if (U(QueryInformationJobObject)(j, JobObjectBasicAndIoAccountingInformation, &a, sizeof a, NULL)) ok++;
    EXPECT_EQ(ok, 5000);
    CloseHandle(j);
}
TEST(jobmisc_stress_set_limits_loop) {
    REQUIRE(U(SetInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    int ok = 0;
    for (DWORD i = 1; i <= 2000; i++) {
        JOBOBJECT_BASIC_LIMIT_INFORMATION b; memset(&b, 0, sizeof b);
        b.LimitFlags = JOB_OBJECT_LIMIT_ACTIVE_PROCESS; b.ActiveProcessLimit = i;
        if (U(SetInformationJobObject)(j, JobObjectBasicLimitInformation, &b, sizeof b)) ok++;
    }
    EXPECT_EQ(ok, 2000);
    JOBOBJECT_BASIC_LIMIT_INFORMATION q; CHECK(U(QueryInformationJobObject)(j, JobObjectBasicLimitInformation, &q, sizeof q, NULL));
    EXPECT_EQ(q.ActiveProcessLimit, 2000);
    CloseHandle(j);
}
TEST(jobmisc_threads_query_concurrently) {
    REQUIRE(U(QueryInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    struct ctx { HANDLE j; volatile LONG bad; } c = { j, 0 };
    DWORD WINAPI worker(LPVOID p) {
        struct ctx *x = p; JOBOBJECT_BASIC_ACCOUNTING_INFORMATION a;
        for (int i = 0; i < 2000; i++) if (!jc_acct(x->j, &a)) InterlockedIncrement(&x->bad);
        return 0;
    }
    HANDLE th[8]; int n = 0;
    for (; n < 8; n++) { th[n] = CreateThread(NULL, 0, worker, &c, 0, NULL); if (!th[n]) break; }
    WaitForMultipleObjects(n, th, TRUE, 30000);
    for (int i = 0; i < n; i++) CloseHandle(th[i]);
    EXPECT_EQ(c.bad, 0);
    CloseHandle(j);
}
