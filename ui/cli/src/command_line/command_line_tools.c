// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// command_line_tools.c: `orior_cli file plugins`, every plugin with its id, where it comes from, its name and the
// extensions it opens, one of the reader's that stands in for one that comes with orior_cli marked so; `orior_cli file
// new-plugin`, a language plugin written to the reader's plugins folder; and the toolchain `orior_cli file toolchains
// add` describes, its name, then each flag and its value, added
#include "../plugins/plugins.h"
#include "../text/text.h"
#include "../toolchains/toolchains.h"
#include "command_line_internal.h"

// the most plugins listed, and the rows their capacity is taken with
#define CLI_PLUGINS_LISTING 1024u

// the nodes and the bytes of texts a tree made of a command's words contains past those its words make
#define CLI_TOOLS_NODES_MORE 64u
#define CLI_TOOLS_TEXT_MORE 1024ull

// the tree a plugin.json is read into, or a command's words are made into, a cell sized to it given back once read
static CliJson s_json;
static char s_line[1u << 16u];

// A plugin's row: its id, where it comes from, its name and the extensions it opens
typedef struct
{
    char id[CLI_NAME_MAX_LEN];
    char source[CLI_NAME_MAX_LEN];
    char name[CLI_NAME_MAX_LEN];
    char opens[1024];
} CliPluginRow;

// the rows listed, in a cell of `s_row_capacity`
static CliPluginRow *s_rows;
static unsigned int s_row_count;
static unsigned int s_row_capacity;

typedef struct
{
    char *into;
    unsigned long long capacity;
    const char *text;
    unsigned long long size;
    VerbaLinea *line;
    unsigned long long width;
    const char *const *words;
    unsigned int count;
    unsigned int object;
    const char *name;
    const char *one;
    const char *other;
    VerbaLinea *err_msg;
} CommandLineToolsCtx;
// `text` copied into `into`, sized for `capacity`, cut where it does not fit
EMBED_INLINE void tools_copy(const CommandLineToolsCtx *args)
{
    const unsigned long long copied_len = (args->size < args->capacity) ? args->size : args->capacity - 1ull;
    memor.cpy(&(MemoriaCfg){.dst = args->into, .src = args->text, .bytes = copied_len});
    args->into[copied_len] = '\0';
}

// a plugin's row made from its plugin.json
static void tools_plugin_row(const CliEachPluginCfg *args)
{
    if (s_row_count >= s_row_capacity)
    {
        return;
    }
    CliPluginRow *const row = &s_rows[s_row_count];
    s_row_count += 1u;
    EMBED_CALL(tools_copy, CommandLineToolsCtx, .into = row->id, .capacity = sizeof(row->id), .text = args->plugin->id,
               .size = cellul.len(&(CatenaFinitaCfg){.src = args->plugin->id, .cap = CLI_CELLS_POOL}));
    EMBED_CALL(tools_copy, CommandLineToolsCtx, .into = row->source, .capacity = sizeof(row->source),
               .text = args->plugin->source,
               .size = cellul.len(&(CatenaFinitaCfg){.src = args->plugin->source, .cap = CLI_CELLS_POOL}));
    char why[512];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &s_json);
    const unsigned int read =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &s_json,
                   .text = args->plugin->text, .size = args->plugin->size, .nodes_more = 0u, .text_more = 0ull,
                   .err_msg = &err_msg, .owner = &g_cli_command_line_cells);
    if (read == CLI_JSON_NONE)
    {
        VerbaLinea name = {row->name, sizeof(row->name), 0ull};
        mmgr_line_put(&name, "does not read: ");
        mmgr_line_put_n(&name, why, mmgr_line_ok(&err_msg) ? err_msg.at : 0ull);
        (void)mmgr_line_finish(&name);
        row->opens[0] = '\0';
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &s_json);
        return;
    }
    const char *const name =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json,
                                      .object = read, .name = "name"),
                   .length_pointer = NULL);
    EMBED_CALL(
        tools_copy, CommandLineToolsCtx, .into = row->name, .capacity = sizeof(row->name),
        .text = (name != NULL) ? name : args->plugin->id,
        .size = cellul.len(&(CatenaFinitaCfg){.src = (name != NULL) ? name : args->plugin->id, .cap = CLI_CELLS_POOL}));
    VerbaLinea opens = {row->opens, sizeof(row->opens), 0ull};
    const unsigned int extensions = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                               .json_read = &s_json, .object = read, .name = "extensions");
    const unsigned int first = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &s_json,
                                          .node = extensions, .kind = CFG_JSON_ARRAY)
                                   ? s_json.node[extensions].first
                                   : CLI_JSON_NONE;
    for (unsigned int extension = first; extension != CLI_JSON_NONE; extension = s_json.node[extension].next)
    {
        const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                            .json_read = &s_json, .node = extension, .length_pointer = NULL);
        if (text != NULL)
        {
            mmgr_line_put(&opens, (opens.at > 0ull) ? " ." : ".");
            mmgr_line_put(&opens, text);
        }
    }
    if (mmgr_line_finish(&opens) == 0ull)
    {
        row->opens[0] = '\0';
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &s_json);
}

