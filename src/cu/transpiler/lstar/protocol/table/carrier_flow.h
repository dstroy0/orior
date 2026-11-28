// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// carrier_flow.h: a carrier's links read as what each writes, reads and loads, and each register followed back to the
// operands of a case it reads (P13, the pairs)
//
// A link writes the register its first operand names and reads every other register it names. A memory operand after
// the first is a load through its register at its offset. The flow follows a register to the nearest link before that
// writes it. A load through the register the carrier's first load reads is the operand of a case its offset names.
//
// A link carries the cases where it loads one of their operands or reads a register one of them reaches. Every other
// link is the carrier's own: the thread's index, the case's index, the bound and the addresses. A form put there moves
// the case each thread reads. The links between a case's load and the link are the carrier's delta, apart from the
// form's. `orior_klq_identity` puts a form only where the link carries the cases, the least delta first.
// `orior_klq_decoder` reads each put's operands back to the case words through the same links
#ifndef CARRIER_FLOW_H
#define CARRIER_FLOW_H

#include "../../../../../../includes/regexp/regexp.h"
#include "../../../../../../includes/platform/platform.h"
#include "../teacher/run_channel.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

#include <stddef.h>

// the bytes of one operand of a case: two words of the run channel
static const unsigned long long s_carrier_operand_bytes = 2ULL * sizeof(unsigned int);

// the bytes of a register's name as a link writes it, its terminator among them. Then the registers one link reads at
// max, and the characters of one link's text that a pattern reads at max
#define CARRIER_NAME_BYTES 24u
#define CARRIER_READS_MAX 32u
#define CARRIER_TEXT_MAX 1024u

// a register's name, terminated, and empty where it names none
struct CarrierName
{
    char text[CARRIER_NAME_BYTES];
};

// One link of a carrier: the register it writes and the registers it reads. Each read is also kept as the link writes
// it, a pair's high word with its `.hi`. Last come the register and offset it loads through
struct CarrierLink
{
    CarrierName written;
    CarrierName read[CARRIER_READS_MAX];
    CarrierName read_written[CARRIER_READS_MAX];
    unsigned int reads;
    int loads;
    CarrierName address_register;
    unsigned long long offset;
};

// The links of a carrier, a PlatformArray of CarrierLink, one a line, in one cell that carrier_read takes once at the
// carrier's count of lines
typedef PlatformArray CarrierLinks;

// the links begun empty, on the temporary tier where `ephemeral` is 1
static inline void carrier_links_begun(CarrierLinks *links, int ephemeral)
{
    platform_array_begun(links, sizeof(CarrierLink), platform_place_g_program_owner, ephemeral);
}

// the link at `at` [BORROWS]
static inline const CarrierLink *carrier_link_at(const CarrierLinks *links, unsigned long long at)
{
    return (const CarrierLink *)platform_array_at(links, at);
}

// numbers in their order, each once, at max CARRIER_NUMBERS_MAX of them: the operands of a case a carrier reads, the
// literals a form writes
#define CARRIER_NUMBERS_MAX 64u

struct CarrierNumbers
{
    unsigned long long number[CARRIER_NUMBERS_MAX];
    unsigned int count;
};

// 1 where `numbers` holds `value`
static inline int carrier_number_held(const CarrierNumbers *numbers, unsigned long long value)
{
    for (unsigned int at = 0u; at < numbers->count; at += 1u)
    {
        if (numbers->number[at] == value)
        {
            return 1;
        }
    }
    return 0;
}

// `value` put into `numbers` in its order, where they do not hold it yet and hold fewer than CARRIER_NUMBERS_MAX
static inline void carrier_number_added(CarrierNumbers *numbers, unsigned long long value)
{
    if (carrier_number_held(numbers, value) || (numbers->count >= CARRIER_NUMBERS_MAX))
    {
        return;
    }
    unsigned int at = numbers->count;
    while ((at > 0u) && (numbers->number[at - 1u] > value))
    {
        numbers->number[at] = numbers->number[at - 1u];
        at -= 1u;
    }
    numbers->number[at] = value;
    numbers->count += 1u;
}

