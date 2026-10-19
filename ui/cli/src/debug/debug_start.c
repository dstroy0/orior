// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_start.c: what a session is started with, as its start says: its language, the adapter that debugs it, the
// program built where its language is built, and the request, launch or attach, with its arguments, its name and the
// session that started it
#include "debug_start.h"
#include "../tests/cargo_tests.h"
#include "../tests/test_runs.h"

// a string member `name` of the object `object` of `json` into `out`, which holds `room`: 1, or 0 with the member said
// missing where it is not a string
static int start_field(const CliJson *json, unsigned int object, const char *name, char *out, unsigned long long room,
                       CliLine *said)
{
    const char *const text = cli_json_string(json, cli_json_member(json, object, name), NULL);
    if (text == NULL)
    {
        cli_line_text(said, "missing field `");
        cli_line_text(said, name);
        cli_line_character(said, '`');
        return 0;
    }
    memor.cpy(&(MemoriaCfg){
        .dst = out, .src = text, .bytes = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}) + 1u});
    return 1;
}

// the language of the session `parent` into `language`: 1, or 0 where it is not listed
static int start_parent_language(unsigned long long parent, char *language)
{
    int found = 0;
    cli_lock_take(&g_cli_debug.lock);
    for (unsigned int at = 0u; at < CLI_DEBUG_SESSIONS; at += 1u)
    {
        const CliDebugSession *const session = &g_cli_debug.sessions[at];
        if (session->taken && session->listed && (session->info.id == parent))
        {
            memor.cpy(&(MemoriaCfg){
                .dst = language,
                .src = session->info.language,
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = session->info.language, .cap = CLI_CELLS_POOL}) + 1u});
            found = 1;
        }
    }
    cli_lock_give(&g_cli_debug.lock);
    return found;
}

// the language, the adapter, and for a C file of a CMake or Meson build orior configured the program that build makes
// from it, read into `plan`: 1, or 0 with why said
static int start_adapter(CliDebugSession *place, const char *root, const CliJson *json, unsigned int start,
                         CliDebugPlan *plan, CliLine *said)
{
    CliJson *const to = &place->start;
    char path[CLI_PATH_LONGEST];
    unsigned long long parent = 0ull;
    plan->has_made = 0;
    if ((cellul.eq(&(CatenaFinitaCfg){.src = plan->kind, .other = "file", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = plan->kind, .other = "process", .cap = CLI_CELLS_POOL})))
    {
        if (!start_field(json, start, "language", plan->language, sizeof(plan->language), said))
        {
            return 0;
        }
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = plan->kind, .other = "test", .cap = CLI_CELLS_POOL}))
    {
        if (!start_field(json, start, "id", plan->id, sizeof(plan->id), said))
        {
            return 0;
        }
        memor.cpy(
            &(MemoriaCfg){.dst = plan->language,
                          .src = cli_cargo_is_rust(plan->id) ? "rust" : "python",
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = cli_cargo_is_rust(plan->id) ? "rust" : "python",
                                                                 .cap = CLI_CELLS_POOL}) +
                                   1u});
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = plan->kind, .other = "child", .cap = CLI_CELLS_POOL}))
    {
        if (!cli_debug_unsigned(json, cli_json_member(json, start, "parent"), &parent))
        {
            cli_line_text(said, "missing field `parent`");
            return 0;
        }
        if (!start_parent_language(parent, plan->language))
        {
            cli_line_text(said, "that session has ended");
            return 0;
        }
    }
    else if ((cellul.eq(&(CatenaFinitaCfg){.src = plan->kind, .other = "address", .cap = CLI_CELLS_POOL})) ||
             (cellul.eq(&(CatenaFinitaCfg){.src = plan->kind, .other = "record", .cap = CLI_CELLS_POOL})))
    {
        memor.cpy(&(MemoriaCfg){.dst = plan->language,
                                .src = "python",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = "python", .cap = CLI_CELLS_POOL}) + 1u});
    }
    else
    {
        cli_line_text(said, "unknown variant `");
        cli_line_text(said, plan->kind);
        cli_line_text(said, "`, expected one of `file`, `test`, `process`, `address`, `child`, `record`");
        return 0;
    }
    plan->recording = (cellul.eq(&(CatenaFinitaCfg){.src = plan->kind, .other = "record", .cap = CLI_CELLS_POOL}));
    if ((cellul.eq(&(CatenaFinitaCfg){.src = plan->kind, .other = "file", .cap = CLI_CELLS_POOL})) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = plan->language, .other = "c", .cap = CLI_CELLS_POOL})))
    {
        char full[CLI_PATH_LONGEST];
        if (!start_field(json, start, "path", path, sizeof(path), said))
        {
            return 0;
        }
        cli_full(root, path, full);
        plan->has_made = cli_compile_db_program_of(root, full, &plan->made);
    }
    if (plan->recording)
    {
        plan->spec = cli_json_made(to, CFG_JSON_OBJECT);
        cli_json_set(to, plan->spec, "program", cli_json_string_made(to, "orior", 5ull));
        cli_json_set(to, plan->spec, "launch", cli_json_made(to, CFG_JSON_OBJECT));
        memor.cpy(&(MemoriaCfg){
            .dst = plan->tool,
            .src = "orior's recorder",
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "orior's recorder", .cap = CLI_CELLS_POOL}) + 1u});
        plan->tool_id[0] = '\0';
        plan->program[0] = '\0';
        return 1;
    }
    const char *const ahead =
        !plan->has_made                                                                                      ? ""
        : (cellul.eq(&(CatenaFinitaCfg){.src = plan->made.compiler, .other = "GNU", .cap = CLI_CELLS_POOL})) ? "gcc"
                                                                                                             : "llvm";
    return cli_debug_adapter(plan->language, ahead, to, &plan->spec, plan->tool, plan->tool_id, plan->program, said);
}

