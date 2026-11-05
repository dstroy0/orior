// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// maven_model.c: what a POM gives its children: its parents read first, by their relative path, `../pom.xml` where it
// gives none, and in the local repository where the path contains no POM of it, eight above it at most; then its own
// coordinates, its properties and its profiles', and the dependencies their management gives a version. The local
// repository the user's settings name, and the program that runs a build. One model is read at a time, under its lock;
// a POM larger than CLI_MAVEN_POM_TEXT is not read as a parent, and properties past what the model contains are left
// out
#include "../command_line/cells.h"
#include "maven_internal.h"

#include "../text/text.h"

// A POM read as a parent or as the user's settings: its bytes, mapped, and their size; the bytes NULL where it is not
// there or is larger than CLI_MAVEN_POM_TEXT
typedef struct
{
    const unsigned char *bytes;
    unsigned long long size;
} CliMavenMapped;
// the most parents above a POM read, and the most bytes of each
#define CLI_MAVEN_DEPTH 8u
#define CLI_MAVEN_POM_TEXT (1ULL << 20u)

typedef struct
{
    const char *path;
    CliMavenMapped mapped;
    CliMavenModel *model;
    const char *name;
    unsigned long long name_size;
    const char *value;
    unsigned long long value_size;
    int replace;
    char *out;
    const CliXml *xml;
    unsigned int of;
    char *capacity;
    unsigned long long size;
    unsigned long long *length;
    unsigned int parent;
    char *place;
    CliMavenMapped *mapped_pointer;
    unsigned int holder;
    const char *text;
    unsigned int depth;
} MavenModelCtx;
// the file at `path` mapped, where it is there and no larger than CLI_MAVEN_POM_TEXT
EMBED_INLINE CliMavenMapped model_map(const MavenModelCtx *args)
{
    const char *path = args->path;
    CliMavenMapped mapped = {NULL, 0ULL};
    mapped.bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path, .size_pointer = &mapped.size,
                              .owner = &g_cli_maven_model_cells);
    if ((mapped.bytes != NULL) && (mapped.size > CLI_MAVEN_POM_TEXT))
    {
        g_cli_cells->persistent_buf_release((void *)(uintptr_t)mapped.bytes, (&g_cli_maven_model_cells));
        mapped.bytes = NULL;
    }
    return mapped;
}

EMBED_INLINE void model_unmapped(const MavenModelCtx *args)
{
    g_cli_cells->persistent_buf_release((void *)(uintptr_t)args->mapped.bytes, (&g_cli_maven_model_cells));
}

const char *cli_maven_property(const CliMavenSizeCtx *args)
{
    unsigned long long *value_size = args->value_size;
    for (unsigned int at = 0u; at < args->model_read->properties; at += 1u)
    {
        if ((args->model_read->name_size[at] == args->size) &&
            (memor.cmp(&(MemoriaCfg){.src = args->model_read->text + args->model_read->name_at[at],
                                     .other = args->name,
                                     .bytes = (size_t)args->size}) == 0))
        {
            *value_size = args->model_read->value_size[at];
            return args->model_read->text + args->model_read->value_at[at];
        }
    }
    *value_size = 0ULL;
    return NULL;
}

// a property set, in place of what it contained where `replace`, or only where it contains none
EMBED_INLINE void model_set(const MavenModelCtx *args)
{
    CliMavenModel *model = args->model;
    unsigned int at = 0u;
    while ((at < model->properties) && !((model->name_size[at] == args->name_size) &&
                                         (memor.cmp(&(MemoriaCfg){.src = model->text + model->name_at[at],
                                                                  .other = args->name,
                                                                  .bytes = (size_t)args->name_size}) == 0)))
    {
        at += 1u;
    }
    if ((at < model->properties) && !args->replace)
    {
        return;
    }
    if ((at == model->properties) && (at >= model->property_capacity))
    {
        return;
    }
    const unsigned int value_at = EMBED_CALL(cli_maven_size_model_internal.model_text_store, CliMavenSizeModelCtx,
                                             .model = model, .bytes = args->value, .size = args->value_size);
    if (value_at == CLI_MAVEN_MODEL_TEXT)
    {
        return;
    }
    if (at == model->properties)
    {
        const unsigned int name_at = EMBED_CALL(cli_maven_size_model_internal.model_text_store, CliMavenSizeModelCtx,
                                                .model = model, .bytes = args->name, .size = args->name_size);
        if (name_at == CLI_MAVEN_MODEL_TEXT)
        {
            return;
        }
        model->name_at[at] = name_at;
        model->name_size[at] = (unsigned int)args->name_size;
        model->properties += 1u;
    }
    model->value_at[at] = value_at;
    model->value_size[at] = (unsigned int)args->value_size;
}

