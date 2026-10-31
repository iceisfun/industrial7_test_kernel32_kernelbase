/* gap20: AppX / AppContainer identity helpers, state lock and stubs (argument-validation paths only). */
#include "t.h"

#define HR_INVALIDARG ((LONG)0x80070057)
#define HR_NOTIMPL    ((LONG)0x80004001)

typedef LONG (WINAPI *hr2_fn)(PVOID, PVOID);
typedef LONG (WINAPI *hr1_fn)(PVOID);
typedef LONG (WINAPI *hr3_fn)(PVOID, PVOID, PVOID);
typedef LONG (WINAPI *hr4_fn)(PVOID, PVOID, PVOID, PVOID);
typedef LONG (WINAPI *hr8_fn)(PVOID, PVOID, PVOID, PVOID, PVOID, PVOID, PVOID, PVOID);
typedef void (WINAPI *free_fn)(PVOID);
typedef BOOL (WINAPI *AcquireStateLock_fn)(PVOID, UINT, UINT);
typedef ULONG_PTR (WINAPI *stub_fn)(ULONG_PTR, ULONG_PTR, ULONG_PTR, ULONG_PTR);

static BYTE g_dummy[256];
#define D ((PVOID)g_dummy)

/* Two-pointer functions: (A, B) where either NULL is E_INVALIDARG. */
static void two_arg_null(const char *name) {
    hr2_fn f = (hr2_fn)t_proc_kb(name);
    if (!f) { t_skip("missing export %s", name); return; }
    LONG r;
    r = f(NULL, NULL); CHECKF(r == HR_INVALIDARG, "%s(NULL,NULL)=%08lx", name, (unsigned long)r);
    r = f(NULL, D);    CHECKF(r == HR_INVALIDARG, "%s(NULL,p)=%08lx", name, (unsigned long)r);
    r = f(D, NULL);    CHECKF(r == HR_INVALIDARG, "%s(p,NULL)=%08lx", name, (unsigned long)r);
}
#define TWO(n) TEST(gap20_nullargs_##n) { two_arg_null(#n); }
TWO(AppXGetOSMaxVersionTested)
TWO(AppXGetDevelopmentMode)
TWO(AppXGetPackageSid)
TWO(AppXLookupDisplayName)
TWO(AppXLookupMoniker)
TWO(AppContainerDeriveSidFromMoniker)
TWO(AppContainerLookupMoniker)
TWO(AppContainerLookupDisplayNameMrtReference)

