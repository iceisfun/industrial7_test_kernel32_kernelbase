#include "jc.h"
#include <aclapi.h>
UDECL(NTSTATUS, NtQueryObject, (HANDLE, int, PVOID, ULONG, PULONG));

#define RIGHT_ASSIGN 0x1
#define RIGHT_SETATTR 0x2
#define RIGHT_QUERY 0x4
#define RIGHT_TERMINATE 0x8

/* ---------- CreateJobObject ---------- */
TEST(jobcreate_unnamed_w) {
    REQUIRE(U(CreateJobObjectW));
    SetLastError(777);
    HANDLE h = U(CreateJobObjectW)(NULL, NULL);
    CHECK(h != NULL);
    EXPECT_ERR(0);               /* success path stores 0 explicitly */
    EXPECT_NE(h, INVALID_HANDLE_VALUE);
    if (h) CloseHandle(h);
}
TEST(jobcreate_unnamed_a) {
    REQUIRE(U(CreateJobObjectA));
    SetLastError(777);
    HANDLE h = U(CreateJobObjectA)(NULL, NULL);
    CHECK(h != NULL);
    EXPECT_ERR(0);
    if (h) CloseHandle(h);
}
TEST(jobcreate_exports_present) {
    CHECK(UK(CreateJobObjectA) != NULL);
    CHECK(UK(CreateJobObjectW) != NULL);
    CHECK(UK(OpenJobObjectA) != NULL);
    CHECK(UK(OpenJobObjectW) != NULL);
    CHECK(UK(AssignProcessToJobObject) != NULL);
    CHECK(UK(IsProcessInJob) != NULL);
    CHECK(UK(QueryInformationJobObject) != NULL);
    CHECK(UK(SetInformationJobObject) != NULL);
    CHECK(UK(TerminateJobObject) != NULL);
    CHECK(UK(CreateJobSet) != NULL);
    CHECK(JC_FreeMemoryJobObject != NULL);
    CHECK(JC_QueryIoRate != NULL);
    CHECK(JC_SetIoRate != NULL);
    CHECK(UB(IsProcessInJob) != NULL);   /* the only job export implemented in kernelbase */
}
TEST(jobcreate_distinct_handles) {
    REQUIRE(U(CreateJobObjectW));
    HANDLE a = U(CreateJobObjectW)(NULL, NULL), b = U(CreateJobObjectW)(NULL, NULL);
    CHECK(a && b);
    CHECK(a != b);
    /* independent objects: limits of one do not leak into the other */
    if (a && b) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION e; memset(&e, 0, sizeof e);
        e.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_ACTIVE_PROCESS; e.BasicLimitInformation.ActiveProcessLimit = 7;
        CHECK(jc_set_ext(a, &e));
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION q; memset(&q, 0xCC, sizeof q);
        CHECK(U(QueryInformationJobObject)(b, JobObjectExtendedLimitInformation, &q, sizeof q, NULL));
        EXPECT_EQ(q.BasicLimitInformation.LimitFlags, 0);
        EXPECT_EQ(q.BasicLimitInformation.ActiveProcessLimit, 0);
    }
    if (a) CloseHandle(a); if (b) CloseHandle(b);
}
TEST(jobcreate_two_unnamed_same_name_irrelevant) {
    REQUIRE(U(CreateJobObjectW));
    HANDLE h[20]; int n = 0;
    for (; n < 20; n++) { h[n] = U(CreateJobObjectW)(NULL, NULL); if (!h[n]) break; EXPECT_ERR(0); }
    EXPECT_EQ(n, 20);
    for (int i = 0; i < n; i++) CloseHandle(h[i]);
}
TEST(jobcreate_many) {
    REQUIRE(U(CreateJobObjectW));
    enum { N = 400 };
    HANDLE *h = calloc(N, sizeof *h); int ok = 0;
    for (int i = 0; i < N; i++) { h[i] = U(CreateJobObjectW)(NULL, NULL); if (h[i]) ok++; }
    EXPECT_EQ(ok, N);
    for (int i = 0; i < N; i++) if (h[i]) CloseHandle(h[i]);
    free(h);
}
TEST(jobcreate_handle_is_job_type) {
    REQUIRE(U(CreateJobObjectW)); REQUIRE(UN(NtQueryObject));
    HANDLE h = jc_job(); CHECK(h);
    if (!h) return;
    BYTE buf[512]; ULONG rl = 0; memset(buf, 0, sizeof buf);
    NTSTATUS st = UN(NtQueryObject)(h, 2 /*ObjectTypeInformation*/, buf, sizeof buf, &rl);
    CHECK(st >= 0);
    if (st >= 0) {
        UNICODE_STRING *us = (UNICODE_STRING *)buf;
        CHECK(us->Length == 6 && us->Buffer && !wcsncmp(us->Buffer, L"Job", 3));
    }
    CloseHandle(h);
}
TEST(jobcreate_fresh_job_unsignaled) {
    REQUIRE(U(CreateJobObjectW));
    HANDLE h = jc_job(); CHECK(h); if (!h) return;
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_TIMEOUT);
    EXPECT_EQ(WaitForSingleObject(h, 20), WAIT_TIMEOUT);
    CloseHandle(h);
}
TEST(jobcreate_full_access_rights) {
    REQUIRE(U(CreateJobObjectW));
    HANDLE h = jc_job(); CHECK(h); if (!h) return;
    JOBOBJECT_BASIC_UI_RESTRICTIONS ui = {0};
    CHECK(U(QueryInformationJobObject)(h, JobObjectBasicUIRestrictions, &ui, sizeof ui, NULL));
    CHECK(U(SetInformationJobObject)(h, JobObjectBasicUIRestrictions, &ui, sizeof ui));
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_TIMEOUT);   /* SYNCHRONIZE granted */
    CHECK(U(TerminateJobObject)(h, 5));
    CloseHandle(h);
}
TEST(jobcreate_inherit_flag_true) {
    REQUIRE(U(CreateJobObjectW));
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE h = U(CreateJobObjectW)(&sa, NULL); CHECK(h); if (!h) return;
    DWORD fl = 0; CHECK(GetHandleInformation(h, &fl));
    CHECK(fl & HANDLE_FLAG_INHERIT);
    CloseHandle(h);
}
TEST(jobcreate_inherit_flag_false) {
    REQUIRE(U(CreateJobObjectW));
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, FALSE };
    HANDLE h = U(CreateJobObjectW)(&sa, NULL); CHECK(h); if (!h) return;
    DWORD fl = 0xFF; CHECK(GetHandleInformation(h, &fl));
    CHECK(!(fl & HANDLE_FLAG_INHERIT));
    CloseHandle(h);
    h = U(CreateJobObjectW)(NULL, NULL); CHECK(h); if (!h) return;
    fl = 0xFF; CHECK(GetHandleInformation(h, &fl));
    CHECK(!(fl & HANDLE_FLAG_INHERIT));
    CloseHandle(h);
}
TEST(jobcreate_inherit_flag_a) {
    REQUIRE(U(CreateJobObjectA));
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE h = U(CreateJobObjectA)(&sa, NULL); CHECK(h); if (!h) return;
    DWORD fl = 0; CHECK(GetHandleInformation(h, &fl));
    CHECK(fl & HANDLE_FLAG_INHERIT);
    CloseHandle(h);
}
TEST(jobcreate_named_w_and_already_exists) {
    REQUIRE(U(CreateJobObjectW));
    WCHAR nm[128]; jc_namew(nm, 128, "nw");
    SetLastError(0);
    HANDLE h1 = U(CreateJobObjectW)(NULL, nm); CHECK(h1);
    EXPECT_ERR(0);
    SetLastError(0);
    HANDLE h2 = U(CreateJobObjectW)(NULL, nm); CHECK(h2);
    EXPECT_ERR(ERROR_ALREADY_EXISTS);
    CHECK(h1 != h2);
    if (h1 && h2) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION e; memset(&e, 0, sizeof e);
        e.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_ACTIVE_PROCESS; e.BasicLimitInformation.ActiveProcessLimit = 3;
        CHECK(jc_set_ext(h1, &e));
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION q; memset(&q, 0, sizeof q);
        CHECK(U(QueryInformationJobObject)(h2, JobObjectExtendedLimitInformation, &q, sizeof q, NULL));
        EXPECT_EQ(q.BasicLimitInformation.ActiveProcessLimit, 3);
        EXPECT_EQ(q.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_ACTIVE_PROCESS, JOB_OBJECT_LIMIT_ACTIVE_PROCESS);
    }
    if (h1) CloseHandle(h1); if (h2) CloseHandle(h2);
}
TEST(jobcreate_named_a_and_already_exists) {
    REQUIRE(U(CreateJobObjectA));
    char nm[128]; jc_name(nm, sizeof nm, "na");
    SetLastError(0);
    HANDLE h1 = U(CreateJobObjectA)(NULL, nm); CHECK(h1);
    EXPECT_ERR(0);
    SetLastError(0);
    HANDLE h2 = U(CreateJobObjectA)(NULL, nm); CHECK(h2);
    EXPECT_ERR(ERROR_ALREADY_EXISTS);
    if (h1) CloseHandle(h1); if (h2) CloseHandle(h2);
}
TEST(jobcreate_named_cross_aw) {
    REQUIRE(U(CreateJobObjectA)); REQUIRE(U(CreateJobObjectW));
    char nm[128]; WCHAR wn[128]; jc_name(nm, sizeof nm, "xaw");
    for (int i = 0; ; i++) { wn[i] = nm[i]; if (!nm[i]) break; }
    HANDLE a = U(CreateJobObjectA)(NULL, nm); CHECK(a);
    SetLastError(0);
    HANDLE w = U(CreateJobObjectW)(NULL, wn); CHECK(w);
    EXPECT_ERR(ERROR_ALREADY_EXISTS);
    if (a) CloseHandle(a); if (w) CloseHandle(w);
}
TEST(jobcreate_named_case_insensitive) {
    REQUIRE(U(CreateJobObjectA));
    char nm[128], up[128]; jc_name(nm, sizeof nm, "case");
    strcpy(up, nm); for (char *p = up; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
    HANDLE a = U(CreateJobObjectA)(NULL, nm); CHECK(a);
    SetLastError(0);
    HANDLE b = U(CreateJobObjectA)(NULL, up); CHECK(b);
    EXPECT_ERR(0);   /* job names are case-sensitive: a different-case name is a distinct, new object (no OBJ_CASE_INSENSITIVE) */
    if (a) CloseHandle(a); if (b) CloseHandle(b);
}
TEST(jobcreate_named_third_open_keeps_exists) {
    REQUIRE(U(CreateJobObjectA));
    char nm[128]; jc_name(nm, sizeof nm, "third");
    HANDLE h[3];
    for (int i = 0; i < 3; i++) { SetLastError(0); h[i] = U(CreateJobObjectA)(NULL, nm); CHECK(h[i]); if (i) EXPECT_ERR(ERROR_ALREADY_EXISTS); else EXPECT_ERR(0); }
    for (int i = 0; i < 3; i++) if (h[i]) CloseHandle(h[i]);
}
TEST(jobcreate_name_reusable_after_close) {
    REQUIRE(U(CreateJobObjectA));
    char nm[128]; jc_name(nm, sizeof nm, "reuse");
    HANDLE a = U(CreateJobObjectA)(NULL, nm); CHECK(a);
    if (a) CloseHandle(a);
    SetLastError(777);
    HANDLE b = U(CreateJobObjectA)(NULL, nm); CHECK(b);
    EXPECT_ERR(0);    /* fresh object */
    if (b) CloseHandle(b);
}
TEST(jobcreate_name_survives_while_other_handle_open) {
    REQUIRE(U(CreateJobObjectA));
    char nm[128]; jc_name(nm, sizeof nm, "surv");
    HANDLE a = U(CreateJobObjectA)(NULL, nm); HANDLE b = U(CreateJobObjectA)(NULL, nm);
    CHECK(a && b);
    if (a) CloseHandle(a);
    HANDLE c = U(OpenJobObjectA)(JOB_OBJECT_QUERY, FALSE, nm); CHECK(c);
    if (b) CloseHandle(b); if (c) CloseHandle(c);
}
TEST(jobcreate_name_type_mismatch_event) {
    REQUIRE(U(CreateJobObjectA));
    char nm[128]; jc_name(nm, sizeof nm, "evt");
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, nm); CHECK(ev); if (!ev) return;
    SetLastError(0);
    HANDLE j = U(CreateJobObjectA)(NULL, nm);
    CHECK(j == NULL);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    if (j) CloseHandle(j);
    CloseHandle(ev);
}
TEST(jobcreate_name_type_mismatch_mutex) {
    REQUIRE(U(CreateJobObjectW));
    char nm[128]; jc_name(nm, sizeof nm, "mtx");
    WCHAR wn[128]; for (int i = 0; ; i++) { wn[i] = nm[i]; if (!nm[i]) break; }
    HANDLE m = CreateMutexA(NULL, FALSE, nm); CHECK(m); if (!m) return;
    SetLastError(0);
    HANDLE j = U(CreateJobObjectW)(NULL, wn);
    CHECK(j == NULL);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    if (j) CloseHandle(j);
    CloseHandle(m);
}
TEST(jobcreate_name_with_backslash_missing_dir) {
    REQUIRE(U(CreateJobObjectA));
    char nm[160]; char base[128]; jc_name(base, sizeof base, "bs");
    snprintf(nm, sizeof nm, "%s\\sub", base);
    SetLastError(0);
    HANDLE j = U(CreateJobObjectA)(NULL, nm);
    CHECK(j == NULL);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_PATH_NOT_FOUND || e == ERROR_INVALID_NAME || e == ERROR_BAD_PATHNAME || e == ERROR_FILE_NOT_FOUND, "err %lu", e);
    if (j) CloseHandle(j);
}
TEST(jobcreate_name_local_prefix_same_object) {
    REQUIRE(U(CreateJobObjectA));
    char nm[128], loc[160]; jc_name(nm, sizeof nm, "loc");
    snprintf(loc, sizeof loc, "Local\\%s", nm);
    HANDLE a = U(CreateJobObjectA)(NULL, nm); CHECK(a);
    SetLastError(0);
    HANDLE b = U(CreateJobObjectA)(NULL, loc); CHECK(b);
    EXPECT_ERR(ERROR_ALREADY_EXISTS);
    if (a) CloseHandle(a); if (b) CloseHandle(b);
}
TEST(jobcreate_name_local_prefix_open) {
    REQUIRE(U(CreateJobObjectA)); REQUIRE(U(OpenJobObjectA));
    char nm[128], loc[160]; jc_name(nm, sizeof nm, "loc2");
    snprintf(loc, sizeof loc, "Local\\%s", nm);
    HANDLE a = U(CreateJobObjectA)(NULL, loc); CHECK(a);
    HANDLE b = U(OpenJobObjectA)(JOB_OBJECT_QUERY, FALSE, nm); CHECK(b);
    if (a) CloseHandle(a); if (b) CloseHandle(b);
}
TEST(jobcreate_name_empty_string) {
    REQUIRE(U(CreateJobObjectW));
    SetLastError(0);
    HANDLE h = U(CreateJobObjectW)(NULL, L"");
    if (h) { t_note("empty name accepted, lasterr %lu", GetLastError()); CloseHandle(h); }
    else t_note("empty name rejected, lasterr %lu", GetLastError());
}
TEST(jobcreate_name_long_240) {
    REQUIRE(U(CreateJobObjectW));
    WCHAR nm[300]; for (int i = 0; i < 240; i++) nm[i] = L'a' + (i % 26); nm[240] = 0;
    swprintf(nm, 300, L"k32t_jo_long_%lu_", GetCurrentProcessId()); size_t l = wcslen(nm); for (; l < 240; l++) nm[l] = L'q'; nm[240] = 0;
    HANDLE h = U(CreateJobObjectW)(NULL, nm); CHECK(h);
    SetLastError(0);
    HANDLE h2 = U(CreateJobObjectW)(NULL, nm); CHECK(h2);
    EXPECT_ERR(ERROR_ALREADY_EXISTS);
    if (h) CloseHandle(h); if (h2) CloseHandle(h2);
}
TEST(jobcreate_name_unicode) {
    REQUIRE(U(CreateJobObjectW)); REQUIRE(U(OpenJobObjectW));
    WCHAR nm[128]; swprintf(nm, 128, L"k32t_jo_u%lu_\x00e9\x4e2d\x6587", GetCurrentProcessId());
    HANDLE h = U(CreateJobObjectW)(NULL, nm); CHECK(h);
    HANDLE o = U(OpenJobObjectW)(JOB_OBJECT_QUERY, FALSE, nm); CHECK(o);
    if (h) CloseHandle(h); if (o) CloseHandle(o);
}
TEST(jobcreate_name_ansi_roundtrip_highchars) {
    REQUIRE(U(CreateJobObjectA)); REQUIRE(U(OpenJobObjectW));
    char nm[128]; snprintf(nm, sizeof nm, "k32t_jo_hi%lu_abc", GetCurrentProcessId());
    WCHAR w[128]; for (int i = 0;; i++) { w[i] = nm[i]; if (!nm[i]) break; }
    HANDLE h = U(CreateJobObjectA)(NULL, nm); CHECK(h);
    HANDLE o = U(OpenJobObjectW)(JOB_OBJECT_QUERY, FALSE, w); CHECK(o);
    if (h) CloseHandle(h); if (o) CloseHandle(o);
}
TEST(jobcreate_sa_empty_dacl_denies_open) {
    REQUIRE(U(CreateJobObjectA)); REQUIRE(U(OpenJobObjectA));
    SECURITY_DESCRIPTOR sd; ACL acl; char nm[128]; jc_name(nm, sizeof nm, "sd");
    CHECK(InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION));
    CHECK(InitializeAcl(&acl, sizeof acl, ACL_REVISION));
    CHECK(SetSecurityDescriptorDacl(&sd, TRUE, &acl, FALSE));
    SECURITY_ATTRIBUTES sa = { sizeof sa, &sd, FALSE };
    HANDLE h = U(CreateJobObjectA)(&sa, nm); CHECK(h);
    SetLastError(0);
    HANDLE o = U(OpenJobObjectA)(JOB_OBJECT_QUERY, FALSE, nm);
    CHECK(o == NULL);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    if (o) CloseHandle(o);
    /* creator handle still has full access */
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION a;
    CHECK(jc_acct(h, &a));
    if (h) CloseHandle(h);
}
TEST(jobcreate_sa_null_dacl_allows_open) {
    REQUIRE(U(CreateJobObjectA)); REQUIRE(U(OpenJobObjectA));
    SECURITY_DESCRIPTOR sd; char nm[128]; jc_name(nm, sizeof nm, "sdn");
    CHECK(InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION));
    CHECK(SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE));
    SECURITY_ATTRIBUTES sa = { sizeof sa, &sd, FALSE };
    HANDLE h = U(CreateJobObjectA)(&sa, nm); CHECK(h);
    HANDLE o = U(OpenJobObjectA)(JOB_OBJECT_ALL_ACCESS, FALSE, nm); CHECK(o);
    if (o) CloseHandle(o); if (h) CloseHandle(h);
}
TEST(jobcreate_sa_read_only_dacl) {
    REQUIRE(U(CreateJobObjectA)); REQUIRE(U(OpenJobObjectA));
    /* DACL granting Everyone only QUERY: open with QUERY ok, with TERMINATE denied */
    char nm[128]; jc_name(nm, sizeof nm, "sdq");
    BYTE sidb[SECURITY_MAX_SID_SIZE]; DWORD sl = sizeof sidb;
    CHECK(CreateWellKnownSid(WinWorldSid, NULL, sidb, &sl));
    BYTE aclb[256]; ACL *acl = (ACL *)aclb;
    CHECK(InitializeAcl(acl, sizeof aclb, ACL_REVISION));
    CHECK(AddAccessAllowedAce(acl, ACL_REVISION, JOB_OBJECT_QUERY, sidb));
    SECURITY_DESCRIPTOR sd;
    CHECK(InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION));
    CHECK(SetSecurityDescriptorDacl(&sd, TRUE, acl, FALSE));
    SECURITY_ATTRIBUTES sa = { sizeof sa, &sd, FALSE };
    HANDLE h = U(CreateJobObjectA)(&sa, nm); CHECK(h);
    HANDLE q = U(OpenJobObjectA)(JOB_OBJECT_QUERY, FALSE, nm); CHECK(q);
    SetLastError(0);
    HANDLE t = U(OpenJobObjectA)(JOB_OBJECT_TERMINATE, FALSE, nm);
    CHECK(t == NULL); EXPECT_ERR(ERROR_ACCESS_DENIED);
    if (t) CloseHandle(t); if (q) CloseHandle(q); if (h) CloseHandle(h);
}

