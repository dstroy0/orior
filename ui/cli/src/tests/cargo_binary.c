// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cargo_binary.c: the program that runs a Rust test, built as cargo test builds it, read from Cargo's own words of the
// artifacts it made, for a debugger to start it
#include "../programs/executables.h"
#include "../text/json_tree.h"
#include "../toolchains/toolchains.h"
#include "cargo_report.h"
#include "cargo_tests.h"

// the most nodes and bytes of one line of Cargo's words read
#define CLI_CARGO_LINE_NODES 65536u
#define CLI_CARGO_LINE_TEXT (1u << 20u)

static CliCargoTests s_binary_tests;
static CliJsonNode s_nodes[CLI_CARGO_LINE_NODES];
static CfgJsonToken s_tokens[CLI_CARGO_LINE_NODES];
static char s_text[CLI_CARGO_LINE_TEXT];
static char s_path[CLI_PATH_TEXT];
static CliLock s_binary_lock = CLI_LOCK_INIT;

// whether Cargo's word of an artifact, its target's kinds and name, is of the target
static int binary_is(const CliCargoTarget *target, const CliJson *json, unsigned int kinds, const char *name)
{
    int is_lib = 0;
    int is_bin = 0;
    int is_test = 0;
    for (unsigned int index = 0u; cli_json_item(json, kinds, index) != CLI_JSON_NONE; index += 1u)
    {
        const char *const kind = cli_json_string(json, cli_json_item(json, kinds, index), NULL);
        const size_t length = (kind != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = kind, .cap = CLI_CELLS_POOL}) : 0u;
        is_lib = is_lib ||
                 ((length >= 3u) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = kind + length - 3u, .other = "lib", .cap = CLI_CELLS_POOL}))) ||
                 ((kind != NULL) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "proc-macro", .cap = CLI_CELLS_POOL})));
        is_bin = is_bin || ((kind != NULL) &&
                            (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "bin", .cap = CLI_CELLS_POOL})));
        is_test = is_test || ((kind != NULL) &&
                              (cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "test", .cap = CLI_CELLS_POOL})));
    }
    return (target->kind == CLI_CARGO_LIB) ? is_lib
           : (target->kind == CLI_CARGO_BIN)
               ? (is_bin && (cellul.eq(&(CatenaFinitaCfg){.src = target->name, .other = name, .cap = CLI_CELLS_POOL})))
               : (is_test &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = target->name, .other = name, .cap = CLI_CELLS_POOL})));
}

// the program Cargo's words name for the test, read line by line from what it wrote: 1, or 0 where none does
static int binary_read(const CliCargoFound *one, const char *out, unsigned long long size, char *program)
{
    unsigned long long at = 0ull;
    while (at < size)
    {
        const char *const newline = memor.chr(&(MemoriaCfg){.src = out + at, .val = '\n', .bytes = size - at});
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - out) : size;
        const unsigned long long start = at;
        at = (newline != NULL) ? end + 1ull : size;
        CliJson json;
        cli_json_begun(&json, s_nodes, CLI_CARGO_LINE_NODES, s_text, CLI_CARGO_LINE_TEXT);
        char why[CLI_SAID_LONGEST];
        CliLine unread = {why, sizeof(why), 0ull};
        const unsigned int said =
            cli_json_read(&json, out + start, end - start, s_tokens, CLI_CARGO_LINE_NODES, &unread);
        const char *const reason = cli_json_string(&json, cli_json_member(&json, said, "reason"), NULL);
        const unsigned int test = cli_json_member(&json, cli_json_member(&json, said, "profile"), "test");
        if ((reason == NULL) ||
            (!cellul.eq(&(CatenaFinitaCfg){.src = reason, .other = "compiler-artifact", .cap = CLI_CELLS_POOL})) ||
            !cli_json_is(&json, test, CFG_JSON_TRUE))
        {
            continue;
        }
        const unsigned int target = cli_json_member(&json, said, "target");
        const char *const name = cli_json_string(&json, cli_json_member(&json, target, "name"), NULL);
        const char *const executable = cli_json_string(&json, cli_json_member(&json, said, "executable"), NULL);
        if ((executable != NULL) &&
            binary_is(&one->target, &json, cli_json_member(&json, target, "kind"), (name != NULL) ? name : ""))
        {
            memor.cpy(
                &(MemoriaCfg){.dst = program,
                              .src = executable,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = executable, .cap = CLI_CELLS_POOL}) + 1u});
            return 1;
        }
    }
    return 0;
}

