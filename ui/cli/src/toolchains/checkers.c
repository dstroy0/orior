// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// checkers.c: the checkers the manifest knows, found and run over a file or a tree, and their findings written by file
#include "../command_line/pool.h"
#include "../machine/trust.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "checkers_internal.h"
#include "toolchains.h"

// The work state a check reads into: the manifest, in a cell of its own, the toolchain found, the checker's words, the
// PATH it runs with, and its findings
typedef struct
{
    CliJson json;
    CliFound found;
    char words[CLI_WORDS_TEXT];
    char path[CLI_PATH_TEXT];
    CliFindings findings;
} CliCheckWorkState;

static CliCheckWorkState *s_work_state;

// the lock taken and the work state a cell under it: 1, or 0 with the lock given back where no cell is left
static int checkers_lock(void)
{
    s_work_state = g_cli_guard->persistent_buf_alloc(sizeof(*s_work_state), g_cli_toolchains_owner);
    if (s_work_state == NULL)
    {
        return 0;
    }
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &s_work_state->json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    return 1;
}

// the manifest read into the work state, the one read before given back: its array of toolchains
static unsigned int cli_manifest(void)
{
    g_cli_guard->persistent_buf_release(s_work_state->json.node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), s_work_state->json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_work_state->json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    return EMBED_CALL(cli_toolchains_json_nodes_more.manifest, CliToolchainsJsonNodesMoreCfg,
                      .json_out = &s_work_state->json, .nodes_more = 0u, .text_more = 0ULL);
}

typedef struct
{
    unsigned int node;
    const char *key;
    const char *otherwise;
    unsigned int manifest;
    const char *id;
    const char *text;
    const char *lower;
    unsigned long long size;
    VerbaLinea *err_msg;
    unsigned int tool;
    const char *args_key;
    const char *file;
    const char *root;
    const char *input;
    unsigned long long length;
    VerbaLinea *out;
} CheckersCtx;
// a member's string, `otherwise` where it is none
EMBED_INLINE const char *cli_string_or(const CheckersCtx *args)
{
    unsigned int node = args->node;
    const char *const text =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_work_state->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                      .json_read = &s_work_state->json, .object = node, .name = args->key),
                   .length_pointer = NULL);
    return (text != NULL) ? text : args->otherwise;
}

// the toolchain of the checker `id` in the manifest read: the tool, or CLI_JSON_NONE where it has no checker
EMBED_INLINE unsigned int cli_checker_get(const CheckersCtx *args)
{
    for (unsigned int at = 0u;
         EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &s_work_state->json,
                    .array = args->manifest, .index = at) != CLI_JSON_NONE;
         at += 1u)
    {
        const unsigned int tool = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = &s_work_state->json, .array = args->manifest, .index = at);
        if ((EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_work_state->json,
                        .object = tool, .name = "checker") != CLI_JSON_NONE) &&
            (cellul.eq(&(CatenaFinitaCfg){
                .src = EMBED_CALL(cli_string_or, CheckersCtx, .node = tool, .key = "id", .otherwise = ""),
                .other = args->id,
                .cap = CLI_POOL_BYTES})))
        {
            return tool;
        }
    }
    return CLI_JSON_NONE;
}

// whether a text is `lower`, ASCII's case aside
EMBED_INLINE int cli_same_lower(const CheckersCtx *args)
{
    return cellul.eq(&(CatenaFinitaCfg){.src = args->text, .other = args->lower, .cap = CLI_POOL_BYTES, .ci = 1});
}

