// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// maven_test.c: Maven's check: a POM is checked, broken XML is found, a POM completes its elements, properties and
// values, and the compiler's settings are read and set
#include "../../../../../ui/cli/src/builds/maven.h"
#include "cli_check.h"

static const char s_pom[] =
    "<?xml version=\"1.0\"?>\n<project>\n  <modelVersion>4.0.0</modelVersion>\n  <groupId>g</groupId>\n"
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

// the findings of `text`, the POM at `path` of the tree at `root`, read into the shared tree: the array's node
static unsigned int maven_checked(const char *root, const char *path, const char *text, const CliJson **json)
{
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_build_check(root, path, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), &out);
    unsigned int found = CLI_JSON_NONE;
    *json = cli_check_parsed(s_out, out.at, &found);
    return found;
}

// the first fix of a finding
static unsigned int maven_fix(const CliJson *json, unsigned int finding)
{
    return cli_json_item(json, cli_json_member(json, cli_json_member(json, finding, "data"), "orior"), 0u);
}

// a POM is checked
static void maven_pom(void)
{
    static const char *const s_has[8][2] = {{"pom-property", "junit.verison"}, {"pom-scope", "tset"},
                                            {"pom-version", "x:loose"},        {"snapshot", "s:snap:1.1-SNAPSHOT"},
                                            {"pom-duplicate", "s:snap"},       {"pom-dependency", "groupId"},
                                            {"pom-phase", "compil"},           {"pom-element", "finalname"}};
    const CliJson *json = NULL;
    const unsigned int found = maven_checked("/tree", "/tree/pom.xml", s_pom, &json);
    int has = 1;
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        has &= cli_check_finding(found, s_has[at][0], s_has[at][1]) != CLI_JSON_NONE;
    }
    cli_check(has, "each fault of the POM is found");
    cli_check(json->node[found].count == 9u, "and nothing more");
    const unsigned int element = maven_fix(json, cli_check_finding(found, "pom-element", NULL));
    cli_check(json->node[cli_json_member(json, element, "edits")].count == 2u,
              "the opening and the closing tag are both named again");
    const unsigned int property = maven_fix(json, cli_check_finding(found, "pom-property", NULL));
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){
            .src = cli_check_member_text(cli_json_item(json, cli_json_member(json, property, "edits"), 0u), "text"),
            .other = "junit.version",
            .cap = CLI_CELLS_POOL}),
        "a property is offered by its nearest name");
    const unsigned int snap = maven_fix(json, cli_check_finding(found, "snapshot", NULL));
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = cli_check_member_text(cli_json_member(json, snap, "run"), "line"),
                                           .other = "mvn -U dependency:get -Dartifact=s:snap:1.1-SNAPSHOT",
                                           .cap = CLI_CELLS_POOL}),
              "a snapshot offers the line that fetches its newest build");
    const unsigned int broken = maven_checked("/t", "/t/pom.xml", "<project>\n  <dependencies>\n  </project>\n", &json);
    cli_check(cli_check_finding(broken, NULL, "`<dependencies>` is not closed") != CLI_JSON_NONE,
              "broken XML is found");
}

// the item of `items` that shows `label`, NULL where none does
static const CliCompletion *maven_item(const CliCompletions *items, const char *label)
{
    for (unsigned int at = 0u; at < items->count; at += 1u)
    {
        const CliCompletion *const item = &items->item[at];
        if ((item->label_size == cellul.len(&(CatenaFinitaCfg){.src = label, .cap = CLI_CELLS_POOL})) &&
            (memor.cmp(&(MemoriaCfg){.src = items->text + item->label_at, .other = label, .bytes = item->label_size}) ==
             0))
        {
            return item;
        }
    }
    return NULL;
}

// whether an item inserts `wanted`
static int maven_inserts(const CliCompletions *items, const CliCompletion *item, const char *wanted)
{
    return (item != NULL) &&
           (item->insert_size == cellul.len(&(CatenaFinitaCfg){.src = wanted, .cap = CLI_CELLS_POOL})) &&
           (memor.cmp(
                &(MemoriaCfg){.src = items->text + item->insert_at, .other = wanted, .bytes = item->insert_size}) == 0);
}

// a POM completes its elements, properties and values
static void maven_completes(void)
{
    const CliCompletions *items =
        cli_check_build_completed("/t", "/t/pom.xml", "<project>\n  <dependencies>\n    <dependency>\n      <gr");
    const CliCompletion *const group = maven_item(items, "groupId");
    cli_check(maven_inserts(items, group, "groupId>$0</groupId>") && group->snippet, "groupId is offered as a snippet");
    items = cli_check_build_completed("/t", "/t/pom.xml", "<project>\n  <build>\n  </");
    cli_check((items->count > 0u) && maven_inserts(items, &items->item[0], "build>"), "a closing tag is offered");
    items =
        cli_check_build_completed("/t", "/t/pom.xml", "<project><properties><a.b>1</a.b></properties><version>${a.");
    cli_check((items->count == 1u) && (maven_item(items, "b") != NULL), "a property completes after its dot");
    items = cli_check_build_completed("/t", "/t/pom.xml", "<project><dependencies><dependency><scope>te");
    cli_check(maven_item(items, "test") != NULL, "a scope completes");
    items = cli_check_build_completed("/t", "/t/pom.xml", "<project><properties><maven.");
    cli_check(
        maven_inserts(items, maven_item(items, "compiler.release"), "compiler.release>$0</maven.compiler.release>"),
        "a compiler property completes inside the properties");
}

