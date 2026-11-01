# P1 corrections: summary

Branch p1 = origin/master + 52 area branches p1-<area> (tips, including audit follow-ups; no merge conflicts) + p0-payload (payload/ and tools/make_payload.sh). Nothing is verified on Win10 yet: all new expectations are UNVERIFIED-until-rerun.

## Build (mingw-w64, make -j4)
58 exes built (52 area TSVs plus pre-existing areas), 0 errors, 21 warnings, identical to the baseline build-new.log (no new warning lines after stripping line numbers). Payload: <build dir> (58 areas, 13176 tests), payload.sha256 verified, CORPUS_HEAD.txt written.

## Verdict totals (all adjudication TSVs)
EXPECT-FIX 391, UNRESOLVED 66, GUARD 61, TOLERANCE 28, DELETE 4, CLASS-G 3, INFO 1 (554 rows).

## Per area
| area | EXPECT-FIX | GUARD | TOLERANCE | UNRESOLVED | DELETE | CLASS-G | INFO |
|---|---|---|---|---|---|---|---|
| appmodel_misc_kernelbase | 8 | 1 | 0 | 1 | 0 | 0 | 0 |
| async_io_iocp | 0 | 1 | 0 | 0 | 0 | 0 | 0 |
| atoms_profile_ini | 4 | 4 | 0 | 0 | 0 | 0 | 0 |
| console_apis | 31 | 8 | 1 | 8 | 0 | 3 | 0 |
| dir_find | 5 | 1 | 0 | 4 | 0 | 0 | 1 |
| environment_sysinfo | 6 | 0 | 0 | 1 | 0 | 0 | 0 |
| errors_exceptions_debug | 48 | 3 | 3 | 1 | 0 | 0 | 0 |
| file_attrs_ops | 4 | 1 | 1 | 2 | 0 | 0 | 0 |
| file_io | 9 | 1 | 0 | 2 | 0 | 0 | 0 |
| gap_00 | 8 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_01 | 4 | 1 | 0 | 0 | 0 | 0 | 0 |
| gap_02 | 12 | 0 | 1 | 3 | 0 | 0 | 0 |
| gap_03 | 3 | 0 | 1 | 4 | 0 | 0 | 0 |
| gap_04 | 9 | 1 | 0 | 0 | 0 | 0 | 0 |
| gap_05 | 7 | 3 | 1 | 0 | 0 | 0 | 0 |
| gap_06 | 10 | 0 | 1 | 0 | 0 | 0 | 0 |
| gap_07 | 4 | 6 | 0 | 0 | 0 | 0 | 0 |
| gap_09 | 5 | 0 | 0 | 3 | 0 | 0 | 0 |
| gap_10 | 0 | 1 | 0 | 0 | 0 | 0 | 0 |
| gap_11 | 5 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_12 | 17 | 2 | 2 | 2 | 0 | 0 | 0 |
| gap_13 | 2 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_14 | 0 | 2 | 0 | 0 | 0 | 0 | 0 |
| gap_15 | 3 | 1 | 0 | 2 | 0 | 0 | 0 |
| gap_16 | 6 | 1 | 2 | 2 | 0 | 0 | 0 |
| gap_17 | 2 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_18 | 3 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_19 | 5 | 1 | 1 | 0 | 0 | 0 | 0 |
| gap_20 | 1 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_22 | 7 | 1 | 0 | 0 | 0 | 0 | 0 |
| gap_24 | 2 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_26 | 1 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_28 | 4 | 0 | 0 | 0 | 0 | 0 | 0 |
| gap_29 | 3 | 0 | 0 | 0 | 0 | 0 | 0 |
| handles_objects | 6 | 0 | 0 | 2 | 2 | 0 | 0 |
| heap_local_global | 2 | 1 | 4 | 0 | 0 | 0 | 0 |
| job_objects | 11 | 0 | 1 | 3 | 0 | 0 | 0 |
| locale_nls | 44 | 3 | 2 | 1 | 0 | 0 | 0 |
| memory_virtual | 4 | 0 | 1 | 4 | 0 | 0 | 0 |
| modules_loader | 12 | 6 | 0 | 5 | 2 | 0 | 0 |
| path_names | 4 | 0 | 0 | 0 | 0 | 0 | 0 |
| pipes_mailslots_comm | 4 | 0 | 0 | 0 | 0 | 0 | 0 |
| process_create | 18 | 4 | 3 | 4 | 0 | 0 | 0 |
| registry_kernelbase | 12 | 1 | 0 | 0 | 0 | 0 | 0 |
| rtl_misc_kernel32 | 4 | 0 | 0 | 0 | 0 | 0 | 0 |
| strings_codepages | 5 | 1 | 0 | 5 | 0 | 0 | 0 |
| sync_objects | 7 | 1 | 0 | 0 | 0 | 0 | 0 |
| threadpool_timers | 0 | 4 | 0 | 2 | 0 | 0 | 0 |
| threads_tls_fibers | 5 | 0 | 0 | 1 | 0 | 0 | 0 |
| time_clocks | 2 | 0 | 0 | 0 | 0 | 0 | 0 |
| toolhelp_psapi | 11 | 0 | 2 | 2 | 0 | 0 | 0 |
| volume_drive | 2 | 0 | 1 | 2 | 0 | 0 | 0 |