void cli_maven_resolve(const CliMavenSizeOutCtx *args)
{
    static char s_turn[2][1u << 16u];
    VerbaLinea line = {s_turn[0], sizeof(s_turn[0]), 0ULL};
    mmgr_line_put_n(&line, args->value, args->size);
    unsigned int turn_at = 0u;
    unsigned int round = 0u;
    int going = 1;
    while (going && ((round < 8u) && mmgr_line_ok(&line)))
    {
        unsigned long long start = 0ULL;
        unsigned long long length = 0ULL;
        if (!EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = s_turn[turn_at], .length_value = line.at,
                        .required = "${", .size = 2u, .at_pointer = &start) ||
            !EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = s_turn[turn_at] + start,
                        .length_value = line.at - start, .required = "}", .size = 1u, .at_pointer = &length))
        {
            going = 0;
            continue;
        }
        unsigned long long input_size = 0ULL;
        const char *const supplied =
            EMBED_CALL(cli_maven_size_internal.property, CliMavenSizeCtx, .model_read = args->model_read,
                       .name = s_turn[turn_at] + start + 2u, .size = length - 2u, .value_size = &input_size);
        if (supplied == NULL)
        {
            going = 0;
            continue;
        }
        const unsigned int next = 1u - turn_at;
        VerbaLinea turned = {s_turn[next], sizeof(s_turn[next]), 0ULL};
        mmgr_line_put_n(&turned, s_turn[turn_at], start);
        mmgr_line_put_n(&turned, supplied, input_size);
        mmgr_line_put_n(&turned, s_turn[turn_at] + start + length + 1u, line.at - start - length - 1u);
        line = turned;
        turn_at = next;
        round += 1u;
    }
    mmgr_line_put_n(args->out, s_turn[turn_at], line.at);
}

int cli_maven_free(const CliMavenSizeNameCtx *args)
{
    const char *const dot = memor.chr(&(MemoriaCfg){.src = args->name, .val = '.', .bytes = (size_t)args->size});
    const unsigned long long first = (dot != NULL) ? (unsigned long long)(dot - args->name) : args->size;
    for (unsigned int at = 0u; at < 16u; at += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = g_cli_maven_input_heads[at], .cap = CLI_CELLS_POOL}) == first) &&
            (memor.cmp(
                 &(MemoriaCfg){.src = g_cli_maven_input_heads[at], .other = args->name, .bytes = (size_t)first}) == 0))
        {
            return 1;
        }
    }
    return (args->size == 4u) && (memor.cmp(&(MemoriaCfg){.src = args->name, .other = "sha1", .bytes = 4u}) == 0);
}

// the reader's home folder into `out`
EMBED_INLINE int model_home(const MavenModelCtx *args)
{
    return EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "USERPROFILE",
                      .value_out = args->out, .capacity = CLI_PATH_MAX_LEN) ||
           EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "HOME",
                      .value_out = args->out, .capacity = CLI_PATH_MAX_LEN);
}

int cli_maven_user_settings(const CliMavenOutCtx *args)
{
    char home[CLI_PATH_MAX_LEN];
    char m2[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(model_home, MavenModelCtx, .out = home))
    {
        return 0;
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = home, .relative = ".m2", .joined = m2);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = m2, .relative = "settings.xml",
               .joined = args->out_capacity);
    return 1;
}

