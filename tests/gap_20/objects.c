/* gap20: named-object directory, object attribute formatting, delete marking, exe arch, actctx workers. */
#include "t.h"

typedef NTSTATUS (WINAPI *BaseGetNamedObjectDirectory_fn)(HANDLE *);
typedef NTSTATUS (WINAPI *BaseFormatObjectAttributes_fn)(OBJECT_ATTRIBUTES *, SECURITY_ATTRIBUTES *, PUNICODE_STRING, OBJECT_ATTRIBUTES **);
typedef void (WINAPI *BaseMarkFileForDelete_fn)(HANDLE, ULONG);
typedef LONG (WINAPI *BasepGetExeArchType_fn)(WORD *, HANDLE, PCWSTR);
typedef NTSTATUS (WINAPI *NtOpenEvent_fn)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES);
typedef NTSTATUS (WINAPI *NtQueryObject_fn)(HANDLE, int, PVOID, ULONG, PULONG);
typedef BOOL (WINAPI *ActivateActCtxWorker_fn)(HANDLE, ULONG_PTR *);
typedef void (WINAPI *AddRefActCtxWorker_fn)(HANDLE);
typedef DWORD (WINAPI *AddLocalAltA_fn)(LPCSTR, int);
typedef DWORD (WINAPI *AddLocalAltW_fn)(LPCWSTR, int);
typedef BOOL (WINAPI *AdjustCalendarDate_fn)(void *, int, int);

#define ST_INVALID_PARAMETER ((NTSTATUS)0xC000000D)

/* ---- BaseGetNamedObjectDirectory ---- */
static NTSTATUS getdir(const char *mod, HANDLE *h) {
    BaseGetNamedObjectDirectory_fn f = (BaseGetNamedObjectDirectory_fn)(mod[0] == 'k' && mod[1] == '3' ? t_proc_k32("BaseGetNamedObjectDirectory") : t_proc_kb("BaseGetNamedObjectDirectory"));
    return f(h);
}
TEST(gap20_getnamedobjectdir_null_k32) {
    REQUIRE(t_proc_k32("BaseGetNamedObjectDirectory"));
    if (t_is_wine()) { t_skip("emulator faults on NULL"); return; }
    CHECK(getdir("k32", NULL) == ST_INVALID_PARAMETER);
}
TEST(gap20_getnamedobjectdir_null_kb) {
    REQUIRE(t_proc_kb("BaseGetNamedObjectDirectory"));
    if (t_is_wine()) { t_skip("emulator faults on NULL"); return; }
    CHECK(getdir("kb", NULL) == ST_INVALID_PARAMETER);
}
TEST(gap20_getnamedobjectdir_returns_handle) {
    REQUIRE(t_proc_kb("BaseGetNamedObjectDirectory"));
    HANDLE h = NULL;
    NTSTATUS st = getdir("kb", &h);
    CHECKF(st == 0, "status %08lx", (unsigned long)st);
    CHECK(h != NULL);
}
TEST(gap20_getnamedobjectdir_cached_same_handle) {
    REQUIRE(t_proc_kb("BaseGetNamedObjectDirectory"));
    HANDLE a = NULL, b = NULL, c = NULL;
    CHECK(getdir("kb", &a) == 0);
    CHECK(getdir("kb", &b) == 0);
    if (t_proc_k32("BaseGetNamedObjectDirectory")) { CHECK(getdir("k32", &c) == 0); }
    CHECK(a == b);
    if (c) CHECK(a == c);
}
TEST(gap20_getnamedobjectdir_is_directory_object) {
    REQUIRE(t_proc_kb("BaseGetNamedObjectDirectory"));
    NtQueryObject_fn q = (NtQueryObject_fn)t_proc_nt("NtQueryObject"); REQUIRE(q);
    HANDLE h = NULL;
    if (getdir("kb", &h) != 0 || !h) { CHECK(0); return; }
    BYTE buf[512]; ULONG rl = 0;
    NTSTATUS st = q(h, 2 /* ObjectTypeInformation */, buf, sizeof buf, &rl);
    CHECKF(st == 0, "NtQueryObject %08lx", (unsigned long)st);
    if (st == 0) {
        UNICODE_STRING *tn = (UNICODE_STRING *)buf;
        CHECK(tn->Length == 18);
        CHECK(tn->Buffer && !memcmp(tn->Buffer, L"Directory", 18));
    }
}
TEST(gap20_getnamedobjectdir_names_resolve_relative) {
    REQUIRE(t_proc_kb("BaseGetNamedObjectDirectory"));
    NtOpenEvent_fn op = (NtOpenEvent_fn)t_proc_nt("NtOpenEvent"); REQUIRE(op);
    WCHAR nm[64]; wsprintfW(nm, L"k32t_gap20_dir_%lu", GetCurrentProcessId());
    HANDLE ev = CreateEventW(NULL, TRUE, FALSE, nm);
    CHECK(ev != NULL);
    HANDLE dir = NULL;
    CHECK(getdir("kb", &dir) == 0);
    UNICODE_STRING us; us.Buffer = nm; us.Length = (USHORT)(lstrlenW(nm) * 2); us.MaximumLength = us.Length + 2;
    OBJECT_ATTRIBUTES oa; memset(&oa, 0, sizeof oa);
    oa.Length = sizeof oa; oa.RootDirectory = dir; oa.ObjectName = &us; oa.Attributes = 0x40;
    HANDLE h2 = NULL;
    NTSTATUS st = op(&h2, 0x1, &oa);
    if (t_is_wine()) t_note("NtOpenEvent relative -> %08lx", (unsigned long)st);
    else CHECKF(st == 0, "NtOpenEvent %08lx", (unsigned long)st);
    if (h2) CloseHandle(h2);
    if (ev) CloseHandle(ev);
}

