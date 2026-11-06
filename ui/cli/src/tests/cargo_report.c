// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cargo_report.c: what cargo test and the test harness write, read: results, failures, the places they name, a build's
// first error, and the folder a package's paths are written from
#include "cargo_report.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"

#include "../text/text.h"

// One failure the report names: its name's start and size in the report, its message's in the failures' texts, and
// its place
typedef struct
{
    unsigned long long name;
    unsigned long long name_size;
    unsigned long long message;
    unsigned long long message_size;
    char path[CLI_PATH_MAX_LEN];
    int positioned;
    long long line;
} CliCargoFailure;
// the most failures one report names
#define CLI_CARGO_FAILURES 4096u

typedef struct
{
    const char *text;
    unsigned long long size;
    unsigned long long *at;
    unsigned long long *start;
    unsigned long long *length;
    unsigned long long start_value;
    unsigned long long length_value;
    const char *head;
    const char *out;
    CliCargoFailure *failure;
    VerbaLinea *text_verba_linea_pointer;
} CargoReportCtx;
// the next line of `size` bytes from `at`, a "\r" before its "\n" taken off, as its start and length: 1, or 0 where
// none is left
EMBED_INLINE int report_line(const CargoReportCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned long long *at = args->at;
    unsigned long long *start = args->start;
    unsigned long long *length = args->length;
    if (*at >= size)
    {
        return 0;
    }
    *start = *at;
    const CliTextLine bounds = EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = text, .at = *at, .size = size);
    unsigned long long end = bounds.stop;
    *at = bounds.next;
    end -= bounds.stop - bounds.cut;
    *length = end - *start;
    return 1;
}

// `length` bytes from `start` trimmed of ASCII whitespace, as their start and length
EMBED_INLINE void report_trim(const CargoReportCtx *args)
{
    unsigned long long *start = args->start;
    unsigned long long *length = args->length;
    while ((*length > 0ULL) && (cellul.chr(&(CatenaFinitaCfg){
                                    .src = " \t\r\n\f\v", .cap = CLI_POOL_BYTES, .byte = args->text[*start]}) != NULL))
    {
        *start += 1ULL;
        *length -= 1ULL;
    }
    while ((*length > 0ULL) &&
           (cellul.chr(&(CatenaFinitaCfg){
                .src = " \t\r\n\f\v", .cap = CLI_POOL_BYTES, .byte = args->text[*start + *length - 1ULL]}) != NULL))
    {
        *length -= 1ULL;
    }
}

// whether `length` bytes from `start` begin with `head`
EMBED_INLINE int report_starts(const CargoReportCtx *args)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->head, .cap = CLI_POOL_BYTES});
    return (args->length_value >= size) &&
           (memor.cmp(&(MemoriaCfg){.src = args->text + args->start_value, .other = args->head, .bytes = size}) == 0);
}

