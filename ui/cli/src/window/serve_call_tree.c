// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// serve_call_tree.c: the calls of the tree as a whole: the tree open, set and told; the page's answer to a command
// line; the definitions; a folder's entries, the files found by name and every file; a file's indentation; the patterns
// of a part of the window; the tree's files and what they declare read behind the window, from the tree's opening, and
// whether each is read yet; a quick open's list held on this side, matched as each letter is typed, and let go; the
// reading of the tree's symbols ended where it stands, Go to Symbol answering from what it read; a search of every
// line; and the folders mounted beside the tree
#include "../code/symbols.h"
#include "../editor/defs.h"
#include "../editor/editorconfig.h"
#include "../editor/files.h"
#include "../editor/picks.h"
#include "../text/patterns.h"
#include "serve_internal.h"

// the most declarations one search of the tree's symbols gives, and the most lines of patterns and recent files a call
// gives
#define CLI_SERVE_SYMBOLS 200u
#define CLI_SERVE_LINES 4096u

// the tree's symbols brought up to date from its files as the listing gives them; left as they are where no room for
// the listing is left
static void tree_symbols_refreshed(const char *root)
{
    char why[CLI_NAME_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    CliServeFiles *const listed = cli_serve_files_listed(root, &said);
    if (listed != NULL)
    {
        cli_symbols_refresh(root, listed->paths, listed->count);
    }
    cli_serve_files_given(listed);
}

static int root_get(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    char root[CLI_PATH_LONGEST];
    if (cli_serve_root_now(root))
    {
        cli_json_quoted_lossy(out, (const unsigned char *)root,
                              cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}));
    }
    else
    {
        cli_line_text(out, "null");
    }
    return 1;
}

static int root_set(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const path = cli_serve_string(args, "path", said);
    return (path != NULL) && cli_serve_root_set(path, out, said);
}

static int command_answer(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    int done = 0;
    const char *const id = cli_serve_string(args, "id", said);
    if ((id == NULL) || !cli_serve_bool(args, "done", &done, said) ||
        !cli_serve_command_answer(id, done, cli_serve_string_or(args, "said", ""), said))
    {
        return 0;
    }
    cli_line_text(out, "null");
    return 1;
}

static int definitions_read(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    char root[CLI_PATH_LONGEST];
    if (!cli_serve_tree(root, said))
    {
        return 0;
    }
    cli_defs_read(root, out);
    return 1;
}

static int tree_list(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    const char *const dir = cli_serve_string(args, "dir", said);
    return (dir != NULL) && cli_serve_tree(root, said) && cli_files_list(root, dir, out, said);
}

static int tree_find(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    const char *const query = cli_serve_string(args, "query", said);
    if ((query == NULL) || !cli_serve_tree(root, said))
    {
        return 0;
    }
    cli_files_find(root, query, out);
    return 1;
}

static int tree_files(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    char root[CLI_PATH_LONGEST];
    if (!cli_serve_tree(root, said))
    {
        return 0;
    }
    CliServeFiles *const listed = cli_serve_files_listed(root, said);
    if (listed == NULL)
    {
        return 0;
    }
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < listed->count; at += 1u)
    {
        const char *const path = listed->paths[at];
        cli_line_text(out, (at > 0u) ? "," : "");
        cli_json_quoted_lossy(out, (const unsigned char *)path,
                              cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    }
    cli_line_character(out, ']');
    cli_serve_files_given(listed);
    return 1;
}

static int files_find(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    static const char *s_recent[CLI_SERVE_LINES];
    char root[CLI_PATH_LONGEST];
    unsigned int recents = 0u;
    unsigned long long most = 0ull;
    const char *const query = cli_serve_string(args, "query", said);
    if ((query == NULL) || !cli_serve_strings(args, "recent", s_recent, CLI_SERVE_LINES, &recents, said) ||
        !cli_serve_whole(args, "most", &most, said) || !cli_serve_tree(root, said))
    {
        return 0;
    }
    // the most is cut to its low word, as no list holds more
    cli_files_ranked(root, query, s_recent, recents, (unsigned int)most, cli_serve_bool_or(args, "fuzzy", 1), out);
    return 1;
}

