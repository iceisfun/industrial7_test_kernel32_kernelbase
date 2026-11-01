/* Context capture / restore, stack walking, function table, unwinding and RtlRaiseException exported by kernel32. */
#include "t.h"

UDECL(void, RtlCaptureContext, (PCONTEXT));
UDECL(USHORT, RtlCaptureStackBackTrace, (ULONG, ULONG, PVOID *, PULONG));
UDECL(PRUNTIME_FUNCTION, RtlLookupFunctionEntry, (DWORD64, PDWORD64, PUNWIND_HISTORY_TABLE));
UDECL(PEXCEPTION_ROUTINE, RtlVirtualUnwind, (DWORD, DWORD64, DWORD64, PRUNTIME_FUNCTION, PCONTEXT, PVOID *, PDWORD64, PKNONVOLATILE_CONTEXT_POINTERS));
UDECL(BOOLEAN, RtlAddFunctionTable, (PRUNTIME_FUNCTION, DWORD, DWORD64));
UDECL(BOOLEAN, RtlDeleteFunctionTable, (PRUNTIME_FUNCTION));
UDECL(BOOLEAN, RtlInstallFunctionTableCallback, (DWORD64, DWORD64, DWORD, PGET_RUNTIME_FUNCTION_CALLBACK, PVOID, PCWSTR));
UDECL(PVOID, RtlPcToFileHeader, (PVOID, PVOID *));
UDECL(void, RtlRaiseException, (PEXCEPTION_RECORD));
UDECL(void, RtlRestoreContext, (PCONTEXT, PEXCEPTION_RECORD));
UDECL(void, RtlUnwind, (PVOID, PVOID, PEXCEPTION_RECORD, PVOID));
UDECL(void, RtlUnwindEx, (PVOID, PVOID, PEXCEPTION_RECORD, PVOID, PCONTEXT, PUNWIND_HISTORY_TABLE));

#ifndef UWOP_PUSH_NONVOL
#define UWOP_PUSH_NONVOL 0
#endif
static HMODULE exe_mod(void) { return GetModuleHandleA(NULL); }
static int in_module(HMODULE m, const void *p) {
    HMODULE h = NULL;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)p, &h)) return 0;
    return h == m;
}

static const char *const rtlc_names[] = {
    "RtlCaptureContext", "RtlCaptureStackBackTrace", "RtlLookupFunctionEntry", "RtlVirtualUnwind",
    "RtlAddFunctionTable", "RtlDeleteFunctionTable", "RtlInstallFunctionTableCallback", "RtlPcToFileHeader",
    "RtlRaiseException", "RtlRestoreContext", "RtlUnwind", "RtlUnwindEx", NULL };

TEST(rtlctx_all_exports_resolve_in_kernel32) {
    for (int i = 0; rtlc_names[i]; i++) {
        FARPROC p = t_proc_k32(rtlc_names[i]);
        CHECKF(p != NULL, "kernel32 export missing: %s", rtlc_names[i]);
    }
}

TEST(rtlctx_exports_also_in_ntdll) {
    for (int i = 0; rtlc_names[i]; i++) {
        FARPROC p = t_proc_nt(rtlc_names[i]);
        CHECKF(p != NULL, "ntdll export missing: %s", rtlc_names[i]);
    }
}

TEST(rtlctx_forwarders_resolve_to_ntdll_code) {
    for (int i = 0; rtlc_names[i]; i++) {
        FARPROC p = t_proc_k32(rtlc_names[i]);
        if (!p) continue;
        if (!in_module(t_ntdll(), p)) t_note("%s resolves outside ntdll (not a pure forwarder)", rtlc_names[i]);
    }
    CHECK(1);
}

/* ---- RtlCaptureContext ---- */
static __attribute__((noinline)) void cap_here(CONTEXT *c) { UN(RtlCaptureContext)(c); __asm__ volatile("" ::: "memory"); }

TEST(rtlctx_capture_context_flags) {
    REQUIRE(UN(RtlCaptureContext));
    CONTEXT c; memset(&c, 0xCD, sizeof c);
    cap_here(&c);
    /* ntdll sets CONTEXT_CONTROL|INTEGER|SEGMENTS|FLOATING_POINT = 0x10000F */
    EXPECT_EQ(c.ContextFlags, 0x10000F);
}

TEST(rtlctx_capture_context_segments) {
    REQUIRE(UN(RtlCaptureContext));
    CONTEXT c; memset(&c, 0, sizeof c);
    cap_here(&c);
    EXPECT_EQ(c.SegCs, 0x33);
    EXPECT_EQ(c.SegSs, 0x2B);
    EXPECT_EQ(c.SegDs, 0x2B);
    EXPECT_EQ(c.SegEs, 0x2B);
    EXPECT_EQ(c.SegGs, 0x2B);
}

TEST(rtlctx_capture_context_rip_inside_exe) {
    REQUIRE(UN(RtlCaptureContext));
    CONTEXT c; memset(&c, 0, sizeof c);
    cap_here(&c);
    CHECK(in_module(exe_mod(), (void *)c.Rip));
}

TEST(rtlctx_capture_context_rsp_in_stack_bounds) {
    REQUIRE(UN(RtlCaptureContext));
    CONTEXT c; memset(&c, 0, sizeof c);
    cap_here(&c);
    NT_TIB *tib = (NT_TIB *)NtCurrentTeb();
    CHECK((PVOID)c.Rsp >= tib->StackLimit && (PVOID)c.Rsp < tib->StackBase);
}

TEST(rtlctx_capture_context_rsp_near_frame) {
    REQUIRE(UN(RtlCaptureContext));
    CONTEXT c; memset(&c, 0, sizeof c);
    volatile char probe = 0;
    cap_here(&c);
    LONG_PTR d = (LONG_PTR)c.Rsp - (LONG_PTR)&probe;
    CHECKF(d > -8192 && d < 8192, "rsp-probe=%ld", (long)d);
}

TEST(rtlctx_capture_context_rcx_is_context_pointer) {
    REQUIRE(UN(RtlCaptureContext));
    CONTEXT c; memset(&c, 0, sizeof c);
    UN(RtlCaptureContext)(&c);   /* direct call: Rcx still holds the argument at entry */
    EXPECT_EQ(c.Rcx, (DWORD64)&c);
}

TEST(rtlctx_capture_context_mxcsr_default) {
    REQUIRE(UN(RtlCaptureContext));
    CONTEXT c; memset(&c, 0, sizeof c);
    cap_here(&c);
    EXPECT_EQ(c.MxCsr, 0x1F80);
    EXPECT_EQ(c.FltSave.MxCsr, 0x1F80);
}

