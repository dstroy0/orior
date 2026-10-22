// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_call_home.c: the window's calls on orior's own folder: the page's entries kept there and read back, the
// settings in force from the reader's and the tree's, the reader's settings carried to and taken from another
// machine, a plugin's own settings, the reader's language files, stylesheet and menus, and its places opened as the
// system opens them
#include "../command_line/cells.h"
#include "../machine/carry_internal.h"
#include "../machine/kept.h"
#include "../machine/settings.h"
#include "../plugins/plugins.h"
#include "window_internal.h"

// the bytes a text written for orior's folder is begun in before it grows
#define CLI_HOME_TEXT_FIRST (4ull << 10u)

// The settings' chains and layers of one call, a cell of them taken for it
typedef struct
{
    CliChain reader;
    CliChain tree;
    CliLayers layers;
} CliHomeSettings;

// the lock a call on orior's folder answers under, one at a time, and the cells it holds while it answers, each sized
// to what it holds
static CliLock s_lock = CLI_LOCK_INIT;
static CliCellsTally s_cells = CLI_CELLS_TALLY("cli_window_home");

// a growing line begun for a text of orior's folder: 1, or 0 with why said where its cell is refused
static int home_line_begun(CliLine *line, CliLine *said)
{
    const int begun = cli_line_growing(line, CLI_HOME_TEXT_FIRST, "a text of orior's folder");
    if (!begun)
    {
        cli_line_text(said, "a text of orior's folder could not be given a cell");
    }
    return begun;
}

// the strings of the argument `key` into a cell of their count, `*items`, given back with cli_cell_given_by(&s_cells,
// ...) either way, their count into `count`: 1, or 0 with why said
static int home_strings(const CliServeArgs *args, const char *key, const char ***items, unsigned int *count,
                        CliLine *said)
{
    const unsigned int node = cli_serve_arg(args, key);
    unsigned int most = 0u;
    const unsigned int first =
        cli_json_is(args->json, node, CFG_JSON_ARRAY) ? args->json->node[node].first : CLI_JSON_NONE;
    for (unsigned int item = first; item != CLI_JSON_NONE; item = args->json->node[item].next)
    {
        most += 1u;
    }
    *count = 0u;
    *items = cli_cell_taken_by(&s_cells, (most + 1ull) * sizeof(**items), "the strings a call on orior's folder names");
    if (*items == NULL)
    {
        cli_line_text(said, "the strings a call on orior's folder names could not be given a cell");
        return 0;
    }
    return cli_serve_strings(args, key, *items, most, count, said);
}

// the page's changed entries kept, each key starting `orior.` with its new text, or null where it is gone
static int home_kept_write(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)out;
    char folder[CLI_PATH_LONGEST];
    if (!cli_home_folder(folder))
    {
        cli_line_text(said, "orior_cli has no folder of its own to keep its entries in");
        return 0;
    }
    const unsigned int changes = cli_serve_arg(args, "changes");
    CliLine kept;
    if (!home_line_begun(&kept, said))
    {
        return 0;
    }
    cli_lock_take(&s_lock);
    cli_line_character(&kept, '{');
    const unsigned int first =
        cli_json_is(args->json, changes, CFG_JSON_OBJECT) ? args->json->node[changes].first : CLI_JSON_NONE;
    for (unsigned int member = first; member != CLI_JSON_NONE; member = args->json->node[member].next)
    {
        const char *const key = args->json->text + args->json->node[member].key;
        if (!cellul.starts(&(CatenaFinitaCfg){.src = key, .other = "orior.", .cap = CLI_CELLS_POOL}))
        {
            continue;
        }
        cli_line_text(&kept, (kept.at > 1ull) ? "," : "");
        cli_window_quoted(&kept, key + 6);
        cli_line_character(&kept, ':');
        cli_json_write(args->json, member, &kept, 0u, 0);
    }
    cli_line_character(&kept, '}');
    const int done = cli_line_fits(&kept) && cli_kept_keep(folder, kept.out, kept.at, said);
    cli_lock_give(&s_lock);
    cli_cell_given(kept.out);
    return done;
}

