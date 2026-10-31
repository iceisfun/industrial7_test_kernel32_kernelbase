#include "jc.h"

HANDLE jc_job(void) { return U(CreateJobObjectW) ? U(CreateJobObjectW)(NULL, NULL) : NULL; }

HANDLE jc_spawn(const char *name, const char *args, DWORD extra, PROCESS_INFORMATION *pi) {
    return t_spawn_child(name, args, CREATE_SUSPENDED | extra, NULL, NULL, NULL, FALSE, NULL, pi);
}
BOOL jc_spawn_in_job(HANDLE job, const char *name, const char *args, PROCESS_INFORMATION *pi) {
    HANDLE h = jc_spawn(name, args, 0, pi);
    if (!h) return FALSE;
    if (!U(AssignProcessToJobObject)(job, h)) {
        DWORD e = GetLastError();
        TerminateProcess(h, 9); WaitForSingleObject(h, 3000);
        CloseHandle(pi->hThread); CloseHandle(h); memset(pi, 0, sizeof *pi);
        SetLastError(e);
        return FALSE;
    }
    ResumeThread(pi->hThread);
    return TRUE;
}
void jc_kill(PROCESS_INFORMATION *pi) {
    if (pi->hProcess) {
        if (WaitForSingleObject(pi->hProcess, 0) != WAIT_OBJECT_0) { TerminateProcess(pi->hProcess, 99); WaitForSingleObject(pi->hProcess, 5000); }
        CloseHandle(pi->hProcess);
    }
    if (pi->hThread) CloseHandle(pi->hThread);
    memset(pi, 0, sizeof *pi);
}
DWORD jc_wait_exit(HANDLE hp, DWORD ms) {
    DWORD code = 0;
    if (WaitForSingleObject(hp, ms) != WAIT_OBJECT_0) return JC_TIMEOUT;
    GetExitCodeProcess(hp, &code);
    return code;
}
BOOL jc_acct(HANDLE job, JOBOBJECT_BASIC_ACCOUNTING_INFORMATION *a) {
    return U(QueryInformationJobObject)(job, JobObjectBasicAccountingInformation, a, sizeof *a, NULL);
}
BOOL jc_wait_active(HANDLE job, DWORD n, DWORD ms) {
    DWORD t0 = GetTickCount();
    for (;;) {
        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION a;
        if (jc_acct(job, &a) && a.ActiveProcesses == n) return TRUE;
        if (GetTickCount() - t0 > ms) return FALSE;
        Sleep(20);
    }
}
BOOL jc_wait_total(HANDLE job, DWORD n, DWORD ms) {
    DWORD t0 = GetTickCount();
    for (;;) {
        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION a;
        if (jc_acct(job, &a) && a.TotalProcesses >= n) return TRUE;
        if (GetTickCount() - t0 > ms) return FALSE;
        Sleep(20);
    }
}
int jc_pids(HANDLE job, DWORD *out, int cap, DWORD *assigned) {
    size_t sz = sizeof(JOBOBJECT_BASIC_PROCESS_ID_LIST) + sizeof(ULONG_PTR) * (cap > 0 ? cap : 1);
    JOBOBJECT_BASIC_PROCESS_ID_LIST *l = calloc(1, sz);
    int r = -1;
    if (U(QueryInformationJobObject)(job, JobObjectBasicProcessIdList, l, (DWORD)sz, NULL)) {
        r = (int)l->NumberOfProcessIdsInList;
        if (assigned) *assigned = l->NumberOfAssignedProcesses;
        for (int i = 0; i < r && i < cap; i++) out[i] = (DWORD)l->ProcessIdList[i];
    }
    free(l);
    return r;
}
int jc_pid_in(HANDLE job, DWORD pid) {
    DWORD ids[64]; int n = jc_pids(job, ids, 64, NULL);
    for (int i = 0; i < n; i++) if (ids[i] == pid) return 1;
    return 0;
}
BOOL jc_set_ext(HANDLE job, JOBOBJECT_EXTENDED_LIMIT_INFORMATION *e) {
    return U(SetInformationJobObject)(job, JobObjectExtendedLimitInformation, e, sizeof *e);
}
BOOL jc_set_flags(HANDLE job, DWORD flags) {
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION e; memset(&e, 0, sizeof e);
    e.BasicLimitInformation.LimitFlags = flags;
    return jc_set_ext(job, &e);
}
HANDLE jc_port(void) { return CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0); }
BOOL jc_assoc(HANDLE job, HANDLE port, ULONG_PTR key) {
    JOBOBJECT_ASSOCIATE_COMPLETION_PORT a; a.CompletionKey = (PVOID)key; a.CompletionPort = port;
    return U(SetInformationJobObject)(job, JobObjectAssociateCompletionPortInformation, &a, sizeof a);
}
int jc_getmsg(HANDLE port, DWORD ms, DWORD *msg, ULONG_PTR *key, ULONG_PTR *pid) {
    DWORD b = 0; ULONG_PTR k = 0; OVERLAPPED *ov = NULL;
    if (!GetQueuedCompletionStatus(port, &b, &k, &ov, ms)) { if (!ov) return 0; }
    if (msg) *msg = b; if (key) *key = k; if (pid) *pid = (ULONG_PTR)ov;
    return 1;
}
int jc_wait_msg(HANDLE port, DWORD want, ULONG_PTR pid, DWORD ms, DWORD *seen) {
    DWORD t0 = GetTickCount();
    for (;;) {
        DWORD el = GetTickCount() - t0, m; ULONG_PTR k, p;
        if (el >= ms) return 0;
        if (!jc_getmsg(port, ms - el, &m, &k, &p)) return 0;
        if (m < 32 && seen) *seen |= 1u << m;
        if (m == want && (!pid || p == pid)) return 1;
    }
}
static LONG g_seq;
void jc_name(char *out, size_t cap, const char *tag) {
    snprintf(out, cap, "k32t_jo_%s_%lu_%ld", tag, GetCurrentProcessId(), InterlockedIncrement(&g_seq));
}
void jc_namew(WCHAR *out, size_t cap, const char *tag) {
    char a[128]; jc_name(a, sizeof a, tag);
    size_t i = 0; for (; a[i] && i + 1 < cap; i++) out[i] = a[i]; out[i] = 0;
}
HANDLE jc_dup(HANDLE h, DWORD access) {
    HANDLE d = NULL;
    if (!DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(), &d, access, FALSE, 0)) return NULL;
    return d;
}
