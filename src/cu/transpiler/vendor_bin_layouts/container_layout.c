// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// container_layout.c: a container's layout read from its file, and looked up by name
#include "container_layout.h"
#include "../../../platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

// one tab-separated word of `line` from `at`, into `word`; the place the next word starts, or 0 at the end
static unsigned int layout_word(const char *line, unsigned int at, char *word, unsigned int room)
{
    // Explicit casts take the length of a word of a line, which an unsigned int counts
    at += (unsigned int)EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = &line[at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                   .other = "\t ");
    // the word up to its tab or its line end, cut to the room
    const unsigned int run = (unsigned int)EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &line[at],
                                                      .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "\t\n\r");
    const unsigned int copied = (run < (room - 1u)) ? run : (room - 1u);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = word, .src = &line[at], .bytes = copied);
    at += run;
    word[copied] = '\0';
    return (copied == 0u) ? 0u : at;
}

int container_layout_read(ContainerLayout *layout, const char *path)
{
    PlatformFile *const file = platform_file_open(path, "rb");
    if (file == NULL)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  container_layout: ");
            mmgr_line_put(output_line, path);
            mmgr_line_put(output_line, " could not be read");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0;
    }
    EMBED_CALL(memor.set, MemoriaCfg, .dst = layout, .bytes = sizeof(*layout), .val = 0);
    {
        VerbaLinea buffer_line = mmgr_line_over(layout->from, sizeof(layout->from));
        mmgr_line_put(&buffer_line, path);
        (void)mmgr_line_finish(&buffer_line);
    }
    char line[512];
    unsigned int refused = 0u;
    while (platform_file_text_read(line, sizeof(line), file) != NULL)
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
        if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = type, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "field",
                       .ci = EMBED_FALSE))
        {
            char width[LAYOUT_TEXT_MAX_LEN];
            row->type = LAYOUT_FIELD;
            row->offset = EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = value, .end = NULL, .base = 10);
            layout_word(line, at, width, sizeof(width));
            row->width =
                (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = width, .end = NULL, .base = 10);
        }
        else if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = type, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = "size", .ci = EMBED_FALSE)) ||
                 (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = type, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                             .other = "tag", .ci = EMBED_FALSE)))
        {
            row->type = (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = type, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                    .other = "size", .ci = EMBED_FALSE))
                            ? (unsigned int)LAYOUT_SIZE
                            : (unsigned int)LAYOUT_TAG;
            row->offset = EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = value, .end = NULL, .base = 10);
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = type, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "name", .ci = EMBED_FALSE))
        {
            row->type = LAYOUT_NAME;
            {
                VerbaLinea buffer_line = mmgr_line_over(row->text, sizeof(row->text));
                mmgr_line_put(&buffer_line, value);
                (void)mmgr_line_finish(&buffer_line);
            }
        }
        else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = type, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .other = "container", .ci = EMBED_FALSE))
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
    platform_file_close(file);
    if (refused != 0u)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  container_layout: ");
        mmgr_line_put(output_line, path);
        mmgr_line_put(output_line, " contains ");
        mmgr_line_decimal(output_line, refused, 1u);
        mmgr_line_put(output_line, " rows this does not read");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return (layout->rows != 0u);
}

static const LayoutRow *layout_row(const ContainerLayout *layout, const char *name, unsigned int type)
{
    for (unsigned int at = 0u; at < layout->rows; at += 1u)
    {
        if ((layout->row[at].type == type) &&
            (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = layout->row[at].name, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = name, .ci = EMBED_FALSE)))
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
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  container_layout: ");
            mmgr_line_put(output_line, layout->from);
            mmgr_line_put(output_line, " specifies nothing about ");
            mmgr_line_put(output_line, name);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
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
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  container_layout: ");
            mmgr_line_put(output_line, layout->from);
            mmgr_line_put(output_line, " specifies nothing about ");
            mmgr_line_put(output_line, name);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0;
    }
    // {part} is all a name is in for. What a part is called belongs to the program and not to the format
    const char *const is =
        EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = row->text, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                   .other = "{part}", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE);
    if (is == NULL)
    {
        {
            VerbaLinea buffer_line = mmgr_line_over(written, room);
            mmgr_line_put(&buffer_line, row->text);
            (void)mmgr_line_finish(&buffer_line);
        }
        return 1;
    }
    {
        VerbaLinea buffer_line = mmgr_line_over(written, room);
        mmgr_line_put_max(&buffer_line, row->text, (size_t)((int)(is - row->text)));
        mmgr_line_put(&buffer_line, (part != NULL) ? part : "");
        mmgr_line_put(&buffer_line, is + 6);
        (void)mmgr_line_finish(&buffer_line);
    }
    return 1;
}
