// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// toolchains.c: the manifest read, orior's own and the reader's, a tool's program found in a folder, and the folders a
// pattern of toolchains.json names
#include "../command_line/cells.h"
#include "../text/regexp.h"
#include "../text/text.h"
#include "toolchains_internal.h"

#define CLI_PATTERN_LEVELS 16u

const char *cli_toolchains_system(void)
{
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#else
    return "linux";
#endif
}

// whether a tool is for this system: it names none, or this one
static int cli_for_here(const CliJson *json, unsigned int tool)
{
    const char *const only = cli_json_string(json, cli_json_member(json, tool, "only"), NULL);
    return (only == NULL) ||
           (cellul.eq(&(CatenaFinitaCfg){.src = only, .other = cli_toolchains_system(), .cap = CLI_CELLS_POOL}));
}

unsigned int cli_toolchains_manifest(CliJson *json, unsigned int nodes_more, unsigned long long text_more)
{
    // the program's own bytes are toolchains.json's text, and the reader's file is read mapped
    cli_json_begun(json, NULL, 0u, NULL, 0ull);
    const char *const bundled_text = (const char *)g_cli_toolchains_json;
    char path[CLI_PATH_LONGEST];
    unsigned long long added_size = 0ull;
    const unsigned char *added_text = NULL;
    if (cli_home_folder(path))
    {
        CliLine line = {path + cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}),
                        CLI_PATH_LONGEST - cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), 0ull};
        cli_line_text(&line, "/user_toolchains.json");
        added_text = (cli_line_closed(&line) != 0ull) ? qry_member_file_mapped(path, &added_size) : NULL;
    }
    // the tree one cell sized to both texts' tokens and bytes and the array kept, the tokens one cell for the larger
    const unsigned int bundled_tokens = cli_json_tokens_counted(bundled_text, g_cli_toolchains_json_size) + 1u;
    const unsigned int added_tokens =
        (added_text != NULL) ? (cli_json_tokens_counted((const char *)added_text, added_size) + 1u) : 0u;
    const unsigned int tokens_most = (bundled_tokens > added_tokens) ? bundled_tokens : added_tokens;
    CfgJsonToken *const tokens =
        cli_cell_taken((unsigned long long)tokens_most * sizeof(CfgJsonToken), "the toolchains' manifest's tokens");
    const int held = (tokens != NULL) && cli_json_held(json, bundled_tokens + added_tokens + 1u + nodes_more,
                                                       g_cli_toolchains_json_size + added_size + 1ull + text_more,
                                                       "the toolchains' manifest");
    char why[CLI_PATH_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    const unsigned int bundled =
        held ? cli_json_read(json, bundled_text, g_cli_toolchains_json_size, tokens, tokens_most, &said)
             : CLI_JSON_NONE;
    const unsigned int tools = cli_json_member(json, bundled, "tools");
    const unsigned int added =
        (cli_json_is(json, tools, CFG_JSON_ARRAY) && (added_text != NULL))
            ? cli_json_read(json, (const char *)added_text, added_size, tokens, tokens_most, &said)
            : CLI_JSON_NONE;
    cli_cell_given(tokens);
    if (added_text != NULL)
    {
        qry_member_file_unmapped(added_text, added_size);
    }
    if (!cli_json_is(json, tools, CFG_JSON_ARRAY))
    {
        return CLI_JSON_NONE;
    }
    const unsigned int theirs = cli_json_member(json, added, "tools");
    for (unsigned int tool = cli_json_is(json, theirs, CFG_JSON_ARRAY) ? json->node[theirs].first : CLI_JSON_NONE;
         tool != CLI_JSON_NONE;)
    {
        const unsigned int next = json->node[tool].next;
        cli_json_append(json, tools, tool);
        tool = next;
    }
    // those for another system left out
    const unsigned int kept = cli_json_made(json, CFG_JSON_ARRAY);
    for (unsigned int tool = json->node[tools].first; tool != CLI_JSON_NONE;)
    {
        const unsigned int next = json->node[tool].next;
        if (cli_for_here(json, tool))
        {
            cli_json_append(json, kept, tool);
        }
        tool = next;
    }
    return json->full ? CLI_JSON_NONE : kept;
}

unsigned int cli_toolchains_tool(CliJson *json, unsigned int manifest, const char *id, CliLine *said)
{
    for (unsigned int tool = cli_json_is(json, manifest, CFG_JSON_ARRAY) ? json->node[manifest].first : CLI_JSON_NONE;
         tool != CLI_JSON_NONE; tool = json->node[tool].next)
    {
        const char *const named = cli_json_string(json, cli_json_member(json, tool, "id"), NULL);
        if ((named != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = named, .other = id, .cap = CLI_CELLS_POOL})))
        {
            return tool;
        }
    }
    cli_line_text(said, "orior knows no toolchain ");
    cli_line_text(said, id);
    cli_line_text(said, ": orior file toolchains lists them");
    return CLI_JSON_NONE;
}

