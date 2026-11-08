// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// preview_markdown.c: a Markdown text as the preview's items. The text is split into lines once, into a cell for the
// layout alone, and its blocks are read as markdown.js reads them, a quote's or a list item's lines read again as a
// container of their own with their marks taken off. The layout runs twice: once counting the items, the blocks and the
// text's bytes, then again writing them into cells taken at those counts, so each cell is the size of what it carries.
// A block's inline text flows a word at a time: a word's letters wait with their marks until a space or the line's end,
// the word moves to the next line where it passes the width, and a run of letters of one mark on one line is one item
#include "preview_markdown.h"

// the sizes in the page's pixels: the body's text and its line, the code's text and its line, the code block's
// padding and its corners, the gap after a block and inside a list, the gap above a heading, a list item's indent,
// the quote's bar and its indent, a table cell's padding and its least width, a rule's thickness and the gap about it,
// a task's box, a bullet's disc, and the most columns of a table
#define MARKDOWN_BODY 14u
#define MARKDOWN_BODY_LINE 22
#define MARKDOWN_CODE 13u
#define MARKDOWN_CODE_LINE 19
#define MARKDOWN_CODE_PAD 10
#define MARKDOWN_CODE_RADIUS 6u
#define MARKDOWN_GAP 12
#define MARKDOWN_TIGHT 4
#define MARKDOWN_HEADING_ABOVE 10
#define MARKDOWN_INDENT 26
#define MARKDOWN_QUOTE_BAR 3
#define MARKDOWN_QUOTE_IN 16
#define MARKDOWN_CELL_PAD 8
#define MARKDOWN_CELL_LEAST 40
#define MARKDOWN_RULE 2
#define MARKDOWN_RULE_GAP 6
#define MARKDOWN_TASK 13
#define MARKDOWN_DISC 6
#define MARKDOWN_COLUMNS 64u
// the most bytes of a word laid at once, the most containers one inside another, a tab's stop in cells, the code
// face's advance in thousandths of an em, the mark a run of code carries among its flags, and no place
#define MARKDOWN_WORD 512u
#define MARKDOWN_DEPTH 16u
#define MARKDOWN_TAB 4u
#define MARKDOWN_CODE_ADVANCE 600u
#define MARKDOWN_CODE_FLAG 32u
#define MARKDOWN_NONE_AT 0xffffffffu

// the headings' text sizes, h1 to h6
static const unsigned int s_heading_size[6] = {26u, 21u, 18u, 16u, 14u, 13u};

// the interface face's advance of each printable ASCII letter from the space on, in thousandths of an em
static const unsigned short s_advance[95] = {
    274u, 270u, 380u, 600u, 560u, 840u, 820u, 220u, 300u, 300u, 440u, 680u, 220u, 400u, 260u, 400u, 560u, 560u, 560u,
    560u, 560u, 560u, 560u, 560u, 560u, 560u, 260u, 260u, 680u, 680u, 680u, 450u, 960u, 660u, 590u, 640u, 740u, 520u,
    500u, 720u, 750u, 280u, 380u, 600u, 480u, 920u, 780u, 790u, 570u, 790u, 610u, 550u, 550u, 730u, 650u, 980u, 620u,
    590u, 590u, 300u, 400u, 300u, 680u, 420u, 280u, 520u, 600u, 470u, 600u, 540u, 320u, 600u, 590u, 250u, 250u, 510u,
    250u, 900u, 590u, 600u, 600u, 600u, 360u, 430u, 350u, 590u, 500u, 760u, 490u, 500u, 450u, 300u, 260u, 300u, 680u};

// One line of a container: its bytes from `from` to before `to` in the text, its end left off, and the line of the
// source it is
typedef struct
{
    unsigned int from;
    unsigned int to;
    unsigned int line;
} MarkdownLine;

// A letter's marks: its flags, CLI_PREVIEW_* and the code mark, and the address of the link it stands in, none where
// `link_length` is 0
typedef struct
{
    unsigned int flags;
    unsigned int link;
    unsigned int link_length;
} MarkdownStyle;

// The flow of one block's inline text: the left and the width it flows in, the pen in thousandths of a pixel, the top
// of the line it stands on, the line's height, the text's size, its color role, the flags every run of it carries and
// the source line; the marks the letters now take; the run laid, whether one is open, where it starts, its text and its
// marks; the word waiting, its letters each with its marks; whether a space waits before the next word; and how many
// lines the flow has laid
typedef struct
{
    int x0;
    int width;
    long long pen;
    int y;
    int line_height;
    unsigned int size;
    unsigned int role;
    unsigned int base_flags;
    unsigned int line;
    MarkdownStyle style;
    int run_open;
    long long run_x;
    unsigned int run_text;
    unsigned int run_length;
    MarkdownStyle run_style;
    unsigned char word[MARKDOWN_WORD];
    MarkdownStyle word_style[MARKDOWN_WORD];
    unsigned int word_length;
    int space;
    int lines;
} MarkdownFlow;

// A link read from the text: its label's bytes, its address's bytes, and the byte after it
typedef struct
{
    unsigned int label_from;
    unsigned int label_to;
    unsigned int address_from;
    unsigned int address_to;
    unsigned int end;
} MarkdownLink;

// A list item's mark read from its line: its indent in columns, the columns its mark and the spaces after take, where
// its content starts, whether it is ordered, and its number
typedef struct
{
    unsigned int indent;
    unsigned int width;
    unsigned int content;
    int ordered;
    unsigned long long number;
} MarkdownItem;

typedef struct
{
    const char *text;
    CliPreviewItems *items;
    int writing;
    MarkdownFlow *flow;
    const MarkdownLine *lines;
    unsigned int count;
    const MarkdownLine *slice;
    unsigned int from;
    unsigned int to;
    unsigned int at;
    int x;
    int width;
    int *y;
    unsigned int depth;
    int tight;
    unsigned int role;
    unsigned int size;
    int line_height;
    unsigned int flags;
    unsigned int line;
    unsigned char letter;
    unsigned int length;
    const CliPreviewItem *item;
    const MarkdownStyle *style;
    const MarkdownStyle *other_style;
    MarkdownLink *link_out;
    MarkdownItem *item_out;
    unsigned int (*range_out)[2];
    const int *widths;
    int height;
    const char *bytes;
    int heading;
    int quoted;
} MarkdownCtx;

// the item `item` put after the items laid, the height every item fills grown to its foot: its row
EMBED_INLINE unsigned int item_put(const MarkdownCtx *args)
{
    CliPreviewItems *const items = args->items;
    const unsigned int at = items->items;
    if (args->writing)
    {
        items->item[at] = *args->item;
    }
    items->items += 1u;
    const int foot = args->item->y + args->item->height;
    items->height = (foot > items->height) ? foot : items->height;
    return at;
}

// the item at the row `at` stretched to `height`, its foot growing the height every item fills
EMBED_INLINE void item_height_put(const MarkdownCtx *args)
{
    CliPreviewItems *const items = args->items;
    if (args->writing)
    {
        items->item[args->at].height = args->height;
        const int foot = items->item[args->at].y + args->height;
        items->height = (foot > items->height) ? foot : items->height;
    }
}

// `length` bytes of `bytes` put after the text laid: where they start in the text cell
EMBED_INLINE unsigned int text_put(const MarkdownCtx *args)
{
    CliPreviewItems *const items = args->items;
    const unsigned int at = items->text_bytes;
    if (args->writing && (args->length != 0u))
    {
        memor.cpy(&(MemoriaCfg){.dst = items->text + at, .src = args->bytes, .bytes = args->length});
    }
    items->text_bytes += args->length;
    return at;
}

// a block starting on the source line `line` at the top `*y`, `depth` deep, a heading where `heading`, `width` wide
// from `x`: the block put after the blocks laid and its rectangle's item after the items, its height put once the
// block is laid: the item's row
EMBED_INLINE unsigned int block_put(const MarkdownCtx *args)
{
    CliPreviewItems *const items = args->items;
    if (args->writing)
    {
        items->block[items->blocks] = (CliPreviewBlock){args->line, *args->y, items->items, args->depth, args->heading};
    }
    items->blocks += 1u;
    return EMBED_CALL(item_put, MarkdownCtx, .items = items, .writing = args->writing,
                      .item = &(CliPreviewItem){.type = CLI_PREVIEW_ITEM_BLOCK,
                                                .x = args->x,
                                                .y = *args->y,
                                                .width = args->width,
                                                .role = CLI_PREVIEW_ROLE_HERE,
                                                .stroke = CLI_PREVIEW_ROLE_NONE,
                                                .line = args->line});
}

// the advance of the byte `letter` with the flags `flags`, in thousandths of an em: a code letter the code face's, a
// UTF-8 letter's first byte a wide letter's and its other bytes none, a bold letter wider and an italic one narrower
EMBED_INLINE unsigned int advance_of(const MarkdownCtx *args)
{
    const unsigned char letter = args->letter;
    if (letter >= 0x80u)
    {
        return (letter >= 0xC0u) ? MARKDOWN_CODE_ADVANCE : 0u;
    }
    if (letter < 32u)
    {
        return 0u;
    }
    if ((args->flags & MARKDOWN_CODE_FLAG) != 0u)
    {
        return MARKDOWN_CODE_ADVANCE;
    }
    const unsigned int plain = s_advance[letter - 32u];
    if ((args->flags & CLI_PREVIEW_BOLD) != 0u)
    {
        return (plain * 106u) / 100u;
    }
    return ((args->flags & CLI_PREVIEW_ITALIC) != 0u) ? (plain * 98u) / 100u : plain;
}

// whether the marks `style` and `other_style` are the same
EMBED_INLINE int style_same(const MarkdownCtx *args)
{
    return (args->style->flags == args->other_style->flags) && (args->style->link == args->other_style->link) &&
           (args->style->link_length == args->other_style->link_length);
}

