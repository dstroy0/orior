// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// devcontainer_read.c: the tree's dev container read from its description into a plan
#include "../text/text.h"
#include "devcontainer_internal.h"

// the lock acquired while a description is read

typedef struct
{
    CliPlan *plan;
    const char *file;
    const char *base;
    const CliPlanPositions *positions;
    const unsigned int *compose;
    unsigned int composes;
    VerbaLinea *err_msg;
    const char *path;
    char *name;
    const char *root;
} DevcontainerReadCtx;
// what the container is built from, as the description specifies, into the plan: 1, or 0 with why given
EMBED_INLINE int cli_source_read(const DevcontainerReadCtx *args)
{
    CliPlan *plan = args->plan;
    const char *base = args->base;
    const CliPlanPositions *positions = args->positions;
    static char s_text[CLI_STRING_MAX_LEN];
    static char s_read[CLI_STRING_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    if (args->composes != 0u)
    {
        if (!EMBED_CALL(cli_devcontainer_value_out_internal.plan_string, CliDevcontainerValueOutCtx,
                        .value = EMBED_CALL(cli_devcontainer_name_internal.plan_member, CliDevcontainerNameCtx,
                                            .name = "service"),
                        .out = s_text))
        {
            mmgr_line_put(args->err_msg, args->file);
            mmgr_line_put(args->err_msg, " names dockerComposeFile and no service");
            return 0;
        }
        plan->source = CLI_SOURCE_COMPOSE;
        plan->service = EMBED_CALL(cli_devcontainer_plan_text_internal.plan_append, CliDevcontainerPlanTextCtx,
                                   .plan = plan, .text = s_text);
        for (unsigned int at = 0u; at < args->composes; at += 1u)
        {
            if (EMBED_CALL(cli_devcontainer_value_out_internal.plan_string, CliDevcontainerValueOutCtx,
                           .value = args->compose[at], .out = s_text))
            {
                EMBED_CALL(cli_devcontainer_out_internal.plan_expand, CliDevcontainerOutCtx, .text = s_text,
                           .positions = positions, .out = s_read);
                EMBED_CALL(cli_devcontainer_out_base_internal.plan_join, CliDevcontainerOutBaseCtx, .base = base,
                           .path = s_read, .out = path);
                EMBED_CALL(cli_devcontainer_plan_text_internal.plan_list, CliDevcontainerPlanTextCtx, .plan = plan,
                           .list = &plan->files, .text = path);
            }
        }
        unsigned int tokens[CLI_PLAN_LIST_MAX];
        const unsigned int services = EMBED_CALL(cli_devcontainer_value_internal.plan_strings, CliDevcontainerValueCtx,
                                                 .value = EMBED_CALL(cli_devcontainer_name_internal.plan_member,
                                                                     CliDevcontainerNameCtx, .name = "runServices"),
                                                 .tokens = tokens, .capacity = CLI_PLAN_LIST_MAX);
        for (unsigned int at = 0u; at < services; at += 1u)
        {
            if (EMBED_CALL(cli_devcontainer_value_out_internal.plan_string, CliDevcontainerValueOutCtx,
                           .value = tokens[at], .out = s_text))
            {
                EMBED_CALL(cli_devcontainer_plan_text_internal.plan_list, CliDevcontainerPlanTextCtx, .plan = plan,
                           .list = &plan->services, .text = s_text);
            }
        }
        return 1;
    }
    if (EMBED_CALL(cli_devcontainer_value_out_internal.plan_string, CliDevcontainerValueOutCtx,
                   .value =
                       EMBED_CALL(cli_devcontainer_name_internal.plan_member, CliDevcontainerNameCtx, .name = "image"),
                   .out = s_text))
    {
        plan->source = CLI_SOURCE_IMAGE;
        EMBED_CALL(cli_devcontainer_out_internal.plan_expand, CliDevcontainerOutCtx, .text = s_text,
                   .positions = positions, .out = s_read);
        plan->image = EMBED_CALL(cli_devcontainer_plan_text_internal.plan_append, CliDevcontainerPlanTextCtx,
                                 .plan = plan, .text = s_read);
        return 1;
    }
    const unsigned int build =
        EMBED_CALL(cli_devcontainer_name_internal.plan_member, CliDevcontainerNameCtx, .name = "build");
    if (!EMBED_CALL(cli_devcontainer_value_out_internal.plan_string, CliDevcontainerValueOutCtx,
                    .value = EMBED_CALL(cli_devcontainer_name_internal.plan_member_get, CliDevcontainerNameCtx,
                                        .object = build, .name = "dockerfile"),
                    .out = s_text) &&
        !EMBED_CALL(cli_devcontainer_value_out_internal.plan_string, CliDevcontainerValueOutCtx,
                    .value = EMBED_CALL(cli_devcontainer_name_internal.plan_member, CliDevcontainerNameCtx,
                                        .name = "dockerFile"),
                    .out = s_text))
    {
        mmgr_line_put(args->err_msg, args->file);
        mmgr_line_put(args->err_msg, " names no image, build.dockerfile or dockerComposeFile");
        return 0;
    }
    plan->source = CLI_SOURCE_BUILD;
    EMBED_CALL(cli_devcontainer_out_internal.plan_expand, CliDevcontainerOutCtx, .text = s_text, .positions = positions,
               .out = s_read);
    EMBED_CALL(cli_devcontainer_out_base_internal.plan_join, CliDevcontainerOutBaseCtx, .base = base, .path = s_read,
               .out = path);
    plan->dockerfile = EMBED_CALL(cli_devcontainer_plan_text_internal.plan_append, CliDevcontainerPlanTextCtx,
                                  .plan = plan, .text = path);
    if (!EMBED_CALL(cli_devcontainer_value_out_internal.plan_string, CliDevcontainerValueOutCtx,
                    .value = EMBED_CALL(cli_devcontainer_name_internal.plan_member_get, CliDevcontainerNameCtx,
                                        .object = build, .name = "context"),
                    .out = s_text))
    {
        memor.cpy(&(MemoriaCfg){.dst = s_text,
                                .src = ".",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = ".", .cap = CLI_CELLS_POOL}) + 1u});
    }
    EMBED_CALL(cli_devcontainer_out_internal.plan_expand, CliDevcontainerOutCtx, .text = s_text, .positions = positions,
               .out = s_read);
    EMBED_CALL(cli_devcontainer_out_base_internal.plan_join, CliDevcontainerOutBaseCtx, .base = base, .path = s_read,
               .out = path);
    plan->context = EMBED_CALL(cli_devcontainer_plan_text_internal.plan_append, CliDevcontainerPlanTextCtx,
                               .plan = plan, .text = path);
    EMBED_CALL(cli_devcontainer_object_internal.plan_pairs, CliDevcontainerObjectCtx, .plan = plan,
               .object = EMBED_CALL(cli_devcontainer_name_internal.plan_member_get, CliDevcontainerNameCtx,
                                    .object = build, .name = "args"),
               .positions = positions, .list = &plan->args);
    plan->targeted = EMBED_CALL(cli_devcontainer_value_out_internal.plan_string, CliDevcontainerValueOutCtx,
                                .value = EMBED_CALL(cli_devcontainer_name_internal.plan_member_get,
                                                    CliDevcontainerNameCtx, .object = build, .name = "target"),
                                .out = s_text);
    plan->target = plan->targeted ? EMBED_CALL(cli_devcontainer_plan_text_internal.plan_append,
                                               CliDevcontainerPlanTextCtx, .plan = plan, .text = s_text)
                                  : 0u;
    return 1;
}

