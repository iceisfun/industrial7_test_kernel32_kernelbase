/* gap_04: GetAce / GetAclInformation on hand built and real ACLs */
#include "t.h"

#define A8 __attribute__((aligned(8)))

static const BYTE SID_WORLD[12]  = { 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0 };                 /* S-1-1-0 */
static const BYTE SID_SYSTEM[12] = { 1, 1, 0, 0, 0, 0, 0, 5, 18, 0, 0, 0 };                /* S-1-5-18 */
static const BYTE SID_ADMINS[16] = { 1, 2, 0, 0, 0, 0, 0, 5, 32, 0, 0, 0, 0x20, 2, 0, 0 }; /* S-1-5-32-544 */
static const BYTE SID_LABEL[12]  = { 1, 1, 0, 0, 0, 0, 0, 16, 0, 0x20, 0, 0 };             /* S-1-16-8192 */

static DWORD status_to_dos(LONG st) {
    typeof(&RtlNtStatusToDosError) f = (typeof(&RtlNtStatusToDosError))UNT(RtlNtStatusToDosError);
    return f ? f(st) : 0xFFFFFFFF;
}

/* returns 0 on success, else a bit identifying the first failed step */
static int mk_acl(BYTE *buf, DWORD size, DWORD rev, int nace) {
    BOOL (WINAPI *init)(PACL, DWORD, DWORD) = (void *)U(InitializeAcl);
    BOOL (WINAPI *allow)(PACL, DWORD, DWORD, PSID) = (void *)U(AddAccessAllowedAce);
    BOOL (WINAPI *deny)(PACL, DWORD, DWORD, PSID) = (void *)U(AddAccessDeniedAce);
    BOOL (WINAPI *audit)(PACL, DWORD, DWORD, PSID, BOOL, BOOL) = (void *)U(AddAuditAccessAce);
    memset(buf, 0xCC, size);
    if (!init || !allow || !deny || !audit) return 1;
    if (!init((PACL)buf, size, rev)) return 2;
    if (nace > 0 && !allow((PACL)buf, rev, 0x1F01FF, (PSID)SID_WORLD)) return 3;
    if (nace > 1 && !deny((PACL)buf, rev, 0x00000002, (PSID)SID_SYSTEM)) return 4;
    if (nace > 2 && !audit((PACL)buf, rev, 0x00000004, (PSID)SID_ADMINS, TRUE, FALSE)) return 5;
    return 0;
}

#define FAILB(call, err) do { SET_ERR(0xBAD); BOOL r_ = (call); EXPECT_EQ(r_, 0); EXPECT_ERR(err); } while (0)

/* ---------------- GetAclInformation ---------------- */

TEST(gap4_aclinfo_revision_class) {
    REQUIRE(U(GetAclInformation)); REQUIRE(U(InitializeAcl));
    static const DWORD revs[] = { 2, 3, 4 };
    for (int i = 0; i < 3; i++) {
        BYTE A8 buf[128]; ACL_REVISION_INFORMATION ri; memset(&ri, 0x5A, sizeof ri);
        if (mk_acl(buf, sizeof buf, revs[i], 0)) { CHECKF(0, "mk_acl rev %lu err=%lu", revs[i], GetLastError()); continue; }
        SET_ERR(0xBAD);
        BOOL r = U(GetAclInformation)((PACL)buf, &ri, sizeof ri, AclRevisionInformation);
        CHECKF(r, "rev %lu err=%lu", revs[i], GetLastError());
        EXPECT_EQ(ri.AclRevision, revs[i]);
    }
}

TEST(gap4_aclinfo_revision_zero_and_big_buffer) {
    REQUIRE(U(GetAclInformation));
    BYTE A8 buf[64]; BYTE out[32];
    CHECK(!mk_acl(buf, sizeof buf, ACL_REVISION, 0));
    memset(out, 0x77, sizeof out);
    CHECK(U(GetAclInformation)((PACL)buf, out, sizeof out, AclRevisionInformation));
    EXPECT_EQ(*(DWORD *)out, 2);
    for (int i = 4; i < 32; i++) if (out[i] != 0x77) { CHECKF(0, "byte %d overwritten", i); break; }
}

