// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tools_table.c: every C program a loop runs, a row each: its sources, its includes, its language and flags, how it
// links, and the folders whose headers make it stale; and the file definitions every program of a run links. A
// pattern names the files of one folder by their extension, `folder/*.c`, or one file, from the top of the tree; a
// list of them ends at NULL
#include "tools_internal.h"

// the sources of the file definitions, which every program of a run links (src/cu/includes/file_defs)
const char *const g_query_file_defs[] = {"src/cu/includes/file_defs/qry/writer/qry_member.c",
                                         "src/cu/includes/file_defs/qry/writer/platform/*.c",
                                         "src/cu/includes/parsers/line_read.c",
                                         "src/cu/includes/file_defs/qry/qry_read.c",
                                         "src/cu/includes/parsers/who.c",
                                         "src/cu/transpiler/lstar/protocol/table_text.c",
                                         "src/cu/includes/file_defs/krep/krep_records/*.c",
                                         "src/cu/includes/file_defs/kfile_lines/kfile_lines.c",
                                         "src/cu/includes/file_defs/kqr/kqr_record/*.c",
                                         "src/cu/includes/file_defs/ksc/ksc.c",
                                         "src/cu/includes/file_defs/kdm/kdm.c",
                                         "src/cu/includes/file_defs/krf/krf.c",
                                         NULL};

// MMgr, which every program of the tree writes, copies, compares and scans its text through: its modules' sources,
// each folder's, and of impensa_ancorae_acus's profiles, each a source of its own, the one MMgr's build takes where
// none is named, generic
const char *const g_query_mmgr[] = {"src/cu/includes/mmgr/src/ascii_persona_bitorum/*.c",
                                    "src/cu/includes/mmgr/src/bitorum_introitus_exitus/*.c",
                                    "src/cu/includes/mmgr/src/cellularum_laboro/*.c",
                                    "src/cu/includes/mmgr/src/clz/*.c",
                                    "src/cu/includes/mmgr/src/confinium_exclusivum_infinitas/*.c",
                                    "src/cu/includes/mmgr/src/confinium_externum/*.c",
                                    "src/cu/includes/mmgr/src/endian/*.c",
                                    "src/cu/includes/mmgr/src/fractio/*.c",
                                    "src/cu/includes/mmgr/src/impensa_ancorae_acus/impensa_ancorae_acus_generic.c",
                                    "src/cu/includes/mmgr/src/locus_carcerum/*.c",
                                    "src/cu/includes/mmgr/src/memoria_anularis/*.c",
                                    "src/cu/includes/mmgr/src/memoria_externa/*.c",
                                    "src/cu/includes/mmgr/src/memoria_operor/*.c",
                                    "src/cu/includes/mmgr/src/memoriam_praetereo/*.c",
                                    "src/cu/includes/mmgr/src/numeros_scribo/*.c",
                                    "src/cu/includes/mmgr/src/octetus_introitus_exitus/*.c",
                                    "src/cu/includes/mmgr/src/proximus_operor/*.c",
                                    "src/cu/includes/mmgr/src/spatium/*.c",
                                    "src/cu/includes/mmgr/src/transformo/*.c",
                                    "src/cu/includes/mmgr/src/verba_scribo/*.c",
                                    "src/cu/includes/mmgr/src/verbum_scrutor/*.c",
                                    NULL};

// MMgr's include folders, from the top of the tree, and the settings it is built with, given every compile
const char *const g_query_mmgr_includes[] = {"src/cu/includes/mmgr/include", "src/cu/includes/mmgr/src",
                                             "src/cu/includes/embedded_types/include", NULL};

const char *const g_query_mmgr_settings[] = {"-DMMGR_ENABLE_DMA=0", "-DMMGR_ENABLE_EXTRAM=0", "-DMMGR_ENABLE_HOST=1", "-DMMGR_ALIGN_BYTES=32",
                                             NULL};

// the folders the file definitions' headers stand in beyond their sources' own
const char *const g_query_file_defs_headers[] = {"src/cu/includes/file_defs", "src/cu/includes/parsers",
                                                 "src/cu/transpiler/lstar/protocol", NULL};

