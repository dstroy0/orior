// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// settings.c: a settings file's own keys, the settings a tree's file does not set, `${name}` read, and a chain of
// files each taking the next, read and written as the window reads it
#include "../deploy/devcontainer.h"
#include "../text/text.h"
#include "settings_internal.h"

// the keys of a settings file's own
static const char s_takes[] = "takes";

static const char s_machine[] = "machine";

static const char *const s_systems[3] = {"windows", "linux", "macos"};

// the settings a tree's file does not set, each a key or the start of keys where it ends in `.`: how the programs a
// build makes are signed, the environment a tree's runs take, the checkers run as files change, the window's title
// bar, whether orior is held off the network, what each plugin may read, write and reach, and the cores kept for the
// reader. Each runs a program, names the reader's own, lets orior or a plugin reach out or takes the reader's machine,
// and a tree cloned from anywhere would set it
static const char *const s_tree_held[7] = {"signing.", "environment.",    "checkers",  "titlebar",
                                           "network",  "plugins.allowed", "cores-kept"};

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

int cli_settings_kind_of(const char *key, const char **kind, unsigned long long *length)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL});
    if ((size < 3ull) || (key[0] != '[') || (key[size - 1ull] != ']'))
    {
        return 0;
    }
    *kind = key + 1;
    *length = size - 2ull;
    return 1;
}

int cli_settings_is_file_key(const char *key)
{
    const char *kind = NULL;
    unsigned long long length = 0ull;
    int found = (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = s_takes, .cap = CLI_CELLS_POOL})) ||
                (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = s_machine, .cap = CLI_CELLS_POOL})) ||
                cli_settings_kind_of(key, &kind, &length);
    for (unsigned int at = 0u; !found && (at < 3u); at += 1u)
    {
        found = cellul.eq(&(CatenaFinitaCfg){.src = key, .other = s_systems[at], .cap = CLI_CELLS_POOL});
    }
    return found;
}

int cli_settings_held(const char *key)
{
    for (unsigned int at = 0u; at < 7u; at += 1u)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = s_tree_held[at], .cap = CLI_CELLS_POOL});
        if ((cellul.eq(&(CatenaFinitaCfg){.src = key, .other = s_tree_held[at], .cap = CLI_CELLS_POOL})) ||
            ((s_tree_held[at][length - 1ull] == '.') && cli_text_same(key, s_tree_held[at], length)))
        {
            return 1;
        }
    }
    return 0;
}

void cli_settings_home(char *out)
{
#if defined(_WIN32)
    static const char s_home[] = "USERPROFILE";
#else
    static const char s_home[] = "HOME";
#endif
    if (!query_environment_read(s_home, out, CLI_PATH_LONGEST))
    {
        out[0] = '\0';
    }
}

void cli_settings_expand(const char *text, unsigned long long length,
                         int (*look)(void *context, const char *name, unsigned long long length, CliLine *out),
                         void *context, CliLine *out)
{
    unsigned long long at = 0ull;
    while (at < length)
    {
        unsigned long long open = at;
        while (((open + 1ull) < length) && !((text[open] == '$') && (text[open + 1ull] == '{')))
        {
            open += 1ull;
        }
        if ((open + 1ull) >= length)
        {
            break;
        }
        cli_line_counted(out, text + at, open - at);
        unsigned long long close = open + 2ull;
        while ((close < length) && (text[close] != '}'))
        {
            close += 1ull;
        }
        if (close >= length)
        {
            cli_line_counted(out, text + open, length - open);
            return;
        }
        const unsigned long long mark = out->at;
        if (!look(context, text + open + 2ull, close - open - 2ull, out))
        {
            out->at = mark;
            cli_line_counted(out, text + open, close + 1ull - open);
        }
        at = close + 1ull;
    }
    cli_line_counted(out, text + at, length - at);
}

