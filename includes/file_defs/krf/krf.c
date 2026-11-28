// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// krf.c: a part's .krf read, its rows read, a removal stored, and written back whole (krf.h)
#include "krf.h"
#include "../../platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

#include "../../parsers/who.h"

// the longest head line, `krf <part>`
#define KRF_HEAD 256u

int krf_read(KrfFile *file, const char *path)
{
    return kfile_lines_read(file, path);
}

int krf_write_by(const KrfFile *file, const char *path, Who who)
{
    return kfile_lines_write_by(file, path, who);
}

void krf_free(KrfFile *file)
{
    kfile_lines_free(file);
}

int krf_line_add(KrfFile *file, const char *text)
{
    return kfile_lines_line_add(file, text);
}

// the word of `text` at `*at` into `word`, which has room for `room`. The call skips the spaces before it and moves
// `*at` past it. The return is 1, or 0 where there is no word or it does not fit
static int krf_word_read(const char *text, size_t *at, char *word, size_t room)
{
    *at +=
        EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = text + *at, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = " \t");
    const size_t length = EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = text + *at,
                                     .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = " \t");
    if ((length == 0u) || (length >= room))
    {
        return 0;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = word, .src = text + *at, .bytes = length);
    word[length] = '\0';
    *at += length;
    return 1;
}

int krf_row_read(const char *line, KrfRow *row)
{
    size_t at = 0u;
    char type[KRF_TYPE];
    if (!krf_word_read(line, &at, type, sizeof(type)) ||
        (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = type, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "removed",
                     .ci = EMBED_FALSE)) ||
        !krf_word_read(line, &at, row->type, sizeof(row->type)) ||
        !krf_word_read(line, &at, row->query, sizeof(row->query)) ||
        !krf_word_read(line, &at, row->reason, sizeof(row->reason)))
    {
        return 0;
    }
    at +=
        EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = line + at, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = " \t");
    {
        VerbaLinea buffer_line = mmgr_line_over(row->row, sizeof(row->row));
        mmgr_line_put(&buffer_line, line + at);
        (void)mmgr_line_finish(&buffer_line);
    }
    return 1;
}

// the name a path ends in, after its last separator, a slash written either way
static const char *krf_base_of(const char *path)
{
    const char *const slash =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = path, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '/');
    const char *const backslash =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = path, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '\\');
    const char *const separator = ((slash == NULL) || ((backslash != NULL) && (backslash > slash))) ? backslash : slash;
    return (separator == NULL) ? path : (separator + 1);
}

// how many characters of `base` are before its last extension, every one where it has none
static size_t krf_stem_length(const char *base)
{
    const char *const dot =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = base, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '.');
    // a name is shorter than a size_t counts, and the dot is at or past the name's start
    return (dot == NULL) ? EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = base, .cap = PLATFORM_TEXT_TO_TERMINATOR)
                         : (size_t)(dot - base);
}

int krf_query_named(char *name, size_t room)
{
    const char *const path = qry_member_path();
    if (path == NULL)
    {
        {
            VerbaLinea buffer_line = mmgr_line_over(name, room);
            mmgr_line_put(&buffer_line, "-");
            (void)mmgr_line_finish(&buffer_line);
        }
        return 0;
    }
    const char *const base = krf_base_of(path);
    // the stem is shorter than an int counts
    {
        VerbaLinea buffer_line = mmgr_line_over(name, room);
        mmgr_line_put_max(&buffer_line, base, (size_t)((int)krf_stem_length(base)));
        (void)mmgr_line_finish(&buffer_line);
    }
    return 1;
}

void krf_path_beside(const char *path, char *krf, size_t room)
{
    const char *const base = krf_base_of(path);
    // the path up to the base's last extension is shorter than an int counts
    const int stem = (int)((size_t)(base - path) + krf_stem_length(base));
    {
        VerbaLinea buffer_line = mmgr_line_over(krf, room);
        mmgr_line_put_max(&buffer_line, path, (size_t)(stem));
        mmgr_line_put(&buffer_line, ".krf");
        (void)mmgr_line_finish(&buffer_line);
    }
}

int krf_removed_by(const char *path, const char *type, const char *reason, const char *const *rows, unsigned int count,
                   Who who)
{
    KrfFile file = {0};
    if (!kfile_lines_read(&file, path))
    {
        return 0;
    }
    if (file.lines == 0u)
    {
        const char *const base = krf_base_of(path);
        char head[KRF_HEAD];
        // the part's name is shorter than an int counts
        {
            VerbaLinea buffer_line = mmgr_line_over(head, sizeof(head));
            mmgr_line_put(&buffer_line, "krf ");
            mmgr_line_put_max(&buffer_line, base, (size_t)((int)krf_stem_length(base)));
            (void)mmgr_line_finish(&buffer_line);
        }
        if (!kfile_lines_line_add(&file, head))
        {
            kfile_lines_free(&file);
            return 0;
        }
    }
    char query[KRF_QUERY];
    krf_query_named(query, sizeof(query));
    char **const lines =
        (count == 0u) ? NULL : platform_cell_cleared(count, sizeof(char *), platform_place_g_program_owner, 0);
    int made = (count == 0u) || (lines != NULL);
    for (unsigned int at = 0u; made && (at < count); at += 1u)
    {
        const size_t length =
            EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = "removed ", .cap = PLATFORM_TEXT_TO_TERMINATOR) +
            EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = type, .cap = PLATFORM_TEXT_TO_TERMINATOR) +
            EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = query, .cap = PLATFORM_TEXT_TO_TERMINATOR) +
            EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = reason, .cap = PLATFORM_TEXT_TO_TERMINATOR) +
            EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = rows[at], .cap = PLATFORM_TEXT_TO_TERMINATOR) + 4u;
        lines[at] = platform_cell_taken(length, platform_place_g_program_owner, 0);
        made = lines[at] != NULL;
        if (made)
        {
            VerbaLinea buffer_line = mmgr_line_over(lines[at], length);
            mmgr_line_put(&buffer_line, "removed ");
            mmgr_line_put(&buffer_line, type);
            mmgr_line_put(&buffer_line, " ");
            mmgr_line_put(&buffer_line, query);
            mmgr_line_put(&buffer_line, " ");
            mmgr_line_put(&buffer_line, reason);
            mmgr_line_put(&buffer_line, " ");
            mmgr_line_put(&buffer_line, rows[at]);
            (void)mmgr_line_finish(&buffer_line);
        }
    }
    const unsigned int put = made ? kfile_lines_put_by(&file, file.lines, (const char *const *)lines, count, who) : 0u;
    const int written = made && ((put != 0u) || (count == 0u)) && kfile_lines_write_by(&file, path, who);
    for (unsigned int at = 0u; (lines != NULL) && (at < count); at += 1u)
    {
        platform_cell_released(lines[at], platform_place_g_program_owner);
    }
    platform_cell_released(lines, platform_place_g_program_owner);
    kfile_lines_free(&file);
    return written;
}
