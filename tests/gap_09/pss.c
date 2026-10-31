/* gap9: process snapshotting (Pss*) - capture, query, walk, markers, duplicate, free. */
#include "g9.h"

typedef struct { void *Context; void *(WINAPI *Alloc)(void *ctx, DWORD size); void (WINAPI *Free)(void *ctx, void *p); } G9_PSS_ALLOC;
UDECL(DWORD, PssCaptureSnapshot, (HANDLE, DWORD, DWORD, HANDLE *));
UDECL(DWORD, PssFreeSnapshot, (HANDLE, HANDLE));
UDECL(DWORD, PssDuplicateSnapshot, (HANDLE, HANDLE, HANDLE, HANDLE *, DWORD));
UDECL(DWORD, PssQuerySnapshot, (HANDLE, DWORD, void *, DWORD));
UDECL(DWORD, PssWalkSnapshot, (HANDLE, DWORD, HANDLE, void *, DWORD));
UDECL(DWORD, PssWalkMarkerCreate, (const G9_PSS_ALLOC *, HANDLE *));
UDECL(DWORD, PssWalkMarkerFree, (HANDLE));
UDECL(DWORD, PssWalkMarkerGetPosition, (HANDLE, ULONG_PTR *));
UDECL(DWORD, PssWalkMarkerSetPosition, (HANDLE, ULONG_PTR));
UDECL(DWORD, PssWalkMarkerSeekToBeginning, (HANDLE));

enum { CAP_NONE = 0, CAP_VA_CLONE = 1, CAP_HANDLES = 4, CAP_HANDLE_NAME = 8, CAP_HANDLE_BASIC = 0x10,
       CAP_THREADS = 0x80, CAP_VA_SPACE = 0x800, CAP_PERF = 0x40000000 };
enum { Q_PROCESS = 0, Q_VA_CLONE = 1, Q_AUX = 2, Q_VA_SPACE = 3, Q_HANDLE = 4, Q_THREAD = 5, Q_HANDLE_TRACE = 6, Q_PERF = 7 };
enum { W_AUX = 0, W_VA = 1, W_HANDLES = 2, W_THREADS = 3 };

typedef struct {
    DWORD ExitStatus; PVOID PebBaseAddress; ULONG_PTR AffinityMask; LONG BasePriority; DWORD ProcessId, ParentProcessId, Flags;
    FILETIME CreateTime, ExitTime, KernelTime, UserTime; DWORD PriorityClass;
    ULONG_PTR PeakVirtualSize, VirtualSize; DWORD PageFaultCount;
    ULONG_PTR PeakWorkingSetSize, WorkingSetSize, QuotaPeakPagedPoolUsage, QuotaPagedPoolUsage, QuotaPeakNonPagedPoolUsage,
              QuotaNonPagedPoolUsage, PagefileUsage, PeakPagefileUsage, PrivateUsage;
    DWORD ExecuteFlags; WCHAR ImageFileName[MAX_PATH];
} G9_PSS_PROC;
_Static_assert(sizeof(G9_PSS_PROC) == 0x2c0, "PSS_PROCESS_INFORMATION size");
typedef struct { HANDLE Handle; BYTE rest[0x80]; } G9_PSS_HANDLE_ENTRY;
_Static_assert(sizeof(G9_PSS_HANDLE_ENTRY) == 0x88, "handle entry");
typedef struct { DWORD ExitStatus; PVOID Teb; DWORD ProcessId, ThreadId; BYTE rest[0x60]; } G9_PSS_THREAD_ENTRY;
_Static_assert(sizeof(G9_PSS_THREAD_ENTRY) == 0x78, "thread entry");
typedef struct { PVOID BaseAddress, AllocationBase; DWORD AllocationProtect, pad; ULONG_PTR RegionSize; DWORD State, Protect, Type, TimeDateStamp, SizeOfImage, pad2; PVOID ImageBase; DWORD CheckSum; WORD ImageNameLength; WORD pad3; PVOID ImageName; } G9_PSS_VA_ENTRY;
_Static_assert(sizeof(G9_PSS_VA_ENTRY) == 0x50, "va entry");

#define NEED_PSS() do { REQUIRE(UN(PssCaptureSnapshot)); REQUIRE(UN(PssFreeSnapshot)); REQUIRE(UN(PssQuerySnapshot)); \
    REQUIRE(UN(PssWalkSnapshot)); REQUIRE(UN(PssWalkMarkerCreate)); REQUIRE(UN(PssWalkMarkerFree)); } while (0)
