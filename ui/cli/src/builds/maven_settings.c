// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// maven_settings.c: the compiler's settings a POM gives, by its properties or its compiler plugin's configuration, the
// plugin's version and its arguments; and Maven's own, from the user's `settings.xml` and the global one in Maven's
// home: the folders, the local repository, whether it works offline, and its mirrors, proxies and active profiles.
// One reading runs at a time; a settings file larger than CLI_MAVEN_SETTINGS_TEXT is read as empty
#include "../command_line/cells.h"
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
            VerbaLinea artifact = {s_artifact, sizeof(s_artifact), 0ull};
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
                   .value_out = args->out_room, .room = CLI_PATH_MAX_LEN) ||
        EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "M2_HOME",
                   .value_out = args->out_room, .room = CLI_PATH_MAX_LEN))
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
           EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = program, .out = bin) &&
           EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = bin, .out = args->out_room);
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
    const char *shown;
    const char *name;
    const char *list;
    const char *each;
    const char *const *parts;
    unsigned int count;
    const char *const *between;
    unsigned int at;
    const char *path;
} MavenSettingsCtx;
// a setting row written
EMBED_INLINE void settings_row(const MavenSettingsCtx *args)
{
    VerbaLinea *out = args->out;
    mmgr_line_put(out, args->first ? "{\"key\":" : ",{\"key\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = args->key,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"group\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = args->group,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->group, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"label\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = args->label,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->label, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"value\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)args->value, .length = args->value_size);
    mmgr_line_put(out, ",\"file\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)args->file,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->file, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, args->settable ? ",\"settable\":true}" : ",\"settable\":false}");
}

