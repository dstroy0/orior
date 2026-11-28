// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef CHAIN_BUILD_H
#define CHAIN_BUILD_H

// Every arrangement of primitives that makes a relation.
//
// An operator is a chain, and a chain can take a new arrangement. A person who writes a compiler affords one
// arrangement for each operator, because a person must write it. Nobody writes these. The program keeps every
// arrangement that works, and a job takes the one that suits it. Cost separates the arrangements, and the program
// measures cost on the target. This file has the part of the work that finds them.
//
// The program keeps a chain when it answers every case of the relation. The cases are arithmetic, and the chain runs
// over them here, with no target. A chain that fails a case is wrong everywhere, and there is no reason to query a
// target about it. A chain that passes is a candidate that the target can or cannot write.

#include "../../../../engine/rmc/precept_value.h"
#include "../counterexample/ladder.h"

// the max nodes that a chain holds. One node means that the target has the operator outright. Two and three are the
// arrangements worth a weigh against one another
#define CHAIN_NODES 3u
// the max chains that the program keeps for one relation
#define CHAIN_MAX 2048u
// how many words past the cases the program puts an arrangement with before it keeps it
#define CHAIN_SWEEP 512u

typedef struct
{
    PreceptNode node[CHAIN_NODES];
    unsigned int nodes;
} Chain;

typedef struct
{
    Chain chain[CHAIN_MAX];
    unsigned int chains;
    // arrangements that answered every case and had nowhere to go
    unsigned int over;
    // arrangements tried
    unsigned int tried;
    // arrangements the cases alone let through: all a ladder that stopped at its own cases would contain
    unsigned int fitted;
} ChainSet;

// Every chain of at max CHAIN_NODES nodes that answers `cases` and the relation that they belong to, into `set`. The
// return is the count kept. The call finds shorter chains first, thus a set that fills holds the shortest
// arrangements that exist
unsigned int chain_build(ChainSet *set, const LadderQuestion *cases, unsigned int count);

// The same, with `swept` to tell whether the call puts an arrangement with words past the cases before it keeps it. A
// set built with no sweep holds every arrangement that fits the cases, and nothing shows which of those is the
// relation. That is a reading of how much the own cases of the ladder decide, and it is not a set to derive from
unsigned int chain_build_cases(ChainSet *set, const LadderQuestion *cases, unsigned int count, int swept);

// `set` put into a new order, from `seed`.
//
// The walk finds arrangements in the order that it walks them, and that order is a bias of the walk. Where the program
// times them in that order, the reading of a chain carries where it sat. The first of a run pays for a cold part, and
// the rest do not, thus an arrangement that always runs first always reads slow. A shuffle washes that out over runs,
// and leaves the difference between the arrangements. A cost from one run is a reading. A cost from many runs, each in
// a different order, is a preference
void chain_shuffle(ChainSet *set, unsigned int seed);

// `chain` written into `text`, which has room for `room`, as the operator applied to its operands. The count written,
// and 0 with the text empty where it does not fit
unsigned int chain_text(const Chain *chain, char *text, unsigned int room);

#endif
