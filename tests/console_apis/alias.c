/* console_apis: aliases, command history, input exe name, control handlers and control events */
#include "ch.h"

#define FAILS(call, err)  do { SetLastError(0xBAD); BOOL r_ = (call); DWORD le_ = GetLastError(); CEQ(r_, 0); CERRV(le_, err); } while (0)
#define FAILRET0(call, err)  do { SetLastError(0xBAD); DWORD r_ = (DWORD)(call); DWORD le_ = GetLastError(); CEQ(r_, 0); CERRV(le_, err); } while (0)

UDECL(UINT, GetConsoleInputExeNameW, (DWORD, LPWSTR));
UDECL(UINT, GetConsoleInputExeNameA, (DWORD, LPSTR));
UDECL(BOOL, SetConsoleInputExeNameW, (LPCWSTR));
UDECL(BOOL, SetConsoleInputExeNameA, (LPCSTR));

#define EXE  L"k32t_alias.exe"
#define EXEA "k32t_alias.exe"
#define EXE2 L"other_tool.exe"

/* P1: Win10 conhost keys each command history to the exe name the process connected with (CommandHistory::s_Allocate
   at connect, found again by process handle in COOKED_READ_DATA); SetConsoleInputExeName only changes the name used
   for alias lookup. History queries must therefore use the real image name, and the history is shared by every
   process with that name on the console, so tests start from an expunged history. UNVERIFIED-until-rerun */
static WCHAR *realexe(void) {
    static WCHAR n[MAX_PATH];
    if (!n[0]) { WCHAR p[MAX_PATH]; GetModuleFileNameW(NULL, p, MAX_PATH); WCHAR *sl = wcsrchr(p, L'\\'); lstrcpyW(n, sl ? sl + 1 : p); }
    return n;
}
static char *realexea(void) {
    static char n[MAX_PATH];
    if (!n[0]) WideCharToMultiByte(CP_ACP, 0, realexe(), -1, n, sizeof n, NULL, NULL);
    return n;
}
#define REALW realexe()
#define REALA realexea()
static void setexe(void) {
    SetConsoleInputExeNameW_pfn f = UN(SetConsoleInputExeNameW);
    if (f) f(EXE);
}
static void cooked(void) {
    SetConsoleMode(hin_(), ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
    inp_reset();
}
static BOOL readline(WCHAR *b, DWORD cap, DWORD *n, const WCHAR *typed) {
    typestr(hin_(), typed);
    return ReadConsoleW(hin_(), b, cap, n, NULL);
}

/* ---------------- SetConsoleInputExeName / GetConsoleInputExeName ---------------- */

CT(con_exename_initial_state) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    GetConsoleInputExeNameW_pfn g = UN(GetConsoleInputExeNameW);
    CREQ(g);
    WCHAR b[300]; for (int i = 0; i < 300; i++) b[i] = 0x7777;
    SetLastError(0xBAD);
    UINT r = g(256, b);
    cnote("initial exe name: ret=%u \"%ls\"", r, b);
    /* P1 EXPECT-FIX: Win10 kernelbase initializes the input exe name to the process image name (baseline
       "console_apis.exe"), it is not empty; with a zero-size buffer the call returns length+1 and sets
       ERROR_BUFFER_OVERFLOW (kernelbase GetConsoleInputExeNameW). */
    CEQ(r, 1);
    CC(!lstrcmpiW(b, realexe()));
    SetLastError(0xBAD);
    r = g(0, b);
    { DWORD e_ = GetLastError(); CEQ(r, lstrlenW(realexe()) + 1); CERRV(e_, ERROR_BUFFER_OVERFLOW); }
}

CT(con_exename_set_get_w) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    GetConsoleInputExeNameW_pfn g = UN(GetConsoleInputExeNameW);
    SetConsoleInputExeNameW_pfn s = UN(SetConsoleInputExeNameW);
    CREQ(g); CREQ(s);
    WCHAR b[300];
    SetLastError(0xBAD);
    CC(s(L"foo.exe"));
    for (int i = 0; i < 300; i++) b[i] = 0x7777;
    CEQ(g(100, b), 1);
    CWSTR(b, L"foo.exe");
    CEQ(b[8], 0x7777);
    for (int i = 0; i < 300; i++) b[i] = 0x7777;
    CEQ(g(8, b), 1);
    CWSTR(b, L"foo.exe");
    SetLastError(0xBAD);
    CEQ(g(7, b), 8); CERR(ERROR_BUFFER_OVERFLOW);
    SetLastError(0xBAD);
    CEQ(g(1, b), 8); CERR(ERROR_BUFFER_OVERFLOW);
    CC(s(L"second.exe"));
    CEQ(g(100, b), 1); CWSTR(b, L"second.exe");
}

CT(con_exename_length_limits) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    GetConsoleInputExeNameW_pfn g = UN(GetConsoleInputExeNameW);
    SetConsoleInputExeNameW_pfn s = UN(SetConsoleInputExeNameW);
    CREQ(g); CREQ(s);
    static WCHAR l[400], b[400];
    for (int i = 0; i < 399; i++) l[i] = L'a';
    l[255] = 0;
    CC(s(l));
    CEQ(g(300, b), 1); CEQ(lstrlenW(b), 255);
    l[255] = L'a'; l[256] = 0;
    FAILS(s(l), ERROR_INVALID_PARAMETER);
    CEQ(lstrlenW(b), 255);
    FAILS(s(L""), ERROR_INVALID_PARAMETER);
    CEQ(g(300, b), 1); CEQ(lstrlenW(b), 255);   /* unchanged by failed sets */
}

CT(con_exename_set_get_a) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    GetConsoleInputExeNameA_pfn g = UN(GetConsoleInputExeNameA);
    SetConsoleInputExeNameA_pfn s = UN(SetConsoleInputExeNameA);
    GetConsoleInputExeNameW_pfn gw = UN(GetConsoleInputExeNameW);
    CREQ(g); CREQ(s); CREQ(gw);
    char b[300]; WCHAR w[300];
    CC(s("bar.exe"));
    memset(b, 0x55, sizeof b);
    SetLastError(0xBAD);
    CEQ(g(100, b), 1);
    CSTR(b, "bar.exe");
    CEQ(gw(100, w), 1); CWSTR(w, L"bar.exe");
    SetLastError(0xBAD);
    UINT r = g(3, b);
    DWORD e_ = GetLastError();   /* P1 GUARD: capture before cnote, whose printf clobbers the last error */
    cnote("GetConsoleInputExeNameA(3) -> %u err=%lu", r, e_);
    CEQ(r, 2); CERRV(e_, ERROR_BUFFER_OVERFLOW);
    static char l[400]; memset(l, 'x', 256); l[256] = 0;
    FAILS(s(l), ERROR_INVALID_PARAMETER);
    FAILS(s(""), ERROR_INVALID_PARAMETER);
    CEQ(g(100, b), 1); CSTR(b, "bar.exe");
}

CT(con_exename_process_scoped) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    GetConsoleInputExeNameW_pfn g = UN(GetConsoleInputExeNameW);
    SetConsoleInputExeNameW_pfn s = UN(SetConsoleInputExeNameW);
    CREQ(g); CREQ(s);
    WCHAR b[64];
    CC(s(L"proc-scoped.exe"));
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    CNE(ev, NULL);
    CEQ(g(64, b), 1); CWSTR(b, L"proc-scoped.exe");
    CloseHandle(ev);
}

