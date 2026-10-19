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

// A call's room, a cell taken with the lock and given back with it: the changes kept and their tree, the keys and
// places asked for, the settings' chains and layers, and what a carry holds
typedef struct
{
    char text[1u << 22u];
    CliJsonNode node[1u << 16u];
    char tree_text[1u << 22u];
    CfgJsonToken token[1u << 16u];
    CliJson json;
    const char *keys[4096];
    const char *places[CLI_CARRIED_MOST];
    CliChain reader;
    CliChain tree;
    CliLayers layers;
    CliCarried carried;
} CliHomeRoom;

static CliLock s_lock = CLI_LOCK_INIT;
static CliHomeRoom *s_home;

// the lock taken and the room a cell under it: 1, or 0 with why said and the lock given back
static int home_held(CliLine *said)
{
    cli_lock_take(&s_lock);
    s_home = cli_cell_taken(sizeof(*s_home), "a call on the window's home");
    if (s_home == NULL)
    {
        cli_lock_give(&s_lock);
        cli_line_text(said, "a call on the window's home could not be taken");
    }
    return s_home != NULL;
}

// the room given back, and the lock with it
static void home_released(void)
{
    cli_cell_given(s_home);
    s_home = NULL;
    cli_lock_give(&s_lock);
}

// the page's changed entries kept, each key starting `orior.` with its new text, or null where it is gone
static int home_kept_write(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)out;
    char folder[CLI_PATH_LONGEST];
    if (!cli_home_folder(folder))
    {
        cli_line_text(said, "orior has no folder of its own to keep its entries in");
        return 0;
    }
    const unsigned int changes = cli_serve_arg(args, "changes");
    if (!home_held(said))
    {
        return 0;
    }
    CliLine kept = {s_home->text, sizeof(s_home->text), 0ull};
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
    const int done = cli_line_fits(&kept) && cli_kept_keep(folder, s_home->text, kept.at, said);
    home_released();
    return done;
}

// what orior's folder keeps read into the tree, under the lock: its object, or CLI_JSON_NONE where it keeps nothing
static unsigned int home_kept_tree(void)
{
    char folder[CLI_PATH_LONGEST];
    cli_json_begun(&s_home->json, s_home->node, sizeof(s_home->node) / sizeof(s_home->node[0]), s_home->tree_text,
                   sizeof(s_home->tree_text));
    CliLine kept = {s_home->text, sizeof(s_home->text), 0ull};
    if (!cli_home_folder(folder) || !cli_kept_read(folder, &kept) || !cli_line_fits(&kept))
    {
        return CLI_JSON_NONE;
    }
    char why[256];
    CliLine ignored = {why, sizeof(why), 0ull};
    const unsigned int tree = cli_json_read(&s_home->json, s_home->text, kept.at, s_home->token,
                                            sizeof(s_home->token) / sizeof(s_home->token[0]), &ignored);
    return cli_json_is(&s_home->json, tree, CFG_JSON_OBJECT) ? tree : CLI_JSON_NONE;
}

// what orior's folder keeps, each key with `orior.` before it, or null where it keeps nothing yet
static int home_kept_read(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    if (!home_held(said))
    {
        return 0;
    }
    const unsigned int tree = home_kept_tree();
    if (tree == CLI_JSON_NONE)
    {
        home_released();
        cli_line_text(out, "null");
        return 1;
    }
    cli_line_character(out, '{');
    for (unsigned int member = s_home->json.node[tree].first; member != CLI_JSON_NONE;
         member = s_home->json.node[member].next)
    {
        cli_line_text(out, (member != s_home->json.node[tree].first) ? ",\"orior." : "\"orior.");
        cli_json_escaped_lossy(out, (const unsigned char *)(s_home->json.text + s_home->json.node[member].key),
                               cellul.len(&(CatenaFinitaCfg){.src = s_home->json.text + s_home->json.node[member].key,
                                                             .cap = CLI_CELLS_POOL}));
        cli_line_text(out, "\":");
        cli_json_write(&s_home->json, member, out, 0u, 0);
    }
    cli_line_character(out, '}');
    home_released();
    return 1;
}

