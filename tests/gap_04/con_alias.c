/* gap_04: console alias / command history entry points (A and W), boundary and parity checks */
#include "ch.h"

UDECL(BOOL, SetConsoleInputExeNameW, (LPCWSTR));
UDECL(BOOL, SetConsoleInputExeNameA, (LPCSTR));

#define G4EXE  L"gap4_alias.exe"
#define G4EXEA "gap4_alias.exe"
#define G4EXE2 L"gap4_other.exe"

#define NOWINE() CWINESKIP("alias and history APIs are not implemented in the emulator")
#define F0(call, err) do { SetLastError(0xBAD); DWORD r_ = (DWORD)(call); CEQ(r_, 0); CERR(err); } while (0)
#define FB(call, err) do { SetLastError(0xBAD); BOOL r_ = (call); CEQ(r_, 0); CERR(err); } while (0)

static void g4_setexe(void) {
    SetConsoleInputExeNameW_pfn f = UN(SetConsoleInputExeNameW);
    if (f) f(G4EXE);
}
static void g4_cooked(void) {
    SetConsoleMode(hin_(), ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
    inp_reset();
}
static BOOL g4_line(const WCHAR *typed) {
    WCHAR b[128]; DWORD n = 0;
    typestr(hin_(), typed);
    return ReadConsoleW(hin_(), b, 128, &n, NULL);
}

/* ---------------- GetConsoleAliasA / W ---------------- */

CT(gap4_alias_w_exact_and_short_buffers) {
    NOWINE();
    CREQ(AddConsoleAliasW); CREQ(GetConsoleAliasW);
    CC(AddConsoleAliasW(L"xb", L"12345", G4EXE));
    const DWORD need = (5 + 1) * 2;
    WCHAR b[32];
    for (int i = 0; i < 32; i++) b[i] = 0x7777;
    SetLastError(0xBAD);
    CEQ(GetConsoleAliasW(L"xb", b, need, G4EXE), need);
    CWSTR(b, L"12345");
    CEQ(b[6], 0x7777);
    for (DWORD sz = 0; sz < need; sz++) {
        for (int i = 0; i < 32; i++) b[i] = 0x7777;
        SetLastError(0xBAD);
        DWORD r = GetConsoleAliasW(L"xb", b, sz, G4EXE);
        CCF(GetLastError() == ERROR_INSUFFICIENT_BUFFER, "size %lu err=%lu ret=%lu", sz, GetLastError(), r);
        CCF((sz & 1) || b[sz / 2] == 0x7777, "size %lu overran", sz);
    }
}

CT(gap4_alias_a_exact_and_short_buffers) {
    NOWINE();
    CREQ(AddConsoleAliasA); CREQ(GetConsoleAliasA);
    CC(AddConsoleAliasA("xa", "abcd", G4EXEA));
    char b[32];
    memset(b, 0x55, sizeof b);
    SetLastError(0xBAD);
    CEQ(GetConsoleAliasA("xa", b, 5, G4EXEA), 5);
    CSTR(b, "abcd");
    CEQ((unsigned char)b[5], 0x55);
    for (DWORD sz = 0; sz < 5; sz++) {
        memset(b, 0x55, sizeof b);
        SetLastError(0xBAD);
        GetConsoleAliasA("xa", b, sz, G4EXEA);
        CCF(GetLastError() == ERROR_INSUFFICIENT_BUFFER, "size %lu err=%lu", sz, GetLastError());
        CCF((unsigned char)b[5] == 0x55 && (unsigned char)b[sz] == 0x55, "size %lu overran", sz);
    }
}

CT(gap4_alias_null_buffer_zero_size) {
    NOWINE();
    CREQ(AddConsoleAliasW); CREQ(GetConsoleAliasW); CREQ(GetConsoleAliasA);
    CC(AddConsoleAliasW(L"nb", L"zz", G4EXE));
    SetLastError(0xBAD);
    DWORD r = GetConsoleAliasW(L"nb", NULL, 0, G4EXE);
    cnote("GetConsoleAliasW(NULL buffer, 0) -> %lu err=%lu", r, GetLastError());
    CERR(ERROR_INSUFFICIENT_BUFFER);
    SetLastError(0xBAD);
    r = GetConsoleAliasA("nb", NULL, 0, "gap4_alias.exe");
    cnote("GetConsoleAliasA(NULL buffer, 0) -> %lu err=%lu", r, GetLastError());
}

CT(gap4_alias_invalid_arguments) {
    NOWINE();
    CREQ(GetConsoleAliasW); CREQ(GetConsoleAliasA); CREQ(AddConsoleAliasW); CREQ(AddConsoleAliasA);
    WCHAR w[8]; char a[8];
    F0(GetConsoleAliasW(NULL, w, sizeof w, G4EXE), ERROR_INVALID_PARAMETER);
    F0(GetConsoleAliasA(NULL, a, sizeof a, G4EXEA), ERROR_INVALID_PARAMETER);
    F0(GetConsoleAliasW(L"x", w, sizeof w, NULL), ERROR_INVALID_PARAMETER);
    F0(GetConsoleAliasA("x", a, sizeof a, NULL), ERROR_INVALID_PARAMETER);
    F0(GetConsoleAliasW(L"x", w, sizeof w, L""), ERROR_INVALID_PARAMETER);
    F0(GetConsoleAliasA("x", a, sizeof a, ""), ERROR_INVALID_PARAMETER);
    FB(AddConsoleAliasW(L"x", L"y", NULL), ERROR_INVALID_PARAMETER);
    FB(AddConsoleAliasA("x", "y", NULL), ERROR_INVALID_PARAMETER);
    FB(AddConsoleAliasW(NULL, L"y", G4EXE), ERROR_INVALID_PARAMETER);
    FB(AddConsoleAliasA(NULL, "y", G4EXEA), ERROR_INVALID_PARAMETER);
}

CT(gap4_alias_missing_source_reports_error) {
    NOWINE();
    CREQ(GetConsoleAliasW); CREQ(AddConsoleAliasW);
    WCHAR b[16];
    CC(AddConsoleAliasW(L"present", L"yes", G4EXE));
    SetLastError(0xBAD);
    DWORD r = GetConsoleAliasW(L"absent", b, sizeof b, G4EXE);
    cnote("missing alias -> %lu err=%lu", r, GetLastError());
    CEQ(r, 0);
    CNE(GetLastError(), 0xBAD);
}

CT(gap4_alias_embedded_punctuation) {
    NOWINE();
    CREQ(AddConsoleAliasW); CREQ(GetConsoleAliasW);
    WCHAR b[64];
    SetLastError(0xBAD);
    BOOL r = AddConsoleAliasW(L"a b", L"spaced", G4EXE);
    cnote("alias name with space -> %d err=%lu", r, GetLastError());
    r = AddConsoleAliasW(L"a=b", L"equals", G4EXE);
    cnote("alias name with '=' -> %d err=%lu", r, GetLastError());
    r = AddConsoleAliasW(L"t", L"tab\there", G4EXE);
    CC(r);
    CEQ(GetConsoleAliasW(L"t", b, sizeof b, G4EXE), (8 + 1) * 2);
    CWSTR(b, L"tab\there");
    r = AddConsoleAliasW(L"q", L"a=b=c", G4EXE);
    CC(r);
    CEQ(GetConsoleAliasW(L"q", b, sizeof b, G4EXE), (5 + 1) * 2);
    CWSTR(b, L"a=b=c");
}

CT(gap4_alias_target_lengths) {
    NOWINE();
    CREQ(AddConsoleAliasW); CREQ(GetConsoleAliasW); CREQ(GetConsoleAliasA);
    static WCHAR t[600], g[600];
    static const int lens[] = { 1, 2, 3, 15, 16, 31, 32, 63, 64, 127, 128, 255, 256, 257, 500 };
    for (int k = 0; k < 15; k++) {
        int n = lens[k];
        for (int i = 0; i < n; i++) t[i] = L'a' + (i % 26);
        t[n] = 0;
        BOOL ok = AddConsoleAliasW(L"lenx", t, G4EXE);
        CCF(ok, "len %d err=%lu", n, GetLastError());
        if (!ok) continue;
        for (int i = 0; i < 600; i++) g[i] = 0x7777;
        DWORD r = GetConsoleAliasW(L"lenx", g, sizeof g, G4EXE);
        CCF(r == (DWORD)(n + 1) * 2 && !lstrcmpW(g, t), "len %d ret=%lu", n, r);
    }
}

CT(gap4_alias_replace_changes_length) {
    NOWINE();
    CREQ(AddConsoleAliasW); CREQ(GetConsoleAliasW); CREQ(GetConsoleAliasesLengthW);
    CC(AddConsoleAliasW(L"rp", L"short", G4EXE));
    CEQ(GetConsoleAliasesLengthW(G4EXE), (2 + 1 + 5 + 1) * 2);
    CC(AddConsoleAliasW(L"rp", L"a much longer replacement", G4EXE));
    CEQ(GetConsoleAliasesLengthW(G4EXE), (2 + 1 + 25 + 1) * 2);
    CC(AddConsoleAliasW(L"rp", L"x", G4EXE));
    CEQ(GetConsoleAliasesLengthW(G4EXE), (2 + 1 + 1 + 1) * 2);
}

CT(gap4_alias_case_of_exe_name) {
    NOWINE();
    CREQ(AddConsoleAliasW); CREQ(GetConsoleAliasW); CREQ(GetConsoleAliasesLengthW);
    WCHAR b[32];
    CC(AddConsoleAliasW(L"ci", L"val", L"MiXeD.ExE"));
    DWORD a = GetConsoleAliasW(L"ci", b, sizeof b, L"mixed.exe");
    DWORD c = GetConsoleAliasesLengthW(L"MIXED.EXE");
    cnote("lookup via different exe case: alias=%lu list-length=%lu", a, c);
    CEQ(a, 8);
    CEQ(c, (2 + 1 + 3 + 1) * 2);
}

CT(gap4_alias_a_w_cross_unicode_ansi_conversion) {
    NOWINE();
    CREQ(AddConsoleAliasW); CREQ(GetConsoleAliasA); CREQ(GetConsoleAliasW);
    char a[64]; WCHAR w[64];
    CC(AddConsoleAliasW(L"cv", L"ascii only", G4EXE));
    CEQ(GetConsoleAliasA("cv", a, sizeof a, G4EXEA), 11);
    CSTR(a, "ascii only");
    CC(AddConsoleAliasA("cw", "from A", G4EXEA));
    CEQ(GetConsoleAliasW(L"cw", w, sizeof w, G4EXE), 14);
    CWSTR(w, L"from A");
}

CT(gap4_alias_a_buffer_size_in_bytes_w_in_bytes) {
    NOWINE();
    CREQ(AddConsoleAliasW); CREQ(GetConsoleAliasW);
    WCHAR b[16];
    CC(AddConsoleAliasW(L"bs", L"abc", G4EXE));
    for (int i = 0; i < 16; i++) b[i] = 0x7777;
    /* TargetBufferLength is in bytes: 8 bytes holds "abc\0" */
    CEQ(GetConsoleAliasW(L"bs", b, 8, G4EXE), 8);
    CWSTR(b, L"abc");
    for (int i = 0; i < 16; i++) b[i] = 0x7777;
    SetLastError(0xBAD);
    GetConsoleAliasW(L"bs", b, 7, G4EXE);
    CERR(ERROR_INSUFFICIENT_BUFFER);
    CEQ(b[4], 0x7777);
}

/* ---------------- GetConsoleAliasesLength / GetConsoleAliases ---------------- */

CT(gap4_aliases_len_w_is_twice_a) {
    NOWINE();
    CREQ(AddConsoleAliasW); CREQ(GetConsoleAliasesLengthA); CREQ(GetConsoleAliasesLengthW);
    CC(AddConsoleAliasW(L"one", L"1111", G4EXE));
    CC(AddConsoleAliasW(L"two", L"22", G4EXE));
    CC(AddConsoleAliasW(L"three", L"3", G4EXE));
    DWORD a = GetConsoleAliasesLengthA(G4EXEA), w = GetConsoleAliasesLengthW(G4EXE);
    CEQ(a, (3 + 1 + 4 + 1) + (3 + 1 + 2 + 1) + (5 + 1 + 1 + 1));
    CEQ(w, a * 2);
}

CT(gap4_aliases_list_w_exact_short_and_long) {
    NOWINE();
    CREQ(AddConsoleAliasW); CREQ(GetConsoleAliasesW); CREQ(GetConsoleAliasesLengthW);
    CC(AddConsoleAliasW(L"k1", L"v1", G4EXE));
    CC(AddConsoleAliasW(L"k22", L"v22", G4EXE));
    DWORD need = GetConsoleAliasesLengthW(G4EXE);
    CEQ(need, ((2 + 1 + 2 + 1) + (3 + 1 + 3 + 1)) * 2);
    WCHAR b[64];
    for (int i = 0; i < 64; i++) b[i] = 0x7777;
    DWORD n = GetConsoleAliasesW(b, need, G4EXE);
    CEQ(n, need);
    CEQ(b[need / 2], 0x7777);
    for (int i = 0; i < 64; i++) b[i] = 0x7777;
    n = GetConsoleAliasesW(b, sizeof b, G4EXE);
    CEQ(n, need);
    CEQ(b[need / 2], 0x7777);
    for (DWORD sz = 0; sz < need; sz += 2) {
        for (int i = 0; i < 64; i++) b[i] = 0x7777;
        SetLastError(0xBAD);
        n = GetConsoleAliasesW(b, sz, G4EXE);
        CCF(n == 0 || n <= sz, "size %lu ret %lu", sz, n);
        CCF(b[sz / 2] == 0x7777, "size %lu overran", sz);
    }
}

CT(gap4_aliases_list_a_contents) {
    NOWINE();
    CREQ(AddConsoleAliasA); CREQ(GetConsoleAliasesA); CREQ(GetConsoleAliasesLengthA);
    CC(AddConsoleAliasA("p", "q r", G4EXEA));
    CC(AddConsoleAliasA("longname", "x", G4EXEA));
    DWORD need = GetConsoleAliasesLengthA(G4EXEA);
    CEQ(need, (1 + 1 + 3 + 1) + (8 + 1 + 1 + 1));
    char b[64]; memset(b, 0x55, sizeof b);
    DWORD n = GetConsoleAliasesA(b, sizeof b, G4EXEA);
    CEQ(n, need);
    int f1 = 0, f2 = 0;
    for (const char *p = b; (DWORD)(p - b) < n && *p; p += strlen(p) + 1) {
        if (!strcmp(p, "p=q r")) f1++;
        if (!strcmp(p, "longname=x")) f2++;
    }
    CEQ(f1, 1); CEQ(f2, 1);
    CEQ((unsigned char)b[need], 0x55);
}

CT(gap4_aliases_list_excludes_other_exe) {
    NOWINE();
    CREQ(AddConsoleAliasW); CREQ(GetConsoleAliasesLengthW);
    CC(AddConsoleAliasW(L"a", L"1", G4EXE));
    CC(AddConsoleAliasW(L"bbbb", L"22222", G4EXE2));
    CEQ(GetConsoleAliasesLengthW(G4EXE), (1 + 1 + 1 + 1) * 2);
    CEQ(GetConsoleAliasesLengthW(G4EXE2), (4 + 1 + 5 + 1) * 2);
}

CT(gap4_aliases_invalid_exe_arguments) {
    NOWINE();
    CREQ(GetConsoleAliasesLengthA); CREQ(GetConsoleAliasesLengthW); CREQ(GetConsoleAliasesA); CREQ(GetConsoleAliasesW);
    WCHAR w[8]; char a[8];
    F0(GetConsoleAliasesLengthA(NULL), ERROR_INVALID_PARAMETER);
    F0(GetConsoleAliasesLengthW(NULL), ERROR_INVALID_PARAMETER);
    F0(GetConsoleAliasesLengthA(""), ERROR_INVALID_PARAMETER);
    F0(GetConsoleAliasesLengthW(L""), ERROR_INVALID_PARAMETER);
    F0(GetConsoleAliasesA(a, sizeof a, NULL), ERROR_INVALID_PARAMETER);
    F0(GetConsoleAliasesW(w, sizeof w, NULL), ERROR_INVALID_PARAMETER);
    F0(GetConsoleAliasesA(a, sizeof a, ""), ERROR_INVALID_PARAMETER);
    F0(GetConsoleAliasesW(w, sizeof w, L""), ERROR_INVALID_PARAMETER);
}

CT(gap4_aliases_unknown_exe_is_empty) {
    NOWINE();
    CREQ(GetConsoleAliasesLengthA); CREQ(GetConsoleAliasesLengthW); CREQ(GetConsoleAliasesA);
    CEQ(GetConsoleAliasesLengthA("gap4_never_added.exe"), 0);
    CEQ(GetConsoleAliasesLengthW(L"gap4_never_added.exe"), 0);
    char a[16]; memset(a, 0x55, sizeof a);
    CEQ(GetConsoleAliasesA(a, sizeof a, "gap4_never_added.exe"), 0);
    CEQ((unsigned char)a[0], 0x55);
}

CT(gap4_aliases_removed_after_delete) {
    NOWINE();
    CREQ(AddConsoleAliasW); CREQ(GetConsoleAliasesLengthW);
    CC(AddConsoleAliasW(L"d1", L"a", G4EXE));
    CC(AddConsoleAliasW(L"d2", L"bb", G4EXE));
    CEQ(GetConsoleAliasesLengthW(G4EXE), ((2 + 1 + 1 + 1) + (2 + 1 + 2 + 1)) * 2);
    CC(AddConsoleAliasW(L"d1", NULL, G4EXE));
    CEQ(GetConsoleAliasesLengthW(G4EXE), (2 + 1 + 2 + 1) * 2);
    CC(AddConsoleAliasW(L"d2", NULL, G4EXE));
    CEQ(GetConsoleAliasesLengthW(G4EXE), 0);
}

CT(gap4_aliases_many_entries_roundtrip) {
    NOWINE();
    CREQ(AddConsoleAliasW); CREQ(GetConsoleAliasesW); CREQ(GetConsoleAliasesLengthW);
    DWORD expect = 0;
    for (int i = 0; i < 60; i++) {
        WCHAR s[16], t[16];
        wsprintfW(s, L"m%02d", i); wsprintfW(t, L"t%d", i * 7);
        CC(AddConsoleAliasW(s, t, G4EXE));
        expect += (lstrlenW(s) + 1 + lstrlenW(t) + 1) * 2;
    }
    DWORD need = GetConsoleAliasesLengthW(G4EXE);
    CEQ(need, expect);
    WCHAR *b = (WCHAR *)calloc(1, need + 16);
    DWORD n = GetConsoleAliasesW(b, need, G4EXE);
    CEQ(n, need);
    int cnt = 0;
    for (const WCHAR *p = b; (DWORD)(p - b) * 2 < n && *p; p += lstrlenW(p) + 1) cnt++;
    CEQ(cnt, 60);
    free(b);
}

/* ---------------- GetConsoleAliasExes ---------------- */

CT(gap4_exes_empty_console) {
    NOWINE();
    CREQ(GetConsoleAliasExesLengthA); CREQ(GetConsoleAliasExesLengthW); CREQ(GetConsoleAliasExesA); CREQ(GetConsoleAliasExesW);
    CEQ(GetConsoleAliasExesLengthA(), 0);
    CEQ(GetConsoleAliasExesLengthW(), 0);
    char a[8]; WCHAR w[8]; memset(a, 0x55, sizeof a); for (int i = 0; i < 8; i++) w[i] = 0x7777;
    CEQ(GetConsoleAliasExesA(a, sizeof a), 0);
    CEQ(GetConsoleAliasExesW(w, sizeof w), 0);
    CEQ((unsigned char)a[0], 0x55);
    CEQ(w[0], 0x7777);
}

CT(gap4_exes_w_is_twice_a) {
    NOWINE();
    CREQ(AddConsoleAliasA); CREQ(GetConsoleAliasExesLengthA); CREQ(GetConsoleAliasExesLengthW);
    CC(AddConsoleAliasA("q", "1", "first.exe"));
    CC(AddConsoleAliasA("q", "2", "second.exe"));
    CC(AddConsoleAliasA("q", "3", "third.exe"));
    DWORD a = GetConsoleAliasExesLengthA(), w = GetConsoleAliasExesLengthW();
    CEQ(a, (9 + 1) + (10 + 1) + (9 + 1));
    CEQ(w, a * 2);
}

CT(gap4_exes_list_a_exact_and_short) {
    NOWINE();
    CREQ(AddConsoleAliasA); CREQ(GetConsoleAliasExesA); CREQ(GetConsoleAliasExesLengthA);
    CC(AddConsoleAliasA("q", "1", "aa.exe"));
    CC(AddConsoleAliasA("q", "2", "bbb.exe"));
    DWORD need = GetConsoleAliasExesLengthA();
    CEQ(need, 7 + 8);
    char b[64]; memset(b, 0x55, sizeof b);
    CEQ(GetConsoleAliasExesA(b, need), need);
    CEQ((unsigned char)b[need], 0x55);
    int fa = 0, fb = 0;
    for (const char *p = b; (DWORD)(p - b) < need && *p; p += strlen(p) + 1) {
        if (!lstrcmpiA(p, "aa.exe")) fa++;
        if (!lstrcmpiA(p, "bbb.exe")) fb++;
    }
    CEQ(fa, 1); CEQ(fb, 1);
    for (DWORD sz = 0; sz < need; sz++) {
        memset(b, 0x55, sizeof b);
        GetConsoleAliasExesA(b, sz);
        CCF((unsigned char)b[sz] == 0x55, "size %lu overran", sz);
    }
}

CT(gap4_exes_list_w_exact_and_short) {
    NOWINE();
    CREQ(AddConsoleAliasW); CREQ(GetConsoleAliasExesW); CREQ(GetConsoleAliasExesLengthW);
    CC(AddConsoleAliasW(L"q", L"1", L"aa.exe"));
    CC(AddConsoleAliasW(L"q", L"2", L"bbb.exe"));
    DWORD need = GetConsoleAliasExesLengthW();
    CEQ(need, (7 + 8) * 2);
    WCHAR b[64];
    for (int i = 0; i < 64; i++) b[i] = 0x7777;
    CEQ(GetConsoleAliasExesW(b, need), need);
    CEQ(b[need / 2], 0x7777);
    for (DWORD sz = 0; sz < need; sz += 2) {
        for (int i = 0; i < 64; i++) b[i] = 0x7777;
        GetConsoleAliasExesW(b, sz);
        CCF(b[sz / 2] == 0x7777, "size %lu overran", sz);
    }
}

CT(gap4_exes_second_alias_same_exe_not_duplicated) {
    NOWINE();
    CREQ(AddConsoleAliasW); CREQ(GetConsoleAliasExesLengthW);
    CC(AddConsoleAliasW(L"a", L"1", L"same.exe"));
    CC(AddConsoleAliasW(L"b", L"2", L"same.exe"));
    CC(AddConsoleAliasW(L"c", L"3", L"SAME.EXE"));
    DWORD n = GetConsoleAliasExesLengthW();
    cnote("exe list length with 3 aliases over same exe (mixed case): %lu", n);
    CCF(n == (8 + 1) * 2 || n == (8 + 1 + 8 + 1) * 2, "unexpected %lu", n);
}

CT(gap4_exes_not_influenced_by_history_only) {
    NOWINE();
    CREQ(GetConsoleAliasExesLengthW); CREQ(ReadConsoleW);
    g4_setexe(); g4_cooked();
    CC(g4_line(L"abc\r"));
    CEQ(GetConsoleAliasExesLengthW(), 0);
}

/* ---------------- command history ---------------- */

CT(gap4_hist_a_w_lengths) {
    NOWINE();
    CREQ(ReadConsoleW); CREQ(GetConsoleCommandHistoryLengthA); CREQ(GetConsoleCommandHistoryLengthW);
    g4_setexe(); g4_cooked();
    CC(g4_line(L"ab\r"));
    CC(g4_line(L"cde\r"));
    DWORD a = GetConsoleCommandHistoryLengthA(G4EXEA), w = GetConsoleCommandHistoryLengthW(G4EXE);
    CEQ(a, (2 + 1) + (3 + 1));
    CEQ(w, a * 2);
}

CT(gap4_hist_a_contents_and_boundaries) {
    NOWINE();
    CREQ(ReadConsoleW); CREQ(GetConsoleCommandHistoryA); CREQ(GetConsoleCommandHistoryLengthA);
    g4_setexe(); g4_cooked();
    CC(g4_line(L"first\r"));
    CC(g4_line(L"second\r"));
    DWORD need = GetConsoleCommandHistoryLengthA(G4EXEA);
    CEQ(need, 6 + 7);
    char b[64]; memset(b, 0x55, sizeof b);
    DWORD n = GetConsoleCommandHistoryA(b, need, G4EXEA);
    CEQ(n, need);
    CC(!strcmp(b, "first")); CC(!strcmp(b + 6, "second"));
    CEQ((unsigned char)b[need], 0x55);
    for (DWORD sz = 0; sz < need; sz++) {
        memset(b, 0x55, sizeof b);
        GetConsoleCommandHistoryA(b, sz, G4EXEA);
        CCF((unsigned char)b[sz] == 0x55, "size %lu overran", sz);
    }
}

CT(gap4_hist_w_contents_and_boundaries) {
    NOWINE();
    CREQ(ReadConsoleW); CREQ(GetConsoleCommandHistoryW); CREQ(GetConsoleCommandHistoryLengthW);
    g4_setexe(); g4_cooked();
    CC(g4_line(L"alpha\r"));
    CC(g4_line(L"be\r"));
    DWORD need = GetConsoleCommandHistoryLengthW(G4EXE);
    CEQ(need, (6 + 3) * 2);
    WCHAR b[64];
    for (int i = 0; i < 64; i++) b[i] = 0x7777;
    CEQ(GetConsoleCommandHistoryW(b, need, G4EXE), need);
    CWSTR(b, L"alpha"); CWSTR(b + 6, L"be");
    CEQ(b[need / 2], 0x7777);
    for (DWORD sz = 0; sz < need; sz += 2) {
        for (int i = 0; i < 64; i++) b[i] = 0x7777;
        GetConsoleCommandHistoryW(b, sz, G4EXE);
        CCF(b[sz / 2] == 0x7777, "size %lu overran", sz);
    }
}

CT(gap4_hist_order_oldest_first) {
    NOWINE();
    CREQ(ReadConsoleW); CREQ(GetConsoleCommandHistoryW);
    g4_setexe(); g4_cooked();
    CC(g4_line(L"c1\r")); CC(g4_line(L"c2\r")); CC(g4_line(L"c3\r"));
    WCHAR b[64]; for (int i = 0; i < 64; i++) b[i] = 0x7777;
    DWORD n = GetConsoleCommandHistoryW(b, sizeof b, G4EXE);
    CEQ(n, (3 * 3) * 2);
    CWSTR(b, L"c1"); CWSTR(b + 3, L"c2"); CWSTR(b + 6, L"c3");
}

CT(gap4_hist_invalid_exe_arguments) {
    NOWINE();
    CREQ(GetConsoleCommandHistoryA); CREQ(GetConsoleCommandHistoryW);
    CREQ(GetConsoleCommandHistoryLengthA); CREQ(GetConsoleCommandHistoryLengthW);
    char a[8]; WCHAR w[8];
    F0(GetConsoleCommandHistoryLengthA(NULL), ERROR_INVALID_PARAMETER);
    F0(GetConsoleCommandHistoryLengthW(NULL), ERROR_INVALID_PARAMETER);
    F0(GetConsoleCommandHistoryLengthA(""), ERROR_INVALID_PARAMETER);
    F0(GetConsoleCommandHistoryLengthW(L""), ERROR_INVALID_PARAMETER);
    F0(GetConsoleCommandHistoryA(a, sizeof a, NULL), ERROR_INVALID_PARAMETER);
    F0(GetConsoleCommandHistoryW(w, sizeof w, NULL), ERROR_INVALID_PARAMETER);
    F0(GetConsoleCommandHistoryA(a, sizeof a, ""), ERROR_INVALID_PARAMETER);
    F0(GetConsoleCommandHistoryW(w, sizeof w, L""), ERROR_INVALID_PARAMETER);
}

CT(gap4_hist_unknown_exe_is_empty) {
    NOWINE();
    CREQ(GetConsoleCommandHistoryA); CREQ(GetConsoleCommandHistoryLengthA); CREQ(GetConsoleCommandHistoryLengthW);
    CEQ(GetConsoleCommandHistoryLengthA("gap4_no_history.exe"), 0);
    CEQ(GetConsoleCommandHistoryLengthW(L"gap4_no_history.exe"), 0);
    char a[8]; memset(a, 0x55, sizeof a);
    CEQ(GetConsoleCommandHistoryA(a, sizeof a, "gap4_no_history.exe"), 0);
    CEQ((unsigned char)a[0], 0x55);
}

CT(gap4_hist_expunge_resets_length) {
    NOWINE();
    CREQ(ReadConsoleW); CREQ(GetConsoleCommandHistoryLengthW); CREQ(ExpungeConsoleCommandHistoryW);
    g4_setexe(); g4_cooked();
    CC(g4_line(L"gone\r"));
    CEQ(GetConsoleCommandHistoryLengthW(G4EXE), 5 * 2);
    ExpungeConsoleCommandHistoryW(G4EXE);
    CEQ(GetConsoleCommandHistoryLengthW(G4EXE), 0);
    CC(g4_line(L"back\r"));
    CEQ(GetConsoleCommandHistoryLengthW(G4EXE), 5 * 2);
}

CT(gap4_hist_empty_lines_not_recorded) {
    NOWINE();
    CREQ(ReadConsoleW); CREQ(GetConsoleCommandHistoryLengthA);
    g4_setexe(); g4_cooked();
    CC(g4_line(L"\r")); CC(g4_line(L"\r"));
    CEQ(GetConsoleCommandHistoryLengthA(G4EXEA), 0);
    CC(g4_line(L"x\r"));
    CEQ(GetConsoleCommandHistoryLengthA(G4EXEA), 2);
}

CT(gap4_hist_zero_size_buffer) {
    NOWINE();
    CREQ(ReadConsoleW); CREQ(GetConsoleCommandHistoryW);
    g4_setexe(); g4_cooked();
    CC(g4_line(L"zzz\r"));
    WCHAR b[4]; for (int i = 0; i < 4; i++) b[i] = 0x7777;
    SetLastError(0xBAD);
    DWORD r = GetConsoleCommandHistoryW(b, 0, G4EXE);
    cnote("history with 0-byte buffer -> %lu err=%lu", r, GetLastError());
    CEQ(b[0], 0x7777);
    SetLastError(0xBAD);
    r = GetConsoleCommandHistoryW(NULL, 0, G4EXE);
    cnote("history with NULL,0 -> %lu err=%lu", r, GetLastError());
}

/* ---------------- without a console ---------------- */

CTD(gap4_detached_alias_and_history_calls) {
    NOWINE();
    CREQ(GetConsoleAliasW); CREQ(GetConsoleAliasesLengthW); CREQ(GetConsoleAliasExesLengthW); CREQ(GetConsoleCommandHistoryLengthW);
    WCHAR w[16]; char a[16];
    SetLastError(0xBAD);
    CEQ(GetConsoleAliasW(L"x", w, sizeof w, G4EXE), 0);
    CERR(ERROR_INVALID_HANDLE);
    SetLastError(0xBAD);
    CEQ(GetConsoleAliasesLengthW(G4EXE), 0);
    CERR(ERROR_INVALID_HANDLE);
    SetLastError(0xBAD);
    CEQ(GetConsoleAliasExesLengthW(), 0);
    CERR(ERROR_INVALID_HANDLE);
    CEQ(GetConsoleAliasExesLengthA(), 0);
    SetLastError(0xBAD);
    CEQ(GetConsoleCommandHistoryLengthW(G4EXE), 0);
    CERR(ERROR_INVALID_HANDLE);
    CEQ(GetConsoleCommandHistoryLengthA(G4EXEA), 0);
    CEQ(GetConsoleAliasesA(a, sizeof a, G4EXEA), 0);
    CEQ(GetConsoleAliasExesA(a, sizeof a), 0);
    CEQ(GetConsoleCommandHistoryA(a, sizeof a, G4EXEA), 0);
}