TEST(gap4_aclinfo_revision_small_buffers) {
    REQUIRE(U(GetAclInformation));
    BYTE A8 buf[64]; BYTE out[16];
    CHECK(!mk_acl(buf, sizeof buf, ACL_REVISION, 0));
    for (DWORD n = 0; n < 4; n++) {
        memset(out, 0x77, sizeof out);
        FAILB(U(GetAclInformation)((PACL)buf, out, n, AclRevisionInformation), ERROR_INSUFFICIENT_BUFFER);
        EXPECT_EQ(out[0], 0x77);
    }
}

TEST(gap4_aclinfo_revision_null_buffer_zero_size) {
    REQUIRE(U(GetAclInformation));
    BYTE A8 buf[64];
    CHECK(!mk_acl(buf, sizeof buf, ACL_REVISION, 0));
    FAILB(U(GetAclInformation)((PACL)buf, NULL, 0, AclRevisionInformation), ERROR_INSUFFICIENT_BUFFER);
    FAILB(U(GetAclInformation)((PACL)buf, NULL, 0, AclSizeInformation), ERROR_INSUFFICIENT_BUFFER);
}

TEST(gap4_aclinfo_size_empty_acl) {
    REQUIRE(U(GetAclInformation));
    BYTE A8 buf[100]; ACL_SIZE_INFORMATION si; memset(&si, 0x5A, sizeof si);
    CHECK(!mk_acl(buf, sizeof buf, ACL_REVISION, 0));
    SET_ERR(0xBAD);
    BOOL r = U(GetAclInformation)((PACL)buf, &si, sizeof si, AclSizeInformation);
    CHECKF(r, "err=%lu", GetLastError());
    EXPECT_EQ(si.AceCount, 0);
    EXPECT_EQ(si.AclBytesInUse, 8);
    EXPECT_EQ(si.AclBytesFree, ((PACL)buf)->AclSize - 8);
}

TEST(gap4_aclinfo_size_counts_grow) {
    REQUIRE(U(GetAclInformation));
    BYTE A8 buf[256]; ACL_SIZE_INFORMATION si;
    static const DWORD inuse[] = { 8, 8 + 20, 8 + 20 + 20, 8 + 20 + 20 + 24 };
    for (int n = 0; n <= 3; n++) {
        CHECK(!mk_acl(buf, sizeof buf, ACL_REVISION, n));
        memset(&si, 0, sizeof si);
        CHECK(U(GetAclInformation)((PACL)buf, &si, sizeof si, AclSizeInformation));
        EXPECT_EQ(si.AceCount, n);
        EXPECT_EQ(si.AclBytesInUse, inuse[n]);
        EXPECT_EQ(si.AclBytesInUse + si.AclBytesFree, ((PACL)buf)->AclSize);
        EXPECT_EQ(((PACL)buf)->AceCount, n);
    }
}

TEST(gap4_aclinfo_size_exact_fit_has_no_free_bytes) {
    REQUIRE(U(GetAclInformation));
    BYTE A8 buf[64]; ACL_SIZE_INFORMATION si;
    CHECK(!mk_acl(buf, 8 + 20, ACL_REVISION, 1));
    CHECK(U(GetAclInformation)((PACL)buf, &si, sizeof si, AclSizeInformation));
    EXPECT_EQ(si.AceCount, 1);
    EXPECT_EQ(si.AclBytesInUse, 28);
    EXPECT_EQ(si.AclBytesFree, 0);
}

TEST(gap4_aclinfo_size_small_buffers) {
    REQUIRE(U(GetAclInformation));
    BYTE A8 buf[64]; BYTE out[32];
    CHECK(!mk_acl(buf, sizeof buf, ACL_REVISION, 1));
    for (DWORD n = 0; n < 12; n++) {
        memset(out, 0x77, sizeof out);
        FAILB(U(GetAclInformation)((PACL)buf, out, n, AclSizeInformation), ERROR_INSUFFICIENT_BUFFER);
        EXPECT_EQ(out[0], 0x77);
    }
    memset(out, 0x77, sizeof out);
    CHECK(U(GetAclInformation)((PACL)buf, out, 12, AclSizeInformation));
    EXPECT_EQ(out[12], 0x77);
}