// the registers a form reads at max, and the names of them in the order the form names them
#define CARRIER_FORM_READS_MAX (CARRIER_READS_MAX * 4u)

struct CarrierReads
{
    CarrierName name[CARRIER_FORM_READS_MAX];
    unsigned int count;
};

// a piece of a text: its first byte and its bytes
struct CarrierPiece
{
    const char *text;
    size_t length;
};

// the `length` bytes of `text` as a name, cut at the room
static inline void carrier_named(CarrierName *name, const char *text, size_t length)
{
    VerbaLinea line = mmgr_line_over(name->text, sizeof(name->text));
    (void)mmgr_line_put_n(&line, text, length);
    (void)mmgr_line_finish(&line);
}

// the bytes that carrier_space takes as spaces, for a span over them
#define CARRIER_SPACES " \t\r\n"

static inline int carrier_space(char character)
{
    return (character == ' ') || (character == '\t') || (character == '\r') || (character == '\n');
}

// `piece` with the space at either end taken off
static inline CarrierPiece carrier_trimmed(CarrierPiece piece)
{
    const size_t lead =
        EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = piece.text, .cap = piece.length, .other = CARRIER_SPACES);
    piece.text += lead;
    piece.length -= lead;
    while ((piece.length != 0u) && carrier_space(piece.text[piece.length - 1u]))
    {
        piece.length -= 1u;
    }
    return piece;
}

// The next piece of `text` cut at `separator` from `*at` into `piece`, and `*at` past its separator. 0 where no piece
// stays: a text that ends at a separator has no empty piece after it
static inline int carrier_piece_next(CarrierPiece text, char separator, size_t *at, CarrierPiece *piece)
{
    if (*at >= text.length)
    {
        return 0;
    }
    size_t end = *at;
    while ((end < text.length) && (text.text[end] != separator))
    {
        end += 1u;
    }
    piece->text = &text.text[*at];
    piece->length = end - *at;
    *at = end + 1u;
    return 1;
}

// the place of the first space or tab of `piece`, or its length where it has none
static inline size_t carrier_space_at(CarrierPiece piece)
{
    return EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = piece.text, .cap = piece.length, .other = " \t");
}

// `piece` from `at` on
static inline CarrierPiece carrier_after(CarrierPiece piece, size_t at)
{
    const size_t from = (at < piece.length) ? at : piece.length;
    const CarrierPiece after = {&piece.text[from], piece.length - from};
    return after;
}

// the terminated `text` as a piece
static inline CarrierPiece carrier_piece_of(const char *text)
{
    const CarrierPiece piece = {
        text, EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR)};
    return piece;
}

// the patterns that read a carrier: a register, a memory operand, and a literal alone
#define CARRIER_PATTERN_REGISTER 0u
#define CARRIER_PATTERN_MEMORY 1u
#define CARRIER_PATTERN_LITERAL 2u

// The patterns, each read once into its own work state on its first use, and 1 where it read. The calls of this module
// share them
struct CarrierPatterns
{
    Regexp regexp[3];
    RegexpWorkState work[3];
    int read[3];
};

static CarrierPatterns s_carrier_patterns;

// pattern `pattern` read on its first use [BORROWS]; NULL where it did not read
static inline const Regexp *carrier_pattern(unsigned int pattern)
{
    static const char *const s_sources[3] = {"\\b(U?R[0-9]+|P[0-9]+)(\\.hi)?",
                                             "\\[(U?R[0-9]+)(\\.64)?(\\+([0-9a-fA-Fx]+))?\\]",
                                             "^\\s*(0x[0-9a-fA-F]+|[0-9]+)\\s*$"};
    CarrierPatterns *const patterns = &s_carrier_patterns;
    if (patterns->read[pattern] == 0)
    {
        const RegexpWorkStateCfg work = {&patterns->regexp[pattern], &patterns->work[pattern]};
        regexp_on_capacity(&work);
        char why[256];
        VerbaLinea error = mmgr_line_over(why, sizeof(why));
        const RegexpSourceCfg source = {&patterns->regexp[pattern], s_sources[pattern], "", &error};
        patterns->read[pattern] = regexp_read(&source) ? 1 : -1;
    }
    return (patterns->read[pattern] > 0) ? &patterns->regexp[pattern] : NULL;
}