// the POM's compiler settings written as rows
EMBED_INLINE void settings_pom(const MavenSettingsCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    VerbaLinea *out = args->out;
    static CliXml s_xml;
    static CliMavenModel s_model;
    static char s_value[1u << 14u];
    EMBED_CALL(cli_maven_xml_text_internal.xml_parse, CliMavenXmlTextCtx, .text = text, .size = size,
               .xml_out = &s_xml);
    const unsigned int project =
        EMBED_CALL(cli_maven_xml_project_internal.xml_root, CliMavenXmlProjectCtx, .xml = &s_xml);
    if ((project == CLI_XML_NONE) || !EMBED_CALL(cli_maven_xml_name_internal.xml_by_name, CliMavenXmlNameCtx,
                                                 .xml = &s_xml, .element = project, .name = "project"))
    {
        EMBED_CALL(cli_maven_xml_text_internal.xml_free, CliMavenXmlTextCtx, .xml_out = &s_xml);
        return;
    }
    EMBED_CALL(cli_maven_text_size_model_internal.model_get, CliMavenTextSizeModelCtx, .path = args->pom, .text = text,
               .size = size, .model = &s_model);
    const unsigned int plugin = EMBED_CALL(cli_maven_xml_project_internal.compiler_plugin, CliMavenXmlProjectCtx,
                                           .xml = &s_xml, .project = project);
    const unsigned int configuration = (plugin != CLI_XML_NONE)
                                           ? EMBED_CALL(cli_maven_xml_out_internal.xml_child, CliMavenXmlOutCtx,
                                                        .xml = &s_xml, .of = plugin, .name = "configuration")
                                           : CLI_XML_NONE;
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        unsigned long long value_size = 0ull;
        const char *value = EMBED_CALL(
            cli_maven_size_internal.property, CliMavenSizeCtx, .model_read = &s_model,
            .name = g_cli_maven_compiler[at][2],
            .size = cellul.len(&(CatenaFinitaCfg){.src = g_cli_maven_compiler[at][2], .cap = CLI_CELLS_POOL}),
            .value_size = &value_size);
        VerbaLinea line = {s_value, sizeof(s_value), 0ull};
        if ((value == NULL) && EMBED_CALL(cli_maven_xml_out_internal.xml_text_get, CliMavenXmlOutCtx, .xml = &s_xml,
                                          .of = configuration, .name = g_cli_maven_compiler[at][3], .out = &line))
        {
            value = s_value;
            value_size = line.at;
        }
        EMBED_CALL(settings_row, MavenSettingsCtx, .out = out, .first = at == 0u, .key = g_cli_maven_compiler[at][0],
                   .group = "pom", .label = g_cli_maven_compiler[at][1], .value = (value != NULL) ? value : "",
                   .value_size = value_size, .file = args->shown, .settable = 1);
    }
    VerbaLinea version = {s_value, sizeof(s_value), 0ull};
    const int has_version = EMBED_CALL(cli_maven_xml_out_internal.xml_text_get, CliMavenXmlOutCtx, .xml = &s_xml,
                                       .of = plugin, .name = "version", .out = &version);
    EMBED_CALL(settings_row, MavenSettingsCtx, .out = out, .first = 0, .key = "compiler-plugin", .group = "pom",
               .label = "Compiler plugin version", .value = s_value, .value_size = has_version ? version.at : 0u,
               .file = args->shown, .settable = has_version);
    VerbaLinea arguments = {s_value, sizeof(s_value), 0ull};
    const unsigned int args_local = (configuration != CLI_XML_NONE)
                                        ? EMBED_CALL(cli_maven_xml_out_internal.xml_child, CliMavenXmlOutCtx,
                                                     .xml = &s_xml, .of = configuration, .name = "compilerArgs")
                                        : CLI_XML_NONE;
    int first = 1;
    for (unsigned int one = (args_local != CLI_XML_NONE) ? s_xml.element[args_local].first : CLI_XML_NONE;
         one != CLI_XML_NONE; one = s_xml.element[one].next)
    {
        const unsigned long long before = arguments.at;
        mmgr_line_put(&arguments, first ? "" : " ");
        if (EMBED_CALL(cli_maven_xml_out_internal.xml_text, CliMavenXmlOutCtx, .xml = &s_xml, .of = one,
                       .out = &arguments))
        {
            first = 0;
        }
        else
        {
            arguments.at = before;
        }
    }
    EMBED_CALL(settings_row, MavenSettingsCtx, .out = out, .first = 0, .key = "compiler-args", .group = "pom",
               .label = "Compiler arguments", .value = s_value, .value_size = arguments.at, .file = args->shown,
               .settable = 0);
    EMBED_CALL(cli_maven_xml_text_internal.xml_free, CliMavenXmlTextCtx, .xml_out = &s_xml);
    EMBED_CALL(cli_maven_text_size_model_internal.model_free, CliMavenTextSizeModelCtx, .model = &s_model);
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

// the text of the root's child `name` in the first settings file that contains it into `out`: 1, or 0 where none does
EMBED_INLINE int settings_value(const MavenSettingsCtx *args)
{
    const char *name = args->name;
    VerbaLinea *out = args->out;
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        const unsigned int root = s_files.read[at] ? EMBED_CALL(cli_maven_xml_project_internal.xml_root,
                                                                CliMavenXmlProjectCtx, .xml = &s_files.xml[at])
                                                   : CLI_XML_NONE;
        if ((root != CLI_XML_NONE) && EMBED_CALL(cli_maven_xml_out_internal.xml_text_get, CliMavenXmlOutCtx,
                                                 .xml = &s_files.xml[at], .of = root, .name = name, .out = out))
        {
            return 1;
        }
    }
    return 0;
}

