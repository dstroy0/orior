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

// the bytes a text written for orior's folder is written in
#define CLI_HOME_TEXT_ROOM (6ull << 20u)

// The settings' chains and layers of one call, a cell of them taken for it
typedef struct
{
    CliChain reader;
    CliChain tree;
    CliLayers layers;
} CliHomeSettings;

// the lock a call on orior's folder answers under, one at a time, and the cells it occupies while it answers, each
// sized to what it contains

typedef struct
{
    VerbaLinea *line;
    VerbaLinea *err_msg;
    const CliServeArgs *request;
    const char *key;
    const char ***items;
    unsigned int *count;
    CliJson *json;
    const CliCarried *carried;
    VerbaLinea *out;
    const char *id;
    char *folder;
    char *path;
} WindowCallHomeCtx;
// a line begun for a text of orior's folder: 1, or 0 with why given where its cell is refused
EMBED_INLINE int home_line_begin(const WindowCallHomeCtx *args)
{
    VerbaLinea *line = args->line;
    char *const cell = g_cli_cells->temporary_buf_alloc(CLI_HOME_TEXT_ROOM, &g_cli_window_home_cells);
    *line = mmgr_line_over(cell, CLI_HOME_TEXT_ROOM);
    const int begun = cell != NULL;
    if (!begun)
    {
        mmgr_line_put(args->err_msg, "a text of orior's folder could not be given a cell");
    }
    return begun;
}

// the strings of the argument `key` into a cell of their count, `*items`, given back with
// g_cli_cells->persistent_buf_release(&g_cli_window_home_cells,
// ...) either way, their count into `count`: 1, or 0 with why given
EMBED_INLINE int home_strings(const WindowCallHomeCtx *args)
{
    const CliServeArgs *arguments = args->request;
    const char *key = args->key;
    const char ***items = args->items;
    unsigned int *count = args->count;
    VerbaLinea *err_msg = args->err_msg;
    const unsigned int node =
        EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = key);
    unsigned int most = 0u;
    const unsigned int first = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                          .json_read = arguments->json, .node = node, .kind = CFG_JSON_ARRAY)
                                   ? arguments->json->node[node].first
                                   : CLI_JSON_NONE;
    for (unsigned int item = first; item != CLI_JSON_NONE; item = arguments->json->node[item].next)
    {
        most += 1u;
    }
    *count = 0u;
    *items = g_cli_cells->temporary_buf_alloc((most + 1ull) * sizeof(**items), &g_cli_window_home_cells);
    if (*items == NULL)
    {
        mmgr_line_put(err_msg, "the strings a call on orior's folder names could not be given a cell");
        return 0;
    }
    return EMBED_CALL(cli_serve_err_msg_internal.strings, CliServeErrMsgCtx, .request = arguments, .key = key,
                      .items = *items, .room = most, .count = count, .err_msg = err_msg);
}

// the page's changed entries stored, each key starting `orior.` with its new text, or null where it is gone
static int home_state_write(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    (void)out;
    char folder[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder))
    {
        mmgr_line_put(err_msg, "orior_cli has no folder of its own to store its entries in");
        return 0;
    }
    const unsigned int changes =
        EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "changes");
    VerbaLinea state_line;
    if (!EMBED_CALL(home_line_begin, WindowCallHomeCtx, .line = &state_line, .err_msg = err_msg))
    {
        return 0;
    }
    mmgr_line_ch(&state_line, '{');
    const unsigned int first = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                          .json_read = arguments->json, .node = changes, .kind = CFG_JSON_OBJECT)
                                   ? arguments->json->node[changes].first
                                   : CLI_JSON_NONE;
    for (unsigned int member = first; member != CLI_JSON_NONE; member = arguments->json->node[member].next)
    {
        const char *const key = arguments->json->text + arguments->json->node[member].key;
        if (!cellul.starts(&(CatenaFinitaCfg){.src = key, .other = "orior.", .cap = CLI_CELLS_POOL}))
        {
            continue;
        }
        mmgr_line_put(&state_line, (state_line.at > 1ull) ? "," : "");
        EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = &state_line, .text = key + 6);
        mmgr_line_ch(&state_line, ':');
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = arguments->json,
                   .node = member, .out = &state_line, .indent = 0u, .sorted = 0);
    }
    mmgr_line_ch(&state_line, '}');
    const int done =
        mmgr_line_ok(&state_line) && EMBED_CALL(cli_state_folder.save, CliStateFolderCfg, .folder = folder,
                                                .changes = state_line.out, .length = state_line.at, .err_msg = err_msg);
    g_cli_cells->persistent_buf_release(state_line.out, &g_cli_window_home_cells);
    return done;
}

