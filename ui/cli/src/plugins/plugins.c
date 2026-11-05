// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugins.c: the plugins found, those that come with orior and the reader's; the language each file opens as; a script
// plugin's code, read from its folder for the reader's and built into the program for one that comes with orior; and a
// name as an id
#include "plugins.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../text/regexp.h"
#include "../text/text.h"

// The reader's plugins' ids in order, in a cell of `capacity`, `count` of them
typedef struct
{
    char (*name)[CLI_NAME_MAX_LEN];
    unsigned int count;
    unsigned int capacity;
} CliUserPlugins;

typedef struct
{
    const char *name;
    int is_folder;
    void (*each)(const CliEachPluginCfg *args);
    void *context;
    CliLanguages *languages;
    const char *key;
    unsigned long long key_length;
    const char *id;
    const char *prefix;
    const char *text;
    unsigned long long length;
    char *out;
    char *key_out;
} PluginsCtx;
// whether a folder's entry is a plugin's folder the reader's list includes
EMBED_INLINE int cli_user_include(const PluginsCtx *args)
{
    return args->is_folder &&
           (cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}) < CLI_NAME_MAX_LEN);
}

static void cli_user_count(const CliEachNameIsFolderCfg *args)
{
    const char *name = args->name;
    int is_folder = args->is_folder;
    CliUserPlugins *const user = (CliUserPlugins *)args->context;
    user->capacity += (EMBED_CALL(cli_user_include, PluginsCtx, .name = name, .is_folder = is_folder) &&
                       (user->capacity < CLI_USER_PLUGINS))
                          ? 1u
                          : 0u;
}

static void cli_user_each(const CliEachNameIsFolderCfg *args)
{
    const char *name = args->name;
    int is_folder = args->is_folder;
    CliUserPlugins *const user = (CliUserPlugins *)args->context;
    if (!EMBED_CALL(cli_user_include, PluginsCtx, .name = name, .is_folder = is_folder) ||
        (user->count >= user->capacity))
    {
        return;
    }
    unsigned int place = user->count;
    while ((place > 0u) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = user->name[place - 1u],
                                       .other_pointer = name) > 0))
    {
        memor.cpy(&(MemoriaCfg){
            .dst = user->name[place],
            .src = user->name[place - 1u],
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = user->name[place - 1u], .cap = CLI_CELLS_POOL}) + 1u});
        place -= 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = user->name[place],
                            .src = name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
    user->count += 1u;
}

// every plugin handed on, the lock acquired: the reader's counted, their ids taken into a cell of their count, and each
// plugin.json handed on mapped where it lies
EMBED_INLINE void cli_each_lock(const PluginsCtx *args)
{
    for (unsigned int at = 0u; at < g_cli_bundled_count; at += 1u)
    {
        // a plugin's bytes are its JSON's text
        const CliPlugin plugin = {g_cli_bundled[at].id, "bundled", NULL, (const char *)g_cli_bundled[at].text,
                                  g_cli_bundled[at].size};
        EMBED_CALL(args->each, CliEachPluginCfg, .context = args->context, .plugin = &plugin);
    }
    char dir[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_folder_internal.home_plugins, CliFolderCtx, .folder_out = dir))
    {
        return;
    }
    CliUserPlugins user = {NULL, 0u, 0u};
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = dir, .each = cli_user_count,
               .context_bytess = &user);
    user.name = (user.capacity != 0u) ? g_cli_cells->persistent_buf_alloc(
                                            (unsigned long long)user.capacity * CLI_NAME_MAX_LEN, &g_cli_plugins_cells)
                                      : NULL;
    user.capacity = (user.name != NULL) ? user.capacity : 0u;
    EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = dir, .each = cli_user_each,
               .context_bytess = &user);
    for (unsigned int at = 0u; at < user.count; at += 1u)
    {
        char folder[CLI_PATH_MAX_LEN];
        char file[CLI_PATH_MAX_LEN];
        VerbaLinea line = {folder, sizeof(folder), 0ULL};
        mmgr_line_put(&line, dir);
        mmgr_line_ch(&line, '/');
        mmgr_line_put(&line, user.name[at]);
        // the folder's path ended before the file's is made from it
        if (mmgr_line_finish(&line) == 0ULL)
        {
            continue;
        }
        VerbaLinea named = {file, sizeof(file), 0ULL};
        mmgr_line_put(&named, folder);
        mmgr_line_put(&named, "/plugin.json");
        if (mmgr_line_finish(&named) == 0ULL)
        {
            continue;
        }
        unsigned long long size = 0ULL;
        const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = file,
                                                      .size_pointer = &size, .owner = &g_cli_plugins_cells);
        if (bytes == NULL)
        {
            continue;
        }
        // a plugin's bytes are its JSON's text
        const CliPlugin plugin = {user.name[at], "user", folder, (const char *)bytes, size};
        EMBED_CALL(args->each, CliEachPluginCfg, .context = args->context, .plugin = &plugin);
        g_cli_cells->persistent_buf_release((void *)(uintptr_t)bytes, (&g_cli_plugins_cells));
    }
    g_cli_cells->persistent_buf_release(user.name, &g_cli_plugins_cells);
}