/* ---------------- aliases ---------------- */

CT(con_alias_add_get_w) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(AddConsoleAliasW); CREQ(GetConsoleAliasW);
    WCHAR b[64]; for (int i = 0; i < 64; i++) b[i] = 0x7777;
    SetLastError(0xBAD);
    CCF(AddConsoleAliasW(L"ll", L"dir /w $*", EXE), "err=%lu", GetLastError());
    DWORD n = GetConsoleAliasW(L"ll", b, sizeof b, EXE);
    CEQ(n, 20);                       /* bytes including the terminating NUL */
    CWSTR(b, L"dir /w $*");
    CEQ(b[10], 0x7777);
}

CT(con_alias_add_get_a) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(AddConsoleAliasA); CREQ(GetConsoleAliasA);
    char b[64]; memset(b, 0x55, sizeof b);
    CCF(AddConsoleAliasA("la", "ls -al $*", EXEA), "err=%lu", GetLastError());
    DWORD n = GetConsoleAliasA("la", b, sizeof b, EXEA);
    CEQ(n, 10);
    CSTR(b, "ls -al $*");
    CEQ((unsigned char)b[10], 0x55);
}

CT(con_alias_a_w_parity) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(AddConsoleAliasA); CREQ(GetConsoleAliasW); CREQ(AddConsoleAliasW); CREQ(GetConsoleAliasA);
    WCHAR w[64]; char a[64];
    CC(AddConsoleAliasA("pa", "from-ansi", EXEA));
    CEQ(GetConsoleAliasW(L"pa", w, sizeof w, EXE), 20); CWSTR(w, L"from-ansi");
    CC(AddConsoleAliasW(L"pw", L"from-wide", EXE));
    CEQ(GetConsoleAliasA("pw", a, sizeof a, EXEA), 10); CSTR(a, "from-wide");
}

CT(con_alias_unicode_text) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(AddConsoleAliasW); CREQ(GetConsoleAliasW);
    static const WCHAR src[] = { 'u', 0x416, 0 }, tgt[] = { 0x4E2D, 'x', 0x20AC, 0 };
    WCHAR b[16];
    CC(AddConsoleAliasW((LPWSTR)src, (LPWSTR)tgt, EXE));
    CEQ(GetConsoleAliasW((LPWSTR)src, b, sizeof b, EXE), 8);
    CWSTR(b, tgt);
}

CT(con_alias_replace_existing) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(AddConsoleAliasW); CREQ(GetConsoleAliasW);
    WCHAR b[64];
    CC(AddConsoleAliasW(L"rr", L"first", EXE));
    CC(AddConsoleAliasW(L"rr", L"second one", EXE));
    CEQ(GetConsoleAliasW(L"rr", b, sizeof b, EXE), 22);
    CWSTR(b, L"second one");
}

CT(con_alias_delete_with_null_target) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(AddConsoleAliasW); CREQ(GetConsoleAliasW);
    WCHAR b[64];
    CC(AddConsoleAliasW(L"dd", L"gone soon", EXE));
    CEQ(GetConsoleAliasW(L"dd", b, sizeof b, EXE), 20);
    SetLastError(0xBAD);
    CCF(AddConsoleAliasW(L"dd", NULL, EXE), "err=%lu", GetLastError());
    SetLastError(0xBAD);
    DWORD n = GetConsoleAliasW(L"dd", b, sizeof b, EXE);
    cnote("after delete: ret=%lu err=%lu", n, GetLastError());
    CEQ(n, 0);
}

CT(con_alias_delete_missing) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(AddConsoleAliasW);
    SetLastError(0xBAD);
    BOOL r = AddConsoleAliasW(L"neverexisted", NULL, EXE);
    cnote("delete of nonexistent alias -> %d err=%lu", r, GetLastError());
}

CT(con_alias_empty_target) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(AddConsoleAliasW); CREQ(GetConsoleAliasW);
    WCHAR b[16];
    CC(AddConsoleAliasW(L"ee", L"something", EXE));
    SetLastError(0xBAD);
    BOOL r = AddConsoleAliasW(L"ee", L"", EXE);
    cnote("empty target -> %d err=%lu", r, GetLastError());
    SetLastError(0xBAD);
    DWORD n = GetConsoleAliasW(L"ee", b, sizeof b, EXE);
    cnote("after empty target: ret=%lu err=%lu text=\"%ls\"", n, GetLastError(), n ? b : L"");
}

CT(con_alias_get_missing) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(GetConsoleAliasW); CREQ(GetConsoleAliasA);
    WCHAR b[16]; char a[16];
    SetLastError(0xBAD);
    CEQ(GetConsoleAliasW(L"nope", b, sizeof b, EXE), 0);
    cnote("missing alias W err=%lu", GetLastError());
    SetLastError(0xBAD);
    CEQ(GetConsoleAliasA("nope", a, sizeof a, EXEA), 0);
    cnote("missing alias A err=%lu", GetLastError());
}

CT(con_alias_get_small_buffer) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(AddConsoleAliasW); CREQ(GetConsoleAliasW);
    WCHAR b[32]; for (int i = 0; i < 32; i++) b[i] = 0x7777;
    CC(AddConsoleAliasW(L"sm", L"12345", EXE));    /* needs 12 bytes */
    SetLastError(0xBAD);
    DWORD n = GetConsoleAliasW(L"sm", b, 4, EXE);
    DWORD e_ = GetLastError();   /* P1 GUARD: capture before cnote, whose printf clobbers the last error */
    cnote("small buffer: ret=%lu err=%lu b0=0x%x", n, e_, b[0]);
    CERRV(e_, ERROR_INSUFFICIENT_BUFFER);
    CEQ(b[8], 0x7777);
    n = GetConsoleAliasW(L"sm", b, 12, EXE);
    CEQ(n, 12);
    CWSTR(b, L"12345");
    SetLastError(0xBAD);
    n = GetConsoleAliasW(L"sm", b, 10, EXE);
    e_ = GetLastError();
    cnote("11-bytes-short buffer: ret=%lu err=%lu", n, e_);
    CERRV(e_, ERROR_INSUFFICIENT_BUFFER);
    SetLastError(0xBAD);
    n = GetConsoleAliasW(L"sm", b, 0, EXE);
    cnote("zero-size buffer: ret=%lu err=%lu", n, GetLastError());
}

CT(con_alias_invalid_exe_name) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(AddConsoleAliasW); CREQ(GetConsoleAliasW); CREQ(AddConsoleAliasA); CREQ(GetConsoleAliasA);
    WCHAR b[16]; char a[16];
    FAILS(AddConsoleAliasW(L"x", L"y", NULL), ERROR_INVALID_PARAMETER);
    FAILS(AddConsoleAliasW(L"x", L"y", L""), ERROR_INVALID_PARAMETER);
    FAILS(AddConsoleAliasA("x", "y", NULL), ERROR_INVALID_PARAMETER);
    FAILS(AddConsoleAliasA("x", "y", ""), ERROR_INVALID_PARAMETER);
    FAILRET0(GetConsoleAliasW(L"x", b, sizeof b, NULL), ERROR_INVALID_PARAMETER);
    FAILRET0(GetConsoleAliasW(L"x", b, sizeof b, L""), ERROR_INVALID_PARAMETER);
    FAILRET0(GetConsoleAliasA("x", a, sizeof a, NULL), ERROR_INVALID_PARAMETER);
    FAILRET0(GetConsoleAliasA("x", a, sizeof a, ""), ERROR_INVALID_PARAMETER);
}

