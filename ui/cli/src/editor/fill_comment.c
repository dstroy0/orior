// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// fill_comment.c: a `/** */` comment written again whole. Its lines are read without their stars, the opening and the
// closing lines' text kept where they hold any; split into paragraphs at blank lines and at each tag; each paragraph's
// words filled under ` * `, and each run of tags of one kind lined up: the tags, their types in braces, the names of
// those that name one, and their descriptions each in one column, a description that runs past the margin going on
// under its own first word. A run's tags are joined from their lines into a room of CLI_FILL_JOINED bytes, those past
// it left out of the run
#include "../command_line/cells.h"
#include "fill_internal.h"

// the most parts of a comment, and the bytes its tags take joined
#define CLI_FILL_PARTS (1u << 18u)
#define CLI_FILL_JOINED (1u << 22u)

// the tags whose first word after a type is a name
static const char *const s_named_tags[7] = {"param", "arg", "argument", "property", "prop", "template", "typedef"};

// A tag of a run: the tag, its type in braces, its name, and its description
typedef struct
{
    const char *tag;
    unsigned long long tag_size;
    const char *kind;
    unsigned long long kind_size;
    const char *name;
    unsigned long long name_size;
    const char *description;
    unsigned long long description_size;
} CliFillTag;

// A fill's room, a cell taken under the fill's lock: each part's first line among the comment's lines and its count,
// 0 for a blank line between parts; each part's tag; the tags' texts joined; and a tag line's head, its hanging lead
// and the comment's lead
typedef struct
{
    unsigned int part_first[CLI_FILL_PARTS];
    unsigned int part_lines[CLI_FILL_PARTS];
    CliFillTag tag[CLI_FILL_PARTS];
    char joined[CLI_FILL_JOINED];
    char head[1u << 16u];
    char hang[1u << 16u];
    char lead[1u << 16u];
} CliFillRoom;

static CliFillRoom *s_fill;

int cli_fill_comment_held(CliLine *said)
{
    s_fill = cli_cell_taken(sizeof(*s_fill), "a comment's fill");
    if (s_fill == NULL)
    {
        cli_line_text(said, "a comment's fill could not be taken");
    }
    return s_fill != NULL;
}

void cli_fill_comment_released(void)
{
    cli_cell_given(s_fill);
    s_fill = NULL;
}

// the start of a line's text, its whitespace taken off
static const char *comment_trimmed_start(const char *text, const CliFillLine *line, unsigned long long *size)
{
    const unsigned long long lead = cli_text_lead(text + line->start, line->end - line->start);
    *size = line->end - line->start - lead;
    return text + line->start + lead;
}

// the bounds of the comment around `at`, its opening line and its closing line: 1, or 0 where `at` is in none
static int comment_bounds(const char *text, unsigned long long size, const CliFillLine *at, CliFillLine *first,
                          CliFillLine *last)
{
    *first = *at;
    for (;;)
    {
        unsigned long long length = 0ull;
        const char *const line = comment_trimmed_start(text, first, &length);
        if (cli_text_starts(line, length, "/*"))
        {
            break;
        }
        if (!cli_text_starts(line, length, "*") ||
            (cli_text_starts(line, length, "*/") && (first->index != at->index)) || (first->index == 0u))
        {
            return 0;
        }
        (void)cli_fill_before(text, first);
    }
    *last = *at;
    for (;;)
    {
        const unsigned long long end = cli_text_trimmed_end(text + last->start, last->end - last->start);
        if (cli_text_ends(text + last->start, end, "*/"))
        {
            break;
        }
        if (!cli_fill_after(text, size, last))
        {
            return 0;
        }
        unsigned long long length = 0ull;
        const char *const line = comment_trimmed_start(text, last, &length);
        if (!cli_text_starts(line, length, "*"))
        {
            return 0;
        }
    }
    return first->index != last->index;
}

// a comment's line put among its lines where it is kept, one place left after them for a description: their count
// after
static unsigned int comment_body_put(unsigned int count, const char *line, unsigned long long size, int kept)
{
    if (kept && (count + 1u < g_cli_fill_slices))
    {
        g_cli_fill_slice[count] = line;
        g_cli_fill_slice_size[count] = size;
        return count + 1u;
    }
    return count;
}

