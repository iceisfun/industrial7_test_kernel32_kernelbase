/* Shared helpers for the threadpool_timers area. All helpers are static inline.
 * Functions under test are only called through U()/UK()/UB() pointers; helpers below use only
 * scaffolding APIs (events, threads, tick count). */
#ifndef TP_H
#define TP_H
#include "t.h"

#define TP_WAIT_MS 5000

UDECL(HANDLE, RegisterWaitForSingleObjectEx, (HANDLE, WAITORTIMERCALLBACK, PVOID, ULONG, ULONG));

static inline HANDLE tp_ev(BOOL manual) { return CreateEventA(NULL, manual, FALSE, NULL); }
static inline HANDLE tp_sem(LONG init, LONG max) { return CreateSemaphoreA(NULL, init, max, NULL); }
static inline BOOL tp_signaled(HANDLE h, DWORD ms) { return WaitForSingleObject(h, ms) == WAIT_OBJECT_0; }
static inline BOOL tp_not_signaled(HANDLE h, DWORD ms) { return WaitForSingleObject(h, ms) == WAIT_TIMEOUT; }
static inline ULONGLONG tp_now(void) { return GetTickCount64(); }

/* Poll an interlocked counter until it reaches `want` (>=) or ms elapse. */
static inline BOOL tp_wait_count(volatile LONG *p, LONG want, DWORD ms) {
    ULONGLONG end = GetTickCount64() + ms;
    while (*p < want) { if (GetTickCount64() >= end) return FALSE; Sleep(1); }
    return TRUE;
}

/* A negative FILETIME relative interval of `ms` milliseconds. */
static inline void tp_rel_ft(FILETIME *ft, LONG ms) {
    LONGLONG v = -(LONGLONG)ms * 10000LL;
    ft->dwLowDateTime = (DWORD)(v & 0xffffffff);
    ft->dwHighDateTime = (DWORD)((ULONGLONG)v >> 32);
}
/* Absolute FILETIME `ms` from now (ms may be negative). */
static inline void tp_abs_ft(FILETIME *ft, LONG ms) {
    ULARGE_INTEGER u; GetSystemTimeAsFileTime(ft);
    u.LowPart = ft->dwLowDateTime; u.HighPart = ft->dwHighDateTime;
    u.QuadPart += (LONGLONG)ms * 10000LL;
    ft->dwLowDateTime = u.LowPart; ft->dwHighDateTime = u.HighPart;
}

/* A helper thread that sleeps `ms` then sets an event (used to release gates while the main
 * thread is blocked inside the call under test). */
typedef struct { HANDLE ev; DWORD ms; } tp_delay_arg;
static inline DWORD WINAPI tp_delay_thread(LPVOID p) {
    tp_delay_arg *a = (tp_delay_arg *)p; Sleep(a->ms); SetEvent(a->ev); return 0;
}
static inline HANDLE tp_delayed_set(tp_delay_arg *a, HANDLE ev, DWORD ms) {
    a->ev = ev; a->ms = ms; return CreateThread(NULL, 0, tp_delay_thread, a, 0, NULL);
}
static inline void tp_join(HANDLE th) { if (th) { WaitForSingleObject(th, 10000); CloseHandle(th); } }

/* Child-process exception probe: runs case `c` in "tp_exc" child. The child's exit code is the
 * exception code that terminated it, or 0x7001 when the case returned normally. */
static inline DWORD tp_exc_run(const char *c) { return t_run_child("tp_exc", c, 25000); }
#define TP_NO_EXC 0x7001

/* Tiny gate: callbacks block on `gate` (manual-reset) and report entry through `entered`. */
typedef struct tp_ctx {
    volatile LONG n;        /* general counter */
    volatile LONG n2;
    volatile LONG inflight, peak;
    volatile LONG seq;
    HANDLE ev;              /* signalled by callback */
    HANDLE gate;            /* callback waits on it */
    HANDLE ev2;
    PVOID  seen_ctx, seen_obj, seen_inst;
    volatile DWORD tid;
    volatile LONG val;
    volatile DWORD res;
    LONG order[64];
    DWORD tids[64];
} tp_ctx;

