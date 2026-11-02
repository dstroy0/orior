// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// containers.c: docker and kubectl run, and docker's list, logs and acts, and what docker writes read
#include "../command_line/cells.h"
#include "../runs/runner.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "containers_internal.h"

typedef struct
{
    const char *variable;
    const char *program;
    const char *const *request;
    unsigned int count;
    CliRan *ran;
    VerbaLinea *err_msg;
    const char *text;
    unsigned long long size;
    const char *from;
    unsigned long long length;
    const CliJson *json;
    unsigned int object;
    const char *key;
    const char *labels;
    unsigned int *at_pointer;
    const char *line;
    int has_one;
    unsigned int one;
    int has_other;
    unsigned int other;
    const CliContainer *one_container_pointer;
    const CliContainer *other_container_pointer;
    VerbaLinea *out;
    int has;
    unsigned int at;
} ContainersCtx;

// docker or kubectl, as `variable` names it or as `program`, run with `args`: 1 with what it gave in `ran`, or 0 with
// why it did not start given
EMBED_INLINE int cli_tool_run(const ContainersCtx *args)
{
    const char *variable = args->variable;
    unsigned int count = args->count;
    CliRan *ran = args->ran;
    VerbaLinea *err_msg = args->err_msg;
    static CliWords s_words;
    static CliLock g_cli_containers_lock = CLI_LOCK_INIT;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &g_cli_containers_lock);
    s_words.count = 0u;
    s_words.used = 0u;
    s_words.full = 0;
    if (!EMBED_CALL(cli_runner_variable.program_words, CliRunnerVariableCfg, .variable = variable, .words = &s_words))
    {
        s_words.count = 0u;
        s_words.used = 0u;
        EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
                   .word = args->program);
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
                   .word = args->request[at]);
    }
    s_words.word[s_words.count] = NULL;
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unstarted = {why, sizeof(why), 0ull};
    const int started =
        !s_words.full && EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = s_words.word, .folder = NULL,
                                    .settings = NULL, .count = 0u, .ran = ran, .err_msg = &unstarted);
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &g_cli_containers_lock);
    if (!started)
    {
        mmgr_line_put(err_msg, args->program);
        mmgr_line_put(err_msg, " is not found: ");
        mmgr_line_put_n(err_msg, why, unstarted.at);
    }
    return started;
}

// the first line of `size` bytes that is not blank, trimmed, written as cli_lossy writes it: 1, or 0 where none is
EMBED_INLINE int cli_first_line(const ContainersCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned long long at = 0ull;
    while (at < size)
    {
        const unsigned long long end =
            EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = text, .at = at, .size = size).stop;
        unsigned long long from = at;
        unsigned long long to = end;
        from = EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg, .text = text, .at = from,
                          .size = to, .set = CLI_TEXT_WHITE);
        to = (from + EMBED_CALL(cli_text_at_size_classes.back_past, CliTextAtSizeClassesCfg, .text = text + from,
                                .at = to - from, .set = CLI_TEXT_WHITE));
        if (to > from)
        {
            EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = args->err_msg,
                       .text_bytes = (const unsigned char *)text + from, .length = to - from);
            return 1;
        }
        at = end + 1ull;
    }
    return 0;
}

int cli_containers_tool(const CliContainersVariableCtx *args)
{
    if (!EMBED_CALL(cli_tool_run, ContainersCtx, .variable = args->variable, .program = args->program,
                    .request = args->request, .count = args->count, .ran = args->ran, .err_msg = args->err_msg))
    {
        return 0;
    }
    if (args->ran->code == 0ll)
    {
        return 1;
    }
    if (!EMBED_CALL(cli_first_line, ContainersCtx, .text = args->ran->err, .size = args->ran->err_size,
                    .err_msg = args->err_msg))
    {
        mmgr_line_put(args->err_msg, args->program);
        mmgr_line_ch(args->err_msg, ' ');
        mmgr_line_put(args->err_msg, (args->count > 0u) ? args->request[0] : "");
        mmgr_line_put(args->err_msg, " failed");
    }
    return 0;
}

CliContainers g_cli_containers;
static CliLock s_containers_lock = CLI_LOCK_INIT;