#define SELF GetCurrentProcess()

static HANDLE g9_cap(DWORD flags) {
    HANDLE s = NULL;
    DWORD r = UN(PssCaptureSnapshot)(SELF, flags, 0, &s);
    if (r) { t_note("PssCaptureSnapshot(0x%lx) -> %lu", flags, r); return NULL; }
    return s;
}
static void g9_free(HANDLE s) { if (s) G9EQ(UN(PssFreeSnapshot)(SELF, s), 0); }

TEST(gap9_pss_exports_present) {
    CHECK(UN(PssCaptureSnapshot) != NULL); CHECK(UN(PssFreeSnapshot) != NULL); CHECK(UN(PssDuplicateSnapshot) != NULL);
    CHECK(UN(PssQuerySnapshot) != NULL); CHECK(UN(PssWalkSnapshot) != NULL);
    CHECK(UN(PssWalkMarkerCreate) != NULL); CHECK(UN(PssWalkMarkerFree) != NULL);
    CHECK(UN(PssWalkMarkerGetPosition) != NULL); CHECK(UN(PssWalkMarkerSetPosition) != NULL);
    CHECK(UN(PssWalkMarkerSeekToBeginning) != NULL);
    CHECK(t_proc_k32("PssCaptureSnapshot") != NULL);   /* kernel32 exports these on Win10 */
}

TEST(gap9_pss_capture_none) {
    NEED_PSS();
    HANDLE s = (HANDLE)(ULONG_PTR)0x1234;
    DWORD r = UN(PssCaptureSnapshot)(SELF, CAP_NONE, 0, &s);
    G9EQ(r, 0);
    CHECK(s != NULL && s != (HANDLE)(ULONG_PTR)0x1234);
    if (!r) {
        t_note("descriptor signature 0x%08lx", *(DWORD *)s);
        G9EQ(UN(PssFreeSnapshot)(SELF, s), 0);
    }
}

TEST(gap9_pss_capture_invalid_flags) {
    NEED_PSS();
    DWORD bad[] = { 0x8000, 0x10000, 0x20000, 0x100000, 0x1000000, 0x2000000, 0x3ff8000, 0x8000 | CAP_VA_CLONE, 0x2000000 | CAP_HANDLES };
    for (int i = 0; i < 9; i++) {
        HANDLE s = (HANDLE)(ULONG_PTR)0x1234;
        DWORD r = UN(PssCaptureSnapshot)(SELF, bad[i], 0, &s);
        CHECKF(r == ERROR_INVALID_PARAMETER, "flags 0x%lx -> %lu", bad[i], r);
        CHECKF(s == NULL, "flags 0x%lx handle not cleared", bad[i]);
        if (!r) UN(PssFreeSnapshot)(SELF, s);
    }
}

TEST(gap9_pss_capture_breakaway_mix) {
    NEED_PSS();
    HANDLE s = (HANDLE)(ULONG_PTR)0x1234;
    DWORD r = UN(PssCaptureSnapshot)(SELF, 0x04000000, 0, &s);   /* PSS_CREATE_BREAKAWAY_OPTIONAL alone */
    CHECKF(r != 0, "bare breakaway-optional flag accepted");
    t_note("breakaway-optional alone -> %lu", r);
    CHECK(s == NULL);
    if (!r && s) UN(PssFreeSnapshot)(SELF, s);
}

TEST(gap9_pss_capture_invalid_process) {
    NEED_PSS();
    HANDLE s = (HANDLE)(ULONG_PTR)0x1234;
    DWORD r = UN(PssCaptureSnapshot)(NULL, CAP_NONE, 0, &s);
    CHECKF(r != 0, "NULL process handle accepted");
    t_note("NULL process -> %lu", r);
    CHECK(s == NULL);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    s = (HANDLE)(ULONG_PTR)0x1234;
    r = UN(PssCaptureSnapshot)(ev, CAP_NONE, 0, &s);
    CHECKF(r != 0, "event handle accepted as process");
    t_note("event as process -> %lu", r);
    CHECK(s == NULL);
    CloseHandle(ev);
    HANDLE closed = CreateEventA(NULL, TRUE, FALSE, NULL); CloseHandle(closed);
    s = (HANDLE)(ULONG_PTR)0x1234;
    r = UN(PssCaptureSnapshot)(closed, CAP_NONE, 0, &s);
    CHECKF(r != 0, "closed handle accepted");
    CHECK(s == NULL);
}