// the settings and themes orior's folder keeps now, by key without `orior.`, and each of the page's keys that is a
// setting the folder no longer keeps
static int home_kept_now(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    unsigned int count = 0u;
    if (!home_held(said))
    {
        return 0;
    }
    if (!cli_serve_strings(args, "keys", s_home->keys, 4096u, &count, said))
    {
        home_released();
        return 0;
    }
    const unsigned int tree = home_kept_tree();
    const unsigned int first = (tree != CLI_JSON_NONE) ? s_home->json.node[tree].first : CLI_JSON_NONE;
    cli_line_text(out, "{\"set\":{");
    int any = 0;
    for (unsigned int member = first; member != CLI_JSON_NONE; member = s_home->json.node[member].next)
    {
        const char *const key = s_home->json.text + s_home->json.node[member].key;
        if (cli_kept_is_carried(key))
        {
            cli_line_text(out, any ? "," : "");
            cli_window_quoted(out, key);
            cli_line_character(out, ':');
            cli_json_write(&s_home->json, member, out, 0u, 0);
            any = 1;
        }
    }
    cli_line_text(out, "},\"gone\":[");
    any = 0;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const key = s_home->keys[at] + 6;
        if ((cellul.starts(&(CatenaFinitaCfg){.src = s_home->keys[at], .other = "orior.", .cap = CLI_CELLS_POOL})) &&
            cli_kept_is_carried(key) &&
            (!cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "themes", .cap = CLI_CELLS_POOL})) &&
            (cli_json_member(&s_home->json, tree, key) == CLI_JSON_NONE))
        {
            cli_line_text(out, any ? "," : "");
            cli_window_quoted(out, key);
            any = 1;
        }
    }
    cli_line_text(out, "]}");
    home_released();
    return 1;
}

// the settings in force for the tree at `root`, from the reader's settings and the tree's, `tree`
static int home_settings_layers(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const root = cli_serve_string(args, "root", said);
    if (root == NULL)
    {
        return 0;
    }
    if (!home_held(said))
    {
        return 0;
    }
    CliLine tree = {s_home->text, sizeof(s_home->text), 0ull};
    cli_window_arg_written(args, "tree", &tree);
    const int read = cli_line_fits(&tree) && cli_settings_chain_read(s_home->text, tree.at, &s_home->tree, said);
    if (read)
    {
        cli_settings_reader(&s_home->reader);
        cli_settings_layers(&s_home->reader, &s_home->tree, root, &s_home->layers);
        cli_settings_layers_written(&s_home->layers, out);
    }
    home_released();
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
    static char s_done[1u << 14u];
    CliLine done = {s_done, sizeof(s_done), 0ull};
    if ((to == NULL) || !cli_window_home(home, said) || !cli_carry_send(home, to, &done, said))
    {
        return 0;
    }
    cli_window_counted(out, s_done, cli_line_fits(&done) ? done.at : 0ull);
    return 1;
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
    if (!home_held(said))
    {
        return 0;
    }
    const int read = cli_carry_read(from, &s_home->carried, said);
    if (read)
    {
        cli_line_text(out, "{\"there\":");
        home_carried_written(&s_home->carried, out);
        cli_line_text(out, ",\"differences\":");
        cli_carry_differences(home, &s_home->carried, out);
        cli_line_character(out, '}');
    }
    home_released();
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
    if (!home_held(said))
    {
        return 0;
    }
    if (!cli_serve_strings(args, "places", s_home->places, CLI_CARRIED_MOST, &places, said) ||
        !cli_serve_strings(args, "keys", s_home->keys, 4096u, &keys, said))
    {
        home_released();
        return 0;
    }
    const unsigned int there = cli_serve_arg(args, "there");
    cli_carried_clear(&s_home->carried);
    const unsigned int first =
        cli_json_is(args->json, there, CFG_JSON_OBJECT) ? args->json->node[there].first : CLI_JSON_NONE;
    for (unsigned int member = first; member != CLI_JSON_NONE; member = args->json->node[member].next)
    {
        unsigned long long length = 0ull;
        const char *const text = cli_json_string(args->json, member, &length);
        if (text != NULL)
        {
            (void)cli_carried_put(&s_home->carried, args->json->text + args->json->node[member].key, text, length);
        }
    }
    const int taken =
        cli_carry_take(home, from, &s_home->carried, s_home->places, places, s_home->keys, keys, out, said);
    home_released();
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
    if (!home_held(said))
    {
        return 0;
    }
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_home->text, sizeof(s_home->text));
    const unsigned long long start = out->at;
    if ((size == QRY_MEMBER_UNREAD) || !cli_json_compacted(s_home->text, size, out))
    {
        out->at = start;
        cli_line_text(out, "{}");
    }
    home_released();
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
    if (!home_held(said))
    {
        return 0;
    }
    CliLine text = {s_home->text, sizeof(s_home->text), 0ull};
    cli_json_write(args->json, values, &text, 2u, 0);
    cli_line_character(&text, '\n');
    const int written = cli_line_fits(&text) && query_file_written(path, s_home->text, text.at);
    home_released();
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
