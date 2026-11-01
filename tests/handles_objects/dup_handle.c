#include "ho.h"

#define DUP U(DuplicateHandle)
#define SELF GetCurrentProcess()
#define NEED_DUP() do { REQUIRE(U(DuplicateHandle)); REQUIRE(U(CloseHandle)); } while (0)

static HANDLE dup_simple(HANDLE h) {
    HANDLE d = NULL;
    if (!DUP(SELF, h, SELF, &d, 0, FALSE, DUPLICATE_SAME_ACCESS)) return NULL;
    return d;
}

TEST(ho_dup_basic_same_access) {
    NEED_DUP();
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL), d = NULL;
    SET_ERR(0);
    EXPECT_EQ(DUP(SELF, h, SELF, &d, 0, FALSE, DUPLICATE_SAME_ACCESS), TRUE);
    CHECK(d != NULL && d != h);
    EXPECT_EQ(ho_access(d), ho_access(h));
    CHECK(SetEvent(d));
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
    CloseHandle(h);
    EXPECT_EQ(WaitForSingleObject(d, 0), WAIT_OBJECT_0); /* dup outlives source */
    CloseHandle(d);
}
TEST(ho_dup_success_keeps_last_error) {
    NEED_DUP();
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL), d = NULL;
    SET_ERR(0x2222);
    EXPECT_EQ(DUP(SELF, h, SELF, &d, 0, FALSE, DUPLICATE_SAME_ACCESS), TRUE);
    EXPECT_ERR(0x2222);
    CloseHandle(h); CloseHandle(d);
}
TEST(ho_dup_target_value_is_multiple_of_4) {
    NEED_DUP();
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL);
    for (int i = 0; i < 20; i++) {
        HANDLE d = dup_simple(h);
        CHECK(d != NULL); if (!d) break;
        EXPECT_EQ((ULONG_PTR)d & 3, 0);
        CHECK((ULONG_PTR)d != 0);
        CloseHandle(d);
    }
    CloseHandle(h);
}
TEST(ho_dup_handle_count_object) {
    NEED_DUP();
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL);
    EXPECT_EQ(ho_hcount(h), 1);
    HANDLE d = dup_simple(h);
    EXPECT_EQ(ho_hcount(h), 2);
    HANDLE d2 = dup_simple(d);
    EXPECT_EQ(ho_hcount(h), 3);
    CloseHandle(d);
    EXPECT_EQ(ho_hcount(h), 2);
    CloseHandle(d2); CloseHandle(h);
}
TEST(ho_dup_process_handle_count_delta) {
    NEED_DUP(); NEED_HC();
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL);
    DWORD base = ho_count(); HANDLE d[16];
    for (int i = 0; i < 16; i++) d[i] = dup_simple(h);
    EXPECT_EQ(ho_count(), base + 16);
    for (int i = 0; i < 16; i++) CloseHandle(d[i]);
    EXPECT_EQ(ho_count(), base);
    CloseHandle(h);
}
TEST(ho_dup_desired_access_subset) {
    NEED_DUP();
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL), d = NULL;
    EXPECT_EQ(DUP(SELF, h, SELF, &d, SYNCHRONIZE, FALSE, 0), TRUE);
    EXPECT_EQ(ho_access(d), SYNCHRONIZE);
    SET_ERR(0);
    CHECK(!SetEvent(d)); EXPECT_ERR(ERROR_ACCESS_DENIED);  /* no EVENT_MODIFY_STATE */
    SetEvent(h);
    EXPECT_EQ(WaitForSingleObject(d, 0), WAIT_OBJECT_0);
    CloseHandle(d);
    d = NULL;
    EXPECT_EQ(DUP(SELF, h, SELF, &d, EVENT_MODIFY_STATE, FALSE, 0), TRUE);
    EXPECT_EQ(ho_access(d), EVENT_MODIFY_STATE);
    SET_ERR(0);
    EXPECT_EQ(WaitForSingleObject(d, 0), WAIT_FAILED);   /* no SYNCHRONIZE */
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CHECK(ResetEvent(d));
    CloseHandle(d); CloseHandle(h);
}
TEST(ho_dup_desired_access_zero) {
    NEED_DUP();
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL), d = NULL;
    EXPECT_EQ(DUP(SELF, h, SELF, &d, 0, FALSE, 0), TRUE);
    EXPECT_EQ(ho_access(d), 0);
    SET_ERR(0);
    EXPECT_EQ(WaitForSingleObject(d, 0), WAIT_FAILED);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(d); CloseHandle(h);
}
TEST(ho_dup_cannot_upgrade_rights) {
    NEED_DUP();
    /* Rights escalation by DuplicateHandle depends on the object's DACL (an access check is made for bits the source handle lacks); observe only. */
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL), lim = NULL, d = NULL;
    EXPECT_EQ(DUP(SELF, h, SELF, &lim, SYNCHRONIZE, FALSE, 0), TRUE);
    SET_ERR(0);
    BOOL r = DUP(SELF, lim, SELF, &d, EVENT_MODIFY_STATE, FALSE, 0);
    t_note("DuplicateHandle escalation to EVENT_MODIFY_STATE: r=%d err=%lu access=0x%lx", r, GetLastError(), r ? (unsigned long)ho_access(d) : 0ul);
    if (r) { CloseHandle(d); d = NULL; }
    EXPECT_EQ(DUP(SELF, lim, SELF, &d, SYNCHRONIZE, FALSE, 0), TRUE);
    CloseHandle(d); CloseHandle(lim); CloseHandle(h);
}
TEST(ho_dup_same_access_ignores_desired) {
    NEED_DUP();
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL), lim = NULL, d = NULL;
    DUP(SELF, h, SELF, &lim, SYNCHRONIZE, FALSE, 0);
    EXPECT_EQ(DUP(SELF, lim, SELF, &d, EVENT_ALL_ACCESS, FALSE, DUPLICATE_SAME_ACCESS), TRUE);
    EXPECT_EQ(ho_access(d), SYNCHRONIZE);
    CloseHandle(d); CloseHandle(lim); CloseHandle(h);
}
TEST(ho_dup_generic_rights_mapped) {
    NEED_DUP();
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL), d = NULL;
    EXPECT_EQ(DUP(SELF, h, SELF, &d, GENERIC_READ, FALSE, 0), TRUE);
    ULONG a = ho_access(d);
    CHECK((a & GENERIC_READ) == 0);             /* generic bits are mapped away */
    CHECK((a & EVENT_QUERY_STATE) != 0);
    CHECK((a & EVENT_MODIFY_STATE) == 0);
    t_note("event GENERIC_READ -> 0x%lx", a);
    CloseHandle(d); d = NULL;
    EXPECT_EQ(DUP(SELF, h, SELF, &d, GENERIC_WRITE, FALSE, 0), TRUE);
    a = ho_access(d);
    CHECK((a & GENERIC_WRITE) == 0);
    CHECK((a & EVENT_MODIFY_STATE) != 0);
    t_note("event GENERIC_WRITE -> 0x%lx", a);
    CloseHandle(d); d = NULL;
    EXPECT_EQ(DUP(SELF, h, SELF, &d, GENERIC_ALL, FALSE, 0), TRUE);
    EXPECT_EQ(ho_access(d), EVENT_ALL_ACCESS);
    CloseHandle(d); d = NULL;
    EXPECT_EQ(DUP(SELF, h, SELF, &d, MAXIMUM_ALLOWED, FALSE, 0), TRUE);
    t_note("event MAXIMUM_ALLOWED -> 0x%lx", ho_access(d));
    CloseHandle(d); CloseHandle(h);
}
TEST(ho_dup_inherit_flag) {
    NEED_DUP();
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL), d = NULL;
    EXPECT_EQ(DUP(SELF, h, SELF, &d, 0, TRUE, DUPLICATE_SAME_ACCESS), TRUE);
    EXPECT_EQ(ho_flags(d) & HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    EXPECT_EQ(ho_flags(h) & HANDLE_FLAG_INHERIT, 0);
    CloseHandle(d); d = NULL;
    EXPECT_EQ(DUP(SELF, h, SELF, &d, 0, FALSE, DUPLICATE_SAME_ACCESS), TRUE);
    EXPECT_EQ(ho_flags(d) & HANDLE_FLAG_INHERIT, 0);
    CloseHandle(d); CloseHandle(h);
}
TEST(ho_dup_inherit_not_copied_from_source) {
    NEED_DUP();
    HANDLE h = ho_mkevent(TRUE, TRUE, FALSE), d = NULL;
    EXPECT_EQ(ho_flags(h) & HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    EXPECT_EQ(DUP(SELF, h, SELF, &d, 0, FALSE, DUPLICATE_SAME_ACCESS), TRUE);
    EXPECT_EQ(ho_flags(d) & HANDLE_FLAG_INHERIT, 0);  /* only bInheritHandle decides */
    CloseHandle(d); CloseHandle(h);
}
TEST(ho_dup_same_attributes_option) {
    NEED_DUP();
    HANDLE h = ho_mkevent(TRUE, TRUE, FALSE), d = NULL;
    SET_ERR(0);
    BOOL r = DUP(SELF, h, SELF, &d, 0, FALSE, DUPLICATE_SAME_ACCESS | 4 /* DUPLICATE_SAME_ATTRIBUTES */);
    DWORD e = GetLastError();
    t_note("SAME_ATTRIBUTES: r=%d err=%lu flags=0x%lx", r, e, r ? ho_flags(d) : 0);
    if (r) CloseHandle(d);
    CloseHandle(h);
}
TEST(ho_dup_protect_flag_not_copied) {
    NEED_DUP(); REQUIRE(U(SetHandleInformation));
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL), d = NULL;
    CHECK(SetHandleInformation(h, HANDLE_FLAG_PROTECT_FROM_CLOSE, HANDLE_FLAG_PROTECT_FROM_CLOSE));
    EXPECT_EQ(DUP(SELF, h, SELF, &d, 0, FALSE, DUPLICATE_SAME_ACCESS), TRUE);
    EXPECT_EQ(ho_flags(d) & HANDLE_FLAG_PROTECT_FROM_CLOSE, 0);
    EXPECT_EQ(U(CloseHandle)(d), TRUE);
    SetHandleInformation(h, HANDLE_FLAG_PROTECT_FROM_CLOSE, 0);
    CloseHandle(h);
}
TEST(ho_dup_close_source) {
    NEED_DUP();
    HANDLE h = CreateEventA(NULL, TRUE, TRUE, NULL), d = NULL;
    SET_ERR(0);
    EXPECT_EQ(DUP(SELF, h, SELF, &d, 0, FALSE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE), TRUE);
    if (d == h) { t_note("target handle value reused the closed source slot"); CloseHandle(d); return; }
    EXPECT_EQ(ho_live(h), 0);
    SET_ERR(0);
    EXPECT_EQ(U(CloseHandle)(h), FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    EXPECT_EQ(WaitForSingleObject(d, 0), WAIT_OBJECT_0);
    EXPECT_EQ(ho_hcount(d), 1);
    CloseHandle(d);
}
TEST(ho_dup_close_source_null_target_pointer) {
    NEED_DUP();
    HANDLE h = CreateEventA(NULL, TRUE, TRUE, NULL);
    DWORD base = ho_count();
    SET_ERR(0);
    BOOL r = DUP(SELF, h, NULL, NULL, 0, FALSE, DUPLICATE_CLOSE_SOURCE);
    DWORD e = GetLastError();
    t_note("close-source with NULL target process+pointer: r=%d err=%lu", r, e);
    if (r) { EXPECT_EQ(ho_live(h), 0); EXPECT_EQ(ho_count(), base - 1); }
    else CloseHandle(h);
}
TEST(ho_dup_close_source_null_target_pointer_real_process) {
    NEED_DUP();
    HANDLE h = CreateEventA(NULL, TRUE, TRUE, NULL);
    DWORD base = ho_count();
    SET_ERR(0);
    BOOL r = DUP(SELF, h, SELF, NULL, 0, FALSE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE);
    DWORD e = GetLastError();
    t_note("close-source with NULL target pointer: r=%d err=%lu count delta=%ld", r, e, (long)ho_count() - (long)base);
    if (!r) CloseHandle(h);
    else if (ho_live(h)) CloseHandle(h);
}
TEST(ho_dup_close_source_protected_handle) {
    NEED_DUP(); REQUIRE(U(SetHandleInformation));
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL), d = NULL;
    SetHandleInformation(h, HANDLE_FLAG_PROTECT_FROM_CLOSE, HANDLE_FLAG_PROTECT_FROM_CLOSE);
    SET_ERR(0);
    BOOL r = DUP(SELF, h, SELF, &d, 0, FALSE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE);
    DWORD e = GetLastError();
    t_note("close-source of protected handle: r=%d err=%lu src_live=%d", r, e, ho_live(h));
    if (ho_live(h)) { SetHandleInformation(h, HANDLE_FLAG_PROTECT_FROM_CLOSE, 0); CloseHandle(h); }
    if (r) CloseHandle(d);
}
TEST(ho_dup_invalid_source_handle) {
    NEED_DUP();
    HANDLE d = (HANDLE)0x5555;
    HANDLE bad[] = { NULL, (HANDLE)0x12344, (HANDLE)0x7FFFFFFC };
    for (unsigned i = 0; i < 3; i++) {
        SET_ERR(0); d = (HANDLE)0x5555;
        EXPECT_EQ(DUP(SELF, bad[i], SELF, &d, 0, FALSE, DUPLICATE_SAME_ACCESS), FALSE);
        EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
}
TEST(ho_dup_failure_output_value) {
    NEED_DUP();
    HANDLE d = (HANDLE)0x5555;
    SET_ERR(0);
    EXPECT_EQ(DUP(SELF, NULL, SELF, &d, 0, FALSE, DUPLICATE_SAME_ACCESS), FALSE);
    t_note("target handle after failed dup = %p", d);
}
TEST(ho_dup_invalid_process_handles) {
    NEED_DUP();
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL), d = NULL;
    HANDLE badp[] = { NULL, (HANDLE)0x12344, h /* wrong type */ };
    for (unsigned i = 0; i < 3; i++) {
        SET_ERR(0);
        EXPECT_EQ(DUP(badp[i], h, SELF, &d, 0, FALSE, DUPLICATE_SAME_ACCESS), FALSE);
        EXPECT_ERR(ERROR_INVALID_HANDLE);
        SET_ERR(0);
        EXPECT_EQ(DUP(SELF, h, badp[i], &d, 0, FALSE, DUPLICATE_SAME_ACCESS), FALSE);
        /* a NULL target process without DUPLICATE_CLOSE_SOURCE is STATUS_INVALID_PARAMETER in ObDuplicateObject */
        EXPECT_ERR(badp[i] == NULL ? ERROR_INVALID_PARAMETER : ERROR_INVALID_HANDLE);
    }
    CloseHandle(h);
}
TEST(ho_dup_process_lacking_dup_handle_right) {
    NEED_DUP(); REQUIRE(U(OpenProcess));
    HANDLE lim = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    CHECK(lim != NULL); if (!lim) return;
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL), d = NULL;
    SET_ERR(0);
    EXPECT_EQ(DUP(lim, h, SELF, &d, 0, FALSE, DUPLICATE_SAME_ACCESS), FALSE);   /* source lacks PROCESS_DUP_HANDLE */
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    SET_ERR(0);
    EXPECT_EQ(DUP(SELF, h, lim, &d, 0, FALSE, DUPLICATE_SAME_ACCESS), FALSE);   /* target lacks it */
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(lim);
    HANDLE dh = OpenProcess(PROCESS_DUP_HANDLE, FALSE, GetCurrentProcessId());
    CHECK(dh != NULL);
    if (dh) {
        EXPECT_EQ(DUP(dh, h, dh, &d, 0, FALSE, DUPLICATE_SAME_ACCESS), TRUE);
        if (d) CloseHandle(d);
        CloseHandle(dh);
    }
    CloseHandle(h);
}
TEST(ho_dup_via_real_process_handle) {
    NEED_DUP(); REQUIRE(U(OpenProcess));
    HANDLE self = OpenProcess(PROCESS_DUP_HANDLE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    CHECK(self != NULL); if (!self) return;
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL), d = NULL;
    EXPECT_EQ(DUP(self, h, self, &d, 0, FALSE, DUPLICATE_SAME_ACCESS), TRUE);
    CHECK(SetEvent(d));
    EXPECT_EQ(WaitForSingleObject(h, 0), WAIT_OBJECT_0);
    CloseHandle(d); CloseHandle(h); CloseHandle(self);
}
TEST(ho_dup_pseudo_process_yields_real_handle) {
    NEED_DUP(); REQUIRE(U(GetProcessId)); REQUIRE(U(GetCurrentProcess));
    HANDLE p = NULL;
    EXPECT_EQ(U(GetCurrentProcess)(), HO_PSEUDO_PROC);
    EXPECT_EQ(DUP(SELF, SELF, SELF, &p, 0, FALSE, DUPLICATE_SAME_ACCESS), TRUE);
    CHECK(p != NULL && p != HO_PSEUDO_PROC);
    EXPECT_EQ(U(GetProcessId)(p), GetCurrentProcessId());
    EXPECT_EQ((ULONG_PTR)p & 3, 0);
    EXPECT_EQ(WaitForSingleObject(p, 0), WAIT_TIMEOUT);   /* this process is alive */
    DWORD code = 0; CHECK(GetExitCodeProcess(p, &code)); EXPECT_EQ(code, STILL_ACTIVE);
    CHECK(ho_access(p) & PROCESS_QUERY_LIMITED_INFORMATION);
    CloseHandle(p);
}
TEST(ho_dup_pseudo_process_explicit_access) {
    NEED_DUP();
    HANDLE p = NULL;
    EXPECT_EQ(DUP(SELF, SELF, SELF, &p, PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, 0), TRUE);
    EXPECT_EQ(ho_access(p), PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE);
    SET_ERR(0);
    DWORD code;
    CHECK(GetExitCodeProcess(p, &code));
    CloseHandle(p);
    EXPECT_EQ(DUP(SELF, SELF, SELF, &p, PROCESS_ALL_ACCESS, FALSE, 0), TRUE);
    CHECK((ho_access(p) & PROCESS_VM_READ) != 0);
    CloseHandle(p);
}
TEST(ho_dup_pseudo_thread_yields_real_handle) {
    NEED_DUP(); REQUIRE(U(GetThreadId));
    HANDLE t = NULL;
    EXPECT_EQ(DUP(SELF, GetCurrentThread(), SELF, &t, 0, FALSE, DUPLICATE_SAME_ACCESS), TRUE);
    CHECK(t != NULL && t != HO_PSEUDO_THREAD);
    EXPECT_EQ(U(GetThreadId)(t), GetCurrentThreadId());
    EXPECT_EQ(WaitForSingleObject(t, 0), WAIT_TIMEOUT);
    CloseHandle(t);
}
static DWORD WINAPI dup_thr_pseudo(LPVOID p) {
    HANDLE *out = p;
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), out, 0, FALSE, DUPLICATE_SAME_ACCESS);
    return GetCurrentThreadId();
}
TEST(ho_dup_pseudo_thread_is_per_calling_thread) {
    NEED_DUP(); REQUIRE(U(GetThreadId));
    HANDLE got = NULL; DWORD tid = 0;
    HANDLE th = CreateThread(NULL, 0, dup_thr_pseudo, &got, 0, &tid);
    WaitForSingleObject(th, 5000);
    DWORD code = 0; GetExitCodeThread(th, &code);
    CHECK(got != NULL);
    EXPECT_EQ(code, tid);
    EXPECT_EQ(U(GetThreadId)(got), tid);         /* the duplicate names the other thread, not the main thread */
    EXPECT_NE(U(GetThreadId)(got), GetCurrentThreadId());
    EXPECT_EQ(WaitForSingleObject(got, 0), WAIT_OBJECT_0);   /* that thread has exited */
    CloseHandle(got); CloseHandle(th);
}
TEST(ho_dup_thread_handle_rights) {
    NEED_DUP();
    HANDLE t = NULL;
    EXPECT_EQ(DUP(SELF, GetCurrentThread(), SELF, &t, THREAD_QUERY_LIMITED_INFORMATION, FALSE, 0), TRUE);
    EXPECT_EQ(ho_access(t), THREAD_QUERY_LIMITED_INFORMATION);
    SET_ERR(0);
    CHECK(!SetThreadPriority(t, THREAD_PRIORITY_NORMAL));   /* no THREAD_SET_INFORMATION */
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(t);
}
TEST(ho_dup_std_pseudo_codes_map_to_peb_handles) {
    NEED_DUP();
    struct { DWORD code; HANDLE pseudo; } tab[] = { { STD_INPUT_HANDLE, (HANDLE)(LONG_PTR)-10 }, { STD_OUTPUT_HANDLE, (HANDLE)(LONG_PTR)-11 }, { STD_ERROR_HANDLE, (HANDLE)(LONG_PTR)-12 } };
    for (int i = 0; i < 3; i++) {
        HANDLE real = GetStdHandle(tab[i].code);
        if (!real || real == INVALID_HANDLE_VALUE) continue;
        HANDLE d = NULL;
        SET_ERR(0);
        BOOL r = DUP(SELF, tab[i].pseudo, SELF, &d, 0, FALSE, DUPLICATE_SAME_ACCESS);
        CHECKF(r, "std %d", i);
        if (r) { EXPECT_EQ(GetFileType(d), GetFileType(real)); CloseHandle(d); }
    }
}
TEST(ho_dup_file_shares_file_pointer) {
    NEED_DUP();
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "dupfp.bin");
    HANDLE f = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL), d = NULL;
    EXPECT_EQ(DUP(SELF, f, SELF, &d, 0, FALSE, DUPLICATE_SAME_ACCESS), TRUE);
    DWORD n; WriteFile(f, "abcdefgh", 8, &n, NULL);
    EXPECT_EQ(SetFilePointer(d, 0, NULL, FILE_CURRENT), 8);   /* position is a property of the file object */
    SetFilePointer(d, 2, NULL, FILE_BEGIN);
    char b[4] = { 0 };
    ReadFile(f, b, 3, &n, NULL);
    EXPECT_EQ(n, 3); CHECK(!memcmp(b, "cde", 3));
    CloseHandle(f);
    SetFilePointer(d, 0, NULL, FILE_BEGIN);
    ReadFile(d, b, 2, &n, NULL); CHECK(!memcmp(b, "ab", 2));
    CloseHandle(d);
    DeleteFileA(p);
}
TEST(ho_dup_file_keeps_sharing_mode_locked) {
    NEED_DUP();
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "dupshare.bin");
    HANDLE f = CreateFileA(p, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL), d = NULL;
    DUP(SELF, f, SELF, &d, 0, FALSE, DUPLICATE_SAME_ACCESS);
    CloseHandle(f);
    HANDLE o = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(o == INVALID_HANDLE_VALUE);     /* the duplicate keeps the exclusive open alive */
    EXPECT_ERR(ERROR_SHARING_VIOLATION);
    CloseHandle(d);
    o = CreateFileA(p, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(o != INVALID_HANDLE_VALUE);
    CloseHandle(o); DeleteFileA(p);
}
TEST(ho_dup_mutex_ownership_shared) {
    NEED_DUP(); REQUIRE(U(ReleaseMutex));
    HANDLE m = CreateMutexA(NULL, TRUE, NULL), d = NULL;
    EXPECT_EQ(DUP(SELF, m, SELF, &d, 0, FALSE, DUPLICATE_SAME_ACCESS), TRUE);
    EXPECT_EQ(U(ReleaseMutex)(d), TRUE);   /* owned by this thread via the original */
    SET_ERR(0);
    EXPECT_EQ(U(ReleaseMutex)(m), FALSE);  /* no longer owned */
    EXPECT_ERR(ERROR_NOT_OWNER);
    CloseHandle(d); CloseHandle(m);
}
TEST(ho_dup_semaphore_count_shared) {
    NEED_DUP(); REQUIRE(U(ReleaseSemaphore));
    HANDLE s = CreateSemaphoreA(NULL, 0, 5, NULL), d = NULL;
    DUP(SELF, s, SELF, &d, 0, FALSE, DUPLICATE_SAME_ACCESS);
    LONG prev = -1;
    EXPECT_EQ(U(ReleaseSemaphore)(s, 2, &prev), TRUE); EXPECT_EQ(prev, 0);
    EXPECT_EQ(U(ReleaseSemaphore)(d, 1, &prev), TRUE); EXPECT_EQ(prev, 2);
    for (int i = 0; i < 3; i++) EXPECT_EQ(WaitForSingleObject(i & 1 ? s : d, 0), WAIT_OBJECT_0);
    EXPECT_EQ(WaitForSingleObject(d, 0), WAIT_TIMEOUT);
    CloseHandle(d); CloseHandle(s);
}
TEST(ho_dup_mapping_shares_section) {
    NEED_DUP();
    HANDLE m = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, NULL), d = NULL;
    DUP(SELF, m, SELF, &d, 0, FALSE, DUPLICATE_SAME_ACCESS);
    char *a = MapViewOfFile(m, FILE_MAP_WRITE, 0, 0, 0), *b = MapViewOfFile(d, FILE_MAP_WRITE, 0, 0, 0);
    CHECK(a && b && a != b);
    if (a && b) { strcpy(a, "hello"); EXPECT_STR(b, "hello"); }
    if (a) UnmapViewOfFile(a);
    if (b) UnmapViewOfFile(b);
    CloseHandle(m);
    b = MapViewOfFile(d, FILE_MAP_READ, 0, 0, 0);   /* still mappable after source closed */
    CHECK(b != NULL); if (b) UnmapViewOfFile(b);
    CloseHandle(d);
}
TEST(ho_dup_many_stress) {
    NEED_DUP(); NEED_HC();
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL);
    static HANDLE d[1000]; DWORD base = ho_count(); int ok = 0;
    for (int i = 0; i < 1000; i++) { d[i] = dup_simple(h); if (d[i]) ok++; }
    EXPECT_EQ(ok, 1000);
    EXPECT_EQ(ho_count(), base + 1000);
    EXPECT_EQ(ho_hcount(h), 1001);
    for (int i = 0; i < 1000; i++) if (d[i]) CloseHandle(d[i]);
    EXPECT_EQ(ho_count(), base);
    CloseHandle(h);
}
TEST(ho_dup_invalid_options) {
    NEED_DUP();
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL), d = NULL;
    DWORD opts[] = { 0x10, 0x100 };
    if (t_is_wine()) { t_skip("emulator server crashes on invalid options"); return; }
    for (int i = 0; i < 2; i++) {
        SET_ERR(0); d = NULL;
        BOOL r = DUP(SELF, h, SELF, &d, 0, FALSE, opts[i]);
        t_note("options 0x%lx: r=%d err=%lu", opts[i], r, GetLastError());
        if (r && d) CloseHandle(d);
    }
    CloseHandle(h);
}

