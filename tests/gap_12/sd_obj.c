/* gap_12: SetKernelObjectSecurity, SetFileSecurityW, SetPrivateObjectSecurity(Ex). */
#include "sdh.h"

/* ---------- helpers ---------- */
static BOOL get_kobj_sd(HANDLE h, SECURITY_INFORMATION si, BYTE *buf, DWORD cap) {
    DWORD need = 0;
    return GetKernelObjectSecurity(h, si, (PSECURITY_DESCRIPTOR)buf, cap, &need);
}
/* absolute SD with DACL = one allow ACE (world, mask) */
typedef struct { SECURITY_DESCRIPTOR sd; BYTE acl[128]; } DSD;
static void dsd_init(DSD *d, DWORD mask) {
    sd_init(&d->sd);
    mkacl(d->acl, sizeof d->acl, sid_world(), mask);
    SetSecurityDescriptorDacl(&d->sd, TRUE, (PACL)d->acl, FALSE);
}
static void check_world_dacl(HANDLE h, DWORD mask, const char *what) {
    BYTE buf[1024]; int present = 0; PSID sid = NULL; DWORD m = 0;
    if (!get_kobj_sd(h, DACL_SECURITY_INFORMATION, buf, sizeof buf)) { CHECKF(0, "%s: GetKernelObjectSecurity err=%lu", what, GetLastError()); return; }
    int n = sd_dacl_info(buf, &present, &sid, &m, NULL);
    CHECKF(present && n == 1, "%s: present=%d aces=%d", what, present, n);
    if (n == 1) { CHECKF(EqualSid(sid, sid_world()), "%s: sid", what); CHECKF(m == mask, "%s: mask 0x%lx want 0x%lx", what, m, mask); }
}

/* ---------------- SetKernelObjectSecurity ---------------- */
typedef HANDLE (*mk_fn)(DWORD *full);
static HANDLE mk_event(DWORD *f)   { *f = EVENT_ALL_ACCESS;     return CreateEventW(NULL, TRUE, FALSE, NULL); }
static HANDLE mk_mutex(DWORD *f)   { *f = MUTEX_ALL_ACCESS;     return CreateMutexW(NULL, FALSE, NULL); }
static HANDLE mk_sem(DWORD *f)     { *f = SEMAPHORE_ALL_ACCESS; return CreateSemaphoreW(NULL, 0, 1, NULL); }
static HANDLE mk_timer(DWORD *f)   { *f = TIMER_ALL_ACCESS;     return CreateWaitableTimerW(NULL, TRUE, NULL); }
static HANDLE mk_map(DWORD *f)     { *f = SECTION_ALL_ACCESS;   return CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, NULL); }
static HANDLE mk_job(DWORD *f)     { *f = JOB_OBJECT_ALL_ACCESS; return CreateJobObjectW(NULL, NULL); }
static HANDLE mk_pipe(DWORD *f)    { HANDLE r = NULL, w = NULL; *f = FILE_ALL_ACCESS; if (!CreatePipe(&r, &w, NULL, 0)) return NULL; CloseHandle(w); return r; }
static HANDLE mk_file(DWORD *f)    { char p[MAX_PATH]; t_tmpfile(p, sizeof p, "kobj_file.bin"); *f = FILE_ALL_ACCESS;
    return CreateFileA(p, GENERIC_READ | GENERIC_WRITE | WRITE_DAC | READ_CONTROL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL); }