// a text a pattern reads: its characters, each a byte of the text, and their count
struct CarrierCharacters
{
    unsigned int character[CARRIER_TEXT_MAX];
    unsigned long long length;
};

static inline void carrier_characters(CarrierPiece piece, CarrierCharacters *characters)
{
    const size_t length = (piece.length < CARRIER_TEXT_MAX) ? piece.length : CARRIER_TEXT_MAX;
    for (size_t at = 0u; at < length; at += 1u)
    {
        characters->character[at] = (unsigned char)piece.text[at];
    }
    characters->length = length;
}

// The first match of pattern `pattern` in `characters` at or past `from`, its groups' spans into `groups`. 1, or 0
// where it matches nowhere there
static inline int carrier_search(unsigned int pattern, const CarrierCharacters *characters, unsigned long long from,
                                 RegexpSpan *groups)
{
    const Regexp *const regexp = carrier_pattern(pattern);
    for (unsigned long long at = from; (regexp != NULL) && (at <= characters->length); at += 1ULL)
    {
        const RegexpTextU32sCfg match = {regexp, characters->character, characters->length, at, groups};
        if (regexp_match_into(&match))
        {
            return 1;
        }
    }
    return 0;
}

// The registers `operand` names into `named`, each by the name its pair's low register goes by. Where `whole` is 1,
// each goes as the operand writes it, a pair's high word with its `.hi`. The answer is their count, at max `room`
static inline unsigned int carrier_registers(CarrierPiece operand, int whole, CarrierName *named, unsigned int room)
{
    CarrierCharacters characters;
    carrier_characters(operand, &characters);
    unsigned int count = 0u;
    RegexpSpan groups[REGEXP_GROUPS + 1u];
    for (unsigned long long from = 0ULL;
         (count < room) && carrier_search(CARRIER_PATTERN_REGISTER, &characters, from, groups);)
    {
        const RegexpSpan span = whole ? groups[0] : groups[1];
        carrier_named(&named[count], &operand.text[span.from], (size_t)(span.to - span.from));
        count += 1u;
        // a match takes at least one character, and the next search starts past it
        from = (groups[0].to > groups[0].from) ? groups[0].to : (groups[0].from + 1ULL);
    }
    return count;
}

// 1 where `name` is the terminated `text`
static inline int carrier_is(const CarrierName *name, const char *text)
{
    return EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name->text, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = text,
                      .ci = EMBED_FALSE);
}

