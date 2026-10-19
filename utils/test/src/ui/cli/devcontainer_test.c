// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// devcontainer_test.c: the dev containers' check: an image's container mounts the tree and runs its commands there, a
// Dockerfile is built from its context with its args, a compose description brings up its service, and a description
// with nothing to build says so
#include "../../../../../ui/cli/src/deploy/devcontainer.h"
#include "cli_check.h"

#include "../../../../../ui/cli/src/text/text.h"

static CliPlan s_plan;
static CliWords s_words;
static char s_dir[CLI_PATH_LONGEST];
static char s_base[CLI_PATH_LONGEST];

// the folder `folder` with `description` written at `at` in it, its path into s_dir and its last name into s_base
static void devcontainer_tree(const char *folder, const char *description, const char *at)
{
    char path[CLI_PATH_LONGEST];
    cli_check_folder(folder, s_dir);
    cli_full(s_dir, ".devcontainer", path);
    (void)query_folder_made(path);
    cli_full(s_dir, at, path);
    (void)query_file_written(path, description,
                             cellul.len(&(CatenaFinitaCfg){.src = description, .cap = CLI_CELLS_POOL}));
    const char *const slash =
        cli_text_last(s_dir, cellul.len(&(CatenaFinitaCfg){.src = s_dir, .cap = CLI_CELLS_POOL}), '/');
    const char *const backslash =
        cli_text_last(s_dir, cellul.len(&(CatenaFinitaCfg){.src = s_dir, .cap = CLI_CELLS_POOL}), '\\');
    memor.cpy(
        &(MemoriaCfg){.dst = s_base,
                      .src = (backslash > slash) ? backslash + 1 : ((slash != NULL) ? slash + 1 : s_dir),
                      .bytes = cellul.len(&(CatenaFinitaCfg){
                                   .src = (backslash > slash) ? backslash + 1 : ((slash != NULL) ? slash + 1 : s_dir),
                                   .cap = CLI_CELLS_POOL}) +
                               1u});
}

// the words made empty
static CliWords *devcontainer_words(void)
{
    s_words.count = 0u;
    s_words.used = 0u;
    s_words.full = 0;
    return &s_words;
}

// whether a word of the words is `first` and the one after it `second`
static int devcontainer_pair(const char *first, const char *second)
{
    for (unsigned int at = 0u; at + 1u < s_words.count; at += 1u)
    {
        if ((cellul.eq(&(CatenaFinitaCfg){.src = s_words.word[at], .other = first, .cap = CLI_CELLS_POOL})) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_words.word[at + 1u], .other = second, .cap = CLI_CELLS_POOL})))
        {
            return 1;
        }
    }
    return 0;
}

// whether two paths are the same, each `\` a `/`
static int devcontainer_same_path(const char *one, const char *other)
{
    for (; (one[0] != '\0') && (other[0] != '\0'); one += 1, other += 1)
    {
        const char left = (one[0] == '\\') ? '/' : one[0];
        const char right = (other[0] == '\\') ? '/' : other[0];
        if (left != right)
        {
            return 0;
        }
    }
    return one[0] == other[0];
}

