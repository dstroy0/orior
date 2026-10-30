// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// window_call_home.c: the window's calls on orior's own folder: the page's entries stored there and read back, the
// settings in force from the reader's and the tree's, the reader's settings carried to and taken from another
// machine, a plugin's own settings, the reader's language files, stylesheet and menus, and its places opened as the
// system opens them
#include "../command_line/cells.h"
#include "../machine/carry_internal.h"
#include "../machine/settings.h"
#include "../machine/state.h"
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

// the lock a call on orior's folder answers under, one at a time, and the cells it occupies while it answers, each
// sized to what it contains
static CliLock s_lock = CLI_LOCK_INIT;
static CellOwner s_cells = MMGR_CELL_OWNER("cli_window_home");

// a growing line begun for a text of orior's folder: 1, or 0 with why given where its cell is refused
static int home_line_begun(VerbaLinea *line, VerbaLinea *err_msg)
{
    const int begun = cli_growing_line_made(line, CLI_HOME_TEXT_FIRST);
    if (!begun)
    {
        mmgr_line_put(err_msg, "a text of orior's folder could not be given a cell");
    }
    return begun;
}

// the strings of the argument `key` into a cell of their count, `*items`, given back with
// cli_cell_release(&s_cells,
// ...) either way, their count into `count`: 1, or 0 with why given
static int home_strings(const CliServeArgs *args, const char *key, const char ***items, unsigned int *count,
                        VerbaLinea *err_msg)
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
    *items = g_cli_cells->persistent_buf_alloc((most + 1ull) * sizeof(**items), &s_cells);
    if (*items == NULL)
    {
        mmgr_line_put(err_msg, "the strings a call on orior's folder names could not be given a cell");
        return 0;
    }
    return cli_serve_strings(args, key, *items, most, count, err_msg);
}

// the page's changed entries stored, each key starting `orior.` with its new text, or null where it is gone
static int home_state_write(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)out;
    char folder[CLI_PATH_MAX_LEN];
    if (!cli_home_folder(folder))
    {
        mmgr_line_put(err_msg, "orior_cli has no folder of its own to store its entries in");
        return 0;
    }
    const unsigned int changes = cli_serve_arg(args, "changes");
    VerbaLinea state_line;
    if (!home_line_begun(&state_line, err_msg))
    {
        return 0;
    }
    cli_lock_take(&s_lock);
    mmgr_line_ch(&state_line, '{');
    const unsigned int first =
        cli_json_is(args->json, changes, CFG_JSON_OBJECT) ? args->json->node[changes].first : CLI_JSON_NONE;
    for (unsigned int member = first; member != CLI_JSON_NONE; member = args->json->node[member].next)
    {
        const char *const key = args->json->text + args->json->node[member].key;
        if (!cellul.starts(&(CatenaFinitaCfg){.src = key, .other = "orior.", .cap = CLI_CELLS_POOL}))
        {
            continue;
        }
        mmgr_line_put(&state_line, (state_line.at > 1ull) ? "," : "");
        cli_window_quoted(&state_line, key + 6);
        mmgr_line_ch(&state_line, ':');
        cli_json_write(args->json, member, &state_line, 0u, 0);
    }
    mmgr_line_ch(&state_line, '}');
    const int done = mmgr_line_ok(&state_line) && cli_state_save(folder, state_line.out, state_line.at, err_msg);
    cli_lock_give(&s_lock);
    cli_cell_release(state_line.out, NULL);
    return done;
}

// what orior's folder stores read into `json`, a tree in a cell sized to it, given back with cli_json_given either
// way, under the lock: its object, or CLI_JSON_NONE where it stores nothing
static unsigned int home_state_tree(CliJson *json)
{
    char folder[CLI_PATH_MAX_LEN];
    cli_json_begun(json, NULL, 0u, NULL, 0ull);
    VerbaLinea state_line;
    char why[256];
    VerbaLinea ignored = {why, sizeof(why), 0ull};
    if (!cli_home_folder(folder) || !home_line_begun(&state_line, &ignored))
    {
        return CLI_JSON_NONE;
    }
    const unsigned int tree = (cli_state_read(folder, &state_line) && mmgr_line_ok(&state_line))
                                  ? cli_json_taken(json, state_line.out, state_line.at, 0u, 0ull, &ignored)
                                  : CLI_JSON_NONE;
    cli_cell_release(state_line.out, NULL);
    return cli_json_is(json, tree, CFG_JSON_OBJECT) ? tree : CLI_JSON_NONE;
}

