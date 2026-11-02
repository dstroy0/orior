// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_inspect_tree.c: what the tree reports across its files: a function's callers and callees in Python and JavaScript, an import cycle in both,
// and a method a mixin supplies against one no class sets
#include "ui/cli/src/inspect/inspect_tree.h"
#include "../../../../../../../../ui/cli/src/command_line/callback_cfg.h"
#include "ui/cli/src/text/text.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

// what a walk of the tree received: each name, or each path and message, and the count of the calls of the first
typedef struct
{
    char item[64][1024];
    unsigned int count;
    unsigned int first_sites;
} TreeFindings;

static TreeFindings s_findings;

static const char *const s_hierarchy_files[4][2] = {
    {"geo/area.py", "def square(side):\n    return side * side\n\n\ndef ring(a, b):\n    def inner(x):\n        return "
                    "square(x)\n    return square(a) - square(b) + inner(1)\n"},
    {"geo/use.py", "from geo.area import ring\n\n\nclass Shape:\n    def total(self):\n        return ring(2, 1) + "
                   "self.part()\n\n    def part(self):\n        return 1\n"},
    {"web/lib.js", "export function twice(x) {\n  return x * 2;\n}\n\nexport const four = () => {\n  return twice(2);\n};\n"},
    {"web/main.js", "import { twice, four } from \"./lib.js\";\nclass App {\n  run() {\n    return twice(four()) + "
                    "this.more();\n  }\n  more() {\n    return 1;\n  }\n}\n"}};

static const char *const s_cycle_files[8][2] = {{"pkg/__init__.py", ""},
                                                {"pkg/a.py", "from pkg import b\n\ndef f():\n    from pkg import c\n"},
                                                {"pkg/b.py", "from . import a\n"},
                                                {"pkg/c.py", "import pkg.a\n"},
                                                {"pkg/d.py", "import os\nfrom typing import TYPE_CHECKING\nif TYPE_CHECKING:\n    from pkg import a\n"},
                                                {"web/x.js", "import { y } from \"./y.js\";\n"},
                                                {"web/y.js", "export { x } from \"./x\";\n"},
                                                {"web/z.js", "import { x } from \"./x.js\";\n"}};

static const char *const s_mixin_files[3][2] = {
    {"shapes/mixins.py", "class Saving:\n    def save(self):\n        return self.render() + self.missing()\n\nclass "
                         "Drawing:\n    def draw(self):\n        return self.render()\n"},
    {"shapes/square.py", "from shapes.mixins import Saving\nimport shapes.mixins as mixins\n\nclass Square(Saving, "
                         "mixins.Drawing):\n    def __init__(self):\n        self.side = self.edge = 2\n\n    def "
                         "render(self):\n        hook = self.hook() if getattr(self, \"hook\", None) else 0\n        "
                         "return self.side * self.edge * self.save() + self.draw() + self.colour + hook\n"},
    {"shapes/outside.py", "from somewhere import Base\n\nclass Loose(Base):\n    def go(self):\n        return "
                          "self.anything()\n"}};

// the files of `files`, `count` of them, each a path and its text, set in a tree made empty
static void tree_of(const char *const (*files)[2], unsigned int count)
{
    cli_inspect_tree_clear();
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const char *const language = EMBED_CALL(cli_inspect_table.language_get, CliInspectCfg, .path = files[at][0]);
        TEST_ASSERT_NOT_NULL_MESSAGE(language, files[at][0]);
        TEST_ASSERT_TRUE_MESSAGE(EMBED_CALL(cli_inspect_tree_path_language.set, CliInspectTreePathLanguageCfg, .path = files[at][0], .language = language, .text = files[at][1], .size = CLI_UNITY_LENGTH(files[at][1])), files[at][0]);
    }
}

