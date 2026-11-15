/* Exited own child whose process handle (and initial thread handle) is still open. */
#include "xpt.h"

#define DEADP(pi) PROCESS_INFORMATION pi; if (!xpt_spawn_dead(&pi)) { CHECK(!"child"); return; }

static void *kernel32_exitthread(void) { return (void *)GetProcAddress(GetModuleHandleA("kernel32.dll"), "ExitThread"); }

TEST(xpt_zp_signaled_and_exit_code) {
    DEADP(pi);
    EQ(U(WaitForSingleObject)(pi.hProcess, 0), WAIT_OBJECT_0);
    DWORD c = 0; SUCCEEDS(U(GetExitCodeProcess)(pi.hProcess, &c)); EQ(c, XPT_CHILD_EXIT);
    xpt_kill(&pi);
}
TEST(xpt_zp_initial_thread_handle_after_exit) {
    DEADP(pi);
    EQ(WaitForSingleObject(pi.hThread, 0), WAIT_OBJECT_0);
    DWORD c = 0; SUCCEEDS(U(GetExitCodeThread)(pi.hThread, &c)); EQ(c, XPT_CHILD_EXIT);
    EQ(U(GetThreadId)(pi.hThread), pi.dwThreadId);
    EQ(U(GetProcessIdOfThread)(pi.hThread), pi.dwProcessId);
    SetLastError(0); EQ(U(SuspendThread)(pi.hThread), (DWORD)-1); EXPECT_ERR(ERROR_ACCESS_DENIED);
    xpt_kill(&pi);
}
TEST(xpt_zp_getpriorityclass) {
    DEADP(pi);
    EQ(U(GetPriorityClass)(pi.hProcess), NORMAL_PRIORITY_CLASS);
    xpt_kill(&pi);
}
TEST(xpt_zp_setpriorityclass) {
    DEADP(pi);
    /* not refused on an exited process: the class is stored and reads back */
    SUCCEEDS(U(SetPriorityClass)(pi.hProcess, BELOW_NORMAL_PRIORITY_CLASS));
    EQ(GetPriorityClass(pi.hProcess), BELOW_NORMAL_PRIORITY_CLASS);
    SUCCEEDS(U(SetPriorityClass)(pi.hProcess, IDLE_PRIORITY_CLASS));
    EQ(GetPriorityClass(pi.hProcess), IDLE_PRIORITY_CLASS);
    xpt_kill(&pi);
}
TEST(xpt_zp_getprocessaffinitymask) {
    DEADP(pi);
    DWORD_PTR a = 0, s = 0, ma, ms;
    SUCCEEDS(U(GetProcessAffinityMask)(pi.hProcess, &a, &s));
    GetProcessAffinityMask(GetCurrentProcess(), &ma, &ms);
    EQ(a, ma); EQ(s, ms);
    xpt_kill(&pi);
}
TEST(xpt_zp_setprocessaffinitymask) {
    DEADP(pi);
    DWORD_PTR a, s; GetProcessAffinityMask(GetCurrentProcess(), &a, &s);
    FAILS(U(SetProcessAffinityMask)(pi.hProcess, a), ERROR_ACCESS_DENIED);
    xpt_kill(&pi);
}
TEST(xpt_zp_getprocesstimes_exit_set_frozen) {
    DEADP(pi);
    FILETIME cr, ex, k, u, cr2, ex2, k2, u2;
    SUCCEEDS(U(GetProcessTimes)(pi.hProcess, &cr, &ex, &k, &u));
    ULONGLONG c = ((ULONGLONG)cr.dwHighDateTime << 32) | cr.dwLowDateTime, e = ((ULONGLONG)ex.dwHighDateTime << 32) | ex.dwLowDateTime;
    NE(c, 0); NE(e, 0); CHECK(e >= c);
    Sleep(30);
    SUCCEEDS(U(GetProcessTimes)(pi.hProcess, &cr2, &ex2, &k2, &u2));
    CHECK(!memcmp(&cr, &cr2, sizeof cr) && !memcmp(&ex, &ex2, sizeof ex) && !memcmp(&k, &k2, sizeof k) && !memcmp(&u, &u2, sizeof u));
    xpt_kill(&pi);
}
TEST(xpt_zp_queryprocesscycletime_frozen) {
    DEADP(pi);
    ULONG64 a = 0, b = 1;
    SUCCEEDS(U(QueryProcessCycleTime)(pi.hProcess, &a));
    NE(a, 0);
    Sleep(20);
    SUCCEEDS(U(QueryProcessCycleTime)(pi.hProcess, &b));
    EQ(a, b);
    xpt_kill(&pi);
}
TEST(xpt_zp_getprocesshandlecount) {
    DEADP(pi);
    DWORD n = 0xCCCC;
    SUCCEEDS(U(GetProcessHandleCount)(pi.hProcess, &n));
    EQ(n, 0);   /* the handle table is gone */
    xpt_kill(&pi);
}
TEST(xpt_zp_getprocessiocounters_frozen) {
    DEADP(pi);
    IO_COUNTERS a, b; memset(&a, 0xCC, sizeof a); memset(&b, 0x33, sizeof b);
    SUCCEEDS(U(GetProcessIoCounters)(pi.hProcess, &a));
    Sleep(20);
    SUCCEEDS(U(GetProcessIoCounters)(pi.hProcess, &b));
    CHECK(!memcmp(&a, &b, sizeof a));
    NE(a.ReadOperationCount + a.OtherOperationCount, 0);   /* the loader read the image */
    xpt_kill(&pi);
}
/* The Win32 form of the image name is no longer available once the process has exited
 * (STATUS_UNSUCCESSFUL); the size in/out parameter is left alone. */