// each `each` of the root's `list` in both settings files, shown by its children `parts` joined by `between`, all
// joined by `; `, into `out`
EMBED_INLINE void settings_list(const MavenSettingsCtx *args)
{
    VerbaLinea *out = args->out;
    int first = 1;
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        const CliXml *const xml = &s_files.xml[at];
        const unsigned int root =
            s_files.read[at] ? EMBED_CALL(cli_maven_xml_project_internal.xml_root, CliMavenXmlProjectCtx, .xml = xml)
                             : CLI_XML_NONE;
        const unsigned int holder = (root != CLI_XML_NONE)
                                        ? EMBED_CALL(cli_maven_xml_out_internal.xml_child, CliMavenXmlOutCtx,
                                                     .xml = xml, .of = root, .name = args->list)
                                        : CLI_XML_NONE;
        for (unsigned int one = (holder != CLI_XML_NONE) ? xml->element[holder].first : CLI_XML_NONE;
             one != CLI_XML_NONE; one = xml->element[one].next)
        {
            if (!EMBED_CALL(cli_maven_xml_name_internal.xml_by_name, CliMavenXmlNameCtx, .xml = xml, .element = one,
                            .name = args->each))
            {
                continue;
            }
            mmgr_line_put(out, first ? "" : "; ");
            first = 0;
            if (args->count == 0u)
            {
                (void)EMBED_CALL(cli_maven_xml_out_internal.xml_text, CliMavenXmlOutCtx, .xml = xml, .of = one,
                                 .out = out);
            }
            for (unsigned int part = 0u; part < args->count; part += 1u)
            {
                mmgr_line_put(out, (part > 0u) ? args->between[part - 1u] : "");
                (void)EMBED_CALL(cli_maven_xml_out_internal.xml_text_get, CliMavenXmlOutCtx, .xml = xml, .of = one,
                                 .name = args->parts[part], .out = out);
            }
        }
    }
}

// a settings file mapped and read into its place among those read, one larger than CLI_MAVEN_SETTINGS_TEXT read as
// empty
EMBED_INLINE void settings_read(const MavenSettingsCtx *args)
{
    const char *path = args->path;
    s_files.size[args->at] = 0ull;
    s_files.bytes[args->at] = (path[0] != '\0')
                                  ? EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = path,
                                               .size_pointer = &s_files.size[args->at], .owner = &g_cli_build_cells)
                                  : NULL;
    s_files.read[args->at] = (s_files.bytes[args->at] != NULL) && (s_files.size[args->at] <= CLI_MAVEN_SETTINGS_TEXT);
    // the file's bytes are read as its text
    EMBED_CALL(cli_maven_xml_text_internal.xml_parse, CliMavenXmlTextCtx,
               .text = s_files.read[args->at] ? (const char *)s_files.bytes[args->at] : "",
               .size = s_files.read[args->at] ? s_files.size[args->at] : 0ull, .xml_out = &s_files.xml[args->at]);
}

// the settings files read let go: each one's XML given back and its bytes unmapped
static void settings_let_go(void)
{
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        EMBED_CALL(cli_maven_xml_text_internal.xml_free, CliMavenXmlTextCtx, .xml_out = &s_files.xml[at]);
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = s_files.bytes[at],
                   .owner = &g_cli_build_cells);
        s_files.bytes[at] = NULL;
        s_files.read[at] = 0;
    }
}