// `path` with each `.` taken out and each `..` taking out the folder before it, into `out`, which holds
// CLI_PATH_LONGEST
static void cli_tidy(const char *path, char *out)
{
    char parts[64][CLI_NAME_LONGEST];
    unsigned int count = 0u;
    // the root the path starts at: a drive and its slash, or a slash
    unsigned long long root = 0ull;
    if ((path[0] != '\0') && (path[1] == ':'))
    {
        root = ((path[2] == '/') || (path[2] == '\\')) ? 3ull : 2ull;
    }
    else if ((path[0] == '/') || (path[0] == '\\'))
    {
        root = 1ull;
    }
    for (const char *at = path + root; at[0] != '\0';)
    {
        const char *stop = at;
        while ((stop[0] != '\0') && (stop[0] != '/') && (stop[0] != '\\'))
        {
            stop += 1;
        }
        const unsigned long long length = (unsigned long long)(stop - at);
        const int here = (length == 1ull) && (at[0] == '.');
        const int up = (length == 2ull) && (at[0] == '.') && (at[1] == '.');
        if (up && (count > 0u) &&
            (!cellul.eq(&(CatenaFinitaCfg){.src = parts[count - 1u], .other = "..", .cap = CLI_CELLS_POOL})))
        {
            count -= 1u;
        }
        else if ((length != 0ull) && !here && (count < 64u))
        {
            CliLine part = {parts[count], CLI_NAME_LONGEST, 0ull};
            cli_line_counted(&part, at, length);
            cli_line_closed(&part);
            count += 1u;
        }
        at = (stop[0] != '\0') ? (stop + 1) : stop;
    }
    CliLine line = {out, CLI_PATH_LONGEST, 0ull};
    cli_line_counted(&line, path, root);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_line_text(&line, (at != 0u) ? "/" : "");
        cli_line_text(&line, parts[at]);
    }
    cli_line_closed(&line);
}

// what a file's `takes` names: the reader's home folder, or a variable of the system's environment
static int cli_taken_look(void *context, const char *name, unsigned long long length, CliLine *out)
{
    (void)context;
    char value[CLI_PATH_LONGEST];
    if ((length == 4ull) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "home", .bytes = 4u}) == 0))
    {
        cli_settings_home(value);
        cli_line_text(out, value);
        return value[0] != '\0';
    }
    if ((length > 4ull) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "env:", .bytes = 4u}) == 0))
    {
        char variable[CLI_NAME_LONGEST];
        CliLine line = {variable, sizeof(variable), 0ull};
        cli_line_counted(&line, name + 4, length - 4ull);
        cli_line_closed(&line);
        if (query_environment_said(variable, out))
        {
            return 1;
        }
    }
    return 0;
}

// a problem put after the chain's
static void cli_problem(CliChain *chain, const char *named, const char *said)
{
    if (chain->problems < CLI_SETTINGS_PROBLEMS)
    {
        CliLine line = {chain->problem[chain->problems], CLI_PATH_LONGEST * 2u, 0ull};
        cli_line_text(&line, named);
        cli_line_text(&line, said);
        cli_line_closed(&line);
        chain->problems += 1u;
    }
}

// a file's path as a chain shows it: from `root` where it is under it, its slashes leaning forward, else as it is
static void cli_shown(const char *path, const char *root, char *out)
{
    const unsigned long long length =
        (root != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) : 0ull;
    const int under = (root != NULL) && cli_text_same(path, root, length) &&
                      ((path[length] == '/') || (path[length] == '\\') || (path[length] == '\0'));
    const char *from = under ? (path + length) : path;
    from += (under && ((from[0] == '/') || (from[0] == '\\'))) ? 1 : 0;
    CliLine line = {out, CLI_PATH_LONGEST, 0ull};
    for (; from[0] != '\0'; from += 1)
    {
        cli_line_character(&line, (under && (from[0] == '\\')) ? '/' : from[0]);
    }
    cli_line_closed(&line);
}

