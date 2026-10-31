#ifndef PMC_H
#define PMC_H
#include "t.h"

/* unique pipe / mailslot names */
void pmc_pipename(char *out, size_t cap);
void pmc_pipenamew(WCHAR *out, size_t cap);
void pmc_slotname(char *out, size_t cap);
void pmc_slotnamew(WCHAR *out, size_t cap);
/* server end via CreateNamedPipeA (through GetProcAddress); INVALID_HANDLE_VALUE if export missing */
HANDLE pmc_server(const char *name, DWORD open, DWORD mode, DWORD max, DWORD ob, DWORD ib);
/* client end via plain CreateFileA (scaffolding) */
HANDLE pmc_client(const char *name, DWORD access, DWORD flags);
/* simple helpers */
HANDLE pmc_thread(LPTHREAD_START_ROUTINE fn, void *arg);
void pmc_join(HANDLE th);
DWORD pmc_sum(const unsigned char *p, size_t n, DWORD seed);
void pmc_fill(unsigned char *p, size_t n, DWORD seed);
BOOL pmc_err_in(DWORD e, ...);   /* 0-terminated list */
#define CHECK_ERR_ONE_OF2(a, b) do { DWORD e_ = GetLastError(); t_check(e_ == (a) || e_ == (b), __FILE__, __LINE__, "err in {" #a "," #b "}", "got %lu", e_); } while (0)
#define CHECK_ERR_ONE_OF3(a, b, c) do { DWORD e_ = GetLastError(); t_check(e_ == (a) || e_ == (b) || e_ == (c), __FILE__, __LINE__, "err in {" #a "," #b "," #c "}", "got %lu", e_); } while (0)

#define CHECK_ERR_ONE_OF4(a, b, c, d) do { DWORD e_ = GetLastError(); t_check(e_ == (a) || e_ == (b) || e_ == (c) || e_ == (d), __FILE__, __LINE__, "err in {" #a "," #b "," #c "," #d "}", "got %lu", e_); } while (0)
#ifndef ERROR_PIPE_LISTENING
#define ERROR_PIPE_LISTENING 536
#endif
#ifndef ERROR_PIPE_CONNECTED
#define ERROR_PIPE_CONNECTED 535
#endif
#ifndef ERROR_NO_DATA
#define ERROR_NO_DATA 232
#endif
#ifndef MAILSLOT_NO_MESSAGE
#define MAILSLOT_NO_MESSAGE ((DWORD)-1)
#endif
#ifndef MAILSLOT_WAIT_FOREVER
#define MAILSLOT_WAIT_FOREVER ((DWORD)-1)
#endif
#ifndef PIPE_REJECT_REMOTE_CLIENTS
#define PIPE_ACCEPT_REMOTE_CLIENTS 0
#define PIPE_REJECT_REMOTE_CLIENTS 8
#endif
#ifndef FILE_FLAG_FIRST_PIPE_INSTANCE
#define FILE_FLAG_FIRST_PIPE_INSTANCE 0x00080000
#endif
UDECL(ULONG, GetCommPorts, (PULONG, ULONG, PULONG));
UDECL(HANDLE, OpenCommPort, (ULONG, DWORD, DWORD));
#endif
