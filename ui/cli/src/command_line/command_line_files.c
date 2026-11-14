// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line_files.c: `imp run run-file`, a file run with its language's toolchain, in bash at the tree's top
// folder, attached to this terminal, exiting with its code; `imp run validate`, each file validated with the tool
// plugin for its language, what it found and its verdict printed, or the whole report as JSON; and `imp edit
// format`, each file formatted in place with its language's formatter, or with --check each that would change named
#include "../code/git.h"
#include "../plugins/validate.h"
#include "../runs/run_file.h"
#include "../runs/runner.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "../toolchains/format.h"
#include "../toolchains/toolchains.h"
#include "command_line_internal.h"
#include "pool.h"

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
    s_work_state = g_cli_guard->temporary_buf_alloc(sizeof(*s_work_state), g_cli_command_line_owner);
    if (s_work_state == NULL)
    {
        EMBED_CALL(cli_command_line_internal.err, CliCommandLineCtx, .text = "a command on files could not be taken");
    }
    return s_work_state != NULL;
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
    int check;
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
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->word_text, .cap = CLI_POOL_BYTES});
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
                   .two = ": no plugin opens it, and so impero knows no way to run it", .three = "");
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

// the first item of the array member of `node` named `name`, or CLI_JSON_NONE where it has none
EMBED_INLINE unsigned int files_first(const CommandLineFilesCtx *args)
{
    const unsigned int array = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                          .json_read = &s_work_state->json, .object = args->node, .name = args->name);
    return EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = &s_work_state->json,
                      .node = array, .type = CFG_JSON_ARRAY)
               ? s_work_state->json.node[array].first
               : CLI_JSON_NONE;
}

// the strings of an array member joined by `between`
EMBED_INLINE void files_join(const CommandLineFilesCtx *args)
{
    const unsigned int first = EMBED_CALL(files_first, CommandLineFilesCtx, .node = args->node, .name = args->name);
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
    if (!EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = &s_work_state->json,
                    .node = number, .type = CFG_JSON_NUMBER))
    {
        return 0.0;
    }
    char text[64];
    const unsigned long long length = s_work_state->json.node[number].length;
    const unsigned long long copied_len = (length < sizeof(text)) ? length : sizeof(text) - 1u;
    memor.cpy(&(MemoriaCfg){
        .dst = text, .src = s_work_state->json.text + s_work_state->json.node[number].text, .bytes = copied_len});
    text[copied_len] = '\0';
    return cellul.to_double(&(TransfiguroCfg){.src = text});
}

// the text member of `node` named `name`, or NULL where it has none
EMBED_INLINE const char *files_member_text(const CommandLineFilesCtx *args)
{
    return EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_work_state->json,
                      .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                         .json_read = &s_work_state->json, .object = args->node, .name = args->name),
                      .length_pointer = NULL);
}

// the work state's line printed on the output
EMBED_INLINE void files_line_print(const CommandLineFilesCtx *args)
{
    EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx, .text = s_work_state->line,
               .size = mmgr_line_ok(args->out) ? args->out->at : 0ULL);
}

// a check of a report printed: its settings, "as it is" where it gives none, its seconds to a tenth, its count of
// errors and the barriers that stopped it, where any did
EMBED_INLINE void files_check_print(const CommandLineFilesCtx *args)
{
    const unsigned int check = args->node;
    VerbaLinea line = {s_work_state->line, sizeof(s_work_state->line), 0ULL};
    mmgr_line_put(&line, "  checked ");
    if (EMBED_CALL(files_first, CommandLineFilesCtx, .node = check, .name = "settings") == CLI_JSON_NONE)
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
    if (EMBED_CALL(files_first, CommandLineFilesCtx, .node = check, .name = "barriers") != CLI_JSON_NONE)
    {
        mmgr_line_put(&line, ", stopped by ");
        EMBED_CALL(files_join, CommandLineFilesCtx, .out = &line, .node = check, .name = "barriers", .between = ", ");
    }
    EMBED_CALL(files_line_print, CommandLineFilesCtx, .out = &line);
}

