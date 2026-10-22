// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// maven_settings.c: the compiler's settings a POM gives, by its properties or its compiler plugin's configuration, the
// plugin's version and its arguments; and Maven's own, from the user's `settings.xml` and the global one in Maven's
// home: the folders, the local repository, whether it works offline, and its mirrors, proxies and active profiles.
// One reading runs at a time; a settings file larger than CLI_MAVEN_SETTINGS_TEXT is read as empty
#include "../text/json_tree.h"
#include "../toolchains/toolchains.h"
#include "maven.h"
#include "maven_internal.h"

// the most bytes of a settings file read
#define CLI_MAVEN_SETTINGS_TEXT (1ull << 20u)

const char *const g_cli_maven_compiler[4][4] = {
    {"release", "Java release", "maven.compiler.release", "release"},
    {"source", "Java source", "maven.compiler.source", "source"},
    {"target", "Java target", "maven.compiler.target", "target"},
    {"encoding", "Source encoding", "project.build.sourceEncoding", "encoding"}};

static CliLock s_settings_lock = CLI_LOCK_INIT;

unsigned int cli_maven_compiler_plugin(const CliXml *xml, unsigned int project)
{
    static const char *const s_build[2] = {"build", "plugins"};
    static const char *const s_managed[3] = {"build", "pluginManagement", "plugins"};
    const unsigned int lists[2] = {cli_xml_at_path(xml, project, s_build, 2u),
                                   cli_xml_at_path(xml, project, s_managed, 3u)};
    static char s_artifact[1u << 10u];
    for (unsigned int list = 0u; list < 2u; list += 1u)
    {
        for (unsigned int one = (lists[list] != CLI_XML_NONE) ? xml->element[lists[list]].first : CLI_XML_NONE;
             one != CLI_XML_NONE; one = xml->element[one].next)
        {
            CliLine artifact = {s_artifact, sizeof(s_artifact), 0ull};
            if (cli_xml_named(xml, one, "plugin") && cli_xml_text_of(xml, one, "artifactId", &artifact) &&
                (artifact.at == 21u) &&
                (memor.cmp(&(MemoriaCfg){.src = s_artifact, .other = "maven-compiler-plugin", .bytes = 21u}) == 0))
            {
                return one;
            }
        }
    }
    return CLI_XML_NONE;
}

int cli_maven_home(char *out)
{
    if (query_environment_read("MAVEN_HOME", out, CLI_PATH_LONGEST) ||
        query_environment_read("M2_HOME", out, CLI_PATH_LONGEST))
    {
        return 1;
    }
    char program[CLI_PATH_LONGEST];
    int found = cli_toolchains_chosen_program("maven", program);
    if (!found)
    {
        static const char *const s_names[1] = {"mvn"};
        found = cli_toolchains_on_path(s_names, 1u, program);
    }
    char bin[CLI_PATH_LONGEST];
    return found && cli_path_parent(program, bin) && cli_path_parent(bin, out);
}