// the program for the test found `one` built, read and said into `program`: 1, or 0 with why said
static int binary_built(const char *root, const CliCargoFound *one, const char *package, char *program, CliLine *said)
{
    char cargo[CLI_PATH_LONGEST];
    char manifest[CLI_PATH_LONGEST];
    if (!cli_executables_program("rust", "cargo", cargo, said))
    {
        return 0;
    }
    cli_full(root, one->manifest, manifest);
    const char *words[12] = {cargo, "test", "--no-run", "--message-format=json", "--package", one->package};
    unsigned int count = 6u;
    words[count] = (one->target.kind == CLI_CARGO_LIB)   ? "--lib"
                   : (one->target.kind == CLI_CARGO_BIN) ? "--bin"
                                                         : "--test";
    count += 1u;
    if (one->target.kind != CLI_CARGO_LIB)
    {
        words[count] = one->target.name;
        count += 1u;
    }
    words[count] = "--manifest-path";
    words[count + 1u] = manifest;
    words[count + 2u] = NULL;
    cli_toolchains_run_path(s_path, sizeof(s_path));
    const CliSetting settings[1] = {{"PATH", s_path}};
    CliRan *const ran = cli_ran_taken();
    if (!cli_ran(words, package, settings, 1u, ran, said))
    {
        cli_ran_given(ran);
        return 0;
    }
    const int built = binary_read(one, ran->out, ran->out_size, program);
    if (!built)
    {
        char error[4096];
        char path[CLI_PATH_LONGEST];
        long long line = 0ll;
        unsigned long long length = 0ull;
        if (cli_cargo_build_error(ran->err, ran->err_size, error, sizeof(error), &length, path, &line))
        {
            cli_line_text(said, "The test's build failed: ");
            cli_line_counted(said, error, length);
        }
        else
        {
            cli_line_text(said, "cargo test built no program for ");
            cli_line_text(said, one->test.id);
        }
    }
    cli_ran_given(ran);
    return built;
}

int cli_cargo_binary(const char *root, const char *id, char *program, char *full, char *package, CliLine *said)
{
    cli_lock_take(&s_binary_lock);
    cli_cargo_found(root, &s_binary_tests);
    const CliCargoFound *one = NULL;
    for (unsigned int at = 0u; (one == NULL) && (at < s_binary_tests.count); at += 1u)
    {
        one =
            (cellul.eq(&(CatenaFinitaCfg){.src = s_binary_tests.found[at].test.id, .other = id, .cap = CLI_CELLS_POOL}))
                ? &s_binary_tests.found[at]
                : NULL;
    }
    int built = 0;
    if (one == NULL)
    {
        cli_line_text(said, id);
        cli_line_text(said, " is no Rust test of this tree");
    }
    else
    {
        char folder[CLI_PATH_LONGEST];
        CliLine folder_line = {folder, sizeof(folder), 0ull};
        cli_line_counted(&folder_line, one->manifest,
                         cellul.len(&(CatenaFinitaCfg){.src = one->manifest, .cap = CLI_CELLS_POOL}) - 10u);
        cli_line_closed(&folder_line);
        cli_full(root, folder, package);
        built = binary_built(root, one, package, program, said);
        if (built)
        {
            memor.cpy(
                &(MemoriaCfg){.dst = full,
                              .src = one->full,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = one->full, .cap = CLI_CELLS_POOL}) + 1u});
        }
    }
    cli_lock_give(&s_binary_lock);
    return built;
}
