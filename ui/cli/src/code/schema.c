// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// schema.c: a JSON file checked: what prevented it from reading, then its root against the schema it names or is known
// by; a schema it names by a URL that orior_cli stores nowhere noted, with the line that fetches it to orior's own
// folder. A file larger than CLI_SCHEMA_LARGEST is not checked. One check or completion runs at a time
#include "../command_line/cells.h"
#include "schema_internal.h"

#include "../text/text.h"

// the largest file checked
#define CLI_SCHEMA_LARGEST (2ull << 20u)

CliSchemaRead *g_cli_schema_read;
CliSchemaStore *g_cli_schema_store;
CliLock g_cli_schema_lock = CLI_LOCK_INIT;

int cli_schema_locked(void)
{
    cli_lock_take(&g_cli_schema_lock);
    g_cli_schema_read = g_cli_cells->persistent_buf_alloc(sizeof(*g_cli_schema_read), NULL);
    g_cli_schema_store = g_cli_cells->persistent_buf_alloc(sizeof(*g_cli_schema_store), NULL);
    if (g_cli_schema_read != NULL)
    {
        g_cli_schema_read->node = NULL;
        g_cli_schema_read->member = NULL;
        g_cli_schema_read->pool = NULL;
        g_cli_schema_read->nodes_most = 0u;
        g_cli_schema_read->members_most = 0u;
        g_cli_schema_read->pool_most = 0u;
    }
    const int grown =
        (g_cli_schema_store != NULL) && cli_json_growing(&g_cli_schema_store->json, CLI_SCHEMA_STORE_NODES_FIRST,
                                                         CLI_SCHEMA_STORE_TEXT_FIRST, "the schemas reached");
    if ((g_cli_schema_read == NULL) || !grown)
    {
        cli_schema_released();
        return 0;
    }
    return 1;
}

void *cli_schema_grown(void *cells, unsigned int *most, unsigned long long needed, size_t each, unsigned int hard,
                       const char *what)
{
    if (needed <= *most)
    {
        return cells;
    }
    if (needed > hard)
    {
        return NULL;
    }
    unsigned long long grown = (*most == 0u) ? CLI_SCHEMA_FIRST : *most;
    while (grown < needed)
    {
        grown *= 2ull;
    }
    grown = (grown < hard) ? grown : hard;
    unsigned char *const moved = g_cli_cells->persistent_buf_alloc(grown * each, &g_cli_schema_cells);
    if (moved == NULL)
    {
        return NULL;
    }
    if (cells != NULL)
    {
        // the places stored are below the list's room, which a size_t stores
        memor.cpy(&(MemoriaCfg){.dst = moved, .src = cells, .bytes = (size_t)*most * each});
    }
    g_cli_cells->persistent_buf_release(cells, &g_cli_schema_cells);
    // no more than `hard`, an unsigned int
    *most = (unsigned int)grown;
    return moved;
}

int cli_schema_read_room(CliSchemaRead *read, unsigned long long nodes, unsigned long long members,
                         unsigned long long pool)
{
    CliSchemaNode *const node = cli_schema_grown(read->node, &read->nodes_most, nodes, sizeof(*node), CLI_SCHEMA_NODES,
                                                 "a schema read's values");
    read->node = (node != NULL) ? node : read->node;
    CliSchemaMember *const member = cli_schema_grown(read->member, &read->members_most, members, sizeof(*member),
                                                     CLI_SCHEMA_MEMBERS, "a schema read's members");
    read->member = (member != NULL) ? member : read->member;
    char *const text =
        cli_schema_grown(read->pool, &read->pool_most, pool, 1u, CLI_SCHEMA_POOL, "a schema read's texts");
    read->pool = (text != NULL) ? text : read->pool;
    return (nodes <= read->nodes_most) && (members <= read->members_most) && (pool <= read->pool_most);
}

void cli_schema_read_given(CliSchemaRead *read)
{
    g_cli_cells->persistent_buf_release(read->node, &g_cli_schema_cells);
    g_cli_cells->persistent_buf_release(read->member, &g_cli_schema_cells);
    g_cli_cells->persistent_buf_release(read->pool, &g_cli_schema_cells);
    read->node = NULL;
    read->member = NULL;
    read->pool = NULL;
    read->nodes_most = 0u;
    read->members_most = 0u;
    read->pool_most = 0u;
}

void cli_schema_released(void)
{
    if (g_cli_schema_read != NULL)
    {
        cli_schema_read_given(g_cli_schema_read);
    }
    if (g_cli_schema_store != NULL)
    {
        cli_json_given(&g_cli_schema_store->json);
    }
    g_cli_cells->persistent_buf_release(g_cli_schema_store, NULL);
    g_cli_cells->persistent_buf_release(g_cli_schema_read, NULL);
    g_cli_schema_store = NULL;
    g_cli_schema_read = NULL;
    cli_lock_give(&g_cli_schema_lock);
}

static CliBuildFindings s_found;

