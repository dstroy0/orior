// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// devcontainer_up.c: the words of each docker command a plan needs, docker run with them, and the tree's dev container
// brought up
#include "devcontainer.h"

// the lock held while docker's words and output are kept
static CliLock s_lock = CLI_LOCK_INIT;

// how a container is kept running, in place of the image's own command: a POSIX shell's line, ended at once by docker
// stop
#define CLI_KEEP_RUNNING "trap 'exit 0' TERM; while sleep 1 & wait $!; do :; done"

// the most bytes of what docker writes that are kept, and the longest word joined from two
#define CLI_DOCKER_OUTPUT (1u << 16u)

#define CLI_STRING_JOINED 16384u

// `first` and `second` joined into one word put after the words so far
static void cli_words_joined(CliWords *words, const char *first, const char *second)
{
    char word[CLI_STRING_JOINED];
    CliLine line = {word, sizeof(word), 0ull};
    cli_line_text(&line, first);
    cli_line_text(&line, second);
    words->full = words->full || !cli_line_fits(&line);
    cli_line_closed(&line);
    cli_words_put(words, word);
}

void cli_plan_address(const CliPlan *plan, const char *container, char *address)
{
    CliLine line = {address, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, "docker:");
    cli_line_text(&line, container);
    cli_line_character(&line, ':');
    cli_line_text(&line, cli_plan_text(plan, plan->folder));
    cli_line_closed(&line);
}

int cli_plan_build_words(const CliPlan *plan, CliWords *words, char *image)
{
    if (plan->source != CLI_SOURCE_BUILD)
    {
        return 0;
    }
    CliLine line = {image, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, cli_plan_text(plan, plan->name));
    cli_line_text(&line, "-image");
    cli_line_closed(&line);
    cli_words_put(words, "build");
    cli_words_put(words, "-f");
    cli_words_put(words, cli_plan_text(plan, plan->dockerfile));
    cli_words_put(words, "-t");
    cli_words_put(words, image);
    for (unsigned int at = 0u; (at + 1u) < plan->args.count; at += 2u)
    {
        cli_words_put(words, "--build-arg");
        char pair[CLI_STRING_JOINED];
        CliLine joined = {pair, sizeof(pair), 0ull};
        cli_line_text(&joined, cli_plan_text(plan, plan->args.at[at]));
        cli_line_character(&joined, '=');
        cli_line_text(&joined, cli_plan_text(plan, plan->args.at[at + 1u]));
        words->full = words->full || !cli_line_fits(&joined);
        cli_line_closed(&joined);
        cli_words_put(words, pair);
    }
    if (plan->targeted)
    {
        cli_words_put(words, "--target");
        cli_words_put(words, cli_plan_text(plan, plan->target));
    }
    cli_words_put(words, cli_plan_text(plan, plan->context));
    return 1;
}

