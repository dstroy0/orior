// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pair_put.cu: the put: one form was in for by the other at each of its links in each carrier, until the
// part answers one apart
#include "../../../../../../../includes/platform/platform.h"
#include "pair.h"

// a line of the trace: `pair <to> in place of <from> at <address>: <tail>`
static void put_traced(const char *to, const char *from, const char *address, const char *tail)
{
    VerbaLinea *const line = query_log_line(s_trace_log);
    mmgr_line_put(line, "pair ");
    mmgr_line_put(line, to);
    mmgr_line_put(line, " in place of ");
    mmgr_line_put(line, from);
    mmgr_line_put(line, " at ");
    mmgr_line_put(line, address);
    mmgr_line_put(line, ": ");
    mmgr_line_put(line, tail);
    mmgr_line_ch(line, '\n');
    query_log_flushed(s_trace_log);
}

// What a put reads before any link. The two forms marked, their names, and the stand-in read against the form. Then
// the form's parameters the stand-in reads, the values the forms write of their own and the values the product is
// whole over
struct PutForms
{
    FormNames from_names;
    FormNames to_names;
    PlatformText from_text;
    PlatformText to_text;
    FormBinding bound;
    FormBinding fixed;
    int unified;
    int const_loaded;
    PairNames read_by_to;
    CarrierNumbers qualifiers;
    ConceptCased pair_cased;
};

// the forms of the put of `to` in place of `from` read into `forms`: 1, or 0 where the ruleset does not write either
// form
static int put_forms_read(const Ruleset *sass, const ConceptCased *cased, const char *from, const char *to,
                          PutForms *forms)
{
    identity_text_begun(&forms->from_text);
    identity_text_begun(&forms->to_text);
    (void)marked(sass, from, &forms->from_names, &forms->from_text);
    (void)marked(sass, to, &forms->to_names, &forms->to_text);
    const char *const from_bytes = platform_text_bytes(&forms->from_text);
    const char *const to_bytes = platform_text_bytes(&forms->to_text);
    const char *const from_written = (from_bytes != NULL) ? from_bytes : "";
    const char *const to_written = (to_bytes != NULL) ? to_bytes : "";
    // the stand-in read against the form token by token. What the form writes in place of the stand-in's parameters,
    // and each of the form's parameters the stand-in writes a literal in place of
    forms->unified = form_unified(sass, to_written, from_written, &forms->bound, &forms->fixed);
    // a const load and a plain one. The first link alike is the part's answer, and the question stays open for the
    // chain to settle. Or it stays open for a lower identity to witness where the rest of the chain fails
    forms->const_loaded = form_const_loaded(from_written, to_written);
    // the form's parameters the stand-in reads: by name, or in place of one of its own
    forms->read_by_to.count = 0u;
    for (unsigned int at = 0u; at < forms->to_names.count; at += 1u)
    {
        pair_name_added(&forms->read_by_to, forms->to_names.name[at].text);
    }
    for (unsigned int marker = 0u; marker < FORM_MARKERS; marker += 1u)
    {
        const CarrierPiece aligned = forms->bound.value[marker];
        if ((forms->bound.held[marker] == 0u) || !forms->unified || (aligned.length < 2u) ||
            (aligned.text[0] != '\x01'))
        {
            continue;
        }
        const unsigned int parameter = (unsigned int)(unsigned char)(aligned.text[1] - 'A');
        if (parameter < forms->from_names.count)
        {
            pair_name_added(&forms->read_by_to, forms->from_names.name[parameter].text);
        }
    }
    // the values the two forms write of their own, as the ruleset gives them, and the values the pair's product is
    // whole over with them
    forms->qualifiers.count = 0u;
    carrier_literals(carrier_piece_of(from_written), &forms->qualifiers);
    carrier_literals(carrier_piece_of(to_written), &forms->qualifiers);
    concept_cased_ended(&forms->pair_cased);
    concept_values_qualified(cased, &forms->qualifiers, &forms->pair_cased);
    return (from_written[0] != '\0') && (to_written[0] != '\0');
}

