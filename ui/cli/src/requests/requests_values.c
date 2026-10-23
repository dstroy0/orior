// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_values.c: the environment files of a file of requests, each the nearest beside it or in a folder above it
// within the tree; the values of an environment they give, the private file's over the shared file's and `$shared`
// under the environment's own; whether an environment checks the peer's certificate; and the reader's private file
// made
#include "../text/json_tree.h"
#include "requests_internal.h"

#include "../text/text.h"

// the bytes a values' pool is begun with before it doubles, and a value written as JSON is begun in before it grows
#define CLI_REQUESTS_POOL_FIRST (1u << 10u)

CliCellsTally g_cli_requests_cells = CLI_CELLS_TALLY("cli_requests");

// the environment file read, a tree in a cell sized to it, and the names of the environments, under the lock and
// given back before it is
static CliJson s_json;
static CliRequestsValues s_names;
static CliLock s_lock = CLI_LOCK_INIT;

// the environment files, the shared one first
static const char *const s_files[2] = {CLI_REQUESTS_PUBLIC, CLI_REQUESTS_PRIVATE};

// 1 where `path` is `base` or stands under it
static int values_under(const char *path, const char *base)
{
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL});
    return cli_text_same(path, base, length) &&
           ((path[length] == '\0') || (path[length] == '/') || (path[length] == '\\'));
}

