// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// servers_emit.c: each file's diagnostics as its server last gave them, the inspections' findings and the checkers',
// what a build tool gives a file and what each Python class's family sets; and a file's diagnostics passed on whole:
// its server's, less its reports of an attribute a class's family sets and of a name a build tool gives it, then the
// inspections', then each checker's, the checkers by their ids in order
#include "../builds/builds.h"
#include "../command_line/cells.h"
#include "servers_internal.h"

#include "../text/text.h"

// the most files whose diagnostics are kept
#define CLI_SERVERS_KEPT (1u << 16u)

CliKeyed g_cli_servers_published = {"published diagnostics", {NULL}, 0u, CLI_SERVERS_KEPT};
CliLock g_cli_servers_published_lock = CLI_LOCK_INIT;
CliKeyed g_cli_servers_inspected = {"inspections' findings", {NULL}, 0u, CLI_SERVERS_KEPT};
CliKeyed g_cli_servers_checked = {"checkers' findings", {NULL}, 0u, CLI_SERVERS_KEPT};
CliKeyed g_cli_servers_given = {"names build tools give", {NULL}, 0u, 4096u};
CliKeyed g_cli_servers_supplied = {"names supplied", {NULL}, 0u, CLI_SERVERS_KEPT};
CliLock g_cli_servers_checks_lock = CLI_LOCK_INIT;

// what is read to pass a file's diagnostics on
static CliServersReading s_reading = {CLI_LOCK_INIT};

void cli_servers_uri_of(const char *path, char *uri)
{
    cli_lsp_uri_of(path, uri, CLI_PATH_LONGEST);
}

void cli_servers_key_of(const char *path, char *key)
{
    char uri[CLI_PATH_LONGEST];
    cli_servers_uri_of(path, uri);
    cli_lsp_key_of(uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), key);
}

void cli_servers_tree_path(const char *tree, const char *path, CliLine *out)
{
    char full_path[CLI_PATH_LONGEST];
    char full_tree[CLI_PATH_LONGEST];
    if ((tree == NULL) || (tree[0] == '\0'))
    {
        cli_line_text(out, path);
        return;
    }
    const char *const whole = query_path_full(path, full_path, sizeof(full_path)) ? full_path : path;
    const char *const top = query_path_full(tree, full_tree, sizeof(full_tree)) ? full_tree : tree;
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = top, .cap = CLI_CELLS_POOL});
    const int under = cli_text_same(whole, top, length) &&
                      ((whole[length] == '\0') || (whole[length] == '/') || (whole[length] == '\\'));
    // a file of a folder mounted beside the tree goes by its mount's name
    const char *names[CLI_MOUNTS_MOST];
    const char *paths[CLI_MOUNTS_MOST];
    const unsigned int mounts = cli_mounts(names, paths, CLI_MOUNTS_MOST);
    int mounted = 0;
    for (unsigned int at = 0u; !under && (at < mounts); at += 1u)
    {
        const size_t base = cellul.len(&(CatenaFinitaCfg){.src = paths[at], .cap = CLI_CELLS_POOL});
        mounted |= cli_text_same(whole, paths[at], base) &&
                   ((whole[base] == '\0') || (whole[base] == '/') || (whole[base] == '\\'));
    }
    if (!under && !mounted)
    {
        cli_line_text(out, whole);
        return;
    }
    char relative[CLI_PATH_LONGEST];
    cli_relative(top, whole, relative);
    cli_line_text(out, relative);
}

// the message of a server's report, NULL where it has none
static const char *emit_message_of(const CliJson *json, unsigned int item, unsigned long long *size)
{
    const unsigned int message = cli_json_member(json, item, "message");
    const char *const text = cli_json_string(json, message, size);
    return (text != NULL) ? text : cli_json_string(json, cli_json_member(json, message, "value"), size);
}