TEST(gap4_aclinfo_invalid_classes) {
    REQUIRE(U(GetAclInformation)); REQUIRE(UNT(RtlNtStatusToDosError));
    BYTE A8 buf[64]; BYTE out[32];
    CHECK(!mk_acl(buf, sizeof buf, ACL_REVISION, 1));
    DWORD want = status_to_dos((LONG)0xC0000003);   /* STATUS_INVALID_INFO_CLASS */
    static const DWORD cls[] = { 0, 3, 4, 5, 100, 0x80000000, 0xFFFFFFFF };
    for (int i = 0; i < 7; i++) {
        SET_ERR(0xBAD);
        BOOL r = U(GetAclInformation)((PACL)buf, out, sizeof out, (ACL_INFORMATION_CLASS)cls[i]);
        CHECKF(!r, "class %lu", cls[i]);
        CHECKF(GetLastError() == want, "class %lu err=%lu want %lu", cls[i], GetLastError(), want);
    }
}

TEST(gap4_aclinfo_invalid_class_checked_before_size) {
    REQUIRE(U(GetAclInformation)); REQUIRE(UNT(RtlNtStatusToDosError));
    BYTE A8 buf[64];
    CHECK(!mk_acl(buf, sizeof buf, ACL_REVISION, 0));
    DWORD want = status_to_dos((LONG)0xC0000003);
    SET_ERR(0xBAD);
    CHECK(!U(GetAclInformation)((PACL)buf, NULL, 0, (ACL_INFORMATION_CLASS)7));
    EXPECT_ERR(want);
}

TEST(gap4_aclinfo_bad_revisions) {
    REQUIRE(U(GetAclInformation));
    BYTE A8 buf[64]; BYTE out[32];
    static const BYTE bad[] = { 0, 1, 5, 6, 0x10, 0x80, 0xFF };
    for (int i = 0; i < 7; i++) {
        CHECK(!mk_acl(buf, sizeof buf, ACL_REVISION, 0));
        buf[0] = bad[i];
        FAILB(U(GetAclInformation)((PACL)buf, out, sizeof out, AclRevisionInformation), ERROR_INVALID_PARAMETER);
        FAILB(U(GetAclInformation)((PACL)buf, out, sizeof out, AclSizeInformation), ERROR_INVALID_PARAMETER);
    }
}

TEST(gap4_aclinfo_bad_revision_beats_bad_class) {
    REQUIRE(U(GetAclInformation));
    BYTE A8 buf[64]; BYTE out[32];
    CHECK(!mk_acl(buf, sizeof buf, ACL_REVISION, 0));
    buf[0] = 1;
    FAILB(U(GetAclInformation)((PACL)buf, out, sizeof out, (ACL_INFORMATION_CLASS)9), ERROR_INVALID_PARAMETER);
}

TEST(gap4_aclinfo_all_valid_revisions_accepted) {
    REQUIRE(U(GetAclInformation));
    BYTE A8 buf[64]; ACL_REVISION_INFORMATION ri;
    for (BYTE r = 2; r <= 4; r++) {
        CHECK(!mk_acl(buf, sizeof buf, ACL_REVISION, 0));
        buf[0] = r;
        CHECK(U(GetAclInformation)((PACL)buf, &ri, sizeof ri, AclRevisionInformation));
        EXPECT_EQ(ri.AclRevision, r);
    }
}

TEST(gap4_aclinfo_corrupt_acl_size_field) {
    REQUIRE(U(GetAclInformation));
    BYTE A8 buf[128]; ACL_SIZE_INFORMATION si;
    CHECK(!mk_acl(buf, sizeof buf, ACL_REVISION, 2));
    ((PACL)buf)->AceCount = 0;               /* claim no ACEs: in-use is just the header */
    memset(&si, 0, sizeof si);
    BOOL r = U(GetAclInformation)((PACL)buf, &si, sizeof si, AclSizeInformation);
    CHECK(r);
    if (r) { EXPECT_EQ(si.AceCount, 0); EXPECT_EQ(si.AclBytesInUse, 8); }
}

