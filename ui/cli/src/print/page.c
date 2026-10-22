// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// page.c: a listing read, the page it prints on, and the listing laid out in pages, each line with its number and a
// long line going on under itself
#include "../command_line/cells.h"
#include "../text/regexp.h"
#include "print.h"

#include <math.h>

// The listing, its layout and the letters a line is laid out from, a cell taken with the lock printing is done under
// and given back with it
typedef struct
{
    CliPrintListing listing;
    CliPrintLaid laid;
    unsigned int letters[CLI_PRINT_TEXT / 4u];
} CliPrintRoom;

// the room, NULL where the cells could not meet it, and the lock printing is done under
static CliPrintRoom *s_room;
static CliLock s_lock = CLI_LOCK_INIT;

void cli_print_take(void)
{
    cli_lock_take(&s_lock);
    s_room = cli_cell_taken(sizeof(*s_room), "a listing printed");
}

void cli_print_give(void)
{
    cli_cell_given(s_room);
    s_room = NULL;
    cli_lock_give(&s_lock);
}

// `length` bytes of `text` kept among the listing's texts: where they start
static unsigned int cli_kept(CliPrintListing *listing, const char *text, unsigned long long length, int *full)
{
    if ((listing->text_used + length) > CLI_PRINT_TEXT)
    {
        *full = 1;
        return 0u;
    }
    const unsigned int at = listing->text_used;
    memor.cpy(&(MemoriaCfg){.dst = listing->text + at, .src = text, .bytes = length});
    // the texts are below CLI_PRINT_TEXT, which an unsigned int counts
    listing->text_used += (unsigned int)length;
    return at;
}

int cli_print_listing_read(const CliJson *json, unsigned int node, CliPrintListing **listing, CliLine *said)
{
    if (s_room == NULL)
    {
        cli_line_text(said, "the listing could not be read: the cells cannot meet it");
        return 0;
    }
    CliPrintListing *const held = &s_room->listing;
    int full = 0;
    held->lines = 0u;
    held->runs = 0u;
    held->text_used = 0u;
    held->tab = 4ull;
    const char *const title = cli_json_string(json, cli_json_member(json, node, "title"), NULL);
    memor.cpy(&(MemoriaCfg){
        .dst = held->title,
        .src = (title != NULL) ? title : "",
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = (title != NULL) ? title : "", .cap = CLI_CELLS_POOL}) + 1u});
    const unsigned int tab = cli_json_member(json, node, "tab");
    unsigned long long tab_value = 0ull;
    if (cli_json_is(json, tab, CFG_JSON_NUMBER))
    {
        const CfgJsonToken token = {CFG_JSON_NUMBER, json->node[tab].text,
                                    json->node[tab].text + json->node[tab].length, 0u, 0u};
        held->tab = cfg_json_unsigned(json->text, &token, &tab_value) ? tab_value : held->tab;
    }
    const unsigned int lines = cli_json_member(json, node, "lines");
    for (unsigned int item = cli_json_is(json, lines, CFG_JSON_ARRAY) ? json->node[lines].first : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json->node[item].next)
    {
        if (held->lines >= CLI_PRINT_LINES)
        {
            full = 1;
            break;
        }
        CliPrintLine *const line = &held->line[held->lines];
        const unsigned int number = cli_json_member(json, item, "number");
        unsigned long long value = 0ull;
        line->numbered = 0;
        if (cli_json_is(json, number, CFG_JSON_NUMBER))
        {
            const CfgJsonToken token = {CFG_JSON_NUMBER, json->node[number].text,
                                        json->node[number].text + json->node[number].length, 0u, 0u};
            line->numbered = cfg_json_unsigned(json->text, &token, &value);
        }
        line->number = value;
        line->first = held->runs;
        line->runs = 0u;
        const unsigned int runs = cli_json_member(json, item, "runs");
        for (unsigned int run = cli_json_is(json, runs, CFG_JSON_ARRAY) ? json->node[runs].first : CLI_JSON_NONE;
             run != CLI_JSON_NONE; run = json->node[run].next)
        {
            if (held->runs >= CLI_PRINT_RUNS)
            {
                full = 1;
                break;
            }
            CliPrintRun *const kept = &held->run[held->runs];
            unsigned long long length = 0ull;
            unsigned long long color_length = 0ull;
            const char *const text = cli_json_string(json, cli_json_member(json, run, "text"), &length);
            const char *const color = cli_json_string(json, cli_json_member(json, run, "color"), &color_length);
            kept->text = cli_kept(held, (text != NULL) ? text : "", (text != NULL) ? length : 0ull, &full);
            kept->length = (text != NULL) ? (unsigned int)length : 0u;
            kept->color = cli_kept(held, (color != NULL) ? color : "", (color != NULL) ? color_length : 0ull, &full);
            kept->color_length = (color != NULL) ? (unsigned int)color_length : 0u;
            kept->bold = cli_json_is(json, cli_json_member(json, run, "bold"), CFG_JSON_TRUE);
            kept->italic = cli_json_is(json, cli_json_member(json, run, "italic"), CFG_JSON_TRUE);
            held->runs += 1u;
            line->runs += 1u;
        }
        held->lines += 1u;
    }
    if (full)
    {
        cli_line_text(said, "the listing holds more than orior_cli prints");
        return 0;
    }
    *listing = held;
    return 1;
}

