#include "g6.h"

/* ---- GetSidLengthRequired ---- */
TEST(gap6_sidlen_formula_all_counts) {
    REQUIRE(U(GetSidLengthRequired));
    int bad = 0;
    for (int n = 0; n < 256; n++) if (U(GetSidLengthRequired)((UCHAR)n) != (DWORD)(8 + 4 * n)) bad++;
    EXPECT_EQ(bad, 0);
}
TEST(gap6_sidlen_known_values) {
    REQUIRE(U(GetSidLengthRequired));
    EXPECT_EQ(U(GetSidLengthRequired)(0), 8);
    EXPECT_EQ(U(GetSidLengthRequired)(1), 12);
    EXPECT_EQ(U(GetSidLengthRequired)(2), 16);
    EXPECT_EQ(U(GetSidLengthRequired)(5), 28);
    EXPECT_EQ(U(GetSidLengthRequired)(15), 68);
    EXPECT_EQ(U(GetSidLengthRequired)(16), 72);   /* no upper bound check */
    EXPECT_EQ(U(GetSidLengthRequired)(255), 1028);
}
TEST(gap6_sidlen_matches_getlengthsid) {
    REQUIRE(U(GetSidLengthRequired));
    BYTE b[68];
    for (int n = 0; n <= 15; n++) {
        SID_IDENTIFIER_AUTHORITY a = {{0,0,0,0,0,5}};
        InitializeSid(b, &a, (BYTE)n);
        EXPECT_EQ(GetLengthSid(b), U(GetSidLengthRequired)((UCHAR)n));
    }
}
TEST(gap6_sidlen_repeat_stable) {
    REQUIRE(U(GetSidLengthRequired));
    for (int i = 0; i < 1000; i++) if (U(GetSidLengthRequired)(3) != 20) { CHECK(0); break; }
}
TEST(gap6_sidlen_lasterror_untouched) {
    REQUIRE(U(GetSidLengthRequired));
    SET_ERR(4321);
    U(GetSidLengthRequired)(2);
    /* pure arithmetic; not documented to touch last error: informational */
    t_note("GetSidLengthRequired leaves last error = %lu", GetLastError());
}

/* ---- GetSidIdentifierAuthority / SubAuthorityCount / SubAuthority ---- */
TEST(gap6_sid_idauth_pointer) {
    REQUIRE(U(GetSidIdentifierAuthority));
    BYTE b[68]; PSID s = g6_sid(b, 5, 2, 32, 544);
    PSID_IDENTIFIER_AUTHORITY a = U(GetSidIdentifierAuthority)(s);
    EXPECT_EQ(a, b + 2);
    CHECK(a->Value[5] == 5 && a->Value[0] == 0 && a->Value[4] == 0);
}
TEST(gap6_sid_idauth_values) {
    REQUIRE(U(GetSidIdentifierAuthority));
    BYTE b[68];
    BYTE auths[] = {0, 1, 2, 3, 4, 5, 9, 15, 16, 0xFF};
    for (unsigned i = 0; i < sizeof auths; i++) {
        PSID s = g6_sid(b, auths[i], 1, 18);
        PSID_IDENTIFIER_AUTHORITY a = U(GetSidIdentifierAuthority)(s);
        CHECK(a != NULL && a->Value[5] == auths[i]);
    }
}
TEST(gap6_sid_idauth_full_6bytes) {
    REQUIRE(U(GetSidIdentifierAuthority));
    BYTE b[68]; PSID s = g6_sid(b, 0, 1, 1);
    b[2] = 1; b[3] = 2; b[4] = 3; b[5] = 4; b[6] = 5; b[7] = 6;
    PSID_IDENTIFIER_AUTHORITY a = U(GetSidIdentifierAuthority)(s);
    CHECK(!memcmp(a->Value, "\x01\x02\x03\x04\x05\x06", 6));
}
TEST(gap6_sid_idauth_clears_lasterror) {
    REQUIRE(U(GetSidIdentifierAuthority));
    BYTE b[68]; PSID s = g6_sid(b, 5, 1, 18);
    SET_ERR(777);
    U(GetSidIdentifierAuthority)(s);
    EXPECT_ERR(0);
}
TEST(gap6_sid_idauth_on_invalid_sid_no_validation) {
    REQUIRE(U(GetSidIdentifierAuthority));
    /* returns pointer arithmetic only: revision / count not validated */
    BYTE b[68]; memset(b, 0xEE, sizeof b);
    PSID_IDENTIFIER_AUTHORITY a = U(GetSidIdentifierAuthority)(b);
    EXPECT_EQ(a, b + 2);
    EXPECT_ERR(0);
}
TEST(gap6_sid_idauth_matches_advapi) {
    REQUIRE(U(GetSidIdentifierAuthority));
    BYTE b[68]; PSID s = g6_sid(b, 5, 3, 21, 1, 2);
    EXPECT_EQ(U(GetSidIdentifierAuthority)(s), GetSidIdentifierAuthority(s));
}

