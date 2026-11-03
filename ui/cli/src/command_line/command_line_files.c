// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line_files.c: `orior_cli run run-file`, a file run with its language's toolchain, in bash at the tree's top
// folder, attached to this terminal, exiting with its code; `orior_cli run validate`, each file validated with the tool
// plugin for its language, what it found and its verdict printed, or the whole report as JSON; and `orior_cli edit
// format`, each file formatted in place with its language's formatter, or with --check each that would change named
#include "../code/git.h"
#include "../plugins/validate.h"
#include "../runs/run_file.h"
#include "../runs/runner.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "../toolchains/format.h"
#include "../toolchains/toolchains.h"
#include "cells.h"
#include "command_line_internal.h"

#include <stdlib.h>

// A command's work state, a cell taken as the command starts and given back as it ends: the tree a text is read into, a
// line printed, a file read and what is made of it
typedef struct
{
    CliJsonNode node[1u << 16u];
    char tree_text[1u << 24u];
    CfgJsonToken token[1u << 16u];
    CliJson json;
    char line[1u << 16u];
    char read[1u << 24u];
    char made[1u << 24u];
} CliFilesWorkState;

static CliFilesWorkState *s_work_state;

// the command's work state taken: 1, or 0 reported on the errors
static int files_work_state_init(void)
{
    s_work_state = g_cli_cells->persistent_buf_alloc(sizeof(*s_work_state), &g_cli_command_line_cells);
    if (s_work_state == NULL)
    {
        EMBED_CALL(cli_command_line_internal.err, CliCommandLineCtx, .text = "a command on files could not be taken");
    }
    return s_work_state != NULL;
}

// the command's work state given back
static void files_release(void)
{
    g_cli_cells->persistent_buf_release(s_work_state, &g_cli_command_line_cells);
    s_work_state = NULL;
}

typedef struct
{
    const char *text;
    unsigned long long size;
    const char *one;
    const char *two;
    const char *three;
    const char *word_text;
    char *full;
    const char *named;
    const char *const *words;
    unsigned int count;
    VerbaLinea *out;
    unsigned int node;
    const char *name;
    const char *between;
    const char *file;
    unsigned int report;
    const char *flag;
    int *flagged;
} CommandLineFilesCtx;
// `text` read as JSON into the tree: its node, or CLI_JSON_NONE
EMBED_INLINE unsigned int files_tree(const CommandLineFilesCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &s_work_state->json,
               .node_json_node_pointer = s_work_state->node,
               .nodes_max = sizeof(s_work_state->node) / sizeof(s_work_state->node[0]),
               .text_out = s_work_state->tree_text, .text_max = sizeof(s_work_state->tree_text));
    char why[256];
    VerbaLinea ignored = {why, sizeof(why), 0ULL};
    return EMBED_CALL(cli_json_tree_text_size_tokens.json_read, CliJsonTreeTextSizeTokensCfg,
                      .json = &s_work_state->json, .text = text, .size = size, .tokens = s_work_state->token,
                      .tokens_max = sizeof(s_work_state->token) / sizeof(s_work_state->token[0]), .err_msg = &ignored);
}

// a line of the errors made of up to three texts
EMBED_INLINE void files_err(const CommandLineFilesCtx *args)
{
    VerbaLinea line = {s_work_state->line, sizeof(s_work_state->line), 0ULL};
    mmgr_line_put(&line, args->one);
    mmgr_line_put(&line, args->two);
    mmgr_line_put(&line, args->three);
    EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = s_work_state->line,
               .size = mmgr_line_ok(&line) ? line.at : 0ULL);
}

// a word naming a file made a full path into `full`, the word as it is where it cannot be
EMBED_INLINE void files_full(const CommandLineFilesCtx *args)
{
    char *full = args->full;
    if (!EMBED_CALL(query_capacity_path_internal.path_full, QueryCapacityPathCtx, .path = args->word_text, .full = full,
                    .capacity = CLI_PATH_MAX_LEN))
    {
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->word_text, .cap = CLI_CELLS_POOL});
        const unsigned long long copied_len = (size < CLI_PATH_MAX_LEN) ? size : CLI_PATH_MAX_LEN - 1ULL;
        memor.cpy(&(MemoriaCfg){.dst = full, .src = args->word_text, .bytes = copied_len});
        full[copied_len] = '\0';
    }
}

