/* AppModel: query paths for a classic (non-packaged) desktop process, AppPolicy*, argument validation. */
#define NTDDI_VERSION 0x0A000008
#include "t.h"
#include <appmodel.h>
#include "am_compat.h"

#define NO_PACKAGE   15700   /* APPMODEL_ERROR_NO_PACKAGE */
#define NO_APPLICATION 15703 /* APPMODEL_ERROR_NO_APPLICATION */

static HANDLE am_token(void) {
    HANDLE h = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &h)) return NULL;
    return h;
}

/* ---------- GetCurrent* on non-packaged process ---------- */
TEST(am_cur_fullname_nopackage) {
    REQUIRE(U(GetCurrentPackageFullName));
    UINT32 len = 0;
    EQ(U(GetCurrentPackageFullName)(&len, NULL), NO_PACKAGE);
    EXPECT_EQ(len, 0);
    WCHAR buf[PACKAGE_FULL_NAME_MAX_LENGTH + 1];
    len = ARRAYSIZE(buf);
    memset(buf, 0xAA, sizeof buf);
    EQ(U(GetCurrentPackageFullName)(&len, buf), NO_PACKAGE);
    EXPECT_EQ(len, ARRAYSIZE(buf));
}
TEST(am_cur_fullname_invalid) {
    REQUIRE(U(GetCurrentPackageFullName));
    WCHAR buf[8];
    EQ(U(GetCurrentPackageFullName)(NULL, NULL), ERROR_INVALID_PARAMETER);
    EQ(U(GetCurrentPackageFullName)(NULL, buf), ERROR_INVALID_PARAMETER);
    UINT32 len = 8;
    EQ(U(GetCurrentPackageFullName)(&len, NULL), ERROR_INVALID_PARAMETER);
    EXPECT_EQ(len, 8);
}
TEST(am_cur_familyname_nopackage) {
    REQUIRE(U(GetCurrentPackageFamilyName));
    UINT32 len = 0;
    EQ(U(GetCurrentPackageFamilyName)(&len, NULL), NO_PACKAGE);
    WCHAR buf[PACKAGE_FAMILY_NAME_MAX_LENGTH + 1];
    len = ARRAYSIZE(buf);
    EQ(U(GetCurrentPackageFamilyName)(&len, buf), NO_PACKAGE);
}
TEST(am_cur_familyname_invalid) {
    REQUIRE(U(GetCurrentPackageFamilyName));
    WCHAR buf[8];
    UINT32 len = 8;
    EQ(U(GetCurrentPackageFamilyName)(NULL, NULL), ERROR_INVALID_PARAMETER);
    EQ(U(GetCurrentPackageFamilyName)(NULL, buf), ERROR_INVALID_PARAMETER);
    EQ(U(GetCurrentPackageFamilyName)(&len, NULL), ERROR_INVALID_PARAMETER);
}
TEST(am_cur_packageid_nopackage) {
    REQUIRE(U(GetCurrentPackageId));
    UINT32 len = 0;
    EQ(U(GetCurrentPackageId)(&len, NULL), NO_PACKAGE);
    BYTE buf[512];
    len = sizeof buf;
    EQ(U(GetCurrentPackageId)(&len, buf), NO_PACKAGE);
}
TEST(am_cur_packageid_invalid) {
    REQUIRE(U(GetCurrentPackageId));
    BYTE buf[64];
    EQ(U(GetCurrentPackageId)(NULL, NULL), ERROR_INVALID_PARAMETER);
    EQ(U(GetCurrentPackageId)(NULL, buf), ERROR_INVALID_PARAMETER);
}
TEST(am_cur_packageid_len_nonzero_nullbuf) {
    REQUIRE(U(GetCurrentPackageId));
    UINT32 len = 16;
    LONG r = U(GetCurrentPackageId)(&len, NULL);
    CHECK(r != ERROR_SUCCESS);
    t_note("GetCurrentPackageId(len=16, NULL) -> %ld", (long)r);
}
TEST(am_cur_path_nopackage) {
    REQUIRE(U(GetCurrentPackagePath));
    UINT32 len = 0;
    EQ(U(GetCurrentPackagePath)(&len, NULL), NO_PACKAGE);
    WCHAR buf[MAX_PATH];
    len = MAX_PATH;
    EQ(U(GetCurrentPackagePath)(&len, buf), NO_PACKAGE);
}
TEST(am_cur_path_invalid) {
    REQUIRE(U(GetCurrentPackagePath));
    LONG r = U(GetCurrentPackagePath)(NULL, NULL);
    CHECK(r != ERROR_SUCCESS);
    t_note("GetCurrentPackagePath(NULL,NULL) -> %ld", (long)r);
    UINT32 len = 4;
    r = U(GetCurrentPackagePath)(&len, NULL);
    CHECK(r != ERROR_SUCCESS);
}
TEST(am_cur_path2_types) {
    REQUIRE(UN(GetCurrentPackagePath2));
    for (int t = PackagePathType_Install; t <= PackagePathType_Effective; t++) {
        UINT32 len = 0;
        LONG r = UN(GetCurrentPackagePath2)((int)t, &len, NULL);
        EXPECT_EQ(r, NO_PACKAGE);
    }
}
TEST(am_cur_path2_badtype) {
    REQUIRE(UN(GetCurrentPackagePath2));
    UINT32 len = 0;
    LONG r = UN(GetCurrentPackagePath2)((int)0x7777, &len, NULL);
    CHECK(r != ERROR_SUCCESS);
    t_note("GetCurrentPackagePath2(type=0x7777) -> %ld", (long)r);
}
TEST(am_cur_info_nopackage) {
    REQUIRE(U(GetCurrentPackageInfo));
    UINT32 len = 0, cnt = 0xFFFF;
    EQ(U(GetCurrentPackageInfo)(PACKAGE_FILTER_HEAD, &len, NULL, &cnt), NO_PACKAGE);
    BYTE buf[1024];
    len = sizeof buf;
    EQ(U(GetCurrentPackageInfo)(PACKAGE_INFORMATION_BASIC, &len, buf, &cnt), NO_PACKAGE);
    EQ(U(GetCurrentPackageInfo)(PACKAGE_INFORMATION_FULL, &len, buf, &cnt), NO_PACKAGE);
}
TEST(am_cur_info_nulls) {
    REQUIRE(U(GetCurrentPackageInfo));
    UINT32 cnt;
    LONG r = U(GetCurrentPackageInfo)(0, NULL, NULL, &cnt);
    CHECK(r != ERROR_SUCCESS);
    t_note("GetCurrentPackageInfo(NULL len) -> %ld", (long)r);
}
TEST(am_cur_info2_nopackage) {
    REQUIRE(UN(GetCurrentPackageInfo2));
    UINT32 len = 0, cnt = 0;
    EQ(UN(GetCurrentPackageInfo2)(0, PackagePathType_Install, &len, NULL, &cnt), NO_PACKAGE);
    EQ(UN(GetCurrentPackageInfo2)(PACKAGE_FILTER_HEAD, PackagePathType_Effective, &len, NULL, &cnt), NO_PACKAGE);
}
TEST(am_cur_aumid_noapp) {
    REQUIRE(U(GetCurrentApplicationUserModelId));
    UINT32 len = 0;
    EQ(U(GetCurrentApplicationUserModelId)(&len, NULL), NO_APPLICATION);
    WCHAR buf[APPLICATION_USER_MODEL_ID_MAX_LENGTH + 1];
    len = ARRAYSIZE(buf);
    EQ(U(GetCurrentApplicationUserModelId)(&len, buf), NO_APPLICATION);
}
TEST(am_cur_aumid_invalid) {
    REQUIRE(U(GetCurrentApplicationUserModelId));
    WCHAR buf[8];
    UINT32 len = 8;
    EQ(U(GetCurrentApplicationUserModelId)(NULL, NULL), ERROR_INVALID_PARAMETER);
    EQ(U(GetCurrentApplicationUserModelId)(NULL, buf), ERROR_INVALID_PARAMETER);
    EQ(U(GetCurrentApplicationUserModelId)(&len, NULL), ERROR_INVALID_PARAMETER);
}
TEST(am_cur_virtualization_ctx) {
    if (!UN(GetCurrentPackageVirtualizationContext)) { t_note("GetCurrentPackageVirtualizationContext not exported"); return; }
    void *c = UN(GetCurrentPackageVirtualizationContext)();
    CHECK(c == NULL);
    t_note("virtualization context = %p", c);
}
TEST(am_cur_repeat_stable) {
    REQUIRE(U(GetCurrentPackageFullName));
    for (int i = 0; i < 200; i++) {
        UINT32 len = 0;
        if (U(GetCurrentPackageFullName)(&len, NULL) != NO_PACKAGE) { CHECK(0); break; }
    }
}
TEST(am_cur_lasterror_untouched) {
    /* The package queries return their error code and are not documented to set last-error. */
    REQUIRE(U(GetCurrentPackageFullName));
    UINT32 len = 0;
    SET_ERR(0x1234);
    LONG r = U(GetCurrentPackageFullName)(&len, NULL);
    EXPECT_EQ(r, NO_PACKAGE);
    t_note("GetLastError after GetCurrentPackageFullName: %lu", GetLastError());
}
TEST(am_cur_thread_independent) {
    REQUIRE(U(GetCurrentPackageFamilyName));
    struct S { LONG r; } s = {0};
    DWORD WINAPI th(LPVOID p) {
        UINT32 len = 0;
        ((struct S *)p)->r = U(GetCurrentPackageFamilyName)(&len, NULL);
        return 0;
    }
    HANDLE h = CreateThread(NULL, 0, th, &s, 0, NULL);
    CHECK(h != NULL);
    if (h) { WaitForSingleObject(h, 5000); CloseHandle(h); }
    EXPECT_EQ(s.r, NO_PACKAGE);
}

