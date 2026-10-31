/* gap24: security-descriptor helpers, unlock/OOBE queries, state/package opens, string loading, pipes. */
#include "t.h"
#include <sddl.h>
#define PK(name) ((name##_pfn)t_proc_k32(#name))
#define PB(name) ((name##_pfn)t_proc_kb(#name))

/* ---------- IsValidRelativeSecurityDescriptor ---------- */
static PSECURITY_DESCRIPTOR make_sd(const WCHAR *sddl, DWORD *len) {
    PSECURITY_DESCRIPTOR sd = NULL;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, SDDL_REVISION_1, &sd, NULL)) return NULL;
    *len = GetSecurityDescriptorLength(sd);
    return sd;
}
TEST(gap24_relsd_valid_and_truncated) {
    BOOL (WINAPI *f)(PSECURITY_DESCRIPTOR, ULONG, SECURITY_INFORMATION) = (void *)t_proc_kb("IsValidRelativeSecurityDescriptor"); REQUIRE(f);
    if (t_is_wine()) { t_skip("IsValidRelativeSecurityDescriptor is a wine stub"); return; }
    DWORD len; PSECURITY_DESCRIPTOR sd = make_sd(L"O:BAG:SYD:(A;;GA;;;WD)", &len); REQUIRE(sd);
    CHECK(f(sd, len, 0));
    CHECK(f(sd, len, OWNER_SECURITY_INFORMATION));
    CHECK(f(sd, len, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION));
    CHECK(f(sd, len, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION));
    CHECK(f(sd, len + 100, 0));    /* longer than needed is fine */
    for (ULONG l = 0; l < 20; l++) { SetLastError(0); CHECKF(!f(sd, l, 0), "len %lu", l); EXPECT_ERR(ERROR_INVALID_SECURITY_DESCR); }
    SetLastError(0); CHECK(!f(sd, 20, OWNER_SECURITY_INFORMATION)); EXPECT_ERR(ERROR_INVALID_SECURITY_DESCR);
    SetLastError(0); BOOL tr = f(sd, len - 1, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION);
    t_note("length-1 -> %d err %lu", tr, GetLastError());
    LocalFree(sd);
}
TEST(gap24_relsd_missing_required_parts) {
    BOOL (WINAPI *f)(PSECURITY_DESCRIPTOR, ULONG, SECURITY_INFORMATION) = (void *)t_proc_kb("IsValidRelativeSecurityDescriptor"); REQUIRE(f);
    if (t_is_wine()) { t_skip("IsValidRelativeSecurityDescriptor is a wine stub"); return; }
    DWORD len; PSECURITY_DESCRIPTOR sd = make_sd(L"O:BA", &len); REQUIRE(sd);
    CHECK(f(sd, len, OWNER_SECURITY_INFORMATION));
    SetLastError(0); CHECK(!f(sd, len, GROUP_SECURITY_INFORMATION)); EXPECT_ERR(ERROR_INVALID_SECURITY_DESCR);
    SetLastError(0); CHECK(!f(sd, len, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION)); EXPECT_ERR(ERROR_INVALID_SECURITY_DESCR);
    LocalFree(sd);
}
TEST(gap24_relsd_minimal_header) {
    BOOL (WINAPI *f)(PSECURITY_DESCRIPTOR, ULONG, SECURITY_INFORMATION) = (void *)t_proc_kb("IsValidRelativeSecurityDescriptor"); REQUIRE(f);
    if (t_is_wine()) { t_skip("IsValidRelativeSecurityDescriptor is a wine stub"); return; }
    BYTE sd[64]; memset(sd, 0, sizeof sd);
    sd[0] = 1; sd[2] = 0x00; sd[3] = 0x80;   /* revision 1, SE_SELF_RELATIVE */
    CHECK(f(sd, 20, 0));
    CHECK(f(sd, 64, 0));
    sd[0] = 2;                               /* bad revision */
    SetLastError(0); BOOL r = f(sd, 20, 0); t_note("revision 2 -> %d err %lu", r, GetLastError()); CHECK(!r);
    sd[0] = 1; sd[3] = 0;                    /* not self-relative */
    SetLastError(0); r = f(sd, 20, 0); t_note("absolute control -> %d err %lu", r, GetLastError());
}
TEST(gap24_relsd_null_and_zero) {
    BOOL (WINAPI *f)(PSECURITY_DESCRIPTOR, ULONG, SECURITY_INFORMATION) = (void *)t_proc_kb("IsValidRelativeSecurityDescriptor"); REQUIRE(f);
    if (t_is_wine()) { t_skip("IsValidRelativeSecurityDescriptor is a wine stub"); return; }
    SetLastError(0); CHECK(!f(NULL, 0, 0)); EXPECT_ERR(ERROR_INVALID_SECURITY_DESCR);
}

