// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_lite.c: a SQLite database open in a process of the tree's Python, the shim stored in a file of orior's own
// temporary folder and run from it; a request written a line at a time and its answer read back a line, its rows read a
// few lines at a time, and an interrupt sent from another thread. Each process's place is a cell taken as it opens and
// given back as it closes, its place here empty while none is open in it
#include "../servers/lsp.h"
#include "../tests/test_runs.h"
#include "db_internal.h"

static CliDbLite *s_lites[CLI_DB_LITES];
static CliLock s_lites_lock = CLI_LOCK_INIT;

// the line written to the process's input: 1, or 0 with the failure given
static int lite_written(CliDbLite *lite, const char *line, unsigned long long size, CliDbFailure *failure)
{
    cli_lock_take(&lite->input_lock);
    const int written = cli_spawn_written(lite->child.in, line, size) && cli_spawn_written(lite->child.in, "\n", 1ull);
    cli_lock_give(&lite->input_lock);
    if (!written)
    {
        cli_db_failure_set(failure, "the connection broke: SQLite's process takes no more");
    }
    return written;
}

// the next line the process prints read into its room, without its newline: 1, or 0 with the failure given
static int lite_line(CliDbLite *lite, CliDbFailure *failure)
{
    lite->size = 0ull;
    int fits = 1;
    for (;;)
    {
        if (lite->start == lite->end)
        {
            const unsigned long long got = cli_spawn_read(lite->child.out, (char *)lite->in, sizeof(lite->in));
            if (got == 0ull)
            {
                cli_db_failure_set(failure, "the connection broke: SQLite's process ended");
                return 0;
            }
            lite->start = 0ull;
            lite->end = got;
        }
        const unsigned char *const newline = memor.chr(
            &(MemoriaCfg){.src = lite->in + lite->start, .val = '\n', .bytes = (size_t)(lite->end - lite->start)});
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - lite->in) : lite->end;
        const unsigned long long piece = end - lite->start;
        fits = fits && (lite->size + piece < sizeof(lite->line));
        if (fits)
        {
            memor.cpy(
                &(MemoriaCfg){.dst = lite->line + lite->size, .src = lite->in + lite->start, .bytes = (size_t)piece});
            lite->size += piece;
        }
        lite->start = (newline != NULL) ? end + 1u : end;
        if (newline != NULL)
        {
            break;
        }
    }
    if (!fits)
    {
        cli_db_failure_set(failure, "SQLite's process printed more in one line than orior_cli reads");
    }
    return fits;
}

// a member of the line the process printed: its start and length, 0 where it has none
static int lite_member(const CliDbLite *lite, const char *name, unsigned long long *start, unsigned long long *length)
{
    return cli_lsp_member(lite->line, lite->size, name, start, length);
}

// `request` sent and its answer read into the line: whether a transaction is open recorded, and an error given as the
// failure. 1, or 0 with the failure given
static int lite_query(CliDbLite *lite, const char *request, unsigned long long size, CliDbFailure *failure)
{
    if (!lite_written(lite, request, size, failure) || !lite_line(lite, failure))
    {
        return 0;
    }
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    if (!cli_json_compacted(lite->line, lite->size, &(VerbaLinea){NULL, 0ull, 0ull}))
    {
        cli_db_failure_set(failure, "SQLite's process printed what orior_cli cannot read");
        return 0;
    }
    lite->in_transaction = lite_member(lite, "tx", &start, &length) && (length == 4u) &&
                           (memor.cmp(&(MemoriaCfg){.src = lite->line + start, .other = "true", .bytes = 4u}) == 0);
    if (lite_member(lite, "error", &start, &length))
    {
        lite->reading = 0;
        memor.set(&(MemoriaCfg){.dst = failure, .val = 0, .bytes = sizeof(*failure)});
        (void)cli_lsp_string(lite->line + start, length, failure->message, sizeof(failure->message));
        if (lite_member(lite, "code", &start, &length))
        {
            (void)cli_lsp_string(lite->line + start, length, failure->code, sizeof(failure->code));
        }
        return 0;
    }
    return 1;
}

