#include "ho.h"
#include <tlhelp32.h>

/* Return-convention table: each row performs a deliberately failing call. */
enum { R_NULL = 1, R_INV = 2 };
typedef struct { const char *name; HANDLE (*fn)(void); int kind; DWORD err; } row_t;   /* err 0 = not asserted */

static char g_colm[96], g_cole[96];       /* names of an existing MUTEX / EVENT (type collisions) */
static WCHAR g_colmw[96], g_colew[96];
static char g_tmp1[MAX_PATH]; static WCHAR g_tmp1w[MAX_PATH];
static char g_nopipe[96];
static HANDLE g_nonov;
static const char g_noname[] = "k32t_ho_no_such_object_name_zz";
static const WCHAR g_nonamew[] = L"k32t_ho_no_such_object_name_zz";
static DWORD WINAPI nop_thread(LPVOID p) { return 0; }

#define ROW(nm, expr) static HANDLE row_##nm(void) { return (expr); }
/* files and directories */
ROW(createfilea_nofile,   U(CreateFileA)(g_tmp1, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL))
ROW(createfilew_nofile,   U(CreateFileW)(g_tmp1w, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL))
ROW(createfilea_nodir,    U(CreateFileA)("C:\\k32t_ho_no_such_dir\\x\\y.bin", GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL))
ROW(createfilew_nodir,    U(CreateFileW)(L"C:\\k32t_ho_no_such_dir\\x\\y.bin", GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL))
ROW(createfile2_nofile,   U(CreateFile2)(g_tmp1w, GENERIC_READ, 0, OPEN_EXISTING, NULL))
ROW(reopenfile_null,      U(ReOpenFile)(NULL, GENERIC_READ, FILE_SHARE_READ, 0))
ROW(findfirstfilea_nodir, U(FindFirstFileA)("C:\\k32t_ho_no_such_dir\\*", &(WIN32_FIND_DATAA){0}))
ROW(findfirstfilea_nofile,U(FindFirstFileA)(g_tmp1, &(WIN32_FIND_DATAA){0}))
ROW(findfirstfilew_nodir, U(FindFirstFileW)(L"C:\\k32t_ho_no_such_dir\\*", &(WIN32_FIND_DATAW){0}))
ROW(findfirstfilew_nofile,U(FindFirstFileW)(g_tmp1w, &(WIN32_FIND_DATAW){0}))
ROW(findfirstfileexw_nofile, U(FindFirstFileExW)(g_tmp1w, FindExInfoStandard, &(WIN32_FIND_DATAW){0}, FindExSearchNameMatch, NULL, 0))
ROW(findfirstchangea_nodir, U(FindFirstChangeNotificationA)("C:\\k32t_ho_no_such_dir", FALSE, FILE_NOTIFY_CHANGE_FILE_NAME))
ROW(findfirstchangew_nodir, U(FindFirstChangeNotificationW)(L"C:\\k32t_ho_no_such_dir", FALSE, FILE_NOTIFY_CHANGE_FILE_NAME))
/* events / mutexes / semaphores / timers / mappings / jobs: type collisions */
ROW(createeventa_coll,    U(CreateEventA)(NULL, TRUE, FALSE, g_colm))
ROW(createeventw_coll,    U(CreateEventW)(NULL, TRUE, FALSE, g_colmw))
ROW(createeventexa_coll,  U(CreateEventExA)(NULL, g_colm, 0, EVENT_ALL_ACCESS))
ROW(createeventexw_coll,  U(CreateEventExW)(NULL, g_colmw, 0, EVENT_ALL_ACCESS))
ROW(createmutexa_coll,    U(CreateMutexA)(NULL, FALSE, g_cole))
ROW(createmutexw_coll,    U(CreateMutexW)(NULL, FALSE, g_colew))
ROW(createmutexexa_coll,  U(CreateMutexExA)(NULL, g_cole, 0, MUTEX_ALL_ACCESS))
ROW(createmutexexw_coll,  U(CreateMutexExW)(NULL, g_colew, 0, MUTEX_ALL_ACCESS))
ROW(createmutexexw_badflags, U(CreateMutexExW)(NULL, NULL, 0x2, MUTEX_ALL_ACCESS))
ROW(createsemaphorea_coll,U(CreateSemaphoreA)(NULL, 0, 1, g_cole))
ROW(createsemaphorew_coll,U(CreateSemaphoreW)(NULL, 0, 1, g_colew))
ROW(createsemaphorea_init_gt_max, U(CreateSemaphoreA)(NULL, 2, 1, NULL))
ROW(createsemaphorew_init_gt_max, U(CreateSemaphoreW)(NULL, 2, 1, NULL))
ROW(createsemaphorea_max_zero,    U(CreateSemaphoreA)(NULL, 0, 0, NULL))
ROW(createsemaphorea_neg_init,    U(CreateSemaphoreA)(NULL, -1, 5, NULL))
ROW(createsemaphorea_neg_max,     U(CreateSemaphoreA)(NULL, 0, -1, NULL))
ROW(createsemaphoreexw_flags,     U(CreateSemaphoreExW)(NULL, 0, 1, NULL, 1, SEMAPHORE_ALL_ACCESS))
ROW(createwaitabletimera_coll, U(CreateWaitableTimerA)(NULL, TRUE, g_cole))
ROW(createwaitabletimerw_coll, U(CreateWaitableTimerW)(NULL, TRUE, g_colew))
ROW(createwaitabletimerexw_coll, U(CreateWaitableTimerExW)(NULL, g_colew, 0, TIMER_ALL_ACCESS))
ROW(createfilemappinga_zero_size, U(CreateFileMappingA)(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 0, NULL))
ROW(createfilemappingw_zero_size, U(CreateFileMappingW)(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 0, NULL))
ROW(createfilemappinga_coll,      U(CreateFileMappingA)(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, g_cole))
ROW(createfilemappingw_coll,      U(CreateFileMappingW)(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, g_colew))
ROW(createfilemappingw_badprot,   U(CreateFileMappingW)(INVALID_HANDLE_VALUE, NULL, 0x3, 0, 4096, NULL))
ROW(createfilemappingw_zeroprot,  U(CreateFileMappingW)(INVALID_HANDLE_VALUE, NULL, 0, 0, 4096, NULL))
ROW(createfilemappingnumaw_badnode, U(CreateFileMappingNumaW)(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, NULL, 0x40))
ROW(createfilemappinga_badfile,   U(CreateFileMappingA)((HANDLE)0x7FFFFFC, NULL, PAGE_READWRITE, 0, 4096, NULL))
ROW(createjobobjecta_coll,        U(CreateJobObjectA)(NULL, g_cole))
ROW(createjobobjectw_coll,        U(CreateJobObjectW)(NULL, g_colew))
/* Open* */
ROW(openevena_missing,    U(OpenEventA)(SYNCHRONIZE, FALSE, g_noname))
ROW(openeventw_missing,   U(OpenEventW)(SYNCHRONIZE, FALSE, g_nonamew))
ROW(openeventa_wrongtype, U(OpenEventA)(SYNCHRONIZE, FALSE, g_colm))
ROW(openmutexa_missing,   U(OpenMutexA)(SYNCHRONIZE, FALSE, g_noname))
ROW(openmutexw_missing,   U(OpenMutexW)(SYNCHRONIZE, FALSE, g_nonamew))
ROW(openmutexa_wrongtype, U(OpenMutexA)(SYNCHRONIZE, FALSE, g_cole))
ROW(opensemaphorea_missing, U(OpenSemaphoreA)(SYNCHRONIZE, FALSE, g_noname))
ROW(opensemaphorew_missing, U(OpenSemaphoreW)(SYNCHRONIZE, FALSE, g_nonamew))
ROW(opensemaphorea_wrongtype, U(OpenSemaphoreA)(SYNCHRONIZE, FALSE, g_cole))
ROW(openwaitabletimera_missing, U(OpenWaitableTimerA)(SYNCHRONIZE, FALSE, g_noname))
ROW(openwaitabletimerw_missing, U(OpenWaitableTimerW)(SYNCHRONIZE, FALSE, g_nonamew))
ROW(openfilemappinga_missing, U(OpenFileMappingA)(FILE_MAP_READ, FALSE, g_noname))
ROW(openfilemappingw_missing, U(OpenFileMappingW)(FILE_MAP_READ, FALSE, g_nonamew))
ROW(openfilemappinga_wrongtype, U(OpenFileMappingA)(FILE_MAP_READ, FALSE, g_colm))
ROW(openjobobjecta_missing, U(OpenJobObjectA)(JOB_OBJECT_QUERY, FALSE, g_noname))
ROW(openjobobjectw_missing, U(OpenJobObjectW)(JOB_OBJECT_QUERY, FALSE, g_nonamew))
ROW(openjobobjecta_wrongtype, U(OpenJobObjectA)(JOB_OBJECT_QUERY, FALSE, g_colm))
ROW(openprocess_pid0,     U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, 0))
ROW(openprocess_nopid,    U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, 0x0FFFFFF8))
ROW(openthread_tid0,      U(OpenThread)(THREAD_QUERY_LIMITED_INFORMATION, FALSE, 0))
ROW(openthread_notid,     U(OpenThread)(THREAD_QUERY_LIMITED_INFORMATION, FALSE, 0x0FFFFFF8))
/* misc creators */
ROW(createnamedpipea_zero_instances, U(CreateNamedPipeA)(g_nopipe, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE, 0, 64, 64, 0, NULL))
ROW(createnamedpipea_256_instances,  U(CreateNamedPipeA)(g_nopipe, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE, 256, 64, 64, 0, NULL))
ROW(createnamedpipea_badname,        U(CreateNamedPipeA)("k32t_ho_not_a_pipe_path", PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE, 1, 64, 64, 0, NULL))
ROW(createnamedpipew_badname,        U(CreateNamedPipeW)(L"k32t_ho_not_a_pipe_path", PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE, 1, 64, 64, 0, NULL))
ROW(createiocp_nonoverlapped,        U(CreateIoCompletionPort)(g_nonov, NULL, 1, 0))
ROW(createtoolhelp_nomodpid,         U(CreateToolhelp32Snapshot)(TH32CS_SNAPMODULE, 0x0FFFFFF8))
ROW(createmailslota_badname,         U(CreateMailslotA)("k32t_ho_not_a_slot", 0, 0, NULL))
ROW(createmailslotw_badname,         U(CreateMailslotW)(L"k32t_ho_not_a_slot", 0, 0, NULL))
ROW(heapcreate_huge,                 U(HeapCreate)(0, (SIZE_T)1 << 60, 0))
ROW(loadlibrarya_missing,            (HANDLE)U(LoadLibraryA)("k32t_ho_no_such_module.dll"))
ROW(loadlibraryw_missing,            (HANDLE)U(LoadLibraryW)(L"k32t_ho_no_such_module.dll"))
ROW(loadlibraryexw_hfile,            (HANDLE)U(LoadLibraryExW)(L"kernel32.dll", (HANDLE)4, 0))
ROW(getmodulehandlea_missing,        (HANDLE)U(GetModuleHandleA)("k32t_ho_no_such_module.dll"))
ROW(getmodulehandlew_missing,        (HANDLE)U(GetModuleHandleW)(L"k32t_ho_no_such_module.dll"))
ROW(createremotethread_nullproc,     U(CreateRemoteThread)(NULL, NULL, 0, nop_thread, NULL, 0, NULL))
ROW(createremotethreadex_nullproc,   U(CreateRemoteThreadEx)(NULL, NULL, 0, nop_thread, NULL, 0, NULL, NULL))
ROW(createthread_absurd_stack,       U(CreateThread)(NULL, (SIZE_T)1 << 60, nop_thread, NULL, STACK_SIZE_PARAM_IS_A_RESERVATION, NULL))
ROW(getstdhandle_bad,                U(GetStdHandle)(0))
ROW(getstdhandle_bad2,               U(GetStdHandle)(5))
ROW(createmem_resnotif_badtype,      U(CreateMemoryResourceNotification)((MEMORY_RESOURCE_NOTIFICATION_TYPE)5))
ROW(createconsolescreenbuffer_badflags, U(CreateConsoleScreenBuffer)(GENERIC_READ | GENERIC_WRITE, 0, NULL, 0, NULL))

