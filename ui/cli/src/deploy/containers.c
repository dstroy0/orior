// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// containers.c: docker and kubectl run, and docker's list, logs and acts, and what docker writes read
#include "../command_line/cells.h"
#include "../runs/runner.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "containers_internal.h"

// docker or kubectl, as `variable` names it or as `program`, run with `args`: 1 with what it gave in `ran`, or 0 with
// why it did not start said
static int cli_tool_ran(const char *variable, const char *program, const char *const *args, unsigned int count,
                        CliRan *ran, CliLine *said)
{
    static CliWords s_words;
    static CliLock g_cli_containers_lock = CLI_LOCK_INIT;
    cli_lock_take(&g_cli_containers_lock);
    s_words.count = 0u;
    s_words.used = 0u;
    s_words.full = 0;
    if (!cli_runner_program_words(variable, &s_words))
    {
        s_words.count = 0u;
        s_words.used = 0u;
        cli_words_put(&s_words, program);
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_words_put(&s_words, args[at]);
    }
    s_words.word[s_words.count] = NULL;
    char why[CLI_NAME_LONGEST];
    CliLine unstarted = {why, sizeof(why), 0ull};
    const int started = !s_words.full && cli_ran(s_words.word, NULL, NULL, 0u, ran, &unstarted);
    cli_lock_give(&g_cli_containers_lock);
    if (!started)
    {
        cli_line_text(said, program);
        cli_line_text(said, " is not found: ");
        cli_line_counted(said, why, unstarted.at);
    }
    return started;
}

// the first line of `size` bytes that is not blank, trimmed, written as cli_lossy writes it: 1, or 0 where none is
static int cli_first_line(const char *text, unsigned long long size, CliLine *said)
{
    unsigned long long at = 0ull;
    while (at < size)
    {
        const char *const newline = memor.chr(&(MemoriaCfg){.src = text + at, .val = '\n', .bytes = size - at});
        // a newline found lies in the text, at or past `at`
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - text) : size;
        unsigned long long from = at;
        unsigned long long to = end;
        while ((from < to) && ((text[from] == ' ') || ((text[from] >= '\t') && (text[from] <= '\r'))))
        {
            from += 1ull;
        }
        while ((to > from) && ((text[to - 1ull] == ' ') || ((text[to - 1ull] >= '\t') && (text[to - 1ull] <= '\r'))))
        {
            to -= 1ull;
        }
        if (to > from)
        {
            cli_lossy(said, (const unsigned char *)text + from, to - from);
            return 1;
        }
        at = end + 1ull;
    }
    return 0;
}

int cli_containers_tool(const char *variable, const char *program, const char *const *args, unsigned int count,
                        CliRan *ran, CliLine *said)
{
    if (!cli_tool_ran(variable, program, args, count, ran, said))
    {
        return 0;
    }
    if (ran->code == 0ll)
    {
        return 1;
    }
    if (!cli_first_line(ran->err, ran->err_size, said))
    {
        cli_line_text(said, program);
        cli_line_character(said, ' ');
        cli_line_text(said, (count > 0u) ? args[0] : "");
        cli_line_text(said, " failed");
    }
    return 0;
}

CliContainers g_cli_containers;
static CliLock s_containers_lock = CLI_LOCK_INIT;

int cli_containers_held(CliLine *out, unsigned int most, unsigned long long text_most)
{
    cli_lock_take(&s_containers_lock);
    const unsigned long long items = (unsigned long long)most * sizeof(CliContainer);
    const unsigned long long orders = (unsigned long long)most * sizeof(unsigned int);
    char *const cell = cli_cell_taken(items + orders + text_most + 1ull, "a listing of containers");
    if (cell == NULL)
    {
        cli_lock_give(&s_containers_lock);
        static const char s_untaken[] = "a listing of containers could not be taken";
        cli_line_text(out, "{\"items\":[");
        cli_containers_problem_written(out, 0, s_untaken, sizeof(s_untaken) - 1u);
        return 0;
    }
    // the items open the cell, on its boundary, the order after them on an unsigned int's, the texts last
    g_cli_containers = (CliContainers){(CliContainer *)(void *)cell,
                                       (unsigned int *)(void *)(cell + items),
                                       0u,
                                       most,
                                       cell + items + orders,
                                       0ull,
                                       text_most + 1ull};
    return 1;
}