EMBED_INLINE int files_run_words(const CommandLineFilesCtx *args)
{
    const char *named = args->named;
    const char *const *words = args->words;
    unsigned int count = args->count;
    if (count != 1u)
    {
        EMBED_CALL(cli_command_line_internal.err, CliCommandLineCtx, .text = "run-file takes the one file to run");
        return CLI_WRONG;
    }
    static char s_root[CLI_PATH_MAX_LEN];
    static char s_err_buf[4096];
    VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ULL};
    if (!EMBED_CALL(cli_command_line_named.tree, CliCommandLineNamedCfg, .named = named, .root = s_root,
                    .err_msg = &err_msg))
    {
        EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = s_err_buf,
                   .size = mmgr_line_ok(&err_msg) ? err_msg.at : 0ULL);
        return CLI_NO_CODE;
    }
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(files_full, CommandLineFilesCtx, .word_text = words[0], .full = path);
    char language[CLI_NAME_MAX_LEN];
    if (!EMBED_CALL(cli_format_table.language_get, CliFormatCfg, .path = path, .id = language))
    {
        EMBED_CALL(files_err, CommandLineFilesCtx, .one = words[0],
                   .two = ": no plugin opens it, and so orior_cli knows no way to run it", .three = "");
        return CLI_NO_CODE;
    }
    VerbaLinea run = {s_work_state->read, sizeof(s_work_state->read), 0ULL};
    if (!EMBED_CALL(cli_run_file.line, CliRunFileCfg, .root = s_root, .path = path, .language = language, .out = &run,
                    .err_msg = &err_msg))
    {
        EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = s_err_buf,
                   .size = mmgr_line_ok(&err_msg) ? err_msg.at : 0ULL);
        return CLI_NO_CODE;
    }
    const unsigned int tree = EMBED_CALL(files_tree, CommandLineFilesCtx, .text = s_work_state->read,
                                         .size = mmgr_line_ok(&run) ? run.at : 0ULL);
    const char *const line =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_work_state->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                      .json_read = &s_work_state->json, .object = tree, .name = "line"),
                   .length_pointer = NULL);
    char bash[CLI_PATH_MAX_LEN];
    if ((line == NULL) || !EMBED_CALL(cli_runner_name.bash, CliRunnerNameCfg, .path = bash, .err_msg = &err_msg))
    {
        EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = s_err_buf,
                   .size = mmgr_line_ok(&err_msg) ? err_msg.at : 0ULL);
        return CLI_NO_CODE;
    }
    EMBED_CALL(files_err, CommandLineFilesCtx, .one = "$ ", .two = line, .three = "");
    static char s_path[CLI_PATH_TEXT];
    EMBED_CALL(cli_toolchains_count_path.run_path, CliToolchainsCountPathCfg, .path = s_path, .size = sizeof(s_path));
    const CliSetting setting = {"PATH", s_path};
    const char *const command[4] = {bash, "-c", line, NULL};
    long long run_result = 0LL;
    err_msg.at = 0ULL;
    int code = CLI_NO_CODE;
    if (!EMBED_CALL(cli_words_err_msg_settings_internal.spawn_share, CliWordsErrMsgSettingsCtx, .words = command,
                    .settings = &setting, .count = 1u, .code = &run_result, .err_msg = &err_msg))
    {
        EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = s_err_buf,
                   .size = mmgr_line_ok(&err_msg) ? err_msg.at : 0ULL);
    }
    else if (run_result >= 0LL)
    {
        // an exit code is the system's int
        code = (int)run_result;
    }
    return code;
}

