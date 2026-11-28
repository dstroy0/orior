// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// chain_check.c: how much of a relation the ladder's own cases decide
//
// A ladder climbs by putting a few small words and reading what comes back. This builds each relation's chains
// twice: once against those cases alone, and once against the relation on words the cases do not carry. The gap is
// the count of arrangements the cases let through that are not the relation at all - right about the words they
// were fitted to and wrong about the rest.
//
// That gap is the reading this exists for. It is how many answers a target can give before an answer means
// anything, and a case added to the ladder is worth adding exactly insofar as it closes it.
#include "../../../../../../../src/cu/transpiler/lstar/protocol/gate/chain_build.h"
#include "../../../../../../../includes/platform/platform.h"

#include "memoria_operor/memoria_operor.h"
#include <stdio.h>

#define LADDER_TEXT(name_, text_, words_, measured_) text_,
static const char *const s_anchor_text[] = {LADDER_ANCHORS(LADDER_TEXT)};
#undef LADDER_TEXT

static unsigned int anchor_cases(unsigned int anchor, LadderQuestion *cases_out)
{
    unsigned int found = 0u;
    for (unsigned int at = 0u; at < LADDER_CASE_COUNT; at += 1u)
    {
        if (s_ladder_cases[at].anchor == anchor)
        {
            cases_out[found] = s_ladder_cases[at];
            found += 1u;
        }
    }
    return found;
}

int program_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    static ChainSet fitted;
    static ChainSet swept;
    static LadderQuestion cases[LADDER_CASE_COUNT];
    char text[512];
    unsigned int let_through_whole = 0u;
    unsigned int chains_whole = 0u;
    for (unsigned int anchor = 0u; anchor < (unsigned int)LADDER_ANCHOR_COUNT; anchor += 1u)
    {
        const unsigned int found = anchor_cases(anchor, cases);
        if ((found == 0u) || (s_ladder_measured[anchor] != 0))
        {
            continue;
        }
        const unsigned int built = chain_build_cases(&swept, cases, found, 1);
        chain_build_cases(&fitted, cases, found, 0);
        const unsigned int let_through = fitted.chains - built;
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put_columns(output_line, s_anchor_text[anchor], 8u);
            mmgr_line_put(output_line, " ");
            mmgr_line_decimal_columns(output_line, found, 2u);
            mmgr_line_put(output_line, " cases  ");
            mmgr_line_decimal_columns(output_line, built, 5u);
            mmgr_line_put(output_line, " are the relation  ");
            mmgr_line_decimal_columns(output_line, let_through, 5u);
            mmgr_line_put(output_line, " more fit the cases and are not it");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        // the shortest arrangement the cases alone would have taken for the relation, the one a ladder
        // that stopped at its own cases would have written down
        for (unsigned int at = 0u; (at < fitted.chains) && (let_through != 0u); at += 1u)
        {
            unsigned int is_relation = 0u;
            for (unsigned int which = 0u; which < built; which += 1u)
            {
                is_relation += (EMBED_CALL(memor.cmp, MemoriaCfg, .src = &fitted.chain[at],
                                           .other = &swept.chain[which], .bytes = sizeof(Chain)) == 0)
                                   ? 1u
                                   : 0u;
            }
            if (is_relation != 0u)
            {
                continue;
            }
            chain_text(&fitted.chain[at], text, sizeof(text));
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "      shortest one that is not: ");
                mmgr_line_put(output_line, text);
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            break;
        }
        let_through_whole += let_through;
        chains_whole += built;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  chain check: ");
        mmgr_line_decimal(output_line, chains_whole, 1u);
        mmgr_line_put(output_line, " chains are the relation, ");
        mmgr_line_decimal(output_line, let_through_whole, 1u);
        mmgr_line_put(output_line, " more fit the ladder's cases and are not");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 0;
}