// the last part of `path` into `name`, sized for CLI_PATH_MAX_LEN: the separators after it left off, and a drive's
// root none
EMBED_INLINE void cli_last_by_name(const DevcontainerReadCtx *args)
{
    const char *end = args->path + cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL});
    end = args->path + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = args->path,
                                  .at = (unsigned long long)(end - args->path), .set = "/\\");
    const char *last = end;
    last = args->path + EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path,
                                   .at = (unsigned long long)(last - args->path), .set = "/\\");
    last += (((end - last) == 2) && (last[1] == ':')) ? 2 : 0;
    VerbaLinea line = {args->name, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put_n(&line, last, (unsigned long long)(end - last));
    mmgr_line_finish(&line);
}

EMBED_INLINE int cli_plan_lock(const DevcontainerReadCtx *args)
{
    const char *root = args->root;
    CliPlan *plan = args->plan;
    VerbaLinea *err_msg = args->err_msg;
    static char s_text[CLI_STRING_MAX_LEN];
    static char s_read[CLI_STRING_MAX_LEN];
    char file[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_devcontainer_table.find, CliDevcontainerCfg, .root = root, .file = file))
    {
        mmgr_line_put(err_msg, root);
        mmgr_line_put(err_msg, " has no .devcontainer/devcontainer.json");
        return 0;
    }
    if (!EMBED_CALL(cli_devcontainer_internal.plan_description_read, CliDevcontainerCtx, .file = file,
                    .err_msg = err_msg))
    {
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = plan, .val = 0, .bytes = sizeof(*plan)});
    // the text at 0 is the empty one
    plan->used = 1u;
    plan->file = EMBED_CALL(cli_devcontainer_plan_text_internal.plan_append, CliDevcontainerPlanTextCtx, .plan = plan,
                            .text = file);
    plan->root = EMBED_CALL(cli_devcontainer_plan_text_internal.plan_append, CliDevcontainerPlanTextCtx, .plan = plan,
                            .text = root);
    char name[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_devcontainer_root_name.name, CliDevcontainerRootNameCfg, .root = root, .name = name);
    plan->name = EMBED_CALL(cli_devcontainer_plan_text_internal.plan_append, CliDevcontainerPlanTextCtx, .plan = plan,
                            .text = name);
    // the description's folder, or the tree where it stands in none
    char base[CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){
        .dst = base, .src = file, .bytes = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}) + 1u});
    char *const forward =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = base,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL}), .byte = '/');
    char *const backward =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = base,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL}), .byte = '\\');
    char *const cut = ((backward != NULL) && ((forward == NULL) || (backward > forward))) ? backward : forward;
    if (cut != NULL)
    {
        cut[0] = '\0';
    }
    else
    {
        memor.cpy(&(MemoriaCfg){.dst = base,
                                .src = root,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    }
    unsigned int compose[CLI_PLAN_LIST_MAX];
    const unsigned int composes = EMBED_CALL(cli_devcontainer_value_internal.plan_strings, CliDevcontainerValueCtx,
                                             .value = EMBED_CALL(cli_devcontainer_name_internal.plan_member,
                                                                 CliDevcontainerNameCtx, .name = "dockerComposeFile"),
                                             .tokens = compose, .capacity = CLI_PLAN_LIST_MAX);
    const CliPlanPositions outside = {root, ""};
    if (EMBED_CALL(cli_devcontainer_value_out_internal.plan_string, CliDevcontainerValueOutCtx,
                   .value = EMBED_CALL(cli_devcontainer_name_internal.plan_member, CliDevcontainerNameCtx,
                                       .name = "workspaceFolder"),
                   .out = s_text))
    {
        EMBED_CALL(cli_devcontainer_out_internal.plan_expand, CliDevcontainerOutCtx, .text = s_text,
                   .positions = &outside, .out = s_read);
        plan->folder = EMBED_CALL(cli_devcontainer_plan_text_internal.plan_append, CliDevcontainerPlanTextCtx,
                                  .plan = plan, .text = s_read);
    }
    else
    {
        // the tree's folder's name is shorter than a path, and the folder it is put under shorter than a string
        char tree_name[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_last_by_name, DevcontainerReadCtx, .path = root, .name = tree_name);
        VerbaLinea line = {s_read, sizeof(s_read), 0ULL};
        mmgr_line_put(&line, (composes != 0u) ? "/" : "/workspaces/");
        mmgr_line_put(&line, (composes != 0u) ? "" : tree_name);
        mmgr_line_finish(&line);
        plan->folder = EMBED_CALL(cli_devcontainer_plan_text_internal.plan_append, CliDevcontainerPlanTextCtx,
                                  .plan = plan, .text = s_read);
    }
    const CliPlanPositions positions = {root, plan->text + plan->folder};
    if (!EMBED_CALL(cli_source_read, DevcontainerReadCtx, .plan = plan, .file = file, .base = base,
                    .positions = &positions, .compose = compose, .composes = composes, .err_msg = err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_devcontainer_object_internal.plan_pairs, CliDevcontainerObjectCtx, .plan = plan,
               .object = EMBED_CALL(cli_devcontainer_name_internal.plan_member, CliDevcontainerNameCtx,
                                    .name = "containerEnv"),
               .positions = &positions, .list = &plan->env);
    EMBED_CALL(cli_devcontainer_object_internal.plan_mounts, CliDevcontainerObjectCtx, .plan = plan,
               .positions = &positions);
    unsigned int tokens[CLI_PLAN_LIST_MAX];
    const unsigned int run_args = EMBED_CALL(
        cli_devcontainer_value_internal.plan_strings, CliDevcontainerValueCtx,
        .value = EMBED_CALL(cli_devcontainer_name_internal.plan_member, CliDevcontainerNameCtx, .name = "runArgs"),
        .tokens = tokens, .capacity = CLI_PLAN_LIST_MAX);
    for (unsigned int each = 0u; each < run_args; each += 1u)
    {
        if (EMBED_CALL(cli_devcontainer_value_out_internal.plan_string, CliDevcontainerValueOutCtx,
                       .value = tokens[each], .out = s_text))
        {
            EMBED_CALL(cli_devcontainer_out_internal.plan_expand, CliDevcontainerOutCtx, .text = s_text,
                       .positions = &positions, .out = s_read);
            EMBED_CALL(cli_devcontainer_plan_text_internal.plan_list, CliDevcontainerPlanTextCtx, .plan = plan,
                       .list = &plan->run_args, .text = s_read);
        }
    }
    plan->has_user = EMBED_CALL(cli_devcontainer_value_out_internal.plan_string, CliDevcontainerValueOutCtx,
                                .value = EMBED_CALL(cli_devcontainer_name_internal.plan_member, CliDevcontainerNameCtx,
                                                    .name = "containerUser"),
                                .out = s_text) ||
                     EMBED_CALL(cli_devcontainer_value_out_internal.plan_string, CliDevcontainerValueOutCtx,
                                .value = EMBED_CALL(cli_devcontainer_name_internal.plan_member, CliDevcontainerNameCtx,
                                                    .name = "remoteUser"),
                                .out = s_text);
    plan->user = plan->has_user ? EMBED_CALL(cli_devcontainer_plan_text_internal.plan_append,
                                             CliDevcontainerPlanTextCtx, .plan = plan, .text = s_text)
                                : 0u;
    EMBED_CALL(cli_devcontainer_plan_internal.plan_commands, CliDevcontainerPlanCtx, .plan = plan,
               .key = "onCreateCommand", .commands = plan->on_create, .count = &plan->on_creates);
    EMBED_CALL(cli_devcontainer_plan_internal.plan_commands, CliDevcontainerPlanCtx, .plan = plan,
               .key = "postCreateCommand", .commands = plan->on_create, .count = &plan->on_creates);
    EMBED_CALL(cli_devcontainer_plan_internal.plan_commands, CliDevcontainerPlanCtx, .plan = plan,
               .key = "postStartCommand", .commands = plan->on_start, .count = &plan->on_starts);
    if (cli_plan_overflow())
    {
        mmgr_line_put(err_msg, file);
        mmgr_line_put(err_msg, ": it contains more than a plan stores");
        return 0;
    }
    return 1;
}

int cli_devcontainer_plan(const CliDevcontainerRootCfg *args)
{
    const int done = EMBED_CALL(cli_plan_lock, DevcontainerReadCtx, .root = args->root, .plan = args->plan_out,
                                .err_msg = args->err_msg);
    cli_plan_description_free();
    return done;
}
