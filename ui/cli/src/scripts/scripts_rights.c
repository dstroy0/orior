// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// scripts_rights.c: the rights of a script plugin (scripts.h). The plugin's plugin.json lists the patterns of the
// tree's files that it reads and writes, the hosts that it reaches, and the keys of the secrets that it keeps. A
// pattern reads as a line of a `.gitignore` reads (text/patterns.h). A host that starts with `*.` names each host under
// it, and a key `*` names every key. The reader answers once for each right of each plugin, and
// plugin_state/<id>.rights.json keeps the answers
#include "../command_line/pool.h"
#include "../plugins/plugins.h"
#include "../text/json_tree.h"
#include "../text/patterns.h"
#include "scripts_internal.h"

// the most items of one list of a plugin.json, and the most bytes of one item with its end
#define CLI_SCRIPTS_RIGHTS_ITEMS 64u
#define CLI_SCRIPTS_RIGHTS_ITEM 256u

// The names of the rights, as plugin.json and the store write them. Then the names of the answers in the store, each
// at the place of its value
static const char *const s_rights[CLI_SCRIPTS_RIGHTS] = {"reads", "writes", "reaches", "secrets"};
static const char *const s_answers[3] = {"unasked", "refused", "allowed"};

// One list of a plugin.json: the right that it is of, its items, each with its end, and how many it has
typedef struct
{
    unsigned int right;
    char item[CLI_SCRIPTS_RIGHTS_ITEMS][CLI_SCRIPTS_RIGHTS_ITEM];
    const char *line[CLI_SCRIPTS_RIGHTS_ITEMS];
    unsigned int items;
} ScriptsRightsList;

typedef struct
{
    const char *pattern;
    const char *name;
    const char *id;
    char *path_out;
    VerbaLinea *err_msg;
} ScriptsRightsCtx;

// the list of one right read from a plugin's kept settings, as cli_plugins_settings_of passes the plugin. An item past
// CLI_SCRIPTS_RIGHTS_ITEM bytes names nothing
static void rights_list_read(const CliEachPluginCfg *args)
{
    ScriptsRightsList *const list = (ScriptsRightsList *)args->context;
    const CliJson *const json = args->plugin->json;
    int more = 1;
    while (more && (list->items < CLI_SCRIPTS_RIGHTS_ITEMS))
    {
        char key[32];
        VerbaLinea keyed = {key, sizeof(key), 0ULL};
        mmgr_line_put(&keyed, s_rights[list->right]);
        mmgr_line_put(&keyed, ".");
        mmgr_line_decimal(&keyed, list->items, 1u);
        (void)mmgr_line_finish(&keyed);
        const unsigned int node = EMBED_CALL(cli_plugins_setting_table.setting, CliPluginsSettingCfg, .json = json,
                                             .node = args->plugin->root, .key = key);
        unsigned long long length = 0ULL;
        const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                            .json_read = json, .node = node, .length_pointer = &length);
        more = (text != NULL) && (length < CLI_SCRIPTS_RIGHTS_ITEM);
        if (more)
        {
            (void)memor.cpy(&(MemoriaCfg){.dst = list->item[list->items], .src = text, .bytes = (embed_word)length});
            list->item[list->items][length] = '\0';
            list->line[list->items] = list->item[list->items];
            list->items += 1u;
        }
    }
}

// whether the host pattern `pattern` names the host `name`, the case of each letter aside
EMBED_INLINE int rights_host(const ScriptsRightsCtx *args)
{
    const unsigned long long host = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_SCRIPTS_RIGHTS_ITEM});
    if (!cellul.starts(&(CatenaFinitaCfg){.src = args->pattern, .other = "*.", .cap = CLI_SCRIPTS_RIGHTS_ITEM}))
    {
        return cellul.eq(
            &(CatenaFinitaCfg){.src = args->name, .other = args->pattern, .cap = CLI_SCRIPTS_RIGHTS_ITEM, .ci = 1});
    }
    // the pattern's `.` and what follows it end the host, after one character of the host at least
    const char *const under = args->pattern + 1;
    const unsigned long long tail = cellul.len(&(CatenaFinitaCfg){.src = under, .cap = CLI_SCRIPTS_RIGHTS_ITEM});
    return (host > tail) &&
           cellul.eq(&(CatenaFinitaCfg){
               .src = args->name + (host - tail), .other = under, .cap = CLI_SCRIPTS_RIGHTS_ITEM, .ci = 1});
}

