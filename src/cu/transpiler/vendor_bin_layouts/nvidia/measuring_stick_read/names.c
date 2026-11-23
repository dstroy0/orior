// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// names.c: the names: a refusal, the order of two texts and a sort, a file read and its lines, the space, a guard and a
// form's guard, the operation of a line, a text put in its record, the cells of a line, and the table of names made
#include "stick_internal.h"

StickName *s_name = NULL;
unsigned int s_names = 0u;
StickKernel *s_kernel = NULL;
unsigned int s_kernels = 0u;
StickEngine *s_engine = NULL;
unsigned int s_engines = 0u;
StickOperations s_listed = {NULL, 0u};
StickOperations s_ruleset = {NULL, 0u};

// `why`, then the `length` bytes at `piece`, written to the errors, and the run ended
void stick_refused(const char *why, const char *piece, size_t length)
{
    static const char s_head[] = "orior_measuring_stick_read: ";
    platform_terminal_write(1, s_head, (unsigned long long)(sizeof(s_head) - 1u));
    platform_terminal_write(
        1, why,
        (unsigned long long)EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = why, .cap = PLATFORM_TEXT_TO_TERMINATOR));
    platform_terminal_write(1, piece, (unsigned long long)length);
    platform_terminal_write(1, "\n", 1ULL);
    platform_process_ended(2);
}

// `pointer`, or the run ended where the pool gave no cell
void *stick_allocated(void *pointer)
{
    if (pointer == NULL)
    {
        stick_refused("the pool gave no cell", "", 0u);
    }
    return pointer;
}

// the order of the terminated texts `left` and `right`: below 0 where `left` comes first, 0 where they are the same,
// above 0 where `right` does
int stick_text_order(const char *left, const char *right)
{
    const size_t left_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = left, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    const size_t right_length =
        EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = right, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    const size_t reach = ((left_length < right_length) ? left_length : right_length) + 1u;
    const size_t differs =
        EMBED_CALL(cellul.diff, CatenaFinitaCfg, .src = left, .cap = reach, .other = right, .ci = EMBED_FALSE);
    if (differs >= reach)
    {
        return 0;
    }
    // a byte of a text read as unsigned, and its order as a signed count
    return (int)(unsigned char)left[differs] - (int)(unsigned char)right[differs];
}

// The `count` elements of `size` bytes at `elements` put in the order of `order`, stable. The sort goes through a spare
// cell of the temporary tier, which goes back as the sort ends
void stick_sorted(void *elements, unsigned int count, size_t size, int (*order)(const void *left, const void *right))
{
    if (count < 2u)
    {
        return;
    }
    void *const spare = stick_allocated(platform_cell_taken((unsigned long long)count * (unsigned long long)size,
                                                            platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY));
    (void)EMBED_CALL(memor.sort, MemoriaCfg, .dst = elements, .bytes = size, .scratch = spare, .count = count,
                     .order = order);
    platform_cell_released(spare, platform_place_g_program_owner);
}

// the file at `path` as the run stores it (qry_member_file_blob). A file that is neither the run's nor the disk's is
// empty, and the reader reads no line of it
StickText stick_text_read(const char *path)
{
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ULL;
    StickText text = {"", 0u};
    if (qry_member_file_blob(path, &bytes, &size) && (bytes != NULL))
    {
        text.bytes = (const char *)bytes;
        // a file the run holds whole is at max what a size_t counts
        text.size = (size_t)size;
    }
    return text;
}

// The line of `text` at `at` into `line`, and the place of the line after it. The line ends at its newline, with one
// carriage return before it cut. A line also ends at a byte 0, as a terminated text ends there
size_t stick_line(const StickText *text, size_t at, StickPiece *line)
{
    const char *const start = &text->bytes[at];
    const char *const newline =
        (const char *)EMBED_CALL(memor.chr, MemoriaCfg, .src = start, .bytes = text->size - at, .val = '\n');
    // the newline lies within the text, at or past the line's start
    size_t length = (newline != NULL) ? (size_t)(newline - start) : (text->size - at);
    const size_t next = at + length + ((newline != NULL) ? 1u : 0u);
    length -= ((length != 0u) && (start[length - 1u] == '\r')) ? 1u : 0u;
    const char *const zero = (const char *)EMBED_CALL(memor.chr, MemoriaCfg, .src = start, .bytes = length, .val = 0);
    // a byte 0 lies within the line
    line->at = start;
    line->length = (zero != NULL) ? (size_t)(zero - start) : length;
    return next;
}

