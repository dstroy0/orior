// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// settings.c: a settings file's own keys, the settings a tree's file does not set, `${name}` read, and a chain of
// files each taking the next, read and written as the window reads it
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../deploy/devcontainer.h"
#include "../text/text.h"
#include "settings_internal.h"

// A file of a chain as the chain is walked: the path it is shown by, its text with its comments taken out, in a cell
// no larger than the file, that text's bytes, and the nodes it is read into, its tokens and one over
typedef struct
{
    char named[CLI_PATH_MAX_LEN];
    char *plain;
    unsigned long long size;
    unsigned int nodes;
} CliChainFile;
// A chain's walk, a cell taken as the walk begins and given back as it ends: each file walked in turn, and the full
// path of each, by which a file taken twice is known
typedef struct
{
    CliChainFile file[CLI_SETTINGS_DEEPEST];
    unsigned int count;
    char seen[CLI_SETTINGS_DEEPEST][CLI_PATH_MAX_LEN];
} CliChainWalk;
// the keys of a settings file's own
static const char s_allocs[] = "takes";

static const char s_machine[] = "machine";

static const char *const s_systems[3] = {"windows", "linux", "macos"};

// the settings a tree's file does not set, each a key or the start of keys where it ends in `.`: how the programs a
// build makes are signed, the environment a tree's runs take, the checkers run as files change, the window's title
// bar, whether orior_cli is blocked from the network, what each plugin may read, write and reach, and the cores
// reserved for the reader. Each runs a program, names the reader's own, lets orior_cli or a plugin reach out or takes
// the reader's machine, and a tree cloned from anywhere would set it
static const char *const s_tree_scoped_keys[7] = {"signing.", "environment.",    "checkers",      "titlebar",
                                                  "network",  "plugins.allowed", "cores-reserved"};

// the most bytes of a settings file read
#define CLI_SETTINGS_FILE (1u << 20u)

const char *cli_settings_system(void)
{
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#else
    return "linux";
#endif
}

int cli_settings_kind_get(const CliSettingsKeyCtx *args)
{
    const char **kind = args->kind;
    unsigned long long *length = args->length_pointer;
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_CELLS_POOL});
    if ((size < 3ULL) || (args->key[0] != '[') || (args->key[size - 1ULL] != ']'))
    {
        return 0;
    }
    *kind = args->key + 1;
    *length = size - 2ULL;
    return 1;
}

int cli_settings_is_file_key(const CliSettingsKeyCfg *args)
{
    const char *kind = NULL;
    unsigned long long length = 0ULL;
    int found = (cellul.eq(&(CatenaFinitaCfg){.src = args->key, .other = s_allocs, .cap = CLI_CELLS_POOL})) ||
                (cellul.eq(&(CatenaFinitaCfg){.src = args->key, .other = s_machine, .cap = CLI_CELLS_POOL})) ||
                EMBED_CALL(cli_settings_key_internal.kind_get, CliSettingsKeyCtx, .key = args->key, .kind = &kind,
                           .length_pointer = &length);
    unsigned int at = 0u;
    while (!found && (at < 3u))
    {
        found = cellul.eq(&(CatenaFinitaCfg){.src = args->key, .other = s_systems[at], .cap = CLI_CELLS_POOL});
        at += 1u;
    }
    return found;
}

int cli_settings_tree_scope(const CliSettingsKeyCtx *args)
{
    for (unsigned int at = 0u; at < 7u; at += 1u)
    {
        const unsigned long long length =
            cellul.len(&(CatenaFinitaCfg){.src = s_tree_scoped_keys[at], .cap = CLI_CELLS_POOL});
        if ((cellul.eq(&(CatenaFinitaCfg){.src = args->key, .other = s_tree_scoped_keys[at], .cap = CLI_CELLS_POOL})) ||
            ((s_tree_scoped_keys[at][length - 1ULL] == '.') &&
             EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->key,
                        .other_pointer = s_tree_scoped_keys[at], .size = length)))
        {
            return 1;
        }
    }
    return 0;
}