CT(con_alias_invalid_source) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(AddConsoleAliasW); CREQ(GetConsoleAliasW); CREQ(GetConsoleAliasA); CREQ(AddConsoleAliasA);
    WCHAR b[16]; char a[16];
    FAILRET0(GetConsoleAliasW(NULL, b, sizeof b, EXE), ERROR_INVALID_PARAMETER);
    FAILRET0(GetConsoleAliasA(NULL, a, sizeof a, EXEA), ERROR_INVALID_PARAMETER);
    SetLastError(0xBAD);
    BOOL r = AddConsoleAliasW(NULL, L"t", EXE);
    cnote("AddConsoleAliasW(NULL source) -> %d err=%lu", r, GetLastError());
    CEQ(r, 0);
    SetLastError(0xBAD);
    r = AddConsoleAliasW(L"", L"t", EXE);
    cnote("AddConsoleAliasW(empty source) -> %d err=%lu", r, GetLastError());
    CEQ(r, 0);
    SetLastError(0xBAD);
    r = AddConsoleAliasA("", "t", EXEA);
    CEQ(r, 0);
}

CT(con_alias_per_exe_scoping) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(AddConsoleAliasW); CREQ(GetConsoleAliasW);
    WCHAR b[32];
    CC(AddConsoleAliasW(L"sc", L"for-one", EXE));
    CC(AddConsoleAliasW(L"sc", L"for-two", EXE2));
    CEQ(GetConsoleAliasW(L"sc", b, sizeof b, EXE), 16); CWSTR(b, L"for-one");
    CEQ(GetConsoleAliasW(L"sc", b, sizeof b, EXE2), 16); CWSTR(b, L"for-two");
    SetLastError(0xBAD);
    CEQ(GetConsoleAliasW(L"sc", b, sizeof b, L"third.exe"), 0);
}

CT(con_alias_case_insensitivity) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(AddConsoleAliasW); CREQ(GetConsoleAliasW);
    WCHAR b[32];
    CC(AddConsoleAliasW(L"CaseTest", L"v", EXE));
    DWORD n1 = GetConsoleAliasW(L"casetest", b, sizeof b, EXE);
    DWORD n2 = GetConsoleAliasW(L"CaseTest", b, sizeof b, EXE);
    DWORD n3 = GetConsoleAliasW(L"CaseTest", b, sizeof b, L"K32T_ALIAS.EXE");
    cnote("alias lookup: lower=%lu exact=%lu upper-exe=%lu", n1, n2, n3);
    CEQ(n2, 4);
}

CT(con_alias_list_w) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(AddConsoleAliasW); CREQ(GetConsoleAliasesW); CREQ(GetConsoleAliasesLengthW);
    CC(AddConsoleAliasW(L"a1", L"t1", EXE));
    CC(AddConsoleAliasW(L"b22", L"t22", EXE));
    CC(AddConsoleAliasW(L"c", L"long target text", EXE));
    DWORD need = (2 + 1 + 2 + 1 + 3 + 1 + 3 + 1 + 1 + 1 + 16 + 1) * 2;
    SetLastError(0xBAD);
    DWORD len = GetConsoleAliasesLengthW(EXE);
    CEQ(len, need);
    WCHAR b[128]; for (int i = 0; i < 128; i++) b[i] = 0x7777;
    DWORD n = GetConsoleAliasesW(b, sizeof b, EXE);
    CEQ(n, need);
    int f1 = 0, f2 = 0, f3 = 0;
    for (const WCHAR *p = b; (DWORD)(p - b) * 2 < n && *p; p += lstrlenW(p) + 1) {
        if (!lstrcmpW(p, L"a1=t1")) f1++;
        if (!lstrcmpW(p, L"b22=t22")) f2++;
        if (!lstrcmpW(p, L"c=long target text")) f3++;
    }
    CEQ(f1, 1); CEQ(f2, 1); CEQ(f3, 1);
}

CT(con_alias_list_a) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(AddConsoleAliasA); CREQ(GetConsoleAliasesA); CREQ(GetConsoleAliasesLengthA);
    CC(AddConsoleAliasA("x1", "yy", EXEA));
    CC(AddConsoleAliasA("x22", "y", EXEA));
    DWORD need = (2 + 1 + 2 + 1) + (3 + 1 + 1 + 1);
    CEQ(GetConsoleAliasesLengthA(EXEA), need);
    char b[64]; memset(b, 0x55, sizeof b);
    DWORD n = GetConsoleAliasesA(b, sizeof b, EXEA);
    CEQ(n, need);
    int f1 = 0, f2 = 0;
    for (const char *p = b; (DWORD)(p - b) < n && *p; p += strlen(p) + 1) {
        if (!strcmp(p, "x1=yy")) f1++;
        if (!strcmp(p, "x22=y")) f2++;
    }
    CEQ(f1, 1); CEQ(f2, 1);
}

CT(con_alias_list_empty) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(GetConsoleAliasesLengthW); CREQ(GetConsoleAliasesW);
    SetLastError(0xBAD);
    DWORD len = GetConsoleAliasesLengthW(L"no_aliases_here.exe");
    cnote("aliases length for exe without aliases: %lu err=%lu", len, GetLastError());
    CEQ(len, 0);
    WCHAR b[8];
    SetLastError(0xBAD);
    DWORD n = GetConsoleAliasesW(b, sizeof b, L"no_aliases_here.exe");
    cnote("aliases for exe without aliases: %lu err=%lu", n, GetLastError());
    CEQ(n, 0);
}

CT(con_alias_list_small_buffer) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(AddConsoleAliasW); CREQ(GetConsoleAliasesW); CREQ(GetConsoleAliasesLengthW);
    CC(AddConsoleAliasW(L"abc", L"defgh", EXE));
    DWORD need = GetConsoleAliasesLengthW(EXE);
    CEQ(need, (3 + 1 + 5 + 1) * 2);
    WCHAR b[32]; for (int i = 0; i < 32; i++) b[i] = 0x7777;
    SetLastError(0xBAD);
    DWORD n = GetConsoleAliasesW(b, need - 2, EXE);
    cnote("short buffer: ret=%lu err=%lu", n, GetLastError());
    CEQ(n, 0);
    CEQ(b[need / 2], 0x7777);
    n = GetConsoleAliasesW(b, need, EXE);
    CEQ(n, need);
}

CT(con_alias_list_invalid_exe) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(GetConsoleAliasesW); CREQ(GetConsoleAliasesLengthW); CREQ(GetConsoleAliasesA); CREQ(GetConsoleAliasesLengthA);
    WCHAR b[8]; char a[8];
    FAILRET0(GetConsoleAliasesLengthW(NULL), ERROR_INVALID_PARAMETER);
    FAILRET0(GetConsoleAliasesLengthW(L""), ERROR_INVALID_PARAMETER);
    FAILRET0(GetConsoleAliasesLengthA(NULL), ERROR_INVALID_PARAMETER);
    FAILRET0(GetConsoleAliasesW(b, sizeof b, NULL), ERROR_INVALID_PARAMETER);
    FAILRET0(GetConsoleAliasesW(b, sizeof b, L""), ERROR_INVALID_PARAMETER);
    FAILRET0(GetConsoleAliasesA(a, sizeof a, NULL), ERROR_INVALID_PARAMETER);
}

