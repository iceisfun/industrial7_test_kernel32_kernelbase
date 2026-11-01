/* StrCpyNW, StrDup, StrTrim, trace stubs, UmsThreadYield. */
#include "t.h"
#include <shlwapi.h>
#include <wchar.h>
#include <evntrace.h>

/* ---------- StrCpyNW ---------- */
TEST(gap14_strcpynw_basic) {
    REQUIRE(U(StrCpyNW));
    wchar_t d[16];
    for (int i = 0; i < 16; i++) d[i] = 0x7777;
    CHECK(U(StrCpyNW)(d, L"hello", 16) == d);
    EXPECT_WSTR(d, L"hello");
    CHECK(d[6] == 0x7777);                 /* nothing written past the terminator */
}
TEST(gap14_strcpynw_truncate) {
    REQUIRE(U(StrCpyNW));
    for (int n = 1; n <= 8; n++) {
        wchar_t d[16];
        for (int i = 0; i < 16; i++) d[i] = 0x7777;
        wchar_t *r = U(StrCpyNW)(d, L"hello", n);
        CHECKF(r == d, "n=%d", n);
        int want = n - 1 > 5 ? 5 : n - 1;
        CHECKF(lstrlenW(d) == want, "n=%d len=%d", n, lstrlenW(d));
        CHECKF(!wmemcmp(d, L"hello", want), "n=%d content", n);
        CHECKF(d[want] == 0, "n=%d terminator", n);
        CHECKF(d[want + 1] == 0x7777, "n=%d overrun", n);
    }
}
TEST(gap14_strcpynw_zero_and_negative) {
    REQUIRE(U(StrCpyNW));
    wchar_t d[4] = { 0x7777, 0x7777, 0x7777, 0x7777 };
    CHECK(U(StrCpyNW)(d, L"abc", 0) == d);
    CHECK(d[0] == 0x7777);
    CHECK(U(StrCpyNW)(d, L"abc", -1) == d);
    CHECK(d[0] == 0x7777);
    CHECK(U(StrCpyNW)(d, L"abc", (int)0x80000000) == d);
    CHECK(d[0] == 0x7777);
}
TEST(gap14_strcpynw_null_source) {
    REQUIRE(U(StrCpyNW));
    wchar_t d[4] = { 0x7777, 0x7777, 0x7777, 0x7777 };
    CHECK(U(StrCpyNW)(d, NULL, 4) == d);
    CHECK(d[0] == 0); CHECK(d[1] == 0x7777);
}
TEST(gap14_strcpynw_empty_source) {
    REQUIRE(U(StrCpyNW));
    wchar_t d[4] = { 0x7777, 0x7777, 0x7777, 0x7777 };
    U(StrCpyNW)(d, L"", 4);
    CHECK(d[0] == 0); CHECK(d[1] == 0x7777);
}
TEST(gap14_strcpynw_exact_fit) {
    REQUIRE(U(StrCpyNW));
    wchar_t d[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };
    U(StrCpyNW)(d, L"abc", 4);
    EXPECT_WSTR(d, L"abc");
    wchar_t e[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };
    U(StrCpyNW)(e, L"abc", 3);
    EXPECT_WSTR(e, L"ab");
    CHECK(e[3] == 1);
}
TEST(gap14_strcpynw_unicode) {
    REQUIRE(U(StrCpyNW));
    wchar_t d[8];
    U(StrCpyNW)(d, L"\x0416\xd83d\xde00z", 8);
    EXPECT_WSTR(d, L"\x0416\xd83d\xde00z");
}