TEST(gap9_pss_capture_flag_combinations) {
    NEED_PSS();
    DWORD combos[] = { CAP_NONE, CAP_VA_CLONE, CAP_HANDLES, CAP_THREADS, CAP_VA_SPACE, CAP_VA_CLONE | CAP_HANDLES,
        CAP_VA_CLONE | CAP_THREADS, CAP_VA_CLONE | CAP_VA_SPACE, CAP_VA_CLONE | CAP_HANDLES | CAP_HANDLE_NAME | CAP_HANDLE_BASIC | CAP_THREADS | CAP_VA_SPACE,
        CAP_VA_CLONE | CAP_PERF };
    for (int i = 0; i < 10; i++) {
        HANDLE s = NULL;
        DWORD r = UN(PssCaptureSnapshot)(SELF, combos[i], 0, &s);
        CHECKF(r == 0, "flags 0x%lx -> %lu", combos[i], r);
        if (!r) { CHECK(s != NULL); G9EQ(UN(PssFreeSnapshot)(SELF, s), 0); }
    }
}

TEST(gap9_pss_query_process_information) {
    NEED_PSS();
    HANDLE s = g9_cap(CAP_VA_CLONE); if (!s) { t_skip("capture failed"); return; }
    G9_PSS_PROC pi; memset(&pi, 0xcc, sizeof pi);
    G9EQ(UN(PssQuerySnapshot)(s, Q_PROCESS, &pi, sizeof pi), 0);
    G9EQ(pi.ProcessId, GetCurrentProcessId());
    G9EQ(pi.ExitStatus, STILL_ACTIVE);
    CHECK(pi.PebBaseAddress == NtCurrentTeb()->ProcessEnvironmentBlock);
    CHECK(pi.CreateTime.dwHighDateTime != 0);
    CHECK(pi.ExitTime.dwLowDateTime == 0 && pi.ExitTime.dwHighDateTime == 0);
    CHECK(pi.PrivateUsage > 0);
    CHECK(pi.VirtualSize >= pi.WorkingSetSize);
    CHECK(pi.PeakVirtualSize >= pi.VirtualSize);
    CHECK(pi.PeakWorkingSetSize >= pi.WorkingSetSize);
    G9EQ(pi.PriorityClass, GetPriorityClass(SELF));
    CHECK(pi.AffinityMask != 0);
    WCHAR mod[MAX_PATH]; GetModuleFileNameW(NULL, mod, MAX_PATH);
    WCHAR *b1 = wcsrchr(mod, L'\\'), *b2 = wcsrchr(pi.ImageFileName, L'\\');
    CHECK(pi.ImageFileName[0] != 0);
    CHECK(b1 && (b2 ? !lstrcmpiW(b1, b2) : !lstrcmpiW(b1 + 1, pi.ImageFileName)));
    g9_free(s);
}

TEST(gap9_pss_query_bad_length) {
    NEED_PSS();
    HANDLE s = g9_cap(CAP_VA_CLONE | CAP_HANDLES | CAP_THREADS | CAP_VA_SPACE | CAP_PERF); if (!s) { t_skip("capture failed"); return; }
    static BYTE buf[0x1000];
    struct { DWORD cls, good; } t[] = { {Q_PROCESS, 0x2c0}, {Q_VA_CLONE, 8}, {Q_AUX, 4}, {Q_VA_SPACE, 4}, {Q_HANDLE, 4},
                                        {Q_THREAD, 8}, {Q_HANDLE_TRACE, 0x10}, {Q_PERF, 0x60} };
    for (int i = 0; i < 8; i++) {
        DWORD good = t[i].good;
        DWORD lens[] = { 0, 1, good - 1, good + 1, good * 2 };
        for (int k = 0; k < 5; k++) {
            DWORD r = UN(PssQuerySnapshot)(s, t[i].cls, buf, lens[k]);
            CHECKF(r == ERROR_BAD_LENGTH, "class %lu len %lu -> %lu", t[i].cls, lens[k], r);
        }
    }
    g9_free(s);
}