// whether `size` bytes of `names`, one to a line, hold the line `size` bytes of `wanted` and then `also`, where it is
// given
static int emit_has_line(const char *names, unsigned long long size, const char *wanted, unsigned long long length,
                         const char *also, unsigned long long also_length)
{
    unsigned long long at = 0ull;
    while (at < size)
    {
        unsigned long long end = at;
        while ((end < size) && (names[end] != '\n'))
        {
            end += 1ull;
        }
        if ((end - at == length + also_length) &&
            (memor.cmp(&(MemoriaCfg){.src = names + at, .other = wanted, .bytes = (size_t)length}) == 0) &&
            ((also_length == 0ull) ||
             (memor.cmp(&(MemoriaCfg){.src = names + at + length, .other = also, .bytes = (size_t)also_length}) == 0)))
        {
            return 1;
        }
        at = end + 1ull;
    }
    return 0;
}

int cli_servers_attribute_of(const char *message, unsigned long long length, const char **name,
                             unsigned long long *name_size, const char **class_name, unsigned long long *class_size)
{
    static const char s_head[] = "Cannot access attribute \"";
    static const char s_middle[] = "\" for class \"";
    if ((message == NULL) || !cli_text_starts(message, length, s_head))
    {
        return 0;
    }
    *name = message + sizeof(s_head) - 1u;
    const unsigned long long rest = length - (sizeof(s_head) - 1u);
    *name_size = 0ull;
    while ((*name_size < rest) && ((*name)[*name_size] != '"'))
    {
        *name_size += 1ull;
    }
    if ((*name_size == rest) || !cli_text_starts(*name + *name_size, rest - *name_size, s_middle))
    {
        return 0;
    }
    *class_name = *name + *name_size + sizeof(s_middle) - 1u;
    const unsigned long long class_rest = rest - *name_size - (sizeof(s_middle) - 1u);
    *class_size = 0ull;
    while ((*class_size < class_rest) && ((*class_name)[*class_size] != '"'))
    {
        *class_size += 1ull;
    }
    if (*class_size == class_rest)
    {
        return 0;
    }
    while ((*class_size > 0ull) && ((*class_name)[0] == '*'))
    {
        *class_name += 1;
        *class_size -= 1ull;
    }
    while ((*class_size > 0ull) && ((*class_name)[*class_size - 1ull] == '*'))
    {
        *class_size -= 1ull;
    }
    return 1;
}

// whether a server's report that a class has no such attribute names an attribute the class's family sets; what the
// file's family sets kept one to a line as `class attribute`
static int emit_supplied(const CliJson *json, unsigned int item, const char *supplied, unsigned long long size)
{
    unsigned long long length = 0ull;
    const char *const message = emit_message_of(json, item, &length);
    const char *name = NULL;
    unsigned long long name_size = 0ull;
    const char *class_name = NULL;
    unsigned long long class_size = 0ull;
    if (!cli_servers_attribute_of(message, length, &name, &name_size, &class_name, &class_size))
    {
        return 0;
    }
    static char s_attribute[1024];
    if (name_size + 1ull >= sizeof(s_attribute))
    {
        return 0;
    }
    s_attribute[0] = ' ';
    memor.cpy(&(MemoriaCfg){.dst = s_attribute + 1u, .src = name, .bytes = (size_t)name_size});
    return emit_has_line(supplied, size, class_name, class_size, s_attribute, name_size + 1ull);
}

// whether a server's report that a name is undefined names one of `names`, as pyright writes it: `"name" is not
// defined`
static int emit_given(const CliJson *json, unsigned int item, const char *names, unsigned long long size)
{
    unsigned long long length = 0ull;
    const char *const message = emit_message_of(json, item, &length);
    static const char s_tail[] = "\" is not defined";
    if ((message == NULL) || (length < sizeof(s_tail)) || (message[0] != '"') ||
        !cli_text_ends(message, length, s_tail))
    {
        return 0;
    }
    return emit_has_line(names, size, message + 1u, length - sizeof(s_tail), "", 0ull);
}

// a name a build tool gives a file put on its own line
static void emit_name_put(void *context, const char *name, unsigned long long size)
{
    CliLine *const names = context;
    cli_line_counted(names, name, size);
    cli_line_character(names, '\n');
}