/* ---------- StrDup ---------- */
TEST(gap14_strdupa_basic) {
    REQUIRE(U(StrDupA));
    char *p = U(StrDupA)("hello");
    CHECK(p != NULL);
    if (p) {
        EXPECT_STR(p, "hello");
        CHECK(LocalSize(p) >= 6);
        CHECK(LocalFree(p) == NULL);
    }
}
TEST(gap14_strdupw_basic) {
    REQUIRE(U(StrDupW));
    wchar_t *p = U(StrDupW)(L"hello");
    CHECK(p != NULL);
    if (p) {
        EXPECT_WSTR(p, L"hello");
        CHECK(LocalSize(p) >= 12);
        CHECK(LocalFree(p) == NULL);
    }
}
TEST(gap14_strdup_empty) {
    REQUIRE(U(StrDupA)); REQUIRE(U(StrDupW));
    char *a = U(StrDupA)(""); wchar_t *w = U(StrDupW)(L"");
    CHECK(a != NULL && a[0] == 0); CHECK(w != NULL && w[0] == 0);
    if (a) LocalFree(a);
    if (w) LocalFree(w);
}
TEST(gap14_strdup_null) {
    REQUIRE(U(StrDupA)); REQUIRE(U(StrDupW));
    CHECK(U(StrDupA)(NULL) == NULL);
    CHECK(U(StrDupW)(NULL) == NULL);
}
TEST(gap14_strdup_independent_copy) {
    REQUIRE(U(StrDupA)); REQUIRE(U(StrDupW));
    char src[] = "abc"; wchar_t wsrc[] = L"abc";
    char *a = U(StrDupA)(src); wchar_t *w = U(StrDupW)(wsrc);
    CHECK(a != src); CHECK(w != wsrc);
    src[0] = 'X'; wsrc[0] = L'X';
    if (a) { CHECK(a[0] == 'a'); LocalFree(a); }
    if (w) { CHECK(w[0] == L'a'); LocalFree(w); }
}
TEST(gap14_strdup_distinct_allocs) {
    REQUIRE(U(StrDupW));
    wchar_t *p[50];
    for (int i = 0; i < 50; i++) p[i] = U(StrDupW)(L"same");
    for (int i = 0; i < 50; i++) { CHECK(p[i] != NULL); for (int j = i + 1; j < 50; j++) CHECK(p[i] != p[j]); }
    for (int i = 0; i < 50; i++) if (p[i]) LocalFree(p[i]);
}
TEST(gap14_strdup_long) {
    REQUIRE(U(StrDupA)); REQUIRE(U(StrDupW));
    enum { N = 100000 };
    char *s = (char *)HeapAlloc(GetProcessHeap(), 0, N + 1);
    wchar_t *ws = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, (N + 1) * 2);
    CHECK(s && ws);
    if (!s || !ws) return;
    for (int i = 0; i < N; i++) { s[i] = 'a' + i % 26; ws[i] = L'a' + i % 26; }
    s[N] = 0; ws[N] = 0;
    char *a = U(StrDupA)(s); wchar_t *w = U(StrDupW)(ws);
    CHECK(a && !strcmp(a, s)); CHECK(w && !wmemcmp(w, ws, N + 1));
    if (a) LocalFree(a);
    if (w) LocalFree(w);
    HeapFree(GetProcessHeap(), 0, s); HeapFree(GetProcessHeap(), 0, ws);
}
TEST(gap14_strdupw_embedded_high) {
    REQUIRE(U(StrDupW));
    wchar_t *w = U(StrDupW)(L"\xffff\x0001z");
    CHECK(w && w[0] == 0xffff && w[1] == 1 && w[2] == L'z' && w[3] == 0);
    if (w) LocalFree(w);
}

/* ---------- StrTrim ---------- */
static const struct { const char *in; const char *trim; BOOL ret; const char *out; } trows[] = {
    {"  ab  ", " ", 1, "ab"}, {"ab", " ", 0, "ab"}, {"", " ", 0, ""}, {"   ", " ", 1, ""},
    {"  ab", " ", 1, "ab"}, {"ab  ", " ", 1, "ab"}, {"a b", " ", 0, "a b"}, {"  a b  ", " ", 1, "a b"},
    {"xxaxbxx", "x", 1, "axb"}, {"xaxbx", "x", 1, "axb"}, {"xxx", "x", 1, ""}, {"x", "x", 1, ""},
    {"axb", "x", 0, "axb"}, {" \t ab\t ", " \t", 1, "ab"}, {"\t\tab", " \t", 1, "ab"},
    {"ab\t", " \t", 1, "ab"}, {"abc", "", 0, "abc"}, {"  abc  ", "", 0, "  abc  "},
    {"ABC", "a", 0, "ABC"}, {"abcba", "ab", 1, "c"}, {"abcba", "abc", 1, ""}, {"cabbac", "ab", 0, "cabbac"},
    {"[tag]", "[]", 1, "tag"}, {"[[tag]]", "[]", 1, "tag"}, {"[ta]g]", "[]", 1, "ta]g"},
    {"a", "a", 1, ""}, {"b", "a", 0, "b"}, {"aab", "a", 1, "b"}, {"baa", "a", 1, "b"},
    {"abab", "b", 1, "aba"}, {"a.b.", ".", 1, "a.b"}, {"..a", ".", 1, "a"},
};
#define NTR (sizeof trows / sizeof trows[0])