// the flow's open run put as an item, a run of code on its ground first, and no run open
EMBED_INLINE void run_commit(const MarkdownCtx *args)
{
    MarkdownFlow *const flow = args->flow;
    if (!flow->run_open)
    {
        return;
    }
    flow->run_open = 0;
    if (flow->run_length == 0u)
    {
        return;
    }
    // a run's ends are thousandths of a pixel of a line, far inside what an int represents once in pixels
    const int x = (int)(flow->run_x / 1000LL);
    const int width = (int)((flow->pen - flow->run_x + 999LL) / 1000LL);
    const int code = (flow->run_style.flags & MARKDOWN_CODE_FLAG) != 0u;
    if (code)
    {
        (void)EMBED_CALL(item_put, MarkdownCtx, .items = args->items, .writing = args->writing,
                         .item = &(CliPreviewItem){.type = CLI_PREVIEW_ITEM_BOX,
                                                   .x = x - 2,
                                                   .y = flow->y + 2,
                                                   .width = width + 4,
                                                   .height = flow->line_height - 4,
                                                   .role = CLI_PREVIEW_ROLE_CODE_GROUND,
                                                   .stroke = CLI_PREVIEW_ROLE_NONE,
                                                   .radius = 3u,
                                                   .line = flow->line});
    }
    const int linked = flow->run_style.link_length != 0u;
    const unsigned int flags =
        (flow->base_flags | flow->run_style.flags | (linked ? CLI_PREVIEW_UNDERLINE : 0u)) & ~MARKDOWN_CODE_FLAG;
    (void)EMBED_CALL(item_put, MarkdownCtx, .items = args->items, .writing = args->writing,
                     .item = &(CliPreviewItem){.type = CLI_PREVIEW_ITEM_TEXT,
                                               .x = x,
                                               .y = flow->y,
                                               .width = width,
                                               .height = flow->line_height,
                                               .text = flow->run_text,
                                               .length = flow->run_length,
                                               .face = code ? CLI_PREVIEW_FACE_CODE : CLI_PREVIEW_FACE_SANS,
                                               .size = code ? flow->size - 1u : flow->size,
                                               .role = linked ? CLI_PREVIEW_ROLE_LINK : flow->role,
                                               .stroke = CLI_PREVIEW_ROLE_NONE,
                                               .flags = flags,
                                               .line = flow->line,
                                               .link = flow->run_style.link,
                                               .link_length = flow->run_style.link_length});
}

// the letter `letter` with the marks `style` put at the end of the flow's run, a run opened at the pen where none is
// open or the open one's marks differ, the pen moved past it
EMBED_INLINE void run_letter(const MarkdownCtx *args)
{
    MarkdownFlow *const flow = args->flow;
    if (flow->run_open && !EMBED_CALL(style_same, MarkdownCtx, .style = &flow->run_style, .other_style = args->style))
    {
        EMBED_CALL(run_commit, MarkdownCtx, .items = args->items, .writing = args->writing, .flow = flow);
    }
    if (!flow->run_open)
    {
        flow->run_open = 1;
        flow->run_x = flow->pen;
        flow->run_text = args->items->text_bytes;
        flow->run_length = 0u;
        flow->run_style = *args->style;
    }
    // a byte of the text read as a letter is the same byte put back
    const char byte = (char)args->letter;
    (void)EMBED_CALL(text_put, MarkdownCtx, .items = args->items, .writing = args->writing, .bytes = &byte,
                     .length = 1u);
    flow->run_length += 1u;
    flow->pen += (long long)EMBED_CALL(advance_of, MarkdownCtx, .letter = args->letter, .flags = args->style->flags) *
                 (long long)flow->size;
}

// the word waiting laid: on the next line where it would pass the width and the line has a word already, after the
// space waiting where the line has a word, each letter in the run of its marks
EMBED_INLINE void word_flush(const MarkdownCtx *args)
{
    MarkdownFlow *const flow = args->flow;
    if (flow->word_length == 0u)
    {
        return;
    }
    long long width = 0LL;
    for (unsigned int at = 0u; at < flow->word_length; at += 1u)
    {
        width += (long long)EMBED_CALL(advance_of, MarkdownCtx, .letter = flow->word[at],
                                       .flags = flow->word_style[at].flags) *
                 (long long)flow->size;
    }
    const long long left = (long long)flow->x0 * 1000LL;
    const long long right = (long long)(flow->x0 + flow->width) * 1000LL;
    long long space = 0LL;
    if (flow->space && (flow->pen > left))
    {
        space = (long long)EMBED_CALL(advance_of, MarkdownCtx, .letter = (unsigned char)' ',
                                      .flags = flow->word_style[0].flags) *
                (long long)flow->size;
    }
    if ((flow->pen > left) && (flow->pen + space + width > right))
    {
        EMBED_CALL(run_commit, MarkdownCtx, .items = args->items, .writing = args->writing, .flow = flow);
        flow->y += flow->line_height;
        flow->lines += 1;
        flow->pen = left;
        space = 0LL;
    }
    flow->lines = (flow->lines == 0) ? 1 : flow->lines;
    if (space != 0LL)
    {
        if (flow->run_open &&
            EMBED_CALL(style_same, MarkdownCtx, .style = &flow->run_style, .other_style = &flow->word_style[0]))
        {
            EMBED_CALL(run_letter, MarkdownCtx, .items = args->items, .writing = args->writing, .flow = flow,
                       .letter = (unsigned char)' ', .style = &flow->word_style[0]);
        }
        else
        {
            EMBED_CALL(run_commit, MarkdownCtx, .items = args->items, .writing = args->writing, .flow = flow);
            flow->pen += space;
        }
    }
    for (unsigned int at = 0u; at < flow->word_length; at += 1u)
    {
        EMBED_CALL(run_letter, MarkdownCtx, .items = args->items, .writing = args->writing, .flow = flow,
                   .letter = flow->word[at], .style = &flow->word_style[at]);
    }
    flow->word_length = 0u;
    flow->space = 0;
}

// the letter `letter` fed to the flow with the marks it now takes: a space lays the word waiting and leaves a space
// waiting, any other letter waits in the word, a word at its most laid first
EMBED_INLINE void flow_letter(const MarkdownCtx *args)
{
    MarkdownFlow *const flow = args->flow;
    if ((args->letter == ' ') || (args->letter == '\t') || (args->letter == '\n'))
    {
        EMBED_CALL(word_flush, MarkdownCtx, .items = args->items, .writing = args->writing, .flow = flow);
        flow->space = 1;
        return;
    }
    if (flow->word_length == MARKDOWN_WORD)
    {
        EMBED_CALL(word_flush, MarkdownCtx, .items = args->items, .writing = args->writing, .flow = flow);
    }
    flow->word[flow->word_length] = args->letter;
    flow->word_style[flow->word_length] = flow->style;
    flow->word_length += 1u;
}

// the flow begun at `x`, `*y`, `width` wide, its text `size` tall on lines `line_height` apart in the role `role` with
// the flags `flags`, from the source line `line`
EMBED_INLINE void flow_begin(const MarkdownCtx *args)
{
    MarkdownFlow *const flow = args->flow;
    flow->x0 = args->x;
    flow->width = (args->width > 1) ? args->width : 1;
    flow->pen = (long long)args->x * 1000LL;
    flow->y = *args->y;
    flow->line_height = args->line_height;
    flow->size = args->size;
    flow->role = args->role;
    flow->base_flags = args->flags;
    flow->line = args->line;
    flow->style = (MarkdownStyle){0u, 0u, 0u};
    flow->run_open = 0;
    flow->word_length = 0u;
    flow->space = 0;
    flow->lines = 0;
}

// the flow's line ended here, the next starting under it
EMBED_INLINE void flow_break(const MarkdownCtx *args)
{
    MarkdownFlow *const flow = args->flow;
    EMBED_CALL(word_flush, MarkdownCtx, .items = args->items, .writing = args->writing, .flow = flow);
    EMBED_CALL(run_commit, MarkdownCtx, .items = args->items, .writing = args->writing, .flow = flow);
    flow->lines = (flow->lines == 0) ? 1 : flow->lines;
    flow->lines += 1;
    flow->y += flow->line_height;
    flow->pen = (long long)flow->x0 * 1000LL;
    flow->space = 0;
}

// the flow ended, its word and its run laid: the height its lines fill
EMBED_INLINE int flow_end(const MarkdownCtx *args)
{
    MarkdownFlow *const flow = args->flow;
    EMBED_CALL(word_flush, MarkdownCtx, .items = args->items, .writing = args->writing, .flow = flow);
    EMBED_CALL(run_commit, MarkdownCtx, .items = args->items, .writing = args->writing, .flow = flow);
    return flow->lines * flow->line_height;
}

// whether `letter` is ASCII punctuation a backslash takes literally
EMBED_INLINE int letter_escaped(const MarkdownCtx *args)
{
    const unsigned char letter = args->letter;
    return (letter == '\\') || (letter == '`') || (letter == '*') || (letter == '_') || (letter == '{') ||
           (letter == '}') || (letter == '[') || (letter == ']') || (letter == '(') || (letter == ')') ||
           (letter == '#') || (letter == '+') || (letter == '-') || (letter == '.') || (letter == '!') ||
           (letter == '|') || (letter == '~') || (letter == '<') || (letter == '>') || (letter == '$');
}

// whether `letter` is a letter or a digit
EMBED_INLINE int letter_word(const MarkdownCtx *args)
{
    const unsigned char letter = args->letter;
    return ((letter >= 'a') && (letter <= 'z')) || ((letter >= 'A') && (letter <= 'Z')) ||
           ((letter >= '0') && (letter <= '9')) || (letter >= 0x80u);
}

// whether the bytes from `from` to before `to` start with the `length` bytes of `bytes`, in any case
EMBED_INLINE int starts_with(const MarkdownCtx *args)
{
    if (args->to - args->from < args->length)
    {
        return 0;
    }
    for (unsigned int at = 0u; at < args->length; at += 1u)
    {
        const unsigned char one = (unsigned char)args->text[args->from + at];
        const unsigned char lower = ((one >= 'A') && (one <= 'Z')) ? (unsigned char)(one + 32u) : one;
        if (lower != (unsigned char)args->bytes[at])
        {
            return 0;
        }
    }
    return 1;
}

// whether the address from `from` to before `to` is safe to follow: none that runs script, its blanks and controls
// passed over as a browser passes them
EMBED_INLINE int address_safe(const MarkdownCtx *args)
{
    char bare[16];
    unsigned int bare_length = 0u;
    for (unsigned int at = args->from; (at < args->to) && (bare_length < sizeof(bare)); at += 1u)
    {
        const unsigned char one = (unsigned char)args->text[at];
        if (one <= ' ')
        {
            continue;
        }
        bare[bare_length] = (char)one;
        bare_length += 1u;
    }
    return !EMBED_CALL(starts_with, MarkdownCtx, .text = bare, .from = 0u, .to = bare_length,
                       .bytes = "javascript:", .length = 11u) &&
           !EMBED_CALL(starts_with, MarkdownCtx, .text = bare, .from = 0u, .to = bare_length,
                       .bytes = "vbscript:", .length = 9u) &&
           !EMBED_CALL(starts_with, MarkdownCtx, .text = bare, .from = 0u, .to = bare_length, .bytes = "data:text/html",
                       .length = 14u);
}

