// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// devcontainer_internal.h: what reading a description shares across devcontainer_plan.c, which reads its JSON, and
// devcontainer_read.c, which reads it into a plan
#ifndef DEVCONTAINER_INTERNAL_H
#define DEVCONTAINER_INTERNAL_H
typedef struct CliDevcontainerPlanCtx CliDevcontainerPlanCtx;
typedef struct CliDevcontainerObjectCtx CliDevcontainerObjectCtx;
typedef struct CliDevcontainerOutCtx CliDevcontainerOutCtx;
typedef struct CliDevcontainerOutBaseCtx CliDevcontainerOutBaseCtx;
typedef struct CliDevcontainerPlanTextCtx CliDevcontainerPlanTextCtx;
typedef struct CliDevcontainerValueCtx CliDevcontainerValueCtx;
typedef struct CliDevcontainerCtx CliDevcontainerCtx;
typedef struct CliDevcontainerNameCtx CliDevcontainerNameCtx;
typedef struct CliDevcontainerValueOutCtx CliDevcontainerValueOutCtx;

#include "devcontainer.h"
#include "embed_dispatch_layout.h"

// the longest text of one of a description's strings
#define CLI_STRING_MAX_LEN 8192u

// What a description's variables read as: the tree, the tree's folder in the container
typedef struct
{
    const char *root;
    const char *folder;
} CliPlanPositions;

// the description at `file` read and made plain JSON, its tokens read, and nothing yet found too long: 1, or 0 with
// why given, the plain text and its tokens in cells sized to it; those cells given back once the plan is made; 1 where
// a text of it did not fit what a plan stores; and the cells the description occupies
int cli_plan_description_read(const CliDevcontainerCtx *args);
void cli_plan_description_free(void);
int cli_plan_overflow(void);
extern CellOwner g_cli_devcontainer_cells;
// the string at token `value` decoded into `out`, sized for CLI_STRING_MAX_LEN: 1, or 0 where it is no string; the
// member `name` of the description, and of the object at token `object` where it is a member found, 0 where none
int cli_plan_string(const CliDevcontainerValueOutCtx *args);
unsigned int cli_plan_member(const CliDevcontainerNameCtx *args);
unsigned int cli_plan_member_get(const CliDevcontainerNameCtx *args);
// the strings a value contains: itself where it is one, else each of an array's that is a string; their count, each
// token into `tokens`
unsigned int cli_plan_strings(const CliDevcontainerValueCtx *args);
// `text` put in the plan's texts, where it starts; and put at the end of `list`
unsigned int cli_plan_append(const CliDevcontainerPlanTextCtx *args);
void cli_plan_list(const CliDevcontainerPlanTextCtx *args);
// `text` with the description's variables read into `out`, sized for CLI_STRING_MAX_LEN; `path` joined onto `base`
// into `out`, sized for CLI_PATH_MAX_LEN
void cli_plan_expand(const CliDevcontainerOutCtx *args);
void cli_plan_join(const CliDevcontainerOutBaseCtx *args);
// each member of the object at token `object` whose value is a string, its key and its value read in turn, put at
// the end of `list`; each mount of the description's `mounts` read into the plan; the commands of the description's
// `key` put at the end of `commands`, `count` of them so far
void cli_plan_pairs(const CliDevcontainerObjectCtx *args);
void cli_plan_mounts(const CliDevcontainerObjectCtx *args);
void cli_plan_commands(const CliDevcontainerPlanCtx *args);

struct CliDevcontainerPlanCtx
{
    CliPlan *plan;
    const char *key;
    CliPlanCommand *commands;
    unsigned int *count;
};

struct CliDevcontainerObjectCtx
{
    unsigned int object;
    CliPlan *plan;
    CliPlanList *list;
    const CliPlanPositions *positions;
};

struct CliDevcontainerOutCtx
{
    char *out;
    const char *text;
    const CliPlanPositions *positions;
};

struct CliDevcontainerOutBaseCtx
{
    char *out;
    const char *base;
    const char *path;
};

struct CliDevcontainerPlanTextCtx
{
    CliPlan *plan;
    const char *text;
    CliPlanList *list;
};

struct CliDevcontainerValueCtx
{
    unsigned int value;
    unsigned int *tokens;
    unsigned int capacity;
};

struct CliDevcontainerCtx
{
    const char *file;
    VerbaLinea *err_msg;
};