int cli_cargo_result_line(const CliCargoReportLineCfg *args)
{
    unsigned long long *name = args->name;
    unsigned long long *name_size = args->name_size;
    const char **outcome = args->outcome;
    if (!EMBED_CALL(report_starts, CargoReportCtx, .text = args->line_text, .start_value = 0ULL,
                    .length_value = args->size, .head = "test "))
    {
        return 0;
    }
    unsigned long long split = 5ULL;
    while ((split + 5ULL <= args->size) &&
           (memor.cmp(&(MemoriaCfg){.src = args->line_text + split, .other = " ... ", .bytes = 5u}) != 0))
    {
        split += 1ULL;
    }
    if (split + 5ULL > args->size)
    {
        return 0;
    }
    unsigned long long start = 5ULL;
    unsigned long long length = split - 5ULL;
    EMBED_CALL(report_trim, CargoReportCtx, .text = args->line_text, .start = &start, .length = &length);
    static const char s_panic[] = " - should panic";
    while ((length >= sizeof(s_panic) - 1ULL) &&
           (memor.cmp(&(MemoriaCfg){.src = args->line_text + start + length - (sizeof(s_panic) - 1ULL),
                                    .other = s_panic,
                                    .bytes = sizeof(s_panic) - 1ULL}) == 0))
    {
        length -= sizeof(s_panic) - 1ULL;
    }
    unsigned long long message_at = split + 5ULL;
    unsigned long long message_size = args->size - message_at;
    EMBED_CALL(report_trim, CargoReportCtx, .text = args->line_text, .start = &message_at, .length = &message_size);
    if ((message_size == 2ULL) &&
        (memor.cmp(&(MemoriaCfg){.src = args->line_text + message_at, .other = "ok", .bytes = 2u}) == 0))
    {
        *outcome = "passed";
    }
    else if ((message_size == 6ULL) &&
             (memor.cmp(&(MemoriaCfg){.src = args->line_text + message_at, .other = "FAILED", .bytes = 6u}) == 0))
    {
        *outcome = "failed";
    }
    else if (EMBED_CALL(report_starts, CargoReportCtx, .text = args->line_text, .start_value = message_at,
                        .length_value = message_size, .head = "ignored"))
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

int cli_cargo_position_get(const CliCargoReportSizeTextPathCfg *args)
{
    long long *line = args->line;
    unsigned long long start = 0ULL;
    unsigned long long length = args->size;
    EMBED_CALL(report_trim, CargoReportCtx, .text = args->text, .start = &start, .length = &length);
    // the column after the last colon, the line before it, and the path before that
    const char *const base = args->text + start;
    unsigned long long column = length;
    column =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = base, .at = column, .set = ":");
    if (column == 0ULL)
    {
        return 0;
    }
    unsigned long long number = column - 1ULL;
    number =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = base, .at = number, .set = ":");
    if (number == 0ULL)
    {
        return 0;
    }
    unsigned long long value = 0ULL;
    const unsigned long long digits_end = column - 1ULL;
    const unsigned long long digits_start = number + ((base[number] == '+') ? 1ULL : 0ULL);
    if (digits_start >= digits_end)
    {
        return 0;
    }
    for (unsigned long long at = digits_start; at < digits_end; at += 1ULL)
    {
        if ((base[at] < '0') || (base[at] > '9') || (value > 429496729ULL))
        {
            return 0;
        }
        value = (value * 10ULL) + (unsigned long long)(base[at] - '0');
    }
    if (value > 4294967295ULL)
    {
        return 0;
    }
    VerbaLinea out = {args->path, CLI_PATH_MAX_LEN, 0ULL};
    for (unsigned long long at = 0ULL; at + 1ULL < number; at += 1ULL)
    {
        mmgr_line_ch(&out, (base[at] == '\\') ? '/' : base[at]);
    }
    mmgr_line_finish(&out);
    *line = (long long)value;
    return 1;
}

// whether the line `start`, `length` bytes of `out`, opens a failure's output: `---- name stdout ----`
EMBED_INLINE int report_failure_by_name(const CargoReportCtx *args)
{
    static const char s_tail[] = " stdout ----";
    const unsigned long long tail = sizeof(s_tail) - 1ULL;
    return EMBED_CALL(report_starts, CargoReportCtx, .text = args->out, .start_value = args->start_value,
                      .length_value = args->length_value, .head = "---- ") &&
           (args->length_value >= 5ULL + tail) &&
           (memor.cmp(&(MemoriaCfg){.src = args->out + args->start_value + args->length_value - tail,
                                    .other = s_tail,
                                    .bytes = tail}) == 0);
}

