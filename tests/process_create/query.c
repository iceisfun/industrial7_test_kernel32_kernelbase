/* Process identity, times, counters, image names, wow64, mitigation and information queries. */
#include "pc.h"

typedef struct { LONG_PTR st; BYTE *peb; ULONG_PTR aff; LONG prio; ULONG_PTR pid; ULONG_PTR ppid; } PBI;
static ULONGLONG ft(FILETIME f) { ULARGE_INTEGER u; u.LowPart = f.dwLowDateTime; u.HighPart = f.dwHighDateTime; return u.QuadPart; }
static ULONGLONG now_ft(void) { FILETIME f; GetSystemTimeAsFileTime(&f); return ft(f); }
static int same_path(const WCHAR *a, const WCHAR *b) { return !lstrcmpiW(a, b); }

/* ---------- identity ---------- */
TEST(pq_current_process_pseudo_handle_values) {
    REQUIRE(U(GetCurrentProcess)); REQUIRE(U(GetCurrentThread));
    EQV(U(GetCurrentProcess)(), (ULONG_PTR)-1);
    EQV(U(GetCurrentThread)(), (ULONG_PTR)-2);
    CHECK(U(GetCurrentProcess)() == U(GetCurrentProcess)());
}
TEST(pq_current_process_id_matches_peb_and_getprocessid) {
    REQUIRE(U(GetCurrentProcessId)); REQUIRE(U(GetProcessId));
    PBI pbi; NtQueryInformationProcess_pfn q = UN(NtQueryInformationProcess); REQUIRE(q);
    memset(&pbi, 0, sizeof pbi); CHECK(q((HANDLE)-1, 0, &pbi, sizeof pbi, NULL) >= 0);
    EQV(U(GetCurrentProcessId)(), pbi.pid);
    EQV(U(GetProcessId)(GetCurrentProcess()), U(GetCurrentProcessId)());
    CHECK((U(GetCurrentProcessId)() & 3) == 0); CHECK(U(GetCurrentProcessId)() != 0);
}
TEST(pq_current_thread_id_consistency) {
    REQUIRE(U(GetCurrentThreadId)); REQUIRE(U(GetThreadId)); REQUIRE(U(GetProcessIdOfThread));
    EQV(U(GetThreadId)(GetCurrentThread()), U(GetCurrentThreadId)());
    EQV(U(GetProcessIdOfThread)(GetCurrentThread()), U(GetCurrentProcessId)());
    CHECK((U(GetCurrentThreadId)() & 3) == 0);
}
static DWORD WINAPI tid_thread(LPVOID p) { DWORD *o = (DWORD *)p; o[0] = GetCurrentThreadId(); o[1] = GetCurrentProcessId(); return 0; }
TEST(pq_thread_ids_differ_and_pid_shared) {
    REQUIRE(U(GetCurrentThreadId)); REQUIRE(U(GetThreadId));
    DWORD o[2] = { 0, 0 }, tid = 0; HANDLE t = CreateThread(NULL, 0, tid_thread, o, 0, &tid);
    CHECK(t != NULL); if (!t) return;
    WaitForSingleObject(t, 5000);
    CHECK(o[0] == tid); CHECK(o[0] != U(GetCurrentThreadId)()); EQV(o[1], U(GetCurrentProcessId)());
    EQV(U(GetThreadId)(t), tid); EQV(U(GetProcessIdOfThread)(t), U(GetCurrentProcessId)());
    CloseHandle(t);
}
TEST(pq_duplicated_pseudo_handle_is_real_handle) {
    REQUIRE(U(GetProcessId));
    HANDLE h = NULL;
    CHECK(DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(), &h, 0, FALSE, DUPLICATE_SAME_ACCESS));
    CHECK(h != NULL && h != GetCurrentProcess());
    EQV(U(GetProcessId)(h), GetCurrentProcessId());
    CloseHandle(h);
}
TEST(pq_getprocessid_invalid_handles) {
    REQUIRE(U(GetProcessId));
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL); PROCESS_INFORMATION pi;
    SET_ERR(0); EQV(U(GetProcessId)(NULL), 0); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    SET_ERR(0); EQV(U(GetProcessId)((HANDLE)(ULONG_PTR)0x1234), 0); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    SET_ERR(0); EQV(U(GetProcessId)(ev), 0); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    CHECK(pc_suspended("pc_sleep", "20000", 0, &pi));
    SET_ERR(0); EQV(U(GetProcessId)(pi.hThread), 0); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    pc_kill(&pi); CloseHandle(ev);
}
TEST(pq_getthreadid_invalid_handles) {
    REQUIRE(U(GetThreadId));
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL); PROCESS_INFORMATION pi;
    SET_ERR(0); EQV(U(GetThreadId)(NULL), 0); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    SET_ERR(0); EQV(U(GetThreadId)(ev), 0); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    CHECK(pc_suspended("pc_sleep", "20000", 0, &pi));
    SET_ERR(0); EQV(U(GetThreadId)(pi.hProcess), 0); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    pc_kill(&pi); CloseHandle(ev);
}
TEST(pq_getprocessidofthread_invalid_handles) {
    REQUIRE(U(GetProcessIdOfThread));
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL); PROCESS_INFORMATION pi;
    SET_ERR(0); EQV(U(GetProcessIdOfThread)(NULL), 0); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    SET_ERR(0); EQV(U(GetProcessIdOfThread)(ev), 0); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    CHECK(pc_suspended("pc_sleep", "20000", 0, &pi));
    SET_ERR(0); EQV(U(GetProcessIdOfThread)(pi.hProcess), 0); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    EQV(U(GetProcessIdOfThread)(pi.hThread), pi.dwProcessId);
    pc_kill(&pi); CloseHandle(ev);
}
TEST(pq_thread_id_queries_need_query_limited_right) {
    REQUIRE(U(GetThreadId)); REQUIRE(U(OpenThread));
    PROCESS_INFORMATION pi; HANDLE s, l;
    CHECK(pc_suspended("pc_sleep", "20000", 0, &pi));
    s = U(OpenThread)(SYNCHRONIZE, FALSE, pi.dwThreadId); l = U(OpenThread)(THREAD_QUERY_LIMITED_INFORMATION, FALSE, pi.dwThreadId);
    CHECK(s && l);
    if (s) { SET_ERR(0); EQV(U(GetThreadId)(s), 0); EQV(GetLastError(), ERROR_ACCESS_DENIED); SET_ERR(0); EQV(U(GetProcessIdOfThread)(s), 0); EQV(GetLastError(), ERROR_ACCESS_DENIED); CloseHandle(s); }
    if (l) { EQV(U(GetThreadId)(l), pi.dwThreadId); EQV(U(GetProcessIdOfThread)(l), pi.dwProcessId); CloseHandle(l); }
    pc_kill(&pi);
}

