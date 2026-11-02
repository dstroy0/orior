// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// catalog_scan.c: a path joined and a path's stem, and what a file declares of itself: its opening paragraph, the names
// a case accepts, the parts a driver's usage names, and a README's first paragraph
#include "../command_line/cells.h"
#include "../text/text.h"
#include "catalog_internal.h"

// A line of a text: where it starts and its length, its `\r` before `\n` left off
typedef struct
{
    const char *at;
    unsigned long long length;
} CatalogTextLine;
void cli_catalog_join(const CliCatalogFolderNameCtx *args)
{
    VerbaLinea line = {args->path_out, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, args->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->name);
    mmgr_line_finish(&line);
}

void cli_catalog_stem(const CliCatalogPathOutCtx *args)
{
    const char *name = args->path + cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL});
    name = args->path + EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path,
                                   .at = (unsigned long long)(name - args->path), .set = "/\\");
    const char *const dot =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = name,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), .byte = '.');
    const unsigned long long length = ((dot != NULL) && (dot != name))
                                          ? (unsigned long long)(dot - name)
                                          : cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    VerbaLinea line = {args->out, CLI_NAME_MAX_LEN, 0ull};
    mmgr_line_put_n(&line, name, length);
    mmgr_line_finish(&line);
}

typedef struct
{
    const char **at;
    const char *end;
    CatalogTextLine *line_pointer;
    char letter;
    CatalogTextLine line;
    const char *word;
    VerbaLinea *out;
    int *count;
    const char *text;
    unsigned long long length;
    const char *open;
    unsigned long long size;
    const char *path;
    const char *from;
    char *joined;
} CatalogScanCtx;
// the line from `*at` before `end`, `*at` moved past it: 1, or 0 at the end
EMBED_INLINE int cli_line(const CatalogScanCtx *args)
{
    const char **at = args->at;
    if (*at >= args->end)
    {
        return 0;
    }
    const char *stop = *at;
    stop += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = stop, .at = 0ull,
                       .size = (unsigned long long)(args->end - stop), .set = "\n");
    args->line_pointer->at = *at;
    args->line_pointer->length = (unsigned long long)(stop - *at);
    args->line_pointer->length -=
        ((args->line_pointer->length != 0ull) && (args->line_pointer->at[args->line_pointer->length - 1u] == '\r'))
            ? 1ull
            : 0ull;
    *at = (stop < args->end) ? (stop + 1) : stop;
    return 1;
}

EMBED_INLINE int cli_space(const CatalogScanCtx *args)
{
    return (args->letter == ' ') || ((args->letter >= '\t') && (args->letter <= '\r'));
}

EMBED_INLINE CatalogTextLine cli_trim(const CatalogScanCtx *args)
{
    CatalogTextLine line = args->line;
    while ((line.length != 0ull) && EMBED_CALL(cli_space, CatalogScanCtx, .letter = line.at[0]))
    {
        line.at += 1;
        line.length -= 1ull;
    }
    while ((line.length != 0ull) && EMBED_CALL(cli_space, CatalogScanCtx, .letter = line.at[line.length - 1u]))
    {
        line.length -= 1ull;
    }
    return line;
}

EMBED_INLINE int cli_line_contains(const CatalogScanCtx *args)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->word, .cap = CLI_CELLS_POOL});
    for (unsigned long long at = 0ull; (at + size) <= args->line.length; at += 1ull)
    {
        if (memor.cmp(&(MemoriaCfg){.src = args->line.at + at, .other = args->word, .bytes = size}) == 0)
        {
            return 1;
        }
    }
    return 0;
}

EMBED_INLINE int cli_starts(const CatalogScanCtx *args)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->word, .cap = CLI_CELLS_POOL});
    return (args->line.length >= size) &&
           (memor.cmp(&(MemoriaCfg){.src = args->line.at, .other = args->word, .bytes = size}) == 0);
}

// a retained line put after the paragraph, a space before each but the first
EMBED_INLINE void cli_paragraph_append(const CatalogScanCtx *args)
{
    int *count = args->count;
    mmgr_line_put(args->out, (*count != 0) ? " " : "");
    mmgr_line_put_n(args->out, args->text, args->length);
    *count += 1;
}

