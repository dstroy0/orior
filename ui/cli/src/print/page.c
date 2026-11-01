// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// page.c: a listing read, the page it prints on, and the listing laid out in pages, each line with its number and a
// long line going on under itself
#include "../command_line/cells.h"
#include "../text/regexp.h"
#include "../text/text.h"
#include "print.h"

#include <math.h>

// The listing, its layout and the letters a run is laid out from, sized to the longest run: the room, a cell taken with
// the lock printing is done under, its lists cells of their own at their most, every one given back with the lock
typedef struct
{
    CliPrintListing listing;
    CliPrintLaid laid;
    unsigned int *letters;
    unsigned int letters_room;
} CliPrintRoom;

// the room, NULL where the cells could not meet it, and the lock printing is done under
static CliPrintRoom *s_room;
static CliLock s_lock = CLI_LOCK_INIT;

void cli_print_take(void)
{
    cli_lock_take(&s_lock);
    const unsigned long long line_bytes = (unsigned long long)CLI_PRINT_LINES * sizeof(CliPrintLine);
    const unsigned long long run_bytes = (unsigned long long)CLI_PRINT_RUNS * sizeof(CliPrintRun);
    const unsigned long long row_bytes = (unsigned long long)CLI_PRINT_ROWS * sizeof(CliPrintRow);
    const unsigned long long piece_bytes = (unsigned long long)CLI_PRINT_PIECES * sizeof(CliPrintPiece);
    const unsigned long long letter_bytes = (unsigned long long)CLI_PRINT_LETTERS * sizeof(unsigned int);
    const unsigned long long list_bytes = line_bytes + run_bytes + row_bytes + piece_bytes + letter_bytes;
    s_room = g_cli_cells->persistent_buf_alloc(sizeof(*s_room), &g_cli_print_cells);
    char *const cell = (s_room != NULL)
                           ? g_cli_cells->persistent_buf_alloc(list_bytes + (2ull * CLI_PRINT_TEXT), &g_cli_print_cells)
                           : NULL;
    if (cell == NULL)
    {
        g_cli_cells->persistent_buf_release(s_room, &g_cli_print_cells);
        s_room = NULL;
        return;
    }
    memor.set(&(MemoriaCfg){.dst = s_room, .val = 0, .bytes = sizeof(*s_room)});
    s_room->listing.line = (CliPrintLine *)(void *)cell;
    s_room->listing.lines_room = CLI_PRINT_LINES;
    s_room->listing.run = (CliPrintRun *)(void *)(cell + line_bytes);
    s_room->listing.runs_room = CLI_PRINT_RUNS;
    s_room->laid.row = (CliPrintRow *)(void *)(cell + line_bytes + run_bytes);
    s_room->laid.rows_room = CLI_PRINT_ROWS;
    s_room->laid.piece = (CliPrintPiece *)(void *)(cell + line_bytes + run_bytes + row_bytes);
    s_room->laid.pieces_room = CLI_PRINT_PIECES;
    s_room->letters = (unsigned int *)(void *)(cell + line_bytes + run_bytes + row_bytes + piece_bytes);
    s_room->letters_room = CLI_PRINT_LETTERS;
    s_room->listing.text = cell + list_bytes;
    s_room->listing.text_room = CLI_PRINT_TEXT;
    s_room->laid.text = cell + list_bytes + CLI_PRINT_TEXT;
    s_room->laid.text_room = CLI_PRINT_TEXT;
}

void cli_print_give(void)
{
    if (s_room != NULL)
    {
        g_cli_cells->persistent_buf_release(s_room->listing.line, &g_cli_print_cells);
    }
    g_cli_cells->persistent_buf_release(s_room, &g_cli_print_cells);
    s_room = NULL;
    cli_lock_give(&s_lock);
}