/* ---------------- GetAce ---------------- */

TEST(gap4_getace_walk_three_aces) {
    REQUIRE(U(GetAce));
    BYTE A8 buf[256]; PACL acl = (PACL)buf;
    CHECK(!mk_acl(buf, sizeof buf, ACL_REVISION, 3));
    static const DWORD off[] = { 8, 28, 48 };
    static const BYTE type[] = { ACCESS_ALLOWED_ACE_TYPE, ACCESS_DENIED_ACE_TYPE, SYSTEM_AUDIT_ACE_TYPE };
    static const WORD size[] = { 20, 20, 24 };
    static const DWORD mask[] = { 0x1F01FF, 2, 4 };
    for (DWORD i = 0; i < 3; i++) {
        LPVOID ace = (LPVOID)0x1; SET_ERR(0xBAD);
        BOOL r = U(GetAce)(acl, i, &ace);
        CHECKF(r, "index %lu err=%lu", i, GetLastError());
        if (!r) continue;
        EXPECT_EQ(ace, buf + off[i]);
        ACE_HEADER *h = ace;
        EXPECT_EQ(h->AceType, type[i]);
        EXPECT_EQ(h->AceSize, size[i]);
        EXPECT_EQ(((ACCESS_ALLOWED_ACE *)ace)->Mask, mask[i]);
    }
}

TEST(gap4_getace_sid_contents) {
    REQUIRE(U(GetAce));
    BYTE A8 buf[256];
    CHECK(!mk_acl(buf, sizeof buf, ACL_REVISION, 3));
    ACCESS_ALLOWED_ACE *a;
    CHECK(U(GetAce)((PACL)buf, 0, (LPVOID *)&a));
    CHECK(!memcmp(&a->SidStart, SID_WORLD, 12));
    CHECK(U(GetAce)((PACL)buf, 1, (LPVOID *)&a));
    CHECK(!memcmp(&a->SidStart, SID_SYSTEM, 12));
    CHECK(U(GetAce)((PACL)buf, 2, (LPVOID *)&a));
    CHECK(!memcmp(&a->SidStart, SID_ADMINS, 16));
}

TEST(gap4_getace_audit_flags_in_header) {
    REQUIRE(U(GetAce));
    BYTE A8 buf[256];
    CHECK(!mk_acl(buf, sizeof buf, ACL_REVISION, 3));
    ACE_HEADER *h;
    CHECK(U(GetAce)((PACL)buf, 2, (LPVOID *)&h));
    EXPECT_EQ(h->AceFlags & SUCCESSFUL_ACCESS_ACE_FLAG, SUCCESSFUL_ACCESS_ACE_FLAG);
    EXPECT_EQ(h->AceFlags & FAILED_ACCESS_ACE_FLAG, 0);
}

TEST(gap4_getace_index_equal_count) {
    REQUIRE(U(GetAce));
    BYTE A8 buf[256]; LPVOID ace = (LPVOID)0x1234;
    for (int n = 0; n <= 3; n++) {
        CHECK(!mk_acl(buf, sizeof buf, ACL_REVISION, n));
        FAILB(U(GetAce)((PACL)buf, n, &ace), ERROR_INVALID_PARAMETER);
    }
}

TEST(gap4_getace_huge_indices) {
    REQUIRE(U(GetAce));
    BYTE A8 buf[256]; LPVOID ace;
    CHECK(!mk_acl(buf, sizeof buf, ACL_REVISION, 2));
    static const DWORD idx[] = { 2, 3, 100, 0xFFFF, 0x10000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF };
    for (int i = 0; i < 8; i++) { FAILB(U(GetAce)((PACL)buf, idx[i], &ace), ERROR_INVALID_PARAMETER); }
}

