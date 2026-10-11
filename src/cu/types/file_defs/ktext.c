// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ktext.c: a k-file people read and write, its lines read, put by a who, and written back whole (ktext.h)
#include "ktext.h"

#include "klang.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// `length` letters at `text` copied into an allocation of their own, ended: NULL where there was no memory
static char *ktext_copy(const char *text, size_t length)
{
    char *const copy = (char *)malloc(length + 1u);
    if (copy != NULL)
    {
        memcpy(copy, text, length);
        copy[length] = '\0';
    }
    return copy;
}

// `file` given room for `more` lines past those it holds: 1, or 0 where there was no memory
static int ktext_room(KtextFile *file, unsigned int more)
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
    char **const larger = (char **)realloc(file->line, (size_t)grown * sizeof(char *));
    if (larger == NULL)
    {
        return 0;
    }
    file->line = larger;
    file->room = grown;
    return 1;
}

int ktext_read(KtextFile *file, const char *path)
{
    ktext_free(file);
    // the file as the run holds it: its latest blob, or the file where the run holds none (qry_file_read)
    unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    if (!qry_file_read(qry_run(), path, &bytes, &size))
    {
        return 1;
    }
    int held = 1;
    unsigned long long at = 0ull;
    while (held && (at < size))
    {
        // a file read whole is at most what a size_t counts
        const char *const start = (const char *)(bytes + at);
        const char *const end = (const char *)memchr(start, '\n', (size_t)(size - at));
        size_t length = (end != NULL) ? (size_t)(end - start) : (size_t)(size - at);
        at += (unsigned long long)length + ((end != NULL) ? 1ull : 0ull);
        length -= ((length != 0u) && (start[length - 1u] == '\r')) ? 1u : 0u;
        char *const copy = ktext_copy(start, length);
        held = (copy != NULL) && ktext_room(file, 1u);
        if (held)
        {
            file->line[file->lines] = copy;
            file->lines += 1u;
        }
        else
        {
            free(copy);
        }
    }
    free(bytes);
    return held;
}

int ktext_write_by(const KtextFile *file, const char *path, QryWho who)
{
    // the file put together in memory and handed whole to the run's writer, which writes it in the file's place
    QryText text = {0};
    for (unsigned int at = 0u; at < file->lines; at += 1u)
    {
        qry_text_added(&text, file->line[at], strlen(file->line[at]));
        qry_text_added(&text, "\n", 1u);
    }
    return qry_text_filed_by(qry_run(), path, &text, who) != 0ull;
}

void ktext_free(KtextFile *file)
{
    for (unsigned int at = 0u; at < file->lines; at += 1u)
    {
        free(file->line[at]);
    }
    free(file->line);
    memset(file, 0, sizeof(*file));
}

int ktext_line_add(KtextFile *file, const char *text)
{
    char *const copy = ktext_copy(text, strlen(text));
    if ((copy == NULL) || !ktext_room(file, 1u))
    {
        free(copy);
        return 0;
    }
    file->line[file->lines] = copy;
    file->lines += 1u;
    return 1;
}

int ktext_line_replaced(KtextFile *file, unsigned int at, const char *text)
{
    char *const copy = (at < file->lines) ? ktext_copy(text, strlen(text)) : NULL;
    if (copy == NULL)
    {
        return 0;
    }
    free(file->line[at]);
    file->line[at] = copy;
    return 1;
}

void ktext_line_drop(KtextFile *file, unsigned int at)
{
    if (at >= file->lines)
    {
        return;
    }
    free(file->line[at]);
    memmove(&file->line[at], &file->line[at + 1u], (size_t)(file->lines - at - 1u) * sizeof(char *));
    file->lines -= 1u;
}

int ktext_who_line(const char *line, QryWho *who, int *held)
{
    if (strncmp(line, "who ", 4u) != 0)
    {
        return 0;
    }
    if (strcmp(line + 4, "-") == 0)
    {
        *held = 0;
        return 1;
    }
    *held = qry_who_read(line + 4, who) && (line[4u + QRY_WHO_CODE_LETTERS] == '\0');
    return *held;
}

int ktext_who_at(const KtextFile *file, unsigned int at, QryWho *who)
{
    for (unsigned int before = (at < file->lines) ? at : file->lines; before != 0u; before -= 1u)
    {
        int held = 0;
        if (ktext_who_line(file->line[before - 1u], who, &held))
        {
            return held;
        }
    }
    return 0;
}

// the who line of `who` into `line`, which holds QRY_WHO_CODE_LETTERS + 5, `who -` where `held` is 0
static void ktext_who_written(QryWho who, int held, char *line, size_t room)
{
    char code[QRY_WHO_CODE_LETTERS + 1u];
    qry_who_code(who, code, sizeof(code));
    snprintf(line, room, "who %s", held ? code : "-");
}

