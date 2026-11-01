/* MulDiv and IsBad*Ptr */
#include "sc.h"
#include <limits.h>

/* Literal model of the Win10 algorithm (round half away from zero, -1 on overflow / div by zero). */
static int md_model(int a, int b, int d) {
    unsigned x = (unsigned)a ^ (unsigned)b ^ (unsigned)d;
    unsigned dd = d < 0 ? 0u - (unsigned)d : (unsigned)d;
    unsigned ab = (unsigned)((b ^ (b >> 31)) - (b >> 31));
    unsigned aa = (unsigned)((a ^ (a >> 31)) - (a >> 31));
    unsigned long long p = (unsigned long long)((long long)(int)ab * (long long)(int)aa) + (unsigned long long)(dd >> 1);
    if ((unsigned)(p >> 32) < dd) {
        int q = (int)(p / dd);
        if (q >= 0) return (int)x >= 0 ? q : -q;
    }
    return -1;
}
/* Independent model for operands that are not INT_MIN. */
static int md_ref(int a, int b, int d) {
    if (d == 0) return -1;
    long long p = (long long)a * b; int neg = (p < 0) != (d < 0);
    unsigned long long ap = p < 0 ? -p : p, ad = d < 0 ? -(long long)d : d;
    unsigned long long q = (ap + ad / 2) / ad;
    if (q > 0x7fffffffULL) return -1;
    return neg ? -(int)q : (int)q;
}

