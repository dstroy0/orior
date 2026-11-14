// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// devcontainer_up.c: the words of each docker command a plan needs, docker run with them, and the tree's dev container
// brought up
#include "../command_line/callback_cfg.h"
#include "../text/text.h"
#include "devcontainer.h"

// the lock acquired while docker's words and output are stored

// how a container is stays running, in place of the image's own command: a POSIX shell's line, ended at once by docker
// stop
#define CLI_CONTAINER_IDLE_SCRIPT "trap 'exit 0' TERM; while sleep 1 & wait $!; do :; done"

// the max bytes of what docker writes that are stored, and the longest word joined from two
#define CLI_DOCKER_OUTPUT (1u << 16u)

#define CLI_STRING_JOINED 16384u

typedef struct
{
    CliWords *words_out;
    const char *first;
    const char *second;
    const CliWords *words;
    const char **line;
    VerbaLinea *err_msg;
    QuerySink sink;
    char *out;
    const CliPlan *plan;
    const char *container;
    const CliPlanCommand *commands;
    unsigned int count;
    char *address;
} DevcontainerUpCtx;
// `first` and `second` joined into one word put after the words so far
EMBED_INLINE void cli_words_join(const DevcontainerUpCtx *args)
{
    CliWords *words = args->words_out;
    char word_text[CLI_STRING_JOINED];
    VerbaLinea line = {word_text, sizeof(word_text), 0ULL};
    mmgr_line_put(&line, args->first);
    mmgr_line_put(&line, args->second);
    words->full = words->full || !mmgr_line_ok(&line);
    mmgr_line_finish(&line);
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = words,
               .word_text = word_text);
}

void cli_plan_address(const CliDevcontainerPlanContainerCfg *args)
{
    VerbaLinea line = {args->address, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, "docker:");
    mmgr_line_put(&line, args->container);
    mmgr_line_ch(&line, ':');
    mmgr_line_put(&line, EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg, .plan = args->plan,
                                    .at = args->plan->folder));
    mmgr_line_finish(&line);
}

int cli_plan_build_words(const CliDevcontainerPlanWordsCfg *args)
{
    if (args->plan->source != CLI_SOURCE_BUILD)
    {
        return 0;
    }
    VerbaLinea line = {args->image, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg, .plan = args->plan,
                                    .at = args->plan->name));
    mmgr_line_put(&line, "-image");
    mmgr_line_finish(&line);
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
               .word_text = "build");
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
               .word_text = "-f");
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
               .word_text = EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg, .plan = args->plan,
                                       .at = args->plan->dockerfile));
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
               .word_text = "-t");
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
               .word_text = args->image);
    for (unsigned int at = 0u; (at + 1u) < args->plan->args.count; at += 2u)
    {
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
                   .word_text = "--build-arg");
        char pair[CLI_STRING_JOINED];
        VerbaLinea joined = {pair, sizeof(pair), 0ULL};
        mmgr_line_put(&joined, EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg,
                                          .plan = args->plan, .at = args->plan->args.at[at]));
        mmgr_line_ch(&joined, '=');
        mmgr_line_put(&joined, EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg,
                                          .plan = args->plan, .at = args->plan->args.at[at + 1u]));
        args->words->full = args->words->full || !mmgr_line_ok(&joined);
        mmgr_line_finish(&joined);
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
                   .word_text = pair);
    }
    if (args->plan->targeted)
    {
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
                   .word_text = "--target");
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
                   .word_text = EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg,
                                           .plan = args->plan, .at = args->plan->target));
    }
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
               .word_text = EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg, .plan = args->plan,
                                       .at = args->plan->context));
    return 1;
}

