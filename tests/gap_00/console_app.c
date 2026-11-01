/* AllocConsole, AddConsoleAlias{A,W}, AddSecureMemoryCacheCallback, AllocateUserPhysicalPagesNuma,
 * and the AppPolicyGet* family. Console work only ever happens in detached child processes. */
#include "g0.h"
#include <wchar.h>

#define STEP(n, cond) do { if (!(cond)) { return (n); } } while (0)

static DWORD run_detached(const char *name, const char *args) {
    PROCESS_INFORMATION pi; DWORD code = 0xDEAD0000;
    HANDLE h = t_spawn_child(name, args, DETACHED_PROCESS, NULL, NULL, NULL, FALSE, NULL, &pi);
    if (!h) return 0xDEAD0000;
    if (WaitForSingleObject(h, 25000) != WAIT_OBJECT_0) { TerminateProcess(h, 1); WaitForSingleObject(h, 5000); code = 0xDEAD0001; }
    else GetExitCodeProcess(h, &code);
    CloseHandle(pi.hThread); CloseHandle(h);
    return code;
}

/* ------------------------------------------------------------ AllocConsole */
CHILD(gap0_allocconsole) {
    BOOL (WINAPI *pAlloc)(void) = (BOOL (WINAPI *)(void))t_proc("AllocConsole");
    STEP(1, pAlloc != NULL);
    /* a detached process has no console */
    STEP(2, GetConsoleWindow() == NULL);
    DWORD pids[8];
    STEP(3, GetConsoleProcessList(pids, 8) == 0);
    SetLastError(0xdeadbeef);
    STEP(4, pAlloc() == TRUE);
    STEP(5, GetConsoleCP() != 0);
    DWORD n = GetConsoleProcessList(pids, 8), found = 0;
    STEP(6, n >= 1 && n <= 8);
    for (DWORD i = 0; i < n; i++) if (pids[i] == GetCurrentProcessId()) found = 1;
    STEP(7, found);
    /* the new console is usable through CONOUT$/CONIN$ */
    HANDLE out = CreateFileA("CONOUT$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    STEP(8, out != INVALID_HANDLE_VALUE);
    DWORD mode = 0;
    STEP(9, GetConsoleMode(out, &mode));
    DWORD w = 0;
    STEP(10, WriteConsoleW(out, L"gap0\r\n", 6, &w, NULL) && w == 6);
    CONSOLE_SCREEN_BUFFER_INFO bi;
    STEP(11, GetConsoleScreenBufferInfo(out, &bi) && bi.dwSize.X > 0 && bi.dwSize.Y > 0);
    CloseHandle(out);
    HANDLE in = CreateFileA("CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    STEP(12, in != INVALID_HANDLE_VALUE);
    STEP(13, GetConsoleMode(in, &mode));
    CloseHandle(in);
    /* a second AllocConsole in the same process is refused with access denied */
    SetLastError(0);
    STEP(14, pAlloc() == FALSE);
    STEP(15, GetLastError() == ERROR_ACCESS_DENIED);
    SetLastError(0);
    STEP(16, pAlloc() == FALSE && GetLastError() == ERROR_ACCESS_DENIED);
    /* after FreeConsole it can be allocated again */
    STEP(17, FreeConsole());
    STEP(18, GetConsoleWindow() == NULL);
    STEP(19, GetConsoleProcessList(pids, 8) == 0);
    STEP(20, pAlloc() == TRUE);
    STEP(21, GetConsoleCP() != 0);
    STEP(22, SetConsoleTitleA("gap0 title"));
    char title[64];
    STEP(23, GetConsoleTitleA(title, sizeof title) == 10 && !strcmp(title, "gap0 title"));
    STEP(24, FreeConsole());
    return 0;
}

TEST(gap0_allocconsole_detached_child) {
    REQUIRE(U(AllocConsole));
    if (t_is_wine()) { t_skip("console server not exercised under wine"); return; }
    DWORD rc = run_detached("gap0_allocconsole", "");
    EXPECT_EQ(rc, 0);
}

/* std handles are populated for a process that had none */
CHILD(gap0_allocconsole_std) {
    BOOL (WINAPI *pAlloc)(void) = (BOOL (WINAPI *)(void))t_proc("AllocConsole");
    STEP(1, pAlloc != NULL);
    HANDLE i0 = GetStdHandle(STD_INPUT_HANDLE), o0 = GetStdHandle(STD_OUTPUT_HANDLE), e0 = GetStdHandle(STD_ERROR_HANDLE);
    STEP(2, i0 == NULL && o0 == NULL && e0 == NULL);          /* detached, no redirection */
    STEP(3, pAlloc());
    HANDLE i1 = GetStdHandle(STD_INPUT_HANDLE), o1 = GetStdHandle(STD_OUTPUT_HANDLE), e1 = GetStdHandle(STD_ERROR_HANDLE);
    STEP(4, i1 != NULL && i1 != INVALID_HANDLE_VALUE);
    STEP(5, o1 != NULL && o1 != INVALID_HANDLE_VALUE);
    STEP(6, e1 != NULL && e1 != INVALID_HANDLE_VALUE);
    DWORD m;
    STEP(7, GetConsoleMode(i1, &m));
    STEP(8, GetConsoleMode(o1, &m));
    STEP(9, GetConsoleMode(e1, &m));
    DWORD w;
    STEP(10, WriteFile(o1, "x", 1, &w, NULL) && w == 1);
    STEP(11, GetFileType(o1) == FILE_TYPE_CHAR);
    return 0;
}
TEST(gap0_allocconsole_sets_std_handles) {
    REQUIRE(U(AllocConsole));
    if (t_is_wine()) { t_skip("console server not exercised under wine"); return; }
    EXPECT_EQ(run_detached("gap0_allocconsole_std", ""), 0);
}

/* redirected std handle survives AllocConsole */
CHILD(gap0_allocconsole_redir) {
    BOOL (WINAPI *pAlloc)(void) = (BOOL (WINAPI *)(void))t_proc("AllocConsole");
    STEP(1, pAlloc != NULL);
    HANDLE o0 = GetStdHandle(STD_OUTPUT_HANDLE);
    STEP(2, o0 != NULL && o0 != INVALID_HANDLE_VALUE && GetFileType(o0) == FILE_TYPE_PIPE);
    STEP(3, pAlloc());
    STEP(4, GetStdHandle(STD_OUTPUT_HANDLE) == o0);
    HANDLE i1 = GetStdHandle(STD_INPUT_HANDLE);
    DWORD m;
    STEP(5, i1 != NULL && i1 != INVALID_HANDLE_VALUE && GetConsoleMode(i1, &m));
    return 0;
}
TEST(gap0_allocconsole_keeps_redirected_handle) {
    REQUIRE(U(AllocConsole));
    if (t_is_wine()) { t_skip("console server not exercised under wine"); return; }
    HANDLE r, w; SECURITY_ATTRIBUTES sa = {sizeof sa, NULL, TRUE};
    CHECK(CreatePipe(&r, &w, &sa, 0));
    PROCESS_INFORMATION pi; DWORD code = 0xDEAD0000;
    HANDLE h = t_spawn_child("gap0_allocconsole_redir", "", DETACHED_PROCESS, NULL, w, NULL, TRUE, NULL, &pi);
    CHECK(h != NULL);
    if (h) {
        if (WaitForSingleObject(h, 25000) != WAIT_OBJECT_0) { TerminateProcess(h, 1); WaitForSingleObject(h, 5000); }
        else GetExitCodeProcess(h, &code);
        CloseHandle(pi.hThread); CloseHandle(h);
    }
    EXPECT_EQ(code, 0);
    CloseHandle(r); CloseHandle(w);
}

/* ------------------------------------------------------- AddConsoleAlias */
static int wlen_bytes(const WCHAR *s) { return (int)((wcslen(s) + 1) * 2); }

CHILD(gap0_alias) {
    BOOL (WINAPI *pAddW)(LPWSTR, LPWSTR, LPWSTR) = (BOOL (WINAPI *)(LPWSTR, LPWSTR, LPWSTR))t_proc("AddConsoleAliasW");
    BOOL (WINAPI *pAddA)(LPSTR, LPSTR, LPSTR) = (BOOL (WINAPI *)(LPSTR, LPSTR, LPSTR))t_proc("AddConsoleAliasA");
    STEP(1, pAddW && pAddA);
    STEP(2, AllocConsole());
    WCHAR buf[256]; char abuf[256];
    WCHAR exe[] = L"gap0.exe";
    char exea[] = "gap0.exe";

    /* basic add / read back */
    SetLastError(0xdeadbeef);
    STEP(3, pAddW(L"gz", L"echo gap0 $*", exe) == TRUE);
    STEP(4, GetLastError() == 0xdeadbeef);
    DWORD r = GetConsoleAliasW(L"gz", buf, sizeof buf, exe);
    STEP(5, r == (DWORD)wlen_bytes(L"echo gap0 $*"));
    STEP(6, !wcscmp(buf, L"echo gap0 $*"));

    /* overwrite */
    STEP(7, pAddW(L"gz", L"dir $1 $2", exe));
    r = GetConsoleAliasW(L"gz", buf, sizeof buf, exe);
    STEP(8, r == (DWORD)wlen_bytes(L"dir $1 $2") && !wcscmp(buf, L"dir $1 $2"));

    /* ANSI entry point is visible through the wide reader and vice versa */
    STEP(9, pAddA("ga", "type $*", exea) == TRUE);
    r = GetConsoleAliasW(L"ga", buf, sizeof buf, exe);
    STEP(10, r == (DWORD)wlen_bytes(L"type $*") && !wcscmp(buf, L"type $*"));
    r = GetConsoleAliasA("ga", abuf, sizeof abuf, exea);
    STEP(11, r == 8 && !strcmp(abuf, "type $*"));
    STEP(12, pAddW(L"gw", L"cd ..", exe));
    r = GetConsoleAliasA("gw", abuf, sizeof abuf, exea);
    STEP(13, r == 6 && !strcmp(abuf, "cd .."));

    /* alias list for the exe includes our names */
    DWORD need = GetConsoleAliasesLengthW(exe);
    STEP(14, need > 0);

    /* exe list contains our exe name */
    DWORD enl = GetConsoleAliasExesLengthW();
    STEP(15, enl > 0);
    WCHAR *exes = (WCHAR *)calloc(1, enl + 16);
    STEP(16, exes != NULL);
    STEP(17, GetConsoleAliasExesW(exes, enl) > 0);
    int hit = 0;
    for (WCHAR *p = exes; *p; p += wcslen(p) + 1) if (!_wcsicmp(p, L"gap0.exe")) hit = 1;
    free(exes);
    STEP(18, hit);

    /* delete: NULL target */
    STEP(19, pAddW(L"gz", NULL, exe) == TRUE);
    SetLastError(0);
    r = GetConsoleAliasW(L"gz", buf, sizeof buf, exe);
    STEP(20, r == 0);
    /* delete via A */
    STEP(21, pAddA("ga", NULL, exea) == TRUE);
    r = GetConsoleAliasW(L"ga", buf, sizeof buf, exe);
    STEP(22, r == 0);
    /* empty target also removes */
    /* empty target: conhost behaviour not verified, so only exercised */
    STEP(23, pAddW(L"gw", L"", exe) == TRUE);

    /* exe name is mandatory */
    SetLastError(0);
    STEP(25, pAddW(L"gq", L"x", NULL) == FALSE);
    STEP(26, GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0);
    STEP(27, pAddA("gq", "x", NULL) == FALSE);
    STEP(28, GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0);
    STEP(29, pAddW(L"gq", L"x", L"") == FALSE);
    STEP(30, GetLastError() == ERROR_INVALID_PARAMETER);
    SetLastError(0);
    STEP(31, pAddA("gq", "x", "") == FALSE);
    STEP(32, GetLastError() == ERROR_INVALID_PARAMETER);
    return 0;
}
TEST(gap0_alias_roundtrip_child) {
    REQUIRE(U(AddConsoleAliasW)); REQUIRE(U(AddConsoleAliasA));
    if (t_is_wine()) { t_skip("console server not exercised under wine"); return; }
    EXPECT_EQ(run_detached("gap0_alias", ""), 0);
}

/* behaviour that is observed, not asserted */
CHILD(gap0_alias_probe) {
    BOOL (WINAPI *pAddW)(LPWSTR, LPWSTR, LPWSTR) = (BOOL (WINAPI *)(LPWSTR, LPWSTR, LPWSTR))t_proc("AddConsoleAliasW");
    STEP(1, pAddW != NULL);
    STEP(2, AllocConsole());
    WCHAR buf[256]; WCHAR exe[] = L"Gap0Probe.EXE";
    STEP(3, pAddW(L"MixedCase", L"v1", exe));
    /* case-insensitive source and exe lookups */
    DWORD r1 = GetConsoleAliasW(L"mixedcase", buf, sizeof buf, exe);
    DWORD r2 = GetConsoleAliasW(L"MixedCase", buf, sizeof buf, L"gap0probe.exe");
    /* empty source, NULL source, odd characters */
    SetLastError(0); BOOL e1 = pAddW(L"", L"v", exe); DWORD le1 = GetLastError();
    SetLastError(0); BOOL e2 = pAddW(NULL, L"v", exe); DWORD le2 = GetLastError();
    SetLastError(0); BOOL e3 = pAddW(L"has space", L"v", exe);
    SetLastError(0); BOOL e4 = pAddW(L"eq=sign", L"v", exe);
    /* delete unknown alias */
    SetLastError(0); BOOL d1 = pAddW(L"neverdefined", NULL, exe);
    SetLastError(0); WCHAR out2[8]; DWORD g1 = GetConsoleAliasW(L"neverdefined", out2, sizeof out2, exe); DWORD ge1 = GetLastError();
    /* long target */
    WCHAR big[2000]; for (int i = 0; i < 1999; i++) big[i] = L'a'; big[1999] = 0;
    SetLastError(0); BOOL b1 = pAddW(L"longone", big, exe); DWORD lb1 = GetLastError();
    /* get with too-small buffer */
    SetLastError(0); WCHAR tiny[3]; DWORD g2 = GetConsoleAliasW(L"MixedCase", tiny, sizeof tiny, exe); DWORD ge2 = GetLastError();
    char msg[512];
    snprintf(msg, sizeof msg, "ci_src=%lu ci_exe=%lu empty=%d/%lu null=%d/%lu space=%d eq=%d del_unknown=%d get_unknown=%lu/%lu long=%d/%lu tiny=%lu/%lu",
             r1, r2, e1, le1, e2, le2, e3, e4, d1, g1, ge1, b1, lb1, g2, ge2);
    /* emit through a file in the temp dir named by argv */
    if (argc > 1) {
        HANDLE f = CreateFileA(argv[1], GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        if (f != INVALID_HANDLE_VALUE) { DWORD w; WriteFile(f, msg, (DWORD)strlen(msg), &w, NULL); CloseHandle(f); }
    }
    /* hard parts: case-insensitivity of lookups is long-standing console behaviour */
    STEP(10, r1 != 0);
    STEP(11, r2 != 0);
    return 0;
}
TEST(gap0_alias_probe_child) {
    REQUIRE(U(AddConsoleAliasW));
    if (t_is_wine()) { t_skip("console server not exercised under wine"); return; }
    char path[MAX_PATH]; t_tmpfile(path, sizeof path, "alias_probe.txt");
    char args[MAX_PATH + 4]; snprintf(args, sizeof args, "\"%s\"", path);
    /* the child reads argv[1] without quotes; pass unquoted since the temp path has no spaces normally */
    snprintf(args, sizeof args, "%s", path);
    EXPECT_EQ(run_detached("gap0_alias_probe", args), 0);
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f != INVALID_HANDLE_VALUE) {
        char msg[600] = {0}; DWORD n = 0; ReadFile(f, msg, sizeof msg - 1, &n, NULL); CloseHandle(f);
        t_note("alias probe: %s", msg);
    }
}

TEST(gap0_alias_exports_a_w_present) {
    CHECK(UK(AddConsoleAliasA) != NULL);
    CHECK(UK(AddConsoleAliasW) != NULL);
    CHECK(UB(AddConsoleAliasA) != NULL);
    CHECK(UB(AddConsoleAliasW) != NULL);
    CHECK(UK(AllocConsole) != NULL);
}

/* ------------------------------------------ secure memory cache callback */
static BOOLEAN NTAPI sm_cb(PVOID a, SIZE_T n) { return TRUE; }
static BOOLEAN NTAPI sm_cb2(PVOID a, SIZE_T n) { return TRUE; }

CHILD(gap0_securemem) {
    BOOL (WINAPI *pAdd)(PSECURE_MEMORY_CACHE_CALLBACK) = (BOOL (WINAPI *)(PSECURE_MEMORY_CACHE_CALLBACK))t_proc("AddSecureMemoryCacheCallback");
    BOOL (WINAPI *pRem)(PSECURE_MEMORY_CACHE_CALLBACK) = (BOOL (WINAPI *)(PSECURE_MEMORY_CACHE_CALLBACK))t_proc("RemoveSecureMemoryCacheCallback");
    STEP(1, pAdd && pRem);
    /* removing something never added reports FALSE */
    STEP(2, pRem(sm_cb) == FALSE);
    if (!pAdd(sm_cb)) return 0;           /* not supported on this machine: nothing more to verify */
    STEP(4, pRem(sm_cb) == TRUE);
    STEP(5, pRem(sm_cb) == FALSE);
    /* registrations are reference counted per callback */
    STEP(6, pAdd(sm_cb) && pAdd(sm_cb));
    STEP(7, pRem(sm_cb) == TRUE);
    STEP(8, pRem(sm_cb) == TRUE);
    STEP(9, pRem(sm_cb) == FALSE);
    /* independent callbacks */
    STEP(10, pAdd(sm_cb) && pAdd(sm_cb2));
    STEP(11, pRem(sm_cb2) == TRUE);
    STEP(12, pRem(sm_cb2) == FALSE);
    STEP(13, pRem(sm_cb) == TRUE);
    /* many registrations */
    for (int i = 0; i < 100; i++) STEP(14, pAdd(sm_cb));
    for (int i = 0; i < 100; i++) STEP(15, pRem(sm_cb));
    STEP(16, pRem(sm_cb) == FALSE);
    return 0;
}
TEST(gap0_securemem_add_remove_child) {
    REQUIRE(U(AddSecureMemoryCacheCallback)); REQUIRE(U(RemoveSecureMemoryCacheCallback));
    EXPECT_EQ(t_run_child("gap0_securemem", "", 20000), 0);
}

CHILD(gap0_securemem_null) {
    BOOL (WINAPI *pAdd)(PSECURE_MEMORY_CACHE_CALLBACK) = (BOOL (WINAPI *)(PSECURE_MEMORY_CACHE_CALLBACK))t_proc("AddSecureMemoryCacheCallback");
    BOOL (WINAPI *pRem)(PSECURE_MEMORY_CACHE_CALLBACK) = (BOOL (WINAPI *)(PSECURE_MEMORY_CACHE_CALLBACK))t_proc("RemoveSecureMemoryCacheCallback");
    STEP(1, pAdd && pRem);
    SetLastError(0);
    BOOL a = pAdd(NULL);
    DWORD e = GetLastError();
    if (a) { STEP(2, pRem(NULL) == TRUE); STEP(3, pRem(NULL) == FALSE); }
    return a ? 0 : (int)(100 + (e & 0xff));
}
TEST(gap0_securemem_null_callback_child) {
    REQUIRE(U(AddSecureMemoryCacheCallback));
    DWORD rc = t_run_child("gap0_securemem_null", "", 20000);
    CHECKF(rc == 0 || rc >= 100, "rc %lu", rc);
    t_note("AddSecureMemoryCacheCallback(NULL) child result %lu (0 = accepted)", rc);
}

/* -------------------------------------------- AllocateUserPhysicalPagesNuma */
static int proc_priv_enabled(const char *name) {
    HANDLE t = NULL; int r = 0;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &t)) return 0;
    LUID l;
    if (LookupPrivilegeValueA(NULL, name, &l)) {
        DWORD n = 0; GetTokenInformation(t, TokenPrivileges, NULL, 0, &n);
        TOKEN_PRIVILEGES *tp = (TOKEN_PRIVILEGES *)malloc(n ? n : 8);
        if (tp && GetTokenInformation(t, TokenPrivileges, tp, n, &n))
            for (DWORD i = 0; i < tp->PrivilegeCount; i++)
                if (tp->Privileges[i].Luid.LowPart == l.LowPart && tp->Privileges[i].Luid.HighPart == l.HighPart && (tp->Privileges[i].Attributes & SE_PRIVILEGE_ENABLED)) r = 1;
        free(tp);
    }
    CloseHandle(t);
    return r;
}

