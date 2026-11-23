// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// log.cu: the log read off the run into the pairs, puts, case sets, metas, categories and choices, and each pair's set,
// product and identity decoded off its puts
#include "../../../../../../platform/platform.h"
#include "decoder_internal.h"
#include "memoria_operor/memoria_operor.h"

#include "../../../../../../../includes/file_defs/qry/qry_read.h"

// the log, every record of it joined in the writer's order
static unsigned char s_log[QRY_WRITER_RING_BYTES / 2ULL];

// `record` joined to the log where it is a record of the log and fits: 1, or 0 where it does not fit
static int log_record_joined(const QryRecord *record, unsigned long long *size)
{
    if ((record->name_length != 3u) ||
        (EMBED_CALL(memor.cmp, MemoriaCfg, .src = QRY_RECORD_NAME(record), .other = "log", .bytes = 3u) != 0))
    {
        return 1;
    }
    if (record->blob_length > (sizeof(s_log) - *size))
    {
        return 0;
    }
    // a record fits half a ring, whose bytes a size_t counts
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = s_log + *size, .src = QRY_RECORD_BLOB(record),
               .bytes = (size_t)record->blob_length);
    *size += record->blob_length;
    return 1;
}

// the log joined into s_log: from the run while its writer runs, from the .qry at `qry` once it stopped. Its
// bytes, or QRY_MEMBER_UNREAD where it does not read whole
static unsigned long long log_joined(const char *qry)
{
    unsigned long long size = 0ULL;
    unsigned long long at = 0ULL;
    const QryRecord *record = NULL;
    int whole = 1;
    if (qry_member_run() != NULL)
    {
        while (whole && qry_member_read(&at, &record))
        {
            whole = log_record_joined(record, &size);
        }
        return whole ? size : QRY_MEMBER_UNREAD;
    }
    unsigned long long mapped = 0ULL;
    const unsigned char *const bytes = qry_member_file_mapped(qry, &mapped);
    while (whole && (bytes != NULL) && qry_record_read(bytes, mapped, &at, &record))
    {
        whole = log_record_joined(record, &size);
    }
    whole = whole && (bytes != NULL) && (at == mapped);
    qry_member_file_unmapped(bytes, mapped);
    return whole ? size : QRY_MEMBER_UNREAD;
}

// a line on stdout after the decoder's name: `text`, then `tail`
static void decoder_said(const char *text, const char *tail)
{
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "orior_klq_decoder: ");
    mmgr_line_put(output_line, text);
    mmgr_line_put(output_line, tail);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
}

// 1 where `line` is `pair <to> in place of <from> at <address>`, eight words with one space between them, each a run of
// characters that are no whitespace, with the three into `to`, `from` and `address`
static int log_put_line(CarrierPiece line, CarrierPiece *to, CarrierPiece *from, CarrierPiece *address)
{
    CarrierPiece words[9];
    unsigned int count = 0u;
    size_t at = 0u;
    CarrierPiece word;
    while ((count < 9u) && carrier_piece_next(line, ' ', &at, &word))
    {
        if (word.length == 0u)
        {
            return 0;
        }
        for (size_t each = 0u; each < word.length; each += 1u)
        {
            if (carrier_space(word.text[each]) || (word.text[each] == '\f') || (word.text[each] == '\v'))
            {
                return 0;
            }
        }
        words[count] = word;
        count += 1u;
    }
    // a line that ends at a space has an empty word after it, which the pattern refuses
    const int trailing = (line.length != 0u) && (line.text[line.length - 1u] == ' ');
    if ((count != 8u) || trailing || !decoder_is(words[0], "pair") || !decoder_is(words[2], "in") ||
        !decoder_is(words[3], "place") || !decoder_is(words[4], "of") || !decoder_is(words[6], "at"))
    {
        return 0;
    }
    *to = words[1];
    *from = words[5];
    *address = words[7];
    return 1;
}

