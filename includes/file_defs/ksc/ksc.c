// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ksc.c: a system's .ksc read, its rows read and replaced, and written back whole (ksc.h)
#include "ksc.h"
#include "../../platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

#include "../../parsers/line_read.h"
#include "../../parsers/who.h"

// the order of the texts `left` and `right`, each read to its terminator or to `cap`: below 0 where `left`
// comes first, 0 where they are the same, above 0 where `right` does
static inline int ksc_text_order(const char *left, const char *right, size_t cap)
{
    const size_t left_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = left, .cap = cap);
    const size_t right_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = right, .cap = cap);
    const size_t shorter = (left_length < right_length) ? left_length : right_length;
    const size_t reach = (shorter < cap) ? (shorter + 1u) : cap;
    const size_t differs =
        EMBED_CALL(cellul.diff, CatenaFinitaCfg, .src = left, .cap = reach, .other = right, .ci = EMBED_FALSE);
    if (differs >= reach)
    {
        return 0;
    }
    // a byte of a text read as unsigned, and its order as a signed count
    return (int)(unsigned char)left[differs] - (int)(unsigned char)right[differs];
}

// the max answers a channel's counts are taken again over
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

// the word of `text` at `*at` into `word`, which has room for `room`, the spaces before it skipped and `*at` moved past
// it: 1, or 0 where there is no word or it does not fit
static int ksc_word_read(const char *text, size_t *at, char *word, size_t room)
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

int ksc_row_read(const char *line, const char *channel, KscRow *row)
{
    size_t at = 0u;
    if (!ksc_word_read(line, &at, row->channel, sizeof(row->channel)) ||
        (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = row->channel, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                     .other = channel, .ci = EMBED_FALSE)) ||
        !ksc_word_read(line, &at, row->answer, sizeof(row->answer)) ||
        !ksc_word_read(line, &at, row->word, sizeof(row->word)))
    {
        return 0;
    }
    const char *after = NULL;
    row->value = EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = row->word, .end = &after, .base = 16);
    at +=
        EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = line + at, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = " \t");
    {
        VerbaLinea buffer_line = mmgr_line_over(row->question, sizeof(row->question));
        mmgr_line_put(&buffer_line, line + at);
        (void)mmgr_line_finish(&buffer_line);
    }
    return 1;
}

// 1 where the question of `line` is one of `channel` that begins with the words `question_prefix`
static int ksc_row_question_starts(const char *line, const char *channel, const char *question_prefix)
{
    static KscRow s_row;
    const size_t length =
        EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = question_prefix, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    return ksc_row_read(line, channel, &s_row) &&
           (ksc_text_order(s_row.question, question_prefix, (size_t)(length)) == 0) &&
           ((s_row.question[length] == '\0') || (s_row.question[length] == ' '));
}

// 1 where `line` is a count of `channel`, `count <channel> <answer> <number>`, with the answer in `answer`
static int ksc_count_read(const char *line, const char *channel, char *answer)
{
    size_t at = 0u;
    char word[KSC_CHANNEL];
    char counted[KSC_CHANNEL];
    return ksc_word_read(line, &at, word, sizeof(word)) &&
           (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = word, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "count",
                       .ci = EMBED_FALSE)) &&
           ksc_word_read(line, &at, counted, sizeof(counted)) &&
           (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = counted, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = channel,
                       .ci = EMBED_FALSE)) &&
           ksc_word_read(line, &at, answer, KSC_ANSWER);
}

int ksc_rows_replaced_by(KscFile *file, const char *channel, const char *question_prefix, const char *const *rows,
                         unsigned int count, Who who)
{
    return ksc_rows_replaced_traced_by(file, channel, question_prefix, rows, count, who, NULL);
}

int ksc_rows_replaced_traced_by(KscFile *file, const char *channel, const char *question_prefix,
                                const char *const *rows, unsigned int count, Who who, KfileLines *removed)
{
    static KscRow s_row;
    // the rows let go that no row put gives again, in the order the file contains them, before any line moves
    for (unsigned int at = 0u; (removed != NULL) && (at < file->lines); at += 1u)
    {
        if (!ksc_row_question_starts(file->line[at], channel, question_prefix))
        {
            continue;
        }
        unsigned int same = 0u;
        while ((same < count) &&
               (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = rows[same], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = file->line[at], .ci = EMBED_FALSE)))
        {
            same += 1u;
        }
        if ((same == count) && !kfile_lines_line_add(removed, file->line[at]))
        {
            return 0;
        }
    }
    // the rows go where the first row of the question was, so that a row put again retains its place; where the file
    // contains none of the question, after the last row of the channel it retains, or after its last line where it
    // retains none of the channel
    unsigned int place = file->lines;
    unsigned int first_match = file->lines;
    for (unsigned int at = 0u; at < file->lines; at += 1u)
    {
        if (!ksc_row_read(file->line[at], channel, &s_row))
        {
            continue;
        }
        if (ksc_row_question_starts(file->line[at], channel, question_prefix))
        {
            first_match = (first_match == file->lines) ? at : first_match;
        }
        else
        {
            place = at + 1u;
        }
    }
    place = (first_match != file->lines) ? first_match : place;
    const unsigned int put = kfile_lines_put_by(file, place, rows, count, who);
    if ((put == 0u) && (count != 0u))
    {
        return 0;
    }
    // every row matched let go but those just put
    unsigned int at = file->lines;
    while (at != 0u)
    {
        at -= 1u;
        if (((at < place) || (at >= (place + put))) &&
            ksc_row_question_starts(file->line[at], channel, question_prefix))
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
        while ((seen < distinct) &&
               (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = answers[seen], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = s_row.answer, .ci = EMBED_FALSE)))
        {
            seen += 1u;
        }
        if ((seen == distinct) && (distinct < KSC_ANSWERS))
        {
            {
                VerbaLinea buffer_line = mmgr_line_over(answers[distinct], sizeof(answers[distinct]));
                mmgr_line_put(&buffer_line, s_row.answer);
                (void)mmgr_line_finish(&buffer_line);
            }
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
        while ((seen < distinct) &&
               (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = answers[seen], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = answer, .ci = EMBED_FALSE)))
        {
            seen += 1u;
        }
        char line[KSC_CHANNEL + KSC_ANSWER + 32u];
        {
            VerbaLinea buffer_line = mmgr_line_over(line, sizeof(line));
            mmgr_line_put(&buffer_line, "count ");
            mmgr_line_put_columns(&buffer_line, channel, 8u);
            mmgr_line_put(&buffer_line, " ");
            mmgr_line_put_columns(&buffer_line, answer, 8u);
            mmgr_line_put(&buffer_line, " ");
            mmgr_line_decimal(&buffer_line, (seen < distinct) ? counted[seen] : 0u, 1u);
            (void)mmgr_line_finish(&buffer_line);
        }
        kfile_lines_line_replaced(file, at, line);
    }
    return 1;
}

int ksc_field_read(const char *line, char *name, size_t room, unsigned int *from, unsigned int *width)
{
    char type[8];
    unsigned long long first = 0ULL;
    unsigned long long bits = 0ULL;
    const char *at = line;
    if (!line_word(&at, type, sizeof(type)) ||
        (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = type, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "field",
                     .ci = EMBED_FALSE)) ||
        !line_word(&at, name, room) || !line_number(&at, 0, &first) || !line_number(&at, 0, &bits) ||
        (first > 0xffffffffULL) || (bits > 0xffffffffULL))
    {
        return 0;
    }
    // each a bit's place or a count of bits, which an unsigned int represents
    *from = (unsigned int)first;
    *width = (unsigned int)bits;
    return 1;
}