TEST(gap4_getace_empty_acl) {
    REQUIRE(U(GetAce));
    BYTE A8 buf[64]; LPVOID ace;
    CHECK(!mk_acl(buf, sizeof buf, ACL_REVISION, 0));
    FAILB(U(GetAce)((PACL)buf, 0, &ace), ERROR_INVALID_PARAMETER);
}

TEST(gap4_getace_failure_leaves_output_alone_or_set) {
    REQUIRE(U(GetAce));
    BYTE A8 buf[64]; LPVOID ace = (LPVOID)0xABCD;
    CHECK(!mk_acl(buf, sizeof buf, ACL_REVISION, 0));
    CHECK(!U(GetAce)((PACL)buf, 0, &ace));
    t_note("output after failed GetAce(empty) = %p", ace);
}

TEST(gap4_getace_bad_revisions) {
    REQUIRE(U(GetAce));
    BYTE A8 buf[128]; LPVOID ace;
    static const BYTE bad[] = { 0, 1, 5, 9, 0xFF };
    for (int i = 0; i < 5; i++) {
        CHECK(!mk_acl(buf, sizeof buf, ACL_REVISION, 2));
        buf[0] = bad[i];
        FAILB(U(GetAce)((PACL)buf, 0, &ace), ERROR_INVALID_PARAMETER);
    }
}

TEST(gap4_getace_all_valid_revisions) {
    REQUIRE(U(GetAce));
    BYTE A8 buf[128]; LPVOID ace;
    for (BYTE r = 2; r <= 4; r++) {
        CHECK(!mk_acl(buf, sizeof buf, ACL_REVISION, 2));
        buf[0] = r;
        CHECK(U(GetAce)((PACL)buf, 1, &ace));
        EXPECT_EQ(ace, buf + 28);
    }
}

TEST(gap4_getace_acl_size_truncation) {
    REQUIRE(U(GetAce));
    BYTE A8 buf[128]; LPVOID ace;
    CHECK(!mk_acl(buf, sizeof buf, ACL_REVISION, 2));
    ((PACL)buf)->AclSize = 8 + 20;     /* second ACE lies past the declared end */
    CHECK(U(GetAce)((PACL)buf, 0, &ace));
    FAILB(U(GetAce)((PACL)buf, 1, &ace), ERROR_INVALID_PARAMETER);
}

TEST(gap4_getace_acl_size_exactly_covers) {
    REQUIRE(U(GetAce));
    BYTE A8 buf[128]; LPVOID ace;
    CHECK(!mk_acl(buf, sizeof buf, ACL_REVISION, 2));
    ((PACL)buf)->AclSize = 8 + 20 + 20;
    CHECK(U(GetAce)((PACL)buf, 1, &ace));
    EXPECT_EQ(ace, buf + 28);
    FAILB(U(GetAce)((PACL)buf, 2, &ace), ERROR_INVALID_PARAMETER);
}

TEST(gap4_getace_count_exceeds_real) {
    REQUIRE(U(GetAce));
    BYTE A8 buf[128]; LPVOID ace;
    CHECK(!mk_acl(buf, 8 + 20 + 20, ACL_REVISION, 2));
    ((PACL)buf)->AceCount = 5;          /* claims more ACEs than the buffer holds */
    CHECK(U(GetAce)((PACL)buf, 1, &ace));
    FAILB(U(GetAce)((PACL)buf, 2, &ace), ERROR_INVALID_PARAMETER);
    FAILB(U(GetAce)((PACL)buf, 4, &ace), ERROR_INVALID_PARAMETER);
}

TEST(gap4_getace_pointer_is_inside_acl) {
    REQUIRE(U(GetAce)); REQUIRE(U(GetAclInformation));
    BYTE A8 buf[256]; ACL_SIZE_INFORMATION si; LPVOID ace;
    CHECK(!mk_acl(buf, sizeof buf, ACL_REVISION, 3));
    CHECK(U(GetAclInformation)((PACL)buf, &si, sizeof si, AclSizeInformation));
    for (DWORD i = 0; i < si.AceCount; i++) {
        CHECK(U(GetAce)((PACL)buf, i, &ace));
        CHECK((BYTE *)ace >= buf + 8 && (BYTE *)ace < buf + si.AclBytesInUse);
    }
}

