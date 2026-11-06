// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_devcontainer.c: an image's container mounts the tree and runs its commands there, a Dockerfile is built from its context with its
// args, a compose description brings up its service, and a description with nothing to build reports so
#include "ui/cli/src/deploy/devcontainer_internal.h"
#include "ui/cli/src/text/text.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static CliPlan s_plan;
static CliWords s_words;
static char s_dir[CLI_PATH_MAX_LEN];
static char s_base[CLI_PATH_MAX_LEN];
static char s_named[CLI_PATH_MAX_LEN];
static char s_error[512];
static VerbaLinea s_err_msg;

// the folder `folder` with `description` written at `at` in it, its path into s_dir and its last name into s_base
static void tree_written(const char *folder, const char *description, const char *at)
{
    char path[CLI_PATH_MAX_LEN];
    cli_unity_folder(folder, s_dir);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_dir, .relative = ".devcontainer", .joined = path);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = path));
    cli_unity_file(s_dir, at, description);
    const char *const slash = EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = s_dir, .length_value = CLI_UNITY_LENGTH(s_dir), .byte = '/');
    const char *const backslash = EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = s_dir, .length_value = CLI_UNITY_LENGTH(s_dir), .byte = '\\');
    const char *const base = (backslash > slash) ? backslash + 1 : ((slash != NULL) ? slash + 1 : s_dir);
    memor.cpy(&(MemoriaCfg){.dst = s_base, .src = base, .bytes = CLI_UNITY_LENGTH(base) + 1ull});
}

// the words emptied
static CliWords *words_emptied(void)
{
    s_words.count = 0u;
    s_words.used = 0u;
    s_words.full = 0;
    return &s_words;
}

// whether a word of the words is `first` and the one after it `second`
static int words_pair(const char *first, const char *second)
{
    for (unsigned int at = 0u; at + 1u < s_words.count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_words.word_text[at], .other = first, .cap = CLI_POOL_BYTES}) && cellul.eq(&(CatenaFinitaCfg){.src = s_words.word_text[at + 1u], .other = second, .cap = CLI_POOL_BYTES}))
        {
            return 1;
        }
    }
    return 0;
}

// whether two paths are the same, each `\` a `/`
static int same_path(const char *one, const char *other)
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

// `text` with s_base in place of `{name}`, in s_named
static const char *with_base(const char *text)
{
    VerbaLinea line = {s_named, sizeof(s_named), 0ull};
    const char *const mark = cellul.find(&(CatenaFinitaCfg){.src = text, .cap = CLI_POOL_BYTES, .other = "{name}", .other_cap = CLI_POOL_BYTES});
    // the mark lies in the text, at or past its start
    mmgr_line_put_n(&line, text, (mark != NULL) ? (unsigned long long)(mark - text) : CLI_UNITY_LENGTH(text));
    if (mark != NULL)
    {
        mmgr_line_put(&line, s_base);
        mmgr_line_put(&line, mark + 6);
    }
    (void)mmgr_line_finish(&line);
    return s_named;
}

// an image's description read into s_plan
static void image_planned(void)
{
    tree_written("devcontainer_image", "{\n  // the image\n  \"image\": \"python:3.12\",\n  \"containerEnv\": { \"TREE\": \"${containerWorkspaceFolder}\" },\n"
                                       "  \"mounts\": [{ \"source\": \"cache\", \"target\": \"/cache\", \"type\": \"volume\" }],\n"
                                       "  \"runArgs\": [\"--cap-add=SYS_PTRACE\"],\n  \"postCreateCommand\": \"pip install -e .\",\n"
                                       "  \"postStartCommand\": [\"git\", \"status\"],\n}\n",
                 ".devcontainer/devcontainer.json");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_devcontainer_root.plan, CliDevcontainerRootCfg, .root = s_dir, .plan_out = &s_plan, .err_msg = &s_err_msg));
}

// a Dockerfile's description read into s_plan and its build words made: the image's name in `image`
static void build_planned(char *image)
{
    tree_written("devcontainer_build",
                 "{ \"build\": { \"dockerfile\": \"Dockerfile\", \"context\": \"..\", \"args\": { \"V\": \"${localWorkspaceFolderBasename}\" }, \"target\": \"dev\" }, \"workspaceFolder\": \"/src\" }",
                 ".devcontainer.json");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_devcontainer_root.plan, CliDevcontainerRootCfg, .root = s_dir, .plan_out = &s_plan, .err_msg = &s_err_msg));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_devcontainer_plan_words.plan_build_words, CliDevcontainerPlanWordsCfg, .plan = &s_plan, .words = words_emptied(), .image = image));
}

