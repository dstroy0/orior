// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cargo_binary.c: the program that runs a Rust test, built as cargo test builds it, read from Cargo's own words of the
// artifacts it made, for a debugger to start it
#include "../programs/executables.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "../toolchains/toolchains.h"
#include "cargo_report.h"
#include "cargo_tests.h"

// the most bytes of one line of Cargo's words read
#define CLI_CARGO_LINE_TEXT (1ull << 20u)

static CliCargoTests s_binary_tests;
static char s_path[CLI_PATH_TEXT];

typedef struct
{
    const CliCargoTarget *target;
    const CliJson *json;
    unsigned int kinds;
    const char *name;
    const CliCargoFound *one;
    const char *line;
    unsigned long long size;
    char *program;
    const char *out;
    const char *root;
    const char *package;
    VerbaLinea *err_msg;
} CargoBinaryCtx;
// whether Cargo's word of an artifact, its target's kinds and name, is of the target
EMBED_INLINE int binary_is(const CargoBinaryCtx *args)
{
    const CliJson *json = args->json;
    int is_lib = 0;
    int is_bin = 0;
    int is_test = 0;
    for (unsigned int index = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = json, .array = args->kinds, .index = index) != CLI_JSON_NONE;
         index += 1u)
    {
        const char *const kind =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = args->kinds, .index = index),
                       .length_pointer = NULL);
        if (kind == NULL)
        {
            continue;
        }
        const size_t length = cellul.len(&(CatenaFinitaCfg){.src = kind, .cap = CLI_CELLS_POOL});
        is_lib = is_lib ||
                 ((length >= 3u) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = kind + length - 3u, .other = "lib", .cap = CLI_CELLS_POOL}))) ||
                 (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "proc-macro", .cap = CLI_CELLS_POOL}));
        is_bin = is_bin || (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "bin", .cap = CLI_CELLS_POOL}));
        is_test = is_test || (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "test", .cap = CLI_CELLS_POOL}));
    }
    return (args->target->kind == CLI_CARGO_LIB) ? is_lib
           : (args->target->kind == CLI_CARGO_BIN)
               ? (is_bin && (cellul.eq(&(CatenaFinitaCfg){
                                .src = args->target->name, .other = args->name, .cap = CLI_CELLS_POOL})))
               : (is_test && (cellul.eq(&(CatenaFinitaCfg){
                                 .src = args->target->name, .other = args->name, .cap = CLI_CELLS_POOL})));
}

// the program one line of Cargo's words, `size` bytes of `line`, names for the test into `program`, the line read
// into a tree sized to it and given back: 1, or 0 where it names none
EMBED_INLINE int binary_line(const CargoBinaryCtx *args)
{
    unsigned long long size = args->size;
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ull);
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea unread = {why, sizeof(why), 0ull};
    const unsigned int message =
        (size <= CLI_CARGO_LINE_TEXT)
            ? EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                         .text = args->line, .size = size, .nodes_more = 0u, .text_more = 0ull, .err_msg = &unread,
                         .owner = &g_cli_tests_cells)
            : CLI_JSON_NONE;
    const char *const reason =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                      .object = message, .name = "reason"),
                   .length_pointer = NULL);
    const unsigned int test = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                         .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                              .json_read = &json, .object = message, .name = "profile"),
                                         .name = "test");
    const unsigned int target = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                           .object = message, .name = "target");
    const char *const name =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                      .object = target, .name = "name"),
                   .length_pointer = NULL);
    const char *const executable =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                      .object = message, .name = "executable"),
                   .length_pointer = NULL);
    const int named =
        (reason != NULL) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = reason, .other = "compiler-artifact", .cap = CLI_CELLS_POOL})) &&
        EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json, .node = test,
                   .kind = CFG_JSON_TRUE) &&
        (executable != NULL) &&
        EMBED_CALL(binary_is, CargoBinaryCtx, .target = &args->one->target, .json = &json,
                   .kinds = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                       .object = target, .name = "kind"),
                   .name = (name != NULL) ? name : "");
    if (named)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = args->program,
                          .src = executable,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = executable, .cap = CLI_CELLS_POOL}) + 1u});
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    return named;
}

// the program Cargo's words name for the test, read line by line from what it wrote: 1, or 0 where none does
EMBED_INLINE int binary_read(const CargoBinaryCtx *args)
{
    unsigned long long size = args->size;
    unsigned long long at = 0ull;
    while (at < size)
    {
        const CliTextLine bounds =
            EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = args->out, .at = at, .size = size);
        const unsigned long long end = bounds.stop;
        const unsigned long long start = at;
        at = bounds.next;
        if (EMBED_CALL(binary_line, CargoBinaryCtx, .one = args->one, .line = args->out + start, .size = end - start,
                       .program = args->program))
        {
            return 1;
        }
    }
    return 0;
}