void cli_settings_home(const CliSettingsCtx *args)
{
#if defined(_WIN32)
    static const char s_home[] = "USERPROFILE";
#else
    static const char s_home[] = "HOME";
#endif
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = s_home,
                    .value_out = args->out, .capacity = CLI_PATH_MAX_LEN))
    {
        args->out[0] = '\0';
    }
}

void cli_settings_expand(const CliSettingsTextCtx *args)
{
    const char *text = args->text;
    unsigned long long at = 0ULL;
    while (at < args->length)
    {
        const unsigned long long open = EMBED_CALL(cli_text_at_size_first.to_pair, CliTextAtSizeFirstCfg, .text = text,
                                                   .at = at, .size = args->length, .first = '$', .second = '{');
        if ((open + 1ULL) >= args->length)
        {
            break;
        }
        mmgr_line_put_n(args->out_verba_linea_pointer, text + at, open - at);
        unsigned long long close = open + 2ULL;
        close = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = text, .at = close,
                           .size = args->length, .set = "}");
        if (close >= args->length)
        {
            mmgr_line_put_n(args->out_verba_linea_pointer, text + open, args->length - open);
            return;
        }
        const unsigned long long mark = args->out_verba_linea_pointer->at;
        if (!EMBED_CALL(args->look, CliEachNameLengthOutCfg, .context = args->context, .name = text + open + 2ULL,
                        .length = close - open - 2ULL, .out = args->out_verba_linea_pointer))
        {
            args->out_verba_linea_pointer->at = mark;
            mmgr_line_put_n(args->out_verba_linea_pointer, text + open, close + 1ULL - open);
        }
        at = close + 1ULL;
    }
    mmgr_line_put_n(args->out_verba_linea_pointer, text + at, args->length - at);
}

typedef struct
{
    const char *path;
    char *out;
    CliChain *chain;
    const char *named;
    const char *detail;
    const char *root;
    const CliChainWalk *walk;
    CliChainWalk *walking;
    char *next;
    int *first;
    const char *text;
    unsigned long long length;
} SettingsCtx;
// `path` with each `.` taken out and each `..` taking out the folder before it, into `out`, sized for
// CLI_PATH_MAX_LEN
static void cli_tidy(const SettingsCtx *args)
{
    char parts[64][CLI_NAME_MAX_LEN];
    unsigned int count = 0u;
    // the root the path starts at: a drive and its slash, or a slash
    unsigned long long root = 0ULL;
    if ((args->path[0] != '\0') && (args->path[1] == ':'))
    {
        root = ((args->path[2] == '/') || (args->path[2] == '\\')) ? 3ULL : 2ULL;
    }
    else if ((args->path[0] == '/') || (args->path[0] == '\\'))
    {
        root = 1ULL;
    }
    for (const char *at = args->path + root; at[0] != '\0';)
    {
        const char *stop = at;
        stop += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = stop, .at = 0ULL,
                           .size = cellul.len(&(CatenaFinitaCfg){.src = stop, .cap = CLI_CELLS_POOL}), .set = "/\\");
        const unsigned long long length = (unsigned long long)(stop - at);
        const int here = (length == 1ULL) && (at[0] == '.');
        const int up = (length == 2ULL) && (at[0] == '.') && (at[1] == '.');
        if (up && (count > 0u) &&
            (!cellul.eq(&(CatenaFinitaCfg){.src = parts[count - 1u], .other = "..", .cap = CLI_CELLS_POOL})))
        {
            count -= 1u;
        }
        else if ((length != 0ULL) && !here && (count < 64u))
        {
            VerbaLinea part = {parts[count], CLI_NAME_MAX_LEN, 0ULL};
            mmgr_line_put_n(&part, at, length);
            mmgr_line_finish(&part);
            count += 1u;
        }
        at = (stop[0] != '\0') ? (stop + 1) : stop;
    }
    VerbaLinea line = {args->out, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put_n(&line, args->path, root);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_put(&line, (at != 0u) ? "/" : "");
        mmgr_line_put(&line, parts[at]);
    }
    mmgr_line_finish(&line);
}