void cli_plugins_each(const CliPluginsEachCfg *args)
{
    void (*each)(const CliEachPluginCfg *) = args->each;
    void *context = args->context;
    EMBED_CALL(cli_each_lock, PluginsCtx, .each = each, .context = context);
}

// `key` put as mapped to `id`, in place of the id it had
EMBED_INLINE void cli_map(const PluginsCtx *args)
{
    char name_buf[CLI_NAME_MAX_LEN];
    VerbaLinea line = {name_buf, sizeof(name_buf), 0ULL};
    mmgr_line_put_n(&line, args->key, args->key_length);
    if (mmgr_line_finish(&line) == 0ULL)
    {
        return;
    }
    for (unsigned int at = 0u; at < args->languages->count; at += 1u)
    {
        if (cellul.eq(
                &(CatenaFinitaCfg){.src = args->languages->pair[at].key, .other = name_buf, .cap = CLI_CELLS_POOL}))
        {
            memor.cpy(
                &(MemoriaCfg){.dst = args->languages->pair[at].id,
                              .src = args->id,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->id, .cap = CLI_CELLS_POOL}) + 1u});
            return;
        }
    }
    if (args->languages->count >= args->languages->capacity)
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->languages->pair[args->languages->count].key,
                            .src = name_buf,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = name_buf, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = args->languages->pair[args->languages->count].id,
                            .src = args->id,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->id, .cap = CLI_CELLS_POOL}) + 1u});
    args->languages->count += 1u;
}

// a text in lower case, letter by letter, put after `prefix` into `out`, sized for CLI_NAME_MAX_LEN: its length
EMBED_INLINE unsigned long long cli_on_lower(const PluginsCtx *args)
{
    const char *text = args->text;
    unsigned int letters[CLI_NAME_MAX_LEN];
    const unsigned long long count =
        EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = text, .size = args->length, .letters = letters,
                   .capacity_u64 = CLI_NAME_MAX_LEN);
    VerbaLinea line = {args->out, CLI_NAME_MAX_LEN, 0ULL};
    mmgr_line_put(&line, args->prefix);
    for (unsigned long long at = 0ULL; at < count; at += 1ULL)
    {
        EMBED_CALL(cli_line_letter_internal.utf8_append, CliLineLetterCtx, .line = &line,
                   .letter = EMBED_CALL(cli_regexp_letter.lower, CliRegexpLetterCfg, .letter = letters[at]));
    }
    return mmgr_line_finish(&line);
}

// an extension in ASCII lower case, as to_ascii_lowercase reads it, into `key`, sized for CLI_NAME_MAX_LEN: its length
EMBED_INLINE unsigned long long cli_extension_key(const PluginsCtx *args)
{
    VerbaLinea line = {args->key_out, CLI_NAME_MAX_LEN, 0ULL};
    for (unsigned long long at = 0ULL; at < args->length; at += 1ULL)
    {
        mmgr_line_ch(&line, ((args->text[at] >= 'A') && (args->text[at] <= 'Z')) ? (char)(args->text[at] + 32)
                                                                                 : args->text[at]);
    }
    return mmgr_line_finish(&line);
}