// the bytes of space at the start of the `length` bytes at `text`
size_t stick_spaced(const char *text, size_t length)
{
    return EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = text, .cap = length, .other = s_space);
}

// The end of the last run of bytes outside `set` in the `length` bytes at `text`, or 0 where every byte is in `set`.
// The walk jumps from run to run, and the end leaves out the bytes of `set` after that run
size_t stick_end_outside(const char *text, size_t length, const char *set)
{
    size_t end = 0u;
    size_t at = 0u;
    while (at < length)
    {
        const size_t run =
            EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &text[at], .cap = length - at, .other = set);
        at += run;
        end = (run != 0u) ? at : end;
        const size_t inside =
            EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = &text[at], .cap = length - at, .other = set);
        at += inside;
        if ((run == 0u) && (inside == 0u))
        {
            break;
        }
    }
    return end;
}

// the bytes of a guard at the start of the `length` bytes at `text`, `@!?U?P[T0-9]+` and the space after it, or 0
// where none is there
size_t stick_guard(const char *text, size_t length)
{
    if ((length == 0u) || (text[0] != '@'))
    {
        return 0u;
    }
    size_t at = 1u;
    at += ((at < length) && (text[at] == '!')) ? 1u : 0u;
    at += ((at < length) && (text[at] == 'U')) ? 1u : 0u;
    if ((at >= length) || (text[at] != 'P'))
    {
        return 0u;
    }
    at += 1u;
    const size_t marks =
        EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = &text[at], .cap = length - at, .other = "T0123456789");
    const size_t space = (marks != 0u) ? stick_spaced(&text[at + marks], length - at - marks) : 0u;
    return (space != 0u) ? (at + marks + space) : 0u;
}

// the bytes of a form's guard at the start of the `length` bytes at `text`, `@!?{?[A-Za-z0-9_]+}?` and the space after
// it, or 0 where none is there
size_t stick_form_guard(const char *text, size_t length)
{
    if ((length == 0u) || (text[0] != '@'))
    {
        return 0u;
    }
    size_t at = 1u;
    at += ((at < length) && (text[at] == '!')) ? 1u : 0u;
    at += ((at < length) && (text[at] == '{')) ? 1u : 0u;
    const size_t name = EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = &text[at], .cap = length - at,
                                   .other = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_");
    if (name == 0u)
    {
        return 0u;
    }
    at += name;
    at += ((at < length) && (text[at] == '}')) ? 1u : 0u;
    const size_t space = stick_spaced(&text[at], length - at);
    return (space != 0u) ? (at + space) : 0u;
}

// 1 with the operation of the instruction line `line` of a listing into `word`, 0 where the line holds none. The line
// is `/*<hex>*/`, then the instruction up to its `;`. The code cuts the instruction's guard, and its first word is the
// operation
int stick_operation_of(StickPiece line, StickPiece *word)
{
    const char *const text = line.at;
    const size_t length = line.length;
    size_t at = stick_spaced(text, length);
    if (((at + 2u) > length) || (text[at] != '/') || (text[at + 1u] != '*'))
    {
        return 0;
    }
    at += 2u;
    const size_t digits =
        EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = &text[at], .cap = length - at, .other = "0123456789abcdef");
    at += digits;
    if ((digits < 4u) || ((at + 2u) > length) || (text[at] != '*') || (text[at + 1u] != '/'))
    {
        return 0;
    }
    at += 2u;
    const size_t space = stick_spaced(&text[at], length - at);
    at += space;
    const char *const semicolon =
        EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = &text[at], .cap = length - at, .byte = (uint8_t)';');
    if ((space == 0u) || (semicolon == NULL))
    {
        return 0;
    }
    // the instruction, the space before its `;` cut; the `;` lies within the line, at or past `at`
    const char *const instruction = &text[at];
    const size_t size = stick_end_outside(instruction, (size_t)(semicolon - instruction), s_space);
    const size_t guard = stick_guard(instruction, size);
    const size_t spaced = guard + stick_spaced(&instruction[guard], size - guard);
    const size_t first = EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &instruction[spaced], .cap = size - spaced,
                                    .other = s_space);
    word->at = &instruction[spaced];
    word->length = first;
    return first != 0u;
}