// what orior's folder stores, each key with `orior.` before it, or null where it stores nothing yet
static int home_state_read(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    (void)err_msg;
    CliJson json;
    cli_lock_take(&s_lock);
    const unsigned int tree = home_state_tree(&json);
    if (tree == CLI_JSON_NONE)
    {
        cli_json_given(&json);
        cli_lock_give(&s_lock);
        mmgr_line_put(out, "null");
        return 1;
    }
    mmgr_line_ch(out, '{');
    for (unsigned int member = json.node[tree].first; member != CLI_JSON_NONE; member = json.node[member].next)
    {
        mmgr_line_put(out, (member != json.node[tree].first) ? ",\"orior." : "\"orior.");
        cli_json_escaped_lossy(
            out, (const unsigned char *)(json.text + json.node[member].key),
            cellul.len(&(CatenaFinitaCfg){.src = json.text + json.node[member].key, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, "\":");
        cli_json_write(&json, member, out, 0u, 0);
    }
    mmgr_line_ch(out, '}');
    cli_json_given(&json);
    cli_lock_give(&s_lock);
    return 1;
}

// the bytes the page's first script is begun in, doubling as it fills
#define CLI_WINDOW_KEPT_SCRIPT_FIRST (16ull << 10u)

// the page's first script before what orior's folder stores and after it
static const char s_kept_before[] =
    "(() => { if (location.protocol === \"view:\" || location.hostname === \"view.localhost\") return; "
    "if (sessionStorage.getItem(\"orior.kept.read\")) return; sessionStorage.setItem(\"orior.kept.read\", \"1\"); "
    "const kept = ";
static const char s_kept_after[] =
    "; if (kept === null) { window.oriorKeepAll = true; return; } "
    "for (const key of Object.keys(localStorage)) { if (key.startsWith(\"orior.\") && !(key in kept)) "
    "localStorage.removeItem(key); } "
    "for (const [key, text] of Object.entries(kept)) localStorage.setItem(key, text); })();";

// the page's first script, made once
static VerbaLinea s_kept_script;

const char *cli_window_kept_script(unsigned long long *size)
{
    *size = 0ull;
    if (!cli_growing_line_made(&s_kept_script, CLI_WINDOW_KEPT_SCRIPT_FIRST))
    {
        return NULL;
    }
    mmgr_line_put(&s_kept_script, s_kept_before);
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    (void)home_state_read(NULL, &s_kept_script, &err_msg);
    mmgr_line_put(&s_kept_script, s_kept_after);
    *size = mmgr_line_ok(&s_kept_script) ? s_kept_script.at : 0ull;
    return s_kept_script.out;
}

void cli_window_kept_script_given(void)
{
    cli_cell_release(s_kept_script.out, NULL);
    s_kept_script = (VerbaLinea){NULL, 0ull, 0ull};
}

int cli_window_title_bar_system(void)
{
    CliJson json;
    cli_lock_take(&s_lock);
    const unsigned int tree = home_state_tree(&json);
    const char *const title_bar = cli_json_string(&json, cli_json_member(&json, tree, "titlebar"), NULL);
    const int system = (title_bar != NULL) &&
                       cellul.eq(&(CatenaFinitaCfg){.src = title_bar, .other = "system", .cap = CLI_CELLS_POOL});
    cli_json_given(&json);
    cli_lock_give(&s_lock);
    return system;
}

// the settings and themes orior's folder stores now, by key without `orior.`, and each of the page's keys that is a
// setting the folder no longer stores
static int home_state_now(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    unsigned int count = 0u;
    const char **keys = NULL;
    if (!home_strings(args, "keys", &keys, &count, err_msg))
    {
        cli_cell_release(keys, &s_cells);
        return 0;
    }
    CliJson json;
    cli_lock_take(&s_lock);
    const unsigned int tree = home_state_tree(&json);
    const unsigned int first = (tree != CLI_JSON_NONE) ? json.node[tree].first : CLI_JSON_NONE;
    mmgr_line_put(out, "{\"set\":{");
    int any = 0;
    for (unsigned int member = first; member != CLI_JSON_NONE; member = json.node[member].next)
    {
        const char *const key = json.text + json.node[member].key;
        if (cli_state_is_carried(key))
        {
            mmgr_line_put(out, any ? "," : "");
            cli_window_quoted(out, key);
            mmgr_line_ch(out, ':');
            cli_json_write(&json, member, out, 0u, 0);
            any = 1;
        }
    }
    mmgr_line_put(out, "},\"gone\":[");
    any = 0;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const key = keys[at] + 6;
        if ((cellul.starts(&(CatenaFinitaCfg){.src = keys[at], .other = "orior.", .cap = CLI_CELLS_POOL})) &&
            cli_state_is_carried(key) &&
            (!cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "themes", .cap = CLI_CELLS_POOL})) &&
            (cli_json_member(&json, tree, key) == CLI_JSON_NONE))
        {
            mmgr_line_put(out, any ? "," : "");
            cli_window_quoted(out, key);
            any = 1;
        }
    }
    mmgr_line_put(out, "]}");
    cli_json_given(&json);
    cli_lock_give(&s_lock);
    cli_cell_release(keys, &s_cells);
    return 1;
}

