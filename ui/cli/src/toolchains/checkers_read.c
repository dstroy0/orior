// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// checkers_read->c: the findings a checker wrote read as the protocol's diagnostics, by file: a finding a line, or the
// JSON of ruff, ESLint and ShellCheck
#include "../command_line/pool.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "checkers_internal.h"

// the most bytes of a finding's fixes
#define CLI_CHECK_FIXES (1u << 16u)

// A check being read, under the reading's lock: the checker's name, the tree, the file `-` stands for, the findings,
// and the tree a checker's JSON is read into, in a cell sized to that JSON and given back as the reading ends
typedef struct
{
    const char *name;
    const char *root;
    const char *path;
    CliFindings *findings;
    CliJson json;
    char fixes[CLI_CHECK_FIXES];
} CliCheckRead;

static CliCheckRead s_capacity;
static CliCheckRead *s_read;

typedef struct
{
    const char *path;
    char *key_out;
    unsigned int node;
    const char *key;
    unsigned long long otherwise;
    const char *otherwise_text;
    unsigned long long *length;
    const char *out;
    unsigned long long size;
    VerbaLinea *out_verba_linea_pointer;
    unsigned int place;
} CheckersReadCtx;
// a path's key: with forward slashes, in lower case
EMBED_INLINE void cli_key(const CheckersReadCtx *args)
{
    memor.cpy(&(MemoriaCfg){.dst = args->key_out,
                            .src = args->path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}) + 1u});
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->key_out, .cap = CLI_POOL_BYTES});
    EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = args->key_out, .size = length, .from = '\\',
               .to = '/');
    EMBED_CALL(cli_text_size_from.lower, CliTextSizeFromCfg, .text_out = args->key_out, .size = length);
}

unsigned int cli_checkers_file(const CliCheckersPathCtx *args)
{
    char key[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_key, CheckersReadCtx, .path = args->path, .key_out = key);
    for (unsigned int at = 0u; at < args->findings->files; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->findings->key[at], .other = key, .cap = CLI_POOL_BYTES}))
        {
            return at;
        }
    }
    if (args->findings->files >= CLI_CHECK_FILES)
    {
        return CLI_CHECK_FILES;
    }
    if (!args->first)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = args->findings->path[args->findings->files],
                          .src = args->path,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}) + 1u});
        memor.cpy(&(MemoriaCfg){.dst = args->findings->key[args->findings->files],
                                .src = key,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_POOL_BYTES}) + 1u});
        args->findings->files += 1u;
        return args->findings->files - 1u;
    }
    for (unsigned int at = args->findings->files; at > 0u; at -= 1u)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = args->findings->path[at],
            .src = args->findings->path[at - 1u],
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->findings->path[at - 1u], .cap = CLI_POOL_BYTES}) + 1u});
        memor.cpy(&(MemoriaCfg){
            .dst = args->findings->key[at],
            .src = args->findings->key[at - 1u],
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->findings->key[at - 1u], .cap = CLI_POOL_BYTES}) + 1u});
    }
    for (unsigned int at = 0u; at < args->findings->findings; at += 1u)
    {
        args->findings->file_of[at] += 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->findings->path[0],
                            .src = args->path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = args->findings->key[0],
                            .src = key,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_POOL_BYTES}) + 1u});
    args->findings->files += 1u;
    return 0u;
}

unsigned int cli_checkers_file_get(const CliCheckersCtx *args)
{
    char supplied[CLI_PATH_MAX_LEN];
    const unsigned long long copied_len = (args->length < CLI_PATH_MAX_LEN) ? args->length : (CLI_PATH_MAX_LEN - 1ULL);
    memor.cpy(&(MemoriaCfg){.dst = supplied, .src = args->name, .bytes = copied_len});
    supplied[copied_len] = '\0';
    const int input = (cellul.eq(&(CatenaFinitaCfg){.src = supplied, .other = "-", .cap = CLI_POOL_BYTES})) ||
                      (cellul.eq(&(CatenaFinitaCfg){.src = supplied, .other = "<stdin>", .cap = CLI_POOL_BYTES})) ||
                      (cellul.eq(&(CatenaFinitaCfg){.src = supplied, .other = "stdin", .cap = CLI_POOL_BYTES}));
    if (input && (s_read->path != NULL))
    {
        return EMBED_CALL(cli_checkers_path_internal.file, CliCheckersPathCtx, .findings = s_read->findings,
                          .path = s_read->path, .first = 0);
    }
    const int absolute =
        (supplied[0] == '/') || (supplied[0] == '\\') || ((supplied[0] != '\0') && (supplied[1] == ':'));
    char path[CLI_PATH_MAX_LEN];
    if (absolute)
    {
        memor.cpy(&(MemoriaCfg){.dst = path,
                                .src = supplied,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = supplied, .cap = CLI_POOL_BYTES}) + 1u});
    }
    else
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_read->root, .relative = supplied, .joined = path);
    }
    return EMBED_CALL(cli_checkers_path_internal.file, CliCheckersPathCtx, .findings = s_read->findings, .path = path,
                      .first = 0);
}

