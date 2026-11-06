// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_maven.c: a POM is checked, broken XML is found, a POM completes its elements, properties and values, and the compiler's settings are
// read and set
#include "ui/cli/src/builds/maven.h"
#include "ui/cli/src/builds/maven_internal.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

#define POM_PATH "/t/pom.xml"

static const char s_pom[] = "<?xml version=\"1.0\"?>\n<project>\n  <modelVersion>4.0.0</modelVersion>\n  <groupId>g</groupId>\n"
                            "  <artifactId>a</artifactId>\n  <version>1.0</version>\n  <properties>\n    <junit.version>4.13</junit.version>\n"
                            "  </properties>\n  <dependencyManagement>\n    <dependencies>\n"
                            "      <dependency><groupId>m</groupId><artifactId>managed</artifactId><version>2</version></dependency>\n"
                            "    </dependencies>\n  </dependencyManagement>\n  <dependencies>\n    <dependency>\n"
                            "      <groupId>junit</groupId>\n      <artifactId>junit</artifactId>\n      <version>${junit.verison}</version>\n"
                            "      <scope>tset</scope>\n    </dependency>\n"
                            "    <dependency><groupId>m</groupId><artifactId>managed</artifactId></dependency>\n"
                            "    <dependency><groupId>x</groupId><artifactId>loose</artifactId></dependency>\n"
                            "    <dependency><groupId>s</groupId><artifactId>snap</artifactId><version>1.1-SNAPSHOT</version></dependency>\n"
                            "    <dependency><groupId>s</groupId><artifactId>snap</artifactId><version>1.1-SNAPSHOT</version></dependency>\n"
                            "    <dependency><artifactId>nogroup</artifactId></dependency>\n  </dependencies>\n  <build>\n    <plugins>\n"
                            "      <plugin>\n        <artifactId>maven-compiler-plugin</artifactId>\n        <version>3.11.0</version>\n"
                            "        <configuration><release>17</release><anything>x</anything></configuration>\n"
                            "        <executions><execution><phase>compil</phase></execution></executions>\n      </plugin>\n"
                            "    </plugins>\n    <finalname>x</finalname>\n  </build>\n  <!-- ${not.checked} -->\n</project>\n";

static char s_out[1u << 18u];
static char s_after[1u << 14u];
static const CliJson *s_json;

// the findings of `text`, the POM at `path` of the tree at `root`, read into the shared tree: the array's node
static unsigned int findings_of(const char *root, const char *path, const char *text)
{
    VerbaLinea out = {s_out, sizeof(s_out), 0ull};
    EMBED_CALL(cli_builds_path.build_check, CliBuildsPathCfg, .root = root, .path = path, .text = text, .size = CLI_UNITY_LENGTH(text), .out = &out);
    unsigned int found = CLI_JSON_NONE;
    s_json = cli_unity_parsed(s_out, out.at, &found);
    return found;
}

// the first fix of a finding
static unsigned int first_fix(unsigned int finding)
{
    return EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = finding, .name = "data"), .name = "orior"), .index = 0u);
}

// the item of `items` that shows `label`, NULL where none does
static const CliCompletion *item_labeled(const CliCompletions *items, const char *label)
{
    for (unsigned int at = 0u; at < items->count; at += 1u)
    {
        const CliCompletion *const item = &items->item[at];
        if ((item->label_size == CLI_UNITY_LENGTH(label)) && (memor.cmp(&(MemoriaCfg){.src = items->text + item->label_at, .other = label, .bytes = item->label_size}) == 0))
        {
            return item;
        }
    }
    return NULL;
}

// the item inserts `required`
static void assert_inserts(const char *required, const CliCompletions *items, const CliCompletion *item)
{
    TEST_ASSERT_NOT_NULL(item);
    CLI_TEST_ASSERT_TEXT(required, items->text + item->insert_at, item->insert_size);
}

// what completes the POM ending in `text`
static const CliCompletions *completed(const char *text)
{
    return cli_unity_build_completed("/t", POM_PATH, text);
}

void setUp(void)
{
    cli_unity_begun();
}

void tearDown(void)
{
}

