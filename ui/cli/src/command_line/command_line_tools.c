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

// the most plugins listed, and the rows their room is begun with before it doubles
#define CLI_PLUGINS_LISTED 1024u

#define CLI_PLUGINS_ROWS_FIRST 32u

// the nodes and the bytes of texts a tree made of a command's words holds past those its words make
#define CLI_TOOLS_NODES_MORE 64u
#define CLI_TOOLS_TEXT_MORE 1024ull

// the tree a plugin.json is read into, or a command's words are made into, a cell sized to it given back once read
static CliJson s_json;
static char s_line[1u << 16u];

// A plugin's row: its id, where it comes from, its name and the extensions it opens
typedef struct
{
    char id[CLI_NAME_LONGEST];
    char source[CLI_NAME_LONGEST];
    char name[CLI_NAME_LONGEST];
    char opens[1024];
} CliPluginRow;

// the rows listed, in a cell whose room doubles as they are put
static CliPluginRow *s_rows;
static unsigned int s_row_count;
static unsigned int s_row_room;

// `text` copied into `into`, which holds `room`, cut where it does not fit
static void tools_copied(char *into, unsigned long long room, const char *text, unsigned long long size)
{
    const unsigned long long kept = (size < room) ? size : room - 1ull;
    memor.cpy(&(MemoriaCfg){.dst = into, .src = text, .bytes = kept});
    into[kept] = '\0';
}

// a plugin's row made from its plugin.json
static void tools_plugin_row(void *context, const CliPlugin *plugin)
{
    (void)context;
    if (s_row_count >= CLI_PLUGINS_LISTED)
    {
        return;
    }
    if (s_row_count == s_row_room)
    {
        const unsigned int room = (s_row_room == 0u) ? CLI_PLUGINS_ROWS_FIRST : (s_row_room * 2u);
        CliPluginRow *const rows =
            cli_cell_taken((unsigned long long)room * sizeof(*rows), "the rows of the plugins listed");
        if (rows == NULL)
        {
            return;
        }
        memor.cpy(&(MemoriaCfg){.dst = rows, .src = s_rows, .bytes = (size_t)s_row_count * sizeof(*rows)});
        cli_cell_given(s_rows);
        s_rows = rows;
        s_row_room = room;
    }
    CliPluginRow *const row = &s_rows[s_row_count];
    s_row_count += 1u;
    tools_copied(row->id, sizeof(row->id), plugin->id,
                 cellul.len(&(CatenaFinitaCfg){.src = plugin->id, .cap = CLI_CELLS_POOL}));
    tools_copied(row->source, sizeof(row->source), plugin->source,
                 cellul.len(&(CatenaFinitaCfg){.src = plugin->source, .cap = CLI_CELLS_POOL}));
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    cli_json_given(&s_json);
    const unsigned int read = cli_json_taken(&s_json, plugin->text, plugin->size, 0u, 0ull, &said);
    if (read == CLI_JSON_NONE)
    {
        CliLine name = {row->name, sizeof(row->name), 0ull};
        cli_line_text(&name, "does not read: ");
        cli_line_counted(&name, why, cli_line_fits(&said) ? said.at : 0ull);
        (void)cli_line_closed(&name);
        row->opens[0] = '\0';
        cli_json_given(&s_json);
        return;
    }
    const char *const name = cli_json_string(&s_json, cli_json_member(&s_json, read, "name"), NULL);
    tools_copied(row->name, sizeof(row->name), (name != NULL) ? name : plugin->id,
                 cellul.len(&(CatenaFinitaCfg){.src = (name != NULL) ? name : plugin->id, .cap = CLI_CELLS_POOL}));
    CliLine opens = {row->opens, sizeof(row->opens), 0ull};
    const unsigned int extensions = cli_json_member(&s_json, read, "extensions");
    const unsigned int first =
        cli_json_is(&s_json, extensions, CFG_JSON_ARRAY) ? s_json.node[extensions].first : CLI_JSON_NONE;
    for (unsigned int extension = first; extension != CLI_JSON_NONE; extension = s_json.node[extension].next)
    {
        const char *const text = cli_json_string(&s_json, extension, NULL);
        if (text != NULL)
        {
            cli_line_text(&opens, (opens.at > 0ull) ? " ." : ".");
            cli_line_text(&opens, text);
        }
    }
    if (cli_line_closed(&opens) == 0ull)
    {
        row->opens[0] = '\0';
    }
    cli_json_given(&s_json);
}

