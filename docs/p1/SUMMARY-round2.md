# P1 corrections, round 2 (p1b): summary

Branch p1b = p1 + 27 area branches p1b-<area> (tips, including audit follow-up commits; merged --no-ff). The only conflicts were in tools/classg.txt (resolved as the union of all sides; entries the audits removed stay removed). Nothing is verified on Win10 yet: every new expectation is UNVERIFIED-until-rerun.

Baseline for this round is the p1a rerun of p1 (<results dir>): 58 areas, 13176 tests, 13047 OK, 98 FAIL, 31 SKIP, 0 NOTRUN (30 areas PASS, 28 FAIL).

## Build (mingw-w64, make -j4)
Source rsynced to <build dir> 58 exes built, 0 errors, rc 0. 21 warnings, identical to the baseline <build dir> once line and column numbers are stripped (no new warning lines). Log: <build dir>
Payload: <build dir> (58 areas, 13176 tests, same test count as p1), payload.sha256 verified, CORPUS_HEAD.txt written.

## Verdict totals (all 58 adjudication TSVs, 645 rows)
Rows superseded by a later round-2 row (SUPERSEDED and "UNRESOLVED (superseded ...)") are counted separately and are not live. Live rows (592): EXPECT-FIX 419, UNRESOLVED 64 rows (53 distinct tests; a few tests have a round-1 and a round-2 row), GUARD 65, TOLERANCE 32, CLASS-G 6 rows (5 tests), DELETE 5, INFO 1. SUPERSEDED 53.

## Per area (rows per verdict)
| area | EXPECT-FIX | GUARD | TOLERANCE | UNRESOLVED | DELETE | CLASS-G | INFO | SUPERSEDED |
|---|---|---|---|---|---|---|---|---|
| appmodel_misc_kernelbase | 8 | 1 | 0 | 1 | 0 | 0 | 0 | 1 |
| async_io_iocp | 0 | 1 | 2 | 0 | 0 | 0 | 0 | 0 |
| atoms_profile_ini | 4 | 5 | 0 | 0 | 0 | 0 | 0 | 1 |
| console_apis | 34 | 8 | 1 | 4 | 0 | 3 | 0 | 17 |
| dir_find | 8 | 1 | 0 | 3 | 0 | 0 | 1 | 4 |
| environment_sysinfo | 6 | 0 | 0 | 1 | 0 | 0 | 0 | 1 |
| errors_exceptions_debug | 49 | 3 | 2 | 1 | 0 | 0 | 0 | 2 |
| file_attrs_ops | 4 | 2 | 1 | 1 | 0 | 0 | 0 | 2 |
| file_io | 9 | 1 | 0 | 2 | 0 | 0 | 0 | 2 |
| gap_00 | 8 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_01 | 4 | 1 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_02 | 15 | 0 | 1 | 0 | 0 | 0 | 0 | 3 |
| gap_03 | 3 | 0 | 1 | 4 | 0 | 0 | 0 | 0 |
| gap_04 | 9 | 1 | 0 | 1 | 0 | 0 | 0 | 0 |
| gap_05 | 7 | 3 | 1 | 0 | 0 | 0 | 0 | 0 |
| gap_06 | 10 | 0 | 1 | 0 | 0 | 0 | 0 | 0 |
| gap_07 | 4 | 6 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_09 | 5 | 0 | 0 | 3 | 0 | 0 | 0 | 0 |
| gap_10 | 11 | 4 | 3 | 0 | 0 | 2 | 0 | 0 |
| gap_11 | 5 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_12 | 17 | 2 | 2 | 2 | 0 | 0 | 0 | 0 |
| gap_13 | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_14 | 0 | 2 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_15 | 3 | 1 | 0 | 2 | 0 | 0 | 0 | 2 |
| gap_16 | 7 | 1 | 2 | 1 | 0 | 0 | 0 | 2 |
| gap_17 | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_18 | 3 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_19 | 5 | 1 | 1 | 0 | 0 | 0 | 0 | 0 |
| gap_20 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_22 | 7 | 1 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_24 | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_26 | 1 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_28 | 4 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_29 | 3 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| handles_objects | 6 | 0 | 0 | 4 | 2 | 0 | 0 | 0 |
| heap_local_global | 2 | 1 | 4 | 0 | 0 | 0 | 0 | 0 |
| job_objects | 11 | 0 | 1 | 6 | 0 | 0 | 0 | 0 |
| locale_nls | 47 | 2 | 2 | 1 | 0 | 0 | 0 | 1 |
| memory_virtual | 4 | 0 | 1 | 4 | 0 | 0 | 0 | 4 |
| modules_loader | 12 | 6 | 0 | 4 | 3 | 0 | 0 | 4 |
| path_names | 5 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| pipes_mailslots_comm | 4 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| process_create | 18 | 4 | 3 | 4 | 0 | 0 | 0 | 0 |
| registry_kernelbase | 13 | 1 | 0 | 0 | 0 | 0 | 0 | 0 |
| rtl_misc_kernel32 | 4 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| strings_codepages | 5 | 1 | 0 | 10 | 0 | 0 | 0 | 0 |
| sync_objects | 7 | 1 | 0 | 0 | 0 | 0 | 0 | 0 |
| threadpool_timers | 0 | 4 | 0 | 2 | 0 | 0 | 0 | 2 |
| threads_tls_fibers | 5 | 0 | 0 | 1 | 0 | 0 | 0 | 1 |
| time_clocks | 2 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| toolhelp_psapi | 12 | 0 | 2 | 0 | 0 | 1 | 0 | 2 |
| volume_drive | 2 | 0 | 1 | 2 | 0 | 0 | 0 | 2 |

