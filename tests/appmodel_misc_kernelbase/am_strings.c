/* AppModel: pure string/identity parsing and formatting APIs. */
#define NTDDI_VERSION 0x0A000008
#include "t.h"
#include <appmodel.h>
#include "am_compat.h"

static const WCHAR *FULL = L"Microsoft.WindowsCalculator_11.2.0.0_x64__8wekyb3d8bbwe";
static const WCHAR *FAM  = L"Microsoft.WindowsCalculator_8wekyb3d8bbwe";
static const WCHAR *AUMID = L"Microsoft.WindowsCalculator_8wekyb3d8bbwe!App";
#define OK ERROR_SUCCESS
#define INV ERROR_INVALID_PARAMETER
#define INS ERROR_INSUFFICIENT_BUFFER

/* ---------- PackageFamilyNameFromFullName ---------- */
TEST(ams_family_from_full) {
    REQUIRE(U(PackageFamilyNameFromFullName));
    WCHAR b[PACKAGE_FAMILY_NAME_MAX_LENGTH + 1];
    UINT32 len = ARRAYSIZE(b);
    EQ(U(PackageFamilyNameFromFullName)(FULL, &len, b), OK);
    EXPECT_WSTR(b, FAM);
    EXPECT_EQ(len, lstrlenW(FAM) + 1);
}
TEST(ams_family_from_full_query_len) {
    REQUIRE(U(PackageFamilyNameFromFullName));
    UINT32 len = 0;
    LONG r = U(PackageFamilyNameFromFullName)(FULL, &len, NULL);
    EXPECT_EQ(r, INS);
    EXPECT_EQ(len, lstrlenW(FAM) + 1);
}
TEST(ams_family_from_full_exact_and_short) {
    REQUIRE(U(PackageFamilyNameFromFullName));
    WCHAR b[64];
    UINT32 need = lstrlenW(FAM) + 1;
    UINT32 len = need;
    memset(b, 0, sizeof b);
    EQ(U(PackageFamilyNameFromFullName)(FULL, &len, b), OK);
    EXPECT_WSTR(b, FAM);
    len = need - 1;
    EQ(U(PackageFamilyNameFromFullName)(FULL, &len, b), INS);
    EXPECT_EQ(len, need);
    len = 1;
    EQ(U(PackageFamilyNameFromFullName)(FULL, &len, b), INS);
    EXPECT_EQ(len, need);
}
TEST(ams_family_from_full_null) {
    REQUIRE(U(PackageFamilyNameFromFullName));
    WCHAR b[8]; UINT32 len = 8;
    EQ(U(PackageFamilyNameFromFullName)(NULL, &len, b), INV);
    EQ(U(PackageFamilyNameFromFullName)(FULL, NULL, b), INV);
    len = 8;
    EQ(U(PackageFamilyNameFromFullName)(FULL, &len, NULL), INV);
}
TEST(ams_family_from_full_invalid_names) {
    REQUIRE(U(PackageFamilyNameFromFullName));
    static const WCHAR *bad[] = {
        L"", L"x", L"Name", L"Name_1.0.0.0", L"Name_1.0.0.0_x64_", L"Name_1.0.0.0_x64__",
        L"Name_1.0.0.0_x64__8wekyb3d8bbw", L"Name_1.0.0.0_x64__8wekyb3d8bbwee",
        L"Name_1.0.0.0_x64__8wekyb3d8bbw!", L"Name_1.0.0.0_x64__8wekyb3d8bbwe_extra",
        L"Na_1.0.0.0_x64__8wekyb3d8bbwe", L"Name With Space_1.0.0.0_x64__8wekyb3d8bbwe",
        L"Name_1.0.0_x64__8wekyb3d8bbwe", L"Name_a.b.c.d_x64__8wekyb3d8bbwe",
        L"Name_1.0.0.0_x64__iiiiiiiiiiiii", L"Name_1.0.0.0_x64__8WEKYB3D8BBWE_",
        L"_1.0.0.0_x64__8wekyb3d8bbwe", L"Name_1.0.0.0_notanarch__8wekyb3d8bbwe",
        L"Name_65536.0.0.0_x64__8wekyb3d8bbwe", L"Name_-1.0.0.0_x64__8wekyb3d8bbwe",
    };
    for (size_t i = 0; i < ARRAYSIZE(bad); i++) {
        WCHAR b[PACKAGE_FAMILY_NAME_MAX_LENGTH + 1];
        UINT32 len = ARRAYSIZE(b);
        LONG r = U(PackageFamilyNameFromFullName)(bad[i], &len, b);
        CHECKF(r != OK, "case %zu", i);
        if (r != INV) t_note("PackageFamilyNameFromFullName case %zu -> %ld", i, (long)r);
    }
}
TEST(ams_family_from_full_variants) {
    REQUIRE(U(PackageFamilyNameFromFullName));
    struct { const WCHAR *full, *fam; } t[] = {
        { L"Contoso.App_1.0.0.0_neutral__8wekyb3d8bbwe", L"Contoso.App_8wekyb3d8bbwe" },
        { L"Contoso.App_1.0.0.0_x86__8wekyb3d8bbwe", L"Contoso.App_8wekyb3d8bbwe" },
        { L"Contoso.App_1.0.0.0_arm__8wekyb3d8bbwe", L"Contoso.App_8wekyb3d8bbwe" },
        { L"Contoso.App_1.0.0.0_arm64__8wekyb3d8bbwe", L"Contoso.App_8wekyb3d8bbwe" },
        { L"Contoso.App_65535.65535.65535.65535_x64__8wekyb3d8bbwe", L"Contoso.App_8wekyb3d8bbwe" },
        { L"Contoso.App_0.0.0.0_x64__8wekyb3d8bbwe", L"Contoso.App_8wekyb3d8bbwe" },
        { L"abc_1.0.0.0_x64__8wekyb3d8bbwe", L"abc_8wekyb3d8bbwe" },
        { L"A-b.9_1.0.0.0_x64__8wekyb3d8bbwe", L"A-b.9_8wekyb3d8bbwe" },
        { L"Contoso.App_1.0.0.0_x64_en-us_8wekyb3d8bbwe", L"Contoso.App_8wekyb3d8bbwe" },
        { L"Contoso.App_1.0.0.0_x64_~_8wekyb3d8bbwe", L"Contoso.App_8wekyb3d8bbwe" },
        { L"Contoso.App_1.0.0.0_neutral_split.scale-100_8wekyb3d8bbwe", L"Contoso.App_8wekyb3d8bbwe" },
        { L"Contoso.App_1.0.0.0_x64__0000000000000", L"Contoso.App_0000000000000" },
        { L"Contoso.App_1.0.0.0_x64__zzzzzzzzzzzzz", L"Contoso.App_zzzzzzzzzzzzz" },
    };
    for (size_t i = 0; i < ARRAYSIZE(t); i++) {
        WCHAR b[PACKAGE_FAMILY_NAME_MAX_LENGTH + 1] = {0};
        UINT32 len = ARRAYSIZE(b);
        LONG r = U(PackageFamilyNameFromFullName)(t[i].full, &len, b);
        if (r != OK) { t_note("variant %zu rejected (%ld)", i, (long)r); continue; }
        CHECKF(!lstrcmpW(b, t[i].fam), "variant %zu", i);
    }
}
TEST(ams_family_from_full_name_len_limits) {
    REQUIRE(U(PackageFamilyNameFromFullName));
    for (int n = 2; n <= 52; n++) {
        WCHAR full[200], fam[100];
        int k = 0;
        for (int i = 0; i < n; i++) full[k++] = L'a';
        wcscpy(full + k, L"_1.0.0.0_x64__8wekyb3d8bbwe");
        WCHAR b[PACKAGE_FAMILY_NAME_MAX_LENGTH + 20]; UINT32 len = ARRAYSIZE(b);
        LONG r = U(PackageFamilyNameFromFullName)(full, &len, b);
        if (n >= 3 && n <= 50) CHECKF(r == OK, "n=%d r=%ld", n, (long)r);
        else CHECKF(r != OK, "n=%d accepted", n);
        if (r == OK) {
            for (int i = 0; i < n; i++) fam[i] = L'a';
            wcscpy(fam + n, L"_8wekyb3d8bbwe");
            CHECK(!lstrcmpW(b, fam));
            CHECK(len <= PACKAGE_FAMILY_NAME_MAX_LENGTH + 1);
        }
    }
}
TEST(ams_family_from_full_reserved_names) {
    REQUIRE(U(PackageFamilyNameFromFullName));
    static const WCHAR *n[] = { L"CON", L"PRN", L"AUX", L"NUL", L"COM1", L"LPT1", L"con", L"nul" };
    for (size_t i = 0; i < ARRAYSIZE(n); i++) {
        WCHAR full[100], b[100]; UINT32 len = ARRAYSIZE(b);
        wsprintfW(full, L"%s_1.0.0.0_x64__8wekyb3d8bbwe", n[i]);
        LONG r = U(PackageFamilyNameFromFullName)(full, &len, b);
        CHECKF(r != OK, "reserved name %zu accepted", i);
    }
}