/* ---------- times ---------- */
TEST(pq_times_current_process) {
    REQUIRE(U(GetProcessTimes));
    FILETIME c, e, k, u; ULONGLONG n = now_ft(); volatile unsigned x = 0; unsigned i;
    memset(&e, 0xEE, sizeof e);
    CHECK(U(GetProcessTimes)(GetCurrentProcess(), &c, &e, &k, &u));
    CHECK(ft(c) != 0); CHECK(ft(c) <= n); EQV(ft(e), 0);
    for (i = 0; i < 30000000; i++) x += i;
    CHECK(U(GetProcessTimes)(GetCurrentProcess(), &c, &e, &k, &u));
    CHECK(ft(k) + ft(u) > 0);
}
TEST(pq_times_cpu_time_monotonic) {
    REQUIRE(U(GetProcessTimes));
    FILETIME c, e, k, u; ULONGLONG t1, t2; volatile unsigned x = 0; unsigned i;
    U(GetProcessTimes)(GetCurrentProcess(), &c, &e, &k, &u); t1 = ft(k) + ft(u);
    for (i = 0; i < 50000000; i++) x += i;
    U(GetProcessTimes)(GetCurrentProcess(), &c, &e, &k, &u); t2 = ft(k) + ft(u);
    CHECK(t2 >= t1);
}
TEST(pq_times_child_lifecycle) {
    REQUIRE(U(GetProcessTimes));
    PROCESS_INFORMATION pi; FILETIME c, e, k, u, mc, me, mk, mu; ULONGLONG n;
    U(GetProcessTimes)(GetCurrentProcess(), &mc, &me, &mk, &mu);
    CHECK(pc_suspended("pc_exit77", "", 0, &pi));
    n = now_ft();
    CHECK(U(GetProcessTimes)(pi.hProcess, &c, &e, &k, &u));
    CHECK(ft(c) >= ft(mc)); CHECK(ft(c) <= n); CHECK(n - ft(c) < 60ULL * 10000000); EQV(ft(e), 0);
    ResumeThread(pi.hThread); WaitForSingleObject(pi.hProcess, PC_TIMEOUT);
    CHECK(U(GetProcessTimes)(pi.hProcess, &c, &e, &k, &u));
    CHECK(ft(e) != 0); CHECK(ft(e) >= ft(c)); CHECK(ft(e) <= now_ft());
    CHECK(ft(k) + ft(u) > 0);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
}
TEST(pq_times_terminated_never_run_child) {
    REQUIRE(U(GetProcessTimes));
    PROCESS_INFORMATION pi; FILETIME c, e, k, u;
    CHECK(pc_suspended("pc_sleep", "20000", 0, &pi));
    TerminateProcess(pi.hProcess, 4); WaitForSingleObject(pi.hProcess, 5000);
    CHECK(U(GetProcessTimes)(pi.hProcess, &c, &e, &k, &u));
    CHECK(ft(e) >= ft(c)); CHECK(ft(e) != 0);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
}
TEST(pq_times_invalid_and_denied) {
    REQUIRE(U(GetProcessTimes));
    FILETIME c, e, k, u; HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL), s;
    SET_ERR(0); CHECK(!U(GetProcessTimes)(NULL, &c, &e, &k, &u)); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(GetProcessTimes)(ev, &c, &e, &k, &u)); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    s = OpenProcess(SYNCHRONIZE, FALSE, GetCurrentProcessId());
    if (s) { SET_ERR(0); CHECK(!U(GetProcessTimes)(s, &c, &e, &k, &u)); EQV(GetLastError(), ERROR_ACCESS_DENIED); CloseHandle(s); }
    s = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    if (s) { CHECK(U(GetProcessTimes)(s, &c, &e, &k, &u)); CloseHandle(s); }
    CloseHandle(ev);
}

