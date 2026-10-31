#include "hlg.h"

typedef struct { unsigned char *p; SIZE_T n; unsigned char seed; } slot;

/* Deterministic LCG-driven alloc/free/realloc with fill-pattern verification. Live bytes capped. */
static int run_stress(HANDLE h, unsigned long long seed, int ops, DWORD flags, int use_realloc, SIZE_T maxsz) {
    enum { SLOTS = 1024 };
    static slot s[SLOTS];
    memset(s, 0, sizeof s);
    hlg_rng r; hlg_seed(&r, seed);
    SIZE_T live = 0, cap = 48u << 20;
    int bad = 0;
    for (int it = 0; it < ops; it++) {
        int i = hlg_next(&r) % SLOTS;
        unsigned op = hlg_next(&r) % 8;
        SIZE_T n = 1 + hlg_next(&r) % maxsz;
        if ((hlg_next(&r) % 50) == 0) n = 70000 + hlg_next(&r) % 200000;       /* occasional big block */
        if (!s[i].p) {
            if (live + n > cap) continue;
            s[i].p = U(HeapAlloc)(h, flags, n);
            if (!s[i].p) { bad++; continue; }
            if ((ULONG_PTR)s[i].p & 0xF) { bad++; t_note("align it=%d", it); }
            if ((flags & HEAP_ZERO_MEMORY) && !hlg_all(s[i].p, n, 0)) bad++;
            s[i].n = n; s[i].seed = (unsigned char)it;
            hlg_fill(s[i].p, n, s[i].seed);
            live += n;
        } else if (op < 4) {
            if (!hlg_verify(s[i].p, s[i].n, s[i].seed)) bad++;
            if (U(HeapSize)(h, 0, s[i].p) != s[i].n) { bad++; t_note("size mismatch it=%d got=%zu want=%zu", it, (size_t)U(HeapSize)(h,0,s[i].p), s[i].n); }
            if (!U(HeapFree)(h, 0, s[i].p)) { bad++; t_note("free fail it=%d", it); }
            live -= s[i].n; s[i].p = NULL;
        } else if (op < 6 && use_realloc) {
            if (live + n > cap) continue;
            SIZE_T keep = n < s[i].n ? n : s[i].n;
            unsigned char *q = U(HeapReAlloc)(h, flags, s[i].p, n);
            if (!q) { bad++; continue; }
            if (!hlg_verify(q, keep, s[i].seed)) { bad++; t_note("realloc verify it=%d old=%zu new=%zu", it, s[i].n, n); }
            if ((flags & HEAP_ZERO_MEMORY) && n > s[i].n && !hlg_all(q + s[i].n, n - s[i].n, 0)) bad++;
            live = live - s[i].n + n;
            s[i].p = q; s[i].n = n;
            hlg_fill(q, n, s[i].seed);
        } else {
            if (!hlg_verify(s[i].p, s[i].n, s[i].seed)) bad++;
        }
        if ((it % 5000) == 4999 && !U(HeapValidate)(h, 0, NULL)) bad++;
    }
    for (int i = 0; i < SLOTS; i++) if (s[i].p) {
        if (!hlg_verify(s[i].p, s[i].n, s[i].seed)) bad++;
        if (!U(HeapFree)(h, 0, s[i].p)) bad++;
    }
    if (!U(HeapValidate)(h, 0, NULL)) bad++;
    return bad;
}

