// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// page.c: a listing read, the page it prints on, and the listing laid out in pages, each line with its number and a
// long line going on under itself
#include "../command_line/callback_cfg.h"
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

void cli_print_take(void)
{
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
}

typedef struct
{
    CliPrintListing *listing;
    const char *text;
    unsigned long long length;
    int *full;
    const char *name;
    const char *word;
    const char **from;
    const char **to;
    const char *from_value;
    const char *to_value;
    unsigned long long most;
    unsigned long long *value;
    const CliPrintLayout *layout;
    int (*each)(const CliEachFromDashToCfg *args);
    void *context;
    char digit;
    CliPrintLaid *laid;
    CliPrintRow *row;
    CliPrintPiece *piece;
    unsigned int letter;
} PageCtx;
// `length` bytes of `text` stored among the listing's texts: where they start
EMBED_INLINE unsigned int cli_text_store(const PageCtx *args)
{
    int *full = args->full;
    if ((args->listing->text_used + args->length) > args->listing->text_room)
    {
        *full = 1;
        return 0u;
    }
    const unsigned int at = args->listing->text_used;
    memor.cpy(&(MemoriaCfg){.dst = args->listing->text + at, .src = args->text, .bytes = args->length});
    // the texts are below CLI_PRINT_TEXT, which an unsigned int counts
    args->listing->text_used += (unsigned int)args->length;
    return at;
}

int cli_print_listing_read(const CliPrintJsonCfg *args)
{
    CliPrintListing **listing = args->listing_pointer;
    if (s_room == NULL)
    {
        mmgr_line_put(args->err_msg, "the listing could not be read: the cells cannot meet it");
        return 0;
    }
    CliPrintListing *const listing_at = &s_room->listing;
    int full = 0;
    unsigned int longest = 0u;
    listing_at->lines = 0u;
    listing_at->runs = 0u;
    listing_at->text_used = 0u;
    listing_at->tab = 4ull;
    const char *const title =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                      .object = args->node, .name = "title"),
                   .length_pointer = NULL);
    EMBED_CALL(cli_text_out.copied_or, CliTextOutCfg, .out_room = listing_at->title, .text = title, .otherwise = "");
    const unsigned int tab = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                        .object = args->node, .name = "tab");
    unsigned long long tab_value = 0ull;
    if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json, .node = tab,
                   .kind = CFG_JSON_NUMBER))
    {
        const CfgJsonToken token = {CFG_JSON_NUMBER, args->json->node[tab].text,
                                    args->json->node[tab].text + args->json->node[tab].length, 0u, 0u};
        listing_at->tab = cfg_json_unsigned(args->json->text, &token, &tab_value) ? tab_value : listing_at->tab;
    }
    const unsigned int lines = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                          .json_read = args->json, .object = args->node, .name = "lines");
    for (unsigned int item = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                        .json_read = args->json, .node = lines, .kind = CFG_JSON_ARRAY)
                                 ? args->json->node[lines].first
                                 : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = args->json->node[item].next)
    {
        if (listing_at->lines >= listing_at->lines_room)
        {
            full = 1;
            break;
        }
        CliPrintLine *const line = &listing_at->line[listing_at->lines];
        const unsigned int number = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                               .json_read = args->json, .object = item, .name = "number");
        unsigned long long value = 0ull;
        line->numbered = 0;
        if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json, .node = number,
                       .kind = CFG_JSON_NUMBER))
        {
            const CfgJsonToken token = {CFG_JSON_NUMBER, args->json->node[number].text,
                                        args->json->node[number].text + args->json->node[number].length, 0u, 0u};
            line->numbered = cfg_json_unsigned(args->json->text, &token, &value);
        }
        line->number = value;
        line->first = listing_at->runs;
        line->runs = 0u;
        const unsigned int runs = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                             .json_read = args->json, .object = item, .name = "runs");
        for (unsigned int run = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                           .json_read = args->json, .node = runs, .kind = CFG_JSON_ARRAY)
                                    ? args->json->node[runs].first
                                    : CLI_JSON_NONE;
             run != CLI_JSON_NONE; run = args->json->node[run].next)
        {
            if (listing_at->runs >= listing_at->runs_room)
            {
                full = 1;
                break;
            }
            CliPrintRun *const run_at = &listing_at->run[listing_at->runs];
            unsigned long long length = 0ull;
            unsigned long long color_length = 0ull;
            const char *const text =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                           .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = args->json, .object = run, .name = "text"),
                           .length_pointer = &length);
            const char *const color =
                EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                           .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = args->json, .object = run, .name = "color"),
                           .length_pointer = &color_length);
            run_at->text = EMBED_CALL(cli_text_store, PageCtx, .listing = listing_at,
                                      .text = (text != NULL) ? text : "", .length = length, .full = &full);
            run_at->length = (unsigned int)length;
            longest = (run_at->length > longest) ? run_at->length : longest;
            run_at->color = EMBED_CALL(cli_text_store, PageCtx, .listing = listing_at,
                                       .text = (color != NULL) ? color : "", .length = color_length, .full = &full);
            run_at->color_length = (unsigned int)color_length;
            run_at->bold = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json,
                                      .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                         .json_read = args->json, .object = run, .name = "bold"),
                                      .kind = CFG_JSON_TRUE);
            run_at->italic =
                EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json,
                           .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                              .json_read = args->json, .object = run, .name = "italic"),
                           .kind = CFG_JSON_TRUE);
            listing_at->runs += 1u;
            line->runs += 1u;
        }
        listing_at->lines += 1u;
    }
    // the letters a run is laid out from, a place for each byte of the longest run and its end
    full = full || ((longest + 1u) > s_room->letters_room);
    if (full)
    {
        mmgr_line_put(args->err_msg, "the listing contains more than orior_cli prints");
        return 0;
    }
    *listing = listing_at;
    return 1;
}

