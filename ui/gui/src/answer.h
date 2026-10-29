// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// answer.h: what the window reads off the CLI's answers and events, each a JSON tree: a whole number at a node, a
// member's whole number, and whether a member is true (answer.c)
#ifndef GUI_ANSWER_H
#define GUI_ANSWER_H
#include "../../cli/src/text/json_tree.h"

// the whole number a number node holds, `otherwise` where the node is none or no whole number; the same for the
// member `name` of `object`; and whether the member `name` of `object` is true
unsigned long long gui_answer_whole(const CliJson *json, unsigned int node, unsigned long long otherwise);
unsigned long long gui_answer_member_whole(const CliJson *json, unsigned int object, const char *name,
                                           unsigned long long otherwise);
int gui_answer_member_true(const CliJson *json, unsigned int object, const char *name);
#endif
