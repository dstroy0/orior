// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// engine.c: the engine: nvcc's listing read and its operations, the lane operations, the engine's writing read, the
// ruleset's operations, the engine of a kernel, the orders, and the counts written
#include "../../../../../../includes/file_defs/klang/klang.h"
#include "stick_internal.h"

// 1 with the name of the kernel that the line `line` of nvcc's listing opens into `name`, `Function : <name>`, 0 where
// the line opens none
static int engine_function(StickPiece line, StickPiece *name)
{
    static const char s_function[] = "Function : ";
    const size_t spaced = stick_spaced(line.at, line.length);
    const size_t opening = sizeof(s_function) - 1u;
    if (((spaced + opening) > line.length) ||
        (EMBED_CALL(memor.cmp, MemoriaCfg, .src = &line.at[spaced], .other = s_function, .bytes = opening) != 0))
    {
        return 0;
    }
    name->at = &line.at[spaced + opening];
    name->length = EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = name->at, .cap = line.length - spaced - opening,
                              .other = s_space);
    return name->length != 0u;
}

// The kernels of the `text` of nvcc's listing and their operations, NOP left out, counted where `filling` is 0 and
// written where it is 1. The operations before the first kernel belong to none, and the reader leaves them out
static void engine_listing_made(const StickText *text, int filling, unsigned int *kernels, unsigned int *operations)
{
    *kernels = 0u;
    *operations = 0u;
    StickKernel *kernel = NULL;
    for (size_t at = 0u; at < text->size;)
    {
        StickPiece line;
        at = stick_line(text, at, &line);
        StickPiece piece;
        if (engine_function(line, &piece))
        {
            if (filling)
            {
                kernel = &s_kernel[*kernels];
                stick_put(kernel->name, sizeof(kernel->name), piece.at, piece.length,
                          "a kernel's name passes its room: ");
                kernel->operations.operation = &s_listed.operation[*operations];
                kernel->operations.count = 0u;
            }
            *kernels += 1u;
            continue;
        }
        if ((*kernels == 0u) || !stick_operation_of(line, &piece) || stick_is_nop(piece))
        {
            continue;
        }
        if (filling)
        {
            StickOperation *const operation = &s_listed.operation[*operations];
            stick_put(operation->text, sizeof(operation->text), piece.at, piece.length,
                      "an operation's name passes its room: ");
            kernel->operations.count += 1u;
        }
        *operations += 1u;
    }
}

// Each kernel's operations from cuobjdump's listing at `path`, NOP left out. A kernel that the listing names twice
// keeps the operations of the last, as the kernels go in the order of their names, each once
void stick_nvcc_read(const char *path)
{
    const StickText text = stick_text_read(path);
    unsigned int kernels = 0u;
    unsigned int operations = 0u;
    engine_listing_made(&text, 0, &kernels, &operations);
    s_kernel = (StickKernel *)stick_allocated(platform_cell_cleared(
        (kernels != 0u) ? kernels : 1u, sizeof(StickKernel), platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT));
    s_listed.operation = stick_operations_taken(operations);
    engine_listing_made(&text, 1, &s_kernels, &s_listed.count);
    stick_sorted(s_kernel, s_kernels, sizeof(StickKernel), stick_kernel_order);
    unsigned int kept = 0u;
    for (unsigned int at = 0u; at < s_kernels; at += 1u)
    {
        const int last = ((at + 1u) == s_kernels) || (stick_kernel_order(&s_kernel[at], &s_kernel[at + 1u]) != 0);
        if (last)
        {
            s_kernel[kept] = s_kernel[at];
            kept += 1u;
        }
    }
    s_kernels = kept;
}

// The operations of the `text` of a listing nvdisasm wrote, in order, NOP left out: their count. They go at
// `operations` where it is not NULL
static unsigned int engine_listed_operations(const StickText *text, StickOperation *operations)
{
    unsigned int count = 0u;
    for (size_t at = 0u; at < text->size;)
    {
        StickPiece line;
        at = stick_line(text, at, &line);
        StickPiece word;
        if (!stick_operation_of(line, &word) || stick_is_nop(word))
        {
            continue;
        }
        if (operations != NULL)
        {
            stick_put(operations[count].text, sizeof(operations[count].text), word.at, word.length,
                      "an operation's name passes its room: ");
        }
        count += 1u;
    }
    return count;
}

