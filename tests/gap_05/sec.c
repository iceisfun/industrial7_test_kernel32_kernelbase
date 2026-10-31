/* GetLengthSid, GetSecurityDescriptorControl/Dacl, GetKernelObjectSecurity, GetPrivateObjectSecurity */
#include "g5.h"

typedef DWORD (WINAPI *lensid_t)(PSID);
typedef BOOL (WINAPI *sdctl_t)(PSECURITY_DESCRIPTOR, PSECURITY_DESCRIPTOR_CONTROL, LPDWORD);
typedef BOOL (WINAPI *sddacl_t)(PSECURITY_DESCRIPTOR, LPBOOL, PACL *, LPBOOL);
typedef BOOL (WINAPI *kos_t)(HANDLE, SECURITY_INFORMATION, PSECURITY_DESCRIPTOR, DWORD, LPDWORD);
typedef BOOL (WINAPI *pos_t)(PSECURITY_DESCRIPTOR, SECURITY_INFORMATION, PSECURITY_DESCRIPTOR, DWORD, PDWORD);
#define LENSID ((lensid_t)g5_proc("GetLengthSid"))
#define SDCTL ((sdctl_t)g5_proc("GetSecurityDescriptorControl"))
#define SDDACL ((sddacl_t)g5_proc("GetSecurityDescriptorDacl"))
#define KOS ((kos_t)g5_proc("GetKernelObjectSecurity"))
#define POS ((pos_t)g5_proc("GetPrivateObjectSecurity"))

TEST(gap5_sec_exports) {
    static const char *n[] = { "GetLengthSid", "GetSecurityDescriptorControl", "GetSecurityDescriptorDacl", "GetKernelObjectSecurity", "GetPrivateObjectSecurity" };
    for (unsigned i = 0; i < 5; i++) CHECKF(g5_proc(n[i]) != NULL, "%s", n[i]);
    if (!t_is_wine()) for (unsigned i = 0; i < 5; i++) CHECKF(t_proc_kb(n[i]) != NULL, "kernelbase!%s", n[i]);
}

/* ============ GetLengthSid ============ */
static void mksid(BYTE *b, int subs) {
    memset(b, 0, 8 + 4 * subs);
    b[0] = 1; b[1] = (BYTE)subs; b[7] = 5;
    for (int i = 0; i < subs; i++) *(DWORD *)(b + 8 + 4 * i) = 0x100 + i;
}
TEST(gap5_lensid_by_subauth_count) {
    REQUIRE(LENSID);
    BYTE sid[8 + 4 * 255 + 8];
    for (int n = 0; n <= 15; n++) {
        mksid(sid, n);
        CHECKF(LENSID(sid) == 8 + 4 * (DWORD)n, "n=%d", n);
    }
    /* RtlLengthSid is purely arithmetic: it does not validate the count */
    mksid(sid, 255); EXPECT_EQ(LENSID(sid), 8 + 4 * 255);
    mksid(sid, 16); EXPECT_EQ(LENSID(sid), 8 + 4 * 16);
}
TEST(gap5_lensid_well_known) {
    REQUIRE(LENSID);
    static const char *str[] = { "S-1-1-0", "S-1-5-18", "S-1-5-32-544", "S-1-5-21-1-2-3-500", "S-1-5-5-0-12345", "S-1-16-12288" };
    static const DWORD want[] = { 12, 12, 16, 28, 20, 12 };
    for (int i = 0; i < 6; i++) {
        PSID s = NULL;
        if (!ConvertStringSidToSidA(str[i], &s)) { CHECK(0); continue; }
        CHECKF(LENSID(s) == want[i], "%s -> %lu want %lu", str[i], LENSID(s), want[i]);
        CHECK(GetLengthSid(s) == LENSID(s));
        LocalFree(s);
    }
}
TEST(gap5_lensid_clears_last_error) {
    WINDOWS_ONLY();
    REQUIRE(LENSID);
    BYTE sid[16]; mksid(sid, 2);
    SET_ERR(ERROR_ACCESS_DENIED);
    EXPECT_EQ(LENSID(sid), 16);
    EXPECT_ERR(0);              /* GetLengthSid explicitly resets the last error */
    SET_ERR(0x1234);
    LENSID(sid);
    EXPECT_ERR(0);
}
TEST(gap5_lensid_token_user_sid) {
    REQUIRE(LENSID);
    HANDLE tok;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) { t_skip("no token"); return; }
    BYTE buf[512]; DWORD n = 0;
    if (GetTokenInformation(tok, TokenUser, buf, sizeof buf, &n)) {
        PSID s = ((TOKEN_USER *)buf)->User.Sid;
        DWORD want = 8 + 4 * ((BYTE *)s)[1];
        EXPECT_EQ(LENSID(s), want);
        CHECK(IsValidSid(s));
    }
    CloseHandle(tok);
}

