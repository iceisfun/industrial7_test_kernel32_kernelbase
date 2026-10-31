#include "ho.h"

#define GHI U(GetHandleInformation)
#define SHI U(SetHandleInformation)
#define NEED_HI() do { REQUIRE(U(GetHandleInformation)); REQUIRE(U(SetHandleInformation)); } while (0)

TEST(ho_ghi_default_flags_zero) {
    NEED_HI();
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL); DWORD f = 0xFF;
    EXPECT_EQ(GHI(h, &f), TRUE);
    EXPECT_EQ(f, 0);
    CloseHandle(h);
}
TEST(ho_ghi_inheritable_flag) {
    NEED_HI();
    HANDLE h = ho_mkevent(TRUE, TRUE, FALSE); DWORD f = 0xFF;
    EXPECT_EQ(GHI(h, &f), TRUE);
    EXPECT_EQ(f, HANDLE_FLAG_INHERIT);
    CloseHandle(h);
}
TEST(ho_ghi_success_keeps_last_error) {
    NEED_HI();
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL); DWORD f;
    SET_ERR(0x3333);
    EXPECT_EQ(GHI(h, &f), TRUE);
    EXPECT_ERR(0x3333);
    SET_ERR(0x4444);
    EXPECT_EQ(SHI(h, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT), TRUE);
    EXPECT_ERR(0x4444);
    CloseHandle(h);
}
TEST(ho_ghi_invalid_handles) {
    NEED_HI();
    HANDLE bad[] = { NULL, (HANDLE)0x12344, (HANDLE)0x7FFFFFFC, (HANDLE)0xFFFFFFF0, (HANDLE)4 };
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        if (ho_live(bad[i])) continue;
        DWORD f = 0x77; SET_ERR(0);
        EXPECT_EQ(GHI(bad[i], &f), FALSE);
        EXPECT_ERR(ERROR_INVALID_HANDLE);
        EXPECT_EQ(f, 0x77);   /* untouched on failure */
    }
}
TEST(ho_ghi_closed_handle) {
    NEED_HI();
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL); DWORD f;
    CloseHandle(h);
    SET_ERR(0);
    EXPECT_EQ(GHI(h, &f), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0);
    EXPECT_EQ(SHI(h, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(ho_shi_inherit_toggle) {
    NEED_HI();
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL);
    EXPECT_EQ(SHI(h, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT), TRUE);
    EXPECT_EQ(ho_flags(h), HANDLE_FLAG_INHERIT);
    EXPECT_EQ(SHI(h, HANDLE_FLAG_INHERIT, 0), TRUE);
    EXPECT_EQ(ho_flags(h), 0);
    for (int i = 0; i < 5; i++) {
        SHI(h, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
        EXPECT_EQ(ho_flags(h), HANDLE_FLAG_INHERIT);
        SHI(h, HANDLE_FLAG_INHERIT, 0);
        EXPECT_EQ(ho_flags(h), 0);
    }
    CloseHandle(h);
}
TEST(ho_shi_protect_toggle) {
    NEED_HI();
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL);
    EXPECT_EQ(SHI(h, HANDLE_FLAG_PROTECT_FROM_CLOSE, HANDLE_FLAG_PROTECT_FROM_CLOSE), TRUE);
    EXPECT_EQ(ho_flags(h), HANDLE_FLAG_PROTECT_FROM_CLOSE);
    EXPECT_EQ(SHI(h, HANDLE_FLAG_PROTECT_FROM_CLOSE, 0), TRUE);
    EXPECT_EQ(ho_flags(h), 0);
    CloseHandle(h);
}
TEST(ho_shi_both_flags) {
    NEED_HI();
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL);
    DWORD both = HANDLE_FLAG_INHERIT | HANDLE_FLAG_PROTECT_FROM_CLOSE;
    EXPECT_EQ(SHI(h, both, both), TRUE);
    EXPECT_EQ(ho_flags(h), 3);
    EXPECT_EQ(SHI(h, HANDLE_FLAG_INHERIT, 0), TRUE);
    EXPECT_EQ(ho_flags(h), 2);                         /* mask limits what changes */
    EXPECT_EQ(SHI(h, both, 0), TRUE);
    EXPECT_EQ(ho_flags(h), 0);
    EXPECT_EQ(SHI(h, HANDLE_FLAG_PROTECT_FROM_CLOSE, both), TRUE);
    EXPECT_EQ(ho_flags(h), 2);                         /* inherit bit in flags ignored when not in mask */
    SHI(h, both, 0);
    CloseHandle(h);
}
TEST(ho_shi_mask_zero_changes_nothing) {
    NEED_HI();
    HANDLE h = ho_mkevent(TRUE, TRUE, FALSE);
    EXPECT_EQ(SHI(h, 0, 0), TRUE);
    EXPECT_EQ(ho_flags(h), HANDLE_FLAG_INHERIT);
    EXPECT_EQ(SHI(h, 0, 3), TRUE);
    EXPECT_EQ(ho_flags(h), HANDLE_FLAG_INHERIT);
    CloseHandle(h);
}
TEST(ho_shi_unknown_mask_bits_ignored) {
    NEED_HI();
    /* kernelbase only looks at mask bits 0 and 1 */
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL);
    DWORD masks[] = { 4, 0x8, 0x10, 0x100, 0xFFFFFFFC, 0x80000000u };
    for (unsigned i = 0; i < sizeof masks / sizeof *masks; i++) {
        SET_ERR(0);
        EXPECT_EQ(SHI(h, masks[i], 0xFFFFFFFFu), TRUE);
        EXPECT_EQ(ho_flags(h), 0);
    }
    EXPECT_EQ(SHI(h, 0xFFFFFFFFu, 0xFFFFFFFFu), TRUE);
    EXPECT_EQ(ho_flags(h), 3);
    SHI(h, 0xFFFFFFFFu, 0);
    CloseHandle(h);
}
TEST(ho_shi_invalid_handles) {
    NEED_HI();
    HANDLE bad[] = { NULL, (HANDLE)0x12344, (HANDLE)0x7FFFFFFC };
    for (unsigned i = 0; i < 3; i++) {
        SET_ERR(0);
        EXPECT_EQ(SHI(bad[i], HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT), FALSE);
        EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
}
TEST(ho_shi_protect_blocks_close) {
    NEED_HI(); REQUIRE(U(CloseHandle));
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL);
    SHI(h, HANDLE_FLAG_PROTECT_FROM_CLOSE, HANDLE_FLAG_PROTECT_FROM_CLOSE);
    SET_ERR(0);
    EXPECT_EQ(U(CloseHandle)(h), FALSE);
    t_note("CloseHandle on protected handle: err=%lu", GetLastError());
    EXPECT_EQ(ho_live(h), 1);
    EXPECT_EQ(SetEvent(h), TRUE);                      /* still fully usable */
    SHI(h, HANDLE_FLAG_PROTECT_FROM_CLOSE, 0);
    EXPECT_EQ(U(CloseHandle)(h), TRUE);
}
TEST(ho_shi_protect_blocks_close_repeatedly) {
    NEED_HI(); REQUIRE(U(CloseHandle));
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL);
    SHI(h, HANDLE_FLAG_PROTECT_FROM_CLOSE, HANDLE_FLAG_PROTECT_FROM_CLOSE);
    for (int i = 0; i < 5; i++) { SET_ERR(0); EXPECT_EQ(U(CloseHandle)(h), FALSE); }
    SHI(h, HANDLE_FLAG_PROTECT_FROM_CLOSE, 0);
    EXPECT_EQ(U(CloseHandle)(h), TRUE);
}
TEST(ho_shi_protect_stays_through_inherit_change) {
    NEED_HI();
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL);
    SHI(h, HANDLE_FLAG_PROTECT_FROM_CLOSE, HANDLE_FLAG_PROTECT_FROM_CLOSE);
    SHI(h, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    EXPECT_EQ(ho_flags(h), 3);
    SHI(h, HANDLE_FLAG_INHERIT, 0);
    EXPECT_EQ(ho_flags(h), 2);
    SHI(h, 3, 0);
    CloseHandle(h);
}
TEST(ho_shi_leaked_protected_handle_count) {
    NEED_HI(); NEED_HC();
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL);
    DWORD base = ho_count();
    SHI(h, HANDLE_FLAG_PROTECT_FROM_CLOSE, HANDLE_FLAG_PROTECT_FROM_CLOSE);
    U(CloseHandle)(h);
    EXPECT_EQ(ho_count(), base);   /* the protected handle is still open */
    SHI(h, 3, 0);
    CloseHandle(h);
    EXPECT_EQ(ho_count(), base - 1);
}
TEST(ho_shi_flags_are_per_handle) {
    NEED_HI(); REQUIRE(U(DuplicateHandle));
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL), d = NULL;
    DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS);
    SHI(h, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    EXPECT_EQ(ho_flags(h), 1);
    EXPECT_EQ(ho_flags(d), 0);
    CloseHandle(h); CloseHandle(d);
}