void cli_containers_released(void)
{
    cli_cell_given(g_cli_containers.item);
    g_cli_containers = (CliContainers){NULL, NULL, 0u, 0u, NULL, 0ull, 0ull};
    cli_lock_give(&s_containers_lock);
}

// `length` bytes put in the texts, a 0 after them: their place, 0 where they do not fit, an empty text there
static unsigned int cli_kept(const char *from, unsigned long long length)
{
    if ((g_cli_containers.used + length + 1ull) > g_cli_containers.text_most)
    {
        return 0u;
    }
    // the texts are what one listing wrote, which an unsigned int's places reach
    const unsigned int at = (unsigned int)g_cli_containers.used;
    memor.cpy(&(MemoriaCfg){.dst = g_cli_containers.text + at, .src = from, .bytes = length});
    g_cli_containers.text[at + length] = '\0';
    g_cli_containers.used += length + 1ull;
    return at;
}

// the string member `key` of an object kept, an empty text where it is none
static unsigned int cli_member_kept(const CliJson *json, unsigned int object, const char *key)
{
    unsigned long long length = 0ull;
    const char *const text = cli_json_string(json, cli_json_member(json, object, key), &length);
    return (text != NULL) ? cli_kept(text, length) : cli_kept("", 0ull);
}

// the value of the label `key` among `labels`, `key=value` parted by commas: 1 with it kept into `at`, or 0
static int cli_label(const char *labels, unsigned long long size, const char *key, unsigned int *at)
{
    const unsigned long long key_length = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL});
    unsigned long long from = 0ull;
    while (from <= size)
    {
        const char *const comma = memor.chr(&(MemoriaCfg){.src = labels + from, .val = ',', .bytes = size - from});
        // a comma found lies in the labels, at or past `from`
        const unsigned long long end = (comma != NULL) ? (unsigned long long)(comma - labels) : size;
        const char *const equals = memor.chr(&(MemoriaCfg){.src = labels + from, .val = '=', .bytes = end - from});
        if ((equals != NULL) && ((unsigned long long)(equals - (labels + from)) == key_length) &&
            (memor.cmp(&(MemoriaCfg){.src = labels + from, .other = key, .bytes = key_length}) == 0))
        {
            // the value runs from past the equals sign to the comma
            *at = cli_kept(equals + 1, (unsigned long long)((labels + end) - (equals + 1)));
            return 1;
        }
        from = end + 1ull;
    }
    return 0;
}

// a line of `docker ps --format '{{json .}}'` read as a container listed
static void cli_container_read(const char *line, unsigned long long size)
{
    CliJson json;
    char why[CLI_NAME_LONGEST];
    CliLine unread = {why, sizeof(why), 0ull};
    const unsigned int one = cli_json_taken(&json, line, size, 0u, 0ull, &unread);
    if ((one == CLI_JSON_NONE) || (g_cli_containers.count >= g_cli_containers.most))
    {
        cli_json_given(&json);
        return;
    }
    unsigned long long labels_size = 0ull;
    const char *labels = cli_json_string(&json, cli_json_member(&json, one, "Labels"), &labels_size);
    labels = (labels != NULL) ? labels : "";
    CliContainer *const found = &g_cli_containers.item[g_cli_containers.count];
    found->id = cli_member_kept(&json, one, "ID");
    found->name = cli_member_kept(&json, one, "Names");
    found->image = cli_member_kept(&json, one, "Image");
    found->state = cli_member_kept(&json, one, "State");
    found->status = cli_member_kept(&json, one, "Status");
    found->ports = cli_member_kept(&json, one, "Ports");
    found->has_project = cli_label(labels, labels_size, "com.docker.compose.project", &found->project);
    found->has_service = cli_label(labels, labels_size, "com.docker.compose.service", &found->service);
    g_cli_containers.count += 1u;
    cli_json_given(&json);
}

// how an optional text sorts against another: none first
static int cli_optional_order(int has_one, unsigned int one, int has_other, unsigned int other)
{
    if (has_one != has_other)
    {
        return has_one ? 1 : -1;
    }
    return has_one ? cli_text_order(g_cli_containers.text + one, g_cli_containers.text + other) : 0;
}

