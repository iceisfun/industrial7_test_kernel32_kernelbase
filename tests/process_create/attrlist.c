/* InitializeProcThreadAttributeList / UpdateProcThreadAttribute / DeleteProcThreadAttributeList and their use with CreateProcess. */
#include "pc.h"

#define A_PARENT   0x00020000
#define A_HANDLES  0x00020002
#define A_GROUPAFF 0x00030003
#define A_NODE     0x00020004
#define A_IDEALCPU 0x00030005
#define A_MITIG    0x00020007
#define A_SECCAP   0x00020009
#define A_PROTLVL  0x0002000B
#define A_JOBLIST  0x0002000D
#define A_CHILDPOL 0x0002000E
#define A_ALLAPP   0x0002000F
#define A_WIN32K   0x00020010
#define A_DESKTOP  0x00020012
#define A_PCON     0x00020016
#define A_ADDITIVE 0x00060001

typedef LPPROC_THREAD_ATTRIBUTE_LIST AL;
static AL mk(DWORD n) {
    SIZE_T sz = 0; AL l;
    U(InitializeProcThreadAttributeList)(NULL, n, 0, &sz);
    l = (AL)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sz + 64);
    if (!U(InitializeProcThreadAttributeList)(l, n, 0, &sz)) { HeapFree(GetProcessHeap(), 0, l); return NULL; }
    return l;
}
static void rel(AL l) { if (l) { U(DeleteProcThreadAttributeList)(l); HeapFree(GetProcessHeap(), 0, l); } }
static BOOL upd(AL l, DWORD_PTR a, PVOID v, SIZE_T cb) { return U(UpdateProcThreadAttribute)(l, 0, a, v, cb, NULL, NULL); }
static void exopts(pc_opts *o, STARTUPINFOEXW *sx, AL l, const char *child, const char *args, DWORD flags) {
    memset(sx, 0, sizeof *sx); sx->StartupInfo.cb = sizeof *sx; sx->lpAttributeList = l;
    memset(o, 0, sizeof *o); o->child = child; o->args = args; o->si = sx; o->flags = flags | EXTENDED_STARTUPINFO_PRESENT;
}
typedef struct { LONG_PTR st; BYTE *peb; ULONG_PTR aff; LONG prio; ULONG_PTR pid; ULONG_PTR ppid; } PBI;
static ULONG_PTR ppid_of(HANDLE h) {
    NtQueryInformationProcess_pfn q = UN(NtQueryInformationProcess); PBI p; memset(&p, 0, sizeof p);
    if (!q || q(h, 0, &p, sizeof p, NULL) < 0) return (ULONG_PTR)-1;
    return p.ppid;
}

