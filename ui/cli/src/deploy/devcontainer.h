// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// devcontainer.h: a tree's dev container, as its own devcontainer.json describes it, brought up with docker for a
// window to open the tree in (devcontainer.c finds the description and names the container, devcontainer_plan.c reads
// its JSON, devcontainer_read.c reads it into a plan, devcontainer_up.c writes docker's words and runs them).
//
// The description is read from `.devcontainer/devcontainer.json`, `.devcontainer.json`, or the first
// `.devcontainer/<name>/devcontainer.json`, as JSON with comments and trailing commas. Its image is `image`, or the one
// `build.dockerfile` builds from `build.context` with `build.args` and `build.target`; the tree is mounted at
// `workspaceFolder`, `/workspaces/<the tree's folder>` where it names none, the container runs with `containerEnv`,
// `mounts`, `runArgs` and `containerUser`, and it is kept running by a command of orior's in place of the image's own.
// A description with `dockerComposeFile` brings up its `service` with docker compose, which mounts the tree as its
// files say. `onCreateCommand` and `postCreateCommand` run in the container once it is made, and `postStartCommand`
// each time it starts. `${localWorkspaceFolder}`, `${localWorkspaceFolderBasename}`, `${containerWorkspaceFolder}`,
// `${containerWorkspaceFolderBasename}` and `${localEnv:NAME}` read as they do elsewhere.
//
// The container is named for the tree, and found again by that name: brought up once, it is started again as it is
#ifndef DEVCONTAINER_H
#define DEVCONTAINER_H

#include "../command_line/cli_internal.h"

// the most bytes a plan's texts take, the most entries of one of its lists, and the most commands it runs at a time
#define CLI_PLAN_TEXT (1u << 16u)

#define CLI_PLAN_LIST_MOST 256u

#define CLI_PLAN_COMMANDS_MOST 64u

// what a tree's dev container is built from: an image, a Dockerfile, or a compose project's service
enum
{
    CLI_SOURCE_IMAGE,
    CLI_SOURCE_BUILD,
    CLI_SOURCE_COMPOSE
};

// A list of a plan's texts, each the place in the plan's `text` it starts at
typedef struct
{
    unsigned int at[CLI_PLAN_LIST_MOST];
    unsigned int count;
} CliPlanList;

// One command the description runs in the container: a shell's line, its one word, where `line` is 1, else a program's
// words; `count` of the plan's `words` from `first`
typedef struct
{
    int line;
    unsigned int first;
    unsigned int count;
} CliPlanCommand;

// A tree's dev container, as its description says. Each text is the place in `text` it starts at: the description's
// file, the tree, the container's and the compose project's name, and the tree's folder in the container. `source`
// says what the container is built from: the `image`; the `dockerfile` built from `context` with `args`, key and value
// in turn, and the `target` where `targeted` is 1; or the compose `files` and their `service`, with `services` the
// ones brought up beside it. `env` holds name and value in turn; `user` is the container's user where `has_user` is 1
typedef struct
{
    char text[CLI_PLAN_TEXT];
    unsigned int used;
    unsigned int file;
    unsigned int root;
    unsigned int name;
    unsigned int folder;
    unsigned int source;
    unsigned int image;
    unsigned int dockerfile;
    unsigned int context;
    CliPlanList args;
    int targeted;
    unsigned int target;
    CliPlanList files;
    unsigned int service;
    CliPlanList services;
    CliPlanList env;
    CliPlanList mounts;
    CliPlanList run_args;
    int has_user;
    unsigned int user;
    CliPlanList words;
    CliPlanCommand on_create[CLI_PLAN_COMMANDS_MOST];
    unsigned int on_creates;
    CliPlanCommand on_start[CLI_PLAN_COMMANDS_MOST];
    unsigned int on_starts;
} CliPlan;

// JSON with comments and trailing commas, as VS Code's files hold it, made plain JSON into `plain`: each comment
// outside a string taken out, and each comma before a closing bracket
void cli_plain_json(const char *text, unsigned long long size, CliLine *plain);
// the tree's description of its dev container into `file`, which holds CLI_PATH_LONGEST: 1, or 0 where it has none
int cli_devcontainer_find(const char *root, char *file);
// the container's name for the tree at `root` into `name`, which holds CLI_PATH_LONGEST: the tree's folder as docker
// takes names, and a mark of its whole path
void cli_devcontainer_name(const char *root, char *name);
// the tree's dev container as its description says into `plan`: 1, or 0 with why said
int cli_devcontainer_plan(const char *root, CliPlan *plan, CliLine *said);
// the plan's text that starts at `at`
const char *cli_plan_text(const CliPlan *plan, unsigned int at);

// the address a window opens the tree at in the container named `container`, into `address`, which holds
// CLI_PATH_LONGEST
void cli_plan_address(const CliPlan *plan, const char *container, char *address);
// the words of `docker build` for an image the description builds, and the image's name into `image`, which holds
// CLI_PATH_LONGEST: 1, or 0 where it builds none
int cli_plan_build_words(const CliPlan *plan, CliWords *words, char *image);
// the words of `docker run` that make the container from `image`
void cli_plan_run_words(const CliPlan *plan, const char *image, CliWords *words);
// the words of `docker compose` that name the description's files and its project: 1, or 0 where it has none
int cli_plan_compose_words(const CliPlan *plan, CliWords *words);
// the words of `docker exec` that run one of the description's commands in `container`
void cli_plan_exec_words(const CliPlan *plan, const char *container, const CliPlanCommand *command, CliWords *words);
// the tree's dev container brought up: started where it is made and stopped, made where it is not, its commands run as
// the description says, each docker line said to `say`; the address a window opens the tree at into `address`, which
// holds CLI_PATH_LONGEST: 1, or 0 with why said
int cli_devcontainer_up(const CliPlan *plan, QuerySink say, char *address, CliLine *said);

#endif
