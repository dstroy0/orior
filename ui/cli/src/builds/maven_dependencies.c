// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// maven_dependencies.c: the dependencies of a `<dependencies>` checked: each names its group and its artifact, is
// listed once by its coordinates, type and classifier, takes a scope Maven knows, and has a version or a management
// that gives it one, where the POM and its parents were all read and none of their managed dependencies imports a
// POM; a version that is a snapshot is noted with the fix that fetches its newest build. Each value has the properties
// it names given. More dependencies than CLI_MAVEN_LISTED in one list are not compared for being listed twice
#include "maven_internal.h"

// the most dependencies of one list compared for being listed twice, and the bytes they take
#define CLI_MAVEN_LISTED 4096u
#define CLI_MAVEN_LISTED_TEXT (1u << 20u)

static unsigned int s_seen_at[CLI_MAVEN_LISTED];
static unsigned int s_seen_size[CLI_MAVEN_LISTED];
static unsigned int s_seen;
static char s_seen_text[CLI_MAVEN_LISTED_TEXT];
static unsigned int s_seen_used;

// the child `name` of `of`, with the properties it names given, into `out`: 1, or 0 where it has none
static int dependencies_get(const CliMavenCheck *check, unsigned int of, const char *name, CliLine *out)
{
    static char s_raw[1u << 14u];
    CliLine raw = {s_raw, sizeof(s_raw), 0ull};
    if (!cli_xml_text_of(check->xml, of, name, &raw) || !cli_line_fits(&raw))
    {
        return 0;
    }
    cli_maven_resolve(s_raw, raw.at, check->model, out);
    return 1;
}

// whether `size` bytes of `whole` were listed above in this list, and kept where they were not
static int dependencies_seen(const char *whole, unsigned long long size)
{
    for (unsigned int at = 0u; at < s_seen; at += 1u)
    {
        if ((s_seen_size[at] == size) &&
            (memor.cmp(&(MemoriaCfg){.src = s_seen_text + s_seen_at[at], .other = whole, .bytes = (size_t)size}) == 0))
        {
            return 1;
        }
    }
    if ((s_seen < CLI_MAVEN_LISTED) && (size <= CLI_MAVEN_LISTED_TEXT - s_seen_used))
    {
        memor.cpy(&(MemoriaCfg){.dst = s_seen_text + s_seen_used, .src = whole, .bytes = (size_t)size});
        s_seen_at[s_seen] = s_seen_used;
        s_seen_size[s_seen] = (unsigned int)size;
        s_seen += 1u;
        s_seen_used += (unsigned int)size;
    }
    return 0;
}

// the scope of a dependency checked against those Maven knows
static void dependencies_scope(const CliMavenCheck *check, unsigned int dependency)
{
    const CliXml *const xml = check->xml;
    const unsigned int scope = cli_xml_child(xml, dependency, "scope");
    static char s_value[1u << 12u];
    CliLine value = {s_value, sizeof(s_value), 0ull};
    unsigned long long found = 0ull;
    if ((scope == CLI_XML_NONE) || !cli_xml_text(xml, scope, &value) || !cli_line_fits(&value) ||
        cli_text_find(s_value, value.at, "${", 2u, &found))
    {
        return;
    }
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = g_cli_maven_scopes[at], .cap = CLI_CELLS_POOL}) == value.at) &&
            (memor.cmp(&(MemoriaCfg){.src = g_cli_maven_scopes[at], .other = s_value, .bytes = value.at}) == 0))
        {
            return;
        }
    }
    const CliXmlElement *const element = &xml->element[scope];
    CliLine said = cli_build_said(check->found);
    cli_line_character(&said, '`');
    cli_line_counted(&said, s_value, value.at);
    cli_line_text(&said, "` is no scope: a scope is ");
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        cli_line_text(&said, (at > 0u) ? ", `" : "`");
        cli_line_text(&said, g_cli_maven_scopes[at]);
        cli_line_character(&said, '`');
    }
    const unsigned int finding = cli_build_finding(check->found, &said, check->lines, element->inner_from,
                                                   element->inner_to, CLI_BUILD_ERROR, "pom-scope");
    CliBuildNear near;
    cli_build_near_begun(&near, s_value, value.at);
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        cli_build_near_offered(&near, g_cli_maven_scopes[at],
                               cellul.len(&(CatenaFinitaCfg){.src = g_cli_maven_scopes[at], .cap = CLI_CELLS_POOL}));
    }
    for (unsigned int at = 0u; at < near.count; at += 1u)
    {
        CliLine title = cli_build_said(check->found);
        cli_line_text(&title, "Change to ");
        cli_line_counted(&title, near.near[at], near.near_size[at]);
        cli_build_edit_fix(check->found, finding, &title, check->lines, element->inner_from, element->inner_to,
                           near.near[at], near.near_size[at]);
    }
}