struct CliDevcontainerNameCtx
{
    const char *name;
    unsigned int object;
};

struct CliDevcontainerValueOutCtx
{
    unsigned int value;
    char *out;
};

typedef struct
{
    int (*plan_description_read)(const CliDevcontainerCtx *args);
} CliDevcontainerNsTable;
EMBED_TABLE_LAYOUT(CliDevcontainerNsTable, plan_description_read);
EMBED_TABLE_STORAGE CliDevcontainerNsTable cli_devcontainer_internal EMBED_UNUSED = {
    .plan_description_read = cli_plan_description_read,
};

typedef struct
{
    int (*plan_string)(const CliDevcontainerValueOutCtx *args);
} CliDevcontainerValueOutNs;
EMBED_TABLE_LAYOUT(CliDevcontainerValueOutNs, plan_string);
EMBED_TABLE_STORAGE CliDevcontainerValueOutNs cli_devcontainer_value_out_internal EMBED_UNUSED = {
    .plan_string = cli_plan_string,
};

typedef struct
{
    unsigned int (*plan_member)(const CliDevcontainerNameCtx *args);
    unsigned int (*plan_member_get)(const CliDevcontainerNameCtx *args);
} CliDevcontainerNameNs;
EMBED_TABLE_LAYOUT(CliDevcontainerNameNs, plan_member, plan_member_get);
EMBED_TABLE_STORAGE CliDevcontainerNameNs cli_devcontainer_name_internal EMBED_UNUSED = {
    .plan_member = cli_plan_member,
    .plan_member_get = cli_plan_member_get,
};

typedef struct
{
    unsigned int (*plan_strings)(const CliDevcontainerValueCtx *args);
} CliDevcontainerValueNs;
EMBED_TABLE_LAYOUT(CliDevcontainerValueNs, plan_strings);
EMBED_TABLE_STORAGE CliDevcontainerValueNs cli_devcontainer_value_internal EMBED_UNUSED = {
    .plan_strings = cli_plan_strings,
};

typedef struct
{
    unsigned int (*plan_append)(const CliDevcontainerPlanTextCtx *args);
    void (*plan_list)(const CliDevcontainerPlanTextCtx *args);
} CliDevcontainerPlanTextNs;
EMBED_TABLE_LAYOUT(CliDevcontainerPlanTextNs, plan_append, plan_list);
EMBED_TABLE_STORAGE CliDevcontainerPlanTextNs cli_devcontainer_plan_text_internal EMBED_UNUSED = {
    .plan_append = cli_plan_append,
    .plan_list = cli_plan_list,
};

typedef struct
{
    void (*plan_expand)(const CliDevcontainerOutCtx *args);
} CliDevcontainerOutNs;
EMBED_TABLE_LAYOUT(CliDevcontainerOutNs, plan_expand);
EMBED_TABLE_STORAGE CliDevcontainerOutNs cli_devcontainer_out_internal EMBED_UNUSED = {
    .plan_expand = cli_plan_expand,
};

typedef struct
{
    void (*plan_join)(const CliDevcontainerOutBaseCtx *args);
} CliDevcontainerOutBaseNs;
EMBED_TABLE_LAYOUT(CliDevcontainerOutBaseNs, plan_join);
EMBED_TABLE_STORAGE CliDevcontainerOutBaseNs cli_devcontainer_out_base_internal EMBED_UNUSED = {
    .plan_join = cli_plan_join,
};

typedef struct
{
    void (*plan_pairs)(const CliDevcontainerObjectCtx *args);
    void (*plan_mounts)(const CliDevcontainerObjectCtx *args);
} CliDevcontainerObjectNs;
EMBED_TABLE_LAYOUT(CliDevcontainerObjectNs, plan_pairs, plan_mounts);
EMBED_TABLE_STORAGE CliDevcontainerObjectNs cli_devcontainer_object_internal EMBED_UNUSED = {
    .plan_pairs = cli_plan_pairs,
    .plan_mounts = cli_plan_mounts,
};

typedef struct
{
    void (*plan_commands)(const CliDevcontainerPlanCtx *args);
} CliDevcontainerPlanNsTable;
EMBED_TABLE_LAYOUT(CliDevcontainerPlanNsTable, plan_commands);
EMBED_TABLE_STORAGE CliDevcontainerPlanNsTable cli_devcontainer_plan_internal EMBED_UNUSED = {
    .plan_commands = cli_plan_commands,
};

#endif
