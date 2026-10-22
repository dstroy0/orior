// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// maven_model.c: what a POM gives its children: its parents read first, by their relative path, `../pom.xml` where it
// gives none, and in the local repository where the path holds no POM of it, eight above it at most; then its own
// coordinates, its properties and its profiles', and the dependencies their management gives a version. The local
// repository the user's settings name, and the program that runs a build. One model is read at a time, under its lock;
// a POM larger than CLI_MAVEN_POM_TEXT is not read as a parent, and properties past what the model holds are left out
#include "../command_line/cells.h"
#include "maven_internal.h"

#include "../text/text.h"

// the most parents above a POM read, and the most bytes of each
#define CLI_MAVEN_DEPTH 8u
#define CLI_MAVEN_POM_TEXT (1ull << 20u)

static CliLock s_model_lock = CLI_LOCK_INIT;

// A POM read as a parent or as the user's settings: its bytes, mapped, and their size; the bytes NULL where it is not
// there or is larger than CLI_MAVEN_POM_TEXT
typedef struct
{
    const unsigned char *bytes;
    unsigned long long size;
} CliMavenMapped;

// the file at `path` mapped, where it is there and no larger than CLI_MAVEN_POM_TEXT
static CliMavenMapped model_mapped(const char *path)
{
    CliMavenMapped mapped = {NULL, 0ull};
    mapped.bytes = cli_file_held(path, &mapped.size);
    if ((mapped.bytes != NULL) && (mapped.size > CLI_MAVEN_POM_TEXT))
    {
        cli_file_let_go(mapped.bytes, mapped.size);
        mapped.bytes = NULL;
    }
    return mapped;
}

static void model_unmapped(CliMavenMapped mapped)
{
    if (mapped.bytes != NULL)
    {
        cli_file_let_go(mapped.bytes, mapped.size);
    }
}

const char *cli_maven_property(const CliMavenModel *model, const char *name, unsigned long long size,
                               unsigned long long *value_size)
{
    for (unsigned int at = 0u; at < model->properties; at += 1u)
    {
        if ((model->name_size[at] == size) &&
            (memor.cmp(&(MemoriaCfg){.src = model->text + model->name_at[at], .other = name, .bytes = (size_t)size}) ==
             0))
        {
            *value_size = model->value_size[at];
            return model->text + model->value_at[at];
        }
    }
    *value_size = 0ull;
    return NULL;
}

// a property set, in place of what it held where `replace`, or only where it holds none
static void model_set(CliMavenModel *model, const char *name, unsigned long long name_size, const char *value,
                      unsigned long long value_size, int replace)
{
    unsigned int at = 0u;
    while ((at < model->properties) &&
           !((model->name_size[at] == name_size) &&
             (memor.cmp(&(MemoriaCfg){
                  .src = model->text + model->name_at[at], .other = name, .bytes = (size_t)name_size}) == 0)))
    {
        at += 1u;
    }
    if ((at < model->properties) && !replace)
    {
        return;
    }
    if ((at == model->properties) && !cli_maven_property_room(model))
    {
        return;
    }
    const unsigned int value_at = cli_maven_model_kept(model, value, value_size);
    if (value_at == CLI_MAVEN_MODEL_TEXT)
    {
        return;
    }
    if (at == model->properties)
    {
        const unsigned int name_at = cli_maven_model_kept(model, name, name_size);
        if (name_at == CLI_MAVEN_MODEL_TEXT)
        {
            return;
        }
        model->name_at[at] = name_at;
        model->name_size[at] = (unsigned int)name_size;
        model->properties += 1u;
    }
    model->value_at[at] = value_at;
    model->value_size[at] = (unsigned int)value_size;
}

void cli_maven_resolve(const char *value, unsigned long long size, const CliMavenModel *model, CliLine *out)
{
    static char s_turn[2][1u << 16u];
    static CliLock s_resolve_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_resolve_lock);
    CliLine line = {s_turn[0], sizeof(s_turn[0]), 0ull};
    cli_line_counted(&line, value, size);
    unsigned int held = 0u;
    for (unsigned int round = 0u; (round < 8u) && cli_line_fits(&line); round += 1u)
    {
        unsigned long long start = 0ull;
        unsigned long long length = 0ull;
        if (!cli_text_find(s_turn[held], line.at, "${", 2u, &start) ||
            !cli_text_find(s_turn[held] + start, line.at - start, "}", 1u, &length))
        {
            break;
        }
        unsigned long long given_size = 0ull;
        const char *const given = cli_maven_property(model, s_turn[held] + start + 2u, length - 2u, &given_size);
        if (given == NULL)
        {
            break;
        }
        const unsigned int next = 1u - held;
        CliLine turned = {s_turn[next], sizeof(s_turn[next]), 0ull};
        cli_line_counted(&turned, s_turn[held], start);
        cli_line_counted(&turned, given, given_size);
        cli_line_counted(&turned, s_turn[held] + start + length + 1u, line.at - start - length - 1u);
        line = turned;
        held = next;
    }
    cli_line_counted(out, s_turn[held], line.at);
    cli_lock_give(&s_resolve_lock);
}

