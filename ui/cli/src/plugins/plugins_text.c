// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugins_text.c: a plugin laid out as its file holds it, a sample in its language, and the plugin written to a folder
// of its own
#include "plugins.h"

// the widest line a plugin's file is laid out to
#define CLI_WIDE 110ull

// the bytes a string takes written as JSON, its quotes with it
static unsigned long long cli_quoted_length(const char *text, unsigned long long length)
{
    char held[4096];
    CliLine line = {held, sizeof(held), 0ull};
    cli_json_quoted(&line, text, length);
    if (cli_line_fits(&line))
    {
        return line.at;
    }
    // a text too long for the room is longer than any line it would fit
    return CLI_WIDE + 1ull;
}

static void cli_written(const CliJson *json, unsigned int node, CliLine *out, unsigned long long indent)
{
    const unsigned long long begun = out->at;
    cli_json_write(json, node, out, 0u, 0);
    const unsigned long long flat = out->at - begun;
    const CfgJsonKind kind = json->node[node].kind;
    if ((cli_line_fits(out) && ((flat + indent) <= CLI_WIDE)) ||
        ((kind != CFG_JSON_ARRAY) && (kind != CFG_JSON_OBJECT)))
    {
        return;
    }
    out->at = begun;
    const unsigned long long inner = indent + 2ull;
    int strings = kind == CFG_JSON_ARRAY;
    for (unsigned int item = json->node[node].first; strings && (item != CLI_JSON_NONE); item = json->node[item].next)
    {
        strings = json->node[item].kind == CFG_JSON_STRING;
    }
    cli_line_text(out, (kind == CFG_JSON_OBJECT) ? "{\n" : "[\n");
    unsigned long long line = 0ull;
    int first = 1;
    for (unsigned int item = json->node[node].first; item != CLI_JSON_NONE; item = json->node[item].next)
    {
        const int last = json->node[item].next == CLI_JSON_NONE;
        if (strings)
        {
            // the words packed onto lines, a line broken where the next would pass the width
            const unsigned long long word =
                cli_quoted_length(json->text + json->node[item].text, json->node[item].length);
            if (!first && ((inner + line + word + 2ull) > CLI_WIDE))
            {
                cli_line_text(out, ",\n");
                line = 0ull;
            }
            else if (!first)
            {
                cli_line_text(out, ", ");
                line += 2ull;
            }
            if (line == 0ull)
            {
                for (unsigned long long space = 0ull; space < inner; space += 1ull)
                {
                    cli_line_character(out, ' ');
                }
            }
            cli_json_quoted(out, json->text + json->node[item].text, json->node[item].length);
            line += word;
            first = 0;
            if (last)
            {
                cli_line_character(out, '\n');
            }
            continue;
        }
        for (unsigned long long space = 0ull; space < inner; space += 1ull)
        {
            cli_line_character(out, ' ');
        }
        if (kind == CFG_JSON_OBJECT)
        {
            const char *const key = json->text + json->node[item].key;
            cli_json_quoted(out, key, cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}));
            cli_line_text(out, ": ");
        }
        cli_written(json, item, out, inner);
        cli_line_text(out, last ? "\n" : ",\n");
    }
    for (unsigned long long space = 0ull; space < indent; space += 1ull)
    {
        cli_line_character(out, ' ');
    }
    cli_line_character(out, (kind == CFG_JSON_OBJECT) ? '}' : ']');
}

void cli_plugins_text_of(const CliJson *json, unsigned int plugin, CliLine *out)
{
    cli_written(json, plugin, out, 0ull);
    cli_line_character(out, '\n');
}

// the first word of the grammar's list `name`, NULL where it has none
static const char *cli_first(const CliJson *json, unsigned int plugin, const char *name)
{
    return cli_json_string(
        json, cli_json_item(json, cli_json_member(json, cli_json_member(json, plugin, "grammar"), name), 0u), NULL);
}

