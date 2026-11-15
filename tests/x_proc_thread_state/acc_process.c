/* Access-mask enforcement on process handles: every API with a handle opened by OpenProcess with exactly
 * one access right. The target is always an own sleeping child; settings applied are its current values. */
#include "xpt.h"

typedef struct { PROCESS_INFORMATION pi; HANDLE full; DWORD pid; DWORD_PTR mask; void *rw; } pctx;
typedef int (*pop)(HANDLE h, pctx *c);

static int op_getpc(HANDLE h, pctx *c) { return XB(U(GetPriorityClass)(h)); }
static int op_setpc(HANDLE h, pctx *c) { return XB(U(SetPriorityClass)(h, NORMAL_PRIORITY_CLASS)); }
static int op_getaff(HANDLE h, pctx *c) { DWORD_PTR a, s; return XB(U(GetProcessAffinityMask)(h, &a, &s)); }
static int op_setaff(HANDLE h, pctx *c) { return XB(U(SetProcessAffinityMask)(h, c->mask)); }
static int op_times(HANDLE h, pctx *c) { FILETIME a, b, d, e; return XB(U(GetProcessTimes)(h, &a, &b, &d, &e)); }
static int op_cycle(HANDLE h, pctx *c) { ULONG64 u; return XB(U(QueryProcessCycleTime)(h, &u)); }
static int op_hcount(HANDLE h, pctx *c) { DWORD n; return XB(U(GetProcessHandleCount)(h, &n)); }
static int op_io(HANDLE h, pctx *c) { IO_COUNTERS io; return XB(U(GetProcessIoCounters)(h, &io)); }
static int op_getboost(HANDLE h, pctx *c) { BOOL b; return XB(U(GetProcessPriorityBoost)(h, &b)); }
static int op_setboost(HANDLE h, pctx *c) { return XB(U(SetProcessPriorityBoost)(h, FALSE)); }
static int op_image(HANDLE h, pctx *c) { WCHAR b[MAX_PATH * 2]; DWORD n = MAX_PATH * 2; return XB(U(QueryFullProcessImageNameW)(h, 0, b, &n)); }
static int op_image_native(HANDLE h, pctx *c) { WCHAR b[MAX_PATH * 2]; DWORD n = MAX_PATH * 2; return XB(U(QueryFullProcessImageNameW)(h, PROCESS_NAME_NATIVE, b, &n)); }
static int op_getmem(HANDLE h, pctx *c) { MEMORY_PRIORITY_INFORMATION m; return XB(U(GetProcessInformation)(h, ProcessMemoryPriority, &m, sizeof m)); }
static int op_setmem(HANDLE h, pctx *c) { MEMORY_PRIORITY_INFORMATION m = { MEMORY_PRIORITY_NORMAL }; return XB(U(SetProcessInformation)(h, ProcessMemoryPriority, &m, sizeof m)); }
static int op_getprot(HANDLE h, pctx *c) { PROCESS_PROTECTION_LEVEL_INFORMATION p; return XB(U(GetProcessInformation)(h, ProcessProtectionLevelInfo, &p, sizeof p)); }
static int op_getappmem(HANDLE h, pctx *c) { APP_MEMORY_INFORMATION a; return XB(U(GetProcessInformation)(h, ProcessAppMemoryInfo, &a, sizeof a)); }
static int op_setthr(HANDLE h, pctx *c) { PROCESS_POWER_THROTTLING_STATE s = { PROCESS_POWER_THROTTLING_CURRENT_VERSION, 0, 0 }; return XB(U(SetProcessInformation)(h, ProcessPowerThrottling, &s, sizeof s)); }
static int op_exitcode(HANDLE h, pctx *c) { DWORD x; return XB(U(GetExitCodeProcess)(h, &x)); }
static int op_pid(HANDLE h, pctx *c) { return XB(U(GetProcessId)(h)); }
static int op_wow2(HANDLE h, pctx *c) { USHORT a, b; return XB(UN(IsWow64Process2)(h, &a, &b)); }
static int op_valloc(HANDLE h, pctx *c) {
    void *p = U(VirtualAllocEx)(h, NULL, 4096, MEM_RESERVE, PAGE_NOACCESS);
    int ok = xpt_b(p != NULL);
    if (p) VirtualFreeEx(c->full, p, 0, MEM_RELEASE);
    return ok;
}
static int op_rpm(HANDLE h, pctx *c) {
    char b[2] = { 0, 0 }; SIZE_T n = 0;
    SetLastError(0xDEADBEEF);
    int ok = xpt_b(U(ReadProcessMemory)(h, GetModuleHandleA("kernel32.dll"), b, 2, &n) != 0);
    if (ok && (b[0] != 'M' || b[1] != 'Z')) ok = 0, xpt_err = 0xFFFFFFFE;
    return ok;
}
static int op_wpm(HANDLE h, pctx *c) { SIZE_T n = 0; DWORD v = 0x5A5A; return XB(U(WriteProcessMemory)(h, c->rw, &v, sizeof v, &n)); }
static int op_rthread(HANDLE h, pctx *c) {
    DWORD tid = 0;
    HANDLE t = U(CreateRemoteThreadEx)(h, NULL, 0, (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleA("kernel32.dll"), "ExitThread"), (LPVOID)7, 0, NULL, &tid);
    int ok = xpt_b(t != NULL);
    if (t) { WaitForSingleObject(t, 5000); CloseHandle(t); }
    return ok;
}
static int op_wait(HANDLE h, pctx *c) { return XB(U(WaitForSingleObject)(h, 0) != WAIT_FAILED); }

