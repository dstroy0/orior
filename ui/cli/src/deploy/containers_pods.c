// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// containers_pods.c: kubectl run for the cluster's pods and a pod's logs, and what it writes read
#include "../text/text.h"
#include "containers_internal.h"

typedef struct
{
    const CliJson *json;
    unsigned int node;
    const char *key;
    const char *otherwise;
    unsigned long long *length;
    unsigned int one;
    unsigned int other;
    int first;
    VerbaLinea *out;
} ContainersPodsCtx;

// the text of a string member, or `otherwise` where it is none
EMBED_INLINE const char *cli_string_or(const ContainersPodsCtx *args)
{
    const CliJson *json = args->json;
    unsigned int node = args->node;
    unsigned long long *length = args->length;
    const char *const text =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = node, .name = args->key),
                   .length_pointer = length);
    if (text == NULL)
    {
        *length = cellul.len(&(CatenaFinitaCfg){.src = args->otherwise, .cap = CLI_CELLS_POOL});
    }
    return (text != NULL) ? text : args->otherwise;
}

// whether a pod comes before another: by namespace, then name
EMBED_INLINE int cli_pod_before(const ContainersPodsCtx *args)
{
    const CliJson *json = args->json;
    unsigned int one = args->one;
    unsigned int other = args->other;
    unsigned long long length = 0ull;
    const unsigned int one_meta = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                             .object = one, .name = "metadata");
    const unsigned int other_meta = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                               .json_read = json, .object = other, .name = "metadata");
    int order =
        EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg,
                   .one = EMBED_CALL(cli_string_or, ContainersPodsCtx, .json = json, .node = one_meta,
                                     .key = "namespace", .otherwise = "default", .length = &length),
                   .other_pointer = EMBED_CALL(cli_string_or, ContainersPodsCtx, .json = json, .node = other_meta,
                                               .key = "namespace", .otherwise = "default", .length = &length));
    order =
        (order != 0)
            ? order
            : EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg,
                         .one = EMBED_CALL(cli_string_or, ContainersPodsCtx, .json = json, .node = one_meta,
                                           .key = "name", .otherwise = "", .length = &length),
                         .other_pointer = EMBED_CALL(cli_string_or, ContainersPodsCtx, .json = json, .node = other_meta,
                                                     .key = "name", .otherwise = "", .length = &length));
    return order < 0;
}

// a pod written as the window reads it
EMBED_INLINE void cli_pod_write(const ContainersPodsCtx *args)
{
    const CliJson *json = args->json;
    VerbaLinea *out = args->out;
    const unsigned int meta = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                         .object = args->one, .name = "metadata");
    const unsigned int spec = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                         .object = args->one, .name = "spec");
    const unsigned int status = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                           .object = args->one, .name = "status");
    const unsigned int containers = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                               .json_read = json, .object = spec, .name = "containers");
    const unsigned int statuses = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                             .object = status, .name = "containerStatuses");
    unsigned long long length = 0ull;
    const char *text = EMBED_CALL(cli_string_or, ContainersPodsCtx, .json = json, .node = meta, .key = "namespace",
                                  .otherwise = "default", .length = &length);
    mmgr_line_put(out, args->first ? "{\"namespace\":" : ",{\"namespace\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = text,
               .length = length);
    text = EMBED_CALL(cli_string_or, ContainersPodsCtx, .json = json, .node = meta, .key = "name", .otherwise = "",
                      .length = &length);
    mmgr_line_put(out, ",\"name\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = text,
               .length = length);
    text = EMBED_CALL(cli_string_or, ContainersPodsCtx, .json = json, .node = status, .key = "phase",
                      .otherwise = "Unknown", .length = &length);
    mmgr_line_put(out, ",\"phase\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = text,
               .length = length);
    unsigned int names = 0u;
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = containers, .index = at) != CLI_JSON_NONE;
         at += 1u)
    {
        names += (EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                             .node = EMBED_CALL(
                                 cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                 .object = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                      .json_read = json, .array = containers, .index = at),
                                 .name = "name"),
                             .length_pointer = NULL) != NULL)
                     ? 1u
                     : 0u;
    }
    unsigned int ready = 0u;
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = statuses, .index = at) != CLI_JSON_NONE;
         at += 1u)
    {
        ready += EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                            .node = EMBED_CALL(
                                cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                .object = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                     .json_read = json, .array = statuses, .index = at),
                                .name = "ready"),
                            .kind = CFG_JSON_TRUE)
                     ? 1u
                     : 0u;
    }
    mmgr_line_put(out, ",\"ready\":\"");
    mmgr_line_decimal(out, ready, 0u);
    mmgr_line_ch(out, '/');
    mmgr_line_decimal(out, names, 0u);
    mmgr_line_put(out, "\",\"containers\":[");
    unsigned int written = 0u;
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = containers, .index = at) != CLI_JSON_NONE;
         at += 1u)
    {
        const char *const name = EMBED_CALL(
            cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
            .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                               .object = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                    .json_read = json, .array = containers, .index = at),
                               .name = "name"),
            .length_pointer = &length);
        if (name != NULL)
        {
            mmgr_line_put(out, (written != 0u) ? "," : "");
            EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = name,
                       .length = length);
            written += 1u;
        }
    }
    text = EMBED_CALL(cli_string_or, ContainersPodsCtx, .json = json, .node = spec, .key = "nodeName", .otherwise = "",
                      .length = &length);
    mmgr_line_put(out, "],\"node\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = text,
               .length = length);
    mmgr_line_ch(out, '}');
}

