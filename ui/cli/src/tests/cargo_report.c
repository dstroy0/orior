// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cargo_report.c: what cargo test and the test harness write, read: results, failures, the places they name, a build's
// first error, and the folder a package's paths are written from
#include "cargo_report.h"

#include "../text/text.h"

// the most failures one report names, and the bytes their messages take
#define CLI_CARGO_FAILURES 4096u
#define CLI_CARGO_FAILURE_TEXT (1u << 20u)

// the next line of `size` bytes from `at`, a "\r" before its "\n" taken off, as its start and length: 1, or 0 where
// none is left
static int report_line(const char *text, unsigned long long size, unsigned long long *at, unsigned long long *start,
                       unsigned long long *length)
{
    if (*at >= size)
    {
        return 0;
    }
    *start = *at;
    const char *const newline = memor.chr(&(MemoriaCfg){.src = text + *at, .val = '\n', .bytes = size - *at});
    unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - text) : size;
    *at = (newline != NULL) ? end + 1ull : size;
    end -= ((newline != NULL) && (end > *start) && (text[end - 1ull] == '\r')) ? 1ull : 0ull;
    *length = end - *start;
    return 1;
}

// `length` bytes from `start` trimmed of ASCII whitespace, as their start and length
static void report_trimmed(const char *text, unsigned long long *start, unsigned long long *length)
{
    while ((*length > 0ull) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r\n\f\v", .cap = CLI_CELLS_POOL, .byte = text[*start]}) != NULL))
    {
        *start += 1ull;
        *length -= 1ull;
    }
    while ((*length > 0ull) &&
           (cellul.chr(&(CatenaFinitaCfg){
                .src = " \t\r\n\f\v", .cap = CLI_CELLS_POOL, .byte = text[*start + *length - 1ull]}) != NULL))
    {
        *length -= 1ull;
    }
}

// whether `length` bytes from `start` begin with `head`
static int report_starts(const char *text, unsigned long long start, unsigned long long length, const char *head)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = head, .cap = CLI_CELLS_POOL});
    return (length >= size) && (memor.cmp(&(MemoriaCfg){.src = text + start, .other = head, .bytes = size}) == 0);
}

int cli_cargo_result_line(const char *line, unsigned long long size, unsigned long long *name,
                          unsigned long long *name_size, const char **outcome)
{
    if (!report_starts(line, 0ull, size, "test "))
    {
        return 0;
    }
    unsigned long long split = 5ull;
    while ((split + 5ull <= size) &&
           (memor.cmp(&(MemoriaCfg){.src = line + split, .other = " ... ", .bytes = 5u}) != 0))
    {
        split += 1ull;
    }
    if (split + 5ull > size)
    {
        return 0;
    }
    unsigned long long start = 5ull;
    unsigned long long length = split - 5ull;
    report_trimmed(line, &start, &length);
    static const char s_panic[] = " - should panic";
    while ((length >= sizeof(s_panic) - 1ull) &&
           (memor.cmp(&(MemoriaCfg){.src = line + start + length - (sizeof(s_panic) - 1ull),
                                    .other = s_panic,
                                    .bytes = sizeof(s_panic) - 1ull}) == 0))
    {
        length -= sizeof(s_panic) - 1ull;
    }
    unsigned long long said = split + 5ull;
    unsigned long long said_size = size - said;
    report_trimmed(line, &said, &said_size);
    if ((said_size == 2ull) && (memor.cmp(&(MemoriaCfg){.src = line + said, .other = "ok", .bytes = 2u}) == 0))
    {
        *outcome = "passed";
    }
    else if ((said_size == 6ull) && (memor.cmp(&(MemoriaCfg){.src = line + said, .other = "FAILED", .bytes = 6u}) == 0))
    {
        *outcome = "failed";
    }
    else if (report_starts(line, said, said_size, "ignored"))
    {
        *outcome = "skipped";
    }
    else
    {
        return 0;
    }
    *name = start;
    *name_size = length;
    return 1;
}

int cli_cargo_place_of(const char *text, unsigned long long size, char *path, long long *line)
{
    unsigned long long start = 0ull;
    unsigned long long length = size;
    report_trimmed(text, &start, &length);
    // the column after the last colon, the line before it, and the path before that
    const char *const base = text + start;
    unsigned long long column = length;
    while ((column > 0ull) && (base[column - 1ull] != ':'))
    {
        column -= 1ull;
    }
    if (column == 0ull)
    {
        return 0;
    }
    unsigned long long number = column - 1ull;
    while ((number > 0ull) && (base[number - 1ull] != ':'))
    {
        number -= 1ull;
    }
    if (number == 0ull)
    {
        return 0;
    }
    unsigned long long value = 0ull;
    const unsigned long long digits_end = column - 1ull;
    const unsigned long long digits_start = number + ((base[number] == '+') ? 1ull : 0ull);
    if (digits_start >= digits_end)
    {
        return 0;
    }
    for (unsigned long long at = digits_start; at < digits_end; at += 1ull)
    {
        if ((base[at] < '0') || (base[at] > '9') || (value > 429496729ull))
        {
            return 0;
        }
        value = (value * 10ull) + (unsigned long long)(base[at] - '0');
    }
    if (value > 4294967295ull)
    {
        return 0;
    }
    CliLine out = {path, CLI_PATH_LONGEST, 0ull};
    for (unsigned long long at = 0ull; at + 1ull < number; at += 1ull)
    {
        cli_line_character(&out, (base[at] == '\\') ? '/' : base[at]);
    }
    cli_line_closed(&out);
    *line = (long long)value;
    return 1;
}