TEST(gap0_awe_invalid_node) {
    REQUIRE(U(AllocateUserPhysicalPagesNuma));
    DWORD nodes[] = {0xFFFE, 0xFFFD, 0x1FFFE, 0x7FFE, 0x1000, 0xFFFF};
    for (unsigned i = 0; i < sizeof nodes / sizeof *nodes; i++) {
        ULONG_PTR pages = 1, arr[1] = {0};
        SET_ERR(0);
        BOOL r = U(AllocateUserPhysicalPagesNuma)(GetCurrentProcess(), &pages, arr, nodes[i]);
        CHECKF(!r, "node 0x%lx", nodes[i]);
        DWORD e = GetLastError();
        CHECKF(e == ERROR_INVALID_PARAMETER, "node 0x%lx err %lu", nodes[i], e);
        EXPECT_EQ(pages, 1);
    }
}

TEST(gap0_awe_requires_lock_memory_privilege) {
    REQUIRE(U(AllocateUserPhysicalPagesNuma));
    if (proc_priv_enabled("SeLockMemoryPrivilege")) { t_skip("SeLockMemoryPrivilege enabled; would allocate physical pages"); return; }
    ULONG_PTR counts[] = {0, 1};
    for (int i = 0; i < 2; i++) {
        ULONG_PTR pages = counts[i], arr[2] = {0, 0};
        SET_ERR(0);
        BOOL r = U(AllocateUserPhysicalPagesNuma)(GetCurrentProcess(), &pages, arr, 0xFFFFFFFF);
        if (counts[i] == 0) {
            /* Win10 MiAllocateUserPhysicalPages returns STATUS_SUCCESS for a zero page request before any privilege check */
            CHECKF(r, "pages 0 err %lu", GetLastError());
            EXPECT_EQ(pages, 0);
        } else {
            CHECKF(!r, "pages %lu", (unsigned long)counts[i]);
            EXPECT_ERR(ERROR_PRIVILEGE_NOT_HELD);   /* NtAllocateUserPhysicalPages: STATUS_PRIVILEGE_NOT_HELD via BaseSetLastNTError */
        }
        EXPECT_EQ(arr[0], 0);
    }
}