void cli_settings_chain(const char *path, const char *root, CliChain *chain)
{
    static char s_file[CLI_SETTINGS_FILE];
    static char s_plain[CLI_SETTINGS_FILE];
    static CfgJsonToken s_tokens[CLI_SETTINGS_NODES];
    static char s_seen[CLI_SETTINGS_DEEPEST][CLI_PATH_LONGEST];
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_json_begun(&chain->json, chain->node, CLI_SETTINGS_NODES, chain->text, CLI_SETTINGS_TEXT);
    chain->files = 0u;
    chain->problems = 0u;
    cli_lock_take(&s_lock);
    char next[CLI_PATH_LONGEST];
    cli_tidy(path, next);
    unsigned int seen = 0u;
    int first = 1;
    int going = 1;
    while (going)
    {
        char named[CLI_PATH_LONGEST];
        cli_shown(next, root, named);
        if (chain->files >= CLI_SETTINGS_DEEPEST)
        {
            cli_problem(chain, named, ": more than 16 files take one another, and those past them are not read");
            break;
        }
        char full[CLI_PATH_LONGEST];
        if (!query_path_full(next, full, sizeof(full)))
        {
            memor.cpy(&(MemoriaCfg){.dst = full,
                                    .src = next,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = next, .cap = CLI_CELLS_POOL}) + 1u});
        }
        int taken_twice = 0;
        for (unsigned int at = 0u; !taken_twice && (at < seen); at += 1u)
        {
            taken_twice = cellul.eq(&(CatenaFinitaCfg){.src = s_seen[at], .other = full, .cap = CLI_CELLS_POOL});
        }
        if (taken_twice)
        {
            cli_problem(chain, named, " is taken twice in one chain, and is read once");
            break;
        }
        memor.cpy(&(MemoriaCfg){.dst = s_seen[seen],
                                .src = full,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = full, .cap = CLI_CELLS_POOL}) + 1u});
        seen += 1u;
        const unsigned long long size = qry_member_file_read(next, (unsigned char *)s_file, sizeof(s_file));
        if (size == QRY_MEMBER_UNREAD)
        {
            if (!first || query_is_file(next))
            {
                cli_problem(chain, named, query_is_file(next) ? ": it could not be read" : ": it is not there");
            }
            break;
        }
        first = 0;
        CliLine plain = {s_plain, sizeof(s_plain), 0ull};
        cli_plain_json(s_file, size, &plain);
        char why[CLI_PATH_LONGEST];
        CliLine reason = {why, sizeof(why), 0ull};
        const unsigned int values = cli_line_fits(&plain) ? cli_json_read(&chain->json, s_plain, plain.at, s_tokens,
                                                                          CLI_SETTINGS_NODES, &reason)
                                                          : CLI_JSON_NONE;
        if (values == CLI_JSON_NONE)
        {
            cli_line_closed(&reason);
            char said[CLI_PATH_LONGEST + 2u];
            CliLine told = {said, sizeof(said), 0ull};
            cli_line_text(&told, ": ");
            cli_line_text(&told, why);
            cli_line_closed(&told);
            cli_problem(chain, named, said);
            break;
        }
        if (!cli_json_is(&chain->json, values, CFG_JSON_OBJECT))
        {
            cli_problem(chain, named, " is not an object of settings");
            break;
        }
        memor.cpy(&(MemoriaCfg){.dst = chain->file[chain->files],
                                .src = named,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_CELLS_POOL}) + 1u});
        chain->values[chain->files] = values;
        chain->files += 1u;
        // the file it takes, from its own folder
        const unsigned int takes = cli_json_member(&chain->json, values, s_takes);
        unsigned long long length = 0ull;
        const char *const taken = cli_json_string(&chain->json, takes, &length);
        going = taken != NULL;
        if ((takes != CLI_JSON_NONE) && (taken == NULL))
        {
            cli_problem(chain, named, ": `takes` is the path of a file");
        }
        if (going)
        {
            char expanded[CLI_PATH_LONGEST];
            CliLine line = {expanded, sizeof(expanded), 0ull};
            cli_settings_expand(taken, length, cli_taken_look, NULL, &line);
            cli_line_closed(&line);
            const int absolute =
                (expanded[0] == '/') || (expanded[0] == '\\') || ((expanded[0] != '\0') && (expanded[1] == ':'));
            char folder[CLI_PATH_LONGEST];
            char joined[CLI_PATH_LONGEST];
            CliLine join = {joined, sizeof(joined), 0ull};
            if (!absolute && cli_path_parent(next, folder))
            {
                cli_line_text(&join, folder);
                cli_line_character(&join, '/');
            }
            cli_line_text(&join, expanded);
            cli_line_closed(&join);
            cli_tidy(joined, next);
        }
    }
    cli_lock_give(&s_lock);
    // the files in the order they are taken, the one taken first
    for (unsigned int at = 0u; at < (chain->files / 2u); at += 1u)
    {
        const unsigned int other = chain->files - 1u - at;
        char held[CLI_PATH_LONGEST];
        memor.cpy(
            &(MemoriaCfg){.dst = held,
                          .src = chain->file[at],
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = chain->file[at], .cap = CLI_CELLS_POOL}) + 1u});
        memor.cpy(&(MemoriaCfg){
            .dst = chain->file[at],
            .src = chain->file[other],
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = chain->file[other], .cap = CLI_CELLS_POOL}) + 1u});
        memor.cpy(&(MemoriaCfg){.dst = chain->file[other],
                                .src = held,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = held, .cap = CLI_CELLS_POOL}) + 1u});
        const unsigned int values = chain->values[at];
        chain->values[at] = chain->values[other];
        chain->values[other] = values;
    }
}

