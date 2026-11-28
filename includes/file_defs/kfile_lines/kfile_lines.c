// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kfile_lines.c: a k-file people read and write, its lines read, put by a who, and written back whole
// (kfile_lines.h)
#include "kfile_lines.h"
#include "../../platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "verba_scribo/verba_scribo.h"

#include "../klang/klang.h"

#include "../../../src/cu/transpiler/lstar/protocol/table_text.h"
#include "../../parsers/who.h"

// the order of the texts `left` and `right`, each read to its terminator or to `cap`: below 0 where `left`
// comes first, 0 where they are the same, above 0 where `right` does
static inline int kfile_lines_text_order(const char *left, const char *right, size_t cap)
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

// `length` characters at `text` copied into an allocation of their own, ended: NULL where there was no memory
static char *kfile_lines_copy(const char *text, size_t length)
{
    char *const copy = (char *)platform_cell_taken(length + 1u, platform_place_g_program_owner, 0);
    if (copy != NULL)
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = copy, .src = text, .bytes = length);
        copy[length] = '\0';
    }
    return copy;
}

// `file` given room for `more` lines past those it contains: 1, or 0 where there was no memory
static int kfile_lines_room(KfileLines *file, unsigned int more)
{
    if ((file->lines + more) <= file->room)
    {
        return 1;
    }
    unsigned int grown = (file->room == 0u) ? 256u : file->room;
    while (grown < (file->lines + more))
    {
        grown *= 2u;
    }
    char **const larger =
        (char **)platform_cell_grown(file->line, (size_t)file->room * sizeof(char *), (size_t)grown * sizeof(char *),
                                     platform_place_g_program_owner, 0);
    if (larger == NULL)
    {
        return 0;
    }
    file->line = larger;
    file->room = grown;
    return 1;
}

int kfile_lines_read(KfileLines *file, const char *path)
{
    kfile_lines_free(file);
    // the file as the run stores it: its latest blob, or the file where the run stores none (qry_member_file)
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ULL;
    if (!qry_member_file_blob(path, &bytes, &size))
    {
        return 1;
    }
    int ok = 1;
    unsigned long long at = 0ULL;
    while (ok && (at < size))
    {
        // a file read whole is at max what a size_t counts
        const char *const start = (const char *)(bytes + at);
        const char *const end =
            (const char *)EMBED_CALL(memor.chr, MemoriaCfg, .src = start, .bytes = (size_t)(size - at), .val = '\n');
        size_t length = (end != NULL) ? (size_t)(end - start) : (size_t)(size - at);
        at += (unsigned long long)length + ((end != NULL) ? 1ULL : 0ULL);
        length -= ((length != 0u) && (start[length - 1u] == '\r')) ? 1u : 0u;
        char *const copy = kfile_lines_copy(start, length);
        ok = (copy != NULL) && kfile_lines_room(file, 1u);
        if (ok)
        {
            file->line[file->lines] = copy;
            file->lines += 1u;
        }
        else
        {
            platform_cell_released(copy, platform_place_g_program_owner);
        }
    }
    return ok;
}

int kfile_lines_write_by(const KfileLines *file, const char *path, Who who)
{
    // the file put together in memory and passed whole to the run's writer, which writes it in the file's place
    static char s_text_bytes[QRY_WRITER_RING_BYTES / 2ULL];
    size_t written = 0u;
    for (unsigned int at = 0u; at < file->lines; at += 1u)
    {
        written = verba_textus.put(
            &(VerbaTextusCfg){.out = s_text_bytes, .cap = sizeof(s_text_bytes), .at = written, .text = file->line[at]});
        written = verba_littera.ch(
            &(VerbaLitteraCfg){.out = s_text_bytes, .cap = sizeof(s_text_bytes), .at = written, .ch = '\n'});
    }
    const unsigned long long time = verba_finis.ok(&(VerbaFinisCfg){.cap = sizeof(s_text_bytes), .at = written})
                                        ? qry_member_filed(path, s_text_bytes, written)
                                        : 0ULL;
    const char *const name = qry_member_file_name(path);
    if ((time != 0ULL) && (name != NULL))
    {
        // the file's row of the table files (files.oracle.tsv): the who that wrote it, its record and its name
        unsigned long long row[4] = {
            0ULL, 0ULL, time,
            table_text(name, EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR))};
        who_words(who, row);
        qry_member_append(TABLE_FILES, row, sizeof(row), QRY_CRITICAL);
    }
    return time != 0ULL;
}