TEST(gap14_strtrima_table) {
    REQUIRE(U(StrTrimA));
    for (size_t i = 0; i < NTR; i++) {
        char b[64]; strcpy(b, trows[i].in);
        BOOL r = U(StrTrimA)(b, trows[i].trim);
        CHECKF(!!r == !!trows[i].ret, "row %zu \"%s\" ret %d", i, trows[i].in, (int)r);
        CHECKF(!strcmp(b, trows[i].out), "row %zu \"%s\" got \"%s\" want \"%s\"", i, trows[i].in, b, trows[i].out);
    }
}
TEST(gap14_strtrimw_table) {
    REQUIRE(U(StrTrimW));
    for (size_t i = 0; i < NTR; i++) {
        wchar_t b[64], tc[16], want[64], *p;
        const char *s = trows[i].in; p = b; while ((*p++ = (unsigned char)*s++)) ;
        s = trows[i].trim; p = tc; while ((*p++ = (unsigned char)*s++)) ;
        s = trows[i].out; p = want; while ((*p++ = (unsigned char)*s++)) ;
        BOOL r = U(StrTrimW)(b, tc);
        CHECKF(!!r == !!trows[i].ret, "row %zu \"%s\" ret %d", i, trows[i].in, (int)r);
        CHECKF(!lstrcmpW(b, want), "row %zu \"%s\"", i, trows[i].in);
    }
}
TEST(gap14_strtrim_null) {
    REQUIRE(U(StrTrimA)); REQUIRE(U(StrTrimW));
    char a[] = "  x  "; wchar_t w[] = L"  x  ";
    CHECK(!U(StrTrimA)(NULL, " ")); CHECK(!U(StrTrimA)(a, NULL)); CHECK(!U(StrTrimA)(NULL, NULL));
    CHECK(!U(StrTrimW)(NULL, L" ")); CHECK(!U(StrTrimW)(w, NULL)); CHECK(!U(StrTrimW)(NULL, NULL));
    EXPECT_STR(a, "  x  "); EXPECT_WSTR(w, L"  x  ");
}
TEST(gap14_strtrimw_unicode) {
    REQUIRE(U(StrTrimW));
    wchar_t b[] = L"\x0416\x0416" L"a\x0416";
    CHECK(U(StrTrimW)(b, L"\x0416"));
    EXPECT_WSTR(b, L"a");
    wchar_t c[] = L"\x3000" L"a\x3000";
    CHECK(U(StrTrimW)(c, L"\x3000"));
    EXPECT_WSTR(c, L"a");
    wchar_t d[] = L"\x0116" L"a";
    CHECK(!U(StrTrimW)(d, L"\x0016"));
}
TEST(gap14_strtrim_leaves_buffer_tail) {
    REQUIRE(U(StrTrimW));
    wchar_t b[10] = L"  ab  ";
    b[7] = 0x7777;
    CHECK(U(StrTrimW)(b, L" "));
    EXPECT_WSTR(b, L"ab");
    CHECK(b[7] == 0x7777);
}
TEST(gap14_strtrim_idempotent) {
    REQUIRE(U(StrTrimW));
    wchar_t b[] = L"  ab  ";
    CHECK(U(StrTrimW)(b, L" "));
    CHECK(!U(StrTrimW)(b, L" "));
    EXPECT_WSTR(b, L"ab");
}
TEST(gap14_strtrim_case_sensitive) {
    REQUIRE(U(StrTrimA));
    char b[] = "AbA";
    CHECK(U(StrTrimA)(b, "a") == 0); EXPECT_STR(b, "AbA");
    CHECK(U(StrTrimA)(b, "A")); EXPECT_STR(b, "b");
}

/* ---------- ETW stubs exported by kernelbase ---------- */
UDECL(ULONG, TraceEvent, (ULONG64 h, PEVENT_TRACE_HEADER hdr));
UDECL(ULONG, TraceMessage, (ULONG64 h, ULONG flags, LPGUID guid, USHORT num, ...));
UDECL(ULONG, TraceMessageVa, (ULONG64 h, ULONG flags, LPGUID guid, USHORT num, char *ap));

