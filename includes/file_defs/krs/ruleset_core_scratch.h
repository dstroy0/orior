// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ruleset_core_scratch.h: the scratch each form of a ruleset takes (ruleset_core.h includes the parts in order)
#ifndef RULESET_CORE_SCRATCH_H
#define RULESET_CORE_SCRATCH_H

#include "ruleset_core_words.h"

// where the code arranges the scratch of a form. It holds the form, and the line of its construct in the read. It holds
// 1 once the code took the arguments of that line and arranged its form. It holds where the registers of the form
// begin among the taken registers
struct RulesetCoreFrame
{
    unsigned int name;
    unsigned int line;
    unsigned int line_end;
    unsigned int arranged;
    unsigned int taken_first;
};

// the scratch of each form of a read ruleset:
//   - the read constructs, lines and arguments.
//   - the placeholders of each form, from form_placeholder_first[form] to form_placeholder_first[form + 1] of
//     form_placeholders.
//   - the banks at places 0, 1 and 2.
//   - the memory that the caller gives: the scratch itself, four words a form, and which forms the code counted. Then
//     a frame a form, and the taken registers, as many as there are arguments.
struct RulesetCoreScratch
{
    const RulesetCoreConstruct *constructs;
    const RulesetCoreLine *lines;
    const RulesetCoreArgument *arguments;
    const unsigned int *form_placeholder_first;
    const unsigned int *form_placeholders;
    unsigned int form_count;
    unsigned int banks[3];
    unsigned int *scratch;
    unsigned char *counted;
    RulesetCoreFrame *frames;
    unsigned int *taken_bank;
    unsigned int *taken_number;
};

// the frame of form `name`, begun where the code did not count it yet, as ruleset_scratch_of begins. The result is the
// allocated frames
CODEGEN_CORE unsigned int ruleset_core_scratch_enter(RulesetCoreScratch *scratch, unsigned int name, unsigned int depth,
                                                     unsigned int taken_top)
{
    if (scratch->counted[name] != 0u)
    {
        return depth;
    }
    scratch->counted[name] = 1u;
    const RulesetCoreConstruct *const construct = &scratch->constructs[name];
    const RulesetCoreFrame frame = {name, construct->line_first, construct->line_first + construct->line_count, 0u,
                                    taken_top};
    scratch->frames[depth] = frame;
    return depth + 1u;
}

// the scratch that form `name` takes in one write, as ruleset_scratch_of puts it out. The code arranges it into its
// four words of the scratch once, and marks it as counted.
//
// It is each scratch register that its lines name, once, and what each form that its lines write takes. A construct
// that the file gave before it arranged that, and the recursion ends.
//
// The frames bound the recursion of the host, a form each at max. The code arranges the form of each line before it
// adds the scratch of the line, as the host adds it
CODEGEN_CORE void ruleset_core_scratch_of(RulesetCoreScratch *scratch, unsigned int name)
{
    unsigned int taken_top = 0u;
    unsigned int depth = ruleset_core_scratch_enter(scratch, name, 0u, taken_top);
    while (depth != 0u)
    {
        RulesetCoreFrame *const frame = &scratch->frames[depth - 1u];
        if (frame->line == frame->line_end)
        {
            taken_top = frame->taken_first;
            depth -= 1u;
            continue;
        }
        const RulesetCoreLine *const line = &scratch->lines[frame->line];
        unsigned int *const own = &scratch->scratch[4u * frame->name];
        if (frame->arranged == 0u)
        {
            for (unsigned int given_at = 0u; given_at < line->argument_count; given_at += 1u)
            {
                const RulesetCoreArgument *const given = &scratch->arguments[line->argument_first + given_at];
                int found = 0;
                for (unsigned int at = frame->taken_first; (given->type == RULESET_CORE_SCRATCH) && (at < taken_top);
                     at += 1u)
                {
                    found = found || ((scratch->taken_bank[at] == given->placeholder) &&
                                      (scratch->taken_number[at] == given->number));
                }
                if ((given->type != RULESET_CORE_SCRATCH) || (found != 0))
                {
                    continue;
                }
                scratch->taken_bank[taken_top] = given->placeholder;
                scratch->taken_number[taken_top] = given->number;
                taken_top += 1u;
                const unsigned int bank_index = (given->placeholder == scratch->banks[0])
                                                    ? 0u
                                                    : ((given->placeholder == scratch->banks[1])
                                                           ? 1u
                                                           : ((given->placeholder == scratch->banks[2]) ? 2u : 3u));
                own[bank_index] = (bank_index == 3u) ? 1u : (own[bank_index] + 1u);
            }
            frame->arranged = 1u;
            depth = ruleset_core_scratch_enter(scratch, line->form, depth, taken_top);
            continue;
        }
        const unsigned int *const written = &scratch->scratch[4u * line->form];
        for (unsigned int bank_index = 0u; bank_index < 3u; bank_index += 1u)
        {
            own[bank_index] += written[bank_index];
        }
        own[3] |= written[3];
        frame->line += 1u;
        frame->arranged = 0u;
    }
}

// the scratch that each form of the ruleset takes in one write, four words a form by its place in the schema. This is
// as ruleset_scratch puts it out
CODEGEN_CORE void ruleset_core_scratch(RulesetCoreScratch *scratch)
{
    for (unsigned int word = 0u; word < (4u * scratch->form_count); word += 1u)
    {
        scratch->scratch[word] = 0u;
    }
    for (unsigned int name = 0u; name < scratch->form_count; name += 1u)
    {
        scratch->counted[name] = 0u;
    }
    // a form given as text takes each scratch register that its text names, once. This comes before the code arranges a
    // construct that writes it. A bank past the three breaks the lane, as a bank of a construct does
    for (unsigned int name = 0u; name < scratch->form_count; name += 1u)
    {
        if (scratch->constructs[name].line_count != 0u)
        {
            continue;
        }
        unsigned int *const own = &scratch->scratch[4u * name];
        for (unsigned int at = scratch->form_placeholder_first[name]; at < scratch->form_placeholder_first[name + 1u];
             at += 1u)
        {
            const unsigned int placeholder = scratch->form_placeholders[at];
            int seen = 0;
            for (unsigned int before = scratch->form_placeholder_first[name]; before < at; before += 1u)
            {
                seen = seen || (scratch->form_placeholders[before] == placeholder);
            }
            if (!RULESET_CORE_IS_SCRATCH(placeholder) || (seen != 0))
            {
                continue;
            }
            const unsigned int bank = RULESET_CORE_SCRATCH_BANK(placeholder);
            const unsigned int bank_index =
                (bank == scratch->banks[0])
                    ? 0u
                    : ((bank == scratch->banks[1]) ? 1u : ((bank == scratch->banks[2]) ? 2u : 3u));
            own[bank_index] = (bank_index == 3u) ? 1u : (own[bank_index] + 1u);
        }
    }
    for (unsigned int name = 0u; name < scratch->form_count; name += 1u)
    {
        ruleset_core_scratch_of(scratch, name);
    }
}

#endif
