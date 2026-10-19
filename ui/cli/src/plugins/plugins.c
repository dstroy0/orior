// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugins.c: the plugins found, those that come with orior and the reader's; the language each file opens as; a
// script plugin's code; and a name as an id
#include "plugins.h"
#include "../text/regexp.h"
#include "../text/text.h"

// the most bytes of a reader's plugin.json, of a script's code, and of the tree a plugin is read into
#define CLI_PLUGIN_BYTES (1u << 22u)

#define CLI_PLUGIN_NODES 65536u

#define CLI_PLUGIN_TEXT (1u << 22u)

#define CLI_PLUGIN_TOKENS 65536u

// the reader's plugins' ids in order, a plugin's text, and a plugin read as JSON, held under the lock
static char s_user[CLI_USER_PLUGINS][CLI_NAME_LONGEST];
static unsigned int s_user_count;
static char s_text[CLI_PLUGIN_BYTES];
static CliJsonNode s_nodes[CLI_PLUGIN_NODES];
static char s_json_text[CLI_PLUGIN_TEXT];
static CfgJsonToken s_tokens[CLI_PLUGIN_TOKENS];
static CliLock s_lock = CLI_LOCK_INIT;

static void cli_user_each(void *context, const char *name, int is_folder)
{
    (void)context;
    if (!is_folder || (s_user_count >= CLI_USER_PLUGINS) ||
        (cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) >= CLI_NAME_LONGEST))
    {
        return;
    }
    unsigned int place = s_user_count;
    while ((place > 0u) && (cli_text_order(s_user[place - 1u], name) > 0))
    {
        memor.cpy(&(MemoriaCfg){
            .dst = s_user[place],
            .src = s_user[place - 1u],
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_user[place - 1u], .cap = CLI_CELLS_POOL}) + 1u});
        place -= 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = s_user[place],
                            .src = name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
    s_user_count += 1u;
}

// every plugin handed on, the lock held: the reader's read one at a time into the one text
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
    s_user_count = 0u;
    query_folder_each(dir, cli_user_each, NULL);
    for (unsigned int at = 0u; at < s_user_count; at += 1u)
    {
        char folder[CLI_PATH_LONGEST];
        char file[CLI_PATH_LONGEST];
        CliLine line = {folder, sizeof(folder), 0ull};
        cli_line_text(&line, dir);
        cli_line_character(&line, '/');
        cli_line_text(&line, s_user[at]);
        CliLine named = {file, sizeof(file), 0ull};
        cli_line_text(&named, folder);
        cli_line_text(&named, "/plugin.json");
        if ((cli_line_closed(&line) == 0ull) || (cli_line_closed(&named) == 0ull))
        {
            continue;
        }
        const unsigned long long size = qry_member_file_read(file, (unsigned char *)s_text, sizeof(s_text) - 1u);
        if (size == QRY_MEMBER_UNREAD)
        {
            continue;
        }
        s_text[size] = '\0';
        const CliPlugin plugin = {s_user[at], "user", folder, s_text, size};
        each(context, &plugin);
    }
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
        if (cellul.eq(&(CatenaFinitaCfg){.src = languages->key[at], .other = held, .cap = CLI_CELLS_POOL}))
        {
            memor.cpy(&(MemoriaCfg){.dst = languages->id[at],
                                    .src = id,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}) + 1u});
            return;
        }
    }
    if (languages->count < CLI_LANGUAGE_KEYS)
    {
        memor.cpy(&(MemoriaCfg){.dst = languages->key[languages->count],
                                .src = held,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = held, .cap = CLI_CELLS_POOL}) + 1u});
        memor.cpy(&(MemoriaCfg){.dst = languages->id[languages->count],
                                .src = id,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}) + 1u});
        languages->count += 1u;
    }
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
    cli_json_begun(&json, s_nodes, CLI_PLUGIN_NODES, s_json_text, CLI_PLUGIN_TEXT);
    char why[CLI_PATH_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    const unsigned int root = cli_json_read(&json, plugin->text, plugin->size, s_tokens, CLI_PLUGIN_TOKENS, &said);
    if (root == CLI_JSON_NONE)
    {
        return;
    }
    const char *const kind = cli_json_string(&json, cli_json_member(&json, root, "kind"), NULL);
    if ((kind != NULL) && (!cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "language", .cap = CLI_CELLS_POOL})))
    {
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
}