static inline void tp_ctx_init(tp_ctx *c) {
    memset(c, 0, sizeof *c);
    c->ev = tp_ev(FALSE); c->gate = tp_ev(TRUE); c->ev2 = tp_ev(FALSE);
}
static inline void tp_ctx_free(tp_ctx *c) {
    if (c->ev) CloseHandle(c->ev);
    if (c->gate) CloseHandle(c->gate);
    if (c->ev2) CloseHandle(c->ev2);
    c->ev = c->gate = c->ev2 = NULL;
}
static inline void tp_enter(tp_ctx *c) {
    LONG v = InterlockedIncrement(&c->inflight);
    LONG p;
    while ((p = c->peak) < v) InterlockedCompareExchange(&c->peak, v, p);
}
static inline void tp_leave(tp_ctx *c) { InterlockedDecrement(&c->inflight); }

/* Common callbacks. Context is a tp_ctx*. */
static VOID NTAPI tp_cb_count(PTP_CALLBACK_INSTANCE i, PVOID p) {
    tp_ctx *c = (tp_ctx *)p; c->seen_inst = i; c->tid = GetCurrentThreadId();
    InterlockedIncrement(&c->n);
    if (c->ev) SetEvent(c->ev);
}
static VOID NTAPI tp_cb_gate(PTP_CALLBACK_INSTANCE i, PVOID p) {
    tp_ctx *c = (tp_ctx *)p; tp_enter(c); InterlockedIncrement(&c->n);
    if (c->ev) SetEvent(c->ev);
    WaitForSingleObject(c->gate, 20000);
    InterlockedIncrement(&c->n2);
    tp_leave(c);
}
static VOID NTAPI tp_work_count(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_WORK w) {
    tp_ctx *c = (tp_ctx *)p; c->seen_inst = i; c->seen_obj = w; c->tid = GetCurrentThreadId();
    InterlockedIncrement(&c->n);
    if (c->ev) SetEvent(c->ev);
}
static VOID NTAPI tp_work_gate(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_WORK w) {
    (void)w; tp_cb_gate(i, p);
}
static VOID NTAPI tp_work_bump(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_WORK w) {
    InterlockedIncrement((volatile LONG *)p);
}
static VOID NTAPI tp_simple_bump(PTP_CALLBACK_INSTANCE i, PVOID p) {
    InterlockedIncrement((volatile LONG *)p);
}
static VOID NTAPI tp_timer_count(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_TIMER t) {
    tp_ctx *c = (tp_ctx *)p; c->seen_inst = i; c->seen_obj = t; c->tid = GetCurrentThreadId();
    InterlockedIncrement(&c->n);
    if (c->ev) SetEvent(c->ev);
}
static VOID NTAPI tp_wait_cbcount(PTP_CALLBACK_INSTANCE i, PVOID p, PTP_WAIT w, TP_WAIT_RESULT r) {
    tp_ctx *c = (tp_ctx *)p; c->seen_inst = i; c->seen_obj = w; c->res = r; c->tid = GetCurrentThreadId();
    InterlockedIncrement(&c->n);
    if (c->ev) SetEvent(c->ev);
}
/* WAITORTIMERCALLBACK that counts and records TimerOrWaitFired. */
static VOID CALLBACK tp_wot_count(PVOID p, BOOLEAN fired) {
    tp_ctx *c = (tp_ctx *)p; c->val = fired; c->tid = GetCurrentThreadId();
    InterlockedIncrement(&c->n);
    if (c->ev) SetEvent(c->ev);
}
static DWORD WINAPI tp_wi_count(LPVOID p) {
    tp_ctx *c = (tp_ctx *)p; c->tid = GetCurrentThreadId();
    InterlockedIncrement(&c->n);
    if (c->ev) SetEvent(c->ev);
    return 0;
}
#endif