void kfile_lines_free(KfileLines *file)
{
    for (unsigned int at = 0u; at < file->lines; at += 1u)
    {
        platform_cell_released(file->line[at], platform_place_g_program_owner);
    }
    platform_cell_released(file->line, platform_place_g_program_owner);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = file, .bytes = sizeof(*file), .val = 0);
}

int kfile_lines_line_add(KfileLines *file, const char *text)
{
    char *const copy = kfile_lines_copy(
        text, EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR));
    if ((copy == NULL) || !kfile_lines_room(file, 1u))
    {
        platform_cell_released(copy, platform_place_g_program_owner);
        return 0;
    }
    file->line[file->lines] = copy;
    file->lines += 1u;
    return 1;
}

int kfile_lines_line_replaced(KfileLines *file, unsigned int at, const char *text)
{
    char *const copy = (at < file->lines) ? kfile_lines_copy(text, EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text,
                                                                              .cap = PLATFORM_TEXT_TO_TERMINATOR))
                                          : NULL;
    if (copy == NULL)
    {
        return 0;
    }
    platform_cell_released(file->line[at], platform_place_g_program_owner);
    file->line[at] = copy;
    return 1;
}

void kfile_lines_line_drop(KfileLines *file, unsigned int at)
{
    if (at >= file->lines)
    {
        return;
    }
    platform_cell_released(file->line[at], platform_place_g_program_owner);
    (((const unsigned char *)(&file->line[at]) <= (const unsigned char *)(&file->line[at + 1u]))
         ? EMBED_CALL(memor.move_down, MemoriaCfg, .dst = &file->line[at], .src = &file->line[at + 1u],
                      .bytes = (size_t)(file->lines - at - 1u) * sizeof(char *))
         : EMBED_CALL(memor.move_up, MemoriaCfg, .dst = &file->line[at], .src = &file->line[at + 1u],
                      .bytes = (size_t)(file->lines - at - 1u) * sizeof(char *)));
    file->lines -= 1u;
}

int kfile_lines_who_line(const char *line, Who *who, int *has_who)
{
    if (!EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "who ",
                    .ci = EMBED_FALSE))
    {
        return 0;
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = line + 4, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "-",
                   .ci = EMBED_FALSE))
    {
        *has_who = 0;
        return 1;
    }
    *has_who = who_read(line + 4, who) && (line[4u + WHO_CODE_CHARACTERS] == '\0');
    return *has_who;
}

int kfile_lines_who_at(const KfileLines *file, unsigned int at, Who *who)
{
    for (unsigned int before = (at < file->lines) ? at : file->lines; before != 0u; before -= 1u)
    {
        int has_who = 0;
        if (kfile_lines_who_line(file->line[before - 1u], who, &has_who))
        {
            return has_who;
        }
    }
    return 0;
}

// the who line of `who` into `line`, which has room for WHO_CODE_CHARACTERS + 5, `who -` where `has_who` is 0
static void kfile_lines_who_written(Who who, int has_who, char *line, size_t room)
{
    char code[WHO_CODE_CHARACTERS + 1u];
    who_code(who, code, sizeof(code));
    {
        VerbaLinea buffer_line = mmgr_line_over(line, room);
        mmgr_line_put(&buffer_line, "who ");
        mmgr_line_put(&buffer_line, has_who ? code : "-");
        (void)mmgr_line_finish(&buffer_line);
    }
}