// A few lines in the new language to open and try: a comment, a keyword, a type, a constant, a string and a number,
// each where the plugin has one
static void cli_sample_of(const CliJson *json, unsigned int plugin, CliLine *out)
{
    const unsigned int comments = cli_json_member(json, plugin, "comments");
    const char *const line = cli_json_string(json, cli_json_member(json, comments, "line"), NULL);
    const char *const name = cli_json_string(json, cli_json_member(json, plugin, "name"), NULL);
    if (line != NULL)
    {
        cli_line_text(out, line);
        cli_line_character(out, ' ');
        cli_line_text(out, (name != NULL) ? name : "this file");
        cli_line_text(out, " opens in this plugin\n");
    }
    const unsigned int block = cli_json_member(json, comments, "block");
    const char *const open = cli_json_string(json, cli_json_item(json, block, 0u), NULL);
    const char *const close = cli_json_string(json, cli_json_item(json, block, 1u), NULL);
    if ((open != NULL) && (close != NULL))
    {
        cli_line_text(out, open);
        cli_line_text(out, " a block comment ");
        cli_line_text(out, close);
        cli_line_character(out, '\n');
    }
    const char *keyword = cli_first(json, plugin, "keywords");
    keyword = (keyword != NULL) ? keyword : cli_first(json, plugin, "words");
    keyword = (keyword != NULL) ? keyword : "word";
    const char *const kind = (cli_first(json, plugin, "types") != NULL) ? cli_first(json, plugin, "types") : "Type";
    const char *const constant =
        (cli_first(json, plugin, "constants") != NULL) ? cli_first(json, plugin, "constants") : "42";
    const char *const quoted =
        cli_json_string(json, cli_json_item(json, cli_json_member(json, plugin, "quotes"), 0u), NULL);
    const char *const quote = (quoted != NULL) ? quoted : "\"";
    cli_line_text(out, keyword);
    cli_line_character(out, ' ');
    cli_line_text(out, kind);
    cli_line_text(out, " name = ");
    cli_line_text(out, constant);
    cli_line_text(out, ";\n");
    cli_line_text(out, keyword);
    cli_line_text(out, " text = ");
    cli_line_text(out, quote);
    cli_line_text(out, "a string");
    cli_line_text(out, quote);
    cli_line_text(out, ", count = 0x2a + 1.5e3;\n");
}

int cli_plugins_create_in(const char *dir, CliJson *json, unsigned int spec, int replace, char *folder, CliLine *said)
{
    static char s_file[CLI_PLUGIN_FILE];
    static CliLock s_writing = CLI_LOCK_INIT;
    const unsigned int plugin = cli_plugins_draft(json, spec, said);
    if (plugin == CLI_JSON_NONE)
    {
        return 0;
    }
    const char *const id = cli_json_string(json, cli_json_member(json, plugin, "id"), NULL);
    char file[CLI_PATH_LONGEST];
    CliLine line = {folder, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, dir);
    cli_line_character(&line, '/');
    cli_line_text(&line, (id != NULL) ? id : "");
    const int folder_fits = cli_line_closed(&line) != 0ull;
    CliLine named = {file, sizeof(file), 0ull};
    cli_line_text(&named, folder);
    cli_line_text(&named, "/plugin.json");
    if (!folder_fits || (cli_line_closed(&named) == 0ull))
    {
        cli_line_text(said, "the plugin's folder is a longer path than orior takes");
        return 0;
    }
    if (query_is_file(file) && !replace)
    {
        cli_line_text(said, "a plugin ");
        cli_line_text(said, id);
        cli_line_text(said, " is there already, in ");
        cli_line_text(said, folder);
        cli_line_text(said, ": give another name, or replace it");
        return 0;
    }
    if (!query_folder_made(folder))
    {
        cli_line_text(said, folder);
        cli_line_text(said, ": it could not be made");
        return 0;
    }
    cli_lock_take(&s_writing);
    CliLine text = {s_file, sizeof(s_file), 0ull};
    cli_plugins_text_of(json, plugin, &text);
    int written = cli_line_fits(&text) && query_file_written(file, s_file, text.at);
    const char *const extension =
        cli_json_string(json, cli_json_item(json, cli_json_member(json, plugin, "extensions"), 0u), NULL);
    char sample[CLI_PATH_LONGEST];
    CliLine sample_name = {sample, sizeof(sample), 0ull};
    cli_line_text(&sample_name, folder);
    cli_line_text(&sample_name, "/sample.");
    cli_line_text(&sample_name, (extension != NULL) ? extension : "txt");
    CliLine sample_text = {s_file, sizeof(s_file), 0ull};
    // 1 where both are written, 0 where plugin.json is not, 2 where the sample is not
    if (written)
    {
        cli_sample_of(json, plugin, &sample_text);
        const int sampled = (cli_line_closed(&sample_name) != 0ull) && cli_line_fits(&sample_text) &&
                            query_file_written(sample, s_file, sample_text.at);
        written = sampled ? 1 : 2;
    }
    cli_lock_give(&s_writing);
    if (written != 1)
    {
        cli_line_text(said, (written == 0) ? file : sample);
        cli_line_text(said, ": it could not be written");
        return 0;
    }
    return 1;
}

int cli_plugins_create(CliJson *json, unsigned int spec, int replace, char *folder, CliLine *said)
{
    char dir[CLI_PATH_LONGEST];
    if (!cli_home_plugins(dir))
    {
        cli_line_text(said, "orior has no folder of its own to keep plugins in");
        return 0;
    }
    return cli_plugins_create_in(dir, json, spec, replace, folder, said);
}