TEST(heapstress_private_small) {
    REQUIRE(U(HeapAlloc)); REQUIRE(U(HeapReAlloc));
    HANDLE h = U(HeapCreate)(0, 0, 0);
    if (!h) { CHECK(h); return; }
    CHECK(run_stress(h, 1, 20000, 0, 0, 512) == 0);
    U(HeapDestroy)(h);
}
TEST(heapstress_private_medium_realloc) {
    REQUIRE(U(HeapAlloc)); REQUIRE(U(HeapReAlloc));
    HANDLE h = U(HeapCreate)(0, 0, 0);
    if (!h) { CHECK(h); return; }
    CHECK(run_stress(h, 2, 20000, 0, 1, 4096) == 0);
    U(HeapDestroy)(h);
}
TEST(heapstress_private_zero_memory) {
    REQUIRE(U(HeapAlloc)); REQUIRE(U(HeapReAlloc));
    HANDLE h = U(HeapCreate)(0, 0, 0);
    if (!h) { CHECK(h); return; }
    CHECK(run_stress(h, 3, 20000, HEAP_ZERO_MEMORY, 1, 2048) == 0);
    U(HeapDestroy)(h);
}
TEST(heapstress_private_noserialize) {
    REQUIRE(U(HeapAlloc)); REQUIRE(U(HeapReAlloc));
    HANDLE h = U(HeapCreate)(HEAP_NO_SERIALIZE, 0, 0);
    if (!h) { CHECK(h); return; }
    CHECK(run_stress(h, 4, 20000, 0, 1, 1024) == 0);
    U(HeapDestroy)(h);
}
TEST(heapstress_process_heap) {
    REQUIRE(U(HeapAlloc)); REQUIRE(U(HeapReAlloc));
    CHECK(run_stress(GetProcessHeap(), 5, 20000, 0, 1, 1500) == 0);
}
TEST(heapstress_lfh_enabled) {
    REQUIRE(U(HeapAlloc)); REQUIRE(U(HeapSetInformation));
    HANDLE h = U(HeapCreate)(0, 0, 0);
    if (!h) { CHECK(h); return; }
    ULONG v = 2;
    U(HeapSetInformation)(h, (HEAP_INFORMATION_CLASS)0, &v, sizeof v);
    CHECK(run_stress(h, 6, 20000, 0, 1, 256) == 0);
    U(HeapDestroy)(h);
}
TEST(heapstress_large_only) {
    REQUIRE(U(HeapAlloc));
    HANDLE h = U(HeapCreate)(0, 0, 0);
    if (!h) { CHECK(h); return; }
    hlg_rng r; hlg_seed(&r, 7);
    unsigned char *p[40]; SIZE_T n[40];
    SIZE_T total = 0;
    for (int i = 0; i < 40; i++) {
        n[i] = 100000 + hlg_next(&r) % 900000;
        p[i] = U(HeapAlloc)(h, 0, n[i]);
        CHECK(p[i] != NULL);
        if (p[i]) { total += n[i]; hlg_fill(p[i], n[i], (unsigned char)i); }
    }
    CHECK(total < (64u << 20));
    for (int i = 0; i < 40; i++) if (p[i]) CHECK(hlg_verify(p[i], n[i], (unsigned char)i));
    for (int i = 0; i < 40; i += 2) if (p[i]) CHECK(U(HeapFree)(h, 0, p[i]));
    CHECK(U(HeapValidate)(h, 0, NULL));
    for (int i = 1; i < 40; i += 2) if (p[i]) CHECK(U(HeapFree)(h, 0, p[i]));
    U(HeapCompact)(h, 0);
    CHECK(U(HeapValidate)(h, 0, NULL));
    U(HeapDestroy)(h);
}
TEST(heapstress_fixed_heap_churn) {
    REQUIRE(U(HeapAlloc));
    HANDLE h = U(HeapCreate)(0, 0x10000, 0x100000);
    if (!h) { CHECK(h); return; }
    hlg_rng r; hlg_seed(&r, 8);
    void *p[64]; memset(p, 0, sizeof p);
    int bad = 0;
    for (int it = 0; it < 20000; it++) {
        int i = hlg_next(&r) % 64;
        if (p[i]) { if (!U(HeapFree)(h, 0, p[i])) bad++; p[i] = NULL; }
        else {
            p[i] = U(HeapAlloc)(h, 0, 1 + hlg_next(&r) % 6000);
            /* NULL is legitimate when the fixed heap is full, but nothing may be corrupted */
        }
    }
    for (int i = 0; i < 64; i++) if (p[i]) U(HeapFree)(h, 0, p[i]);
    CHECK(bad == 0);
    CHECK(U(HeapValidate)(h, 0, NULL));
    U(HeapDestroy)(h);
}
/* run_stress uses a static slot array, so threads get their own simpler loop */
static DWORD WINAPI own_heap_worker(LPVOID a) {
    HANDLE h = U(HeapCreate)(0, 0, 0);
    if (!h) return 1000;
    hlg_rng r; hlg_seed(&r, (unsigned long long)(ULONG_PTR)a + 100);
    unsigned char *p[128]; SIZE_T n[128]; memset(p, 0, sizeof p);
    int bad = 0;
    for (int it = 0; it < 5000; it++) {
        int i = hlg_next(&r) % 128;
        if (p[i]) {
            if (!hlg_verify(p[i], n[i], (unsigned char)i)) bad++;
            if (!U(HeapFree)(h, 0, p[i])) bad++;
            p[i] = NULL;
        } else {
            n[i] = 1 + hlg_next(&r) % 3000;
            p[i] = U(HeapAlloc)(h, 0, n[i]);
            if (!p[i]) bad++; else hlg_fill(p[i], n[i], (unsigned char)i);
        }
    }
    for (int i = 0; i < 128; i++) if (p[i] && !U(HeapFree)(h, 0, p[i])) bad++;
    if (!U(HeapValidate)(h, 0, NULL)) bad++;
    if (!U(HeapDestroy)(h)) bad++;
    return bad;
}
TEST(heapstress_threads_own_heaps) {
    REQUIRE(U(HeapAlloc));
    HANDLE t[8]; int n = 0;
    for (int i = 0; i < 8; i++) { t[n] = CreateThread(NULL, 0, own_heap_worker, (LPVOID)(ULONG_PTR)i, 0, NULL); if (t[n]) n++; }
    CHECK(n == 8);
    if (n) WaitForMultipleObjects(n, t, TRUE, 30000);
    for (int i = 0; i < n; i++) { DWORD ec = 99; GetExitCodeThread(t[i], &ec); CHECK(ec == 0); CloseHandle(t[i]); }
}
static DWORD WINAPI shared_heap_worker(LPVOID a) {
    HANDLE h = (HANDLE)a;
    hlg_rng r; hlg_seed(&r, (unsigned long long)GetCurrentThreadId());
    int bad = 0;
    for (int it = 0; it < 4000; it++) {
        SIZE_T n = 1 + hlg_next(&r) % 5000;
        unsigned char *p = U(HeapAlloc)(h, 0, n);
        if (!p) { bad++; continue; }
        hlg_fill(p, n, (unsigned char)it);
        if (it % 3 == 0) { unsigned char *q = U(HeapReAlloc)(h, 0, p, n * 2); if (!q) { bad++; U(HeapFree)(h, 0, p); continue; } if (!hlg_verify(q, n, (unsigned char)it)) bad++; p = q; }
        if (!U(HeapFree)(h, 0, p)) bad++;
    }
    return bad;
}
TEST(heapstress_threads_shared_heap) {
    REQUIRE(U(HeapAlloc));
    HANDLE h = U(HeapCreate)(0, 0, 0);
    if (!h) { CHECK(h); return; }
    HANDLE t[8]; int n = 0;
    for (int i = 0; i < 8; i++) { t[n] = CreateThread(NULL, 0, shared_heap_worker, h, 0, NULL); if (t[n]) n++; }
    CHECK(n == 8);
    if (n) WaitForMultipleObjects(n, t, TRUE, 30000);
    for (int i = 0; i < n; i++) { DWORD ec = 99; GetExitCodeThread(t[i], &ec); CHECK(ec == 0); CloseHandle(t[i]); }
    CHECK(U(HeapValidate)(h, 0, NULL));
    CHECK(U(HeapDestroy)(h));
}
TEST(heapstress_threads_process_heap) {
    REQUIRE(U(HeapAlloc));
    HANDLE t[8]; int n = 0;
    for (int i = 0; i < 8; i++) { t[n] = CreateThread(NULL, 0, shared_heap_worker, GetProcessHeap(), 0, NULL); if (t[n]) n++; }
    CHECK(n == 8);
    if (n) WaitForMultipleObjects(n, t, TRUE, 30000);
    for (int i = 0; i < n; i++) { DWORD ec = 99; GetExitCodeThread(t[i], &ec); CHECK(ec == 0); CloseHandle(t[i]); }
    CHECK(U(HeapValidate)(GetProcessHeap(), 0, NULL));
}
TEST(heapstress_destroy_with_outstanding_blocks) {
    REQUIRE(U(HeapAlloc));
    for (int round = 0; round < 20; round++) {
        HANDLE h = U(HeapCreate)(0, 0, 0);
        if (!h) { CHECK(h); return; }
        for (int i = 0; i < 200; i++) U(HeapAlloc)(h, 0, 10 + i * 5);
        U(HeapAlloc)(h, 0, 500000);
        CHECK(U(HeapDestroy)(h));
    }
}
