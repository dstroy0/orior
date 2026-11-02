// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// document.c: a text's lines of UTF-16 units as the page splits them, and find over them: text sought by the sift, and
// a pattern, or text sought without case outside ASCII, by the regular expressions the colorer reads
#include "document.h"
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../text/regexp.h"
#include "../text/sift.h"
#include "../text/text.h"
#include "../text/vector.h"

// A text's lines joined, each ended by "\n", and the unit each starts at, the units' count last, in a cell sized to
// them
typedef struct
{
    unsigned short *unit;
    unsigned long long used;
    unsigned long long *start;
    unsigned int lines;
} CliJoined;
// A search's work state, each part in a cell sized to what it holds: the text's lines joined, the units sought, and the
// matches found, work state for as many as the joined units hold of the units sought, CLI_FIND_MATCHES at the most
typedef struct
{
    CliJoined joined;
    unsigned short *required;
    unsigned long long (*match)[3];
    unsigned long long match_capacity;
} CliFindWorkState;
typedef struct
{
    CliLines *lines_out;
    unsigned int letter;
    const CliLines *lines;
    CliJoined *joined;
    unsigned short unit;
    const unsigned short *line;
    unsigned long long length;
    unsigned long long from;
    unsigned long long to;
    const CliDocNeedle *needle;
    VerbaLinea *out;
    unsigned int line_u32;
    unsigned int *letters;
    unsigned int *cols;
    unsigned long long most;
    CliFound3 each;
    void *context;
    VerbaLinea *err_msg;
    CliFindWorkState *work_state;
    const CliHistogram *histogram;
    unsigned long long size;
} DocumentCtx;

// a letter put after the lines' units, as UTF-16
EMBED_INLINE void cli_unit_append(const DocumentCtx *args)
{
    const unsigned long long need = (args->letter > 0xffffu) ? 2ull : 1ull;
    if ((args->lines_out->used + need) > args->lines_out->units_max)
    {
        args->lines_out->full = 1;
        return;
    }
    if (args->letter > 0xffffu)
    {
        const unsigned int rest = args->letter - 0x10000u;
        // each half of a pair of surrogates is below 2^16
        args->lines_out->unit[args->lines_out->used] = (unsigned short)(0xd800u + (rest >> 10u));
        args->lines_out->unit[args->lines_out->used + 1ull] = (unsigned short)(0xdc00u + (rest & 0x3ffu));
        EMBED_CALL(cli_lines_histogram_append, CliLinesHistogramAppendCfg, .lines = args->lines_out,
                   .unit = args->lines_out->unit[args->lines_out->used + 1ull]);
    }
    else
    {
        // a letter of the basic plane is below 2^16
        args->lines_out->unit[args->lines_out->used] = (unsigned short)args->letter;
    }
    EMBED_CALL(cli_lines_histogram_append, CliLinesHistogramAppendCfg, .lines = args->lines_out,
               .unit = args->lines_out->unit[args->lines_out->used]);
    args->lines_out->used += need;
}

void cli_document_lines_get(const CliDocumentTextSizeCfg *args)
{
    CliLines *lines = args->lines_out;
    lines->used = 0ull;
    lines->lines = 0u;
    lines->full = 0;
    lines->crlf = 0;
    memor.set(&(MemoriaCfg){.dst = &lines->histogram, .val = 0, .bytes = sizeof(lines->histogram)});
    unsigned long long at = 0ull;
    for (;;)
    {
        const char *const newline =
            (at < args->size) ? memor.chr(&(MemoriaCfg){.src = args->text + at, .val = '\n', .bytes = args->size - at})
                              : NULL;
        // a newline found lies in the text, at or past `at`
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - args->text) : args->size;
        unsigned long long length = end - at;
        const int returned = (length > 0ull) && (args->text[end - 1ull] == '\r');
        lines->crlf = lines->crlf || (returned && (newline != NULL));
        length -= returned ? 1ull : 0ull;
        if (lines->lines >= lines->lines_max)
        {
            lines->full = 1;
            return;
        }
        lines->start[lines->lines] = lines->used;
        if (lines->begun != NULL)
        {
            lines->begun[lines->lines] = CLI_DOC_UNCOLORED;
            lines->ended[lines->lines] = CLI_DOC_UNCOLORED;
            lines->regions[lines->lines] = 0u;
            lines->runs[lines->lines] = CLI_DOC_NO_RUNS;
        }
        for (unsigned long long place = at; place < (at + length);)
        {
            unsigned int letter = 0u;
            place += EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                                .text_bytes = (const unsigned char *)args->text, .length = at + length, .at = place,
                                .letter = &letter);
            EMBED_CALL(cli_unit_append, DocumentCtx, .lines_out = lines, .letter = letter);
        }
        lines->length[lines->lines] = lines->used - lines->start[lines->lines];
        lines->lines += 1u;
        if (newline == NULL)
        {
            return;
        }
        at = end + 1ull;
    }
}

