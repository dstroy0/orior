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

int cli_schema_lock(void)
{
    g_cli_schema_read = g_cli_cells->persistent_buf_alloc(sizeof(*g_cli_schema_read), &g_cli_code_cells);
    g_cli_schema_store = g_cli_cells->persistent_buf_alloc(sizeof(*g_cli_schema_store), &g_cli_code_cells);
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
        (g_cli_schema_store != NULL) &&
        EMBED_CALL(cli_json_tree_nodes_most.json_reserved_temporary, CliJsonTreeNodesMostCfg,
                   .json = &g_cli_schema_store->json, .nodes_most = CLI_SCHEMA_STORE_NODES_ROOM,
                   .text_most = CLI_SCHEMA_STORE_TEXT_ROOM, .what = "the schemas reached", .owner = &g_cli_code_cells);
    if ((g_cli_schema_read == NULL) || !grown)
    {
        cli_schema_release();
        return 0;
    }
    return 1;
}

void cli_schema_read_free(const CliSchemaReadTextCtx *args)
{
    g_cli_cells->persistent_buf_release(args->read_out->node, &g_cli_schema_cells);
    args->read_out->node = NULL;
    args->read_out->member = NULL;
    args->read_out->pool = NULL;
    args->read_out->nodes_most = 0u;
    args->read_out->members_most = 0u;
    args->read_out->pool_most = 0u;
}

void cli_schema_release(void)
{
    if (g_cli_schema_read != NULL)
    {
        EMBED_CALL(cli_schema_read_text_internal.read_free, CliSchemaReadTextCtx, .read_out = g_cli_schema_read);
    }
    if (g_cli_schema_store != NULL)
    {
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &g_cli_schema_store->json);
    }
    g_cli_cells->persistent_buf_release(g_cli_schema_store, &g_cli_code_cells);
    g_cli_cells->persistent_buf_release(g_cli_schema_read, &g_cli_code_cells);
    g_cli_schema_store = NULL;
    g_cli_schema_read = NULL;
}

static CliBuildFindings s_found;

int cli_schema_reads(const CliSchemaCfg *args)
{
    const char *const dot = EMBED_CALL(
        cli_text_byte.last, CliTextByteCfg, .text = args->path,
        .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}), .byte = '.');
    const char *const slash = EMBED_CALL(
        cli_text_byte.last, CliTextByteCfg, .text = args->path,
        .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}), .byte = '/');
    const char *const back = EMBED_CALL(
        cli_text_byte.last, CliTextByteCfg, .text = args->path,
        .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}), .byte = '\\');
    if ((dot == NULL) || ((slash != NULL) && (slash > dot)) || ((back != NULL) && (back > dot)) ||
        (dot == args->path) || (dot[-1] == '/') || (dot[-1] == '\\'))
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

typedef struct
{
    const char *root;
    const char *key;
    const char *text;
    unsigned long long from;
    unsigned long long to;
    const CliBuildLines *lines;
} SchemaCtx;
// a schema named by `key` and stored nowhere noted over `from` to `to`, with the line that fetches a URL's
EMBED_INLINE void schema_missing(const SchemaCtx *args)
{
    const char *root = args->root;
    const char *text = args->text;
    unsigned long long from = args->from;
    unsigned long long to = args->to;
    const CliBuildLines *lines = args->lines;
    VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = &s_found);
    mmgr_line_put(&msg, "orior_cli stores no schema at ");
    mmgr_line_put_n(&msg, text + from, to - from);
    const unsigned int finding =
        EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = &s_found, .message = &msg,
                   .lines = lines, .from = from, .to = to, .severity = CLI_BUILD_NOTE, .code = "schema-missing");
    char place[CLI_PATH_MAX_LEN];
    if ((!cellul.starts(&(CatenaFinitaCfg){.src = args->key, .other = "http", .cap = CLI_CELLS_POOL})) ||
        !EMBED_CALL(cli_schema_url_internal.cache_path, CliSchemaUrlCtx, .url = args->key, .out_room = place))
    {
        return;
    }
    EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = place,
               .size = cellul.len(&(CatenaFinitaCfg){.src = place, .cap = CLI_CELLS_POOL}), .from = '\\', .to = '/');
    VerbaLinea title = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = &s_found);
    mmgr_line_put(&title, "Fetch the schema to ");
    mmgr_line_put(&title, place);
    char line[CLI_PATH_MAX_LEN * 3u];
    VerbaLinea command = {line, sizeof(line), 0ull};
    mmgr_line_put(&command, "curl -fsSL --create-dirs -o '");
    mmgr_line_put(&command, place);
    mmgr_line_put(&command, "' '");
    mmgr_line_put(&command, args->key);
    mmgr_line_ch(&command, '\'');
    char folder[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_builds_root.build_inside, CliBuildsRootCfg, .root = root, .folder = root, .out_room = folder);
    EMBED_CALL(cli_builds_found.build_run_fix, CliBuildsFoundCfg, .found = &s_found, .finding = finding,
               .title = &title, .folder = folder,
               .folder_size = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}), .line_text = line,
               .line_size = command.at);
}