// the interface every program that starts a probe links
static const char *const s_interface[] = {"src/cu/transpiler/lstar/interface/interface.c",
                                          "src/cu/transpiler/lstar/interface/platform/*.c",
                                          "src/cu/transpiler/lstar/interface/interface_names.c", NULL};

// the code generator, compiled as C++, and the sources every reader of it skips
static const char *const s_codegen[] = {"src/cu/engine/rmc/*.cu", "src/cu/includes/parsers/*.cu",
                                        "src/cu/includes/file_defs/krs/ruleset_flat/*.cu", NULL};

static const char *const s_codegen_skipped[] = {"asm_printer_", "codegen", NULL};

static const char *const s_c11[] = {"-std=c11", "-O2", "-Wall", "-Wextra", NULL};

static const char *const s_c11_plain[] = {"-std=c11", "-O2", "-Wall", NULL};

static const char *const s_cxx17[] = {"-std=c++17", "-O2", "-Wall", "-Wextra", NULL};

static const char *const s_cxx17_plain[] = {"-std=c++17", "-O2", "-Wall", NULL};

static const char *const s_none[] = {NULL};

static const char *const s_engine[] = {"src/cu/engine", NULL};

static const char *const s_static[] = {"-static", NULL};

static const char *const s_bus_clock[] = {"src/cu/transpiler/vendor_bin_layouts/nvidia/bus_clock.c",
                                          "src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding/mmio_read.c",
                                          "src/cu/transpiler/lstar/protocol/query/query_ask.c", NULL};

static const char *const s_bus_clock_headers[] = {"src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding",
                                                  "src/cu/transpiler/lstar/protocol/query", NULL};

static const char *const s_qry_writer[] = {"src/cu/includes/file_defs/qry/qry_writer.c",
                                           "src/cu/includes/file_defs/qry/writer/*.c",
                                           "src/cu/includes/file_defs/qry/writer/platform/*.c",
                                           "src/cu/includes/file_defs/qry/qry_read.c",
                                           "src/cu/includes/file_defs/ksg/ksg_seal.c",
                                           "src/cu/includes/file_defs/ksg/ksg_writing.c",
                                           NULL};

static const char *const s_protocol[] = {"src/cu/transpiler/lstar/protocol", NULL};

static const char *const s_cubin_run[] = {"src/cu/transpiler/vendor_bin_layouts/nvidia/cubin_run.c",
                                          "src/cu/transpiler/vendor_bin_layouts/nvidia/cubin_run/*.c",
                                          "src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding/mmio_read.c",
                                          "src/cu/transpiler/vendor_bin_layouts/nvidia/cubin_safe/*.c",
                                          "src/cu/transpiler/vendor_bin_layouts/nvidia/tables/read.c",
                                          "src/cu/transpiler/vendor_bin_layouts/nvidia/cubin_write.c",
                                          "src/cu/transpiler/vendor_bin_layouts/container_write.c",
                                          "src/cu/transpiler/vendor_bin_layouts/container_pattern.c",
                                          "src/cu/transpiler/vendor_bin_layouts/container_layout.c",
                                          "src/cu/transpiler/vendor_bin_layouts/nvidia/sass_assemble/*.c",
                                          "src/cu/transpiler/vendor_bin_layouts/nvidia/sass_machine/*.c",
                                          NULL};

static const char *const s_cubin_listing[] = {"src/cu/transpiler/vendor_bin_layouts/nvidia/cubin_listing.c",
                                              "src/cu/transpiler/vendor_bin_layouts/nvidia/cubin_listing/*.c",
                                              "src/cu/transpiler/vendor_bin_layouts/nvidia/cubin_write.c",
                                              "src/cu/transpiler/vendor_bin_layouts/container_write.c",
                                              "src/cu/transpiler/vendor_bin_layouts/container_pattern.c",
                                              "src/cu/transpiler/vendor_bin_layouts/container_layout.c",
                                              NULL};

static const char *const s_stick_query[] = {"src/cu/transpiler/vendor_bin_layouts/nvidia/measuring_stick_query.c",
                                            "src/cu/transpiler/vendor_bin_layouts/nvidia/measuring_stick_query/*.c",
                                            "src/cu/transpiler/vendor_bin_layouts/nvidia/tables/read.c", NULL};

static const char *const s_pthread_c[] = {"-std=c11", "-O2", "-Wall", "-Wextra", "-pthread", NULL};