// whether `name` is a path inside the tree: no `/` at its start, no `\` or `:` in it, and no name `.` or `..`
EMBED_INLINE int rights_inside(const ScriptsRightsCtx *args)
{
    const char *const name = args->name;
    int inside = (name[0] != '\0') && (name[0] != '/');
    unsigned long long from = 0ULL;
    unsigned long long at = 0ULL;
    while (inside && (at < CLI_PATH_MAX_LEN))
    {
        const char letter = name[at];
        inside = (letter != '\\') && (letter != ':');
        if ((letter == '/') || (letter == '\0'))
        {
            const unsigned long long length = at - from;
            inside = inside && (length != 0ULL) && !((length == 1ULL) && (name[from] == '.')) &&
                     !((length == 2ULL) && (name[from] == '.') && (name[from + 1ULL] == '.'));
            from = at + 1ULL;
        }
        at = (letter == '\0') ? CLI_PATH_MAX_LEN : (at + 1ULL);
    }
    return inside;
}

int cli_scripts_rights_named(const CliScriptsRightsCfg *args)
{
    if ((args->right >= CLI_SCRIPTS_RIGHTS) || (args->name == NULL) || (args->id == NULL))
    {
        return 0;
    }
    ScriptsRightsList *const list =
        (ScriptsRightsList *)g_cli_guard->temporary_buf_alloc(sizeof(ScriptsRightsList), g_cli_scripts_owner);
    if (list == NULL)
    {
        return 0;
    }
    list->right = args->right;
    list->items = 0u;
    (void)EMBED_CALL(cli_plugins_settings_of_table.of, CliPluginsSettingsOfCfg, .id = args->id,
                     .each = rights_list_read, .context = list);
    int named = 0;
    if (args->right == CLI_SCRIPTS_REACHES)
    {
        unsigned int at = 0u;
        while (!named && (at < list->items))
        {
            named = EMBED_CALL(rights_host, ScriptsRightsCtx, .pattern = list->item[at], .name = args->name);
            at += 1u;
        }
    }
    else if (args->right == CLI_SCRIPTS_SECRETS)
    {
        unsigned int at = 0u;
        while (!named && (at < list->items))
        {
            named = cellul.eq(&(CatenaFinitaCfg){.src = list->item[at], .other = "*", .cap = 2u}) ||
                    cellul.eq(
                        &(CatenaFinitaCfg){.src = list->item[at], .other = args->name, .cap = CLI_SCRIPTS_RIGHTS_ITEM});
            at += 1u;
        }
    }
    else if ((list->items > 0u) && EMBED_CALL(rights_inside, ScriptsRightsCtx, .name = args->name))
    {
        CliPatterns *const patterns =
            (CliPatterns *)g_cli_guard->temporary_buf_alloc(sizeof(CliPatterns), g_cli_scripts_owner);
        if (patterns != NULL)
        {
            EMBED_CALL(cli_patterns_lines.read_set, CliPatternsLinesCfg, .lines = list->line, .count = list->items,
                       .patterns_out = patterns);
            // a set's read turns each pattern about: a path in the set is one that the patterns do not hide
            named = !EMBED_CALL(cli_patterns.empty, CliPatternsCfg, .patterns = patterns) &&
                    !EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = patterns, .path = args->name,
                                .dir = 0);
        }
        g_cli_guard->persistent_buf_release(patterns, g_cli_scripts_owner);
    }
    g_cli_guard->persistent_buf_release(list, g_cli_scripts_owner);
    return named;
}

