// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// view_tidy.c: a view's text made ready for its file, and a formatter's answer written back over the lines that differ.
// Every change is one step of the view's, which one undo takes back whole
#include "view_tidy.h"

#include "../command_line/pool.h"
#include "../text/text.h"
#include "../toolchains/format.h"
#include "document.h"
#include "view_edits.h"

// how much room the formatted text gets past twice the text it comes from
#define CLI_VIEW_FORMAT_ROOM 65536ULL

typedef struct
{
    const char *key;
    const char *text;
    char *out;
    unsigned long long size;
    unsigned long long line;
    unsigned long long at;
} ViewTidyCtx;

// the units of the line `line` of the text under `key`, 0 where there is none
EMBED_INLINE unsigned long long tidy_units(const ViewTidyCtx *args)
{
    unsigned long long count = 0ULL;
    (void)EMBED_CALL(cli_document_line.line_units, CliDocumentLineUnitsCfg, .key = args->key, .line = args->line,
                     .count = &count, .dst = NULL, .capacity = 0ULL);
    return count;
}

// the place past the line that starts at `at` of `text`, `size` bytes: the newline's place, or `size` on the last line
EMBED_INLINE unsigned long long tidy_line_end(const ViewTidyCtx *args)
{
    return EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->text, .at = args->at,
                      .size = args->size, .set = "\n");
}

// the lines `text`, `size` bytes, holds: one more than its newlines
EMBED_INLINE unsigned long long tidy_lines(const ViewTidyCtx *args)
{
    unsigned long long lines = 1ULL;
    unsigned long long at = 0ULL;
    while (at < args->size)
    {
        const unsigned long long end =
            EMBED_CALL(tidy_line_end, ViewTidyCtx, .text = args->text, .size = args->size, .at = at);
        if (end >= args->size)
        {
            break;
        }
        lines += 1ULL;
        at = end + 1ULL;
    }
    return lines;
}

// the place the line `line` of `text`, `size` bytes, starts at, a line the text holds
EMBED_INLINE unsigned long long tidy_line_start(const ViewTidyCtx *args)
{
    unsigned long long at = 0ULL;
    for (unsigned long long line = 0ULL; line < args->line; line += 1ULL)
    {
        at = EMBED_CALL(tidy_line_end, ViewTidyCtx, .text = args->text, .size = args->size, .at = at) + 1ULL;
    }
    return at;
}

// every carriage return before a newline in `out`, `size` bytes, taken out in place: the bytes left
EMBED_INLINE unsigned long long tidy_unix_ends(const ViewTidyCtx *args)
{
    char *const out = args->out;
    unsigned long long read = 0ULL;
    unsigned long long written = 0ULL;
    while (read < args->size)
    {
        const unsigned long long next = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = out,
                                                   .at = read, .size = args->size, .set = "\r");
        if (written != read)
        {
            memor.move_down(&(MemoriaCfg){.dst = out + written, .src = out + read, .bytes = next - read});
        }
        written += next - read;
        read = next;
        if (read >= args->size)
        {
            break;
        }
        // a carriage return before a newline goes; one on its own stays
        if ((read + 1ULL < args->size) && (out[read + 1ULL] == '\n'))
        {
            read += 1ULL;
        }
        else
        {
            out[written] = '\r';
            written += 1ULL;
            read += 1ULL;
        }
    }
    return written;
}