int cli_checkers_by_name(const CliCheckersNameCfg *args)
{
    char required[CLI_NAME_MAX_LEN];
    const char *from = args->name;
    from +=
        EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = from, .at = 0ULL,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_POOL_BYTES}), .set = CLI_TEXT_WHITE);
    memor.cpy(&(MemoriaCfg){.dst = required,
                            .src = from,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_POOL_BYTES}) + 1u});
    unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = required, .cap = CLI_POOL_BYTES});
    length = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = required, .at = length,
                        .set = CLI_TEXT_WHITE);
    required[length] = '\0';
    EMBED_CALL(cli_text_size_from.lower, CliTextSizeFromCfg, .text_out = required,
               .size = cellul.len(&(CatenaFinitaCfg){.src = required, .cap = CLI_POOL_BYTES}));
    if (!checkers_lock())
    {
        return 0;
    }
    const unsigned int manifest = cli_manifest();
    int found = 0;
    for (unsigned int at = 0u;
         !found && (EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                               .json_read = &s_work_state->json, .array = manifest, .index = at) != CLI_JSON_NONE);
         at += 1u)
    {
        const unsigned int tool = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = &s_work_state->json, .array = manifest, .index = at);
        const char *const tool_id = EMBED_CALL(cli_string_or, CheckersCtx, .node = tool, .key = "id", .otherwise = "");
        found =
            (EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_work_state->json,
                        .object = tool, .name = "checker") != CLI_JSON_NONE) &&
            (EMBED_CALL(cli_same_lower, CheckersCtx, .text = tool_id, .lower = required) ||
             EMBED_CALL(cli_same_lower, CheckersCtx,
                        .text = EMBED_CALL(cli_string_or, CheckersCtx, .node = tool, .key = "name", .otherwise = ""),
                        .lower = required));
        if (found)
        {
            memor.cpy(
                &(MemoriaCfg){.dst = args->id_out,
                              .src = tool_id,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = tool_id, .cap = CLI_POOL_BYTES}) + 1u});
        }
    }
    g_cli_guard->persistent_buf_release(s_work_state->json.node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), s_work_state->json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_work_state->json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    g_cli_guard->persistent_buf_release(s_work_state, g_cli_toolchains_owner);
    s_work_state = NULL;
    return found;
}

int cli_checkers_reads_tree(const CliCheckersCfg *args)
{
    if (!checkers_lock())
    {
        return 0;
    }
    const unsigned int tool = EMBED_CALL(cli_checker_get, CheckersCtx, .manifest = cli_manifest(), .id = args->id);
    const unsigned int spec = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                         .json_read = &s_work_state->json, .object = tool, .name = "checker");
    const int tree =
        EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_work_state->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                      .json_read = &s_work_state->json, .object = spec, .name = "tree"),
                   .kind = CFG_JSON_ARRAY);
    g_cli_guard->persistent_buf_release(s_work_state->json.node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), s_work_state->json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_work_state->json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    g_cli_guard->persistent_buf_release(s_work_state, g_cli_toolchains_owner);
    s_work_state = NULL;
    return tree;
}

int cli_checkers_reads_input(const CliCheckersCfg *args)
{
    if (!checkers_lock())
    {
        return 0;
    }
    const unsigned int tool = EMBED_CALL(cli_checker_get, CheckersCtx, .manifest = cli_manifest(), .id = args->id);
    const unsigned int spec = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                         .json_read = &s_work_state->json, .object = tool, .name = "checker");
    const int input =
        EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_work_state->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                      .json_read = &s_work_state->json, .object = spec, .name = "stdin"),
                   .kind = CFG_JSON_TRUE);
    g_cli_guard->persistent_buf_release(s_work_state->json.node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), s_work_state->json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_work_state->json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    g_cli_guard->persistent_buf_release(s_work_state, g_cli_toolchains_owner);
    s_work_state = NULL;
    return input;
}

int cli_checkers_checks(const CliCheckersCfg *args)
{
    if (!checkers_lock())
    {
        return 0;
    }
    const unsigned int tool = EMBED_CALL(cli_checker_get, CheckersCtx, .manifest = cli_manifest(), .id = args->id);
    const unsigned int languages =
        EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_work_state->json,
                   .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                        .json_read = &s_work_state->json, .object = tool, .name = "checker"),
                   .name = "languages");
    int checks = 0;
    for (unsigned int at = 0u;
         !checks && (EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                .json_read = &s_work_state->json, .array = languages, .index = at) != CLI_JSON_NONE);
         at += 1u)
    {
        const char *const one = EMBED_CALL(
            cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_work_state->json,
            .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                               .json_read = &s_work_state->json, .array = languages, .index = at),
            .length_pointer = NULL);
        checks = (one != NULL) &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = one, .other = args->language, .cap = CLI_POOL_BYTES}));
    }
    g_cli_guard->persistent_buf_release(s_work_state->json.node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), s_work_state->json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_work_state->json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    g_cli_guard->persistent_buf_release(s_work_state, g_cli_toolchains_owner);
    s_work_state = NULL;
    return checks;
}