void cli_plan_run_words(const CliDevcontainerPlanWordsImageCfg *args)
{
    const char *const root = EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg,
                                        .plan = args->plan, .at = args->plan->root);
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
               .word_text = "run");
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
               .word_text = "-d");
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
               .word_text = "--name");
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
               .word_text = EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg, .plan = args->plan,
                                       .at = args->plan->name));
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
               .word_text = "--label");
    EMBED_CALL(cli_words_join, DevcontainerUpCtx, .words_out = args->words, .first = "orior.tree=", .second = root);
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
               .word_text = "--mount");
    char mount[CLI_STRING_JOINED];
    VerbaLinea line = {mount, sizeof(mount), 0ULL};
    mmgr_line_put(&line, "type=bind,source=");
    mmgr_line_put(&line, root);
    mmgr_line_put(&line, ",target=");
    mmgr_line_put(&line, EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg, .plan = args->plan,
                                    .at = args->plan->folder));
    args->words->full = args->words->full || !mmgr_line_ok(&line);
    mmgr_line_finish(&line);
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
               .word_text = mount);
    for (unsigned int at = 0u; (at + 1u) < args->plan->env.count; at += 2u)
    {
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
                   .word_text = "-e");
        char pair[CLI_STRING_JOINED];
        VerbaLinea joined = {pair, sizeof(pair), 0ULL};
        mmgr_line_put(&joined, EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg,
                                          .plan = args->plan, .at = args->plan->env.at[at]));
        mmgr_line_ch(&joined, '=');
        mmgr_line_put(&joined, EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg,
                                          .plan = args->plan, .at = args->plan->env.at[at + 1u]));
        args->words->full = args->words->full || !mmgr_line_ok(&joined);
        mmgr_line_finish(&joined);
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
                   .word_text = pair);
    }
    for (unsigned int at = 0u; at < args->plan->mounts.count; at += 1u)
    {
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
                   .word_text = "--mount");
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
                   .word_text = EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg,
                                           .plan = args->plan, .at = args->plan->mounts.at[at]));
    }
    for (unsigned int at = 0u; at < args->plan->run_args.count; at += 1u)
    {
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
                   .word_text = EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg,
                                           .plan = args->plan, .at = args->plan->run_args.at[at]));
    }
    if (args->plan->has_user)
    {
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
                   .word_text = "-u");
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
                   .word_text = EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg,
                                           .plan = args->plan, .at = args->plan->user));
    }
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
               .word_text = "--entrypoint");
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
               .word_text = "/bin/sh");
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
               .word_text = args->image_read);
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
               .word_text = "-c");
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
               .word_text = CLI_CONTAINER_IDLE_SCRIPT);
}

int cli_plan_compose_words(const CliDevcontainerPlanWordsCfg *args)
{
    if (args->plan->source != CLI_SOURCE_COMPOSE)
    {
        return 0;
    }
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
               .word_text = "compose");
    for (unsigned int at = 0u; at < args->plan->files.count; at += 1u)
    {
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
                   .word_text = "-f");
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
                   .word_text = EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg,
                                           .plan = args->plan, .at = args->plan->files.at[at]));
    }
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
               .word_text = "-p");
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
               .word_text = EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg, .plan = args->plan,
                                       .at = args->plan->name));
    return 1;
}

void cli_plan_exec_words(const CliDevcontainerPlanCfg *args)
{
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
               .word_text = "exec");
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
               .word_text = "-w");
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
               .word_text = EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg, .plan = args->plan,
                                       .at = args->plan->folder));
    if (args->plan->has_user)
    {
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
                   .word_text = "-u");
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
                   .word_text = EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg,
                                           .plan = args->plan, .at = args->plan->user));
    }
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
               .word_text = args->container);
    if (args->command->line)
    {
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
                   .word_text = "/bin/sh");
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
                   .word_text = "-c");
    }
    for (unsigned int at = 0u; at < args->command->count; at += 1u)
    {
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
                   .word_text = EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg,
                                           .plan = args->plan, .at = args->plan->words.at[args->command->first + at]));
    }
}

// the words docker is run with, `docker` before them, into `line` with a NULL after the last
EMBED_INLINE int cli_docker_line(const DevcontainerUpCtx *args)
{
    if (args->words->full)
    {
        mmgr_line_put(args->err_msg, "a docker command contains more words than a plan stores");
        return 0;
    }
    args->line[0] = "docker";
    for (unsigned int at = 0u; at <= args->words->count; at += 1u)
    {
        args->line[at + 1u] = args->words->word_text[at];
    }
    return 1;
}

