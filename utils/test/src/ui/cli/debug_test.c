// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_test.c: the debugger's check: a function's bytecode is read with its lines; the machine's processes are
// listed with the lines they started with; launch arguments take the program and the folder; a frame counts its line
// from zero; breakpoints carry their conditions, counts and messages; memory comes as base64; stepping sets debugpy's
// rules and lldb's patterns; and C, Rust and Python each have a debugger, and C and Rust a build. The checks that start
// a debugger's adapter are each marked ignored where they are written and are not brought
#include "../../../../../ui/cli/src/debug/debug_internal.h"
#include "../../../../../ui/cli/src/runs/runner.h"
#include "../../../../../ui/cli/src/toolchains/toolchains.h"
#include "cli_check.h"

static char s_out[1u << 20u];
static CliJson *s_read;

// `text` read into the tree `to` in a cell sized to it with room for the arguments a check puts in it, the tree read
// before given back: its root
static unsigned int debug_read(CliJson *to, const char *text)
{
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    if (s_read != NULL)
    {
        cli_json_given(s_read);
    }
    s_read = to;
    return cli_json_taken(to, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), 64u, 4096ull,
                          &said);
}

// node `node` of `json` written compact, its keys in order, into s_out
static const char *debug_written(const CliJson *json, unsigned int node)
{
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_json_write(json, node, &out, 0u, 1);
    (void)cli_line_closed(&out);
    return s_out;
}

// a function's bytecode is read with its lines
static void debug_bytecode(void)
{
    char dir[CLI_PATH_LONGEST];
    char file[CLI_PATH_LONGEST];
    char python[CLI_PATH_LONGEST];
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check_folder("debug_bytecode", dir);
    cli_full(dir, "a.py", file);
    static const char s_area[] = "x = 1\n\n\ndef area(width, height):\n    return width * height\n";
    (void)query_file_written(file, s_area, sizeof(s_area) - 1u);
    cli_runner_python(python);
    const char *const version[3] = {python, "--version", NULL};
    CliRan *const ran = cli_ran_taken();
    const int has_python = cli_ran(version, NULL, NULL, 0u, ran, &said) && (ran->code == 0ll);
    cli_ran_given(ran);
    if (!has_python)
    {
        cli_check(1, "no Python to compile the function with");
        return;
    }
    CliLine out = {s_out, sizeof(s_out), 0ull};
    said.at = 0ull;
    cli_check(cli_debug_bytecode(dir, file, 4u, &out, &said), "a function's bytecode is read");
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_out, out.at, &read);
    const char *const name = cli_json_string(json, cli_json_item(json, read, 0u), NULL);
    const unsigned int instructions = cli_json_item(json, read, 1u);
    int found = 0;
    for (unsigned int at = 0u; !found && (cli_json_item(json, instructions, at) != CLI_JSON_NONE); at += 1u)
    {
        const unsigned int one = cli_json_item(json, instructions, at);
        const unsigned int line = cli_json_member(json, one, "line");
        found = (cellul.starts(&(CatenaFinitaCfg){
                    .src = cli_check_member_text(one, "text"), .other = "BINARY_OP", .cap = CLI_CELLS_POOL})) &&
                (line != CLI_JSON_NONE) && (json->node[line].length == 1ull) &&
                (json->text[json->node[line].text] == '4');
    }
    cli_check((name != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "area", .cap = CLI_CELLS_POOL})) &&
                  found,
              "its name, and an instruction of its line");
}

// whether the processes listed hold `pid` with a line it started with: its line into `line`, which holds `room`
static int debug_listed(unsigned long long pid, char *line, unsigned long long room)
{
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_debug_processes(&out);
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_out, out.at, &read);
    line[0] = '\0';
    int own = 0;
    int found = 0;
    for (unsigned int at = 0u; cli_json_item(json, read, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int one = cli_json_item(json, read, at);
        const unsigned int number = cli_json_member(json, one, "pid");
        unsigned long long value = 0ull;
        for (unsigned long long digit = 0ull; (number != CLI_JSON_NONE) && (digit < json->node[number].length);
             digit += 1ull)
        {
            value = (value * 10ull) + (unsigned long long)(json->text[json->node[number].text + digit] - '0');
        }
        own = own || (value == cli_process_own());
        if (value == pid)
        {
            memor.cpy(&(MemoriaCfg){.dst = line,
                                    .src = cli_check_member_text(one, "command"),
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = cli_check_member_text(one, "command"),
                                                                           .cap = CLI_CELLS_POOL}) +
                                             1u});
            found = 1;
        }
    }
    cli_check(!own, "this process is not listed");
    return found && (line[0] != '\0');
}