SUPERSEDED labels are heterogeneous in the TSVs (SUPERSEDED, SUPERSEDED(UNRESOLVED), "UNRESOLVED (superseded by round-2 row)", ...); all are folded into the SUPERSEDED column.

## Round 2 decisions worth knowing
- CLASS-G (tools/classg.txt, 5 tests): console_apis con_write_cjk_two_cells, con_write_cjk_sequence (font/code-page dependent cell flags); gap_10 gap10_qwsex_access_denied_limited, gap10_qws_access_rights (access check depends on the run token: admin or SeProfileSingleProcessPrivilege, MiIsUserQueryVmCallerTrusted); toolhelp_psapi ps_wswatch_self_in_child (working-set watch count differs between runs).
- Audits moved four wrongly classified rows out of CLASS-G: errors_exceptions_debug eed_hw_trap_flag_single_step, volume_drive vd_gvibh_zero_access_volume_handle and vd_ioctl_is_volume_mounted (all back to UNRESOLVED), atoms_profile_ini atoms_global_name_empty_string (now GUARD: 127 was the harness t_is_wine() clobbering the last error). threadpool_timers entries were also kept out of classg.
- DELETE (5 rows): handles_objects 2, modules_loader 3 (round 1 and round 2); the round-2 addition is the ac_query_assembly_detailed_information ulFileCount check, replaced by a note because the observed value is 0 and the docs do not fix it.
- Corrected evidence: path_names (check 20 not check 10; =Q: with a trailing backslash gets a doubled separator, now EXPECT-FIX), async_io_iocp (1784 from ntfs!NtfsGetCompression), console_apis (b[15] must stay 0x7777), gap_02 (exact error code from the image subsystem version).

