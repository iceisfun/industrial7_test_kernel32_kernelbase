#ifndef G16_H
#define G16_H
#include "t.h"
#include <winver.h>

/* Own typedefs so the tests do not depend on header coverage. */
typedef PVOID (WINAPI *g16_VirtualAlloc2_t)(HANDLE, PVOID, SIZE_T, ULONG, ULONG, void *, ULONG);
typedef struct { ULONG64 Type; union { PVOID Pointer; ULONG64 U64; SIZE_T Size; HANDLE Handle; ULONG U32; } u; } g16_mep;
typedef struct { PVOID Lowest; PVOID Highest; SIZE_T Alignment; } g16_addrreq;
#define G16_MEP_ADDRREQ 1
#define G16_MEP_NUMA 2

UDECL(HRESULT, WerRegisterFile, (PCWSTR, int, DWORD));
UDECL(HRESULT, WerUnregisterFile, (PCWSTR));
UDECL(HRESULT, WerRegisterMemoryBlock, (PVOID, DWORD));
UDECL(HRESULT, WerUnregisterMemoryBlock, (PVOID));
UDECL(HRESULT, WerRegisterExcludedMemoryBlock, (const void *, DWORD));
UDECL(HRESULT, WerUnregisterExcludedMemoryBlock, (const void *));
UDECL(HRESULT, WerRegisterAdditionalProcess, (DWORD, DWORD));
UDECL(HRESULT, WerUnregisterAdditionalProcess, (DWORD));
UDECL(HRESULT, WerRegisterCustomMetadata, (PCWSTR, PCWSTR));
UDECL(HRESULT, WerUnregisterCustomMetadata, (PCWSTR));
UDECL(HRESULT, WerRegisterAppLocalDump, (PCWSTR));
UDECL(HRESULT, WerUnregisterAppLocalDump, (void));
UDECL(HRESULT, WerRegisterRuntimeExceptionModule, (PCWSTR, PVOID));
UDECL(HRESULT, WerUnregisterRuntimeExceptionModule, (PCWSTR, PVOID));
UDECL(BOOL, VerQueryValueW, (LPCVOID, LPCWSTR, LPVOID *, PUINT));
UDECL(BOOL, GetFileVersionInfoW, (LPCWSTR, DWORD, DWORD, LPVOID));
UDECL(DWORD, GetFileVersionInfoSizeW, (LPCWSTR, LPDWORD));
UDECL(DWORD, WriteTapemark, (HANDLE, DWORD, DWORD, BOOL));
UDECL(ULONGLONG, VerSetConditionMask, (ULONGLONG, DWORD, BYTE));

#define EXPECT_ERR2(a, b) do { DWORD e_ = GetLastError(); \
    t_check(e_ == (DWORD)(a) || e_ == (DWORD)(b), __FILE__, __LINE__, #a " or " #b, "GetLastError got %lu", e_); } while (0)
static int kc_lines[64], kc_n;
#define KC(c) do { if (!(c)) { if (kc_n < 64) kc_lines[kc_n++] = __LINE__; if (!kc_bad) kc_bad = __LINE__; } } while (0)
static void kc_dump(void) { printf("\nKCFAILS:"); for (int i = 0; i < kc_n; i++) printf(" %d", kc_lines[i]); printf("\n"); }
#define G16_E_INVALIDARG ((HRESULT)0x80070057)
#define G16_E_NOTFOUND   ((HRESULT)0x80070490)
#endif