CliShape cli_document_shape_get(const CliDocumentLinesOutCfg *args)
{
    CliShape shape = {args->lines->lines, 0ull};
    for (unsigned int at = 0u; at < args->lines->lines; at += 1u)
    {
        shape.units += args->lines->length[at];
    }
    return shape;
}

unsigned int cli_document_unit_letter(const CliDocumentUnitsCfg *args)
{
    unsigned long long *consumed = args->consumed;
    unsigned int letter = args->units[args->at];
    *consumed = 1ull;
    if ((letter >= 0xd800u) && (letter <= 0xdbffu) && ((args->at + 1ull) < args->count) &&
        (args->units[args->at + 1ull] >= 0xdc00u) && (args->units[args->at + 1ull] <= 0xdfffu))
    {
        letter = 0x10000u + ((letter - 0xd800u) << 10u) + (args->units[args->at + 1ull] - 0xdc00u);
        *consumed = 2ull;
    }
    else if ((letter >= 0xd800u) && (letter <= 0xdfffu))
    {
        letter = 0xfffdu;
    }
    return letter;
}

void cli_document_units_write(const CliDocumentUnitsCountCfg *args)
{
    char bytes[8];
    mmgr_line_ch(args->out, '"');
    unsigned long long at = 0ull;
    while (at < args->count)
    {
        unsigned long long consumed = 0ull;
        const unsigned int letter =
            EMBED_CALL(cli_document_units.unit_letter, CliDocumentUnitsCfg, .units = args->units, .count = args->count,
                       .at = at, .consumed = &consumed);
        at += consumed;
        VerbaLinea one = {bytes, sizeof(bytes), 0ull};
        EMBED_CALL(cli_line_letter_internal.utf8_append, CliLineLetterCtx, .line = &one, .letter = letter);
        EMBED_CALL(cli_json_tree_length_out.json_escaped_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
                   .text_bytes = (const unsigned char *)bytes, .length = one.at);
    }
    mmgr_line_ch(args->out, '"');
}

void cli_document_units_utf8(const CliDocumentUnitsCountCfg *args)
{
    unsigned long long at = 0ull;
    while (at < args->count)
    {
        unsigned long long consumed = 0ull;
        const unsigned int letter =
            EMBED_CALL(cli_document_units.unit_letter, CliDocumentUnitsCfg, .units = args->units, .count = args->count,
                       .at = at, .consumed = &consumed);
        at += consumed;
        EMBED_CALL(cli_line_letter_internal.utf8_append, CliLineLetterCtx, .line = args->out, .letter = letter);
    }
}