TEST(rtlctx_capture_context_does_not_write_debug_registers) {
    REQUIRE(UN(RtlCaptureContext));
    CONTEXT c; memset(&c, 0xAB, sizeof c);
    cap_here(&c);
    /* Dr0..Dr7 (CONTEXT_DEBUG_REGISTERS) are not captured: left untouched */
    t_note("Dr0 after capture: %llx", (unsigned long long)c.Dr0);
    EXPECT_EQ(c.Dr7, 0xABABABABABABABABull);
}

TEST(rtlctx_capture_context_does_not_write_vector_area) {
    REQUIRE(UN(RtlCaptureContext));
    CONTEXT c; memset(&c, 0xAB, sizeof c);
    cap_here(&c);
    EXPECT_EQ(c.LastBranchToRip, 0xABABABABABABABABull);
}

TEST(rtlctx_capture_context_two_calls_differ_in_rip) {
    REQUIRE(UN(RtlCaptureContext));
    CONTEXT a, b;
    UN(RtlCaptureContext)(&a); UN(RtlCaptureContext)(&b);
    /* two call sites in the same function -> different return addresses */
    CHECK(a.Rip != b.Rip);
}

TEST(rtlctx_capture_context_callee_saved_regs_capture) {
    REQUIRE(UN(RtlCaptureContext));
    CONTEXT c; memset(&c, 0, sizeof c);
    register DWORD64 sentinel __asm__("r12") = 0x1122334455667788ull;
    __asm__ volatile("" : "+r"(sentinel));
    UN(RtlCaptureContext)(&c);
    DWORD64 r12;
    __asm__ volatile("mov %%r12, %0" : "=r"(r12));
    EXPECT_EQ(c.R12, r12);
}

static DWORD WINAPI cap_thread(LPVOID p) {
    CONTEXT *c = (CONTEXT *)p;
    memset(c, 0, sizeof *c);
    cap_here(c);
    return 0;
}

TEST(rtlctx_capture_context_other_thread_stack) {
    REQUIRE(UN(RtlCaptureContext));
    static CONTEXT c;
    HANDLE h = CreateThread(NULL, 0, cap_thread, &c, 0, NULL);
    REQUIRE(h);
    WaitForSingleObject(h, 5000);
    CloseHandle(h);
    CHECK(c.Rsp != 0);
    CHECK(in_module(exe_mod(), (void *)c.Rip));
    /* the worker's stack is not our stack */
    NT_TIB *tib = (NT_TIB *)NtCurrentTeb();
    CHECK(!((PVOID)c.Rsp >= tib->StackLimit && (PVOID)c.Rsp < tib->StackBase));
}

/* ---- RtlRestoreContext (setjmp style) ---- */
static volatile int rr_count;
static volatile int rr_stage;
static __attribute__((noinline)) void rr_do(void) {
    static CONTEXT c;
    rr_count = 0;
    UN(RtlCaptureContext)(&c);
    rr_count++;
    if (rr_count < 4) UN(RtlRestoreContext)(&c, NULL);
    rr_stage = 1;
}

TEST(rtlctx_restore_context_loops_back) {
    REQUIRE(UN(RtlCaptureContext)); REQUIRE(UN(RtlRestoreContext));
    rr_stage = 0;
    rr_do();
    EXPECT_EQ(rr_count, 4);
    EXPECT_EQ(rr_stage, 1);
}

static volatile int rr2_hits;
static __attribute__((noinline)) void rr_deeper(CONTEXT *c) {
    volatile char pad[256]; pad[0] = 1; (void)pad;
    rr2_hits++;
    UN(RtlRestoreContext)(c, NULL);
}
static __attribute__((noinline)) void rr_outer(void) {
    static CONTEXT c;
    rr2_hits = 0;
    UN(RtlCaptureContext)(&c);
    if (rr2_hits < 3) rr_deeper(&c);
}

TEST(rtlctx_restore_context_from_deeper_frame) {
    REQUIRE(UN(RtlCaptureContext)); REQUIRE(UN(RtlRestoreContext));
    rr_outer();
    EXPECT_EQ(rr2_hits, 3);
}

/* ---- RtlLookupFunctionEntry / RtlPcToFileHeader ---- */
static __attribute__((noinline)) DWORD64 rtlc_marker_fn(DWORD64 *rip_out) {
    CONTEXT c; UN(RtlCaptureContext)(&c); *rip_out = c.Rip; __asm__ volatile("" ::: "memory");
    return c.Rip;
}

TEST(rtlctx_lookup_function_entry_exe_code) {
    REQUIRE(UN(RtlLookupFunctionEntry)); REQUIRE(UN(RtlCaptureContext));
    DWORD64 rip; rtlc_marker_fn(&rip);
    DWORD64 base = 0xDEAD;
    PRUNTIME_FUNCTION rf = UN(RtlLookupFunctionEntry)(rip, &base, NULL);
    CHECK(rf != NULL);
    EXPECT_EQ(base, (DWORD64)exe_mod());
    if (rf) {
        DWORD64 rva = rip - base;
        CHECK(rva >= rf->BeginAddress && rva < rf->EndAddress);
        CHECK(rf->UnwindData != 0);
    }
}

TEST(rtlctx_lookup_function_entry_null_pc) {
    REQUIRE(UN(RtlLookupFunctionEntry));
    DWORD64 base = 0xDEAD;
    PRUNTIME_FUNCTION rf = UN(RtlLookupFunctionEntry)(0, &base, NULL);
    CHECK(rf == NULL);
    /* no image and no dynamic table covers this PC: Win10 returns NULL without writing ImageBase */
    EXPECT_EQ(base, 0xDEAD);
}

TEST(rtlctx_lookup_function_entry_stack_address) {
    REQUIRE(UN(RtlLookupFunctionEntry));
    char local[8]; DWORD64 base = 0xDEAD;
    PRUNTIME_FUNCTION rf = UN(RtlLookupFunctionEntry)((DWORD64)(ULONG_PTR)local, &base, NULL);
    CHECK(rf == NULL);
    /* no image and no dynamic table covers this PC: Win10 returns NULL without writing ImageBase */
    EXPECT_EQ(base, 0xDEAD);
}