int cli_maven_local_repository(const CliMavenOutCtx *args)
{
    // the settings mapped and read into a tree; settings larger than a POM is read as are read as settings that are not
    // there
    char path[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_maven_out_internal.user_settings, CliMavenOutCtx, .out_capacity = path))
    {
        const CliMavenMapped mapped = EMBED_CALL(model_map, MavenModelCtx, .path = path);
        int named_one = 0;
        if (mapped.bytes != NULL)
        {
            CliXml settings;
            // the file's bytes are read as its text
            EMBED_CALL(cli_maven_xml_text_internal.xml_parse, CliMavenXmlTextCtx, .text = (const char *)mapped.bytes,
                       .size = mapped.size, .xml_out = &settings);
            VerbaLinea named = {args->out_capacity, CLI_PATH_MAX_LEN, 0ULL};
            const unsigned int root =
                EMBED_CALL(cli_maven_xml_project_internal.xml_root, CliMavenXmlProjectCtx, .xml = &settings);
            named_one = (root != CLI_XML_NONE) &&
                        EMBED_CALL(cli_maven_xml_out_internal.xml_text_get, CliMavenXmlOutCtx, .xml = &settings,
                                   .of = root, .name = "localRepository", .out = &named) &&
                        (named.at > 0u) && (mmgr_line_finish(&named) > 0u);
            g_cli_cells->persistent_buf_release(settings.element, &g_cli_xml_cells);
            *(&settings) = (CliXml){settings.text, 0ULL, NULL, 0u, 0u, NULL, NULL, 0u, 0u, NULL, 0u, 0u, NULL, 0u};
        }
        EMBED_CALL(model_unmapped, MavenModelCtx, .mapped = mapped);
        if (named_one)
        {
            return 1;
        }
    }
    char home[CLI_PATH_MAX_LEN];
    char m2[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(model_home, MavenModelCtx, .out = home))
    {
        return 0;
    }
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = home, .relative = ".m2", .joined = m2);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = m2, .relative = "repository", .joined = args->out_capacity);
    return 1;
}

// a child's text of `of` in the xml at `depth` into a line over `capacity`: its size, or 0 where it has none
EMBED_INLINE int model_text_get(const MavenModelCtx *args)
{
    const CliXml *xml = args->xml;
    unsigned int of = args->of;
    const char *name = args->name;
    unsigned long long *length = args->length;
    VerbaLinea line = {args->capacity, args->size, 0ULL};
    const int found = EMBED_CALL(cli_maven_xml_out_internal.xml_text_get, CliMavenXmlOutCtx, .xml = xml, .of = of,
                                 .name = name, .out = &line) &&
                      mmgr_line_ok(&line);
    *length = line.at;
    return found;
}

// the POM the `<parent>` at `parent` of the POM at `path` names, mapped into `mapped`: 1 with its path, or 0 where none
// is found
EMBED_INLINE int model_parent_pom(const MavenModelCtx *args)
{
    const char *path = args->path;
    const CliXml *xml = args->xml;
    CliMavenMapped *mapped = args->mapped_pointer;
    char folder[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = path, .out = folder))
    {
        return 0;
    }
    char relative[CLI_PATH_MAX_LEN];
    unsigned long long relative_size = 0ULL;
    if (EMBED_CALL(cli_maven_xml_out_internal.xml_child, CliMavenXmlOutCtx, .xml = xml, .of = args->parent,
                   .name = "relativePath") == CLI_XML_NONE)
    {
        relative_size = 10u;
        memor.cpy(&(MemoriaCfg){.dst = relative, .src = "../pom.xml", .bytes = 11u});
    }
    else if (!EMBED_CALL(model_text_get, MavenModelCtx, .xml = xml, .of = args->parent, .name = "relativePath",
                         .capacity = relative, .size = sizeof(relative) - 1u, .length = &relative_size))
    {
        relative_size = 0u;
    }
    relative[relative_size] = '\0';
    if (relative_size > 0u)
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = relative, .joined = args->place);
        if (EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = args->place))
        {
            char folder_position[CLI_PATH_MAX_LEN];
            (void)cellul.copy(&(CatenaFinitaCfg){.dst = folder_position, .src = args->place, .cap = CLI_PATH_MAX_LEN});
            EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder_position, .relative = "pom.xml",
                       .joined = args->place);
        }
        *mapped = EMBED_CALL(model_map, MavenModelCtx, .path = args->place);
        if (mapped->bytes != NULL)
        {
            return 1;
        }
    }
    char group[1024];
    char artifact[1024];
    char version[1024];
    unsigned long long group_size = 0ULL;
    unsigned long long artifact_size = 0ULL;
    unsigned long long version_size = 0ULL;
    char repository[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(model_text_get, MavenModelCtx, .xml = xml, .of = args->parent, .name = "groupId", .capacity = group,
                    .size = sizeof(group), .length = &group_size) ||
        !EMBED_CALL(model_text_get, MavenModelCtx, .xml = xml, .of = args->parent, .name = "artifactId",
                    .capacity = artifact, .size = sizeof(artifact), .length = &artifact_size) ||
        !EMBED_CALL(model_text_get, MavenModelCtx, .xml = xml, .of = args->parent, .name = "version",
                    .capacity = version, .size = sizeof(version), .length = &version_size) ||
        !EMBED_CALL(cli_maven_out_internal.local_repository, CliMavenOutCtx, .out_capacity = repository))
    {
        return 0;
    }
    VerbaLinea line = {args->place, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, repository);
    mmgr_line_ch(&line, '/');
    for (unsigned long long at = 0ULL; at < group_size; at += 1u)
    {
        mmgr_line_ch(&line, (group[at] == '.') ? '/' : group[at]);
    }
    mmgr_line_ch(&line, '/');
    mmgr_line_put_n(&line, artifact, artifact_size);
    mmgr_line_ch(&line, '/');
    mmgr_line_put_n(&line, version, version_size);
    mmgr_line_ch(&line, '/');
    mmgr_line_put_n(&line, artifact, artifact_size);
    mmgr_line_ch(&line, '-');
    mmgr_line_put_n(&line, version, version_size);
    mmgr_line_put(&line, ".pom");
    if (mmgr_line_finish(&line) == 0u)
    {
        return 0;
    }
    *mapped = EMBED_CALL(model_map, MavenModelCtx, .path = args->place);
    return mapped->bytes != NULL;
}

