#include "gap13.h"
/* Set* APIs that are dangerous on a live system: only safe error paths, scratch objects, or no-op values. */
#include <string.h>
#include <sddl.h>

/* ---------- StrCatBuffA/W, StrCatChainW ---------- */
TEST(gap13_strcatbuffa_basic) {
    REQUIRE(U(StrCatBuffA));
    char b[16] = "ab";
    EXPECT_EQ(U(StrCatBuffA)(b, "cd", 16), b);
    EXPECT_STR(b, "abcd");
    U(StrCatBuffA)(b, "", 16);
    EXPECT_STR(b, "abcd");
    char c[16] = "";
    U(StrCatBuffA)(c, "xyz", 16);
    EXPECT_STR(c, "xyz");
}
TEST(gap13_strcatbuffa_truncation) {
    REQUIRE(U(StrCatBuffA));
    char b[8]; memset(b, 'Q', sizeof b);
    strcpy(b, "ab");
    char *r = U(StrCatBuffA)(b, "cdefghij", 5);   /* only 5 bytes of buffer count */
    EXPECT_EQ(r, b);
    CHECK(strlen(b) <= 4);
    CHECK(strncmp(b, "abcd", strlen(b)) == 0);
    EXPECT_EQ(b[5], 'Q');                          /* nothing written past the declared size */
    EXPECT_EQ(b[6], 'Q');
}
TEST(gap13_strcatbuffa_exact_fit) {
    REQUIRE(U(StrCatBuffA));
    char b[8] = "ab";
    U(StrCatBuffA)(b, "cde", 6);                   /* "abcde" + NUL == 6 */
    EXPECT_STR(b, "abcde");
    char d[8] = "ab";
    U(StrCatBuffA)(d, "cdef", 6);                  /* needs 7 */
    CHECK(strlen(d) <= 5);
}
TEST(gap13_strcatbuffa_null) {
    REQUIRE(U(StrCatBuffA));
    if (t_is_wine()) { t_skip("emulator faults on NULL dest"); return; }
    EXPECT_EQ(U(StrCatBuffA)(NULL, "a", 4), NULL);
    char b[4] = "x";
    EXPECT_EQ(U(StrCatBuffA)(b, NULL, 4), b);
    EXPECT_STR(b, "x");
}
TEST(gap13_strcatbuffa_zero_size) {
    REQUIRE(U(StrCatBuffA));
    char b[4] = "x";
    EXPECT_EQ(U(StrCatBuffA)(b, "y", 0), b);
    EXPECT_STR(b, "x");
}
TEST(gap13_strcatbuffw_basic) {
    REQUIRE(U(StrCatBuffW));
    WCHAR b[16] = L"ab";
    EXPECT_EQ(U(StrCatBuffW)(b, L"cd", 16), b);
    EXPECT_WSTR(b, L"abcd");
    WCHAR c[16] = L"";
    U(StrCatBuffW)(c, L"xyz", 16);
    EXPECT_WSTR(c, L"xyz");
}
TEST(gap13_strcatbuffw_truncation) {
    REQUIRE(U(StrCatBuffW));
    WCHAR b[8]; for (int i = 0; i < 8; i++) b[i] = L'Q';
    b[0] = L'a'; b[1] = L'b'; b[2] = 0;
    U(StrCatBuffW)(b, L"cdefghij", 5);
    CHECK(lstrlenW(b) <= 4);
    EXPECT_EQ(b[5], L'Q');
    EXPECT_EQ(b[6], L'Q');
    EXPECT_EQ(b[7], L'Q');
}
TEST(gap13_strcatbuffw_exact_and_null) {
    REQUIRE(U(StrCatBuffW));
    WCHAR b[8] = L"ab";
    U(StrCatBuffW)(b, L"cde", 6);
    EXPECT_WSTR(b, L"abcde");
    if (!t_is_wine()) EXPECT_EQ(U(StrCatBuffW)(NULL, L"a", 4), NULL);
    WCHAR c[4] = L"x";
    EXPECT_EQ(U(StrCatBuffW)(c, NULL, 4), c);
    EXPECT_WSTR(c, L"x");
    U(StrCatBuffW)(c, L"y", 0);
    EXPECT_WSTR(c, L"x");
}
TEST(gap13_strcatchainw_basic) {
    REQUIRE(U(StrCatChainW));
    WCHAR b[16] = L"";
    DWORD at = U(StrCatChainW)(b, 16, 0, L"abc");
    EXPECT_EQ(at, 3);
    EXPECT_WSTR(b, L"abc");
    at = U(StrCatChainW)(b, 16, at, L"de");
    EXPECT_EQ(at, 5);
    EXPECT_WSTR(b, L"abcde");
    at = U(StrCatChainW)(b, 16, at, L"");
    EXPECT_EQ(at, 5);
    EXPECT_WSTR(b, L"abcde");
}
TEST(gap13_strcatchainw_minus1_appends) {
    REQUIRE(U(StrCatChainW));
    WCHAR b[16] = L"ab";
    DWORD at = U(StrCatChainW)(b, 16, (DWORD)-1, L"cd");
    EXPECT_EQ(at, 4);
    EXPECT_WSTR(b, L"abcd");
    at = U(StrCatChainW)(b, 16, 1, L"Z");           /* explicit offset overwrites */
    EXPECT_EQ(at, 2);
    EXPECT_WSTR(b, L"aZ");
}
TEST(gap13_strcatchainw_truncation) {
    REQUIRE(U(StrCatChainW));
    WCHAR b[8]; for (int i = 0; i < 8; i++) b[i] = L'Q';
    DWORD at = U(StrCatChainW)(b, 5, 0, L"abcdefgh");
    EXPECT_EQ(at, 4);                                /* cch-1 */
    CHECK(b[4] == 0);
    CHECK(!wcsncmp(b, L"abcd", 4));
    EXPECT_EQ(b[5], L'Q');
}
TEST(gap13_strcatchainw_exact_fit) {
    REQUIRE(U(StrCatChainW));
    WCHAR b[8]; for (int i = 0; i < 8; i++) b[i] = L'Q';
    DWORD at = U(StrCatChainW)(b, 5, 0, L"abcd");   /* 4 chars + NUL fits */
    EXPECT_EQ(at, 4);
    EXPECT_WSTR(b, L"abcd");
    EXPECT_EQ(b[5], L'Q');
}
TEST(gap13_strcatchainw_zero_size) {
    REQUIRE(U(StrCatChainW));
    WCHAR b[4] = L"xy";
    DWORD at = U(StrCatChainW)(b, 0, 2, L"zz");
    EXPECT_EQ(at, 2);
    EXPECT_WSTR(b, L"xy");
}
TEST(gap13_strcatchainw_offset_beyond) {
    REQUIRE(U(StrCatChainW));
    WCHAR b[8]; for (int i = 0; i < 8; i++) b[i] = L'Q';
    /* ichAt > cchDst: nothing written, offset returned unchanged */
    DWORD at = U(StrCatChainW)(b, 4, 6, L"zz");
    EXPECT_EQ(at, 6);
    EXPECT_EQ(b[6], L'Q');
    /* ichAt == cchDst: terminator forced at cch-1 */
    at = U(StrCatChainW)(b, 4, 4, L"zz");
    EXPECT_EQ(at, 3);
    EXPECT_EQ(b[3], 0);
}
TEST(gap13_strcatchainw_chain_many) {
    REQUIRE(U(StrCatChainW));
    WCHAR b[32] = L"";
    const WCHAR *parts[] = { L"one", L" ", L"two", L" ", L"three" };
    DWORD at = 0;
    for (int i = 0; i < 5; i++) at = U(StrCatChainW)(b, 32, at, parts[i]);
    EXPECT_EQ(at, 13);
    EXPECT_WSTR(b, L"one two three");
}

