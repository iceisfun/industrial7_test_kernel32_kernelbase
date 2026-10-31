#include "pmc.h"
static volatile LONG g_ctr;
void pmc_pipename(char *out, size_t cap) { snprintf(out, cap, "\\\\.\\pipe\\k32t_%lu_%ld", GetCurrentProcessId(), (long)InterlockedIncrement(&g_ctr)); }
void pmc_pipenamew(WCHAR *out, size_t cap) { char a[128]; pmc_pipename(a, sizeof a); MultiByteToWideChar(CP_ACP, 0, a, -1, out, (int)cap); }
void pmc_slotname(char *out, size_t cap) { snprintf(out, cap, "\\\\.\\mailslot\\k32t_%lu_%ld", GetCurrentProcessId(), (long)InterlockedIncrement(&g_ctr)); }
void pmc_slotnamew(WCHAR *out, size_t cap) { char a[128]; pmc_slotname(a, sizeof a); MultiByteToWideChar(CP_ACP, 0, a, -1, out, (int)cap); }
HANDLE pmc_server(const char *name, DWORD open, DWORD mode, DWORD max, DWORD ob, DWORD ib) {
    __typeof__(&CreateNamedPipeA) f = U(CreateNamedPipeA);
    if (!f) return INVALID_HANDLE_VALUE;
    return f(name, open, mode, max, ob, ib, 0, NULL);
}
HANDLE pmc_client(const char *name, DWORD access, DWORD flags) {
    return CreateFileA(name, access, 0, NULL, OPEN_EXISTING, flags, NULL);
}
HANDLE pmc_thread(LPTHREAD_START_ROUTINE fn, void *arg) { return CreateThread(NULL, 0, fn, arg, 0, NULL); }
void pmc_join(HANDLE th) { if (!th) return; if (WaitForSingleObject(th, 20000) != WAIT_OBJECT_0) TerminateThread(th, 0xBAD); CloseHandle(th); }
DWORD pmc_sum(const unsigned char *p, size_t n, DWORD seed) { DWORD h = seed ? seed : 2166136261u; for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; } return h; }
void pmc_fill(unsigned char *p, size_t n, DWORD seed) { DWORD x = seed * 2654435761u + 12345; for (size_t i = 0; i < n; i++) { x = x * 1103515245u + 12345; p[i] = (unsigned char)(x >> 16); } }
BOOL pmc_err_in(DWORD e, ...) { va_list a; va_start(a, e); for (;;) { DWORD v = va_arg(a, DWORD); if (!v) break; if (v == e) { va_end(a); return TRUE; } } va_end(a); return FALSE; }