// the launch of a file: the program built where its language is built, the file itself where it runs from its source
static int start_file(CliDebugSession *place, const char *root, const CliJson *json, unsigned int start,
                      CliDebugPlan *plan, CliLine *said)
{
    CliJson *const to = &place->start;
    char path[CLI_PATH_LONGEST];
    char file[CLI_PATH_LONGEST];
    char program[CLI_PATH_LONGEST];
    int built = 0;
    if (!start_field(json, start, "path", path, sizeof(path), said))
    {
        return 0;
    }
    cli_full(root, path, file);
    if (plan->has_made)
    {
        built = cli_debug_build_program(&plan->made, program, said);
        if (!built)
        {
            return 0;
        }
    }
    else if (!cli_debug_build(root, file, plan->language, plan->tool_id, program, &built, said))
    {
        return 0;
    }
    const char *const names[3] = {"program", "path", "root"};
    const char *const withs[3] = {built ? program : file, file, root};
    plan->request = "launch";
    plan->arguments = cli_debug_filled(to, to, cli_json_member(to, plan->spec, "launch"), names, withs, 3u, 0);
    memor.cpy(&(MemoriaCfg){.dst = plan->name,
                            .src = path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
    return 1;
}

// the launch of one test: a Rust test's program, built as cargo test builds it, run for that test alone, or pytest
// run on a Python test, in place of a program
static int start_test(CliDebugSession *place, const char *root, CliDebugPlan *plan, CliLine *said)
{
    CliJson *const to = &place->start;
    const unsigned int launch = cli_json_member(to, plan->spec, "launch");
    plan->request = "launch";
    memor.cpy(&(MemoriaCfg){.dst = plan->name,
                            .src = plan->id,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = plan->id, .cap = CLI_CELLS_POOL}) + 1u});
    if (cli_cargo_is_rust(plan->id))
    {
        char program[CLI_PATH_LONGEST];
        char full[CLI_PATH_LONGEST];
        char folder[CLI_PATH_LONGEST];
        if (!cli_cargo_binary(root, plan->id, program, full, folder, said))
        {
            return 0;
        }
        const char *const names[3] = {"program", "path", "root"};
        const char *const withs[3] = {program, "", folder};
        plan->arguments = cli_debug_filled(to, to, launch, names, withs, 3u, 0);
        if (!cli_json_is(to, plan->arguments, CFG_JSON_OBJECT))
        {
            plan->arguments = cli_json_made(to, CFG_JSON_OBJECT);
        }
        const char *const words[4] = {full, "--exact", "--nocapture", "--test-threads=1"};
        const unsigned int args = cli_json_made(to, CFG_JSON_ARRAY);
        for (unsigned int at = 0u; at < 4u; at += 1u)
        {
            cli_json_append(
                to, args,
                cli_json_string_made(to, words[at],
                                     cellul.len(&(CatenaFinitaCfg){.src = words[at], .cap = CLI_CELLS_POOL})));
        }
        cli_json_set(to, plan->arguments, "args", args);
        cli_json_set(
            to, plan->arguments, "cwd",
            cli_json_string_made(to, folder, cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL})));
        return 1;
    }
    const char *const names[3] = {"program", "path", "root"};
    const char *const withs[3] = {"", "", root};
    plan->arguments = cli_debug_filled(to, to, launch, names, withs, 3u, 0);
    if (cli_json_is(to, plan->arguments, CFG_JSON_OBJECT))
    {
        char python[CLI_PATH_LONGEST];
        const char *const words[4] = {plan->id, "-p", "no:cacheprovider", "-q"};
        const unsigned int args = cli_json_made(to, CFG_JSON_ARRAY);
        for (unsigned int at = 0u; at < 4u; at += 1u)
        {
            cli_json_append(
                to, args,
                cli_json_string_made(to, words[at],
                                     cellul.len(&(CatenaFinitaCfg){.src = words[at], .cap = CLI_CELLS_POOL})));
        }
        cli_json_remove(to, plan->arguments, "program");
        cli_json_set(to, plan->arguments, "module", cli_json_string_made(to, "pytest", 6ull));
        cli_json_set(to, plan->arguments, "args", args);
        if (cli_test_python(python))
        {
            cli_json_set(
                to, plan->arguments, "python",
                cli_json_string_made(to, python, cellul.len(&(CatenaFinitaCfg){.src = python, .cap = CLI_CELLS_POOL})));
        }
    }
    return 1;
}