/* ---- BaseFormatObjectAttributes ---- */
static BaseFormatObjectAttributes_fn fmt_fn(void) { return (BaseFormatObjectAttributes_fn)t_proc_kb("BaseFormatObjectAttributes"); }
TEST(gap20_formatoa_null_null) {
    BaseFormatObjectAttributes_fn f = fmt_fn(); REQUIRE(f);
    OBJECT_ATTRIBUTES oa, *out = (OBJECT_ATTRIBUTES *)(ULONG_PTR)0x1234;
    memset(&oa, 0xCC, sizeof oa);
    NTSTATUS st = f(&oa, NULL, NULL, &out);
    CHECK(st == 0);
    CHECK(out == NULL);
    /* nothing is written to the attribute block in this case */
    CHECK(oa.Length == 0xCCCCCCCC);
}
TEST(gap20_formatoa_name_only) {
    BaseFormatObjectAttributes_fn f = fmt_fn(); REQUIRE(f);
    HANDLE dir = NULL; REQUIRE(t_proc_kb("BaseGetNamedObjectDirectory"));
    CHECK(getdir("kb", &dir) == 0);
    UNICODE_STRING us; static WCHAR w[] = L"k32t_gap20_fmt";
    us.Buffer = w; us.Length = 28; us.MaximumLength = 30;
    OBJECT_ATTRIBUTES oa, *out = NULL; memset(&oa, 0xCC, sizeof oa);
    NTSTATUS st = f(&oa, NULL, &us, &out);
    CHECKF(st == 0, "st=%08lx", (unsigned long)st);
    CHECK(out == &oa);
    CHECK(oa.Length == sizeof(OBJECT_ATTRIBUTES));
    CHECK(oa.ObjectName == &us);
    CHECK(oa.RootDirectory == dir);
    CHECK(oa.SecurityDescriptor == NULL);
    CHECK(oa.SecurityQualityOfService == NULL);
    CHECK((oa.Attributes & 0x80) != 0);          /* OBJ_OPENIF */
    CHECK((oa.Attributes & OBJ_INHERIT) == 0);
}
TEST(gap20_formatoa_inherit_flag) {
    BaseFormatObjectAttributes_fn f = fmt_fn(); REQUIRE(f);
    SECURITY_ATTRIBUTES sa; memset(&sa, 0, sizeof sa);
    sa.nLength = sizeof sa; sa.bInheritHandle = TRUE; sa.lpSecurityDescriptor = (PVOID)(ULONG_PTR)0x7777000;
    OBJECT_ATTRIBUTES oa, *out = NULL; memset(&oa, 0xCC, sizeof oa);
    NTSTATUS st = f(&oa, &sa, NULL, &out);
    CHECKF(st == 0, "st=%08lx", (unsigned long)st);
    CHECK(out == &oa);
    CHECK(oa.Length == sizeof(OBJECT_ATTRIBUTES));
    CHECK(oa.RootDirectory == NULL);
    CHECK(oa.ObjectName == NULL);
    CHECK(oa.SecurityDescriptor == sa.lpSecurityDescriptor);
    CHECK((oa.Attributes & OBJ_INHERIT) != 0);
    CHECK((oa.Attributes & 0x80) == 0);
    CHECK(oa.SecurityQualityOfService == NULL);
}
TEST(gap20_formatoa_not_inherit) {
    BaseFormatObjectAttributes_fn f = fmt_fn(); REQUIRE(f);
    SECURITY_ATTRIBUTES sa; memset(&sa, 0, sizeof sa);
    sa.nLength = sizeof sa; sa.bInheritHandle = FALSE; sa.lpSecurityDescriptor = (PVOID)(ULONG_PTR)0x5000;
    OBJECT_ATTRIBUTES oa, *out = NULL; memset(&oa, 0xCC, sizeof oa);
    CHECK(f(&oa, &sa, NULL, &out) == 0);
    CHECK(out == &oa);
    CHECK((oa.Attributes & OBJ_INHERIT) == 0);
    CHECK(oa.SecurityDescriptor == sa.lpSecurityDescriptor);
}
TEST(gap20_formatoa_inherit_and_name) {
    BaseFormatObjectAttributes_fn f = fmt_fn(); REQUIRE(f);
    HANDLE dir = NULL; REQUIRE(t_proc_kb("BaseGetNamedObjectDirectory"));
    CHECK(getdir("kb", &dir) == 0);
    SECURITY_ATTRIBUTES sa; memset(&sa, 0, sizeof sa);
    sa.nLength = sizeof sa; sa.bInheritHandle = TRUE;
    UNICODE_STRING us; static WCHAR w[] = L"x";
    us.Buffer = w; us.Length = 2; us.MaximumLength = 4;
    OBJECT_ATTRIBUTES oa, *out = NULL; memset(&oa, 0xCC, sizeof oa);
    CHECK(f(&oa, &sa, &us, &out) == 0);
    CHECK(out == &oa);
    CHECK((oa.Attributes & (OBJ_INHERIT | 0x80)) == (OBJ_INHERIT | 0x80));
    CHECK(oa.RootDirectory == dir);
    CHECK(oa.ObjectName == &us);
    CHECK(oa.SecurityDescriptor == NULL);
}
TEST(gap20_formatoa_k32_forward_same_result) {
    BaseFormatObjectAttributes_fn a = (BaseFormatObjectAttributes_fn)t_proc_k32("BaseFormatObjectAttributes");
    BaseFormatObjectAttributes_fn b = fmt_fn();
    REQUIRE(a); REQUIRE(b);
    SECURITY_ATTRIBUTES sa; memset(&sa, 0, sizeof sa); sa.nLength = sizeof sa; sa.bInheritHandle = TRUE;
    OBJECT_ATTRIBUTES o1, o2, *p1 = NULL, *p2 = NULL; memset(&o1, 0xCC, sizeof o1); memset(&o2, 0xCC, sizeof o2);
    CHECK(a(&o1, &sa, NULL, &p1) == 0);
    CHECK(b(&o2, &sa, NULL, &p2) == 0);
    CHECK(memcmp(&o1, &o2, sizeof o1) == 0);
}

