# P1 corrections, round 3 (p1c): summary

Branch p1c = p1b + 23 area branches p1c-<area> (tips, including audit follow-up commits; merged --no-ff; the other 35 areas are unchanged from p1b). No merge conflicts occurred (tools/classg.txt merged cleanly; entries the audits removed stay removed). Nothing is verified on Win10 yet: every new expectation is UNVERIFIED-until-rerun.

Baseline for this round is the p1b2 rerun of p1b (<results dir>): 58 areas, 13176 tests, 13093 OK, 52 FAIL, 31 SKIP, 0 NOTRUN (36 areas PASS, 22 FAIL).

## Build (mingw-w64, make -j4)
Source rsynced to <build dir> 58 exes built, 0 errors, rc 0. 21 warnings, identical to the baseline <build dir> once line and column numbers are stripped (no new warning lines). Log: <build dir>
Payload: <build dir> (58 areas, 13176 tests, same count as p1b), payload.sha256 verified, CORPUS_HEAD.txt written.

## Verdict totals (all 58 adjudication TSVs, 713 rows)
Rows superseded by a later row (SUPERSEDED labels of any spelling, "UNRESOLVED (superseded ...)") are counted separately. Row counts: EXPECT-FIX 438, GUARD 69, TOLERANCE 35, UNRESOLVED 80, CLASS-G 14, DELETE 7, INFO 1, SUPERSEDED 69. Row counts include historical rows that a later live row replaced without a SUPERSEDED label; the net per-test view (latest live row per test, R2:/P3: name prefixes folded, 494 tests): EXPECT-FIX 350, GUARD 64, TOLERANCE 34, UNRESOLVED 35, CLASS-G 5, DELETE 5, INFO 1. Compared with round 2 (53 UNRESOLVED distinct tests), 18 net fewer.

## Per area (rows per verdict)
| area | EXPECT-FIX | GUARD | TOLERANCE | UNRESOLVED | DELETE | CLASS-G | INFO | SUPERSEDED |
|---|---|---|---|---|---|---|---|---|
| appmodel_misc_kernelbase | 8 | 1 | 0 | 2 | 0 | 0 | 0 | 1 |
| async_io_iocp | 0 | 1 | 2 | 0 | 0 | 0 | 0 | 0 |
| atoms_profile_ini | 4 | 5 | 0 | 0 | 0 | 0 | 0 | 1 |
| console_apis | 35 | 8 | 1 | 9 | 0 | 5 | 0 | 17 |
| dir_find | 8 | 2 | 0 | 4 | 0 | 1 | 1 | 4 |
| environment_sysinfo | 7 | 0 | 0 | 2 | 0 | 0 | 0 | 1 |
| errors_exceptions_debug | 49 | 3 | 2 | 2 | 0 | 0 | 0 | 2 |
| file_attrs_ops | 5 | 2 | 1 | 1 | 0 | 0 | 0 | 3 |
| file_io | 9 | 1 | 0 | 4 | 0 | 0 | 0 | 2 |
| gap_00 | 8 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_01 | 4 | 1 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_02 | 15 | 0 | 1 | 0 | 0 | 0 | 0 | 3 |
| gap_03 | 3 | 0 | 1 | 4 | 0 | 0 | 0 | 0 |
| gap_04 | 10 | 1 | 0 | 0 | 0 | 0 | 0 | 3 |
| gap_05 | 7 | 3 | 1 | 0 | 0 | 0 | 0 | 0 |
| gap_06 | 10 | 0 | 1 | 0 | 0 | 0 | 0 | 0 |
| gap_07 | 4 | 6 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_09 | 5 | 3 | 0 | 0 | 0 | 0 | 0 | 3 |
| gap_10 | 11 | 4 | 3 | 0 | 0 | 4 | 0 | 0 |
| gap_11 | 5 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_12 | 17 | 2 | 2 | 2 | 0 | 0 | 0 | 0 |
| gap_13 | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_14 | 0 | 2 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_15 | 5 | 1 | 0 | 3 | 0 | 0 | 0 | 2 |
| gap_16 | 7 | 1 | 2 | 2 | 0 | 0 | 0 | 3 |
| gap_17 | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_18 | 3 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_19 | 5 | 1 | 1 | 0 | 0 | 0 | 0 | 0 |
| gap_20 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_22 | 7 | 1 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_24 | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_26 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_28 | 4 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_29 | 3 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| handles_objects | 7 | 0 | 0 | 5 | 2 | 0 | 0 | 0 |
| heap_local_global | 2 | 1 | 4 | 0 | 0 | 0 | 0 | 0 |
| job_objects | 14 | 0 | 1 | 7 | 0 | 0 | 0 | 0 |
| locale_nls | 47 | 2 | 2 | 1 | 0 | 0 | 0 | 1 |
| memory_virtual | 6 | 0 | 2 | 6 | 0 | 2 | 0 | 4 |
| modules_loader | 14 | 6 | 2 | 1 | 5 | 0 | 0 | 7 |
| path_names | 5 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| pipes_mailslots_comm | 4 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| process_create | 18 | 3 | 3 | 5 | 0 | 0 | 0 | 5 |
| registry_kernelbase | 13 | 1 | 0 | 0 | 0 | 0 | 0 | 0 |
| rtl_misc_kernel32 | 4 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| strings_codepages | 8 | 1 | 0 | 12 | 0 | 0 | 0 | 0 |
| sync_objects | 7 | 2 | 0 | 0 | 0 | 0 | 0 | 0 |
| threadpool_timers | 0 | 4 | 0 | 4 | 0 | 0 | 0 | 2 |
| threads_tls_fibers | 5 | 0 | 0 | 2 | 0 | 0 | 0 | 1 |
| time_clocks | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| toolhelp_psapi | 12 | 0 | 2 | 0 | 0 | 2 | 0 | 2 |
| volume_drive | 4 | 0 | 1 | 2 | 0 | 0 | 0 | 2 |