int cli_requests_nearest(const char *root, const char *path, const char *name, char *found)
{
    char file[CLI_PATH_LONGEST];
    char why[CLI_SAID_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    char folder[CLI_PATH_LONGEST];
    if (!cli_inside(root, path, file, &said) || !cli_path_parent(file, folder))
    {
        return 0;
    }
    while (values_under(folder, root))
    {
        CliLine line = {found, CLI_PATH_LONGEST, 0ull};
        cli_line_text(&line, folder);
        cli_line_character(&line, '/');
        cli_line_text(&line, name);
        if ((cli_line_closed(&line) != 0ull) && query_is_file(found))
        {
            return 1;
        }
        char above[CLI_PATH_LONGEST];
        if (!cli_path_parent(folder, above))
        {
            return 0;
        }
        memor.cpy(&(MemoriaCfg){.dst = folder, .src = above, .bytes = sizeof(folder)});
    }
    return 0;
}

// the environment file at `file` read into the tree, the lock held: its object, or CLI_JSON_NONE with why said
static unsigned int values_read(const char *file, CliLine *said)
{
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_held(file, &size);
    if ((bytes == NULL) || !cli_utf8_whole((const char *)bytes, size))
    {
        if (bytes != NULL)
        {
            cli_file_let_go(bytes, size);
        }
        cli_line_text(said, file);
        cli_line_text(said, ": it could not be read");
        return CLI_JSON_NONE;
    }
    char why[CLI_SAID_LONGEST];
    CliLine read_said = {why, sizeof(why), 0ull};
    cli_json_given(&s_json);
    const unsigned int object = cli_json_taken(&s_json, (const char *)bytes, size, 0u, 0ull, &read_said);
    cli_file_let_go(bytes, size);
    if (object == CLI_JSON_NONE)
    {
        cli_line_text(said, file);
        cli_line_text(said, ": ");
        cli_line_counted(said, why, read_said.at);
        return CLI_JSON_NONE;
    }
    if (!cli_json_is(&s_json, object, CFG_JSON_OBJECT))
    {
        cli_line_text(said, file);
        cli_line_text(said, " holds no object of environments");
        return CLI_JSON_NONE;
    }
    return object;
}

int cli_requests_value_put(CliRequestsValues *values, const char *name, unsigned long long name_size, const char *value,
                           unsigned long long value_size)
{
    unsigned int at = 0u;
    while ((at < values->count) &&
           ((values->name[at].size != name_size) ||
            (memor.cmp(&(MemoriaCfg){
                 .src = values->text + values->name[at].at, .other = name, .bytes = (size_t)name_size}) != 0)))
    {
        at += 1u;
    }
    const unsigned long long needed = value_size + 1u + ((at == values->count) ? (name_size + 1u) : 0u);
    if ((values->used + needed > CLI_REQUESTS_TEXT) || (at == CLI_REQUESTS_VALUES))
    {
        return 0;
    }
    if (values->used + needed > values->room)
    {
        // the pool's room doubled until the value fits, CLI_REQUESTS_TEXT at the most
        unsigned long long room = (values->room == 0ull) ? CLI_REQUESTS_POOL_FIRST : values->room;
        while (room < values->used + needed)
        {
            room *= 2ull;
        }
        room = (room < CLI_REQUESTS_TEXT) ? room : CLI_REQUESTS_TEXT;
        char *const pool = cli_cell_taken_by(&g_cli_requests_cells, room, "the values' texts");
        if (pool == NULL)
        {
            return 0;
        }
        memor.cpy(&(MemoriaCfg){.dst = pool, .src = values->text, .bytes = (size_t)values->used});
        cli_cell_given_by(&g_cli_requests_cells, values->text);
        values->text = pool;
        values->room = room;
    }
    if (at == values->count)
    {
        values->name[at].at = values->used;
        values->name[at].size = name_size;
        memor.cpy(&(MemoriaCfg){.dst = values->text + values->used, .src = name, .bytes = (size_t)name_size});
        values->text[values->used + name_size] = '\0';
        values->used += name_size + 1u;
        values->count += 1u;
    }
    values->value[at].at = values->used;
    values->value[at].size = value_size;
    memor.cpy(&(MemoriaCfg){.dst = values->text + values->used, .src = value, .bytes = (size_t)value_size});
    values->text[values->used + value_size] = '\0';
    values->used += value_size + 1u;
    return 1;
}

void cli_requests_values_given(CliRequestsValues *values)
{
    cli_cell_given_by(&g_cli_requests_cells, values->text);
    values->text = NULL;
    values->room = 0ull;
    values->used = 0ull;
    values->count = 0u;
}

const CliRequestsText *cli_requests_value(const CliRequestsValues *values, const char *name, unsigned long long size)
{
    for (unsigned int at = 0u; at < values->count; at += 1u)
    {
        if ((values->name[at].size == size) &&
            (memor.cmp(
                 &(MemoriaCfg){.src = values->text + values->name[at].at, .other = name, .bytes = (size_t)size}) == 0))
        {
            return &values->value[at];
        }
    }
    return NULL;
}

// the reason the values of an environment did not fit
static void values_full_said(CliLine *said)
{
    cli_line_text(said, "the values of the environment are larger than orior_cli holds");
}

// the names of the environments of the file at `path` put in s_names, the lock held, and the tree's paths of the
// shared file and the private one into `relative` where `has` says it is there: 1, or 0 with why said
static int values_listed(const char *root, const char *path, char (*relative)[CLI_PATH_LONGEST], int *has,
                         CliLine *said)
{
    int read = 1;
    int fits = 1;
    cli_requests_values_given(&s_names);
    for (unsigned int slot = 0u; read && fits && (slot < 2u); slot += 1u)
    {
        char file[CLI_PATH_LONGEST];
        has[slot] = cli_requests_nearest(root, path, s_files[slot], file);
        if (!has[slot])
        {
            continue;
        }
        const unsigned int object = values_read(file, said);
        read = (object != CLI_JSON_NONE);
        for (unsigned int member = read ? s_json.node[object].first : CLI_JSON_NONE; fits && (member != CLI_JSON_NONE);
             member = s_json.node[member].next)
        {
            const char *const key = s_json.text + s_json.node[member].key;
            fits = (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "$shared", .cap = CLI_CELLS_POOL})) ||
                   cli_requests_value_put(&s_names, key,
                                          cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}), "", 0ull);
        }
        cli_relative(root, file, relative[slot]);
    }
    if (!fits)
    {
        values_full_said(said);
    }
    return read && fits;
}

int cli_requests_environments(const char *root, const char *path, CliLine *out, CliLine *said)
{
    char relative[2][CLI_PATH_LONGEST];
    int has[2] = {0, 0};
    cli_lock_take(&s_lock);
    const int listed = values_listed(root, path, relative, has, said);
    if (listed)
    {
        cli_line_text(out, "{\"names\":[");
        for (unsigned int at = 0u; at < s_names.count; at += 1u)
        {
            cli_line_text(out, (at > 0u) ? "," : "");
            cli_json_quoted(out, s_names.text + s_names.name[at].at, s_names.name[at].size);
        }
        static const char *const keys[2] = {"],\"shared\":", ",\"private\":"};
        for (unsigned int slot = 0u; slot < 2u; slot += 1u)
        {
            cli_line_text(out, keys[slot]);
            if (has[slot])
            {
                cli_json_quoted(out, relative[slot],
                                cellul.len(&(CatenaFinitaCfg){.src = relative[slot], .cap = CLI_CELLS_POOL}));
            }
            else
            {
                cli_line_text(out, "null");
            }
        }
        cli_line_character(out, '}');
    }
    cli_requests_values_given(&s_names);
    cli_json_given(&s_json);
    cli_lock_give(&s_lock);
    return listed;
}