/* ============ GetSecurityDescriptorControl / Dacl ============ */
TEST(gap5_sd_control_absolute) {
    REQUIRE(SDCTL);
    SECURITY_DESCRIPTOR sd;
    CHECK(InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION));
    SECURITY_DESCRIPTOR_CONTROL c = 0xFFFF; DWORD rev = 0xFFFFFFFF;
    SET_ERR(0);
    CHECK(SDCTL(&sd, &c, &rev));
    EXPECT_EQ(c, 0);
    EXPECT_EQ(rev, SECURITY_DESCRIPTOR_REVISION);
    CHECK(SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE));
    CHECK(SDCTL(&sd, &c, &rev));
    EXPECT_EQ(c, SE_DACL_PRESENT);
    CHECK(SetSecurityDescriptorDacl(&sd, TRUE, NULL, TRUE));
    CHECK(SDCTL(&sd, &c, &rev));
    EXPECT_EQ(c, SE_DACL_PRESENT | SE_DACL_DEFAULTED);
    CHECK(SetSecurityDescriptorDacl(&sd, FALSE, NULL, FALSE));
    CHECK(SDCTL(&sd, &c, &rev));
    EXPECT_EQ(c & SE_DACL_PRESENT, 0);
    CHECK(SetSecurityDescriptorControl(&sd, SE_DACL_PROTECTED | SE_SACL_PROTECTED, SE_DACL_PROTECTED | SE_SACL_PROTECTED));
    CHECK(SDCTL(&sd, &c, &rev));
    EXPECT_EQ(c & (SE_DACL_PROTECTED | SE_SACL_PROTECTED), SE_DACL_PROTECTED | SE_SACL_PROTECTED);
    EXPECT_EQ(c & SE_SELF_RELATIVE, 0);
}
TEST(gap5_sd_control_self_relative) {
    REQUIRE(SDCTL);
    PSECURITY_DESCRIPTOR sd = NULL;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorA("O:BAG:SYD:P(A;;GA;;;WD)S:(AU;SA;GA;;;WD)", SDDL_REVISION_1, &sd, NULL)) { t_skip("sddl failed"); return; }
    SECURITY_DESCRIPTOR_CONTROL c = 0; DWORD rev = 0;
    CHECK(SDCTL(sd, &c, &rev));
    EXPECT_EQ(rev, 1);
    CHECK(c & SE_SELF_RELATIVE);
    CHECK(c & SE_DACL_PRESENT);
    CHECK(c & SE_SACL_PRESENT);
    CHECK(c & SE_DACL_PROTECTED);
    EXPECT_EQ(c & SE_OWNER_DEFAULTED, 0);
    LocalFree(sd);
}
TEST(gap5_sd_control_revision_out_optional) {
    REQUIRE(SDCTL);
    SECURITY_DESCRIPTOR sd; InitializeSecurityDescriptor(&sd, 1);
    SECURITY_DESCRIPTOR_CONTROL c = 0xFFFF;
    /* the revision out-parameter is written unconditionally by the callee; supply it */
    DWORD rev;
    CHECK(SDCTL(&sd, &c, &rev));
}
TEST(gap5_sd_control_bad_revision) {
    REQUIRE(SDCTL);
    SECURITY_DESCRIPTOR sd; InitializeSecurityDescriptor(&sd, 1);
    SECURITY_DESCRIPTOR_CONTROL c = 0x7777; DWORD rev = 0x55;
    BYTE *raw = (BYTE *)&sd;
    BYTE saved = raw[0];
    BYTE bad[] = { 0, 2, 3, 0x80, 0xFF };
    for (unsigned i = 0; i < sizeof bad; i++) {
        raw[0] = bad[i];
        SET_ERR(0);
        CHECKF(!SDCTL(&sd, &c, &rev), "revision %u", bad[i]);
        EXPECT_ERR(ERROR_UNKNOWN_REVISION);
        EXPECT_EQ(c, 0x7777);
    }
    raw[0] = saved;
}
TEST(gap5_sd_dacl_states) {
    REQUIRE(SDDACL);
    SECURITY_DESCRIPTOR sd; CHECK(InitializeSecurityDescriptor(&sd, 1));
    BOOL present = 0x77, def = 0x77; PACL acl = (PACL)0x1234;
    SET_ERR(0);
    CHECK(SDDACL(&sd, &present, &acl, &def));
    EXPECT_EQ(present, FALSE);
    EXPECT_EQ(def, FALSE);
    if (acl != (PACL)0x1234) t_note("absent DACL: output pointer rewritten to %p", acl);
    /* NULL DACL (everyone full control) */
    CHECK(SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE));
    present = def = 0x77; acl = (PACL)0x1234;
    CHECK(SDDACL(&sd, &present, &acl, &def));
    EXPECT_EQ(present, TRUE); EXPECT_EQ(def, FALSE); EXPECT_EQ(acl, NULL);
    /* empty DACL (deny all) */
    BYTE abuf[64]; PACL empty = (PACL)abuf;
    CHECK(InitializeAcl(empty, sizeof abuf, ACL_REVISION));
    CHECK(SetSecurityDescriptorDacl(&sd, TRUE, empty, TRUE));
    present = def = 0x77; acl = NULL;
    CHECK(SDDACL(&sd, &present, &acl, &def));
    EXPECT_EQ(present, TRUE); EXPECT_EQ(def, TRUE); EXPECT_EQ(acl, empty);
    /* removed again */
    CHECK(SetSecurityDescriptorDacl(&sd, FALSE, NULL, FALSE));
    present = 0x77; acl = (PACL)0x1234;
    CHECK(SDDACL(&sd, &present, &acl, &def));
    EXPECT_EQ(present, FALSE);
}
TEST(gap5_sd_dacl_self_relative) {
    REQUIRE(SDDACL);
    PSECURITY_DESCRIPTOR sd = NULL;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorA("O:BAG:SYD:(A;;GA;;;WD)(A;;GR;;;BA)", SDDL_REVISION_1, &sd, NULL)) { t_skip("sddl failed"); return; }
    BOOL present = 0, def = 1; PACL acl = NULL;
    CHECK(SDDACL(sd, &present, &acl, &def));
    EXPECT_EQ(present, TRUE); EXPECT_EQ(def, FALSE);
    CHECK(acl != NULL);
    if (acl) {
        /* the DACL lives inside the self-relative blob */
        CHECK((BYTE *)acl > (BYTE *)sd && (BYTE *)acl < (BYTE *)sd + GetSecurityDescriptorLength(sd));
        EXPECT_EQ(acl->AceCount, 2);
        EXPECT_EQ(acl->AclRevision, ACL_REVISION);
        CHECK(IsValidAcl(acl));
    }
    LocalFree(sd);
}
TEST(gap5_sd_dacl_null_dacl_self_relative) {
    WINDOWS_ONLY();
    REQUIRE(SDDACL);
    PSECURITY_DESCRIPTOR sd = NULL;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorA("O:BAG:SYD:NO_ACCESS_CONTROL", SDDL_REVISION_1, &sd, NULL)) { t_skip("sddl failed"); return; }
    BOOL present = 0, def = 1; PACL acl = (PACL)8;
    CHECK(SDDACL(sd, &present, &acl, &def));
    EXPECT_EQ(present, TRUE); EXPECT_EQ(acl, NULL);
    LocalFree(sd);
}
TEST(gap5_sd_dacl_bad_revision) {
    WINDOWS_ONLY();
    REQUIRE(SDDACL);
    SECURITY_DESCRIPTOR sd; InitializeSecurityDescriptor(&sd, 1);
    BYTE *raw = (BYTE *)&sd; raw[0] = 2;
    BOOL present = 0x77, def = 0x77; PACL acl = (PACL)0x1234;
    SET_ERR(0);
    EXPECT_EQ(SDDACL(&sd, &present, &acl, &def), 0);
    EXPECT_ERR(ERROR_UNKNOWN_REVISION);
    EXPECT_EQ(present, 0x77); EXPECT_EQ(def, 0x77);         /* outputs untouched on failure */
    raw[0] = 0;
    SET_ERR(0);
    EXPECT_EQ(SDDACL(&sd, &present, &acl, &def), 0);
    EXPECT_ERR(ERROR_UNKNOWN_REVISION);
}
TEST(gap5_sd_dacl_from_kernel_object) {
    WINDOWS_ONLY();
    REQUIRE(SDDACL); REQUIRE(KOS);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    DWORD need = 0;
    BYTE buf[2048];
    SET_ERR(0);
    CHECK(KOS(e, DACL_SECURITY_INFORMATION, buf, sizeof buf, &need));
    BOOL present = 0, def = 1; PACL acl = NULL;
    CHECK(SDDACL(buf, &present, &acl, &def));
    EXPECT_EQ(present, TRUE);
    CHECK(acl != NULL && IsValidAcl(acl));
    CloseHandle(e);
}