// A link's text read as the register its first operand writes and the registers the rest read. A memory operand after
// the first reads as a load through its register at its offset
static inline CarrierLink carrier_link_read(CarrierPiece text)
{
    CarrierLink link;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &link, .bytes = sizeof(link), .val = 0);
    CarrierPiece rest = carrier_trimmed(text);
    if ((rest.length != 0u) && (rest.text[0] == '@'))
    {
        const size_t space = carrier_space_at(rest);
        const CarrierPiece guard = {rest.text, space};
        CarrierName guards[CARRIER_READS_MAX];
        const unsigned int count = carrier_registers(guard, 0, guards, CARRIER_READS_MAX);
        for (unsigned int at = 0u; (at < count) && (link.reads < CARRIER_READS_MAX); at += 1u)
        {
            link.read[link.reads] = guards[at];
            link.read_written[link.reads] = guards[at];
            link.reads += 1u;
        }
        rest = carrier_trimmed(carrier_after(rest, space));
    }
    const size_t space = carrier_space_at(rest);
    rest = (space == rest.length) ? carrier_after(rest, rest.length) : carrier_after(rest, space);
    for (size_t at = 0u; at < rest.length; at += 1u)
    {
        if (rest.text[at] == ';')
        {
            rest.length = at;
        }
    }
    size_t cut = 0u;
    CarrierPiece operand;
    for (unsigned int place = 0u; carrier_piece_next(rest, ',', &cut, &operand); place += 1u)
    {
        CarrierCharacters characters;
        carrier_characters(operand, &characters);
        RegexpSpan groups[REGEXP_GROUPS + 1u];
        const int memory = carrier_search(CARRIER_PATTERN_MEMORY, &characters, 0ULL, groups);
        if (memory && (place > 0u))
        {
            link.loads = 1;
            carrier_named(&link.address_register, &operand.text[groups[1].from],
                          (size_t)(groups[1].to - groups[1].from));
            link.offset = 0ULL;
            if (groups[4].set)
            {
                CarrierName offset;
                carrier_named(&offset, &operand.text[groups[4].from], (size_t)(groups[4].to - groups[4].from));
                // an offset written 0x... reads as hex, and one written bare as decimal
                const int hex = (offset.text[0] == '0') && ((offset.text[1] == 'x') || (offset.text[1] == 'X'));
                link.offset = EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = hex ? &offset.text[2] : offset.text,
                                         .end = NULL, .base = hex ? 16u : 10u);
            }
        }
        CarrierName named[CARRIER_READS_MAX];
        CarrierName named_whole[CARRIER_READS_MAX];
        const unsigned int count = carrier_registers(operand, 0, named, CARRIER_READS_MAX);
        (void)carrier_registers(operand, 1, named_whole, CARRIER_READS_MAX);
        for (unsigned int each = 0u; each < count; each += 1u)
        {
            if ((place == 0u) && (each == 0u) && !memory)
            {
                link.written = named[each];
                continue;
            }
            if (link.reads < CARRIER_READS_MAX)
            {
                link.read[link.reads] = named[each];
                link.read_written[link.reads] = named_whole[each];
                link.reads += 1u;
            }
        }
    }
    return link;
}

// the nearest link of `links` before `before` that writes `register_name`, and -1 where none does
static inline long carrier_definition(const CarrierLinks *links, const char *register_name, long before)
{
    for (long at = before - 1; at >= 0; at -= 1)
    {
        if (carrier_is(&carrier_link_at(links, (unsigned long long)at)->written, register_name))
        {
            return at;
        }
    }
    return -1;
}

// The operands of a case that `register_name` reads at link `before` of `links` put into `read`. The carrier loads the
// cases through the register the link at `cases_defined` writes
static inline void carrier_operands_read(const CarrierLinks *links, const char *register_name, long before,
                                         long cases_defined, CarrierNumbers *read)
{
    const long defined = carrier_definition(links, register_name, before);
    if (defined < 0)
    {
        return;
    }
    const CarrierLink *const link = carrier_link_at(links, (unsigned long long)defined);
    if (link->loads && (cases_defined >= 0) &&
        (carrier_definition(links, link->address_register.text, defined) == cases_defined))
    {
        carrier_number_added(read, link->offset / s_carrier_operand_bytes);
        return;
    }
    for (unsigned int at = 0u; at < link->reads; at += 1u)
    {
        carrier_operands_read(links, link->read[at].text, defined, cases_defined, read);
    }
}

// A carrier's text read link by link into `links`, which it replaces, a link to a line. In one cell taken once at the
// text's count of lines. The link that writes the register its first load reads through goes into `cases_defined`, -1
// where none writes that register, and -2 where the carrier loads nothing. 1, or 0 where the pool cannot meet the cell
static inline int carrier_read(CarrierPiece text, CarrierLinks *links, long *cases_defined)
{
    *cases_defined = -2;
    unsigned long long lines = 0ULL;
    size_t at = 0u;
    CarrierPiece line;
    while (carrier_piece_next(text, '\n', &at, &line))
    {
        lines += 1ULL;
    }
    if (!platform_array_counted(links, 0ULL) || !platform_array_counted(links, lines))
    {
        return 0;
    }
    at = 0u;
    for (unsigned long long index = 0ULL; carrier_piece_next(text, '\n', &at, &line); index += 1ULL)
    {
        CarrierLink *const read = (CarrierLink *)platform_array_at(links, index);
        *read = carrier_link_read(line);
        if (read->loads && (*cases_defined == -2))
        {
            // a carrier's links are far below 2^63
            *cases_defined = carrier_definition(links, read->address_register.text, (long)index);
        }
    }
    return 1;
}

