/* gap_12: security descriptor scaffolding (static helpers only; the Set* calls under test go through U()). */
#ifndef SDH_H
#define SDH_H
#include "t.h"
#include <sddl.h>
#include <aclapi.h>

#ifndef SE_DACL_UNTRUSTED
#define SE_DACL_UNTRUSTED 0x0040
#define SE_SERVER_SECURITY 0x0080
#endif

static BYTE s_world[SECURITY_MAX_SID_SIZE];
static BYTE s_admin[SECURITY_MAX_SID_SIZE];
static BYTE s_user[SECURITY_MAX_SID_SIZE];

static PSID sid_world(void) { DWORD n = sizeof s_world; CreateWellKnownSid(WinWorldSid, NULL, s_world, &n); return s_world; }
static PSID sid_admin(void) { DWORD n = sizeof s_admin; CreateWellKnownSid(WinBuiltinAdministratorsSid, NULL, s_admin, &n); return s_admin; }
static PSID sid_user(void) {
    HANDLE t; BYTE buf[256]; DWORD n = 0;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &t)) return sid_world();
    if (GetTokenInformation(t, TokenUser, buf, sizeof buf, &n)) CopySid(sizeof s_user, s_user, ((TOKEN_USER *)buf)->User.Sid);
    CloseHandle(t);
    return s_user;
}
/* ACL with one allow ACE */
static PACL mkacl(void *buf, DWORD cap, PSID sid, DWORD mask) {
    PACL a = (PACL)buf;
    if (!InitializeAcl(a, cap, ACL_REVISION)) return NULL;
    if (!AddAccessAllowedAce(a, ACL_REVISION, mask, sid)) return NULL;
    return a;
}
static PACL mkacl_empty(void *buf, DWORD cap) {
    PACL a = (PACL)buf;
    return InitializeAcl(a, cap, ACL_REVISION) ? a : NULL;
}
static void sd_init(SECURITY_DESCRIPTOR *sd) { memset(sd, 0xCC, sizeof *sd); InitializeSecurityDescriptor(sd, SECURITY_DESCRIPTOR_REVISION); }
static WORD sd_ctl(void *sd) { return ((SECURITY_DESCRIPTOR *)sd)->Control; }

/* first allow-ACE of the DACL of an SD (absolute or self-relative): returns ace count, fills sid/mask */
static int sd_dacl_info(PSECURITY_DESCRIPTOR sd, int *present, PSID *sid, DWORD *mask, BOOL *nulldacl) {
    PACL acl = NULL; BOOL p = FALSE, d = FALSE; ACL_SIZE_INFORMATION si;
    if (!GetSecurityDescriptorDacl(sd, &p, &acl, &d)) return -1;
    *present = p;
    if (nulldacl) *nulldacl = (p && !acl);
    if (!p || !acl) return 0;
    if (!GetAclInformation(acl, &si, sizeof si, AclSizeInformation)) return -1;
    if (si.AceCount) {
        ACCESS_ALLOWED_ACE *ace = NULL;
        if (GetAce(acl, 0, (LPVOID *)&ace)) { if (sid) *sid = (PSID)&ace->SidStart; if (mask) *mask = ace->Mask; }
    }
    return (int)si.AceCount;
}
#endif