// the names a build tool gives the file of `key` and `uri`, kept as its text was checked or read from the disk for a
// file the editor does not hold, one to a line: 1 with them into `names`, or 0 for a file of no build tool that gives
// names; under the checks' lock
static int emit_given_names(const char *key, const char *uri, const char **names, unsigned long long *size)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL});
    static const char s_construct[] = "/sconstruct";
    static const char s_script[] = "/sconscript";
    int construct = (length >= sizeof(s_construct) - 1u);
    int script = (length >= sizeof(s_script) - 1u);
    for (unsigned int at = 0u; construct && (at < sizeof(s_construct) - 1u); at += 1u)
    {
        const char one = uri[length - (sizeof(s_construct) - 1u) + at];
        const char lower = ((one >= 'A') && (one <= 'Z')) ? (char)(one - 'A' + 'a') : one;
        construct = (lower == s_construct[at]);
    }
    for (unsigned int at = 0u; script && (at < sizeof(s_script) - 1u); at += 1u)
    {
        const char one = uri[length - (sizeof(s_script) - 1u) + at];
        const char lower = ((one >= 'A') && (one <= 'Z')) ? (char)(one - 'A' + 'a') : one;
        script = (lower == s_script[at]);
    }
    if (!construct && !script)
    {
        return 0;
    }
    const char *beside = NULL;
    *names = cli_keyed_get(&g_cli_servers_given, key, &beside, size);
    if (*names != NULL)
    {
        return 1;
    }
    char path[CLI_PATH_LONGEST];
    if (!cli_lsp_path_of(uri, length, path))
    {
        return 0;
    }
    unsigned long long build_size = 0ull;
    const unsigned char *const build = cli_file_held(path, &build_size);
    if (build == NULL)
    {
        return 0;
    }
    char piece[1024];
    CliPieces pieces;
    CliLine given;
    cli_pieces_begun(&pieces, &given, piece, sizeof(piece));
    cli_build_given_names(path, (const char *)build, build_size, emit_name_put, &given);
    cli_file_let_go(build, build_size);
    unsigned long long given_size = 0ull;
    char *const joined = cli_pieces_joined(&pieces, &given, "the names a build tool gives", &given_size);
    (void)cli_keyed_put(&g_cli_servers_given, key, "", (joined != NULL) ? joined : "",
                        (joined != NULL) ? given_size : 0ull);
    cli_cell_given(joined);
    *names = cli_keyed_get(&g_cli_servers_given, key, &beside, size);
    return *names != NULL;
}

// the kept text of `key` in `keyed` read into the reading's tree under its lock: its root, CLI_JSON_NONE where none is
static unsigned int emit_kept_read(CliKeyed *keyed, CliLock *lock, const char *key)
{
    cli_lock_take(lock);
    const char *beside = NULL;
    unsigned long long size = 0ull;
    const char *const text = cli_keyed_get(keyed, key, &beside, &size);
    const unsigned int root =
        ((text != NULL) && (size > 0ull)) ? cli_servers_read(&s_reading, text, size) : CLI_JSON_NONE;
    cli_lock_give(lock);
    return root;
}

// each report of an array written as orior's diagnostic, after those written, those `kept` refuses left out
static void emit_items_written(unsigned int array, int filtered, const char *key, const char *uri, int *written,
                               CliLine *out)
{
    const CliJson *const json = &s_reading.json;
    for (unsigned int item = cli_json_is(json, array, CFG_JSON_ARRAY) ? json->node[array].first : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json->node[item].next)
    {
        if (filtered)
        {
            cli_lock_take(&g_cli_servers_checks_lock);
            const char *beside = NULL;
            unsigned long long size = 0ull;
            const char *const supplied = cli_keyed_get(&g_cli_servers_supplied, key, &beside, &size);
            int dropped = (supplied != NULL) && emit_supplied(json, item, supplied, size);
            const char *names = NULL;
            dropped = dropped || (emit_given_names(key, uri, &names, &size) && emit_given(json, item, names, size));
            cli_lock_give(&g_cli_servers_checks_lock);
            if (dropped)
            {
                continue;
            }
        }
        cli_line_text(out, *written ? "," : "");
        cli_servers_diagnostic_of(json, item, out);
        *written = 1;
    }
}