// `text` written and padded with spaces to `width` letters
EMBED_INLINE void tools_pad(const CommandLineToolsCtx *args)
{
    const char *text = args->text;
    mmgr_line_put(args->line, text);
    for (unsigned long long letters =
             EMBED_CALL(cli_text_size_byte.letters, CliTextSizeByteCfg, .text = text,
                        .size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
         letters < args->width; letters += 1ull)
    {
        mmgr_line_ch(args->line, ' ');
    }
}

int cli_plugins_list(void)
{
    s_rows = g_cli_cells->temporary_buf_alloc((unsigned long long)CLI_PLUGINS_LISTING * sizeof(*s_rows),
                                              &g_cli_command_line_cells);
    s_row_capacity = (s_rows != NULL) ? CLI_PLUGINS_LISTING : 0u;
    s_row_count = 0u;
    EMBED_CALL(cli_plugins_each_table.each, CliPluginsEachCfg, .each = tools_plugin_row, .context = NULL);
    unsigned long long pixel_width[3] = {0ull, 0ull, 0ull};
    for (unsigned int at = 0u; at < s_row_count; at += 1u)
    {
        const char *const columns[3] = {s_rows[at].id, s_rows[at].source, s_rows[at].name};
        for (unsigned int column = 0u; column < 3u; column += 1u)
        {
            const unsigned long long size =
                cellul.len(&(CatenaFinitaCfg){.src = columns[column], .cap = CLI_CELLS_POOL});
            pixel_width[column] = (size > pixel_width[column]) ? size : pixel_width[column];
        }
    }
    for (unsigned int at = 0u; at < s_row_count; at += 1u)
    {
        int replaced = 0;
        for (unsigned int user = 0u;
             (user < s_row_count) &&
             (cellul.eq(&(CatenaFinitaCfg){.src = s_rows[at].source, .other = "bundled", .cap = CLI_CELLS_POOL}));
             user += 1u)
        {
            replaced |=
                (cellul.eq(&(CatenaFinitaCfg){.src = s_rows[user].source, .other = "user", .cap = CLI_CELLS_POOL})) &&
                (cellul.eq(&(CatenaFinitaCfg){.src = s_rows[user].id, .other = s_rows[at].id, .cap = CLI_CELLS_POOL}));
        }
        VerbaLinea line = {s_line, sizeof(s_line), 0ull};
        EMBED_CALL(tools_pad, CommandLineToolsCtx, .line = &line, .text = s_rows[at].id, .width = pixel_width[0]);
        mmgr_line_put(&line, "  ");
        EMBED_CALL(tools_pad, CommandLineToolsCtx, .line = &line,
                   .text = replaced ? "bundled, replaced" : s_rows[at].source, .width = pixel_width[1]);
        mmgr_line_put(&line, "  ");
        EMBED_CALL(tools_pad, CommandLineToolsCtx, .line = &line, .text = s_rows[at].name, .width = pixel_width[2]);
        mmgr_line_put(&line, "  ");
        mmgr_line_put(&line, s_rows[at].opens);
        EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx, .text = s_line,
                   .size = mmgr_line_ok(&line) ? line.at : 0ull);
    }
    char folder[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_folder_internal.home_plugins, CliFolderCtx, .folder_out = folder))
    {
        VerbaLinea line = {s_line, sizeof(s_line), 0ull};
        mmgr_line_put(&line, "\nthe reader's plugins: ");
        mmgr_line_put(&line, folder);
        EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx, .text = s_line,
                   .size = mmgr_line_ok(&line) ? line.at : 0ull);
    }
    g_cli_cells->persistent_buf_release(s_rows, &g_cli_command_line_cells);
    s_rows = NULL;
    s_row_count = 0u;
    s_row_capacity = 0u;
    return 0;
}