// the next line of `text` from `at`, its CR passed over: 1, or 0 past the last
static int line_next(CarrierPiece text, size_t *at, CarrierPiece *line)
{
    if (!carrier_piece_next(text, '\n', at, line))
    {
        return 0;
    }
    line->length -= ((line->length != 0u) && (line->text[line->length - 1u] == '\r')) ? 1u : 0u;
    return 1;
}

// the key a `facts <first> <second> ...` line names
static DecoderKey facts_key(CarrierPiece line)
{
    DecoderWords words;
    decoder_words(carrier_after(line, 6u), &words);
    const CarrierPiece none = {"", 0u};
    return decoder_key((words.count > 0u) ? words.word[0] : none, (words.count > 1u) ? words.word[1] : none);
}

// the key a `pair <first> <second>` line of the bridge names
static DecoderKey bridge_key(CarrierPiece line)
{
    DecoderWords words;
    decoder_words(line, &words);
    const CarrierPiece none = {"", 0u};
    return decoder_key((words.count > 1u) ? words.word[1] : none, (words.count > 2u) ? words.word[2] : none);
}

static int key_order(const void *left, const void *right)
{
    return decoder_order(carrier_piece_of(((const DecoderKey *)left)->text),
                         carrier_piece_of(((const DecoderKey *)right)->text));
}

// The keys of every pair the log and the bridge name, `count` of them, into the decoding's pairs, each once in the
// order of its key. 1, or 0 where the pool cannot meet a cell
static int pairs_gathered(Decoding *decoding, CarrierPiece log, unsigned long long count)
{
    DecoderKey *const keys = (DecoderKey *)decoder_cell(count, sizeof(DecoderKey));
    if (keys == NULL)
    {
        return 0;
    }
    unsigned long long filled = 0ULL;
    size_t at = 0u;
    CarrierPiece line;
    while (line_next(log, &at, &line) && (filled < count))
    {
        CarrierPiece to;
        CarrierPiece from;
        CarrierPiece address;
        if (decoder_begins(line, "choice "))
        {
            continue;
        }
        if (decoder_begins(line, "facts "))
        {
            keys[filled] = facts_key(line);
            filled += 1ULL;
        }
        else if (!decoder_begins(line, "binds") && !decoder_begins(line, "cases ") && !decoder_begins(line, "query ") &&
                 log_put_line(line, &to, &from, &address))
        {
            keys[filled] = decoder_key(from, to);
            filled += 1ULL;
        }
    }
    for (unsigned long long line_at = 0ULL; (line_at < decoding->bridge_count) && (filled < count); line_at += 1ULL)
    {
        if (decoder_begins(decoding->bridge[line_at], "pair "))
        {
            keys[filled] = bridge_key(decoding->bridge[line_at]);
            filled += 1ULL;
        }
    }
    if (!decoder_sorted(keys, filled, sizeof(DecoderKey), key_order))
    {
        platform_cell_released(keys, platform_place_g_program_owner);
        return 0;
    }
    unsigned long long unique = 0ULL;
    for (unsigned long long each = 0ULL; each < filled; each += 1ULL)
    {
        unique += ((each == 0ULL) || (key_order(&keys[each - 1ULL], &keys[each]) != 0)) ? 1ULL : 0ULL;
    }
    decoding->pairs = (DecoderPair *)decoder_cell(unique, sizeof(DecoderPair));
    if (decoding->pairs == NULL)
    {
        platform_cell_released(keys, platform_place_g_program_owner);
        return 0;
    }
    for (unsigned long long each = 0ULL; each < filled; each += 1ULL)
    {
        if ((each == 0ULL) || (key_order(&keys[each - 1ULL], &keys[each]) != 0))
        {
            decoding->pairs[decoding->pair_count].key = keys[each];
            decoding->pair_count += 1ULL;
        }
    }
    platform_cell_released(keys, platform_place_g_program_owner);
    return 1;
}

