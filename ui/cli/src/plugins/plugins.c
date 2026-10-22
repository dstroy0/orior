// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugins.c: the plugins found, those that come with orior and the reader's; the language each file opens as; a script
// plugin's code, read from its folder for the reader's and built into the program for one that comes with orior; and a
// name as an id
#include "plugins.h"
#include "../command_line/cells.h"
#include "../text/regexp.h"
#include "../text/text.h"

CliCellsTally g_cli_plugins_cells = CLI_CELLS_TALLY("cli_plugins");

static CliLock s_lock = CLI_LOCK_INIT;

// The reader's plugins' ids in order, in a cell of `room`, `count` of them
typedef struct
{
    char (*name)[CLI_NAME_LONGEST];
    unsigned int count;
    unsigned int room;
} CliUserPlugins;

// whether a folder's entry is a plugin's folder the reader's list keeps
static int cli_user_kept(const char *name, int is_folder)
{
    return is_folder && (cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) < CLI_NAME_LONGEST);
}

static void cli_user_counted(void *context, const char *name, int is_folder)
{
    CliUserPlugins *const user = (CliUserPlugins *)context;
    user->room += (cli_user_kept(name, is_folder) && (user->room < CLI_USER_PLUGINS)) ? 1u : 0u;
}

static void cli_user_each(void *context, const char *name, int is_folder)
{
    CliUserPlugins *const user = (CliUserPlugins *)context;
    if (!cli_user_kept(name, is_folder) || (user->count >= user->room))
    {
        return;
    }
    unsigned int place = user->count;
    while ((place > 0u) && (cli_text_order(user->name[place - 1u], name) > 0))
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

// every plugin handed on, the lock held: the reader's counted, their ids taken into a cell of their count, and each
// plugin.json handed on mapped where it lies
static void cli_each_held(void (*each)(void *context, const CliPlugin *plugin), void *context)
{
    for (unsigned int at = 0u; at < g_cli_bundled_count; at += 1u)
    {
        // a plugin's bytes are its JSON's text
        const CliPlugin plugin = {g_cli_bundled[at].id, "bundled", NULL, (const char *)g_cli_bundled[at].text,
                                  g_cli_bundled[at].size};
        each(context, &plugin);
    }
    char dir[CLI_PATH_LONGEST];
    if (!cli_home_plugins(dir))
    {
        return;
    }
    CliUserPlugins user = {NULL, 0u, 0u};
    query_folder_each(dir, cli_user_counted, &user);
    user.name = (user.room != 0u)
                    ? cli_cell_taken_by(&g_cli_plugins_cells, (unsigned long long)user.room * CLI_NAME_LONGEST,
                                        "the reader's plugins' ids")
                    : NULL;
    user.room = (user.name != NULL) ? user.room : 0u;
    query_folder_each(dir, cli_user_each, &user);
    for (unsigned int at = 0u; at < user.count; at += 1u)
    {
        char folder[CLI_PATH_LONGEST];
        char file[CLI_PATH_LONGEST];
        CliLine line = {folder, sizeof(folder), 0ull};
        cli_line_text(&line, dir);
        cli_line_character(&line, '/');
        cli_line_text(&line, user.name[at]);
        // the folder's path ended before the file's is made from it
        if (cli_line_closed(&line) == 0ull)
        {
            continue;
        }
        CliLine named = {file, sizeof(file), 0ull};
        cli_line_text(&named, folder);
        cli_line_text(&named, "/plugin.json");
        if (cli_line_closed(&named) == 0ull)
        {
            continue;
        }
        unsigned long long size = 0ull;
        const unsigned char *const bytes = cli_file_held(file, &size);
        if (bytes == NULL)
        {
            continue;
        }
        // a plugin's bytes are its JSON's text
        const CliPlugin plugin = {user.name[at], "user", folder, (const char *)bytes, size};
        each(context, &plugin);
        cli_file_let_go(bytes, size);
    }
    cli_cell_given_by(&g_cli_plugins_cells, user.name);
}

void cli_plugins_each(void (*each)(void *context, const CliPlugin *plugin), void *context)
{
    cli_lock_take(&s_lock);
    cli_each_held(each, context);
    cli_lock_give(&s_lock);
}

// `key` put as mapped to `id`, in place of the id it had
static void cli_mapped(CliLanguages *languages, const char *key, unsigned long long key_length, const char *id)
{
    char held[CLI_NAME_LONGEST];
    CliLine line = {held, sizeof(held), 0ull};
    cli_line_counted(&line, key, key_length);
    if (cli_line_closed(&line) == 0ull)
    {
        return;
    }
    for (unsigned int at = 0u; at < languages->count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = languages->pair[at].key, .other = held, .cap = CLI_CELLS_POOL}))
        {
            memor.cpy(&(MemoriaCfg){.dst = languages->pair[at].id,
                                    .src = id,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}) + 1u});
            return;
        }
    }
    if (languages->count == languages->room)
    {
        // the pairs' room doubled, the first CLI_LANGUAGE_PAIRS_FIRST of them
        const unsigned int room = (languages->room == 0u) ? CLI_LANGUAGE_PAIRS_FIRST : (languages->room * 2u);
        CliLanguagePair *const pair =
            cli_cell_taken_by(&g_cli_plugins_cells, (unsigned long long)room * sizeof(*pair), "the languages' keys");
        if (pair == NULL)
        {
            return;
        }
        memor.cpy(&(MemoriaCfg){.dst = pair, .src = languages->pair, .bytes = languages->count * sizeof(*pair)});
        cli_cell_given_by(&g_cli_plugins_cells, languages->pair);
        languages->pair = pair;
        languages->room = room;
    }
    memor.cpy(&(MemoriaCfg){.dst = languages->pair[languages->count].key,
                            .src = held,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = held, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = languages->pair[languages->count].id,
                            .src = id,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}) + 1u});
    languages->count += 1u;
}