// a tree allocated empty in a cell sized to what `count` words make, every comma of them a node and the texts twice
// their bytes at the most: 1, or 0 where the cell is refused
EMBED_INLINE int tools_tree_reserve(const CommandLineToolsCtx *args)
{
    unsigned long long bytes = 0ull;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        bytes += cellul.len(&(CatenaFinitaCfg){.src = args->words[at], .cap = CLI_CELLS_POOL}) + 1ull;
    }
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &s_json);
    return EMBED_CALL(cli_json_tree_nodes_max.json_reserve, CliJsonTreeNodesMaxCfg, .json = &s_json,
                      .nodes_max = (unsigned int)bytes + (4u * args->count) + CLI_TOOLS_NODES_MORE,
                      .text_max = (2ull * bytes) + CLI_TOOLS_TEXT_MORE, .what = "a tree made of a command's words",
                      .owner = &g_cli_command_line_cells);
}

// `text` split at commas into its parts, each trimmed, the empty ones left out
EMBED_INLINE unsigned int tools_list(const CommandLineToolsCtx *args)
{
    const char *text = args->text;
    const unsigned int list =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = &s_json, .kind = CFG_JSON_ARRAY);
    const char *part = text;
    for (;;)
    {
        const char *const comma = cellul.chr(&(CatenaFinitaCfg){.src = part, .cap = CLI_CELLS_POOL, .byte = ','});
        const unsigned long long size = (comma != NULL)
                                            ? (unsigned long long)(comma - part)
                                            : cellul.len(&(CatenaFinitaCfg){.src = part, .cap = CLI_CELLS_POOL});
        unsigned long long from = 0ull;
        unsigned long long to = 0ull;
        EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = part, .size = size,
                   .from_u64s = &from, .to_u64s = &to);
        if (to > from)
        {
            EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = &s_json, .array = list,
                       .value =
                           EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg,
                                      .json = &s_json, .text = part + from, .length = to - from));
        }
        if (comma == NULL)
        {
            return list;
        }
        part = comma + 1;
    }
}

// a string member set
EMBED_INLINE void tools_set_text(const CommandLineToolsCtx *args)
{
    unsigned int object = args->object;
    const char *name = args->name;
    const char *text = args->text;
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = &s_json, .object = object,
               .name = name,
               .value = EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg,
                                   .json = &s_json, .text = text,
                                   .length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL})));
}

// a line of the errors made of two texts
EMBED_INLINE void tools_err_two(const CommandLineToolsCtx *args)
{
    VerbaLinea line = {s_line, sizeof(s_line), 0ull};
    mmgr_line_put(&line, args->one);
    mmgr_line_put(&line, args->other);
    EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = s_line,
               .size = mmgr_line_ok(&line) ? line.at : 0ull);
}