// what orior's folder stores read into `json`, a tree in a cell sized to it, given back with cli_json_free either
// way, under the lock: its object, or CLI_JSON_NONE where it stores nothing
EMBED_INLINE unsigned int home_state_tree(const WindowCallHomeCtx *args)
{
    CliJson *json = args->json;
    char folder[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = json, .node_json_node_pointer = NULL,
               .nodes_most = 0u, .text_out = NULL, .text_most = 0ull);
    VerbaLinea state_line;
    char why[256];
    VerbaLinea ignored = {why, sizeof(why), 0ull};
    if (!EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder) ||
        !EMBED_CALL(home_line_begin, WindowCallHomeCtx, .line = &state_line, .err_msg = &ignored))
    {
        return CLI_JSON_NONE;
    }
    const unsigned int tree =
        (EMBED_CALL(cli_state_out.read, CliStateOutCfg, .folder = folder, .out = &state_line) &&
         mmgr_line_ok(&state_line))
            ? EMBED_CALL(cli_json_tree_text_size_err_msg.json_take_temporary, CliJsonTreeTextSizeErrMsgCfg,
                         .json = json, .text = state_line.out, .size = state_line.at, .nodes_more = 0u,
                         .text_more = 0ull, .err_msg = &ignored, .owner = &g_cli_window_home_cells)
            : CLI_JSON_NONE;
    g_cli_cells->persistent_buf_release(state_line.out, &g_cli_window_home_cells);
    return EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json, .node = tree,
                      .kind = CFG_JSON_OBJECT)
               ? tree
               : CLI_JSON_NONE;
}

// what orior's folder stores, each key with `orior.` before it, or null where it stores nothing yet
static int home_state_read(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    CliJson json;
    const unsigned int tree = EMBED_CALL(home_state_tree, WindowCallHomeCtx, .json = &json);
    if (tree == CLI_JSON_NONE)
    {
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
        mmgr_line_put(out, "null");
        return 1;
    }
    mmgr_line_ch(out, '{');
    for (unsigned int member = json.node[tree].first; member != CLI_JSON_NONE; member = json.node[member].next)
    {
        mmgr_line_put(out, (member != json.node[tree].first) ? ",\"orior." : "\"orior.");
        EMBED_CALL(cli_json_tree_length_out.json_escaped_lossy, CliJsonTreeLengthOutCfg, .out = out,
                   .text_bytes = (const unsigned char *)(json.text + json.node[member].key),
                   .length =
                       cellul.len(&(CatenaFinitaCfg){.src = json.text + json.node[member].key, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, "\":");
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &json, .node = member,
                   .out = out, .indent = 0u, .sorted = 0);
    }
    mmgr_line_ch(out, '}');
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    return 1;
}

// the bytes the page's first script is written in
#define CLI_WINDOW_SAVED_SCRIPT_ROOM (16ull << 20u)

// the page's first script before what orior's folder stores and after it
static const char s_saved_before[] =
    "(() => { if (location.protocol === \"view:\" || location.hostname === \"view.localhost\") return; "
    "if (sessionStorage.getItem(\"orior.kept.read\")) return; sessionStorage.setItem(\"orior.kept.read\", \"1\"); "
    "const kept = ";
static const char s_saved_after[] =
    "; if (kept === null) { window.oriorKeepAll = true; return; } "
    "for (const key of Object.keys(localStorage)) { if (key.startsWith(\"orior.\") && !(key in kept)) "
    "localStorage.removeItem(key); } "
    "for (const [key, text] of Object.entries(kept)) localStorage.setItem(key, text); })();";

// the page's first script, made once
static VerbaLinea s_saved_script;

const char *cli_window_kept_script(unsigned long long *size)
{
    *size = 0ull;
    char *const script_cell = g_cli_cells->temporary_buf_alloc(CLI_WINDOW_SAVED_SCRIPT_ROOM, &g_cli_window_home_cells);
    if (script_cell == NULL)
    {
        return NULL;
    }
    s_saved_script = (VerbaLinea){script_cell, CLI_WINDOW_SAVED_SCRIPT_ROOM, 0ull};
    mmgr_line_put(&s_saved_script, s_saved_before);
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    (void)EMBED_CALL(home_state_read, CliServeAnswerCfg, .request = NULL, .out = &s_saved_script, .err_msg = &err_msg);
    mmgr_line_put(&s_saved_script, s_saved_after);
    *size = mmgr_line_ok(&s_saved_script) ? s_saved_script.at : 0ull;
    return s_saved_script.out;
}

void cli_window_kept_script_given(void)
{
    g_cli_cells->persistent_buf_release(s_saved_script.out, &g_cli_window_home_cells);
    s_saved_script = (VerbaLinea){NULL, 0ull, 0ull};
}

int cli_window_title_bar_system(void)
{
    CliJson json;
    const unsigned int tree = EMBED_CALL(home_state_tree, WindowCallHomeCtx, .json = &json);
    const char *const title_bar =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                      .object = tree, .name = "titlebar"),
                   .length_pointer = NULL);
    const int system = (title_bar != NULL) &&
                       cellul.eq(&(CatenaFinitaCfg){.src = title_bar, .other = "system", .cap = CLI_CELLS_POOL});
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    return system;
}