static const char *const s_pthread[] = {"-pthread", NULL};

static const char *const s_khw_learn[] = {"src/cu/transpiler/lstar/protocol/khw_learn/*.c",
                                          "src/cu/includes/file_defs/khw/khw_write/*.c",
                                          "src/cu/transpiler/lstar/protocol/teacher/run_channel/*.c",
                                          "src/cu/transpiler/lstar/protocol/overseer/*.c",
                                          "src/cu/transpiler/lstar/protocol/query/answer_read.c",
                                          "src/cu/transpiler/lstar/protocol/gate/survivors.c",
                                          "src/cu/transpiler/lstar/protocol/order/scheduler.c",
                                          "src/cu/transpiler/vendor_bin_layouts/container_layout.c",
                                          NULL};

static const char *const s_khw_learn_headers[] = {
    "src/cu/transpiler/lstar/protocol/teacher", "src/cu/transpiler/lstar/protocol/query",
    "src/cu/transpiler/lstar/protocol/gate", "src/cu/transpiler/lstar/protocol/order", NULL};

static const char *const s_khw_machine_write[] = {"src/cu/includes/file_defs/khw/khw_machine_write.c",
                                                  "src/cu/includes/file_defs/khw/khw_machine_write/*.c",
                                                  "src/cu/transpiler/vendor_bin_layouts/nvidia/cubin_write.c",
                                                  "src/cu/transpiler/vendor_bin_layouts/container_write.c",
                                                  "src/cu/transpiler/vendor_bin_layouts/container_pattern.c",
                                                  "src/cu/transpiler/vendor_bin_layouts/container_layout.c",
                                                  "src/cu/transpiler/vendor_bin_layouts/nvidia/sass_assemble/*.c",
                                                  "src/cu/transpiler/vendor_bin_layouts/nvidia/sass_machine/*.c",
                                                  NULL};

static const char *const s_sass_listing_seeds[] = {"src/cu/transpiler/vendor_bin_layouts/nvidia/sass_listing_seeds.c",
                                                   NULL};

static const char *const s_krs_write[] = {"src/cu/includes/file_defs/krs/krs_write.c",
                                          "src/cu/includes/file_defs/krs/krs_write/*.c", NULL};

// the word web and the precepts the words are written over
static const char *const s_rmc[] = {"src/cu/engine/rmc", NULL};

static const char *const s_gsm_write[] = {"src/cu/includes/file_defs/gsm/gsm_write.c", NULL};

static const char *const s_member_c[] = {"src/cu/includes/file_defs/qry/writer/qry_member.c",
                                         "src/cu/includes/file_defs/qry/writer/platform/*.c",
                                         "src/cu/includes/file_defs/qry/qry_read.c", NULL};

static const char *const s_klq_write[] = {"src/cu/includes/file_defs/klq/klq_write/klq_write.cu", NULL};

static const char *const s_klq_includes[] = {"src/cu/engine", "src/cu/engine/rmc", "src/cu/includes/parsers",
                                             "src/cu/transpiler/lstar/protocol", NULL};

static const char *const s_klq_write_headers[] = {"src/cu/transpiler/lstar/protocol",
                                                  "src/cu/transpiler/lstar/protocol/table", NULL};

static const char *const s_klq_identity_c[] = {"src/cu/transpiler/lstar/protocol/teacher/run_channel/*.c",
                                               "src/cu/transpiler/lstar/protocol/overseer/*.c", NULL};

static const char *const s_klq_identity_cxx[] = {"src/cu/transpiler/lstar/protocol/table/klq_identity.cu",
                                                 "src/cu/transpiler/lstar/protocol/table/identity/*.cu", NULL};

static const char *const s_klq_identity_includes[] = {"src/cu/engine",
                                                      "src/cu/engine/rmc",
                                                      "src/cu/includes/parsers",
                                                      "src/cu/transpiler/lstar/protocol",
                                                      "src/cu/transpiler/lstar/protocol/table",
                                                      NULL};

static const char *const s_klq_identity_headers[] = {"src/cu/transpiler/lstar/protocol",
                                                     "src/cu/transpiler/lstar/protocol/teacher", NULL};