/* ---------- PackageIdFromFullName / PackageFullNameFromId / PackageFamilyNameFromId ---------- */
TEST(ams_id_from_full_basic) {
    REQUIRE(U(PackageIdFromFullName));
    UINT32 len = 0;
    LONG r = U(PackageIdFromFullName)(FULL, PACKAGE_INFORMATION_BASIC, &len, NULL);
    EXPECT_EQ(r, INS);
    CHECK(len >= sizeof(PACKAGE_ID));
    BYTE *buf = (BYTE *)HeapAlloc(GetProcessHeap(), 0, len);
    CHECK(buf != NULL);
    if (!buf) return;
    UINT32 l2 = len;
    r = U(PackageIdFromFullName)(FULL, PACKAGE_INFORMATION_BASIC, &l2, buf);
    EXPECT_EQ(r, OK);
    PACKAGE_ID *id = (PACKAGE_ID *)buf;
    EXPECT_EQ(id->processorArchitecture, PROCESSOR_ARCHITECTURE_AMD64);
    EXPECT_EQ(id->version.Major, 11);
    EXPECT_EQ(id->version.Minor, 2);
    EXPECT_EQ(id->version.Build, 0);
    EXPECT_EQ(id->version.Revision, 0);
    CHECK(id->name && !lstrcmpW(id->name, L"Microsoft.WindowsCalculator"));
    CHECK(id->publisherId && !lstrcmpW(id->publisherId, L"8wekyb3d8bbwe"));
    CHECK(id->resourceId == NULL || id->resourceId[0] == 0);
    CHECK(id->publisher == NULL);
    CHECK((BYTE *)id->name >= buf && (BYTE *)id->name < buf + len);
    /* too small: exactly one byte short */
    r = U(PackageIdFromFullName)(FULL, PACKAGE_INFORMATION_BASIC, &(UINT32){len - 1}, buf);
    EXPECT_EQ(r, INS);
    /* round trip */
    WCHAR full[PACKAGE_FULL_NAME_MAX_LENGTH + 1]; UINT32 fl = ARRAYSIZE(full);
    if (U(PackageFullNameFromId)) {
        r = U(PackageFullNameFromId)(id, &fl, full);
        EXPECT_EQ(r, OK);
        EXPECT_WSTR(full, FULL);
        EXPECT_EQ(fl, lstrlenW(FULL) + 1);
    }
    HeapFree(GetProcessHeap(), 0, buf);
}
TEST(ams_id_from_full_len_update) {
    REQUIRE(U(PackageIdFromFullName));
    UINT32 len = 1;
    BYTE b[1];
    LONG r = U(PackageIdFromFullName)(FULL, PACKAGE_INFORMATION_BASIC, &len, b);
    EXPECT_EQ(r, INS);
    CHECK(len > 1);
}
TEST(ams_id_from_full_invalid) {
    REQUIRE(U(PackageIdFromFullName));
    BYTE b[512]; UINT32 len = sizeof b;
    EQ(U(PackageIdFromFullName)(NULL, 0, &len, b), INV);
    EQ(U(PackageIdFromFullName)(FULL, 0, NULL, b), INV);
    len = sizeof b;
    EQ(U(PackageIdFromFullName)(FULL, 0, &len, NULL), INV);
    len = sizeof b;
    LONG r = U(PackageIdFromFullName)(L"not a package name", 0, &len, b);
    EXPECT_EQ(r, INV);
    len = sizeof b;
    r = U(PackageIdFromFullName)(L"", 0, &len, b);
    EXPECT_EQ(r, INV);
}
TEST(ams_id_from_full_neutral_arch) {
    REQUIRE(U(PackageIdFromFullName));
    struct { const WCHAR *s; UINT32 arch; } t[] = {
        { L"Contoso.App_1.0.0.0_x86__8wekyb3d8bbwe", PROCESSOR_ARCHITECTURE_INTEL },
        { L"Contoso.App_1.0.0.0_arm__8wekyb3d8bbwe", PROCESSOR_ARCHITECTURE_ARM },
        { L"Contoso.App_1.0.0.0_x64__8wekyb3d8bbwe", PROCESSOR_ARCHITECTURE_AMD64 },
        { L"Contoso.App_1.0.0.0_neutral__8wekyb3d8bbwe", 11 },
        { L"Contoso.App_1.0.0.0_arm64__8wekyb3d8bbwe", 12 },
    };
    for (size_t i = 0; i < ARRAYSIZE(t); i++) {
        BYTE b[512]; UINT32 len = sizeof b;
        LONG r = U(PackageIdFromFullName)(t[i].s, 0, &len, b);
        if (r != OK) { t_note("arch case %zu -> %ld", i, (long)r); continue; }
        EXPECT_EQ(((PACKAGE_ID *)b)->processorArchitecture, t[i].arch);
    }
}
TEST(ams_id_from_full_resource) {
    REQUIRE(U(PackageIdFromFullName));
    BYTE b[512]; UINT32 len = sizeof b;
    LONG r = U(PackageIdFromFullName)(L"Contoso.App_1.2.3.4_neutral_split.scale-100_8wekyb3d8bbwe", 0, &len, b);
    if (r != OK) { t_note("resource case -> %ld", (long)r); return; }
    PACKAGE_ID *id = (PACKAGE_ID *)b;
    CHECK(id->resourceId && !lstrcmpW(id->resourceId, L"split.scale-100"));
    EXPECT_EQ(id->version.Major, 1); EXPECT_EQ(id->version.Minor, 2);
    EXPECT_EQ(id->version.Build, 3); EXPECT_EQ(id->version.Revision, 4);
    EXPECT_EQ(id->version.Version, 0x0001000200030004ULL);
}
TEST(ams_id_from_full_flags_full) {
    REQUIRE(U(PackageIdFromFullName));
    BYTE b[1024]; UINT32 len = sizeof b;
    LONG r = U(PackageIdFromFullName)(FULL, PACKAGE_INFORMATION_FULL, &len, b);
    t_note("PackageIdFromFullName(FULL flags) -> %ld", (long)r);
}
TEST(ams_fullname_from_id_basic) {
    REQUIRE(U(PackageFullNameFromId));
    WCHAR name[] = L"Contoso.App", pub[] = L"CN=Contoso", rid[] = L"", pid[] = L"8wekyb3d8bbwe";
    PACKAGE_ID id; memset(&id, 0, sizeof id);
    id.processorArchitecture = PROCESSOR_ARCHITECTURE_AMD64;
    id.version.Major = 1; id.version.Minor = 2; id.version.Build = 3; id.version.Revision = 4;
    id.name = name; id.resourceId = rid; id.publisherId = pid; id.publisher = NULL;
    WCHAR out[PACKAGE_FULL_NAME_MAX_LENGTH + 1];
    UINT32 len = ARRAYSIZE(out);
    LONG r = U(PackageFullNameFromId)(&id, &len, out);
    EXPECT_EQ(r, OK);
    if (r == OK) EXPECT_WSTR(out, L"Contoso.App_1.2.3.4_x64__8wekyb3d8bbwe");
    len = 0;
    EQ(U(PackageFullNameFromId)(&id, &len, NULL), INS);
    EXPECT_EQ(len, lstrlenW(L"Contoso.App_1.2.3.4_x64__8wekyb3d8bbwe") + 1);
    len = 5;
    EQ(U(PackageFullNameFromId)(&id, &len, out), INS);
    (void)pub;
}
TEST(ams_fullname_from_id_invalid) {
    REQUIRE(U(PackageFullNameFromId));
    WCHAR out[64]; UINT32 len = 64;
    PACKAGE_ID id; memset(&id, 0, sizeof id);
    EQ(U(PackageFullNameFromId)(NULL, &len, out), INV);
    EQ(U(PackageFullNameFromId)(&id, NULL, out), INV);
    len = 64;
    EQ(U(PackageFullNameFromId)(&id, &len, NULL), INV);
    len = 64;
    LONG r = U(PackageFullNameFromId)(&id, &len, out);   /* NULL name */
    EXPECT_EQ(r, INV);
}
TEST(ams_family_from_id) {
    REQUIRE(U(PackageFamilyNameFromId));
    WCHAR name[] = L"Contoso.App", rid[] = L"", pid[] = L"8wekyb3d8bbwe";
    PACKAGE_ID id; memset(&id, 0, sizeof id);
    id.processorArchitecture = PROCESSOR_ARCHITECTURE_AMD64;
    id.version.Major = 1;
    id.name = name; id.resourceId = rid; id.publisherId = pid;
    WCHAR out[PACKAGE_FAMILY_NAME_MAX_LENGTH + 1]; UINT32 len = ARRAYSIZE(out);
    LONG r = U(PackageFamilyNameFromId)(&id, &len, out);
    EXPECT_EQ(r, OK);
    if (r == OK) { EXPECT_WSTR(out, L"Contoso.App_8wekyb3d8bbwe"); EXPECT_EQ(len, 26); }
    len = 0;
    EQ(U(PackageFamilyNameFromId)(&id, &len, NULL), INS);
    EXPECT_EQ(len, 26);
    EQ(U(PackageFamilyNameFromId)(NULL, &len, out), INV);
    len = 26;
    EQ(U(PackageFamilyNameFromId)(&id, NULL, out), INV);
    EQ(U(PackageFamilyNameFromId)(&id, &len, NULL), INV);
}
TEST(ams_family_from_id_hashes_publisher) {
    REQUIRE(U(PackageFamilyNameFromId));
    /* The family name's publisher id is the hash of the publisher subject when only publisher is given. */
    WCHAR name[] = L"Microsoft.WindowsCalculator", rid[] = L"";
    WCHAR pub[] = L"CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US";
    PACKAGE_ID id; memset(&id, 0, sizeof id);
    id.processorArchitecture = PROCESSOR_ARCHITECTURE_AMD64; id.version.Major = 1;
    id.name = name; id.resourceId = rid; id.publisher = pub;
    WCHAR out[PACKAGE_FAMILY_NAME_MAX_LENGTH + 1]; UINT32 len = ARRAYSIZE(out);
    LONG r = U(PackageFamilyNameFromId)(&id, &len, out);
    if (r != OK) { t_note("PackageFamilyNameFromId with publisher -> %ld", (long)r); return; }
    CHECK(lstrlenW(out) == lstrlenW(L"Microsoft.WindowsCalculator_") + PACKAGE_PUBLISHERID_MAX_LENGTH);
    /* deterministic */
    WCHAR out2[PACKAGE_FAMILY_NAME_MAX_LENGTH + 1]; len = ARRAYSIZE(out2);
    EQ(U(PackageFamilyNameFromId)(&id, &len, out2), OK);
    EXPECT_WSTR(out, out2);
    /* a different publisher yields a different hash */
    WCHAR pub2[] = L"CN=K32T Test";
    id.publisher = pub2; len = ARRAYSIZE(out2);
    r = U(PackageFamilyNameFromId)(&id, &len, out2);
    if (r != OK) { t_note("PackageFamilyNameFromId(CN=K32T Test) -> %ld", (long)r); return; }
    CHECK(lstrcmpW(out, out2) != 0);
}

