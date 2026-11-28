// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// bridge.cu: each pair's concept folded, each pair closed with what it is read into written beneath it, the bridge
// passed back to the run's writer, and the decoding printed
#include "../../../../../../../includes/platform/platform.h"
#include "decoder_internal.h"

// 1 where a line of the bridge is a set's name alone, `<set>_coherence`
static int set_line(CarrierPiece entry)
{
    static const char s_suffix[] = "_coherence";
    const size_t suffix = sizeof(s_suffix) - 1u;
    return (EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = entry.text, .cap = entry.length, .byte = ' ') == NULL) &&
           (entry.length > suffix) &&
           (EMBED_CALL(memor.cmp, MemoriaCfg, .src = &entry.text[entry.length - suffix], .other = s_suffix,
                       .bytes = suffix) == 0);
}

// a line on stdout after the decoder's name: `klq`, then `tail`
static void bridge_said(const char *klq, const char *tail)
{
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "orior_klq_decoder: ");
    mmgr_line_put(output_line, klq);
    mmgr_line_put(output_line, tail);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
}

// the pair a `pair <first> <second>` line of the bridge names, or NULL where the decoding holds none
static DecoderPair *bridge_pair(const Decoding *decoding, CarrierPiece entry)
{
    DecoderWords words;
    decoder_words(entry, &words);
    const CarrierPiece none = {"", 0u};
    return decoding->pair_of(
        decoder_key((words.count > 1u) ? words.word[1] : none, (words.count > 2u) ? words.word[2] : none));
}

// a pair's concept by its place among the pairs, which orders the concepts
struct ConceptHeld
{
    CarrierPiece concept_text;
    unsigned long long pair;
};

static int concept_order(const void *left, const void *right)
{
    return decoder_order(((const ConceptHeld *)left)->concept_text, ((const ConceptHeld *)right)->concept_text);
}

void Decoding::concepts_folded()
{
    // each pair's concept as the bridge writes it, its own product's where this log answered it and otherwise the one
    // the bridge recorded beneath it. The decoding folds a pair whose concept another pair shares: one query answers
    // both
    DecoderPair *concept_pair = NULL;
    for (unsigned long long line_at = 0ULL; line_at < bridge_count; line_at += 1ULL)
    {
        const CarrierPiece entry = bridge[line_at];
        if (decoder_begins(entry, "pair "))
        {
            concept_pair = bridge_pair(this, entry);
        }
        else if ((concept_pair != NULL) && decoder_begins(entry, "concept_coherence ") && !concept_pair->decoded_held)
        {
            concept_pair->concept_text = carrier_after(entry, 18u);
            concept_pair->concept_held = 1;
        }
    }
    unsigned long long held = 0ULL;
    for (unsigned long long at = 0ULL; at < pair_count; at += 1ULL)
    {
        DecoderPair *const pair = &pairs[at];
        if (pair->identity_held)
        {
            pair->concept_text = carrier_piece_of(pair->identity);
            pair->concept_held = 1;
        }
        held += pair->concept_held ? 1ULL : 0ULL;
    }
    ConceptHeld *const concepts_held = (ConceptHeld *)decoder_cell(held, sizeof(ConceptHeld));
    if (concepts_held == NULL)
    {
        bridge_said("the concepts", ": the pool cannot meet their cell");
        return;
    }
    unsigned long long filled = 0ULL;
    for (unsigned long long at = 0ULL; at < pair_count; at += 1ULL)
    {
        if (pairs[at].concept_held)
        {
            concepts_held[filled].concept_text = pairs[at].concept_text;
            concepts_held[filled].pair = at;
            filled += 1ULL;
        }
    }
    (void)decoder_sorted(concepts_held, filled, sizeof(ConceptHeld), concept_order);
    const unsigned int folding = decoder_name_bit(carrier_piece_of("folding_coherence"));
    unsigned long long run = 0ULL;
    while (run < filled)
    {
        unsigned long long next = run + 1ULL;
        while ((next < filled) && (concept_order(&concepts_held[run], &concepts_held[next]) == 0))
        {
            next += 1ULL;
        }
        for (unsigned long long each = run; (next - run > 1ULL) && (each < next); each += 1ULL)
        {
            pairs[concepts_held[each].pair].metas |= folding;
        }
        run = next;
    }
    platform_cell_released(concepts_held, platform_place_g_program_owner);
}

