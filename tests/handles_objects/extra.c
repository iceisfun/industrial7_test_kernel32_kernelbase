#include "ho.h"

/* Additional handle/object edge cases (named lifetime, A/W parity, wait semantics on duplicates). */

TEST(ho_x_named_event_gone_after_last_close) {
    REQUIRE(U(OpenEventA)); REQUIRE(U(CreateEventA));
    char n[96]; ho_uniq(n, sizeof n, "gone");
    HANDLE a = U(CreateEventA)(NULL, TRUE, FALSE, n);
    CHECK(a != NULL); if (!a) return;
    HANDLE b = U(OpenEventA)(SYNCHRONIZE, FALSE, n);
    CHECK(b != NULL);
    CloseHandle(a);
    HANDLE c = U(OpenEventA)(SYNCHRONIZE, FALSE, n);
    CHECK(c != NULL);              /* b keeps the object alive */
    if (c) CloseHandle(c);
    if (b) CloseHandle(b);
    SET_ERR(0);
    HANDLE d = U(OpenEventA)(SYNCHRONIZE, FALSE, n);
    EXPECT_EQ(d, NULL); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    if (d) CloseHandle(d);
}
TEST(ho_x_create_event_a_w_parity_named) {
    REQUIRE(U(CreateEventA)); REQUIRE(U(CreateEventW)); REQUIRE(U(OpenEventW));
    char n[96]; WCHAR w[96]; ho_uniq(n, sizeof n, "aw"); ho_wide(n, w, 96);
    HANDLE a = U(CreateEventA)(NULL, TRUE, FALSE, n);
    SET_ERR(0);
    HANDLE b = U(CreateEventW)(NULL, TRUE, FALSE, w);
    EXPECT_ERR(ERROR_ALREADY_EXISTS);
    CHECK(a && b);
    if (a && b) {
        SetEvent(a);
        EXPECT_EQ(WaitForSingleObject(b, 0), WAIT_OBJECT_0);
        HANDLE c = U(OpenEventW)(SYNCHRONIZE, FALSE, w);
        CHECK(c != NULL);
        if (c) { EXPECT_EQ(WaitForSingleObject(c, 0), WAIT_OBJECT_0); CloseHandle(c); }
    }
    if (a) CloseHandle(a);
    if (b) CloseHandle(b);
}
TEST(ho_x_open_event_null_name_fails) {
    REQUIRE(U(OpenEventW)); REQUIRE(U(OpenEventA));
    SET_ERR(0);
    HANDLE h = U(OpenEventW)(SYNCHRONIZE, FALSE, NULL);
    EXPECT_EQ(h, NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    if (h) CloseHandle(h);
    SET_ERR(0);
    h = U(OpenEventA)(SYNCHRONIZE, FALSE, NULL);
    EXPECT_EQ(h, NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    if (h) CloseHandle(h);
}
TEST(ho_x_open_event_missing_name) {
    REQUIRE(U(OpenEventA)); REQUIRE(U(OpenMutexA)); REQUIRE(U(OpenSemaphoreA));
    char n[96]; ho_uniq(n, sizeof n, "missing");
    SET_ERR(0); HANDLE h = U(OpenEventA)(SYNCHRONIZE, FALSE, n);
    EXPECT_EQ(h, NULL); EXPECT_ERR(ERROR_FILE_NOT_FOUND); if (h) CloseHandle(h);
    SET_ERR(0); h = U(OpenMutexA)(SYNCHRONIZE, FALSE, n);
    EXPECT_EQ(h, NULL); EXPECT_ERR(ERROR_FILE_NOT_FOUND); if (h) CloseHandle(h);
    SET_ERR(0); h = U(OpenSemaphoreA)(SYNCHRONIZE, FALSE, n);
    EXPECT_EQ(h, NULL); EXPECT_ERR(ERROR_FILE_NOT_FOUND); if (h) CloseHandle(h);
}
TEST(ho_x_open_wrong_type_by_name) {
    REQUIRE(U(OpenEventA)); REQUIRE(U(OpenMutexA));
    char n[96]; ho_uniq(n, sizeof n, "wrongtype");
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, n);
    CHECK(e != NULL); if (!e) return;
    SET_ERR(0); HANDLE m = U(OpenMutexA)(SYNCHRONIZE, FALSE, n);
    EXPECT_EQ(m, NULL); EXPECT_ERR(ERROR_INVALID_HANDLE);
    if (m) CloseHandle(m);
    CloseHandle(e);
}
TEST(ho_x_dup_event_state_shared) {
    REQUIRE(U(DuplicateHandle)); REQUIRE(U(SetEvent)); REQUIRE(U(ResetEvent));
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL), d = NULL;
    CHECK(U(DuplicateHandle)(GetCurrentProcess(), e, GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS));
    CHECK(d != NULL && d != e);
    if (d) {
        EXPECT_EQ(WaitForSingleObject(e, 0), WAIT_TIMEOUT);
        EXPECT_EQ(U(SetEvent)(d), TRUE);
        EXPECT_EQ(WaitForSingleObject(e, 0), WAIT_OBJECT_0);
        EXPECT_EQ(U(ResetEvent)(e), TRUE);
        EXPECT_EQ(WaitForSingleObject(d, 0), WAIT_TIMEOUT);
        CloseHandle(d);
        EXPECT_EQ(WaitForSingleObject(e, 0), WAIT_TIMEOUT);   /* object survives closing the duplicate */
    }
    CloseHandle(e);
}
TEST(ho_x_wait_closed_handle_values) {
    REQUIRE(U(WaitForSingleObject));
    HANDLE e = CreateEventA(NULL, TRUE, TRUE, NULL);
    CloseHandle(e);
    SET_ERR(0); EXPECT_EQ(U(WaitForSingleObject)(e, 0), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(U(WaitForSingleObject)(NULL, 0), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(ho_x_wait_multiple_invalid_count) {
    REQUIRE(U(WaitForMultipleObjects));
    HANDLE e = CreateEventA(NULL, TRUE, TRUE, NULL);
    SET_ERR(0); EXPECT_EQ(U(WaitForMultipleObjects)(0, &e, FALSE, 0), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(WaitForMultipleObjects)(MAXIMUM_WAIT_OBJECTS + 1, &e, FALSE, 0), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    EXPECT_EQ(U(WaitForMultipleObjects)(1, &e, FALSE, 0), WAIT_OBJECT_0);
    CloseHandle(e);
}
TEST(ho_x_wait_multiple_duplicate_handles_waitall) {
    REQUIRE(U(WaitForMultipleObjects));
    /* the same handle twice in a wait-all array is rejected */
    HANDLE e = CreateEventA(NULL, TRUE, TRUE, NULL), a[2];
    a[0] = a[1] = e;
    SET_ERR(0);
    DWORD r = U(WaitForMultipleObjects)(2, a, TRUE, 0);
    t_note("WaitForMultipleObjects(all, same handle twice) = 0x%lx err=%lu", r, GetLastError());
    EXPECT_EQ(U(WaitForMultipleObjects)(2, a, FALSE, 0), WAIT_OBJECT_0);
    CloseHandle(e);
}
TEST(ho_x_manual_vs_auto_event_wait) {
    REQUIRE(U(WaitForSingleObject));
    HANDLE a = CreateEventA(NULL, FALSE, TRUE, NULL), m = CreateEventA(NULL, TRUE, TRUE, NULL);
    EXPECT_EQ(U(WaitForSingleObject)(a, 0), WAIT_OBJECT_0);
    EXPECT_EQ(U(WaitForSingleObject)(a, 0), WAIT_TIMEOUT);
    EXPECT_EQ(U(WaitForSingleObject)(m, 0), WAIT_OBJECT_0);
    EXPECT_EQ(U(WaitForSingleObject)(m, 0), WAIT_OBJECT_0);
    CloseHandle(a); CloseHandle(m);
}
static DWORD WINAPI x_quick(LPVOID p) { return 321; }
TEST(ho_x_thread_handle_after_exit) {
    REQUIRE(U(GetExitCodeThread));
    HANDLE t = CreateThread(NULL, 0, x_quick, NULL, 0, NULL);
    CHECK(t != NULL); if (!t) return;
    EXPECT_EQ(WaitForSingleObject(t, 5000), WAIT_OBJECT_0);
    DWORD c = 0; EXPECT_EQ(U(GetExitCodeThread)(t, &c), TRUE); EXPECT_EQ(c, 321);
    EXPECT_EQ(WaitForSingleObject(t, 0), WAIT_OBJECT_0);      /* stays signaled */
    EXPECT_EQ(CloseHandle(t), TRUE);
}
TEST(ho_x_handle_value_multiple_of_four) {
    HANDLE hs[32]; int bad = 0;
    for (int i = 0; i < 32; i++) hs[i] = CreateEventA(NULL, TRUE, FALSE, NULL);
    for (int i = 0; i < 32; i++) { if (!hs[i] || ((ULONG_PTR)hs[i] & 3)) bad++; }
    EXPECT_EQ(bad, 0);
    for (int i = 0; i < 32; i++) if (hs[i]) CloseHandle(hs[i]);
}
TEST(ho_x_getcurrentprocess_thread_pseudo_values) {
    REQUIRE(U(GetCurrentProcess)); REQUIRE(U(GetCurrentThread));
    EXPECT_EQ(U(GetCurrentProcess)(), (HANDLE)(LONG_PTR)-1);
    EXPECT_EQ(U(GetCurrentThread)(), (HANDLE)(LONG_PTR)-2);
}
TEST(ho_x_pseudo_process_waitable_zero) {
    REQUIRE(U(WaitForSingleObject));
    /* the current process is not signaled while it runs */
    EXPECT_EQ(U(WaitForSingleObject)(GetCurrentProcess(), 0), WAIT_TIMEOUT);
}
