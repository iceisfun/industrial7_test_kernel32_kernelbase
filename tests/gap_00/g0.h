/* shared helpers for the gap_00 area (static, header-only) */
#ifndef G0_H
#define G0_H
#include "t.h"
#include <aclapi.h>
#include <sddl.h>
#include <appmodel.h>

typedef BOOL (*g0_adder_fn)(PACL, DWORD rev, DWORD flags, DWORD mask, PSID sid);

/* raw SIDs (authority is big-endian in the byte image) */
static BYTE g0_sid_world[12]  = {1,1,0,0,0,0,0,1, 0,0,0,0};
static BYTE g0_sid_admins[16] = {1,2,0,0,0,0,0,5, 32,0,0,0, 32,2,0,0};
static BYTE g0_sid_fake[28]   = {1,5,0,0,0,0,0,5, 21,0,0,0, 1,0,0,0, 2,0,0,0, 3,0,0,0, 0xE8,3,0,0};
static BYTE g0_sid_low[12]    = {1,1,0,0,0,0,0,16, 0,0x10,0,0};   /* S-1-16-4096 */
static BYTE g0_sid_scoped[12] = {1,1,0,0,0,0,0,17, 1,0,0,0};      /* S-1-17-1 */
static BYTE g0_sid_badrev[12] = {2,1,0,0,0,0,0,1, 0,0,0,0};       /* revision 2: invalid */
static BYTE g0_sid_bigcnt[72] = {1,16,0,0,0,0,0,5};               /* 16 subauthorities: invalid */

static const GUID g0_guid_a = {0x11111111,0x2222,0x3333,{1,2,3,4,5,6,7,8}};
static const GUID g0_guid_b = {0xaaaaaaaa,0xbbbb,0xcccc,{8,7,6,5,4,3,2,1}};

#define G0_SID(x) ((PSID)(x))

static PACL g0_acl(void *buf, DWORD size, DWORD rev) {
    memset(buf, 0xCC, size);
    if (!InitializeAcl((PACL)buf, size, rev)) return NULL;
    return (PACL)buf;
}
static ACE_HEADER *g0_ace(PACL a, DWORD i) {
    LPVOID p = NULL;
    if (!GetAce(a, i, &p)) return NULL;
    return (ACE_HEADER *)p;
}
static DWORD g0_cnt(PACL a) { return a->AceCount; }

/* build a classic ACE image: type, flags, mask, sid; returns size */
static DWORD g0_mkace(BYTE *out, BYTE type, BYTE flags, DWORD mask, PSID sid) {
    DWORD sl = GetLengthSid(sid);
    ACE_HEADER *h = (ACE_HEADER *)out;
    h->AceType = type; h->AceFlags = flags; h->AceSize = (WORD)(8 + sl);
    *(DWORD *)(out + 4) = mask;
    memcpy(out + 8, sid, sl);
    return 8 + sl;
}
static DWORD g0_ace_mask(ACE_HEADER *h) { return *(DWORD *)((BYTE *)h + 4); }
static PSID g0_ace_sid(ACE_HEADER *h) { return (PSID)((BYTE *)h + 8); }

#endif