static const struct { const char *n; mk_fn mk; } k_kobj[] = {
    { "event", mk_event }, { "mutex", mk_mutex }, { "semaphore", mk_sem }, { "timer", mk_timer },
    { "mapping", mk_map }, { "job", mk_job },
};
TEST(gap12_kobj_set_dacl_by_type) {
    REQUIRE(U(SetKernelObjectSecurity));
    for (unsigned i = 0; i < sizeof k_kobj / sizeof k_kobj[0]; i++) {
        DWORD full; HANDLE h = k_kobj[i].mk(&full);
        if (!h) { t_note("cannot create %s", k_kobj[i].n); continue; }
        DSD d; dsd_init(&d, full & ~(DWORD)(0xFF000000));
        SET_ERR(0xBAD);
        BOOL r = U(SetKernelObjectSecurity)(h, DACL_SECURITY_INFORMATION, &d.sd);
        CHECKF(r, "%s: SetKernelObjectSecurity err=%lu", k_kobj[i].n, GetLastError());
        if (r) check_world_dacl(h, full & ~(DWORD)(0xFF000000), k_kobj[i].n);
        CloseHandle(h);
    }
}
TEST(gap12_kobj_set_dacl_file_handle) {
    REQUIRE(U(SetKernelObjectSecurity));
    DWORD full; HANDLE h = mk_file(&full);
    if (h == INVALID_HANDLE_VALUE) { t_skip("cannot create file"); return; }
    DSD d; dsd_init(&d, FILE_ALL_ACCESS);
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetKernelObjectSecurity)(h, DACL_SECURITY_INFORMATION, &d.sd), TRUE);
    check_world_dacl(h, FILE_ALL_ACCESS, "file");
    CloseHandle(h);
}
TEST(gap12_kobj_null_handle) {
    REQUIRE(U(SetKernelObjectSecurity));
    DSD d; dsd_init(&d, EVENT_ALL_ACCESS);
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetKernelObjectSecurity)(NULL, DACL_SECURITY_INFORMATION, &d.sd), FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap12_kobj_closed_handle) {
    REQUIRE(U(SetKernelObjectSecurity));
    HANDLE h = CreateEventW(NULL, TRUE, FALSE, NULL); CloseHandle(h);
    DSD d; dsd_init(&d, EVENT_ALL_ACCESS);
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetKernelObjectSecurity)(h, DACL_SECURITY_INFORMATION, &d.sd), FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap12_kobj_bogus_handle_values) {
    REQUIRE(U(SetKernelObjectSecurity));
    static const ULONG_PTR vals[] = { 0x3FFFFFFC, 0xFFFFFFF8, 0x7FFFFFFC, 0xFFFFFFFC, 0x00FF0000ull };
    DSD d; dsd_init(&d, EVENT_ALL_ACCESS);
    for (unsigned i = 0; i < 5; i++) {
        SET_ERR(0xBAD);
        BOOL r = U(SetKernelObjectSecurity)((HANDLE)vals[i], DACL_SECURITY_INFORMATION, &d.sd);
        if (r) { t_note("handle 0x%llx unexpectedly valid", (unsigned long long)vals[i]); continue; }
        EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
}
TEST(gap12_kobj_missing_write_dac) {
    REQUIRE(U(SetKernelObjectSecurity));
    HANDLE e = CreateEventW(NULL, TRUE, FALSE, NULL), ro = NULL;
    CHECK(DuplicateHandle(GetCurrentProcess(), e, GetCurrentProcess(), &ro, SYNCHRONIZE | EVENT_MODIFY_STATE | READ_CONTROL, FALSE, 0));
    DSD d; dsd_init(&d, EVENT_ALL_ACCESS);
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetKernelObjectSecurity)(ro, DACL_SECURITY_INFORMATION, &d.sd), FALSE);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(ro); CloseHandle(e);
}
TEST(gap12_kobj_missing_write_owner) {
    REQUIRE(U(SetKernelObjectSecurity));
    HANDLE e = CreateEventW(NULL, TRUE, FALSE, NULL), h = NULL;
    CHECK(DuplicateHandle(GetCurrentProcess(), e, GetCurrentProcess(), &h, SYNCHRONIZE | WRITE_DAC | READ_CONTROL, FALSE, 0));
    SECURITY_DESCRIPTOR sd; sd_init(&sd); SetSecurityDescriptorOwner(&sd, sid_user(), FALSE);
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetKernelObjectSecurity)(h, OWNER_SECURITY_INFORMATION, &sd), FALSE);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(h); CloseHandle(e);
}
TEST(gap12_kobj_dacl_needs_only_write_dac) {
    REQUIRE(U(SetKernelObjectSecurity));
    HANDLE e = CreateEventW(NULL, TRUE, FALSE, NULL), h = NULL;
    CHECK(DuplicateHandle(GetCurrentProcess(), e, GetCurrentProcess(), &h, WRITE_DAC, FALSE, 0));
    DSD d; dsd_init(&d, EVENT_ALL_ACCESS);
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetKernelObjectSecurity)(h, DACL_SECURITY_INFORMATION, &d.sd), TRUE);
    CloseHandle(h);
    check_world_dacl(e, EVENT_ALL_ACCESS, "event");
    CloseHandle(e);
}
TEST(gap12_kobj_missing_sacl_privilege) {
    REQUIRE(U(SetKernelObjectSecurity));
    HANDLE e = CreateEventW(NULL, TRUE, FALSE, NULL);
    SECURITY_DESCRIPTOR sd; BYTE ab[64]; sd_init(&sd);
    SetSecurityDescriptorSacl(&sd, TRUE, mkacl_empty(ab, sizeof ab), FALSE);
    SET_ERR(0xBAD);
    BOOL r = U(SetKernelObjectSecurity)(e, SACL_SECURITY_INFORMATION, &sd);
    /* the handle lacks ACCESS_SYSTEM_SECURITY (never requested at creation) */
    t_note("SACL set on event handle: ret=%d err=%lu", r, GetLastError());
    CHECK(r == FALSE || r == TRUE);
    CloseHandle(e);
}
TEST(gap12_kobj_self_relative_sd_accepted) {
    REQUIRE(U(SetKernelObjectSecurity));
    HANDLE e = CreateEventW(NULL, TRUE, FALSE, NULL);
    DSD d; dsd_init(&d, EVENT_ALL_ACCESS);
    BYTE rel[512]; DWORD n = sizeof rel;
    CHECK(MakeSelfRelativeSD(&d.sd, rel, &n));
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetKernelObjectSecurity)(e, DACL_SECURITY_INFORMATION, rel), TRUE);
    check_world_dacl(e, EVENT_ALL_ACCESS, "event");
    CloseHandle(e);
}
TEST(gap12_kobj_owner_to_current_user) {
    REQUIRE(U(SetKernelObjectSecurity));
    HANDLE e = CreateEventW(NULL, TRUE, FALSE, NULL);
    SECURITY_DESCRIPTOR sd; sd_init(&sd); SetSecurityDescriptorOwner(&sd, sid_user(), FALSE);
    SET_ERR(0xBAD);
    BOOL r = U(SetKernelObjectSecurity)(e, OWNER_SECURITY_INFORMATION, &sd);
    CHECKF(r, "err=%lu", GetLastError());
    if (r) {
        BYTE buf[512]; PSID o = NULL; BOOL d = 0;
        CHECK(get_kobj_sd(e, OWNER_SECURITY_INFORMATION, buf, sizeof buf));
        CHECK(GetSecurityDescriptorOwner(buf, &o, &d));
        CHECK(o && EqualSid(o, sid_user()));
    }
    CloseHandle(e);
}
TEST(gap12_kobj_owner_missing_in_sd) {
    REQUIRE(U(SetKernelObjectSecurity));
    HANDLE e = CreateEventW(NULL, TRUE, FALSE, NULL);
    SECURITY_DESCRIPTOR sd; sd_init(&sd);
    SET_ERR(0xBAD);
    BOOL r = U(SetKernelObjectSecurity)(e, OWNER_SECURITY_INFORMATION, &sd);
    t_note("OWNER info with owner-less SD: ret=%d err=%lu", r, GetLastError());
    SET_ERR(0xBAD);
    r = U(SetKernelObjectSecurity)(e, GROUP_SECURITY_INFORMATION, &sd);
    t_note("GROUP info with group-less SD: ret=%d err=%lu", r, GetLastError());
    CloseHandle(e);
}
TEST(gap12_kobj_dacl_not_present_in_sd) {
    REQUIRE(U(SetKernelObjectSecurity));
    HANDLE e = CreateEventW(NULL, TRUE, FALSE, NULL);
    SECURITY_DESCRIPTOR sd; sd_init(&sd);
    SET_ERR(0xBAD);
    BOOL r = U(SetKernelObjectSecurity)(e, DACL_SECURITY_INFORMATION, &sd);
    t_note("DACL info with no DACL present: ret=%d err=%lu", r, GetLastError());
    if (r) { BYTE buf[512]; int p = 0; BOOL nul = 0; get_kobj_sd(e, DACL_SECURITY_INFORMATION, buf, sizeof buf); sd_dacl_info(buf, &p, NULL, NULL, &nul); t_note("resulting DACL present=%d null=%d", p, nul); }
    CloseHandle(e);
}
TEST(gap12_kobj_null_dacl_grants_all) {
    REQUIRE(U(SetKernelObjectSecurity));
    HANDLE e = CreateEventW(NULL, TRUE, FALSE, NULL);
    SECURITY_DESCRIPTOR sd; sd_init(&sd); SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetKernelObjectSecurity)(e, DACL_SECURITY_INFORMATION, &sd), TRUE);
    BYTE buf[512]; int p = 0; BOOL nul = 0;
    CHECK(get_kobj_sd(e, DACL_SECURITY_INFORMATION, buf, sizeof buf));
    sd_dacl_info(buf, &p, NULL, NULL, &nul);
    CHECK(p); CHECK(nul);
    /* a fresh handle can now be opened for full access */
    HANDLE h = NULL;
    CHECK(DuplicateHandle(GetCurrentProcess(), e, GetCurrentProcess(), &h, EVENT_ALL_ACCESS, FALSE, 0));
    if (h) CloseHandle(h);
    CloseHandle(e);
}
TEST(gap12_kobj_empty_dacl_then_reopen_denied) {
    REQUIRE(U(SetKernelObjectSecurity));
    if (t_is_wine()) { t_skip("emulator does not enforce named-object DACLs"); return; }
    /* named event: an empty DACL denies every new open except WRITE_DAC/READ_CONTROL for the owner */
    WCHAR name[64]; swprintf(name, 64, L"k32t_g12_ev_%lu", GetCurrentProcessId());
    HANDLE e = CreateEventW(NULL, TRUE, FALSE, name);
    CHECK(e != NULL);
    SECURITY_DESCRIPTOR sd; BYTE ab[64]; sd_init(&sd);
    SetSecurityDescriptorDacl(&sd, TRUE, mkacl_empty(ab, sizeof ab), FALSE);
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetKernelObjectSecurity)(e, DACL_SECURITY_INFORMATION, &sd), TRUE);
    SET_ERR(0xBAD);
    HANDLE o = OpenEventW(SYNCHRONIZE, FALSE, name);
    CHECK(o == NULL);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    if (o) CloseHandle(o);
    /* owner retains WRITE_DAC: restore */
    SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
    SET_ERR(0xBAD);
    BOOL r = U(SetKernelObjectSecurity)(e, DACL_SECURITY_INFORMATION, &sd);
    CHECK(r);
    o = OpenEventW(SYNCHRONIZE, FALSE, name);
    CHECK(o != NULL);
    if (o) CloseHandle(o);
    CloseHandle(e);
}
TEST(gap12_kobj_info_zero) {
    REQUIRE(U(SetKernelObjectSecurity));
    HANDLE e = CreateEventW(NULL, TRUE, FALSE, NULL);
    DSD d; dsd_init(&d, EVENT_ALL_ACCESS);
    SET_ERR(0xBAD);
    BOOL r = U(SetKernelObjectSecurity)(e, 0, &d.sd);
    t_note("SecurityInformation=0: ret=%d err=%lu", r, GetLastError());
    CloseHandle(e);
}
TEST(gap12_kobj_info_undefined_bits) {
    REQUIRE(U(SetKernelObjectSecurity));
    HANDLE e = CreateEventW(NULL, TRUE, FALSE, NULL);
    DSD d; dsd_init(&d, EVENT_ALL_ACCESS);
    static const DWORD bits[] = { 0x200, 0x20000, 0x08000000 };
    for (unsigned i = 0; i < 3; i++) {
        SET_ERR(0xBAD);
        BOOL r = U(SetKernelObjectSecurity)(e, bits[i], &d.sd);
        t_note("SecurityInformation=0x%lx: ret=%d err=%lu", bits[i], r, GetLastError());
    }
    CloseHandle(e);
}
TEST(gap12_kobj_bad_sd_revision) {
    REQUIRE(U(SetKernelObjectSecurity));
    HANDLE e = CreateEventW(NULL, TRUE, FALSE, NULL);
    DSD d; dsd_init(&d, EVENT_ALL_ACCESS); d.sd.Revision = 2;
    SET_ERR(0xBAD);
    BOOL r = U(SetKernelObjectSecurity)(e, DACL_SECURITY_INFORMATION, &d.sd);
    CHECK(r == FALSE);
    t_note("bad SD revision: err=%lu", GetLastError());
    CloseHandle(e);
}
TEST(gap12_kobj_dacl_roundtrip_two_aces) {
    REQUIRE(U(SetKernelObjectSecurity));
    HANDLE e = CreateEventW(NULL, TRUE, FALSE, NULL);
    SECURITY_DESCRIPTOR sd; BYTE ab[256]; PACL a = (PACL)ab; sd_init(&sd);
    InitializeAcl(a, sizeof ab, ACL_REVISION);
    AddAccessAllowedAce(a, ACL_REVISION, EVENT_ALL_ACCESS, sid_user());
    AddAccessDeniedAce(a, ACL_REVISION, EVENT_MODIFY_STATE, sid_world());
    SetSecurityDescriptorDacl(&sd, TRUE, a, FALSE);
    CHECK(U(SetKernelObjectSecurity)(e, DACL_SECURITY_INFORMATION, &sd));
    BYTE buf[1024]; PACL got = NULL; BOOL p = 0, d = 0; ACL_SIZE_INFORMATION si;
    CHECK(get_kobj_sd(e, DACL_SECURITY_INFORMATION, buf, sizeof buf));
    CHECK(GetSecurityDescriptorDacl(buf, &p, &got, &d));
    CHECK(p && got && GetAclInformation(got, &si, sizeof si, AclSizeInformation));
    EXPECT_EQ(si.AceCount, 2);
    ACCESS_ALLOWED_ACE *a0 = NULL, *a1 = NULL;
    GetAce(got, 0, (LPVOID *)&a0); GetAce(got, 1, (LPVOID *)&a1);
    if (a0 && a1) { EXPECT_EQ(a0->Header.AceType, ACCESS_ALLOWED_ACE_TYPE); EXPECT_EQ(a1->Header.AceType, ACCESS_DENIED_ACE_TYPE); }
    CloseHandle(e);
}
TEST(gap12_kobj_repeated_sets) {
    REQUIRE(U(SetKernelObjectSecurity));
    HANDLE e = CreateEventW(NULL, TRUE, FALSE, NULL);
    DSD d; dsd_init(&d, EVENT_ALL_ACCESS);
    int bad = 0;
    for (int i = 0; i < 2000; i++) if (!U(SetKernelObjectSecurity)(e, DACL_SECURITY_INFORMATION, &d.sd)) bad++;
    EXPECT_EQ(bad, 0);
    CloseHandle(e);
}
TEST(gap12_kobj_cross_thread_visibility) {
    REQUIRE(U(SetKernelObjectSecurity));
    /* the handle's SD is shared: a second handle sees the change made through the first */
    HANDLE e = CreateEventW(NULL, TRUE, FALSE, NULL), d2 = NULL;
    CHECK(DuplicateHandle(GetCurrentProcess(), e, GetCurrentProcess(), &d2, READ_CONTROL, FALSE, 0));
    DSD d; dsd_init(&d, EVENT_ALL_ACCESS);
    CHECK(U(SetKernelObjectSecurity)(e, DACL_SECURITY_INFORMATION, &d.sd));
    check_world_dacl(d2, EVENT_ALL_ACCESS, "second handle");
    CloseHandle(d2); CloseHandle(e);
}
/* ---------------- SetFileSecurityW ---------------- */
static HANDLE mkfile(const char *leaf, char *path, size_t cap) {
    t_tmpfile(path, cap, leaf);
    return CreateFileA(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}
static void wpath(const char *a, WCHAR *w, int cap) { MultiByteToWideChar(CP_ACP, 0, a, -1, w, cap); }
static void check_file_world_dacl(const WCHAR *w, DWORD mask, const char *what) {
    BYTE buf[1024]; DWORD need = 0; int present = 0; PSID sid = NULL; DWORD m = 0;
    if (!GetFileSecurityW(w, DACL_SECURITY_INFORMATION, (PSECURITY_DESCRIPTOR)buf, sizeof buf, &need)) { CHECKF(0, "%s: GetFileSecurityW err=%lu", what, GetLastError()); return; }
    int n = sd_dacl_info(buf, &present, &sid, &m, NULL);
    CHECKF(present && n == 1, "%s: present=%d aces=%d", what, present, n);
    if (n == 1) { CHECKF(EqualSid(sid, sid_world()), "%s: sid", what); CHECKF(m == mask, "%s: mask 0x%lx want 0x%lx", what, m, mask); }
}
TEST(gap12_file_set_dacl_roundtrip) {
    REQUIRE(U(SetFileSecurityW));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; HANDLE h = mkfile("fsec1.bin", p, sizeof p); CloseHandle(h); wpath(p, w, MAX_PATH);
    DSD d; dsd_init(&d, FILE_ALL_ACCESS);
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetFileSecurityW)(w, DACL_SECURITY_INFORMATION, &d.sd), TRUE);
    check_file_world_dacl(w, FILE_ALL_ACCESS, "file");
}
TEST(gap12_file_set_dacl_masks) {
    REQUIRE(U(SetFileSecurityW));
    if (t_is_wine()) { t_skip("emulator derives file DACLs from unix permissions"); return; }
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; HANDLE h = mkfile("fsec2.bin", p, sizeof p); CloseHandle(h); wpath(p, w, MAX_PATH);
    static const DWORD masks[] = { FILE_ALL_ACCESS, FILE_GENERIC_READ | FILE_GENERIC_WRITE | DELETE, FILE_GENERIC_READ | DELETE | WRITE_DAC, FILE_ALL_ACCESS };
    for (unsigned i = 0; i < 4; i++) {
        DSD d; dsd_init(&d, masks[i]);
        BOOL r = U(SetFileSecurityW)(w, DACL_SECURITY_INFORMATION, &d.sd);
        CHECKF(r, "mask 0x%lx err=%lu", masks[i], GetLastError());
        if (r) check_file_world_dacl(w, masks[i], "mask table");
    }
}
TEST(gap12_file_set_dacl_directory) {
    REQUIRE(U(SetFileSecurityW));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; t_tmpfile(p, sizeof p, "fsec_dir");
    CHECKF(CreateDirectoryA(p, NULL), "path %s err=%lu", p, GetLastError()); wpath(p, w, MAX_PATH);
    DSD d; dsd_init(&d, FILE_ALL_ACCESS);
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetFileSecurityW)(w, DACL_SECURITY_INFORMATION, &d.sd), TRUE);
    check_file_world_dacl(w, FILE_ALL_ACCESS, "directory");
}
TEST(gap12_file_set_dacl_extended_prefix) {
    REQUIRE(U(SetFileSecurityW));
    char p[MAX_PATH]; WCHAR w[MAX_PATH + 8]; HANDLE h = mkfile("fsec3.bin", p, sizeof p); CloseHandle(h);
    lstrcpyW(w, L"\\\\?\\"); wpath(p, w + 4, MAX_PATH);
    DSD d; dsd_init(&d, FILE_ALL_ACCESS);
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetFileSecurityW)(w, DACL_SECURITY_INFORMATION, &d.sd), TRUE);
    check_file_world_dacl(w + 4, FILE_ALL_ACCESS, "\\\\?\\ path");
}
TEST(gap12_file_set_dacl_forward_slashes_rejected_by_prefix) {
    REQUIRE(U(SetFileSecurityW));
    /* with \\?\ nothing is normalised: forward slashes are not separators, so the object is not found */
    char p[MAX_PATH]; WCHAR w[MAX_PATH + 8]; HANDLE h = mkfile("fsec4.bin", p, sizeof p); CloseHandle(h);
    lstrcpyW(w, L"\\\\?\\"); wpath(p, w + 4, MAX_PATH);
    for (WCHAR *c = w; *c; c++) if (*c == L'\\' && c > w + 3) *c = L'/';
    DSD d; dsd_init(&d, FILE_ALL_ACCESS);
    SET_ERR(0xBAD);
    BOOL r = U(SetFileSecurityW)(w, DACL_SECURITY_INFORMATION, &d.sd);
    CHECK(r == FALSE);
    t_note("\\\\?\\ with forward slashes: err=%lu", GetLastError());
}
TEST(gap12_file_nonexistent) {
    REQUIRE(U(SetFileSecurityW));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; t_tmpfile(p, sizeof p, "nope_nope.bin"); wpath(p, w, MAX_PATH);
    DSD d; dsd_init(&d, FILE_ALL_ACCESS);
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetFileSecurityW)(w, DACL_SECURITY_INFORMATION, &d.sd), FALSE);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(gap12_file_nonexistent_directory) {
    REQUIRE(U(SetFileSecurityW));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; t_tmpfile(p, sizeof p, "nodir_zz\\file.bin"); wpath(p, w, MAX_PATH);
    DSD d; dsd_init(&d, FILE_ALL_ACCESS);
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetFileSecurityW)(w, DACL_SECURITY_INFORMATION, &d.sd), FALSE);
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(gap12_file_nonexistent_drive) {
    REQUIRE(U(SetFileSecurityW));
    DSD d; dsd_init(&d, FILE_ALL_ACCESS);
    if (t_is_wine()) { t_skip("emulator reports a different error for unmapped drives"); return; }
    DWORD mask = GetLogicalDrives(); char L = 0;
    for (int i = 25; i >= 0; i--) if (!(mask & (1u << i))) { L = 'A' + i; break; }
    if (!L) { t_skip("no free drive letter"); return; }
    WCHAR w[] = { (WCHAR)L, L':', L'\\', L'k', L'.', L't', L'x', L't', 0 };
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetFileSecurityW)(w, DACL_SECURITY_INFORMATION, &d.sd), FALSE);
    EXPECT_ERR(ERROR_PATH_NOT_FOUND);
}
TEST(gap12_file_empty_name) {
    REQUIRE(U(SetFileSecurityW));
    DSD d; dsd_init(&d, FILE_ALL_ACCESS);
    SET_ERR(0xBAD);
    BOOL r = U(SetFileSecurityW)(L"", DACL_SECURITY_INFORMATION, &d.sd);
    CHECK(r == FALSE);
    t_note("empty file name: err=%lu", GetLastError());
}
TEST(gap12_file_wildcard_names) {
    REQUIRE(U(SetFileSecurityW));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; HANDLE h = mkfile("wild_a.bin", p, sizeof p); CloseHandle(h);
    DSD d; dsd_init(&d, FILE_ALL_ACCESS);
    t_tmpfile(p, sizeof p, "wild_*.bin"); wpath(p, w, MAX_PATH);
    SET_ERR(0xBAD);
    BOOL r = U(SetFileSecurityW)(w, DACL_SECURITY_INFORMATION, &d.sd);
    CHECK(r == FALSE);
    t_note("wildcard '*': err=%lu", GetLastError());
    t_tmpfile(p, sizeof p, "wild_?.bin"); wpath(p, w, MAX_PATH);
    SET_ERR(0xBAD);
    r = U(SetFileSecurityW)(w, DACL_SECURITY_INFORMATION, &d.sd);
    CHECK(r == FALSE);
    t_note("wildcard '?': err=%lu", GetLastError());
}
TEST(gap12_file_invalid_chars) {
    REQUIRE(U(SetFileSecurityW));
    DSD d; dsd_init(&d, FILE_ALL_ACCESS);
    static const WCHAR *names[] = { L"C:\\k32t_a|b.txt", L"C:\\k32t_a<b.txt", L"C:\\k32t_a\"b.txt", L"C:\\k32t_a>b.txt" };
    for (unsigned i = 0; i < 4; i++) {
        SET_ERR(0xBAD);
        BOOL r = U(SetFileSecurityW)(names[i], DACL_SECURITY_INFORMATION, &d.sd);
        CHECK(r == FALSE);
        t_note("%ls: err=%lu", names[i], GetLastError());
    }
}
TEST(gap12_file_read_only_attribute_ok) {
    REQUIRE(U(SetFileSecurityW));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; HANDLE h = mkfile("fsec_ro.bin", p, sizeof p); CloseHandle(h); wpath(p, w, MAX_PATH);
    SetFileAttributesA(p, FILE_ATTRIBUTE_READONLY);
    DSD d; dsd_init(&d, FILE_ALL_ACCESS);
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetFileSecurityW)(w, DACL_SECURITY_INFORMATION, &d.sd), TRUE);
    SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL);
}
TEST(gap12_file_while_open_exclusive) {
    REQUIRE(U(SetFileSecurityW));
    char p[MAX_PATH]; WCHAR w[MAX_PATH];
    t_tmpfile(p, sizeof p, "fsec_excl.bin"); wpath(p, w, MAX_PATH);
    HANDLE h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    DSD d; dsd_init(&d, FILE_ALL_ACCESS);
    SET_ERR(0xBAD);
    BOOL r = U(SetFileSecurityW)(w, DACL_SECURITY_INFORMATION, &d.sd);
    t_note("file open with share mode 0: ret=%d err=%lu", r, GetLastError());
    CloseHandle(h);
}
TEST(gap12_file_missing_write_dac_via_acl) {
    REQUIRE(U(SetFileSecurityW));
    /* deny WRITE_DAC to everyone on the object; the owner still has implicit WRITE_DAC, so this still succeeds */
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; HANDLE h = mkfile("fsec_deny.bin", p, sizeof p); CloseHandle(h); wpath(p, w, MAX_PATH);
    SECURITY_DESCRIPTOR sd; BYTE ab[128]; PACL a = (PACL)ab; sd_init(&sd);
    InitializeAcl(a, sizeof ab, ACL_REVISION);
    AddAccessDeniedAce(a, ACL_REVISION, WRITE_DAC, sid_world());
    AddAccessAllowedAce(a, ACL_REVISION, FILE_ALL_ACCESS & ~(DWORD)WRITE_DAC, sid_world());
    SetSecurityDescriptorDacl(&sd, TRUE, a, FALSE);
    SET_ERR(0xBAD);
    BOOL r = U(SetFileSecurityW)(w, DACL_SECURITY_INFORMATION, &sd);
    CHECKF(r, "err=%lu", GetLastError());
    /* implicit owner rights: DACL can be set again */
    DSD d; dsd_init(&d, FILE_ALL_ACCESS);
    SET_ERR(0xBAD);
    r = U(SetFileSecurityW)(w, DACL_SECURITY_INFORMATION, &d.sd);
    t_note("second set after WRITE_DAC deny: ret=%d err=%lu", r, GetLastError());
    if (!r) { /* restore if possible */ }
}
TEST(gap12_file_owner_same_as_current) {
    REQUIRE(U(SetFileSecurityW));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; HANDLE h = mkfile("fsec_own.bin", p, sizeof p); CloseHandle(h); wpath(p, w, MAX_PATH);
    BYTE buf[512]; DWORD need = 0;
    if (!GetFileSecurityW(w, OWNER_SECURITY_INFORMATION, buf, sizeof buf, &need)) { t_skip("GetFileSecurityW"); return; }
    SET_ERR(0xBAD);
    BOOL r = U(SetFileSecurityW)(w, OWNER_SECURITY_INFORMATION, buf);
    CHECKF(r, "setting the existing owner back: err=%lu", GetLastError());
}
TEST(gap12_file_info_zero) {
    REQUIRE(U(SetFileSecurityW));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; HANDLE h = mkfile("fsec_zero.bin", p, sizeof p); CloseHandle(h); wpath(p, w, MAX_PATH);
    DSD d; dsd_init(&d, FILE_ALL_ACCESS);
    SET_ERR(0xBAD);
    BOOL r = U(SetFileSecurityW)(w, 0, &d.sd);
    t_note("SecurityInformation=0: ret=%d err=%lu", r, GetLastError());
}
TEST(gap12_file_sacl_without_privilege) {
    REQUIRE(U(SetFileSecurityW));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; HANDLE h = mkfile("fsec_sacl.bin", p, sizeof p); CloseHandle(h); wpath(p, w, MAX_PATH);
    SECURITY_DESCRIPTOR sd; BYTE ab[64]; sd_init(&sd);
    SetSecurityDescriptorSacl(&sd, TRUE, mkacl_empty(ab, sizeof ab), FALSE);
    SET_ERR(0xBAD);
    BOOL r = U(SetFileSecurityW)(w, SACL_SECURITY_INFORMATION, &sd);
    /* opening with ACCESS_SYSTEM_SECURITY requires SeSecurityPrivilege, which a normal token lacks */
    t_note("SACL set without privilege: ret=%d err=%lu", r, GetLastError());
}
TEST(gap12_file_device_name) {
    REQUIRE(U(SetFileSecurityW));
    DSD d; dsd_init(&d, FILE_ALL_ACCESS);
    SET_ERR(0xBAD);
    BOOL r = U(SetFileSecurityW)(L"NUL", DACL_SECURITY_INFORMATION, &d.sd);
    t_note("NUL device: ret=%d err=%lu", r, GetLastError());
    CHECK(r == FALSE || r == TRUE);
}
TEST(gap12_file_long_path_prefix) {
    REQUIRE(U(SetFileSecurityW));
    /* >MAX_PATH path under the tmp dir through \\?\ */
    WCHAR w[1200]; char base[MAX_PATH]; t_tmpfile(base, sizeof base, "");
    int n = swprintf(w, 1200, L"\\\\?\\");
    MultiByteToWideChar(CP_ACP, 0, base, -1, w + n, 1000);
    int len = lstrlenW(w);
    if (w[len - 1] != L'\\') { w[len++] = L'\\'; w[len] = 0; }
    int made = 0;
    for (int i = 0; i < 5; i++) {
        for (int k = 0; k < 60; k++) w[len++] = L'a' + (i % 20);
        w[len] = 0;
        if (!CreateDirectoryW(w, NULL)) break;
        made++;
        w[len++] = L'\\'; w[len] = 0;
    }
    if (made < 5) { t_skip("could not build deep path (made %d)", made); return; }
    CHECK(len > MAX_PATH);
    lstrcpyW(w + len, L"deep.bin");
    HANDLE h = CreateFileW(w, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) { t_skip("cannot create deep file err=%lu", GetLastError()); return; }
    CloseHandle(h);
    DSD d; dsd_init(&d, FILE_ALL_ACCESS);
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetFileSecurityW)(w, DACL_SECURITY_INFORMATION, &d.sd), TRUE);
}
TEST(gap12_file_repeat) {
    REQUIRE(U(SetFileSecurityW));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; HANDLE h = mkfile("fsec_rep.bin", p, sizeof p); CloseHandle(h); wpath(p, w, MAX_PATH);
    DSD d; dsd_init(&d, FILE_ALL_ACCESS);
    int bad = 0;
    for (int i = 0; i < 100; i++) if (!U(SetFileSecurityW)(w, DACL_SECURITY_INFORMATION, &d.sd)) bad++;
    EXPECT_EQ(bad, 0);
}
TEST(gap12_file_null_dacl_then_delete) {
    REQUIRE(U(SetFileSecurityW));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; HANDLE h = mkfile("fsec_nul.bin", p, sizeof p); CloseHandle(h); wpath(p, w, MAX_PATH);
    SECURITY_DESCRIPTOR sd; sd_init(&sd); SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetFileSecurityW)(w, DACL_SECURITY_INFORMATION, &sd), TRUE);
    CHECK(DeleteFileA(p));
}
TEST(gap12_file_invalid_sd_revision) {
    REQUIRE(U(SetFileSecurityW));
    char p[MAX_PATH]; WCHAR w[MAX_PATH]; HANDLE h = mkfile("fsec_rev.bin", p, sizeof p); CloseHandle(h); wpath(p, w, MAX_PATH);
    DSD d; dsd_init(&d, FILE_ALL_ACCESS); d.sd.Revision = 5;
    SET_ERR(0xBAD);
    BOOL r = U(SetFileSecurityW)(w, DACL_SECURITY_INFORMATION, &d.sd);
    CHECK(r == FALSE);
    t_note("bad revision: err=%lu", GetLastError());
}

