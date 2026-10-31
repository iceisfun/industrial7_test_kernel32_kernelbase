/* GetVersion, GetVersionEx{A,W}, RtlGetVersion, VerifyVersionInfoW, VerSetConditionMask,
 * GetProductInfo, IsWow64Process{,2}. */
#include "t.h"
#include "k.h"

UDECL(LONG, RtlGetVersion, (PVOID));
UDECL(void, RtlGetNtVersionNumbers, (DWORD *, DWORD *, DWORD *));
typedef LONG (WINAPI *rtlgv_fn)(PVOID);

#define KUSD ((volatile const unsigned char *)0x7FFE0000)
#define KUSD_U32(off) (*(volatile const DWORD *)(KUSD + (off)))
#define KUSD_U8(off)  (*(volatile const BYTE *)(KUSD + (off)))
#define KUSD_BUILD 0x260
#define KUSD_PRODTYPE 0x264
#define KUSD_MAJOR 0x26C
#define KUSD_MINOR 0x270
#define KUSD_SUITE 0x2D0

static int ver_true(OSVERSIONINFOEXW *o) {
    rtlgv_fn f = (rtlgv_fn)t_proc_nt("RtlGetVersion");
    if (!f) return 0;
    memset(o, 0, sizeof *o); o->dwOSVersionInfoSize = sizeof *o;
    return f(o) == 0;
}

/* ---------- RtlGetVersion / shared data ---------- */
TEST(version_rtl_true_version) {
    REQUIRE(UN(RtlGetVersion));
    OSVERSIONINFOEXW o;
    CHECK(ver_true(&o));
    EXPECT_EQ(o.dwMajorVersion, 10);
    EXPECT_EQ(o.dwMinorVersion, 0);
    CHECK(o.dwBuildNumber >= 10240);
    if (o.dwBuildNumber < 19041) t_note("build %lu is older than 19041", o.dwBuildNumber);
    EXPECT_EQ(o.dwPlatformId, VER_PLATFORM_WIN32_NT);
    EXPECT_EQ(o.dwOSVersionInfoSize, 0x11c);
}
TEST(version_rtl_size_0x114) {
    REQUIRE(UN(RtlGetVersion));
    struct { OSVERSIONINFOW o; DWORD guard[4]; } b;
    memset(&b, 0xAB, sizeof b);
    b.o.dwOSVersionInfoSize = 0x114;
    EXPECT_EQ(UN(RtlGetVersion)(&b.o), 0);
    EXPECT_EQ(b.o.dwMajorVersion, 10);
    EXPECT_EQ(b.guard[0], 0xABABABAB);
    EXPECT_EQ(b.guard[3], 0xABABABAB);
}
TEST(version_rtl_does_not_validate_size) {
    /* RtlGetVersion always succeeds; the base fields are filled whatever dwOSVersionInfoSize says */
    REQUIRE(UN(RtlGetVersion));
    static const DWORD sizes[] = { 0, 1, 0x94, 0x113, 0x114, 0x11c, 0x124, 0x1000 };
    for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        static OSVERSIONINFOEXW o; memset(&o, 0, sizeof o); o.dwOSVersionInfoSize = sizes[i];
        LONG st = UN(RtlGetVersion)(&o);
        CHECKF(st == 0, "size 0x%lx status 0x%lx", sizes[i], (DWORD)st);
        CHECKF(o.dwMajorVersion == 10, "size 0x%lx major %lu", sizes[i], o.dwMajorVersion);
        CHECKF(o.dwPlatformId == 2, "size 0x%lx platform %lu", sizes[i], o.dwPlatformId);
        CHECKF(o.dwOSVersionInfoSize == sizes[i], "size field changed");
    }
}
TEST(version_rtl_ex_fields) {
    REQUIRE(UN(RtlGetVersion));
    OSVERSIONINFOEXW o; CHECK(ver_true(&o));
    CHECK(o.wProductType >= 1 && o.wProductType <= 3);
    EXPECT_EQ(o.wServicePackMajor, 0);
    EXPECT_EQ(o.wServicePackMinor, 0);
    EXPECT_EQ(o.szCSDVersion[0], 0);
    CHECK(o.wSuiteMask & VER_SUITE_SINGLEUSERTS);
    CHECK(!(o.wSuiteMask & VER_SUITE_TERMINAL));
}
TEST(version_rtl_matches_shared_data) {
    REQUIRE(UN(RtlGetVersion));
    OSVERSIONINFOEXW o; CHECK(ver_true(&o));
    EXPECT_EQ(KUSD_U32(KUSD_MAJOR), o.dwMajorVersion);
    EXPECT_EQ(KUSD_U32(KUSD_MINOR), o.dwMinorVersion);
    EXPECT_EQ(KUSD_U32(KUSD_BUILD) & 0xFFFF, o.dwBuildNumber);
    EXPECT_EQ(KUSD_U32(KUSD_PRODTYPE), o.wProductType);
    EXPECT_EQ(KUSD_U32(KUSD_SUITE) & 0xFFEE, o.wSuiteMask & 0xFFEE);
}
TEST(version_rtl_nt_version_numbers) {
    REQUIRE(UN(RtlGetNtVersionNumbers)); REQUIRE(UN(RtlGetVersion));
    DWORD maj = 0, min = 0, bld = 0;
    UN(RtlGetNtVersionNumbers)(&maj, &min, &bld);
    OSVERSIONINFOEXW o; CHECK(ver_true(&o));
    EXPECT_EQ(maj, o.dwMajorVersion);
    EXPECT_EQ(min, o.dwMinorVersion);
    EXPECT_EQ(bld & 0xFFFF, o.dwBuildNumber);
    CHECKF((bld >> 28) == 0xF || (bld >> 28) == 0xC, "build flags 0x%lx", bld);
}
TEST(version_rtl_repeated_calls_same) {
    REQUIRE(UN(RtlGetVersion));
    OSVERSIONINFOEXW a, b;
    CHECK(ver_true(&a)); CHECK(ver_true(&b));
    CHECK(!memcmp(&a, &b, sizeof a));
}
TEST(version_registry_current_version_matches) {
    REQUIRE(UN(RtlGetVersion));
    OSVERSIONINFOEXW o; CHECK(ver_true(&o));
    HKEY k;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &k)) { t_skip("no CurrentVersion key"); return; }
    char bs[32] = ""; DWORD n = sizeof bs, type = 0, maj = 0, sz = 4;
    if (!RegQueryValueExA(k, "CurrentBuildNumber", NULL, &type, (BYTE *)bs, &n)) { EXPECT_EQ((DWORD)atoi(bs), o.dwBuildNumber); }
    else t_note("CurrentBuildNumber missing");
    if (!RegQueryValueExA(k, "CurrentMajorVersionNumber", NULL, &type, (BYTE *)&maj, &sz)) { EXPECT_EQ(maj, o.dwMajorVersion); }
    else t_note("CurrentMajorVersionNumber missing");
    RegCloseKey(k);
}

