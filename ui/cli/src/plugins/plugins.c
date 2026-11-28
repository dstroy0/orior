// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugins.c: the plugins found, those that come with orior and the reader's; the language each file opens as; a script
// plugin's code, read from its folder for the reader's and built into the program for one that comes with orior; and a
// name as an id
#include "plugins.h"
#include "../../../../includes/regexp/regexp.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "../text/text.h"
#include "ascii_persona_bitorum/ascii_persona_bitorum.h"

// The reader's plugins' ids in order, in a cell of `capacity`, `count` of them
typedef struct
{
    char (*name)[CLI_NAME_MAX_LEN];
    unsigned int count;
    unsigned int capacity;
} CliUserPlugins;

// One key an extension, whole name or pattern of paths opens by, and the id of the language it opens as
typedef struct
{
    char key[CLI_NAME_MAX_LEN];
    char id[CLI_NAME_MAX_LEN];
} CliLanguagePair;
// One language with a grammar, as the cell of the languages keeps it. The record holds its id, and the marks at the
// start of a directive's line and of its next line. It holds whether the tokens name a declaration. The last member
// says whether the grammar went to the colorer: 1, -1 where the colorer refused it, or 0 before the first ask
typedef struct
{
    char id[CLI_NAME_MAX_LEN];
    char directive_mark[CLI_LANGUAGE_MARK_MAX];
    char directive_continued[CLI_LANGUAGE_MARK_MAX];
    int declares;
    int stored;
} CliLanguageGrammarEntry;
// The head of the languages' cell: the count of pairs after it, and the count of grammars after the pairs
typedef struct
{
    unsigned int count;
    unsigned int grammars;
} CliLanguagesHead;
// the offsets in a languages' cell of the pair `at`'s key and of its id; and of the grammar `at` after `count` pairs
#define CLI_LANGUAGE_KEY_AT(at) (sizeof(CliLanguagesHead) + ((unsigned long long)(at) * sizeof(CliLanguagePair)))
#define CLI_LANGUAGE_ID_AT(at) (CLI_LANGUAGE_KEY_AT(at) + offsetof(CliLanguagePair, id))
#define CLI_LANGUAGE_GRAMMAR_AT(count, at)                                                                             \
    (CLI_LANGUAGE_KEY_AT(count) + ((unsigned long long)(at) * sizeof(CliLanguageGrammarEntry)))

// The language that each extension, whole name and pattern of paths opens as, from the walk over the plugins. The
// walk fills a cell of the temporary tier with `capacity` pairs, `count` of them in use. CLI_LANGUAGE_GRAMMARS grammars
// follow, `grammars` of them in use. The build takes the cell and gives it back
typedef struct
{
    CliLanguagePair *pair;
    unsigned int count;
    unsigned int capacity;
    CliLanguageGrammarEntry *grammar;
    unsigned int grammars;
} CliLanguagesBuilt;

// The token of the languages' cell every thread reads, its head and then its pairs, sized to them. The plugins module
// owns it, and a write of a plugin puts a new cell in its place
static embed_atomic_word s_languages;
static const CarcerMovableEntry *cli_languages_token(void);

typedef struct
{
    const char *name;
    int is_folder;
    void (*each)(const CliEachPluginCfg *args);
    void *context;
    CliLanguagesBuilt *languages;
    const char *key;
    unsigned long long key_length;
    const char *id;
    const char *prefix;
    const char *text;
    unsigned long long length;
    char *out;
    char *key_out;
    const CarcerMovableEntry *pairs;
    unsigned int count;
    unsigned int at;
    char *id_out;
    const char *path;
    const CliJson *json;
    unsigned int root;
    int language;
    const CliPlugin *plugin;
} PluginsCtx;
// whether a folder's entry is a plugin's folder the reader's list includes
EMBED_INLINE int cli_user_include(const PluginsCtx *args)
{
    return args->is_folder &&
           (cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES}) < CLI_NAME_MAX_LEN);
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
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = user->name[place - 1u], .cap = CLI_POOL_BYTES}) + 1u});
        place -= 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = user->name[place],
                            .src = name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES}) + 1u});
    user->count += 1u;
}