// the attach to a process by its number, its native code where a session started it
static int start_process(CliDebugSession *place, const char *root, const CliJson *json, unsigned int start,
                         CliDebugPlan *plan, CliLine *said)
{
    CliJson *const to = &place->start;
    unsigned long long pid = 0ull;
    if (!cli_debug_unsigned(json, cli_json_member(json, start, "pid"), &pid) || (pid > 0xffffffffull))
    {
        cli_line_text(said, "missing field `pid`");
        return 0;
    }
    plan->has_parent = cli_debug_unsigned(json, cli_json_member(json, start, "parent"), &plan->parent);
    plan->has_pid = 1;
    plan->pid = (unsigned int)pid;
    char number[32];
    CliLine pid_line = {number, sizeof(number), 0ull};
    cli_line_decimal(&pid_line, pid, 1u);
    cli_line_closed(&pid_line);
    unsigned int template = cli_json_member(to, plan->spec, "attach");
    if (template == CLI_JSON_NONE)
    {
        template = cli_json_made(to, CFG_JSON_OBJECT);
        cli_json_set(to, template, "pid", cli_json_string_made(to, "{pid}", 5ull));
    }
    const char *const names[2] = {"pid", "root"};
    const char *const withs[2] = {number, root};
    plan->request = "attach";
    plan->arguments = cli_debug_filled(to, to, template, names, withs, 2u, 1);
    CliLine name = {plan->name, sizeof(plan->name), 0ull};
    cli_line_text(&name, plan->has_parent ? "the native code of process " : "process ");
    cli_line_text(&name, number);
    cli_line_closed(&name);
    return 1;
}