// whether a container comes before another: by project, then service, then name
static int cli_container_before(const CliContainer *one, const CliContainer *other)
{
    int order = cli_optional_order(one->has_project, one->project, other->has_project, other->project);
    order =
        (order != 0) ? order : cli_optional_order(one->has_service, one->service, other->has_service, other->service);
    order =
        (order != 0) ? order : cli_text_order(g_cli_containers.text + one->name, g_cli_containers.text + other->name);
    return order < 0;
}

// an optional text written: the text, or null
static void cli_optional_written(CliLine *out, int has, unsigned int at)
{
    if (has)
    {
        cli_json_quoted(out, g_cli_containers.text + at,
                        cellul.len(&(CatenaFinitaCfg){.src = g_cli_containers.text + at, .cap = CLI_CELLS_POOL}));
    }
    else
    {
        cli_line_text(out, "null");
    }
}

// a member of a container written, a comma before it
static void cli_field_written(CliLine *out, const char *key, unsigned int at)
{
    cli_line_text(out, key);
    cli_json_quoted(out, g_cli_containers.text + at,
                    cellul.len(&(CatenaFinitaCfg){.src = g_cli_containers.text + at, .cap = CLI_CELLS_POOL}));
}

void cli_containers_problem_written(CliLine *out, int listed, const char *problem, unsigned long long length)
{
    cli_line_text(out, "],\"problem\":");
    if (listed)
    {
        cli_line_text(out, "null}");
        return;
    }
    cli_json_quoted(out, problem, length);
    cli_line_character(out, '}');
}

void cli_containers_list(CliLine *out)
{
    static const char *const s_args[5] = {"ps", "-a", "--no-trunc", "--format", "{{json .}}"};
    char problem[CLI_NAME_LONGEST * 4u];
    CliLine said = {problem, sizeof(problem), 0ull};
    CliRan *const ran = cli_ran_taken();
    const int listed = cli_containers_tool("ORIOR_DOCKER", "docker", s_args, 5u, ran, &said);
    cli_containers_listed(ran->out, listed ? ran->out_size : 0ull, listed, problem, said.at, out);
    cli_ran_given(ran);
}

void cli_containers_listed(const char *text, unsigned long long size, int listed, const char *problem,
                           unsigned long long problem_size, CliLine *out)
{
    // a container a line, and the texts kept of a line no more than its bytes and a 0 after each of its eight
    unsigned int lines = (size != 0ull) ? 1u : 0u;
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        lines += (text[at] == '\n') ? 1u : 0u;
    }
    if (!cli_containers_held(out, lines, size + (8ull * lines)))
    {
        return;
    }
    for (unsigned long long at = 0ull; at < size;)
    {
        const char *const newline = memor.chr(&(MemoriaCfg){.src = text + at, .val = '\n', .bytes = size - at});
        // a newline found lies in the text, at or past `at`
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - text) : size;
        cli_container_read(text + at, end - at);
        at = end + 1ull;
    }
    for (unsigned int at = 0u; at < g_cli_containers.count; at += 1u)
    {
        unsigned int place = at;
        while ((place > 0u) && cli_container_before(&g_cli_containers.item[at],
                                                    &g_cli_containers.item[g_cli_containers.order[place - 1u]]))
        {
            g_cli_containers.order[place] = g_cli_containers.order[place - 1u];
            place -= 1u;
        }
        g_cli_containers.order[place] = at;
    }
    cli_line_text(out, "{\"items\":[");
    for (unsigned int at = 0u; at < g_cli_containers.count; at += 1u)
    {
        const CliContainer *const one = &g_cli_containers.item[g_cli_containers.order[at]];
        cli_field_written(out, (at != 0u) ? ",{\"id\":" : "{\"id\":", one->id);
        cli_field_written(out, ",\"name\":", one->name);
        cli_field_written(out, ",\"image\":", one->image);
        cli_field_written(out, ",\"state\":", one->state);
        cli_field_written(out, ",\"status\":", one->status);
        cli_line_text(out, ",\"project\":");
        cli_optional_written(out, one->has_project, one->project);
        cli_line_text(out, ",\"service\":");
        cli_optional_written(out, one->has_service, one->service);
        cli_field_written(out, ",\"ports\":", one->ports);
        cli_line_character(out, '}');
    }
    cli_containers_problem_written(out, listed, problem, problem_size);
    cli_containers_released();
}

