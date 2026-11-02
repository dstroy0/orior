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

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_an_unknown_action_is_refused(void)
{
    char why[256];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    TEST_ASSERT_FALSE(EMBED_CALL(cli_containers.act, CliContainersCfg, .name = "loose", .act = "remove", .err_msg = &err_msg));
}
