// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// krf.c: a part's .krf read, its rows read, a removal kept, and written back whole (krf.h)
#include "krf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the longest head line, `krf <part>`
#define KRF_HEAD 256u

int krf_read(KrfFile *file, const char *path)
{
    return ktext_read(file, path);
}

int krf_write_by(const KrfFile *file, const char *path, QryWho who)
{
    return ktext_write_by(file, path, who);
}

void krf_free(KrfFile *file)
{
    ktext_free(file);
}

int krf_line_add(KrfFile *file, const char *text)
{
    return ktext_line_add(file, text);
}

// the word of `text` at `*at` into `word`, which holds `room`, the spaces before it skipped and `*at` moved past it: 1,
// or 0 where there is no word or it does not fit
static int krf_word_read(const char *text, size_t *at, char *word, size_t room)
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

int krf_row_read(const char *line, KrfRow *row)
{
    size_t at = 0u;
    char kind[KRF_TYPE];
    if (!krf_word_read(line, &at, kind, sizeof(kind)) || (strcmp(kind, "removed") != 0) ||
        !krf_word_read(line, &at, row->type, sizeof(row->type)) ||
        !krf_word_read(line, &at, row->query, sizeof(row->query)) ||
        !krf_word_read(line, &at, row->reason, sizeof(row->reason)))
    {
        return 0;
    }
    at += strspn(line + at, " \t");
    snprintf(row->row, sizeof(row->row), "%s", line + at);
    return 1;
}

// the name a path ends in, after its last separator, a slash written either way
static const char *krf_base_of(const char *path)
{
    const char *const slash = strrchr(path, '/');
    const char *const backslash = strrchr(path, '\\');
    const char *const separator = ((slash == NULL) || ((backslash != NULL) && (backslash > slash))) ? backslash : slash;
    return (separator == NULL) ? path : (separator + 1);
}

// how many letters of `base` stand before its last extension, every one where it has none
static size_t krf_stem_length(const char *base)
{
    const char *const dot = strrchr(base, '.');
    // a name is shorter than a size_t counts, and the dot stands at or past the name's start
    return (dot == NULL) ? strlen(base) : (size_t)(dot - base);
}

int krf_query_named(char *name, size_t room)
{
    const QryBuffer *const run = qry_run();
    const char *const path = (run == NULL) ? NULL : qry_path(run);
    if (path == NULL)
    {
        snprintf(name, room, "-");
        return 0;
    }
    const char *const base = krf_base_of(path);
    // the stem is shorter than an int counts
    snprintf(name, room, "%.*s", (int)krf_stem_length(base), base);
    return 1;
}

void krf_path_beside(const char *path, char *krf, size_t room)
{
    const char *const base = krf_base_of(path);
    // the path up to the base's last extension is shorter than an int counts
    const int stem = (int)((size_t)(base - path) + krf_stem_length(base));
    snprintf(krf, room, "%.*s.krf", stem, path);
}

int krf_removed_by(const char *path, const char *type, const char *reason, const char *const *rows,
                   unsigned int count, QryWho who)
{
    KrfFile file = {0};
    if (!ktext_read(&file, path))
    {
        return 0;
    }
    if (file.lines == 0u)
    {
        const char *const base = krf_base_of(path);
        char head[KRF_HEAD];
        // the part's name is shorter than an int counts
        snprintf(head, sizeof(head), "krf %.*s", (int)krf_stem_length(base), base);
        if (!ktext_line_add(&file, head))
        {
            ktext_free(&file);
            return 0;
        }
    }
    char query[KRF_QUERY];
    krf_query_named(query, sizeof(query));
    char **const lines = (count == 0u) ? NULL : calloc(count, sizeof(char *));
    int made = (count == 0u) || (lines != NULL);
    for (unsigned int at = 0u; made && (at < count); at += 1u)
    {
        const size_t length = strlen("removed ") + strlen(type) + strlen(query) + strlen(reason) + strlen(rows[at]) + 4u;
        lines[at] = malloc(length);
        made = lines[at] != NULL;
        if (made)
        {
            snprintf(lines[at], length, "removed %s %s %s %s", type, query, reason, rows[at]);
        }
    }
    const unsigned int put = made ? ktext_lines_put_by(&file, file.lines, (const char *const *)lines, count, who) : 0u;
    const int written = made && ((put != 0u) || (count == 0u)) && ktext_write_by(&file, path, who);
    for (unsigned int at = 0u; (lines != NULL) && (at < count); at += 1u)
    {
        free(lines[at]);
    }
    free(lines);
    ktext_free(&file);
    return written;
}
