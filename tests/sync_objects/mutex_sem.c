#include "so.h"

static HANDLE mx_create(int w, SECURITY_ATTRIBUTES *sa, BOOL owner, const char *name) {
    WCHAR wn[300];
    if (w) { if (name) so_wide(name, wn, 300); return U(CreateMutexW)(sa, owner, name ? wn : NULL); }
    return U(CreateMutexA)(sa, owner, name);
}
static HANDLE mx_createex(int w, SECURITY_ATTRIBUTES *sa, const char *name, DWORD fl, DWORD acc) {
    WCHAR wn[300];
    if (w) { if (name) so_wide(name, wn, 300); return U(CreateMutexExW)(sa, name ? wn : NULL, fl, acc); }
    return U(CreateMutexExA)(sa, name, fl, acc);
}
static HANDLE mx_open(int w, DWORD acc, BOOL inh, const char *name) {
    WCHAR wn[300];
    if (w) { if (name) so_wide(name, wn, 300); return U(OpenMutexW)(acc, inh, name ? wn : NULL); }
    return U(OpenMutexA)(acc, inh, name);
}
#define NEED_MX() do { REQUIRE(U(CreateMutexA)); REQUIRE(U(CreateMutexW)); REQUIRE(U(CreateMutexExA)); REQUIRE(U(CreateMutexExW)); \
    REQUIRE(U(OpenMutexA)); REQUIRE(U(OpenMutexW)); REQUIRE(U(ReleaseMutex)); } while (0)

static HANDLE sm_create(int w, SECURITY_ATTRIBUTES *sa, LONG i, LONG m, const char *name) {
    WCHAR wn[300];
    if (w) { if (name) so_wide(name, wn, 300); return U(CreateSemaphoreW)(sa, i, m, name ? wn : NULL); }
    return U(CreateSemaphoreA)(sa, i, m, name);
}
static HANDLE sm_createex(int w, SECURITY_ATTRIBUTES *sa, LONG i, LONG m, const char *name, DWORD fl, DWORD acc) {
    WCHAR wn[300];
    if (w) { if (name) so_wide(name, wn, 300); return U(CreateSemaphoreExW)(sa, i, m, name ? wn : NULL, fl, acc); }
    return U(CreateSemaphoreExA)(sa, i, m, name, fl, acc);
}
static HANDLE sm_open(int w, DWORD acc, BOOL inh, const char *name) {
    WCHAR wn[300];
    if (w) { if (name) so_wide(name, wn, 300); return U(OpenSemaphoreW)(acc, inh, name ? wn : NULL); }
    return U(OpenSemaphoreA)(acc, inh, name);
}
#define NEED_SM() do { REQUIRE(U(CreateSemaphoreA)); REQUIRE(U(CreateSemaphoreW)); REQUIRE(U(CreateSemaphoreExA)); REQUIRE(U(CreateSemaphoreExW)); \
    REQUIRE(U(OpenSemaphoreA)); REQUIRE(U(OpenSemaphoreW)); REQUIRE(U(ReleaseSemaphore)); } while (0)

