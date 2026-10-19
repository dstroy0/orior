// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// db_history.c: each statement run on a connection kept in its history, a JSON line each in orior's own folder under
// databases/, by the account's hash, the newest 1000 of them; the history read back, newest first; and the statements
// of an editor's text found by their spans, counted in characters as the editor counts them
#include "../text/digest.h"
#include "db_internal.h"
#include "sql.h"

// the connection's history file into `path`, which holds CLI_PATH_LONGEST, its folder made: 1, or 0 where orior has
// no home folder
static int history_file(const CliDbOpen *open, char *path)
{
    char home[CLI_PATH_LONGEST];
    char folder[CLI_PATH_LONGEST];
    char account[CLI_PATH_LONGEST];
    char hash[80];
    if (!cli_home_folder(home))
    {
        return 0;
    }
    cli_full(home, "databases", folder);
    if (!query_folder_made(folder))
    {
        return 0;
    }
    CliLine account_line = {account, sizeof(account), 0ull};
    cli_db_account(&open->config, &account_line);
    CliLine hash_line = {hash, sizeof(hash), 0ull};
    cli_sha256_text(account_line.out, account_line.at, &hash_line);
    hash_line.at = 16ull;
    cli_line_text(&hash_line, ".jsonl");
    cli_line_closed(&hash_line);
    cli_full(folder, hash, path);
    return 1;
}

void cli_db_remembered(CliDbOpen *open, const char *statement, unsigned long long size, unsigned long long took,
                       const char *said, int failed)
{
    static char s_entry[1u << 20u];
    static CliLock s_lock = CLI_LOCK_INIT;
    char path[CLI_PATH_LONGEST];
    char next[CLI_PATH_LONGEST + 8u];
    if (!history_file(open, path))
    {
        return;
    }
    CliLine next_line = {next, sizeof(next), 0ull};
    cli_line_text(&next_line, path);
    cli_line_text(&next_line, ".next");
    cli_line_closed(&next_line);
    cli_lock_take(&s_lock);
    CliLine entry = {s_entry, sizeof(s_entry), 0ull};
    cli_line_text(&entry, "{\"at\":");
    cli_line_decimal(&entry, cli_wall_milliseconds(), 1u);
    cli_line_text(&entry, ",\"sql\":");
    cli_json_quoted_lossy(&entry, (const unsigned char *)statement, size);
    cli_line_text(&entry, ",\"ms\":");
    cli_line_decimal(&entry, took, 1u);
    cli_line_text(&entry, failed ? ",\"failed\":" : ",\"said\":");
    cli_json_quoted_lossy(&entry, (const unsigned char *)said,
                          cellul.len(&(CatenaFinitaCfg){.src = said, .cap = CLI_CELLS_POOL}));
    cli_line_text(&entry, "}\n");
    // the history's newest lines but one kept, then the entry after them
    unsigned long long held_size = 0ull;
    const unsigned char *const held = qry_member_file_mapped(path, &held_size);
    // each newline before the last line's end starts one more line kept, counted back from the end
    unsigned long long start = 0ull;
    unsigned long long end =
        ((held != NULL) && (held_size > 0u) && (held[held_size - 1u] == '\n')) ? held_size - 1u : held_size;
    unsigned int kept = 0u;
    for (unsigned long long at = end; (held != NULL) && (at > 0u); at -= 1u)
    {
        if (held[at - 1u] == '\n')
        {
            kept += 1u;
            if (kept == CLI_DB_HISTORY - 1u)
            {
                start = at;
                break;
            }
        }
    }
    int written = cli_line_fits(&entry) && cli_file_appended(next, "", 0ull, 1);
    if (written && (held != NULL) && (start < held_size))
    {
        written = cli_file_appended(next, held + start, held_size - start, 0);
        if (written && (held[held_size - 1u] != '\n'))
        {
            written = cli_file_appended(next, "\n", 1ull, 0);
        }
    }
    if (held != NULL)
    {
        qry_member_file_unmapped(held, held_size);
    }
    written = written && cli_file_appended(next, entry.out, entry.at, 0);
    if (written)
    {
        (void)cli_moved(next, path);
    }
    cli_lock_give(&s_lock);
}

void cli_db_history(CliDbOpen *open, CliLine *out)
{
    char path[CLI_PATH_LONGEST];
    unsigned long long size = 0ull;
    const unsigned char *const bytes = history_file(open, path) ? qry_member_file_mapped(path, &size) : NULL;
    cli_line_character(out, '[');
    unsigned int written = 0u;
    // the lines from the last back, each that reads as JSON
    for (unsigned long long end = size; (bytes != NULL) && (end > 0u);)
    {
        unsigned long long start = end;
        while ((start > 0u) && (bytes[start - 1u] != '\n'))
        {
            start -= 1u;
        }
        CliLine none = {NULL, 0ull, 0ull};
        if ((end > start) && cli_json_compacted((const char *)bytes + start, end - start, &none))
        {
            cli_line_text(out, (written > 0u) ? "," : "");
            cli_line_counted(out, (const char *)bytes + start, end - start);
            written += 1u;
        }
        end = (start > 0u) ? start - 1u : 0u;
    }
    if (bytes != NULL)
    {
        qry_member_file_unmapped(bytes, size);
    }
    cli_line_character(out, ']');
}

// the characters of the first `size` bytes of a text, each counted once at its first byte
static unsigned long long history_characters(const char *text, unsigned long long size)
{
    unsigned long long count = 0ull;
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        count += (((unsigned char)text[at] & 0xc0u) != 0x80u) ? 1ull : 0ull;
    }
    return count;
}

void cli_db_statement_at(const char *text, unsigned long long size, unsigned long long offset, const char *kind,
                         CliLine *out)
{
    // the byte the character at `offset` starts at, the text's end where it is past its last
    unsigned long long byte = 0ull;
    for (unsigned long long seen = 0ull; byte < size; byte += 1ull)
    {
        if (((unsigned char)text[byte] & 0xc0u) != 0x80u)
        {
            if (seen == offset)
            {
                break;
            }
            seen += 1ull;
        }
    }
    CliSqlSpan span;
    if (!cli_sql_statement_at(text, size, byte, cli_sql_dialect(kind), &span))
    {
        cli_line_text(out, "null");
        return;
    }
    cli_line_character(out, '[');
    cli_line_decimal(out, history_characters(text, span.start), 1u);
    cli_line_character(out, ',');
    cli_line_decimal(out, history_characters(text, span.end), 1u);
    cli_line_character(out, ']');
}

// The spans of a text's statements being written: the text and where they go
typedef struct
{
    const char *text;
    CliLine *out;
    unsigned int written;
} CliDbSpans;

static void history_span(void *context, CliSqlSpan span)
{
    CliDbSpans *const spans = (CliDbSpans *)context;
    cli_line_text(spans->out, (spans->written > 0u) ? ",[" : "[");
    cli_line_decimal(spans->out, history_characters(spans->text, span.start), 1u);
    cli_line_character(spans->out, ',');
    cli_line_decimal(spans->out, history_characters(spans->text, span.end), 1u);
    cli_line_character(spans->out, ']');
    spans->written += 1u;
}

void cli_db_statements(const char *text, unsigned long long size, const char *kind, CliLine *out)
{
    CliDbSpans spans = {text, out, 0u};
    cli_line_character(out, '[');
    cli_sql_statements(text, size, cli_sql_dialect(kind), history_span, &spans);
    cli_line_character(out, ']');
}