// `text` with s_base in place of `{name}` into `out`, which holds CLI_PATH_LONGEST
static const char *devcontainer_named(const char *text, char *out)
{
    CliLine line = {out, CLI_PATH_LONGEST, 0ull};
    const char *const mark = cellul.find(
        &(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL, .other = "{name}", .other_cap = CLI_CELLS_POOL});
    // the mark lies in the text, at or past its start
    cli_line_counted(&line, text,
                     (mark != NULL) ? (unsigned long long)(mark - text)
                                    : cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
    if (mark != NULL)
    {
        cli_line_text(&line, s_base);
        cli_line_text(&line, mark + 6);
    }
    (void)cli_line_closed(&line);
    return out;
}

// an image's container mounts the tree and runs its commands there
static void devcontainer_image(void)
{
    static const char s_description[] =
        "{\n  // the image\n  \"image\": \"python:3.12\",\n  \"containerEnv\": { \"TREE\": "
        "\"${containerWorkspaceFolder}\" },\n"
        "  \"mounts\": [{ \"source\": \"cache\", \"target\": \"/cache\", \"type\": \"volume\" }],\n"
        "  \"runArgs\": [\"--cap-add=SYS_PTRACE\"],\n  \"postCreateCommand\": \"pip install -e .\",\n"
        "  \"postStartCommand\": [\"git\", \"status\"],\n}\n";
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    char wanted[CLI_PATH_LONGEST];
    devcontainer_tree("devcontainer_image", s_description, ".devcontainer/devcontainer.json");
    cli_check(cli_devcontainer_plan(s_dir, &s_plan, &said), "an image's description is read");
    cli_check((s_plan.source == CLI_SOURCE_IMAGE) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = cli_plan_text(&s_plan, s_plan.image), .other = "python:3.12", .cap = CLI_CELLS_POOL})),
              "the container is made from its image");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = cli_plan_text(&s_plan, s_plan.folder),
                                           .other = devcontainer_named("/workspaces/{name}", wanted),
                                           .cap = CLI_CELLS_POOL}),
              "the tree is in /workspaces");
    cli_check((s_plan.env.count == 2u) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = cli_plan_text(&s_plan, s_plan.env.at[0]), .other = "TREE", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = cli_plan_text(&s_plan, s_plan.env.at[1]), .other = wanted, .cap = CLI_CELLS_POOL})),
              "the container's settings name the folder");
    cli_plan_run_words(&s_plan, "python:3.12", devcontainer_words());
    char bind[CLI_PATH_LONGEST * 2u];
    CliLine line = {bind, sizeof(bind), 0ull};
    cli_line_text(&line, "type=bind,source=");
    cli_line_text(&line, s_dir);
    cli_line_text(&line, ",target=");
    cli_line_text(&line, wanted);
    (void)cli_line_closed(&line);
    cli_check(devcontainer_pair("--mount", bind), "the tree is mounted");
    cli_check(devcontainer_pair("--mount", "source=cache,target=/cache,type=volume"), "and the description's mounts");
    int ptrace = 0;
    for (unsigned int at = 0u; at < s_words.count; at += 1u)
    {
        ptrace |= cellul.eq(
            &(CatenaFinitaCfg){.src = s_words.word[at], .other = "--cap-add=SYS_PTRACE", .cap = CLI_CELLS_POOL});
    }
    cli_check(ptrace, "with its own words to run");
    const char *const name = cli_plan_text(&s_plan, s_plan.name);
    cli_plan_exec_words(&s_plan, name, &s_plan.on_create[0], devcontainer_words());
    cli_check(
        (s_words.count == 7u) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_words.word[4], .other = "/bin/sh", .cap = CLI_CELLS_POOL})) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_words.word[5], .other = "-c", .cap = CLI_CELLS_POOL})) &&
            (cellul.eq(&(CatenaFinitaCfg){.src = s_words.word[6], .other = "pip install -e .", .cap = CLI_CELLS_POOL})),
        "a line runs in the shell");
    cli_plan_exec_words(&s_plan, name, &s_plan.on_start[0], devcontainer_words());
    cli_check((s_words.count == 6u) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_words.word[4], .other = "git", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_words.word[5], .other = "status", .cap = CLI_CELLS_POOL})),
              "and words as they are");
    char address[CLI_PATH_LONGEST];
    cli_plan_address(&s_plan, "c1", address);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = address,
                                           .other = devcontainer_named("docker:c1:/workspaces/{name}", wanted),
                                           .cap = CLI_CELLS_POOL}),
              "the window opens the tree in the container");
    char named[CLI_PATH_LONGEST];
    cli_devcontainer_name(s_dir, named);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = named, .other = name, .cap = CLI_CELLS_POOL}),
              "the container is named for the tree");
}