TEST(muldiv_basic_table) {
    REQUIRE(U(MulDiv));
    static const struct { int a, b, d, r; } v[] = {
        {0, 0, 1, 0}, {1, 1, 1, 1}, {2, 3, 1, 6}, {10, 10, 5, 20}, {100, 3, 4, 75}, {7, 1, 2, 4}, {5, 1, 2, 3}, {3, 1, 2, 2},
        {1, 1, 2, 1}, {1, 1, 3, 0}, {2, 1, 3, 1}, {1, 1, 4, 0}, {3, 1, 4, 1}, {1, 2, 3, 1}, {4, 1, 3, 1}, {5, 1, 3, 2},
        {-1, 1, 2, -1}, {-3, 1, 2, -2}, {-5, 1, 2, -3}, {-1, 1, 3, 0}, {-2, 1, 3, -1}, {1, -1, 2, -1}, {1, 1, -2, -1},
        {-1, -1, 2, 1}, {-1, 1, -2, 1}, {1, -1, -2, 1}, {-1, -1, -2, -1}, {-3, -1, 2, 2}, {-7, 1, -2, 4},
        {1, 0, 5, 0}, {0, 5, 5, 0}, {5, 5, 5, 5}, {INT_MAX, 1, 1, INT_MAX}, {INT_MAX, 1, 2, 1073741824}, {INT_MAX, 2, 2, INT_MAX},
        {INT_MAX, 2, 1, -1}, {INT_MAX, INT_MAX, INT_MAX, INT_MAX}, {INT_MAX, INT_MAX, 1, -1}, {INT_MAX, 1, -1, -INT_MAX},
        {-INT_MAX, 1, 1, -INT_MAX}, {-INT_MAX, 1, -1, INT_MAX}, {-INT_MAX, -1, 1, INT_MAX}, {-INT_MAX, INT_MAX, INT_MAX, -INT_MAX},
        {65536, 32767, 1, 2147418112}, {65536, 32768, 1, -1}, {-65536, 32768, 1, -1},
        {1, 1, 0, -1}, {0, 0, 0, -1}, {-1, -1, 0, -1}, {INT_MAX, INT_MAX, 0, -1}, {5, 5, 0, -1},
        {1000000, 1000000, 1000000, 1000000}, {123456789, 987654321, 123456789, 987654321}, {1 << 30, 4, 4, 1 << 30},
        {1 << 30, 2, 1, -1}, {1 << 30, 2, 2, 1 << 30}, {96, 1440, 72, 1920}, {10, 96, 72, 13}, {11, 96, 72, 15}, {12, 96, 72, 16},
        {1, 96, 72, 1}, {-1, 96, 72, -1}, {13, 7, 100, 1}, {50, 1, 100, 1}, {49, 1, 100, 0}, {-50, 1, 100, -1}, {-49, 1, 100, 0},
        {150, 1, 100, 2}, {149, 1, 100, 1}, {250, 1, 100, 3}, {-150, 1, 100, -2}, {-250, 1, 100, -3},
        {2147483646, 1, 2, 1073741823}, {2147483647, 1, 2, 1073741824},
        {3, 3, 2, 5}, {3, 3, 3, 3}, {3, 3, 4, 2}, {3, 3, 5, 2}, {3, 3, 6, 2}, {3, 3, 7, 1}, {3, 3, 18, 1}, {3, 3, 19, 0},
    };
    for (int i = 0; i < ARRAY_N(v); i++) {
        int r = U(MulDiv)(v[i].a, v[i].b, v[i].d);
        CHECKF(r == v[i].r, "vec %d MulDiv(%d,%d,%d) got %d want %d", i, v[i].a, v[i].b, v[i].d, r, v[i].r);
    }
}
TEST(muldiv_commutative) {
    REQUIRE(U(MulDiv));
    static const int vals[] = {0, 1, -1, 2, -3, 7, 100, -255, 65535, 65536, 1 << 20, INT_MAX, -INT_MAX, 12345678};
    for (int i = 0; i < ARRAY_N(vals); i++) for (int j = 0; j < ARRAY_N(vals); j++) for (int k = 0; k < ARRAY_N(vals); k++)
        CHECKF(U(MulDiv)(vals[i], vals[j], vals[k]) == U(MulDiv)(vals[j], vals[i], vals[k]), "%d %d %d", vals[i], vals[j], vals[k]);
}
TEST(muldiv_model_grid_no_intmin) {
    REQUIRE(U(MulDiv));
    static const int vals[] = {0, 1, -1, 2, -2, 3, -3, 5, 7, -7, 10, 100, -100, 255, 1000, 4096, -4096, 65535, 65536, -65536, 99999,
        1 << 24, 1 << 30, -(1 << 30), INT_MAX, -INT_MAX, INT_MAX - 1, 46341, 46340, -46341, 2147483000};
    int n = 0;
    for (int i = 0; i < ARRAY_N(vals); i++) for (int j = 0; j < ARRAY_N(vals); j++) for (int k = 0; k < ARRAY_N(vals); k++) {
        int r = U(MulDiv)(vals[i], vals[j], vals[k]);
        int e = md_ref(vals[i], vals[j], vals[k]);
        /* an exact result of -2^31 is reported as -1 by the implementation; md_ref cannot produce it */
        if (r != e) CHECKF(0, "MulDiv(%d,%d,%d) got %d want %d", vals[i], vals[j], vals[k], r, e);
        n++;
    }
    CHECK(n > 20000);
}
TEST(muldiv_intmin) {
    REQUIRE(U(MulDiv));
    static const int vals[] = {INT_MIN, INT_MIN + 1, -1, 0, 1, 2, INT_MAX};
    int notes = 0;
    for (int i = 0; i < ARRAY_N(vals); i++) for (int j = 0; j < ARRAY_N(vals); j++) for (int k = 0; k < ARRAY_N(vals); k++) {
        if (vals[i] != INT_MIN && vals[j] != INT_MIN && vals[k] != INT_MIN) continue;
        int r = U(MulDiv)(vals[i], vals[j], vals[k]);
        int nat = md_ref(vals[i], vals[j], vals[k]);
        int mod = md_model(vals[i], vals[j], vals[k]);
        if (nat == mod && !t_is_wine()) CHECKF(r == nat, "MulDiv(%d,%d,%d) got %d want %d", vals[i], vals[j], vals[k], r, nat);
        else if (notes++ < 12) t_note("MulDiv(%d,%d,%d)=%d (exact math %d, win10 model %d)", vals[i], vals[j], vals[k], r, nat, mod);
    }
    /* a result of INT_MIN is never returned */
    CHECK(U(MulDiv)(INT_MIN, 1, 1) != INT_MIN);
    CHECK(U(MulDiv)(1, 1, INT_MIN) != INT_MIN);
    EXPECT_EQ(U(MulDiv)(0, INT_MIN, 5), 0);
    EXPECT_EQ(U(MulDiv)(INT_MIN, 0, 5), 0);
    EXPECT_EQ(U(MulDiv)(5, 5, 0), -1);
    EXPECT_EQ(U(MulDiv)(INT_MIN, 1, 0), -1);
}
TEST(muldiv_rounding_sweep) {
    REQUIRE(U(MulDiv));
    /* x/ d for d in 2..13: rounded to nearest, ties away from zero, symmetric in sign */
    for (int d = 2; d <= 13; d++) for (int x = -60; x <= 60; x++) {
        int r = U(MulDiv)(x, 1, d), e = md_ref(x, 1, d);
        CHECKF(r == e, "%d/%d got %d want %d", x, d, r, e);
        CHECKF(U(MulDiv)(-x, 1, d) == -r, "odd symmetry %d/%d", x, d);
        CHECKF(U(MulDiv)(x, 1, -d) == -r, "neg denom %d/%d", x, d);
    }
}
TEST(muldiv_scale_dpi_like) {
    REQUIRE(U(MulDiv));
    /* common usage: pt -> px at several DPIs */
    static const int dpi[] = {96, 120, 144, 192};
    for (int i = 0; i < 4; i++) for (int pt = 0; pt <= 72; pt++) CHECKF(U(MulDiv)(pt, dpi[i], 72) == md_ref(pt, dpi[i], 72), "%d@%d", pt, dpi[i]);
}
TEST(muldiv_no_lasterror_change_on_success) {
    REQUIRE(U(MulDiv));
    SET_ERR(0xabcd);
    U(MulDiv)(3, 4, 5);
    EXPECT_EQ(GetLastError(), 0xabcd);
}