CT(con_alias_exes_list) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(AddConsoleAliasW); CREQ(GetConsoleAliasExesW); CREQ(GetConsoleAliasExesLengthW);
    CC(AddConsoleAliasW(L"q", L"1", EXE));
    CC(AddConsoleAliasW(L"q", L"2", EXE2));
    DWORD need = (14 + 1 + 14 + 1) * 2;
    DWORD len = GetConsoleAliasExesLengthW();
    CEQ(len, need);
    WCHAR b[128]; for (int i = 0; i < 128; i++) b[i] = 0x7777;
    DWORD n = GetConsoleAliasExesW(b, sizeof b);
    CEQ(n, need);
    int f1 = 0, f2 = 0;
    for (const WCHAR *p = b; (DWORD)(p - b) * 2 < n && *p; p += lstrlenW(p) + 1) {
        if (!lstrcmpiW(p, EXE)) f1++;
        if (!lstrcmpiW(p, EXE2)) f2++;
    }
    CEQ(f1, 1); CEQ(f2, 1);
}

CT(con_alias_exes_list_a) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(AddConsoleAliasA); CREQ(GetConsoleAliasExesA); CREQ(GetConsoleAliasExesLengthA);
    CC(AddConsoleAliasA("q", "1", "one.exe"));
    CC(AddConsoleAliasA("q", "2", "two.exe"));
    DWORD need = 8 + 8;
    CEQ(GetConsoleAliasExesLengthA(), need);
    char b[64]; memset(b, 0x55, sizeof b);
    DWORD n = GetConsoleAliasExesA(b, sizeof b);
    CEQ(n, need);
    int f1 = 0, f2 = 0;
    for (const char *p = b; (DWORD)(p - b) < n && *p; p += strlen(p) + 1) {
        if (!lstrcmpiA(p, "one.exe")) f1++;
        if (!lstrcmpiA(p, "two.exe")) f2++;
    }
    CEQ(f1, 1); CEQ(f2, 1);
}

CT(con_alias_exes_empty_and_small) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(AddConsoleAliasW); CREQ(GetConsoleAliasExesW); CREQ(GetConsoleAliasExesLengthW);
    SetLastError(0xBAD);
    CEQ(GetConsoleAliasExesLengthW(), 0);
    WCHAR b[32]; for (int i = 0; i < 32; i++) b[i] = 0x7777;
    SetLastError(0xBAD);
    CEQ(GetConsoleAliasExesW(b, sizeof b), 0);
    CC(AddConsoleAliasW(L"z", L"1", L"abcdef.exe"));
    SetLastError(0xBAD);
    DWORD n = GetConsoleAliasExesW(b, 6);
    cnote("short exes buffer: ret=%lu err=%lu", n, GetLastError());
    CEQ(n, 0);
}

CT(con_alias_delete_last_removes_exe) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(AddConsoleAliasW); CREQ(GetConsoleAliasExesLengthW);
    CC(AddConsoleAliasW(L"only", L"one", L"lonely.exe"));
    CEQ(GetConsoleAliasExesLengthW(), (10 + 1) * 2);
    CC(AddConsoleAliasW(L"only", NULL, L"lonely.exe"));
    DWORD n = GetConsoleAliasExesLengthW();
    cnote("exes length after deleting last alias: %lu", n);
}

CT(con_alias_many) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(AddConsoleAliasW); CREQ(GetConsoleAliasW);
    for (int i = 0; i < 200; i++) {
        WCHAR s[16], t[24];
        wsprintfW(s, L"al%03d", i); wsprintfW(t, L"target-%03d", i);
        CC(AddConsoleAliasW(s, t, EXE));
    }
    for (int i = 0; i < 200; i++) {
        WCHAR s[16], t[24], g[24];
        wsprintfW(s, L"al%03d", i); wsprintfW(t, L"target-%03d", i);
        DWORD n = GetConsoleAliasW(s, g, sizeof g, EXE);
        CCF(n == (DWORD)(lstrlenW(t) + 1) * 2 && !lstrcmpW(g, t), "alias %d", i);
    }
}

CT(con_alias_long_text) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(AddConsoleAliasW); CREQ(GetConsoleAliasW);
    static WCHAR t[2001], g[2100];
    for (int i = 0; i < 2000; i++) t[i] = L'a' + (i % 26);
    t[2000] = 0;
    SetLastError(0xBAD);
    BOOL ok = AddConsoleAliasW(L"longone", t, EXE);
    cnote("2000-char alias -> %d err=%lu", ok, GetLastError());
    if (ok) {
        DWORD n = GetConsoleAliasW(L"longone", g, sizeof g, EXE);
        CEQ(n, 4002);
        CWSTR(g, t);
    }
}

/* ---------------- alias expansion by ReadConsole ---------------- */

CT(con_alias_expansion_star) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(AddConsoleAliasW); CREQ(ReadConsoleW); CREQ(UN(SetConsoleInputExeNameW));
    setexe(); cooked();
    CC(AddConsoleAliasW(L"gg", L"hello $*", EXE));
    WCHAR b[64] = { 0 }; DWORD n = 0;
    CC(readline(b, 64, &n, L"gg world and more\r"));
    cnote("expanded: \"%ls\" n=%lu", b, n);
    CWSTR(b, L"hello world and more\r\n");
    CEQ(n, 22);
}

CT(con_alias_expansion_positional) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(AddConsoleAliasW); CREQ(ReadConsoleW);
    setexe(); cooked();
    CC(AddConsoleAliasW(L"pp", L"[$1|$2|$3]", EXE));
    WCHAR b[64] = { 0 }; DWORD n = 0;
    CC(readline(b, 64, &n, L"pp a b c d\r"));
    cnote("positional expanded: \"%ls\"", b);
    CWSTR(b, L"[a|b|c]\r\n");
}

CT(con_alias_expansion_no_args) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(AddConsoleAliasW); CREQ(ReadConsoleW);
    setexe(); cooked();
    CC(AddConsoleAliasW(L"na", L"dir", EXE));
    WCHAR b[64] = { 0 }; DWORD n = 0;
    CC(readline(b, 64, &n, L"na\r"));
    CWSTR(b, L"dir\r\n");
}

CT(con_alias_expansion_only_first_word) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(AddConsoleAliasW); CREQ(ReadConsoleW);
    setexe(); cooked();
    CC(AddConsoleAliasW(L"fw", L"EXPANDED", EXE));
    WCHAR b[64] = { 0 }; DWORD n = 0;
    CC(readline(b, 64, &n, L"echo fw\r"));
    CWSTR(b, L"echo fw\r\n");
}

CT(con_alias_expansion_wrong_exe) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(AddConsoleAliasW); CREQ(ReadConsoleW);
    setexe(); cooked();
    CC(AddConsoleAliasW(L"we", L"EXPANDED", EXE2));
    WCHAR b[64] = { 0 }; DWORD n = 0;
    CC(readline(b, 64, &n, L"we\r"));
    CWSTR(b, L"we\r\n");
}

CT(con_alias_expansion_after_delete) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(AddConsoleAliasW); CREQ(ReadConsoleW);
    setexe(); cooked();
    CC(AddConsoleAliasW(L"ad", L"EXPANDED", EXE));
    CC(AddConsoleAliasW(L"ad", NULL, EXE));
    WCHAR b[64] = { 0 }; DWORD n = 0;
    CC(readline(b, 64, &n, L"ad\r"));
    CWSTR(b, L"ad\r\n");
}