TEST(xpt_zp_queryfullprocessimagenamew) {
    DEADP(pi);
    WCHAR b[MAX_PATH * 2]; DWORD n = MAX_PATH * 2;
    FAILS(U(QueryFullProcessImageNameW)(pi.hProcess, 0, b, &n), ERROR_GEN_FAILURE);
    EQ(n, MAX_PATH * 2);
    xpt_kill(&pi);
}
TEST(xpt_zp_queryfullprocessimagenamew_live_matches_self) {
    PROCESS_INFORMATION pi; HANDLE hp = xpt_spawn_sleeper(&pi);
    CHECK(hp != NULL); if (!hp) return;
    WCHAR b[MAX_PATH * 2], me[MAX_PATH * 2]; DWORD n = MAX_PATH * 2;
    SUCCEEDS(U(QueryFullProcessImageNameW)(hp, 0, b, &n));
    DWORD m = GetModuleFileNameW(NULL, me, MAX_PATH * 2);
    EQ(n, m);
    CHECK(!lstrcmpiW(b, me));
    xpt_kill(&pi);
}
TEST(xpt_zp_queryfullprocessimagenamew_native) {
    DEADP(pi);
    WCHAR b[MAX_PATH * 2]; DWORD n = MAX_PATH * 2;
    SUCCEEDS(U(QueryFullProcessImageNameW)(pi.hProcess, PROCESS_NAME_NATIVE, b, &n));
    CHECK(n > 8 && !wcsncmp(b, L"\\Device\\", 8));
    EQ(n, lstrlenW(b));
    xpt_kill(&pi);
}
TEST(xpt_zp_queryfullprocessimagenamew_small_buffer) {
    DEADP(pi);
    WCHAR b[4]; DWORD n = 4;
    FAILS(U(QueryFullProcessImageNameW)(pi.hProcess, 0, b, &n), ERROR_GEN_FAILURE);   /* state is checked before the size */
    n = 4;
    FAILS(U(QueryFullProcessImageNameW)(pi.hProcess, PROCESS_NAME_NATIVE, b, &n), ERROR_INSUFFICIENT_BUFFER);
    EQ(n, 4);
    xpt_kill(&pi);
}
TEST(xpt_zp_getprocessinformation_memprio) {
    DEADP(pi);
    MEMORY_PRIORITY_INFORMATION m = { 0xCC };
    SUCCEEDS(U(GetProcessInformation)(pi.hProcess, ProcessMemoryPriority, &m, sizeof m));
    EQ(m.MemoryPriority, MEMORY_PRIORITY_NORMAL);
    xpt_kill(&pi);
}
TEST(xpt_zp_getprocessinformation_protection) {
    DEADP(pi);
    PROCESS_PROTECTION_LEVEL_INFORMATION p = { 0xCC };
    SUCCEEDS(U(GetProcessInformation)(pi.hProcess, ProcessProtectionLevelInfo, &p, sizeof p));
    EQ(p.ProtectionLevel, PROTECTION_LEVEL_NONE);
    xpt_kill(&pi);
}
TEST(xpt_zp_getprocessinformation_appmemory) {
    DEADP(pi);
    APP_MEMORY_INFORMATION a; memset(&a, 0xCC, sizeof a);
    SUCCEEDS(U(GetProcessInformation)(pi.hProcess, ProcessAppMemoryInfo, &a, sizeof a));
    CHECK(a.PeakPrivateCommitUsage != 0xCCCCCCCCCCCCCCCCull && a.PeakPrivateCommitUsage >= a.PrivateCommitUsage);
    xpt_kill(&pi);
}
TEST(xpt_zp_setprocessinformation_memprio) {
    DEADP(pi);
    MEMORY_PRIORITY_INFORMATION m = { MEMORY_PRIORITY_LOW };
    FAILS(U(SetProcessInformation)(pi.hProcess, ProcessMemoryPriority, &m, sizeof m), ERROR_ACCESS_DENIED);
    xpt_kill(&pi);
}
TEST(xpt_zp_priority_boost_get_set) {
    DEADP(pi);
    BOOL b = 7;
    SUCCEEDS(U(GetProcessPriorityBoost)(pi.hProcess, &b));
    EQ(b, FALSE);
    FAILS(U(SetProcessPriorityBoost)(pi.hProcess, TRUE), ERROR_ACCESS_DENIED);
    xpt_kill(&pi);
}
TEST(xpt_zp_ids_and_wow64) {
    DEADP(pi);
    EQ(U(GetProcessId)(pi.hProcess), pi.dwProcessId);
    if (UN(IsWow64Process2)) {
        USHORT pm = 0xCCCC, nm = 0xCCCC;
        SUCCEEDS(UN(IsWow64Process2)(pi.hProcess, &pm, &nm));
        EQ(pm, IMAGE_FILE_MACHINE_UNKNOWN);
        EQ(nm, IMAGE_FILE_MACHINE_AMD64);
    }
    xpt_kill(&pi);
}
TEST(xpt_zp_processidtosessionid_while_held) {
    DEADP(pi);
    DWORD s = 0xCCCC, me = 0xDDDD;
    SUCCEEDS(U(ProcessIdToSessionId)(pi.dwProcessId, &s));
    ProcessIdToSessionId(GetCurrentProcessId(), &me);
    EQ(s, me);
    xpt_kill(&pi);
}
TEST(xpt_zp_createremotethreadex_fails) {
    DEADP(pi);
    DWORD tid = 0x1234;
    FAILS(U(CreateRemoteThreadEx)(pi.hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)kernel32_exitthread(), NULL, 0, NULL, &tid), ERROR_ACCESS_DENIED);
    xpt_kill(&pi);
}
TEST(xpt_zp_virtualallocex_fails) {
    DEADP(pi);
    FAILS(U(VirtualAllocEx)(pi.hProcess, NULL, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE), ERROR_ACCESS_DENIED);
    xpt_kill(&pi);
}
TEST(xpt_zp_readprocessmemory_fails) {
    DEADP(pi);
    char b[2] = { 7, 7 }; SIZE_T n = 0x55;
    FAILS(U(ReadProcessMemory)(pi.hProcess, GetModuleHandleA("kernel32.dll"), b, 2, &n), ERROR_PARTIAL_COPY);
    EQ(n, 0);
    xpt_kill(&pi);
}
TEST(xpt_zp_terminate_again_keeps_code) {
    DEADP(pi);
    SetLastError(0);
    BOOL r = U(TerminateProcess)(pi.hProcess, 0x99);
    DWORD e = GetLastError();
    CHECKF(!r && e == ERROR_ACCESS_DENIED, "r=%d err=%lu", r, e);
    DWORD c = 0; GetExitCodeProcess(pi.hProcess, &c); EQ(c, XPT_CHILD_EXIT);
    xpt_kill(&pi);
}
TEST(xpt_zp_duplicatehandle_into_exited_target) {
    DEADP(pi);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL), out = (HANDLE)0x1234;
    FAILS(U(DuplicateHandle)(GetCurrentProcess(), ev, pi.hProcess, &out, 0, FALSE, DUPLICATE_SAME_ACCESS), ERROR_ACCESS_DENIED);
    CloseHandle(ev);
    xpt_kill(&pi);
}
TEST(xpt_zp_openprocess_by_id_queries) {
    DEADP(pi);
    HANDLE q = U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pi.dwProcessId);
    CHECKF(q != NULL, "err=%lu", GetLastError());
    if (q) {
        EQ(WaitForSingleObject(q, 0), WAIT_OBJECT_0);
        DWORD c = 0; SUCCEEDS(U(GetExitCodeProcess)(q, &c)); EQ(c, XPT_CHILD_EXIT);
        EQ(U(GetPriorityClass)(q), NORMAL_PRIORITY_CLASS);
        FILETIME a, b, d, e; SUCCEEDS(U(GetProcessTimes)(q, &a, &b, &d, &e));
        WCHAR nm[MAX_PATH * 2]; DWORD n = MAX_PATH * 2; FAILS(U(QueryFullProcessImageNameW)(q, 0, nm, &n), ERROR_GEN_FAILURE);
        n = MAX_PATH * 2; SUCCEEDS(U(QueryFullProcessImageNameW)(q, PROCESS_NAME_NATIVE, nm, &n));
        CloseHandle(q);
    }
    xpt_kill(&pi);
}
/* A multi-threaded child: ExitProcess ends every thread; the process exit code is the ExitProcess code. */
TEST(xpt_zp_multithreaded_exitprocess_code) {
    PROCESS_INFORMATION pi;
    HANDLE hp = t_spawn_child("xpt_quick_mt", NULL, 0, NULL, NULL, NULL, FALSE, NULL, &pi);
    CHECK(hp != NULL); if (!hp) return;
    EQ(WaitForSingleObject(hp, 15000), WAIT_OBJECT_0);
    DWORD c = 0; GetExitCodeProcess(hp, &c); EQ(c, 0x2B);
    GetExitCodeThread(pi.hThread, &c); EQ(c, 0x2B);
    xpt_kill(&pi);
}
/* A child killed by TerminateProcess has its exit time set and the given code. */
TEST(xpt_zp_terminated_child_state) {
    PROCESS_INFORMATION pi; HANDLE hp = xpt_spawn_sleeper(&pi);
    CHECK(hp != NULL); if (!hp) return;
    SUCCEEDS(U(TerminateProcess)(hp, 0xC1));
    EQ(WaitForSingleObject(hp, 5000), WAIT_OBJECT_0);
    DWORD c = 0; GetExitCodeProcess(hp, &c); EQ(c, 0xC1);
    FILETIME a, b, d, e; SUCCEEDS(U(GetProcessTimes)(hp, &a, &b, &d, &e));
    NE(b.dwLowDateTime | b.dwHighDateTime, 0);
    EQ(GetPriorityClass(hp), NORMAL_PRIORITY_CLASS);
    FAILS(U(VirtualAllocEx)(hp, NULL, 4096, MEM_RESERVE, PAGE_NOACCESS), ERROR_ACCESS_DENIED);
    xpt_kill(&pi);
}
/* A live child's exit time is zero and its exit code reads STILL_ACTIVE. */
TEST(xpt_zp_live_child_baseline) {
    PROCESS_INFORMATION pi; HANDLE hp = xpt_spawn_sleeper(&pi);
    CHECK(hp != NULL); if (!hp) return;
    FILETIME a, b, d, e; SUCCEEDS(U(GetProcessTimes)(hp, &a, &b, &d, &e));
    EQ(b.dwLowDateTime | b.dwHighDateTime, 0);
    DWORD c = 0; GetExitCodeProcess(hp, &c); EQ(c, STILL_ACTIVE);
    DWORD n = 0; SUCCEEDS(U(GetProcessHandleCount)(hp, &n)); NE(n, 0);
    xpt_kill(&pi);
}