/* ---------- PackageNameAndPublisherIdFromFamilyName ---------- */
TEST(ams_namepub_basic) {
    REQUIRE(U(PackageNameAndPublisherIdFromFamilyName));
    WCHAR n[PACKAGE_NAME_MAX_LENGTH + 1], p[PACKAGE_PUBLISHERID_MAX_LENGTH + 1];
    UINT32 nl = ARRAYSIZE(n), pl = ARRAYSIZE(p);
    LONG r = U(PackageNameAndPublisherIdFromFamilyName)(FAM, &nl, n, &pl, p);
    EXPECT_EQ(r, OK);
    EXPECT_WSTR(n, L"Microsoft.WindowsCalculator");
    EXPECT_WSTR(p, L"8wekyb3d8bbwe");
    EXPECT_EQ(nl, lstrlenW(L"Microsoft.WindowsCalculator") + 1);
    EXPECT_EQ(pl, PACKAGE_PUBLISHERID_MAX_LENGTH + 1);
}
TEST(ams_namepub_query_lengths) {
    REQUIRE(U(PackageNameAndPublisherIdFromFamilyName));
    UINT32 nl = 0, pl = 0;
    LONG r = U(PackageNameAndPublisherIdFromFamilyName)(FAM, &nl, NULL, &pl, NULL);
    EXPECT_EQ(r, INS);
    EXPECT_EQ(nl, lstrlenW(L"Microsoft.WindowsCalculator") + 1);
    EXPECT_EQ(pl, 14);
}
TEST(ams_namepub_partial_buffers) {
    REQUIRE(U(PackageNameAndPublisherIdFromFamilyName));
    WCHAR n[64], p[64];
    UINT32 nl = 64, pl = 13;   /* publisher buffer one short */
    LONG r = U(PackageNameAndPublisherIdFromFamilyName)(FAM, &nl, n, &pl, p);
    EXPECT_EQ(r, INS);
    EXPECT_EQ(pl, 14);
    nl = 5; pl = 64;
    r = U(PackageNameAndPublisherIdFromFamilyName)(FAM, &nl, n, &pl, p);
    EXPECT_EQ(r, INS);
    EXPECT_EQ(nl, lstrlenW(L"Microsoft.WindowsCalculator") + 1);
    nl = lstrlenW(L"Microsoft.WindowsCalculator"); pl = 14;   /* name one short */
    EQ(U(PackageNameAndPublisherIdFromFamilyName)(FAM, &nl, n, &pl, p), INS);
    nl = lstrlenW(L"Microsoft.WindowsCalculator") + 1; pl = 14;   /* exact */
    EQ(U(PackageNameAndPublisherIdFromFamilyName)(FAM, &nl, n, &pl, p), OK);
}
TEST(ams_namepub_null) {
    REQUIRE(U(PackageNameAndPublisherIdFromFamilyName));
    WCHAR n[64], p[64]; UINT32 nl = 64, pl = 64;
    EQ(U(PackageNameAndPublisherIdFromFamilyName)(NULL, &nl, n, &pl, p), INV);
    EQ(U(PackageNameAndPublisherIdFromFamilyName)(FAM, NULL, n, &pl, p), INV);
    EQ(U(PackageNameAndPublisherIdFromFamilyName)(FAM, &nl, NULL, &pl, p), INV);
    EQ(U(PackageNameAndPublisherIdFromFamilyName)(FAM, &nl, n, NULL, p), INV);
    EQ(U(PackageNameAndPublisherIdFromFamilyName)(FAM, &nl, n, &pl, NULL), INV);
}
TEST(ams_namepub_invalid) {
    REQUIRE(U(PackageNameAndPublisherIdFromFamilyName));
    static const WCHAR *bad[] = {
        L"", L"_", L"NoUnderscore", L"Name_", L"Name_8wekyb3d8bbw", L"Name_8wekyb3d8bbwee",
        L"Name_8wekyb3d8bbw!", L"Na_8wekyb3d8bbwe", L"_8wekyb3d8bbwe", L"Name_iiiiiiiiiiiii",
        L"Name_llllllllllll1", L"Name_oooooooooooo1", L"Name_uuuuuuuuuuuuu", L"Name__8wekyb3d8bbwe",
        L"Name With Space_8wekyb3d8bbwe", L"Name_1.0.0.0_x64__8wekyb3d8bbwe", L"Name_8wekyb3d8bbwe_",
        L"Na/me_8wekyb3d8bbwe", L"Na\\me_8wekyb3d8bbwe", L"Name!_8wekyb3d8bbwe", L"Name:x_8wekyb3d8bbwe",
        L"CON_8wekyb3d8bbwe", L"NUL_8wekyb3d8bbwe", L"Name_8wekyb3d8bbwe!App",
    };
    for (size_t i = 0; i < ARRAYSIZE(bad); i++) {
        WCHAR n[64], p[64]; UINT32 nl = 64, pl = 64;
        LONG r = U(PackageNameAndPublisherIdFromFamilyName)(bad[i], &nl, n, &pl, p);
        CHECKF(r == INV, "case %zu -> %ld", i, (long)r);
    }
}
TEST(ams_namepub_name_length_limits) {
    REQUIRE(U(PackageNameAndPublisherIdFromFamilyName));
    for (int k = 1; k <= 52; k++) {
        WCHAR fam[100]; int c = 0;
        for (int i = 0; i < k; i++) fam[c++] = L'b';
        wcscpy(fam + c, L"_8wekyb3d8bbwe");
        WCHAR n[100], p[20]; UINT32 nl = 100, pl = 20;
        LONG r = U(PackageNameAndPublisherIdFromFamilyName)(fam, &nl, n, &pl, p);
        if (k >= 3 && k <= 50) { CHECKF(r == OK, "k=%d r=%ld", k, (long)r); if (r == OK) CHECK(nl == (UINT32)k + 1); }
        else CHECKF(r == INV, "k=%d r=%ld", k, (long)r);
    }
}
TEST(ams_namepub_name_charset) {
    REQUIRE(U(PackageNameAndPublisherIdFromFamilyName));
    const WCHAR ok_chars[] = L"abcxyzABCXYZ019-.";
    for (size_t i = 0; i < wcslen(ok_chars); i++) {
        WCHAR fam[64];
        wsprintfW(fam, L"a%cb_8wekyb3d8bbwe", ok_chars[i]);
        WCHAR n[64], p[64]; UINT32 nl = 64, pl = 64;
        CHECKF(U(PackageNameAndPublisherIdFromFamilyName)(fam, &nl, n, &pl, p) == OK, "ok char %c", (char)ok_chars[i]);
    }
    const WCHAR bad_chars[] = L" !\"#$%&'()*+,/:;<=>?@[\\]^`{|}~";
    for (size_t i = 0; i < wcslen(bad_chars); i++) {
        WCHAR fam[64];
        wsprintfW(fam, L"a%cb_8wekyb3d8bbwe", bad_chars[i]);
        WCHAR n[64], p[64]; UINT32 nl = 64, pl = 64;
        CHECKF(U(PackageNameAndPublisherIdFromFamilyName)(fam, &nl, n, &pl, p) == INV, "bad char %c", (char)bad_chars[i]);
    }
}
TEST(ams_namepub_publisher_charset) {
    REQUIRE(U(PackageNameAndPublisherIdFromFamilyName));
    const char alpha[] = "1234567890abcdefghjkmnpqrstvwxyz";
    for (const char *c = alpha; *c; c++) {
        WCHAR fam[64];
        wsprintfW(fam, L"Name_aaaaaaaaaaaa%c", (WCHAR)*c);
        WCHAR n[64], p[64]; UINT32 nl = 64, pl = 64;
        CHECKF(U(PackageNameAndPublisherIdFromFamilyName)(fam, &nl, n, &pl, p) == OK, "pub char %c", *c);
    }
    const char excl[] = "ilou";
    for (const char *c = excl; *c; c++) {
        WCHAR fam[64];
        wsprintfW(fam, L"Name_aaaaaaaaaaaa%c", (WCHAR)*c);
        WCHAR n[64], p[64]; UINT32 nl = 64, pl = 64;
        CHECKF(U(PackageNameAndPublisherIdFromFamilyName)(fam, &nl, n, &pl, p) == INV, "excluded pub char %c", *c);
    }
}