#define E(nm, kind, err) { #nm, row_##nm, kind, err }
static const row_t g_rows[] = {
    E(createfilea_nofile, R_INV, ERROR_FILE_NOT_FOUND), E(createfilew_nofile, R_INV, ERROR_FILE_NOT_FOUND),
    E(createfilea_nodir, R_INV, ERROR_PATH_NOT_FOUND), E(createfilew_nodir, R_INV, ERROR_PATH_NOT_FOUND),
    E(createfile2_nofile, R_INV, ERROR_FILE_NOT_FOUND), E(reopenfile_null, R_INV, 0),
    E(findfirstfilea_nodir, R_INV, ERROR_PATH_NOT_FOUND), E(findfirstfilea_nofile, R_INV, ERROR_FILE_NOT_FOUND),
    E(findfirstfilew_nodir, R_INV, ERROR_PATH_NOT_FOUND), E(findfirstfilew_nofile, R_INV, ERROR_FILE_NOT_FOUND),
    E(findfirstfileexw_nofile, R_INV, ERROR_FILE_NOT_FOUND),
    E(findfirstchangea_nodir, R_INV, 0), E(findfirstchangew_nodir, R_INV, 0),
    E(createeventa_coll, R_NULL, ERROR_INVALID_HANDLE), E(createeventw_coll, R_NULL, ERROR_INVALID_HANDLE),
    E(createeventexa_coll, R_NULL, ERROR_INVALID_HANDLE), E(createeventexw_coll, R_NULL, ERROR_INVALID_HANDLE),
    E(createmutexa_coll, R_NULL, ERROR_INVALID_HANDLE), E(createmutexw_coll, R_NULL, ERROR_INVALID_HANDLE),
    E(createmutexexa_coll, R_NULL, ERROR_INVALID_HANDLE), E(createmutexexw_coll, R_NULL, ERROR_INVALID_HANDLE),
    E(createmutexexw_badflags, R_NULL, ERROR_INVALID_PARAMETER),
    E(createsemaphorea_coll, R_NULL, ERROR_INVALID_HANDLE), E(createsemaphorew_coll, R_NULL, ERROR_INVALID_HANDLE),
    E(createsemaphorea_init_gt_max, R_NULL, ERROR_INVALID_PARAMETER), E(createsemaphorew_init_gt_max, R_NULL, ERROR_INVALID_PARAMETER),
    E(createsemaphorea_max_zero, R_NULL, ERROR_INVALID_PARAMETER), E(createsemaphorea_neg_init, R_NULL, ERROR_INVALID_PARAMETER),
    E(createsemaphorea_neg_max, R_NULL, ERROR_INVALID_PARAMETER), E(createsemaphoreexw_flags, R_NULL, ERROR_INVALID_PARAMETER),
    E(createwaitabletimera_coll, R_NULL, ERROR_INVALID_HANDLE), E(createwaitabletimerw_coll, R_NULL, ERROR_INVALID_HANDLE),
    E(createwaitabletimerexw_coll, R_NULL, ERROR_INVALID_HANDLE),
    E(createfilemappinga_zero_size, R_NULL, ERROR_INVALID_PARAMETER), E(createfilemappingw_zero_size, R_NULL, ERROR_INVALID_PARAMETER),
    E(createfilemappinga_coll, R_NULL, ERROR_INVALID_HANDLE), E(createfilemappingw_coll, R_NULL, ERROR_INVALID_HANDLE),
    E(createfilemappingw_badprot, R_NULL, ERROR_INVALID_PARAMETER), E(createfilemappingw_zeroprot, R_NULL, ERROR_INVALID_PARAMETER),
    E(createfilemappingnumaw_badnode, R_NULL, ERROR_INVALID_PARAMETER), E(createfilemappinga_badfile, R_NULL, ERROR_INVALID_HANDLE),
    E(createjobobjecta_coll, R_NULL, ERROR_INVALID_HANDLE), E(createjobobjectw_coll, R_NULL, ERROR_INVALID_HANDLE),
    E(openevena_missing, R_NULL, ERROR_FILE_NOT_FOUND), E(openeventw_missing, R_NULL, ERROR_FILE_NOT_FOUND),
    E(openeventa_wrongtype, R_NULL, ERROR_INVALID_HANDLE),
    E(openmutexa_missing, R_NULL, ERROR_FILE_NOT_FOUND), E(openmutexw_missing, R_NULL, ERROR_FILE_NOT_FOUND),
    E(openmutexa_wrongtype, R_NULL, ERROR_INVALID_HANDLE),
    E(opensemaphorea_missing, R_NULL, ERROR_FILE_NOT_FOUND), E(opensemaphorew_missing, R_NULL, ERROR_FILE_NOT_FOUND),
    E(opensemaphorea_wrongtype, R_NULL, ERROR_INVALID_HANDLE),
    E(openwaitabletimera_missing, R_NULL, ERROR_FILE_NOT_FOUND), E(openwaitabletimerw_missing, R_NULL, ERROR_FILE_NOT_FOUND),
    E(openfilemappinga_missing, R_NULL, ERROR_FILE_NOT_FOUND), E(openfilemappingw_missing, R_NULL, ERROR_FILE_NOT_FOUND),
    E(openfilemappinga_wrongtype, R_NULL, ERROR_INVALID_HANDLE),
    E(openjobobjecta_missing, R_NULL, ERROR_FILE_NOT_FOUND), E(openjobobjectw_missing, R_NULL, ERROR_FILE_NOT_FOUND),
    E(openjobobjecta_wrongtype, R_NULL, ERROR_INVALID_HANDLE),
    E(openprocess_pid0, R_NULL, ERROR_INVALID_PARAMETER), E(openprocess_nopid, R_NULL, ERROR_INVALID_PARAMETER),
    E(openthread_tid0, R_NULL, ERROR_INVALID_PARAMETER), E(openthread_notid, R_NULL, ERROR_INVALID_PARAMETER),
    E(createnamedpipea_zero_instances, R_INV, 0), E(createnamedpipea_256_instances, R_INV, 0),
    E(createnamedpipea_badname, R_INV, 0), E(createnamedpipew_badname, R_INV, 0),
    E(createiocp_nonoverlapped, R_NULL, ERROR_INVALID_PARAMETER),
    E(createtoolhelp_nomodpid, R_INV, 0),
    E(createmailslota_badname, R_INV, 0), E(createmailslotw_badname, R_INV, 0),
    E(heapcreate_huge, R_NULL, ERROR_NOT_ENOUGH_MEMORY),
    E(loadlibrarya_missing, R_NULL, ERROR_MOD_NOT_FOUND), E(loadlibraryw_missing, R_NULL, ERROR_MOD_NOT_FOUND),
    E(loadlibraryexw_hfile, R_NULL, ERROR_INVALID_PARAMETER),
    E(getmodulehandlea_missing, R_NULL, ERROR_MOD_NOT_FOUND), E(getmodulehandlew_missing, R_NULL, ERROR_MOD_NOT_FOUND),
    E(createremotethread_nullproc, R_NULL, ERROR_INVALID_HANDLE), E(createremotethreadex_nullproc, R_NULL, ERROR_INVALID_HANDLE),
    E(createthread_absurd_stack, R_NULL, 0),
    E(getstdhandle_bad, R_INV, 0), E(getstdhandle_bad2, R_INV, 0),
    E(createmem_resnotif_badtype, R_NULL, ERROR_INVALID_PARAMETER),
    E(createconsolescreenbuffer_badflags, R_INV, 0),
};