// `text` written and padded with spaces to `width` letters
static void tools_padded(CliLine *line, const char *text, unsigned long long width)
{
    cli_line_text(line, text);
    for (unsigned long long letters =
             cli_text_letters(text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
         letters < width; letters += 1ull)
    {
        cli_line_character(line, ' ');
    }
}

int cli_plugins_list(void)
{
    s_row_count = 0u;
    cli_plugins_each(tools_plugin_row, NULL);
    unsigned long long wide[3] = {0ull, 0ull, 0ull};
    for (unsigned int at = 0u; at < s_row_count; at += 1u)
    {
        const char *const columns[3] = {s_rows[at].id, s_rows[at].source, s_rows[at].name};
        for (unsigned int column = 0u; column < 3u; column += 1u)
        {
            const unsigned long long size =
                cellul.len(&(CatenaFinitaCfg){.src = columns[column], .cap = CLI_CELLS_POOL});
            wide[column] = (size > wide[column]) ? size : wide[column];
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
        CliLine line = {s_line, sizeof(s_line), 0ull};
        tools_padded(&line, s_rows[at].id, wide[0]);
        cli_line_text(&line, "  ");
        tools_padded(&line, replaced ? "bundled, replaced" : s_rows[at].source, wide[1]);
        cli_line_text(&line, "  ");
        tools_padded(&line, s_rows[at].name, wide[2]);
        cli_line_text(&line, "  ");
        cli_line_text(&line, s_rows[at].opens);
        cli_out_line(s_line, cli_line_fits(&line) ? line.at : 0ull);
    }
    char folder[CLI_PATH_LONGEST];
    if (cli_home_plugins(folder))
    {
        CliLine line = {s_line, sizeof(s_line), 0ull};
        cli_line_text(&line, "\nthe reader's plugins: ");
        cli_line_text(&line, folder);
        cli_out_line(s_line, cli_line_fits(&line) ? line.at : 0ull);
    }
    cli_cell_given(s_rows);
    s_rows = NULL;
    s_row_count = 0u;
    s_row_room = 0u;
    return 0;
}

// a tree held empty in a cell sized to what `count` words make, every comma of them a node and the texts twice their
// bytes at the most: 1, or 0 where the cell is refused
static int tools_tree_held(const char *const *words, unsigned int count)
{
    unsigned long long bytes = 0ull;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        bytes += cellul.len(&(CatenaFinitaCfg){.src = words[at], .cap = CLI_CELLS_POOL}) + 1ull;
    }
    cli_json_given(&s_json);
    return cli_json_held(&s_json, (unsigned int)bytes + (4u * count) + CLI_TOOLS_NODES_MORE,
                         (2ull * bytes) + CLI_TOOLS_TEXT_MORE, "a tree made of a command's words");
}

// `text` split at commas into an array of the parts, each trimmed, the empty ones left out
static unsigned int tools_list(const char *text)
{
    const unsigned int list = cli_json_made(&s_json, CFG_JSON_ARRAY);
    const char *part = text;
    for (;;)
    {
        const char *const comma = cellul.chr(&(CatenaFinitaCfg){.src = part, .cap = CLI_CELLS_POOL, .byte = ','});
        const unsigned long long size = (comma != NULL)
                                            ? (unsigned long long)(comma - part)
                                            : cellul.len(&(CatenaFinitaCfg){.src = part, .cap = CLI_CELLS_POOL});
        unsigned long long from = 0ull;
        unsigned long long to = 0ull;
        cli_text_trimmed(part, size, &from, &to);
        if (to > from)
        {
            cli_json_append(&s_json, list, cli_json_string_made(&s_json, part + from, to - from));
        }
        if (comma == NULL)
        {
            return list;
        }
        part = comma + 1;
    }
}

// a string member set
static void tools_set_text(unsigned int object, const char *name, const char *text)
{
    cli_json_set(
        &s_json, object, name,
        cli_json_string_made(&s_json, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL})));
}