void cli_print_layout_read(const CliPrintJsonNodeCfg *args)
{
    const char *const paper =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                      .object = args->node, .name = "paper"),
                   .length_pointer = NULL);
    (void)cellul.copy(&(CatenaFinitaCfg){
        .dst = args->layout_out->paper, .src = (paper != NULL) ? paper : "a4", .cap = sizeof(args->layout_out->paper)});
    args->layout_out->landscape =
        EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                      .object = args->node, .name = "landscape"),
                   .kind = CFG_JSON_TRUE);
    args->layout_out->gray =
        EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                      .object = args->node, .name = "gray"),
                   .kind = CFG_JSON_TRUE);
    const char *const pages =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = args->json,
                                      .object = args->node, .name = "pages"),
                   .length_pointer = NULL);
    args->layout_out->has_pages = pages != NULL;
    EMBED_CALL(cli_text_out.copied_or, CliTextOutCfg, .out_room = args->layout_out->pages, .text = pages,
               .otherwise = "");
    const unsigned int copies = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                           .json_read = args->json, .object = args->node, .name = "copies");
    unsigned long long value = 1ull;
    if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json, .node = copies,
                   .kind = CFG_JSON_NUMBER))
    {
        const CfgJsonToken token = {CFG_JSON_NUMBER, args->json->node[copies].text,
                                    args->json->node[copies].text + args->json->node[copies].length, 0u, 0u};
        value = (cfg_json_unsigned(args->json->text, &token, &value) && (value <= 0xffffffffull)) ? value : 1ull;
    }
    // checked against the most 32 bits represent above
    args->layout_out->copies = (unsigned int)value;
}

// whether a name is `word` in ASCII's either case
EMBED_INLINE int cli_by_name(const PageCtx *args)
{
    return cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = args->word, .cap = CLI_CELLS_POOL, .ci = 1});
}

void cli_print_size(const CliPrintLayoutWidthCfg *args)
{
    float *width = args->width;
    float *height = args->height;
    const int letter = EMBED_CALL(cli_by_name, PageCtx, .name = args->layout->paper, .word = "letter");
    const float wide = letter ? 612.0f : 595.276f;
    const float high = letter ? 792.0f : 841.89f;
    *width = args->layout->landscape ? high : wide;
    *height = args->layout->landscape ? wide : high;
}

