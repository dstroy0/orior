// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// owners.h: the owners every module's cells count to, the program's one list, as data: pool.c lays MMgr's owner
// table from it with MMGR_OWNERS, and each owner here is a const pointer into that table the linker resolves, which
// its module names on every take, give and growth. Each line is the owner's identity and the name a reader prints,
// unique in the table. A module that takes cells is one line here
#ifndef OWNERS_H
#define OWNERS_H
#include "locus_carcerum/locus_carcerum.h"

// clang-format off
#define CLI_OWNERS(owner_)                                               \
    owner_(g_checks_owner, "checks")                                     \
    owner_(g_checks_grown_owner, "checks_grown")                         \
    owner_(g_cli_build_owner, "cli_builds")                              \
    owner_(g_cli_bundle_owner, "cli_bundle")                             \
    owner_(g_cli_cargo_owner, "cli_cargo_tests")                         \
    owner_(g_cli_catalog_owner, "cli_catalog")                           \
    owner_(g_cli_code_owner, "cli_code")                                 \
    owner_(g_cli_command_line_owner, "cli_command_line")                 \
    owner_(g_cli_comms_owner, "cli_comms")                               \
    owner_(g_cli_compile_owner, "cli_compile_db")                        \
    owner_(g_cli_db_owner, "cli_databases")                              \
    owner_(g_cli_db_pane_owner, "cli_db_pane")                           \
    owner_(g_cli_debug_owner, "cli_debug")                               \
    owner_(g_cli_debug_pane_owner, "cli_debug_pane")                     \
    owner_(g_cli_debug_processes_owner, "cli_debug_processes")           \
    owner_(g_cli_defs_owner, "cli_defs")                                 \
    owner_(g_cli_deploy_owner, "cli_deploy")                             \
    owner_(g_cli_devcontainer_owner, "cli_devcontainer")                 \
    owner_(g_cli_diff_view_owner, "cli_diff_view")                       \
    owner_(g_cli_ipc_owner, "cli_door")                                  \
    owner_(g_cli_editor_owner, "cli_editor")                             \
    owner_(g_cli_envs_owner, "cli_envs")                                 \
    owner_(g_cli_executables_owner, "cli_executables")                   \
    owner_(g_cli_files_views_owner, "cli_files_views")                   \
    owner_(g_cli_git_owner, "cli_git")                                   \
    owner_(g_cli_git_log_owner, "cli_git_log")                           \
    owner_(g_cli_gradle_owner, "cli_gradle")                             \
    owner_(g_cli_grammars_owner, "cli_grammars")                         \
    owner_(g_cli_inspect_owner, "cli_inspect")                           \
    owner_(g_cli_ipp_owner, "cli_ipp")                                   \
    owner_(g_cli_job_runs_owner, "cli_job_runs")                         \
    owner_(g_cli_leaves_owner, "cli_leaves")                             \
    owner_(g_cli_line_catalog_owner, "cli_line_catalog")                 \
    owner_(g_cli_lsp_files_owner, "cli_lsp_files")                       \
    owner_(g_cli_machine_owner, "cli_machine")                           \
    owner_(g_cli_maven_model_owner, "cli_maven_model")                   \
    owner_(g_cli_notebook_owner, "cli_notebooks")                        \
    owner_(g_cli_notebook_open_owner, "cli_notebook_open")               \
    owner_(g_cli_preview_owner, "cli_preview")                           \
    owner_(g_cli_quick_owner, "cli_quick")                               \
    owner_(g_cli_vim_owner, "cli_vim")                                   \
    owner_(g_cli_widgets_owner, "cli_widgets")                           \
    owner_(g_cli_platform_owner, "cli_platform")                         \
    owner_(g_cli_plugins_owner, "cli_plugins")                           \
    owner_(g_cli_print_owner, "cli_print")                               \
    owner_(g_cli_profile_owner, "cli_profile")                           \
    owner_(g_cli_programs_owner, "cli_programs")                         \
    owner_(g_cli_purpose_owner, "cli_purpose")                           \
    owner_(g_cli_python_owner, "cli_python_paths")                       \
    owner_(g_cli_qry_listing_owner, "cli_qry_listed")                    \
    owner_(g_cli_queue_owner, "cli_queue")                               \
    owner_(g_cli_requests_owner, "cli_requests")                         \
    owner_(g_cli_review_owner, "cli_review")                             \
    owner_(g_cli_runs_owner, "cli_runs")                                 \
    owner_(g_cli_schema_owner, "cli_schemas")                            \
    owner_(g_cli_scons_work_states_owner, "cli_scons_work_states")       \
    owner_(g_cli_sea_owner, "cli_sea")                                   \
    owner_(g_cli_search_owner, "cli_files_search")                       \
    owner_(g_cli_search_pane_owner, "cli_search_pane")                   \
    owner_(g_cli_servers_owner, "cli_servers")                           \
    owner_(g_cli_servers_check_owner, "cli_servers_check")               \
    owner_(g_cli_servers_check_work_state_owner, "the tree's check")     \
    owner_(g_cli_servers_edits_owner, "cli_servers_edits")               \
    owner_(g_cli_servers_launch_owner, "cli_servers_launch")             \
    owner_(g_cli_settings_owner, "cli_settings")                         \
    owner_(g_cli_shape_owner, "cli_shape")                               \
    owner_(g_cli_state_owner, "cli_state")                               \
    owner_(g_cli_templates_owner, "cli_templates")                       \
    owner_(g_cli_terminal_owner, "cli_terminal")                         \
    owner_(g_cli_test_report_owner, "cli_test_report")                   \
    owner_(g_cli_tests_owner, "cli_tests")                               \
    owner_(g_cli_tests_pane_owner, "cli_tests_pane")                     \
    owner_(g_cli_text_owner, "cli_text")                                 \
    owner_(g_cli_tls_owner, "cli_tls")                                   \
    owner_(g_cli_toolchains_owner, "cli_toolchains")                     \
    owner_(g_cli_toolchains_found_owner, "cli_toolchains_found")         \
    owner_(g_cli_toolchains_words_owner, "cli_toolchains_words")         \
    owner_(g_cli_trust_owner, "cli_trust")                               \
    owner_(g_cli_versions_owner, "cli_version_catalogs")                 \
    owner_(g_cli_view_owner, "cli_view")                                 \
    owner_(g_cli_views_owner, "cli_views")                               \
    owner_(g_cli_watch_owner, "cli_watch")                               \
    owner_(g_cli_watch_folders_owner, "cli_watch_folders")               \
    owner_(g_cli_xml_owner, "cli_xml")                                   \
    owner_(g_gui_base_owner, "gui_base")                                 \
    owner_(g_gui_owner, "gui")                                           \
    owner_(g_gui_framebuffer_owner, "gui_framebuffer")                   \
    owner_(g_gui_glyphs_owner, "gui_glyphs")                             \
    owner_(g_gui_icons_owner, "gui_icons")                               \
    owner_(g_gui_lattice_owner, "gui_lattice")                           \
    owner_(g_gui_raster_owner, "gui_raster")                             \
    owner_(g_query_owner, "cli_query")
// clang-format on

CLI_OWNERS(MMGR_OWNER_EXTERN)

#endif
