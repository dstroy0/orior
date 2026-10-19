// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// devcontainer_internal.h: what reading a description shares across devcontainer_plan.c, which reads its JSON, and
// devcontainer_read.c, which reads it into a plan
#ifndef DEVCONTAINER_INTERNAL_H
#define DEVCONTAINER_INTERNAL_H

#include "devcontainer.h"

// the longest text of one of a description's strings
#define CLI_STRING_LONGEST 8192u

// What a description's variables read as: the tree, the tree's folder in the container
typedef struct
{
    const char *root;
    const char *folder;
} CliPlanPlaces;

// the description at `file` read and made plain JSON, its tokens read, and nothing yet found too long: 1, or 0 with
// why said; 1 where a text of it did not fit what a plan keeps
int cli_plan_description_read(const char *file, CliLine *said);
int cli_plan_overflowed(void);
// the string at token `value` decoded into `out`, which holds CLI_STRING_LONGEST: 1, or 0 where it is no string; the
// member `name` of the description, and of the object at token `object` where it is a member found, 0 where none
int cli_plan_string(unsigned int value, char *out);
unsigned int cli_plan_member(const char *name);
unsigned int cli_plan_member_of(unsigned int object, const char *name);
// the strings a value holds: itself where it is one, else each of an array's that is a string; their count, each
// token into `tokens`
unsigned int cli_plan_strings(unsigned int value, unsigned int *tokens, unsigned int room);
// `text` put in the plan's texts, where it starts; and put at the end of `list`
unsigned int cli_plan_put(CliPlan *plan, const char *text);
void cli_plan_listed(CliPlan *plan, CliPlanList *list, const char *text);
// `text` with the description's variables read into `out`, which holds CLI_STRING_LONGEST; `path` joined onto `base`
// into `out`, which holds CLI_PATH_LONGEST
void cli_plan_expanded(const char *text, const CliPlanPlaces *places, char *out);
void cli_plan_joined(const char *base, const char *path, char *out);
// each member of the object at token `object` whose value is a string, its key and its value read in turn, put at
// the end of `list`; each mount of the description's `mounts` read into the plan; the commands of the description's
// `key` put at the end of `commands`, `count` of them so far
void cli_plan_pairs(CliPlan *plan, unsigned int object, const CliPlanPlaces *places, CliPlanList *list);
void cli_plan_mounts(CliPlan *plan, const CliPlanPlaces *places);
void cli_plan_commands(CliPlan *plan, const char *key, CliPlanCommand *commands, unsigned int *count);

#endif