CT(con_alias_expansion_not_in_raw_mode) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(AddConsoleAliasW); CREQ(ReadConsoleW);
    setexe();
    SetConsoleMode(hin_(), 0); inp_reset();
    CC(AddConsoleAliasW(L"rw", L"EXPANDED", EXE));
    typestr(hin_(), L"rw\r");
    WCHAR b[64] = { 0 }; DWORD n = 0;
    CC(ReadConsoleW(hin_(), b, 64, &n, NULL));
    CEQ(n, 3);
    CC(b[0] == L'r' && b[1] == L'w' && b[2] == L'\r');
}

CT(con_alias_expansion_dollar_dollar) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(AddConsoleAliasW); CREQ(ReadConsoleW);
    setexe(); cooked();
    CC(AddConsoleAliasW(L"dl", L"cost $$5", EXE));
    WCHAR b[64] = { 0 }; DWORD n = 0;
    CC(readline(b, 64, &n, L"dl\r"));
    cnote("$$ expanded: \"%ls\"", b);
    /* P1 EXPECT-FIX: Win10 conhost Alias::s_ReplaceMacros only knows $1-$9, $*, $L, $G, $B, $T; for any other
       character after '$' it emits both characters unchanged, so "$$" stays "$$" (baseline "cost $$5"). */
    CWSTR(b, L"cost $$5\r\n");
}

CT(con_alias_expansion_separator) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(AddConsoleAliasW); CREQ(ReadConsoleW);
    setexe(); cooked();
    CC(AddConsoleAliasW(L"sp", L"one $T two", EXE));
    WCHAR b[64] = { 0 }; DWORD n = 0;
    CC(readline(b, 64, &n, L"sp\r"));
    cnote("$T expanded: \"%ls\" n=%lu", b, n);
    CC(b[0] == L'o');
}

/* ---------------- command history ---------------- */

CT(con_hist_records_cooked_lines) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(ReadConsoleW); CREQ(GetConsoleCommandHistoryW); CREQ(GetConsoleCommandHistoryLengthW);
    setexe(); cooked();
    /* P1b: no pre-expunge. Win10 conhost ExpungeConsoleCommandHistory(W/A) on the history of a live process
       assigns the history flags = CLE_RESET (mov dword [rbx+0x48],2 in ExpungeConsoleCommandHistoryWImpl), wiping
       CLE_ALLOCATED, and the next cooked read of that process fail-fasts in CommandHistory::s_Find. The child owns a
       brand new console, so its history already starts empty. UNVERIFIED-until-rerun */
    WCHAR b[64]; DWORD n;
    CC(readline(b, 64, &n, L"one\r"));
    CC(readline(b, 64, &n, L"two2\r"));
    CC(readline(b, 64, &n, L"three\r"));
    DWORD need = (3 + 1 + 4 + 1 + 5 + 1) * 2;
    SetLastError(0xBAD);
    DWORD len = GetConsoleCommandHistoryLengthW(REALW);
    CEQ(len, need);
    WCHAR h[64]; for (int i = 0; i < 64; i++) h[i] = 0x7777;
    DWORD r = GetConsoleCommandHistoryW(h, sizeof h, REALW);
    CEQ(r, need);
    CC(!lstrcmpW(h, L"one"));
    CC(!lstrcmpW(h + 4, L"two2"));
    CC(!lstrcmpW(h + 9, L"three"));
}

CT(con_hist_skips_empty_lines) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(ReadConsoleW); CREQ(GetConsoleCommandHistoryLengthW);
    setexe(); cooked();
    WCHAR b[64]; DWORD n;
    CC(readline(b, 64, &n, L"\r"));
    CC(readline(b, 64, &n, L"real\r"));
    CC(readline(b, 64, &n, L"\r"));
    CEQ(GetConsoleCommandHistoryLengthW(REALW), 10);
}

CT(con_hist_a_variant) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(ReadConsoleW); CREQ(GetConsoleCommandHistoryA); CREQ(GetConsoleCommandHistoryLengthA);
    setexe(); cooked();
    WCHAR b[64]; DWORD n;
    CC(readline(b, 64, &n, L"ab\r"));
    CC(readline(b, 64, &n, L"cde\r"));
    CEQ(GetConsoleCommandHistoryLengthA(REALA), 3 + 4);
    char h[32]; memset(h, 0x55, sizeof h);
    CEQ(GetConsoleCommandHistoryA(h, sizeof h, REALA), 7);
    CSTR(h, "ab"); CSTR(h + 3, "cde");
}

CT(con_hist_expunge) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(ReadConsoleW); CREQ(ExpungeConsoleCommandHistoryW); CREQ(GetConsoleCommandHistoryLengthW);
    setexe(); cooked();
    WCHAR b[64]; DWORD n;
    CC(readline(b, 64, &n, L"keepme\r"));
    CEQ(GetConsoleCommandHistoryLengthW(REALW), 14);
    SetLastError(0xBAD);
    ExpungeConsoleCommandHistoryW(REALW);
    CEQ(GetConsoleCommandHistoryLengthW(REALW), 0);
    /* P1b: nothing may read a line after this point: the expunge leaves the process history unallocated and the
       next cooked read kills conhost (see con_hist_records_cooked_lines). Re-recording after an expunge is
       therefore not testable on Win10 and the former "again" check was dropped. UNVERIFIED-until-rerun */
}

CT(con_hist_expunge_a) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(ReadConsoleW); CREQ(ExpungeConsoleCommandHistoryA); CREQ(GetConsoleCommandHistoryLengthA);
    setexe(); cooked();
    WCHAR b[64]; DWORD n;
    CC(readline(b, 64, &n, L"hist\r"));
    CEQ(GetConsoleCommandHistoryLengthA(REALA), 5);
    ExpungeConsoleCommandHistoryA(REALA);
    CEQ(GetConsoleCommandHistoryLengthA(REALA), 0);
}

CT(con_hist_set_number_of_commands) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(ReadConsoleW); CREQ(SetConsoleNumberOfCommandsW); CREQ(GetConsoleCommandHistoryW); CREQ(GetConsoleCommandHistoryLengthW);
    setexe(); cooked();
    WCHAR b[64]; DWORD n;
    CC(readline(b, 64, &n, L"first\r"));
    SetLastError(0xBAD);
    CCF(SetConsoleNumberOfCommandsW(2, REALW), "err=%lu", GetLastError());
    CC(readline(b, 64, &n, L"second\r"));
    CC(readline(b, 64, &n, L"third\r"));
    DWORD len = GetConsoleCommandHistoryLengthW(REALW);
    cnote("history length after limiting to 2: %lu", len);
    WCHAR h[64]; for (int i = 0; i < 64; i++) h[i] = 0;
    DWORD r = GetConsoleCommandHistoryW(h, sizeof h, REALW);
    /* P1c UNRESOLVED: baseline len=0x26 (first+second+third all kept) although the limit is 2. NOTE prints the entries. */
    { WCHAR *p_ = h; int k_ = 0; while (*p_ && k_ < 6) { cnote("history entry %d: %ls", k_, p_); p_ += lstrlenW(p_) + 1; k_++; } }
    CEQ(r, len);
    CEQ(len, (6 + 1 + 5 + 1) * 2);
    CC(!lstrcmpW(h, L"second"));
    CC(!lstrcmpW(h + 7, L"third"));
    SetConsoleNumberOfCommandsW(50, REALW);   /* P1: the limit lives in the shared per-exe history, restore it */
}