// a value of the tree, a string as its letters and any other as JSON on one line, put as `name` in `values`: 1, or 0
// where it does not fit
static int values_member_put(CliRequestsValues *values, unsigned int member)
{
    const char *const key = s_json.text + s_json.node[member].key;
    unsigned long long length = 0ull;
    const char *const text = cli_json_string(&s_json, member, &length);
    if (text != NULL)
    {
        return cli_requests_value_put(values, key, cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}),
                                      text, length);
    }
    CliLine written;
    if (!cli_line_growing(&written, CLI_REQUESTS_POOL_FIRST, "a value written as JSON"))
    {
        return 0;
    }
    cli_json_write(&s_json, member, &written, 0u, 0);
    const int put =
        cli_line_fits(&written) &&
        cli_requests_value_put(values, key, cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}),
                               written.out, written.at);
    cli_cell_given(written.out);
    return put;
}

int cli_requests_values(const char *root, const char *path, const char *env, CliRequestsValues *values, CliLine *said)
{
    cli_requests_values_given(values);
    int read = 1;
    int fits = 1;
    cli_lock_take(&s_lock);
    for (unsigned int slot = 0u; read && fits && (slot < 2u); slot += 1u)
    {
        char file[CLI_PATH_LONGEST];
        if (!cli_requests_nearest(root, path, s_files[slot], file))
        {
            continue;
        }
        const unsigned int object = values_read(file, said);
        read = (object != CLI_JSON_NONE);
        for (unsigned int group = 0u; read && fits && (group < ((env != NULL) ? 2u : 1u)); group += 1u)
        {
            const unsigned int set = cli_json_member(&s_json, object, (group == 0u) ? "$shared" : env);
            for (unsigned int member = cli_json_is(&s_json, set, CFG_JSON_OBJECT) ? s_json.node[set].first
                                                                                  : CLI_JSON_NONE;
                 fits && (member != CLI_JSON_NONE); member = s_json.node[member].next)
            {
                fits = values_member_put(values, member);
            }
        }
    }
    cli_json_given(&s_json);
    cli_lock_give(&s_lock);
    if (!fits)
    {
        values_full_said(said);
    }
    return read && fits;
}

int cli_requests_verifies(const char *root, const char *path, const char *env)
{
    if (env == NULL)
    {
        return 1;
    }
    int verifies = 1;
    cli_lock_take(&s_lock);
    for (unsigned int slot = 0u; verifies && (slot < 2u); slot += 1u)
    {
        char file[CLI_PATH_LONGEST];
        char why[CLI_SAID_LONGEST];
        CliLine said = {why, sizeof(why), 0ull};
        const unsigned int object =
            cli_requests_nearest(root, path, s_files[slot], file) ? values_read(file, &said) : CLI_JSON_NONE;
        const unsigned int set = cli_json_member(&s_json, object, env);
        const unsigned int ssl = cli_json_member(&s_json, set, "SSLConfiguration");
        verifies = !cli_json_is(&s_json, cli_json_member(&s_json, ssl, "verifyHostCertificate"), CFG_JSON_FALSE);
    }
    cli_json_given(&s_json);
    cli_lock_give(&s_lock);
    return verifies;
}

// whether the .gitignore at `ignore` has a line that, trimmed, names the reader's private file; and whether it ends
// in a newline or is empty, into `joined`
static int values_ignored(const char *ignore, int *joined)
{
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_held(ignore, &size);
    const char *const listed = (const char *)bytes;
    const unsigned long long length = (bytes != NULL) ? size : 0ull;
    *joined = (length == 0ull) || (listed[length - 1u] == '\n');
    int named = 0;
    for (unsigned long long at = 0ull; !named && (at < length);)
    {
        const char *const newline =
            memor.chr(&(MemoriaCfg){.src = listed + at, .val = '\n', .bytes = (size_t)(length - at)});
        unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - listed) : length;
        unsigned long long start = at;
        at = (newline != NULL) ? end + 1u : length;
        cli_requests_trimmed(listed, &start, &end);
        static const char *const names[3] = {CLI_REQUESTS_PRIVATE, "/" CLI_REQUESTS_PRIVATE, "*.private.env.json"};
        for (unsigned int name = 0u; name < 3u; name += 1u)
        {
            named =
                named || ((cellul.len(&(CatenaFinitaCfg){.src = names[name], .cap = CLI_CELLS_POOL}) == end - start) &&
                          (memor.cmp(&(MemoriaCfg){
                               .src = listed + start, .other = names[name], .bytes = (size_t)(end - start)}) == 0));
        }
    }
    if (bytes != NULL)
    {
        cli_file_let_go(bytes, size);
    }
    return named;
}

