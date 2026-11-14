// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// format_width.c: the width a formatter wraps a file's lines to, read from the project's settings for it, and the
// nearest settings file read
#include "../command_line/pool.h"
#include "../text/text.h"
#include "format_internal.h"

// A span of a text: where it starts and its bytes
typedef struct
{
    const char *at;
    unsigned long long length;
} CliSpan;

typedef struct
{
    char letter;
    CliSpan span;
    CliSpan part;
    const char *key;
    unsigned int *number;
    const char *text;
} FormatWidthCtx;
// whether a byte is ASCII's whitespace
EMBED_INLINE int cli_blank(const FormatWidthCtx *args)
{
    return (args->letter == ' ') || ((args->letter >= '\t') && (args->letter <= '\r'));
}

// the span with the whitespace at either end left off
EMBED_INLINE CliSpan cli_trim(const FormatWidthCtx *args)
{
    CliSpan span = args->span;
    while ((span.length > 0ULL) && EMBED_CALL(cli_blank, FormatWidthCtx, .letter = span.at[0]))
    {
        span = (CliSpan){span.at + 1, span.length - 1ULL};
    }
    while ((span.length > 0ULL) && EMBED_CALL(cli_blank, FormatWidthCtx, .letter = span.at[span.length - 1ULL]))
    {
        span.length -= 1ULL;
    }
    return span;
}

int cli_format_utf8(const CliFormatBytesCtx *args)
{
    for (unsigned long long at = 0ULL; at < args->size;)
    {
        unsigned int letter = 0u;
        const unsigned long long step = EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                                                   .text_bytes = (const unsigned char *)args->bytes,
                                                   .length = args->size, .at = at, .letter = &letter);
        if ((step == 1ULL) && ((unsigned char)args->bytes[at] >= 0x80u))
        {
            return 0;
        }
        at += step;
    }
    return 1;
}

void cli_format_fail(const CliFormatErrMsgCtx *args)
{
    const char *name = args->path;
    const unsigned long long name_end =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}), .set = "/\\");
    name = (name_end != 0ULL) ? (args->path + name_end) : name;
    mmgr_line_put(args->err_msg, args->program);
    mmgr_line_put(args->err_msg, " could not format ");
    mmgr_line_put(args->err_msg, (name[0] != '\0') ? name : args->path);
    mmgr_line_put(args->err_msg, ": ");
    const CliSpan whole =
        EMBED_CALL(cli_trim, FormatWidthCtx, .span = (CliSpan){args->run_result->err, args->run_result->err_size});
    if (whole.length == 0ULL)
    {
        mmgr_line_put(args->err_msg, "it printed nothing");
        return;
    }
    unsigned long long at = 0ULL;
    for (unsigned int line = 0u; (line < 6u) && (at < whole.length); line += 1u)
    {
        const CliTextLine bounds =
            EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = whole.at, .at = at, .size = whole.length);
        const unsigned long long end = bounds.stop;
        unsigned long long length = end - at;
        length -= bounds.stop - bounds.cut;
        if (line != 0u)
        {
            mmgr_line_ch(args->err_msg, '\n');
        }
        EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = args->err_msg,
                   .text_bytes = (const unsigned char *)whole.at + at, .length = length);
        at = end + 1ULL;
    }
}