void cli_checkers_finding_append(const CliCheckersFileCtx *args)
{
    CliFindings *const findings = s_read->findings;
    if ((args->file >= findings->files) || (findings->findings >= CLI_CHECK_FINDINGS))
    {
        return;
    }
    VerbaLinea out = {findings->text + findings->used, CLI_CHECK_TEXT - findings->used, 0ULL};
    const unsigned long long line = (args->finding->line > 0ULL) ? (args->finding->line - 1ULL) : 0ULL;
    const unsigned long long col = (args->finding->col > 0ULL) ? (args->finding->col - 1ULL) : 0ULL;
    mmgr_line_put(&out, "{\"range\":{\"start\":{\"line\":");
    mmgr_line_decimal(&out, line, 0u);
    mmgr_line_put(&out, ",\"character\":");
    mmgr_line_decimal(&out, col, 0u);
    mmgr_line_put(&out, "},\"end\":{\"line\":");
    if (args->finding->has_end)
    {
        mmgr_line_decimal(&out, (args->finding->end_line > 0ULL) ? (args->finding->end_line - 1ULL) : 0ULL, 0u);
        mmgr_line_put(&out, ",\"character\":");
        mmgr_line_decimal(&out, (args->finding->end_col > 0ULL) ? (args->finding->end_col - 1ULL) : 0ULL, 0u);
    }
    else
    {
        mmgr_line_decimal(&out, line, 0u);
        mmgr_line_put(&out, ",\"character\":");
        mmgr_line_decimal(&out, args->finding->col, 0u);
    }
    mmgr_line_put(&out, "}},\"severity\":");
    mmgr_line_decimal(&out, args->finding->severity, 0u);
    mmgr_line_put(&out, ",\"message\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &out,
               .text_bytes = (const unsigned char *)args->finding->message, .length = args->finding->message_length);
    mmgr_line_put(&out, ",\"source\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &out, .text = s_read->name,
               .length = cellul.len(&(CatenaFinitaCfg){.src = s_read->name, .cap = CLI_POOL_BYTES}));
    mmgr_line_put(&out, ",\"code\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = &out,
               .text_bytes = (const unsigned char *)args->finding->code, .length = args->finding->code_length);
    mmgr_line_put(&out, ",\"data\":{\"orior\":");
    mmgr_line_put(&out, args->finding->fixes);
    mmgr_line_put(&out, "}}");
    if (!mmgr_line_ok(&out))
    {
        return;
    }
    findings->file_of[findings->findings] = args->file;
    findings->start[findings->findings] = findings->used;
    // a diagnostic is below CLI_CHECK_TEXT bytes, which an unsigned int represents
    findings->length[findings->findings] = (unsigned int)out.at;
    findings->used += (unsigned int)out.at;
    findings->findings += 1u;
}

// a whole number member of a node, or `otherwise` where it is none
EMBED_INLINE unsigned long long cli_span_is_whole_word(const CheckersReadCtx *args)
{
    unsigned int node = args->node;
    const unsigned int member = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                           .json_read = &s_read->json, .object = node, .name = args->key);
    if (!EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = &s_read->json, .node = member,
                    .type = CFG_JSON_NUMBER))
    {
        return args->otherwise;
    }
    const CliJsonNode *const one = &s_read->json.node[member];
    const char *const digits = s_read->json.text + one->text;
    unsigned long long value = 0ULL;
    for (unsigned long long at = 0ULL; at < one->length; at += 1ULL)
    {
        if ((digits[at] < '0') || (digits[at] > '9'))
        {
            return args->otherwise;
        }
        value = (value * 10ULL) + (unsigned long long)(digits[at] - '0');
    }
    return (one->length > 0ULL) ? value : args->otherwise;
}