/* ---- BaseMarkFileForDelete ---- */
static HANDLE mkfile(const char *leaf, DWORD attrs, char *path, size_t cap) {
    t_tmpfile(path, cap, leaf);
    HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE | DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           NULL, CREATE_ALWAYS, attrs, NULL);
    if (h != INVALID_HANDLE_VALUE) { DWORD w; WriteFile(h, "data", 4, &w, NULL); }
    return h;
}
TEST(gap20_markfordelete_normal_file) {
    BaseMarkFileForDelete_fn f = (BaseMarkFileForDelete_fn)t_proc_kb("BaseMarkFileForDelete"); REQUIRE(f);
    char p[MAX_PATH]; HANDLE h = mkfile("g20_md1.tmp", FILE_ATTRIBUTE_NORMAL, p, sizeof p);
    CHECK(h != INVALID_HANDLE_VALUE); if (h == INVALID_HANDLE_VALUE) return;
    CHECK(GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES);
    f(h, 0);
    CloseHandle(h);
    CHECK(GetFileAttributesA(p) == INVALID_FILE_ATTRIBUTES);
    EXPECT_ERR(ERROR_FILE_NOT_FOUND);
}
TEST(gap20_markfordelete_deletes_only_on_close) {
    BaseMarkFileForDelete_fn f = (BaseMarkFileForDelete_fn)t_proc_kb("BaseMarkFileForDelete"); REQUIRE(f);
    char p[MAX_PATH]; HANDLE h = mkfile("g20_md2.tmp", FILE_ATTRIBUTE_NORMAL, p, sizeof p);
    CHECK(h != INVALID_HANDLE_VALUE); if (h == INVALID_HANDLE_VALUE) return;
    f(h, 0);
    /* The file is still visible until the last handle closes. */
    DWORD at = GetFileAttributesA(p);
    t_note("attributes while delete-pending: %08lx err=%lu", at, GetLastError());
    DWORD sz = GetFileSize(h, NULL);
    CHECK(sz == 4);
    CloseHandle(h);
    CHECK(GetFileAttributesA(p) == INVALID_FILE_ATTRIBUTES);
}
/* A read-only file cannot be opened with DELETE access, so the attribute is set while the handle is already open. */
static void ro_case(const char *leaf, ULONG flags, int expect_deleted) {
    BaseMarkFileForDelete_fn f = (BaseMarkFileForDelete_fn)t_proc_kb("BaseMarkFileForDelete");
    if (!f) { t_skip("missing export"); return; }
    char p[MAX_PATH]; HANDLE h = mkfile(leaf, FILE_ATTRIBUTE_NORMAL, p, sizeof p);
    CHECK(h != INVALID_HANDLE_VALUE); if (h == INVALID_HANDLE_VALUE) return;
    CHECK(SetFileAttributesA(p, FILE_ATTRIBUTE_READONLY));
    f(h, flags);
    CloseHandle(h);
    DWORD at = GetFileAttributesA(p);
    if (expect_deleted) {
        CHECKF(at == INVALID_FILE_ATTRIBUTES, "readonly file survived, attrs=%08lx", at);
    } else {
        CHECKF(at != INVALID_FILE_ATTRIBUTES && (at & FILE_ATTRIBUTE_READONLY), "attrs=%08lx", at);
    }
    if (at != INVALID_FILE_ATTRIBUTES) { SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL); DeleteFileA(p); }
}
TEST(gap20_markfordelete_readonly_flag_zero) { ro_case("g20_md3.tmp", 0, 1); }
TEST(gap20_markfordelete_readonly_flag_one) { ro_case("g20_md4.tmp", 1, 1); }
TEST(gap20_markfordelete_readonly_flag_three) { ro_case("g20_md4b.tmp", 3, 1); }
TEST(gap20_markfordelete_readonly_flag_without_clear) {
    if (t_is_wine()) { t_skip("read-only delete semantics differ on emulator"); return; }
    REQUIRE(t_proc_kb("BaseMarkFileForDelete"));
    ro_case("g20_md5.tmp", 2, 0);   /* bit 0 clear: read-only is not cleared and the disposition is rejected */
}
TEST(gap20_markfordelete_no_delete_access_leaves_file) {
    BaseMarkFileForDelete_fn f = (BaseMarkFileForDelete_fn)t_proc_kb("BaseMarkFileForDelete"); REQUIRE(f);
    char p[MAX_PATH]; HANDLE h = mkfile("g20_md6.tmp", FILE_ATTRIBUTE_NORMAL, p, sizeof p);
    CHECK(h != INVALID_HANDLE_VALUE); if (h == INVALID_HANDLE_VALUE) return;
    CloseHandle(h);
    h = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) { f(h, 0); CloseHandle(h); }
    CHECK(GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES);
    DeleteFileA(p);
}
TEST(gap20_markfordelete_invalid_handle_no_crash) {
    BaseMarkFileForDelete_fn f = (BaseMarkFileForDelete_fn)t_proc_kb("BaseMarkFileForDelete"); REQUIRE(f);
    f(INVALID_HANDLE_VALUE, 0);
    f(NULL, 0);
    f(INVALID_HANDLE_VALUE, 1);
    CHECK(1);
}
TEST(gap20_markfordelete_empty_file_and_dir_content) {
    BaseMarkFileForDelete_fn f = (BaseMarkFileForDelete_fn)t_proc_kb("BaseMarkFileForDelete"); REQUIRE(f);
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "g20_md7.tmp");
    HANDLE h = CreateFileA(p, GENERIC_READ | DELETE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE); if (h == INVALID_HANDLE_VALUE) return;
    f(h, 0);
    CloseHandle(h);
    CHECK(GetFileAttributesA(p) == INVALID_FILE_ATTRIBUTES);
}

