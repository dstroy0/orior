// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// carriers.cu: the carriers of the pairs: their links, scratch and marks, and each pair's categories, lines beneath it
// and verdict row
#include "../../../../../../../includes/parsers/who.h"
#include "../../../../../../../includes/platform/platform.h"
#include "../../table_text.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "identity_internal.h"
#include "memoria_operor/memoria_operor.h"

int carriers_read(const char *engine, const HostAnswers *host, Carriers *carriers)
{
    carriers->count = 0ULL;
    carriers->chain =
        (CarrierChain *)platform_cell_cleared((host->count != 0ULL) ? host->count : 1ULL, sizeof(CarrierChain),
                                              platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT);
    if (carriers->chain == NULL)
    {
        return 0;
    }
    // the host's chains go in the order of their numbers, and so do the carriers
    for (unsigned long long at = 0ULL; at < host->count; at += 1ULL)
    {
        const char *const number = host->chain[at].number.text;
        char path[PLATFORM_PATH_BYTES];
        VerbaLinea path_line = mmgr_line_over(path, sizeof(path));
        mmgr_line_put(&path_line, engine);
        mmgr_line_ch(&path_line, '/');
        mmgr_line_put(&path_line, number);
        mmgr_line_put(&path_line, ".sass");
        (void)mmgr_line_finish(&path_line);
        size_t size = 0u;
        char *const text = identity_file_read(path, &size);
        if (text == NULL)
        {
            continue;
        }
        // the text kept on the persistent tier, where it lasts as long as the carriers
        CarrierChain *const chain = &carriers->chain[carriers->count];
        chain->text =
            (char *)platform_cell_taken(size + 1ULL, platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT);
        carrier_links_begun(&chain->links, PLATFORM_TIER_PERSISTENT);
        if (chain->text == NULL)
        {
            platform_cell_released(text, platform_place_g_program_owner);
            return 0;
        }
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = chain->text, .src = text, .bytes = size + 1u);
        platform_cell_released(text, platform_place_g_program_owner);
        chain->number = host->chain[at].number;
        chain->length = size;
        chain->cases_defined = -2;
        const CarrierPiece piece = {chain->text, size};
        if (!carrier_read(piece, &chain->links, &chain->cases_defined))
        {
            return 0;
        }
        CarrierNumbers loaded;
        loaded.count = 0u;
        carrier_operands_loaded(&chain->links, chain->cases_defined, &loaded);
        chain->registers = chain_registers(engine, number);
        chain->operands = loaded.count;
        carriers->count += 1ULL;
    }
    return 1;
}

// the scratch a writing takes past a chain's own registers, as scratch_past_taken gives it
static int scratch_past_taken(void *context, const char *bank, VerbaLinea *name)
{
    ScratchPast *const past = (ScratchPast *)context;
    char written[IDENTITY_REGISTER_BYTES];
    VerbaLinea line = mmgr_line_over(written, sizeof(written));
    int named = 0;
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = bank, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "predicate",
                   .ci = EMBED_FALSE))
    {
        named = ruleset_register_written(past->sass, bank, 5u, &line);
    }
    else
    {
        const unsigned int first = ((past->reserved + 1u) & ~1u) + *past->taken;
        *past->taken += EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = bank, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                   .other = "wide", .ci = EMBED_FALSE)
                            ? 2u
                            : 1u;
        named = ruleset_register_written(past->sass, bank, first, &line);
    }
    const size_t length = mmgr_line_finish(&line);
    if (!named || (length == 0u))
    {
        return 0;
    }
    (void)mmgr_line_put_n(name, written, length);
    return 1;
}

RulesetNamedScratch scratch_past(ScratchPast *past)
{
    const RulesetNamedScratch scratch = {scratch_past_taken, past};
    return scratch;
}