// The folders a plugin stands for as they are listed: the languages they are mapped into, the plugin's `each` in its
// tree, the tree the tables are read from, and the plugin's id
typedef struct
{
    CliLanguages *languages;
    const CliJson *json;
    unsigned int each;
    const char *tree;
    const char *id;
} CliFoldersMapped;

// a folder that is one of the plugin's languages, its name an extension mapped to `<id>:<name>`
static void cli_folder_map(const CliEachNameIsFolderCfg *args)
{
    const char *name = args->name;
    const CliFoldersMapped *const mapped = (const CliFoldersMapped *)args->context;
    if (!args->is_folder || !EMBED_CALL(cli_plugins_json.folder_exists, CliPluginsJsonCfg, .json = mapped->json,
                                        .each_u32 = mapped->each, .tree = mapped->tree, .name = name))
    {
        return;
    }
    char language[CLI_NAME_MAX_LEN];
    VerbaLinea line = {language, sizeof(language), 0ULL};
    mmgr_line_put(&line, mapped->id);
    mmgr_line_ch(&line, ':');
    mmgr_line_put(&line, name);
    char key[CLI_NAME_MAX_LEN];
    const unsigned long long key_length =
        EMBED_CALL(cli_extension_key, PluginsCtx, .text = name,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), .key_out = key);
    if ((mmgr_line_finish(&line) != 0ULL) && (key_length != 0ULL))
    {
        EMBED_CALL(cli_map, PluginsCtx, .languages = mapped->languages, .key = key, .key_length = key_length,
                   .id = language);
    }
}

