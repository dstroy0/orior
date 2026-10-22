// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// maven_complete.c: what completes the word before the cursor in a POM: after `${`, the properties the POM, its parents
// and Maven set; after `</`, the element left open; after `<`, the elements the open one may hold, each written with
// its closing tag where no `>` follows; and in an element's text, the values it takes, the local repository's groups,
// artifacts and versions among them. More folders of the repository than CLI_MAVEN_FOLDERS are not offered
#include "maven_internal.h"

#include "../text/text.h"
#include <stdlib.h>

// the most folders of the local repository offered, and the bytes their names take
#define CLI_MAVEN_FOLDERS 4096u
#define CLI_MAVEN_FOLDERS_TEXT (1u << 20u)

static CliXml s_xml;
static CliMavenModel s_model;
static CliLock s_complete_lock = CLI_LOCK_INIT;

// the properties Maven gives that a POM is offered
static const char *const s_maven_given[6] = {
    "basedir", "maven.build.timestamp", "env.", "settings.localRepository", "java.version", "user.home"};

// whether a letter goes in an element's name, and in a word
static int complete_name_letter(unsigned int letter)
{
    return cli_xml_name_letter(letter);
}

// the properties a POM may name, put after `token`
static void complete_properties(const char *path, const char *text, unsigned long long size, const char *token,
                                unsigned long long token_size, CliCompletions *items)
{
    cli_maven_model_of(path, text, size, &s_model);
    for (unsigned int at = 0u; at < s_model.properties; at += 1u)
    {
        cli_build_relative(items, token, token_size, s_model.text + s_model.name_at[at], s_model.name_size[at],
                           "constant", s_model.text + s_model.value_at[at], s_model.value_size[at]);
    }
    cli_maven_model_given(&s_model);
    for (unsigned int at = 0u; at < 11u; at += 1u)
    {
        cli_build_relative(
            items, token, token_size, g_cli_maven_project_properties[at],
            cellul.len(&(CatenaFinitaCfg){.src = g_cli_maven_project_properties[at], .cap = CLI_CELLS_POOL}),
            "constant", "Maven", 5u);
    }
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        cli_build_relative(items, token, token_size, s_maven_given[at],
                           cellul.len(&(CatenaFinitaCfg){.src = s_maven_given[at], .cap = CLI_CELLS_POOL}), "constant",
                           "Maven", 5u);
    }
}

// the elements the element left open before `end` may hold, put after `token`, each written with its closing tag
// where no `>` follows the cursor
static void complete_elements(const char *text, unsigned long long end, const char *token,
                              unsigned long long token_size, int follows, CliCompletions *items)
{
    cli_xml_given(&s_xml);
    cli_xml_parse(text, end, &s_xml);
    if (s_xml.opens == 0u)
    {
        cli_build_relative(items, token, token_size, "project", 7u, "keyword", "POM", 3u);
        return;
    }
    const unsigned int open = s_xml.open[s_xml.opens - 1u];
    const CliXmlElement *const parent = &s_xml.element[open];
    const char *const name = text + parent->from;
    const unsigned long long name_size = parent->to - parent->from;
    const char *const *names = NULL;
    unsigned int count = 0u;
    static char s_detail[1u << 12u];
    CliLine detail = {s_detail, sizeof(s_detail), 0ull};
    if (cli_xml_named(&s_xml, open, "properties"))
    {
        names = g_cli_maven_common_properties;
        count = 6u;
        cli_line_text(&detail, "property");
    }
    else if (cli_xml_inside_free(&s_xml, open) || cli_xml_named(&s_xml, open, "configuration"))
    {
        return;
    }
    else
    {
        names = cli_maven_schema(name, name_size, &count);
        cli_line_text(&detail, "in <");
        cli_line_counted(&detail, name, name_size);
        cli_line_character(&detail, '>');
    }
    const unsigned long long base = cli_build_run_before(token, token_size, cli_build_word_letter);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const unsigned int first = items->count;
        cli_build_relative(items, token, token_size, names[at],
                           cellul.len(&(CatenaFinitaCfg){.src = names[at], .cap = CLI_CELLS_POOL}), "keyword", s_detail,
                           detail.at);
        if ((items->count == first) || follows)
        {
            continue;
        }
        CliCompletion *const item = &items->item[first];
        static char s_insert[1u << 13u];
        CliLine insert = {s_insert, sizeof(s_insert), 0ull};
        cli_line_counted(&insert, items->text + item->insert_at, item->insert_size);
        cli_line_text(&insert, ">$0</");
        cli_line_counted(&insert, token, base);
        cli_line_counted(&insert, items->text + item->insert_at, item->insert_size);
        cli_line_character(&insert, '>');
        const CliCompletion kept = *item;
        items->count = first;
        (void)cli_completion_put(items, items->text + kept.label_at, kept.label_size, kept.kind,
                                 items->text + kept.detail_at, kept.detail_size, s_insert, insert.at, 1);
    }
}