/* ---- BasepGetExeArchType ---- */
TEST(gap20_exearchtype_system_kernel32_is_amd64) {
    BasepGetExeArchType_fn f = (BasepGetExeArchType_fn)t_proc_k32("BasepGetExeArchType"); REQUIRE(f);
    char sys[MAX_PATH]; GetSystemDirectoryA(sys, sizeof sys);
    lstrcatA(sys, "\\kernel32.dll");
    HANDLE h = CreateFileA(sys, GENERIC_READ | GENERIC_EXECUTE, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE); if (h == INVALID_HANDLE_VALUE) return;
    WORD m = 0x1234;
    LONG st = f(&m, h, NULL);
    if (t_is_wine()) { t_note("st=%08lx machine=%04x", (unsigned long)st, m); CloseHandle(h); return; }
    CHECKF(st >= 0, "st=%08lx", (unsigned long)st);
    CHECKF(m == IMAGE_FILE_MACHINE_AMD64, "machine %04x", m);
    CloseHandle(h);
}
TEST(gap20_exearchtype_system_ntdll_is_amd64) {
    BasepGetExeArchType_fn f = (BasepGetExeArchType_fn)t_proc_k32("BasepGetExeArchType"); REQUIRE(f);
    char sys[MAX_PATH]; GetSystemDirectoryA(sys, sizeof sys);
    lstrcatA(sys, "\\ntdll.dll");
    HANDLE h = CreateFileA(sys, GENERIC_READ | GENERIC_EXECUTE, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE); if (h == INVALID_HANDLE_VALUE) return;
    WORD m = 0;
    LONG st = f(&m, h, NULL);
    if (t_is_wine()) { t_note("st=%08lx machine=%04x", (unsigned long)st, m); CloseHandle(h); return; }
    CHECK(st >= 0);
    CHECK(m == IMAGE_FILE_MACHINE_AMD64);
    CloseHandle(h);
}
TEST(gap20_exearchtype_self_exe_is_amd64) {
    BasepGetExeArchType_fn f = (BasepGetExeArchType_fn)t_proc_k32("BasepGetExeArchType"); REQUIRE(f);
    HANDLE h = CreateFileA(t_self_path(), GENERIC_READ | GENERIC_EXECUTE, FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE); if (h == INVALID_HANDLE_VALUE) return;
    WORD m = 0;
    LONG st = f(&m, h, NULL);
    if (t_is_wine()) { t_note("st=%08lx machine=%04x", (unsigned long)st, m); CloseHandle(h); return; }
    CHECK(st >= 0);
    CHECK(m == IMAGE_FILE_MACHINE_AMD64);
    CloseHandle(h);
}
TEST(gap20_exearchtype_not_an_image) {
    BasepGetExeArchType_fn f = (BasepGetExeArchType_fn)t_proc_k32("BasepGetExeArchType"); REQUIRE(f);
    char p[MAX_PATH]; HANDLE h = mkfile("g20_arch.txt", FILE_ATTRIBUTE_NORMAL, p, sizeof p);
    CHECK(h != INVALID_HANDLE_VALUE); if (h == INVALID_HANDLE_VALUE) return;
    CloseHandle(h);
    h = CreateFileA(p, GENERIC_READ | GENERIC_EXECUTE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    if (h != INVALID_HANDLE_VALUE) {
        WORD m = 0x1234;
        LONG st = f(&m, h, NULL);
        CHECKF(st < 0, "st=%08lx", (unsigned long)st);
        CHECK(m == 0);
        CloseHandle(h);
    }
    DeleteFileA(p);
}
TEST(gap20_exearchtype_invalid_handle) {
    BasepGetExeArchType_fn f = (BasepGetExeArchType_fn)t_proc_k32("BasepGetExeArchType"); REQUIRE(f);
    WORD m = 0x1234;
    LONG st = f(&m, INVALID_HANDLE_VALUE, NULL);
    CHECKF(st < 0, "st=%08lx", (unsigned long)st);
    CHECK(m == 0);
    m = 0x1234;
    st = f(&m, NULL, NULL);
    CHECKF(st < 0, "st=%08lx", (unsigned long)st);
    CHECK(m == 0);
}
TEST(gap20_exearchtype_empty_file) {
    BasepGetExeArchType_fn f = (BasepGetExeArchType_fn)t_proc_k32("BasepGetExeArchType"); REQUIRE(f);
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "g20_arch_empty.exe");
    HANDLE h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE); if (h == INVALID_HANDLE_VALUE) return;
    WORD m = 0x1234;
    LONG st = f(&m, h, NULL);
    CHECKF(st < 0, "st=%08lx", (unsigned long)st);
    CHECK(m == 0);
    CloseHandle(h);
    DeleteFileA(p);
}