// a function reached recorded by its name
static void tree_reached(const CliInspectReachedCfg *args)
{
    unsigned int count = args->count;
    if (s_findings.count < 64u)
    {
        if (s_findings.count == 0u)
        {
            s_findings.first_sites = count;
        }
        const char *const name = EMBED_CALL(cli_inspect_pool.text, CliInspectPoolCfg, .pool = args->facts->text, .text_inspect_text = args->facts->function[args->function].name);
        memor.cpy(&(MemoriaCfg){.dst = s_findings.item[s_findings.count], .src = name, .bytes = CLI_UNITY_LENGTH(name) + 1u});
        s_findings.count += 1u;
    }
}

// what was received, joined by `|`
static const char *tree_findings_joined(void)
{
    static char s_joined[8192];
    VerbaLinea line = {s_joined, sizeof(s_joined), 0ull};
    for (unsigned int at = 0u; at < s_findings.count; at += 1u)
    {
        mmgr_line_put(&line, (at > 0u) ? "|" : "");
        mmgr_line_put(&line, s_findings.item[at]);
    }
    (void)mmgr_line_finish(&line);
    return s_joined;
}

// the name of the function at `at` of `path`
static const char *tree_name(const char *path, unsigned int at)
{
    s_findings.count = 0u;
    (void)EMBED_CALL(cli_inspect_tree_path.function, CliInspectTreePathCfg, .path = path, .at = at, .each = tree_reached, .context = NULL);
    return tree_findings_joined();
}

// the callers, or the callees, of the function at `at` of `path`, joined by `|`
static const char *tree_calls(const char *path, unsigned int at, int callers)
{
    s_findings.count = 0u;
    s_findings.first_sites = 0u;
    if (callers)
    {
        EMBED_CALL(cli_inspect_tree_path.callers, CliInspectTreePathCfg, .path = path, .at = at, .each = tree_reached, .context = NULL);
    }
    else
    {
        EMBED_CALL(cli_inspect_tree_path.callees, CliInspectTreePathCfg, .path = path, .at = at, .each = tree_reached, .context = NULL);
    }
    return tree_findings_joined();
}

// a finding recorded as `path` then its message
static void tree_found(const CliInspectFoundCfg *args)
{
    if ((args->finding == NULL) || (args->size == 0ull) || (s_findings.count >= 64u))
    {
        return;
    }
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(args->finding, args->size, &read);
    VerbaLinea line = {s_findings.item[s_findings.count], sizeof(s_findings.item[0]), 0ull};
    mmgr_line_put(&line, args->path);
    mmgr_line_ch(&line, '\t');
    mmgr_line_put(&line, cli_unity_member_text(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json, .object = read, .name = "message"), "value"));
    (void)mmgr_line_finish(&line);
    s_findings.count += 1u;
}

// the messages found in `path`, joined by `|`
static const char *tree_messages(const char *path)
{
    static char s_messages[8192];
    VerbaLinea line = {s_messages, sizeof(s_messages), 0ull};
    const unsigned long long length = CLI_UNITY_LENGTH(path);
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < s_findings.count; at += 1u)
    {
        if (EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = s_findings.item[at], .other_pointer = path, .size = length) && (s_findings.item[at][length] == '\t'))
        {
            mmgr_line_put(&line, (count > 0u) ? "|" : "");
            mmgr_line_put(&line, s_findings.item[at] + length + 1u);
            count += 1u;
        }
    }
    (void)mmgr_line_finish(&line);
    return s_messages;
}

// every finding of the tree received
static void tree_findings(void)
{
    s_findings.count = 0u;
    EMBED_CALL(cli_inspect_tree.findings, CliInspectTreeCfg, .each_inspect_found = tree_found, .context = NULL);
}

// an attribute a class's family supplies recorded as `path|class|attribute`
static void tree_supplied(const CliInspectSuppliedCfg *args)
{
    if (s_findings.count < 64u)
    {
        VerbaLinea line = {s_findings.item[s_findings.count], sizeof(s_findings.item[0]), 0ull};
        mmgr_line_put(&line, args->path);
        mmgr_line_ch(&line, '|');
        mmgr_line_put(&line, args->class_name);
        mmgr_line_ch(&line, '|');
        mmgr_line_put(&line, args->attribute);
        (void)mmgr_line_finish(&line);
        s_findings.count += 1u;
    }
}