/* ======================= mutex ======================= */
TEST(mutex_create_unowned_wait_release) {
    NEED_MX();
    for (int w = 0; w < 2; w++) {
        HANDLE h = mx_create(w, NULL, FALSE, NULL);
        CHECK(h != NULL); if (!h) continue;
        SET_ERR(0); EXPECT_EQ(U(ReleaseMutex)(h), FALSE); EXPECT_ERR(ERROR_NOT_OWNER);
        EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
        EXPECT_EQ(U(ReleaseMutex)(h), TRUE);
        SET_ERR(0); EXPECT_EQ(U(ReleaseMutex)(h), FALSE); EXPECT_ERR(ERROR_NOT_OWNER);
        CloseHandle(h);
    }
}
TEST(mutex_create_initial_owner) {
    NEED_MX();
    for (int w = 0; w < 2; w++) {
        HANDLE h = mx_create(w, NULL, TRUE, NULL);
        CHECK(h != NULL); if (!h) continue;
        EXPECT_EQ(U(ReleaseMutex)(h), TRUE);
        SET_ERR(0); EXPECT_EQ(U(ReleaseMutex)(h), FALSE); EXPECT_ERR(ERROR_NOT_OWNER);
        CloseHandle(h);
    }
}
TEST(mutex_initial_owner_recursion_count_is_one) {
    NEED_MX();
    HANDLE h = U(CreateMutexA)(NULL, TRUE, NULL);
    CHECK(h != NULL); if (!h) return;
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);   /* recursion 2 */
    EXPECT_EQ(U(ReleaseMutex)(h), TRUE);
    EXPECT_EQ(U(ReleaseMutex)(h), TRUE);
    SET_ERR(0); EXPECT_EQ(U(ReleaseMutex)(h), FALSE); EXPECT_ERR(ERROR_NOT_OWNER);
    CloseHandle(h);
}
TEST(mutex_recursive_acquire_release_many) {
    NEED_MX();
    HANDLE h = U(CreateMutexW)(NULL, FALSE, NULL);
    CHECK(h != NULL); if (!h) return;
    for (int i = 0; i < 1000; i++) if (WaitForSingleObject(h, 0) != WAIT_OBJECT_0) { CHECKF(0, "acq %d", i); break; }
    int rel = 0;
    for (int i = 0; i < 1000; i++) if (U(ReleaseMutex)(h)) rel++;
    EXPECT_EQ(rel, 1000);
    SET_ERR(0); EXPECT_EQ(U(ReleaseMutex)(h), FALSE); EXPECT_ERR(ERROR_NOT_OWNER);
    CloseHandle(h);
}
TEST(mutex_ex_flags) {
    NEED_MX();
    for (int w = 0; w < 2; w++) {
        HANDLE h = mx_createex(w, NULL, NULL, CREATE_MUTEX_INITIAL_OWNER, MUTEX_ALL_ACCESS);
        CHECK(h != NULL);
        if (h) { EXPECT_EQ(U(ReleaseMutex)(h), TRUE); SET_ERR(0); EXPECT_EQ(U(ReleaseMutex)(h), FALSE); EXPECT_ERR(ERROR_NOT_OWNER); CloseHandle(h); }
        h = mx_createex(w, NULL, NULL, 0, MUTEX_ALL_ACCESS);
        CHECK(h != NULL);
        if (h) { SET_ERR(0); EXPECT_EQ(U(ReleaseMutex)(h), FALSE); EXPECT_ERR(ERROR_NOT_OWNER); CloseHandle(h); }
    }
    EXPECT_EQ(CREATE_MUTEX_INITIAL_OWNER, 1);
    EXPECT_EQ(MUTEX_MODIFY_STATE, 1);
}
TEST(mutex_ex_invalid_flags) {
    NEED_MX();
    DWORD bad[] = { 2, 4, 0x80, 0x100, 0x80000000, 0xFFFFFFFE, 0xFFFFFFFF };
    for (int w = 0; w < 2; w++)
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        SET_ERR(0);
        HANDLE h = mx_createex(w, NULL, NULL, bad[i], MUTEX_ALL_ACCESS);
        CHECKF(h == NULL, "flags 0x%lx", bad[i]); EXPECT_ERR(ERROR_INVALID_PARAMETER);
        if (h) CloseHandle(h);
    }
}
TEST(mutex_ex_access_modify_only) {
    NEED_MX();
    HANDLE h = U(CreateMutexExW)(NULL, NULL, CREATE_MUTEX_INITIAL_OWNER, MUTEX_MODIFY_STATE);
    CHECK(h != NULL); if (!h) return;
    SET_ERR(0); EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_FAILED); EXPECT_ERR(ERROR_ACCESS_DENIED);
    EXPECT_EQ(U(ReleaseMutex)(h), TRUE);
    CloseHandle(h);
}
TEST(mutex_ex_access_synchronize_only) {
    NEED_MX();
    HANDLE h = U(CreateMutexExA)(NULL, NULL, 0, SYNCHRONIZE);
    CHECK(h != NULL); if (!h) return;
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
    /* ReleaseMutex needs no specific right; being the owner is what counts */
    BOOL r = U(ReleaseMutex)(h);
    t_note("ReleaseMutex with SYNCHRONIZE-only handle -> %d err %lu", r, r ? 0 : GetLastError());
    CloseHandle(h);
}
TEST(mutex_invalid_handles_release) {
    NEED_MX();
    HANDLE bad[] = { NULL, (HANDLE)(ULONG_PTR)0x7777ABC4, (HANDLE)(LONG_PTR)-2, INVALID_HANDLE_VALUE };
    for (size_t i = 0; i < 4; i++) {
        SET_ERR(0); EXPECT_EQ(U(ReleaseMutex)(bad[i]), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
    HANDLE h = U(CreateMutexA)(NULL, TRUE, NULL); CloseHandle(h);
    SET_ERR(0); EXPECT_EQ(U(ReleaseMutex)(h), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(mutex_release_wrong_type) {
    NEED_MX();
    HANDLE e = CreateEventA(NULL, TRUE, TRUE, NULL), s = CreateSemaphoreA(NULL, 1, 1, NULL);
    SET_ERR(0); EXPECT_EQ(U(ReleaseMutex)(e), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(U(ReleaseMutex)(s), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(U(ReleaseMutex)(GetCurrentThread()), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(e); CloseHandle(s);
}
typedef struct { HANDLE h; volatile LONG state; DWORD r; BOOL rel; DWORD relerr; } mxctx;
static DWORD WINAPI mx_try_thread(LPVOID p) {
    mxctx *c = p;
    c->r = WaitForSingleObject(c->h, 100);
    if (c->r == WAIT_OBJECT_0 || c->r == WAIT_ABANDONED) ReleaseMutex(c->h);
    return 0;
}
TEST(mutex_other_thread_cannot_acquire_while_owned) {
    NEED_MX();
    HANDLE h = U(CreateMutexA)(NULL, TRUE, NULL);
    CHECK(h != NULL); if (!h) return;
    mxctx c = { h };
    so_join(so_start(mx_try_thread, &c), 5000);
    EXPECT_EQ(c.r, WAIT_TIMEOUT);
    U(ReleaseMutex)(h);
    so_join(so_start(mx_try_thread, &c), 5000);
    EXPECT_EQ(c.r, WAIT_OBJECT_0);
    CloseHandle(h);
}
static DWORD WINAPI mx_release_thread(LPVOID p) {
    mxctx *c = p;
    SetLastError(0);
    c->rel = U(ReleaseMutex)(c->h);
    c->relerr = GetLastError();
    return 0;
}
TEST(mutex_release_from_non_owner_thread_fails) {
    NEED_MX();
    HANDLE h = U(CreateMutexA)(NULL, TRUE, NULL);
    CHECK(h != NULL); if (!h) return;
    mxctx c = { h };
    so_join(so_start(mx_release_thread, &c), 5000);
    EXPECT_EQ(c.rel, FALSE); EXPECT_EQ(c.relerr, ERROR_NOT_OWNER);
    EXPECT_EQ(U(ReleaseMutex)(h), TRUE);
    CloseHandle(h);
}
static DWORD WINAPI mx_abandon_thread(LPVOID p) {
    mxctx *c = p;
    WaitForSingleObject(c->h, 1000);
    WaitForSingleObject(c->h, 1000);   /* recursion 2, never released */
    InterlockedExchange(&c->state, 1);
    return 0;
}
TEST(mutex_abandoned_when_owner_thread_exits) {
    NEED_MX();
    HANDLE h = U(CreateMutexA)(NULL, FALSE, NULL);
    CHECK(h != NULL); if (!h) return;
    mxctx c = { h };
    so_join(so_start(mx_abandon_thread, &c), 5000);
    EXPECT_EQ(c.state, 1);
    EXPECT_EQ(WaitForSingleObject(h, 1000), WAIT_ABANDONED_0);
    EXPECT_EQ(WAIT_ABANDONED, 0x80);
    /* we own it now, with recursion count 1 */
    EXPECT_EQ(U(ReleaseMutex)(h), TRUE);
    SET_ERR(0); EXPECT_EQ(U(ReleaseMutex)(h), FALSE); EXPECT_ERR(ERROR_NOT_OWNER);
    /* abandoned flag is consumed */
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
    EXPECT_EQ(U(ReleaseMutex)(h), TRUE);
    CloseHandle(h);
}
TEST(mutex_abandoned_via_initial_owner_thread) {
    NEED_MX();
    /* a mutex created owned by a worker thread, which then exits */
    static HANDLE g;
    g = NULL;
    DWORD WINAPI cr(LPVOID p) { g = U(CreateMutexA)(NULL, TRUE, NULL); return 0; }
    so_join(so_start(cr, NULL), 5000);
    CHECK(g != NULL); if (!g) return;
    EXPECT_EQ(WaitForSingleObject(g, 1000), WAIT_ABANDONED_0);
    EXPECT_EQ(U(ReleaseMutex)(g), TRUE);
    CloseHandle(g);
}
TEST(mutex_abandoned_wakes_blocked_waiter) {
    NEED_MX();
    HANDLE h = U(CreateMutexA)(NULL, FALSE, NULL);
    static HANDLE hh; hh = h;
    static volatile LONG have;
    have = 0;
    DWORD WINAPI holder(LPVOID p) { WaitForSingleObject(hh, 1000); InterlockedExchange(&have, 1); Sleep(300); return 0; }
    so_thr t = so_start(holder, NULL);
    CHECK(so_spin_until(&have, 1, 3000));
    EXPECT_EQ(WaitForSingleObject(h, 4000), WAIT_ABANDONED_0);
    U(ReleaseMutex)(h);
    so_join(t, 3000);
    CloseHandle(h);
}
TEST(mutex_wait_by_owner_recursion_zero_timeout) {
    NEED_MX();
    HANDLE h = U(CreateMutexA)(NULL, TRUE, NULL);
    CHECK(h != NULL); if (!h) return;
    for (int i = 0; i < 5; i++) EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
    for (int i = 0; i < 6; i++) EXPECT_EQ(U(ReleaseMutex)(h), TRUE);
    SET_ERR(0); EXPECT_EQ(U(ReleaseMutex)(h), FALSE); EXPECT_ERR(ERROR_NOT_OWNER);
    CloseHandle(h);
}
TEST(mutex_named_already_exists) {
    NEED_MX();
    for (int w = 0; w < 2; w++) {
        char n[80]; so_name(n, sizeof n, "mxn");
        SET_ERR(0); HANDLE a = mx_create(w, NULL, FALSE, n); CHECK(a != NULL); EXPECT_ERR(0);
        SET_ERR(0); HANDLE b = mx_create(w, NULL, FALSE, n); CHECK(b != NULL); EXPECT_ERR(ERROR_ALREADY_EXISTS);
        SET_ERR(0); HANDLE c = mx_createex(w, NULL, n, 0, MUTEX_ALL_ACCESS); CHECK(c != NULL); EXPECT_ERR(ERROR_ALREADY_EXISTS);
        SET_ERR(0); HANDLE d = mx_open(w, MUTEX_ALL_ACCESS, FALSE, n); CHECK(d != NULL);
        if (a && b && c && d) {
            EXPECT_EQ(WaitForSingleObject(a, 0), WAIT_OBJECT_0);
            EXPECT_EQ(WaitForSingleObject(d, 0), WAIT_OBJECT_0);    /* same thread: recursive */
            EXPECT_EQ(U(ReleaseMutex)(b), TRUE);                    /* ownership is per thread, any handle */
            EXPECT_EQ(U(ReleaseMutex)(c), TRUE);
            SET_ERR(0); EXPECT_EQ(U(ReleaseMutex)(a), FALSE); EXPECT_ERR(ERROR_NOT_OWNER);
        }
        if (a) CloseHandle(a); if (b) CloseHandle(b); if (c) CloseHandle(c); if (d) CloseHandle(d);
    }
}
TEST(mutex_named_initial_owner_ignored_on_existing) {
    NEED_MX();
    char n[80]; so_name(n, sizeof n, "mxown");
    HANDLE a = U(CreateMutexA)(NULL, FALSE, n);
    SET_ERR(0);
    HANDLE b = U(CreateMutexA)(NULL, TRUE, n);
    CHECK(a && b); EXPECT_ERR(ERROR_ALREADY_EXISTS); if (!a || !b) return;
    SET_ERR(0); EXPECT_EQ(U(ReleaseMutex)(a), FALSE); EXPECT_ERR(ERROR_NOT_OWNER);
    CloseHandle(a); CloseHandle(b);
}
TEST(mutex_named_initial_owner_new) {
    NEED_MX();
    char n[80]; so_name(n, sizeof n, "mxown2");
    SET_ERR(0);
    HANDLE a = U(CreateMutexA)(NULL, TRUE, n);
    CHECK(a != NULL); EXPECT_ERR(0); if (!a) return;
    mxctx c = { a };
    so_join(so_start(mx_try_thread, &c), 5000);
    EXPECT_EQ(c.r, WAIT_TIMEOUT);
    EXPECT_EQ(U(ReleaseMutex)(a), TRUE);
    CloseHandle(a);
}
TEST(mutex_open_errors) {
    NEED_MX();
    for (int w = 0; w < 2; w++) {
        char n[80]; so_name(n, sizeof n, "mxopen");
        SET_ERR(0); HANDLE h = mx_open(w, SYNCHRONIZE, FALSE, n); CHECK(h == NULL); EXPECT_ERR(ERROR_FILE_NOT_FOUND); if (h) CloseHandle(h);
        SET_ERR(0); h = mx_open(w, SYNCHRONIZE, FALSE, NULL); CHECK(h == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER); if (h) CloseHandle(h);
    }
}
TEST(mutex_open_access_and_inherit) {
    NEED_MX();
    char n[80]; so_name(n, sizeof n, "mxacc");
    HANDLE c = U(CreateMutexA)(NULL, FALSE, n);
    CHECK(c != NULL); if (!c) return;
    HANDLE sy = U(OpenMutexA)(SYNCHRONIZE, TRUE, n), ms;
    WCHAR wn[80]; so_wide(n, wn, 80);
    ms = U(OpenMutexW)(MUTEX_MODIFY_STATE, FALSE, wn);
    CHECK(sy && ms);
    if (sy) { EXPECT_EQ(so_inheritable(sy), 1); EXPECT_EQ(WaitForSingleObject(sy, 0), WAIT_OBJECT_0); U(ReleaseMutex)(sy); }
    if (ms) { EXPECT_EQ(so_inheritable(ms), 0); SET_ERR(0); EXPECT_EQ(WaitForSingleObject(ms, 0), WAIT_FAILED); EXPECT_ERR(ERROR_ACCESS_DENIED); }
    if (sy) CloseHandle(sy); if (ms) CloseHandle(ms); CloseHandle(c);
}
TEST(mutex_open_denied_by_empty_dacl) {
    NEED_MX();
    char n[80]; so_name(n, sizeof n, "mxdeny");
    so_denysa d; SECURITY_ATTRIBUTES *sa = so_deny(&d, FALSE);
    HANDLE c = U(CreateMutexA)(sa, FALSE, n);
    CHECK(c != NULL); if (!c) return;
    SET_ERR(0); HANDLE o = U(OpenMutexA)(SYNCHRONIZE, FALSE, n);
    CHECK(o == NULL); EXPECT_ERR(ERROR_ACCESS_DENIED);
    if (o) CloseHandle(o);
    CloseHandle(c);
}
TEST(mutex_type_conflicts) {
    NEED_MX();
    char n[80]; so_name(n, sizeof n, "mxconf");
    HANDLE m = U(CreateMutexA)(NULL, FALSE, n);
    CHECK(m != NULL); if (!m) return;
    SET_ERR(0); HANDLE e = CreateEventA(NULL, TRUE, FALSE, n); CHECK(e == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE); if (e) CloseHandle(e);
    SET_ERR(0); e = CreateSemaphoreA(NULL, 0, 1, n); CHECK(e == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE); if (e) CloseHandle(e);
    SET_ERR(0); e = OpenEventA(SYNCHRONIZE, FALSE, n); CHECK(e == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE); if (e) CloseHandle(e);
    SET_ERR(0); e = OpenSemaphoreA(SYNCHRONIZE, FALSE, n); CHECK(e == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE); if (e) CloseHandle(e);
    CloseHandle(m);
}
TEST(mutex_sa_inherit) {
    NEED_MX();
    SECURITY_ATTRIBUTES yes = { sizeof yes, NULL, TRUE };
    for (int w = 0; w < 2; w++) {
        HANDLE a = mx_create(w, &yes, FALSE, NULL), b = mx_create(w, NULL, FALSE, NULL);
        HANDLE c = mx_createex(w, &yes, NULL, 0, MUTEX_ALL_ACCESS);
        CHECK(a && b && c);
        if (a) { EXPECT_EQ(so_inheritable(a), 1); CloseHandle(a); }
        if (b) { EXPECT_EQ(so_inheritable(b), 0); CloseHandle(b); }
        if (c) { EXPECT_EQ(so_inheritable(c), 1); CloseHandle(c); }
    }
}
TEST(mutex_duplicate_handle_shares_ownership) {
    NEED_MX();
    HANDLE h = U(CreateMutexA)(NULL, TRUE, NULL), d = NULL;
    CHECK(h != NULL); if (!h) return;
    CHECK(DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS));
    if (d) {
        EXPECT_EQ(WaitForSingleObject(d, 0), WAIT_OBJECT_0);   /* recursive via dup */
        EXPECT_EQ(U(ReleaseMutex)(d), TRUE);
        EXPECT_EQ(U(ReleaseMutex)(h), TRUE);
        SET_ERR(0); EXPECT_EQ(U(ReleaseMutex)(d), FALSE); EXPECT_ERR(ERROR_NOT_OWNER);
        CloseHandle(d);
    }
    CloseHandle(h);
}
TEST(mutex_name_long_and_backslash) {
    NEED_MX();
    char n[100]; so_name(n, sizeof n, "x"); strcat(n, "\\y");
    SET_ERR(0); HANDLE h = U(CreateMutexA)(NULL, FALSE, n);
    CHECK(h == NULL);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_PATH_NOT_FOUND || e == ERROR_INVALID_NAME || e == ERROR_FILE_NOT_FOUND, "err %lu", e);
    if (h) CloseHandle(h);
}
TEST(mutex_local_global_prefix) {
    NEED_MX();
    char base[80], l[100], g[100]; so_name(base, sizeof base, "mxpfx");
    snprintf(l, sizeof l, "Local\\%s", base); snprintf(g, sizeof g, "Global\\%s", base);
    HANDLE a = U(CreateMutexA)(NULL, FALSE, l);
    CHECK(a != NULL); if (!a) return;
    SET_ERR(0); HANDLE b = U(OpenMutexA)(SYNCHRONIZE, FALSE, base); CHECK(b != NULL); if (b) CloseHandle(b);
    if (so_session() != 0) { SET_ERR(0); b = U(OpenMutexA)(SYNCHRONIZE, FALSE, g); CHECK(b == NULL); EXPECT_ERR(ERROR_FILE_NOT_FOUND); if (b) CloseHandle(b); }
    SET_ERR(0); HANDLE c = U(CreateMutexA)(NULL, FALSE, g);
    if (c) { t_note("Global mutex create ok err %lu", GetLastError()); CloseHandle(c); } else t_note("Global mutex create failed %lu", GetLastError());
    CloseHandle(a);
}
TEST(mutex_exclusion_counter) {
    NEED_MX();
    static HANDLE h; static volatile LONG inside, bad; static int shared;
    h = U(CreateMutexA)(NULL, FALSE, NULL); inside = bad = 0; shared = 0;
    CHECK(h != NULL); if (!h) return;
    DWORD WINAPI worker(LPVOID p) {
        for (int i = 0; i < 300; i++) {
            if (WaitForSingleObject(h, 5000) != WAIT_OBJECT_0) { InterlockedIncrement(&bad); return 1; }
            if (InterlockedIncrement(&inside) != 1) InterlockedIncrement(&bad);
            shared++;
            InterlockedDecrement(&inside);
            U(ReleaseMutex)(h);
        }
        return 0;
    }
    so_thr t[4];
    for (int i = 0; i < 4; i++) t[i] = so_start(worker, NULL);
    for (int i = 0; i < 4; i++) so_join(t[i], 20000);
    EXPECT_EQ(bad, 0); EXPECT_EQ(shared, 1200);
    CloseHandle(h);
}

/* ======================= semaphore ======================= */
TEST(sem_create_validation_table) {
    NEED_SM();
    static const struct { LONG i, m; int ok; } tab[] = {
        { 0, 1, 1 }, { 1, 1, 1 }, { 0, 100, 1 }, { 100, 100, 1 }, { 5, 10, 1 }, { 0, LONG_MAX, 1 }, { LONG_MAX, LONG_MAX, 1 },
        { 0, 0, 0 }, { 1, 0, 0 }, { 2, 1, 0 }, { -1, 1, 0 }, { 0, -1, 0 }, { -1, -1, 0 }, { LONG_MIN, 1, 0 }, { 0, LONG_MIN, 0 }, { LONG_MAX, LONG_MAX - 1, 0 },
    };
    for (int w = 0; w < 2; w++)
    for (size_t k = 0; k < sizeof tab / sizeof *tab; k++) {
        SET_ERR(0);
        HANDLE h = sm_create(w, NULL, tab[k].i, tab[k].m, NULL);
        CHECKF((h != NULL) == tab[k].ok, "init=%ld max=%ld w=%d got %p", tab[k].i, tab[k].m, w, h);
        if (!tab[k].ok) EXPECT_ERR(ERROR_INVALID_PARAMETER);
        if (h) CloseHandle(h);
    }
}
TEST(sem_initial_count_consumed_by_waits) {
    NEED_SM();
    HANDLE h = U(CreateSemaphoreA)(NULL, 3, 5, NULL);
    CHECK(h != NULL); if (!h) return;
    for (int i = 0; i < 3; i++) EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_TIMEOUT);
    CloseHandle(h);
}
TEST(sem_release_previous_count) {
    NEED_SM();
    HANDLE h = U(CreateSemaphoreW)(NULL, 2, 10, NULL);
    CHECK(h != NULL); if (!h) return;
    LONG prev = -99;
    EXPECT_EQ(U(ReleaseSemaphore)(h, 3, &prev), TRUE); EXPECT_EQ(prev, 2);
    EXPECT_EQ(U(ReleaseSemaphore)(h, 1, &prev), TRUE); EXPECT_EQ(prev, 5);
    EXPECT_EQ(U(ReleaseSemaphore)(h, 4, NULL), TRUE);   /* NULL previous count allowed */
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
    EXPECT_EQ(U(ReleaseSemaphore)(h, 1, &prev), TRUE); EXPECT_EQ(prev, 9);
    CloseHandle(h);
}
TEST(sem_release_to_exact_max) {
    NEED_SM();
    HANDLE h = U(CreateSemaphoreA)(NULL, 0, 4, NULL);
    CHECK(h != NULL); if (!h) return;
    LONG prev = -1;
    EXPECT_EQ(U(ReleaseSemaphore)(h, 4, &prev), TRUE); EXPECT_EQ(prev, 0);
    SET_ERR(0); prev = 1234;
    EXPECT_EQ(U(ReleaseSemaphore)(h, 1, &prev), FALSE); EXPECT_ERR(ERROR_TOO_MANY_POSTS);
    EXPECT_EQ(ERROR_TOO_MANY_POSTS, 298);
    for (int i = 0; i < 4; i++) EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_TIMEOUT);
    CloseHandle(h);
}
TEST(sem_release_overflow_leaves_count_untouched) {
    NEED_SM();
    HANDLE h = U(CreateSemaphoreA)(NULL, 3, 5, NULL);
    CHECK(h != NULL); if (!h) return;
    LONG prev = 1234;
    SET_ERR(0); EXPECT_EQ(U(ReleaseSemaphore)(h, 3, &prev), FALSE); EXPECT_ERR(ERROR_TOO_MANY_POSTS);
    SET_ERR(0); EXPECT_EQ(U(ReleaseSemaphore)(h, LONG_MAX, &prev), FALSE); EXPECT_ERR(ERROR_TOO_MANY_POSTS);
    t_note("previous count after failed release: %ld", prev);
    int n = 0; while (WaitForSingleObject(h, 0) == WAIT_OBJECT_0 && n < 10) n++;
    EXPECT_EQ(n, 3);
    CloseHandle(h);
}
TEST(sem_release_invalid_counts) {
    NEED_SM();
    HANDLE h = U(CreateSemaphoreA)(NULL, 0, 10, NULL);
    CHECK(h != NULL); if (!h) return;
    LONG counts[] = { 0, -1, -100, LONG_MIN };
    for (int i = 0; i < 4; i++) { LONG prev = 55; SET_ERR(0); EXPECT_EQ(U(ReleaseSemaphore)(h, counts[i], &prev), FALSE); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_TIMEOUT);
    CloseHandle(h);
}
TEST(sem_release_max_semaphore) {
    NEED_SM();
    HANDLE h = U(CreateSemaphoreA)(NULL, 0, LONG_MAX, NULL);
    CHECK(h != NULL); if (!h) return;
    LONG prev = -1;
    EXPECT_EQ(U(ReleaseSemaphore)(h, LONG_MAX, &prev), TRUE); EXPECT_EQ(prev, 0);
    SET_ERR(0); EXPECT_EQ(U(ReleaseSemaphore)(h, 1, &prev), FALSE); EXPECT_ERR(ERROR_TOO_MANY_POSTS);
    CloseHandle(h);
}
TEST(sem_release_invalid_handles) {
    NEED_SM();
    HANDLE bad[] = { NULL, (HANDLE)(ULONG_PTR)0x7777ABC8, (HANDLE)(LONG_PTR)-2, INVALID_HANDLE_VALUE };
    for (int i = 0; i < 4; i++) { LONG p; SET_ERR(0); EXPECT_EQ(U(ReleaseSemaphore)(bad[i], 1, &p), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE); }
    HANDLE h = U(CreateSemaphoreA)(NULL, 0, 1, NULL); CloseHandle(h);
    SET_ERR(0); EXPECT_EQ(U(ReleaseSemaphore)(h, 1, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(sem_release_wrong_type) {
    NEED_SM();
    HANDLE e = CreateEventA(NULL, TRUE, TRUE, NULL), m = CreateMutexA(NULL, FALSE, NULL), t = CreateWaitableTimerA(NULL, TRUE, NULL);
    HANDLE o[] = { e, m, t, GetCurrentProcess(), GetCurrentThread() };
    for (int i = 0; i < 5; i++) { LONG p; SET_ERR(0); EXPECT_EQ(U(ReleaseSemaphore)(o[i], 1, &p), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE); }
    CloseHandle(e); CloseHandle(m); CloseHandle(t);
}
TEST(sem_release_validation_order) {
    NEED_SM();
    /* invalid count on invalid handle: handle check or count check, record result */
    LONG p = 0; SET_ERR(0);
    BOOL r = U(ReleaseSemaphore)(NULL, 0, &p);
    DWORD e = GetLastError();
    CHECK(!r);
    CHECKF(e == ERROR_INVALID_HANDLE || e == ERROR_INVALID_PARAMETER, "err %lu", e);
    t_note("ReleaseSemaphore(NULL,0) err %lu", e);
}
TEST(sem_ex_flags_must_be_zero) {
    NEED_SM();
    DWORD bad[] = { 1, 2, 4, 0x80000000, 0xFFFFFFFF };
    for (int w = 0; w < 2; w++)
    for (int i = 0; i < 5; i++) {
        SET_ERR(0); HANDLE h = sm_createex(w, NULL, 0, 1, NULL, bad[i], SEMAPHORE_ALL_ACCESS);
        CHECKF(h == NULL, "flags 0x%lx", bad[i]); EXPECT_ERR(ERROR_INVALID_PARAMETER); if (h) CloseHandle(h);
    }
    for (int w = 0; w < 2; w++) { HANDLE h = sm_createex(w, NULL, 1, 1, NULL, 0, SEMAPHORE_ALL_ACCESS); CHECK(h != NULL); if (h) CloseHandle(h); }
}
TEST(sem_ex_count_validation) {
    NEED_SM();
    SET_ERR(0); HANDLE h = U(CreateSemaphoreExA)(NULL, 2, 1, NULL, 0, SEMAPHORE_ALL_ACCESS); CHECK(h == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER); if (h) CloseHandle(h);
    SET_ERR(0); h = U(CreateSemaphoreExW)(NULL, 0, 0, NULL, 0, SEMAPHORE_ALL_ACCESS); CHECK(h == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER); if (h) CloseHandle(h);
    SET_ERR(0); h = U(CreateSemaphoreExW)(NULL, -1, 5, NULL, 0, SEMAPHORE_ALL_ACCESS); CHECK(h == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER); if (h) CloseHandle(h);
}
TEST(sem_ex_access_modify_only) {
    NEED_SM();
    HANDLE h = U(CreateSemaphoreExW)(NULL, 0, 3, NULL, 0, SEMAPHORE_MODIFY_STATE);
    CHECK(h != NULL); if (!h) return;
    LONG prev = -1;
    EXPECT_EQ(U(ReleaseSemaphore)(h, 1, &prev), TRUE); EXPECT_EQ(prev, 0);
    SET_ERR(0); EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_FAILED); EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(h);
}
TEST(sem_ex_access_synchronize_only) {
    NEED_SM();
    HANDLE h = U(CreateSemaphoreExA)(NULL, 1, 3, NULL, 0, SYNCHRONIZE);
    CHECK(h != NULL); if (!h) return;
    LONG prev = 0;
    SET_ERR(0); EXPECT_EQ(U(ReleaseSemaphore)(h, 1, &prev), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_TIMEOUT);
    CloseHandle(h);
}
TEST(sem_constants) {
    EXPECT_EQ(SEMAPHORE_MODIFY_STATE, 2);
    EXPECT_EQ(SEMAPHORE_ALL_ACCESS, 0x1F0003);
    EXPECT_EQ(MUTANT_QUERY_STATE, 1);
    EXPECT_EQ(MUTEX_ALL_ACCESS, 0x1F0001);
    EXPECT_EQ(WAIT_OBJECT_0, 0); EXPECT_EQ(WAIT_TIMEOUT, 258); EXPECT_EQ(WAIT_FAILED, 0xFFFFFFFF); EXPECT_EQ(WAIT_IO_COMPLETION, 192);
    EXPECT_EQ(INFINITE, 0xFFFFFFFF); EXPECT_EQ(MAXIMUM_WAIT_OBJECTS, 64);
}
TEST(sem_named_already_exists) {
    NEED_SM();
    for (int w = 0; w < 2; w++) {
        char n[80]; so_name(n, sizeof n, "smn");
        SET_ERR(0); HANDLE a = sm_create(w, NULL, 0, 5, n); CHECK(a != NULL); EXPECT_ERR(0);
        SET_ERR(0); HANDLE b = sm_create(w, NULL, 3, 9, n); CHECK(b != NULL); EXPECT_ERR(ERROR_ALREADY_EXISTS);
        SET_ERR(0); HANDLE c = sm_open(w, SEMAPHORE_ALL_ACCESS, FALSE, n); CHECK(c != NULL);
        if (a && b && c) {
            EXPECT_EQ(WaitForSingleObject(c, 0), WAIT_TIMEOUT);       /* initial count of 2nd create ignored */
            LONG prev = -1;
            EXPECT_EQ(U(ReleaseSemaphore)(c, 5, &prev), TRUE); EXPECT_EQ(prev, 0);
            SET_ERR(0); EXPECT_EQ(U(ReleaseSemaphore)(a, 1, &prev), FALSE); EXPECT_ERR(ERROR_TOO_MANY_POSTS);   /* max stays 5 */
        }
        if (a) CloseHandle(a); if (b) CloseHandle(b); if (c) CloseHandle(c);
    }
}
TEST(sem_named_reopen_invalid_counts_still_validated) {
    NEED_SM();
    char n[80]; so_name(n, sizeof n, "smv");
    HANDLE a = U(CreateSemaphoreA)(NULL, 0, 5, n);
    CHECK(a != NULL); if (!a) return;
    SET_ERR(0); HANDLE b = U(CreateSemaphoreA)(NULL, 9, 1, n);
    CHECK(b == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    if (b) CloseHandle(b);
    CloseHandle(a);
}
TEST(sem_open_errors_and_conflicts) {
    NEED_SM();
    char n[80]; so_name(n, sizeof n, "smopen");
    for (int w = 0; w < 2; w++) {
        SET_ERR(0); HANDLE h = sm_open(w, SYNCHRONIZE, FALSE, n); CHECK(h == NULL); EXPECT_ERR(ERROR_FILE_NOT_FOUND); if (h) CloseHandle(h);
        SET_ERR(0); h = sm_open(w, SYNCHRONIZE, FALSE, NULL); CHECK(h == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER); if (h) CloseHandle(h);
    }
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, n);
    SET_ERR(0); HANDLE h = U(CreateSemaphoreA)(NULL, 0, 1, n); CHECK(h == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE); if (h) CloseHandle(h);
    SET_ERR(0); h = U(OpenSemaphoreA)(SYNCHRONIZE, FALSE, n); CHECK(h == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE); if (h) CloseHandle(h);
    if (e) CloseHandle(e);
}
TEST(sem_open_access_inherit) {
    NEED_SM();
    char n[80]; so_name(n, sizeof n, "smacc");
    HANDLE c = U(CreateSemaphoreA)(NULL, 1, 2, n);
    CHECK(c != NULL); if (!c) return;
    HANDLE sy = U(OpenSemaphoreA)(SYNCHRONIZE, TRUE, n), ms = U(OpenSemaphoreA)(SEMAPHORE_MODIFY_STATE, FALSE, n);
    CHECK(sy && ms);
    if (sy && ms) {
        EXPECT_EQ(so_inheritable(sy), 1); EXPECT_EQ(so_inheritable(ms), 0);
        EXPECT_EQ(WaitForSingleObject(sy, 0), WAIT_OBJECT_0);
        SET_ERR(0); EXPECT_EQ(U(ReleaseSemaphore)(sy, 1, NULL), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED);
        EXPECT_EQ(U(ReleaseSemaphore)(ms, 1, NULL), TRUE);
        SET_ERR(0); EXPECT_EQ(WaitForSingleObject(ms, 0), WAIT_FAILED); EXPECT_ERR(ERROR_ACCESS_DENIED);
    }
    if (sy) CloseHandle(sy); if (ms) CloseHandle(ms); CloseHandle(c);
}
TEST(sem_open_denied_by_empty_dacl) {
    NEED_SM();
    char n[80]; so_name(n, sizeof n, "smdeny");
    so_denysa d; SECURITY_ATTRIBUTES *sa = so_deny(&d, FALSE);
    HANDLE c = U(CreateSemaphoreA)(sa, 0, 1, n);
    CHECK(c != NULL); if (!c) return;
    SET_ERR(0); HANDLE o = U(OpenSemaphoreA)(SYNCHRONIZE, FALSE, n); CHECK(o == NULL); EXPECT_ERR(ERROR_ACCESS_DENIED);
    if (o) CloseHandle(o);
    CloseHandle(c);
}
TEST(sem_sa_inherit) {
    NEED_SM();
    SECURITY_ATTRIBUTES yes = { sizeof yes, NULL, TRUE };
    for (int w = 0; w < 2; w++) {
        HANDLE a = sm_create(w, &yes, 0, 1, NULL), b = sm_create(w, NULL, 0, 1, NULL), c = sm_createex(w, &yes, 0, 1, NULL, 0, SEMAPHORE_ALL_ACCESS);
        CHECK(a && b && c);
        if (a) { EXPECT_EQ(so_inheritable(a), 1); CloseHandle(a); }
        if (b) { EXPECT_EQ(so_inheritable(b), 0); CloseHandle(b); }
        if (c) { EXPECT_EQ(so_inheritable(c), 1); CloseHandle(c); }
    }
}
typedef struct { HANDLE h; volatile LONG ready, woke; DWORD to; } smctx;
static DWORD WINAPI sm_waiter(LPVOID p) {
    smctx *c = p;
    InterlockedIncrement(&c->ready);
    if (WaitForSingleObject(c->h, c->to) == WAIT_OBJECT_0) InterlockedIncrement(&c->woke);
    return 0;
}
TEST(sem_release_n_wakes_n_waiters) {
    NEED_SM();
    smctx c = { U(CreateSemaphoreA)(NULL, 0, 10, NULL), 0, 0, 1200 };
    CHECK(c.h != NULL); if (!c.h) return;
    so_thr t[4];
    for (int i = 0; i < 4; i++) t[i] = so_start(sm_waiter, &c);
    so_spin_ge(&c.ready, 4, 3000); Sleep(150);
    LONG prev = -1;
    EXPECT_EQ(U(ReleaseSemaphore)(c.h, 2, &prev), TRUE); EXPECT_EQ(prev, 0);
    Sleep(300);
    EXPECT_EQ(c.woke, 2);
    for (int i = 0; i < 4; i++) so_join(t[i], 5000);
    EXPECT_EQ(c.woke, 2);
    CloseHandle(c.h);
}
TEST(sem_release_with_waiters_previous_count_zero) {
    NEED_SM();
    smctx c = { U(CreateSemaphoreA)(NULL, 0, 10, NULL), 0, 0, 3000 };
    CHECK(c.h != NULL); if (!c.h) return;
    so_thr t = so_start(sm_waiter, &c);
    so_spin_ge(&c.ready, 1, 3000); Sleep(150);
    LONG prev = -1;
    EXPECT_EQ(U(ReleaseSemaphore)(c.h, 3, &prev), TRUE); EXPECT_EQ(prev, 0);
    so_join(t, 5000);
    EXPECT_EQ(c.woke, 1);
    EXPECT_EQ(U(ReleaseSemaphore)(c.h, 1, &prev), TRUE); EXPECT_EQ(prev, 2);
    CloseHandle(c.h);
}
TEST(sem_binary_as_lock) {
    NEED_SM();
    static HANDLE h; static volatile LONG inside, bad; static int shared;
    h = U(CreateSemaphoreA)(NULL, 1, 1, NULL); inside = bad = shared = 0;
    CHECK(h != NULL); if (!h) return;
    DWORD WINAPI worker(LPVOID p) {
        for (int i = 0; i < 300; i++) {
            if (WaitForSingleObject(h, 5000) != WAIT_OBJECT_0) { InterlockedIncrement(&bad); return 1; }
            if (InterlockedIncrement(&inside) != 1) InterlockedIncrement(&bad);
            shared++; InterlockedDecrement(&inside);
            if (!U(ReleaseSemaphore)(h, 1, NULL)) InterlockedIncrement(&bad);
        }
        return 0;
    }
    so_thr t[4];
    for (int i = 0; i < 4; i++) t[i] = so_start(worker, NULL);
    for (int i = 0; i < 4; i++) so_join(t[i], 20000);
    EXPECT_EQ(bad, 0); EXPECT_EQ(shared, 1200);
    CloseHandle(h);
}
TEST(sem_duplicate_handle) {
    NEED_SM();
    HANDLE h = U(CreateSemaphoreA)(NULL, 0, 2, NULL), d = NULL;
    CHECK(h != NULL); if (!h) return;
    CHECK(DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS));
    if (d) {
        LONG prev = -1;
        EXPECT_EQ(U(ReleaseSemaphore)(d, 2, &prev), TRUE); EXPECT_EQ(prev, 0);
        CloseHandle(h);
        EXPECT_EQ(WaitForSingleObject(d, 0), WAIT_OBJECT_0);
        EXPECT_EQ(U(ReleaseSemaphore)(d, 1, &prev), TRUE); EXPECT_EQ(prev, 1);
        CloseHandle(d);
    } else CloseHandle(h);
}
TEST(sem_many_create_close) {
    NEED_SM();
    for (int i = 0; i < 1000; i++) { HANDLE h = U(CreateSemaphoreW)(NULL, i % 3, 3, NULL); if (!h) { CHECKF(0, "iter %d", i); break; } CloseHandle(h); }
}
TEST(sem_local_prefix_names) {
    NEED_SM();
    char base[80], l[100]; so_name(base, sizeof base, "smpfx"); snprintf(l, sizeof l, "Local\\%s", base);
    HANDLE a = U(CreateSemaphoreA)(NULL, 0, 1, l);
    SET_ERR(0); HANDLE b = U(CreateSemaphoreA)(NULL, 0, 1, base);
    CHECK(a && b); EXPECT_ERR(ERROR_ALREADY_EXISTS);
    if (a) CloseHandle(a); if (b) CloseHandle(b);
}
TEST(sem_name_backslash) {
    NEED_SM();
    char n[100]; so_name(n, sizeof n, "x"); strcat(n, "\\y");
    SET_ERR(0); HANDLE h = U(CreateSemaphoreA)(NULL, 0, 1, n);
    CHECK(h == NULL); DWORD e = GetLastError();
    CHECKF(e == ERROR_PATH_NOT_FOUND || e == ERROR_INVALID_NAME || e == ERROR_FILE_NOT_FOUND, "err %lu", e);
    if (h) CloseHandle(h);
}
TEST(sem_wait_all_semaphores) {
    NEED_SM();
    HANDLE h[2] = { U(CreateSemaphoreA)(NULL, 1, 3, NULL), U(CreateSemaphoreA)(NULL, 0, 3, NULL) };
    CHECK(h[0] && h[1]); if (!h[0] || !h[1]) return;
    EXPECT_EQ(WaitForMultipleObjects(2, h, TRUE, 0), WAIT_TIMEOUT);
    EXPECT_EQ(WaitForSingleObject(h[0], 0), WAIT_OBJECT_0);   /* wait-all must not have decremented h[0] ... */
    U(ReleaseSemaphore)(h[0], 1, NULL);
    U(ReleaseSemaphore)(h[1], 1, NULL);
    EXPECT_EQ(WaitForMultipleObjects(2, h, TRUE, 0), WAIT_OBJECT_0);
    EXPECT_EQ(WaitForSingleObject(h[0], 0), WAIT_TIMEOUT);
    EXPECT_EQ(WaitForSingleObject(h[1], 0), WAIT_TIMEOUT);
    CloseHandle(h[0]); CloseHandle(h[1]);
}