/* ---------- process / token based queries ---------- */
TEST(am_proc_fullname) {
    REQUIRE(U(GetPackageFullName));
    UINT32 len = 0;
    EQ(U(GetPackageFullName)(GetCurrentProcess(), &len, NULL), NO_PACKAGE);
    WCHAR buf[PACKAGE_FULL_NAME_MAX_LENGTH + 1];
    len = ARRAYSIZE(buf);
    EQ(U(GetPackageFullName)(GetCurrentProcess(), &len, buf), NO_PACKAGE);
}
TEST(am_proc_fullname_realhandle) {
    REQUIRE(U(GetPackageFullName));
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    CHECK(h != NULL);
    if (!h) return;
    UINT32 len = 0;
    EQ(U(GetPackageFullName)(h, &len, NULL), NO_PACKAGE);
    CloseHandle(h);
}
TEST(am_proc_fullname_invalid) {
    REQUIRE(U(GetPackageFullName));
    WCHAR buf[8];
    UINT32 len = 8;
    EQ(U(GetPackageFullName)(NULL, &len, buf), ERROR_INVALID_PARAMETER);
    EQ(U(GetPackageFullName)(GetCurrentProcess(), NULL, buf), ERROR_INVALID_PARAMETER);
    EQ(U(GetPackageFullName)(GetCurrentProcess(), &len, NULL), ERROR_INVALID_PARAMETER);
    EQ(U(GetPackageFullName)(NULL, NULL, NULL), ERROR_INVALID_PARAMETER);
}
TEST(am_proc_fullname_badhandle) {
    REQUIRE(U(GetPackageFullName));
    UINT32 len = 0;
    LONG r = U(GetPackageFullName)((HANDLE)(ULONG_PTR)0x7FFFF0, &len, NULL);
    CHECK(r != ERROR_SUCCESS);
    t_note("GetPackageFullName(bad handle) -> %ld", (long)r);
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
    r = U(GetPackageFullName)(ev, &len, NULL);
    CHECK(r != ERROR_SUCCESS);
    t_note("GetPackageFullName(event) -> %ld", (long)r);
    CloseHandle(ev);
}
TEST(am_proc_familyname) {
    REQUIRE(U(GetPackageFamilyName));
    UINT32 len = 0;
    EQ(U(GetPackageFamilyName)(GetCurrentProcess(), &len, NULL), NO_PACKAGE);
    WCHAR buf[PACKAGE_FAMILY_NAME_MAX_LENGTH + 1];
    len = ARRAYSIZE(buf);
    EQ(U(GetPackageFamilyName)(GetCurrentProcess(), &len, buf), NO_PACKAGE);
}
TEST(am_proc_familyname_invalid) {
    REQUIRE(U(GetPackageFamilyName));
    WCHAR buf[8];
    UINT32 len = 8;
    EQ(U(GetPackageFamilyName)(NULL, &len, buf), ERROR_INVALID_PARAMETER);
    EQ(U(GetPackageFamilyName)(GetCurrentProcess(), NULL, buf), ERROR_INVALID_PARAMETER);
    EQ(U(GetPackageFamilyName)(GetCurrentProcess(), &len, NULL), ERROR_INVALID_PARAMETER);
}
TEST(am_proc_id) {
    REQUIRE(U(GetPackageId));
    UINT32 len = 0;
    EQ(U(GetPackageId)(GetCurrentProcess(), &len, NULL), NO_PACKAGE);
    BYTE buf[512];
    len = sizeof buf;
    EQ(U(GetPackageId)(GetCurrentProcess(), &len, buf), NO_PACKAGE);
}
TEST(am_proc_id_invalid) {
    REQUIRE(U(GetPackageId));
    BYTE buf[16];
    UINT32 len = 16;
    EQ(U(GetPackageId)(NULL, &len, buf), ERROR_INVALID_PARAMETER);
    EQ(U(GetPackageId)(GetCurrentProcess(), NULL, buf), ERROR_INVALID_PARAMETER);
    EQ(U(GetPackageId)(GetCurrentProcess(), &len, NULL), ERROR_INVALID_PARAMETER);
}
TEST(am_proc_aumid) {
    REQUIRE(U(GetApplicationUserModelId));
    UINT32 len = 0;
    EQ(U(GetApplicationUserModelId)(GetCurrentProcess(), &len, NULL), NO_APPLICATION);
}
TEST(am_proc_aumid_invalid) {
    REQUIRE(U(GetApplicationUserModelId));
    WCHAR buf[8];
    UINT32 len = 8;
    EQ(U(GetApplicationUserModelId)(NULL, &len, buf), ERROR_INVALID_PARAMETER);
    EQ(U(GetApplicationUserModelId)(GetCurrentProcess(), NULL, buf), ERROR_INVALID_PARAMETER);
    EQ(U(GetApplicationUserModelId)(GetCurrentProcess(), &len, NULL), ERROR_INVALID_PARAMETER);
}
TEST(am_token_aumid) {
    REQUIRE(U(GetApplicationUserModelIdFromToken));
    HANDLE tok = am_token();
    CHECK(tok != NULL);
    if (!tok) return;
    UINT32 len = 0;
    LONG r = U(GetApplicationUserModelIdFromToken)(tok, &len, NULL);
    EXPECT_EQ(r, NO_APPLICATION);
    CloseHandle(tok);
}
TEST(am_token_aumid_invalid) {
    REQUIRE(U(GetApplicationUserModelIdFromToken));
    HANDLE tok = am_token();
    WCHAR buf[8];
    UINT32 len = 8;
    EQ(U(GetApplicationUserModelIdFromToken)(NULL, &len, buf), ERROR_INVALID_PARAMETER);
    if (tok) {
        EQ(U(GetApplicationUserModelIdFromToken)(tok, NULL, buf), ERROR_INVALID_PARAMETER);
        EQ(U(GetApplicationUserModelIdFromToken)(tok, &len, NULL), ERROR_INVALID_PARAMETER);
        CloseHandle(tok);
    }
}
TEST(am_token_aumid_badhandle) {
    REQUIRE(U(GetApplicationUserModelIdFromToken));
    UINT32 len = 0;
    LONG r = U(GetApplicationUserModelIdFromToken)((HANDLE)(ULONG_PTR)0x7FFFF0, &len, NULL);
    CHECK(r != ERROR_SUCCESS);
    t_note("AUMID from bad token -> %ld", (long)r);
}
TEST(am_token_fullname) {
    REQUIRE(U(GetPackageFullNameFromToken));
    HANDLE tok = am_token();
    if (!tok) { CHECK(0); return; }
    UINT32 len = 0;
    LONG r = U(GetPackageFullNameFromToken)(tok, &len, NULL);
    EXPECT_EQ(r, NO_PACKAGE);
    EQ(U(GetPackageFullNameFromToken)(NULL, &len, NULL), ERROR_INVALID_PARAMETER);
    CloseHandle(tok);
}
TEST(am_token_familyname) {
    REQUIRE(U(GetPackageFamilyNameFromToken));
    HANDLE tok = am_token();
    if (!tok) { CHECK(0); return; }
    UINT32 len = 0;
    LONG r = U(GetPackageFamilyNameFromToken)(tok, &len, NULL);
    EXPECT_EQ(r, NO_PACKAGE);
    EQ(U(GetPackageFamilyNameFromToken)(NULL, &len, NULL), ERROR_INVALID_PARAMETER);
    CloseHandle(tok);
}
TEST(am_token_pseudo_handle) {
    REQUIRE(U(GetPackageFullNameFromToken));
    UINT32 len = 0;
    LONG r = U(GetPackageFullNameFromToken)((HANDLE)(LONG_PTR)-4, &len, NULL);
    t_note("GetPackageFullNameFromToken(pseudo token) -> %ld", (long)r);
}
TEST(am_proc_applicationids) {
    REQUIRE(UN(GetPackageApplicationIds));
    /* takes a PACKAGE_INFO_REFERENCE, NULL is rejected */
    UINT32 len = 0, cnt = 0;
    LONG r = UN(GetPackageApplicationIds)(NULL, &len, NULL, &cnt);
    EXPECT_EQ(r, ERROR_INVALID_PARAMETER);
    r = UN(GetPackageApplicationIds)(NULL, NULL, NULL, NULL);
    EXPECT_EQ(r, ERROR_INVALID_PARAMETER);
}
TEST(am_info_ref_null) {
    REQUIRE(UN(GetPackageInfo));
    UINT32 len = 0, cnt = 0;
    LONG r = UN(GetPackageInfo)(NULL, PACKAGE_FILTER_HEAD, &len, NULL, &cnt);
    CHECK(r == ERROR_INVALID_PARAMETER || (ULONG)r == 0x80070057UL);
    t_note("GetPackageInfo(NULL ref) -> 0x%lx", (unsigned long)r);
    r = UN(GetPackageInfo)(NULL, 0, NULL, NULL, NULL);
    CHECK(r == ERROR_INVALID_PARAMETER || (ULONG)r == 0x80070057UL);
}
TEST(am_info_close_null) {
    REQUIRE(UN(ClosePackageInfo));
    LONG r = UN(ClosePackageInfo)(NULL);
    CHECK(r == ERROR_INVALID_PARAMETER || (ULONG)r == 0x80070057UL);
}
TEST(am_info_open_nonexistent) {
    REQUIRE(UN(OpenPackageInfoByFullName));
    void *ref = (void *)(ULONG_PTR)0x11;
    LONG r = UN(OpenPackageInfoByFullName)(L"K32tNoSuchPackage_1.0.0.0_x64__8wekyb3d8bbwe", 0, &ref);
    CHECK(r != ERROR_SUCCESS);
    t_note("OpenPackageInfoByFullName(nonexistent) -> %ld (0x%lx)", (long)r, (unsigned long)r);
    if (r == ERROR_SUCCESS) UN(ClosePackageInfo)(ref);
}
TEST(am_info_open_invalid) {
    REQUIRE(UN(OpenPackageInfoByFullName));
    void *ref;
    LONG r = UN(OpenPackageInfoByFullName)(NULL, 0, &ref);
    CHECK(r != ERROR_SUCCESS);
    r = UN(OpenPackageInfoByFullName)(L"nonsense", 0, &ref);
    CHECK(r != ERROR_SUCCESS);
    r = UN(OpenPackageInfoByFullName)(L"", 0, &ref);
    CHECK(r != ERROR_SUCCESS);
    r = UN(OpenPackageInfoByFullName)(L"K32tNoSuchPackage_1.0.0.0_x64__8wekyb3d8bbwe", 0, NULL);
    CHECK(r != ERROR_SUCCESS);
}

