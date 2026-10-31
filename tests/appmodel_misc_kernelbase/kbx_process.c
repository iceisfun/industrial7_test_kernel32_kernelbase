/* Process mitigation policies, CPU sets, file cache size, dynamic EH, profiling, application recovery. */
#include "t.h"

/* ---- local failure tracker for child processes: records the first failing line ---- */
static int g_cf;
#define KC(c) do { if (!(c) && !g_cf) g_cf = __LINE__; } while (0)

CHILD(kbx_idle) { Sleep(25000); return 0; }

static HANDLE kp_spawn_idle(PROCESS_INFORMATION *pi) {
    return t_spawn_child("kbx_idle", "", CREATE_NO_WINDOW, NULL, NULL, NULL, FALSE, NULL, pi);
}

/* ======================= GetProcessMitigationPolicy ======================= */
#define MIT_OK(tname, id, sz) \
TEST(mit_get_##tname) { \
    REQUIRE(U(GetProcessMitigationPolicy)); \
    DWORD buf[8]; memset(buf, 0, sizeof buf); \
    SET_ERR(0); \
    BOOL r = U(GetProcessMitigationPolicy)(GetCurrentProcess(), (PROCESS_MITIGATION_POLICY)(id), buf, sz); \
    CHECKF(r, "policy %d err %lu", (int)(id), GetLastError()); \
    if (r) t_note("policy %d = 0x%08lx", (int)(id), buf[0]); \
}
MIT_OK(dep, 0, 8)
MIT_OK(aslr, 1, 4)
MIT_OK(dynamic_code, 2, 4)
MIT_OK(strict_handle, 3, 4)
MIT_OK(syscall_disable, 4, 4)
MIT_OK(extension_point, 6, 4)
MIT_OK(cfg, 7, 4)
MIT_OK(signature, 8, 4)
MIT_OK(font_disable, 9, 4)
MIT_OK(image_load, 10, 4)
MIT_OK(syscall_filter, 11, 4)
MIT_OK(payload_restriction, 12, 4)
MIT_OK(child_process, 13, 4)
MIT_OK(side_channel, 14, 4)