static const char *const s_klq_decoder_cxx[] = {"src/cu/transpiler/lstar/protocol/table/klq_decoder.cu",
                                                "src/cu/transpiler/lstar/protocol/table/decoder/*.cu",
                                                "src/cu/includes/file_defs/klq/query_trace.cu", NULL};

static const char *const s_klq_decoder_includes[] = {
    "src/cu/transpiler/lstar/protocol/table", "src/cu/includes/parsers", "src/cu/transpiler/lstar/protocol", NULL};

static const char *const s_tables_funnel[] = {"src/cu/transpiler/vendor_bin_layouts/nvidia/tables/tables_funnel.c",
                                              "src/cu/transpiler/vendor_bin_layouts/nvidia/tables/funnel.c",
                                              "src/cu/includes/parsers/line_read.c", NULL};

static const char *const s_tables_read[] = {"src/cu/transpiler/vendor_bin_layouts/nvidia/tables/tables_read.c",
                                            "src/cu/transpiler/vendor_bin_layouts/nvidia/tables/read.c",
                                            "src/cu/transpiler/vendor_bin_layouts/nvidia/cubin_safe/constant.c",
                                            "src/cu/includes/parsers/line_read.c", NULL};

static const char *const s_tables_read_headers[] = {"src/cu/transpiler/vendor_bin_layouts/nvidia/tables",
                                                    "src/cu/transpiler/vendor_bin_layouts/nvidia/cubin_safe", NULL};

static const char *const s_kdecode[] = {"src/cu/transpiler/lstar/protocol/kdecode/kdecode.c",
                                        "src/cu/transpiler/lstar/protocol/kdecode/kdecode/*.c", NULL};

static const char *const s_kdecode_headers[] = {"src/cu/transpiler/lstar/protocol/kdecode/orior_kdecode",
                                                "src/cu/transpiler/lstar/protocol", "src/cu/includes/file_defs/kqr",
                                                NULL};

static const char *const s_stick_read[] = {"src/cu/transpiler/vendor_bin_layouts/nvidia/measuring_stick_read.c",
                                           "src/cu/transpiler/vendor_bin_layouts/nvidia/measuring_stick_read/*.c",
                                           NULL};

static const char *const s_stick_engine_c[] = {
    "src/cu/transpiler/vendor_bin_layouts/nvidia/sass_machine/*.c",
    "src/cu/transpiler/vendor_bin_layouts/nvidia/sass_assemble/*.c",
    "src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding/interface_sass_probe_class.c",
    "src/cu/includes/types/integers/exact_integer_add.c",
    "src/cu/includes/types/integers/exact_integer_limbs.c",
    "src/cu/includes/types/integers/exact_integer_multiply.c",
    "src/cu/includes/types/integers/exact_integer_divide.c",
    "src/cu/includes/types/integers/exact_integer_gcd.c",
    "src/cu/includes/types/integers/exact_integer_decimal.c",
    "src/cu/includes/types/integers/exact_integer_hash.c",
    NULL};

static const char *const s_stick_engine_keys[] = {"src/cu/engine/analysis/keymath/keymath.cu",
                                                  "src/cu/engine/analysis/key_schedule/key_schedule.cu", NULL};

static const char *const s_stick_engine_targets[] = {
    "src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding/cu_target.cu",
    "src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding/sass_target.cu", NULL};

static const char *const s_stick_engine_tool[] = {
    "src/cu/transpiler/vendor_bin_layouts/nvidia/measuring_stick_engine.cpp", NULL};

static const char *const s_stick_engine_includes[] = {"src/cu/engine",
                                                      "src/cu/engine/includes/codecs/crc",
                                                      "src/cu/engine/analysis/cycle",
                                                      "src/cu/engine/rmc",
                                                      "src/cu/includes/parsers",
                                                      "src/cu/engine/analysis/keymath",
                                                      "src/cu/engine/analysis/key_schedule",
                                                      "src/cu/includes/types/integers",
                                                      "src/cu/transpiler/vendor_bin_layouts/nvidia/scaffolding",
                                                      "utils/test/src/cu/engine/analysis/cycle",
                                                      "src/cu/transpiler/lstar/interface",
                                                      NULL};

static const char *const s_static_threads[] = {"-static", "-lpthread", NULL};

static const char *const s_stick_engine_headers[] = {"utils/test/src/cu/transpiler/lstar/interface",
                                                     "src/cu/transpiler/lstar/interface", NULL};

