/* Shared static helpers for the volume_drive area. */
#ifndef VD_H
#define VD_H
#include "t.h"
#include <ctype.h>

#define VD_ERR_INVALID_NAME 123
#define VD_GUID_LEN 49

/* DISK_SPACE_INFORMATION (Win10 2004+), declared locally. */
typedef struct {
    ULONGLONG ActualTotalAllocationUnits, ActualAvailableAllocationUnits, ActualPoolUnavailableAllocationUnits;
    ULONGLONG CallerTotalAllocationUnits, CallerAvailableAllocationUnits, CallerPoolUnavailableAllocationUnits;
    ULONGLONG UsedAllocationUnits, TotalReservedAllocationUnits, VolumeStorageReserveAllocationUnits;
    ULONGLONG AvailableCommittedAllocationUnits, PoolAvailableAllocationUnits;
    DWORD SectorsPerAllocationUnit, BytesPerSector;
} VD_DSI;
UDECL(LONG, GetDiskSpaceInformationW, (LPCWSTR, VD_DSI *));
UDECL(LONG, GetDiskSpaceInformationA, (LPCSTR, VD_DSI *));

/* ---- scaffolding helpers ---- */
static char vd_sysletter(void) {
    char b[MAX_PATH]; UINT n = GetSystemDirectoryA(b, sizeof b);
    return (n >= 2 && b[1] == ':') ? (char)toupper((unsigned char)b[0]) : 'C';
}
static void vd_sysroot(char *out /* >=4 */) { out[0] = vd_sysletter(); out[1] = ':'; out[2] = '\\'; out[3] = 0; }
static char vd_tmpletter(void) { const char *t = t_tmpdir(); return (t[1] == ':') ? (char)toupper((unsigned char)t[0]) : 0; }
static void vd_tmproot(char *out) { out[0] = vd_tmpletter(); out[1] = ':'; out[2] = '\\'; out[3] = 0; }
static char vd_freeletter(void) {
    DWORD m = GetLogicalDrives();
    for (char c = 'Z'; c >= 'D'; c--) if (!((m >> (c - 'A')) & 1)) return c;
    return 0;
}
static void vd_widen(const char *a, WCHAR *w, int cap) { MultiByteToWideChar(CP_ACP, 0, a, -1, w, cap); }
static void vd_narrow(const WCHAR *w, char *a, int cap) { WideCharToMultiByte(CP_ACP, 0, w, -1, a, cap, NULL, NULL); }

/* Volume GUID name ("\\?\Volume{...}\") for a drive root, via the function under test. */
static int vd_volname(const char *root, char *out /* >=64 */) {
    __typeof__(&GetVolumeNameForVolumeMountPointA) f = U(GetVolumeNameForVolumeMountPointA);
    if (!f) return 0;
    return f(root, out, 64) != 0;
}
/* "\\?\Volume{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}\" shape check. */
static int vd_is_guid_name_a(const char *s) {
    static const char pat[] = "\\\\?\\Volume{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}\\";
    if (strlen(s) != VD_GUID_LEN) return 0;
    for (int i = 0; i < VD_GUID_LEN; i++) {
        if (pat[i] == 'x') { if (!isxdigit((unsigned char)s[i])) return 0; }
        else if (s[i] != pat[i]) return 0;
    }
    return 1;
}
static int vd_is_guid_name_w(const WCHAR *w) {
    char a[128]; int i;
    for (i = 0; w[i] && i < 120; i++) { if (w[i] > 127) return 0; a[i] = (char)w[i]; }
    a[i] = 0; return vd_is_guid_name_a(a);
}
static int vd_known_fs(const char *fs) {
    return !strcmp(fs, "NTFS") || !strcmp(fs, "FAT32") || !strcmp(fs, "FAT") || !strcmp(fs, "exFAT") ||
           !strcmp(fs, "ReFS") || !strcmp(fs, "CDFS") || !strcmp(fs, "UDF") || !strcmp(fs, "FAT16");
}
static int vd_stricmp(const char *a, const char *b) { return lstrcmpiA(a, b); }
static int vd_pow2(DWORD v) { return v && !(v & (v - 1)); }
/* Letters (as bit index) of fixed drives. */
static DWORD vd_fixed_mask(void) {
    __typeof__(&GetDriveTypeA) f = U(GetDriveTypeA);
    DWORD m = GetLogicalDrives(), r = 0;
    if (!f) return 0;
    for (int i = 0; i < 26; i++) if ((m >> i) & 1) { char root[4] = { (char)('A' + i), ':', '\\', 0 }; if (f(root) == DRIVE_FIXED) r |= 1u << i; }
    return r;
}
#endif