const char *cli_print_media(const CliPrintLayoutCfg *args)
{
    return EMBED_CALL(cli_by_name, PageCtx, .name = args->layout->paper, .word = "letter") ? "na_letter_8.5x11in"
                                                                                           : "iso_a4_210x297mm";
}

// The bytes from `from` to `to` trimmed of ASCII's spaces
EMBED_INLINE void cli_trim(const PageCtx *args)
{
    const char **from = args->from;
    const char **to = args->to;
    *from += EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg, .text = *from, .at = 0ull,
                        .size = (unsigned long long)(*to - *from), .set = CLI_TEXT_WHITE);
    *to = *from + EMBED_CALL(cli_text_at_size_classes.back_past, CliTextAtSizeClassesCfg, .text = *from,
                             .at = (unsigned long long)(*to - *from), .set = CLI_TEXT_WHITE);
}

// the whole number the bytes from `from` to `to` write, as an unsigned number reads, at most `most`: 1, or 0 where
// they write none
EMBED_INLINE int cli_parse(const PageCtx *args)
{
    const char *from = args->from_value;
    unsigned long long *value = args->value;
    from += ((from < args->to_value) && (from[0] == '+')) ? 1 : 0;
    if (from == args->to_value)
    {
        return 0;
    }
    unsigned long long total = 0ull;
    for (const char *at = from; at < args->to_value; at += 1)
    {
        const unsigned long long digit = (unsigned long long)(at[0] - '0');
        if ((at[0] < '0') || (at[0] > '9') || (total > ((args->most - digit) / 10ull)))
        {
            return 0;
        }
        total = (total * 10ull) + digit;
    }
    *value = total;
    return 1;
}

// each part of the pages named, between commas, handed its two ends and the place of its `-`, NULL where it has none
EMBED_INLINE int cli_parts(const PageCtx *args)
{
    const char *from = args->layout->pages;
    const char *to = from + cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL});
    EMBED_CALL(cli_trim, PageCtx, .from = &from, .to = &to);
    if (!args->layout->has_pages || (from == to))
    {
        return -1;
    }
    const char *part = from;
    while (part <= to)
    {
        const char *end = part;
        end += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = end, .at = 0ull,
                          .size = (unsigned long long)(to - end), .set = ",");
        const char *start = part;
        const char *stop = end;
        EMBED_CALL(cli_trim, PageCtx, .from = &start, .to = &stop);
        const char *dash = start;
        dash += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = dash, .at = 0ull,
                           .size = (unsigned long long)(stop - dash), .set = "-");
        if (EMBED_CALL(args->each, CliEachFromDashToCfg, .context = args->context, .from = start,
                       .dash = (dash < stop) ? dash : NULL, .to = stop))
        {
            return 1;
        }
        part = end + 1;
    }
    return 0;
}

static int cli_prints_part(const CliEachFromDashToCfg *args)
{
    const char *from = args->from;
    const char *to = args->to;
    const unsigned long long page = *(const unsigned long long *)args->context;
    if (args->dash == NULL)
    {
        unsigned long long value = 0ull;
        return EMBED_CALL(cli_parse, PageCtx, .from_value = from, .to_value = to, .most = ~0ull, .value = &value) &&
               (value == page);
    }
    const char *low_from = from;
    const char *low_to = args->dash;
    const char *high_from = args->dash + 1;
    const char *high_to = to;
    EMBED_CALL(cli_trim, PageCtx, .from = &low_from, .to = &low_to);
    EMBED_CALL(cli_trim, PageCtx, .from = &high_from, .to = &high_to);
    unsigned long long low = 1ull;
    unsigned long long high = ~0ull;
    low = EMBED_CALL(cli_parse, PageCtx, .from_value = low_from, .to_value = low_to, .most = ~0ull, .value = &low)
              ? low
              : 1ull;
    high = EMBED_CALL(cli_parse, PageCtx, .from_value = high_from, .to_value = high_to, .most = ~0ull, .value = &high)
               ? high
               : ~0ull;
    return (page >= low) && (page <= high);
}

int cli_print_prints(const CliPrintCfg *args)
{
    unsigned long long page = args->page_u64;
    return EMBED_CALL(cli_parts, PageCtx, .layout = args->layout, .each = cli_prints_part, .context = &page) != 0;
}