/* ---------- SetTokenInformation ---------- */
TEST(gap13_settokeninfo_null_handle) {
    REQUIRE(U(SetTokenInformation));
    DWORD v = 0;
    SET_ERR(0);
    EXPECT_EQ(U(SetTokenInformation)(NULL, TokenSessionId, &v, sizeof v), FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap13_settokeninfo_invalid_handle_value) {
    REQUIRE(U(SetTokenInformation));
    DWORD v = 0;
    SET_ERR(0);
    EXPECT_EQ(U(SetTokenInformation)(INVALID_HANDLE_VALUE, TokenSessionId, &v, sizeof v), FALSE);
    DWORD e = GetLastError();
    CHECK(e == ERROR_INVALID_HANDLE || e == ERROR_ACCESS_DENIED);
}
TEST(gap13_settokeninfo_wrong_handle_type) {
    REQUIRE(U(SetTokenInformation));
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    CHECK(ev != NULL);
    DWORD v = 0;
    SET_ERR(0);
    EXPECT_EQ(U(SetTokenInformation)(ev, TokenSessionId, &v, sizeof v), FALSE);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_INVALID_HANDLE, "err %lu", e);
    CloseHandle(ev);
}
TEST(gap13_settokeninfo_closed_handle) {
    REQUIRE(U(SetTokenInformation));
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    CloseHandle(ev);
    DWORD v = 0;
    SET_ERR(0);
    EXPECT_EQ(U(SetTokenInformation)(ev, TokenSessionId, &v, sizeof v), FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap13_settokeninfo_query_only_denied) {
    REQUIRE(U(SetTokenInformation));
    HANDLE tok = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) { t_skip("no token"); return; }
    /* TOKEN_QUERY lacks TOKEN_ADJUST_DEFAULT: setting owner to its own owner cannot succeed */
    BYTE buf[256]; DWORD len = 0;
    if (!GetTokenInformation(tok, TokenOwner, buf, sizeof buf, &len)) { CloseHandle(tok); t_skip("owner"); return; }
    SET_ERR(0);
    BOOL ok = U(SetTokenInformation)(tok, TokenOwner, buf, len);
    CHECK(ok == FALSE);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(tok);
}
TEST(gap13_settokeninfo_bad_class_on_query_handle) {
    REQUIRE(U(SetTokenInformation));
    HANDLE tok = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) { t_skip("no token"); return; }
    DWORD v = 0;
    SET_ERR(0);
    BOOL ok = U(SetTokenInformation)(tok, (TOKEN_INFORMATION_CLASS)0, &v, sizeof v);
    CHECK(ok == FALSE);
    DWORD e = GetLastError();
    t_note("class 0 set error %lu", e);
    SET_ERR(0);
    ok = U(SetTokenInformation)(tok, (TOKEN_INFORMATION_CLASS)0x7fff, &v, sizeof v);
    CHECK(ok == FALSE);
    t_note("class 0x7fff set error %lu", GetLastError());
    CloseHandle(tok);
}
TEST(gap13_settokeninfo_query_only_classes_fail) {
    REQUIRE(U(SetTokenInformation));
    /* classes that are query-only can never be set */
    HANDLE tok = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &tok)) { t_skip("no token"); return; }
    HANDLE dup = NULL;
    if (!DuplicateTokenEx(tok, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenImpersonation, &dup)) {
        CloseHandle(tok); t_skip("dup"); return;
    }
    DWORD v = 1;
    TOKEN_INFORMATION_CLASS cls[] = { TokenType, TokenStatistics, TokenElevation, TokenGroups };
    for (int i = 0; i < 4; i++) {
        SET_ERR(0);
        BOOL ok = U(SetTokenInformation)(dup, cls[i], &v, sizeof v);
        CHECKF(ok == FALSE, "class %d", (int)cls[i]);
        DWORD e = GetLastError();
        CHECKF(e != 0, "class %d error", (int)cls[i]);
    }
    CloseHandle(dup); CloseHandle(tok);
}
TEST(gap13_settokeninfo_owner_on_duplicate) {
    REQUIRE(U(SetTokenInformation));
    HANDLE tok = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &tok)) { t_skip("no token"); return; }
    HANDLE dup = NULL;
    if (!DuplicateTokenEx(tok, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenImpersonation, &dup)) {
        CloseHandle(tok); t_skip("dup"); return;
    }
    BYTE ub[512]; DWORD len = 0;
    CHECK(GetTokenInformation(dup, TokenUser, ub, sizeof ub, &len));
    TOKEN_OWNER ow; ow.Owner = ((TOKEN_USER *)ub)->User.Sid;
    SET_ERR(0);
    BOOL ok = U(SetTokenInformation)(dup, TokenOwner, &ow, sizeof ow);
    CHECKF(ok, "err %lu", GetLastError());
    BYTE ob[512];
    CHECK(GetTokenInformation(dup, TokenOwner, ob, sizeof ob, &len));
    CHECK(EqualSid(((TOKEN_OWNER *)ob)->Owner, ((TOKEN_USER *)ub)->User.Sid));
    /* the duplicate is private: the process token is unaffected */
    CloseHandle(dup); CloseHandle(tok);
}
TEST(gap13_settokeninfo_owner_null_sid) {
    REQUIRE(U(SetTokenInformation));
    HANDLE tok = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &tok)) { t_skip("no token"); return; }
    HANDLE dup = NULL;
    if (!DuplicateTokenEx(tok, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenImpersonation, &dup)) {
        CloseHandle(tok); t_skip("dup"); return;
    }
    TOKEN_OWNER ow; ow.Owner = NULL;
    SET_ERR(0);
    BOOL ok = U(SetTokenInformation)(dup, TokenOwner, &ow, sizeof ow);
    CHECK(ok == FALSE);
    t_note("NULL owner sid error %lu", GetLastError());
    /* length too small */
    SET_ERR(0);
    ok = U(SetTokenInformation)(dup, TokenOwner, &ow, 1);
    CHECK(ok == FALSE);
    DWORD e = GetLastError();
    CHECKF(e == ERROR_BAD_LENGTH || e == ERROR_INSUFFICIENT_BUFFER || e == ERROR_INVALID_PARAMETER, "err %lu", e);
    CloseHandle(dup); CloseHandle(tok);
}
TEST(gap13_settokeninfo_primarygroup_on_duplicate) {
    REQUIRE(U(SetTokenInformation));
    HANDLE tok = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &tok)) { t_skip("no token"); return; }
    HANDLE dup = NULL;
    if (!DuplicateTokenEx(tok, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenImpersonation, &dup)) {
        CloseHandle(tok); t_skip("dup"); return;
    }
    BYTE gb[512]; DWORD len = 0;
    CHECK(GetTokenInformation(dup, TokenPrimaryGroup, gb, sizeof gb, &len));
    SET_ERR(0);
    BOOL ok = U(SetTokenInformation)(dup, TokenPrimaryGroup, gb, sizeof(TOKEN_PRIMARY_GROUP));
    if (!ok) t_note("primary group set failed err %lu (group may not be in token)", GetLastError());
    CloseHandle(dup); CloseHandle(tok);
}
TEST(gap13_settokeninfo_session_requires_tcb) {
    REQUIRE(U(SetTokenInformation));
    HANDLE tok = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &tok)) { t_skip("no token"); return; }
    HANDLE dup = NULL;
    if (!DuplicateTokenEx(tok, MAXIMUM_ALLOWED, NULL, SecurityImpersonation, TokenImpersonation, &dup)) {
        CloseHandle(tok); t_skip("dup"); return;
    }
    /* writing the session id of a private duplicate cannot affect the process */
    DWORD sid = 0;
    DWORD cur = 0, len = 0;
    GetTokenInformation(dup, TokenSessionId, &cur, sizeof cur, &len);
    SET_ERR(0);
    BOOL ok = U(SetTokenInformation)(dup, TokenSessionId, &cur, sizeof cur);
    if (!ok) { CHECK(GetLastError() == ERROR_PRIVILEGE_NOT_HELD); }
    else t_note("SeTcbPrivilege held; session id set to its own value");
    CloseHandle(dup); CloseHandle(tok);
}