// a line of the errors made of two texts
static void tools_err_two(const char *one, const char *other)
{
    CliLine line = {s_line, sizeof(s_line), 0ull};
    cli_line_text(&line, one);
    cli_line_text(&line, other);
    cli_err_line(s_line, cli_line_fits(&line) ? line.at : 0ull);
}

// the language plugin `new-plugin` describes written, the tree held for its words: 0, or CLI_WRONG with why said
static int tools_new_plugin(const char *const *words, unsigned int count)
{
    static const char *const s_lists[6][2] = {{"--ext", "extensions"}, {"--keywords", "keywords"},
                                              {"--types", "types"},    {"--constants", "constants"},
                                              {"--quotes", "quotes"},  {NULL, NULL}};
    static const char *const s_texts[3][2] = {{"--id", "id"}, {"--from", "from"}, {"--line-comment", "lineComment"}};
    const unsigned int spec = cli_json_made(&s_json, CFG_JSON_OBJECT);
    static char s_name[1u << 12u];
    CliLine name = {s_name, sizeof(s_name), 0ull};
    int replace = 0;
    unsigned int at = 0u;
    while (at < count)
    {
        const char *const word = words[at];
        const char *const value = (at + 1u < count) ? words[at + 1u] : NULL;
        unsigned int taken = 0u;
        int known = 0;
        if (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "--replace", .cap = CLI_CELLS_POOL}))
        {
            replace = 1;
            known = 1;
        }
        for (unsigned int list = 0u; (s_lists[list][0] != NULL) && !known && (value != NULL); list += 1u)
        {
            if (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = s_lists[list][0], .cap = CLI_CELLS_POOL}))
            {
                cli_json_set(&s_json, spec, s_lists[list][1], tools_list(value));
                taken = 1u;
                known = 1;
            }
        }
        for (unsigned int text = 0u; (text < 3u) && !known && (value != NULL); text += 1u)
        {
            if (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = s_texts[text][0], .cap = CLI_CELLS_POOL}))
            {
                tools_set_text(spec, s_texts[text][1], value);
                taken = 1u;
                known = 1;
            }
        }
        if (!known && (value != NULL) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = word, .other = "--block-comment", .cap = CLI_CELLS_POOL})))
        {
            if (at + 2u >= count)
            {
                cli_err("--block-comment takes two words, the opening and the closing");
                return CLI_WRONG;
            }
            const unsigned int block = cli_json_made(&s_json, CFG_JSON_ARRAY);
            cli_json_append(&s_json, block,
                            cli_json_string_made(&s_json, value,
                                                 cellul.len(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL})));
            cli_json_append(
                &s_json, block,
                cli_json_string_made(&s_json, words[at + 2u],
                                     cellul.len(&(CatenaFinitaCfg){.src = words[at + 2u], .cap = CLI_CELLS_POOL})));
            cli_json_set(&s_json, spec, "blockComment", block);
            taken = 2u;
            known = 1;
        }
        if (!known && (cellul.starts(&(CatenaFinitaCfg){.src = word, .other = "--", .cap = CLI_CELLS_POOL})))
        {
            if (value == NULL)
            {
                tools_err_two(word, " needs a value");
            }
            else
            {
                CliLine line = {s_line, sizeof(s_line), 0ull};
                cli_line_text(&line, "new-plugin takes no ");
                cli_line_text(&line, word);
                cli_line_text(&line, ": orior_cli help names what it takes");
                cli_err_line(s_line, cli_line_fits(&line) ? line.at : 0ull);
            }
            return CLI_WRONG;
        }
        if (!known)
        {
            cli_line_text(&name, (name.at > 0ull) ? " " : "");
            cli_line_text(&name, word);
        }
        at += 1u + taken;
    }
    (void)cli_line_closed(&name);
    tools_set_text(spec, "name", s_name);
    char folder[CLI_PATH_LONGEST];
    char why[4096];
    CliLine said = {why, sizeof(why), 0ull};
    if (!cli_plugins_create(&s_json, spec, replace, folder, &said))
    {
        cli_err_line(why, cli_line_fits(&said) ? said.at : 0ull);
        return CLI_WRONG;
    }
    cli_out(folder);
    return 0;
}