// The ranges stored as they are read
typedef struct
{
    int *low;
    int *high;
    unsigned int room;
    unsigned int count;
} CliRanges;

static int cli_range_part(const CliEachFromDashToCfg *args)
{
    const char *from = args->from;
    const char *to = args->to;
    CliRanges *const ranges = (CliRanges *)args->context;
    unsigned long long low = 0ull;
    unsigned long long high = 0x7fffffffull;
    if (args->dash == NULL)
    {
        if (!EMBED_CALL(cli_parse, PageCtx, .from_value = from, .to_value = to, .most = 0x7fffffffull, .value = &low))
        {
            return 0;
        }
        high = low;
    }
    else
    {
        const char *low_from = from;
        const char *low_to = args->dash;
        const char *high_from = args->dash + 1;
        const char *high_to = to;
        EMBED_CALL(cli_trim, PageCtx, .from = &low_from, .to = &low_to);
        EMBED_CALL(cli_trim, PageCtx, .from = &high_from, .to = &high_to);
        if (!EMBED_CALL(cli_parse, PageCtx, .from_value = low_from, .to_value = low_to, .most = 0x7fffffffull,
                        .value = &low))
        {
            return 0;
        }
        high = EMBED_CALL(cli_parse, PageCtx, .from_value = high_from, .to_value = high_to, .most = 0x7fffffffull,
                          .value = &high)
                   ? high
                   : 0x7fffffffull;
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

unsigned int cli_print_ranges(const CliPrintLayoutLowCfg *args)
{
    CliRanges ranges = {args->low, args->high, args->room, 0u};
    EMBED_CALL(cli_parts, PageCtx, .layout = args->layout, .each = cli_range_part, .context = &ranges);
    return ranges.count;
}

// a hex digit's value, or 16 where the byte is none
EMBED_INLINE unsigned int cli_hex(const PageCtx *args)
{
    return ((args->digit >= '0') && (args->digit <= '9'))   ? (unsigned int)(args->digit - '0')
           : ((args->digit >= 'a') && (args->digit <= 'f')) ? (unsigned int)(args->digit - 'a') + 10u
           : ((args->digit >= 'A') && (args->digit <= 'F')) ? (unsigned int)(args->digit - 'A') + 10u
                                                            : 16u;
}

void cli_print_color(const CliPrintTextCfg *args)
{
    const char *from = args->text;
    const char *to = args->text + args->length;
    EMBED_CALL(cli_trim, PageCtx, .from = &from, .to = &to);
    from += EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg, .text = from, .at = 0ull,
                       .size = (unsigned long long)(to - from), .set = "#");
    const unsigned long long size = (unsigned long long)(to - from);
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        const unsigned long long place = 2ull * at;
        const unsigned int high = (place + 1ull < size) ? EMBED_CALL(cli_hex, PageCtx, .digit = from[place]) : 16u;
        const unsigned int low = (place + 1ull < size) ? EMBED_CALL(cli_hex, PageCtx, .digit = from[place + 1u]) : 16u;
        // a byte is two hex digits, 0x22 where they are none
        args->rgb[at] = ((size >= 6ull) && (high < 16u) && (low < 16u)) ? (unsigned char)((high << 4u) | low) : 0x22u;
    }
    if (args->gray)
    {
        const float level =
            roundf((0.2126f * (float)args->rgb[0]) + (0.7152f * (float)args->rgb[1]) + (0.0722f * (float)args->rgb[2]));
        // a lightness of three bytes is a byte
        args->rgb[0] = (unsigned char)level;
        args->rgb[1] = args->rgb[0];
        args->rgb[2] = args->rgb[0];
    }
}

// The piece being made ended where it contains text: put after the row's pieces
EMBED_INLINE void cli_piece_put(const PageCtx *args)
{
    if (args->piece->length == 0u)
    {
        return;
    }
    if (args->laid->pieces >= args->laid->pieces_room)
    {
        args->laid->full = 1;
        args->piece->length = 0u;
        return;
    }
    args->laid->piece[args->laid->pieces] = *args->piece;
    args->laid->pieces += 1u;
    args->row->pieces += 1u;
    args->piece->text = args->laid->text_used;
    args->piece->length = 0u;
}

