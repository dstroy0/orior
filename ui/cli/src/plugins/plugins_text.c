// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugins_text.c: a plugin laid out as its file lays out it, a sample in its language, and the plugin written to a
// folder of its own
#include "plugins.h"

// the widest line a plugin's file is laid out to
#define CLI_WIDE 110ull

// the bytes a string takes written as JSON, its quotes with it
static unsigned long long cli_quoted_length(const char *text, unsigned long long length)
{
    char buf[4096];
    VerbaLinea line = {buf, sizeof(buf), 0ull};
    cli_json_quoted(&line, text, length);
    if (mmgr_line_ok(&line))
    {
        return line.at;
    }
    // a text too long for the room is longer than any line it would fit
    return CLI_WIDE + 1ull;
}

static void cli_written(const CliJson *json, unsigned int node, VerbaLinea *out, unsigned long long indent)
{
    const unsigned long long begun = out->at;
    cli_json_write(json, node, out, 0u, 0);
    const unsigned long long flat = out->at - begun;
    const CfgJsonKind kind = json->node[node].kind;
    if ((mmgr_line_ok(out) && ((flat + indent) <= CLI_WIDE)) || ((kind != CFG_JSON_ARRAY) && (kind != CFG_JSON_OBJECT)))
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
    mmgr_line_put(out, (kind == CFG_JSON_OBJECT) ? "{\n" : "[\n");
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
                mmgr_line_put(out, ",\n");
                line = 0ull;
            }
            else if (!first)
            {
                mmgr_line_put(out, ", ");
                line += 2ull;
            }
            if (line == 0ull)
            {
                for (unsigned long long space = 0ull; space < inner; space += 1ull)
                {
                    mmgr_line_ch(out, ' ');
                }
            }
            cli_json_quoted(out, json->text + json->node[item].text, json->node[item].length);
            line += word;
            first = 0;
            if (last)
            {
                mmgr_line_ch(out, '\n');
            }
            continue;
        }
        for (unsigned long long space = 0ull; space < inner; space += 1ull)
        {
            mmgr_line_ch(out, ' ');
        }
        if (kind == CFG_JSON_OBJECT)
        {
            const char *const key = json->text + json->node[item].key;
            cli_json_quoted(out, key, cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}));
            mmgr_line_put(out, ": ");
        }
        cli_written(json, item, out, inner);
        mmgr_line_put(out, last ? "\n" : ",\n");
    }
    for (unsigned long long space = 0ull; space < indent; space += 1ull)
    {
        mmgr_line_ch(out, ' ');
    }
    mmgr_line_ch(out, (kind == CFG_JSON_OBJECT) ? '}' : ']');
}

void cli_plugins_text_of(const CliJson *json, unsigned int plugin, VerbaLinea *out)
{
    cli_written(json, plugin, out, 0ull);
    mmgr_line_ch(out, '\n');
}

// the first word of the grammar's list `name`, NULL where it has none
static const char *cli_first(const CliJson *json, unsigned int plugin, const char *name)
{
    return cli_json_string(
        json, cli_json_item(json, cli_json_member(json, cli_json_member(json, plugin, "grammar"), name), 0u), NULL);
}