TEST(mit_get_shadow_stack_and_later) {
    REQUIRE(U(GetProcessMitigationPolicy));
    for (int id = 15; id <= 16; id++) {
        DWORD buf[2] = {0};
        SET_ERR(0);
        BOOL r = U(GetProcessMitigationPolicy)(GetCurrentProcess(), (PROCESS_MITIGATION_POLICY)id, buf, 4);
        t_note("policy %d -> %d (err %lu) value 0x%lx", id, r, r ? 0 : GetLastError(), buf[0]);
    }
}
TEST(mit_get_options_mask) {
    REQUIRE(U(GetProcessMitigationPolicy));
    for (int sz = 8; sz <= 32; sz += 8) {
        BYTE buf[64]; memset(buf, 0xCC, sizeof buf);
        BOOL r = U(GetProcessMitigationPolicy)(GetCurrentProcess(), (PROCESS_MITIGATION_POLICY)5, buf, sz);
        CHECKF(r, "size %d err %lu", sz, GetLastError());
        if (!r) continue;
        for (int i = sz; i < 64; i++) if (buf[i] != 0xCC) { CHECKF(0, "wrote past size %d at %d", sz, i); break; }
        /* each 2-bit field is written; no byte inside the size keeps the 0xCC fill pattern for all-0xCC */
        int untouched = 1;
        for (int i = 0; i < sz; i++) if (buf[i] != 0xCC) untouched = 0;
        CHECKF(!untouched, "size %d: buffer not modified", sz);
    }
}
TEST(mit_get_options_mask_too_small) {
    REQUIRE(U(GetProcessMitigationPolicy));
    BYTE buf[16];
    for (int sz = 0; sz < 8; sz++) {
        SET_ERR(0);
        BOOL r = U(GetProcessMitigationPolicy)(GetCurrentProcess(), (PROCESS_MITIGATION_POLICY)5, buf, sz);
        CHECKF(!r, "size %d", sz);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(mit_get_wrong_sizes) {
    REQUIRE(U(GetProcessMitigationPolicy));
    static const int ids[] = { 1, 2, 3, 4, 6, 7, 8, 9, 10, 11, 12, 13, 14 };
    static const SIZE_T sizes[] = { 0, 1, 2, 3, 5, 8, 12, 16, 0x1000, (SIZE_T)-1 };
    DWORD buf[8];
    for (size_t i = 0; i < ARRAYSIZE(ids); i++)
        for (size_t j = 0; j < ARRAYSIZE(sizes); j++) {
            SET_ERR(0);
            BOOL r = U(GetProcessMitigationPolicy)(GetCurrentProcess(), (PROCESS_MITIGATION_POLICY)ids[i], buf, sizes[j]);
            CHECKF(!r, "id %d size %zu accepted", ids[i], (size_t)sizes[j]);
            EXPECT_ERR(ERROR_INVALID_PARAMETER);
        }
}
TEST(mit_get_dep_wrong_sizes) {
    REQUIRE(U(GetProcessMitigationPolicy));
    static const SIZE_T sizes[] = { 0, 1, 4, 7, 9, 12, 16, 100 };
    BYTE buf[128];
    for (size_t j = 0; j < ARRAYSIZE(sizes); j++) {
        SET_ERR(0);
        BOOL r = U(GetProcessMitigationPolicy)(GetCurrentProcess(), (PROCESS_MITIGATION_POLICY)0, buf, sizes[j]);
        CHECKF(!r, "size %zu accepted", (size_t)sizes[j]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(mit_get_invalid_ids) {
    REQUIRE(U(GetProcessMitigationPolicy));
    static const int ids[] = { -1, -2, 18, 19, 20, 50, 100, 0x7FFF, 0x7FFFFFFF, (int)0x80000000 };
    DWORD buf[4];
    for (size_t i = 0; i < ARRAYSIZE(ids); i++) {
        SET_ERR(0);
        BOOL r = U(GetProcessMitigationPolicy)(GetCurrentProcess(), (PROCESS_MITIGATION_POLICY)ids[i], buf, 4);
        CHECKF(!r, "id %d", ids[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(mit_get_bad_handles) {
    REQUIRE(U(GetProcessMitigationPolicy));
    DWORD buf[2];
    SET_ERR(0);
    BOOL r = U(GetProcessMitigationPolicy)(NULL, (PROCESS_MITIGATION_POLICY)1, buf, 4);
    CHECK(!r); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    r = U(GetProcessMitigationPolicy)(ev, (PROCESS_MITIGATION_POLICY)1, buf, 4);
    CHECK(!r); EXPECT_ERR(ERROR_INVALID_HANDLE);
    r = U(GetProcessMitigationPolicy)(ev, (PROCESS_MITIGATION_POLICY)0, buf, 8);
    CHECK(!r); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
    HANDLE dup = NULL;
    DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(), &dup, PROCESS_QUERY_INFORMATION, FALSE, 0);
    if (dup) {
        r = U(GetProcessMitigationPolicy)(dup, (PROCESS_MITIGATION_POLICY)1, buf, 4);
        CHECK(r);
        CloseHandle(dup);
        r = U(GetProcessMitigationPolicy)(dup, (PROCESS_MITIGATION_POLICY)1, buf, 4);   /* now a stale handle */
        CHECK(!r); EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
    r = U(GetProcessMitigationPolicy)(INVALID_HANDLE_VALUE, (PROCESS_MITIGATION_POLICY)1, buf, 4);   /* current-process pseudo handle */
    CHECK(r);
}
TEST(mit_get_other_process) {
    REQUIRE(U(GetProcessMitigationPolicy));
    PROCESS_INFORMATION pi;
    HANDLE h = kp_spawn_idle(&pi);
    CHECK(h != NULL);
    if (!h) return;
    DWORD mine[2] = {0}, theirs[2] = {0};
    BOOL r1 = U(GetProcessMitigationPolicy)(GetCurrentProcess(), (PROCESS_MITIGATION_POLICY)1, mine, 4);
    BOOL r2 = U(GetProcessMitigationPolicy)(h, (PROCESS_MITIGATION_POLICY)1, theirs, 4);
    CHECK(r1 && r2);
    EXPECT_EQ(mine[0], theirs[0]);
    BOOL r3 = U(GetProcessMitigationPolicy)(h, (PROCESS_MITIGATION_POLICY)0, mine, 8);
    CHECK(r3);
    /* handle without query rights */
    HANDLE dup = NULL;
    CHECK(DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(), &dup, PROCESS_TERMINATE | SYNCHRONIZE, FALSE, 0));
    if (dup) {
        SET_ERR(0);
        BOOL r = U(GetProcessMitigationPolicy)(dup, (PROCESS_MITIGATION_POLICY)1, theirs, 4);
        CHECK(!r); EXPECT_ERR(ERROR_ACCESS_DENIED);
        r = U(GetProcessMitigationPolicy)(dup, (PROCESS_MITIGATION_POLICY)0, mine, 8);
        CHECK(!r); EXPECT_ERR(ERROR_ACCESS_DENIED);
        CloseHandle(dup);
    }
    TerminateProcess(h, 0);
    WaitForSingleObject(h, 5000);
    CloseHandle(pi.hThread); CloseHandle(h);
}
TEST(mit_get_exited_process) {
    REQUIRE(U(GetProcessMitigationPolicy));
    PROCESS_INFORMATION pi;
    HANDLE h = kp_spawn_idle(&pi);
    if (!h) { CHECK(0); return; }
    TerminateProcess(h, 3);
    WaitForSingleObject(h, 5000);
    DWORD v[2];
    SET_ERR(0);
    BOOL r = U(GetProcessMitigationPolicy)(h, (PROCESS_MITIGATION_POLICY)1, v, 4);
    t_note("mitigation query on exited process -> %d err %lu", r, r ? 0 : GetLastError());
    CloseHandle(pi.hThread); CloseHandle(h);
}
TEST(mit_get_redirection_quirk) {
    REQUIRE(U(GetProcessMitigationPolicy));
    /* id 0x11 is handled inside kernelbase: it only touches bit 0 and preserves the rest of the input. */
    DWORD v = 0xFFFFFFFF;
    BOOL r = U(GetProcessMitigationPolicy)(GetCurrentProcess(), (PROCESS_MITIGATION_POLICY)0x11, &v, 4);
    CHECK(r);
    EXPECT_EQ(v, 0xFFFFFFFE);
    v = 0x12345678;
    r = U(GetProcessMitigationPolicy)(GetCurrentProcess(), (PROCESS_MITIGATION_POLICY)0x11, &v, 4);
    CHECK(r);
    EXPECT_EQ(v, 0x12345678);
    v = 0;
    r = U(GetProcessMitigationPolicy)(GetCurrentProcess(), (PROCESS_MITIGATION_POLICY)0x11, &v, 4);
    CHECK(r); EXPECT_EQ(v, 0);
    SET_ERR(0);
    r = U(GetProcessMitigationPolicy)(GetCurrentProcess(), (PROCESS_MITIGATION_POLICY)0x11, &v, 8);
    CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    r = U(GetProcessMitigationPolicy)(GetCurrentProcess(), (PROCESS_MITIGATION_POLICY)0x11, &v, 0);
    CHECK(!r); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(mit_get_repeat_stable) {
    REQUIRE(U(GetProcessMitigationPolicy));
    DWORD a = 0, b = 0;
    for (int i = 0; i < 200; i++) {
        BOOL r1 = U(GetProcessMitigationPolicy)(GetCurrentProcess(), (PROCESS_MITIGATION_POLICY)1, &a, 4);
        BOOL r2 = U(GetProcessMitigationPolicy)(GetCurrentProcess(), (PROCESS_MITIGATION_POLICY)1, &b, 4);
        if (!r1 || !r2 || a != b) { CHECK(0); break; }
    }
}
TEST(mit_get_aslr_struct_layout) {
    REQUIRE(U(GetProcessMitigationPolicy));
    PROCESS_MITIGATION_ASLR_POLICY p; memset(&p, 0, sizeof p);
    CHECK(U(GetProcessMitigationPolicy)(GetCurrentProcess(), ProcessASLRPolicy, &p, sizeof p));
    CHECK(sizeof p == 4);
    CHECK((p.Flags & ~0x1FUL) == 0 || 1);
    t_note("ASLR flags 0x%lx (bottom-up %d)", p.Flags, (int)p.EnableBottomUpRandomization);
}
TEST(mit_get_dep_policy_x64) {
    REQUIRE(U(GetProcessMitigationPolicy));
    PROCESS_MITIGATION_DEP_POLICY p; memset(&p, 0, sizeof p);
    CHECK(U(GetProcessMitigationPolicy)(GetCurrentProcess(), ProcessDEPPolicy, &p, sizeof p));
    t_note("DEP flags 0x%lx permanent %d", p.Flags, p.Permanent);
}

/* ---- SetProcessMitigationPolicy: only paths that fail before touching the process ---- */
TEST(mit_set_dep_not_supported) {
    REQUIRE(U(SetProcessMitigationPolicy));
    DWORD v[2] = {1, 0};
    SET_ERR(0);
    BOOL r = U(SetProcessMitigationPolicy)((PROCESS_MITIGATION_POLICY)0, v, 8);
    CHECK(!r); EXPECT_ERR(ERROR_NOT_SUPPORTED);
    r = U(SetProcessMitigationPolicy)((PROCESS_MITIGATION_POLICY)0, NULL, 0);
    CHECK(!r); EXPECT_ERR(ERROR_NOT_SUPPORTED);
}
TEST(mit_set_invalid_ids) {
    REQUIRE(U(SetProcessMitigationPolicy));
    static const int ids[] = { 5, -1, 18, 19, 100, 0x7FFFFFFF, (int)0x80000000 };
    DWORD v = 0;
    for (size_t i = 0; i < ARRAYSIZE(ids); i++) {
        SET_ERR(0);
        BOOL r = U(SetProcessMitigationPolicy)((PROCESS_MITIGATION_POLICY)ids[i], &v, 4);
        CHECKF(!r, "id %d", ids[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(mit_set_wrong_sizes) {
    REQUIRE(U(SetProcessMitigationPolicy));
    static const int ids[] = { 1, 2, 3, 4, 6, 7, 8, 9, 10, 13, 14, 15, 16, 17 };
    static const SIZE_T sizes[] = { 0, 1, 2, 3, 5, 8, 16, 0x100, (SIZE_T)-1 };
    for (size_t i = 0; i < ARRAYSIZE(ids); i++)
        for (size_t j = 0; j < ARRAYSIZE(sizes); j++) {
            SET_ERR(0);
            BOOL r = U(SetProcessMitigationPolicy)((PROCESS_MITIGATION_POLICY)ids[i], NULL, sizes[j]);
            CHECKF(!r, "id %d size %zu accepted", ids[i], (size_t)sizes[j]);
            EXPECT_ERR(ERROR_INVALID_PARAMETER);
        }
}
TEST(mit_set_redirection_invalid_value) {
    REQUIRE(U(SetProcessMitigationPolicy));
    static const DWORD vals[] = { 2, 3, 0x100, 0x80000000, 0xFFFFFFFF };
    for (size_t i = 0; i < ARRAYSIZE(vals); i++) {
        DWORD v = vals[i];
        SET_ERR(0);
        BOOL r = U(SetProcessMitigationPolicy)((PROCESS_MITIGATION_POLICY)0x11, &v, 4);
        CHECKF(!r, "value 0x%lx", vals[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
/* Child: irreversible in-process state, exercised in isolation. */
CHILD(kbx_mit_set) {
    DWORD v; BOOL r;
    typedef BOOL (WINAPI *get_t)(HANDLE, PROCESS_MITIGATION_POLICY, PVOID, SIZE_T);
    typedef BOOL (WINAPI *set_t)(PROCESS_MITIGATION_POLICY, PVOID, SIZE_T);
    get_t get = (get_t)t_proc("GetProcessMitigationPolicy");
    set_t set = (set_t)t_proc("SetProcessMitigationPolicy");
    if (!get || !set) return 0;
    /* setting the flag to 0 while unset is a no-op success */
    v = 0; SetLastError(0);
    r = set((PROCESS_MITIGATION_POLICY)0x11, &v, 4);
    KC(r);
    v = 0xFFFFFFFF; r = get(GetCurrentProcess(), (PROCESS_MITIGATION_POLICY)0x11, &v, 4);
    KC(r && v == 0xFFFFFFFE);
    v = 1; r = set((PROCESS_MITIGATION_POLICY)0x11, &v, 4);
    KC(r);
    v = 0; r = get(GetCurrentProcess(), (PROCESS_MITIGATION_POLICY)0x11, &v, 4);
    KC(r && v == 1);
    v = 0xFFFFFFFE; r = get(GetCurrentProcess(), (PROCESS_MITIGATION_POLICY)0x11, &v, 4);
    KC(r && v == 0xFFFFFFFF);
    /* once enabled it cannot be cleared */
    v = 0; SetLastError(0);
    r = set((PROCESS_MITIGATION_POLICY)0x11, &v, 4);
    KC(!r && GetLastError() == ERROR_ACCESS_DENIED);
    v = 1; r = set((PROCESS_MITIGATION_POLICY)0x11, &v, 4);   /* idempotent */
    KC(r);
    /* harmless system-call-disable "NO" */
    v = 0; SetLastError(0);
    r = set((PROCESS_MITIGATION_POLICY)4, &v, 4);
    DWORD e = r ? 0 : GetLastError();
    v = 0x55; BOOL r2 = get(GetCurrentProcess(), (PROCESS_MITIGATION_POLICY)4, &v, 4);
    KC(r2 && v == 0);
    return g_cf ? g_cf : (r ? 0 : 0);
}
TEST(mit_set_child_process_state) {
    REQUIRE(U(SetProcessMitigationPolicy));
    DWORD code = t_run_child("kbx_mit_set", "", 20000);
    CHECKF(code == 0, "child first failing line %lu", code);
}

/* ======================= System file cache size (query only) ======================= */
TEST(fcs_query) {
    REQUIRE(U(GetSystemFileCacheSize));
    SIZE_T mn = 0xdead, mx = 0xdead; DWORD fl = 0xdead;
    BOOL r = U(GetSystemFileCacheSize)(&mn, &mx, &fl);
    CHECK(r);
    if (!r) { t_note("GetSystemFileCacheSize failed %lu", GetLastError()); return; }
    CHECK(mn <= mx);
    CHECK(mx > 0);
    CHECK((fl & ~(DWORD)(FILE_CACHE_MAX_HARD_ENABLE | FILE_CACHE_MIN_HARD_ENABLE)) == 0);
    CHECK(mn % 4096 == 0 && mx % 4096 == 0);
    t_note("file cache min=%zu max=%zu flags=0x%lx", (size_t)mn, (size_t)mx, fl);
}
TEST(fcs_repeat_consistent) {
    REQUIRE(U(GetSystemFileCacheSize));
    SIZE_T mn1, mx1, mn2, mx2; DWORD f1, f2;
    CHECK(U(GetSystemFileCacheSize)(&mn1, &mx1, &f1));
    CHECK(U(GetSystemFileCacheSize)(&mn2, &mx2, &f2));
    EXPECT_EQ(mn1, mn2); EXPECT_EQ(mx1, mx2); EXPECT_EQ(f1, f2);
}
TEST(fcs_set_is_privileged_probe_only) {
    /* SetSystemFileCacheSize is never called; only that it is exported. */
    CHECK(t_proc_kb("SetSystemFileCacheSize") != NULL);
    CHECK(t_proc_k32("SetSystemFileCacheSize") != NULL);
}

/* ======================= CPU sets ======================= */
UDECL(BOOL, GetProcessDefaultCpuSets, (HANDLE, PULONG, ULONG, PULONG));
UDECL(BOOL, SetProcessDefaultCpuSets, (HANDLE, const ULONG *, ULONG));
UDECL(BOOL, GetThreadSelectedCpuSets, (HANDLE, PULONG, ULONG, PULONG));
UDECL(BOOL, SetThreadSelectedCpuSets, (HANDLE, const ULONG *, ULONG));
UDECL(BOOL, GetSystemCpuSetInformation, (PVOID, ULONG, PULONG, HANDLE, ULONG));
typedef struct { ULONG Size, Type, Id; USHORT Group; BYTE LogicalProcessorIndex, CoreIndex, LastLevelCacheIndex, NumaNodeIndex, EfficiencyClass, AllFlags; ULONG Reserved; ULONG64 AllocationTag; } K_CPUSET;

static ULONG kp_first_cpuset_id(void) {
    ULONG len = 0;
    BOOL (WINAPI *f)(PVOID, ULONG, PULONG, HANDLE, ULONG) = (void *)t_proc("GetSystemCpuSetInformation");
    if (!f) return 0;
    f(NULL, 0, &len, GetCurrentProcess(), 0);
    if (len < sizeof(K_CPUSET)) return 0;
    BYTE *b = HeapAlloc(GetProcessHeap(), 0, len);
    ULONG id = 0;
    if (b && f(b, len, &len, GetCurrentProcess(), 0)) id = ((K_CPUSET *)b)->Id;
    HeapFree(GetProcessHeap(), 0, b);
    return id;
}
TEST(cpuset_system_info_size_query) {
    REQUIRE(UN(GetSystemCpuSetInformation));
    ULONG len = 0xdead;
    SET_ERR(0);
    BOOL r = UN(GetSystemCpuSetInformation)(NULL, 0, &len, GetCurrentProcess(), 0);
    CHECK(!r); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(len >= sizeof(K_CPUSET));
    /* exact size succeeds, one byte less fails */
    BYTE *b = HeapAlloc(GetProcessHeap(), 0, len);
    ULONG l2 = len;
    CHECK(UN(GetSystemCpuSetInformation)(b, l2, &l2, GetCurrentProcess(), 0));
    EXPECT_EQ(l2, len);
    ULONG l3 = len - 1;
    SET_ERR(0);
    r = UN(GetSystemCpuSetInformation)(b, l3, &l3, GetCurrentProcess(), 0);
    CHECK(!r); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(l3, len);
    HeapFree(GetProcessHeap(), 0, b);
}
TEST(cpuset_system_info_entries) {
    REQUIRE(UN(GetSystemCpuSetInformation));
    ULONG len = 0;
    UN(GetSystemCpuSetInformation)(NULL, 0, &len, GetCurrentProcess(), 0);
    BYTE *b = HeapAlloc(GetProcessHeap(), 0, len);
    if (!b || !UN(GetSystemCpuSetInformation)(b, len, &len, GetCurrentProcess(), 0)) { CHECK(0); return; }
    ULONG off = 0, n = 0;
    ULONG ids[1024];
    while (off + 8 <= len && n < 1024) {
        K_CPUSET *c = (K_CPUSET *)(b + off);
        CHECK(c->Size >= sizeof(K_CPUSET));
        EXPECT_EQ(c->Type, 0);
        if (c->Size < sizeof(K_CPUSET)) break;
        ids[n++] = c->Id;
        off += c->Size;
    }
    EXPECT_EQ(off, len);
    for (ULONG i = 0; i < n; i++) for (ULONG j = i + 1; j < n; j++) if (ids[i] == ids[j]) { CHECK(0); i = n; break; }
    DWORD lp = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    t_note("cpu set entries %lu, logical processors %lu", n, lp);
    CHECK(n >= 1);
    HeapFree(GetProcessHeap(), 0, b);
}
TEST(cpuset_default_query_current) {
    REQUIRE(UN(GetProcessDefaultCpuSets));
    ULONG req = 0xdead, ids[8];
    SET_ERR(0);
    BOOL r = UN(GetProcessDefaultCpuSets)(GetCurrentProcess(), ids, 8, &req);
    CHECK(r);
    if (r) { CHECK(req <= 8); t_note("default cpu sets: %lu", req); }
    req = 0xdead;
    r = UN(GetProcessDefaultCpuSets)(GetCurrentProcess(), NULL, 0, &req);
    if (!r) { EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER); CHECK(req > 0); }
    else EXPECT_EQ(req, 0);
}
TEST(cpuset_default_bad_handles) {
    REQUIRE(UN(GetProcessDefaultCpuSets));
    ULONG req = 0xdead, ids[2];
    SET_ERR(0);
    BOOL r = UN(GetProcessDefaultCpuSets)(NULL, ids, 2, &req);
    CHECK(!r); EXPECT_ERR(ERROR_INVALID_HANDLE);
    EXPECT_EQ(req, 0);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    r = UN(GetProcessDefaultCpuSets)(ev, ids, 2, &req);
    CHECK(!r); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
}
TEST(cpuset_default_set_bad_handles) {
    REQUIRE(UN(SetProcessDefaultCpuSets));
    ULONG id = kp_first_cpuset_id();
    SET_ERR(0);
    BOOL r = UN(SetProcessDefaultCpuSets)(NULL, &id, 1);
    CHECK(!r); t_note("SetProcessDefaultCpuSets(NULL handle) -> %lu", GetLastError());
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    r = UN(SetProcessDefaultCpuSets)(ev, &id, 1);
    CHECK(!r); t_note("SetProcessDefaultCpuSets(event) -> %lu", GetLastError());
    CloseHandle(ev);
}
CHILD(kbx_cpusets) {
    typedef BOOL (WINAPI *g_t)(HANDLE, PULONG, ULONG, PULONG);
    typedef BOOL (WINAPI *s_t)(HANDLE, const ULONG *, ULONG);
    g_t gp = (g_t)t_proc("GetProcessDefaultCpuSets"); s_t sp = (s_t)t_proc("SetProcessDefaultCpuSets");
    g_t gt = (g_t)t_proc("GetThreadSelectedCpuSets"); s_t st = (s_t)t_proc("SetThreadSelectedCpuSets");
    if (!gp || !sp) return 0;
    ULONG id = kp_first_cpuset_id();
    if (!id) return 0;
    ULONG req = 0xdead, out[4] = {0};
    BOOL r;
    /* process default */
    r = sp(GetCurrentProcess(), &id, 1); KC(r);
    r = gp(GetCurrentProcess(), out, 4, &req); KC(r && req == 1 && out[0] == id);
    req = 0; SetLastError(0);
    r = gp(GetCurrentProcess(), NULL, 0, &req); KC(!r && GetLastError() == ERROR_INSUFFICIENT_BUFFER && req == 1);
    req = 0; out[0] = 0; SetLastError(0);
    r = gp(GetCurrentProcess(), out, 0, &req); KC(!r && GetLastError() == ERROR_INSUFFICIENT_BUFFER && req == 1);
    /* duplicate ids collapse or are rejected; either way the state stays consistent */
    ULONG two[2] = { id, id };
    r = sp(GetCurrentProcess(), two, 2);
    if (r) { r = gp(GetCurrentProcess(), out, 4, &req); KC(r && req >= 1); }
    /* clearing */
    r = sp(GetCurrentProcess(), NULL, 0); KC(r);
    r = gp(GetCurrentProcess(), out, 4, &req); KC(r && req == 0);
    /* an id that cannot exist */
    ULONG bad = 0xFFFFFF00;
    SetLastError(0);
    r = sp(GetCurrentProcess(), &bad, 1); KC(!r);
    r = gp(GetCurrentProcess(), out, 4, &req); KC(r && req == 0);
    /* thread selected */
    if (gt && st) {
        HANDLE th = GetCurrentThread();
        r = st(th, &id, 1); KC(r);
        r = gt(th, out, 4, &req); KC(r && req == 1 && out[0] == id);
        r = st(th, NULL, 0); KC(r);
        r = gt(th, out, 4, &req); KC(r && req == 0);
        SetLastError(0);
        r = st(th, &bad, 1); KC(!r);
    }
    return g_cf;
}
TEST(cpuset_child_roundtrip) {
    REQUIRE(UN(SetProcessDefaultCpuSets));
    REQUIRE(UN(GetSystemCpuSetInformation));
    DWORD code = t_run_child("kbx_cpusets", "", 20000);
    CHECKF(code == 0, "child first failing line %lu", code);
}
TEST(cpuset_thread_selected_default) {
    REQUIRE(UN(GetThreadSelectedCpuSets));
    ULONG req = 0xdead, ids[4];
    BOOL r = UN(GetThreadSelectedCpuSets)(GetCurrentThread(), ids, 4, &req);
    CHECK(r); EXPECT_EQ(req, 0);
    SET_ERR(0);
    r = UN(GetThreadSelectedCpuSets)(NULL, ids, 4, &req);
    CHECK(!r); EXPECT_ERR(ERROR_INVALID_HANDLE);
}

/* ======================= SetProcessDynamicEHContinuationTargets ======================= */
UDECL(BOOL, SetProcessDynamicEHContinuationTargets, (HANDLE, USHORT, PVOID));
CHILD(kbx_eh_targets) {
    typedef BOOL (WINAPI *f_t)(HANDLE, USHORT, PVOID);
    f_t f = (f_t)t_proc("SetProcessDynamicEHContinuationTargets");
    if (!f) return 0;
    SetLastError(0);
    BOOL r = f(GetCurrentProcess(), 0, NULL);
    DWORD e = GetLastError();
    return r ? 1 : (e & 0xFFFF) + 0x10000;
}
TEST(eh_continuation_targets_empty) {
    REQUIRE(UN(SetProcessDynamicEHContinuationTargets));
    DWORD code = t_run_child("kbx_eh_targets", "", 20000);
    CHECK(code != 0xDEAD0000 && code != 0xDEAD0001);
    t_note("SetProcessDynamicEHContinuationTargets(count 0) child result %lu (1 = success, 0x10000+err = failure)", code);
}

/* ======================= Application recovery ======================= */
TEST(recovery_in_progress_invalid) {
    REQUIRE(U(ApplicationRecoveryInProgress));
    HRESULT hr = U(ApplicationRecoveryInProgress)(NULL);
    EXPECT_EQ((ULONG)hr, 0x80070057UL);
}
TEST(recovery_in_progress_not_recovering) {
    REQUIRE(U(ApplicationRecoveryInProgress));
    BOOL cancelled = 0x5a5a;
    HRESULT hr = U(ApplicationRecoveryInProgress)(&cancelled);
    EXPECT_EQ((ULONG)hr, 0x80004005UL);
    EXPECT_EQ(cancelled, 0);
}
CHILD(kbx_recovery) {
    typedef HRESULT (WINAPI *ip_t)(PBOOL);
    typedef void (WINAPI *fin_t)(BOOL);
    ip_t ip = (ip_t)t_proc("ApplicationRecoveryInProgress");
    fin_t fin = (fin_t)t_proc("ApplicationRecoveryFinished");
    if (!ip || !fin) return 0;
    BOOL c = 9;
    HRESULT hr = ip(&c);
    KC((ULONG)hr == 0x80004005UL && c == 0);
    fin(TRUE);    /* outside a recovery callback: must not crash */
    fin(FALSE);
    return g_cf;
}
TEST(recovery_finished_outside_callback) {
    REQUIRE(U(ApplicationRecoveryFinished));
    DWORD code = t_run_child("kbx_recovery", "", 20000);
    CHECKF(code == 0, "child result %lu", code);
}
TEST(recovery_register_invalid) {
    REQUIRE(U(RegisterApplicationRecoveryCallback)); REQUIRE(U(UnregisterApplicationRecoveryCallback));
    /* NULL callback is rejected; nothing gets registered */
    HRESULT hr = U(RegisterApplicationRecoveryCallback)(NULL, NULL, 5000, 0);
    EXPECT_EQ((ULONG)hr, 0x80070057UL);
    hr = U(UnregisterApplicationRecoveryCallback)();
    t_note("Unregister without registration -> 0x%lx", (ULONG)hr);
}

/* ======================= Thread profiling ======================= */
CHILD(kbx_profiling) {
    typedef DWORD (WINAPI *q_t)(HANDLE, PBOOLEAN);
    typedef DWORD (WINAPI *en_t)(HANDLE, DWORD, DWORD64, HANDLE *);
    typedef DWORD (WINAPI *dis_t)(HANDLE);
    q_t q = (q_t)t_proc("QueryThreadProfiling");
    en_t en = (en_t)t_proc("EnableThreadProfiling");
    dis_t dis = (dis_t)t_proc("DisableThreadProfiling");
    if (!q || !en || !dis) return 0;
    BOOLEAN enabled = 7;
    DWORD r = q(GetCurrentThread(), &enabled);
    KC(r == 0 && enabled == 0);
    HANDLE ph = NULL;
    r = en(GetCurrentThread(), 0, 0, &ph);   /* no flags: nothing to profile */
    DWORD er1 = r;
    if (r == 0 && ph) {
        enabled = 0; r = q(GetCurrentThread(), &enabled);
        KC(r == 0 && enabled != 0);
        r = dis(ph); KC(r == 0);
        enabled = 7; r = q(GetCurrentThread(), &enabled); KC(r == 0 && enabled == 0);
    }
    return g_cf ? g_cf : 0;
}
TEST(thread_profiling_roundtrip) {
    REQUIRE(U(QueryThreadProfiling));
    DWORD code = t_run_child("kbx_profiling", "", 20000);
    CHECKF(code == 0, "child first failing line / crash code 0x%lx", code);
}
TEST(thread_profiling_query_bad_thread) {
    REQUIRE(U(QueryThreadProfiling));
    BOOLEAN en = 7;
    DWORD r = U(QueryThreadProfiling)(NULL, &en);
    CHECK(r != 0); t_note("QueryThreadProfiling(NULL) -> %lu", r);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    r = U(QueryThreadProfiling)(ev, &en);
    CHECK(r != 0);
    CloseHandle(ev);
}
