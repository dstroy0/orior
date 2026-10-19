// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// containers_pods.c: kubectl run for the cluster's pods and a pod's logs, and what it writes read
#include "../text/text.h"
#include "containers_internal.h"

// the text of a string member, or `otherwise` where it is none
static const char *cli_string_or(const CliJson *json, unsigned int node, const char *key, const char *otherwise,
                                 unsigned long long *length)
{
    const char *const text = cli_json_string(json, cli_json_member(json, node, key), length);
    if (text == NULL)
    {
        *length = cellul.len(&(CatenaFinitaCfg){.src = otherwise, .cap = CLI_CELLS_POOL});
    }
    return (text != NULL) ? text : otherwise;
}

// whether a pod comes before another: by namespace, then name
static int cli_pod_before(const CliJson *json, unsigned int one, unsigned int other)
{
    unsigned long long length = 0ull;
    const unsigned int one_meta = cli_json_member(json, one, "metadata");
    const unsigned int other_meta = cli_json_member(json, other, "metadata");
    int order = cli_text_order(cli_string_or(json, one_meta, "namespace", "default", &length),
                               cli_string_or(json, other_meta, "namespace", "default", &length));
    order = (order != 0) ? order
                         : cli_text_order(cli_string_or(json, one_meta, "name", "", &length),
                                          cli_string_or(json, other_meta, "name", "", &length));
    return order < 0;
}

// a pod written as the window reads it
static void cli_pod_written(const CliJson *json, unsigned int one, int first, CliLine *out)
{
    const unsigned int meta = cli_json_member(json, one, "metadata");
    const unsigned int spec = cli_json_member(json, one, "spec");
    const unsigned int status = cli_json_member(json, one, "status");
    const unsigned int containers = cli_json_member(json, spec, "containers");
    const unsigned int statuses = cli_json_member(json, status, "containerStatuses");
    unsigned long long length = 0ull;
    const char *text = cli_string_or(json, meta, "namespace", "default", &length);
    cli_line_text(out, first ? "{\"namespace\":" : ",{\"namespace\":");
    cli_json_quoted(out, text, length);
    text = cli_string_or(json, meta, "name", "", &length);
    cli_line_text(out, ",\"name\":");
    cli_json_quoted(out, text, length);
    text = cli_string_or(json, status, "phase", "Unknown", &length);
    cli_line_text(out, ",\"phase\":");
    cli_json_quoted(out, text, length);
    unsigned int names = 0u;
    for (unsigned int at = 0u; cli_json_item(json, containers, at) != CLI_JSON_NONE; at += 1u)
    {
        names +=
            (cli_json_string(json, cli_json_member(json, cli_json_item(json, containers, at), "name"), NULL) != NULL)
                ? 1u
                : 0u;
    }
    unsigned int ready = 0u;
    for (unsigned int at = 0u; cli_json_item(json, statuses, at) != CLI_JSON_NONE; at += 1u)
    {
        ready += cli_json_is(json, cli_json_member(json, cli_json_item(json, statuses, at), "ready"), CFG_JSON_TRUE)
                     ? 1u
                     : 0u;
    }
    cli_line_text(out, ",\"ready\":\"");
    cli_line_decimal(out, ready, 0u);
    cli_line_character(out, '/');
    cli_line_decimal(out, names, 0u);
    cli_line_text(out, "\",\"containers\":[");
    unsigned int written = 0u;
    for (unsigned int at = 0u; cli_json_item(json, containers, at) != CLI_JSON_NONE; at += 1u)
    {
        const char *const name =
            cli_json_string(json, cli_json_member(json, cli_json_item(json, containers, at), "name"), &length);
        if (name != NULL)
        {
            cli_line_text(out, (written != 0u) ? "," : "");
            cli_json_quoted(out, name, length);
            written += 1u;
        }
    }
    text = cli_string_or(json, spec, "nodeName", "", &length);
    cli_line_text(out, "],\"node\":");
    cli_json_quoted(out, text, length);
    cli_line_character(out, '}');
}

void cli_containers_pods(CliLine *out)
{
    static const char *const s_args[6] = {"get", "pods", "-A", "-o", "json", "--request-timeout=5s"};
    char problem[CLI_NAME_LONGEST * 4u];
    CliLine said = {problem, sizeof(problem), 0ull};
    CliRan *const ran = cli_ran_taken();
    const int listed = cli_containers_tool("ORIOR_KUBECTL", "kubectl", s_args, 6u, ran, &said);
    cli_containers_pods_listed(ran->out, listed ? ran->out_size : 0ull, listed, problem, said.at, out);
    cli_ran_given(ran);
}

void cli_containers_pods_listed(const char *text, unsigned long long size, int listed, const char *problem,
                                unsigned long long problem_size, CliLine *out)
{
    CliJson json;
    char why[CLI_NAME_LONGEST];
    CliLine unread = {why, sizeof(why), 0ull};
    const unsigned int root = cli_json_taken(&json, text, listed ? size : 0ull, 0u, 0ull, &unread);
    const unsigned int items = cli_json_member(&json, root, "items");
    if (!cli_containers_held(out, (items != CLI_JSON_NONE) ? json.node[items].count : 0u, 0ull))
    {
        cli_json_given(&json);
        return;
    }
    unsigned int count = 0u;
    for (unsigned int at = 0u; (count < g_cli_containers.most) && (cli_json_item(&json, items, at) != CLI_JSON_NONE);
         at += 1u)
    {
        const unsigned int one = cli_json_item(&json, items, at);
        unsigned int place = count;
        while ((place > 0u) && cli_pod_before(&json, one, g_cli_containers.order[place - 1u]))
        {
            g_cli_containers.order[place] = g_cli_containers.order[place - 1u];
            place -= 1u;
        }
        g_cli_containers.order[place] = one;
        count += 1u;
    }
    cli_line_text(out, "{\"items\":[");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_pod_written(&json, g_cli_containers.order[at], at == 0u, out);
    }
    cli_containers_problem_written(out, listed, problem, problem_size);
    cli_containers_released();
    cli_json_given(&json);
}

int cli_containers_pod_logs(const char *space, const char *name, const char *container, CliLine *out, CliLine *said)
{
    char lines[16];
    CliLine count = {lines, sizeof(lines), 0ull};
    cli_line_decimal(&count, CLI_LOG_LINES, 0u);
    cli_line_closed(&count);
    const char *const args[7] = {"logs", "-n", space, name, "--tail", lines, container};
    const unsigned int words = ((container != NULL) && (container[0] != '\0')) ? 7u : 6u;
    const char *const with[8] = {args[0], args[1], args[2], args[3], args[4], args[5], "-c", container};
    CliRan *const ran = cli_ran_taken();
    const int read = cli_containers_tool("ORIOR_KUBECTL", "kubectl", (words == 7u) ? with : args,
                                         (words == 7u) ? 8u : 6u, ran, said);
    if (read)
    {
        cli_json_quoted_lossy(out, (const unsigned char *)ran->out, ran->out_size);
    }
    cli_ran_given(ran);
    return read;
}