// a text in lower case, letter by letter, put after `prefix` into `out`, which holds CLI_NAME_LONGEST: its length
static unsigned long long cli_lowered(const char *prefix, const char *text, unsigned long long length, char *out)
{
    unsigned int letters[CLI_NAME_LONGEST];
    const unsigned long long count = cli_regexp_letters(text, length, letters, CLI_NAME_LONGEST);
    CliLine line = {out, CLI_NAME_LONGEST, 0ull};
    cli_line_text(&line, prefix);
    for (unsigned long long at = 0ull; at < count; at += 1ull)
    {
        cli_utf8_put(&line, cli_regexp_lower(letters[at]));
    }
    return cli_line_closed(&line);
}

// A language plugin's keys mapped to its id
static void cli_language_each(void *context, const CliPlugin *plugin)
{
    CliLanguages *const languages = (CliLanguages *)context;
    CliJson json;
    cli_json_begun(&json, NULL, 0u, NULL, 0ull);
    char why[CLI_PATH_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    const unsigned int root = cli_json_taken(&json, plugin->text, plugin->size, 0u, 0ull, &said);
    const char *const kind =
        (root != CLI_JSON_NONE) ? cli_json_string(&json, cli_json_member(&json, root, "kind"), NULL) : NULL;
    if ((root == CLI_JSON_NONE) ||
        ((kind != NULL) && (!cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "language", .cap = CLI_CELLS_POOL}))))
    {
        cli_json_given(&json);
        return;
    }
    const char *const named = cli_json_string(&json, cli_json_member(&json, root, "id"), NULL);
    const char *const id = (named != NULL) ? named : plugin->id;
    static const char *const s_lists[3] = {"extensions", "names", "paths"};
    static const char *const s_prefixes[3] = {"", "/", "?"};
    for (unsigned int list = 0u; list < 3u; list += 1u)
    {
        const unsigned int items = cli_json_member(&json, root, s_lists[list]);
        for (unsigned int item = cli_json_is(&json, items, CFG_JSON_ARRAY) ? json.node[items].first : CLI_JSON_NONE;
             item != CLI_JSON_NONE; item = json.node[item].next)
        {
            unsigned long long length = 0ull;
            const char *const text = cli_json_string(&json, item, &length);
            if (text == NULL)
            {
                continue;
            }
            char key[CLI_NAME_LONGEST];
            unsigned long long key_length = 0ull;
            if (list == 2u)
            {
                CliLine line = {key, sizeof(key), 0ull};
                cli_line_text(&line, "?");
                cli_line_counted(&line, text, length);
                key_length = cli_line_closed(&line);
            }
            else if (list == 0u)
            {
                // an extension in ASCII lower case, as to_ascii_lowercase reads it
                CliLine line = {key, sizeof(key), 0ull};
                for (unsigned long long at = 0ull; at < length; at += 1ull)
                {
                    cli_line_character(&line,
                                       ((text[at] >= 'A') && (text[at] <= 'Z')) ? (char)(text[at] + 32) : text[at]);
                }
                key_length = cli_line_closed(&line);
            }
            else
            {
                key_length = cli_lowered(s_prefixes[list], text, length, key);
            }
            if (key_length != 0ull)
            {
                cli_mapped(languages, key, key_length, id);
            }
        }
    }
    cli_json_given(&json);
}