// the strings of an array member joined by `between`
EMBED_INLINE void files_join(const CommandLineFilesCtx *args)
{
    unsigned int node = args->node;
    const char *name = args->name;
    const unsigned int array = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                          .json_read = &s_work_state->json, .object = node, .name = name);
    const unsigned int first = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                          .json_read = &s_work_state->json, .node = array, .kind = CFG_JSON_ARRAY)
                                   ? s_work_state->json.node[array].first
                                   : CLI_JSON_NONE;
    for (unsigned int item = first; item != CLI_JSON_NONE; item = s_work_state->json.node[item].next)
    {
        const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                            .json_read = &s_work_state->json, .node = item, .length_pointer = NULL);
        mmgr_line_put(args->out, (item != first) ? args->between : "");
        mmgr_line_put(args->out, (text != NULL) ? text : "");
    }
}

// a number member, as written, read; 0 where it has none
EMBED_INLINE double files_number(const CommandLineFilesCtx *args)
{
    unsigned int node = args->node;
    const char *name = args->name;
    const unsigned int number = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                           .json_read = &s_work_state->json, .object = node, .name = name);
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_work_state->json,
                    .node = number, .kind = CFG_JSON_NUMBER))
    {
        return 0.0;
    }
    char text[64];
    const unsigned long long length = s_work_state->json.node[number].length;
    const unsigned long long copied_len = (length < sizeof(text)) ? length : sizeof(text) - 1u;
    memor.cpy(&(MemoriaCfg){
        .dst = text, .src = s_work_state->json.text + s_work_state->json.node[number].text, .bytes = copied_len});
    text[copied_len] = '\0';
    return strtod(text, NULL);
}

