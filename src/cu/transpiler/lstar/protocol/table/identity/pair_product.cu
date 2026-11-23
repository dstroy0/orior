// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pair_product.cu: the put beneath its query in the log, and the product queried over the cases at its link: what
// the form reads there, and what it and its stand-in write
#include "pair.h"

// the piece written onto `line` with each line's end a space
static void line_ends_spaced(VerbaLinea *line, CarrierPiece piece)
{
    for (size_t at = 0u; at < piece.length; at += 1u)
    {
        mmgr_line_ch(line, (piece.text[at] == '\n') ? ' ' : piece.text[at]);
    }
}

// 1 where the terminated `value` is a number as a literal operand writes it. That is a sign at its start, and then
// hex digits after 0x or decimal digits
static int literal_number(const char *value)
{
    static TablePatternOnce s_number;
    TableMatch match;
    return table_matched(table_once(&s_number, "-?(0x[0-9a-fA-F]+|[0-9]+)", 1), value,
                         EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = value, .cap = PLATFORM_TEXT_TO_TERMINATOR),
                         &match);
}

// the terminated `value` read as std::stoll reads it with base 0: a sign, then hex after 0x, octal after a 0, decimal
static unsigned long long literal_value(const char *value)
{
    const int negative = value[0] == '-';
    const char *digits = negative ? (value + 1) : value;
    unsigned int base = 10u;
    if ((digits[0] == '0') && ((digits[1] == 'x') || (digits[1] == 'X')))
    {
        base = 16u;
        digits += 2;
    }
    else if (digits[0] == '0')
    {
        base = 8u;
    }
    const unsigned long long magnitude =
        EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = digits, .end = NULL, .base = base);
    // a negative literal goes as its two's complement, as the cast of a negative long long does
    return negative ? (0ULL - magnitude) : magnitude;
}

// a read the chain binds to a literal, and the literal's value
struct LiteralRead
{
    CarrierName name;
    unsigned long long value;
};

// the reads the chain binds to literals, each once
struct LiteralReads
{
    LiteralRead read[CARRIER_FORM_READS_MAX];
    unsigned int count;
};

// the value `literals` holds of the terminated `name`, or NULL where it holds none
static const unsigned long long *literal_of(const LiteralReads *literals, const char *name)
{
    for (unsigned int at = 0u; at < literals->count; at += 1u)
    {
        if (carrier_is(&literals->read[at].name, name))
        {
            return &literals->read[at].value;
        }
    }
    return NULL;
}

// `value` given to `name` in `literals`, in place of what it gave the name before, where they hold room for it
static void literal_set(LiteralReads *literals, const CarrierName *name, unsigned long long value)
{
    unsigned int at = 0u;
    while ((at < literals->count) && !carrier_is(&literals->read[at].name, name->text))
    {
        at += 1u;
    }
    if (at >= CARRIER_FORM_READS_MAX)
    {
        return;
    }
    literals->read[at].name = *name;
    literals->read[at].value = value;
    literals->count += (at == literals->count) ? 1u : 0u;
}

// what one round of the product queries of the link, and what came back of it. It holds the link and the work,
// and the reads in the order the round queries them. Then the literal value of each read the chain binds to one
struct ProductRound
{
    const PutLink *link;
    PairWork *work;
    const char *chain_text;
    CarrierPiece found;
    long form_lines;
    const CarrierReads *reads_named;
    const LiteralReads *literal_reads;
};

// what each side of a put wrote on every case, in cells of the temporary tier at the question's count of cases. Then
// how many each holds
struct ProductWrites
{
    unsigned long long *side[2];
    unsigned long long count[2];
};

// the writes' cells given back
static void product_writes_ended(ProductWrites *writes)
{
    for (unsigned int side = 0u; side < 2u; side += 1u)
    {
        platform_cell_released(writes->side[side], platform_place_g_program_owner);
        writes->side[side] = NULL;
        writes->count[side] = 0ULL;
    }
}

// a line of the trace: `head`, the name, the cut's place, its count of links, then the cut's text
static void cut_traced(const char *head, const char *name, const char *place, const char *address,
                       unsigned long long links, const char *cut)
{
    VerbaLinea *const line = query_log_line(s_trace_log);
    mmgr_line_put(line, head);
    mmgr_line_put(line, name);
    mmgr_line_put(line, place);
    mmgr_line_put(line, address);
    mmgr_line_put(line, ", ");
    mmgr_line_decimal(line, links, 1u);
    mmgr_line_put(line, " links:\n");
    mmgr_line_put(line, cut);
    query_log_flushed(s_trace_log);
}