/* ---------- Tape APIs: all on invalid / non-tape handles ---------- */
TEST(gap13_settapeparameters_bad_operation) {
    REQUIRE(U(SetTapeParameters));
    /* operations other than 0 and 1 return ERROR_INVALID_FUNCTION without touching the handle */
    EXPECT_EQ(U(SetTapeParameters)(INVALID_HANDLE_VALUE, 2, NULL), ERROR_INVALID_FUNCTION);
    EXPECT_EQ(U(SetTapeParameters)(NULL, 2, NULL), ERROR_INVALID_FUNCTION);
    EXPECT_EQ(U(SetTapeParameters)(NULL, 0xffffffff, NULL), ERROR_INVALID_FUNCTION);
    EXPECT_EQ(U(SetTapeParameters)(NULL, 100, NULL), ERROR_INVALID_FUNCTION);
}
TEST(gap13_settapeparameters_invalid_handle) {
    REQUIRE(U(SetTapeParameters));
    TAPE_SET_MEDIA_PARAMETERS mp; mp.BlockSize = 512;
    DWORD r = U(SetTapeParameters)(INVALID_HANDLE_VALUE, SET_TAPE_MEDIA_INFORMATION, &mp);
    CHECKF(r == ERROR_INVALID_HANDLE, "r=%lu", r);
    r = U(SetTapeParameters)(NULL, SET_TAPE_MEDIA_INFORMATION, &mp);
    CHECKF(r == ERROR_INVALID_HANDLE, "r=%lu", r);
    TAPE_SET_DRIVE_PARAMETERS dp; memset(&dp, 0, sizeof dp);
    r = U(SetTapeParameters)(INVALID_HANDLE_VALUE, SET_TAPE_DRIVE_INFORMATION, &dp);
    CHECKF(r == ERROR_INVALID_HANDLE, "r=%lu", r);
}
TEST(gap13_settapeparameters_non_tape_handle) {
    REQUIRE(U(SetTapeParameters));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "gap13_tape.bin");
    HANDLE h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    TAPE_SET_MEDIA_PARAMETERS mp; mp.BlockSize = 512;
    DWORD r = U(SetTapeParameters)(h, SET_TAPE_MEDIA_INFORMATION, &mp);
    CHECK(r != NO_ERROR);
    t_note("SetTapeParameters on a file returns %lu", r);
    CloseHandle(h);
    DeleteFileA(p);
}
TEST(gap13_settapeparameters_closed_handle) {
    REQUIRE(U(SetTapeParameters));
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    CloseHandle(ev);
    TAPE_SET_MEDIA_PARAMETERS mp; mp.BlockSize = 512;
    DWORD r = U(SetTapeParameters)(ev, SET_TAPE_MEDIA_INFORMATION, &mp);
    CHECKF(r == ERROR_INVALID_HANDLE, "r=%lu", r);
}
TEST(gap13_settapeposition_invalid_handle) {
    REQUIRE(U(SetTapePosition));
    DWORD r = U(SetTapePosition)(INVALID_HANDLE_VALUE, TAPE_REWIND, 0, 0, 0, FALSE);
    CHECKF(r == ERROR_INVALID_HANDLE, "r=%lu", r);
    r = U(SetTapePosition)(NULL, TAPE_REWIND, 0, 0, 0, TRUE);
    CHECKF(r == ERROR_INVALID_HANDLE, "r=%lu", r);
}
TEST(gap13_settapeposition_methods_invalid_handle) {
    REQUIRE(U(SetTapePosition));
    DWORD methods[] = { TAPE_REWIND, TAPE_ABSOLUTE_BLOCK, TAPE_LOGICAL_BLOCK, TAPE_PSEUDO_LOGICAL_BLOCK,
                        TAPE_SPACE_END_OF_DATA, TAPE_SPACE_RELATIVE_BLOCKS, TAPE_SPACE_FILEMARKS,
                        TAPE_SPACE_SEQUENTIAL_FMKS, TAPE_SPACE_SETMARKS, TAPE_SPACE_SEQUENTIAL_SMKS, 0xdeadbeef };
    for (size_t i = 0; i < sizeof methods / sizeof *methods; i++) {
        DWORD r = U(SetTapePosition)(INVALID_HANDLE_VALUE, methods[i], 1, 2, 3, FALSE);
        CHECKF(r == ERROR_INVALID_HANDLE, "method 0x%lx r=%lu", methods[i], r);
    }
}
TEST(gap13_settapeposition_non_tape_handle) {
    REQUIRE(U(SetTapePosition));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "gap13_tapepos.bin");
    HANDLE h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    DWORD r = U(SetTapePosition)(h, TAPE_REWIND, 0, 0, 0, FALSE);
    CHECK(r != NO_ERROR);
    t_note("SetTapePosition on a file returns %lu", r);
    CloseHandle(h);
    DeleteFileA(p);
}
TEST(gap13_settape_does_not_set_lasterror_on_bad_op) {
    REQUIRE(U(SetTapeParameters));
    SET_ERR(0x4242);
    DWORD r = U(SetTapeParameters)(NULL, 7, NULL);
    EXPECT_EQ(r, ERROR_INVALID_FUNCTION);
    EXPECT_EQ(GetLastError(), 0x4242);
}