// A language plugin's keys mapped to its id
static void cli_language_each(const CliEachPluginCfg *args)
{
    void *context = args->context;
    CliLanguages *const languages = (CliLanguages *)context;
    CliJson json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const unsigned int root =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc_temporary, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                   .text = args->plugin->text, .size = args->plugin->size, .nodes_more = 0u, .text_more = 0ULL,
                   .err_msg = &err_msg, .owner = &g_cli_plugins_cells);
    const char *const kind =
        (root != CLI_JSON_NONE)
            ? EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                         .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                            .object = root, .name = "kind"),
                         .length_pointer = NULL)
            : NULL;
    if ((root == CLI_JSON_NONE) ||
        ((kind != NULL) && (!cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "language", .cap = CLI_CELLS_POOL}))))
    {
        g_cli_cells->persistent_buf_release(json.node,
                                            EMBED_CALL(cli_cells_table.cell_owner, CliCellsCfg, .cell = json.node));
        EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
                   .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
        return;
    }
    const char *const named =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                      .object = root, .name = "id"),
                   .length_pointer = NULL);
    const char *const id = (named != NULL) ? named : args->plugin->id;
    static const char *const s_lists[3] = {"extensions", "names", "paths"};
    static const char *const s_prefixes[3] = {"", "/", "?"};
    for (unsigned int list = 0u; list < 3u; list += 1u)
    {
        const unsigned int items = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = &json, .object = root, .name = s_lists[list]);
        for (unsigned int item = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json,
                                            .node = items, .kind = CFG_JSON_ARRAY)
                                     ? json.node[items].first
                                     : CLI_JSON_NONE;
             item != CLI_JSON_NONE; item = json.node[item].next)
        {
            unsigned long long length = 0ULL;
            const char *const text = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                                .json_read = &json, .node = item, .length_pointer = &length);
            if (text == NULL)
            {
                continue;
            }
            char key[CLI_NAME_MAX_LEN];
            unsigned long long key_length = 0ULL;
            if (list == 2u)
            {
                VerbaLinea line = {key, sizeof(key), 0ULL};
                mmgr_line_put(&line, "?");
                mmgr_line_put_n(&line, text, length);
                key_length = mmgr_line_finish(&line);
            }
            else if (list == 0u)
            {
                key_length = EMBED_CALL(cli_extension_key, PluginsCtx, .text = text, .length = length, .key_out = key);
            }
            else
            {
                key_length = EMBED_CALL(cli_on_lower, PluginsCtx, .prefix = s_prefixes[list], .text = text,
                                        .length = length, .out = key);
            }
            if (key_length != 0ULL)
            {
                EMBED_CALL(cli_map, PluginsCtx, .languages = languages, .key = key, .key_length = key_length, .id = id);
            }
        }
    }
    // a plugin standing for each folder of the tree that holds its table
    const unsigned int each = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                         .object = root, .name = "each");
    char tree[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    unsigned long long folder_length = 0ULL;
    const char *const named_folder =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                      .object = each, .name = "folder"),
                   .length_pointer = &folder_length);
    if ((named_folder != NULL) && EMBED_CALL(cli_plugins_tree_table.tree, CliPluginsTreeCfg, .tree_out = tree))
    {
        VerbaLinea line = {folder, sizeof(folder), 0ULL};
        mmgr_line_put(&line, tree);
        mmgr_line_ch(&line, '/');
        mmgr_line_put_n(&line, named_folder, folder_length);
        CliFoldersMapped mapped = {languages, &json, each, tree, id};
        if (mmgr_line_finish(&line) != 0ULL)
        {
            (void)EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = folder,
                             .each = cli_folder_map, .context_bytess = &mapped);
        }
    }
    g_cli_cells->persistent_buf_release(json.node,
                                        EMBED_CALL(cli_cells_table.cell_owner, CliCellsCfg, .cell = json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
}

void cli_plugins_languages(const CliPluginsCfg *args)
{
    CliLanguages *languages = args->languages;
    CliLanguagePair *const pair =
        g_cli_cells->persistent_buf_alloc((unsigned long long)CLI_LANGUAGE_PAIRS * sizeof(*pair), &g_cli_plugins_cells);
    *languages = (CliLanguages){pair, 0u, (pair != NULL) ? CLI_LANGUAGE_PAIRS : 0u};
    EMBED_CALL(cli_plugins_each_table.each, CliPluginsEachCfg, .each = cli_language_each, .context = languages);
}

void cli_plugins_languages_for_call(const CliPluginsCfg *args)
{
    CliLanguages *languages = args->languages;
    CliLanguagePair *const pair =
        g_cli_cells->temporary_buf_alloc((unsigned long long)CLI_LANGUAGE_PAIRS * sizeof(*pair), &g_cli_plugins_cells);
    *languages = (CliLanguages){pair, 0u, (pair != NULL) ? CLI_LANGUAGE_PAIRS : 0u};
    EMBED_CALL(cli_plugins_each_table.each, CliPluginsEachCfg, .each = cli_language_each, .context = languages);
}

int cli_plugins_language_for(const CliPluginsLanguagesPathCfg *args)
{
    const char *name = args->path + cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL});
    name = args->path + EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path,
                                   .at = (unsigned long long)(name - args->path), .set = "/\\");
    char key[CLI_NAME_MAX_LEN];
    EMBED_CALL(cli_on_lower, PluginsCtx, .prefix = "/", .text = name,
               .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), .out = key);
    for (unsigned int at = 0u; at < args->languages_read->count; at += 1u)
    {
        if (cellul.eq(
                &(CatenaFinitaCfg){.src = args->languages_read->pair[at].key, .other = key, .cap = CLI_CELLS_POOL}))
        {
            memor.cpy(&(MemoriaCfg){.dst = args->id_out,
                                    .src = args->languages_read->pair[at].id,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->languages_read->pair[at].id,
                                                                           .cap = CLI_CELLS_POOL}) +
                                             1u});
            return 1;
        }
    }
    // the path's letters are no more than its bytes, and the work state a pattern is read into is taken only where a
    // key is a pattern
    int patterned = 0;
    for (unsigned int at = 0u; (at < args->languages_read->count) && !patterned; at += 1u)
    {
        patterned = args->languages_read->pair[at].key[0] == '?';
    }
    const unsigned long long bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL});
    unsigned int *const slashed =
        patterned ? g_cli_cells->temporary_buf_alloc((bytes + 1ULL) * sizeof(unsigned int), &g_cli_plugins_cells)
                  : NULL;
    CliRegexpWorkState *const work_state =
        (slashed != NULL) ? g_cli_cells->temporary_buf_alloc(sizeof(CliRegexpWorkState), &g_cli_plugins_cells) : NULL;
    const unsigned long long length = (work_state != NULL)
                                          ? EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = args->path,
                                                       .size = bytes, .letters = slashed, .capacity_u64 = bytes + 1ULL)
                                          : 0ULL;
    for (unsigned long long at = 0ULL; at < length; at += 1ULL)
    {
        slashed[at] = (slashed[at] == '\\') ? '/' : slashed[at];
    }
    int matched = 0;
    for (unsigned int at = 0u; (work_state != NULL) && (at < args->languages_read->count) && !matched; at += 1u)
    {
        if (args->languages_read->pair[at].key[0] != '?')
        {
            continue;
        }
        CliRegexp pattern;
        EMBED_CALL(cli_regexp_capacity.capacity, CliRegexpWorkStateCfg, .regexp_out = &pattern,
                   .work_state = work_state);
        char why[CLI_PATH_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        if (!EMBED_CALL(cli_regexp_source.read, CliRegexpSourceCfg, .regexp_out = &pattern,
                        .source = args->languages_read->pair[at].key + 1, .flags = "", .err_msg = &err_msg))
        {
            continue;
        }
        CliRegexpSpan groups[CLI_REGEXP_GROUPS + 1u];
        unsigned long long place = 0ULL;
        while ((place < length) && !matched)
        {
            if (EMBED_CALL(cli_regexp_text_u_32s.match_into, CliRegexpTextU32sCfg, .regexp = &pattern,
                           .text_u32s = slashed, .length = length, .at = place, .groups = groups))
            {
                memor.cpy(&(MemoriaCfg){.dst = args->id_out,
                                        .src = args->languages_read->pair[at].id,
                                        .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->languages_read->pair[at].id,
                                                                               .cap = CLI_CELLS_POOL}) +
                                                 1u});
                matched = 1;
            }
            place += 1ULL;
        }
    }
    g_cli_cells->persistent_buf_release(work_state, &g_cli_plugins_cells);
    g_cli_cells->persistent_buf_release(slashed, &g_cli_plugins_cells);
    if (matched)
    {
        return 1;
    }
    const char *const dot =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = name,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), .byte = '.');
    if ((dot == NULL) || (dot == name))
    {
        return 0;
    }
    EMBED_CALL(cli_on_lower, PluginsCtx, .prefix = "", .text = dot + 1,
               .length = cellul.len(&(CatenaFinitaCfg){.src = dot + 1, .cap = CLI_CELLS_POOL}), .out = key);
    for (unsigned int at = 0u; at < args->languages_read->count; at += 1u)
    {
        if ((args->languages_read->pair[at].key[0] != '/') && (args->languages_read->pair[at].key[0] != '?') &&
            (cellul.eq(
                &(CatenaFinitaCfg){.src = args->languages_read->pair[at].key, .other = key, .cap = CLI_CELLS_POOL})))
        {
            memor.cpy(&(MemoriaCfg){.dst = args->id_out,
                                    .src = args->languages_read->pair[at].id,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->languages_read->pair[at].id,
                                                                           .cap = CLI_CELLS_POOL}) +
                                             1u});
            return 1;
        }
    }
    return 0;
}