TEST(gap6_sid_count_pointer) {
    REQUIRE(U(GetSidSubAuthorityCount));
    BYTE b[68];
    for (int n = 0; n <= 15; n++) {
        PSID s = g6_sid(b, 5, 0); b[1] = (BYTE)n;
        PUCHAR c = U(GetSidSubAuthorityCount)(s);
        EXPECT_EQ(c, b + 1);
        CHECK(*c == n);
    }
}
TEST(gap6_sid_count_write_through) {
    REQUIRE(U(GetSidSubAuthorityCount));
    BYTE b[68]; PSID s = g6_sid(b, 5, 2, 1, 2);
    *U(GetSidSubAuthorityCount)(s) = 1;
    EXPECT_EQ(GetSidSubAuthorityCount(s)[0], 1);
    EXPECT_EQ(GetLengthSid(s), 12);
}
TEST(gap6_sid_count_clears_lasterror) {
    REQUIRE(U(GetSidSubAuthorityCount));
    BYTE b[68]; PSID s = g6_sid(b, 5, 1, 18);
    SET_ERR(555);
    U(GetSidSubAuthorityCount)(s);
    EXPECT_ERR(0);
}
TEST(gap6_sid_count_no_validation) {
    REQUIRE(U(GetSidSubAuthorityCount));
    BYTE b[68]; memset(b, 0xAB, sizeof b);
    PUCHAR c = U(GetSidSubAuthorityCount)(b);
    EXPECT_EQ(c, b + 1);
    EXPECT_EQ(*c, 0xAB);
}

