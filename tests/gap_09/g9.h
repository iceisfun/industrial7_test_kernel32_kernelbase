#ifndef G9_H
#define G9_H
#include "t.h"
/* Single-evaluation equality check (the harness EXPECT_EQ evaluates its arguments twice). */
#define G9EQ(a, b) do { unsigned long long a_ = (unsigned long long)(ULONG_PTR)(a); unsigned long long b_ = (unsigned long long)(ULONG_PTR)(b); \
    t_check(a_ == b_, __FILE__, __LINE__, #a " == " #b, "got 0x%llx want 0x%llx", a_, b_); } while (0)
#endif