unsigned int ktext_lines_put_by(KtextFile *file, unsigned int at, const char *const *lines, unsigned int count,
                                QryWho who)
{
    at = (at < file->lines) ? at : file->lines;
    QryWho there;
    const int there_held = ktext_who_at(file, at, &there);
    const int own =
        there_held && (there.source == who.source) && (there.line == who.line) && (there.program == who.program);
    // lines of the file follow where `at` stands before one that is no who line of its own
    QryWho next_who;
    int next_held = 0;
    const int follows = (at < file->lines) && !ktext_who_line(file->line[at], &next_who, &next_held);
    // the lines put, each its own allocation, made before the file is touched
    const unsigned int put = count + (own ? 0u : 1u) + ((!own && follows) ? 1u : 0u);
    char **const made = (char **)calloc((size_t)put + 1u, sizeof(char *));
    if ((made == NULL) || !ktext_room(file, put))
    {
        free(made);
        return 0u;
    }
    char whose[QRY_WHO_CODE_LETTERS + 8u];
    unsigned int next = 0u;
    int held = 1;
    if (!own)
    {
        ktext_who_written(who, 1, whose, sizeof(whose));
        made[next] = ktext_copy(whose, strlen(whose));
        held = held && (made[next] != NULL);
        next += 1u;
    }
    for (unsigned int line = 0u; line < count; line += 1u)
    {
        made[next] = ktext_copy(lines[line], strlen(lines[line]));
        held = held && (made[next] != NULL);
        next += 1u;
    }
    if (!own && follows)
    {
        ktext_who_written(there, there_held, whose, sizeof(whose));
        made[next] = ktext_copy(whose, strlen(whose));
        held = held && (made[next] != NULL);
        next += 1u;
    }
    if (!held)
    {
        for (unsigned int line = 0u; line < next; line += 1u)
        {
            free(made[line]);
        }
        free(made);
        return 0u;
    }
    memmove(&file->line[at + put], &file->line[at], (size_t)(file->lines - at) * sizeof(char *));
    memcpy(&file->line[at], made, (size_t)put * sizeof(char *));
    file->lines += put;
    free(made);
    return put;
}

int ktext_lines_cut(QryText *text, const char ***lines, unsigned int *count)
{
    *lines = NULL;
    *count = 0u;
    qry_text_added(text, "", 1u);
    if (text->failed)
    {
        return 0;
    }
    unsigned int ends = 0u;
    for (unsigned long long at = 0ull; at < text->size; at += 1ull)
    {
        ends += (text->bytes[at] == '\n') ? 1u : 0u;
    }
    const char **const cut = (const char **)calloc((size_t)ends + 1u, sizeof(char *));
    if (cut == NULL)
    {
        return 0;
    }
    char *start = text->bytes;
    for (unsigned long long at = 0ull; (at < text->size) && (*count < ends); at += 1ull)
    {
        if (text->bytes[at] == '\n')
        {
            text->bytes[at] = '\0';
            cut[*count] = start;
            *count += 1u;
            start = &text->bytes[at + 1u];
        }
    }
    *lines = cut;
    return 1;
}

int ktext_block_lines(const KtextFile *file, const char *lang, unsigned int from, unsigned int *first,
                      unsigned int *past)
{
    // the lines from `from` joined, each ended by a newline, and where each starts, for the block's places to be
    // turned back into lines
    QryText text = {0};
    size_t *const starts = (size_t *)calloc((size_t)file->lines + 1u, sizeof(size_t));
    if (starts == NULL)
    {
        return 0;
    }
    for (unsigned int at = from; at < file->lines; at += 1u)
    {
        // a line's start is the bytes joined before it, which a size_t counts
        starts[at] = (size_t)text.size;
        qry_text_printed(&text, "%s\n", file->line[at]);
    }
    KlangBlock block;
    // the joined lines are at most what a size_t counts
    const int found =
        !text.failed && (text.bytes != NULL) && klang_block_find(text.bytes, (size_t)text.size, 0u, lang, &block);
    // the line holding the opener and the line holding the closer: the last to start at or before each, the starts
    // rising line by line
    unsigned int opener = file->lines;
    unsigned int closer = file->lines;
    for (unsigned int at = from; found && (at < file->lines); at += 1u)
    {
        opener = (starts[at] <= block.opened) ? at : opener;
        closer = (starts[at] <= block.text_end) ? at : closer;
    }
    qry_text_freed(&text);
    free(starts);
    if (!found || (opener == file->lines) || (closer == file->lines))
    {
        return 0;
    }
    *first = opener + 1u;
    *past = (closer > opener) ? closer : *first;
    return 1;
}