TEST(gap20_nullargs_AppContainerRegisterSid) {
    hr3_fn f = (hr3_fn)t_proc_kb("AppContainerRegisterSid"); REQUIRE(f);
    PVOID a[3] = { D, D, D };
    for (int i = 0; i < 3; i++) {
        PVOID b[3] = { a[0], a[1], a[2] }; b[i] = NULL;
        LONG r = f(b[0], b[1], b[2]);
        CHECKF(r == HR_INVALIDARG, "arg %d NULL -> %08lx", i, (unsigned long)r);
    }
    CHECK(f(NULL, NULL, NULL) == HR_INVALIDARG);
}
TEST(gap20_nullargs_AppContainerUnregisterSid) {
    hr1_fn f = (hr1_fn)t_proc_kb("AppContainerUnregisterSid"); REQUIRE(f);
    CHECK(f(NULL) == HR_INVALIDARG);
}
TEST(gap20_nullargs_AppXGetApplicationData) {
    hr8_fn f = (hr8_fn)t_proc_kb("AppXGetApplicationData"); REQUIRE(f);
    for (int i = 0; i < 8; i++) {
        PVOID a[8]; for (int k = 0; k < 8; k++) a[k] = D;
        a[i] = NULL;
        LONG r = f(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
        CHECKF(r == HR_INVALIDARG, "arg %d NULL -> %08lx", i, (unsigned long)r);
    }
    CHECK(f(NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL) == HR_INVALIDARG);
}
TEST(gap20_nullargs_AppXGetPackageCapabilities) {
    hr4_fn f = (hr4_fn)t_proc_kb("AppXGetPackageCapabilities"); REQUIRE(f);
    for (int i = 0; i < 4; i++) {
        PVOID a[4] = { D, D, D, D }; a[i] = NULL;
        ULONG_PTR r = ((ULONG_PTR (WINAPI *)(PVOID, PVOID, PVOID, PVOID))f)(a[0], a[1], a[2], a[3]);
        CHECKF((LONG)r == HR_INVALIDARG, "arg %d NULL -> %08lx", i, (unsigned long)r);
    }
}

/* ---- Free helpers ---- */
TEST(gap20_free_memory_null_noop) {
    free_fn a = (free_fn)t_proc_kb("AppXFreeMemory"), b = (free_fn)t_proc_kb("AppContainerFreeMemory");
    REQUIRE(a); REQUIRE(b);
    SetLastError(0x1234);
    a(NULL); b(NULL);
    EXPECT_ERR(0x1234);
}

/* ---- Derived AppContainer SID ---- */
typedef PVOID (WINAPI *dummy_t)(void);
static LONG derive(const WCHAR *mon, PSID *sid) {
    hr2_fn f = (hr2_fn)t_proc_kb("AppContainerDeriveSidFromMoniker");
    return f((PVOID)mon, sid);
}
TEST(gap20_derive_sid_basic) {
    REQUIRE(t_proc_kb("AppContainerDeriveSidFromMoniker")); REQUIRE(t_proc_kb("AppContainerFreeMemory"));
    free_fn fr = (free_fn)t_proc_kb("AppContainerFreeMemory");
    PSID sid = NULL;
    LONG r = derive(L"k32t.gap20.moniker", &sid);
    if (t_is_wine()) { t_note("derive -> %08lx", (unsigned long)r); if (sid) fr(sid); return; }
    CHECKF(r == 0, "hr=%08lx", (unsigned long)r);
    if (r != 0 || !sid) return;
    CHECK(IsValidSid(sid));
    SID_IDENTIFIER_AUTHORITY *ia = GetSidIdentifierAuthority(sid);
    CHECK(ia->Value[5] == 15);                      /* SECURITY_APP_PACKAGE_AUTHORITY */
    CHECK(*GetSidSubAuthorityCount(sid) == 8);      /* S-1-15-2-<7 hash words> */
    CHECK(*GetSidSubAuthority(sid, 0) == 2);        /* SECURITY_APP_PACKAGE_BASE_RID */
    fr(sid);
}
TEST(gap20_derive_sid_deterministic) {
    REQUIRE(t_proc_kb("AppContainerDeriveSidFromMoniker"));
    free_fn fr = (free_fn)t_proc_kb("AppContainerFreeMemory"); REQUIRE(fr);
    PSID a = NULL, b = NULL, c = NULL;
    if (derive(L"k32t.gap20.moniker", &a) != 0 || !a) { if (t_is_wine()) t_skip("not implemented"); else CHECK(0); return; }
    CHECK(derive(L"k32t.gap20.moniker", &b) == 0);
    CHECK(derive(L"k32t.gap20.other", &c) == 0);
    if (b) CHECK(EqualSid(a, b));
    if (c) CHECK(!EqualSid(a, c));
    CHECK(a != b);
    fr(a); if (b) fr(b); if (c) fr(c);
}
TEST(gap20_derive_sid_case_note) {
    REQUIRE(t_proc_kb("AppContainerDeriveSidFromMoniker"));
    free_fn fr = (free_fn)t_proc_kb("AppContainerFreeMemory"); REQUIRE(fr);
    PSID a = NULL, b = NULL;
    LONG r1 = derive(L"K32T.GAP20.CASE", &a), r2 = derive(L"k32t.gap20.case", &b);
    t_note("derive case: %08lx %08lx equal=%d", (unsigned long)r1, (unsigned long)r2, (a && b) ? EqualSid(a, b) : -1);
    if (a) fr(a);
    if (b) fr(b);
}
TEST(gap20_derive_sid_matches_userenv) {
    REQUIRE(t_proc_kb("AppContainerDeriveSidFromMoniker"));
    free_fn fr = (free_fn)t_proc_kb("AppContainerFreeMemory"); REQUIRE(fr);
    HMODULE ue = LoadLibraryA("userenv.dll");
    typedef HRESULT (WINAPI *dsid_t)(PCWSTR, PSID *);
    dsid_t ds = ue ? (dsid_t)GetProcAddress(ue, "DeriveAppContainerSidFromAppContainerName") : NULL;
    if (!ds) { t_skip("userenv derive unavailable"); return; }
    PSID a = NULL, b = NULL;
    LONG r1 = derive(L"k32t.gap20.moniker", &a);
    HRESULT r2 = ds(L"k32t.gap20.moniker", &b);
    t_note("kernelbase derive %08lx, userenv derive %08lx", (unsigned long)r1, (unsigned long)r2);
    if (r1 == 0 && r2 == 0 && a && b) CHECK(EqualSid(a, b) || t_is_wine());
    if (a) fr(a);
    if (b) FreeSid(b);
}
TEST(gap20_derive_sid_lengths) {
    REQUIRE(t_proc_kb("AppContainerDeriveSidFromMoniker"));
    free_fn fr = (free_fn)t_proc_kb("AppContainerFreeMemory"); REQUIRE(fr);
    WCHAR m[200];
    static const int lens[] = { 1, 2, 10, 50, 64 };
    for (size_t i = 0; i < sizeof lens / sizeof lens[0]; i++) {
        for (int k = 0; k < lens[i]; k++) m[k] = (WCHAR)('a' + k % 26);
        m[lens[i]] = 0;
        PSID s = NULL;
        LONG r = derive(m, &s);
        t_note("moniker len %d -> %08lx", lens[i], (unsigned long)r);
        if (r == 0 && s) { CHECK(IsValidSid(s)); fr(s); }
    }
}
TEST(gap20_derive_sid_empty_moniker_note) {
    REQUIRE(t_proc_kb("AppContainerDeriveSidFromMoniker"));
    free_fn fr = (free_fn)t_proc_kb("AppContainerFreeMemory"); REQUIRE(fr);
    PSID s = NULL;
    LONG r = derive(L"", &s);
    t_note("empty moniker -> %08lx sid=%p", (unsigned long)r, s);
    if (r == 0 && s) fr(s);
}

/* ---- Stubs ---- */
TEST(gap20_stub_appxgetosminversion_notimpl) {
    stub_fn f = (stub_fn)t_proc_kb("AppXGetOSMinVersion"); REQUIRE(f);
    if (t_is_wine()) { t_note("stub value not enforced on emulator"); return; }
    CHECK((LONG)f(0, 0, 0, 0) == HR_NOTIMPL);
    CHECK((LONG)f((ULONG_PTR)D, (ULONG_PTR)D, 0, 0) == HR_NOTIMPL);
}
TEST(gap20_stub_appxupdatepackagecapabilities_notimpl) {
    stub_fn f = (stub_fn)t_proc_kb("AppXUpdatePackageCapabilities"); REQUIRE(f);
    if (t_is_wine()) { t_note("stub value not enforced on emulator"); return; }
    CHECK((LONG)f(0, 0, 0, 0) == HR_NOTIMPL);
    CHECK((LONG)f(1, 2, 3, 4) == HR_NOTIMPL);
}
#define PID_STUB(n) TEST(gap20_stub_##n) { stub_fn f = (stub_fn)t_proc_kb(#n); REQUIRE(f); \
    if (t_is_wine()) { t_note("stub value not enforced on emulator"); return; } \
    CHECK((DWORD)f(0, 0, 0, 0) == ERROR_NOT_SUPPORTED); \
    CHECK((DWORD)f((ULONG_PTR)D, (ULONG_PTR)D, (ULONG_PTR)D, (ULONG_PTR)D) == ERROR_NOT_SUPPORTED); }