int cli_maven_given(const char *name, unsigned long long size)
{
    const char *const dot = memor.chr(&(MemoriaCfg){.src = name, .val = '.', .bytes = (size_t)size});
    const unsigned long long first = (dot != NULL) ? (unsigned long long)(dot - name) : size;
    for (unsigned int at = 0u; at < 16u; at += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = g_cli_maven_given_heads[at], .cap = CLI_CELLS_POOL}) == first) &&
            (memor.cmp(&(MemoriaCfg){.src = g_cli_maven_given_heads[at], .other = name, .bytes = (size_t)first}) == 0))
        {
            return 1;
        }
    }
    return (size == 4u) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "sha1", .bytes = 4u}) == 0);
}

// the reader's home folder into `out`
static int model_home(char *out)
{
    return query_environment_read("USERPROFILE", out, CLI_PATH_LONGEST) ||
           query_environment_read("HOME", out, CLI_PATH_LONGEST);
}

int cli_maven_user_settings(char *out)
{
    char home[CLI_PATH_LONGEST];
    char m2[CLI_PATH_LONGEST];
    if (!model_home(home))
    {
        return 0;
    }
    cli_full(home, ".m2", m2);
    cli_full(m2, "settings.xml", out);
    return 1;
}

int cli_maven_local_repository(char *out)
{
    // the settings mapped and read into a tree; settings larger than a POM is read as are read as settings that are not
    // there
    char path[CLI_PATH_LONGEST];
    if (cli_maven_user_settings(path))
    {
        const CliMavenMapped mapped = model_mapped(path);
        int named_one = 0;
        if (mapped.bytes != NULL)
        {
            CliXml settings;
            // the file's bytes are read as its text
            cli_xml_parse((const char *)mapped.bytes, mapped.size, &settings);
            CliLine named = {out, CLI_PATH_LONGEST, 0ull};
            const unsigned int root = cli_xml_root(&settings);
            named_one = (root != CLI_XML_NONE) && cli_xml_text_of(&settings, root, "localRepository", &named) &&
                        (named.at > 0u) && (cli_line_closed(&named) > 0u);
            cli_xml_given(&settings);
        }
        model_unmapped(mapped);
        if (named_one)
        {
            return 1;
        }
    }
    char home[CLI_PATH_LONGEST];
    char m2[CLI_PATH_LONGEST];
    if (!model_home(home))
    {
        return 0;
    }
    cli_full(home, ".m2", m2);
    cli_full(m2, "repository", out);
    return 1;
}

// a child's text of `of` in the xml at `depth` into a line over `room`: its size, or 0 where it has none
static int model_text_of(const CliXml *xml, unsigned int of, const char *name, char *room, unsigned long long size,
                         unsigned long long *length)
{
    CliLine line = {room, size, 0ull};
    const int found = cli_xml_text_of(xml, of, name, &line) && cli_line_fits(&line);
    *length = line.at;
    return found;
}