// the link whose `[` stands at `from`, before `to`, read into `link_out`: its label to the matching `]`, then `(`, its
// address up to a blank or the `)`, `<` and `>` about it taken off, and any title after: 1, or 0 where none stands
// there
EMBED_INLINE int link_read(const MarkdownCtx *args)
{
    const char *const text = args->text;
    unsigned int depth = 0u;
    unsigned int close = MARKDOWN_NONE_AT;
    for (unsigned int at = args->from; (close == MARKDOWN_NONE_AT) && (at < args->to); at += 1u)
    {
        if ((text[at] == '\\') && (at + 1u < args->to))
        {
            at += 1u;
            continue;
        }
        depth += (text[at] == '[') ? 1u : 0u;
        if (text[at] == ']')
        {
            depth -= 1u;
            close = (depth == 0u) ? at : MARKDOWN_NONE_AT;
        }
    }
    if ((close == MARKDOWN_NONE_AT) || (close + 1u >= args->to) || (text[close + 1u] != '('))
    {
        return 0;
    }
    unsigned int end = close + 2u;
    while ((end < args->to) && (text[end] != ')'))
    {
        end += 1u;
    }
    if (end >= args->to)
    {
        return 0;
    }
    unsigned int from = close + 2u;
    while ((from < end) && (text[from] == ' '))
    {
        from += 1u;
    }
    const int angled = (from < end) && (text[from] == '<');
    from += angled ? 1u : 0u;
    unsigned int to = from;
    while ((to < end) && (text[to] != ' ') && (!angled || (text[to] != '>')))
    {
        to += 1u;
    }
    *args->link_out = (MarkdownLink){args->from + 1u, close, from, to, end + 1u};
    return 1;
}

// whether a closing run of `length` of the letter `letter`, after a letter that is no blank, stands after `from`,
// before `to`
EMBED_INLINE int closer_after(const MarkdownCtx *args)
{
    const char *const text = args->text;
    for (unsigned int at = args->from + 1u; at + args->length <= args->to; at += 1u)
    {
        int run = 1;
        for (unsigned int each = 0u; each < args->length; each += 1u)
        {
            run = run && ((unsigned char)text[at + each] == args->letter);
        }
        if (run && (text[at - 1u] != ' ') && (text[at - 1u] != '\t'))
        {
            return 1;
        }
    }
    return 0;
}

// the end of the address written bare from `from`, before `to`, its last punctuation left off: MARKDOWN_NONE_AT
// where no address starts there
EMBED_INLINE unsigned int bare_end(const MarkdownCtx *args)
{
    const char *const text = args->text;
    const int starts = EMBED_CALL(starts_with, MarkdownCtx, .text = text, .from = args->from, .to = args->to,
                                  .bytes = "http://", .length = 7u) ||
                       EMBED_CALL(starts_with, MarkdownCtx, .text = text, .from = args->from, .to = args->to,
                                  .bytes = "https://", .length = 8u) ||
                       EMBED_CALL(starts_with, MarkdownCtx, .text = text, .from = args->from, .to = args->to,
                                  .bytes = "www.", .length = 4u);
    if (!starts)
    {
        return MARKDOWN_NONE_AT;
    }
    unsigned int end = args->from;
    while ((end < args->to) && (text[end] != ' ') && (text[end] != '\t') && (text[end] != '<'))
    {
        end += 1u;
    }
    while ((end > args->from) &&
           ((text[end - 1u] == '.') || (text[end - 1u] == ',') || (text[end - 1u] == ':') || (text[end - 1u] == ';') ||
            (text[end - 1u] == '"') || (text[end - 1u] == '\'') || (text[end - 1u] == ')') || (text[end - 1u] == ']')))
    {
        end -= 1u;
    }
    return (end > args->from + 4u) ? end : MARKDOWN_NONE_AT;
}

// the bytes from `from` to before `to` fed to the flow as they stand, with the marks `flags` added to the marks the
// letters take, a blank a space
EMBED_INLINE void letters_fed(const MarkdownCtx *args)
{
    MarkdownFlow *const flow = args->flow;
    const unsigned int was = flow->style.flags;
    flow->style.flags |= args->flags;
    for (unsigned int at = args->from; at < args->to; at += 1u)
    {
        EMBED_CALL(flow_letter, MarkdownCtx, .items = args->items, .writing = args->writing, .flow = flow,
                   .letter = (unsigned char)args->text[at]);
    }
    flow->style.flags = was;
}

// a link's address, from `from` to before `to`, put into the text cell after the word waiting and the run are laid,
// `www.` taking `http://` before it, and the letters marked as its link from here on
EMBED_INLINE void link_opened(const MarkdownCtx *args)
{
    MarkdownFlow *const flow = args->flow;
    EMBED_CALL(word_flush, MarkdownCtx, .items = args->items, .writing = args->writing, .flow = flow);
    EMBED_CALL(run_commit, MarkdownCtx, .items = args->items, .writing = args->writing, .flow = flow);
    const int www = EMBED_CALL(starts_with, MarkdownCtx, .text = args->text, .from = args->from, .to = args->to,
                               .bytes = "www.", .length = 4u);
    const unsigned int at = www ? EMBED_CALL(text_put, MarkdownCtx, .items = args->items, .writing = args->writing,
                                             .bytes = "http://", .length = 7u)
                                : args->items->text_bytes;
    (void)EMBED_CALL(text_put, MarkdownCtx, .items = args->items, .writing = args->writing,
                     .bytes = args->text + args->from, .length = args->to - args->from);
    flow->style.link = at;
    flow->style.link_length = (args->to - args->from) + (www ? 7u : 0u);
}

// the emphasis the `length` letters `letter` at `from` open or close, before `to`: bold for two, italic for one,
// struck through for two tildes, an opener before a letter that is no blank with its closer after it and a closer
// after a letter that is no blank, an underscore's only at a word's edge: 1 where they did
EMBED_INLINE int emphasis_turned(const MarkdownCtx *args)
{
    MarkdownFlow *const flow = args->flow;
    const char *const text = args->text;
    const unsigned int mark = (args->letter == '~')  ? CLI_PREVIEW_STRIKE
                              : (args->length == 2u) ? CLI_PREVIEW_BOLD
                                                     : CLI_PREVIEW_ITALIC;
    const unsigned char before = (args->from > args->at) ? (unsigned char)text[args->from - 1u] : (unsigned char)' ';
    const unsigned char after =
        (args->from + args->length < args->to) ? (unsigned char)text[args->from + args->length] : (unsigned char)' ';
    const int under = args->letter == '_';
    if ((flow->style.flags & mark) != 0u)
    {
        const int closes =
            (before != ' ') && (before != '\t') && (!under || !EMBED_CALL(letter_word, MarkdownCtx, .letter = after));
        flow->style.flags &= closes ? ~mark : ~0u;
        return closes;
    }
    const int opens = (after != ' ') && (after != '\t') &&
                      (!under || !EMBED_CALL(letter_word, MarkdownCtx, .letter = before)) &&
                      EMBED_CALL(closer_after, MarkdownCtx, .text = text, .from = args->from + args->length - 1u,
                                 .to = args->to, .letter = args->letter, .length = args->length);
    flow->style.flags |= opens ? mark : 0u;
    return opens;
}

// the inline text from `from` to before `to` fed to the flow: a backslash's punctuation as it stands, code between
// backticks in the code face, an image's text, a link's label marked with its address, an address between `<` and `>`
// or written bare as a link, emphasis and strike turned on and off by their marks, and every other letter as it is;
// a link's label is read in the same pass, its end ending the link and the pass going on after it
EMBED_INLINE void inline_laid(const MarkdownCtx *args)
{
    MarkdownFlow *const flow = args->flow;
    const char *const text = args->text;
    const unsigned int to = args->to;
    unsigned int at = args->from;
    unsigned int label_to = MARKDOWN_NONE_AT;
    unsigned int resume = MARKDOWN_NONE_AT;
    unsigned int label_flags = 0u;
    while (at < to)
    {
        if (at == label_to)
        {
            flow->style.flags &= ~label_flags;
            flow->style.link = 0u;
            flow->style.link_length = 0u;
            at = resume;
            label_to = MARKDOWN_NONE_AT;
            continue;
        }
        const unsigned char letter = (unsigned char)text[at];
        const unsigned char next = (at + 1u < to) ? (unsigned char)text[at + 1u] : 0u;
        const int inside = label_to != MARKDOWN_NONE_AT;
        if ((letter == '\\') && EMBED_CALL(letter_escaped, MarkdownCtx, .letter = next))
        {
            EMBED_CALL(flow_letter, MarkdownCtx, .items = args->items, .writing = args->writing, .flow = flow,
                       .letter = next);
            at += 2u;
            continue;
        }
        if (letter == '`')
        {
            unsigned int ticks = 0u;
            while ((at + ticks < to) && (text[at + ticks] == '`'))
            {
                ticks += 1u;
            }
            unsigned int close = at + ticks;
            unsigned int found = MARKDOWN_NONE_AT;
            while ((found == MARKDOWN_NONE_AT) && (close < to))
            {
                unsigned int run = 0u;
                while ((close + run < to) && (text[close + run] == '`'))
                {
                    run += 1u;
                }
                found = (run == ticks) ? close : MARKDOWN_NONE_AT;
                close += (run != 0u) ? run : 1u;
            }
            if (found == MARKDOWN_NONE_AT)
            {
                EMBED_CALL(letters_fed, MarkdownCtx, .items = args->items, .writing = args->writing, .flow = flow,
                           .text = text, .from = at, .to = at + ticks);
                at += ticks;
                continue;
            }
            const unsigned int inner = at + ticks;
            const int trimmed = (found > inner + 1u) && (text[inner] == ' ') && (text[found - 1u] == ' ');
            EMBED_CALL(letters_fed, MarkdownCtx, .items = args->items, .writing = args->writing, .flow = flow,
                       .text = text, .from = inner + (trimmed ? 1u : 0u), .to = found - (trimmed ? 1u : 0u),
                       .flags = MARKDOWN_CODE_FLAG);
            at = found + ticks;
            continue;
        }
        MarkdownLink link;
        if (!inside && (((letter == '!') && (next == '[')) || (letter == '[')) &&
            EMBED_CALL(link_read, MarkdownCtx, .text = text, .from = (letter == '!') ? at + 1u : at, .to = to,
                       .link_out = &link))
        {
            const int image = letter == '!';
            if (!image &&
                EMBED_CALL(address_safe, MarkdownCtx, .text = text, .from = link.address_from, .to = link.address_to))
            {
                EMBED_CALL(link_opened, MarkdownCtx, .items = args->items, .writing = args->writing, .flow = flow,
                           .text = text, .from = link.address_from, .to = link.address_to);
            }
            label_flags = image ? CLI_PREVIEW_ITALIC : 0u;
            flow->style.flags |= label_flags;
            label_to = link.label_to;
            resume = link.end;
            at = link.label_from;
            continue;
        }
        if (!inside && (letter == '<'))
        {
            unsigned int close = at + 1u;
            while ((close < to) && (text[close] != '>') && (text[close] != ' '))
            {
                close += 1u;
            }
            const int autolink = (close < to) && (text[close] == '>') &&
                                 (EMBED_CALL(starts_with, MarkdownCtx, .text = text, .from = at + 1u, .to = close,
                                             .bytes = "http://", .length = 7u) ||
                                  EMBED_CALL(starts_with, MarkdownCtx, .text = text, .from = at + 1u, .to = close,
                                             .bytes = "https://", .length = 8u) ||
                                  EMBED_CALL(starts_with, MarkdownCtx, .text = text, .from = at + 1u, .to = close,
                                             .bytes = "mailto:", .length = 7u));
            if (autolink)
            {
                EMBED_CALL(link_opened, MarkdownCtx, .items = args->items, .writing = args->writing, .flow = flow,
                           .text = text, .from = at + 1u, .to = close);
                label_flags = 0u;
                label_to = close;
                resume = close + 1u;
                at += 1u;
                continue;
            }
        }
        const unsigned char before = (at > args->from) ? (unsigned char)text[at - 1u] : (unsigned char)' ';
        const unsigned int bare = (!inside && ((before == ' ') || (before == '\t') || (before == '(')))
                                      ? EMBED_CALL(bare_end, MarkdownCtx, .text = text, .from = at, .to = to)
                                      : MARKDOWN_NONE_AT;
        if (bare != MARKDOWN_NONE_AT)
        {
            EMBED_CALL(link_opened, MarkdownCtx, .items = args->items, .writing = args->writing, .flow = flow,
                       .text = text, .from = at, .to = bare);
            label_flags = 0u;
            label_to = bare;
            resume = bare;
            continue;
        }
        const int doubled = (at + 1u < to) && (next == letter);
        if (((letter == '*') || (letter == '_') || ((letter == '~') && doubled)) &&
            EMBED_CALL(emphasis_turned, MarkdownCtx, .flow = flow, .text = text, .at = args->from, .from = at, .to = to,
                       .letter = letter, .length = (doubled || (letter == '~')) ? 2u : 1u))
        {
            at += (doubled || (letter == '~')) ? 2u : 1u;
            continue;
        }
        EMBED_CALL(flow_letter, MarkdownCtx, .items = args->items, .writing = args->writing, .flow = flow,
                   .letter = letter);
        at += 1u;
    }
    if (label_to != MARKDOWN_NONE_AT)
    {
        flow->style.flags &= ~label_flags;
        flow->style.link = 0u;
        flow->style.link_length = 0u;
    }
}