void cli_print_layout_read(const CliJson *json, unsigned int node, CliPrintLayout *layout)
{
    const char *const paper = cli_json_string(json, cli_json_member(json, node, "paper"), NULL);
    memor.cpy(
        &(MemoriaCfg){.dst = layout->paper, .bytes = sizeof(layout->paper), .src = (paper != NULL) ? paper : "a4"});
    layout->landscape = cli_json_is(json, cli_json_member(json, node, "landscape"), CFG_JSON_TRUE);
    layout->gray = cli_json_is(json, cli_json_member(json, node, "gray"), CFG_JSON_TRUE);
    const char *const pages = cli_json_string(json, cli_json_member(json, node, "pages"), NULL);
    layout->has_pages = pages != NULL;
    memor.cpy(&(MemoriaCfg){
        .dst = layout->pages,
        .src = (pages != NULL) ? pages : "",
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = (pages != NULL) ? pages : "", .cap = CLI_CELLS_POOL}) + 1u});
    const unsigned int copies = cli_json_member(json, node, "copies");
    unsigned long long value = 1ull;
    if (cli_json_is(json, copies, CFG_JSON_NUMBER))
    {
        const CfgJsonToken token = {CFG_JSON_NUMBER, json->node[copies].text,
                                    json->node[copies].text + json->node[copies].length, 0u, 0u};
        value = (cfg_json_unsigned(json->text, &token, &value) && (value <= 0xffffffffull)) ? value : 1ull;
    }
    // checked against the most 32 bits hold above
    layout->copies = (unsigned int)value;
}

// whether a name is `word` in ASCII's either case
static int cli_named(const char *name, const char *word)
{
    for (; (name[0] != '\0') && (word[0] != '\0'); name += 1, word += 1)
    {
        const char letter = ((name[0] >= 'A') && (name[0] <= 'Z')) ? (char)(name[0] + 32) : name[0];
        if (letter != word[0])
        {
            return 0;
        }
    }
    return (name[0] == '\0') && (word[0] == '\0');
}

void cli_print_size(const CliPrintLayout *layout, float *width, float *height)
{
    const int letter = cli_named(layout->paper, "letter");
    const float wide = letter ? 612.0f : 595.276f;
    const float high = letter ? 792.0f : 841.89f;
    *width = layout->landscape ? high : wide;
    *height = layout->landscape ? wide : high;
}