// a string member of a node with its length, `otherwise` where it is none
EMBED_INLINE const char *cli_text(const CheckersReadCtx *args)
{
    unsigned int node = args->node;
    unsigned long long *length = args->length;
    const char *const text =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_read->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                      .json_read = &s_read->json, .object = node, .name = args->key),
                   .length_pointer = length);
    if (text == NULL)
    {
        *length = cellul.len(&(CatenaFinitaCfg){.src = args->otherwise_text, .cap = CLI_POOL_BYTES});
    }
    return (text != NULL) ? text : args->otherwise_text;
}

// the checker's JSON read into a tree sized to it: its root, or CLI_JSON_NONE
EMBED_INLINE unsigned int cli_json_get(const CheckersReadCtx *args)
{
    unsigned long long size = args->size;
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unread = {why, sizeof(why), 0ULL};
    return EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &s_read->json,
                      .text = args->out, .size = size, .nodes_more = 0u, .text_more = 0ULL, .err_msg = &unread,
                      .owner = g_cli_toolchains_owner);
}

// a place in ruff's findings, its row and column counted from one, written counted from zero as `{"line", "col"}`
EMBED_INLINE void cli_position_write(const CheckersReadCtx *args)
{
    const unsigned long long row =
        EMBED_CALL(cli_span_is_whole_word, CheckersReadCtx, .node = args->place, .key = "row", .otherwise = 1ULL);
    const unsigned long long column =
        EMBED_CALL(cli_span_is_whole_word, CheckersReadCtx, .node = args->place, .key = "column", .otherwise = 1ULL);
    mmgr_line_put(args->out_verba_linea_pointer, "{\"line\":");
    mmgr_line_decimal(args->out_verba_linea_pointer, (row > 0ULL) ? (row - 1ULL) : 0ULL, 0u);
    mmgr_line_put(args->out_verba_linea_pointer, ",\"col\":");
    mmgr_line_decimal(args->out_verba_linea_pointer, (column > 0ULL) ? (column - 1ULL) : 0ULL, 0u);
    mmgr_line_ch(args->out_verba_linea_pointer, '}');
}

// ruff's findings, each with the fix it gives
EMBED_INLINE void cli_ruff_read(const CheckersReadCtx *args)
{
    const char *out = args->out;
    const CliJson *const json = &s_read->json;
    const unsigned int root = EMBED_CALL(cli_json_get, CheckersReadCtx, .out = out, .size = args->size);
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = root, .index = at) != CLI_JSON_NONE;
         at += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = json, .array = root, .index = at);
        const unsigned int location = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                 .json_read = json, .object = one, .name = "location");
        const unsigned int end = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                            .object = one, .name = "end_location");
        const unsigned int fix = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                            .object = one, .name = "fix");
        const unsigned int edits = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                              .object = fix, .name = "edits");
        VerbaLinea fixes = {s_read->fixes, sizeof(s_read->fixes), 0ULL};
        if (EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json, .node = edits,
                       .type = CFG_JSON_ARRAY))
        {
            unsigned long long length = 0ULL;
            const char *const title = EMBED_CALL(cli_text, CheckersReadCtx, .node = fix, .key = "message",
                                                 .otherwise_text = "Fix it as ruff does", .length = &length);
            mmgr_line_put(&fixes, "[{\"title\":");
            EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &fixes, .text = title,
                       .length = length);
            mmgr_line_put(&fixes, ",\"edits\":[");
            for (unsigned int edit = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                    .json_read = json, .array = edits, .index = edit) != CLI_JSON_NONE;
                 edit += 1u)
            {
                const unsigned int one_edit = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                         .json_read = json, .array = edits, .index = edit);
                mmgr_line_put(&fixes, (edit != 0u) ? ",{\"from\":" : "{\"from\":");
                EMBED_CALL(cli_position_write, CheckersReadCtx, .out_verba_linea_pointer = &fixes,
                           .place = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                               .json_read = json, .object = one_edit, .name = "location"));
                mmgr_line_put(&fixes, ",\"to\":");
                EMBED_CALL(cli_position_write, CheckersReadCtx, .out_verba_linea_pointer = &fixes,
                           .place = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                               .json_read = json, .object = one_edit, .name = "end_location"));
                const char *const content = EMBED_CALL(cli_text, CheckersReadCtx, .node = one_edit, .key = "content",
                                                       .otherwise_text = "", .length = &length);
                mmgr_line_put(&fixes, ",\"text\":");
                EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &fixes,
                           .text = content, .length = length);
                mmgr_line_ch(&fixes, '}');
            }
            mmgr_line_put(&fixes, "]}]");
        }
        else
        {
            mmgr_line_put(&fixes, "[]");
        }
        mmgr_line_finish(&fixes);
        unsigned long long name_length = 0ULL;
        const char *const name = EMBED_CALL(cli_text, CheckersReadCtx, .node = one, .key = "filename",
                                            .otherwise_text = "-", .length = &name_length);
        CliFinding finding = {
            EMBED_CALL(cli_span_is_whole_word, CheckersReadCtx, .node = location, .key = "row", .otherwise = 1ULL),
            EMBED_CALL(cli_span_is_whole_word, CheckersReadCtx, .node = location, .key = "column", .otherwise = 1ULL),
            1,
            EMBED_CALL(cli_span_is_whole_word, CheckersReadCtx, .node = end, .key = "row", .otherwise = 1ULL),
            EMBED_CALL(cli_span_is_whole_word, CheckersReadCtx, .node = end, .key = "column", .otherwise = 1ULL),
            2u,
            NULL,
            0ULL,
            NULL,
            0ULL,
            mmgr_line_ok(&fixes) ? s_read->fixes : "[]"};
        finding.message = EMBED_CALL(cli_text, CheckersReadCtx, .node = one, .key = "message", .otherwise_text = "",
                                     .length = &finding.message_length);
        finding.code = EMBED_CALL(cli_text, CheckersReadCtx, .node = one, .key = "code", .otherwise_text = "",
                                  .length = &finding.code_length);
        EMBED_CALL(cli_checkers_file_internal.finding_append, CliCheckersFileCtx,
                   .file =
                       EMBED_CALL(cli_checkers_internal.file_get, CliCheckersCtx, .name = name, .length = name_length),
                   .finding = &finding);
    }
}