// the columns the line `slice` is indented by, a tab to its stop, and where its first byte that is no blank stands
// into `at`: how many columns
EMBED_INLINE unsigned int slice_indent(const MarkdownCtx *args)
{
    unsigned int columns = 0u;
    unsigned int at = args->slice->from;
    while ((at < args->slice->to) && ((args->text[at] == ' ') || (args->text[at] == '\t')))
    {
        columns = (args->text[at] == '\t') ? ((columns / MARKDOWN_TAB) + 1u) * MARKDOWN_TAB : columns + 1u;
        at += 1u;
    }
    if (args->range_out != NULL)
    {
        args->range_out[0][0] = at;
    }
    return columns;
}

// whether the line `slice` is blank
EMBED_INLINE int slice_blank(const MarkdownCtx *args)
{
    for (unsigned int at = args->slice->from; at < args->slice->to; at += 1u)
    {
        if ((args->text[at] != ' ') && (args->text[at] != '\t'))
        {
            return 0;
        }
    }
    return 1;
}

// the run of `letter` the line `slice` opens with after no more than three spaces, and what follows it into `at` of
// `range_out`: how long the run is
EMBED_INLINE unsigned int slice_run(const MarkdownCtx *args)
{
    unsigned int first[1][2] = {{0u, 0u}};
    const unsigned int indent =
        EMBED_CALL(slice_indent, MarkdownCtx, .text = args->text, .slice = args->slice, .range_out = first);
    if (indent > 3u)
    {
        return 0u;
    }
    unsigned int at = first[0][0];
    while ((at < args->slice->to) && ((unsigned char)args->text[at] == args->letter))
    {
        at += 1u;
    }
    if (args->range_out != NULL)
    {
        args->range_out[0][0] = at;
    }
    return at - first[0][0];
}

// the fence the line `slice` is, three or more backticks or tildes after no more than three spaces: how long its run
// is, 0 for none
EMBED_INLINE unsigned int slice_fence(const MarkdownCtx *args)
{
    const unsigned int ticks =
        EMBED_CALL(slice_run, MarkdownCtx, .text = args->text, .slice = args->slice, .letter = (unsigned char)'`');
    if (ticks >= 3u)
    {
        return ticks;
    }
    const unsigned int tildes =
        EMBED_CALL(slice_run, MarkdownCtx, .text = args->text, .slice = args->slice, .letter = (unsigned char)'~');
    return (tildes >= 3u) ? tildes : 0u;
}

// the level of the heading the line `slice` is, one to six `#` after no more than three spaces and a blank or the
// line's end after them, its text's bytes into `range_out`, closing `#`s and blanks left off: 0 for none
EMBED_INLINE unsigned int slice_heading(const MarkdownCtx *args)
{
    unsigned int after[1][2] = {{0u, 0u}};
    const unsigned int level = EMBED_CALL(slice_run, MarkdownCtx, .text = args->text, .slice = args->slice,
                                          .letter = (unsigned char)'#', .range_out = after);
    const unsigned int at = after[0][0];
    if ((level == 0u) || (level > 6u) ||
        ((at < args->slice->to) && (args->text[at] != ' ') && (args->text[at] != '\t')))
    {
        return 0u;
    }
    unsigned int from = at;
    while ((from < args->slice->to) && ((args->text[from] == ' ') || (args->text[from] == '\t')))
    {
        from += 1u;
    }
    unsigned int to = args->slice->to;
    while ((to > from) && ((args->text[to - 1u] == ' ') || (args->text[to - 1u] == '\t')))
    {
        to -= 1u;
    }
    unsigned int hashes = to;
    while ((hashes > from) && (args->text[hashes - 1u] == '#'))
    {
        hashes -= 1u;
    }
    if ((hashes < to) && ((hashes == from) || (args->text[hashes - 1u] == ' ') || (args->text[hashes - 1u] == '\t')))
    {
        to = hashes;
        while ((to > from) && ((args->text[to - 1u] == ' ') || (args->text[to - 1u] == '\t')))
        {
            to -= 1u;
        }
    }
    args->range_out[0][0] = from;
    args->range_out[0][1] = to;
    return level;
}

// whether the line `slice` is a rule, three or more of one of `-`, `*` and `_` after no more than three spaces with
// only blanks between
EMBED_INLINE int slice_rule(const MarkdownCtx *args)
{
    unsigned int first[1][2] = {{0u, 0u}};
    if (EMBED_CALL(slice_indent, MarkdownCtx, .text = args->text, .slice = args->slice, .range_out = first) > 3u)
    {
        return 0;
    }
    const char mark = (first[0][0] < args->slice->to) ? args->text[first[0][0]] : '\0';
    if ((mark != '-') && (mark != '*') && (mark != '_'))
    {
        return 0;
    }
    unsigned int marks = 0u;
    for (unsigned int at = first[0][0]; at < args->slice->to; at += 1u)
    {
        const char one = args->text[at];
        if ((one != mark) && (one != ' ') && (one != '\t'))
        {
            return 0;
        }
        marks += (one == mark) ? 1u : 0u;
    }
    return marks >= 3u;
}

// whether the line `slice` opens a quote, `>` after no more than three spaces
EMBED_INLINE int slice_quote(const MarkdownCtx *args)
{
    return EMBED_CALL(slice_run, MarkdownCtx, .text = args->text, .slice = args->slice, .letter = (unsigned char)'>') !=
           0u;
}

// the list item the line `slice` opens, `-`, `*`, `+` or one to nine digits and `.` or `)` after no more than three
// spaces and a blank or the line's end after the mark, read into `item_out`: 1, or 0 where it opens none
EMBED_INLINE int slice_item(const MarkdownCtx *args)
{
    unsigned int first[1][2] = {{0u, 0u}};
    const unsigned int indent =
        EMBED_CALL(slice_indent, MarkdownCtx, .text = args->text, .slice = args->slice, .range_out = first);
    const char *const text = args->text;
    const unsigned int to = args->slice->to;
    unsigned int at = first[0][0];
    if ((indent > 3u) || (at >= to))
    {
        return 0;
    }
    unsigned long long number = 0ULL;
    unsigned int digits = 0u;
    while ((at < to) && (text[at] >= '0') && (text[at] <= '9') && (digits < 9u))
    {
        // a digit's value is its byte less the zero's
        number = (number * 10ULL) + (unsigned long long)(unsigned char)(text[at] - '0');
        digits += 1u;
        at += 1u;
    }
    const int ordered = digits != 0u;
    if (ordered ? ((at >= to) || ((text[at] != '.') && (text[at] != ')')))
                : ((text[at] != '-') && (text[at] != '*') && (text[at] != '+')))
    {
        return 0;
    }
    at += 1u;
    if ((at < to) && (text[at] != ' ') && (text[at] != '\t'))
    {
        return 0;
    }
    unsigned int blanks = 0u;
    unsigned int content = at;
    while ((content < to) && ((text[content] == ' ') || (text[content] == '\t')))
    {
        blanks += 1u;
        content += 1u;
    }
    const unsigned int spaces = (blanks < 1u) ? 1u : ((blanks > 4u) ? 1u : blanks);
    if (blanks > 4u)
    {
        content = at + 1u;
    }
    *args->item_out = (MarkdownItem){indent, (at - first[0][0]) + spaces, content, ordered, number};
    return 1;
}

// whether the line `slice` opens a block that ends a paragraph: a fence, a heading, a rule, a quote or a list item
EMBED_INLINE int slice_interrupts(const MarkdownCtx *args)
{
    unsigned int range[1][2] = {{0u, 0u}};
    MarkdownItem item;
    return (EMBED_CALL(slice_fence, MarkdownCtx, .text = args->text, .slice = args->slice) != 0u) ||
           (EMBED_CALL(slice_heading, MarkdownCtx, .text = args->text, .slice = args->slice, .range_out = range) !=
            0u) ||
           EMBED_CALL(slice_rule, MarkdownCtx, .text = args->text, .slice = args->slice) ||
           EMBED_CALL(slice_quote, MarkdownCtx, .text = args->text, .slice = args->slice) ||
           EMBED_CALL(slice_item, MarkdownCtx, .text = args->text, .slice = args->slice, .item_out = &item);
}