// the lines joined into `joined`, in a cell taken for them, given back with cli_joined_let_go: 1, or 0 where the cells
// cannot meet them
EMBED_INLINE int cli_joined_get(const DocumentCtx *args)
{
    const CliLines *lines = args->lines;
    CliJoined *joined = args->joined;
    unsigned long long units = lines->lines;
    for (unsigned int at = 0u; at < lines->lines; at += 1u)
    {
        units += lines->length[at];
    }
    // the units' bytes rounded up to a boundary of the starts after them
    const unsigned long long unit_bytes = ((units * sizeof(unsigned short)) + 7ull) & ~7ull;
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(
        unit_bytes + (((unsigned long long)lines->lines + 1ull) * sizeof(unsigned long long)), &g_cli_editor_cells);
    *joined = (CliJoined){(unsigned short *)(void *)cell, 0ull, (unsigned long long *)(void *)(cell + unit_bytes), 0u};
    if (cell == NULL)
    {
        return 0;
    }
    for (unsigned int at = 0u; at < lines->lines; at += 1u)
    {
        joined->start[at] = joined->used;
        memor.cpy(&(MemoriaCfg){.dst = joined->unit + joined->used,
                                .src = lines->unit + lines->start[at],
                                .bytes = lines->length[at] * sizeof(unsigned short)});
        joined->used += lines->length[at];
        joined->unit[joined->used] = '\n';
        joined->used += 1ull;
    }
    joined->start[lines->lines] = joined->used;
    joined->lines = lines->lines;
    return 1;
}

// the cell the lines were joined in given back
EMBED_INLINE void cli_joined_let_go(const DocumentCtx *args)
{
    CliJoined *joined = args->joined;
    g_cli_cells->persistent_buf_release(joined->unit, &g_cli_editor_cells);
    *joined = (CliJoined){NULL, 0ull, NULL, 0u};
}

// a word's letter as find reads one for whole words, where it is ASCII: a letter, a digit, `_` or `$`: 1 or 0, or -1
// for a unit outside ASCII, which the regular expressions' tables read
EMBED_INLINE int cli_word_unit(const DocumentCtx *args)
{
    if (args->unit >= 0x80u)
    {
        return -1;
    }
    return ((args->unit >= 'a') && (args->unit <= 'z')) || ((args->unit >= 'A') && (args->unit <= 'Z')) ||
           ((args->unit >= '0') && (args->unit <= '9')) || (args->unit == '_') || (args->unit == '$');
}

// whether the span [from, to) of a line is a whole word: 1 or 0, or -1 where a unit beside it is outside ASCII
EMBED_INLINE int cli_span_is_whole_word(const DocumentCtx *args)
{
    const int before =
        (args->from == 0ull) ? 0 : EMBED_CALL(cli_word_unit, DocumentCtx, .unit = args->line[args->from - 1ull]);
    const int after =
        (args->to == args->length) ? 0 : EMBED_CALL(cli_word_unit, DocumentCtx, .unit = args->line[args->to]);
    if ((before < 0) || (after < 0))
    {
        return -1;
    }
    return !before && !after;
}

// the most matches found that are buffered until the sift's search is known to be whole
#define CLI_FIND_MATCHES (1u << 20u)

// whether `letter` is a letter of a word as find reads one for whole words: a letter, a number, `_` or `$`
EMBED_INLINE int cli_word_letter(const DocumentCtx *args)
{
    unsigned int letter = args->letter;
    return (letter == '_') || (letter == '$') ||
           EMBED_CALL(cli_regexp_letter.property, CliRegexpLetterCfg, .property = CLI_REGEXP_LETTER,
                      .letter = letter) ||
           EMBED_CALL(cli_regexp_letter.property, CliRegexpLetterCfg, .property = CLI_REGEXP_NUMBER, .letter = letter);
}

// the query of `needle` written into `out` as a pattern's source: the query itself where it is a pattern, else with
// each sign a pattern reads escaped
EMBED_INLINE void cli_pattern_source(const DocumentCtx *args)
{
    VerbaLinea *out = args->out;
    if (args->needle->regex)
    {
        mmgr_line_put(out, args->needle->query);
        return;
    }
    static const char s_signs[] = ".*+?^${}()|[]\\";
    EMBED_CALL(cli_text_size_set.append_mark, CliTextSizeSetCfg, .out = out, .text = args->needle->query,
               .set = s_signs, .mark = '\\');
}