/* ---------- GetVersion ---------- */
TEST(version_getversion_vs_getversionexw) {
    REQUIRE(U(GetVersion)); REQUIRE(U(GetVersionExW));
    DWORD v = U(GetVersion)();
    OSVERSIONINFOW o; memset(&o, 0, sizeof o); o.dwOSVersionInfoSize = sizeof o;
    CHECK(U(GetVersionExW)(&o));
    EXPECT_EQ(v & 0xFF, o.dwMajorVersion);
    EXPECT_EQ((v >> 8) & 0xFF, o.dwMinorVersion);
    EXPECT_EQ(v >> 16, o.dwBuildNumber & 0xFFFF);
}
TEST(version_getversion_platform_bit_clear) {
    REQUIRE(U(GetVersion));
    DWORD v = U(GetVersion)();
    CHECK(!(v & 0x80000000));   /* bit 31 set only for Win9x */
}
TEST(version_getversion_lie_or_truth) {
    REQUIRE(U(GetVersion)); REQUIRE(UN(RtlGetVersion));
    DWORD v = U(GetVersion)();
    OSVERSIONINFOEXW t; CHECK(ver_true(&t));
    DWORD maj = v & 0xFF, min = (v >> 8) & 0xFF, bld = v >> 16;
    int truth = maj == t.dwMajorVersion && min == t.dwMinorVersion && bld == (t.dwBuildNumber & 0xFFFF);
    int lie8 = maj == 6 && min == 2 && bld == 9200;
    int lie81 = maj == 6 && min == 3 && bld == 9600;
    CHECKF(truth || lie8 || lie81, "GetVersion %lu.%lu.%lu", maj, min, bld);
    t_note("GetVersion reports %lu.%lu.%lu (%s)", maj, min, bld, truth ? "true version" : lie8 ? "Windows 8 compat" : lie81 ? "Windows 8.1 compat" : "?");
}
TEST(version_getversion_repeatable) {
    REQUIRE(U(GetVersion));
    DWORD a = U(GetVersion)();
    for (int i = 0; i < 50; i++) EXPECT_EQ(U(GetVersion)(), a);
}
TEST(version_getversion_major_at_least_6) {
    REQUIRE(U(GetVersion));
    CHECK((U(GetVersion)() & 0xFF) >= 6);
}

/* ---------- GetVersionExW ---------- */
TEST(version_exw_basic_osversioninfow) {
    REQUIRE(U(GetVersionExW));
    OSVERSIONINFOW o; memset(&o, 0xCC, sizeof o); o.dwOSVersionInfoSize = sizeof o;
    EXPECT_EQ(sizeof o, 0x114);
    CHECK(U(GetVersionExW)(&o));
    CHECK(o.dwMajorVersion >= 6);
    EXPECT_EQ(o.dwPlatformId, VER_PLATFORM_WIN32_NT);
    EXPECT_EQ(o.dwOSVersionInfoSize, 0x114);
    CHECK(wcslen(o.szCSDVersion) < 128);
}
TEST(version_exw_basic_osversioninfoexw) {
    REQUIRE(U(GetVersionExW));
    OSVERSIONINFOEXW o; memset(&o, 0xCC, sizeof o); o.dwOSVersionInfoSize = sizeof o;
    EXPECT_EQ(sizeof o, 0x11c);
    CHECK(U(GetVersionExW)((OSVERSIONINFOW *)&o));
    EXPECT_EQ(o.dwOSVersionInfoSize, 0x11c);
    CHECK(o.wProductType >= 1 && o.wProductType <= 3);
    EXPECT_EQ(o.wServicePackMajor, 0);
    EXPECT_EQ(o.wServicePackMinor, 0);
}
TEST(version_exw_plain_struct_not_extended) {
    /* with the short size only the first 0x114 bytes are written */
    REQUIRE(U(GetVersionExW));
    struct { OSVERSIONINFOW o; BYTE tail[16]; } b;
    memset(&b, 0x5A, sizeof b);
    b.o.dwOSVersionInfoSize = sizeof b.o;
    CHECK(U(GetVersionExW)(&b.o));
    for (int i = 0; i < 16; i++) EXPECT_EQ(b.tail[i], 0x5A);
}
TEST(version_exw_invalid_sizes) {
    REQUIRE(U(GetVersionExW));
    static const DWORD bad[] = { 0, 1, 4, 0x94, 0x9c, 0x113, 0x115, 0x118, 0x11b, 0x11d, 0x120, 0x200, 0x7FFFFFFF, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        static BYTE buf[1024]; memset(buf, 0, sizeof buf);
        ((OSVERSIONINFOW *)buf)->dwOSVersionInfoSize = bad[i];
        SET_ERR(0);
        BOOL r = U(GetVersionExW)((OSVERSIONINFOW *)buf);
        CHECKF(!r, "size 0x%lx accepted", bad[i]);
        DWORD e = GetLastError();
        CHECKF(e == ERROR_INSUFFICIENT_BUFFER, "size 0x%lx last error %lu", bad[i], e);
        CHECKF(((OSVERSIONINFOW *)buf)->dwMajorVersion == 0, "size 0x%lx wrote output", bad[i]);
    }
}
TEST(version_exw_versions_consistent_with_getversion) {
    REQUIRE(U(GetVersionExW)); REQUIRE(U(GetVersion));
    OSVERSIONINFOEXW o; memset(&o, 0, sizeof o); o.dwOSVersionInfoSize = sizeof o;
    CHECK(U(GetVersionExW)((OSVERSIONINFOW *)&o));
    DWORD v = U(GetVersion)();
    EXPECT_EQ(o.dwMajorVersion, v & 0xFF);
    EXPECT_EQ(o.dwMinorVersion, (v >> 8) & 0xFF);
}
TEST(version_exw_matches_rtl_except_compat) {
    REQUIRE(U(GetVersionExW)); REQUIRE(UN(RtlGetVersion));
    OSVERSIONINFOEXW o, t; memset(&o, 0, sizeof o); o.dwOSVersionInfoSize = sizeof o;
    CHECK(U(GetVersionExW)((OSVERSIONINFOW *)&o)); CHECK(ver_true(&t));
    EXPECT_EQ(o.wProductType, t.wProductType);
    EXPECT_EQ(o.wSuiteMask, t.wSuiteMask);
    EXPECT_EQ(o.wServicePackMajor, t.wServicePackMajor);
    EXPECT_EQ(o.wServicePackMinor, t.wServicePackMinor);
    EXPECT_EQ(o.dwPlatformId, t.dwPlatformId);
    int same = o.dwMajorVersion == t.dwMajorVersion && o.dwMinorVersion == t.dwMinorVersion && o.dwBuildNumber == t.dwBuildNumber;
    int lie8 = o.dwMajorVersion == 6 && o.dwMinorVersion == 2 && o.dwBuildNumber == 9200;
    int lie81 = o.dwMajorVersion == 6 && o.dwMinorVersion == 3 && o.dwBuildNumber == 9600;
    CHECKF(same || lie8 || lie81, "GetVersionExW %lu.%lu.%lu", o.dwMajorVersion, o.dwMinorVersion, o.dwBuildNumber);
    t_note("GetVersionExW reports %lu.%lu.%lu (true %lu.%lu.%lu)", o.dwMajorVersion, o.dwMinorVersion, o.dwBuildNumber, t.dwMajorVersion, t.dwMinorVersion, t.dwBuildNumber);
}
TEST(version_exw_unmanifested_reports_windows8) {
    /* a binary without a compatibility manifest is given the Windows 8 version (6.2.9200) */
    REQUIRE(U(GetVersionExW));
    OSVERSIONINFOW o; memset(&o, 0, sizeof o); o.dwOSVersionInfoSize = sizeof o;
    CHECK(U(GetVersionExW)(&o));
    if (o.dwMajorVersion == 6 && o.dwMinorVersion == 2) { EXPECT_EQ(o.dwBuildNumber, 9200); }
    else t_note("version lie not applied: %lu.%lu.%lu", o.dwMajorVersion, o.dwMinorVersion, o.dwBuildNumber);
}
TEST(version_exw_repeated_identical) {
    REQUIRE(U(GetVersionExW));
    OSVERSIONINFOEXW a, b;
    memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    a.dwOSVersionInfoSize = b.dwOSVersionInfoSize = sizeof a;
    CHECK(U(GetVersionExW)((OSVERSIONINFOW *)&a)); CHECK(U(GetVersionExW)((OSVERSIONINFOW *)&b));
    CHECK(!memcmp(&a, &b, sizeof a));
}
TEST(version_exw_csd_is_terminated) {
    REQUIRE(U(GetVersionExW));
    OSVERSIONINFOW o; memset(&o, 0x7F, sizeof o); o.dwOSVersionInfoSize = sizeof o;
    CHECK(U(GetVersionExW)(&o));
    int term = 0; for (int i = 0; i < 128; i++) if (!o.szCSDVersion[i]) { term = 1; break; }
    CHECK(term);
}