// ESLint's findings, by file
EMBED_INLINE void cli_eslint_read(const CheckersReadCtx *args)
{
    const CliJson *const json = &s_read->json;
    const unsigned int root = cli_json_get(args);
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = root, .index = at) != CLI_JSON_NONE;
         at += 1u)
    {
        const unsigned int file = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = json, .array = root, .index = at);
        unsigned long long name_length = 0ULL;
        const char *const name = EMBED_CALL(cli_text, CheckersReadCtx, .node = file, .key = "filePath",
                                            .otherwise_text = "-", .length = &name_length);
        const unsigned int messages = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                 .json_read = json, .object = file, .name = "messages");
        for (unsigned int index = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                 .json_read = json, .array = messages, .index = index) != CLI_JSON_NONE;
             index += 1u)
        {
            const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                .json_read = json, .array = messages, .index = index);
            const int has_end = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json,
                                           .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                              .json_read = json, .object = one, .name = "endLine"),
                                           .type = CFG_JSON_NUMBER);
            CliFinding finding = {
                EMBED_CALL(cli_span_is_whole_word, CheckersReadCtx, .node = one, .key = "line", .otherwise = 1ULL),
                EMBED_CALL(cli_span_is_whole_word, CheckersReadCtx, .node = one, .key = "column", .otherwise = 1ULL),
                has_end,
                EMBED_CALL(cli_span_is_whole_word, CheckersReadCtx, .node = one, .key = "endLine", .otherwise = 1ULL),
                EMBED_CALL(cli_span_is_whole_word, CheckersReadCtx, .node = one, .key = "endColumn", .otherwise = 1ULL),
                (EMBED_CALL(cli_span_is_whole_word, CheckersReadCtx, .node = one, .key = "severity",
                            .otherwise = 0ULL) == 2ULL)
                    ? 1u
                    : 2u,
                NULL,
                0ULL,
                NULL,
                0ULL,
                "[]"};
            finding.message = EMBED_CALL(cli_text, CheckersReadCtx, .node = one, .key = "message", .otherwise_text = "",
                                         .length = &finding.message_length);
            finding.code = EMBED_CALL(cli_text, CheckersReadCtx, .node = one, .key = "ruleId", .otherwise_text = "",
                                      .length = &finding.code_length);
            EMBED_CALL(
                cli_checkers_file_internal.finding_append, CliCheckersFileCtx,
                .file = EMBED_CALL(cli_checkers_internal.file_get, CliCheckersCtx, .name = name, .length = name_length),
                .finding = &finding);
        }
    }
}

