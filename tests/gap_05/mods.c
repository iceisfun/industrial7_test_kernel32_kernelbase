/* GetModuleBaseName{A,W}, GetModuleFileNameEx{A,W}, GetModuleInformation */
#include "g5.h"

typedef DWORD (WINAPI *nameW_t)(HANDLE, HMODULE, LPWSTR, DWORD);
typedef DWORD (WINAPI *nameA_t)(HANDLE, HMODULE, LPSTR, DWORD);
typedef BOOL (WINAPI *modinfo_t)(HANDLE, HMODULE, LPMODULEINFO, DWORD);
#define BN_W ((nameW_t)g5_proc("GetModuleBaseNameW"))
#define BN_A ((nameA_t)g5_proc("GetModuleBaseNameA"))
#define FN_W ((nameW_t)g5_proc("GetModuleFileNameExW"))
#define FN_A ((nameA_t)g5_proc("GetModuleFileNameExA"))
#define MODINFO ((modinfo_t)g5_proc("GetModuleInformation"))

#define NEED_NAMES() do { if (!BN_W || !BN_A || !FN_W || !FN_A) { t_skip("missing psapi name exports"); return; } } while (0)

TEST(gap5_mod_exports) {
    static const char *n[] = { "GetModuleBaseNameA", "GetModuleBaseNameW", "GetModuleFileNameExA", "GetModuleFileNameExW", "GetModuleInformation" };
    for (unsigned i = 0; i < 5; i++) CHECKF(g5_proc(n[i]) != NULL, "%s", n[i]);
    if (!t_is_wine()) for (unsigned i = 0; i < 5; i++) {
        CHECKF(t_proc_kb(n[i]) != NULL, "kernelbase!%s", n[i]);
        char k[64]; snprintf(k, sizeof k, "K32%s", n[i]);
        CHECKF(t_proc_k32(k) != NULL, "kernel32!%s", k);
        /* kernel32 exports K32* as a local 'jmp qword ptr [rip+disp32]' thunk through its import table, not a forwarder */
        FARPROC kp = t_proc_k32(k), bp = t_proc_kb(k);
        const BYTE *c = (const BYTE *)kp;
        if (kp && c[0] == 0x48 && c[1] == 0xFF && c[2] == 0x25) {
            FARPROC tgt = *(FARPROC *)(c + 7 + *(const INT32 *)(c + 3));
            CHECKF(tgt == bp, "%s thunk target is kernelbase", k);
        } else CHECKF(kp == bp, "%s same target", k);
    }
}