// What a link gives each parameter of the form by name, into `given`: what the group of its marker matched
static void link_given(const FormGroups *group, const FormNames *from_names, const FoundLink *found_link,
                       const char *chain_text, PairGiven *given)
{
    given->count = 0u;
    for (unsigned int marker = (unsigned int)'A'; marker < FORM_MARKERS; marker += 1u)
    {
        const unsigned int at_group = group->group[marker];
        const unsigned int parameter = marker - (unsigned int)'A';
        if ((at_group != 0u) && (parameter < from_names->count) && (at_group < FOUND_GROUPS))
        {
            const CarrierPiece value = found_group(found_link, at_group, chain_text);
            pair_given_set(given, from_names->name[parameter].text, value.text, value.length);
        }
    }
}

// Each of the stand-in's parameters given by name, or where the link gives none, by what the form writes in its place,
// token by token. That is an argument of the form, or a literal the form writes there. Each goes into `arguments`. 1,
// or 0 where one is unbound
static int stand_in_arguments(const PutForms *forms, const PairGiven *given, IdentityName *arguments)
{
    for (unsigned int parameter = 0u; parameter < forms->to_names.count; parameter += 1u)
    {
        const char *const name = forms->to_names.name[parameter].text;
        const unsigned int marker = (unsigned int)'A' + parameter;
        const int aligned = (marker < FORM_MARKERS) && (forms->bound.held[marker] != 0u);
        const CarrierPiece aligned_piece = aligned ? forms->bound.value[marker] : CarrierPiece{"", 0u};
        const IdentityName *const direct = pair_given_of(given, name);
        // a marker's letter lies past its first byte, and an empty binding names none
        const unsigned int through =
            (aligned_piece.length >= 2u) ? (unsigned int)(unsigned char)(aligned_piece.text[1] - 'A') : FORM_NAMES_MAX;
        const int marked_binding = aligned && (aligned_piece.length != 0u) && (aligned_piece.text[0] == '\x01');
        const IdentityName *const by_form = (forms->unified && marked_binding && (through < forms->from_names.count))
                                                ? pair_given_of(given, forms->from_names.name[through].text)
                                                : NULL;
        if (direct != NULL)
        {
            arguments[parameter] = *direct;
        }
        else if (by_form != NULL)
        {
            arguments[parameter] = *by_form;
        }
        else if (forms->unified && aligned && !marked_binding)
        {
            (void)IDENTITY_SET(arguments[parameter], aligned_piece.text, aligned_piece.length);
        }
        else
        {
            return 0;
        }
    }
    return 1;
}