// the attach to a debugger listening at an address, and to the child a session's program started
static int start_reached(CliDebugSession *place, const char *root, const CliJson *json, unsigned int start,
                         CliDebugPlan *plan, CliLine *said)
{
    CliJson *const to = &place->start;
    plan->request = "attach";
    if (cellul.eq(&(CatenaFinitaCfg){.src = plan->kind, .other = "child", .cap = CLI_CELLS_POOL}))
    {
        const unsigned int config = cli_json_member(json, start, "config");
        (void)cli_debug_unsigned(json, cli_json_member(json, start, "parent"), &plan->parent);
        plan->has_parent = 1;
        plan->arguments =
            (config != CLI_JSON_NONE) ? cli_json_copy_from(to, json, config) : cli_json_made(to, CFG_JSON_NULL);
        const char *const named = cli_json_string(json, cli_json_member(json, config, "name"), NULL);
        unsigned long long sub = 0ull;
        CliLine name = {plan->name, sizeof(plan->name), 0ull};
        if (named != NULL)
        {
            cli_line_text(&name, named);
        }
        else if (cli_debug_unsigned(json, cli_json_member(json, config, "subProcessId"), &sub))
        {
            cli_line_text(&name, "process ");
            cli_line_decimal(&name, sub, 1u);
        }
        else
        {
            cli_line_text(&name, "child");
        }
        cli_line_closed(&name);
        const unsigned int connect = cli_json_member(json, config, "connect");
        unsigned long long port = 0ull;
        plan->listening = cli_debug_unsigned(json, cli_json_member(json, connect, "port"), &port);
        // a port keeps its low 16 bits, as the protocol's ports are
        plan->port = (unsigned int)(port & 0xffffull);
        memor.cpy(
            &(MemoriaCfg){.dst = plan->host,
                          .src = cli_debug_text(json, cli_json_member(json, connect, "host"), "127.0.0.1"),
                          .bytes = cellul.len(&(CatenaFinitaCfg){
                                       .src = cli_debug_text(json, cli_json_member(json, connect, "host"), "127.0.0.1"),
                                       .cap = CLI_CELLS_POOL}) +
                                   1u});
        plan->ends_program = 1;
        return 1;
    }
    unsigned long long port = 0ull;
    if (!start_field(json, start, "host", plan->host, sizeof(plan->host), said))
    {
        return 0;
    }
    if (!cli_debug_unsigned(json, cli_json_member(json, start, "port"), &port) || (port > 0xffffull))
    {
        cli_line_text(said, "missing field `port`");
        return 0;
    }
    plan->port = (unsigned int)port;
    plan->listening = 1;
    plan->ends_program = 0;
    plan->arguments = cli_json_made(to, CFG_JSON_OBJECT);
    const unsigned int connect = cli_json_made(to, CFG_JSON_OBJECT);
    cli_json_set(
        to, connect, "host",
        cli_json_string_made(to, plan->host, cellul.len(&(CatenaFinitaCfg){.src = plan->host, .cap = CLI_CELLS_POOL})));
    char number[32];
    CliLine port_line = {number, sizeof(number), 0ull};
    cli_line_decimal(&port_line, port, 1u);
    cli_line_closed(&port_line);
    cli_json_set(to, connect, "port", cli_json_number_made(to, number));
    cli_json_set(to, plan->arguments, "connect", connect);
    const char *const remote = cli_json_string(json, cli_json_member(json, start, "remote"), NULL);
    if ((remote != NULL) && (remote[0] != '\0'))
    {
        const unsigned int mappings = cli_json_made(to, CFG_JSON_ARRAY);
        const unsigned int mapping = cli_json_made(to, CFG_JSON_OBJECT);
        cli_json_set(
            to, mapping, "localRoot",
            cli_json_string_made(to, root, cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL})));
        cli_json_set(
            to, mapping, "remoteRoot",
            cli_json_string_made(to, remote, cellul.len(&(CatenaFinitaCfg){.src = remote, .cap = CLI_CELLS_POOL})));
        cli_json_append(to, mappings, mapping);
        cli_json_set(to, plan->arguments, "pathMappings", mappings);
    }
    CliLine name = {plan->name, sizeof(plan->name), 0ull};
    cli_line_text(&name, plan->host);
    cli_line_character(&name, ':');
    cli_line_text(&name, number);
    cli_line_closed(&name);
    return 1;
}