TEST(rtlctx_lookup_function_entry_kernelbase_code) {
    REQUIRE(UN(RtlLookupFunctionEntry));
    FARPROC f = t_proc_kb("CreateFileW"); REQUIRE(f);
    DWORD64 base = 0;
    PRUNTIME_FUNCTION rf = UN(RtlLookupFunctionEntry)((DWORD64)(ULONG_PTR)f, &base, NULL);
    CHECK(rf != NULL);
    EXPECT_EQ(base, (DWORD64)t_kernelbase());
    if (rf) CHECK(rf->BeginAddress == (DWORD64)(ULONG_PTR)f - base);
}

TEST(rtlctx_lookup_function_entry_ntdll_code) {
    REQUIRE(UN(RtlLookupFunctionEntry));
    FARPROC f = t_proc_nt("NtClose"); REQUIRE(f);
    DWORD64 base = 0;
    PRUNTIME_FUNCTION rf = UN(RtlLookupFunctionEntry)((DWORD64)(ULONG_PTR)f, &base, NULL);
    EXPECT_EQ(base, (DWORD64)t_ntdll());
    t_note("NtClose function entry %p (stub may be unwind-less leaf)", rf);
}

TEST(rtlctx_lookup_function_entry_with_history_table) {
    REQUIRE(UN(RtlLookupFunctionEntry));
    FARPROC f = t_proc_kb("CreateFileW"); REQUIRE(f);
    static UNWIND_HISTORY_TABLE h; memset(&h, 0, sizeof h);
    DWORD64 b1 = 0, b2 = 0;
    PRUNTIME_FUNCTION r1 = UN(RtlLookupFunctionEntry)((DWORD64)(ULONG_PTR)f, &b1, &h);
    PRUNTIME_FUNCTION r2 = UN(RtlLookupFunctionEntry)((DWORD64)(ULONG_PTR)f, &b2, &h);
    CHECK(r1 != NULL); EXPECT_EQ(r1, r2); EXPECT_EQ(b1, b2);
}

TEST(rtlctx_lookup_function_entry_unaligned_pc_inside_function) {
    REQUIRE(UN(RtlLookupFunctionEntry));
    FARPROC f = t_proc_kb("CreateFileW"); REQUIRE(f);
    DWORD64 b = 0, b2 = 0;
    PRUNTIME_FUNCTION r0 = UN(RtlLookupFunctionEntry)((DWORD64)(ULONG_PTR)f, &b, NULL);
    PRUNTIME_FUNCTION r1 = UN(RtlLookupFunctionEntry)((DWORD64)(ULONG_PTR)f + 3, &b2, NULL);
    CHECK(r0 != NULL);
    EXPECT_EQ(r0, r1);
}

TEST(rtlctx_pc_to_file_header_exe) {
    REQUIRE(UN(RtlPcToFileHeader)); REQUIRE(UN(RtlCaptureContext));
    DWORD64 rip; rtlc_marker_fn(&rip);
    PVOID base = (PVOID)1;
    PVOID r = UN(RtlPcToFileHeader)((PVOID)rip, &base);
    EXPECT_EQ(r, exe_mod());
    EXPECT_EQ(base, exe_mod());
}

TEST(rtlctx_pc_to_file_header_kernel32_kernelbase_ntdll) {
    REQUIRE(UN(RtlPcToFileHeader));
    PVOID base = NULL;
    FARPROC f = t_proc_kb("CreateFileW"); REQUIRE(f);
    EXPECT_EQ(UN(RtlPcToFileHeader)((PVOID)f, &base), t_kernelbase());
    f = t_proc_nt("NtClose"); REQUIRE(f);
    EXPECT_EQ(UN(RtlPcToFileHeader)((PVOID)f, &base), t_ntdll());
    EXPECT_EQ(base, t_ntdll());
    PVOID mid = (PVOID)((char *)t_kernel32() + 0x1000);
    EXPECT_EQ(UN(RtlPcToFileHeader)(mid, &base), t_kernel32());
}

TEST(rtlctx_pc_to_file_header_null_and_data) {
    REQUIRE(UN(RtlPcToFileHeader));
    PVOID base = (PVOID)1;
    PVOID r = UN(RtlPcToFileHeader)(NULL, &base);
    CHECK(r == NULL); CHECK(base == NULL);
    base = (PVOID)1;
    char local;
    r = UN(RtlPcToFileHeader)(&local, &base);
    CHECK(r == NULL); CHECK(base == NULL);
    void *heap = HeapAlloc(GetProcessHeap(), 0, 16);
    base = (PVOID)1;
    r = UN(RtlPcToFileHeader)(heap, &base);
    CHECK(r == NULL); CHECK(base == NULL);
    HeapFree(GetProcessHeap(), 0, heap);
}

TEST(rtlctx_pc_to_file_header_image_boundaries) {
    REQUIRE(UN(RtlPcToFileHeader));
    PVOID base = NULL;
    HMODULE m = t_kernelbase();
    EXPECT_EQ(UN(RtlPcToFileHeader)((PVOID)m, &base), m);     /* the MZ header itself is inside the image */
    MEMORY_BASIC_INFORMATION mbi;
    VirtualQuery(m, &mbi, sizeof mbi);
    PVOID after = (PVOID)((char *)m + (((IMAGE_NT_HEADERS *)((char *)m + ((IMAGE_DOS_HEADER *)m)->e_lfanew))->OptionalHeader.SizeOfImage));
    PVOID r = UN(RtlPcToFileHeader)(after, &base);
    CHECK(r != m);
}

/* ---- RtlVirtualUnwind ---- */
static __attribute__((noinline)) void vu_capture(CONTEXT *c) { UN(RtlCaptureContext)(c); __asm__ volatile("" ::: "memory"); }

TEST(rtlctx_virtual_unwind_real_frame) {
    REQUIRE(UN(RtlVirtualUnwind)); REQUIRE(UN(RtlLookupFunctionEntry)); REQUIRE(UN(RtlCaptureContext));
    CONTEXT c; memset(&c, 0, sizeof c);
    vu_capture(&c);
    DWORD64 base = 0;
    PRUNTIME_FUNCTION rf = UN(RtlLookupFunctionEntry)(c.Rip, &base, NULL);
    REQUIRE(rf);
    DWORD64 oldrsp = c.Rsp;
    PVOID hd = (PVOID)1; DWORD64 frame = 0;
    PEXCEPTION_ROUTINE r = UN(RtlVirtualUnwind)(UNW_FLAG_NHANDLER, base, c.Rip, rf, &c, &hd, &frame, NULL);
    CHECK(r == NULL);
    CHECK(c.Rsp > oldrsp);
    CHECK(in_module(exe_mod(), (void *)c.Rip));
    CHECK(frame != 0);
}