const char *cli_print_media(const CliPrintLayout *layout)
{
    return cli_named(layout->paper, "letter") ? "na_letter_8.5x11in" : "iso_a4_210x297mm";
}

// The bytes from `from` to `to` trimmed of ASCII's spaces
static void cli_trimmed(const char **from, const char **to)
{
    while ((*from < *to) && (((*from)[0] == ' ') || (((*from)[0] >= '\t') && ((*from)[0] <= '\r'))))
    {
        *from += 1;
    }
    while ((*to > *from) && (((*to)[-1] == ' ') || (((*to)[-1] >= '\t') && ((*to)[-1] <= '\r'))))
    {
        *to -= 1;
    }
}

// the whole number the bytes from `from` to `to` write, as an unsigned number reads, at most `most`: 1, or 0 where
// they write none
static int cli_parsed(const char *from, const char *to, unsigned long long most, unsigned long long *value)
{
    from += ((from < to) && (from[0] == '+')) ? 1 : 0;
    if (from == to)
    {
        return 0;
    }
    unsigned long long total = 0ull;
    for (const char *at = from; at < to; at += 1)
    {
        const unsigned long long digit = (unsigned long long)(at[0] - '0');
        if ((at[0] < '0') || (at[0] > '9') || (total > ((most - digit) / 10ull)))
        {
            return 0;
        }
        total = (total * 10ull) + digit;
    }
    *value = total;
    return 1;
}

// each part of the pages named, between commas, handed its two ends and the place of its `-`, NULL where it has none
static int cli_parts(const CliPrintLayout *layout,
                     int (*each)(void *context, const char *from, const char *dash, const char *to), void *context)
{
    const char *from = layout->pages;
    const char *to = from + cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL});
    cli_trimmed(&from, &to);
    if (!layout->has_pages || (from == to))
    {
        return -1;
    }
    const char *part = from;
    while (part <= to)
    {
        const char *end = part;
        while ((end < to) && (end[0] != ','))
        {
            end += 1;
        }
        const char *start = part;
        const char *stop = end;
        cli_trimmed(&start, &stop);
        const char *dash = start;
        while ((dash < stop) && (dash[0] != '-'))
        {
            dash += 1;
        }
        if (each(context, start, (dash < stop) ? dash : NULL, stop))
        {
            return 1;
        }
        part = end + 1;
    }
    return 0;
}

static int cli_prints_part(void *context, const char *from, const char *dash, const char *to)
{
    const unsigned long long page = *(const unsigned long long *)context;
    if (dash == NULL)
    {
        unsigned long long value = 0ull;
        return cli_parsed(from, to, ~0ull, &value) && (value == page);
    }
    const char *low_from = from;
    const char *low_to = dash;
    const char *high_from = dash + 1;
    const char *high_to = to;
    cli_trimmed(&low_from, &low_to);
    cli_trimmed(&high_from, &high_to);
    unsigned long long low = 1ull;
    unsigned long long high = ~0ull;
    low = cli_parsed(low_from, low_to, ~0ull, &low) ? low : 1ull;
    high = cli_parsed(high_from, high_to, ~0ull, &high) ? high : ~0ull;
    return (page >= low) && (page <= high);
}

int cli_print_prints(const CliPrintLayout *layout, unsigned long long page)
{
    return cli_parts(layout, cli_prints_part, &page) != 0;
}

// The ranges kept as they are read
typedef struct
{
    int *low;
    int *high;
    unsigned int room;
    unsigned int count;
} CliRanges;