DecoderPair *Decoding::pair_of(const DecoderKey &key) const
{
    unsigned long long low = 0ULL;
    unsigned long long high = pair_count;
    while (low < high)
    {
        const unsigned long long middle = low + ((high - low) / 2ULL);
        if (key_order(&pairs[middle].key, &key) < 0)
        {
            low = middle + 1ULL;
        }
        else
        {
            high = middle;
        }
    }
    return ((low < pair_count) && (key_order(&pairs[low].key, &key) == 0)) ? &pairs[low] : NULL;
}

// What the main read of the log carries from line to line. What the last query came back as, and 1 where the part
// answered a query of the put to come. Then the pair the facts named last, the trace's flags of the lower witness and
// of an answer, and the categories' bits
struct LogState
{
    CarrierPiece came_back;
    int block_answered;
    DecoderPair *choosing;
    unsigned long long lower_flag;
    unsigned long long answer_flags;
    unsigned int category_bits;
};

// a `choice <flags> <link>` line counted: each flag of the trace it carries, for the pair the facts named last and for
// every pair
static void choice_read(Decoding *decoding, CarrierPiece line, LogState *state)
{
    const unsigned long long word = decoder_hex(carrier_after(line, 7u));
    if (((word & state->lower_flag) != 0ULL) && ((word & state->answer_flags) != 0ULL) && (state->choosing != NULL))
    {
        state->choosing->witnessed = 1;
    }
    for (unsigned int flag_at = 0u; flag_at < decoding->trace.count; flag_at += 1u)
    {
        if ((word & decoding->trace.flags[flag_at].word) == 0ULL)
        {
            continue;
        }
        if (state->choosing != NULL)
        {
            state->choosing->choice[flag_at] += 1u;
            state->choosing->choice_held |= 1ULL << flag_at;
        }
        decoding->chosen[flag_at] += 1u;
        decoding->chosen_held |= 1ULL << flag_at;
    }
}

// a `facts <first> <second> <category>...` line read: the pair named, and each word a category or a meta of it
static void facts_read(Decoding *decoding, CarrierPiece line, LogState *state)
{
    DecoderPair *const pair = decoding->pair_of(facts_key(line));
    state->choosing = pair;
    if (pair == NULL)
    {
        return;
    }
    pair->faceted = 1;
    DecoderWords words;
    decoder_words(carrier_after(line, 6u), &words);
    for (unsigned int at = 2u; at < words.count; at += 1u)
    {
        char set[256];
        VerbaLinea set_line = mmgr_line_over(set, sizeof(set));
        (void)mmgr_line_put_n(&set_line, words.word[at].text, words.word[at].length);
        mmgr_line_put(&set_line, "_coherence");
        const size_t length = mmgr_line_finish(&set_line);
        const unsigned int bit = decoder_name_bit(CarrierPiece{set, length});
        if ((bit & state->category_bits) != 0u)
        {
            pair->categories |= bit;
        }
        else
        {
            pair->metas |= bit;
        }
    }
}