// the POM the `<parent>` at `parent` of the POM at `path` names, mapped into `mapped`: 1 with its path, or 0 where none
// is found
static int model_parent_pom(const char *path, const CliXml *xml, unsigned int parent, char *place,
                            CliMavenMapped *mapped)
{
    char folder[CLI_PATH_LONGEST];
    if (!cli_path_parent(path, folder))
    {
        return 0;
    }
    char relative[CLI_PATH_LONGEST];
    unsigned long long relative_size = 0ull;
    if (cli_xml_child(xml, parent, "relativePath") == CLI_XML_NONE)
    {
        relative_size = 10u;
        memor.cpy(&(MemoriaCfg){.dst = relative, .src = "../pom.xml", .bytes = 11u});
    }
    else if (!model_text_of(xml, parent, "relativePath", relative, sizeof(relative) - 1u, &relative_size))
    {
        relative_size = 0u;
    }
    relative[relative_size] = '\0';
    if (relative_size > 0u)
    {
        cli_full(folder, relative, place);
        if (query_is_folder(place))
        {
            char folder_place[CLI_PATH_LONGEST];
            memor.cpy(&(MemoriaCfg){.dst = folder_place, .src = place, .bytes = CLI_PATH_LONGEST});
            cli_full(folder_place, "pom.xml", place);
        }
        *mapped = model_mapped(place);
        if (mapped->bytes != NULL)
        {
            return 1;
        }
    }
    char group[1024];
    char artifact[1024];
    char version[1024];
    unsigned long long group_size = 0ull;
    unsigned long long artifact_size = 0ull;
    unsigned long long version_size = 0ull;
    char repository[CLI_PATH_LONGEST];
    if (!model_text_of(xml, parent, "groupId", group, sizeof(group), &group_size) ||
        !model_text_of(xml, parent, "artifactId", artifact, sizeof(artifact), &artifact_size) ||
        !model_text_of(xml, parent, "version", version, sizeof(version), &version_size) ||
        !cli_maven_local_repository(repository))
    {
        return 0;
    }
    CliLine line = {place, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, repository);
    cli_line_character(&line, '/');
    for (unsigned long long at = 0ull; at < group_size; at += 1u)
    {
        cli_line_character(&line, (group[at] == '.') ? '/' : group[at]);
    }
    cli_line_character(&line, '/');
    cli_line_counted(&line, artifact, artifact_size);
    cli_line_character(&line, '/');
    cli_line_counted(&line, version, version_size);
    cli_line_character(&line, '/');
    cli_line_counted(&line, artifact, artifact_size);
    cli_line_character(&line, '-');
    cli_line_counted(&line, version, version_size);
    cli_line_text(&line, ".pom");
    if (cli_line_closed(&line) == 0u)
    {
        return 0;
    }
    *mapped = model_mapped(place);
    return mapped->bytes != NULL;
}

// the properties and the managed dependencies of the holder at `holder` put in the model
static void model_holder(const CliXml *xml, unsigned int holder, CliMavenModel *model)
{
    static char s_value[1u << 16u];
    static char s_group[1u << 12u];
    static char s_artifact[1u << 12u];
    const unsigned int properties = cli_xml_child(xml, holder, "properties");
    for (unsigned int one = (properties != CLI_XML_NONE) ? xml->element[properties].first : CLI_XML_NONE;
         one != CLI_XML_NONE; one = xml->element[one].next)
    {
        CliLine value = {s_value, sizeof(s_value), 0ull};
        if (!cli_xml_text(xml, one, &value) || !cli_line_fits(&value))
        {
            value.at = 0ull;
        }
        const CliXmlElement *const element = &xml->element[one];
        model_set(model, xml->text + element->from, element->to - element->from, s_value, value.at, 1);
    }
    static const char *const s_path[2] = {"dependencyManagement", "dependencies"};
    const unsigned int dependencies = cli_xml_at_path(xml, holder, s_path, 2u);
    for (unsigned int one = (dependencies != CLI_XML_NONE) ? xml->element[dependencies].first : CLI_XML_NONE;
         one != CLI_XML_NONE; one = xml->element[one].next)
    {
        if (!cli_xml_named(xml, one, "dependency"))
        {
            continue;
        }
        unsigned long long group_size = 0ull;
        unsigned long long artifact_size = 0ull;
        if (model_text_of(xml, one, "groupId", s_group, sizeof(s_group), &group_size) &&
            model_text_of(xml, one, "artifactId", s_artifact, sizeof(s_artifact), &artifact_size) &&
            cli_maven_managed_room(model))
        {
            CliLine key = {s_value, sizeof(s_value), 0ull};
            cli_maven_resolve(s_group, group_size, model, &key);
            cli_line_character(&key, ':');
            cli_maven_resolve(s_artifact, artifact_size, model, &key);
            const unsigned int at =
                cli_line_fits(&key) ? cli_maven_model_kept(model, s_value, key.at) : CLI_MAVEN_MODEL_TEXT;
            if (at != CLI_MAVEN_MODEL_TEXT)
            {
                model->managed_at[model->managed] = at;
                model->managed_size[model->managed] = (unsigned int)key.at;
                model->managed += 1u;
            }
        }
        CliLine scope = {s_group, sizeof(s_group), 0ull};
        model->imports |= cli_xml_text_of(xml, one, "scope", &scope) && (scope.at == 6u) &&
                          (memor.cmp(&(MemoriaCfg){.src = s_group, .other = "import", .bytes = 6u}) == 0);
    }
}