unsigned int cli_toolchains_for_system(const CliJson *json, unsigned int map)
{
    const unsigned int here = cli_json_member(json, map, cli_toolchains_system());
    return (here != CLI_JSON_NONE) ? here : cli_json_member(json, map, "any");
}

int cli_toolchains_program_in(const char *dir, const char *const *names, unsigned int count, char *path)
{
#if defined(_WIN32)
    static const char *const s_extensions[3] = {".exe", ".cmd", ".bat"};
    const unsigned int forms = 3u;
#else
    static const char *const s_extensions[1] = {""};
    const unsigned int forms = 1u;
#endif
    for (unsigned int name = 0u; name < count; name += 1u)
    {
        for (unsigned int form = 0u; form < forms; form += 1u)
        {
            CliLine line = {path, CLI_PATH_LONGEST, 0ull};
            cli_line_text(&line, dir);
            cli_line_character(&line, '/');
            cli_line_text(&line, names[name]);
            cli_line_text(&line, s_extensions[form]);
            if ((cli_line_closed(&line) != 0ull) && query_is_file(path))
            {
                return 1;
            }
        }
    }
    return 0;
}

int cli_toolchains_tool_program_in(const CliJson *json, unsigned int tool, const char *dir, char *path)
{
    const char *names[64];
    unsigned int count = 0u;
    const unsigned int programs = cli_json_member(json, tool, "programs");
    for (unsigned int item = cli_json_is(json, programs, CFG_JSON_ARRAY) ? json->node[programs].first : CLI_JSON_NONE;
         (item != CLI_JSON_NONE) && (count < 64u); item = json->node[item].next)
    {
        const char *const name = cli_json_string(json, item, NULL);
        if (name != NULL)
        {
            names[count] = name;
            count += 1u;
        }
    }
    return cli_toolchains_program_in(dir, names, count, path);
}

// A folder as written in toolchains.json, its variables filled in: %NAME% on Windows, and ~ and $NAME elsewhere, into
// `out`, which holds CLI_PATH_LONGEST. A variable that is not set leaves the folder out: 0
static int cli_expand(const char *text, char *out)
{
    CliLine line = {out, CLI_PATH_LONGEST, 0ull};
    const char *rest = text;
    char value[CLI_PATH_LONGEST];
    if (rest[0] == '~')
    {
        if (!query_environment_read("HOME", value, sizeof(value)) &&
            !query_environment_read("USERPROFILE", value, sizeof(value)))
        {
            return 0;
        }
        cli_line_text(&line, value);
        rest += 1;
    }
    for (;;)
    {
        const char *mark = rest;
        while ((mark[0] != '\0') && (mark[0] != '%') && (mark[0] != '$'))
        {
            mark += 1;
        }
        cli_line_counted(&line, rest, (unsigned long long)(mark - rest));
        if (mark[0] == '\0')
        {
            break;
        }
        const char *const after = mark + 1;
        const char *end = after;
        if (mark[0] == '%')
        {
            while ((end[0] != '\0') && (end[0] != '%'))
            {
                end += 1;
            }
            if (end[0] == '\0')
            {
                return 0;
            }
        }
        else
        {
            while (((end[0] >= 'a') && (end[0] <= 'z')) || ((end[0] >= 'A') && (end[0] <= 'Z')) ||
                   ((end[0] >= '0') && (end[0] <= '9')) || (end[0] == '_'))
            {
                end += 1;
            }
        }
        char name[CLI_NAME_LONGEST];
        CliLine named = {name, sizeof(name), 0ull};
        cli_line_counted(&named, after, (unsigned long long)(end - after));
        if ((cli_line_closed(&named) == 0ull) || !query_environment_said(name, &line))
        {
            return 0;
        }
        rest = (mark[0] == '%') ? (end + 1) : end;
    }
    return cli_line_closed(&line) != 0ull;
}