CT(con_hist_set_number_a) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(SetConsoleNumberOfCommandsA);
    SetLastError(0xBAD);
    BOOL r = SetConsoleNumberOfCommandsA(5, EXEA);
    cnote("SetConsoleNumberOfCommandsA(5) -> %d err=%lu", r, GetLastError());
    CC(r);
}

CT(con_hist_invalid_exe) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(GetConsoleCommandHistoryW); CREQ(GetConsoleCommandHistoryLengthW); CREQ(SetConsoleNumberOfCommandsW); CREQ(ExpungeConsoleCommandHistoryW);
    CREQ(GetConsoleCommandHistoryA); CREQ(GetConsoleCommandHistoryLengthA); CREQ(SetConsoleNumberOfCommandsA); CREQ(ExpungeConsoleCommandHistoryA);
    WCHAR b[8]; char a[8];
    FAILRET0(GetConsoleCommandHistoryLengthW(NULL), ERROR_INVALID_PARAMETER);
    FAILRET0(GetConsoleCommandHistoryLengthW(L""), ERROR_INVALID_PARAMETER);
    FAILRET0(GetConsoleCommandHistoryLengthA(NULL), ERROR_INVALID_PARAMETER);
    FAILRET0(GetConsoleCommandHistoryW(b, sizeof b, NULL), ERROR_INVALID_PARAMETER);
    FAILRET0(GetConsoleCommandHistoryA(a, sizeof a, NULL), ERROR_INVALID_PARAMETER);
    FAILS(SetConsoleNumberOfCommandsW(3, NULL), ERROR_INVALID_PARAMETER);
    FAILS(SetConsoleNumberOfCommandsA(3, NULL), ERROR_INVALID_PARAMETER);
    SetLastError(0xBAD);
    ExpungeConsoleCommandHistoryW(NULL);
    CERR(ERROR_INVALID_PARAMETER);
    SetLastError(0xBAD);
    ExpungeConsoleCommandHistoryA(NULL);
    CERR(ERROR_INVALID_PARAMETER);
}

CT(con_hist_unknown_exe) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(GetConsoleCommandHistoryLengthW); CREQ(GetConsoleCommandHistoryW);
    SetLastError(0xBAD);
    DWORD n = GetConsoleCommandHistoryLengthW(L"never_read.exe");
    cnote("history length for unknown exe: %lu err=%lu", n, GetLastError());
    CEQ(n, 0);
    WCHAR b[8];
    SetLastError(0xBAD);
    n = GetConsoleCommandHistoryW(b, sizeof b, L"never_read.exe");
    cnote("history for unknown exe: %lu err=%lu", n, GetLastError());
    CEQ(n, 0);
}

CT(con_hist_small_buffer) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(ReadConsoleW); CREQ(GetConsoleCommandHistoryW);
    setexe(); cooked();
    WCHAR b[64]; DWORD n;
    CC(readline(b, 64, &n, L"longerline\r"));
    WCHAR h[32]; for (int i = 0; i < 32; i++) h[i] = 0x7777;
    SetLastError(0xBAD);
    DWORD r = GetConsoleCommandHistoryW(h, 8, EXE);
    cnote("short history buffer: ret=%lu err=%lu h0=0x%x", r, GetLastError(), h[0]);
    CEQ(h[8], 0x7777);
}

CT(con_hist_per_exe_name) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(ReadConsoleW); CREQ(GetConsoleCommandHistoryLengthW); CREQ(UN(SetConsoleInputExeNameW));
    setexe(); cooked();
    WCHAR b[64]; DWORD n;
    CC(readline(b, 64, &n, L"under-a\r"));
    SetConsoleInputExeNameW_pfn s = UN(SetConsoleInputExeNameW);
    s(EXE2);
    CC(readline(b, 64, &n, L"under-b\r"));
    /* P1 EXPECT-FIX: both lines go to the single history of this process (keyed by its real image name), whatever
       SetConsoleInputExeName says: "under-a"+"under-b" = 16 bytes there, nothing under the invented names. */
    /* P1c EXPECT-FIX: the length counts every command with its terminating NUL, in bytes: (7+1)*2 per line, two
       lines = 32 (same convention as con_hist_expunge: "keepme" = (6+1)*2 = 14, passed in the same baseline run).
       UNVERIFIED-until-rerun */
    CEQ(GetConsoleCommandHistoryLengthW(REALW), 32);
    CEQ(GetConsoleCommandHistoryLengthW(EXE2), 0);
    CEQ(GetConsoleCommandHistoryLengthW(EXE), 0);
}

CT(con_hist_no_dup_flag) {
    CWINESKIP("alias, history and input exe-name APIs are not implemented in the emulator");
    CREQ(ReadConsoleW); CREQ(GetConsoleCommandHistoryLengthW); CREQ(SetConsoleHistoryInfo);
    CONSOLE_HISTORY_INFO hi; hi.cbSize = sizeof hi; hi.HistoryBufferSize = 50; hi.NumberOfHistoryBuffers = 4; hi.dwFlags = HISTORY_NO_DUP_FLAG;
    CC(SetConsoleHistoryInfo(&hi));
    setexe(); cooked();
    WCHAR b[64]; DWORD n;
    CC(readline(b, 64, &n, L"dup\r"));
    CC(readline(b, 64, &n, L"dup\r"));
    CC(readline(b, 64, &n, L"dup\r"));
    CEQ(GetConsoleCommandHistoryLengthW(REALW), 8);
    hi.dwFlags = 0; SetConsoleHistoryInfo(&hi);   /* P1: the no-dup flag is console-wide, restore it */
}

/* ---------------- control handlers ---------------- */

static volatile LONG g_hits, g_ctype[8], g_order[8], g_tids[8];
static HANDLE g_ev;
static DWORD g_main_tid;

static BOOL WINAPI h_true(DWORD t) {
    LONG i = InterlockedIncrement(&g_hits) - 1;
    if (i < 8) { g_ctype[i] = (LONG)t; g_order[i] = 1; g_tids[i] = (LONG)GetCurrentThreadId(); }
    SetEvent(g_ev);
    return TRUE;
}
static BOOL WINAPI h_false_first(DWORD t) {
    LONG i = InterlockedIncrement(&g_hits) - 1;
    if (i < 8) { g_ctype[i] = (LONG)t; g_order[i] = 2; }
    return FALSE;
}
static BOOL WINAPI h_second_true(DWORD t) {
    LONG i = InterlockedIncrement(&g_hits) - 1;
    if (i < 8) { g_ctype[i] = (LONG)t; g_order[i] = 3; }
    SetEvent(g_ev);
    return TRUE;
}
static void hits_reset(void) { g_hits = 0; for (int i = 0; i < 8; i++) g_ctype[i] = g_order[i] = g_tids[i] = -1; ResetEvent(g_ev); }

CT(con_ctrl_handler_add_remove) {
    CREQ(SetConsoleCtrlHandler);
    SetLastError(0xBAD);
    CC(SetConsoleCtrlHandler(h_true, TRUE));
    CC(SetConsoleCtrlHandler(h_true, FALSE));
    SetLastError(0xBAD);
    CEQ(SetConsoleCtrlHandler(h_true, FALSE), 0); CERR(ERROR_INVALID_PARAMETER);
    SetLastError(0xBAD);
    CEQ(SetConsoleCtrlHandler(h_false_first, FALSE), 0); CERR(ERROR_INVALID_PARAMETER);
}

