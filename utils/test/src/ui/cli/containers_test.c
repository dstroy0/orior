// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// containers_test.c: the containers' check: docker's list gives each container its Compose project and service,
// kubectl's list gives each pod its containers and how many are ready, and a container is not asked to do what it does
// not
#include "../../../../../ui/cli/src/deploy/containers.h"
#include "../../../../../ui/cli/src/deploy/containers_internal.h"
#include "cli_check.h"

static char s_out[1u << 16u];

// docker's list gives each container its Compose project and service
static void containers_docker(void)
{
    static const char s_text[] =
        "{\"ID\":\"b2\",\"Image\":\"redis:7\",\"Labels\":\"com.docker.compose.project=shop,com.docker.compose.service="
        "cache\",\"Names\":\"shop-cache-1\",\"Ports\":\"6379/tcp\",\"State\":\"running\",\"Status\":\"Up 2 hours\"}\n"
        "{\"ID\":\"a1\",\"Image\":\"python:3.12-slim\",\"Labels\":\"\",\"Names\":\"loose\",\"Ports\":\"\",\"State\":"
        "\"exited\",\"Status\":\"Exited (0) 3 minutes ago\"}\n";
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_containers_listed(s_text, sizeof(s_text) - 1u, 1, "", 0ull, &out);
    unsigned int list = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_out, out.at, &list);
    const unsigned int items = cli_json_member(json, list, "items");
    const unsigned int loose = cli_json_item(json, items, 0u);
    const unsigned int cache = cli_json_item(json, items, 1u);
    cli_check(cli_json_is(json, items, CFG_JSON_ARRAY) && (json->node[items].count == 2u), "each line is a container");
    cli_check((cellul.eq(&(CatenaFinitaCfg){
                  .src = cli_check_member_text(loose, "name"), .other = "loose", .cap = CLI_CELLS_POOL})) &&
                  cli_json_is(json, cli_json_member(json, loose, "project"), CFG_JSON_NULL),
              "a container of no project comes first, with none");
    cli_check((cellul.eq(&(CatenaFinitaCfg){
                  .src = cli_check_member_text(cache, "project"), .other = "shop", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = cli_check_member_text(cache, "service"), .other = "cache", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = cli_check_member_text(cache, "state"), .other = "running", .cap = CLI_CELLS_POOL})),
              "a Compose container gives its project, its service and its state");
}

// kubectl's list gives each pod its containers and how many are ready
static void containers_pods(void)
{
    static const char s_text[] =
        "{\"items\":[{\"metadata\":{\"name\":\"web-7f\",\"namespace\":\"shop\"},\"spec\":{\"nodeName\":\"n1\","
        "\"containers\":[{\"name\":\"web\"},{\"name\":\"proxy\"}]},\"status\":{\"phase\":\"Running\","
        "\"containerStatuses\":[{\"ready\":true},{\"ready\":false}]}},{\"metadata\":{\"name\":\"dns\",\"namespace\":"
        "\"kube-system\"},\"spec\":{\"containers\":[{\"name\":\"coredns\"}]},\"status\":{\"phase\":\"Running\","
        "\"containerStatuses\":[{\"ready\":true}]}}]}";
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_containers_pods_listed(s_text, sizeof(s_text) - 1u, 1, "", 0ull, &out);
    unsigned int list = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_out, out.at, &list);
    const unsigned int items = cli_json_member(json, list, "items");
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = cli_check_member_text(cli_json_item(json, items, 0u), "namespace"),
                                           .other = "kube-system",
                                           .cap = CLI_CELLS_POOL}),
              "pods come by namespace");
    const unsigned int web = cli_json_item(json, items, 1u);
    const unsigned int containers = cli_json_member(json, web, "containers");
    const char *const first = cli_json_string(json, cli_json_item(json, containers, 0u), NULL);
    const char *const second = cli_json_string(json, cli_json_item(json, containers, 1u), NULL);
    cli_check((cellul.eq(&(CatenaFinitaCfg){
                  .src = cli_check_member_text(web, "namespace"), .other = "shop", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = cli_check_member_text(web, "name"), .other = "web-7f", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = cli_check_member_text(web, "phase"), .other = "Running", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = cli_check_member_text(web, "ready"), .other = "1/2", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = cli_check_member_text(web, "node"), .other = "n1", .cap = CLI_CELLS_POOL})) &&
                  (json->node[containers].count == 2u) && (first != NULL) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = "web", .cap = CLI_CELLS_POOL})) &&
                  (second != NULL) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = second, .other = "proxy", .cap = CLI_CELLS_POOL})),
              "a pod gives its containers, how many are ready and its node");
}

int main(void)
{
    containers_docker();
    containers_pods();
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check(!cli_containers_act("loose", "remove", &said), "a container is not asked to do what it does not");
    return cli_checked();
}