int cli_containers_lock(const CliContainersCtx *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_containers_lock);
    const unsigned long long items = (unsigned long long)args->most * sizeof(CliContainer);
    const unsigned long long orders = (unsigned long long)args->most * sizeof(unsigned int);
    char *const cell = g_cli_cells->persistent_buf_alloc(items + orders + args->text_most + 1ull, &g_cli_deploy_cells);
    if (cell == NULL)
    {
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_containers_lock);
        static const char s_untaken[] = "a listing of containers could not be taken";
        mmgr_line_put(args->out, "{\"items\":[");
        EMBED_CALL(cli_containers_out_listed_internal.problem_write, CliContainersOutListedCtx, .out = args->out,
                   .listed = 0, .problem = s_untaken, .length = sizeof(s_untaken) - 1u);
        return 0;
    }
    // the items open the cell, on its boundary, the order after them on an unsigned int's, the texts last
    g_cli_containers = (CliContainers){(CliContainer *)(void *)cell,
                                       (unsigned int *)(void *)(cell + items),
                                       0u,
                                       args->most,
                                       cell + items + orders,
                                       0ull,
                                       args->text_most + 1ull};
    return 1;
}

void cli_containers_release(void)
{
    g_cli_cells->persistent_buf_release(g_cli_containers.item, &g_cli_deploy_cells);
    g_cli_containers = (CliContainers){NULL, NULL, 0u, 0u, NULL, 0ull, 0ull};
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_containers_lock);
}

// `length` bytes put in the texts, a 0 after them: their place, 0 where they do not fit, an empty text there
EMBED_INLINE unsigned int cli_text_store(const ContainersCtx *args)
{
    if ((g_cli_containers.used + args->length + 1ull) > g_cli_containers.text_most)
    {
        return 0u;
    }
    // the texts are what one listing wrote, which an unsigned int's places reach
    const unsigned int at = (unsigned int)g_cli_containers.used;
    memor.cpy(&(MemoriaCfg){.dst = g_cli_containers.text + at, .src = args->from, .bytes = args->length});
    g_cli_containers.text[at + args->length] = '\0';
    g_cli_containers.used += args->length + 1ull;
    return at;
}

// the string member `key` of an object stored, an empty text where it is none
EMBED_INLINE unsigned int cli_member_store(const ContainersCtx *args)
{
    const CliJson *json = args->json;
    unsigned int object = args->object;
    unsigned long long length = 0ull;
    const char *const text =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = object, .name = args->key),
                   .length_pointer = &length);
    return (text != NULL) ? EMBED_CALL(cli_text_store, ContainersCtx, .from = text, .length = length)
                          : EMBED_CALL(cli_text_store, ContainersCtx, .from = "", .length = 0ull);
}

// the value of the label `key` among `labels`, `key=value` parted by commas: 1 with it stored into `at`, or 0
EMBED_INLINE int cli_label(const ContainersCtx *args)
{
    unsigned int *at = args->at_pointer;
    const unsigned long long key_length = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_CELLS_POOL});
    unsigned long long from = 0ull;
    while (from <= args->size)
    {
        const char *const comma =
            memor.chr(&(MemoriaCfg){.src = args->labels + from, .val = ',', .bytes = args->size - from});
        // a comma found lies in the labels, at or past `from`
        const unsigned long long end = (comma != NULL) ? (unsigned long long)(comma - args->labels) : args->size;
        const char *const equals =
            memor.chr(&(MemoriaCfg){.src = args->labels + from, .val = '=', .bytes = end - from});
        if ((equals != NULL) && ((unsigned long long)(equals - (args->labels + from)) == key_length) &&
            (memor.cmp(&(MemoriaCfg){.src = args->labels + from, .other = args->key, .bytes = key_length}) == 0))
        {
            // the value runs from past the equals sign to the comma
            *at = EMBED_CALL(cli_text_store, ContainersCtx, .from = equals + 1,
                             .length = (unsigned long long)((args->labels + end) - (equals + 1)));
            return 1;
        }
        from = end + 1ull;
    }
    return 0;
}