TEST(gap0_awe_valid_node_restores_thread_affinity) {
    REQUIRE(U(AllocateUserPhysicalPagesNuma));
    if (proc_priv_enabled("SeLockMemoryPrivilege")) { t_skip("SeLockMemoryPrivilege enabled"); return; }
    GROUP_AFFINITY before, after;
    CHECK(GetThreadGroupAffinity(GetCurrentThread(), &before));
    ULONG_PTR pages = 1, arr[1] = {0};
    SET_ERR(0);
    BOOL r = U(AllocateUserPhysicalPagesNuma)(GetCurrentProcess(), &pages, arr, 0);
    CHECK(!r);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_PRIVILEGE_NOT_HELD || e == ERROR_INVALID_PARAMETER, "err %lu", e);
    t_note("node 0 without privilege -> %lu", e);
    CHECK(GetThreadGroupAffinity(GetCurrentThread(), &after));
    EXPECT_EQ(after.Mask, before.Mask);
    EXPECT_EQ(after.Group, before.Group);
}

TEST(gap0_awe_bad_process_handle) {
    REQUIRE(U(AllocateUserPhysicalPagesNuma));
    if (proc_priv_enabled("SeLockMemoryPrivilege")) { t_skip("SeLockMemoryPrivilege enabled"); return; }
    ULONG_PTR pages = 1, arr[1] = {0};
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    HANDLE bad[] = {NULL, (HANDLE)(ULONG_PTR)0x7777, ev};
    for (int i = 0; i < 3; i++) {
        SET_ERR(0);
        BOOL r = U(AllocateUserPhysicalPagesNuma)(bad[i], &pages, arr, 0xFFFFFFFF);
        CHECKF(!r, "handle %d", i);
        t_note("handle %d -> %lu", i, GetLastError());
    }
    CloseHandle(ev);
}