// the operands of a case `links` loads, by their place in the case, put into `loaded`
static inline void carrier_operands_loaded(const CarrierLinks *links, long cases_defined, CarrierNumbers *loaded)
{
    for (unsigned long long at = 0ULL; at < links->count; at += 1ULL)
    {
        const CarrierLink *const link = carrier_link_at(links, at);
        if (link->loads && (cases_defined >= 0) &&
            (carrier_definition(links, link->address_register.text, (long)at) == cases_defined))
        {
            carrier_number_added(loaded, link->offset / s_carrier_operand_bytes);
        }
    }
}

// The links of `links` between a case's load and link `before`'s read of `register_name`, the max over every case
// operand that reaches it. 0 where a load of a case writes it, and -1 where no operand of a case reaches it
static inline long carrier_delta(const CarrierLinks *links, const char *register_name, long before, long cases_defined)
{
    const long defined = carrier_definition(links, register_name, before);
    if (defined < 0)
    {
        return -1;
    }
    const CarrierLink *const link = carrier_link_at(links, (unsigned long long)defined);
    if (link->loads && (cases_defined >= 0) &&
        (carrier_definition(links, link->address_register.text, defined) == cases_defined))
    {
        return 0;
    }
    long max = -1;
    for (unsigned int at = 0u; at < link->reads; at += 1u)
    {
        const long further = carrier_delta(links, link->read[at].text, defined, cases_defined);
        max = ((further >= 0) && ((further + 1) > max)) ? (further + 1) : max;
    }
    return max;
}

// the registers a form writes before the line the read is at, each once: the form's own, followed no further
struct CarrierWritten
{
    CarrierName name[CARRIER_READS_MAX * 4u];
    unsigned int count;
};

static inline int carrier_written_holds(const CarrierWritten *written, const char *name)
{
    for (unsigned int at = 0u; at < written->count; at += 1u)
    {
        if (carrier_is(&written->name[at], name))
        {
            return 1;
        }
    }
    return 0;
}

static inline void carrier_written_added(CarrierWritten *written, const CarrierName *name)
{
    if (!carrier_written_holds(written, name->text) && (written->count < (CARRIER_READS_MAX * 4u)))
    {
        written->name[written->count] = *name;
        written->count += 1u;
    }
}

// The carrier's own delta before the form `form_text`, its instructions cut at `;` and the first at link `at` of
// `links`. That is the links between a case's load and the form, the max over every operand of a case the form reads.
// It is 0 where the form reads the cases as the carrier loaded them or loads them itself, and -1 where it reads none. A
// put at 0 answers of the form alone. Past 0 the answer is the form's delta composed with the carrier's, a delta of its
// own
static inline long carrier_form_delta(const CarrierLinks *links, long cases_defined, CarrierPiece form_text, long at)
{
    CarrierWritten form_written;
    form_written.count = 0u;
    long max = -1;
    long line_at = at;
    size_t cut = 0u;
    CarrierPiece form_line;
    while (carrier_piece_next(form_text, ';', &cut, &form_line))
    {
        if (carrier_trimmed(form_line).length == 0u)
        {
            continue;
        }
        const CarrierLink link = carrier_link_read(form_line);
        if (link.loads && (cases_defined >= 0) &&
            (carrier_definition(links, link.address_register.text, line_at) == cases_defined))
        {
            max = (max < 0) ? 0 : max;
        }
        for (unsigned int each = 0u; each < link.reads; each += 1u)
        {
            const long further = !carrier_written_holds(&form_written, link.read[each].text)
                                     ? carrier_delta(links, link.read[each].text, line_at, cases_defined)
                                     : -1;
            max = (further > max) ? further : max;
        }
        carrier_written_added(&form_written, &link.written);
        line_at += 1;
    }
    return max;
}

// 1 where `reads` holds `name`
static inline int carrier_reads_hold(const CarrierReads *reads, const char *name)
{
    for (unsigned int at = 0u; at < reads->count; at += 1u)
    {
        if (carrier_is(&reads->name[at], name))
        {
            return 1;
        }
    }
    return 0;
}