static void rows_setup(HANDLE *mutex, HANDLE *event, HANDLE *file) {
    char p[MAX_PATH];
    ho_uniq(g_colm, sizeof g_colm, "colm"); ho_wide(g_colm, g_colmw, 96);
    ho_uniq(g_cole, sizeof g_cole, "cole"); ho_wide(g_cole, g_colew, 96);
    t_tmpfile(g_tmp1, sizeof g_tmp1, "no_such_file.txt"); ho_wide(g_tmp1, g_tmp1w, MAX_PATH);
    snprintf(g_nopipe, sizeof g_nopipe, "\\\\.\\pipe\\k32t_ho_ret_%lu", GetCurrentProcessId());
    *mutex = CreateMutexA(NULL, FALSE, g_colm);
    *event = CreateEventA(NULL, TRUE, FALSE, g_cole);
    t_tmpfile(p, sizeof p, "nonov.bin");
    g_nonov = *file = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, NULL);
}
static void run_rows(unsigned from, unsigned to) {
    HANDLE m, e, f; rows_setup(&m, &e, &f);
    for (unsigned i = from; i < to && i < sizeof g_rows / sizeof *g_rows; i++) {
        const row_t *r = &g_rows[i];
        SET_ERR(0);
        HANDLE h = r->fn();
        DWORD err = GetLastError();
        if (r->kind == R_NULL) CHECKF(h == NULL, "%s: returned %p, want NULL", r->name, h);
        else CHECKF(h == INVALID_HANDLE_VALUE, "%s: returned %p, want INVALID_HANDLE_VALUE", r->name, h);
        if (r->err) CHECKF(err == r->err, "%s: GetLastError %lu want %lu", r->name, err, r->err);
        else t_note("%s: err=%lu", r->name, err);
        if (h && h != INVALID_HANDLE_VALUE && r->kind != R_NULL + 100) {
            if (strstr(r->name, "loadlibrary") || strstr(r->name, "getmodulehandle")) FreeLibrary((HMODULE)h); else CloseHandle(h);
        }
    }
    CloseHandle(m); CloseHandle(e); CloseHandle(f);
    g_nonov = NULL;
}
#define NROWS ((unsigned)(sizeof g_rows / sizeof *g_rows))
TEST(ho_ret_failure_table_files_and_find) { run_rows(0, 13); }
TEST(ho_ret_failure_table_event_mutex_sem) { run_rows(13, 29); }
TEST(ho_ret_failure_table_timer_mapping_job) { run_rows(29, 44); }
TEST(ho_ret_failure_table_open_calls) { run_rows(44, 62); }
TEST(ho_ret_failure_table_misc) { run_rows(62, NROWS); }
TEST(ho_ret_failure_table_covers_all_rows) { CHECK(NROWS >= 80); t_note("rows=%u", NROWS); }
TEST(ho_ret_failure_table_no_leak) {
    NEED_HC();
    run_rows(0, NROWS);          /* warm-up: lazily cached handles */
    DWORD base = ho_count();
    run_rows(0, NROWS);
    EXPECT_EQ(ho_count(), base);
}
TEST(ho_ret_failure_table_repeat_stable) {
    for (int i = 0; i < 3; i++) run_rows(13, 62);
}