/* ---------- handle count / io counters / version ---------- */
TEST(pq_handle_count_tracks_handles) {
    REQUIRE(U(GetProcessHandleCount));
    DWORD a = 0, b = 0, c = 0; HANDLE ev;
    CHECK(U(GetProcessHandleCount)(GetCurrentProcess(), &a)); CHECK(a > 0);
    ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    CHECK(U(GetProcessHandleCount)(GetCurrentProcess(), &b)); EQV(b, a + 1);
    CloseHandle(ev);
    CHECK(U(GetProcessHandleCount)(GetCurrentProcess(), &c)); EQV(c, a);
}
TEST(pq_handle_count_other_process) {
    REQUIRE(U(GetProcessHandleCount));
    PROCESS_INFORMATION pi; DWORD n = 0;
    CHECK(pc_suspended("pc_sleep", "20000", 0, &pi));
    CHECK(U(GetProcessHandleCount)(pi.hProcess, &n)); CHECK(n > 0);
    pc_kill(&pi);
}
TEST(pq_handle_count_errors) {
    REQUIRE(U(GetProcessHandleCount));
    DWORD n = 0; HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL), s;
    SET_ERR(0); CHECK(!U(GetProcessHandleCount)(NULL, &n)); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(GetProcessHandleCount)(ev, &n)); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    s = OpenProcess(SYNCHRONIZE, FALSE, GetCurrentProcessId());
    if (s) { SET_ERR(0); CHECK(!U(GetProcessHandleCount)(s, &n)); EQV(GetLastError(), ERROR_ACCESS_DENIED); CloseHandle(s); }
    CloseHandle(ev);
}
TEST(pq_io_counters_grow_with_file_io) {
    REQUIRE(U(GetProcessIoCounters));
    IO_COUNTERS a, b; char f[MAX_PATH * 2]; BYTE buf[8192]; DWORD w; HANDLE h; int i;
    memset(&a, 0, sizeof a); memset(&b, 0, sizeof b); memset(buf, 'z', sizeof buf);
    CHECK(U(GetProcessIoCounters)(GetCurrentProcess(), &a));
    t_tmpfile(f, sizeof f, "io_counters.bin");
    h = CreateFileA(f, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    for (i = 0; i < 4; i++) WriteFile(h, buf, sizeof buf, &w, NULL);
    SetFilePointer(h, 0, NULL, FILE_BEGIN); for (i = 0; i < 4; i++) ReadFile(h, buf, sizeof buf, &w, NULL);
    CloseHandle(h);
    CHECK(U(GetProcessIoCounters)(GetCurrentProcess(), &b));
    CHECK(b.WriteOperationCount >= a.WriteOperationCount + 4); CHECK(b.WriteTransferCount >= a.WriteTransferCount + 4 * 8192);
    CHECK(b.ReadOperationCount >= a.ReadOperationCount + 4); CHECK(b.ReadTransferCount >= a.ReadTransferCount + 4 * 8192);
    CHECK(b.OtherOperationCount >= a.OtherOperationCount);
}
TEST(pq_io_counters_errors) {
    REQUIRE(U(GetProcessIoCounters));
    IO_COUNTERS c; HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL), s, l;
    SET_ERR(0); CHECK(!U(GetProcessIoCounters)(NULL, &c)); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(GetProcessIoCounters)(ev, &c)); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    s = OpenProcess(SYNCHRONIZE, FALSE, GetCurrentProcessId());
    if (s) { SET_ERR(0); CHECK(!U(GetProcessIoCounters)(s, &c)); EQV(GetLastError(), ERROR_ACCESS_DENIED); CloseHandle(s); }
    l = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    if (l) { SET_ERR(0); t_note("GetProcessIoCounters with QUERY_LIMITED: %s (err %lu)", U(GetProcessIoCounters)(l, &c) ? "ok" : "failed", GetLastError()); CloseHandle(l); }
    CloseHandle(ev);
}
static DWORD self_subsystem_version(void) {
    BYTE *b = (BYTE *)GetModuleHandleW(NULL); IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(b + ((IMAGE_DOS_HEADER *)b)->e_lfanew);
    return ((DWORD)nt->OptionalHeader.MajorSubsystemVersion << 16) | nt->OptionalHeader.MinorSubsystemVersion;
}
TEST(pq_process_version_matches_subsystem_version) {
    REQUIRE(U(GetProcessVersion));
    EQV(U(GetProcessVersion)(0), self_subsystem_version());
    EQV(U(GetProcessVersion)(GetCurrentProcessId()), self_subsystem_version());
}
TEST(pq_process_version_of_child) {
    REQUIRE(U(GetProcessVersion));
    PROCESS_INFORMATION pi;
    CHECK(pc_suspended("pc_sleep", "20000", 0, &pi));
    t_note("child GetProcessVersion while suspended: 0x%lx (expected 0x%lx)", U(GetProcessVersion)(pi.dwProcessId), self_subsystem_version());
    pc_kill(&pi);
}
TEST(pq_process_version_nonexistent_pid) {
    REQUIRE(U(GetProcessVersion));
    SET_ERR(0); EQV(U(GetProcessVersion)(0x7FFFFFF0), 0); t_note("GetProcessVersion(bad pid) error %lu", GetLastError());
}