/* ----- base name ----- */
TEST(gap5_basename_self_exe) {
    NEED_NAMES();
    WCHAR w[MAX_PATH]; char a[MAX_PATH], want[MAX_PATH]; g5_self_base(want, sizeof want);
    HANDLE me = GetCurrentProcess();
    SET_ERR(0);
    DWORD n = BN_W(me, NULL, w, MAX_PATH);
    EXPECT_EQ(n, strlen(want));
    char got[MAX_PATH]; g5_w2a(w, got, sizeof got);
    CHECKF(!lstrcmpiA(got, want), "got %s want %s", got, want);
    EXPECT_EQ(w[n], 0);
    n = BN_A(me, NULL, a, MAX_PATH);
    EXPECT_EQ(n, strlen(want));
    CHECK(!lstrcmpiA(a, want));
    /* explicit module handle of the exe equals NULL */
    n = BN_A(me, GetModuleHandleA(NULL), a, MAX_PATH);
    EXPECT_EQ(n, strlen(want));
    CHECK(!lstrcmpiA(a, want));
}
TEST(gap5_basename_pseudo_handle_minus1) {
    NEED_NAMES();
    char a[MAX_PATH], want[MAX_PATH]; g5_self_base(want, sizeof want);
    DWORD n = BN_A((HANDLE)-1, NULL, a, MAX_PATH);
    EXPECT_EQ(n, strlen(want));
    CHECK(!lstrcmpiA(a, want));
}
TEST(gap5_basename_system_modules) {
    NEED_NAMES();
    static const char *m[] = { "kernel32.dll", "ntdll.dll", "kernelbase.dll" };
    for (unsigned i = 0; i < 3; i++) {
        HMODULE h = GetModuleHandleA(m[i]); if (!h) continue;
        char a[MAX_PATH]; WCHAR w[MAX_PATH];
        SET_ERR(0);
        DWORD n = BN_A(GetCurrentProcess(), h, a, MAX_PATH);
        CHECKF(n == strlen(m[i]) && !lstrcmpiA(a, m[i]), "%s -> %s (%lu)", m[i], a, n);
        n = BN_W(GetCurrentProcess(), h, w, MAX_PATH);
        EXPECT_EQ(n, strlen(m[i]));
        g5_w2a(w, a, sizeof a); CHECK(!lstrcmpiA(a, m[i]));
        t_note("loader base name for %s is \"%s\"", m[i], a);
    }
}
TEST(gap5_basename_loaded_dll_roundtrip) {
    NEED_NAMES();
    HMODULE h = LoadLibraryA("version.dll");
    if (!h) { t_skip("no version.dll"); return; }
    char a[MAX_PATH];
    DWORD n = BN_A(GetCurrentProcess(), h, a, MAX_PATH);
    CHECK(n == 11 && !lstrcmpiA(a, "version.dll"));
    FreeLibrary(h);
}
TEST(gap5_basename_buffer_sizes_w) {
    WINDOWS_ONLY();
    NEED_NAMES();
    char want[MAX_PATH]; g5_self_base(want, sizeof want);
    DWORD len = (DWORD)strlen(want);
    HANDLE me = GetCurrentProcess();
    WCHAR w[MAX_PATH]; char got[MAX_PATH];
    /* exact fit including NUL */
    for (int i = 0; i < MAX_PATH; i++) w[i] = 0x7777;
    DWORD n = BN_W(me, NULL, w, len + 1);
    EXPECT_EQ(n, len); EXPECT_EQ(w[len], 0); g5_w2a(w, got, sizeof got); CHECK(!lstrcmpiA(got, want));
    EXPECT_EQ(w[len + 1], 0x7777);                       /* nothing written past the buffer */
    /* roomy */
    n = BN_W(me, NULL, w, MAX_PATH); EXPECT_EQ(n, len);
    /* too small by one: truncated and still terminated */
    for (int i = 0; i < MAX_PATH; i++) w[i] = 0x7777;
    n = BN_W(me, NULL, w, len);
    CHECKF(n == len || n == len - 1, "ret %lu len %lu", n, len);
    EXPECT_EQ(w[len - 1], 0);
    EXPECT_EQ(w[len], 0x7777);
    g5_w2a(w, got, sizeof got); CHECK(strlen(got) == len - 1 && !_strnicmp(got, want, len - 1));
    /* one char buffer */
    for (int i = 0; i < 4; i++) w[i] = 0x7777;
    n = BN_W(me, NULL, w, 1);
    CHECK(n <= 1); EXPECT_EQ(w[0], 0); EXPECT_EQ(w[1], 0x7777);
    /* zero-size buffer */
    for (int i = 0; i < 4; i++) w[i] = 0x7777;
    n = BN_W(me, NULL, w, 0);
    EXPECT_EQ(n, 0); EXPECT_EQ(w[0], 0x7777);
}
TEST(gap5_basename_buffer_sizes_a) {
    WINDOWS_ONLY();
    NEED_NAMES();
    char want[MAX_PATH]; g5_self_base(want, sizeof want);
    DWORD len = (DWORD)strlen(want);
    HANDLE me = GetCurrentProcess();
    char a[MAX_PATH];
    memset(a, 0x77, sizeof a);
    DWORD n = BN_A(me, NULL, a, len + 1);
    EXPECT_EQ(n, len); EXPECT_EQ(a[len], 0); EXPECT_EQ((unsigned char)a[len + 1], 0x77);
    memset(a, 0x77, sizeof a);
    n = BN_A(me, NULL, a, len);
    CHECKF(n == len || n == len - 1, "ret %lu", n);
    EXPECT_EQ(a[len - 1], 0);
    CHECK(!_strnicmp(a, want, len - 1));
    memset(a, 0x77, sizeof a);
    n = BN_A(me, NULL, a, 1);
    CHECK(n <= 1); EXPECT_EQ(a[0], 0);
    memset(a, 0x77, sizeof a);
    n = BN_A(me, NULL, a, 0);
    EXPECT_EQ(n, 0); EXPECT_EQ((unsigned char)a[0], 0x77);
}
TEST(gap5_basename_bad_module) {
    NEED_NAMES();
    WCHAR w[MAX_PATH]; char a[MAX_PATH];
    HANDLE me = GetCurrentProcess();
    SET_ERR(0); EXPECT_EQ(BN_W(me, (HMODULE)0x10000, w, MAX_PATH), 0); t_note("badmod err=%lu", GetLastError());
    SET_ERR(0); EXPECT_EQ(BN_A(me, (HMODULE)0x10000, a, MAX_PATH), 0);
    /* datafile-style handles (low bit set) are not in the loader list */
    HMODULE df = LoadLibraryExA("version.dll", NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (df) { SET_ERR(0); EXPECT_EQ(BN_A(me, df, a, MAX_PATH), 0); FreeLibrary(df); }
    /* base+1 of a real module */
    HMODULE k = GetModuleHandleA("kernel32.dll");
    EXPECT_EQ(BN_A(me, (HMODULE)((char *)k + 1), a, MAX_PATH), 0);
    EXPECT_EQ(BN_A(me, (HMODULE)((char *)k + 0x1000), a, MAX_PATH), 0);
}
TEST(gap5_basename_bad_process) {
    NEED_NAMES();
    WCHAR w[MAX_PATH]; char a[MAX_PATH];
    SET_ERR(0); EXPECT_EQ(BN_W(NULL, NULL, w, MAX_PATH), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(BN_A(NULL, NULL, a, MAX_PATH), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0); EXPECT_EQ(BN_A(e, NULL, a, MAX_PATH), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE t = GetCurrentThread();
    SET_ERR(0); EXPECT_EQ(BN_A(t, NULL, a, MAX_PATH), 0); t_note("thread pseudo handle err=%lu", GetLastError());
    CloseHandle(e);
    SET_ERR(0); EXPECT_EQ(BN_A(e, NULL, a, MAX_PATH), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);   /* closed */
}
TEST(gap5_basename_insufficient_access) {
    NEED_NAMES();
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    if (!h) { t_skip("open failed"); return; }
    char a[MAX_PATH]; WCHAR w[MAX_PATH];
    SET_ERR(0); EXPECT_EQ(BN_A(h, NULL, a, MAX_PATH), 0); t_note("limited-info handle: err=%lu", GetLastError());
    SET_ERR(0); EXPECT_EQ(BN_W(h, NULL, w, MAX_PATH), 0);
    CloseHandle(h);
    h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, GetCurrentProcessId());   /* no VM_READ */
    if (h) {
        SET_ERR(0); EXPECT_EQ(BN_A(h, NULL, a, MAX_PATH), 0); EXPECT_ERR(ERROR_ACCESS_DENIED);
        CloseHandle(h);
    }
    h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, GetCurrentProcessId());
    if (h) {
        char want[MAX_PATH]; g5_self_base(want, sizeof want);
        EXPECT_EQ(BN_A(h, NULL, a, MAX_PATH), strlen(want));
        CloseHandle(h);
    }
}
TEST(gap5_basename_child_process) {
    NEED_NAMES();
    HANDLE ready, quit; DWORD pid;
    HANDLE c = g5_spawn_waiter(&ready, &quit, &pid);
    if (!c) { t_skip("child spawn failed"); return; }
    char want[MAX_PATH], a[MAX_PATH]; g5_self_base(want, sizeof want);
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    CHECK(h != NULL);
    if (h) {
        SET_ERR(0);
        DWORD n = BN_A(h, NULL, a, MAX_PATH);
        EXPECT_EQ(n, strlen(want)); CHECK(!lstrcmpiA(a, want));
        /* a module handle valid in this process (kernel32 base) usually has the same base in the child (ASLR is per boot) */
        HMODULE k = GetModuleHandleA("kernel32.dll");
        n = BN_A(h, k, a, MAX_PATH);
        if (n) CHECK(!lstrcmpiA(a, "kernel32.dll")); else t_note("kernel32 base differs in child");
        CloseHandle(h);
    }
    /* full-access handle from CreateProcess is not available here; the limited right must not work */
    h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (h) { SET_ERR(0); EXPECT_EQ(BN_A(h, NULL, a, MAX_PATH), 0); CloseHandle(h); }
    g5_end_waiter(c, ready, quit);
}
TEST(gap5_basename_exited_process) {
    NEED_NAMES();
    HANDLE ready, quit; DWORD pid;
    HANDLE c = g5_spawn_waiter(&ready, &quit, &pid);
    if (!c) { t_skip("child spawn failed"); return; }
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | SYNCHRONIZE, FALSE, pid);
    g5_end_waiter(c, ready, quit);
    if (!h) { t_skip("open failed"); return; }
    WaitForSingleObject(h, 5000);
    char a[MAX_PATH]; SET_ERR(0);
    EXPECT_EQ(BN_A(h, NULL, a, MAX_PATH), 0);
    t_note("exited process: err=%lu", GetLastError());
    CloseHandle(h);
}