// a finding's message `text` printed after what the line `out` holds: its first line not blank there, each other line
// not blank on a line of its own set in by four spaces, a line's carriage return dropped; `out` printed as it is
// where every line is blank
EMBED_INLINE void files_message_print(const CommandLineFilesCtx *args)
{
    VerbaLinea *const line = args->out;
    const char *rest = args->text;
    int head = 1;
    for (;;)
    {
        const char *const end = cellul.chr(&(CatenaFinitaCfg){.src = rest, .cap = CLI_POOL_BYTES, .byte = '\n'});
        const unsigned long long size = (end != NULL)
                                            ? (unsigned long long)(end - rest)
                                            : cellul.len(&(CatenaFinitaCfg){.src = rest, .cap = CLI_POOL_BYTES});
        unsigned long long from = 0ULL;
        unsigned long long to = 0ULL;
        EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = rest, .size = size,
                   .from_u64s = &from, .to_u64s = &to);
        if ((to > from) && !head)
        {
            line->at = 0ULL;
            mmgr_line_put(line, "    ");
        }
        if (to > from)
        {
            const unsigned long long len = ((size > 0ULL) && (rest[size - 1ULL] == '\r')) ? size - 1ULL : size;
            mmgr_line_put_n(line, rest, len);
            EMBED_CALL(files_line_print, CommandLineFilesCtx, .out = line);
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
        EMBED_CALL(files_line_print, CommandLineFilesCtx, .out = line);
    }
}

// a finding of a report printed: the file, its line and column counted from 1, its type and its message, then what it
// lifted, where it gives it, on a line of its own set in by four spaces
EMBED_INLINE void files_finding_print(const CommandLineFilesCtx *args)
{
    const unsigned int finding = args->node;
    const char *const type = EMBED_CALL(files_member_text, CommandLineFilesCtx, .node = finding, .name = "type");
    const char *const message = EMBED_CALL(files_member_text, CommandLineFilesCtx, .node = finding, .name = "message");
    const char *const lifted = EMBED_CALL(files_member_text, CommandLineFilesCtx, .node = finding, .name = "lifted");
    VerbaLinea line = {s_work_state->line, sizeof(s_work_state->line), 0ULL};
    mmgr_line_put(&line, args->file);
    mmgr_line_ch(&line, ':');
    // a line and a column counted from 0 are whole numbers
    mmgr_line_decimal(
        &line,
        (unsigned long long)EMBED_CALL(files_number, CommandLineFilesCtx, .node = finding, .name = "line") + 1ULL, 1u);
    mmgr_line_ch(&line, ':');
    mmgr_line_decimal(
        &line, (unsigned long long)EMBED_CALL(files_number, CommandLineFilesCtx, .node = finding, .name = "col") + 1ULL,
        1u);
    mmgr_line_put(&line, ": ");
    mmgr_line_put(&line, (type != NULL) ? type : "");
    mmgr_line_put(&line, ": ");
    EMBED_CALL(files_message_print, CommandLineFilesCtx, .out = &line, .text = (message != NULL) ? message : "");
    if (lifted != NULL)
    {
        line.at = 0ULL;
        mmgr_line_put(&line, "    ");
        mmgr_line_put(&line, lifted);
        EMBED_CALL(files_line_print, CommandLineFilesCtx, .out = &line);
    }
}

// each check of a report printed, then each finding, then the file's verdict
EMBED_INLINE void files_report_print(const CommandLineFilesCtx *args)
{
    for (unsigned int check = EMBED_CALL(files_first, CommandLineFilesCtx, .node = args->report, .name = "checks");
         check != CLI_JSON_NONE; check = s_work_state->json.node[check].next)
    {
        EMBED_CALL(files_check_print, CommandLineFilesCtx, .node = check);
    }
    for (unsigned int finding = EMBED_CALL(files_first, CommandLineFilesCtx, .node = args->report, .name = "findings");
         finding != CLI_JSON_NONE; finding = s_work_state->json.node[finding].next)
    {
        EMBED_CALL(files_finding_print, CommandLineFilesCtx, .node = finding, .file = args->file);
    }
    const int passes =
        EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = &s_work_state->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                      .json_read = &s_work_state->json, .object = args->report, .name = "passes"),
                   .type = CFG_JSON_TRUE);
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
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->words[at], .other = args->flag, .cap = CLI_POOL_BYTES}))
        {
            *flagged = 1;
            continue;
        }
        if (cellul.starts(&(CatenaFinitaCfg){.src = args->words[at], .other = "--", .cap = CLI_POOL_BYTES}))
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
        if (cellul.eq(&(CatenaFinitaCfg){.src = file, .other = "--json", .cap = CLI_POOL_BYTES}))
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
        if (!EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = &s_work_state->json,
                        .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                           .json_read = &s_work_state->json, .object = tree, .name = "passes"),
                        .type = CFG_JSON_TRUE))
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