void test_each_fault_of_the_pom_is_found(void)
{
    static const char *const s_faults[8][2] = {{"pom-property", "junit.verison"}, {"pom-scope", "tset"}, {"pom-version", "x:loose"}, {"snapshot", "s:snap:1.1-SNAPSHOT"}, {"pom-duplicate", "s:snap"}, {"pom-dependency", "groupId"}, {"pom-phase", "compil"}, {"pom-element", "finalname"}};
    const unsigned int found = findings_of("/tree", "/tree/pom.xml", s_pom);
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        TEST_ASSERT_NOT_EQUAL_MESSAGE(CLI_JSON_NONE, cli_unity_finding(found, s_faults[at][0], s_faults[at][1]), s_faults[at][0]);
    }
}

void test_nothing_else_is_found(void)
{
    const unsigned int found = findings_of("/tree", "/tree/pom.xml", s_pom);
    TEST_ASSERT_EQUAL_UINT(9u, s_json->node[found].count);
}

void test_a_misnamed_elements_fix_renames_the_opening_and_closing_tag(void)
{
    const unsigned int found = findings_of("/tree", "/tree/pom.xml", s_pom);
    const unsigned int fix = first_fix(cli_unity_finding(found, "pom-element", NULL));
    TEST_ASSERT_EQUAL_UINT(2u, s_json->node[EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = fix, .name = "edits")].count);
}

void test_a_propertys_fix_offers_its_nearest_name(void)
{
    const unsigned int found = findings_of("/tree", "/tree/pom.xml", s_pom);
    const unsigned int fix = first_fix(cli_unity_finding(found, "pom-property", NULL));
    TEST_ASSERT_EQUAL_STRING("junit.version", cli_unity_member_text(EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = s_json, .array = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = fix, .name = "edits"), .index = 0u), "text"));
}

void test_a_snapshot_offers_the_command_that_fetches_its_newest_build(void)
{
    const unsigned int found = findings_of("/tree", "/tree/pom.xml", s_pom);
    const unsigned int fix = first_fix(cli_unity_finding(found, "snapshot", NULL));
    TEST_ASSERT_EQUAL_STRING("mvn -U dependency:get -Dartifact=s:snap:1.1-SNAPSHOT", cli_unity_member_text(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = s_json, .object = fix, .name = "run"), "line"));
}

void test_broken_xml_is_found(void)
{
    const unsigned int broken = findings_of("/t", POM_PATH, "<project>\n  <dependencies>\n  </project>\n");
    TEST_ASSERT_NOT_EQUAL(CLI_JSON_NONE, cli_unity_finding(broken, NULL, "`<dependencies>` is not closed"));
}

void test_group_id_is_offered_as_a_snippet(void)
{
    const CliCompletions *const items = completed("<project>\n  <dependencies>\n    <dependency>\n      <gr");
    const CliCompletion *const group = item_labeled(items, "groupId");
    assert_inserts("groupId>$0</groupId>", items, group);
    TEST_ASSERT_TRUE(group->snippet);
}

void test_a_closing_tag_is_offered(void)
{
    const CliCompletions *const items = completed("<project>\n  <build>\n  </");
    TEST_ASSERT_GREATER_THAN_UINT(0u, items->count);
    assert_inserts("build>", items, &items->item[0]);
}

void test_a_property_completes_after_its_dot(void)
{
    const CliCompletions *const items = completed("<project><properties><a.b>1</a.b></properties><version>${a.");
    TEST_ASSERT_EQUAL_UINT(1u, items->count);
    TEST_ASSERT_NOT_NULL(item_labeled(items, "b"));
}

void test_a_scope_completes(void)
{
    TEST_ASSERT_NOT_NULL(item_labeled(completed("<project><dependencies><dependency><scope>te"), "test"));
}

void test_a_compiler_property_completes_inside_the_properties(void)
{
    const CliCompletions *const items = completed("<project><properties><maven.");
    assert_inserts("compiler.release>$0</maven.compiler.release>", items, item_labeled(items, "compiler.release"));
}

void test_every_xml_files_memory_is_released(void)
{
    (void)findings_of("/tree", "/tree/pom.xml", s_pom);
    TEST_ASSERT_EQUAL_UINT64(0ull, mmgr_owner_in_use(g_cli_xml_owner));
    TEST_ASSERT_NOT_EQUAL(0ull, mmgr_owner_hw(g_cli_xml_owner));
}

void test_every_models_memory_is_released(void)
{
    (void)findings_of("/tree", "/tree/pom.xml", s_pom);
    TEST_ASSERT_EQUAL_UINT64(0ull, mmgr_owner_in_use(g_cli_maven_model_owner));
    TEST_ASSERT_NOT_EQUAL(0ull, mmgr_owner_hw(g_cli_maven_model_owner));
}