// what orior's folder keeps read into `json`, a tree in a cell sized to it, given back with cli_json_given either
// way, under the lock: its object, or CLI_JSON_NONE where it keeps nothing
static unsigned int home_kept_tree(CliJson *json)
{
    char folder[CLI_PATH_LONGEST];
    cli_json_begun(json, NULL, 0u, NULL, 0ull);
    CliLine kept;
    char why[256];
    CliLine ignored = {why, sizeof(why), 0ull};
    if (!cli_home_folder(folder) || !home_line_begun(&kept, &ignored))
    {
        return CLI_JSON_NONE;
    }
    const unsigned int tree = (cli_kept_read(folder, &kept) && cli_line_fits(&kept))
                                  ? cli_json_taken(json, kept.out, kept.at, 0u, 0ull, &ignored)
                                  : CLI_JSON_NONE;
    cli_cell_given(kept.out);
    return cli_json_is(json, tree, CFG_JSON_OBJECT) ? tree : CLI_JSON_NONE;
}

// what orior's folder keeps, each key with `orior.` before it, or null where it keeps nothing yet
static int home_kept_read(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    CliJson json;
    cli_lock_take(&s_lock);
    const unsigned int tree = home_kept_tree(&json);
    if (tree == CLI_JSON_NONE)
    {
        cli_json_given(&json);
        cli_lock_give(&s_lock);
        cli_line_text(out, "null");
        return 1;
    }
    cli_line_character(out, '{');
    for (unsigned int member = json.node[tree].first; member != CLI_JSON_NONE; member = json.node[member].next)
    {
        cli_line_text(out, (member != json.node[tree].first) ? ",\"orior." : "\"orior.");
        cli_json_escaped_lossy(
            out, (const unsigned char *)(json.text + json.node[member].key),
            cellul.len(&(CatenaFinitaCfg){.src = json.text + json.node[member].key, .cap = CLI_CELLS_POOL}));
        cli_line_text(out, "\":");
        cli_json_write(&json, member, out, 0u, 0);
    }
    cli_line_character(out, '}');
    cli_json_given(&json);
    cli_lock_give(&s_lock);
    return 1;
}

// the settings and themes orior's folder keeps now, by key without `orior.`, and each of the page's keys that is a
// setting the folder no longer keeps
static int home_kept_now(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    unsigned int count = 0u;
    const char **keys = NULL;
    if (!home_strings(args, "keys", &keys, &count, said))
    {
        cli_cell_given_by(&s_cells, keys);
        return 0;
    }
    CliJson json;
    cli_lock_take(&s_lock);
    const unsigned int tree = home_kept_tree(&json);
    const unsigned int first = (tree != CLI_JSON_NONE) ? json.node[tree].first : CLI_JSON_NONE;
    cli_line_text(out, "{\"set\":{");
    int any = 0;
    for (unsigned int member = first; member != CLI_JSON_NONE; member = json.node[member].next)
    {
        const char *const key = json.text + json.node[member].key;
        if (cli_kept_is_carried(key))
        {
            cli_line_text(out, any ? "," : "");
            cli_window_quoted(out, key);
            cli_line_character(out, ':');
            cli_json_write(&json, member, out, 0u, 0);
            any = 1;
        }
    }
    cli_line_text(out, "},\"gone\":[");
    any = 0;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const key = keys[at] + 6;
        if ((cellul.starts(&(CatenaFinitaCfg){.src = keys[at], .other = "orior.", .cap = CLI_CELLS_POOL})) &&
            cli_kept_is_carried(key) &&
            (!cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "themes", .cap = CLI_CELLS_POOL})) &&
            (cli_json_member(&json, tree, key) == CLI_JSON_NONE))
        {
            cli_line_text(out, any ? "," : "");
            cli_window_quoted(out, key);
            any = 1;
        }
    }
    cli_line_text(out, "]}");
    cli_json_given(&json);
    cli_lock_give(&s_lock);
    cli_cell_given_by(&s_cells, keys);
    return 1;
}