/* ---- cross-process ---- */
/* child: reads 8 bytes (handle value) from stdin, signals that handle, exits 0 (or error code) */
CHILD(ho_dup_recv) {
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE); ULONG_PTR v = 0; DWORD n = 0;
    if (!ReadFile(in, &v, sizeof v, &n, NULL) || n != sizeof v) return 10;
    if (!SetEvent((HANDLE)v)) return 20 + (GetLastError() & 0xF);
    return 0;
}
TEST(ho_dup_into_child_process) {
    NEED_DUP();
    HANDLE r, w; SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    CHECK(CreatePipe(&r, &w, &sa, 0));
    SetHandleInformation(w, HANDLE_FLAG_INHERIT, 0);
    PROCESS_INFORMATION pi;
    HANDLE hp = t_spawn_child("ho_dup_recv", NULL, 0, r, GetStdHandle(STD_OUTPUT_HANDLE), GetStdHandle(STD_ERROR_HANDLE), TRUE, NULL, &pi);
    CHECK(hp != NULL); CloseHandle(r);
    if (!hp) { CloseHandle(w); return; }
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL), remote = NULL;
    SET_ERR(0);
    EXPECT_EQ(DUP(SELF, ev, hp, &remote, 0, FALSE, DUPLICATE_SAME_ACCESS), TRUE);
    CHECK(remote != NULL);
    CHECK(ho_hcount(ev) == 2);   /* both handles count against the one object */
    ULONG_PTR v = (ULONG_PTR)remote; DWORD n;
    WriteFile(w, &v, sizeof v, &n, NULL); CloseHandle(w);
    EXPECT_EQ(WaitForSingleObject(ev, 10000), WAIT_OBJECT_0);
    EXPECT_EQ(WaitForSingleObject(hp, 10000), WAIT_OBJECT_0);
    DWORD code = 99; GetExitCodeProcess(hp, &code); EXPECT_EQ(code, 0);
    CloseHandle(ev); CloseHandle(pi.hThread); CloseHandle(hp);
}
/* child: creates a signaled event, prints "<hex>\n" on stdout, waits for stdin EOF/byte, then reports whether the handle is still valid */
CHILD(ho_dup_send) {
    HANDLE ev = CreateEventA(NULL, TRUE, TRUE, NULL);
    char buf[64]; DWORD n; int len = snprintf(buf, sizeof buf, "%llx\n", (unsigned long long)(ULONG_PTR)ev);
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buf, len, &n, NULL);
    char c; ReadFile(GetStdHandle(STD_INPUT_HANDLE), &c, 1, &n, NULL);  /* parent releases us */
    DWORD f;
    return GetHandleInformation(ev, &f) ? 1 : 2;   /* 2 == the parent closed our handle */
}
static HANDLE spawn_sender(HANDLE *to_child, HANDLE *from_child, PROCESS_INFORMATION *pi) {
    HANDLE ir, iw, or_, ow; SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    CreatePipe(&ir, &iw, &sa, 0); CreatePipe(&or_, &ow, &sa, 0);
    SetHandleInformation(iw, HANDLE_FLAG_INHERIT, 0); SetHandleInformation(or_, HANDLE_FLAG_INHERIT, 0);
    HANDLE hp = t_spawn_child("ho_dup_send", NULL, 0, ir, ow, GetStdHandle(STD_ERROR_HANDLE), TRUE, NULL, pi);
    CloseHandle(ir); CloseHandle(ow);
    *to_child = iw; *from_child = or_;
    return hp;
}
static ULONG_PTR read_hex_line(HANDLE h) {
    char buf[64] = { 0 }; DWORD n = 0, tot = 0;
    while (tot < sizeof buf - 1 && ReadFile(h, buf + tot, 1, &n, NULL) && n == 1) { if (buf[tot] == '\n') break; tot++; }
    return (ULONG_PTR)strtoull(buf, NULL, 16);
}
TEST(ho_dup_from_child_process) {
    NEED_DUP();
    PROCESS_INFORMATION pi; HANDLE tc, fc;
    HANDLE hp = spawn_sender(&tc, &fc, &pi);
    CHECK(hp != NULL); if (!hp) { CloseHandle(tc); CloseHandle(fc); return; }
    ULONG_PTR v = read_hex_line(fc);
    CHECK(v != 0);
    HANDLE mine = NULL;
    SET_ERR(0);
    EXPECT_EQ(DUP(hp, (HANDLE)v, SELF, &mine, 0, FALSE, DUPLICATE_SAME_ACCESS), TRUE);
    CHECK(mine != NULL);
    EXPECT_EQ(WaitForSingleObject(mine, 0), WAIT_OBJECT_0);
    EXPECT_EQ(ho_hcount(mine), 2);
    /* bad handle value in the remote process */
    HANDLE junk = NULL;
    SET_ERR(0);
    EXPECT_EQ(DUP(hp, (HANDLE)0x7FFFFFC, SELF, &junk, 0, FALSE, DUPLICATE_SAME_ACCESS), FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    /* close the source in the child via CLOSE_SOURCE; the child then reports it */
    HANDLE mine2 = NULL;
    EXPECT_EQ(DUP(hp, (HANDLE)v, SELF, &mine2, 0, FALSE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE), TRUE);
    EXPECT_EQ(ho_hcount(mine), 2);        /* mine, mine2 (remote closed) */
    DWORD n; WriteFile(tc, "x", 1, &n, NULL);
    EXPECT_EQ(WaitForSingleObject(hp, 10000), WAIT_OBJECT_0);
    DWORD code = 0; GetExitCodeProcess(hp, &code); EXPECT_EQ(code, 2);
    CloseHandle(mine); if (mine2) CloseHandle(mine2);
    CloseHandle(tc); CloseHandle(fc); CloseHandle(pi.hThread); CloseHandle(hp);
}
TEST(ho_dup_from_child_without_dup_right) {
    NEED_DUP(); REQUIRE(U(OpenProcess));
    PROCESS_INFORMATION pi; HANDLE tc, fc;
    HANDLE hp = spawn_sender(&tc, &fc, &pi);
    CHECK(hp != NULL); if (!hp) { CloseHandle(tc); CloseHandle(fc); return; }
    ULONG_PTR v = read_hex_line(fc);
    HANDLE lim = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pi.dwProcessId), d = NULL;
    CHECK(lim != NULL);
    if (lim) {
        SET_ERR(0);
        EXPECT_EQ(DUP(lim, (HANDLE)v, SELF, &d, 0, FALSE, DUPLICATE_SAME_ACCESS), FALSE);
        EXPECT_ERR(ERROR_ACCESS_DENIED);
        CloseHandle(lim);
    }
    DWORD n; WriteFile(tc, "x", 1, &n, NULL);
    WaitForSingleObject(hp, 10000);
    DWORD code = 0; GetExitCodeProcess(hp, &code); EXPECT_EQ(code, 1);
    CloseHandle(tc); CloseHandle(fc); CloseHandle(pi.hThread); CloseHandle(hp);
}
TEST(ho_dup_pseudo_handles_with_child_target) {
    NEED_DUP();
    /* The pseudo process handle is always "the calling process", also when the target is another process:
       duplicating -1 as the *source* in our own process into the child yields a handle to *us* inside the child. */
    PROCESS_INFORMATION pi; HANDLE tc, fc;
    HANDLE hp = spawn_sender(&tc, &fc, &pi);
    if (!hp) { CloseHandle(tc); CloseHandle(fc); CHECK(0); return; }
    read_hex_line(fc);
    HANDLE remote = NULL;
    EXPECT_EQ(DUP(SELF, SELF, hp, &remote, PROCESS_QUERY_LIMITED_INFORMATION, FALSE, 0), TRUE);
    CHECK(remote != NULL);
    DWORD n; WriteFile(tc, "x", 1, &n, NULL);
    WaitForSingleObject(hp, 10000);
    CloseHandle(tc); CloseHandle(fc); CloseHandle(pi.hThread); CloseHandle(hp);
}
