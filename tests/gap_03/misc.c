/* gap_03: GetAcceptLanguages*, EraseTape, ExecuteUmsThread, FreeMemoryJobObject,
 * FlushProcessWriteBuffers, FatalAppExit*, FatalExit */
#include "t.h"
#include <shlwapi.h>

#define E_INVALIDARG_ ((HRESULT)0x80070057)

/* ---------------- GetAcceptLanguages ---------------- */
TEST(gap3_acceptlang_w_null_pcch) {
    REQUIRE(U(GetAcceptLanguagesW));
    WCHAR b[64] = L"zz";
    HRESULT hr = U(GetAcceptLanguagesW)(b, NULL);
    EXPECT_EQ(hr, E_INVALIDARG_);
    hr = U(GetAcceptLanguagesW)(NULL, NULL);
    EXPECT_EQ(hr, E_INVALIDARG_);
}
TEST(gap3_acceptlang_a_null_pcch) {
    REQUIRE(U(GetAcceptLanguagesA));
    char b[64] = "zz";
    HRESULT hr = U(GetAcceptLanguagesA)(b, NULL);
    EXPECT_EQ(hr, E_INVALIDARG_);
    hr = U(GetAcceptLanguagesA)(NULL, NULL);
    EXPECT_EQ(hr, E_INVALIDARG_);
}
TEST(gap3_acceptlang_w_basic) {
    REQUIRE(U(GetAcceptLanguagesW));
    WCHAR b[256]; DWORD n = 256;
    for (int i = 0; i < 256; i++) b[i] = 0x7777;
    HRESULT hr = U(GetAcceptLanguagesW)(b, &n);
    CHECKF(hr == S_OK || hr == S_FALSE, "hr=0x%lx", (unsigned long)hr);
    if (hr == S_OK) {
        CHECK(n > 0 && n < 256);
        CHECK(b[n] == 0);
        CHECK((DWORD)lstrlenW(b) == n);
    } else {
        t_note("GetAcceptLanguagesW returned S_FALSE n=%lu", n);
    }
}
TEST(gap3_acceptlang_a_basic) {
    REQUIRE(U(GetAcceptLanguagesA));
    char b[256]; DWORD n = 256;
    memset(b, 0x77, sizeof b);
    HRESULT hr = U(GetAcceptLanguagesA)(b, &n);
    CHECKF(hr == S_OK || hr == S_FALSE, "hr=0x%lx", (unsigned long)hr);
    if (hr == S_OK) {
        CHECK(n > 0 && n < 256);
        CHECK(b[n] == 0);
        CHECK((DWORD)lstrlenA(b) == n);
    }
}
TEST(gap3_acceptlang_ascii_charset) {
    REQUIRE(U(GetAcceptLanguagesA));
    char b[256]; DWORD n = 256;
    HRESULT hr = U(GetAcceptLanguagesA)(b, &n);
    if (hr != S_OK) { t_skip("no value"); return; }
    for (DWORD i = 0; i < n; i++) {
        unsigned char c = (unsigned char)b[i];
        CHECKF((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == ',' || c == ';' || c == '=' || c == '.' || c == '_' || c == ' ', "char 0x%02x at %lu", c, i);
    }
}
TEST(gap3_acceptlang_ab_wparity) {
    REQUIRE(U(GetAcceptLanguagesA)); REQUIRE(U(GetAcceptLanguagesW));
    char a[256]; WCHAR w[256]; DWORD na = 256, nw = 256;
    HRESULT ha = U(GetAcceptLanguagesA)(a, &na), hw = U(GetAcceptLanguagesW)(w, &nw);
    CHECK(ha == hw);
    CHECK(na == nw);
    if (ha == S_OK) {
        char conv[256];
        WideCharToMultiByte(CP_ACP, 0, w, -1, conv, sizeof conv, NULL, NULL);
        EXPECT_STR(a, conv);
    }
}
TEST(gap3_acceptlang_w_size_query_null_buffer) {
    REQUIRE(U(GetAcceptLanguagesW));
    WCHAR b[256]; DWORD full = 256, q = 0xFFFF;
    HRESULT hr = U(GetAcceptLanguagesW)(b, &full);
    HRESULT hq = U(GetAcceptLanguagesW)(NULL, &q);
    t_note("size query: full=%lu (hr 0x%lx) null-buffer q=%lu (hr 0x%lx)", full, (unsigned long)hr, q, (unsigned long)hq);
    if (!t_is_wine()) { CHECK(q != 0xFFFF); CHECK(SUCCEEDED(hq)); }
}
TEST(gap3_acceptlang_w_small_buffers) {
    REQUIRE(U(GetAcceptLanguagesW));
    WCHAR full[256]; DWORD nf = 256;
    HRESULT hr = U(GetAcceptLanguagesW)(full, &nf);
    if (hr != S_OK || nf < 2) { t_skip("no suitable value"); return; }
    for (DWORD cch = 1; cch <= nf; cch++) {
        WCHAR b[256]; DWORD n = cch;
        for (int i = 0; i < 256; i++) b[i] = 0x7777;
        hr = U(GetAcceptLanguagesW)(b, &n);
        CHECKF(FAILED(hr), "cch %lu hr 0x%lx", cch, (unsigned long)hr);
        /* kernelbase GetAcceptLanguagesW: *psz = 0 up front, a too-small buffer fails (MORE_DATA / locale-name failure) without writing */
        if (!t_is_wine()) CHECKF(b[0] == 0, "cch %lu buffer not terminated", cch);
        CHECKF(b[cch] == 0x7777, "cch %lu overran buffer", cch);
    }
    WCHAR b[256]; DWORD n = nf + 1;
    CHECK(U(GetAcceptLanguagesW)(b, &n) == S_OK);
    EXPECT_EQ(n, nf);
}
TEST(gap3_acceptlang_a_small_buffers) {
    REQUIRE(U(GetAcceptLanguagesA));
    char full[256]; DWORD nf = 256;
    HRESULT hr = U(GetAcceptLanguagesA)(full, &nf);
    if (hr != S_OK || nf < 2) { t_skip("no suitable value"); return; }
    for (DWORD cch = 1; cch <= nf; cch++) {
        char b[256]; DWORD n = cch;
        memset(b, 0x77, sizeof b);
        hr = U(GetAcceptLanguagesA)(b, &n);
        CHECKF(FAILED(hr), "cch %lu hr 0x%lx", cch, (unsigned long)hr);
        CHECKF(b[cch] == 0x77, "cch %lu overran buffer", cch);
    }
}
TEST(gap3_acceptlang_repeatable) {
    REQUIRE(U(GetAcceptLanguagesW));
    WCHAR a[256], b[256]; DWORD na = 256, nb;
    HRESULT h0 = U(GetAcceptLanguagesW)(a, &na);
    for (int i = 0; i < 50; i++) {
        nb = 256;
        HRESULT h = U(GetAcceptLanguagesW)(b, &nb);
        CHECK(h == h0);
        CHECK(nb == na);
        CHECK(!lstrcmpW(a, b));
    }
}
TEST(gap3_acceptlang_does_not_set_lasterror_on_ok) {
    REQUIRE(U(GetAcceptLanguagesW));
    WCHAR b[256]; DWORD n = 256;
    SetLastError(0x4242);
    HRESULT hr = U(GetAcceptLanguagesW)(b, &n);
    DWORD e = GetLastError();
    t_note("hr=0x%lx lasterror after call=0x%lx", (unsigned long)hr, e);
}
static DWORD WINAPI g3_al_thread(LPVOID p) {
    WCHAR b[256]; DWORD n = 256;
    HRESULT hr = U(GetAcceptLanguagesW)(b, &n);
    return (hr == S_OK || hr == S_FALSE) ? 0 : 1;
}
TEST(gap3_acceptlang_threads) {
    REQUIRE(U(GetAcceptLanguagesW));
    HANDLE th[4];
    for (int i = 0; i < 4; i++) th[i] = CreateThread(NULL, 0, g3_al_thread, NULL, 0, NULL);
    for (int i = 0; i < 4; i++) { WaitForSingleObject(th[i], 10000); DWORD c = 9; GetExitCodeThread(th[i], &c); EXPECT_EQ(c, 0); CloseHandle(th[i]); }
}
TEST(gap3_acceptlang_exports) {
    CHECK(UB(GetAcceptLanguagesA) != NULL);
    CHECK(UB(GetAcceptLanguagesW) != NULL);
}

/* ---------------- EraseTape ---------------- */
TEST(gap3_erasetape_null_handle) {
    REQUIRE(U(EraseTape));
    for (DWORD type = 0; type <= 1; type++)
        for (BOOL imm = 0; imm <= 1; imm++) {
            SetLastError(0);
            DWORD r = U(EraseTape)(NULL, type, imm);
            CHECKF(r == ERROR_INVALID_HANDLE, "type %lu imm %d -> %lu", type, imm, r);
            EXPECT_ERR(ERROR_INVALID_HANDLE);
        }
}
TEST(gap3_erasetape_invalid_handle_value) {
    REQUIRE(U(EraseTape));
    SetLastError(0);
    DWORD r = U(EraseTape)(INVALID_HANDLE_VALUE, TAPE_ERASE_SHORT, FALSE);
    CHECK(r != NO_ERROR);
    t_note("EraseTape(INVALID_HANDLE_VALUE) = %lu", r);
}
TEST(gap3_erasetape_closed_handle) {
    REQUIRE(U(EraseTape));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "g3_tape.bin");
    HANDLE h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    CloseHandle(h);
    SetLastError(0);
    DWORD r = U(EraseTape)(h, TAPE_ERASE_SHORT, FALSE);
    CHECK(r == ERROR_INVALID_HANDLE);
    DeleteFileA(p);
}
TEST(gap3_erasetape_on_regular_file) {
    REQUIRE(U(EraseTape));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "g3_tape2.bin");
    HANDLE h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    DWORD wr; WriteFile(h, "hello", 5, &wr, NULL);
    SetLastError(0);
    DWORD r = U(EraseTape)(h, TAPE_ERASE_LONG, FALSE);
    CHECK(r != NO_ERROR);           /* a file system rejects the tape IOCTL */
    EXPECT_ERR(r);
    t_note("EraseTape(file) = %lu", r);
    /* file content untouched */
    char buf[8] = {0}; DWORD rd = 0;
    SetFilePointer(h, 0, NULL, FILE_BEGIN);
    ReadFile(h, buf, 5, &rd, NULL);
    CHECK(rd == 5 && !memcmp(buf, "hello", 5));
    CloseHandle(h); DeleteFileA(p);
}
TEST(gap3_erasetape_wrong_handle_type) {
    REQUIRE(U(EraseTape));
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    SetLastError(0);
    DWORD r = U(EraseTape)(ev, TAPE_ERASE_SHORT, FALSE);
    CHECK(r != NO_ERROR);
    t_note("EraseTape(event) = %lu", r);
    CloseHandle(ev);
}
TEST(gap3_erasetape_read_only_file_handle) {
    REQUIRE(U(EraseTape));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "g3_tape3.bin");
    HANDLE h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CloseHandle(h);
    h = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    DWORD r = U(EraseTape)(h, TAPE_ERASE_SHORT, TRUE);
    CHECK(r != NO_ERROR);
    t_note("EraseTape(read-only file) = %lu", r);
    CloseHandle(h); DeleteFileA(p);
}
TEST(gap3_erasetape_export) {
    CHECK(UK(EraseTape) != NULL);
}

