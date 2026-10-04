// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// target_parse.cu: where rulesets are found; their lines are read by ruleset_core.h
#include "target_internal.h"

// The rulesets a lane is written in, one a language, each read once a process from its .krs file in
// src/cu/transpiler/codegen/rulesets, or in the folder $CYCLE_RULESETS names. The format is the comment at the head of
// ptx.krs. Each language's code generator names every bank of registers it takes from, every register it passes to a
// form by name, and every form it writes with the parameters each takes, in the schema its class gives the base; a
// ruleset that lacks one of them, holds one they do not name, or gives a form other parameters errors on whole, and
// the record machine sends its programs on to another language or the interpreter. A form is kept cut at its
// parameters: writing one appends its pieces with each argument between them

// the folder rulesets are read from: $CYCLE_RULESETS, else rulesets in this file's folder in the tree it was built from
std::string ruleset_folder(void)
{
    const char *const named = getenv("CYCLE_RULESETS");
    if ((named != NULL) && (named[0] != '\0'))
    {
        return std::string(named);
    }
    const std::string file = __FILE__;
    const size_t slash = file.find_last_of("/\\");
    return (slash == std::string::npos) ? std::string("rulesets") : (file.substr(0u, slash + 1u) + "rulesets");
}

// the place of `word` among `count` names, or `count` where it is none of them
unsigned int ruleset_find(const RulesetName *names, unsigned int count, const std::string &word)
{
    unsigned int found = count;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        found = (word == names[at].text) ? at : found;
    }
    return found;
}
