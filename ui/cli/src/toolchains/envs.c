// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// envs.c: the environments a tree holds found, each with the packages the files beside it require
#include "envs.h"
#include "../editor/files_internal.h"
#include "../text/json_tree.h"
#include "../text/text.h"

// how many folders below the top a search for environments goes, the most folders it holds to look in, the most
// environments found, and the most names one folder requires and the bytes of them
#define CLI_ENVS_DEPTH 5u
#define CLI_ENVS_PENDING 4096u
#define CLI_ENVS_MOST 1024u
#define CLI_ENVS_NAMES 1024u
#define CLI_ENVS_TEXT (1u << 20u)

// A span of a text: where it starts and its bytes
typedef struct
{
    const char *at;
    unsigned long long length;
} CliSpan;

// the span with the whitespace at either end left off
static CliSpan cli_trimmed(CliSpan span)
{
    unsigned long long start = span.length;
    unsigned long long end = 0ull;
    unsigned long long at = 0ull;
    while (at < span.length)
    {
        unsigned int letter = 0u;
        const unsigned long long step = cli_lossy_letter((const unsigned char *)span.at, span.length, at, &letter);
        if (!cli_fuzzy_white(letter))
        {
            start = (start == span.length) ? at : start;
            end = at + step;
        }
        at += step;
    }
    return (start < end) ? (CliSpan){span.at + start, end - start} : (CliSpan){span.at, 0ull};
}

// whether the span starts with `text`, and whether it is `text`
static int cli_starts(CliSpan span, const char *text)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    return (span.length >= length) && (memor.cmp(&(MemoriaCfg){.src = span.at, .other = text, .bytes = length}) == 0);
}

static int cli_is(CliSpan span, const char *text)
{
    return (span.length == cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL})) &&
           cli_starts(span, text);
}

// The names a folder requires and their texts
typedef struct
{
    unsigned int start[CLI_ENVS_NAMES];
    unsigned int length[CLI_ENVS_NAMES];
    unsigned int count;
    char text[CLI_ENVS_TEXT];
    unsigned int used;
} CliRequired;

// a name put among those required: the letters, numbers, `-`, `_` and `.` it starts with, where they are some and no
// name the same but for ASCII's case is there
static void cli_added(CliRequired *required, CliSpan name)
{
    unsigned long long length = 0ull;
    while (length < name.length)
    {
        unsigned int letter = 0u;
        const unsigned long long step = cli_lossy_letter((const unsigned char *)name.at, name.length, length, &letter);
        if (!cli_hit_wordy(letter) && (letter != '-') && (letter != '.'))
        {
            break;
        }
        length += step;
    }
    if ((length == 0ull) || (required->count >= CLI_ENVS_NAMES) || ((required->used + length) > CLI_ENVS_TEXT))
    {
        return;
    }
    for (unsigned int at = 0u; at < required->count; at += 1u)
    {
        if (required->length[at] != length)
        {
            continue;
        }
        unsigned long long same = 0ull;
        while (same < length)
        {
            const char one = required->text[required->start[at] + same];
            const char other = name.at[same];
            const char one_lower = ((one >= 'A') && (one <= 'Z')) ? (char)(one + 32) : one;
            const char other_lower = ((other >= 'A') && (other <= 'Z')) ? (char)(other + 32) : other;
            if (one_lower != other_lower)
            {
                break;
            }
            same += 1ull;
        }
        if (same == length)
        {
            return;
        }
    }
    memor.cpy(&(MemoriaCfg){.dst = required->text + required->used, .src = name.at, .bytes = length});
    required->start[required->count] = required->used;
    // a name is below CLI_ENVS_TEXT bytes, which an unsigned int holds
    required->length[required->count] = (unsigned int)length;
    required->used += (unsigned int)length;
    required->count += 1u;
}