// 1 with the operation of the line `line` of a lane's text into `word`, 0 where the line is space, a comment, a
// directive or a label. The operation is the instruction's first word, its guard and its `;` cut
static int engine_lane_operation(StickPiece line, StickPiece *word)
{
    const size_t spaced = stick_spaced(line.at, line.length);
    const char *const text = &line.at[spaced];
    const size_t length = stick_end_outside(text, line.length - spaced, s_space);
    if ((length == 0u) || ((length >= 2u) && (text[0] == '/') && (text[1] == '/')) || (text[0] == '.') ||
        (text[length - 1u] == ':'))
    {
        return 0;
    }
    const size_t guard = stick_guard(text, length);
    const size_t characters =
        EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &text[guard], .cap = length - guard, .other = s_space);
    word->at = &text[guard];
    word->length = stick_end_outside(word->at, characters, ";");
    return 1;
}

// The operations of the lane's text at `path`, NOP left out, against `operations`, position by position: the count of
// them into `text`, and the count that agree into `agree`
static void engine_lane_against(const char *path, const StickOperations *operations, unsigned int *text,
                                unsigned int *agree)
{
    const StickText lane = stick_text_read(path);
    *text = 0u;
    *agree = 0u;
    for (size_t at = 0u; at < lane.size;)
    {
        StickPiece line;
        at = stick_line(&lane, at, &line);
        StickPiece word;
        if (!engine_lane_operation(line, &word) || stick_is_nop(word))
        {
            continue;
        }
        if (*text < operations->count)
        {
            const char *const listed = operations->operation[*text].text;
            const size_t size = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = listed, .cap = STICK_WORD_BYTES);
            *agree += ((size == word.length) &&
                       (EMBED_CALL(memor.cmp, MemoriaCfg, .src = listed, .other = word.at, .bytes = size) == 0))
                          ? 1u
                          : 0u;
        }
        *text += 1u;
    }
}

// the path `<directory>/<number><suffix>` put into the `room` bytes at `path`
static void engine_path(char *path, size_t room, const char *directory, StickPiece number, const char *suffix)
{
    VerbaLinea line = mmgr_line_over(path, room);
    mmgr_line_put(&line, directory);
    mmgr_line_put(&line, "/");
    (void)mmgr_line_put_n(&line, number.at, number.length);
    mmgr_line_put(&line, suffix);
    if (!mmgr_line_ok(&line))
    {
        stick_refused("a path passes its room: ", directory,
                      EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = directory, .cap = PLATFORM_TEXT_TO_TERMINATOR));
    }
    (void)mmgr_line_finish(&line);
}

// 1 where the `length` bytes at `text` are the terminated `word`
static int engine_is(StickPiece piece, const char *word)
{
    const size_t size = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = word, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    return (piece.length == size) &&
           (EMBED_CALL(memor.cmp, MemoriaCfg, .src = piece.at, .other = word, .bytes = size) == 0);
}

static int engine_order(const void *left, const void *right)
{
    return stick_text_order(((const StickEngine *)left)->name, ((const StickEngine *)right)->name);
}

// One line of the engine's run, `line`, read into `entry`: 1, or 0 where the line is the head or no line of five
// cells. Where the kernel answers, its operations as nvdisasm reads them back from `directory` and how many agree with
// the lane's text, position by position
static int engine_line_read(StickPiece line, const char *directory, StickEngine *entry)
{
    StickPiece cell[5];
    if ((stick_cells(line, cell, 5u) != 5u) || engine_is(cell[0], "number"))
    {
        return 0;
    }
    VerbaLinea name = mmgr_line_over(entry->name, sizeof(entry->name));
    mmgr_line_put(&name, s_prefix);
    (void)mmgr_line_put_n(&name, cell[0].at, cell[0].length);
    if (!mmgr_line_ok(&name))
    {
        stick_refused("a kernel's name passes its room: ", cell[0].at, cell[0].length);
    }
    (void)mmgr_line_finish(&name);
    stick_put(entry->answer, sizeof(entry->answer), cell[1].at, cell[1].length, "an answer passes its room: ");
    // a count of steps is far below 2^32, and the tab after it ends the read of its digits
    entry->steps =
        (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = cell[2].at, .end = NULL, .base = 10);
    stick_put(entry->note, sizeof(entry->note), cell[4].at, cell[4].length, "a note passes its room: ");
    entry->operations.operation = NULL;
    entry->operations.count = 0u;
    entry->agree = 0u;
    entry->text = 0u;
    if (!engine_is(cell[1], "answered"))
    {
        return 1;
    }
    char path[PLATFORM_PATH_BYTES];
    engine_path(path, sizeof(path), directory, cell[0], ".dis");
    const StickText listing = stick_text_read(path);
    const unsigned int count = engine_listed_operations(&listing, NULL);
    entry->operations.operation = stick_operations_taken(count);
    entry->operations.count = engine_listed_operations(&listing, entry->operations.operation);
    engine_path(path, sizeof(path), directory, cell[0], ".sass");
    engine_lane_against(path, &entry->operations, &entry->text, &entry->agree);
    return 1;
}