// whether the line `slice` carries a `|`
EMBED_INLINE int slice_piped(const MarkdownCtx *args)
{
    for (unsigned int at = args->slice->from; at < args->slice->to; at += 1u)
    {
        if (args->text[at] == '|')
        {
            return 1;
        }
    }
    return 0;
}

// whether the line `slice` is a table's alignment row: only pipes, colons, dashes and blanks, a dash among them
EMBED_INLINE int slice_aligns(const MarkdownCtx *args)
{
    int dashed = 0;
    for (unsigned int at = args->slice->from; at < args->slice->to; at += 1u)
    {
        const char one = args->text[at];
        if ((one != '|') && (one != ':') && (one != '-') && (one != ' ') && (one != '\t'))
        {
            return 0;
        }
        dashed = dashed || (one == '-');
    }
    return dashed;
}

// the cells of the table row `slice` into `range_out`, each from and to with its blanks left off, the pipes at the
// row's ends left off and an escaped pipe standing in its cell: how many, MARKDOWN_COLUMNS at the most
EMBED_INLINE unsigned int slice_cells(const MarkdownCtx *args)
{
    const char *const text = args->text;
    unsigned int from = args->slice->from;
    unsigned int to = args->slice->to;
    while ((from < to) && ((text[from] == ' ') || (text[from] == '\t')))
    {
        from += 1u;
    }
    while ((to > from) && ((text[to - 1u] == ' ') || (text[to - 1u] == '\t')))
    {
        to -= 1u;
    }
    from += ((from < to) && (text[from] == '|')) ? 1u : 0u;
    to -= ((to > from) && (text[to - 1u] == '|') && ((to - 1u == from) || (text[to - 2u] != '\\'))) ? 1u : 0u;
    unsigned int count = 0u;
    unsigned int start = from;
    for (unsigned int at = from; (at <= to) && (count < MARKDOWN_COLUMNS); at += 1u)
    {
        const int ends = (at == to) || ((text[at] == '|') && ((at == from) || (text[at - 1u] != '\\')));
        if (!ends)
        {
            continue;
        }
        unsigned int cell_from = start;
        unsigned int cell_to = at;
        while ((cell_from < cell_to) && ((text[cell_from] == ' ') || (text[cell_from] == '\t')))
        {
            cell_from += 1u;
        }
        while ((cell_to > cell_from) && ((text[cell_to - 1u] == ' ') || (text[cell_to - 1u] == '\t')))
        {
            cell_to -= 1u;
        }
        args->range_out[count][0] = cell_from;
        args->range_out[count][1] = cell_to;
        count += 1u;
        start = at + 1u;
    }
    return count;
}

// the width the bytes from `from` to before `to` take at `size` with the flags `flags`, in the page's pixels
EMBED_INLINE int bytes_width(const MarkdownCtx *args)
{
    long long width = 0LL;
    for (unsigned int at = args->from; at < args->to; at += 1u)
    {
        width += (long long)EMBED_CALL(advance_of, MarkdownCtx, .letter = (unsigned char)args->text[at],
                                       .flags = args->flags) *
                 (long long)args->size;
    }
    // a line's width in pixels is far inside what an int represents
    return (int)((width + 999LL) / 1000LL);
}

// a line of code, the bytes of `slice` from `from`, laid at `x`, `*y` in the code face, a tab to its stop and cut
// before the width `width`
EMBED_INLINE void code_line_put(const MarkdownCtx *args)
{
    const char *const text = args->text;
    const unsigned long long fit = ((unsigned long long)((args->width > 0) ? args->width : 0) * 1000ULL) /
                                   ((unsigned long long)MARKDOWN_CODE_ADVANCE * MARKDOWN_CODE);
    const unsigned int start = args->items->text_bytes;
    unsigned int columns = 0u;
    for (unsigned int at = args->from; at < args->slice->to; at += 1u)
    {
        const unsigned char one = (unsigned char)text[at];
        const int lead = (one < 0x80u) || (one >= 0xC0u);
        if (lead && (columns >= fit))
        {
            break;
        }
        if (one == '\t')
        {
            const unsigned int stop = ((columns / MARKDOWN_TAB) + 1u) * MARKDOWN_TAB;
            while ((columns < stop) && (columns < fit))
            {
                (void)EMBED_CALL(text_put, MarkdownCtx, .items = args->items, .writing = args->writing, .bytes = " ",
                                 .length = 1u);
                columns += 1u;
            }
            continue;
        }
        (void)EMBED_CALL(text_put, MarkdownCtx, .items = args->items, .writing = args->writing, .bytes = text + at,
                         .length = 1u);
        columns += lead ? 1u : 0u;
    }
    const unsigned int length = args->items->text_bytes - start;
    if (length == 0u)
    {
        return;
    }
    // the columns fit the width, an int
    (void)EMBED_CALL(item_put, MarkdownCtx, .items = args->items, .writing = args->writing,
                     .item =
                         &(CliPreviewItem){.type = CLI_PREVIEW_ITEM_TEXT,
                                           .x = args->x,
                                           .y = *args->y,
                                           .width = (int)((columns * MARKDOWN_CODE_ADVANCE * MARKDOWN_CODE) / 1000u),
                                           .height = MARKDOWN_CODE_LINE,
                                           .text = start,
                                           .length = length,
                                           .face = CLI_PREVIEW_FACE_CODE,
                                           .size = MARKDOWN_CODE,
                                           .role = CLI_PREVIEW_ROLE_TEXT,
                                           .stroke = CLI_PREVIEW_ROLE_NONE,
                                           .line = args->line});
}

// where the line `slice` goes on past the first `at` columns of its indent, a tab to its stop
EMBED_INLINE unsigned int slice_after(const MarkdownCtx *args)
{
    unsigned int columns = 0u;
    unsigned int at = args->slice->from;
    while ((at < args->slice->to) && (columns < args->at) && ((args->text[at] == ' ') || (args->text[at] == '\t')))
    {
        columns = (args->text[at] == '\t') ? ((columns / MARKDOWN_TAB) + 1u) * MARKDOWN_TAB : columns + 1u;
        at += 1u;
    }
    return at;
}

// the code block of `count` lines from `lines`, each from the columns `at` of its indent, laid at `x`, `*y`, `width`
// wide on the code's ground, `*y` moved past it
EMBED_INLINE void code_laid(const MarkdownCtx *args)
{
    int *const y = args->y;
    const unsigned int ground = EMBED_CALL(item_put, MarkdownCtx, .items = args->items, .writing = args->writing,
                                           .item = &(CliPreviewItem){.type = CLI_PREVIEW_ITEM_BOX,
                                                                     .x = args->x,
                                                                     .y = *y,
                                                                     .width = args->width,
                                                                     .role = CLI_PREVIEW_ROLE_CODE_GROUND,
                                                                     .stroke = CLI_PREVIEW_ROLE_NONE,
                                                                     .radius = MARKDOWN_CODE_RADIUS,
                                                                     .line = args->line});
    int inner = *y + MARKDOWN_CODE_PAD;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        const MarkdownLine *const slice = &args->lines[at];
        EMBED_CALL(code_line_put, MarkdownCtx, .text = args->text, .items = args->items, .writing = args->writing,
                   .slice = slice,
                   .from = EMBED_CALL(slice_after, MarkdownCtx, .text = args->text, .slice = slice, .at = args->at),
                   .x = args->x + MARKDOWN_CODE_PAD, .y = &inner, .width = args->width - (2 * MARKDOWN_CODE_PAD),
                   .line = slice->line);
        inner += MARKDOWN_CODE_LINE;
    }
    const int height = (inner - *y) + MARKDOWN_CODE_PAD;
    EMBED_CALL(item_height_put, MarkdownCtx, .items = args->items, .writing = args->writing, .at = ground,
               .height = height);
    *y += height;
}

// the inline text from `from` to before `to` of the line `slice` and the lines after it in `lines`, `count` in all,
// laid as one flow at `x`, `*y`, `width` wide, at `size` on lines `line_height` apart in the role `role` with the flags
// `flags`: each line's leading blanks left off, a line ending in two spaces or a backslash breaking the flow and any
// other joining the next with a space; `*y` moved past it
EMBED_INLINE void flow_laid(const MarkdownCtx *args)
{
    MarkdownFlow *const flow = args->flow;
    EMBED_CALL(flow_begin, MarkdownCtx, .flow = flow, .x = args->x, .y = args->y, .width = args->width,
               .size = args->size, .line_height = args->line_height, .role = args->role, .flags = args->flags,
               .line = args->line);
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        const MarkdownLine *const slice = &args->lines[at];
        unsigned int from = (at == 0u) ? args->from : slice->from;
        unsigned int to = (at == 0u) ? args->to : slice->to;
        while ((from < to) && ((args->text[from] == ' ') || (args->text[from] == '\t')))
        {
            from += 1u;
        }
        const int broken = ((to >= from + 2u) && (args->text[to - 1u] == ' ') && (args->text[to - 2u] == ' ')) ||
                           ((to > from) && (args->text[to - 1u] == '\\'));
        to -= ((to > from) && (args->text[to - 1u] == '\\')) ? 1u : 0u;
        while ((to > from) && ((args->text[to - 1u] == ' ') || (args->text[to - 1u] == '\t')))
        {
            to -= 1u;
        }
        EMBED_CALL(inline_laid, MarkdownCtx, .text = args->text, .items = args->items, .writing = args->writing,
                   .flow = flow, .from = from, .to = to);
        if (at + 1u == args->count)
        {
            continue;
        }
        if (broken)
        {
            EMBED_CALL(flow_break, MarkdownCtx, .items = args->items, .writing = args->writing, .flow = flow);
        }
        else
        {
            EMBED_CALL(flow_letter, MarkdownCtx, .items = args->items, .writing = args->writing, .flow = flow,
                       .letter = (unsigned char)' ');
        }
    }
    *args->y += EMBED_CALL(flow_end, MarkdownCtx, .items = args->items, .writing = args->writing, .flow = flow);
}

// a rule laid `width` wide at `x`, `*y`, `*y` moved past it
EMBED_INLINE void rule_laid(const MarkdownCtx *args)
{
    (void)EMBED_CALL(item_put, MarkdownCtx, .items = args->items, .writing = args->writing,
                     .item = &(CliPreviewItem){.type = CLI_PREVIEW_ITEM_RULE,
                                               .x = args->x,
                                               .y = *args->y + args->height,
                                               .width = args->width,
                                               .height = MARKDOWN_RULE,
                                               .role = CLI_PREVIEW_ROLE_LINE,
                                               .stroke = CLI_PREVIEW_ROLE_NONE,
                                               .line = args->line});
    *args->y += (2 * args->height) + MARKDOWN_RULE;
}

