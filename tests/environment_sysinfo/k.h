/* Local helpers for the environment_sysinfo area. Included after t.h. */
#ifndef K_H
#define K_H
/* The stock EXPECT_* macros evaluate their arguments twice (once for the test, once for the
 * message). Calls with side effects (in/out size parameters) need single evaluation. */
#undef EXPECT_EQ
#undef EXPECT_NE
#define EXPECT_EQ(a, b) do { unsigned long long a_ = (unsigned long long)(ULONG_PTR)(a), b_ = (unsigned long long)(ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", a_, b_); } while (0)
#define EXPECT_NE(a, b) do { unsigned long long a_ = (unsigned long long)(ULONG_PTR)(a), b_ = (unsigned long long)(ULONG_PTR)(b); \
    t_check(a_ != b_, __FILE__, __LINE__, #a " != " #b, "got 0x%llx", a_); } while (0)
#endif
