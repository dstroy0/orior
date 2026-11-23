// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// One form of one ruleset written, with no device and no suite: the check that a ruleset a hand just edited still
// reads against its code generator's schema, and that a form given as a construct writes the lines it declares it does
//
//     ruleset_read_one <form> <argument> ...
//     ruleset_read_one predicate_bitxor P0 P1 P2
#include "../../../../../platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "sass_target.h"
#include "target.h"

// the bytes of the form's text at max
#define READ_ONE_TEXT_BYTES (1ULL << 24u)

// The scratch a writing takes, each bank's registers numbered from 0 as the lane's would be: the temporaries R100 up,
// the 64-bit temporaries R120 up and the predicates P0 up, numbers no argument here is likely to carry. 0 for a bank
// it does not know
static int read_one_scratch(void *context, const char *bank, VerbaLinea *name)
{
    (void)context;
    static unsigned int temporaries;
    static unsigned int wides;
    static unsigned int predicates;
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = bank, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "temporary",
                   .ci = EMBED_FALSE))
    {
        temporaries += 1u;
        mmgr_line_ch(name, 'R');
        mmgr_line_decimal(name, 99u + temporaries, 1u);
        return 1;
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = bank, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "wide",
                   .ci = EMBED_FALSE))
    {
        wides += 1u;
        mmgr_line_ch(name, 'R');
        mmgr_line_decimal(name, 119u + wides, 1u);
        return 1;
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = bank, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "predicate",
                   .ci = EMBED_FALSE))
    {
        predicates += 1u;
        mmgr_line_ch(name, 'P');
        mmgr_line_decimal(name, predicates - 1u, 1u);
        return 1;
    }
    return 0;
}

int program_main(int count, char **arguments)
{
    if (count < 2)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "ruleset_read_one <form> <argument> ...");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 2;
    }
    const Ruleset *const rules = sass_target().ruleset(1);
    if (rules == NULL)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "sass.krs did not read");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }
    // the arguments after the form, which the process holds to its end
    const char *const *const given = (const char *const *)&arguments[2];
    // the arguments are far fewer than 2^32
    const unsigned int given_count = (unsigned int)(count - 2);
    const RulesetNamedScratch scratch = {read_one_scratch, NULL};
    PlatformText text;
    platform_text_begun(&text, READ_ONE_TEXT_BYTES, platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT);
    if (ruleset_form_written(rules, arguments[1], given, given_count, &scratch, &text.line) == 0)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "sass.krs does not write ");
            mmgr_line_put(output_line, arguments[1]);
            mmgr_line_put(output_line, " with ");
            mmgr_line_decimal(output_line, given_count, 1u);
            mmgr_line_put(output_line, " arguments");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        platform_text_ended(&text);
        return 1;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, platform_text_bytes(&text));
        platform_line_out_ended();
    }
    platform_text_ended(&text);
    return 0;
}
