#include "so.h"

/* A/W dispatch helpers: w=0 -> A entry point, w=1 -> W entry point */
static HANDLE ev_create(int w, SECURITY_ATTRIBUTES *sa, BOOL manual, BOOL init, const char *name) {
    WCHAR wn[600];
    if (w) { if (name) so_wide(name, wn, 600); return U(CreateEventW)(sa, manual, init, name ? wn : NULL); }
    return U(CreateEventA)(sa, manual, init, name);
}
static HANDLE ev_createex(int w, SECURITY_ATTRIBUTES *sa, const char *name, DWORD flags, DWORD access) {
    WCHAR wn[600];
    if (w) { if (name) so_wide(name, wn, 600); return U(CreateEventExW)(sa, name ? wn : NULL, flags, access); }
    return U(CreateEventExA)(sa, name, flags, access);
}
static HANDLE ev_open(int w, DWORD access, BOOL inh, const char *name) {
    WCHAR wn[600];
    if (w) { if (name) so_wide(name, wn, 600); return U(OpenEventW)(access, inh, name ? wn : NULL); }
    return U(OpenEventA)(access, inh, name);
}
#define NEED_EV() do { REQUIRE(U(CreateEventA)); REQUIRE(U(CreateEventW)); REQUIRE(U(CreateEventExA)); REQUIRE(U(CreateEventExW)); \
    REQUIRE(U(OpenEventA)); REQUIRE(U(OpenEventW)); REQUIRE(U(SetEvent)); REQUIRE(U(ResetEvent)); } while (0)

TEST(ev_create_manual_initial_set) {
    NEED_EV();
    for (int w = 0; w < 2; w++) {
        HANDLE h = ev_create(w, NULL, TRUE, TRUE, NULL);
        CHECKF(h != NULL, "w=%d", w); if (!h) continue;
        EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
        EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
        EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
        CloseHandle(h);
    }
}
TEST(ev_create_manual_initial_clear) {
    NEED_EV();
    for (int w = 0; w < 2; w++) {
        HANDLE h = ev_create(w, NULL, TRUE, FALSE, NULL);
        CHECK(h != NULL); if (!h) continue;
        EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_TIMEOUT);
        CloseHandle(h);
    }
}
TEST(ev_create_auto_initial_set_consumed_by_wait) {
    NEED_EV();
    for (int w = 0; w < 2; w++) {
        HANDLE h = ev_create(w, NULL, FALSE, TRUE, NULL);
        CHECK(h != NULL); if (!h) continue;
        EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
        EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_TIMEOUT);
        CloseHandle(h);
    }
}
TEST(ev_create_auto_initial_clear) {
    NEED_EV();
    for (int w = 0; w < 2; w++) {
        HANDLE h = ev_create(w, NULL, FALSE, FALSE, NULL);
        CHECK(h != NULL); if (!h) continue;
        EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_TIMEOUT);
        CloseHandle(h);
    }
}
TEST(ev_create_nonzero_bool_values) {
    NEED_EV();
    /* any non-zero BOOL counts as TRUE */
    HANDLE h = U(CreateEventA)(NULL, 0x100, 0x200, NULL);
    CHECK(h != NULL); if (!h) return;
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);   /* manual */
    CloseHandle(h);
    h = U(CreateEventW)(NULL, (BOOL)0x80000000, 0, NULL);
    CHECK(h != NULL); if (!h) return;
    EXPECT_EQ(U(SetEvent)(h), TRUE);
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
    CloseHandle(h);
}
TEST(ev_create_unnamed_clears_last_error) {
    NEED_EV();
    for (int w = 0; w < 2; w++) {
        SET_ERR(1234);
        HANDLE h = ev_create(w, NULL, TRUE, FALSE, NULL);
        CHECK(h != NULL);
        EXPECT_ERR(0);
        if (h) CloseHandle(h);
        SET_ERR(1234);
        h = ev_createex(w, NULL, NULL, 0, EVENT_ALL_ACCESS);
        CHECK(h != NULL);
        EXPECT_ERR(0);
        if (h) CloseHandle(h);
    }
}
TEST(ev_create_distinct_handles) {
    NEED_EV();
    HANDLE a = U(CreateEventA)(NULL, TRUE, FALSE, NULL), b = U(CreateEventA)(NULL, TRUE, FALSE, NULL);
    CHECK(a && b && a != b);
    if (a && b) { U(SetEvent)(a); EXPECT_EQ(WaitForSingleObject(a, 0), WAIT_OBJECT_0); EXPECT_EQ(WaitForSingleObject(b, 0), WAIT_TIMEOUT); }
    if (a) CloseHandle(a);
    if (b) CloseHandle(b);
}
TEST(ev_handle_value_is_multiple_of_four) {
    NEED_EV();
    HANDLE h = U(CreateEventW)(NULL, TRUE, FALSE, NULL);
    CHECK(h != NULL);
    CHECK(((ULONG_PTR)h & 3) == 0);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h) CloseHandle(h);
}
TEST(ev_set_reset_manual) {
    NEED_EV();
    HANDLE h = U(CreateEventA)(NULL, TRUE, FALSE, NULL);
    CHECK(h != NULL); if (!h) return;
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_TIMEOUT);
    EXPECT_EQ(U(SetEvent)(h), TRUE);
    EXPECT_EQ(U(SetEvent)(h), TRUE);    /* idempotent */
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
    EXPECT_EQ(U(ResetEvent)(h), TRUE);
    EXPECT_EQ(U(ResetEvent)(h), TRUE);  /* idempotent */
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_TIMEOUT);
    EXPECT_EQ(U(SetEvent)(h), TRUE);
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
    CloseHandle(h);
}
TEST(ev_set_auto_single_release) {
    NEED_EV();
    HANDLE h = U(CreateEventA)(NULL, FALSE, FALSE, NULL);
    CHECK(h != NULL); if (!h) return;
    EXPECT_EQ(U(SetEvent)(h), TRUE);
    EXPECT_EQ(U(SetEvent)(h), TRUE);    /* two sets collapse into one signal */
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_TIMEOUT);
    EXPECT_EQ(U(SetEvent)(h), TRUE);
    EXPECT_EQ(U(ResetEvent)(h), TRUE);
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_TIMEOUT);
    CloseHandle(h);
}
TEST(ev_reset_on_auto_clears_initial) {
    NEED_EV();
    HANDLE h = U(CreateEventA)(NULL, FALSE, TRUE, NULL);
    CHECK(h != NULL); if (!h) return;
    EXPECT_EQ(U(ResetEvent)(h), TRUE);
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_TIMEOUT);
    CloseHandle(h);
}