// One failure the report names: its name's start and size in the report, its message's in the failures' texts, and
// its place
typedef struct
{
    unsigned long long name;
    unsigned long long name_size;
    unsigned long long message;
    unsigned long long message_size;
    char path[CLI_PATH_LONGEST];
    int placed;
    long long line;
} CliCargoFailure;

static CliCargoFailure s_failures[CLI_CARGO_FAILURES];
static char s_failure_text[CLI_CARGO_FAILURE_TEXT];
static CliLock s_failure_lock = CLI_LOCK_INIT;

// the failure the lines after a test's `---- name stdout ----` give: its message and place, the lines read past
static void report_failure_read(const char *out, unsigned long long size, unsigned long long *at,
                                CliCargoFailure *failure, CliLine *text)
{
    failure->message = text->at;
    failure->message_size = 0ull;
    failure->placed = 0;
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    unsigned long long peek = *at;
    while (report_line(out, size, &peek, &start, &length))
    {
        if (report_starts(out, start, length, "---- ") ||
            ((length == 9ull) &&
             (memor.cmp(&(MemoriaCfg){.src = out + start, .other = "failures:", .bytes = 9u}) == 0)))
        {
            break;
        }
        *at = peek;
        unsigned long long after = start;
        while ((after + 12ull <= start + length) &&
               (memor.cmp(&(MemoriaCfg){.src = out + after, .other = "panicked at ", .bytes = 12u}) != 0))
        {
            after += 1ull;
        }
        if (after + 12ull > start + length)
        {
            continue;
        }
        after += 12ull;
        const unsigned long long rest = start + length - after;
        text->at = failure->message;
        if ((rest > 0ull) && (out[after] == '\''))
        {
            // panicked at 'message', src/lib.rs:4:5
            unsigned long long split = rest;
            while ((split >= 3ull) &&
                   (memor.cmp(&(MemoriaCfg){.src = out + after + split - 3ull, .other = "', ", .bytes = 3u}) != 0))
            {
                split -= 1ull;
            }
            if (split >= 4ull)
            {
                cli_line_counted(text, out + after + 1ull, split - 4ull);
                failure->message_size = split - 4ull;
                failure->placed = cli_cargo_place_of(out + after + split, rest - split, failure->path, &failure->line);
            }
            continue;
        }
        // panicked at src/lib.rs:4:5:, the message on the lines after it
        unsigned long long place_size = rest;
        while ((place_size > 0ull) && (out[after + place_size - 1ull] == ':'))
        {
            place_size -= 1ull;
        }
        failure->placed = cli_cargo_place_of(out + after, place_size, failure->path, &failure->line);
        failure->message_size = 0ull;
        while (report_line(out, size, &peek, &start, &length))
        {
            unsigned long long trimmed = start;
            unsigned long long trimmed_size = length;
            report_trimmed(out, &trimmed, &trimmed_size);
            if ((trimmed_size == 0ull) || report_starts(out, start, length, "note:") ||
                report_starts(out, start, length, "---- "))
            {
                break;
            }
            *at = peek;
            cli_line_text(text, (failure->message_size > 0ull) ? "\n" : "");
            cli_line_counted(text, out + start, length);
            failure->message_size = text->at - failure->message;
        }
        peek = *at;
    }
}

void cli_cargo_failures_each(const char *out, unsigned long long size,
                             void (*each)(void *context, const char *name, unsigned long long name_size,
                                          const char *message, unsigned long long message_size, const char *path,
                                          long long line),
                             void *context)
{
    cli_lock_take(&s_failure_lock);
    CliLine text = {s_failure_text, sizeof(s_failure_text), 0ull};
    unsigned int count = 0u;
    unsigned long long at = 0ull;
    unsigned long long start = 0ull;
    unsigned long long length = 0ull;
    while (report_line(out, size, &at, &start, &length) && (count < CLI_CARGO_FAILURES))
    {
        static const char s_tail[] = " stdout ----";
        const unsigned long long tail = sizeof(s_tail) - 1ull;
        if (!report_starts(out, start, length, "---- ") || (length < 5ull + tail) ||
            (memor.cmp(&(MemoriaCfg){.src = out + start + length - tail, .other = s_tail, .bytes = tail}) != 0))
        {
            continue;
        }
        CliCargoFailure *const failure = &s_failures[count];
        failure->name = start + 5ull;
        failure->name_size = length - 5ull - tail;
        report_failure_read(out, size, &at, failure, &text);
        count += 1u;
    }
    for (unsigned int one = 0u; one < count; one += 1u)
    {
        const CliCargoFailure *const failure = &s_failures[one];
        int later = 0;
        for (unsigned int other = one + 1u; !later && (other < count); other += 1u)
        {
            later = (s_failures[other].name_size == failure->name_size) &&
                    (memor.cmp(&(MemoriaCfg){.src = out + s_failures[other].name,
                                             .other = out + failure->name,
                                             .bytes = failure->name_size}) == 0);
        }
        if (!later)
        {
            each(context, out + failure->name, failure->name_size, s_failure_text + failure->message,
                 failure->message_size, failure->placed ? failure->path : NULL, failure->line);
        }
    }
    cli_lock_give(&s_failure_lock);
}