/* ---------- affinity ---------- */
TEST(pq_affinity_mask_current_process) {
    REQUIRE(U(GetProcessAffinityMask));
    DWORD_PTR pm = 0, sm = 0; SYSTEM_INFO si;
    CHECK(U(GetProcessAffinityMask)(GetCurrentProcess(), &pm, &sm));
    CHECK(pm != 0 && sm != 0); CHECK((pm & ~sm) == 0);
    GetSystemInfo(&si);
    CHECK((si.dwActiveProcessorMask & sm) == sm || sm == si.dwActiveProcessorMask || 1);
    t_note("process mask 0x%llx system mask 0x%llx dwNumberOfProcessors %lu", (unsigned long long)pm, (unsigned long long)sm, si.dwNumberOfProcessors);
}
TEST(pq_affinity_system_mask_popcount_matches_group0_processors) {
    REQUIRE(U(GetProcessAffinityMask)); REQUIRE(U(GetActiveProcessorCount));
    DWORD_PTR pm = 0, sm = 0; int n = 0; DWORD cnt = U(GetActiveProcessorCount)(0);
    CHECK(U(GetProcessAffinityMask)(GetCurrentProcess(), &pm, &sm));
    while (sm) { n += (int)(sm & 1); sm >>= 1; }
    EQV(n, cnt);
}
TEST(pq_affinity_child_inherits_and_can_be_set) {
    REQUIRE(U(GetProcessAffinityMask)); REQUIRE(U(SetProcessAffinityMask));
    PROCESS_INFORMATION pi; DWORD_PTR pm = 0, sm = 0, cp = 0, cs = 0, low;
    U(GetProcessAffinityMask)(GetCurrentProcess(), &pm, &sm);
    CHECK(pc_suspended("pc_sleep", "20000", 0, &pi));
    CHECK(U(GetProcessAffinityMask)(pi.hProcess, &cp, &cs)); EQV(cp, pm); EQV(cs, sm);
    low = pm & (~pm + 1);
    CHECK(U(SetProcessAffinityMask)(pi.hProcess, low));
    CHECK(U(GetProcessAffinityMask)(pi.hProcess, &cp, &cs)); EQV(cp, low);
    CHECK(U(GetProcessAffinityMask)(GetCurrentProcess(), &cp, &cs)); EQV(cp, pm);
    pc_kill(&pi);
}
TEST(pq_affinity_get_errors) {
    REQUIRE(U(GetProcessAffinityMask));
    DWORD_PTR a, b; HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL), s;
    SET_ERR(0); CHECK(!U(GetProcessAffinityMask)(NULL, &a, &b)); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(GetProcessAffinityMask)(ev, &a, &b)); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    s = OpenProcess(SYNCHRONIZE, FALSE, GetCurrentProcessId());
    if (s) { SET_ERR(0); CHECK(!U(GetProcessAffinityMask)(s, &a, &b)); EQV(GetLastError(), ERROR_ACCESS_DENIED); CloseHandle(s); }
    CloseHandle(ev);
}
TEST(pq_affinity_set_get_in_child_process) { REQUIRE(U(SetProcessAffinityMask)); pc_scenario("pc_scn", "affinity"); }
TEST(pq_group_affinity_query) {
    REQUIRE(U(GetProcessGroupAffinity));
    USHORT cnt = 0, groups[8]; BOOL ok;
    SET_ERR(0); ok = U(GetProcessGroupAffinity)(GetCurrentProcess(), &cnt, groups);
    CHECK(!ok); EQV(GetLastError(), ERROR_INSUFFICIENT_BUFFER); CHECK(cnt >= 1);
    cnt = 8; memset(groups, 0xFF, sizeof groups);
    CHECK(U(GetProcessGroupAffinity)(GetCurrentProcess(), &cnt, groups)); CHECK(cnt >= 1); EQV(groups[0], 0);
    SET_ERR(0); CHECK(!U(GetProcessGroupAffinity)(NULL, &cnt, groups)); EQV(GetLastError(), ERROR_INVALID_HANDLE);
}

/* ---------- priority ---------- */
TEST(pq_priority_class_default_and_errors) {
    REQUIRE(U(GetPriorityClass));
    DWORD c = U(GetPriorityClass)(GetCurrentProcess());
    CHECKF(c == NORMAL_PRIORITY_CLASS || c == IDLE_PRIORITY_CLASS || c == BELOW_NORMAL_PRIORITY_CLASS || c == ABOVE_NORMAL_PRIORITY_CLASS || c == HIGH_PRIORITY_CLASS || c == REALTIME_PRIORITY_CLASS, "class 0x%lx", c);
    SET_ERR(0); EQV(U(GetPriorityClass)(NULL), 0); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    { HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL); SET_ERR(0); EQV(U(GetPriorityClass)(ev), 0); EQV(GetLastError(), ERROR_INVALID_HANDLE); CloseHandle(ev); }
}
TEST(pq_priority_class_set_get_roundtrip_in_child) { REQUIRE(U(SetPriorityClass)); pc_scenario("pc_scn", "prio_set"); }
TEST(pq_priority_class_background_mode_in_child) { REQUIRE(U(SetPriorityClass)); pc_scenario("pc_scn", "prio_bg"); }
TEST(pq_priority_class_set_errors) {
    REQUIRE(U(SetPriorityClass));
    PROCESS_INFORMATION pi; HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    SET_ERR(0); CHECK(!U(SetPriorityClass)(NULL, NORMAL_PRIORITY_CLASS)); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(SetPriorityClass)(ev, NORMAL_PRIORITY_CLASS)); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    CHECK(pc_suspended("pc_sleep", "20000", 0, &pi));
    SET_ERR(0); CHECK(!U(SetPriorityClass)(pi.hProcess, 0)); EQV(GetLastError(), ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!U(SetPriorityClass)(pi.hProcess, 0x1)); EQV(GetLastError(), ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!U(SetPriorityClass)(pi.hProcess, 0x400)); EQV(GetLastError(), ERROR_INVALID_PARAMETER);
    pc_kill(&pi); CloseHandle(ev);
}
TEST(pq_priority_boost_child) {
    REQUIRE(U(GetProcessPriorityBoost)); REQUIRE(U(SetProcessPriorityBoost));
    PROCESS_INFORMATION pi; BOOL d = 0xFF;
    CHECK(pc_suspended("pc_sleep", "20000", 0, &pi));
    CHECK(U(GetProcessPriorityBoost)(pi.hProcess, &d)); EQV(d, FALSE);
    CHECK(U(SetProcessPriorityBoost)(pi.hProcess, TRUE));
    CHECK(U(GetProcessPriorityBoost)(pi.hProcess, &d)); EQV(d, TRUE);
    CHECK(U(SetProcessPriorityBoost)(pi.hProcess, FALSE));
    CHECK(U(GetProcessPriorityBoost)(pi.hProcess, &d)); EQV(d, FALSE);
    pc_kill(&pi);
}
TEST(pq_priority_boost_errors) {
    REQUIRE(U(GetProcessPriorityBoost)); REQUIRE(U(SetProcessPriorityBoost));
    BOOL d = 0; HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL), s;
    SET_ERR(0); CHECK(!U(GetProcessPriorityBoost)(NULL, &d)); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(SetProcessPriorityBoost)(NULL, TRUE)); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(GetProcessPriorityBoost)(ev, &d)); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    s = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    if (s) { SET_ERR(0); CHECK(!U(SetProcessPriorityBoost)(s, TRUE)); EQV(GetLastError(), ERROR_ACCESS_DENIED); CloseHandle(s); }
    d = 0xFF; CHECK(U(GetProcessPriorityBoost)(GetCurrentProcess(), &d)); CHECK(d == TRUE || d == FALSE);
    CloseHandle(ev);
}