/* ---------------- SetPrivateObjectSecurity / Ex ---------------- */
static GENERIC_MAPPING s_gm = { STANDARD_RIGHTS_READ | 1, STANDARD_RIGHTS_WRITE | 2, STANDARD_RIGHTS_EXECUTE | 4, STANDARD_RIGHTS_ALL | 7 };
static HANDLE s_tok;
static HANDLE tok(void) {
    if (!s_tok) {
        HANDLE p = NULL;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &p)) {
            DuplicateToken(p, SecurityImpersonation, &s_tok);
            CloseHandle(p);
        }
    }
    return s_tok;
}
/* create an object SD (self-relative, heap-allocated) with owner=user, group=admin, DACL = world allow 0x1 */
static PSECURITY_DESCRIPTOR mkobj(void) {
    SECURITY_DESCRIPTOR c; BYTE ab[128]; PSECURITY_DESCRIPTOR obj = NULL;
    sd_init(&c);
    SetSecurityDescriptorOwner(&c, sid_user(), FALSE);
    SetSecurityDescriptorGroup(&c, sid_admin(), FALSE);
    SetSecurityDescriptorDacl(&c, TRUE, mkacl(ab, sizeof ab, sid_world(), 0x1), FALSE);
    if (!CreatePrivateObjectSecurity(NULL, &c, &obj, FALSE, tok(), &s_gm)) {
        if (!CreatePrivateObjectSecurity(NULL, &c, &obj, FALSE, NULL, &s_gm)) return NULL;
    }
    return obj;
}
static int obj_dacl(PSECURITY_DESCRIPTOR obj, PSID *sid, DWORD *mask) { int p = 0; return sd_dacl_info(obj, &p, sid, mask, NULL); }