// the file named `file` formatted in place with its language's formatter, or with `check` named on the output where
// it would change; 0, or CLI_NO_CODE where it is not known, read, formatted or written, or with `check` would change
EMBED_INLINE int files_format_file(const CommandLineFilesCtx *args)
{
    static char s_err_buf[4096];
    const char *const file = args->file;
    char full[CLI_PATH_MAX_LEN];
    EMBED_CALL(files_full, CommandLineFilesCtx, .word_text = file, .full = full);
    char language[CLI_NAME_MAX_LEN];
    if (!EMBED_CALL(cli_format_table.language_get, CliFormatCfg, .path = file, .id = language))
    {
        EMBED_CALL(files_err, CommandLineFilesCtx, .one = file,
                   .two = ": no plugin opens it, and so no formatter knows it", .three = "");
        return CLI_NO_CODE;
    }
    const unsigned long long size =
        qry_member_file_read(file, (unsigned char *)s_work_state->read, sizeof(s_work_state->read));
    if ((size == QRY_MEMBER_UNREAD) ||
        !EMBED_CALL(cli_json_tree_size.utf8_valid, CliJsonTreeSizeCfg, .bytes = s_work_state->read, .size = size))
    {
        EMBED_CALL(files_err, CommandLineFilesCtx, .one = file, .two = ": it could not be read", .three = "");
        return CLI_NO_CODE;
    }
    VerbaLinea formatted = {s_work_state->made, sizeof(s_work_state->made), 0ULL};
    VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ULL};
    if (!EMBED_CALL(cli_format_path.format, CliFormatPathCfg, .path = full, .language = language,
                    .text = s_work_state->read, .length = size, .out = &formatted, .err_msg = &err_msg))
    {
        EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = s_err_buf,
                   .size = mmgr_line_ok(&err_msg) ? err_msg.at : 0ULL);
        return CLI_NO_CODE;
    }
    unsigned long long length = 0ULL;
    const char *const text =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_work_state->json,
                   .node = EMBED_CALL(files_tree, CommandLineFilesCtx, .text = s_work_state->made,
                                      .size = mmgr_line_ok(&formatted) ? formatted.at : 0ULL),
                   .length_pointer = &length);
    if ((text != NULL) && (length == size) &&
        (memor.cmp(&(MemoriaCfg){.src = text, .other = s_work_state->read, .bytes = size}) == 0))
    {
        return 0;
    }
    VerbaLinea line = {s_work_state->line, sizeof(s_work_state->line), 0ULL};
    mmgr_line_put(&line, file);
    if (args->check)
    {
        mmgr_line_put(&line, " would change");
        EMBED_CALL(files_line_print, CommandLineFilesCtx, .out = &line);
        return CLI_NO_CODE;
    }
    if ((text == NULL) || !EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = file,
                                      .bytes = text, .size = length))
    {
        EMBED_CALL(files_err, CommandLineFilesCtx, .one = file, .two = ": it could not be written", .three = "");
        return CLI_NO_CODE;
    }
    mmgr_line_put(&line, " formatted");
    EMBED_CALL(files_line_print, CommandLineFilesCtx, .out = &line);
    return 0;
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
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        const char *const file = args->words[at];
        if (!cellul.eq(&(CatenaFinitaCfg){.src = file, .other = "--check", .cap = CLI_POOL_BYTES}) &&
            (EMBED_CALL(files_format_file, CommandLineFilesCtx, .file = file, .check = check) != 0))
        {
            code = CLI_NO_CODE;
        }
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
    g_cli_guard->persistent_buf_release(s_work_state, g_cli_command_line_owner);
    s_work_state = NULL;
    return code;
}

int cli_validate_files(const CliCommandLineWordsCountCtx *args)
{
    if (!files_work_state_init())
    {
        return CLI_NO_CODE;
    }
    const int code = EMBED_CALL(files_validate, CommandLineFilesCtx, .words = args->words, .count = args->count);
    g_cli_guard->persistent_buf_release(s_work_state, g_cli_command_line_owner);
    s_work_state = NULL;
    return code;
}

int cli_format_files(const CliCommandLineWordsCountCtx *args)
{
    if (!files_work_state_init())
    {
        return CLI_NO_CODE;
    }
    const int code = EMBED_CALL(files_format, CommandLineFilesCtx, .words = args->words, .count = args->count);
    g_cli_guard->persistent_buf_release(s_work_state, g_cli_command_line_owner);
    s_work_state = NULL;
    return code;
}