// a line of the bridge written: the piece and its line's end
static void written_line(Decoding *decoding, CarrierPiece line)
{
    (void)mmgr_line_put_n(&decoding->written.line, line.text, line.length);
    mmgr_line_ch(&decoding->written.line, '\n');
}

// 1 where `sets` holds the piece `set` among the sets that are none of the decoder's names
static int foreign_held(const FoldSets *sets, CarrierPiece set)
{
    for (unsigned int at = 0u; at < sets->foreign_count; at += 1u)
    {
        if (decoder_order(sets->foreign[at], set) == 0)
        {
            return 1;
        }
    }
    return 0;
}

// the piece `set` put among the sets in the order of their names, where they do not hold it and hold room for it
static void foreign_added(FoldSets *sets, CarrierPiece set)
{
    if (foreign_held(sets, set) || (sets->foreign_count >= DECODER_FOREIGN_MAX))
    {
        return;
    }
    unsigned int at = sets->foreign_count;
    while ((at > 0u) && (decoder_order(sets->foreign[at - 1u], set) > 0))
    {
        sets->foreign[at] = sets->foreign[at - 1u];
        at -= 1u;
    }
    sets->foreign[at] = set;
    sets->foreign_count += 1u;
}

// the metas of the pair beneath written and counted. Each read now where the log read the pair's texts or answers, the
// fold always, and any other as the bridge recorded it
static void metas_written(Decoding *decoding, const DecoderPair *pair, int texts_read, int answered)
{
    const unsigned int folding = decoder_name_bit(carrier_piece_of("folding_coherence"));
    for (unsigned int meta_at = 0u; meta_at < DECODER_METAS; meta_at += 1u)
    {
        const unsigned int bit = DECODER_META_BIT(meta_at);
        const int now = texts_read || answered || (bit == folding);
        const int meta_read = (pair->metas & bit) != 0u;
        const int meta_known = (decoding->metas_beneath & bit) != 0u;
        if (now ? meta_read : meta_known)
        {
            written_line(decoding, carrier_piece_of(s_metas[meta_at]));
            decoding->counted[meta_at] += 1u;
        }
    }
}

// a set of the pair beneath written and counted with its pair's signal and noise. `at` is its place among the
// decoder's names, or DECODER_NAMES where it is none of them
static void set_written(Decoding *decoding, CarrierPiece set, unsigned int at, int named, unsigned long long signal,
                        unsigned long long noise)
{
    written_line(decoding, set);
    if (at >= DECODER_NAMES)
    {
        return;
    }
    decoding->counted[at] += 1u;
    decoding->named_pairs[at] += named ? 1u : 0u;
    decoding->set_signal[at] += signal;
    decoding->set_noise[at] += noise;
}