// the value of the setting `key` in the rows of the shared tree, the empty text where it has none
static const char *maven_row(const CliJson *json, unsigned int rows, const char *key)
{
    for (unsigned int at = 0u; cli_json_item(json, rows, at) != CLI_JSON_NONE; at += 1u)
    {
        const unsigned int row = cli_json_item(json, rows, at);
        if (cellul.eq(
                &(CatenaFinitaCfg){.src = cli_check_member_text(row, "key"), .other = key, .cap = CLI_CELLS_POOL}))
        {
            return cli_check_member_text(row, "value");
        }
    }
    return "";
}

// a whole number member of a node of the shared tree
static unsigned int maven_whole(const CliJson *json, unsigned int node, const char *name)
{
    const unsigned int number = cli_json_member(json, node, name);
    unsigned int value = 0u;
    for (unsigned long long at = 0ull; cli_json_is(json, number, CFG_JSON_NUMBER) && (at < json->node[number].length);
         at += 1ull)
    {
        value = value * 10u + (unsigned int)(json->text[json->node[number].text + at] - '0');
    }
    return value;
}

// `text` with the first file's edits the setting `key` set to `value` makes written over it, into s_after
static const char *maven_set(const char *text, const char *key, const char *value)
{
    static CliTextEdit s_edits[16];
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    CliLine out = {s_out, sizeof(s_out), 0ull};
    s_after[0] = '\0';
    if (!cli_maven_set("/t/pom.xml", text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), key,
                       value, &out, &said))
    {
        return s_after;
    }
    unsigned int files = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_out, out.at, &files);
    const unsigned int edits = cli_json_member(json, cli_json_item(json, files, 0u), "edits");
    unsigned int count = 0u;
    for (unsigned int at = 0u; (at < 16u) && (cli_json_item(json, edits, at) != CLI_JSON_NONE); at += 1u)
    {
        const unsigned int edit = cli_json_item(json, edits, at);
        const unsigned int from = cli_json_member(json, edit, "from");
        const unsigned int to = cli_json_member(json, edit, "to");
        unsigned long long size = 0ull;
        const char *const written = cli_json_string(json, cli_json_member(json, edit, "text"), &size);
        s_edits[at] = (CliTextEdit){{maven_whole(json, from, "line"), maven_whole(json, from, "col")},
                                    {maven_whole(json, to, "line"), maven_whole(json, to, "col")},
                                    (written != NULL) ? written : "",
                                    size};
        count += 1u;
    }
    CliLine after = {s_after, sizeof(s_after), 0ull};
    (void)cli_text_applied(text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), s_edits, count,
                           &after);
    (void)cli_line_closed(&after);
    return s_after;
}

// the compiler's settings are read and set
static void maven_settings(void)
{
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_maven_settings("/t", "/t/pom.xml", s_pom, sizeof(s_pom) - 1u, &out);
    unsigned int rows = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_out, out.at, &rows);
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){.src = maven_row(json, rows, "release"), .other = "17", .cap = CLI_CELLS_POOL}),
        "the release is read");
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = maven_row(json, rows, "compiler-plugin"), .other = "3.11.0", .cap = CLI_CELLS_POOL}),
              "and the compiler plugin's version");
    cli_check(cellul.find(&(CatenaFinitaCfg){.src = maven_set(s_pom, "release", "21"),
                                             .cap = CLI_CELLS_POOL,
                                             .other = "<release>21</release>",
                                             .other_cap = CLI_CELLS_POOL}) != NULL,
              "a setting is set in place");
    cli_check(
        cellul.find(&(CatenaFinitaCfg){
            .src = maven_set(s_pom, "source", "21"),
            .cap = CLI_CELLS_POOL,
            .other = "    <junit.version>4.13</junit.version>\n    <maven.compiler.source>21</maven.compiler.source>\n"
                     "  </properties>",
            .other_cap = CLI_CELLS_POOL}) != NULL,
        "a setting the POM does not hold is put among its properties");
    static const char s_bare[] =
        "<project>\n  <modelVersion>4.0.0</modelVersion>\n  <artifactId>a</artifactId>\n</project>\n";
    cli_check(
        cellul.find(&(CatenaFinitaCfg){
            .src = maven_set(s_bare, "encoding", "UTF-8"),
            .cap = CLI_CELLS_POOL,
            .other = "<artifactId>a</artifactId>\n\n  <properties>\n    "
                     "<project.build.sourceEncoding>UTF-8</project.build.sourceEncoding>\n  </properties>\n</project>",
            .other_cap = CLI_CELLS_POOL}) != NULL,
        "and its properties made where it has none");
}

int main(void)
{
    maven_pom();
    maven_completes();
    maven_settings();
    return cli_checked();
}