// every plugin passed on, with the lock acquired. The call counts the reader's plugins and takes their ids into a cell
// of their count. It maps each plugin.json where it lies and passes it on
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
    // the names live for this call alone: a cell of the temporary tier
    user.name = (user.capacity != 0u) ? g_platform_guard->temporary_buf_alloc(
                                            (unsigned long long)user.capacity * CLI_NAME_MAX_LEN, g_cli_plugins_owner)
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
        // the folder's path gets its end before the call makes the file's path from it
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
                                                      .size_pointer = &size, .owner = g_cli_plugins_owner);
        if (bytes == NULL)
        {
            continue;
        }
        // a plugin's bytes are its JSON's text
        const CliPlugin plugin = {user.name[at], "user", folder, (const char *)bytes, size};
        EMBED_CALL(args->each, CliEachPluginCfg, .context = args->context, .plugin = &plugin);
        g_platform_guard->persistent_buf_release((void *)(embed_word)bytes, g_cli_plugins_owner);
    }
    g_platform_guard->persistent_buf_release(user.name, g_cli_plugins_owner);
}

void cli_plugins_each(const CliPluginsEachCfg *args)
{
    cli_plugins_settings_each(args);
}

void cli_plugins_files_each(const CliPluginsEachCfg *args)
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
    // each key and id written with its end
    const unsigned long long key_bytes = cellul.len(&(CatenaFinitaCfg){.src = name_buf, .cap = sizeof(name_buf)}) + 1u;
    const unsigned long long id_bytes = cellul.len(&(CatenaFinitaCfg){.src = args->id, .cap = CLI_NAME_MAX_LEN}) + 1u;
    CliLanguagePair *const pair = args->languages->pair;
    unsigned int at = 0u;
    while ((at < args->languages->count) &&
           !cellul.eq(&(CatenaFinitaCfg){.src = pair[at].key, .other = name_buf, .cap = CLI_NAME_MAX_LEN}))
    {
        at += 1u;
    }
    if (at == args->languages->capacity)
    {
        return;
    }
    if (at == args->languages->count)
    {
        memor.cpy(&(MemoriaCfg){.dst = pair[at].key, .src = name_buf, .bytes = key_bytes});
        args->languages->count += 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = pair[at].id, .src = args->id, .bytes = id_bytes});
}

// a text in lower case, character by character, put after `prefix` into `out`, sized for CLI_NAME_MAX_LEN: its length
EMBED_INLINE unsigned long long cli_on_lower(const PluginsCtx *args)
{
    const char *text = args->text;
    unsigned int characters[CLI_NAME_MAX_LEN];
    const unsigned long long count =
        EMBED_CALL(regexp_text.characters, RegexpTextCfg, .text = text, .size = args->length, .characters = characters,
                   .capacity_u64 = CLI_NAME_MAX_LEN);
    VerbaLinea line = {args->out, CLI_NAME_MAX_LEN, 0ULL};
    mmgr_line_put(&line, args->prefix);
    for (unsigned long long at = 0ULL; at < count; at += 1ULL)
    {
        EMBED_CALL(cli_line_character_internal.utf8_append, CliLineLetterCtx, .line = &line,
                   .character = EMBED_CALL(regexp_character.lower, RegexpLetterCfg, .character = characters[at]));
    }
    return mmgr_line_finish(&line);
}

// an extension in ASCII lower case, as to_ascii_lowercase reads it, into `key`, sized for CLI_NAME_MAX_LEN: its length
EMBED_INLINE unsigned long long cli_extension_key(const PluginsCtx *args)
{
    VerbaLinea line = {args->key_out, CLI_NAME_MAX_LEN, 0ULL};
    for (unsigned long long at = 0ULL; at < args->length; at += 1ULL)
    {
        // Explicit cast reads the byte as its code point
        mmgr_line_ch(&line, (ascii.in(&(AsciiCfg){.type = MMGR_ASCII_UPPER, .byte = (uint8_t)args->text[at]}))
                                ? (char)(args->text[at] + 32)
                                : args->text[at]);
    }
    return mmgr_line_finish(&line);
}