// One line of the log read into the state and the pairs. The lines a put carries stay in its block, which put_viewed
// reads
static void log_line_read(Decoding *decoding, CarrierPiece line, LogState *state)
{
    LoggedPut *const last = (decoding->put_count != 0ULL) ? &decoding->puts[decoding->put_count - 1ULL] : NULL;
    CarrierPiece to;
    CarrierPiece from;
    CarrierPiece address;
    if (decoder_begins(line, "choice "))
    {
        choice_read(decoding, line, state);
    }
    else if (decoder_begins(line, "facts "))
    {
        facts_read(decoding, line, state);
    }
    else if ((last != NULL) && decoder_begins(line, "binds"))
    {
        decoding->pairs[last->pair].metas |= decoder_name_bit(carrier_piece_of("structural_coherence"));
    }
    else if (decoder_begins(line, "cases "))
    {
        decoding->case_sets[decoding->case_set_count] = carrier_after(line, 6u);
        decoding->case_set_count += 1ULL;
    }
    else if (decoder_begins(line, "query "))
    {
        // a put walks its registers over several queries, and what the part answered of every case is the same at
        // every count it answered at. The put keeps an answered query's cases over one read from R or refused
        DecoderWords words;
        decoder_words(line, &words);
        const CarrierPiece answer = (words.count > 2u) ? words.word[2] : CarrierPiece{"", 0u};
        const int answered = !decoder_is(answer, "recorded") && !decoder_is(answer, "refused");
        state->came_back = (answered || !state->block_answered) ? answer : state->came_back;
        state->block_answered |= answered ? 1 : 0;
    }
    else if (log_put_line(line, &to, &from, &address))
    {
        DecoderPair *const pair = decoding->pair_of(decoder_key(from, to));
        if (last != NULL)
        {
            // the lines beneath the put above end where this line begins
            last->block.length = (size_t)(line.text - last->block.text);
        }
        if (pair != NULL)
        {
            LoggedPut *const put = &decoding->puts[decoding->put_count];
            decoding->put_count += 1ULL;
            put->pair = (unsigned long long)(pair - decoding->pairs);
            put->from = from;
            put->to = to;
            put->address = address;
            put->came_back = state->came_back;
            put->cases_at = (decoding->case_set_count == 0ULL) ? 0ULL : (decoding->case_set_count - 1ULL);
            put->block.text = line.text + line.length;
            put->block.length = 0u;
        }
        state->came_back = CarrierPiece{"", 0u};
        state->block_answered = 0;
    }
}

// the lines of `log` counted: the puts, the case sets, and the keys of pairs the facts and puts name
static void log_counted(CarrierPiece log, unsigned long long *puts, unsigned long long *case_sets,
                        unsigned long long *keys)
{
    size_t at = 0u;
    CarrierPiece line;
    while (line_next(log, &at, &line))
    {
        CarrierPiece to;
        CarrierPiece from;
        CarrierPiece address;
        if (decoder_begins(line, "choice ") || decoder_begins(line, "binds") || decoder_begins(line, "query "))
        {
            continue;
        }
        if (decoder_begins(line, "facts "))
        {
            *keys += 1ULL;
        }
        else if (decoder_begins(line, "cases "))
        {
            *case_sets += 1ULL;
        }
        else if (log_put_line(line, &to, &from, &address))
        {
            *puts += 1ULL;
            *keys += 1ULL;
        }
    }
}

// the bridge at `klq` read from the run into the decoding's lines, each a piece of the run's bytes. 1, or 0 where it
// does not read
static int bridge_lines_read(Decoding *decoding, const char *klq)
{
    const unsigned char *bridge_bytes = NULL;
    unsigned long long bridge_size = 0ULL;
    if (!qry_member_file_blob(klq, &bridge_bytes, &bridge_size))
    {
        decoder_said(klq, " could not be read");
        return 0;
    }
    // a file read whole is at max what a size_t counts. Explicit cast reads its bytes as the text that they are
    const CarrierPiece text = {(const char *)bridge_bytes, (size_t)bridge_size};
    unsigned long long lines = 0ULL;
    size_t at = 0u;
    CarrierPiece line;
    while (line_next(text, &at, &line))
    {
        lines += 1ULL;
    }
    decoding->bridge = (CarrierPiece *)decoder_cell(lines, sizeof(CarrierPiece));
    if (decoding->bridge == NULL)
    {
        decoder_said(klq, ": the pool cannot meet the bridge's lines");
        return 0;
    }
    at = 0u;
    while (line_next(text, &at, &line))
    {
        decoding->bridge[decoding->bridge_count] = line;
        decoding->bridge_count += 1ULL;
    }
    return 1;
}

