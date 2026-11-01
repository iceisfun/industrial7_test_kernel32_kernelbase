# kernel32 / kernelbase conformance corpus

Black-box tests for Windows 10 x64 `kernel32.dll` and `kernelbase.dll`. The functions under test are
resolved with `GetProcAddress` (`U(Name)`: kernel32 -> kernelbase -> ntdll; `UK`/`UB`/`UNT` pin a module,
which matters for forwarders), typed from the SDK headers.

    make                        # build/<area>.exe (mingw-w64)
    build\<area>.exe [substr]   # on Windows; optional test-name filter

Tests re-launch the exe as a child (`--child <name>`) when they need a separate process, and use a
private scratch directory `%TEMP%\k32t_<pid>\` that is removed on exit.
Output: `RUN/OK/FAIL/SKIP` per test, `  FAIL file:line expr (detail)` per failed check, `SUMMARY` last; exit 1 on failure.
`tools/kernel32_exports.txt` / `tools/kernelbase_exports.txt` list named exports.

## Areas

Each directory under `tests/` builds to `build/<area>.exe`. Test names are unique project-wide (prefixed by area).

| area | covers |
|---|---|
| core_smoke | harness sanity |
| appmodel_misc_kernelbase | AppModel / AppPolicy / package queries (classic desktop process), kernelbase-only helpers |
| async_io_iocp | overlapped I/O, completion ports, scatter/gather, pipe cancellation |
| atoms_profile_ini | global/local atoms, GetPrivateProfile*/WritePrivateProfile*, profile structs, WOW64 legacy exports |
| console_apis | console aliases, history, input/output/screen buffers, ConPTY (all in child processes with their own console) |
| dir_find | Create/RemoveDirectory, current directory (child only), FindFirstFile*, streams, change notification |
| environment_sysinfo | computer name, environment block/variables, version lies, system info, topology, firmware queries |
| errors_exceptions_debug | last-error, FormatMessage, SEH, unwind, UnhandledExceptionFilter, fail-fast, debugger API (own children only) |
| file_attrs_ops | attributes, times, CopyFile/MoveFile/Delete, links, final path, transacted-path error paths |
| file_io | CreateFile*, share modes, read/write/seek, locks, legacy _lread/OpenFile |
| handles_objects | handle duplication, inheritance, access rights, wrong-type handles, close semantics |
| heap_local_global | Heap*, Local*/Global* allocators, HeapSetInformation termination behaviour (child) |
| job_objects | job creation, limits, accounting, notifications, nesting (processes are always our own children) |
| locale_nls | locale info, date/time/number formatting, geo, calendars, time zones (read only), MUI |
| memory_virtual | VirtualAlloc*/Protect/Query, mappings, working set, NUMA, FromApp variants |
| modules_loader | LoadLibrary*, GetProcAddress, DLL directories (child), activation contexts, resources, MUI |
| path_names | GetFullPathName, long/short names, namespaces, PathCch*, system directories, temp names |
| pipes_mailslots_comm | anonymous/named pipes, mailslots, Comm* parsing and error paths (no real ports) |
| process_create | CreateProcess*, attribute lists, flags, I/O redirection, lifecycle, ConPTY |
| registry_kernelbase | Reg* in kernelbase under HKCU\Software\k32t_<pid>, plus read-only system keys |
| rtl_misc_kernel32 | Rtl-style exports, SLists, context APIs, legacy and FromApp memory wrappers |
| strings_codepages | Compare/Find/LCMap, MultiByteToWideChar/WideCharToMultiByte, normalization, code pages |
| sync_objects | events, mutexes, semaphores, timers, waits, WaitOnAddress, SRW/condvars, named-object namespaces |
| threadpool_timers | thread pool (new and legacy APIs), timer queues, callback instance helpers |
| threads_tls_fibers | threads, TLS, FLS, fibers, APCs, remote threads in own children |
| time_clocks | file/system time conversion, tick/performance counters, time-zone conversion with fixed biases |
| toolhelp_psapi | Toolhelp snapshots, K32/PSAPI process and module queries |
| volume_drive | drives, volumes, volume paths, free space, mount-point error paths |
| gap_00 .. gap_19 | remaining public surface (security descriptors/ACL/SID/token APIs, console setters and readers, Path*/Url*/Sh* shlwapi-style exports in kernelbase, StrTo*/Char*/lstr*, Wer*, Pss*, version info, PSAPI extras, VirtualAlloc2, SetXxx machine setters on safe paths, ...) |
| gap_20 .. gap_29 | undocumented / internal exports: presence, module ownership, forwarders, and only the calls proven side-effect free |

`COVERAGE.md` (regenerate with `python3 tools/gen_coverage.py > COVERAGE.md`) lists public surface covered and uncovered.

## Safety

The corpus is meant to be run on a machine that is also used for other things.

- Files only under `%TEMP%\k32t_<pid>\` (and read-only system files); registry writes only under `HKCU\Software\k32t_<pid>`.
- Process-global or irreversible state (current directory, DLL search path, error mode, environment, console, standard handles, affinity/priority, working-set limits) is changed only in a child process or restored exactly.
- Console tests run only in children created with `CREATE_NEW_CONSOLE` or `DETACHED_PROCESS`, or through ConPTY. The main process never calls `GenerateConsoleCtrlEvent`, `AttachConsole`, `FreeConsole` or `AllocConsole`.
- Never called (beyond argument-validation failure paths verified from the reference analysis, or presence checks): `SetSystemTime`, `SetLocalTime`, `SetTimeZoneInformation`, `SetDynamicTimeZoneInformation`, `SetComputerName*`, `SetVolumeLabel`, `DefineDosDevice` on existing devices, `SetSystemTimeAdjustment`, `SetSystemFileCacheSize`, `FatalAppExit*`, `ExitWindowsEx`, `Beep`/`MessageBeep`. `SetProcessDEPPolicy` runs in a child only. `SetThreadExecutionState` is always restored to `ES_CONTINUOUS`.
- Processes are only created, suspended, debugged or terminated if the test itself spawned them. Children have a hard timeout (30 s at most) and are killed on expiry.
- Resource bounds: under 1 GB memory per test, files under 64 MB, at most 256 threads, 8192 handles, 100000 loop iterations; every handle, file, key, mapping and process created is cleaned up.
- Behaviour that is version dependent or not verified against the reference analysis/docs is reported with `t_note()` rather than a hard CHECK.

## Running everything on Windows

1. Copy the `build\` folder, `run_all.bat` and `tools\` to the Windows 10 x64 machine (21H2/22H2, build 19041+).
2. Open a normal `cmd.exe` console window (interactive desktop session; not a service, scheduled-task or SSH session, because console and window-station tests need a real desktop) and run `run_all.bat`.
3. Output goes to `results\<area>.txt` (stdout) and `results\<area>.err`; a summary table is printed (`python tools\summarize.py results --failures` lists every failed check).

Budget about 10 minutes in total; antivirus real-time scanning can slow the file and process heavy areas (`file_attrs_ops`, `process_create`, `console_apis`, `job_objects`, `modules_loader`) considerably. Each exe is limited to 600 s by `run_all.bat`.

Elevation: run as an ordinary (non-admin) user for the primary result. Tests are written to accept either outcome where privilege decides (for example `ERROR_ACCESS_DENIED` versus success for registry/volume/firmware queries) and note which occurred; running elevated additionally exercises success paths (privilege-gated queries, `DefineDosDevice` with a unique K32T name) and is also safe. A few checks (`CreateSymbolicLink`, volume handle opens) report skips, not failures, without the privilege.

Exit code: each exe returns 1 when any test failed. Only a genuine Windows 10 result is authoritative: Wine runs are for harness validation, and wine-specific differences are guarded with `t_is_wine()`.
