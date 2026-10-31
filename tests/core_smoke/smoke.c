#include "t.h"
TEST(smoke_getcurrentprocessid) {
    REQUIRE(U(GetCurrentProcessId));
    CHECK(U(GetCurrentProcessId)() == GetCurrentProcessId());
}
CHILD(echo) { return argc > 1 ? atoi(argv[1]) : 7; }
TEST(smoke_child_roundtrip) { EXPECT_EQ(t_run_child("echo", "42", 20000), 42); }
TEST(smoke_tmpdir) { char p[MAX_PATH]; t_tmpfile(p, sizeof p, "a.txt"); HANDLE h = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); CHECK(h != INVALID_HANDLE_VALUE); CloseHandle(h); }