void ruleset_parameter_names(const Ruleset *rules, const char *form, FormNames *names)
{
    names->count = 0u;
    PlatformText read;
    ruleset_text_begun(&read);
    char why[RULESET_ERROR_BYTES];
    VerbaLinea error = mmgr_line_over(why, sizeof(why));
    (void)ruleset_text(rules->schema, rules->path, &read, &error);
    (void)mmgr_line_finish(&error);
    const unsigned long long size = platform_text_size(&read);
    const char *const bytes = platform_text_bytes(&read);
    const CarrierPiece text = {(bytes != NULL) ? bytes : "", (size_t)size};
    size_t cut = 0u;
    CarrierPiece line;
    while (carrier_piece_next(text, '\n', &cut, &line))
    {
        size_t at = 0u;
        CarrierPiece head;
        CarrierPiece name;
        if (!identity_word_next(line, &at, &head) || !identity_word_next(line, &at, &name) ||
            !identity_piece_is(name, form))
        {
            continue;
        }
        const int named = identity_piece_is(head, "form") || identity_piece_is(head, "err") ||
                          identity_piece_is(head, "nop") || identity_piece_is(head, "construct");
        if (!named)
        {
            continue;
        }
        CarrierPiece word;
        while (identity_word_next(line, &at, &word) && !identity_piece_is(word, "=") && (names->count < FORM_NAMES_MAX))
        {
            (void)IDENTITY_SET(names->name[names->count], word.text, word.length);
            names->count += 1u;
        }
        break;
    }
    platform_text_ended(&read);
}

// the scratch of a marked writing: each register a marker of its own past the parameters' markers
static int marked_scratch(void *context, const char *bank, VerbaLinea *name)
{
    (void)bank;
    unsigned int *const scratch_marker = (unsigned int *)context;
    *scratch_marker += 1u;
    mmgr_line_ch(name, '\x01');
    // a marker's letter counts from A, and a form has far fewer parameters than a char holds
    mmgr_line_ch(name, (char)('A' + *scratch_marker - 1u));
    mmgr_line_ch(name, '\x01');
    return 1;
}

int marked(const Ruleset *sass, const char *form, FormNames *names, PlatformText *out)
{
    ruleset_parameter_names(sass, form, names);
    char markers[FORM_NAMES_MAX][4];
    const char *arguments[FORM_NAMES_MAX];
    for (unsigned int at = 0u; at < names->count; at += 1u)
    {
        // a marker's letter counts from A, and a form has far fewer parameters than a char holds
        markers[at][0] = '\x01';
        markers[at][1] = (char)('A' + at);
        markers[at][2] = '\x01';
        markers[at][3] = '\0';
        arguments[at] = markers[at];
    }
    unsigned int scratch_marker = names->count;
    const RulesetNamedScratch scratch = {marked_scratch, &scratch_marker};
    return ruleset_form_put(sass, form, arguments, names->count, &scratch, out);
}

// 1 where the vector `left` is below the vector `right`: by the delta, then the registers, then the operands
static int vector_below(const FoundLink &left, const FoundLink &right)
{
    if (left.delta != right.delta)
    {
        return left.delta < right.delta;
    }
    if (left.registers != right.registers)
    {
        return left.registers < right.registers;
    }
    return left.operands < right.operands;
}

// 1 where two links have the same vector
static int vector_same(const FoundLink *left, const FoundLink *right)
{
    return (left->delta == right->delta) && (left->registers == right->registers) &&
           (left->operands == right->operands);
}

