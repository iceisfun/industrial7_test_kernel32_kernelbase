/* Atoms: global vs local isolation, cross-process visibility, InitAtomTable, GlobalAddAtomEx. */
#include "ap.h"

TEST(atoms_isolation_local_not_in_global) {
    REQUIRE(U(AddAtomA)); REQUIRE(U(GlobalFindAtomA)); REQUIRE(U(DeleteAtom)); REQUIRE(U(FindAtomA));
    char n[64]; snprintf(n, sizeof n, "k32t%lu_iso_local", GetCurrentProcessId());
    ATOM a = U(AddAtomA)(n);
    CHECK(a >= 0xC000);
    SET_ERR(0);
    EXPECT_EQ(U(GlobalFindAtomA)(n), 0);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    EXPECT_EQ(U(FindAtomA)(n), a);
    U(DeleteAtom)(a);
}

TEST(atoms_isolation_global_not_in_local) {
    REQUIRE(U(GlobalAddAtomA)); REQUIRE(U(FindAtomA)); REQUIRE(U(GlobalDeleteAtom)); REQUIRE(U(GlobalFindAtomA));
    char n[64]; snprintf(n, sizeof n, "k32t%lu_iso_global", GetCurrentProcessId());
    ATOM a = U(GlobalAddAtomA)(n);
    CHECK(a >= 0xC000);
    SET_ERR(0);
    EXPECT_EQ(U(FindAtomA)(n), 0);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    EXPECT_EQ(U(GlobalFindAtomA)(n), a);
    U(GlobalDeleteAtom)(a);
}

TEST(atoms_isolation_same_name_both_tables) {
    REQUIRE(U(GlobalAddAtomA)); REQUIRE(U(AddAtomA)); REQUIRE(U(GlobalDeleteAtom)); REQUIRE(U(DeleteAtom));
    REQUIRE(U(GlobalGetAtomNameA)); REQUIRE(U(GetAtomNameA));
    char n[64], o[64]; snprintf(n, sizeof n, "k32t%lu_iso_both", GetCurrentProcessId());
    ATOM g = U(GlobalAddAtomA)(n), l = U(AddAtomA)(n);
    CHECK(g >= 0xC000 && l >= 0xC000);
    EXPECT_EQ(U(GlobalGetAtomNameA)(g, o, sizeof o), strlen(n));
    EXPECT_STR(o, n);
    EXPECT_EQ(U(GetAtomNameA)(l, o, sizeof o), strlen(n));
    /* deleting from one table does not affect the other */
    U(GlobalDeleteAtom)(g);
    CHECK(U(FindAtomA)(n) == l);
    CHECK(U(GlobalFindAtomA)(n) == 0);
    U(DeleteAtom)(l);
}

TEST(atoms_isolation_integer_atoms_same_in_both) {
    REQUIRE(U(GlobalAddAtomA)); REQUIRE(U(AddAtomA));
    EXPECT_EQ(U(GlobalAddAtomA)(MAKEINTATOM(77)), 77);
    EXPECT_EQ(U(AddAtomA)(MAKEINTATOM(77)), 77);
}

TEST(atoms_global_addex_flags_zero) {
    REQUIRE(U(GlobalAddAtomExA)); REQUIRE(U(GlobalAddAtomExW)); REQUIRE(U(GlobalFindAtomA)); REQUIRE(U(GlobalDeleteAtom));
    REQUIRE(U(GlobalAddAtomA));
    char n[64]; WCHAR w[64]; snprintf(n, sizeof n, "k32t%lu_addex0", GetCurrentProcessId()); ap_widen(w, n);
    SET_ERR(0xdeadbeef);
    ATOM a = U(GlobalAddAtomExA)(n, 0);
    CHECK(a >= 0xC000);
    EXPECT_ERR(0xdeadbeef);
    EXPECT_EQ(U(GlobalAddAtomExW)(w, 0), a);
    EXPECT_EQ(U(GlobalAddAtomA)(n), a);
    EXPECT_EQ(U(GlobalDeleteAtom)(a), 0);
    EXPECT_EQ(U(GlobalDeleteAtom)(a), 0);
    EXPECT_EQ(U(GlobalFindAtomA)(n), a);   /* three references, two deleted */
    EXPECT_EQ(U(GlobalDeleteAtom)(a), 0);
    EXPECT_EQ(U(GlobalFindAtomA)(n), 0);
}