int cli_schema_reads(const char *path)
{
    const char *const dot =
        cli_text_last(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), '.');
    const char *const slash =
        cli_text_last(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), '/');
    const char *const back =
        cli_text_last(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), '\\');
    if ((dot == NULL) || ((slash != NULL) && (slash > dot)) || ((back != NULL) && (back > dot)) || (dot == path) ||
        (dot[-1] == '/') || (dot[-1] == '\\'))
    {
        return 0;
    }
    char lower[8] = {0};
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = dot + 1, .cap = CLI_CELLS_POOL});
    if (length >= sizeof(lower))
    {
        return 0;
    }
    for (size_t at = 0u; at < length; at += 1u)
    {
        lower[at] = ((dot[1 + at] >= 'A') && (dot[1 + at] <= 'Z')) ? (char)(dot[1 + at] - 'A' + 'a') : dot[1 + at];
    }
    return (cellul.eq(&(CatenaFinitaCfg){.src = lower, .other = "json", .cap = CLI_CELLS_POOL})) ||
           (cellul.eq(&(CatenaFinitaCfg){.src = lower, .other = "jsonc", .cap = CLI_CELLS_POOL}));
}

// a schema named by `key` and stored nowhere noted over `from` to `to`, with the line that fetches a URL's
static void schema_missing(const char *root, const char *key, const char *text, unsigned long long from,
                           unsigned long long to, const CliBuildLines *lines)
{
    VerbaLinea msg = cli_build_msg_line(&s_found);
    mmgr_line_put(&msg, "orior_cli stores no schema at ");
    mmgr_line_put_n(&msg, text + from, to - from);
    const unsigned int finding = cli_build_finding(&s_found, &msg, lines, from, to, CLI_BUILD_NOTE, "schema-missing");
    char place[CLI_PATH_MAX_LEN];
    if ((!cellul.starts(&(CatenaFinitaCfg){.src = key, .other = "http", .cap = CLI_CELLS_POOL})) ||
        !cli_schema_cache_path(key, place))
    {
        return;
    }
    for (char *at = place; *at != '\0'; at += 1)
    {
        *at = (*at == '\\') ? '/' : *at;
    }
    VerbaLinea title = cli_build_msg_line(&s_found);
    mmgr_line_put(&title, "Fetch the schema to ");
    mmgr_line_put(&title, place);
    char line[CLI_PATH_MAX_LEN * 3u];
    VerbaLinea command = {line, sizeof(line), 0ull};
    mmgr_line_put(&command, "curl -fsSL --create-dirs -o '");
    mmgr_line_put(&command, place);
    mmgr_line_put(&command, "' '");
    mmgr_line_put(&command, key);
    mmgr_line_ch(&command, '\'');
    char folder[CLI_PATH_MAX_LEN];
    cli_build_inside(root, root, folder);
    cli_build_run_fix(&s_found, finding, &title, folder,
                      cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}), line, command.at);
}

void cli_schema_check(const char *root, const char *path, const char *text, unsigned long long size, VerbaLinea *out)
{
    if (size > CLI_SCHEMA_LARGEST)
    {
        mmgr_line_put(out, "[]");
        return;
    }
    if (!cli_schema_locked())
    {
        mmgr_line_put(out, "[]");
        return;
    }
    cli_build_findings_begun(&s_found);
    CliBuildLines lines;
    unsigned int *const starts = cli_build_lines_taken(text, size, &lines);
    CliSchemaRead *const read = g_cli_schema_read;
    cli_schema_read(text, size, read);
    const unsigned long long checked = (unsigned long long)read->nodes + read->members + CLI_SCHEMA_CHECK_NODES;
    (void)cli_schema_read_room(read, (checked < CLI_SCHEMA_NODES) ? checked : CLI_SCHEMA_NODES, 0ull, 0ull);
    for (unsigned int at = 0u; at < read->errors; at += 1u)
    {
        const CliSchemaError *const error = &read->error[at];
        const VerbaLinea msg = {read->pool + error->message_at, error->message_size + 1u, error->message_size};
        (void)cli_build_finding(&s_found, &msg, &lines, error->from, error->to, CLI_BUILD_ERROR, "json");
    }
    char key[CLI_PATH_MAX_LEN];
    int named = 0;
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    if ((read->root != CLI_SCHEMA_NONE) && cli_schema_of(path, read, key, &named, &from, &to))
    {
        cli_schema_store_begun(g_cli_schema_store);
        const unsigned int schema = cli_schema_load(g_cli_schema_store, key);
        if (schema != CLI_SCHEMA_NONE)
        {
            CliSchemaCheck check = {g_cli_schema_store, read, &lines, text, &s_found};
            const CliSchemaKey none = {0, 0ull, 0ull};
            cli_schema_node(&check, read->root, schema, key, none, 0u);
        }
        else if (named)
        {
            schema_missing(root, key, text, from, to, &lines);
        }
    }
    cli_build_findings_written(&s_found, out);
    cli_build_findings_given(&s_found);
    g_cli_cells->persistent_buf_release(starts, &g_cli_build_cells);
    cli_schema_released();
}