// the settings in force for the tree at `root`, from the reader's settings and the tree's, `tree`
static int home_settings_layers(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    const char *const root = cli_serve_string(args, "root", err_msg);
    VerbaLinea tree;
    if ((root == NULL) || !home_line_begun(&tree, err_msg))
    {
        return 0;
    }
    CliHomeSettings *const settings = g_cli_cells->persistent_buf_alloc(sizeof(*settings), &s_cells);
    if (settings == NULL)
    {
        cli_cell_release(tree.out, NULL);
        mmgr_line_put(err_msg, "the settings' chains and layers could not be given a cell");
        return 0;
    }
    cli_lock_take(&s_lock);
    cli_window_arg_written(args, "tree", &tree);
    const int read = mmgr_line_ok(&tree) && cli_settings_chain_read(tree.out, tree.at, &settings->tree, err_msg);
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
    cli_cell_release(settings, &s_cells);
    cli_cell_release(tree.out, NULL);
    return read;
}

// the carried files written as an object of each place and its text
static void home_carried_written(const CliCarried *carried, VerbaLinea *out)
{
    mmgr_line_ch(out, '{');
    for (unsigned int at = 0u; at < carried->count; at += 1u)
    {
        mmgr_line_put(out, (at > 0u) ? "," : "");
        cli_window_quoted(out, cli_carried_place(carried, at));
        mmgr_line_ch(out, ':');
        cli_window_counted(out, cli_carried_value(carried, at), carried->size[at]);
    }
    mmgr_line_ch(out, '}');
}

// the reader's settings that travel written to `to`, a file or a folder of the reader's
static int home_carry_send(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char home[CLI_PATH_MAX_LEN];
    const char *const to = cli_serve_string(args, "to", err_msg);
    VerbaLinea done;
    if ((to == NULL) || !cli_window_home(home, err_msg) || !home_line_begun(&done, err_msg))
    {
        return 0;
    }
    const int sent = cli_carry_send(home, to, &done, err_msg);
    if (sent)
    {
        cli_window_counted(out, done.out, mmgr_line_ok(&done) ? done.at : 0ull);
    }
    cli_cell_release(done.out, NULL);
    return sent;
}

// what the file or folder `from` carries, and how it differs from the reader's own here
static int home_carry_read(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char home[CLI_PATH_MAX_LEN];
    const char *const from = cli_serve_string(args, "from", err_msg);
    if ((from == NULL) || !cli_window_home(home, err_msg))
    {
        return 0;
    }
    CliCarried *const carried = g_cli_cells->persistent_buf_alloc(sizeof(*carried), &s_cells);
    if (carried == NULL)
    {
        mmgr_line_put(err_msg, "the files a carry contains could not be given a cell");
        return 0;
    }
    carried->count = 0u;
    cli_lock_take(&s_lock);
    const int read = cli_carry_read(from, carried, err_msg);
    if (read)
    {
        mmgr_line_put(out, "{\"there\":");
        home_carried_written(carried, out);
        mmgr_line_put(out, ",\"differences\":");
        cli_carry_differences(home, carried, out);
        mmgr_line_ch(out, '}');
    }
    cli_carried_clear(carried);
    cli_lock_give(&s_lock);
    cli_cell_release(carried, &s_cells);
    return read;
}