/* flags across object types */
typedef HANDLE (*mk_fn)(SECURITY_ATTRIBUTES *sa, HANDLE *aux);
static HANDLE mk_event(SECURITY_ATTRIBUTES *sa, HANDLE *a) { return CreateEventA(sa, TRUE, FALSE, NULL); }
static HANDLE mk_mutex(SECURITY_ATTRIBUTES *sa, HANDLE *a) { return CreateMutexA(sa, FALSE, NULL); }
static HANDLE mk_sem(SECURITY_ATTRIBUTES *sa, HANDLE *a) { return CreateSemaphoreA(sa, 0, 1, NULL); }
static HANDLE mk_timer(SECURITY_ATTRIBUTES *sa, HANDLE *a) { return CreateWaitableTimerA(sa, TRUE, NULL); }
static HANDLE mk_map(SECURITY_ATTRIBUTES *sa, HANDLE *a) { return CreateFileMappingA(INVALID_HANDLE_VALUE, sa, PAGE_READWRITE, 0, 4096, NULL); }
static HANDLE mk_job(SECURITY_ATTRIBUTES *sa, HANDLE *a) { return CreateJobObjectA(sa, NULL); }
static HANDLE mk_file(SECURITY_ATTRIBUTES *sa, HANDLE *a) {
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "flagfile.bin");
    return CreateFileA(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, sa, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, NULL);
}
static HANDLE mk_thread(SECURITY_ATTRIBUTES *sa, HANDLE *a) {
    DWORD WINAPI f(LPVOID p) { return 0; }
    return CreateThread(sa, 0, f, NULL, 0, NULL);
}
static HANDLE mk_dir(SECURITY_ATTRIBUTES *sa, HANDLE *a) {
    return CreateFileA(t_tmpdir(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, sa, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
}
static HANDLE mk_pipe_r(SECURITY_ATTRIBUTES *sa, HANDLE *a) { HANDLE r, w; if (!CreatePipe(&r, &w, sa, 0)) return NULL; *a = w; return r; }
static HANDLE mk_namedpipe(SECURITY_ATTRIBUTES *sa, HANDLE *a) {
    char n[100]; snprintf(n, sizeof n, "\\\\.\\pipe\\k32t_ho_flags_%lu", GetCurrentProcessId());
    return CreateNamedPipeA(n, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE, 1, 64, 64, 0, sa);
}
static HANDLE mk_iocp(SECURITY_ATTRIBUTES *sa, HANDLE *a) { return CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0); }
static const struct { const char *name; mk_fn fn; int honors_sa; } g_mk[] = {
    { "event", mk_event, 1 }, { "mutex", mk_mutex, 1 }, { "semaphore", mk_sem, 1 }, { "timer", mk_timer, 1 },
    { "mapping", mk_map, 1 }, { "job", mk_job, 1 }, { "file", mk_file, 1 }, { "thread", mk_thread, 1 },
    { "directory", mk_dir, 1 }, { "pipe", mk_pipe_r, 1 }, { "namedpipe", mk_namedpipe, 1 }, { "iocp", mk_iocp, 0 },
};
TEST(ho_ghi_inherit_from_security_attributes_table) {
    NEED_HI();
    for (unsigned i = 0; i < sizeof g_mk / sizeof *g_mk; i++) {
        for (int inh = 0; inh < 2; inh++) {
            SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, inh }; HANDLE aux = NULL;
            HANDLE h = g_mk[i].fn(&sa, &aux);
            CHECKF(h != NULL && h != INVALID_HANDLE_VALUE, "%s inh=%d", g_mk[i].name, inh);
            if (!h || h == INVALID_HANDLE_VALUE) continue;
            DWORD f = ho_flags(h);
            if (g_mk[i].honors_sa) CHECKF((f & 1) == (DWORD)inh, "%s inh=%d flags=%lx", g_mk[i].name, inh, f);
            else CHECKF(f == 0, "%s flags=%lx", g_mk[i].name, f);
            if (aux) { CHECKF((ho_flags(aux) & 1) == (DWORD)inh, "%s aux inh=%d", g_mk[i].name, inh); CloseHandle(aux); }
            CloseHandle(h);
        }
    }
}
TEST(ho_ghi_null_sa_is_not_inheritable_table) {
    NEED_HI();
    for (unsigned i = 0; i < sizeof g_mk / sizeof *g_mk; i++) {
        HANDLE aux = NULL; HANDLE h = g_mk[i].fn(NULL, &aux);
        CHECKF(h != NULL && h != INVALID_HANDLE_VALUE, "%s", g_mk[i].name);
        if (!h || h == INVALID_HANDLE_VALUE) continue;
        EXPECT_EQ(ho_flags(h), 0);
        if (aux) { EXPECT_EQ(ho_flags(aux), 0); CloseHandle(aux); }
        CloseHandle(h);
    }
}
TEST(ho_shi_works_on_all_types_table) {
    NEED_HI();
    for (unsigned i = 0; i < sizeof g_mk / sizeof *g_mk; i++) {
        HANDLE aux = NULL; HANDLE h = g_mk[i].fn(NULL, &aux);
        if (!h || h == INVALID_HANDLE_VALUE) { CHECKF(0, "%s create", g_mk[i].name); continue; }
        CHECKF(SHI(h, 3, 1), "%s", g_mk[i].name);
        CHECKF(ho_flags(h) == 1, "%s flags=%lx", g_mk[i].name, ho_flags(h));
        CHECKF(SHI(h, 3, 2), "%s", g_mk[i].name);
        CHECKF(ho_flags(h) == 2, "%s", g_mk[i].name);
        SHI(h, 3, 0);
        if (aux) CloseHandle(aux);
        CloseHandle(h);
    }
}
TEST(ho_ghi_open_calls_honor_inherit_arg) {
    NEED_HI(); REQUIRE(U(OpenProcess)); REQUIRE(U(OpenThread)); REQUIRE(U(OpenEventA));
    for (int inh = 0; inh < 2; inh++) {
        HANDLE p = U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION, inh, GetCurrentProcessId());
        CHECK(p != NULL);
        if (p) { EXPECT_EQ(ho_flags(p), (DWORD)inh); CloseHandle(p); }
        HANDLE t = U(OpenThread)(THREAD_QUERY_LIMITED_INFORMATION, inh, GetCurrentThreadId());
        CHECK(t != NULL);
        if (t) { EXPECT_EQ(ho_flags(t), (DWORD)inh); CloseHandle(t); }
        char n[80]; ho_uniq(n, sizeof n, "ghi");
        HANDLE e = CreateEventA(NULL, TRUE, FALSE, n);
        HANDLE o = U(OpenEventA)(SYNCHRONIZE, inh, n);
        CHECK(o != NULL);
        if (o) { EXPECT_EQ(ho_flags(o), (DWORD)inh); CloseHandle(o); }
        CloseHandle(e);
    }
}
TEST(ho_ghi_createfile_sa_without_handle_inherit) {
    NEED_HI();
    /* a SECURITY_ATTRIBUTES with a null descriptor and bInheritHandle FALSE gives a non-inheritable handle */
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, FALSE };
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "sa0.bin");
    HANDLE h = CreateFileA(p, GENERIC_WRITE, 0, &sa, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    EXPECT_EQ(ho_flags(h), 0);
    CloseHandle(h);
}