TEST(gap9_pss_query_invalid_class) {
    NEED_PSS();
    HANDLE s = g9_cap(CAP_NONE); if (!s) { t_skip("capture failed"); return; }
    static BYTE buf[0x1000];
    DWORD cls[] = { 11, 12, 16, 100, 0x1000, 0x7fffffff, 0xffffffff };
    for (int i = 0; i < 7; i++) {
        DWORD r = UN(PssQuerySnapshot)(s, cls[i], buf, 4);
        CHECKF(r == ERROR_INVALID_PARAMETER, "class %lu -> %lu", cls[i], r);
    }
    g9_free(s);
}

TEST(gap9_pss_query_null_snapshot) {
    NEED_PSS();
    static BYTE buf[0x300];
    SET_ERR(0);
    G9EQ(UN(PssQuerySnapshot)(NULL, Q_PROCESS, buf, 0x2c0), ERROR_INVALID_HANDLE);
    G9EQ(UN(PssQuerySnapshot)(NULL, Q_THREAD, buf, 8), ERROR_INVALID_HANDLE);
    G9EQ(UN(PssQuerySnapshot)(NULL, 99, buf, 8), ERROR_INVALID_HANDLE);   /* handle validated first */
    G9EQ(UN(PssFreeSnapshot)(SELF, NULL), ERROR_INVALID_HANDLE);
}

TEST(gap9_pss_query_thread_and_handle_counts) {
    NEED_PSS();
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE s = g9_cap(CAP_VA_CLONE | CAP_HANDLES | CAP_THREADS | CAP_VA_SPACE); if (!s) { CloseHandle(ev); t_skip("capture failed"); return; }
    DWORD th[2] = {0}, hc = 0, vc = 0, aux = 0xdead;
    G9EQ(UN(PssQuerySnapshot)(s, Q_THREAD, th, 8), 0);
    CHECK(th[0] >= 1);
    G9EQ(th[1], 0);   /* no context requested */
    G9EQ(UN(PssQuerySnapshot)(s, Q_HANDLE, &hc, 4), 0);
    CHECK(hc >= 3);
    G9EQ(UN(PssQuerySnapshot)(s, Q_VA_SPACE, &vc, 4), 0);
    CHECK(vc >= 10);
    G9EQ(UN(PssQuerySnapshot)(s, Q_AUX, &aux, 4), 0);
    G9EQ(aux, 0);
    g9_free(s); CloseHandle(ev);
}

TEST(gap9_pss_query_va_clone_handle) {
    NEED_PSS();
    HANDLE s = g9_cap(CAP_VA_CLONE); if (!s) { t_skip("capture failed"); return; }
    HANDLE clone = NULL;
    G9EQ(UN(PssQuerySnapshot)(s, Q_VA_CLONE, &clone, 8), 0);
    CHECK(clone != NULL);
    if (clone) {
        DWORD pid = GetProcessId(clone);
        CHECKF(pid != 0 && pid != GetCurrentProcessId(), "clone pid %lu", pid);
    }
    g9_free(s);
    HANDLE s2 = g9_cap(CAP_NONE);
    if (s2) {
        HANDLE c2 = (HANDLE)(ULONG_PTR)0x99; DWORD r = UN(PssQuerySnapshot)(s2, Q_VA_CLONE, &c2, 8);
        t_note("VA clone query without clone flag -> %lu handle %p", r, c2);
        g9_free(s2);
    }
}

TEST(gap9_pss_query_perf_counters) {
    NEED_PSS();
    HANDLE s = g9_cap(CAP_VA_CLONE | CAP_PERF); if (!s) { t_skip("capture failed"); return; }
    ULONGLONG pc[12]; memset(pc, 0, sizeof pc);
    DWORD r = UN(PssQuerySnapshot)(s, Q_PERF, pc, sizeof pc);
    G9EQ(r, 0);
    t_note("perf counters: %llu %llu %llu %llu", pc[0], pc[1], pc[2], pc[3]);
    g9_free(s);
}

