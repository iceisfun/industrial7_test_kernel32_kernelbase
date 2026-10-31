#include "so.h"

/* ---- helpers ---- */
static HANDLE xp_spawn(const char *child, const char *args, BOOL inherit, PROCESS_INFORMATION *pi) {
    return t_spawn_child(child, args, 0, NULL, NULL, NULL, inherit, NULL, pi);
}
/* wait (bounded) for process exit, kill the child on timeout; returns exit code or 0xDEAD0001 */
static DWORD xp_finish(HANDLE hp, DWORD ms) {
    DWORD code = 0xDEAD0001;
    if (WaitForSingleObject(hp, ms) == WAIT_OBJECT_0) GetExitCodeProcess(hp, &code);
    else { TerminateProcess(hp, 1); WaitForSingleObject(hp, 5000); }
    return code;
}
static HANDLE xp_hex(const char *s) { return (HANDLE)(ULONG_PTR)strtoull(s, NULL, 16); }

/* ---------- named event, child signals ---------- */
CHILD(xp_set_named_event) {
    HANDLE e = U(OpenEventA)(EVENT_MODIFY_STATE, FALSE, argv[1]);
    if (!e) return 100 + (int)GetLastError();
    return U(SetEvent)(e) ? 0 : 2;
}
TEST(xp_named_event_set_by_child) {
    REQUIRE(U(CreateEventA)); REQUIRE(U(OpenEventA)); REQUIRE(U(SetEvent));
    char n[80]; so_name(n, sizeof n, "xpev");
    HANDLE e = U(CreateEventA)(NULL, TRUE, FALSE, n);
    CHECK(e != NULL); if (!e) return;
    EXPECT_EQ(WaitForSingleObject(e, 0), WAIT_TIMEOUT);
    EXPECT_EQ(t_run_child("xp_set_named_event", n, 30000), 0);
    EXPECT_EQ(WaitForSingleObject(e, 0), WAIT_OBJECT_0);
    CloseHandle(e);
}
TEST(xp_named_event_auto_reset_consumed_after_child_set) {
    REQUIRE(U(CreateEventA)); REQUIRE(U(OpenEventA));
    char n[80]; so_name(n, sizeof n, "xpev2");
    HANDLE e = U(CreateEventA)(NULL, FALSE, FALSE, n);
    CHECK(e != NULL); if (!e) return;
    EXPECT_EQ(t_run_child("xp_set_named_event", n, 30000), 0);
    EXPECT_EQ(WaitForSingleObject(e, 0), WAIT_OBJECT_0);
    EXPECT_EQ(WaitForSingleObject(e, 0), WAIT_TIMEOUT);
    CloseHandle(e);
}
CHILD(xp_open_missing_event) {
    SetLastError(0);
    HANDLE e = U(OpenEventA)(SYNCHRONIZE, FALSE, argv[1]);
    if (e) { CloseHandle(e); return 1; }
    return GetLastError() == ERROR_FILE_NOT_FOUND ? 0 : 2;
}
TEST(xp_named_event_gone_after_creator_closes) {
    REQUIRE(U(CreateEventA));
    char n[80]; so_name(n, sizeof n, "xpev3");
    HANDLE e = U(CreateEventA)(NULL, TRUE, FALSE, n);
    EXPECT_EQ(t_run_child("xp_set_named_event", n, 30000), 0);
    CloseHandle(e);
    EXPECT_EQ(t_run_child("xp_open_missing_event", n, 30000), 0);
}
/* ---------- named event, parent signals, child waits ---------- */
CHILD(xp_wait_named_event) {
    HANDLE ready = U(OpenEventA)(EVENT_MODIFY_STATE, FALSE, argv[2]);
    HANDLE e = U(OpenEventA)(SYNCHRONIZE, FALSE, argv[1]);
    if (!e || !ready) return 100;
    SetEvent(ready);
    DWORD r = U(WaitForSingleObject)(e, 15000);
    return r == WAIT_OBJECT_0 ? 0 : (int)r;
}
TEST(xp_child_waits_on_event_set_by_parent) {
    REQUIRE(U(CreateEventA)); REQUIRE(U(SetEvent));
    char n[80], rn[80], args[200]; so_name(n, sizeof n, "xpw"); so_name(rn, sizeof rn, "xpr");
    HANDLE e = U(CreateEventA)(NULL, TRUE, FALSE, n), ready = U(CreateEventA)(NULL, TRUE, FALSE, rn);
    CHECK(e && ready); if (!e || !ready) return;
    snprintf(args, sizeof args, "%s %s", n, rn);
    PROCESS_INFORMATION pi; HANDLE hp = xp_spawn("xp_wait_named_event", args, FALSE, &pi);
    CHECK(hp != NULL); if (!hp) return;
    EXPECT_EQ(WaitForSingleObject(ready, 20000), WAIT_OBJECT_0);
    Sleep(100);
    EXPECT_EQ(WaitForSingleObject(hp, 100), WAIT_TIMEOUT);    /* child still blocked */
    U(SetEvent)(e);
    EXPECT_EQ(xp_finish(hp, 20000), 0);
    CloseHandle(pi.hThread); CloseHandle(hp); CloseHandle(e); CloseHandle(ready);
}
TEST(xp_auto_reset_event_wakes_exactly_one_of_two_children) {
    REQUIRE(U(CreateEventA)); REQUIRE(U(SetEvent));
    char n[80], rn[80], args[200]; so_name(n, sizeof n, "xpw2"); so_name(rn, sizeof rn, "xpr2");
    HANDLE e = U(CreateEventA)(NULL, FALSE, FALSE, n);
    HANDLE ready = CreateSemaphoreA(NULL, 0, 2, rn);
    CHECK(e && ready); if (!e || !ready) return;
    /* children signal readiness through a named *event*; reuse xp_wait_named_event by giving each its own ready name */
    char rn1[80], rn2[80]; so_name(rn1, sizeof rn1, "xpr2a"); so_name(rn2, sizeof rn2, "xpr2b");
    HANDLE r1 = U(CreateEventA)(NULL, TRUE, FALSE, rn1), r2 = U(CreateEventA)(NULL, TRUE, FALSE, rn2);
    PROCESS_INFORMATION p1, p2;
    snprintf(args, sizeof args, "%s %s", n, rn1); HANDLE h1 = xp_spawn("xp_wait_named_event", args, FALSE, &p1);
    snprintf(args, sizeof args, "%s %s", n, rn2); HANDLE h2 = xp_spawn("xp_wait_named_event", args, FALSE, &p2);
    CHECK(h1 && h2);
    if (h1 && h2) {
        EXPECT_EQ(WaitForSingleObject(r1, 20000), WAIT_OBJECT_0); EXPECT_EQ(WaitForSingleObject(r2, 20000), WAIT_OBJECT_0);
        Sleep(200);
        U(SetEvent)(e);
        HANDLE both[2] = { h1, h2 };
        DWORD w = WaitForMultipleObjects(2, both, FALSE, 10000);
        CHECK(w == WAIT_OBJECT_0 || w == WAIT_OBJECT_0 + 1);
        DWORD other = (w == WAIT_OBJECT_0) ? 1 : 0;
        EXPECT_EQ(WaitForSingleObject(both[other], 300), WAIT_TIMEOUT);   /* the second child is still waiting */
        U(SetEvent)(e);
        EXPECT_EQ(WaitForSingleObject(both[other], 10000), WAIT_OBJECT_0);
    }
    if (h1) { xp_finish(h1, 5000); CloseHandle(p1.hThread); CloseHandle(h1); }
    if (h2) { xp_finish(h2, 5000); CloseHandle(p2.hThread); CloseHandle(h2); }
    CloseHandle(e); CloseHandle(ready); if (r1) CloseHandle(r1); if (r2) CloseHandle(r2);
}
/* ---------- mutex ---------- */
CHILD(xp_mutex_acquire_exit) {
    /* acquire the named mutex and exit without releasing */
    HANDLE m = U(OpenMutexA)(SYNCHRONIZE, FALSE, argv[1]);
    if (!m) return 100;
    DWORD r = U(WaitForSingleObject)(m, 10000);
    return (r == WAIT_OBJECT_0 || r == WAIT_ABANDONED) ? 0 : 1;
}
TEST(xp_mutex_abandoned_when_owner_process_exits) {
    REQUIRE(U(CreateMutexA)); REQUIRE(U(OpenMutexA)); REQUIRE(U(ReleaseMutex));
    char n[80]; so_name(n, sizeof n, "xpmx");
    HANDLE m = U(CreateMutexA)(NULL, FALSE, n);
    CHECK(m != NULL); if (!m) return;
    EXPECT_EQ(t_run_child("xp_mutex_acquire_exit", n, 30000), 0);
    EXPECT_EQ(WaitForSingleObject(m, 5000), WAIT_ABANDONED_0);
    EXPECT_EQ(U(ReleaseMutex)(m), TRUE);
    EXPECT_EQ(WaitForSingleObject(m, 0), WAIT_OBJECT_0);       /* abandoned state consumed */
    U(ReleaseMutex)(m);
    CloseHandle(m);
}
CHILD(xp_mutex_hold_sleep) {
    HANDLE m = U(OpenMutexA)(SYNCHRONIZE, FALSE, argv[1]);
    HANDLE ready = U(OpenEventA)(EVENT_MODIFY_STATE, FALSE, argv[2]);
    if (!m || !ready) return 100;
    if (U(WaitForSingleObject)(m, 10000) != WAIT_OBJECT_0) return 101;
    SetEvent(ready);
    Sleep(25000);     /* killed by the parent */
    return 0;
}
TEST(xp_mutex_abandoned_when_owner_process_terminated) {
    REQUIRE(U(CreateMutexA)); REQUIRE(U(ReleaseMutex));
    char n[80], rn[80], args[200]; so_name(n, sizeof n, "xpmx2"); so_name(rn, sizeof rn, "xpmr2");
    HANDLE m = U(CreateMutexA)(NULL, FALSE, n), ready = CreateEventA(NULL, TRUE, FALSE, rn);
    CHECK(m && ready); if (!m || !ready) return;
    snprintf(args, sizeof args, "%s %s", n, rn);
    PROCESS_INFORMATION pi; HANDLE hp = xp_spawn("xp_mutex_hold_sleep", args, FALSE, &pi);
    CHECK(hp != NULL); if (!hp) return;
    EXPECT_EQ(WaitForSingleObject(ready, 20000), WAIT_OBJECT_0);
    EXPECT_EQ(WaitForSingleObject(m, 100), WAIT_TIMEOUT);     /* held by the child */
    SET_ERR(0); EXPECT_EQ(U(ReleaseMutex)(m), FALSE); EXPECT_ERR(ERROR_NOT_OWNER);
    TerminateProcess(hp, 9);                                  /* a process this test created */
    WaitForSingleObject(hp, 10000);
    EXPECT_EQ(WaitForSingleObject(m, 5000), WAIT_ABANDONED_0);
    U(ReleaseMutex)(m);
    CloseHandle(pi.hThread); CloseHandle(hp); CloseHandle(m); CloseHandle(ready);
}
TEST(xp_mutex_owned_by_parent_blocks_child) {
    REQUIRE(U(CreateMutexA)); REQUIRE(U(ReleaseMutex));
    char n[80]; so_name(n, sizeof n, "xpmx3");
    HANDLE m = U(CreateMutexA)(NULL, TRUE, n);
    CHECK(m != NULL); if (!m) return;
    PROCESS_INFORMATION pi; HANDLE hp = xp_spawn("xp_mutex_acquire_exit", n, FALSE, &pi);
    CHECK(hp != NULL); if (!hp) return;
    EXPECT_EQ(WaitForSingleObject(hp, 700), WAIT_TIMEOUT);    /* child blocked on our mutex */
    U(ReleaseMutex)(m);
    EXPECT_EQ(xp_finish(hp, 20000), 0);
    EXPECT_EQ(WaitForSingleObject(m, 5000), WAIT_ABANDONED_0); /* child died holding it */
    U(ReleaseMutex)(m);
    CloseHandle(pi.hThread); CloseHandle(hp); CloseHandle(m);
}
/* ---------- semaphore ---------- */
CHILD(xp_sem_release) {
    HANDLE s = U(OpenSemaphoreA)(SEMAPHORE_MODIFY_STATE, FALSE, argv[1]);
    if (!s) return 100;
    LONG prev = -1;
    if (!U(ReleaseSemaphore)(s, atoi(argv[2]), &prev)) return 101 + (int)GetLastError();
    return (int)prev;
}
TEST(xp_semaphore_released_by_child) {
    REQUIRE(U(CreateSemaphoreA)); REQUIRE(U(ReleaseSemaphore));
    char n[80], args[120]; so_name(n, sizeof n, "xpsm");
    HANDLE s = U(CreateSemaphoreA)(NULL, 1, 10, n);
    CHECK(s != NULL); if (!s) return;
    snprintf(args, sizeof args, "%s 3", n);
    EXPECT_EQ(t_run_child("xp_sem_release", args, 30000), 1);   /* exit code = previous count */
    int got = 0; while (WaitForSingleObject(s, 0) == WAIT_OBJECT_0 && got < 10) got++;
    EXPECT_EQ(got, 4);
    CloseHandle(s);
}
TEST(xp_semaphore_child_overflow_reports_too_many_posts) {
    REQUIRE(U(CreateSemaphoreA)); REQUIRE(U(ReleaseSemaphore));
    char n[80], args[120]; so_name(n, sizeof n, "xpsm2");
    HANDLE s = U(CreateSemaphoreA)(NULL, 2, 3, n);
    CHECK(s != NULL); if (!s) return;
    snprintf(args, sizeof args, "%s 5", n);
    EXPECT_EQ(t_run_child("xp_sem_release", args, 30000), 101 + ERROR_TOO_MANY_POSTS);
    CloseHandle(s);
}
CHILD(xp_sem_take) {
    HANDLE s = U(OpenSemaphoreA)(SYNCHRONIZE, FALSE, argv[1]);
    if (!s) return 100;
    int n = atoi(argv[2]), got = 0;
    for (int i = 0; i < n; i++) if (U(WaitForSingleObject)(s, 5000) == WAIT_OBJECT_0) got++;
    return got;
}
TEST(xp_semaphore_taken_by_child) {
    REQUIRE(U(CreateSemaphoreA)); REQUIRE(U(ReleaseSemaphore));
    char n[80], args[120]; so_name(n, sizeof n, "xpsm3");
    HANDLE s = U(CreateSemaphoreA)(NULL, 5, 5, n);
    CHECK(s != NULL); if (!s) return;
    snprintf(args, sizeof args, "%s 3", n);
    EXPECT_EQ(t_run_child("xp_sem_take", args, 30000), 3);
    LONG prev = -1; EXPECT_EQ(U(ReleaseSemaphore)(s, 1, &prev), TRUE); EXPECT_EQ(prev, 2);
    CloseHandle(s);
}
/* ---------- timer ---------- */
CHILD(xp_timer_set) {
    HANDLE t = U(OpenWaitableTimerA)(TIMER_MODIFY_STATE, FALSE, argv[1]);
    if (!t) return 100;
    LARGE_INTEGER d; d.QuadPart = -10000 * 30;
    return U(SetWaitableTimer)(t, &d, 0, NULL, NULL, FALSE) ? 0 : 1;
}
TEST(xp_named_timer_set_by_child_waited_by_parent) {
    REQUIRE(U(CreateWaitableTimerA)); REQUIRE(U(OpenWaitableTimerA));
    char n[80]; so_name(n, sizeof n, "xptm");
    HANDLE t = U(CreateWaitableTimerA)(NULL, TRUE, n);
    CHECK(t != NULL); if (!t) return;
    EXPECT_EQ(t_run_child("xp_timer_set", n, 30000), 0);
    EXPECT_EQ(WaitForSingleObject(t, 5000), WAIT_OBJECT_0);
    CloseHandle(t);
}
/* ---------- file mapping + WaitOnAddress across processes ---------- */
CHILD(xp_mapping_write) {
    HANDLE m = OpenFileMappingA(FILE_MAP_WRITE, FALSE, argv[1]);
    if (!m) return 100;
    volatile LONG *p = MapViewOfFile(m, FILE_MAP_WRITE, 0, 0, 4096);
    if (!p) return 101;
    p[1] = p[0] + 100;
    p[0] = 0x1234;
    UnmapViewOfFile((void *)p); CloseHandle(m);
    return 0;
}
TEST(xp_named_mapping_visible_to_child) {
    char n[80]; so_name(n, sizeof n, "xpmap");
    HANDLE m = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, n);
    CHECK(m != NULL); if (!m) return;
    volatile LONG *p = MapViewOfFile(m, FILE_MAP_WRITE, 0, 0, 4096);
    CHECK(p != NULL); if (!p) { CloseHandle(m); return; }
    p[0] = 5; p[1] = 0;
    EXPECT_EQ(t_run_child("xp_mapping_write", n, 30000), 0);
    EXPECT_EQ(p[0], 0x1234); EXPECT_EQ(p[1], 105);
    UnmapViewOfFile((void *)p); CloseHandle(m);
}
CHILD(xp_woa_waiter) {
    HANDLE m = OpenFileMappingA(FILE_MAP_WRITE, FALSE, argv[1]);
    if (!m) return 100;
    volatile LONG *p = MapViewOfFile(m, FILE_MAP_WRITE, 0, 0, 4096);
    if (!p) return 101;
    LONG undesired = 0;
    InterlockedIncrement(p + 1);                    /* "I am about to wait" */
    DWORD t0 = GetTickCount();
    while (p[0] == 0 && GetTickCount() - t0 < 15000) {
        if (!U(WaitOnAddress)(p, &undesired, sizeof undesired, 1000) && GetLastError() != ERROR_TIMEOUT) return 102;
    }
    return p[0] != 0 ? 0 : 103;
}
TEST(xp_wait_on_address_shared_memory) {
    REQUIRE(U(WaitOnAddress)); REQUIRE(U(WakeByAddressAll));
    char n[80]; so_name(n, sizeof n, "xpwoa");
    HANDLE m = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, n);
    CHECK(m != NULL); if (!m) return;
    volatile LONG *p = MapViewOfFile(m, FILE_MAP_WRITE, 0, 0, 4096);
    CHECK(p != NULL); if (!p) { CloseHandle(m); return; }
    p[0] = 0; p[1] = 0;
    PROCESS_INFORMATION pi; HANDLE hp = xp_spawn("xp_woa_waiter", n, FALSE, &pi);
    CHECK(hp != NULL);
    if (hp) {
        so_spin_until(&p[1], 1, 20000); Sleep(300);
        EXPECT_EQ(WaitForSingleObject(hp, 100), WAIT_TIMEOUT);
        p[0] = 1;
        U(WakeByAddressAll)((PVOID)p);
        DWORD rc = xp_finish(hp, 20000);
        if (rc != 0 && t_is_wine()) t_note("cross-process WaitOnAddress rc=%lu (emulator)", rc);
        else EXPECT_EQ(rc, 0);
        CloseHandle(pi.hThread); CloseHandle(hp);
    }
    UnmapViewOfFile((void *)p); CloseHandle(m);
}
/* ---------- inheritance ---------- */
CHILD(xp_inherited_ops) {
    /* argv: ev(hex) sem(hex) mtx(hex) tmr(hex) notinh(hex) */
    HANDLE ev = xp_hex(argv[1]), sem = xp_hex(argv[2]), mtx = xp_hex(argv[3]), tmr = xp_hex(argv[4]), notinh = xp_hex(argv[5]);
    int rc = 0;
    if (!U(SetEvent)(ev)) rc |= 1;
    LONG prev; if (!U(ReleaseSemaphore)(sem, 2, &prev)) rc |= 2;
    if (U(WaitForSingleObject)(mtx, 5000) != WAIT_OBJECT_0) rc |= 4;
    else U(ReleaseMutex)(mtx);
    LARGE_INTEGER d; d.QuadPart = -10000 * 20;
    if (!U(SetWaitableTimer)(tmr, &d, 0, NULL, NULL, FALSE)) rc |= 8;
    SetLastError(0);
    if (U(SetEvent)(notinh)) rc |= 16; else if (GetLastError() != ERROR_INVALID_HANDLE) rc |= 32;
    return rc;
}
TEST(xp_inherited_sync_handles_usable_in_child) {
    REQUIRE(U(CreateEventA)); REQUIRE(U(CreateSemaphoreA)); REQUIRE(U(CreateMutexA)); REQUIRE(U(CreateWaitableTimerA));
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE ev = U(CreateEventA)(&sa, TRUE, FALSE, NULL), sem = U(CreateSemaphoreA)(&sa, 0, 5, NULL);
    HANDLE mtx = U(CreateMutexA)(&sa, FALSE, NULL), tmr = U(CreateWaitableTimerA)(&sa, TRUE, NULL);
    HANDLE notinh = U(CreateEventA)(NULL, TRUE, FALSE, NULL);
    CHECK(ev && sem && mtx && tmr && notinh);
    if (!(ev && sem && mtx && tmr && notinh)) return;
    char args[400];
    snprintf(args, sizeof args, "%llx %llx %llx %llx %llx", (unsigned long long)(ULONG_PTR)ev, (unsigned long long)(ULONG_PTR)sem,
        (unsigned long long)(ULONG_PTR)mtx, (unsigned long long)(ULONG_PTR)tmr, (unsigned long long)(ULONG_PTR)notinh);
    PROCESS_INFORMATION pi; HANDLE hp = xp_spawn("xp_inherited_ops", args, TRUE, &pi);
    CHECK(hp != NULL);
    if (hp) {
        EXPECT_EQ(xp_finish(hp, 30000), 0);
        CloseHandle(pi.hThread); CloseHandle(hp);
    }
    EXPECT_EQ(WaitForSingleObject(ev, 0), WAIT_OBJECT_0);
    LONG prev = -1; EXPECT_EQ(WaitForSingleObject(sem, 0), WAIT_OBJECT_0); ReleaseSemaphore(sem, 1, &prev); EXPECT_EQ(prev, 1);
    EXPECT_EQ(WaitForSingleObject(tmr, 3000), WAIT_OBJECT_0);
    EXPECT_EQ(WaitForSingleObject(notinh, 0), WAIT_TIMEOUT);
    HANDLE all[] = { ev, sem, mtx, tmr, notinh }; for (int i = 0; i < 5; i++) CloseHandle(all[i]);
}
CHILD(xp_inherited_named_mutex_abandon) {
    /* inherits a handle to the (initially owned? no) mutex and waits on it, exiting while owning it */
    HANDLE m = xp_hex(argv[1]);
    DWORD r = U(WaitForSingleObject)(m, 10000);
    return (r == WAIT_OBJECT_0) ? 0 : (r == WAIT_ABANDONED ? 2 : 1);
}
TEST(xp_inherited_mutex_abandoned_on_child_exit) {
    REQUIRE(U(CreateMutexA)); REQUIRE(U(ReleaseMutex));
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE m = U(CreateMutexA)(&sa, FALSE, NULL);
    CHECK(m != NULL); if (!m) return;
    char args[40]; snprintf(args, sizeof args, "%llx", (unsigned long long)(ULONG_PTR)m);
    PROCESS_INFORMATION pi; HANDLE hp = xp_spawn("xp_inherited_named_mutex_abandon", args, TRUE, &pi);
    CHECK(hp != NULL);
    if (hp) { EXPECT_EQ(xp_finish(hp, 30000), 0); CloseHandle(pi.hThread); CloseHandle(hp); }
    EXPECT_EQ(WaitForSingleObject(m, 5000), WAIT_ABANDONED_0);
    U(ReleaseMutex)(m);
    CloseHandle(m);
}
CHILD(xp_noninherit_check) {
    HANDLE h = xp_hex(argv[1]);
    SetLastError(0);
    DWORD r = U(WaitForSingleObject)(h, 0);
    return (r == WAIT_FAILED && GetLastError() == ERROR_INVALID_HANDLE) ? 0 : 1;
}
TEST(xp_non_inheritable_handle_invalid_in_child) {
    REQUIRE(U(CreateEventA));
    HANDLE e = U(CreateEventA)(NULL, TRUE, TRUE, NULL);
    CHECK(e != NULL); if (!e) return;
    char args[40]; snprintf(args, sizeof args, "%llx", (unsigned long long)(ULONG_PTR)e);
    PROCESS_INFORMATION pi; HANDLE hp = xp_spawn("xp_noninherit_check", args, TRUE, &pi);   /* bInheritHandles but handle not inheritable */
    CHECK(hp != NULL);
    if (hp) { EXPECT_EQ(xp_finish(hp, 30000), 0); CloseHandle(pi.hThread); CloseHandle(hp); }
    CloseHandle(e);
}
TEST(xp_inheritable_handle_not_inherited_without_flag) {
    REQUIRE(U(CreateEventA));
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE e = U(CreateEventA)(&sa, TRUE, TRUE, NULL);
    CHECK(e != NULL); if (!e) return;
    char args[40]; snprintf(args, sizeof args, "%llx", (unsigned long long)(ULONG_PTR)e);
    PROCESS_INFORMATION pi; HANDLE hp = xp_spawn("xp_noninherit_check", args, FALSE, &pi);
    CHECK(hp != NULL);
    if (hp) { EXPECT_EQ(xp_finish(hp, 30000), 0); CloseHandle(pi.hThread); CloseHandle(hp); }
    CloseHandle(e);
}
/* ---------- DuplicateHandle into child ---------- */
CHILD(xp_dup_target) {
    /* argv: go-event name, mapping name; handle value is published in the mapping */
    HANDLE go = U(OpenEventA)(SYNCHRONIZE, FALSE, argv[1]);
    HANDLE m = OpenFileMappingA(FILE_MAP_READ, FALSE, argv[2]);
    if (!go || !m) return 100;
    volatile ULONG_PTR *p = MapViewOfFile(m, FILE_MAP_READ, 0, 0, 4096);
    if (!p) return 101;
    if (U(WaitForSingleObject)(go, 15000) != WAIT_OBJECT_0) return 102;
    HANDLE ev = (HANDLE)p[0], sem = (HANDLE)p[1];
    int rc = 0;
    if (!U(SetEvent)(ev)) rc |= 1;
    if (!U(ReleaseSemaphore)(sem, 1, NULL)) rc |= 2;
    return rc;
}
TEST(xp_duplicate_handles_into_child) {
    REQUIRE(U(CreateEventA)); REQUIRE(U(CreateSemaphoreA)); REQUIRE(U(SetEvent));
    char gn[80], mn[80], args[200]; so_name(gn, sizeof gn, "xpgo"); so_name(mn, sizeof mn, "xpdm");
    HANDLE go = U(CreateEventA)(NULL, TRUE, FALSE, gn);
    HANDLE map = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, mn);
    HANDLE ev = U(CreateEventA)(NULL, TRUE, FALSE, NULL), sem = U(CreateSemaphoreA)(NULL, 0, 2, NULL);
    CHECK(go && map && ev && sem); if (!(go && map && ev && sem)) return;
    volatile ULONG_PTR *p = MapViewOfFile(map, FILE_MAP_WRITE, 0, 0, 4096);
    snprintf(args, sizeof args, "%s %s", gn, mn);
    PROCESS_INFORMATION pi; HANDLE hp = xp_spawn("xp_dup_target", args, FALSE, &pi);
    CHECK(hp != NULL && p != NULL);
    if (hp && p) {
        HANDLE dev = NULL, dsem = NULL;
        CHECK(DuplicateHandle(GetCurrentProcess(), ev, hp, &dev, 0, FALSE, DUPLICATE_SAME_ACCESS));
        CHECK(DuplicateHandle(GetCurrentProcess(), sem, hp, &dsem, 0, FALSE, DUPLICATE_SAME_ACCESS));
        p[0] = (ULONG_PTR)dev; p[1] = (ULONG_PTR)dsem;
        U(SetEvent)(go);
        EXPECT_EQ(xp_finish(hp, 30000), 0);
        EXPECT_EQ(WaitForSingleObject(ev, 0), WAIT_OBJECT_0);
        EXPECT_EQ(WaitForSingleObject(sem, 0), WAIT_OBJECT_0);
        CloseHandle(pi.hThread); CloseHandle(hp);
    }
    if (p) UnmapViewOfFile((void *)p);
    CloseHandle(go); CloseHandle(map); CloseHandle(ev); CloseHandle(sem);
}
/* ---------- access control across processes ---------- */
CHILD(xp_open_denied) {
    SetLastError(0);
    HANDLE e = U(OpenEventA)(SYNCHRONIZE, FALSE, argv[1]);
    if (e) { CloseHandle(e); return 1; }
    return GetLastError() == ERROR_ACCESS_DENIED ? 0 : 2;
}
TEST(xp_empty_dacl_denies_other_process) {
    REQUIRE(U(CreateEventA));
    char n[80]; so_name(n, sizeof n, "xpdeny");
    so_denysa d; SECURITY_ATTRIBUTES *sa = so_deny(&d, FALSE);
    HANDLE e = U(CreateEventA)(sa, TRUE, FALSE, n);
    CHECK(e != NULL); if (!e) return;
    DWORD rc = t_run_child("xp_open_denied", n, 30000);
    if (rc != 0 && t_is_wine()) t_note("empty DACL not enforced by emulator (rc=%lu)", rc); else EXPECT_EQ(rc, 0);
    CloseHandle(e);
}
/* ---------- SignalObjectAndWait ping-pong ---------- */
CHILD(xp_pingpong) {
    HANDLE ping = U(OpenEventA)(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, argv[1]), pong = U(OpenEventA)(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, argv[2]);
    if (!ping || !pong) return 100;
    for (int i = 0; i < 30; i++) {
        if (U(WaitForSingleObject)(ping, 10000) != WAIT_OBJECT_0) return 101;
        if (!U(SetEvent)(pong)) return 102;
    }
    return 0;
}
TEST(xp_signal_object_and_wait_with_child) {
    REQUIRE(U(CreateEventA)); REQUIRE(U(SignalObjectAndWait));
    char a[80], b[80], args[200]; so_name(a, sizeof a, "xppa"); so_name(b, sizeof b, "xppb");
    HANDLE ping = U(CreateEventA)(NULL, FALSE, FALSE, a), pong = U(CreateEventA)(NULL, FALSE, FALSE, b);
    CHECK(ping && pong); if (!ping || !pong) return;
    snprintf(args, sizeof args, "%s %s", a, b);
    PROCESS_INFORMATION pi; HANDLE hp = xp_spawn("xp_pingpong", args, FALSE, &pi);
    CHECK(hp != NULL);
    if (hp) {
        int ok = 0;
        for (int i = 0; i < 30; i++) if (U(SignalObjectAndWait)(ping, pong, 10000, FALSE) == WAIT_OBJECT_0) ok++;
        EXPECT_EQ(ok, 30);
        EXPECT_EQ(xp_finish(hp, 20000), 0);
        CloseHandle(pi.hThread); CloseHandle(hp);
    }
    CloseHandle(ping); CloseHandle(pong);
}
/* ---------- misc ---------- */
CHILD(xp_local_prefix_open) {
    char loc[200]; snprintf(loc, sizeof loc, "Local\\%s", argv[1]);
    HANDLE e = U(OpenEventA)(SYNCHRONIZE, FALSE, loc);
    if (!e) return 100;
    HANDLE g = U(OpenEventA)(SYNCHRONIZE, FALSE, argv[1]);   /* unprefixed == Local */
    if (!g) return 101;
    return U(WaitForSingleObject)(e, 0) == WAIT_OBJECT_0 ? 0 : 1;
}
TEST(xp_local_namespace_shared_within_session) {
    REQUIRE(U(CreateEventA));
    char n[80], loc[100]; so_name(n, sizeof n, "xploc"); snprintf(loc, sizeof loc, "Local\\%s", n);
    HANDLE e = U(CreateEventA)(NULL, TRUE, TRUE, loc);
    CHECK(e != NULL); if (!e) return;
    EXPECT_EQ(t_run_child("xp_local_prefix_open", n, 30000), 0);
    CloseHandle(e);
}
CHILD(xp_global_prefix_open) {
    char g[200]; snprintf(g, sizeof g, "Global\\%s", argv[1]);
    HANDLE e = U(OpenEventA)(SYNCHRONIZE, FALSE, g);
    if (!e) return 100;
    return U(WaitForSingleObject)(e, 0) == WAIT_OBJECT_0 ? 0 : 1;
}
TEST(xp_global_namespace_visible_to_child) {
    REQUIRE(U(CreateEventA));
    char n[80], g[100]; so_name(n, sizeof n, "xpglob"); snprintf(g, sizeof g, "Global\\%s", n);
    HANDLE e = U(CreateEventA)(NULL, TRUE, TRUE, g);
    if (!e) { t_note("Global\\ event create failed %lu", GetLastError()); return; }
    EXPECT_EQ(t_run_child("xp_global_prefix_open", n, 30000), 0);
    CloseHandle(e);
}
CHILD(xp_create_named_exit) {
    HANDLE e = U(CreateEventA)(NULL, TRUE, TRUE, argv[1]);
    return e ? 0 : 1;      /* exits without closing */
}
TEST(xp_object_destroyed_when_creator_process_exits) {
    REQUIRE(U(CreateEventA)); REQUIRE(U(OpenEventA));
    char n[80]; so_name(n, sizeof n, "xpdie");
    EXPECT_EQ(t_run_child("xp_create_named_exit", n, 30000), 0);
    SET_ERR(0);
    HANDLE e = U(OpenEventA)(SYNCHRONIZE, FALSE, n);
    CHECK(e == NULL); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    if (e) CloseHandle(e);
}
TEST(xp_object_survives_creator_exit_if_parent_holds_handle) {
    REQUIRE(U(CreateEventA)); REQUIRE(U(OpenEventA));
    char n[80]; so_name(n, sizeof n, "xpsurv");
    HANDLE keep = U(CreateEventA)(NULL, TRUE, FALSE, n);
    CHECK(keep != NULL); if (!keep) return;
    EXPECT_EQ(t_run_child("xp_set_named_event", n, 30000), 0);
    EXPECT_EQ(WaitForSingleObject(keep, 0), WAIT_OBJECT_0);
    CloseHandle(keep);
}
TEST(xp_wait_any_event_or_process) {
    REQUIRE(U(CreateEventA)); REQUIRE(U(WaitForMultipleObjects));
    HANDLE e = U(CreateEventA)(NULL, TRUE, FALSE, NULL);
    PROCESS_INFORMATION pi; HANDLE hp = xp_spawn("xp_exit_after_sleep", "400", FALSE, &pi);
    CHECK(hp != NULL); if (!hp) return;
    HANDLE hs[2] = { e, hp };
    EXPECT_EQ(U(WaitForMultipleObjects)(2, hs, FALSE, 0), WAIT_TIMEOUT);
    EXPECT_EQ(U(WaitForMultipleObjects)(2, hs, FALSE, 20000), WAIT_OBJECT_0 + 1);
    DWORD code = 0; GetExitCodeProcess(hp, &code); EXPECT_EQ(code, 21);
    CloseHandle(pi.hThread); CloseHandle(hp); CloseHandle(e);
}
CHILD(xp_exit_after_sleep) { Sleep(argc > 1 ? atoi(argv[1]) : 100); return 21; }