// a setting row written
static void settings_row(CliLine *out, int first, const char *key, const char *group, const char *label,
                         const char *value, unsigned long long value_size, const char *file, int settable)
{
    cli_line_text(out, first ? "{\"key\":" : ",{\"key\":");
    cli_json_quoted(out, key, cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"group\":");
    cli_json_quoted(out, group, cellul.len(&(CatenaFinitaCfg){.src = group, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"label\":");
    cli_json_quoted(out, label, cellul.len(&(CatenaFinitaCfg){.src = label, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"value\":");
    cli_json_quoted_lossy(out, (const unsigned char *)value, value_size);
    cli_line_text(out, ",\"file\":");
    cli_json_quoted_lossy(out, (const unsigned char *)file,
                          cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, settable ? ",\"settable\":true}" : ",\"settable\":false}");
}

// the POM's compiler settings written as rows
static void settings_pom(const char *pom, const char *text, unsigned long long size, const char *shown, CliLine *out)
{
    static CliXml s_xml;
    static CliMavenModel s_model;
    static char s_value[1u << 14u];
    cli_xml_parse(text, size, &s_xml);
    const unsigned int project = cli_xml_root(&s_xml);
    if ((project == CLI_XML_NONE) || !cli_xml_named(&s_xml, project, "project"))
    {
        cli_xml_given(&s_xml);
        return;
    }
    cli_maven_model_of(pom, text, size, &s_model);
    const unsigned int plugin = cli_maven_compiler_plugin(&s_xml, project);
    const unsigned int configuration =
        (plugin != CLI_XML_NONE) ? cli_xml_child(&s_xml, plugin, "configuration") : CLI_XML_NONE;
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        unsigned long long value_size = 0ull;
        const char *value = cli_maven_property(
            &s_model, g_cli_maven_compiler[at][2],
            cellul.len(&(CatenaFinitaCfg){.src = g_cli_maven_compiler[at][2], .cap = CLI_CELLS_POOL}), &value_size);
        CliLine line = {s_value, sizeof(s_value), 0ull};
        if ((value == NULL) && cli_xml_text_of(&s_xml, configuration, g_cli_maven_compiler[at][3], &line))
        {
            value = s_value;
            value_size = line.at;
        }
        settings_row(out, at == 0u, g_cli_maven_compiler[at][0], "pom", g_cli_maven_compiler[at][1],
                     (value != NULL) ? value : "", value_size, shown, 1);
    }
    CliLine version = {s_value, sizeof(s_value), 0ull};
    const int has_version = cli_xml_text_of(&s_xml, plugin, "version", &version);
    settings_row(out, 0, "compiler-plugin", "pom", "Compiler plugin version", s_value, has_version ? version.at : 0u,
                 shown, has_version);
    CliLine arguments = {s_value, sizeof(s_value), 0ull};
    const unsigned int args =
        (configuration != CLI_XML_NONE) ? cli_xml_child(&s_xml, configuration, "compilerArgs") : CLI_XML_NONE;
    int first = 1;
    for (unsigned int one = (args != CLI_XML_NONE) ? s_xml.element[args].first : CLI_XML_NONE; one != CLI_XML_NONE;
         one = s_xml.element[one].next)
    {
        const unsigned long long before = arguments.at;
        cli_line_text(&arguments, first ? "" : " ");
        if (cli_xml_text(&s_xml, one, &arguments))
        {
            first = 0;
        }
        else
        {
            arguments.at = before;
        }
    }
    settings_row(out, 0, "compiler-args", "pom", "Compiler arguments", s_value, arguments.at, shown, 0);
    cli_xml_given(&s_xml);
    cli_maven_model_given(&s_model);
}

// The settings files read: the user's and the global one, each its bytes mapped and their size, its XML, and whether
// it was read
typedef struct
{
    CliXml xml[2];
    const unsigned char *bytes[2];
    unsigned long long size[2];
    int read[2];
} CliMavenSettingsFiles;

static CliMavenSettingsFiles s_files;

// the text of the root's child `name` in the first settings file that holds it into `out`: 1, or 0 where none does
static int settings_value(const char *name, CliLine *out)
{
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        const unsigned int root = s_files.read[at] ? cli_xml_root(&s_files.xml[at]) : CLI_XML_NONE;
        if ((root != CLI_XML_NONE) && cli_xml_text_of(&s_files.xml[at], root, name, out))
        {
            return 1;
        }
    }
    return 0;
}

// each `each` of the root's `list` in both settings files, shown by its children `parts` joined by `between`, all
// joined by `; `, into `out`
static void settings_listed(const char *list, const char *each, const char *const *parts, unsigned int count,
                            const char *const *between, CliLine *out)
{
    int first = 1;
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        const CliXml *const xml = &s_files.xml[at];
        const unsigned int root = s_files.read[at] ? cli_xml_root(xml) : CLI_XML_NONE;
        const unsigned int holder = (root != CLI_XML_NONE) ? cli_xml_child(xml, root, list) : CLI_XML_NONE;
        for (unsigned int one = (holder != CLI_XML_NONE) ? xml->element[holder].first : CLI_XML_NONE;
             one != CLI_XML_NONE; one = xml->element[one].next)
        {
            if (!cli_xml_named(xml, one, each))
            {
                continue;
            }
            cli_line_text(out, first ? "" : "; ");
            first = 0;
            if (count == 0u)
            {
                (void)cli_xml_text(xml, one, out);
            }
            for (unsigned int part = 0u; part < count; part += 1u)
            {
                cli_line_text(out, (part > 0u) ? between[part - 1u] : "");
                (void)cli_xml_text_of(xml, one, parts[part], out);
            }
        }
    }
}

// a settings file mapped and read into its place among those read, one larger than CLI_MAVEN_SETTINGS_TEXT read as
// empty
static void settings_read(unsigned int at, const char *path)
{
    s_files.size[at] = 0ull;
    s_files.bytes[at] = (path[0] != '\0') ? cli_file_held(path, &s_files.size[at]) : NULL;
    s_files.read[at] = (s_files.bytes[at] != NULL) && (s_files.size[at] <= CLI_MAVEN_SETTINGS_TEXT);
    // the file's bytes are read as its text
    cli_xml_parse(s_files.read[at] ? (const char *)s_files.bytes[at] : "", s_files.read[at] ? s_files.size[at] : 0ull,
                  &s_files.xml[at]);
}

// the settings files read let go: each one's XML given back and its bytes unmapped
static void settings_let_go(void)
{
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        cli_xml_given(&s_files.xml[at]);
        if (s_files.bytes[at] != NULL)
        {
            cli_file_let_go(s_files.bytes[at], s_files.size[at]);
        }
        s_files.bytes[at] = NULL;
        s_files.read[at] = 0;
    }
}