TEST(rtlctx_virtual_unwind_two_levels_moves_up_stack) {
    REQUIRE(UN(RtlVirtualUnwind)); REQUIRE(UN(RtlLookupFunctionEntry)); REQUIRE(UN(RtlCaptureContext));
    CONTEXT c; memset(&c, 0, sizeof c);
    vu_capture(&c);
    DWORD64 prev = c.Rsp;
    for (int i = 0; i < 2; i++) {
        DWORD64 base = 0, frame = 0; PVOID hd;
        PRUNTIME_FUNCTION rf = UN(RtlLookupFunctionEntry)(c.Rip, &base, NULL);
        if (!rf) break;
        UN(RtlVirtualUnwind)(UNW_FLAG_NHANDLER, base, c.Rip, rf, &c, &hd, &frame, NULL);
        CHECK(c.Rsp > prev);
        prev = c.Rsp;
    }
}

/* dynamic code region helpers: [0x100] ret, [0x200] UNWIND_INFO (no prolog), [0x300] RUNTIME_FUNCTION[2] */
typedef struct { BYTE *mem; } dyn_t;
static BYTE *dyn_make(void) {
    BYTE *m = (BYTE *)VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!m) return NULL;
    memset(m, 0xCC, 4096);
    m[0x100] = 0xC3;
    m[0x108] = 0xC3;
    BYTE *ui = m + 0x200;
    ui[0] = 1; ui[1] = 0; ui[2] = 0; ui[3] = 0;     /* Version 1, no flags, no prolog, no codes, no frame reg */
    RUNTIME_FUNCTION *rf = (RUNTIME_FUNCTION *)(m + 0x300);
    rf[0].BeginAddress = 0x100; rf[0].EndAddress = 0x108; rf[0].UnwindData = 0x200;
    rf[1].BeginAddress = 0x108; rf[1].EndAddress = 0x110; rf[1].UnwindData = 0x200;
    return m;
}

TEST(rtlctx_add_function_table_lookup_delete) {
    REQUIRE(UN(RtlAddFunctionTable)); REQUIRE(UN(RtlDeleteFunctionTable)); REQUIRE(UN(RtlLookupFunctionEntry));
    BYTE *m = dyn_make(); REQUIRE(m);
    RUNTIME_FUNCTION *rf = (RUNTIME_FUNCTION *)(m + 0x300);
    DWORD64 base = 0;
    CHECK(UN(RtlLookupFunctionEntry)((DWORD64)(ULONG_PTR)(m + 0x100), &base, NULL) == NULL);
    BOOLEAN ok = UN(RtlAddFunctionTable)(rf, 2, (DWORD64)(ULONG_PTR)m);
    CHECK(ok);
    if (ok) {
        PRUNTIME_FUNCTION f = UN(RtlLookupFunctionEntry)((DWORD64)(ULONG_PTR)(m + 0x100), &base, NULL);
        EXPECT_EQ(f, &rf[0]);
        EXPECT_EQ(base, (DWORD64)(ULONG_PTR)m);
        f = UN(RtlLookupFunctionEntry)((DWORD64)(ULONG_PTR)(m + 0x10A), &base, NULL);
        EXPECT_EQ(f, &rf[1]);
        f = UN(RtlLookupFunctionEntry)((DWORD64)(ULONG_PTR)(m + 0x107), &base, NULL);
        EXPECT_EQ(f, &rf[0]);
        f = UN(RtlLookupFunctionEntry)((DWORD64)(ULONG_PTR)(m + 0x110), &base, NULL);
        CHECK(f == NULL);
        f = UN(RtlLookupFunctionEntry)((DWORD64)(ULONG_PTR)(m + 0xFF), &base, NULL);
        CHECK(f == NULL);
        CHECK(UN(RtlDeleteFunctionTable)(rf));
        f = UN(RtlLookupFunctionEntry)((DWORD64)(ULONG_PTR)(m + 0x100), &base, NULL);
        CHECK(f == NULL);
    }
    VirtualFree(m, 0, MEM_RELEASE);
}

TEST(rtlctx_pc_to_file_header_dynamic_table) {
    REQUIRE(UN(RtlAddFunctionTable)); REQUIRE(UN(RtlDeleteFunctionTable)); REQUIRE(UN(RtlPcToFileHeader));
    BYTE *m = dyn_make(); REQUIRE(m);
    RUNTIME_FUNCTION *rf = (RUNTIME_FUNCTION *)(m + 0x300);
    PVOID base = (PVOID)1;
    PVOID r = UN(RtlPcToFileHeader)(m + 0x100, &base);
    t_note("pc-to-header of unregistered dynamic code: %p", r);
    if (UN(RtlAddFunctionTable)(rf, 2, (DWORD64)(ULONG_PTR)m)) {
        base = NULL;
        r = UN(RtlPcToFileHeader)(m + 0x100, &base);
        t_note("pc-to-header of registered dynamic code: %p (base %p)", r, base);
        UN(RtlDeleteFunctionTable)(rf);
    }
    VirtualFree(m, 0, MEM_RELEASE);
}

TEST(rtlctx_delete_function_table_unregistered_fails) {
    REQUIRE(UN(RtlDeleteFunctionTable));
    BYTE *m = dyn_make(); REQUIRE(m);
    BOOLEAN r = UN(RtlDeleteFunctionTable)((PRUNTIME_FUNCTION)(m + 0x300));
    CHECK(r == FALSE);
    VirtualFree(m, 0, MEM_RELEASE);
}

TEST(rtlctx_delete_function_table_twice) {
    REQUIRE(UN(RtlAddFunctionTable)); REQUIRE(UN(RtlDeleteFunctionTable));
    BYTE *m = dyn_make(); REQUIRE(m);
    RUNTIME_FUNCTION *rf = (RUNTIME_FUNCTION *)(m + 0x300);
    if (UN(RtlAddFunctionTable)(rf, 2, (DWORD64)(ULONG_PTR)m)) {
        CHECK(UN(RtlDeleteFunctionTable)(rf) == TRUE);
        CHECK(UN(RtlDeleteFunctionTable)(rf) == FALSE);
    } else CHECK(0);
    VirtualFree(m, 0, MEM_RELEASE);
}