// the failure the lines after a test's `---- name stdout ----` give: its message and place, the lines read past
EMBED_INLINE void report_failure_read(const CargoReportCtx *args)
{
    unsigned long long size = args->size;
    unsigned long long *at = args->at;
    VerbaLinea *text = args->text_verba_linea_pointer;
    args->failure->message = text->at;
    args->failure->message_size = 0ULL;
    args->failure->positioned = 0;
    unsigned long long start = 0ULL;
    unsigned long long length = 0ULL;
    unsigned long long peek = *at;
    while (EMBED_CALL(report_line, CargoReportCtx, .text = args->out, .size = size, .at = &peek, .start = &start,
                      .length = &length))
    {
        if (EMBED_CALL(report_starts, CargoReportCtx, .text = args->out, .start_value = start, .length_value = length,
                       .head = "---- ") ||
            ((length == 9ULL) &&
             (memor.cmp(&(MemoriaCfg){.src = args->out + start, .other = "failures:", .bytes = 9u}) == 0)))
        {
            break;
        }
        *at = peek;
        unsigned long long after = start;
        while ((after + 12ULL <= start + length) &&
               (memor.cmp(&(MemoriaCfg){.src = args->out + after, .other = "panicked at ", .bytes = 12u}) != 0))
        {
            after += 1ULL;
        }
        if (after + 12ULL > start + length)
        {
            continue;
        }
        after += 12ULL;
        const unsigned long long rest = start + length - after;
        text->at = args->failure->message;
        if ((rest > 0ULL) && (args->out[after] == '\''))
        {
            // panicked at 'message', src/lib.rs:4:5
            unsigned long long split = rest;
            while ((split >= 3ULL) && (memor.cmp(&(MemoriaCfg){
                                           .src = args->out + after + split - 3ULL, .other = "', ", .bytes = 3u}) != 0))
            {
                split -= 1ULL;
            }
            if (split >= 4ULL)
            {
                mmgr_line_put_n(text, args->out + after + 1ULL, split - 4ULL);
                args->failure->message_size = split - 4ULL;
                args->failure->positioned =
                    EMBED_CALL(cli_cargo_report_size_text_path.cargo_position_get, CliCargoReportSizeTextPathCfg,
                               .text = args->out + after + split, .size = rest - split, .path = args->failure->path,
                               .line = &args->failure->line);
            }
            continue;
        }
        // panicked at src/lib.rs:4:5:, the message on the lines after it
        unsigned long long position_size = rest;
        position_size = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg,
                                   .text = args->out + after, .at = position_size, .set = ":");
        args->failure->positioned =
            EMBED_CALL(cli_cargo_report_size_text_path.cargo_position_get, CliCargoReportSizeTextPathCfg,
                       .text = args->out + after, .size = position_size, .path = args->failure->path,
                       .line = &args->failure->line);
        args->failure->message_size = 0ULL;
        while (EMBED_CALL(report_line, CargoReportCtx, .text = args->out, .size = size, .at = &peek, .start = &start,
                          .length = &length))
        {
            unsigned long long trimmed = start;
            unsigned long long trimmed_size = length;
            EMBED_CALL(report_trim, CargoReportCtx, .text = args->out, .start = &trimmed, .length = &trimmed_size);
            if ((trimmed_size == 0ULL) ||
                EMBED_CALL(report_starts, CargoReportCtx, .text = args->out, .start_value = start,
                           .length_value = length, .head = "note:") ||
                EMBED_CALL(report_starts, CargoReportCtx, .text = args->out, .start_value = start,
                           .length_value = length, .head = "---- "))
            {
                break;
            }
            *at = peek;
            mmgr_line_put(text, (args->failure->message_size > 0ULL) ? "\n" : "");
            mmgr_line_put_n(text, args->out + start, length);
            args->failure->message_size = text->at - args->failure->message;
        }
        peek = *at;
    }
}

