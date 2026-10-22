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
// bar, whether orior_cli is held off the network, what each plugin may read, write and reach, and the cores kept for
// the reader. Each runs a program, names the reader's own, lets orior_cli or a plugin reach out or takes the reader's
// machine, and a tree cloned from anywhere would set it
static const char *const s_tree_held[7] = {"signing.", "environment.",    "checkers",  "titlebar",
                                           "network",  "plugins.allowed", "cores-kept"};

// the most bytes of a settings file read
#define CLI_SETTINGS_FILE (1u << 20u)

CliCellsTally g_cli_settings_cells = CLI_CELLS_TALLY("cli_settings");

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

// A file of a chain as the chain is walked: the path it is shown by, its text with its comments taken out, in a cell
// no larger than the file, that text's bytes, and the nodes it is read into, its tokens and one over
typedef struct
{
    char named[CLI_PATH_LONGEST];
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
    char seen[CLI_SETTINGS_DEEPEST][CLI_PATH_LONGEST];
} CliChainWalk;

// the files walked read into the chain's tree in the order walked, the tree in a cell of their nodes and bytes, the
// tokens in a cell of the most one file holds; a problem where the cells are refused
static void cli_chain_read(CliChain *chain, const CliChainWalk *walk)
{
    unsigned int nodes = 0u;
    unsigned int most = 0u;
    unsigned long long text = 0ull;
    for (unsigned int at = 0u; at < walk->count; at += 1u)
    {
        nodes += walk->file[at].nodes;
        most = (walk->file[at].nodes > most) ? walk->file[at].nodes : most;
        text += walk->file[at].size;
    }
    if (walk->count == 0u)
    {
        return;
    }
    CfgJsonToken *const tokens = cli_cell_taken_by(
        &g_cli_settings_cells, (unsigned long long)most * sizeof(CfgJsonToken), "a settings file's tokens");
    if ((tokens == NULL) || !cli_json_held(&chain->json, nodes, text, "a chain of settings files"))
    {
        cli_cell_given_by(&g_cli_settings_cells, tokens);
        cli_problem(chain, walk->file[0].named, ": the cells its chain is read into could not be taken");
        return;
    }
    for (unsigned int at = 0u; at < walk->count; at += 1u)
    {
        const CliChainFile *const file = &walk->file[at];
        char why[CLI_PATH_LONGEST];
        CliLine reason = {why, sizeof(why), 0ull};
        const unsigned int values = cli_json_read(&chain->json, file->plain, file->size, tokens, most, &reason);
        if (values == CLI_JSON_NONE)
        {
            break;
        }
        memor.cpy(
            &(MemoriaCfg){.dst = chain->file[chain->files],
                          .src = file->named,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = file->named, .cap = CLI_CELLS_POOL}) + 1u});
        chain->values[chain->files] = values;
        chain->files += 1u;
    }
    cli_cell_given_by(&g_cli_settings_cells, tokens);
}

void cli_settings_chain(const char *path, const char *root, CliChain *chain)
{
    cli_json_begun(&chain->json, NULL, 0u, NULL, 0ull);
    chain->files = 0u;
    chain->problems = 0u;
    CliChainWalk *const walk = cli_cell_taken_by(&g_cli_settings_cells, sizeof(*walk), "a chain of settings walked");
    if (walk == NULL)
    {
        return;
    }
    walk->count = 0u;
    char next[CLI_PATH_LONGEST];
    cli_tidy(path, next);
    int first = 1;
    int going = 1;
    while (going)
    {
        char named[CLI_PATH_LONGEST];
        cli_shown(next, root, named);
        if (walk->count >= CLI_SETTINGS_DEEPEST)
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
        for (unsigned int at = 0u; !taken_twice && (at < walk->count); at += 1u)
        {
            taken_twice = cellul.eq(&(CatenaFinitaCfg){.src = walk->seen[at], .other = full, .cap = CLI_CELLS_POOL});
        }
        if (taken_twice)
        {
            cli_problem(chain, named, " is taken twice in one chain, and is read once");
            break;
        }
        memor.cpy(&(MemoriaCfg){.dst = walk->seen[walk->count],
                                .src = full,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = full, .cap = CLI_CELLS_POOL}) + 1u});
        unsigned long long size = 0ull;
        const unsigned char *const bytes = cli_file_held(next, &size);
        if ((bytes == NULL) || (size > CLI_SETTINGS_FILE))
        {
            if (bytes != NULL)
            {
                cli_file_let_go(bytes, size);
            }
            if (!first || query_is_file(next))
            {
                cli_problem(chain, named, query_is_file(next) ? ": it could not be read" : ": it is not there");
            }
            break;
        }
        first = 0;
        // a file's text with its comments taken out is no longer than the file
        char *const plain_text = cli_cell_taken_by(&g_cli_settings_cells, size + 1ull, "a settings file's text");
        CliLine plain = {plain_text, (plain_text != NULL) ? (size + 1ull) : 0ull, 0ull};
        if (plain_text != NULL)
        {
            // the file's bytes are its text
            cli_plain_json((const char *)bytes, size, &plain);
        }
        cli_file_let_go(bytes, size);
        char why[CLI_PATH_LONGEST];
        CliLine reason = {why, sizeof(why), 0ull};
        CliJson own;
        cli_json_begun(&own, NULL, 0u, NULL, 0ull);
        const unsigned int values = ((plain_text != NULL) && cli_line_fits(&plain))
                                        ? cli_json_taken(&own, plain_text, plain.at, 0u, 0ull, &reason)
                                        : CLI_JSON_NONE;
        if ((values == CLI_JSON_NONE) || !cli_json_is(&own, values, CFG_JSON_OBJECT))
        {
            cli_line_closed(&reason);
            char said[CLI_PATH_LONGEST + 2u];
            CliLine told = {said, sizeof(said), 0ull};
            cli_line_text(&told, ": ");
            cli_line_text(&told, why);
            cli_line_closed(&told);
            cli_problem(chain, named, (values == CLI_JSON_NONE) ? said : " is not an object of settings");
            cli_json_given(&own);
            cli_cell_given_by(&g_cli_settings_cells, plain_text);
            break;
        }
        CliChainFile *const walked = &walk->file[walk->count];
        memor.cpy(&(MemoriaCfg){.dst = walked->named,
                                .src = named,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_CELLS_POOL}) + 1u});
        walked->plain = plain_text;
        walked->size = plain.at;
        walked->nodes = cli_json_tokens_counted(plain_text, plain.at) + 1u;
        walk->count += 1u;
        // the file it takes, from its own folder
        const unsigned int takes = cli_json_member(&own, values, s_takes);
        unsigned long long length = 0ull;
        const char *const taken = cli_json_string(&own, takes, &length);
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
        cli_json_given(&own);
    }
    cli_chain_read(chain, walk);
    for (unsigned int at = 0u; at < walk->count; at += 1u)
    {
        cli_cell_given_by(&g_cli_settings_cells, walk->file[at].plain);
    }
    cli_cell_given_by(&g_cli_settings_cells, walk);
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
        cli_json_begun(&chain->json, NULL, 0u, NULL, 0ull);
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