// the files of `places` that `there`, read from `from`, carries taken in, and of settings.json the settings of `keys`
static int home_carry_take(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    unsigned int places = 0u;
    unsigned int keys = 0u;
    char home[CLI_PATH_MAX_LEN];
    const char *const from = cli_serve_string(args, "from", err_msg);
    if ((from == NULL) || !cli_window_home(home, err_msg))
    {
        return 0;
    }
    const char **place_of = NULL;
    const char **key_of = NULL;
    CliCarried *const carried = g_cli_cells->persistent_buf_alloc(sizeof(*carried), &s_cells);
    if ((carried == NULL) || !home_strings(args, "places", &place_of, &places, err_msg) ||
        !home_strings(args, "keys", &key_of, &keys, err_msg))
    {
        if (carried == NULL)
        {
            mmgr_line_put(err_msg, "the files a carry contains could not be given a cell");
        }
        cli_cell_release(key_of, &s_cells);
        cli_cell_release(place_of, &s_cells);
        cli_cell_release(carried, &s_cells);
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
    const int taken = cli_carry_take(home, from, carried, place_of, places, key_of, keys, out, err_msg);
    cli_carried_clear(carried);
    cli_lock_give(&s_lock);
    cli_cell_release(key_of, &s_cells);
    cli_cell_release(place_of, &s_cells);
    cli_cell_release(carried, &s_cells);
    return taken;
}

// every taking saved, the last first
static int home_carry_takings(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)args;
    (void)err_msg;
    char home[CLI_PATH_MAX_LEN];
    if (!cli_home_folder(home))
    {
        mmgr_line_put(out, "[]");
        return 1;
    }
    cli_carry_takings(home, out);
    return 1;
}

// the taking `id` undone
static int home_carry_undo(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char home[CLI_PATH_MAX_LEN];
    const char *const id = cli_serve_string(args, "id", err_msg);
    return (id != NULL) && cli_window_home(home, err_msg) && cli_carry_undo(home, id, out, err_msg);
}

// the file of the plugin `id`'s settings into `path`: 1, or 0 with why given
static int home_plugin_settings_path(const char *id, char *folder, char *path, VerbaLinea *err_msg)
{
    char home[CLI_PATH_MAX_LEN];
    char slug[CLI_NAME_MAX_LEN];
    if (!cli_window_home(home, err_msg))
    {
        return 0;
    }
    cli_full(home, "plugin_settings", folder);
    cli_plugins_slug(id, slug);
    VerbaLinea line = {path, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, slug);
    mmgr_line_put(&line, ".json");
    return mmgr_line_finish(&line) != 0ull;
}

// the settings of the plugin `id`, as it last wrote them, or an empty object
static int home_plugin_settings_read(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    char folder[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    const char *const id = cli_serve_string(args, "id", err_msg);
    if ((id == NULL) || !home_plugin_settings_path(id, folder, path, err_msg))
    {
        return 0;
    }
    cli_lock_take(&s_lock);
    unsigned long long size = 0ull;
    const unsigned char *const mapped = cli_file_read(path, &size);
    const unsigned long long start = out->at;
    if ((mapped == NULL) || !cli_json_compacted((const char *)mapped, size, out))
    {
        out->at = start;
        mmgr_line_put(out, "{}");
    }
    cli_file_let_go(mapped, size);
    cli_lock_give(&s_lock);
    return 1;
}

// `values` stored as the settings of the plugin `id`, in place of those it stored before
static int home_plugin_settings_write(const CliServeArgs *args, VerbaLinea *out, VerbaLinea *err_msg)
{
    (void)out;
    const char *const id = cli_serve_string(args, "id", err_msg);
    const unsigned int values = cli_serve_arg(args, "values");
    if (id == NULL)
    {
        return 0;
    }
    if (!cli_json_is(args->json, values, CFG_JSON_OBJECT))
    {
        mmgr_line_put(err_msg, "a plugin's settings are an object");
        return 0;
    }
    char folder[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    if (!home_plugin_settings_path(id, folder, path, err_msg))
    {
        return 0;
    }
    if (!query_folder_made(folder))
    {
        mmgr_line_put(err_msg, folder);
        mmgr_line_put(err_msg, ": it could not be made");
        return 0;
    }
    VerbaLinea text;
    if (!home_line_begun(&text, err_msg))
    {
        return 0;
    }
    cli_lock_take(&s_lock);
    cli_json_write(args->json, values, &text, 2u, 0);
    mmgr_line_ch(&text, '\n');
    const int written = mmgr_line_ok(&text) && query_file_written(path, text.out, text.at);
    cli_lock_give(&s_lock);
    cli_cell_release(text.out, NULL);
    if (!written)
    {
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, ": it could not be written");
    }
    return written;
}

const CliServeCall g_cli_window_home_calls[] = {
    {"state_write", home_state_write},
    {"state_read", home_state_read},
    {"state_now", home_state_now},
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