// a line of `docker ps --format '{{json .}}'` read as a container listed
EMBED_INLINE void cli_container_read(const ContainersCtx *args)
{
    unsigned long long size = args->size;
    CliJson json;
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unread = {why, sizeof(why), 0ull};
    const unsigned int one = EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg,
                                        .json = &json, .text = args->line, .size = size, .nodes_more = 0u,
                                        .text_more = 0ull, .err_msg = &unread, .owner = &g_cli_deploy_cells);
    if ((one == CLI_JSON_NONE) || (g_cli_containers.count >= g_cli_containers.most))
    {
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
        return;
    }
    unsigned long long labels_size = 0ull;
    const char *labels = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                                    .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                       .json_read = &json, .object = one, .name = "Labels"),
                                    .length_pointer = &labels_size);
    labels = (labels != NULL) ? labels : "";
    CliContainer *const found = &g_cli_containers.item[g_cli_containers.count];
    found->id = EMBED_CALL(cli_member_store, ContainersCtx, .json = &json, .object = one, .key = "ID");
    found->name = EMBED_CALL(cli_member_store, ContainersCtx, .json = &json, .object = one, .key = "Names");
    found->image = EMBED_CALL(cli_member_store, ContainersCtx, .json = &json, .object = one, .key = "Image");
    found->state = EMBED_CALL(cli_member_store, ContainersCtx, .json = &json, .object = one, .key = "State");
    found->status = EMBED_CALL(cli_member_store, ContainersCtx, .json = &json, .object = one, .key = "Status");
    found->ports = EMBED_CALL(cli_member_store, ContainersCtx, .json = &json, .object = one, .key = "Ports");
    found->has_project = EMBED_CALL(cli_label, ContainersCtx, .labels = labels, .size = labels_size,
                                    .key = "com.docker.compose.project", .at_pointer = &found->project);
    found->has_service = EMBED_CALL(cli_label, ContainersCtx, .labels = labels, .size = labels_size,
                                    .key = "com.docker.compose.service", .at_pointer = &found->service);
    g_cli_containers.count += 1u;
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
}

// how an optional text sorts against another: none first
EMBED_INLINE int cli_optional_order(const ContainersCtx *args)
{
    unsigned int one = args->one;
    unsigned int other = args->other;
    if (args->has_one != args->has_other)
    {
        return args->has_one ? 1 : -1;
    }
    return args->has_one ? EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = g_cli_containers.text + one,
                                      .other_pointer = g_cli_containers.text + other)
                         : 0;
}

// whether a container comes before another: by project, then service, then name
EMBED_INLINE int cli_container_before(const ContainersCtx *args)
{
    const CliContainer *one = args->one_container_pointer;
    const CliContainer *other = args->other_container_pointer;
    int order = EMBED_CALL(cli_optional_order, ContainersCtx, .has_one = one->has_project, .one = one->project,
                           .has_other = other->has_project, .other = other->project);
    order = (order != 0) ? order
                         : EMBED_CALL(cli_optional_order, ContainersCtx, .has_one = one->has_service,
                                      .one = one->service, .has_other = other->has_service, .other = other->service);
    order = (order != 0)
                ? order
                : EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = g_cli_containers.text + one->name,
                             .other_pointer = g_cli_containers.text + other->name);
    return order < 0;
}

// an optional text written: the text, or null
EMBED_INLINE void cli_optional_write(const ContainersCtx *args)
{
    VerbaLinea *out = args->out;
    if (args->has)
    {
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out,
                   .text = g_cli_containers.text + args->at,
                   .length =
                       cellul.len(&(CatenaFinitaCfg){.src = g_cli_containers.text + args->at, .cap = CLI_CELLS_POOL}));
    }
    else
    {
        mmgr_line_put(out, "null");
    }
}

// a member of a container written, a comma before it
EMBED_INLINE void cli_field_write(const ContainersCtx *args)
{
    VerbaLinea *out = args->out;
    mmgr_line_put(out, args->key);
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out,
               .text = g_cli_containers.text + args->at,
               .length =
                   cellul.len(&(CatenaFinitaCfg){.src = g_cli_containers.text + args->at, .cap = CLI_CELLS_POOL}));
}

void cli_containers_problem_write(const CliContainersOutListedCtx *args)
{
    mmgr_line_put(args->out, "],\"problem\":");
    if (args->listed)
    {
        mmgr_line_put(args->out, "null}");
        return;
    }
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out, .text = args->problem,
               .length = args->length);
    mmgr_line_ch(args->out, '}');
}