/* ---------- MakeAbsoluteSD2 ---------- */
TEST(gap24_makeabsolutesd2_error_paths) {
    BOOL (WINAPI *f)(PSECURITY_DESCRIPTOR, LPDWORD) = (void *)t_proc_kb("MakeAbsoluteSD2"); REQUIRE(f);
    if (t_is_wine()) { t_skip("MakeAbsoluteSD2 is a wine stub"); return; }
    BYTE sd[0x80]; memset(sd, 0, sizeof sd); sd[0] = 1; sd[3] = 0x80;
    DWORD sz = sizeof sd;
    SetLastError(0); CHECK(!f(NULL, &sz)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SetLastError(0); CHECK(!f(NULL, NULL)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SetLastError(0); CHECK(!f(sd, NULL)); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    DWORD small[] = {0, 1, 0x10, 0x13};
    for (size_t i = 0; i < 4; i++) { sz = small[i]; SetLastError(0); CHECKF(!f(sd, &sz), "size %lu", small[i]); EXPECT_ERR(ERROR_INVALID_PARAMETER); EXPECT_EQ(sz, small[i]); }
    sz = 0x14; SetLastError(0);
    CHECK(!f(sd, &sz)); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(sz, 0x28);                                     /* required absolute size */
    EXPECT_EQ(sd[3] & 0x80, 0x80);                           /* untouched on failure */
}
TEST(gap24_makeabsolutesd2_empty_descriptor) {
    BOOL (WINAPI *f)(PSECURITY_DESCRIPTOR, LPDWORD) = (void *)t_proc_kb("MakeAbsoluteSD2"); REQUIRE(f);
    if (t_is_wine()) { t_skip("MakeAbsoluteSD2 is a wine stub"); return; }
    BYTE sd[0x80]; memset(sd, 0, sizeof sd); sd[0] = 1; sd[3] = 0x80;
    DWORD sz = sizeof sd;
    BOOL r = f(sd, &sz);
    CHECKF(r, "err %lu", GetLastError());
    if (r) {
        WORD ctl = *(WORD *)(sd + 2);
        CHECK(!(ctl & SE_SELF_RELATIVE));
        EXPECT_EQ(sd[0], 1);
        BOOL (WINAPI *isv)(PSECURITY_DESCRIPTOR) = (void *)t_proc("IsValidSecurityDescriptor");
        if (isv) CHECK(isv(sd));
    }
}
TEST(gap24_makeabsolutesd2_with_acl) {
    BOOL (WINAPI *f)(PSECURITY_DESCRIPTOR, LPDWORD) = (void *)t_proc_kb("MakeAbsoluteSD2"); REQUIRE(f);
    if (t_is_wine()) { t_skip("MakeAbsoluteSD2 is a wine stub"); return; }
    DWORD len; PSECURITY_DESCRIPTOR rel = make_sd(L"O:BAG:SYD:(A;;GA;;;WD)", &len); REQUIRE(rel);
    BYTE *buf = (BYTE *)LocalAlloc(LPTR, len + 0x100);
    memcpy(buf, rel, len);
    DWORD sz = len;   /* too small for the absolute form (extra 0x14 header) */
    SetLastError(0);
    BOOL r = f(buf, &sz);
    if (!r) { EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER); CHECKF(sz > len, "required %lu vs %lu", sz, len); }
    else t_note("relative SD of %lu bytes fit absolute conversion in place", len);
    memcpy(buf, rel, len);
    sz = len + 0x100;
    r = f(buf, &sz);
    CHECKF(r, "conversion failed err %lu", GetLastError());
    if (r) {
        SECURITY_DESCRIPTOR_CONTROL c; DWORD rev;
        CHECK(GetSecurityDescriptorControl(buf, &c, &rev));
        CHECK(!(c & SE_SELF_RELATIVE));
        PSID o = NULL; BOOL def; CHECK(GetSecurityDescriptorOwner(buf, &o, &def)); CHECK(o != NULL);
        PSID g = NULL; CHECK(GetSecurityDescriptorGroup(buf, &g, &def)); CHECK(g != NULL);
        BOOL pr, dd; PACL acl = NULL; CHECK(GetSecurityDescriptorDacl(buf, &pr, &acl, &dd)); CHECK(pr && acl);
        CHECK(IsValidSecurityDescriptor(buf));
    }
    LocalFree(buf); LocalFree(rel);
}

/* ---------- unlock / developer mode / OOBE ---------- */
UDECL(HRESULT, IsDeveloperModeEnabled, (UINT *));
UDECL(HRESULT, IsSideloadingEnabled, (UINT *));
UDECL(void, IsDeveloperModePolicyApplied, (UINT *));
UDECL(void, IsSideloadingPolicyApplied, (UINT *));
TEST(gap24_developer_mode_null) {
    IsDeveloperModeEnabled_pfn f = PB(IsDeveloperModeEnabled); REQUIRE(f);
    EXPECT_EQ((DWORD)f(NULL), 0x80004003);   /* E_POINTER */
}
TEST(gap24_developer_mode_query) {
    IsDeveloperModeEnabled_pfn f = PB(IsDeveloperModeEnabled); REQUIRE(f);
    UINT v = 0xdeadbeef; HRESULT hr = f(&v);
    t_note("IsDeveloperModeEnabled hr=0x%lx value=%u", (DWORD)hr, v);
    if (SUCCEEDED(hr)) CHECK(v == 0 || v == 1);
    UINT w = 0xdeadbeef; HRESULT hr2 = f(&w);
    CHECK(hr2 == hr); if (SUCCEEDED(hr)) CHECK(w == v);
}
TEST(gap24_sideloading_null) {
    IsSideloadingEnabled_pfn f = PB(IsSideloadingEnabled); REQUIRE(f);
    EXPECT_EQ((DWORD)f(NULL), 0x80004003);
}
TEST(gap24_sideloading_query) {
    IsSideloadingEnabled_pfn f = PB(IsSideloadingEnabled); REQUIRE(f);
    UINT v = 0xdeadbeef; HRESULT hr = f(&v);
    t_note("IsSideloadingEnabled hr=0x%lx value=%u", (DWORD)hr, v);
    if (SUCCEEDED(hr)) CHECK(v == 0 || v == 1);
    UINT w = 0xdeadbeef; CHECK(f(&w) == hr);
}
TEST(gap24_unlock_policy_applied_values) {
    IsDeveloperModePolicyApplied_pfn d = PB(IsDeveloperModePolicyApplied);
    IsSideloadingPolicyApplied_pfn s = PB(IsSideloadingPolicyApplied);
    REQUIRE(d); REQUIRE(s);
    /* the out value is only written when the policy value could be read: 1 when the value is 0 or 1 */
    UINT v = 0xdeadbeef; d(&v);
    CHECKF(v == 0xdeadbeef || v == 0 || v == 1, "dev policy flag 0x%x", v);
    UINT w = 0xdeadbeef; s(&w);
    CHECKF(w == 0xdeadbeef || w == 0 || w == 1, "sideload policy flag 0x%x", w);
    UINT v2 = 0xdeadbeef; d(&v2); CHECK(v2 == v);
    UINT w2 = 0xdeadbeef; s(&w2); CHECK(w2 == w);
    t_note("dev policy %x sideload policy %x", v, w);
}
TEST(gap24_oobe_complete) {
    BOOL (WINAPI *f)(BOOL *) = (void *)t_proc_k32("OOBEComplete"); REQUIRE(f);
    SetLastError(0); BOOL rn = f(NULL); t_note("OOBEComplete(NULL)=%d err %lu", rn, GetLastError());
    BOOL v = 0x55; BOOL r = f(&v);
    t_note("OOBEComplete ret=%d value=%d err=%lu", r, v, GetLastError());
    if (r) CHECK(v == 0 || v == 1);
    BOOL v2 = 0x55; BOOL r2 = f(&v2); CHECK(r2 == r);
}
TEST(gap24_open_profile_user_mapping) {
    BOOL (WINAPI *f)(VOID) = (void *)t_proc_k32("OpenProfileUserMapping"); REQUIRE(f);
    if (t_is_wine()) { t_skip("wine fails OpenProfileUserMapping"); return; }
    for (int i = 0; i < 10; i++) { SetLastError(0x1234); EXPECT_EQ(f(), 1); }
    BOOL (WINAPI *c)(VOID) = (void *)t_proc_k32("CloseProfileUserMapping");
    if (c) EXPECT_EQ(c(), 1);
}

/* ---------- Globalization user settings ---------- */
UDECL(LONG, OpenGlobalizationUserSettingsKey, (REGSAM, PVOID, HKEY *));
TEST(gap24_globalization_key_null_out) {
    OpenGlobalizationUserSettingsKey_pfn f = PB(OpenGlobalizationUserSettingsKey); REQUIRE(f);
    EXPECT_EQ((DWORD)f(KEY_READ, NULL, NULL), 0xC000000D);
    EXPECT_EQ((DWORD)f(0, NULL, NULL), 0xC000000D);
    EXPECT_EQ((DWORD)f(0xffffffff, NULL, NULL), 0xC000000D);
}
TEST(gap24_globalization_key_open_read) {
    OpenGlobalizationUserSettingsKey_pfn f = PB(OpenGlobalizationUserSettingsKey); REQUIRE(f);
    HKEY k = (HKEY)0x1; LONG st = f(KEY_READ, NULL, &k);
    t_note("OpenGlobalizationUserSettingsKey status 0x%lx", (DWORD)st);
    if (st == 0) {
        CHECK(k != NULL && k != (HKEY)0x1);
        DWORD n = 0; LONG rc = RegQueryInfoKeyW(k, NULL, NULL, NULL, &n, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
        CHECKF(rc == ERROR_SUCCESS, "RegQueryInfoKey on returned key: %ld", rc);
        RegCloseKey(k);
    }
}

/* ---------- LoadStringBaseExW ---------- */
UDECL(int, LoadStringBaseExW, (HMODULE, UINT, LPWSTR, int, WORD));
TEST(gap24_loadstringbase_null_buffer) {
    LoadStringBaseExW_pfn f = PB(LoadStringBaseExW); REQUIRE(f);
    if (t_is_wine()) { t_skip("LoadStringBaseExW is a wine stub"); return; }
    SetLastError(0); EXPECT_EQ(f(GetModuleHandleW(L"kernel32.dll"), 1, NULL, 10, 0), 0);
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SetLastError(0); EXPECT_EQ(f(NULL, 1, NULL, 0, 0), 0); EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap24_loadstringbase_missing_string) {
    LoadStringBaseExW_pfn f = PB(LoadStringBaseExW); REQUIRE(f);
    if (t_is_wine()) { t_skip("LoadStringBaseExW is a wine stub"); return; }
    WCHAR b[16]; for (int i = 0; i < 16; i++) b[i] = 0x7777;
    SetLastError(0);
    EXPECT_EQ(f(NULL, 0xFFFF0, b, 16, 0), 0);        /* nonexistent id in the process image */
    EXPECT_EQ(b[0], 0);                              /* buffer is NUL-terminated on failure */
    CHECK(GetLastError() != 0);
    t_note("missing string last error %lu", GetLastError());
    WCHAR *p = (WCHAR *)0x1234;
    EXPECT_EQ(f(NULL, 0xFFFF0, (LPWSTR)&p, 0, 0), 0);
}
TEST(gap24_loadstringbase_parity_with_loadstringw) {
    LoadStringBaseExW_pfn f = PB(LoadStringBaseExW); REQUIRE(f);
    if (t_is_wine()) { t_skip("LoadStringBaseExW is a wine stub"); return; }
    int (WINAPI *lsw)(HINSTANCE, UINT, LPWSTR, int) = (void *)GetProcAddress(t_user32(), "LoadStringW"); REQUIRE(lsw);
    HMODULE m = t_user32(); UINT id = 0; WCHAR ref[512];
    for (UINT i = 1; i < 3000; i++) { if (lsw(m, i, ref, 512) > 0) { id = i; break; } }
    if (!id) { t_skip("no string resource found in user32"); return; }
    int n = (int)lstrlenW(ref);
    WCHAR b[512]; memset(b, 0xAA, sizeof b);
    int got = f(m, id, b, 512, 0);
    if (got == 0) { t_note("string %u only reachable through MUI redirection (err %lu)", id, GetLastError()); return; }
    EXPECT_EQ(got, n); CHECK(!lstrcmpW(b, ref));
    /* truncation: cch smaller than string+1 copies cch-1 characters */
    if (n > 3) {
        memset(b, 0xAA, sizeof b);
        EXPECT_EQ(f(m, id, b, 4, 0), 3); EXPECT_EQ(b[3], 0); CHECK(!memcmp(b, ref, 3 * sizeof(WCHAR)));
        memset(b, 0xAA, sizeof b);
        EXPECT_EQ(f(m, id, b, 1, 0), 0); EXPECT_EQ(b[0], 0);
    }
    /* cch == 0: *buffer receives a pointer into the resource */
    WCHAR *rp = NULL;
    EXPECT_EQ(f(m, id, (LPWSTR)&rp, 0, 0), n);
    CHECK(rp != NULL);
    if (rp) CHECK(!memcmp(rp, ref, n * sizeof(WCHAR)));
}

/* ---------- state / package open entry points ---------- */
TEST(gap24_state_open_atom_null_container) {
    void *(WINAPI *f)(void *, LPCWSTR) = (void *)t_proc_kb("OpenStateAtom"); REQUIRE(f);
    SetLastError(0); void *a = f(NULL, L"k32t_atom24");
    CHECK(a == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SetLastError(0); a = f(NULL, NULL);
    CHECK(a == NULL); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap24_openstate_forwarder_identity) {
    FARPROC k = t_proc_k32("OpenState"), b = t_proc_kb("OpenState");
    if (!k || !b) { t_skip("OpenState missing"); return; }
    CHECK(k == b);      /* kernel32 forwards to kernelbase */
    /* not called: it allocates a state-repository connection */
}
UDECL(LONG, OpenPackageInfoByFullNameForMachine, (LPCWSTR, PVOID, PVOID *));
UDECL(LONG, OpenPackageInfoByFullNameForUser, (PSID, LPCWSTR, PVOID, PVOID *));
TEST(gap24_open_package_info_machine_invalid) {
    OpenPackageInfoByFullNameForMachine_pfn f = PB(OpenPackageInfoByFullNameForMachine); REQUIRE(f);
    PVOID ref = (PVOID)0x1;
    EXPECT_EQ(f(NULL, NULL, &ref), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(f(L"x", NULL, NULL), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(f(NULL, NULL, NULL), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(ref, 0x1);
}
TEST(gap24_open_package_info_machine_unknown_name) {
    OpenPackageInfoByFullNameForMachine_pfn f = PB(OpenPackageInfoByFullNameForMachine); REQUIRE(f);
    PVOID ref = NULL;
    LONG r = f(L"K32t.NoSuchPackage_1.0.0.0_x64__8wekyb3d8bbwe", NULL, &ref);
    t_note("OpenPackageInfoByFullNameForMachine(unknown) = %ld", r);
    CHECK(r != ERROR_SUCCESS); CHECK(ref == NULL);
    r = f(L"", NULL, &ref); t_note("empty name = %ld", r); CHECK(r != ERROR_SUCCESS);
    r = f(L"not a package full name", NULL, &ref); t_note("garbage name = %ld", r); CHECK(r != ERROR_SUCCESS);
}
TEST(gap24_open_package_info_user_invalid) {
    OpenPackageInfoByFullNameForUser_pfn f = PB(OpenPackageInfoByFullNameForUser); REQUIRE(f);
    PVOID ref = (PVOID)0x1;
    EXPECT_EQ(f(NULL, NULL, NULL, &ref), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(f(NULL, L"x", NULL, NULL), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(f(NULL, NULL, NULL, NULL), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(ref, 0x1);
}

/* ---------- named pipe event entry points (error paths only) ---------- */
TEST(gap24_namedpipe_event_enum_bad_handles) {
    BOOL (WINAPI *f)(HANDLE, DWORD *, ULONG_PTR) = (void *)t_proc_kb("NamedPipeEventEnum"); REQUIRE(f);
    DWORD out = 0x1234;
    SetLastError(0); CHECK(!f(NULL, &out, 0)); EXPECT_ERR(ERROR_INVALID_HANDLE); EXPECT_EQ(out, 0x1234);
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "pipeenum24.bin");
    HANDLE h = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h != INVALID_HANDLE_VALUE) {
        SetLastError(0); BOOL r = f(h, &out, 0);
        CHECK(!r); t_note("regular file handle err %lu", GetLastError()); EXPECT_EQ(out, 0x1234);
        CloseHandle(h); DeleteFileA(p);
    }
}
TEST(gap24_namedpipe_event_select_bad_handle) {
    BOOL (WINAPI *f)(HANDLE, DWORD, ULONG_PTR) = (void *)t_proc_kb("NamedPipeEventSelect"); REQUIRE(f);
    SetLastError(0); CHECK(!f(NULL, 0, 0)); EXPECT_ERR(ERROR_INVALID_HANDLE);
}

/* ---------- dangerous / unknown-signature exports: presence only ---------- */
TEST(gap24_not_called_exports_resolve) {
    /* NotifyMountMgr sends a mount-manager IOCTL; OpenRegKey / NlsValidateLocale /
     * IsMrtResourceRedirectionEnabled / OpenState have undocumented side effects or signatures. */
    const char *kb[] = {"NotifyMountMgr", "OpenRegKey", "NlsValidateLocale", "NlsIsUserDefaultLocale", "IsMrtResourceRedirectionEnabled", "OpenState", "LoadStringBaseExW"};
    for (size_t i = 0; i < sizeof kb / sizeof *kb; i++) {
        FARPROC p = t_proc_kb(kb[i]);
        if (!p && t_is_wine()) { t_note("wine lacks %s", kb[i]); continue; }
        CHECKF(p != NULL, "%s", kb[i]);
    }
    FARPROC k = t_proc_k32("LoadStringBaseW");
    if (!k && t_is_wine()) t_note("wine lacks LoadStringBaseW"); else CHECK(k != NULL);
}
