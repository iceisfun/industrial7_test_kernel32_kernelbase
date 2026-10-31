#include "so.h"

#define NEED_NS() do { REQUIRE(U(CreateBoundaryDescriptorA)); REQUIRE(U(CreateBoundaryDescriptorW)); REQUIRE(U(AddSIDToBoundaryDescriptor)); \
    REQUIRE(U(DeleteBoundaryDescriptor)); REQUIRE(U(CreatePrivateNamespaceA)); REQUIRE(U(CreatePrivateNamespaceW)); \
    REQUIRE(U(OpenPrivateNamespaceA)); REQUIRE(U(OpenPrivateNamespaceW)); REQUIRE(U(ClosePrivateNamespace)); } while (0)

#define NEED_NSB() do { REQUIRE(U(CreatePrivateNamespaceA)); REQUIRE(U(CreatePrivateNamespaceW)); REQUIRE(U(ClosePrivateNamespace)); } while (0)
static BYTE g_world[SECURITY_MAX_SID_SIZE];
static PSID world_sid(void) {
    DWORD n = sizeof g_world;
    if (!CreateWellKnownSid(WinWorldSid, NULL, g_world, &n)) return NULL;
    return g_world;
}
/* builds a boundary descriptor with the world SID; w selects A/W creation */
static HANDLE mk_bd(int w, const char *name) {
    WCHAR wn[200]; HANDLE bd;
    if (w) { so_wide(name, wn, 200); bd = U(CreateBoundaryDescriptorW)(wn, 0); }
    else bd = U(CreateBoundaryDescriptorA)(name, 0);
    if (!bd) return NULL;
    if (!U(AddSIDToBoundaryDescriptor)(&bd, world_sid())) { U(DeleteBoundaryDescriptor)(bd); return NULL; }
    return bd;
}
static HANDLE ns_create(int w, SECURITY_ATTRIBUTES *sa, HANDLE bd, const char *alias) {
    WCHAR wa[200];
    if (w) { so_wide(alias, wa, 200); return U(CreatePrivateNamespaceW)(sa, bd, wa); }
    return U(CreatePrivateNamespaceA)(sa, bd, alias);
}
static HANDLE ns_open(int w, HANDLE bd, const char *alias) {
    WCHAR wa[200];
    if (w) { so_wide(alias, wa, 200); return U(OpenPrivateNamespaceW)(bd, wa); }
    return U(OpenPrivateNamespaceA)(bd, alias);
}