// The last plugin of an id found: whether it is the reader's, its folder, and its plugin.json in a cell of its size
typedef struct
{
    const char *id;
    int found;
    int user;
    char folder[CLI_PATH_MAX_LEN];
    char *text;
    unsigned long long size;
} CliCodeFound;

static void plugins_code_each(const CliEachPluginCfg *args)
{
    CliCodeFound *const code = (CliCodeFound *)args->context;
    if (!cellul.eq(&(CatenaFinitaCfg){.src = args->plugin->id, .other = code->id, .cap = CLI_CELLS_POOL}))
    {
        return;
    }
    code->found = 1;
    code->user = args->plugin->folder != NULL;
    const char *const folder = (args->plugin->folder != NULL) ? args->plugin->folder : "";
    memor.cpy(&(MemoriaCfg){.dst = code->folder,
                            .src = folder,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}) + 1u});
    g_cli_cells->persistent_buf_release(code->text, &g_cli_plugins_cells);
    code->text = g_cli_cells->persistent_buf_alloc(args->plugin->size + 1ULL, &g_cli_plugins_cells);
    code->size = (code->text != NULL) ? args->plugin->size : 0ULL;
    if (code->text != NULL)
    {
        // a plugin.json's size fits a size_t, as it was read whole
        memor.cpy(&(MemoriaCfg){.dst = code->text, .src = args->plugin->text, .bytes = (size_t)args->plugin->size});
        code->text[args->plugin->size] = '\0';
    }
}