/* waiter threads for wake-count tests */
typedef struct { HANDLE ev; volatile LONG ready; volatile LONG woke; DWORD timeout; } waitctx;
static DWORD WINAPI ev_waiter(LPVOID p) {
    waitctx *c = p;
    InterlockedIncrement(&c->ready);
    DWORD r = WaitForSingleObject(c->ev, c->timeout);
    if (r == WAIT_OBJECT_0) InterlockedIncrement(&c->woke);
    return r;
}
static void run_waiters(int n, waitctx *c, so_thr *t) {
    for (int i = 0; i < n; i++) t[i] = so_start(ev_waiter, c);
    so_spin_ge(&c->ready, n, 3000);
    Sleep(150);
}
TEST(ev_set_auto_wakes_exactly_one_waiter) {
    NEED_EV();
    waitctx c = { U(CreateEventA)(NULL, FALSE, FALSE, NULL), 0, 0, 1500 };
    CHECK(c.ev != NULL); if (!c.ev) return;
    so_thr t[3]; run_waiters(3, &c, t);
    U(SetEvent)(c.ev);
    Sleep(300);
    EXPECT_EQ(c.woke, 1);
    for (int i = 0; i < 3; i++) so_join(t[i], 5000);
    EXPECT_EQ(c.woke, 1);   /* the other two timed out */
    CloseHandle(c.ev);
}
TEST(ev_set_manual_wakes_all_waiters) {
    NEED_EV();
    waitctx c = { U(CreateEventA)(NULL, TRUE, FALSE, NULL), 0, 0, 4000 };
    CHECK(c.ev != NULL); if (!c.ev) return;
    so_thr t[4]; run_waiters(4, &c, t);
    U(SetEvent)(c.ev);
    for (int i = 0; i < 4; i++) EXPECT_EQ(so_join(t[i], 5000), WAIT_OBJECT_0);
    EXPECT_EQ(c.woke, 4);
    CloseHandle(c.ev);
}
TEST(ev_pulse_manual_wakes_all_then_clears) {
    NEED_EV(); REQUIRE(U(PulseEvent));
    waitctx c = { U(CreateEventA)(NULL, TRUE, FALSE, NULL), 0, 0, 4000 };
    CHECK(c.ev != NULL); if (!c.ev) return;
    so_thr t[3]; run_waiters(3, &c, t);
    EXPECT_EQ(U(PulseEvent)(c.ev), TRUE);
    for (int i = 0; i < 3; i++) EXPECT_EQ(so_join(t[i], 5000), WAIT_OBJECT_0);
    EXPECT_EQ(c.woke, 3);
    EXPECT_EQ(WaitForSingleObject(c.ev, 0), WAIT_TIMEOUT);
    CloseHandle(c.ev);
}
TEST(ev_pulse_auto_wakes_one_waiter) {
    NEED_EV(); REQUIRE(U(PulseEvent));
    waitctx c = { U(CreateEventA)(NULL, FALSE, FALSE, NULL), 0, 0, 1200 };
    CHECK(c.ev != NULL); if (!c.ev) return;
    so_thr t[3]; run_waiters(3, &c, t);
    EXPECT_EQ(U(PulseEvent)(c.ev), TRUE);
    Sleep(300);
    EXPECT_EQ(c.woke, 1);
    EXPECT_EQ(WaitForSingleObject(c.ev, 0), WAIT_TIMEOUT);
    for (int i = 0; i < 3; i++) so_join(t[i], 5000);
    CloseHandle(c.ev);
}
TEST(ev_pulse_without_waiters_leaves_nonsignaled) {
    NEED_EV(); REQUIRE(U(PulseEvent));
    for (int m = 0; m < 2; m++) {
        HANDLE h = U(CreateEventA)(NULL, m, FALSE, NULL);
        CHECK(h != NULL); if (!h) continue;
        EXPECT_EQ(U(PulseEvent)(h), TRUE);
        EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_TIMEOUT);
        CloseHandle(h);
    }
}
TEST(ev_pulse_on_initially_set_event_clears_it) {
    NEED_EV(); REQUIRE(U(PulseEvent));
    HANDLE h = U(CreateEventA)(NULL, TRUE, TRUE, NULL);
    CHECK(h != NULL); if (!h) return;
    EXPECT_EQ(U(PulseEvent)(h), TRUE);
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_TIMEOUT);
    CloseHandle(h);
}
TEST(ev_invalid_handle_set_reset_pulse) {
    NEED_EV(); REQUIRE(U(PulseEvent));
    HANDLE bad[] = { NULL, (HANDLE)(ULONG_PTR)0x7777ABC0, (HANDLE)(LONG_PTR)-2 };
    for (size_t i = 0; i < 3; i++) {
        SET_ERR(0); EXPECT_EQ(U(SetEvent)(bad[i]), FALSE);   EXPECT_ERR(ERROR_INVALID_HANDLE);
        SET_ERR(0); EXPECT_EQ(U(ResetEvent)(bad[i]), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
        SET_ERR(0); EXPECT_EQ(U(PulseEvent)(bad[i]), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
}
TEST(ev_invalid_handle_value_minus_one) {
    NEED_EV(); REQUIRE(U(PulseEvent));
    /* -1 is the current-process pseudo handle: valid handle, wrong type */
    SET_ERR(0); EXPECT_EQ(U(SetEvent)(INVALID_HANDLE_VALUE), FALSE);   EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(U(ResetEvent)(INVALID_HANDLE_VALUE), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(U(PulseEvent)(INVALID_HANDLE_VALUE), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(ev_closed_handle_operations_fail) {
    NEED_EV();
    HANDLE h = U(CreateEventA)(NULL, TRUE, TRUE, NULL);
    CHECK(h != NULL); if (!h) return;
    CloseHandle(h);
    SET_ERR(0); EXPECT_EQ(U(SetEvent)(h), FALSE);   EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(U(ResetEvent)(h), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(ev_type_confusion_with_other_objects) {
    NEED_EV(); REQUIRE(U(PulseEvent));
    HANDLE o[5]; const char *nm[5] = { "mutex", "semaphore", "timer", "thread-pseudo", "process-pseudo" };
    o[0] = CreateMutexA(NULL, FALSE, NULL);
    o[1] = CreateSemaphoreA(NULL, 0, 1, NULL);
    o[2] = CreateWaitableTimerA(NULL, TRUE, NULL);
    o[3] = GetCurrentThread();
    o[4] = GetCurrentProcess();
    for (int i = 0; i < 5; i++) {
        CHECKF(o[i] != NULL, "%s", nm[i]); if (!o[i]) continue;
        SET_ERR(0); EXPECT_EQ(U(SetEvent)(o[i]), FALSE);   EXPECT_ERR(ERROR_INVALID_HANDLE);
        SET_ERR(0); EXPECT_EQ(U(ResetEvent)(o[i]), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
        SET_ERR(0); EXPECT_EQ(U(PulseEvent)(o[i]), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
    for (int i = 0; i < 3; i++) if (o[i]) CloseHandle(o[i]);
}
TEST(ev_event_handle_rejected_by_other_apis) {
    NEED_EV();
    HANDLE e = U(CreateEventA)(NULL, TRUE, FALSE, NULL);
    CHECK(e != NULL); if (!e) return;
    LONG prev = 77;
    SET_ERR(0); EXPECT_EQ(ReleaseSemaphore(e, 1, &prev), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(ReleaseMutex(e), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    LARGE_INTEGER li; li.QuadPart = -10000000;
    SET_ERR(0); EXPECT_EQ(SetWaitableTimer(e, &li, 0, NULL, NULL, FALSE), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(CancelWaitableTimer(e), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(e);
}

/* Ex flags */
TEST(ev_ex_flags_matrix) {
    NEED_EV();
    for (int w = 0; w < 2; w++)
    for (DWORD fl = 0; fl < 4; fl++) {
        HANDLE h = ev_createex(w, NULL, NULL, fl, EVENT_ALL_ACCESS);
        CHECKF(h != NULL, "flags=%lu w=%d", fl, w); if (!h) continue;
        BOOL manual = (fl & CREATE_EVENT_MANUAL_RESET) != 0, init = (fl & CREATE_EVENT_INITIAL_SET) != 0;
        DWORD r1 = WaitForSingleObject(h, 0), r2 = WaitForSingleObject(h, 0);
        EXPECT_EQ(r1, init ? WAIT_OBJECT_0 : WAIT_TIMEOUT);
        EXPECT_EQ(r2, init && manual ? WAIT_OBJECT_0 : WAIT_TIMEOUT);
        CloseHandle(h);
    }
}
TEST(ev_ex_flag_constants) {
    EXPECT_EQ(CREATE_EVENT_MANUAL_RESET, 1);
    EXPECT_EQ(CREATE_EVENT_INITIAL_SET, 2);
    EXPECT_EQ(EVENT_MODIFY_STATE, 2);
    EXPECT_EQ(EVENT_ALL_ACCESS, 0x1F0003);
}
TEST(ev_ex_invalid_flags_rejected) {
    NEED_EV();
    DWORD bad[] = { 4, 8, 0x10, 0x100, 0x80000000, 0xFFFFFFFC, 0xFFFFFFFF, 0x7FFFFFFC };
    for (int w = 0; w < 2; w++)
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        SET_ERR(0);
        HANDLE h = ev_createex(w, NULL, NULL, bad[i], EVENT_ALL_ACCESS);
        CHECKF(h == NULL, "flags=0x%lx w=%d", bad[i], w);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        if (h) CloseHandle(h);
    }
}
TEST(ev_ex_invalid_flags_with_name_not_created) {
    NEED_EV();
    char n[80]; so_name(n, sizeof n, "exbad");
    HANDLE h = U(CreateEventExA)(NULL, n, 0x10, EVENT_ALL_ACCESS);
    CHECK(h == NULL);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    HANDLE o = U(OpenEventA)(SYNCHRONIZE, FALSE, n);
    CHECK(o == NULL);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    if (h) CloseHandle(h);
    if (o) CloseHandle(o);
}
TEST(ev_ex_access_modify_only) {
    NEED_EV();
    for (int w = 0; w < 2; w++) {
        HANDLE h = ev_createex(w, NULL, NULL, CREATE_EVENT_MANUAL_RESET, EVENT_MODIFY_STATE);
        CHECK(h != NULL); if (!h) continue;
        EXPECT_EQ(U(SetEvent)(h), TRUE);
        EXPECT_EQ(U(ResetEvent)(h), TRUE);
        SET_ERR(0);
        EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_FAILED);
        EXPECT_ERR(ERROR_ACCESS_DENIED);
        CloseHandle(h);
    }
}
TEST(ev_ex_access_synchronize_only) {
    NEED_EV(); REQUIRE(U(PulseEvent));
    for (int w = 0; w < 2; w++) {
        HANDLE h = ev_createex(w, NULL, NULL, CREATE_EVENT_MANUAL_RESET | CREATE_EVENT_INITIAL_SET, SYNCHRONIZE);
        CHECK(h != NULL); if (!h) continue;
        EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
        SET_ERR(0); EXPECT_EQ(U(SetEvent)(h), FALSE);   EXPECT_ERR(ERROR_ACCESS_DENIED);
        SET_ERR(0); EXPECT_EQ(U(ResetEvent)(h), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
        SET_ERR(0); EXPECT_EQ(U(PulseEvent)(h), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
        EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);   /* unchanged */
        CloseHandle(h);
    }
}
TEST(ev_ex_access_zero_and_query) {
    NEED_EV();
    HANDLE h = U(CreateEventExW)(NULL, NULL, 0, 0);
    if (!h) { t_note("CreateEventExW access 0 -> NULL, err %lu", GetLastError()); return; }
    t_note("CreateEventExW with access 0 succeeds");
    SET_ERR(0); EXPECT_EQ(U(SetEvent)(h), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(h);
}
TEST(ev_ex_access_all_access_allows_everything) {
    NEED_EV();
    HANDLE h = U(CreateEventExA)(NULL, NULL, 0, EVENT_ALL_ACCESS);
    CHECK(h != NULL); if (!h) return;
    EXPECT_EQ(U(SetEvent)(h), TRUE);
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
    CloseHandle(h);
}
TEST(ev_a_ex_default_access_nonstandard) {
    NEED_EV();
    /* GENERIC_ALL maps to full access */
    HANDLE h = U(CreateEventExW)(NULL, NULL, CREATE_EVENT_MANUAL_RESET, GENERIC_ALL);
    CHECK(h != NULL); if (!h) return;
    EXPECT_EQ(U(SetEvent)(h), TRUE);
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
    CloseHandle(h);
    h = U(CreateEventExW)(NULL, NULL, CREATE_EVENT_MANUAL_RESET, GENERIC_READ);
    if (h) {   /* GENERIC_READ maps to STANDARD_RIGHTS_READ|SYNCHRONIZE|... */
        EXPECT_EQ(U(SetEvent)(h), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
        CloseHandle(h);
    } else t_note("GENERIC_READ event create failed %lu", GetLastError());
}

/* Named events */
TEST(ev_named_create_then_already_exists) {
    NEED_EV();
    for (int w = 0; w < 2; w++) {
        char n[80]; so_name(n, sizeof n, "named1");
        SET_ERR(0);
        HANDLE a = ev_create(w, NULL, TRUE, FALSE, n);
        CHECK(a != NULL); EXPECT_ERR(0);
        SET_ERR(0);
        HANDLE b = ev_create(w, NULL, TRUE, FALSE, n);
        CHECK(b != NULL); EXPECT_ERR(ERROR_ALREADY_EXISTS);
        CHECK(a != b);
        if (a && b) {
            U(SetEvent)(a);
            EXPECT_EQ(WaitForSingleObject(b, 0), WAIT_OBJECT_0);
            U(ResetEvent)(b);
            EXPECT_EQ(WaitForSingleObject(a, 0), WAIT_TIMEOUT);
        }
        if (a) CloseHandle(a);
        if (b) CloseHandle(b);
    }
}
TEST(ev_named_ex_already_exists_and_open) {
    NEED_EV();
    for (int w = 0; w < 2; w++) {
        char n[80]; so_name(n, sizeof n, "named2");
        SET_ERR(0);
        HANDLE a = ev_createex(w, NULL, n, CREATE_EVENT_MANUAL_RESET, EVENT_ALL_ACCESS);
        CHECK(a != NULL); EXPECT_ERR(0);
        SET_ERR(0);
        HANDLE b = ev_createex(w, NULL, n, 0, EVENT_ALL_ACCESS);
        CHECK(b != NULL); EXPECT_ERR(ERROR_ALREADY_EXISTS);
        SET_ERR(0);
        HANDLE c = ev_open(w, EVENT_ALL_ACCESS, FALSE, n);
        CHECK(c != NULL);
        if (a && b && c) { U(SetEvent)(c); EXPECT_EQ(WaitForSingleObject(a, 0), WAIT_OBJECT_0); EXPECT_EQ(WaitForSingleObject(b, 0), WAIT_OBJECT_0); }
        if (a) CloseHandle(a); if (b) CloseHandle(b); if (c) CloseHandle(c);
    }
}
TEST(ev_named_reopen_ignores_state_args) {
    NEED_EV();
    char n[80]; so_name(n, sizeof n, "named3");
    HANDLE a = U(CreateEventA)(NULL, TRUE, FALSE, n);       /* manual, clear */
    HANDLE b = U(CreateEventA)(NULL, FALSE, TRUE, n);       /* args must be ignored */
    CHECK(a && b); if (!a || !b) return;
    EXPECT_EQ(WaitForSingleObject(a, 0), WAIT_TIMEOUT);     /* initial state not applied */
    U(SetEvent)(a);
    EXPECT_EQ(WaitForSingleObject(b, 0), WAIT_OBJECT_0);
    EXPECT_EQ(WaitForSingleObject(b, 0), WAIT_OBJECT_0);    /* still manual-reset */
    CloseHandle(a); CloseHandle(b);
}
TEST(ev_named_auto_reset_shared_state) {
    NEED_EV();
    char n[80]; so_name(n, sizeof n, "named4");
    HANDLE a = U(CreateEventA)(NULL, FALSE, FALSE, n);
    HANDLE b = U(OpenEventA)(EVENT_ALL_ACCESS, FALSE, n);
    CHECK(a && b); if (!a || !b) return;
    U(SetEvent)(a);
    EXPECT_EQ(WaitForSingleObject(b, 0), WAIT_OBJECT_0);
    EXPECT_EQ(WaitForSingleObject(a, 0), WAIT_TIMEOUT);     /* consumed through b */
    CloseHandle(a); CloseHandle(b);
}
TEST(ev_named_lifetime_follows_last_handle) {
    NEED_EV();
    char n[80]; so_name(n, sizeof n, "named5");
    HANDLE a = U(CreateEventA)(NULL, TRUE, FALSE, n);
    HANDLE b = U(OpenEventA)(EVENT_ALL_ACCESS, FALSE, n);
    CHECK(a && b); if (!a || !b) return;
    CloseHandle(a);
    SET_ERR(0);
    HANDLE c = U(OpenEventA)(EVENT_ALL_ACCESS, FALSE, n);
    CHECK(c != NULL);
    if (c) CloseHandle(c);
    CloseHandle(b);
    SET_ERR(0);
    c = U(OpenEventA)(EVENT_ALL_ACCESS, FALSE, n);
    CHECK(c == NULL); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    if (c) CloseHandle(c);
    /* a new object of the same name starts fresh */
    SET_ERR(0);
    a = U(CreateEventA)(NULL, TRUE, FALSE, n);
    CHECK(a != NULL); EXPECT_ERR(0);
    if (a) CloseHandle(a);
}
TEST(ev_named_case_insensitive) {
    NEED_EV();
    /* BaseFormatObjectAttributes sets only OBJ_OPENIF (no OBJ_CASE_INSENSITIVE); the outcome depends on the
       kernel object manager, so it is reported only (MSDN says names are case sensitive). */
    char n[80], up[80]; so_name(n, sizeof n, "CaseTest");
    strcpy(up, n); for (char *p = up; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
    CHECK(strcmp(n, up) != 0);
    HANDLE a = U(CreateEventA)(NULL, TRUE, FALSE, n);
    CHECK(a != NULL);
    SET_ERR(0);
    HANDLE b = U(CreateEventA)(NULL, TRUE, FALSE, up);
    t_note("upper-case name create -> %p err %lu (183 = same object)", b, GetLastError());
    if (a) CloseHandle(a); if (b) CloseHandle(b);
}
TEST(ev_named_type_conflicts) {
    NEED_EV();
    char n[80]; so_name(n, sizeof n, "conflict");
    /* event created first, then other object types with the same name */
    HANDLE e = U(CreateEventA)(NULL, TRUE, FALSE, n);
    CHECK(e != NULL); if (!e) return;
    SET_ERR(0); HANDLE m = CreateMutexA(NULL, FALSE, n);               CHECK(m == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE); if (m) CloseHandle(m);
    SET_ERR(0); HANDLE s = CreateSemaphoreA(NULL, 0, 1, n);            CHECK(s == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE); if (s) CloseHandle(s);
    SET_ERR(0); HANDLE t = CreateWaitableTimerA(NULL, TRUE, n);        CHECK(t == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE); if (t) CloseHandle(t);
    SET_ERR(0); HANDLE f = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, n);
    CHECK(f == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE); if (f) CloseHandle(f);
    SET_ERR(0); m = OpenMutexA(SYNCHRONIZE, FALSE, n);                 CHECK(m == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE); if (m) CloseHandle(m);
    SET_ERR(0); s = OpenSemaphoreA(SYNCHRONIZE, FALSE, n);             CHECK(s == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE); if (s) CloseHandle(s);
    SET_ERR(0); t = OpenWaitableTimerA(SYNCHRONIZE, FALSE, n);         CHECK(t == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE); if (t) CloseHandle(t);
    CloseHandle(e);
}
TEST(ev_named_type_conflict_reverse) {
    NEED_EV();
    char n[80]; so_name(n, sizeof n, "conflict2");
    HANDLE m = CreateMutexA(NULL, FALSE, n);
    CHECK(m != NULL); if (!m) return;
    SET_ERR(0); HANDLE e = U(CreateEventA)(NULL, TRUE, FALSE, n); CHECK(e == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE); if (e) CloseHandle(e);
    SET_ERR(0); e = U(CreateEventExW)(NULL, L"", 0, EVENT_ALL_ACCESS); if (e) CloseHandle(e);
    SET_ERR(0); e = U(OpenEventA)(SYNCHRONIZE, FALSE, n); CHECK(e == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE); if (e) CloseHandle(e);
    CloseHandle(m);
}
TEST(ev_open_nonexistent) {
    NEED_EV();
    for (int w = 0; w < 2; w++) {
        char n[80]; so_name(n, sizeof n, "nonexistent");
        SET_ERR(0);
        HANDLE h = ev_open(w, EVENT_ALL_ACCESS, FALSE, n);
        CHECK(h == NULL); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
        if (h) CloseHandle(h);
    }
}
TEST(ev_open_null_name) {
    NEED_EV();
    SET_ERR(0); HANDLE h = U(OpenEventA)(EVENT_ALL_ACCESS, FALSE, NULL); CHECK(h == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER); if (h) CloseHandle(h);
    SET_ERR(0); h = U(OpenEventW)(EVENT_ALL_ACCESS, FALSE, NULL); CHECK(h == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER); if (h) CloseHandle(h);
}
TEST(ev_open_empty_name_note) {
    NEED_EV();
    SET_ERR(0); HANDLE h = U(OpenEventW)(EVENT_ALL_ACCESS, FALSE, L"");
    t_note("OpenEventW(\"\") -> %p err %lu", h, GetLastError());
    CHECK(h == NULL);
    if (h) CloseHandle(h);
}
TEST(ev_open_access_rights_subset) {
    NEED_EV(); REQUIRE(U(PulseEvent));
    char n[80]; so_name(n, sizeof n, "openacc");
    HANDLE c = U(CreateEventA)(NULL, TRUE, FALSE, n);
    CHECK(c != NULL); if (!c) return;
    HANDLE ms = U(OpenEventA)(EVENT_MODIFY_STATE, FALSE, n);
    HANDLE sy = U(OpenEventA)(SYNCHRONIZE, FALSE, n);
    CHECK(ms && sy);
    if (ms && sy) {
        EXPECT_EQ(U(SetEvent)(ms), TRUE);
        SET_ERR(0); EXPECT_EQ(WaitForSingleObject(ms, 0), WAIT_FAILED); EXPECT_ERR(ERROR_ACCESS_DENIED);
        EXPECT_EQ(WaitForSingleObject(sy, 0), WAIT_OBJECT_0);
        SET_ERR(0); EXPECT_EQ(U(ResetEvent)(sy), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
        EXPECT_EQ(U(ResetEvent)(ms), TRUE);
        EXPECT_EQ(WaitForSingleObject(sy, 0), WAIT_TIMEOUT);
    }
    if (ms) CloseHandle(ms); if (sy) CloseHandle(sy); CloseHandle(c);
}
TEST(ev_open_denied_by_empty_dacl) {
    NEED_EV();
    for (int w = 0; w < 2; w++) {
        char n[80]; so_name(n, sizeof n, "deny");
        so_denysa d; SECURITY_ATTRIBUTES *sa = so_deny(&d, FALSE);
        HANDLE c = ev_create(w, sa, TRUE, FALSE, n);
        CHECK(c != NULL); if (!c) continue;
        EXPECT_EQ(U(SetEvent)(c), TRUE);   /* creator handle has the requested access */
        SET_ERR(0); HANDLE o = ev_open(w, SYNCHRONIZE, FALSE, n);
        CHECK(o == NULL); EXPECT_ERR(ERROR_ACCESS_DENIED);
        if (o) CloseHandle(o);
        SET_ERR(0); o = ev_open(w, EVENT_ALL_ACCESS, FALSE, n);
        CHECK(o == NULL); EXPECT_ERR(ERROR_ACCESS_DENIED);
        if (o) CloseHandle(o);
        /* CreateEvent on the existing name also needs access */
        SET_ERR(0); o = ev_create(w, NULL, TRUE, FALSE, n);
        CHECK(o == NULL); EXPECT_ERR(ERROR_ACCESS_DENIED);
        if (o) CloseHandle(o);
        CloseHandle(c);
    }
}
TEST(ev_open_inherit_flag) {
    NEED_EV();
    char n[80]; so_name(n, sizeof n, "inh");
    HANDLE c = U(CreateEventA)(NULL, TRUE, FALSE, n);
    CHECK(c != NULL); if (!c) return;
    EXPECT_EQ(so_inheritable(c), 0);
    for (int w = 0; w < 2; w++) {
        HANDLE a = ev_open(w, SYNCHRONIZE, TRUE, n), b = ev_open(w, SYNCHRONIZE, FALSE, n);
        CHECK(a && b);
        if (a) { EXPECT_EQ(so_inheritable(a), 1); CloseHandle(a); }
        if (b) { EXPECT_EQ(so_inheritable(b), 0); CloseHandle(b); }
    }
    CloseHandle(c);
}
TEST(ev_create_sa_inherit_flag) {
    NEED_EV();
    SECURITY_ATTRIBUTES yes = { sizeof yes, NULL, TRUE }, no = { sizeof no, NULL, FALSE };
    for (int w = 0; w < 2; w++) {
        HANDLE a = ev_create(w, &yes, TRUE, FALSE, NULL), b = ev_create(w, &no, TRUE, FALSE, NULL), c = ev_create(w, NULL, TRUE, FALSE, NULL);
        CHECK(a && b && c);
        if (a) { EXPECT_EQ(so_inheritable(a), 1); CloseHandle(a); }
        if (b) { EXPECT_EQ(so_inheritable(b), 0); CloseHandle(b); }
        if (c) { EXPECT_EQ(so_inheritable(c), 0); CloseHandle(c); }
        a = ev_createex(w, &yes, NULL, 0, EVENT_ALL_ACCESS); b = ev_createex(w, &no, NULL, 0, EVENT_ALL_ACCESS);
        CHECK(a && b);
        if (a) { EXPECT_EQ(so_inheritable(a), 1); CloseHandle(a); }
        if (b) { EXPECT_EQ(so_inheritable(b), 0); CloseHandle(b); }
    }
}
TEST(ev_create_sa_inherit_named) {
    NEED_EV();
    SECURITY_ATTRIBUTES yes = { sizeof yes, NULL, TRUE };
    char n[80]; so_name(n, sizeof n, "inhnamed");
    HANDLE a = U(CreateEventA)(&yes, TRUE, FALSE, n);
    CHECK(a != NULL); if (!a) return;
    EXPECT_EQ(so_inheritable(a), 1);
    CloseHandle(a);
}
TEST(ev_create_sa_length_ignored) {
    NEED_EV();
    SECURITY_ATTRIBUTES sa = { 0, NULL, TRUE };   /* nLength not validated by CreateEvent */
    HANDLE a = U(CreateEventA)(&sa, TRUE, FALSE, NULL);
    t_note("CreateEventA with nLength=0 -> %p err %lu", a, GetLastError());
    if (a) { EXPECT_EQ(so_inheritable(a), 1); CloseHandle(a); }
}
TEST(ev_name_with_backslash) {
    NEED_EV();
    char n[120]; so_name(n, sizeof n, "dir");
    strcat(n, "\\leaf");
    SET_ERR(0);
    HANDLE h = U(CreateEventA)(NULL, TRUE, FALSE, n);
    CHECK(h == NULL);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_PATH_NOT_FOUND || e == ERROR_INVALID_NAME || e == ERROR_FILE_NOT_FOUND, "err=%lu", e);
    t_note("CreateEventA(\"a\\\\b\") err %lu", e);
    if (h) CloseHandle(h);
    SET_ERR(0);
    h = U(CreateEventA)(NULL, TRUE, FALSE, "\\");
    t_note("CreateEventA(\"\\\") -> %p err %lu", h, GetLastError());
    if (h) CloseHandle(h);
}
TEST(ev_name_long) {
    NEED_EV();
    static char big[1100];
    memset(big, 'L', sizeof big - 1); big[sizeof big - 1] = 0;
    char pre[40]; so_name(pre, sizeof pre, "L");
    memcpy(big, pre, strlen(pre));
    static const int lens[] = { 100, 259, 260, 261, 400, 1000 };
    for (size_t i = 0; i < sizeof lens / sizeof *lens; i++) {
        char save = big[lens[i]]; big[lens[i]] = 0;
        SET_ERR(0);
        HANDLE h = U(CreateEventA)(NULL, TRUE, FALSE, big);
        DWORD e = GetLastError();
        t_note("event name len %d -> %s err %lu", lens[i], h ? "ok" : "NULL", e);
        if (lens[i] <= 259) CHECKF(h != NULL, "len=%d err=%lu", lens[i], e);
        if (h) {
            HANDLE o = U(OpenEventA)(SYNCHRONIZE, FALSE, big);
            CHECKF(o != NULL, "reopen len=%d", lens[i]);
            if (o) CloseHandle(o);
            CloseHandle(h);
        }
        big[lens[i]] = save;
    }
}
TEST(ev_name_very_long_rejected_or_ok) {
    NEED_EV();
    /* name longer than a UNICODE_STRING can hold must not crash */
    enum { N = 33000 };
    WCHAR *w = HeapAlloc(GetProcessHeap(), 0, (N + 1) * sizeof(WCHAR));
    CHECK(w != NULL); if (!w) return;
    for (int i = 0; i < N; i++) w[i] = L'v';
    w[N] = 0;
    SET_ERR(0);
    HANDLE h = U(CreateEventW)(NULL, TRUE, FALSE, w);
    t_note("32K+ name -> %p err %lu", h, GetLastError());
    if (h) CloseHandle(h);
    SET_ERR(0);
    h = U(OpenEventW)(SYNCHRONIZE, FALSE, w);
    t_note("32K+ open -> %p err %lu", h, GetLastError());
    CHECK(h == NULL);
    if (h) CloseHandle(h);
    HeapFree(GetProcessHeap(), 0, w);
}
TEST(ev_name_prefix_local) {
    NEED_EV();
    char base[80], loc[100]; so_name(base, sizeof base, "pfxL");
    snprintf(loc, sizeof loc, "Local\\%s", base);
    HANDLE a = U(CreateEventA)(NULL, TRUE, FALSE, loc);
    CHECK(a != NULL); EXPECT_ERR(0); if (!a) return;
    SET_ERR(0);
    HANDLE b = U(OpenEventA)(SYNCHRONIZE, FALSE, base);   /* unprefixed == Local */
    CHECK(b != NULL);
    if (b) { U(SetEvent)(a); EXPECT_EQ(WaitForSingleObject(b, 0), WAIT_OBJECT_0); CloseHandle(b); }
    SET_ERR(0);
    b = U(CreateEventA)(NULL, TRUE, FALSE, base);
    CHECK(b != NULL); EXPECT_ERR(ERROR_ALREADY_EXISTS);
    if (b) CloseHandle(b);
    CloseHandle(a);
}
TEST(ev_name_prefix_local_case) {
    NEED_EV();
    char base[80], loc[100], loc2[100]; so_name(base, sizeof base, "pfxLc");
    snprintf(loc, sizeof loc, "Local\\%s", base);
    snprintf(loc2, sizeof loc2, "LOCAL\\%s", base);
    HANDLE a = U(CreateEventA)(NULL, TRUE, FALSE, loc);
    CHECK(a != NULL); if (!a) return;
    SET_ERR(0);
    HANDLE b = U(OpenEventA)(SYNCHRONIZE, FALSE, loc2);
    t_note("LOCAL\\ alias open -> %p err %lu (kernel case rules not verified)", b, GetLastError());
    if (b) CloseHandle(b);
    CloseHandle(a);
}
TEST(ev_name_prefix_global) {
    NEED_EV();
    char base[80], g[100]; so_name(base, sizeof base, "pfxG");
    snprintf(g, sizeof g, "Global\\%s", base);
    SET_ERR(0);
    HANDLE a = U(CreateEventA)(NULL, TRUE, FALSE, g);
    if (!a) { t_note("Global\\ create failed err %lu", GetLastError()); return; }
    EXPECT_ERR(0);
    SET_ERR(0);
    HANDLE b = U(OpenEventA)(SYNCHRONIZE, FALSE, g);
    CHECK(b != NULL);
    if (b) { U(SetEvent)(a); EXPECT_EQ(WaitForSingleObject(b, 0), WAIT_OBJECT_0); CloseHandle(b); }
    if (so_session() != 0) {
        /* in session > 0 the Local namespace differs from Global */
        SET_ERR(0);
        b = U(OpenEventA)(SYNCHRONIZE, FALSE, base);
        CHECK(b == NULL); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
        if (b) CloseHandle(b);
    }
    CloseHandle(a);
}
TEST(ev_name_prefix_session_note) {
    NEED_EV();
    char base[80], s[160]; so_name(base, sizeof base, "pfxS");
    snprintf(s, sizeof s, "Session\\%lu\\%s", so_session(), base);
    SET_ERR(0);
    HANDLE a = U(CreateEventA)(NULL, TRUE, FALSE, s);
    t_note("Session\\N\\name create -> %p err %lu", a, GetLastError());
    if (a) CloseHandle(a);
    snprintf(s, sizeof s, "Session\\%lu\\BaseNamedObjects\\%s", so_session(), base);
    SET_ERR(0);
    a = U(CreateEventA)(NULL, TRUE, FALSE, s);
    t_note("Session\\N\\BaseNamedObjects\\name create -> %p err %lu", a, GetLastError());
    if (a) CloseHandle(a);
}
TEST(ev_name_local_prefix_is_not_part_of_object_identity) {
    NEED_EV();
    char base[80], loc[100]; so_name(base, sizeof base, "pfxL2");
    snprintf(loc, sizeof loc, "Local\\%s", base);
    HANDLE a = U(CreateEventA)(NULL, TRUE, FALSE, base);
    SET_ERR(0);
    HANDLE b = U(CreateEventA)(NULL, TRUE, FALSE, loc);
    CHECK(a && b); EXPECT_ERR(ERROR_ALREADY_EXISTS);
    if (a) CloseHandle(a); if (b) CloseHandle(b);
}
TEST(ev_name_unicode_w_roundtrip) {
    NEED_EV();
    WCHAR n[100]; wsprintfW(n, L"k32t_%lu_uni_\x00e9\x4e2d\x6587\x0416", GetCurrentProcessId());
    HANDLE a = U(CreateEventW)(NULL, TRUE, FALSE, n);
    CHECK(a != NULL); if (!a) return;
    SET_ERR(0);
    HANDLE b = U(OpenEventW)(SYNCHRONIZE, FALSE, n);
    CHECK(b != NULL);
    if (b) CloseHandle(b);
    CloseHandle(a);
}
TEST(ev_name_a_w_same_object) {
    NEED_EV();
    char n[80]; so_name(n, sizeof n, "aw"); WCHAR w[80]; so_wide(n, w, 80);
    HANDLE a = U(CreateEventA)(NULL, TRUE, FALSE, n);
    SET_ERR(0);
    HANDLE b = U(CreateEventW)(NULL, TRUE, FALSE, w);
    CHECK(a && b); EXPECT_ERR(ERROR_ALREADY_EXISTS);
    SET_ERR(0);
    HANDLE c = U(OpenEventW)(SYNCHRONIZE, FALSE, w), d = U(OpenEventA)(SYNCHRONIZE, FALSE, n);
    CHECK(c && d);
    if (a && c) { U(SetEvent)(a); EXPECT_EQ(WaitForSingleObject(c, 0), WAIT_OBJECT_0); EXPECT_EQ(WaitForSingleObject(d, 0), WAIT_OBJECT_0); }
    if (a) CloseHandle(a); if (b) CloseHandle(b); if (c) CloseHandle(c); if (d) CloseHandle(d);
}
TEST(ev_name_empty_string_unnamed_or_error) {
    NEED_EV();
    SET_ERR(0);
    HANDLE a = U(CreateEventW)(NULL, TRUE, FALSE, L"");
    DWORD e = GetLastError();
    t_note("CreateEventW(\"\") -> %p err %lu", a, e);
    SET_ERR(0);
    HANDLE b = U(CreateEventW)(NULL, TRUE, FALSE, L"");
    t_note("2nd CreateEventW(\"\") -> %p err %lu", b, GetLastError());
    if (a) CloseHandle(a); if (b) CloseHandle(b);
}
TEST(ev_name_with_spaces_and_dots) {
    NEED_EV();
    const char *tails[] = { " space", "dot.dot", "..", "trail ", "semi;colon", "a/b", "caret^", "pct%25" };
    for (size_t i = 0; i < sizeof tails / sizeof *tails; i++) {
        char n[120], b[80]; so_name(b, sizeof b, "chars");
        snprintf(n, sizeof n, "%s%s", b, tails[i]);
        SET_ERR(0);
        HANDLE h = U(CreateEventA)(NULL, TRUE, FALSE, n);
        CHECKF(h != NULL, "tail '%s' err=%lu", tails[i], GetLastError());
        if (h) {
            HANDLE o = U(OpenEventA)(SYNCHRONIZE, FALSE, n);
            CHECKF(o != NULL, "reopen '%s'", tails[i]);
            if (o) CloseHandle(o);
            CloseHandle(h);
        }
    }
}
TEST(ev_duplicate_handle_waits) {
    NEED_EV();
    HANDLE h = U(CreateEventA)(NULL, TRUE, FALSE, NULL), d = NULL;
    CHECK(h != NULL); if (!h) return;
    CHECK(DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS));
    CHECK(d && d != h);
    if (d) {
        EXPECT_EQ(WaitForSingleObject(d, 0), WAIT_TIMEOUT);
        U(SetEvent)(h);
        EXPECT_EQ(WaitForSingleObject(d, 0), WAIT_OBJECT_0);
        CloseHandle(h);
        EXPECT_EQ(U(ResetEvent)(d), TRUE);   /* object survives the first close */
        EXPECT_EQ(WaitForSingleObject(d, 0), WAIT_TIMEOUT);
        CloseHandle(d);
    } else CloseHandle(h);
}
TEST(ev_duplicate_handle_reduced_access) {
    NEED_EV();
    HANDLE h = U(CreateEventA)(NULL, TRUE, FALSE, NULL), d = NULL, d2 = NULL;
    CHECK(h != NULL); if (!h) return;
    CHECK(DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(), &d, SYNCHRONIZE, FALSE, 0));
    CHECK(DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(), &d2, EVENT_MODIFY_STATE, FALSE, 0));
    if (d && d2) {
        SET_ERR(0); EXPECT_EQ(U(SetEvent)(d), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
        EXPECT_EQ(U(SetEvent)(d2), TRUE);
        EXPECT_EQ(WaitForSingleObject(d, 0), WAIT_OBJECT_0);
        SET_ERR(0); EXPECT_EQ(WaitForSingleObject(d2, 0), WAIT_FAILED); EXPECT_ERR(ERROR_ACCESS_DENIED);
    }
    if (d) CloseHandle(d); if (d2) CloseHandle(d2); CloseHandle(h);
}
TEST(ev_duplicate_close_source_flag) {
    NEED_EV();
    HANDLE h = U(CreateEventA)(NULL, TRUE, TRUE, NULL), d = NULL;
    CHECK(h != NULL); if (!h) return;
    CHECK(DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE));
    /* Win10 ObDuplicateObject closes the source before creating the target handle, so in the same
     * process the new handle can reuse the closed value (d == h). */
    SET_ERR(0);
    if (d != h) { EXPECT_EQ(U(SetEvent)(h), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE); }
    else EXPECT_EQ(U(SetEvent)(h), TRUE);
    if (d) { EXPECT_EQ(WaitForSingleObject(d, 0), WAIT_OBJECT_0); CloseHandle(d); }
}
TEST(ev_handle_flags_protect_from_close) {
    NEED_EV();
    HANDLE h = U(CreateEventA)(NULL, TRUE, FALSE, NULL);
    CHECK(h != NULL); if (!h) return;
    CHECK(SetHandleInformation(h, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT));
    EXPECT_EQ(so_inheritable(h), 1);
    CHECK(SetHandleInformation(h, HANDLE_FLAG_INHERIT, 0));
    EXPECT_EQ(so_inheritable(h), 0);
    CloseHandle(h);
}
static DWORD WINAPI ev_setter(LPVOID p) { Sleep(100); U(SetEvent)((HANDLE)p); return 0; }
TEST(ev_cross_thread_set_wakes_main) {
    NEED_EV();
    HANDLE h = U(CreateEventA)(NULL, FALSE, FALSE, NULL);
    CHECK(h != NULL); if (!h) return;
    so_thr t = so_start(ev_setter, h);
    EXPECT_EQ(WaitForSingleObject(h, 5000), WAIT_OBJECT_0);
    so_join(t, 3000);
    CloseHandle(h);
}
TEST(ev_many_create_close_loop) {
    NEED_EV();
    HANDLE hs[500];
    int ok = 0;
    for (int i = 0; i < 500; i++) { hs[i] = U(CreateEventW)(NULL, i & 1, i & 2, NULL); if (hs[i]) ok++; }
    EXPECT_EQ(ok, 500);
    for (int i = 0; i < 500; i++) if (hs[i]) { EXPECT_EQ(WaitForSingleObject(hs[i], 0), ((i & 2) ? WAIT_OBJECT_0 : WAIT_TIMEOUT)); CloseHandle(hs[i]); }
    for (int i = 0; i < 2000; i++) { HANDLE h = U(CreateEventA)(NULL, TRUE, FALSE, NULL); if (!h) { CHECKF(0, "iter %d", i); break; } CloseHandle(h); }
}
TEST(ev_ex_named_flags_apply_only_on_create) {
    NEED_EV();
    char n[80]; so_name(n, sizeof n, "exnamed");
    HANDLE a = U(CreateEventExA)(NULL, n, CREATE_EVENT_MANUAL_RESET | CREATE_EVENT_INITIAL_SET, EVENT_ALL_ACCESS);
    HANDLE b = U(CreateEventExA)(NULL, n, 0, EVENT_ALL_ACCESS);
    CHECK(a && b); if (!a || !b) return;
    EXPECT_EQ(WaitForSingleObject(b, 0), WAIT_OBJECT_0);
    EXPECT_EQ(WaitForSingleObject(b, 0), WAIT_OBJECT_0);
    CloseHandle(a); CloseHandle(b);
}
TEST(ev_ex_named_existing_with_lesser_access) {
    NEED_EV();
    char n[80]; so_name(n, sizeof n, "exless");
    HANDLE a = U(CreateEventExA)(NULL, n, CREATE_EVENT_MANUAL_RESET, EVENT_ALL_ACCESS);
    SET_ERR(0);
    HANDLE b = U(CreateEventExA)(NULL, n, 0, SYNCHRONIZE);
    CHECK(a && b); EXPECT_ERR(ERROR_ALREADY_EXISTS);
    if (b) { SET_ERR(0); EXPECT_EQ(U(SetEvent)(b), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED); }
    if (a) CloseHandle(a); if (b) CloseHandle(b);
}
TEST(ev_wait_on_event_timeout_values) {
    NEED_EV();
    HANDLE h = U(CreateEventA)(NULL, TRUE, FALSE, NULL);
    CHECK(h != NULL); if (!h) return;
    DWORD t0 = GetTickCount();
    EXPECT_EQ(WaitForSingleObject(h, 50), WAIT_TIMEOUT);
    DWORD dt = GetTickCount() - t0;
    CHECKF(dt >= 30 && dt < 2000, "elapsed %lu", dt);
    CloseHandle(h);
}