static int indent_for(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    char full[CLI_PATH_LONGEST];
    const char *const path = cli_serve_string(args, "path", said);
    if ((path == NULL) || !cli_serve_tree(root, said) || !cli_inside(root, path, full, said))
    {
        return 0;
    }
    CliIndent indent = {-1, 0, 0u};
    cli_indent(full, &indent);
    cli_line_text(out, (indent.tabs < 0) ? "{\"tabs\":null" : (indent.tabs ? "{\"tabs\":true" : "{\"tabs\":false"));
    cli_line_text(out, ",\"size\":");
    if (indent.sized)
    {
        cli_line_decimal(out, indent.size, 1u);
    }
    else
    {
        cli_line_text(out, "null");
    }
    cli_line_character(out, '}');
    return 1;
}

static int patterns_set(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    static const char *const s_parts[CLI_PARTS] = {"explorer", "search", "watching"};
    static const char *s_lines[CLI_SERVE_LINES];
    unsigned int count = 0u;
    const char *const part = cli_serve_string(args, "part", said);
    if ((part == NULL) || !cli_serve_strings(args, "lines", s_lines, CLI_SERVE_LINES, &count, said))
    {
        return 0;
    }
    unsigned int at = 0u;
    while ((at < CLI_PARTS) &&
           (!cellul.eq(&(CatenaFinitaCfg){.src = s_parts[at], .other = part, .cap = CLI_CELLS_POOL})))
    {
        at += 1u;
    }
    if (at == CLI_PARTS)
    {
        cli_line_text(said, "part: unknown variant `");
        cli_line_text(said, part);
        cli_line_text(said, "`, expected one of `explorer`, `search`, `watching`");
        return 0;
    }
    const int changed = cli_patterns_set(at, s_lines, count);
    if (changed && (at == CLI_PART_SEARCH))
    {
        cli_files_forget_held();
    }
    cli_line_text(out, changed ? "true" : "false");
    return 1;
}

static int tree_read(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    char root[CLI_PATH_LONGEST];
    if (!cli_serve_tree(root, said))
    {
        return 0;
    }
    cli_files_read_behind(root);
    tree_symbols_refreshed(root);
    cli_line_text(out, "null");
    return 1;
}

static int tree_unread(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    char root[CLI_PATH_LONGEST];
    if (!cli_serve_tree(root, said))
    {
        return 0;
    }
    unsigned long long done = 0ull;
    unsigned long long of = 0ull;
    if (!cli_symbols_progress(&done, &of))
    {
        done = 0ull;
        of = 0ull;
    }
    cli_line_text(out, cli_files_unread(root) ? "{\"files\":true" : "{\"files\":false");
    cli_line_text(out, cli_symbols_unread() ? ",\"symbols\":true,\"done\":" : ",\"symbols\":false,\"done\":");
    cli_line_decimal(out, done, 1u);
    cli_line_text(out, ",\"of\":");
    cli_line_decimal(out, of, 1u);
    cli_line_character(out, '}');
    return 1;
}

static int pick_hold(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    unsigned long long length = 0ull;
    const char *const texts = cli_serve_string(args, "texts", said);
    if (texts == NULL)
    {
        return 0;
    }
    (void)cli_json_string(args->json, cli_serve_arg(args, "texts"), &length);
    cli_line_decimal(out, cli_picks_hold(texts, length), 1u);
    return 1;
}

static int pick_find(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    unsigned long long id = 0ull;
    unsigned long long most = 0ull;
    const char *const query = cli_serve_string(args, "query", said);
    return (query != NULL) && cli_serve_whole(args, "id", &id, said) && cli_serve_whole(args, "most", &most, said) &&
           cli_picks_find(id, query, most, out, said);
}

static int pick_forget(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    unsigned long long id = 0ull;
    if (!cli_serve_whole(args, "id", &id, said))
    {
        return 0;
    }
    cli_picks_forget(id);
    cli_line_text(out, "null");
    return 1;
}

static int tree_read_stop(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    cli_symbols_stop();
    cli_line_text(out, "null");
    return 1;
}

