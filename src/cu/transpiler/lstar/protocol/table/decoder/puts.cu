// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// puts.cu: the puts of the log: the lines beneath a put read, a link's operation stem, the operands deciding a put, and
// the set a put is read into
#include "decoder_internal.h"

// the values of the words of `words` from `first` on, each in hex, up to a word `refused`: how many there are
static unsigned long long values_counted(const DecoderWords *words, unsigned int first)
{
    unsigned long long count = 0ULL;
    for (unsigned int at = first; (at < words->count) && !decoder_is(words->word[at], "refused"); at += 1u)
    {
        count += 1ULL;
    }
    return count;
}

// the values counted as above put at `values` from `at` on, `at` moved past them
static void values_put(const DecoderWords *words, unsigned int first, unsigned long long *values,
                       unsigned long long *at)
{
    for (unsigned int each = first; (each < words->count) && !decoder_is(words->word[each], "refused"); each += 1u)
    {
        values[*at] = decoder_hex(words->word[each]);
        *at += 1ULL;
    }
}

// 1 where the line is one the main read of the log takes, and no line beneath a put
static int line_of_the_log(CarrierPiece line)
{
    return decoder_begins(line, "choice ") || decoder_begins(line, "facts ") || decoder_begins(line, "binds") ||
           decoder_begins(line, "cases ") || decoder_begins(line, "query ");
}

void put_viewed(const LoggedPut *put, PutView *view)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = view, .bytes = sizeof(*view), .val = 0);
    // the read counts the values each side writes first, and takes their cells once at their count
    unsigned long long write_values[2] = {0ULL, 0ULL};
    size_t at = 0u;
    CarrierPiece line;
    while (carrier_piece_next(put->block, '\n', &at, &line))
    {
        line.length -= ((line.length != 0u) && (line.text[line.length - 1u] == '\r')) ? 1u : 0u;
        if (!line_of_the_log(line) && decoder_begins(line, "writes "))
        {
            DecoderWords words;
            decoder_words(carrier_after(line, 7u), &words);
            const int from_side = (words.count > 0u) && decoder_is(words.word[0], "from");
            write_values[from_side ? 0u : 1u] += values_counted(&words, 1u);
        }
    }
    for (unsigned int side = 0u; side < 2u; side += 1u)
    {
        view->writes[side] = (write_values[side] != 0ULL)
                                 ? (unsigned long long *)decoder_cell(write_values[side], sizeof(unsigned long long))
                                 : NULL;
    }
    at = 0u;
    while (carrier_piece_next(put->block, '\n', &at, &line))
    {
        line.length -= ((line.length != 0u) && (line.text[line.length - 1u] == '\r')) ? 1u : 0u;
        if (line_of_the_log(line))
        {
            continue;
        }
        if (decoder_begins(line, "from "))
        {
            view->from_text = carrier_after(line, 5u);
        }
        else if (decoder_begins(line, "to "))
        {
            view->to_text = carrier_after(line, 3u);
        }
        else if (decoder_begins(line, "reads ") && (view->reads.count < CONCEPT_READS_MAX))
        {
            DecoderWords words;
            decoder_words(carrier_after(line, 6u), &words);
            ConceptRead *const read = &view->reads.read[view->reads.count];
            view->reads.count += 1u;
            read->name.text[0] = '\0';
            if (words.count > 0u)
            {
                carrier_named(&read->name, words.word[0].text, words.word[0].length);
            }
            const unsigned long long count = values_counted(&words, 1u);
            read->values =
                (count != 0ULL) ? (unsigned long long *)decoder_cell(count, sizeof(unsigned long long)) : NULL;
            read->count = 0ULL;
            if (read->values != NULL)
            {
                values_put(&words, 1u, read->values, &read->count);
            }
        }
        else if (decoder_begins(line, "qualifies") || decoder_begins(line, "qualified"))
        {
            DecoderWords words;
            decoder_words(carrier_after(line, 9u), &words);
            for (unsigned int each = 0u; each < words.count; each += 1u)
            {
                carrier_number_added(&view->qualifiers, decoder_hex(words.word[each]));
            }
        }
        else if (decoder_begins(line, "writes "))
        {
            DecoderWords words;
            decoder_words(carrier_after(line, 7u), &words);
            const unsigned int side = ((words.count > 0u) && decoder_is(words.word[0], "from")) ? 0u : 1u;
            if (view->writes[side] != NULL)
            {
                values_put(&words, 1u, view->writes[side], &view->write_count[side]);
            }
        }
        else if (decoder_begins(line, "only"))
        {
            DecoderWords words;
            decoder_words(carrier_after(line, 4u), &words);
            for (unsigned int each = 0u; (each < words.count) && (view->only_count < DECODER_ONLY_MAX); each += 1u)
            {
                view->only[view->only_count] = words.word[each];
                view->only_count += 1u;
            }
        }
    }
}

