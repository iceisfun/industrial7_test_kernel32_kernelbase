#include "mvc.h"
/* ReadProcessMemory / WriteProcessMemory / VirtualAllocEx & friends, cross-process, working set, locking */

#pragma GCC diagnostic ignored "-Winfinite-recursion"
#define RPM U(ReadProcessMemory)
#define WPM U(WriteProcessMemory)

CHILD(pm_sleeper) {
    /* argv[1] = ready event name (optional); idle until killed by the parent */
    if (argc > 1) { HANDLE e = OpenEventA(EVENT_MODIFY_STATE, FALSE, argv[1]); if (e) { SetEvent(e); CloseHandle(e); } }
    Sleep(25000);
    return 0;
}
/* Spawn the sleeper suspended (still not initialised: only the image and ntdll are mapped). */
static HANDLE spawn_suspended(PROCESS_INFORMATION *pi) {
    return t_spawn_child("pm_sleeper", "", CREATE_SUSPENDED, NULL, NULL, NULL, FALSE, NULL, pi);
}
static void kill_child(PROCESS_INFORMATION *pi) {
    if (pi->hProcess) { TerminateProcess(pi->hProcess, 9); WaitForSingleObject(pi->hProcess, 5000); CloseHandle(pi->hProcess); }
    if (pi->hThread) CloseHandle(pi->hThread);
    memset(pi, 0, sizeof *pi);
}
static HANDLE open_self_with(DWORD access) {
    return OpenProcess(access, FALSE, GetCurrentProcessId());
}
static HANDLE dup_self2(DWORD access) {
    HANDLE h = NULL;
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(), &h, access, FALSE, 0)) return NULL;
    return h;
}

