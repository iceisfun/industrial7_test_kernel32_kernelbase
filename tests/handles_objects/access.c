#include "ho.h"

#define ACCESS_DENIED_CHECK(call, what) do { SET_ERR(0); BOOL r_ = (call); DWORD e_ = GetLastError(); \
    CHECKF(!r_, "%s unexpectedly succeeded", what); CHECKF(e_ == ERROR_ACCESS_DENIED, "%s: err=%lu want 5", what, e_); } while (0)
#define WAIT_DENIED(h, what) do { SET_ERR(0); DWORD r_ = U(WaitForSingleObject)((h), 0); DWORD e_ = GetLastError(); \
    CHECKF(r_ == WAIT_FAILED, "%s wait returned %lu", what, r_); CHECKF(e_ == ERROR_ACCESS_DENIED, "%s wait err=%lu", what, e_); } while (0)

/* ---- events ---- */
TEST(ho_acc_event_synchronize_only) {
    REQUIRE(U(OpenEventA)); REQUIRE(U(SetEvent)); REQUIRE(U(ResetEvent)); REQUIRE(U(WaitForSingleObject));
    char n[96]; ho_uniq(n, sizeof n, "evs");
    HANDLE full = CreateEventA(NULL, TRUE, FALSE, n);
    HANDLE h = U(OpenEventA)(SYNCHRONIZE, FALSE, n);
    CHECK(h != NULL); if (!h) { CloseHandle(full); return; }
    EXPECT_EQ(ho_access(h), SYNCHRONIZE);
    EXPECT_EQ(U(WaitForSingleObject)(h, 0), WAIT_TIMEOUT);
    ACCESS_DENIED_CHECK(U(SetEvent)(h), "SetEvent");
    ACCESS_DENIED_CHECK(U(ResetEvent)(h), "ResetEvent");
    ACCESS_DENIED_CHECK(U(PulseEvent)(h), "PulseEvent");
    SetEvent(full);
    EXPECT_EQ(U(WaitForSingleObject)(h, 0), WAIT_OBJECT_0);
    CloseHandle(h); CloseHandle(full);
}
TEST(ho_acc_event_modify_only) {
    REQUIRE(U(OpenEventA));
    char n[96]; ho_uniq(n, sizeof n, "evm");
    HANDLE full = CreateEventA(NULL, TRUE, FALSE, n);
    HANDLE h = U(OpenEventA)(EVENT_MODIFY_STATE, FALSE, n);
    CHECK(h != NULL); if (!h) { CloseHandle(full); return; }
    EXPECT_EQ(ho_access(h), EVENT_MODIFY_STATE);
    CHECK(U(SetEvent)(h));
    EXPECT_EQ(WaitForSingleObject(full, 0), WAIT_OBJECT_0);
    CHECK(U(ResetEvent)(h));
    EXPECT_EQ(WaitForSingleObject(full, 0), WAIT_TIMEOUT);
    WAIT_DENIED(h, "event without SYNCHRONIZE");
    DWORD r = WaitForMultipleObjects(1, &h, FALSE, 0); EXPECT_EQ(r, WAIT_FAILED);
    CloseHandle(h); CloseHandle(full);
}
TEST(ho_acc_event_zero_access) {
    REQUIRE(U(OpenEventA));
    char n[96]; ho_uniq(n, sizeof n, "ev0");
    HANDLE full = CreateEventA(NULL, TRUE, FALSE, n);
    HANDLE h = U(OpenEventA)(0, FALSE, n);
    if (!h) { t_note("OpenEvent(0) failed err=%lu", GetLastError()); CloseHandle(full); t_skip("zero-access open not supported here"); return; }
    EXPECT_EQ(ho_access(h), 0);
    ACCESS_DENIED_CHECK(U(SetEvent)(h), "SetEvent");
    WAIT_DENIED(h, "zero-access event");
    EXPECT_EQ(U(CloseHandle)(h), TRUE);
    CloseHandle(full);
}
TEST(ho_acc_event_exact_granted_bits) {
    REQUIRE(U(OpenEventA));
    char n[96]; ho_uniq(n, sizeof n, "evx");
    HANDLE full = CreateEventA(NULL, TRUE, FALSE, n);
    DWORD bits[] = { SYNCHRONIZE, EVENT_MODIFY_STATE, READ_CONTROL, DELETE, WRITE_DAC, WRITE_OWNER, SYNCHRONIZE | EVENT_MODIFY_STATE, EVENT_ALL_ACCESS };
    for (unsigned i = 0; i < sizeof bits / sizeof *bits; i++) {
        HANDLE h = U(OpenEventA)(bits[i], FALSE, n);
        CHECKF(h != NULL, "bits=%lx err=%lu", bits[i], GetLastError());
        if (h) { CHECKF(ho_access(h) == bits[i], "bits=%lx granted=%lx", bits[i], ho_access(h)); CloseHandle(h); }
    }
    CloseHandle(full);
}
TEST(ho_acc_event_generic_rights_mapped) {
    REQUIRE(U(OpenEventA));
    char n[96]; ho_uniq(n, sizeof n, "evg");
    HANDLE full = CreateEventA(NULL, TRUE, FALSE, n);
    HANDLE h = U(OpenEventA)(GENERIC_READ, FALSE, n);
    CHECK(h != NULL);
    if (h) { ULONG a = ho_access(h); CHECK((a & GENERIC_READ) == 0); CHECK((a & EVENT_MODIFY_STATE) == 0); t_note("GENERIC_READ event -> %lx", a); CloseHandle(h); }
    h = U(OpenEventA)(GENERIC_ALL, FALSE, n);
    CHECK(h != NULL);
    if (h) { EXPECT_EQ(ho_access(h), EVENT_ALL_ACCESS); CloseHandle(h); }
    h = U(OpenEventA)(GENERIC_EXECUTE, FALSE, n);
    CHECK(h != NULL);
    if (h) { CHECK(ho_access(h) & SYNCHRONIZE); t_note("GENERIC_EXECUTE event -> %lx", ho_access(h)); CloseHandle(h); }
    h = U(OpenEventA)(MAXIMUM_ALLOWED, FALSE, n);
    CHECK(h != NULL);
    if (h) { CHECK(ho_access(h) & EVENT_MODIFY_STATE); CloseHandle(h); }
    CloseHandle(full);
}
TEST(ho_acc_event_create_ex_desired_access) {
    REQUIRE(U(CreateEventExA));
    HANDLE h = U(CreateEventExA)(NULL, NULL, 0, SYNCHRONIZE);
    CHECK(h != NULL);
    if (h) { EXPECT_EQ(ho_access(h), SYNCHRONIZE); ACCESS_DENIED_CHECK(SetEvent(h), "SetEvent on SYNCHRONIZE-only creation"); CloseHandle(h); }
    h = U(CreateEventExA)(NULL, NULL, CREATE_EVENT_MANUAL_RESET | CREATE_EVENT_INITIAL_SET, EVENT_MODIFY_STATE | SYNCHRONIZE);
    CHECK(h != NULL);
    if (h) { EXPECT_EQ(ho_access(h), EVENT_MODIFY_STATE | SYNCHRONIZE); EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0); EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0); CloseHandle(h); }
    h = U(CreateEventExA)(NULL, NULL, 0, 0);
    CHECK(h != NULL);
    if (h) { EXPECT_EQ(ho_access(h), 0); CloseHandle(h); }
}
TEST(ho_acc_event_access_system_security) {
    REQUIRE(U(OpenEventA));
    char n[96]; ho_uniq(n, sizeof n, "evs2");
    HANDLE full = CreateEventA(NULL, TRUE, FALSE, n);
    SET_ERR(0);
    HANDLE h = U(OpenEventA)(ACCESS_SYSTEM_SECURITY, FALSE, n);
    t_note("OpenEvent(ACCESS_SYSTEM_SECURITY): h=%p err=%lu", h, GetLastError());
    if (h) CloseHandle(h);
    CloseHandle(full);
}
/* ---- mutex ---- */
TEST(ho_acc_mutex_synchronize_only_can_acquire_and_release) {
    REQUIRE(U(OpenMutexA)); REQUIRE(U(ReleaseMutex));
    char n[96]; ho_uniq(n, sizeof n, "mtx");
    HANDLE full = CreateMutexA(NULL, FALSE, n);
    HANDLE h = U(OpenMutexA)(SYNCHRONIZE, FALSE, n);
    CHECK(h != NULL); if (!h) { CloseHandle(full); return; }
    EXPECT_EQ(ho_access(h), SYNCHRONIZE);
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
    EXPECT_EQ(U(ReleaseMutex)(h), TRUE);      /* MUTEX_MODIFY_STATE is reserved: not required to release */
    SET_ERR(0);
    EXPECT_EQ(U(ReleaseMutex)(h), FALSE); EXPECT_ERR(ERROR_NOT_OWNER);
    CloseHandle(h); CloseHandle(full);
}
TEST(ho_acc_mutex_zero_access_wait_denied) {
    REQUIRE(U(OpenMutexA));
    char n[96]; ho_uniq(n, sizeof n, "mtx0");
    HANDLE full = CreateMutexA(NULL, FALSE, n);
    HANDLE h = U(OpenMutexA)(0, FALSE, n);
    if (!h) { t_note("OpenMutex(0) failed err=%lu", GetLastError()); CloseHandle(full); t_skip("zero-access open not supported here"); return; }
    WAIT_DENIED(h, "mutex"); CloseHandle(h);
    CloseHandle(full);
}
TEST(ho_acc_mutex_create_ex_access) {
    REQUIRE(U(CreateMutexExA));
    HANDLE h = U(CreateMutexExA)(NULL, NULL, CREATE_MUTEX_INITIAL_OWNER, SYNCHRONIZE);
    CHECK(h != NULL);
    if (h) { EXPECT_EQ(ho_access(h), SYNCHRONIZE); EXPECT_EQ(U(ReleaseMutex)(h), TRUE); CloseHandle(h); }
}
/* ---- semaphore ---- */
TEST(ho_acc_semaphore_rights) {
    REQUIRE(U(OpenSemaphoreA)); REQUIRE(U(ReleaseSemaphore));
    char n[96]; ho_uniq(n, sizeof n, "sem");
    HANDLE full = CreateSemaphoreA(NULL, 1, 5, n);
    HANDLE s = U(OpenSemaphoreA)(SYNCHRONIZE, FALSE, n);
    CHECK(s != NULL);
    if (s) {
        EXPECT_EQ(ho_access(s), SYNCHRONIZE);
        ACCESS_DENIED_CHECK(U(ReleaseSemaphore)(s, 1, NULL), "ReleaseSemaphore");
        EXPECT_EQ(U(WaitForSingleObject)(s, 0), WAIT_OBJECT_0);
        EXPECT_EQ(WaitForSingleObject(full, 0), WAIT_TIMEOUT);
        CloseHandle(s);
    }
    HANDLE m = U(OpenSemaphoreA)(SEMAPHORE_MODIFY_STATE, FALSE, n);
    CHECK(m != NULL);
    if (m) {
        LONG prev = -1;
        EXPECT_EQ(U(ReleaseSemaphore)(m, 2, &prev), TRUE); EXPECT_EQ(prev, 0);
        WAIT_DENIED(m, "semaphore");
        CloseHandle(m);
    }
    CloseHandle(full);
}
TEST(ho_acc_semaphore_release_overflow_error) {
    REQUIRE(U(ReleaseSemaphore));
    HANDLE s = CreateSemaphoreA(NULL, 1, 2, NULL);
    LONG prev = 77;
    SET_ERR(0);
    EXPECT_EQ(U(ReleaseSemaphore)(s, 2, &prev), FALSE); EXPECT_ERR(ERROR_TOO_MANY_POSTS);
    SET_ERR(0);
    EXPECT_EQ(U(ReleaseSemaphore)(s, 0, &prev), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    EXPECT_EQ(U(ReleaseSemaphore)(s, -1, &prev), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    EXPECT_EQ(U(ReleaseSemaphore)(s, 1, &prev), TRUE); EXPECT_EQ(prev, 1);
    CloseHandle(s);
}
/* ---- timer ---- */
TEST(ho_acc_timer_rights) {
    REQUIRE(U(OpenWaitableTimerA)); REQUIRE(U(SetWaitableTimer)); REQUIRE(U(CancelWaitableTimer));
    char n[96]; ho_uniq(n, sizeof n, "tmr");
    HANDLE full = CreateWaitableTimerA(NULL, TRUE, n);
    LARGE_INTEGER due; due.QuadPart = -10000000LL * 3600;
    HANDLE s = U(OpenWaitableTimerA)(SYNCHRONIZE, FALSE, n);
    CHECK(s != NULL);
    if (s) {
        EXPECT_EQ(ho_access(s), SYNCHRONIZE);
        EXPECT_EQ(U(WaitForSingleObject)(s, 0), WAIT_TIMEOUT);
        ACCESS_DENIED_CHECK(U(SetWaitableTimer)(s, &due, 0, NULL, NULL, FALSE), "SetWaitableTimer");
        ACCESS_DENIED_CHECK(U(CancelWaitableTimer)(s), "CancelWaitableTimer");
        CloseHandle(s);
    }
    HANDLE m = U(OpenWaitableTimerA)(TIMER_MODIFY_STATE, FALSE, n);
    CHECK(m != NULL);
    if (m) {
        EXPECT_EQ(ho_access(m), TIMER_MODIFY_STATE);
        EXPECT_EQ(U(SetWaitableTimer)(m, &due, 0, NULL, NULL, FALSE), TRUE);
        WAIT_DENIED(m, "timer");
        EXPECT_EQ(U(CancelWaitableTimer)(m), TRUE);
        CloseHandle(m);
    }
    CloseHandle(full);
}
TEST(ho_acc_timer_fires_visible_through_sync_only_handle) {
    REQUIRE(U(OpenWaitableTimerA)); REQUIRE(U(SetWaitableTimer));
    char n[96]; ho_uniq(n, sizeof n, "tmr2");
    HANDLE full = CreateWaitableTimerA(NULL, TRUE, n);
    HANDLE s = U(OpenWaitableTimerA)(SYNCHRONIZE, FALSE, n);
    LARGE_INTEGER due; due.QuadPart = -10 * 1000 * 50;
    U(SetWaitableTimer)(full, &due, 0, NULL, NULL, FALSE);
    CHECK(s != NULL);
    if (s) { EXPECT_EQ(U(WaitForSingleObject)(s, 5000), WAIT_OBJECT_0); CloseHandle(s); }
    CloseHandle(full);
}
/* ---- file mapping ---- */
TEST(ho_acc_mapping_create_protection_granted_access_table) {
    REQUIRE(U(CreateFileMappingW));
    struct { DWORD prot; ULONG want; } t[] = {
        { PAGE_READONLY, 0xF0005 }, { PAGE_READWRITE, 0xF0007 }, { PAGE_WRITECOPY, 0xF0005 },
        { PAGE_EXECUTE_READ, 0xF000D }, { PAGE_EXECUTE_READWRITE, 0xF000F }, { PAGE_EXECUTE_WRITECOPY, 0xF000D } };
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "mapacc.bin");
    HANDLE f = CreateFileA(p, GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, NULL);
    DWORD n; WriteFile(f, "0123456789", 10, &n, NULL);
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        HANDLE m = U(CreateFileMappingW)(f, NULL, t[i].prot, 0, 0, NULL);   /* size 0: whole file (a read-only mapping cannot extend it) */
        CHECKF(m != NULL, "prot=%lx err=%lu", t[i].prot, GetLastError());
        if (m) { CHECKF(ho_access(m) == t[i].want, "prot=%lx granted=%lx want=%lx", t[i].prot, ho_access(m), t[i].want); CloseHandle(m); }
        m = U(CreateFileMappingW)(INVALID_HANDLE_VALUE, NULL, t[i].prot, 0, 4096, NULL);
        CHECKF(m != NULL, "pagefile prot=%lx err=%lu", t[i].prot, GetLastError());
        if (m) { CHECKF(ho_access(m) == t[i].want, "pagefile prot=%lx granted=%lx", t[i].prot, ho_access(m)); CloseHandle(m); }
    }
    CloseHandle(f);
}
TEST(ho_acc_mapping_readonly_handle_cannot_map_write) {
    REQUIRE(U(CreateFileMappingA)); REQUIRE(U(MapViewOfFile));
    HANDLE m = U(CreateFileMappingA)(INVALID_HANDLE_VALUE, NULL, PAGE_READONLY | SEC_COMMIT, 0, 4096, NULL);
    CHECK(m != NULL); if (!m) return;
    SET_ERR(0);
    void *v = U(MapViewOfFile)(m, FILE_MAP_WRITE, 0, 0, 0);
    CHECK(v == NULL); EXPECT_ERR(ERROR_ACCESS_DENIED);
    if (v) UnmapViewOfFile(v);
    v = U(MapViewOfFile)(m, FILE_MAP_READ, 0, 0, 0);
    CHECK(v != NULL);
    if (v) { EXPECT_EQ(*(volatile char *)v, 0); UnmapViewOfFile(v); }
    CloseHandle(m);
}
TEST(ho_acc_mapping_open_with_minimal_rights) {
    REQUIRE(U(OpenFileMappingA)); REQUIRE(U(MapViewOfFile));
    char n[96]; ho_uniq(n, sizeof n, "map");
    HANDLE full = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, n);
    char *w = MapViewOfFile(full, FILE_MAP_WRITE, 0, 0, 0); strcpy(w, "data");
    HANDLE rd = U(OpenFileMappingA)(FILE_MAP_READ, FALSE, n);
    CHECK(rd != NULL);
    if (rd) {
        EXPECT_EQ(ho_access(rd), FILE_MAP_READ);
        char *v = U(MapViewOfFile)(rd, FILE_MAP_READ, 0, 0, 0);
        CHECK(v != NULL); if (v) { EXPECT_STR(v, "data"); UnmapViewOfFile(v); }
        SET_ERR(0);
        v = U(MapViewOfFile)(rd, FILE_MAP_WRITE, 0, 0, 0);
        CHECK(v == NULL); EXPECT_ERR(ERROR_ACCESS_DENIED); if (v) UnmapViewOfFile(v);
        CloseHandle(rd);
    }
    HANDLE sy = U(OpenFileMappingA)(SYNCHRONIZE, FALSE, n);
    CHECK(sy != NULL);
    if (sy) {
        SET_ERR(0);
        void *v = U(MapViewOfFile)(sy, FILE_MAP_READ, 0, 0, 0);
        CHECK(v == NULL); EXPECT_ERR(ERROR_ACCESS_DENIED); if (v) UnmapViewOfFile(v);
        CloseHandle(sy);
    }
    HANDLE wr = U(OpenFileMappingA)(FILE_MAP_WRITE, FALSE, n);
    CHECK(wr != NULL);
    if (wr) {
        char *v = U(MapViewOfFile)(wr, FILE_MAP_WRITE, 0, 0, 0);
        t_note("write-only mapping handle: MapViewOfFile(WRITE)=%p err=%lu", v, v ? 0 : GetLastError());
        if (v) UnmapViewOfFile(v);
        CloseHandle(wr);
    }
    UnmapViewOfFile(w); CloseHandle(full);
}
TEST(ho_acc_mapping_view_survives_handle_close) {
    HANDLE m = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, NULL);
    char *v = MapViewOfFile(m, FILE_MAP_WRITE, 0, 0, 0);
    CloseHandle(m);
    strcpy(v, "alive");
    EXPECT_STR(v, "alive");
    UnmapViewOfFile(v);
}
/* ---- process ---- */
CHILD(ho_idle2) { Sleep(8000); return 0; }
static HANDLE spawn_idle(PROCESS_INFORMATION *pi) { return t_spawn_child("ho_idle2", NULL, 0, NULL, NULL, NULL, FALSE, NULL, pi); }
TEST(ho_acc_process_limited_query_only) {
    REQUIRE(U(OpenProcess)); REQUIRE(U(GetExitCodeProcess)); REQUIRE(U(TerminateProcess)); REQUIRE(U(SetPriorityClass)); REQUIRE(U(GetPriorityClass));
    PROCESS_INFORMATION pi; HANDLE own = spawn_idle(&pi);
    CHECK(own != NULL); if (!own) return;
    HANDLE h = U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pi.dwProcessId);
    CHECK(h != NULL);
    if (h) {
        EXPECT_EQ(ho_access(h), PROCESS_QUERY_LIMITED_INFORMATION);
        DWORD c = 0;
        EXPECT_EQ(U(GetExitCodeProcess)(h, &c), TRUE); EXPECT_EQ(c, STILL_ACTIVE);
        EXPECT_EQ(U(GetProcessId)(h), pi.dwProcessId);
        DWORD pc = U(GetPriorityClass)(h); CHECK(pc != 0);
        ACCESS_DENIED_CHECK(U(TerminateProcess)(h, 1), "TerminateProcess");
        ACCESS_DENIED_CHECK(U(SetPriorityClass)(h, IDLE_PRIORITY_CLASS), "SetPriorityClass");
        WAIT_DENIED(h, "process without SYNCHRONIZE");
        char b[4]; SIZE_T rd;
        ACCESS_DENIED_CHECK(U(ReadProcessMemory)(h, GetModuleHandleA(NULL), b, 2, &rd), "ReadProcessMemory");
        SET_ERR(0); void *p = U(VirtualAllocEx)(h, NULL, 4096, MEM_RESERVE, PAGE_NOACCESS);
        CHECK(p == NULL); EXPECT_ERR(ERROR_ACCESS_DENIED); if (p) VirtualFreeEx(h, p, 0, MEM_RELEASE);
        CloseHandle(h);
    }
    TerminateProcess(own, 0); WaitForSingleObject(own, 5000);
    CloseHandle(pi.hThread); CloseHandle(own);
}
TEST(ho_acc_process_synchronize_only) {
    REQUIRE(U(OpenProcess)); REQUIRE(U(GetExitCodeProcess));
    PROCESS_INFORMATION pi; HANDLE own = spawn_idle(&pi);
    CHECK(own != NULL); if (!own) return;
    HANDLE h = U(OpenProcess)(SYNCHRONIZE, FALSE, pi.dwProcessId);
    CHECK(h != NULL);
    if (h) {
        DWORD c;
        ACCESS_DENIED_CHECK(U(GetExitCodeProcess)(h, &c), "GetExitCodeProcess");
        EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_TIMEOUT);
        TerminateProcess(own, 77);
        EXPECT_EQ(WaitForSingleObject(h, 5000), WAIT_OBJECT_0);
        EXPECT_EQ(U(GetProcessId)(h), 0);   /* needs PROCESS_QUERY_LIMITED_INFORMATION */
        EXPECT_ERR(ERROR_ACCESS_DENIED);
        CloseHandle(h);
    }
    GetExitCodeProcess(own, &(DWORD){0});
    CloseHandle(pi.hThread); CloseHandle(own);
}
TEST(ho_acc_process_terminate_only) {
    REQUIRE(U(OpenProcess)); REQUIRE(U(TerminateProcess));
    PROCESS_INFORMATION pi; HANDLE own = spawn_idle(&pi);
    CHECK(own != NULL); if (!own) return;
    HANDLE h = U(OpenProcess)(PROCESS_TERMINATE, FALSE, pi.dwProcessId);
    CHECK(h != NULL);
    if (h) {
        DWORD c;
        ACCESS_DENIED_CHECK(GetExitCodeProcess(h, &c), "GetExitCodeProcess with PROCESS_TERMINATE");
        EXPECT_EQ(U(TerminateProcess)(h, 1234), TRUE);
        EXPECT_EQ(WaitForSingleObject(own, 5000), WAIT_OBJECT_0);
        GetExitCodeProcess(own, &c); EXPECT_EQ(c, 1234);
        CloseHandle(h);
    }
    CloseHandle(pi.hThread); CloseHandle(own);
}
TEST(ho_acc_process_open_exact_rights_table) {
    REQUIRE(U(OpenProcess));
    DWORD rights[] = { PROCESS_QUERY_LIMITED_INFORMATION, PROCESS_QUERY_INFORMATION, PROCESS_VM_READ, PROCESS_DUP_HANDLE, SYNCHRONIZE, PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE };
    for (unsigned i = 0; i < sizeof rights / sizeof *rights; i++) {
        HANDLE h = U(OpenProcess)(rights[i], FALSE, GetCurrentProcessId());
        CHECKF(h != NULL, "rights=%lx err=%lu", rights[i], GetLastError());
        if (h) { ULONG a = ho_access(h); CHECKF((a & rights[i]) == rights[i], "rights=%lx granted=%lx", rights[i], a); if (a != rights[i]) t_note("rights=%lx granted=%lx", rights[i], a); CloseHandle(h); }
    }
}
TEST(ho_acc_process_self_all_access) {
    REQUIRE(U(OpenProcess));
    HANDLE h = U(OpenProcess)(PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId());
    CHECK(h != NULL);
    if (h) { CHECK((ho_access(h) & PROCESS_ALL_ACCESS) == PROCESS_ALL_ACCESS); char b[2]; SIZE_T n; CHECK(U(ReadProcessMemory)(h, GetModuleHandleA(NULL), b, 2, &n)); EXPECT_EQ(n, 2); CHECK(b[0] == 'M' && b[1] == 'Z'); CloseHandle(h); }
}
/* ---- thread ---- */
static DWORD WINAPI parked(LPVOID p) { WaitForSingleObject((HANDLE)p, 20000); return 55; }
TEST(ho_acc_thread_rights) {
    REQUIRE(U(OpenThread));
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL); DWORD tid;
    HANDLE own = CreateThread(NULL, 0, parked, ev, 0, &tid);
    HANDLE ql = U(OpenThread)(THREAD_QUERY_LIMITED_INFORMATION, FALSE, tid);
    CHECK(ql != NULL);
    if (ql) {
        EXPECT_EQ(ho_access(ql), THREAD_QUERY_LIMITED_INFORMATION);
        EXPECT_EQ(U(GetThreadId)(ql), tid);
        DWORD c; EXPECT_EQ(U(GetExitCodeThread)(ql, &c), TRUE); EXPECT_EQ(c, STILL_ACTIVE);
        SET_ERR(0); EXPECT_EQ(SuspendThread(ql), (DWORD)-1); EXPECT_ERR(ERROR_ACCESS_DENIED);
        SET_ERR(0); EXPECT_EQ(ResumeThread(ql), (DWORD)-1); EXPECT_ERR(ERROR_ACCESS_DENIED);
        ACCESS_DENIED_CHECK(SetThreadPriority(ql, THREAD_PRIORITY_NORMAL), "SetThreadPriority");
        ACCESS_DENIED_CHECK(TerminateThread(ql, 9), "TerminateThread");
        WAIT_DENIED(ql, "thread without SYNCHRONIZE");
        CONTEXT cx; memset(&cx, 0, sizeof cx); cx.ContextFlags = CONTEXT_CONTROL;
        ACCESS_DENIED_CHECK(GetThreadContext(ql, &cx), "GetThreadContext");
        CloseHandle(ql);
    }
    HANDLE sr = U(OpenThread)(THREAD_SUSPEND_RESUME, FALSE, tid);
    CHECK(sr != NULL);
    if (sr) {
        EXPECT_EQ(SuspendThread(sr), 0);
        EXPECT_EQ(ResumeThread(sr), 1);
        CloseHandle(sr);
    }
    HANDLE sl = U(OpenThread)(THREAD_SET_LIMITED_INFORMATION | THREAD_QUERY_LIMITED_INFORMATION, FALSE, tid);
    CHECK(sl != NULL);
    if (sl) { EXPECT_EQ(SetThreadPriority(sl, THREAD_PRIORITY_BELOW_NORMAL), TRUE); EXPECT_EQ(GetThreadPriority(sl), THREAD_PRIORITY_BELOW_NORMAL); CloseHandle(sl); }
    SetEvent(ev);
    EXPECT_EQ(WaitForSingleObject(own, 5000), WAIT_OBJECT_0);
    DWORD c; GetExitCodeThread(own, &c); EXPECT_EQ(c, 55);
    CloseHandle(own); CloseHandle(ev);
}
TEST(ho_acc_thread_synchronize_only_waits) {
    REQUIRE(U(OpenThread));
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL); DWORD tid;
    HANDLE own = CreateThread(NULL, 0, parked, ev, 0, &tid);
    HANDLE s = U(OpenThread)(SYNCHRONIZE, FALSE, tid);
    CHECK(s != NULL);
    if (s) {
        EXPECT_EQ(WaitForSingleObject(s, 0), WAIT_TIMEOUT);
        DWORD c; ACCESS_DENIED_CHECK(GetExitCodeThread(s, &c), "GetExitCodeThread");
        SetEvent(ev);
        EXPECT_EQ(WaitForSingleObject(s, 5000), WAIT_OBJECT_0);
        CloseHandle(s);
    } else SetEvent(ev);
    WaitForSingleObject(own, 5000);
    CloseHandle(own); CloseHandle(ev);
}
TEST(ho_acc_thread_open_exact_rights_table) {
    REQUIRE(U(OpenThread));
    DWORD rights[] = { THREAD_QUERY_LIMITED_INFORMATION, THREAD_QUERY_INFORMATION, THREAD_GET_CONTEXT, THREAD_SET_LIMITED_INFORMATION, SYNCHRONIZE, THREAD_QUERY_LIMITED_INFORMATION | SYNCHRONIZE };
    for (unsigned i = 0; i < sizeof rights / sizeof *rights; i++) {
        HANDLE h = U(OpenThread)(rights[i], FALSE, GetCurrentThreadId());
        CHECKF(h != NULL, "rights=%lx err=%lu", rights[i], GetLastError());
        if (h) { ULONG a = ho_access(h); CHECKF((a & rights[i]) == rights[i], "rights=%lx granted=%lx", rights[i], a); if (a != rights[i]) t_note("rights=%lx granted=%lx", rights[i], a); CloseHandle(h); }
    }
}
/* ---- file ---- */
TEST(ho_acc_file_read_only_handle) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(WriteFile));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "acc_r.bin");
    HANDLE w = CreateFileA(p, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, NULL); DWORD n;
    WriteFile(w, "abcdef", 6, &n, NULL); CloseHandle(w);
    HANDLE h = U(CreateFileA)(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) {
        ULONG a = ho_access(h);
        CHECK(a & FILE_READ_DATA); CHECK(!(a & FILE_WRITE_DATA)); CHECK(!(a & FILE_APPEND_DATA)); CHECK(a & SYNCHRONIZE);
        ACCESS_DENIED_CHECK(U(WriteFile)(h, "x", 1, &n, NULL), "WriteFile on read handle");
        ACCESS_DENIED_CHECK(SetEndOfFile(h), "SetEndOfFile on read handle");
        FILETIME ft = { 1, 1 };
        ACCESS_DENIED_CHECK(SetFileTime(h, NULL, NULL, &ft), "SetFileTime on read handle");
        char b[8]; EXPECT_EQ(ReadFile(h, b, 6, &n, NULL), TRUE); EXPECT_EQ(n, 6);
        CloseHandle(h);
    }
    DeleteFileA(p);
}
TEST(ho_acc_file_write_only_handle) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(ReadFile));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "acc_w.bin");
    HANDLE h = U(CreateFileA)(p, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) {
        DWORD n; char b[4];
        EXPECT_EQ(WriteFile(h, "abcd", 4, &n, NULL), TRUE);
        SetFilePointer(h, 0, NULL, FILE_BEGIN);
        ACCESS_DENIED_CHECK(U(ReadFile)(h, b, 4, &n, NULL), "ReadFile on write handle");
        CloseHandle(h);
    }
    DeleteFileA(p);
}
TEST(ho_acc_file_zero_access_handle) {
    REQUIRE(U(CreateFileA)); REQUIRE(U(GetFileInformationByHandle)); REQUIRE(U(GetFileType));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "acc_0.bin");
    HANDLE w = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); DWORD n;
    WriteFile(w, "12345", 5, &n, NULL); CloseHandle(w);
    HANDLE h = U(CreateFileA)(p, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) {
        BY_HANDLE_FILE_INFORMATION bi;
        EXPECT_EQ(U(GetFileInformationByHandle)(h, &bi), TRUE);
        EXPECT_EQ(bi.nFileSizeLow, 5);
        EXPECT_EQ(U(GetFileType)(h), FILE_TYPE_DISK);
        EXPECT_EQ(GetFileSize(h, NULL), 5);
        char b[4];
        ACCESS_DENIED_CHECK(ReadFile(h, b, 1, &n, NULL), "ReadFile on zero-access handle");
        ACCESS_DENIED_CHECK(WriteFile(h, "x", 1, &n, NULL), "WriteFile on zero-access handle");
        CloseHandle(h);
    }
    DeleteFileA(p);
}
TEST(ho_acc_file_append_only_handle) {
    REQUIRE(U(CreateFileA));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "acc_a.bin");
    HANDLE w = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); DWORD n;
    WriteFile(w, "head", 4, &n, NULL); CloseHandle(w);
    HANDLE h = U(CreateFileA)(p, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) {
        EXPECT_EQ(WriteFile(h, "TAIL", 4, &n, NULL), TRUE);
        EXPECT_EQ(GetFileSize(h, NULL), 8);
        EXPECT_EQ(ho_access(h), FILE_APPEND_DATA | SYNCHRONIZE | FILE_READ_ATTRIBUTES);   /* CreateFile adds both */
        CloseHandle(h);
    }
    h = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    char b[9] = { 0 }; ReadFile(h, b, 8, &n, NULL); EXPECT_STR(b, "headTAIL");
    CloseHandle(h); DeleteFileA(p);
}
TEST(ho_acc_file_share_violation_vs_access_denied) {
    REQUIRE(U(CreateFileA));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "acc_s.bin");
    HANDLE a = CreateFileA(p, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    SET_ERR(0);
    HANDLE b = U(CreateFileA)(p, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(b == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_SHARING_VIOLATION);
    b = U(CreateFileA)(p, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(b == INVALID_HANDLE_VALUE);           /* first opener allows only reads, but the second asks to deny write sharing it holds */
    CloseHandle(a);
    SetFileAttributesA(p, FILE_ATTRIBUTE_READONLY);
    SET_ERR(0);
    b = U(CreateFileA)(p, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(b == INVALID_HANDLE_VALUE); EXPECT_ERR(ERROR_ACCESS_DENIED);   /* read-only attribute */
    SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL);
    DeleteFileA(p);
}
/* ---- job ---- */
TEST(ho_acc_job_rights) {
    REQUIRE(U(OpenJobObjectA)); REQUIRE(U(QueryInformationJobObject)); REQUIRE(U(SetInformationJobObject)); REQUIRE(U(TerminateJobObject));
    char n[96]; ho_uniq(n, sizeof n, "job");
    HANDLE full = CreateJobObjectA(NULL, n);
    CHECK(full != NULL); if (!full) return;
    HANDLE q = U(OpenJobObjectA)(JOB_OBJECT_QUERY, FALSE, n);
    CHECK(q != NULL);
    if (q) {
        EXPECT_EQ(ho_access(q), JOB_OBJECT_QUERY);
        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION bi;
        EXPECT_EQ(U(QueryInformationJobObject)(q, JobObjectBasicAccountingInformation, &bi, sizeof bi, NULL), TRUE);
        EXPECT_EQ(bi.ActiveProcesses, 0);
        JOBOBJECT_BASIC_UI_RESTRICTIONS ui = { 0 };
        ACCESS_DENIED_CHECK(U(SetInformationJobObject)(q, JobObjectBasicUIRestrictions, &ui, sizeof ui), "SetInformationJobObject");
        ACCESS_DENIED_CHECK(U(TerminateJobObject)(q, 1), "TerminateJobObject");
        WAIT_DENIED(q, "job without SYNCHRONIZE");
        CloseHandle(q);
    }
    HANDLE sa = U(OpenJobObjectA)(JOB_OBJECT_SET_ATTRIBUTES, FALSE, n);
    CHECK(sa != NULL);
    if (sa) {
        JOBOBJECT_BASIC_UI_RESTRICTIONS ui = { 0 };
        EXPECT_EQ(U(SetInformationJobObject)(sa, JobObjectBasicUIRestrictions, &ui, sizeof ui), TRUE);
        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION bi;
        ACCESS_DENIED_CHECK(U(QueryInformationJobObject)(sa, JobObjectBasicAccountingInformation, &bi, sizeof bi, NULL), "QueryInformationJobObject");
        CloseHandle(sa);
    }
    HANDLE t = U(OpenJobObjectA)(JOB_OBJECT_TERMINATE, FALSE, n);
    CHECK(t != NULL);
    if (t) { EXPECT_EQ(U(TerminateJobObject)(t, 3), TRUE); CloseHandle(t); }   /* empty job: nothing to kill */
    CloseHandle(full);
}
TEST(ho_acc_job_assign_process_right_checked_first) {
    REQUIRE(U(OpenJobObjectA)); REQUIRE(U(AssignProcessToJobObject));
    char n[96]; ho_uniq(n, sizeof n, "job2");
    HANDLE full = CreateJobObjectA(NULL, n);
    HANDLE q = U(OpenJobObjectA)(JOB_OBJECT_QUERY, FALSE, n);
    PROCESS_INFORMATION pi; HANDLE own = spawn_idle(&pi);
    CHECK(q && own);
    if (q && own) { ACCESS_DENIED_CHECK(U(AssignProcessToJobObject)(q, own), "AssignProcessToJobObject without ASSIGN_PROCESS"); }
    if (own) { TerminateProcess(own, 0); WaitForSingleObject(own, 5000); CloseHandle(pi.hThread); CloseHandle(own); }
    if (q) CloseHandle(q);
    CloseHandle(full);
}
/* ---- io completion port ---- */
TEST(ho_acc_iocp_rights) {
    REQUIRE(U(PostQueuedCompletionStatus)); REQUIRE(U(DuplicateHandle));
    HANDLE port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0), d = NULL;
    CHECK(port != NULL);
    EXPECT_EQ(ho_access(port), 0x1F0003);
    DuplicateHandle(GetCurrentProcess(), port, GetCurrentProcess(), &d, SYNCHRONIZE, FALSE, 0);
    ACCESS_DENIED_CHECK(U(PostQueuedCompletionStatus)(d, 1, 2, NULL), "PostQueuedCompletionStatus");
    CloseHandle(d); d = NULL;
    DuplicateHandle(GetCurrentProcess(), port, GetCurrentProcess(), &d, 0x2 /* IO_COMPLETION_MODIFY_STATE */, FALSE, 0);
    EXPECT_EQ(U(PostQueuedCompletionStatus)(d, 5, 6, NULL), TRUE);
    DWORD n; ULONG_PTR k; OVERLAPPED *o;
    EXPECT_EQ(GetQueuedCompletionStatus(port, &n, &k, &o, 1000), TRUE);
    EXPECT_EQ(n, 5); EXPECT_EQ(k, 6); CHECK(o == NULL);
    CloseHandle(d); CloseHandle(port);
}
/* ---- object type names ---- */
static int type_name(HANDLE h, char *out, size_t cap) {
    ho_NtQueryObject_pfn q = (ho_NtQueryObject_pfn)t_proc_nt("NtQueryObject");
    union { UNICODE_STRING us; char raw[1024]; } b; ULONG len = 0;
    out[0] = 0;
    if (!q || q(h, 2, &b, sizeof b, &len) < 0) return 0;
    int n = WideCharToMultiByte(CP_ACP, 0, b.us.Buffer, b.us.Length / 2, out, (int)cap - 1, NULL, NULL);
    out[n > 0 ? n : 0] = 0;
    return 1;
}
TEST(ho_acc_object_type_names_table) {
    REQUIRE(t_proc_nt("NtQueryObject"));
    char p[MAX_PATH], buf[64], kn[80];
    t_tmpfile(p, sizeof p, "types.bin");
    HANDLE r, w; CreatePipe(&r, &w, NULL, 0);
    HANDLE tok = NULL, key = NULL, dup = NULL;
    OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok);
    snprintf(kn, sizeof kn, "Software\\k32t_%lu", GetCurrentProcessId());
    LONG rc = RegCreateKeyExA(HKEY_CURRENT_USER, kn, 0, NULL, 0, KEY_ALL_ACCESS, NULL, (PHKEY)&key, NULL);
    DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(), &dup, 0, FALSE, DUPLICATE_SAME_ACCESS);
    struct { const char *want; HANDLE h; } t[] = {
        { "Event", CreateEventA(NULL, TRUE, FALSE, NULL) }, { "Mutant", CreateMutexA(NULL, FALSE, NULL) },
        { "Semaphore", CreateSemaphoreA(NULL, 0, 1, NULL) }, { "Timer", CreateWaitableTimerA(NULL, TRUE, NULL) },
        { "File", CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, NULL) },
        { "File", r }, { "File", w },
        { "File", CreateFileA(t_tmpdir(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL) },
        { "Section", CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, NULL) },
        { "Job", CreateJobObjectA(NULL, NULL) }, { "IoCompletion", CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0) },
        { "Process", dup }, { "Thread", OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentThreadId()) },
        { "Token", tok }, { "Key", rc == 0 ? key : NULL },
        { "Event", U(CreateMemoryResourceNotification) ? U(CreateMemoryResourceNotification)(LowMemoryResourceNotification) : NULL },
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        if (!t[i].h || t[i].h == INVALID_HANDLE_VALUE) { CHECKF(0, "row %u (%s): creation failed", i, t[i].want); continue; }
        int ok = type_name(t[i].h, buf, sizeof buf);
        CHECKF(ok && !strcmp(buf, t[i].want), "row %u: type '%s' want '%s'", i, buf, t[i].want);
    }
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        if (!t[i].h || t[i].h == INVALID_HANDLE_VALUE) continue;
        if (t[i].h == key) RegCloseKey((HKEY)key); else CloseHandle(t[i].h);
    }
    if (rc == 0) RegDeleteKeyA(HKEY_CURRENT_USER, kn);
}
TEST(ho_acc_object_name_of_named_event) {
    REQUIRE(t_proc_nt("NtQueryObject"));
    char n[96]; ho_uniq(n, sizeof n, "nm");
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, n);
    ho_NtQueryObject_pfn q = (ho_NtQueryObject_pfn)t_proc_nt("NtQueryObject");
    union { UNICODE_STRING us; char raw[2048]; } b; ULONG len = 0;
    LONG st = q(e, 1, &b, sizeof b, &len);
    CHECK(st >= 0);
    if (st >= 0) {
        char nm[512]; int k = WideCharToMultiByte(CP_ACP, 0, b.us.Buffer, b.us.Length / 2, nm, sizeof nm - 1, NULL, NULL); nm[k > 0 ? k : 0] = 0;
        t_note("object path: %s", nm);
        size_t ln = strlen(n), lm = strlen(nm);
        CHECK(lm > ln + 1 && !strcmp(nm + lm - ln, n) && nm[lm - ln - 1] == '\\');
        CHECK(strstr(nm, "BaseNamedObjects") != NULL);
    }
    CloseHandle(e);
}
TEST(ho_acc_unnamed_object_has_empty_name) {
    REQUIRE(t_proc_nt("NtQueryObject"));
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    ho_NtQueryObject_pfn q = (ho_NtQueryObject_pfn)t_proc_nt("NtQueryObject");
    union { UNICODE_STRING us; char raw[2048]; } b; ULONG len = 0;
    LONG st = q(e, 1, &b, sizeof b, &len);
    CHECK(st >= 0);
    if (st >= 0) EXPECT_EQ(b.us.Length, 0);
    CloseHandle(e);
}