// the properties and the managed dependencies of the holder at `holder` put in the model
static void model_holder(const MavenModelCtx *args)
{
    const CliXml *xml = args->xml;
    CliMavenModel *model = args->model;
    static char s_value[1u << 16u];
    static char s_group[1u << 12u];
    static char s_artifact[1u << 12u];
    const unsigned int properties = EMBED_CALL(cli_maven_xml_out_internal.xml_child, CliMavenXmlOutCtx, .xml = xml,
                                               .of = args->holder, .name = "properties");
    for (unsigned int one = (properties != CLI_XML_NONE) ? xml->element[properties].first : CLI_XML_NONE;
         one != CLI_XML_NONE; one = xml->element[one].next)
    {
        VerbaLinea value = {s_value, sizeof(s_value), 0ULL};
        if (!EMBED_CALL(cli_maven_xml_out_internal.xml_text, CliMavenXmlOutCtx, .xml = xml, .of = one, .out = &value) ||
            !mmgr_line_ok(&value))
        {
            value.at = 0ULL;
        }
        const CliXmlElement *const element = &xml->element[one];
        EMBED_CALL(model_set, MavenModelCtx, .model = model, .name = xml->text + element->from,
                   .name_size = element->to - element->from, .value = s_value, .value_size = value.at, .replace = 1);
    }
    static const char *const s_path[2] = {"dependencyManagement", "dependencies"};
    const unsigned int dependencies = EMBED_CALL(cli_maven_xml_internal.xml_at_path, CliMavenXmlCtx, .xml = xml,
                                                 .of = args->holder, .path_pointer = s_path, .count = 2u);
    for (unsigned int one = (dependencies != CLI_XML_NONE) ? xml->element[dependencies].first : CLI_XML_NONE;
         one != CLI_XML_NONE; one = xml->element[one].next)
    {
        if (!EMBED_CALL(cli_maven_xml_name_internal.xml_by_name, CliMavenXmlNameCtx, .xml = xml, .element = one,
                        .name = "dependency"))
        {
            continue;
        }
        unsigned long long group_size = 0ULL;
        unsigned long long artifact_size = 0ULL;
        if (EMBED_CALL(model_text_get, MavenModelCtx, .xml = xml, .of = one, .name = "groupId", .capacity = s_group,
                       .size = sizeof(s_group), .length = &group_size) &&
            EMBED_CALL(model_text_get, MavenModelCtx, .xml = xml, .of = one, .name = "artifactId",
                       .capacity = s_artifact, .size = sizeof(s_artifact), .length = &artifact_size) &&
            (model->managed < model->managed_capacity))
        {
            VerbaLinea key = {s_value, sizeof(s_value), 0ULL};
            EMBED_CALL(cli_maven_size_out_internal.resolve, CliMavenSizeOutCtx, .value = s_group, .size = group_size,
                       .model_read = model, .out = &key);
            mmgr_line_ch(&key, ':');
            EMBED_CALL(cli_maven_size_out_internal.resolve, CliMavenSizeOutCtx, .value = s_artifact,
                       .size = artifact_size, .model_read = model, .out = &key);
            const unsigned int at =
                mmgr_line_ok(&key) ? EMBED_CALL(cli_maven_size_model_internal.model_text_store, CliMavenSizeModelCtx,
                                                .model = model, .bytes = s_value, .size = key.at)
                                   : CLI_MAVEN_MODEL_TEXT;
            if (at != CLI_MAVEN_MODEL_TEXT)
            {
                model->managed_at[model->managed] = at;
                model->managed_size[model->managed] = (unsigned int)key.at;
                model->managed += 1u;
            }
        }
        VerbaLinea scope = {s_group, sizeof(s_group), 0ULL};
        model->imports |= EMBED_CALL(cli_maven_xml_out_internal.xml_text_get, CliMavenXmlOutCtx, .xml = xml, .of = one,
                                     .name = "scope", .out = &scope) &&
                          (scope.at == 6u) &&
                          (memor.cmp(&(MemoriaCfg){.src = s_group, .other = "import", .bytes = 6u}) == 0);
    }
}