// the line `line` of `lines` as code points into `letters`, each one's col in UTF-16 units into `cols` and the line's
// length after the last: how many
EMBED_INLINE unsigned long long cli_line_letters(const DocumentCtx *args)
{
    const unsigned short *const unit = args->lines->unit + args->lines->start[args->line_u32];
    const unsigned long long length = args->lines->length[args->line_u32];
    unsigned long long count = 0ull;
    for (unsigned long long place = 0ull; place < length; count += 1ull)
    {
        const int paired = (unit[place] >= 0xd800u) && (unit[place] < 0xdc00u) && ((place + 1ull) < length) &&
                           (unit[place + 1ull] >= 0xdc00u) && (unit[place + 1ull] < 0xe000u);
        // a line's cols stand below its length, which an unsigned int counts
        args->cols[count] = (unsigned int)place;
        args->letters[count] =
            paired ? (0x10000u + ((unit[place] - 0xd800u) << 10u) + (unit[place + 1ull] - 0xdc00u)) : unit[place];
        place += paired ? 2ull : 1ull;
    }
    // the length stands below 2^32, as a line's cols do
    args->cols[count] = (unsigned int)length;
    return count;
}

// the matches of `needle` read as a pattern by the regular expressions the colorer reads, under the `u` flag, and `i`
// where it runs without case, the `u` let go where the pattern reads only without it and whole words are not sought, as
// the page's find reads one: each line matched from each letter a match can start at, a match of no letters passed
// over and one that is no whole word where whole words are sought, each handed to `each` with its line and its cols in
// UTF-16 units, the lines searched whole until one ends with `most` found: 1, or 0 with why given where the pattern
// does not read or the cells cannot meet it
static int cli_found_pattern(const DocumentCtx *args)
{
    VerbaLinea *err_msg = args->err_msg;
    char source[2u * CLI_REGEXP_LETTERS];
    VerbaLinea written = {source, sizeof(source), 0ull};
    EMBED_CALL(cli_pattern_source, DocumentCtx, .needle = args->needle, .out = &written);
    if (mmgr_line_finish(&written) == 0ull)
    {
        mmgr_line_put(err_msg, "the query is too long to read as a pattern");
        return 0;
    }
    unsigned long long longest = 0ull;
    for (unsigned int line = 0u; line < args->lines->lines; line += 1u)
    {
        longest = (args->lines->length[line] > longest) ? args->lines->length[line] : longest;
    }
    const unsigned long long bytes = sizeof(CliRegexpWorkState) + (2ull * (longest + 1ull) * sizeof(unsigned int));
    unsigned char *const cell = g_cli_cells->temporary_buf_alloc(bytes, &g_cli_editor_cells);
    if (cell == NULL)
    {
        mmgr_line_put(err_msg, "the text could not be searched: the cells cannot meet it");
        return 0;
    }
    unsigned int *const letters = (unsigned int *)(void *)(cell + sizeof(CliRegexpWorkState));
    unsigned int *const cols = letters + longest + 1ull;
    CliRegexp regexp;
    EMBED_CALL(cli_regexp_capacity.capacity, CliRegexpWorkStateCfg, .regexp_out = &regexp,
               .work_state = (CliRegexpWorkState *)(void *)cell);
    const unsigned long long output = err_msg->at;
    int read = EMBED_CALL(cli_regexp_source.read, CliRegexpSourceCfg, .regexp_out = &regexp, .source = source,
                          .flags = args->needle->match_case ? "u" : "iu", .err_msg = err_msg);
    if (!read && !args->needle->word)
    {
        err_msg->at = output;
        EMBED_CALL(cli_regexp_capacity.capacity, CliRegexpWorkStateCfg, .regexp_out = &regexp,
                   .work_state = (CliRegexpWorkState *)(void *)cell);
        read = EMBED_CALL(cli_regexp_source.read, CliRegexpSourceCfg, .regexp_out = &regexp, .source = source,
                          .flags = args->needle->match_case ? "" : "i", .err_msg = err_msg);
    }
    unsigned long long found = 0ull;
    for (unsigned int line = 0u; read && (line < args->lines->lines) && (found < args->most); line += 1u)
    {
        const unsigned long long count = EMBED_CALL(cli_line_letters, DocumentCtx, .lines = args->lines,
                                                    .line_u32 = line, .letters = letters, .cols = cols);
        for (unsigned long long at = 0ull; at < count;)
        {
            CliRegexpSpan groups[CLI_REGEXP_GROUPS + 1u];
            const int starts = (!regexp.anchored || (at == 0ull)) &&
                               (regexp.first.empty || EMBED_CALL(cli_regexp.allows, CliRegexpCfg,
                                                                 .first = &regexp.first, .letter = letters[at]));
            const int matched = starts &&
                                EMBED_CALL(cli_regexp_text_u_32s.match_into, CliRegexpTextU32sCfg, .regexp = &regexp,
                                           .text_u32s = letters, .length = count, .at = at, .groups = groups) &&
                                (groups[0].to > groups[0].from);
            const unsigned long long to = matched ? groups[0].to : at;
            const int whole =
                !args->needle->word ||
                (((at == 0ull) || !EMBED_CALL(cli_word_letter, DocumentCtx, .letter = letters[at - 1ull])) &&
                 ((to == count) || !EMBED_CALL(cli_word_letter, DocumentCtx, .letter = letters[to])));
            if (matched && whole)
            {
                EMBED_CALL(args->each, CliFound3Cfg, .context = args->context, .line = line, .from = cols[at],
                           .to = cols[to]);
                found += 1ull;
                at = to;
                continue;
            }
            at += 1ull;
        }
    }
    g_cli_cells->persistent_buf_release(cell, &g_cli_editor_cells);
    return read;
}