/* ---------- OpenJobObject ---------- */
TEST(jobopen_w_basic) {
    REQUIRE(U(CreateJobObjectW)); REQUIRE(U(OpenJobObjectW));
    WCHAR nm[128]; jc_namew(nm, 128, "ow");
    HANDLE c = U(CreateJobObjectW)(NULL, nm); CHECK(c);
    HANDLE o = U(OpenJobObjectW)(JOB_OBJECT_ALL_ACCESS, FALSE, nm); CHECK(o);
    CHECK(o != c);
    if (o) { JOBOBJECT_BASIC_ACCOUNTING_INFORMATION a; CHECK(jc_acct(o, &a)); }
    if (o) CloseHandle(o); if (c) CloseHandle(c);
}
TEST(jobopen_a_basic) {
    REQUIRE(U(CreateJobObjectA)); REQUIRE(U(OpenJobObjectA));
    char nm[128]; jc_name(nm, sizeof nm, "oa");
    HANDLE c = U(CreateJobObjectA)(NULL, nm); CHECK(c);
    HANDLE o = U(OpenJobObjectA)(JOB_OBJECT_ALL_ACCESS, FALSE, nm); CHECK(o);
    if (o) CloseHandle(o); if (c) CloseHandle(c);
}
TEST(jobopen_nonexistent) {
    REQUIRE(U(OpenJobObjectA)); REQUIRE(U(OpenJobObjectW));
    char nm[128]; jc_name(nm, sizeof nm, "none");
    SetLastError(0);
    HANDLE o = U(OpenJobObjectA)(JOB_OBJECT_ALL_ACCESS, FALSE, nm);
    CHECK(o == NULL); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    WCHAR w[128]; jc_namew(w, 128, "none");
    SetLastError(0);
    o = U(OpenJobObjectW)(JOB_OBJECT_ALL_ACCESS, FALSE, w);
    CHECK(o == NULL); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(jobopen_null_name) {
    REQUIRE(U(OpenJobObjectA)); REQUIRE(U(OpenJobObjectW));
    SetLastError(0);
    HANDLE o = U(OpenJobObjectW)(JOB_OBJECT_ALL_ACCESS, FALSE, NULL);
    CHECK(o == NULL);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_INVALID_PARAMETER, "err %lu", e);
    SetLastError(0);
    o = U(OpenJobObjectA)(JOB_OBJECT_ALL_ACCESS, FALSE, NULL);
    CHECK(o == NULL);
    e = GetLastError();
    CHECKF(e == ERROR_INVALID_PARAMETER, "err %lu", e);
}
TEST(jobopen_empty_name) {
    REQUIRE(U(OpenJobObjectW));
    SetLastError(0);
    HANDLE o = U(OpenJobObjectW)(JOB_OBJECT_QUERY, FALSE, L"");
    if (o) { t_note("empty name open succeeded"); CloseHandle(o); }
    else t_note("empty name open failed %lu", GetLastError());
}
TEST(jobopen_type_mismatch_event) {
    REQUIRE(U(OpenJobObjectA));
    char nm[128]; jc_name(nm, sizeof nm, "oev");
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, nm); CHECK(ev); if (!ev) return;
    SetLastError(0);
    HANDLE o = U(OpenJobObjectA)(JOB_OBJECT_QUERY, FALSE, nm);
    CHECK(o == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE);
    if (o) CloseHandle(o);
    CloseHandle(ev);
}
TEST(jobopen_inherit_true) {
    REQUIRE(U(CreateJobObjectA)); REQUIRE(U(OpenJobObjectA));
    char nm[128]; jc_name(nm, sizeof nm, "oi1");
    HANDLE c = U(CreateJobObjectA)(NULL, nm); CHECK(c);
    HANDLE o = U(OpenJobObjectA)(JOB_OBJECT_QUERY, TRUE, nm); CHECK(o);
    DWORD fl = 0; if (o) { CHECK(GetHandleInformation(o, &fl)); CHECK(fl & HANDLE_FLAG_INHERIT); CloseHandle(o); }
    if (c) CloseHandle(c);
}
TEST(jobopen_inherit_false) {
    REQUIRE(U(CreateJobObjectA)); REQUIRE(U(OpenJobObjectA));
    char nm[128]; jc_name(nm, sizeof nm, "oi0");
    HANDLE c = U(CreateJobObjectA)(NULL, nm); CHECK(c);
    HANDLE o = U(OpenJobObjectA)(JOB_OBJECT_QUERY, FALSE, nm); CHECK(o);
    DWORD fl = 0xFF; if (o) { CHECK(GetHandleInformation(o, &fl)); CHECK(!(fl & HANDLE_FLAG_INHERIT)); CloseHandle(o); }
    if (c) CloseHandle(c);
}
TEST(jobopen_inherit_nonzero_bool) {
    REQUIRE(U(CreateJobObjectA)); REQUIRE(U(OpenJobObjectA));
    char nm[128]; jc_name(nm, sizeof nm, "oi2");
    HANDLE c = U(CreateJobObjectA)(NULL, nm); CHECK(c);
    HANDLE o = U(OpenJobObjectA)(JOB_OBJECT_QUERY, 0x100, nm); CHECK(o);   /* any nonzero BOOL */
    DWORD fl = 0; if (o) { GetHandleInformation(o, &fl); CHECK(fl & HANDLE_FLAG_INHERIT); CloseHandle(o); }
    if (c) CloseHandle(c);
}
TEST(jobopen_case_insensitive) {
    REQUIRE(U(CreateJobObjectA)); REQUIRE(U(OpenJobObjectA));
    char nm[128], up[128]; jc_name(nm, sizeof nm, "ocase");
    strcpy(up, nm); for (char *p = up; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
    HANDLE c = U(CreateJobObjectA)(NULL, nm); CHECK(c);
    SetLastError(0);
    HANDLE o = U(OpenJobObjectA)(JOB_OBJECT_QUERY, FALSE, up); CHECK(!o);   /* open is case-sensitive (no OBJ_CASE_INSENSITIVE) */
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    if (o) CloseHandle(o); if (c) CloseHandle(c);
}
TEST(jobopen_after_all_handles_closed) {
    REQUIRE(U(CreateJobObjectA)); REQUIRE(U(OpenJobObjectA));
    char nm[128]; jc_name(nm, sizeof nm, "ogone");
    HANDLE c = U(CreateJobObjectA)(NULL, nm); CHECK(c);
    if (c) CloseHandle(c);
    SetLastError(0);
    HANDLE o = U(OpenJobObjectA)(JOB_OBJECT_QUERY, FALSE, nm);
    CHECK(o == NULL); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    if (o) CloseHandle(o);
}
TEST(jobopen_max_allowed) {
    REQUIRE(U(CreateJobObjectA)); REQUIRE(U(OpenJobObjectA));
    char nm[128]; jc_name(nm, sizeof nm, "omax");
    HANDLE c = U(CreateJobObjectA)(NULL, nm); CHECK(c);
    HANDLE o = U(OpenJobObjectA)(MAXIMUM_ALLOWED, FALSE, nm); CHECK(o);
    if (o) { JOBOBJECT_BASIC_UI_RESTRICTIONS ui = {0}; CHECK(U(SetInformationJobObject)(o, JobObjectBasicUIRestrictions, &ui, sizeof ui)); CloseHandle(o); }
    if (c) CloseHandle(c);
}
TEST(jobopen_generic_all) {
    REQUIRE(U(CreateJobObjectA)); REQUIRE(U(OpenJobObjectA));
    char nm[128]; jc_name(nm, sizeof nm, "ogall");
    HANDLE c = U(CreateJobObjectA)(NULL, nm); CHECK(c);
    HANDLE o = U(OpenJobObjectA)(GENERIC_ALL, FALSE, nm); CHECK(o);
    if (o) CloseHandle(o); if (c) CloseHandle(c);
}
TEST(jobopen_zero_access) {
    REQUIRE(U(CreateJobObjectA)); REQUIRE(U(OpenJobObjectA));
    char nm[128]; jc_name(nm, sizeof nm, "o0");
    HANDLE c = U(CreateJobObjectA)(NULL, nm); CHECK(c);
    HANDLE o = U(OpenJobObjectA)(0, FALSE, nm);
    t_note("open with access 0: %s (err %lu)", o ? "ok" : "fail", o ? 0 : GetLastError());
    if (o) {
        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION a; SetLastError(0);
        CHECK(!jc_acct(o, &a)); EXPECT_ERR(ERROR_ACCESS_DENIED);
        CloseHandle(o);
    }
    if (c) CloseHandle(c);
}
TEST(jobopen_access_system_security) {
    REQUIRE(U(CreateJobObjectA)); REQUIRE(U(OpenJobObjectA));
    char nm[128]; jc_name(nm, sizeof nm, "oacl");
    HANDLE c = U(CreateJobObjectA)(NULL, nm); CHECK(c);
    SetLastError(0);
    HANDLE o = U(OpenJobObjectA)(ACCESS_SYSTEM_SECURITY, FALSE, nm);
    if (o) { t_note("ACCESS_SYSTEM_SECURITY granted (privileged run)"); CloseHandle(o); }
    else { t_note("ACCESS_SYSTEM_SECURITY open err %lu", GetLastError()); CHECK(GetLastError() == ERROR_PRIVILEGE_NOT_HELD || GetLastError() == ERROR_ACCESS_DENIED); }
    if (c) CloseHandle(c);
}
TEST(jobopen_each_right_alone) {
    REQUIRE(U(CreateJobObjectA)); REQUIRE(U(OpenJobObjectA));
    static const DWORD r[] = { JOB_OBJECT_ASSIGN_PROCESS, JOB_OBJECT_SET_ATTRIBUTES, JOB_OBJECT_QUERY, JOB_OBJECT_TERMINATE,
        JOB_OBJECT_SET_SECURITY_ATTRIBUTES, JOB_OBJECT_IMPERSONATE, SYNCHRONIZE, READ_CONTROL, DELETE };
    char nm[128]; jc_name(nm, sizeof nm, "oeach");
    HANDLE c = U(CreateJobObjectA)(NULL, nm); CHECK(c);
    for (unsigned i = 0; i < sizeof r / sizeof *r; i++) {
        HANDLE o = U(OpenJobObjectA)(r[i], FALSE, nm);
        CHECKF(o != NULL, "right 0x%lx err %lu", r[i], GetLastError());
        if (o) CloseHandle(o);
    }
    if (c) CloseHandle(c);
}
TEST(jobopen_synchronize_wait) {
    REQUIRE(U(CreateJobObjectA)); REQUIRE(U(OpenJobObjectA));
    char nm[128]; jc_name(nm, sizeof nm, "osync");
    HANDLE c = U(CreateJobObjectA)(NULL, nm); CHECK(c);
    HANDLE o = U(OpenJobObjectA)(SYNCHRONIZE, FALSE, nm); CHECK(o);
    if (o) { EXPECT_EQ(WaitForSingleObject(o, 0), WAIT_TIMEOUT); CloseHandle(o); }
    HANDLE n = U(OpenJobObjectA)(JOB_OBJECT_QUERY, FALSE, nm); CHECK(n);
    if (n) { SetLastError(0); EXPECT_EQ(WaitForSingleObject(n, 0), WAIT_FAILED); EXPECT_ERR(ERROR_ACCESS_DENIED); CloseHandle(n); }
    if (c) CloseHandle(c);
}
TEST(jobopen_shares_state_with_creator) {
    REQUIRE(U(CreateJobObjectA)); REQUIRE(U(OpenJobObjectA));
    char nm[128]; jc_name(nm, sizeof nm, "oshare");
    HANDLE c = U(CreateJobObjectA)(NULL, nm); CHECK(c);
    HANDLE o = U(OpenJobObjectA)(JOB_OBJECT_QUERY | JOB_OBJECT_SET_ATTRIBUTES, FALSE, nm); CHECK(o);
    if (c && o) {
        JOBOBJECT_BASIC_UI_RESTRICTIONS u = { JOB_OBJECT_UILIMIT_DESKTOP | JOB_OBJECT_UILIMIT_GLOBALATOMS };
        CHECK(U(SetInformationJobObject)(o, JobObjectBasicUIRestrictions, &u, sizeof u));
        JOBOBJECT_BASIC_UI_RESTRICTIONS q = {0};
        CHECK(U(QueryInformationJobObject)(c, JobObjectBasicUIRestrictions, &q, sizeof q, NULL));
        EXPECT_EQ(q.UIRestrictionsClass, u.UIRestrictionsClass);
    }
    if (o) CloseHandle(o); if (c) CloseHandle(c);
}
TEST(jobopen_w_name_with_backslash_missing) {
    REQUIRE(U(OpenJobObjectW));
    WCHAR nm[128]; swprintf(nm, 128, L"k32t_jo_nodir%lu\\x", GetCurrentProcessId());
    SetLastError(0);
    HANDLE o = U(OpenJobObjectW)(JOB_OBJECT_QUERY, FALSE, nm);
    CHECK(o == NULL);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_PATH_NOT_FOUND || e == ERROR_FILE_NOT_FOUND || e == ERROR_INVALID_NAME || e == ERROR_BAD_PATHNAME, "err %lu", e);
    if (o) CloseHandle(o);
}
TEST(jobopen_global_prefix_visible_name) {
    REQUIRE(U(CreateJobObjectA)); REQUIRE(U(OpenJobObjectA));
    char nm[128], g[160]; jc_name(nm, sizeof nm, "glob");
    snprintf(g, sizeof g, "Global\\%s", nm);
    HANDLE c = U(CreateJobObjectA)(NULL, g);
    if (!c) { t_note("Global\\ create failed %lu", GetLastError()); return; }
    HANDLE o = U(OpenJobObjectA)(JOB_OBJECT_QUERY, FALSE, g); CHECK(o);
    if (o) CloseHandle(o);
    CloseHandle(c);
}

