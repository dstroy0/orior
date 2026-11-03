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
// `mounts`, `runArgs` and `containerUser`, and it is stays running by a command of orior's in place of the image's own.
// A description with `dockerComposeFile` brings up its `service` with docker compose, which mounts the tree as its
// files specify. `onCreateCommand` and `postCreateCommand` run in the container once it is made, and `postStartCommand`
// each time it starts. `${localWorkspaceFolder}`, `${localWorkspaceFolderBasename}`, `${containerWorkspaceFolder}`,
// `${containerWorkspaceFolderBasename}` and `${localEnv:NAME}` read as they do elsewhere.
//
// The container is named for the tree, and found again by that name: brought up once, it is started again as it is
#ifndef DEVCONTAINER_H
#define DEVCONTAINER_H
#include "../command_line/cli_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliDevcontainerErrMsgCfg CliDevcontainerErrMsgCfg;
typedef struct CliDevcontainerPlanCfg CliDevcontainerPlanCfg;
typedef struct CliDevcontainerRootCfg CliDevcontainerRootCfg;
typedef struct CliDevcontainerTextCfg CliDevcontainerTextCfg;
typedef struct CliDevcontainerPlanContainerCfg CliDevcontainerPlanContainerCfg;
typedef struct CliDevcontainerPlanWordsCfg CliDevcontainerPlanWordsCfg;
typedef struct CliDevcontainerPlanWordsImageCfg CliDevcontainerPlanWordsImageCfg;
typedef struct CliDevcontainerCfg CliDevcontainerCfg;
typedef struct CliDevcontainerRootNameCfg CliDevcontainerRootNameCfg;
typedef struct CliDevcontainerPlanAtCfg CliDevcontainerPlanAtCfg;

// the most bytes a plan's texts take, the most entries of one of its lists, and the most commands it runs at a time
#define CLI_PLAN_TEXT (1u << 16u)

#define CLI_PLAN_LIST_MAX 256u

#define CLI_PLAN_COMMANDS_MAX 64u

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
    unsigned int at[CLI_PLAN_LIST_MAX];
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

// A tree's dev container, as its description specifies. Each text is the place in `text` it starts at: the
// description's file, the tree, the container's and the compose project's name, and the tree's folder in the container.
// `source` specifies what the container is built from: the `image`; the `dockerfile` built from `context` with `args`,
// key and value in turn, and the `target` where `targeted` is 1; or the compose `files` and their `service`, with
// `services` the ones brought up beside it. `env` contains name and value in turn; `user` is the container's user where
// `has_user` is 1
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
    CliPlanCommand on_create[CLI_PLAN_COMMANDS_MAX];
    unsigned int on_creates;
    CliPlanCommand on_start[CLI_PLAN_COMMANDS_MAX];
    unsigned int on_starts;
} CliPlan;

// JSON with comments and trailing commas, as VS Code's files write it, made plain JSON into `plain`: each comment
// outside a string taken out, and each comma before a closing bracket
void cli_plain_json(const CliDevcontainerTextCfg *args);
// the tree's description of its dev container into `file`, sized for CLI_PATH_MAX_LEN: 1, or 0 where it has none
int cli_devcontainer_find(const CliDevcontainerCfg *args);
// the container's name for the tree at `root` into `name`, sized for CLI_PATH_MAX_LEN: the tree's folder as docker
// takes names, and a mark of its whole path
void cli_devcontainer_name(const CliDevcontainerRootNameCfg *args);
// the tree's dev container as its description specifies into `plan`: 1, or 0 with why given
int cli_devcontainer_plan(const CliDevcontainerRootCfg *args);
// the plan's text that starts at `at`
const char *cli_plan_text(const CliDevcontainerPlanAtCfg *args);

// the address a window opens the tree at in the container named `container`, into `address`, sized for
// CLI_PATH_MAX_LEN
void cli_plan_address(const CliDevcontainerPlanContainerCfg *args);
// the words of `docker build` for an image the description builds, and the image's name into `image`, sized for
// CLI_PATH_MAX_LEN: 1, or 0 where it builds none
int cli_plan_build_words(const CliDevcontainerPlanWordsCfg *args);
// the words of `docker run` that make the container from `image`
void cli_plan_run_words(const CliDevcontainerPlanWordsImageCfg *args);
// the words of `docker compose` that name the description's files and its project: 1, or 0 where it has none
int cli_plan_compose_words(const CliDevcontainerPlanWordsCfg *args);
// the words of `docker exec` that run one of the description's commands in `container`
void cli_plan_exec_words(const CliDevcontainerPlanCfg *args);
// the tree's dev container brought up: started where it is made and stopped, made where it is not, its commands run as
// the description specifies, each docker line printed to `sink`; the address a window opens the tree at into `address`,
// which is sized for CLI_PATH_MAX_LEN: 1, or 0 with why given
int cli_devcontainer_up(const CliDevcontainerErrMsgCfg *args);

struct CliDevcontainerErrMsgCfg
{
    VerbaLinea *err_msg;
    const CliPlan *plan;
    char *address;
    QuerySink sink;
};

struct CliDevcontainerPlanCfg
{
    const CliPlan *plan;
    const char *container;
    CliWords *words;
    const CliPlanCommand *command;
};

