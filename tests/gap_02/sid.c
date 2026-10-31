/* CreateWellKnownSid, DeleteAce, DeriveCapabilitySidsFromName */
#include "g2.h"

static void wks_check(int type, const char *want)
{
    BOOL (WINAPI *f)(WELL_KNOWN_SID_TYPE, PSID, PSID, DWORD *) = (void *)t_proc("CreateWellKnownSid");
    BYTE buf[SECURITY_MAX_SID_SIZE], dom[32], tmp[SECURITY_MAX_SID_SIZE];
    DWORD cb = sizeof buf, len;
    char s[160];
    REQUIRE(f);
    memset(buf, 0xCC, sizeof buf);
    SET_ERR(0xDEAD);
    if (!f(type, NULL, buf, &cb)) { CHECKF(0, "type %d not supported, err=%lu", type, GetLastError()); return; }
    if (GetLastError() != 0xDEAD) t_note("type %d: last error touched on success: %lu", type, GetLastError());
    CHECK(IsValidSid(buf));
    g2_sid_str(buf, s, sizeof s);
    CHECKF(!strcmp(s, want), "type %d got %s want %s", type, s, want);
    len = GetLengthSid(buf);
    EXPECT_EQ(cb, len);
    CHECK(IsWellKnownSid(buf, type));
    /* size query */
    cb = 0;
    SET_ERR(0);
    CHECK(!f(type, NULL, NULL, &cb));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(cb, len);
    /* too small by one: buffer untouched, size updated */
    memset(tmp, 0xCC, sizeof tmp);
    cb = len - 1;
    SET_ERR(0);
    CHECK(!f(type, NULL, tmp, &cb));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(cb, len);
    CHECK(tmp[0] == 0xCC && tmp[len - 1] == 0xCC);
    /* exact */
    cb = len;
    memset(tmp, 0, sizeof tmp);
    CHECK(f(type, NULL, tmp, &cb));
    EXPECT_EQ(cb, len);
    CHECK(EqualSid(tmp, buf));
    /* oversized buffer: size is reported as the required length */
    cb = sizeof tmp;
    CHECK(f(type, NULL, tmp, &cb));
    EXPECT_EQ(cb, len);
    /* domain SID is ignored for non domain-relative types */
    g2_mksid(dom, 5, 4, 21u, 1u, 2u, 3u);
    cb = sizeof tmp;
    CHECK(f(type, dom, tmp, &cb));
    CHECK(EqualSid(tmp, buf));
    /* NULL size pointer */
    SET_ERR(0);
    CHECK(!f(type, NULL, tmp, NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}

#define WKS(nm, ty, str) TEST(gap2_wks_##nm) { wks_check(ty, str); }
WKS(null, WinNullSid, "S-1-0-0")
WKS(world, WinWorldSid, "S-1-1-0")
WKS(local, WinLocalSid, "S-1-2-0")
WKS(creator_owner, WinCreatorOwnerSid, "S-1-3-0")
WKS(creator_group, WinCreatorGroupSid, "S-1-3-1")
WKS(creator_owner_server, WinCreatorOwnerServerSid, "S-1-3-2")
WKS(creator_group_server, WinCreatorGroupServerSid, "S-1-3-3")
WKS(nt_authority, WinNtAuthoritySid, "S-1-5")
WKS(dialup, WinDialupSid, "S-1-5-1")
WKS(network, WinNetworkSid, "S-1-5-2")
WKS(batch, WinBatchSid, "S-1-5-3")
WKS(interactive, WinInteractiveSid, "S-1-5-4")
WKS(service, WinServiceSid, "S-1-5-6")
WKS(anonymous, WinAnonymousSid, "S-1-5-7")
WKS(proxy, WinProxySid, "S-1-5-8")
WKS(enterprise_controllers, WinEnterpriseControllersSid, "S-1-5-9")
WKS(self, WinSelfSid, "S-1-5-10")
WKS(authenticated_user, WinAuthenticatedUserSid, "S-1-5-11")
WKS(restricted_code, WinRestrictedCodeSid, "S-1-5-12")
WKS(terminal_server, WinTerminalServerSid, "S-1-5-13")
WKS(remote_logon_id, WinRemoteLogonIdSid, "S-1-5-14")
WKS(local_system, WinLocalSystemSid, "S-1-5-18")
WKS(local_service, WinLocalServiceSid, "S-1-5-19")
WKS(network_service, WinNetworkServiceSid, "S-1-5-20")
WKS(builtin_domain, WinBuiltinDomainSid, "S-1-5-32")
WKS(builtin_admins, WinBuiltinAdministratorsSid, "S-1-5-32-544")
WKS(builtin_users, WinBuiltinUsersSid, "S-1-5-32-545")
WKS(builtin_guests, WinBuiltinGuestsSid, "S-1-5-32-546")
WKS(builtin_power_users, WinBuiltinPowerUsersSid, "S-1-5-32-547")
WKS(builtin_account_ops, WinBuiltinAccountOperatorsSid, "S-1-5-32-548")
WKS(builtin_system_ops, WinBuiltinSystemOperatorsSid, "S-1-5-32-549")
WKS(builtin_print_ops, WinBuiltinPrintOperatorsSid, "S-1-5-32-550")
WKS(builtin_backup_ops, WinBuiltinBackupOperatorsSid, "S-1-5-32-551")
WKS(builtin_replicator, WinBuiltinReplicatorSid, "S-1-5-32-552")
WKS(builtin_prewin2k, WinBuiltinPreWindows2000CompatibleAccessSid, "S-1-5-32-554")
WKS(builtin_rdp_users, WinBuiltinRemoteDesktopUsersSid, "S-1-5-32-555")
WKS(builtin_netcfg_ops, WinBuiltinNetworkConfigurationOperatorsSid, "S-1-5-32-556")
WKS(ntlm_auth, WinNTLMAuthenticationSid, "S-1-5-64-10")
WKS(digest_auth, WinDigestAuthenticationSid, "S-1-5-64-21")
WKS(schannel_auth, WinSChannelAuthenticationSid, "S-1-5-64-14")
WKS(this_org, WinThisOrganizationSid, "S-1-5-15")
WKS(other_org, WinOtherOrganizationSid, "S-1-5-1000")
WKS(builtin_forest_trust, WinBuiltinIncomingForestTrustBuildersSid, "S-1-5-32-557")
WKS(builtin_perfmon, WinBuiltinPerfMonitoringUsersSid, "S-1-5-32-558")
WKS(builtin_perflog, WinBuiltinPerfLoggingUsersSid, "S-1-5-32-559")
WKS(builtin_authz_access, WinBuiltinAuthorizationAccessSid, "S-1-5-32-560")
WKS(builtin_ts_license, WinBuiltinTerminalServerLicenseServersSid, "S-1-5-32-561")
WKS(builtin_dcom_users, WinBuiltinDCOMUsersSid, "S-1-5-32-562")
WKS(builtin_iusers, WinBuiltinIUsersSid, "S-1-5-32-568")
WKS(iuser, WinIUserSid, "S-1-5-17")
WKS(builtin_crypto_ops, WinBuiltinCryptoOperatorsSid, "S-1-5-32-569")
WKS(label_untrusted, WinUntrustedLabelSid, "S-1-16-0")
WKS(label_low, WinLowLabelSid, "S-1-16-4096")
WKS(label_medium, WinMediumLabelSid, "S-1-16-8192")
WKS(label_high, WinHighLabelSid, "S-1-16-12288")
WKS(label_system, WinSystemLabelSid, "S-1-16-16384")
WKS(write_restricted, WinWriteRestrictedCodeSid, "S-1-5-33")
WKS(creator_owner_rights, WinCreatorOwnerRightsSid, "S-1-3-4")
WKS(enterprise_ro_controllers, WinEnterpriseReadonlyControllersSid, "S-1-5-498")
WKS(builtin_eventlog_readers, WinBuiltinEventLogReadersGroup, "S-1-5-32-573")
WKS(builtin_certsvc_dcom, WinBuiltinCertSvcDComAccessGroup, "S-1-5-32-574")
WKS(label_medium_plus, WinMediumPlusLabelSid, "S-1-16-8448")
WKS(console_logon, WinConsoleLogonSid, "S-1-2-1")
WKS(app_package_authority, WinApplicationPackageAuthoritySid, "S-1-15-2")
WKS(builtin_any_package, WinBuiltinAnyPackageSid, "S-1-15-2-1")
WKS(cap_internet_client, WinCapabilityInternetClientSid, "S-1-15-3-1")
WKS(cap_internet_client_server, WinCapabilityInternetClientServerSid, "S-1-15-3-2")
WKS(cap_private_network, WinCapabilityPrivateNetworkClientServerSid, "S-1-15-3-3")
WKS(cap_pictures, WinCapabilityPicturesLibrarySid, "S-1-15-3-4")
WKS(cap_videos, WinCapabilityVideosLibrarySid, "S-1-15-3-5")
WKS(cap_music, WinCapabilityMusicLibrarySid, "S-1-15-3-6")
WKS(cap_documents, WinCapabilityDocumentsLibrarySid, "S-1-15-3-7")
WKS(cap_shared_certs, WinCapabilitySharedUserCertificatesSid, "S-1-15-3-9")
WKS(cap_enterprise_auth, WinCapabilityEnterpriseAuthenticationSid, "S-1-15-3-8")
WKS(cap_removable_storage, WinCapabilityRemovableStorageSid, "S-1-15-3-10")

/* ---- domain relative SIDs ---- */
static const struct { int type; DWORD rid; const char *name; } dom_rids[] = {
    { WinAccountAdministratorSid, 500, "administrator" },
    { WinAccountGuestSid, 501, "guest" },
    { WinAccountKrbtgtSid, 502, "krbtgt" },
    { WinAccountDomainAdminsSid, 512, "domain admins" },
    { WinAccountDomainUsersSid, 513, "domain users" },
    { WinAccountDomainGuestsSid, 514, "domain guests" },
    { WinAccountComputersSid, 515, "computers" },
    { WinAccountControllersSid, 516, "controllers" },
    { WinAccountCertAdminsSid, 517, "cert admins" },
    { WinAccountSchemaAdminsSid, 518, "schema admins" },
    { WinAccountEnterpriseAdminsSid, 519, "enterprise admins" },
    { WinAccountPolicyAdminsSid, 520, "policy admins" },
    { WinAccountRasAndIasServersSid, 553, "ras and ias" },
    { WinAccountReadonlyControllersSid, 521, "ro controllers" },
};

TEST(gap2_wks_domain_relative_table) {
    BOOL (WINAPI *f)(WELL_KNOWN_SID_TYPE, PSID, PSID, DWORD *) = (void *)t_proc("CreateWellKnownSid");
    BYTE dom[32], out[SECURITY_MAX_SID_SIZE];
    size_t i;
    REQUIRE(f);
    g2_mksid(dom, 5, 4, 21u, 1111u, 2222u, 3333u);
    for (i = 0; i < sizeof dom_rids / sizeof dom_rids[0]; i++) {
        DWORD cb = sizeof out;
        char s[160], want[160];
        memset(out, 0, sizeof out);
        BOOL ok = f(dom_rids[i].type, dom, out, &cb);
        CHECKF(ok, "%s err=%lu", dom_rids[i].name, GetLastError());
        if (!ok) continue;
        g2_sid_str(out, s, sizeof s);
        snprintf(want, sizeof want, "S-1-5-21-1111-2222-3333-%lu", (unsigned long)dom_rids[i].rid);
        CHECKF(!strcmp(s, want), "%s got %s want %s", dom_rids[i].name, s, want);
        EXPECT_EQ(cb, 28);
        EXPECT_EQ(GetLengthSid(out), 28);
    }
}

TEST(gap2_wks_domain_relative_requires_domain_sid) {
    BOOL (WINAPI *f)(WELL_KNOWN_SID_TYPE, PSID, PSID, DWORD *) = (void *)t_proc("CreateWellKnownSid");
    BYTE out[SECURITY_MAX_SID_SIZE];
    size_t i;
    REQUIRE(f);
    for (i = 0; i < sizeof dom_rids / sizeof dom_rids[0]; i++) {
        DWORD cb = sizeof out;
        SET_ERR(0);
        BOOL ok = f(dom_rids[i].type, NULL, out, &cb);
        CHECKF(!ok, "%s unexpectedly succeeded without domain SID", dom_rids[i].name);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}

TEST(gap2_wks_domain_relative_size_query) {
    BOOL (WINAPI *f)(WELL_KNOWN_SID_TYPE, PSID, PSID, DWORD *) = (void *)t_proc("CreateWellKnownSid");
    BYTE dom[32], out[SECURITY_MAX_SID_SIZE];
    DWORD cb;
    REQUIRE(f);
    g2_mksid(dom, 5, 4, 21u, 1u, 2u, 3u);
    cb = 0; SET_ERR(0);
    CHECK(!f(WinAccountAdministratorSid, dom, NULL, &cb));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(cb, 28);
    cb = 27; SET_ERR(0);
    CHECK(!f(WinAccountAdministratorSid, dom, out, &cb));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(cb, 28);
    cb = 28;
    CHECK(f(WinAccountAdministratorSid, dom, out, &cb));
    EXPECT_EQ(cb, 28);
}

TEST(gap2_wks_domain_sid_one_subauth) {
    BOOL (WINAPI *f)(WELL_KNOWN_SID_TYPE, PSID, PSID, DWORD *) = (void *)t_proc("CreateWellKnownSid");
    BYTE dom[16], out[SECURITY_MAX_SID_SIZE];
    DWORD cb = sizeof out; char s[100];
    REQUIRE(f);
    g2_mksid(dom, 5, 1, 21u);
    if (f(WinAccountDomainAdminsSid, dom, out, &cb)) {
        g2_sid_str(out, s, sizeof s);
        t_note("domain SID with 1 subauth -> %s", s);
    } else t_note("domain SID with 1 subauth rejected, err=%lu", GetLastError());
}

TEST(gap2_wks_invalid_domain_sid) {
    BOOL (WINAPI *f)(WELL_KNOWN_SID_TYPE, PSID, PSID, DWORD *) = (void *)t_proc("CreateWellKnownSid");
    BYTE bad[32], out[SECURITY_MAX_SID_SIZE];
    DWORD cb;
    REQUIRE(f);
    g2_mksid(bad, 5, 4, 21u, 1u, 2u, 3u);
    bad[0] = 2;                      /* bad revision */
    cb = sizeof out; SET_ERR(0);
    CHECK(!f(WinAccountAdministratorSid, bad, out, &cb));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    cb = sizeof out; SET_ERR(0);
    CHECK(!f(WinWorldSid, bad, out, &cb));     /* validated even when unused */
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    g2_mksid(bad, 5, 4, 21u, 1u, 2u, 3u);
    bad[1] = 16;                     /* subauthority count above SID_MAX_SUB_AUTHORITIES */
    cb = sizeof out; SET_ERR(0);
    CHECK(!f(WinAccountAdministratorSid, bad, out, &cb));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}

TEST(gap2_wks_logon_ids_not_creatable) {
    BOOL (WINAPI *f)(WELL_KNOWN_SID_TYPE, PSID, PSID, DWORD *) = (void *)t_proc("CreateWellKnownSid");
    BYTE out[SECURITY_MAX_SID_SIZE]; DWORD cb = sizeof out;
    REQUIRE(f);
    SET_ERR(0);
    CHECK(!f(WinLogonIdsSid, NULL, out, &cb));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}

TEST(gap2_wks_out_of_range_types) {
    BOOL (WINAPI *f)(WELL_KNOWN_SID_TYPE, PSID, PSID, DWORD *) = (void *)t_proc("CreateWellKnownSid");
    BYTE out[SECURITY_MAX_SID_SIZE];
    static const int types[] = { -1, 0x7FFFFFFF, (int)0x80000000, 1000, 0xFFFF, 4096 };
    size_t i;
    REQUIRE(f);
    for (i = 0; i < sizeof types / sizeof types[0]; i++) {
        DWORD cb = sizeof out;
        SET_ERR(0);
        CHECKF(!f((WELL_KNOWN_SID_TYPE)types[i], NULL, out, &cb), "type %d succeeded", types[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
    }
}

TEST(gap2_wks_null_size_pointer) {
    BOOL (WINAPI *f)(WELL_KNOWN_SID_TYPE, PSID, PSID, DWORD *) = (void *)t_proc("CreateWellKnownSid");
    BYTE out[SECURITY_MAX_SID_SIZE];
    REQUIRE(f);
    SET_ERR(0);
    CHECK(!f(WinWorldSid, NULL, out, NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0);
    CHECK(!f(WinWorldSid, NULL, NULL, NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}

TEST(gap2_wks_null_buffer_large_size) {
    BOOL (WINAPI *f)(WELL_KNOWN_SID_TYPE, PSID, PSID, DWORD *) = (void *)t_proc("CreateWellKnownSid");
    DWORD cb = 100;
    BOOL ok;
    REQUIRE(f);
    SET_ERR(0);
    ok = f(WinWorldSid, NULL, NULL, &cb);
    t_note("NULL pSid with cb=100: ret=%d err=%lu cb=%lu", ok, GetLastError(), cb);
    EXPECT_EQ(cb, 12);
}

TEST(gap2_wks_size_is_not_clobbered_by_other_types) {
    BOOL (WINAPI *f)(WELL_KNOWN_SID_TYPE, PSID, PSID, DWORD *) = (void *)t_proc("CreateWellKnownSid");
    BYTE out[SECURITY_MAX_SID_SIZE]; DWORD cb;
    REQUIRE(f);
    cb = 0; CHECK(!f(WinNtAuthoritySid, NULL, out, &cb)); EXPECT_EQ(cb, 8);
    cb = 0; CHECK(!f(WinWorldSid, NULL, out, &cb)); EXPECT_EQ(cb, 12);
    cb = 0; CHECK(!f(WinBuiltinAdministratorsSid, NULL, out, &cb)); EXPECT_EQ(cb, 16);
    cb = 0; CHECK(!f(WinNTLMAuthenticationSid, NULL, out, &cb)); EXPECT_EQ(cb, 16);
    cb = 0; CHECK(!f(WinBuiltinAnyPackageSid, NULL, out, &cb)); EXPECT_EQ(cb, 16);
}

TEST(gap2_wks_repeat_deterministic) {
    BOOL (WINAPI *f)(WELL_KNOWN_SID_TYPE, PSID, PSID, DWORD *) = (void *)t_proc("CreateWellKnownSid");
    BYTE a[SECURITY_MAX_SID_SIZE], b[SECURITY_MAX_SID_SIZE];
    int i;
    REQUIRE(f);
    for (i = 0; i < 200; i++) {
        DWORD ca = sizeof a, cb = sizeof b;
        CHECK(f(WinBuiltinAdministratorsSid, NULL, a, &ca));
        CHECK(f(WinBuiltinAdministratorsSid, NULL, b, &cb));
        CHECK(ca == cb && EqualSid(a, b));
    }
}

TEST(gap2_wks_all_types_scan) {
    BOOL (WINAPI *f)(WELL_KNOWN_SID_TYPE, PSID, PSID, DWORD *) = (void *)t_proc("CreateWellKnownSid");
    BYTE dom[32], out[SECURITY_MAX_SID_SIZE];
    int t, ok_n = 0, domain_n = 0;
    REQUIRE(f);
    g2_mksid(dom, 5, 4, 21u, 7u, 8u, 9u);
    for (t = 0; t < 140; t++) {
        DWORD cb = sizeof out;
        char s[160];
        if (f(t, NULL, out, &cb)) {
            ok_n++;
            CHECK(IsValidSid(out));
            CHECK(GetLengthSid(out) == cb);
        } else {
            DWORD e = GetLastError();
            cb = sizeof out;
            if (f(t, dom, out, &cb)) { domain_n++; g2_sid_str(out, s, sizeof s); CHECK(IsValidSid(out)); }
            else t_note("type %d unsupported (err %lu)", t, e);
        }
    }
    CHECK(ok_n >= 70);
    CHECK(domain_n >= 14);
    t_note("scan: %d plain, %d domain-relative", ok_n, domain_n);
}

TEST(gap2_wks_domain_sid_does_not_alias) {
    BOOL (WINAPI *f)(WELL_KNOWN_SID_TYPE, PSID, PSID, DWORD *) = (void *)t_proc("CreateWellKnownSid");
    BYTE dom[32], buf[SECURITY_MAX_SID_SIZE];
    DWORD cb = sizeof buf;
    REQUIRE(f);
    /* output buffer may be the domain SID buffer itself? use a separate copy, domain sid must be unchanged */
    g2_mksid(dom, 5, 4, 21u, 1u, 2u, 3u);
    BYTE copy[32]; memcpy(copy, dom, 32);
    CHECK(f(WinAccountGuestSid, dom, buf, &cb));
    CHECK(!memcmp(copy, dom, 32));
}

/* ---------------- DeleteAce ---------------- */
static PACL make_acl(BYTE *buf, DWORD size, int n)
{
    BYTE sidb[16]; PSID s = g2_mksid(sidb, 1, 1, 0u);
    int i;
    PACL a = (PACL)buf;
    if (!InitializeAcl(a, size, ACL_REVISION)) return NULL;
    for (i = 0; i < n; i++)
        if (!AddAccessAllowedAce(a, ACL_REVISION, 0x100 + i, s)) return NULL;
    return a;
}

TEST(gap2_deleteace_middle) {
    BOOL (WINAPI *f)(PACL, DWORD) = (void *)t_proc("DeleteAce");
    BYTE buf[256]; PACL a; void *ace;
    REQUIRE(f);
    a = make_acl(buf, sizeof buf, 3);
    CHECK(a && g2_ace_count(a) == 3);
    DWORD sz = a->AclSize;
    CHECK(f(a, 1));
    EXPECT_EQ(g2_ace_count(a), 2);
    EXPECT_EQ(a->AclSize, sz);
    CHECK(GetAce(a, 0, &ace) && ((ACCESS_ALLOWED_ACE *)ace)->Mask == 0x100);
    CHECK(GetAce(a, 1, &ace) && ((ACCESS_ALLOWED_ACE *)ace)->Mask == 0x102);
    CHECK(IsValidAcl(a));
}
TEST(gap2_deleteace_first) {
    BOOL (WINAPI *f)(PACL, DWORD) = (void *)t_proc("DeleteAce");
    BYTE buf[256]; PACL a; void *ace;
    REQUIRE(f);
    a = make_acl(buf, sizeof buf, 3);
    CHECK(f(a, 0));
    EXPECT_EQ(g2_ace_count(a), 2);
    CHECK(GetAce(a, 0, &ace) && ((ACCESS_ALLOWED_ACE *)ace)->Mask == 0x101);
    CHECK(GetAce(a, 1, &ace) && ((ACCESS_ALLOWED_ACE *)ace)->Mask == 0x102);
}
TEST(gap2_deleteace_last) {
    BOOL (WINAPI *f)(PACL, DWORD) = (void *)t_proc("DeleteAce");
    BYTE buf[256]; PACL a; void *ace;
    REQUIRE(f);
    a = make_acl(buf, sizeof buf, 3);
    CHECK(f(a, 2));
    EXPECT_EQ(g2_ace_count(a), 2);
    CHECK(GetAce(a, 1, &ace) && ((ACCESS_ALLOWED_ACE *)ace)->Mask == 0x101);
    CHECK(!GetAce(a, 2, &ace));
}
TEST(gap2_deleteace_all_one_by_one) {
    BOOL (WINAPI *f)(PACL, DWORD) = (void *)t_proc("DeleteAce");
    BYTE buf[512]; PACL a;
    int i;
    REQUIRE(f);
    a = make_acl(buf, sizeof buf, 10);
    CHECK(g2_ace_count(a) == 10);
    for (i = 9; i >= 0; i--) {
        CHECK(f(a, 0));
        EXPECT_EQ(g2_ace_count(a), i);
        CHECK(IsValidAcl(a));
    }
    SET_ERR(0);
    CHECK(!f(a, 0));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap2_deleteace_index_out_of_range) {
    BOOL (WINAPI *f)(PACL, DWORD) = (void *)t_proc("DeleteAce");
    BYTE buf[256]; PACL a;
    static const DWORD idx[] = { 3, 4, 100, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF };
    size_t i;
    REQUIRE(f);
    a = make_acl(buf, sizeof buf, 3);
    for (i = 0; i < sizeof idx / sizeof idx[0]; i++) {
        SET_ERR(0);
        CHECKF(!f(a, idx[i]), "index %lu", idx[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        EXPECT_EQ(g2_ace_count(a), 3);
    }
}
TEST(gap2_deleteace_empty_acl) {
    BOOL (WINAPI *f)(PACL, DWORD) = (void *)t_proc("DeleteAce");
    BYTE buf[64]; PACL a = (PACL)buf;
    REQUIRE(f);
    CHECK(InitializeAcl(a, sizeof buf, ACL_REVISION));
    SET_ERR(0);
    CHECK(!f(a, 0));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap2_deleteace_then_add_reuses_space) {
    BOOL (WINAPI *f)(PACL, DWORD) = (void *)t_proc("DeleteAce");
    BYTE buf[64 ]; BYTE sidb[16]; PSID s = g2_mksid(sidb, 1, 1, 0u);
    PACL a = (PACL)buf;
    REQUIRE(f);
    CHECK(InitializeAcl(a, 8 + 3 * 20, ACL_REVISION));   /* ACE = 4 header + 4 mask + 12 SID */
    CHECK(AddAccessAllowedAce(a, ACL_REVISION, 1, s));
    CHECK(AddAccessAllowedAce(a, ACL_REVISION, 2, s));
    CHECK(AddAccessAllowedAce(a, ACL_REVISION, 3, s));
    SET_ERR(0);
    CHECK(!AddAccessAllowedAce(a, ACL_REVISION, 4, s));   /* full */
    EXPECT_ERR(ERROR_ALLOTTED_SPACE_EXCEEDED);
    CHECK(f(a, 1));
    CHECK(AddAccessAllowedAce(a, ACL_REVISION, 4, s));   /* fits again */
    EXPECT_EQ(g2_ace_count(a), 3);
}
TEST(gap2_deleteace_mixed_ace_types) {
    BOOL (WINAPI *f)(PACL, DWORD) = (void *)t_proc("DeleteAce");
    BYTE buf[256]; BYTE sidb[16]; PSID s = g2_mksid(sidb, 1, 1, 0u);
    PACL a = (PACL)buf; void *ace;
    REQUIRE(f);
    CHECK(InitializeAcl(a, sizeof buf, ACL_REVISION));
    CHECK(AddAccessDeniedAce(a, ACL_REVISION, 0x11, s));
    CHECK(AddAccessAllowedAce(a, ACL_REVISION, 0x22, s));
    CHECK(AddAccessDeniedAce(a, ACL_REVISION, 0x33, s));
    CHECK(f(a, 1));
    CHECK(GetAce(a, 0, &ace) && ((ACE_HEADER *)ace)->AceType == ACCESS_DENIED_ACE_TYPE);
    CHECK(GetAce(a, 1, &ace) && ((ACE_HEADER *)ace)->AceType == ACCESS_DENIED_ACE_TYPE && ((ACCESS_DENIED_ACE *)ace)->Mask == 0x33);
}
TEST(gap2_deleteace_kernel32_or_kernelbase_only) {
    /* DeleteAce lives in kernelbase on Win10 (advapi32 forwards there) */
    CHECK(t_proc_kb("DeleteAce") != NULL);
}

/* ---------------- DeriveCapabilitySidsFromName ---------------- */
typedef BOOL (WINAPI *derive_fn)(LPCWSTR, PSID **, DWORD *, PSID **, DWORD *);

static void free_derived(PSID *g, DWORD gn, PSID *c, DWORD cn)
{
    DWORD i;
    if (g) { for (i = 0; i < gn; i++) LocalFree(g[i]); LocalFree(g); }
    if (c) { for (i = 0; i < cn; i++) LocalFree(c[i]); LocalFree(c); }
}

TEST(gap2_derivecap_basic) {
    derive_fn f = (derive_fn)t_proc("DeriveCapabilitySidsFromName");
    PSID *g = NULL, *c = NULL; DWORD gn = 0, cn = 0;
    char s1[200], s2[200];
    REQUIRE(f);
    CHECK(f(L"internetClient", &g, &gn, &c, &cn));
    EXPECT_EQ(gn, 1);
    EXPECT_EQ(cn, 1);
    CHECK(g && c && g[0] && c[0]);
    if (g && c && g[0] && c[0]) {
        CHECK(IsValidSid(g[0]));
        CHECK(IsValidSid(c[0]));
        g2_sid_str(g[0], s1, sizeof s1);
        g2_sid_str(c[0], s2, sizeof s2);
        t_note("capability group sid %s, capability sid %s", s1, s2);
        CHECK(strncmp(s1, "S-1-5-21-", 9) == 0 || strncmp(s1, "S-1-15-3-1024-", 14) == 0);
        CHECK(strncmp(s2, "S-1-15-3-1024-", 14) == 0 || strncmp(s2, "S-1-5-21-", 9) == 0);
        CHECK(!EqualSid(g[0], c[0]));
    }
    free_derived(g, gn, c, cn);
}

TEST(gap2_derivecap_deterministic) {
    derive_fn f = (derive_fn)t_proc("DeriveCapabilitySidsFromName");
    PSID *g1 = NULL, *c1 = NULL, *g2 = NULL, *c2 = NULL; DWORD gn1 = 0, cn1 = 0, gn2 = 0, cn2 = 0;
    REQUIRE(f);
    CHECK(f(L"picturesLibrary", &g1, &gn1, &c1, &cn1));
    CHECK(f(L"picturesLibrary", &g2, &gn2, &c2, &cn2));
    if (g1 && g2 && c1 && c2) {
        CHECK(EqualSid(g1[0], g2[0]));
        CHECK(EqualSid(c1[0], c2[0]));
    }
    free_derived(g1, gn1, c1, cn1);
    free_derived(g2, gn2, c2, cn2);
}

TEST(gap2_derivecap_different_names_differ) {
    derive_fn f = (derive_fn)t_proc("DeriveCapabilitySidsFromName");
    PSID *g1 = NULL, *c1 = NULL, *g2 = NULL, *c2 = NULL; DWORD gn1 = 0, cn1 = 0, gn2 = 0, cn2 = 0;
    REQUIRE(f);
    CHECK(f(L"internetClient", &g1, &gn1, &c1, &cn1));
    CHECK(f(L"internetClientServer", &g2, &gn2, &c2, &cn2));
    if (g1 && g2 && c1 && c2) {
        CHECK(!EqualSid(g1[0], g2[0]));
        CHECK(!EqualSid(c1[0], c2[0]));
    }
    free_derived(g1, gn1, c1, cn1);
    free_derived(g2, gn2, c2, cn2);
}

TEST(gap2_derivecap_case_sensitivity) {
    derive_fn f = (derive_fn)t_proc("DeriveCapabilitySidsFromName");
    PSID *g1 = NULL, *c1 = NULL, *g2 = NULL, *c2 = NULL; DWORD gn1 = 0, cn1 = 0, gn2 = 0, cn2 = 0;
    REQUIRE(f);
    CHECK(f(L"internetClient", &g1, &gn1, &c1, &cn1));
    CHECK(f(L"INTERNETCLIENT", &g2, &gn2, &c2, &cn2));
    if (g1 && g2) t_note("capability names case-insensitive: %d", EqualSid(g1[0], g2[0]));
    free_derived(g1, gn1, c1, cn1);
    free_derived(g2, gn2, c2, cn2);
}

TEST(gap2_derivecap_empty_and_null_name) {
    derive_fn f = (derive_fn)t_proc("DeriveCapabilitySidsFromName");
    PSID *g = NULL, *c = NULL; DWORD gn = 0, cn = 0;
    REQUIRE(f);
    SET_ERR(0);
    CHECK(!f(L"", &g, &gn, &c, &cn));
    t_note("empty name: err=%lu", GetLastError());
    CHECK(g == NULL && c == NULL && gn == 0 && cn == 0);
    SET_ERR(0);
    CHECK(!f(NULL, &g, &gn, &c, &cn));
    t_note("NULL name: err=%lu", GetLastError());
    CHECK(g == NULL && c == NULL && gn == 0 && cn == 0);
}

TEST(gap2_derivecap_many_names) {
    derive_fn f = (derive_fn)t_proc("DeriveCapabilitySidsFromName");
    static const WCHAR *names[] = { L"internetClient", L"internetClientServer", L"privateNetworkClientServer",
        L"picturesLibrary", L"videosLibrary", L"musicLibrary", L"documentsLibrary", L"enterpriseAuthentication",
        L"sharedUserCertificates", L"removableStorage", L"appointments", L"contacts", L"x", L"a b c" };
    PSID firstg[14] = {0};
    size_t i, j;
    REQUIRE(f);
    for (i = 0; i < sizeof names / sizeof names[0]; i++) {
        PSID *g = NULL, *c = NULL; DWORD gn = 0, cn = 0;
        BOOL ok = f(names[i], &g, &gn, &c, &cn);
        CHECKF(ok, "name %zu err %lu", i, GetLastError());
        if (ok && g) {
            BYTE *copy = (BYTE *)LocalAlloc(0, GetLengthSid(g[0]));
            CopySid(GetLengthSid(g[0]), copy, g[0]);
            firstg[i] = copy;
        }
        free_derived(g, gn, c, cn);
    }
    for (i = 0; i < 14; i++)
        for (j = i + 1; j < 14; j++)
            if (firstg[i] && firstg[j]) CHECK(!EqualSid(firstg[i], firstg[j]));
    for (i = 0; i < 14; i++) LocalFree(firstg[i]);
}

TEST(gap2_derivecap_long_name) {
    derive_fn f = (derive_fn)t_proc("DeriveCapabilitySidsFromName");
    WCHAR name[300]; int i;
    PSID *g = NULL, *c = NULL; DWORD gn = 0, cn = 0;
    REQUIRE(f);
    for (i = 0; i < 299; i++) name[i] = L'a' + (i % 26);
    name[299] = 0;
    BOOL ok = f(name, &g, &gn, &c, &cn);
    t_note("299-char capability name: ok=%d err=%lu", ok, ok ? 0 : GetLastError());
    free_derived(g, gn, c, cn);
}