// what a file's `allocs` names: the reader's home folder, or a variable of the system's environment
static int cli_alloc_look(const CliEachNameLengthOutCfg *args)
{
    const char *name = args->name;
    VerbaLinea *out = args->out;
    char value[CLI_PATH_MAX_LEN];
    if ((args->length == 4ULL) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "home", .bytes = 4u}) == 0))
    {
        EMBED_CALL(cli_settings_internal.home, CliSettingsCtx, .out = value);
        mmgr_line_put(out, value);
        return value[0] != '\0';
    }
    if ((args->length > 4ULL) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "env:", .bytes = 4u}) == 0))
    {
        char variable[CLI_NAME_MAX_LEN];
        VerbaLinea line = {variable, sizeof(variable), 0ULL};
        mmgr_line_put_n(&line, name + 4, args->length - 4ULL);
        mmgr_line_finish(&line);
        if (EMBED_CALL(query_name_out_internal.environment_write, QueryNameOutCtx, .name = variable, .out = out))
        {
            return 1;
        }
    }
    return 0;
}

// a problem put after the chain's
EMBED_INLINE void cli_problem(const SettingsCtx *args)
{
    if (args->chain->problems < CLI_SETTINGS_PROBLEMS)
    {
        VerbaLinea line = {args->chain->problem[args->chain->problems], CLI_PATH_MAX_LEN * 2u, 0ULL};
        mmgr_line_put(&line, args->named);
        mmgr_line_put(&line, args->detail);
        mmgr_line_finish(&line);
        args->chain->problems += 1u;
    }
}

// a file's path as a chain shows it: from `root` where it is under it, its slashes leaning forward, else as it is
EMBED_INLINE void cli_show(const SettingsCtx *args)
{
    char *out = args->out;
    const unsigned long long length =
        (args->root != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_CELLS_POOL}) : 0ULL;
    const int under = (args->root != NULL) &&
                      EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->path,
                                 .other_pointer = args->root, .size = length) &&
                      ((args->path[length] == '/') || (args->path[length] == '\\') || (args->path[length] == '\0'));
    const char *from = under ? (args->path + length) : args->path;
    from += (under && ((from[0] == '/') || (from[0] == '\\'))) ? 1 : 0;
    VerbaLinea line = {out, CLI_PATH_MAX_LEN, 0ULL};
    if (under)
    {
        (void)EMBED_CALL(cli_text_from.append_swap, CliTextFromCfg, .out = &line, .text = from, .from = '\\',
                         .to = '/');
    }
    else
    {
        mmgr_line_put(&line, from);
    }
    mmgr_line_finish(&line);
}