// the settings in force for the tree at `root`, from the reader's settings and the tree's, `tree`
static int home_settings_layers(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const root = cli_serve_string(args, "root", said);
    CliLine tree;
    if ((root == NULL) || !home_line_begun(&tree, said))
    {
        return 0;
    }
    CliHomeSettings *const settings =
        cli_cell_taken_by(&s_cells, sizeof(*settings), "the settings' chains and layers of a call");
    if (settings == NULL)
    {
        cli_cell_given(tree.out);
        cli_line_text(said, "the settings' chains and layers could not be given a cell");
        return 0;
    }
    cli_lock_take(&s_lock);
    cli_window_arg_written(args, "tree", &tree);
    const int read = cli_line_fits(&tree) && cli_settings_chain_read(tree.out, tree.at, &settings->tree, said);
    if (read)
    {
        cli_settings_reader(&settings->reader);
        cli_settings_layers(&settings->reader, &settings->tree, root, &settings->layers);
        cli_settings_layers_written(&settings->layers, out);
        cli_settings_layers_given(&settings->layers);
        cli_settings_chain_given(&settings->reader);
    }
    cli_settings_chain_given(&settings->tree);
    cli_lock_give(&s_lock);
    cli_cell_given_by(&s_cells, settings);
    cli_cell_given(tree.out);
    return read;
}

// the carried files written as an object of each place and its text
static void home_carried_written(const CliCarried *carried, CliLine *out)
{
    cli_line_character(out, '{');
    for (unsigned int at = 0u; at < carried->count; at += 1u)
    {
        cli_line_text(out, (at > 0u) ? "," : "");
        cli_window_quoted(out, cli_carried_place(carried, at));
        cli_line_character(out, ':');
        cli_window_counted(out, cli_carried_value(carried, at), carried->size[at]);
    }
    cli_line_character(out, '}');
}

// the reader's settings that travel written to `to`, a file or a folder of the reader's
static int home_carry_send(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char home[CLI_PATH_LONGEST];
    const char *const to = cli_serve_string(args, "to", said);
    CliLine done;
    if ((to == NULL) || !cli_window_home(home, said) || !home_line_begun(&done, said))
    {
        return 0;
    }
    const int sent = cli_carry_send(home, to, &done, said);
    if (sent)
    {
        cli_window_counted(out, done.out, cli_line_fits(&done) ? done.at : 0ull);
    }
    cli_cell_given(done.out);
    return sent;
}

// what the file or folder `from` carries, and how it differs from the reader's own here
static int home_carry_read(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char home[CLI_PATH_LONGEST];
    const char *const from = cli_serve_string(args, "from", said);
    if ((from == NULL) || !cli_window_home(home, said))
    {
        return 0;
    }
    CliCarried *const carried = cli_cell_taken_by(&s_cells, sizeof(*carried), "the files a carry holds");
    if (carried == NULL)
    {
        cli_line_text(said, "the files a carry holds could not be given a cell");
        return 0;
    }
    carried->count = 0u;
    cli_lock_take(&s_lock);
    const int read = cli_carry_read(from, carried, said);
    if (read)
    {
        cli_line_text(out, "{\"there\":");
        home_carried_written(carried, out);
        cli_line_text(out, ",\"differences\":");
        cli_carry_differences(home, carried, out);
        cli_line_character(out, '}');
    }
    cli_carried_clear(carried);
    cli_lock_give(&s_lock);
    cli_cell_given_by(&s_cells, carried);
    return read;
}

// the files of `places` that `there`, read from `from`, carries taken in, and of settings.json the settings of `keys`
static int home_carry_take(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    unsigned int places = 0u;
    unsigned int keys = 0u;
    char home[CLI_PATH_LONGEST];
    const char *const from = cli_serve_string(args, "from", said);
    if ((from == NULL) || !cli_window_home(home, said))
    {
        return 0;
    }
    const char **place_of = NULL;
    const char **key_of = NULL;
    CliCarried *const carried = cli_cell_taken_by(&s_cells, sizeof(*carried), "the files a carry holds");
    if ((carried == NULL) || !home_strings(args, "places", &place_of, &places, said) ||
        !home_strings(args, "keys", &key_of, &keys, said))
    {
        if (carried == NULL)
        {
            cli_line_text(said, "the files a carry holds could not be given a cell");
        }
        cli_cell_given_by(&s_cells, key_of);
        cli_cell_given_by(&s_cells, place_of);
        cli_cell_given_by(&s_cells, carried);
        return 0;
    }
    carried->count = 0u;
    cli_lock_take(&s_lock);
    const unsigned int there = cli_serve_arg(args, "there");
    const unsigned int first =
        cli_json_is(args->json, there, CFG_JSON_OBJECT) ? args->json->node[there].first : CLI_JSON_NONE;
    for (unsigned int member = first; member != CLI_JSON_NONE; member = args->json->node[member].next)
    {
        unsigned long long length = 0ull;
        const char *const text = cli_json_string(args->json, member, &length);
        if (text != NULL)
        {
            (void)cli_carried_put(carried, args->json->text + args->json->node[member].key, text, length);
        }
    }
    const int taken = cli_carry_take(home, from, carried, place_of, places, key_of, keys, out, said);
    cli_carried_clear(carried);
    cli_lock_give(&s_lock);
    cli_cell_given_by(&s_cells, key_of);
    cli_cell_given_by(&s_cells, place_of);
    cli_cell_given_by(&s_cells, carried);
    return taken;
}

