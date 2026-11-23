// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pair_witness.cu: the lower witness: a pair whose puts queried nothing put at the links of a third form, both
// forms located in there and the part's two answers checked against each other
#include "../../../../../../platform/platform.h"
#include "memoria_operor/memoria_operor.h"
#include "pair.h"

// the arguments of a form at max, each a name, and how many it holds
struct WitnessArguments
{
    IdentityName argument[FORM_NAMES_MAX];
    const char *text[FORM_NAMES_MAX];
    unsigned int count;
};

// The arguments of a form of parameters `names`, into `arguments`. Each is its operand of the third form's link where
// `given` names it. Or else it is what the other form writes in its place, the operand `given` names of the other's
// parameter there or the literal it writes. 1 where every parameter has an argument
static int arguments_of(const FormNames *names, int unified, const FormBinding *bound, const FormNames *other_names,
                        const PairGiven *given, WitnessArguments *arguments)
{
    arguments->count = 0u;
    for (unsigned int at = 0u; at < names->count; at += 1u)
    {
        IdentityName *const argument = &arguments->argument[at];
        arguments->text[at] = argument->text;
        arguments->count += 1u;
        const IdentityName *const direct = pair_given_of(given, names->name[at].text);
        const unsigned int marker = (unsigned int)'A' + at;
        const int aligned = (marker < FORM_MARKERS) && (bound->held[marker] != 0u);
        if (direct != NULL)
        {
            *argument = *direct;
            continue;
        }
        if (!unified || !aligned)
        {
            return 0;
        }
        const CarrierPiece aligned_piece = bound->value[marker];
        if ((aligned_piece.length == 0u) || (aligned_piece.text[0] != '\x01'))
        {
            (void)IDENTITY_SET(*argument, aligned_piece.text, aligned_piece.length);
            continue;
        }
        // a marker's letter lies past its first byte
        const unsigned int other =
            (aligned_piece.length >= 2u) ? (unsigned int)(unsigned char)(aligned_piece.text[1] - 'A') : FORM_NAMES_MAX;
        const IdentityName *const through =
            (other < other_names->count) ? pair_given_of(given, other_names->name[other].text) : NULL;
        if (through == NULL)
        {
            return 0;
        }
        *argument = *through;
    }
    return 1;
}

// the pair read for the witness. Each form marked and its names, and each read against the other, its binding a piece
// of the other's text
struct WitnessForms
{
    FormNames first_names;
    FormNames second_names;
    PlatformText first_text;
    PlatformText second_text;
    FormBinding first_bound;
    FormBinding second_bound;
    FormBinding fixed;
    int first_unified;
    int second_unified;
};

// what the witness writes at one link: each form located there, its scratch, the two chains assembled, and the two
// questions put
struct WitnessSides
{
    PlatformText located[2];
    unsigned int scratch[2];
    PlatformArray codes;
};

// the sides' texts and codes given back
static void witness_sides_ended(WitnessSides *sides)
{
    identity_codes_ended(&sides->codes);
    platform_text_ended(&sides->located[1]);
    platform_text_ended(&sides->located[0]);
}