// each check of a report printed, then each finding, then the file's verdict
EMBED_INLINE void files_report_print(const CommandLineFilesCtx *args)
{
    const unsigned int checks = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                           .json_read = &s_work_state->json, .object = args->report, .name = "checks");
    const unsigned int first = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                          .json_read = &s_work_state->json, .node = checks, .kind = CFG_JSON_ARRAY)
                                   ? s_work_state->json.node[checks].first
                                   : CLI_JSON_NONE;
    for (unsigned int check = first; check != CLI_JSON_NONE; check = s_work_state->json.node[check].next)
    {
        VerbaLinea line = {s_work_state->line, sizeof(s_work_state->line), 0ULL};
        mmgr_line_put(&line, "  checked ");
        const unsigned int settings = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                 .json_read = &s_work_state->json, .object = check, .name = "settings");
        if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_work_state->json,
                        .node = settings, .kind = CFG_JSON_ARRAY) ||
            (s_work_state->json.node[settings].count == 0u))
        {
            mmgr_line_put(&line, "as it is");
        }
        EMBED_CALL(files_join, CommandLineFilesCtx, .out = &line, .node = check, .name = "settings", .between = " ");
        mmgr_line_put(&line, ": ");
        const double seconds = EMBED_CALL(files_number, CommandLineFilesCtx, .node = check, .name = "seconds");
        // the seconds to a tenth, rounded; a check's seconds are not negative
        const unsigned long long tenths = (unsigned long long)(seconds * 10.0 + 0.5);
        mmgr_line_decimal(&line, tenths / 10ULL, 1u);
        mmgr_line_ch(&line, '.');
        mmgr_line_decimal(&line, tenths % 10ULL, 1u);
        mmgr_line_put(&line, " s, ");
        const double errors = EMBED_CALL(files_number, CommandLineFilesCtx, .node = check, .name = "errors");
        // a count of errors is a whole number
        mmgr_line_decimal(&line, (unsigned long long)errors, 1u);
        mmgr_line_put(&line, (errors == 1.0) ? " error" : " errors");
        const unsigned int barriers = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                 .json_read = &s_work_state->json, .object = check, .name = "barriers");
        if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_work_state->json,
                       .node = barriers, .kind = CFG_JSON_ARRAY) &&
            (s_work_state->json.node[barriers].count > 0u))
        {
            mmgr_line_put(&line, ", stopped by ");
            EMBED_CALL(files_join, CommandLineFilesCtx, .out = &line, .node = check, .name = "barriers",
                       .between = ", ");
        }
        EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx, .text = s_work_state->line,
                   .size = mmgr_line_ok(&line) ? line.at : 0ULL);
    }
    const unsigned int findings =
        EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_work_state->json,
                   .object = args->report, .name = "findings");
    const unsigned int found = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                          .json_read = &s_work_state->json, .node = findings, .kind = CFG_JSON_ARRAY)
                                   ? s_work_state->json.node[findings].first
                                   : CLI_JSON_NONE;
    for (unsigned int finding = found; finding != CLI_JSON_NONE; finding = s_work_state->json.node[finding].next)
    {
        const char *const kind = EMBED_CALL(
            cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_work_state->json,
            .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_work_state->json,
                               .object = finding, .name = "kind"),
            .length_pointer = NULL);
        const char *const message = EMBED_CALL(
            cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_work_state->json,
            .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_work_state->json,
                               .object = finding, .name = "message"),
            .length_pointer = NULL);
        const char *const lifted = EMBED_CALL(
            cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_work_state->json,
            .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_work_state->json,
                               .object = finding, .name = "lifted"),
            .length_pointer = NULL);
        VerbaLinea line = {s_work_state->line, sizeof(s_work_state->line), 0ULL};
        mmgr_line_put(&line, args->file);
        mmgr_line_ch(&line, ':');
        // a line and a column counted from 0 are whole numbers
        mmgr_line_decimal(
            &line,
            (unsigned long long)EMBED_CALL(files_number, CommandLineFilesCtx, .node = finding, .name = "line") + 1ULL,
            1u);
        mmgr_line_ch(&line, ':');
        mmgr_line_decimal(
            &line,
            (unsigned long long)EMBED_CALL(files_number, CommandLineFilesCtx, .node = finding, .name = "col") + 1ULL,
            1u);
        mmgr_line_put(&line, ": ");
        mmgr_line_put(&line, (kind != NULL) ? kind : "");
        mmgr_line_put(&line, ": ");
        const char *rest = (message != NULL) ? message : "";
        int head = 1;
        for (;;)
        {
            const char *const end = cellul.chr(&(CatenaFinitaCfg){.src = rest, .cap = CLI_CELLS_POOL, .byte = '\n'});
            const unsigned long long size = (end != NULL)
                                                ? (unsigned long long)(end - rest)
                                                : cellul.len(&(CatenaFinitaCfg){.src = rest, .cap = CLI_CELLS_POOL});
            unsigned long long from = 0ULL;
            unsigned long long to = 0ULL;
            EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = rest, .size = size,
                       .from_u64s = &from, .to_u64s = &to);
            if (to > from)
            {
                if (!head)
                {
                    line.at = 0ULL;
                    mmgr_line_put(&line, "    ");
                }
                const unsigned long long len = ((size > 0ULL) && (rest[size - 1ULL] == '\r')) ? size - 1ULL : size;
                mmgr_line_put_n(&line, rest, len);
                EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx,
                           .text = s_work_state->line, .size = mmgr_line_ok(&line) ? line.at : 0ULL);
                head = 0;
            }
            if (end == NULL)
            {
                break;
            }
            rest = end + 1;
        }
        if (head)
        {
            EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx,
                       .text = s_work_state->line, .size = mmgr_line_ok(&line) ? line.at : 0ULL);
        }
        if (lifted != NULL)
        {
            line.at = 0ULL;
            mmgr_line_put(&line, "    ");
            mmgr_line_put(&line, lifted);
            EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx,
                       .text = s_work_state->line, .size = mmgr_line_ok(&line) ? line.at : 0ULL);
        }
    }
    const int passes =
        EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_work_state->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                      .json_read = &s_work_state->json, .object = args->report, .name = "passes"),
                   .kind = CFG_JSON_TRUE);
    const char *const verdict =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_work_state->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                      .json_read = &s_work_state->json, .object = args->report, .name = "verdict"),
                   .length_pointer = NULL);
    VerbaLinea line = {s_work_state->line, sizeof(s_work_state->line), 0ULL};
    mmgr_line_put(&line, args->file);
    mmgr_line_put(&line, passes ? ": passes: " : ": fails: ");
    mmgr_line_put(&line, (verdict != NULL) ? verdict : "");
    EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx, .text = s_work_state->line,
               .size = mmgr_line_ok(&line) ? line.at : 0ULL);
}