// the file `name` in `folder` mapped, where it is UTF-8 whole: its bytes, or NULL
static const unsigned char *cli_text_mapped(const char *folder, const char *name, unsigned long long *size)
{
    char path[CLI_PATH_LONGEST];
    cli_full(folder, name, path);
    const unsigned char *const bytes = qry_member_file_mapped(path, size);
    for (unsigned long long at = 0ull; (bytes != NULL) && (at < *size);)
    {
        unsigned int letter = 0u;
        const unsigned long long step = cli_lossy_letter(bytes, *size, at, &letter);
        if ((step == 1ull) && (bytes[at] >= 0x80u))
        {
            qry_member_file_unmapped(bytes, *size);
            return NULL;
        }
        at += step;
    }
    return bytes;
}

// the next line of `size` bytes from `*at`, `*at` moved past it: 1, or 0 where the text has ended
static int cli_next_line(const unsigned char *bytes, unsigned long long size, unsigned long long *at, CliSpan *line)
{
    if (*at >= size)
    {
        return 0;
    }
    const unsigned char *const newline = memor.chr(&(MemoriaCfg){.src = bytes + *at, .val = '\n', .bytes = size - *at});
    // a newline found lies in the bytes, at or past `*at`
    const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - bytes) : size;
    *line = (CliSpan){(const char *)bytes + *at, end - *at};
    if ((newline != NULL) && (line->length > 0ull) && (line->at[line->length - 1ull] == '\r'))
    {
        line->length -= 1ull;
    }
    *at = end + 1ull;
    return 1;
}

// the names `setup.cfg` requires under `[options]`, `install_requires`, those on its line and those indented below it
static void cli_setup_read(CliRequired *required, const unsigned char *bytes, unsigned long long size)
{
    CliSpan line;
    unsigned long long at = 0ull;
    int options = 0;
    int taking = 0;
    while (cli_next_line(bytes, size, &at, &line))
    {
        const CliSpan trimmed = cli_trimmed(line);
        if (cli_starts(trimmed, "["))
        {
            options = cli_is(trimmed, "[options]");
            taking = 0;
            continue;
        }
        if (!options || cli_starts(trimmed, "#") || (trimmed.length == 0ull))
        {
            continue;
        }
        if (cli_starts(trimmed, "install_requires"))
        {
            taking = 1;
            CliSpan rest = {trimmed.at + 16, trimmed.length - 16ull};
            while ((rest.length > 0ull) && cli_fuzzy_white((unsigned char)rest.at[0]))
            {
                rest = (CliSpan){rest.at + 1, rest.length - 1ull};
            }
            if (!cli_starts(rest, "="))
            {
                continue;
            }
            unsigned long long from = 1ull;
            for (unsigned long long place = 1ull; place <= rest.length; place += 1ull)
            {
                if ((place == rest.length) || (rest.at[place] == ',') || (rest.at[place] == ';'))
                {
                    cli_added(required, (CliSpan){rest.at + from, place - from});
                    from = place + 1ull;
                }
            }
        }
        else if (taking && (line.length > 0ull) && ((line.at[0] == ' ') || (line.at[0] == '\t')))
        {
            cli_added(required, trimmed);
        }
        else
        {
            taking = 0;
        }
    }
}

// the names a Pipfile requires under `[packages]`, and those of `requirements.txt`, each a line not a comment or an
// option
static void cli_pipfile_read(CliRequired *required, const unsigned char *bytes, unsigned long long size)
{
    CliSpan line;
    unsigned long long at = 0ull;
    int packages = 0;
    while (cli_next_line(bytes, size, &at, &line))
    {
        const CliSpan trimmed = cli_trimmed(line);
        if (cli_starts(trimmed, "["))
        {
            packages = cli_is(trimmed, "[packages]");
            continue;
        }
        const char *const equals =
            packages ? memor.chr(&(MemoriaCfg){.src = trimmed.at, .val = '=', .bytes = trimmed.length}) : NULL;
        if (equals != NULL)
        {
            // the name before the equals sign, trimmed and with its quotes left off
            CliSpan name = cli_trimmed((CliSpan){trimmed.at, (unsigned long long)(equals - trimmed.at)});
            while ((name.length > 0ull) && (name.at[0] == '"'))
            {
                name = (CliSpan){name.at + 1, name.length - 1ull};
            }
            while ((name.length > 0ull) && (name.at[name.length - 1ull] == '"'))
            {
                name.length -= 1ull;
            }
            cli_added(required, name);
        }
    }
}

