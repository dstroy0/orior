// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// gradle_test.c: Gradle's check: a script is checked, braces left open are found, a script completes by its block and
// its catalog, and every script read is given back to the cellblock
#include "../../../../../ui/cli/src/builds/gradle_internal.h"
#include "../../../../../ui/cli/src/builds/version_catalog_internal.h"
#include "cli_check.h"

static char s_out[1u << 18u];

// the file `name` of the folder `dir` written with `text`, in its own `gradle` folder where it is one
static void gradle_file(const char *dir, const char *name, const char *text)
{
    char path[CLI_PATH_LONGEST];
    cli_full(dir, "gradle", path);
    (void)query_folder_made(path);
    cli_full(dir, name, path);
    (void)query_file_written(path, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

// the findings of the script at `path` of the tree at `root`, `text` its text: the array's node in the shared tree
static unsigned int gradle_checked(const char *root, const char *path, const char *text, const CliJson **json)
{
    CliLine out = {s_out, sizeof(s_out), 0ull};
    cli_build_check(root, path, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), &out);
    unsigned int found = CLI_JSON_NONE;
    *json = cli_check_parsed(s_out, out.at, &found);
    return found;
}

// a script is checked
static void gradle_script(void)
{
    static const char s_script[] =
        "plugins {\n    id(\"java\")\n}\n\ndependencies {\n    implementation(libs.groovy.core)\n"
        "    implementation(libs.groovy.cor)\n    compile(\"a:b:1\")\n    implmentation(\"a:b:1\")\n"
        "    testImplementation(\"org.junit:junit:4.13\")\n    api(\"onlyone\")\n    "
        "runtimeOnly(\"g:a:2.0-SNAPSHOT\")\n"
        "    implementation(libs.versions.groovy.get())\n    integrationTestImplementation(\"x:y:1\")\n"
        "    constraints {\n        implementation(\"c:d:1\")\n    }\n}\n// } a brace in a comment\nval s = \"{\"\n";
    char root[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    cli_check_folder("gradle_check", root);
    gradle_file(root, "settings.gradle.kts", "rootProject.name = \"x\"\n");
    gradle_file(
        root, "gradle/libs.versions.toml",
        "[versions]\ngroovy = \"3\"\n[libraries]\ngroovy-core = { module = \"g:groovy\", version.ref = \"groovy\" }\n");
    gradle_file(root, "build.gradle.kts", s_script);
    cli_full(root, "build.gradle.kts", path);
    const CliJson *json = NULL;
    const unsigned int found = gradle_checked(root, path, s_script, &json);
    const unsigned int cor = cli_check_finding(found, "gradle-catalog", "libs.groovy.cor");
    cli_check(cor != CLI_JSON_NONE, "an alias the catalog does not give is found");
    cli_check(cli_check_finding(found, "gradle-configuration", "`compile`") != CLI_JSON_NONE,
              "a configuration Gradle took out is found");
    const unsigned int typo = cli_check_finding(found, "gradle-configuration", "`implmentation`");
    cli_check(typo != CLI_JSON_NONE, "a configuration no plugin makes is found");
    cli_check(cli_check_finding(found, "gradle-notation", "onlyone") != CLI_JSON_NONE,
              "a dependency of one part is found");
    cli_check(cli_check_finding(found, "snapshot", NULL) != CLI_JSON_NONE, "a snapshot is found");
    cli_check(json->node[found].count == 5u, "and nothing more is found");
    const unsigned int typo_fix =
        cli_json_item(json, cli_json_member(json, cli_json_member(json, typo, "data"), "orior"), 0u);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = cli_check_member_text(typo_fix, "title"),
                                           .other = "Change to implementation",
                                           .cap = CLI_CELLS_POOL}),
              "the nearest configuration is offered");
    const unsigned int cor_fix =
        cli_json_item(json, cli_json_member(json, cli_json_member(json, cor, "data"), "orior"), 0u);
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){
            .src = cli_check_member_text(cli_json_item(json, cli_json_member(json, cor_fix, "edits"), 0u), "text"),
            .other = "groovy.core",
            .cap = CLI_CELLS_POOL}),
        "and the nearest alias written over it");
}

// braces left open are found
static void gradle_braces(void)
{
    const CliJson *json = NULL;
    const unsigned int found =
        gradle_checked("/none", "/none/build.gradle", "dependencies {\n  implementation 'a:b:1'\n", &json);
    cli_check((json->node[found].count == 1u) && (cli_check_finding(found, NULL, "`dependencies`") != CLI_JSON_NONE),
              "a block left open is the one finding");
}

// a script completes by its block and its catalog
static void gradle_completes(void)
{
    char root[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    cli_check_folder("gradle_complete", root);
    gradle_file(root, "gradle/libs.versions.toml",
                "[versions]\ngroovy = \"3\"\n[libraries]\ngroovy-core = \"g:groovy:3\"\ngroovy-json = \"g:json:3\"\n"
                "[plugins]\nshadow = \"com.github.johnrengelman.shadow:8\"\n");
    cli_full(root, "build.gradle", path);
    const CliCompletions *items =
        cli_check_build_completed(root, path, "dependencies {\n    implementation libs.groovy.");
    cli_check((items->count == 2u) && cli_check_completes(items, "core") && cli_check_completes(items, "json"),
              "a catalog's aliases complete after a dot");
    cli_check(cli_check_completes(cli_check_build_completed(root, path, "dependencies {\n    implementation libs."),
                                  "versions.groovy"),
              "and its versions");
    cli_check(
        cli_check_completes(cli_check_build_completed(root, path, "plugins {\n    alias(libs.plugins.sh"), "shadow"),
        "and its plugins");
    cli_check(cli_check_completes(cli_check_build_completed(root, path, "dependencies {\n    impl"), "implementation"),
              "a block's configurations complete");
    cli_check(cli_check_completes(cli_check_build_completed(root, path, "repositories {\n    mav"), "mavenCentral"),
              "a block's own calls complete");
    cli_check(cli_check_completes(cli_check_build_completed(root, path, "dep"), "dependencies"),
              "the script's blocks complete");
    cli_check(cli_check_build_completed(root, path, "x = \"dep")->count == 0u, "nothing completes in a string");
}

int main(void)
{
    gradle_script();
    gradle_braces();
    gradle_completes();
    cli_check((g_cli_gradle_cells.holds == 0ull) && (g_cli_gradle_cells.hw != 0ull),
              "every script read is given back, the most the scripts held marked");
    cli_check(g_cli_versions_cells.holds == 0ull, "and every catalog's aliases");
    cli_check((g_cli_build_cells.holds == 0ull) && (g_cli_build_cells.hw != 0ull),
              "and the findings and the lines indexed, once the findings are written");
    return cli_checked();
}