// the machine's processes are listed with the lines they started with
static void debug_processes(void)
{
#if defined(_WIN32)
    const char *const words[8] = {"cmd", "/c", "ping", "-n", "4", "127.0.0.1", ">NUL", NULL};
    static const char s_held[] = "127.0.0.1";
#else
    const char *const words[3] = {"sleep", "3", NULL};
    static const char s_held[] = "3";
#endif
    CliSpawned child;
    char why[1024];
    char line[4096];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check(cli_spawn(&child, words, NULL, NULL, 0u, 1, &said), "a process is started");
    const unsigned long long began = cli_clock_milliseconds();
    int listed = debug_listed(child.id, line, sizeof(line));
    const unsigned long long took = cli_clock_milliseconds() - began;
    // on Linux a process takes its name as its program starts and its line an instant after, once the system has laid
    // the program out: the list is read again until the line shows
    for (unsigned int tries = 0u; !listed && (tries < 40u); tries += 1u)
    {
        query_paused(25u);
        listed = debug_listed(child.id, line, sizeof(line));
    }
    cli_spawn_tree_end(child.id);
    (void)cli_spawn_wait(&child);
    cli_spawn_closed(&child);
    cli_check(listed &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = line, .cap = CLI_CELLS_POOL, .other = s_held, .other_cap = CLI_CELLS_POOL}) != NULL),
              "the machine's processes, with the lines they started with");
    cli_check(took < 2000ull, "quick enough for a picker");
}

// launch arguments take the program and the folder, and a frame counts its line from zero
static void debug_arguments(void)
{
    static CliJsonNode s_to_nodes[1024];
    static char s_to_text[1u << 14u];
    CliJson to;
    unsigned int read = CLI_JSON_NONE;
    const CliJson *json = cli_check_parsed(
        "{\"program\":\"{program}\",\"cwd\":\"{root}\",\"args\":[],\"deep\":{\"file\":\"{path}\"}}", 73ull, &read);
    static const char *const s_names[3] = {"program", "path", "root"};
    static const char *const s_withs[3] = {"a.exe", "a.c", "/t"};
    cli_json_begun(&to, s_to_nodes, 1024u, s_to_text, sizeof(s_to_text));
    unsigned int filled = cli_debug_filled(&to, json, read, s_names, s_withs, 3u, 0);
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = debug_written(&to, filled),
                  .other = "{\"args\":[],\"cwd\":\"/t\",\"deep\":{\"file\":\"a.c\"},\"program\":\"a.exe\"}",
                  .cap = CLI_CELLS_POOL}),
              "launch arguments take the program and the folder");
    json = cli_check_parsed("{\"processId\":\"{pid}\"}", 21ull, &read);
    static const char *const s_pid[1] = {"pid"};
    static const char *const s_number[1] = {"412"};
    cli_json_begun(&to, s_to_nodes, 1024u, s_to_text, sizeof(s_to_text));
    filled = cli_debug_filled(&to, json, read, s_pid, s_number, 1u, 1);
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = debug_written(&to, filled), .other = "{\"processId\":412}", .cap = CLI_CELLS_POOL}),
              "a number filled in is a number");
    static const char s_frame[] = "{\"id\":7,\"name\":\"main\",\"source\":{\"path\":\"/t/a.c\"},\"line\":6,\"column\":"
                                  "13,\"instructionPointerReference\":\"0x1000\"}";
    json = cli_check_parsed(s_frame, sizeof(s_frame) - 1u, &read);
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_debug_frame_written(json, read, NULL, NULL, &out);
    cli_check_text(s_out, out.at,
                   "{\"id\":7,\"name\":\"main\",\"path\":\"/t/a.c\",\"line\":5,\"col\":12,\"ip\":\"0x1000\"}",
                   "a frame counts its line from zero");
}