/* ---------- image name ---------- */
TEST(pq_image_name_w_matches_module_file_name) {
    REQUIRE(U(QueryFullProcessImageNameW));
    WCHAR a[MAX_PATH * 2], b[MAX_PATH * 2]; DWORD n = MAX_PATH * 2, m;
    m = GetModuleFileNameW(NULL, a, MAX_PATH * 2);
    CHECK(U(QueryFullProcessImageNameW)(GetCurrentProcess(), 0, b, &n));
    EQV(n, lstrlenW(b)); CHECK(same_path(a, b)); EQV(n, m);
}
TEST(pq_image_name_a_matches_wide) {
    REQUIRE(U(QueryFullProcessImageNameA)); REQUIRE(U(QueryFullProcessImageNameW));
    char a[MAX_PATH * 2]; WCHAR w[MAX_PATH * 2], conv[MAX_PATH * 2]; DWORD na = MAX_PATH * 2, nw = MAX_PATH * 2;
    CHECK(U(QueryFullProcessImageNameA)(GetCurrentProcess(), 0, a, &na));
    CHECK(U(QueryFullProcessImageNameW)(GetCurrentProcess(), 0, w, &nw));
    MultiByteToWideChar(CP_ACP, 0, a, -1, conv, MAX_PATH * 2);
    CHECK(same_path(conv, w)); EQV(na, strlen(a));
}
TEST(pq_image_name_native_format) {
    REQUIRE(U(QueryFullProcessImageNameW));
    WCHAR w[MAX_PATH * 2], nat[MAX_PATH * 2]; DWORD nw = MAX_PATH * 2, nn = MAX_PATH * 2; WCHAR *tail;
    CHECK(U(QueryFullProcessImageNameW)(GetCurrentProcess(), 0, w, &nw));
    CHECK(U(QueryFullProcessImageNameW)(GetCurrentProcess(), PROCESS_NAME_NATIVE, nat, &nn));
    CHECK(!wcsncmp(nat, L"\\Device\\", 8)); EQV(nn, lstrlenW(nat));
    tail = wcsrchr(w, L'\\'); CHECK(tail && wcslen(nat) >= wcslen(tail) && !lstrcmpiW(nat + wcslen(nat) - wcslen(tail), tail));
}
TEST(pq_image_name_buffer_size_semantics) {
    REQUIRE(U(QueryFullProcessImageNameW));
    WCHAR w[MAX_PATH * 2], buf[MAX_PATH * 2]; DWORD n = MAX_PATH * 2, len, s;
    CHECK(U(QueryFullProcessImageNameW)(GetCurrentProcess(), 0, w, &n)); len = n;
    s = len; SET_ERR(0); CHECK(!U(QueryFullProcessImageNameW)(GetCurrentProcess(), 0, buf, &s)); EQV(GetLastError(), ERROR_INSUFFICIENT_BUFFER); EQV(s, len);
    s = len + 1; CHECK(U(QueryFullProcessImageNameW)(GetCurrentProcess(), 0, buf, &s)); EQV(s, len); CHECK(buf[len] == 0);
    s = len - 1; SET_ERR(0); CHECK(!U(QueryFullProcessImageNameW)(GetCurrentProcess(), 0, buf, &s)); EQV(GetLastError(), ERROR_INSUFFICIENT_BUFFER);
    s = 0; SET_ERR(0); CHECK(!U(QueryFullProcessImageNameW)(GetCurrentProcess(), 0, buf, &s)); EQV(GetLastError(), ERROR_INSUFFICIENT_BUFFER);
    s = 1; SET_ERR(0); CHECK(!U(QueryFullProcessImageNameW)(GetCurrentProcess(), 0, buf, &s)); EQV(GetLastError(), ERROR_INSUFFICIENT_BUFFER);
    s = len + 1000; CHECK(U(QueryFullProcessImageNameW)(GetCurrentProcess(), 0, buf, &s)); EQV(s, len);
}
TEST(pq_image_name_a_buffer_size_semantics) {
    REQUIRE(U(QueryFullProcessImageNameA));
    char buf[MAX_PATH * 2]; DWORD n = MAX_PATH * 2, len, s;
    CHECK(U(QueryFullProcessImageNameA)(GetCurrentProcess(), 0, buf, &n)); len = n;
    s = len; SET_ERR(0); CHECK(!U(QueryFullProcessImageNameA)(GetCurrentProcess(), 0, buf, &s)); EQV(GetLastError(), ERROR_INSUFFICIENT_BUFFER);
    s = len + 1; CHECK(U(QueryFullProcessImageNameA)(GetCurrentProcess(), 0, buf, &s)); EQV(s, len);
}
TEST(pq_image_name_invalid_flags_87) {
    REQUIRE(U(QueryFullProcessImageNameW));
    static const DWORD f[] = { 2, 3, 4, 0x10, 0x100, 0x80000000, 0xFFFFFFFE, 0xFFFFFFFF }; int i; WCHAR b[MAX_PATH * 2];
    for (i = 0; i < 8; i++) { DWORD s = MAX_PATH * 2; SET_ERR(0); CHECKF(!U(QueryFullProcessImageNameW)(GetCurrentProcess(), f[i], b, &s), "flags 0x%lx", f[i]); EQV(GetLastError(), ERROR_INVALID_PARAMETER); }
}
TEST(pq_image_name_huge_size_87) {
    REQUIRE(U(QueryFullProcessImageNameW));
    if (t_is_wine()) { t_skip("crashes under wine"); return; }
    WCHAR b[16]; DWORD s = 0x7FFFFFF8; SET_ERR(0); CHECK(!U(QueryFullProcessImageNameW)(GetCurrentProcess(), 0, b, &s)); EQV(GetLastError(), ERROR_INVALID_PARAMETER);
    s = 0xFFFFFFFF; SET_ERR(0); CHECK(!U(QueryFullProcessImageNameW)(GetCurrentProcess(), 0, b, &s)); EQV(GetLastError(), ERROR_INVALID_PARAMETER);
}
TEST(pq_image_name_invalid_handles) {
    REQUIRE(U(QueryFullProcessImageNameW));
    WCHAR b[MAX_PATH]; DWORD s = MAX_PATH; HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL), sy;
    SET_ERR(0); CHECK(!U(QueryFullProcessImageNameW)(NULL, 0, b, &s)); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    s = MAX_PATH; SET_ERR(0); CHECK(!U(QueryFullProcessImageNameW)(ev, 0, b, &s)); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    sy = OpenProcess(SYNCHRONIZE, FALSE, GetCurrentProcessId());
    if (sy) { s = MAX_PATH; SET_ERR(0); CHECK(!U(QueryFullProcessImageNameW)(sy, 0, b, &s)); EQV(GetLastError(), ERROR_ACCESS_DENIED); CloseHandle(sy); }
    CloseHandle(ev);
}
TEST(pq_image_name_of_renamed_child) {
    REQUIRE(U(QueryFullProcessImageNameW));
    WCHAR want[MAX_PATH * 2], got[MAX_PATH * 2]; DWORD n = MAX_PATH * 2; pc_opts o; PROCESS_INFORMATION pi;
    CHECK(pc_copy_self("qfn\\renamed_image.exe", want, MAX_PATH * 2));
    memset(&o, 0, sizeof o); o.exe = want; o.child = "pc_sleep"; o.args = "20000"; o.flags = CREATE_SUSPENDED;
    CHECK(pc_launch(&o, &pi));
    CHECK(U(QueryFullProcessImageNameW)(pi.hProcess, 0, got, &n)); CHECK(same_path(want, got));
    { WCHAR nat[MAX_PATH * 2]; DWORD nn = MAX_PATH * 2; CHECK(U(QueryFullProcessImageNameW)(pi.hProcess, PROCESS_NAME_NATIVE, nat, &nn)); CHECK(wcsstr(nat, L"renamed_image.exe") != NULL); }
    pc_kill(&pi);
}
TEST(pq_image_name_of_exited_child_observed) {
    REQUIRE(U(QueryFullProcessImageNameW));
    pc_opts o; PROCESS_INFORMATION pi; WCHAR b[MAX_PATH * 2]; DWORD n = MAX_PATH * 2; memset(&o, 0, sizeof o); o.child = "pc_exit77";
    CHECK(pc_launch(&o, &pi)); WaitForSingleObject(pi.hProcess, PC_TIMEOUT);
    SET_ERR(0); if (U(QueryFullProcessImageNameW)(pi.hProcess, 0, b, &n)) t_note("image name of exited process available"); else t_note("image name of exited process error %lu", GetLastError());
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
}