// The folders of a plugin, as the folder walk lists them. The record holds the languages that take the folders, and the
// plugin's `each` in its tree. It also holds the tree with the tables, and the plugin's id
typedef struct
{
    CliLanguagesBuilt *languages;
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
                   .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES}), .key_out = key);
    if ((mmgr_line_finish(&line) != 0ULL) && (key_length != 0ULL))
    {
        EMBED_CALL(cli_map, PluginsCtx, .languages = mapped->languages, .key = key, .key_length = key_length,
                   .id = language);
    }
}

// A language plugin's grammar noted where it has one, in the place of an earlier note of its id. The note holds whether
// the tokens name a declaration, and the marks at the start of a directive's line and of its next line. A later plugin
// of the id with no grammar removes the note. The last plugin of each id gives the note
EMBED_INLINE void cli_language_grammar_note(const PluginsCtx *args)
{
    CliLanguagesBuilt *const languages = args->languages;
    const CliPlugin *const plugin = args->plugin;
    const int grammared =
        args->language &&
        EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = args->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                      .object = args->root, .name = "grammar"),
                   .type = CFG_JSON_OBJECT);
    unsigned int at = 0u;
    while (
        (at < languages->grammars) &&
        !cellul.eq(&(CatenaFinitaCfg){.src = languages->grammar[at].id, .other = plugin->id, .cap = CLI_NAME_MAX_LEN}))
    {
        at += 1u;
    }
    if (!grammared && (at < languages->grammars))
    {
        languages->grammar[at] = languages->grammar[languages->grammars - 1u];
        languages->grammars -= 1u;
        return;
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = plugin->id, .cap = CLI_NAME_MAX_LEN});
    if (!grammared || (at == CLI_LANGUAGE_GRAMMARS) || (length == CLI_NAME_MAX_LEN))
    {
        return;
    }
    CliLanguageGrammarEntry *const entry = &languages->grammar[at];
    memor.set(&(MemoriaCfg){.dst = entry, .val = 0, .bytes = sizeof(*entry)});
    memor.cpy(&(MemoriaCfg){.dst = entry->id, .src = plugin->id, .bytes = length + 1ULL});
    static const char *const s_mark_names[2] = {"directive_mark", "directive_continued"};
    char *const marks[2] = {entry->directive_mark, entry->directive_continued};
    for (unsigned int mark = 0u; mark < 2u; mark += 1u)
    {
        const char *const named =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                          .json_read = args->json, .object = args->root, .name = s_mark_names[mark]),
                       .length_pointer = NULL);
        if (named != NULL)
        {
            memor.cpy(&(MemoriaCfg){
                .dst = marks[mark],
                .src = named,
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_LANGUAGE_MARK_MAX - 1u})});
        }
    }
    unsigned long long found = 0ULL;
    entry->declares =
        EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = plugin->text, .length_value = plugin->size,
                   .required = "declaration.", .size = 12u, .at_pointer = &found);
    languages->grammars += (at == languages->grammars) ? 1u : 0u;
}

// A language plugin's keys mapped to its id
static void cli_language_each(const CliEachPluginCfg *args)
{
    void *context = args->context;
    CliLanguagesBuilt *const languages = (CliLanguagesBuilt *)context;
    // the tree the plugins module keeps for the plugin. The record views the kept tree's cells, and this
    // reader gives none of them back
    const CliJson json = (args->plugin->json != NULL) ? *args->plugin->json : (CliJson){0};
    const unsigned int root = (args->plugin->json != NULL) ? args->plugin->root : CLI_JSON_NONE;
    const char *const type =
        (root != CLI_JSON_NONE)
            ? EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                         .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                            .object = root, .name = "type"),
                         .length_pointer = NULL)
            : NULL;
    if ((root == CLI_JSON_NONE) ||
        ((type != NULL) && (!cellul.eq(&(CatenaFinitaCfg){.src = type, .other = "language", .cap = CLI_POOL_BYTES}))))
    {
        EMBED_CALL(cli_language_grammar_note, PluginsCtx, .languages = languages, .json = &json, .root = root,
                   .language = 0, .plugin = args->plugin);
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
        for (unsigned int item = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = &json,
                                            .node = items, .type = CFG_JSON_ARRAY)
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
    // a plugin located for each folder of the tree that holds its table
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
    EMBED_CALL(cli_language_grammar_note, PluginsCtx, .languages = languages, .json = &json, .root = root,
               .language = 1, .plugin = args->plugin);
}