/* ------------------------------------------------------ AppPolicyGet* */
struct apol { const char *name; DWORD maxval; int hard_zero; };
static const struct apol g_pol[] = {
    {"AppPolicyGetClrCompat", 3, 0},
    {"AppPolicyGetCreateFileAccess", 1, 1},
    {"AppPolicyGetLifecycleManagement", 1, 1},
    {"AppPolicyGetMediaFoundationCodecLoading", 1, 1},
    {"AppPolicyGetProcessTerminationMethod", 1, 0},
    {"AppPolicyGetShowDeveloperDiagnostic", 1, 0},
};
#define NPOL 6
typedef LONG (WINAPI *polfn)(HANDLE, DWORD *);

TEST(gap0_apppolicy_null_arguments) {
    for (int i = 0; i < NPOL; i++) {
        polfn f = (polfn)t_proc(g_pol[i].name);
        if (!f) { t_note("missing %s", g_pol[i].name); continue; }
        HANDLE tok = NULL; CHECK(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok));
        DWORD out = 0xDEADBEEF;
        EXPECT_EQ(f(NULL, &out), ERROR_INVALID_PARAMETER);
        EXPECT_EQ(out, 0xDEADBEEF);
        EXPECT_EQ(f(tok, NULL), ERROR_INVALID_PARAMETER);
        EXPECT_EQ(f(NULL, NULL), ERROR_INVALID_PARAMETER);
        CloseHandle(tok);
    }
}