// Each kernel's line of the engine's run at `table`, in one cell taken at the count of its lines. A kernel that the
// table names twice keeps its last line, as the lines go in the order of their kernels, each once
void stick_engine_read(const char *table, const char *directory)
{
    const StickText text = stick_text_read(table);
    unsigned int lines = 0u;
    for (size_t at = 0u; at < text.size;)
    {
        StickPiece line;
        at = stick_line(&text, at, &line);
        lines += 1u;
    }
    s_engine = (StickEngine *)stick_allocated(platform_cell_cleared(
        (lines != 0u) ? lines : 1u, sizeof(StickEngine), platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT));
    s_engines = 0u;
    for (size_t at = 0u; at < text.size;)
    {
        StickPiece line;
        at = stick_line(&text, at, &line);
        s_engines += engine_line_read(line, directory, &s_engine[s_engines]) ? 1u : 0u;
    }
    stick_sorted(s_engine, s_engines, sizeof(StickEngine), engine_order);
    unsigned int kept = 0u;
    for (unsigned int at = 0u; at < s_engines; at += 1u)
    {
        const int last = ((at + 1u) == s_engines) || (engine_order(&s_engine[at], &s_engine[at + 1u]) != 0);
        if (last)
        {
            s_engine[kept] = s_engine[at];
            kept += 1u;
        }
    }
    s_engines = kept;
}

// The operation that one instruction of a form's text writes, the `length` bytes at `text`, into `word`: 1, or 0
// where it writes none. A written tab is a space. The code cuts the space at either end and the guard, and a comment, a
// directive and a label write none. The operation is the first word, its `;` cut
static int engine_form_operation(const char *text, size_t length, char *room, size_t room_bytes, StickPiece *word)
{
    VerbaLinea line = mmgr_line_over(room, room_bytes);
    size_t at = 0u;
    while (at < length)
    {
        const char *const tab = EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = &text[at], .cap = length - at,
                                           .other = "\\t", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE);
        // a found tab lies within the instruction, at or past `at`
        const size_t run = (tab != NULL) ? (size_t)(tab - &text[at]) : (length - at);
        (void)mmgr_line_put_n(&line, &text[at], run);
        at += run;
        if (tab != NULL)
        {
            mmgr_line_ch(&line, ' ');
            at += 2u;
        }
    }
    if (!mmgr_line_ok(&line))
    {
        stick_refused("an instruction of a form passes its room: ", text, length);
    }
    const size_t size = line.at;
    (void)mmgr_line_finish(&line);
    const size_t spaced = stick_spaced(room, size);
    const char *const start = &room[spaced];
    const size_t trimmed = stick_end_outside(start, size - spaced, s_space);
    const size_t guard = stick_form_guard(start, trimmed);
    const char *const first = &start[guard];
    const size_t words = trimmed - guard;
    if ((words == 0u) || ((words >= 2u) && (first[0] == '/') && (first[1] == '/')) || (first[0] == '.') ||
        (first[words - 1u] == ':'))
    {
        return 0;
    }
    const size_t characters =
        EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = first, .cap = words, .other = s_space);
    word->at = first;
    word->length = stick_end_outside(first, characters, ";");
    return word->length != 0u;
}