// The two forms written at the link `found` of `flow`, into `sides`, with the arguments `given` names. The answer is
// NULL where the ruleset writes both and both assemble, else the choice that passes the link over
static const char *witness_written(const PairWork *work, const WitnessForms *forms, const char *const names[2],
                                   const CarrierChain *flow, CarrierPiece found, CarrierPiece after,
                                   const PairGiven *given, WitnessSides *sides)
{
    // each form written in the link's place with the arguments the third form's link gives by name. Where it gives
    // none, through the pair's binding
    static thread_local WitnessArguments t_arguments[2];
    (void)arguments_of(&forms->first_names, forms->first_unified, &forms->first_bound, &forms->second_names, given,
                       &t_arguments[0]);
    (void)arguments_of(&forms->second_names, forms->second_unified, &forms->second_bound, &forms->first_names, given,
                       &t_arguments[1]);
    int writable = 1;
    for (unsigned int side = 0u; side < 2u; side += 1u)
    {
        sides->scratch[side] = 0u;
        ScratchPast past = {work->sass, flow->registers, &sides->scratch[side]};
        const RulesetNamedScratch taken = scratch_past(&past);
        writable = writable && ruleset_form_put(work->sass, names[side], t_arguments[side].text,
                                                t_arguments[side].count, &taken, &sides->located[side]);
    }
    if (!writable)
    {
        return "unwritable";
    }
    const unsigned long long first_size = platform_text_size(&sides->located[0]);
    const unsigned long long second_size = platform_text_size(&sides->located[1]);
    const char *const first_text = platform_text_bytes(&sides->located[0]);
    const char *const second_text = platform_text_bytes(&sides->located[1]);
    // two stand-ins of one text, the pair's binding writing each as the other, query the part for nothing
    if ((first_size == second_size) &&
        ((first_size == 0ULL) || (EMBED_CALL(memor.cmp, MemoriaCfg, .src = first_text, .other = second_text,
                                             .bytes = (size_t)first_size) == 0)))
    {
        return "unchanged";
    }
    PlatformText standing[2];
    const char *standing_texts[2];
    for (unsigned int side = 0u; side < 2u; side += 1u)
    {
        identity_text_begun(&standing[side]);
        // the link's first byte lies within the chain
        (void)mmgr_line_put_n(&standing[side].line, flow->text, (size_t)(found.text - flow->text));
        (void)mmgr_line_put_n(&standing[side].line, platform_text_bytes(&sides->located[side]),
                              (size_t)platform_text_size(&sides->located[side]));
        (void)mmgr_line_put_n(&standing[side].line, after.text, after.length);
        standing_texts[side] = platform_text_bytes(&standing[side]);
    }
    vendor_assembled(work->machine, standing_texts, 2ULL, &sides->codes);
    platform_text_ended(&standing[1]);
    platform_text_ended(&standing[0]);
    if ((sides->codes.count < 2ULL) ||
        (((const IdentityCode *)platform_array_at(&sides->codes, 0ULL))->count == 0ULL) ||
        (((const IdentityCode *)platform_array_at(&sides->codes, 1ULL))->count == 0ULL))
    {
        return "unassembled";
    }
    return NULL;
}