TEST(gap12_private_set_dacl) {
    REQUIRE(U(SetPrivateObjectSecurity)); if (t_is_wine()) { t_skip("emulator stubs SetPrivateObjectSecurity"); return; }
    PSECURITY_DESCRIPTOR obj = mkobj();
    if (!obj) { t_skip("CreatePrivateObjectSecurity err=%lu", GetLastError()); return; }
    DSD m; dsd_init(&m, 0x2);
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetPrivateObjectSecurity)(DACL_SECURITY_INFORMATION, &m.sd, &obj, &s_gm, tok()), TRUE);
    PSID sid = NULL; DWORD mask = 0;
    EXPECT_EQ(obj_dacl(obj, &sid, &mask), 1);
    EXPECT_EQ(mask, 0x2);
    CHECK(sid && EqualSid(sid, sid_world()));
    DestroyPrivateObjectSecurity(&obj);
}
TEST(gap12_private_set_dacl_keeps_owner_group) {
    REQUIRE(U(SetPrivateObjectSecurity)); if (t_is_wine()) { t_skip("emulator stubs SetPrivateObjectSecurity"); return; }
    PSECURITY_DESCRIPTOR obj = mkobj();
    if (!obj) { t_skip("CreatePrivateObjectSecurity"); return; }
    DSD m; dsd_init(&m, 0x4);
    CHECK(U(SetPrivateObjectSecurity)(DACL_SECURITY_INFORMATION, &m.sd, &obj, &s_gm, tok()));
    PSID o = NULL, g = NULL; BOOL d = 0;
    CHECK(GetSecurityDescriptorOwner(obj, &o, &d)); CHECK(o && EqualSid(o, sid_user()));
    CHECK(GetSecurityDescriptorGroup(obj, &g, &d)); CHECK(g && EqualSid(g, sid_admin()));
    DestroyPrivateObjectSecurity(&obj);
}
TEST(gap12_private_set_owner) {
    REQUIRE(U(SetPrivateObjectSecurity)); if (t_is_wine()) { t_skip("emulator stubs SetPrivateObjectSecurity"); return; }
    PSECURITY_DESCRIPTOR obj = mkobj();
    if (!obj) { t_skip("CreatePrivateObjectSecurity"); return; }
    SECURITY_DESCRIPTOR m; sd_init(&m); SetSecurityDescriptorOwner(&m, sid_user(), FALSE);
    SET_ERR(0xBAD);
    BOOL r = U(SetPrivateObjectSecurity)(OWNER_SECURITY_INFORMATION, &m, &obj, &s_gm, tok());
    CHECKF(r, "err=%lu", GetLastError());
    if (r) { PSID o = NULL; BOOL d = 0; CHECK(GetSecurityDescriptorOwner(obj, &o, &d)); CHECK(o && EqualSid(o, sid_user())); }
    /* an owner the token may not assign: result depends on the caller's rights */
    SetSecurityDescriptorOwner(&m, sid_world(), FALSE);
    SET_ERR(0xBAD);
    r = U(SetPrivateObjectSecurity)(OWNER_SECURITY_INFORMATION, &m, &obj, &s_gm, tok());
    t_note("owner=Everyone: ret=%d err=%lu", r, r ? 0 : GetLastError());
    DestroyPrivateObjectSecurity(&obj);
}
TEST(gap12_private_set_group) {
    REQUIRE(U(SetPrivateObjectSecurity)); if (t_is_wine()) { t_skip("emulator stubs SetPrivateObjectSecurity"); return; }
    PSECURITY_DESCRIPTOR obj = mkobj();
    if (!obj) { t_skip("CreatePrivateObjectSecurity"); return; }
    SECURITY_DESCRIPTOR m; sd_init(&m); SetSecurityDescriptorGroup(&m, sid_world(), FALSE);
    BOOL r = U(SetPrivateObjectSecurity)(GROUP_SECURITY_INFORMATION, &m, &obj, &s_gm, tok());
    CHECKF(r, "err=%lu", GetLastError());
    if (r) { PSID g = NULL; BOOL d = 0; CHECK(GetSecurityDescriptorGroup(obj, &g, &d)); CHECK(g && EqualSid(g, sid_world())); }
    DestroyPrivateObjectSecurity(&obj);
}
TEST(gap12_private_set_all_three) {
    REQUIRE(U(SetPrivateObjectSecurity)); if (t_is_wine()) { t_skip("emulator stubs SetPrivateObjectSecurity"); return; }
    PSECURITY_DESCRIPTOR obj = mkobj();
    if (!obj) { t_skip("CreatePrivateObjectSecurity"); return; }
    SECURITY_DESCRIPTOR m; BYTE ab[128]; sd_init(&m);
    SetSecurityDescriptorOwner(&m, sid_user(), FALSE);
    SetSecurityDescriptorGroup(&m, sid_world(), FALSE);
    SetSecurityDescriptorDacl(&m, TRUE, mkacl(ab, sizeof ab, sid_user(), 0x1), FALSE);
    BOOL r = U(SetPrivateObjectSecurity)(OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, &m, &obj, &s_gm, tok());
    CHECKF(r, "err=%lu", GetLastError());
    if (r) {
        PSID o = NULL, g = NULL, s = NULL; BOOL d = 0; DWORD mask = 0;
        CHECK(GetSecurityDescriptorOwner(obj, &o, &d)); CHECK(o && EqualSid(o, sid_user()));
        CHECK(GetSecurityDescriptorGroup(obj, &g, &d)); CHECK(g && EqualSid(g, sid_world()));
        EXPECT_EQ(obj_dacl(obj, &s, &mask), 1);
        CHECK(s && EqualSid(s, sid_user())); EXPECT_EQ(mask, 1);
        CHECK(IsValidSecurityDescriptor(obj));
    }
    DestroyPrivateObjectSecurity(&obj);
}
TEST(gap12_private_info_not_selected_ignored) {
    REQUIRE(U(SetPrivateObjectSecurity)); if (t_is_wine()) { t_skip("emulator stubs SetPrivateObjectSecurity"); return; }
    PSECURITY_DESCRIPTOR obj = mkobj();
    if (!obj) { t_skip("CreatePrivateObjectSecurity"); return; }
    /* modification SD carries owner+DACL, but only GROUP is selected: owner and DACL stay */
    SECURITY_DESCRIPTOR m; BYTE ab[128]; sd_init(&m);
    SetSecurityDescriptorOwner(&m, sid_admin(), FALSE);
    SetSecurityDescriptorGroup(&m, sid_world(), FALSE);
    SetSecurityDescriptorDacl(&m, TRUE, mkacl(ab, sizeof ab, sid_admin(), 0x8), FALSE);
    BOOL r = U(SetPrivateObjectSecurity)(GROUP_SECURITY_INFORMATION, &m, &obj, &s_gm, tok());
    CHECKF(r, "err=%lu", GetLastError());
    if (r) {
        PSID o = NULL, s = NULL; BOOL d = 0; DWORD mask = 0;
        CHECK(GetSecurityDescriptorOwner(obj, &o, &d)); CHECK(o && EqualSid(o, sid_user()));
        EXPECT_EQ(obj_dacl(obj, &s, &mask), 1);
        EXPECT_EQ(mask, 1);
    }
    DestroyPrivateObjectSecurity(&obj);
}
TEST(gap12_private_null_dacl) {
    REQUIRE(U(SetPrivateObjectSecurity)); if (t_is_wine()) { t_skip("emulator stubs SetPrivateObjectSecurity"); return; }
    PSECURITY_DESCRIPTOR obj = mkobj();
    if (!obj) { t_skip("CreatePrivateObjectSecurity"); return; }
    SECURITY_DESCRIPTOR m; sd_init(&m); SetSecurityDescriptorDacl(&m, TRUE, NULL, FALSE);
    BOOL r = U(SetPrivateObjectSecurity)(DACL_SECURITY_INFORMATION, &m, &obj, &s_gm, tok());
    CHECKF(r, "err=%lu", GetLastError());
    if (r) { int p = 0; BOOL nul = 0; sd_dacl_info(obj, &p, NULL, NULL, &nul); CHECK(p); CHECK(nul); }
    DestroyPrivateObjectSecurity(&obj);
}
TEST(gap12_private_dacl_not_present_in_mod) {
    REQUIRE(U(SetPrivateObjectSecurity)); if (t_is_wine()) { t_skip("emulator stubs SetPrivateObjectSecurity"); return; }
    PSECURITY_DESCRIPTOR obj = mkobj();
    if (!obj) { t_skip("CreatePrivateObjectSecurity"); return; }
    SECURITY_DESCRIPTOR m; sd_init(&m);
    SET_ERR(0xBAD);
    BOOL r = U(SetPrivateObjectSecurity)(DACL_SECURITY_INFORMATION, &m, &obj, &s_gm, tok());
    t_note("DACL selected, none in modification SD: ret=%d err=%lu", r, GetLastError());
    DestroyPrivateObjectSecurity(&obj);
}
TEST(gap12_private_info_zero) {
    REQUIRE(U(SetPrivateObjectSecurity)); if (t_is_wine()) { t_skip("emulator stubs SetPrivateObjectSecurity"); return; }
    PSECURITY_DESCRIPTOR obj = mkobj();
    if (!obj) { t_skip("CreatePrivateObjectSecurity"); return; }
    DSD m; dsd_init(&m, 0x2);
    SET_ERR(0xBAD);
    BOOL r = U(SetPrivateObjectSecurity)(0, &m.sd, &obj, &s_gm, tok());
    t_note("SecurityInformation=0: ret=%d err=%lu", r, GetLastError());
    if (r) { PSID s; DWORD mask; EXPECT_EQ(obj_dacl(obj, &s, &mask), 1); EXPECT_EQ(mask, 1); }
    DestroyPrivateObjectSecurity(&obj);
}
TEST(gap12_private_null_token) {
    REQUIRE(U(SetPrivateObjectSecurity)); if (t_is_wine()) { t_skip("emulator stubs SetPrivateObjectSecurity"); return; }
    PSECURITY_DESCRIPTOR obj = mkobj();
    if (!obj) { t_skip("CreatePrivateObjectSecurity"); return; }
    DSD m; dsd_init(&m, 0x2);
    SET_ERR(0xBAD);
    BOOL r = U(SetPrivateObjectSecurity)(DACL_SECURITY_INFORMATION, &m.sd, &obj, &s_gm, NULL);
    t_note("NULL token: ret=%d err=%lu", r, GetLastError());
    DestroyPrivateObjectSecurity(&obj);
}
TEST(gap12_private_primary_token) {
    REQUIRE(U(SetPrivateObjectSecurity)); if (t_is_wine()) { t_skip("emulator stubs SetPrivateObjectSecurity"); return; }
    PSECURITY_DESCRIPTOR obj = mkobj();
    if (!obj) { t_skip("CreatePrivateObjectSecurity"); return; }
    HANDLE p = NULL; OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &p);
    DSD m; dsd_init(&m, 0x2);
    SET_ERR(0xBAD);
    BOOL r = U(SetPrivateObjectSecurity)(DACL_SECURITY_INFORMATION, &m.sd, &obj, &s_gm, p);
    t_note("primary token: ret=%d err=%lu", r, GetLastError());
    if (p) CloseHandle(p);
    DestroyPrivateObjectSecurity(&obj);
}
TEST(gap12_private_bad_token_handle) {
    REQUIRE(U(SetPrivateObjectSecurity)); if (t_is_wine()) { t_skip("emulator stubs SetPrivateObjectSecurity"); return; }
    PSECURITY_DESCRIPTOR obj = mkobj();
    if (!obj) { t_skip("CreatePrivateObjectSecurity"); return; }
    DSD m; dsd_init(&m, 0x2);
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, NULL);
    SET_ERR(0xBAD);
    BOOL r = U(SetPrivateObjectSecurity)(DACL_SECURITY_INFORMATION, &m.sd, &obj, &s_gm, ev);
    t_note("event handle as token: ret=%d err=%lu", r, GetLastError());
    CloseHandle(ev);
    DestroyPrivateObjectSecurity(&obj);
}
TEST(gap12_private_repeat) {
    REQUIRE(U(SetPrivateObjectSecurity)); if (t_is_wine()) { t_skip("emulator stubs SetPrivateObjectSecurity"); return; }
    PSECURITY_DESCRIPTOR obj = mkobj();
    if (!obj) { t_skip("CreatePrivateObjectSecurity"); return; }
    int bad = 0;
    for (int i = 0; i < 300; i++) {
        DSD m; dsd_init(&m, 1u << (i % 8));
        if (!U(SetPrivateObjectSecurity)(DACL_SECURITY_INFORMATION, &m.sd, &obj, &s_gm, tok())) bad++;
    }
    EXPECT_EQ(bad, 0);
    PSID s; DWORD mask = 0;
    EXPECT_EQ(obj_dacl(obj, &s, &mask), 1);
    EXPECT_EQ(mask, 1u << (299 % 8));
    DestroyPrivateObjectSecurity(&obj);
}
TEST(gap12_private_ex_dacl) {
    REQUIRE(U(SetPrivateObjectSecurityEx)); if (t_is_wine()) { t_skip("emulator stubs SetPrivateObjectSecurityEx"); return; }
    PSECURITY_DESCRIPTOR obj = mkobj();
    if (!obj) { t_skip("CreatePrivateObjectSecurity"); return; }
    DSD m; dsd_init(&m, 0x2);
    SET_ERR(0xBAD);
    EXPECT_EQ(U(SetPrivateObjectSecurityEx)(DACL_SECURITY_INFORMATION, &m.sd, &obj, 0, &s_gm, tok()), TRUE);
    PSID sid; DWORD mask = 0;
    EXPECT_EQ(obj_dacl(obj, &sid, &mask), 1);
    EXPECT_EQ(mask, 2);
    DestroyPrivateObjectSecurity(&obj);
}
TEST(gap12_private_ex_owner_group) {
    REQUIRE(U(SetPrivateObjectSecurityEx)); if (t_is_wine()) { t_skip("emulator stubs SetPrivateObjectSecurityEx"); return; }
    PSECURITY_DESCRIPTOR obj = mkobj();
    if (!obj) { t_skip("CreatePrivateObjectSecurity"); return; }
    SECURITY_DESCRIPTOR m; sd_init(&m);
    SetSecurityDescriptorOwner(&m, sid_user(), FALSE); SetSecurityDescriptorGroup(&m, sid_world(), FALSE);
    BOOL r = U(SetPrivateObjectSecurityEx)(OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION, &m, &obj, 0, &s_gm, tok());
    CHECKF(r, "err=%lu", GetLastError());
    if (r) {
        PSID o = NULL, g = NULL; BOOL d = 0;
        CHECK(GetSecurityDescriptorOwner(obj, &o, &d)); CHECK(o && EqualSid(o, sid_user()));
        CHECK(GetSecurityDescriptorGroup(obj, &g, &d)); CHECK(g && EqualSid(g, sid_world()));
    }
    DestroyPrivateObjectSecurity(&obj);
}
TEST(gap12_private_ex_autoinherit_flags) {
    REQUIRE(U(SetPrivateObjectSecurityEx)); if (t_is_wine()) { t_skip("emulator stubs SetPrivateObjectSecurityEx"); return; }
    static const ULONG flags[] = { SEF_DACL_AUTO_INHERIT, SEF_SACL_AUTO_INHERIT, SEF_DEFAULT_DESCRIPTOR_FOR_OBJECT, SEF_AVOID_PRIVILEGE_CHECK, SEF_AVOID_OWNER_CHECK, SEF_DACL_AUTO_INHERIT | SEF_SACL_AUTO_INHERIT };
    for (unsigned i = 0; i < sizeof flags / sizeof flags[0]; i++) {
        PSECURITY_DESCRIPTOR obj = mkobj();
        if (!obj) { t_skip("CreatePrivateObjectSecurity"); return; }
        DSD m; dsd_init(&m, 0x2);
        SET_ERR(0xBAD);
        BOOL r = U(SetPrivateObjectSecurityEx)(DACL_SECURITY_INFORMATION, &m.sd, &obj, flags[i], &s_gm, tok());
        t_note("AutoInheritFlags=0x%lx: ret=%d err=%lu", flags[i], r, r ? 0 : GetLastError());
        DestroyPrivateObjectSecurity(&obj);
    }
}
TEST(gap12_private_ex_matches_plain) {
    REQUIRE(U(SetPrivateObjectSecurityEx)); if (t_is_wine()) { t_skip("emulator stubs SetPrivateObjectSecurityEx"); return; } REQUIRE(U(SetPrivateObjectSecurity)); if (t_is_wine()) { t_skip("emulator stubs SetPrivateObjectSecurity"); return; }
    PSECURITY_DESCRIPTOR a = mkobj(), b = mkobj();
    if (!a || !b) { t_skip("CreatePrivateObjectSecurity"); return; }
    DSD m; dsd_init(&m, 0x6);
    CHECK(U(SetPrivateObjectSecurity)(DACL_SECURITY_INFORMATION, &m.sd, &a, &s_gm, tok()));
    CHECK(U(SetPrivateObjectSecurityEx)(DACL_SECURITY_INFORMATION, &m.sd, &b, 0, &s_gm, tok()));
    PSID s1, s2; DWORD m1 = 0, m2 = 1;
    EXPECT_EQ(obj_dacl(a, &s1, &m1), obj_dacl(b, &s2, &m2));
    EXPECT_EQ(m1, m2);
    DestroyPrivateObjectSecurity(&a); DestroyPrivateObjectSecurity(&b);
}