// whether `name` matches `pattern`, whose * matches any run of letters, case aside on Windows
int cli_toolchains_matches(const char *pattern, const char *name)
{
    char left[CLI_NAME_LONGEST];
    char right[CLI_NAME_LONGEST];
    memor.cpy(&(MemoriaCfg){.dst = left,
                            .src = pattern,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = pattern, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){
        .dst = right, .src = name, .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
#if defined(_WIN32)
    for (char *at = left; at[0] != '\0'; at += 1)
    {
        at[0] = ((at[0] >= 'A') && (at[0] <= 'Z')) ? (char)(at[0] + 32) : at[0];
    }
    for (char *at = right; at[0] != '\0'; at += 1)
    {
        at[0] = ((at[0] >= 'A') && (at[0] <= 'Z')) ? (char)(at[0] + 32) : at[0];
    }
#endif
    if (cellul.chr(&(CatenaFinitaCfg){.src = left, .cap = CLI_CELLS_POOL, .byte = '*'}) == NULL)
    {
        return cellul.eq(&(CatenaFinitaCfg){.src = left, .other = right, .cap = CLI_CELLS_POOL});
    }
    unsigned long long at = 0ull;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = right, .cap = CLI_CELLS_POOL});
    const char *part = left;
    int first = 1;
    for (;;)
    {
        const char *const star = cellul.chr(&(CatenaFinitaCfg){.src = part, .cap = CLI_CELLS_POOL, .byte = '*'});
        const unsigned long long size = (star != NULL)
                                            ? (unsigned long long)(star - part)
                                            : cellul.len(&(CatenaFinitaCfg){.src = part, .cap = CLI_CELLS_POOL});
        if (first)
        {
            if (!cli_text_same(right, part, size))
            {
                return 0;
            }
            at = size;
            first = 0;
        }
        else if (star == NULL)
        {
            return (length >= (at + size)) &&
                   (cellul.eq(&(CatenaFinitaCfg){.src = right + length - size, .other = part, .cap = CLI_CELLS_POOL}));
        }
        else
        {
            char piece[CLI_NAME_LONGEST];
            CliLine line = {piece, sizeof(piece), 0ull};
            cli_line_counted(&line, part, size);
            cli_line_closed(&line);
            const char *const found = cellul.find(&(CatenaFinitaCfg){
                .src = right + at, .cap = CLI_CELLS_POOL, .other = piece, .other_cap = CLI_CELLS_POOL});
            if (found == NULL)
            {
                return 0;
            }
            at = (unsigned long long)(found - right) + size;
        }
        if (star == NULL)
        {
            return 1;
        }
        part = star + 1;
    }
}

// Orders names as their numbers read, so that Python314 comes after Python39: below 0 where `a` comes first
int cli_toolchains_natural(const char *a, const char *b)
{
    for (;;)
    {
        if ((a[0] == '\0') || (b[0] == '\0'))
        {
            return (a[0] == '\0') ? ((b[0] == '\0') ? 0 : -1) : 1;
        }
        const int a_digit = (a[0] >= '0') && (a[0] <= '9');
        const int b_digit = (b[0] >= '0') && (b[0] <= '9');
        const char *a_end = a;
        const char *b_end = b;
        while ((a_end[0] != '\0') && (((a_end[0] >= '0') && (a_end[0] <= '9')) == a_digit))
        {
            a_end += 1;
        }
        while ((b_end[0] != '\0') && (((b_end[0] >= '0') && (b_end[0] <= '9')) == b_digit))
        {
            b_end += 1;
        }
        int order = 0;
        if (a_digit && b_digit)
        {
            while ((a < a_end) && (a[0] == '0'))
            {
                a += 1;
            }
            while ((b < b_end) && (b[0] == '0'))
            {
                b += 1;
            }
            const long long a_size = a_end - a;
            const long long b_size = b_end - b;
            order = (a_size != b_size)
                        ? ((a_size < b_size) ? -1 : 1)
                        : memor.cmp(&(MemoriaCfg){.src = a, .other = b, .bytes = (unsigned long long)a_size});
        }
        else
        {
            for (const char *x = a, *y = b; (order == 0) && ((x < a_end) || (y < b_end)); x += 1, y += 1)
            {
                const char left = (x < a_end) ? (((x[0] >= 'A') && (x[0] <= 'Z')) ? (char)(x[0] + 32) : x[0]) : '\0';
                const char right = (y < b_end) ? (((y[0] >= 'A') && (y[0] <= 'Z')) ? (char)(y[0] + 32) : y[0]) : '\0';
                order = (left != right) ? ((left < right) ? -1 : 1) : 0;
            }
        }
        if (order != 0)
        {
            return (order < 0) ? -1 : 1;
        }
        a = a_end;
        b = b_end;
    }
}

// A level's folders read: those whose names match the part, the newest first
typedef struct
{
    const char *part;
    char (*names)[CLI_NAME_LONGEST];
    unsigned int count;
    unsigned int room;
} CliMatching;