// the comment's lines between `first` and `last`, each without its star, the opening and closing lines' text where
// they hold any: their count
static unsigned int comment_body(const char *text, unsigned long long size, const CliFillLine *first,
                                 const CliFillLine *last)
{
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    cli_text_trimmed(text + first->start, first->end - first->start, &from, &to);
    const char *open = text + first->start + from;
    unsigned long long open_size = to - from;
    while ((open_size > 0u) && (open[0] == '/'))
    {
        open += 1;
        open_size -= 1u;
    }
    while ((open_size > 0u) && (open[0] == '*'))
    {
        open += 1;
        open_size -= 1u;
    }
    cli_text_trimmed(open, open_size, &from, &to);
    unsigned int count = comment_body_put(0u, open + from, to - from, to > from);
    CliFillLine line = *first;
    while (cli_fill_after(text, size, &line) && (line.index < last->index))
    {
        unsigned long long length = 0ull;
        const char *body = comment_trimmed_start(text, &line, &length);
        while ((length > 0u) && (body[0] == '*'))
        {
            body += 1;
            length -= 1u;
        }
        cli_text_trimmed(body, length, &from, &to);
        count = comment_body_put(count, body + from, to - from, 1);
    }
    cli_text_trimmed(text + last->start, last->end - last->start, &from, &to);
    const char *close = text + last->start + from;
    unsigned long long close_size = to - from;
    while ((close_size > 0u) && (close[close_size - 1u] == '/'))
    {
        close_size -= 1u;
    }
    while ((close_size > 0u) && (close[close_size - 1u] == '*'))
    {
        close_size -= 1u;
    }
    while ((close_size > 0u) && (close[0] == '*'))
    {
        close += 1;
        close_size -= 1u;
    }
    cli_text_trimmed(close, close_size, &from, &to);
    return comment_body_put(count, close + from, to - from, to > from);
}

// the comment's lines split into parts: each blank line a part of no lines, each tag the start of a part, and each
// other line put on the part before it where that holds lines: their count
static unsigned int comment_parts(unsigned int lines)
{
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < lines; at += 1u)
    {
        const int blank = (g_cli_fill_slice_size[at] == 0u);
        const int tag = !blank && (g_cli_fill_slice[at][0] == '@');
        if (!blank && !tag && (count > 0u) && (s_fill->part_lines[count - 1u] > 0u))
        {
            s_fill->part_lines[count - 1u] += 1u;
        }
        else if (count < CLI_FILL_PARTS)
        {
            s_fill->part_first[count] = at;
            s_fill->part_lines[count] = blank ? 0u : 1u;
            count += 1u;
        }
    }
    return count;
}

// the kind of the tag a part of `size` bytes starts with: its tag, its first word, into `tag`, and its kind, the tag
// without its `@`, into `kind`
static void comment_tag_of(const char *line, unsigned long long size, unsigned long long *tag_size, const char **kind,
                           unsigned long long *kind_size)
{
    unsigned long long walk = 0ull;
    unsigned long long word = 0ull;
    *tag_size = 0ull;
    (void)cli_text_word(line, size, &walk, &word, tag_size);
    *kind = line;
    *kind_size = *tag_size;
    while ((*kind_size > 0u) && ((*kind)[0] == '@'))
    {
        *kind += 1;
        *kind_size -= 1u;
    }
}