void cli_cargo_failures_each(const CliCargoReportSizeOutCfg *args)
{
    const char *out = args->out;
    unsigned long long size = args->size;
    // the failures in a cell of the count the report names, and their messages, each a part of the report, in a cell
    // of its size
    unsigned int named = 0u;
    unsigned long long at = 0ULL;
    unsigned long long start = 0ULL;
    unsigned long long length = 0ULL;
    while (EMBED_CALL(report_line, CargoReportCtx, .text = out, .size = size, .at = &at, .start = &start,
                      .length = &length) &&
           (named < CLI_CARGO_FAILURES))
    {
        named +=
            EMBED_CALL(report_failure_by_name, CargoReportCtx, .out = out, .start_value = start, .length_value = length)
                ? 1u
                : 0u;
    }
    CliCargoFailure *const failures = g_cli_guard->temporary_buf_alloc(
        ((unsigned long long)named + 1ULL) * sizeof(CliCargoFailure), g_cli_tests_owner);
    char *const texts = g_cli_guard->temporary_buf_alloc(size + 1ULL, g_cli_tests_owner);
    VerbaLinea text = mmgr_line_over(texts, size + 1ULL);
    unsigned int count = 0u;
    at = 0ULL;
    while ((failures != NULL) && (texts != NULL) &&
           EMBED_CALL(report_line, CargoReportCtx, .text = out, .size = size, .at = &at, .start = &start,
                      .length = &length) &&
           (count < named))
    {
        if (!EMBED_CALL(report_failure_by_name, CargoReportCtx, .out = out, .start_value = start,
                        .length_value = length))
        {
            continue;
        }
        static const char s_tail[] = " stdout ----";
        const unsigned long long tail = sizeof(s_tail) - 1ULL;
        CliCargoFailure *const failure = &failures[count];
        failure->name = start + 5ULL;
        failure->name_size = length - 5ULL - tail;
        EMBED_CALL(report_failure_read, CargoReportCtx, .out = out, .size = size, .at = &at, .failure = failure,
                   .text_verba_linea_pointer = &text);
        count += 1u;
    }
    for (unsigned int one = 0u; one < count; one += 1u)
    {
        const CliCargoFailure *const failure = &failures[one];
        int later = 0;
        unsigned int other = one + 1u;
        while (!later && (other < count))
        {
            later = (failures[other].name_size == failure->name_size) &&
                    (memor.cmp(&(MemoriaCfg){.src = out + failures[other].name,
                                             .other = out + failure->name,
                                             .bytes = failure->name_size}) == 0);
            other += 1u;
        }
        if (!later)
        {
            EMBED_CALL(args->each, CliEachNameMessagePathLineCfg, .context = args->context, .name = out + failure->name,
                       .name_size = failure->name_size, .message = texts + failure->message,
                       .message_size = failure->message_size, .path = failure->positioned ? failure->path : NULL,
                       .line = failure->line);
        }
    }
    g_cli_guard->persistent_buf_release(texts, g_cli_tests_owner);
    g_cli_guard->persistent_buf_release(failures, g_cli_tests_owner);
}

int cli_cargo_build_error(const CliCargoReportSizeCfg *args)
{
    unsigned long long *length = args->length;
    unsigned long long at = 0ULL;
    unsigned long long start = 0ULL;
    unsigned long long size_of = 0ULL;
    int found = 0;
    while (!found && EMBED_CALL(report_line, CargoReportCtx, .text = args->output, .size = args->size, .at = &at,
                                .start = &start, .length = &size_of))
    {
        found = EMBED_CALL(report_starts, CargoReportCtx, .text = args->output, .start_value = start,
                           .length_value = size_of, .head = "error");
    }
    if (!found)
    {
        return 0;
    }
    VerbaLinea out = {args->error, args->capacity, 0ULL};
    mmgr_line_put_n(&out, args->output + start, size_of);
    *length = mmgr_line_ok(&out) ? out.at : args->capacity;
    args->path[0] = '\0';
    for (unsigned int reserved = 0u;
         (reserved < 4u) && EMBED_CALL(report_line, CargoReportCtx, .text = args->output, .size = args->size, .at = &at,
                                       .start = &start, .length = &size_of);
         reserved += 1u)
    {
        while ((size_of > 0ULL) && (cellul.chr(&(CatenaFinitaCfg){
                                        .src = " \t", .cap = CLI_POOL_BYTES, .byte = args->output[start]}) != NULL))
        {
            start += 1ULL;
            size_of -= 1ULL;
        }
        if (EMBED_CALL(report_starts, CargoReportCtx, .text = args->output, .start_value = start,
                       .length_value = size_of, .head = "--> "))
        {
            if (!EMBED_CALL(cli_cargo_report_size_text_path.cargo_position_get, CliCargoReportSizeTextPathCfg,
                            .text = args->output + start + 4ULL, .size = size_of - 4ULL, .path = args->path,
                            .line = args->line))
            {
                args->path[0] = '\0';
            }
            break;
        }
    }
    return 1;
}