TEST(atoms_global_addex_integer_and_invalid) {
    REQUIRE(U(GlobalAddAtomExA)); REQUIRE(U(GlobalAddAtomExW));
    EXPECT_EQ(U(GlobalAddAtomExA)(MAKEINTATOM(5), 0), 5);
    EXPECT_EQ(U(GlobalAddAtomExW)((LPCWSTR)(ULONG_PTR)0xBFFF, 0), 0xBFFF);
    EXPECT_EQ(U(GlobalAddAtomExA)(NULL, 0), 0);
    EXPECT_EQ(U(GlobalAddAtomExA)((LPCSTR)(ULONG_PTR)0xC000, 0), 0);
    char big[300]; memset(big, 'k', 299); big[299] = 0;
    SET_ERR(0);
    EXPECT_EQ(U(GlobalAddAtomExA)(big, 0), 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}

TEST(atoms_global_addex_nonzero_flags_note) {
    REQUIRE(U(GlobalAddAtomExA)); REQUIRE(U(GlobalDeleteAtom));
    DWORD flags[] = { 1, 2, 0x100, 0x80000000 };
    for (unsigned i = 0; i < 4; i++) {
        char n[64]; snprintf(n, sizeof n, "k32t%lu_addexf%u", GetCurrentProcessId(), i);
        SET_ERR(0);
        ATOM a = U(GlobalAddAtomExA)(n, flags[i]);
        t_note("GlobalAddAtomExA flags=0x%lx -> %x err=%lu", flags[i], a, GetLastError());
        if (a >= 0xC000) U(GlobalDeleteAtom)(a);
    }
}

CHILD(atom_find_global) {
    /* argv[1]: global atom name that must exist; argv[2]: expected atom value (hex) */
    ATOM want = (ATOM)strtoul(argv[2], NULL, 16);
    ATOM a = U(GlobalFindAtomA)(argv[1]);
    if (a != want) return 11;
    char o[128];
    if (U(GlobalGetAtomNameA)(a, o, sizeof o) != strlen(argv[1])) return 12;
    if (strcmp(o, argv[1])) return 13;
    /* a reference taken by the child is released with the child's own delete */
    if (U(GlobalAddAtomA)(argv[1]) != a) return 14;
    if (U(GlobalDeleteAtom)(a) != 0) return 15;
    return 0;
}

CHILD(atom_add_global_and_exit) {
    ATOM a = U(GlobalAddAtomA)(argv[1]);
    if (a < 0xC000) return 21;
    return 0;   /* deliberately leaves the atom behind */
}

CHILD(atom_local_empty) {
    /* the local table of a new process does not contain the parent's local atoms */
    if (U(FindAtomA)(argv[1]) != 0) return 31;
    ATOM a = U(AddAtomA)(argv[1]);
    if (a < 0xC000) return 32;
    if (U(DeleteAtom)(a) != 0) return 33;
    if (U(FindAtomA)(argv[1]) != 0) return 34;
    return 0;
}

TEST(atoms_global_visible_in_child_process) {
    REQUIRE(U(GlobalAddAtomA)); REQUIRE(U(GlobalDeleteAtom)); REQUIRE(U(GlobalFindAtomA)); REQUIRE(U(GlobalGetAtomNameA));
    char n[64], args[160]; snprintf(n, sizeof n, "k32t%lu_xproc", GetCurrentProcessId());
    ATOM a = U(GlobalAddAtomA)(n);
    CHECK(a >= 0xC000);
    snprintf(args, sizeof args, "%s %x", n, a);
    EXPECT_EQ(t_run_child("atom_find_global", args, 20000), 0);
    EXPECT_EQ(U(GlobalFindAtomA)(n), a);
    U(GlobalDeleteAtom)(a);
    EXPECT_EQ(U(GlobalFindAtomA)(n), 0);
}

TEST(atoms_global_survives_creating_process) {
    REQUIRE(U(GlobalFindAtomA)); REQUIRE(U(GlobalDeleteAtom));
    char n[64]; snprintf(n, sizeof n, "k32t%lu_orphan", GetCurrentProcessId());
    EXPECT_EQ(t_run_child("atom_add_global_and_exit", n, 20000), 0);
    ATOM a = U(GlobalFindAtomA)(n);
    CHECKF(a >= 0xC000, "global atom vanished with its creator (%x)", a);
    if (a >= 0xC000) { EXPECT_EQ(U(GlobalDeleteAtom)(a), 0); }
    EXPECT_EQ(U(GlobalFindAtomA)(n), 0);
}

TEST(atoms_local_not_inherited_by_child) {
    REQUIRE(U(AddAtomA)); REQUIRE(U(DeleteAtom)); REQUIRE(U(FindAtomA));
    char n[64]; snprintf(n, sizeof n, "k32t%lu_localchild", GetCurrentProcessId());
    ATOM a = U(AddAtomA)(n);
    CHECK(a >= 0xC000);
    EXPECT_EQ(t_run_child("atom_local_empty", n, 20000), 0);
    EXPECT_EQ(U(FindAtomA)(n), a);
    U(DeleteAtom)(a);
}

CHILD(atom_inittable) {
    /* InitAtomTable before first use: size is only a hint */
    if (!U(InitAtomTable)(0)) return 41;
    if (!U(InitAtomTable)(100)) return 42;       /* second call is a no-op success */
    ATOM a = U(AddAtomA)("child_inittable");
    if (a < 0xC000) return 43;
    if (U(FindAtomA)("CHILD_INITTABLE") != a) return 44;
    if (U(DeleteAtom)(a) != 0) return 45;
    return 0;
}
CHILD(atom_inittable_sizes) {
    DWORD sz[] = { 1, 4, 5, 37, 100, 0x1FF, 0x200, 0x201, 0xFFFF, 0xFFFFFFFF };
    int idx = atoi(argv[1]);
    if (!U(InitAtomTable)(sz[idx])) return 51;
    char n[32];
    for (int i = 0; i < 300; i++) {
        snprintf(n, sizeof n, "it%d_%d", idx, i);
        if (U(AddAtomA)(n) < 0xC000) return 52;
    }
    for (int i = 0; i < 300; i++) {
        snprintf(n, sizeof n, "it%d_%d", idx, i);
        ATOM a = U(FindAtomA)(n);
        if (a < 0xC000) return 53;
        if (U(DeleteAtom)(a) != 0) return 54;
    }
    return 0;
}
CHILD(atom_inittable_after_use) {
    ATOM a = U(AddAtomA)("preexisting");
    if (a < 0xC000) return 61;
    if (!U(InitAtomTable)(50)) return 62;
    if (U(FindAtomA)("preexisting") != a) return 63;   /* existing table is kept */
    return 0;
}

TEST(atoms_inittable_child_basic) {
    REQUIRE(U(InitAtomTable));
    EXPECT_EQ(t_run_child("atom_inittable", "", 20000), 0);
}
TEST(atoms_inittable_child_sizes) {
    REQUIRE(U(InitAtomTable));
    for (int i = 0; i < 10; i++) {
        char a[8]; snprintf(a, sizeof a, "%d", i);
        DWORD r = t_run_child("atom_inittable_sizes", a, 20000);
        CHECKF(r == 0, "size index %d -> child exit %lu", i, r);
    }
}
TEST(atoms_inittable_child_after_first_use) {
    REQUIRE(U(InitAtomTable));
    EXPECT_EQ(t_run_child("atom_inittable_after_use", "", 20000), 0);
}

typedef struct { const char *name; volatile LONG fails; int g; } thr_ctx;
static DWORD WINAPI atom_thread(LPVOID p) {
    thr_ctx *c = p;
    for (int i = 0; i < 200; i++) {
        char n[64]; snprintf(n, sizeof n, "%s_%d", c->name, i % 20);
        ATOM a = ap_add_a(c->g, n);
        if (a < 0xC000) InterlockedIncrement(&c->fails);
        if (ap_find_a(c->g, n) != a) InterlockedIncrement(&c->fails);
        if (ap_del(c->g, a) != 0) InterlockedIncrement(&c->fails);
    }
    return 0;
}
static void atom_threads(int g) {
    char n[48]; snprintf(n, sizeof n, "k32t%lu_thr%d", GetCurrentProcessId(), g);
    thr_ctx c = { n, 0, g };
    HANDLE h[4];
    for (int i = 0; i < 4; i++) h[i] = CreateThread(NULL, 0, atom_thread, &c, 0, NULL);
    WaitForMultipleObjects(4, h, TRUE, 20000);
    for (int i = 0; i < 4; i++) CloseHandle(h[i]);
    EXPECT_EQ(c.fails, 0);
    for (int i = 0; i < 20; i++) { char m[64]; snprintf(m, sizeof m, "%s_%d", n, i); EXPECT_EQ(ap_find_a(g, m), 0); }
}
TEST(atoms_global_threads_refcount_balanced) {
    REQUIRE(U(GlobalAddAtomA)); REQUIRE(U(GlobalFindAtomA)); REQUIRE(U(GlobalDeleteAtom));
    atom_threads(1);
}
TEST(atoms_local_threads_refcount_balanced) {
    REQUIRE(U(AddAtomA)); REQUIRE(U(FindAtomA)); REQUIRE(U(DeleteAtom));
    atom_threads(0);
}

TEST(atoms_a_vs_w_ansi_codepage_note) {
    REQUIRE(U(GlobalAddAtomA)); REQUIRE(U(GlobalFindAtomW)); REQUIRE(U(GlobalDeleteAtom));
    char n[64]; WCHAR w[64];
    snprintf(n, sizeof n, "k32t%lu_cp\xe9", GetCurrentProcessId());
    ATOM a = U(GlobalAddAtomA)(n);
    CHECK(a >= 0xC000);
    int k = MultiByteToWideChar(CP_ACP, 0, n, -1, w, 64);
    CHECK(k > 0);
    EXPECT_EQ(U(GlobalFindAtomW)(w), a);     /* ANSI names are converted with the ACP */
    if (a >= 0xC000) U(GlobalDeleteAtom)(a);
}