void links_of(const Carriers *carriers, unsigned long seed, const char *text, FormGroups *group, const char *drawn_for,
              PlatformArray *found_links)
{
    // the pattern of the form's lines, each marker an operand, the same marker the same operand
    PlatformText pattern;
    identity_text_begun(&pattern);
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    for (size_t at = 0u; at < length; at += 1u)
    {
        if ((text[at] == '\x01') && ((at + 2u) < length) && (text[at + 2u] == '\x01'))
        {
            const unsigned char marker = (unsigned char)text[at + 1u];
            if (group->group[marker] == 0u)
            {
                group->count += 1u;
                group->group[marker] = group->count;
                mmgr_line_put(&pattern.line, "([^,;\\s]+?)");
            }
            else
            {
                mmgr_line_ch(&pattern.line, '\\');
                mmgr_line_decimal(&pattern.line, group->group[marker], 1u);
            }
            at += 2u;
            continue;
        }
        const char character = text[at];
        // a byte of the text read as the unsigned byte the search takes
        const int special =
            (character != '\0') && (EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = "\\^$.|?*+()[]{}",
                                               .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = (uint8_t)character) != NULL);
        if (special)
        {
            mmgr_line_ch(&pattern.line, '\\');
        }
        mmgr_line_ch(&pattern.line, character);
    }
    TablePattern lines;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &lines, .bytes = sizeof(lines), .val = 0);
    const int read = table_pattern_read(&lines, platform_text_bytes(&pattern));
    platform_text_ended(&pattern);
    const unsigned long long first_found = found_links->count;
    for (unsigned long long chain_at = 0ULL; read && (chain_at < carriers->count); chain_at += 1ULL)
    {
        const CarrierChain *const chain = &carriers->chain[chain_at];
        const char *const chain_bytes = chain->text;
        TableWalk walk;
        table_walk_begun(&walk, &lines, chain_bytes, chain->length);
        long link_at = 0;
        unsigned long long counted_to = 0ULL;
        while (table_walk_next(&walk))
        {
            const RegexpSpan whole = walk.match.group[0];
            for (; counted_to < whole.from; counted_to += 1ULL)
            {
                link_at += (chain_bytes[counted_to] == '\n') ? 1 : 0;
            }
            const CarrierPiece found = {&chain_bytes[whole.from], (size_t)(whole.to - whole.from)};
            const long delta = carrier_form_delta(&chain->links, chain->cases_defined, found, link_at);
            FoundLink link;
            EMBED_CALL(memor.set, MemoriaCfg, .dst = &link, .bytes = sizeof(link), .val = 0);
            link.chain = chain->number;
            link.link_at = link_at;
            for (unsigned int group_at = 0u; group_at < FOUND_GROUPS; group_at += 1u)
            {
                link.group[group_at] = walk.match.group[group_at];
            }
            link.delta = (delta < 0) ? LONG_MAX : delta;
            link.registers = chain->registers;
            link.operands = chain->operands;
            (void)platform_array_appended(found_links, &link);
        }
        table_walk_ended(&walk);
    }
    if (read)
    {
        table_pattern_ended(&lines);
    }
    // The links in the order of their vectors, the least first. A form the cases reach as the carrier loaded them
    // answers of the form alone. The put tries links of one vector in an order drawn from the seed and the forms
    // `drawn_for` names alone. No other pair, put or settled, moves the order of these
    FoundLink *const own = (FoundLink *)platform_array_at(found_links, first_found);
    const unsigned long long own_count = found_links->count - first_found;
    if (own == NULL)
    {
        return;
    }
    (void)identity_sorted(own, own_count, vector_below);
    const unsigned long long drawn_hash = record_hash(
        drawn_for, EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = drawn_for, .cap = PLATFORM_TEXT_TO_TERMINATOR), seed);
    // the seed's words as the sequence takes them: the hash's low and high words, and the seed cut to a word
    const unsigned int words[3] = {(unsigned int)(drawn_hash & 0xffffffffULL), (unsigned int)(drawn_hash >> 32u),
                                   (unsigned int)seed};
    TableDraw drawn;
    table_draw_sequenced(&drawn, words, 3u);
    for (unsigned long long first = 0ULL; first < own_count;)
    {
        unsigned long long last = first;
        while ((last < own_count) && vector_same(&own[last], &own[first]))
        {
            last += 1ULL;
        }
        table_shuffled(&own[first], last - first, &drawn);
        first = last;
    }
}