// breakpoints carry their conditions, counts and messages; and memory comes as base64
static void debug_breakpoints(void)
{
    static const char s_given[] = "[{\"line\":4},{\"line\":9,\"condition\":\"x > 2\",\"hits\":\">= 3\",\"log\":\"\"},{"
                                  "\"line\":11,\"log\":\"x is {x}\"}]";
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_given, sizeof(s_given) - 1u, &read);
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_debug_breakpoints_written(json, read, &out);
    cli_check_text(s_out, out.at,
                   "[{\"line\":5},{\"line\":10,\"condition\":\"x > 2\",\"hitCondition\":\">= 3\"},{\"line\":12,"
                   "\"logMessage\":\"x is {x}\"}]",
                   "breakpoints carry their conditions, counts and messages");
    cli_check(cli_debug_takes("python", "a/b.py") && !cli_debug_takes("python", "a/b.c") &&
                  cli_debug_takes("c", "a/b.c") && !cli_debug_takes("rust", "x.py"),
              "a session takes the files of its language");
    unsigned char bytes[16];
    cli_check((cli_debug_base64("SGVsbG8=", 8ull, bytes, sizeof(bytes)) == 5ull) &&
                  (memor.cmp(&(MemoriaCfg){.src = bytes, .other = "Hello", .bytes = 5u}) == 0),
              "memory comes as base64");
    static const unsigned char s_wanted[4] = {0u, 1u, 2u, 255u};
    cli_check((cli_debug_base64("AAEC/w==", 8ull, bytes, sizeof(bytes)) == 4ull) &&
                  (memor.cmp(&(MemoriaCfg){.src = bytes, .other = s_wanted, .bytes = 4u}) == 0),
              "every byte of it");
}

// stepping sets debugpy's rules and lldb's patterns
static void debug_stepping(void)
{
    CliJson to;
    unsigned int read = CLI_JSON_NONE;
    static const char s_python[] = "{\"mine_only\":true,\"skip\":[\"requests\",\"**/vendor/**\"]}";
    const CliJson *json = cli_check_parsed(s_python, sizeof(s_python) - 1u, &read);
    unsigned int arguments = debug_read(&to, "{\"program\":\"a.py\"}");
    cli_debug_step_arguments("python", json, read, &to, arguments);
    cli_check(cli_json_is(&to, cli_json_member(&to, arguments, "justMyCode"), CFG_JSON_TRUE), "debugpy steps in mine");
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){
            .src = debug_written(&to, cli_json_member(&to, arguments, "rules")),
            .other = "[{\"include\":false,\"module\":\"requests\"},{\"include\":false,\"path\":\"**/vendor/**\"}]",
            .cap = CLI_CELLS_POOL}),
        "and skips what it is told, a module or a path");
    static const char s_native[] = "{\"mine_only\":false,\"skip\":[\"^std::\",\"^boost::\"]}";
    json = cli_check_parsed(s_native, sizeof(s_native) - 1u, &read);
    arguments = debug_read(&to, "{\"program\":\"a.exe\",\"initCommands\":[\"x\"]}");
    cli_debug_step_arguments("c", json, read, &to, arguments);
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = debug_written(&to, cli_json_member(&to, arguments, "initCommands")),
                  .other = "[\"x\",\"settings set target.process.thread.step-avoid-regexp ^std::|^boost::\"]",
                  .cap = CLI_CELLS_POOL}),
              "lldb avoids the patterns it is told");
}

// whether a tool of the manifest debugs `language`, or builds `kind`
static int debug_tool_has(const CliJson *json, unsigned int manifest, const char *language, const char *kind)
{
    for (unsigned int tool = cli_json_is(json, manifest, CFG_JSON_ARRAY) ? json->node[manifest].first : CLI_JSON_NONE;
         tool != CLI_JSON_NONE; tool = json->node[tool].next)
    {
        if (kind != NULL)
        {
            if (cli_json_member(json, cli_json_member(json, tool, "builds"), kind) != CLI_JSON_NONE)
            {
                return 1;
            }
            continue;
        }
        const unsigned int languages = cli_json_member(json, cli_json_member(json, tool, "debugger"), "languages");
        for (unsigned int at = 0u; cli_json_item(json, languages, at) != CLI_JSON_NONE; at += 1u)
        {
            const char *const one = cli_json_string(json, cli_json_item(json, languages, at), NULL);
            if ((one != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = one, .other = language, .cap = CLI_CELLS_POOL})))
            {
                return 1;
            }
        }
    }
    return 0;
}

// C, Rust and Python each have a debugger, and C and Rust a build
static void debug_manifest(void)
{
    CliJson json;
    const unsigned int manifest = cli_toolchains_manifest(&json, 0u, 0ull);
    cli_check(debug_tool_has(&json, manifest, "c", NULL) && debug_tool_has(&json, manifest, "rust", NULL) &&
                  debug_tool_has(&json, manifest, "python", NULL),
              "C, Rust and Python each have a debugger");
    cli_check(debug_tool_has(&json, manifest, NULL, "c.c") && debug_tool_has(&json, manifest, NULL, "rust") &&
                  !debug_tool_has(&json, manifest, NULL, "python"),
              "and C and Rust a build");
    cli_json_given(&json);
}

int main(void)
{
    debug_bytecode();
    debug_processes();
    debug_arguments();
    debug_breakpoints();
    debug_stepping();
    debug_manifest();
    return cli_checked();
}