// the cut into `code`, which this begins, assembled where its text is not empty, and the count of its links
static unsigned long long cut_assembled(const PairWork *work, PlatformText *cut, IdentityCode *code)
{
    identity_code_begun(code, PLATFORM_TIER_TEMPORARY);
    if (platform_text_size(cut) != 0ULL)
    {
        vendor_assembled(work->machine, platform_text_bytes(cut), code);
    }
    return code->count / 16ULL;
}

// a read's values: a cell of the temporary tier at `cases` values [OWNS], or NULL where the pool cannot meet it
static unsigned long long *read_values(unsigned int cases)
{
    return (unsigned long long *)platform_cell_taken(((cases != 0u) ? cases : 1u) * sizeof(unsigned long long),
                                                     platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
}

// the cuts a put requires at max: one for each read and one for each side
#define PRODUCT_CUTS_MAX (CARRIER_FORM_READS_MAX + 2u)

// The cuts the product requires, put together. One goes before the form for each value it reads that a register
// contains, and one after it for each side's product. Each is the carrier with the value stored in place of its
// answer, and nothing between. A cut the part refuses costs no other cut its answer. What each read came to goes
// into `put_reads`, and what each side wrote into `put_writes`
static void product_put(const ProductRound *round, ConceptReads *put_reads, ProductWrites *put_writes)
{
    const PutLink *const link = round->link;
    PairWork *const work = round->work;
    RunQuestion *const question = link->question;
    IdentityCode cut_codes[PRODUCT_CUTS_MAX];
    int cut_dangerous[PRODUCT_CUTS_MAX];
    // the stand-in's cut stores it at the form's link, its probe place; every other cut is the carrier's own and
    // contains none
    unsigned int cut_places[PRODUCT_CUTS_MAX];
    unsigned int cut_count = 0u;
    for (unsigned int at = 0u; at < round->reads_named->count; at += 1u)
    {
        const char *const read_text = round->reads_named->name[at].text;
        if ((literal_of(round->literal_reads, read_text) != NULL) || (read_text[0] == '['))
        {
            continue;
        }
        PlatformText cut;
        identity_text_begun(&cut);
        (void)carrier_cut_at_read(&link->flow->links, link->flow->cases_defined, round->chain_text, link->link_at,
                                  round->form_lines, read_text, "", 0, &cut);
        const unsigned long long links = cut_assembled(work, &cut, &cut_codes[cut_count]);
        if (s_trace_log != NULL)
        {
            cut_traced("reads ", read_text, ", the carrier cut at ", link->address, links,
                       (platform_text_size(&cut) != 0ULL) ? platform_text_bytes(&cut) : "");
        }
        platform_text_ended(&cut);
        cut_dangerous[cut_count] = 0;
        cut_places[cut_count] = (unsigned int)RUN_PLACE_NONE;
        cut_count += 1u;
    }
    // the form's product and its stand-in's, each the value it writes on every case as the part stores it at the
    // form. The carrier cut just after it gives the value: the links after the form are a delta of their own
    PlatformText found_copy;
    identity_text_begun(&found_copy);
    (void)mmgr_line_put_n(&found_copy.line, round->found.text, round->found.length);
    const char *const found_bytes = platform_text_bytes(&found_copy);
    const char *const sides[2] = {(found_bytes != NULL) ? found_bytes : "", link->located};
    const char *const side_names[2] = {"from", "to"};
    for (unsigned int side = 0u; side < 2u; side += 1u)
    {
        int wide = 0;
        CarrierName product;
        carrier_form_product(carrier_piece_of(sides[side]), &wide, &product);
        // the stand-in's side goes unwritten where a lesson blocks the stand-in from the part
        PlatformText cut;
        identity_text_begun(&cut);
        if ((product.text[0] != '\0') && !((side == 1u) && link->are_in_blocked))
        {
            (void)carrier_cut_at_read(&link->flow->links, link->flow->cases_defined, round->chain_text, link->link_at,
                                      round->form_lines, product.text, sides[side], wide, &cut);
        }
        const unsigned long long links = cut_assembled(work, &cut, &cut_codes[cut_count]);
        if (s_trace_log != NULL)
        {
            char name[CARRIER_NAME_BYTES + 8u];
            VerbaLinea name_line = mmgr_line_over(name, sizeof(name));
            mmgr_line_put(&name_line, side_names[side]);
            mmgr_line_put(&name_line, " ");
            mmgr_line_put(&name_line, product.text);
            (void)mmgr_line_finish(&name_line);
            cut_traced("writes ", name, ", the carrier cut after ", link->address, links,
                       (platform_text_size(&cut) != 0ULL) ? platform_text_bytes(&cut) : "");
        }
        platform_text_ended(&cut);
        cut_dangerous[cut_count] = (side == 1u) ? t_dangerous : 0;
        // a carrier's lines are far below 2^32
        cut_places[cut_count] = (side == 1u) ? (unsigned int)link->link_at : (unsigned int)RUN_PLACE_NONE;
        cut_count += 1u;
    }
    platform_text_ended(&found_copy);
    PlatformArray cut_answers;
    platform_array_begun(&cut_answers, sizeof(RunQuestion), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    cuts_queried(cut_codes, cut_count, cut_dangerous, cut_places, question, &work->queries, link->flow->registers,
                 &cut_answers);
    VerbaLinea *const line = query_log_line(s_query_log);
    unsigned long long cut_at = 0ULL;
    for (unsigned int at = 0u; (at < round->reads_named->count) && (put_reads->count < CONCEPT_READS_MAX); at += 1u)
    {
        const CarrierName *const read = &round->reads_named->name[at];
        const unsigned long long *const literal = literal_of(round->literal_reads, read->text);
        ConceptRead *const concept_read = &put_reads->read[put_reads->count];
        put_reads->count += 1u;
        concept_read->name = *read;
        concept_read->values = NULL;
        concept_read->count = 0ULL;
        // a parameter the chain binds to a literal is that literal on every case, and requires nothing
        if (literal != NULL)
        {
            mmgr_line_put(line, "reads ");
            mmgr_line_put(line, read->text);
            concept_read->values = read_values(question->cases);
            for (unsigned int place = 0u; (concept_read->values != NULL) && (place < question->cases); place += 1u)
            {
                mmgr_line_put(line, " ");
                mmgr_line_hex(line, *literal, 1u);
                concept_read->values[place] = *literal;
                concept_read->count += 1ULL;
            }
            mmgr_line_ch(line, '\n');
            continue;
        }
        // an operand of a case the form loads itself reads as the question contains it, and requires nothing
        if (read->text[0] == '[')
        {
            // the operand's number follows `[case `, six bytes, and is far below 2^32
            const unsigned int operand = (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg,
                                                                  .src = read->text + 6, .end = NULL, .base = 10);
            mmgr_line_put(line, "reads [");
            mmgr_line_decimal(line, operand, 1u);
            mmgr_line_put(line, "]");
            char name[32];
            VerbaLinea name_line = mmgr_line_over(name, sizeof(name));
            mmgr_line_ch(&name_line, '[');
            mmgr_line_decimal(&name_line, operand, 1u);
            mmgr_line_ch(&name_line, ']');
            const size_t name_length = mmgr_line_finish(&name_line);
            carrier_named(&concept_read->name, name, name_length);
            concept_read->values = read_values(question->cases);
            for (unsigned int place = 0u; (concept_read->values != NULL) && (place < question->cases); place += 1u)
            {
                const unsigned int *const words = question->word[place];
                const unsigned long long value =
                    ((2u * operand + 1u) < RUN_IN_WORDS)
                        ? (((unsigned long long)words[2u * operand + 1u] << 32u) | words[2u * operand])
                        : 0ULL;
                mmgr_line_put(line, " ");
                mmgr_line_hex(line, value, 1u);
                concept_read->values[place] = value;
                concept_read->count += 1ULL;
            }
            mmgr_line_ch(line, '\n');
            continue;
        }
        const RunQuestion *const cut = (const RunQuestion *)platform_array_at(&cut_answers, cut_at);
        const int unwritten = cut_codes[cut_at].count == 0ULL;
        cut_at += 1ULL;
        mmgr_line_put(line, "reads ");
        mmgr_line_put(line, read->text);
        if (unwritten || (cut->outcome != RUN_ANSWERED))
        {
            mmgr_line_put(line, " refused ");
            mmgr_line_put(line, unwritten ? "unwritten" : cut->refused);
            mmgr_line_ch(line, '\n');
            continue;
        }
        concept_read->values = read_values(question->cases);
        for (unsigned int place = 0u; (concept_read->values != NULL) && (place < question->cases); place += 1u)
        {
            mmgr_line_put(line, " ");
            mmgr_line_hex(line, cut->answered[place], 1u);
            concept_read->values[place] = cut->answered[place];
            concept_read->count += 1ULL;
        }
        mmgr_line_ch(line, '\n');
    }
    for (unsigned int side = 0u; side < 2u; side += 1u)
    {
        const RunQuestion *const cut = (const RunQuestion *)platform_array_at(&cut_answers, cut_at);
        const int unwritten = cut_codes[cut_at].count == 0ULL;
        cut_at += 1ULL;
        mmgr_line_put(line, "writes ");
        mmgr_line_put(line, side_names[side]);
        if (unwritten || (cut->outcome != RUN_ANSWERED))
        {
            mmgr_line_put(line, " refused ");
            mmgr_line_put(line, unwritten ? "unwritten" : cut->refused);
            mmgr_line_ch(line, '\n');
            continue;
        }
        put_writes->side[side] = read_values(question->cases);
        for (unsigned int place = 0u; (put_writes->side[side] != NULL) && (place < question->cases); place += 1u)
        {
            mmgr_line_put(line, " ");
            mmgr_line_hex(line, cut->answered[place], 1u);
            put_writes->side[side][place] = cut->answered[place];
            put_writes->count[side] += 1ULL;
        }
        mmgr_line_ch(line, '\n');
    }
    query_log_flushed(s_query_log);
    platform_array_ended(&cut_answers);
    for (unsigned int at = 0u; at < cut_count; at += 1u)
    {
        platform_array_ended(&cut_codes[at]);
    }
}

// the put's product made of its reads and writes, over the cases both sides wrote
static void product_made(const ConceptReads *reads, const ProductWrites *writes, const ConceptCased *cased,
                         ConceptProduct *product)
{
    const unsigned long long count = (writes->count[0] < writes->count[1]) ? writes->count[0] : writes->count[1];
    concept_product_made(reads, writes->side[0], writes->side[1], count, cased, product);
}

// the values the pair's forms write of their own on the query's log: `head`, then each value
static void qualifiers_logged(const char *head, const CarrierNumbers *qualifiers)
{
    VerbaLinea *const line = query_log_line(s_query_log);
    mmgr_line_put(line, head);
    for (unsigned int at = 0u; at < qualifiers->count; at += 1u)
    {
        mmgr_line_put(line, " ");
        mmgr_line_hex(line, qualifiers->number[at], 1u);
    }
    mmgr_line_ch(line, '\n');
}

// 1 where the terminated `value` is the terminated `other`
static int value_is(const char *value, const char *other)
{
    return EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = value, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = other,
                      .ci = EMBED_FALSE);
}

// What the form reads, in the order it names its parameters, into `form_reads`, and the literal of each read the chain
// binds to one into `literal_reads`. A parameter inside `[` and `]` shows where the form reads and is no value it
// reads. The registers the form reads follow, each once
static void form_reads_named(const PutLink *link, const Ruleset *sass, const CarrierReads *registers_named,
                             CarrierReads *form_reads, LiteralReads *literal_reads)
{
    PlatformArray tokens;
    identity_pieces_begun(&tokens);
    form_tokens(carrier_piece_of(link->from_text), &tokens);
    const IdentityName zero = ruleset_fixed_name(sass, "zero");
    int addressed = 0;
    for (unsigned long long at = 0ULL; at < tokens.count; at += 1ULL)
    {
        const CarrierPiece token = identity_piece_at(&tokens, at);
        addressed += identity_piece_is(token, "[") ? 1 : identity_piece_is(token, "]") ? -1 : 0;
        const unsigned int parameter =
            (token.length >= 3u) ? (unsigned int)(unsigned char)(token.text[1] - 'A') : link->from_names->count;
        const IdentityName *const value_name = (parameter < link->from_names->count)
                                                   ? pair_given_of(link->given, link->from_names->name[parameter].text)
                                                   : NULL;
        if ((token.text[0] != '\x01') || (addressed > 0) || (value_name == NULL))
        {
            continue;
        }
        const CarrierPiece part = carrier_after(token, 3u);
        const char *const value = value_name->text;
        char named[CARRIER_TEXT_MAX * 2u];
        VerbaLinea named_line = mmgr_line_over(named, sizeof(named));
        mmgr_line_put(&named_line, value);
        (void)mmgr_line_put_n(&named_line, part.text, part.length);
        const size_t named_length = mmgr_line_finish(&named_line);
        CarrierName read;
        read.text[0] = '\0';
        const int flag = value_is(value, "PT") || value_is(value, "!PT");
        if (carrier_reads_hold(registers_named, named))
        {
            carrier_named(&read, named, named_length);
        }
        else if (value_is(value, zero.text) || flag || literal_number(value))
        {
            // a flag bound to a literal reads as a flag, `P` before its name
            char literal_name[CARRIER_TEXT_MAX * 2u];
            VerbaLinea literal_line = mmgr_line_over(literal_name, sizeof(literal_name));
            mmgr_line_put(&literal_line, flag ? "P=" : "=");
            mmgr_line_put(&literal_line, link->from_names->name[parameter].text);
            (void)mmgr_line_put_n(&literal_line, part.text, part.length);
            const size_t literal_length = mmgr_line_finish(&literal_line);
            carrier_named(&read, literal_name, literal_length);
            const unsigned long long literal = value_is(value, "PT") ? 1ULL
                                               : (value_is(value, zero.text) || value_is(value, "!PT"))
                                                   ? 0ULL
                                                   : literal_value(value);
            literal_set(literal_reads, &read, literal);
        }
        if ((read.text[0] != '\0') && !carrier_reads_hold(form_reads, read.text))
        {
            carrier_reads_added(form_reads, &read);
        }
    }
    platform_array_ended(&tokens);
    for (unsigned int at = 0u; at < registers_named->count; at += 1u)
    {
        if (!carrier_reads_hold(form_reads, registers_named->name[at].text))
        {
            carrier_reads_added(form_reads, &registers_named->name[at]);
        }
    }
}

// 1 where every read of `reads` holds a value
static int reads_whole(const ConceptReads *reads)
{
    int whole = reads->count != 0u;
    for (unsigned int at = 0u; at < reads->count; at += 1u)
    {
        whole &= (reads->read[at].count != 0ULL) ? 1 : 0;
    }
    return whole;
}

// The put beneath its query in the log. The link as it was, and the link located in its place, each on one line of
// the log. Then, as names, the operands only the form it was gives. Then the product: what the form reads at the
// link, and what it and its stand-in write. The put queries them over the cases and puts them to the product.
//
// A product the question's cases leave short of whole goes again over every combination of the values the cases
// hold. 1 where a put added a row that came back one way, signal, 0 where it added none or only rows that came
// back both ways
int PutLink::logged(PairWork *work) const
{
    const char *const chain_text = flow->text;
    const CarrierPiece found_text = found_group(found, 0u, chain_text);
    VerbaLinea *const line = query_log_line(s_query_log);
    mmgr_line_put(line, "pair ");
    mmgr_line_put(line, to);
    mmgr_line_put(line, " in place of ");
    mmgr_line_put(line, from);
    mmgr_line_put(line, " at ");
    mmgr_line_put(line, address);
    mmgr_line_put(line, "\nfrom ");
    line_ends_spaced(line, found_text);
    mmgr_line_put(line, "\nto ");
    line_ends_spaced(line, carrier_piece_of(located));
    mmgr_line_put(line, "\nonly");
    for (unsigned int at = 0u; at < from_names->count; at += 1u)
    {
        const char *const name = from_names->name[at].text;
        const IdentityName *const value = pair_given_of(given, name);
        if ((value != NULL) && !pair_name_held(read_by_to, name))
        {
            mmgr_line_put(line, " ");
            mmgr_line_put(line, value->text);
        }
    }
    // each of the form's parameters the stand-in writes a literal in place of: one structure, bound
    if (unified && (fixed->count != 0u))
    {
        mmgr_line_put(line, "\nbinds");
        for (unsigned int marker = 0u; marker < FORM_MARKERS; marker += 1u)
        {
            if (fixed->held[marker] == 0u)
            {
                continue;
            }
            const unsigned int parameter = marker - (unsigned int)'A';
            mmgr_line_put(line, " ");
            mmgr_line_put(line, (parameter < from_names->count) ? from_names->name[parameter].text : "?");
            mmgr_line_put(line, "=");
            (void)mmgr_line_put_n(line, fixed->value[marker].text, fixed->value[marker].length);
        }
    }
    mmgr_line_put(line, "\nvector registers ");
    mmgr_line_decimal(line, question->registers, 1u);
    mmgr_line_put(line, " lanes ");
    mmgr_line_decimal(line, question->cases, 1u);
    mmgr_line_put(line, " operands ");
    mmgr_line_decimal(line, flow->operands, 1u);
    mmgr_line_put(line, " delta ");
    mmgr_line_decimal_signed(line, delta);
    mmgr_line_put(line, " seed ");
    mmgr_line_decimal(line, work->seed, 1u);
    mmgr_line_ch(line, '\n');
    // what the form's product holds. Each register it reads that a case reaches, and its value on every case as the
    // part stores it at the form. The carrier cut there stores the value in its place
    long form_lines = 0;
    for (size_t at = 0u; at < found_text.length; at += 1u)
    {
        form_lines += (found_text.text[at] == '\n') ? 1 : 0;
    }
    form_lines += ((found_text.length != 0u) && (found_text.text[found_text.length - 1u] == '\n')) ? 0 : 1;
    // What the form reads, in the order it names its parameters. A parameter the chain binds to a literal reads as
    // that literal on every case. So every carrier of the form reads it as one set of values. With none bound, the
    // registers go in the order the form names them
    static thread_local CarrierReads t_registers_named;
    static thread_local CarrierReads t_form_reads;
    static thread_local LiteralReads t_literal_reads;
    t_registers_named.count = 0u;
    t_form_reads.count = 0u;
    t_literal_reads.count = 0u;
    carrier_form_reads(&flow->links, flow->cases_defined, found_text, link_at, &t_registers_named);
    form_reads_named(this, work->sass, &t_registers_named, &t_form_reads, &t_literal_reads);
    const CarrierReads *const reads_named = (t_literal_reads.count == 0u) ? &t_registers_named : &t_form_reads;
    const ProductRound round = {this, work, chain_text, found_text, form_lines, reads_named, &t_literal_reads};
    // the values the pair's forms write of their own, each put to the product with every case value
    qualifiers_logged("qualifies", qualifiers);
    const unsigned long long signal_before = concept_signal(product);
    static thread_local ConceptReads t_put_reads;
    t_put_reads.count = 0u;
    ProductWrites put_writes = {{NULL, NULL}, {0ULL, 0ULL}};
    product_put(&round, &t_put_reads, &put_writes);
    product_made(&t_put_reads, &put_writes, pair_cased, product);
    // A product the question's cases leave short of whole goes over every combination of the values the cases hold.
    // That happens at each put of a shape no link read before whose every read the part answered. It goes run by
    // run, until the product is whole or the runs end
    const int read_whole = reads_whole(&t_put_reads);
    concept_reads_ended(&t_put_reads);
    product_writes_ended(&put_writes);
    if (shape_new && read_whole && !concept_whole(product, pair_cased))
    {
        const unsigned int runs = whole_cases(question, qualifiers->number, qualifiers->count, 0u);
        for (unsigned int run = 0u; (run < runs) && !concept_whole(product, pair_cased); run += 1u)
        {
            (void)whole_cases(question, qualifiers->number, qualifiers->count, run);
            VerbaLinea *const head = query_log_line(s_query_log);
            mmgr_line_put(head, "pair ");
            mmgr_line_put(head, to);
            mmgr_line_put(head, " in place of ");
            mmgr_line_put(head, from);
            mmgr_line_put(head, " at ");
            mmgr_line_put(head, address);
            mmgr_line_ch(head, '\n');
            qualifiers_logged("qualified", qualifiers);
            t_put_reads.count = 0u;
            ProductWrites whole_writes = {{NULL, NULL}, {0ULL, 0ULL}};
            product_put(&round, &t_put_reads, &whole_writes);
            product_made(&t_put_reads, &whole_writes, pair_cased, product);
            concept_reads_ended(&t_put_reads);
            product_writes_ended(&whole_writes);
        }
        stall_cases(question, places);
    }
    return (concept_signal(product) > signal_before) ? 1 : 0;
}