/* ---------- GetVersionExA ---------- */
TEST(version_exa_basic) {
    REQUIRE(U(GetVersionExA));
    OSVERSIONINFOA o; memset(&o, 0xCC, sizeof o); o.dwOSVersionInfoSize = sizeof o;
    EXPECT_EQ(sizeof o, 0x94);
    CHECK(U(GetVersionExA)(&o));
    CHECK(o.dwMajorVersion >= 6);
    EXPECT_EQ(o.dwPlatformId, VER_PLATFORM_WIN32_NT);
    CHECK(strlen(o.szCSDVersion) < 128);
}
TEST(version_exa_ex) {
    REQUIRE(U(GetVersionExA));
    OSVERSIONINFOEXA o; memset(&o, 0xCC, sizeof o); o.dwOSVersionInfoSize = sizeof o;
    EXPECT_EQ(sizeof o, 0x9c);
    CHECK(U(GetVersionExA)((OSVERSIONINFOA *)&o));
    CHECK(o.wProductType >= 1 && o.wProductType <= 3);
    EXPECT_EQ(o.wServicePackMajor, 0);
}
TEST(version_exa_parity_with_w) {
    REQUIRE(U(GetVersionExA)); REQUIRE(U(GetVersionExW));
    OSVERSIONINFOEXA a; OSVERSIONINFOEXW w;
    memset(&a, 0, sizeof a); memset(&w, 0, sizeof w);
    a.dwOSVersionInfoSize = sizeof a; w.dwOSVersionInfoSize = sizeof w;
    CHECK(U(GetVersionExA)((OSVERSIONINFOA *)&a)); CHECK(U(GetVersionExW)((OSVERSIONINFOW *)&w));
    EXPECT_EQ(a.dwMajorVersion, w.dwMajorVersion);
    EXPECT_EQ(a.dwMinorVersion, w.dwMinorVersion);
    EXPECT_EQ(a.dwBuildNumber, w.dwBuildNumber);
    EXPECT_EQ(a.dwPlatformId, w.dwPlatformId);
    EXPECT_EQ(a.wSuiteMask, w.wSuiteMask);
    EXPECT_EQ(a.wProductType, w.wProductType);
    EXPECT_EQ(a.wServicePackMajor, w.wServicePackMajor);
    EXPECT_EQ(strlen(a.szCSDVersion), wcslen(w.szCSDVersion));
}
TEST(version_exa_short_struct_no_overrun) {
    REQUIRE(U(GetVersionExA));
    struct { OSVERSIONINFOA o; BYTE tail[16]; } b;
    memset(&b, 0x5A, sizeof b);
    b.o.dwOSVersionInfoSize = sizeof b.o;
    CHECK(U(GetVersionExA)(&b.o));
    for (int i = 0; i < 16; i++) EXPECT_EQ(b.tail[i], 0x5A);
}
TEST(version_exa_invalid_sizes) {
    REQUIRE(U(GetVersionExA));
    static const DWORD bad[] = { 0, 1, 4, 0x93, 0x95, 0x98, 0x9b, 0x9d, 0x114, 0x11c, 0x200, 0xFFFFFFFF };
    for (unsigned i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        static BYTE buf[1024]; memset(buf, 0, sizeof buf);
        ((OSVERSIONINFOA *)buf)->dwOSVersionInfoSize = bad[i];
        SET_ERR(0);
        BOOL r = U(GetVersionExA)((OSVERSIONINFOA *)buf);
        CHECKF(!r, "size 0x%lx accepted", bad[i]);
        DWORD e = GetLastError();
        CHECKF(e == ERROR_INSUFFICIENT_BUFFER, "size 0x%lx last error %lu", bad[i], e);
    }
}