// a folder of the local repository kept by its name
typedef struct
{
    unsigned int at[CLI_MAVEN_FOLDERS];
    unsigned int size[CLI_MAVEN_FOLDERS];
    unsigned int count;
    char text[CLI_MAVEN_FOLDERS_TEXT];
    unsigned int used;
    const char *base;
    unsigned long long base_size;
    const char *folder;
} CliMavenFolders;

static CliMavenFolders s_folders;

// a folder of the repository kept, its name after the base
static void complete_folder_seen(void *context, const char *name, int is_folder)
{
    CliMavenFolders *const folders = context;
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    if (!is_folder || (folders->count == CLI_MAVEN_FOLDERS) ||
        (folders->base_size + length + 1u > CLI_MAVEN_FOLDERS_TEXT - folders->used))
    {
        return;
    }
    char *const at = folders->text + folders->used;
    memor.cpy(&(MemoriaCfg){.dst = at, .src = folders->base, .bytes = (size_t)folders->base_size});
    memor.cpy(&(MemoriaCfg){.dst = at + folders->base_size, .src = name, .bytes = length});
    at[folders->base_size + length] = '\0';
    folders->at[folders->count] = folders->used;
    folders->size[folders->count] = (unsigned int)(folders->base_size + length);
    folders->count += 1u;
    folders->used += (unsigned int)(folders->base_size + length + 1u);
}

// two folders in the order of their names
static int complete_folder_order(const void *one, const void *other)
{
    return cli_text_order(s_folders.text + *(const unsigned int *)one, s_folders.text + *(const unsigned int *)other);
}

// the folders of `folder` in the local repository, each after `base`, put after `token`
static void complete_repository(const char *folder, const char *base, unsigned long long base_size, const char *token,
                                unsigned long long token_size, CliCompletions *items)
{
    s_folders.count = 0u;
    s_folders.used = 0u;
    s_folders.base = base;
    s_folders.base_size = base_size;
    (void)query_folder_each(folder, complete_folder_seen, &s_folders);
    qsort(s_folders.at, s_folders.count, sizeof(s_folders.at[0]), complete_folder_order);
    for (unsigned int at = 0u; at < s_folders.count; at += 1u)
    {
        const char *const value = s_folders.text + s_folders.at[at];
        cli_build_relative(items, token, token_size, value,
                           cellul.len(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL}), "module",
                           "local repository", 16u);
    }
}

// `size` bytes of a group joined to `repository` with its dots as slashes into `out`
static void complete_group_path(const char *repository, const char *group, unsigned long long size, char *out)
{
    char path[CLI_PATH_LONGEST];
    CliLine line = {path, sizeof(path), 0ull};
    for (unsigned long long at = 0ull; at < size; at += 1u)
    {
        cli_line_character(&line, (group[at] == '.') ? '/' : group[at]);
    }
    (void)cli_line_closed(&line);
    cli_full(repository, path, out);
}