/* ---------- rights matrix (duplicated handles) ---------- */
static int op_query(HANDLE h) { JOBOBJECT_BASIC_ACCOUNTING_INFORMATION a; return U(QueryInformationJobObject)(h, JobObjectBasicAccountingInformation, &a, sizeof a, NULL); }
static int op_set(HANDLE h) { JOBOBJECT_BASIC_UI_RESTRICTIONS u = {0}; return U(SetInformationJobObject)(h, JobObjectBasicUIRestrictions, &u, sizeof u); }
static int op_term(HANDLE h) { return U(TerminateJobObject)(h, 3); }
static int op_assign(HANDLE h) {
    PROCESS_INFORMATION pi; HANDLE p = jc_spawn("jo_sleep", "3000", 0, &pi);
    if (!p) return -1;
    int r = U(AssignProcessToJobObject)(h, p);
    DWORD e = GetLastError();
    jc_kill(&pi); SetLastError(e);
    return r;
}
static int rights_run(DWORD mask, DWORD need, int (*op)(HANDLE), const char *nm) {
    HANDLE j = jc_job(), d = NULL;
    int bad = 0;
    if (!j) return 1;
    d = jc_dup(j, mask);
    if (!d) { CloseHandle(j); return 1; }
    SetLastError(0);
    int r = op(d);
    DWORD e = GetLastError();
    int should = (mask & need) == need || mask == JOB_OBJECT_ALL_ACCESS;
    if (r == -1) bad = 1;
    else if (should && !r) { t_check(0, __FILE__, __LINE__, nm, "mask 0x%lx should succeed err %lu", mask, e); bad = 1; }
    else if (!should && (r || e != ERROR_ACCESS_DENIED)) { t_check(0, __FILE__, __LINE__, nm, "mask 0x%lx should fail 5, r=%d err %lu", mask, r, e); bad = 1; }
    else t_check(1, __FILE__, __LINE__, nm, NULL);
    CloseHandle(d); CloseHandle(j);
    return bad;
}
static const DWORD masks[] = { 0, SYNCHRONIZE, RIGHT_ASSIGN, RIGHT_SETATTR, RIGHT_QUERY, RIGHT_TERMINATE, 0x10, 0x20,
    RIGHT_QUERY | RIGHT_SETATTR, RIGHT_ASSIGN | RIGHT_TERMINATE, RIGHT_QUERY | RIGHT_TERMINATE | RIGHT_ASSIGN, JOB_OBJECT_ALL_ACCESS };