int cli_view_tidy(const CliViewTidyCfg *args)
{
    unsigned long long size = 0ULL;
    char *const text = EMBED_CALL(cli_document_key.text_alloc, CliDocumentKeyCfg, .key = args->key,
                                  .owner = g_cli_view_owner, .size_pointer = &size);
    if (text == NULL)
    {
        // a paged text, or none: nothing to tidy
        return 1;
    }
    const unsigned long long lines = EMBED_CALL(tidy_lines, ViewTidyCtx, .text = text, .size = size);
    CliEdit *const edits = g_cli_guard->temporary_buf_alloc((lines + 1ULL) * sizeof(CliEdit), g_cli_view_owner);
    if (edits == NULL)
    {
        g_cli_guard->persistent_buf_release(text, g_cli_view_owner);
        mmgr_line_put(args->err_msg, "the cells cannot meet the edits that tidy the text");
        return 0;
    }
    unsigned int count = 0u;
    unsigned long long at = 0ULL;
    for (unsigned long long line = 0ULL; line < lines; line += 1ULL)
    {
        const unsigned long long end = EMBED_CALL(tidy_line_end, ViewTidyCtx, .text = text, .size = size, .at = at);
        const unsigned long long units = EMBED_CALL(tidy_units, ViewTidyCtx, .key = args->key, .line = line);
        // the blanks at the line's end are ASCII, each one unit
        const unsigned long long kept = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg,
                                                   .text = text + at, .at = end - at, .set = " \t");
        const unsigned long long blanks = end - at - kept;
        if (args->trim && (blanks != 0ULL) && (units >= blanks))
        {
            edits[count] = (CliEdit){{line, units - blanks}, {line, units}, "", 0ULL};
            count += 1u;
        }
        if ((end >= size) && args->final_newline && (size != 0ULL) && (text[size - 1ULL] != '\n'))
        {
            edits[count] = (CliEdit){{line, units}, {line, units}, "\n", 1ULL};
            count += 1u;
        }
        at = end + 1ULL;
    }
    g_cli_guard->persistent_buf_release(text, g_cli_view_owner);
    const int applied = (count == 0u) || EMBED_CALL(cli_view_edits.apply, CliViewEditsCfg, .key = args->key,
                                                   .edit = edits, .count = count, .err_msg = args->err_msg);
    g_cli_guard->persistent_buf_release(edits, g_cli_view_owner);
    return applied;
}

int cli_view_replace(const CliViewReplaceCfg *args)
{
    unsigned long long size = 0ULL;
    char *const before = EMBED_CALL(cli_document_key.text_alloc, CliDocumentKeyCfg, .key = args->key,
                                    .owner = g_cli_view_owner, .size_pointer = &size);
    if (before == NULL)
    {
        mmgr_line_put(args->err_msg, "no text read whole is open under the key");
        return 0;
    }
    const char *const after = args->text;
    const unsigned long long after_size = args->length;
    // the lines alike at the texts' starts, each followed by a newline in both
    unsigned long long prefix = 0ULL;
    unsigned long long before_at = 0ULL;
    unsigned long long after_at = 0ULL;
    for (;;)
    {
        const unsigned long long before_end =
            EMBED_CALL(tidy_line_end, ViewTidyCtx, .text = before, .size = size, .at = before_at);
        const unsigned long long after_end =
            EMBED_CALL(tidy_line_end, ViewTidyCtx, .text = after, .size = after_size, .at = after_at);
        if ((before_end >= size) || (after_end >= after_size) || ((before_end - before_at) != (after_end - after_at)) ||
            (memor.cmp(&(MemoriaCfg){.src = before + before_at, .other = after + after_at, .bytes = before_end - before_at}) !=
             0))
        {
            break;
        }
        prefix += 1ULL;
        before_at = before_end + 1ULL;
        after_at = after_end + 1ULL;
    }
    const unsigned long long before_lines = EMBED_CALL(tidy_lines, ViewTidyCtx, .text = before, .size = size);
    const unsigned long long after_lines = EMBED_CALL(tidy_lines, ViewTidyCtx, .text = after, .size = after_size);
    // the lines alike at the texts' ends, each whole, none of the prefix among them
    unsigned long long suffix = 0ULL;
    unsigned long long before_stop = size;
    unsigned long long after_stop = after_size;
    while ((prefix + suffix < before_lines) && (prefix + suffix < after_lines))
    {
        const unsigned long long before_start = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg,
                                                           .text = before, .at = before_stop, .set = "\n");
        const unsigned long long after_start = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg,
                                                          .text = after, .at = after_stop, .set = "\n");
        if (((before_stop - before_start) != (after_stop - after_start)) ||
            (memor.cmp(&(MemoriaCfg){
                 .src = before + before_start, .other = after + after_start, .bytes = before_stop - before_start}) != 0))
        {
            break;
        }
        suffix += 1ULL;
        if ((before_start == 0ULL) || (after_start == 0ULL))
        {
            break;
        }
        before_stop = before_start - 1ULL;
        after_stop = after_start - 1ULL;
    }
    if ((prefix + suffix == before_lines) && (prefix + suffix == after_lines))
    {
        g_cli_guard->persistent_buf_release(before, g_cli_view_owner);
        return 1;
    }
    // the lines of the text from `prefix` to before `before_lines - suffix` replaced by the other's from `prefix` to
    // before `after_lines - suffix`: whole lines with their newlines where lines follow the run, else to the text's end
    const unsigned long long after_from =
        EMBED_CALL(tidy_line_start, ViewTidyCtx, .text = after, .size = after_size, .line = prefix);
    CliEdit edit;
    if (suffix != 0ULL)
    {
        const unsigned long long after_to = EMBED_CALL(tidy_line_start, ViewTidyCtx, .text = after, .size = after_size,
                                                       .line = after_lines - suffix);
        edit = (CliEdit){{prefix, 0ULL}, {before_lines - suffix, 0ULL}, after + after_from, after_to - after_from};
    }
    else
    {
        const unsigned long long last_units =
            EMBED_CALL(tidy_units, ViewTidyCtx, .key = args->key, .line = before_lines - 1ULL);
        edit = (CliEdit){{prefix, 0ULL}, {before_lines - 1ULL, last_units}, after + after_from, after_size - after_from};
    }
    g_cli_guard->persistent_buf_release(before, g_cli_view_owner);
    return EMBED_CALL(cli_view_edits.apply, CliViewEditsCfg, .key = args->key, .edit = &edit, .count = 1u,
                      .err_msg = args->err_msg);
}