// whether `flag` is among the words, and whether every other word names a file, there being one
EMBED_INLINE int files_words(const CommandLineFilesCtx *args)
{
    int *flagged = args->flagged;
    unsigned int files = 0u;
    *flagged = 0;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->words[at], .other = args->flag, .cap = CLI_CELLS_POOL}))
        {
            *flagged = 1;
            continue;
        }
        if (cellul.starts(&(CatenaFinitaCfg){.src = args->words[at], .other = "--", .cap = CLI_CELLS_POOL}))
        {
            return 0;
        }
        files += 1u;
    }
    return files > 0u;
}

EMBED_INLINE int files_validate(const CommandLineFilesCtx *args)
{
    int json = 0;
    if (!EMBED_CALL(files_words, CommandLineFilesCtx, .words = args->words, .count = args->count, .flag = "--json",
                    .flagged = &json))
    {
        EMBED_CALL(cli_command_line_internal.err, CliCommandLineCtx,
                   .text = "validate takes [--json] and the files to validate");
        return CLI_WRONG;
    }
    int code = 0;
    static char s_err_buf[4096];
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        const char *const file = args->words[at];
        if (cellul.eq(&(CatenaFinitaCfg){.src = file, .other = "--json", .cap = CLI_CELLS_POOL}))
        {
            continue;
        }
        char path[CLI_PATH_MAX_LEN];
        EMBED_CALL(files_full, CommandLineFilesCtx, .word_text = file, .full = path);
        char language[CLI_NAME_MAX_LEN];
        char tool[CLI_NAME_MAX_LEN];
        if (!EMBED_CALL(cli_format_table.language_get, CliFormatCfg, .path = path, .id = language) ||
            !EMBED_CALL(cli_validate.tool_for, CliValidateCfg, .language = language, .id = tool))
        {
            EMBED_CALL(files_err, CommandLineFilesCtx, .one = file, .two = ": no tool plugin validates it",
                       .three = "");
            code = CLI_NO_CODE;
            continue;
        }
        VerbaLinea report = {s_work_state->read, sizeof(s_work_state->read), 0ULL};
        VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ULL};
        if (!EMBED_CALL(cli_validate_path.validate, CliValidatePathCfg, .id_read = tool, .path = path,
                        .report = &report, .err_msg = &err_msg))
        {
            (void)mmgr_line_finish(&err_msg);
            EMBED_CALL(files_err, CommandLineFilesCtx, .one = file, .two = ": ", .three = s_err_buf);
            code = CLI_NO_CODE;
            continue;
        }
        const unsigned int tree = EMBED_CALL(files_tree, CommandLineFilesCtx, .text = s_work_state->read,
                                             .size = mmgr_line_ok(&report) ? report.at : 0ULL);
        if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_work_state->json,
                        .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                           .json_read = &s_work_state->json, .object = tree, .name = "passes"),
                        .kind = CFG_JSON_TRUE))
        {
            code = CLI_NO_CODE;
        }
        if (json)
        {
            VerbaLinea pretty = {s_work_state->made, sizeof(s_work_state->made), 0ULL};
            if (tree != CLI_JSON_NONE)
            {
                EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &s_work_state->json,
                           .node = tree, .out = &pretty, .indent = 2u, .sorted = 0);
            }
            EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx,
                       .text = s_work_state->made, .size = mmgr_line_ok(&pretty) ? pretty.at : 0ULL);
            continue;
        }
        EMBED_CALL(files_report_print, CommandLineFilesCtx, .file = file, .report = tree);
    }
    return code;
}