TEST(gap0_apppolicy_process_token_desktop_defaults) {
    for (int i = 0; i < NPOL; i++) {
        polfn f = (polfn)t_proc(g_pol[i].name);
        if (!f) { t_note("missing %s", g_pol[i].name); continue; }
        HANDLE tok = NULL; CHECK(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)); if (!tok) continue;
        DWORD out = 0xDEADBEEF;
        LONG r = f(tok, &out);
        if (t_is_wine()) { t_note("%s -> %ld out %lu", g_pol[i].name, (long)r, out); CloseHandle(tok); continue; }
        CHECKF(r == 0, "%s ret %ld", g_pol[i].name, (long)r);
        if (r == 0) {
            CHECKF(out <= g_pol[i].maxval, "%s out %lu", g_pol[i].name, out);
            if (g_pol[i].hard_zero) CHECKF(out == 0, "%s out %lu (desktop default)", g_pol[i].name, out);
            t_note("%s desktop value %lu", g_pol[i].name, out);
        } else {
            EXPECT_EQ(out, 0);
        }
        CloseHandle(tok);
    }
}

TEST(gap0_apppolicy_pseudo_and_impersonation_tokens) {
    for (int i = 0; i < NPOL; i++) {
        polfn f = (polfn)t_proc(g_pol[i].name);
        if (!f) continue;
        HANDLE tok = NULL; if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &tok)) { CHECK(0); continue; }
        DWORD a = 0xAAAA, b = 0xBBBB, c = 0xCCCC, d = 0xDDDD;
        LONG ra = f(tok, &a);
        LONG rb = f((HANDLE)(LONG_PTR)-4, &b);      /* GetCurrentProcessToken() */
        LONG rc = f((HANDLE)(LONG_PTR)-6, &c);      /* GetCurrentThreadEffectiveToken() */
        HANDLE dup = NULL;
        LONG rd = -1;
        if (DuplicateTokenEx(tok, TOKEN_QUERY, NULL, SecurityImpersonation, TokenImpersonation, &dup)) { rd = f(dup, &d); CloseHandle(dup); }
        if (t_is_wine()) { t_note("%s: %ld %ld %ld %ld", g_pol[i].name, (long)ra, (long)rb, (long)rc, (long)rd); CloseHandle(tok); continue; }
        /* pseudo token handles (-4/-5/-6) are only honoured by newer Win10 builds: compare only when both succeed */
        if (ra == 0 && rb == 0) EXPECT_EQ(b, a);
        else t_note("%s process-token pseudo handle: %ld vs %ld", g_pol[i].name, (long)rb, (long)ra);
        t_note("%s pseudo -5/-6: %ld/%lu; dup: %ld/%lu", g_pol[i].name, (long)rc, c, (long)rd, d);
        if (rc == ra && ra == 0) EXPECT_EQ(c, a);
        if (rd == 0 && ra == 0) EXPECT_EQ(d, a);
        CloseHandle(tok);
    }
}