// the language plugin `new-plugin` describes written, the tree allocated for its words: 0, or CLI_WRONG with why given
EMBED_INLINE int tools_new_plugin(const CommandLineToolsCtx *args)
{
    static const char *const s_lists[6][2] = {{"--ext", "extensions"}, {"--keywords", "keywords"},
                                              {"--types", "types"},    {"--constants", "constants"},
                                              {"--quotes", "quotes"},  {NULL, NULL}};
    static const char *const s_texts[3][2] = {{"--id", "id"}, {"--from", "from"}, {"--line-comment", "lineComment"}};
    const unsigned int spec =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = &s_json, .kind = CFG_JSON_OBJECT);
    static char s_name[1u << 12u];
    VerbaLinea name = {s_name, sizeof(s_name), 0ull};
    int replace = 0;
    unsigned int at = 0u;
    while (at < args->count)
    {
        const char *const word = args->words[at];
        const char *const value = (at + 1u < args->count) ? args->words[at + 1u] : NULL;
        unsigned int reserved = 0u;
        int known = 0;
        if (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "--replace", .cap = CLI_CELLS_POOL}))
        {
            replace = 1;
            known = 1;
        }
        if (value != NULL)
        {
            for (unsigned int list = 0u; (s_lists[list][0] != NULL) && !known; list += 1u)
            {
                if (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = s_lists[list][0], .cap = CLI_CELLS_POOL}))
                {
                    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = &s_json,
                               .object = spec, .name = s_lists[list][1],
                               .value = EMBED_CALL(tools_list, CommandLineToolsCtx, .text = value));
                    reserved = 1u;
                    known = 1;
                }
            }
            for (unsigned int text = 0u; (text < 3u) && !known; text += 1u)
            {
                if (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = s_texts[text][0], .cap = CLI_CELLS_POOL}))
                {
                    EMBED_CALL(tools_set_text, CommandLineToolsCtx, .object = spec, .name = s_texts[text][1],
                               .text = value);
                    reserved = 1u;
                    known = 1;
                }
            }
            if (!known &&
                (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "--block-comment", .cap = CLI_CELLS_POOL})))
            {
                if (at + 2u >= args->count)
                {
                    EMBED_CALL(cli_command_line_internal.err, CliCommandLineCtx,
                               .text = "--block-comment takes two words, the opening and the closing");
                    return CLI_WRONG;
                }
                const unsigned int block = EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = &s_json,
                                                      .kind = CFG_JSON_ARRAY);
                EMBED_CALL(
                    cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = &s_json, .array = block,
                    .value = EMBED_CALL(cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg,
                                        .json = &s_json, .text = value,
                                        .length = cellul.len(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL})));
                EMBED_CALL(
                    cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = &s_json, .array = block,
                    .value = EMBED_CALL(
                        cli_json_tree_text_length_kind.json_string_init, CliJsonTreeTextLengthKindCfg, .json = &s_json,
                        .text = args->words[at + 2u],
                        .length = cellul.len(&(CatenaFinitaCfg){.src = args->words[at + 2u], .cap = CLI_CELLS_POOL})));
                EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = &s_json,
                           .object = spec, .name = "blockComment", .value = block);
                reserved = 2u;
                known = 1;
            }
        }
        if (!known && (cellul.starts(&(CatenaFinitaCfg){.src = word, .other = "--", .cap = CLI_CELLS_POOL})))
        {
            if (value == NULL)
            {
                EMBED_CALL(tools_err_two, CommandLineToolsCtx, .one = word, .other = " needs a value");
            }
            else
            {
                VerbaLinea line = {s_line, sizeof(s_line), 0ull};
                mmgr_line_put(&line, "new-plugin takes no ");
                mmgr_line_put(&line, word);
                mmgr_line_put(&line, ": orior_cli help names what it takes");
                EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = s_line,
                           .size = mmgr_line_ok(&line) ? line.at : 0ull);
            }
            return CLI_WRONG;
        }
        if (!known)
        {
            mmgr_line_put(&name, (name.at > 0ull) ? " " : "");
            mmgr_line_put(&name, word);
        }
        at += 1u + reserved;
    }
    (void)mmgr_line_finish(&name);
    EMBED_CALL(tools_set_text, CommandLineToolsCtx, .object = spec, .name = "name", .text = s_name);
    char folder[CLI_PATH_MAX_LEN];
    char why[4096];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (!EMBED_CALL(cli_plugins_err_msg.create, CliPluginsErrMsgCfg, .json = &s_json, .spec = spec, .replace = replace,
                    .folder = folder, .err_msg = &err_msg))
    {
        EMBED_CALL(cli_command_line_text_size_internal.err_line, CliCommandLineTextSizeCtx, .text = why,
                   .size = mmgr_line_ok(&err_msg) ? err_msg.at : 0ull);
        return CLI_WRONG;
    }
    EMBED_CALL(cli_command_line_internal.out, CliCommandLineCtx, .text = folder);
    return 0;
}

int cli_new_plugin(const CliCommandLineWordsCountCtx *args)
{
    if (!EMBED_CALL(tools_tree_reserve, CommandLineToolsCtx, .words = args->words, .count = args->count))
    {
        EMBED_CALL(cli_command_line_internal.err, CliCommandLineCtx,
                   .text = "the tree of the plugin's words could not be given a cell");
        return CLI_WRONG;
    }
    const int written = EMBED_CALL(tools_new_plugin, CommandLineToolsCtx, .words = args->words, .count = args->count);
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &s_json);
    return written;
}