/* success conventions */
TEST(ho_ret_success_never_null_or_invalid) {
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "succ.bin");
    char n[96]; ho_uniq(n, sizeof n, "succ");
    HANDLE hs[24]; const char *nm[24]; int k = 0;
#define ADD(label, expr) do { nm[k] = label; hs[k++] = (expr); } while (0)
    ADD("CreateFileA", U(CreateFileA)(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL));
    ADD("CreateEventA", U(CreateEventA)(NULL, TRUE, FALSE, NULL));
    ADD("CreateEventExA", U(CreateEventExA)(NULL, NULL, 0, EVENT_ALL_ACCESS));
    ADD("CreateMutexA", U(CreateMutexA)(NULL, FALSE, NULL));
    ADD("CreateMutexExA", U(CreateMutexExA)(NULL, NULL, 0, MUTEX_ALL_ACCESS));
    ADD("CreateSemaphoreA", U(CreateSemaphoreA)(NULL, 0, 1, NULL));
    ADD("CreateSemaphoreExW", U(CreateSemaphoreExW)(NULL, 0, 1, NULL, 0, SEMAPHORE_ALL_ACCESS));
    ADD("CreateWaitableTimerA", U(CreateWaitableTimerA)(NULL, TRUE, NULL));
    ADD("CreateWaitableTimerExW", U(CreateWaitableTimerExW)(NULL, NULL, 0, TIMER_ALL_ACCESS));
    ADD("CreateFileMappingA", U(CreateFileMappingA)(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, NULL));
    ADD("CreateJobObjectA", U(CreateJobObjectA)(NULL, NULL));
    ADD("CreateIoCompletionPort", U(CreateIoCompletionPort)(INVALID_HANDLE_VALUE, NULL, 0, 0));
    ADD("CreateThread", U(CreateThread)(NULL, 0, nop_thread, NULL, 0, NULL));
    ADD("OpenProcess", U(OpenProcess)(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentProcessId()));
    ADD("OpenThread", U(OpenThread)(THREAD_QUERY_LIMITED_INFORMATION, FALSE, GetCurrentThreadId()));
    ADD("CreateToolhelp32Snapshot", U(CreateToolhelp32Snapshot)(TH32CS_SNAPPROCESS, 0));
    ADD("CreateMemoryResourceNotification", U(CreateMemoryResourceNotification)(LowMemoryResourceNotification));
    ADD("HeapCreate", U(HeapCreate)(0, 0x10000, 0));
    ADD("CreateNamedPipeA", U(CreateNamedPipeA)(g_nopipe[0] ? g_nopipe : "\\\\.\\pipe\\k32t_ho_succ", PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE, 1, 64, 64, 0, NULL));
    char pat[MAX_PATH]; t_tmpfile(pat, sizeof pat, "*");
    ADD("FindFirstFileA", U(FindFirstFileA)(pat, &(WIN32_FIND_DATAA){0}));
    ADD("GetStdHandle(ERR)", U(GetStdHandle)(STD_ERROR_HANDLE));
    ADD("FindFirstChangeNotificationA", U(FindFirstChangeNotificationA)(t_tmpdir(), FALSE, FILE_NOTIFY_CHANGE_FILE_NAME));