const QueryTool g_query_tools[] = {
    {"orior_bus_clock", {s_bus_clock, NULL}, {NULL}, s_none, 1, s_none, s_none, s_c11, s_none, s_none, s_bus_clock_headers},
    {"orior_qry_writer", {s_qry_writer, NULL}, {NULL}, s_none, 0, s_none, s_none, s_c11, s_none, s_none, s_protocol},
    {"orior_cubin_run", {s_cubin_run, NULL}, {NULL}, s_none, 1, s_none, s_none, s_c11_plain, s_none, s_none, s_none},
    {"orior_cubin_listing",
     {s_cubin_listing, s_interface, NULL},
     {NULL},
     s_none,
     1,
     s_none,
     s_engine,
     s_c11,
     s_none,
     s_none,
     s_none},
    {"orior_measuring_stick_query",
     {s_stick_query, s_interface, NULL},
     {NULL},
     s_none,
     1,
     s_none,
     s_engine,
     s_pthread_c,
     s_none,
     s_pthread,
     s_none},
    {"orior_khw_learn",
     {s_khw_learn, s_interface, NULL},
     {NULL},
     s_none,
     1,
     s_none,
     s_engine,
     s_c11,
     s_none,
     s_none,
     s_khw_learn_headers},
    {"orior_khw_machine_write",
     {s_khw_machine_write, NULL},
     {NULL},
     s_none,
     1,
     s_none,
     s_none,
     s_c11_plain,
     s_none,
     s_none,
     s_none},
    {"orior_sass_listing_seeds",
     {s_sass_listing_seeds, NULL},
     {NULL},
     s_none,
     0,
     s_none,
     s_none,
     s_c11,
     s_none,
     s_none,
     s_none},
    {"orior_krs_write", {s_krs_write, s_interface, NULL}, {NULL}, s_none, 1, s_none, s_engine, s_c11, s_none, s_none, s_rmc},
    {"orior_gsm_write", {s_gsm_write, s_interface, NULL}, {NULL}, s_none, 1, s_none, s_engine, s_c11, s_none, s_none, s_none},
    {"orior_klq_write",
     {s_member_c, NULL},
     {s_codegen, s_klq_write, NULL},
     s_none,
     0,
     s_codegen_skipped,
     s_klq_includes,
     s_c11,
     s_cxx17,
     s_static,
     s_klq_write_headers},
    {"orior_klq_identity",
     {s_klq_identity_c, s_interface, NULL},
     {s_codegen, s_klq_identity_cxx, NULL},
     s_none,
     1,
     s_codegen_skipped,
     s_klq_identity_includes,
     s_c11,
     s_cxx17,
     s_static,
     s_klq_identity_headers},
    {"orior_klq_decoder",
     {s_member_c, NULL},
     {s_klq_decoder_cxx, NULL},
     s_none,
     0,
     s_none,
     s_klq_decoder_includes,
     s_c11,
     s_cxx17,
     s_static,
     s_protocol},
    {"orior_tables_funnel", {s_tables_funnel, NULL}, {NULL}, s_none, 0, s_none, s_none, s_c11, s_none, s_none, s_none},
    {"orior_tables_read",
     {s_tables_read, NULL},
     {NULL},
     s_none,
     0,
     s_none,
     s_none,
     s_c11,
     s_none,
     s_none,
     s_tables_read_headers},
    {"orior_kdecode", {s_kdecode, NULL}, {NULL}, s_none, 1, s_none, s_none, s_c11, s_none, s_none, s_kdecode_headers},
    {"orior_measuring_stick_read", {s_stick_read, NULL}, {NULL}, s_none, 1, s_none, s_none, s_c11, s_none, s_none, s_none},
    {"orior_measuring_stick_engine",
     {s_stick_engine_c, NULL},
     {s_stick_engine_keys, s_codegen, s_stick_engine_targets, NULL},
     s_stick_engine_tool,
     1,
     s_codegen_skipped,
     s_stick_engine_includes,
     s_c11,
     s_cxx17_plain,
     s_static_threads,
     s_stick_engine_headers},
    {NULL, {NULL}, {NULL}, NULL, 0, NULL, NULL, NULL, NULL, NULL, NULL}};