int Decoding::log_read(const char *qry, const char *klq)
{
    const unsigned long long joined_size = log_joined(qry);
    if (joined_size == QRY_MEMBER_UNREAD)
    {
        decoder_said("the log could not be read whole", "");
        return 1;
    }
    char trace_path[PLATFORM_PATH_BYTES];
    char trace_why[QUERY_TRACE_GLOSS_BYTES];
    VerbaLinea trace_error = mmgr_line_over(trace_why, sizeof(trace_why));
    const int trace_read =
        query_trace_path(trace_path, sizeof(trace_path)) && query_trace_read(trace_path, &trace, &trace_error);
    (void)mmgr_line_finish(&trace_error);
    if (!trace_read)
    {
        decoder_said(trace_why, "");
        return 1;
    }
    if (!bridge_lines_read(this, klq))
    {
        return 1;
    }
    // Explicit cast reads the log's bytes as the text that they are, whose size a size_t counts
    const CarrierPiece log = {(const char *)s_log, (size_t)joined_size};
    unsigned long long put_room = 0ULL;
    unsigned long long case_room = 0ULL;
    unsigned long long key_room = 0ULL;
    log_counted(log, &put_room, &case_room, &key_room);
    for (unsigned long long line_at = 0ULL; line_at < bridge_count; line_at += 1ULL)
    {
        key_room += decoder_begins(bridge[line_at], "pair ") ? 1ULL : 0ULL;
    }
    puts = (LoggedPut *)decoder_cell(put_room, sizeof(LoggedPut));
    case_sets = (CarrierPiece *)decoder_cell(case_room, sizeof(CarrierPiece));
    if ((puts == NULL) || (case_sets == NULL) || !pairs_gathered(this, log, key_room))
    {
        decoder_said("the pool cannot meet the log's cells", "");
        return 1;
    }
    LogState state = {{"", 0u}, 0, NULL, 0ULL, 0ULL, decoder_category_bits()};
    // the pairs the lower witness answered: a choice of a link of a third form the part answered alike or apart
    state.lower_flag = query_trace_flag(&trace, "lower");
    state.answer_flags = query_trace_flag(&trace, "alike") | query_trace_flag(&trace, "apart");
    size_t at = 0u;
    CarrierPiece line;
    while (line_next(log, &at, &line))
    {
        log_line_read(this, line, &state);
    }
    if (put_count != 0ULL)
    {
        LoggedPut *const last = &puts[put_count - 1ULL];
        // the lines beneath the last put end at the log's end
        last->block.length = (size_t)(log.text + log.length - last->block.text);
    }
    return 0;
}

// a put by its place in the log and its pair's, which orders the puts
struct PutOrder
{
    unsigned long long pair;
    unsigned long long put;
};

static int put_order(const void *left, const void *right)
{
    const unsigned long long one = ((const PutOrder *)left)->pair;
    const unsigned long long other = ((const PutOrder *)right)->pair;
    return (one == other) ? 0 : ((one < other) ? -1 : 1);
}

// The values of the cases, every operand of every case set, into `cased`. 1, or 0 where the pool cannot meet the cell
static int cases_valued(const Decoding *decoding, ConceptCased *cased)
{
    unsigned long long count = 0ULL;
    for (unsigned int pass = 0u; pass < 2u; pass += 1u)
    {
        unsigned long long *const operands =
            (pass == 1u) ? (unsigned long long *)decoder_cell(count, sizeof(unsigned long long)) : NULL;
        if ((pass == 1u) && (operands == NULL))
        {
            return 0;
        }
        unsigned long long filled = 0ULL;
        for (unsigned long long set = 0ULL; set < decoding->case_set_count; set += 1ULL)
        {
            size_t at = 0u;
            CarrierPiece written;
            while (carrier_piece_next(decoding->case_sets[set], ' ', &at, &written))
            {
                size_t cut = 0u;
                CarrierPiece operand;
                while (carrier_piece_next(written, ',', &cut, &operand))
                {
                    if (operands != NULL)
                    {
                        operands[filled] = decoder_hex(operand);
                    }
                    filled += 1ULL;
                }
            }
        }
        count = filled;
        if (operands != NULL)
        {
            concept_values(operands, count, cased);
            platform_cell_released(operands, platform_place_g_program_owner);
        }
    }
    return 1;
}

