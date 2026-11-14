// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// maven_settings.c: the compiler's settings a POM gives, by its properties or its compiler plugin's configuration, the
// plugin's version and its arguments; and Maven's own, from the user's `settings.xml` and the global one in Maven's
// home: the folders, the local repository, whether it works offline, and its mirrors, proxies and active profiles.
// One reading runs at a time; a settings file larger than CLI_MAVEN_SETTINGS_TEXT is read as empty
#include "../command_line/pool.h"
#include "../text/json_tree.h"
#include "../toolchains/toolchains.h"
#include "maven.h"
#include "maven_internal.h"

// the max bytes of a settings file read
#define CLI_MAVEN_SETTINGS_TEXT (1ULL << 20u)

const char *const g_cli_maven_compiler[4][4] = {
    {"release", "Java release", "maven.compiler.release", "release"},
    {"source", "Java source", "maven.compiler.source", "source"},
    {"target", "Java target", "maven.compiler.target", "target"},
    {"encoding", "Source encoding", "project.build.sourceEncoding", "encoding"}};

unsigned int cli_maven_compiler_plugin(const CliMavenXmlProjectCtx *args)
{
    static const char *const s_build[2] = {"build", "plugins"};
    static const char *const s_managed[3] = {"build", "pluginManagement", "plugins"};
    const unsigned int lists[2] = {EMBED_CALL(cli_maven_xml_internal.xml_at_path, CliMavenXmlCtx, .xml = args->xml,
                                              .of = args->project, .path_pointer = s_build, .count = 2u),
                                   EMBED_CALL(cli_maven_xml_internal.xml_at_path, CliMavenXmlCtx, .xml = args->xml,
                                              .of = args->project, .path_pointer = s_managed, .count = 3u)};
    static char s_artifact[1u << 10u];
    for (unsigned int list = 0u; list < 2u; list += 1u)
    {
        for (unsigned int one = (lists[list] != CLI_XML_NONE) ? args->xml->element[lists[list]].first : CLI_XML_NONE;
             one != CLI_XML_NONE; one = args->xml->element[one].next)
        {
            VerbaLinea artifact = {s_artifact, sizeof(s_artifact), 0ULL};
            if (EMBED_CALL(cli_maven_xml_name_internal.xml_by_name, CliMavenXmlNameCtx, .xml = args->xml,
                           .element = one, .name = "plugin") &&
                EMBED_CALL(cli_maven_xml_out_internal.xml_text_get, CliMavenXmlOutCtx, .xml = args->xml, .of = one,
                           .name = "artifactId", .out = &artifact) &&
                (artifact.at == 21u) &&
                (memor.cmp(&(MemoriaCfg){.src = s_artifact, .other = "maven-compiler-plugin", .bytes = 21u}) == 0))
            {
                return one;
            }
        }
    }
    return CLI_XML_NONE;
}

int cli_maven_home(const CliMavenOutCtx *args)
{
    if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "MAVEN_HOME",
                   .value_out = args->out_capacity, .capacity = CLI_PATH_MAX_LEN) ||
        EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "M2_HOME",
                   .value_out = args->out_capacity, .capacity = CLI_PATH_MAX_LEN))
    {
        return 1;
    }
    char program[CLI_PATH_MAX_LEN];
    int found =
        EMBED_CALL(cli_toolchains_id_path.chosen_program, CliToolchainsIdPathCfg, .id = "maven", .path = program);
    if (!found)
    {
        static const char *const s_names[1] = {"mvn"};
        found = EMBED_CALL(cli_toolchains_names.on_path, CliToolchainsNamesCfg, .names = s_names, .count = 1u,
                           .program = program);
    }
    char bin[CLI_PATH_MAX_LEN];
    return found &&
           EMBED_CALL(cli_path_out_type_internal.path_parent, CliPathOutTypeCtx, .path = program, .out = bin) &&
           EMBED_CALL(cli_path_out_type_internal.path_parent, CliPathOutTypeCtx, .path = bin,
                      .out = args->out_capacity);
}

typedef struct
{
    VerbaLinea *out;
    int first;
    const char *key;
    const char *group;
    const char *label;
    const char *value;
    unsigned long long value_size;
    const char *file;
    int settable;
    const char *pom;
    const char *text;
    unsigned long long size;
    const char *displayed;
    const char *name;
    const char *list;
    const char *each;
    const char *const *parts;
    unsigned int count;
    const char *const *between;
    unsigned int at;
    const char *path;
} MavenSettingsCtx;
// The settings files read: the user's and the global one, each its bytes mapped and their size, its XML, and whether
// it was read
typedef struct
{
    CliXml xml[2];
    const unsigned char *bytes[2];
    unsigned long long size[2];
    int read[2];
} CliMavenSettingsFiles;
