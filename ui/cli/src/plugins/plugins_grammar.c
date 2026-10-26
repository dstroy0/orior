// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugins_grammar.c: a language plugin's grammar stored for the colorer under `plugin:<id>`, read from the plugin.json
// of the plugin of that id that stands, the reader's in place of one that comes with orior, and read again only where
// that text changed
#include "../command_line/cells.h"
#include "../editor/highlight.h"
#include "plugins.h"

// the plugins whose grammars' stamps the list's room is begun with before it doubles
#define CLI_PLUGIN_GRAMMARS_FIRST 16u

// A plugin whose grammar is stored: its id, and the stamp of the plugin.json it was read from
typedef struct
{
    char id[CLI_NAME_MAX_LEN];
    unsigned long long stamp;
} CliPluginGrammar;

// the plugins whose grammars are stored, in a cell of the plugins' whose room doubles as they are put, and how many
static CliPluginGrammar *s_grammars;
static unsigned int s_grammar_room;
static unsigned int s_grammar_count;
static CliLock s_lock = CLI_LOCK_INIT;

// the list of the plugins whose grammars are stored twice its room, or CLI_PLUGIN_GRAMMARS_FIRST where it has none: 1,
// or 0 where the cells cannot meet it; under the lock
static int cli_grammars_grown(void)
{
    const unsigned int room = (s_grammar_room == 0u) ? CLI_PLUGIN_GRAMMARS_FIRST : (2u * s_grammar_room);
    CliPluginGrammar *const grown =
        g_cli_cells->persistent_buf_alloc((unsigned long long)room * sizeof(*grown), &g_cli_plugins_cells);
    if (grown == NULL)
    {
        return 0;
    }
    if (s_grammars != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = grown, .src = s_grammars, .bytes = (size_t)s_grammar_count * sizeof(*grown)});
    }
    g_cli_cells->persistent_buf_release(s_grammars, &g_cli_plugins_cells);
    s_grammars = grown;
    s_grammar_room = room;
    return 1;
}

// The plugin sought: its id; whether one of that id was found, and the stamp of the last, the one that stands; and on
// the pass that stores it, the key, whether it was stored, and why where it was not
typedef struct
{
    const char *id;
    int found;
    unsigned long long stamp;
    const char *key;
    int store;
    int stored;
    VerbaLinea *err_msg;
} CliGrammarSought;

// a text's stamp, FNV-1a over its bytes
static unsigned long long cli_grammar_stamp(const char *text, unsigned long long size)
{
    unsigned long long stamp = 0xcbf29ce484222325ull;
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        stamp = (stamp ^ (unsigned char)text[at]) * 0x100000001b3ull;
    }
    return stamp;
}

// a plugin of the id sought: its stamp noted, or on the pass that stores, its grammar stored where it is the one that
// stands
static void cli_grammar_each(void *context, const CliPlugin *plugin)
{
    CliGrammarSought *const sought = (CliGrammarSought *)context;
    if (!cellul.eq(&(CatenaFinitaCfg){.src = plugin->id, .other = sought->id, .cap = CLI_CELLS_POOL}))
    {
        return;
    }
    const unsigned long long stamp = cli_grammar_stamp(plugin->text, plugin->size);
    if (!sought->store)
    {
        sought->found = 1;
        sought->stamp = stamp;
        return;
    }
    if (stamp != sought->stamp)
    {
        return;
    }
    CliJson json;
    cli_json_begun(&json, NULL, 0u, NULL, 0ull);
    const unsigned int root = cli_json_taken(&json, plugin->text, plugin->size, 0u, 0ull, sought->err_msg);
    const unsigned int grammar = (root != CLI_JSON_NONE) ? cli_json_member(&json, root, "grammar") : CLI_JSON_NONE;
    if ((root != CLI_JSON_NONE) && (grammar == CLI_JSON_NONE))
    {
        mmgr_line_put(sought->err_msg, "the plugin ");
        mmgr_line_put(sought->err_msg, sought->id);
        mmgr_line_put(sought->err_msg, " has no grammar");
    }
    sought->stored = (grammar != CLI_JSON_NONE) && cli_highlight_store(sought->key, &json, grammar, sought->err_msg);
    cli_json_given(&json);
}

int cli_plugins_grammar_stored(const char *id, char *key, VerbaLinea *err_msg)
{
    VerbaLinea named = {key, CLI_NAME_MAX_LEN, 0ull};
    mmgr_line_put(&named, "plugin:");
    mmgr_line_put(&named, id);
    if (mmgr_line_finish(&named) == 0ull)
    {
        mmgr_line_put(err_msg, "a plugin's id is longer than orior_cli supports");
        return 0;
    }
    cli_lock_take(&s_lock);
    CliGrammarSought sought = {id, 0, 0ull, key, 0, 0, err_msg};
    cli_plugins_each(cli_grammar_each, &sought);
    unsigned int at = 0u;
    while ((at < s_grammar_count) &&
           !cellul.eq(&(CatenaFinitaCfg){.src = s_grammars[at].id, .other = id, .cap = CLI_CELLS_POOL}))
    {
        at += 1u;
    }
    int stored = sought.found && (at < s_grammar_count) && (s_grammars[at].stamp == sought.stamp);
    if (sought.found && !stored)
    {
        sought.store = 1;
        cli_plugins_each(cli_grammar_each, &sought);
        stored = sought.stored;
        // a grammar whose stamp the cells cannot hold is read again the next time it is sought
        if (stored && (at == s_grammar_count) && ((s_grammar_count < s_grammar_room) || cli_grammars_grown()))
        {
            memor.cpy(&(MemoriaCfg){.dst = s_grammars[at].id,
                                    .src = id,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}) + 1u});
            s_grammar_count += 1u;
        }
        if (stored && (at < s_grammar_count))
        {
            s_grammars[at].stamp = sought.stamp;
        }
    }
    cli_lock_give(&s_lock);
    if (!sought.found)
    {
        mmgr_line_put(err_msg, "no plugin is named ");
        mmgr_line_put(err_msg, id);
    }
    return stored;
}