// the search of `size` units sought through the work state's lines joined, the lines `lines`, each match handed to
// `each`, searched again as a pattern where whole words are sought and a unit beside a match is outside ASCII: 1, or 0
// with why given
EMBED_INLINE int cli_found_each(const DocumentCtx *args);

int cli_document_find_each(const CliDocumentLinesCfg *args)
{
    int ascii = 1;
    ascii =
        ascii && EMBED_CALL(cli_text_size_byte.ascii, CliTextSizeByteCfg, .text = args->needle->query,
                            .size = cellul.len(&(CatenaFinitaCfg){.src = args->needle->query, .cap = CLI_CELLS_POOL}));
    if (args->needle->regex || (!args->needle->match_case && !ascii))
    {
        return EMBED_CALL(cli_found_pattern, DocumentCtx, .lines = args->lines, .needle = args->needle,
                          .most = args->most, .each = args->each, .context = args->context, .err_msg = args->err_msg);
    }
    const unsigned long long query_size =
        cellul.len(&(CatenaFinitaCfg){.src = args->needle->query, .cap = CLI_CELLS_POOL});
    // a unit of the query takes a byte of it at the least, and the unit after the last is written as a pair's second
    CliFindWorkState work_state = {
        {NULL, 0ull, NULL, 0u},
        g_cli_cells->persistent_buf_alloc((query_size + 1ull) * sizeof(unsigned short), &g_cli_editor_cells),
        NULL,
        0ull};
    if (work_state.required == NULL)
    {
        mmgr_line_put(args->err_msg, "the text could not be searched: the cells cannot meet it");
        return 0;
    }
    unsigned long long size = 0ull;
    for (unsigned long long at = 0ull; at < query_size;)
    {
        unsigned int letter = 0u;
        at += EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                         .text_bytes = (const unsigned char *)args->needle->query, .length = query_size, .at = at,
                         .letter = &letter);
        const unsigned long long need = (letter > 0xffffu) ? 2ull : 1ull;
        // each unit of a letter is below 2^16
        work_state.required[size] =
            (unsigned short)((letter > 0xffffu) ? (0xd800u + ((letter - 0x10000u) >> 10u)) : letter);
        work_state.required[size + 1ull] = (unsigned short)(0xdc00u + ((letter - 0x10000u) & 0x3ffu));
        size += need;
    }
    int newline = 0;
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        newline = newline || (work_state.required[at] == '\n');
    }
    // the search reads the text's stored census, with the newline the join puts after each line, and folded as the
    // search folds where it runs without case
    CliHistogram histogram = args->lines->histogram;
    histogram.occurrences['\n'] += args->lines->lines;
    histogram.occurrences[0] += args->lines->lines;
    histogram.total += 2ull * args->lines->lines;
    if (!args->needle->match_case)
    {
        CliHistogram folded;
        EMBED_CALL(cli_sift.histogram_fold, CliSiftCfg, .exact = &histogram, .folded = &folded);
        histogram = folded;
    }
    const int needle_any = (size != 0ull) && !newline;
    const int joined =
        needle_any && EMBED_CALL(cli_joined_get, DocumentCtx, .lines = args->lines, .joined = &work_state.joined);
    if (joined)
    {
        const unsigned long long fit = (work_state.joined.used / size) + 1ull;
        work_state.match_capacity = (fit < CLI_FIND_MATCHES) ? fit : CLI_FIND_MATCHES;
        work_state.match = g_cli_cells->persistent_buf_alloc(work_state.match_capacity * sizeof(*work_state.match),
                                                             &g_cli_editor_cells);
    }
    const int with_capacity = !needle_any || (joined && (work_state.match != NULL));
    if (!with_capacity)
    {
        mmgr_line_put(args->err_msg, "the text could not be searched: the cells cannot meet it");
    }
    const int done =
        with_capacity &&
        (!needle_any || EMBED_CALL(cli_found_each, DocumentCtx, .work_state = &work_state, .lines = args->lines,
                                   .histogram = &histogram, .size = size, .needle = args->needle, .most = args->most,
                                   .each = args->each, .context = args->context, .err_msg = args->err_msg));
    g_cli_cells->persistent_buf_release(work_state.match, &g_cli_editor_cells);
    EMBED_CALL(cli_joined_let_go, DocumentCtx, .joined = &work_state.joined);
    g_cli_cells->persistent_buf_release(work_state.required, &g_cli_editor_cells);
    return done;
}