void cli_containers_pods(const CliContainersOutLineCfg *args)
{
    static const char *const s_args[6] = {"get", "pods", "-A", "-o", "json", "--request-timeout=5s"};
    char problem[CLI_NAME_MAX_LEN * 4u];
    VerbaLinea err_msg = {problem, sizeof(problem), 0ull};
    CliRan *const ran = cli_ran_take();
    const int listed =
        EMBED_CALL(cli_containers_variable_internal.tool, CliContainersVariableCtx, .variable = "ORIOR_KUBECTL",
                   .program = "kubectl", .request = s_args, .count = 6u, .ran = ran, .err_msg = &err_msg);
    EMBED_CALL(cli_containers_out_internal.pods_list, CliContainersOutCtx, .text = ran->out,
               .size = listed ? ran->out_size : 0ull, .listed = listed, .problem = problem, .problem_size = err_msg.at,
               .out = args->out);
    EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
}

void cli_containers_pods_list(const CliContainersOutCtx *args)
{
    CliJson json;
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unread = {why, sizeof(why), 0ull};
    const unsigned int root =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                   .text = args->text, .size = args->listed ? args->size : 0ull, .nodes_more = 0u, .text_more = 0ull,
                   .err_msg = &unread, .owner = &g_cli_deploy_cells);
    const unsigned int items = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                          .object = root, .name = "items");
    if (!EMBED_CALL(cli_containers_internal.lock, CliContainersCtx, .out = args->out,
                    .most = (items != CLI_JSON_NONE) ? json.node[items].count : 0u, .text_most = 0ull))
    {
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
        return;
    }
    unsigned int count = 0u;
    for (unsigned int at = 0u; (count < g_cli_containers.most) &&
                               (EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                           .json_read = &json, .array = items, .index = at) != CLI_JSON_NONE);
         at += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = &json, .array = items, .index = at);
        unsigned int place = count;
        while ((place > 0u) && EMBED_CALL(cli_pod_before, ContainersPodsCtx, .json = &json, .one = one,
                                          .other = g_cli_containers.order[place - 1u]))
        {
            g_cli_containers.order[place] = g_cli_containers.order[place - 1u];
            place -= 1u;
        }
        g_cli_containers.order[place] = one;
        count += 1u;
    }
    mmgr_line_put(args->out, "{\"items\":[");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        EMBED_CALL(cli_pod_write, ContainersPodsCtx, .json = &json, .one = g_cli_containers.order[at],
                   .first = at == 0u, .out = args->out);
    }
    EMBED_CALL(cli_containers_out_listed_internal.problem_write, CliContainersOutListedCtx, .out = args->out,
               .listed = args->listed, .problem = args->problem, .length = args->problem_size);
    cli_containers_release();
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
}

int cli_containers_pod_logs(const CliContainersOutCfg *args)
{
    char lines[16];
    VerbaLinea count = {lines, sizeof(lines), 0ull};
    mmgr_line_decimal(&count, CLI_LOG_LINES, 0u);
    mmgr_line_finish(&count);
    const char *const arguments[7] = {"logs", "-n", args->space, args->name, "--tail", lines, args->container};
    const unsigned int words = ((args->container != NULL) && (args->container[0] != '\0')) ? 7u : 6u;
    const char *const with[8] = {arguments[0], arguments[1], arguments[2], arguments[3],
                                 arguments[4], arguments[5], "-c",         args->container};
    CliRan *const ran = cli_ran_take();
    const int read =
        EMBED_CALL(cli_containers_variable_internal.tool, CliContainersVariableCtx, .variable = "ORIOR_KUBECTL",
                   .program = "kubectl", .request = (words == 7u) ? with : arguments, .count = (words == 7u) ? 8u : 6u,
                   .ran = ran, .err_msg = args->err_msg);
    if (read)
    {
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
                   .text_bytes = (const unsigned char *)ran->out, .length = ran->out_size);
    }
    EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
    return read;
}