// the row being made put after the rows, and a row with no number begun after it
EMBED_INLINE void cli_row_put(const PageCtx *args)
{
    if (args->laid->rows < args->laid->rows_room)
    {
        args->laid->row[args->laid->rows] = *args->row;
        args->laid->rows += 1u;
    }
    else
    {
        args->laid->full = 1;
    }
    args->row->numbered = 0;
    args->row->number = 0ull;
    args->row->first = args->laid->pieces;
    args->row->pieces = 0u;
}

// one letter put after the piece being made, in UTF-8 among the layout's texts
EMBED_INLINE void cli_letter_put(const PageCtx *args)
{
    unsigned int letter = args->letter;
    // a letter is at most four bytes
    if ((args->laid->text_used + 4u) > args->laid->text_room)
    {
        args->laid->full = 1;
        return;
    }
    VerbaLinea line = {args->laid->text + args->laid->text_used, args->laid->text_room - args->laid->text_used, 0ull};
    EMBED_CALL(cli_line_letter_internal.utf8_put, CliLineLetterCtx, .line = &line, .letter = letter);
    if (!mmgr_line_ok(&line))
    {
        args->laid->full = 1;
        return;
    }
    // a letter is at most four bytes
    args->laid->text_used += (unsigned int)line.at;
    args->piece->length += (unsigned int)line.at;
}

const CliPrintLaid *cli_print_lay_out(const CliPrintLayoutListingOutCfg *args)
{
    unsigned int *const letters = s_room->letters;
    CliPrintLaid *const laid = &s_room->laid;
    laid->rows = 0u;
    laid->pieces = 0u;
    laid->text_used = 0u;
    laid->full = 0;
    float width = 0.0f;
    float height = 0.0f;
    EMBED_CALL(cli_print_layout_width.size, CliPrintLayoutWidthCfg, .layout = args->layout, .width = &width,
               .height = &height);
    unsigned long long widest = 0ull;
    for (unsigned int at = 0u; at < args->listing->lines; at += 1u)
    {
        widest = (args->listing->line[at].numbered && (args->listing->line[at].number > widest))
                     ? args->listing->line[at].number
                     : widest;
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
    const unsigned long long tab = args->listing->tab;
    for (unsigned int index = 0u; index < args->listing->lines; index += 1u)
    {
        const CliPrintLine *const line = &args->listing->line[index];
        CliPrintRow row = {line->numbered, line->number, laid->pieces, 0u};
        unsigned long long column = 0ull;
        for (unsigned int each = 0u; each < line->runs; each += 1u)
        {
            const CliPrintRun *const run = &args->listing->run[line->first + each];
            CliPrintPiece piece;
            memor.set(&(MemoriaCfg){.dst = &piece, .val = 0, .bytes = sizeof(piece)});
            EMBED_CALL(cli_print_text.color, CliPrintTextCfg, .text = args->listing->text + run->color,
                       .length = run->color_length, .gray = args->layout->gray, .rgb = piece.color);
            piece.bold = run->bold;
            piece.italic = run->italic;
            piece.column = column;
            piece.text = laid->text_used;
            const unsigned long long count =
                EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = args->listing->text + run->text,
                           .size = run->length, .letters = letters, .room_u64 = s_room->letters_room);
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
                        EMBED_CALL(cli_piece_put, PageCtx, .laid = laid, .row = &row, .piece = &piece);
                        EMBED_CALL(cli_row_put, PageCtx, .laid = laid, .row = &row);
                        column = 0ull;
                        piece.column = 0ull;
                    }
                    EMBED_CALL(cli_letter_put, PageCtx, .laid = laid, .piece = &piece,
                               .letter = (letter == '\t') ? ' ' : letter);
                    column += 1ull;
                }
            }
            EMBED_CALL(cli_piece_put, PageCtx, .laid = laid, .row = &row, .piece = &piece);
        }
        EMBED_CALL(cli_row_put, PageCtx, .laid = laid, .row = &row);
    }
    laid->pages = (laid->rows + laid->rows_per_page - 1u) / laid->rows_per_page;
    return laid;
}