// whether `size` bytes contain no more than whitespace, and the last line of them that is not blank written into
// `err_msg`
EMBED_INLINE int cli_blank_text(const CheckersCtx *args)
{
    for (unsigned long long at = 0ULL; at < args->size; at += 1ULL)
    {
        if ((args->text[at] != ' ') && ((args->text[at] < '\t') || (args->text[at] > '\r')))
        {
            return 0;
        }
    }
    return 1;
}

EMBED_INLINE void cli_last_line(const CheckersCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned long long end = size;
    end = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = text, .at = end,
                     .set = CLI_TEXT_WHITE);
    unsigned long long start = end;
    start =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = text, .at = start, .set = "\n");
    start = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = text, .at = start,
                       .size = end, .set = CLI_TEXT_WHITE);
    EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = args->err_msg,
               .text_bytes = (const unsigned char *)text + start, .length = end - start);
}

// the checker `tool` run with the words of its spec's `args_key`, `{file}` written as `file`, from the tree `root`,
// `length` bytes of `input` on its input where it is given, and its findings read into the work state: 1, or 0 with why
// given
static int cli_on_check(const CheckersCtx *args)
{
    const char *root = args->root;
    const char *input = args->input;
    unsigned long long length = args->length;
    VerbaLinea *err_msg = args->err_msg;
    const CliJson *const json = &s_work_state->json;
    const char *const name = EMBED_CALL(cli_string_or, CheckersCtx, .node = args->tool, .key = "name", .otherwise = "");
    const unsigned int spec = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                         .object = args->tool, .name = "checker");
    const char *const program_name =
        EMBED_CALL(cli_string_or, CheckersCtx, .node = spec, .key = "program", .otherwise = "");
    const char *const reads = EMBED_CALL(cli_string_or, CheckersCtx, .node = spec, .key = "reads", .otherwise = "");
    if (EMBED_CALL(cli_trust_tree_words.block, CliTrustTreeWordsCfg, .tree = root))
    {
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, " does not run in a tree whose own code is disabled");
        return 0;
    }
    unsigned int folders = 0u;
    char (*const path)[CLI_PATH_MAX_LEN] = cli_toolchains_path_allocated(&folders, g_cli_toolchains_owner);
    EMBED_CALL(cli_toolchains_json.find, CliToolchainsJsonCfg, .json = json, .tool = args->tool,
               .path_callback = (const char (*)[CLI_PATH_MAX_LEN])path, .count = folders,
               .found = &s_work_state->found);
    g_cli_guard->persistent_buf_release(path, g_cli_toolchains_owner);
    char program[CLI_PATH_MAX_LEN];
    const char *const names[1] = {program_name};
    if ((s_work_state->found.folder[0] == '\0') ||
        !EMBED_CALL(cli_toolchains_dir.program_in, CliToolchainsDirCfg, .dir = s_work_state->found.folder,
                    .names = names, .count = 1u, .path = program))
    {
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, " is not found: File, Toolchains opens its install page or takes its folder");
        return 0;
    }
    const char *words[CLI_WORDS_MAX + 2u] = {program};
    unsigned int count = 1u;
    VerbaLinea filled = {s_work_state->words, sizeof(s_work_state->words), 0ULL};
    const unsigned int arguments = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                              .object = spec, .name = args->args_key);
    for (unsigned int at = 0u;
         (count <= CLI_WORDS_MAX) && (EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                 .json_read = json, .array = arguments, .index = at) != CLI_JSON_NONE);
         at += 1u)
    {
        const char *const word_text =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = arguments, .index = at),
                       .length_pointer = NULL);
        if (word_text == NULL)
        {
            continue;
        }
        const unsigned long long start = filled.at;
        if (args->file != NULL)
        {
            EMBED_CALL(cli_text_out_name.append_fill, CliTextOutNameCfg, .out = &filled, .text = word_text,
                       .name = "{file}", .value = args->file);
        }
        else
        {
            mmgr_line_put(&filled, word_text);
        }
        mmgr_line_ch(&filled, '\0');
        words[count] = s_work_state->words + start;
        count += mmgr_line_ok(&filled) ? 1u : 0u;
    }
    words[count] = NULL;
    EMBED_CALL(cli_toolchains_count_path.run_path, CliToolchainsCountPathCfg, .path = s_work_state->path,
               .size = sizeof(s_work_state->path));
    const CliSetting settings[1] = {{"PATH", s_work_state->path}};
    CliRunResult *const run_result = cli_run_result_alloc();
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unstarted = {why, sizeof(why), 0ULL};
    if (!EMBED_CALL(cli_words_internal.run_result_feed, CliWordsCtx, .words = words, .folder = root,
                    .settings = settings, .count = 1u, .input = input, .length = length, .run_result = run_result,
                    .err_msg = &unstarted))
    {
        EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
        mmgr_line_put(err_msg, program);
        mmgr_line_put(err_msg, ": ");
        mmgr_line_put_n(err_msg, why, unstarted.at);
        return 0;
    }
    if (EMBED_CALL(cli_blank_text, CheckersCtx, .text = run_result->out, .size = run_result->out_size) &&
        !EMBED_CALL(cli_blank_text, CheckersCtx, .text = run_result->err, .size = run_result->err_size) &&
        (!cellul.eq(&(CatenaFinitaCfg){.src = reads, .other = "lines", .cap = CLI_POOL_BYTES})))
    {
        mmgr_line_put(err_msg, name);
        mmgr_line_put(err_msg, ": ");
        EMBED_CALL(cli_last_line, CheckersCtx, .text = run_result->err, .size = run_result->err_size,
                   .err_msg = err_msg);
        EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
        return 0;
    }
    EMBED_CALL(cli_checkers_name_internal.read, CliCheckersNameCtx, .name = name, .reads = reads,
               .out = run_result->out, .out_size = run_result->out_size, .err = run_result->err,
               .err_size = run_result->err_size, .root = root, .path = args->file, .findings = &s_work_state->findings);
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    return 1;
}