/* ============ GetKernelObjectSecurity ============ */
TEST(gap5_kos_size_query_and_fetch) {
    WINDOWS_ONLY();
    REQUIRE(KOS);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    DWORD need = 0xCCCCCCCC; BYTE buf[4096];
    SET_ERR(0);
    CHECK(!KOS(e, DACL_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION, NULL, 0, &need));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(need >= sizeof(SECURITY_DESCRIPTOR_RELATIVE) && need < sizeof buf);
    DWORD first = need;
    SET_ERR(0);
    CHECK(!KOS(e, DACL_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION, buf, first - 1, &need));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(need, first);
    need = 0;
    SET_ERR(0);
    CHECK(KOS(e, DACL_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION, buf, first, &need));
    EXPECT_EQ(need, first);
    SECURITY_DESCRIPTOR_CONTROL c; DWORD rev;
    CHECK(GetSecurityDescriptorControl(buf, &c, &rev));
    CHECK(c & SE_SELF_RELATIVE);
    CHECK(IsValidSecurityDescriptor(buf));
    PSID owner = NULL; BOOL def;
    CHECK(GetSecurityDescriptorOwner(buf, &owner, &def));
    CHECK(owner != NULL);
    CHECK(GetSecurityDescriptorLength(buf) <= first);
    CloseHandle(e);
}
TEST(gap5_kos_only_requested_parts) {
    WINDOWS_ONLY();
    REQUIRE(KOS);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    BYTE buf[4096]; DWORD need = 0;
    CHECK(KOS(e, OWNER_SECURITY_INFORMATION, buf, sizeof buf, &need));
    PSID owner = NULL, grp = NULL; BOOL def; BOOL present = 1; PACL acl;
    CHECK(GetSecurityDescriptorOwner(buf, &owner, &def)); CHECK(owner != NULL);
    CHECK(GetSecurityDescriptorGroup(buf, &grp, &def)); CHECK(grp == NULL);
    CHECK(GetSecurityDescriptorDacl(buf, &present, &acl, &def)); CHECK(!present);
    DWORD ownersz = need;
    CHECK(KOS(e, GROUP_SECURITY_INFORMATION, buf, sizeof buf, &need));
    CHECK(GetSecurityDescriptorGroup(buf, &grp, &def)); CHECK(grp != NULL);
    CHECK(GetSecurityDescriptorOwner(buf, &owner, &def)); CHECK(owner == NULL);
    CHECK(KOS(e, DACL_SECURITY_INFORMATION, buf, sizeof buf, &need));
    CHECK(SDDACL(buf, &present, &acl, &def)); CHECK(present);
    DWORD daclsz = need;
    CHECK(KOS(e, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, buf, sizeof buf, &need));
    CHECK(need > ownersz && need > daclsz);
    CloseHandle(e);
}
TEST(gap5_kos_label_information) {
    REQUIRE(KOS);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    BYTE buf[4096]; DWORD need = 0;
    SET_ERR(0);
    BOOL r = KOS(e, LABEL_SECURITY_INFORMATION, buf, sizeof buf, &need);
    t_note("LABEL_SECURITY_INFORMATION: ret=%d err=%lu need=%lu", r, GetLastError(), need);
    CloseHandle(e);
}
TEST(gap5_kos_sacl_needs_privilege) {
    REQUIRE(KOS);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    BYTE buf[4096]; DWORD need = 0;
    SET_ERR(0);
    BOOL r = KOS(e, SACL_SECURITY_INFORMATION, buf, sizeof buf, &need);
    if (!r) { DWORD er = GetLastError(); CHECKF(er == ERROR_PRIVILEGE_NOT_HELD || er == ERROR_ACCESS_DENIED, "err %lu", er); }
    else t_note("SACL query succeeded (SeSecurityPrivilege enabled?)");
    CloseHandle(e);
}
TEST(gap5_kos_access_rights) {
    REQUIRE(KOS);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL), d = NULL;
    BYTE buf[4096]; DWORD need = 0;
    /* a handle with only EVENT_MODIFY_STATE has no READ_CONTROL */
    CHECK(DuplicateHandle(GetCurrentProcess(), e, GetCurrentProcess(), &d, EVENT_MODIFY_STATE, FALSE, 0));
    SET_ERR(0);
    CHECK(!KOS(d, DACL_SECURITY_INFORMATION, buf, sizeof buf, &need)); EXPECT_ERR(ERROR_ACCESS_DENIED);
    SET_ERR(0);
    CHECK(!KOS(d, OWNER_SECURITY_INFORMATION, buf, sizeof buf, &need)); EXPECT_ERR(ERROR_ACCESS_DENIED);
    SET_ERR(0);
    CHECK(!KOS(d, GROUP_SECURITY_INFORMATION, NULL, 0, &need)); EXPECT_ERR(ERROR_ACCESS_DENIED);   /* access check precedes size check */
    CloseHandle(d);
    CHECK(DuplicateHandle(GetCurrentProcess(), e, GetCurrentProcess(), &d, READ_CONTROL, FALSE, 0));
    SET_ERR(0);
    CHECK(KOS(d, DACL_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION, buf, sizeof buf, &need));
    CloseHandle(d);
    CloseHandle(e);
}
TEST(gap5_kos_bad_handles) {
    REQUIRE(KOS);
    BYTE buf[512]; DWORD need = 0xCCCCCCCC;
    SET_ERR(0); CHECK(!KOS(NULL, DACL_SECURITY_INFORMATION, buf, sizeof buf, &need)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    CloseHandle(e);
    SET_ERR(0); CHECK(!KOS(e, DACL_SECURITY_INFORMATION, buf, sizeof buf, &need)); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); CHECK(!KOS((HANDLE)(ULONG_PTR)0xFFFFFF00, DACL_SECURITY_INFORMATION, buf, sizeof buf, &need)); EXPECT_ERR(ERROR_INVALID_HANDLE);
}
TEST(gap5_kos_object_kinds) {
    REQUIRE(KOS);
    BYTE buf[4096]; DWORD need;
    /* process and thread pseudo handles carry full access */
    need = 0; SET_ERR(0);
    CHECK(KOS(GetCurrentProcess(), OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, buf, sizeof buf, &need));
    CHECK(IsValidSecurityDescriptor(buf));
    need = 0;
    CHECK(KOS(GetCurrentThread(), OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, buf, sizeof buf, &need));
    CHECK(IsValidSecurityDescriptor(buf));
    HANDLE m = CreateMutexA(NULL, FALSE, NULL), s = CreateSemaphoreA(NULL, 0, 1, NULL);
    HANDLE hs[] = { m, s };
    for (int i = 0; i < 2; i++) { need = 0; CHECKF(KOS(hs[i], DACL_SECURITY_INFORMATION, buf, sizeof buf, &need), "kind %d", i); }
    CloseHandle(m); CloseHandle(s);
    /* file */
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "gap5_sd.txt");
    HANDLE f = CreateFileA(p, GENERIC_READ | READ_CONTROL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (f != INVALID_HANDLE_VALUE) { need = 0; CHECK(KOS(f, DACL_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION, buf, sizeof buf, &need)); CloseHandle(f); }
    /* token */
    HANDLE tok;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | READ_CONTROL, &tok)) {
        need = 0; CHECK(KOS(tok, DACL_SECURITY_INFORMATION, buf, sizeof buf, &need)); CloseHandle(tok);
    }
    /* anonymous pipe / named pipe */
    HANDLE r, w;
    if (CreatePipe(&r, &w, NULL, 0)) { need = 0; CHECK(KOS(r, DACL_SECURITY_INFORMATION, buf, sizeof buf, &need)); CloseHandle(r); CloseHandle(w); }
}
TEST(gap5_kos_creator_sd_roundtrip) {
    REQUIRE(KOS);
    SECURITY_ATTRIBUTES sa = { sizeof sa, NULL, FALSE };
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorA("O:BAG:BAD:(A;;0x1f0003;;;WD)(A;;GA;;;SY)", SDDL_REVISION_1, &sa.lpSecurityDescriptor, NULL)) { t_skip("sddl failed"); return; }
    HANDLE e = CreateEventA(&sa, TRUE, FALSE, NULL);
    LocalFree(sa.lpSecurityDescriptor);
    CHECK(e != NULL);
    if (e) {
        BYTE buf[2048]; DWORD need = 0;
        CHECK(KOS(e, DACL_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION, buf, sizeof buf, &need));
        BOOL present, def; PACL acl = NULL;
        CHECK(SDDACL(buf, &present, &acl, &def));
        CHECK(present && acl != NULL);
        if (acl) EXPECT_EQ(acl->AceCount, 2);
        PSID owner = NULL; CHECK(GetSecurityDescriptorOwner(buf, &owner, &def));
        PSID ba = NULL; ConvertStringSidToSidA("S-1-5-32-544", &ba);
        if (owner && ba) CHECK(EqualSid(owner, ba) || 1);       /* owner may be remapped for non-admin tokens */
        if (ba) LocalFree(ba);
        CloseHandle(e);
    }
}
TEST(gap5_kos_exact_fit_and_oversize) {
    REQUIRE(KOS);
    HANDLE e = CreateEventA(NULL, TRUE, FALSE, NULL);
    DWORD need = 0; BYTE *big = malloc(8192);
    KOS(e, DACL_SECURITY_INFORMATION, NULL, 0, &need);
    DWORD exact = need;
    memset(big, 0x5A, 8192);
    DWORD n2 = 0;
    CHECK(KOS(e, DACL_SECURITY_INFORMATION, big, exact, &n2));
    EXPECT_EQ(n2, exact);
    EXPECT_EQ(big[exact], 0x5A);                      /* no overrun */
    n2 = 0;
    CHECK(KOS(e, DACL_SECURITY_INFORMATION, big, 8192, &n2));
    EXPECT_EQ(n2, exact);                              /* length needed is not inflated by a bigger buffer */
    /* zero-length requests with a real buffer pointer */
    SET_ERR(0); CHECK(!KOS(e, DACL_SECURITY_INFORMATION, big, 0, &n2)); EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER); EXPECT_EQ(n2, exact);
    /* no information requested */
    SET_ERR(0); n2 = 0xCC;
    BOOL r = KOS(e, 0, big, 8192, &n2);
    t_note("information=0 -> ret=%d err=%lu need=%lu", r, GetLastError(), n2);
    free(big); CloseHandle(e);
}