static int pctx_init(pctx *c) {
    memset(c, 0, sizeof *c);
    c->full = xpt_spawn_sleeper(&c->pi);
    if (!c->full) return 0;
    c->pid = c->pi.dwProcessId;
    DWORD_PTR sm; GetProcessAffinityMask(c->full, &c->mask, &sm);
    c->rw = VirtualAllocEx(c->full, NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    return 1;
}
static void pmatrix(const char *api, pop fn, DWORD allowed) {
    pctx c;
    if (!pctx_init(&c)) { CHECK(!"sleeper child"); return; }
    for (int i = 0; i < XPT_NPR; i++) {
        DWORD r = xpt_prights[i].r;
        HANDLE h = U(OpenProcess)(r, FALSE, c.pid);
        CHECKF(h != NULL, "%s: OpenProcess(%s) err=%lu", api, xpt_prights[i].n, GetLastError());
        if (!h) continue;
        xpt_err = 0xFFFFFFFF;
        int ok = fn(h, &c);
        int want = (allowed & r) != 0;
        CHECKF(ok == want, "%s with %s: ok=%d want %d err=%lu", api, xpt_prights[i].n, ok, want, xpt_err);
        if (!ok && !want) CHECKF(xpt_err == ERROR_ACCESS_DENIED, "%s with %s: err=%lu want 5", api, xpt_prights[i].n, xpt_err);
        CloseHandle(h);
    }
    EQ(WaitForSingleObject(c.full, 0), WAIT_TIMEOUT);   /* the child is still alive */
    xpt_kill(&c.pi);
}
#define PM(name, fn, allowed) TEST(xpt_acc_proc_##name) { pmatrix(#name, fn, allowed); }

PM(getpriorityclass, op_getpc, PQ)
PM(setpriorityclass, op_setpc, PROCESS_SET_INFORMATION)
PM(getprocessaffinitymask, op_getaff, PQ)
PM(setprocessaffinitymask, op_setaff, PROCESS_SET_INFORMATION)
PM(getprocesstimes, op_times, PQ)
PM(queryprocesscycletime, op_cycle, PQ)
PM(getprocesshandlecount, op_hcount, PQ)
PM(getprocessiocounters, op_io, PQ)
PM(getprocesspriorityboost, op_getboost, PQ)
PM(setprocesspriorityboost, op_setboost, PROCESS_SET_INFORMATION)
PM(queryfullprocessimagenamew, op_image, PQ)
PM(queryfullprocessimagenamew_native, op_image_native, PQ)
PM(getprocessinformation_memprio, op_getmem, PQ)
PM(setprocessinformation_memprio, op_setmem, PROCESS_SET_INFORMATION)
PM(getprocessinformation_protection, op_getprot, PQ)
PM(getprocessinformation_appmemory, op_getappmem, PQ)
PM(setprocessinformation_powerthrottling, op_setthr, PS)
PM(getexitcodeprocess, op_exitcode, PQ)
PM(getprocessid, op_pid, PQ)
PM(iswow64process2, op_wow2, PQ)
PM(virtualallocex, op_valloc, PROCESS_VM_OPERATION)
PM(readprocessmemory, op_rpm, PROCESS_VM_READ)
PM(writeprocessmemory, op_wpm, 0)
/* CreateRemoteThreadEx is not limited by the rights of the handle passed in: kernelbase retries with a
 * handle of its own when the kernel refuses (see xpt_acc_proc_createremotethreadex_zero_access_handle). */
PM(createremotethreadex, op_rthread, 0xFFFFFFFF)
PM(wait, op_wait, SYNCHRONIZE)

/* TerminateProcess: every right but TERMINATE is refused, then a TERMINATE-only handle ends the child. */
TEST(xpt_acc_proc_terminateprocess) {
    pctx c; if (!pctx_init(&c)) { CHECK(!"sleeper child"); return; }
    for (int i = 0; i < XPT_NPR; i++) {
        if (xpt_prights[i].r == PROCESS_TERMINATE) continue;
        HANDLE h = U(OpenProcess)(xpt_prights[i].r, FALSE, c.pid);
        if (!h) { CHECK(h != NULL); continue; }
        SetLastError(0);
        BOOL r = U(TerminateProcess)(h, 0x77);
        DWORD e = GetLastError();
        CHECKF(!r && e == ERROR_ACCESS_DENIED, "TerminateProcess with %s: r=%d err=%lu", xpt_prights[i].n, r, e);
        CloseHandle(h);
    }
    EQ(WaitForSingleObject(c.full, 0), WAIT_TIMEOUT);
    HANDLE t = U(OpenProcess)(PROCESS_TERMINATE, FALSE, c.pid);
    CHECK(t != NULL);
    if (t) { SUCCEEDS(U(TerminateProcess)(t, 0x77)); CloseHandle(t); }
    EQ(WaitForSingleObject(c.full, 5000), WAIT_OBJECT_0);
    DWORD code = 0; GetExitCodeProcess(c.full, &code); EQ(code, 0x77);
    xpt_kill(&c.pi);
}

/* The documented combinations for writing memory and creating a remote thread. */
TEST(xpt_acc_proc_writeprocessmemory_write_plus_operation) {
    pctx c; if (!pctx_init(&c)) { CHECK(!"sleeper child"); return; }
    HANDLE h = U(OpenProcess)(PROCESS_VM_WRITE | PROCESS_VM_OPERATION, FALSE, c.pid);
    CHECK(h != NULL);
    if (h) {
        DWORD v = 0xA5A5A5A5, back = 0; SIZE_T n = 0;
        SUCCEEDS(U(WriteProcessMemory)(h, c.rw, &v, sizeof v, &n));
        EQ(n, sizeof v);
        ReadProcessMemory(c.full, c.rw, &back, sizeof back, &n);
        EQ(back, 0xA5A5A5A5);
        CloseHandle(h);
    }
    xpt_kill(&c.pi);
}
TEST(xpt_acc_proc_createremotethreadex_documented_rights) {
    pctx c; if (!pctx_init(&c)) { CHECK(!"sleeper child"); return; }
    HANDLE h = U(OpenProcess)(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, c.pid);
    CHECK(h != NULL);
    if (h) {
        DWORD tid = 0;
        HANDLE t = U(CreateRemoteThreadEx)(h, NULL, 0, (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleA("kernel32.dll"), "ExitThread"), (LPVOID)0x31, 0, NULL, &tid);
        CHECKF(t != NULL, "err=%lu", GetLastError());
        if (t) {
            NE(tid, 0);
            EQ(WaitForSingleObject(t, 5000), WAIT_OBJECT_0);
            DWORD code = 0; GetExitCodeThread(t, &code); EQ(code, 0x31);
            EQ(GetProcessIdOfThread(t), c.pid);
            CloseHandle(t);
        }
        CloseHandle(h);
    }
    xpt_kill(&c.pi);
}
/* OpenProcess with no access is refused; a zero-access duplicate is refused by queries and waits. */
TEST(xpt_acc_proc_zero_access_handle) {
    pctx c; if (!pctx_init(&c)) { CHECK(!"sleeper child"); return; }
    FAILS(U(OpenProcess)(0, FALSE, c.pid), ERROR_ACCESS_DENIED);
    HANDLE h = NULL;
    SUCCEEDS(U(DuplicateHandle)(GetCurrentProcess(), c.full, GetCurrentProcess(), &h, 0, FALSE, 0));
    if (h) {
        EQ(xpt_access(h), 0);
        DWORD x = 0x4321;
        FAILS(U(GetExitCodeProcess)(h, &x), ERROR_ACCESS_DENIED);
        EQ(x, 0x4321);
        FAILS(U(GetPriorityClass)(h), ERROR_ACCESS_DENIED);
        FAILS(U(GetProcessId)(h), ERROR_ACCESS_DENIED);
        SetLastError(0); EQ(U(WaitForSingleObject)(h, 0), WAIT_FAILED); EXPECT_ERR(ERROR_ACCESS_DENIED);
        CloseHandle(h);
    }
    xpt_kill(&c.pi);
}
/* SetPriorityClass with SET_INFORMATION really changes the class; restored afterwards (child only). */
TEST(xpt_acc_proc_setpriorityclass_applies) {
    pctx c; if (!pctx_init(&c)) { CHECK(!"sleeper child"); return; }
    HANDLE s = U(OpenProcess)(PROCESS_SET_INFORMATION, FALSE, c.pid);
    HANDLE q = U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, c.pid);
    CHECK(s && q);
    if (s && q) {
        SUCCEEDS(U(SetPriorityClass)(s, BELOW_NORMAL_PRIORITY_CLASS));
        EQ(U(GetPriorityClass)(q), BELOW_NORMAL_PRIORITY_CLASS);
        SUCCEEDS(U(SetPriorityClass)(s, IDLE_PRIORITY_CLASS));
        EQ(U(GetPriorityClass)(q), IDLE_PRIORITY_CLASS);
        SUCCEEDS(U(SetPriorityClass)(s, NORMAL_PRIORITY_CLASS));
        EQ(U(GetPriorityClass)(q), NORMAL_PRIORITY_CLASS);
    }
    if (s) CloseHandle(s);
    if (q) CloseHandle(q);
    xpt_kill(&c.pi);
}

