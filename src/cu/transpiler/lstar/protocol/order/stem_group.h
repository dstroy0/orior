// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef STEM_GROUP_H
#define STEM_GROUP_H

// Which members share a stem: groups built on an anchor, by a rule that reads only the members.
//
// The program takes agreement between two members at the coarser of their two floors. There it is symmetric, but
// still not transitive. A first member can agree with a second, and the second with a third, while the first and the
// third do not. Pairwise agreement names no set, and a group made by it gives an answer that turns on which member the
// program queried first. A group here has one member as its anchor, and every member of it agrees with that anchor.
//
// The rule. The program puts the members in one order, fixed by what they are: the finest floor first. Between two
// floors that are alike, the order goes by their rows, then by their mark of which rows had a measurement. The first
// member with no group anchors a new one, and each member with no group that agrees with it joins it. The program
// repeats that until every member has a group.
//
// The finest member is the anchor, because its answers are the sharpest. A group read from its anchor reads at the
// floor that the anchor can tell apart.
//
// Agreement is a conjunction over rows. Every row that both members measured differs by no more than the coarser
// floor. A row that one member measured and the other did not separates them. A refused or censored row is an answer.
// Two members that cost the same and differ in what they refused are two members.
//
// What follows from the rule, and what stem_group_check.c checks:
//
// - Every member agrees with its anchor.
// - No anchor agrees with an anchor placed before it.
// - The groups are the same, whatever order the members come in.
//
// A group is a function of the whole set. A member added to the set can change which member is the anchor, and thus
// which members share a stem. The program writes every block for a group again when the set changes.
//
// Every value is an exact integer.

// the max rows one member carries
#define STEM_GROUP_ROWS 64u
// the max members one set contains
#define STEM_GROUP_MEMBERS 256u
// the group that a member has before the rule places it
#define STEM_GROUP_NONE 0xffffffffu

// one member: a cost for each row, which rows had a measurement, and the floor that the member carries
typedef struct
{
    unsigned long long cost[STEM_GROUP_ROWS];
    // 1 where the row had a measurement, 0 where the part refused it, a censor removed it, or nothing queried it
    unsigned char measured[STEM_GROUP_ROWS];
    unsigned long long floor;
} StemMember;

// 1 where `one` and `two` agree over `rows` rows at the coarser of their two floors
int stem_group_agree(const StemMember *one, const StemMember *two, unsigned int rows);

// The `count` members grouped by the rule: `group[member]` the index of the member that anchors its group. The count
// of groups, or 0 where `count` or `rows` is past what a set contains
unsigned int stem_group_build(const StemMember *member, unsigned int count, unsigned int rows, unsigned int *group);

#endif