## Round 3 decisions worth knowing
- CLASS-G (tools/classg.txt, still 5 tests, same as round 2): console_apis con_write_cjk_two_cells, con_write_cjk_sequence; gap_10 gap10_qwsex_access_denied_limited, gap10_qws_access_rights (token-dependent access check, re-verified in MmQueryVirtualMemory); toolhelp_psapi ps_wswatch_self_in_child (count varies between runs).
- Audits rejected three new CLASS-G proposals as defects: memory_virtual mv_va_physical_flag_rules (deterministic 1450) and mv_va_huge_sizes (deterministic 487) became exact EXPECT-FIX checks; dir_find dir_create_reserved_device_name was a test bug (t_is_wine() clobbered the last error with 127 before GetLastError()), fixed, expected 267 unchanged, now GUARD; file_attrs_ops fsinfo_set_dispex_posix_semantics_removes_name_immediately was a test construction error (the POSIX delete handle was still open), test made stricter, now EXPECT-FIX.
- Audits promoted to EXPECT-FIX with a reference analysis cause: gap_04 gap4_hist_expunge_resets_length (conhost fail-fast on the first cooked read after expunge; ReadConsoleW expects 233, length query 0), environment_sysinfo wow64_invalid_handles (IsWow64Process(NULL) returns TRUE, error 0, w == FALSE; the w == FALSE part is unconfirmed), volume_drive vd_gvibh_zero_access_volume_handle and vd_ioctl_is_volume_mounted (direct-device open, IopParseDevice flag 8), gap_15 gap15_verfind_cur_buffer_small (cl == 0 explained by RtlUnicodeStringToAnsiString truncation).
- Evidence confirmed without code change: job_objects (TerminateJobObject posts no exit messages), modules_loader ac_force_early_deactivation_pops_stack (disassembly), handles_objects GetExitCodeProcess VDM fallback, threadpool_timers, strings_codepages SortFindString, gap_16, threads_tls_fibers, toolhelp_psapi.
- DELETE: 7 rows (5 distinct tests), unchanged in substance from round 2.
- process_create: the four rows that audits had put back to UNRESOLVED (pgec_invalid_handles, pexit_crash_raise_array_bounds, pty_* x3) now only add diagnostics (return/error/out values, 0x38 exit for "RaiseException returned", otype/itype in the PTYHELLO line).