## UNRESOLVED (66 rows; need the Win10 rerun or reference analysis work)
Test / site, by area:

- appmodel_misc_kernelbase: pwr_duplicate_handle_shares_request (kbx_misc.c:148)
- console_apis: con_vt_sgr_256_and_rgb (write.c:681)
- console_apis: con_vt_sgr_256_and_rgb (write.c:683)
- console_apis: con_write_wrap_off (write.c:262)
- console_apis: con_wco_invalid_handles (screen.c:1375 (x2))
- console_apis: con_sbs_cursor_stays_in_bounds_when_grown (screen.c:446)
- console_apis: con_reada_utf8_cp (input.c:842)
- console_apis: con_title_original_buffer_sizes (basic.c:705)
- console_apis: con_title_original_matches_start_title (basic.c:684,689)
- dir_find: cwd_set_null_w (dir_cwd.c:218 (child line))
- dir_find: dir_create_length_over_max_path (dir_create.c:414)
- dir_find: dir_create_nul_exact_error (dir_create.c:327,328)
- dir_find: dir_create_reserved_device_name (dir_create.c:318,319)
- environment_sysinfo: wow64_invalid_handles (version.c:651)
- errors_exceptions_debug: eed_hw_trap_flag_single_step (tests/errors_exceptions_debug/eed_exc.c:833)
- file_attrs_ops: fsinfo_set_io_priority_valid_values (infoex_set.c:530)
- file_attrs_ops: fsinfo_set_dispex_posix_semantics_removes_name_immediately (infoex_set.c:362)
- file_io: fio_dir_create_new_on_directory_name (access_share.c:362)
- file_io: fio_attr_flags_applied_to_new_file (access_share.c:276)
- gap_02: gap2_rtok_chain (token.c:785)
- gap_02: gap2_cpau_not_an_executable (proc.c:166)
- gap_02: gap2_cpau_impersonation_token_rejected (proc.c:90)
- gap_03: gap3_eventactivityid_get_set_swaps (tests/gap_03/etw.c:415)
- gap_03: gap3_genctrl_private_console (tests/gap_03/console.c:343 (child exit 0x157))
- gap_03: gap3_expunge_history_detached (tests/gap_03/console.c:80 (child exit 0x50))
- gap_03: gap3_console_no_console_invalid_handles (tests/gap_03/console.c:43 (child exit 0x2b))
- gap_09: gap9_pss_walk_handles_contains_own_event ((any; AV inside ntdll))
- gap_09: gap9_pss_query_bad_length ((any; AV inside ntdll))
- gap_09: gap9_pss_capture_flag_combinations ((any; AV inside ntdll))
- gap_12: gap12_con_window_relative_out_of_range (console.c:464 x3 (child))
- gap_12: gap12_con_window_absolute_out_of_buffer (console.c:410 x4, :414 x2 (child))
- gap_15: gap15_verfind_cur_buffer_small (ver_map.c:197 CHECK(cl==1))
- gap_15: gap15_urlcanon_flags (gap15.h:75-78 (case http://a/%20, URL_UNESCAPE))
- gap_16: gap16_conw_write_console_output_invalid_handles (console.c:202 (child 'woberr'))
- gap_16: gap16_conw_write_console_basic_controls_wrap_vt_codepages (console.c:65 (child 'wc'))
- handles_objects: ho_ret_type_confusion_matrix (returns.c:408)
- handles_objects: ho_err_thread_isolation (leaks.c:142)
- job_objects: jobset_preserve_job_time_rules (jo_info.c:622)
- job_objects: jobnotify_associate_after_assign (jo_notify.c:143)
- job_objects: jobnotify_multiple_children_counts (jo_notify.c:94)
- locale_nls: uipref_set_thread_invalid_child (mui_nls.c:381 bit 0x2)
- memory_virtual: mv_va_physical_flag_rules (valloc.c:313)
- memory_virtual: mv_va_huge_sizes (valloc.c:233)
- memory_virtual: mv_va2_placeholder_flag_rules (misc.c:755)
- memory_virtual: mv_ww_read_does_not_dirty (misc.c:212)
- modules_loader: ld_ex_search_flags_dll_load_dir_with_altered_invalid (loadlib.c:282)
- modules_loader: dd_set_nonexistent_dir_accepted (dlldir.c:122)
- modules_loader: dd_set_get_roundtrip (dlldir.c:74)
- modules_loader: ac_query_assembly_detailed_information (actctx.c:617)
- modules_loader: ac_force_early_deactivation_pops_stack (actctx.c:495)
- process_create: pgec_invalid_handles (lifecycle.c:189)
- process_create: pty_child_output_arrives_on_pipe (conpty.c:57)
- process_create: pty_child_sees_a_console (conpty.c:66)
- process_create: pty_buffer_size_is_reported_to_child (conpty.c:75)
- strings_codepages: cpinfo_other_dbcs_note (tests/strings_codepages/cpinfo.c:45)
- strings_codepages: findnls_lcid_basic (tests/strings_codepages/compare.c:531-532)
- strings_codepages: findnls_ex_errors (tests/strings_codepages/compare.c:510)
- strings_codepages: findnls_ex_errors (tests/strings_codepages/compare.c:511)
- strings_codepages: cmp_ex_invalid_locale_name (tests/strings_codepages/compare.c:161 (name 2 L"en-US-garbage-more"))
- threadpool_timers: tp_timer_wait_no_cancel_runs_queued_callback (pool_timer.c:320)
- threadpool_timers: tp_wait_abandoned_mutex_result (pool_wait.c:241 (exception 0xc000070a in SetThreadpoolWait path))
- threads_tls_fibers: stress_last_error_per_thread (stress.c:146)
- toolhelp_psapi: th_thread_basic (th_snap.c:511)
- toolhelp_psapi: ps_wswatch_self_in_child (ps_mem.c:698)
- volume_drive: vd_gvibh_zero_access_volume_handle (volinfo.c:316,318)
- volume_drive: vd_ioctl_is_volume_mounted (misc.c:136)

## NOTRUN tests unblocked by guards
- async_io_iocp: aio_iocp_packets_survive_handle_close hung to the 600 s area timeout; 115 later tests were NOTRUN.
- gap_10: gap10_shco_bad_string_and_null_ids crashed the process (NULL clsid fault in SHStringFromGUIDW); 290 later tests were NOTRUN. Now run under a vectored-handler guard.
- threadpool_timers: tp_cbctx_may_run_long_longfunction_env crashed on a pool thread; 13 later tests were NOTRUN. The two TppRaiseInvalidParameter cases (tp_io_create_with_invalid_handles and the env cases) are moved to child processes.
- dir_find: notify_rdc_not_directory_handle_flag_missing hung to the area TIMEOUT; notify_fcn_* (21) and notify_rdc_* (25) were NOTRUN. A helper thread now creates files while the blocking call runs.
- atoms_profile_ini: four heap-corruption/AV tests (profile_file_over_64k_lookup, profile_large_file_write_into_middle, profile_large_file_key_enumeration, profile_concurrent_writers_distinct_files) killed the area (0xc0000374); the later tests in the area were NOTRUN. Guarded.
- Other guarded crash sites (area survives, test no longer takes the process down): appmodel cp2_null_names, file_attrs_ops fcopy_null_and_empty_names, file_io fio_readfileex_argument_errors, gap_01/07/12/14/15/16/19/22, heap global_lock_unreadable_pointer, locale_nls (3), registry regkb_pre_unassigned_predefined_values, strings isbad_readptr_null_and_zero, sync initonce_execute_context_reserved_bits, modules_loader ld2/ac_activate_nested_lifo.

## Expected effect of a rerun
- The five areas above should now complete rather than abort or time out, so the roughly 115 + 290 + 13 + 46+ previously NOTRUN tests will report results for the first time; some of those may expose new, independent failures.
- Rows marked EXPECT-FIX should flip from FAIL to OK (391 rows; any that stay red mean the reference analysis reading was wrong, and the row is then UNRESOLVED).
- GUARD rows: crashes become contained or cleanly reported; no area abort expected.
- TOLERANCE rows: pass on the accepted alternatives.
- UNRESOLVED rows: expected to remain failing; the rerun supplies the missing evidence. These are not counted as fixed.
- CLASS-G (console CJK/DBCS font-dependent tests; tools/classg.txt lists the cell-flag ones): expected to differ by environment and are excluded from pass-rate comparisons.
- Pre-existing OK tests should stay OK (audit restored two baseline-passing expectations: gap1_cfm2_desired_access_read_only_handle and the unrelated test changed by mistake).

## Audit
All 52 area branches were audited by independent reviewers (about a third to all cited reference analyses re-read). Defects were fixed in follow-up commits already included in the tips: async_io_iocp 6570aec, dir_find e9ab13a, gap_10 aed68f6, locale_nls f83d419 and 239261f, console_apis 89a612b, process_create 519583c, modules_loader c20a75b, gap_16 df8f1d9, registry_kernelbase 70dbec7, strings_codepages 4da059e, file_io 2d1d618, gap_06 751a28c, gap_04 38fbfa9, file_attrs_ops 3dadb26, sync_objects 87a315a, handles_objects 50493ad, appmodel_misc_kernelbase 46108d5, threads_tls_fibers 67990ac, gap_01 0f2f7a1, rtl_misc_kernel32 8cf9d61, gap_19 441823f, gap_22 00acfb8, path_names a7912d3 and ea319c4, gap_28 aaa0a77, gap_18 edd1eef, gap_24 ef61b80, gap_26 47b02f5, gap_13 03994c4. atoms_profile_ini and threadpool_timers passed unchanged.