// A Python module's docstring: from the line it opens on, each line trimmed of its quotes, up to its close or a blank
// line after text
EMBED_INLINE void cli_docstring(const CatalogScanCtx *args)
{
    int count = 0;
    int first = 1;
    CatalogTextLine line;
    const char *at = args->open;
    while (EMBED_CALL(cli_line, CatalogScanCtx, .at = &at, .end = args->end, .line_pointer = &line))
    {
        line = EMBED_CALL(cli_trim, CatalogScanCtx, .line = line);
        while (first && EMBED_CALL(cli_starts, CatalogScanCtx, .line = line, .word = "\"\"\""))
        {
            line.at += 3;
            line.length -= 3ull;
        }
        first = 0;
        const int ended = EMBED_CALL(cli_line_contains, CatalogScanCtx, .line = line, .word = "\"\"\"");
        while ((line.length >= 3ull) &&
               (memor.cmp(&(MemoriaCfg){.src = line.at + line.length - 3u, .other = "\"\"\"", .bytes = 3u}) == 0))
        {
            line.length -= 3ull;
        }
        line = EMBED_CALL(cli_trim, CatalogScanCtx, .line = line);
        if ((line.length == 0ull) && (count != 0))
        {
            return;
        }
        if (line.length != 0ull)
        {
            EMBED_CALL(cli_paragraph_append, CatalogScanCtx, .out = args->out, .count = &count, .text = line.at,
                       .length = line.length);
        }
        if (ended)
        {
            return;
        }
    }
}

// the end of the first 120 lines of the `size` bytes at `text`, the lines what a file declares of itself is read from
EMBED_INLINE const char *cli_about_end(const CatalogScanCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    const char *end = text;
    const char *const whole = text + size;
    for (unsigned int lines = 0u; (lines < 120u) && (end < whole); lines += 1u)
    {
        end += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = end, .at = 0ull,
                          .size = (unsigned long long)(whole - end), .set = "\n");
        end += (end < whole) ? 1 : 0;
    }
    return end;
}

// what the file at `path`, its text from `text` to `end`, declares of itself into `out`
static void cli_about_write(const CatalogScanCtx *args)
{
    const char *text = args->text;
    const char *const dot = EMBED_CALL(
        cli_text_byte.last, CliTextByteCfg, .text = args->path,
        .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}), .byte = '.');
    const char *const extension = (dot != NULL) ? (dot + 1) : "";
    CatalogTextLine line;
    const char *at = text;
    if (cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = "py", .cap = CLI_CELLS_POOL}))
    {
        while (EMBED_CALL(cli_line, CatalogScanCtx, .at = &at, .end = args->end, .line_pointer = &line))
        {
            if (EMBED_CALL(cli_starts, CatalogScanCtx, .line = EMBED_CALL(cli_trim, CatalogScanCtx, .line = line),
                           .word = "\"\"\""))
            {
                EMBED_CALL(cli_docstring, CatalogScanCtx, .open = line.at, .end = args->end, .out = args->out);
                return;
            }
        }
        at = text;
    }
    const char *const mark = ((cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = "sh", .cap = CLI_CELLS_POOL})) ||
                              (cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = "py", .cap = CLI_CELLS_POOL})) ||
                              (cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = "ps1", .cap = CLI_CELLS_POOL})))
                                 ? "#"
                                 : "//";
    int count = 0;
    while (EMBED_CALL(cli_line, CatalogScanCtx, .at = &at, .end = args->end, .line_pointer = &line))
    {
        line = EMBED_CALL(cli_trim, CatalogScanCtx, .line = line);
        const int license = EMBED_CALL(cli_line_contains, CatalogScanCtx, .line = line, .word = "SPDX") ||
                            EMBED_CALL(cli_line_contains, CatalogScanCtx, .line = line, .word = "Copyright") ||
                            EMBED_CALL(cli_starts, CatalogScanCtx, .line = line, .word = "#!");
        if ((line.length == 0ull) && (count == 0))
        {
            continue;
        }
        if (!EMBED_CALL(cli_starts, CatalogScanCtx, .line = line, .word = mark))
        {
            if ((count == 0) && license)
            {
                continue;
            }
            break;
        }
        if (license)
        {
            continue;
        }
        CatalogTextLine about_text = line;
        while (EMBED_CALL(cli_starts, CatalogScanCtx, .line = about_text, .word = mark))
        {
            about_text.at += cellul.len(&(CatenaFinitaCfg){.src = mark, .cap = CLI_CELLS_POOL});
            about_text.length -= cellul.len(&(CatenaFinitaCfg){.src = mark, .cap = CLI_CELLS_POOL});
        }
        {
            const unsigned long long lead =
                EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = about_text.at, .at = 0ull,
                           .size = about_text.length, .set = "!");
            about_text.at += lead;
            about_text.length -= lead;
        }
        about_text = EMBED_CALL(cli_trim, CatalogScanCtx, .line = about_text);
        if (about_text.length == 0ull)
        {
            if (count == 0)
            {
                continue;
            }
            break;
        }
        EMBED_CALL(cli_paragraph_append, CatalogScanCtx, .out = args->out, .count = &count, .text = about_text.at,
                   .length = about_text.length);
    }
}

