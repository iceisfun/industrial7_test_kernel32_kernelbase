/* Declarations missing from the mingw headers (19H1+ appmodel additions, constants). */
#ifndef AM_COMPAT_H
#define AM_COMPAT_H
#ifndef PACKAGE_FULL_NAME_MAX_LENGTH
#define PACKAGE_FULL_NAME_MAX_LENGTH 127
#define PACKAGE_FAMILY_NAME_MAX_LENGTH 64
#define PACKAGE_NAME_MAX_LENGTH 50
#define PACKAGE_PUBLISHERID_MAX_LENGTH 13
#define PACKAGE_RESOURCEID_MAX_LENGTH 30
#define PACKAGE_RELATIVE_APPLICATION_ID_MAX_LENGTH 64
#define APPLICATION_USER_MODEL_ID_MAX_LENGTH 130
#endif
#ifndef PackagePathType_Install
#define PackagePathType_Install 0
#define PackagePathType_Mutable 1
#define PackagePathType_Effective 2
#endif
typedef int am_PackagePathType;
UDECL(LONG, GetCurrentPackagePath2, (int type, UINT32 *len, PWSTR path));
UDECL(LONG, GetCurrentPackageInfo2, (UINT32 flags, int type, UINT32 *len, BYTE *buf, UINT32 *count));
UDECL(LONG, GetPackagePathByFullName2, (PCWSTR name, int type, UINT32 *len, PWSTR path));
UDECL(LONG, GetStagedPackagePathByFullName2, (PCWSTR name, int type, UINT32 *len, PWSTR path));
UDECL(UINT32, GetPackageGraphRevisionId, (void));
UDECL(void *, GetCurrentPackageVirtualizationContext, (void));
UDECL(LONG, GetPackageApplicationIds, (void *ref, UINT32 *len, BYTE *buf, UINT32 *count));
UDECL(LONG, GetPackageInfo, (void *ref, UINT32 flags, UINT32 *len, BYTE *buf, UINT32 *count));
UDECL(LONG, ClosePackageInfo, (void *ref));
UDECL(LONG, OpenPackageInfoByFullName, (PCWSTR name, UINT32 reserved, void **ref));
/* Evaluate the call once (EXPECT_EQ evaluates its arguments twice). */
#define EQ(call, want) do { ULONG eq_r_ = (ULONG)(call); t_check(eq_r_ == (ULONG)(want), __FILE__, __LINE__, #call " == " #want, "got 0x%lx want 0x%lx", (unsigned long)eq_r_, (unsigned long)(ULONG)(want)); } while (0)
#endif