#undef ADD
    for (int i = 0; i < k; i++) {
        if (!strcmp(nm[i], "GetStdHandle(ERR)")) { t_note("stderr handle = %p", hs[i]); continue; }
        CHECKF(hs[i] != NULL && hs[i] != INVALID_HANDLE_VALUE, "%s -> %p", nm[i], hs[i]);
        if (!strcmp(nm[i], "HeapCreate")) { if (hs[i]) HeapDestroy(hs[i]); }
        else if (!strcmp(nm[i], "FindFirstFileA")) { if (hs[i] != INVALID_HANDLE_VALUE) FindClose(hs[i]); }
        else if (!strcmp(nm[i], "FindFirstChangeNotificationA")) { if (hs[i] != INVALID_HANDLE_VALUE) FindCloseChangeNotification(hs[i]); }
        else if (hs[i] && hs[i] != INVALID_HANDLE_VALUE) CloseHandle(hs[i]);
    }
    DeleteFileA(p);
}
TEST(ho_ret_success_createfile_family_invalid_on_failure_only) {
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "cf.bin");
    HANDLE h = U(CreateFileA)(p, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE && h != NULL);
    HANDLE h2 = U(CreateFileA)(p, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
    CHECK(h2 == INVALID_HANDLE_VALUE);
    EXPECT_ERR(ERROR_FILE_EXISTS);
    CloseHandle(h); DeleteFileA(p);
}
typedef HANDLE (*named_create_fn)(const char *);
static HANDLE nc_event(const char *n) { return CreateEventA(NULL, TRUE, FALSE, n); }
static HANDLE nc_mutex(const char *n) { return CreateMutexA(NULL, FALSE, n); }
static HANDLE nc_sem(const char *n) { return CreateSemaphoreA(NULL, 0, 1, n); }
static HANDLE nc_timer(const char *n) { return CreateWaitableTimerA(NULL, TRUE, n); }
static HANDLE nc_map(const char *n) { return CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, n); }
static HANDLE nc_job(const char *n) { return CreateJobObjectA(NULL, n); }
static const struct { const char *n; named_create_fn f; } g_nc[] = {
    { "event", nc_event }, { "mutex", nc_mutex }, { "semaphore", nc_sem }, { "timer", nc_timer }, { "mapping", nc_map }, { "job", nc_job } };
TEST(ho_ret_named_already_exists_same_type) {
    for (unsigned i = 0; i < sizeof g_nc / sizeof *g_nc; i++) {
        char n[96]; ho_uniq(n, sizeof n, g_nc[i].n);
        SET_ERR(0);
        HANDLE a = g_nc[i].f(n); DWORD e1 = GetLastError();
        CHECKF(a != NULL, "%s create", g_nc[i].n);
        CHECKF(e1 == 0, "%s first create err=%lu", g_nc[i].n, e1);
        HANDLE b = g_nc[i].f(n); DWORD e2 = GetLastError();
        CHECKF(b != NULL && b != a, "%s second create b=%p a=%p", g_nc[i].n, b, a);
        CHECKF(e2 == ERROR_ALREADY_EXISTS, "%s second create err=%lu", g_nc[i].n, e2);
        if (a) CloseHandle(a);
        if (b) CloseHandle(b);
    }
}
TEST(ho_ret_named_cross_type_collision_matrix) {
    for (unsigned i = 0; i < sizeof g_nc / sizeof *g_nc; i++) {
        char n[96]; ho_uniq(n, sizeof n, "x");
        HANDLE a = g_nc[i].f(n);
        CHECKF(a != NULL, "%s", g_nc[i].n);
        for (unsigned j = 0; j < sizeof g_nc / sizeof *g_nc; j++) {
            if (i == j) continue;
            SET_ERR(0);
            HANDLE b = g_nc[j].f(n); DWORD e = GetLastError();
            CHECKF(b == NULL, "%s over %s returned %p", g_nc[j].n, g_nc[i].n, b);
            CHECKF(e == ERROR_INVALID_HANDLE, "%s over %s err=%lu", g_nc[j].n, g_nc[i].n, e);
            if (b) CloseHandle(b);
        }
        if (a) CloseHandle(a);
    }
}
TEST(ho_ret_named_case_insensitive) {
    char n[96], up[96]; ho_uniq(n, sizeof n, "Case");
    strcpy(up, n); for (char *c = up; *c; c++) if (*c >= 'a' && *c <= 'z') *c -= 32;
    HANDLE a = CreateEventA(NULL, TRUE, FALSE, n);
    SET_ERR(0);
    HANDLE b = U(CreateEventA)(NULL, TRUE, FALSE, up);
    CHECK(b != NULL); EXPECT_ERR(ERROR_ALREADY_EXISTS);
    SetEvent(a);
    if (b) EXPECT_EQ(WaitForSingleObject(b, 0), WAIT_OBJECT_0);
    HANDLE c = U(OpenEventA)(SYNCHRONIZE, FALSE, up);
    CHECK(c != NULL);
    CloseHandle(a); if (b) CloseHandle(b); if (c) CloseHandle(c);
}
TEST(ho_ret_named_local_prefix_same_object) {
    REQUIRE(U(CompareObjectHandles));
    char n[96], ln[110]; ho_uniq(n, sizeof n, "loc");
    snprintf(ln, sizeof ln, "Local\\%s", n);
    HANDLE a = CreateEventA(NULL, TRUE, FALSE, n);
    SET_ERR(0);
    HANDLE b = U(CreateEventA)(NULL, TRUE, FALSE, ln);
    DWORD e = GetLastError();
    CHECK(b != NULL);
    t_note("Local\\ prefix: ERROR_ALREADY_EXISTS=%d", e == ERROR_ALREADY_EXISTS);
    if (b && e == ERROR_ALREADY_EXISTS) EXPECT_EQ(U(CompareObjectHandles)(a, b), TRUE);
    CloseHandle(a); if (b) CloseHandle(b);
}
TEST(ho_ret_named_backslash_in_name) {
    char n[96]; ho_uniq(n, sizeof n, "bs"); strcat(n, "\\sub");
    SET_ERR(0);
    HANDLE h = U(CreateEventA)(NULL, TRUE, FALSE, n);
    t_note("event name with embedded backslash: h=%p err=%lu", h, GetLastError());
    if (h) CloseHandle(h);
}
TEST(ho_ret_named_too_long) {
    static char n[40000]; memset(n, 'a', sizeof n - 1); n[sizeof n - 1] = 0;
    SET_ERR(0);
    HANDLE h = U(CreateEventA)(NULL, TRUE, FALSE, n);
    CHECK(h == NULL);
    t_note("40000-char event name: err=%lu", GetLastError());
    if (h) CloseHandle(h);
}
TEST(ho_ret_named_empty_string) {
    SET_ERR(0);
    HANDLE h = U(CreateEventA)(NULL, TRUE, FALSE, "");
    t_note("empty event name: h=%p err=%lu", h, GetLastError());
    if (h) CloseHandle(h);
}