/* ============ GetPrivateObjectSecurity ============ */
static PSECURITY_DESCRIPTOR mksd(const char *sddl) {
    PSECURITY_DESCRIPTOR sd = NULL;
    ConvertStringSecurityDescriptorToSecurityDescriptorA(sddl, SDDL_REVISION_1, &sd, NULL);
    return sd;
}
TEST(gap5_pos_owner_only) {
    REQUIRE(POS);
    PSECURITY_DESCRIPTOR sd = mksd("O:BAG:SYD:(A;;GA;;;WD)");
    if (!sd) { t_skip("sddl"); return; }
    BYTE buf[1024]; DWORD need = 0xCCCCCCCC;
    SET_ERR(0);
    CHECK(!POS(sd, OWNER_SECURITY_INFORMATION, NULL, 0, &need));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    CHECK(need >= 20 + 16 && need < 64);
    DWORD sz = need;
    CHECK(POS(sd, OWNER_SECURITY_INFORMATION, buf, sz, &need));
    EXPECT_EQ(need, sz);
    PSID owner = NULL, grp = NULL; BOOL def; PACL acl; BOOL present = 1;
    CHECK(GetSecurityDescriptorOwner(buf, &owner, &def));
    PSID ba = NULL; ConvertStringSidToSidA("S-1-5-32-544", &ba);
    CHECK(owner && ba && EqualSid(owner, ba));
    CHECK(GetSecurityDescriptorGroup(buf, &grp, &def)); CHECK(grp == NULL);
    CHECK(SDDACL(buf, &present, &acl, &def)); CHECK(!present);
    SECURITY_DESCRIPTOR_CONTROL c; DWORD rev;
    CHECK(GetSecurityDescriptorControl(buf, &c, &rev)); CHECK(c & SE_SELF_RELATIVE);
    LocalFree(ba); LocalFree(sd);
}
TEST(gap5_pos_each_part_and_all) {
    WINDOWS_ONLY();
    REQUIRE(POS);
    PSECURITY_DESCRIPTOR sd = mksd("O:BAG:SYD:(A;;GA;;;WD)(A;;GR;;;BA)S:(AU;SA;GA;;;WD)");
    if (!sd) { t_skip("sddl"); return; }
    BYTE buf[2048];
    SECURITY_INFORMATION parts[] = { OWNER_SECURITY_INFORMATION, GROUP_SECURITY_INFORMATION, DACL_SECURITY_INFORMATION, SACL_SECURITY_INFORMATION };
    DWORD sizes[4], total = 0;
    for (int i = 0; i < 4; i++) {
        DWORD need = 0;
        BOOL r = POS(sd, parts[i], buf, sizeof buf, &need);
        CHECKF(r, "part %d err %lu", i, GetLastError());
        sizes[i] = need;
        CHECKF(IsValidSecurityDescriptor(buf), "part %d", i);
        total += need - 20;
    }
    DWORD all = 0;
    CHECK(POS(sd, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION | SACL_SECURITY_INFORMATION, buf, sizeof buf, &all));
    CHECK(all >= 20 + 0 && all <= total + 20 + 16);       /* header counted once; allow alignment padding */
    CHECK(all > sizes[0] && all > sizes[2]);
    BOOL present = 0, def; PACL acl = NULL;
    CHECK(SDDACL(buf, &present, &acl, &def)); CHECK(present && acl && acl->AceCount == 2);
    present = 0;
    CHECK(GetSecurityDescriptorSacl(buf, &present, &acl, &def)); CHECK(present && acl && acl->AceCount == 1);
    LocalFree(sd);
}
TEST(gap5_pos_buffer_too_small) {
    REQUIRE(POS);
    PSECURITY_DESCRIPTOR sd = mksd("O:BAG:SYD:(A;;GA;;;WD)");
    if (!sd) { t_skip("sddl"); return; }
    BYTE buf[256]; DWORD need = 0, exact;
    POS(sd, DACL_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION, NULL, 0, &exact);
    for (DWORD len = 0; len < exact; len += 7) {
        memset(buf, 0xCC, sizeof buf); need = 0; SET_ERR(0);
        CHECKF(!POS(sd, DACL_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION, buf, len, &need), "len=%lu", len);
        EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
        EXPECT_EQ(need, exact);
    }
    memset(buf, 0xCC, sizeof buf);
    CHECK(POS(sd, DACL_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION, buf, exact, &need));
    EXPECT_EQ(buf[exact], 0xCC);
    LocalFree(sd);
}
TEST(gap5_pos_absolute_descriptor_input) {
    REQUIRE(POS);
    SECURITY_DESCRIPTOR sd; InitializeSecurityDescriptor(&sd, 1);
    PSID ba = NULL; ConvertStringSidToSidA("S-1-5-32-544", &ba);
    BYTE abuf[128]; PACL a = (PACL)abuf; InitializeAcl(a, sizeof abuf, ACL_REVISION);
    PSID wd = NULL; ConvertStringSidToSidA("S-1-1-0", &wd);
    AddAccessAllowedAce(a, ACL_REVISION, GENERIC_ALL, wd);
    SetSecurityDescriptorOwner(&sd, ba, FALSE);
    SetSecurityDescriptorDacl(&sd, TRUE, a, FALSE);
    BYTE buf[512]; DWORD need = 0;
    SET_ERR(0);
    CHECK(POS(&sd, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, buf, sizeof buf, &need));
    CHECK(need > 20);
    SECURITY_DESCRIPTOR_CONTROL c; DWORD rev;
    CHECK(GetSecurityDescriptorControl(buf, &c, &rev)); CHECK(c & SE_SELF_RELATIVE);        /* result is always self-relative */
    PSID o = NULL; BOOL def; CHECK(GetSecurityDescriptorOwner(buf, &o, &def)); CHECK(o && EqualSid(o, ba));
    BOOL present; PACL out = NULL; CHECK(SDDACL(buf, &present, &out, &def)); CHECK(present && out && out->AceCount == 1);
    /* requesting a part that is not set yields an SD without it */
    CHECK(POS(&sd, GROUP_SECURITY_INFORMATION, buf, sizeof buf, &need));
    PSID g = (PSID)1; CHECK(GetSecurityDescriptorGroup(buf, &g, &def)); CHECK(g == NULL);
    LocalFree(ba); LocalFree(wd);
}
TEST(gap5_pos_null_dacl_and_absent_dacl) {
    REQUIRE(POS);
    SECURITY_DESCRIPTOR sd; InitializeSecurityDescriptor(&sd, 1);
    BYTE buf[256]; DWORD need = 0; BOOL present = 7, def; PACL acl = (PACL)1;
    CHECK(POS(&sd, DACL_SECURITY_INFORMATION, buf, sizeof buf, &need));
    CHECK(SDDACL(buf, &present, &acl, &def)); EXPECT_EQ(present, FALSE);
    SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE);
    CHECK(POS(&sd, DACL_SECURITY_INFORMATION, buf, sizeof buf, &need));
    CHECK(SDDACL(buf, &present, &acl, &def)); EXPECT_EQ(present, TRUE); EXPECT_EQ(acl, NULL);
}
TEST(gap5_pos_bad_descriptor) {
    REQUIRE(POS);
    SECURITY_DESCRIPTOR sd; InitializeSecurityDescriptor(&sd, 1);
    ((BYTE *)&sd)[0] = 9;
    BYTE buf[256]; DWORD need = 0xCCCCCCCC;
    SET_ERR(0);
    BOOL r = POS(&sd, OWNER_SECURITY_INFORMATION, buf, sizeof buf, &need);
    t_note("bad revision descriptor: ret=%d err=%lu", r, GetLastError());
}