static int symbols_find(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char root[CLI_PATH_LONGEST];
    const char *const query = cli_serve_string(args, "query", said);
    if ((query == NULL) || !cli_serve_tree(root, said))
    {
        return 0;
    }
    tree_symbols_refreshed(root);
    cli_symbols_find(query, CLI_SERVE_SYMBOLS, out);
    return 1;
}

static int tree_search(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    static const char *s_set[CLI_SERVE_LINES];
    static CliPatterns s_patterns;
    static CliLock s_patterns_lock = CLI_LOCK_INIT;
    char root[CLI_PATH_LONGEST];
    unsigned int count = 0u;
    const char *const query = cli_serve_string(args, "query", said);
    const unsigned int how = cli_serve_arg(args, "how");
    if ((query == NULL) || !cli_serve_strings_or(args, "set", s_set, CLI_SERVE_LINES, &count, said) ||
        !cli_serve_tree(root, said))
    {
        return 0;
    }
    if (!cli_json_is(args->json, how, CFG_JSON_OBJECT))
    {
        cli_line_text(said, "how: invalid type: null, expected struct Searching");
        return 0;
    }
    const CliServeArgs read = {args->json, how};
    const CliSearching searching = {cli_serve_bool_or(&read, "case", 0), cli_serve_bool_or(&read, "word", 0),
                                    cli_serve_bool_or(&read, "regex", 0)};
    cli_lock_take(&s_patterns_lock);
    cli_patterns_read_set(s_set, count, &s_patterns);
    const int done = cli_files_search(root, query, searching, &s_patterns, out, said);
    cli_lock_give(&s_patterns_lock);
    return done;
}

static int tree_mount(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    char name[CLI_NAME_LONGEST];
    const char *const path = cli_serve_string(args, "path", said);
    if ((path == NULL) || !cli_mounted(path, name, said))
    {
        return 0;
    }
    cli_json_quoted_lossy(out, (const unsigned char *)name,
                          cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    return 1;
}

static int tree_unmount(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    const char *const name = cli_serve_string(args, "name", said);
    if (name == NULL)
    {
        return 0;
    }
    cli_unmounted(name);
    cli_line_text(out, "null");
    return 1;
}

static int tree_mounts(const CliServeArgs *args, CliLine *out, CliLine *said)
{
    (void)args;
    (void)said;
    const char *names[CLI_MOUNTS_MOST];
    const char *paths[CLI_MOUNTS_MOST];
    const unsigned int count = cli_mounts(names, paths, CLI_MOUNTS_MOST);
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_line_text(out, (at > 0u) ? ",{\"name\":" : "{\"name\":");
        cli_json_quoted_lossy(out, (const unsigned char *)names[at],
                              cellul.len(&(CatenaFinitaCfg){.src = names[at], .cap = CLI_CELLS_POOL}));
        cli_line_text(out, ",\"path\":");
        cli_json_quoted_lossy(out, (const unsigned char *)paths[at],
                              cellul.len(&(CatenaFinitaCfg){.src = paths[at], .cap = CLI_CELLS_POOL}));
        cli_line_text(out, query_is_folder(paths[at]) ? ",\"dir\":true}" : ",\"dir\":false}");
    }
    cli_line_character(out, ']');
    return 1;
}

const CliServeCall g_cli_serve_tree_calls[] = {
    {"root_get", root_get},
    {"root_set", root_set},
    {"command_answer", command_answer},
    {"definitions_read", definitions_read},
    {"tree_list", tree_list},
    {"tree_find", tree_find},
    {"tree_files", tree_files},
    {"files_find", files_find},
    {"indent_for", indent_for},
    {"patterns_set", patterns_set},
    {"tree_read", tree_read},
    {"tree_unread", tree_unread},
    {"pick_hold", pick_hold},
    {"pick_find", pick_find},
    {"pick_forget", pick_forget},
    {"tree_read_stop", tree_read_stop},
    {"symbols_find", symbols_find},
    {"tree_search", tree_search},
    {"tree_mount", tree_mount},
    {"tree_unmount", tree_unmount},
    {"tree_mounts", tree_mounts},
};
const unsigned int g_cli_serve_tree_count = sizeof(g_cli_serve_tree_calls) / sizeof(g_cli_serve_tree_calls[0]);
