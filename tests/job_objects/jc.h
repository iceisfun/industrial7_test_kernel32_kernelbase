#ifndef JC_H
#define JC_H
#include "t.h"
#include <stdlib.h>
#pragma GCC diagnostic ignored "-Wmisleading-indentation"

/* Local helpers for the job_objects area. Calls under test go through U()/UK(). */
#define JO_CREATE_SUSPENDED 0x4
#define JC_TIMEOUT 0xDEAD0001u

/* Create an unnamed job through the pointer under test. */
HANDLE jc_job(void);
/* Spawn this exe as child CHILD(name) suspended, not inherited, new process group not used. */
HANDLE jc_spawn(const char *name, const char *args, DWORD extra_flags, PROCESS_INFORMATION *pi);
/* Spawn suspended, assign to job, resume. Returns TRUE on success (pi filled); on failure the process is killed. */
BOOL jc_spawn_in_job(HANDLE job, const char *name, const char *args, PROCESS_INFORMATION *pi);
/* Kill (if running) and close handles in pi. */
void jc_kill(PROCESS_INFORMATION *pi);
/* Wait for exit; returns exit code or JC_TIMEOUT. */
DWORD jc_wait_exit(HANDLE hproc, DWORD ms);
/* Query basic accounting for job; returns TRUE on success. */
BOOL jc_acct(HANDLE job, JOBOBJECT_BASIC_ACCOUNTING_INFORMATION *a);
/* Poll until ActiveProcesses == n (<= ms). Returns TRUE if reached. */
BOOL jc_wait_active(HANDLE job, DWORD n, DWORD ms);
/* Poll until TotalProcesses >= n. */
BOOL jc_wait_total(HANDLE job, DWORD n, DWORD ms);
/* Process ids in a job (max cap); returns count in list or -1. *assigned gets NumberOfAssignedProcesses. */
int jc_pids(HANDLE job, DWORD *out, int cap, DWORD *assigned);
int jc_pid_in(HANDLE job, DWORD pid);
/* Set the extended limit flags (and others) on a job. */
BOOL jc_set_ext(HANDLE job, JOBOBJECT_EXTENDED_LIMIT_INFORMATION *e);
BOOL jc_set_flags(HANDLE job, DWORD flags);
/* Completion port helpers */
HANDLE jc_port(void);
BOOL jc_assoc(HANDLE job, HANDLE port, ULONG_PTR key);
/* Fetch the next job message: returns 1 on message, 0 timeout */
int jc_getmsg(HANDLE port, DWORD ms, DWORD *msg, ULONG_PTR *key, ULONG_PTR *pid);
/* Wait for a particular message id for given pid (pid 0 = any); 1 if seen within ms. Other messages are logged into mask. */
int jc_wait_msg(HANDLE port, DWORD want, ULONG_PTR pid, DWORD ms, DWORD *seen_mask);
/* Unique object name for this test run */
void jc_name(char *out, size_t cap, const char *tag);
void jc_namew(WCHAR *out, size_t cap, const char *tag);
/* duplicate a handle with a given access mask */
HANDLE jc_dup(HANDLE h, DWORD access);

/* Declarations missing from the mingw headers */
typedef struct { LONG64 MaxIops; LONG64 MaxBandwidth; LONG64 ReservationIops; PCWSTR VolumeName; ULONG BaseIoSize; ULONG ControlFlags; } JC_IO_RATE_INFO;
typedef VOID  (WINAPI *jc_FreeMemoryJobObject_t)(PVOID);
typedef DWORD (WINAPI *jc_QueryIoRate_t)(HANDLE, PCWSTR, JC_IO_RATE_INFO **, ULONG *);
typedef DWORD (WINAPI *jc_SetIoRate_t)(HANDLE, JC_IO_RATE_INFO *);
#define JC_FreeMemoryJobObject ((jc_FreeMemoryJobObject_t)t_proc_k32("FreeMemoryJobObject"))
#define JC_QueryIoRate ((jc_QueryIoRate_t)t_proc_k32("QueryIoRateControlInformationJobObject"))
#define JC_SetIoRate ((jc_SetIoRate_t)t_proc_k32("SetIoRateControlInformationJobObject"))
/* CPU rate control (mingw lacks MinRate/MaxRate view) */
typedef struct { DWORD ControlFlags; DWORD Value; } JC_CPU_RATE;
#endif
