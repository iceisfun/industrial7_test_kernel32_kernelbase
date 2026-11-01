/* helpers for the gap_02 area (static, header-only) */
#ifndef G2_H
#define G2_H
#include "t.h"
#include <psapi.h>
#include <sddl.h>

#ifndef SEF_DACL_AUTO_INHERIT
#define SEF_DACL_AUTO_INHERIT 0x01
#endif
#ifndef SEF_SACL_AUTO_INHERIT
#define SEF_SACL_AUTO_INHERIT 0x02
#endif
#ifndef SEF_DEFAULT_DESCRIPTOR_FOR_OBJECT
#define SEF_DEFAULT_DESCRIPTOR_FOR_OBJECT 0x04
#endif
#ifndef SEF_AVOID_PRIVILEGE_CHECK
#define SEF_AVOID_PRIVILEGE_CHECK 0x08
#endif
#ifndef SEF_AVOID_OWNER_CHECK
#define SEF_AVOID_OWNER_CHECK 0x10
#endif
#ifndef SEF_DEFAULT_OWNER_FROM_PARENT
#define SEF_DEFAULT_OWNER_FROM_PARENT 0x20
#endif
#ifndef SEF_DEFAULT_GROUP_FROM_PARENT
#define SEF_DEFAULT_GROUP_FROM_PARENT 0x40
#endif

/* S-1-... text form of a SID (own implementation, no advapi32 call). */
static inline void g2_sid_str(PSID sid, char *out, size_t cap)
{
    SID *s = (SID *)sid;
    unsigned long long auth = 0;
    int i;
    char tmp[256];
    size_t n = 0;
    for (i = 0; i < 6; i++) auth = (auth << 8) | s->IdentifierAuthority.Value[i];
    n += snprintf(tmp + n, sizeof tmp - n, "S-%u-%llu", (unsigned)s->Revision, auth);
    for (i = 0; i < s->SubAuthorityCount && n < sizeof tmp - 16; i++)
        n += snprintf(tmp + n, sizeof tmp - n, "-%lu", (unsigned long)s->SubAuthority[i]);
    snprintf(out, cap, "%s", tmp);
}

/* Build a SID in buf: authority a (last byte), n subauthorities. Returns buf. */
static inline PSID g2_mksid(void *buf, BYTE authority, int n, ...)
{
    SID *s = (SID *)buf;
    va_list ap;
    int i;
    memset(buf, 0, 8 + 4 * n);
    s->Revision = 1;
    s->SubAuthorityCount = (BYTE)n;
    s->IdentifierAuthority.Value[5] = authority;
    va_start(ap, n);
    for (i = 0; i < n; i++) s->SubAuthority[i] = va_arg(ap, DWORD);
    va_end(ap);
    return buf;
}

static inline HANDLE g2_open_proc_token(DWORD access)
{
    HANDLE h = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), access, &h)) return NULL;
    return h;
}

/* GrantedAccess of a handle via NtQueryObject(ObjectBasicInformation). */
static inline DWORD g2_granted(HANDLE h)
{
    typedef LONG (NTAPI *pfn)(HANDLE, int, void *, ULONG, ULONG *);
    pfn q = (pfn)t_proc_nt("NtQueryObject");
    ULONG buf[14]; ULONG ret = 0;
    memset(buf, 0, sizeof buf);
    /* ObjectBasicInformation: the kernel requires Length == sizeof(OBJECT_BASIC_INFORMATION) == 0x38 exactly */
    if (!q || q(h, 0, buf, 0x38, &ret) < 0) return 0xFFFFFFFF;
    return buf[1];   /* Attributes, GrantedAccess */
}

/* Find first ACE in acl whose mask == mask. Returns pointer to the ACE header or NULL. */
static inline ACE_HEADER *g2_find_ace(PACL acl, DWORD mask)
{
    DWORD i;
    ACL_SIZE_INFORMATION si;
    if (!acl) return NULL;
    if (!GetAclInformation(acl, &si, sizeof si, AclSizeInformation)) return NULL;
    for (i = 0; i < si.AceCount; i++) {
        void *ace;
        if (!GetAce(acl, i, &ace)) return NULL;
        if (((ACCESS_ALLOWED_ACE *)ace)->Mask == mask) return (ACE_HEADER *)ace;
    }
    return NULL;
}
static inline PSID g2_ace_sid(ACE_HEADER *a) { return (PSID)&((ACCESS_ALLOWED_ACE *)a)->SidStart; }
static inline DWORD g2_ace_count(PACL acl)
{
    ACL_SIZE_INFORMATION si;
    if (!acl || !GetAclInformation(acl, &si, sizeof si, AclSizeInformation)) return 0xFFFFFFFF;
    return si.AceCount;
}
#endif