// the groups, artifacts or versions of the local repository an element `name` of the POM takes, put after `token`
static void complete_coordinates(const char *text, unsigned long long tag_end, const char *name, const char *token,
                                 unsigned long long token_size, CliCompletions *items)
{
    char repository[CLI_PATH_LONGEST];
    if (!cli_maven_local_repository(repository))
    {
        return;
    }
    cli_xml_given(&s_xml);
    cli_xml_parse(text, tag_end, &s_xml);
    const unsigned int open = (s_xml.opens > 0u) ? s_xml.open[s_xml.opens - 1u] : CLI_XML_NONE;
    static char s_group[1u << 12u];
    static char s_artifact[1u << 12u];
    CliLine group = {s_group, sizeof(s_group), 0ull};
    CliLine artifact = {s_artifact, sizeof(s_artifact), 0ull};
    const int has_group = cli_xml_text_of(&s_xml, open, "groupId", &group) && cli_line_fits(&group);
    const int has_artifact = cli_xml_text_of(&s_xml, open, "artifactId", &artifact) && cli_line_fits(&artifact);
    char folder[CLI_PATH_LONGEST];
    if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "groupId", .cap = CLI_CELLS_POOL}))
    {
        unsigned long long typed = token_size;
        while ((typed > 0u) && (token[typed - 1u] != '.'))
        {
            typed -= 1u;
        }
        complete_group_path(repository, token, typed, folder);
        complete_repository(folder, token, typed, token, token_size, items);
        return;
    }
    if (!has_group ||
        ((cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "version", .cap = CLI_CELLS_POOL})) && !has_artifact))
    {
        return;
    }
    complete_group_path(repository, s_group, group.at, folder);
    if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "version", .cap = CLI_CELLS_POOL}))
    {
        char version_folder[CLI_PATH_LONGEST];
        (void)cli_line_closed(&artifact);
        cli_full(folder, s_artifact, version_folder);
        complete_repository(version_folder, "", 0ull, token, token_size, items);
        return;
    }
    complete_repository(folder, "", 0ull, token, token_size, items);
}

// the values an element of `size` bytes of `name` takes, put after `token`: 1, or 0 where it takes none of these
static int complete_values(const char *name, unsigned long long size, const char *token, unsigned long long token_size,
                           CliCompletions *items)
{
    static const char *const s_version[1] = {"4.0.0"};
    static const char *const s_update[4] = {"always", "daily", "never", "interval:"};
    static const char *const s_checksum[3] = {"fail", "warn", "ignore"};
    static const char *const s_layout[2] = {"default", "legacy"};
    static const char *const s_boolean_names[8] = {"optional", "activeByDefault", "filtering",       "inherited",
                                                   "enabled",  "uniqueVersion",   "excludeDefaults", "offline"};
    const char *const *list = NULL;
    unsigned int count = 0u;
    static const struct
    {
        const char *name;
        const char *const *list;
        unsigned int count;
    } s_lists[8] = {{"scope", g_cli_maven_scopes, 6u},         {"phase", g_cli_maven_phases, 29u},
                    {"packaging", g_cli_maven_packagings, 8u}, {"type", g_cli_maven_types, 7u},
                    {"modelVersion", s_version, 1u},           {"updatePolicy", s_update, 4u},
                    {"checksumPolicy", s_checksum, 3u},        {"layout", s_layout, 2u}};
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = s_lists[at].name, .cap = CLI_CELLS_POOL}) == size) &&
            (memor.cmp(&(MemoriaCfg){.src = s_lists[at].name, .other = name, .bytes = (size_t)size}) == 0))
        {
            list = s_lists[at].list;
            count = s_lists[at].count;
        }
        if ((cellul.len(&(CatenaFinitaCfg){.src = s_boolean_names[at], .cap = CLI_CELLS_POOL}) == size) &&
            (memor.cmp(&(MemoriaCfg){.src = s_boolean_names[at], .other = name, .bytes = (size_t)size}) == 0))
        {
            list = g_cli_maven_booleans;
            count = 2u;
        }
    }
    if (list == NULL)
    {
        return 0;
    }
    static char s_detail[1u << 12u];
    CliLine detail = {s_detail, sizeof(s_detail), 0ull};
    cli_line_character(&detail, '<');
    cli_line_counted(&detail, name, size);
    cli_line_character(&detail, '>');
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_build_relative(items, token, token_size, list[at],
                           cellul.len(&(CatenaFinitaCfg){.src = list[at], .cap = CLI_CELLS_POOL}), "value", s_detail,
                           detail.at);
    }
    return 1;
}