void cuts_queried(const IdentityCode *codes, unsigned long long count, const int *dangerous, const unsigned int *places,
                  const RunQuestion *question, unsigned long long *queries, unsigned int mark, PlatformArray *questions)
{
    const unsigned int max = 255u;
    (void)platform_array_counted(questions, 0ULL);
    (void)platform_array_counted(questions, count);
    // the questions carried and each one's danger, the round and the round after, at max every cut
    RunQuestion **const carried =
        (RunQuestion **)platform_cell_taken(((count != 0ULL) ? count : 1ULL) * sizeof(RunQuestion *),
                                            platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    int *const carried_dangerous = (int *)platform_cell_taken(((count != 0ULL) ? count : 1ULL) * sizeof(int),
                                                              platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    if ((carried == NULL) || (carried_dangerous == NULL))
    {
        platform_cell_released(carried_dangerous, platform_place_g_program_owner);
        platform_cell_released(carried, platform_place_g_program_owner);
        return;
    }
    unsigned long long carried_count = 0ULL;
    for (unsigned long long at = 0ULL; at < count; at += 1ULL)
    {
        if (codes[at].count == 0ULL)
        {
            continue;
        }
        RunQuestion *const cut = (RunQuestion *)platform_array_at(questions, at);
        cut->code = (const unsigned char *)codes[at].cell;
        cut->code_size = codes[at].count;
        cut->registers = ((mark != 0u) && (mark < max)) ? mark : max;
        cut->place = (places != NULL) ? places[at] : RUN_PLACE_NONE;
        cut->cases = question->cases;
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = cut->word, .src = question->word,
                   .bytes = (size_t)question->cases * sizeof(question->word[0]));
        carried[carried_count] = cut;
        carried_dangerous[carried_count] = (dangerous != NULL) ? dangerous[at] : 0;
        carried_count += 1ULL;
    }
    *queries += carried_count;
    if (carried_count != 0ULL)
    {
        // one round's cuts are far below 2^32
        queries_carried(carried, carried_dangerous, (unsigned int)carried_count);
    }
    // the cuts refused at the mark go again at the max, after those kept in the cells' front
    unsigned long long again = 0ULL;
    for (unsigned long long at = 0ULL; at < carried_count; at += 1ULL)
    {
        RunQuestion *const cut = carried[at];
        if ((cut->outcome != RUN_ANSWERED) && (cut->registers != max))
        {
            cut->registers = max;
            carried[again] = cut;
            carried_dangerous[again] = carried_dangerous[at];
            again += 1ULL;
        }
    }
    *queries += again;
    if (again != 0ULL)
    {
        // one round's cuts are far below 2^32
        queries_carried(carried, carried_dangerous, (unsigned int)again);
    }
    platform_cell_released(carried_dangerous, platform_place_g_program_owner);
    platform_cell_released(carried, platform_place_g_program_owner);
}

// the operation the first token of `tokens` names, its word before the first `.`
static CarrierPiece first_stem(const PlatformArray *tokens)
{
    const CarrierPiece token = identity_piece_at(tokens, 0ULL);
    const char *const dot =
        EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = token.text, .cap = token.length, .byte = '.');
    // the dot lies within the token
    const CarrierPiece stem = {token.text, (dot != NULL) ? (size_t)(dot - token.text) : token.length};
    return stem;
}

// the conditions of `from` put into `into`, in their order and each once
static void conditions_joined(FormConditions *into, const FormConditions *from)
{
    for (unsigned int at = 0u; at < from->count; at += 1u)
    {
        int held = 0;
        for (unsigned int each = 0u; each < into->count; each += 1u)
        {
            held |= identity_same(into->condition[each].text, from->condition[at].text) ? 1 : 0;
        }
        if (held || (into->count >= FORM_CONDITIONS_MAX))
        {
            continue;
        }
        unsigned int place = into->count;
        while ((place > 0u) && (identity_order_text(into->condition[place - 1u].text, from->condition[at].text) > 0))
        {
            into->condition[place] = into->condition[place - 1u];
            place -= 1u;
        }
        into->condition[place] = from->condition[at];
        into->count += 1u;
    }
}