void cli_schema_check(const CliSchemaCfg *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    VerbaLinea *out = args->out;
    if (size > CLI_SCHEMA_LARGEST)
    {
        mmgr_line_put(out, "[]");
        return;
    }
    if (!cli_schema_lock())
    {
        mmgr_line_put(out, "[]");
        return;
    }
    EMBED_CALL(cli_builds_lines.build_findings_begin, CliBuildsLinesCfg, .found = &s_found);
    CliBuildLines lines;
    unsigned int *const starts = EMBED_CALL(cli_builds_text_size.build_lines_take, CliBuildsTextSizeCfg, .text = text,
                                            .size = size, .lines_out = &lines);
    CliSchemaRead *const read = g_cli_schema_read;
    EMBED_CALL(cli_schema_read_text_internal.read, CliSchemaReadTextCtx, .text = text, .size = size, .read_out = read);
    for (unsigned int at = 0u; at < read->errors; at += 1u)
    {
        const CliSchemaError *const error = &read->error[at];
        const VerbaLinea msg = {read->pool + error->message_at, error->message_size + 1u, error->message_size};
        (void)EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = &s_found, .message = &msg,
                         .lines = &lines, .from = error->from, .to = error->to, .severity = CLI_BUILD_ERROR,
                         .code = "json");
    }
    char key[CLI_PATH_MAX_LEN];
    int named = 0;
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    if ((read->root != CLI_SCHEMA_NONE) &&
        EMBED_CALL(cli_schema_read_internal.get, CliSchemaReadCtx, .path = args->path, .read = read, .key = key,
                   .named = &named, .from = &from, .to = &to))
    {
        EMBED_CALL(cli_schema_store_internal.store_begin, CliSchemaStoreCtx, .store = g_cli_schema_store);
        const unsigned int schema =
            EMBED_CALL(cli_schema_store_internal.load, CliSchemaStoreCtx, .store = g_cli_schema_store, .key_read = key);
        if (schema != CLI_SCHEMA_NONE)
        {
            CliSchemaCheck check = {g_cli_schema_store, read, &lines, text, &s_found};
            const CliSchemaKey none = {0, 0ull, 0ull};
            EMBED_CALL(cli_schema_base_internal.node, CliSchemaBaseCtx, .check = &check, .node = read->root,
                       .schema = schema, .base = key, .key_schema_key = none, .depth = 0u);
        }
        else if (named)
        {
            EMBED_CALL(schema_missing, SchemaCtx, .root = args->root, .key = key, .text = text, .from = from, .to = to,
                       .lines = &lines);
        }
    }
    EMBED_CALL(cli_builds_table.build_findings_write, CliBuildsCfg, .found_read = &s_found, .out = out);
    EMBED_CALL(cli_builds_lines.build_findings_free, CliBuildsLinesCfg, .found = &s_found);
    g_cli_cells->persistent_buf_release(starts, &g_cli_build_cells);
    cli_schema_release();
}