// a tag of `size` bytes split into its parts
static void comment_split_tag(const char *line, unsigned long long size, CliFillTag *tag)
{
    const char *kind = NULL;
    unsigned long long kind_size = 0ull;
    comment_tag_of(line, size, &tag->tag_size, &kind, &kind_size);
    tag->tag = line;
    const char *rest = line + tag->tag_size;
    unsigned long long rest_size = size - tag->tag_size;
    unsigned long long skipped = cli_text_lead(rest, rest_size);
    rest += skipped;
    rest_size -= skipped;
    tag->kind = rest;
    tag->kind_size = 0ull;
    if ((rest_size > 0u) && (rest[0] == '{'))
    {
        long long depth = 0;
        unsigned long long end = 0ull;
        for (unsigned long long at = 0ull; at < rest_size; at += 1u)
        {
            depth += (rest[at] == '{') ? 1 : (rest[at] == '}') ? -1 : 0;
            if ((rest[at] == '}') && (depth == 0))
            {
                end = at + 1u;
                break;
            }
        }
        if (end > 0u)
        {
            tag->kind_size = end;
            rest += end;
            rest_size -= end;
            skipped = cli_text_lead(rest, rest_size);
            rest += skipped;
            rest_size -= skipped;
        }
    }
    tag->name = rest;
    tag->name_size = 0ull;
    for (unsigned int at = 0u; at < 7u; at += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = s_named_tags[at], .cap = CLI_CELLS_POOL}) == kind_size) &&
            (memor.cmp(&(MemoriaCfg){.src = s_named_tags[at], .other = kind, .bytes = (size_t)kind_size}) == 0))
        {
            unsigned long long walk = 0ull;
            unsigned long long word = 0ull;
            (void)cli_text_word(rest, rest_size, &walk, &word, &tag->name_size);
            rest += tag->name_size;
            rest_size -= tag->name_size;
            skipped = cli_text_lead(rest, rest_size);
            rest += skipped;
            rest_size -= skipped;
            break;
        }
    }
    while ((rest_size > 0u) && (rest[0] == '-'))
    {
        rest += 1;
        rest_size -= 1u;
    }
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    cli_text_trimmed(rest, rest_size, &from, &to);
    tag->description = rest + from;
    tag->description_size = to - from;
}

// `size` bytes of `text` written padded with spaces to `width` letters
static void comment_padded(CliLine *out, const char *text, unsigned long long size, unsigned long long width)
{
    cli_line_counted(out, text, size);
    for (unsigned long long letters = cli_text_letters(text, size); letters < width; letters += 1u)
    {
        cli_line_character(out, ' ');
    }
}

// the part at `part` of the comment's lines joined by spaces into the room from `*used`: its start and size
static const char *comment_joined(unsigned int part, unsigned long long *used, unsigned long long *size)
{
    char *const start = s_fill->joined + *used;
    CliLine line = {start, CLI_FILL_JOINED - *used, 0ull};
    for (unsigned int at = 0u; at < s_fill->part_lines[part]; at += 1u)
    {
        const unsigned int index = s_fill->part_first[part] + at;
        cli_line_text(&line, (at > 0u) ? " " : "");
        cli_line_counted(&line, g_cli_fill_slice[index], g_cli_fill_slice_size[index]);
    }
    if (!cli_line_fits(&line))
    {
        return NULL;
    }
    *used += line.at;
    *size = line.at;
    return start;
}

// the run of tags of one kind from the part at `*part`, lined up and written, `*part` past it
static void comment_run(CliFilled *filled, unsigned int *part, unsigned int parts, const char *lead,
                        unsigned long long lead_size, unsigned int width, unsigned int lines)
{
    const char *kind = NULL;
    unsigned long long kind_size = 0ull;
    unsigned long long tag_size = 0ull;
    const unsigned int head_line = s_fill->part_first[*part];
    comment_tag_of(g_cli_fill_slice[head_line], g_cli_fill_slice_size[head_line], &tag_size, &kind, &kind_size);
    unsigned int count = 0u;
    unsigned long long used = 0ull;
    unsigned long long widths[3] = {0ull, 0ull, 0ull};
    while ((*part < parts) && (s_fill->part_lines[*part] > 0u) &&
           (g_cli_fill_slice[s_fill->part_first[*part]][0] == '@'))
    {
        const char *other = NULL;
        unsigned long long other_size = 0ull;
        const unsigned int line = s_fill->part_first[*part];
        comment_tag_of(g_cli_fill_slice[line], g_cli_fill_slice_size[line], &tag_size, &other, &other_size);
        if ((other_size != kind_size) ||
            (memor.cmp(&(MemoriaCfg){.src = other, .other = kind, .bytes = (size_t)kind_size}) != 0))
        {
            break;
        }
        unsigned long long size = 0ull;
        const char *const joined = comment_joined(*part, &used, &size);
        *part += 1u;
        if (joined == NULL)
        {
            continue;
        }
        CliFillTag *const tag = &s_fill->tag[count];
        comment_split_tag(joined, size, tag);
        const unsigned long long letters[3] = {cli_text_letters(tag->tag, tag->tag_size),
                                               cli_text_letters(tag->kind, tag->kind_size),
                                               cli_text_letters(tag->name, tag->name_size)};
        for (unsigned int at = 0u; at < 3u; at += 1u)
        {
            widths[at] = (letters[at] > widths[at]) ? letters[at] : widths[at];
        }
        count += 1u;
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const CliFillTag *const tag = &s_fill->tag[at];
        CliLine head = {s_fill->head, sizeof(s_fill->head), 0ull};
        cli_line_counted(&head, lead, lead_size);
        comment_padded(&head, tag->tag, tag->tag_size, widths[0]);
        if (widths[1] > 0u)
        {
            cli_line_character(&head, ' ');
            comment_padded(&head, tag->kind, tag->kind_size, widths[1]);
        }
        if (widths[2] > 0u)
        {
            cli_line_character(&head, ' ');
            comment_padded(&head, tag->name, tag->name_size, widths[2]);
        }
        if (tag->description_size == 0u)
        {
            cli_filled_line(filled, s_fill->head, cli_text_trimmed_end(s_fill->head, head.at));
            continue;
        }
        cli_line_character(&head, ' ');
        CliLine hang = {s_fill->hang, sizeof(s_fill->hang), 0ull};
        cli_line_counted(&hang, lead, lead_size);
        for (unsigned long long letters = cli_text_letters(lead, lead_size);
             letters < cli_text_letters(s_fill->head, head.at); letters += 1u)
        {
            cli_line_character(&hang, ' ');
        }
        g_cli_fill_slice[lines] = tag->description;
        g_cli_fill_slice_size[lines] = tag->description_size;
        cli_fill_words(filled, lines, 1u, s_fill->head, head.at, s_fill->hang, hang.at, width);
    }
}