EMBED_INLINE int files_format(const CommandLineFilesCtx *args)
{
    int check = 0;
    if (!EMBED_CALL(files_words, CommandLineFilesCtx, .words = args->words, .count = args->count, .flag = "--check",
                    .flagged = &check))
    {
        EMBED_CALL(cli_command_line_internal.err, CliCommandLineCtx,
                   .text = "format takes [--check] and the files to format");
        return CLI_WRONG;
    }
    int code = 0;
    static char s_err_buf[4096];
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        const char *const file = args->words[at];
        if (cellul.eq(&(CatenaFinitaCfg){.src = file, .other = "--check", .cap = CLI_CELLS_POOL}))
        {
            continue;
        }
        char full[CLI_PATH_MAX_LEN];
        EMBED_CALL(files_full, CommandLineFilesCtx, .word_text = file, .full = full);
        char language[CLI_NAME_MAX_LEN];
        if (!EMBED_CALL(cli_format_table.language_get, CliFormatCfg, .path = file, .id = language))
        {
            EMBED_CALL(files_err, CommandLineFilesCtx, .one = file,
                       .two = ": no plugin opens it, and so no formatter knows it", .three = "");
            code = CLI_NO_CODE;
            continue;
        }
        const unsigned long long size =
            qry_member_file_read(file, (unsigned char *)s_work_state->read, sizeof(s_work_state->read));
        if ((size == QRY_MEMBER_UNREAD) ||
            !EMBED_CALL(cli_json_tree_size.utf8_valid, CliJsonTreeSizeCfg, .bytes = s_work_state->read, .size = size))
        {
            EMBED_CALL(files_err, CommandLineFilesCtx, .one = file, .two = ": it could not be read", .three = "");
            code = CLI_NO_CODE;
            continue;
        }
        VerbaLinea formatted = {s_work_state->made, sizeof(s_work_state->made), 0ULL};
        VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ULL};
        if (!EMBED_CALL(cli_format_path.format, CliFormatPathCfg, .path = full, .language = language,
                        .text = s_work_state->read, .length = size, .out = &formatted, .err_msg = &err_msg))
        {
            EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = s_err_buf,
                       .size = mmgr_line_ok(&err_msg) ? err_msg.at : 0ULL);
            code = CLI_NO_CODE;
            continue;
        }
        unsigned long long length = 0ULL;
        const char *const text = EMBED_CALL(
            cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_work_state->json,
            .node = EMBED_CALL(files_tree, CommandLineFilesCtx, .text = s_work_state->made,
                               .size = mmgr_line_ok(&formatted) ? formatted.at : 0ULL),
            .length_pointer = &length);
        if ((text != NULL) && (length == size) &&
            (memor.cmp(&(MemoriaCfg){.src = text, .other = s_work_state->read, .bytes = size}) == 0))
        {
            continue;
        }
        if (check)
        {
            VerbaLinea line = {s_work_state->line, sizeof(s_work_state->line), 0ULL};
            mmgr_line_put(&line, file);
            mmgr_line_put(&line, " would change");
            EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx,
                       .text = s_work_state->line, .size = mmgr_line_ok(&line) ? line.at : 0ULL);
            code = CLI_NO_CODE;
            continue;
        }
        if ((text == NULL) || !EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx,
                                          .path = file, .bytes = text, .size = length))
        {
            EMBED_CALL(files_err, CommandLineFilesCtx, .one = file, .two = ": it could not be written", .three = "");
            code = CLI_NO_CODE;
            continue;
        }
        VerbaLinea line = {s_work_state->line, sizeof(s_work_state->line), 0ULL};
        mmgr_line_put(&line, file);
        mmgr_line_put(&line, " formatted");
        EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx, .text = s_work_state->line,
                   .size = mmgr_line_ok(&line) ? line.at : 0ULL);
    }
    return code;
}

int cli_run_file_words(const CliCommandLineWordsCountCtx *args)
{
    if (!files_work_state_init())
    {
        return CLI_NO_CODE;
    }
    const int code = EMBED_CALL(files_run_words, CommandLineFilesCtx, .named = args->named, .words = args->words,
                                .count = args->count);
    files_release();
    return code;
}

int cli_validate_files(const CliCommandLineWordsCountCtx *args)
{
    if (!files_work_state_init())
    {
        return CLI_NO_CODE;
    }
    const int code = EMBED_CALL(files_validate, CommandLineFilesCtx, .words = args->words, .count = args->count);
    files_release();
    return code;
}

int cli_format_files(const CliCommandLineWordsCountCtx *args)
{
    if (!files_work_state_init())
    {
        return CLI_NO_CODE;
    }
    const int code = EMBED_CALL(files_format, CommandLineFilesCtx, .words = args->words, .count = args->count);
    files_release();
    return code;
}