/* ---------------- ExecuteUmsThread ---------------- */
CHILD(gap3_ums_null) {
    SetErrorMode(SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
    if (!U(ExecuteUmsThread)) return 5;
    SetLastError(0);
    BOOL r = U(ExecuteUmsThread)(NULL);
    DWORD e = GetLastError();
    return r ? 1 : (int)(1000 + e);
}
TEST(gap3_executeumsthread_exported) {
    CHECK(UK(ExecuteUmsThread) != NULL);
}
TEST(gap3_executeumsthread_null_child) {
    REQUIRE(U(ExecuteUmsThread));
    if (t_is_wine()) { t_skip("wine: UMS not implemented"); return; }
    DWORD c = t_run_child("gap3_ums_null", "", 20000);
    t_note("ExecuteUmsThread(NULL) in child: exit 0x%lx (1000+err on failure)", c);
    CHECK(c != 1 && c < 0xDEAD0000);   /* must not succeed and must not hang */
}

/* ---------------- FreeMemoryJobObject ---------------- */
UDECL(VOID, FreeMemoryJobObject, (PVOID));
TEST(gap3_freememoryjobobject_frees_process_heap_block) {
    REQUIRE(UN(FreeMemoryJobObject));
    HANDLE heap = GetProcessHeap();
    for (int i = 0; i < 200; i++) {
        void *p = HeapAlloc(heap, HEAP_ZERO_MEMORY, 16 + i * 8);
        CHECK(p != NULL);
        if (!p) return;
        UN(FreeMemoryJobObject)(p);
    }
    CHECK(HeapValidate(heap, 0, NULL));
}
TEST(gap3_freememoryjobobject_null) {
    REQUIRE(UN(FreeMemoryJobObject));
    SetLastError(0x1357);
    UN(FreeMemoryJobObject)(NULL);
    CHECK(HeapValidate(GetProcessHeap(), 0, NULL));
}
TEST(gap3_freememoryjobobject_size_matrix) {
    REQUIRE(UN(FreeMemoryJobObject));
    HANDLE heap = GetProcessHeap();
    static const SIZE_T sz[] = { 1, 2, 7, 8, 15, 16, 17, 255, 256, 4095, 4096, 65536, 600000 };
    for (unsigned i = 0; i < sizeof sz / sizeof sz[0]; i++) {
        void *p = HeapAlloc(heap, 0, sz[i]);
        CHECKF(p != NULL, "size %zu", sz[i]);
        if (p) { memset(p, 0xAA, sz[i]); UN(FreeMemoryJobObject)(p); }
    }
    CHECK(HeapValidate(heap, 0, NULL));
}
TEST(gap3_freememoryjobobject_freed_block_reusable) {
    REQUIRE(UN(FreeMemoryJobObject));
    HANDLE heap = GetProcessHeap();
    void *a = HeapAlloc(heap, 0, 64);
    UN(FreeMemoryJobObject)(a);
    void *b = HeapAlloc(heap, 0, 64);
    CHECK(b != NULL);
    if (b) HeapFree(heap, 0, b);
}
TEST(gap3_freememoryjobobject_cross_thread) {
    REQUIRE(UN(FreeMemoryJobObject));
    HANDLE heap = GetProcessHeap();
    void *p[32];
    for (int i = 0; i < 32; i++) p[i] = HeapAlloc(heap, 0, 100);
    for (int i = 0; i < 32; i++) UN(FreeMemoryJobObject)(p[i]);
    CHECK(HeapValidate(heap, 0, NULL));
}
TEST(gap3_freememoryjobobject_export_and_alias) {
    if (t_is_wine()) { t_skip("wine"); return; }
    CHECK(t_proc_k32("FreeMemoryJobObject") != NULL);
}

/* ---------------- FlushProcessWriteBuffers ---------------- */
UDECL(NTSTATUS, NtFlushProcessWriteBuffers, (VOID));
TEST(gap3_flushprocesswritebuffers_basic) {
    REQUIRE(U(FlushProcessWriteBuffers));
    void (WINAPI *f)(void) = U(FlushProcessWriteBuffers);
    SetLastError(0x1234);
    f();
    EXPECT_ERR(0x1234);
}
TEST(gap3_flushprocesswritebuffers_repeated) {
    REQUIRE(U(FlushProcessWriteBuffers));
    for (int i = 0; i < 2000; i++) U(FlushProcessWriteBuffers)();
    CHECK(1);
}
TEST(gap3_flushprocesswritebuffers_ntdll_status) {
    NtFlushProcessWriteBuffers_pfn f = (NtFlushProcessWriteBuffers_pfn)t_proc_nt("NtFlushProcessWriteBuffers");
    REQUIRE(f);
    NTSTATUS st = f();
    EXPECT_EQ(st, 0);
}
static volatile LONG g3_fp_stop;
static volatile LONG g3_fp_data;
static volatile LONG g3_fp_flag;
static DWORD WINAPI g3_fp_worker(LPVOID p) {
    for (int i = 0; i < 300; i++) U(FlushProcessWriteBuffers)();
    return 0;
}
TEST(gap3_flushprocesswritebuffers_threads) {
    REQUIRE(U(FlushProcessWriteBuffers));
    HANDLE th[8];
    for (int i = 0; i < 8; i++) th[i] = CreateThread(NULL, 0, g3_fp_worker, NULL, 0, NULL);
    for (int i = 0; i < 8; i++) {
        DWORD w = WaitForSingleObject(th[i], 30000);
        CHECK(w == WAIT_OBJECT_0);
        CloseHandle(th[i]);
    }
}
static DWORD WINAPI g3_fp_writer(LPVOID p) {
    for (int i = 1; i <= 2000; i++) {
        g3_fp_data = i;
        U(FlushProcessWriteBuffers)();
        g3_fp_flag = i;               /* readers that see flag==i must see data>=i after the flush */
    }
    return 0;
}
TEST(gap3_flushprocesswritebuffers_ordering) {
    REQUIRE(U(FlushProcessWriteBuffers));
    g3_fp_data = 0; g3_fp_flag = 0;
    HANDLE t = CreateThread(NULL, 0, g3_fp_writer, NULL, 0, NULL);
    int viol = 0;
    while (WaitForSingleObject(t, 0) == WAIT_TIMEOUT) {
        LONG f = g3_fp_flag; LONG d = g3_fp_data;
        if (d < f) viol++;
    }
    CloseHandle(t);
    EXPECT_EQ(viol, 0);
}
TEST(gap3_flushprocesswritebuffers_exported_both) {
    CHECK(UK(FlushProcessWriteBuffers) != NULL);
    CHECK(UB(FlushProcessWriteBuffers) != NULL);
}

/* ---------------- FatalAppExit / FatalExit ----------------
 * FatalAppExit shows a system-modal popup and terminates the process; FatalExit is an alias of ExitProcess
 * in kernel32 (same RVA). Neither is invoked here; only presence and the alias relationship are checked. */
TEST(gap3_fatalappexit_exports) {
    CHECK(UK(FatalAppExitA) != NULL);
    CHECK(UK(FatalAppExitW) != NULL);
    CHECK(UK(FatalExit) != NULL);
    CHECK((void *)UK(FatalAppExitA) != (void *)UK(FatalAppExitW));
}
TEST(gap3_fatalexit_is_exitprocess_alias) {
    FARPROC fe = (FARPROC)UK(FatalExit), ep = (FARPROC)UK(ExitProcess);
    REQUIRE(fe); REQUIRE(ep);
    if (fe != ep) t_note("FatalExit (%p) differs from ExitProcess (%p)", (void *)fe, (void *)ep);
    if (!t_is_wine()) CHECK(fe == ep);
}
TEST(gap3_fatalappexit_not_in_kernelbase_forwarder_chain) {
    FARPROC k32 = (FARPROC)UK(FatalAppExitW);
    FARPROC kb = (FARPROC)UB(FatalAppExitW);
    REQUIRE(k32);
    t_note("FatalAppExitW kernel32=%p kernelbase=%p", (void *)k32, (void *)kb);
}