TEST(gap4_getace_walk_sums_to_inuse) {
    REQUIRE(U(GetAce)); REQUIRE(U(GetAclInformation));
    BYTE A8 buf[256]; ACL_SIZE_INFORMATION si; ACE_HEADER *h; DWORD sum = 8;
    CHECK(!mk_acl(buf, sizeof buf, ACL_REVISION, 3));
    CHECK(U(GetAclInformation)((PACL)buf, &si, sizeof si, AclSizeInformation));
    for (DWORD i = 0; i < si.AceCount; i++) { CHECK(U(GetAce)((PACL)buf, i, (LPVOID *)&h)); sum += h->AceSize; }
    EXPECT_EQ(sum, si.AclBytesInUse);
}

TEST(gap4_getace_mandatory_label_ace) {
    REQUIRE(U(GetAce)); REQUIRE(U(AddMandatoryAce));
    BYTE A8 buf[128]; ACE_HEADER *h;
    BOOL (WINAPI *init)(PACL, DWORD, DWORD) = (void *)U(InitializeAcl);
    CHECK(init((PACL)buf, sizeof buf, ACL_REVISION));
    SET_ERR(0xBAD);
    BOOL r = U(AddMandatoryAce)((PACL)buf, ACL_REVISION, 0, SYSTEM_MANDATORY_LABEL_NO_WRITE_UP, (PSID)SID_LABEL);
    CHECKF(r, "err=%lu", GetLastError());
    if (!r) return;
    CHECK(U(GetAce)((PACL)buf, 0, (LPVOID *)&h));
    EXPECT_EQ(h->AceType, SYSTEM_MANDATORY_LABEL_ACE_TYPE);
    EXPECT_EQ(((SYSTEM_MANDATORY_LABEL_ACE *)h)->Mask, SYSTEM_MANDATORY_LABEL_NO_WRITE_UP);
}

TEST(gap4_getace_ex_flags_preserved) {
    REQUIRE(U(GetAce)); REQUIRE(U(AddAccessAllowedAceEx));
    BYTE A8 buf[128]; ACE_HEADER *h;
    BOOL (WINAPI *init)(PACL, DWORD, DWORD) = (void *)U(InitializeAcl);
    CHECK(init((PACL)buf, sizeof buf, ACL_REVISION));
    CHECK(U(AddAccessAllowedAceEx)((PACL)buf, ACL_REVISION, OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE | INHERIT_ONLY_ACE, GENERIC_READ, (PSID)SID_WORLD));
    CHECK(U(GetAce)((PACL)buf, 0, (LPVOID *)&h));
    EXPECT_EQ(h->AceFlags, OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE | INHERIT_ONLY_ACE);
}

TEST(gap4_getace_after_delete_ace) {
    REQUIRE(U(GetAce)); REQUIRE(U(DeleteAce)); REQUIRE(U(GetAclInformation));
    BYTE A8 buf[256]; ACL_SIZE_INFORMATION si; ACE_HEADER *h; LPVOID ace;
    CHECK(!mk_acl(buf, sizeof buf, ACL_REVISION, 3));
    CHECK(U(DeleteAce)((PACL)buf, 0));
    CHECK(U(GetAclInformation)((PACL)buf, &si, sizeof si, AclSizeInformation));
    EXPECT_EQ(si.AceCount, 2);
    EXPECT_EQ(si.AclBytesInUse, 8 + 20 + 24);
    CHECK(U(GetAce)((PACL)buf, 0, (LPVOID *)&h));
    EXPECT_EQ(h->AceType, ACCESS_DENIED_ACE_TYPE);
    FAILB(U(GetAce)((PACL)buf, 2, &ace), ERROR_INVALID_PARAMETER);
}

TEST(gap4_getace_repeat_calls_stable) {
    REQUIRE(U(GetAce));
    BYTE A8 buf[256]; LPVOID a, b;
    CHECK(!mk_acl(buf, sizeof buf, ACL_REVISION, 3));
    for (int i = 0; i < 1000; i++) {
        CHECK(U(GetAce)((PACL)buf, 2, &a));
        CHECK(U(GetAce)((PACL)buf, 2, &b));
        if (a != b) { CHECK(a == b); break; }
    }
}