// the table row of the cells `range_out`, `count` of them, laid in the `at` columns of the widths `widths` at `x`,
// `*y`: each cell on a border in the hairline, a heading row's on the code's ground and bold where `heading`, its text
// flowing inside its padding, the row as tall as its tallest cell; `*y` moved past it
EMBED_INLINE void row_laid(const MarkdownCtx *args)
{
    int *const y = args->y;
    const int top = *y;
    const unsigned int columns = args->at;
    int total = 0;
    for (unsigned int column = 0u; column < columns; column += 1u)
    {
        total += args->widths[column];
    }
    const unsigned int ground = args->heading
                                    ? EMBED_CALL(item_put, MarkdownCtx, .items = args->items, .writing = args->writing,
                                                 .item = &(CliPreviewItem){.type = CLI_PREVIEW_ITEM_BOX,
                                                                           .x = args->x,
                                                                           .y = top,
                                                                           .width = total,
                                                                           .role = CLI_PREVIEW_ROLE_CODE_GROUND,
                                                                           .stroke = CLI_PREVIEW_ROLE_NONE,
                                                                           .line = args->line})
                                    : MARKDOWN_NONE_AT;
    const unsigned int border = args->items->items;
    int cell_x = args->x;
    for (unsigned int column = 0u; column < columns; column += 1u)
    {
        const int cell_width = args->widths[column];
        (void)EMBED_CALL(item_put, MarkdownCtx, .items = args->items, .writing = args->writing,
                         .item = &(CliPreviewItem){.type = CLI_PREVIEW_ITEM_BOX,
                                                   .x = cell_x,
                                                   .y = top,
                                                   .width = cell_width,
                                                   .role = CLI_PREVIEW_ROLE_NONE,
                                                   .stroke = CLI_PREVIEW_ROLE_LINE,
                                                   .line = args->line});
        cell_x += cell_width;
    }
    int tallest = MARKDOWN_BODY_LINE;
    cell_x = args->x;
    for (unsigned int column = 0u; column < columns; column += 1u)
    {
        const int cell_width = args->widths[column];
        if (column < args->count)
        {
            int cell_y = top + MARKDOWN_CELL_PAD;
            const MarkdownLine cell = {args->range_out[column][0], args->range_out[column][1], args->line};
            EMBED_CALL(flow_laid, MarkdownCtx, .text = args->text, .items = args->items, .writing = args->writing,
                       .flow = args->flow, .lines = &cell, .count = 1u, .from = cell.from, .to = cell.to,
                       .x = cell_x + MARKDOWN_CELL_PAD, .y = &cell_y, .width = cell_width - (2 * MARKDOWN_CELL_PAD),
                       .size = MARKDOWN_BODY, .line_height = MARKDOWN_BODY_LINE, .role = args->role,
                       .flags = args->heading ? CLI_PREVIEW_BOLD : 0u, .line = args->line);
            const int height = cell_y - (top + MARKDOWN_CELL_PAD);
            tallest = (height > tallest) ? height : tallest;
        }
        cell_x += cell_width;
    }
    const int row_height = tallest + (2 * MARKDOWN_CELL_PAD);
    if (ground != MARKDOWN_NONE_AT)
    {
        EMBED_CALL(item_height_put, MarkdownCtx, .items = args->items, .writing = args->writing, .at = ground,
                   .height = row_height);
    }
    for (unsigned int column = 0u; column < columns; column += 1u)
    {
        EMBED_CALL(item_height_put, MarkdownCtx, .items = args->items, .writing = args->writing, .at = border + column,
                   .height = row_height);
    }
    *y = top + row_height;
}

// the table of `count` lines from `lines`, its head, its alignment row and its body, laid at `x`, `*y`, no wider than
// `width`: each column as wide as its widest cell's text and padding and no narrower than the least, every column
// narrowed by one share where the table passes the width; `*y` moved past it
EMBED_INLINE void table_laid(const MarkdownCtx *args)
{
    unsigned int cells[MARKDOWN_COLUMNS][2];
    int widths[MARKDOWN_COLUMNS];
    const unsigned int columns =
        EMBED_CALL(slice_cells, MarkdownCtx, .text = args->text, .slice = &args->lines[0], .range_out = cells);
    for (unsigned int column = 0u; column < columns; column += 1u)
    {
        widths[column] = MARKDOWN_CELL_LEAST;
    }
    for (unsigned int row = 0u; row < args->count; row += 1u)
    {
        if (row == 1u)
        {
            continue;
        }
        const unsigned int count =
            EMBED_CALL(slice_cells, MarkdownCtx, .text = args->text, .slice = &args->lines[row], .range_out = cells);
        for (unsigned int column = 0u; (column < count) && (column < columns); column += 1u)
        {
            const int natural = EMBED_CALL(bytes_width, MarkdownCtx, .text = args->text, .from = cells[column][0],
                                           .to = cells[column][1], .size = MARKDOWN_BODY,
                                           .flags = (row == 0u) ? CLI_PREVIEW_BOLD : 0u) +
                                (2 * MARKDOWN_CELL_PAD);
            widths[column] = (natural > widths[column]) ? natural : widths[column];
        }
    }
    unsigned long long total = 0ULL;
    for (unsigned int column = 0u; column < columns; column += 1u)
    {
        // a width is not negative
        total += (unsigned long long)widths[column];
    }
    // the width the table stands in is not negative
    const unsigned long long fit_width = (args->width > 0) ? (unsigned long long)args->width : 1ULL;
    for (unsigned int column = 0u; (total > fit_width) && (column < columns); column += 1u)
    {
        // a share of a width is no wider than the width, an int
        widths[column] = (int)(((unsigned long long)widths[column] * fit_width) / total);
    }
    for (unsigned int row = 0u; row < args->count; row += 1u)
    {
        if (row == 1u)
        {
            continue;
        }
        const unsigned int count =
            EMBED_CALL(slice_cells, MarkdownCtx, .text = args->text, .slice = &args->lines[row], .range_out = cells);
        EMBED_CALL(row_laid, MarkdownCtx, .text = args->text, .items = args->items, .writing = args->writing,
                   .flow = args->flow, .widths = widths, .at = columns, .range_out = cells, .count = count,
                   .x = args->x, .y = args->y, .role = args->role, .heading = row == 0u, .line = args->lines[row].line);
    }
}

static void blocks_laid(const MarkdownCtx *args);

// the lines of `lines` from `at`, `count` in all, a container's, laid as blocks at `x`, `*y`, `width` wide, `depth`
// deep, `tight` where they are a list item's, their marks taken off: a quote's first `>` and the space after it where
// `quoted`, else `from` columns of indent, the first line starting at `to`; a cell for the lines alone carries them as
// the container reads them
static void container_laid(const MarkdownCtx *args)
{
    if (args->count == 0u)
    {
        return;
    }
    MarkdownLine *const inner =
        g_cli_guard->temporary_buf_alloc(args->count * sizeof(MarkdownLine), g_cli_preview_owner);
    if (inner == NULL)
    {
        return;
    }
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        const MarkdownLine *const slice = &args->lines[args->at + at];
        unsigned int from = slice->from;
        if (args->quoted)
        {
            unsigned int first[1][2] = {{0u, 0u}};
            (void)EMBED_CALL(slice_indent, MarkdownCtx, .text = args->text, .slice = slice, .range_out = first);
            const unsigned int mark = first[0][0];
            if ((mark < slice->to) && (args->text[mark] == '>'))
            {
                from = mark + (((mark + 1u < slice->to) && (args->text[mark + 1u] == ' ')) ? 2u : 1u);
            }
        }
        else if (at != 0u)
        {
            from = EMBED_CALL(slice_after, MarkdownCtx, .text = args->text, .slice = slice, .at = args->from);
        }
        inner[at] = (MarkdownLine){from, slice->to, slice->line};
    }
    if (!args->quoted)
    {
        inner[0].from = args->to;
    }
    EMBED_CALL(blocks_laid, MarkdownCtx, .text = args->text, .items = args->items, .writing = args->writing,
               .flow = args->flow, .lines = inner, .count = args->count, .x = args->x, .y = args->y,
               .width = args->width, .depth = args->depth, .tight = args->tight, .role = args->role);
    g_cli_guard->persistent_buf_release(inner, g_cli_preview_owner);
}

