// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// containers.c: docker and kubectl run, and docker's list, logs and acts, and what docker writes read
#include "../command_line/pool.h"
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
    CliRunResult *run_result;
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

// docker or kubectl, as `variable` names it or as `program`, run with `args`: 1 with what it gave in `run_result`, or 0
// with why it did not start given
EMBED_INLINE int cli_tool_run(const ContainersCtx *args)
{
    const char *variable = args->variable;
    unsigned int count = args->count;
    CliRunResult *run_result = args->run_result;
    VerbaLinea *err_msg = args->err_msg;
    static CliWords s_words;
    s_words.count = 0u;
    s_words.used = 0u;
    s_words.full = 0;
    if (!EMBED_CALL(cli_runner_variable.program_words, CliRunnerVariableCfg, .variable = variable, .words = &s_words))
    {
        s_words.count = 0u;
        s_words.used = 0u;
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
                   .word_text = args->program);
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
                   .word_text = args->request[at]);
    }
    s_words.word_text[s_words.count] = NULL;
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unstarted = {why, sizeof(why), 0ULL};
    const int started =
        !s_words.full && EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = s_words.word_text, .folder = NULL,
                                    .settings = NULL, .count = 0u, .run_result = run_result, .err_msg = &unstarted);
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
    unsigned long long at = 0ULL;
    while (at < size)
    {
        const unsigned long long end =
            EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = text, .at = at, .size = size).stop;
        unsigned long long from = at;
        unsigned long long to = end;
        from = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = text, .at = from,
                          .size = to, .set = CLI_TEXT_WHITE);
        to = (from + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = text + from,
                                .at = to - from, .set = CLI_TEXT_WHITE));
        if (to > from)
        {
            EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = args->err_msg,
                       .text_bytes = (const unsigned char *)text + from, .length = to - from);
            return 1;
        }
        at = end + 1ULL;
    }
    return 0;
}

int cli_containers_tool(const CliContainersVariableCtx *args)
{
    if (!EMBED_CALL(cli_tool_run, ContainersCtx, .variable = args->variable, .program = args->program,
                    .request = args->request, .count = args->count, .run_result = args->run_result,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    if (args->run_result->code == 0LL)
    {
        return 1;
    }
    if (!EMBED_CALL(cli_first_line, ContainersCtx, .text = args->run_result->err, .size = args->run_result->err_size,
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

int cli_containers_act(const CliContainersCfg *args)
{
    if ((!cellul.eq(&(CatenaFinitaCfg){.src = args->act, .other = "start", .cap = CLI_POOL_BYTES})) &&
        (!cellul.eq(&(CatenaFinitaCfg){.src = args->act, .other = "stop", .cap = CLI_POOL_BYTES})) &&
        (!cellul.eq(&(CatenaFinitaCfg){.src = args->act, .other = "restart", .cap = CLI_POOL_BYTES})))
    {
        mmgr_line_put(args->err_msg, args->act);
        mmgr_line_put(args->err_msg, " is not something a container is");
        return 0;
    }
    const char *const arguments[2] = {args->act, args->name};
    CliRunResult *const run_result = cli_run_result_alloc();
    const int acted = EMBED_CALL(cli_containers_variable_internal.tool, CliContainersVariableCtx,
                                 .variable = "ORIOR_DOCKER", .program = "docker", .request = arguments, .count = 2u,
                                 .run_result = run_result, .err_msg = args->err_msg);
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    return acted;
}

int cli_containers_logs(const CliContainersOutLineCfg *args)
{
    char lines[16];
    VerbaLinea count = {lines, sizeof(lines), 0ULL};
    mmgr_line_decimal(&count, CLI_LOG_LINES, 0u);
    mmgr_line_finish(&count);
    const char *const arguments[4] = {"logs", "--tail", lines, args->name};
    CliRunResult *const run_result = cli_run_result_alloc();
    if (!EMBED_CALL(cli_tool_run, ContainersCtx, .variable = "ORIOR_DOCKER", .program = "docker", .request = arguments,
                    .count = 4u, .run_result = run_result, .err_msg = args->err_msg))
    {
        EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
        return 0;
    }
    if (run_result->code != 0LL)
    {
        // the first line of what it wrote and then printed, an empty one where it starts with a newline
        if ((run_result->out_size == 0ULL) && (run_result->err_size == 0ULL))
        {
            mmgr_line_put(args->err_msg, "docker logs failed");
            EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
            return 0;
        }
        // the output's first line, and the error's first after it where the output's has no newline
        const CliTextLine out_line = EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = run_result->out, .at = 0ULL,
                                                .size = run_result->out_size);
        const CliTextLine err_line = EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = run_result->err, .at = 0ULL,
                                                .size = run_result->err_size);
        EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = args->err_msg,
                   .text_bytes = (const unsigned char *)run_result->out, .length = out_line.cut);
        EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = args->err_msg,
                   .text_bytes = (const unsigned char *)run_result->err,
                   .length = (out_line.stop < run_result->out_size) ? 0ULL : err_line.cut);
        EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
        return 0;
    }
    mmgr_line_ch(args->out, '"');
    EMBED_CALL(cli_json_tree_length_out.json_escaped_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
               .text_bytes = (const unsigned char *)run_result->out, .length = run_result->out_size);
    EMBED_CALL(cli_json_tree_length_out.json_escaped_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
               .text_bytes = (const unsigned char *)run_result->err, .length = run_result->err_size);
    mmgr_line_ch(args->out, '"');
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    return 1;
}

int cli_containers_run_in(const CliContainersOutLineCfg *args)
{
    const char *const arguments[5] = {"exec", args->name, "sh", "-c", args->line};
    CliRunResult *const run_result = cli_run_result_alloc();
    if (!EMBED_CALL(cli_tool_run, ContainersCtx, .variable = "ORIOR_DOCKER", .program = "docker", .request = arguments,
                    .count = 5u, .run_result = run_result, .err_msg = args->err_msg))
    {
        EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
        return 0;
    }
    // a run a signal ended gives what it wrote as why it failed
    if (run_result->code < 0LL)
    {
        EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = args->err_msg,
                   .text_bytes = (const unsigned char *)run_result->out, .length = run_result->out_size);
        EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = args->err_msg,
                   .text_bytes = (const unsigned char *)run_result->err, .length = run_result->err_size);
        EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
        return 0;
    }
    mmgr_line_ch(args->out, '"');
    EMBED_CALL(cli_json_tree_length_out.json_escaped_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
               .text_bytes = (const unsigned char *)run_result->out, .length = run_result->out_size);
    EMBED_CALL(cli_json_tree_length_out.json_escaped_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
               .text_bytes = (const unsigned char *)run_result->err, .length = run_result->err_size);
    if (run_result->code != 0LL)
    {
        mmgr_line_put(args->out, "\\nexit code ");
        mmgr_line_decimal_signed(args->out, run_result->code);
    }
    mmgr_line_ch(args->out, '"');
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    return 1;
}