// `name` put after the names `reads` holds, where they hold fewer than CARRIER_FORM_READS_MAX
static inline void carrier_reads_added(CarrierReads *reads, const CarrierName *name)
{
    if (reads->count < CARRIER_FORM_READS_MAX)
    {
        reads->name[reads->count] = *name;
        reads->count += 1u;
    }
}

// The registers the form `form_text` reads that an operand of a case reaches and no earlier instruction of the form
// writes, each once, put into `reads`. The read cuts the form's instructions at `;`, the first at link `at` of `links`.
// Each goes as the form writes it and in the order it names them. `[case <n>]` goes for each operand of a case the form
// loads itself: what the form's product holds
static inline void carrier_form_reads(const CarrierLinks *links, long cases_defined, CarrierPiece form_text, long at,
                                      CarrierReads *reads)
{
    CarrierWritten form_written;
    form_written.count = 0u;
    long line_at = at;
    size_t cut = 0u;
    CarrierPiece form_line;
    while (carrier_piece_next(form_text, ';', &cut, &form_line))
    {
        if (carrier_trimmed(form_line).length == 0u)
        {
            continue;
        }
        const CarrierLink link = carrier_link_read(form_line);
        if (link.loads && (cases_defined >= 0) &&
            (carrier_definition(links, link.address_register.text, line_at) == cases_defined))
        {
            CarrierName loaded;
            VerbaLinea loaded_line = mmgr_line_over(loaded.text, sizeof(loaded.text));
            mmgr_line_put(&loaded_line, "[case ");
            mmgr_line_decimal(&loaded_line, link.offset / s_carrier_operand_bytes, 1u);
            mmgr_line_ch(&loaded_line, ']');
            (void)mmgr_line_finish(&loaded_line);
            if (!carrier_reads_hold(reads, loaded.text))
            {
                carrier_reads_added(reads, &loaded);
            }
            carrier_written_added(&form_written, &link.written);
            line_at += 1;
            continue;
        }
        for (unsigned int each = 0u; each < link.reads; each += 1u)
        {
            const CarrierName *const named = &link.read_written[each];
            if (!carrier_written_holds(&form_written, link.read[each].text) &&
                !carrier_reads_hold(reads, named->text) &&
                (carrier_delta(links, link.read[each].text, line_at, cases_defined) >= 0))
            {
                carrier_reads_added(reads, named);
            }
        }
        carrier_written_added(&form_written, &link.written);
        line_at += 1;
    }
}

// The literal values the text `form_text` writes, put into `literals`. An operand that is a number alone, and no
// operand of memory or of a bank of constants. These are the values a form brings of its own, whatever carrier carries
// it
static inline void carrier_literals(CarrierPiece form_text, CarrierNumbers *literals)
{
    size_t cut = 0u;
    CarrierPiece form_line;
    while (carrier_piece_next(form_text, ';', &cut, &form_line))
    {
        CarrierPiece rest = carrier_trimmed(form_line);
        if ((rest.length != 0u) && (rest.text[0] == '@'))
        {
            rest = carrier_trimmed(carrier_after(rest, carrier_space_at(rest) + 1u));
        }
        const size_t space = carrier_space_at(rest);
        const CarrierPiece operands =
            (space == rest.length) ? carrier_after(rest, rest.length) : carrier_after(rest, space);
        size_t at = 0u;
        CarrierPiece operand;
        while (carrier_piece_next(operands, ',', &at, &operand))
        {
            CarrierCharacters characters;
            carrier_characters(operand, &characters);
            RegexpSpan groups[REGEXP_GROUPS + 1u];
            const Regexp *const regexp = carrier_pattern(CARRIER_PATTERN_LITERAL);
            const RegexpTextU32sCfg match = {regexp, characters.character, characters.length, 0ULL, groups};
            // the pattern anchors at both ends, and a match from the first character is the whole operand's
            if ((regexp != NULL) && regexp_match_into(&match) && (groups[0].to == characters.length))
            {
                CarrierName number;
                carrier_named(&number, &operand.text[groups[1].from], (size_t)(groups[1].to - groups[1].from));
                const int hex = (number.text[0] == '0') && ((number.text[1] == 'x') || (number.text[1] == 'X'));
                carrier_number_added(literals, EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg,
                                                          .src = hex ? &number.text[2] : number.text, .end = NULL,
                                                          .base = hex ? 16u : 10u));
            }
        }
    }
}