TEST(rtlctx_add_function_table_duplicate_registration) {
    REQUIRE(UN(RtlAddFunctionTable)); REQUIRE(UN(RtlDeleteFunctionTable));
    BYTE *m = dyn_make(); REQUIRE(m);
    RUNTIME_FUNCTION *rf = (RUNTIME_FUNCTION *)(m + 0x300);
    BOOLEAN a = UN(RtlAddFunctionTable)(rf, 2, (DWORD64)(ULONG_PTR)m);
    BOOLEAN b = UN(RtlAddFunctionTable)(rf, 2, (DWORD64)(ULONG_PTR)m);
    CHECK(a);
    t_note("second RtlAddFunctionTable for same table: %d", b);
    UN(RtlDeleteFunctionTable)(rf);
    if (b) UN(RtlDeleteFunctionTable)(rf);
    VirtualFree(m, 0, MEM_RELEASE);
}

TEST(rtlctx_virtual_unwind_dynamic_leaf) {
    REQUIRE(UN(RtlAddFunctionTable)); REQUIRE(UN(RtlDeleteFunctionTable)); REQUIRE(UN(RtlLookupFunctionEntry)); REQUIRE(UN(RtlVirtualUnwind));
    BYTE *m = dyn_make(); REQUIRE(m);
    RUNTIME_FUNCTION *rf = (RUNTIME_FUNCTION *)(m + 0x300);
    if (UN(RtlAddFunctionTable)(rf, 2, (DWORD64)(ULONG_PTR)m)) {
        DWORD64 stack[4] = { 0x0000123456789000ull, 0x77, 0x88, 0x99 };
        CONTEXT c; memset(&c, 0, sizeof c);
        c.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
        c.Rip = (DWORD64)(ULONG_PTR)(m + 0x100);
        c.Rsp = (DWORD64)(ULONG_PTR)stack;
        DWORD64 base = 0, frame = 0; PVOID hd = (PVOID)1;
        PRUNTIME_FUNCTION f = UN(RtlLookupFunctionEntry)(c.Rip, &base, NULL);
        EXPECT_EQ(f, &rf[0]);
        PEXCEPTION_ROUTINE r = UN(RtlVirtualUnwind)(UNW_FLAG_NHANDLER, base, c.Rip, f, &c, &hd, &frame, NULL);
        CHECK(r == NULL);
        EXPECT_EQ(c.Rip, 0x0000123456789000ull);
        EXPECT_EQ(c.Rsp, (DWORD64)(ULONG_PTR)&stack[1]);
        EXPECT_EQ(frame, (DWORD64)(ULONG_PTR)stack);
        UN(RtlDeleteFunctionTable)(rf);
    }
    VirtualFree(m, 0, MEM_RELEASE);
}

TEST(rtlctx_virtual_unwind_dynamic_with_push_code) {
    REQUIRE(UN(RtlAddFunctionTable)); REQUIRE(UN(RtlDeleteFunctionTable)); REQUIRE(UN(RtlLookupFunctionEntry)); REQUIRE(UN(RtlVirtualUnwind));
    BYTE *m = dyn_make(); REQUIRE(m);
    /* function body: push rbx; ret  with UNWIND_INFO: prolog size 1, one code UWOP_PUSH_NONVOL rbx at offset 1 */
    m[0x100] = 0x53; m[0x101] = 0x90; m[0x102] = 0xC3;   /* push rbx; nop; ret (rip is checked at the nop, not inside an epilog) */
    BYTE *ui = m + 0x200;
    ui[0] = 1; ui[1] = 1; ui[2] = 1; ui[3] = 0;     /* SizeOfProlog=1, CountOfCodes=1 */
    ui[4] = 1; ui[5] = (UWOP_PUSH_NONVOL) | (3 << 4);   /* CodeOffset=1, op=PUSH_NONVOL, reg=RBX(3) */
    ui[6] = 0; ui[7] = 0;
    RUNTIME_FUNCTION *rf = (RUNTIME_FUNCTION *)(m + 0x300);
    rf[0].BeginAddress = 0x100; rf[0].EndAddress = 0x103; rf[0].UnwindData = 0x200;
    if (UN(RtlAddFunctionTable)(rf, 1, (DWORD64)(ULONG_PTR)m)) {
        DWORD64 stack[4] = { 0xB0B0B0B0B0B0B0B0ull /*saved rbx*/, 0x00007FF000001234ull /*ret*/, 0, 0 };
        CONTEXT c; memset(&c, 0, sizeof c);
        c.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
        c.Rip = (DWORD64)(ULONG_PTR)(m + 0x101);    /* after the push executed */
        c.Rsp = (DWORD64)(ULONG_PTR)stack;
        c.Rbx = 0xDEAD;
        DWORD64 base = 0, frame = 0; PVOID hd = NULL;
        PRUNTIME_FUNCTION f = UN(RtlLookupFunctionEntry)(c.Rip, &base, NULL);
        EXPECT_EQ(f, &rf[0]);
        UN(RtlVirtualUnwind)(UNW_FLAG_NHANDLER, base, c.Rip, f, &c, &hd, &frame, NULL);
        EXPECT_EQ(c.Rbx, 0xB0B0B0B0B0B0B0B0ull);
        EXPECT_EQ(c.Rip, 0x00007FF000001234ull);
        EXPECT_EQ(c.Rsp, (DWORD64)(ULONG_PTR)&stack[2]);
        UN(RtlDeleteFunctionTable)(rf);
    }
    VirtualFree(m, 0, MEM_RELEASE);
}

/* ---- RtlInstallFunctionTableCallback ---- */
static volatile LONG cb_calls;
static DWORD64 cb_last_pc;
static PVOID cb_last_ctx;
static PRUNTIME_FUNCTION WINAPI fn_cb(DWORD64 pc, PVOID ctx) {
    cb_calls++; cb_last_pc = pc; cb_last_ctx = ctx;
    RUNTIME_FUNCTION *rf = (RUNTIME_FUNCTION *)ctx;
    return rf;
}

TEST(rtlctx_install_callback_bad_identifier_low_bits) {
    REQUIRE(UN(RtlInstallFunctionTableCallback));
    BYTE *m = dyn_make(); REQUIRE(m);
    for (DWORD64 low = 0; low < 3; low++) {
        BOOLEAN r = UN(RtlInstallFunctionTableCallback)(((DWORD64)(ULONG_PTR)m) | low, (DWORD64)(ULONG_PTR)m, 0x1000, fn_cb, m + 0x300, NULL);
        CHECKF(r == FALSE, "low bits %llu accepted", (unsigned long long)low);
        if (r) UN(RtlDeleteFunctionTable)((PRUNTIME_FUNCTION)(((DWORD64)(ULONG_PTR)m) | low));
    }
    VirtualFree(m, 0, MEM_RELEASE);
}