struct CliDevcontainerRootCfg
{
    const char *root;
    CliPlan *plan_out;
    VerbaLinea *err_msg;
};

struct CliDevcontainerTextCfg
{
    const char *text;
    unsigned long long size;
    VerbaLinea *plain;
};

struct CliDevcontainerPlanContainerCfg
{
    const CliPlan *plan;
    const char *container;
    char *address;
};

struct CliDevcontainerPlanWordsCfg
{
    const CliPlan *plan;
    CliWords *words;
    char *image;
};

struct CliDevcontainerPlanWordsImageCfg
{
    const CliPlan *plan;
    CliWords *words;
    const char *image_read;
};

struct CliDevcontainerCfg
{
    const char *root;
    char *file;
};

struct CliDevcontainerRootNameCfg
{
    const char *root;
    char *name;
};

struct CliDevcontainerPlanAtCfg
{
    const CliPlan *plan;
    unsigned int at;
};

typedef struct
{
    void (*plain_json)(const CliDevcontainerTextCfg *args);
} CliDevcontainerTextNs;
EMBED_TABLE_LAYOUT(CliDevcontainerTextNs, plain_json);
EMBED_TABLE_STORAGE CliDevcontainerTextNs cli_devcontainer_text EMBED_UNUSED = {
    .plain_json = cli_plain_json,
};

typedef struct
{
    int (*find)(const CliDevcontainerCfg *args);
} CliDevcontainerNs;
EMBED_TABLE_LAYOUT(CliDevcontainerNs, find);
EMBED_TABLE_STORAGE CliDevcontainerNs cli_devcontainer_table EMBED_UNUSED = {
    .find = cli_devcontainer_find,
};

typedef struct
{
    void (*name)(const CliDevcontainerRootNameCfg *args);
} CliDevcontainerRootNameNs;
EMBED_TABLE_LAYOUT(CliDevcontainerRootNameNs, name);
EMBED_TABLE_STORAGE CliDevcontainerRootNameNs cli_devcontainer_root_name EMBED_UNUSED = {
    .name = cli_devcontainer_name,
};

typedef struct
{
    int (*plan)(const CliDevcontainerRootCfg *args);
} CliDevcontainerRootNs;
EMBED_TABLE_LAYOUT(CliDevcontainerRootNs, plan);
EMBED_TABLE_STORAGE CliDevcontainerRootNs cli_devcontainer_root EMBED_UNUSED = {
    .plan = cli_devcontainer_plan,
};

typedef struct
{
    const char *(*plan_text)(const CliDevcontainerPlanAtCfg *args);
} CliDevcontainerPlanAtNs;
EMBED_TABLE_LAYOUT(CliDevcontainerPlanAtNs, plan_text);
EMBED_TABLE_STORAGE CliDevcontainerPlanAtNs cli_devcontainer_plan_at EMBED_UNUSED = {
    .plan_text = cli_plan_text,
};

typedef struct
{
    void (*plan_address)(const CliDevcontainerPlanContainerCfg *args);
} CliDevcontainerPlanContainerNs;
EMBED_TABLE_LAYOUT(CliDevcontainerPlanContainerNs, plan_address);
EMBED_TABLE_STORAGE CliDevcontainerPlanContainerNs cli_devcontainer_plan_container EMBED_UNUSED = {
    .plan_address = cli_plan_address,
};

typedef struct
{
    int (*plan_build_words)(const CliDevcontainerPlanWordsCfg *args);
    int (*plan_compose_words)(const CliDevcontainerPlanWordsCfg *args);
} CliDevcontainerPlanWordsNs;
EMBED_TABLE_LAYOUT(CliDevcontainerPlanWordsNs, plan_build_words, plan_compose_words);
EMBED_TABLE_STORAGE CliDevcontainerPlanWordsNs cli_devcontainer_plan_words EMBED_UNUSED = {
    .plan_build_words = cli_plan_build_words,
    .plan_compose_words = cli_plan_compose_words,
};

typedef struct
{
    void (*plan_run_words)(const CliDevcontainerPlanWordsImageCfg *args);
} CliDevcontainerPlanWordsImageNs;
EMBED_TABLE_LAYOUT(CliDevcontainerPlanWordsImageNs, plan_run_words);
EMBED_TABLE_STORAGE CliDevcontainerPlanWordsImageNs cli_devcontainer_plan_words_image EMBED_UNUSED = {
    .plan_run_words = cli_plan_run_words,
};

typedef struct
{
    void (*plan_exec_words)(const CliDevcontainerPlanCfg *args);
} CliDevcontainerPlanNs;
EMBED_TABLE_LAYOUT(CliDevcontainerPlanNs, plan_exec_words);
EMBED_TABLE_STORAGE CliDevcontainerPlanNs cli_devcontainer_plan_table EMBED_UNUSED = {
    .plan_exec_words = cli_plan_exec_words,
};

typedef struct
{
    int (*up)(const CliDevcontainerErrMsgCfg *args);
} CliDevcontainerErrMsgNs;
EMBED_TABLE_LAYOUT(CliDevcontainerErrMsgNs, up);
EMBED_TABLE_STORAGE CliDevcontainerErrMsgNs cli_devcontainer_err_msg EMBED_UNUSED = {
    .up = cli_devcontainer_up,
};

#endif