int cli_cargo_build_error(const char *said, unsigned long long size, char *error, unsigned long long room,
                          unsigned long long *length, char *path, long long *line)
{
    unsigned long long at = 0ull;
    unsigned long long start = 0ull;
    unsigned long long size_of = 0ull;
    int found = 0;
    while (!found && report_line(said, size, &at, &start, &size_of))
    {
        found = report_starts(said, start, size_of, "error");
    }
    if (!found)
    {
        return 0;
    }
    CliLine out = {error, room, 0ull};
    cli_line_counted(&out, said + start, size_of);
    *length = cli_line_fits(&out) ? out.at : room;
    path[0] = '\0';
    for (unsigned int taken = 0u; (taken < 4u) && report_line(said, size, &at, &start, &size_of); taken += 1u)
    {
        while ((size_of > 0ull) &&
               (cellul.chr(&(CatenaFinitaCfg){.src = " \t", .cap = CLI_CELLS_POOL, .byte = said[start]}) != NULL))
        {
            start += 1ull;
            size_of -= 1ull;
        }
        if (report_starts(said, start, size_of, "--> "))
        {
            if (!cli_cargo_place_of(said + start + 4ull, size_of - 4ull, path, line))
            {
                path[0] = '\0';
            }
            break;
        }
    }
    return 1;
}

void cli_cargo_last_line(const char *text, unsigned long long size, const char *none, char *out,
                         unsigned long long room, unsigned long long *length)
{
    unsigned long long at = 0ull;
    unsigned long long start = 0ull;
    unsigned long long size_of = 0ull;
    unsigned long long last = 0ull;
    unsigned long long last_size = 0ull;
    int found = 0;
    while (report_line(text, size, &at, &start, &size_of))
    {
        report_trimmed(text, &start, &size_of);
        if (size_of > 0ull)
        {
            last = start;
            last_size = size_of;
            found = 1;
        }
    }
    CliLine line = {out, room, 0ull};
    cli_line_counted(&line, found ? text + last : none,
                     found ? last_size : cellul.len(&(CatenaFinitaCfg){.src = none, .cap = CLI_CELLS_POOL}));
    *length = line.at;
}

void cli_cargo_workspace_of(const char *root, const char *package, char *workspace)
{
    char folder[CLI_PATH_LONGEST];
    char above[CLI_PATH_LONGEST];
    memor.cpy(&(MemoriaCfg){.dst = workspace,
                            .src = package,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = package, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = folder,
                            .src = package,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = package, .cap = CLI_CELLS_POOL}) + 1u});
    const size_t root_size = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL});
    for (;;)
    {
        const int inside = cli_text_same(folder, root, root_size) &&
                           ((folder[root_size] == '\0') || (folder[root_size] == '/') || (folder[root_size] == '\\') ||
                            (root_size == 0u) || (root[root_size - 1u] == '/') || (root[root_size - 1u] == '\\'));
        if (!inside)
        {
            break;
        }
        char manifest[CLI_PATH_LONGEST];
        cli_full(folder, "Cargo.toml", manifest);
        unsigned long long size = 0ull;
        const unsigned char *const bytes = qry_member_file_mapped(manifest, &size);
        unsigned long long at = 0ull;
        unsigned long long start = 0ull;
        unsigned long long length = 0ull;
        int workspace_table = 0;
        while ((bytes != NULL) && !workspace_table && report_line((const char *)bytes, size, &at, &start, &length))
        {
            report_trimmed((const char *)bytes, &start, &length);
            workspace_table =
                (length == 11ull) &&
                (memor.cmp(&(MemoriaCfg){.src = bytes + start, .other = "[workspace]", .bytes = 11u}) == 0);
        }
        if (bytes != NULL)
        {
            qry_member_file_unmapped(bytes, size);
        }
        if (workspace_table)
        {
            memor.cpy(
                &(MemoriaCfg){.dst = workspace,
                              .src = folder,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}) + 1u});
        }
        if (!cli_path_parent(folder, above))
        {
            break;
        }
        memor.cpy(&(MemoriaCfg){.dst = folder, .src = above, .bytes = sizeof(folder)});
    }
}