CT(con_ctrl_handler_add_twice) {
    CREQ(SetConsoleCtrlHandler);
    CC(SetConsoleCtrlHandler(h_true, TRUE));
    SetLastError(0xBAD);
    BOOL r = SetConsoleCtrlHandler(h_true, TRUE);
    cnote("adding the same handler twice -> %d err=%lu", r, GetLastError());
    CC(SetConsoleCtrlHandler(h_true, FALSE));
    SetLastError(0xBAD);
    r = SetConsoleCtrlHandler(h_true, FALSE);
    cnote("removing it a second time -> %d err=%lu", r, GetLastError());
}

CT(con_ctrl_null_handler_toggle) {
    CREQ(SetConsoleCtrlHandler);
    SetLastError(0xBAD);
    CC(SetConsoleCtrlHandler(NULL, TRUE));
    CC(SetConsoleCtrlHandler(NULL, TRUE));
    CC(SetConsoleCtrlHandler(NULL, FALSE));
    CC(SetConsoleCtrlHandler(NULL, FALSE));
}

CT(con_ctrl_many_handlers) {
    CREQ(SetConsoleCtrlHandler);
    CC(SetConsoleCtrlHandler(h_true, TRUE));
    CC(SetConsoleCtrlHandler(h_false_first, TRUE));
    CC(SetConsoleCtrlHandler(h_second_true, TRUE));
    CC(SetConsoleCtrlHandler(h_second_true, FALSE));
    CC(SetConsoleCtrlHandler(h_false_first, FALSE));
    CC(SetConsoleCtrlHandler(h_true, FALSE));
    SetLastError(0xBAD);
    CEQ(SetConsoleCtrlHandler(h_true, FALSE), 0); CERR(ERROR_INVALID_PARAMETER);
}

CT(con_ctrl_generate_invalid_events) {
    CREQ(GenerateConsoleCtrlEvent);
    DWORD ev[] = { 2, 3, 4, 5, 6, 7, 100, 0x7FFFFFFF, 0xFFFFFFFF };
    for (size_t k = 0; k < sizeof ev / sizeof ev[0]; k++) {
        FAILS(GenerateConsoleCtrlEvent(ev[k], 0), ERROR_INVALID_PARAMETER);
        FAILS(GenerateConsoleCtrlEvent(ev[k], GetCurrentProcessId()), ERROR_INVALID_PARAMETER);
    }
}

CT(con_ctrl_c_reaches_handler) {
    CREQ(GenerateConsoleCtrlEvent); CREQ(SetConsoleCtrlHandler);
    g_ev = CreateEventA(NULL, TRUE, FALSE, NULL); g_main_tid = GetCurrentThreadId(); hits_reset();
    CC(SetConsoleCtrlHandler(h_true, TRUE));
    SetLastError(0xBAD);
    CCF(GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0), "err=%lu", GetLastError());
    CEQ(WaitForSingleObject(g_ev, 5000), WAIT_OBJECT_0);
    CEQ(g_hits, 1);
    CEQ(g_ctype[0], CTRL_C_EVENT);
    CC(g_tids[0] != (LONG)g_main_tid);       /* handlers run on a thread created by the system */
    CC(SetConsoleCtrlHandler(h_true, FALSE));
    CloseHandle(g_ev);
}

CT(con_ctrl_break_reaches_handler) {
    CWINESKIP("CTRL+BREAK delivery terminates the emulated console client");
    CREQ(GenerateConsoleCtrlEvent); CREQ(SetConsoleCtrlHandler);
    g_ev = CreateEventA(NULL, TRUE, FALSE, NULL); hits_reset();
    CC(SetConsoleCtrlHandler(h_true, TRUE));
    CC(GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, 0));
    CEQ(WaitForSingleObject(g_ev, 5000), WAIT_OBJECT_0);
    CEQ(g_ctype[0], CTRL_BREAK_EVENT);
    CC(SetConsoleCtrlHandler(h_true, FALSE));
    CloseHandle(g_ev);
}

CT(con_ctrl_handlers_lifo_and_stop_on_true) {
    CREQ(GenerateConsoleCtrlEvent); CREQ(SetConsoleCtrlHandler);
    g_ev = CreateEventA(NULL, TRUE, FALSE, NULL); hits_reset();
    CC(SetConsoleCtrlHandler(h_true, TRUE));            /* oldest */
    CC(SetConsoleCtrlHandler(h_false_first, TRUE));     /* newest: called first, returns FALSE */
    CC(GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0));
    CEQ(WaitForSingleObject(g_ev, 5000), WAIT_OBJECT_0);
    Sleep(200);
    CEQ(g_hits, 2);
    CEQ(g_order[0], 2); CEQ(g_order[1], 1);
    hits_reset();
    CC(SetConsoleCtrlHandler(h_second_true, TRUE));     /* newest now returns TRUE: older ones not called */
    CC(GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0));
    CEQ(WaitForSingleObject(g_ev, 5000), WAIT_OBJECT_0);
    Sleep(300);
    CEQ(g_hits, 1);
    CEQ(g_order[0], 3);
    CC(SetConsoleCtrlHandler(h_second_true, FALSE));
    CC(SetConsoleCtrlHandler(h_false_first, FALSE));
    CC(SetConsoleCtrlHandler(h_true, FALSE));
    CloseHandle(g_ev);
}

CT(con_ctrl_removed_handler_not_called) {
    CREQ(GenerateConsoleCtrlEvent); CREQ(SetConsoleCtrlHandler);
    g_ev = CreateEventA(NULL, TRUE, FALSE, NULL); hits_reset();
    CC(SetConsoleCtrlHandler(h_true, TRUE));
    CC(SetConsoleCtrlHandler(h_second_true, TRUE));
    CC(SetConsoleCtrlHandler(h_second_true, FALSE));
    CC(GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0));
    CEQ(WaitForSingleObject(g_ev, 5000), WAIT_OBJECT_0);
    Sleep(200);
    CEQ(g_hits, 1);
    CEQ(g_order[0], 1);
    CC(SetConsoleCtrlHandler(h_true, FALSE));
    CloseHandle(g_ev);
}

CT(con_ctrl_ignore_ctrl_c_flag) {
    CWINESKIP("CTRL+BREAK delivery terminates the emulated console client");
    CREQ(GenerateConsoleCtrlEvent); CREQ(SetConsoleCtrlHandler);
    g_ev = CreateEventA(NULL, TRUE, FALSE, NULL); hits_reset();
    CC(SetConsoleCtrlHandler(h_true, TRUE));
    CC(SetConsoleCtrlHandler(NULL, TRUE));               /* ignore CTRL+C */
    CC(GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0));
    CEQ(WaitForSingleObject(g_ev, 800), WAIT_TIMEOUT);
    CEQ(g_hits, 0);
    CC(GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, 0));   /* CTRL+BREAK is not affected */
    CEQ(WaitForSingleObject(g_ev, 5000), WAIT_OBJECT_0);
    CEQ(g_ctype[0], CTRL_BREAK_EVENT);
    hits_reset();
    CC(SetConsoleCtrlHandler(NULL, FALSE));              /* normal processing restored */
    CC(GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0));
    CEQ(WaitForSingleObject(g_ev, 5000), WAIT_OBJECT_0);
    CEQ(g_ctype[0], CTRL_C_EVENT);
    CC(SetConsoleCtrlHandler(h_true, FALSE));
    CloseHandle(g_ev);
}