// docker run with `words`, its output going where this program's goes, after the line it runs is printed to `sink`: 1,
// or 0 with why given
static int cli_docker(const DevcontainerUpCtx *args)
{
    const CliWords *words = args->words;
    const char *line[CLI_WORDS_MAX + 2u] = {NULL};
    if (!EMBED_CALL(cli_docker_line, DevcontainerUpCtx, .words = words, .line = line, .err_msg = args->err_msg))
    {
        return 0;
    }
    char displayed[CLI_WORDS_TEXT + CLI_WORDS_MAX];
    VerbaLinea displayed_line = {displayed, sizeof(displayed), 0ULL};
    for (unsigned int at = 0u; line[at] != NULL; at += 1u)
    {
        if (at != 0u)
        {
            mmgr_line_ch(&displayed_line, ' ');
        }
        mmgr_line_put(&displayed_line, line[at]);
    }
    const unsigned long long length = mmgr_line_finish(&displayed_line);
    EMBED_CALL(args->sink.print, CliEachLineSizeCfg, .context = args->sink.context, .line = displayed, .size = length);
    QueryProcess process;
    if (!EMBED_CALL(query_started_internal.process_start, QueryStartedCtx, .started = &process, .words = line,
                    .folder = NULL, .output = QUERY_OUTPUT_OWN))
    {
        mmgr_line_put(args->err_msg, "docker: it could not be started");
        return 0;
    }
    const long long code = EMBED_CALL(query_process_internal.process_wait, QueryProcessCtx, .process_out = &process);
    if (code == 0LL)
    {
        return 1;
    }
    mmgr_line_put(args->err_msg, "docker ");
    mmgr_line_put(args->err_msg, (words->count != 0u) ? words->word_text[0] : "");
    mmgr_line_put(args->err_msg, " ended with ");
    if (code < 0LL)
    {
        mmgr_line_put(args->err_msg, "a signal");
        return 0;
    }
    mmgr_line_put(args->err_msg, "exit code ");
    mmgr_line_decimal_signed(args->err_msg, code);
    return 0;
}

// docker run with `words`, what it wrote to its output stored, trimmed, in `out`, sized for CLI_DOCKER_OUTPUT: 1, or
// 0 where it failed
static int cli_docker_read(const DevcontainerUpCtx *args)
{
    const CliWords *words = args->words;
    const char *line[CLI_WORDS_MAX + 2u];
    char err_buf[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {err_buf, sizeof(err_buf), 0ULL};
    QueryProcess process;
    if (!EMBED_CALL(cli_docker_line, DevcontainerUpCtx, .words = words, .line = line, .err_msg = &err_msg) ||
        !EMBED_CALL(query_started_internal.process_start, QueryStartedCtx, .started = &process, .words = line,
                    .folder = NULL, .output = QUERY_OUTPUT_PIPED_ALONE))
    {
        return 0;
    }
    unsigned long long out_len = 0ULL;
    char piece[4096];
    unsigned long long got = EMBED_CALL(query_process_piece_internal.process_read, QueryProcessPieceCtx,
                                        .process = &process, .piece = piece, .capacity_u64 = sizeof(piece));
    while (got != 0ULL)
    {
        const unsigned long long capacity = (CLI_DOCKER_OUTPUT - 1u) - out_len;
        const unsigned long long reserved = (got < capacity) ? got : capacity;
        memor.cpy(&(MemoriaCfg){.dst = args->out + out_len, .src = piece, .bytes = reserved});
        out_len += reserved;
        got = EMBED_CALL(query_process_piece_internal.process_read, QueryProcessPieceCtx, .process = &process,
                         .piece = piece, .capacity_u64 = sizeof(piece));
    }
    args->out[out_len] = '\0';
    if (EMBED_CALL(query_process_internal.process_wait, QueryProcessCtx, .process_out = &process) != 0LL)
    {
        return 0;
    }
    unsigned long long from = 0ULL;
    from = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = args->out, .at = from,
                      .size = out_len, .set = CLI_TEXT_WHITE);
    out_len = (from + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = args->out + from,
                                 .at = out_len - from, .set = CLI_TEXT_WHITE));
    // moved down to the start, each byte from above where it goes
    for (unsigned long long at = from; at < out_len; at += 1ULL)
    {
        args->out[at - from] = args->out[at];
    }
    args->out[out_len - from] = '\0';
    return 1;
}

