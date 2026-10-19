// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// schema.c: a JSON file checked: what kept it from reading, then its root against the schema it names or is known by;
// a schema it names by a URL that orior holds nowhere noted, with the line that fetches it to orior's own folder. A
// file larger than CLI_SCHEMA_LARGEST is not checked. One check or completion runs at a time
#include "../command_line/cells.h"
#include "schema_internal.h"

#include "../text/text.h"

// the largest file checked, and the most lines of it indexed for its findings' places
#define CLI_SCHEMA_LARGEST (2ull << 20u)
#define CLI_SCHEMA_LINES (1u << 20u)

CliSchemaRead *g_cli_schema_read;
CliSchemaStore *g_cli_schema_store;
CliLock g_cli_schema_lock = CLI_LOCK_INIT;

int cli_schema_held(void)
{
    cli_lock_take(&g_cli_schema_lock);
    g_cli_schema_read = cli_cell_taken(sizeof(*g_cli_schema_read), "a file read against its schema");
    g_cli_schema_store = cli_cell_taken(sizeof(*g_cli_schema_store), "the schemas reached");
    if ((g_cli_schema_read == NULL) || (g_cli_schema_store == NULL))
    {
        cli_schema_released();
        return 0;
    }
    return 1;
}

void cli_schema_released(void)
{
    cli_cell_given(g_cli_schema_store);
    cli_cell_given(g_cli_schema_read);
    g_cli_schema_store = NULL;
    g_cli_schema_read = NULL;
    cli_lock_give(&g_cli_schema_lock);
}

static CliBuildFindings s_found;
static unsigned int s_starts[CLI_SCHEMA_LINES];

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

// a schema named by `key` and held nowhere noted over `from` to `to`, with the line that fetches a URL's
static void schema_missing(const char *root, const char *key, const char *text, unsigned long long from,
                           unsigned long long to, const CliBuildLines *lines)
{
    CliLine said = cli_build_said(&s_found);
    cli_line_text(&said, "orior holds no schema at ");
    cli_line_counted(&said, text + from, to - from);
    const unsigned int finding = cli_build_finding(&s_found, &said, lines, from, to, CLI_BUILD_NOTE, "schema-missing");
    char place[CLI_PATH_LONGEST];
    if ((!cellul.starts(&(CatenaFinitaCfg){.src = key, .other = "http", .cap = CLI_CELLS_POOL})) ||
        !cli_schema_kept_at(key, place))
    {
        return;
    }
    for (char *at = place; *at != '\0'; at += 1)
    {
        *at = (*at == '\\') ? '/' : *at;
    }
    CliLine title = cli_build_said(&s_found);
    cli_line_text(&title, "Fetch the schema to ");
    cli_line_text(&title, place);
    char line[CLI_PATH_LONGEST * 3u];
    CliLine command = {line, sizeof(line), 0ull};
    cli_line_text(&command, "curl -fsSL --create-dirs -o '");
    cli_line_text(&command, place);
    cli_line_text(&command, "' '");
    cli_line_text(&command, key);
    cli_line_character(&command, '\'');
    char folder[CLI_PATH_LONGEST];
    cli_build_inside(root, root, folder);
    cli_build_run_fix(&s_found, finding, &title, folder,
                      cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}), line, command.at);
}

void cli_schema_check(const char *root, const char *path, const char *text, unsigned long long size, CliLine *out)
{
    if (size > CLI_SCHEMA_LARGEST)
    {
        cli_line_text(out, "[]");
        return;
    }
    if (!cli_schema_held())
    {
        cli_line_text(out, "[]");
        return;
    }
    cli_build_findings_begun(&s_found);
    CliBuildLines lines;
    cli_build_lines_of(text, size, s_starts, CLI_SCHEMA_LINES, &lines);
    CliSchemaRead *const read = g_cli_schema_read;
    cli_schema_read(text, size, read);
    for (unsigned int at = 0u; at < read->errors; at += 1u)
    {
        const CliSchemaError *const error = &read->error[at];
        const CliLine said = {read->pool + error->message_at, error->message_size + 1u, error->message_size};
        (void)cli_build_finding(&s_found, &said, &lines, error->from, error->to, CLI_BUILD_ERROR, "json");
    }
    char key[CLI_PATH_LONGEST];
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
    cli_schema_released();
}