CT(con_ctrl_key_injection_observation) {
    CREQ(SetConsoleCtrlHandler); CREQ(WriteConsoleInputW);
    g_ev = CreateEventA(NULL, TRUE, FALSE, NULL); hits_reset();
    CC(SetConsoleCtrlHandler(h_true, TRUE));
    SetConsoleMode(hin_(), ENABLE_PROCESSED_INPUT);
    inp_reset();
    INPUT_RECORD r = keyrec(0x03, 'C', TRUE, LEFT_CTRL_PRESSED); DWORD n;
    CC(WriteConsoleInputW(hin_(), &r, 1, &n));
    DWORD w = WaitForSingleObject(g_ev, 1500);
    cnote("injected Ctrl+C with ENABLE_PROCESSED_INPUT: handler %s, events pending %lu", w == WAIT_OBJECT_0 ? "called" : "not called", inp_count());
    SetConsoleCtrlHandler(h_true, FALSE);
    CloseHandle(g_ev);
}

CT(con_ctrl_generate_bogus_group) {
    CREQ(GenerateConsoleCtrlEvent); CREQ(SetConsoleCtrlHandler);
    g_ev = CreateEventA(NULL, TRUE, FALSE, NULL); hits_reset();
    CC(SetConsoleCtrlHandler(h_true, TRUE));
    SetLastError(0xBAD);
    BOOL r = GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, 0x7FFFFF00);
    DWORD e = GetLastError();
    DWORD w = WaitForSingleObject(g_ev, 800);
    cnote("CTRL_BREAK to bogus group id -> %d err=%lu; own handler %s", r, e, w == WAIT_OBJECT_0 ? "called" : "not called");
    SetConsoleCtrlHandler(h_true, FALSE);
    CloseHandle(g_ev);
}

/* grandchild: new process group, exits with 77 on CTRL+BREAK (78 if it also saw CTRL+C) */
static volatile LONG g_gc_sawc;
static BOOL WINAPI h_group(DWORD t) {
    if (t == CTRL_C_EVENT) { g_gc_sawc = 1; return TRUE; }
    if (t == CTRL_BREAK_EVENT) ExitProcess(77 + g_gc_sawc);
    return FALSE;
}
CG(con_breakwait) {
    GSETUP();
    HANDLE ready = OpenEventA(EVENT_MODIFY_STATE, FALSE, argv[2]);
    SetConsoleCtrlHandler(h_group, TRUE);
    if (ready) SetEvent(ready);
    Sleep(15000);
    return 1;
}

CT(con_ctrl_break_to_new_process_group) {
    CWINESKIP("CTRL+BREAK delivery terminates the emulated console client");
    CREQ(GenerateConsoleCtrlEvent); CREQ(SetConsoleCtrlHandler);
    char name[64]; snprintf(name, sizeof name, "Local\\k32t_brk_%lu", GetCurrentProcessId());
    HANDLE ready = CreateEventA(NULL, TRUE, FALSE, name);
    PROCESS_INFORMATION pi;
    HANDLE g = gspawn("con_breakwait", name, CREATE_NEW_PROCESS_GROUP, NULL, NULL, NULL, FALSE, &pi);
    CNE(g, NULL); if (!g) return;
    CEQ(WaitForSingleObject(ready, 10000), WAIT_OBJECT_0);
    Sleep(200);
    /* CTRL+C aimed at the group: new groups have CTRL+C disabled */
    SetLastError(0xBAD);
    BOOL rc = GenerateConsoleCtrlEvent(CTRL_C_EVENT, pi.dwProcessId);
    cnote("CTRL_C to child group -> %d err=%lu", rc, GetLastError());
    Sleep(500);
    DWORD alive = WaitForSingleObject(g, 0);
    cnote("child after CTRL_C: %s", alive == WAIT_TIMEOUT ? "alive" : "exited");
    SetLastError(0xBAD);
    CCF(GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pi.dwProcessId), "err=%lu", GetLastError());
    DWORD code = gwait(g, 10000);
    cnote("child exit code after CTRL_BREAK: %lu", code);
    CC(code == 77 || code == 78);
    CloseHandle(pi.hThread); CloseHandle(g); CloseHandle(ready);
}

/* probe: registers a CTRL+C handler, reports whether it fired within 2.5 s */
static volatile LONG g_probe_hit;
static BOOL WINAPI h_probe(DWORD t) { if (t == CTRL_C_EVENT) g_probe_hit = 1; return TRUE; }
CG(con_ctrlc_probe) {
    GSETUP();
    HANDLE ready = OpenEventA(EVENT_MODIFY_STATE, FALSE, argv[2]);
    SetConsoleCtrlHandler(h_probe, TRUE);
    if (ready) SetEvent(ready);
    for (int i = 0; i < 25 && !g_probe_hit; i++) Sleep(100);
    return g_probe_hit ? 0 : 1;
}
static BOOL WINAPI h_parent_swallow(DWORD t) { return TRUE; }

static DWORD run_probe(void) {
    char name[64]; snprintf(name, sizeof name, "Local\\k32t_pr_%lu_%lu", GetCurrentProcessId(), GetTickCount());
    HANDLE ready = CreateEventA(NULL, TRUE, FALSE, name);
    PROCESS_INFORMATION pi;
    HANDLE g = gspawn("con_ctrlc_probe", name, 0, NULL, NULL, NULL, FALSE, &pi);
    if (!g) return 0xDEAD0000;
    if (WaitForSingleObject(ready, 10000) != WAIT_OBJECT_0) { TerminateProcess(g, 5); }
    Sleep(200);
    GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
    DWORD code = gwait(g, 10000);
    CloseHandle(pi.hThread); CloseHandle(g); CloseHandle(ready);
    return code;
}

CT(con_ctrl_c_group0_reaches_other_process) {
    CREQ(GenerateConsoleCtrlEvent); CREQ(SetConsoleCtrlHandler);
    CC(SetConsoleCtrlHandler(h_parent_swallow, TRUE));
    CEQ(run_probe(), 0);
    SetConsoleCtrlHandler(h_parent_swallow, FALSE);
}

CT(con_ctrl_ignore_flag_inherited_by_child) {
    CREQ(GenerateConsoleCtrlEvent); CREQ(SetConsoleCtrlHandler);
    CC(SetConsoleCtrlHandler(NULL, TRUE));
    CC(SetConsoleCtrlHandler(h_parent_swallow, TRUE));
    CEQ(run_probe(), 1);                   /* the child inherited "ignore CTRL+C" and never saw the event */
    SetConsoleCtrlHandler(NULL, FALSE);
    SetConsoleCtrlHandler(h_parent_swallow, FALSE);
}

CT(con_ctrl_clear_ignore_before_spawn) {
    CREQ(GenerateConsoleCtrlEvent); CREQ(SetConsoleCtrlHandler);
    CC(SetConsoleCtrlHandler(NULL, TRUE));
    CC(SetConsoleCtrlHandler(NULL, FALSE));
    CC(SetConsoleCtrlHandler(h_parent_swallow, TRUE));
    CEQ(run_probe(), 0);
    SetConsoleCtrlHandler(h_parent_swallow, FALSE);
}