/* ---------- InitializeProcThreadAttributeList ---------- */
TEST(pcattr_init_size_query_table) {
    REQUIRE(U(InitializeProcThreadAttributeList));
    DWORD n;
    for (n = 0; n < 28; n++) {
        SIZE_T sz = 0xDEAD;
        SET_ERR(0); CHECK(!U(InitializeProcThreadAttributeList)(NULL, n, 0, &sz)); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
        CHECKF(sz == (n + 1) * 24, "count %lu size %llu", n, (unsigned long long)sz);
    }
}
TEST(pcattr_init_count_28_or_more_fails_87) {
    REQUIRE(U(InitializeProcThreadAttributeList));
    static const DWORD c[] = { 28, 29, 64, 0xFFFF, 0x7FFFFFFF, 0xFFFFFFFF };
    int i; for (i = 0; i < 6; i++) {
        SIZE_T sz = 0x1234; SET_ERR(0);
        CHECK(!U(InitializeProcThreadAttributeList)(NULL, c[i], 0, &sz)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(pcattr_init_nonzero_flags_fail_87) {
    REQUIRE(U(InitializeProcThreadAttributeList));
    static const DWORD f[] = { 1, 2, 0x80, 0x80000000, 0xFFFFFFFF }; int i;
    for (i = 0; i < 5; i++) { SIZE_T sz = 0; SET_ERR(0); CHECK(!U(InitializeProcThreadAttributeList)(NULL, 1, f[i], &sz)); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
}
TEST(pcattr_init_too_small_buffer_reports_required_size) {
    REQUIRE(U(InitializeProcThreadAttributeList));
    BYTE buf[1024]; SIZE_T sz = 3 * 24 - 1;
    SET_ERR(0); CHECK(!U(InitializeProcThreadAttributeList)((AL)buf, 2, 0, &sz)); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    EQV(sz, 3 * 24);
    sz = 0; SET_ERR(0); CHECK(!U(InitializeProcThreadAttributeList)((AL)buf, 2, 0, &sz)); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER); EQV(sz, 72);
}
TEST(pcattr_init_exact_and_oversized_buffer) {
    REQUIRE(U(InitializeProcThreadAttributeList));
    BYTE buf[1024]; SIZE_T sz = 3 * 24;
    CHECK(U(InitializeProcThreadAttributeList)((AL)buf, 2, 0, &sz)); EQV(sz, 72);
    U(DeleteProcThreadAttributeList)((AL)buf);
    sz = 1000; CHECK(U(InitializeProcThreadAttributeList)((AL)buf, 2, 0, &sz)); EQV(sz, 72);
    U(DeleteProcThreadAttributeList)((AL)buf);
}
TEST(pcattr_init_zero_attributes) {
    REQUIRE(U(InitializeProcThreadAttributeList));
    BYTE buf[64]; SIZE_T sz = 0;
    SET_ERR(0); CHECK(!U(InitializeProcThreadAttributeList)(NULL, 0, 0, &sz)); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER); EQV(sz, 24);
    CHECK(U(InitializeProcThreadAttributeList)((AL)buf, 0, 0, &sz));
    { DWORD v = 1; SET_ERR(0); CHECK(!upd((AL)buf, A_NODE, &v, 2)); EXPECT_ERR(ERROR_GEN_FAILURE); }
    U(DeleteProcThreadAttributeList)((AL)buf);
}
TEST(pcattr_init_header_fields) {
    REQUIRE(U(InitializeProcThreadAttributeList));
    BYTE buf[256]; SIZE_T sz = sizeof buf; memset(buf, 0xEE, sizeof buf);
    CHECK(U(InitializeProcThreadAttributeList)((AL)buf, 5, 0, &sz));
    CHECK(*(DWORD *)(buf + 0) == 0); CHECK(*(DWORD *)(buf + 4) == 5); CHECK(*(DWORD *)(buf + 8) == 0);
    U(DeleteProcThreadAttributeList)((AL)buf);
}
TEST(pcattr_init_reinitialise_same_buffer) {
    REQUIRE(U(InitializeProcThreadAttributeList));
    BYTE buf[256]; SIZE_T sz = sizeof buf; WORD node = 0; int i;
    for (i = 0; i < 3; i++) {
        sz = sizeof buf; CHECK(U(InitializeProcThreadAttributeList)((AL)buf, 2, 0, &sz));
        CHECK(upd((AL)buf, A_NODE, &node, 2));
        U(DeleteProcThreadAttributeList)((AL)buf);
    }
}
TEST(pcattr_delete_fresh_and_used_lists) {
    REQUIRE(U(DeleteProcThreadAttributeList));
    AL l = mk(3); WORD node = 0; CHECK(l != NULL);
    if (!l) return;
    CHECK(upd(l, A_NODE, &node, 2));
    rel(l);
    l = mk(1); rel(l);
}

/* ---------- UpdateProcThreadAttribute ---------- */
TEST(pcattr_update_bad_flags_87) {
    REQUIRE(U(UpdateProcThreadAttribute));
    AL l = mk(2); WORD node = 0; static const DWORD f[] = { 2, 4, 0x100, 0x80000000, 0xFFFFFFFE }; int i;
    for (i = 0; i < 5; i++) { SET_ERR(0); CHECK(!U(UpdateProcThreadAttribute)(l, f[i], A_NODE, &node, 2, NULL, NULL)); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
    rel(l);
}
TEST(pcattr_update_replace_value_flag_rejected_for_non_additive) {
    REQUIRE(U(UpdateProcThreadAttribute));
    AL l = mk(2); WORD node = 0;
    SET_ERR(0); CHECK(!U(UpdateProcThreadAttribute)(l, PROC_THREAD_ATTRIBUTE_REPLACE_VALUE, A_NODE, &node, 2, NULL, NULL)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    rel(l);
}
TEST(pcattr_update_unknown_attributes_not_supported) {
    REQUIRE(U(UpdateProcThreadAttribute));
    AL l = mk(4); WORD v = 0; static const DWORD_PTR a[] = { 0, 0x20001, 0x20050, 0x2FFFF, 0x10000, 0x7FFF0000 }; int i;
    for (i = 0; i < 6; i++) { SET_ERR(0); CHECKF(!upd(l, a[i], &v, 2), "attr 0x%llx", (unsigned long long)a[i]); EXPECT_ERR(ERROR_NOT_SUPPORTED); }
    rel(l);
}
TEST(pcattr_update_size_checks_bad_length) {
    REQUIRE(U(UpdateProcThreadAttribute));
    static const struct { DWORD_PTR a; SIZE_T good; SIZE_T bad[4]; } t[] = {
        { A_PARENT,   8, { 0, 4, 16, 1 } },
        { A_NODE,     2, { 0, 1, 4, 8 } },
        { A_PROTLVL,  4, { 0, 1, 2, 8 } },
        { A_CHILDPOL, 4, { 0, 1, 8, 2 } },
        { A_ALLAPP,   4, { 0, 1, 8, 2 } },
        { A_GROUPAFF, 16, { 0, 8, 12, 24 } },
        { A_IDEALCPU, 4, { 0, 2, 8, 16 } },
        { A_PCON,     8, { 0, 4, 16, 1 } },
    };
    BYTE v[64]; int i, j; AL l = mk(2);
    memset(v, 0, sizeof v);
    for (i = 0; i < (int)(sizeof t / sizeof t[0]); i++) {
        for (j = 0; j < 4; j++) { SET_ERR(0); CHECKF(!upd(l, t[i].a, v, t[i].bad[j]), "attr 0x%llx size %llu", (unsigned long long)t[i].a, (unsigned long long)t[i].bad[j]); EXPECT_ERR(ERROR_BAD_LENGTH); }
        {
            AL one = mk(1); CHECKF(upd(one, t[i].a, v, t[i].good), "attr 0x%llx good size %llu", (unsigned long long)t[i].a, (unsigned long long)t[i].good); rel(one);
        }
    }
    rel(l);
}
TEST(pcattr_update_handle_list_size_multiples_of_8) {
    REQUIRE(U(UpdateProcThreadAttribute));
    HANDLE h[3] = { 0 }; AL l = mk(1); SIZE_T s;
    for (s = 1; s < 8; s++) { SET_ERR(0); CHECK(!upd(l, A_HANDLES, h, s)); EXPECT_ERR(ERROR_BAD_LENGTH); }
    SET_ERR(0); CHECK(!upd(l, A_HANDLES, h, 0)); EXPECT_ERR(ERROR_BAD_LENGTH);
    SET_ERR(0); CHECK(!upd(l, A_HANDLES, h, 12)); EXPECT_ERR(ERROR_BAD_LENGTH);
    CHECK(upd(l, A_HANDLES, h, 8));
    rel(l);
    l = mk(1); CHECK(upd(l, A_HANDLES, h, 24)); rel(l);
}
TEST(pcattr_update_job_list_size_multiples_of_8) {
    REQUIRE(U(UpdateProcThreadAttribute));
    HANDLE h[2] = { 0 }; AL l = mk(1);
    SET_ERR(0); CHECK(!upd(l, A_JOBLIST, h, 0)); EXPECT_ERR(ERROR_BAD_LENGTH);
    SET_ERR(0); CHECK(!upd(l, A_JOBLIST, h, 4)); EXPECT_ERR(ERROR_BAD_LENGTH);
    SET_ERR(0); CHECK(!upd(l, A_JOBLIST, h, 12)); EXPECT_ERR(ERROR_BAD_LENGTH);
    CHECK(upd(l, A_JOBLIST, h, 16));
    rel(l);
}
TEST(pcattr_update_mitigation_policy_sizes) {
    REQUIRE(U(UpdateProcThreadAttribute));
    BYTE v[32]; SIZE_T s; AL l; memset(v, 0, sizeof v);
    static const SIZE_T good[] = { 4, 8, 16 }; static const SIZE_T bad[] = { 0, 1, 2, 3, 5, 6, 7, 9, 12, 20, 24, 32 }; int i;
    for (i = 0; i < 3; i++) { l = mk(1); CHECKF(upd(l, A_MITIG, v, good[i]), "size %llu", (unsigned long long)good[i]); rel(l); }
    l = mk(1);
    for (i = 0; i < 12; i++) { s = bad[i]; SET_ERR(0); CHECKF(!upd(l, A_MITIG, v, s), "size %llu", (unsigned long long)s); EXPECT_ERR(ERROR_BAD_LENGTH); }
    rel(l);
}
TEST(pcattr_update_security_capabilities_validation) {
    REQUIRE(U(UpdateProcThreadAttribute));
    BYTE v[0x30]; AL l = mk(2); memset(v, 0, sizeof v);
    SET_ERR(0); CHECK(!upd(l, A_SECCAP, v, 0x10)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!upd(l, A_SECCAP, v, 0x20)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!upd(l, A_SECCAP, v, 0)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    *(DWORD *)(v + 0x14) = 1;     /* Reserved must be zero */
    SET_ERR(0); CHECK(!upd(l, A_SECCAP, v, 0x18)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    *(DWORD *)(v + 0x14) = 0;
    CHECK(upd(l, A_SECCAP, v, 0x18));
    rel(l);
}
TEST(pcattr_update_capacity_exhausted) {
    REQUIRE(U(UpdateProcThreadAttribute));
    AL l = mk(2); WORD node = 0; DWORD pol = 0; HANDLE par = GetCurrentProcess();
    CHECK(upd(l, A_NODE, &node, 2)); CHECK(upd(l, A_CHILDPOL, &pol, 4));
    SET_ERR(0); CHECK(!upd(l, A_PARENT, &par, 8)); EXPECT_ERR(ERROR_GEN_FAILURE);
    rel(l);
}
TEST(pcattr_update_duplicate_attribute_fails) {
    REQUIRE(U(UpdateProcThreadAttribute));
    AL l = mk(3); WORD node = 0;
    CHECK(upd(l, A_NODE, &node, 2));
    SET_ERR(0); CHECK(!upd(l, A_NODE, &node, 2)); t_note("duplicate attribute error %lu", GetLastError());
    node = 1; CHECK(!upd(l, A_NODE, &node, 2));
    rel(l);
}
TEST(pcattr_update_previous_value_and_return_size_must_be_null) {
    REQUIRE(U(UpdateProcThreadAttribute));
    AL l = mk(3); WORD node = 0; WORD prev = 0; SIZE_T rs = 0;
    SET_ERR(0); CHECK(!U(UpdateProcThreadAttribute)(l, 0, A_NODE, &node, 2, &prev, NULL)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!U(UpdateProcThreadAttribute)(l, 0, A_NODE, &node, 2, NULL, &rs)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!U(UpdateProcThreadAttribute)(l, 0, A_NODE, &node, 2, &prev, &rs)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    CHECK(U(UpdateProcThreadAttribute)(l, 0, A_NODE, &node, 2, NULL, NULL));
    rel(l);
}
TEST(pcattr_update_failed_update_does_not_consume_slot) {
    REQUIRE(U(UpdateProcThreadAttribute));
    AL l = mk(1); WORD node = 0; BYTE bad[3] = { 0 };
    CHECK(!upd(l, A_NODE, bad, 3));
    CHECK(!upd(l, 0x20050, bad, 2));
    CHECK(upd(l, A_NODE, &node, 2));
    rel(l);
}
TEST(pcattr_update_additive_attribute) {
    REQUIRE(U(UpdateProcThreadAttribute));
    AL l = mk(1); DWORD v1 = 1, v2 = 2, bad = 8, prev = 0xFF; WORD node = 0;
    CHECK(upd(l, A_ADDITIVE, &v1, 4));
    CHECK(upd(l, A_ADDITIVE, &v2, 4));        /* additive: neither duplicate nor capacity error */
    CHECK(U(UpdateProcThreadAttribute)(l, PROC_THREAD_ATTRIBUTE_REPLACE_VALUE, A_ADDITIVE, &v2, 4, NULL, NULL));
    SET_ERR(0); CHECK(!upd(l, A_ADDITIVE, &bad, 4)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!upd(l, A_ADDITIVE, &v1, 8)); EXPECT_ERR(ERROR_BAD_LENGTH);
    SET_ERR(0); CHECK(!upd(l, A_ADDITIVE, &v1, 2)); EXPECT_ERR(ERROR_BAD_LENGTH);
    if (U(UpdateProcThreadAttribute)(l, 0, A_ADDITIVE, &v1, 4, &prev, NULL)) t_note("previous value reported: %lu", prev);
    rel(l);
}
TEST(pcattr_update_distinct_attributes_all_fit) {
    REQUIRE(U(UpdateProcThreadAttribute));
    AL l = mk(6); WORD node = 0; DWORD pol = 0, prot = 0; HANDLE par = GetCurrentProcess(); HANDLE hl[1] = { 0 }; GROUP_AFFINITY ga; PROCESSOR_NUMBER pn; DWORD all = 0;
    memset(&ga, 0, sizeof ga); memset(&pn, 0, sizeof pn);
    ga.Mask = 1;
    CHECK(upd(l, A_NODE, &node, 2)); CHECK(upd(l, A_CHILDPOL, &pol, 4)); CHECK(upd(l, A_PARENT, &par, 8));
    CHECK(upd(l, A_GROUPAFF, &ga, sizeof ga)); CHECK(upd(l, A_IDEALCPU, &pn, sizeof pn)); CHECK(upd(l, A_ALLAPP, &all, 4));
    SET_ERR(0); CHECK(!upd(l, A_PROTLVL, &prot, 4)); EXPECT_ERR(ERROR_GEN_FAILURE);
    rel(l);
}
TEST(pcattr_update_win32k_and_desktop_policy_sizes) {
    REQUIRE(U(UpdateProcThreadAttribute));
    BYTE v[16] = { 0 }; AL l = mk(2);
    CHECK(upd(l, A_DESKTOP, v, 4));
    SET_ERR(0); CHECK(!upd(l, A_DESKTOP, v, 8));
    rel(l); l = mk(1);
    SET_ERR(0); CHECK(!upd(l, A_WIN32K, v, 4)); EXPECT_ERR(ERROR_BAD_LENGTH);
    CHECK(upd(l, A_WIN32K, v, 8));
    rel(l);
}

/* ---------- attributes applied by CreateProcess ---------- */
TEST(pcattr_create_with_empty_extended_startupinfo) {
    REQUIRE(UK(CreateProcessW));
    STARTUPINFOEXW sx; pc_opts o; AL l = mk(0);
    exopts(&o, &sx, l, "pc_exit77", "", 0);
    EQV(pc_run(&o, PC_TIMEOUT), 77); rel(l);
    exopts(&o, &sx, NULL, "pc_exit77", "", 0);
    EQV(pc_run(&o, PC_TIMEOUT), 77);
}
TEST(pcattr_list_is_reusable_for_multiple_creations) {
    REQUIRE(UK(CreateProcessW));
    PROCESS_INFORMATION p, c1, c2; HANDLE ph; STARTUPINFOEXW sx; pc_opts o; AL l = mk(1);
    CHECK(pc_suspended("pc_sleep", "20000", 0, &p)); ph = p.hProcess;
    CHECK(upd(l, A_PARENT, &ph, sizeof ph));
    exopts(&o, &sx, l, "pc_exit77", "", CREATE_SUSPENDED);
    CHECK(pc_launch(&o, &c1)); CHECK(pc_launch(&o, &c2));
    EQV(ppid_of(c1.hProcess), p.dwProcessId); EQV(ppid_of(c2.hProcess), p.dwProcessId);
    pc_kill(&c1); pc_kill(&c2); pc_kill(&p); rel(l);
}
TEST(pcattr_parent_process_changes_reported_parent) {
    REQUIRE(UK(CreateProcessW));
    PROCESS_INFORMATION p, c; HANDLE ph; STARTUPINFOEXW sx; pc_opts o; AL l = mk(1);
    CHECK(pc_suspended("pc_sleep", "20000", 0, &p)); ph = p.hProcess;
    CHECK(upd(l, A_PARENT, &ph, sizeof ph));
    exopts(&o, &sx, l, "pc_exit77", "", CREATE_SUSPENDED);
    CHECK(pc_launch(&o, &c));
    EQV(ppid_of(c.hProcess), p.dwProcessId); CHECK(ppid_of(c.hProcess) != GetCurrentProcessId());
    pc_kill(&c); pc_kill(&p); rel(l);
}
TEST(pcattr_parent_process_pseudo_handle_is_self) {
    REQUIRE(UK(CreateProcessW));
    PROCESS_INFORMATION c; HANDLE ph = GetCurrentProcess(); STARTUPINFOEXW sx; pc_opts o; AL l = mk(1);
    CHECK(upd(l, A_PARENT, &ph, sizeof ph));
    exopts(&o, &sx, l, "pc_exit77", "", CREATE_SUSPENDED);
    CHECK(pc_launch(&o, &c));
    EQV(ppid_of(c.hProcess), GetCurrentProcessId());
    pc_kill(&c); rel(l);
}
TEST(pcattr_parent_process_requires_create_process_access) {
    REQUIRE(UK(CreateProcessW));
    PROCESS_INFORMATION p, c; HANDLE ph, lim; STARTUPINFOEXW sx; pc_opts o; AL l = mk(1);
    CHECK(pc_suspended("pc_sleep", "20000", 0, &p));
    lim = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_DUP_HANDLE, FALSE, p.dwProcessId); CHECK(lim != NULL); ph = lim;
    CHECK(upd(l, A_PARENT, &ph, sizeof ph));
    exopts(&o, &sx, l, "pc_exit77", "", CREATE_SUSPENDED);
    SET_ERR(0); CHECK(!pc_launch(&o, &c)); EXPECT_ERR(ERROR_ACCESS_DENIED);
    if (lim) CloseHandle(lim);
    pc_kill(&p); rel(l);
}
TEST(pcattr_parent_process_bad_handles) {
    REQUIRE(UK(CreateProcessW));
    PROCESS_INFORMATION c; HANDLE ph; STARTUPINFOEXW sx; pc_opts o; AL l = mk(1); HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    ph = (HANDLE)(ULONG_PTR)0x7654; CHECK(upd(l, A_PARENT, &ph, sizeof ph));
    exopts(&o, &sx, l, "pc_exit77", "", CREATE_SUSPENDED);
    SET_ERR(0); CHECK(!pc_launch(&o, &c)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    rel(l); l = mk(1); ph = ev; CHECK(upd(l, A_PARENT, &ph, sizeof ph)); exopts(&o, &sx, l, "pc_exit77", "", CREATE_SUSPENDED);
    SET_ERR(0); CHECK(!pc_launch(&o, &c)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    rel(l); l = mk(1); ph = NULL; CHECK(upd(l, A_PARENT, &ph, sizeof ph)); exopts(&o, &sx, l, "pc_exit77", "", CREATE_SUSPENDED);
    SET_ERR(0); CHECK(!pc_launch(&o, &c)); t_note("NULL parent handle error %lu", GetLastError());
    rel(l); CloseHandle(ev);
}
TEST(pcattr_parent_process_terminated_parent_observed) {
    REQUIRE(UK(CreateProcessW));
    PROCESS_INFORMATION p, c; HANDLE ph; STARTUPINFOEXW sx; pc_opts o; AL l = mk(1);
    CHECK(pc_suspended("pc_sleep", "20000", 0, &p)); ph = p.hProcess;
    TerminateProcess(p.hProcess, 3); WaitForSingleObject(p.hProcess, 5000);
    CHECK(upd(l, A_PARENT, &ph, sizeof ph));
    exopts(&o, &sx, l, "pc_exit77", "", CREATE_SUSPENDED);
    SET_ERR(0);
    if (pc_launch(&o, &c)) { t_note("terminated parent accepted"); pc_kill(&c); } else t_note("terminated parent rejected err %lu", GetLastError());
    pc_kill(&p); rel(l);
}

/* handle list */
static HANDLE hl_junk[600]; static int hl_nj;
static HANDLE hl_mk(BOOL inh) {
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, inh }; HANDLE h = CreateEventW(&sa, TRUE, FALSE, NULL); hl_junk[hl_nj++] = h; return h;
}
static void hl_fill(void) { hl_nj = 0; while (hl_nj < 590) { HANDLE h = hl_mk(FALSE); if (h && (ULONG_PTR)h >= 0x400) break; } }
static void hl_free(void) { while (hl_nj > 0) CloseHandle(hl_junk[--hl_nj]); }
static void hl_args(char *a, HANDLE h1, HANDLE h2, HANDLE h3) { sprintf(a, "%llx %llx %llx", (unsigned long long)(ULONG_PTR)h1, (unsigned long long)(ULONG_PTR)h2, (unsigned long long)(ULONG_PTR)h3); }
TEST(pcattr_handle_list_limits_inheritance) {
    REQUIRE(UK(CreateProcessW));
    HANDLE e1, e2, e3, lst[1]; char a[200]; STARTUPINFOEXW sx; pc_opts o; AL l = mk(1);
    hl_fill(); e1 = hl_mk(TRUE); e2 = hl_mk(TRUE); e3 = hl_mk(TRUE); lst[0] = e1; hl_args(a, e1, e2, e3);
    CHECK(upd(l, A_HANDLES, lst, sizeof lst));
    exopts(&o, &sx, l, "pc_handles_x", a, 0); o.inherit = TRUE;
    EQV(pc_run(&o, PC_TIMEOUT), 1);
    rel(l); hl_free();
}
TEST(pcattr_handle_list_two_entries) {
    REQUIRE(UK(CreateProcessW));
    HANDLE e1, e2, e3, lst[2]; char a[200]; STARTUPINFOEXW sx; pc_opts o; AL l = mk(1);
    hl_fill(); e1 = hl_mk(TRUE); e2 = hl_mk(TRUE); e3 = hl_mk(TRUE); lst[0] = e1; lst[1] = e3; hl_args(a, e1, e2, e3);
    CHECK(upd(l, A_HANDLES, lst, sizeof lst));
    exopts(&o, &sx, l, "pc_handles_x", a, 0); o.inherit = TRUE;
    EQV(pc_run(&o, PC_TIMEOUT), 5);
    rel(l); hl_free();
}
TEST(pcattr_handle_list_ignored_without_extended_flag) {
    REQUIRE(UK(CreateProcessW));
    HANDLE e1, e2, e3, lst[1]; char a[200]; STARTUPINFOEXW sx; pc_opts o; AL l = mk(1);
    hl_fill(); e1 = hl_mk(TRUE); e2 = hl_mk(TRUE); e3 = hl_mk(TRUE); lst[0] = e1; hl_args(a, e1, e2, e3);
    CHECK(upd(l, A_HANDLES, lst, sizeof lst));
    exopts(&o, &sx, l, "pc_handles_x", a, 0); o.inherit = TRUE; o.flags &= ~EXTENDED_STARTUPINFO_PRESENT;
    EQV(pc_run(&o, PC_TIMEOUT), 7);
    rel(l); hl_free();
}
TEST(pcattr_handle_list_requires_inherit_handles_true) {
    REQUIRE(UK(CreateProcessW));
    HANDLE e1, lst[1]; STARTUPINFOEXW sx; pc_opts o; PROCESS_INFORMATION pi; AL l = mk(1);
    hl_fill(); e1 = hl_mk(TRUE); lst[0] = e1;
    CHECK(upd(l, A_HANDLES, lst, sizeof lst));
    exopts(&o, &sx, l, "pc_exit77", "", CREATE_SUSPENDED); o.inherit = FALSE;
    SET_ERR(0); CHECK(!pc_launch(&o, &pi)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    rel(l); hl_free();
}
TEST(pcattr_handle_list_rejects_non_inheritable_handle) {
    REQUIRE(UK(CreateProcessW));
    HANDLE e1, lst[1]; STARTUPINFOEXW sx; pc_opts o; PROCESS_INFORMATION pi; AL l = mk(1);
    hl_fill(); e1 = hl_mk(FALSE); lst[0] = e1;
    CHECK(upd(l, A_HANDLES, lst, sizeof lst));
    exopts(&o, &sx, l, "pc_exit77", "", CREATE_SUSPENDED); o.inherit = TRUE;
    SET_ERR(0); CHECK(!pc_launch(&o, &pi)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    rel(l); hl_free();
}
TEST(pcattr_handle_list_invalid_handle_value_observed) {
    REQUIRE(UK(CreateProcessW));
    HANDLE lst[1]; STARTUPINFOEXW sx; pc_opts o; PROCESS_INFORMATION pi; AL l = mk(1);
    lst[0] = (HANDLE)(ULONG_PTR)0x7654;
    CHECK(upd(l, A_HANDLES, lst, sizeof lst));
    exopts(&o, &sx, l, "pc_exit77", "", CREATE_SUSPENDED); o.inherit = TRUE;
    SET_ERR(0);
    if (pc_launch(&o, &pi)) { t_note("invalid handle in list accepted"); pc_kill(&pi); } else t_note("invalid handle in list rejected err %lu", GetLastError());
    rel(l);
}
TEST(pcattr_handle_list_with_std_pipe_in_list_works) {
    REQUIRE(UK(CreateProcessW));
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE }; HANDLE r, w, lst[1]; STARTUPINFOEXW sx; pc_opts o; AL l = mk(1); char buf[256] = ""; DWORD got = 0, code;
    PROCESS_INFORMATION pi;
    CHECK(CreatePipe(&r, &w, &sa, 4096)); SetHandleInformation(r, HANDLE_FLAG_INHERIT, 0);
    lst[0] = w; CHECK(upd(l, A_HANDLES, lst, sizeof lst));
    exopts(&o, &sx, l, "pc_stdio", "noread", CREATE_NO_WINDOW); o.inherit = TRUE; o.hout = w; o.herr = w;
    CHECK(pc_launch(&o, &pi)); CloseHandle(w);
    WaitForSingleObject(pi.hProcess, PC_TIMEOUT); GetExitCodeProcess(pi.hProcess, &code); EQV(code, 0);
    CHECK(ReadFile(r, buf, 255, &got, NULL)); buf[got] = 0; CHECK(strstr(buf, "out=hello-stdout") != NULL);
    CloseHandle(r); CloseHandle(pi.hProcess); CloseHandle(pi.hThread); rel(l);
}
TEST(pcattr_handle_list_std_handle_missing_from_list_observed) {
    REQUIRE(UK(CreateProcessW));
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE }; HANDLE r, w, other, lst[1]; STARTUPINFOEXW sx; pc_opts o; AL l = mk(1); PROCESS_INFORMATION pi;
    CHECK(CreatePipe(&r, &w, &sa, 4096)); other = hl_mk(TRUE);
    lst[0] = other; CHECK(upd(l, A_HANDLES, lst, sizeof lst));
    exopts(&o, &sx, l, "pc_stdprobe", "", CREATE_NO_WINDOW); o.inherit = TRUE; o.hout = w; o.herr = w;
    SET_ERR(0);
    if (pc_launch(&o, &pi)) { DWORD c = 0; WaitForSingleObject(pi.hProcess, PC_TIMEOUT); GetExitCodeProcess(pi.hProcess, &c); t_note("std handle outside HANDLE_LIST: child probe exit %lu", c); CloseHandle(pi.hProcess); CloseHandle(pi.hThread); }
    else t_note("std handle outside HANDLE_LIST: CreateProcess failed err %lu", GetLastError());
    CloseHandle(r); CloseHandle(w); rel(l); hl_free();
}

/* thread-class attributes */
TEST(pcattr_group_affinity_applies_to_initial_thread) {
    REQUIRE(UK(CreateProcessW)); REQUIRE(U(GetThreadGroupAffinity));
    DWORD_PTR pm = 0, sm = 0; GROUP_AFFINITY ga, got; STARTUPINFOEXW sx; pc_opts o; PROCESS_INFORMATION pi; AL l = mk(1);
    GetProcessAffinityMask(GetCurrentProcess(), &pm, &sm);
    memset(&ga, 0, sizeof ga); ga.Mask = pm & (~pm + 1); ga.Group = 0;
    CHECK(upd(l, A_GROUPAFF, &ga, sizeof ga));
    exopts(&o, &sx, l, "pc_exit77", "", CREATE_SUSPENDED);
    CHECK(pc_launch(&o, &pi));
    memset(&got, 0xCC, sizeof got);
    CHECK(U(GetThreadGroupAffinity)(pi.hThread, &got)); EQV(got.Mask, ga.Mask); EQV(got.Group, 0);
    pc_kill(&pi); rel(l);
}
TEST(pcattr_group_affinity_zero_mask_observed) {
    REQUIRE(UK(CreateProcessW));
    GROUP_AFFINITY ga; STARTUPINFOEXW sx; pc_opts o; PROCESS_INFORMATION pi; AL l = mk(1);
    memset(&ga, 0, sizeof ga);
    CHECK(upd(l, A_GROUPAFF, &ga, sizeof ga));
    exopts(&o, &sx, l, "pc_exit77", "", CREATE_SUSPENDED);
    SET_ERR(0); if (pc_launch(&o, &pi)) { t_note("zero group affinity mask accepted"); pc_kill(&pi); } else t_note("zero group affinity mask rejected err %lu", GetLastError());
    rel(l);
}
TEST(pcattr_ideal_processor_applies_to_initial_thread) {
    REQUIRE(UK(CreateProcessW)); REQUIRE(U(GetThreadIdealProcessorEx));
    PROCESSOR_NUMBER pn, got; STARTUPINFOEXW sx; pc_opts o; PROCESS_INFORMATION pi; AL l = mk(1); SYSTEM_INFO si;
    GetSystemInfo(&si);
    memset(&pn, 0, sizeof pn); pn.Group = 0; pn.Number = si.dwNumberOfProcessors > 1 ? 1 : 0;
    CHECK(upd(l, A_IDEALCPU, &pn, sizeof pn));
    exopts(&o, &sx, l, "pc_exit77", "", CREATE_SUSPENDED);
    CHECK(pc_launch(&o, &pi));
    memset(&got, 0xCC, sizeof got);
    CHECK(U(GetThreadIdealProcessorEx)(pi.hThread, &got)); EQV(got.Number, pn.Number); EQV(got.Group, 0);
    pc_kill(&pi); rel(l);
}
TEST(pcattr_preferred_node_zero_accepted) {
    REQUIRE(UK(CreateProcessW));
    WORD node = 0; STARTUPINFOEXW sx; pc_opts o; AL l = mk(1);
    CHECK(upd(l, A_NODE, &node, 2));
    exopts(&o, &sx, l, "pc_exit77", "", 0);
    EQV(pc_run(&o, PC_TIMEOUT), 77); rel(l);
}
TEST(pcattr_preferred_node_out_of_range_observed) {
    REQUIRE(UK(CreateProcessW));
    WORD node = 0x7FF0; STARTUPINFOEXW sx; pc_opts o; AL l = mk(1); PROCESS_INFORMATION pi;
    CHECK(upd(l, A_NODE, &node, 2));
    exopts(&o, &sx, l, "pc_exit77", "", CREATE_SUSPENDED);
    SET_ERR(0); if (pc_launch(&o, &pi)) { t_note("out-of-range preferred node accepted"); pc_kill(&pi); } else t_note("out-of-range preferred node rejected err %lu", GetLastError());
    rel(l);
}

/* mitigation / child process policy / job list */
TEST(pcattr_mitigation_policy_strict_handle_checks) {
    REQUIRE(UK(CreateProcessW));
    ULONG64 pol = 1ULL << 24; STARTUPINFOEXW sx; pc_opts o; AL l = mk(1); char out[2048]; DWORD code;
    CHECK(upd(l, A_MITIG, &pol, 8));
    exopts(&o, &sx, l, "pc_mit_child", "", CREATE_NO_WINDOW);
    CHECK(pc_capture(&o, out, sizeof out, NULL, 0, &code, PC_TIMEOUT)); EQV(code, 0);
    EQV(pc_num(out, "ok", -1), 1); EQV(pc_num(out, "raise", -1), 1);
    rel(l);
}
TEST(pcattr_mitigation_policy_dword_form) {
    REQUIRE(UK(CreateProcessW));
    DWORD pol = 1UL << 24; STARTUPINFOEXW sx; pc_opts o; AL l = mk(1); char out[2048]; DWORD code;
    CHECK(upd(l, A_MITIG, &pol, 4));
    exopts(&o, &sx, l, "pc_mit_child", "", CREATE_NO_WINDOW);
    CHECK(pc_capture(&o, out, sizeof out, NULL, 0, &code, PC_TIMEOUT)); EQV(code, 0);
    EQV(pc_num(out, "raise", -1), 1);
    rel(l);
}
TEST(pcattr_mitigation_policy_absent_by_default) {
    REQUIRE(UK(CreateProcessW));
    char out[2048]; DWORD code = pc_cap("pc_mit_child", "", CREATE_NO_WINDOW, out, sizeof out);
    EQV(code, 0); EQV(pc_num(out, "ok", -1), 1); EQV(pc_num(out, "raise", -1), 0);
}
TEST(pcattr_child_process_policy_restricted) {
    REQUIRE(UK(CreateProcessW));
    DWORD pol = 1; STARTUPINFOEXW sx; pc_opts o; AL l = mk(1); char out[2048]; DWORD code;
    CHECK(upd(l, A_CHILDPOL, &pol, 4));
    exopts(&o, &sx, l, "pc_childpol_child", "", CREATE_NO_WINDOW);
    CHECK(pc_capture(&o, out, sizeof out, NULL, 0, &code, PC_TIMEOUT)); EQV(code, 0);
    EQV(pc_num(out, "cok", -1), 1); EQV(pc_num(out, "nochild", -1), 1);
    EQV(pc_num(out, "created", -1), 0);
    t_note("restricted child CreateProcess error %lld", pc_num(out, "err", -1));
    rel(l);
}
TEST(pcattr_child_process_policy_default_allows_children) {
    REQUIRE(UK(CreateProcessW));
    char out[2048]; DWORD code = pc_cap("pc_childpol_child", "", CREATE_NO_WINDOW, out, sizeof out);
    EQV(code, 0); EQV(pc_num(out, "nochild", -1), 0); EQV(pc_num(out, "created", -1), 1);
}
TEST(pcattr_job_list_assigns_child_to_job) {
    REQUIRE(UK(CreateProcessW));
    HANDLE job = CreateJobObjectW(NULL, NULL); STARTUPINFOEXW sx; pc_opts o; AL l = mk(1); PROCESS_INFORMATION pi; BOOL in = FALSE, other = TRUE;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION li; HANDLE other_job = CreateJobObjectW(NULL, NULL);
    memset(&li, 0, sizeof li); li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(job, JobObjectExtendedLimitInformation, &li, sizeof li);
    CHECK(upd(l, A_JOBLIST, &job, sizeof job));
    exopts(&o, &sx, l, "pc_sleep", "20000", CREATE_SUSPENDED);
    CHECK(pc_launch(&o, &pi));
    CHECK(IsProcessInJob(pi.hProcess, job, &in)); CHECK(in);
    CHECK(IsProcessInJob(pi.hProcess, other_job, &other)); CHECK(!other);
    pc_kill(&pi); CloseHandle(job); CloseHandle(other_job); rel(l);
}
TEST(pcattr_job_list_invalid_job_handle) {
    REQUIRE(UK(CreateProcessW));
    HANDLE job = (HANDLE)(ULONG_PTR)0x7654; STARTUPINFOEXW sx; pc_opts o; AL l = mk(1); PROCESS_INFORMATION pi;
    CHECK(upd(l, A_JOBLIST, &job, sizeof job));
    exopts(&o, &sx, l, "pc_exit77", "", CREATE_SUSPENDED);
    SET_ERR(0); CHECK(!pc_launch(&o, &pi)); t_note("invalid job handle error %lu", GetLastError());
    rel(l);
}
TEST(pcattr_all_application_packages_policy_accepted) {
    REQUIRE(UK(CreateProcessW));
    DWORD pol = 1; STARTUPINFOEXW sx; pc_opts o; AL l = mk(1); PROCESS_INFORMATION pi;
    CHECK(upd(l, A_ALLAPP, &pol, 4));
    exopts(&o, &sx, l, "pc_exit77", "", CREATE_SUSPENDED);
    SET_ERR(0); if (pc_launch(&o, &pi)) { t_note("ALL_APPLICATION_PACKAGES_POLICY accepted"); pc_kill(&pi); } else t_note("ALL_APPLICATION_PACKAGES_POLICY rejected err %lu", GetLastError());
    rel(l);
}
TEST(pcattr_list_with_flag_but_wrong_cb_observed) {
    REQUIRE(UK(CreateProcessW));
    WORD node = 0; STARTUPINFOEXW sx; pc_opts o; AL l = mk(1); PROCESS_INFORMATION pi;
    CHECK(upd(l, A_NODE, &node, 2));
    exopts(&o, &sx, l, "pc_exit77", "", CREATE_SUSPENDED);
    sx.StartupInfo.cb = sizeof(STARTUPINFOW);
    SET_ERR(0); if (pc_launch(&o, &pi)) { t_note("EXTENDED flag with cb=sizeof(STARTUPINFOW) accepted (list ignored?)"); pc_kill(&pi); } else t_note("EXTENDED flag with short cb rejected err %lu", GetLastError());
    rel(l);
}
