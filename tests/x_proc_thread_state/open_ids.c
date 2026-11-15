/* OpenThread / OpenProcess by id before exit, after exit while a handle is still held, and after
 * the last handle is closed (the id is released and the open fails with ERROR_INVALID_PARAMETER). */
#include "xpt.h"

TEST(xpt_open_thread_live_then_exited_while_held) {
    xpt_tw w; if (!xpt_tw_start(&w)) { CHECK(!"worker"); xpt_tw_stop(&w); return; }
    HANDLE a = U(OpenThread)(THREAD_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, w.tid);
    CHECK(a != NULL);
    SetEvent(w.go);
    EQ(WaitForSingleObject(w.h, 5000), WAIT_OBJECT_0);
    HANDLE b = U(OpenThread)(THREAD_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, w.tid);
    CHECKF(b != NULL, "err=%lu", GetLastError());
    if (b) {
        EQ(WaitForSingleObject(b, 0), WAIT_OBJECT_0);
        DWORD c = 0; GetExitCodeThread(b, &c); EQ(c, 0x51);
        CloseHandle(b);
    }
    if (a) { EQ(WaitForSingleObject(a, 0), WAIT_OBJECT_0); CloseHandle(a); }
    xpt_tw_stop(&w);
}
TEST(xpt_open_thread_after_last_close) {
    DWORD tid = 0;
    HANDLE h = xpt_dead_thread(1, &tid);
    CHECK(h != NULL); if (!h) return;
    CloseHandle(h);
    EQ(xpt_poll_open_gone(1, tid), ERROR_INVALID_PARAMETER);
}
TEST(xpt_open_thread_held_only_by_opened_handle) {
    /* The creator's handle is closed first; a handle from OpenThread alone keeps the id valid. */
    DWORD tid = 0;
    HANDLE h = xpt_dead_thread(0x77, &tid);
    CHECK(h != NULL); if (!h) return;
    HANDLE o = U(OpenThread)(THREAD_QUERY_LIMITED_INFORMATION, FALSE, tid);
    CHECK(o != NULL);
    CloseHandle(h);
    Sleep(20);
    HANDLE o2 = U(OpenThread)(THREAD_QUERY_LIMITED_INFORMATION, FALSE, tid);
    CHECKF(o2 != NULL, "err=%lu", GetLastError());
    if (o2) { DWORD c = 0; GetExitCodeThread(o2, &c); EQ(c, 0x77); CloseHandle(o2); }
    if (o) CloseHandle(o);
    EQ(xpt_poll_open_gone(1, tid), ERROR_INVALID_PARAMETER);
}
TEST(xpt_open_thread_full_access_on_exited) {
    DWORD tid = 0;
    HANDLE h = xpt_dead_thread(0x78, &tid);
    CHECK(h != NULL); if (!h) return;
    HANDLE o = U(OpenThread)(THREAD_ALL_ACCESS, FALSE, tid);
    CHECKF(o != NULL, "err=%lu", GetLastError());
    if (o) {
        EQ(xpt_access(o), THREAD_ALL_ACCESS);
        SetLastError(0); EQ(U(SuspendThread)(o), (DWORD)-1); EXPECT_ERR(ERROR_ACCESS_DENIED);
        CloseHandle(o);
    }
    CloseHandle(h);
}
TEST(xpt_open_process_live_then_exited_while_held) {
    PROCESS_INFORMATION pi; HANDLE hp = xpt_spawn_sleeper(&pi);
    CHECK(hp != NULL); if (!hp) return;
    HANDLE a = U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pi.dwProcessId);
    CHECK(a != NULL);
    TerminateProcess(hp, 0x61);
    EQ(WaitForSingleObject(hp, 5000), WAIT_OBJECT_0);
    HANDLE b = U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pi.dwProcessId);
    CHECKF(b != NULL, "err=%lu", GetLastError());
    if (b) { DWORD c = 0; GetExitCodeProcess(b, &c); EQ(c, 0x61); EQ(WaitForSingleObject(b, 0), WAIT_OBJECT_0); CloseHandle(b); }
    if (a) CloseHandle(a);
    xpt_kill(&pi);
}
TEST(xpt_open_process_after_last_close) {
    PROCESS_INFORMATION pi;
    if (!xpt_spawn_dead(&pi)) { CHECK(!"child"); return; }
    DWORD pid = pi.dwProcessId;
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    EQ(xpt_poll_open_gone(0, pid), ERROR_INVALID_PARAMETER);
}
TEST(xpt_open_process_thread_handle_keeps_process) {
    /* Only the initial thread handle is held: the thread references its process, so the pid stays open-able. */
    PROCESS_INFORMATION pi;
    if (!xpt_spawn_dead(&pi)) { CHECK(!"child"); return; }
    DWORD pid = pi.dwProcessId;
    CloseHandle(pi.hProcess);
    Sleep(20);
    HANDLE q = U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    CHECKF(q != NULL, "err=%lu", GetLastError());
    if (q) { DWORD c = 0; GetExitCodeProcess(q, &c); EQ(c, XPT_CHILD_EXIT); CloseHandle(q); }
    HANDLE t = U(OpenThread)(THREAD_QUERY_LIMITED_INFORMATION, FALSE, pi.dwThreadId);
    CHECKF(t != NULL, "err=%lu", GetLastError());
    if (t) { EQ(GetProcessIdOfThread(t), pid); CloseHandle(t); }
    CloseHandle(pi.hThread);
    EQ(xpt_poll_open_gone(0, pid), ERROR_INVALID_PARAMETER);
}
TEST(xpt_open_child_thread_after_last_close) {
    PROCESS_INFORMATION pi;
    if (!xpt_spawn_dead(&pi)) { CHECK(!"child"); return; }
    DWORD tid = pi.dwThreadId;
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    EQ(xpt_poll_open_gone(1, tid), ERROR_INVALID_PARAMETER);
}
TEST(xpt_open_processidtosessionid_after_last_close) {
    PROCESS_INFORMATION pi;
    if (!xpt_spawn_dead(&pi)) { CHECK(!"child"); return; }
    DWORD pid = pi.dwProcessId;
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    if (xpt_poll_open_gone(0, pid) != ERROR_INVALID_PARAMETER) { CHECK(!"pid still open-able"); return; }
    DWORD s = 0xCCCC;
    FAILS(U(ProcessIdToSessionId)(pid, &s), ERROR_INVALID_PARAMETER);
    EQ(s, 0xCCCC);
}
TEST(xpt_open_inheritable_handle_on_exited) {
    PROCESS_INFORMATION pi;
    if (!xpt_spawn_dead(&pi)) { CHECK(!"child"); return; }
    HANDLE q = U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION, TRUE, pi.dwProcessId);
    CHECK(q != NULL);
    if (q) { DWORD f = 0; GetHandleInformation(q, &f); EQ(f & HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT); CloseHandle(q); }
    xpt_kill(&pi);
}
TEST(xpt_open_zero_and_self_ids) {
    FAILS(U(OpenThread)(THREAD_QUERY_LIMITED_INFORMATION, FALSE, 0), ERROR_INVALID_PARAMETER);
    FAILS(U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, 0), ERROR_INVALID_PARAMETER);
    HANDLE t = U(OpenThread)(THREAD_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentThreadId());
    CHECK(t != NULL); if (t) { EQ(GetThreadId(t), GetCurrentThreadId()); CloseHandle(t); }
    /* a process id is not a thread id and vice versa */
    FAILS(U(OpenThread)(THREAD_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId()), ERROR_INVALID_PARAMETER);
    FAILS(U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentThreadId()), ERROR_INVALID_PARAMETER);
}