unsigned int kfile_lines_put_by(KfileLines *file, unsigned int at, const char *const *lines, unsigned int count,
                                Who who)
{
    at = (at < file->lines) ? at : file->lines;
    Who there;
    const int there_has_who = kfile_lines_who_at(file, at, &there);
    const int own =
        there_has_who && (there.source == who.source) && (there.line == who.line) && (there.program == who.program);
    // lines of the file follow where `at` is before one that is no who line of its own
    Who next_who;
    int next_has_who = 0;
    const int follows = (at < file->lines) && !kfile_lines_who_line(file->line[at], &next_who, &next_has_who);
    // the lines put, each its own allocation, made before the file is touched
    const unsigned int put = count + (own ? 0u : 1u) + ((!own && follows) ? 1u : 0u);
    char **const made =
        (char **)platform_cell_cleared((size_t)put + 1u, sizeof(char *), platform_place_g_program_owner, 0);
    if ((made == NULL) || !kfile_lines_room(file, put))
    {
        platform_cell_released(made, platform_place_g_program_owner);
        return 0u;
    }
    char whose[WHO_CODE_CHARACTERS + 8u];
    unsigned int next = 0u;
    int ok = 1;
    if (!own)
    {
        kfile_lines_who_written(who, 1, whose, sizeof(whose));
        made[next] = kfile_lines_copy(
            whose, EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = whose, .cap = PLATFORM_TEXT_TO_TERMINATOR));
        ok = ok && (made[next] != NULL);
        next += 1u;
    }
    for (unsigned int line = 0u; line < count; line += 1u)
    {
        made[next] = kfile_lines_copy(lines[line], EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = lines[line],
                                                              .cap = PLATFORM_TEXT_TO_TERMINATOR));
        ok = ok && (made[next] != NULL);
        next += 1u;
    }
    if (!own && follows)
    {
        kfile_lines_who_written(there, there_has_who, whose, sizeof(whose));
        made[next] = kfile_lines_copy(
            whose, EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = whose, .cap = PLATFORM_TEXT_TO_TERMINATOR));
        ok = ok && (made[next] != NULL);
        next += 1u;
    }
    if (!ok)
    {
        for (unsigned int line = 0u; line < next; line += 1u)
        {
            platform_cell_released(made[line], platform_place_g_program_owner);
        }
        platform_cell_released(made, platform_place_g_program_owner);
        return 0u;
    }
    (((const unsigned char *)(&file->line[at + put]) <= (const unsigned char *)(&file->line[at]))
         ? EMBED_CALL(memor.move_down, MemoriaCfg, .dst = &file->line[at + put], .src = &file->line[at],
                      .bytes = (size_t)(file->lines - at) * sizeof(char *))
         : EMBED_CALL(memor.move_up, MemoriaCfg, .dst = &file->line[at + put], .src = &file->line[at],
                      .bytes = (size_t)(file->lines - at) * sizeof(char *)));
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &file->line[at], .src = made, .bytes = (size_t)put * sizeof(char *));
    file->lines += put;
    platform_cell_released(made, platform_place_g_program_owner);
    return put;
}

int kfile_lines_cut(char *text, unsigned long long size, const char ***lines, unsigned int *count)
{
    *lines = NULL;
    *count = 0u;
    unsigned int ends = 0u;
    for (unsigned long long at = 0ULL; at < size; at += 1ULL)
    {
        ends += (text[at] == '\n') ? 1u : 0u;
    }
    const char **const cut =
        (const char **)platform_cell_cleared((size_t)ends + 1u, sizeof(char *), platform_place_g_program_owner, 0);
    if (cut == NULL)
    {
        return 0;
    }
    char *start = text;
    for (unsigned long long at = 0ULL; (at < size) && (*count < ends); at += 1ULL)
    {
        if (text[at] == '\n')
        {
            text[at] = '\0';
            cut[*count] = start;
            *count += 1u;
            start = &text[at + 1u];
        }
    }
    *lines = cut;
    return 1;
}