/* ---------- SetUmsThreadInformation: unsupported classes fail before touching the context ---------- */
TEST(gap13_setumsthreadinfo_bad_class) {
    REQUIRE(U(SetUmsThreadInformation));
    if (t_is_wine()) { t_skip("UMS stub under emulator"); return; }
    ULONG_PTR v = 0;
    SET_ERR(0);
    BOOL ok = U(SetUmsThreadInformation)(NULL, (UMS_THREAD_INFO_CLASS)0, &v, sizeof v);
    CHECK(ok == FALSE);
    EXPECT_ERR(ERROR_NOT_SUPPORTED);
    SET_ERR(0);
    ok = U(SetUmsThreadInformation)(NULL, (UMS_THREAD_INFO_CLASS)2, &v, sizeof v);
    CHECK(ok == FALSE);
    EXPECT_ERR(ERROR_NOT_SUPPORTED);
    SET_ERR(0);
    ok = U(SetUmsThreadInformation)(NULL, (UMS_THREAD_INFO_CLASS)0x7fffffff, &v, sizeof v);
    CHECK(ok == FALSE);
    EXPECT_ERR(ERROR_NOT_SUPPORTED);
}
TEST(gap13_setumsthreadinfo_bad_length) {
    REQUIRE(U(SetUmsThreadInformation));
    if (t_is_wine()) { t_skip("UMS stub under emulator"); return; }
    ULONG_PTR v = 0;
    ULONG lens[] = { 0, 1, 4, 7, 9, 16, 0xffffffff };
    for (int i = 0; i < 7; i++) {
        SET_ERR(0);
        BOOL ok = U(SetUmsThreadInformation)(NULL, UmsThreadUserContext, &v, lens[i]);
        CHECKF(ok == FALSE, "len %lu", lens[i]);
        DWORD e = GetLastError();
        CHECKF(e == ERROR_BAD_LENGTH, "len %lu err %lu", lens[i], e);
    }
}