// the token `pairs` put in place of the languages' cell, and the cell before it given back
EMBED_INLINE void cli_languages_placed(const PluginsCtx *args)
{
    // the token goes through the atomic word as the integer of its address
    const CarcerMovableEntry *const before = (const CarcerMovableEntry *)(embed_word)EMBED_CALL(
        embed_atomic_word_table.exchange, EmbedAtomicWordExchangeCfg, .word = &s_languages,
        .value = (embed_atomic_word_value)(embed_word)args->pairs);
    (void)g_platform_guard->movable_buf_release(before, g_cli_plugins_owner);
}

// The walk over the plugins reads the pairs and the grammars into one cell of the temporary tier, at their max. The
// call then writes them into a movable cell of their size, in place of the cell before it
void cli_plugins_languages_changed(void)
{
    // the call reads the settings again first, and then reads the languages from them
    cli_plugins_settings_changed();
    CliLanguagePair *const pair = g_platform_guard->temporary_buf_alloc(
        ((unsigned long long)CLI_LANGUAGE_PAIRS * sizeof(CliLanguagePair)) +
            ((unsigned long long)CLI_LANGUAGE_GRAMMARS * sizeof(CliLanguageGrammarEntry)),
        g_cli_plugins_owner);
    if (pair == NULL)
    {
        return;
    }
    // the grammars follow the pairs' max in the one cell
    CliLanguagesBuilt built = {pair, 0u, CLI_LANGUAGE_PAIRS,
                               (CliLanguageGrammarEntry *)(void *)(pair + CLI_LANGUAGE_PAIRS), 0u};
    EMBED_CALL(cli_plugins_each_table.each, CliPluginsEachCfg, .each = cli_language_each, .context = &built);
    const CarcerMovableEntry *const pairs =
        g_platform_guard->movable_buf_alloc(CLI_LANGUAGE_GRAMMAR_AT(built.count, built.grammars), g_cli_plugins_owner);
    const CliLanguagesHead head = {built.count, built.grammars};
    (void)g_platform_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = pairs, .offset = 0ULL, .src = &head, .bytes = sizeof(head)});
    (void)g_platform_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = pairs,
                            .offset = CLI_LANGUAGE_KEY_AT(0u),
                            .src = built.pair,
                            .bytes = (unsigned long long)built.count * sizeof(CliLanguagePair)});
    (void)g_platform_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = pairs,
                            .offset = CLI_LANGUAGE_GRAMMAR_AT(built.count, 0u),
                            .src = built.grammar,
                            .bytes = (unsigned long long)built.grammars * sizeof(CliLanguageGrammarEntry)});
    g_platform_guard->persistent_buf_release(pair, g_cli_plugins_owner);
    EMBED_CALL(cli_languages_placed, PluginsCtx, .pairs = pairs);
}