// the list whose first item stands at `at` of `lines`, `count` in all, laid at `x`, `*y`, `width` wide, `depth` deep:
// each item a block, its mark at the left, a disc, its number or a task's box with the tick where it is checked, and
// its lines as a container of their own past the indent; the list ends at a line that opens no item of its type and
// belongs to none: the line after it
static unsigned int list_laid(const MarkdownCtx *args)
{
    const char *const text = args->text;
    int *const y = args->y;
    MarkdownItem first;
    (void)EMBED_CALL(slice_item, MarkdownCtx, .text = text, .slice = &args->lines[args->at], .item_out = &first);
    unsigned long long number = first.number;
    unsigned int at = args->at;
    int begun = 0;
    while (at < args->count)
    {
        MarkdownItem one;
        if (!EMBED_CALL(slice_item, MarkdownCtx, .text = text, .slice = &args->lines[at], .item_out = &one) ||
            (one.ordered != first.ordered) || (one.indent > first.indent + 1u))
        {
            break;
        }
        const unsigned int indent = one.indent + one.width;
        unsigned int end = at + 1u;
        while (end < args->count)
        {
            const MarkdownLine *const next = &args->lines[end];
            if (EMBED_CALL(slice_blank, MarkdownCtx, .text = text, .slice = next))
            {
                const int goes_on =
                    (end + 1u < args->count) &&
                    !EMBED_CALL(slice_blank, MarkdownCtx, .text = text, .slice = &args->lines[end + 1u]) &&
                    (EMBED_CALL(slice_indent, MarkdownCtx, .text = text, .slice = &args->lines[end + 1u]) >= indent);
                if (!goes_on)
                {
                    break;
                }
                end += 1u;
                continue;
            }
            MarkdownItem nested;
            const int deeper = EMBED_CALL(slice_indent, MarkdownCtx, .text = text, .slice = next) >= indent;
            const int lazy = !EMBED_CALL(slice_interrupts, MarkdownCtx, .text = text, .slice = next) &&
                             !EMBED_CALL(slice_item, MarkdownCtx, .text = text, .slice = next, .item_out = &nested) &&
                             (text[next->from] != ' ') && (text[next->from] != '\t');
            if (!deeper && !lazy)
            {
                break;
            }
            end += 1u;
        }
        while ((end > at + 1u) && EMBED_CALL(slice_blank, MarkdownCtx, .text = text, .slice = &args->lines[end - 1u]))
        {
            end -= 1u;
        }
        *y += begun ? MARKDOWN_TIGHT : 0;
        begun = 1;
        const unsigned int line = args->lines[at].line;
        const int item_top = *y;
        const unsigned int block =
            EMBED_CALL(block_put, MarkdownCtx, .items = args->items, .writing = args->writing, .line = line, .y = y,
                       .x = args->x, .width = args->width, .depth = args->depth + 1u);
        const MarkdownLine *const marked = &args->lines[at];
        unsigned int content = one.content;
        const int task = (marked->to >= content + 3u) && (text[content] == '[') && (text[content + 2u] == ']') &&
                         ((text[content + 1u] == ' ') || (text[content + 1u] == 'x') || (text[content + 1u] == 'X')) &&
                         ((marked->to == content + 3u) || (text[content + 3u] == ' ') || (text[content + 3u] == '\t'));
        if (task)
        {
            const int checked = text[content + 1u] != ' ';
            const int box_y = item_top + ((MARKDOWN_BODY_LINE - MARKDOWN_TASK) / 2);
            (void)EMBED_CALL(item_put, MarkdownCtx, .items = args->items, .writing = args->writing,
                             .item =
                                 &(CliPreviewItem){.type = CLI_PREVIEW_ITEM_BOX,
                                                   .x = args->x + 2,
                                                   .y = box_y,
                                                   .width = MARKDOWN_TASK,
                                                   .height = MARKDOWN_TASK,
                                                   .role = checked ? CLI_PREVIEW_ROLE_SIGNAL : CLI_PREVIEW_ROLE_NONE,
                                                   .stroke = CLI_PREVIEW_ROLE_LINE,
                                                   .radius = 3u,
                                                   .line = line});
            if (checked)
            {
                (void)EMBED_CALL(item_put, MarkdownCtx, .items = args->items, .writing = args->writing,
                                 .item = &(CliPreviewItem){.type = CLI_PREVIEW_ITEM_SEGMENT,
                                                           .x = args->x + 5,
                                                           .y = box_y + 7,
                                                           .width = 2,
                                                           .height = 3,
                                                           .role = CLI_PREVIEW_ROLE_TEXT,
                                                           .stroke = CLI_PREVIEW_ROLE_NONE,
                                                           .line = line});
                (void)EMBED_CALL(item_put, MarkdownCtx, .items = args->items, .writing = args->writing,
                                 .item = &(CliPreviewItem){.type = CLI_PREVIEW_ITEM_SEGMENT,
                                                           .x = args->x + 7,
                                                           .y = box_y + 10,
                                                           .width = 5,
                                                           .height = -7,
                                                           .role = CLI_PREVIEW_ROLE_TEXT,
                                                           .stroke = CLI_PREVIEW_ROLE_NONE,
                                                           .line = line});
            }
            content += 3u;
            while ((content < marked->to) && ((text[content] == ' ') || (text[content] == '\t')))
            {
                content += 1u;
            }
        }
        else if (first.ordered)
        {
            char digits[24];
            VerbaLinea written = {digits, sizeof(digits), 0ULL};
            mmgr_line_decimal(&written, number, 1u);
            mmgr_line_ch(&written, '.');
            // a number's digits are a few bytes
            const unsigned int length = (unsigned int)written.at;
            const unsigned int at_text = EMBED_CALL(text_put, MarkdownCtx, .items = args->items,
                                                    .writing = args->writing, .bytes = digits, .length = length);
            (void)EMBED_CALL(item_put, MarkdownCtx, .items = args->items, .writing = args->writing,
                             .item =
                                 &(CliPreviewItem){.type = CLI_PREVIEW_ITEM_TEXT,
                                                   .x = args->x,
                                                   .y = item_top,
                                                   .width = EMBED_CALL(bytes_width, MarkdownCtx, .text = digits,
                                                                       .from = 0u, .to = length, .size = MARKDOWN_BODY),
                                                   .height = MARKDOWN_BODY_LINE,
                                                   .text = at_text,
                                                   .length = length,
                                                   .face = CLI_PREVIEW_FACE_SANS,
                                                   .size = MARKDOWN_BODY,
                                                   .role = args->role,
                                                   .stroke = CLI_PREVIEW_ROLE_NONE,
                                                   .line = line});
        }
        else
        {
            (void)EMBED_CALL(item_put, MarkdownCtx, .items = args->items, .writing = args->writing,
                             .item = &(CliPreviewItem){.type = CLI_PREVIEW_ITEM_BOX,
                                                       .x = args->x + 8,
                                                       .y = item_top + ((MARKDOWN_BODY_LINE - MARKDOWN_DISC) / 2),
                                                       .width = MARKDOWN_DISC,
                                                       .height = MARKDOWN_DISC,
                                                       .role = args->role,
                                                       .stroke = CLI_PREVIEW_ROLE_NONE,
                                                       .radius = (unsigned int)(MARKDOWN_DISC / 2),
                                                       .line = line});
        }
        EMBED_CALL(container_laid, MarkdownCtx, .text = text, .items = args->items, .writing = args->writing,
                   .flow = args->flow, .lines = args->lines, .at = at, .count = end - at, .from = indent, .to = content,
                   .x = args->x + MARKDOWN_INDENT, .y = y, .width = args->width - MARKDOWN_INDENT,
                   .depth = args->depth + 2u, .tight = 1, .role = args->role, .quoted = 0);
        *y = (*y > item_top) ? *y : item_top + MARKDOWN_BODY_LINE;
        EMBED_CALL(item_height_put, MarkdownCtx, .items = args->items, .writing = args->writing, .at = block,
                   .height = *y - item_top);
        number += 1ULL;
        at = end;
    }
    return at;
}

// the quote whose first line stands at `at` of `lines`, `count` in all, laid at `x`, `*y`, `width` wide, `depth`
// deep: a bar in the hairline down its left and its lines, `>` taken off, as a container of their own in the quieter
// role, a line that opens no quote going with it while it goes on a paragraph: the line after it
static unsigned int quote_laid(const MarkdownCtx *args)
{
    const char *const text = args->text;
    int *const y = args->y;
    unsigned int end = args->at;
    while (end < args->count)
    {
        const MarkdownLine *const next = &args->lines[end];
        if (EMBED_CALL(slice_blank, MarkdownCtx, .text = text, .slice = next))
        {
            break;
        }
        const int quoted = EMBED_CALL(slice_quote, MarkdownCtx, .text = text, .slice = next);
        if (!quoted && ((end == args->at) || EMBED_CALL(slice_interrupts, MarkdownCtx, .text = text, .slice = next)))
        {
            break;
        }
        end += 1u;
    }
    const int top = *y;
    const unsigned int bar = EMBED_CALL(item_put, MarkdownCtx, .items = args->items, .writing = args->writing,
                                        .item = &(CliPreviewItem){.type = CLI_PREVIEW_ITEM_BOX,
                                                                  .x = args->x,
                                                                  .y = top,
                                                                  .width = MARKDOWN_QUOTE_BAR,
                                                                  .role = CLI_PREVIEW_ROLE_LINE,
                                                                  .stroke = CLI_PREVIEW_ROLE_NONE,
                                                                  .line = args->lines[args->at].line});
    EMBED_CALL(container_laid, MarkdownCtx, .text = text, .items = args->items, .writing = args->writing,
               .flow = args->flow, .lines = args->lines, .at = args->at, .count = end - args->at,
               .x = args->x + MARKDOWN_QUOTE_IN, .y = y, .width = args->width - MARKDOWN_QUOTE_IN,
               .depth = args->depth + 1u, .tight = 0, .role = CLI_PREVIEW_ROLE_QUIET, .quoted = 1);
    *y = (*y > top) ? *y : top + MARKDOWN_BODY_LINE;
    EMBED_CALL(item_height_put, MarkdownCtx, .items = args->items, .writing = args->writing, .at = bar,
               .height = *y - top);
    return end;
}