TEST(gap6_sid_subauth_pointers) {
    REQUIRE(U(GetSidSubAuthority));
    BYTE b[68]; PSID s = g6_sid(b, 5, 5, 21, 100, 200, 300, 1001);
    DWORD expect[] = {21, 100, 200, 300, 1001};
    for (DWORD i = 0; i < 5; i++) {
        PDWORD p = U(GetSidSubAuthority)(s, i);
        EXPECT_EQ(p, b + 8 + 4 * i);
        CHECK(*p == expect[i]);
    }
}
TEST(gap6_sid_subauth_beyond_count_is_pointer_math) {
    REQUIRE(U(GetSidSubAuthority));
    BYTE b[68]; PSID s = g6_sid(b, 5, 1, 18);
    /* No bounds check against the count: only compare pointers, never deref. */
    for (DWORD i = 0; i < 64; i++) {
        PDWORD p = U(GetSidSubAuthority)(s, i);
        EXPECT_EQ(p, (b + 8 + 4 * (ULONG_PTR)i));
    }
    EXPECT_ERR(0);
}
TEST(gap6_sid_subauth_huge_index_pointer) {
    REQUIRE(U(GetSidSubAuthority));
    BYTE b[68]; PSID s = g6_sid(b, 5, 1, 18);
    /* index is zero-extended 32->64 bit; pointer only, never dereferenced */
    PDWORD p = U(GetSidSubAuthority)(s, 0xFFFFFFFFu);
    EXPECT_EQ(p, (PDWORD)((ULONG_PTR)b + ((ULONGLONG)0xFFFFFFFFu + 2) * 4));
}
TEST(gap6_sid_subauth_clears_lasterror) {
    REQUIRE(U(GetSidSubAuthority));
    BYTE b[68]; PSID s = g6_sid(b, 5, 2, 32, 544);
    SET_ERR(31337);
    U(GetSidSubAuthority)(s, 1);
    EXPECT_ERR(0);
}
TEST(gap6_sid_subauth_write_through) {
    REQUIRE(U(GetSidSubAuthority));
    BYTE b[68]; PSID s = g6_sid(b, 5, 2, 32, 544);
    *U(GetSidSubAuthority)(s, 1) = 545;
    EXPECT_EQ(*GetSidSubAuthority(s, 1), 545);
    EXPECT_EQ(*GetSidSubAuthority(s, 0), 32);
}
TEST(gap6_sid_subauth_all_15) {
    REQUIRE(U(GetSidSubAuthority));
    BYTE b[68]; PSID s = g6_sid(b, 5, 0); b[1] = 15;
    for (DWORD i = 0; i < 15; i++) *GetSidSubAuthority(s, i) = 0x1000 + i;
    int bad = 0;
    for (DWORD i = 0; i < 15; i++) if (*U(GetSidSubAuthority)(s, i) != 0x1000 + i) bad++;
    EXPECT_EQ(bad, 0);
}
TEST(gap6_sid_accessors_agree_with_convert) {
    REQUIRE(U(GetSidSubAuthority)); REQUIRE(U(GetSidSubAuthorityCount));
    PSID s = NULL;
    if (!ConvertStringSidToSidA("S-1-5-21-1111-2222-3333-500", &s)) { t_skip("ConvertStringSidToSid failed"); return; }
    EXPECT_EQ(*U(GetSidSubAuthorityCount)(s), 5);
    EXPECT_EQ(*U(GetSidSubAuthority)(s, 0), 21);
    EXPECT_EQ(*U(GetSidSubAuthority)(s, 1), 1111);
    EXPECT_EQ(*U(GetSidSubAuthority)(s, 2), 2222);
    EXPECT_EQ(*U(GetSidSubAuthority)(s, 3), 3333);
    EXPECT_EQ(*U(GetSidSubAuthority)(s, 4), 500);
    LocalFree(s);
}
TEST(gap6_sid_wellknown_everyone) {
    REQUIRE(U(GetSidSubAuthorityCount)); REQUIRE(U(GetSidIdentifierAuthority));
    PSID s = NULL;
    if (!ConvertStringSidToSidA("S-1-1-0", &s)) { t_skip("convert failed"); return; }
    EXPECT_EQ(*U(GetSidSubAuthorityCount)(s), 1);
    EXPECT_EQ(U(GetSidIdentifierAuthority)(s)->Value[5], 1);
    EXPECT_EQ(*U(GetSidSubAuthority)(s, 0), 0);
    LocalFree(s);
}