void cli_maven_settings(const CliMavenCfg *args)
{
    const char *root = args->root;
    VerbaLinea *out = args->out;
    static char s_value[1u << 16u];
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_settings_lock);
    char shown[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_builds_root.build_inside, CliBuildsRootCfg, .root = root, .folder = args->pom, .out_room = shown);
    mmgr_line_ch(out, '[');
    const unsigned long long start = out->at;
    EMBED_CALL(settings_pom, MavenSettingsCtx, .pom = args->pom, .text = args->text, .size = args->size, .shown = shown,
               .out = out);
    const int first = (out->at == start);
    char home[CLI_PATH_MAX_LEN] = {0};
    char user[CLI_PATH_MAX_LEN] = {0};
    char global[CLI_PATH_MAX_LEN] = {0};
    char repository[CLI_PATH_MAX_LEN] = {0};
    if (EMBED_CALL(cli_maven_out_internal.home, CliMavenOutCtx, .out_room = home))
    {
        char conf[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = home, .relative = "conf", .joined = conf);
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = conf, .relative = "settings.xml", .joined = global);
        if (!EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = global))
        {
            global[0] = '\0';
        }
    }
    (void)EMBED_CALL(cli_maven_out_internal.user_settings, CliMavenOutCtx, .out_room = user);
    EMBED_CALL(settings_read, MavenSettingsCtx, .at = 0u, .path = user);
    EMBED_CALL(settings_read, MavenSettingsCtx, .at = 1u, .path = global);
    const char *const user_file = user;
    EMBED_CALL(settings_row, MavenSettingsCtx, .out = out, .first = first, .key = "maven-home", .group = "settings",
               .label = "Maven home", .value = home,
               .value_size = cellul.len(&(CatenaFinitaCfg){.src = home, .cap = CLI_CELLS_POOL}), .file = "",
               .settable = 0);
    const int user_there =
        (user[0] != '\0') && EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = user);
    EMBED_CALL(settings_row, MavenSettingsCtx, .out = out, .first = 0, .key = "user-settings", .group = "settings",
               .label = "User settings", .value = user_there ? user : "",
               .value_size = user_there ? cellul.len(&(CatenaFinitaCfg){.src = user, .cap = CLI_CELLS_POOL}) : 0u,
               .file = user_file, .settable = 0);
    EMBED_CALL(settings_row, MavenSettingsCtx, .out = out, .first = 0, .key = "global-settings", .group = "settings",
               .label = "Global settings", .value = global,
               .value_size = cellul.len(&(CatenaFinitaCfg){.src = global, .cap = CLI_CELLS_POOL}), .file = global,
               .settable = 0);
    VerbaLinea value = {s_value, sizeof(s_value), 0ull};
    if (!EMBED_CALL(settings_value, MavenSettingsCtx, .name = "localRepository", .out = &value) &&
        EMBED_CALL(cli_maven_out_internal.local_repository, CliMavenOutCtx, .out_room = repository))
    {
        mmgr_line_put(&value, repository);
    }
    EMBED_CALL(settings_row, MavenSettingsCtx, .out = out, .first = 0, .key = "local-repository", .group = "settings",
               .label = "Local repository", .value = s_value, .value_size = value.at, .file = user_file, .settable = 1);
    value.at = 0ull;
    if (!EMBED_CALL(settings_value, MavenSettingsCtx, .name = "offline", .out = &value))
    {
        mmgr_line_put(&value, "false");
    }
    EMBED_CALL(settings_row, MavenSettingsCtx, .out = out, .first = 0, .key = "offline", .group = "settings",
               .label = "Work offline", .value = s_value, .value_size = value.at, .file = user_file, .settable = 1);
    static const char *const s_mirror[3] = {"id", "url", "mirrorOf"};
    static const char *const s_mirror_between[2] = {" ", " for "};
    value.at = 0ull;
    EMBED_CALL(settings_list, MavenSettingsCtx, .list = "mirrors", .each = "mirror", .parts = s_mirror, .count = 3u,
               .between = s_mirror_between, .out = &value);
    EMBED_CALL(settings_row, MavenSettingsCtx, .out = out, .first = 0, .key = "mirrors", .group = "settings",
               .label = "Mirrors", .value = s_value, .value_size = value.at, .file = user_file, .settable = 0);
    static const char *const s_proxy[2] = {"host", "port"};
    static const char *const s_proxy_between[1] = {":"};
    value.at = 0ull;
    EMBED_CALL(settings_list, MavenSettingsCtx, .list = "proxies", .each = "proxy", .parts = s_proxy, .count = 2u,
               .between = s_proxy_between, .out = &value);
    EMBED_CALL(settings_row, MavenSettingsCtx, .out = out, .first = 0, .key = "proxies", .group = "settings",
               .label = "Proxies", .value = s_value, .value_size = value.at, .file = user_file, .settable = 0);
    value.at = 0ull;
    EMBED_CALL(settings_list, MavenSettingsCtx, .list = "activeProfiles", .each = "activeProfile", .parts = NULL,
               .count = 0u, .between = NULL, .out = &value);
    EMBED_CALL(settings_row, MavenSettingsCtx, .out = out, .first = 0, .key = "active-profiles", .group = "settings",
               .label = "Active profiles", .value = s_value, .value_size = value.at, .file = user_file, .settable = 0);
    mmgr_line_ch(out, ']');
    settings_let_go();
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_settings_lock);
}
