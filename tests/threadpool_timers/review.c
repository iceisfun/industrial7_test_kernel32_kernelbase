/* Additional environment-version and persistent-callback checks. */
#include "tp.h"

static VOID NTAPI rv_work(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_WORK w) { InterlockedIncrement((volatile LONG *)p); }

TEST(tp_rv_env_version2_reads_priority) {
    REQUIRE(U(CreateThreadpoolWork)); REQUIRE(U(CloseThreadpoolWork));
    TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e);
    e.Version = 2;
    e.CallbackPriority = TP_CALLBACK_PRIORITY_INVALID;
    SET_ERR(0);
    PTP_WORK w = U(CreateThreadpoolWork)(rv_work, NULL, &e);
    if (t_is_wine() && w) t_note("wine accepts invalid priority in a V2 environment");
    else { CHECK(w == NULL); EXPECT_ERR(ERROR_INVALID_PARAMETER); }
    if (w) U(CloseThreadpoolWork)(w);
}
TEST(tp_rv_env_version0_ignores_priority) {
    REQUIRE(U(CreateThreadpoolWork)); REQUIRE(U(SubmitThreadpoolWork));
    REQUIRE(U(WaitForThreadpoolWorkCallbacks)); REQUIRE(U(CloseThreadpoolWork));
    TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e);
    e.Version = 0;
    e.CallbackPriority = TP_CALLBACK_PRIORITY_INVALID;
    volatile LONG n = 0;
    PTP_WORK w = U(CreateThreadpoolWork)(rv_work, (PVOID)&n, &e);
    CHECK(w != NULL);
    if (w) {
        U(SubmitThreadpoolWork)(w);
        U(WaitForThreadpoolWorkCallbacks)(w, FALSE);
        U(CloseThreadpoolWork)(w);
        EXPECT_EQ(n, 1);
    }
}
TEST(tp_rv_persistent_env_default_pool_runs) {
    REQUIRE(U(CreateThreadpoolWork)); REQUIRE(U(SubmitThreadpoolWork));
    REQUIRE(U(WaitForThreadpoolWorkCallbacks)); REQUIRE(U(CloseThreadpoolWork));
    TP_CALLBACK_ENVIRON e; InitializeThreadpoolEnvironment(&e);
    e.u.Flags |= 2;                                          /* Persistent, no explicit pool */
    volatile LONG n = 0;
    PTP_WORK w = U(CreateThreadpoolWork)(rv_work, (PVOID)&n, &e);
    CHECK(w != NULL);
    if (w) {
        for (int k = 0; k < 20; k++) U(SubmitThreadpoolWork)(w);
        U(WaitForThreadpoolWorkCallbacks)(w, FALSE);
        U(CloseThreadpoolWork)(w);
        EXPECT_EQ(n, 20);
    }
}
TEST(tp_rv_submit_repeated_wait_all_counts) {
    REQUIRE(U(CreateThreadpoolWork)); REQUIRE(U(SubmitThreadpoolWork));
    REQUIRE(U(WaitForThreadpoolWorkCallbacks)); REQUIRE(U(CloseThreadpoolWork));
    volatile LONG n = 0;
    PTP_WORK w = U(CreateThreadpoolWork)(rv_work, (PVOID)&n, NULL);
    CHECK(w != NULL);
    if (!w) return;
    for (int r = 0; r < 5; r++) {
        for (int k = 0; k < 100; k++) U(SubmitThreadpoolWork)(w);
        U(WaitForThreadpoolWorkCallbacks)(w, FALSE);
        EXPECT_EQ(n, (r + 1) * 100);
    }
    U(CloseThreadpoolWork)(w);
}