void cli_maven_complete(const char *path, const char *text, unsigned long long size, unsigned long long at,
                        CliCompletions *items)
{
    cli_lock_take(&s_complete_lock);
    const unsigned long long token = cli_build_run_before(text, at, complete_name_letter);
    unsigned long long line_start = token;
    while ((line_start > 0u) && (text[line_start - 1u] != '\n'))
    {
        line_start -= 1u;
    }
    const char *const lead = text + line_start;
    const unsigned long long lead_size = token - line_start;
    const char *const typed = text + token;
    const unsigned long long typed_size = at - token;
    if (cli_text_ends(lead, lead_size, "${"))
    {
        complete_properties(path, text, size, typed, typed_size, items);
    }
    else if (cli_text_ends(lead, lead_size, "</"))
    {
        cli_xml_given(&s_xml);
        cli_xml_parse(text, token - 2u, &s_xml);
        if (s_xml.opens > 0u)
        {
            const CliXmlElement *const open = &s_xml.element[s_xml.open[s_xml.opens - 1u]];
            static char s_insert[1u << 12u];
            CliLine insert = {s_insert, sizeof(s_insert), 0ull};
            cli_line_counted(&insert, text + open->from, open->to - open->from);
            cli_line_character(&insert, '>');
            (void)cli_completion_put(items, text + open->from, open->to - open->from, "keyword", "close", 5u, s_insert,
                                     insert.at, 0);
        }
    }
    else if (cli_text_ends(lead, lead_size, "<"))
    {
        complete_elements(text, token - 1u, typed, typed_size, (at < size) && (text[at] == '>'), items);
    }
    else
    {
        unsigned long long tag = lead_size;
        while ((tag > 0u) && (lead[tag - 1u] != '<'))
        {
            tag -= 1u;
        }
        if ((tag > 0u) && (tag < lead_size) && (lead[tag] != '/') &&
            (memor.chr(&(MemoriaCfg){.src = lead + tag, .val = '>', .bytes = (size_t)(lead_size - tag)}) != NULL))
        {
            unsigned long long name_end = tag;
            while (name_end < lead_size)
            {
                unsigned long long length = 0ull;
                if (!cli_xml_name_letter(cli_text_letter(lead, lead_size, name_end, &length)))
                {
                    break;
                }
                name_end += length;
            }
            static char s_name[1u << 12u];
            const unsigned long long name_size = (name_end - tag < sizeof(s_name)) ? name_end - tag : 0ull;
            memor.cpy(&(MemoriaCfg){.dst = s_name, .src = lead + tag, .bytes = (size_t)name_size});
            s_name[name_size] = '\0';
            if (!complete_values(s_name, name_size, typed, typed_size, items) &&
                ((cellul.eq(&(CatenaFinitaCfg){.src = s_name, .other = "groupId", .cap = CLI_CELLS_POOL})) ||
                 (cellul.eq(&(CatenaFinitaCfg){.src = s_name, .other = "artifactId", .cap = CLI_CELLS_POOL})) ||
                 (cellul.eq(&(CatenaFinitaCfg){.src = s_name, .other = "version", .cap = CLI_CELLS_POOL}))))
            {
                complete_coordinates(text, line_start + tag - 1u, s_name, typed, typed_size, items);
            }
        }
    }
    cli_xml_given(&s_xml);
    cli_lock_give(&s_complete_lock);
}