void Decoding::pair_closed()
{
    DecoderPair *const pair = pair_beneath;
    if (pair != NULL)
    {
        // a pair the log read the texts or the answers of hangs from the metas read of it now. Any other hangs from
        // the metas the bridge recorded. The decoding reads the fold off every pair's concept, and always now
        const int texts_read = pair->faceted;
        const int answered = pair->decoded_held;
        metas_written(this, pair, texts_read, answered);
        // every set the decoding reads the pair into, each a light of its own. The categories its texts read it into
        // and the sets its answers read it into. Each read now where the log read it, and otherwise as the bridge
        // recorded it
        const unsigned int category_bits = decoder_category_bits();
        unsigned int lit = sets_beneath.bits & (texts_read ? ~category_bits : ~0u) & (answered ? category_bits : ~0u);
        FoldSets foreign_lit;
        foreign_lit.foreign_count = 0u;
        for (unsigned int at = 0u; !answered && (at < sets_beneath.foreign_count); at += 1u)
        {
            foreign_added(&foreign_lit, sets_beneath.foreign[at]);
        }
        lit |= answered ? pair->decoded : 0u;
        lit |= texts_read ? pair->categories : 0u;
        // only an answer reads a pair out of `unknown_coherence`, a member of every category beneath
        const unsigned int unknown = decoder_set_bit("unknown_coherence");
        const int read_down = ((lit & ~category_bits & ~unknown) != 0u) || (foreign_lit.foreign_count != 0u);
        // each set's signal and noise. Its pairs, those a whole product names, and the rows of their products that
        // came back one way and both ways
        const unsigned long long signal_rows = pair->product_held ? concept_signal(&pair->product) : 0ULL;
        const unsigned long long noise_rows = pair->product_held ? (pair->product.rows.count - signal_rows) : 0ULL;
        // the sets in the tree's order, then the rest of `lit` in the order of their names before unknown_coherence.
        // Then unknown_coherence goes last where the pair is a member of it
        for (unsigned int set_at = 0u; set_at < (unsigned int)DECODER_COUNT(s_sets); set_at += 1u)
        {
            const unsigned int bit = DECODER_SET_BIT(set_at);
            if ((bit != unknown) && ((lit & bit) != 0u))
            {
                set_written(this, carrier_piece_of(s_sets[set_at]), DECODER_METAS + set_at, pair->identity_held,
                            signal_rows, noise_rows);
            }
        }
        for (unsigned int at = 0u; at < foreign_lit.foreign_count; at += 1u)
        {
            set_written(this, foreign_lit.foreign[at], DECODER_NAMES, pair->identity_held, signal_rows, noise_rows);
        }
        if (!read_down)
        {
            set_written(this, carrier_piece_of("unknown_coherence"),
                        decoder_name_at(carrier_piece_of("unknown_coherence")), pair->identity_held, signal_rows,
                        noise_rows);
        }
        // a pair answered in this log is the concept its own product is, and none until that product is whole
        char own[64];
        CarrierPiece identity = {"", 0u};
        if (pair->identity_held)
        {
            VerbaLinea identity_line = mmgr_line_over(own, sizeof(own));
            mmgr_line_put(&identity_line, "concept_coherence ");
            mmgr_line_put(&identity_line, pair->identity);
            identity.text = own;
            identity.length = mmgr_line_finish(&identity_line);
        }
        else if (!answered)
        {
            identity = identity_beneath;
        }
        if (identity.length != 0u)
        {
            written_line(this, identity);
            (void)mmgr_line_put_n(&concepts.line, identity.text, identity.length);
            mmgr_line_ch(&concepts.line, '\n');
        }
        // the intent the pair's forms carry of their own, the values each writes as the ruleset gives it
        if (pair->qualifiers_held && (pair->qualifiers.count != 0u))
        {
            for (unsigned int at = 0u; at < pair->qualifiers.count; at += 1u)
            {
                mmgr_line_put(&written.line, (at == 0u) ? "intent_coherence " : " ");
                mmgr_line_hex(&written.line, pair->qualifiers.number[at], 1u);
            }
            mmgr_line_ch(&written.line, '\n');
        }
        else if (!answered && (intent_beneath.length != 0u))
        {
            written_line(this, intent_beneath);
        }
    }
    pair_beneath = NULL;
    sets_beneath.bits = 0u;
    sets_beneath.foreign_count = 0u;
    metas_beneath = 0u;
    identity_beneath = CarrierPiece{"", 0u};
    intent_beneath = CarrierPiece{"", 0u};
}

void Decoding::bridge_folded()
{
    platform_text_begun(&written, 1ULL << 30u, platform_place_g_program_owner, PLATFORM_TIER_MOVABLE);
    platform_text_begun(&concepts, 1ULL << 30u, platform_place_g_program_owner, PLATFORM_TIER_MOVABLE);
    texts_begun = 1;
    for (unsigned long long at = 0ULL; at < bridge_count; at += 1ULL)
    {
        const CarrierPiece entry = bridge[at];
        const int verdict = decoder_begins(entry, "open ") || decoder_begins(entry, "closed ");
        const unsigned int named = decoder_name_at(entry);
        const int meta = named < DECODER_METAS;
        const int beneath = pair_beneath != NULL;
        if (beneath && meta)
        {
            metas_beneath |= 1u << named;
            continue;
        }
        if (beneath && set_line(entry))
        {
            if (named < DECODER_NAMES)
            {
                sets_beneath.bits |= 1u << named;
            }
            else
            {
                foreign_added(&sets_beneath, entry);
            }
            continue;
        }
        if (beneath && decoder_begins(entry, "concept_coherence "))
        {
            identity_beneath = entry;
            continue;
        }
        if (beneath && decoder_begins(entry, "intent_coherence "))
        {
            intent_beneath = entry;
            continue;
        }
        if (beneath && verdict)
        {
            written_line(this, entry);
            continue;
        }
        pair_closed();
        written_line(this, entry);
        if (decoder_begins(entry, "pair "))
        {
            pair_beneath = bridge_pair(this, entry);
        }
    }
    pair_closed();
}