/* ---------- ParseApplicationUserModelId ---------- */
TEST(ams_parse_aumid_basic) {
    REQUIRE(U(ParseApplicationUserModelId));
    WCHAR f[PACKAGE_FAMILY_NAME_MAX_LENGTH + 1], a[PACKAGE_RELATIVE_APPLICATION_ID_MAX_LENGTH + 1];
    UINT32 fl = ARRAYSIZE(f), al = ARRAYSIZE(a);
    LONG r = U(ParseApplicationUserModelId)(AUMID, &fl, f, &al, a);
    EXPECT_EQ(r, OK);
    EXPECT_WSTR(f, FAM);
    EXPECT_WSTR(a, L"App");
    EXPECT_EQ(fl, lstrlenW(FAM) + 1);
    EXPECT_EQ(al, 4);
}
TEST(ams_parse_aumid_query_lengths) {
    REQUIRE(U(ParseApplicationUserModelId));
    UINT32 fl = 0, al = 0;
    LONG r = U(ParseApplicationUserModelId)(AUMID, &fl, NULL, &al, NULL);
    EXPECT_EQ(r, INS);
    EXPECT_EQ(fl, lstrlenW(FAM) + 1);
    EXPECT_EQ(al, 4);
}
TEST(ams_parse_aumid_short_buffers) {
    REQUIRE(U(ParseApplicationUserModelId));
    WCHAR f[64], a[64];
    UINT32 fl = lstrlenW(FAM), al = 64;
    EQ(U(ParseApplicationUserModelId)(AUMID, &fl, f, &al, a), INS);
    EXPECT_EQ(fl, lstrlenW(FAM) + 1);
    fl = 64; al = 3;
    EQ(U(ParseApplicationUserModelId)(AUMID, &fl, f, &al, a), INS);
    EXPECT_EQ(al, 4);
    fl = lstrlenW(FAM) + 1; al = 4;
    EQ(U(ParseApplicationUserModelId)(AUMID, &fl, f, &al, a), OK);
}
TEST(ams_parse_aumid_null) {
    REQUIRE(U(ParseApplicationUserModelId));
    WCHAR f[64], a[64]; UINT32 fl = 64, al = 64;
    EQ(U(ParseApplicationUserModelId)(NULL, &fl, f, &al, a), INV);
    EQ(U(ParseApplicationUserModelId)(AUMID, NULL, f, &al, a), INV);
    EQ(U(ParseApplicationUserModelId)(AUMID, &fl, NULL, &al, a), INV);
    EQ(U(ParseApplicationUserModelId)(AUMID, &fl, f, NULL, a), INV);
    EQ(U(ParseApplicationUserModelId)(AUMID, &fl, f, &al, NULL), INV);
}
TEST(ams_parse_aumid_invalid) {
    REQUIRE(U(ParseApplicationUserModelId));
    static const WCHAR *bad[] = {
        L"", L"!", L"App", L"Microsoft.WindowsCalculator_8wekyb3d8bbwe", L"Microsoft.WindowsCalculator_8wekyb3d8bbwe!",
        L"Microsoft.WindowsCalculator_8wekyb3d8bbw!App", L"Microsoft.WindowsCalculator_8wekyb3d8bbwee!App",
        L"Microsoft.WindowsCalculator!App", L"_8wekyb3d8bbwe!App", L"Na_8wekyb3d8bbwe!App",
        L"Microsoft.WindowsCalculator_8wekyb3d8bbwe!App Name", L"Microsoft.WindowsCalculator_8wekyb3d8bbwe!App!X",
        L"Microsoft.WindowsCalculator_8wekyb3d8bbwe!App-1", L"Microsoft.WindowsCalculator_8wekyb3d8bbwe!App_1",
        L"Microsoft.WindowsCalculator_iiiiiiiiiiiii!App", L"Microsoft.WindowsCalculator_8wekyb3d8bbwe !App",
    };
    for (size_t i = 0; i < ARRAYSIZE(bad); i++) {
        WCHAR f[100], a[100]; UINT32 fl = 100, al = 100;
        LONG r = U(ParseApplicationUserModelId)(bad[i], &fl, f, &al, a);
        CHECKF(r == INV, "case %zu -> %ld", i, (long)r);
    }
}
TEST(ams_parse_aumid_appid_chars_and_len) {
    REQUIRE(U(ParseApplicationUserModelId));
    /* App id: alnum and '.', 1..64 chars */
    for (int k = 1; k <= 66; k++) {
        WCHAR au[200];
        int c = wsprintfW(au, L"%s!", FAM);
        for (int i = 0; i < k; i++) au[c++] = (i % 2) ? L'x' : L'A';
        au[c] = 0;
        WCHAR f[100], a[100]; UINT32 fl = 100, al = 100;
        LONG r = U(ParseApplicationUserModelId)(au, &fl, f, &al, a);
        if (k <= 64) { CHECKF(r == OK, "k=%d r=%ld", k, (long)r); if (r == OK) CHECK(al == (UINT32)k + 1); }
        else CHECKF(r == INV, "k=%d r=%ld", k, (long)r);
    }
    WCHAR f[100], a[100]; UINT32 fl = 100, al = 100;
    EQ(U(ParseApplicationUserModelId)(L"Microsoft.WindowsCalculator_8wekyb3d8bbwe!A.b.9", &fl, f, &al, a), OK);
    EXPECT_WSTR(a, L"A.b.9");
}
TEST(ams_parse_aumid_max_len) {
    REQUIRE(U(ParseApplicationUserModelId));
    /* longest possible: 50 + 1 + 13 + 1 + 64 = 129 chars (+NUL = 130) */
    WCHAR au[200]; int c = 0;
    for (int i = 0; i < 50; i++) au[c++] = L'n';
    c += wsprintfW(au + c, L"_8wekyb3d8bbwe!");
    for (int i = 0; i < 64; i++) au[c++] = L'z';
    au[c] = 0;
    CHECK(c + 1 == APPLICATION_USER_MODEL_ID_MAX_LENGTH);
    WCHAR f[100], a[100]; UINT32 fl = 100, al = 100;
    EQ(U(ParseApplicationUserModelId)(au, &fl, f, &al, a), OK);
    EXPECT_EQ(fl, PACKAGE_FAMILY_NAME_MAX_LENGTH + 1);
    EXPECT_EQ(al, PACKAGE_RELATIVE_APPLICATION_ID_MAX_LENGTH + 1);
}