// 1 where every read of the put holds a value for each case that came back
static int put_read(const LoggedPut *put, const PutView *view)
{
    const unsigned long long cases = put->came_back.length;
    int read = view->reads.count != 0u;
    for (unsigned int at = 0u; at < view->reads.count; at += 1u)
    {
        read &= (view->reads.read[at].count >= cases) ? 1 : 0;
    }
    return read;
}

// the values every statement reads as themselves lie below the top of a byte read signed
static const unsigned long long s_statement_free_below = 0x80ULL;

CarrierPiece operation_stem(CarrierPiece text)
{
    DecoderWords words;
    decoder_words(text, &words);
    CarrierPiece word = {"", 0u};
    if (words.count != 0u)
    {
        word = words.word[0];
    }
    if ((word.length != 0u) && (word.text[0] == '@'))
    {
        word = (words.count > 1u) ? words.word[1] : CarrierPiece{"", 0u};
    }
    for (size_t at = 0u; at < word.length; at += 1u)
    {
        if (word.text[at] == '.')
        {
            word.length = at;
        }
    }
    return word;
}

// the cases of a case set: each case's operands, pieces of the set, those of case `c` from first[c] to first[c + 1]
struct CaseView
{
    CarrierPiece *operand;
    unsigned long long *first;
    unsigned long long count;
};

// the case set `cases` read into `view`, its cells of the temporary tier, which the caller gives back: 1, or 0 where
// the pool cannot meet them
static int case_viewed(CarrierPiece cases, CaseView *view)
{
    unsigned long long case_count = 0ULL;
    unsigned long long operand_count = 0ULL;
    size_t at = 0u;
    CarrierPiece written;
    while (carrier_piece_next(cases, ' ', &at, &written))
    {
        case_count += 1ULL;
        size_t cut = 0u;
        CarrierPiece operand;
        while (carrier_piece_next(written, ',', &cut, &operand))
        {
            operand_count += 1ULL;
        }
    }
    view->count = case_count;
    view->operand = (CarrierPiece *)decoder_cell(operand_count, sizeof(CarrierPiece));
    view->first = (unsigned long long *)decoder_cell(case_count + 1ULL, sizeof(unsigned long long));
    if ((view->operand == NULL) || (view->first == NULL))
    {
        return 0;
    }
    unsigned long long put_at = 0ULL;
    unsigned long long case_at = 0ULL;
    at = 0u;
    while (carrier_piece_next(cases, ' ', &at, &written))
    {
        view->first[case_at] = put_at;
        case_at += 1ULL;
        size_t cut = 0u;
        CarrierPiece operand;
        while (carrier_piece_next(written, ',', &cut, &operand))
        {
            view->operand[put_at] = operand;
            put_at += 1ULL;
        }
    }
    view->first[case_at] = put_at;
    return 1;
}

static void case_view_ended(CaseView *view)
{
    platform_cell_released(view->first, platform_place_g_program_owner);
    platform_cell_released(view->operand, platform_place_g_program_owner);
}

// a case of a case set as the operands decide it. Its operands but the one `skip` names, and the mark of what came
// back of it, one bit
struct CaseRest
{
    const CarrierPiece *operand;
    unsigned long long count;
    unsigned long long skip;
    unsigned int mark;
};

// two cases in the order of their operands but the one each skips, which reads as `*` in both
static int case_rest_order(const void *left, const void *right)
{
    const CaseRest *const one = (const CaseRest *)left;
    const CaseRest *const other = (const CaseRest *)right;
    const unsigned long long shorter = (one->count < other->count) ? one->count : other->count;
    for (unsigned long long at = 0ULL; at < shorter; at += 1ULL)
    {
        const CarrierPiece star = {"*", 1u};
        const int order =
            decoder_order((at == one->skip) ? star : one->operand[at], (at == other->skip) ? star : other->operand[at]);
        if (order != 0)
        {
            return order;
        }
    }
    return (one->count == other->count) ? 0 : ((one->count < other->count) ? -1 : 1);
}