// The lower witness. A pair whose puts queried nothing, neither form located at a link that carries the cases, goes at
// the links of a third form. That form is the first of the bridge's keys whose parameters name every parameter of both
// and that is at such a link. Both forms are in at each of its links, and the part answers both over the cases. The
// witness checks the two answers against each other. The first case apart closes the pair as two operations, and
// alike on every case leaves it open with its count of cases.
//
// The witness passes over a link of a shape read before. It passes over one whose writing reaches nothing the part
// answers, where both stand-ins answer as the chain did as it was. The part answers two forms that read nothing but the
// register they write at the first link left. 1 with the verdict where the part answered both at a link, 0 otherwise
int PairWork::witness_lower(const char *first, const char *second, PairVerdict *verdict)
{
    static thread_local WitnessForms t_forms;
    WitnessForms &forms = t_forms;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &forms, .bytes = sizeof(forms), .val = 0);
    identity_text_begun(&forms.first_text);
    identity_text_begun(&forms.second_text);
    (void)marked(sass, first, &forms.first_names, &forms.first_text);
    (void)marked(sass, second, &forms.second_names, &forms.second_text);
    const char *const first_bytes = platform_text_bytes(&forms.first_text);
    const char *const second_bytes = platform_text_bytes(&forms.second_text);
    if ((first_bytes == NULL) || (second_bytes == NULL) || (first_bytes[0] == '\0') || (second_bytes[0] == '\0'))
    {
        platform_text_ended(&forms.second_text);
        platform_text_ended(&forms.first_text);
        return 0;
    }
    // each form read against the other token by token, as a put reads a stand-in against its form. What the other
    // writes in place of each of its parameters, the other's operand or a literal
    forms.first_unified = form_unified(sass, first_bytes, second_bytes, &forms.first_bound, &forms.fixed);
    forms.second_unified = form_unified(sass, second_bytes, first_bytes, &forms.second_bound, &forms.fixed);
    const char *const names[2] = {first, second};
    unsigned int alike_cases = 0u;
    int answered = 0;
    int closed = 0;
    static thread_local FormNames t_third_names;
    static thread_local FormGroups t_group;
    static thread_local PairGiven t_given;
    static thread_local WitnessArguments t_unused;
    for (unsigned long long key_at = 0ULL; !closed && (key_at < key_count); key_at += 1ULL)
    {
        IdentityName third_name;
        (void)IDENTITY_SET(third_name, keys[key_at].text, keys[key_at].length);
        if (identity_same(third_name.text, first) || identity_same(third_name.text, second))
        {
            continue;
        }
        PlatformText third_text;
        identity_text_begun(&third_text);
        (void)marked(sass, third_name.text, &t_third_names, &third_text);
        const char *const third_bytes = platform_text_bytes(&third_text);
        // the third form's parameters, each given its own name
        t_given.count = 0u;
        for (unsigned int at = 0u; at < t_third_names.count; at += 1u)
        {
            const char *const name = t_third_names.name[at].text;
            pair_given_set(&t_given, name, name,
                           EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR));
        }
        const int arguable = (third_bytes != NULL) && (third_bytes[0] != '\0') &&
                             arguments_of(&forms.first_names, forms.first_unified, &forms.first_bound,
                                          &forms.second_names, &t_given, &t_unused) &&
                             arguments_of(&forms.second_names, forms.second_unified, &forms.second_bound,
                                          &forms.first_names, &t_given, &t_unused);
        if (!arguable)
        {
            platform_text_ended(&third_text);
            continue;
        }
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &t_group, .bytes = sizeof(t_group), .val = 0);
        PlatformArray found_links;
        platform_array_begun(&found_links, sizeof(FoundLink), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
        char drawn_for[CARRIER_TEXT_MAX * 3u];
        VerbaLinea drawn_line = mmgr_line_over(drawn_for, sizeof(drawn_for));
        mmgr_line_put(&drawn_line, first);
        mmgr_line_ch(&drawn_line, ' ');
        mmgr_line_put(&drawn_line, second);
        mmgr_line_ch(&drawn_line, ' ');
        mmgr_line_put(&drawn_line, third_name.text);
        (void)mmgr_line_finish(&drawn_line);
        links_of(&carriers, seed, third_bytes, &t_group, drawn_for, &found_links);
        platform_text_ended(&third_text);
        PlatformText shapes;
        identity_text_begun(&shapes);
        int witnessed = 0;
        int stop = 0;
        for (unsigned long long found_at = 0ULL; !stop && !closed && (found_at < found_links.count); found_at += 1ULL)
        {
            const FoundLink *const found_link = (const FoundLink *)platform_array_at(&found_links, found_at);
            const CarrierChain *const flow = carrier_chain_of(&carriers, found_link->chain.text);
            if (flow == NULL)
            {
                continue;
            }
            const char *const chain_text = flow->text;
            const size_t chain_size = flow->length;
            const CarrierPiece found = found_group(found_link, 0u, chain_text);
            const long link_at = found_link->link_at;
            char link_address[CARRIER_TEXT_MAX];
            link_address_written(found_link->chain.text, link_at, link_address, sizeof(link_address));
            const char *lower[2] = {"lower", NULL};
            t_given.count = 0u;
            for (unsigned int marker = (unsigned int)'A'; marker < FORM_MARKERS; marker += 1u)
            {
                const unsigned int at_group = t_group.group[marker];
                const unsigned int parameter = marker - (unsigned int)'A';
                if ((at_group != 0u) && (parameter < t_third_names.count) && (at_group < FOUND_GROUPS))
                {
                    const CarrierPiece value = found_group(found_link, at_group, chain_text);
                    pair_given_set(&t_given, t_third_names.name[parameter].text, value.text, value.length);
                }
            }
            const unsigned int first_group = t_group.group[(unsigned int)'A'];
            const CarrierPiece written = ((first_group != 0u) && (first_group < FOUND_GROUPS))
                                             ? found_group(found_link, first_group, chain_text)
                                             : CarrierPiece{chain_text, 0u};
            const CarrierPiece written_register = register_of(written);
            const CarrierPiece after = {found.text + found.length,
                                        chain_size - (size_t)(found.text + found.length - chain_text)};
            const char *verdict_choice = NULL;
            if ((written_register.length == 0u) || !register_named_after(after, written_register))
            {
                verdict_choice = "unread";
            }
            else if (!carrier_form_carries(&flow->links, flow->cases_defined, found, link_at))
            {
                verdict_choice = "carrier_own";
            }
            else
            {
                PlatformText shape;
                identity_text_begun(&shape);
                read_shape(&flow->links, CarrierPiece{chain_text, chain_size}, flow->cases_defined, found, link_at,
                           &shape);
                verdict_choice = shape_added(&shapes, &shape) ? NULL : "shape_read";
                platform_text_ended(&shape);
            }
            if (verdict_choice != NULL)
            {
                lower[1] = verdict_choice;
                chosen(lower, 2u, link_address);
                continue;
            }
            if (!witnessed && (s_query_log != NULL))
            {
                VerbaLinea *const line = query_log_line(s_query_log);
                mmgr_line_put(line, "witness ");
                mmgr_line_put(line, first);
                mmgr_line_put(line, " ");
                mmgr_line_put(line, second);
                mmgr_line_put(line, " ");
                mmgr_line_put(line, third_name.text);
                mmgr_line_ch(line, '\n');
            }
            witnessed = 1;
            WitnessSides sides;
            identity_text_begun(&sides.located[0]);
            identity_text_begun(&sides.located[1]);
            platform_array_begun(&sides.codes, sizeof(IdentityCode), platform_place_g_program_owner,
                                 PLATFORM_TIER_TEMPORARY);
            const char *refusal = witness_written(this, &forms, names, flow, found, after, &t_given, &sides);
            const char *const first_text = platform_text_bytes(&sides.located[0]);
            const char *const second_text = platform_text_bytes(&sides.located[1]);
            // a stand-in a lesson of the overseer blocks answers as blocked, and neither goes to the part
            char overseen[160];
            const char *const chain_name = found_link->chain.text;
            if ((refusal == NULL) && (overseer_blocks(first_text, chain_name, overseen, sizeof(overseen)) ||
                                      overseer_blocks(second_text, chain_name, overseen, sizeof(overseen))))
            {
                refusal = "overseen";
            }
            // both put over the pair's cases at the carrier's registers. Where the part refuses one, it goes again and
            // declares every register
            RunQuestion *const put_sides =
                (refusal == NULL)
                    ? (RunQuestion *)platform_cell_cleared(2ULL, sizeof(RunQuestion), platform_place_g_program_owner,
                                                           PLATFORM_TIER_TEMPORARY)
                    : NULL;
            if ((refusal == NULL) && (put_sides == NULL))
            {
                refusal = "unassembled";
            }
            if (refusal != NULL)
            {
                lower[1] = refusal;
                chosen(lower, 2u, link_address);
                witness_sides_ended(&sides);
                continue;
            }
            const int dangerous[2] = {form_dangerous(first_text), form_dangerous(second_text)};
            RunQuestion *sides_put[2] = {&put_sides[0], &put_sides[1]};
            for (unsigned int side = 0u; side < 2u; side += 1u)
            {
                const IdentityCode *const code = (const IdentityCode *)platform_array_at(&sides.codes, side);
                put_sides[side].code = (const unsigned char *)platform_array_at(code, 0ULL);
                put_sides[side].code_size = code->count;
                // each stand-in is its side's probe place, at the third form's link; a carrier's lines are far below
                // 2^32
                put_sides[side].place = (unsigned int)link_at;
                put_sides[side].registers = flow->registers;
                put_sides[side].cases = t_question->cases;
                EMBED_CALL(memor.cpy, MemoriaCfg, .dst = put_sides[side].word, .src = t_question->word,
                           .bytes = (size_t)t_question->cases * sizeof(t_question->word[0]));
            }
            queries += 2ULL;
            queries_carried(sides_put, dangerous, 2u);
            for (unsigned int side = 0u; side < 2u; side += 1u)
            {
                if ((put_sides[side].outcome != RUN_ANSWERED) && (put_sides[side].registers != 255u))
                {
                    put_sides[side].registers = 255u;
                    queries += 1ULL;
                    queries_carried(&sides_put[side], &dangerous[side], 1u);
                }
            }
            if ((put_sides[0].outcome != RUN_ANSWERED) || (put_sides[1].outcome != RUN_ANSWERED))
            {
                // each side the part refused, or a carrier ended on, calls a conference of its own. A lesson it
                // teaches blocks the stand-in at the links after this one
                const char *const standings_of[2] = {first_text, second_text};
                for (unsigned int side = 0u; side < 2u; side += 1u)
                {
                    if ((put_sides[side].outcome != RUN_ANSWERED) && (put_sides[side].outcome != RUN_GATED))
                    {
                        char statement[CARRIER_TEXT_MAX * 6u];
                        VerbaLinea statement_line = mmgr_line_over(statement, sizeof(statement));
                        mmgr_line_put(&statement_line, "the pair ");
                        mmgr_line_put(&statement_line, first);
                        mmgr_line_put(&statement_line, " ");
                        mmgr_line_put(&statement_line, second);
                        mmgr_line_put(&statement_line, " witnessed at the link ");
                        mmgr_line_put(&statement_line, link_address);
                        mmgr_line_put(&statement_line, " of ");
                        mmgr_line_put(&statement_line, third_name.text);
                        mmgr_line_put(&statement_line, ", ");
                        mmgr_line_put(&statement_line, names[side]);
                        mmgr_line_put(&statement_line, " located there");
                        (void)mmgr_line_finish(&statement_line);
                        (void)conferred(first, second, statement, standings_of[side], chain_name, sides.scratch[side],
                                        &put_sides[side], "", link_address);
                    }
                }
                lower[1] = "refused";
                chosen(lower, 2u, link_address);
                platform_cell_released(put_sides, platform_place_g_program_owner);
                witness_sides_ended(&sides);
                continue;
            }
            answered = 1;
            unsigned int place = 0u;
            while ((place < put_sides[0].cases) && (put_sides[0].answered[place] == put_sides[1].answered[place]))
            {
                place += 1u;
            }
            const HostCases *const expected = host_cases_of(&host, chain_name);
            if (place < put_sides[0].cases)
            {
                lower[1] = "apart";
                chosen(lower, 2u, link_address);
                VerbaLinea closing_line = mmgr_line_over(verdict->text, sizeof(verdict->text));
                mmgr_line_put(&closing_line, "closed ");
                mmgr_line_put(&closing_line, link_address);
                mmgr_line_put(&closing_line, " ");
                case_text(t_places[place], &closing_line);
                mmgr_line_put(&closing_line, "->");
                mmgr_line_hex(&closing_line, put_sides[0].answered[place], 1u);
                mmgr_line_put(&closing_line, "/");
                mmgr_line_hex(&closing_line, put_sides[1].answered[place], 1u);
                (void)mmgr_line_finish(&closing_line);
                closed = 1;
            }
            else
            {
                // the link reaches what the part answers where a stand-in's answer leaves the host's for the chain as
                // it was at a case. A link whose writing reaches nothing answers alike whatever is there
                int reaches = 0;
                for (unsigned int at = 0u; (expected != NULL) && (at < put_sides[0].cases); at += 1u)
                {
                    const char *const bracket = host_case(expected, t_places[at]);
                    reaches |= ((bracket_contains(bracket, put_sides[0].answered[at]) == 0) ||
                                (bracket_contains(bracket, put_sides[1].answered[at]) == 0))
                                   ? 1
                                   : 0;
                }
                lower[1] = reaches ? "alike" : "unread";
                chosen(lower, 2u, link_address);
                if (reaches)
                {
                    alike_cases += put_sides[0].cases;
                    // two forms that read nothing but the register they write answer the same at every link. The
                    // first link that reaches what the part answers, where the part answers both, answers the question
                    stop = (forms.first_names.count == 1u) && (forms.second_names.count == 1u);
                }
            }
            platform_cell_released(put_sides, platform_place_g_program_owner);
            witness_sides_ended(&sides);
        }
        platform_text_ended(&shapes);
        platform_array_ended(&found_links);
        if (witnessed)
        {
            break;
        }
    }
    platform_text_ended(&forms.second_text);
    platform_text_ended(&forms.first_text);
    if (closed)
    {
        return 1;
    }
    if (answered)
    {
        VerbaLinea open_line = mmgr_line_over(verdict->text, sizeof(verdict->text));
        mmgr_line_put(&open_line, "open ");
        mmgr_line_decimal(&open_line, alike_cases, 1u);
        (void)mmgr_line_finish(&open_line);
    }
    return answered;
}
