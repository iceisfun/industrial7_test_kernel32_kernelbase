#include "so.h"
/* Kernel-side view of synchronization objects: type names, granted access, handle counts, object names,
   and the Nt*Query* information classes. Everything is resolved through GetProcAddress. */

typedef struct { USHORT Length, MaximumLength; PWSTR Buffer; } NQ_USTR;
typedef struct { ULONG Attributes; ULONG GrantedAccess; ULONG HandleCount; ULONG PointerCount; ULONG Rest[10]; } NQ_OBI;
typedef struct { LONG EventType; LONG EventState; } NQ_EVT;
typedef struct { LONG Current; LONG Maximum; } NQ_SEM;
typedef struct { LONG Current; BOOLEAN Owned; BOOLEAN Abandoned; } NQ_MUT;
typedef struct { LONGLONG Remaining; BOOLEAN State; } NQ_TMR;
UDECL(LONG, NtQueryObject, (HANDLE, ULONG, PVOID, ULONG, PULONG));
UDECL(LONG, NtQueryEvent, (HANDLE, ULONG, PVOID, ULONG, PULONG));
UDECL(LONG, NtQuerySemaphore, (HANDLE, ULONG, PVOID, ULONG, PULONG));
UDECL(LONG, NtQueryMutant, (HANDLE, ULONG, PVOID, ULONG, PULONG));
UDECL(LONG, NtQueryTimer, (HANDLE, ULONG, PVOID, ULONG, PULONG));

#define ST_TYPE_MISMATCH   ((LONG)0xC0000024)
#ifndef EVENT_QUERY_STATE
#define EVENT_QUERY_STATE 1
#endif
#define ST_INFO_LEN        ((LONG)0xC0000004)

#define NEED_NQ() do { REQUIRE(UN(NtQueryObject)); REQUIRE(U(CreateEventA)); REQUIRE(U(CreateMutexA)); REQUIRE(U(CreateSemaphoreA)); \
    REQUIRE(U(CreateWaitableTimerA)); } while (0)

static int nq_type(HANDLE h, WCHAR *out, int cap) {
    union { BYTE raw[512]; NQ_USTR u; } b; ULONG ret = 0;
    memset(&b, 0, sizeof b);
    LONG st = UN(NtQueryObject)(h, 2, &b, sizeof b, &ret);
    if (st < 0) return -1;
    int n = b.u.Length / 2; if (n >= cap) n = cap - 1;
    memcpy(out, b.u.Buffer, n * 2); out[n] = 0;
    return n;
}
static int nq_name(HANDLE h, WCHAR *out, int cap) {
    static BYTE raw[2048]; ULONG ret = 0;
    memset(raw, 0, sizeof raw);
    LONG st = UN(NtQueryObject)(h, 1, raw, sizeof raw, &ret);
    if (st < 0) return -1;
    NQ_USTR *u = (NQ_USTR *)raw;
    int n = u->Length / 2; if (n >= cap) n = cap - 1;
    if (n) memcpy(out, u->Buffer, n * 2);
    out[n] = 0; return n;
}
static int nq_basic(HANDLE h, NQ_OBI *o) {
    ULONG ret = 0; memset(o, 0, sizeof *o);
    return UN(NtQueryObject)(h, 0, o, sizeof *o, &ret) >= 0;
}
static int wends(const WCHAR *s, const char *suffix) {
    WCHAR w[300]; so_wide(suffix, w, 300);
    size_t a = wcslen(s), b = wcslen(w);
    return a >= b && wcscmp(s + a - b, w) == 0;
}