int cli_plugins_grammar_of(const CliPluginsGrammarOfCfg *args)
{
    char id[CLI_NAME_MAX_LEN];
    const char *const wanted = (args->id != NULL)
                                   ? args->id
                                   : (EMBED_CALL(cli_plugins_languages_path.language_for, CliPluginsLanguagesPathCfg,
                                                 .path = args->path, .id_out = id)
                                          ? id
                                          : NULL);
    if (wanted == NULL)
    {
        return 0;
    }
    const CarcerMovableEntry *const pairs = cli_languages_token();
    CliLanguagesHead head = {0u, 0u};
    (void)g_platform_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = pairs, .offset = 0ULL, .dst = &head, .bytes = sizeof(head)});
    const unsigned long long wanted_bytes =
        cellul.len(&(CatenaFinitaCfg){.src = wanted, .cap = CLI_NAME_MAX_LEN}) + 1ULL;
    unsigned int at = 0u;
    while (
        (at < head.grammars) &&
        (g_platform_guard->movable_buf_compare(&(CarcerMovableCfg){
             .cell = pairs, .offset = CLI_LANGUAGE_GRAMMAR_AT(head.count, at), .src = wanted, .bytes = wanted_bytes}) !=
         0))
    {
        at += 1u;
    }
    CliLanguageGrammarEntry entry;
    if ((at == head.grammars) ||
        !g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = pairs, .offset = CLI_LANGUAGE_GRAMMAR_AT(head.count, at), .dst = &entry, .bytes = sizeof(entry)}))
    {
        return 0;
    }
    CliLanguageGrammar *const out = args->grammar_out;
    memor.cpy(&(MemoriaCfg){.dst = out->id, .src = entry.id, .bytes = sizeof(out->id)});
    memor.cpy(
        &(MemoriaCfg){.dst = out->directive_mark, .src = entry.directive_mark, .bytes = sizeof(entry.directive_mark)});
    memor.cpy(&(MemoriaCfg){
        .dst = out->directive_continued, .src = entry.directive_continued, .bytes = sizeof(entry.directive_continued)});
    out->declares = entry.declares;
    // the grammar goes to the colorer at the first ask that stores it, and the cell keeps whether it went
    if (args->store && (entry.stored == 0))
    {
        char why[CLI_PATH_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        entry.stored = EMBED_CALL(cli_plugins_id_key.grammar_store, CliPluginsIdKeyCfg, .id = entry.id, .key = out->key,
                                  .err_msg = &err_msg)
                           ? 1
                           : -1;
        (void)g_platform_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = pairs,
            .offset = CLI_LANGUAGE_GRAMMAR_AT(head.count, at) + offsetof(CliLanguageGrammarEntry, stored),
            .src = &entry.stored,
            .bytes = sizeof(entry.stored)});
    }
    VerbaLinea key = {out->key, sizeof(out->key), 0ULL};
    mmgr_line_put(&key, "plugin:");
    mmgr_line_put(&key, entry.id);
    (void)mmgr_line_finish(&key);
    return (entry.stored >= 0) && (!args->store || (entry.stored == 1));
}

void cli_plugins_watch_start(void)
{
    char dir[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_folder_internal.home_plugins, CliFolderCtx, .folder_out = dir) &&
        EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = dir))
    {
        (void)EMBED_CALL(cli_folder_watch_internal.start, CliFolderWatchCtx, .folder = dir,
                         .changed = cli_plugins_languages_changed);
    }
}

void cli_plugins_languages_end(void)
{
    EMBED_CALL(cli_languages_placed, PluginsCtx, .pairs = NULL);
    cli_plugins_settings_end();
}

// the token of the languages' cell, the cell read from the plugins where no call read it before
static const CarcerMovableEntry *cli_languages_token(void)
{
    if (EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg, .word = &s_languages) == 0u)
    {
        cli_plugins_languages_changed();
    }
    // the atomic word holds the integer of the token's address
    return (const CarcerMovableEntry *)(embed_word)EMBED_CALL(embed_atomic_word_table.load, EmbedAtomicWordLoadCfg,
                                                              .word = &s_languages);
}

// the id of the pair `at` of the cell `pairs` read into `id_out` with its end: 1
EMBED_INLINE int cli_language_id_read(const PluginsCtx *args)
{
    const unsigned long long length = g_platform_guard->movable_buf_measure(
        &(CarcerMovableCfg){.cell = args->pairs, .offset = CLI_LANGUAGE_ID_AT(args->at)});
    (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = args->pairs, .offset = CLI_LANGUAGE_ID_AT(args->at), .dst = args->id_out, .bytes = length + 1ULL});
    return 1;
}

// the first byte of the pair `at`'s key in the cell `pairs`
EMBED_INLINE char cli_language_key_first(const PluginsCtx *args)
{
    char first = '\0';
    (void)g_platform_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = args->pairs, .offset = CLI_LANGUAGE_KEY_AT(args->at), .dst = &first, .bytes = 1u});
    return first;
}