// the lines of `lines`, `count` in all, laid as blocks at `x`, `*y`, `width` wide, `depth` deep, in the role `role`,
// a gap before every block but the first, a list item's narrower where `tight`: each block put with its rectangle and
// read in markdown.js's order, a fence, a heading, a rule, indented code, a quote, a list, a table, and a paragraph
// that runs to a blank line or a line that opens another block, a line of `=` or `-` under it making it a heading.
// Containers lay their own lines through blocks_laid again, so the walk is a function that calls itself and is not
// inlined
static void blocks_laid(const MarkdownCtx *args)
{
    const char *const text = args->text;
    int *const y = args->y;
    unsigned int at = 0u;
    int begun = 0;
    while (at < args->count)
    {
        const MarkdownLine *const slice = &args->lines[at];
        if (EMBED_CALL(slice_blank, MarkdownCtx, .text = text, .slice = slice))
        {
            at += 1u;
            continue;
        }
        *y += begun ? (args->tight ? MARKDOWN_TIGHT : MARKDOWN_GAP) : 0;
        begun = 1;
        unsigned int range[1][2] = {{0u, 0u}};
        MarkdownItem item;
        const unsigned int fence = EMBED_CALL(slice_fence, MarkdownCtx, .text = text, .slice = slice);
        const unsigned int level =
            EMBED_CALL(slice_heading, MarkdownCtx, .text = text, .slice = slice, .range_out = range);
        const int nests = args->depth < MARKDOWN_DEPTH;
        const int top = *y;
        if (level != 0u)
        {
            *y += (top > CLI_PREVIEW_PAD_TOP) ? MARKDOWN_HEADING_ABOVE : 0;
        }
        const unsigned int block =
            EMBED_CALL(block_put, MarkdownCtx, .items = args->items, .writing = args->writing, .line = slice->line,
                       .y = y, .x = args->x, .width = args->width, .depth = args->depth, .heading = level != 0u);
        const int block_top = *y;
        if (fence != 0u)
        {
            const char mark = text[EMBED_CALL(slice_after, MarkdownCtx, .text = text, .slice = slice, .at = 3u)];
            unsigned int end = at + 1u;
            while ((end < args->count) && (EMBED_CALL(slice_run, MarkdownCtx, .text = text, .slice = &args->lines[end],
                                                      .letter = (unsigned char)mark) < fence))
            {
                end += 1u;
            }
            EMBED_CALL(code_laid, MarkdownCtx, .text = text, .items = args->items, .writing = args->writing,
                       .lines = args->lines + at + 1u, .count = end - (at + 1u), .at = 0u, .x = args->x, .y = y,
                       .width = args->width, .line = slice->line);
            at = (end < args->count) ? end + 1u : end;
        }
        else if (level != 0u)
        {
            const unsigned int size = s_heading_size[level - 1u];
            // a heading's line is its size and a share more, a few dozen pixels
            EMBED_CALL(flow_laid, MarkdownCtx, .text = text, .items = args->items, .writing = args->writing,
                       .flow = args->flow, .lines = slice, .count = 1u, .from = range[0][0], .to = range[0][1],
                       .x = args->x, .y = y, .width = args->width, .size = size,
                       .line_height = (int)((size * 13u) / 10u) + 4, .role = args->role, .flags = CLI_PREVIEW_BOLD,
                       .line = slice->line);
            if (level <= 2u)
            {
                EMBED_CALL(rule_laid, MarkdownCtx, .items = args->items, .writing = args->writing, .x = args->x, .y = y,
                           .width = args->width, .height = 2, .line = slice->line);
            }
            at += 1u;
        }
        else if (EMBED_CALL(slice_rule, MarkdownCtx, .text = text, .slice = slice))
        {
            EMBED_CALL(rule_laid, MarkdownCtx, .items = args->items, .writing = args->writing, .x = args->x, .y = y,
                       .width = args->width, .height = MARKDOWN_RULE_GAP, .line = slice->line);
            at += 1u;
        }
        else if (EMBED_CALL(slice_indent, MarkdownCtx, .text = text, .slice = slice) >= MARKDOWN_TAB)
        {
            unsigned int end = at;
            unsigned int last = at;
            while ((end < args->count) &&
                   (EMBED_CALL(slice_blank, MarkdownCtx, .text = text, .slice = &args->lines[end]) ||
                    (EMBED_CALL(slice_indent, MarkdownCtx, .text = text, .slice = &args->lines[end]) >= MARKDOWN_TAB)))
            {
                last = EMBED_CALL(slice_blank, MarkdownCtx, .text = text, .slice = &args->lines[end]) ? last : end;
                end += 1u;
            }
            EMBED_CALL(code_laid, MarkdownCtx, .text = text, .items = args->items, .writing = args->writing,
                       .lines = slice, .count = (last - at) + 1u, .at = MARKDOWN_TAB, .x = args->x, .y = y,
                       .width = args->width, .line = slice->line);
            at = last + 1u;
        }
        else if (nests && EMBED_CALL(slice_quote, MarkdownCtx, .text = text, .slice = slice))
        {
            at = EMBED_CALL(quote_laid, MarkdownCtx, .text = text, .items = args->items, .writing = args->writing,
                            .flow = args->flow, .lines = args->lines, .count = args->count, .at = at, .x = args->x,
                            .y = y, .width = args->width, .depth = args->depth);
        }
        else if (nests && EMBED_CALL(slice_item, MarkdownCtx, .text = text, .slice = slice, .item_out = &item))
        {
            at = EMBED_CALL(list_laid, MarkdownCtx, .text = text, .items = args->items, .writing = args->writing,
                            .flow = args->flow, .lines = args->lines, .count = args->count, .at = at, .x = args->x,
                            .y = y, .width = args->width, .depth = args->depth, .role = args->role);
        }
        else if (EMBED_CALL(slice_piped, MarkdownCtx, .text = text, .slice = slice) && (at + 1u < args->count) &&
                 EMBED_CALL(slice_aligns, MarkdownCtx, .text = text, .slice = &args->lines[at + 1u]))
        {
            unsigned int end = at + 2u;
            while ((end < args->count) &&
                   !EMBED_CALL(slice_blank, MarkdownCtx, .text = text, .slice = &args->lines[end]) &&
                   EMBED_CALL(slice_piped, MarkdownCtx, .text = text, .slice = &args->lines[end]))
            {
                end += 1u;
            }
            EMBED_CALL(table_laid, MarkdownCtx, .text = text, .items = args->items, .writing = args->writing,
                       .flow = args->flow, .lines = slice, .count = end - at, .x = args->x, .y = y,
                       .width = args->width, .role = args->role);
            at = end;
        }
        else
        {
            unsigned int end = at + 1u;
            unsigned int setext = 0u;
            while ((end < args->count) &&
                   !EMBED_CALL(slice_blank, MarkdownCtx, .text = text, .slice = &args->lines[end]))
            {
                const MarkdownLine *const next = &args->lines[end];
                const unsigned int equals =
                    EMBED_CALL(slice_run, MarkdownCtx, .text = text, .slice = next, .letter = (unsigned char)'=');
                const unsigned int dashes =
                    EMBED_CALL(slice_run, MarkdownCtx, .text = text, .slice = next, .letter = (unsigned char)'-');
                unsigned int after[1][2] = {{0u, 0u}};
                (void)EMBED_CALL(slice_run, MarkdownCtx, .text = text, .slice = next,
                                 .letter = (unsigned char)((equals != 0u) ? '=' : '-'), .range_out = after);
                const MarkdownLine rest = {after[0][0], next->to, next->line};
                if (((equals != 0u) || (dashes != 0u)) &&
                    EMBED_CALL(slice_blank, MarkdownCtx, .text = text, .slice = &rest))
                {
                    setext = (equals != 0u) ? 1u : 2u;
                    break;
                }
                if (EMBED_CALL(slice_interrupts, MarkdownCtx, .text = text, .slice = next))
                {
                    break;
                }
                end += 1u;
            }
            const unsigned int size = (setext != 0u) ? s_heading_size[setext - 1u] : MARKDOWN_BODY;
            if ((setext != 0u) && args->writing)
            {
                args->items->block[args->items->blocks - 1u].heading = 1;
            }
            // a heading's line is its size and a share more, a few dozen pixels
            EMBED_CALL(flow_laid, MarkdownCtx, .text = text, .items = args->items, .writing = args->writing,
                       .flow = args->flow, .lines = slice, .count = end - at, .from = slice->from, .to = slice->to,
                       .x = args->x, .y = y, .width = args->width, .size = size,
                       .line_height = (setext != 0u) ? (int)((size * 13u) / 10u) + 4 : MARKDOWN_BODY_LINE,
                       .role = args->role, .flags = (setext != 0u) ? CLI_PREVIEW_BOLD : 0u, .line = slice->line);
            if (setext != 0u)
            {
                EMBED_CALL(rule_laid, MarkdownCtx, .items = args->items, .writing = args->writing, .x = args->x, .y = y,
                           .width = args->width, .height = 2, .line = slice->line);
            }
            at = (setext != 0u) ? end + 1u : end;
        }
        EMBED_CALL(item_height_put, MarkdownCtx, .items = args->items, .writing = args->writing, .at = block,
                   .height = *y - block_top);
    }
}

int cli_preview_markdown_layout(const CliPreviewMarkdownCfg *args)
{
    const char *const text = args->text;
    CliPreviewItems *const items = args->items;
    // a text the preview lays is far shorter than four gigabytes, so its places are words
    const unsigned int length = (args->length < 0xffffffffULL) ? (unsigned int)args->length : 0xfffffffeu;
    unsigned int count = 1u;
    for (unsigned int at = 0u; at < length; at += 1u)
    {
        count += (text[at] == '\n') ? 1u : 0u;
    }
    MarkdownLine *const lines = g_cli_guard->temporary_buf_alloc(count * sizeof(MarkdownLine), g_cli_preview_owner);
    if (lines == NULL)
    {
        return 0;
    }
    unsigned int line = 0u;
    unsigned int start = 0u;
    for (unsigned int at = 0u; at <= length; at += 1u)
    {
        if ((at < length) && (text[at] != '\n'))
        {
            continue;
        }
        const unsigned int end = ((at > start) && (text[at - 1u] == '\r')) ? at - 1u : at;
        lines[line] = (MarkdownLine){start, end, args->first_line + line};
        line += 1u;
        start = at + 1u;
    }
    MarkdownFlow flow;
    const int width = args->width_px - (2 * CLI_PREVIEW_PAD);
    const int first_top = (items->height > 0) ? items->height : CLI_PREVIEW_PAD_TOP;
    // the count pass over the counts the items have, writing nothing
    CliPreviewItems counted = {NULL, items->items,  NULL,         items->blocks, NULL, items->text_bytes,
                               NULL, items->points, items->height};
    int y = first_top;
    EMBED_CALL(blocks_laid, MarkdownCtx, .text = text, .items = &counted, .writing = 0, .flow = &flow, .lines = lines,
               .count = count, .x = CLI_PREVIEW_PAD, .y = &y, .width = width, .depth = 0u,
               .role = CLI_PREVIEW_ROLE_TEXT);
    const unsigned long long item_bytes = ((unsigned long long)counted.items * sizeof(CliPreviewItem)) +
                                          ((unsigned long long)counted.blocks * sizeof(CliPreviewBlock));
    unsigned char *const cell =
        (item_bytes != 0ULL) ? g_cli_guard->persistent_buf_alloc(item_bytes, g_cli_preview_owner) : NULL;
    char *const laid_text =
        (counted.text_bytes != 0u) ? g_cli_guard->persistent_buf_alloc(counted.text_bytes, g_cli_preview_owner) : NULL;
    if (((item_bytes != 0ULL) && (cell == NULL)) || ((counted.text_bytes != 0u) && (laid_text == NULL)))
    {
        g_cli_guard->persistent_buf_release(cell, g_cli_preview_owner);
        g_cli_guard->persistent_buf_release(laid_text, g_cli_preview_owner);
        g_cli_guard->persistent_buf_release(lines, g_cli_preview_owner);
        return 0;
    }
    CliPreviewItems laid = {(CliPreviewItem *)(void *)cell,
                            items->items,
                            (cell != NULL) ? (CliPreviewBlock *)(void *)(cell + ((unsigned long long)counted.items *
                                                                                 sizeof(CliPreviewItem)))
                                           : NULL,
                            items->blocks,
                            laid_text,
                            items->text_bytes,
                            items->point,
                            items->points,
                            items->height};
    if (items->items != 0u)
    {
        memor.cpy(&(MemoriaCfg){.dst = laid.item, .src = items->item, .bytes = items->items * sizeof(CliPreviewItem)});
    }
    if (items->blocks != 0u)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = laid.block, .src = items->block, .bytes = items->blocks * sizeof(CliPreviewBlock)});
    }
    if (items->text_bytes != 0u)
    {
        memor.cpy(&(MemoriaCfg){.dst = laid.text, .src = items->text, .bytes = items->text_bytes});
    }
    y = first_top;
    EMBED_CALL(blocks_laid, MarkdownCtx, .text = text, .items = &laid, .writing = 1, .flow = &flow, .lines = lines,
               .count = count, .x = CLI_PREVIEW_PAD, .y = &y, .width = width, .depth = 0u,
               .role = CLI_PREVIEW_ROLE_TEXT);
    laid.height = ((y + CLI_PREVIEW_PAD_FOOT) > laid.height) ? y + CLI_PREVIEW_PAD_FOOT : laid.height;
    g_cli_guard->persistent_buf_release(items->item, g_cli_preview_owner);
    g_cli_guard->persistent_buf_release(items->text, g_cli_preview_owner);
    *items = laid;
    g_cli_guard->persistent_buf_release(lines, g_cli_preview_owner);
    return 1;
}