// 1 where `word` is NOP
int stick_is_nop(StickPiece word)
{
    return (word.length == 3u) && (EMBED_CALL(memor.cmp, MemoriaCfg, .src = word.at, .other = "NOP", .bytes = 3u) == 0);
}

// The `length` bytes at `text` put into the `room` bytes at `into`, terminated. A text past the room ends the run, with
// `what` and the text as the reason
void stick_put(char *into, size_t room, const char *text, size_t length, const char *what)
{
    if (length >= room)
    {
        stick_refused(what, text, length);
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = into, .src = text, .bytes = length);
    into[length] = '\0';
}

// `count` operations in one cell of the persistent tier, at least one
StickOperation *stick_operations_taken(unsigned int count)
{
    return (StickOperation *)stick_allocated(platform_cell_cleared(
        (count != 0u) ? count : 1u, sizeof(StickOperation), platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT));
}

// The tab-apart cells of `line`, at max `max` of them, and the last has the rest of the line. The answer is their
// count, each cell a piece of the line at `cell`
unsigned int stick_cells(StickPiece line, StickPiece *cell, unsigned int max)
{
    unsigned int count = 0u;
    size_t at = 0u;
    while (count < max)
    {
        const size_t rest = line.length - at;
        const size_t length = ((count + 1u) == max) ? rest
                                                    : EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &line.at[at],
                                                                 .cap = rest, .other = "\t");
        cell[count].at = &line.at[at];
        cell[count].length = length;
        count += 1u;
        if (((at + length) >= line.length) || (line.at[at + length] != '\t'))
        {
            break;
        }
        at += length + 1u;
    }
    return count;
}

static int names_order(const void *left, const void *right)
{
    return stick_text_order(((const StickName *)left)->text, ((const StickName *)right)->text);
}

// the place of the name `text` among the names, which hold it
static unsigned int names_place(const char *text)
{
    unsigned int low = 0u;
    unsigned int high = s_names;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (stick_text_order(s_name[middle].text, text) < 0)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    return low;
}

// the names of `operations` put at `names` from `*count`, and `*count` past them
static void names_gathered(const StickOperations *operations, StickName *names, unsigned int *count)
{
    for (unsigned int at = 0u; at < operations->count; at += 1u)
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = names[*count].text, .src = operations->operation[at].text,
                   .bytes = sizeof(names[*count].text));
        *count += 1u;
    }
}

// each operation of `operations` numbered by its name's place among the names
static void names_numbered(StickOperations *operations)
{
    for (unsigned int at = 0u; at < operations->count; at += 1u)
    {
        operations->operation[at].number = names_place(operations->operation[at].text);
    }
}

// The table of names: every name the listing, the forms and the engine's read back write, in one cell at their count.
// The code sorts them once and keeps each once. Then each operation gets its name's place in the table as its number
void stick_names_made(void)
{
    unsigned int total = s_listed.count + s_ruleset.count;
    for (unsigned int engine = 0u; engine < s_engines; engine += 1u)
    {
        total += s_engine[engine].operations.count;
    }
    s_name = (StickName *)stick_allocated(platform_cell_cleared(
        (total != 0u) ? total : 1u, sizeof(StickName), platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT));
    unsigned int count = 0u;
    names_gathered(&s_listed, s_name, &count);
    names_gathered(&s_ruleset, s_name, &count);
    for (unsigned int engine = 0u; engine < s_engines; engine += 1u)
    {
        names_gathered(&s_engine[engine].operations, s_name, &count);
    }
    stick_sorted(s_name, count, sizeof(StickName), names_order);
    unsigned int kept = 0u;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if ((kept == 0u) || (stick_text_order(s_name[kept - 1u].text, s_name[at].text) != 0))
        {
            s_name[kept] = s_name[at];
            kept += 1u;
        }
    }
    s_names = kept;
    names_numbered(&s_listed);
    names_numbered(&s_ruleset);
    for (unsigned int engine = 0u; engine < s_engines; engine += 1u)
    {
        names_numbered(&s_engine[engine].operations);
    }
}