int kfile_lines_block_lines(const KfileLines *file, const char *lang, unsigned int from, unsigned int *first,
                            unsigned int *past)
{
    // the lines from `from` joined, each ended by a newline, and where each starts, for the block's places to be
    // turned back into lines
    static char s_text_bytes[QRY_WRITER_RING_BYTES / 2ULL];
    size_t joined = 0u;
    size_t *const starts =
        (size_t *)platform_cell_cleared((size_t)file->lines + 1u, sizeof(size_t), platform_place_g_program_owner, 0);
    if (starts == NULL)
    {
        return 0;
    }
    for (unsigned int at = from; at < file->lines; at += 1u)
    {
        starts[at] = joined;
        joined = verba_textus.put(
            &(VerbaTextusCfg){.out = s_text_bytes, .cap = sizeof(s_text_bytes), .at = joined, .text = file->line[at]});
        joined = verba_littera.ch(
            &(VerbaLitteraCfg){.out = s_text_bytes, .cap = sizeof(s_text_bytes), .at = joined, .ch = '\n'});
    }
    KlangBlock block;
    const int found = verba_finis.ok(&(VerbaFinisCfg){.cap = sizeof(s_text_bytes), .at = joined}) &&
                      klang_block_find(s_text_bytes, joined, 0u, lang, &block);
    // the line containing the opener and the line containing the closer: the last to start at or before each, the
    // starts rising line by line
    unsigned int opener = file->lines;
    unsigned int closer = file->lines;
    for (unsigned int at = from; found && (at < file->lines); at += 1u)
    {
        opener = (starts[at] <= block.opened) ? at : opener;
        closer = (starts[at] <= block.text_end) ? at : closer;
    }
    platform_cell_released(starts, platform_place_g_program_owner);
    if (!found || (opener == file->lines) || (closer == file->lines))
    {
        return 0;
    }
    *first = opener + 1u;
    *past = (closer > opener) ? closer : *first;
    return 1;
}

// a line the file contained that is no who line: its text and its place among such lines
typedef struct
{
    const char *text;
    unsigned int at;
} KfileLinesEntry;

// the order of two contained lines, by their text and then by their place
static int kfile_lines_entry_order(const void *left, const void *right)
{
    const KfileLinesEntry *const one = (const KfileLinesEntry *)left;
    const KfileLinesEntry *const other = (const KfileLinesEntry *)right;
    const int order = kfile_lines_text_order(one->text, other->text, PLATFORM_TEXT_TO_TERMINATOR);
    return (order != 0) ? order : ((one->at < other->at) ? -1 : ((one->at > other->at) ? 1 : 0));
}

// 1 where the two are one who: the same code, in the same program, at the same time
static int kfile_lines_who_equal(Who one, Who other)
{
    return (one.source == other.source) && (one.line == other.line) && (one.program == other.program) &&
           (one.time == other.time);
}