// What one pair's puts read it into. The sets its puts apart read it into, and 1 where a put came back apart. Then the
// marks of its puts alike, 1 where they were of one text and 2 where of two
struct PairAnswers
{
    unsigned int apart_sets;
    int apart;
    int alike;
};

// one put of `pair` read into its product and its answers
static void put_answered(const Decoding *decoding, const LoggedPut *put, const ConceptCased *qualified,
                         const char *engine, DecoderPair *pair, PairAnswers *answers)
{
    static PutView s_view;
    put_viewed(put, &s_view);
    // a put past the pair's first case apart requires nothing of its verdict, and records its product alone
    const unsigned long long count =
        (s_view.write_count[0] < s_view.write_count[1]) ? s_view.write_count[0] : s_view.write_count[1];
    concept_product_made(&s_view.reads, s_view.writes[0], s_view.writes[1], count, qualified, &pair->product);
    const CarrierPiece came = put->came_back;
    if (decoder_is(came, "recorded") || decoder_is(came, "refused") || (came.length == 0u) ||
        (decoding->case_set_count == 0ULL))
    {
        put_view_ended(&s_view);
        return;
    }
    const CarrierPiece from_stem = operation_stem(s_view.from_text);
    const CarrierPiece to_stem = operation_stem(s_view.to_text);
    const int one_text = decoder_order(to_stem, from_stem) == 0;
    if (EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = came.text, .cap = came.length, .byte = 'x') == NULL)
    {
        answers->alike |= one_text ? 1 : 2;
        put_view_ended(&s_view);
        return;
    }
    answers->apart = 1;
    const unsigned int set = put_decoded(put, &s_view, decoding->case_sets[put->cases_at], engine);
    if (set != 0u)
    {
        answers->apart_sets |= one_text ? set : decoder_set_bit("unknown_coherence");
    }
    put_view_ended(&s_view);
}

// The pair's sets read off its puts' answers. A pair is a member of every set its puts apart read it into, each a light
// of its own. It is a member of `unknown_coherence` where they read it into none. A pair whose every put the part
// answered came back alike is a qualifier's where its puts were of one text
static void pair_read_into(const PairAnswers *answers, DecoderPair *pair)
{
    const unsigned int unknown = decoder_set_bit("unknown_coherence");
    if (answers->apart)
    {
        // a set holds unknown_coherence apart where it holds another set as well
        unsigned int sets = answers->apart_sets;
        sets &= ((sets & ~unknown) != 0u) ? ~unknown : ~0u;
        pair->decoded = (sets != 0u) ? sets : unknown;
        pair->decoded_held = 1;
    }
    else if (answers->alike != 0)
    {
        pair->decoded = (answers->alike == 1) ? decoder_set_bit("qualifier_coherence") : unknown;
        pair->decoded_held = 1;
    }
}

// The pair's product read for its concept and its metas. A whole product names the pair's concept. Something outside
// the values the form reads decides a row both alike and apart: noise. An answered product the cycle left
// short of whole is one the cases could not make whole: the data's quality
static void pair_product_read(const ConceptCased *qualified, DecoderPair *pair)
{
    const int whole = concept_identity(&pair->product, qualified, pair->identity);
    pair->identity_held = whole;
    const unsigned int unknown = decoder_set_bit("unknown_coherence");
    if (whole && pair->decoded_held && (pair->decoded == unknown) && pair->qualifiers_held &&
        concept_qualified(&pair->product, &pair->qualifiers))
    {
        pair->decoded = decoder_set_bit("qualifier_coherence");
    }
    int both = 0;
    for (unsigned long long row = 0ULL; row < pair->product.rows.count; row += 1ULL)
    {
        both |= (concept_row_at(&pair->product, row)->value == (CONCEPT_ALIKE | CONCEPT_APART)) ? 1 : 0;
    }
    const int rows = pair->product.rows.count != 0ULL;
    if (both)
    {
        pair->metas |= decoder_name_bit(carrier_piece_of("noise_coherence"));
    }
    if (rows && !whole && pair->decoded_held)
    {
        pair->metas |= decoder_name_bit(carrier_piece_of("data_quality_coherence"));
    }
}