// the files walked read into the chain's tree in the order walked, the tree in a cell of their nodes and bytes, the
// tokens in a cell of the most one file contains; a problem where the cells are refused
EMBED_INLINE void cli_chain_read(const SettingsCtx *args)
{
    unsigned int nodes = 0u;
    unsigned int most = 0u;
    unsigned long long text = 0ULL;
    for (unsigned int at = 0u; at < args->walk->count; at += 1u)
    {
        nodes += args->walk->file[at].nodes;
        most = (args->walk->file[at].nodes > most) ? args->walk->file[at].nodes : most;
        text += args->walk->file[at].size;
    }
    if (args->walk->count == 0u)
    {
        return;
    }
    CfgJsonToken *const tokens =
        g_cli_cells->temporary_buf_alloc((unsigned long long)most * sizeof(CfgJsonToken), &g_cli_settings_cells);
    if ((tokens == NULL) || !EMBED_CALL(cli_json_tree_nodes_max.json_reserve, CliJsonTreeNodesMaxCfg,
                                        .json = &args->chain->json, .nodes_max = nodes, .text_max = text,
                                        .what = "a chain of settings files", .owner = &g_cli_settings_cells))
    {
        g_cli_cells->persistent_buf_release(tokens, &g_cli_settings_cells);
        EMBED_CALL(cli_problem, SettingsCtx, .chain = args->chain, .named = args->walk->file[0].named,
                   .detail = ": the cells its chain is read into could not be taken");
        return;
    }
    for (unsigned int at = 0u; at < args->walk->count; at += 1u)
    {
        const CliChainFile *const file = &args->walk->file[at];
        char why[CLI_PATH_MAX_LEN];
        VerbaLinea reason = {why, sizeof(why), 0ULL};
        const unsigned int values = EMBED_CALL(cli_json_tree_text_size_tokens.json_read, CliJsonTreeTextSizeTokensCfg,
                                               .json = &args->chain->json, .text = file->plain, .size = file->size,
                                               .tokens = tokens, .tokens_max = most, .err_msg = &reason);
        if (values == CLI_JSON_NONE)
        {
            break;
        }
        memor.cpy(
            &(MemoriaCfg){.dst = args->chain->file[args->chain->files],
                          .src = file->named,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = file->named, .cap = CLI_CELLS_POOL}) + 1u});
        args->chain->values[args->chain->files] = values;
        args->chain->files += 1u;
    }
    g_cli_cells->persistent_buf_release(tokens, &g_cli_settings_cells);
}

// whether the file at the whole `path` is among those the `walk` has walked
EMBED_INLINE int cli_walked_twice(const SettingsCtx *args)
{
    int twice = 0;
    for (unsigned int at = 0u; !twice && (at < args->walk->count); at += 1u)
    {
        twice = cellul.eq(&(CatenaFinitaCfg){.src = args->walk->seen[at], .other = args->path, .cap = CLI_CELLS_POOL});
    }
    return twice;
}

// the file a chain's file at `path` takes, `length` bytes of `text` with their names expanded and joined to the file's
// folder where they name no whole path, tidied into `out`
EMBED_INLINE void cli_takes_followed(const SettingsCtx *args)
{
    char expanded[CLI_PATH_MAX_LEN];
    VerbaLinea line = {expanded, sizeof(expanded), 0ULL};
    EMBED_CALL(cli_settings_text_internal.expand, CliSettingsTextCtx, .text = args->text, .length = args->length,
               .look = cli_alloc_look, .context = NULL, .out_verba_linea_pointer = &line);
    mmgr_line_finish(&line);
    const int absolute =
        (expanded[0] == '/') || (expanded[0] == '\\') || ((expanded[0] != '\0') && (expanded[1] == ':'));
    char folder[CLI_PATH_MAX_LEN];
    char joined[CLI_PATH_MAX_LEN];
    VerbaLinea join = {joined, sizeof(joined), 0ULL};
    if (!absolute &&
        EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = args->path, .out = folder))
    {
        mmgr_line_put(&join, folder);
        mmgr_line_ch(&join, '/');
    }
    mmgr_line_put(&join, expanded);
    mmgr_line_finish(&join);
    EMBED_CALL(cli_tidy, SettingsCtx, .path = joined, .out = args->out);
}