void cli_plugins_languages(CliLanguages *languages)
{
    languages->count = 0u;
    cli_plugins_each(cli_language_each, languages);
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
        if (cellul.eq(&(CatenaFinitaCfg){.src = languages->key[at], .other = key, .cap = CLI_CELLS_POOL}))
        {
            memor.cpy(&(MemoriaCfg){
                .dst = id,
                .src = languages->id[at],
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = languages->id[at], .cap = CLI_CELLS_POOL}) + 1u});
            return 1;
        }
    }
    static CliRegexpRoom s_room;
    static unsigned int s_slashed[CLI_PATH_LONGEST];
    static CliLock s_matching = CLI_LOCK_INIT;
    cli_lock_take(&s_matching);
    const unsigned long long length = cli_regexp_letters(
        path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), s_slashed, CLI_PATH_LONGEST);
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        s_slashed[at] = (s_slashed[at] == '\\') ? '/' : s_slashed[at];
    }
    for (unsigned int at = 0u; at < languages->count; at += 1u)
    {
        if (languages->key[at][0] != '?')
        {
            continue;
        }
        CliRegexp pattern;
        cli_regexp_roomed(&pattern, &s_room);
        char why[CLI_PATH_LONGEST];
        CliLine said = {why, sizeof(why), 0ull};
        if (!cli_regexp_read(&pattern, languages->key[at] + 1, "", &said))
        {
            continue;
        }
        CliRegexpSpan groups[CLI_REGEXP_GROUPS + 1u];
        for (unsigned long long place = 0ull; place < length; place += 1ull)
        {
            if (cli_regexp_match_into(&pattern, s_slashed, length, place, groups))
            {
                memor.cpy(&(MemoriaCfg){
                    .dst = id,
                    .src = languages->id[at],
                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = languages->id[at], .cap = CLI_CELLS_POOL}) + 1u});
                cli_lock_give(&s_matching);
                return 1;
            }
        }
    }
    cli_lock_give(&s_matching);
    const char *const dot =
        cli_text_last(name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), '.');
    if ((dot == NULL) || (dot == name))
    {
        return 0;
    }
    cli_lowered("", dot + 1, cellul.len(&(CatenaFinitaCfg){.src = dot + 1, .cap = CLI_CELLS_POOL}), key);
    for (unsigned int at = 0u; at < languages->count; at += 1u)
    {
        if ((languages->key[at][0] != '/') && (languages->key[at][0] != '?') &&
            (cellul.eq(&(CatenaFinitaCfg){.src = languages->key[at], .other = key, .cap = CLI_CELLS_POOL})))
        {
            memor.cpy(&(MemoriaCfg){
                .dst = id,
                .src = languages->id[at],
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = languages->id[at], .cap = CLI_CELLS_POOL}) + 1u});
            return 1;
        }
    }
    return 0;
}

// The last plugin of an id found: its folder and its plugin.json's `main`, kept while each is handed
typedef struct
{
    const char *id;
    int found;
    int user;
    char folder[CLI_PATH_LONGEST];
    char main[CLI_PATH_LONGEST];
    int read;
} CliCodeFound;

static void cli_code_each(void *context, const CliPlugin *plugin)
{
    CliCodeFound *const code = (CliCodeFound *)context;
    if (!cellul.eq(&(CatenaFinitaCfg){.src = plugin->id, .other = code->id, .cap = CLI_CELLS_POOL}))
    {
        return;
    }
    code->found = 1;
    code->user = plugin->folder != NULL;
    memor.cpy(
        &(MemoriaCfg){.dst = code->folder,
                      .src = (plugin->folder != NULL) ? plugin->folder : "",
                      .bytes = cellul.len(&(CatenaFinitaCfg){.src = (plugin->folder != NULL) ? plugin->folder : "",
                                                             .cap = CLI_CELLS_POOL}) +
                               1u});
    CliJson json;
    cli_json_begun(&json, s_nodes, CLI_PLUGIN_NODES, s_json_text, CLI_PLUGIN_TEXT);
    char why[CLI_PATH_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    const unsigned int root = cli_json_read(&json, plugin->text, plugin->size, s_tokens, CLI_PLUGIN_TOKENS, &said);
    code->read = root != CLI_JSON_NONE;
    const char *const main = code->read ? cli_json_string(&json, cli_json_member(&json, root, "main"), NULL) : NULL;
    memor.cpy(
        &(MemoriaCfg){.dst = code->main, .bytes = sizeof(code->main), .src = (main != NULL) ? main : "plugin.js"});
}

int cli_plugins_code_of(const char *id, CliLine *out, CliLine *said)
{
    CliCodeFound code;
    memor.set(&(MemoriaCfg){.dst = &code, .val = 0, .bytes = sizeof(code)});
    code.id = id;
    cli_plugins_each(cli_code_each, &code);
    if (!code.found)
    {
        cli_line_text(said, "no plugin is named ");
        cli_line_text(said, id);
        return 0;
    }
    if (!code.read)
    {
        cli_line_text(said, id);
        cli_line_text(said, ": its plugin.json does not read");
        return 0;
    }
    // each part of the name a folder's own: none empty, `.` or `..`, and the name not whole
    int own = (code.main[0] != '/') && (code.main[0] != '\\') &&
              (cellul.chr(&(CatenaFinitaCfg){.src = code.main, .cap = CLI_CELLS_POOL, .byte = ':'}) == NULL);
    const char *part = code.main;
    while (own && (part[0] != '\0'))
    {
        const char *end = part;
        while ((end[0] != '\0') && (end[0] != '/') && (end[0] != '\\'))
        {
            end += 1;
        }
        const unsigned long long length = (unsigned long long)(end - part);
        own = (length != 0ull) && !((length == 1u) && (part[0] == '.')) &&
              !((length == 2u) && (part[0] == '.') && (part[1] == '.'));
        part = (end[0] != '\0') ? (end + 1) : end;
    }
    if (!own)
    {
        cli_line_text(said, id);
        cli_line_text(said, " names ");
        cli_line_text(said, code.main);
        cli_line_text(said, ", which is not a file of its own folder");
        return 0;
    }
    if (!code.user)
    {
        cli_line_text(said, id);
        cli_line_text(said, " comes with orior and runs no code");
        return 0;
    }
    char file[CLI_PATH_LONGEST];
    CliLine line = {file, sizeof(file), 0ull};
    cli_line_text(&line, code.folder);
    cli_line_character(&line, '/');
    cli_line_text(&line, code.main);
    const unsigned long long room = cli_line_fits(out) ? (out->capacity - out->at) : 0ull;
    const unsigned long long size = (cli_line_closed(&line) != 0ull)
                                        ? qry_member_file_read(file, (unsigned char *)(out->out + out->at), room)
                                        : QRY_MEMBER_UNREAD;
    if (size == QRY_MEMBER_UNREAD)
    {
        cli_line_text(said, file);
        cli_line_text(said, ": it could not be read");
        return 0;
    }
    out->at += size;
    return 1;
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