// The register the form `form_text` makes its product in into `product`, as its last instruction's first operand
// writes it, a pair's high word with its `.hi`. `wide` gets 1 where that instruction writes the pair whole. The product
// is empty where the form writes none
static inline void carrier_form_product(CarrierPiece form_text, int *wide, CarrierName *product)
{
    CarrierPiece last = {form_text.text, 0u};
    size_t cut = 0u;
    CarrierPiece form_line;
    while (carrier_piece_next(form_text, ';', &cut, &form_line))
    {
        last = (carrier_trimmed(form_line).length == 0u) ? last : carrier_trimmed(form_line);
    }
    if ((last.length != 0u) && (last.text[0] == '@'))
    {
        const size_t space = carrier_space_at(last);
        last = (space == last.length) ? carrier_after(last, last.length) : carrier_trimmed(carrier_after(last, space));
    }
    const size_t space = carrier_space_at(last);
    const CarrierPiece operation = {last.text, space};
    const CarrierPiece operands =
        (space == last.length) ? carrier_after(last, last.length) : carrier_after(last, space);
    size_t at = 0u;
    CarrierPiece first_operand = {operands.text, 0u};
    const int has_operand = carrier_piece_next(operands, ',', &at, &first_operand);
    CarrierName first[CARRIER_READS_MAX];
    const unsigned int count = has_operand ? carrier_registers(first_operand, 1, first, CARRIER_READS_MAX) : 0u;
    int sixty_four = 0;
    for (size_t place = 0u; (place + 3u) <= operation.length; place += 1u)
    {
        sixty_four |=
            (EMBED_CALL(memor.cmp, MemoriaCfg, .src = &operation.text[place], .other = ".64", .bytes = 3u) == 0) ? 1
                                                                                                                 : 0;
        sixty_four |=
            ((place + 5u) <= operation.length) &&
            (EMBED_CALL(memor.cmp, MemoriaCfg, .src = &operation.text[place], .other = ".WIDE", .bytes = 5u) == 0);
    }
    *wide = sixty_four;
    int bracket = 0;
    for (size_t place = 0u; place < first_operand.length; place += 1u)
    {
        bracket |= (first_operand.text[place] == '[') ? 1 : 0;
    }
    product->text[0] = '\0';
    if ((count != 0u) && !bracket)
    {
        *product = first[0];
    }
}

// 1 where the form `form_text`, its instructions cut at `;` and the first at link `at` of `links`, carries the cases.
// One of its instructions loads an operand of a case, or reads a register one reaches that no earlier instruction of
// the form writes
static inline int carrier_form_carries(const CarrierLinks *links, long cases_defined, CarrierPiece form_text, long at)
{
    CarrierWritten form_written;
    form_written.count = 0u;
    long line_at = at;
    size_t cut = 0u;
    CarrierPiece form_line;
    while (carrier_piece_next(form_text, ';', &cut, &form_line))
    {
        if (carrier_trimmed(form_line).length == 0u)
        {
            continue;
        }
        const CarrierLink link = carrier_link_read(form_line);
        if (link.loads && (cases_defined >= 0) &&
            (carrier_definition(links, link.address_register.text, line_at) == cases_defined))
        {
            return 1;
        }
        for (unsigned int each = 0u; each < link.reads; each += 1u)
        {
            if (carrier_written_holds(&form_written, link.read[each].text))
            {
                continue;
            }
            CarrierNumbers read;
            read.count = 0u;
            carrier_operands_read(links, link.read[each].text, line_at, cases_defined, &read);
            const int reaches = read.count != 0u;
            if (reaches)
            {
                return 1;
            }
        }
        carrier_written_added(&form_written, &link.written);
        line_at += 1;
    }
    return 0;
}

#endif
