/* AttachConsole: every call happens in a child process (CREATE_NEW_CONSOLE / DETACHED_PROCESS). */
#include "gap1.h"

static DWORD g1_run_flags(const char *name, const char *args, DWORD flags, DWORD timeout) {
    PROCESS_INFORMATION pi; DWORD code = 0xDEAD0000;
    HANDLE h = t_spawn_child(name, args, flags, NULL, NULL, NULL, FALSE, NULL, &pi);
    if (!h) return 0xDEAD0000;
    if (WaitForSingleObject(h, timeout) != WAIT_OBJECT_0) { TerminateProcess(h, 1); WaitForSingleObject(h, 5000); code = 0xDEAD0001; }
    else GetExitCodeProcess(h, &code);
    CloseHandle(pi.hThread); CloseHandle(h);
    return code;
}

CHILD(gap1_attach_when_attached) {
    BOOL (WINAPI *Att)(DWORD) = (BOOL (WINAPI *)(DWORD))t_proc("AttachConsole");
    if (!Att) return 100;
    DWORD res = 0;
    if (!GetConsoleWindow() && !GetConsoleCP()) return 101;      /* no console: environment problem */
    DWORD pids[] = { GetCurrentProcessId(), (DWORD)-1 /* ATTACH_PARENT_PROCESS */, 0, 4, 0x7FFFFFF0, 0xFFFFFFF0 };
    for (int i = 0; i < 6; i++) {
        SetLastError(0);
        if (Att(pids[i])) res |= 1u << (2 * i);
        else if (GetLastError() != ERROR_ACCESS_DENIED) res |= 2u << (2 * i);
    }
    return res;
}
TEST(gap1_attachconsole_when_already_attached) {
    REQUIRE(U(AttachConsole));
    DWORD r = g1_run_flags("gap1_attach_when_attached", "", CREATE_NEW_CONSOLE, 30000);
    if (r == 101) { t_skip("child could not get a console"); return; }
    CHECKF(r == 0, "child result 0x%lx", r);
}

CHILD(gap1_attach_bad_pids) {
    BOOL (WINAPI *Att)(DWORD) = (BOOL (WINAPI *)(DWORD))t_proc("AttachConsole");
    if (!Att) return 100;
    DWORD res = 0;
    if (GetConsoleWindow() || GetConsoleCP()) return 101;       /* should be detached */
    DWORD pids[] = { 0x7FFFFFF0, 0xFFFFFFF0, 0xFFFFFFFE, 0x0FFFFFF0, 0 };
    for (int i = 0; i < 5; i++) {
        SetLastError(0);
        BOOL ok = Att(pids[i]);
        DWORD e = GetLastError();
        if (ok) { res |= 1u << i; FreeConsole(); }
        else if (e == 0) res |= 1u << (8 + i);
        if (GetConsoleWindow() != NULL) res |= 1u << (16 + i);
    }
    return res;
}
TEST(gap1_attachconsole_nonexistent_pids) {
    REQUIRE(U(AttachConsole));
    DWORD r = g1_run_flags("gap1_attach_bad_pids", "", DETACHED_PROCESS, 30000);
    if (r == 101) { t_skip("child unexpectedly has a console"); return; }
    CHECKF(r == 0, "child result 0x%lx", r);
}

CHILD(gap1_attach_bad_pid_errors) {
    BOOL (WINAPI *Att)(DWORD) = (BOOL (WINAPI *)(DWORD))t_proc("AttachConsole");
    if (!Att) return 100;
    if (GetConsoleWindow() || GetConsoleCP()) return 101;
    DWORD pid = 0x7FFFFFF0;
    SetLastError(0);
    BOOL ok = Att(pid);
    printf("");   /* no console: output goes nowhere */
    return ok ? 1 : (int)(GetLastError() & 0xFFFF) + 1000;
}
TEST(gap1_attachconsole_nonexistent_pid_error_code) {
    REQUIRE(U(AttachConsole));
    DWORD r = g1_run_flags("gap1_attach_bad_pid_errors", "", DETACHED_PROCESS, 30000);
    t_note("AttachConsole(nonexistent pid) child result %lu (error + 1000)", r);
    CHECK(r >= 1000 && r != 0xDEAD0000 && r != 0xDEAD0001);
}

CHILD(gap1_attach_self_no_console) {
    BOOL (WINAPI *Att)(DWORD) = (BOOL (WINAPI *)(DWORD))t_proc("AttachConsole");
    if (!Att) return 100;
    if (GetConsoleWindow() || GetConsoleCP()) return 101;
    SetLastError(0);
    BOOL ok = Att(GetCurrentProcessId());
    DWORD e = GetLastError();
    if (ok) return 1;
    return (int)(e & 0xFFFF) + 1000;
}
TEST(gap1_attachconsole_own_pid_without_console) {
    REQUIRE(U(AttachConsole));
    DWORD r = g1_run_flags("gap1_attach_self_no_console", "", DETACHED_PROCESS, 30000);
    t_note("AttachConsole(self) with no console: result %lu", r);
    CHECK(r >= 1000 && r != 0xDEAD0000 && r != 0xDEAD0001);
}