// what the POM at `path` gives, its parents read up to `depth` above it, put in the model
static void model_read(const char *path, const char *text, unsigned long long size, unsigned int depth,
                       CliMavenModel *model)
{
    CliXml read;
    CliXml *const xml = &read;
    cli_xml_parse(text, size, xml);
    // a POM with no room left to read it in leaves the model not whole
    model->whole = model->whole && ((xml->element != NULL) || (size == 0ull));
    const unsigned int root = cli_xml_root(xml);
    if (root == CLI_XML_NONE)
    {
        cli_xml_given(xml);
        return;
    }
    const unsigned int parent = cli_xml_child(xml, root, "parent");
    static char s_value[1u << 16u];
    if (parent != CLI_XML_NONE)
    {
        char place[CLI_PATH_LONGEST];
        CliMavenMapped mapped = {NULL, 0ull};
        if ((depth < CLI_MAVEN_DEPTH) && model_parent_pom(path, xml, parent, place, &mapped))
        {
            // the parent's bytes are read as its text; what the model keeps of it is copied into the model's text
            model_read(place, (const char *)mapped.bytes, mapped.size, depth + 1u, model);
            model_unmapped(mapped);
        }
        else
        {
            model->whole = 0;
        }
        unsigned long long version = 0ull;
        if (model_text_of(xml, parent, "version", s_value, sizeof(s_value), &version))
        {
            model_set(model, "project.parent.version", 22u, s_value, version, 1);
            model_set(model, "project.version", 15u, s_value, version, 0);
        }
    }
    static const char *const s_names[3][2] = {
        {"project.groupId", "groupId"}, {"project.artifactId", "artifactId"}, {"project.version", "version"}};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        unsigned long long found = 0ull;
        if (model_text_of(xml, root, s_names[at][1], s_value, sizeof(s_value), &found))
        {
            model_set(model, s_names[at][0],
                      cellul.len(&(CatenaFinitaCfg){.src = s_names[at][0], .cap = CLI_CELLS_POOL}), s_value, found, 1);
        }
    }
    model_holder(xml, root, model);
    const unsigned int profiles = cli_xml_child(xml, root, "profiles");
    for (unsigned int one = (profiles != CLI_XML_NONE) ? xml->element[profiles].first : CLI_XML_NONE;
         one != CLI_XML_NONE; one = xml->element[one].next)
    {
        if (cli_xml_named(xml, one, "profile"))
        {
            model_holder(xml, one, model);
        }
    }
    cli_xml_given(xml);
}

void cli_maven_model_of(const char *path, const char *text, unsigned long long size, CliMavenModel *model)
{
    cli_lock_take(&s_model_lock);
    cli_maven_model_given(model);
    model->whole = 1;
    model_read(path, text, size, 0u, model);
    cli_lock_give(&s_model_lock);
}

void cli_maven_program(const char *root, const char *folder, char *out)
{
    CliLine line = {out, CLI_PATH_LONGEST, 0ull};
    char at[CLI_PATH_LONGEST];
    char above[CLI_PATH_LONGEST];
    memor.cpy(&(MemoriaCfg){
        .dst = at, .src = folder, .bytes = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}) + 1u});
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL});
    char up[CLI_PATH_LONGEST];
    CliLine ups = {up, sizeof(up), 0ull};
    for (;;)
    {
        char wrapper[CLI_PATH_LONGEST];
        cli_full(at, "mvnw", wrapper);
        if (query_is_file(wrapper))
        {
            cli_line_counted(&line, up, ups.at);
            cli_line_text(&line, (ups.at == 0u) ? "./mvnw" : "mvnw");
            (void)cli_line_closed(&line);
            return;
        }
        const char after = at[length];
        const int inside = cli_text_same(at, root, length) && ((after == '\0') || (after == '/') || (after == '\\'));
        if ((cellul.eq(&(CatenaFinitaCfg){.src = at, .other = root, .cap = CLI_CELLS_POOL})) || !inside ||
            !cli_path_parent(at, above))
        {
            cli_line_text(&line, "mvn");
            (void)cli_line_closed(&line);
            return;
        }
        memor.cpy(&(MemoriaCfg){.dst = at,
                                .src = above,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = above, .cap = CLI_CELLS_POOL}) + 1u});
        cli_line_text(&ups, "../");
    }
}