/* ---------- SetThreadUILanguage (per-thread, run in a scratch thread) ---------- */
static DWORD WINAPI uilang_thread(LPVOID arg) {
    LANGID (WINAPI *set)(LANGID) = (LANGID (WINAPI *)(LANGID))arg;
    LANGID (WINAPI *get)(void) = (LANGID (WINAPI *)(void))t_proc("GetThreadUILanguage");
    DWORD result = 0;
    LANGID before = get ? get() : 0;

    /* neutral-sublang style ids (0x400, 0x800, ...) are rejected: ERROR_NOT_SUPPORTED, returns current */
    SetLastError(0);
    LANGID r = set(0x0400);
    if (GetLastError() != ERROR_NOT_SUPPORTED) result |= 1;
    if (get && r != get()) result |= 2;
    SetLastError(0);
    r = set(0x0c00);
    if (GetLastError() != ERROR_NOT_SUPPORTED) result |= 4;
    SetLastError(0);
    r = set(0x1000);
    if (GetLastError() != ERROR_NOT_SUPPORTED) result |= 8;

    /* an invalid language id is not found */
    SetLastError(0);
    r = set(0xfffe);
    if (GetLastError() != ERROR_FILE_NOT_FOUND) result |= 16;
    if (get && get() != before) result |= 32;

    /* reset (0) restores system default */
    SetLastError(0xbeef);
    r = set(0);
    if (get && r != get()) result |= 64;
    return result;
}
TEST(gap13_setthreaduilanguage_error_paths) {
    REQUIRE(U(SetThreadUILanguage));
    REQUIRE(U(GetThreadUILanguage));
    if (t_is_wine()) { t_skip("SetThreadUILanguage error paths differ in emulator"); return; }
    HANDLE th = CreateThread(NULL, 0, uilang_thread, (LPVOID)U(SetThreadUILanguage), 0, NULL);
    CHECK(th != NULL);
    if (!th) return;
    CHECK(WaitForSingleObject(th, 10000) == WAIT_OBJECT_0);
    DWORD code = 0xff; GetExitCodeThread(th, &code);
    CloseHandle(th);
    CHECKF(code == 0, "failure bits 0x%lx", code);
}
static DWORD WINAPI uilang_valid_thread(LPVOID arg) {
    LANGID (WINAPI *set)(LANGID) = (LANGID (WINAPI *)(LANGID))arg;
    LANGID (WINAPI *get)(void) = (LANGID (WINAPI *)(void))t_proc("GetThreadUILanguage");
    SetLastError(0xbeef);
    LANGID r = set(0x0409);
    DWORD err = GetLastError();
    LANGID now = get();
    set(0);
    return (r == 0x0409 && now == 0x0409 && err == 0) ? 0 : (r | (err << 16) | 0x80000000);
}
TEST(gap13_setthreaduilanguage_valid_enus) {
    REQUIRE(U(SetThreadUILanguage));
    REQUIRE(U(GetThreadUILanguage));
    if (t_is_wine()) { t_skip("MUI language state under emulator"); return; }
    HANDLE th = CreateThread(NULL, 0, uilang_valid_thread, (LPVOID)U(SetThreadUILanguage), 0, NULL);
    CHECK(th != NULL);
    if (!th) return;
    CHECK(WaitForSingleObject(th, 10000) == WAIT_OBJECT_0);
    DWORD code = 0; GetExitCodeThread(th, &code);
    CloseHandle(th);
    if (code != 0) t_note("setting 0x0409 reported 0x%lx (en-US may be unavailable)", code);
}
static DWORD WINAPI uilang_isolation_thread(LPVOID arg) {
    LANGID (WINAPI *set)(LANGID) = (LANGID (WINAPI *)(LANGID))arg;
    set(0x0407);
    Sleep(50);
    set(0);
    return 0;
}
TEST(gap13_setthreaduilanguage_per_thread) {
    REQUIRE(U(SetThreadUILanguage));
    REQUIRE(U(GetThreadUILanguage));
    LANGID before = U(GetThreadUILanguage)();
    HANDLE th = CreateThread(NULL, 0, uilang_isolation_thread, (LPVOID)U(SetThreadUILanguage), 0, NULL);
    if (!th) { t_skip("thread"); return; }
    WaitForSingleObject(th, 10000);
    CloseHandle(th);
    EXPECT_EQ(U(GetThreadUILanguage)(), before);        /* other thread's setting is invisible here */
}