// one file of a chain walked: the file at `next`, shown from `root`, read with its comments taken out and put on
// `walking` where it is an object of settings, and `next` put as the file it takes; a problem told to `chain` where it
// cannot be, the `first` file let be missing: 1 where the walk goes on, else 0
EMBED_INLINE int cli_chain_step(const SettingsCtx *args)
{
    CliChainWalk *const walk = args->walking;
    char *const next = args->next;
    char named[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_show, SettingsCtx, .path = next, .root = args->root, .out = named);
    if (walk->count >= CLI_SETTINGS_DEEPEST)
    {
        EMBED_CALL(cli_problem, SettingsCtx, .chain = args->chain, .named = named,
                   .detail = ": more than 16 files take one another, and those past them are not read");
        return 0;
    }
    char full[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(query_capacity_path_internal.path_full, QueryCapacityPathCtx, .path = next, .full = full,
                    .capacity = sizeof(full)))
    {
        memor.cpy(&(MemoriaCfg){.dst = full,
                                .src = next,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = next, .cap = CLI_CELLS_POOL}) + 1u});
    }
    if (EMBED_CALL(cli_walked_twice, SettingsCtx, .walk = walk, .path = full))
    {
        EMBED_CALL(cli_problem, SettingsCtx, .chain = args->chain, .named = named,
                   .detail = " is taken twice in one chain, and is read once");
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = walk->seen[walk->count],
                            .src = full,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = full, .cap = CLI_CELLS_POOL}) + 1u});
    unsigned long long size = 0ULL;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = next,
                                                  .size_pointer = &size, .owner = &g_cli_settings_cells);
    if ((bytes == NULL) || (size > CLI_SETTINGS_FILE))
    {
        g_cli_cells->persistent_buf_release((void *)(uintptr_t)bytes, (&g_cli_settings_cells));
        const int is_file = EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = next);
        if (!*args->first || is_file)
        {
            EMBED_CALL(cli_problem, SettingsCtx, .chain = args->chain, .named = named,
                       .detail = is_file ? ": it could not be read" : ": it is not there");
        }
        return 0;
    }
    *args->first = 0;
    // a file's text with its comments taken out is no longer than the file
    char *const plain_text = g_cli_cells->persistent_buf_alloc(size + 1ULL, &g_cli_settings_cells);
    VerbaLinea plain = mmgr_line_over(plain_text, size + 1ULL);
    // the file's bytes are its text
    EMBED_CALL(cli_devcontainer_text.plain_json, CliDevcontainerTextCfg, .text = (const char *)bytes, .size = size,
               .plain = &plain);
    g_cli_cells->persistent_buf_release((void *)(uintptr_t)bytes, (&g_cli_settings_cells));
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea reason = {why, sizeof(why), 0ULL};
    CliJson own;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &own, .node_json_node_pointer = NULL,
               .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    const unsigned int values =
        mmgr_line_ok(&plain) ? EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg,
                                          .json = &own, .text = plain_text, .size = plain.at, .nodes_more = 0u,
                                          .text_more = 0ULL, .err_msg = &reason, .owner = &g_cli_settings_cells)
                             : CLI_JSON_NONE;
    if ((values == CLI_JSON_NONE) || !EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                                 .json_read = &own, .node = values, .kind = CFG_JSON_OBJECT))
    {
        mmgr_line_finish(&reason);
        char msg_buf[CLI_PATH_MAX_LEN + 2u];
        VerbaLinea msg = {msg_buf, sizeof(msg_buf), 0ULL};
        mmgr_line_put(&msg, ": ");
        mmgr_line_put(&msg, why);
        mmgr_line_finish(&msg);
        EMBED_CALL(cli_problem, SettingsCtx, .chain = args->chain, .named = named,
                   .detail = (values == CLI_JSON_NONE) ? msg_buf : " is not an object of settings");
        g_cli_cells->persistent_buf_release(own.node,
                                            EMBED_CALL(cli_cells_table.cell_owner, CliCellsCfg, .cell = own.node));
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&own),
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
        g_cli_cells->persistent_buf_release(plain_text, &g_cli_settings_cells);
        return 0;
    }
    CliChainFile *const walked = &walk->file[walk->count];
    memor.cpy(&(MemoriaCfg){.dst = walked->named,
                            .src = named,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_CELLS_POOL}) + 1u});
    walked->plain = plain_text;
    walked->size = plain.at;
    walked->nodes =
        EMBED_CALL(cli_json_tree_text.json_tokens_count, CliJsonTreeTextCfg, .text = plain_text, .size = plain.at) + 1u;
    walk->count += 1u;
    // the file it takes, from its own folder
    const unsigned int allocs = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &own,
                                           .object = values, .name = s_allocs);
    unsigned long long length = 0ULL;
    const char *const reserved = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                            .json_read = &own, .node = allocs, .length_pointer = &length);
    if ((allocs != CLI_JSON_NONE) && (reserved == NULL))
    {
        EMBED_CALL(cli_problem, SettingsCtx, .chain = args->chain, .named = named,
                   .detail = ": `takes` is the path of a file");
    }
    if (reserved != NULL)
    {
        EMBED_CALL(cli_takes_followed, SettingsCtx, .text = reserved, .length = length, .path = next, .out = next);
    }
    g_cli_cells->persistent_buf_release(own.node,
                                        EMBED_CALL(cli_cells_table.cell_owner, CliCellsCfg, .cell = own.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&own),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    return reserved != NULL;
}