PairCategories pair_categories(const Ruleset *sass, const char *first, const char *second)
{
    FormNames first_names;
    FormNames second_names;
    PlatformText first_marked;
    PlatformText second_marked;
    identity_text_begun(&first_marked);
    identity_text_begun(&second_marked);
    (void)marked(sass, first, &first_names, &first_marked);
    (void)marked(sass, second, &second_names, &second_marked);
    const char *const first_bytes = platform_text_bytes(&first_marked);
    const char *const second_bytes = platform_text_bytes(&second_marked);
    const char *const first_text = (first_bytes != NULL) ? first_bytes : "";
    const char *const second_text = (second_bytes != NULL) ? second_bytes : "";
    static thread_local FormBinding t_bound;
    static thread_local FormBinding t_fixed;
    const int structural = (form_unified(sass, first_text, second_text, &t_bound, &t_fixed) && (t_fixed.count != 0u)) ||
                           (form_unified(sass, second_text, first_text, &t_bound, &t_fixed) && (t_fixed.count != 0u));
    const int launched =
        (first_text[0] != '\0') && (second_text[0] != '\0') && form_launched(first_text) && form_launched(second_text);
    PlatformArray first_tokens;
    PlatformArray second_tokens;
    identity_pieces_begun(&first_tokens);
    identity_pieces_begun(&second_tokens);
    form_tokens(carrier_piece_of(first_text), &first_tokens);
    form_tokens(carrier_piece_of(second_text), &second_tokens);
    int two_operations = (first_tokens.count != 0ULL) && (second_tokens.count != 0ULL);
    if (two_operations)
    {
        const CarrierPiece one = first_stem(&first_tokens);
        const CarrierPiece other = first_stem(&second_tokens);
        two_operations = (one.length != other.length) || (EMBED_CALL(memor.cmp, MemoriaCfg, .src = one.text,
                                                                     .other = other.text, .bytes = one.length) != 0);
    }
    platform_array_ended(&first_tokens);
    platform_array_ended(&second_tokens);
    FormConditions first_conditions;
    FormConditions second_conditions;
    FormConditions conditions;
    conditions.count = 0u;
    form_conditions(first_text, &first_conditions);
    form_conditions(second_text, &second_conditions);
    conditions_joined(&conditions, &first_conditions);
    conditions_joined(&conditions, &second_conditions);
    const int compared = (first_conditions.count != 0u) && (second_conditions.count != 0u);
    int ordered = 0;
    int equal = compared;
    for (unsigned int at = 0u; at < conditions.count; at += 1u)
    {
        const char *const condition = conditions.condition[at].text;
        ordered |= (identity_same(condition, "LT") || identity_same(condition, "LE") ||
                    identity_same(condition, "GT") || identity_same(condition, "GE"))
                       ? 1
                       : 0;
        equal &= (identity_same(condition, "EQ") || identity_same(condition, "NE")) ? 1 : 0;
    }
    const int placed = form_launched(first_text) || form_launched(second_text);
    const int ranged = form_ranged(first_text) || form_ranged(second_text);
    const int controlled = form_controls(first_text) || form_controls(second_text);
    const int switched = form_switches(first_text) || form_switches(second_text);
    const int operated = form_operates(first_text) || form_operates(second_text);
    const int commuted = form_commutes(first_text) || form_commutes(second_text);
    const int verbed = form_verbs(first_text) || form_verbs(second_text);
    platform_text_ended(&first_marked);
    platform_text_ended(&second_marked);
    PairCategories categories;
    VerbaLinea line = mmgr_line_over(categories.text, sizeof(categories.text));
    mmgr_line_put(&line, structural ? " structural" : "");
    mmgr_line_put(&line, launched ? " pragmatic" : "");
    mmgr_line_put(&line, two_operations ? " syntactic" : "");
    mmgr_line_put(&line, compared ? " comparison" : "");
    mmgr_line_put(&line, equal ? " equality" : "");
    mmgr_line_put(&line, (compared && ordered) ? " order" : "");
    mmgr_line_put(&line, operated ? " operation" : "");
    mmgr_line_put(&line, commuted ? " commutative" : "");
    mmgr_line_put(&line, verbed ? " verb" : "");
    mmgr_line_put(&line, ranged ? " range" : "");
    mmgr_line_put(&line, placed ? " vector" : "");
    mmgr_line_put(&line, controlled ? " control" : "");
    mmgr_line_put(&line, switched ? " switch" : "");
    (void)mmgr_line_finish(&line);
    return categories;
}

// 1 where the line `line` is `pair <one> <other>`
static int pair_line_is(CarrierPiece line, const char *one, const char *other)
{
    char wanted[CARRIER_TEXT_MAX * 2u];
    VerbaLinea wanted_line = mmgr_line_over(wanted, sizeof(wanted));
    mmgr_line_put(&wanted_line, "pair ");
    mmgr_line_put(&wanted_line, one);
    mmgr_line_ch(&wanted_line, ' ');
    mmgr_line_put(&wanted_line, other);
    (void)mmgr_line_finish(&wanted_line);
    return identity_piece_is(line, wanted);
}