int cli_view_format(const CliViewFormatCfg *args)
{
    unsigned long long size = 0ULL;
    char *const text = EMBED_CALL(cli_document_key.text_alloc, CliDocumentKeyCfg, .key = args->key,
                                  .owner = g_cli_view_owner, .size_pointer = &size);
    if (text == NULL)
    {
        mmgr_line_put(args->err_msg, "no text read whole is open under the key");
        return 0;
    }
    const unsigned long long capacity = (size * 2ULL) + CLI_VIEW_FORMAT_ROOM;
    char *const formatted = g_cli_guard->temporary_buf_alloc(capacity, g_cli_view_owner);
    if (formatted == NULL)
    {
        g_cli_guard->persistent_buf_release(text, g_cli_view_owner);
        mmgr_line_put(args->err_msg, "the cells cannot meet the formatted text");
        return 0;
    }
    VerbaLinea out = {formatted, capacity, 0ULL};
    const int ran = EMBED_CALL(cli_format_path.format, CliFormatPathCfg, .path = args->full, .language = args->language,
                               .text = text, .length = size, .out = &out, .raw = 1, .err_msg = args->err_msg);
    g_cli_guard->persistent_buf_release(text, g_cli_view_owner);
    int replaced = 0;
    if (ran && !mmgr_line_ok(&out))
    {
        mmgr_line_put(args->err_msg, "the formatted text does not fit the cells taken for it");
    }
    else if (ran)
    {
        const unsigned long long length = EMBED_CALL(tidy_unix_ends, ViewTidyCtx, .out = formatted, .size = out.at);
        replaced = EMBED_CALL(cli_view_replace_table.replace, CliViewReplaceCfg, .key = args->key, .text = formatted,
                              .length = length, .err_msg = args->err_msg);
    }
    g_cli_guard->persistent_buf_release(formatted, g_cli_view_owner);
    return replaced;
}