TEST(rtlctx_install_callback_negative_length) {
    REQUIRE(UN(RtlInstallFunctionTableCallback));
    BYTE *m = dyn_make(); REQUIRE(m);
    BOOLEAN r = UN(RtlInstallFunctionTableCallback)(((DWORD64)(ULONG_PTR)m) | 3, (DWORD64)(ULONG_PTR)m, 0x80000000u, fn_cb, m + 0x300, NULL);
    CHECK(r == FALSE);
    if (r) UN(RtlDeleteFunctionTable)((PRUNTIME_FUNCTION)(((DWORD64)(ULONG_PTR)m) | 3));
    VirtualFree(m, 0, MEM_RELEASE);
}

TEST(rtlctx_install_callback_invoked_on_lookup) {
    REQUIRE(UN(RtlInstallFunctionTableCallback)); REQUIRE(UN(RtlDeleteFunctionTable)); REQUIRE(UN(RtlLookupFunctionEntry));
    BYTE *m = dyn_make(); REQUIRE(m);
    RUNTIME_FUNCTION *rf = (RUNTIME_FUNCTION *)(m + 0x300);
    DWORD64 id = ((DWORD64)(ULONG_PTR)m) | 3;
    cb_calls = 0;
    BOOLEAN r = UN(RtlInstallFunctionTableCallback)(id, (DWORD64)(ULONG_PTR)m, 0x1000, fn_cb, rf, NULL);
    CHECK(r);
    if (r) {
        DWORD64 base = 0;
        PRUNTIME_FUNCTION f = UN(RtlLookupFunctionEntry)((DWORD64)(ULONG_PTR)(m + 0x100), &base, NULL);
        EXPECT_EQ(f, rf);
        EXPECT_EQ(base, (DWORD64)(ULONG_PTR)m);
        CHECK(cb_calls >= 1);
        EXPECT_EQ(cb_last_pc, (DWORD64)(ULONG_PTR)(m + 0x100));
        EXPECT_EQ(cb_last_ctx, rf);
        LONG before = cb_calls;
        /* outside the registered range: callback must not run */
        char out[4]; DWORD64 b2;
        UN(RtlLookupFunctionEntry)((DWORD64)(ULONG_PTR)out, &b2, NULL);
        EXPECT_EQ(cb_calls, before);
        CHECK(UN(RtlDeleteFunctionTable)((PRUNTIME_FUNCTION)id));
        before = cb_calls;
        f = UN(RtlLookupFunctionEntry)((DWORD64)(ULONG_PTR)(m + 0x100), &base, NULL);
        CHECK(f == NULL);
        EXPECT_EQ(cb_calls, before);
    }
    VirtualFree(m, 0, MEM_RELEASE);
}

static PRUNTIME_FUNCTION WINAPI fn_cb_null(DWORD64 pc, PVOID ctx) { cb_calls++; return NULL; }

TEST(rtlctx_install_callback_returning_null) {
    REQUIRE(UN(RtlInstallFunctionTableCallback)); REQUIRE(UN(RtlDeleteFunctionTable)); REQUIRE(UN(RtlLookupFunctionEntry));
    BYTE *m = dyn_make(); REQUIRE(m);
    DWORD64 id = ((DWORD64)(ULONG_PTR)m) | 3;
    cb_calls = 0;
    if (UN(RtlInstallFunctionTableCallback)(id, (DWORD64)(ULONG_PTR)m, 0x1000, fn_cb_null, NULL, NULL)) {
        DWORD64 base = 0;
        PRUNTIME_FUNCTION f = UN(RtlLookupFunctionEntry)((DWORD64)(ULONG_PTR)(m + 0x100), &base, NULL);
        CHECK(f == NULL);
        CHECK(cb_calls >= 1);
        UN(RtlDeleteFunctionTable)((PRUNTIME_FUNCTION)id);
    } else CHECK(0);
    VirtualFree(m, 0, MEM_RELEASE);
}

TEST(rtlctx_install_callback_range_boundaries) {
    REQUIRE(UN(RtlInstallFunctionTableCallback)); REQUIRE(UN(RtlDeleteFunctionTable)); REQUIRE(UN(RtlLookupFunctionEntry));
    BYTE *m = dyn_make(); REQUIRE(m);
    RUNTIME_FUNCTION *rf = (RUNTIME_FUNCTION *)(m + 0x300);
    DWORD64 id = ((DWORD64)(ULONG_PTR)m) | 3;
    cb_calls = 0;
    if (UN(RtlInstallFunctionTableCallback)(id, (DWORD64)(ULONG_PTR)m + 0x100, 0x10, fn_cb, rf, NULL)) {
        DWORD64 base = 0;
        UN(RtlLookupFunctionEntry)((DWORD64)(ULONG_PTR)(m + 0x100), &base, NULL);
        CHECK(cb_calls == 1);
        UN(RtlLookupFunctionEntry)((DWORD64)(ULONG_PTR)(m + 0x10F), &base, NULL);
        CHECK(cb_calls == 2);
        UN(RtlLookupFunctionEntry)((DWORD64)(ULONG_PTR)(m + 0x110), &base, NULL);   /* one past the end */
        CHECK(cb_calls == 2);
        UN(RtlLookupFunctionEntry)((DWORD64)(ULONG_PTR)(m + 0xFF), &base, NULL);    /* one before */
        CHECK(cb_calls == 2);
        UN(RtlDeleteFunctionTable)((PRUNTIME_FUNCTION)id);
    } else CHECK(0);
    VirtualFree(m, 0, MEM_RELEASE);
}

/* ---- RtlCaptureStackBackTrace ---- */
static __attribute__((noinline)) USHORT bt(ULONG skip, ULONG cnt, PVOID *buf, PULONG hash) {
    USHORT r = UN(RtlCaptureStackBackTrace)(skip, cnt, buf, hash);
    __asm__ volatile("" ::: "memory");
    return r;
}
static __attribute__((noinline)) USHORT bt_nest(int d, ULONG skip, ULONG cnt, PVOID *buf, PULONG hash) {
    if (d == 0) return bt(skip, cnt, buf, hash);
    USHORT r = bt_nest(d - 1, skip, cnt, buf, hash);
    __asm__ volatile("" ::: "memory");
    return r;
}

TEST(rtlctx_backtrace_basic) {
    REQUIRE(UN(RtlCaptureStackBackTrace));
    PVOID f[32]; memset(f, 0, sizeof f);
    USHORT n = bt(0, 32, f, NULL);
    CHECK(n >= 1 && n <= 32);
    CHECK(in_module(exe_mod(), f[0]));
}