TEST(gap9_pss_walk_threads) {
    NEED_PSS();
    HANDLE s = g9_cap(CAP_VA_CLONE | CAP_THREADS); if (!s) { t_skip("capture failed"); return; }
    DWORD th[2] = {0}; UN(PssQuerySnapshot)(s, Q_THREAD, th, 8);
    HANDLE m = NULL; G9EQ(UN(PssWalkMarkerCreate)(NULL, &m), 0);
    G9_PSS_THREAD_ENTRY e; DWORD n = 0, r, self = 0;
    while ((r = UN(PssWalkSnapshot)(s, W_THREADS, m, &e, sizeof e)) == 0 && n < 5000) {
        n++;
        if (e.ThreadId == GetCurrentThreadId()) self++;
        G9EQ(e.ProcessId, GetCurrentProcessId());
        CHECK(e.Teb != NULL);
    }
    G9EQ(r, ERROR_NO_MORE_ITEMS);
    G9EQ(n, th[0]);
    G9EQ(self, 1);
    /* the walk stays at the end */
    G9EQ(UN(PssWalkSnapshot)(s, W_THREADS, m, &e, sizeof e), ERROR_NO_MORE_ITEMS);
    /* rewinding gives the same count again */
    G9EQ(UN(PssWalkMarkerSeekToBeginning)(m), 0);
    DWORD n2 = 0; while (UN(PssWalkSnapshot)(s, W_THREADS, m, &e, sizeof e) == 0 && n2 < 5000) n2++;
    G9EQ(n2, n);
    G9EQ(UN(PssWalkMarkerFree)(m), 0);
    g9_free(s);
}

TEST(gap9_pss_walk_handles_contains_own_event) {
    NEED_PSS();
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE s = g9_cap(CAP_VA_CLONE | CAP_HANDLES); if (!s) { CloseHandle(ev); t_skip("capture failed"); return; }
    DWORD hc = 0; UN(PssQuerySnapshot)(s, Q_HANDLE, &hc, 4);
    HANDLE m = NULL; G9EQ(UN(PssWalkMarkerCreate)(NULL, &m), 0);
    G9_PSS_HANDLE_ENTRY e; DWORD n = 0, r, found = 0;
    while ((r = UN(PssWalkSnapshot)(s, W_HANDLES, m, &e, sizeof e)) == 0 && n < 20000) {
        n++;
        CHECK(e.Handle != NULL);
        if (e.Handle == ev) found++;
    }
    G9EQ(r, ERROR_NO_MORE_ITEMS);
    G9EQ(n, hc);
    G9EQ(found, 1);
    UN(PssWalkMarkerFree)(m);
    g9_free(s); CloseHandle(ev);
}

TEST(gap9_pss_walk_va_space) {
    NEED_PSS();
    HANDLE s = g9_cap(CAP_VA_CLONE | CAP_VA_SPACE); if (!s) { t_skip("capture failed"); return; }
    DWORD rc = 0; UN(PssQuerySnapshot)(s, Q_VA_SPACE, &rc, 4);
    HANDLE m = NULL; G9EQ(UN(PssWalkMarkerCreate)(NULL, &m), 0);
    G9_PSS_VA_ENTRY e; DWORD n = 0, r, hit = 0; ULONG_PTR prev = 0;
    ULONG_PTR here = (ULONG_PTR)(void *)&g9_cap;
    while ((r = UN(PssWalkSnapshot)(s, W_VA, m, &e, sizeof e)) == 0 && n < 100000) {
        n++;
        CHECK(((ULONG_PTR)e.BaseAddress & 0xfff) == 0);
        CHECK(e.RegionSize != 0 && (e.RegionSize & 0xfff) == 0);
        CHECK((ULONG_PTR)e.BaseAddress >= prev);
        prev = (ULONG_PTR)e.BaseAddress + e.RegionSize;
        if (here >= (ULONG_PTR)e.BaseAddress && here < prev) {
            hit++;
            G9EQ(e.State, MEM_COMMIT);
            G9EQ(e.Type, MEM_IMAGE);
        }
    }
    G9EQ(r, ERROR_NO_MORE_ITEMS);
    G9EQ(n, rc);
    G9EQ(hit, 1);
    UN(PssWalkMarkerFree)(m);
    g9_free(s);
}

TEST(gap9_pss_walk_bad_length_and_class) {
    NEED_PSS();
    HANDLE s = g9_cap(CAP_VA_CLONE | CAP_HANDLES | CAP_THREADS | CAP_VA_SPACE); if (!s) { t_skip("capture failed"); return; }
    HANDLE m = NULL; UN(PssWalkMarkerCreate)(NULL, &m);
    static BYTE buf[0x200];
    struct { DWORD cls, good; } t[] = { {W_VA, 0x50}, {W_HANDLES, 0x88}, {W_THREADS, 0x78} };
    for (int i = 0; i < 3; i++) {
        DWORD lens[] = { 0, 1, t[i].good - 1, t[i].good + 1, t[i].good * 2 };
        for (int k = 0; k < 5; k++) {
            DWORD r = UN(PssWalkSnapshot)(s, t[i].cls, m, buf, lens[k]);
            CHECKF(r == ERROR_BAD_LENGTH, "class %lu len %lu -> %lu", t[i].cls, lens[k], r);
        }
    }
    DWORD cls[] = { 4, 5, 100, 0xffffffff };
    for (int i = 0; i < 4; i++) {
        DWORD r = UN(PssWalkSnapshot)(s, cls[i], m, buf, 0x50);
        CHECKF(r == ERROR_INVALID_PARAMETER, "walk class %lu -> %lu", cls[i], r);
    }
    UN(PssWalkMarkerFree)(m);
    g9_free(s);
}