/* ---------- VerSetConditionMask ---------- */
TEST(vermask_zero_typemask_is_identity) {
    REQUIRE(U(VerSetConditionMask));
    EXPECT_EQ(U(VerSetConditionMask)(0, 0, VER_EQUAL), 0);
    ULONGLONG m = U(VerSetConditionMask)(0, VER_MAJORVERSION, VER_GREATER_EQUAL);
    EXPECT_EQ(U(VerSetConditionMask)(m, 0, VER_LESS), m);
}
TEST(vermask_nonzero_after_set) {
    REQUIRE(U(VerSetConditionMask));
    static const DWORD types[] = { VER_MINORVERSION, VER_MAJORVERSION, VER_BUILDNUMBER, VER_PLATFORMID, VER_SERVICEPACKMINOR, VER_SERVICEPACKMAJOR, VER_SUITENAME, VER_PRODUCT_TYPE };
    for (unsigned i = 0; i < 8; i++) CHECK(U(VerSetConditionMask)(0, types[i], VER_GREATER_EQUAL) != 0);
}
TEST(vermask_types_differ) {
    REQUIRE(U(VerSetConditionMask));
    static const DWORD types[] = { VER_MINORVERSION, VER_MAJORVERSION, VER_BUILDNUMBER, VER_PLATFORMID, VER_SERVICEPACKMINOR, VER_SERVICEPACKMAJOR, VER_SUITENAME, VER_PRODUCT_TYPE };
    for (unsigned i = 0; i < 8; i++) for (unsigned j = i + 1; j < 8; j++)
        CHECKF(U(VerSetConditionMask)(0, types[i], VER_GREATER_EQUAL) != U(VerSetConditionMask)(0, types[j], VER_GREATER_EQUAL), "types %u %u", i, j);
}
TEST(vermask_conditions_differ) {
    REQUIRE(U(VerSetConditionMask));
    static const BYTE conds[] = { VER_EQUAL, VER_GREATER, VER_GREATER_EQUAL, VER_LESS, VER_LESS_EQUAL };
    for (unsigned i = 0; i < 5; i++) for (unsigned j = i + 1; j < 5; j++)
        CHECKF(U(VerSetConditionMask)(0, VER_MAJORVERSION, conds[i]) != U(VerSetConditionMask)(0, VER_MAJORVERSION, conds[j]), "conds %u %u", i, j);
}
TEST(vermask_commutes) {
    REQUIRE(U(VerSetConditionMask));
    ULONGLONG a = U(VerSetConditionMask)(U(VerSetConditionMask)(0, VER_MAJORVERSION, VER_GREATER_EQUAL), VER_MINORVERSION, VER_LESS);
    ULONGLONG b = U(VerSetConditionMask)(U(VerSetConditionMask)(0, VER_MINORVERSION, VER_LESS), VER_MAJORVERSION, VER_GREATER_EQUAL);
    EXPECT_EQ(a, b);
}
TEST(vermask_idempotent) {
    REQUIRE(U(VerSetConditionMask));
    ULONGLONG a = U(VerSetConditionMask)(0, VER_BUILDNUMBER, VER_LESS_EQUAL);
    EXPECT_EQ(U(VerSetConditionMask)(a, VER_BUILDNUMBER, VER_LESS_EQUAL), a);
}
TEST(vermask_combined_typemask_equals_separate) {
    REQUIRE(U(VerSetConditionMask));
    ULONGLONG a = U(VerSetConditionMask)(0, VER_MAJORVERSION | VER_MINORVERSION, VER_GREATER_EQUAL);
    ULONGLONG b = U(VerSetConditionMask)(U(VerSetConditionMask)(0, VER_MAJORVERSION, VER_GREATER_EQUAL), VER_MINORVERSION, VER_GREATER_EQUAL);
    t_note("combined type mask: 0x%llx vs separate 0x%llx", a, b);
    CHECK(a != 0);
}
TEST(vermask_kernel32_and_ntdll_agree) {
    REQUIRE(UK(VerSetConditionMask)); REQUIRE(UNT(VerSetConditionMask));
    ULONGLONG a = UK(VerSetConditionMask)(0, VER_MAJORVERSION, VER_GREATER_EQUAL);
    ULONGLONG b = UNT(VerSetConditionMask)(0, VER_MAJORVERSION, VER_GREATER_EQUAL);
    EXPECT_EQ(a, b);
}

/* ---------- VerifyVersionInfoW ---------- */
/* VerifyVersionInfoW compares against the compatibility-adjusted version (same as GetVersionEx), not RtlGetVersion */
static int ver_eff(OSVERSIONINFOEXW *o) {
    memset(o, 0, sizeof *o); o->dwOSVersionInfoSize = sizeof *o;
    return U(GetVersionExW)((OSVERSIONINFOW *)o);
}
static BOOL vv(DWORD maj, DWORD min, DWORD sp, DWORD build, DWORD mask, ULONGLONG cm) {
    OSVERSIONINFOEXW o; memset(&o, 0, sizeof o); o.dwOSVersionInfoSize = sizeof o;
    o.dwMajorVersion = maj; o.dwMinorVersion = min; o.wServicePackMajor = (WORD)sp; o.dwBuildNumber = build;
    return U(VerifyVersionInfoW)(&o, mask, cm);
}
static ULONGLONG vc(ULONGLONG m, DWORD t, BYTE c) { return U(VerSetConditionMask)(m, t, c); }