/* ---- ActivateActCtxWorker / AddRefActCtxWorker ---- */
static HANDLE make_actctx(void) {
    char mp[MAX_PATH]; t_tmpfile(mp, sizeof mp, "g20_actctx.manifest");
    static const char xml[] = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
        "<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">"
        "<assemblyIdentity type=\"win32\" name=\"k32t.gap20.actctx\" version=\"1.0.0.0\"/></assembly>";
    HANDLE f = CreateFileA(mp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return INVALID_HANDLE_VALUE;
    DWORD w; WriteFile(f, xml, sizeof xml - 1, &w, NULL); CloseHandle(f);
    ACTCTXA ac; memset(&ac, 0, sizeof ac); ac.cbSize = sizeof ac; ac.lpSource = mp;
    typedef HANDLE (WINAPI *cac_t)(PCACTCTXA);
    cac_t cac = (cac_t)t_proc("CreateActCtxA");
    if (!cac) return INVALID_HANDLE_VALUE;
    HANDLE h = cac(&ac);
    DeleteFileA(mp);
    return h;
}
TEST(gap20_activateactctxworker_roundtrip) {
    ActivateActCtxWorker_fn act = (ActivateActCtxWorker_fn)t_proc_k32("ActivateActCtxWorker"); REQUIRE(act);
    REQUIRE(t_proc("DeactivateActCtx")); REQUIRE(t_proc("ReleaseActCtx")); REQUIRE(t_proc("GetCurrentActCtx"));
    typedef BOOL (WINAPI *deact_t)(DWORD, ULONG_PTR); typedef void (WINAPI *rel_t)(HANDLE); typedef BOOL (WINAPI *cur_t)(HANDLE *);
    deact_t deact = (deact_t)t_proc("DeactivateActCtx"); rel_t rel = (rel_t)t_proc("ReleaseActCtx"); cur_t cur = (cur_t)t_proc("GetCurrentActCtx");
    HANDLE h = make_actctx();
    if (h == INVALID_HANDLE_VALUE) { t_skip("CreateActCtx unavailable (err %lu)", GetLastError()); return; }
    ULONG_PTR cookie = 0;
    BOOL r = act(h, &cookie);
    CHECK(r == TRUE);
    CHECK(cookie != 0);
    HANDLE c = NULL;
    CHECK(cur(&c));
    CHECK(c == h);
    if (c) rel(c);
    CHECK(deact(0, cookie));
    HANDLE c2 = (HANDLE)1;
    CHECK(cur(&c2));
    CHECK(c2 != h);
    if (c2 && c2 != (HANDLE)1) rel(c2);
    rel(h);
}
TEST(gap20_activateactctxworker_invalid_handle) {
    ActivateActCtxWorker_fn act = (ActivateActCtxWorker_fn)t_proc_k32("ActivateActCtxWorker"); REQUIRE(act);
    ULONG_PTR cookie = 0x77;
    SetLastError(0);
    BOOL r = act(INVALID_HANDLE_VALUE, &cookie);
    CHECK(r == FALSE);
    t_note("ActivateActCtxWorker(-1) last error %lu cookie %llx", GetLastError(), (unsigned long long)cookie);
}
TEST(gap20_activateactctxworker_nested) {
    ActivateActCtxWorker_fn act = (ActivateActCtxWorker_fn)t_proc_k32("ActivateActCtxWorker"); REQUIRE(act);
    REQUIRE(t_proc("DeactivateActCtx")); REQUIRE(t_proc("ReleaseActCtx"));
    typedef BOOL (WINAPI *deact_t)(DWORD, ULONG_PTR); typedef void (WINAPI *rel_t)(HANDLE);
    deact_t deact = (deact_t)t_proc("DeactivateActCtx"); rel_t rel = (rel_t)t_proc("ReleaseActCtx");
    HANDLE h = make_actctx();
    if (h == INVALID_HANDLE_VALUE) { t_skip("CreateActCtx unavailable"); return; }
    ULONG_PTR c1 = 0, c2 = 0;
    CHECK(act(h, &c1)); CHECK(act(h, &c2));
    CHECK(c1 != 0 && c2 != 0 && c1 != c2);
    CHECK(deact(0, c2)); CHECK(deact(0, c1));
    rel(h);
}
TEST(gap20_addrefactctxworker_balances_release) {
    AddRefActCtxWorker_fn ar = (AddRefActCtxWorker_fn)t_proc_k32("AddRefActCtxWorker"); REQUIRE(ar);
    REQUIRE(t_proc("ReleaseActCtx")); REQUIRE(t_proc("ActivateActCtx")); REQUIRE(t_proc("DeactivateActCtx"));
    typedef void (WINAPI *rel_t)(HANDLE); typedef BOOL (WINAPI *act_t)(HANDLE, ULONG_PTR *); typedef BOOL (WINAPI *deact_t)(DWORD, ULONG_PTR);
    rel_t rel = (rel_t)t_proc("ReleaseActCtx"); act_t act = (act_t)t_proc("ActivateActCtx"); deact_t deact = (deact_t)t_proc("DeactivateActCtx");
    HANDLE h = make_actctx();
    if (h == INVALID_HANDLE_VALUE) { t_skip("CreateActCtx unavailable"); return; }
    ar(h); ar(h);
    rel(h); rel(h);
    /* still alive after two extra releases: the original reference remains */
    ULONG_PTR ck = 0;
    CHECK(act(h, &ck));
    CHECK(ck != 0);
    CHECK(deact(0, ck));
    rel(h);
}

/* ---- AddLocalAlternateComputerName{A,W} (error paths only; nothing is registered) ---- */
TEST(gap20_addlocalaltcomputername_w_null) {
    AddLocalAltW_fn f = (AddLocalAltW_fn)t_proc_k32("AddLocalAlternateComputerNameW"); REQUIRE(f);
    CHECK(f(NULL, 0) == ERROR_INVALID_PARAMETER);
    CHECK(f(NULL, 1) == ERROR_INVALID_PARAMETER);
}
TEST(gap20_addlocalaltcomputername_w_nonzero_flags) {
    AddLocalAltW_fn f = (AddLocalAltW_fn)t_proc_k32("AddLocalAlternateComputerNameW"); REQUIRE(f);
    CHECK(f(L"k32t-gap20.example.invalid", 1) == ERROR_INVALID_PARAMETER);
    CHECK(f(L"k32t-gap20.example.invalid", -1) == ERROR_INVALID_PARAMETER);
    CHECK(f(L"k32t-gap20.example.invalid", 0x100) == ERROR_INVALID_PARAMETER);
}
TEST(gap20_addlocalaltcomputername_a_null) {
    AddLocalAltA_fn f = (AddLocalAltA_fn)t_proc_k32("AddLocalAlternateComputerNameA"); REQUIRE(f);
    CHECK(f(NULL, 0) == ERROR_INVALID_PARAMETER);
    CHECK(f(NULL, 7) == ERROR_INVALID_PARAMETER);
}
TEST(gap20_addlocalaltcomputername_a_nonzero_flags) {
    AddLocalAltA_fn f = (AddLocalAltA_fn)t_proc_k32("AddLocalAlternateComputerNameA"); REQUIRE(f);
    CHECK(f("k32t-gap20.example.invalid", 1) == ERROR_INVALID_PARAMETER);
    CHECK(f("k32t-gap20.example.invalid", 0x80000000) == ERROR_INVALID_PARAMETER);
}

/* ---- AdjustCalendarDate (invalid-argument paths) ---- */
TEST(gap20_adjustcalendardate_null) {
    AdjustCalendarDate_fn f = (AdjustCalendarDate_fn)t_proc_k32("AdjustCalendarDate"); REQUIRE(f);
    for (int unit = 0; unit <= 5; unit++) {
        SetLastError(0);
        CHECK(f(NULL, unit, 1) == FALSE);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}
TEST(gap20_adjustcalendardate_invalid_calid) {
    AdjustCalendarDate_fn f = (AdjustCalendarDate_fn)t_proc_k32("AdjustCalendarDate"); REQUIRE(f);
    static const DWORD ids[] = { 0, 13, 14, 20, 21, 24, 100, 0x7777, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        DWORD buf[16]; memset(buf, 0, sizeof buf); buf[0] = ids[i];
        SetLastError(0);
        BOOL r = f(buf, 4, 1);
        CHECKF(r == FALSE, "calid %lu", ids[i]);
        CHECKF(GetLastError() == ERROR_INVALID_PARAMETER, "calid %lu err %lu", ids[i], GetLastError());
    }
}