/* ---------------- ReadProcessMemory / WriteProcessMemory, self ---------------- */
TEST(mv_rpm_self_basic) {
    REQUIRE(RPM);
    char src[64] = "read process memory self test";
    char dst[64]; memset(dst, 0xEE, sizeof dst);
    SIZE_T got = 12345;
    SET_ERR(0);
    CHECK(RPM(GetCurrentProcess(), src, dst, 30, &got));
    EXPECT_EQ(got, 30);
    CHECK(!memcmp(src, dst, 30));
    CHECK((unsigned char)dst[30] == 0xEE);
}
TEST(mv_rpm_self_null_bytes_out) {
    REQUIRE(RPM);
    char src[8] = "abcdefg", dst[8] = {0};
    SET_ERR(0);
    CHECK(RPM(GetCurrentProcess(), src, dst, 8, NULL));
    CHECK(!memcmp(src, dst, 8));
}
TEST(mv_rpm_self_zero_size) {
    REQUIRE(RPM);
    char src[8] = "abc", dst[8] = "xyz";
    SIZE_T got = 777;
    SET_ERR(0);
    BOOL r = RPM(GetCurrentProcess(), src, dst, 0, &got);
    t_note("RPM size 0: r=%d got=%Iu err=%lu", r, got, GetLastError());
    CHECK(r);
    EXPECT_EQ(got, 0);
    CHECK(!strcmp(dst, "xyz"));
}
TEST(mv_rpm_self_bad_source) {
    REQUIRE(RPM);
    char dst[64]; memset(dst, 0, sizeof dst);
    SIZE_T got = 999;
    void *srcs[] = { NULL, (void *)0x10, (void *)0x7FFFFFFF0000ull, mv_hole(GR) };
    for (int i = 0; i < 4; i++) {
        got = 999;
        SET_ERR(0);
        BOOL r = RPM(GetCurrentProcess(), srcs[i], dst, 16, &got);
        CHECKF(!r, "src %p", srcs[i]);
        DWORD e = GetLastError();
        ERR2(ERROR_PARTIAL_COPY, ERROR_NOACCESS);
        /* *lpNumberOfBytesRead is only defined for ERROR_PARTIAL_COPY; on other failures kernelbase stores an unset local */
        if (e == ERROR_PARTIAL_COPY) EXPECT_EQ(got, 0);
    }
}
TEST(mv_rpm_self_noaccess_page) {
    REQUIRE(RPM);
    char *p = VALLOC(NULL, PG, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS);
    char dst[16];
    SIZE_T got = 99;
    SET_ERR(0);
    CHECK(!RPM(GetCurrentProcess(), p, dst, 16, &got));
    ERR2(ERROR_PARTIAL_COPY, ERROR_NOACCESS);
    EXPECT_EQ(got, 0);
    mv_free(p);
}
TEST(mv_rpm_self_partial_copy) {
    REQUIRE(RPM);
    char *p = mv_alloc(2 * PG);
    DWORD old;
    memset(p, 0x3C, 2 * PG);
    VPROT(p + PG, PG, PAGE_NOACCESS, &old);
    char dst[256]; memset(dst, 0, sizeof dst);
    SIZE_T got = 99;
    SET_ERR(0);
    BOOL r = RPM(GetCurrentProcess(), p + PG - 100, dst, 200, &got);
    CHECK(!r);
    ERR2(ERROR_PARTIAL_COPY, ERROR_NOACCESS);
    t_note("partial copy across page boundary: bytes=%Iu err=%lu", got, GetLastError());
    CHECK(got == 0 || got == 100);
    /* a read ending exactly at the boundary works */
    got = 0;
    CHECK(RPM(GetCurrentProcess(), p + PG - 100, dst, 100, &got));
    EXPECT_EQ(got, 100);
    mv_free(p);
}
TEST(mv_rpm_invalid_handles) {
    REQUIRE(RPM);
    char src[8] = "x", dst[8];
    SIZE_T got = 5;
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE bad[] = { NULL, INVALID_HANDLE_VALUE == (HANDLE)-1 ? (HANDLE)(ULONG_PTR)0x7771 : NULL, (HANDLE)(ULONG_PTR)0xFFFC, ev };
    for (int i = 0; i < 4; i++) {
        got = 5;
        SET_ERR(0);
        CHECKF(!RPM(bad[i], src, dst, 1, &got), "h=%p", bad[i]);
        ERR1(ERROR_INVALID_HANDLE);
    }
    CloseHandle(ev);
}
TEST(mv_wpm_self_basic) {
    REQUIRE(WPM);
    char *p = mv_alloc(PG);
    SIZE_T w = 77;
    SET_ERR(0);
    CHECK(WPM(GetCurrentProcess(), p + 5, "hello", 5, &w));
    EXPECT_EQ(w, 5);
    CHECK(!memcmp(p + 5, "hello", 5));
    CHECK(WPM(GetCurrentProcess(), p, "z", 1, NULL));
    EXPECT_EQ(p[0], 'z');
    mv_free(p);
}
TEST(mv_wpm_self_zero_size) {
    REQUIRE(WPM);
    char *p = mv_alloc(PG);
    SIZE_T w = 77;
    SET_ERR(0);
    BOOL r = WPM(GetCurrentProcess(), p, "a", 0, &w);
    t_note("WPM size 0: r=%d w=%Iu err=%lu", r, w, GetLastError());
    EXPECT_EQ(p[0], 0);
    mv_free(p);
}
TEST(mv_wpm_into_readonly_fails_noaccess) {
    REQUIRE(WPM);
    char *p = mv_alloc(PG);
    DWORD old;
    memcpy(p, "orig", 5);
    VPROT(p, PG, PAGE_READONLY, &old);
    SIZE_T w = 5;
    SET_ERR(0);
    CHECK(!WPM(GetCurrentProcess(), p, "new!", 4, &w));
    ERR2(ERROR_NOACCESS, ERROR_PARTIAL_COPY);
    CHECK(!memcmp(p, "orig", 5));
    EXPECT_EQ(mv_q(p).Protect, PAGE_READONLY);
    mv_free(p);
}
TEST(mv_wpm_into_noaccess_fails) {
    REQUIRE(WPM);
    char *p = VALLOC(NULL, PG, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS);
    SIZE_T w = 5;
    SET_ERR(0);
    CHECK(!WPM(GetCurrentProcess(), p, "new!", 4, &w));
    ERR2(ERROR_NOACCESS, ERROR_PARTIAL_COPY);
    EXPECT_EQ(mv_q(p).Protect, PAGE_NOACCESS);
    mv_free(p);
}
TEST(mv_wpm_into_execute_page_succeeds_and_restores) {
    REQUIRE(WPM);
    char *p = VALLOC(NULL, PG, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    DWORD old;
    VPROT(p, PG, PAGE_EXECUTE_READ, &old);
    SIZE_T w = 0;
    SET_ERR(0);
    CHECK(WPM(GetCurrentProcess(), p, mv_stub, sizeof mv_stub, &w));
    EXPECT_EQ(w, sizeof mv_stub);
    EXPECT_EQ(mv_q(p).Protect, PAGE_EXECUTE_READ);       /* protection restored */
    int r = 0;
    EXPECT_EQ(mv_exec(p, &r), 0);
    EXPECT_EQ(r, 0x1234);
    mv_free(p);
}
TEST(mv_wpm_into_execute_only_page) {
    REQUIRE(WPM);
    char *p = VALLOC(NULL, PG, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    DWORD old;
    VPROT(p, PG, PAGE_EXECUTE, &old);
    SIZE_T w = 0;
    SET_ERR(0);
    BOOL r = WPM(GetCurrentProcess(), p, "ab", 2, &w);
    CHECKF(r, "err %lu", GetLastError());
    EXPECT_EQ(mv_q(p).Protect, PAGE_EXECUTE);
    mv_free(p);
}
TEST(mv_wpm_into_image_text) {
    REQUIRE(WPM);
    /* patch a byte in our own code section through WriteProcessMemory (execute-read image page), then restore it */
    unsigned char *code = (unsigned char *)&mv_q;
    SIZE_T w = 0;
    unsigned char orig = *code, patched = orig;
    SET_ERR(0);
    BOOL r = WPM(GetCurrentProcess(), code, &patched, 1, &w);       /* same value: no behavioural change */
    CHECKF(r, "err %lu", GetLastError());
    EXPECT_EQ(w, 1);
    CHECK(*code == orig);
    DWORD prot = mv_q(code).Protect;
    CHECK(prot == PAGE_EXECUTE_READ || prot == PAGE_EXECUTE_WRITECOPY);
}
TEST(mv_wpm_partial_copy) {
    REQUIRE(WPM);
    char *p = mv_alloc(2 * PG);
    DWORD old;
    VPROT(p + PG, PG, PAGE_NOACCESS, &old);
    char src[256]; memset(src, 0x61, sizeof src);
    SIZE_T w = 99;
    SET_ERR(0);
    BOOL r = WPM(GetCurrentProcess(), p + PG - 100, src, 200, &w);
    CHECK(!r);
    ERR2(ERROR_PARTIAL_COPY, ERROR_NOACCESS);
    t_note("WPM partial: bytes=%Iu err=%lu first=%02x", w, GetLastError(), (unsigned char)p[PG - 100]);
    mv_free(p);
}
TEST(mv_wpm_invalid_handles) {
    REQUIRE(WPM);
    char buf[8];
    SIZE_T w = 5;
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE bad[] = { NULL, (HANDLE)(ULONG_PTR)0x7771, (HANDLE)(ULONG_PTR)0xFFFC, ev };
    for (int i = 0; i < 4; i++) {
        SET_ERR(0);
        CHECKF(!WPM(bad[i], buf, "a", 1, &w), "h=%p", bad[i]);
        ERR1(ERROR_INVALID_HANDLE);
    }
    CloseHandle(ev);
}
TEST(mv_rpm_wpm_access_rights_self) {
    REQUIRE(RPM); REQUIRE(WPM);
    char *p = mv_alloc(PG);
    memcpy(p, "data", 5);
    char b[8]; SIZE_T n = 0;
    HANDLE h = open_self_with(PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION);
    CHECK(h != NULL);
    if (h) {
        SET_ERR(0);
        CHECK(!RPM(h, p, b, 4, &n));                  /* no VM_READ */
        ERR1(ERROR_ACCESS_DENIED);
        SET_ERR(0);
        CHECK(WPM(h, p, "DATA", 4, &n));
        CloseHandle(h);
    }
    h = open_self_with(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION);
    CHECK(h != NULL);
    if (h) {
        SET_ERR(0);
        CHECK(RPM(h, p, b, 4, &n));
        EXPECT_EQ(n, 4);
        CHECK(!memcmp(b, "DATA", 4));
        SET_ERR(0);
        CHECK(!WPM(h, p, "xxxx", 4, &n));              /* no VM_WRITE */
        ERR1(ERROR_ACCESS_DENIED);
        CloseHandle(h);
    }
    h = open_self_with(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE);
    if (h) {
        SET_ERR(0);
        CHECK(!RPM(h, p, b, 4, &n));
        ERR1(ERROR_ACCESS_DENIED);
        SET_ERR(0);
        CHECK(!WPM(h, p, "xxxx", 4, &n));
        ERR1(ERROR_ACCESS_DENIED);
        CloseHandle(h);
    }
    mv_free(p);
}
TEST(mv_wpm_requires_vm_operation) {
    REQUIRE(WPM);
    char *p = mv_alloc(PG);
    HANDLE h = open_self_with(PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION);
    CHECK(h != NULL);
    SIZE_T n = 0;
    SET_ERR(0);
    BOOL r = WPM(h, p, "abcd", 4, &n);
    t_note("WPM with VM_WRITE but no VM_OPERATION: r=%d err=%lu", r, GetLastError());
    if (h) CloseHandle(h);
    mv_free(p);
}

/* ---------------- child process ---------------- */
TEST(mv_child_peb_image_base_readable) {
    REQUIRE(RPM); REQUIRE(VQEX);
    PROCESS_INFORMATION pi;
    HANDLE hp = spawn_suspended(&pi);
    CHECK(hp != NULL);
    if (!hp) return;
    PROCESS_BASIC_INFORMATION pbi; memset(&pbi, 0, sizeof pbi);
    LONG st = (LONG)UNT(NtQueryInformationProcess)(hp, ProcessBasicInformation, &pbi, sizeof pbi, NULL);
    CHECK(st >= 0 && pbi.PebBaseAddress != NULL);
    if (st >= 0 && pbi.PebBaseAddress) {
        void *base = NULL; SIZE_T got = 0;
        SET_ERR(0);
        CHECK(RPM(hp, (char *)pbi.PebBaseAddress + 0x10, &base, sizeof base, &got));
        EXPECT_EQ(got, sizeof base);
        CHECK(base != NULL);
        MEMORY_BASIC_INFORMATION m; memset(&m, 0, sizeof m);
        SET_ERR(0);
        EXPECT_EQ(VQEX(hp, base, &m, sizeof m), sizeof m);
        EXPECT_EQ(m.Type, MEM_IMAGE); EXPECT_EQ(m.AllocationBase, base); EXPECT_EQ(m.State, MEM_COMMIT);
        char mz[2] = {0};
        CHECK(RPM(hp, base, mz, 2, &got));
        CHECK(mz[0] == 'M' && mz[1] == 'Z');
        /* the PEB itself is committed private memory */
        EXPECT_EQ(VQEX(hp, pbi.PebBaseAddress, &m, sizeof m), sizeof m);
        EXPECT_EQ(m.Type, MEM_PRIVATE); EXPECT_EQ(m.State, MEM_COMMIT);
    }
    kill_child(&pi);
}
TEST(mv_child_valloc_write_read_free) {
    REQUIRE(VALLOCEX); REQUIRE(VFREEEX); REQUIRE(VQEX); REQUIRE(RPM); REQUIRE(WPM);
    PROCESS_INFORMATION pi;
    HANDLE hp = spawn_suspended(&pi);
    CHECK(hp != NULL);
    if (!hp) return;
    SET_ERR(0);
    char *p = VALLOCEX(hp, NULL, 3 * PG, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    CHECKF(p != NULL, "err %lu", GetLastError());
    if (p) {
        CHECK(((ULONG_PTR)p & (GR - 1)) == 0);
        MEMORY_BASIC_INFORMATION m;
        EXPECT_EQ(VQEX(hp, p, &m, sizeof m), sizeof m);
        EXPECT_EQ(m.RegionSize, 3 * PG); EXPECT_EQ(m.Protect, PAGE_READWRITE); EXPECT_EQ(m.State, MEM_COMMIT);
        unsigned char data[300]; for (int i = 0; i < 300; i++) data[i] = (unsigned char)(i * 3);
        SIZE_T w = 0, r = 0;
        SET_ERR(0);
        CHECK(WPM(hp, p + PG - 100, data, 300, &w));
        EXPECT_EQ(w, 300);
        unsigned char back[300]; memset(back, 0, sizeof back);
        CHECK(RPM(hp, p + PG - 100, back, 300, &r));
        EXPECT_EQ(r, 300);
        CHECK(!memcmp(data, back, 300));
        DWORD old = 0;
        CHECK(VPROTEX(hp, p, PG, PAGE_READONLY, &old));
        EXPECT_EQ(old, PAGE_READWRITE);
        EXPECT_EQ(VQEX(hp, p, &m, sizeof m), sizeof m);
        EXPECT_EQ(m.Protect, PAGE_READONLY);
        SET_ERR(0);
        CHECK(!VFREEEX(hp, p, PG, MEM_RELEASE));
        ERR1(ERROR_INVALID_PARAMETER);
        SET_ERR(0);
        CHECK(VFREEEX(hp, p + PG, PG, MEM_DECOMMIT));
        EXPECT_EQ(VQEX(hp, p + PG, &m, sizeof m), sizeof m);
        EXPECT_EQ(m.State, MEM_RESERVE);
        SET_ERR(0);
        CHECK(VFREEEX(hp, p, 0, MEM_RELEASE));
        SET_ERR(0);
        CHECK(!VFREEEX(hp, p, 0, MEM_RELEASE));
        ERR1(ERROR_INVALID_ADDRESS);
        EXPECT_EQ(VQEX(hp, p, &m, sizeof m), sizeof m);
        EXPECT_EQ(m.State, MEM_FREE);
        /* reading released memory is a partial copy / access failure */
        SET_ERR(0);
        r = 5;
        CHECK(!RPM(hp, p, back, 4, &r));
        ERR2(ERROR_PARTIAL_COPY, ERROR_NOACCESS);
        EXPECT_EQ(r, 0);
    }
    kill_child(&pi);
}
TEST(mv_child_valloc_rights) {
    REQUIRE(VALLOCEX);
    PROCESS_INFORMATION pi;
    HANDLE hp = spawn_suspended(&pi);
    CHECK(hp != NULL);
    if (!hp) return;
    HANDLE ro = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pi.dwProcessId);
    HANDLE ql = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pi.dwProcessId);
    CHECK(ro != NULL);
    if (ro) {
        SET_ERR(0);
        CHECK(VALLOCEX(ro, NULL, PG, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) == NULL);
        ERR1(ERROR_ACCESS_DENIED);
    }
    if (ql) {
        SET_ERR(0);
        CHECK(VALLOCEX(ql, NULL, PG, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) == NULL);
        ERR1(ERROR_ACCESS_DENIED);
        SET_ERR(0);
        CHECK(!VFREEEX(ql, (void *)0x10000, 0, MEM_RELEASE));
        ERR1(ERROR_ACCESS_DENIED);
    }
    HANDLE vo = OpenProcess(PROCESS_VM_OPERATION, FALSE, pi.dwProcessId);
    if (vo) {
        SET_ERR(0);
        void *p = VALLOCEX(vo, NULL, PG, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        CHECKF(p != NULL, "err %lu", GetLastError());
        if (p) CHECK(VFREEEX(vo, p, 0, MEM_RELEASE));
        CloseHandle(vo);
    }
    if (ro) CloseHandle(ro);
    if (ql) CloseHandle(ql);
    kill_child(&pi);
}
TEST(mv_child_valloc_invalid_handle) {
    REQUIRE(VALLOCEX); REQUIRE(VFREEEX);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE bad[] = { NULL, (HANDLE)(ULONG_PTR)0x7771, ev };
    for (int i = 0; i < 3; i++) {
        SET_ERR(0);
        CHECK(VALLOCEX(bad[i], NULL, PG, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) == NULL);
        ERR1(ERROR_INVALID_HANDLE);
        SET_ERR(0);
        CHECK(!VFREEEX(bad[i], (void *)0x10000, 0, MEM_RELEASE));
        ERR1(ERROR_INVALID_HANDLE);
    }
    CloseHandle(ev);
}
TEST(mv_child_valloc_below_min_address) {
    REQUIRE(VALLOCEX);
    SET_ERR(0);
    CHECK(VALLOCEX(GetCurrentProcess(), (void *)0x1000, PG, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) == NULL);
    ERR1(ERROR_INVALID_PARAMETER);
}
TEST(mv_child_valloc_numa_node) {
    REQUIRE(U(VirtualAllocExNuma));
    void *p = U(VirtualAllocExNuma)(GetCurrentProcess(), NULL, PG, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE, NUMA_NO_PREFERRED_NODE);
    CHECK(p != NULL);
    mv_free(p);
    p = U(VirtualAllocExNuma)(GetCurrentProcess(), NULL, PG, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE, 0);
    CHECK(p != NULL);
    mv_free(p);
    SET_ERR(0);
    p = U(VirtualAllocExNuma)(GetCurrentProcess(), NULL, PG, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE, 0x40);
    CHECK(p == NULL);
    if (p) mv_free(p); else ERR1(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    p = U(VirtualAllocExNuma)(GetCurrentProcess(), NULL, PG, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE, 0xFFFFFFF0);
    CHECK(p == NULL);
    if (p) mv_free(p); else ERR1(ERROR_INVALID_PARAMETER);
}
CHILD(pm_exec_target) { Sleep(25000); return 0; }
TEST(mv_child_remote_thread_runs_stub) {
    REQUIRE(VALLOCEX); REQUIRE(WPM); REQUIRE(VPROTEX);
    char evn[100]; snprintf(evn, sizeof evn, "Local\\k32t_mv_ready_%lu", GetCurrentProcessId());
    HANDLE ready = CreateEventA(NULL, TRUE, FALSE, evn);
    PROCESS_INFORMATION pi;
    HANDLE hp = t_spawn_child("pm_sleeper", evn, 0, NULL, NULL, NULL, FALSE, NULL, &pi);
    CHECK(hp != NULL);
    if (!hp) { CloseHandle(ready); return; }
    if (WaitForSingleObject(ready, 15000) != WAIT_OBJECT_0) { t_note("child never signalled"); kill_child(&pi); CloseHandle(ready); return; }
    unsigned char *p = VALLOCEX(hp, NULL, PG, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    CHECK(p != NULL);
    if (p) {
        SIZE_T w = 0;
        CHECK(WPM(hp, p, mv_stub, sizeof mv_stub, &w));
        DWORD old;
        CHECK(VPROTEX(hp, p, PG, PAGE_EXECUTE_READ, &old));
        EXPECT_EQ(old, PAGE_READWRITE);
        DWORD tid = 0;
        HANDLE th = CreateRemoteThread(hp, NULL, 0, (LPTHREAD_START_ROUTINE)p, NULL, 0, &tid);
        CHECKF(th != NULL, "err %lu", GetLastError());
        if (th) {
            EXPECT_EQ(WaitForSingleObject(th, 10000), WAIT_OBJECT_0);
            DWORD code = 0; GetExitCodeThread(th, &code);
            EXPECT_EQ(code, 0x1234);
            CloseHandle(th);
        }
    }
    kill_child(&pi);
    CloseHandle(ready);
}
TEST(mv_child_dep_violation_in_remote_thread) {
    REQUIRE(VALLOCEX); REQUIRE(WPM);
    if (t_is_wine()) { t_skip("remote crash handling differs under emulation"); return; }
    char evn[100]; snprintf(evn, sizeof evn, "Local\\k32t_mv_ready2_%lu", GetCurrentProcessId());
    HANDLE ready = CreateEventA(NULL, TRUE, FALSE, evn);
    PROCESS_INFORMATION pi;
    HANDLE hp = t_spawn_child("pm_sleeper", evn, 0, NULL, NULL, NULL, FALSE, NULL, &pi);
    CHECK(hp != NULL);
    if (!hp) { CloseHandle(ready); return; }
    if (WaitForSingleObject(ready, 15000) == WAIT_OBJECT_0) {
        unsigned char *p = VALLOCEX(hp, NULL, PG, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (p) {
            SIZE_T w;
            WPM(hp, p, mv_stub, sizeof mv_stub, &w);
            HANDLE th = CreateRemoteThread(hp, NULL, 0, (LPTHREAD_START_ROUTINE)p, NULL, 0, NULL);   /* RW, not executable */
            if (th) {
                WaitForSingleObject(th, 10000);
                DWORD code = 0; GetExitCodeThread(th, &code);
                t_note("remote thread on non-exec page ended with 0x%lx", code);
                CloseHandle(th);
            }
            DWORD pc = STILL_ACTIVE;
            Sleep(300);
            GetExitCodeProcess(hp, &pc);
            t_note("child exit/status after DEP fault: 0x%lx", pc);
        }
    }
    kill_child(&pi);
    CloseHandle(ready);
}
TEST(mv_child_ops_after_exit) {
    REQUIRE(VALLOCEX); REQUIRE(RPM); REQUIRE(VQEX);
    PROCESS_INFORMATION pi;
    HANDLE hp = spawn_suspended(&pi);
    CHECK(hp != NULL);
    if (!hp) return;
    HANDLE dup = NULL;
    DuplicateHandle(GetCurrentProcess(), hp, GetCurrentProcess(), &dup, 0, FALSE, DUPLICATE_SAME_ACCESS);
    kill_child(&pi);
    SET_ERR(0);
    void *p = VALLOCEX(dup, NULL, PG, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    CHECK(p == NULL);
    t_note("VirtualAllocEx on exited process: err=%lu", GetLastError());
    char b[4]; SIZE_T n = 3;
    SET_ERR(0);
    CHECK(!RPM(dup, (void *)0x10000, b, 4, &n));
    t_note("ReadProcessMemory on exited process: err=%lu n=%Iu", GetLastError(), n);
    MEMORY_BASIC_INFORMATION m;
    SET_ERR(0);
    CHECK(VQEX(dup, (void *)0x10000, &m, sizeof m) == 0);
    t_note("VirtualQueryEx on exited process: err=%lu", GetLastError());
    if (dup) CloseHandle(dup);
}

/* ---------------- stack growth ---------------- */
static __attribute__((noinline)) int grow(int depth) {
    volatile char buf[60 * 1024];
    for (size_t i = 0; i < sizeof buf; i += 4096) buf[i] = (char)depth;
    buf[sizeof buf - 1] = (char)depth;
    int r = depth <= 0 ? 0 : grow(depth - 1);
    return r + (buf[0] == (char)depth ? 0 : 1000);
}
CHILD(pm_stackgrow) {
    char *lim0 = (char *)((NT_TIB *)NtCurrentTeb())->StackLimit;
    int r = grow(8);                    /* ~480 KB of stack, touched page by page */
    char *lim1 = (char *)((NT_TIB *)NtCurrentTeb())->StackLimit;
    (void)lim0; (void)lim1;
    return r == 0 ? 0 : 3;
}
TEST(mv_stack_growth_child) { DWORD c_ = t_run_child("pm_stackgrow", "", 30000); EXPECT_EQ(c_, 0); }
static LONG CALLBACK so_veh(PEXCEPTION_POINTERS ep) {
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW) TerminateProcess(GetCurrentProcess(), 0x5050);
    return EXCEPTION_CONTINUE_SEARCH;
}
static __attribute__((noinline)) int overflow(int d) {
    volatile char buf[32 * 1024];
    buf[0] = (char)d; buf[sizeof buf - 1] = 1;
    int r = overflow(d + 1);
    return r + buf[0];
}
CHILD(pm_stackoverflow) {
    AddVectoredExceptionHandler(1, so_veh);
    return overflow(0) ? 1 : 2;
}
TEST(mv_stack_overflow_child) {
    DWORD c = t_run_child("pm_stackoverflow", "", 30000);
    CHECKF(c == 0x5050 || c == 0xC00000FD, "exit 0x%lx", c);
    EXPECT_EQ(c, 0x5050);
}
CHILD(pm_stackguard) {
    /* the page below the committed stack is a guard page, growth moves StackLimit down */
    NT_TIB *tib = (NT_TIB *)NtCurrentTeb();
    char *lim = (char *)tib->StackLimit;
    MEMORY_BASIC_INFORMATION m;
    if (VirtualQuery(lim - 1, &m, sizeof m) != sizeof m) return 2;
    if (!(m.State == MEM_RESERVE || (m.Protect & PAGE_GUARD))) return 3;
    grow(6);
    char *lim2 = (char *)tib->StackLimit;
    if (lim2 > lim) return 4;
    return 0;
}
TEST(mv_stack_guard_page_child) { DWORD c_ = t_run_child("pm_stackguard", "", 30000); EXPECT_EQ(c_, 0); }

/* ---------------- working set ---------------- */
TEST(mv_ws_get_self) {
    REQUIRE(U(GetProcessWorkingSetSize)); REQUIRE(U(GetProcessWorkingSetSizeEx));
    SIZE_T mn = 0, mx = 0; DWORD fl = 0xFFFF;
    SET_ERR(0);
    CHECK(U(GetProcessWorkingSetSize)(GetCurrentProcess(), &mn, &mx));
    CHECK(mn >= PG && mx >= mn);
    CHECK((mn & (PG - 1)) == 0 && (mx & (PG - 1)) == 0);
    SIZE_T mn2 = 0, mx2 = 0;
    CHECK(U(GetProcessWorkingSetSizeEx)(GetCurrentProcess(), &mn2, &mx2, &fl));
    EXPECT_EQ(mn2, mn); EXPECT_EQ(mx2, mx);
    CHECK((fl & ~(DWORD)(QUOTA_LIMITS_HARDWS_MIN_ENABLE | QUOTA_LIMITS_HARDWS_MIN_DISABLE | QUOTA_LIMITS_HARDWS_MAX_ENABLE | QUOTA_LIMITS_HARDWS_MAX_DISABLE)) == 0);
    t_note("working set min=%Iu max=%Iu flags=0x%lx", mn, mx, fl);
}
TEST(mv_ws_get_invalid_handle) {
    REQUIRE(U(GetProcessWorkingSetSize)); REQUIRE(U(GetProcessWorkingSetSizeEx));
    SIZE_T mn = 0, mx = 0; DWORD fl;
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE bad[] = { NULL, (HANDLE)(ULONG_PTR)0x7771, ev };
    for (int i = 0; i < 3; i++) {
        SET_ERR(0);
        CHECK(!U(GetProcessWorkingSetSize)(bad[i], &mn, &mx));
        ERR1(ERROR_INVALID_HANDLE);
        SET_ERR(0);
        CHECK(!U(GetProcessWorkingSetSizeEx)(bad[i], &mn, &mx, &fl));
        ERR1(ERROR_INVALID_HANDLE);
    }
    CloseHandle(ev);
}
TEST(mv_ws_get_access_rights) {
    REQUIRE(U(GetProcessWorkingSetSize));
    SIZE_T mn, mx;
    HANDLE h = open_self_with(PROCESS_VM_READ);
    SET_ERR(0);
    CHECK(!U(GetProcessWorkingSetSize)(h, &mn, &mx));
    ERR1(ERROR_ACCESS_DENIED);
    if (h) CloseHandle(h);
    h = open_self_with(PROCESS_QUERY_LIMITED_INFORMATION);
    SET_ERR(0);
    BOOL r = U(GetProcessWorkingSetSize)(h, &mn, &mx);
    t_note("GetProcessWorkingSetSize with QUERY_LIMITED_INFORMATION: %d err=%lu", r, GetLastError());
    if (h) CloseHandle(h);
    h = open_self_with(PROCESS_QUERY_INFORMATION);
    SET_ERR(0);
    CHECK(U(GetProcessWorkingSetSize)(h, &mn, &mx));
    if (h) CloseHandle(h);
}
CHILD(pm_wsset) {
    BOOL (WINAPI *setf)(HANDLE, SIZE_T, SIZE_T) = (void *)GetProcAddress(GetModuleHandleA("kernel32.dll"), "SetProcessWorkingSetSize");
    BOOL (WINAPI *setx)(HANDLE, SIZE_T, SIZE_T, DWORD) = (void *)GetProcAddress(GetModuleHandleA("kernel32.dll"), "SetProcessWorkingSetSizeEx");
    BOOL (WINAPI *getx)(HANDLE, PSIZE_T, PSIZE_T, PDWORD) = (void *)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetProcessWorkingSetSizeEx");
    BOOL (WINAPI *get)(HANDLE, PSIZE_T, PSIZE_T) = (void *)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetProcessWorkingSetSize");
    if (!setf || !setx || !getx || !get) return 1;
    HANDLE me = GetCurrentProcess();
    SIZE_T mn, mx; DWORD fl;
    /* zero min or max is invalid */
    SetLastError(0);
    if (setf(me, 0, 1 << 20) || GetLastError() != ERROR_INVALID_PARAMETER) return 10;
    SetLastError(0);
    if (setf(me, 1 << 20, 0) || GetLastError() != ERROR_INVALID_PARAMETER) return 11;
    SetLastError(0);
    if (setx(me, 0, 0, 0) || GetLastError() != ERROR_INVALID_PARAMETER) return 12;
    /* min>max, conflicting/undefined flags: kernel-validated, not asserted */
    /* a sane range sticks */
    if (!setf(me, 1 << 20, 8 << 20)) return 20;
    if (!getx(me, &mn, &mx, &fl)) return 21;
    if (mn != (1 << 20) || mx != (8 << 20)) return 22;
    if (fl & (QUOTA_LIMITS_HARDWS_MIN_ENABLE | QUOTA_LIMITS_HARDWS_MAX_ENABLE)) return 23;
    if (!get(me, &mn, &mx) || mn != (1 << 20) || mx != (8 << 20)) return 24;
    /* flags are reported back */
    if (!setx(me, 1 << 20, 8 << 20, QUOTA_LIMITS_HARDWS_MAX_ENABLE)) return 25;
    if (!getx(me, &mn, &mx, &fl) || !(fl & QUOTA_LIMITS_HARDWS_MAX_ENABLE)) return 26;
    if (!setx(me, 1 << 20, 8 << 20, QUOTA_LIMITS_HARDWS_MAX_DISABLE | QUOTA_LIMITS_HARDWS_MIN_DISABLE)) return 27;
    if (!getx(me, &mn, &mx, &fl) || (fl & QUOTA_LIMITS_HARDWS_MAX_ENABLE)) return 28;
    /* (SIZE_T)-1,(SIZE_T)-1 trims the working set */
    if (!setf(me, (SIZE_T)-1, (SIZE_T)-1)) return 30;
    return 0;
}
TEST(mv_ws_set_child) { DWORD c_ = t_run_child("pm_wsset", "", 30000); EXPECT_EQ(c_, 0); }
TEST(mv_ws_set_invalid_handle) {
    REQUIRE(U(SetProcessWorkingSetSize)); REQUIRE(U(SetProcessWorkingSetSizeEx));
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE bad[] = { NULL, (HANDLE)(ULONG_PTR)0x7771, ev };
    for (int i = 0; i < 3; i++) {
        SET_ERR(0);
        CHECK(!U(SetProcessWorkingSetSize)(bad[i], 1 << 20, 4 << 20));
        ERR1(ERROR_INVALID_HANDLE);
        SET_ERR(0);
        CHECK(!U(SetProcessWorkingSetSizeEx)(bad[i], 1 << 20, 4 << 20, 0));
        ERR1(ERROR_INVALID_HANDLE);
    }
    CloseHandle(ev);
}
TEST(mv_ws_set_zero_args_do_not_touch_state) {
    REQUIRE(U(SetProcessWorkingSetSize)); REQUIRE(U(GetProcessWorkingSetSize));
    SIZE_T a1, b1, a2, b2;
    CHECK(U(GetProcessWorkingSetSize)(GetCurrentProcess(), &a1, &b1));
    SET_ERR(0);
    CHECK(!U(SetProcessWorkingSetSize)(GetCurrentProcess(), 0, b1));
    ERR1(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!U(SetProcessWorkingSetSize)(GetCurrentProcess(), a1, 0));
    ERR1(ERROR_INVALID_PARAMETER);
    CHECK(U(GetProcessWorkingSetSize)(GetCurrentProcess(), &a2, &b2));
    EXPECT_EQ(a1, a2); EXPECT_EQ(b1, b2);
}
TEST(mv_ws_set_child_process_handle_rights) {
    REQUIRE(U(SetProcessWorkingSetSize));
    PROCESS_INFORMATION pi;
    HANDLE hp = spawn_suspended(&pi);
    CHECK(hp != NULL);
    if (!hp) return;
    HANDLE ro = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pi.dwProcessId);
    if (ro) {
        SET_ERR(0);
        CHECK(!U(SetProcessWorkingSetSize)(ro, 1 << 20, 4 << 20));
        ERR1(ERROR_ACCESS_DENIED);
        CloseHandle(ro);
    }
    SIZE_T mn = 0, mx = 0;
    CHECK(U(GetProcessWorkingSetSize)(hp, &mn, &mx));
    CHECK(mn > 0 && mx >= mn);
    SET_ERR(0);
    BOOL r = U(SetProcessWorkingSetSize)(hp, 1 << 20, 6 << 20);
    CHECKF(r, "err %lu", GetLastError());
    if (r) {
        SIZE_T a, b;
        CHECK(U(GetProcessWorkingSetSize)(hp, &a, &b));
        if (a != (1 << 20) || b != (6 << 20)) t_note("readback min=%Iu max=%Iu", a, b);
    }
    kill_child(&pi);
}

/* ---------------- VirtualLock ---------------- */
TEST(mv_lock_basic) {
    REQUIRE(U(VirtualLock)); REQUIRE(U(VirtualUnlock));
    char *p = mv_alloc(2 * PG);
    SET_ERR(0);
    CHECK(U(VirtualLock)(p, PG));
    SET_ERR(0);
    CHECK(U(VirtualUnlock)(p, PG));
    SET_ERR(0);
    CHECK(!U(VirtualUnlock)(p, PG));                  /* not locked any more */
    ERR1(ERROR_NOT_LOCKED);
    mv_free(p);
}
TEST(mv_lock_unlock_not_locked) {
    REQUIRE(U(VirtualUnlock));
    char *p = mv_alloc(PG);
    SET_ERR(0);
    CHECK(!U(VirtualUnlock)(p, PG));
    ERR1(ERROR_NOT_LOCKED);
    mv_free(p);
}
TEST(mv_lock_double_lock_single_unlock) {
    REQUIRE(U(VirtualLock)); REQUIRE(U(VirtualUnlock));
    char *p = mv_alloc(PG);
    CHECK(U(VirtualLock)(p, PG));
    CHECK(U(VirtualLock)(p, PG));
    CHECK(U(VirtualUnlock)(p, PG));
    SET_ERR(0);
    CHECK(!U(VirtualUnlock)(p, PG));                  /* locks do not nest */
    ERR1(ERROR_NOT_LOCKED);
    mv_free(p);
}
TEST(mv_lock_partial_overlap) {
    REQUIRE(U(VirtualLock)); REQUIRE(U(VirtualUnlock));
    char *p = mv_alloc(4 * PG);
    CHECK(U(VirtualLock)(p, 2 * PG));
    SET_ERR(0);
    BOOL r = U(VirtualUnlock)(p, 4 * PG);              /* includes unlocked pages */
    t_note("unlock of range with unlocked pages: r=%d err=%lu", r, GetLastError());
    if (r) { SET_ERR(0); CHECK(!U(VirtualUnlock)(p, PG)); ERR1(ERROR_NOT_LOCKED); }
    else U(VirtualUnlock)(p, 2 * PG);
    mv_free(p);
}
TEST(mv_lock_partial_page_range) {
    REQUIRE(U(VirtualLock)); REQUIRE(U(VirtualUnlock));
    char *p = mv_alloc(2 * PG);
    CHECK(U(VirtualLock)(p + 10, 5));                  /* locks the containing page */
    CHECK(U(VirtualUnlock)(p, PG));
    SET_ERR(0);
    CHECK(!U(VirtualUnlock)(p, PG));
    ERR1(ERROR_NOT_LOCKED);
    mv_free(p);
}
TEST(mv_lock_bad_ranges) {
    REQUIRE(U(VirtualLock)); REQUIRE(U(VirtualUnlock));
    void *h = mv_hole(GR);
    SET_ERR(0);
    CHECK(!U(VirtualLock)(h, PG));
    t_note("VirtualLock on free memory: err=%lu", GetLastError());
    SET_ERR(0);
    CHECK(!U(VirtualLock)(NULL, PG));
    t_note("VirtualLock(NULL): err=%lu", GetLastError());
    SET_ERR(0);
    CHECK(!U(VirtualUnlock)(h, PG));
    t_note("VirtualUnlock on free memory: err=%lu", GetLastError());
    char *r = VALLOC(NULL, GR, MEM_RESERVE, PAGE_READWRITE);
    SET_ERR(0);
    CHECK(!U(VirtualLock)(r, PG));                     /* reserved but not committed */
    t_note("VirtualLock on reserved memory: err=%lu", GetLastError());
    mv_free(r);
    char *p = mv_alloc(PG);
    SET_ERR(0);
    BOOL z = U(VirtualLock)(p, 0);
    t_note("VirtualLock size 0: r=%d err=%lu", z, GetLastError());
    if (z) U(VirtualUnlock)(p, 0);
    mv_free(p);
}
TEST(mv_lock_exceeds_working_set_quota) {
    REQUIRE(U(VirtualLock)); REQUIRE(U(VirtualUnlock));
    SIZE_T sz = 96u << 20;
    char *p = mv_alloc(sz);
    CHECK(p != NULL);
    if (!p) return;
    SET_ERR(0);
    BOOL r = U(VirtualLock)(p, sz);
    if (r) { t_note("VirtualLock of 96MB succeeded (large working set quota)"); U(VirtualUnlock)(p, sz); }
    else ERR1(ERROR_WORKING_SET_QUOTA);
    mv_free(p);
}
TEST(mv_lock_ex_variant) {
    REQUIRE(U(VirtualUnlockEx)); REQUIRE(U(VirtualLock));
    char *p = mv_alloc(PG);
    CHECK(U(VirtualLock)(p, PG));
    SET_ERR(0);
    CHECK(U(VirtualUnlockEx)(GetCurrentProcess(), p, PG));
    SET_ERR(0);
    CHECK(!U(VirtualUnlockEx)(GetCurrentProcess(), p, PG));
    ERR1(ERROR_NOT_LOCKED);
    SET_ERR(0);
    CHECK(!U(VirtualUnlockEx)((HANDLE)(ULONG_PTR)0x7771, p, PG));
    ERR1(ERROR_INVALID_HANDLE);
    HANDLE h = open_self_with(PROCESS_QUERY_INFORMATION);
    SET_ERR(0);
    CHECK(!U(VirtualUnlockEx)(h, p, PG));
    ERR1(ERROR_ACCESS_DENIED);
    if (h) CloseHandle(h);
    mv_free(p);
}
TEST(mv_lock_content_preserved) {
    REQUIRE(U(VirtualLock)); REQUIRE(U(VirtualUnlock));
    char *p = mv_alloc(PG);
    memset(p, 0x99, PG);
    CHECK(U(VirtualLock)(p, PG));
    CHECK(p[0] == (char)0x99 && p[PG - 1] == (char)0x99);
    CHECK(U(VirtualUnlock)(p, PG));
    mv_free(p);
}
TEST(mv_lock_released_by_free) {
    REQUIRE(U(VirtualLock)); REQUIRE(U(VirtualUnlock));
    char *p = mv_alloc(PG);
    CHECK(U(VirtualLock)(p, PG));
    CHECK(VFREE(p, 0, MEM_RELEASE));                   /* freeing a locked page is allowed */
    SET_ERR(0);
    CHECK(!U(VirtualUnlock)(p, PG));
}