void cli_catalog_about(const CliCatalogCfg *args)
{
    unsigned long long size = 0ull;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = args->path,
                                                  .size_pointer = &size, .owner = &g_cli_catalog_cells);
    if (bytes == NULL)
    {
        return;
    }
    // the file's bytes read as its text
    const char *const text = (const char *)bytes;
    EMBED_CALL(cli_about_write, CatalogScanCtx, .path = args->path, .text = text,
               .end = EMBED_CALL(cli_about_end, CatalogScanCtx, .text = text, .size = size), .out = args->out);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_catalog_cells);
}

// what a file declares of itself is the text of the lines it is read from, a space put for each line's end it passes,
// so it is no longer than those lines
char *cli_catalog_about_alloc(const CliCatalogPathCtx *args)
{
    unsigned long long size = 0ull;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = args->path,
                                                  .size_pointer = &size, .owner = &g_cli_catalog_cells);
    if (bytes == NULL)
    {
        return NULL;
    }
    // the file's bytes read as its text
    const char *const text = (const char *)bytes;
    const char *const end = EMBED_CALL(cli_about_end, CatalogScanCtx, .text = text, .size = size);
    const unsigned long long capacity = (unsigned long long)(end - text) + 1ull;
    char *const about = g_cli_cells->persistent_buf_alloc(capacity, &g_cli_catalog_cells);
    if (about != NULL)
    {
        VerbaLinea line = {about, capacity, 0ull};
        EMBED_CALL(cli_about_write, CatalogScanCtx, .path = args->path, .text = text, .end = end, .out = &line);
        mmgr_line_finish(&line);
    }
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_catalog_cells);
    return about;
}

unsigned int cli_catalog_case_names(const CliCatalogTextVariableCtx *args)
{
    const char *text = args->text_read;
    char opener[CLI_NAME_MAX_LEN];
    VerbaLinea line = {opener, sizeof(opener), 0ull};
    mmgr_line_put(&line, "case \"$");
    mmgr_line_put(&line, args->variable);
    mmgr_line_put(&line, "\" in");
    mmgr_line_finish(&line);
    const char *const found = cellul.find(
        &(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL, .other = opener, .other_cap = CLI_CELLS_POOL});
    if (found == NULL)
    {
        return 0u;
    }
    const char *at = found + cellul.len(&(CatenaFinitaCfg){.src = opener, .cap = CLI_CELLS_POOL});
    const char *const end = text + cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    CatalogTextLine read;
    CatalogTextLine first = {"", 0ull};
    while (EMBED_CALL(cli_line, CatalogScanCtx, .at = &at, .end = end, .line_pointer = &read))
    {
        read = EMBED_CALL(cli_trim, CatalogScanCtx, .line = read);
        if (read.length != 0ull)
        {
            first = read;
            break;
        }
    }
    unsigned long long close = 0ull;
    close = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = first.at, .at = 0ull,
                       .size = first.length, .set = ")");
    if (close == first.length)
    {
        return 0u;
    }
    unsigned int count = 0u;
    unsigned long long from = 0ull;
    for (unsigned long long cut = 0ull; cut <= close; cut += 1ull)
    {
        if ((cut == close) || (first.at[cut] == '|'))
        {
            CatalogTextLine name = {first.at + from, cut - from};
            name = EMBED_CALL(cli_trim, CatalogScanCtx, .line = name);
            const int named = (name.length != 0ull) && !((name.length == 1u) && (name.at[0] == '*'));
            if (named && (args->names_callback == NULL))
            {
                count += 1u;
            }
            else if (named && (count < args->capacity))
            {
                VerbaLinea name_line = {args->names_callback[count], CLI_NAME_MAX_LEN, 0ull};
                mmgr_line_put_n(&name_line, name.at, name.length);
                count += (mmgr_line_finish(&name_line) != 0ull) ? 1u : 0u;
            }
            from = cut + 1ull;
        }
    }
    return count;
}

