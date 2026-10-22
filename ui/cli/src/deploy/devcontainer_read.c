// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// devcontainer_read.c: the tree's dev container read from its description into a plan
#include "../text/text.h"
#include "devcontainer_internal.h"

// the lock held while a description is read
static CliLock s_lock = CLI_LOCK_INIT;

// what the container is built from, as the description says, into the plan: 1, or 0 with why said
static int cli_source_read(CliPlan *plan, const char *file, const char *base, const CliPlanPlaces *places,
                           const unsigned int *compose, unsigned int composes, CliLine *said)
{
    static char s_text[CLI_STRING_LONGEST];
    static char s_read[CLI_STRING_LONGEST];
    char path[CLI_PATH_LONGEST];
    if (composes != 0u)
    {
        if (!cli_plan_string(cli_plan_member("service"), s_text))
        {
            cli_line_text(said, file);
            cli_line_text(said, " names dockerComposeFile and no service");
            return 0;
        }
        plan->source = CLI_SOURCE_COMPOSE;
        plan->service = cli_plan_put(plan, s_text);
        for (unsigned int at = 0u; at < composes; at += 1u)
        {
            if (cli_plan_string(compose[at], s_text))
            {
                cli_plan_expanded(s_text, places, s_read);
                cli_plan_joined(base, s_read, path);
                cli_plan_listed(plan, &plan->files, path);
            }
        }
        unsigned int tokens[CLI_PLAN_LIST_MOST];
        const unsigned int services = cli_plan_strings(cli_plan_member("runServices"), tokens, CLI_PLAN_LIST_MOST);
        for (unsigned int at = 0u; at < services; at += 1u)
        {
            if (cli_plan_string(tokens[at], s_text))
            {
                cli_plan_listed(plan, &plan->services, s_text);
            }
        }
        return 1;
    }
    if (cli_plan_string(cli_plan_member("image"), s_text))
    {
        plan->source = CLI_SOURCE_IMAGE;
        cli_plan_expanded(s_text, places, s_read);
        plan->image = cli_plan_put(plan, s_read);
        return 1;
    }
    const unsigned int build = cli_plan_member("build");
    if (!cli_plan_string(cli_plan_member_of(build, "dockerfile"), s_text) &&
        !cli_plan_string(cli_plan_member("dockerFile"), s_text))
    {
        cli_line_text(said, file);
        cli_line_text(said, " names no image, build.dockerfile or dockerComposeFile");
        return 0;
    }
    plan->source = CLI_SOURCE_BUILD;
    cli_plan_expanded(s_text, places, s_read);
    cli_plan_joined(base, s_read, path);
    plan->dockerfile = cli_plan_put(plan, path);
    if (!cli_plan_string(cli_plan_member_of(build, "context"), s_text))
    {
        memor.cpy(&(MemoriaCfg){.dst = s_text,
                                .src = ".",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = ".", .cap = CLI_CELLS_POOL}) + 1u});
    }
    cli_plan_expanded(s_text, places, s_read);
    cli_plan_joined(base, s_read, path);
    plan->context = cli_plan_put(plan, path);
    cli_plan_pairs(plan, cli_plan_member_of(build, "args"), places, &plan->args);
    plan->targeted = cli_plan_string(cli_plan_member_of(build, "target"), s_text);
    plan->target = plan->targeted ? cli_plan_put(plan, s_text) : 0u;
    return 1;
}

// the last part of `path` into `name`, which holds CLI_PATH_LONGEST: the separators after it left off, and a drive's
// root none
static void cli_last_named(const char *path, char *name)
{
    const char *end = path + cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
    while ((end > path) && ((end[-1] == '/') || (end[-1] == '\\')))
    {
        end -= 1;
    }
    const char *last = end;
    while ((last > path) && (last[-1] != '/') && (last[-1] != '\\'))
    {
        last -= 1;
    }
    last += (((end - last) == 2) && (last[1] == ':')) ? 2 : 0;
    CliLine line = {name, CLI_PATH_LONGEST, 0ull};
    cli_line_counted(&line, last, (unsigned long long)(end - last));
    cli_line_closed(&line);
}