/* ---- type confusion ---- */
enum { K_EVENT, K_MUTEX, K_SEM, K_TIMER, K_FILE, K_MAP, K_PROC, K_THREAD, K_JOB, K_IOCP, K_PIPE, K_COUNT };
static const char *g_kn[K_COUNT] = { "event", "mutex", "semaphore", "timer", "file", "mapping", "process", "thread", "job", "iocp", "pipe" };
static HANDLE g_kh[K_COUNT]; static HANDLE g_kaux;
static void kinds_make(void) {
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "kinds.bin");
    g_kh[K_EVENT] = CreateEventA(NULL, TRUE, FALSE, NULL);
    g_kh[K_MUTEX] = CreateMutexA(NULL, FALSE, NULL);
    g_kh[K_SEM] = CreateSemaphoreA(NULL, 0, 10, NULL);
    g_kh[K_TIMER] = CreateWaitableTimerA(NULL, TRUE, NULL);
    g_kh[K_FILE] = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, NULL);
    g_kh[K_MAP] = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, NULL);
    g_kh[K_PROC] = NULL; DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(), &g_kh[K_PROC], 0, FALSE, DUPLICATE_SAME_ACCESS);
    g_kh[K_THREAD] = NULL; DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &g_kh[K_THREAD], 0, FALSE, DUPLICATE_SAME_ACCESS);
    g_kh[K_JOB] = CreateJobObjectA(NULL, NULL);
    g_kh[K_IOCP] = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    CreatePipe(&g_kh[K_PIPE], &g_kaux, NULL, 0);
}
static void kinds_free(void) { for (int i = 0; i < K_COUNT; i++) if (g_kh[i]) CloseHandle(g_kh[i]); if (g_kaux) CloseHandle(g_kaux); g_kaux = NULL; memset(g_kh, 0, sizeof g_kh); }