// The toolchain `add` describes: its name, then each flag and its value, made in the tree: its node, or CLI_JSON_NONE
// with why given
EMBED_INLINE unsigned int tools_describe(const CommandLineToolsCtx *args)
{
    if ((args->count == 0u) ||
        (cellul.starts(&(CatenaFinitaCfg){.src = args->words[0], .other = "--", .cap = CLI_CELLS_POOL})))
    {
        mmgr_line_put(args->err_msg, "add takes the toolchain's name first");
        return CLI_JSON_NONE;
    }
    if (!tools_tree_reserve(args))
    {
        mmgr_line_put(args->err_msg, "the tree of the toolchain's words could not be given a cell");
        return CLI_JSON_NONE;
    }
    const unsigned int tool =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = &s_json, .kind = CFG_JSON_OBJECT);
    EMBED_CALL(tools_set_text, CommandLineToolsCtx, .object = tool, .name = "id", .text = "");
    EMBED_CALL(tools_set_text, CommandLineToolsCtx, .object = tool, .name = "name", .text = args->words[0]);
    EMBED_CALL(tools_set_text, CommandLineToolsCtx, .object = tool, .name = "group", .text = "");
    EMBED_CALL(tools_set_text, CommandLineToolsCtx, .object = tool, .name = "for", .text = "");
    EMBED_CALL(
        cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = &s_json, .object = tool,
        .name = "programs",
        .value = EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = &s_json, .kind = CFG_JSON_ARRAY));
    for (unsigned int at = 1u; at < args->count; at += 2u)
    {
        if (at + 1u >= args->count)
        {
            mmgr_line_put(args->err_msg, args->words[at]);
            mmgr_line_put(args->err_msg, " takes a value");
            return CLI_JSON_NONE;
        }
        const char *const flag = args->words[at];
        const char *const value = args->words[at + 1u];
        if ((cellul.eq(&(CatenaFinitaCfg){.src = flag, .other = "--group", .cap = CLI_CELLS_POOL})) ||
            (cellul.eq(&(CatenaFinitaCfg){.src = flag, .other = "--for", .cap = CLI_CELLS_POOL})) ||
            (cellul.eq(&(CatenaFinitaCfg){.src = flag, .other = "--id", .cap = CLI_CELLS_POOL})))
        {
            EMBED_CALL(tools_set_text, CommandLineToolsCtx, .object = tool, .name = flag + 2, .text = value);
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = flag, .other = "--program", .cap = CLI_CELLS_POOL}))
        {
            EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = &s_json, .object = tool,
                       .name = "programs", .value = EMBED_CALL(tools_list, CommandLineToolsCtx, .text = value));
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = flag, .other = "--version", .cap = CLI_CELLS_POOL}))
        {
            EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = &s_json, .object = tool,
                       .name = "version", .value = EMBED_CALL(tools_list, CommandLineToolsCtx, .text = value));
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = flag, .other = "--install", .cap = CLI_CELLS_POOL}))
        {
            unsigned int install = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = &s_json, .object = tool, .name = "install");
            if (install == CLI_JSON_NONE)
            {
                install = EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = &s_json,
                                     .kind = CFG_JSON_OBJECT);
                EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = &s_json,
                           .object = tool, .name = "install", .value = install);
            }
            EMBED_CALL(tools_set_text, CommandLineToolsCtx, .object = install, .name = "any", .text = value);
        }
        else
        {
            mmgr_line_put(args->err_msg, "add knows no ");
            mmgr_line_put(args->err_msg, flag);
            return CLI_JSON_NONE;
        }
    }
    return tool;
}

// the toolchain `add` describes added: 1 with what was added reported into `out`, or 0 with why given
int cli_toolchain_add(const CliCommandLineOutErrMsgCtx *args)
{
    VerbaLinea *err_msg = args->err_msg;
    const unsigned int tool =
        EMBED_CALL(tools_describe, CommandLineToolsCtx, .words = args->words, .count = args->count, .err_msg = err_msg);
    char id[CLI_NAME_MAX_LEN];
    const int added =
        (tool != CLI_JSON_NONE) && EMBED_CALL(cli_toolchains_err_msg.add, CliToolchainsErrMsgCfg, .supplied = &s_json,
                                              .tool = tool, .id_out = id, .err_msg = err_msg);
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &s_json);
    if (!added)
    {
        return 0;
    }
    mmgr_line_put(args->out, "added ");
    mmgr_line_put(args->out, id);
    mmgr_line_put(args->out, ": orior_cli file toolchains lists it");
    return 1;
}