// The case operands that decide which cases of `cases` came back apart in `came_back`, put into `deciding`. An operand
// decides where two cases the same in every other operand came back two ways
static void operands_deciding(const CaseView *cases, CarrierPiece came_back, CarrierNumbers *deciding)
{
    const unsigned long long operands = (cases->count == 0ULL) ? 0ULL : (cases->first[1] - cases->first[0]);
    CaseRest *const rests = (CaseRest *)decoder_cell(cases->count, sizeof(CaseRest));
    if (rests == NULL)
    {
        return;
    }
    for (unsigned long long operand = 0ULL; operand < operands; operand += 1ULL)
    {
        unsigned long long rest_count = 0ULL;
        for (unsigned long long place = 0ULL; (place < cases->count) && (place < came_back.length); place += 1ULL)
        {
            const char mark = came_back.text[place];
            if (mark == '-')
            {
                continue;
            }
            CaseRest *const rest = &rests[rest_count];
            rest_count += 1ULL;
            rest->operand = &cases->operand[cases->first[place]];
            rest->count = cases->first[place + 1ULL] - cases->first[place];
            rest->skip = operand;
            rest->mark = (mark == '=') ? 1u : ((mark == 'x') ? 2u : 4u);
        }
        (void)decoder_sorted(rests, rest_count, sizeof(CaseRest), case_rest_order);
        int decides = 0;
        unsigned long long run = 0ULL;
        while (!decides && (run < rest_count))
        {
            unsigned int marks = rests[run].mark;
            unsigned long long next = run + 1ULL;
            while ((next < rest_count) && (case_rest_order(&rests[run], &rests[next]) == 0))
            {
                marks |= rests[next].mark;
                next += 1ULL;
            }
            // a mark is one bit, and more than one bit is more than one answer
            decides = (marks & (marks - 1u)) != 0u;
            run = next;
        }
        if (decides)
        {
            carrier_number_added(deciding, operand);
        }
    }
    platform_cell_released(rests, platform_place_g_program_owner);
}

// 1 where `operand` names `source` among the registers it names
static int operand_names(CarrierPiece operand, const char *source)
{
    CarrierName named[CARRIER_READS_MAX];
    const unsigned int count = carrier_registers(operand, 0, named, CARRIER_READS_MAX);
    int names = 0;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        names |= carrier_is(&named[at], source) ? 1 : 0;
    }
    return names;
}

// the file at `path` read whole into a cell of the temporary tier, its bytes into `size`; NULL where it does not read
static char *decoder_file_read(const char *path, size_t *size)
{
    unsigned long long bytes = 0ULL;
    char *const text =
        (platform_file_size(path, &bytes) != 0)
            ? (char *)platform_cell_taken(bytes + 1ULL, platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY)
            : NULL;
    unsigned long long read = 0ULL;
    if ((text == NULL) || !platform_file_read(path, text, bytes, &read))
    {
        platform_cell_released(text, platform_place_g_program_owner);
        *size = 0u;
        return NULL;
    }
    text[read] = '\0';
    // a file read whole is at max what a size_t counts
    *size = (size_t)read;
    return text;
}

// the numbers `from` holds put into `into`
static void numbers_joined(CarrierNumbers *into, const CarrierNumbers *from)
{
    for (unsigned int at = 0u; at < from->count; at += 1u)
    {
        carrier_number_added(into, from->number[at]);
    }
}

// The operands of a case the form reads at the carrier's link. They go into `qualifying` where an operand only the
// form names reads them, and into `qualified` otherwise. The registers the form writes go into `form_written`. 1, or
// 0 where the carrier does not read
static int form_operands(const LoggedPut *put, const PutView *view, const char *folder, CarrierNumbers *qualifying,
                         CarrierNumbers *qualified, CarrierWritten *form_written)
{
    const CarrierPiece address = put->address;
    const char *const colon =
        EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = address.text, .cap = address.length, .byte = ':');
    if (colon == NULL)
    {
        return 0;
    }
    char path[PLATFORM_PATH_BYTES];
    VerbaLinea path_line = mmgr_line_over(path, sizeof(path));
    mmgr_line_put(&path_line, folder);
    mmgr_line_ch(&path_line, '/');
    // the address's carrier ends at its colon, which lies past its start
    (void)mmgr_line_put_n(&path_line, address.text, (size_t)(colon - address.text));
    mmgr_line_put(&path_line, ".sass");
    (void)mmgr_line_finish(&path_line);
    // the place after the colon, in hex, up to the address's end
    char place[32];
    VerbaLinea place_line = mmgr_line_over(place, sizeof(place));
    // the colon lies within the address
    (void)mmgr_line_put_n(&place_line, colon + 1, address.length - (size_t)(colon + 1 - address.text));
    (void)mmgr_line_finish(&place_line);
    const long at = (long)(EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = place, .end = NULL, .base = 16) / 16u);
    size_t carrier_size = 0u;
    char *const carrier = decoder_file_read(path, &carrier_size);
    if (carrier == NULL)
    {
        return 0;
    }
    long cases_defined = -2;
    CarrierLinks links;
    carrier_links_begun(&links, PLATFORM_TIER_TEMPORARY);
    const CarrierPiece carrier_text = {carrier, carrier_size};
    const int read_whole = carrier_read(carrier_text, &links, &cases_defined);
    platform_cell_released(carrier, platform_place_g_program_owner);
    if (!read_whole)
    {
        platform_array_ended(&links);
        return 0;
    }
    // each line of the form at its own place in the carrier. A register an earlier line of the form writes is the
    // form's own, and the read follows it no further
    long line_at = at;
    size_t cut = 0u;
    CarrierPiece form_line;
    while (carrier_piece_next(view->from_text, ';', &cut, &form_line))
    {
        if (carrier_trimmed(form_line).length == 0u)
        {
            continue;
        }
        const CarrierLink link = carrier_link_read(form_line);
        if (link.loads && (cases_defined >= 0) &&
            (carrier_definition(&links, link.address_register.text, line_at) == cases_defined))
        {
            carrier_number_added(qualified, link.offset / s_carrier_operand_bytes);
        }
        for (unsigned int each = 0u; each < link.reads; each += 1u)
        {
            const char *const source = link.read[each].text;
            if (carrier_written_holds(form_written, source))
            {
                continue;
            }
            CarrierNumbers read;
            read.count = 0u;
            carrier_operands_read(&links, source, line_at, cases_defined, &read);
            int named_only = 0;
            for (unsigned int only = 0u; only < view->only_count; only += 1u)
            {
                named_only |= operand_names(view->only[only], source);
            }
            numbers_joined(named_only ? qualifying : qualified, &read);
        }
        carrier_written_added(form_written, &link.written);
        line_at += 1;
    }
    platform_array_ended(&links);
    return 1;
}

