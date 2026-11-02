// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// answer.h: what the window reads off the CLI's answers and events, each a JSON tree: a whole number at a node, a
// member's whole number, and whether a member is true (answer.c)
#ifndef GUI_ANSWER_H
#define GUI_ANSWER_H
typedef struct GuiAnswerObjectCfg GuiAnswerObjectCfg;
typedef struct GuiAnswerCfg GuiAnswerCfg;
#include "../../cli/src/text/json_tree.h"
#include "embed_dispatch_layout.h"

// the whole number a number node holds, `otherwise` where the node is none or no whole number; the same for the
// member `name` of `object`; and whether the member `name` of `object` is true
unsigned long long gui_answer_whole(const GuiAnswerCfg *args);
unsigned long long gui_answer_member_whole(const GuiAnswerObjectCfg *args);
int gui_answer_member_true(const GuiAnswerObjectCfg *args);
struct GuiAnswerObjectCfg
{
    const CliJson *json;
    unsigned long long otherwise;
    unsigned int object;
    const char *name;
};

struct GuiAnswerCfg
{
    const CliJson *json;
    unsigned int node;
    unsigned long long otherwise;
};

typedef struct
{
    unsigned long long (*whole)(const GuiAnswerCfg *args);
} GuiAnswerNs;
EMBED_TABLE_LAYOUT(GuiAnswerNs, whole);
EMBED_TABLE_STORAGE GuiAnswerNs gui_answer_table EMBED_UNUSED = {
    .whole = gui_answer_whole,
};

typedef struct
{
    unsigned long long (*member_whole)(const GuiAnswerObjectCfg *args);
    int (*member_true)(const GuiAnswerObjectCfg *args);
} GuiAnswerObjectNs;
EMBED_TABLE_LAYOUT(GuiAnswerObjectNs, member_whole, member_true);
EMBED_TABLE_STORAGE GuiAnswerObjectNs gui_answer_object EMBED_UNUSED = {
    .member_whole = gui_answer_member_whole,
    .member_true = gui_answer_member_true,
};

#endif
