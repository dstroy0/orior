// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ksc.c: a system's .ksc read, its rows read and replaced, and written back whole (ksc.h)
#include "ksc.h"

#include "../../parsers/line_read.h"
#include "../../parsers/who.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the most answers a channel's counts are taken again over
#define KSC_ANSWERS 32u

int ksc_read(KscFile *file, const char *path)
{
    return kfile_lines_read(file, path);
}

int ksc_write_by(const KscFile *file, const char *path, Who who)
{
    return kfile_lines_write_by(file, path, who);
}

void ksc_free(KscFile *file)
{
    kfile_lines_free(file);
}

int ksc_line_add(KscFile *file, const char *text)
{
    return kfile_lines_line_add(file, text);
}

// the word of `text` at `*at` into `word`, which holds `room`, the spaces before it skipped and `*at` moved past it: 1,
// or 0 where there is no word or it does not fit
static int ksc_word_read(const char *text, size_t *at, char *word, size_t room)
{
    *at += strspn(text + *at, " \t");
    const size_t length = strcspn(text + *at, " \t");
    if ((length == 0u) || (length >= room))
    {
        return 0;
    }
    memcpy(word, text + *at, length);
    word[length] = '\0';
    *at += length;
    return 1;
}

int ksc_row_read(const char *line, const char *channel, KscRow *row)
{
    size_t at = 0u;
    if (!ksc_word_read(line, &at, row->channel, sizeof(row->channel)) || (strcmp(row->channel, channel) != 0) ||
        !ksc_word_read(line, &at, row->answer, sizeof(row->answer)) ||
        !ksc_word_read(line, &at, row->word, sizeof(row->word)))
    {
        return 0;
    }
    char *after = NULL;
    row->value = strtoull(row->word, &after, 16);
    at += strspn(line + at, " \t");
    snprintf(row->question, sizeof(row->question), "%s", line + at);
    return 1;
}

// 1 where the question of `line` is one of `channel` that begins with the words `asked`
static int ksc_row_asked(const char *line, const char *channel, const char *asked)
{
    static KscRow s_row;
    const size_t length = strlen(asked);
    return ksc_row_read(line, channel, &s_row) && (strncmp(s_row.question, asked, length) == 0) &&
           ((s_row.question[length] == '\0') || (s_row.question[length] == ' '));
}

// 1 where `line` is a count of `channel`, `count <channel> <answer> <number>`, with the answer in `answer`
static int ksc_count_read(const char *line, const char *channel, char *answer)
{
    size_t at = 0u;
    char word[KSC_CHANNEL];
    char counted[KSC_CHANNEL];
    return ksc_word_read(line, &at, word, sizeof(word)) && (strcmp(word, "count") == 0) &&
           ksc_word_read(line, &at, counted, sizeof(counted)) && (strcmp(counted, channel) == 0) &&
           ksc_word_read(line, &at, answer, KSC_ANSWER);
}

int ksc_rows_replaced_by(KscFile *file, const char *channel, const char *asked, const char *const *rows,
                         unsigned int count, Who who)
{
    return ksc_rows_replaced_traced_by(file, channel, asked, rows, count, who, NULL);
}

int ksc_rows_replaced_traced_by(KscFile *file, const char *channel, const char *asked, const char *const *rows,
                                unsigned int count, Who who, KfileLines *removed)
{
    static KscRow s_row;
    // the rows let go that no row put gives again, in the order the file holds them, before any line moves
    for (unsigned int at = 0u; (removed != NULL) && (at < file->lines); at += 1u)
    {
        if (!ksc_row_asked(file->line[at], channel, asked))
        {
            continue;
        }
        unsigned int same = 0u;
        while ((same < count) && (strcmp(rows[same], file->line[at]) != 0))
        {
            same += 1u;
        }
        if ((same == count) && !kfile_lines_line_add(removed, file->line[at]))
        {
            return 0;
        }
    }
    // the rows go where the first row of the question stood, so that a row put again keeps its place; where the file
    // holds none of the question, after the last row of the channel it keeps, or after its last line where it keeps
    // none of the channel
    unsigned int place = file->lines;
    unsigned int first_asked = file->lines;
    for (unsigned int at = 0u; at < file->lines; at += 1u)
    {
        if (!ksc_row_read(file->line[at], channel, &s_row))
        {
            continue;
        }
        if (ksc_row_asked(file->line[at], channel, asked))
        {
            first_asked = (first_asked == file->lines) ? at : first_asked;
        }
        else
        {
            place = at + 1u;
        }
    }
    place = (first_asked != file->lines) ? first_asked : place;
    const unsigned int put = kfile_lines_put_by(file, place, rows, count, who);
    if ((put == 0u) && (count != 0u))
    {
        return 0;
    }
    // every row asked let go but those just put
    unsigned int at = file->lines;
    while (at != 0u)
    {
        at -= 1u;
        if (((at < place) || (at >= (place + put))) && ksc_row_asked(file->line[at], channel, asked))
        {
            kfile_lines_line_drop(file, at);
        }
    }
    kfile_lines_who_tidied(file);
    // each answer of the channel and the rows that give it, then each count of the channel written again over them
    char answers[KSC_ANSWERS][KSC_ANSWER];
    unsigned int counted[KSC_ANSWERS];
    unsigned int distinct = 0u;
    for (at = 0u; at < file->lines; at += 1u)
    {
        if (!ksc_row_read(file->line[at], channel, &s_row))
        {
            continue;
        }
        unsigned int seen = 0u;
        while ((seen < distinct) && (strcmp(answers[seen], s_row.answer) != 0))
        {
            seen += 1u;
        }
        if ((seen == distinct) && (distinct < KSC_ANSWERS))
        {
            snprintf(answers[distinct], sizeof(answers[distinct]), "%s", s_row.answer);
            counted[distinct] = 0u;
            distinct += 1u;
        }
        counted[seen] += (seen < distinct) ? 1u : 0u;
    }
    for (at = 0u; at < file->lines; at += 1u)
    {
        char answer[KSC_ANSWER];
        if (!ksc_count_read(file->line[at], channel, answer))
        {
            continue;
        }
        unsigned int seen = 0u;
        while ((seen < distinct) && (strcmp(answers[seen], answer) != 0))
        {
            seen += 1u;
        }
        char line[KSC_CHANNEL + KSC_ANSWER + 32u];
        snprintf(line, sizeof(line), "count %-8s %-8s %u", channel, answer, (seen < distinct) ? counted[seen] : 0u);
        kfile_lines_line_replaced(file, at, line);
    }
    return 1;
}

int ksc_field_read(const char *line, char *name, size_t room, unsigned int *from, unsigned int *width)
{
    char kind[8];
    unsigned long long first = 0ull;
    unsigned long long bits = 0ull;
    const char *at = line;
    if (!line_word(&at, kind, sizeof(kind)) || (strcmp(kind, "field") != 0) || !line_word(&at, name, room) ||
        !line_number(&at, 0, &first) || !line_number(&at, 0, &bits) || (first > 0xffffffffull) ||
        (bits > 0xffffffffull))
    {
        return 0;
    }
    // each a bit's place or a count of bits, which an unsigned int holds
    *from = (unsigned int)first;
    *width = (unsigned int)bits;
    return 1;
}