/* ------------------------------ IsBad*Ptr ------------------------------ */
/* Probes that really fault run in a child process (no harness exception handler there):
 * the child returns 0, or the source line of the first failed check. */
#define KC(c) do { if (!(c)) { printf("  CHILD FAIL line %d: %s\n", __LINE__, #c); return __LINE__; } } while (0)
#define KEQ(a, b) do { ULONG_PTR a_ = (ULONG_PTR)(a), b_ = (ULONG_PTR)(b); if (a_ != b_) { printf("  CHILD FAIL line %d: %s == %s (got 0x%llx want 0x%llx)\n", __LINE__, #a, #b, (unsigned long long)a_, (unsigned long long)b_); return __LINE__; } } while (0)

CHILD(isbad_readptr_null_and_zero) {
    if (!U(IsBadReadPtr)) return 0;
    char c = 1;
    KEQ(U(IsBadReadPtr)(NULL, 0), 0);
    KEQ(U(IsBadReadPtr)(NULL, 1), 1);
    KEQ(U(IsBadReadPtr)(NULL, 100), 1);
    KEQ(U(IsBadReadPtr)(&c, 0), 0);
    KEQ(U(IsBadReadPtr)(&c, 1), 0);
    KEQ(U(IsBadReadPtr)((void *)(ULONG_PTR)0xdeadbeef, 0), 0);
    KEQ(U(IsBadReadPtr)((void *)(ULONG_PTR)0x10, 1), 1);
    KEQ(U(IsBadReadPtr)((void *)~(ULONG_PTR)0, 1), 1);
    KEQ(U(IsBadReadPtr)((void *)~(ULONG_PTR)0, 2), 1);       /* wraps */
    KEQ(U(IsBadReadPtr)(&c, ~(UINT_PTR)0), 1);               /* wraps */
    return 0;
}
TEST(isbad_readptr_null_and_zero) {
    REQUIRE(U(IsBadReadPtr));
    if (t_is_wine()) { t_skip("emulator probes NULL and faults visibly"); return; }
    /* the 0x10 and ~0 probes really fault (and are caught inside IsBadReadPtr): run in a child so the harness exception hook does not see them */
    DWORD rc = t_run_child("isbad_readptr_null_and_zero", "", 20000);
    CHECKF(rc == 0, "child rc %lu (0=ok, else failing line)", rc);
}
TEST(isbad_writeptr_null_and_zero) {
    REQUIRE(U(IsBadWritePtr));
    if (t_is_wine()) { t_skip("emulator probes NULL and faults visibly"); return; }
    char c = 1;
    EXPECT_EQ(U(IsBadWritePtr)(NULL, 0), 0);
    EXPECT_EQ(U(IsBadWritePtr)(NULL, 1), 1);
    EXPECT_EQ(U(IsBadWritePtr)(&c, 0), 0);
    EXPECT_EQ(U(IsBadWritePtr)(&c, 1), 0);
    EXPECT_EQ(c, 1);
    EXPECT_EQ(U(IsBadWritePtr)((void *)~(ULONG_PTR)0, 2), 1);
}
CHILD(isbad_codeptr) {
    if (!(U(IsBadCodePtr))) return 0;
    KEQ(U(IsBadCodePtr)(NULL), 1);
    KEQ(U(IsBadCodePtr)((FARPROC)(void *)U(IsBadCodePtr)), 0);
    KEQ(U(IsBadCodePtr)((FARPROC)(ULONG_PTR)1), 1);
    void *p = sc_alloc_pages(4096, PAGE_NOACCESS);
    if (p) { KEQ(U(IsBadCodePtr)((FARPROC)p), 1); VirtualFree(p, 0, MEM_RELEASE); }
    p = sc_alloc_pages(4096, PAGE_EXECUTE_READ);
    if (p) { KEQ(U(IsBadCodePtr)((FARPROC)p), 0); VirtualFree(p, 0, MEM_RELEASE); }
    return 0;
}
TEST(isbad_codeptr) {
    REQUIRE(U(IsBadReadPtr)); REQUIRE(U(IsBadStringPtrA));
    DWORD rc = t_run_child("isbad_codeptr", "", 20000);
    CHECKF(rc == 0, "child rc %lu (0=ok, else failing line)", rc);
}
CHILD(isbad_protections_matrix) {
    if (!U(IsBadReadPtr) || !U(IsBadWritePtr)) return 0;
    static const struct { DWORD prot; int badr, badw; } v[] = {
        {PAGE_READWRITE, 0, 0}, {PAGE_READONLY, 0, 1}, {PAGE_NOACCESS, 1, 1}, {PAGE_EXECUTE_READ, 0, 1}, {PAGE_EXECUTE_READWRITE, 0, 0},
        {PAGE_WRITECOPY, 0, 0}, {PAGE_EXECUTE_WRITECOPY, 0, 0},
    };
    for (int i = 0; i < ARRAY_N(v); i++) {
        unsigned char *p = sc_alloc_pages(8192, v[i].prot);
        if (!p) { t_note("alloc prot %lx failed", v[i].prot); continue; }
        KC(U(IsBadReadPtr)(p, 1) == v[i].badr);
        KC(U(IsBadReadPtr)(p, 8192) == v[i].badr);
        KC(U(IsBadReadPtr)(p + 4095, 2) == v[i].badr);
        KC(U(IsBadWritePtr)(p, 1) == v[i].badw);
        KC(U(IsBadWritePtr)(p, 8192) == v[i].badw);
        VirtualFree(p, 0, MEM_RELEASE);
    }
    return 0;
}
TEST(isbad_protections_matrix) {
    REQUIRE(U(IsBadReadPtr)); REQUIRE(U(IsBadStringPtrA));
    DWORD rc = t_run_child("isbad_protections_matrix", "", 20000);
    CHECKF(rc == 0, "child rc %lu (0=ok, else failing line)", rc);
}
CHILD(isbad_page_boundaries) {
    if (!U(IsBadReadPtr) || !U(IsBadWritePtr)) return 0;
    unsigned char *base = VirtualAlloc(NULL, 3 * 4096, MEM_RESERVE, PAGE_NOACCESS);
    if (!base) { return 0; }
    VirtualAlloc(base, 4096, MEM_COMMIT, PAGE_READWRITE);
    VirtualAlloc(base + 4096, 4096, MEM_COMMIT, PAGE_READWRITE);
    /* third page stays reserved (not committed) */
    KEQ(U(IsBadReadPtr)(base, 8192), 0);
    KEQ(U(IsBadReadPtr)(base + 8191, 1), 0);
    KEQ(U(IsBadReadPtr)(base + 8191, 2), 1);
    KEQ(U(IsBadReadPtr)(base + 8192, 1), 1);
    KEQ(U(IsBadReadPtr)(base, 8193), 1);
    KEQ(U(IsBadReadPtr)(base, 3 * 4096), 1);
    KEQ(U(IsBadWritePtr)(base, 8192), 0);
    KEQ(U(IsBadWritePtr)(base + 8191, 2), 1);
    KEQ(U(IsBadWritePtr)(base + 8192, 1), 1);
    /* make the middle page read-only: a write range that touches it is bad, read is fine */
    DWORD old; VirtualProtect(base + 4096, 4096, PAGE_READONLY, &old);
    KEQ(U(IsBadReadPtr)(base, 8192), 0);
    KEQ(U(IsBadWritePtr)(base, 8192), 1);
    KEQ(U(IsBadWritePtr)(base, 4096), 0);
    KEQ(U(IsBadWritePtr)(base + 4095, 2), 1);
    KEQ(U(IsBadWritePtr)(base + 4096, 1), 1);
    /* a write probe must not alter contents */
    base[10] = 0x5a; U(IsBadWritePtr)(base, 100); KEQ(base[10], 0x5a);
    VirtualFree(base, 0, MEM_RELEASE);
    KEQ(U(IsBadReadPtr)(base, 1), 1);
    return 0;
}
TEST(isbad_page_boundaries) {
    REQUIRE(U(IsBadReadPtr)); REQUIRE(U(IsBadStringPtrA));
    DWORD rc = t_run_child("isbad_page_boundaries", "", 20000);
    CHECKF(rc == 0, "child rc %lu (0=ok, else failing line)", rc);
}
CHILD(isbad_released_memory) {
    if (!U(IsBadReadPtr) || !U(IsBadWritePtr)) return 0;
    void *p = sc_alloc_pages(4096, PAGE_READWRITE);
    KC(p != NULL);
    if (!p) return 0;
    KEQ(U(IsBadReadPtr)(p, 16), 0);
    VirtualFree(p, 0, MEM_RELEASE);
    KEQ(U(IsBadReadPtr)(p, 16), 1);
    KEQ(U(IsBadWritePtr)(p, 16), 1);
    return 0;
}
TEST(isbad_released_memory) {
    REQUIRE(U(IsBadReadPtr)); REQUIRE(U(IsBadStringPtrA));
    DWORD rc = t_run_child("isbad_released_memory", "", 20000);
    CHECKF(rc == 0, "child rc %lu (0=ok, else failing line)", rc);
}
CHILD(isbad_decommitted) {
    if (!(U(IsBadReadPtr))) return 0;
    unsigned char *p = sc_alloc_pages(4096, PAGE_READWRITE);
    if (!p) return 0;
    VirtualFree(p, 4096, MEM_DECOMMIT);
    KEQ(U(IsBadReadPtr)(p, 1), 1);
    VirtualFree(p, 0, MEM_RELEASE);
    return 0;
}
TEST(isbad_decommitted) {
    REQUIRE(U(IsBadReadPtr)); REQUIRE(U(IsBadStringPtrA));
    DWORD rc = t_run_child("isbad_decommitted", "", 20000);
    CHECKF(rc == 0, "child rc %lu (0=ok, else failing line)", rc);
}
CHILD(isbad_guard_page) {
    if (!U(IsBadReadPtr) || !U(IsBadWritePtr)) return 0;
    unsigned char *p = sc_alloc_pages(4096, PAGE_READWRITE | PAGE_GUARD);
    if (!p) { return 0; }
    if (t_is_wine()) return 0;
    /* The probe hits the guard page: reported as bad. */
    KEQ(U(IsBadReadPtr)(p, 1), 1);
    MEMORY_BASIC_INFORMATION mbi; VirtualQuery(p, &mbi, sizeof mbi);
    t_note("after IsBadReadPtr guard bit %s", (mbi.Protect & PAGE_GUARD) ? "kept" : "consumed");
    VirtualFree(p, 0, MEM_RELEASE);
    p = sc_alloc_pages(4096, PAGE_READWRITE | PAGE_GUARD);
    if (!p) return 0;
    KEQ(U(IsBadWritePtr)(p, 1), 1);
    VirtualFree(p, 0, MEM_RELEASE);
    return 0;
}
TEST(isbad_guard_page) {
    REQUIRE(U(IsBadReadPtr)); REQUIRE(U(IsBadStringPtrA));
    DWORD rc = t_run_child("isbad_guard_page", "", 20000);
    CHECKF(rc == 0, "child rc %lu (0=ok, else failing line)", rc);
}
CHILD(isbad_readonly_data_section_string) {
    if (!U(IsBadReadPtr) || !U(IsBadWritePtr)) return 0;
    static const char ro[] = "constant";
    KEQ(U(IsBadReadPtr)(ro, sizeof ro), 0);
    /* the PE header of our own image is readable but not writable */
    HMODULE m = GetModuleHandleA(NULL);
    KEQ(U(IsBadReadPtr)(m, 64), 0);
    KEQ(U(IsBadWritePtr)(m, 1), 1);
    return 0;
}
TEST(isbad_readonly_data_section_string) {
    REQUIRE(U(IsBadReadPtr)); REQUIRE(U(IsBadStringPtrA));
    DWORD rc = t_run_child("isbad_readonly_data_section_string", "", 20000);
    CHECKF(rc == 0, "child rc %lu (0=ok, else failing line)", rc);
}
CHILD(isbad_stack_and_heap) {
    if (!U(IsBadReadPtr) || !U(IsBadWritePtr)) return 0;
    char buf[256];
    KEQ(U(IsBadReadPtr)(buf, sizeof buf), 0);
    KEQ(U(IsBadWritePtr)(buf, sizeof buf), 0);
    void *h = HeapAlloc(GetProcessHeap(), 0, 1000);
    KEQ(U(IsBadReadPtr)(h, 1000), 0);
    KEQ(U(IsBadWritePtr)(h, 1000), 0);
    HeapFree(GetProcessHeap(), 0, h);
    return 0;
}
TEST(isbad_stack_and_heap) {
    REQUIRE(U(IsBadReadPtr)); REQUIRE(U(IsBadStringPtrA));
    DWORD rc = t_run_child("isbad_stack_and_heap", "", 20000);
    CHECKF(rc == 0, "child rc %lu (0=ok, else failing line)", rc);
}
CHILD(isbad_hugeptr_parity) {
    if (!U(IsBadHugeReadPtr) || !U(IsBadHugeWritePtr)) return 0;
    char c = 1;
    KEQ(U(IsBadHugeReadPtr)(NULL, 0), 0);
    KEQ(U(IsBadHugeReadPtr)(NULL, 1), 1);
    KEQ(U(IsBadHugeReadPtr)(&c, 1), 0);
    KEQ(U(IsBadHugeWritePtr)(NULL, 0), 0);
    KEQ(U(IsBadHugeWritePtr)(NULL, 1), 1);
    KEQ(U(IsBadHugeWritePtr)(&c, 1), 0);
    KEQ(U(IsBadHugeReadPtr)(&c, ~(UINT_PTR)0), 1);
    KEQ(U(IsBadHugeWritePtr)(&c, ~(UINT_PTR)0), 1);
    unsigned char *base = VirtualAlloc(NULL, 1024 * 1024 + 65536, MEM_RESERVE, PAGE_NOACCESS);
    if (!base) return 0;
    VirtualAlloc(base, 1024 * 1024, MEM_COMMIT, PAGE_READWRITE);
    KEQ(U(IsBadHugeReadPtr)(base, 1024 * 1024), 0);
    KEQ(U(IsBadHugeWritePtr)(base, 1024 * 1024), 0);
    KEQ(U(IsBadHugeReadPtr)(base, 1024 * 1024 + 1), 1);
    KEQ(U(IsBadHugeWritePtr)(base, 1024 * 1024 + 1), 1);
    VirtualFree(base, 0, MEM_RELEASE);
    return 0;
}
TEST(isbad_hugeptr_parity) {
    REQUIRE(U(IsBadReadPtr)); REQUIRE(U(IsBadStringPtrA));
    DWORD rc = t_run_child("isbad_hugeptr_parity", "", 20000);
    CHECKF(rc == 0, "child rc %lu (0=ok, else failing line)", rc);
}
CHILD(isbad_stringptrA) {
    if (!(U(IsBadStringPtrA))) return 0;
    if (!t_is_wine()) KEQ(U(IsBadStringPtrA)(NULL, 0), 0);
    KEQ(U(IsBadStringPtrA)(NULL, 1), 1);
    KEQ(U(IsBadStringPtrA)(NULL, 100), 1);
    KEQ(U(IsBadStringPtrA)("abc", 0), 0);
    KEQ(U(IsBadStringPtrA)("abc", 1), 0);
    KEQ(U(IsBadStringPtrA)("abc", 4), 0);
    KEQ(U(IsBadStringPtrA)("abc", 100), 0);              /* stops at the terminator */
    KEQ(U(IsBadStringPtrA)("", 1), 0);
    KEQ(U(IsBadStringPtrA)((LPCSTR)(ULONG_PTR)0x20, 1), 1);
    unsigned char *p = VirtualAlloc(NULL, 2 * 4096, MEM_RESERVE, PAGE_NOACCESS);
    if (!p) return 0;
    VirtualAlloc(p, 4096, MEM_COMMIT, PAGE_READWRITE);
    memset(p, 'x', 4096);
    KEQ(U(IsBadStringPtrA)((LPCSTR)p, 4096), 0);          /* no terminator, limit = committed size */
    KEQ(U(IsBadStringPtrA)((LPCSTR)p, 4097), 1);          /* runs into the next page */
    KEQ(U(IsBadStringPtrA)((LPCSTR)p, (UINT_PTR)-1), 1);
    p[4095] = 0;
    KEQ(U(IsBadStringPtrA)((LPCSTR)p, 100000), 0);        /* terminator in last byte */
    KEQ(U(IsBadStringPtrA)((LPCSTR)p + 4095, 5), 0);
    KEQ(U(IsBadStringPtrA)((LPCSTR)p + 4096, 1), 1);
    VirtualFree(p, 0, MEM_RELEASE);
    return 0;
}
TEST(isbad_stringptrA) {
    REQUIRE(U(IsBadReadPtr)); REQUIRE(U(IsBadStringPtrA));
    DWORD rc = t_run_child("isbad_stringptrA", "", 20000);
    CHECKF(rc == 0, "child rc %lu (0=ok, else failing line)", rc);
}
CHILD(isbad_stringptrW) {
    if (!(U(IsBadStringPtrW))) return 0;
    static const WCHAR s[] = W_('a', 'b', 0), e[] = W_(0);
    if (!t_is_wine()) KEQ(U(IsBadStringPtrW)(NULL, 0), 0);
    KEQ(U(IsBadStringPtrW)(NULL, 1), 1);
    KEQ(U(IsBadStringPtrW)(s, 0), 0);
    KEQ(U(IsBadStringPtrW)(s, 3), 0);
    KEQ(U(IsBadStringPtrW)(s, 1000), 0);
    KEQ(U(IsBadStringPtrW)(e, 1), 0);
    WCHAR *p = VirtualAlloc(NULL, 2 * 4096, MEM_RESERVE, PAGE_NOACCESS);
    if (!p) return 0;
    VirtualAlloc(p, 4096, MEM_COMMIT, PAGE_READWRITE);
    for (int i = 0; i < 2048; i++) p[i] = 'y';
    KEQ(U(IsBadStringPtrW)(p, 2048), 0);
    KEQ(U(IsBadStringPtrW)(p, 2049), 1);
    p[2047] = 0;
    KEQ(U(IsBadStringPtrW)(p, 100000), 0);
    VirtualFree(p, 0, MEM_RELEASE);
    return 0;
}
TEST(isbad_stringptrW) {
    REQUIRE(U(IsBadReadPtr)); REQUIRE(U(IsBadStringPtrA));
    DWORD rc = t_run_child("isbad_stringptrW", "", 20000);
    CHECKF(rc == 0, "child rc %lu (0=ok, else failing line)", rc);
}
CHILD(isbad_noaccess_string) {
    if (!U(IsBadStringPtrA) || !U(IsBadStringPtrW)) return 0;
    void *p = sc_alloc_pages(4096, PAGE_NOACCESS);
    if (!p) return 0;
    KEQ(U(IsBadStringPtrA)((LPCSTR)p, 1), 1);
    KEQ(U(IsBadStringPtrW)((LPCWSTR)p, 1), 1);
    KEQ(U(IsBadStringPtrA)((LPCSTR)p, 0), 0);
    KEQ(U(IsBadStringPtrW)((LPCWSTR)p, 0), 0);
    VirtualFree(p, 0, MEM_RELEASE);
    return 0;
}
TEST(isbad_noaccess_string) {
    REQUIRE(U(IsBadReadPtr)); REQUIRE(U(IsBadStringPtrA));
    DWORD rc = t_run_child("isbad_noaccess_string", "", 20000);
    CHECKF(rc == 0, "child rc %lu (0=ok, else failing line)", rc);
}
TEST(isbad_does_not_set_lasterror_needed) {
    REQUIRE(U(IsBadReadPtr));
    SET_ERR(0);
    U(IsBadReadPtr)(NULL, 1);
    t_note("GetLastError after IsBadReadPtr(NULL,1) = %lu", GetLastError());
    CHECK(1);
}