int Decoding::bridge_passed(const char *klq)
{
    // the bridge passed to the run's writer, which writes it in the file's place (qry_member_filed)
    const unsigned long long size = platform_text_size(&written);
    const char *const bytes = platform_text_bytes(&written);
    const int filed = (bytes != NULL) && qry_member_filed(klq, bytes, size);
    if (!filed)
    {
        bridge_said(klq, " could not be written through the run's writer");
        return 1;
    }
    return 0;
}

// the choices of `counts` by the trace's flags onto `line`, each flag `held` counts: each count and flag, a comma
// between them
static void bridge_choices_written(const QueryTrace *trace, const unsigned int *counts, unsigned long long held,
                                   VerbaLinea *line)
{
    int first = 1;
    for (unsigned int flag_at = 0u; flag_at < trace->count; flag_at += 1u)
    {
        if ((held & (1ULL << flag_at)) != 0ULL)
        {
            mmgr_line_put(line, first ? "" : ", ");
            mmgr_line_decimal(line, counts[flag_at], 1u);
            mmgr_line_ch(line, ' ');
            mmgr_line_put(line, trace->flags[flag_at].name);
            first = 0;
        }
    }
}

// the names of the bits of `bits` onto `line` in the order of their names, each with a space after it
static void bridge_names_put(unsigned int bits, VerbaLinea *line)
{
    unsigned int left = bits;
    while (left != 0u)
    {
        unsigned int least = DECODER_NAMES;
        for (unsigned int at = 0u; at < DECODER_NAMES; at += 1u)
        {
            if (((left & (1u << at)) != 0u) &&
                ((least == DECODER_NAMES) || (decoder_order(carrier_piece_of(decoder_name_text(at)),
                                                            carrier_piece_of(decoder_name_text(least))) < 0)))
            {
                least = at;
            }
        }
        mmgr_line_put(line, decoder_name_text(least));
        mmgr_line_ch(line, ' ');
        left &= ~(1u << least);
    }
}

static int piece_order(const void *left, const void *right)
{
    return decoder_order(*(const CarrierPiece *)left, *(const CarrierPiece *)right);
}

// the concepts written, each once, and those more than one pair is: 1, or 0 where the pool cannot meet the cell
static int concepts_counted(PlatformText *concepts, unsigned long long *count, unsigned int *shared)
{
    *count = 0ULL;
    *shared = 0u;
    const char *const bytes = platform_text_bytes(concepts);
    // the text of the concepts is at max what a size_t counts
    const CarrierPiece text = {(bytes != NULL) ? bytes : "", (size_t)platform_text_size(concepts)};
    unsigned long long lines = 0ULL;
    size_t at = 0u;
    CarrierPiece line;
    while (carrier_piece_next(text, '\n', &at, &line))
    {
        lines += 1ULL;
    }
    CarrierPiece *const pieces = (CarrierPiece *)decoder_cell(lines, sizeof(CarrierPiece));
    if (pieces == NULL)
    {
        return 0;
    }
    unsigned long long filled = 0ULL;
    at = 0u;
    while (carrier_piece_next(text, '\n', &at, &line))
    {
        pieces[filled] = line;
        filled += 1ULL;
    }
    (void)decoder_sorted(pieces, filled, sizeof(CarrierPiece), piece_order);
    unsigned long long run = 0ULL;
    while (run < filled)
    {
        unsigned long long next = run + 1ULL;
        while ((next < filled) && (piece_order(&pieces[run], &pieces[next]) == 0))
        {
            next += 1ULL;
        }
        *count += 1ULL;
        *shared += ((next - run) > 1ULL) ? 1u : 0u;
        run = next;
    }
    platform_cell_released(pieces, platform_place_g_program_owner);
    return 1;
}