// the text of the literals from `from` up to the first `\n` written in them, joined into `joined` where it is not
// NULL: its length
EMBED_INLINE unsigned long long cli_driver_join(const CatalogScanCtx *args)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->from, .cap = CLI_CELLS_POOL});
    unsigned long long length = 0ull;
    // the walk starts inside a literal, and each piece runs to its closing quote
    for (unsigned long long at = 0ull; at < size;)
    {
        const unsigned long long close = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                    .text = args->from, .at = at, .size = size, .set = "\"");
        const unsigned long long stop =
            EMBED_CALL(cli_text_at_size_first.to_pair, CliTextAtSizeFirstCfg, .text = args->from, .at = at,
                       .size = close, .first = '\\', .second = 'n');
        if (args->joined != NULL)
        {
            memor.cpy(
                &(MemoriaCfg){.dst = args->joined + length, .src = args->from + at, .bytes = (size_t)(stop - at)});
        }
        length += stop - at;
        if (stop < close)
        {
            return length;
        }
        const unsigned long long open =
            (close < size) ? EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = args->from,
                                        .at = close + 1ull, .size = size, .set = "\"")
                           : size;
        at = (open < size) ? (open + 1ull) : size;
    }
    return length;
}

unsigned int cli_catalog_driver_parts(const CliCatalogCapacityCtx *args)
{
    static const char s_marker[] = "in the order given:";
    const char *const found = cellul.find(
        &(CatenaFinitaCfg){.src = args->source, .cap = CLI_CELLS_POOL, .other = s_marker, .other_cap = CLI_CELLS_POOL});
    if (found == NULL)
    {
        return 0u;
    }
    const char *const literals = found + cellul.len(&(CatenaFinitaCfg){.src = s_marker, .cap = CLI_CELLS_POOL});
    const unsigned long long length = EMBED_CALL(cli_driver_join, CatalogScanCtx, .from = literals, .joined = NULL);
    char *const joined = g_cli_cells->temporary_buf_alloc(length + 1ull, &g_cli_catalog_cells);
    if (joined == NULL)
    {
        return 0u;
    }
    (void)EMBED_CALL(cli_driver_join, CatalogScanCtx, .from = literals, .joined = joined);
    unsigned int count = 0u;
    unsigned long long from = 0ull;
    for (unsigned long long cut = 0ull; cut <= length; cut += 1ull)
    {
        if ((cut == length) || (joined[cut] == ','))
        {
            CatalogTextLine part = {joined + from, cut - from};
            part = EMBED_CALL(cli_trim, CatalogScanCtx, .line = part);
            if ((part.length != 0ull) && (args->parts == NULL))
            {
                count += 1u;
            }
            else if ((part.length != 0ull) && (count < args->capacity))
            {
                VerbaLinea name_line = {args->parts[count], CLI_NAME_MAX_LEN, 0ull};
                mmgr_line_put_n(&name_line, part.at, part.length);
                count += (mmgr_line_finish(&name_line) != 0ull) ? 1u : 0u;
            }
            from = cut + 1ull;
        }
    }
    g_cli_cells->persistent_buf_release(joined, &g_cli_catalog_cells);
    return count;
}

// a README's first paragraph is the text of its lines, a space put for each line's end it passes, so it is no longer
// than the README
char *cli_catalog_markdown_alloc(const CliCatalogPathOutCtx *args)
{
    unsigned long long size = 0ull;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = args->path,
                                                  .size_pointer = &size, .owner = &g_cli_catalog_cells);
    if (bytes == NULL)
    {
        return NULL;
    }
    char *const opening = g_cli_cells->persistent_buf_alloc(size + 1ull, &g_cli_catalog_cells);
    if (opening == NULL)
    {
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes,
                   .owner = &g_cli_catalog_cells);
        return NULL;
    }
    VerbaLinea opened = {opening, size + 1ull, 0ull};
    // the file's bytes read as its text
    const char *at = (const char *)bytes;
    const char *const end = at + size;
    int count = 0;
    CatalogTextLine line;
    while (EMBED_CALL(cli_line, CatalogScanCtx, .at = &at, .end = end, .line_pointer = &line))
    {
        line = EMBED_CALL(cli_trim, CatalogScanCtx, .line = line);
        if ((line.length == 0ull) || EMBED_CALL(cli_starts, CatalogScanCtx, .line = line, .word = "#") ||
            EMBED_CALL(cli_starts, CatalogScanCtx, .line = line, .word = "**"))
        {
            if (count != 0)
            {
                break;
            }
            continue;
        }
        EMBED_CALL(cli_paragraph_append, CatalogScanCtx, .out = &opened, .count = &count, .text = line.at,
                   .length = line.length);
    }
    mmgr_line_finish(&opened);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_catalog_cells);
    return opening;
}