/* NtCreateThreadEx refuses a zero-access process handle, but CreateRemoteThreadEx / CreateRemoteThread
 * with the same handle still create the thread in the child (the object grants us full access). */
typedef LONG (NTAPI *xpt_NtCreateThreadEx_pfn)(PHANDLE, ACCESS_MASK, PVOID, HANDLE, PVOID, PVOID, ULONG, SIZE_T, SIZE_T, SIZE_T, PVOID);
TEST(xpt_acc_proc_createremotethreadex_zero_access_handle) {
    xpt_NtCreateThreadEx_pfn nct = (xpt_NtCreateThreadEx_pfn)t_proc_nt("NtCreateThreadEx");
    REQUIRE(nct); REQUIRE(U(CreateRemoteThreadEx)); REQUIRE(U(CreateRemoteThread));
    pctx c; if (!pctx_init(&c)) { CHECK(!"sleeper child"); return; }
    void *et = (void *)GetProcAddress(GetModuleHandleA("kernel32.dll"), "ExitThread");
    HANDLE z = NULL;
    SUCCEEDS(DuplicateHandle(GetCurrentProcess(), c.full, GetCurrentProcess(), &z, 0, FALSE, 0));
    if (z) {
        EQ(xpt_access(z), 0);
        HANDLE t = NULL;
        EQ((ULONG)nct(&t, THREAD_ALL_ACCESS, NULL, z, et, (PVOID)9, 0, 0, 0, 0, NULL), 0xC0000022u);   /* STATUS_ACCESS_DENIED */
        EQ(t, NULL);
        DWORD tid = 0;
        t = U(CreateRemoteThreadEx)(z, NULL, 0, (LPTHREAD_START_ROUTINE)et, (LPVOID)0x41, 0, NULL, &tid);
        CHECKF(t != NULL, "err=%lu", GetLastError());
        if (t) {
            EQ(WaitForSingleObject(t, 5000), WAIT_OBJECT_0);
            DWORD code = 0; GetExitCodeThread(t, &code); EQ(code, 0x41);
            EQ(GetProcessIdOfThread(t), c.pid);
            EQ(GetThreadId(t), tid);
            CloseHandle(t);
        }
        t = U(CreateRemoteThread)(z, NULL, 0, (LPTHREAD_START_ROUTINE)et, (LPVOID)0x42, 0, NULL);
        CHECKF(t != NULL, "err=%lu", GetLastError());
        if (t) { EQ(WaitForSingleObject(t, 5000), WAIT_OBJECT_0); DWORD code = 0; GetExitCodeThread(t, &code); EQ(code, 0x42); CloseHandle(t); }
        /* the handle itself is unchanged */
        EQ(xpt_access(z), 0);
        CloseHandle(z);
    }
    EQ(WaitForSingleObject(c.full, 0), WAIT_TIMEOUT);
    xpt_kill(&c.pi);
}
/* DuplicateHandle from a zero-access process handle to a wider mask is granted for an own child. */
TEST(xpt_acc_proc_dup_widen_from_zero_access) {
    pctx c; if (!pctx_init(&c)) { CHECK(!"sleeper child"); return; }
    HANDLE z = NULL, w = NULL;
    SUCCEEDS(DuplicateHandle(GetCurrentProcess(), c.full, GetCurrentProcess(), &z, 0, FALSE, 0));
    if (z) {
        SUCCEEDS(U(DuplicateHandle)(GetCurrentProcess(), z, GetCurrentProcess(), &w, PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, 0));
        if (w) {
            EQ(xpt_access(w), PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE);
            EQ(U(GetProcessId)(w), c.pid);
            EQ(WaitForSingleObject(w, 0), WAIT_TIMEOUT);
            CloseHandle(w);
        }
        CloseHandle(z);
    }
    xpt_kill(&c.pi);
}