void cli_plan_run_words(const CliPlan *plan, const char *image, CliWords *words)
{
    const char *const root = cli_plan_text(plan, plan->root);
    cli_words_put(words, "run");
    cli_words_put(words, "-d");
    cli_words_put(words, "--name");
    cli_words_put(words, cli_plan_text(plan, plan->name));
    cli_words_put(words, "--label");
    cli_words_joined(words, "orior.tree=", root);
    cli_words_put(words, "--mount");
    char mount[CLI_STRING_JOINED];
    CliLine line = {mount, sizeof(mount), 0ull};
    cli_line_text(&line, "type=bind,source=");
    cli_line_text(&line, root);
    cli_line_text(&line, ",target=");
    cli_line_text(&line, cli_plan_text(plan, plan->folder));
    words->full = words->full || !cli_line_fits(&line);
    cli_line_closed(&line);
    cli_words_put(words, mount);
    for (unsigned int at = 0u; (at + 1u) < plan->env.count; at += 2u)
    {
        cli_words_put(words, "-e");
        char pair[CLI_STRING_JOINED];
        CliLine joined = {pair, sizeof(pair), 0ull};
        cli_line_text(&joined, cli_plan_text(plan, plan->env.at[at]));
        cli_line_character(&joined, '=');
        cli_line_text(&joined, cli_plan_text(plan, plan->env.at[at + 1u]));
        words->full = words->full || !cli_line_fits(&joined);
        cli_line_closed(&joined);
        cli_words_put(words, pair);
    }
    for (unsigned int at = 0u; at < plan->mounts.count; at += 1u)
    {
        cli_words_put(words, "--mount");
        cli_words_put(words, cli_plan_text(plan, plan->mounts.at[at]));
    }
    for (unsigned int at = 0u; at < plan->run_args.count; at += 1u)
    {
        cli_words_put(words, cli_plan_text(plan, plan->run_args.at[at]));
    }
    if (plan->has_user)
    {
        cli_words_put(words, "-u");
        cli_words_put(words, cli_plan_text(plan, plan->user));
    }
    cli_words_put(words, "--entrypoint");
    cli_words_put(words, "/bin/sh");
    cli_words_put(words, image);
    cli_words_put(words, "-c");
    cli_words_put(words, CLI_KEEP_RUNNING);
}

int cli_plan_compose_words(const CliPlan *plan, CliWords *words)
{
    if (plan->source != CLI_SOURCE_COMPOSE)
    {
        return 0;
    }
    cli_words_put(words, "compose");
    for (unsigned int at = 0u; at < plan->files.count; at += 1u)
    {
        cli_words_put(words, "-f");
        cli_words_put(words, cli_plan_text(plan, plan->files.at[at]));
    }
    cli_words_put(words, "-p");
    cli_words_put(words, cli_plan_text(plan, plan->name));
    return 1;
}

void cli_plan_exec_words(const CliPlan *plan, const char *container, const CliPlanCommand *command, CliWords *words)
{
    cli_words_put(words, "exec");
    cli_words_put(words, "-w");
    cli_words_put(words, cli_plan_text(plan, plan->folder));
    if (plan->has_user)
    {
        cli_words_put(words, "-u");
        cli_words_put(words, cli_plan_text(plan, plan->user));
    }
    cli_words_put(words, container);
    if (command->line)
    {
        cli_words_put(words, "/bin/sh");
        cli_words_put(words, "-c");
    }
    for (unsigned int at = 0u; at < command->count; at += 1u)
    {
        cli_words_put(words, cli_plan_text(plan, plan->words.at[command->first + at]));
    }
}

// the words docker is run with, `docker` before them, into `line` with a NULL after the last
static int cli_docker_line(const CliWords *words, const char **line, CliLine *said)
{
    if (words->full)
    {
        cli_line_text(said, "a docker command holds more words than a plan keeps");
        return 0;
    }
    line[0] = "docker";
    for (unsigned int at = 0u; at <= words->count; at += 1u)
    {
        line[at + 1u] = words->word[at];
    }
    return 1;
}

// docker run with `words`, its output going where this program's goes, after the line it runs is said to `say`: 1, or
// 0 with why said
static int cli_docker(const CliWords *words, QuerySink say, CliLine *said)
{
    const char *line[CLI_WORDS_MOST + 2u];
    if (!cli_docker_line(words, line, said))
    {
        return 0;
    }
    char shown[CLI_WORDS_TEXT + CLI_WORDS_MOST];
    CliLine shown_line = {shown, sizeof(shown), 0ull};
    for (unsigned int at = 0u; line[at] != NULL; at += 1u)
    {
        if (at != 0u)
        {
            cli_line_character(&shown_line, ' ');
        }
        cli_line_text(&shown_line, line[at]);
    }
    const unsigned long long length = cli_line_closed(&shown_line);
    say.said(say.context, shown, length);
    QueryProcess process;
    if (!query_process_start(&process, line, NULL, QUERY_OUTPUT_OWN))
    {
        cli_line_text(said, "docker: it could not be started");
        return 0;
    }
    const long long code = query_process_wait(&process);
    if (code == 0ll)
    {
        return 1;
    }
    cli_line_text(said, "docker ");
    cli_line_text(said, (words->count != 0u) ? words->word[0] : "");
    cli_line_text(said, " ended with ");
    if (code < 0ll)
    {
        cli_line_text(said, "a signal");
        return 0;
    }
    cli_line_text(said, "exit code ");
    cli_line_signed(said, code);
    return 0;
}