int cli_format_nearest(const CliFormatPathCtx *args)
{
    char folder[CLI_PATH_MAX_LEN];
    char up[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_path_out_type_internal.path_parent, CliPathOutTypeCtx, .path = args->path, .out = folder))
    {
        return 0;
    }
    for (;;)
    {
        for (unsigned int at = 0u; at < args->count; at += 1u)
        {
            char file[CLI_PATH_MAX_LEN];
            EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = args->names[at], .joined = file);
            unsigned long long size = 0ULL;
            const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = file,
                                                          .size_pointer = &size, .owner = g_cli_toolchains_owner);
            if (bytes == NULL)
            {
                continue;
            }
            // the settings stored in a cell sized to the file, ended by a 0, the ones stored before given back
            g_cli_guard->persistent_buf_release(g_cli_format.settings, g_cli_toolchains_owner);
            g_cli_format.settings = EMBED_CALL(cli_format_bytes_internal.utf8, CliFormatBytesCtx,
                                               .bytes = (const char *)bytes, .size = size)
                                        ? g_cli_guard->temporary_buf_alloc(size + 1ULL, g_cli_toolchains_owner)
                                        : NULL;
            const int text = g_cli_format.settings != NULL;
            if (text)
            {
                memor.cpy(&(MemoriaCfg){.dst = g_cli_format.settings, .src = bytes, .bytes = size});
                g_cli_format.settings[size] = '\0';
            }
            g_cli_format.settings_size = text ? size : 0ULL;
            g_cli_guard->persistent_buf_release((void *)(embed_word)bytes, (g_cli_toolchains_owner));
            if (text)
            {
                return 1;
            }
        }
        if (!EMBED_CALL(cli_path_out_type_internal.path_parent, CliPathOutTypeCtx, .path = folder, .out = up) ||
            (cellul.eq(&(CatenaFinitaCfg){.src = up, .other = folder, .cap = CLI_POOL_BYTES})))
        {
            return 0;
        }
        memor.cpy(&(MemoriaCfg){
            .dst = folder, .src = up, .bytes = cellul.len(&(CatenaFinitaCfg){.src = up, .cap = CLI_POOL_BYTES}) + 1u});
    }
}

// the whole number set for `key` in a part of a line, `key = 88`, `key: 88` or `"key": 88`: 1 with it into
// `number`, or 0
EMBED_INLINE int cli_number_after(const FormatWidthCtx *args)
{
    unsigned int *number = args->number;
    CliSpan rest = EMBED_CALL(cli_trim, FormatWidthCtx, .span = args->part);
    const unsigned long long lead = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg,
                                               .text = rest.at, .at = 0ULL, .size = rest.length, .set = "\"'");
    rest = (CliSpan){rest.at + lead, rest.length - lead};
    const unsigned long long key_length = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_POOL_BYTES});
    if ((rest.length < key_length) ||
        (memor.cmp(&(MemoriaCfg){.src = rest.at, .other = args->key, .bytes = key_length}) != 0))
    {
        return 0;
    }
    rest = (CliSpan){rest.at + key_length, rest.length - key_length};
    const unsigned long long prefix_after = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg,
                                                       .text = rest.at, .at = 0ULL, .size = rest.length, .set = "\"'");
    rest = (CliSpan){rest.at + prefix_after, rest.length - prefix_after};
    rest = EMBED_CALL(cli_trim, FormatWidthCtx, .span = rest);
    if ((rest.length == 0ULL) || ((rest.at[0] != '=') && (rest.at[0] != ':')))
    {
        return 0;
    }
    rest = EMBED_CALL(cli_trim, FormatWidthCtx, .span = (CliSpan){rest.at + 1, rest.length - 1ULL});
    unsigned long long at = 0ULL;
    const unsigned long long value = EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = rest.at, .at = 0ULL,
                                                .size = rest.length, .end = &at);
    if (value > 0xffffffffULL)
    {
        return 0;
    }
    // a value checked to fit 32 bits
    *number = (unsigned int)value;
    return at > 0ULL;
}

