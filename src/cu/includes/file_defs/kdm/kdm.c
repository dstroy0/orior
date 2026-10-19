// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kdm.c: a part's .kdm read, its rows read, its section and its pipes written by a who, and written back whole (kdm.h)
#include "kdm.h"

#include "../../parsers/who.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        length[fields] = strcspn(at, "\t");
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
    row->verdict = (fields == 6u) ? (int)strtol(field[5], NULL, 10) : -1;
    // each field is cut to its length, which an int counts for a field that fits its room
    snprintf(row->operation, sizeof(row->operation), "%.*s", (int)length[0], field[0]);
    row->nodes = (unsigned int)strtoul(field[1], NULL, 10);
    snprintf(row->chain, sizeof(row->chain), "%.*s", (int)length[2], field[2]);
    snprintf(row->cost, sizeof(row->cost), "%.*s", (int)length[3], field[3]);
    row->runs = (unsigned int)strtoul(field[4], NULL, 10);
    return 1;
}

unsigned int kdm_tail_at(const KdmFile *file)
{
    unsigned int head = file->lines;
    for (unsigned int at = 0u; (at < file->lines) && (head == file->lines); at += 1u)
    {
        head = (strcmp(file->line[at], KDM_ROWS_HEAD) == 0) ? at : head;
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
    const int there_held = (tail < file->lines) && kfile_lines_who_at(file, tail, &there);
    Who own;
    int own_held = 0;
    const int tail_own = (tail < file->lines) && kfile_lines_who_line(file->line[tail], &own, &own_held);
    KdmFile written = {NULL, 0u, 0u};
    char first[KDM_OPERATOR + 8u];
    snprintf(first, sizeof(first), "kdm %s", part);
    const int headed = (file->lines != 0u) && (strncmp(file->line[0], "kdm ", 4u) == 0);
    int held = kfile_lines_line_add(&written, headed ? file->line[0] : first) &&
               ((count == 0u) || (kfile_lines_put_by(&written, written.lines, lines, count, who) != 0u));
    if (held && (tail < file->lines) && !tail_own &&
        (!there_held || (there.source != who.source) || (there.line != who.line) || (there.program != who.program)))
    {
        char code[WHO_CODE_LETTERS + 1u];
        char whose[WHO_CODE_LETTERS + 8u];
        who_code(there, code, sizeof(code));
        snprintf(whose, sizeof(whose), "who %s", there_held ? code : "-");
        held = kfile_lines_line_add(&written, whose);
    }
    for (unsigned int at = tail; held && (at < file->lines); at += 1u)
    {
        held = kfile_lines_line_add(&written, file->line[at]);
    }
    if (!held)
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
    // the lines put, made before the file is touched: a blank line, the comment and the pipes
    const char **const put = (const char **)calloc((size_t)comments + count + 2u, sizeof(char *));
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
        int dropped = (strncmp(file->line[at], "pipe ", 5u) == 0);
        for (unsigned int line = 0u; (line < comments) && !dropped; line += 1u)
        {
            dropped = (strcmp(file->line[at], comment[line]) == 0);
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
    const int held = (count == 0u) || (kfile_lines_put_by(file, file->lines, put, putting, who) != 0u);
    free(put);
    return held;
}