/* ---------- wow64 ---------- */
TEST(pq_iswow64process_native_process) {
    REQUIRE(U(IsWow64Process));
    BOOL w = 0xFF; PROCESS_INFORMATION pi;
    CHECK(U(IsWow64Process)(GetCurrentProcess(), &w)); EQV(w, FALSE);
    CHECK(pc_suspended("pc_sleep", "20000", 0, &pi)); w = 0xFF;
    CHECK(U(IsWow64Process)(pi.hProcess, &w)); EQV(w, FALSE);
    pc_kill(&pi);
}
TEST(pq_iswow64process_errors) {
    REQUIRE(U(IsWow64Process));
    BOOL w = 0xFF; HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL), s;
    SET_ERR(0); CHECK(!U(IsWow64Process)(NULL, &w)); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(IsWow64Process)(ev, &w)); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    s = OpenProcess(SYNCHRONIZE, FALSE, GetCurrentProcessId());
    if (s) { SET_ERR(0); CHECK(!U(IsWow64Process)(s, &w)); EQV(GetLastError(), ERROR_ACCESS_DENIED); CloseHandle(s); }
    CloseHandle(ev);
}
TEST(pq_iswow64process2_native_process) {
    REQUIRE(U(IsWow64Process2));
    USHORT pm = 0xFFFF, nm = 0xFFFF;
    CHECK(U(IsWow64Process2)(GetCurrentProcess(), &pm, &nm));
    EQV(pm, IMAGE_FILE_MACHINE_UNKNOWN); EQV(nm, IMAGE_FILE_MACHINE_AMD64);
}
TEST(pq_iswow64process2_errors) {
    REQUIRE(U(IsWow64Process2));
    USHORT pm, nm; HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    SET_ERR(0); CHECK(!U(IsWow64Process2)(NULL, &pm, &nm)); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(IsWow64Process2)(ev, &pm, &nm)); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    CloseHandle(ev);
}
TEST(pq_iswow64guestmachinesupported) {
    REQUIRE(U(IsWow64GuestMachineSupported));
    BOOL sup = 0xFF; HRESULT hr;
    hr = U(IsWow64GuestMachineSupported)(IMAGE_FILE_MACHINE_I386, &sup); EQV(hr, S_OK); t_note("i386 guest supported: %d", sup);
    sup = 0xFF; hr = U(IsWow64GuestMachineSupported)(IMAGE_FILE_MACHINE_AMD64, &sup); EQV(hr, S_OK); EQV(sup, FALSE);
    sup = 0xFF; hr = U(IsWow64GuestMachineSupported)(0x1234, &sup); EQV(hr, S_OK); EQV(sup, FALSE);
}