// whether the model's management gives `size` bytes of `key` a version
static int dependencies_managed(const CliMavenModel *model, const char *key, unsigned long long size)
{
    for (unsigned int at = 0u; at < model->managed; at += 1u)
    {
        if ((model->managed_size[at] == size) &&
            (memor.cmp(
                 &(MemoriaCfg){.src = model->text + model->managed_at[at], .other = key, .bytes = (size_t)size}) == 0))
        {
            return 1;
        }
    }
    return 0;
}

// one dependency of the list checked
static void dependencies_one(const CliMavenCheck *check, unsigned int dependency, int managing)
{
    const CliXml *const xml = check->xml;
    const CliXmlElement *const element = &xml->element[dependency];
    static char s_key[1u << 14u];
    static char s_whole[1u << 14u];
    static char s_version[1u << 14u];
    CliLine key = {s_key, sizeof(s_key), 0ull};
    if (!dependencies_get(check, dependency, "groupId", &key))
    {
        CliLine said = cli_build_said(check->found);
        cli_line_text(&said, "A dependency names its `<groupId>` and its `<artifactId>`");
        (void)cli_build_finding(check->found, &said, check->lines, element->from, element->to, CLI_BUILD_ERROR,
                                "pom-dependency");
        return;
    }
    cli_line_character(&key, ':');
    if (!dependencies_get(check, dependency, "artifactId", &key))
    {
        CliLine said = cli_build_said(check->found);
        cli_line_text(&said, "A dependency names its `<groupId>` and its `<artifactId>`");
        (void)cli_build_finding(check->found, &said, check->lines, element->from, element->to, CLI_BUILD_ERROR,
                                "pom-dependency");
        return;
    }
    CliLine whole = {s_whole, sizeof(s_whole), 0ull};
    cli_line_counted(&whole, s_key, key.at);
    cli_line_character(&whole, ':');
    if (!dependencies_get(check, dependency, "type", &whole))
    {
        cli_line_text(&whole, "jar");
    }
    cli_line_character(&whole, ':');
    (void)dependencies_get(check, dependency, "classifier", &whole);
    if (dependencies_seen(s_whole, whole.at))
    {
        CliLine said = cli_build_said(check->found);
        cli_line_character(&said, '`');
        cli_line_counted(&said, s_key, key.at);
        cli_line_text(&said, "` is listed above it in these dependencies");
        (void)cli_build_finding(check->found, &said, check->lines, element->from, element->to, CLI_BUILD_WARNING,
                                "pom-duplicate");
    }
    dependencies_scope(check, dependency);
    CliLine version = {s_version, sizeof(s_version), 0ull};
    if (!dependencies_get(check, dependency, "version", &version))
    {
        const CliMavenModel *const model = check->model;
        if (!managing && model->whole && !model->imports && !dependencies_managed(model, s_key, key.at))
        {
            CliLine said = cli_build_said(check->found);
            cli_line_character(&said, '`');
            cli_line_counted(&said, s_key, key.at);
            cli_line_text(&said, "` has no version, and no `<dependencyManagement>` gives it one");
            (void)cli_build_finding(check->found, &said, check->lines, element->from, element->to, CLI_BUILD_WARNING,
                                    "pom-version");
        }
        return;
    }
    if (!cli_build_snapshot(s_version, version.at))
    {
        return;
    }
    const unsigned int child = cli_xml_child(xml, dependency, "version");
    const CliXmlElement *const at = &xml->element[(child != CLI_XML_NONE) ? child : dependency];
    CliLine coordinates = {s_whole, sizeof(s_whole), 0ull};
    cli_line_counted(&coordinates, s_key, key.at);
    cli_line_character(&coordinates, ':');
    cli_line_counted(&coordinates, s_version, version.at);
    CliLine said = cli_build_said(check->found);
    cli_line_character(&said, '`');
    cli_line_counted(&said, s_whole, coordinates.at);
    cli_line_text(&said, "` is a snapshot, whose newest build Maven fetches with `-U`");
    const unsigned int finding =
        cli_build_finding(check->found, &said, check->lines, at->inner_from, at->inner_to, CLI_BUILD_NOTE, "snapshot");
    cli_maven_update(check, finding, s_whole, coordinates.at);
}

void cli_maven_dependencies(const CliMavenCheck *check, unsigned int list)
{
    const CliXml *const xml = check->xml;
    const unsigned int parent = xml->element[list].parent;
    const int managing = (parent != CLI_XML_NONE) && cli_xml_named(xml, parent, "dependencyManagement");
    s_seen = 0u;
    s_seen_used = 0u;
    for (unsigned int one = xml->element[list].first; one != CLI_XML_NONE; one = xml->element[one].next)
    {
        if (cli_xml_named(xml, one, "dependency"))
        {
            dependencies_one(check, one, managing);
        }
    }
}