// A search's walk over the joined units as the sift hands each offset it finds the needle at: the matches taken in
// order, each past the one before, on a unit's boundary, a whole word where one is sought, as the page searches each
// line whole and stops before the first line it starts with `most` found
typedef struct
{
    CliFindWorkState *work_state;
    const CliDocNeedle *needle;
    unsigned long long size;
    unsigned long long most;
    unsigned long long found;
    unsigned int line;
    unsigned int last;
    unsigned long long next;
    int stopped;
    int patterned;
} CliFindWalk;

static void cli_found_at(const CliEachAtCfg *args)
{
    CliFindWalk *const walk = (CliFindWalk *)args->context;
    const unsigned long long hit = args->at / 2ull;
    // a hit between two units, or inside the match before, is none
    if (walk->stopped || walk->patterned || ((args->at % 2ull) != 0ull) || (hit < walk->next))
    {
        return;
    }
    const CliJoined *const joined = &walk->work_state->joined;
    while (joined->start[walk->line + 1u] <= hit)
    {
        walk->line += 1u;
    }
    if ((walk->last != 0xffffffffu) && (walk->line > walk->last))
    {
        walk->stopped = 1;
        return;
    }
    const unsigned long long from = hit - joined->start[walk->line];
    const unsigned long long to = from + walk->size;
    if (walk->needle->word)
    {
        const unsigned long long length = joined->start[walk->line + 1u] - joined->start[walk->line] - 1ull;
        const int whole =
            EMBED_CALL(cli_span_is_whole_word, DocumentCtx, .line = joined->unit + joined->start[walk->line],
                       .length = length, .from = from, .to = to);
        if (whole < 0)
        {
            walk->patterned = 1;
            return;
        }
        if (!whole)
        {
            walk->next = hit + 1ull;
            return;
        }
    }
    if (walk->found < walk->work_state->match_capacity)
    {
        walk->work_state->match[walk->found][0] = walk->line;
        walk->work_state->match[walk->found][1] = from;
        walk->work_state->match[walk->found][2] = to;
    }
    walk->found += 1ull;
    if ((walk->found >= walk->most) && (walk->last == 0xffffffffu))
    {
        walk->last = walk->line;
    }
    walk->next = hit + walk->size;
}