void cli_cargo_last_line(const CliCargoReportSizeTextCfg *args)
{
    unsigned long long *length = args->length;
    unsigned long long at = 0ULL;
    unsigned long long start = 0ULL;
    unsigned long long size_of = 0ULL;
    unsigned long long last = 0ULL;
    unsigned long long last_size = 0ULL;
    int found = 0;
    while (EMBED_CALL(report_line, CargoReportCtx, .text = args->text, .size = args->size, .at = &at, .start = &start,
                      .length = &size_of))
    {
        EMBED_CALL(report_trim, CargoReportCtx, .text = args->text, .start = &start, .length = &size_of);
        if (size_of > 0ULL)
        {
            last = start;
            last_size = size_of;
            found = 1;
        }
    }
    VerbaLinea line = {args->out_out, args->capacity, 0ULL};
    mmgr_line_put_n(&line, found ? args->text + last : args->none,
                    found ? last_size : cellul.len(&(CatenaFinitaCfg){.src = args->none, .cap = CLI_POOL_BYTES}));
    *length = line.at;
}

void cli_cargo_workspace_get(const CliCargoReportCfg *args)
{
    char folder[CLI_PATH_MAX_LEN];
    char above[CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = args->workspace,
                            .src = args->package,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->package, .cap = CLI_POOL_BYTES}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = folder,
                            .src = args->package,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->package, .cap = CLI_POOL_BYTES}) + 1u});
    const size_t root_size = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_POOL_BYTES});
    int going = 1;
    while (going)
    {
        const int inside =
            EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = folder, .other_pointer = args->root,
                       .size = root_size) &&
            ((folder[root_size] == '\0') || (folder[root_size] == '/') || (folder[root_size] == '\\') ||
             (root_size == 0u) || (args->root[root_size - 1u] == '/') || (args->root[root_size - 1u] == '\\'));
        if (!inside)
        {
            going = 0;
            continue;
        }
        char manifest[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = "Cargo.toml", .joined = manifest);
        unsigned long long size = 0ULL;
        const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = manifest,
                                                      .size_pointer = &size, .owner = g_cli_tests_owner);
        unsigned long long at = 0ULL;
        unsigned long long start = 0ULL;
        unsigned long long length = 0ULL;
        int workspace_table = 0;
        while ((bytes != NULL) && !workspace_table &&
               EMBED_CALL(report_line, CargoReportCtx, .text = (const char *)bytes, .size = size, .at = &at,
                          .start = &start, .length = &length))
        {
            EMBED_CALL(report_trim, CargoReportCtx, .text = (const char *)bytes, .start = &start, .length = &length);
            workspace_table =
                (length == 11ULL) &&
                (memor.cmp(&(MemoriaCfg){.src = bytes + start, .other = "[workspace]", .bytes = 11u}) == 0);
        }
        g_cli_guard->persistent_buf_release((void *)(uintptr_t)bytes, (g_cli_tests_owner));
        if (workspace_table)
        {
            memor.cpy(
                &(MemoriaCfg){.dst = args->workspace,
                              .src = folder,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_POOL_BYTES}) + 1u});
        }
        if (!EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = folder, .out = above))
        {
            going = 0;
            continue;
        }
        memor.cpy(&(MemoriaCfg){.dst = folder, .src = above, .bytes = sizeof(folder)});
    }
}