// A few lines in the new language to open and try: a comment, a keyword, a type, a constant, a string and a number,
// each where the plugin has one
void cli_plugins_sample_of(const CliJson *json, unsigned int plugin, VerbaLinea *out)
{
    const unsigned int comments = cli_json_member(json, plugin, "comments");
    const char *const line = cli_json_string(json, cli_json_member(json, comments, "line"), NULL);
    const char *const name = cli_json_string(json, cli_json_member(json, plugin, "name"), NULL);
    if (line != NULL)
    {
        mmgr_line_put(out, line);
        mmgr_line_ch(out, ' ');
        mmgr_line_put(out, (name != NULL) ? name : "this file");
        mmgr_line_put(out, " opens in this plugin\n");
    }
    const unsigned int block = cli_json_member(json, comments, "block");
    const char *const open = cli_json_string(json, cli_json_item(json, block, 0u), NULL);
    const char *const close = cli_json_string(json, cli_json_item(json, block, 1u), NULL);
    if ((open != NULL) && (close != NULL))
    {
        mmgr_line_put(out, open);
        mmgr_line_put(out, " a block comment ");
        mmgr_line_put(out, close);
        mmgr_line_ch(out, '\n');
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
    mmgr_line_put(out, keyword);
    mmgr_line_ch(out, ' ');
    mmgr_line_put(out, kind);
    mmgr_line_put(out, " name = ");
    mmgr_line_put(out, constant);
    mmgr_line_put(out, ";\n");
    mmgr_line_put(out, keyword);
    mmgr_line_put(out, " text = ");
    mmgr_line_put(out, quote);
    mmgr_line_put(out, "a string");
    mmgr_line_put(out, quote);
    mmgr_line_put(out, ", count = 0x2a + 1.5e3;\n");
}

// the plugin `plugin` of `json` written under `dir` as cli_plugins_create_in writes one: 1, or 0 with why given
static int plugins_written(const char *dir, const CliJson *json, unsigned int plugin, int replace, char *folder,
                           VerbaLinea *err_msg)
{
    const char *const id = cli_json_string(json, cli_json_member(json, plugin, "id"), NULL);
    char file[CLI_PATH_MAX_LEN];
    VerbaLinea line = {folder, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, dir);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, (id != NULL) ? id : "");
    const int folder_fits = mmgr_line_finish(&line) != 0ull;
    VerbaLinea named = {file, sizeof(file), 0ull};
    mmgr_line_put(&named, folder);
    mmgr_line_put(&named, "/plugin.json");
    if (!folder_fits || (mmgr_line_finish(&named) == 0ull))
    {
        mmgr_line_put(err_msg, "the plugin's folder is a longer path than orior_cli takes");
        return 0;
    }
    if (query_is_file(file) && !replace)
    {
        mmgr_line_put(err_msg, "a plugin ");
        mmgr_line_put(err_msg, id);
        mmgr_line_put(err_msg, " is there already, in ");
        mmgr_line_put(err_msg, folder);
        mmgr_line_put(err_msg, ": give another name, or replace it");
        return 0;
    }
    if (!query_folder_made(folder))
    {
        mmgr_line_put(err_msg, folder);
        mmgr_line_put(err_msg, ": it could not be made");
        return 0;
    }
    VerbaLinea text;
    (void)cli_growing_line_made(&text, CLI_PLUGIN_TEXT_FIRST);
    cli_plugins_text_of(json, plugin, &text);
    int written = mmgr_line_ok(&text) && query_file_written(file, text.out, text.at);
    cli_cell_release(text.out, NULL);
    const char *const extension =
        cli_json_string(json, cli_json_item(json, cli_json_member(json, plugin, "extensions"), 0u), NULL);
    char sample[CLI_PATH_MAX_LEN];
    VerbaLinea sample_name = {sample, sizeof(sample), 0ull};
    mmgr_line_put(&sample_name, folder);
    mmgr_line_put(&sample_name, "/sample.");
    mmgr_line_put(&sample_name, (extension != NULL) ? extension : "txt");
    // 1 where both are written, 0 where plugin.json is not, 2 where the sample is not
    if (written)
    {
        VerbaLinea sample_text;
        (void)cli_growing_line_made(&sample_text, CLI_PLUGIN_TEXT_FIRST);
        cli_plugins_sample_of(json, plugin, &sample_text);
        const int sampled = (mmgr_line_finish(&sample_name) != 0ull) && mmgr_line_ok(&sample_text) &&
                            query_file_written(sample, sample_text.out, sample_text.at);
        cli_cell_release(sample_text.out, NULL);
        written = sampled ? 1 : 2;
    }
    if (written != 1)
    {
        mmgr_line_put(err_msg, (written == 0) ? file : sample);
        mmgr_line_put(err_msg, ": it could not be written");
        return 0;
    }
    return 1;
}

int cli_plugins_create_in(const char *dir, const CliJson *json, unsigned int spec, int replace, char *folder,
                          VerbaLinea *err_msg)
{
    CliJson draft = {0};
    const unsigned int answers = cli_plugins_draft_tree(&draft, json, spec, err_msg);
    const unsigned int plugin =
        (answers != CLI_JSON_NONE) ? cli_plugins_draft(&draft, answers, err_msg) : CLI_JSON_NONE;
    const int written = (plugin != CLI_JSON_NONE) && plugins_written(dir, &draft, plugin, replace, folder, err_msg);
    cli_json_given(&draft);
    return written;
}

int cli_plugins_create(const CliJson *json, unsigned int spec, int replace, char *folder, VerbaLinea *err_msg)
{
    char dir[CLI_PATH_MAX_LEN];
    if (!cli_home_plugins(dir))
    {
        mmgr_line_put(err_msg, "orior_cli has no folder of its own to store plugins in");
        return 0;
    }
    return cli_plugins_create_in(dir, json, spec, replace, folder, err_msg);
}
