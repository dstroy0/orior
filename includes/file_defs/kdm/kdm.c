// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kdm.c: a part's .kdm read, its rows read, its section and its pipes written by a who, and written back whole (kdm.h)
#include "kdm.h"
#include "../../platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

#include "../../parsers/who.h"

int kdm_read(KdmFile *file, const char *path)
{
    return kfile_lines_read(file, path);
}

int kdm_write_by(const KdmFile *file, const char *path, Who who)
{
    return kfile_lines_write_by(file, path, who);
}

void kdm_free(KdmFile *file)
{
    kfile_lines_free(file);
}

int kdm_row_read(const char *line, KdmRow *row)
{
    const char *field[6];
    size_t length[6];
    const char *at = line;
    unsigned int fields = 0u;
    while (fields < 6u)
    {
        field[fields] = at;
        length[fields] =
            EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = at, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "\t");
        at += length[fields];
        fields += 1u;
        if (*at != '\t')
        {
            break;
        }
        at += 1;
    }
    if ((*at != '\0') || (fields < 5u) || (length[0] == 0u) || (field[0][0] == '#') ||
        (length[0] >= sizeof(row->operation)) || (length[2] >= sizeof(row->chain)) || (length[3] >= sizeof(row->cost)))
    {
        return 0;
    }
    // Explicit cast narrows the signed word of the read to the int of a verdict, which holds one of a few values
    row->verdict = (fields == 6u) ? (int)EMBED_CALL(cellul.to_long, TransfiguroCfg, .src = field[5], .end = NULL) : -1;
    // the call cuts each field to its length, which an int counts for a field that fits its room
    {
        VerbaLinea buffer_line = mmgr_line_over(row->operation, sizeof(row->operation));
        mmgr_line_put_max(&buffer_line, field[0], (size_t)((int)length[0]));
        (void)mmgr_line_finish(&buffer_line);
    }
    row->nodes = (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = field[1], .end = NULL, .base = 10);
    {
        VerbaLinea buffer_line = mmgr_line_over(row->chain, sizeof(row->chain));
        mmgr_line_put_max(&buffer_line, field[2], (size_t)((int)length[2]));
        (void)mmgr_line_finish(&buffer_line);
    }
    {
        VerbaLinea buffer_line = mmgr_line_over(row->cost, sizeof(row->cost));
        mmgr_line_put_max(&buffer_line, field[3], (size_t)((int)length[3]));
        (void)mmgr_line_finish(&buffer_line);
    }
    row->runs = (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = field[4], .end = NULL, .base = 10);
    return 1;
}

unsigned int kdm_tail_at(const KdmFile *file)
{
    unsigned int head = file->lines;
    for (unsigned int at = 0u; (at < file->lines) && (head == file->lines); at += 1u)
    {
        head = (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = file->line[at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                           .other = KDM_ROWS_HEAD, .ci = EMBED_FALSE))
                   ? at
                   : head;
    }
    if (head == file->lines)
    {
        return file->lines;
    }
    static KdmRow s_row;
    unsigned int tail = head + 1u;
    for (unsigned int at = head + 1u; at < file->lines; at += 1u)
    {
        tail = kdm_row_read(file->line[at], &s_row) ? (at + 1u) : tail;
    }
    return tail;
}

int kdm_section_written_by(KdmFile *file, const char *part, const char *const *lines, unsigned int count, Who who)
{
    const unsigned int tail = kdm_tail_at(file);
    Who there;
    const int there_has_who = (tail < file->lines) && kfile_lines_who_at(file, tail, &there);
    Who own;
    int own_has_who = 0;
    const int tail_own = (tail < file->lines) && kfile_lines_who_line(file->line[tail], &own, &own_has_who);
    KdmFile written = {NULL, 0u, 0u};
    char first[KDM_OPERATOR + 8u];
    {
        VerbaLinea buffer_line = mmgr_line_over(first, sizeof(first));
        mmgr_line_put(&buffer_line, "kdm ");
        mmgr_line_put(&buffer_line, part);
        (void)mmgr_line_finish(&buffer_line);
    }
    const int headed =
        (file->lines != 0u) && (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = file->line[0],
                                           .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "kdm ", .ci = EMBED_FALSE));
    int ok = kfile_lines_line_add(&written, headed ? file->line[0] : first) &&
             ((count == 0u) || (kfile_lines_put_by(&written, written.lines, lines, count, who) != 0u));
    if (ok && (tail < file->lines) && !tail_own &&
        (!there_has_who || (there.source != who.source) || (there.line != who.line) || (there.program != who.program)))
    {
        char code[WHO_CODE_CHARACTERS + 1u];
        char whose[WHO_CODE_CHARACTERS + 8u];
        who_code(there, code, sizeof(code));
        {
            VerbaLinea buffer_line = mmgr_line_over(whose, sizeof(whose));
            mmgr_line_put(&buffer_line, "who ");
            mmgr_line_put(&buffer_line, there_has_who ? code : "-");
            (void)mmgr_line_finish(&buffer_line);
        }
        ok = kfile_lines_line_add(&written, whose);
    }
    for (unsigned int at = tail; ok && (at < file->lines); at += 1u)
    {
        ok = kfile_lines_line_add(&written, file->line[at]);
    }
    if (!ok)
    {
        kfile_lines_free(&written);
        return 0;
    }
    kfile_lines_free(file);
    *file = written;
    return 1;
}

int kdm_pipes_replaced_by(KdmFile *file, const char *const *comment, unsigned int comments, const char *const *pipes,
                          unsigned int count, Who who)
{
    // the lines put, made before the call touches the file: a blank line, the comment and the pipes
    const char **const put = (const char **)platform_cell_cleared((unsigned long long)((size_t)comments + count + 2u),
                                                                  (unsigned long long)(sizeof(char *)),
                                                                  (unsigned int)platform_place_g_program_owner, 0);
    if (put == NULL)
    {
        return 0;
    }
    unsigned int putting = 0u;
    put[putting] = "";
    putting += 1u;
    for (unsigned int at = 0u; at < comments; at += 1u)
    {
        put[putting] = comment[at];
        putting += 1u;
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        put[putting] = pipes[at];
        putting += 1u;
    }
    const unsigned int tail = kdm_tail_at(file);
    unsigned int at = file->lines;
    while (at > tail)
    {
        at -= 1u;
        int dropped = (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = file->line[at],
                                  .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "pipe ", .ci = EMBED_FALSE));
        for (unsigned int line = 0u; (line < comments) && !dropped; line += 1u)
        {
            dropped = (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = file->line[at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                  .other = comment[line], .ci = EMBED_FALSE));
        }
        if (dropped)
        {
            kfile_lines_line_drop(file, at);
        }
    }
    kfile_lines_who_tidied(file);
    while ((file->lines != 0u) && (file->line[file->lines - 1u][0] == '\0'))
    {
        kfile_lines_line_drop(file, file->lines - 1u);
    }
    const int ok = (count == 0u) || (kfile_lines_put_by(file, file->lines, put, putting, who) != 0u);
    platform_cell_released(put, (unsigned int)platform_place_g_program_owner);
    return ok;
}
