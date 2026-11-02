// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cli_unity.c: what orior_cli's Unity suites share
#include "cli_unity.h"

#include "ui/cli/src/command_line/cells.h"
#include "ui/cli/src/text/text.h"

#include <stdlib.h>

// ORIOR_CELLS_OWNERS set: the pool's report and MMgr's owner table written as the process ends, each owner's bytes now and its peak
static void owners_written(void)
{
    char requested[8];
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "ORIOR_CELLS_OWNERS", .value_out = requested, .capacity = sizeof(requested)))
    {
        return;
    }
    static char s_owners[256ull * 1024ull];
    VerbaLinea owners = {s_owners, sizeof(s_owners), 0ull};
    mmgr_line_put(&owners, "cells pool ");
    EMBED_CALL(cli_cells_table.usage_write, CliCellsCfg, .out = &owners);
    mmgr_line_ch(&owners, '\n');
    mmgr_line_put(&owners, "cells owners ");
    EMBED_CALL(cli_cells_table.owners_write, CliCellsCfg, .out = &owners);
    mmgr_line_ch(&owners, '\n');
    EMBED_CALL(query_errors_internal.terminal_write, QueryErrorsCtx, .errors = 0, .text = owners.out, .length = mmgr_line_ok(&owners) ? owners.at : 0ull);
}

void cli_unity_begun(void)
{
    static int s_begun;
    if (!s_begun)
    {
        (void)EMBED_CALL(cli_cells_table.init, CliCellsCfg, .bytes = CLI_CELLS_POOL);
        (void)atexit(owners_written);
        s_begun = 1;
    }
}

void cli_unity_folder(const char *name, char *full)
{
    if (!EMBED_CALL(query_capacity_path_internal.path_full, QueryCapacityPathCtx, .path = name, .full = full, .capacity = CLI_PATH_MAX_LEN))
    {
        memor.cpy(&(MemoriaCfg){.dst = full, .src = name, .bytes = CLI_UNITY_LENGTH(name) + 1ull});
    }
    (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = full);
    (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = full);
}

void cli_unity_file(const char *root, const char *name, const char *text)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = name, .joined = path);
    TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = text, .size = CLI_UNITY_LENGTH(text)), name);
}

void cli_unity_git(const char *root, const char *const *words, unsigned int count)
{
    const char *line[8] = {"git"};
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        line[at + 1u] = words[at];
    }
    line[count + 1u] = NULL;
    CliRunResult *const run_result = cli_run_result_alloc();
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    const int run_result_well = EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = line, .folder = root, .settings = NULL, .count = 0u, .run_result = run_result, .err_msg = &err_msg) && (run_result->code == 0);
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    TEST_ASSERT_TRUE_MESSAGE(run_result_well, words[0]);
}

// the tree a test's JSON is parsed into, replaced at each parse
static CliJson s_json;

const CliJson *cli_unity_parsed(const char *text, unsigned long long size, unsigned int *root)
{
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &s_json);
    char why[512];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    *root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &s_json, .text = text, .size = size, .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg, .owner = &g_checks_cells);
    return &s_json;
}

void cli_unity_parsed_released(void)
{
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &s_json);
}

const char *cli_unity_member_text(unsigned int node, const char *name)
{
    const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = node, .name = name), .length_pointer = NULL);
    return (text != NULL) ? text : "";
}

unsigned int cli_unity_finding(unsigned int found, const char *code, const char *part)
{
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &s_json, .array = found, .index = at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &s_json, .array = found, .index = at);
        const char *const message = cli_unity_member_text(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json, .object = one, .name = "message"), "value");
        const int code_matches = (code == NULL) || cellul.eq(&(CatenaFinitaCfg){.src = cli_unity_member_text(one, "code"), .other = code, .cap = CLI_CELLS_POOL});
        const int part_matches = (part == NULL) || (cellul.find(&(CatenaFinitaCfg){.src = message, .cap = CLI_CELLS_POOL, .other = part, .other_cap = CLI_CELLS_POOL}) != NULL);
        if (code_matches && part_matches)
        {
            return one;
        }
    }
    return CLI_JSON_NONE;
}

int cli_unity_completes(const CliCompletions *items, const char *label)
{
    for (unsigned int at = 0u; at < items->count; at += 1u)
    {
        const CliCompletion *const item = &items->item[at];
        if ((item->label_size == CLI_UNITY_LENGTH(label)) && (memor.cmp(&(MemoriaCfg){.src = items->text + item->label_at, .other = label, .bytes = item->label_size}) == 0))
        {
            return 1;
        }
    }
    return 0;
}

const CliCompletions *cli_unity_build_completed(const char *root, const char *path, const char *text)
{
    static CliCompletions s_items;
    unsigned int line = 0u;
    unsigned int col = 0u;
    for (const char *at = text; at[0] != '\0'; at += 1)
    {
        line += (at[0] == '\n') ? 1u : 0u;
        col = (at[0] == '\n') ? 0u : col + 1u;
    }
    EMBED_CALL(cli_edits_items.completions_begin, CliEditsItemsCfg, .items = &s_items);
    EMBED_CALL(cli_builds_path.build_complete, CliBuildsPathCfg, .root = root, .path = path, .text = text, .size = CLI_UNITY_LENGTH(text), .line = line, .col = col, .items = &s_items);
    return &s_items;
}

void cli_unity_server(const char *name, unsigned int port, char *host, unsigned int *at)
{
    char variable[64];
    VerbaLinea line = {variable, sizeof(variable), 0ull};
    mmgr_line_put(&line, "ORIOR_");
    mmgr_line_put(&line, name);
    mmgr_line_put(&line, "_ADDRESS");
    (void)mmgr_line_finish(&line);
    char named[CLI_NAME_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = host, .src = "127.0.0.1", .bytes = 10u});
    *at = port;
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = variable, .value_out = named, .capacity = sizeof(named)))
    {
        return;
    }
    const char *const colon = EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = named, .length_value = CLI_UNITY_LENGTH(named), .byte = ':');
    unsigned long long number = 0ull;
    int digits = (colon != NULL) && (colon[1] != '\0');
    for (const char *letter = (colon != NULL) ? colon + 1 : ""; digits && (letter[0] != '\0'); letter += 1)
    {
        digits = (letter[0] >= '0') && (letter[0] <= '9') && (number < 65536ull);
        number = (number * 10ull) + (unsigned long long)(letter[0] - '0');
    }
    if (digits && (number < 65536ull))
    {
        named[colon - named] = '\0';
        memor.cpy(&(MemoriaCfg){.dst = host, .src = named, .bytes = CLI_UNITY_LENGTH(named) + 1ull});
        // a port is below 65536, which an unsigned int represents
        *at = (unsigned int)number;
    }
}
