/* CreateRemoteThread(Ex) into a child process we created; process-level thread behaviour. */
#include "tf.h"

CHILD(tf_sleeper) {
    HANDLE ev = (HANDLE)(ULONG_PTR)strtoull(argc > 1 ? argv[1] : "0", NULL, 16);
    if (ev) SetEvent(ev);
    Sleep(25000);
    return 3;
}

/* Spawn a sleeping child; returns process handle (full access) or NULL. */
static HANDLE spawn_sleeper(PROCESS_INFORMATION *pi) {
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    HANDLE ev = CreateEventA(&sa, TRUE, FALSE, NULL);
    char arg[32]; snprintf(arg, sizeof arg, "%llx", (unsigned long long)(ULONG_PTR)ev);
    HANDLE hp = t_spawn_child("tf_sleeper", arg, 0, NULL, NULL, NULL, TRUE, NULL, pi);
    if (!hp) { CloseHandle(ev); return NULL; }
    if (WaitForSingleObject(ev, 15000) != WAIT_OBJECT_0) { TerminateProcess(hp, 1); CloseHandle(pi->hThread); CloseHandle(hp); CloseHandle(ev); return NULL; }
    CloseHandle(ev);
    Sleep(50);
    return hp;
}
static void kill_child(HANDLE hp, PROCESS_INFORMATION *pi) {
    TerminateProcess(hp, 9);
    WaitForSingleObject(hp, 5000);
    CloseHandle(pi->hThread);
    CloseHandle(hp);
}