void Decoding::printed(const char *klq)
{
    for (unsigned long long at = 0ULL; at < pair_count; at += 1ULL)
    {
        const DecoderPair *const pair = &pairs[at];
        if (!pair->decoded_held)
        {
            continue;
        }
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, pair->key.text);
        mmgr_line_put(output_line, ": ");
        bridge_names_put(pair->metas, output_line);
        bridge_names_put(pair->categories, output_line);
        bridge_names_put(pair->decoded, output_line);
        mmgr_line_put(output_line, pair->identity_held ? pair->identity : "-");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    unsigned long long concept_count = 0ULL;
    unsigned int shared = 0u;
    (void)concepts_counted(&concepts, &concept_count, &shared);
    // the choices of every pair's puts by the trace's flags. Then those of each pair the part answered nothing of, the
    // questions a further pass takes up
    if (chosen_held != 0ULL)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  choices: ");
        bridge_choices_written(&trace, chosen, chosen_held, output_line);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    const unsigned long long answered_flags = query_trace_flag(&trace, "alike") | query_trace_flag(&trace, "apart");
    for (unsigned long long at = 0ULL; at < pair_count; at += 1ULL)
    {
        const DecoderPair *const pair = &pairs[at];
        int answered = 0;
        for (unsigned int flag_at = 0u; flag_at < trace.count; flag_at += 1u)
        {
            answered |= (((pair->choice_held & (1ULL << flag_at)) != 0ULL) &&
                         ((trace.flags[flag_at].word & answered_flags) != 0ULL))
                            ? 1
                            : 0;
        }
        if ((pair->choice_held != 0ULL) && (pair->key.text[0] != '\0') && !pair->decoded_held && !answered)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  answered nothing, ");
            mmgr_line_put(output_line, pair->key.text);
            mmgr_line_put(output_line, ": ");
            bridge_choices_written(&trace, pair->choice, pair->choice_held, output_line);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    // each set's pairs, those a whole product names, and its signal against its noise in rows of the products
    for (unsigned int set_at = 0u; set_at < (unsigned int)DECODER_COUNT(s_sets); set_at += 1u)
    {
        const unsigned int at = DECODER_METAS + set_at;
        if (counted[at] != 0u)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, s_sets[set_at]);
            mmgr_line_put(output_line, ": ");
            mmgr_line_decimal(output_line, counted[at], 1u);
            mmgr_line_put(output_line, " pairs, ");
            mmgr_line_decimal(output_line, named_pairs[at], 1u);
            mmgr_line_put(output_line, " named by a concept, ");
            mmgr_line_decimal(output_line, set_signal[at], 1u);
            mmgr_line_put(output_line, " rows of signal, ");
            mmgr_line_decimal(output_line, set_noise[at], 1u);
            mmgr_line_put(output_line, " of noise");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "orior_klq_decoder: ");
    mmgr_line_decimal(output_line, put_count, 1u);
    mmgr_line_put(output_line, " puts read, ");
    mmgr_line_decimal(output_line, concept_count, 1u);
    mmgr_line_put(output_line, " concepts, ");
    mmgr_line_decimal(output_line, shared, 1u);
    mmgr_line_put(output_line, " of more than one pair; metas");
    for (unsigned int meta_at = 0u; meta_at < DECODER_METAS; meta_at += 1u)
    {
        mmgr_line_ch(output_line, ' ');
        mmgr_line_decimal(output_line, counted[meta_at], 1u);
        mmgr_line_ch(output_line, ' ');
        mmgr_line_put(output_line, s_metas[meta_at]);
    }
    mmgr_line_put(output_line, "; sets");
    for (unsigned int set_at = 0u; set_at < (unsigned int)DECODER_COUNT(s_sets); set_at += 1u)
    {
        mmgr_line_ch(output_line, ' ');
        mmgr_line_decimal(output_line, counted[DECODER_METAS + set_at], 1u);
        mmgr_line_ch(output_line, ' ');
        mmgr_line_put(output_line, s_sets[set_at]);
    }
    mmgr_line_put(output_line, "; written to ");
    mmgr_line_put(output_line, klq);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
}