int cli_format_number_in(const CliFormatTextCtx *args)
{
    int inside = args->table == NULL;
    for (const char *line = args->text; line[0] != '\0';)
    {
        const char *end = cellul.chr(&(CatenaFinitaCfg){.src = line, .cap = CLI_POOL_BYTES, .byte = '\n'});
        end = (end != NULL) ? end : (line + cellul.len(&(CatenaFinitaCfg){.src = line, .cap = CLI_POOL_BYTES}));
        const char *const hash =
            memor.chr(&(MemoriaCfg){.src = line, .val = '#', .bytes = (unsigned long long)(end - line)});
        const CliSpan code =
            EMBED_CALL(cli_trim, FormatWidthCtx,
                       .span = (CliSpan){line, (unsigned long long)(((hash != NULL) ? hash : end) - line)});
        line = (end[0] == '\n') ? (end + 1) : end;
        if ((code.length > 0ULL) && (code.at[0] == '[') && (code.at[code.length - 1ULL] == ']'))
        {
            CliSpan name = code;
            const unsigned long long lead = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg,
                                                       .text = name.at, .at = 0ULL, .size = name.length, .set = "[]");
            name = (CliSpan){name.at + lead, name.length - lead};
            name.length = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = name.at,
                                     .at = name.length, .set = "[]");
            name = EMBED_CALL(cli_trim, FormatWidthCtx, .span = name);
            inside = (args->table != NULL) &&
                     (name.length == cellul.len(&(CatenaFinitaCfg){.src = args->table, .cap = CLI_POOL_BYTES})) &&
                     (memor.cmp(&(MemoriaCfg){.src = name.at, .other = args->table, .bytes = name.length}) == 0);
            continue;
        }
        unsigned long long from = 0ULL;
        for (unsigned long long at = 0ULL; inside && (at <= code.length); at += 1ULL)
        {
            if ((at < code.length) && (code.at[at] != ',') && (code.at[at] != '{') && (code.at[at] != '}'))
            {
                continue;
            }
            if (EMBED_CALL(cli_number_after, FormatWidthCtx, .part = (CliSpan){code.at + from, at - from},
                           .key = args->key, .number = args->number))
            {
                return 1;
            }
            from = at + 1ULL;
        }
    }
    return 0;
}

// whether Prettier's settings set `proseWrap` to `always`
EMBED_INLINE int cli_wraps_prose(const FormatWidthCtx *args)
{
    const char *part = args->text;
    for (const char *at = args->text;; at += 1)
    {
        if ((at[0] != '\n') && (at[0] != ',') && (at[0] != '{') && (at[0] != '}') && (at[0] != '\0'))
        {
            continue;
        }
        char piece[CLI_NAME_MAX_LEN * 4u];
        // the part lies before `at`, its end at or past its start
        const unsigned long long length = (unsigned long long)(at - part);
        if (length < sizeof(piece))
        {
            memor.cpy(&(MemoriaCfg){.dst = piece, .src = part, .bytes = length});
            piece[length] = '\0';
            if ((cellul.find(&(CatenaFinitaCfg){
                     .src = piece, .cap = CLI_POOL_BYTES, .other = "proseWrap", .other_cap = CLI_POOL_BYTES}) !=
                 NULL) &&
                (cellul.find(&(CatenaFinitaCfg){
                     .src = piece, .cap = CLI_POOL_BYTES, .other = "always", .other_cap = CLI_POOL_BYTES}) != NULL))
            {
                return 1;
            }
        }
        if (at[0] == '\0')
        {
            return 0;
        }
        part = at + 1;
    }
}