// what the POM at `path` gives, its parents read up to `depth` above it, put in the model
static void model_read(const MavenModelCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    CliXml read;
    CliXml *const xml = &read;
    EMBED_CALL(cli_maven_xml_text_internal.xml_parse, CliMavenXmlTextCtx, .text = text, .size = size, .xml_out = xml);
    // a POM with no capacity left to read it in leaves the model not whole
    args->model->whole = args->model->whole && ((xml->element != NULL) || (size == 0ULL));
    const unsigned int root = EMBED_CALL(cli_maven_xml_project_internal.xml_root, CliMavenXmlProjectCtx, .xml = xml);
    if (root == CLI_XML_NONE)
    {
        g_cli_cells->persistent_buf_release(xml->element, &g_cli_xml_cells);
        *xml = (CliXml){xml->text, 0ULL, NULL, 0u, 0u, NULL, NULL, 0u, 0u, NULL, 0u, 0u, NULL, 0u};
        return;
    }
    const unsigned int parent =
        EMBED_CALL(cli_maven_xml_out_internal.xml_child, CliMavenXmlOutCtx, .xml = xml, .of = root, .name = "parent");
    static char s_value[1u << 16u];
    if (parent != CLI_XML_NONE)
    {
        char place[CLI_PATH_MAX_LEN];
        CliMavenMapped mapped = {NULL, 0ULL};
        if ((args->depth < CLI_MAVEN_DEPTH) &&
            EMBED_CALL(model_parent_pom, MavenModelCtx, .path = args->path, .xml = xml, .parent = parent,
                       .place = place, .mapped_pointer = &mapped))
        {
            // the parent's bytes are read as its text; what the model retains of it is copied into the model's text
            EMBED_CALL(model_read, MavenModelCtx, .path = place, .text = (const char *)mapped.bytes,
                       .size = mapped.size, .depth = args->depth + 1u, .model = args->model);
            EMBED_CALL(model_unmapped, MavenModelCtx, .mapped = mapped);
        }
        else
        {
            args->model->whole = 0;
        }
        unsigned long long version = 0ULL;
        if (EMBED_CALL(model_text_get, MavenModelCtx, .xml = xml, .of = parent, .name = "version", .capacity = s_value,
                       .size = sizeof(s_value), .length = &version))
        {
            EMBED_CALL(model_set, MavenModelCtx, .model = args->model, .name = "project.parent.version",
                       .name_size = 22u, .value = s_value, .value_size = version, .replace = 1);
            EMBED_CALL(model_set, MavenModelCtx, .model = args->model, .name = "project.version", .name_size = 15u,
                       .value = s_value, .value_size = version, .replace = 0);
        }
    }
    static const char *const s_names[3][2] = {
        {"project.groupId", "groupId"}, {"project.artifactId", "artifactId"}, {"project.version", "version"}};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        unsigned long long found = 0ULL;
        if (EMBED_CALL(model_text_get, MavenModelCtx, .xml = xml, .of = root, .name = s_names[at][1],
                       .capacity = s_value, .size = sizeof(s_value), .length = &found))
        {
            EMBED_CALL(model_set, MavenModelCtx, .model = args->model, .name = s_names[at][0],
                       .name_size = cellul.len(&(CatenaFinitaCfg){.src = s_names[at][0], .cap = CLI_CELLS_POOL}),
                       .value = s_value, .value_size = found, .replace = 1);
        }
    }
    EMBED_CALL(model_holder, MavenModelCtx, .xml = xml, .holder = root, .model = args->model);
    const unsigned int profiles =
        EMBED_CALL(cli_maven_xml_out_internal.xml_child, CliMavenXmlOutCtx, .xml = xml, .of = root, .name = "profiles");
    for (unsigned int one = (profiles != CLI_XML_NONE) ? xml->element[profiles].first : CLI_XML_NONE;
         one != CLI_XML_NONE; one = xml->element[one].next)
    {
        if (EMBED_CALL(cli_maven_xml_name_internal.xml_by_name, CliMavenXmlNameCtx, .xml = xml, .element = one,
                       .name = "profile"))
        {
            EMBED_CALL(model_holder, MavenModelCtx, .xml = xml, .holder = one, .model = args->model);
        }
    }
    g_cli_cells->persistent_buf_release(xml->element, &g_cli_xml_cells);
    *xml = (CliXml){xml->text, 0ULL, NULL, 0u, 0u, NULL, NULL, 0u, 0u, NULL, 0u, 0u, NULL, 0u};
}