/* ---- InitializeSid ---- */
TEST(gap6_initsid_counts_0_to_15) {
    REQUIRE(U(InitializeSid));
    SID_IDENTIFIER_AUTHORITY a = {{0, 0, 0, 0, 0, 5}};
    for (int n = 0; n <= 15; n++) {
        BYTE b[80]; memset(b, 0xCC, sizeof b);
        SET_ERR(0);
        BOOL r = U(InitializeSid)(b, &a, (BYTE)n);
        CHECKF(r == TRUE, "count %d", n);
        CHECKF(b[0] == 1, "rev count %d", n);
        CHECKF(b[1] == n, "count %d", n);
        CHECKF(!memcmp(b + 2, a.Value, 6), "auth %d", n);
        /* subauthorities are not initialized and bytes beyond the SID are untouched */
        int bad = 0;
        for (int i = 8 + 4 * n; i < 80; i++) if (b[i] != 0xCC) bad++;
        CHECKF(bad == 0, "overrun count %d", n);
        CHECKF(IsValidSid(b), "valid %d", n);
    }
}
TEST(gap6_initsid_counts_too_big) {
    REQUIRE(U(InitializeSid));
    SID_IDENTIFIER_AUTHORITY a = {{0, 0, 0, 0, 0, 5}};
    int counts[] = {16, 17, 32, 100, 128, 255};
    for (unsigned i = 0; i < sizeof counts / sizeof *counts; i++) {
        BYTE b[80]; memset(b, 0xCC, sizeof b);
        SET_ERR(0);
        BOOL r = U(InitializeSid)(b, &a, (BYTE)counts[i]);
        CHECKF(r == FALSE, "count %d", counts[i]);
        EXPECT_ERR(ERROR_INVALID_PARAMETER);
        int bad = 0;
        for (unsigned j = 0; j < sizeof b; j++) if (b[j] != 0xCC) bad++;
        CHECKF(bad == 0, "buffer modified on failure, count %d", counts[i]);
    }
}
TEST(gap6_initsid_authority_variants) {
    REQUIRE(U(InitializeSid));
    BYTE b[80];
    BYTE auths[][6] = {{0,0,0,0,0,0},{0,0,0,0,0,1},{0,0,0,0,0,16},{1,2,3,4,5,6},{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},{0,0,0,0,1,0}};
    for (unsigned i = 0; i < 6; i++) {
        SID_IDENTIFIER_AUTHORITY a; memcpy(a.Value, auths[i], 6);
        memset(b, 0, sizeof b);
        CHECK(U(InitializeSid)(b, &a, 1));
        CHECK(!memcmp(b + 2, auths[i], 6));
    }
}
TEST(gap6_initsid_then_set_subauth) {
    REQUIRE(U(InitializeSid));
    SID_IDENTIFIER_AUTHORITY a = {{0, 0, 0, 0, 0, 5}};
    BYTE b[80];
    CHECK(U(InitializeSid)(b, &a, 2));
    *GetSidSubAuthority(b, 0) = 32; *GetSidSubAuthority(b, 1) = 544;
    PSID ref = NULL;
    if (ConvertStringSidToSidA("S-1-5-32-544", &ref)) { CHECK(EqualSid(b, ref)); LocalFree(ref); }
    char *str = NULL;
    if (ConvertSidToStringSidA(b, &str)) { EXPECT_STR(str, "S-1-5-32-544"); LocalFree(str); }
}
TEST(gap6_initsid_zero_count_valid) {
    REQUIRE(U(InitializeSid));
    SID_IDENTIFIER_AUTHORITY a = {{0, 0, 0, 0, 0, 5}};
    BYTE b[16]; memset(b, 0, sizeof b);
    CHECK(U(InitializeSid)(b, &a, 0));
    EXPECT_EQ(GetLengthSid(b), 8);
}
TEST(gap6_initsid_overwrites_old_header) {
    REQUIRE(U(InitializeSid));
    SID_IDENTIFIER_AUTHORITY a = {{0, 0, 0, 0, 0, 5}};
    BYTE b[80]; memset(b, 0x77, sizeof b);
    CHECK(U(InitializeSid)(b, &a, 3));
    EXPECT_EQ(b[0], 1); EXPECT_EQ(b[1], 3);
}
TEST(gap6_initsid_repeated) {
    REQUIRE(U(InitializeSid));
    SID_IDENTIFIER_AUTHORITY a = {{0, 0, 0, 0, 0, 5}};
    BYTE b[80];
    for (int i = 0; i < 2000; i++) if (!U(InitializeSid)(b, &a, (BYTE)(i % 16))) { CHECK(0); break; }
}
TEST(gap6_initsid_boundary_15_16) {
    REQUIRE(U(InitializeSid));
    SID_IDENTIFIER_AUTHORITY a = {{0, 0, 0, 0, 0, 5}};
    BYTE b[80];
    CHECK(U(InitializeSid)(b, &a, SID_MAX_SUB_AUTHORITIES));
    CHECK(!U(InitializeSid)(b, &a, SID_MAX_SUB_AUTHORITIES + 1));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}