## UNRESOLVED (35 distinct tests; need the Win10 rerun or reference analysis work)
- appmodel_misc_kernelbase: pwr_duplicate_handle_shares_request (kbx_misc.c:150)
- console_apis: con_hist_set_number_of_commands (alias.c:695-697)
- console_apis: con_vt_sgr_256_and_rgb (write.c:696,698)
- console_apis: con_write_wrap_off (write.c:266)
- console_apis: con_sbs_cursor_stays_in_bounds_when_grown (screen.c:463)
- console_apis: con_reada_utf8_cp (input.c:843)
- dir_find: cwd_set_null_w (dir_cwd.c:218)
- dir_find: notify_rdc_directory_events_need_dir_filter (change_notify.c:436 n>=1)
- errors_exceptions_debug: eed_hw_trap_flag_single_step (tests/errors_exceptions_debug/eed_exc.c:832)
- file_io: fio_dir_create_new_on_directory_name (access_share.c:363)
- file_io: fio_attr_flags_applied_to_new_file (access_share.c:277)
- gap_03: gap3_eventactivityid_get_set_swaps (tests/gap_03/etw.c:415)
- gap_03: gap3_genctrl_private_console (tests/gap_03/console.c:343 (child exit 0x157))
- gap_03: gap3_expunge_history_detached (tests/gap_03/console.c:80 (child exit 0x50))
- gap_03: gap3_console_no_console_invalid_handles (tests/gap_03/console.c:43 (child exit 0x2b))
- gap_12: gap12_con_window_relative_out_of_range (console.c:464 x3 (child))
- gap_12: gap12_con_window_absolute_out_of_buffer (console.c:410 x4, :414 x2 (child))
- gap_15: gap15_urlcanon_flags (gap15.h:75-78 (http://a/%20, URL_UNESCAPE 0x10000000))
- gap_16: gap16_conw_write_console_basic_controls_wrap_vt_codepages (console.c:74 (WriteConsoleA "\xe4\xb8\xad" at CP 65001 then ReadConsoleOutputCharacterW 1 cell at (0,1)))
- handles_objects: ho_err_thread_isolation (leaks.c:143)
- job_objects: jobset_preserve_job_time_rules (jo_info.c:632)
- locale_nls: uipref_set_thread_invalid_child (mui_nls.c:381 bit 0x2)
- memory_virtual: mv_va2_placeholder_flag_rules (misc.c:755)
- memory_virtual: mv_ww_read_does_not_dirty (misc.c:212)
- modules_loader: ld_ex_search_flags_dll_load_dir_with_altered_invalid (loadlib.c:282)
- process_create: pgec_invalid_handles (lifecycle.c:189)
- process_create: pexit_crash_raise_array_bounds (lifecycle.c:54)
- process_create: pty_child_output_arrives_on_pipe (conpty.c:57)
- process_create: pty_child_sees_a_console (conpty.c:66)
- process_create: pty_buffer_size_is_reported_to_child (conpty.c:75)
- strings_codepages: cpinfo_other_dbcs_note (tests/strings_codepages/cpinfo.c:45)
- strings_codepages: cmp_ex_invalid_locale_name (tests/strings_codepages/compare.c:161 (name 2))
- threadpool_timers: tp_timer_wait_no_cancel_runs_queued_callback (pool_timer.c:320)
- threadpool_timers: tp_wait_abandoned_mutex_result (pool_wait.c:241)
- threads_tls_fibers: stress_last_error_per_thread (stress.c:146)

## NOTRUN tests unblocked by guards
The p1b2 rerun had 0 NOTRUN, so no test is currently blocked from running. What round 3 changes in how tests run:
- gap_09 gap9_pss_walk_handles_contains_own_event, gap9_pss_query_bad_length, gap9_pss_capture_flag_combinations: the PssCaptureSnapshot(PSS_CAPTURE_HANDLES) access violation inside ntdll RtlCopyMemory is now contained in a child process (g9_pss_handles_child, modes walk, badlen, combos); the parent only checks the child exit code. These stop failing the area as soon as the child runs; the AV itself remains a finding, now recorded as GUARD.
- dir_find dir_create_reserved_device_name: error read right after the call; no longer UNRESOLVED.
- Diagnostic-only changes keep tests in the run without changing outcomes (process_create pgec/pexit/pty, gap_16 cjkdiag).

## Expected effect of a rerun (against the p1b2 baseline of 52 FAIL)
- Of the 52 p1b2 failures: 21 are targeted by EXPECT-FIX (15), GUARD (4) or TOLERANCE (2) rows and should turn OK (any that stay red mean the reading was wrong and the row goes back to UNRESOLVED); 26 are UNRESOLVED and expected to stay red (most have new diagnostics that give the missing evidence); 5 are CLASS-G and excluded from pass-rate comparison.
- Best case: about 31 FAIL left (26 UNRESOLVED + 5 CLASS-G), i.e. 13093 + 21 = about 13114 OK of 13176 with 31 SKIP.
- Items with unconfirmed parts: environment_sysinfo w == FALSE, gap_04 233/length 0, memory_virtual 1450/487, file_attrs_ops strengthened test; a red on these is information, not a regression.
- Tests OK in p1b2 must stay OK; a new FAIL there is a regression from round 3 (21 test source files changed). Total test count stays 13176.