// the code built into the program of the plugin `id` that comes with orior, or NULL where it runs none
EMBED_INLINE const CliBundled *plugins_code_bundle(const PluginsCtx *args)
{
    for (unsigned int at = 0u; at < g_cli_bundled_count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = g_cli_bundled[at].id, .other = args->id, .cap = CLI_CELLS_POOL}) &&
            (g_cli_bundled[at].code != NULL))
        {
            return &g_cli_bundled[at];
        }
    }
    return NULL;
}

int cli_plugins_code_get(const CliPluginsIdCfg *args)
{
    const char *id = args->id;
    CliCodeFound code;
    memor.set(&(MemoriaCfg){.dst = &code, .val = 0, .bytes = sizeof(code)});
    code.id = id;
    EMBED_CALL(cli_plugins_each_table.each, CliPluginsEachCfg, .each = plugins_code_each, .context = &code);
    const CliBundled *const bundled =
        (code.found && !code.user) ? EMBED_CALL(plugins_code_bundle, PluginsCtx, .id = id) : NULL;
    int done = 0;
    if (!code.found)
    {
        mmgr_line_put(args->err_msg, "no plugin is named ");
        mmgr_line_put(args->err_msg, id);
    }
    else if (code.user && (code.text != NULL))
    {
        done = EMBED_CALL(cli_plugins_id.code_in, CliPluginsIdCfg, .id = id, .folder_read = code.folder,
                          .text = code.text, .size = code.size, .out = args->out, .err_msg = args->err_msg);
    }
    else if (bundled != NULL)
    {
        mmgr_line_put_n(args->out, (const char *)bundled->code, bundled->code_size);
        done = mmgr_line_ok(args->out);
    }
    else
    {
        mmgr_line_put(args->err_msg, id);
        mmgr_line_put(args->err_msg,
                      code.user ? ": its plugin.json does not read" : " comes with orior and runs no code");
    }
    g_cli_cells->persistent_buf_release(code.text, &g_cli_plugins_cells);
    return done;
}
void cli_plugins_slug(const CliPluginsIdNameCfg *args)
{
    const char *from = args->name;
    const char *to = args->name + cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL});
    from += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = from, .at = 0ULL,
                       .size = (unsigned long long)(to - from), .set = CLI_TEXT_WHITE);
    to = from + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = from,
                           .at = (unsigned long long)(to - from), .set = CLI_TEXT_WHITE);
    unsigned long long length = 0ULL;
    {
        const char *at = from;
        while ((at < to) && ((length + 1ULL) < CLI_NAME_MAX_LEN))
        {
            const char letter = at[0];
            const int allowed = ((letter >= 'a') && (letter <= 'z')) || ((letter >= 'A') && (letter <= 'Z')) ||
                                ((letter >= '0') && (letter <= '9'));
            if (allowed)
            {
                args->id_out[length] = ((letter >= 'A') && (letter <= 'Z')) ? (char)(letter + 32) : letter;
                length += 1ULL;
            }
            else if ((length != 0ULL) && (args->id_out[length - 1u] != '-'))
            {
                args->id_out[length] = '-';
                length += 1ULL;
            }
            // a byte past ASCII that starts or carries a letter is no letter or digit here, and one run of them one
            // dash
            at += 1;
        }
    }
    length = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = args->id_out, .at = length,
                        .set = "-");
    args->id_out[length] = '\0';
}