PID_STUB(ApplicationUserModelIdFromProductId)
PID_STUB(PackageFullNameFromProductId)
PID_STUB(PackageIdFromProductId)
PID_STUB(PackageRelativeApplicationIdFromProductId)
PID_STUB(ProductIdFromPackageFamilyName)
TEST(gap20_stub_productid_preserves_lasterror) {
    stub_fn f = (stub_fn)t_proc_kb("ApplicationUserModelIdFromProductId"); REQUIRE(f);
    if (t_is_wine()) return;
    SetLastError(0x3131);
    f(0, 0, 0, 0);
    EXPECT_ERR(0x3131);
}

/* ---- AcquireStateLock ---- */
TEST(gap20_acquirestatelock_null_lock) {
    AcquireStateLock_fn f = (AcquireStateLock_fn)t_proc_kb("AcquireStateLock"); REQUIRE(f);
    SetLastError(0);
    BOOL r = f(NULL, 0, 0);
    CHECK(r == FALSE);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap20_acquirestatelock_null_lock_params) {
    AcquireStateLock_fn f = (AcquireStateLock_fn)t_proc_kb("AcquireStateLock"); REQUIRE(f);
    static const UINT v[] = { 0, 1, 2, 100, 0xFFFFFFFF };
    for (size_t i = 0; i < sizeof v / sizeof v[0]; i++) {
        SetLastError(0);
        CHECK(f(NULL, v[i], v[i]) == FALSE);
        EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
}