// the path of the plugin's store into `path_out`, sized for CLI_PATH_MAX_LEN: 1, or 0 with why given
EMBED_INLINE int rights_store(const ScriptsRightsCtx *args)
{
    char name[CLI_NAME_MAX_LEN + 16u];
    VerbaLinea named = {name, sizeof(name), 0ULL};
    mmgr_line_put(&named, args->id);
    mmgr_line_put(&named, ".rights.json");
    return (mmgr_line_finish(&named) != 0ULL) &&
           EMBED_CALL(cli_scripts_internal.path, CliScriptsPathCtx, .leaf = "plugin_state", .name = name,
                      .path_out = args->path_out, .err_msg = args->err_msg);
}

unsigned int cli_scripts_rights_answer(const CliScriptsRightsCfg *args)
{
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    char path[CLI_PATH_MAX_LEN];
    if ((args->right >= CLI_SCRIPTS_RIGHTS) || (args->id == NULL) ||
        !EMBED_CALL(rights_store, ScriptsRightsCtx, .id = args->id, .path_out = path, .err_msg = &err_msg))
    {
        return CLI_SCRIPTS_UNASKED;
    }
    CliJson json;
    const unsigned int root =
        EMBED_CALL(cli_json_tree_err_msg.json_file_alloc, CliJsonTreeErrMsgCfg, .json = &json, .path = path,
                   .nodes_more = 0u, .text_more = 0ULL, .err_msg = &err_msg, .owner = g_cli_scripts_owner);
    const unsigned int node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                         .object = root, .name = s_rights[args->right]);
    const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                        .json_read = &json, .node = node, .length_pointer = NULL);
    unsigned int answer = CLI_SCRIPTS_UNASKED;
    unsigned int at = CLI_SCRIPTS_REFUSED;
    while ((text != NULL) && (answer == CLI_SCRIPTS_UNASKED) && (at <= CLI_SCRIPTS_ALLOWED))
    {
        answer = cellul.eq(&(CatenaFinitaCfg){.src = text, .other = s_answers[at], .cap = 16u}) ? at : answer;
        at += 1u;
    }
    if (root != CLI_JSON_NONE)
    {
        g_cli_guard->persistent_buf_release(json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), json.node));
    }
    return answer;
}

int cli_scripts_rights_answer_set(const CliScriptsRightsCfg *args)
{
    if ((args->right >= CLI_SCRIPTS_RIGHTS) || (args->answer > CLI_SCRIPTS_ALLOWED) || (args->id == NULL))
    {
        mmgr_line_put(args->err_msg, "no such right or answer");
        return 0;
    }
    unsigned int answers[CLI_SCRIPTS_RIGHTS];
    unsigned int right = 0u;
    while (right < CLI_SCRIPTS_RIGHTS)
    {
        answers[right] = (right == args->right) ? args->answer
                                                : EMBED_CALL(cli_scripts_rights.answer, CliScriptsRightsCfg,
                                                             .id = args->id, .right = right);
        right += 1u;
    }
    char text[128];
    VerbaLinea store = {text, sizeof(text), 0ULL};
    mmgr_line_put(&store, "{");
    right = 0u;
    while (right < CLI_SCRIPTS_RIGHTS)
    {
        mmgr_line_put(&store, (right == 0u) ? "\"" : ",\"");
        mmgr_line_put(&store, s_rights[right]);
        mmgr_line_put(&store, "\":\"");
        mmgr_line_put(&store, s_answers[answers[right]]);
        mmgr_line_put(&store, "\"");
        right += 1u;
    }
    mmgr_line_put(&store, "}\n");
    char path[CLI_PATH_MAX_LEN];
    if (!mmgr_line_ok(&store) ||
        !EMBED_CALL(rights_store, ScriptsRightsCtx, .id = args->id, .path_out = path, .err_msg = args->err_msg))
    {
        return 0;
    }
    if (!EMBED_CALL(cli_path_bytes_internal.private_file_write, CliPathBytesCtx, .path = path,
                    .bytes = (const unsigned char *)text, .size = store.at, .fresh = 1))
    {
        mmgr_line_put(args->err_msg, "the answer could not be written into orior's own folder");
        return 0;
    }
    return 1;
}