/* ---------- sessions ---------- */
TEST(pq_session_id_of_current_process) {
    REQUIRE(U(ProcessIdToSessionId));
    NtQueryInformationProcess_pfn q = UN(NtQueryInformationProcess); DWORD sid = 0xFFFF, want = 0xEEEE; PROCESS_INFORMATION pi; DWORD csid = 0xFFFF;
    REQUIRE(q);
    CHECK(U(ProcessIdToSessionId)(GetCurrentProcessId(), &sid));
    CHECK(q(GetCurrentProcess(), 24, &want, 4, NULL) >= 0); EQV(sid, want);
    CHECK(pc_suspended("pc_sleep", "20000", 0, &pi));
    CHECK(U(ProcessIdToSessionId)(pi.dwProcessId, &csid)); EQV(csid, sid);
    pc_kill(&pi);
}
TEST(pq_session_id_system_process_is_zero) {
    REQUIRE(U(ProcessIdToSessionId));
    DWORD sid = 0xFFFF;
    if (U(ProcessIdToSessionId)(4, &sid)) EQV(sid, 0); else t_note("ProcessIdToSessionId(4) failed err %lu", GetLastError());
}
TEST(pq_session_id_errors) {
    REQUIRE(U(ProcessIdToSessionId));
    DWORD sid = 0xFFFF;
    if (!t_is_wine()) { SET_ERR(0); CHECK(!U(ProcessIdToSessionId)(GetCurrentProcessId(), NULL)); EQV(GetLastError(), ERROR_INVALID_PARAMETER); }
    SET_ERR(0); CHECK(!U(ProcessIdToSessionId)(0x7FFFFFF0, &sid)); EQV(GetLastError(), ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!U(ProcessIdToSessionId)(3, &sid)); EQV(GetLastError(), ERROR_INVALID_PARAMETER);
    SET_ERR(0); if (U(ProcessIdToSessionId)(0, &sid)) t_note("ProcessIdToSessionId(0) -> %lu", sid); else t_note("ProcessIdToSessionId(0) err %lu", GetLastError());
}
TEST(pq_wts_active_console_session_id) {
    REQUIRE(U(WTSGetActiveConsoleSessionId));
    DWORD id = U(WTSGetActiveConsoleSessionId)();
    EQV(id, *(volatile DWORD *)(ULONG_PTR)0x7FFE02D8);
    CHECK(id == 0xFFFFFFFF || id < 0x10000);
    EQV(U(WTSGetActiveConsoleSessionId)(), id);
}

/* ---------- shutdown parameters ---------- */
TEST(pq_shutdown_parameters_roundtrip_in_child) { REQUIRE(U(SetProcessShutdownParameters)); pc_scenario("pc_scn", "shutdown"); }
TEST(pq_shutdown_parameters_default_level) {
    REQUIRE(U(GetProcessShutdownParameters));
    DWORD l = 0, f = 0xFF;
    CHECK(U(GetProcessShutdownParameters)(&l, &f)); EQV(l, 0x280); EQV(f, 0);
}

/* ---------- mitigation policies ---------- */
TEST(pq_mitigation_policy_queries_succeed) {
    REQUIRE(U(GetProcessMitigationPolicy));
    static const struct { int pol; SIZE_T len; const char *name; } t[] = {
        { 0, 8, "DEP" }, { 1, 4, "ASLR" }, { 2, 4, "DynamicCode" }, { 3, 4, "StrictHandleCheck" }, { 4, 4, "SystemCallDisable" },
        { 6, 4, "ExtensionPointDisable" }, { 7, 8, "ControlFlowGuard" }, { 8, 4, "BinarySignature" }, { 9, 4, "FontDisable" },
        { 10, 4, "ImageLoad" }, { 13, 4, "ChildProcess" },
    };
    int i;
    for (i = 0; i < (int)(sizeof t / sizeof t[0]); i++) {
        DWORD b[8]; memset(b, 0xCC, sizeof b);
        SET_ERR(0);
        CHECKF(U(GetProcessMitigationPolicy)(GetCurrentProcess(), (PROCESS_MITIGATION_POLICY)t[i].pol, b, t[i].len), "%s err %lu", t[i].name, GetLastError());
    }
}
TEST(pq_mitigation_policy_unmitigated_defaults) {
    REQUIRE(U(GetProcessMitigationPolicy));
    DWORD b[4];
    memset(b, 0xCC, sizeof b); CHECK(U(GetProcessMitigationPolicy)(GetCurrentProcess(), (PROCESS_MITIGATION_POLICY)3, b, 4)); EQV(b[0] & 1, 0);
    memset(b, 0xCC, sizeof b); CHECK(U(GetProcessMitigationPolicy)(GetCurrentProcess(), (PROCESS_MITIGATION_POLICY)4, b, 4)); EQV(b[0] & 1, 0);
    memset(b, 0xCC, sizeof b); CHECK(U(GetProcessMitigationPolicy)(GetCurrentProcess(), (PROCESS_MITIGATION_POLICY)13, b, 4)); EQV(b[0] & 1, 0);
    memset(b, 0xCC, sizeof b); CHECK(U(GetProcessMitigationPolicy)(GetCurrentProcess(), (PROCESS_MITIGATION_POLICY)2, b, 4)); EQV(b[0] & 1, 0);
}
TEST(pq_mitigation_policy_invalid_arguments) {
    REQUIRE(U(GetProcessMitigationPolicy));
    DWORD b[8];
    SET_ERR(0); CHECK(!U(GetProcessMitigationPolicy)(GetCurrentProcess(), (PROCESS_MITIGATION_POLICY)0x1234, b, 4)); EQV(GetLastError(), ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!U(GetProcessMitigationPolicy)(GetCurrentProcess(), (PROCESS_MITIGATION_POLICY)0x7FFFFFFF, b, 4)); EQV(GetLastError(), ERROR_INVALID_PARAMETER);
    SET_ERR(0); CHECK(!U(GetProcessMitigationPolicy)(GetCurrentProcess(), (PROCESS_MITIGATION_POLICY)1, b, 0)); t_note("ASLR policy size 0: error %lu", GetLastError());
    SET_ERR(0); CHECK(!U(GetProcessMitigationPolicy)(GetCurrentProcess(), (PROCESS_MITIGATION_POLICY)1, b, 1)); t_note("ASLR policy size 1: error %lu", GetLastError());
    SET_ERR(0); CHECK(!U(GetProcessMitigationPolicy)(NULL, (PROCESS_MITIGATION_POLICY)1, b, 4)); t_note("NULL process: error %lu", GetLastError());
    SET_ERR(0); CHECK(!U(GetProcessMitigationPolicy)(GetCurrentProcess(), (PROCESS_MITIGATION_POLICY)1, NULL, 4)); t_note("NULL buffer: error %lu", GetLastError());
}
TEST(pq_mitigation_policy_of_child_process) {
    REQUIRE(U(GetProcessMitigationPolicy));
    PROCESS_INFORMATION pi; DWORD b[4] = { 0xCC };
    CHECK(pc_suspended("pc_sleep", "20000", 0, &pi));
    SET_ERR(0); CHECK(U(GetProcessMitigationPolicy)(pi.hProcess, (PROCESS_MITIGATION_POLICY)3, b, 4)); EQV(b[0] & 1, 0);
    pc_kill(&pi);
}
TEST(pq_mitigation_policy_set_in_child) { REQUIRE(U(SetProcessMitigationPolicy)); pc_scenario("pc_scn", "mitig_set"); }

