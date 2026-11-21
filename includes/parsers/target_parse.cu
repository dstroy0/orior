// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// target_parse.cu: where rulesets are found; their lines are read by ruleset_core.h
#include "../../src/cu/engine/rmc/target_internal.h"
#include "tree_folder.h"

// The rulesets of the lanes, one a language. A process reads each once from its .krs file in
// src/cu/transpiler/lstar/protocol/table, or in the folder that $CYCLE_RULESETS names. The format is the comment that
// the files of the rulesets open with.
//
// The code generator of each language names each bank of registers that it takes from. It names each register that it
// passes to a form by name, and each form that it writes, with the parameters of each. This is in the schema that its
// class gives the base.
//
// A ruleset that lacks one of them, holds one that they do not name, or gives a form other parameters fails whole. The
// record machine then sends its programs on to another language or the interpreter.
//
// The code stores a form cut at its parameters. A write of one appends its pieces with each argument between them

// the folder of the rulesets. It is $CYCLE_RULESETS where set. Otherwise it is src/cu/transpiler/lstar/protocol/table
// two folders above the folder of this file, in the tree of its build
std::string ruleset_folder(void)
{
    const char *const named = getenv("CYCLE_RULESETS");
    if ((named != NULL) && (named[0] != '\0'))
    {
        return std::string(named);
    }
    return tree_folder_from(__FILE__, 2u, "src/cu/transpiler/lstar/protocol/table");
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
