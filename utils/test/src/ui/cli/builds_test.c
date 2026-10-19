// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// builds_test.c: the build files' shared check: kinds go by the file name, values complete after what was typed, places
// count columns in UTF-16, and near names are offered
#include "cli_check.h"

static CliCompletions s_items;

// what completes `typed` among Maven's two property names, each a value put where it stands after what was typed
static const CliCompletions *builds_relative(const char *typed)
{
    static const char *const s_values[2] = {"maven.compiler.source", "project.version"};
    cli_completions_begun(&s_items);
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        cli_build_relative(&s_items, typed, cellul.len(&(CatenaFinitaCfg){.src = typed, .cap = CLI_CELLS_POOL}),
                           s_values[at], cellul.len(&(CatenaFinitaCfg){.src = s_values[at], .cap = CLI_CELLS_POOL}),
                           "property", "", 0ull);
    }
    return &s_items;
}

// whether the item `at` inserts `wanted`
static int builds_inserts(const CliCompletions *items, unsigned int at, const char *wanted)
{
    return (at < items->count) &&
           (items->item[at].insert_size == cellul.len(&(CatenaFinitaCfg){.src = wanted, .cap = CLI_CELLS_POOL})) &&
           (memor.cmp(&(MemoriaCfg){.src = items->text + items->item[at].insert_at,
                                    .other = wanted,
                                    .bytes = items->item[at].insert_size}) == 0);
}

// places count columns in UTF-16
static void builds_places(void)
{
    static const char s_text[] = "ab\nx\xC3\xA9\xF0\x9F\x98\x80z\n";
    static unsigned int s_starts[8];
    CliBuildLines lines;
    cli_build_lines_of(s_text, sizeof(s_text) - 1u, s_starts, 8u, &lines);
    const char *const z = cellul.chr(&(CatenaFinitaCfg){.src = s_text, .cap = CLI_CELLS_POOL, .byte = 'z'});
    // `z` lies in the text, past its start
    const unsigned long long at = (unsigned long long)(z - s_text);
    const CliTextPlace place = cli_build_place(&lines, at);
    cli_check((place.line == 1u) && (place.col == 4u), "a place counts its column in UTF-16");
    cli_check(cli_build_byte_at(s_text, sizeof(s_text) - 1u, 1u, 4u) == at, "and a column gives its byte back");
}

// the names near `name` offered among `count` of `known`, `|` between them, into `out`, which holds 256
static const char *builds_near(const char *name, const char *const *known, unsigned int count, char *out)
{
    CliBuildNear near;
    cli_build_near_begun(&near, name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_build_near_offered(&near, known[at],
                               cellul.len(&(CatenaFinitaCfg){.src = known[at], .cap = CLI_CELLS_POOL}));
    }
    CliLine line = {out, 256u, 0ull};
    for (unsigned int at = 0u; at < near.count; at += 1u)
    {
        cli_line_text(&line, (at > 0u) ? "|" : "");
        cli_line_counted(&line, near.near[at], near.near_size[at]);
    }
    (void)cli_line_closed(&line);
    return out;
}

int main(void)
{
    cli_check(cli_build_kind_of("a/build.gradle.kts") == CLI_BUILD_GRADLE, "a Gradle script is Gradle's");
    cli_check(cli_build_kind_of("settings.gradle") == CLI_BUILD_GRADLE, "and its settings script");
    cli_check(cli_build_kind_of("gradle/libs.versions.toml") == CLI_BUILD_CATALOG, "a version catalog is a catalog");
    cli_check(cli_build_kind_of("x/pom.xml") == CLI_BUILD_MAVEN, "a POM is Maven's");
    cli_check(cli_build_kind_of("src/SConscript") == CLI_BUILD_SCONS, "a SConscript is SCons's");
    cli_check(cli_build_kind_of("Cargo.toml") == 0u, "and Cargo's manifest is none of them");
    const CliCompletions *items = builds_relative("maven.compiler.so");
    cli_check((items->count == 1u) && builds_inserts(items, 0u, "source"), "a value completes after what was typed");
    cli_check(builds_relative("")->count == 2u, "every value completes nothing typed");
    cli_check(builds_inserts(builds_relative("maven."), 0u, "compiler.source"), "and the rest after a dot");
    builds_places();
    static const char *const s_known[3] = {"implementation", "api", "compileOnly"};
    char near[256];
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = builds_near("implmentation", s_known, 3u, near),
                                           .other = "implementation",
                                           .cap = CLI_CELLS_POOL}),
              "a near name is offered");
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){.src = builds_near("zzz", s_known, 1u, near), .other = "", .cap = CLI_CELLS_POOL}),
        "and none that is far");
    return cli_checked();
}