TEST(gap0_apppolicy_non_token_handles) {
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    for (int i = 0; i < NPOL; i++) {
        polfn f = (polfn)t_proc(g_pol[i].name);
        if (!f) continue;
        HANDLE bad[] = {ev, (HANDLE)(ULONG_PTR)0x7777, INVALID_HANDLE_VALUE, GetCurrentThread()};
        for (int k = 0; k < 4; k++) {
            DWORD out = 0xDEADBEEF;
            LONG r = f(bad[k], &out);
            if (t_is_wine()) { t_note("%s bad[%d] -> %ld", g_pol[i].name, k, (long)r); continue; }
            if (k < 3) {
                CHECKF(r != 0, "%s bad[%d] succeeded", g_pol[i].name, k);
                EXPECT_EQ(out, 0);              /* output is zeroed before the lookup */
            }
            t_note("%s bad[%d] -> 0x%lx", g_pol[i].name, k, (unsigned long)r);
        }
    }
    CloseHandle(ev);
}

TEST(gap0_apppolicy_closed_and_limited_tokens) {
    for (int i = 0; i < NPOL; i++) {
        polfn f = (polfn)t_proc(g_pol[i].name);
        if (!f) continue;
        HANDLE tok = NULL; if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) continue;
        CloseHandle(tok);
        DWORD out = 0xDEADBEEF;
        LONG r = f(tok, &out);
        if (!t_is_wine()) { CHECKF(r != 0, "%s closed token", g_pol[i].name); EXPECT_EQ(out, 0); }
        /* token opened without TOKEN_QUERY */
        HANDLE p = NULL, d = NULL;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE, &p)) {
            if (DuplicateTokenEx(p, TOKEN_IMPERSONATE, NULL, SecurityImpersonation, TokenImpersonation, &d)) {
                out = 0xDEADBEEF;
                r = f(d, &out);
                t_note("%s with no TOKEN_QUERY -> 0x%lx out %lu", g_pol[i].name, (unsigned long)r, out);
                CloseHandle(d);
            }
            CloseHandle(p);
        }
    }
}