/* ---- pseudo handles ---- */
TEST(ho_pseudo_values) {
    REQUIRE(U(GetCurrentProcess)); REQUIRE(U(GetCurrentThread));
    EXPECT_EQ(U(GetCurrentProcess)(), HO_PSEUDO_PROC);
    EXPECT_EQ(U(GetCurrentThread)(), HO_PSEUDO_THREAD);
    for (int i = 0; i < 3; i++) { EXPECT_EQ(U(GetCurrentProcess)(), (HANDLE)(LONG_PTR)-1); EXPECT_EQ(U(GetCurrentThread)(), (HANDLE)(LONG_PTR)-2); }
}
static HANDLE g_thr_pseudo;
static DWORD WINAPI pseudo_thread(LPVOID p) { g_thr_pseudo = GetCurrentThread(); return 0; }
TEST(ho_pseudo_thread_same_value_in_other_thread) {
    REQUIRE(U(GetCurrentThread));
    HANDLE t = CreateThread(NULL, 0, pseudo_thread, NULL, 0, NULL);
    WaitForSingleObject(t, 5000); CloseHandle(t);
    EXPECT_EQ(g_thr_pseudo, HO_PSEUDO_THREAD);
}
TEST(ho_pseudo_process_usable_apis) {
    REQUIRE(U(GetProcessId)); REQUIRE(U(GetThreadId)); REQUIRE(U(GetProcessHandleCount));
    EXPECT_EQ(U(GetProcessId)(HO_PSEUDO_PROC), GetCurrentProcessId());
    EXPECT_EQ(U(GetThreadId)(HO_PSEUDO_THREAD), GetCurrentThreadId());
    DWORD n = 0; EXPECT_EQ(U(GetProcessHandleCount)(HO_PSEUDO_PROC, &n), TRUE); CHECK(n > 0 || t_is_wine());
    DWORD code = 0; CHECK(GetExitCodeProcess(HO_PSEUDO_PROC, &code)); EXPECT_EQ(code, STILL_ACTIVE);
    CHECK(GetExitCodeThread(HO_PSEUDO_THREAD, &code)); EXPECT_EQ(code, STILL_ACTIVE);
}
TEST(ho_pseudo_ghi_shi_behaviour) {
    NEED_HI();
    DWORD f = 0xAA; SET_ERR(0);
    BOOL r1 = GHI(HO_PSEUDO_PROC, &f); DWORD e1 = GetLastError();
    t_note("GetHandleInformation(-1): r=%d f=%lx err=%lu", r1, f, e1);
    f = 0xAA; SET_ERR(0);
    BOOL r2 = GHI(HO_PSEUDO_THREAD, &f); DWORD e2 = GetLastError();
    t_note("GetHandleInformation(-2): r=%d f=%lx err=%lu", r2, f, e2);
    SET_ERR(0);
    BOOL r3 = SHI(HO_PSEUDO_PROC, HANDLE_FLAG_INHERIT, 0); DWORD e3 = GetLastError();
    t_note("SetHandleInformation(-1, inherit, 0): r=%d err=%lu", r3, e3);
    SET_ERR(0);
    BOOL r4 = SHI(HO_PSEUDO_THREAD, HANDLE_FLAG_PROTECT_FROM_CLOSE, 0); DWORD e4 = GetLastError();
    t_note("SetHandleInformation(-2, protect, 0): r=%d err=%lu", r4, e4);
    EXPECT_EQ(r1, r2);
}
TEST(ho_pseudo_std_codes_map_to_stdhandles) {
    NEED_HI();
    struct { DWORD code; HANDLE pseudo; } tab[] = { { STD_INPUT_HANDLE, (HANDLE)(LONG_PTR)-10 }, { STD_OUTPUT_HANDLE, (HANDLE)(LONG_PTR)-11 }, { STD_ERROR_HANDLE, (HANDLE)(LONG_PTR)-12 } };
    for (int i = 0; i < 3; i++) {
        HANDLE real = GetStdHandle(tab[i].code);
        if (!real || real == INVALID_HANDLE_VALUE) continue;
        DWORD fr = 0, fp = 0xAA;
        CHECK(GHI(real, &fr));
        SET_ERR(0);
        BOOL r = GHI(tab[i].pseudo, &fp);
        CHECKF(r, "std code %d err=%lu", i, GetLastError());
        if (r) EXPECT_EQ(fp, fr);
    }
}
TEST(ho_pseudo_std_set_inherit_roundtrip) {
    NEED_HI();
    HANDLE real = GetStdHandle(STD_ERROR_HANDLE);
    if (!real || real == INVALID_HANDLE_VALUE) { t_skip("no stderr"); return; }
    DWORD orig = 0; CHECK(GHI(real, &orig)); orig &= HANDLE_FLAG_INHERIT;
    HANDLE ps = (HANDLE)(LONG_PTR)-12;
    DWORD flip = orig ? 0 : HANDLE_FLAG_INHERIT;
    if (SHI(ps, HANDLE_FLAG_INHERIT, flip)) {
        DWORD now = 0; GHI(real, &now);
        EXPECT_EQ(now & HANDLE_FLAG_INHERIT, flip);
    } else t_note("SetHandleInformation(-12) failed err=%lu", GetLastError());
    SHI(real, HANDLE_FLAG_INHERIT, orig);          /* restore */
    DWORD fin = 0; GHI(real, &fin);
    EXPECT_EQ(fin & HANDLE_FLAG_INHERIT, orig);
}
TEST(ho_pseudo_token_handles_note) {
    REQUIRE(U(DuplicateHandle)); REQUIRE(U(CompareObjectHandles));
    HANDLE real = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &real)) { t_skip("OpenProcessToken failed"); return; }
    HANDLE ptok = (HANDLE)(LONG_PTR)-4, d = NULL;
    SET_ERR(0);
    BOOL r = DuplicateHandle(GetCurrentProcess(), ptok, GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS);
    t_note("DuplicateHandle(process token pseudo -4): r=%d err=%lu", r, GetLastError());
    if (r) CloseHandle(d);
    SET_ERR(0);
    BOOL c = U(CompareObjectHandles)(ptok, real);
    t_note("CompareObjectHandles(-4, realtoken)=%d err=%lu", c, GetLastError());
    DWORD len = 0; SET_ERR(0);
    BOOL g = GetTokenInformation(ptok, TokenStatistics, NULL, 0, &len);
    t_note("GetTokenInformation(-4): r=%d err=%lu len=%lu", g, GetLastError(), len);
    CloseHandle(real);
}