/* ---------- path / staged queries for packages that do not exist ---------- */
static const WCHAR *g_nopkg = L"K32tNoSuchPackage_1.0.0.0_x64__8wekyb3d8bbwe";
TEST(am_pathbyfull_invalid) {
    REQUIRE(U(GetPackagePathByFullName));
    WCHAR buf[MAX_PATH];
    UINT32 len = MAX_PATH;
    EQ(U(GetPackagePathByFullName)(NULL, &len, buf), ERROR_INVALID_PARAMETER);
    EQ(U(GetPackagePathByFullName)(g_nopkg, NULL, buf), ERROR_INVALID_PARAMETER);
    EQ(U(GetPackagePathByFullName)(g_nopkg, &len, NULL), ERROR_INVALID_PARAMETER);
}
TEST(am_pathbyfull_nonexistent) {
    REQUIRE(U(GetPackagePathByFullName));
    UINT32 len = 0;
    LONG r = U(GetPackagePathByFullName)(g_nopkg, &len, NULL);
    CHECK(r != ERROR_SUCCESS && r != ERROR_INSUFFICIENT_BUFFER);
    t_note("GetPackagePathByFullName(nonexistent) -> %ld", (long)r);
    r = U(GetPackagePathByFullName)(L"garbage", &len, NULL);
    CHECK(r != ERROR_SUCCESS);
}
TEST(am_pathbyfull2_nonexistent) {
    REQUIRE(UN(GetPackagePathByFullName2));
    UINT32 len = 0;
    for (int t = 0; t <= 2; t++) {
        LONG r = UN(GetPackagePathByFullName2)(g_nopkg, (int)t, &len, NULL);
        CHECK(r != ERROR_SUCCESS);
    }
    EQ(UN(GetPackagePathByFullName2)(NULL, PackagePathType_Install, &len, NULL), ERROR_INVALID_PARAMETER);
}
TEST(am_staged_path_invalid) {
    REQUIRE(U(GetStagedPackagePathByFullName));
    WCHAR buf[MAX_PATH];
    UINT32 len = MAX_PATH;
    EQ(U(GetStagedPackagePathByFullName)(NULL, &len, buf), ERROR_INVALID_PARAMETER);
    EQ(U(GetStagedPackagePathByFullName)(g_nopkg, NULL, buf), ERROR_INVALID_PARAMETER);
    EQ(U(GetStagedPackagePathByFullName)(g_nopkg, &len, NULL), ERROR_INVALID_PARAMETER);
    len = 0;
    LONG r = U(GetStagedPackagePathByFullName)(g_nopkg, &len, NULL);
    CHECK(r != ERROR_SUCCESS && r != ERROR_INSUFFICIENT_BUFFER);
}
TEST(am_staged_path2) {
    REQUIRE(UN(GetStagedPackagePathByFullName2));
    UINT32 len = 0;
    EQ(UN(GetStagedPackagePathByFullName2)(NULL, PackagePathType_Install, &len, NULL), ERROR_INVALID_PARAMETER);
    LONG r = UN(GetStagedPackagePathByFullName2)(g_nopkg, PackagePathType_Install, &len, NULL);
    CHECK(r != ERROR_SUCCESS);
}
TEST(am_staged_origin_invalid) {
    REQUIRE(U(GetStagedPackageOrigin));
    PackageOrigin o = (PackageOrigin)77;
    EQ(U(GetStagedPackageOrigin)(NULL, &o), ERROR_INVALID_PARAMETER);
    EQ(U(GetStagedPackageOrigin)(g_nopkg, NULL), ERROR_INVALID_PARAMETER);
}
TEST(am_staged_origin_nonexistent) {
    REQUIRE(U(GetStagedPackageOrigin));
    PackageOrigin o = (PackageOrigin)77;
    LONG r = U(GetStagedPackageOrigin)(g_nopkg, &o);
    CHECK(r != ERROR_SUCCESS);
    t_note("GetStagedPackageOrigin(nonexistent) -> %ld origin=%d", (long)r, (int)o);
}
TEST(am_pathbyid_invalid) {
    REQUIRE(U(GetPackagePath));
    UINT32 len = 0;
    EQ(U(GetPackagePath)(NULL, 0, &len, NULL), ERROR_INVALID_PARAMETER);
    PACKAGE_ID id; memset(&id, 0, sizeof id);
    EQ(U(GetPackagePath)(&id, 0, NULL, NULL), ERROR_INVALID_PARAMETER);
    WCHAR b[4];
    len = 4;
    EQ(U(GetPackagePath)(&id, 0, &len, NULL), ERROR_INVALID_PARAMETER);
    /* an id with NULL name cannot be formatted */
    len = 0;
    LONG r = U(GetPackagePath)(&id, 0, &len, b);
    CHECK(r != ERROR_SUCCESS);
}
TEST(am_find_family_nonexistent) {
    REQUIRE(U(FindPackagesByPackageFamily));
    UINT32 count = 99, buflen = 0, props = 0;
    LONG r = U(FindPackagesByPackageFamily)(L"K32tNoSuchPackage_8wekyb3d8bbwe", PACKAGE_FILTER_HEAD | PACKAGE_INFORMATION_BASIC, &count, NULL, &buflen, NULL, NULL);
    t_note("FindPackagesByPackageFamily(nonexistent) -> %ld count=%u buflen=%u", (long)r, count, buflen);
    if (r == ERROR_SUCCESS) EXPECT_EQ(count, 0);
    (void)props;
}
TEST(am_find_family_invalid) {
    REQUIRE(U(FindPackagesByPackageFamily));
    UINT32 count = 0, buflen = 0;
    LONG r = U(FindPackagesByPackageFamily)(NULL, PACKAGE_FILTER_HEAD, &count, NULL, &buflen, NULL, NULL);
    CHECK(r != ERROR_SUCCESS);
    r = U(FindPackagesByPackageFamily)(L"K32tNoSuchPackage_8wekyb3d8bbwe", PACKAGE_FILTER_HEAD, NULL, NULL, &buflen, NULL, NULL);
    CHECK(r != ERROR_SUCCESS);
    r = U(FindPackagesByPackageFamily)(L"bad", PACKAGE_FILTER_HEAD, &count, NULL, &buflen, NULL, NULL);
    CHECK(r != ERROR_SUCCESS);
}
TEST(am_get_packages_family) {
    REQUIRE(U(GetPackagesByPackageFamily));
    UINT32 count = 99, buflen = 0;
    LONG r = U(GetPackagesByPackageFamily)(L"K32tNoSuchPackage_8wekyb3d8bbwe", &count, NULL, &buflen, NULL);
    t_note("GetPackagesByPackageFamily(nonexistent) -> %ld count=%u", (long)r, count);
    if (r == ERROR_SUCCESS) EXPECT_EQ(count, 0);
    r = U(GetPackagesByPackageFamily)(NULL, &count, NULL, &buflen, NULL);
    CHECK(r != ERROR_SUCCESS);
}
TEST(am_get_graph_revision) {
    REQUIRE(UN(GetPackageGraphRevisionId));
    UINT32 a = UN(GetPackageGraphRevisionId)();
    UINT32 b = UN(GetPackageGraphRevisionId)();
    EXPECT_EQ(a, b);
    t_note("GetPackageGraphRevisionId = %u", a);
}