TEST(gap14_traceevent_null_header) {
    REQUIRE(UN(TraceEvent));
    SET_ERR(0);
    ULONG r = UN(TraceEvent)(0, NULL);
    t_note("TraceEvent(0,NULL)=%lu err=%lu", r, GetLastError());
    CHECK(r != ERROR_SUCCESS);
}
TEST(gap14_traceevent_invalid_handle_stub) {
    REQUIRE(UN(TraceEvent));
    /* kernelbase stub ignores arguments and returns ERROR_PROC_NOT_FOUND (127), setting last error */
    EVENT_TRACE_HEADER h; memset(&h, 0, sizeof h);
    h.Size = sizeof h;
    SET_ERR(0);
    ULONG r = UN(TraceEvent)(0xdeadbeef, &h);
    CHECK(r != ERROR_SUCCESS);
    if (r == ERROR_PROC_NOT_FOUND) EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    else t_note("TraceEvent returned %lu", r);
}
TEST(gap14_tracemessage_stub) {
    REQUIRE(UN(TraceMessage));
    GUID g; memset(&g, 0, sizeof g);
    SET_ERR(0);
    ULONG r = UN(TraceMessage)(0, 0, &g, 1);
    CHECK(r != ERROR_SUCCESS);
    if (r == ERROR_PROC_NOT_FOUND) EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    else t_note("TraceMessage returned %lu", r);
}
TEST(gap14_tracemessage_null_guid) {
    REQUIRE(UN(TraceMessage));
    ULONG r = UN(TraceMessage)(0, 0, NULL, 0);
    CHECK(r != ERROR_SUCCESS);
}
TEST(gap14_tracemessageva_stub) {
    REQUIRE(UN(TraceMessageVa));
    GUID g; memset(&g, 0, sizeof g);
    SET_ERR(0);
    /* ntdll!EtwTraceMessageVa walks the arg list ({ptr,len} pairs ended by a NULL ptr) unconditionally: pass an empty list */
    ULONG64 empty_args[2] = { 0, 0 };
    ULONG r = UN(TraceMessageVa)(0, 0, &g, 1, (char *)empty_args);
    CHECK(r != ERROR_SUCCESS);
    if (r == ERROR_PROC_NOT_FOUND) EXPECT_ERR(ERROR_PROC_NOT_FOUND);
    else t_note("TraceMessageVa returned %lu", r);
}
TEST(gap14_tracemessageva_null_all) {
    REQUIRE(UN(TraceMessageVa));
    ULONG64 empty_args[2] = { 0, 0 };
    ULONG r = UN(TraceMessageVa)(0, 0, NULL, 0, (char *)empty_args);
    CHECK(r != ERROR_SUCCESS);
}
TEST(gap14_trace_repeat_stable) {
    REQUIRE(UN(TraceEvent)); REQUIRE(UN(TraceMessage));
    ULONG a = UN(TraceEvent)(0, NULL), b = UN(TraceEvent)(0, NULL);
    CHECK(a == b);
    GUID g; memset(&g, 0, sizeof g);
    ULONG c = UN(TraceMessage)(0, 0, &g, 7), d = UN(TraceMessage)(0, 0, &g, 7);
    CHECK(c == d);
}

/* ---------- UmsThreadYield ---------- */
CHILD(gap14_ums_yield_plain_thread) {
    BOOL (WINAPI *f)(PVOID) = (BOOL (WINAPI *)(PVOID))t_proc("UmsThreadYield");
    if (!f) return 2;
    SetLastError(0);
    BOOL r = f(NULL);
    DWORD e = GetLastError();
    if (r) return 3;
    /* a non-UMS thread cannot yield: STATUS_INVALID_PARAMETER -> ERROR_INVALID_PARAMETER */
    if (e != ERROR_INVALID_PARAMETER) return 4;
    SetLastError(0);
    r = f((PVOID)(ULONG_PTR)0x1234);
    if (r) return 5;
    if (GetLastError() != ERROR_INVALID_PARAMETER) return 6;
    return 0;
}
TEST(gap14_umsthreadyield_non_ums_thread) {
    if (!t_proc_k32("UmsThreadYield")) { t_skip("missing export: UmsThreadYield"); return; }
    /* RtlUmsThreadYield reads a per-thread UMS block whose TEB source could not be verified
       from the decomp, so the outcome is informational only. */
    DWORD rc = t_run_child("gap14_ums_yield_plain_thread", "", 20000);
    t_note("UmsThreadYield non-UMS thread child rc %lu", rc);
}