// `from` was in for by `to` at each of its links in each carrier, until the part answers one apart. 1 with its
// verdict, closed at the first link apart or open with the cases alike at every link the part answered. 0 where the
// part answered none
int PairWork::put(const char *from, const char *to, PairVerdict *verdict)
{
    RunQuestion *const question = t_question;
    unsigned int *const places = t_places;
    static thread_local PutForms t_forms;
    PutForms &forms = t_forms;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &forms, .bytes = sizeof(forms), .val = 0);
    if (!put_forms_read(sass, &cased, from, to, &forms))
    {
        platform_text_ended(&forms.to_text);
        platform_text_ended(&forms.from_text);
        return 0;
    }
    const char *const from_written = platform_text_bytes(&forms.from_text);
    static thread_local FormGroups t_group;
    FormGroups &group = t_group;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &group, .bytes = sizeof(group), .val = 0);
    PlatformArray found_links;
    platform_array_begun(&found_links, sizeof(FoundLink), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    char drawn_for[CARRIER_TEXT_MAX * 2u];
    {
        VerbaLinea drawn_line = mmgr_line_over(drawn_for, sizeof(drawn_for));
        mmgr_line_put(&drawn_line, from);
        mmgr_line_ch(&drawn_line, ' ');
        mmgr_line_put(&drawn_line, to);
        (void)mmgr_line_finish(&drawn_line);
    }
    links_of(&carriers, seed, from_written, &group, drawn_for, &found_links);
    // the pair's product over every put, the shapes of what the form read at each, and its verdict where a put
    // closed it
    unsigned int alike_cases = 0u;
    int answered = 0;
    ConceptProduct product;
    concept_product_begun(&product, PLATFORM_TIER_TEMPORARY);
    PlatformText shapes;
    identity_text_begun(&shapes);
    char closed[CARRIER_TEXT_MAX];
    closed[0] = '\0';
    long band = LONG_MIN;
    int band_truthy = 0;
    int ended = 0;
    int result = -1;
    if (found_links.count == 0ULL)
    {
        chose("no_link", "-");
    }
    static thread_local PairGiven t_given;
    PairGiven &given = t_given;
    for (unsigned long long found_at = 0ULL; !ended && (found_at < found_links.count); found_at += 1ULL)
    {
        const FoundLink *const found_link = (const FoundLink *)platform_array_at(&found_links, found_at);
        const CarrierChain *const flow = carrier_chain_of(&carriers, found_link->chain.text);
        const char *const chain_text = (flow != NULL) ? flow->text : "";
        const size_t chain_size = (flow != NULL) ? flow->length : 0u;
        const CarrierPiece found = found_group(found_link, 0u, chain_text);
        const long link_at = found_link->link_at;
        char link_address[CARRIER_TEXT_MAX];
        link_address_written(found_link->chain.text, link_at, link_address, sizeof(link_address));
        link_given(&group, &forms.from_names, found_link, chain_text, &given);
        // A carrier is one that reads what the form writes: its first parameter, the register it writes, named again
        // after it. It reads a pair's high word where the chain names the pair. A link nothing reads answers alike
        // whatever is in for it
        const unsigned int first_group = group.group[(unsigned int)'A'];
        const CarrierPiece written = ((first_group != 0u) && (first_group < FOUND_GROUPS))
                                         ? found_group(found_link, first_group, chain_text)
                                         : CarrierPiece{chain_text, 0u};
        const CarrierPiece written_register = register_of(written);
        const CarrierPiece after = {found.text + found.length,
                                    chain_size - (size_t)(found.text + found.length - chain_text)};
        if ((flow == NULL) || (written_register.length == 0u) || !register_named_after(after, written_register))
        {
            chose("unread", link_address);
            continue;
        }
        // a link of the carrier's own, the thread's index, the case's index, the bound or an address, moves the case
        // every thread reads. A form goes in only where its link carries the cases (carrier_flow.h)
        if (!carrier_form_carries(&flow->links, flow->cases_defined, found, link_at))
        {
            chose("carrier_own", link_address);
            continue;
        }
        IdentityName arguments[FORM_NAMES_MAX];
        const char *argument_texts[FORM_NAMES_MAX];
        if (!stand_in_arguments(&forms, &given, arguments))
        {
            chose("parameter_unbound", link_address);
            result = 0;
            ended = 1;
            continue;
        }
        for (unsigned int at = 0u; at < forms.to_names.count; at += 1u)
        {
            argument_texts[at] = arguments[at].text;
        }
        // a stand-in the ruleset cannot write is a malformed question: the put exits, settled. Its scratch is
        // registers past the chain's own
        PlatformText located_text;
        identity_text_begun(&located_text);
        unsigned int scratch_taken = 0u;
        ScratchPast past = {sass, flow->registers, &scratch_taken};
        const RulesetNamedScratch scratch = scratch_past(&past);
        if (!ruleset_form_put(sass, to, argument_texts, forms.to_names.count, &scratch, &located_text))
        {
            chose("unwritable", link_address);
            platform_text_ended(&located_text);
            result = 0;
            ended = 1;
            continue;
        }
        const unsigned long long located_size = platform_text_size(&located_text);
        const char *const located_bytes = platform_text_bytes(&located_text);
        const char *const located = (located_bytes != NULL) ? located_bytes : "";
        // a link the stand-in writes as it was is no query
        if ((located_size == found.length) &&
            (EMBED_CALL(memor.cmp, MemoriaCfg, .src = located, .other = found.text, .bytes = found.length) == 0))
        {
            chose("unchanged", link_address);
            platform_text_ended(&located_text);
            continue;
        }
        t_dangerous = form_dangerous(located);
        PlatformText changed;
        identity_text_begun(&changed);
        // the link's first byte lies within the chain
        (void)mmgr_line_put_n(&changed.line, chain_text, (size_t)(found.text - chain_text));
        (void)mmgr_line_put_n(&changed.line, located, (size_t)located_size);
        (void)mmgr_line_put_n(&changed.line, after.text, after.length);
        IdentityCode code;
        identity_code_begun(&code, PLATFORM_TIER_TEMPORARY);
        vendor_assembled(machine, platform_text_bytes(&changed), &code);
        platform_text_ended(&changed);
        if ((code.count / 16ULL) == 0ULL)
        {
            chose("unassembled", link_address);
            platform_array_ended(&code);
            platform_text_ended(&located_text);
            continue;
        }
        // the link's place, its lines before it, is the line it was at
        const unsigned int link = (unsigned int)link_at;
        const char *const address = link_address;
        const HostCases *const expected = host_cases_of(&host, found_link->chain.text);
        // a stand-in a lesson of the overseer blocks answers as blocked at this link, and the part never sees it
        char overseen[160];
        const char *const chain_name = found_link->chain.text;
        if (overseer_blocks(located, chain_name, overseen, sizeof(overseen)))
        {
            chose("overseen", link_address);
            if (s_trace_log != NULL)
            {
                put_traced(to, from, address, overseen);
            }
            platform_array_ended(&code);
            platform_text_ended(&located_text);
            continue;
        }
        // Past the first case apart a link adds to the product alone, only where the form reads in a shape no
        // link read before it. A link of a shape read before contains the same rows, and is no branch of its own. The
        // cycle loops while the product is not whole and a branch stays. It ends at a whole product or with every
        // branch put or steered off. Its verdict is, and it requires nothing
        const int past_closed = closed[0] != '\0';
        // The steer: the links of one delta are one band, the nearest first. A band is truthy where a put in it adds a
        // row that came back one way, signal. It is falsy where its puts add none or only rows that came back both
        // ways, noise. A truthy band steers on to the next. Past the first case apart, a falsy band steers off every
        // farther one. Their links are farther from the cases and read them through more links
        if (found_link->delta != band)
        {
            if (past_closed && (band != LONG_MIN) && !band_truthy)
            {
                if (s_query_log != NULL)
                {
                    chose("steered_off", link_address);
                }
                platform_array_ended(&code);
                platform_text_ended(&located_text);
                break;
            }
            band = found_link->delta;
            band_truthy = 0;
        }
        PlatformText shape;
        identity_text_begun(&shape);
        read_shape(&flow->links, CarrierPiece{chain_text, chain_size}, flow->cases_defined, found, link_at, &shape);
        const int shape_new = shape_added(&shapes, &shape);
        platform_text_ended(&shape);
        if (past_closed && !shape_new)
        {
            chose("shape_read", link_address);
            platform_array_ended(&code);
            platform_text_ended(&located_text);
            continue;
        }
        // the stand-in is the question's probe place: the cross reads it as the probe it is (measuring_stick_query.c)
        question->place = link;
        const int alike = past_closed ? 0 : vector_alike(&code, expected, places, question, &queries, flow->registers);
        const unsigned int put_outcome = past_closed ? (unsigned int)RUN_GATED : question->outcome;
        platform_array_ended(&code);
        if (!past_closed)
        {
            chose(alike ? "alike" : (put_outcome == RUN_ANSWERED) ? "apart" : "refused", link_address);
        }
        if ((s_trace_log != NULL) && !past_closed)
        {
            put_traced(to, from, address,
                       alike                           ? "alike"
                       : (put_outcome == RUN_ANSWERED) ? "apart"
                                                       : question->refused);
        }
        // a put the part refused, or a carrier ended on, calls a conference. Its guidance is what the learner puts
        // next, and a lesson it teaches blocks the stand-in's own product below from the part
        unsigned int guidance = OVERSEER_HANDS_OFF;
        if (!past_closed && !alike && (put_outcome != RUN_ANSWERED) && (put_outcome != RUN_GATED))
        {
            char statement[CARRIER_TEXT_MAX * 4u];
            VerbaLinea statement_line = mmgr_line_over(statement, sizeof(statement));
            mmgr_line_put(&statement_line, "the pair ");
            mmgr_line_put(&statement_line, from);
            mmgr_line_put(&statement_line, " ");
            mmgr_line_put(&statement_line, to);
            mmgr_line_put(&statement_line, ", ");
            mmgr_line_put(&statement_line, to);
            mmgr_line_put(&statement_line, " put in place of ");
            mmgr_line_put(&statement_line, from);
            mmgr_line_put(&statement_line, " at ");
            mmgr_line_put(&statement_line, address);
            (void)mmgr_line_finish(&statement_line);
            guidance =
                conferred(from, to, statement, located, chain_name, scratch_taken, question, closed, link_address);
        }
        const int are_in_blocked = overseer_blocks(located, chain_name, overseen, sizeof(overseen));
        // the put beneath its query in the log. The link as it was, the link located in its place, each on one line
        // of the log. Then, as names, the operands only the form it was gives
        if (s_query_log != NULL)
        {
            const PutLink put_link = {from,
                                      to,
                                      &forms.from_names,
                                      from_written,
                                      &forms.read_by_to,
                                      forms.unified,
                                      &forms.fixed,
                                      &forms.qualifiers,
                                      &forms.pair_cased,
                                      question,
                                      places,
                                      &product,
                                      found_link->chain.text,
                                      found_link,
                                      &given,
                                      flow,
                                      link_at,
                                      link_address,
                                      located,
                                      address,
                                      found_link->delta,
                                      are_in_blocked,
                                      shape_new};
            band_truthy |= put_link.logged(this);
        }
        platform_text_ended(&located_text);
        // the conference's guidance: the stand-in brought it on, and the pair goes the other way round, or its
        // relation waits. Otherwise the put goes on to its next link as it would
        if (guidance == OVERSEER_OTHER_WAY)
        {
            t_turned = 1;
            break;
        }
        if (guidance == OVERSEER_WAITS)
        {
            break;
        }
        if (past_closed && concept_whole(&product, &forms.pair_cased))
        {
            (void)IDENTITY_SET_TEXT(*verdict, closed);
            result = 1;
            ended = 1;
            continue;
        }
        if (alike)
        {
            for (unsigned int at = 0u; at < question->cases; at += 1u)
            {
                alike_cases += identity_same(host_case(expected, places[at]), "-") ? 0u : 1u;
            }
            answered = 1;
            if (forms.const_loaded)
            {
                const char *const stop[2] = {"const_stop", "alike"};
                chosen(stop, 2u, link_address);
                break;
            }
            continue;
        }
        if (put_outcome == RUN_ANSWERED)
        {
            // the verdict is the first case apart. Where the run keeps the log, the put goes on until the pair's
            // product is whole
            if (closed[0] == '\0')
            {
                VerbaLinea closed_line = mmgr_line_over(closed, sizeof(closed));
                mmgr_line_put(&closed_line, "closed ");
                mmgr_line_put(&closed_line, address);
                mmgr_line_put(&closed_line, " ");
                case_text(s_apart_at, &closed_line);
                mmgr_line_put(&closed_line, "->");
                mmgr_line_put(&closed_line, host_case(expected, s_apart_at));
                (void)mmgr_line_finish(&closed_line);
            }
            if ((s_query_log == NULL) || concept_whole(&product, &forms.pair_cased))
            {
                (void)IDENTITY_SET_TEXT(*verdict, closed);
                result = 1;
                ended = 1;
            }
        }
    }
    platform_text_ended(&forms.to_text);
    platform_text_ended(&forms.from_text);
    concept_product_ended(&product);
    platform_text_ended(&shapes);
    platform_array_ended(&found_links);
    if (result >= 0)
    {
        return result;
    }
    if (closed[0] != '\0')
    {
        (void)IDENTITY_SET_TEXT(*verdict, closed);
        return 1;
    }
    VerbaLinea open_line = mmgr_line_over(verdict->text, sizeof(verdict->text));
    mmgr_line_put(&open_line, "open ");
    mmgr_line_decimal(&open_line, alike_cases, 1u);
    (void)mmgr_line_finish(&open_line);
    return answered;
}