typedef BOOL (*op_fn)(HANDLE h);
static BOOL op_setevent(HANDLE h) { return U(SetEvent)(h); }
static BOOL op_resetevent(HANDLE h) { return U(ResetEvent)(h); }
static BOOL op_pulseevent(HANDLE h) { return U(PulseEvent)(h); }
static BOOL op_relsem(HANDLE h) { return U(ReleaseSemaphore)(h, 1, NULL); }
static BOOL op_relmutex(HANDLE h) { return U(ReleaseMutex)(h); }
static BOOL op_settimer(HANDLE h) { LARGE_INTEGER d; d.QuadPart = -10000000LL * 3600; return U(SetWaitableTimer)(h, &d, 0, NULL, NULL, FALSE); }
static BOOL op_canceltimer(HANDLE h) { return U(CancelWaitableTimer)(h); }
static BOOL op_readfile(HANDLE h) { char b[4]; DWORD n; return U(ReadFile)(h, b, 1, &n, NULL); }
static BOOL op_mapview(HANDLE h) { void *v = U(MapViewOfFile)(h, FILE_MAP_READ, 0, 0, 0); if (v) { UnmapViewOfFile(v); return TRUE; } return FALSE; }
static BOOL op_exitproc(HANDLE h) { DWORD c; return U(GetExitCodeProcess)(h, &c); }
static BOOL op_exitthread(HANDLE h) { DWORD c; return U(GetExitCodeThread)(h, &c); }
static BOOL op_queryjob(HANDLE h) { JOBOBJECT_BASIC_ACCOUNTING_INFORMATION i; return U(QueryInformationJobObject)(h, JobObjectBasicAccountingInformation, &i, sizeof i, NULL); }
static BOOL op_termjob(HANDLE h) { return U(TerminateJobObject)(h, 1); }
static BOOL op_setthreadprio(HANDLE h) { return U(SetThreadPriority)(h, THREAD_PRIORITY_NORMAL); }
static BOOL op_flushfile(HANDLE h) { return U(FlushFileBuffers)(h); }
static BOOL op_getqueued(HANDLE h) { DWORD n; ULONG_PTR k; OVERLAPPED *o; return U(GetQueuedCompletionStatus)(h, &n, &k, &o, 0) || GetLastError() == WAIT_TIMEOUT; }
static BOOL op_postqueued(HANDLE h) { return U(PostQueuedCompletionStatus)(h, 0, 0, NULL); }
static const struct { const char *n; op_fn f; unsigned okmask; } g_ops[] = {
    { "SetEvent", op_setevent, 1u << K_EVENT }, { "ResetEvent", op_resetevent, 1u << K_EVENT }, { "PulseEvent", op_pulseevent, 1u << K_EVENT },
    { "ReleaseSemaphore", op_relsem, 1u << K_SEM }, { "ReleaseMutex", op_relmutex, 0 /* mutex not owned: ERROR_NOT_OWNER, checked separately */ },
    { "SetWaitableTimer", op_settimer, 1u << K_TIMER }, { "CancelWaitableTimer", op_canceltimer, 1u << K_TIMER },
    { "ReadFile", op_readfile, (1u << K_FILE) | (1u << K_PIPE) },
    { "MapViewOfFile", op_mapview, 1u << K_MAP },
    { "GetExitCodeProcess", op_exitproc, 1u << K_PROC }, { "GetExitCodeThread", op_exitthread, 1u << K_THREAD },
    { "QueryInformationJobObject", op_queryjob, 1u << K_JOB }, { "TerminateJobObject(empty)", op_termjob, 1u << K_JOB },
    { "SetThreadPriority", op_setthreadprio, 1u << K_THREAD },
    { "PostQueuedCompletionStatus", op_postqueued, 1u << K_IOCP },
};
TEST(ho_ret_type_confusion_matrix) {
    REQUIRE(U(SetEvent)); REQUIRE(U(ResetEvent)); REQUIRE(U(PulseEvent)); REQUIRE(U(ReleaseSemaphore)); REQUIRE(U(ReleaseMutex));
    REQUIRE(U(SetWaitableTimer)); REQUIRE(U(CancelWaitableTimer)); REQUIRE(U(ReadFile)); REQUIRE(U(MapViewOfFile));
    REQUIRE(U(GetExitCodeProcess)); REQUIRE(U(GetExitCodeThread)); REQUIRE(U(QueryInformationJobObject)); REQUIRE(U(TerminateJobObject));
    REQUIRE(U(SetThreadPriority)); REQUIRE(U(PostQueuedCompletionStatus));
    kinds_make();
    for (unsigned o = 0; o < sizeof g_ops / sizeof *g_ops; o++) {
        if (!strcmp(g_ops[o].n, "ReleaseMutex")) continue;
        for (int k = 0; k < K_COUNT; k++) {
            if (!g_kh[k]) continue;
            if (k == K_PIPE && !strcmp(g_ops[o].n, "ReadFile")) continue;   /* could block on an empty pipe */
            if (g_ops[o].okmask & (1u << k)) continue;     /* the right type: covered by the positive tests */
            SET_ERR(0);
            BOOL r = g_ops[o].f(g_kh[k]); DWORD e = GetLastError();
            CHECKF(r == FALSE, "%s on %s handle succeeded", g_ops[o].n, g_kn[k]);
            CHECKF(e == ERROR_INVALID_HANDLE, "%s on %s: err=%lu want 6", g_ops[o].n, g_kn[k], e);
        }
    }
    kinds_free();
}
TEST(ho_ret_type_confusion_positive_controls) {
    kinds_make();
    for (unsigned o = 0; o < sizeof g_ops / sizeof *g_ops; o++) {
        if (!strcmp(g_ops[o].n, "ReleaseMutex")) continue;
        if (!strcmp(g_ops[o].n, "TerminateJobObject(empty)")) { }
        for (int k = 0; k < K_COUNT; k++) {
            if (!g_kh[k] || !(g_ops[o].okmask & (1u << k))) continue;
            if (k == K_PIPE && !strcmp(g_ops[o].n, "ReadFile")) continue;
            if (!strcmp(g_ops[o].n, "SetThreadPriority")) continue;  /* would change this thread's priority to the same value: fine, but skipped */
            SET_ERR(0);
            BOOL r = g_ops[o].f(g_kh[k]);
            CHECKF(r, "%s on %s failed err=%lu", g_ops[o].n, g_kn[k], GetLastError());
        }
    }
    kinds_free();
}
TEST(ho_ret_releasemutex_wrong_types) {
    REQUIRE(U(ReleaseMutex));
    kinds_make();
    for (int k = 0; k < K_COUNT; k++) {
        if (k == K_MUTEX || !g_kh[k]) continue;
        SET_ERR(0);
        EXPECT_EQ(U(ReleaseMutex)(g_kh[k]), FALSE);
        CHECKF(GetLastError() == ERROR_INVALID_HANDLE, "ReleaseMutex on %s", g_kn[k]);
    }
    SET_ERR(0);
    EXPECT_EQ(U(ReleaseMutex)(g_kh[K_MUTEX]), FALSE);   /* a mutex this thread does not own */
    EXPECT_ERR(ERROR_NOT_OWNER);
    kinds_free();
}
TEST(ho_ret_task_examples_explicit) {
    REQUIRE(U(SetEvent)); REQUIRE(U(ReleaseSemaphore)); REQUIRE(U(ReleaseMutex)); REQUIRE(U(ResetEvent));
    HANDLE s = CreateSemaphoreA(NULL, 0, 1, NULL), e = CreateEventA(NULL, TRUE, FALSE, NULL), m = CreateMutexA(NULL, FALSE, NULL);
    SET_ERR(0); EXPECT_EQ(U(SetEvent)(s), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(U(ReleaseSemaphore)(e, 1, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(U(ReleaseMutex)(e), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(U(ResetEvent)(m), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(s); CloseHandle(e); CloseHandle(m);
}
TEST(ho_ret_null_and_invalid_for_sync_ops) {
    REQUIRE(U(SetEvent)); REQUIRE(U(ResetEvent)); REQUIRE(U(ReleaseSemaphore)); REQUIRE(U(ReleaseMutex)); REQUIRE(U(SetWaitableTimer)); REQUIRE(U(CancelWaitableTimer));
    HANDLE bad[] = { NULL, (HANDLE)0x12344 };
    for (int i = 0; i < 2; i++) {
        SET_ERR(0); EXPECT_EQ(U(SetEvent)(bad[i]), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
        SET_ERR(0); EXPECT_EQ(U(ResetEvent)(bad[i]), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
        SET_ERR(0); EXPECT_EQ(U(ReleaseSemaphore)(bad[i], 1, NULL), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
        SET_ERR(0); EXPECT_EQ(U(ReleaseMutex)(bad[i]), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
        SET_ERR(0); EXPECT_EQ(U(CancelWaitableTimer)(bad[i]), FALSE); EXPECT_ERR(ERROR_INVALID_HANDLE);
    }
    SET_ERR(0); EXPECT_EQ(U(SetEvent)(INVALID_HANDLE_VALUE), FALSE);
    t_note("SetEvent(INVALID_HANDLE_VALUE) err=%lu", GetLastError());
}

/* ---- waits on unusual handle types ---- */
TEST(ho_wait_null_and_invalid) {
    REQUIRE(U(WaitForSingleObject)); REQUIRE(U(WaitForSingleObjectEx));
    SET_ERR(0); EXPECT_EQ(U(WaitForSingleObject)(NULL, 0), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(U(WaitForSingleObject)((HANDLE)0x12344, 0), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(U(WaitForSingleObjectEx)(NULL, 0, TRUE), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE);
    SET_ERR(0); EXPECT_EQ(U(WaitForSingleObject)(INVALID_HANDLE_VALUE, 0), WAIT_TIMEOUT);   /* (HANDLE)-1 is the current process */
}
TEST(ho_wait_pseudo_handles_timeout) {
    REQUIRE(U(WaitForSingleObject));
    EXPECT_EQ(U(WaitForSingleObject)(GetCurrentProcess(), 0), WAIT_TIMEOUT);
    EXPECT_EQ(U(WaitForSingleObject)(GetCurrentThread(), 0), WAIT_TIMEOUT);
    EXPECT_EQ(U(WaitForSingleObject)(GetCurrentProcess(), 10), WAIT_TIMEOUT);
}
TEST(ho_wait_non_waitable_types_fail_invalid_handle) {
    REQUIRE(U(WaitForSingleObject));
    HANDLE key = NULL, tok = NULL, map = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, 4096, NULL);
    HANDLE iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    char kn[80]; snprintf(kn, sizeof kn, "Software\\k32t_%lu", GetCurrentProcessId());
    LONG rc = RegCreateKeyExA(HKEY_CURRENT_USER, kn, 0, NULL, 0, KEY_ALL_ACCESS, NULL, (PHKEY)&key, NULL);
    BOOL ot = OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &tok);
    /* want: 5 for handles that lack SYNCHRONIZE (the access check comes first); the port has SYNCHRONIZE but is not waitable */
    struct { const char *n; HANDLE h; DWORD want; } t[] = { { "section", map, 5 }, { "iocp", iocp, 0 }, { "registry key", rc == 0 ? key : NULL, 5 }, { "token", ot ? tok : NULL, 5 } };
    for (int i = 0; i < 4; i++) {
        if (!t[i].h) { t_note("no %s handle", t[i].n); continue; }
        SET_ERR(0);
        DWORD r = U(WaitForSingleObject)(t[i].h, 0); DWORD e = GetLastError();
        if (!t[i].want) { t_note("%s: wait returned %lu err=%lu", t[i].n, r, e); continue; }   /* IOCP waitability not verified against ntoskrnl */
        CHECKF(r == WAIT_FAILED, "%s: wait returned %lu", t[i].n, r);
        if (t[i].want) CHECKF(e == t[i].want, "%s: err=%lu want %lu", t[i].n, e, t[i].want);
        else t_note("%s: wait err=%lu", t[i].n, e);
    }
    if (rc == 0) { RegCloseKey((HKEY)key); RegDeleteKeyA(HKEY_CURRENT_USER, kn); }
    if (ot) CloseHandle(tok);
    CloseHandle(map); CloseHandle(iocp);
}
TEST(ho_wait_job_and_timer_timeout) {
    REQUIRE(U(WaitForSingleObject));
    HANDLE j = CreateJobObjectA(NULL, NULL), t = CreateWaitableTimerA(NULL, TRUE, NULL);
    EXPECT_EQ(U(WaitForSingleObject)(j, 0), WAIT_TIMEOUT);
    EXPECT_EQ(U(WaitForSingleObject)(t, 0), WAIT_TIMEOUT);
    CloseHandle(j); CloseHandle(t);
}
TEST(ho_wait_file_handle_signals_after_overlapped_io) {
    REQUIRE(U(WaitForSingleObject));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "ovl.bin");
    HANDLE f = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_OVERLAPPED | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    CHECK(f != INVALID_HANDLE_VALUE);
    DWORD r0 = U(WaitForSingleObject)(f, 0);
    t_note("fresh overlapped file handle wait(0) = %lu", r0);
    OVERLAPPED ov; memset(&ov, 0, sizeof ov);
    DWORD n = 0;
    BOOL ok = WriteFile(f, "data", 4, &n, &ov);
    if (!ok && GetLastError() == ERROR_IO_PENDING) ok = GetOverlappedResult(f, &ov, &n, TRUE);
    CHECK(ok); EXPECT_EQ(n, 4);
    EXPECT_EQ(U(WaitForSingleObject)(f, 2000), WAIT_OBJECT_0);   /* the file object is signaled by I/O completion */
    CloseHandle(f);
}
TEST(ho_wait_sync_file_handle_does_not_fail) {
    REQUIRE(U(WaitForSingleObject));
    char p[MAX_PATH]; t_tmpfile(p, sizeof p, "syncwait.bin");
    HANDLE f = CreateFileA(p, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, NULL);
    SET_ERR(0);
    DWORD r = U(WaitForSingleObject)(f, 0);
    CHECKF(r == WAIT_OBJECT_0 || r == WAIT_TIMEOUT, "wait on file returned %lu", r);
    t_note("synchronous file wait(0) = %lu", r);
    DWORD n; WriteFile(f, "x", 1, &n, NULL);
    r = U(WaitForSingleObject)(f, 0);
    CHECKF(r == WAIT_OBJECT_0 || r == WAIT_TIMEOUT, "wait on file returned %lu", r);
    CloseHandle(f);
}
TEST(ho_wait_dir_change_handle_signals) {
    REQUIRE(U(WaitForSingleObject));
    char d[MAX_PATH]; t_tmpfile(d, sizeof d, "chg");
    CreateDirectoryA(d, NULL);
    HANDLE c = U(FindFirstChangeNotificationA)(d, FALSE, FILE_NOTIFY_CHANGE_FILE_NAME);
    CHECK(c != INVALID_HANDLE_VALUE); if (c == INVALID_HANDLE_VALUE) return;
    EXPECT_EQ(U(WaitForSingleObject)(c, 0), WAIT_TIMEOUT);
    char f[MAX_PATH]; snprintf(f, sizeof f, "%s\\new.txt", d);
    HANDLE h = CreateFileA(f, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); CloseHandle(h);
    EXPECT_EQ(U(WaitForSingleObject)(c, 5000), WAIT_OBJECT_0);
    FindNextChangeNotification(c);
    EXPECT_EQ(U(WaitForSingleObject)(c, 0), WAIT_TIMEOUT);
    FindCloseChangeNotification(c);
    DeleteFileA(f); RemoveDirectoryA(d);
}
TEST(ho_wait_multiple_with_one_invalid) {
    REQUIRE(U(WaitForMultipleObjects));
    HANDLE e = CreateEventA(NULL, TRUE, TRUE, NULL);
    HANDLE hs[2] = { e, (HANDLE)0x12344 };
    SET_ERR(0);
    DWORD r = U(WaitForMultipleObjects)(2, hs, FALSE, 0);
    t_note("WaitForMultipleObjects(valid signaled, invalid) = %lu err=%lu", r, GetLastError());
    HANDLE hs2[2] = { (HANDLE)0x12344, e };
    SET_ERR(0);
    r = U(WaitForMultipleObjects)(2, hs2, TRUE, 0);
    EXPECT_EQ(r, WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE);
    HANDLE hs3[2] = { NULL, e };
    SET_ERR(0);
    r = U(WaitForMultipleObjects)(2, hs3, TRUE, 0);
    EXPECT_EQ(r, WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_HANDLE);
    CloseHandle(e);
}
TEST(ho_wait_multiple_count_limits) {
    REQUIRE(U(WaitForMultipleObjects));
    HANDLE e = CreateEventA(NULL, TRUE, TRUE, NULL);
    static HANDLE hs[65]; for (int i = 0; i < 65; i++) hs[i] = e;
    SET_ERR(0); EXPECT_EQ(U(WaitForMultipleObjects)(65, hs, FALSE, 0), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    SET_ERR(0); EXPECT_EQ(U(WaitForMultipleObjects)(0, hs, FALSE, 0), WAIT_FAILED); EXPECT_ERR(ERROR_INVALID_PARAMETER);
    EXPECT_EQ(U(WaitForMultipleObjects)(64, hs, FALSE, 0), WAIT_OBJECT_0);
    CloseHandle(e);
}