// the reader's private file written at `file`, an empty set for each environment named in s_names, or for `dev`
// where none is, as serde_json's pretty writer lays the object out: 1, or 0 where it could not be written
static int values_private_written(const char *file)
{
    CliLine object;
    if (!cli_line_growing(&object, CLI_REQUESTS_POOL_FIRST, "the reader's private file written"))
    {
        return 0;
    }
    cli_line_character(&object, '{');
    for (unsigned int at = 0u; at < s_names.count; at += 1u)
    {
        cli_line_text(&object, (at > 0u) ? ",\n  " : "\n  ");
        cli_json_quoted(&object, s_names.text + s_names.name[at].at, s_names.name[at].size);
        cli_line_text(&object, ": {}");
    }
    if (s_names.count == 0u)
    {
        cli_line_text(&object, "\n  \"dev\": {}");
    }
    cli_line_text(&object, "\n}\n");
    const int written = cli_line_fits(&object) && query_file_written(file, object.out, object.at);
    cli_cell_given(object.out);
    return written;
}

int cli_requests_private_made(const char *root, const char *path, CliLine *out, CliLine *said)
{
    char file[CLI_PATH_LONGEST];
    char relative[2][CLI_PATH_LONGEST];
    if (cli_requests_nearest(root, path, CLI_REQUESTS_PRIVATE, file))
    {
        cli_relative(root, file, relative[0]);
        cli_json_quoted(out, relative[0], cellul.len(&(CatenaFinitaCfg){.src = relative[0], .cap = CLI_CELLS_POOL}));
        return 1;
    }
    char inside[CLI_PATH_LONGEST];
    char folder[CLI_PATH_LONGEST];
    if (!cli_inside(root, path, inside, said))
    {
        return 0;
    }
    if (!cli_path_parent(inside, folder))
    {
        cli_line_text(said, "the file has no folder");
        return 0;
    }
    CliLine made = {file, sizeof(file), 0ull};
    cli_line_text(&made, folder);
    cli_line_text(&made, "/" CLI_REQUESTS_PRIVATE);
    char ignore[CLI_PATH_LONGEST];
    CliLine ignore_line = {ignore, sizeof(ignore), 0ull};
    cli_line_text(&ignore_line, root);
    cli_line_text(&ignore_line, "/.gitignore");
    if ((cli_line_closed(&made) == 0ull) || (cli_line_closed(&ignore_line) == 0ull))
    {
        cli_line_text(said, path);
        cli_line_text(said, ": its path is too long");
        return 0;
    }
    int has[2] = {0, 0};
    cli_lock_take(&s_lock);
    int done = values_listed(root, path, relative, has, said);
    int joined = 1;
    const char *failed = NULL;
    if (done && !values_ignored(ignore, &joined))
    {
        // a tree with no .gitignore is given one, which an empty file's `joined` leaves to the second write to make
        const int fresh = !query_is_file(ignore);
        done = (joined || cli_file_appended(ignore, "\n", 1ull, 0)) &&
               cli_file_appended(ignore, CLI_REQUESTS_PRIVATE "\n", sizeof(CLI_REQUESTS_PRIVATE), fresh);
        failed = done ? NULL : ignore;
    }
    if (done)
    {
        done = values_private_written(file);
        failed = done ? NULL : file;
    }
    cli_requests_values_given(&s_names);
    cli_json_given(&s_json);
    cli_lock_give(&s_lock);
    if (failed != NULL)
    {
        cli_line_text(said, failed);
        cli_line_text(said, ": it could not be written");
    }
    if (done)
    {
        cli_relative(root, file, relative[0]);
        cli_json_quoted(out, relative[0], cellul.len(&(CatenaFinitaCfg){.src = relative[0], .cap = CLI_CELLS_POOL}));
    }
    return done;
}
