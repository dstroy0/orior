// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// container_layout.c: a container's layout read from its file, and looked up by name
#include "container_layout.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// one tab-separated word of `line` from `at`, into `word`; the place the next word starts, or 0 at the end
static unsigned int layout_word(const char *line, unsigned int at, char *word, unsigned int room)
{
    while ((line[at] == '\t') || (line[at] == ' '))
    {
        at += 1u;
    }
    unsigned int copied = 0u;
    while ((line[at] != '\0') && (line[at] != '\t') && (line[at] != '\n') && (line[at] != '\r'))
    {
        word[(copied < (room - 1u)) ? copied : (room - 1u)] = line[at];
        copied += (copied < (room - 1u)) ? 1u : 0u;
        at += 1u;
    }
    word[copied] = '\0';
    return (copied == 0u) ? 0u : at;
}

int container_layout_read(ContainerLayout *layout, const char *path)
{
    FILE *const file = fopen(path, "rb");
    if (file == NULL)
    {
        printf("  container_layout: %s could not be read\n", path);
        return 0;
    }
    memset(layout, 0, sizeof(*layout));
    snprintf(layout->from, sizeof(layout->from), "%s", path);
    char line[512];
    unsigned int refused = 0u;
    while (fgets(line, sizeof(line), file) != NULL)
    {
        if ((line[0] == '#') || (line[0] == '\n') || (line[0] == '\r'))
        {
            continue;
        }
        char type[LAYOUT_NAME_MAX_LEN];
        unsigned int at = layout_word(line, 0u, type, sizeof(type));
        if (at == 0u)
        {
            continue;
        }
        if (layout->rows == LAYOUT_ROWS)
        {
            refused += 1u;
            continue;
        }
        LayoutRow *const row = &layout->row[layout->rows];
        at = layout_word(line, at, row->name, sizeof(row->name));
        if (at == 0u)
        {
            continue;
        }
        char value[LAYOUT_TEXT_MAX_LEN];
        at = layout_word(line, at, value, sizeof(value));
        if (strcmp(type, "field") == 0)
        {
            char width[LAYOUT_TEXT_MAX_LEN];
            row->type = LAYOUT_FIELD;
            row->offset = strtoull(value, NULL, 10);
            layout_word(line, at, width, sizeof(width));
            row->width = (unsigned int)strtoul(width, NULL, 10);
        }
        else if ((strcmp(type, "size") == 0) || (strcmp(type, "tag") == 0))
        {
            row->type = (strcmp(type, "size") == 0) ? (unsigned int)LAYOUT_SIZE : (unsigned int)LAYOUT_TAG;
            row->offset = strtoull(value, NULL, 10);
        }
        else if (strcmp(type, "name") == 0)
        {
            row->type = LAYOUT_NAME;
            snprintf(row->text, sizeof(row->text), "%s", value);
        }
        else if (strcmp(type, "container") == 0)
        {
            // the container the system accepted, which the pattern's reader takes (cubin_pattern_read)
            continue;
        }
        else
        {
            refused += 1u;
            continue;
        }
        layout->rows += 1u;
    }
    fclose(file);
    if (refused != 0u)
    {
        printf("  container_layout: %s contains %u rows this does not read\n", path, refused);
    }
    return (layout->rows != 0u);
}

static const LayoutRow *layout_row(const ContainerLayout *layout, const char *name, unsigned int type)
{
    for (unsigned int at = 0u; at < layout->rows; at += 1u)
    {
        if ((layout->row[at].type == type) && (strcmp(layout->row[at].name, name) == 0))
        {
            return &layout->row[at];
        }
    }
    return NULL;
}

int layout_field(const ContainerLayout *layout, const char *name, unsigned long long *offset, unsigned int *width)
{
    const LayoutRow *const row = layout_row(layout, name, LAYOUT_FIELD);
    if (row == NULL)
    {
        printf("  container_layout: %s specifies nothing about %s\n", layout->from, name);
        return 0;
    }
    *offset = row->offset;
    *width = row->width;
    return 1;
}

unsigned long long layout_number(const ContainerLayout *layout, const char *name, unsigned long long otherwise)
{
    const LayoutRow *row = layout_row(layout, name, LAYOUT_SIZE);
    row = (row != NULL) ? row : layout_row(layout, name, LAYOUT_TAG);
    return (row != NULL) ? row->offset : otherwise;
}

int layout_text(const ContainerLayout *layout, const char *name, const char *part, char *written, unsigned int room)
{
    const LayoutRow *const row = layout_row(layout, name, LAYOUT_NAME);
    if (row == NULL)
    {
        printf("  container_layout: %s specifies nothing about %s\n", layout->from, name);
        return 0;
    }
    // {part} is all a name is in for. What a part is called belongs to the program and not to the format
    const char *const is = strstr(row->text, "{part}");
    if (is == NULL)
    {
        snprintf(written, room, "%s", row->text);
        return 1;
    }
    snprintf(written, room, "%.*s%s%s", (int)(is - row->text), row->text, (part != NULL) ? part : "",
             is + 6);
    return 1;
}