// the settings and themes orior's folder stores now, by key without `orior.`, and each of the page's keys that is a
// setting the folder no longer stores
static int home_state_now(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    unsigned int count = 0u;
    const char **keys = NULL;
    if (!EMBED_CALL(home_strings, WindowCallHomeCtx, .request = arguments, .key = "keys", .items = &keys,
                    .count = &count, .err_msg = err_msg))
    {
        g_cli_cells->persistent_buf_release(keys, &g_cli_window_home_cells);
        return 0;
    }
    CliJson json;
    const unsigned int tree = EMBED_CALL(home_state_tree, WindowCallHomeCtx, .json = &json);
    const unsigned int first = (tree != CLI_JSON_NONE) ? json.node[tree].first : CLI_JSON_NONE;
    mmgr_line_put(out, "{\"set\":{");
    int any = 0;
    for (unsigned int member = first; member != CLI_JSON_NONE; member = json.node[member].next)
    {
        const char *const key = json.text + json.node[member].key;
        if (EMBED_CALL(cli_state_table.is_carry, CliStateCfg, .key = key))
        {
            mmgr_line_put(out, any ? "," : "");
            EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out, .text = key);
            mmgr_line_ch(out, ':');
            EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &json, .node = member,
                       .out = out, .indent = 0u, .sorted = 0);
            any = 1;
        }
    }
    mmgr_line_put(out, "},\"gone\":[");
    any = 0;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const key = keys[at] + 6;
        if ((cellul.starts(&(CatenaFinitaCfg){.src = keys[at], .other = "orior.", .cap = CLI_CELLS_POOL})) &&
            EMBED_CALL(cli_state_table.is_carry, CliStateCfg, .key = key) &&
            (!cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "themes", .cap = CLI_CELLS_POOL})) &&
            (EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json, .object = tree,
                        .name = key) == CLI_JSON_NONE))
        {
            mmgr_line_put(out, any ? "," : "");
            EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out, .text = key);
            any = 1;
        }
    }
    mmgr_line_put(out, "]}");
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    g_cli_cells->persistent_buf_release(keys, &g_cli_window_home_cells);
    return 1;
}