// The operations that the form line `line` writes, counted into `*count`, and put at `operations` from `*count` where
// it is not NULL. A form line is `form <name> ... = <text>`, and its text's instructions are apart by a written newline
static void engine_form_line(StickPiece line, StickOperation *operations, unsigned int *count)
{
    static const char s_form[] = "form ";
    if ((line.length < (sizeof(s_form) - 1u)) ||
        (EMBED_CALL(memor.cmp, MemoriaCfg, .src = line.at, .other = s_form, .bytes = sizeof(s_form) - 1u) != 0))
    {
        return;
    }
    const char *const equals = EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = line.at, .cap = line.length,
                                          .other = " = ", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE);
    if (equals == NULL)
    {
        return;
    }
    // the ` = ` lies within the line
    size_t at = (size_t)(equals - line.at) + 3u;
    for (;;)
    {
        const char *const newline =
            EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = &line.at[at], .cap = line.length - at, .other = "\\n",
                       .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE);
        // a found newline lies within the line, at or past `at`
        const size_t length = (newline != NULL) ? (size_t)(newline - &line.at[at]) : (line.length - at);
        char room[STICK_INSTRUCTION_BYTES];
        StickPiece word;
        if (engine_form_operation(&line.at[at], length, room, sizeof(room), &word))
        {
            if (operations != NULL)
            {
                stick_put(operations[*count].text, sizeof(operations[*count].text), word.at, word.length,
                          "an operation's name passes its room: ");
            }
            *count += 1u;
        }
        if (newline == NULL)
        {
            break;
        }
        at += length + 2u;
    }
}

// the start of the line that holds the place `at` of `text`
static size_t engine_line_start(const StickText *text, size_t at)
{
    const char *const newline =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = text->bytes, .cap = at, .byte = (uint8_t)'\n');
    // a found newline lies before `at`
    return (newline != NULL) ? ((size_t)(newline - text->bytes) + 1u) : 0u;
}

// the start of the line after the one that holds the place `at` of `text`, or the end of the text
static size_t engine_line_next(const StickText *text, size_t at)
{
    const char *const newline =
        (const char *)EMBED_CALL(memor.chr, MemoriaCfg, .src = &text->bytes[at], .bytes = text->size - at, .val = '\n');
    // a found newline lies at or past `at`
    return (newline != NULL) ? ((size_t)(newline - text->bytes) + 1u) : text->size;
}

// The operations the form lines of `text` write, from the place `from` to the place `past`, counted into `*count` and
// put at `operations` where it is not NULL
static void engine_form_lines(const StickText *text, size_t from, size_t past, StickOperation *operations,
                              unsigned int *count)
{
    for (size_t at = from; at < past;)
    {
        StickPiece line;
        at = stick_line(text, at, &line);
        engine_form_line(line, operations, count);
    }
}

// The operations of the four texts at `texts`: the first two whole, a .krs each, and the blocks of the ruleset's
// language of the last two. Each block runs from the line after its opener to the line of its closer. The count goes
// into `*count`, and the operations at `operations` where it is not NULL
static void engine_rulesets_made(const StickText *texts, StickOperation *operations, unsigned int *count)
{
    *count = 0u;
    for (unsigned int file = 0u; file < 4u; file += 1u)
    {
        const StickText *const text = &texts[file];
        if (file < 2u)
        {
            engine_form_lines(text, 0u, text->size, operations, count);
            continue;
        }
        size_t from = 0u;
        KlangBlock block;
        while ((from < text->size) && klang_block_find(text->bytes, text->size, from, "krs", &block))
        {
            const size_t opener = engine_line_start(text, block.opened);
            const size_t first = engine_line_next(text, block.opened);
            const size_t closer = engine_line_start(text, block.text_end);
            engine_form_lines(text, first, (closer > first) ? closer : first, operations, count);
            const size_t past = (closer > opener) ? closer : first;
            from = (past > from) ? past : engine_line_next(text, from);
        }
    }
}