## UNRESOLVED (53 distinct tests; need the Win10 rerun or reference analysis work)
- appmodel_misc_kernelbase: pwr_duplicate_handle_shares_request (kbx_misc.c:149)
- console_apis: con_vt_sgr_256_and_rgb (write.c:695,697)
- console_apis: con_write_wrap_off (write.c:265)
- console_apis: con_sbs_cursor_stays_in_bounds_when_grown (screen.c:463)
- console_apis: con_reada_utf8_cp (input.c:843)
- dir_find: cwd_set_null_w (dir_cwd.c:218)
- dir_find: dir_create_reserved_device_name (dir_create.c:319 (CON COM1 LPT1 AUX PRN))
- dir_find: notify_rdc_directory_events_need_dir_filter (change_notify.c:435 n>=1)
- environment_sysinfo: wow64_invalid_handles (version.c:652 (line of the NULL check; both EXPECT_EQ and EXPECT_ERR fail))
- errors_exceptions_debug: eed_hw_trap_flag_single_step (tests/errors_exceptions_debug/eed_exc.c:832)
- file_attrs_ops: fsinfo_set_dispex_posix_semantics_removes_name_immediately (infoex_set.c:364 CHECKF(n != INVALID_HANDLE_VALUE))
- file_io: fio_dir_create_new_on_directory_name (access_share.c:363)
- file_io: fio_attr_flags_applied_to_new_file (access_share.c:277)
- gap_03: gap3_eventactivityid_get_set_swaps (tests/gap_03/etw.c:415)
- gap_03: gap3_genctrl_private_console (tests/gap_03/console.c:343 (child exit 0x157))
- gap_03: gap3_expunge_history_detached (tests/gap_03/console.c:80 (child exit 0x50))
- gap_03: gap3_console_no_console_invalid_handles (tests/gap_03/console.c:43 (child exit 0x2b))
- gap_04: gap4_hist_expunge_resets_length (con_alias.c:514,515)
- gap_09: gap9_pss_walk_handles_contains_own_event ((any; AV inside ntdll))
- gap_09: gap9_pss_query_bad_length ((any; AV inside ntdll))
- gap_09: gap9_pss_capture_flag_combinations ((any; AV inside ntdll))
- gap_12: gap12_con_window_relative_out_of_range (console.c:464 x3 (child))
- gap_12: gap12_con_window_absolute_out_of_buffer (console.c:410 x4, :414 x2 (child))
- gap_15: gap15_verfind_cur_buffer_small (ver_map.c:197 CHECK(cl==1))
- gap_15: gap15_urlcanon_flags (gap15.h:75-78 (http://a/%20, URL_UNESCAPE))
- gap_16: gap16_conw_write_console_basic_controls_wrap_vt_codepages (console.c:65 round2 (child 'wc' exit 65))
- handles_objects: ho_ret_type_confusion_matrix (returns.c:408)
- handles_objects: ho_err_thread_isolation (leaks.c:142)
- job_objects: jobset_preserve_job_time_rules (jo_info.c:622)
- job_objects: jobnotify_associate_after_assign (jo_notify.c:143)
- job_objects: jobnotify_multiple_children_counts (jo_notify.c:94)
- locale_nls: uipref_set_thread_invalid_child (mui_nls.c:381 bit 0x2)
- memory_virtual: mv_va_physical_flag_rules (valloc.c:314)
- memory_virtual: mv_va_huge_sizes (valloc.c:234)
- memory_virtual: mv_va2_placeholder_flag_rules (misc.c:755)
- memory_virtual: mv_ww_read_does_not_dirty (misc.c:212)
- modules_loader: ld_ex_search_flags_dll_load_dir_with_altered_invalid (loadlib.c:282)
- modules_loader: dd_set_nonexistent_dir_accepted (dlldir.c:122)
- modules_loader: dd_set_get_roundtrip (dlldir.c:74)
- modules_loader: ac_force_early_deactivation_pops_stack (actctx.c:508)
- process_create: pgec_invalid_handles (lifecycle.c:189)
- process_create: pty_child_output_arrives_on_pipe (conpty.c:57)
- process_create: pty_child_sees_a_console (conpty.c:66)
- process_create: pty_buffer_size_is_reported_to_child (conpty.c:75)
- strings_codepages: cpinfo_other_dbcs_note (tests/strings_codepages/cpinfo.c:45)
- strings_codepages: findnls_lcid_basic (tests/strings_codepages/compare.c:531-532)
- strings_codepages: findnls_ex_errors (tests/strings_codepages/compare.c:510)
- strings_codepages: cmp_ex_invalid_locale_name (tests/strings_codepages/compare.c:161 (name 2 L"en-US-garbage-more"))
- threadpool_timers: tp_timer_wait_no_cancel_runs_queued_callback (pool_timer.c:320)
- threadpool_timers: tp_wait_abandoned_mutex_result (pool_wait.c:241)
- threads_tls_fibers: stress_last_error_per_thread (stress.c:146)
- volume_drive: vd_gvibh_zero_access_volume_handle (volinfo.c:324 okz; 326 serial; 326 fs)
- volume_drive: vd_ioctl_is_volume_mounted (misc.c:138 ok (err 1); ret==0 (got 0xffff))

## NOTRUN tests unblocked by guards
The p1a rerun had no NOTRUN tests: the round-1 guards already released the tests that used to be NOTRUN (async_io_iocp 115, gap_10 290, threadpool_timers 13, dir_find 46, atoms_profile_ini tail). Round 2 adds no new unblock of that size. What round 2 adds:
- gap_10 gap10_badmem_unregister_null: UnregisterBadMemoryNotification(NULL) faulted inside ntdll (no NULL check before RtlUnsubscribeWnfStateChangeNotification) and failed the test; the call now runs on its own thread under a vectored handler, so the fault is contained and recorded.
- threadpool_timers tp_wait_abandoned_mutex_result: the status of the 0xC000070A handle exception is now printed from the test's own handler (the harness longjmps past later code).
- gap_09 gap9_pss_*: stage notes added to locate the AV inside ntdll; still UNRESOLVED, no guard.
- Guard-only changes keep tests in the run (no crash or hang expected): dir_find change-notify note, path_names drive-variable precondition (exit 77 if Q:\k32\dir is unreachable).

## Expected effect of a rerun (against the p1a baseline of 98 FAIL)
- Of the 98 p1a failures: 48 are targeted by EXPECT-FIX, GUARD, TOLERANCE or DELETE rows and should turn OK (any that stay red mean the reference analysis reading was wrong and the row goes back to UNRESOLVED); 45 are UNRESOLVED and are expected to stay red, with extra diagnostics (t_note values, stage notes) giving the missing evidence; 5 are CLASS-G and are excluded from pass-rate comparison.
- So the best case is roughly 48 fewer failures (about 50 FAIL left, of which 5 are CLASS-G). The 31 SKIPs are unchanged except where a precondition guard now skips (path_names, exit 77).
- 8 UNRESOLVED rows are for tests that passed in p1a (gap_03 x4, gap_12 x2, locale_nls uipref_set_thread_invalid_child, modules_loader ld_ex_search_flags_dll_load_dir_with_altered_invalid); they are residual round-1 rows, expected to stay OK.
- Tests that were OK in p1a must stay OK; any new FAIL there is a regression from round 2 (the changes touch 31 test source files).
- Total test count stays 13176; no tests were renamed or removed.