/* ---------- FormatApplicationUserModelId ---------- */
TEST(ams_format_aumid_basic) {
    REQUIRE(U(FormatApplicationUserModelId));
    WCHAR o[APPLICATION_USER_MODEL_ID_MAX_LENGTH + 1]; UINT32 len = ARRAYSIZE(o);
    LONG r = U(FormatApplicationUserModelId)(FAM, L"App", &len, o);
    EXPECT_EQ(r, OK);
    EXPECT_WSTR(o, AUMID);
    EXPECT_EQ(len, lstrlenW(AUMID) + 1);
}
TEST(ams_format_aumid_query_len) {
    REQUIRE(U(FormatApplicationUserModelId));
    UINT32 len = 0;
    EQ(U(FormatApplicationUserModelId)(FAM, L"App", &len, NULL), INS);
    EXPECT_EQ(len, lstrlenW(AUMID) + 1);
    WCHAR o[100];
    len = lstrlenW(AUMID);
    EQ(U(FormatApplicationUserModelId)(FAM, L"App", &len, o), INS);
    EXPECT_EQ(len, lstrlenW(AUMID) + 1);
    len = lstrlenW(AUMID) + 1;
    EQ(U(FormatApplicationUserModelId)(FAM, L"App", &len, o), OK);
}
TEST(ams_format_aumid_null) {
    REQUIRE(U(FormatApplicationUserModelId));
    WCHAR o[100]; UINT32 len = 100;
    EQ(U(FormatApplicationUserModelId)(NULL, L"App", &len, o), INV);
    EQ(U(FormatApplicationUserModelId)(FAM, NULL, &len, o), INV);
    EQ(U(FormatApplicationUserModelId)(FAM, L"App", NULL, o), INV);
    EQ(U(FormatApplicationUserModelId)(FAM, L"App", &len, NULL), INV);
}
TEST(ams_format_aumid_invalid) {
    REQUIRE(U(FormatApplicationUserModelId));
    WCHAR o[200]; UINT32 len;
    const WCHAR *badfam[] = { L"", L"Name", L"Name_", L"Name_8wekyb3d8bbw", L"Name_8wekyb3d8bbwe!App" };
    for (size_t i = 0; i < ARRAYSIZE(badfam); i++) {
        len = 200;
        LONG r = U(FormatApplicationUserModelId)(badfam[i], L"App", &len, o);
        CHECKF(r == INV, "badfam %zu -> %ld", i, (long)r);
    }
    const WCHAR *badapp[] = { L"A B", L"A!B", L"A_B" };
    for (size_t i = 0; i < ARRAYSIZE(badapp); i++) {
        len = 200;
        LONG r = U(FormatApplicationUserModelId)(FAM, badapp[i], &len, o);
        CHECKF(r == INV, "badapp %zu -> %ld", i, (long)r);
    }
    len = 200;
    { LONG r = U(FormatApplicationUserModelId)(L"Name_iiiiiiiiiiiii", L"App", &len, o); t_note("Format(bad pubid chars) -> %ld", (long)r); }
    len = 200;
    { LONG r = U(FormatApplicationUserModelId)(FAM, L"", &len, o); t_note("Format(empty app id) -> %ld", (long)r); }
    len = 200;
    { LONG r = U(FormatApplicationUserModelId)(FAM, L"A-B", &len, o); t_note("Format(A-B) -> %ld", (long)r); }
    WCHAR big[100];
    for (int i = 0; i < 65; i++) big[i] = L'a';
    big[65] = 0;
    len = 200;
    EQ(U(FormatApplicationUserModelId)(FAM, big, &len, o), INV);
    big[64] = 0;
    len = 200;
    EQ(U(FormatApplicationUserModelId)(FAM, big, &len, o), OK);
}
TEST(ams_format_parse_roundtrip) {
    REQUIRE(U(FormatApplicationUserModelId));
    REQUIRE(U(ParseApplicationUserModelId));
    const WCHAR *apps[] = { L"App", L"x", L"Calc.Main", L"A1.B2.C3", L"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" };
    for (size_t i = 0; i < ARRAYSIZE(apps); i++) {
        WCHAR o[200]; UINT32 len = 200;
        if (U(FormatApplicationUserModelId)(FAM, apps[i], &len, o) != OK) { CHECK(0); continue; }
        WCHAR f[100], a[100]; UINT32 fl = 100, al = 100;
        EQ(U(ParseApplicationUserModelId)(o, &fl, f, &al, a), OK);
        EXPECT_WSTR(f, FAM);
        EXPECT_WSTR(a, apps[i]);
    }
}
TEST(ams_format_aumid_overlap_in_place) {
    REQUIRE(U(FormatApplicationUserModelId));
    WCHAR o[200]; UINT32 len = 200;
    EQ(U(FormatApplicationUserModelId)(FAM, L"A", &len, o), OK);
    EXPECT_WSTR(o, L"Microsoft.WindowsCalculator_8wekyb3d8bbwe!A");
}