void Decoding::products_decoded(const char *engine)
{
    static ConceptCased s_cased;
    concept_cased_ended(&s_cased);
    PutOrder *const order = (PutOrder *)decoder_cell(put_count, sizeof(PutOrder));
    if (!cases_valued(this, &s_cased) || (order == NULL))
    {
        decoder_said("the pool cannot meet the products' cells", "");
        platform_cell_released(order, platform_place_g_program_owner);
        return;
    }
    // each pair's qualifiers, the values its forms write of their own over every put
    for (unsigned long long at = 0ULL; at < put_count; at += 1ULL)
    {
        order[at].pair = puts[at].pair;
        order[at].put = at;
        static PutView s_view;
        put_viewed(&puts[at], &s_view);
        DecoderPair *const pair = &pairs[puts[at].pair];
        for (unsigned int each = 0u; each < s_view.qualifiers.count; each += 1u)
        {
            carrier_number_added(&pair->qualifiers, s_view.qualifiers.number[each]);
        }
        pair->qualifiers_held = 1;
        put_view_ended(&s_view);
    }
    // the puts by pair, each pair's in the order of the log, so that each pair decodes whole in its turn
    (void)decoder_sorted(order, put_count, sizeof(PutOrder), put_order);
    static ConceptCased s_qualified;
    unsigned long long run = 0ULL;
    while (run < put_count)
    {
        DecoderPair *const pair = &pairs[order[run].pair];
        concept_cased_ended(&s_qualified);
        concept_values_qualified(&s_cased, &pair->qualifiers, &s_qualified);
        concept_product_begun(&pair->product, PLATFORM_TIER_PERSISTENT);
        pair->product_held = 1;
        PairAnswers answers = {0u, 0, 0};
        unsigned long long next = run;
        while ((next < put_count) && (order[next].pair == order[run].pair))
        {
            put_answered(this, &puts[order[next].put], &s_qualified, engine, pair, &answers);
            next += 1ULL;
        }
        pair_read_into(&answers, pair);
        // a pair no put of its own answered, witnessed at a third form's link. The part answered its two forms there,
        // alike or apart. The decoding knows the pair, and reads it out of unknown_coherence into witness_coherence
        if (pair->witnessed && !pair->decoded_held)
        {
            pair->decoded = decoder_set_bit("witness_coherence");
            pair->decoded_held = 1;
        }
        pair_product_read(&s_qualified, pair);
        run = next;
    }
    for (unsigned long long at = 0ULL; at < pair_count; at += 1ULL)
    {
        DecoderPair *const pair = &pairs[at];
        if (pair->witnessed && !pair->decoded_held)
        {
            pair->decoded = decoder_set_bit("witness_coherence");
            pair->decoded_held = 1;
        }
    }
    platform_cell_released(order, platform_place_g_program_owner);
}

void Decoding::ended()
{
    for (unsigned long long at = 0ULL; (pairs != NULL) && (at < pair_count); at += 1ULL)
    {
        if (pairs[at].product_held)
        {
            concept_product_ended(&pairs[at].product);
        }
    }
    platform_cell_released(pairs, platform_place_g_program_owner);
    platform_cell_released(case_sets, platform_place_g_program_owner);
    platform_cell_released(puts, platform_place_g_program_owner);
    platform_cell_released(bridge, platform_place_g_program_owner);
}