/* ---------- SetSystemTimeAdjustment / SetTimeZoneInformation / SetUserGeo*: deliberately limited ---------- */
TEST(gap13_getsystemtimeadjustment_consistency) {
    /* SetSystemTimeAdjustment itself is never called (modifies the system clock rate). The getter
       defines the legal range for its argument. */
    REQUIRE(U(GetSystemTimeAdjustment));
    DWORD adj = 0, inc = 0; BOOL dis = 2;
    CHECK(U(GetSystemTimeAdjustment)(&adj, &inc, &dis));
    CHECK(inc > 0);
    CHECK(dis == TRUE || dis == FALSE);
    if (dis) CHECK(adj == inc);                          /* disabled: adjustment equals increment */
}
TEST(gap13_settimezone_exports_present) {
    /* SetTimeZoneInformation changes the machine time zone: no call is made here, only presence. */
    REQUIRE(U(SetTimeZoneInformation));
    REQUIRE(UK(SetTimeZoneInformation));
    CHECK(UK(SetTimeZoneInformation) != NULL);
    REQUIRE(U(GetTimeZoneInformation));
    TIME_ZONE_INFORMATION tz;
    DWORD r = U(GetTimeZoneInformation)(&tz);
    CHECK(r != TIME_ZONE_ID_INVALID);
}
TEST(gap13_setusergeoname_current_noop) {
    REQUIRE(U(SetUserGeoName));
    REQUIRE(U(GetUserDefaultGeoName));
    /* setting the already-current region name short-circuits and returns TRUE without any change */
    WCHAR cur[16] = {0};
    int n = U(GetUserDefaultGeoName)(cur, 16);
    if (n == 0) { t_skip("no geo name"); return; }
    SET_ERR(0);
    BOOL ok = U(SetUserGeoName)(cur);
    CHECKF(ok, "err %lu", GetLastError());
    WCHAR after[16] = {0};
    CHECK(U(GetUserDefaultGeoName)(after, 16) == n);
    EXPECT_WSTR(cur, after);
}
/* SetUserGeoID is not called with a valid id: it rewrites HKCU\Control Panel\International\Geo.
   Only its presence and the getter are checked. */
TEST(gap13_setusergeoid_present) {
    REQUIRE(U(SetUserGeoID));
    REQUIRE(UK(SetUserGeoID));
    REQUIRE(U(GetUserGeoID));
    GEOID cur = U(GetUserGeoID)(GEOCLASS_NATION);
    CHECK(cur != 0);
}