// whether what was received contains `required`
static int tree_findings_has(const char *required)
{
    for (unsigned int at = 0u; at < s_findings.count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = s_findings.item[at], .other = required, .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    return 0;
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_a_function_is_found_by_the_line_of_its_name(void)
{
    unsigned int square = 7u;
    tree_of(s_hierarchy_files, 4u);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_inspect_tree_path_line.function_on, CliInspectTreePathLineCfg, .path = "geo/area.py", .line = 0u, .at_pointer = &square));
    TEST_ASSERT_EQUAL_STRING("square", tree_name("geo/area.py", square));
}

void test_a_function_is_found_by_a_line_it_contains(void)
{
    unsigned int ring = 0u;
    tree_of(s_hierarchy_files, 4u);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_inspect_tree_path_line.function_at, CliInspectTreePathLineCfg, .path = "geo/area.py", .line = 7u, .at_pointer = &ring));
    TEST_ASSERT_EQUAL_STRING("ring", tree_name("geo/area.py", ring));
}

void test_a_functions_callers_are_found(void)
{
    unsigned int square = 0u;
    tree_of(s_hierarchy_files, 4u);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_inspect_tree_path_line.function_on, CliInspectTreePathLineCfg, .path = "geo/area.py", .line = 0u, .at_pointer = &square));
    TEST_ASSERT_EQUAL_STRING("ring|inner", tree_calls("geo/area.py", square, 1));
}

void test_a_functions_callees_count_both_calls_and_not_the_one_inner_makes(void)
{
    unsigned int ring = 0u;
    tree_of(s_hierarchy_files, 4u);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_inspect_tree_path_line.function_at, CliInspectTreePathLineCfg, .path = "geo/area.py", .line = 7u, .at_pointer = &ring));
    TEST_ASSERT_EQUAL_STRING("square|inner", tree_calls("geo/area.py", ring, 0));
    TEST_ASSERT_EQUAL_UINT(2u, s_findings.first_sites);
}

void test_a_caller_in_another_file_is_found(void)
{
    unsigned int ring = 0u;
    tree_of(s_hierarchy_files, 4u);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_inspect_tree_path_line.function_at, CliInspectTreePathLineCfg, .path = "geo/area.py", .line = 7u, .at_pointer = &ring));
    TEST_ASSERT_EQUAL_STRING("total", tree_calls("geo/area.py", ring, 1));
}

void test_a_methods_callees_include_its_own_classes(void)
{
    unsigned int total = 0u;
    tree_of(s_hierarchy_files, 4u);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_inspect_tree_path_line.function_on, CliInspectTreePathLineCfg, .path = "geo/use.py", .line = 4u, .at_pointer = &total));
    TEST_ASSERT_EQUAL_STRING("ring|part", tree_calls("geo/use.py", total, 0));
}

void test_a_javascript_functions_callers_are_found(void)
{
    unsigned int twice = 0u;
    tree_of(s_hierarchy_files, 4u);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_inspect_tree_path_line.function_on, CliInspectTreePathLineCfg, .path = "web/lib.js", .line = 0u, .at_pointer = &twice));
    TEST_ASSERT_EQUAL_STRING("four|run", tree_calls("web/lib.js", twice, 1));
}

void test_a_javascript_methods_callees_are_found(void)
{
    unsigned int run = 0u;
    tree_of(s_hierarchy_files, 4u);
    TEST_ASSERT_TRUE(EMBED_CALL(cli_inspect_tree_path_line.function_at, CliInspectTreePathLineCfg, .path = "web/main.js", .line = 3u, .at_pointer = &run));
    TEST_ASSERT_EQUAL_STRING("twice|four|more", tree_calls("web/main.js", run, 0));
}