/* ---- CompareObjectHandles ---- */
#define CMP U(CompareObjectHandles)
TEST(ho_cmp_same_handle) {
    REQUIRE(CMP);
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0x5151);
    EXPECT_EQ(CMP(h, h), TRUE);
    EXPECT_ERR(0x5151);
    CloseHandle(h);
}
TEST(ho_cmp_duplicate_equal) {
    REQUIRE(CMP);
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL), d = NULL;
    DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(), &d, SYNCHRONIZE, FALSE, 0);
    EXPECT_EQ(CMP(h, d), TRUE);
    EXPECT_EQ(CMP(d, h), TRUE);
    CloseHandle(h); CloseHandle(d);
}
TEST(ho_cmp_different_objects) {
    REQUIRE(CMP);
    HANDLE a = CreateEventA(NULL, TRUE, FALSE, NULL), b = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0x5151);
    EXPECT_EQ(CMP(a, b), FALSE);
    EXPECT_ERR(ERROR_NOT_SAME_OBJECT);   /* STATUS_NOT_SAME_OBJECT is mapped through BaseSetLastNTError */
    CloseHandle(a); CloseHandle(b);
}
TEST(ho_cmp_different_types) {
    REQUIRE(CMP);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL), m = CreateMutexA(NULL, FALSE, NULL), s = CreateSemaphoreA(NULL, 0, 1, NULL);
    EXPECT_EQ(CMP(e, m), FALSE); EXPECT_EQ(CMP(m, s), FALSE); EXPECT_EQ(CMP(s, e), FALSE);
    CloseHandle(e); CloseHandle(m); CloseHandle(s);
}
TEST(ho_cmp_invalid_handles) {
    REQUIRE(CMP);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL), bad = (HANDLE)0x7FFFFFFC;
    SET_ERR(0); EXPECT_EQ(CMP(NULL, e), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(CMP(e, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(CMP(NULL, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(CMP(bad, e), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(CMP(e, bad), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(CMP(bad, bad), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(e);
}
TEST(ho_cmp_closed_handle) {
    REQUIRE(CMP);
    HANDLE a = CreateEventA(NULL, TRUE, FALSE, NULL), b = CreateEventA(NULL, TRUE, FALSE, NULL);
    CloseHandle(a);
    SET_ERR(0); EXPECT_EQ(CMP(a, b), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(b);
}
TEST(ho_cmp_pseudo_process_vs_real) {
    REQUIRE(CMP); REQUIRE(U(OpenProcess));
    HANDLE r = U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId()), d = NULL;
    CHECK(r != NULL);
    DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS);
    SET_ERR(0);
    EXPECT_EQ(CMP(GetCurrentProcess(), d), TRUE);
    EXPECT_EQ(CMP(d, GetCurrentProcess()), TRUE);
    EXPECT_EQ(CMP(GetCurrentProcess(), GetCurrentProcess()), TRUE);
    if (r) { EXPECT_EQ(CMP(GetCurrentProcess(), r), TRUE); EXPECT_EQ(CMP(r, d), TRUE); }
    CloseHandle(d); if (r) CloseHandle(r);
}
TEST(ho_cmp_pseudo_thread_vs_real) {
    REQUIRE(CMP); REQUIRE(U(OpenThread));
    HANDLE r = U(OpenThread)(THREAD_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentThreadId()), d = NULL;
    CHECK(r != NULL);
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS);
    EXPECT_EQ(CMP(GetCurrentThread(), d), TRUE);
    EXPECT_EQ(CMP(GetCurrentThread(), GetCurrentThread()), TRUE);
    if (r) { EXPECT_EQ(CMP(GetCurrentThread(), r), TRUE); EXPECT_EQ(CMP(r, d), TRUE); }
    CloseHandle(d); if (r) CloseHandle(r);
}
TEST(ho_cmp_process_vs_thread_pseudo) {
    REQUIRE(CMP);
    SET_ERR(0);
    EXPECT_EQ(CMP(GetCurrentProcess(), GetCurrentThread()), FALSE);
    EXPECT_EQ(CMP(GetCurrentThread(), GetCurrentProcess()), FALSE);
}
TEST(ho_cmp_two_openprocess_same_pid) {
    REQUIRE(CMP); REQUIRE(U(OpenProcess));
    HANDLE a = U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    HANDLE b = U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, GetCurrentProcessId());
    CHECK(a && b && a != b);
    EXPECT_EQ(CMP(a, b), TRUE);       /* different handles and rights, same process object */
    CloseHandle(a); CloseHandle(b);
}
TEST(ho_cmp_different_access_same_object) {
    REQUIRE(CMP);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL), l = NULL;
    DuplicateHandle(GetCurrentProcess(), e, GetCurrentProcess(), &l, SYNCHRONIZE, FALSE, 0);
    EXPECT_EQ(CMP(e, l), TRUE);
    CloseHandle(e); CloseHandle(l);
}
TEST(ho_cmp_named_object_opened_twice) {
    REQUIRE(CMP); REQUIRE(U(OpenEventA));
    char n[80]; ho_uniq(n, sizeof n, "cmp");
    HANDLE a = CreateEventA(NULL, TRUE, FALSE, n), b = U(OpenEventA)(SYNCHRONIZE, FALSE, n), c = CreateEventA(NULL, TRUE, FALSE, n);
    CHECK(a && b && c);
    EXPECT_EQ(GetLastError(), ERROR_ALREADY_EXISTS);
    EXPECT_EQ(CMP(a, b), TRUE); EXPECT_EQ(CMP(a, c), TRUE); EXPECT_EQ(CMP(b, c), TRUE);
    CloseHandle(a); CloseHandle(b); CloseHandle(c);
}
TEST(ho_cmp_files_same_path_distinct_file_objects) {
    REQUIRE(CMP);
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "cmpfile.bin");
    HANDLE a = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, NULL);
    HANDLE b = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL), d = NULL;
    DuplicateHandle(GetCurrentProcess(), a, GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS);
    EXPECT_EQ(CMP(a, b), FALSE);      /* two opens are two file objects */
    EXPECT_EQ(CMP(a, d), TRUE);
    CloseHandle(a); CloseHandle(b); CloseHandle(d); DeleteFileA(p);
}
TEST(ho_cmp_pipe_ends_differ) {
    REQUIRE(CMP);
    HANDLE r, w; CreatePipe(&r, &w, NULL, 0);
    EXPECT_EQ(CMP(r, w), FALSE);
    EXPECT_EQ(CMP(r, r), TRUE);
    CloseHandle(r); CloseHandle(w);
}
TEST(ho_cmp_child_process_handles) {
    REQUIRE(CMP); REQUIRE(U(OpenProcess));
    HANDLE r, w; SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, TRUE };
    CreatePipe(&r, &w, &sa, 0); SetHandleInformation(w, HANDLE_FLAG_INHERIT, 0);
    PROCESS_INFORMATION pi;
    HANDLE hp = t_spawn_child("ho_wait_stdin", NULL, 0, r, GetStdHandle(STD_OUTPUT_HANDLE), GetStdHandle(STD_ERROR_HANDLE), TRUE, NULL, &pi);
    CloseHandle(r);
    CHECK(hp != NULL);
    if (hp) {
        HANDLE o = U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pi.dwProcessId);
        CHECK(o != NULL);
        if (o) { EXPECT_EQ(CMP(hp, o), TRUE); EXPECT_EQ(CMP(o, GetCurrentProcess()), FALSE); CloseHandle(o); }
        EXPECT_EQ(CMP(hp, pi.hThread), FALSE);
        CloseHandle(w);   /* EOF: child exits */
        WaitForSingleObject(hp, 10000);
        CloseHandle(pi.hThread); CloseHandle(hp);
    } else CloseHandle(w);
}
CHILD(ho_wait_stdin) { char c; DWORD n; ReadFile(GetStdHandle(STD_INPUT_HANDLE), &c, 1, &n, NULL); return 0; }