TEST(gap9_pss_walk_null_snapshot) {
    NEED_PSS();
    HANDLE m = NULL; UN(PssWalkMarkerCreate)(NULL, &m);
    static BYTE buf[0x100];
    G9EQ(UN(PssWalkSnapshot)(NULL, W_THREADS, m, buf, 0x78), ERROR_INVALID_HANDLE);
    G9EQ(UN(PssWalkSnapshot)(NULL, 99, m, buf, 0), ERROR_INVALID_HANDLE);
    UN(PssWalkMarkerFree)(m);
}

TEST(gap9_pss_two_markers_independent) {
    NEED_PSS();
    HANDLE s = g9_cap(CAP_VA_CLONE | CAP_VA_SPACE); if (!s) { t_skip("capture failed"); return; }
    HANDLE a = NULL, b = NULL; UN(PssWalkMarkerCreate)(NULL, &a); UN(PssWalkMarkerCreate)(NULL, &b);
    G9_PSS_VA_ENTRY e1, e2, e3;
    G9EQ(UN(PssWalkSnapshot)(s, W_VA, a, &e1, sizeof e1), 0);
    G9EQ(UN(PssWalkSnapshot)(s, W_VA, a, &e2, sizeof e2), 0);
    G9EQ(UN(PssWalkSnapshot)(s, W_VA, b, &e3, sizeof e3), 0);
    CHECK(e1.BaseAddress == e3.BaseAddress);
    CHECK(e2.BaseAddress != e1.BaseAddress);
    UN(PssWalkMarkerFree)(a); UN(PssWalkMarkerFree)(b);
    g9_free(s);
}

TEST(gap9_pss_marker_position_roundtrip) {
    NEED_PSS(); REQUIRE(UN(PssWalkMarkerGetPosition)); REQUIRE(UN(PssWalkMarkerSetPosition)); REQUIRE(UN(PssWalkMarkerSeekToBeginning));
    HANDLE m = (HANDLE)(ULONG_PTR)0x1;
    G9EQ(UN(PssWalkMarkerCreate)(NULL, &m), 0);
    CHECK(m != NULL && m != (HANDLE)(ULONG_PTR)1);
    ULONG_PTR pos = 0xdead;
    G9EQ(UN(PssWalkMarkerGetPosition)(m, &pos), 0);
    G9EQ(pos, 0);
    ULONG_PTR vals[] = { 1, 2, 7, 0x1000, 0xffffffffu, (ULONG_PTR)-1 };
    for (int i = 0; i < 6; i++) {
        G9EQ(UN(PssWalkMarkerSetPosition)(m, vals[i]), 0);
        pos = 0; G9EQ(UN(PssWalkMarkerGetPosition)(m, &pos), 0);
        G9EQ(pos, vals[i]);
    }
    G9EQ(UN(PssWalkMarkerSeekToBeginning)(m), 0);
    pos = 0xdead; G9EQ(UN(PssWalkMarkerGetPosition)(m, &pos), 0);
    G9EQ(pos, 0);
    G9EQ(UN(PssWalkMarkerFree)(m), 0);
}