/* ---- GetWindowsAccountDomainSid ---- */
static int dom(PSID s, PSID out, DWORD *cb) { return U(GetWindowsAccountDomainSid)(s, out, cb); }

TEST(gap6_domsid_valid_account_sid) {
    REQUIRE(U(GetWindowsAccountDomainSid));
    BYTE b[68], o[80]; PSID s = g6_sid(b, 5, 5, 21, 10, 20, 30, 500);
    memset(o, 0xCC, sizeof o);
    DWORD cb = 24; SET_ERR(0);
    CHECK(dom(s, o, &cb));
    EXPECT_EQ(cb, 24);
    EXPECT_EQ(o[0], 1); EXPECT_EQ(o[1], 4);
    CHECK(!memcmp(o + 2, b + 2, 6));
    DWORD *sa = (DWORD *)(o + 8);
    EXPECT_EQ(sa[0], 21); EXPECT_EQ(sa[1], 10); EXPECT_EQ(sa[2], 20); EXPECT_EQ(sa[3], 30);
    EXPECT_EQ(o[24], 0xCC);
    CHECK(IsValidSid(o));
}
TEST(gap6_domsid_domain_sid_itself) {
    REQUIRE(U(GetWindowsAccountDomainSid));
    BYTE b[68], o[80]; PSID s = g6_sid(b, 5, 4, 21, 1, 2, 3);
    DWORD cb = sizeof o;
    CHECK(dom(s, o, &cb));
    EXPECT_EQ(cb, 24);
    CHECK(EqualSid(o, s));
}
TEST(gap6_domsid_longer_sid_trimmed) {
    REQUIRE(U(GetWindowsAccountDomainSid));
    BYTE b[68], o[80]; PSID s = g6_sid(b, 5, 7, 21, 1, 2, 3, 4, 5, 6);
    DWORD cb = 80;
    CHECK(dom(s, o, &cb));
    EXPECT_EQ(o[1], 4);
    EXPECT_EQ(cb, 24);
}
TEST(gap6_domsid_buffer_size_queries) {
    REQUIRE(U(GetWindowsAccountDomainSid));
    BYTE b[68], o[80]; PSID s = g6_sid(b, 5, 5, 21, 10, 20, 30, 500);
    DWORD sizes[] = {0, 1, 8, 16, 23};
    for (unsigned i = 0; i < 5; i++) {
        DWORD cb = sizes[i]; memset(o, 0xCC, sizeof o);
        SET_ERR(0);
        BOOL r = dom(s, o, &cb);
        CHECKF(r == FALSE, "cb=%lu", sizes[i]);
        EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
        CHECKF(cb == 24, "required size cb=%lu got %lu", sizes[i], cb);
        CHECKF(o[0] == 0xCC, "output written on failure");
    }
    /* NULL output with too-small size is the documented size query */
    DWORD cb = 0; SET_ERR(0);
    CHECK(!dom(s, NULL, &cb));
    EXPECT_ERR(ERROR_INSUFFICIENT_BUFFER);
    EXPECT_EQ(cb, 24);
}
TEST(gap6_domsid_exact_and_larger_buffer) {
    REQUIRE(U(GetWindowsAccountDomainSid));
    BYTE b[68], o[80]; PSID s = g6_sid(b, 5, 5, 21, 10, 20, 30, 500);
    DWORD cb = 24; CHECK(dom(s, o, &cb)); EXPECT_EQ(cb, 24);
    cb = 80; CHECK(dom(s, o, &cb)); EXPECT_EQ(cb, 24);
    cb = 0xFFFFFFFF; CHECK(dom(s, o, &cb)); EXPECT_EQ(cb, 24);
}
TEST(gap6_domsid_null_outputs) {
    REQUIRE(U(GetWindowsAccountDomainSid));
    BYTE b[68], o[80]; PSID s = g6_sid(b, 5, 5, 21, 10, 20, 30, 500);
    SET_ERR(0);
    CHECK(!dom(s, o, NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
    DWORD cb = 24; SET_ERR(0);
    CHECK(!dom(s, NULL, &cb));
    EXPECT_ERR(ERROR_INVALID_PARAMETER);
}
TEST(gap6_domsid_non_account_sids) {
    REQUIRE(U(GetWindowsAccountDomainSid));
    struct { const char *s; } t[] = {
        {"S-1-5-18"}, {"S-1-5-32-544"}, {"S-1-1-0"}, {"S-1-5-11"}, {"S-1-5-21"},
        {"S-1-5-21-1"}, {"S-1-5-21-1-2"}, {"S-1-5-22-1-2-3"}, {"S-1-3-0"}, {"S-1-16-4096"},
        {"S-1-5-80-1-2-3-4-5"},
    };
    for (unsigned i = 0; i < sizeof t / sizeof *t; i++) {
        PSID s = NULL; BYTE o[80]; DWORD cb = sizeof o;
        if (!ConvertStringSidToSidA(t[i].s, &s)) continue;
        SET_ERR(0);
        BOOL r = dom(s, o, &cb);
        CHECKF(r == FALSE, "%s", t[i].s);
        CHECKF(GetLastError() == ERROR_NON_ACCOUNT_SID, "%s err %lu", t[i].s, GetLastError());
        LocalFree(s);
    }
}
TEST(gap6_domsid_non_nt_authority_with_21) {
    REQUIRE(U(GetWindowsAccountDomainSid));
    BYTE b[68], o[80]; DWORD cb = sizeof o;
    PSID s = g6_sid(b, 6, 5, 21, 1, 2, 3, 500);
    SET_ERR(0);
    CHECK(!dom(s, o, &cb));
    EXPECT_ERR(ERROR_NON_ACCOUNT_SID);
}
TEST(gap6_domsid_invalid_sid) {
    REQUIRE(U(GetWindowsAccountDomainSid));
    BYTE b[80], o[80]; DWORD cb = sizeof o;
    PSID s = g6_sid(b, 5, 5, 21, 1, 2, 3, 500);
    b[0] = 2;                       /* bad revision */
    SET_ERR(0);
    CHECK(!dom(s, o, &cb));
    EXPECT_ERR(ERROR_INVALID_SID);
    b[0] = 0; SET_ERR(0);
    CHECK(!dom(s, o, &cb)); EXPECT_ERR(ERROR_INVALID_SID);
    b[0] = 1; b[1] = 16; SET_ERR(0);
    CHECK(!dom(s, o, &cb)); EXPECT_ERR(ERROR_INVALID_SID);
    b[1] = 255; SET_ERR(0);
    CHECK(!dom(s, o, &cb)); EXPECT_ERR(ERROR_INVALID_SID);
    /* invalid SID takes priority over NULL size pointer */
    b[0] = 9; SET_ERR(0);
    CHECK(!dom(s, o, NULL)); EXPECT_ERR(ERROR_INVALID_SID);
}
TEST(gap6_domsid_current_user) {
    REQUIRE(U(GetWindowsAccountDomainSid));
    HANDLE tok = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok)) { t_skip("OpenProcessToken"); return; }
    BYTE ub[SECURITY_MAX_SID_SIZE + 16]; DWORD n = 0;
    if (GetTokenInformation(tok, TokenUser, ub, sizeof ub, &n)) {
        PSID u = ((TOKEN_USER *)ub)->User.Sid;
        BYTE o[80]; DWORD cb = sizeof o; SET_ERR(0);
        BOOL r = dom(u, o, &cb);
        if (r) { EXPECT_EQ(cb, 24); EXPECT_EQ(o[1], 4); EXPECT_EQ(*(DWORD *)(o + 8), 21);
                 CHECK(!memcmp((BYTE *)u + 2, o + 2, 6 + 16)); }
        else t_note("user SID not a domain/machine account SID, err=%lu", GetLastError());
    }
    CloseHandle(tok);
}