// the program for the test found `one` built, read and written into `program`: 1, or 0 with why given
EMBED_INLINE int binary_build(const CargoBinaryCtx *args)
{
    const char *root = args->root;
    VerbaLinea *err_msg = args->err_msg;
    char cargo[CLI_PATH_MAX_LEN];
    char manifest[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_executables_tool.program, CliExecutablesToolCfg, .tool = "rust", .name = "cargo", .path = cargo,
                    .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = args->one->manifest, .joined = manifest);
    const char *words[12] = {cargo, "test", "--no-run", "--message-format=json", "--package", args->one->package};
    unsigned int count = 6u;
    words[count] = (args->one->target.kind == CLI_CARGO_LIB)   ? "--lib"
                   : (args->one->target.kind == CLI_CARGO_BIN) ? "--bin"
                                                               : "--test";
    count += 1u;
    if (args->one->target.kind != CLI_CARGO_LIB)
    {
        words[count] = args->one->target.name;
        count += 1u;
    }
    words[count] = "--manifest-path";
    words[count + 1u] = manifest;
    words[count + 2u] = NULL;
    EMBED_CALL(cli_toolchains_count_path.run_path, CliToolchainsCountPathCfg, .path = s_path, .size = sizeof(s_path));
    const CliSetting settings[1] = {{"PATH", s_path}};
    CliRunResult *const run_result = cli_run_result_alloc();
    if (!EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = words, .folder = args->package, .settings = settings,
                    .count = 1u, .run_result = run_result, .err_msg = err_msg))
    {
        EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
        return 0;
    }
    const int built = EMBED_CALL(binary_read, CargoBinaryCtx, .one = args->one, .out = run_result->out,
                                 .size = run_result->out_size, .program = args->program);
    if (!built)
    {
        char error[4096];
        char path[CLI_PATH_MAX_LEN];
        long long line = 0ll;
        unsigned long long length = 0ull;
        if (EMBED_CALL(cli_cargo_report_size.cargo_build_error, CliCargoReportSizeCfg, .output = run_result->err,
                       .size = run_result->err_size, .error = error, .capacity = sizeof(error), .length = &length,
                       .path = path, .line = &line))
        {
            mmgr_line_put(err_msg, "The test's build failed: ");
            mmgr_line_put_n(err_msg, error, length);
        }
        else
        {
            mmgr_line_put(err_msg, "cargo test built no program for ");
            mmgr_line_put(err_msg, args->one->test.id);
        }
    }
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    return built;
}

int cli_cargo_binary(const CliCargoTestsIdCfg *args)
{
    EMBED_CALL(cli_cargo_tests_table.cargo_find, CliCargoTestsCfg, .root = args->root, .tests = &s_binary_tests);
    const CliCargoFound *one = NULL;
    for (unsigned int at = 0u; (one == NULL) && (at < s_binary_tests.count); at += 1u)
    {
        one = (cellul.eq(&(CatenaFinitaCfg){
                  .src = s_binary_tests.found[at].test.id, .other = args->id, .cap = CLI_CELLS_POOL}))
                  ? &s_binary_tests.found[at]
                  : NULL;
    }
    int built = 0;
    if (one == NULL)
    {
        mmgr_line_put(args->err_msg, args->id);
        mmgr_line_put(args->err_msg, " is no Rust test of this tree");
    }
    else
    {
        char folder[CLI_PATH_MAX_LEN];
        VerbaLinea folder_line = {folder, sizeof(folder), 0ull};
        mmgr_line_put_n(&folder_line, one->manifest,
                        cellul.len(&(CatenaFinitaCfg){.src = one->manifest, .cap = CLI_CELLS_POOL}) - 10u);
        mmgr_line_finish(&folder_line);
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = folder, .joined = args->package);
        built = EMBED_CALL(binary_build, CargoBinaryCtx, .root = args->root, .one = one, .package = args->package,
                           .program = args->program, .err_msg = args->err_msg);
        if (built)
        {
            memor.cpy(
                &(MemoriaCfg){.dst = args->full,
                              .src = one->full,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = one->full, .cap = CLI_CELLS_POOL}) + 1u});
        }
    }
    EMBED_CALL(cli_cargo_tests_table.free, CliCargoTestsCfg, .tests = &s_binary_tests);
    return built;
}