void cli_maven_settings(const char *root, const char *pom, const char *text, unsigned long long size, CliLine *out)
{
    static char s_value[1u << 16u];
    cli_lock_take(&s_settings_lock);
    char shown[CLI_PATH_LONGEST];
    cli_build_inside(root, pom, shown);
    cli_line_character(out, '[');
    const unsigned long long start = out->at;
    settings_pom(pom, text, size, shown, out);
    const int first = (out->at == start);
    char home[CLI_PATH_LONGEST] = {0};
    char user[CLI_PATH_LONGEST] = {0};
    char global[CLI_PATH_LONGEST] = {0};
    char repository[CLI_PATH_LONGEST] = {0};
    if (cli_maven_home(home))
    {
        char conf[CLI_PATH_LONGEST];
        cli_full(home, "conf", conf);
        cli_full(conf, "settings.xml", global);
        if (!query_is_file(global))
        {
            global[0] = '\0';
        }
    }
    (void)cli_maven_user_settings(user);
    settings_read(0u, user);
    settings_read(1u, global);
    const char *const user_file = user;
    settings_row(out, first, "maven-home", "settings", "Maven home", home,
                 cellul.len(&(CatenaFinitaCfg){.src = home, .cap = CLI_CELLS_POOL}), "", 0);
    const int user_there = (user[0] != '\0') && query_is_file(user);
    settings_row(out, 0, "user-settings", "settings", "User settings", user_there ? user : "",
                 user_there ? cellul.len(&(CatenaFinitaCfg){.src = user, .cap = CLI_CELLS_POOL}) : 0u, user_file, 0);
    settings_row(out, 0, "global-settings", "settings", "Global settings", global,
                 cellul.len(&(CatenaFinitaCfg){.src = global, .cap = CLI_CELLS_POOL}), global, 0);
    CliLine value = {s_value, sizeof(s_value), 0ull};
    if (!settings_value("localRepository", &value) && cli_maven_local_repository(repository))
    {
        cli_line_text(&value, repository);
    }
    settings_row(out, 0, "local-repository", "settings", "Local repository", s_value, value.at, user_file, 1);
    value.at = 0ull;
    if (!settings_value("offline", &value))
    {
        cli_line_text(&value, "false");
    }
    settings_row(out, 0, "offline", "settings", "Work offline", s_value, value.at, user_file, 1);
    static const char *const s_mirror[3] = {"id", "url", "mirrorOf"};
    static const char *const s_mirror_between[2] = {" ", " for "};
    value.at = 0ull;
    settings_listed("mirrors", "mirror", s_mirror, 3u, s_mirror_between, &value);
    settings_row(out, 0, "mirrors", "settings", "Mirrors", s_value, value.at, user_file, 0);
    static const char *const s_proxy[2] = {"host", "port"};
    static const char *const s_proxy_between[1] = {":"};
    value.at = 0ull;
    settings_listed("proxies", "proxy", s_proxy, 2u, s_proxy_between, &value);
    settings_row(out, 0, "proxies", "settings", "Proxies", s_value, value.at, user_file, 0);
    value.at = 0ull;
    settings_listed("activeProfiles", "activeProfile", NULL, 0u, NULL, &value);
    settings_row(out, 0, "active-profiles", "settings", "Active profiles", s_value, value.at, user_file, 0);
    cli_line_character(out, ']');
    settings_let_go();
    cli_lock_give(&s_settings_lock);
}