TEST(gap9_pss_marker_position_tracks_walk) {
    NEED_PSS(); REQUIRE(UN(PssWalkMarkerGetPosition)); REQUIRE(UN(PssWalkMarkerSetPosition));
    HANDLE s = g9_cap(CAP_VA_CLONE | CAP_VA_SPACE); if (!s) { t_skip("capture failed"); return; }
    HANDLE m = NULL; UN(PssWalkMarkerCreate)(NULL, &m);
    G9_PSS_VA_ENTRY e, first, second;
    ULONG_PTR p0 = 0, p1 = 0, p2 = 0;
    UN(PssWalkMarkerGetPosition)(m, &p0);
    UN(PssWalkSnapshot)(s, W_VA, m, &first, sizeof e);
    UN(PssWalkMarkerGetPosition)(m, &p1);
    UN(PssWalkSnapshot)(s, W_VA, m, &second, sizeof e);
    UN(PssWalkMarkerGetPosition)(m, &p2);
    CHECK(p1 != p0 && p2 != p1);
    /* restoring a saved position replays the walk from there */
    G9EQ(UN(PssWalkMarkerSetPosition)(m, p1), 0);
    G9EQ(UN(PssWalkSnapshot)(s, W_VA, m, &e, sizeof e), 0);
    CHECK(e.BaseAddress == second.BaseAddress && e.RegionSize == second.RegionSize);
    G9EQ(UN(PssWalkMarkerSetPosition)(m, p0), 0);
    G9EQ(UN(PssWalkSnapshot)(s, W_VA, m, &e, sizeof e), 0);
    CHECK(e.BaseAddress == first.BaseAddress);
    UN(PssWalkMarkerFree)(m);
    g9_free(s);
}

TEST(gap9_pss_marker_null_handle) {
    NEED_PSS(); REQUIRE(UN(PssWalkMarkerGetPosition)); REQUIRE(UN(PssWalkMarkerSetPosition)); REQUIRE(UN(PssWalkMarkerSeekToBeginning));
    ULONG_PTR pos = 0x55;
    G9EQ(UN(PssWalkMarkerFree)(NULL), ERROR_INVALID_HANDLE);
    G9EQ(UN(PssWalkMarkerGetPosition)(NULL, &pos), ERROR_INVALID_HANDLE);
    G9EQ(pos, 0x55);
    G9EQ(UN(PssWalkMarkerSetPosition)(NULL, 1), ERROR_INVALID_HANDLE);
    G9EQ(UN(PssWalkMarkerSeekToBeginning)(NULL), ERROR_INVALID_HANDLE);
}

static int g_ma_alloc, g_ma_free, g_ma_size, g_ma_fail; static void *g_ma_last; static void *g_ma_ctx_a, *g_ma_ctx_f;
static void *WINAPI ma_alloc(void *ctx, DWORD sz) { g_ma_alloc++; g_ma_size = (int)sz; g_ma_ctx_a = ctx; if (g_ma_fail) return NULL; return g_ma_last = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sz); }
static void WINAPI ma_free(void *ctx, void *p) { g_ma_free++; g_ma_ctx_f = ctx; if (p == g_ma_last) g_ma_last = NULL; HeapFree(GetProcessHeap(), 0, p); }

TEST(gap9_pss_marker_custom_allocator) {
    NEED_PSS();
    static int ctxv; G9_PSS_ALLOC al = { &ctxv, ma_alloc, ma_free };
    g_ma_alloc = g_ma_free = g_ma_fail = 0; g_ma_last = NULL;
    HANDLE m = NULL;
    G9EQ(UN(PssWalkMarkerCreate)(&al, &m), 0);
    G9EQ(g_ma_alloc, 1);
    CHECK(g_ma_ctx_a == &ctxv);
    CHECK(m == g_ma_last);
    CHECK(g_ma_size >= 0x20);
    G9EQ(UN(PssWalkMarkerFree)(m), 0);
    G9EQ(g_ma_free, 1);
    CHECK(g_ma_ctx_f == &ctxv);
    /* allocation failure */
    g_ma_fail = 1; m = (HANDLE)(ULONG_PTR)0x77;
    G9EQ(UN(PssWalkMarkerCreate)(&al, &m), ERROR_NOT_ENOUGH_MEMORY);
    CHECK(m == NULL);
    G9EQ(g_ma_alloc, 2);
}

TEST(gap9_pss_marker_many_create_free) {
    NEED_PSS();
    for (int i = 0; i < 1000; i++) {
        HANDLE m = NULL;
        if (UN(PssWalkMarkerCreate)(NULL, &m) || !m) { CHECKF(0, "create %d failed", i); break; }
        if (UN(PssWalkMarkerFree)(m)) { CHECKF(0, "free %d failed", i); break; }
    }
}