// the findings written by file, as `[[path, [diagnostic]]]`
EMBED_INLINE void cli_findings_write(const CheckersCtx *args)
{
    VerbaLinea *out = args->out;
    const CliFindings *const findings = &s_work_state->findings;
    mmgr_line_ch(out, '[');
    for (unsigned int file = 0u; file < findings->files; file += 1u)
    {
        mmgr_line_put(out, (file != 0u) ? ",[" : "[");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out,
                   .text = findings->path[file],
                   .length = cellul.len(&(CatenaFinitaCfg){.src = findings->path[file], .cap = CLI_POOL_BYTES}));
        mmgr_line_put(out, ",[");
        unsigned int written = 0u;
        for (unsigned int at = 0u; at < findings->findings; at += 1u)
        {
            if (findings->file_of[at] == file)
            {
                mmgr_line_put(out, (written != 0u) ? "," : "");
                mmgr_line_put_n(out, findings->text + findings->start[at], findings->length[at]);
                written += 1u;
            }
        }
        mmgr_line_put(out, "]]");
    }
    mmgr_line_ch(out, ']');
}

int cli_checkers_check_file(const CliCheckersOutCfg *args)
{
    if (!checkers_lock())
    {
        mmgr_line_put(args->err_msg, "a check could not be taken");
        return 0;
    }
    const unsigned int tool = EMBED_CALL(cli_checker_get, CheckersCtx, .manifest = cli_manifest(), .id = args->id);
    if (tool == CLI_JSON_NONE)
    {
        mmgr_line_put(args->err_msg, args->id);
        mmgr_line_put(args->err_msg, " is not in the manifest");
        g_cli_guard->persistent_buf_release(s_work_state->json.node,
                                            mmgr_owner_of_buf(cli_pool_series_cellularum(), s_work_state->json.node));
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_work_state->json),
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
        g_cli_guard->persistent_buf_release(s_work_state, g_cli_toolchains_owner);
        s_work_state = NULL;
        return 0;
    }
    const unsigned int spec = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                         .json_read = &s_work_state->json, .object = tool, .name = "checker");
    const int stdin_read =
        EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_work_state->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                      .json_read = &s_work_state->json, .object = spec, .name = "stdin"),
                   .kind = CFG_JSON_TRUE);
    unsigned long long size = 0ULL;
    const unsigned char *const mapped = (stdin_read && (args->text == NULL))
                                            ? EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = args->path,
                                                         .size_pointer = &size, .owner = g_cli_toolchains_owner)
                                            : NULL;
    const char *input;
    if (stdin_read)
    {
        if (args->text != NULL)
        {
            input = args->text;
        }
        else if (mapped != NULL)
        {
            input = (const char *)mapped;
        }
        else
        {
            input = "";
        }
    }
    else
    {
        input = NULL;
    }
    const unsigned long long input_length = (args->text != NULL) ? args->length : size;
    const int checked =
        EMBED_CALL(cli_on_check, CheckersCtx, .tool = tool, .args_key = "args", .file = args->path, .root = args->root,
                   .input = input, .length = input_length, .err_msg = args->err_msg);
    g_cli_guard->persistent_buf_release((void *)(uintptr_t)mapped, (g_cli_toolchains_owner));
    if (checked)
    {
        EMBED_CALL(cli_checkers_path_internal.file, CliCheckersPathCtx, .findings = &s_work_state->findings,
                   .path = args->path, .first = 1);
        EMBED_CALL(cli_findings_write, CheckersCtx, .out = args->out);
    }
    g_cli_guard->persistent_buf_release(s_work_state->json.node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), s_work_state->json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_work_state->json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    g_cli_guard->persistent_buf_release(s_work_state, g_cli_toolchains_owner);
    s_work_state = NULL;
    return checked;
}