int cli_fill_comment(const char *text, unsigned long long size, const CliFillLine *at, unsigned int width,
                     CliFilled *filled, CliTextEdit *edit)
{
    CliFillLine first = *at;
    CliFillLine last = *at;
    if (!comment_bounds(text, size, at, &first, &last))
    {
        return 0;
    }
    const char *const indent = text + first.start;
    const unsigned long long indent_size = cli_text_lead(indent, first.end - first.start);
    CliLine lead = {s_fill->lead, sizeof(s_fill->lead), 0ull};
    cli_line_counted(&lead, indent, indent_size);
    cli_line_text(&lead, " * ");
    const unsigned int lines = comment_body(text, size, &first, &last);
    const unsigned int parts = comment_parts(lines);
    cli_filled_line(filled, indent, indent_size);
    cli_line_text(filled->out, "/**");
    for (unsigned int part = 0u; part < parts;)
    {
        if (s_fill->part_lines[part] == 0u)
        {
            unsigned long long last_size = 0ull;
            const char *const written = cli_filled_last(filled, &last_size);
            unsigned long long from = 0ull;
            unsigned long long to = 0ull;
            cli_text_trimmed(written, last_size, &from, &to);
            const int star = (to - from == 1u) && (written[from] == '*');
            const int open = (to - from == 3u) &&
                             (memor.cmp(&(MemoriaCfg){.src = written + from, .other = "/**", .bytes = 3u}) == 0);
            if (!star && !open)
            {
                cli_filled_line(filled, indent, indent_size);
                cli_line_text(filled->out, " *");
            }
            part += 1u;
            continue;
        }
        if (g_cli_fill_slice[s_fill->part_first[part]][0] != '@')
        {
            cli_fill_words(filled, s_fill->part_first[part], s_fill->part_lines[part], s_fill->lead, lead.at,
                           s_fill->lead, lead.at, width);
            part += 1u;
            continue;
        }
        comment_run(filled, &part, parts, s_fill->lead, lead.at, width, lines);
    }
    for (;;)
    {
        unsigned long long last_size = 0ull;
        const char *const written = cli_filled_last(filled, &last_size);
        unsigned long long from = 0ull;
        unsigned long long to = 0ull;
        cli_text_trimmed(written, last_size, &from, &to);
        if ((to - from != 1u) || (written[from] != '*'))
        {
            break;
        }
        cli_filled_dropped(filled);
    }
    cli_filled_line(filled, indent, indent_size);
    cli_line_text(filled->out, " */");
    cli_fill_edit_of(text, &first, &last, filled, edit);
    return 1;
}