// The operations the SASS ruleset's forms write, each instruction of each form's text, its guard cut, into s_ruleset.
// The forms are those of sass.krs at `path`, then those of sm_86.krs beside it. Then those of the blocks of the
// ruleset's language of sm_86.kdm and of sass.ksc beside it
void stick_ruleset_operations(const char *path)
{
    // the folder sass.krs is in, to its last separator, and its stem, to the dot of its extension
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = path, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    const char *const forward = EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = path, .cap = length, .byte = '/');
    const char *const back = EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = path, .cap = length, .byte = '\\');
    const char *const slash = ((forward != NULL) && ((back == NULL) || (forward > back))) ? forward : back;
    // the separator and the dot lie within the path
    const size_t folder = (slash != NULL) ? (size_t)(slash - path) : 0u;
    const char *const dot = EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = path, .cap = length, .byte = '.');
    const size_t stem = ((dot != NULL) && ((slash == NULL) || (dot > slash))) ? (size_t)(dot - path) : length;
    static const char *const s_beside[3] = {"sm_86.krs", "sm_86.kdm", ".ksc"};
    char files[4][PLATFORM_PATH_BYTES];
    for (unsigned int file = 0u; file < 4u; file += 1u)
    {
        VerbaLinea line = mmgr_line_over(files[file], sizeof(files[file]));
        if (file == 0u)
        {
            mmgr_line_put(&line, path);
        }
        else if (file < 3u)
        {
            (void)mmgr_line_put_n(&line, path, folder);
            mmgr_line_put(&line, (slash != NULL) ? "/" : "");
            mmgr_line_put(&line, s_beside[file - 1u]);
        }
        else
        {
            (void)mmgr_line_put_n(&line, path, stem);
            mmgr_line_put(&line, s_beside[2]);
        }
        if (!mmgr_line_ok(&line))
        {
            stick_refused("a path passes its room: ", path, length);
        }
        (void)mmgr_line_finish(&line);
    }
    StickText texts[4];
    for (unsigned int file = 0u; file < 4u; file += 1u)
    {
        texts[file] = stick_text_read(files[file]);
    }
    unsigned int count = 0u;
    engine_rulesets_made(texts, NULL, &count);
    s_ruleset.operation = stick_operations_taken(count);
    engine_rulesets_made(texts, s_ruleset.operation, &s_ruleset.count);
}

// the line of the engine's run for the kernel named `name`, or NULL where the run has none
const StickEngine *stick_engine_of(const char *name)
{
    unsigned int low = 0u;
    unsigned int high = s_engines;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (stick_text_order(s_engine[middle].name, name) < 0)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    return ((low < s_engines) && (stick_text_order(s_engine[low].name, name) == 0)) ? &s_engine[low] : NULL;
}

// <0, 0 or >0 as the kernel at `left` is named before, with or after the one at `right`
int stick_kernel_order(const void *left, const void *right)
{
    return stick_text_order(((const StickKernel *)left)->name, ((const StickKernel *)right)->name);
}

// `text` with every `|` written `\|`, put on `line`
void stick_escaped(VerbaLinea *line, const char *text)
{
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    size_t at = 0u;
    while (at < length)
    {
        const size_t run =
            EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &text[at], .cap = length - at, .other = "|");
        (void)mmgr_line_put_n(line, &text[at], run);
        at += run;
        if (at < length)
        {
            mmgr_line_put(line, "\\|");
            at += 1u;
        }
    }
}

// the count of each operation of `operations` into `count`, one for each name of the table
void stick_counted(const StickOperations *operations, unsigned int *count)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = count, .bytes = s_names * sizeof(unsigned int), .val = 0);
    for (unsigned int at = 0u; at < operations->count; at += 1u)
    {
        count[operations->operation[at].number] += 1u;
    }
}

// Each operation that `count` holds more of than `less`, `<name> <more>`, apart by ", ", in the order of their names,
// put on `line`. `less` is NULL where the code takes none away
void stick_counts_written(VerbaLinea *line, const unsigned int *count, const unsigned int *less)
{
    int first = 1;
    for (unsigned int operation = 0u; operation < s_names; operation += 1u)
    {
        const unsigned int taken = (less != NULL) ? less[operation] : 0u;
        if (count[operation] > taken)
        {
            mmgr_line_put(line, first ? "" : ", ");
            mmgr_line_put(line, s_name[operation].text);
            mmgr_line_ch(line, ' ');
            mmgr_line_decimal(line, count[operation] - taken, 1u);
            first = 0;
        }
    }
}