void cli_settings_chain(const CliSettingsPathCfg *args)
{
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &args->chain->json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    args->chain->files = 0u;
    args->chain->problems = 0u;
    CliChainWalk *const walk = g_cli_cells->temporary_buf_alloc(sizeof(*walk), &g_cli_settings_cells);
    if (walk == NULL)
    {
        return;
    }
    walk->count = 0u;
    char next[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_tidy, SettingsCtx, .path = args->path, .out = next);
    int first = 1;
    int going = 1;
    while (going)
    {
        going = EMBED_CALL(cli_chain_step, SettingsCtx, .walking = walk, .next = next, .first = &first,
                           .root = args->root, .chain = args->chain);
    }
    EMBED_CALL(cli_chain_read, SettingsCtx, .chain = args->chain, .walk = walk);
    for (unsigned int at = 0u; at < walk->count; at += 1u)
    {
        g_cli_cells->persistent_buf_release(walk->file[at].plain, &g_cli_settings_cells);
    }
    g_cli_cells->persistent_buf_release(walk, &g_cli_settings_cells);
    // the files in the order they are taken, the one taken first
    for (unsigned int at = 0u; at < (args->chain->files / 2u); at += 1u)
    {
        const unsigned int other = args->chain->files - 1u - at;
        char saved_path[CLI_PATH_MAX_LEN];
        memor.cpy(&(MemoriaCfg){
            .dst = saved_path,
            .src = args->chain->file[at],
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->chain->file[at], .cap = CLI_CELLS_POOL}) + 1u});
        memor.cpy(&(MemoriaCfg){
            .dst = args->chain->file[at],
            .src = args->chain->file[other],
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->chain->file[other], .cap = CLI_CELLS_POOL}) + 1u});
        memor.cpy(
            &(MemoriaCfg){.dst = args->chain->file[other],
                          .src = saved_path,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = saved_path, .cap = CLI_CELLS_POOL}) + 1u});
        const unsigned int values = args->chain->values[at];
        args->chain->values[at] = args->chain->values[other];
        args->chain->values[other] = values;
    }
}

void cli_settings_tree(const CliSettingsPathCfg *args)
{
    char tidy[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_tidy, SettingsCtx, .path = args->root, .out = tidy);
    VerbaLinea line = {path, sizeof(path), 0ULL};
    mmgr_line_put(&line, tidy);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, CLI_SETTINGS_TREE);
    mmgr_line_finish(&line);
    EMBED_CALL(cli_settings_path_table.chain, CliSettingsPathCfg, .path = path, .root = tidy, .chain = args->chain);
}

void cli_settings_reader(const CliSettingsPathCfg *args)
{
    char home[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = home))
    {
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &args->chain->json,
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
        args->chain->files = 0u;
        args->chain->problems = 0u;
        return;
    }
    VerbaLinea line = {path, sizeof(path), 0ULL};
    mmgr_line_put(&line, home);
    mmgr_line_put(&line, "/settings.json");
    mmgr_line_finish(&line);
    EMBED_CALL(cli_settings_path_table.chain, CliSettingsPathCfg, .path = path, .root = NULL, .chain = args->chain);
}