EMBED_INLINE int cli_found_each(const DocumentCtx *args)
{
    // a search without case folds the work state's units and the needle's first, each unit to the one the page's search
    // folds it to, and the sift reads them as they are: the units searched as their bytes, the needle's laid the same
    // way, a match starting on an even byte; a text within one of the sift's windows goes by Horspool's shifts and a
    // longer one by the sift's plan
    if (!args->needle->match_case)
    {
        for (unsigned long long at = 0ull; at < args->work_state->joined.used; at += 1ull)
        {
            args->work_state->joined.unit[at] =
                EMBED_CALL(cli_vector_unit.fold, CliVectorUnitCfg, .unit = args->work_state->joined.unit[at]);
        }
        for (unsigned long long at = 0ull; at < args->size; at += 1ull)
        {
            args->work_state->required[at] =
                EMBED_CALL(cli_vector_unit.fold, CliVectorUnitCfg, .unit = args->work_state->required[at]);
        }
    }
    CliFindWalk walk = {args->work_state, args->needle, args->size, args->most, 0ull, 0u, 0xffffffffu, 0ull, 0, 0};
    (void)EMBED_CALL(cli_sift_length_fold.find_all_with, CliSiftLengthFoldCfg, .histogram_read = args->histogram,
                     .text = (const unsigned char *)args->work_state->joined.unit,
                     .length = args->work_state->joined.used * 2ull,
                     .needle = (const unsigned char *)args->work_state->required, .needle_length = args->size * 2ull,
                     .fold = 0, .found = cli_found_at, .context = &walk);
    if (walk.patterned)
    {
        return cli_found_pattern(args);
    }
    for (unsigned long long one = 0ull; (one < walk.found) && (one < args->work_state->match_capacity); one += 1ull)
    {
        EMBED_CALL(args->each, CliFound3Cfg, .context = args->context, .line = args->work_state->match[one][0],
                   .from = args->work_state->match[one][1], .to = args->work_state->match[one][2]);
    }
    return 1;
}

// A match written as `[line, from, to]`, a comma before it where it is not the first
typedef struct
{
    VerbaLinea *out;
    unsigned long long written;
} CliMatchesOut;

static void cli_match_write(const CliFound3Cfg *args)
{
    CliMatchesOut *const matches = (CliMatchesOut *)args->context;
    mmgr_line_put(matches->out, (matches->written != 0ull) ? ",[" : "[");
    mmgr_line_decimal(matches->out, args->line, 0u);
    mmgr_line_ch(matches->out, ',');
    mmgr_line_decimal(matches->out, args->from, 0u);
    mmgr_line_ch(matches->out, ',');
    mmgr_line_decimal(matches->out, args->to, 0u);
    mmgr_line_ch(matches->out, ']');
    matches->written += 1ull;
}

int cli_document_find_in(const CliDocumentLinesOutCfg *args)
{
    // the matches written into `out` as they come, taken back out where the search is refused
    const unsigned long long start = args->out->at;
    mmgr_line_ch(args->out, '[');
    CliMatchesOut matches = {args->out, 0ull};
    const int found =
        EMBED_CALL(cli_document_lines.find_each, CliDocumentLinesCfg, .lines = args->lines, .needle = args->needle,
                   .most = args->most, .each = cli_match_write, .context = &matches, .err_msg = args->err_msg);
    mmgr_line_ch(args->out, ']');
    args->out->at = found ? args->out->at : start;
    return found;
}