TEST(nq_type_names) {
    NEED_NQ();
    HANDLE e = U(CreateEventA)(NULL, TRUE, FALSE, NULL), m = U(CreateMutexA)(NULL, FALSE, NULL);
    HANDLE s = U(CreateSemaphoreA)(NULL, 0, 1, NULL), t = U(CreateWaitableTimerA)(NULL, TRUE, NULL);
    WCHAR n[64];
    struct { HANDLE h; const wchar_t *want; } tab[] = { { e, L"Event" }, { m, L"Mutant" }, { s, L"Semaphore" }, { t, L"Timer" } };
    for (int i = 0; i < 4; i++) {
        CHECK(tab[i].h != NULL); if (!tab[i].h) continue;
        int r = nq_type(tab[i].h, n, 64);
        CHECKF(r > 0 && wcscmp(n, tab[i].want) == 0, "type of object %d is '%ls'", i, n);
    }
    HANDLE hr = U(CreateWaitableTimerExW)(NULL, NULL, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (hr) { int r = nq_type(hr, n, 64); CHECKF(r > 0 && wcscmp(n, L"Timer") == 0, "'%ls'", n); CloseHandle(hr); }
    CloseHandle(e); CloseHandle(m); CloseHandle(s); CloseHandle(t);
}
TEST(nq_granted_access_default_all_access) {
    NEED_NQ();
    HANDLE e = U(CreateEventA)(NULL, TRUE, FALSE, NULL), m = U(CreateMutexA)(NULL, FALSE, NULL);
    HANDLE s = U(CreateSemaphoreA)(NULL, 0, 1, NULL), t = U(CreateWaitableTimerA)(NULL, TRUE, NULL);
    NQ_OBI o;
    CHECK(nq_basic(e, &o)); EXPECT_EQ(o.GrantedAccess & EVENT_ALL_ACCESS, EVENT_ALL_ACCESS);
    CHECK(nq_basic(m, &o)); EXPECT_EQ(o.GrantedAccess & MUTEX_ALL_ACCESS, MUTEX_ALL_ACCESS);
    CHECK(nq_basic(s, &o)); EXPECT_EQ(o.GrantedAccess & SEMAPHORE_ALL_ACCESS, SEMAPHORE_ALL_ACCESS);
    CHECK(nq_basic(t, &o)); EXPECT_EQ(o.GrantedAccess & TIMER_ALL_ACCESS, TIMER_ALL_ACCESS);
    CloseHandle(e); CloseHandle(m); CloseHandle(s); CloseHandle(t);
}
TEST(nq_granted_access_ex_exact_mask) {
    NEED_NQ(); REQUIRE(U(CreateEventExA)); REQUIRE(U(CreateSemaphoreExA)); REQUIRE(U(CreateMutexExA)); REQUIRE(U(CreateWaitableTimerExA));
    NQ_OBI o;
    HANDLE e = U(CreateEventExA)(NULL, NULL, 0, SYNCHRONIZE | EVENT_MODIFY_STATE);
    CHECK(e != NULL); if (e) { CHECK(nq_basic(e, &o)); EXPECT_EQ(o.GrantedAccess, SYNCHRONIZE | EVENT_MODIFY_STATE); CloseHandle(e); }
    HANDLE s = U(CreateSemaphoreExA)(NULL, 0, 1, NULL, 0, SYNCHRONIZE);
    CHECK(s != NULL); if (s) { CHECK(nq_basic(s, &o)); EXPECT_EQ(o.GrantedAccess, SYNCHRONIZE); CloseHandle(s); }
    HANDLE m = U(CreateMutexExA)(NULL, NULL, 0, SYNCHRONIZE);
    CHECK(m != NULL); if (m) { CHECK(nq_basic(m, &o)); EXPECT_EQ(o.GrantedAccess, SYNCHRONIZE); CloseHandle(m); }
    HANDLE t = U(CreateWaitableTimerExA)(NULL, NULL, 0, TIMER_MODIFY_STATE);
    CHECK(t != NULL); if (t) { CHECK(nq_basic(t, &o)); EXPECT_EQ(o.GrantedAccess, TIMER_MODIFY_STATE); CloseHandle(t); }
}
TEST(nq_handle_count_tracks_handles) {
    NEED_NQ();
    HANDLE e = U(CreateEventA)(NULL, TRUE, FALSE, NULL), d = NULL;
    NQ_OBI o;
    CHECK(nq_basic(e, &o)); EXPECT_EQ(o.HandleCount, 1);
    CHECK(DuplicateHandle(GetCurrentProcess(), e, GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS));
    CHECK(nq_basic(e, &o)); EXPECT_EQ(o.HandleCount, 2);
    CloseHandle(d);
    CHECK(nq_basic(e, &o)); EXPECT_EQ(o.HandleCount, 1);
    CloseHandle(e);
}
TEST(nq_handle_count_named_create_twice) {
    NEED_NQ();
    char n[80]; so_name(n, sizeof n, "nqhc");
    HANDLE a = U(CreateSemaphoreA)(NULL, 0, 1, n), b = U(CreateSemaphoreA)(NULL, 0, 1, n);
    NQ_OBI o;
    CHECK(a && b); if (!a || !b) return;
    CHECK(nq_basic(a, &o)); EXPECT_EQ(o.HandleCount, 2);
    CloseHandle(b);
    CHECK(nq_basic(a, &o)); EXPECT_EQ(o.HandleCount, 1);
    CloseHandle(a);
}
TEST(nq_object_name_plain_global_local) {
    NEED_NQ();
    char n[80], full[200]; WCHAR w[400];
    so_name(n, sizeof n, "nqn1");
    HANDLE e = U(CreateEventA)(NULL, TRUE, FALSE, n);
    CHECK(e != NULL);
    if (e) {
        CHECK(nq_name(e, w, 400) > 0);
        snprintf(full, sizeof full, "\\BaseNamedObjects\\%s", n);
        CHECKF(wends(w, full), "name '%ls'", w);
        CloseHandle(e);
    }
    so_name(n, sizeof n, "nqn2"); snprintf(full, sizeof full, "Global\\%s", n);
    e = U(CreateEventA)(NULL, TRUE, FALSE, full);
    if (e) {
        CHECK(nq_name(e, w, 400) > 0);
        char want[200]; snprintf(want, sizeof want, "\\BaseNamedObjects\\%s", n);
        CHECKF(wends(w, want), "name '%ls'", w);
        CloseHandle(e);
    } else t_note("Global\\ create failed err %lu", GetLastError());
    so_name(n, sizeof n, "nqn3"); snprintf(full, sizeof full, "Local\\%s", n);
    e = U(CreateEventA)(NULL, TRUE, FALSE, full);
    CHECK(e != NULL);
    if (e) {
        CHECK(nq_name(e, w, 400) > 0);
        char want[200]; snprintf(want, sizeof want, "\\BaseNamedObjects\\%s", n);
        CHECKF(wends(w, want), "name '%ls'", w);
        CloseHandle(e);
    }
}
TEST(nq_object_name_unnamed_is_empty) {
    NEED_NQ();
    HANDLE h[4] = { U(CreateEventA)(NULL, TRUE, FALSE, NULL), U(CreateMutexA)(NULL, FALSE, NULL),
                    U(CreateSemaphoreA)(NULL, 0, 1, NULL), U(CreateWaitableTimerA)(NULL, TRUE, NULL) };
    WCHAR w[100];
    for (int i = 0; i < 4; i++) { CHECK(h[i] != NULL); if (!h[i]) continue; EXPECT_EQ(nq_name(h[i], w, 100), 0); CloseHandle(h[i]); }
}
TEST(nq_name_wide_matches_ansi_name_in_namespace) {
    NEED_NQ(); REQUIRE(U(CreateMutexW));
    char n[80]; WCHAR wn[80], w[400]; so_name(n, sizeof n, "nqw"); so_wide(n, wn, 80);
    HANDLE m = U(CreateMutexW)(NULL, FALSE, wn);
    CHECK(m != NULL); if (!m) return;
    CHECK(nq_name(m, w, 400) > 0);
    CHECKF(wends(w, n), "'%ls'", w);
    CloseHandle(m);
}
/* ---------- event ---------- */
TEST(nq_event_basic_information) {
    NEED_NQ(); REQUIRE(UN(NtQueryEvent));
    NQ_EVT i; ULONG ret = 0;
    HANDLE man = U(CreateEventA)(NULL, TRUE, TRUE, NULL), au = U(CreateEventA)(NULL, FALSE, FALSE, NULL);
    memset(&i, 0xAA, sizeof i);
    EXPECT_EQ(UN(NtQueryEvent)(man, 0, &i, sizeof i, &ret), 0);
    EXPECT_EQ(i.EventType, 0); EXPECT_EQ(i.EventState, 1); EXPECT_EQ(ret, sizeof i);
    EXPECT_EQ(UN(NtQueryEvent)(au, 0, &i, sizeof i, &ret), 0);
    EXPECT_EQ(i.EventType, 1); EXPECT_EQ(i.EventState, 0);
    SetEvent(au);
    UN(NtQueryEvent)(au, 0, &i, sizeof i, &ret); EXPECT_EQ(i.EventState, 1);
    WaitForSingleObject(au, 0);
    UN(NtQueryEvent)(au, 0, &i, sizeof i, &ret); EXPECT_EQ(i.EventState, 0);
    ResetEvent(man);
    UN(NtQueryEvent)(man, 0, &i, sizeof i, &ret); EXPECT_EQ(i.EventState, 0);
    CloseHandle(man); CloseHandle(au);
}
TEST(nq_event_query_errors) {
    NEED_NQ(); REQUIRE(UN(NtQueryEvent));
    NQ_EVT i; ULONG ret = 0;
    HANDLE e = U(CreateEventA)(NULL, TRUE, FALSE, NULL), s = U(CreateSemaphoreA)(NULL, 0, 1, NULL);
    EXPECT_EQ(UN(NtQueryEvent)(e, 0, &i, 0, &ret), ST_INFO_LEN);
    EXPECT_EQ(UN(NtQueryEvent)(e, 0, &i, sizeof i - 1, &ret), ST_INFO_LEN);
    EXPECT_EQ(UN(NtQueryEvent)(s, 0, &i, sizeof i, &ret), ST_TYPE_MISMATCH);
    LONG st = UN(NtQueryEvent)(e, 7, &i, sizeof i, &ret);
    CHECKF(st < 0, "bad info class status 0x%lx", st);
    EXPECT_EQ(UN(NtQueryEvent)(e, 0, &i, sizeof i, NULL) >= 0, 1);   /* result length is optional */
    CloseHandle(e); CloseHandle(s);
}
TEST(nq_event_query_requires_query_state_access) {
    NEED_NQ(); REQUIRE(UN(NtQueryEvent)); REQUIRE(U(CreateEventExA));
    NQ_EVT i; ULONG ret = 0;
    HANDLE e = U(CreateEventExA)(NULL, NULL, 0, SYNCHRONIZE);
    CHECK(e != NULL); if (!e) return;
    LONG st = UN(NtQueryEvent)(e, 0, &i, sizeof i, &ret);
    EXPECT_EQ(st, (LONG)0xC0000022);          /* STATUS_ACCESS_DENIED */
    CloseHandle(e);
    e = U(CreateEventExA)(NULL, NULL, 0, EVENT_QUERY_STATE);
    CHECK(e != NULL); if (!e) return;
    EXPECT_EQ(UN(NtQueryEvent)(e, 0, &i, sizeof i, &ret), 0);
    CloseHandle(e);
}
/* ---------- semaphore ---------- */
TEST(nq_semaphore_basic_information) {
    NEED_NQ(); REQUIRE(UN(NtQuerySemaphore)); REQUIRE(U(ReleaseSemaphore));
    NQ_SEM i; ULONG ret = 0; LONG prev = -1;
    HANDLE s = U(CreateSemaphoreA)(NULL, 2, 5, NULL);
    CHECK(s != NULL); if (!s) return;
    EXPECT_EQ(UN(NtQuerySemaphore)(s, 0, &i, sizeof i, &ret), 0);
    EXPECT_EQ(i.Current, 2); EXPECT_EQ(i.Maximum, 5); EXPECT_EQ(ret, sizeof i);
    WaitForSingleObject(s, 0);
    UN(NtQuerySemaphore)(s, 0, &i, sizeof i, &ret); EXPECT_EQ(i.Current, 1);
    EXPECT_EQ(U(ReleaseSemaphore)(s, 3, &prev), TRUE); EXPECT_EQ(prev, 1);
    UN(NtQuerySemaphore)(s, 0, &i, sizeof i, &ret); EXPECT_EQ(i.Current, 4); EXPECT_EQ(i.Maximum, 5);
    CloseHandle(s);
}
TEST(nq_semaphore_query_errors) {
    NEED_NQ(); REQUIRE(UN(NtQuerySemaphore));
    NQ_SEM i; ULONG ret = 0;
    HANDLE s = U(CreateSemaphoreA)(NULL, 0, 1, NULL), e = U(CreateEventA)(NULL, TRUE, FALSE, NULL);
    EXPECT_EQ(UN(NtQuerySemaphore)(s, 0, &i, 0, &ret), ST_INFO_LEN);
    EXPECT_EQ(UN(NtQuerySemaphore)(e, 0, &i, sizeof i, &ret), ST_TYPE_MISMATCH);
    CloseHandle(s); CloseHandle(e);
}
TEST(nq_semaphore_max_and_initial_extremes) {
    NEED_NQ(); REQUIRE(UN(NtQuerySemaphore));
    NQ_SEM i; ULONG ret = 0;
    HANDLE s = U(CreateSemaphoreA)(NULL, 0x7FFFFFFF, 0x7FFFFFFF, NULL);
    CHECK(s != NULL); if (!s) return;
    UN(NtQuerySemaphore)(s, 0, &i, sizeof i, &ret);
    EXPECT_EQ(i.Current, 0x7FFFFFFF); EXPECT_EQ(i.Maximum, 0x7FFFFFFF);
    SET_ERR(0); EXPECT_EQ(U(ReleaseSemaphore)(s, 1, NULL), FALSE); EXPECT_ERR(ERROR_TOO_MANY_POSTS);
    CloseHandle(s);
}
/* ---------- mutant ---------- */
TEST(nq_mutant_basic_information) {
    NEED_NQ(); REQUIRE(UN(NtQueryMutant)); REQUIRE(U(ReleaseMutex));
    NQ_MUT i; ULONG ret = 0;
    HANDLE m = U(CreateMutexA)(NULL, FALSE, NULL);
    CHECK(m != NULL); if (!m) return;
    memset(&i, 0xAA, sizeof i);
    EXPECT_EQ(UN(NtQueryMutant)(m, 0, &i, sizeof i, &ret), 0);
    EXPECT_EQ(i.Current, 1); EXPECT_EQ(i.Owned, 0); EXPECT_EQ(i.Abandoned, 0);
    WaitForSingleObject(m, 0);
    UN(NtQueryMutant)(m, 0, &i, sizeof i, &ret);
    EXPECT_EQ(i.Current, 0); EXPECT_EQ(i.Owned, 1); EXPECT_EQ(i.Abandoned, 0);
    WaitForSingleObject(m, 0);
    UN(NtQueryMutant)(m, 0, &i, sizeof i, &ret);
    t_note("recursion 2: CurrentCount %ld", i.Current);
    CHECK(i.Current < 0 && i.Owned);
    U(ReleaseMutex)(m); U(ReleaseMutex)(m);
    UN(NtQueryMutant)(m, 0, &i, sizeof i, &ret);
    EXPECT_EQ(i.Current, 1); EXPECT_EQ(i.Owned, 0);
    CloseHandle(m);
}
TEST(nq_mutant_initial_owner) {
    NEED_NQ(); REQUIRE(UN(NtQueryMutant)); REQUIRE(U(ReleaseMutex));
    NQ_MUT i; ULONG ret = 0;
    HANDLE m = U(CreateMutexA)(NULL, TRUE, NULL);
    CHECK(m != NULL); if (!m) return;
    UN(NtQueryMutant)(m, 0, &i, sizeof i, &ret);
    EXPECT_EQ(i.Current, 0); EXPECT_EQ(i.Owned, 1);
    U(ReleaseMutex)(m);
    CloseHandle(m);
}
static HANDLE g_nq_mutex;
static DWORD WINAPI nq_take_and_die(LPVOID p) { WaitForSingleObject(g_nq_mutex, 5000); return 0; }
TEST(nq_mutant_owned_by_caller_is_per_thread_and_abandoned_state) {
    NEED_NQ(); REQUIRE(UN(NtQueryMutant)); REQUIRE(U(ReleaseMutex));
    NQ_MUT i; ULONG ret = 0;
    g_nq_mutex = U(CreateMutexA)(NULL, FALSE, NULL);
    CHECK(g_nq_mutex != NULL); if (!g_nq_mutex) return;
    so_thr t = so_start(nq_take_and_die, NULL);
    EXPECT_EQ(so_join(t, 5000), 0);
    UN(NtQueryMutant)(g_nq_mutex, 0, &i, sizeof i, &ret);
    EXPECT_EQ(i.Owned, 0); EXPECT_EQ(i.Abandoned, 1); EXPECT_EQ(i.Current, 1);
    EXPECT_EQ(WaitForSingleObject(g_nq_mutex, 0), WAIT_ABANDONED_0);
    UN(NtQueryMutant)(g_nq_mutex, 0, &i, sizeof i, &ret);
    EXPECT_EQ(i.Owned, 1); EXPECT_EQ(i.Abandoned, 0);
    U(ReleaseMutex)(g_nq_mutex);
    CloseHandle(g_nq_mutex);
}
TEST(nq_mutant_query_errors) {
    NEED_NQ(); REQUIRE(UN(NtQueryMutant));
    NQ_MUT i; ULONG ret = 0;
    HANDLE m = U(CreateMutexA)(NULL, FALSE, NULL), s = U(CreateSemaphoreA)(NULL, 0, 1, NULL);
    EXPECT_EQ(UN(NtQueryMutant)(m, 0, &i, 0, &ret), ST_INFO_LEN);
    EXPECT_EQ(UN(NtQueryMutant)(s, 0, &i, sizeof i, &ret), ST_TYPE_MISMATCH);
    CloseHandle(m); CloseHandle(s);
}
/* ---------- timer ---------- */
TEST(nq_timer_basic_information) {
    NEED_NQ(); REQUIRE(UN(NtQueryTimer)); REQUIRE(U(SetWaitableTimer)); REQUIRE(U(CancelWaitableTimer));
    NQ_TMR i; ULONG ret = 0;
    HANDLE t = U(CreateWaitableTimerA)(NULL, TRUE, NULL);
    CHECK(t != NULL); if (!t) return;
    memset(&i, 0, sizeof i);
    EXPECT_EQ(UN(NtQueryTimer)(t, 0, &i, sizeof i, &ret), 0);
    EXPECT_EQ(i.State, 0);
    LARGE_INTEGER d; d.QuadPart = -600000000LL;       /* 60 s relative */
    EXPECT_EQ(U(SetWaitableTimer)(t, &d, 0, NULL, NULL, FALSE), TRUE);
    UN(NtQueryTimer)(t, 0, &i, sizeof i, &ret);
    EXPECT_EQ(i.State, 0);
    CHECKF(i.Remaining > 0 && i.Remaining <= 600000000LL + 20000000LL, "remaining %lld", i.Remaining);
    EXPECT_EQ(U(CancelWaitableTimer)(t), TRUE);
    UN(NtQueryTimer)(t, 0, &i, sizeof i, &ret);
    t_note("remaining after cancel: %lld state %d", i.Remaining, i.State);
    CloseHandle(t);
}
TEST(nq_timer_signaled_state_after_fire) {
    NEED_NQ(); REQUIRE(UN(NtQueryTimer)); REQUIRE(U(SetWaitableTimer));
    NQ_TMR i; ULONG ret = 0;
    HANDLE t = U(CreateWaitableTimerA)(NULL, TRUE, NULL);
    CHECK(t != NULL); if (!t) return;
    LARGE_INTEGER d; d.QuadPart = -100000LL;          /* 10 ms */
    U(SetWaitableTimer)(t, &d, 0, NULL, NULL, FALSE);
    EXPECT_EQ(WaitForSingleObject(t, 5000), WAIT_OBJECT_0);
    UN(NtQueryTimer)(t, 0, &i, sizeof i, &ret);
    EXPECT_EQ(i.State, 1); EXPECT_EQ(i.Remaining, 0);
    CloseHandle(t);
}
TEST(nq_timer_query_errors) {
    NEED_NQ(); REQUIRE(UN(NtQueryTimer));
    NQ_TMR i; ULONG ret = 0;
    HANDLE t = U(CreateWaitableTimerA)(NULL, TRUE, NULL), e = U(CreateEventA)(NULL, TRUE, FALSE, NULL);
    EXPECT_EQ(UN(NtQueryTimer)(t, 0, &i, 0, &ret), ST_INFO_LEN);
    EXPECT_EQ(UN(NtQueryTimer)(e, 0, &i, sizeof i, &ret), ST_TYPE_MISMATCH);
    CloseHandle(t); CloseHandle(e);
}
/* ---------- object query errors ---------- */
TEST(nq_object_query_invalid_handles_and_lengths) {
    NEED_NQ();
    NQ_OBI o; ULONG ret = 0;
    EXPECT_EQ(UN(NtQueryObject)(NULL, 0, &o, sizeof o, &ret), (LONG)0xC0000008);                  /* STATUS_INVALID_HANDLE */
    EXPECT_EQ(UN(NtQueryObject)((HANDLE)(ULONG_PTR)0x7777ABCC, 0, &o, sizeof o, &ret), (LONG)0xC0000008);
    HANDLE e = U(CreateEventA)(NULL, TRUE, FALSE, NULL);
    EXPECT_EQ(UN(NtQueryObject)(e, 0, &o, 4, &ret), ST_INFO_LEN);
    CloseHandle(e);
    EXPECT_EQ(UN(NtQueryObject)(e, 0, &o, sizeof o, &ret), (LONG)0xC0000008);                      /* closed */
}
TEST(nq_sync_type_confusion_status_matrix) {
    NEED_NQ(); REQUIRE(UN(NtQueryEvent)); REQUIRE(UN(NtQuerySemaphore)); REQUIRE(UN(NtQueryMutant)); REQUIRE(UN(NtQueryTimer));
    HANDLE h[4] = { U(CreateEventA)(NULL, TRUE, FALSE, NULL), U(CreateSemaphoreA)(NULL, 0, 1, NULL),
                    U(CreateMutexA)(NULL, FALSE, NULL), U(CreateWaitableTimerA)(NULL, TRUE, NULL) };
    typedef LONG (WINAPI *qf)(HANDLE, ULONG, PVOID, ULONG, PULONG);
    qf f[4] = { (qf)UN(NtQueryEvent), (qf)UN(NtQuerySemaphore), (qf)UN(NtQueryMutant), (qf)UN(NtQueryTimer) };
    BYTE buf[64]; ULONG ret;
    for (int q = 0; q < 4; q++) for (int o = 0; o < 4; o++) {
        LONG st = f[q](h[o], 0, buf, q == 3 ? 16 : 8, &ret);
        if (q == o) { CHECKF(st >= 0, "q%d o%d st 0x%lx", q, o, st); }
        else { CHECKF(st == ST_TYPE_MISMATCH, "q%d o%d st 0x%lx", q, o, st); }
    }
    for (int i = 0; i < 4; i++) CloseHandle(h[i]);
}
/* ---------- exports ---------- */
TEST(exports_kernel32_sync_surface_present) {
    static const char *names[] = {
        "AcquireSRWLockExclusive", "AcquireSRWLockShared", "AddIntegrityLabelToBoundaryDescriptor", "AddSIDToBoundaryDescriptor",
        "CancelWaitableTimer", "ClosePrivateNamespace", "CreateBoundaryDescriptorA", "CreateBoundaryDescriptorW", "CreateEventA", "CreateEventExA",
        "CreateEventExW", "CreateEventW", "CreateMutexA", "CreateMutexExA", "CreateMutexExW", "CreateMutexW", "CreatePrivateNamespaceA",
        "CreatePrivateNamespaceW", "CreateSemaphoreA", "CreateSemaphoreExA", "CreateSemaphoreExW", "CreateSemaphoreW", "CreateWaitableTimerA",
        "CreateWaitableTimerExA", "CreateWaitableTimerExW", "CreateWaitableTimerW", "DeleteBoundaryDescriptor", "DeleteCriticalSection",
        "DeleteSynchronizationBarrier", "EnterCriticalSection", "EnterSynchronizationBarrier", "InitOnceBeginInitialize", "InitOnceComplete",
        "InitOnceExecuteOnce", "InitOnceInitialize", "InitializeConditionVariable", "InitializeCriticalSection", "InitializeCriticalSectionAndSpinCount",
        "InitializeCriticalSectionEx", "InitializeSListHead", "InitializeSRWLock", "InitializeSynchronizationBarrier", "InterlockedFlushSList",
        "InterlockedPopEntrySList", "InterlockedPushEntrySList", "InterlockedPushListSList", "InterlockedPushListSListEx", "LeaveCriticalSection",
        "OpenEventA", "OpenEventW", "OpenMutexA", "OpenMutexW", "OpenPrivateNamespaceA", "OpenPrivateNamespaceW", "OpenSemaphoreA", "OpenSemaphoreW",
        "OpenWaitableTimerA", "OpenWaitableTimerW", "PulseEvent", "QueryDepthSList", "ReleaseMutex", "ReleaseSRWLockExclusive", "ReleaseSRWLockShared",
        "ReleaseSemaphore", "ResetEvent", "SetCriticalSectionSpinCount", "SetEvent", "SetWaitableTimer", "SetWaitableTimerEx", "SignalObjectAndWait",
        "Sleep", "SleepEx", "SleepConditionVariableCS", "SleepConditionVariableSRW", "TryAcquireSRWLockExclusive", "TryAcquireSRWLockShared",
        "TryEnterCriticalSection", "WaitForMultipleObjects", "WaitForMultipleObjectsEx", "WaitForSingleObject", "WaitForSingleObjectEx",
        "WakeAllConditionVariable", "WakeConditionVariable", "SwitchToThread",
    };
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) CHECKF(t_proc_k32(names[i]) != NULL, "kernel32!%s", names[i]);
}
TEST(exports_kernelbase_sync_surface_present) {
    static const char *names[] = {
        "CreateEventA", "CreateEventW", "CreateEventExA", "CreateEventExW", "CreateMutexExA", "CreateMutexExW", "CreateSemaphoreExW",
        "CreateWaitableTimerExW", "OpenEventW", "OpenMutexW", "OpenSemaphoreW", "OpenWaitableTimerW", "SetWaitableTimerEx", "SetWaitableTimer",
        "WaitOnAddress", "WakeByAddressAll", "WakeByAddressSingle", "Sleep", "SleepEx", "SleepConditionVariableCS", "SleepConditionVariableSRW",
        "InitOnceBeginInitialize", "InitOnceComplete", "InitOnceExecuteOnce", "InitializeCriticalSectionAndSpinCount", "InitializeCriticalSectionEx",
        "SetCriticalSectionSpinCount", "SignalObjectAndWait", "WaitForSingleObjectEx", "WaitForMultipleObjectsEx", "SwitchToThread",
        "ReleaseMutex", "ReleaseSemaphore", "SetEvent", "ResetEvent", "CancelWaitableTimer",
    };
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) CHECKF(t_proc_kb(names[i]) != NULL, "kernelbase!%s", names[i]);
}
TEST(exports_ntdll_sync_primitives_present) {
    static const char *names[] = {
        "NtCreateEvent", "NtOpenEvent", "NtSetEvent", "NtResetEvent", "NtPulseEvent", "NtQueryEvent", "NtCreateMutant", "NtOpenMutant", "NtReleaseMutant",
        "NtQueryMutant", "NtCreateSemaphore", "NtOpenSemaphore", "NtReleaseSemaphore", "NtQuerySemaphore", "NtCreateTimer", "NtOpenTimer", "NtSetTimer",
        "NtCancelTimer", "NtQueryTimer", "NtWaitForSingleObject", "NtWaitForMultipleObjects", "NtSignalAndWaitForSingleObject",
        "RtlInitializeCriticalSection", "RtlEnterCriticalSection", "RtlLeaveCriticalSection", "RtlTryEnterCriticalSection", "RtlDeleteCriticalSection",
        "RtlAcquireSRWLockExclusive", "RtlReleaseSRWLockExclusive", "RtlWakeConditionVariable", "RtlWakeAddressAll", "RtlWaitOnAddress",
    };
    for (size_t i = 0; i < sizeof names / sizeof *names; i++) CHECKF(t_proc_nt(names[i]) != NULL, "ntdll!%s", names[i]);
}