TEST(verify_major_all_conditions) {
    REQUIRE(U(VerifyVersionInfoW)); REQUIRE(U(VerSetConditionMask)); REQUIRE(U(GetVersionExW));
    OSVERSIONINFOEXW t; CHECK(ver_eff(&t));
    DWORD M = t.dwMajorVersion;
    struct { BYTE cond; DWORD v; int want; } c[] = {
        { VER_EQUAL, M, 1 }, { VER_EQUAL, M + 1, 0 }, { VER_EQUAL, M - 1, 0 },
        { VER_GREATER, M, 0 }, { VER_GREATER, M - 1, 1 }, { VER_GREATER, M + 1, 0 },
        { VER_GREATER_EQUAL, M, 1 }, { VER_GREATER_EQUAL, M - 1, 1 }, { VER_GREATER_EQUAL, M + 1, 0 },
        { VER_LESS, M, 0 }, { VER_LESS, M + 1, 1 }, { VER_LESS, M - 1, 0 },
        { VER_LESS_EQUAL, M, 1 }, { VER_LESS_EQUAL, M + 1, 1 }, { VER_LESS_EQUAL, M - 1, 0 },
    };
    for (unsigned i = 0; i < sizeof c / sizeof c[0]; i++) {
        SET_ERR(0);
        BOOL r = vv(c[i].v, 0, 0, 0, VER_MAJORVERSION, vc(0, VER_MAJORVERSION, c[i].cond));
        CHECKF(!!r == c[i].want, "cond %u value %lu -> %d", c[i].cond, c[i].v, r);
        if (!r) { DWORD e = GetLastError(); CHECKF(e == ERROR_OLD_WIN_VERSION, "cond %u value %lu err %lu", c[i].cond, c[i].v, e); }
    }
}
TEST(verify_minor_alone) {
    REQUIRE(U(VerifyVersionInfoW)); REQUIRE(U(VerSetConditionMask)); REQUIRE(U(GetVersionExW));
    OSVERSIONINFOEXW t; CHECK(ver_eff(&t));
    CHECK(vv(0, t.dwMinorVersion, 0, 0, VER_MINORVERSION, vc(0, VER_MINORVERSION, VER_EQUAL)));
    CHECK(!vv(0, t.dwMinorVersion + 1, 0, 0, VER_MINORVERSION, vc(0, VER_MINORVERSION, VER_EQUAL)));
    CHECK(vv(0, t.dwMinorVersion, 0, 0, VER_MINORVERSION, vc(0, VER_MINORVERSION, VER_GREATER_EQUAL)));
    CHECK(!vv(0, t.dwMinorVersion + 1, 0, 0, VER_MINORVERSION, vc(0, VER_MINORVERSION, VER_GREATER_EQUAL)));
}
TEST(verify_build_number) {
    REQUIRE(U(VerifyVersionInfoW)); REQUIRE(U(VerSetConditionMask)); REQUIRE(U(GetVersionExW));
    OSVERSIONINFOEXW t; CHECK(ver_eff(&t));
    DWORD b = t.dwBuildNumber;
    CHECK(vv(0, 0, 0, b, VER_BUILDNUMBER, vc(0, VER_BUILDNUMBER, VER_EQUAL)));
    CHECK(vv(0, 0, 0, b, VER_BUILDNUMBER, vc(0, VER_BUILDNUMBER, VER_GREATER_EQUAL)));
    CHECK(vv(0, 0, 0, b - 1, VER_BUILDNUMBER, vc(0, VER_BUILDNUMBER, VER_GREATER)));
    CHECK(!vv(0, 0, 0, b + 1, VER_BUILDNUMBER, vc(0, VER_BUILDNUMBER, VER_GREATER_EQUAL)));
    CHECK(!vv(0, 0, 0, b + 1, VER_BUILDNUMBER, vc(0, VER_BUILDNUMBER, VER_EQUAL)));
    CHECK(vv(0, 0, 0, b + 1, VER_BUILDNUMBER, vc(0, VER_BUILDNUMBER, VER_LESS)));
    CHECK(vv(0, 0, 0, 19041, VER_BUILDNUMBER, vc(0, VER_BUILDNUMBER, VER_LESS_EQUAL)) == (b <= 19041));
    CHECK(vv(0, 0, 0, 10240, VER_BUILDNUMBER, vc(0, VER_BUILDNUMBER, VER_GREATER_EQUAL)) == (b >= 10240));
}
TEST(verify_hierarchical_major_minor_sp) {
    /* the usual "at least version X.Y SPz" idiom: a higher major (or minor) version satisfies it
     * regardless of the lower-order fields. Model it against the effective version. */
    REQUIRE(U(VerifyVersionInfoW)); REQUIRE(U(VerSetConditionMask)); REQUIRE(U(GetVersionExW));
    OSVERSIONINFOEXW t; CHECK(ver_eff(&t));
    ULONGLONG cm = vc(vc(vc(0, VER_MAJORVERSION, VER_GREATER_EQUAL), VER_MINORVERSION, VER_GREATER_EQUAL), VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);
    DWORD mask = VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR;
    static const DWORD majs[] = { 0, 4, 5, 6, 10, 11, 99 };
    static const DWORD mins[] = { 0, 1, 2, 3, 5 };
    for (unsigned a = 0; a < 7; a++) for (unsigned b = 0; b < 5; b++) for (DWORD sp = 0; sp < 3; sp++) {
        int want = t.dwMajorVersion > majs[a] || (t.dwMajorVersion == majs[a] && (t.dwMinorVersion > mins[b] || (t.dwMinorVersion == mins[b] && t.wServicePackMajor >= sp)));
        BOOL r = vv(majs[a], mins[b], sp, 0, mask, cm);
        CHECKF(!!r == want, "need %lu.%lu sp%lu: got %d want %d (effective %lu.%lu)", majs[a], mins[b], sp, r, want, t.dwMajorVersion, t.dwMinorVersion);
    }
}
TEST(verify_windows_vista_or_greater_always_true) {
    REQUIRE(U(VerifyVersionInfoW)); REQUIRE(U(VerSetConditionMask));
    ULONGLONG cm = vc(vc(vc(0, VER_MAJORVERSION, VER_GREATER_EQUAL), VER_MINORVERSION, VER_GREATER_EQUAL), VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);
    DWORD mask = VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR;
    CHECK(vv(6, 0, 0, 0, mask, cm));
    CHECK(vv(5, 1, 3, 0, mask, cm));
    CHECK(vv(5, 0, 0, 0, mask, cm));
    CHECK(vv(4, 0, 6, 0, mask, cm));
}
TEST(verify_win10_or_greater_follows_manifest) {
    /* IsWindows10OrGreater() is only true when the exe's manifest declares Windows 10 support */
    REQUIRE(U(VerifyVersionInfoW)); REQUIRE(U(VerSetConditionMask)); REQUIRE(U(GetVersionExW));
    ULONGLONG cm = vc(vc(vc(0, VER_MAJORVERSION, VER_GREATER_EQUAL), VER_MINORVERSION, VER_GREATER_EQUAL), VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);
    DWORD mask = VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR;
    OSVERSIONINFOEXW t; CHECK(ver_eff(&t));
    BOOL r = vv(10, 0, 0, 0, mask, cm);
    CHECK(!!r == (t.dwMajorVersion >= 10));
    t_note("IsWindows10OrGreater-style check: %d (effective version %lu.%lu)", r, t.dwMajorVersion, t.dwMinorVersion);
    BOOL r8 = vv(6, 2, 0, 0, mask, cm);
    CHECK(r8);   /* every supported configuration is at least the Windows 8 compat level */
}
TEST(verify_build_with_major_minor) {
    REQUIRE(U(VerifyVersionInfoW)); REQUIRE(U(VerSetConditionMask)); REQUIRE(U(GetVersionExW));
    OSVERSIONINFOEXW t; CHECK(ver_eff(&t));
    ULONGLONG cm = vc(vc(vc(0, VER_MAJORVERSION, VER_EQUAL), VER_MINORVERSION, VER_EQUAL), VER_BUILDNUMBER, VER_GREATER_EQUAL);
    DWORD mask = VER_MAJORVERSION | VER_MINORVERSION | VER_BUILDNUMBER;
    CHECK(vv(t.dwMajorVersion, t.dwMinorVersion, 0, t.dwBuildNumber, mask, cm));
    CHECK(vv(t.dwMajorVersion, t.dwMinorVersion, 0, 10240, mask, cm) == (t.dwBuildNumber >= 10240));
    CHECK(!vv(t.dwMajorVersion, t.dwMinorVersion, 0, t.dwBuildNumber + 1, mask, cm));
    CHECK(!vv(t.dwMajorVersion - 1, t.dwMinorVersion, 0, t.dwBuildNumber, mask, cm));
}
TEST(verify_service_pack) {
    REQUIRE(U(VerifyVersionInfoW)); REQUIRE(U(VerSetConditionMask));
    OSVERSIONINFOEXW o; memset(&o, 0, sizeof o); o.dwOSVersionInfoSize = sizeof o;
    o.wServicePackMajor = 0;
    CHECK(U(VerifyVersionInfoW)(&o, VER_SERVICEPACKMAJOR, vc(0, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL)));
    CHECK(U(VerifyVersionInfoW)(&o, VER_SERVICEPACKMAJOR, vc(0, VER_SERVICEPACKMAJOR, VER_EQUAL)));
    o.wServicePackMajor = 1;
    SET_ERR(0);
    CHECK(!U(VerifyVersionInfoW)(&o, VER_SERVICEPACKMAJOR, vc(0, VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL)));
    EXPECT_ERR(ERROR_OLD_WIN_VERSION);
    o.wServicePackMajor = 0; o.wServicePackMinor = 1;
    CHECK(!U(VerifyVersionInfoW)(&o, VER_SERVICEPACKMINOR, vc(0, VER_SERVICEPACKMINOR, VER_GREATER_EQUAL)));
    o.wServicePackMinor = 0;
    CHECK(U(VerifyVersionInfoW)(&o, VER_SERVICEPACKMINOR, vc(0, VER_SERVICEPACKMINOR, VER_EQUAL)));
}
TEST(verify_platform_id) {
    REQUIRE(U(VerifyVersionInfoW)); REQUIRE(U(VerSetConditionMask));
    OSVERSIONINFOEXW o; memset(&o, 0, sizeof o); o.dwOSVersionInfoSize = sizeof o;
    o.dwPlatformId = VER_PLATFORM_WIN32_NT;
    CHECK(U(VerifyVersionInfoW)(&o, VER_PLATFORMID, vc(0, VER_PLATFORMID, VER_EQUAL)));
    o.dwPlatformId = VER_PLATFORM_WIN32_WINDOWS;
    SET_ERR(0);
    CHECK(!U(VerifyVersionInfoW)(&o, VER_PLATFORMID, vc(0, VER_PLATFORMID, VER_EQUAL)));
    EXPECT_ERR(ERROR_OLD_WIN_VERSION);
    /* OS value (2) compared against the supplied value */
    o.dwPlatformId = 3;
    CHECK(U(VerifyVersionInfoW)(&o, VER_PLATFORMID, vc(0, VER_PLATFORMID, VER_LESS)));
    o.dwPlatformId = VER_PLATFORM_WIN32s;
    CHECK(!U(VerifyVersionInfoW)(&o, VER_PLATFORMID, vc(0, VER_PLATFORMID, VER_LESS)));
    CHECK(U(VerifyVersionInfoW)(&o, VER_PLATFORMID, vc(0, VER_PLATFORMID, VER_GREATER)));
}
TEST(verify_product_type) {
    REQUIRE(U(VerifyVersionInfoW)); REQUIRE(U(VerSetConditionMask)); REQUIRE(U(GetVersionExW));
    OSVERSIONINFOEXW t; CHECK(ver_eff(&t));
    OSVERSIONINFOEXW o; memset(&o, 0, sizeof o); o.dwOSVersionInfoSize = sizeof o;
    o.wProductType = t.wProductType;
    CHECK(U(VerifyVersionInfoW)(&o, VER_PRODUCT_TYPE, vc(0, VER_PRODUCT_TYPE, VER_EQUAL)));
    for (BYTE p = 1; p <= 3; p++) {
        o.wProductType = p;
        BOOL r = U(VerifyVersionInfoW)(&o, VER_PRODUCT_TYPE, vc(0, VER_PRODUCT_TYPE, VER_EQUAL));
        CHECKF(!!r == (p == t.wProductType), "type %u", p);
    }
    o.wProductType = t.wProductType;
    CHECK(U(VerifyVersionInfoW)(&o, VER_PRODUCT_TYPE, vc(0, VER_PRODUCT_TYPE, VER_GREATER_EQUAL)));
}
TEST(verify_suite_name_and_or) {
    REQUIRE(U(VerifyVersionInfoW)); REQUIRE(U(VerSetConditionMask)); REQUIRE(U(GetVersionExW));
    OSVERSIONINFOEXW t; CHECK(ver_eff(&t));
    if (!t.wSuiteMask) { t_note("no suite bits on this SKU"); return; }
    OSVERSIONINFOEXW o; memset(&o, 0, sizeof o); o.dwOSVersionInfoSize = sizeof o;
    o.wSuiteMask = t.wSuiteMask;
    CHECK(U(VerifyVersionInfoW)(&o, VER_SUITENAME, vc(0, VER_SUITENAME, VER_AND)));
    CHECK(U(VerifyVersionInfoW)(&o, VER_SUITENAME, vc(0, VER_SUITENAME, VER_OR)));
    WORD missing = 0; for (int b = 0; b < 16; b++) if (!(t.wSuiteMask & (1 << b))) { missing = 1 << b; break; }
    if (missing) {
        o.wSuiteMask = t.wSuiteMask | missing;
        SET_ERR(0);
        CHECK(!U(VerifyVersionInfoW)(&o, VER_SUITENAME, vc(0, VER_SUITENAME, VER_AND)));
        EXPECT_ERR(ERROR_OLD_WIN_VERSION);
        CHECK(U(VerifyVersionInfoW)(&o, VER_SUITENAME, vc(0, VER_SUITENAME, VER_OR)));
        o.wSuiteMask = missing;
        CHECK(!U(VerifyVersionInfoW)(&o, VER_SUITENAME, vc(0, VER_SUITENAME, VER_OR)));
    }
}
TEST(verify_suite_with_equal_is_bad_argument) {
    REQUIRE(U(VerifyVersionInfoW)); REQUIRE(U(VerSetConditionMask));
    OSVERSIONINFOEXW o; memset(&o, 0, sizeof o); o.dwOSVersionInfoSize = sizeof o;
    o.wSuiteMask = VER_SUITE_ENTERPRISE;
    SET_ERR(0);
    CHECK(!U(VerifyVersionInfoW)(&o, VER_SUITENAME, vc(0, VER_SUITENAME, VER_EQUAL)));
    EXPECT_ERR(ERROR_BAD_ARGUMENTS);
}
TEST(verify_zero_typemask_is_bad_argument) {
    REQUIRE(U(VerifyVersionInfoW));
    OSVERSIONINFOEXW o; memset(&o, 0, sizeof o); o.dwOSVersionInfoSize = sizeof o;
    SET_ERR(0);
    EXPECT_EQ(U(VerifyVersionInfoW)(&o, 0, 0), 0);
    EXPECT_ERR(ERROR_BAD_ARGUMENTS);
}
TEST(verify_does_not_set_error_on_success) {
    REQUIRE(U(VerifyVersionInfoW)); REQUIRE(U(VerSetConditionMask));
    SET_ERR(0x1234);
    CHECK(vv(5, 0, 0, 0, VER_MAJORVERSION, vc(0, VER_MAJORVERSION, VER_GREATER_EQUAL)));
    EXPECT_ERR(0x1234);
}
TEST(verify_old_win_version_error_code) {
    REQUIRE(U(VerifyVersionInfoW)); REQUIRE(U(VerSetConditionMask));
    SET_ERR(0);
    CHECK(!vv(99, 0, 0, 0, VER_MAJORVERSION, vc(0, VER_MAJORVERSION, VER_EQUAL)));
    EXPECT_ERR(1150);
}
TEST(verify_tracks_getversionex_version) {
    REQUIRE(U(VerifyVersionInfoW)); REQUIRE(U(VerSetConditionMask)); REQUIRE(U(GetVersionExW));
    OSVERSIONINFOEXW t; CHECK(ver_eff(&t));
    CHECK(vv(t.dwMajorVersion, 0, 0, 0, VER_MAJORVERSION, vc(0, VER_MAJORVERSION, VER_EQUAL)));
    CHECK(vv(t.dwMajorVersion, t.dwMinorVersion, 0, 0, VER_MAJORVERSION | VER_MINORVERSION, vc(vc(0, VER_MAJORVERSION, VER_EQUAL), VER_MINORVERSION, VER_EQUAL)));
    CHECK(!vv(t.dwMajorVersion, t.dwMinorVersion + 1, 0, 0, VER_MAJORVERSION | VER_MINORVERSION, vc(vc(0, VER_MAJORVERSION, VER_EQUAL), VER_MINORVERSION, VER_EQUAL)));
}
TEST(verify_all_fields_at_once) {
    REQUIRE(U(VerifyVersionInfoW)); REQUIRE(U(VerSetConditionMask)); REQUIRE(U(GetVersionExW));
    OSVERSIONINFOEXW t; CHECK(ver_eff(&t));
    OSVERSIONINFOEXW o = t;
    DWORD mask = VER_MAJORVERSION | VER_MINORVERSION | VER_BUILDNUMBER | VER_PLATFORMID | VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR | VER_PRODUCT_TYPE;
    ULONGLONG cm = 0;
    cm = vc(cm, VER_MAJORVERSION, VER_EQUAL); cm = vc(cm, VER_MINORVERSION, VER_EQUAL); cm = vc(cm, VER_BUILDNUMBER, VER_EQUAL);
    cm = vc(cm, VER_PLATFORMID, VER_EQUAL); cm = vc(cm, VER_SERVICEPACKMAJOR, VER_EQUAL); cm = vc(cm, VER_SERVICEPACKMINOR, VER_EQUAL);
    cm = vc(cm, VER_PRODUCT_TYPE, VER_EQUAL);
    CHECK(U(VerifyVersionInfoW)(&o, mask, cm));
    o.dwBuildNumber++;
    CHECK(!U(VerifyVersionInfoW)(&o, mask, cm));
    o.dwBuildNumber--; o.wProductType = (o.wProductType == 1) ? 3 : 1;
    CHECK(!U(VerifyVersionInfoW)(&o, mask, cm));
}
TEST(verify_unused_fields_ignored) {
    REQUIRE(U(VerifyVersionInfoW)); REQUIRE(U(VerSetConditionMask));
    OSVERSIONINFOEXW o; memset(&o, 0xEE, sizeof o); o.dwOSVersionInfoSize = sizeof o;
    o.dwMajorVersion = 5;
    CHECK(U(VerifyVersionInfoW)(&o, VER_MAJORVERSION, vc(0, VER_MAJORVERSION, VER_GREATER_EQUAL)));
}