TEST(jobrights_query_matrix) {
    REQUIRE(U(QueryInformationJobObject)); REQUIRE(U(CreateJobObjectW));
    for (unsigned i = 0; i < sizeof masks / sizeof *masks; i++) rights_run(masks[i], RIGHT_QUERY, op_query, "query-right");
}
TEST(jobrights_set_matrix) {
    REQUIRE(U(SetInformationJobObject)); REQUIRE(U(CreateJobObjectW));
    for (unsigned i = 0; i < sizeof masks / sizeof *masks; i++) rights_run(masks[i], RIGHT_SETATTR, op_set, "set-right");
}
TEST(jobrights_terminate_matrix) {
    REQUIRE(U(TerminateJobObject)); REQUIRE(U(CreateJobObjectW));
    for (unsigned i = 0; i < sizeof masks / sizeof *masks; i++) rights_run(masks[i], RIGHT_TERMINATE, op_term, "terminate-right");
}
TEST(jobrights_assign_matrix) {
    REQUIRE(U(AssignProcessToJobObject)); REQUIRE(U(CreateJobObjectW));
    for (unsigned i = 0; i < sizeof masks / sizeof *masks; i++) rights_run(masks[i], RIGHT_ASSIGN, op_assign, "assign-right");
}
TEST(jobrights_named_open_enforced) {
    REQUIRE(U(CreateJobObjectA)); REQUIRE(U(OpenJobObjectA));
    char nm[128]; jc_name(nm, sizeof nm, "rn");
    HANDLE c = U(CreateJobObjectA)(NULL, nm); CHECK(c);
    HANDLE q = U(OpenJobObjectA)(JOB_OBJECT_QUERY, FALSE, nm); CHECK(q);
    if (q) {
        CHECK(op_query(q));
        SetLastError(0); CHECK(!op_set(q)); EXPECT_ERR(ERROR_ACCESS_DENIED);
        SetLastError(0); CHECK(!op_term(q)); EXPECT_ERR(ERROR_ACCESS_DENIED);
        CloseHandle(q);
    }
    HANDLE t = U(OpenJobObjectA)(JOB_OBJECT_TERMINATE, FALSE, nm); CHECK(t);
    if (t) {
        CHECK(op_term(t));
        SetLastError(0); CHECK(!op_query(t)); EXPECT_ERR(ERROR_ACCESS_DENIED);
        CloseHandle(t);
    }
    if (c) CloseHandle(c);
}
