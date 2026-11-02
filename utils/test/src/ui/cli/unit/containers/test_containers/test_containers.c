// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_containers.c: docker's list gives each container its Compose project and service, kubectl's gives each pod its containers
// and ready count, and an unknown action is refused
#include "ui/cli/src/deploy/containers.h"
#include "ui/cli/src/deploy/containers_internal.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static char s_out[1u << 16u];
static const CliJson *s_json;
static unsigned int s_items;

static const char s_docker[] =
    "{\"ID\":\"b2\",\"Image\":\"redis:7\",\"Labels\":\"com.docker.compose.project=shop,com.docker.compose.service="
    "cache\",\"Names\":\"shop-cache-1\",\"Ports\":\"6379/tcp\",\"State\":\"running\",\"Status\":\"Up 2 hours\"}\n"
    "{\"ID\":\"a1\",\"Image\":\"python:3.12-slim\",\"Labels\":\"\",\"Names\":\"loose\",\"Ports\":\"\",\"State\":"
    "\"exited\",\"Status\":\"Exited (0) 3 minutes ago\"}\n";

static const char s_pods[] =
    "{\"items\":[{\"metadata\":{\"name\":\"web-7f\",\"namespace\":\"shop\"},\"spec\":{\"nodeName\":\"n1\","
    "\"containers\":[{\"name\":\"web\"},{\"name\":\"proxy\"}]},\"status\":{\"phase\":\"Running\","
    "\"containerStatuses\":[{\"ready\":true},{\"ready\":false}]}},{\"metadata\":{\"name\":\"dns\",\"namespace\":"
    "\"kube-system\"},\"spec\":{\"containers\":[{\"name\":\"coredns\"}]},\"status\":{\"phase\":\"Running\","
    "\"containerStatuses\":[{\"ready\":true}]}}]}";

// docker's list parsed into the shared tree
static void docker_listed(void)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_containers_out_internal.list, CliContainersOutCtx, .text = s_docker, .size = sizeof(s_docker) - 1u, .listed = 1, .problem = "", .problem_size = 0ull, .out = &out);
    unsigned int list = CLI_JSON_NONE;
    s_json = cli_unity_parsed(s_out, out.at, &list);
    s_items = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = list, .name = "items");
}

// kubectl's list parsed into the shared tree
static void pods_listed(void)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_containers_out_internal.pods_list, CliContainersOutCtx, .text = s_pods, .size = sizeof(s_pods) - 1u, .listed = 1, .problem = "", .problem_size = 0ull, .out = &out);
    unsigned int list = CLI_JSON_NONE;
    s_json = cli_unity_parsed(s_out, out.at, &list);
    s_items = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = list, .name = "items");
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_each_docker_line_is_a_container(void)
{
    docker_listed();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = s_json, .node = s_items, .kind = CFG_JSON_ARRAY));
    TEST_ASSERT_EQUAL_UINT(2u, s_json->node[s_items].count);
}

void test_a_container_with_no_project_comes_first_with_none(void)
{
    docker_listed();
    const unsigned int loose = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = s_items, .index = 0u);
    TEST_ASSERT_EQUAL_STRING("loose", cli_unity_member_text(loose, "name"));
    TEST_ASSERT_TRUE(EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = s_json, .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = loose, .name = "project"), .kind = CFG_JSON_NULL));
}

void test_a_compose_container_gives_its_project_service_and_state(void)
{
    docker_listed();
    const unsigned int cache = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = s_items, .index = 1u);
    TEST_ASSERT_EQUAL_STRING("shop", cli_unity_member_text(cache, "project"));
    TEST_ASSERT_EQUAL_STRING("cache", cli_unity_member_text(cache, "service"));
    TEST_ASSERT_EQUAL_STRING("running", cli_unity_member_text(cache, "state"));
}

void test_pods_are_sorted_by_namespace(void)
{
    pods_listed();
    TEST_ASSERT_EQUAL_STRING("kube-system", cli_unity_member_text(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = s_items, .index = 0u), "namespace"));
}

void test_a_pod_gives_its_namespace_name_phase_ready_count_and_node(void)
{
    pods_listed();
    const unsigned int web = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = s_items, .index = 1u);
    TEST_ASSERT_EQUAL_STRING("shop", cli_unity_member_text(web, "namespace"));
    TEST_ASSERT_EQUAL_STRING("web-7f", cli_unity_member_text(web, "name"));
    TEST_ASSERT_EQUAL_STRING("Running", cli_unity_member_text(web, "phase"));
    TEST_ASSERT_EQUAL_STRING("1/2", cli_unity_member_text(web, "ready"));
    TEST_ASSERT_EQUAL_STRING("n1", cli_unity_member_text(web, "node"));
}

void test_a_pod_lists_its_containers(void)
{
    pods_listed();
    const unsigned int containers = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = s_items, .index = 1u), .name = "containers");
    TEST_ASSERT_EQUAL_UINT(2u, s_json->node[containers].count);
    TEST_ASSERT_EQUAL_STRING("web", EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = s_json, .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = containers, .index = 0u), .length_pointer = NULL));
    TEST_ASSERT_EQUAL_STRING("proxy", EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = s_json, .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = containers, .index = 1u), .length_pointer = NULL));
}

void test_an_unknown_action_is_refused(void)
{
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    TEST_ASSERT_FALSE(EMBED_CALL(cli_containers.act, CliContainersCfg, .name = "loose", .act = "remove", .err_msg = &err_msg));
}