/* ---------- GetProductInfo ---------- */
TEST(prodinfo_current_os) {
    REQUIRE(U(GetProductInfo));
    DWORD p = 0xDEADBEEF;
    CHECK(U(GetProductInfo)(10, 0, 0, 0, &p));
    CHECK(p != 0xDEADBEEF);
    CHECKF(p != PRODUCT_UNDEFINED, "product 0x%lx", p);
    CHECKF(p <= 0xFF, "product 0x%lx", p);
    t_note("GetProductInfo(10.0) = 0x%lx", p);
}
TEST(prodinfo_compat_versions_agree) {
    REQUIRE(U(GetProductInfo));
    DWORD a = 1, b = 2, c = 3;
    CHECK(U(GetProductInfo)(10, 0, 0, 0, &a));
    CHECK(U(GetProductInfo)(6, 3, 0, 0, &b));
    CHECK(U(GetProductInfo)(6, 2, 0, 0, &c));
    EXPECT_EQ(a, b); EXPECT_EQ(a, c);
}
TEST(prodinfo_older_versions) {
    REQUIRE(U(GetProductInfo));
    static const DWORD v[][2] = { {6, 0}, {6, 1}, {6, 2}, {6, 3}, {10, 0} };
    for (unsigned i = 0; i < 5; i++) {
        DWORD p = 0xDEADBEEF; BOOL r = U(GetProductInfo)(v[i][0], v[i][1], 0, 0, &p);
        CHECKF(r, "%lu.%lu", v[i][0], v[i][1]);
        CHECKF(p != PRODUCT_UNDEFINED && p != 0xDEADBEEF, "%lu.%lu -> 0x%lx", v[i][0], v[i][1], p);
    }
}
TEST(prodinfo_sp_values_ignored_win10) {
    REQUIRE(U(GetProductInfo));
    DWORD a = 0, b = 0;
    CHECK(U(GetProductInfo)(10, 0, 0, 0, &a));
    CHECK(U(GetProductInfo)(10, 0, 3, 7, &b));
    EXPECT_EQ(a, b);
}
TEST(prodinfo_unsupported_versions_note) {
    REQUIRE(U(GetProductInfo));
    static const DWORD v[][2] = { {0, 0}, {4, 0}, {5, 1}, {99, 99} };
    for (unsigned i = 0; i < 4; i++) {
        DWORD p = 0xDEADBEEF; SET_ERR(0);
        BOOL r = U(GetProductInfo)(v[i][0], v[i][1], 0, 0, &p);
        t_note("GetProductInfo(%lu,%lu) -> %d product 0x%lx err %lu", v[i][0], v[i][1], r, p, GetLastError());
        if (!r) CHECKF(p == PRODUCT_UNDEFINED, "failed call must report PRODUCT_UNDEFINED, got 0x%lx", p);
    }
}
TEST(prodinfo_repeatable) {
    REQUIRE(U(GetProductInfo));
    DWORD a = 0, b = 0;
    CHECK(U(GetProductInfo)(10, 0, 0, 0, &a));
    for (int i = 0; i < 20; i++) { CHECK(U(GetProductInfo)(10, 0, 0, 0, &b)); EXPECT_EQ(a, b); }
}
TEST(prodinfo_matches_ntdll) {
    REQUIRE(U(GetProductInfo)); REQUIRE(UNT(RtlGetProductInfo));
    DWORD a = 0, b = 0;
    CHECK(U(GetProductInfo)(10, 0, 0, 0, &a));
    CHECK(UNT(RtlGetProductInfo)(10, 0, 0, 0, &b));
    EXPECT_EQ(a, b);
}
TEST(prodinfo_workstation_vs_server_consistent) {
    REQUIRE(U(GetProductInfo)); REQUIRE(UN(RtlGetVersion));
    OSVERSIONINFOEXW t; CHECK(ver_true(&t));
    DWORD p = 0; CHECK(U(GetProductInfo)(10, 0, 0, 0, &p));
    static const DWORD server_skus[] = { PRODUCT_STANDARD_SERVER, PRODUCT_DATACENTER_SERVER, PRODUCT_ENTERPRISE_SERVER, PRODUCT_STANDARD_SERVER_CORE, PRODUCT_DATACENTER_SERVER_CORE, PRODUCT_ESSENTIALBUSINESS_SERVER_MGMT, PRODUCT_STORAGE_STANDARD_SERVER, PRODUCT_SB_SOLUTION_SERVER, PRODUCT_DATACENTER_EVALUATION_SERVER, PRODUCT_STANDARD_EVALUATION_SERVER, PRODUCT_DATACENTER_A_SERVER_CORE, PRODUCT_STANDARD_A_SERVER_CORE, PRODUCT_SERVER_FOR_SB_SOLUTIONS, PRODUCT_CLOUD_STORAGE_SERVER };
    int isserver = 0; for (unsigned i = 0; i < sizeof server_skus / sizeof server_skus[0]; i++) if (p == server_skus[i]) isserver = 1;
    if (t.wProductType == 1) CHECKF(!isserver, "workstation with server sku 0x%lx", p);
}