TEST(gap9_pss_duplicate_snapshot_same_process) {
    NEED_PSS(); REQUIRE(UN(PssDuplicateSnapshot));
    HANDLE s = g9_cap(CAP_VA_CLONE | CAP_THREADS); if (!s) { t_skip("capture failed"); return; }
    HANDLE d = NULL;
    DWORD r = UN(PssDuplicateSnapshot)(SELF, s, SELF, &d, 0);
    t_note("PssDuplicateSnapshot -> %lu dup=%p src=%p", r, d, s);
    if (!r) {
        CHECK(d != NULL);
        DWORD th[2] = {0}, th2[2] = {0};
        G9EQ(UN(PssQuerySnapshot)(d, Q_THREAD, th2, 8), 0);
        UN(PssQuerySnapshot)(s, Q_THREAD, th, 8);
        G9EQ(th[0], th2[0]);
        g9_free(d);
    }
    g9_free(s);
}

TEST(gap9_pss_duplicate_snapshot_invalid) {
    NEED_PSS(); REQUIRE(UN(PssDuplicateSnapshot));
    HANDLE d = (HANDLE)(ULONG_PTR)0x1234;
    DWORD r = UN(PssDuplicateSnapshot)(SELF, NULL, SELF, &d, 0);
    CHECKF(r == ERROR_INVALID_HANDLE, "NULL snapshot -> %lu", r);
    HANDLE s = g9_cap(CAP_NONE); if (!s) return;
    d = (HANDLE)(ULONG_PTR)0x1234;
    r = UN(PssDuplicateSnapshot)(SELF, s, NULL, &d, 0);
    CHECKF(r != 0, "NULL target process accepted");
    t_note("NULL target process -> %lu", r);
    r = UN(PssDuplicateSnapshot)(SELF, s, SELF, &d, 0x80);
    t_note("bad dup flags -> %lu", r);
    if (!r && d != s) g9_free(d);
    g9_free(s);
}

CHILD(g9_pss_sleeper) { Sleep(25000); return 0; }

TEST(gap9_pss_capture_other_process) {
    NEED_PSS();
    PROCESS_INFORMATION pi; HANDLE hp = t_spawn_child("g9_pss_sleeper", "", 0, NULL, NULL, NULL, FALSE, NULL, &pi);
    if (!hp) { t_skip("spawn failed"); return; }
    Sleep(300);
    HANDLE s = NULL;
    DWORD r = UN(PssCaptureSnapshot)(hp, CAP_VA_CLONE | CAP_THREADS | CAP_HANDLES, 0, &s);
    t_note("capture of child -> %lu", r);
    if (r == 0) {
        CHECK(s != NULL);
        G9_PSS_PROC info; memset(&info, 0, sizeof info);
        G9EQ(UN(PssQuerySnapshot)(s, Q_PROCESS, &info, sizeof info), 0);
        G9EQ(info.ProcessId, pi.dwProcessId);
        G9EQ(info.ExitStatus, STILL_ACTIVE);
        DWORD th[2] = {0}; G9EQ(UN(PssQuerySnapshot)(s, Q_THREAD, th, 8), 0); CHECK(th[0] >= 1);
        HANDLE m = NULL; UN(PssWalkMarkerCreate)(NULL, &m);
        G9_PSS_THREAD_ENTRY e; DWORD found = 0, n = 0;
        while (UN(PssWalkSnapshot)(s, W_THREADS, m, &e, sizeof e) == 0 && n++ < 1000) if (e.ThreadId == pi.dwThreadId) found++;
        G9EQ(found, 1);
        UN(PssWalkMarkerFree)(m);
        r = UN(PssFreeSnapshot)(hp, s);
        t_note("remote free -> %lu", r);
        G9EQ(r, 0);
    } else CHECK(0);
    TerminateProcess(hp, 0); WaitForSingleObject(hp, 5000);
    CloseHandle(pi.hThread); CloseHandle(hp);
}

TEST(gap9_pss_repeated_capture_free) {
    NEED_PSS();
    for (int i = 0; i < 30; i++) {
        HANDLE s = NULL;
        DWORD r = UN(PssCaptureSnapshot)(SELF, (i & 1) ? CAP_VA_CLONE : CAP_NONE, 0, &s);
        if (r) { CHECKF(0, "iter %d -> %lu", i, r); break; }
        if (UN(PssFreeSnapshot)(SELF, s)) { CHECKF(0, "iter %d free failed", i); break; }
    }
}

TEST(gap9_pss_two_snapshots_distinct) {
    NEED_PSS();
    HANDLE a = g9_cap(CAP_NONE), b = g9_cap(CAP_NONE);
    CHECK(a != b);
    g9_free(a); g9_free(b);
}