void cli_maven_model_get(const CliMavenTextSizeModelCtx *args)
{
    g_cli_cells->persistent_buf_release(args->model->name_at, &g_cli_maven_model_cells);
    *args->model = (CliMavenModel){NULL, NULL, NULL, NULL, 0u, 0u, NULL, NULL, 0u, 0u, 0, 0, NULL, 0u, 0u};
    cli_maven_text_size_model_internal.model_alloc(args);
    args->model->whole = args->model->text != NULL;
    EMBED_CALL(model_read, MavenModelCtx, .path = args->path, .text = args->text, .size = args->size, .depth = 0u,
               .model = args->model);
}

void cli_maven_program(const CliMavenOutCtx *args)
{
    VerbaLinea line = {args->out_capacity, CLI_PATH_MAX_LEN, 0ULL};
    char at[CLI_PATH_MAX_LEN];
    char above[CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = at,
                            .src = args->folder,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->folder, .cap = CLI_CELLS_POOL}) + 1u});
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_CELLS_POOL});
    char up[CLI_PATH_MAX_LEN];
    VerbaLinea ups = {up, sizeof(up), 0ULL};
    for (;;)
    {
        char wrapper[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = at, .relative = "mvnw", .joined = wrapper);
        if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = wrapper))
        {
            mmgr_line_put_n(&line, up, ups.at);
            mmgr_line_put(&line, (ups.at == 0u) ? "./mvnw" : "mvnw");
            (void)mmgr_line_finish(&line);
            return;
        }
        const char after = at[length];
        const int inside = EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = at, .other_pointer = args->root,
                                      .size = length) &&
                           ((after == '\0') || (after == '/') || (after == '\\'));
        if ((cellul.eq(&(CatenaFinitaCfg){.src = at, .other = args->root, .cap = CLI_CELLS_POOL})) || !inside ||
            !EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = at, .out = above))
        {
            mmgr_line_put(&line, "mvn");
            (void)mmgr_line_finish(&line);
            return;
        }
        memor.cpy(&(MemoriaCfg){.dst = at,
                                .src = above,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = above, .cap = CLI_CELLS_POOL}) + 1u});
        mmgr_line_put(&ups, "../");
    }
}