/* ----- file name ----- */
TEST(gap5_filenameex_self_exe) {
    NEED_NAMES();
    char want[MAX_PATH], a[MAX_PATH]; WCHAR w[MAX_PATH], ww[MAX_PATH];
    GetModuleFileNameA(NULL, want, MAX_PATH);
    HANDLE me = GetCurrentProcess();
    SET_ERR(0);
    DWORD n = FN_A(me, NULL, a, MAX_PATH);
    EXPECT_EQ(n, strlen(want)); CHECKF(!lstrcmpiA(a, want), "%s vs %s", a, want);
    n = FN_W(me, NULL, w, MAX_PATH);
    EXPECT_EQ(n, strlen(want));
    g5_w2a(w, a, sizeof a); CHECK(!lstrcmpiA(a, want));
    n = FN_A(me, GetModuleHandleA(NULL), a, MAX_PATH);
    EXPECT_EQ(n, strlen(want)); CHECK(!lstrcmpiA(a, want));
    n = FN_A((HANDLE)-1, NULL, a, MAX_PATH);
    EXPECT_EQ(n, strlen(want));
    (void)ww;
}
TEST(gap5_filenameex_system_modules) {
    NEED_NAMES();
    static const char *m[] = { "kernel32.dll", "ntdll.dll", "kernelbase.dll" };
    char sysd[MAX_PATH]; GetSystemDirectoryA(sysd, sizeof sysd);
    for (unsigned i = 0; i < 3; i++) {
        HMODULE h = GetModuleHandleA(m[i]); if (!h) continue;
        char a[MAX_PATH], want[MAX_PATH], direct[MAX_PATH];
        GetModuleFileNameA(h, direct, sizeof direct);
        DWORD n = FN_A(GetCurrentProcess(), h, a, MAX_PATH);
        CHECKF(n == strlen(direct) && !lstrcmpiA(a, direct), "%s: %s vs %s", m[i], a, direct);
        snprintf(want, sizeof want, "%s\\%s", sysd, m[i]);
        if (!t_is_wine()) CHECKF(!lstrcmpiA(a, want), "%s vs %s", a, want);
        WCHAR w[MAX_PATH]; n = FN_W(GetCurrentProcess(), h, w, MAX_PATH);
        EXPECT_EQ(n, strlen(direct));
    }
}
TEST(gap5_filenameex_zero_size) {
    WINDOWS_ONLY();
    NEED_NAMES();
    char a[8]; WCHAR w[8]; memset(a, 0x77, sizeof a); for (int i = 0; i < 8; i++) w[i] = 0x7777;
    HANDLE me = GetCurrentProcess();
    SET_ERR(0); EXPECT_EQ(FN_A(me, NULL, a, 0), 0); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    SET_ERR(0); EXPECT_EQ(FN_W(me, NULL, w, 0), 0); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    SET_ERR(0); EXPECT_EQ(FN_W(me, GetModuleHandleA("kernel32.dll"), w, 0), 0); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    SET_ERR(0); EXPECT_EQ(FN_W(me, (HMODULE)0x10000, w, 0), 0); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);   /* size check is first */
    EXPECT_EQ((unsigned char)a[0], 0x77); EXPECT_EQ(w[0], 0x7777);
    SET_ERR(0); EXPECT_EQ(FN_W(NULL, NULL, w, 0), 0); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
}
TEST(gap5_filenameex_truncation_exe) {
    WINDOWS_ONLY();
    NEED_NAMES();
    /* NULL module goes through ProcessImageFileNameWin32 (decomp: NtQueryInformationProcess class 0x2b), so take the reference
     * from the same call rather than from the loader's copy of the path. */
    char want[MAX_PATH]; FN_A(GetCurrentProcess(), NULL, want, MAX_PATH);
    DWORD len = (DWORD)strlen(want);
    HANDLE me = GetCurrentProcess();
    WCHAR w[MAX_PATH]; char a[MAX_PATH], got[MAX_PATH];
    for (int i = 0; i < MAX_PATH; i++) w[i] = 0x7777;
    DWORD n = FN_W(me, NULL, w, len + 1);
    EXPECT_EQ(n, len); EXPECT_EQ(w[len], 0); EXPECT_EQ(w[len + 1], 0x7777);
    for (int i = 0; i < MAX_PATH; i++) w[i] = 0x7777;
    n = FN_W(me, NULL, w, len);                  /* exe path via image-name query: nSize-1 chars + NUL */
    EXPECT_EQ(n, len - 1); EXPECT_EQ(w[len - 1], 0); EXPECT_EQ(w[len], 0x7777);
    g5_w2a(w, got, sizeof got); CHECK(!_strnicmp(got, want, len - 1));
    for (int i = 0; i < MAX_PATH; i++) w[i] = 0x7777;
    n = FN_W(me, NULL, w, 1);
    EXPECT_EQ(n, 0); EXPECT_EQ(w[0], 0); EXPECT_EQ(w[1], 0x7777);
    n = FN_W(me, NULL, w, 2);
    EXPECT_EQ(n, 1); EXPECT_EQ(w[1], 0);
    memset(a, 0x77, sizeof a);
    n = FN_A(me, NULL, a, len + 1); EXPECT_EQ(n, len); EXPECT_EQ(a[len], 0); EXPECT_EQ((unsigned char)a[len + 1], 0x77);
    memset(a, 0x77, sizeof a);
    n = FN_A(me, NULL, a, len); EXPECT_EQ(n, len - 1); EXPECT_EQ(a[len - 1], 0);
    memset(a, 0x77, sizeof a);
    /* A form: GetModuleFileNameExW returns 0 for nSize 1, and the A wrapper then converts nothing (decomp: DVar1 == 0 skips WideCharToMultiByte) */
    n = FN_A(me, NULL, a, 1); EXPECT_EQ(n, 0); EXPECT_EQ((unsigned char)a[0], 0x77);
}
TEST(gap5_filenameex_truncation_module) {
    NEED_NAMES();
    HMODULE k = GetModuleHandleA("kernel32.dll");
    char direct[MAX_PATH]; GetModuleFileNameA(k, direct, sizeof direct);
    DWORD len = (DWORD)strlen(direct);
    HANDLE me = GetCurrentProcess();
    WCHAR w[MAX_PATH]; char got[MAX_PATH];
    for (int i = 0; i < MAX_PATH; i++) w[i] = 0x7777;
    DWORD n = FN_W(me, k, w, len + 1);
    EXPECT_EQ(n, len); EXPECT_EQ(w[len], 0); EXPECT_EQ(w[len + 1], 0x7777);
    for (int i = 0; i < MAX_PATH; i++) w[i] = 0x7777;
    n = FN_W(me, k, w, len);
    CHECKF(n == len || n == len - 1, "ret %lu len %lu", n, len);
    EXPECT_EQ(w[len - 1], 0); EXPECT_EQ(w[len], 0x7777);
    g5_w2a(w, got, sizeof got); CHECK(!_strnicmp(got, direct, len - 1));
    for (int i = 0; i < 4; i++) w[i] = 0x7777;
    n = FN_W(me, k, w, 1);
    CHECK(n <= 1); EXPECT_EQ(w[0], 0); EXPECT_EQ(w[1], 0x7777);
}
TEST(gap5_filenameex_large_nsize_clamped) {
    NEED_NAMES();
    char want[MAX_PATH], a[MAX_PATH]; GetModuleFileNameA(NULL, want, MAX_PATH);
    WCHAR *w = malloc(0x9000 * sizeof(WCHAR));
    DWORD n = FN_W(GetCurrentProcess(), NULL, w, 0x8000);
    EXPECT_EQ(n, strlen(want));
    n = FN_W(GetCurrentProcess(), NULL, w, 0x8001);     /* clamped to 0x7fff internally */
    EXPECT_EQ(n, strlen(want));
    g5_w2a(w, a, sizeof a); CHECK(!lstrcmpiA(a, want));
    n = FN_W(GetCurrentProcess(), GetModuleHandleA("kernel32.dll"), w, 0x8000);
    CHECK(n > 0 && n < MAX_PATH);
    free(w);
}
TEST(gap5_filenameex_bad_args) {
    NEED_NAMES();
    char a[MAX_PATH];
    SET_ERR(0); EXPECT_EQ(FN_A(NULL, NULL, a, MAX_PATH), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(FN_A(NULL, GetModuleHandleA(NULL), a, MAX_PATH), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0); EXPECT_EQ(FN_A(e, NULL, a, MAX_PATH), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(e);
    SET_ERR(0); EXPECT_EQ(FN_A(GetCurrentProcess(), (HMODULE)0x10000, a, MAX_PATH), 0);
    SET_ERR(0); EXPECT_EQ(FN_A(GetCurrentProcess(), (HMODULE)1, a, MAX_PATH), 0);
}
TEST(gap5_filenameex_limited_handle_exe) {
    NEED_NAMES();
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    if (!h) { t_skip("open failed"); return; }
    char want[MAX_PATH], a[MAX_PATH]; GetModuleFileNameA(NULL, want, MAX_PATH);
    SET_ERR(0);
    DWORD n = FN_A(h, NULL, a, MAX_PATH);       /* image-name query only needs limited information */
    if (n) { EXPECT_EQ(n, strlen(want)); CHECK(!lstrcmpiA(a, want)); }
    else t_note("limited handle: err=%lu", GetLastError());
    SET_ERR(0);
    EXPECT_EQ(FN_A(h, GetModuleHandleA("kernel32.dll"), a, MAX_PATH), 0);   /* module lookup needs VM_READ */
    CloseHandle(h);
    h = OpenProcess(SYNCHRONIZE, FALSE, GetCurrentProcessId());
    if (h) { SET_ERR(0); EXPECT_EQ(FN_A(h, NULL, a, MAX_PATH), 0); EXPECT_ERR(ERROR_ACCESS_DENIED); CloseHandle(h); }
}
TEST(gap5_filenameex_child_process) {
    NEED_NAMES();
    HANDLE ready, quit; DWORD pid;
    HANDLE c = g5_spawn_waiter(&ready, &quit, &pid);
    if (!c) { t_skip("child spawn failed"); return; }
    char want[MAX_PATH], a[MAX_PATH]; GetModuleFileNameA(NULL, want, MAX_PATH);
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    CHECK(h != NULL);
    if (h) {
        DWORD n = FN_A(h, NULL, a, MAX_PATH);
        EXPECT_EQ(n, strlen(want)); CHECK(!lstrcmpiA(a, want));
        n = FN_A(h, GetModuleHandleA("ntdll.dll"), a, MAX_PATH);
        if (n) { char d[MAX_PATH]; GetModuleFileNameA(GetModuleHandleA("ntdll.dll"), d, sizeof d); CHECK(!lstrcmpiA(a, d)); }
        CloseHandle(h);
    }
    g5_end_waiter(c, ready, quit);
}

/* ----- GetModuleInformation ----- */
static IMAGE_NT_HEADERS *nth(HMODULE m) { return (IMAGE_NT_HEADERS *)((char *)m + ((IMAGE_DOS_HEADER *)m)->e_lfanew); }
TEST(gap5_modinfo_exe_and_system) {
    REQUIRE(MODINFO);
    HMODULE mods[3] = { GetModuleHandleA(NULL), GetModuleHandleA("kernel32.dll"), GetModuleHandleA("ntdll.dll") };
    for (int i = 0; i < 3; i++) {
        MODULEINFO mi; memset(&mi, 0xCC, sizeof mi);
        SET_ERR(0);
        CHECK(MODINFO(GetCurrentProcess(), mods[i], &mi, sizeof mi));
        EXPECT_EQ(mi.lpBaseOfDll, mods[i]);
        EXPECT_EQ(mi.SizeOfImage, nth(mods[i])->OptionalHeader.SizeOfImage);
        /* the loader records a NULL entry point for an image without one (ntdll.dll has AddressOfEntryPoint 0) */
        DWORD aoe = nth(mods[i])->OptionalHeader.AddressOfEntryPoint;
        EXPECT_EQ(mi.EntryPoint, aoe ? (char *)mods[i] + aoe : (char *)NULL);
    }
}
TEST(gap5_modinfo_struct_size) {
    REQUIRE(MODINFO);
    HMODULE k = GetModuleHandleA("kernel32.dll");
    EXPECT_EQ(sizeof(MODULEINFO), 0x18);
    MODULEINFO big[2];
    for (DWORD cb = 0; cb < 0x18; cb++) {
        memset(big, 0xCC, sizeof big);
        SET_ERR(0);
        CHECKF(!MODINFO(GetCurrentProcess(), k, &big[0], cb), "cb=%lu", cb);
        EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
        CHECKF(((unsigned char *)big)[0] == 0xCC && ((unsigned char *)big)[0x17] == 0xCC, "touched on failure cb=%lu", cb);
    }
    memset(big, 0xCC, sizeof big);
    CHECK(MODINFO(GetCurrentProcess(), k, &big[0], 0x18));
    CHECK(MODINFO(GetCurrentProcess(), k, &big[0], 0x100));
    CHECK(MODINFO(GetCurrentProcess(), k, &big[0], 0xFFFFFFFF));
    EXPECT_EQ(((unsigned char *)&big[1])[0], 0xCC);    /* only 0x18 bytes are written */
}
TEST(gap5_modinfo_bad_module) {
    REQUIRE(MODINFO);
    MODULEINFO mi; memset(&mi, 0xCC, sizeof mi);
    HMODULE k = GetModuleHandleA("kernel32.dll");
    SET_ERR(0); EXPECT_EQ(MODINFO(GetCurrentProcess(), (HMODULE)0x10000, &mi, sizeof mi), 0);
    t_note("modinfo bad module err=%lu", GetLastError());
    EXPECT_EQ(MODINFO(GetCurrentProcess(), (HMODULE)((char *)k + 1), &mi, sizeof mi), 0);
    EXPECT_EQ(MODINFO(GetCurrentProcess(), (HMODULE)((char *)k + 0x40), &mi, sizeof mi), 0);   /* inside image, not a base */
    HMODULE df = LoadLibraryExA("version.dll", NULL, LOAD_LIBRARY_AS_DATAFILE);
    if (df) { EXPECT_EQ(MODINFO(GetCurrentProcess(), df, &mi, sizeof mi), 0); FreeLibrary(df); }
    EXPECT_EQ(((unsigned char *)&mi)[0], 0xCC);        /* untouched on failure */
    EXPECT_EQ(((unsigned char *)&mi)[0x17], 0xCC);
    EXPECT_EQ(MODINFO(GetCurrentProcess(), NULL, &mi, sizeof mi) != 0, 1);   /* NULL = main exe */
    t_note("modinfo(NULL module): lpBaseOfDll=%p (exe base %p)", mi.lpBaseOfDll, GetModuleHandleA(NULL));
}
TEST(gap5_modinfo_bad_process) {
    REQUIRE(MODINFO);
    MODULEINFO mi; memset(&mi, 0xCC, sizeof mi);
    HMODULE k = GetModuleHandleA("kernel32.dll");
    SET_ERR(0); EXPECT_EQ(MODINFO(NULL, k, &mi, sizeof mi), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0); EXPECT_EQ(MODINFO(e, k, &mi, sizeof mi), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(e);
    SET_ERR(0); EXPECT_EQ(MODINFO(e, k, &mi, sizeof mi), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, GetCurrentProcessId());
    if (h) { SET_ERR(0); EXPECT_EQ(MODINFO(h, k, &mi, sizeof mi), 0); EXPECT_ERR(ERROR_ACCESS_DENIED); CloseHandle(h); }
    h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, GetCurrentProcessId());
    if (h) { CHECK(MODINFO(h, k, &mi, sizeof mi)); EXPECT_EQ(mi.lpBaseOfDll, k); CloseHandle(h); }
}
TEST(gap5_modinfo_child_process) {
    REQUIRE(MODINFO);
    HANDLE ready, quit; DWORD pid;
    HANDLE c = g5_spawn_waiter(&ready, &quit, &pid);
    if (!c) { t_skip("child spawn failed"); return; }
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    CHECK(h != NULL);
    if (h) {
        MODULEINFO mi; HMODULE k = GetModuleHandleA("ntdll.dll");
        if (MODINFO(h, k, &mi, sizeof mi)) {
            EXPECT_EQ(mi.lpBaseOfDll, k);
            EXPECT_EQ(mi.SizeOfImage, nth(k)->OptionalHeader.SizeOfImage);
        } else t_note("ntdll base differs in child, err=%lu", GetLastError());
        /* main module of the child: NULL */
        memset(&mi, 0xCC, sizeof mi);
        CHECK(MODINFO(h, NULL, &mi, sizeof mi));
        CloseHandle(h);
    }
    g5_end_waiter(c, ready, quit);
}
TEST(gap5_modinfo_a_w_and_enum_consistency) {
    REQUIRE(MODINFO);
    /* every module returned by EnumProcessModules has info consistent with its PE header */
    typedef BOOL (WINAPI *enum_t)(HANDLE, HMODULE *, DWORD, LPDWORD);
    enum_t en = (enum_t)g5_proc("EnumProcessModules");
    if (!en) { t_skip("no EnumProcessModules"); return; }
    HMODULE ms[512]; DWORD need = 0;
    CHECK(en(GetCurrentProcess(), ms, sizeof ms, &need));
    unsigned cnt = need / sizeof(HMODULE); if (cnt > 512) cnt = 512;
    CHECK(cnt >= 3);
    for (unsigned i = 0; i < cnt; i++) {
        MODULEINFO mi; char n1[MAX_PATH], n2[MAX_PATH], d[MAX_PATH];
        CHECKF(MODINFO(GetCurrentProcess(), ms[i], &mi, sizeof mi), "module %u", i);
        EXPECT_EQ(mi.lpBaseOfDll, ms[i]);
        EXPECT_EQ(mi.SizeOfImage, nth(ms[i])->OptionalHeader.SizeOfImage);
        DWORD a = BN_A(GetCurrentProcess(), ms[i], n1, MAX_PATH);
        DWORD b = FN_A(GetCurrentProcess(), ms[i], n2, MAX_PATH);
        CHECK(a > 0 && b > 0);
        GetModuleFileNameA(ms[i], d, sizeof d);
        CHECKF(!lstrcmpiA(n2, d), "%s vs %s", n2, d);
        CHECKF(!lstrcmpiA(n1, g5_base(n2)), "%s vs %s", n1, n2);
    }
}