void cli_plugins_languages(CliLanguages *languages)
{
    *languages = (CliLanguages){NULL, 0u, 0u};
    cli_plugins_each(cli_language_each, languages);
}

void cli_plugins_languages_given(CliLanguages *languages)
{
    cli_cell_given_by(&g_cli_plugins_cells, languages->pair);
    *languages = (CliLanguages){NULL, 0u, 0u};
}

int cli_plugins_language_for(const CliLanguages *languages, const char *path, char *id)
{
    const char *name = path + cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
    while ((name > path) && (name[-1] != '/') && (name[-1] != '\\'))
    {
        name -= 1;
    }
    char key[CLI_NAME_LONGEST];
    cli_lowered("/", name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), key);
    for (unsigned int at = 0u; at < languages->count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = languages->pair[at].key, .other = key, .cap = CLI_CELLS_POOL}))
        {
            memor.cpy(&(MemoriaCfg){
                .dst = id,
                .src = languages->pair[at].id,
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = languages->pair[at].id, .cap = CLI_CELLS_POOL}) + 1u});
            return 1;
        }
    }
    // the path's letters are no more than its bytes, and the room a pattern is read into is taken only where a key is
    // a pattern
    int patterned = 0;
    for (unsigned int at = 0u; (at < languages->count) && !patterned; at += 1u)
    {
        patterned = languages->pair[at].key[0] == '?';
    }
    const unsigned long long bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
    unsigned int *const slashed =
        patterned ? cli_cell_taken_by(&g_cli_plugins_cells, (bytes + 1ull) * sizeof(unsigned int), "a path's letters")
                  : NULL;
    CliRegexpRoom *const room =
        (slashed != NULL) ? cli_cell_taken_by(&g_cli_plugins_cells, sizeof(CliRegexpRoom), "a language's pattern")
                          : NULL;
    const unsigned long long length = (room != NULL) ? cli_regexp_letters(path, bytes, slashed, bytes + 1ull) : 0ull;
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        slashed[at] = (slashed[at] == '\\') ? '/' : slashed[at];
    }
    int matched = 0;
    for (unsigned int at = 0u; (room != NULL) && (at < languages->count) && !matched; at += 1u)
    {
        if (languages->pair[at].key[0] != '?')
        {
            continue;
        }
        CliRegexp pattern;
        cli_regexp_roomed(&pattern, room);
        char why[CLI_PATH_LONGEST];
        CliLine said = {why, sizeof(why), 0ull};
        if (!cli_regexp_read(&pattern, languages->pair[at].key + 1, "", &said))
        {
            continue;
        }
        CliRegexpSpan groups[CLI_REGEXP_GROUPS + 1u];
        for (unsigned long long place = 0ull; (place < length) && !matched; place += 1ull)
        {
            if (cli_regexp_match_into(&pattern, slashed, length, place, groups))
            {
                memor.cpy(&(MemoriaCfg){
                    .dst = id,
                    .src = languages->pair[at].id,
                    .bytes =
                        cellul.len(&(CatenaFinitaCfg){.src = languages->pair[at].id, .cap = CLI_CELLS_POOL}) + 1u});
                matched = 1;
            }
        }
    }
    cli_cell_given_by(&g_cli_plugins_cells, room);
    cli_cell_given_by(&g_cli_plugins_cells, slashed);
    if (matched)
    {
        return 1;
    }
    const char *const dot =
        cli_text_last(name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), '.');
    if ((dot == NULL) || (dot == name))
    {
        return 0;
    }
    cli_lowered("", dot + 1, cellul.len(&(CatenaFinitaCfg){.src = dot + 1, .cap = CLI_CELLS_POOL}), key);
    for (unsigned int at = 0u; at < languages->count; at += 1u)
    {
        if ((languages->pair[at].key[0] != '/') && (languages->pair[at].key[0] != '?') &&
            (cellul.eq(&(CatenaFinitaCfg){.src = languages->pair[at].key, .other = key, .cap = CLI_CELLS_POOL})))
        {
            memor.cpy(&(MemoriaCfg){
                .dst = id,
                .src = languages->pair[at].id,
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = languages->pair[at].id, .cap = CLI_CELLS_POOL}) + 1u});
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
    char folder[CLI_PATH_LONGEST];
    char *text;
    unsigned long long size;
} CliCodeFound;

