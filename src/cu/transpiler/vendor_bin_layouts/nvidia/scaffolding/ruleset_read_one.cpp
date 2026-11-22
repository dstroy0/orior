// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// One form of one ruleset written, with no device and no suite: the check that a ruleset a hand just edited still
// reads against its code generator's schema, and that a form given as a construct writes the lines it declares it does
//
//     ruleset_read_one <form> <argument> ...
//     ruleset_read_one predicate_bitxor P0 P1 P2
#include "../../../../../platform/platform.h"
#include "../../../../scaffolding/ruleset_strings.h"
#include "sass_target.h"
#include "target.h"

#include <cstdio>
#include <string>
#include <vector>

// the scratch a writing takes, each bank's registers numbered from 0 as the lane's would be: the temporaries R100 up,
// the 64-bit temporaries R120 up and the predicates P0 up, numbers no argument here is likely to carry
static std::string read_one_scratch(const std::string &bank)
{
    static unsigned int temporaries;
    static unsigned int wides;
    static unsigned int predicates;
    if (bank == "temporary")
    {
        temporaries += 1u;
        return "R" + std::to_string(99u + temporaries);
    }
    if (bank == "wide")
    {
        wides += 1u;
        return "R" + std::to_string(119u + wides);
    }
    if (bank == "predicate")
    {
        predicates += 1u;
        return "P" + std::to_string(predicates - 1u);
    }
    return std::string();
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
    std::vector<std::string> given;
    for (int at = 2; at < count; at += 1)
    {
        given.push_back(std::string(arguments[at]));
    }
    std::string text;
    if (ruleset_opcode(rules, arguments[1], given, read_one_scratch, text) == 0)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "sass.krs does not write ");
            mmgr_line_put(output_line, arguments[1]);
            mmgr_line_put(output_line, " with ");
            mmgr_line_decimal(output_line, given.size(), 1u);
            mmgr_line_put(output_line, " arguments");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, text.c_str());
        platform_line_out_ended();
    }
    return 0;
}