// each command of `commands` run in `container`: 1, or 0 with why given
EMBED_INLINE int cli_commands_run(const DevcontainerUpCtx *args)
{
    const CliPlan *plan = args->plan;
    const char *container = args->container;
    static CliWords s_words;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
        EMBED_CALL(cli_devcontainer_plan_table.plan_exec_words, CliDevcontainerPlanCfg, .plan = plan,
                   .container = container, .command = &args->commands[at], .words = &s_words);
        if (!EMBED_CALL(cli_docker, DevcontainerUpCtx, .words = &s_words, .sink = args->sink, .err_msg = args->err_msg))
        {
            return 0;
        }
    }
    return 1;
}

// the compose project's service brought up: its containers made or started, the description's commands run in it as
// they were made or started; the address a window opens the tree at into `address`: 1, or 0 with why given
EMBED_INLINE int cli_compose_up(const DevcontainerUpCtx *args)
{
    const CliPlan *plan = args->plan;
    char *address = args->address;
    static CliWords s_words;
    static char s_output[CLI_DOCKER_OUTPUT];
    const char *const service =
        EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg, .plan = plan, .at = plan->service);
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    EMBED_CALL(cli_devcontainer_plan_words.plan_compose_words, CliDevcontainerPlanWordsCfg, .plan = plan,
               .words = &s_words);
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = "ps");
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = "-a");
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = "-q");
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = service);
    const int made =
        EMBED_CALL(cli_docker_read, DevcontainerUpCtx, .words = &s_words, .out = s_output) && (s_output[0] != '\0');
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    EMBED_CALL(cli_devcontainer_plan_words.plan_compose_words, CliDevcontainerPlanWordsCfg, .plan = plan,
               .words = &s_words);
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = "up");
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = "-d");
    int named = plan->services.count == 0u;
    for (unsigned int at = 0u; at < plan->services.count; at += 1u)
    {
        const char *const each = EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg, .plan = plan,
                                            .at = plan->services.at[at]);
        named = named || (cellul.eq(&(CatenaFinitaCfg){.src = each, .other = service, .cap = CLI_POOL_BYTES}));
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
                   .word_text = each);
    }
    if (!named)
    {
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
                   .word_text = service);
    }
    if (!EMBED_CALL(cli_docker, DevcontainerUpCtx, .words = &s_words, .sink = args->sink, .err_msg = args->err_msg))
    {
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    EMBED_CALL(cli_devcontainer_plan_words.plan_compose_words, CliDevcontainerPlanWordsCfg, .plan = plan,
               .words = &s_words);
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = "ps");
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = "-q");
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = service);
    if (!EMBED_CALL(cli_docker_read, DevcontainerUpCtx, .words = &s_words, .out = s_output) || (s_output[0] == '\0'))
    {
        mmgr_line_put(args->err_msg, "docker compose started no ");
        mmgr_line_put(args->err_msg, service);
        return 0;
    }
    // the first line of what docker wrote, its end left off
    char container[CLI_PATH_MAX_LEN];
    unsigned long long length = 0ULL;
    length = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = s_output, .at = 0ULL,
                        .size = cellul.len(&(CatenaFinitaCfg){.src = s_output, .cap = CLI_POOL_BYTES}), .set = "\n");
    length = (length < sizeof(container)) ? length : (sizeof(container) - 1u);
    memor.cpy(&(MemoriaCfg){.dst = container, .src = s_output, .bytes = (embed_word)length});
    length -= ((length != 0ULL) && (container[length - 1u] == '\r')) ? 1ULL : 0ULL;
    container[length] = '\0';
    const int run = made ? EMBED_CALL(cli_commands_run, DevcontainerUpCtx, .plan = plan, .container = container,
                                      .commands = plan->on_start, .count = plan->on_starts, .sink = args->sink,
                                      .err_msg = args->err_msg)
                         : EMBED_CALL(cli_commands_run, DevcontainerUpCtx, .plan = plan, .container = container,
                                      .commands = plan->on_create, .count = plan->on_creates, .sink = args->sink,
                                      .err_msg = args->err_msg);
    if (!run)
    {
        return 0;
    }
    EMBED_CALL(cli_devcontainer_plan_container.plan_address, CliDevcontainerPlanContainerCfg, .plan = plan,
               .container = container, .address = address);
    return 1;
}