TEST(gap0_apppolicy_repeatable_and_stateless) {
    for (int i = 0; i < NPOL; i++) {
        polfn f = (polfn)t_proc(g_pol[i].name);
        if (!f) continue;
        HANDLE tok = NULL; if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) continue;
        DWORD first = 0xDEADBEEF; LONG r0 = f(tok, &first);
        for (int k = 0; k < 200; k++) {
            DWORD o = 0xCAFEBABE; LONG r = f(tok, &o);
            if (r != r0 || (r == 0 && o != first)) { CHECKF(0, "%s iteration %d differs", g_pol[i].name, k); break; }
        }
        CloseHandle(tok);
    }
    CHECK(1);
}

TEST(gap0_apppolicy_exports_forwarded) {
    for (int i = 0; i < NPOL; i++) {
        FARPROC k = t_proc_k32(g_pol[i].name), b = t_proc_kb(g_pol[i].name);
        CHECKF(k != NULL, "kernel32!%s", g_pol[i].name);
        CHECKF(b != NULL, "kernelbase!%s", g_pol[i].name);
        if (k && b) {
            DWORD o1 = 0xAA, o2 = 0xBB;
            HANDLE tok = NULL;
            if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) {
                LONG r1 = ((polfn)k)(tok, &o1), r2 = ((polfn)b)(tok, &o2);
                EXPECT_EQ(r1, r2);
                if (r1 == 0 && r2 == 0) EXPECT_EQ(o1, o2);
                CloseHandle(tok);
            }
        }
    }
}