// The first of the `count` pairs of the cell `pairs` whose key is a pattern that `path` matches, its id into `id_out`:
// 1, or 0 where none matches. The path's characters and the pattern's work state are this call's own
static int cli_language_pattern_for(const PluginsCtx *args)
{
    const unsigned long long whole = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_PATH_MAX_LEN});
    unsigned int slashed[CLI_PATH_MAX_LEN + 1u];
    RegexpWorkState work_state;
    const unsigned long long length = EMBED_CALL(regexp_text.characters, RegexpTextCfg, .text = args->path,
                                                 .size = whole, .characters = slashed, .capacity_u64 = whole + 1ULL);
    for (unsigned long long at = 0ULL; at < length; at += 1ULL)
    {
        slashed[at] = (slashed[at] == '\\') ? '/' : slashed[at];
    }
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        if (EMBED_CALL(cli_language_key_first, PluginsCtx, .pairs = args->pairs, .at = at) != '?')
        {
            continue;
        }
        char key[CLI_NAME_MAX_LEN];
        (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = args->pairs, .offset = CLI_LANGUAGE_KEY_AT(at), .dst = key, .bytes = sizeof(key)});
        Regexp pattern;
        EMBED_CALL(regexp_capacity.capacity, RegexpWorkStateCfg, .regexp_out = &pattern, .work_state = &work_state);
        char why[CLI_PATH_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        if (!EMBED_CALL(regexp_source.read, RegexpSourceCfg, .regexp_out = &pattern, .source = key + 1, .flags = "",
                        .err_msg = &err_msg))
        {
            continue;
        }
        RegexpSpan groups[REGEXP_GROUPS + 1u];
        for (unsigned long long place = 0ULL; place < length; place += 1ULL)
        {
            if (EMBED_CALL(regexp_text_u_32s.match_into, RegexpTextU32sCfg, .regexp = &pattern, .text_u32s = slashed,
                           .length = length, .at = place, .groups = groups))
            {
                return EMBED_CALL(cli_language_id_read, PluginsCtx, .pairs = args->pairs, .at = at,
                                  .id_out = args->id_out);
            }
        }
    }
    return 0;
}