static int cli_range_part(void *context, const char *from, const char *dash, const char *to)
{
    CliRanges *const ranges = (CliRanges *)context;
    unsigned long long low = 0ull;
    unsigned long long high = 0x7fffffffull;
    if (dash == NULL)
    {
        if (!cli_parsed(from, to, 0x7fffffffull, &low))
        {
            return 0;
        }
        high = low;
    }
    else
    {
        const char *low_from = from;
        const char *low_to = dash;
        const char *high_from = dash + 1;
        const char *high_to = to;
        cli_trimmed(&low_from, &low_to);
        cli_trimmed(&high_from, &high_to);
        if (!cli_parsed(low_from, low_to, 0x7fffffffull, &low))
        {
            return 0;
        }
        high = cli_parsed(high_from, high_to, 0x7fffffffull, &high) ? high : 0x7fffffffull;
    }
    if (ranges->count < ranges->room)
    {
        // each is below the most an int holds, checked as it was read
        ranges->low[ranges->count] = (int)low;
        ranges->high[ranges->count] = (int)high;
        ranges->count += 1u;
    }
    return 0;
}

unsigned int cli_print_ranges(const CliPrintLayout *layout, int *low, int *high, unsigned int room)
{
    CliRanges ranges = {low, high, room, 0u};
    cli_parts(layout, cli_range_part, &ranges);
    return ranges.count;
}

// a hex digit's value, or 16 where the byte is none
static unsigned int cli_hex(char digit)
{
    return ((digit >= '0') && (digit <= '9'))   ? (unsigned int)(digit - '0')
           : ((digit >= 'a') && (digit <= 'f')) ? (unsigned int)(digit - 'a') + 10u
           : ((digit >= 'A') && (digit <= 'F')) ? (unsigned int)(digit - 'A') + 10u
                                                : 16u;
}

void cli_print_color(const char *text, unsigned long long length, int gray, unsigned char *rgb)
{
    const char *from = text;
    const char *to = text + length;
    cli_trimmed(&from, &to);
    while ((from < to) && (from[0] == '#'))
    {
        from += 1;
    }
    const unsigned long long size = (unsigned long long)(to - from);
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        const unsigned long long place = 2ull * at;
        const unsigned int high = (place + 1ull < size) ? cli_hex(from[place]) : 16u;
        const unsigned int low = (place + 1ull < size) ? cli_hex(from[place + 1u]) : 16u;
        // a byte is two hex digits, 0x22 where they are none
        rgb[at] = ((size >= 6ull) && (high < 16u) && (low < 16u)) ? (unsigned char)((high << 4u) | low) : 0x22u;
    }
    if (gray)
    {
        const float level = roundf((0.2126f * (float)rgb[0]) + (0.7152f * (float)rgb[1]) + (0.0722f * (float)rgb[2]));
        // a lightness of three bytes is a byte
        rgb[0] = (unsigned char)level;
        rgb[1] = rgb[0];
        rgb[2] = rgb[0];
    }
}

// The piece being made ended where it holds text: put after the row's pieces
static void cli_piece_put(CliPrintLaid *laid, CliPrintRow *row, CliPrintPiece *piece)
{
    if (piece->length == 0u)
    {
        return;
    }
    if (laid->pieces >= CLI_PRINT_PIECES)
    {
        laid->full = 1;
        piece->length = 0u;
        return;
    }
    laid->piece[laid->pieces] = *piece;
    laid->pieces += 1u;
    row->pieces += 1u;
    piece->text = laid->text_used;
    piece->length = 0u;
}

// the row being made put after the rows, and a row with no number begun after it
static void cli_row_put(CliPrintLaid *laid, CliPrintRow *row)
{
    if (laid->rows >= CLI_PRINT_ROWS)
    {
        laid->full = 1;
    }
    else
    {
        laid->row[laid->rows] = *row;
        laid->rows += 1u;
    }
    row->numbered = 0;
    row->number = 0ull;
    row->first = laid->pieces;
    row->pieces = 0u;
}

// one letter put after the piece being made, in UTF-8 among the layout's texts
static void cli_letter_put(CliPrintLaid *laid, CliPrintPiece *piece, unsigned int letter)
{
    CliLine line = {laid->text + laid->text_used, CLI_PRINT_TEXT - laid->text_used, 0ull};
    cli_utf8_put(&line, letter);
    if (!cli_line_fits(&line))
    {
        laid->full = 1;
        return;
    }
    // a letter is at most four bytes
    laid->text_used += (unsigned int)line.at;
    piece->length += (unsigned int)line.at;
}