void cli_settings_tree(const char *root, CliChain *chain)
{
    char tidy[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    cli_tidy(root, tidy);
    CliLine line = {path, sizeof(path), 0ull};
    cli_line_text(&line, tidy);
    cli_line_character(&line, '/');
    cli_line_text(&line, CLI_SETTINGS_TREE);
    cli_line_closed(&line);
    cli_settings_chain(path, tidy, chain);
}

void cli_settings_reader(CliChain *chain)
{
    char home[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    if (!cli_home_folder(home))
    {
        cli_json_begun(&chain->json, chain->node, CLI_SETTINGS_NODES, chain->text, CLI_SETTINGS_TEXT);
        chain->files = 0u;
        chain->problems = 0u;
        return;
    }
    CliLine line = {path, sizeof(path), 0ull};
    cli_line_text(&line, home);
    cli_line_text(&line, "/settings.json");
    cli_line_closed(&line);
    cli_settings_chain(path, NULL, chain);
}

void cli_settings_chain_written(const CliChain *chain, CliLine *out)
{
    cli_line_text(out, "{\"files\":[");
    for (unsigned int at = 0u; at < chain->files; at += 1u)
    {
        cli_line_text(out, (at != 0u) ? ",{\"file\":" : "{\"file\":");
        cli_json_quoted(out, chain->file[at],
                        cellul.len(&(CatenaFinitaCfg){.src = chain->file[at], .cap = CLI_CELLS_POOL}));
        cli_line_text(out, ",\"values\":");
        cli_json_write(&chain->json, chain->values[at], out, 0u, 0);
        cli_line_character(out, '}');
    }
    cli_line_text(out, "],\"problems\":[");
    for (unsigned int at = 0u; at < chain->problems; at += 1u)
    {
        cli_line_text(out, (at != 0u) ? "," : "");
        cli_json_quoted(out, chain->problem[at],
                        cellul.len(&(CatenaFinitaCfg){.src = chain->problem[at], .cap = CLI_CELLS_POOL}));
    }
    cli_line_text(out, "]}");
}

int cli_settings_chain_read(const char *text, unsigned long long length, CliChain *chain, CliLine *said)
{
    static CfgJsonToken s_tokens[CLI_SETTINGS_NODES];
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_json_begun(&chain->json, chain->node, CLI_SETTINGS_NODES, chain->text, CLI_SETTINGS_TEXT);
    chain->files = 0u;
    chain->problems = 0u;
    cli_lock_take(&s_lock);
    const unsigned int root = cli_json_read(&chain->json, text, length, s_tokens, CLI_SETTINGS_NODES, said);
    cli_lock_give(&s_lock);
    if (!cli_json_is(&chain->json, root, CFG_JSON_OBJECT))
    {
        return 0;
    }
    const unsigned int files = cli_json_member(&chain->json, root, "files");
    for (unsigned int item = cli_json_is(&chain->json, files, CFG_JSON_ARRAY) ? chain->json.node[files].first
                                                                              : CLI_JSON_NONE;
         (item != CLI_JSON_NONE) && (chain->files < CLI_SETTINGS_DEEPEST); item = chain->json.node[item].next)
    {
        const char *const file = cli_json_string(&chain->json, cli_json_member(&chain->json, item, "file"), NULL);
        const unsigned int values = cli_json_member(&chain->json, item, "values");
        if ((file == NULL) || !cli_json_is(&chain->json, values, CFG_JSON_OBJECT))
        {
            continue;
        }
        memor.cpy(&(MemoriaCfg){.dst = chain->file[chain->files],
                                .src = file,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}) + 1u});
        chain->values[chain->files] = values;
        chain->files += 1u;
    }
    const unsigned int problems = cli_json_member(&chain->json, root, "problems");
    for (unsigned int item = cli_json_is(&chain->json, problems, CFG_JSON_ARRAY) ? chain->json.node[problems].first
                                                                                 : CLI_JSON_NONE;
         (item != CLI_JSON_NONE) && (chain->problems < CLI_SETTINGS_PROBLEMS); item = chain->json.node[item].next)
    {
        const char *const problem = cli_json_string(&chain->json, item, NULL);
        if (problem != NULL)
        {
            memor.cpy(
                &(MemoriaCfg){.dst = chain->problem[chain->problems], .bytes = CLI_PATH_LONGEST * 2u, .src = problem});
            chain->problems += 1u;
        }
    }
    return 1;
}