int cli_format_margin(const CliFormatPathLanguageCfg *args)
{
    unsigned int *width = args->width;
    static const char *const s_black[1] = {"pyproject.toml"};
    static const char *const s_rustfmt[2] = {"rustfmt.toml", ".rustfmt.toml"};
    static const char *const s_clang[2] = {".clang-format", "_clang-format"};
    static const char *const s_prettier[4] = {".prettierrc", ".prettierrc.json", ".prettierrc.yaml", ".prettierrc.yml"};
    char why[CLI_NAME_MAX_LEN * 4u];
    VerbaLinea unknown = {why, sizeof(why), 0ULL};
    const unsigned int tool = EMBED_CALL(cli_format_language_internal.tool, CliFormatLanguageCtx,
                                         .language = args->language, .err_msg = &unknown);
    const CliJson *const json = &g_cli_format.json;
    const char *const program =
        (tool != CLI_JSON_NONE)
            ? EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                         .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                            .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                                 .json_read = json, .object = tool, .name = "format"),
                                            .name = "program"),
                         .length_pointer = NULL)
            : NULL;
    const char *const run_by = (program != NULL) ? program : "";
    const char *const *names = NULL;
    unsigned int count = 0u;
    const char *table = NULL;
    const char *key = NULL;
    unsigned int own = 0u;
    if (cellul.eq(&(CatenaFinitaCfg){.src = run_by, .other = "black", .cap = CLI_POOL_BYTES}))
    {
        names = s_black;
        count = 1u;
        table = "tool.black";
        key = "line-length";
        own = 88u;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = run_by, .other = "rustfmt", .cap = CLI_POOL_BYTES}))
    {
        names = s_rustfmt;
        count = 2u;
        key = "max_width";
        own = 100u;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = run_by, .other = "clang-format", .cap = CLI_POOL_BYTES}))
    {
        names = s_clang;
        count = 2u;
        key = "ColumnLimit";
        own = 80u;
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = run_by, .other = "prettier", .cap = CLI_POOL_BYTES}))
    {
        names = s_prettier;
        count = 4u;
        key = "printWidth";
        own = 80u;
    }
    if (names == NULL)
    {
        g_cli_guard->persistent_buf_release(g_cli_format.json.node,
                                            mmgr_owner_of_buf(cli_pool_series_cellularum(), g_cli_format.json.node));
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&g_cli_format.json),
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
        g_cli_guard->persistent_buf_release(g_cli_format.settings, g_cli_toolchains_owner);
        g_cli_format.settings = NULL;
        g_cli_format.settings_size = 0ULL;
        return 0;
    }
    const int settings = EMBED_CALL(cli_format_path_internal.nearest, CliFormatPathCtx, .path = args->path,
                                    .names = names, .count = count);
    if ((cellul.eq(&(CatenaFinitaCfg){.src = args->language, .other = "markdown", .cap = CLI_POOL_BYTES})) &&
        !(settings && EMBED_CALL(cli_wraps_prose, FormatWidthCtx, .text = g_cli_format.settings)))
    {
        g_cli_guard->persistent_buf_release(g_cli_format.json.node,
                                            mmgr_owner_of_buf(cli_pool_series_cellularum(), g_cli_format.json.node));
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&g_cli_format.json),
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
        g_cli_guard->persistent_buf_release(g_cli_format.settings, g_cli_toolchains_owner);
        g_cli_format.settings = NULL;
        g_cli_format.settings_size = 0ULL;
        return 0;
    }
    unsigned int found = own;
    if (settings && !EMBED_CALL(cli_format_text_internal.number_in, CliFormatTextCtx, .text = g_cli_format.settings,
                                .table = table, .key = key, .number = &found))
    {
        found = own;
    }
    const char *const name =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = tool, .name = "name"),
                   .length_pointer = NULL);
    EMBED_CALL(cli_text_out.copied_or, CliTextOutCfg, .out_capacity = args->by, .text = name, .otherwise = "");
    *width = found;
    g_cli_guard->persistent_buf_release(g_cli_format.json.node,
                                        mmgr_owner_of_buf(cli_pool_series_cellularum(), g_cli_format.json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&g_cli_format.json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    g_cli_guard->persistent_buf_release(g_cli_format.settings, g_cli_toolchains_owner);
    g_cli_format.settings = NULL;
    g_cli_format.settings_size = 0ULL;
    return found > 0u;
}

int cli_format_width(const CliFormatPathLanguageCfg *args)
{
    char by[CLI_NAME_MAX_LEN];
    return EMBED_CALL(cli_format_path_language.margin, CliFormatPathLanguageCfg, .path = args->path,
                      .language = args->language, .width = args->width, .by = by);
}