static void plugins_code_each(void *context, const CliPlugin *plugin)
{
    CliCodeFound *const code = (CliCodeFound *)context;
    if (!cellul.eq(&(CatenaFinitaCfg){.src = plugin->id, .other = code->id, .cap = CLI_CELLS_POOL}))
    {
        return;
    }
    code->found = 1;
    code->user = plugin->folder != NULL;
    const char *const folder = (plugin->folder != NULL) ? plugin->folder : "";
    memor.cpy(&(MemoriaCfg){.dst = code->folder,
                            .src = folder,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}) + 1u});
    cli_cell_given(code->text);
    code->text = cli_cell_taken(plugin->size + 1ull, "a plugin.json whose code is read");
    code->size = (code->text != NULL) ? plugin->size : 0ull;
    if (code->text != NULL)
    {
        // a plugin.json's size fits a size_t, as it was read whole
        memor.cpy(&(MemoriaCfg){.dst = code->text, .src = plugin->text, .bytes = (size_t)plugin->size});
        code->text[plugin->size] = '\0';
    }
}

// the code built into the program of the plugin `id` that comes with orior, or NULL where it runs none
static const CliBundled *plugins_code_bundled(const char *id)
{
    for (unsigned int at = 0u; at < g_cli_bundled_count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = g_cli_bundled[at].id, .other = id, .cap = CLI_CELLS_POOL}) &&
            (g_cli_bundled[at].code != NULL))
        {
            return &g_cli_bundled[at];
        }
    }
    return NULL;
}

int cli_plugins_code_of(const char *id, CliLine *out, CliLine *said)
{
    CliCodeFound code;
    memor.set(&(MemoriaCfg){.dst = &code, .val = 0, .bytes = sizeof(code)});
    code.id = id;
    cli_plugins_each(plugins_code_each, &code);
    const CliBundled *const bundled = (code.found && !code.user) ? plugins_code_bundled(id) : NULL;
    int done = 0;
    if (!code.found)
    {
        cli_line_text(said, "no plugin is named ");
        cli_line_text(said, id);
    }
    else if (code.user && (code.text != NULL))
    {
        done = cli_plugins_code_in(id, code.folder, code.text, code.size, out, said);
    }
    else if (bundled != NULL)
    {
        cli_line_counted(out, (const char *)bundled->code, bundled->code_size);
        done = cli_line_fits(out);
    }
    else
    {
        cli_line_text(said, id);
        cli_line_text(said, code.user ? ": its plugin.json does not read" : " comes with orior and runs no code");
    }
    cli_cell_given(code.text);
    return done;
}
void cli_plugins_slug(const char *name, char *id)
{
    const char *from = name;
    const char *to = name + cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    while ((from < to) && ((from[0] == ' ') || ((from[0] >= '\t') && (from[0] <= '\r'))))
    {
        from += 1;
    }
    while ((to > from) && ((to[-1] == ' ') || ((to[-1] >= '\t') && (to[-1] <= '\r'))))
    {
        to -= 1;
    }
    unsigned long long length = 0ull;
    for (const char *at = from; (at < to) && ((length + 1ull) < CLI_NAME_LONGEST); at += 1)
    {
        const char letter = at[0];
        const int kept = ((letter >= 'a') && (letter <= 'z')) || ((letter >= 'A') && (letter <= 'Z')) ||
                         ((letter >= '0') && (letter <= '9'));
        if (kept)
        {
            id[length] = ((letter >= 'A') && (letter <= 'Z')) ? (char)(letter + 32) : letter;
            length += 1ull;
        }
        else if ((length != 0ull) && (id[length - 1u] != '-'))
        {
            id[length] = '-';
            length += 1ull;
        }
        // a byte past ASCII that starts or carries a letter is no letter or digit here, and one run of them one dash
    }
    while ((length != 0ull) && (id[length - 1u] == '-'))
    {
        length -= 1ull;
    }
    id[length] = '\0';
}