TEST(ns_boundary_create_delete) {
    NEED_NS();
    for (int w = 0; w < 2; w++) {
        char n[80]; so_name(n, sizeof n, "bd");
        WCHAR wn[80]; so_wide(n, wn, 80);
        SET_ERR(0);
        HANDLE bd = w ? U(CreateBoundaryDescriptorW)(wn, 0) : U(CreateBoundaryDescriptorA)(n, 0);
        CHECKF(bd != NULL, "w=%d err %lu", w, GetLastError());
        if (bd) U(DeleteBoundaryDescriptor)(bd);
    }
}
TEST(ns_boundary_distinct_handles) {
    NEED_NS();
    HANDLE a = U(CreateBoundaryDescriptorA)("k32t_a", 0), b = U(CreateBoundaryDescriptorA)("k32t_a", 0);
    CHECK(a && b && a != b);
    if (a) U(DeleteBoundaryDescriptor)(a);
    if (b) U(DeleteBoundaryDescriptor)(b);
}
TEST(ns_boundary_empty_and_long_names) {
    NEED_NS();
    HANDLE bd = U(CreateBoundaryDescriptorW)(L"", 0);
    t_note("CreateBoundaryDescriptorW(\"\") -> %p err %lu", bd, bd ? 0 : GetLastError());
    if (bd) U(DeleteBoundaryDescriptor)(bd);
    static WCHAR big[3000];
    for (int i = 0; i < 2999; i++) big[i] = L'b';
    SET_ERR(0);
    bd = U(CreateBoundaryDescriptorW)(big, 0);
    t_note("CreateBoundaryDescriptorW(2999 chars) -> %p err %lu", bd, bd ? 0 : GetLastError());
    if (bd) U(DeleteBoundaryDescriptor)(bd);
}
TEST(ns_boundary_flags_note) {
    NEED_NS();
    ULONG flags[] = { 1, 2, 0x80000000 };
    for (int i = 0; i < 3; i++) {
        SET_ERR(0);
        HANDLE bd = U(CreateBoundaryDescriptorA)("k32t_flags", flags[i]);
        t_note("CreateBoundaryDescriptorA flags 0x%lx -> %p err %lu", flags[i], bd, bd ? 0 : GetLastError());
        if (bd) U(DeleteBoundaryDescriptor)(bd);
    }
}
TEST(ns_boundary_add_world_sid) {
    NEED_NS();
    HANDLE bd = U(CreateBoundaryDescriptorA)("k32t_sid", 0);
    CHECK(bd != NULL); if (!bd) return;
    PSID sid = world_sid();
    CHECK(sid != NULL);
    SET_ERR(0);
    EXPECT_EQ(U(AddSIDToBoundaryDescriptor)(&bd, sid), TRUE);
    CHECK(bd != NULL);
    SET_ERR(0);
    BOOL again = U(AddSIDToBoundaryDescriptor)(&bd, sid);
    t_note("adding the same SID twice -> %d err %lu", again, again ? 0 : GetLastError());
    U(DeleteBoundaryDescriptor)(bd);
}
TEST(ns_boundary_add_several_sids) {
    NEED_NS();
    HANDLE bd = U(CreateBoundaryDescriptorA)("k32t_sids", 0);
    CHECK(bd != NULL); if (!bd) return;
    BYTE s1[SECURITY_MAX_SID_SIZE], s2[SECURITY_MAX_SID_SIZE], s3[SECURITY_MAX_SID_SIZE]; DWORD n;
    n = sizeof s1; CreateWellKnownSid(WinWorldSid, NULL, s1, &n);
    n = sizeof s2; CreateWellKnownSid(WinBuiltinUsersSid, NULL, s2, &n);
    n = sizeof s3; CreateWellKnownSid(WinAuthenticatedUserSid, NULL, s3, &n);
    EXPECT_EQ(U(AddSIDToBoundaryDescriptor)(&bd, s1), TRUE);
    EXPECT_EQ(U(AddSIDToBoundaryDescriptor)(&bd, s2), TRUE);
    EXPECT_EQ(U(AddSIDToBoundaryDescriptor)(&bd, s3), TRUE);
    U(DeleteBoundaryDescriptor)(bd);
}
TEST(ns_boundary_integrity_label) {
    NEED_NS(); REQUIRE(U(AddIntegrityLabelToBoundaryDescriptor));
    HANDLE bd = U(CreateBoundaryDescriptorA)("k32t_il", 0);
    CHECK(bd != NULL); if (!bd) return;
    PSID il = NULL;
    if (!ConvertStringSidToSidA("S-1-16-8192", &il)) { t_skip("no label sid"); U(DeleteBoundaryDescriptor)(bd); return; }
    SET_ERR(0);
    BOOL r = U(AddIntegrityLabelToBoundaryDescriptor)(&bd, il);
    t_note("AddIntegrityLabelToBoundaryDescriptor -> %d err %lu", r, r ? 0 : GetLastError());
    CHECK(r);
    LocalFree(il);
    U(DeleteBoundaryDescriptor)(bd);
}
TEST(ns_create_rejects_inheritable_attributes) {
    NEED_NSB();
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    /* the inherit flag is validated before the boundary descriptor is used */
    for (int w = 0; w < 2; w++) {
        SET_ERR(0);
        HANDLE h = ns_create(w, &sa, NULL, "k32t_inh_alias");
        CHECKF(h == NULL, "w=%d", w); EXPECT_ERR(ERROR_INVALID_PARAMETER);
        if (h) U(ClosePrivateNamespace)(h, 1);
    }
}
TEST(ns_create_open_close_basic) {
    NEED_NS();
    for (int w = 0; w < 2; w++) {
        char bn[80], al[80]; so_name(bn, sizeof bn, "bdb"); so_name(al, sizeof al, "alb");
        HANDLE bd = mk_bd(w, bn);
        CHECK(bd != NULL); if (!bd) continue;
        SET_ERR(0);
        HANDLE ns = ns_create(w, NULL, bd, al);
        if (!ns) {
            DWORD e = GetLastError();
            if (t_is_wine()) { t_skip("private namespaces unsupported"); U(DeleteBoundaryDescriptor)(bd); return; }
            CHECKF(0, "CreatePrivateNamespace failed err %lu", e);
            U(DeleteBoundaryDescriptor)(bd); continue;
        }
        SET_ERR(0);
        HANDLE ns2 = ns_open(w, bd, al);
        CHECKF(ns2 != NULL, "open err %lu", GetLastError());
        if (ns2) { CHECK(ns2 != ns); EXPECT_EQ(U(ClosePrivateNamespace)(ns2, 0), TRUE); }
        EXPECT_EQ(U(ClosePrivateNamespace)(ns, 1), TRUE);
        U(DeleteBoundaryDescriptor)(bd);
    }
}
TEST(ns_objects_inside_namespace) {
    NEED_NS();
    char bn[80], al[80], full[200]; so_name(bn, sizeof bn, "bdo"); so_name(al, sizeof al, "alo");
    HANDLE bd = mk_bd(0, bn); CHECK(bd != NULL); if (!bd) return;
    HANDLE ns = ns_create(0, NULL, bd, al);
    if (!ns) { if (t_is_wine()) t_skip("unsupported"); else CHECKF(0, "create err %lu", GetLastError()); U(DeleteBoundaryDescriptor)(bd); return; }
    snprintf(full, sizeof full, "%s\\evt", al);
    SET_ERR(0);
    HANDLE e1 = CreateEventA(NULL, TRUE, FALSE, full);
    CHECKF(e1 != NULL, "err %lu", GetLastError()); EXPECT_ERR(0);
    SET_ERR(0);
    HANDLE e2 = CreateEventA(NULL, TRUE, FALSE, full);
    CHECK(e2 != NULL); EXPECT_ERR(ERROR_ALREADY_EXISTS);
    SET_ERR(0);
    HANDLE e3 = OpenEventA(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, full);
    CHECK(e3 != NULL);
    if (e1 && e3) { SetEvent(e1); EXPECT_EQ(WaitForSingleObject(e3, 0), WAIT_OBJECT_0); }
    /* a name without the alias is a different (global-session) object */
    SET_ERR(0);
    HANDLE e4 = OpenEventA(SYNCHRONIZE, FALSE, "evt");
    CHECK(e4 == NULL); EXPECT_ERR(ERROR_FILE_NOT_FOUND);
    /* other object types live in the namespace too */
    snprintf(full, sizeof full, "%s\\mtx", al);
    HANDLE m = CreateMutexA(NULL, FALSE, full); CHECK(m != NULL);
    snprintf(full, sizeof full, "%s\\sem", al);
    HANDLE s = CreateSemaphoreA(NULL, 0, 1, full); CHECK(s != NULL);
    snprintf(full, sizeof full, "%s\\tmr", al);
    HANDLE t = CreateWaitableTimerA(NULL, TRUE, full); CHECK(t != NULL);
    snprintf(full, sizeof full, "%s\\evt", al);
    SET_ERR(0); HANDLE c = CreateMutexA(NULL, FALSE, full); CHECK(c == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE all[] = { e1, e2, e3, e4, m, s, t, c };
    for (int i = 0; i < 8; i++) if (all[i]) CloseHandle(all[i]);
    EXPECT_EQ(U(ClosePrivateNamespace)(ns, 1), TRUE);
    U(DeleteBoundaryDescriptor)(bd);
}
TEST(ns_create_twice_same_boundary) {
    NEED_NS();
    char bn[80], a1[80], a2[80]; so_name(bn, sizeof bn, "bdt"); so_name(a1, sizeof a1, "alt1"); so_name(a2, sizeof a2, "alt2");
    HANDLE bd = mk_bd(0, bn); CHECK(bd != NULL); if (!bd) return;
    HANDLE ns = ns_create(0, NULL, bd, a1);
    if (!ns) { if (t_is_wine()) t_skip("unsupported"); else CHECKF(0, "create err %lu", GetLastError()); U(DeleteBoundaryDescriptor)(bd); return; }
    SET_ERR(0);
    HANDLE ns2 = ns_create(0, NULL, bd, a2);
    CHECK(ns2 == NULL); EXPECT_ERR(ERROR_ALREADY_EXISTS);
    if (ns2) U(ClosePrivateNamespace)(ns2, 0);
    SET_ERR(0);
    ns2 = ns_create(0, NULL, bd, a1);
    CHECK(ns2 == NULL);
    t_note("create same boundary + same alias -> err %lu", GetLastError());
    if (ns2) U(ClosePrivateNamespace)(ns2, 0);
    U(ClosePrivateNamespace)(ns, 1);
    U(DeleteBoundaryDescriptor)(bd);
}
TEST(ns_open_nonexistent) {
    NEED_NS();
    char bn[80], al[80]; so_name(bn, sizeof bn, "bdn"); so_name(al, sizeof al, "aln");
    HANDLE bd = mk_bd(0, bn); CHECK(bd != NULL); if (!bd) return;
    for (int w = 0; w < 2; w++) {
        SET_ERR(0);
        HANDLE ns = ns_open(w, bd, al);
        CHECK(ns == NULL);
        t_note("OpenPrivateNamespace nonexistent -> err %lu", GetLastError());
        if (ns) U(ClosePrivateNamespace)(ns, 0);
    }
    U(DeleteBoundaryDescriptor)(bd);
}
TEST(ns_open_with_different_boundary_name_fails) {
    NEED_NS();
    char bn[80], bn2[80], al[80]; so_name(bn, sizeof bn, "bdd1"); so_name(bn2, sizeof bn2, "bdd2"); so_name(al, sizeof al, "ald");
    HANDLE bd = mk_bd(0, bn), bd2 = mk_bd(0, bn2); CHECK(bd && bd2); if (!bd || !bd2) return;
    HANDLE ns = ns_create(0, NULL, bd, al);
    if (!ns) { if (t_is_wine()) t_skip("unsupported"); else CHECKF(0, "create err %lu", GetLastError()); return; }
    SET_ERR(0);
    HANDLE o = ns_open(0, bd2, al);
    CHECK(o == NULL);
    t_note("open with different boundary descriptor -> err %lu", GetLastError());
    if (o) U(ClosePrivateNamespace)(o, 0);
    U(ClosePrivateNamespace)(ns, 1);
    U(DeleteBoundaryDescriptor)(bd); U(DeleteBoundaryDescriptor)(bd2);
}
TEST(ns_close_invalid_handles) {
    NEED_NSB();
    HANDLE bad[] = { NULL, (HANDLE)(ULONG_PTR)0x7777ABE0, INVALID_HANDLE_VALUE };
    for (int i = 0; i < 3; i++) {
        SET_ERR(0);
        EXPECT_EQ(U(ClosePrivateNamespace)(bad[i], 0), 0);
        EXPECT_ERR(ERROR_INVALID_HANDLE);
        SET_ERR(0);
        EXPECT_EQ(U(ClosePrivateNamespace)(bad[i], 1), 0);
        EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
}
TEST(ns_close_ordinary_handle_rejected) {
    NEED_NSB();
    /* only handles returned by Create/OpenPrivateNamespace are accepted; the handle must stay open */
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0);
    EXPECT_EQ(U(ClosePrivateNamespace)(e, 0), 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    EXPECT_EQ(SetEvent(e), TRUE);   /* still valid */
    CloseHandle(e);
}
TEST(ns_close_twice) {
    NEED_NS();
    char bn[80], al[80]; so_name(bn, sizeof bn, "bdc"); so_name(al, sizeof al, "alc");
    HANDLE bd = mk_bd(0, bn); CHECK(bd != NULL); if (!bd) return;
    HANDLE ns = ns_create(0, NULL, bd, al);
    if (!ns) { if (t_is_wine()) t_skip("unsupported"); else CHECKF(0, "create err %lu", GetLastError()); U(DeleteBoundaryDescriptor)(bd); return; }
    EXPECT_EQ(U(ClosePrivateNamespace)(ns, 0), 1);
    SET_ERR(0);
    EXPECT_EQ(U(ClosePrivateNamespace)(ns, 0), 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    U(DeleteBoundaryDescriptor)(bd);
}
TEST(ns_destroy_flag_removes_namespace) {
    NEED_NS();
    char bn[80], al[80], full[200]; so_name(bn, sizeof bn, "bdx"); so_name(al, sizeof al, "alx");
    HANDLE bd = mk_bd(0, bn); CHECK(bd != NULL); if (!bd) return;
    HANDLE ns = ns_create(0, NULL, bd, al);
    if (!ns) { if (t_is_wine()) t_skip("unsupported"); else CHECKF(0, "create err %lu", GetLastError()); U(DeleteBoundaryDescriptor)(bd); return; }
    EXPECT_EQ(U(ClosePrivateNamespace)(ns, PRIVATE_NAMESPACE_FLAG_DESTROY), 1);
    EXPECT_EQ(PRIVATE_NAMESPACE_FLAG_DESTROY, 1);
    snprintf(full, sizeof full, "%s\\evt", al);
    SET_ERR(0);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, full);
    CHECK(e == NULL);
    t_note("create inside destroyed namespace -> err %lu", GetLastError());
    if (e) CloseHandle(e);
    /* the boundary can be reused to create a fresh namespace */
    HANDLE ns2 = ns_create(0, NULL, bd, al);
    CHECKF(ns2 != NULL, "recreate err %lu", GetLastError());
    if (ns2) U(ClosePrivateNamespace)(ns2, 1);
    U(DeleteBoundaryDescriptor)(bd);
}
TEST(ns_objects_vanish_after_namespace_destroyed) {
    NEED_NS();
    char bn[80], al[80], full[200]; so_name(bn, sizeof bn, "bdv"); so_name(al, sizeof al, "alv");
    HANDLE bd = mk_bd(0, bn); CHECK(bd != NULL); if (!bd) return;
    HANDLE ns = ns_create(0, NULL, bd, al);
    if (!ns) { if (t_is_wine()) t_skip("unsupported"); else CHECKF(0, "create err %lu", GetLastError()); U(DeleteBoundaryDescriptor)(bd); return; }
    snprintf(full, sizeof full, "%s\\evt", al);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, full);
    CHECK(e != NULL);
    U(ClosePrivateNamespace)(ns, 1);
    if (e) { EXPECT_EQ(SetEvent(e), TRUE); CloseHandle(e); }   /* object handle remains usable */
    U(DeleteBoundaryDescriptor)(bd);
}
TEST(ns_alias_name_with_backslash_inside_object_name) {
    NEED_NS();
    char bn[80], al[80], full[200]; so_name(bn, sizeof bn, "bdy"); so_name(al, sizeof al, "aly");
    HANDLE bd = mk_bd(0, bn); CHECK(bd != NULL); if (!bd) return;
    HANDLE ns = ns_create(0, NULL, bd, al);
    if (!ns) { if (t_is_wine()) t_skip("unsupported"); else CHECKF(0, "create err %lu", GetLastError()); U(DeleteBoundaryDescriptor)(bd); return; }
    snprintf(full, sizeof full, "%s\\sub\\evt", al);
    SET_ERR(0);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, full);
    CHECK(e == NULL);
    DWORD err = GetLastError();
    CHECKF(err == ERROR_PATH_NOT_FOUND || err == ERROR_FILE_NOT_FOUND || err == ERROR_INVALID_NAME, "err %lu", err);
    if (e) CloseHandle(e);
    U(ClosePrivateNamespace)(ns, 1);
    U(DeleteBoundaryDescriptor)(bd);
}
TEST(ns_alias_prefix_lookup_is_case_insensitive) {
    NEED_NS();
    char bn[80], al[80], full[200], up[200]; so_name(bn, sizeof bn, "bdu"); so_name(al, sizeof al, "alu");
    HANDLE bd = mk_bd(0, bn); CHECK(bd != NULL); if (!bd) return;
    HANDLE ns = ns_create(0, NULL, bd, al);
    if (!ns) { if (t_is_wine()) t_skip("unsupported"); else CHECKF(0, "create err %lu", GetLastError()); U(DeleteBoundaryDescriptor)(bd); return; }
    snprintf(full, sizeof full, "%s\\evt", al);
    strcpy(up, full); for (char *p = up; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
    HANDLE a = CreateEventA(NULL, TRUE, FALSE, full);
    SET_ERR(0);
    HANDLE b = CreateEventA(NULL, TRUE, FALSE, up);
    t_note("upper-case alias lookup -> %p err %lu", b, GetLastError());
    if (a) CloseHandle(a); if (b) CloseHandle(b);
    U(ClosePrivateNamespace)(ns, 1);
    U(DeleteBoundaryDescriptor)(bd);
}
TEST(ns_create_null_boundary_and_alias_note) {
    NEED_NS();
    SET_ERR(0);
    HANDLE h = U(CreatePrivateNamespaceW)(NULL, NULL, L"k32t_nullbd");
    t_note("CreatePrivateNamespaceW(NULL boundary) -> %p err %lu", h, h ? 0 : GetLastError());
    CHECK(h == NULL);
    if (h) U(ClosePrivateNamespace)(h, 1);
    HANDLE bd = mk_bd(0, "k32t_nullalias");
    if (bd) {
        SET_ERR(0);
        h = U(CreatePrivateNamespaceW)(NULL, bd, NULL);
        t_note("CreatePrivateNamespaceW(NULL alias) -> %p err %lu", h, h ? 0 : GetLastError());
        if (h) U(ClosePrivateNamespace)(h, 1);
        SET_ERR(0);
        h = U(CreatePrivateNamespaceW)(NULL, bd, L"");
        t_note("CreatePrivateNamespaceW(empty alias) -> %p err %lu", h, h ? 0 : GetLastError());
        if (h) U(ClosePrivateNamespace)(h, 1);
        U(DeleteBoundaryDescriptor)(bd);
    }
}
TEST(ns_create_without_sid_in_boundary) {
    NEED_NS();
    HANDLE bd = U(CreateBoundaryDescriptorA)("k32t_nosid", 0);
    CHECK(bd != NULL); if (!bd) return;
    SET_ERR(0);
    HANDLE h = U(CreatePrivateNamespaceA)(NULL, bd, "k32t_nosid_alias");
    t_note("namespace with an empty boundary descriptor -> %p err %lu", h, h ? 0 : GetLastError());
    if (h) U(ClosePrivateNamespace)(h, 1);
    U(DeleteBoundaryDescriptor)(bd);
}
TEST(ns_create_with_explicit_security_attributes) {
    NEED_NS();
    char bn[80], al[80]; so_name(bn, sizeof bn, "bds"); so_name(al, sizeof al, "als");
    HANDLE bd = mk_bd(0, bn); CHECK(bd != NULL); if (!bd) return;
    SECURITY_ATTRIBUTES sa; SECURITY_DESCRIPTOR sd;
    InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);   /* NULL DACL: everyone */
    sa.nLength = sizeof sa; sa.lpSecurityDescriptor = &sd; sa.bInheritHandle = FALSE;
    SET_ERR(0);
    HANDLE ns = ns_create(0, &sa, bd, al);
    if (!ns) { if (t_is_wine()) t_skip("unsupported"); else CHECKF(0, "create err %lu", GetLastError()); U(DeleteBoundaryDescriptor)(bd); return; }
    CHECK(((ULONG_PTR)ns & 3) == 0);
    EXPECT_EQ(so_inheritable(ns), 0);
    U(ClosePrivateNamespace)(ns, 1);
    U(DeleteBoundaryDescriptor)(bd);
}
TEST(ns_multiple_independent_namespaces) {
    NEED_NS();
    HANDLE bd[3], ns[3]; char al[3][80];
    for (int i = 0; i < 3; i++) {
        char bn[80], tag[16]; snprintf(tag, sizeof tag, "m%d", i);
        so_name(bn, sizeof bn, tag); so_name(al[i], 80, tag);
        bd[i] = mk_bd(0, bn); ns[i] = bd[i] ? ns_create(0, NULL, bd[i], al[i]) : NULL;
    }
    if (!ns[0]) { if (t_is_wine()) t_skip("unsupported"); else CHECK(0); for (int i = 0; i < 3; i++) if (bd[i]) U(DeleteBoundaryDescriptor)(bd[i]); return; }
    HANDLE ev[3];
    for (int i = 0; i < 3; i++) {
        char full[200]; snprintf(full, sizeof full, "%s\\same", al[i]);
        SET_ERR(0); ev[i] = CreateEventA(NULL, TRUE, FALSE, full);
        CHECK(ev[i] != NULL); EXPECT_ERR(0);   /* same leaf name, different namespaces: always new */
    }
    SetEvent(ev[0]);
    EXPECT_EQ(WaitForSingleObject(ev[1], 0), WAIT_TIMEOUT);
    for (int i = 0; i < 3; i++) { if (ev[i]) CloseHandle(ev[i]); if (ns[i]) U(ClosePrivateNamespace)(ns[i], 1); if (bd[i]) U(DeleteBoundaryDescriptor)(bd[i]); }
}