static int cli_plan_held(const char *root, CliPlan *plan, CliLine *said)
{
    static char s_text[CLI_STRING_LONGEST];
    static char s_read[CLI_STRING_LONGEST];
    char file[CLI_PATH_LONGEST];
    if (!cli_devcontainer_find(root, file))
    {
        cli_line_text(said, root);
        cli_line_text(said, " has no .devcontainer/devcontainer.json");
        return 0;
    }
    if (!cli_plan_description_read(file, said))
    {
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = plan, .val = 0, .bytes = sizeof(*plan)});
    // the text at 0 is the empty one
    plan->used = 1u;
    plan->file = cli_plan_put(plan, file);
    plan->root = cli_plan_put(plan, root);
    char name[CLI_PATH_LONGEST];
    cli_devcontainer_name(root, name);
    plan->name = cli_plan_put(plan, name);
    // the description's folder, or the tree where it stands in none
    char base[CLI_PATH_LONGEST];
    memor.cpy(&(MemoriaCfg){
        .dst = base, .src = file, .bytes = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}) + 1u});
    char *const forward = cli_text_last(base, cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL}), '/');
    char *const backward =
        cli_text_last(base, cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL}), '\\');
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
    unsigned int compose[CLI_PLAN_LIST_MOST];
    const unsigned int composes = cli_plan_strings(cli_plan_member("dockerComposeFile"), compose, CLI_PLAN_LIST_MOST);
    const CliPlanPlaces outside = {root, ""};
    if (cli_plan_string(cli_plan_member("workspaceFolder"), s_text))
    {
        cli_plan_expanded(s_text, &outside, s_read);
        plan->folder = cli_plan_put(plan, s_read);
    }
    else
    {
        // the tree's folder's name is shorter than a path, and the folder it is put under shorter than a string
        char tree_name[CLI_PATH_LONGEST];
        cli_last_named(root, tree_name);
        CliLine line = {s_read, sizeof(s_read), 0ull};
        cli_line_text(&line, (composes != 0u) ? "/" : "/workspaces/");
        cli_line_text(&line, (composes != 0u) ? "" : tree_name);
        cli_line_closed(&line);
        plan->folder = cli_plan_put(plan, s_read);
    }
    const CliPlanPlaces places = {root, plan->text + plan->folder};
    if (!cli_source_read(plan, file, base, &places, compose, composes, said))
    {
        return 0;
    }
    cli_plan_pairs(plan, cli_plan_member("containerEnv"), &places, &plan->env);
    cli_plan_mounts(plan, &places);
    unsigned int tokens[CLI_PLAN_LIST_MOST];
    const unsigned int run_args = cli_plan_strings(cli_plan_member("runArgs"), tokens, CLI_PLAN_LIST_MOST);
    for (unsigned int each = 0u; each < run_args; each += 1u)
    {
        if (cli_plan_string(tokens[each], s_text))
        {
            cli_plan_expanded(s_text, &places, s_read);
            cli_plan_listed(plan, &plan->run_args, s_read);
        }
    }
    plan->has_user = cli_plan_string(cli_plan_member("containerUser"), s_text) ||
                     cli_plan_string(cli_plan_member("remoteUser"), s_text);
    plan->user = plan->has_user ? cli_plan_put(plan, s_text) : 0u;
    cli_plan_commands(plan, "onCreateCommand", plan->on_create, &plan->on_creates);
    cli_plan_commands(plan, "postCreateCommand", plan->on_create, &plan->on_creates);
    cli_plan_commands(plan, "postStartCommand", plan->on_start, &plan->on_starts);
    if (cli_plan_overflowed())
    {
        cli_line_text(said, file);
        cli_line_text(said, ": it holds more than a plan keeps");
        return 0;
    }
    return 1;
}

int cli_devcontainer_plan(const char *root, CliPlan *plan, CliLine *said)
{
    cli_lock_take(&s_lock);
    const int done = cli_plan_held(root, plan, said);
    cli_plan_description_given();
    cli_lock_give(&s_lock);
    return done;
}