const CliPrintLaid *cli_print_lay_out(const CliPrintListing *listing, const CliPrintLayout *layout)
{
    unsigned int *const letters = s_room->letters;
    CliPrintLaid *const laid = &s_room->laid;
    laid->rows = 0u;
    laid->pieces = 0u;
    laid->text_used = 0u;
    laid->full = 0;
    float width = 0.0f;
    float height = 0.0f;
    cli_print_size(layout, &width, &height);
    unsigned long long widest = 0ull;
    for (unsigned int at = 0u; at < listing->lines; at += 1u)
    {
        widest =
            (listing->line[at].numbered && (listing->line[at].number > widest)) ? listing->line[at].number : widest;
    }
    unsigned long long digits = 0ull;
    for (unsigned long long left = widest; left != 0ull; left /= 10ull)
    {
        digits += 1ull;
    }
    laid->number_columns = (widest == 0ull) ? 0ull : (digits + 2ull);
    // a page's columns and rows are whole and not negative, the floor of what fits
    const unsigned long long fit = (unsigned long long)floorf((width - (2.0f * CLI_PRINT_MARGIN)) / CLI_PRINT_ADVANCE);
    const unsigned long long left = (fit > laid->number_columns) ? (fit - laid->number_columns) : 0ull;
    const unsigned long long columns = (left > 8ull) ? left : 8ull;
    const unsigned long long rows =
        (unsigned long long)floorf((height - (2.0f * CLI_PRINT_MARGIN)) / CLI_PRINT_LEADING);
    // a page holds far fewer rows than an unsigned int counts
    laid->rows_per_page = (rows > 1ull) ? (unsigned int)rows : 1u;
    const unsigned long long tab = listing->tab;
    for (unsigned int index = 0u; index < listing->lines; index += 1u)
    {
        const CliPrintLine *const line = &listing->line[index];
        CliPrintRow row = {line->numbered, line->number, laid->pieces, 0u};
        unsigned long long column = 0ull;
        for (unsigned int each = 0u; each < line->runs; each += 1u)
        {
            const CliPrintRun *const run = &listing->run[line->first + each];
            CliPrintPiece piece;
            memor.set(&(MemoriaCfg){.dst = &piece, .val = 0, .bytes = sizeof(piece)});
            cli_print_color(listing->text + run->color, run->color_length, layout->gray, piece.color);
            piece.bold = run->bold;
            piece.italic = run->italic;
            piece.column = column;
            piece.text = laid->text_used;
            const unsigned long long count =
                cli_regexp_letters(listing->text + run->text, run->length, letters, CLI_PRINT_TEXT / 4u);
            for (unsigned long long at = 0ull; at < count; at += 1ull)
            {
                const unsigned int letter = letters[at];
                const int control = (letter < 0x20u) || ((letter >= 0x7fu) && (letter <= 0x9fu));
                const unsigned long long spaces =
                    (letter == '\t') ? (tab - (column % ((tab > 1ull) ? tab : 1ull))) : (control ? 0ull : 1ull);
                for (unsigned long long one = 0ull; one < spaces; one += 1ull)
                {
                    if (column == columns)
                    {
                        cli_piece_put(laid, &row, &piece);
                        cli_row_put(laid, &row);
                        column = 0ull;
                        piece.column = 0ull;
                    }
                    cli_letter_put(laid, &piece, (letter == '\t') ? ' ' : letter);
                    column += 1ull;
                }
            }
            cli_piece_put(laid, &row, &piece);
        }
        cli_row_put(laid, &row);
    }
    laid->pages = (laid->rows + laid->rows_per_page - 1u) / laid->rows_per_page;
    return laid;
}