int cli_plugins_language_for(const CliPluginsLanguagesPathCfg *args)
{
    const CarcerMovableEntry *const pairs = cli_languages_token();
    CliLanguagesHead head = {0u};
    (void)g_platform_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = pairs, .offset = 0ULL, .dst = &head, .bytes = sizeof(head)});
    const char *name = args->path + cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES});
    name = args->path + EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path,
                                   .at = (unsigned long long)(name - args->path), .set = "/\\");
    char key[CLI_NAME_MAX_LEN];
    EMBED_CALL(cli_on_lower, PluginsCtx, .prefix = "/", .text = name,
               .length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES}), .out = key);
    unsigned long long key_bytes = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = sizeof(key)}) + 1ULL;
    for (unsigned int at = 0u; at < head.count; at += 1u)
    {
        if (g_platform_guard->movable_buf_compare(&(CarcerMovableCfg){
                .cell = pairs, .offset = CLI_LANGUAGE_KEY_AT(at), .src = key, .bytes = key_bytes}) == 0)
        {
            return EMBED_CALL(cli_language_id_read, PluginsCtx, .pairs = pairs, .at = at, .id_out = args->id_out);
        }
    }
    // the call reads the path's characters and the work state of a pattern only where a key is a pattern
    int patterned = 0;
    for (unsigned int at = 0u; (at < head.count) && !patterned; at += 1u)
    {
        patterned = EMBED_CALL(cli_language_key_first, PluginsCtx, .pairs = pairs, .at = at) == '?';
    }
    if (patterned && cli_language_pattern_for(&(PluginsCtx){
                         .pairs = pairs, .count = head.count, .path = args->path, .id_out = args->id_out}))
    {
        return 1;
    }
    const char *const dot =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = name,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES}), .byte = '.');
    if ((dot == NULL) || (dot == name))
    {
        return 0;
    }
    EMBED_CALL(cli_on_lower, PluginsCtx, .prefix = "", .text = dot + 1,
               .length = cellul.len(&(CatenaFinitaCfg){.src = dot + 1, .cap = CLI_POOL_BYTES}), .out = key);
    key_bytes = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = sizeof(key)}) + 1ULL;
    for (unsigned int at = 0u; at < head.count; at += 1u)
    {
        const char first = EMBED_CALL(cli_language_key_first, PluginsCtx, .pairs = pairs, .at = at);
        if ((first != '/') && (first != '?') &&
            (g_platform_guard->movable_buf_compare(&(CarcerMovableCfg){
                 .cell = pairs, .offset = CLI_LANGUAGE_KEY_AT(at), .src = key, .bytes = key_bytes}) == 0))
        {
            return EMBED_CALL(cli_language_id_read, PluginsCtx, .pairs = pairs, .at = at, .id_out = args->id_out);
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
    if (!cellul.eq(&(CatenaFinitaCfg){.src = args->plugin->id, .other = code->id, .cap = CLI_POOL_BYTES}))
    {
        return;
    }
    code->found = 1;
    code->user = args->plugin->folder != NULL;
    const char *const folder = (args->plugin->folder != NULL) ? args->plugin->folder : "";
    memor.cpy(&(MemoriaCfg){.dst = code->folder,
                            .src = folder,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_POOL_BYTES}) + 1u});
    g_platform_guard->persistent_buf_release(code->text, g_cli_plugins_owner);
    code->text = g_platform_guard->temporary_buf_alloc(args->plugin->size + 1ULL, g_cli_plugins_owner);
    code->size = (code->text != NULL) ? args->plugin->size : 0ULL;
    if (code->text != NULL)
    {
        // the size of a plugin.json fits an embed_word, because the whole file is in memory
        memor.cpy(&(MemoriaCfg){.dst = code->text, .src = args->plugin->text, .bytes = (embed_word)args->plugin->size});
        code->text[args->plugin->size] = '\0';
    }
}

// the code built into the program of the plugin `id` that comes with orior, or NULL where it runs none
EMBED_INLINE const CliBundled *plugins_code_bundle(const PluginsCtx *args)
{
    for (unsigned int at = 0u; at < g_cli_bundled_count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = g_cli_bundled[at].id, .other = args->id, .cap = CLI_POOL_BYTES}) &&
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
    g_platform_guard->persistent_buf_release(code.text, g_cli_plugins_owner);
    return done;
}
void cli_plugins_slug(const CliPluginsIdNameCfg *args)
{
    const char *from = args->name;
    const char *to = args->name + cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES});
    from += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = from, .at = 0ULL,
                       .size = (unsigned long long)(to - from), .set = CLI_TEXT_WHITE);
    to = from + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = from,
                           .at = (unsigned long long)(to - from), .set = CLI_TEXT_WHITE);
    unsigned long long length = 0ULL;
    {
        const char *at = from;
        while ((at < to) && ((length + 1ULL) < CLI_NAME_MAX_LEN))
        {
            const char character = at[0];
            // Explicit cast reads the byte as its code point
            const int allowed = (ascii.in(&(AsciiCfg){.type = MMGR_ASCII_LOWER, .byte = (uint8_t)character})) ||
                                (ascii.in(&(AsciiCfg){.type = MMGR_ASCII_UPPER, .byte = (uint8_t)character})) ||
                                (ascii.in(&(AsciiCfg){.type = MMGR_ASCII_NUM, .byte = (uint8_t)character}));
            if (allowed)
            {
                // Explicit cast reads the byte as its code point
                args->id_out[length] = (char)ascii.lower(&(AsciiCfg){.byte = (uint8_t)character});
                length += 1ULL;
            }
            else if ((length != 0ULL) && (args->id_out[length - 1u] != '-'))
            {
                args->id_out[length] = '-';
                length += 1ULL;
            }
            // a byte past ASCII that starts or carries a character is no character or digit here, and one run of them
            // one dash
            at += 1;
        }
    }
    length = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = args->id_out, .at = length,
                        .set = "-");
    args->id_out[length] = '\0';
}