TEST(rthread_exitthread_code) {
    REQUIRE(U(CreateRemoteThread)); REQUIRE(U(ExitThread)); REQUIRE(U(GetProcessIdOfThread));
    PROCESS_INFORMATION pi; HANDLE hp = spawn_sleeper(&pi);
    CHECK(hp != NULL); if (!hp) return;
    DWORD tid = 0;
    HANDLE h = U(CreateRemoteThread)(hp, NULL, 0, (LPTHREAD_START_ROUTINE)U(ExitThread), (LPVOID)0x5A5A, 0, &tid);
    CHECK(h != NULL);
    if (h) {
        CHECK(tid != 0);
        EQ(WaitForSingleObject(h, 10000), WAIT_OBJECT_0);
        DWORD c = 0; GetExitCodeThread(h, &c);
        EQ(c, 0x5A5A);
        EQ(U(GetProcessIdOfThread)(h), pi.dwProcessId);
        CloseHandle(h);
    }
    EQ(WaitForSingleObject(hp, 0), WAIT_TIMEOUT);   /* child still alive */
    kill_child(hp, &pi);
}
TEST(rthread_exitprocess_code) {
    REQUIRE(U(CreateRemoteThread)); REQUIRE(U(ExitProcess));
    PROCESS_INFORMATION pi; HANDLE hp = spawn_sleeper(&pi);
    CHECK(hp != NULL); if (!hp) return;
    HANDLE h = U(CreateRemoteThread)(hp, NULL, 0, (LPTHREAD_START_ROUTINE)U(ExitProcess), (LPVOID)77, 0, NULL);
    CHECK(h != NULL);
    if (h) CloseHandle(h);
    EQ(WaitForSingleObject(hp, 10000), WAIT_OBJECT_0);
    DWORD c = 0; GetExitCodeProcess(hp, &c);
    EQ(c, 77);
    kill_child(hp, &pi);
}
TEST(rthread_create_suspended) {
    REQUIRE(U(CreateRemoteThread)); REQUIRE(U(ExitThread)); REQUIRE(U(ResumeThread));
    PROCESS_INFORMATION pi; HANDLE hp = spawn_sleeper(&pi);
    CHECK(hp != NULL); if (!hp) return;
    HANDLE h = U(CreateRemoteThread)(hp, NULL, 0, (LPTHREAD_START_ROUTINE)U(ExitThread), (LPVOID)0x33, CREATE_SUSPENDED, NULL);
    CHECK(h != NULL);
    if (h) {
        Sleep(100);
        EQ(WaitForSingleObject(h, 0), WAIT_TIMEOUT);
        EQ(U(ResumeThread)(h), 1);
        EQ(WaitForSingleObject(h, 10000), WAIT_OBJECT_0);
        DWORD c = 0; GetExitCodeThread(h, &c); EQ(c, 0x33);
        CloseHandle(h);
    }
    kill_child(hp, &pi);
}
TEST(rthread_terminate_remote_thread) {
    REQUIRE(U(CreateRemoteThread)); REQUIRE(U(TerminateThread));
    PROCESS_INFORMATION pi; HANDLE hp = spawn_sleeper(&pi);
    CHECK(hp != NULL); if (!hp) return;
    /* a suspended remote thread that we terminate before it ever runs */
    HANDLE h = U(CreateRemoteThread)(hp, NULL, 0, (LPTHREAD_START_ROUTINE)U(ExitThread), (LPVOID)1, CREATE_SUSPENDED, NULL);
    CHECK(h != NULL);
    if (h) {
        CHECK(U(TerminateThread)(h, 0x1234));
        EQ(WaitForSingleObject(h, 10000), WAIT_OBJECT_0);
        DWORD c = 0; GetExitCodeThread(h, &c); EQ(c, 0x1234);
        CloseHandle(h);
    }
    kill_child(hp, &pi);
}
TEST(rthread_access_denied_without_create_thread) {
    REQUIRE(U(CreateRemoteThread)); REQUIRE(U(ExitThread));
    PROCESS_INFORMATION pi; HANDLE hp = spawn_sleeper(&pi);
    CHECK(hp != NULL); if (!hp) return;
    HANDLE lim = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pi.dwProcessId);
    CHECK(lim != NULL);
    SET_ERR(0);
    HANDLE h = U(CreateRemoteThread)(lim, NULL, 0, (LPTHREAD_START_ROUTINE)U(ExitThread), NULL, 0, NULL);
    CHECK(h == NULL);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    if (h) CloseHandle(h);
    CloseHandle(lim);
    HANDLE ct = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pi.dwProcessId);
    CHECK(ct != NULL);
    if (ct) {
        h = U(CreateRemoteThread)(ct, NULL, 0, (LPTHREAD_START_ROUTINE)U(ExitThread), (LPVOID)6, 0, NULL);
        CHECK(h != NULL);
        if (h) { WaitForSingleObject(h, 10000); CloseHandle(h); }
        CloseHandle(ct);
    }
    kill_child(hp, &pi);
}
TEST(rthread_open_remote_thread_by_id) {
    REQUIRE(U(CreateRemoteThread)); REQUIRE(U(OpenThread)); REQUIRE(U(GetProcessIdOfThread)); REQUIRE(U(GetThreadId));
    PROCESS_INFORMATION pi; HANDLE hp = spawn_sleeper(&pi);
    CHECK(hp != NULL); if (!hp) return;
    DWORD tid = 0;
    HANDLE h = U(CreateRemoteThread)(hp, NULL, 0, (LPTHREAD_START_ROUTINE)U(ExitThread), (LPVOID)1, CREATE_SUSPENDED, &tid);
    CHECK(h != NULL);
    if (h) {
        HANDLE o = U(OpenThread)(THREAD_QUERY_LIMITED_INFORMATION, FALSE, tid);
        CHECK(o != NULL);
        if (o) {
            EQ(U(GetThreadId)(o), tid);
            EQ(U(GetProcessIdOfThread)(o), pi.dwProcessId);
            CloseHandle(o);
        }
        TerminateThread(h, 0);
        CloseHandle(h);
    }
    kill_child(hp, &pi);
}
TEST(rthread_ex_attribute_list_ideal_processor) {
    REQUIRE(U(CreateRemoteThreadEx)); REQUIRE(U(InitializeProcThreadAttributeList)); REQUIRE(U(UpdateProcThreadAttribute));
    REQUIRE(U(DeleteProcThreadAttributeList)); REQUIRE(U(GetThreadIdealProcessorEx));
    SIZE_T sz = 0;
    U(InitializeProcThreadAttributeList)(NULL, 1, 0, &sz);
    CHECK(sz > 0);
    LPPROC_THREAD_ATTRIBUTE_LIST al = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, sz);
    CHECK(U(InitializeProcThreadAttributeList)(al, 1, 0, &sz));
    PROCESSOR_NUMBER pn; memset(&pn, 0, sizeof pn);
    GetCurrentProcessorNumberEx(&pn);
    CHECK(U(UpdateProcThreadAttribute)(al, 0, PROC_THREAD_ATTRIBUTE_IDEAL_PROCESSOR, &pn, sizeof pn, NULL, NULL));
    DWORD tid = 0;
    HANDLE h = U(CreateRemoteThreadEx)(GetCurrentProcess(), NULL, 0, tf_th_ret, (LPVOID)12, CREATE_SUSPENDED, al, &tid);
    CHECK(h != NULL);
    if (h) {
        PROCESSOR_NUMBER got; memset(&got, 0xCC, sizeof got);
        if (U(GetThreadIdealProcessorEx)(h, &got)) {
            if (got.Group != pn.Group || got.Number != pn.Number)
                t_note("ideal processor attribute: got %u/%u want %u/%u", got.Group, got.Number, pn.Group, pn.Number);
        }
        ResumeThread(h);
        EQ(tf_join(h), 12);
    }
    U(DeleteProcThreadAttributeList)(al);
    HeapFree(GetProcessHeap(), 0, al);
}
TEST(rthread_ex_attribute_list_empty) {
    REQUIRE(U(CreateRemoteThreadEx)); REQUIRE(U(InitializeProcThreadAttributeList)); REQUIRE(U(DeleteProcThreadAttributeList));
    SIZE_T sz = 0;
    U(InitializeProcThreadAttributeList)(NULL, 0, 0, &sz);
    LPPROC_THREAD_ATTRIBUTE_LIST al = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, sz);
    CHECK(U(InitializeProcThreadAttributeList)(al, 0, 0, &sz));
    HANDLE h = U(CreateRemoteThreadEx)(GetCurrentProcess(), NULL, 0, tf_th_ret, (LPVOID)13, 0, al, NULL);
    CHECK(h != NULL);
    EQ(tf_join(h), 13);
    U(DeleteProcThreadAttributeList)(al);
    HeapFree(GetProcessHeap(), 0, al);
}
TEST(rthread_ex_attribute_list_group_affinity) {
    REQUIRE(U(CreateRemoteThreadEx)); REQUIRE(U(InitializeProcThreadAttributeList)); REQUIRE(U(UpdateProcThreadAttribute));
    REQUIRE(U(DeleteProcThreadAttributeList)); REQUIRE(U(GetThreadGroupAffinity));
    DWORD_PTR pm, sm;
    GetProcessAffinityMask(GetCurrentProcess(), &pm, &sm);
    SIZE_T sz = 0;
    U(InitializeProcThreadAttributeList)(NULL, 1, 0, &sz);
    LPPROC_THREAD_ATTRIBUTE_LIST al = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, sz);
    CHECK(U(InitializeProcThreadAttributeList)(al, 1, 0, &sz));
    GROUP_AFFINITY ga; memset(&ga, 0, sizeof ga);
    GROUP_AFFINITY cur; GetThreadGroupAffinity(GetCurrentThread(), &cur);
    ga.Group = cur.Group; ga.Mask = pm & (~pm + 1);
    CHECK(U(UpdateProcThreadAttribute)(al, 0, PROC_THREAD_ATTRIBUTE_GROUP_AFFINITY, &ga, sizeof ga, NULL, NULL));
    HANDLE h = U(CreateRemoteThreadEx)(GetCurrentProcess(), NULL, 0, tf_th_ret, (LPVOID)14, CREATE_SUSPENDED, al, NULL);
    CHECK(h != NULL);
    if (h) {
        GROUP_AFFINITY got; memset(&got, 0, sizeof got);
        if (U(GetThreadGroupAffinity)(h, &got)) EQ(got.Mask, ga.Mask);
        ResumeThread(h);
        EQ(tf_join(h), 14);
    }
    U(DeleteProcThreadAttributeList)(al);
    HeapFree(GetProcessHeap(), 0, al);
}
TEST(rthread_ex_attribute_list_garbage_count) {
    REQUIRE(U(CreateRemoteThreadEx)); REQUIRE(U(InitializeProcThreadAttributeList)); REQUIRE(U(DeleteProcThreadAttributeList));
    /* an attribute list with a never-set slot: attributes with Attribute==0 / unfilled */
    SIZE_T sz = 0;
    U(InitializeProcThreadAttributeList)(NULL, 2, 0, &sz);
    LPPROC_THREAD_ATTRIBUTE_LIST al = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, sz);
    CHECK(U(InitializeProcThreadAttributeList)(al, 2, 0, &sz));
    SET_ERR(0);
    HANDLE h = U(CreateRemoteThreadEx)(GetCurrentProcess(), NULL, 0, tf_th_ret, (LPVOID)15, 0, al, NULL);
    t_note("CreateRemoteThreadEx(attr list with no attributes set): %p err %lu", h, GetLastError());
    if (h) tf_join(h);
    U(DeleteProcThreadAttributeList)(al);
    HeapFree(GetProcessHeap(), 0, al);
}
TEST(rthread_ex_process_attribute_rejected) {
    REQUIRE(U(CreateRemoteThreadEx)); REQUIRE(U(InitializeProcThreadAttributeList)); REQUIRE(U(UpdateProcThreadAttribute));
    REQUIRE(U(DeleteProcThreadAttributeList));
    /* PROC_THREAD_ATTRIBUTE_PARENT_PROCESS is process-only, not valid for thread creation */
    SIZE_T sz = 0;
    U(InitializeProcThreadAttributeList)(NULL, 1, 0, &sz);
    LPPROC_THREAD_ATTRIBUTE_LIST al = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, sz);
    CHECK(U(InitializeProcThreadAttributeList)(al, 1, 0, &sz));
    HANDLE hp = GetCurrentProcess();
    CHECK(U(UpdateProcThreadAttribute)(al, 0, PROC_THREAD_ATTRIBUTE_PARENT_PROCESS, &hp, sizeof hp, NULL, NULL));
    SET_ERR(0);
    HANDLE h = U(CreateRemoteThreadEx)(GetCurrentProcess(), NULL, 0, tf_th_ret, (LPVOID)16, 0, al, NULL);
    t_note("CreateRemoteThreadEx(PARENT_PROCESS attr): %p err %lu", h, GetLastError());
    if (h) tf_join(h);
    U(DeleteProcThreadAttributeList)(al);
    HeapFree(GetProcessHeap(), 0, al);
}

