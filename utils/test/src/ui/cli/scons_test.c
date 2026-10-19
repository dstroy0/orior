// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// scons_test.c: SCons's check: scripts are read and names imported are checked, and SCons's names complete
#include "../../../../../ui/cli/src/builds/builds.h"
#include "../../../../../ui/cli/src/builds/scons_internal.h"
#include "cli_check.h"

static char s_dir[CLI_PATH_LONGEST];
static char s_out[1u << 18u];
static const CliCompletions *s_items;

// the findings of the file `name` of the folder, `text` its text, read into the shared tree: its node
static unsigned int scons_checked(const char *name, const char *text, const CliJson **json)
{
    char path[CLI_PATH_LONGEST];
    cli_full(s_dir, name, path);
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_build_check(s_dir, path, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), &out);
    unsigned int found = CLI_JSON_NONE;
    *json = cli_check_parsed(s_out, out.at, &found);
    return found;
}

// the message of a finding
static const char *scons_message(const CliJson *json, unsigned int finding)
{
    return cli_check_member_text(cli_json_member(json, finding, "message"), "value");
}

// a whole number of the range's `end` or `start`
static unsigned long long scons_at(const CliJson *json, unsigned int finding, const char *end, const char *name)
{
    const unsigned int number =
        cli_json_member(json, cli_json_member(json, cli_json_member(json, finding, "range"), end), name);
    unsigned long long value = 0ull;
    for (unsigned long long at = 0ull; cli_json_is(json, number, CFG_JSON_NUMBER) && (at < json->node[number].length);
         at += 1ull)
    {
        value = value * 10ull + (unsigned long long)(json->text[json->node[number].text + at] - '0');
    }
    return value;
}

// whether a name SCons gives the script is `wanted`
typedef struct
{
    const char *wanted;
    int found;
} SconsGiven;

static void scons_given(void *context, const char *name, unsigned long long size)
{
    SconsGiven *const given = context;
    given->found |= (cellul.len(&(CatenaFinitaCfg){.src = given->wanted, .cap = CLI_CELLS_POOL}) == size) &&
                    (memor.cmp(&(MemoriaCfg){.src = name, .other = given->wanted, .bytes = size}) == 0);
}

// whether what completes `text` at its end holds the label `label`, the items kept in s_items
static int scons_completes(const char *text, const char *label)
{
    char path[CLI_PATH_LONGEST];
    cli_full(s_dir, "SConstruct", path);
    s_items = cli_check_build_completed(s_dir, path, text);
    return cli_check_completes(s_items, label);
}

int main(void)
{
    static const char s_top[] = "env = Environment(CPPPATH=['include'])\nExport('env')\nSConscript('src/SConscript')\n"
                                "SConscript(['lib/SConscript'], exports='libs')\nSConscript(dirs=['gone'])\n"
                                "# SConscript('commented')\n";
    static const char s_sub[] = "Import('env libs tools')\nenv.Program('hello', ['hello.c'])\n";
    char path[CLI_PATH_LONGEST];
    cli_check_folder("scons", s_dir);
    cli_full(s_dir, "src", path);
    (void)query_folder_made(path);
    cli_full(s_dir, "SConstruct", path);
    (void)query_file_written(path, s_top, sizeof(s_top) - 1u);
    cli_full(s_dir, "src/SConscript", path);
    (void)query_file_written(path, s_sub, sizeof(s_sub) - 1u);
    const CliJson *json = NULL;
    unsigned int found = scons_checked("SConstruct", s_top, &json);
    cli_check((json->node[found].count == 2u) &&
                  (cellul.find(&(CatenaFinitaCfg){.src = scons_message(json, cli_json_item(json, found, 0u)),
                                                  .cap = CLI_CELLS_POOL,
                                                  .other = "`lib/SConscript`",
                                                  .other_cap = CLI_CELLS_POOL}) != NULL) &&
                  (cellul.find(&(CatenaFinitaCfg){.src = scons_message(json, cli_json_item(json, found, 1u)),
                                                  .cap = CLI_CELLS_POOL,
                                                  .other = "`gone/SConscript`",
                                                  .other_cap = CLI_CELLS_POOL}) != NULL),
              "each script named that is not there is a finding");
    found = scons_checked("src/SConscript", s_sub, &json);
    const unsigned int tools = cli_json_item(json, found, 0u);
    cli_check((json->node[found].count == 1u) &&
                  (cellul.eq(&(CatenaFinitaCfg){
                      .src = scons_message(json, tools),
                      .other = "`tools` is imported here, and no SConstruct or SConscript of the tree exports it",
                      .cap = CLI_CELLS_POOL})),
              "a name imported that nothing exports is a finding");
    cli_check((scons_at(json, tools, "start", "line") == 0ull) &&
                  (scons_at(json, tools, "start", "character") == 17ull) &&
                  (scons_at(json, tools, "end", "character") == 22ull),
              "the finding stands on the name");
    cli_full(s_dir, "src/SConscript", path);
    SconsGiven libs = {"libs", 0};
    SconsGiven program = {"Program", 0};
    cli_build_given_names(path, s_sub, sizeof(s_sub) - 1u, scons_given, &libs);
    cli_build_given_names(path, s_sub, sizeof(s_sub) - 1u, scons_given, &program);
    cli_check(libs.found && program.found, "the names imported and SCons's own are given");
    cli_check(scons_completes("env = Envi", "Environment"), "SCons's names complete");
    cli_check(scons_completes("env.App", "AppendUnique"), "an environment's methods complete after a dot");
    cli_check(scons_completes("env['CPP", "CPPPATH"), "construction variables complete in brackets");
    cli_check(scons_completes("env = Environment(CPP", "CPPPATH"), "and as keywords of the calls that set them");
    cli_check(!scons_completes("x = Environment(tools=foo(CPP", "CPPPATH"), "and not in a call within the call");
    (void)scons_completes("x = 'Envi", "Environment");
    cli_check(s_items->count == 0u, "nothing completes in a string");
    // the scons checks report their own mark: the most a call's rooms held of the cellblock at once
    char report[160];
    CliLine said = {report, sizeof(report), 0ull};
    cli_line_text(&said, "the scons checks count each call's rooms in the cellblock, their report ");
    const unsigned long long numbers = said.at;
    cli_scons_cells_said(&said);
    (void)cli_line_closed(&said);
    const char *const mark =
        cellul.chr(&(CatenaFinitaCfg){.src = report + numbers, .cap = CLI_CELLS_POOL, .byte = ' '});
    cli_check((mark != NULL) && (mark[1] != '0'), report);
    return cli_checked();
}