// ShellCheck's findings, each with its level as a severity and its code as SC and the number
EMBED_INLINE void cli_shellcheck_read(const CheckersReadCtx *args)
{
    const CliJson *const json = &s_read->json;
    const unsigned int comments = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                             .object = cli_json_get(args), .name = "comments");
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = comments, .index = at) != CLI_JSON_NONE;
         at += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = json, .array = comments, .index = at);
        unsigned long long length = 0ULL;
        const char *const level =
            EMBED_CALL(cli_text, CheckersReadCtx, .node = one, .key = "level", .otherwise_text = "", .length = &length);
        unsigned int severity;
        if (cellul.eq(&(CatenaFinitaCfg){.src = level, .other = "error", .cap = CLI_POOL_BYTES}))
        {
            severity = 1u;
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = level, .other = "warning", .cap = CLI_POOL_BYTES}))
        {
            severity = 2u;
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = level, .other = "info", .cap = CLI_POOL_BYTES}))
        {
            severity = 3u;
        }
        else
        {
            severity = 4u;
        }
        char code[32] = "";
        if (EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = one, .name = "code"),
                       .type = CFG_JSON_NUMBER))
        {
            VerbaLinea written = {code, sizeof(code), 0ULL};
            mmgr_line_put(&written, "SC");
            mmgr_line_decimal(
                &written,
                EMBED_CALL(cli_span_is_whole_word, CheckersReadCtx, .node = one, .key = "code", .otherwise = 0ULL), 0u);
            mmgr_line_finish(&written);
        }
        const int has_end = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json,
                                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                          .json_read = json, .object = one, .name = "endLine"),
                                       .type = CFG_JSON_NUMBER);
        CliFinding finding = {
            EMBED_CALL(cli_span_is_whole_word, CheckersReadCtx, .node = one, .key = "line", .otherwise = 1ULL),
            EMBED_CALL(cli_span_is_whole_word, CheckersReadCtx, .node = one, .key = "column", .otherwise = 1ULL),
            has_end,
            EMBED_CALL(cli_span_is_whole_word, CheckersReadCtx, .node = one, .key = "endLine", .otherwise = 1ULL),
            EMBED_CALL(cli_span_is_whole_word, CheckersReadCtx, .node = one, .key = "endColumn", .otherwise = 1ULL),
            severity,
            NULL,
            0ULL,
            code,
            cellul.len(&(CatenaFinitaCfg){.src = code, .cap = CLI_POOL_BYTES}),
            "[]"};
        finding.message = EMBED_CALL(cli_text, CheckersReadCtx, .node = one, .key = "message", .otherwise_text = "",
                                     .length = &finding.message_length);
        unsigned long long name_length = 0ULL;
        const char *const name = EMBED_CALL(cli_text, CheckersReadCtx, .node = one, .key = "file",
                                            .otherwise_text = "-", .length = &name_length);
        EMBED_CALL(cli_checkers_file_internal.finding_append, CliCheckersFileCtx,
                   .file =
                       EMBED_CALL(cli_checkers_internal.file_get, CliCheckersCtx, .name = name, .length = name_length),
                   .finding = &finding);
    }
}

void cli_checkers_read(const CliCheckersNameCtx *args)
{
    const char *name = args->name;
    const char *root = args->root;
    const char *path = args->path;
    CliFindings *findings = args->findings;
    s_read = &s_capacity;
    s_read->name = name;
    s_read->root = root;
    s_read->path = path;
    s_read->findings = findings;
    findings->files = 0u;
    findings->findings = 0u;
    findings->used = 0u;
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->reads, .other = "ruff", .cap = CLI_POOL_BYTES}))
    {
        EMBED_CALL(cli_ruff_read, CheckersReadCtx, .out = args->out, .size = args->out_size);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->reads, .other = "eslint", .cap = CLI_POOL_BYTES}))
    {
        EMBED_CALL(cli_eslint_read, CheckersReadCtx, .out = args->out, .size = args->out_size);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->reads, .other = "shellcheck", .cap = CLI_POOL_BYTES}))
    {
        EMBED_CALL(cli_shellcheck_read, CheckersReadCtx, .out = args->out, .size = args->out_size);
    }
    else
    {
        // what it wrote, then, for a checker that writes its findings a line each, what it printed
        EMBED_CALL(cli_checkers_text_internal.lines_read, CliCheckersTextCtx, .text = args->out,
                   .size = args->out_size);
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->reads, .other = "lines", .cap = CLI_POOL_BYTES}))
        {
            EMBED_CALL(cli_checkers_text_internal.lines_read, CliCheckersTextCtx, .text = args->err,
                       .size = args->err_size);
        }
    }
    g_cli_guard->persistent_buf_release(s_read->json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), s_read->json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_read->json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    s_read = NULL;
}