static void cli_requirements_read(CliRequired *required, const unsigned char *bytes, unsigned long long size)
{
    CliSpan line;
    unsigned long long at = 0ull;
    while (cli_next_line(bytes, size, &at, &line))
    {
        const CliSpan trimmed = cli_trimmed(line);
        if ((trimmed.length > 0ull) && !cli_starts(trimmed, "#") && !cli_starts(trimmed, "-"))
        {
            cli_added(required, trimmed);
        }
    }
}

// the names a folder's files require read into `required`
static void cli_required_in(const char *folder, CliRequired *required)
{
    required->count = 0u;
    required->used = 0u;
    static const char *const s_files[3] = {"setup.cfg", "Pipfile", "requirements.txt"};
    for (unsigned int file = 0u; file < 3u; file += 1u)
    {
        unsigned long long size = 0ull;
        const unsigned char *const bytes = cli_text_mapped(folder, s_files[file], &size);
        if (bytes == NULL)
        {
            continue;
        }
        if (file == 0u)
        {
            cli_setup_read(required, bytes, size);
        }
        else if (file == 1u)
        {
            cli_pipfile_read(required, bytes, size);
        }
        else
        {
            cli_requirements_read(required, bytes, size);
        }
        qry_member_file_unmapped(bytes, size);
    }
}

// the names required written as a JSON array
static void cli_required_written(const CliRequired *required, CliLine *out)
{
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < required->count; at += 1u)
    {
        cli_line_text(out, (at != 0u) ? "," : "");
        cli_json_quoted(out, required->text + required->start[at], required->length[at]);
    }
    cli_line_character(out, ']');
}

void cli_envs_requires_in(const char *folder, CliLine *out)
{
    static CliRequired s_required;
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    cli_required_in(folder, &s_required);
    cli_required_written(&s_required, out);
    cli_lock_give(&s_lock);
}

// An environment found: its kind, its place in the tree, and where its requirements are read
typedef struct
{
    const char *kind;
    char place[CLI_PATH_LONGEST];
    char requires_from[CLI_PATH_LONGEST];
} CliFoundEnv;

// The search: the folders to look in with their depths, the folder's folders as they are read, and what it found
typedef struct
{
    char pending[CLI_ENVS_PENDING][CLI_PATH_LONGEST];
    unsigned int depth[CLI_ENVS_PENDING];
    unsigned int pending_count;
    const char *folder;
    unsigned int folder_depth;
    CliFoundEnv found[CLI_ENVS_MOST];
    unsigned int order[CLI_ENVS_MOST];
    unsigned int count;
    CliRequired required;
} CliEnvSearch;

static CliEnvSearch s_search;

static void cli_below_each(void *context, const char *name, int is_folder)
{
    (void)context;
    static const char *const s_not[6] = {".git", "node_modules", "target", "build", "__pycache__", ".tox"};
    for (unsigned int at = 0u; is_folder && (at < 6u); at += 1u)
    {
        is_folder = !cellul.eq(&(CatenaFinitaCfg){.src = name, .other = s_not[at], .cap = CLI_CELLS_POOL});
    }
    if (is_folder && (s_search.pending_count < CLI_ENVS_PENDING))
    {
        cli_full(s_search.folder, name, s_search.pending[s_search.pending_count]);
        s_search.depth[s_search.pending_count] = s_search.folder_depth + 1u;
        s_search.pending_count += 1u;
    }
}

// an environment put among those found
static void cli_env_put(const char *kind, const char *place, const char *requires_from)
{
    if (s_search.count < CLI_ENVS_MOST)
    {
        CliFoundEnv *const one = &s_search.found[s_search.count];
        one->kind = kind;
        memor.cpy(&(MemoriaCfg){.dst = one->place,
                                .src = place,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = place, .cap = CLI_CELLS_POOL}) + 1u});
        memor.cpy(
            &(MemoriaCfg){.dst = one->requires_from,
                          .src = requires_from,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = requires_from, .cap = CLI_CELLS_POOL}) + 1u});
        s_search.count += 1u;
    }
}