// a line the file held that is no who line: its text and its place among such lines
typedef struct
{
    const char *text;
    unsigned int at;
} KtextHeld;

// the order of two held lines, by their text and then by their place
static int ktext_held_order(const void *left, const void *right)
{
    const KtextHeld *const one = (const KtextHeld *)left;
    const KtextHeld *const other = (const KtextHeld *)right;
    const int order = strcmp(one->text, other->text);
    return (order != 0) ? order : ((one->at < other->at) ? -1 : ((one->at > other->at) ? 1 : 0));
}

// 1 where the two are one who: the same code, in the same program, at the same time
static int ktext_who_equal(QryWho one, QryWho other)
{
    return (one.source == other.source) && (one.line == other.line) && (one.program == other.program) &&
           (one.nanoseconds == other.nanoseconds);
}

int ktext_lines_written_by(KtextFile *file, unsigned int first, const char *const *lines, unsigned int count,
                           QryWho who)
{
    // each line the file holds that is no who line, in the order of its text, and the who in force over it
    KtextHeld *const held = (KtextHeld *)calloc((size_t)file->lines + 1u, sizeof(KtextHeld));
    QryWho *const whose = (QryWho *)calloc((size_t)file->lines + 1u, sizeof(QryWho));
    unsigned char *const whose_held = (unsigned char *)calloc((size_t)file->lines + 1u, 1u);
    unsigned char *const taken = (unsigned char *)calloc((size_t)file->lines + 1u, 1u);
    int written = (held != NULL) && (whose != NULL) && (whose_held != NULL) && (taken != NULL);
    unsigned int kept = 0u;
    QryWho now = {0u, 0u, 0u, 0ull};
    int now_held = 0;
    for (unsigned int at = 0u; written && (at < file->lines); at += 1u)
    {
        QryWho read;
        int read_held = 0;
        if (ktext_who_line(file->line[at], &read, &read_held))
        {
            now = read;
            now_held = read_held;
            continue;
        }
        held[kept].text = file->line[at];
        held[kept].at = kept;
        whose[kept] = now;
        whose_held[kept] = (unsigned char)now_held;
        kept += 1u;
    }
    if (written)
    {
        qsort(held, kept, sizeof(KtextHeld), ktext_held_order);
    }
    KtextFile made = {NULL, 0u, 0u};
    QryWho in_force = {0u, 0u, 0u, 0ull};
    int in_force_held = 0;
    for (unsigned int line = 0u; written && (line < count); line += 1u)
    {
        // the first line the file held of this text and not yet taken: the first of the text, by its place
        unsigned int low = 0u;
        unsigned int high = kept;
        while (low < high)
        {
            const unsigned int middle = low + ((high - low) / 2u);
            low = (strcmp(held[middle].text, lines[line]) < 0) ? (middle + 1u) : low;
            high = (strcmp(held[middle].text, lines[line]) < 0) ? high : middle;
        }
        while ((low < kept) && (strcmp(held[low].text, lines[line]) == 0) && taken[held[low].at])
        {
            low += 1u;
        }
        const int found = (low < kept) && (strcmp(held[low].text, lines[line]) == 0);
        if (found)
        {
            taken[held[low].at] = 1u;
        }
        const QryWho wanted = found ? whose[held[low].at] : who;
        const int wanted_held = found ? (int)whose_held[held[low].at] : 1;
        const int same = (wanted_held == in_force_held) && (!wanted_held || ktext_who_equal(wanted, in_force));
        if ((line >= first) && !same)
        {
            char whose_line[QRY_WHO_CODE_LETTERS + 8u];
            ktext_who_written(wanted, wanted_held, whose_line, sizeof(whose_line));
            written = ktext_line_add(&made, whose_line);
            in_force = wanted;
            in_force_held = wanted_held;
        }
        written = written && ktext_line_add(&made, lines[line]);
    }
    free(taken);
    free(whose_held);
    free(whose);
    free(held);
    if (!written)
    {
        ktext_free(&made);
        return 0;
    }
    ktext_free(file);
    *file = made;
    return 1;
}

void ktext_who_tidied(KtextFile *file)
{
    QryWho who;
    int held = 0;
    unsigned int at = file->lines;
    while (at != 0u)
    {
        at -= 1u;
        const int over_none = ((at + 1u) >= file->lines) || ktext_who_line(file->line[at + 1u], &who, &held);
        if (ktext_who_line(file->line[at], &who, &held) && over_none)
        {
            ktext_line_drop(file, at);
        }
    }
}