// `length` bytes of `text` stored among the listing's texts: where they start
static unsigned int cli_text_stored(CliPrintListing *listing, const char *text, unsigned long long length, int *full)
{
    if ((listing->text_used + length) > listing->text_room)
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

int cli_print_listing_read(const CliJson *json, unsigned int node, CliPrintListing **listing, VerbaLinea *err_msg)
{
    if (s_room == NULL)
    {
        mmgr_line_put(err_msg, "the listing could not be read: the cells cannot meet it");
        return 0;
    }
    CliPrintListing *const listing_at = &s_room->listing;
    int full = 0;
    unsigned int longest = 0u;
    listing_at->lines = 0u;
    listing_at->runs = 0u;
    listing_at->text_used = 0u;
    listing_at->tab = 4ull;
    const char *const title = cli_json_string(json, cli_json_member(json, node, "title"), NULL);
    cli_text_copied_or(listing_at->title, title, "");
    const unsigned int tab = cli_json_member(json, node, "tab");
    unsigned long long tab_value = 0ull;
    if (cli_json_is(json, tab, CFG_JSON_NUMBER))
    {
        const CfgJsonToken token = {CFG_JSON_NUMBER, json->node[tab].text,
                                    json->node[tab].text + json->node[tab].length, 0u, 0u};
        listing_at->tab = cfg_json_unsigned(json->text, &token, &tab_value) ? tab_value : listing_at->tab;
    }
    const unsigned int lines = cli_json_member(json, node, "lines");
    for (unsigned int item = cli_json_is(json, lines, CFG_JSON_ARRAY) ? json->node[lines].first : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json->node[item].next)
    {
        if (listing_at->lines >= listing_at->lines_room)
        {
            full = 1;
            break;
        }
        CliPrintLine *const line = &listing_at->line[listing_at->lines];
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
        line->first = listing_at->runs;
        line->runs = 0u;
        const unsigned int runs = cli_json_member(json, item, "runs");
        for (unsigned int run = cli_json_is(json, runs, CFG_JSON_ARRAY) ? json->node[runs].first : CLI_JSON_NONE;
             run != CLI_JSON_NONE; run = json->node[run].next)
        {
            if (listing_at->runs >= listing_at->runs_room)
            {
                full = 1;
                break;
            }
            CliPrintRun *const run_at = &listing_at->run[listing_at->runs];
            unsigned long long length = 0ull;
            unsigned long long color_length = 0ull;
            const char *const text = cli_json_string(json, cli_json_member(json, run, "text"), &length);
            const char *const color = cli_json_string(json, cli_json_member(json, run, "color"), &color_length);
            run_at->text = cli_text_stored(listing_at, (text != NULL) ? text : "", length, &full);
            run_at->length = (unsigned int)length;
            longest = (run_at->length > longest) ? run_at->length : longest;
            run_at->color = cli_text_stored(listing_at, (color != NULL) ? color : "", color_length, &full);
            run_at->color_length = (unsigned int)color_length;
            run_at->bold = cli_json_is(json, cli_json_member(json, run, "bold"), CFG_JSON_TRUE);
            run_at->italic = cli_json_is(json, cli_json_member(json, run, "italic"), CFG_JSON_TRUE);
            listing_at->runs += 1u;
            line->runs += 1u;
        }
        listing_at->lines += 1u;
    }
    // the letters a run is laid out from, a place for each byte of the longest run and its end
    full = full || ((longest + 1u) > s_room->letters_room);
    if (full)
    {
        mmgr_line_put(err_msg, "the listing contains more than orior_cli prints");
        return 0;
    }
    *listing = listing_at;
    return 1;
}

void cli_print_layout_read(const CliJson *json, unsigned int node, CliPrintLayout *layout)
{
    const char *const paper = cli_json_string(json, cli_json_member(json, node, "paper"), NULL);
    (void)cellul.copy(
        &(CatenaFinitaCfg){.dst = layout->paper, .src = (paper != NULL) ? paper : "a4", .cap = sizeof(layout->paper)});
    layout->landscape = cli_json_is(json, cli_json_member(json, node, "landscape"), CFG_JSON_TRUE);
    layout->gray = cli_json_is(json, cli_json_member(json, node, "gray"), CFG_JSON_TRUE);
    const char *const pages = cli_json_string(json, cli_json_member(json, node, "pages"), NULL);
    layout->has_pages = pages != NULL;
    cli_text_copied_or(layout->pages, pages, "");
    const unsigned int copies = cli_json_member(json, node, "copies");
    unsigned long long value = 1ull;
    if (cli_json_is(json, copies, CFG_JSON_NUMBER))
    {
        const CfgJsonToken token = {CFG_JSON_NUMBER, json->node[copies].text,
                                    json->node[copies].text + json->node[copies].length, 0u, 0u};
        value = (cfg_json_unsigned(json->text, &token, &value) && (value <= 0xffffffffull)) ? value : 1ull;
    }
    // checked against the most 32 bits represent above
    layout->copies = (unsigned int)value;
}

// whether a name is `word` in ASCII's either case
static int cli_named(const char *name, const char *word)
{
    return cellul.eq(&(CatenaFinitaCfg){.src = name, .other = word, .cap = CLI_CELLS_POOL, .ci = 1});
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
    *from += cli_text_past_any(*from, 0ull, (unsigned long long)(*to - *from), CLI_TEXT_WHITE);
    *to = *from + cli_text_back_past(*from, (unsigned long long)(*to - *from), CLI_TEXT_WHITE);
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
        end += cli_text_to_any(end, 0ull, (unsigned long long)(to - end), ",");
        const char *start = part;
        const char *stop = end;
        cli_trimmed(&start, &stop);
        const char *dash = start;
        dash += cli_text_to_any(dash, 0ull, (unsigned long long)(stop - dash), "-");
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

// The ranges stored as they are read
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
        // each is below the most an int represents, checked as it was read
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
    from += cli_text_past_any(from, 0ull, (unsigned long long)(to - from), "#");
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

// The piece being made ended where it contains text: put after the row's pieces
static void cli_piece_put(CliPrintLaid *laid, CliPrintRow *row, CliPrintPiece *piece)
{
    if (piece->length == 0u)
    {
        return;
    }
    if (laid->pieces >= laid->pieces_room)
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
    if (laid->rows < laid->rows_room)
    {
        laid->row[laid->rows] = *row;
        laid->rows += 1u;
    }
    else
    {
        laid->full = 1;
    }
    row->numbered = 0;
    row->number = 0ull;
    row->first = laid->pieces;
    row->pieces = 0u;
}

// one letter put after the piece being made, in UTF-8 among the layout's texts
static void cli_letter_put(CliPrintLaid *laid, CliPrintPiece *piece, unsigned int letter)
{
    // a letter is at most four bytes
    if ((laid->text_used + 4u) > laid->text_room)
    {
        laid->full = 1;
        return;
    }
    VerbaLinea line = {laid->text + laid->text_used, laid->text_room - laid->text_used, 0ull};
    cli_utf8_put(&line, letter);
    if (!mmgr_line_ok(&line))
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
    // a page contains far fewer rows than an unsigned int counts
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
                cli_regexp_letters(listing->text + run->text, run->length, letters, s_room->letters_room);
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