int cli_new_plugin(const char *const *words, unsigned int count)
{
    if (!tools_tree_held(words, count))
    {
        cli_err("the tree of the plugin's words could not be given a cell");
        return CLI_WRONG;
    }
    const int written = tools_new_plugin(words, count);
    cli_json_given(&s_json);
    return written;
}

// The toolchain `add` describes: its name, then each flag and its value, made in the tree: its node, or CLI_JSON_NONE
// with why said
static unsigned int tools_described(const char *const *words, unsigned int count, CliLine *said)
{
    if ((count == 0u) || (cellul.starts(&(CatenaFinitaCfg){.src = words[0], .other = "--", .cap = CLI_CELLS_POOL})))
    {
        cli_line_text(said, "add takes the toolchain's name first");
        return CLI_JSON_NONE;
    }
    if (!tools_tree_held(words, count))
    {
        cli_line_text(said, "the tree of the toolchain's words could not be given a cell");
        return CLI_JSON_NONE;
    }
    const unsigned int tool = cli_json_made(&s_json, CFG_JSON_OBJECT);
    tools_set_text(tool, "id", "");
    tools_set_text(tool, "name", words[0]);
    tools_set_text(tool, "group", "");
    tools_set_text(tool, "for", "");
    cli_json_set(&s_json, tool, "programs", cli_json_made(&s_json, CFG_JSON_ARRAY));
    for (unsigned int at = 1u; at < count; at += 2u)
    {
        if (at + 1u >= count)
        {
            cli_line_text(said, words[at]);
            cli_line_text(said, " takes a value");
            return CLI_JSON_NONE;
        }
        const char *const flag = words[at];
        const char *const value = words[at + 1u];
        if ((cellul.eq(&(CatenaFinitaCfg){.src = flag, .other = "--group", .cap = CLI_CELLS_POOL})) ||
            (cellul.eq(&(CatenaFinitaCfg){.src = flag, .other = "--for", .cap = CLI_CELLS_POOL})) ||
            (cellul.eq(&(CatenaFinitaCfg){.src = flag, .other = "--id", .cap = CLI_CELLS_POOL})))
        {
            tools_set_text(tool, flag + 2, value);
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = flag, .other = "--program", .cap = CLI_CELLS_POOL}))
        {
            cli_json_set(&s_json, tool, "programs", tools_list(value));
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = flag, .other = "--version", .cap = CLI_CELLS_POOL}))
        {
            cli_json_set(&s_json, tool, "version", tools_list(value));
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = flag, .other = "--install", .cap = CLI_CELLS_POOL}))
        {
            unsigned int install = cli_json_member(&s_json, tool, "install");
            if (install == CLI_JSON_NONE)
            {
                install = cli_json_made(&s_json, CFG_JSON_OBJECT);
                cli_json_set(&s_json, tool, "install", install);
            }
            tools_set_text(install, "any", value);
        }
        else
        {
            cli_line_text(said, "add knows no ");
            cli_line_text(said, flag);
            return CLI_JSON_NONE;
        }
    }
    return tool;
}

// the toolchain `add` describes added: 1 with what was added said into `out`, or 0 with why said
int cli_toolchain_added(const char *const *words, unsigned int count, CliLine *out, CliLine *said)
{
    const unsigned int tool = tools_described(words, count, said);
    char id[CLI_NAME_LONGEST];
    const int added = (tool != CLI_JSON_NONE) && cli_toolchains_add(&s_json, tool, id, said);
    cli_json_given(&s_json);
    if (!added)
    {
        return 0;
    }
    cli_line_text(out, "added ");
    cli_line_text(out, id);
    cli_line_text(out, ": orior_cli file toolchains lists it");
    return 1;
}