// 1 where the piece begins with the terminated `head`
static int piece_starts(CarrierPiece piece, const char *head)
{
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = head, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    return (piece.length >= length) &&
           (EMBED_CALL(memor.cmp, MemoriaCfg, .src = piece.text, .other = head, .bytes = length) == 0);
}

void pair_beneath(const CarrierPiece *bridge, unsigned long long count, const char *first, const char *second,
                  PlatformArray *beneath)
{
    for (unsigned long long at = 0ULL; at < count; at += 1ULL)
    {
        if (!pair_line_is(bridge[at], first, second) && !pair_line_is(bridge[at], second, first))
        {
            continue;
        }
        for (unsigned long long under = at + 1ULL; under < count; under += 1ULL)
        {
            const CarrierPiece line = bridge[under];
            const int verdict = piece_starts(line, "open ") || piece_starts(line, "closed ");
            const int set =
                (line.length > 10u) &&
                (EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = line.text, .cap = line.length, .byte = ' ') == NULL) &&
                (EMBED_CALL(memor.cmp, MemoriaCfg, .src = &line.text[line.length - 10u], .other = "_coherence",
                            .bytes = 10u) == 0);
            const int named = piece_starts(line, "concept_coherence ") || piece_starts(line, "intent_coherence ");
            if (!verdict && !set && !named)
            {
                break;
            }
            (void)platform_array_appended(beneath, &line);
        }
        break;
    }
}

PairVerdict pair_verdicts_joined(const char *one_way, const char *other_way)
{
    PairVerdict joined;
    if (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = one_way, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                   .other = "closed ", .ci = EMBED_FALSE))
    {
        (void)IDENTITY_SET_TEXT(joined, one_way);
        return joined;
    }
    if (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = other_way, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                   .other = "closed ", .ci = EMBED_FALSE))
    {
        (void)IDENTITY_SET_TEXT(joined, other_way);
        return joined;
    }
    const unsigned long long one =
        EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = one_way, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "open ",
                   .ci = EMBED_FALSE)
            ? EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = one_way + 5, .end = NULL, .base = 10)
            : 0ULL;
    const unsigned long long other =
        EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = other_way, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                   .other = "open ", .ci = EMBED_FALSE)
            ? EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = other_way + 5, .end = NULL, .base = 10)
            : 0ULL;
    VerbaLinea line = mmgr_line_over(joined.text, sizeof(joined.text));
    mmgr_line_put(&line, "open ");
    mmgr_line_decimal(&line, one + other, 1u);
    (void)mmgr_line_finish(&line);
    return joined;
}

// on a pair's thread: 1 where a conference turned its put the other way round. Then 1 while its put is the other way
// round already or a witness. A conference then rules the relation waits and turns nothing again (overseer.h)
thread_local int t_turned = 0;

thread_local int t_other_way = 0;

void pair_row_passed(const char *first, const char *second, int read, unsigned long long reads, const char *verdict)
{
    const int closed = EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = verdict, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                  .other = "closed ", .ci = EMBED_FALSE);
    const int open = EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = verdict, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                .other = "open ", .ci = EMBED_FALSE);
    const size_t verdict_length =
        EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = verdict, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    const size_t closing_length = closed ? (verdict_length - 7u) : 0u;
    unsigned long long row[2u + 8] = {
        0ULL,
        0ULL,
        table_text(first, EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = first, .cap = PLATFORM_TEXT_TO_TERMINATOR)),
        table_text(second, EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = second, .cap = PLATFORM_TEXT_TO_TERMINATOR)),
        closed ? 1ULL : 0ULL,
        open ? EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = verdict + 5, .end = NULL, .base = 10) : 0ULL,
        table_text(closed ? (verdict + 7) : "", closing_length),
        read ? 1ULL : 0ULL,
        read ? reads : 0ULL,
        table_text(verdict, verdict_length)};
    who_words(WHO_HERE, row);
    qry_member_append("pairs", row, sizeof(row), QRY_ORDINARY);
}