void test_an_import_cycle_names_the_files_it_passes_through(void)
{
    tree_of(s_cycle_files, 8u);
    tree_findings();
    TEST_ASSERT_EQUAL_STRING("This import leads back round to the file that makes it, by way of:\n\n1. `pkg/a.py`\n2. `pkg/b.py`\n3. `pkg/a.py`", tree_messages("pkg/a.py"));
}

void test_an_import_cycle_is_reported_once_in_its_other_file(void)
{
    tree_of(s_cycle_files, 8u);
    tree_findings();
    TEST_ASSERT_NOT_EQUAL('\0', tree_messages("pkg/b.py")[0]);
    CLI_TEST_ASSERT_NOT_CONTAINS("|", tree_messages("pkg/b.py"));
}

void test_an_import_inside_a_function_is_not_a_cycle(void)
{
    tree_of(s_cycle_files, 8u);
    tree_findings();
    TEST_ASSERT_EQUAL_STRING("", tree_messages("pkg/c.py"));
}

void test_an_import_only_a_type_checker_makes_is_not_a_cycle(void)
{
    tree_of(s_cycle_files, 8u);
    tree_findings();
    TEST_ASSERT_EQUAL_STRING("", tree_messages("pkg/d.py"));
}

void test_a_javascript_cycle_includes_a_reexport(void)
{
    tree_of(s_cycle_files, 8u);
    tree_findings();
    TEST_ASSERT_EQUAL_STRING("This import leads back round to the file that makes it, by way of:\n\n1. `web/x.js`\n2. `web/y.js`\n3. `web/x.js`", tree_messages("web/x.js"));
}

void test_a_file_that_only_reaches_a_cycle_is_not_in_it(void)
{
    tree_of(s_cycle_files, 8u);
    tree_findings();
    TEST_ASSERT_EQUAL_STRING("", tree_messages("web/z.js"));
}

void test_the_class_a_mixin_is_mixed_into_supplies_its_method(void)
{
    tree_of(s_mixin_files, 3u);
    s_findings.count = 0u;
    EMBED_CALL(cli_inspect_tree_context_each_supplied.supplied, CliInspectTreeContextEachSuppliedCfg, .each_inspect_supplied = tree_supplied, .context = NULL);
    TEST_ASSERT_TRUE(tree_findings_has("shapes/mixins.py|Saving|render"));
}

void test_a_method_no_class_sets_is_not_supplied(void)
{
    tree_of(s_mixin_files, 3u);
    s_findings.count = 0u;
    EMBED_CALL(cli_inspect_tree_context_each_supplied.supplied, CliInspectTreeContextEachSuppliedCfg, .each_inspect_supplied = tree_supplied, .context = NULL);
    TEST_ASSERT_FALSE(tree_findings_has("shapes/mixins.py|Saving|missing"));
}

void test_an_attribute_no_class_of_a_mixins_family_sets_is_found(void)
{
    tree_of(s_mixin_files, 3u);
    tree_findings();
    TEST_ASSERT_EQUAL_STRING("`Saving` has no `missing`: neither it, a class it comes from, nor a class that comes from it sets `missing`.", tree_messages("shapes/mixins.py"));
}

void test_an_attribute_of_the_class_a_mixin_is_mixed_into_is_found(void)
{
    tree_of(s_mixin_files, 3u);
    tree_findings();
    TEST_ASSERT_EQUAL_STRING("`Square` has no `colour`: neither it, a class it comes from, nor a class that comes from it sets `colour`.", tree_messages("shapes/square.py"));
}

void test_a_class_whose_family_is_outside_the_tree_reports_nothing(void)
{
    tree_of(s_mixin_files, 3u);
    tree_findings();
    TEST_ASSERT_EQUAL_STRING("", tree_messages("shapes/outside.py"));
}