// the settings in force for the tree at `root`, from the reader's settings and the tree's, `tree`
static int home_settings_layers(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    const char *const root = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "root", .err_msg = err_msg);
    VerbaLinea tree;
    if ((root == NULL) || !EMBED_CALL(home_line_begin, WindowCallHomeCtx, .line = &tree, .err_msg = err_msg))
    {
        return 0;
    }
    CliHomeSettings *const settings = g_cli_cells->temporary_buf_alloc(sizeof(*settings), &g_cli_window_home_cells);
    if (settings == NULL)
    {
        g_cli_cells->persistent_buf_release(tree.out, &g_cli_window_home_cells);
        mmgr_line_put(err_msg, "the settings' chains and layers could not be given a cell");
        return 0;
    }
    EMBED_CALL(cli_window_out_request_internal.arg_write, CliWindowOutRequestCtx, .request = arguments, .key = "tree",
               .out = &tree);
    const int read =
        mmgr_line_ok(&tree) && EMBED_CALL(cli_settings_chain_table.chain_read, CliSettingsChainCfg, .text = tree.out,
                                          .length = tree.at, .chain = &settings->tree, .err_msg = err_msg);
    if (read)
    {
        EMBED_CALL(cli_settings_path_table.reader, CliSettingsPathCfg, .chain = &settings->reader);
        EMBED_CALL(cli_settings_root.layers, CliSettingsRootCfg, .reader = &settings->reader, .tree = &settings->tree,
                   .root = root, .layers = &settings->layers);
        EMBED_CALL(cli_settings_out.layers_write, CliSettingsOutCfg, .layers_read = &settings->layers, .out = out);
        EMBED_CALL(cli_settings_root.layers_free, CliSettingsRootCfg, .layers = &settings->layers);
        EMBED_CALL(cli_settings_path_table.chain_free, CliSettingsPathCfg, .chain = &settings->reader);
    }
    EMBED_CALL(cli_settings_path_table.chain_free, CliSettingsPathCfg, .chain = &settings->tree);
    g_cli_cells->persistent_buf_release(settings, &g_cli_window_home_cells);
    g_cli_cells->persistent_buf_release(tree.out, &g_cli_window_home_cells);
    return read;
}

// the carried files written as an object of each place and its text
EMBED_INLINE void home_carried_write(const WindowCallHomeCtx *args)
{
    const CliCarried *carried = args->carried;
    VerbaLinea *out = args->out;
    mmgr_line_ch(out, '{');
    for (unsigned int at = 0u; at < carried->count; at += 1u)
    {
        mmgr_line_put(out, (at > 0u) ? "," : "");
        EMBED_CALL(cli_window_out_text_internal.quote, CliWindowOutTextCtx, .out = out,
                   .text = EMBED_CALL(cli_carry_carried_at_internal.carried_place, CliCarryCarriedAtCtx,
                                      .carried = carried, .at = at));
        mmgr_line_ch(out, ':');
        EMBED_CALL(cli_window_out_text_internal.count, CliWindowOutTextCtx, .out = out,
                   .text = EMBED_CALL(cli_carry_carried_at_internal.carried_value, CliCarryCarriedAtCtx,
                                      .carried = carried, .at = at),
                   .size = carried->size[at]);
    }
    mmgr_line_ch(out, '}');
}

// the reader's settings that travel written to `to`, a file or a folder of the reader's
static int home_carry_send(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char home[CLI_PATH_MAX_LEN];
    const char *const to = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                      .key = "to", .err_msg = err_msg);
    VerbaLinea done;
    if ((to == NULL) || !EMBED_CALL(cli_window_internal.home, CliWindowCtx, .folder = home, .err_msg = err_msg) ||
        !EMBED_CALL(home_line_begin, WindowCallHomeCtx, .line = &done, .err_msg = err_msg))
    {
        return 0;
    }
    const int sent =
        EMBED_CALL(cli_carry_home_to.send, CliCarryHomeToCfg, .home = home, .to = to, .out = &done, .err_msg = err_msg);
    if (sent)
    {
        EMBED_CALL(cli_window_out_text_internal.count, CliWindowOutTextCtx, .out = out, .text = done.out,
                   .size = mmgr_line_ok(&done) ? done.at : 0ull);
    }
    g_cli_cells->persistent_buf_release(done.out, &g_cli_window_home_cells);
    return sent;
}