int kfile_lines_written_by(KfileLines *file, unsigned int first, const char *const *lines, unsigned int count, Who who)
{
    // each line the file contains that is no who line, in the order of its text, and the who in force over it
    KfileLinesEntry *const entries = (KfileLinesEntry *)platform_cell_cleared(
        (size_t)file->lines + 1u, sizeof(KfileLinesEntry), platform_place_g_program_owner, 0);
    Who *const whose =
        (Who *)platform_cell_cleared((size_t)file->lines + 1u, sizeof(Who), platform_place_g_program_owner, 0);
    unsigned char *const whose_has_who =
        (unsigned char *)platform_cell_cleared((size_t)file->lines + 1u, 1u, platform_place_g_program_owner, 0);
    unsigned char *const taken =
        (unsigned char *)platform_cell_cleared((size_t)file->lines + 1u, 1u, platform_place_g_program_owner, 0);
    int written = (entries != NULL) && (whose != NULL) && (whose_has_who != NULL) && (taken != NULL);
    unsigned int entry_count = 0u;
    Who now = {0u, 0u, 0u, 0ULL};
    int now_has_who = 0;
    for (unsigned int at = 0u; written && (at < file->lines); at += 1u)
    {
        Who read;
        int read_has_who = 0;
        if (kfile_lines_who_line(file->line[at], &read, &read_has_who))
        {
            now = read;
            now_has_who = read_has_who;
            continue;
        }
        entries[entry_count].text = file->line[at];
        entries[entry_count].at = entry_count;
        whose[entry_count] = now;
        whose_has_who[entry_count] = (unsigned char)now_has_who;
        entry_count += 1u;
    }
    if (written)
    {
        // the room of the sort, on the temporary tier, which goes back as the sort ends
        void *const sort_room =
            platform_cell_taken((unsigned long long)(entry_count) * (unsigned long long)(sizeof(KfileLinesEntry)),
                                platform_place_g_program_owner, 1);
        (void)EMBED_CALL(memor.sort, MemoriaCfg, .dst = entries, .bytes = sizeof(KfileLinesEntry), .scratch = sort_room,
                         .count = entry_count, .order = kfile_lines_entry_order);
        platform_cell_released(sort_room, platform_place_g_program_owner);
    }
    KfileLines made = {NULL, 0u, 0u};
    Who in_force = {0u, 0u, 0u, 0ULL};
    int in_force_has_who = 0;
    for (unsigned int line = 0u; written && (line < count); line += 1u)
    {
        // the first line the file contained of this text and not yet taken: the first of the text, by its place
        unsigned int low = 0u;
        unsigned int high = entry_count;
        while (low < high)
        {
            const unsigned int middle = low + ((high - low) / 2u);
            low = (kfile_lines_text_order(entries[middle].text, lines[line], PLATFORM_TEXT_TO_TERMINATOR) < 0)
                      ? (middle + 1u)
                      : low;
            high = (kfile_lines_text_order(entries[middle].text, lines[line], PLATFORM_TEXT_TO_TERMINATOR) < 0)
                       ? high
                       : middle;
        }
        while ((low < entry_count) &&
               (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = entries[low].text, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                           .other = lines[line], .ci = EMBED_FALSE)) &&
               taken[entries[low].at])
        {
            low += 1u;
        }
        const int found = (low < entry_count) &&
                          (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = entries[low].text,
                                      .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = lines[line], .ci = EMBED_FALSE));
        if (found)
        {
            taken[entries[low].at] = 1u;
        }
        const Who wanted = found ? whose[entries[low].at] : who;
        const int wanted_has_who = found ? (int)whose_has_who[entries[low].at] : 1;
        const int same =
            (wanted_has_who == in_force_has_who) && (!wanted_has_who || kfile_lines_who_equal(wanted, in_force));
        if ((line >= first) && !same)
        {
            char whose_line[WHO_CODE_CHARACTERS + 8u];
            kfile_lines_who_written(wanted, wanted_has_who, whose_line, sizeof(whose_line));
            written = kfile_lines_line_add(&made, whose_line);
            in_force = wanted;
            in_force_has_who = wanted_has_who;
        }
        written = written && kfile_lines_line_add(&made, lines[line]);
    }
    platform_cell_released(taken, platform_place_g_program_owner);
    platform_cell_released(whose_has_who, platform_place_g_program_owner);
    platform_cell_released(whose, platform_place_g_program_owner);
    platform_cell_released(entries, platform_place_g_program_owner);
    if (!written)
    {
        kfile_lines_free(&made);
        return 0;
    }
    kfile_lines_free(file);
    *file = made;
    return 1;
}

void kfile_lines_who_tidied(KfileLines *file)
{
    Who who;
    int has_who = 0;
    unsigned int at = file->lines;
    while (at != 0u)
    {
        at -= 1u;
        const int over_none = ((at + 1u) >= file->lines) || kfile_lines_who_line(file->line[at + 1u], &who, &has_who);
        if (kfile_lines_who_line(file->line[at], &who, &has_who) && over_none)
        {
            kfile_lines_line_drop(file, at);
        }
    }
}