// docker run with `words`, what it wrote to its output kept, trimmed, in `out`, which holds CLI_DOCKER_OUTPUT: 1, or
// 0 where it failed
static int cli_docker_read(const CliWords *words, char *out)
{
    const char *line[CLI_WORDS_MOST + 2u];
    char unsaid[CLI_PATH_LONGEST];
    CliLine said = {unsaid, sizeof(unsaid), 0ull};
    QueryProcess process;
    if (!cli_docker_line(words, line, &said) || !query_process_start(&process, line, NULL, QUERY_OUTPUT_PIPED_ALONE))
    {
        return 0;
    }
    unsigned long long kept = 0ull;
    char piece[4096];
    unsigned long long got = query_process_read(&process, piece, sizeof(piece));
    while (got != 0ull)
    {
        const unsigned long long room = (CLI_DOCKER_OUTPUT - 1u) - kept;
        const unsigned long long taken = (got < room) ? got : room;
        memor.cpy(&(MemoriaCfg){.dst = out + kept, .src = piece, .bytes = taken});
        kept += taken;
        got = query_process_read(&process, piece, sizeof(piece));
    }
    out[kept] = '\0';
    if (query_process_wait(&process) != 0ll)
    {
        return 0;
    }
    unsigned long long from = 0ull;
    while ((from < kept) && ((out[from] == ' ') || ((out[from] >= '\t') && (out[from] <= '\r'))))
    {
        from += 1ull;
    }
    while ((kept > from) && ((out[kept - 1u] == ' ') || ((out[kept - 1u] >= '\t') && (out[kept - 1u] <= '\r'))))
    {
        kept -= 1ull;
    }
    // moved down to the start, each byte from above where it goes
    for (unsigned long long at = from; at < kept; at += 1ull)
    {
        out[at - from] = out[at];
    }
    out[kept - from] = '\0';
    return 1;
}

// each command of `commands` run in `container`: 1, or 0 with why said
static int cli_commands_run(const CliPlan *plan, const char *container, const CliPlanCommand *commands,
                            unsigned int count, QuerySink say, CliLine *said)
{
    static CliWords s_words;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
        cli_plan_exec_words(plan, container, &commands[at], &s_words);
        if (!cli_docker(&s_words, say, said))
        {
            return 0;
        }
    }
    return 1;
}