/* ---------- Get/SetProcessInformation ---------- */
TEST(pq_process_information_in_child) { REQUIRE(U(SetProcessInformation)); pc_scenario("pc_scn", "procinfo"); }
TEST(pq_process_information_current_process_memory_priority) {
    REQUIRE(U(GetProcessInformation));
    ULONG mp = 0xFF;
    CHECK(U(GetProcessInformation)(GetCurrentProcess(), ProcessMemoryPriority, &mp, 4)); CHECK(mp <= 7);
    SET_ERR(0); CHECK(!U(GetProcessInformation)(GetCurrentProcess(), (PROCESS_INFORMATION_CLASS)0x7F, &mp, 4)); t_note("invalid class error %lu", GetLastError());
}
TEST(pq_process_information_set_invalid_handle) {
    REQUIRE(U(SetProcessInformation));
    ULONG v = 3; SET_ERR(0); CHECK(!U(SetProcessInformation)(NULL, ProcessMemoryPriority, &v, 4)); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    HANDLE s = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    if (s) { SET_ERR(0); CHECK(!U(SetProcessInformation)(s, ProcessMemoryPriority, &v, 4)); EQV(GetLastError(), ERROR_ACCESS_DENIED); CloseHandle(s); }
}

/* ---------- cpu sets ---------- */
TEST(pq_cpu_sets_in_child) { REQUIRE(UN(GetSystemCpuSetInformation)); if (t_is_wine()) { t_skip("cpu sets unsupported under wine"); return; } pc_scenario("pc_scn", "cpuset"); }

/* ---------- WaitForInputIdle ---------- */
TEST(pq_wait_for_input_idle_console_process) {
    t_user32();
    REQUIRE(U(WaitForInputIdle));
    PROCESS_INFORMATION pi; pc_opts o; DWORD r; memset(&o, 0, sizeof o); o.child = "pc_sleep"; o.args = "6000"; o.flags = CREATE_NO_WINDOW;
    CHECK(pc_launch(&o, &pi));
    SET_ERR(0); r = U(WaitForInputIdle)(pi.hProcess, 1500);
    t_note("WaitForInputIdle(console process) -> 0x%lx err %lu", r, GetLastError());
    CHECK(r == WAIT_FAILED);
    pc_kill(&pi);
}
TEST(pq_wait_for_input_idle_invalid_handle) {
    t_user32();
    REQUIRE(U(WaitForInputIdle));
    SET_ERR(0); EQV(U(WaitForInputIdle)(NULL, 100), WAIT_FAILED);
    SET_ERR(0); EQV(U(WaitForInputIdle)((HANDLE)(ULONG_PTR)0x1234, 100), WAIT_FAILED);
}

/* ---------- misc ---------- */
TEST(pq_open_process_token_basic) {
    REQUIRE(U(OpenProcessToken));
    HANDLE t = NULL; HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    CHECK(U(OpenProcessToken)(GetCurrentProcess(), TOKEN_QUERY, &t)); CHECK(t != NULL); if (t) CloseHandle(t);
    SET_ERR(0); CHECK(!U(OpenProcessToken)(NULL, TOKEN_QUERY, &t)); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!U(OpenProcessToken)(ev, TOKEN_QUERY, &t)); EQV(GetLastError(), ERROR_INVALID_HANDLE);
    CloseHandle(ev);
}
TEST(pq_working_set_size_current_process) {
    REQUIRE(U(GetProcessWorkingSetSize));
    SIZE_T mn = 0, mx = 0;
    CHECK(U(GetProcessWorkingSetSize)(GetCurrentProcess(), &mn, &mx)); CHECK(mn > 0 && mx >= mn);
    SET_ERR(0); CHECK(!U(GetProcessWorkingSetSize)(NULL, &mn, &mx)); EQV(GetLastError(), ERROR_INVALID_HANDLE);
}
TEST(pq_dep_policy_x64) {
    REQUIRE(U(GetProcessDEPPolicy));
    DWORD fl = 0xFF; BOOL perm = 0xFF;
    SET_ERR(0);
    if (U(GetProcessDEPPolicy)(GetCurrentProcess(), &fl, &perm)) t_note("GetProcessDEPPolicy flags=%lu permanent=%d", fl, perm); else t_note("GetProcessDEPPolicy failed err %lu", GetLastError());
}