/* ---------- process-level thread semantics, in child processes ---------- */
static DWORD WINAPI th_sleep_long(LPVOID p) { Sleep(20000); return 0; }
static DWORD WINAPI th_exitprocess(LPVOID p) {
    void (WINAPI *ep)(UINT) = (void (WINAPI *)(UINT))t_proc("ExitProcess");
    Sleep(50);
    ep((UINT)(ULONG_PTR)p);
    return 0;
}
CHILD(tf_main_returns_with_thread) {
    CreateThread(NULL, 0, th_sleep_long, NULL, 0, NULL);
    Sleep(50);
    return 11;
}
CHILD(tf_thread_exitprocess) {
    CreateThread(NULL, 0, th_exitprocess, (LPVOID)21, 0, NULL);
    Sleep(15000);
    return 22;
}
CHILD(tf_main_exitthread_last_thread_exits) {
    void (WINAPI *et)(DWORD) = (void (WINAPI *)(DWORD))t_proc("ExitThread");
    CreateThread(NULL, 0, th_exitprocess, (LPVOID)31, 0, NULL);
    et(5);
    return 99;
}
CHILD(tf_main_exitthread_last_thread_returns) {
    void (WINAPI *et)(DWORD) = (void (WINAPI *)(DWORD))t_proc("ExitThread");
    CreateThread(NULL, 0, tf_th_ret, (LPVOID)0x41, 0, NULL);
    et(5);
    return 99;
}
static DWORD WINAPI th_null_deref(LPVOID p) { volatile int *q = (volatile int *)p; *q = 1; return 0; }
CHILD(tf_thread_access_violation) {
    SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
    CreateThread(NULL, 0, th_null_deref, NULL, 0, NULL);
    Sleep(15000);
    return 0;
}
static DWORD WINAPI th_div_zero(LPVOID p) { volatile int z = (int)(ULONG_PTR)p; volatile int r = 100 / z; return r; }
CHILD(tf_thread_div_zero) {
    SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
    CreateThread(NULL, 0, th_div_zero, NULL, 0, NULL);
    Sleep(15000);
    return 0;
}
static LONG WINAPI uef_handle(PEXCEPTION_POINTERS ep) { ExitProcess(0x4000 | (ep->ExceptionRecord->ExceptionCode & 0xFF)); return EXCEPTION_EXECUTE_HANDLER; }
CHILD(tf_thread_uef) {
    SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
    SetUnhandledExceptionFilter(uef_handle);
    CreateThread(NULL, 0, th_null_deref, NULL, 0, NULL);
    Sleep(15000);
    return 0;
}
CHILD(tf_thread_raise_custom) {
    SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
    RaiseException(0xE0001234, EXCEPTION_NONCONTINUABLE, 0, NULL);
    return 0;
}
TEST(thrproc_main_returns_kills_threads) {
    EQ(t_run_child("tf_main_returns_with_thread", "", 25000), 11);
}
TEST(thrproc_exitprocess_from_worker) {
    EQ(t_run_child("tf_thread_exitprocess", "", 25000), 21);
}
TEST(thrproc_main_exitthread_worker_exitprocess) {
    EQ(t_run_child("tf_main_exitthread_last_thread_exits", "", 25000), 31);
}
TEST(thrproc_main_exitthread_last_thread_returns) {
    if (t_is_wine()) { t_skip("process lingers under wine"); return; }
    DWORD c = t_run_child("tf_main_exitthread_last_thread_returns", "", 25000);
    t_note("process exit code when last thread returns 0x41 after main ExitThread(5): %lx", c);
    CHECK(c != 0xDEAD0000 && c != 0xDEAD0001);
}
TEST(thrproc_worker_access_violation_kills_process) {
    EQ(t_run_child("tf_thread_access_violation", "", 25000), 0xC0000005);
}
TEST(thrproc_worker_div_zero_kills_process) {
    EQ(t_run_child("tf_thread_div_zero", "", 25000), 0xC0000094);
}
TEST(thrproc_worker_uef_runs) {
    EQ(t_run_child("tf_thread_uef", "", 25000), 0x4005);
}
TEST(thrproc_unhandled_custom_exception) {
    DWORD c = t_run_child("tf_thread_raise_custom", "", 25000);
    EQ(c, 0xE0001234);
}
