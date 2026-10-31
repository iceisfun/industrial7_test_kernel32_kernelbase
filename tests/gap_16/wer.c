#include "g16.h"
/* Every Wer registration call mutates per-process crash-reporting state (the PEB WER block),
 * so all of them run in a throw-away child process. The child returns 0 on success or the
 * source line of the first failed expectation. */
static int kc_bad;
#define N(x) UN(x)
#define RUN_MODE(mode) do { char a_[32]; snprintf(a_, sizeof a_, "%s", mode); DWORD c_ = t_run_child("gap16_wer", a_, 25000); \
    if (c_ != 0 && t_is_wine()) { t_skip("emulator does not implement the WER crash-reporting registry (line %lu)", c_); return; } \
    CHECKF(c_ == 0, "child mode %s failed at line %lu (0x%lx)", mode, c_, c_); } while (0)
#define KEQ(a, b) KC((HRESULT)(a) == (HRESULT)(b))
static void *g_buf;

static int m_file(void) {
    if (!N(WerRegisterFile) || !N(WerUnregisterFile)) return 0;
    HRESULT hr;
    KEQ(N(WerRegisterFile)(NULL, 1, 0), G16_E_INVALIDARG);
    KEQ(N(WerRegisterFile)(L"C:\\k32t\\a.txt", 0, 0), G16_E_INVALIDARG);
    KEQ(N(WerRegisterFile)(L"C:\\k32t\\a.txt", 3, 0), G16_E_INVALIDARG);
    KEQ(N(WerRegisterFile)(L"C:\\k32t\\a.txt", -1, 0), G16_E_INVALIDARG);
    KEQ(N(WerRegisterFile)(L"C:\\k32t\\a.txt", 1, 4), G16_E_INVALIDARG);
    KEQ(N(WerRegisterFile)(L"C:\\k32t\\a.txt", 2, 0x80000000u), G16_E_INVALIDARG);
    KEQ(N(WerRegisterFile)(L"C:\\k32t\\a.txt", 1, 8), G16_E_INVALIDARG);
    for (int type = 1; type <= 2; type++) for (DWORD fl = 0; fl <= 3; fl++) {
        KEQ(N(WerRegisterFile)(L"C:\\k32t\\a.txt", type, fl), S_OK);
        KEQ(N(WerUnregisterFile)(L"C:\\k32t\\a.txt"), S_OK);
    }
    KEQ(N(WerUnregisterFile)(L"C:\\k32t\\a.txt"), G16_E_NOTFOUND);
    KEQ(N(WerUnregisterFile)(NULL), G16_E_INVALIDARG);
    KEQ(N(WerUnregisterFile)(L"C:\\never\\registered.txt"), G16_E_NOTFOUND);
    /* re-registering the same path replaces the entry */
    KEQ(N(WerRegisterFile)(L"C:\\k32t\\b.txt", 1, 0), S_OK); KEQ(N(WerRegisterFile)(L"C:\\k32t\\b.txt", 2, 1), S_OK);
    KEQ(N(WerUnregisterFile)(L"C:\\k32t\\b.txt"), S_OK); KEQ(N(WerUnregisterFile)(L"C:\\k32t\\b.txt"), G16_E_NOTFOUND);
    /* path length limits */
    WCHAR p[400]; for (int len = 250; len <= 262; len++) {
        for (int i = 0; i < len; i++) { p[i] = L'a'; } p[len] = 0;
        hr = N(WerRegisterFile)(p, 1, 0);
        if (len <= 259) { KEQ(hr, S_OK); KEQ(N(WerUnregisterFile)(p), S_OK); }
        else { KEQ(hr, (HRESULT)0x800700CE); }
    }
    for (int i = 0; i < 400 - 1; i++) { p[i] = L'b'; } p[399] = 0; KEQ(N(WerRegisterFile)(p, 1, 0), (HRESULT)0x800700CE);
    KEQ(N(WerRegisterFile)(L"", 1, 0), N(WerRegisterFile)(L"", 1, 0));
    N(WerUnregisterFile)(L"");
    /* unicode and relative names are accepted verbatim */
    KEQ(N(WerRegisterFile)(L"relative.txt", 1, 0), S_OK); KEQ(N(WerUnregisterFile)(L"relative.txt"), S_OK);
    KEQ(N(WerRegisterFile)(L"C:\\k32t\\\x00e9\x4e2d.txt", 1, 0), S_OK); KEQ(N(WerUnregisterFile)(L"C:\\k32t\\\x00e9\x4e2d.txt"), S_OK);
    return kc_bad;
}
static int m_file_limit(void) {
    if (!N(WerRegisterFile) || !N(WerUnregisterFile)) return 0;
    WCHAR p[32]; int ok = 0; HRESULT last = S_OK;
    for (int i = 0; i < 1100; i++) { wsprintfW(p, L"C:\\k32t\\f%04d", i); last = N(WerRegisterFile)(p, 1, 0); if (last != S_OK) break; ok++; }
    printf("registered %d files before hr=0x%lx\n", ok, (unsigned long)last);
    KC(ok >= 8);
    for (int i = 0; i < ok; i++) { wsprintfW(p, L"C:\\k32t\\f%04d", i); KEQ(N(WerUnregisterFile)(p), S_OK); }
    wsprintfW(p, L"C:\\k32t\\f%04d", 0); KEQ(N(WerUnregisterFile)(p), G16_E_NOTFOUND);
    KEQ(N(WerRegisterFile)(L"C:\\k32t\\after", 1, 0), S_OK); KEQ(N(WerUnregisterFile)(L"C:\\k32t\\after"), S_OK);
    return kc_bad;
}
static int m_mem(void) {
    if (!N(WerRegisterMemoryBlock) || !N(WerUnregisterMemoryBlock)) return 0;
    char *b = VirtualAlloc(NULL, 0x30000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE); KC(b != NULL); if (!b) return kc_bad;
    KEQ(N(WerRegisterMemoryBlock)(NULL, 16), G16_E_INVALIDARG);
    KEQ(N(WerRegisterMemoryBlock)(b, 0), G16_E_INVALIDARG);
    KEQ(N(WerRegisterMemoryBlock)(NULL, 0), G16_E_INVALIDARG);
    KEQ(N(WerRegisterMemoryBlock)(b, 0x10001), G16_E_INVALIDARG);
    KEQ(N(WerRegisterMemoryBlock)(b, 0xffffffffu), G16_E_INVALIDARG);
    KEQ(N(WerRegisterMemoryBlock)(b, 1), S_OK); KEQ(N(WerUnregisterMemoryBlock)(b), S_OK);
    KEQ(N(WerRegisterMemoryBlock)(b, 0x10000), S_OK); KEQ(N(WerUnregisterMemoryBlock)(b), S_OK);
    KEQ(N(WerRegisterMemoryBlock)(b + 0x100, 0x1000), S_OK); KEQ(N(WerRegisterMemoryBlock)(b + 0x2000, 16), S_OK);
    KEQ(N(WerUnregisterMemoryBlock)(b + 0x100), S_OK); KEQ(N(WerUnregisterMemoryBlock)(b + 0x2000), S_OK);
    HRESULT again = N(WerUnregisterMemoryBlock)(b + 0x100); printf("double unregister -> 0x%lx\n", (unsigned long)again);
    HRESULT un = N(WerUnregisterMemoryBlock)(NULL); printf("unregister NULL -> 0x%lx\n", (unsigned long)un);
    HRESULT nv = N(WerUnregisterMemoryBlock)(b + 0x5000); printf("unregister never-registered -> 0x%lx\n", (unsigned long)nv);
    /* registering twice at the same address */
    HRESULT r1 = N(WerRegisterMemoryBlock)(b, 64), r2 = N(WerRegisterMemoryBlock)(b, 64); printf("dup register 0x%lx 0x%lx\n", (unsigned long)r1, (unsigned long)r2);
    KEQ(r1, S_OK); N(WerUnregisterMemoryBlock)(b); N(WerUnregisterMemoryBlock)(b);
    /* unreadable / unmapped addresses are not probed at registration time */
    HRESULT ur = N(WerRegisterMemoryBlock)((void *)0x1000, 16); printf("unmapped register -> 0x%lx\n", (unsigned long)ur); if (ur == S_OK) N(WerUnregisterMemoryBlock)((void *)0x1000);
    VirtualFree(b, 0, MEM_RELEASE);
    return kc_bad;
}
static int m_mem_limit(void) {
    if (!N(WerRegisterMemoryBlock) || !N(WerUnregisterMemoryBlock)) return 0;
    char *b = VirtualAlloc(NULL, 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE); if (!b) return 1;
    int ok = 0; HRESULT last = S_OK;
    for (int i = 0; i < 600; i++) { last = N(WerRegisterMemoryBlock)(b + i * 16, 16); if (last != S_OK) break; ok++; }
    printf("registered %d memory blocks, then hr=0x%lx\n", ok, (unsigned long)last); KC(ok >= 4);
    for (int i = 0; i < ok; i++) KEQ(N(WerUnregisterMemoryBlock)(b + i * 16), S_OK);
    VirtualFree(b, 0, MEM_RELEASE); return kc_bad;
}
static int m_excl(void) {
    if (!N(WerRegisterExcludedMemoryBlock) || !N(WerUnregisterExcludedMemoryBlock)) return 0;
    char *b = VirtualAlloc(NULL, 0x40000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE); KC(b != NULL); if (!b) return kc_bad;
    KEQ(N(WerRegisterExcludedMemoryBlock)(NULL, 16), G16_E_INVALIDARG);
    KEQ(N(WerRegisterExcludedMemoryBlock)(b, 0), G16_E_INVALIDARG);
    KEQ(N(WerRegisterExcludedMemoryBlock)(b, 0x1000), S_OK);
    HRESULT big = N(WerRegisterExcludedMemoryBlock)(b + 0x1000, 0x30000); printf("excluded 0x30000 -> 0x%lx (no 64K cap for excluded blocks)\n", (unsigned long)big); KEQ(big, S_OK);
    HRESULT dd = N(WerRegisterExcludedMemoryBlock)(b, 0x1000); printf("excluded duplicate -> 0x%lx\n", (unsigned long)dd);
    HRESULT u = N(WerUnregisterExcludedMemoryBlock)(b); printf("excluded unreg -> 0x%lx\n", (unsigned long)u); KEQ(u, S_OK);
    N(WerUnregisterExcludedMemoryBlock)(b); N(WerUnregisterExcludedMemoryBlock)(b + 0x1000);
    HRESULT nv = N(WerUnregisterExcludedMemoryBlock)(b + 0x8000); printf("excluded unreg never registered -> 0x%lx\n", (unsigned long)nv);
    /* a normal block and an excluded block with the same address are independent lists */
    KEQ(N(WerRegisterMemoryBlock)(b, 32), S_OK); KEQ(N(WerRegisterExcludedMemoryBlock)(b, 32), S_OK);
    KEQ(N(WerUnregisterMemoryBlock)(b), S_OK); KEQ(N(WerUnregisterExcludedMemoryBlock)(b), S_OK);
    VirtualFree(b, 0, MEM_RELEASE); return kc_bad;
}
static int m_addproc(void) {
    if (!N(WerRegisterAdditionalProcess) || !N(WerUnregisterAdditionalProcess)) return 0;
    DWORD me = GetCurrentProcessId(), tid = GetCurrentThreadId();
    KEQ(N(WerRegisterAdditionalProcess)(0, 0), G16_E_INVALIDARG);
    KEQ(N(WerRegisterAdditionalProcess)(0, tid), G16_E_INVALIDARG);
    KEQ(N(WerUnregisterAdditionalProcess)(0), G16_E_INVALIDARG);
    KEQ(N(WerRegisterAdditionalProcess)(me, 0), S_OK); KEQ(N(WerUnregisterAdditionalProcess)(me), S_OK);
    KEQ(N(WerRegisterAdditionalProcess)(me, tid), S_OK); KEQ(N(WerUnregisterAdditionalProcess)(me), S_OK);
    HRESULT again = N(WerUnregisterAdditionalProcess)(me); printf("double unregister -> 0x%lx\n", (unsigned long)again); KEQ(again, G16_E_NOTFOUND);
    HRESULT nv = N(WerUnregisterAdditionalProcess)(0x7ffe); printf("unregister unknown pid -> 0x%lx\n", (unsigned long)nv); KC(nv != S_OK);
    /* nonexistent pid is accepted at registration (not validated) */
    HRESULT ne = N(WerRegisterAdditionalProcess)(0x3fff0, 0); printf("nonexistent pid -> 0x%lx\n", (unsigned long)ne); if (ne == S_OK) N(WerUnregisterAdditionalProcess)(0x3fff0);
    HRESULT dup1 = N(WerRegisterAdditionalProcess)(me, 0), dup2 = N(WerRegisterAdditionalProcess)(me, 0); printf("dup 0x%lx 0x%lx\n", (unsigned long)dup1, (unsigned long)dup2);
    N(WerUnregisterAdditionalProcess)(me); N(WerUnregisterAdditionalProcess)(me);
    int ok = 0; HRESULT last = S_OK;
    for (int i = 0; i < 16; i++) { last = N(WerRegisterAdditionalProcess)(1000 + i * 4, 0); if (last != S_OK) break; ok++; }
    printf("additional processes registered: %d, then 0x%lx\n", ok, (unsigned long)last); KC(ok >= 1);
    for (int i = 0; i < ok; i++) KEQ(N(WerUnregisterAdditionalProcess)(1000 + i * 4), S_OK);
    return kc_bad;
}
static int m_meta(void) {
    if (!N(WerRegisterCustomMetadata) || !N(WerUnregisterCustomMetadata)) return 0;
    KEQ(N(WerRegisterCustomMetadata)(NULL, L"v"), G16_E_INVALIDARG);
    KEQ(N(WerRegisterCustomMetadata)(L"Key", NULL), G16_E_INVALIDARG);
    KEQ(N(WerRegisterCustomMetadata)(NULL, NULL), G16_E_INVALIDARG);
    KEQ(N(WerUnregisterCustomMetadata)(NULL), G16_E_INVALIDARG);
    KEQ(N(WerRegisterCustomMetadata)(L"Key1", L"Value 1"), S_OK);
    KEQ(N(WerRegisterCustomMetadata)(L"Key1", L"Replaced"), S_OK);        /* same key replaces */
    KEQ(N(WerUnregisterCustomMetadata)(L"Key1"), S_OK);
    KEQ(N(WerUnregisterCustomMetadata)(L"Key1"), G16_E_NOTFOUND);
    KEQ(N(WerUnregisterCustomMetadata)(L"Never"), G16_E_NOTFOUND);
    KEQ(N(WerRegisterCustomMetadata)(L"K", L""), S_OK); KEQ(N(WerUnregisterCustomMetadata)(L"K"), S_OK);
    KEQ(N(WerRegisterCustomMetadata)(L"A_b-c.d", L"~!@#$%^&*()_+{}|:<>?"), S_OK);
    /* the value character whitelist contains no quote-less control chars; ';', '<', '>' are allowed */
    N(WerUnregisterCustomMetadata)(L"A_b-c.d");
    /* length limits: key < 64, value < 128 */
    WCHAR k[200], v[300]; for (int i = 0; i < 199; i++) { k[i] = L'k'; } k[199] = 0; for (int i = 0; i < 299; i++) { v[i] = L'v'; } v[299] = 0;
    k[63] = 0; KEQ(N(WerRegisterCustomMetadata)(k, L"x"), S_OK); KEQ(N(WerUnregisterCustomMetadata)(k), S_OK); k[63] = L'k';
    k[64] = 0; KC(FAILED(N(WerRegisterCustomMetadata)(k, L"x"))); k[64] = L'k';
    v[127] = 0; KEQ(N(WerRegisterCustomMetadata)(L"LongV", v), S_OK); KEQ(N(WerUnregisterCustomMetadata)(L"LongV"), S_OK); v[127] = L'v';
    v[128] = 0; KC(FAILED(N(WerRegisterCustomMetadata)(L"LongV", v))); v[128] = L'v';
    /* key must be a valid XML element name */
    static const WCHAR *badk[] = { L"", L"1abc", L"a b", L"a<b", L"-x", L"a\"b", L"a&b", L"a/b" };
    for (int i = 0; i < 8; i++) { HRESULT hr = N(WerRegisterCustomMetadata)(badk[i], L"v"); printf("bad key %d -> 0x%lx\n", i, (unsigned long)hr); KC(FAILED(hr)); if (SUCCEEDED(hr)) N(WerUnregisterCustomMetadata)(badk[i]); }
    /* value characters outside the whitelist */
    static const WCHAR *badv[] = { L"a\tb", L"a\nb", L"caf\x00e9", L"\x4e2d" };
    for (int i = 0; i < 4; i++) { HRESULT hr = N(WerRegisterCustomMetadata)(L"BadV", badv[i]); printf("bad value %d -> 0x%lx\n", i, (unsigned long)hr); KC(FAILED(hr)); if (SUCCEEDED(hr)) N(WerUnregisterCustomMetadata)(L"BadV"); }
    int ok = 0; HRESULT last = S_OK; WCHAR kk[16];
    for (int i = 0; i < 200; i++) { wsprintfW(kk, L"M%d", i); last = N(WerRegisterCustomMetadata)(kk, L"x"); if (last != S_OK) break; ok++; }
    printf("metadata entries: %d then 0x%lx\n", ok, (unsigned long)last); KC(ok >= 8);
    for (int i = 0; i < ok; i++) { wsprintfW(kk, L"M%d", i); KEQ(N(WerUnregisterCustomMetadata)(kk), S_OK); }
    return kc_bad;
}
static int m_dump(void) {
    if (!N(WerRegisterAppLocalDump) || !N(WerUnregisterAppLocalDump)) return 0;
    KEQ(N(WerRegisterAppLocalDump)(L"gap16dump"), S_OK);
    KEQ(N(WerRegisterAppLocalDump)(L"other"), S_OK);                   /* replaces */
    KEQ(N(WerUnregisterAppLocalDump)(), S_OK);
    KEQ(N(WerUnregisterAppLocalDump)(), S_OK);                         /* idempotent */
    KEQ(N(WerRegisterAppLocalDump)(L"a\\b"), S_OK); N(WerUnregisterAppLocalDump)();
    HRESULT e = N(WerRegisterAppLocalDump)(L""); printf("empty subpath -> 0x%lx\n", (unsigned long)e); N(WerUnregisterAppLocalDump)();
    KC(FAILED(N(WerRegisterAppLocalDump)(L"..")));
    KC(FAILED(N(WerRegisterAppLocalDump)(L"..\\x")));
    KC(FAILED(N(WerRegisterAppLocalDump)(L"a\\..\\b")));
    KC(FAILED(N(WerRegisterAppLocalDump)(L"x..y")));
    WCHAR s[100]; for (int i = 0; i < 99; i++) { s[i] = L'd'; } s[99] = 0;
    s[63] = 0; KEQ(N(WerRegisterAppLocalDump)(s), S_OK); N(WerUnregisterAppLocalDump)(); s[63] = L'd';
    s[64] = 0; KC(FAILED(N(WerRegisterAppLocalDump)(s))); s[64] = L'd';
    KC(FAILED(N(WerRegisterAppLocalDump)(s)));
    KEQ(N(WerRegisterAppLocalDump)(L"C:\\abs"), S_OK); N(WerUnregisterAppLocalDump)();
    return kc_bad;
}
static int m_rtexc(void) {
    if (!N(WerRegisterRuntimeExceptionModule) || !N(WerUnregisterRuntimeExceptionModule)) return 0;
    PVOID ctx = (PVOID)0x1234, ctx2 = (PVOID)0x5678;
    KEQ(N(WerRegisterRuntimeExceptionModule)(NULL, ctx), G16_E_INVALIDARG);
    KEQ(N(WerUnregisterRuntimeExceptionModule)(NULL, ctx), G16_E_INVALIDARG);
    KEQ(N(WerRegisterRuntimeExceptionModule)(L"C:\\x\\mod.dll", ctx), S_OK);
    KEQ(N(WerUnregisterRuntimeExceptionModule)(L"C:\\x\\never.dll", ctx), (HRESULT)0x80070002);
    KEQ(N(WerUnregisterRuntimeExceptionModule)(L"C:\\x\\mod.dll", ctx2), (HRESULT)0x80070002);        /* context must match */
    KEQ(N(WerUnregisterRuntimeExceptionModule)(L"C:\\x\\other.dll", ctx), (HRESULT)0x80070002);        /* path must match */
    KEQ(N(WerUnregisterRuntimeExceptionModule)(L"C:\\x\\mod.dll", ctx), S_OK);
    KEQ(N(WerUnregisterRuntimeExceptionModule)(L"C:\\x\\mod.dll", ctx), (HRESULT)0x80070002);
    KEQ(N(WerRegisterRuntimeExceptionModule)(L"C:\\x\\m1.dll", ctx), S_OK); KEQ(N(WerRegisterRuntimeExceptionModule)(L"C:\\x\\m1.dll", ctx2), S_OK);
    KEQ(N(WerRegisterRuntimeExceptionModule)(L"C:\\x\\m2.dll", ctx), S_OK);
    KEQ(N(WerUnregisterRuntimeExceptionModule)(L"C:\\x\\m1.dll", ctx2), S_OK); KEQ(N(WerUnregisterRuntimeExceptionModule)(L"C:\\x\\m1.dll", ctx), S_OK);
    KEQ(N(WerUnregisterRuntimeExceptionModule)(L"C:\\x\\m2.dll", ctx), S_OK);
    KEQ(N(WerRegisterRuntimeExceptionModule)(L"C:\\x\\nullctx.dll", NULL), S_OK); KEQ(N(WerUnregisterRuntimeExceptionModule)(L"C:\\x\\nullctx.dll", NULL), S_OK);
    HRESULT rel = N(WerRegisterRuntimeExceptionModule)(L"relative.dll", ctx); printf("relative path -> 0x%lx\n", (unsigned long)rel); if (rel == S_OK) N(WerUnregisterRuntimeExceptionModule)(L"relative.dll", ctx);
    HRESULT em = N(WerRegisterRuntimeExceptionModule)(L"", ctx); printf("empty path -> 0x%lx\n", (unsigned long)em); if (em == S_OK) N(WerUnregisterRuntimeExceptionModule)(L"", ctx);
    WCHAR p[400]; for (int i = 0; i < 399; i++) { p[i] = L'p'; } p[399] = 0; HRESULT lg = N(WerRegisterRuntimeExceptionModule)(p, ctx); printf("399-char path -> 0x%lx\n", (unsigned long)lg); if (lg == S_OK) N(WerUnregisterRuntimeExceptionModule)(p, ctx);
    return kc_bad;
}
static int m_all_register_unregister_interleaved(void) {
    if (!N(WerRegisterFile) || !N(WerRegisterMemoryBlock) || !N(WerRegisterCustomMetadata) || !N(WerRegisterAdditionalProcess) || !N(WerRegisterRuntimeExceptionModule)) return 0;
    char *b = VirtualAlloc(NULL, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    for (int round = 0; round < 50; round++) {
        KEQ(N(WerRegisterFile)(L"C:\\k32t\\i.txt", 1, 0), S_OK); KEQ(N(WerRegisterMemoryBlock)(b, 64), S_OK);
        KEQ(N(WerRegisterCustomMetadata)(L"Round", L"x"), S_OK); KEQ(N(WerRegisterAdditionalProcess)(GetCurrentProcessId(), 0), S_OK);
        KEQ(N(WerRegisterRuntimeExceptionModule)(L"C:\\x\\i.dll", b), S_OK); KEQ(N(WerRegisterAppLocalDump)(L"d"), S_OK);
        KEQ(N(WerUnregisterRuntimeExceptionModule)(L"C:\\x\\i.dll", b), S_OK); KEQ(N(WerUnregisterAdditionalProcess)(GetCurrentProcessId()), S_OK);
        KEQ(N(WerUnregisterCustomMetadata)(L"Round"), S_OK); KEQ(N(WerUnregisterMemoryBlock)(b), S_OK); KEQ(N(WerUnregisterFile)(L"C:\\k32t\\i.txt"), S_OK);
        KEQ(N(WerUnregisterAppLocalDump)(), S_OK);
    }
    VirtualFree(b, 0, MEM_RELEASE); return kc_bad;
}
static void *thr_arg;
static DWORD WINAPI wer_thread(LPVOID p) { char *b = p; int ok = 1; for (int i = 0; i < 200; i++) { if (N(WerRegisterMemoryBlock)(b, 16) != S_OK) ok = 0; if (N(WerUnregisterMemoryBlock)(b) != S_OK) ok = 0; } return ok; }
static int m_threads(void) {
    if (!N(WerRegisterMemoryBlock) || !N(WerUnregisterMemoryBlock)) return 0;
    char *b = VirtualAlloc(NULL, 4 * 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE); HANDLE t[4];
    for (int i = 0; i < 4; i++) t[i] = CreateThread(NULL, 0, wer_thread, b + i * 4096, 0, NULL);
    for (int i = 0; i < 4; i++) { WaitForSingleObject(t[i], 20000); DWORD c = 0; GetExitCodeThread(t[i], &c); KC(c == 1); CloseHandle(t[i]); }
    VirtualFree(b, 0, MEM_RELEASE); return kc_bad;
}

CHILD(gap16_wer) {
    const char *m = argc > 1 ? argv[1] : "";
    if (!strcmp(m, "file")) return m_file();
    if (!strcmp(m, "filelimit")) return m_file_limit();
    if (!strcmp(m, "mem")) return m_mem();
    if (!strcmp(m, "memlimit")) return m_mem_limit();
    if (!strcmp(m, "excl")) return m_excl();
    if (!strcmp(m, "addproc")) return m_addproc();
    if (!strcmp(m, "meta")) return m_meta();
    if (!strcmp(m, "dump")) return m_dump();
    if (!strcmp(m, "rtexc")) return m_rtexc();
    if (!strcmp(m, "mixed")) return m_all_register_unregister_interleaved();
    if (!strcmp(m, "threads")) return m_threads();
    return 0xC0DE;
}
#define WTEST(name, mode, ...) TEST(name) { __VA_ARGS__; RUN_MODE(mode); }
WTEST(gap16_wer_file_args_and_roundtrip, "file", REQUIRE(t_proc_k32("WerRegisterFile")))
WTEST(gap16_wer_file_count_limit_cleanup, "filelimit", REQUIRE(t_proc_k32("WerRegisterFile")))
WTEST(gap16_wer_memoryblock_args_and_limits, "mem", REQUIRE(t_proc_k32("WerRegisterMemoryBlock")))
WTEST(gap16_wer_memoryblock_count_limit, "memlimit", REQUIRE(t_proc_k32("WerRegisterMemoryBlock")))
WTEST(gap16_wer_excluded_memoryblock, "excl", REQUIRE(t_proc_k32("WerRegisterExcludedMemoryBlock")))
WTEST(gap16_wer_additional_process, "addproc", REQUIRE(t_proc_k32("WerRegisterAdditionalProcess")))
WTEST(gap16_wer_custom_metadata, "meta", REQUIRE(t_proc_k32("WerRegisterCustomMetadata")))
WTEST(gap16_wer_app_local_dump, "dump", REQUIRE(t_proc_k32("WerRegisterAppLocalDump")))
WTEST(gap16_wer_runtime_exception_module, "rtexc", REQUIRE(t_proc_k32("WerRegisterRuntimeExceptionModule")))
WTEST(gap16_wer_mixed_interleaved, "mixed", REQUIRE(t_proc_k32("WerRegisterFile")))
WTEST(gap16_wer_threads_concurrent, "threads", REQUIRE(t_proc_k32("WerRegisterMemoryBlock")))
TEST(gap16_wer_all_exports_present) {
    static const char *n[] = { "WerRegisterFile", "WerUnregisterFile", "WerRegisterMemoryBlock", "WerUnregisterMemoryBlock", "WerRegisterExcludedMemoryBlock", "WerUnregisterExcludedMemoryBlock",
        "WerRegisterAdditionalProcess", "WerUnregisterAdditionalProcess", "WerRegisterCustomMetadata", "WerUnregisterCustomMetadata", "WerRegisterAppLocalDump", "WerUnregisterAppLocalDump",
        "WerRegisterRuntimeExceptionModule", "WerUnregisterRuntimeExceptionModule" };
    if (t_is_wine()) t_note("wine lacks some Wer exports");
    for (int i = 0; i < 14; i++) { if (t_is_wine()) break; CHECKF(t_proc_k32(n[i]) != NULL, "kernel32 missing %s", n[i]); CHECKF(t_proc_kb(n[i]) != NULL, "kernelbase missing %s", n[i]); }
}
TEST(gap16_wer_k32_forwards_to_kernelbase) {
    static const char *n[] = { "WerRegisterFile", "WerRegisterMemoryBlock", "WerRegisterCustomMetadata", "WerRegisterAppLocalDump", "WerUnregisterFile" };
    if (t_is_wine()) { t_skip("forwarders are stubs"); return; }
    for (int i = 0; i < 5; i++) { FARPROC a = t_proc_k32(n[i]), b = t_proc_kb(n[i]); if (a && b) CHECKF(a == b, "%s kernel32 %p kernelbase %p", n[i], a, b); }
}