/* holder: owns a console; signals ready and waits for release */
CHILD(gap1_attach_holder) {
    if (argc < 3) return 90;
    HANDLE ready = OpenEventA(EVENT_MODIFY_STATE, FALSE, argv[1]);
    HANDLE rel = OpenEventA(SYNCHRONIZE, FALSE, argv[2]);
    if (!ready || !rel) return 91;
    if (!GetConsoleWindow() && !GetConsoleCP()) return 92;
    SetEvent(ready);
    WaitForSingleObject(rel, 25000);
    return 0;
}
/* attacher: detached; attaches to the holder's console */
CHILD(gap1_attach_to_holder) {
    BOOL (WINAPI *Att)(DWORD) = (BOOL (WINAPI *)(DWORD))t_proc("AttachConsole");
    BOOL (WINAPI *GPL)(LPDWORD, DWORD) = (BOOL (WINAPI *)(LPDWORD, DWORD))t_proc("GetConsoleProcessList");
    if (!Att || argc < 2) return 100;
    DWORD hp = (DWORD)strtoul(argv[1], NULL, 10);
    DWORD res = 0;
    if (GetConsoleWindow() || GetConsoleCP()) return 101;
    SetLastError(0);
    if (!Att(hp)) return 200 + (GetLastError() & 0xFF);
    if (!GetConsoleCP()) res |= 1;
    if (GPL) {
        DWORD list[64]; DWORD n = GPL(list, 64);
        int me = 0, h = 0;
        for (DWORD i = 0; i < n && i < 64; i++) { if (list[i] == GetCurrentProcessId()) me = 1; if (list[i] == hp) h = 1; }
        if (!me) res |= 2;
        if (!h) res |= 4;
    }
    /* now attached: another attach must fail with ACCESS_DENIED */
    SetLastError(0);
    if (Att(hp)) res |= 8; else if (GetLastError() != ERROR_ACCESS_DENIED) res |= 16;
    if (!FreeConsole()) res |= 32;
    if (GetConsoleWindow() || GetConsoleCP()) res |= 64;
    /* detached again: attach works a second time */
    SetLastError(0);
    if (!Att(hp)) res |= 128; else { if (!GetConsoleCP()) res |= 256; FreeConsole(); }
    return res;
}
TEST(gap1_attachconsole_attach_to_other_console_and_reattach) {
    REQUIRE(U(AttachConsole));
    char en1[64], en2[64];
    snprintf(en1, sizeof en1, "Local\\k32t_gap1_rdy_%lu", GetCurrentProcessId());
    snprintf(en2, sizeof en2, "Local\\k32t_gap1_rel_%lu", GetCurrentProcessId());
    HANDLE ready = CreateEventA(NULL, TRUE, FALSE, en1);
    HANDLE rel = CreateEventA(NULL, TRUE, FALSE, en2);
    if (!ready || !rel) { t_skip("events"); return; }
    char args[256]; snprintf(args, sizeof args, "%s %s", en1, en2);
    PROCESS_INFORMATION hp;
    HANDLE holder = t_spawn_child("gap1_attach_holder", args, CREATE_NEW_CONSOLE, NULL, NULL, NULL, FALSE, NULL, &hp);
    if (!holder) { CloseHandle(ready); CloseHandle(rel); t_skip("spawn holder"); return; }
    if (WaitForSingleObject(ready, 15000) != WAIT_OBJECT_0) {
        TerminateProcess(holder, 1); CloseHandle(hp.hThread); CloseHandle(holder); CloseHandle(ready); CloseHandle(rel);
        t_skip("holder never became ready (no console host?)"); return;
    }
    char a2[32]; snprintf(a2, sizeof a2, "%lu", hp.dwProcessId);
    DWORD r = g1_run_flags("gap1_attach_to_holder", a2, DETACHED_PROCESS, 30000);
    SetEvent(rel);
    DWORD hc = 0;
    if (WaitForSingleObject(holder, 20000) != WAIT_OBJECT_0) TerminateProcess(holder, 1);
    GetExitCodeProcess(holder, &hc);
    CloseHandle(hp.hThread); CloseHandle(holder); CloseHandle(ready); CloseHandle(rel);
    if (r == 101) { t_skip("attacher unexpectedly had a console"); return; }
    if (r >= 200 && r < 456) t_note("AttachConsole(holder) failed with error %lu", r - 200);
    CHECKF(r == 0, "attacher result 0x%lx", r);
}