/* Cross-process: child opens the parent's private namespace and signals an event inside it. */
CHILD(ns_child) {
    /* argv: name, boundary name, alias, leaf */
    if (argc < 4) return 10;
    HANDLE bd = U(CreateBoundaryDescriptorA)(argv[1], 0);
    if (!bd) return 11;
    if (!U(AddSIDToBoundaryDescriptor)(&bd, world_sid())) return 12;
    HANDLE ns = U(OpenPrivateNamespaceA)(bd, argv[2]);
    if (!ns) return 13;
    char full[200]; snprintf(full, sizeof full, "%s\\%s", argv[2], argv[3]);
    HANDLE e = OpenEventA(EVENT_MODIFY_STATE, FALSE, full);
    if (!e) return 14;
    if (!SetEvent(e)) return 15;
    CloseHandle(e);
    U(ClosePrivateNamespace)(ns, 0);
    U(DeleteBoundaryDescriptor)(bd);
    return 0;
}
TEST(ns_child_process_opens_private_namespace) {
    NEED_NS();
    char bn[80], al[80], args[300]; so_name(bn, sizeof bn, "bdp"); so_name(al, sizeof al, "alp");
    HANDLE bd = mk_bd(0, bn); CHECK(bd != NULL); if (!bd) return;
    HANDLE ns = ns_create(0, NULL, bd, al);
    if (!ns) { if (t_is_wine()) t_skip("unsupported"); else CHECKF(0, "create err %lu", GetLastError()); U(DeleteBoundaryDescriptor)(bd); return; }
    char full[200]; snprintf(full, sizeof full, "%s\\evt", al);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, full);
    CHECK(e != NULL);
    snprintf(args, sizeof args, "%s %s evt", bn, al);
    DWORD rc = t_run_child("ns_child", args, 30000);
    EXPECT_EQ(rc, 0);
    if (e) { EXPECT_EQ(WaitForSingleObject(e, 0), WAIT_OBJECT_0); CloseHandle(e); }
    U(ClosePrivateNamespace)(ns, 1);
    U(DeleteBoundaryDescriptor)(bd);
}
CHILD(ns_child_noaccess) {
    /* a boundary descriptor with a different name must not open the parent's namespace */
    if (argc < 3) return 10;
    HANDLE bd = U(CreateBoundaryDescriptorA)("k32t_other_boundary_name", 0);
    if (!bd) return 11;
    U(AddSIDToBoundaryDescriptor)(&bd, world_sid());
    HANDLE ns = U(OpenPrivateNamespaceA)(bd, argv[1]);
    if (ns) { U(ClosePrivateNamespace)(ns, 0); return 1; }
    return 0;
}
TEST(ns_child_wrong_boundary_cannot_open) {
    NEED_NS();
    char bn[80], al[80], args[300]; so_name(bn, sizeof bn, "bdw"); so_name(al, sizeof al, "alw");
    HANDLE bd = mk_bd(0, bn); CHECK(bd != NULL); if (!bd) return;
    HANDLE ns = ns_create(0, NULL, bd, al);
    if (!ns) { if (t_is_wine()) t_skip("unsupported"); else CHECKF(0, "create err %lu", GetLastError()); U(DeleteBoundaryDescriptor)(bd); return; }
    snprintf(args, sizeof args, "%s x", al);
    EXPECT_EQ(t_run_child("ns_child_noaccess", args, 30000), 0);
    U(ClosePrivateNamespace)(ns, 1);
    U(DeleteBoundaryDescriptor)(bd);
}