void cli_containers_list(const CliContainersOutLineCfg *args)
{
    static const char *const s_args[5] = {"ps", "-a", "--no-trunc", "--format", "{{json .}}"};
    char problem[CLI_NAME_MAX_LEN * 4u];
    VerbaLinea err_msg = {problem, sizeof(problem), 0ull};
    CliRan *const ran = cli_ran_take();
    const int listed =
        EMBED_CALL(cli_containers_variable_internal.tool, CliContainersVariableCtx, .variable = "ORIOR_DOCKER",
                   .program = "docker", .request = s_args, .count = 5u, .ran = ran, .err_msg = &err_msg);
    EMBED_CALL(cli_containers_out_internal.list, CliContainersOutCtx, .text = ran->out,
               .size = listed ? ran->out_size : 0ull, .listed = listed, .problem = problem, .problem_size = err_msg.at,
               .out = args->out);
    EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
}

void cli_containers_on_list(const CliContainersOutCtx *args)
{
    // a container a line, and the texts stored of a line no more than its bytes and a 0 after each of its eight
    unsigned int lines = (args->size != 0ull) ? 1u : 0u;
    for (unsigned long long at = 0ull; at < args->size; at += 1ull)
    {
        lines += (args->text[at] == '\n') ? 1u : 0u;
    }
    if (!EMBED_CALL(cli_containers_internal.lock, CliContainersCtx, .out = args->out, .most = lines,
                    .text_most = args->size + (8ull * lines)))
    {
        return;
    }
    for (unsigned long long at = 0ull; at < args->size;)
    {
        const unsigned long long end =
            EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = args->text, .at = at, .size = args->size).stop;
        EMBED_CALL(cli_container_read, ContainersCtx, .line = args->text + at, .size = end - at);
        at = end + 1ull;
    }
    for (unsigned int at = 0u; at < g_cli_containers.count; at += 1u)
    {
        unsigned int place = at;
        while ((place > 0u) &&
               EMBED_CALL(cli_container_before, ContainersCtx, .one_container_pointer = &g_cli_containers.item[at],
                          .other_container_pointer = &g_cli_containers.item[g_cli_containers.order[place - 1u]]))
        {
            g_cli_containers.order[place] = g_cli_containers.order[place - 1u];
            place -= 1u;
        }
        g_cli_containers.order[place] = at;
    }
    mmgr_line_put(args->out, "{\"items\":[");
    for (unsigned int at = 0u; at < g_cli_containers.count; at += 1u)
    {
        const CliContainer *const one = &g_cli_containers.item[g_cli_containers.order[at]];
        EMBED_CALL(cli_field_write, ContainersCtx, .out = args->out,
                   .key = (at != 0u) ? ",{\"id\":" : "{\"id\":", .at = one->id);
        EMBED_CALL(cli_field_write, ContainersCtx, .out = args->out, .key = ",\"name\":", .at = one->name);
        EMBED_CALL(cli_field_write, ContainersCtx, .out = args->out, .key = ",\"image\":", .at = one->image);
        EMBED_CALL(cli_field_write, ContainersCtx, .out = args->out, .key = ",\"state\":", .at = one->state);
        EMBED_CALL(cli_field_write, ContainersCtx, .out = args->out, .key = ",\"status\":", .at = one->status);
        mmgr_line_put(args->out, ",\"project\":");
        EMBED_CALL(cli_optional_write, ContainersCtx, .out = args->out, .has = one->has_project, .at = one->project);
        mmgr_line_put(args->out, ",\"service\":");
        EMBED_CALL(cli_optional_write, ContainersCtx, .out = args->out, .has = one->has_service, .at = one->service);
        EMBED_CALL(cli_field_write, ContainersCtx, .out = args->out, .key = ",\"ports\":", .at = one->ports);
        mmgr_line_ch(args->out, '}');
    }
    EMBED_CALL(cli_containers_out_listed_internal.problem_write, CliContainersOutListedCtx, .out = args->out,
               .listed = args->listed, .problem = args->problem, .length = args->problem_size);
    cli_containers_release();
}