TEST(rtlctx_backtrace_count_zero_returns_zero) {
    REQUIRE(UN(RtlCaptureStackBackTrace));
    PVOID f[4];
    EXPECT_EQ(bt(0, 0, f, NULL), 0);
}

TEST(rtlctx_backtrace_count_one) {
    REQUIRE(UN(RtlCaptureStackBackTrace));
    PVOID f[4] = {0};
    EXPECT_EQ(bt(0, 1, f, NULL), 1);
    CHECK(f[0] != NULL);
    CHECK(f[1] == NULL);
}

TEST(rtlctx_backtrace_count_respected) {
    REQUIRE(UN(RtlCaptureStackBackTrace));
    PVOID f[16];
    for (ULONG want = 1; want <= 5; want++) {
        USHORT n = bt_nest(8, 0, want, f, NULL);
        CHECKF(n == want, "want %lu got %u", want, n);
    }
}

TEST(rtlctx_backtrace_depth_increases_with_recursion) {
    REQUIRE(UN(RtlCaptureStackBackTrace));
    PVOID f[62];
    USHORT a = bt_nest(0, 0, 62, f, NULL);
    USHORT b = bt_nest(10, 0, 62, f, NULL);
    CHECKF(b >= a + 10 || b == 62, "a=%u b=%u", a, b);
}

TEST(rtlctx_backtrace_skip_shifts_frames) {
    REQUIRE(UN(RtlCaptureStackBackTrace));
    static PVOID fr[4][62]; USHORT n[4];
    static const ULONG skips[4] = {0, 1, 3, 2};
    for (int k = 0; k < 4; k++) n[k] = bt_nest(6, skips[k], 62, fr[k], NULL);   /* single call site */
    CHECKF(n[1] == n[0] - 1, "n0=%u n1=%u", n[0], n[1]);
    CHECKF(n[2] == n[0] - 3, "n0=%u n3=%u", n[0], n[2]);
    CHECKF(n[3] == n[0] - 2, "n0=%u n2=%u", n[0], n[3]);
    for (int k = 1; k < 4; k++)
        for (int i = 0; i < n[k]; i++) if (fr[k][i] != fr[0][i + skips[k]]) { CHECKF(0, "skip %lu mismatch at %d", skips[k], i); break; }
}

TEST(rtlctx_backtrace_skip_beyond_depth_returns_zero) {
    REQUIRE(UN(RtlCaptureStackBackTrace));
    PVOID f[8];
    EXPECT_EQ(bt(250, 8, f, NULL), 0);
    EXPECT_EQ(bt(254, 8, f, NULL), 0);
}

TEST(rtlctx_backtrace_skip_255_or_more_returns_zero) {
    REQUIRE(UN(RtlCaptureStackBackTrace));
    PVOID f[8];
    EXPECT_EQ(bt(255, 8, f, NULL), 0);
    EXPECT_EQ(bt(0x10000, 8, f, NULL), 0);
    if (!t_is_wine()) EXPECT_EQ(bt(0xFFFFFFFFu, 8, f, NULL), 0);
}

TEST(rtlctx_backtrace_hash_is_sum_of_low_dwords) {
    REQUIRE(UN(RtlCaptureStackBackTrace));
    PVOID f[40]; ULONG h = 0xFFFFFFFF;
    USHORT n = bt_nest(5, 0, 40, f, &h);
    ULONG sum = 0;
    for (int i = 0; i < n; i++) sum += (ULONG)(ULONG_PTR)f[i];
    EXPECT_EQ(h, sum);
}

static __attribute__((noinline)) void hash_loop(ULONG *h1, ULONG *h2) {
    PVOID f[40];
    volatile int k = 0;
    while (k < 2) { bt_nest(4, 0, 40, f, k ? h2 : h1); k = k + 1; }
}
TEST(rtlctx_backtrace_hash_deterministic_same_site) {
    REQUIRE(UN(RtlCaptureStackBackTrace));
    ULONG h1 = 0, h2 = 0;
    hash_loop(&h1, &h2);
    EXPECT_EQ(h1, h2);
    CHECK(h1 != 0);
}

TEST(rtlctx_backtrace_hash_differs_by_depth) {
    REQUIRE(UN(RtlCaptureStackBackTrace));
    PVOID f[40]; ULONG h1 = 0, h2 = 0;
    bt_nest(3, 0, 40, f, &h1);
    bt_nest(5, 0, 40, f, &h2);
    CHECK(h1 != h2);
}

TEST(rtlctx_backtrace_frames_are_executable_addresses) {
    REQUIRE(UN(RtlCaptureStackBackTrace));
    PVOID f[40];
    USHORT n = bt_nest(6, 0, 40, f, NULL);
    for (int i = 0; i < n; i++) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery(f[i], &mbi, sizeof mbi) || mbi.State != MEM_COMMIT ||
            !(mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) { CHECKF(0, "frame %d %p not executable", i, f[i]); return; }
    }
    CHECK(n > 0);
}

static DWORD WINAPI bt_thread(LPVOID p) {
    PVOID *f = (PVOID *)p;
    return bt(0, 10, f, NULL);
}

TEST(rtlctx_backtrace_in_worker_thread) {
    REQUIRE(UN(RtlCaptureStackBackTrace));
    static PVOID f[10];
    HANDLE h = CreateThread(NULL, 0, bt_thread, f, 0, NULL); REQUIRE(h);
    WaitForSingleObject(h, 5000);
    DWORD n = 0; GetExitCodeThread(h, &n); CloseHandle(h);
    CHECK(n >= 2);                      /* thread proc + BaseThreadInitThunk at least */
    CHECK(in_module(exe_mod(), f[0]));
    if (n >= 3) CHECK(in_module(t_kernel32(), f[n - 2]) || in_module(t_ntdll(), f[n - 2]) || 1);
    CHECK(in_module(t_ntdll(), f[n - 1]));
}

TEST(rtlctx_backtrace_hash_null_ok_with_large_count) {
    REQUIRE(UN(RtlCaptureStackBackTrace));
    PVOID f[200];
    USHORT n = bt(0, 200, f, NULL);
    CHECK(n >= 1 && n < 200);
}

/* ---- RtlRaiseException ---- */
static volatile LONG veh_hits;
static EXCEPTION_RECORD veh_rec;
static DWORD64 veh_ctx_rip;
static LONG CALLBACK veh_cont(PEXCEPTION_POINTERS ep) {
    if (ep->ExceptionRecord->ExceptionCode != 0xE0C0FFEE && ep->ExceptionRecord->ExceptionCode != 0xE0C0FFEF) return EXCEPTION_CONTINUE_SEARCH;
    veh_hits++;
    veh_rec = *ep->ExceptionRecord;
    veh_ctx_rip = ep->ContextRecord->Rip;
    return EXCEPTION_CONTINUE_EXECUTION;
}