int cli_containers_act(const char *name, const char *act, CliLine *said)
{
    if ((!cellul.eq(&(CatenaFinitaCfg){.src = act, .other = "start", .cap = CLI_CELLS_POOL})) &&
        (!cellul.eq(&(CatenaFinitaCfg){.src = act, .other = "stop", .cap = CLI_CELLS_POOL})) &&
        (!cellul.eq(&(CatenaFinitaCfg){.src = act, .other = "restart", .cap = CLI_CELLS_POOL})))
    {
        cli_line_text(said, act);
        cli_line_text(said, " is not something a container is");
        return 0;
    }
    const char *const args[2] = {act, name};
    CliRan *const ran = cli_ran_taken();
    const int acted = cli_containers_tool("ORIOR_DOCKER", "docker", args, 2u, ran, said);
    cli_ran_given(ran);
    return acted;
}

int cli_containers_logs(const char *name, CliLine *out, CliLine *said)
{
    char lines[16];
    CliLine count = {lines, sizeof(lines), 0ull};
    cli_line_decimal(&count, CLI_LOG_LINES, 0u);
    cli_line_closed(&count);
    const char *const args[4] = {"logs", "--tail", lines, name};
    CliRan *const ran = cli_ran_taken();
    if (!cli_tool_ran("ORIOR_DOCKER", "docker", args, 4u, ran, said))
    {
        cli_ran_given(ran);
        return 0;
    }
    if (ran->code != 0ll)
    {
        // the first line of what it wrote and then said, an empty one where it starts with a newline
        if ((ran->out_size == 0ull) && (ran->err_size == 0ull))
        {
            cli_line_text(said, "docker logs failed");
            cli_ran_given(ran);
            return 0;
        }
        const char *const out_newline = memor.chr(&(MemoriaCfg){.src = ran->out, .val = '\n', .bytes = ran->out_size});
        const char *const err_newline = memor.chr(&(MemoriaCfg){.src = ran->err, .val = '\n', .bytes = ran->err_size});
        // a newline found lies in its text, at or past its start
        unsigned long long out_length =
            (out_newline != NULL) ? (unsigned long long)(out_newline - ran->out) : ran->out_size;
        unsigned long long err_length = (out_newline != NULL)   ? 0ull
                                        : (err_newline != NULL) ? (unsigned long long)(err_newline - ran->err)
                                                                : ran->err_size;
        // a carriage return before the line's newline is no part of it
        const int ends_in_err = (out_newline == NULL) && (err_newline != NULL);
        if ((out_newline != NULL) && (out_length > 0ull) && (ran->out[out_length - 1ull] == '\r'))
        {
            out_length -= 1ull;
        }
        if (ends_in_err && (err_length > 0ull) && (ran->err[err_length - 1ull] == '\r'))
        {
            err_length -= 1ull;
        }
        cli_lossy(said, (const unsigned char *)ran->out, out_length);
        cli_lossy(said, (const unsigned char *)ran->err, err_length);
        cli_ran_given(ran);
        return 0;
    }
    cli_line_character(out, '"');
    cli_json_escaped_lossy(out, (const unsigned char *)ran->out, ran->out_size);
    cli_json_escaped_lossy(out, (const unsigned char *)ran->err, ran->err_size);
    cli_line_character(out, '"');
    cli_ran_given(ran);
    return 1;
}

int cli_containers_run_in(const char *name, const char *line, CliLine *out, CliLine *said)
{
    const char *const args[5] = {"exec", name, "sh", "-c", line};
    CliRan *const ran = cli_ran_taken();
    if (!cli_tool_ran("ORIOR_DOCKER", "docker", args, 5u, ran, said))
    {
        cli_ran_given(ran);
        return 0;
    }
    // a run a signal ended gives what it wrote as why it failed
    if (ran->code < 0ll)
    {
        cli_lossy(said, (const unsigned char *)ran->out, ran->out_size);
        cli_lossy(said, (const unsigned char *)ran->err, ran->err_size);
        cli_ran_given(ran);
        return 0;
    }
    cli_line_character(out, '"');
    cli_json_escaped_lossy(out, (const unsigned char *)ran->out, ran->out_size);
    cli_json_escaped_lossy(out, (const unsigned char *)ran->err, ran->err_size);
    if (ran->code != 0ll)
    {
        cli_line_text(out, "\\nexit code ");
        cli_line_signed(out, ran->code);
    }
    cli_line_character(out, '"');
    cli_ran_given(ran);
    return 1;
}