int cli_containers_act(const CliContainersCfg *args)
{
    if ((!cellul.eq(&(CatenaFinitaCfg){.src = args->act, .other = "start", .cap = CLI_CELLS_POOL})) &&
        (!cellul.eq(&(CatenaFinitaCfg){.src = args->act, .other = "stop", .cap = CLI_CELLS_POOL})) &&
        (!cellul.eq(&(CatenaFinitaCfg){.src = args->act, .other = "restart", .cap = CLI_CELLS_POOL})))
    {
        mmgr_line_put(args->err_msg, args->act);
        mmgr_line_put(args->err_msg, " is not something a container is");
        return 0;
    }
    const char *const arguments[2] = {args->act, args->name};
    CliRan *const ran = cli_ran_take();
    const int acted =
        EMBED_CALL(cli_containers_variable_internal.tool, CliContainersVariableCtx, .variable = "ORIOR_DOCKER",
                   .program = "docker", .request = arguments, .count = 2u, .ran = ran, .err_msg = args->err_msg);
    EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
    return acted;
}

int cli_containers_logs(const CliContainersOutLineCfg *args)
{
    char lines[16];
    VerbaLinea count = {lines, sizeof(lines), 0ull};
    mmgr_line_decimal(&count, CLI_LOG_LINES, 0u);
    mmgr_line_finish(&count);
    const char *const arguments[4] = {"logs", "--tail", lines, args->name};
    CliRan *const ran = cli_ran_take();
    if (!EMBED_CALL(cli_tool_run, ContainersCtx, .variable = "ORIOR_DOCKER", .program = "docker", .request = arguments,
                    .count = 4u, .ran = ran, .err_msg = args->err_msg))
    {
        EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
        return 0;
    }
    if (ran->code != 0ll)
    {
        // the first line of what it wrote and then printed, an empty one where it starts with a newline
        if ((ran->out_size == 0ull) && (ran->err_size == 0ull))
        {
            mmgr_line_put(args->err_msg, "docker logs failed");
            EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
            return 0;
        }
        // the output's first line, and the error's first after it where the output's has no newline
        const CliTextLine out_line =
            EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = ran->out, .at = 0ull, .size = ran->out_size);
        const CliTextLine err_line =
            EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = ran->err, .at = 0ull, .size = ran->err_size);
        EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = args->err_msg,
                   .text_bytes = (const unsigned char *)ran->out, .length = out_line.cut);
        EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = args->err_msg,
                   .text_bytes = (const unsigned char *)ran->err,
                   .length = (out_line.stop < ran->out_size) ? 0ull : err_line.cut);
        EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
        return 0;
    }
    mmgr_line_ch(args->out, '"');
    EMBED_CALL(cli_json_tree_length_out.json_escaped_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
               .text_bytes = (const unsigned char *)ran->out, .length = ran->out_size);
    EMBED_CALL(cli_json_tree_length_out.json_escaped_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
               .text_bytes = (const unsigned char *)ran->err, .length = ran->err_size);
    mmgr_line_ch(args->out, '"');
    EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
    return 1;
}

int cli_containers_run_in(const CliContainersOutLineCfg *args)
{
    const char *const arguments[5] = {"exec", args->name, "sh", "-c", args->line};
    CliRan *const ran = cli_ran_take();
    if (!EMBED_CALL(cli_tool_run, ContainersCtx, .variable = "ORIOR_DOCKER", .program = "docker", .request = arguments,
                    .count = 5u, .ran = ran, .err_msg = args->err_msg))
    {
        EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
        return 0;
    }
    // a run a signal ended gives what it wrote as why it failed
    if (ran->code < 0ll)
    {
        EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = args->err_msg,
                   .text_bytes = (const unsigned char *)ran->out, .length = ran->out_size);
        EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = args->err_msg,
                   .text_bytes = (const unsigned char *)ran->err, .length = ran->err_size);
        EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
        return 0;
    }
    mmgr_line_ch(args->out, '"');
    EMBED_CALL(cli_json_tree_length_out.json_escaped_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
               .text_bytes = (const unsigned char *)ran->out, .length = ran->out_size);
    EMBED_CALL(cli_json_tree_length_out.json_escaped_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
               .text_bytes = (const unsigned char *)ran->err, .length = ran->err_size);
    if (ran->code != 0ll)
    {
        mmgr_line_put(args->out, "\\nexit code ");
        mmgr_line_decimal_signed(args->out, ran->code);
    }
    mmgr_line_ch(args->out, '"');
    EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
    return 1;
}