unsigned int put_decoded(const LoggedPut *put, const PutView *view, CarrierPiece cases, const char *folder)
{
    static CarrierNumbers s_qualifying;
    static CarrierNumbers s_qualified;
    static CarrierWritten s_form_written;
    s_qualifying.count = 0u;
    s_qualified.count = 0u;
    s_form_written.count = 0u;
    if (!form_operands(put, view, folder, &s_qualifying, &s_qualified, &s_form_written))
    {
        return 0u;
    }
    CaseView case_view;
    static CarrierNumbers s_deciding;
    s_deciding.count = 0u;
    if (case_viewed(cases, &case_view))
    {
        operands_deciding(&case_view, put->came_back, &s_deciding);
    }
    case_view_ended(&case_view);
    // a case word the carrier reads after the link decides the cases apart beside the words the link reads. Neither
    // side of the pair reads it
    int qualifier = 0;
    int link_read = 0;
    for (unsigned int each = 0u; each < s_deciding.count; each += 1u)
    {
        const unsigned long long operand = s_deciding.number[each];
        const int qualifying = carrier_number_held(&s_qualifying, operand);
        const int qualified = carrier_number_held(&s_qualified, operand);
        qualifier |= (qualifying && !qualified) ? 1 : 0;
        link_read |= (qualified || qualifying) ? 1 : 0;
    }
    // A qualifier is a condition, a flag. An operand only one form names that carries a value, the value copied or the
    // addend added, makes the result and qualifies nothing
    for (unsigned int only = 0u; only < view->only_count; only += 1u)
    {
        CarrierName named[CARRIER_READS_MAX];
        const unsigned int count = carrier_registers(view->only[only], 0, named, CARRIER_READS_MAX);
        for (unsigned int each = 0u; each < count; each += 1u)
        {
            qualifier &= (named[each].text[0] == 'P') ? 1 : 0;
        }
        qualifier &= (count == 0u) ? 0 : 1;
    }
    if (qualifier)
    {
        return decoder_set_bit("qualifier_coherence");
    }
    // two forms of a flag apart whatever the link reads are the two members of one node, each the other's negation.
    // Every case is apart, or apart where only words the link does not read decide it
    int flags = s_form_written.count != 0u;
    for (unsigned int each = 0u; each < s_form_written.count; each += 1u)
    {
        flags &= (s_form_written.name[each].text[0] == 'P') ? 1 : 0;
    }
    const int only_empty = view->only_count == 0u;
    if (only_empty && !link_read && flags)
    {
        return decoder_set_bit("negation_coherence");
    }
    if (!only_empty || !link_read)
    {
        return 0u;
    }
    // Two readings of the same bits under two statements agree wherever every value the form reads fits every
    // statement. That is below the top of a byte read signed, the narrowest signed width the cases represent. A
    // modifier apart only past that is the statement's. The values are the form's own, read at the form. The decoding
    // reads a put the part recorded no read of into no set
    if (!put_read(put, view))
    {
        return 0u;
    }
    int past_every_statement = 1;
    for (size_t place = 0u; place < put->came_back.length; place += 1u)
    {
        if (put->came_back.text[place] != 'x')
        {
            continue;
        }
        int fits = 1;
        for (unsigned int each = 0u; each < view->reads.count; each += 1u)
        {
            fits &= (view->reads.read[each].values[place] < s_statement_free_below) ? 1 : 0;
        }
        past_every_statement &= fits ? 0 : 1;
    }
    return decoder_set_bit(past_every_statement ? "statement_coherence" : "modifier_coherence");
}