int cli_checkers_check_tree(const CliCheckersOutCfg *args)
{
    if (!checkers_lock())
    {
        mmgr_line_put(args->err_msg, "a check could not be taken");
        return 0;
    }
    const unsigned int tool = EMBED_CALL(cli_checker_get, CheckersCtx, .manifest = cli_manifest(), .id = args->id);
    const unsigned int spec = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                         .json_read = &s_work_state->json, .object = tool, .name = "checker");
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_work_state->json,
                    .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                       .json_read = &s_work_state->json, .object = spec, .name = "tree"),
                    .kind = CFG_JSON_ARRAY))
    {
        mmgr_line_put(args->out, "[]");
        g_cli_guard->persistent_buf_release(s_work_state->json.node,
                                            mmgr_owner_of_buf(cli_pool_series_cellularum(), s_work_state->json.node));
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_work_state->json),
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
        g_cli_guard->persistent_buf_release(s_work_state, g_cli_toolchains_owner);
        s_work_state = NULL;
        return 1;
    }
    const int checked = EMBED_CALL(cli_on_check, CheckersCtx, .tool = tool, .args_key = "tree", .file = NULL,
                                   .root = args->root, .input = NULL, .length = 0ULL, .err_msg = args->err_msg);
    if (checked)
    {
        EMBED_CALL(cli_findings_write, CheckersCtx, .out = args->out);
    }
    g_cli_guard->persistent_buf_release(s_work_state->json.node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), s_work_state->json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_work_state->json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    g_cli_guard->persistent_buf_release(s_work_state, g_cli_toolchains_owner);
    s_work_state = NULL;
    return checked;
}