int cli_debug_planned(CliDebugSession *place, const char *root, const CliJson *json, unsigned int start,
                      unsigned int options, CliDebugPlan *plan, CliLine *said)
{
    CliJson *const to = &place->start;
    cli_json_begun(to, place->start_nodes, CLI_DEBUG_START_NODES, place->start_text, sizeof(place->start_text));
    memor.set(&(MemoriaCfg){.dst = plan, .val = 0, .bytes = sizeof(*plan)});
    if (!start_field(json, start, "kind", plan->kind, sizeof(plan->kind), said) ||
        !start_adapter(place, root, json, start, plan, said))
    {
        return 0;
    }
    int made = 0;
    if (cellul.eq(&(CatenaFinitaCfg){.src = plan->kind, .other = "file", .cap = CLI_CELLS_POOL}))
    {
        made = start_file(place, root, json, start, plan, said);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = plan->kind, .other = "test", .cap = CLI_CELLS_POOL}))
    {
        made = start_test(place, root, plan, said);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = plan->kind, .other = "process", .cap = CLI_CELLS_POOL}))
    {
        made = start_process(place, root, json, start, plan, said);
    }
    else if (plan->recording)
    {
        char path[CLI_PATH_LONGEST];
        char full[CLI_PATH_LONGEST];
        char python[CLI_PATH_LONGEST];
        made = start_field(json, start, "path", path, sizeof(path), said);
        cli_full(root, path, full);
        python[0] = '\0';
        (void)cli_test_python(python);
        plan->request = "launch";
        plan->arguments = cli_json_made(to, CFG_JSON_OBJECT);
        cli_json_set(
            to, plan->arguments, "program",
            cli_json_string_made(to, full, cellul.len(&(CatenaFinitaCfg){.src = full, .cap = CLI_CELLS_POOL})));
        cli_json_set(
            to, plan->arguments, "python",
            cli_json_string_made(to, python, cellul.len(&(CatenaFinitaCfg){.src = python, .cap = CLI_CELLS_POOL})));
        memor.cpy(&(MemoriaCfg){.dst = plan->name,
                                .src = path,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
    }
    else
    {
        made = start_reached(place, root, json, start, plan, said);
    }
    if (!made)
    {
        return 0;
    }
    const int launch = (cellul.eq(&(CatenaFinitaCfg){.src = plan->request, .other = "launch", .cap = CLI_CELLS_POOL}));
    if (launch && (cellul.eq(&(CatenaFinitaCfg){.src = plan->language, .other = "python", .cap = CLI_CELLS_POOL})) &&
        !plan->recording && cli_json_is(to, plan->arguments, CFG_JSON_OBJECT))
    {
        // the processes a Python program starts are debugged with it
        cli_json_set(to, plan->arguments, "subProcess", cli_json_made(to, CFG_JSON_TRUE));
    }
    if (!cellul.eq(&(CatenaFinitaCfg){.src = plan->kind, .other = "child", .cap = CLI_CELLS_POOL}))
    {
        cli_debug_step_arguments(plan->language, json, cli_json_member(json, options, "stepping"), to, plan->arguments);
    }
    if (cli_debug_true(json, cli_json_member(json, options, "entry")) && launch)
    {
        if (!cli_json_is(to, plan->arguments, CFG_JSON_OBJECT))
        {
            plan->arguments = cli_json_made(to, CFG_JSON_OBJECT);
        }
        cli_json_set(to, plan->arguments, "stopOnEntry", cli_json_made(to, CFG_JSON_TRUE));
    }
    if (to->full)
    {
        cli_line_text(said, "the session's start does not fit the room it is made in");
        return 0;
    }
    return 1;
}