// a Dockerfile is built from its context with its args
static void devcontainer_build(void)
{
    static const char s_description[] =
        "{ \"build\": { \"dockerfile\": \"Dockerfile\", \"context\": \"..\", \"args\": { \"V\": "
        "\"${localWorkspaceFolderBasename}\" }, \"target\": \"dev\" }, \"workspaceFolder\": \"/src\" }";
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    char image[CLI_PATH_LONGEST];
    char wanted[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    devcontainer_tree("devcontainer_build", s_description, ".devcontainer.json");
    (void)cli_devcontainer_plan(s_dir, &s_plan, &said);
    cli_check(cli_plan_build_words(&s_plan, devcontainer_words(), image), "the description builds an image");
    CliLine line = {wanted, sizeof(wanted), 0ull};
    cli_line_text(&line, cli_plan_text(&s_plan, s_plan.name));
    cli_line_text(&line, "-image");
    (void)cli_line_closed(&line);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = image, .other = wanted, .cap = CLI_CELLS_POOL}),
              "named for the container");
    cli_full(s_dir, "Dockerfile", path);
    cli_check((s_words.count > 3u) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_words.word[0], .other = "build", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_words.word[1], .other = "-f", .cap = CLI_CELLS_POOL})) &&
                  devcontainer_same_path(s_words.word[2], path),
              "from its Dockerfile");
    cli_check(devcontainer_pair("--build-arg", devcontainer_named("V={name}", wanted)), "with its args");
    cli_check(devcontainer_pair("--target", "dev"), "and its target");
    cli_full(s_dir, "..", path);
    cli_check(devcontainer_same_path(s_words.word[s_words.count - 1u], path), "in its context");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = cli_plan_text(&s_plan, s_plan.folder), .other = "/src", .cap = CLI_CELLS_POOL}),
              "and the tree in the folder it names");
}

// a compose description brings up its service
static void devcontainer_compose(void)
{
    static const char s_description[] = "{ \"dockerComposeFile\": [\"compose.yml\", \"more.yml\"], \"service\": "
                                        "\"app\", \"workspaceFolder\": \"/work\" }";
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    char path[CLI_PATH_LONGEST];
    devcontainer_tree("devcontainer_compose", s_description, ".devcontainer/devcontainer.json");
    (void)cli_devcontainer_plan(s_dir, &s_plan, &said);
    cli_check(cli_plan_compose_words(&s_plan, devcontainer_words()), "a compose description names its files");
    cli_full(s_dir, ".devcontainer/compose.yml", path);
    cli_check((s_words.count > 3u) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_words.word[0], .other = "compose", .cap = CLI_CELLS_POOL})) &&
                  devcontainer_same_path(s_words.word[2], path),
              "each beside the description");
    cli_check((cellul.eq(
                  &(CatenaFinitaCfg){.src = s_words.word[s_words.count - 2u], .other = "-p", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = s_words.word[s_words.count - 1u],
                                                .other = cli_plan_text(&s_plan, s_plan.name),
                                                .cap = CLI_CELLS_POOL})),
              "and its project");
    cli_check((s_plan.source == CLI_SOURCE_COMPOSE) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = cli_plan_text(&s_plan, s_plan.service), .other = "app", .cap = CLI_CELLS_POOL})),
              "it brings up its service");
}

int main(void)
{
    devcontainer_image();
    devcontainer_build();
    devcontainer_compose();
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    devcontainer_tree("devcontainer_empty", "{ \"name\": \"empty\" }", ".devcontainer/devcontainer.json");
    cli_check(
        !cli_devcontainer_plan(s_dir, &s_plan, &said) && (cli_line_closed(&said) > 0ull) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = why, .cap = CLI_CELLS_POOL, .other = "names no image", .other_cap = CLI_CELLS_POOL}) != NULL),
        "a description with nothing to build says so");
    return cli_checked();
}
