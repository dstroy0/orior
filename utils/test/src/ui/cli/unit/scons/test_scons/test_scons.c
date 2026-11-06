// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_scons.c: SCons scripts are read and the names they import checked, and SCons's names complete
#include "ui/cli/src/builds/builds.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/builds/scons_internal.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static const char s_top[] = "env = Environment(CPPPATH=['include'])\nExport('env')\nSConscript('src/SConscript')\n"
                            "SConscript(['lib/SConscript'], exports='libs')\nSConscript(dirs=['gone'])\n"
                            "# SConscript('commented')\n";
static const char s_sub[] = "Import('env libs tools')\nenv.Program('hello', ['hello.c'])\n";

static char s_dir[CLI_PATH_MAX_LEN];
static char s_out[1u << 18u];
static const CliJson *s_json;

// the findings of the file `name` of the folder, `text` its text, read into the shared tree: its node
static unsigned int findings_of(const char *name, const char *text)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_dir, .relative = name, .joined = path);
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_builds_path.build_check, CliBuildsPathCfg, .root = s_dir, .path = path, .text = text, .size = CLI_UNITY_LENGTH(text), .out = &out);
    unsigned int found = CLI_JSON_NONE;
    s_json = cli_unity_parsed(s_out, out.at, &found);
    return found;
}

// the message of a finding
static const char *message_of(unsigned int finding)
{
    return cli_unity_member_text(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = finding, .name = "message"), "value");
}

// a whole number of the range's `end` or `start`
static unsigned long long range_number(unsigned int finding, const char *end, const char *name)
{
    const unsigned int number = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = finding, .name = "range"), .name = end), .name = name);
    unsigned long long value = 0ull;
    for (unsigned long long at = 0ull; EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = s_json, .node = number, .kind = CFG_JSON_NUMBER) && (at < s_json->node[number].length); at += 1ull)
    {
        value = value * 10ull + (unsigned long long)(s_json->text[s_json->node[number].text + at] - '0');
    }
    return value;
}

// A name SCons defines for a script, and whether it was found
typedef struct
{
    const char *required;
    int found;
} SconsName;

static void name_defined(const CliEachNameSizeCfg *args)
{
    SconsName *const defined = args->context;
    defined->found |= (CLI_UNITY_LENGTH(defined->required) == args->size) && (memor.cmp(&(MemoriaCfg){.src = args->name, .other = defined->required, .bytes = args->size}) == 0);
}

// what completes `text` at its end in the top script
static const CliCompletions *completed(const char *text)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_dir, .relative = "SConstruct", .joined = path);
    return cli_unity_build_completed(s_dir, path, text);
}

void setUp(void)
{
    char src[CLI_PATH_MAX_LEN];
    cli_unity_begun();
    cli_unity_folder("scons", s_dir);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_dir, .relative = "src", .joined = src);
    TEST_ASSERT_TRUE(EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = src));
    cli_unity_file(s_dir, "SConstruct", s_top);
    cli_unity_file(s_dir, "src/SConscript", s_sub);
}

void tearDown(void)
{
}

void test_each_named_script_that_is_missing_is_a_finding(void)
{
    const unsigned int found = findings_of("SConstruct", s_top);
    TEST_ASSERT_EQUAL_UINT(2u, s_json->node[found].count);
    CLI_TEST_ASSERT_CONTAINS("`lib/SConscript`", message_of(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = found, .index = 0u)));
    CLI_TEST_ASSERT_CONTAINS("`gone/SConscript`", message_of(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = found, .index = 1u)));
}

void test_an_imported_name_nothing_exports_is_a_finding(void)
{
    const unsigned int found = findings_of("src/SConscript", s_sub);
    TEST_ASSERT_EQUAL_UINT(1u, s_json->node[found].count);
    TEST_ASSERT_EQUAL_STRING("`tools` is imported here, and no SConstruct or SConscript of the tree exports it", message_of(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = found, .index = 0u)));
}

void test_the_finding_is_positioned_on_the_name(void)
{
    const unsigned int found = findings_of("src/SConscript", s_sub);
    const unsigned int finding = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = found, .index = 0u);
    TEST_ASSERT_EQUAL_UINT64(0ull, range_number(finding, "start", "line"));
    TEST_ASSERT_EQUAL_UINT64(17ull, range_number(finding, "start", "character"));
    TEST_ASSERT_EQUAL_UINT64(22ull, range_number(finding, "end", "character"));
}

void test_an_imported_name_is_defined_for_the_script(void)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_dir, .relative = "src/SConscript", .joined = path);
    SconsName name = {"libs", 0};
    EMBED_CALL(cli_builds_path_text.build_input_names, CliBuildsPathTextCfg, .path = path, .text = s_sub, .size = sizeof(s_sub) - 1u, .each = name_defined, .context = &name);
    TEST_ASSERT_TRUE(name.found);
}

void test_sconss_own_name_is_defined_for_the_script(void)
{
    char path[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = s_dir, .relative = "src/SConscript", .joined = path);
    SconsName name = {"Program", 0};
    EMBED_CALL(cli_builds_path_text.build_input_names, CliBuildsPathTextCfg, .path = path, .text = s_sub, .size = sizeof(s_sub) - 1u, .each = name_defined, .context = &name);
    TEST_ASSERT_TRUE(name.found);
}

void test_sconss_names_complete(void)
{
    TEST_ASSERT_TRUE(cli_unity_completes(completed("env = Envi"), "Environment"));
}

void test_an_environments_methods_complete_after_a_dot(void)
{
    TEST_ASSERT_TRUE(cli_unity_completes(completed("env.App"), "AppendUnique"));
}

void test_construction_variables_complete_in_brackets(void)
{
    TEST_ASSERT_TRUE(cli_unity_completes(completed("env['CPP"), "CPPPATH"));
}

void test_construction_variables_complete_as_keywords(void)
{
    TEST_ASSERT_TRUE(cli_unity_completes(completed("env = Environment(CPP"), "CPPPATH"));
}

void test_construction_variables_do_not_complete_in_a_nested_call(void)
{
    TEST_ASSERT_FALSE(cli_unity_completes(completed("x = Environment(tools=foo(CPP"), "CPPPATH"));
}

void test_nothing_completes_in_a_string(void)
{
    TEST_ASSERT_EQUAL_UINT(0u, completed("x = 'Envi")->count);
}

void test_the_scons_checks_report_their_peak_memory(void)
{
    (void)findings_of("src/SConscript", s_sub);
    char report[160];
    VerbaLinea line = {report, sizeof(report), 0ull};
    EMBED_CALL(cli_scons_internal.cells_usage_write, CliSconsCtx, .out = &line);
    (void)mmgr_line_finish(&line);
    const char *const peak = cellul.chr(&(CatenaFinitaCfg){.src = report, .cap = CLI_POOL_BYTES, .byte = ' '});
    TEST_ASSERT_NOT_NULL(peak);
    TEST_ASSERT_NOT_EQUAL('0', peak[1]);
}