// a compose description read into s_plan and its compose words made
static void compose_planned(void)
{
    tree_written("devcontainer_compose", "{ \"dockerComposeFile\": [\"compose.yml\", \"more.yml\"], \"service\": \"app\", \"workspaceFolder\": \"/work\" }", ".devcontainer/devcontainer.json");
    TEST_ASSERT_TRUE(EMBED_CALL(cli_devcontainer_root.plan, CliDevcontainerRootCfg, .root = s_dir, .plan_out = &s_plan, .err_msg = &s_err_msg));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_devcontainer_plan_words.plan_compose_words, CliDevcontainerPlanWordsCfg, .plan = &s_plan, .words = words_emptied()));
}

void setUp(void)
{
    cli_unity_begun();
    s_err_msg = (VerbaLinea){s_error, sizeof(s_error), 0ull};
}

void tearDown(void)
{
}

void test_the_container_is_created_from_its_image(void)
{
    image_planned();
    TEST_ASSERT_EQUAL_INT(CLI_SOURCE_IMAGE, s_plan.source);
    TEST_ASSERT_EQUAL_STRING("python:3.12", EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg, .plan = &s_plan, .at = s_plan.image));
}

void test_the_tree_is_in_workspaces(void)
{
    image_planned();
    TEST_ASSERT_EQUAL_STRING(with_base("/workspaces/{name}"), EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg, .plan = &s_plan, .at = s_plan.folder));
}

void test_the_containers_settings_name_the_folder(void)
{
    image_planned();
    TEST_ASSERT_EQUAL_UINT(2u, s_plan.env.count);
    TEST_ASSERT_EQUAL_STRING("TREE", EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg, .plan = &s_plan, .at = s_plan.env.at[0]));
    TEST_ASSERT_EQUAL_STRING(with_base("/workspaces/{name}"), EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg, .plan = &s_plan, .at = s_plan.env.at[1]));
}

void test_the_tree_is_mounted(void)
{
    char bind[CLI_PATH_MAX_LEN * 2u];
    image_planned();
    EMBED_CALL(cli_devcontainer_plan_words_image.plan_run_words, CliDevcontainerPlanWordsImageCfg, .plan = &s_plan, .image_read = "python:3.12", .words = words_emptied());
    VerbaLinea line = {bind, sizeof(bind), 0ull};
    mmgr_line_put(&line, "type=bind,source=");
    mmgr_line_put(&line, s_dir);
    mmgr_line_put(&line, ",target=");
    mmgr_line_put(&line, with_base("/workspaces/{name}"));
    (void)mmgr_line_finish(&line);
    TEST_ASSERT_TRUE(words_pair("--mount", bind));
}

void test_the_descriptions_mounts_are_mounted(void)
{
    image_planned();
    EMBED_CALL(cli_devcontainer_plan_words_image.plan_run_words, CliDevcontainerPlanWordsImageCfg, .plan = &s_plan, .image_read = "python:3.12", .words = words_emptied());
    TEST_ASSERT_TRUE(words_pair("--mount", "source=cache,target=/cache,type=volume"));
}

void test_the_descriptions_own_run_arguments_are_used(void)
{
    image_planned();
    EMBED_CALL(cli_devcontainer_plan_words_image.plan_run_words, CliDevcontainerPlanWordsImageCfg, .plan = &s_plan, .image_read = "python:3.12", .words = words_emptied());
    int ptrace = 0;
    for (unsigned int at = 0u; at < s_words.count; at += 1u)
    {
        ptrace |= cellul.eq(&(CatenaFinitaCfg){.src = s_words.word_text[at], .other = "--cap-add=SYS_PTRACE", .cap = CLI_POOL_BYTES});
    }
    TEST_ASSERT_TRUE(ptrace);
}

void test_a_command_line_runs_in_the_shell(void)
{
    image_planned();
    EMBED_CALL(cli_devcontainer_plan_table.plan_exec_words, CliDevcontainerPlanCfg, .plan = &s_plan, .container = EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg, .plan = &s_plan, .at = s_plan.name), .command = &s_plan.on_create[0], .words = words_emptied());
    TEST_ASSERT_EQUAL_UINT(7u, s_words.count);
    TEST_ASSERT_EQUAL_STRING("/bin/sh", s_words.word_text[4]);
    TEST_ASSERT_EQUAL_STRING("-c", s_words.word_text[5]);
    TEST_ASSERT_EQUAL_STRING("pip install -e .", s_words.word_text[6]);
}

void test_a_command_of_words_runs_as_it_is(void)
{
    image_planned();
    EMBED_CALL(cli_devcontainer_plan_table.plan_exec_words, CliDevcontainerPlanCfg, .plan = &s_plan, .container = EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg, .plan = &s_plan, .at = s_plan.name), .command = &s_plan.on_start[0], .words = words_emptied());
    TEST_ASSERT_EQUAL_UINT(6u, s_words.count);
    TEST_ASSERT_EQUAL_STRING("git", s_words.word_text[4]);
    TEST_ASSERT_EQUAL_STRING("status", s_words.word_text[5]);
}