void cli_servers_emit_whole(const char *key, const char *uri, const CliServersEmit *emit)
{
    char path[CLI_PATH_LONGEST];
    if ((emit->told == NULL) ||
        !cli_lsp_path_of(uri, cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}), path))
    {
        return;
    }
    cli_servers_reading_held(&s_reading);
    char piece[CLI_SERVERS_PIECE];
    CliPieces pieces;
    CliLine out;
    cli_pieces_begun(&pieces, &out, piece, sizeof(piece));
    cli_line_text(&out, "{\"path\":");
    char shown[CLI_PATH_LONGEST];
    CliLine where = {shown, sizeof(shown), 0ull};
    cli_servers_tree_path(emit->tree, path, &where);
    cli_json_quoted_lossy(&out, (const unsigned char *)shown, cli_line_fits(&where) ? where.at : 0ull);
    cli_line_text(&out, ",\"items\":[");
    int written = 0;
    emit_items_written(emit_kept_read(&g_cli_servers_published, &g_cli_servers_published_lock, key), 1, key, uri,
                       &written, &out);
    emit_items_written(emit_kept_read(&g_cli_servers_inspected, &g_cli_servers_checks_lock, key), 0, key, uri, &written,
                       &out);
    const unsigned int by_checker = emit_kept_read(&g_cli_servers_checked, &g_cli_servers_checks_lock, key);
    const CliJson *const json = &s_reading.json;
    for (unsigned int id = cli_json_is(json, by_checker, CFG_JSON_OBJECT) ? json->node[by_checker].first
                                                                          : CLI_JSON_NONE;
         id != CLI_JSON_NONE; id = json->node[id].next)
    {
        emit_items_written(id, 0, key, uri, &written, &out);
    }
    cli_line_text(&out, "]}");
    unsigned long long told_size = 0ull;
    char *const told = cli_pieces_joined(&pieces, &out, "a file's diagnostics", &told_size);
    if (told != NULL)
    {
        emit->told(emit->context, "lsp-diagnostics", told, told_size);
    }
    cli_cell_given(told);
    cli_servers_reading_released(&s_reading);
}

// a file kept among those whose server reports an attribute, where it does, put on the line that is `context`: its key
// then its URI, each ended by a 0
static void emit_reporting(void *context, const char *key, const char *beside, const char *text,
                           unsigned long long size)
{
    CliLine *const reporting = context;
    static const char s_report[] = "Cannot access attribute \\\"";
    unsigned long long at = 0ull;
    if (cli_text_find(text, size, s_report, sizeof(s_report) - 1u, &at))
    {
        cli_line_counted(reporting, key, cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u);
        cli_line_counted(reporting, beside, cellul.len(&(CatenaFinitaCfg){.src = beside, .cap = CLI_CELLS_POOL}) + 1u);
    }
}

void cli_servers_emit_reporting_attributes(const CliServersEmit *emit)
{
    char piece[CLI_SERVERS_PIECE];
    CliPieces pieces;
    CliLine line;
    cli_pieces_begun(&pieces, &line, piece, sizeof(piece));
    cli_lock_take(&g_cli_servers_published_lock);
    cli_keyed_each(&g_cli_servers_published, emit_reporting, &line);
    cli_lock_give(&g_cli_servers_published_lock);
    unsigned long long size = 0ull;
    char *const reporting = cli_pieces_joined(&pieces, &line, "the files whose server reports an attribute", &size);
    for (unsigned long long at = 0ull; (reporting != NULL) && (at < size);)
    {
        const char *const key = reporting + at;
        const char *const uri = key + cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u;
        cli_servers_emit_whole(key, uri, emit);
        at += cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}) + 1u +
              cellul.len(&(CatenaFinitaCfg){.src = uri, .cap = CLI_CELLS_POOL}) + 1u;
    }
    cli_cell_given(reporting);
}