// what the file or folder `from` carries, and how it differs from the reader's own here
static int home_carry_read(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char home[CLI_PATH_MAX_LEN];
    const char *const from = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "from", .err_msg = err_msg);
    if ((from == NULL) || !EMBED_CALL(cli_window_internal.home, CliWindowCtx, .folder = home, .err_msg = err_msg))
    {
        return 0;
    }
    CliCarried *const carried = g_cli_cells->temporary_buf_alloc(sizeof(*carried), &g_cli_window_home_cells);
    if (carried == NULL)
    {
        mmgr_line_put(err_msg, "the files a carry contains could not be given a cell");
        return 0;
    }
    carried->count = 0u;
    const int read =
        EMBED_CALL(cli_carry_from.read, CliCarryFromCfg, .from = from, .there_out = carried, .err_msg = err_msg);
    if (read)
    {
        mmgr_line_put(out, "{\"there\":");
        EMBED_CALL(home_carried_write, WindowCallHomeCtx, .carried = carried, .out = out);
        mmgr_line_put(out, ",\"differences\":");
        EMBED_CALL(cli_carry_home.differences, CliCarryHomeCfg, .home = home, .there = carried, .out = out);
        mmgr_line_ch(out, '}');
    }
    EMBED_CALL(cli_carry_carried_place_internal.carried_clear, CliCarryCarriedPlaceCtx, .carried_out = carried);
    g_cli_cells->persistent_buf_release(carried, &g_cli_window_home_cells);
    return read;
}

// the files of `places` that `there`, read from `from`, carries taken in, and of settings.json the settings of `keys`
static int home_carry_take(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    unsigned int places = 0u;
    unsigned int keys = 0u;
    char home[CLI_PATH_MAX_LEN];
    const char *const from = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                        .key = "from", .err_msg = err_msg);
    if ((from == NULL) || !EMBED_CALL(cli_window_internal.home, CliWindowCtx, .folder = home, .err_msg = err_msg))
    {
        return 0;
    }
    const char **place_of = NULL;
    const char **key_of = NULL;
    CliCarried *const carried = g_cli_cells->temporary_buf_alloc(sizeof(*carried), &g_cli_window_home_cells);
    if ((carried == NULL) ||
        !EMBED_CALL(home_strings, WindowCallHomeCtx, .request = arguments, .key = "places", .items = &place_of,
                    .count = &places, .err_msg = err_msg) ||
        !EMBED_CALL(home_strings, WindowCallHomeCtx, .request = arguments, .key = "keys", .items = &key_of,
                    .count = &keys, .err_msg = err_msg))
    {
        if (carried == NULL)
        {
            mmgr_line_put(err_msg, "the files a carry contains could not be given a cell");
        }
        g_cli_cells->persistent_buf_release(key_of, &g_cli_window_home_cells);
        g_cli_cells->persistent_buf_release(place_of, &g_cli_window_home_cells);
        g_cli_cells->persistent_buf_release(carried, &g_cli_window_home_cells);
        return 0;
    }
    carried->count = 0u;
    const unsigned int there =
        EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "there");
    const unsigned int first = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                          .json_read = arguments->json, .node = there, .kind = CFG_JSON_OBJECT)
                                   ? arguments->json->node[there].first
                                   : CLI_JSON_NONE;
    for (unsigned int member = first; member != CLI_JSON_NONE; member = arguments->json->node[member].next)
    {
        unsigned long long length = 0ull;
        const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                            .json_read = arguments->json, .node = member, .length_pointer = &length);
        if (text != NULL)
        {
            (void)EMBED_CALL(cli_carry_carried_place_internal.carried_put, CliCarryCarriedPlaceCtx,
                             .carried_out = carried, .place = arguments->json->text + arguments->json->node[member].key,
                             .value = text, .size = length);
        }
    }
    const int taken = EMBED_CALL(cli_carry_home.take, CliCarryHomeCfg, .home = home, .from = from, .there = carried,
                                 .places = place_of, .count = places, .keys = key_of, .key_count = keys, .out = out,
                                 .err_msg = err_msg);
    EMBED_CALL(cli_carry_carried_place_internal.carried_clear, CliCarryCarriedPlaceCtx, .carried_out = carried);
    g_cli_cells->persistent_buf_release(key_of, &g_cli_window_home_cells);
    g_cli_cells->persistent_buf_release(place_of, &g_cli_window_home_cells);
    g_cli_cells->persistent_buf_release(carried, &g_cli_window_home_cells);
    return taken;
}

// every taking saved, the last first
static int home_carry_takings(const CliServeAnswerCfg *args)
{
    VerbaLinea *out = args->out;
    char home[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = home))
    {
        mmgr_line_put(out, "[]");
        return 1;
    }
    EMBED_CALL(cli_carry_home_to.takings, CliCarryHomeToCfg, .home = home, .out = out);
    return 1;
}