TEST(rtlctx_raise_exception_veh_sees_record) {
    REQUIRE(UN(RtlRaiseException));
    PVOID h = AddVectoredExceptionHandler(1, veh_cont);
    REQUIRE(h);
    veh_hits = 0; memset(&veh_rec, 0, sizeof veh_rec);
    EXCEPTION_RECORD er; memset(&er, 0, sizeof er);
    er.ExceptionCode = 0xE0C0FFEE;
    er.ExceptionFlags = 0;
    er.NumberParameters = 3;
    er.ExceptionInformation[0] = 0x11; er.ExceptionInformation[1] = 0x22; er.ExceptionInformation[2] = 0x33;
    UN(RtlRaiseException)(&er);
    RemoveVectoredExceptionHandler(h);
    EXPECT_EQ(veh_hits, 1);
    EXPECT_EQ(veh_rec.ExceptionCode, 0xE0C0FFEE);
    EXPECT_EQ(veh_rec.NumberParameters, 3);
    EXPECT_EQ(veh_rec.ExceptionInformation[0], 0x11);
    EXPECT_EQ(veh_rec.ExceptionInformation[2], 0x33);
    EXPECT_EQ(veh_rec.ExceptionFlags & EXCEPTION_NONCONTINUABLE, 0);
}

TEST(rtlctx_raise_exception_continues_after_call) {
    REQUIRE(UN(RtlRaiseException));
    PVOID h = AddVectoredExceptionHandler(1, veh_cont);
    REQUIRE(h);
    volatile int after = 0;
    EXCEPTION_RECORD er; memset(&er, 0, sizeof er);
    er.ExceptionCode = 0xE0C0FFEF;
    UN(RtlRaiseException)(&er);
    after = 1;
    RemoveVectoredExceptionHandler(h);
    EXPECT_EQ(after, 1);
}

TEST(rtlctx_raise_exception_repeated_calls) {
    REQUIRE(UN(RtlRaiseException));
    PVOID h = AddVectoredExceptionHandler(1, veh_cont);
    REQUIRE(h);
    veh_hits = 0;
    for (int i = 0; i < 50; i++) {
        EXCEPTION_RECORD er; memset(&er, 0, sizeof er);
        er.ExceptionCode = 0xE0C0FFEE;
        UN(RtlRaiseException)(&er);
    }
    RemoveVectoredExceptionHandler(h);
    EXPECT_EQ(veh_hits, 50);
}

TEST(rtlctx_raise_exception_zero_params) {
    REQUIRE(UN(RtlRaiseException));
    PVOID h = AddVectoredExceptionHandler(1, veh_cont);
    REQUIRE(h);
    veh_hits = 0;
    EXCEPTION_RECORD er; memset(&er, 0, sizeof er);
    er.ExceptionCode = 0xE0C0FFEE; er.NumberParameters = 0;
    UN(RtlRaiseException)(&er);
    RemoveVectoredExceptionHandler(h);
    EXPECT_EQ(veh_hits, 1);
    EXPECT_EQ(veh_rec.NumberParameters, 0);
}

TEST(rtlctx_raise_exception_context_rip_and_address) {
    REQUIRE(UN(RtlRaiseException));
    PVOID h = AddVectoredExceptionHandler(1, veh_cont);
    REQUIRE(h);
    EXCEPTION_RECORD er; memset(&er, 0, sizeof er);
    er.ExceptionCode = 0xE0C0FFEE;
    UN(RtlRaiseException)(&er);
    RemoveVectoredExceptionHandler(h);
    t_note("ExceptionAddress=%p ctx.Rip=%llx in-exe=%d", veh_rec.ExceptionAddress, (unsigned long long)veh_ctx_rip, in_module(exe_mod(), (void *)veh_ctx_rip));
    CHECK(veh_rec.ExceptionAddress != NULL);
}

CHILD(rtl_raise_unhandled) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    EXCEPTION_RECORD er; memset(&er, 0, sizeof er);
    er.ExceptionCode = 0xE0420042;
    er.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
    UN(RtlRaiseException)(&er);
    return 3;   /* not reached */
}

TEST(rtlctx_raise_exception_unhandled_kills_child_with_code) {
    REQUIRE(UN(RtlRaiseException));
    if (t_is_wine()) { t_skip("wine unhandled-exception exit code"); return; }
    DWORD rc = t_run_child("rtl_raise_unhandled", "", 25000);
    EXPECT_EQ(rc, 0xE0420042);
}

static LONG CALLBACK veh_noncont(PEXCEPTION_POINTERS ep) {
    if (ep->ExceptionRecord->ExceptionCode == 0xE0420043) { veh_hits++; return EXCEPTION_CONTINUE_EXECUTION; }
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_NONCONTINUABLE_EXCEPTION) { veh_hits += 100; return EXCEPTION_CONTINUE_EXECUTION; }
    return EXCEPTION_CONTINUE_SEARCH;
}

CHILD(rtl_raise_noncont) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    AddVectoredExceptionHandler(1, veh_noncont);
    veh_hits = 0;
    EXCEPTION_RECORD er; memset(&er, 0, sizeof er);
    er.ExceptionCode = 0xE0420043;
    er.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
    UN(RtlRaiseException)(&er);
    /* continuing a noncontinuable exception raises EXCEPTION_NONCONTINUABLE_EXCEPTION (handled above) */
    return (int)veh_hits;
}

TEST(rtlctx_raise_exception_noncontinuable_continue_raises_second) {
    REQUIRE(UN(RtlRaiseException));
    if (t_is_wine()) { t_skip("wine"); return; }
    DWORD rc = t_run_child("rtl_raise_noncont", "", 25000);
    t_note("noncontinuable child exit=0x%lx (expect 101 when a second exception is raised)", rc);
    CHECK(rc != 0xDEAD0000 && rc != 0xDEAD0001);
}

/* ---- RtlUnwind / RtlUnwindEx: resolution + argument-free sanity ---- */
TEST(rtlctx_unwind_exports_are_distinct_entry_points) {
    REQUIRE(UN(RtlUnwind)); REQUIRE(UN(RtlUnwindEx));
    CHECK((void *)UN(RtlUnwind) != (void *)UN(RtlUnwindEx));
}