// the compose project's service brought up: its containers made or started, the description's commands run in it as
// they were made or started; the address a window opens the tree at into `address`: 1, or 0 with why said
static int cli_compose_up(const CliPlan *plan, QuerySink say, char *address, CliLine *said)
{
    static CliWords s_words;
    static char s_output[CLI_DOCKER_OUTPUT];
    const char *const service = cli_plan_text(plan, plan->service);
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    cli_plan_compose_words(plan, &s_words);
    cli_words_put(&s_words, "ps");
    cli_words_put(&s_words, "-a");
    cli_words_put(&s_words, "-q");
    cli_words_put(&s_words, service);
    const int made = cli_docker_read(&s_words, s_output) && (s_output[0] != '\0');
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    cli_plan_compose_words(plan, &s_words);
    cli_words_put(&s_words, "up");
    cli_words_put(&s_words, "-d");
    int named = plan->services.count == 0u;
    for (unsigned int at = 0u; at < plan->services.count; at += 1u)
    {
        const char *const each = cli_plan_text(plan, plan->services.at[at]);
        named = named || (cellul.eq(&(CatenaFinitaCfg){.src = each, .other = service, .cap = CLI_CELLS_POOL}));
        cli_words_put(&s_words, each);
    }
    if (!named)
    {
        cli_words_put(&s_words, service);
    }
    if (!cli_docker(&s_words, say, said))
    {
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    cli_plan_compose_words(plan, &s_words);
    cli_words_put(&s_words, "ps");
    cli_words_put(&s_words, "-q");
    cli_words_put(&s_words, service);
    if (!cli_docker_read(&s_words, s_output) || (s_output[0] == '\0'))
    {
        cli_line_text(said, "docker compose started no ");
        cli_line_text(said, service);
        return 0;
    }
    // the first line of what docker wrote, its end left off
    char container[CLI_PATH_LONGEST];
    unsigned long long length = 0ull;
    while ((s_output[length] != '\0') && (s_output[length] != '\n') && (length + 1ull < sizeof(container)))
    {
        container[length] = s_output[length];
        length += 1ull;
    }
    length -= ((length != 0ull) && (container[length - 1u] == '\r')) ? 1ull : 0ull;
    container[length] = '\0';
    const int run = made ? cli_commands_run(plan, container, plan->on_start, plan->on_starts, say, said)
                         : cli_commands_run(plan, container, plan->on_create, plan->on_creates, say, said);
    if (!run)
    {
        return 0;
    }
    cli_plan_address(plan, container, address);
    return 1;
}

static int cli_up_held(const CliPlan *plan, QuerySink say, char *address, CliLine *said)
{
    static CliWords s_words;
    static char s_output[CLI_DOCKER_OUTPUT];
    if (plan->source == CLI_SOURCE_COMPOSE)
    {
        return cli_compose_up(plan, say, address, said);
    }
    const char *const name = cli_plan_text(plan, plan->name);
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    cli_words_put(&s_words, "inspect");
    cli_words_put(&s_words, "-f");
    cli_words_put(&s_words, "{{.State.Running}}");
    cli_words_put(&s_words, name);
    if (cli_docker_read(&s_words, s_output))
    {
        if (!cellul.eq(&(CatenaFinitaCfg){.src = s_output, .other = "true", .cap = CLI_CELLS_POOL}))
        {
            memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
            cli_words_put(&s_words, "start");
            cli_words_put(&s_words, name);
            if (!cli_docker(&s_words, say, said) ||
                !cli_commands_run(plan, name, plan->on_start, plan->on_starts, say, said))
            {
                return 0;
            }
        }
        cli_plan_address(plan, name, address);
        return 1;
    }
    char image[CLI_PATH_LONGEST];
    if (plan->source == CLI_SOURCE_IMAGE)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = image,
            .src = cli_plan_text(plan, plan->image),
            .bytes =
                cellul.len(&(CatenaFinitaCfg){.src = cli_plan_text(plan, plan->image), .cap = CLI_CELLS_POOL}) + 1u});
    }
    else
    {
        memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
        if (!cli_plan_build_words(plan, &s_words, image))
        {
            cli_line_text(said, "the description names no image");
            return 0;
        }
        if (!cli_docker(&s_words, say, said))
        {
            return 0;
        }
    }
    memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
    cli_plan_run_words(plan, image, &s_words);
    if (!cli_docker(&s_words, say, said) ||
        !cli_commands_run(plan, name, plan->on_create, plan->on_creates, say, said) ||
        !cli_commands_run(plan, name, plan->on_start, plan->on_starts, say, said))
    {
        return 0;
    }
    cli_plan_address(plan, name, address);
    return 1;
}

int cli_devcontainer_up(const CliPlan *plan, QuerySink say, char *address, CliLine *said)
{
    cli_lock_take(&s_lock);
    const int done = cli_up_held(plan, say, address, said);
    cli_lock_give(&s_lock);
    return done;
}