EMBED_INLINE int cli_up_lock(const DevcontainerUpCtx *args)
{
    const CliPlan *plan = args->plan;
    char *address = args->address;
    static CliWords s_words;
    static char s_output[CLI_DOCKER_OUTPUT];
    if (plan->source == CLI_SOURCE_COMPOSE)
    {
        return EMBED_CALL(cli_compose_up, DevcontainerUpCtx, .plan = plan, .sink = args->sink, .address = address,
                          .err_msg = args->err_msg);
    }
    const char *const name =
        EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg, .plan = plan, .at = plan->name);
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = "inspect");
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = "-f");
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = "{{.State.Running}}");
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = name);
    if (EMBED_CALL(cli_docker_read, DevcontainerUpCtx, .words = &s_words, .out = s_output))
    {
        if (!cellul.eq(&(CatenaFinitaCfg){.src = s_output, .other = "true", .cap = CLI_POOL_BYTES}))
        {
            memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
            EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
                       .word_text = "start");
            EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
                       .word_text = name);
            if (!EMBED_CALL(cli_docker, DevcontainerUpCtx, .words = &s_words, .sink = args->sink,
                            .err_msg = args->err_msg) ||
                !EMBED_CALL(cli_commands_run, DevcontainerUpCtx, .plan = plan, .container = name,
                            .commands = plan->on_start, .count = plan->on_starts, .sink = args->sink,
                            .err_msg = args->err_msg))
            {
                return 0;
            }
        }
        EMBED_CALL(cli_devcontainer_plan_container.plan_address, CliDevcontainerPlanContainerCfg, .plan = plan,
                   .container = name, .address = address);
        return 1;
    }
    char image[CLI_PATH_MAX_LEN];
    if (plan->source == CLI_SOURCE_IMAGE)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = image,
                          .src = EMBED_CALL(cli_devcontainer_plan_at.plan_text, CliDevcontainerPlanAtCfg, .plan = plan,
                                            .at = plan->image),
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_devcontainer_plan_at.plan_text,
                                                                                   CliDevcontainerPlanAtCfg,
                                                                                   .plan = plan, .at = plan->image),
                                                                 .cap = CLI_POOL_BYTES}) +
                                   1u});
    }
    else
    {
        memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
        if (!EMBED_CALL(cli_devcontainer_plan_words.plan_build_words, CliDevcontainerPlanWordsCfg, .plan = plan,
                        .words = &s_words, .image = image))
        {
            mmgr_line_put(args->err_msg, "the description names no image");
            return 0;
        }
        if (!EMBED_CALL(cli_docker, DevcontainerUpCtx, .words = &s_words, .sink = args->sink, .err_msg = args->err_msg))
        {
            return 0;
        }
    }
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    EMBED_CALL(cli_devcontainer_plan_words_image.plan_run_words, CliDevcontainerPlanWordsImageCfg, .plan = plan,
               .image_read = image, .words = &s_words);
    if (!EMBED_CALL(cli_docker, DevcontainerUpCtx, .words = &s_words, .sink = args->sink, .err_msg = args->err_msg) ||
        !EMBED_CALL(cli_commands_run, DevcontainerUpCtx, .plan = plan, .container = name, .commands = plan->on_create,
                    .count = plan->on_creates, .sink = args->sink, .err_msg = args->err_msg) ||
        !EMBED_CALL(cli_commands_run, DevcontainerUpCtx, .plan = plan, .container = name, .commands = plan->on_start,
                    .count = plan->on_starts, .sink = args->sink, .err_msg = args->err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_devcontainer_plan_container.plan_address, CliDevcontainerPlanContainerCfg, .plan = plan,
               .container = name, .address = address);
    return 1;
}

int cli_devcontainer_up(const CliDevcontainerErrMsgCfg *args)
{
    const int done = EMBED_CALL(cli_up_lock, DevcontainerUpCtx, .plan = args->plan, .sink = args->sink,
                                .address = args->address, .err_msg = args->err_msg);
    return done;
}