// every taking kept, the last first
static int home_carry_takings(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    char home[CLI_PATH_LONGEST];
    if (!cli_home_folder(home))
    {
        cli_line_text(out, "[]");
        return 1;
    }
    cli_carry_takings(home, out);
    return 1;
}

// the taking `id` undone
static int home_carry_undo(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char home[CLI_PATH_LONGEST];
    const char *const id = cli_serve_string(args, "id", said);
    return (id != NULL) && cli_window_home(home, said) && cli_carry_undo(home, id, out, said);
}

// the file of the plugin `id`'s settings into `path`: 1, or 0 with why said
static int home_plugin_settings_path(const char *id, char *folder, char *path, CliLine *said)
{
    char home[CLI_PATH_LONGEST];
    char slug[CLI_NAME_LONGEST];
    if (!cli_window_home(home, said))
    {
        return 0;
    }
    cli_full(home, "plugin_settings", folder);
    cli_plugins_slug(id, slug);
    CliLine line = {path, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, folder);
    cli_line_character(&line, '/');
    cli_line_text(&line, slug);
    cli_line_text(&line, ".json");
    return cli_line_closed(&line) != 0ull;
}

// the settings of the plugin `id`, as it last wrote them, or an empty object
static int home_plugin_settings_read(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    const char *const id = cli_serve_string(args, "id", said);
    if ((id == NULL) || !home_plugin_settings_path(id, folder, path, said))
    {
        return 0;
    }
    cli_lock_take(&s_lock);
    unsigned long long size = 0ull;
    const unsigned char *const mapped = cli_file_held(path, &size);
    const unsigned long long start = out->at;
    if ((mapped == NULL) || !cli_json_compacted((const char *)mapped, size, out))
    {
        out->at = start;
        cli_line_text(out, "{}");
    }
    cli_file_let_go(mapped, size);
    cli_lock_give(&s_lock);
    return 1;
}

// `values` kept as the settings of the plugin `id`, in place of those it kept before
static int home_plugin_settings_write(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)out;
    const char *const id = cli_serve_string(args, "id", said);
    const unsigned int values = cli_serve_arg(args, "values");
    if (id == NULL)
    {
        return 0;
    }
    if (!cli_json_is(args->json, values, CFG_JSON_OBJECT))
    {
        cli_line_text(said, "a plugin's settings are an object");
        return 0;
    }
    char folder[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    if (!home_plugin_settings_path(id, folder, path, said))
    {
        return 0;
    }
    if (!query_folder_made(folder))
    {
        cli_line_text(said, folder);
        cli_line_text(said, ": it could not be made");
        return 0;
    }
    CliLine text;
    if (!home_line_begun(&text, said))
    {
        return 0;
    }
    cli_lock_take(&s_lock);
    cli_json_write(args->json, values, &text, 2u, 0);
    cli_line_character(&text, '\n');
    const int written = cli_line_fits(&text) && query_file_written(path, text.out, text.at);
    cli_lock_give(&s_lock);
    cli_cell_given(text.out);
    if (!written)
    {
        cli_line_text(said, path);
        cli_line_text(said, ": it could not be written");
    }
    return written;
}

const CliServeCall g_cli_window_home_calls[] = {
    {"kept_write", home_kept_write},
    {"kept_read", home_kept_read},
    {"kept_now", home_kept_now},
    {"settings_layers", home_settings_layers},
    {"carry_send", home_carry_send},
    {"carry_read", home_carry_read},
    {"carry_take", home_carry_take},
    {"carry_takings", home_carry_takings},
    {"carry_undo", home_carry_undo},
    {"plugin_settings_read", home_plugin_settings_read},
    {"plugin_settings_write", home_plugin_settings_write},
};
const unsigned int g_cli_window_home_count = sizeof(g_cli_window_home_calls) / sizeof(g_cli_window_home_calls[0]);