/* ---------- Verify* ---------- */
TEST(ams_verify_full_name) {
    REQUIRE(U(VerifyPackageFullName));
    EQ(U(VerifyPackageFullName)(FULL), OK);
    EQ(U(VerifyPackageFullName)(L"Contoso.App_1.0.0.0_neutral__8wekyb3d8bbwe"), OK);
    EQ(U(VerifyPackageFullName)(NULL), INV);
    EQ(U(VerifyPackageFullName)(L""), INV);
    EQ(U(VerifyPackageFullName)(FAM), INV);
    EQ(U(VerifyPackageFullName)(AUMID), INV);
    EQ(U(VerifyPackageFullName)(L"Microsoft.WindowsCalculator_11.2.0.0_x64__8wekyb3d8bbw"), INV);
    EQ(U(VerifyPackageFullName)(L"Microsoft.WindowsCalculator_11.2.0.0_x64_8wekyb3d8bbwe"), INV);
}
TEST(ams_verify_family_name) {
    REQUIRE(U(VerifyPackageFamilyName));
    EQ(U(VerifyPackageFamilyName)(FAM), OK);
    EQ(U(VerifyPackageFamilyName)(NULL), INV);
    EQ(U(VerifyPackageFamilyName)(L""), INV);
    EQ(U(VerifyPackageFamilyName)(FULL), INV);
    EQ(U(VerifyPackageFamilyName)(AUMID), INV);
    EQ(U(VerifyPackageFamilyName)(L"Microsoft.WindowsCalculator_8wekyb3d8bbw"), INV);
    EQ(U(VerifyPackageFamilyName)(L"Microsoft.WindowsCalculator-8wekyb3d8bbwe"), INV);
    EQ(U(VerifyPackageFamilyName)(L"abc_0000000000000"), OK);
}
TEST(ams_verify_aumid) {
    REQUIRE(U(VerifyApplicationUserModelId));
    EQ(U(VerifyApplicationUserModelId)(AUMID), OK);
    EQ(U(VerifyApplicationUserModelId)(NULL), INV);
    EQ(U(VerifyApplicationUserModelId)(L""), INV);
    EQ(U(VerifyApplicationUserModelId)(FAM), INV);
    EQ(U(VerifyApplicationUserModelId)(FULL), INV);
    EQ(U(VerifyApplicationUserModelId)(L"Microsoft.WindowsCalculator_8wekyb3d8bbwe!"), INV);
    EQ(U(VerifyApplicationUserModelId)(L"!App"), INV);
}
TEST(ams_verify_relative_app_id) {
    REQUIRE(U(VerifyPackageRelativeApplicationId));
    EQ(U(VerifyPackageRelativeApplicationId)(L"App"), OK);
    EQ(U(VerifyPackageRelativeApplicationId)(L"A.b.1"), OK);
    EQ(U(VerifyPackageRelativeApplicationId)(NULL), INV);
    t_note("VerifyRelative(empty) -> %ld", (long)U(VerifyPackageRelativeApplicationId)(L""));
    EQ(U(VerifyPackageRelativeApplicationId)(L"A B"), INV);
    EQ(U(VerifyPackageRelativeApplicationId)(L"A!B"), INV);
    EQ(U(VerifyPackageRelativeApplicationId)(L"A_B"), INV);
}
TEST(ams_verify_package_id) {
    REQUIRE(U(VerifyPackageId));
    EQ(U(VerifyPackageId)(NULL), INV);
    WCHAR name[] = L"Contoso.App", rid[] = L"", pid[] = L"8wekyb3d8bbwe";
    PACKAGE_ID id; memset(&id, 0, sizeof id);
    id.processorArchitecture = PROCESSOR_ARCHITECTURE_AMD64;
    id.version.Major = 1;
    id.name = name; id.resourceId = rid; id.publisherId = pid;
    EQ(U(VerifyPackageId)(&id), OK);
    id.name = NULL;
    EQ(U(VerifyPackageId)(&id), INV);
    WCHAR shortn[] = L"ab";
    id.name = shortn;
    t_note("VerifyPackageId(short name) -> %ld", (long)U(VerifyPackageId)(&id));
    id.name = name;
    WCHAR badp[] = L"iiiiiiiiiiiii";
    id.publisherId = badp;
    t_note("VerifyPackageId(bad pubid) -> %ld", (long)U(VerifyPackageId)(&id));
}
TEST(ams_verify_lastcalls_do_not_set_lasterror_semantics) {
    REQUIRE(U(VerifyPackageFullName));
    SET_ERR(0x4242);
    U(VerifyPackageFullName)(FULL);
    t_note("GetLastError after successful Verify: %lu", GetLastError());
}