// the shim written to orior's temporary folder, once, its path into `path`: 1, or 0 where it could not be
static int lite_shim(char *path)
{
    static int s_written;
    static CliLock s_lock = CLI_LOCK_INIT;
    char temporary[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    cli_temp_folder(temporary);
    cli_full(temporary, "orior-sqlite", folder);
    cli_full(folder, "orior_sqlite.py", path);
    cli_lock_take(&s_lock);
    if (!s_written)
    {
        s_written =
            query_folder_made(folder) &&
            query_file_written(path, g_cli_db_lite_shim,
                               cellul.len(&(CatenaFinitaCfg){.src = g_cli_db_lite_shim, .cap = CLI_CELLS_POOL}));
    }
    const int written = s_written;
    cli_lock_give(&s_lock);
    return written;
}

CliDbLite *cli_db_lite_open(const char *file, int read_only, CliDbFailure *failure)
{
    char python[CLI_PATH_MAX_LEN];
    char shim[CLI_PATH_MAX_LEN];
    if (!cli_test_python(python))
    {
        cli_db_failure_set(failure, "no Python is found to read SQLite with");
        memor.cpy(&(MemoriaCfg){
            .dst = failure->hint,
            .src = "File, Toolchains finds Python, or takes its folder.",
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "File, Toolchains finds Python, or takes its folder.",
                                                   .cap = CLI_CELLS_POOL}) +
                     1u});
        return NULL;
    }
    if (!lite_shim(shim))
    {
        cli_db_failure_set(failure, "orior's SQLite shim could not be written to the temporary folder");
        return NULL;
    }
    CliDbLite *lite = NULL;
    cli_lock_take(&s_lites_lock);
    unsigned int entry_at = 0u;
    while ((entry_at < CLI_DB_LITES) && (s_lites[entry_at] != NULL))
    {
        entry_at += 1u;
    }
    if (entry_at < CLI_DB_LITES)
    {
        lite = g_cli_cells->persistent_buf_alloc(sizeof(CliDbLite), &g_cli_db_cells);
        s_lites[entry_at] = lite;
    }
    cli_lock_give(&s_lites_lock);
    if (lite == NULL)
    {
        cli_db_failure_set(failure, (entry_at < CLI_DB_LITES)
                                        ? "no room is left to open a SQLite database in"
                                        : "orior_cli has as many SQLite databases open as it can");
        return NULL;
    }
    lite->start = 0ull;
    lite->end = 0ull;
    lite->in_transaction = 0;
    lite->reading = 0;
    lite->count = 0ull;
    lite->input_lock = (CliLock)CLI_LOCK_INIT;
    cli_toolchains_run_path(lite->path_text, sizeof(lite->path_text));
    const CliSetting settings[2] = {{"PATH", lite->path_text}, {"PYTHONIOENCODING", "utf-8"}};
    const char *const words[5] = {python, "-I", "-u", shim, NULL};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    if (!cli_spawn_fed(&lite->child, words, NULL, settings, 2u, 1, &err_msg))
    {
        char message[CLI_ERR_MSG_MAX_LEN + 16u];
        VerbaLinea line = {message, sizeof(message), 0ull};
        mmgr_line_put(&line, "Python: ");
        mmgr_line_put_n(&line, err_msg.out, err_msg.at);
        mmgr_line_finish(&line);
        cli_db_failure_set(failure, message);
        cli_lock_take(&s_lites_lock);
        s_lites[entry_at] = NULL;
        cli_lock_give(&s_lites_lock);
        g_cli_cells->persistent_buf_release(lite, &g_cli_db_cells);
        return NULL;
    }
    char request[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea query = {request, sizeof(request), 0ull};
    mmgr_line_put(&query, "{\"open\":");
    cli_json_quoted_lossy(&query, (const unsigned char *)file,
                          cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(&query, read_only ? ",\"readonly\":true}" : ",\"readonly\":false}");
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    if (!lite_query(lite, query.out, query.at, failure))
    {
        cli_db_lite_closed(lite);
        return NULL;
    }
    lite->version[0] = '\0';
    if (lite_member(lite, "version", &start, &length))
    {
        (void)cli_lsp_string(lite->line + start, length, lite->version, sizeof(lite->version));
    }
    return lite;
}

void cli_db_lite_closed(CliDbLite *lite)
{
    cli_spawn_input_closed(&lite->child);
    cli_spawn_killed(&lite->child);
    (void)cli_spawn_wait(&lite->child);
    cli_spawn_closed(&lite->child);
    cli_lock_take(&s_lites_lock);
    for (unsigned int entry_at = 0u; entry_at < CLI_DB_LITES; entry_at += 1u)
    {
        s_lites[entry_at] = (s_lites[entry_at] == lite) ? NULL : s_lites[entry_at];
    }
    cli_lock_give(&s_lites_lock);
    g_cli_cells->persistent_buf_release(lite, &g_cli_db_cells);
}

int cli_db_lite_interrupted(CliDbLite *lite, VerbaLinea *err_msg)
{
    static const char s_interrupt[] = "{\"interrupt\":true}\n";
    cli_lock_take(&lite->input_lock);
    const int written = cli_spawn_written(lite->child.in, s_interrupt, sizeof(s_interrupt) - 1u);
    cli_lock_give(&lite->input_lock);
    if (!written)
    {
        mmgr_line_put(err_msg, "SQLite's process takes no more");
    }
    return written;
}

// where the JSON value at `at` of `size` bytes of `text` ends: past it, or `size` where it runs on
static unsigned long long lite_value_end(const char *text, unsigned long long size, unsigned long long at)
{
    unsigned int depth = 0u;
    int quoted = 0;
    for (; at < size; at += 1ull)
    {
        const char one = text[at];
        if (quoted)
        {
            at += (one == '\\') ? 1ull : 0ull;
            quoted = (one != '"');
            if (!quoted && (depth == 0u))
            {
                return at + 1ull;
            }
            continue;
        }
        if (one == '"')
        {
            quoted = 1;
        }
        else if ((one == '[') || (one == '{'))
        {
            depth += 1u;
        }
        else if ((one == ']') || (one == '}'))
        {
            // a closing bracket past a value at the top ends it before the bracket, and one that closes the value's
            // own after the bracket
            if (depth == 0u)
            {
                return at;
            }
            depth -= 1u;
            if (depth == 0u)
            {
                return at + 1ull;
            }
        }
        else if ((depth == 0u) &&
                 ((one == ',') ||
                  (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r\n", .cap = CLI_CELLS_POOL, .byte = one}) != NULL)))
        {
            return at;
        }
    }
    return size;
}

// the items of the JSON array of `length` bytes at `text`, each handed to `each` by its start and length
static void lite_items(const char *text, unsigned long long length,
                       void (*each)(void *context, const char *item, unsigned long long size), void *context)
{
    unsigned long long at = 0ull;
    while ((at < length) && (text[at] != '['))
    {
        at += 1ull;
    }
    for (at += 1ull; at < length;)
    {
        while ((at < length) &&
               (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r\n,", .cap = CLI_CELLS_POOL, .byte = text[at]}) != NULL))
        {
            at += 1ull;
        }
        if ((at >= length) || (text[at] == ']'))
        {
            return;
        }
        const unsigned long long end = lite_value_end(text, length, at);
        each(context, text + at, end - at);
        at = (end > at) ? end : at + 1ull;
    }
}

// a column's name stored in the head
static void lite_column(void *context, const char *item, unsigned long long size)
{
    CliDbLite *const lite = (CliDbLite *)context;
    if (lite->head.column_count < CLI_DB_COLUMNS)
    {
        CliDbColumn *const column = &lite->head.columns[lite->head.column_count];
        column->name[0] = '\0';
        (void)cli_lsp_string(item, size, column->name, sizeof(column->name));
        column->kind[0] = '\0';
        column->numeric = 0;
        lite->head.column_count += 1u;
    }
}

int cli_db_lite_start(CliDbLite *lite, const char *statement, unsigned long long size, CliDbFailure *failure)
{
    char tag[CLI_DB_TAG];
    for (int done = !lite->reading; !done;)
    {
        if (!cli_db_lite_rows(lite, 4096ull, NULL, NULL, &done, tag, failure))
        {
            return 0;
        }
    }
    // the request a cell of the statement's quoted size, six bytes at most for each of its bytes, and no more than a
    // line SQLite's process reads
    const unsigned long long quoted = (6ull * size) + 16ull;
    const unsigned long long room = (quoted < CLI_DB_LITE_LINE) ? quoted : CLI_DB_LITE_LINE;
    char *const request = g_cli_cells->temporary_buf_alloc(room, &g_cli_db_cells);
    VerbaLinea query = {request, (request != NULL) ? room : 0ull, 0ull};
    mmgr_line_put(&query, "{\"start\":");
    cli_json_quoted_lossy(&query, (const unsigned char *)statement, size);
    mmgr_line_ch(&query, '}');
    const int fits = mmgr_line_ok(&query);
    const int answered = fits && lite_query(lite, query.out, query.at, failure);
    g_cli_cells->persistent_buf_release(request, &g_cli_db_cells);
    if (!fits)
    {
        cli_db_failure_set(failure, (request != NULL) ? "the statement is longer than orior_cli sends SQLite's process"
                                                      : "no room is left to send the statement in");
    }
    if (!answered)
    {
        return 0;
    }
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    lite->head.column_count = 0u;
    if (lite_member(lite, "done", &start, &length) && (lite->line[start] == '"'))
    {
        lite->head.rows = 0;
        lite->head.tag[0] = '\0';
        (void)cli_lsp_string(lite->line + start, length, lite->head.tag, sizeof(lite->head.tag));
        return 1;
    }
    lite->reading = 1;
    lite->count = 0ull;
    lite->head.rows = 1;
    if (lite_member(lite, "columns", &start, &length))
    {
        lite_items(lite->line + start, length, lite_column, lite);
    }
    return 1;
}

// The reading of one line's rows: the process, the text its values are decoded into so far, how many values a row has
// so far, where its rows go, and whether they are wanted still
typedef struct
{
    CliDbLite *lite;
    unsigned long long used;
    unsigned int values;
    CliDbRowFn each_row;
    void *context;
    int going;
    unsigned long long rows;
} CliDbLiteRead;

// a value of a row decoded into the line's text
static void lite_value(void *context, const char *item, unsigned long long size)
{
    CliDbLiteRead *const read = (CliDbLiteRead *)context;
    CliDbLite *const lite = read->lite;
    if (read->values == CLI_DB_COLUMNS)
    {
        return;
    }
    CliDbValue *const value = &lite->row[read->values];
    read->values += 1u;
    if ((size == 4u) && (memor.cmp(&(MemoriaCfg){.src = item, .other = "null", .bytes = 4u}) == 0))
    {
        *value = (CliDbValue){NULL, 0ull, 1};
        return;
    }
    char *const text = lite->text + read->used;
    if (!cli_lsp_string(item, size, text, sizeof(lite->text) - read->used))
    {
        *value = (CliDbValue){"", 0ull, 0};
        return;
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    *value = (CliDbValue){text, length, 0};
    read->used += length + 1ull;
}

// a row of the line read and handed on
static void lite_row(void *context, const char *item, unsigned long long size)
{
    CliDbLiteRead *const read = (CliDbLiteRead *)context;
    read->values = 0u;
    read->used = 0ull;
    lite_items(item, size, lite_value, read);
    read->rows += 1ull;
    if (read->going && (read->each_row != NULL))
    {
        read->going = read->each_row(read->context, read->lite->row, read->values);
    }
}

int cli_db_lite_rows(CliDbLite *lite, unsigned long long count, CliDbRowFn each_row, void *context, int *done,
                     char *tag, CliDbFailure *failure)
{
    *done = 0;
    tag[0] = '\0';
    if (!lite->reading)
    {
        *done = 1;
        return 1;
    }
    CliDbLiteRead read = {lite, 0ull, 0u, each_row, context, 1, 0ull};
    while (read.going && (read.rows < count))
    {
        const unsigned long long left = count - read.rows;
        char request[64];
        VerbaLinea query = {request, sizeof(request), 0ull};
        mmgr_line_put(&query, "{\"rows\":");
        mmgr_line_decimal(&query, (left < CLI_DB_LITE_ROWS) ? left : CLI_DB_LITE_ROWS, 1u);
        mmgr_line_ch(&query, '}');
        if (!lite_query(lite, query.out, query.at, failure))
        {
            return 0;
        }
        unsigned long long start = 0ull;
        unsigned long long length = 0ull;
        const unsigned long long before = read.rows;
        if (lite_member(lite, "rows", &start, &length))
        {
            lite_items(lite->line + start, length, lite_row, &read);
        }
        lite->count += read.rows - before;
        const int ended = !lite_member(lite, "done", &start, &length) ||
                          ((length == 4u) &&
                           (memor.cmp(&(MemoriaCfg){.src = lite->line + start, .other = "true", .bytes = 4u}) == 0));
        if (ended)
        {
            lite->reading = 0;
            VerbaLinea out = {tag, CLI_DB_TAG, 0ull};
            mmgr_line_decimal(&out, lite->count, 1u);
            mmgr_line_put(&out, (lite->count == 1ull) ? " row read" : " rows read");
            mmgr_line_finish(&out);
            *done = 1;
            return 1;
        }
    }
    return 1;
}