/* ---------- AppPolicy* ---------- */
#define APPPOLICY_DESKTOP(fn, type, expect) \
TEST(ap_##fn##_desktop) { \
    REQUIRE(U(AppPolicyGet##fn)); \
    HANDLE tok = am_token(); CHECK(tok != NULL); if (!tok) return; \
    type v = (type)0x5555; \
    LONG r = U(AppPolicyGet##fn)(tok, &v); \
    EXPECT_EQ(r, ERROR_SUCCESS); \
    EXPECT_EQ(v, expect); \
    CloseHandle(tok); \
} \
TEST(ap_##fn##_invalid) { \
    REQUIRE(U(AppPolicyGet##fn)); \
    HANDLE tok = am_token(); \
    type v = (type)0x5555; \
    EQ(U(AppPolicyGet##fn)(NULL, &v), ERROR_INVALID_PARAMETER); \
    EXPECT_EQ((int)v, 0x5555); \
    if (tok) { EQ(U(AppPolicyGet##fn)(tok, NULL), ERROR_INVALID_PARAMETER); CloseHandle(tok); } \
    EQ(U(AppPolicyGet##fn)(NULL, NULL), ERROR_INVALID_PARAMETER); \
} \
TEST(ap_##fn##_badtoken) { \
    REQUIRE(U(AppPolicyGet##fn)); \
    type v = (type)0x5555; \
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL); \
    LONG r = U(AppPolicyGet##fn)(ev, &v); \
    CHECK(r != ERROR_SUCCESS); \
    EXPECT_EQ((int)v, 0); \
    v = (type)0x5555; \
    r = U(AppPolicyGet##fn)((HANDLE)(ULONG_PTR)0x7FFFF0, &v); \
    CHECK(r != ERROR_SUCCESS); \
    CloseHandle(ev); \
} \
TEST(ap_##fn##_repeat) { \
    REQUIRE(U(AppPolicyGet##fn)); \
    HANDLE tok = am_token(); if (!tok) return; \
    for (int i = 0; i < 50; i++) { type v = (type)0x5555; if (U(AppPolicyGet##fn)(tok, &v) != 0 || v != expect) { CHECK(0); break; } } \
    CloseHandle(tok); \
}

APPPOLICY_DESKTOP(WindowingModel, AppPolicyWindowingModel, AppPolicyWindowingModel_ClassicDesktop)
APPPOLICY_DESKTOP(LifecycleManagement, AppPolicyLifecycleManagement, AppPolicyLifecycleManagement_Unmanaged)
APPPOLICY_DESKTOP(ProcessTerminationMethod, AppPolicyProcessTerminationMethod, AppPolicyProcessTerminationMethod_ExitProcess)
APPPOLICY_DESKTOP(ThreadInitializationType, AppPolicyThreadInitializationType, AppPolicyThreadInitializationType_None)
APPPOLICY_DESKTOP(ShowDeveloperDiagnostic, AppPolicyShowDeveloperDiagnostic, AppPolicyShowDeveloperDiagnostic_None)
APPPOLICY_DESKTOP(MediaFoundationCodecLoading, AppPolicyMediaFoundationCodecLoading, AppPolicyMediaFoundationCodecLoading_All)
APPPOLICY_DESKTOP(ClrCompat, AppPolicyClrCompat, AppPolicyClrCompat_ClassicDesktop)
APPPOLICY_DESKTOP(CreateFileAccess, AppPolicyCreateFileAccess, AppPolicyCreateFileAccess_Full)

TEST(ap_pseudo_token) {
    REQUIRE(U(AppPolicyGetWindowingModel));
    AppPolicyWindowingModel v = (AppPolicyWindowingModel)0x5555;
    LONG r = U(AppPolicyGetWindowingModel)((HANDLE)(LONG_PTR)-4, &v);
    t_note("AppPolicyGetWindowingModel(pseudo token) -> 0x%lx v=%d", (unsigned long)r, (int)v);
    v = (AppPolicyWindowingModel)0x5555;
    v = (AppPolicyWindowingModel)0x5555;
    r = U(AppPolicyGetWindowingModel)(GetCurrentProcess(), &v);   /* a process handle is not a token */
    t_note("AppPolicyGetWindowingModel(process pseudo handle) -> 0x%lx v=%d", (unsigned long)r, (int)v);
}
TEST(ap_other_policies_probe) {
    static const char *names[] = {
        "AppPolicyGetWindowingModel","AppPolicyGetLifecycleManagement","AppPolicyGetProcessTerminationMethod",
        "AppPolicyGetThreadInitializationType","AppPolicyGetShowDeveloperDiagnostic","AppPolicyGetMediaFoundationCodecLoading",
        "AppPolicyGetClrCompat","AppPolicyGetCreateFileAccess" };
    for (size_t i = 0; i < ARRAYSIZE(names); i++) CHECKF(t_proc(names[i]) != NULL, "%s", names[i]);
}