/* ---------- constants and cross checks ---------- */
TEST(ams_constants_consistent) {
    CHECK(PACKAGE_FAMILY_NAME_MAX_LENGTH == PACKAGE_NAME_MAX_LENGTH + 1 + PACKAGE_PUBLISHERID_MAX_LENGTH);
    CHECK(APPLICATION_USER_MODEL_ID_MAX_LENGTH == PACKAGE_FAMILY_NAME_MAX_LENGTH + 1 + PACKAGE_RELATIVE_APPLICATION_ID_MAX_LENGTH + 1);
    CHECK(PACKAGE_FULL_NAME_MAX_LENGTH == 127);
}
TEST(ams_longest_full_name) {
    REQUIRE(U(PackageFamilyNameFromFullName));
    REQUIRE(U(VerifyPackageFullName));
    /* 50 name + _ + 23 version + _ + 5 arch + _ + 30 resid + _ + 13 pubid */
    WCHAR full[200]; int c = 0;
    for (int i = 0; i < 50; i++) full[c++] = L'n';
    c += wsprintfW(full + c, L"_65535.65535.65535.65535_neutral_");
    for (int i = 0; i < 30; i++) full[c++] = L'r';
    c += wsprintfW(full + c, L"_8wekyb3d8bbwe");
    LONG r = U(VerifyPackageFullName)(full);
    t_note("longest full name (%d chars) -> %ld", c, (long)r);
    CHECK(c < PACKAGE_FULL_NAME_MAX_LENGTH + 20);
}
TEST(ams_family_from_full_buffer_boundary_table) {
    REQUIRE(U(PackageFamilyNameFromFullName));
    UINT32 need = lstrlenW(FAM) + 1;
    for (UINT32 l = 0; l <= need + 2; l++) {
        WCHAR b[100]; memset(b, 0, sizeof b);
        UINT32 len = l;
        LONG r = U(PackageFamilyNameFromFullName)(FULL, &len, l ? b : NULL);
        if (l >= need) { CHECKF(r == OK, "l=%u r=%ld", l, (long)r); CHECKF(len == need, "l=%u len=%u", l, len); }
        else { CHECKF(r == INS, "l=%u r=%ld", l, (long)r); CHECKF(len == need, "l=%u len=%u", l, len); }
    }
}
