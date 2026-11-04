// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cells_owners.h: every owner of the shared cellblock, one line each, read by cells.h for its
// declarations and by cells.c for its definitions and for the registrations made with the pool.
// Each line names the owner as a reader prints it, unique in MMgr's queue. Each reader expands the list with what it
// writes for one owner
#ifndef CELLS_OWNERS_H
#define CELLS_OWNERS_H
#define CLI_CELL_OWNERS(owner_)                                                                                        \
    owner_(g_checks_cells, "checks") owner_(g_cli_build_cells, "cli_builds") owner_(                                   \
        g_cli_bundle_cells, "cli_bundle") owner_(g_cli_cargo_cells, "cli_cargo_tests") owner_(g_cli_catalog_cells,     \
                                                                                              "cli_catalog")           \
        owner_(g_cli_code_cells, "cli_code") owner_(g_cli_command_line_cells, "cli_command_line") owner_(              \
            g_cli_comms_cells, "cli_comms") owner_(g_cli_compile_cells,                                                \
                                                   "cli_compile_db") owner_(g_cli_db_cells,                            \
                                                                            "cli_databases") owner_(g_cli_debug_cells, \
                                                                                                    "cli_debug")       \
            owner_(g_cli_debug_processes_cells, "cli_debug_processes") owner_(g_cli_defs_cells, "cli_defs") owner_(    \
                g_cli_deploy_cells,                                                                                    \
                "cli_deploy") owner_(g_cli_devcontainer_cells,                                                         \
                                     "cli_devcontainer") owner_(g_cli_ipc_cells,                                       \
                                                                "cli_door") owner_(g_cli_editor_cells, "cli_editor")   \
                owner_(g_cli_envs_cells, "cli_envs") owner_(g_cli_executables_cells, "cli_executables") owner_(        \
                    g_cli_files_views_cells,                                                                           \
                    "cli_files_views") owner_(g_cli_git_cells, "cli_git") owner_(g_cli_gradle_cells, "cli_gradle")     \
                    owner_(g_cli_grammars_cells, "cli_grammars") owner_(g_cli_inspect_cells, "cli_inspect") owner_(    \
                        g_cli_ipp_cells,                                                                               \
                        "cli_ipp") owner_(g_cli_leaves_cells,                                                          \
                                          "cli_leaves") owner_(g_cli_line_catalog_cells,                               \
                                                               "cli_line_catalog") owner_(g_cli_lsp_files_cells,       \
                                                                                          "cli_lsp_files")             \
                        owner_(g_cli_machine_cells, "cli_machine") owner_(                                             \
                            g_cli_maven_model_cells,                                                                   \
                            "cli_maven_model") owner_(g_cli_notebook_cells,                                            \
                                                      "cli_notebooks") owner_(g_cli_platform_cells, "cli_platform")    \
                            owner_(g_cli_plugins_cells, "cli_plugins") owner_(g_cli_print_cells, "cli_print") owner_(  \
                                g_cli_profile_cells,                                                                   \
                                "cli_profile") owner_(g_cli_programs_cells,                                            \
                                                      "cli_programs") owner_(g_cli_purpose_cells,                      \
                                                                             "cli_purpose") owner_(g_cli_python_cells, \
                                                                                                   "cli_python_paths") \
                                owner_(g_cli_qry_listing_cells, "cli_qry_listed") owner_(                              \
                                    g_cli_queue_cells, "cli_queue") owner_(g_cli_requests_cells,                       \
                                                                           "cli_requests") owner_(g_cli_review_cells,  \
                                                                                                  "cli_review")        \
                                    owner_(g_cli_runs_cells, "cli_runs") owner_(                                       \
                                        g_cli_schema_cells,                                                            \
                                        "cli_schemas") owner_(g_cli_scons_work_states_cells,                           \
                                                              "cli_scons_work_states") owner_(g_cli_sea_cells,         \
                                                                                              "cli_sea")               \
                                        owner_(g_cli_search_cells, "cli_files_search") owner_(                         \
                                            g_cli_servers_cells, "cli_servers") owner_(g_cli_servers_check_cells,      \
                                                                                       "cli_servers_check")            \
                                            owner_(g_cli_servers_check_work_state_cells, "the tree's check") owner_(   \
                                                g_cli_servers_edits_cells,                                             \
                                                "cli_servers_edits") owner_(g_cli_servers_launch_cells,                \
                                                                            "cli_servers_launch")                      \
                                                owner_(g_cli_settings_cells, "cli_settings") owner_(                   \
                                                    g_cli_shape_cells,                                                 \
                                                    "cli_shape") owner_(g_cli_state_cells,                             \
                                                                        "cli_state") owner_(g_cli_templates_cells,     \
                                                                                            "cli_templates")           \
                                                    owner_(g_cli_test_report_cells, "cli_test_report") owner_(         \
                                                        g_cli_tests_cells,                                             \
                                                        "cli_tests") owner_(g_cli_text_cells,                          \
                                                                            "cli_text") owner_(g_cli_terminal_cells,   \
                                                                                               "cli_terminal")         \
                                                        owner_(g_cli_tls_cells, "cli_tls") owner_(                     \
                                                            g_cli_toolchains_cells,                                    \
                                                            "cli_toolchains") owner_(g_cli_toolchains_found_cells,     \
                                                                                     "cli_toolchains_found")           \
                                                            owner_(g_cli_toolchains_words_cells,                       \
                                                                   "cli_toolchains_words") owner_(g_cli_trust_cells,   \
                                                                                                  "cli_trust")         \
                                                                owner_(g_cli_versions_cells, "cli_version_catalogs")   \
                                                                    owner_(g_cli_views_cells, "cli_views")             \
                                                                        owner_(g_cli_watch_cells, "cli_watch") owner_( \
                                                                            g_cli_watch_folders_cells,                 \
                                                                            "cli_watch_folders")                       \
                                                                            owner_(g_cli_xml_cells, "cli_xml") owner_( \
                                                                                g_gui_base_cells, "gui_base")          \
                                                                                owner_(g_gui_cells, "gui") owner_(     \
                                                                                    g_gui_glyphs_cells, "gui_glyphs")  \
                                                                                    owner_(g_gui_icons_cells,          \
                                                                                           "gui_icons")                \
                                                                                        owner_(g_gui_lattice_cells,    \
                                                                                               "gui_lattice")          \
                                                                                            owner_(g_gui_raster_cells, \
                                                                                                   "gui_raster")       \
                                                                                                owner_(                \
                                                                                                    g_cli_view_cells,  \
                                                                                                    "cli_view")        \
                                                                                                    owner_(            \
                                                                                                        g_query_cells, \
                                                                                                        "cli_query")
#endif