static void cli_matching_each(void *context, const char *name, int is_folder)
{
    CliMatching *const matching = (CliMatching *)context;
    if (!is_folder || (matching->count >= matching->room) || !cli_toolchains_matches(matching->part, name))
    {
        return;
    }
    unsigned int place = matching->count;
    while ((place > 0u) && (cli_toolchains_natural(matching->names[place - 1u], name) < 0))
    {
        memor.cpy(&(MemoriaCfg){
            .dst = matching->names[place],
            .src = matching->names[place - 1u],
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = matching->names[place - 1u], .cap = CLI_CELLS_POOL}) + 1u});
        place -= 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = matching->names[place],
                            .src = name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
    matching->count += 1u;
}

unsigned int cli_toolchains_folders_of(const char *pattern, char (*folders)[CLI_PATH_LONGEST], unsigned int count,
                                       unsigned int room)
{
    static char s_found[2][64][CLI_PATH_LONGEST];
    static char s_names[64][CLI_NAME_LONGEST];
    static CliLock s_walking = CLI_LOCK_INIT;
    char full[CLI_PATH_LONGEST];
    if (!cli_expand(pattern, full))
    {
        return count;
    }
    cli_lock_take(&s_walking);
    unsigned int now = 0u;
    unsigned int found = 1u;
    s_found[now][0][0] = '\0';
    // each part of the path in turn, a part with a * read from each folder found so far
    const char *part = full;
    while (part[0] != '\0')
    {
        const char *end = part;
        while ((end[0] != '\0') && (end[0] != '/') && (end[0] != '\\'))
        {
            end += 1;
        }
        char piece[CLI_NAME_LONGEST];
        CliLine line = {piece, sizeof(piece), 0ull};
        cli_line_counted(&line, part, (unsigned long long)(end - part));
        cli_line_closed(&line);
        const char separator = (end[0] != '\0') ? end[0] : '/';
        unsigned int next = 0u;
        for (unsigned int at = 0u; at < found; at += 1u)
        {
            if (cellul.chr(&(CatenaFinitaCfg){.src = piece, .cap = CLI_CELLS_POOL, .byte = '*'}) == NULL)
            {
                CliLine joined = {s_found[1u - now][next], CLI_PATH_LONGEST, 0ull};
                cli_line_text(&joined, s_found[now][at]);
                cli_line_text(&joined, piece);
                cli_line_character(&joined, separator);
                next += ((cli_line_closed(&joined) != 0ull) && (next < 64u)) ? 1u : 0u;
                continue;
            }
            CliMatching matching = {piece, s_names, 0u, 64u};
            query_folder_each(s_found[now][at], cli_matching_each, &matching);
            for (unsigned int each = 0u; (each < matching.count) && (next < 64u); each += 1u)
            {
                CliLine joined = {s_found[1u - now][next], CLI_PATH_LONGEST, 0ull};
                cli_line_text(&joined, s_found[now][at]);
                cli_line_text(&joined, s_names[each]);
                cli_line_character(&joined, separator);
                next += (cli_line_closed(&joined) != 0ull) ? 1u : 0u;
            }
        }
        now = 1u - now;
        found = next;
        part = (end[0] != '\0') ? (end + 1) : end;
    }
    for (unsigned int at = 0u; (at < found) && (count < room); at += 1u)
    {
        // the separator after the last part left off
        const unsigned long long length =
            cellul.len(&(CatenaFinitaCfg){.src = s_found[now][at], .cap = CLI_CELLS_POOL});
        if ((length > 1u) && ((s_found[now][at][length - 1u] == '/') || (s_found[now][at][length - 1u] == '\\')))
        {
            s_found[now][at][length - 1u] = '\0';
        }
        if (query_is_folder(s_found[now][at]))
        {
            memor.cpy(&(MemoriaCfg){
                .dst = folders[count],
                .src = s_found[now][at],
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_found[now][at], .cap = CLI_CELLS_POOL}) + 1u});
            count += 1u;
        }
    }
    cli_lock_give(&s_walking);
    return count;
}

void cli_toolchains_key_of(const char *dir, char *key)
{
    memor.cpy(&(MemoriaCfg){
        .dst = key, .src = dir, .bytes = cellul.len(&(CatenaFinitaCfg){.src = dir, .cap = CLI_CELLS_POOL}) + 1u});
    unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL});
    while ((length != 0ull) && ((key[length - 1u] == '\\') || (key[length - 1u] == '/')))
    {
        length -= 1ull;
    }
    key[length] = '\0';
#if defined(_WIN32)
    unsigned int letters[CLI_PATH_LONGEST];
    const unsigned long long count = cli_regexp_letters(key, length, letters, CLI_PATH_LONGEST);
    CliLine line = {key, CLI_PATH_LONGEST, 0ull};
    for (unsigned long long at = 0ull; at < count; at += 1ull)
    {
        cli_utf8_put(&line, cli_regexp_lower(letters[at]));
    }
    cli_line_closed(&line);
#endif
}