/* ---- process handle count ---- */
#define GPHC U(GetProcessHandleCount)
TEST(ho_hc_event_delta) {
    NEED_HC();
    DWORD base = ho_count();
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL);
    EXPECT_EQ(ho_count(), base + 1);
    CloseHandle(h);
    EXPECT_EQ(ho_count(), base);
}
TEST(ho_hc_per_type_delta) {
    NEED_HC();
    struct { const char *n; mk_fn f; int extra; } tab[] = {
        { "event", mk_event, 0 }, { "mutex", mk_mutex, 0 }, { "sem", mk_sem, 0 }, { "timer", mk_timer, 0 }, { "mapping", mk_map, 0 },
        { "job", mk_job, 0 }, { "file", mk_file, 0 }, { "dir", mk_dir, 0 }, { "pipe", mk_pipe_r, 1 }, { "iocp", mk_iocp, 0 },
    };
    for (unsigned i = 0; i < sizeof tab / sizeof *tab; i++) {
        HANDLE aux = NULL; DWORD base = ho_count();
        HANDLE h = tab[i].f(NULL, &aux);
        CHECKF(h != NULL && h != INVALID_HANDLE_VALUE, "%s", tab[i].n);
        DWORD now = ho_count();
        CHECKF(now == base + 1 + tab[i].extra, "%s delta=%ld", tab[i].n, (long)now - (long)base);
        if (aux) CloseHandle(aux);
        CloseHandle(h);
        CHECKF(ho_count() == base, "%s after close", tab[i].n);
    }
}
TEST(ho_hc_thread_delta) {
    NEED_HC();
    DWORD base = ho_count();
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    DWORD WINAPI w(LPVOID p) { WaitForSingleObject((HANDLE)p, 5000); return 0; }
    HANDLE t = CreateThread(NULL, 0, w, ev, 0, NULL);
    EXPECT_EQ(ho_count(), base + 2);        /* event + thread handle */
    SetEvent(ev); WaitForSingleObject(t, 5000);
    EXPECT_EQ(ho_count(), base + 2);        /* an exited thread's handle still counts */
    CloseHandle(t); CloseHandle(ev);
    EXPECT_EQ(ho_count(), base);
}
TEST(ho_hc_500_events) {
    NEED_HC();
    static HANDLE hs[500]; DWORD base = ho_count();
    for (int i = 0; i < 500; i++) { hs[i] = CreateEventA(NULL, TRUE, FALSE, NULL); if (i % 100 == 99) EXPECT_EQ(ho_count(), base + i + 1); }
    EXPECT_EQ(ho_count(), base + 500);
    for (int i = 0; i < 500; i++) { CloseHandle(hs[i]); }
    EXPECT_EQ(ho_count(), base);
}
TEST(ho_hc_close_minus_one_each) {
    NEED_HC();
    HANDLE hs[50]; for (int i = 0; i < 50; i++) hs[i] = CreateSemaphoreA(NULL, 0, 1, NULL);
    DWORD c = ho_count();
    for (int i = 0; i < 50; i++) { U(CloseHandle)(hs[i]); EXPECT_EQ(ho_count(), c - 1 - i); }
}
TEST(ho_hc_failed_close_no_change) {
    NEED_HC(); REQUIRE(U(CloseHandle));
    DWORD base = ho_count();
    U(CloseHandle)((HANDLE)0x12344); U(CloseHandle)(NULL);
    EXPECT_EQ(ho_count(), base);
}
TEST(ho_hc_failed_create_no_leak) {
    NEED_HC();
    DWORD base = ho_count();
    char n[80]; ho_uniq(n, sizeof n, "hcf");
    HANDLE m = CreateMutexA(NULL, FALSE, n);
    DWORD b1 = ho_count();
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, n);   /* type collision */
    CHECK(e == NULL);
    EXPECT_EQ(ho_count(), b1);
    HANDLE s = CreateSemaphoreA(NULL, 2, 1, NULL);
    CHECK(s == NULL);
    EXPECT_EQ(ho_count(), b1);
    CloseHandle(m);
    EXPECT_EQ(ho_count(), base);
}
TEST(ho_hc_dup_open_deltas) {
    NEED_HC(); REQUIRE(U(OpenProcess));
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL), d;
    DWORD base = ho_count();
    DuplicateHandle(GetCurrentProcess(), e, GetCurrentProcess(), &d, 0, FALSE, DUPLICATE_SAME_ACCESS);
    EXPECT_EQ(ho_count(), base + 1);
    HANDLE p = U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    EXPECT_EQ(ho_count(), base + 2);
    HANDLE t = U(OpenThread)(THREAD_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentThreadId());
    EXPECT_EQ(ho_count(), base + 3);
    CloseHandle(p); CloseHandle(t); CloseHandle(d); CloseHandle(e);
    EXPECT_EQ(ho_count(), base - 1);
}
TEST(ho_hc_pseudo_handle_no_count) {
    NEED_HC();
    DWORD base = ho_count();
    for (int i = 0; i < 100; i++) { GetCurrentProcess(); GetCurrentThread(); }
    EXPECT_EQ(ho_count(), base);
}
TEST(ho_hc_invalid_handles) {
    NEED_HC();
    DWORD n = 0x99;
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    SET_ERR(0); EXPECT_EQ(GPHC(NULL, &n), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(GPHC((HANDLE)0x12344, &n), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(GPHC(e, &n), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);   /* not a process */
    EXPECT_EQ(n, 0x99);
    CloseHandle(e);
}
TEST(ho_hc_access_rights) {
    NEED_HC(); REQUIRE(U(OpenProcess));
    HANDLE p = U(OpenProcess)(SYNCHRONIZE, FALSE, GetCurrentProcessId()); DWORD n = 0;
    CHECK(p != NULL);
    if (p) { SET_ERR(0); EXPECT_EQ(GPHC(p, &n), FALSE); EXPECT_ERR(ERROR_ACCESS_DENIED); CloseHandle(p); }
    p = U(OpenProcess)(PROCESS_QUERY_INFORMATION, FALSE, GetCurrentProcessId());
    CHECK(p != NULL);
    if (p) { EXPECT_EQ(GPHC(p, &n), TRUE); CHECK(n > 0); CloseHandle(p); }
    p = U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    if (p) { SET_ERR(0); BOOL r = GPHC(p, &n); t_note("limited-rights GetProcessHandleCount=%d err=%lu", r, GetLastError()); CloseHandle(p); }
}
TEST(ho_hc_matches_self_dup_lookup) {
    NEED_HC();
    DWORD a = 0, b = 0;
    HANDLE h = NULL;
    DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(), &h, 0, FALSE, DUPLICATE_SAME_ACCESS);
    CHECK(GPHC(GetCurrentProcess(), &a)); CHECK(GPHC(h, &b));
    EXPECT_EQ(b, a);
    CloseHandle(h);
}
CHILD(ho_idle) { Sleep(3000); return 0; }
TEST(ho_hc_child_process_plausible) {
    NEED_HC();
    PROCESS_INFORMATION pi; HANDLE hp = t_spawn_child("ho_idle", NULL, 0, NULL, NULL, NULL, FALSE, NULL, &pi);
    CHECK(hp != NULL); if (!hp) return;
    DWORD n = 0; EXPECT_EQ(GPHC(hp, &n), TRUE);
    CHECKF(n >= 2 && n < 500, "child handle count %lu", n);
    TerminateProcess(hp, 0); WaitForSingleObject(hp, 5000);
    CloseHandle(pi.hThread); CloseHandle(hp);
}

/* ---- handle value properties ---- */
TEST(ho_hv_multiple_of_four_and_nonzero) {
    HANDLE hs[200];
    for (int i = 0; i < 200; i++) hs[i] = CreateEventA(NULL, TRUE, FALSE, NULL);
    for (int i = 0; i < 200; i++) {
        CHECKF(hs[i] != NULL && hs[i] != INVALID_HANDLE_VALUE, "i=%d", i);
        CHECKF(((ULONG_PTR)hs[i] & 3) == 0, "i=%d h=%p", i, hs[i]);
        CHECKF(((ULONG_PTR)hs[i] >> 32) == 0, "upper dword set: %p", hs[i]);
        CHECKF((ULONG_PTR)hs[i] >= 4, "i=%d h=%p", i, hs[i]);
    }
    for (int i = 0; i < 200; i++) for (int j = i + 1; j < 200; j++) if (hs[i] == hs[j]) CHECKF(0, "dup value %p", hs[i]);
    for (int i = 0; i < 200; i++) CloseHandle(hs[i]);
}
TEST(ho_hv_small_values) {
    HANDLE hs[100];
    for (int i = 0; i < 100; i++) hs[i] = CreateEventA(NULL, TRUE, FALSE, NULL);
    for (int i = 0; i < 100; i++) CHECKF((ULONG_PTR)hs[i] < 0x10000, "h=%p", hs[i]);
    for (int i = 0; i < 100; i++) CloseHandle(hs[i]);
}
TEST(ho_hv_reuse_after_close) {
    int reused = 0;
    for (int i = 0; i < 50; i++) {
        HANDLE a = CreateEventA(NULL, TRUE, FALSE, NULL); CloseHandle(a);
        HANDLE b = CreateEventA(NULL, TRUE, FALSE, NULL);
        if (a == b) reused++;
        CloseHandle(b);
    }
    t_note("handle value reused immediately in %d/50 iterations", reused);
    CHECK(reused > 0);
}
TEST(ho_hv_all_types_valid_shape) {
    for (unsigned i = 0; i < sizeof g_mk / sizeof *g_mk; i++) {
        HANDLE aux = NULL; HANDLE h = g_mk[i].fn(NULL, &aux);
        CHECKF(h && h != INVALID_HANDLE_VALUE && ((ULONG_PTR)h & 3) == 0, "%s h=%p", g_mk[i].name, h);
        if (aux) CloseHandle(aux);
        if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }
}
TEST(ho_sethandlecount_returns_arg) {
    REQUIRE(U(SetHandleCount));
    UINT vals[] = { 0, 1, 20, 255, 256, 1000, 0x10000, 0xFFFFFFFFu };
    for (unsigned i = 0; i < sizeof vals / sizeof *vals; i++) {
        DWORD base = ho_count();
        UINT r = U(SetHandleCount)(vals[i]);
        CHECKF(r == vals[i], "arg=%u got=%u", vals[i], r);
        EXPECT_EQ(ho_count(), base);
    }
}