// how deep an environment's place is, for its order: its slashes, and one more where it is not the top
static unsigned int cli_env_depth(const CliFoundEnv *one)
{
    unsigned int depth = (one->place[0] != '\0') ? 1u : 0u;
    for (const char *at = one->place; at[0] != '\0'; at += 1)
    {
        depth += (at[0] == '/') ? 1u : 0u;
    }
    return depth;
}

// whether one environment comes before another: by depth, then place, then kind
static int cli_env_before(const CliFoundEnv *one, const CliFoundEnv *other)
{
    const unsigned int one_depth = cli_env_depth(one);
    const unsigned int other_depth = cli_env_depth(other);
    if (one_depth != other_depth)
    {
        return one_depth < other_depth;
    }
    const int order = cli_text_order(one->place, other->place);
    return (order < 0) || ((order == 0) && (cli_text_order(one->kind, other->kind) < 0));
}

// whether the file `name` is in `folder`
static int cli_holds(const char *folder, const char *name)
{
    char path[CLI_PATH_LONGEST];
    cli_full(folder, name, path);
    return query_is_file(path);
}

void cli_envs_found(const char *root, CliLine *out)
{
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    s_search.count = 0u;
    memor.cpy(&(MemoriaCfg){.dst = s_search.pending[0],
                            .src = root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    s_search.depth[0] = 0u;
    s_search.pending_count = 1u;
    while (s_search.pending_count > 0u)
    {
        s_search.pending_count -= 1u;
        char folder[CLI_PATH_LONGEST];
        memor.cpy(
            &(MemoriaCfg){.dst = folder, .bytes = sizeof(folder), .src = s_search.pending[s_search.pending_count]});
        const unsigned int depth = s_search.depth[s_search.pending_count];
        char place[CLI_PATH_LONGEST];
        cli_relative(root, folder, place);
        for (char *at = place; at[0] != '\0'; at += 1)
        {
            at[0] = (at[0] == '\\') ? '/' : at[0];
        }
        if (cli_holds(folder, "pyvenv.cfg"))
        {
            char parent[CLI_PATH_LONGEST];
            cli_env_put("venv", place, cli_path_parent(folder, parent) ? parent : "");
            continue;
        }
        if (cli_holds(folder, "Pipfile"))
        {
            cli_env_put("pipenv", place, folder);
        }
        if (cli_holds(folder, "shell.nix") || cli_holds(folder, "flake.nix") || cli_holds(folder, "default.nix"))
        {
            cli_env_put("nix", place, folder);
        }
        if (depth >= CLI_ENVS_DEPTH)
        {
            continue;
        }
        s_search.folder = folder;
        s_search.folder_depth = depth;
        query_folder_each(folder, cli_below_each, NULL);
    }
    for (unsigned int at = 0u; at < s_search.count; at += 1u)
    {
        unsigned int place = at;
        while ((place > 0u) && cli_env_before(&s_search.found[at], &s_search.found[s_search.order[place - 1u]]))
        {
            s_search.order[place] = s_search.order[place - 1u];
            place -= 1u;
        }
        s_search.order[place] = at;
    }
    cli_line_character(out, '[');
    for (unsigned int at = 0u; at < s_search.count; at += 1u)
    {
        const CliFoundEnv *const one = &s_search.found[s_search.order[at]];
        cli_line_text(out, (at != 0u) ? ",{\"kind\":" : "{\"kind\":");
        cli_json_quoted(out, one->kind, cellul.len(&(CatenaFinitaCfg){.src = one->kind, .cap = CLI_CELLS_POOL}));
        cli_line_text(out, ",\"place\":");
        cli_json_quoted(out, one->place, cellul.len(&(CatenaFinitaCfg){.src = one->place, .cap = CLI_CELLS_POOL}));
        cli_line_text(out, ",\"requires\":");
        s_search.required.count = 0u;
        if (one->requires_from[0] != '\0')
        {
            cli_required_in(one->requires_from, &s_search.required);
        }
        cli_required_written(&s_search.required, out);
        cli_line_character(out, '}');
    }
    cli_line_character(out, ']');
    cli_lock_give(&s_lock);
}
