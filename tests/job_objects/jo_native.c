#include "jc.h"

/* Export placement and native (ntdll) cross-checks for the job object API. */
UDECL(LONG, NtQueryInformationJobObject, (HANDLE, int, PVOID, ULONG, PULONG));
UDECL(LONG, NtTerminateJobObject, (HANDLE, LONG));
UDECL(LONG, NtIsProcessInJob, (HANDLE, HANDLE));
UDECL(LONG, NtQueryObject, (HANDLE, int, PVOID, ULONG, PULONG));
#define NTP(n) ((n##_pfn)t_proc_nt(#n))

TEST(jobnative_kernel32_exports_all_present) {
    static const char *const n[] = { "AssignProcessToJobObject", "CreateJobObjectA", "CreateJobObjectW", "CreateJobSet", "FreeMemoryJobObject",
        "IsProcessInJob", "OpenJobObjectA", "OpenJobObjectW", "QueryInformationJobObject", "QueryIoRateControlInformationJobObject",
        "SetInformationJobObject", "SetIoRateControlInformationJobObject", "TerminateJobObject" };
    for (unsigned i = 0; i < sizeof n / sizeof *n; i++) CHECKF(t_proc_k32(n[i]) != NULL, "kernel32 does not export %s", n[i]);
}
TEST(jobnative_isprocessinjob_in_kernelbase) {
    CHECK(t_proc_kb("IsProcessInJob") != NULL);
    CHECK(t_proc_k32("IsProcessInJob") == t_proc_kb("IsProcessInJob"));   /* kernel32 forwards to kernelbase */
}
TEST(jobnative_other_job_exports_in_kernelbase_observe) {
    static const char *const n[] = { "CreateJobObjectW", "OpenJobObjectW", "TerminateJobObject", "QueryInformationJobObject", "SetInformationJobObject", "AssignProcessToJobObject" };
    for (unsigned i = 0; i < sizeof n / sizeof *n; i++) t_note("kernelbase exports %s: %d", n[i], t_proc_kb(n[i]) != NULL);
}
TEST(jobnative_accounting_matches_ntdll) {
    REQUIRE(U(QueryInformationJobObject)); REQUIRE(NTP(NtQueryInformationJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    CHECK(jc_spawn_in_job(j, "jo_exit", "0", &pi));
    if (pi.hProcess) {
        jc_wait_exit(pi.hProcess, 8000); jc_wait_active(j, 0, 3000);
        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION a, b; ULONG rl = 0; memset(&b, 0, sizeof b);
        CHECK(jc_acct(j, &a));
        LONG st = NTP(NtQueryInformationJobObject)(j, JobObjectBasicAccountingInformation, &b, sizeof b, &rl);
        CHECKF(st >= 0, "NtQueryInformationJobObject 0x%lx", (unsigned long)st);
        EXPECT_EQ(rl, sizeof b);
        EXPECT_EQ(a.TotalProcesses, b.TotalProcesses); EXPECT_EQ(a.ActiveProcesses, b.ActiveProcesses);
        EXPECT_EQ(a.TotalUserTime.QuadPart, b.TotalUserTime.QuadPart);
        jc_kill(&pi);
    }
    CloseHandle(j);
}
TEST(jobnative_invalid_class_status_vs_win32_error) {
    REQUIRE(U(QueryInformationJobObject)); REQUIRE(NTP(NtQueryInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    char buf[64]; ULONG rl = 0;
    LONG st = NTP(NtQueryInformationJobObject)(j, 0x7FFF, buf, sizeof buf, &rl);
    CHECKF(st < 0, "native query of a bogus class succeeded (0x%lx)", (unsigned long)st);
    SetLastError(0); BOOL r = U(QueryInformationJobObject)(j, (JOBOBJECTINFOCLASS)0x7FFF, buf, sizeof buf, NULL);
    CHECK(!r);
    t_note("bogus class: native status 0x%lx, win32 error %lu", (unsigned long)st, GetLastError());
    CloseHandle(j);
}
TEST(jobnative_terminate_via_ntdll_applies_exit_code) {
    REQUIRE(NTP(NtTerminateJobObject));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    CHECK(jc_spawn_in_job(j, "jo_sleep", "20000", &pi));
    if (pi.hProcess) {
        LONG st = NTP(NtTerminateJobObject)(j, 0x6B);
        CHECKF(st >= 0, "NtTerminateJobObject 0x%lx", (unsigned long)st);
        EXPECT_EQ(jc_wait_exit(pi.hProcess, 8000), 0x6B);
        jc_kill(&pi);
    }
    CloseHandle(j);
}
TEST(jobnative_terminate_invalid_handle_status) {
    REQUIRE(NTP(NtTerminateJobObject));
    LONG st = NTP(NtTerminateJobObject)(NULL, 1);
    EXPECT_EQ((unsigned long)st, 0xC0000008UL);   /* STATUS_INVALID_HANDLE */
}
TEST(jobnative_isprocessinjob_status_codes) {
    REQUIRE(NTP(NtIsProcessInJob));
    HANDLE j = jc_job(); PROCESS_INFORMATION pi; CHECK(j); if (!j) return;
    HANDLE p = jc_spawn("jo_sleep", "5000", 0, &pi); CHECK(p);
    if (p) {
        LONG a = NTP(NtIsProcessInJob)(p, j);
        EXPECT_EQ((unsigned long)a, 0x123UL);   /* STATUS_PROCESS_NOT_IN_JOB */
        CHECK(U(AssignProcessToJobObject)(j, p));
        a = NTP(NtIsProcessInJob)(p, j);
        EXPECT_EQ((unsigned long)a, 0x124UL);   /* STATUS_PROCESS_IN_JOB */
        jc_kill(&pi);
    }
    CloseHandle(j);
}
TEST(jobnative_named_job_object_name) {
    REQUIRE(U(CreateJobObjectW)); REQUIRE(NTP(NtQueryObject));
    WCHAR nm[128]; jc_namew(nm, 128, "objname");
    HANDLE j = U(CreateJobObjectW)(NULL, nm); CHECK(j); if (!j) return;
    union { char raw[1024]; struct { USHORT Len, Max; PWSTR Buf; } us; } u; ULONG rl = 0;
    LONG st = NTP(NtQueryObject)(j, 1 /* ObjectNameInformation */, &u, sizeof u, &rl);
    CHECKF(st >= 0, "NtQueryObject name 0x%lx", (unsigned long)st);
    if (st >= 0) {
        size_t n = u.us.Len / sizeof(WCHAR), l = wcslen(nm);
        CHECK(n > l && memcmp(u.us.Buf + n - l, nm, l * sizeof(WCHAR)) == 0);   /* ...\BaseNamedObjects\<name> */
        CHECK(u.us.Buf[n - l - 1] == L'\\');
    }
    CloseHandle(j);
}
TEST(jobnative_unnamed_job_has_empty_name) {
    REQUIRE(NTP(NtQueryObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    union { char raw[1024]; struct { USHORT Len, Max; PWSTR Buf; } us; } u; ULONG rl = 0;
    LONG st = NTP(NtQueryObject)(j, 1, &u, sizeof u, &rl);
    CHECK(st >= 0); if (st >= 0) EXPECT_EQ(u.us.Len, 0);
    CloseHandle(j);
}
TEST(jobnative_object_type_name_is_job) {
    REQUIRE(NTP(NtQueryObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    union { char raw[1024]; struct { USHORT Len, Max; PWSTR Buf; } us; } u; ULONG rl = 0;
    LONG st = NTP(NtQueryObject)(j, 2 /* ObjectTypeInformation */, &u, sizeof u, &rl);
    CHECK(st >= 0);
    if (st >= 0) { CHECK(u.us.Len == 3 * sizeof(WCHAR) && memcmp(u.us.Buf, L"Job", 6) == 0); }
    CloseHandle(j);
}
TEST(jobnative_basic_limit_defaults_match_ntdll) {
    REQUIRE(NTP(NtQueryInformationJobObject));
    HANDLE j = jc_job(); CHECK(j); if (!j) return;
    JOBOBJECT_BASIC_LIMIT_INFORMATION b; ULONG rl = 0; memset(&b, 0x5A, sizeof b);
    LONG st = NTP(NtQueryInformationJobObject)(j, JobObjectBasicLimitInformation, &b, sizeof b, &rl);
    CHECK(st >= 0); EXPECT_EQ(rl, sizeof b);
    EXPECT_EQ(b.LimitFlags, 0); EXPECT_EQ(b.ActiveProcessLimit, 0); EXPECT_EQ(b.Affinity, 0);
    EXPECT_EQ(b.PerJobUserTimeLimit.QuadPart, 0); EXPECT_EQ(b.PerProcessUserTimeLimit.QuadPart, 0);
    CloseHandle(j);
}