TEST(gap4_getace_many_aces) {
    REQUIRE(U(GetAce)); REQUIRE(U(GetAclInformation));
    BYTE A8 buf[4096]; ACL_SIZE_INFORMATION si; ACE_HEADER *h;
    BOOL (WINAPI *init)(PACL, DWORD, DWORD) = (void *)U(InitializeAcl);
    BOOL (WINAPI *allow)(PACL, DWORD, DWORD, PSID) = (void *)U(AddAccessAllowedAce);
    REQUIRE(allow);
    CHECK(init((PACL)buf, sizeof buf, ACL_REVISION));
    for (int i = 0; i < 150; i++) if (!allow((PACL)buf, ACL_REVISION, 0x100 + i, (PSID)SID_WORLD)) { CHECKF(0, "add %d err=%lu", i, GetLastError()); break; }
    CHECK(U(GetAclInformation)((PACL)buf, &si, sizeof si, AclSizeInformation));
    EXPECT_EQ(si.AceCount, 150);
    for (DWORD i = 0; i < 150; i++) {
        CHECK(U(GetAce)((PACL)buf, i, (LPVOID *)&h));
        if (h) EXPECT_EQ(((ACCESS_ALLOWED_ACE *)h)->Mask, 0x100 + i);
    }
    LPVOID ace;
    FAILB(U(GetAce)((PACL)buf, 150, &ace), ERROR_INVALID_PARAMETER);
}

TEST(gap4_getace_forwarded_from_kernel32) {
    /* GetAce / GetAclInformation are kernelbase implementations; kernel32 may re-export them */
    FARPROC k = t_proc_k32("GetAce"), b = t_proc_kb("GetAce");
    REQUIRE(b);
    t_note("GetAce kernel32 export %s", k ? "present" : "absent");
    k = t_proc_k32("GetAclInformation");
    t_note("GetAclInformation kernel32 export %s", k ? "present" : "absent");
}

/* ---------------- real DACLs ---------------- */

TEST(gap4_realdacl_of_temp_file) {
    REQUIRE(U(GetAce)); REQUIRE(U(GetAclInformation)); REQUIRE(U(GetFileSecurityW));
    REQUIRE(U(GetSecurityDescriptorDacl));
    char path[MAX_PATH]; WCHAR wp[MAX_PATH]; t_tmpfile(path, sizeof path, "dacl.txt");
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE); if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    MultiByteToWideChar(CP_ACP, 0, path, -1, wp, MAX_PATH);
    BYTE sd[4096]; DWORD need = 0;
    CHECK(U(GetFileSecurityW)(wp, DACL_SECURITY_INFORMATION, sd, sizeof sd, &need));
    BOOL present = FALSE, deflt = FALSE; PACL dacl = NULL;
    CHECK(U(GetSecurityDescriptorDacl)(sd, &present, &dacl, &deflt));
    if (!present || !dacl) { t_note("no DACL present"); return; }
    ACL_SIZE_INFORMATION si;
    CHECK(U(GetAclInformation)(dacl, &si, sizeof si, AclSizeInformation));
    CHECK(si.AceCount >= 1);
    for (DWORD i = 0; i < si.AceCount; i++) {
        ACE_HEADER *ah = NULL;
        CHECK(U(GetAce)(dacl, i, (LPVOID *)&ah));
        if (!ah) break;
        CHECK(ah->AceType == ACCESS_ALLOWED_ACE_TYPE || ah->AceType == ACCESS_DENIED_ACE_TYPE || ah->AceType < 0x20);
        CHECK(ah->AceSize >= 8);
    }
    ACL_REVISION_INFORMATION ri;
    CHECK(U(GetAclInformation)(dacl, &ri, sizeof ri, AclRevisionInformation));
    CHECK(ri.AclRevision >= 2 && ri.AclRevision <= 4);
    DeleteFileA(path);
}
