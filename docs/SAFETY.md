# Static safety audit (before the first real-Windows run)

Scope: every `tests/*/*.c` (58 areas, 348 files) read for calls that could touch real system
state, other applications or the session. Static reading only: nothing was run. Every call is made
through `GetProcAddress`, so the audit followed the `U(Name)` / `UK` / `UB` / `UNT` call sites and
the `CHILD()` / `CT*()` bodies. Line numbers are for the commit this file is committed on.

Verdicts: SAFE (nothing risky, or every risky call is confined to a child process, the scratch
directory `%TEMP%\k32t_<pid>\` or a private `HKCU\Software\k32t_<pid>*` key); NEEDS-GUARD (a real
but low-probability or recoverable risk; the guard is named); DANGEROUS (cannot be run even with the
usual guards). Result: **45 SAFE, 13 NEEDS-GUARD, 0 DANGEROUS.** No test calls any of ExitWindowsEx,
InitiateSystemShutdown, LockWorkStation, SendInput/keybd_event/mouse_event, PostMessage/SendMessage
to foreign windows, service-control or ShellExecute APIs, enables a dangerous privilege on the
process token, terminates a foreign process, or assigns the runner to a job object.

## Run conditions every verdict assumes

The first Windows baseline must run in a disposable guest (overlay or snapshot discarded after the
run), headless, with a per-area hard timeout. `payload/run.ps1` provides:

| Guard | Where |
|---|---|
| Fresh per-area scratch tree; `TEMP`, `TMP` and the working directory point into it (covers relative-path writes such as `tests/gap_04/misc.c:598`) | run.ps1 |
| Hard per-area timeout, `taskkill /F /T`, sweep of leftover processes started from the payload copy | run.ps1 |
| stdin is `NUL` (stdin-reading children, `tests/process_create/children.c:162,186`) | run.ps1 |
| WER disabled (`Disabled`, `DontShowUI`, `LocalDumps` removed) for fail-fast and heap-corruption children | run.cmd |
| After each area: leftover scratch entries, leftover `HKCU\Software\k32t_*` keys (deleted), stragglers reported on an `I7TC leak` line | run.ps1 |
| Before/after `I7TC state` lines: computer name (pending and active), time zone, `PendingFileRenameOperations` count, mountvol line count, free MB on C:, user locale and geo | run.ps1 |

Prerequisites for the guest: NTFS system volume with at least 6 GB free (several tests set the end
of file of a scratch file to 4-5 GiB, see below), a session where `CREATE_NEW_CONSOLE` works
(console children create hidden or minimized consoles; they need no interactive user), no network
required (a few tests resolve a non-existent host name and wait for the resolver, bounded to 25-30 s).

## Verdict table

| Area | Verdict | Evidence and guard |
|---|---|---|
| appmodel_misc_kernelbase | SAFE | `SetProcessMitigationPolicy` only with failing argument sets in the main process (`kbx_process.c:216-260`), effective calls in `kbx_mit_set` (`:262-292`); `SetSystemFileCacheSize` is export-checked only (`:321-323`, `kbx_misc.c:1054`); `DeviceIoControl` on scratch files only (`kbx_files.c:589-712`); `\\.\PhysicalDrive0` opened with access 0 (`kbx_files.c:326`). |
| async_io_iocp | SAFE | Scratch files and `\\.\pipe\k32t_aio_*_<pid>`; FSCTLs on scratch files (`ex_scatter.c:320-410`). Unbounded `GetOverlappedResult(TRUE)` / `GetQueuedCompletionStatus(INFINITE)` (`pipe_cancel.c:43,55,86,232,284,422,428`, `file_basic.c:174-530`, `iocp_threads.c:79`) and a blocking `ReadFile` (`pipe_cancel.c:308`): the per-area timeout is the only containment. |
| atoms_profile_ini | SAFE | Every `WritePrivateProfile*` takes a scratch path; no `WriteProfile*` is called; win.ini is only read; `SetCurrentDirectory` in child `ini_relative` only (`prof_misc.c:171-210`). Global atoms are named `k32t<pid>_*` and deleted. |
| console_apis | SAFE | All ~500 tests run in children with their own console (`CREATE_NEW_CONSOLE`, detached, no-window or ConPTY; `ch.h` `prun_ex`, 25 s kill). `GenerateConsoleCtrlEvent` with group 0 (`alias.c:836-909,931,961-1033`) reaches only that child's console. Guard: none beyond the area timeout. |
| core_smoke | SAFE | Three tests: pid, child round trip, one scratch file. |
| dir_find | NEEDS-GUARD | `find_basic.c:325-327`: 5 GiB `SetEndOfFile` on a scratch file, needs 6 GB free on the scratch volume (or skip `find_struct_large_size_high_part`). `dir_cwd.c:292-297,305`: child `SetCurrentDirectory` to `\\localhost\C$` and `\\k32t-no-such-host-7731\share` (SMB/DNS wait, 25 s child cap). All `SetCurrentDirectory` / `SetEnvironmentVariable` calls are in children. |
| environment_sysinfo | SAFE | Environment edits on `K32T*` names, process-local and deleted (`envvar.c`, `extra.c`); whole-block replacement in `CHILD` bodies (`envblock.c:567-625`); `SetThreadExecutionState` always reset to `ES_CONTINUOUS` (`firmware_misc.c:408-472`); firmware reads only (`firmware_misc.c:47-100`). |
| errors_exceptions_debug | NEEDS-GUARD | Deliberate crashes, `int 0x29`, `RaiseFailFastException`, `abort()` in children (`eed_crash.c:47-170`) can start WER (report or dump). Guard: WER disabled by run.cmd. Debugger APIs only on own children (`eed_dbg.c:36-75,654-672,810-880`). Main-process exception tests rely on vectored handlers; a wrong one ends the exe (area timeout). |
| file_attrs_ops | NEEDS-GUARD (low) | `MOVEFILE_DELAY_UNTIL_REBOOT|0x10` on scratch paths (`move_delete.c:246,383`); expected rejected before the registry, otherwise a `PendingFileRenameOperations` entry. Guard: `I7TC state` pendingrename count before/after. 4-5 GiB sparse files in scratch (`links_misc.c:366-368`, `attrs_get.c:430-431`): NTFS scratch with 6 GB free. |
| file_io | SAFE | All writes via `t_tmpfile`; zero-access opens of `\\.\PhysicalDrive0`, `\\.\C:`, COM1/COM2/LPT1 with no IOCTL (`access_share.c:553-646`); 8-12 GB `SetFilePointer` moves only the pointer (`extra.c:121-148`). |
| gap_00 | SAFE | `AdjustTokenPrivileges` only on duplicated impersonation tokens (`token_sid.c:6-12,40-300`); `AllocConsole`/`FreeConsole`/`AddConsoleAlias` in children (`console_app.c:19-262`); `*AndAuditAlarm` may write a few Security log entries (`access_check.c:628-690`). |
| gap_01 | SAFE | `AttachConsole`/`FreeConsole` only in `DETACHED_PROCESS` / `CREATE_NEW_CONSOLE` children (`console.c:15-154`); `SetThreadToken` on a helper thread (`access_sid.c:395-404`). |
| gap_02 | SAFE | `DefineDosDevice` only with invalid flag combinations expected to fail (`proc.c:205-296`); `CreateProcessAsUser` re-launches this exe (`proc.c:6-200`); Ds* calls in children (`proc.c:536-720`); `CveEventWrite` with fake ids writes event-log entries (`misc.c:638-690`). |
| gap_03 | SAFE | CTRL events only inside a `CREATE_NEW_CONSOLE` child (`console.c:332-370`); `FatalAppExit`/`FatalExit` are export-checked, never called (`misc.c:353-372`); terminates only its own sleepers. |
| gap_04 | SAFE | All console tests in children (`ch.h:219-261`); `DeleteFileA("sddot")` is a relative path (`misc.c:598`), covered by cwd = scratch. |
| gap_05 | SAFE | Console work in `gap5_con_own` / `gap5_con_detached` children (`con.c:31-231`); firmware variable read only (`misc.c:218-260`); profile writes and `CopyFile` of kernel32.dll go to scratch. |
| gap_06 | SAFE | Impersonation of a duplicate of the own token with `RevertToSelf` (`impersonate.c:105-225`); `InstallELAMCertificateInfo` only with bad handles (`misc.c:564-575`); tape APIs get bogus or file handles. |
| gap_07 | SAFE | Token work on duplicates and helper threads (`sec.c:554-717`); no machine setters; enclave calls fail immediately (`misc.c:143-207`). |
| gap_08 | SAFE | Pure Path*/string functions; no file, registry or process APIs. |
| gap_09 | NEEDS-GUARD | `gap9_preparetape_physicaldrive_zero_access` (`misc.c:716-724`) opens `\\.\PhysicalDrive0` with access 0 and calls `PrepareTape`; safety rests on the I/O manager rejecting the ioctl on a zero-access handle. Guard: skip the test (or run only in the disposable guest). `PssCaptureSnapshot(VA_CLONE)` clones the runner and frees it (`pss.c:119-122`). |
| gap_10 | SAFE | Console tests in minimized `CREATE_NEW_CONSOLE` children (`g10core.c:84-97`); registry only under `HKCU\Software\k32t_<pid>_g10` (`sh_usk.c:16-26`), deleted at exit; stale key swept by run.ps1. |
| gap_11 | NEEDS-GUARD | `SetComputerNameW/A`, `SetComputerNameExW/A` with names meant to be rejected (`misc.c:351-489`, `gap11_cname_*`; types 4/5/6 at `:433-445,470-480`): if Windows 10 accepted one, a pending computer or DNS name would persist. Guard: `I7TC state` computername before/after; optionally skip `gap11_cname_*`. `SetCalendarInfo` only with invalid arguments (`misc.c:225-349`). Registry only under `HKCU\Software\k32t_<pid>_g11`. |
| gap_12 | NEEDS-GUARD | `gap12_file_device_name` (`sd_obj.c:482-487`): `SetFileSecurityW(L"NUL", DACL, world FILE_ALL_ACCESS)` in the main process; elevated it can rewrite the DACL of the NUL device until reboot (test accepts TRUE or FALSE). Guard: disposable guest or skip the test. `SetLocaleInfoW/A` with bogus locales/types (`misc.c:511-570`, includes real `0x409` at `:520-531,552-562`): `I7TC state` userlocale before/after. `SetKernelObjectSecurity` on guessed handle values (`sd_obj.c:78-86`). Firmware setters only with NULL/malformed GUIDs (`misc.c:763-840`). |
| gap_13 | SAFE | `SetSystemTimeAdjustment`, `SetTimeZoneInformation`, `SetUserGeoID` presence-checked only (`setters.c:503-545`); `SetUserGeoName(current)` (`:524-536`) at worst rewrites the same HKCU value; `SetTokenInformation` only on bogus handles or duplicates. |
| gap_14 | SAFE | Str* helpers and ETW stubs with bogus handles (`misc.c:216-296`). |
| gap_15 | SAFE | String/URL functions, `VerFindFile`, mappings of its own. |
| gap_16 | SAFE | WER registration in child `gap16_wer` (`wer.c:6-261`); console in children; `VirtualFreeEx(SELF, 0x10000000000 ...)` expects failure (`mem.c:251-252`, would only matter if that address were a live allocation; skip `gap16_vfex_release_never_allocated` for an airtight run). |
| gap_17 | SAFE | Balanced `timeBeginPeriod`/`timeEndPeriod` (`winmm_time.c:176-250`). |
| gap_18 | NEEDS-GUARD | `DefineDosDevice` of `K32T_G18_<pid>` (with `DDD_NO_BROADCAST_SYSTEM`) removed afterwards (`volume.c:84-109`); `volume.c:16,90` unknown-flag loop targets `C:\` without NO_BROADCAST. `gap18_setvmp_existing_mount_point_refused` (`volume.c:208-217`) calls `SetVolumeMountPointA` on the real system drive root with a bogus GUID (expected refusal). Guard: skip that test or rely on the disposable guest. 5 GiB `SetEndOfFile` (`misc.c:399-402`): 6 GB free. DELAY_UNTIL_REBOOT cases (`move.c:261-264,378`). |
| gap_19 | SAFE | Registry in `HKCU\Software\k32t_<pid>` only (`reg.c:19-37`); `RegLoadKey` into HKU/HKLM with a missing scratch file (`reg.c:508-565`); `SetDefaultCommConfigW` only with non-COM names. |
| gap_20 | NEEDS-GUARD (low) | `gap20_addlocalaltcomputername_*` (`objects.c:404-424`): NULL name / non-zero flags expected rejected; if accepted an alternate computer name would be registered. Guard: `I7TC state` computername, or skip those tests. `BaseMarkFileForDelete` on scratch files only. |
| gap_21 | SAFE | Export presence plus three pure functions. |
| gap_22 | SAFE | Argument-validation calls on non-public exports; package queries may make an AppX RPC call (`internal_exports.c:130-420`). |
| gap_23 | SAFE | Export lookups and three read-only calls (`internal.c:84-110`). |
| gap_24 | NEEDS-GUARD (low) | `NlsUpdateLocale` / `NlsUpdateSystemLocale` with flags expected invalid (`exports.c:226-245`, tests `gap24_nls_update_locale_error_paths`, `gap24_nls_update_system_locale_error_paths`): if accepted they would change locale state. Guard: disposable guest, or skip the two tests. |
| gap_25 | SAFE | Export presence plus `PathIsValidChar`. |
| gap_26 | SAFE | Only `HKCU\Software\k32t_<pid>_g26[m]`, always deleted (`internal26.c:192-290`); risky exports resolved, never called (`:300-323`). |
| gap_27 | SAFE | Export/PE-header checks; `SetClient*TimeZoneInformation` and similar are never called (`internal27.c:6`). |
| gap_28 | SAFE | Export checks; `_exit`/`_cexit` in children (`internal28.c:266-292`); `_invalid_parameter()` in the main process (`:259-263`) at worst crashes the exe. |
| gap_29 | SAFE | `exit`/`atexit`/`_purecall` only in `t_run_child` children, with an `ExitProcess` vectored handler (`internal29.c:675-770`). |
| handles_objects | SAFE | Only `HKCU\Software\k32t_<pid>` (`access.c:583-608`, `returns.c:486-498`); toggles the inherit flag on stderr and restores it (`handle_info.c:326-337`); `CloseHandle` on guessed values guarded by liveness checks (`close_handle.c:38-46`). Unbounded pipe reads in the main process (`inherit.c:117-119,196,345`, `dup_handle.c:505`): area timeout. |
| heap_local_global | NEEDS-GUARD | Heap-corruption children expected to die with `0xC0000374` (`heap_info.c:246-312`, `heap_basic.c:734-749`) can queue a WER report or `LocalDumps` file. Guard: WER disabled by run.cmd. Largest allocation 48 MB live, 256 MB reserve-only (`lg_specific.c:492`). |
| job_objects | SAFE | No `AssignProcessToJobObject` on the runner; `GetCurrentProcess()` is assigned only inside children `jo_nest`/`jo_nest2` (`jc_children.c:119,135,136`); KILL_ON_JOB_CLOSE and active-process limits only on jobs holding spawned children (`jo_limits.c:20-90,196,521-586`). Children may commit 128-256 MB when a limit is not enforced (`jo_limits.c:154,197,258,272`): guest needs 1 GB free commit. |
| locale_nls | SAFE | `SetThreadLocale` and the preferred-UI-language setters only in children (`loc_names.c:497-560`, `mui_nls.c:315-430`); no time-zone or geo setters. |
| memory_virtual | SAFE | Own-process or own-child memory only; real commits up to 128 MB / 96 MB (`misc.c:126,1008`, `procmem.c:784`), above the 64 MB line but in this single area; reserve-only 256 MB-16 GB (`valloc.c:247-254`); remote writes only into `pm_sleeper` children (`procmem.c:438-700`). |
| modules_loader | SAFE | `SetDllDirectory`/`SetDefaultDllDirectories`/cwd/PATH edits only in children (`dlldir.c:24-535`); `UpdateResource` only on scratch copies; remote thread only into the test's own child (`loadlib2.c:304-331`). |
| path_names | SAFE | cwd, environment and `SetSearchPathMode` in children (`fullpath2.c:12-129`, `sysdirs.c:393-521`); `GetTempFileName` with non-zero unique (no file created, `temp.c:277-284`); loopback/missing UNC probes in children (`namespace.c:213-228,411-432`). |
| pipes_mailslots_comm | SAFE | `\\.\pipe\k32t_<pid>_*` and mailslots only; COM names opened with access 0 or not at all (`comm_handles.c:282-313`); every Comm call targets a file, pipe or NULL (`:9-30`). |
| process_create | NEEDS-GUARD | Crash children (AV, `int3`, stack overflow, `int 0x29`, `RaiseFailFastException`) can reach WER and stretch past the 25 s child cap (`children.c:28-41`, `lifecycle.c:56-72`); stdin readers inherit the runner's stdin (`children.c:162,186`, `pc.h:121-124`). Guard: WER disabled and stdin `NUL` (both in the runner). Job objects with KILL_ON_JOB_CLOSE only in own children (`children.c:241-262`, `attrlist.c:510-519`); `TerminateProcess` only on own children or grandchildren; `OpenProcess` on pid 4/0 is query-only (`lifecycle.c:304-319`). |
| registry_kernelbase | SAFE | All writes under `HKCU\Software\k32t_<pid>` with an atexit cleanup (`rk.h`); `RegOverridePredefKey` only in children (`system.c:300-364`); `RegSaveKey/RestoreKey/LoadKey/ReplaceKey` on scratch or missing files without enabling backup/restore privileges (`system.c:369-465`); a synchronous `RegNotifyChangeKeyValue` worker has no timeout (`notify.c:163-180`). Stale keys swept by run.ps1. |
| rtl_misc_kernel32 | SAFE | Exceptions that can end a process run in children with `SEM_NOGPFAULTERRORBOX` (`rtl_ctx.c:841,863`); function-table tests on a private RWX page, undone (`rtl_ctx.c:372-600`); TxF only on scratch paths (`transacted.c:374-430`); one 4 GB `SEC_RESERVE` mapping closed at once (`fromapp.c:25`). |
| strings_codepages | SAFE | Pure string and code-page calls; guard-page `IsBad*` probes on private pages (`muldiv_isbad.c:188-370`). |
| sync_objects | SAFE | In-process primitives and own children; `SetWaitableTimer(fResume=TRUE)` 30 ms (`timers.c:445`); the `INFINITE` waits are on signaled objects or have a waker (`waits.c`, `addr_slist.c:167`, `locks.c:682`). |
| threadpool_timers | SAFE | Crash/exit cases in children (`exc.c:6-9,75`; fail-fast variants `:57-170` may reach WER); pipes `k32t_tpio_*_<pid>`; up to 10000 queued items (`stress.c`). Callback waits with `INVALID_HANDLE_VALUE` hang only if a callback never completes (area timeout). |
| threads_tls_fibers | SAFE | `CreateRemoteThread` / `TerminateProcess` only against the `tf_sleeper` child (`thr_remote.c:35-110`); 1 TB stack/fiber sizes fail at commit (`thr_create.c:277-292`); `SleepEx(INFINITE)` is in a joined worker (`apc_sleep.c:139`, `tf.h:49-56`). |
| time_clocks | NEEDS-GUARD (low) | `timemisc_setsystemtime_invalid_87`, `timemisc_setlocaltime_invalid_87`, `timemisc_clock_unchanged_after_invalid_set` (`misc.c:12-47`) call `SetSystemTime`/`SetLocalTime` in the main process with 12+6 invalid values; field validation rejects them first. `SetTimeZoneInformation` and `SetSystemTimeAdjustment` are not called. Guard: disposable guest with host time sync not under test; `I7TC state` tz before/after. |
| toolhelp_psapi | SAFE | `TerminateProcess` only on own `thp_sleeper` children (`thp.h:70-79`); read-only `OpenProcess` on snapshot pids and pid 4; 64 MB commit (`ps_mem.c:109`). |
| volume_drive | NEEDS-GUARD | Volume-mutating APIs aimed at the system drive with arguments that must be rejected: `DeleteVolumeMountPointA/W("C:")` (`misc.c:256-265`, test `vd_delmountpoint_null_and_no_backslash`), `SetVolumeMountPointW/A("C:\\", <C: volume GUID>)` (`misc.c:266-273`, `extra.c:24-33`), `DefineDosDeviceW/A` on `K32T:` without `DDD_NO_BROADCAST_SYSTEM` (`misc.c:224-255`), `SetVolumeLabelW/A` on a free letter (`misc.c:281-289`). Read-only `DeviceIoControl` on zero-access `\\.\C:`, volume GUID and `PhysicalDrive0` (`misc.c:114-215`). Guard: disposable guest (or skip those tests); drive-letter probing can stall on a stale network or optical drive (area timeout). |

## Cross-cutting findings

1. **WER**: fail-fast and heap-corruption children (process_create, errors_exceptions_debug,
   heap_local_global, threadpool_timers, job_objects `jo_crash`) bypass `SEM_NOGPFAULTERRORBOX`
   and can start WerFault. run.cmd disables WER for the guest session.
2. **Leaks after a kill**: `HKCU\Software\k32t_*` keys (registry_kernelbase, gap_10, gap_11,
   gap_19, gap_26, handles_objects), scratch directories, `K32T:` / `K32T_G18*` DOS devices,
   leftover 25 s sleeper children. run.ps1 removes the registry keys and scratch trees and
   kills leftover payload processes; DOS devices and the pending-rename list are only
   reported through `I7TC state` (they vanish with the disposable guest).
3. **Disk**: 4-5 GiB `SetEndOfFile` on scratch files (dir_find `find_basic.c:325-327`,
   file_attrs_ops `links_misc.c:366-368`, `attrs_get.c:430-431`, gap_18 `misc.c:399-402`);
   needs 6 GB free on an NTFS scratch volume or these tests fail on space (harmless).
4. **Memory**: every committed allocation is at most 128 MB (memory_virtual `misc.c:126,1008`,
   `procmem.c:784`; job_objects children up to 256 MB only when a job memory limit is not
   enforced). Over the 64 MB planning cap in P1: memory_virtual and job_objects.
5. **Hangs**: no main-process wait without any bound except those listed under async_io_iocp,
   handles_objects and registry_kernelbase `notify.c:163-180`; the per-area timeout is the
   containment. The harness has no per-test timeout (P3).
6. **Network**: path_names and dir_find probe `\\localhost\C$\` and a non-existent host, each in a
   child with a 25-30 s cap; no data is sent anywhere else.

## Optional per-test skips for the strictest run

None is required for the baseline in a disposable guest. If a stricter posture is wanted, the
tests to skip are: `gap9_preparetape_physicaldrive_zero_access`, `gap12_file_device_name`,
`gap18_setvmp_existing_mount_point_refused`, `vd_delmountpoint_null_and_no_backslash`,
`gap11_cname_*`, `gap20_addlocalaltcomputername_*`, `gap24_nls_update_locale_error_paths`,
`gap24_nls_update_system_locale_error_paths`, `gap16_vfex_release_never_allocated`. The harness has
no exclusion filter yet (its single argument is a positive substring filter); adding one is P3.