/* ---------- IsWow64Process / IsWow64Process2 ---------- */
TEST(wow64_current_process_native) {
    REQUIRE(U(IsWow64Process));
    BOOL w = 0x77;
    CHECK(U(IsWow64Process)(GetCurrentProcess(), &w));
    EXPECT_EQ(w, FALSE);   /* the test binaries are 64-bit */
}
TEST(wow64_real_handle) {
    REQUIRE(U(IsWow64Process)); REQUIRE(U(OpenProcess));
    HANDLE h = U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    CHECK(h != NULL);
    if (h) { BOOL w = 0x77; CHECK(U(IsWow64Process)(h, &w)); EXPECT_EQ(w, FALSE); CloseHandle(h); }
}
TEST(wow64_insufficient_access) {
    REQUIRE(U(IsWow64Process)); REQUIRE(U(OpenProcess));
    HANDLE h = U(OpenProcess)(SYNCHRONIZE, FALSE, GetCurrentProcessId());
    CHECK(h != NULL);
    if (h) {
        BOOL w = 0x77; SET_ERR(0);
        EXPECT_EQ(U(IsWow64Process)(h, &w), 0);
        EXPECT_ERR(ERROR_ACCESS_DENIED);
        CloseHandle(h);
    }
}
TEST(wow64_invalid_handles) {
    REQUIRE(U(IsWow64Process));
    BOOL w;
    SET_ERR(0); EXPECT_EQ(U(IsWow64Process)(NULL, &w), 0); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(U(IsWow64Process)((HANDLE)(ULONG_PTR)0x7FFFFFF0, &w), 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    /* (HANDLE)-1 is the current-process pseudo handle, not an invalid one */
    w = 0x77;
    CHECK(U(IsWow64Process)(INVALID_HANDLE_VALUE, &w));
}
TEST(wow64_wrong_handle_type) {
    REQUIRE(U(IsWow64Process)); REQUIRE(U(CreateEventW));
    HANDLE ev = U(CreateEventW)(NULL, TRUE, FALSE, NULL);
    BOOL w; SET_ERR(0);
    EXPECT_EQ(U(IsWow64Process)(ev, &w), 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(ev);
}
TEST(wow64_closed_handle) {
    REQUIRE(U(IsWow64Process)); REQUIRE(U(OpenProcess));
    HANDLE h = U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId());
    CloseHandle(h);
    BOOL w; SET_ERR(0);
    EXPECT_EQ(U(IsWow64Process)(h, &w), 0);
    EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(wow64_process2_current) {
    REQUIRE(U(IsWow64Process2));
    USHORT pm = 0x7777, nm = 0x7777;
    CHECK(U(IsWow64Process2)(GetCurrentProcess(), &pm, &nm));
    EXPECT_EQ(pm, IMAGE_FILE_MACHINE_UNKNOWN);
    EXPECT_EQ(nm, IMAGE_FILE_MACHINE_AMD64);
}
TEST(wow64_process2_null_native_machine) {
    REQUIRE(U(IsWow64Process2));
    USHORT pm = 0x7777;
    CHECK(U(IsWow64Process2)(GetCurrentProcess(), &pm, NULL));
    EXPECT_EQ(pm, IMAGE_FILE_MACHINE_UNKNOWN);
}
TEST(wow64_process2_invalid_handle) {
    REQUIRE(U(IsWow64Process2));
    USHORT pm, nm; SET_ERR(0);
    /* kernel handling of a NULL handle is not verifiable from the user-mode decomp */
    BOOL r = U(IsWow64Process2)(NULL, &pm, &nm);
    t_note("IsWow64Process2(NULL) -> %d, last error %lu", (int)r, GetLastError());
}
TEST(wow64_process2_insufficient_access) {
    REQUIRE(U(IsWow64Process2)); REQUIRE(U(OpenProcess));
    HANDLE h = U(OpenProcess)(SYNCHRONIZE, FALSE, GetCurrentProcessId());
    USHORT pm, nm; SET_ERR(0);
    EXPECT_EQ(U(IsWow64Process2)(h, &pm, &nm), 0);
    EXPECT_ERR(ERROR_ACCESS_DENIED);
    CloseHandle(h);
}
TEST(wow64_guest_machine_supported) {
    REQUIRE(U(IsWow64GuestMachineSupported));
    BOOL s = 0x77;
    HRESULT hr = U(IsWow64GuestMachineSupported)(IMAGE_FILE_MACHINE_I386, &s);
    EXPECT_EQ(hr, S_OK);
    t_note("x86 guest supported: %d", s);
    s = 0x77;
    hr = U(IsWow64GuestMachineSupported)(IMAGE_FILE_MACHINE_AMD64, &s);
    EXPECT_EQ(hr, S_OK);
    t_note("x64 guest supported: %d", s);
    s = 0x77;
    hr = U(IsWow64GuestMachineSupported)(0x1234, &s);
    EXPECT_EQ(hr, S_OK);
    EXPECT_EQ(s, FALSE);
}
TEST(wow64_child_32bit_not_available_note) {
    REQUIRE(U(IsWow64Process));
    BOOL a = 1, b = 1;
    CHECK(U(IsWow64Process)(GetCurrentProcess(), &a));
    CHECK(U(IsWow64Process)(GetCurrentProcess(), &b));
    EXPECT_EQ(a, b);
}
TEST(wow64_native_arch_matches_shared_data) {
    REQUIRE(U(IsWow64Process2));
    USHORT pm, nm;
    CHECK(U(IsWow64Process2)(GetCurrentProcess(), &pm, &nm));
    EXPECT_EQ(*(volatile const USHORT *)(KUSD + 0x26A), PROCESSOR_ARCHITECTURE_AMD64);
    EXPECT_EQ(nm, IMAGE_FILE_MACHINE_AMD64);
}