// the taking `id` undone
static int home_carry_undo(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char home[CLI_PATH_MAX_LEN];
    const char *const id = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                      .key = "id", .err_msg = err_msg);
    return (id != NULL) && EMBED_CALL(cli_window_internal.home, CliWindowCtx, .folder = home, .err_msg = err_msg) &&
           EMBED_CALL(cli_carry_home_out.undo, CliCarryHomeOutCfg, .home = home, .id = id, .out = out,
                      .err_msg = err_msg);
}

// the file of the plugin `id`'s settings into `path`: 1, or 0 with why given
EMBED_INLINE int home_plugin_settings_path(const WindowCallHomeCtx *args)
{
    const char *id = args->id;
    char *folder = args->folder;
    VerbaLinea *err_msg = args->err_msg;
    char home[CLI_PATH_MAX_LEN];
    char slug[CLI_NAME_MAX_LEN];
    if (!EMBED_CALL(cli_window_internal.home, CliWindowCtx, .folder = home, .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = home, .relative = "plugin_settings", .joined = folder);
    EMBED_CALL(cli_plugins_id_name.slug, CliPluginsIdNameCfg, .name = id, .id_out = slug);
    VerbaLinea line = {args->path, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, slug);
    mmgr_line_put(&line, ".json");
    return mmgr_line_finish(&line) != 0ull;
}

// the settings of the plugin `id`, as it last wrote them, or an empty object
static int home_plugin_settings_read(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    char folder[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    const char *const id = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                      .key = "id", .err_msg = err_msg);
    if ((id == NULL) || !EMBED_CALL(home_plugin_settings_path, WindowCallHomeCtx, .id = id, .folder = folder,
                                    .path = path, .err_msg = err_msg))
    {
        return 0;
    }
    unsigned long long size = 0ull;
    const unsigned char *const mapped = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = path,
                                                   .size_pointer = &size, .owner = &g_cli_window_home_cells);
    const unsigned long long start = out->at;
    if ((mapped == NULL) || !EMBED_CALL(cli_json_tree_text.json_compact, CliJsonTreeTextCfg,
                                        .text = (const char *)mapped, .size = size, .out = out))
    {
        out->at = start;
        mmgr_line_put(out, "{}");
    }
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = mapped,
               .owner = &g_cli_window_home_cells);
    return 1;
}

// `values` stored as the settings of the plugin `id`, in place of those it stored before
static int home_plugin_settings_write(const CliServeAnswerCfg *args)
{
    const CliServeArgs *arguments = args->request;
    VerbaLinea *out = args->out;
    VerbaLinea *err_msg = args->err_msg;
    (void)out;
    const char *const id = EMBED_CALL(cli_serve_err_msg_internal.string, CliServeErrMsgCtx, .request = arguments,
                                      .key = "id", .err_msg = err_msg);
    const unsigned int values =
        EMBED_CALL(cli_serve_err_msg_internal.arg, CliServeErrMsgCtx, .request = arguments, .key = "values");
    if (id == NULL)
    {
        return 0;
    }
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = arguments->json,
                    .node = values, .kind = CFG_JSON_OBJECT))
    {
        mmgr_line_put(err_msg, "a plugin's settings are an object");
        return 0;
    }
    char folder[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(home_plugin_settings_path, WindowCallHomeCtx, .id = id, .folder = folder, .path = path,
                    .err_msg = err_msg))
    {
        return 0;
    }
    if (!EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder))
    {
        mmgr_line_put(err_msg, folder);
        mmgr_line_put(err_msg, ": it could not be made");
        return 0;
    }
    VerbaLinea text;
    if (!EMBED_CALL(home_line_begin, WindowCallHomeCtx, .line = &text, .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = arguments->json, .node = values,
               .out = &text, .indent = 2u, .sorted = 0);
    mmgr_line_ch(&text, '\n');
    const int written =
        mmgr_line_ok(&text) && EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx,
                                          .path = path, .bytes = text.out, .size = text.at);
    g_cli_cells->persistent_buf_release(text.out, &g_cli_window_home_cells);
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