void test_the_window_opens_the_tree_in_the_container(void)
{
    char address[CLI_PATH_MAX_LEN];
    image_planned();
    EMBED_CALL(cli_devcontainer_plan_container.plan_address, CliDevcontainerPlanContainerCfg, .plan = &s_plan, .container = "c1", .address = address);
    TEST_ASSERT_EQUAL_STRING(with_base("docker:c1:/workspaces/{name}"), address);
}

void test_the_container_is_named_for_the_tree(void)
{
    char named[CLI_PATH_MAX_LEN];
    image_planned();
    EMBED_CALL(cli_devcontainer_root_name.name, CliDevcontainerRootNameCfg, .root = s_dir, .name = named);
    TEST_ASSERT_EQUAL_STRING(EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg, .plan = &s_plan, .at = s_plan.name), named);
}

void test_the_image_is_named_for_the_container(void)
{
    char image[CLI_PATH_MAX_LEN];
    char required[CLI_PATH_MAX_LEN];
    build_planned(image);
    VerbaLinea line = {required, sizeof(required), 0ull};
    mmgr_line_put(&line, EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg, .plan = &s_plan, .at = s_plan.name));
    mmgr_line_put(&line, "-image");
    (void)mmgr_line_finish(&line);
    TEST_ASSERT_EQUAL_STRING(required, image);
}

void test_the_image_is_built_from_its_dockerfile(void)
{
    char image[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    build_planned(image);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_dir, .relative = "Dockerfile", .joined = path);
    TEST_ASSERT_GREATER_THAN_UINT(3u, s_words.count);
    TEST_ASSERT_EQUAL_STRING("build", s_words.word_text[0]);
    TEST_ASSERT_EQUAL_STRING("-f", s_words.word_text[1]);
    TEST_ASSERT_TRUE(same_path(s_words.word_text[2], path));
}

void test_the_image_is_built_with_its_args(void)
{
    char image[CLI_PATH_MAX_LEN];
    build_planned(image);
    TEST_ASSERT_TRUE(words_pair("--build-arg", with_base("V={name}")));
}

void test_the_image_is_built_with_its_target(void)
{
    char image[CLI_PATH_MAX_LEN];
    build_planned(image);
    TEST_ASSERT_TRUE(words_pair("--target", "dev"));
}

void test_the_image_is_built_in_its_context(void)
{
    char image[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    build_planned(image);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_dir, .relative = "..", .joined = path);
    TEST_ASSERT_TRUE(same_path(s_words.word_text[s_words.count - 1u], path));
}

void test_the_tree_is_in_the_folder_the_description_names(void)
{
    char image[CLI_PATH_MAX_LEN];
    build_planned(image);
    TEST_ASSERT_EQUAL_STRING("/src", EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg, .plan = &s_plan, .at = s_plan.folder));
}

void test_a_compose_description_names_its_files_adjacent_it(void)
{
    char path[CLI_PATH_MAX_LEN];
    compose_planned();
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_dir, .relative = ".devcontainer/compose.yml", .joined = path);
    TEST_ASSERT_GREATER_THAN_UINT(3u, s_words.count);
    TEST_ASSERT_EQUAL_STRING("compose", s_words.word_text[0]);
    TEST_ASSERT_TRUE(same_path(s_words.word_text[2], path));
}

void test_a_compose_description_names_its_project(void)
{
    compose_planned();
    TEST_ASSERT_EQUAL_STRING("-p", s_words.word_text[s_words.count - 2u]);
    TEST_ASSERT_EQUAL_STRING(EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg, .plan = &s_plan, .at = s_plan.name), s_words.word_text[s_words.count - 1u]);
}

void test_a_compose_description_fetches_up_its_service(void)
{
    compose_planned();
    TEST_ASSERT_EQUAL_INT(CLI_SOURCE_COMPOSE, s_plan.source);
    TEST_ASSERT_EQUAL_STRING("app", EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg, .plan = &s_plan, .at = s_plan.service));
}

void test_a_description_with_nothing_to_build_reports_it(void)
{
    tree_written("devcontainer_empty", "{ \"name\": \"empty\" }", ".devcontainer/devcontainer.json");
    TEST_ASSERT_FALSE(EMBED_CALL(cli_devcontainer_root.plan, CliDevcontainerRootCfg, .root = s_dir, .plan_out = &s_plan, .err_msg = &s_err_msg));
    TEST_ASSERT_GREATER_THAN_UINT64(0ull, mmgr_line_finish(&s_err_msg));
    CLI_TEST_ASSERT_CONTAINS("names no image", s_error);
}

void test_a_description_uses_memory_sized_to_it_and_releases_it(void)
{
    image_planned();
    TEST_ASSERT_EQUAL_UINT64(0ull, mmgr_owner_in_use(g_cli_devcontainer_owner));
    TEST_ASSERT_NOT_EQUAL(0ull, mmgr_owner_hw(g_cli_devcontainer_owner));
    TEST_ASSERT_LESS_THAN_UINT64(65536ull, mmgr_owner_hw(g_cli_devcontainer_owner));
}
