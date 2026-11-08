// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// maven_complete.c: what completes the word before the cursor in a POM: after `${`, the properties the POM, its parents
// and Maven set; after `</`, the element left open; after `<`, the elements the open one may contain, each written with
// its closing tag where no `>` follows; and in an element's text, the values it takes, the local repository's groups,
// artifacts and versions among them. More folders of the repository than CLI_MAVEN_FOLDERS are not offered
#include "../command_line/callback_cfg.h"
#include "maven_internal.h"

#include "../text/text.h"
#include <stdlib.h>

// the most folders of the local repository offered, and the bytes their names take
#define CLI_MAVEN_FOLDERS 4096u
#define CLI_MAVEN_FOLDERS_TEXT (1u << 20u)

static CliXml s_xml;
static CliMavenModel s_model;

// the properties Maven gives that a POM is offered
static const char *const s_maven_input[6] = {
    "basedir", "maven.build.timestamp", "env.", "settings.localRepository", "java.version", "user.home"};

// whether a letter goes in an element's name, and in a word
static int complete_name_letter(const CliEachLetterCfg *args)
{
    unsigned int letter = args->letter;
    return EMBED_CALL(cli_maven_internal.xml_name_letter, CliMavenCtx, .letter = letter);
}

typedef struct
{
    const char *path;
    const char *text;
    unsigned long long size;
    const char *token;
    unsigned long long token_size;
    CliCompletions *items;
    unsigned long long end;
    int follows;
    const char *folder;
    const char *base;
    unsigned long long base_size;
    const char *repository;
    const char *group;
    char *out;
    unsigned long long tag_end;
    const char *name;
} MavenCompleteCtx;
// the properties a POM may name, put after `token`
EMBED_INLINE void complete_properties(const MavenCompleteCtx *args)
{
    const char *path = args->path;
    const char *text = args->text;
    unsigned long long size = args->size;
    const char *token = args->token;
    unsigned long long token_size = args->token_size;
    CliCompletions *items = args->items;
    EMBED_CALL(cli_maven_text_size_model_internal.model_get, CliMavenTextSizeModelCtx, .path = path, .text = text,
               .size = size, .model = &s_model);
    for (unsigned int at = 0u; at < s_model.properties; at += 1u)
    {
        EMBED_CALL(cli_builds_items.build_relative, CliBuildsItemsCfg, .items = items, .token = token,
                   .token_size = token_size, .value = s_model.text + s_model.name_at[at],
                   .value_size = s_model.name_size[at], .type = "constant",
                   .detail = s_model.text + s_model.value_at[at], .detail_size = s_model.value_size[at]);
    }
    g_cli_guard->persistent_buf_release(s_model.name_at, g_cli_maven_model_owner);
    *(&s_model) = (CliMavenModel){NULL, NULL, NULL, NULL, 0u, 0u, NULL, NULL, 0u, 0u, 0, 0, NULL, 0u, 0u};
    for (unsigned int at = 0u; at < 11u; at += 1u)
    {
        EMBED_CALL(cli_builds_items.build_relative, CliBuildsItemsCfg, .items = items, .token = token,
                   .token_size = token_size, .value = g_cli_maven_project_properties[at],
                   .value_size =
                       cellul.len(&(CatenaFinitaCfg){.src = g_cli_maven_project_properties[at], .cap = CLI_POOL_BYTES}),
                   .type = "constant", .detail = "Maven", .detail_size = 5u);
    }
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        EMBED_CALL(cli_builds_items.build_relative, CliBuildsItemsCfg, .items = items, .token = token,
                   .token_size = token_size, .value = s_maven_input[at],
                   .value_size = cellul.len(&(CatenaFinitaCfg){.src = s_maven_input[at], .cap = CLI_POOL_BYTES}),
                   .type = "constant", .detail = "Maven", .detail_size = 5u);
    }
}

// the elements the element left open before `end` may contain, put after `token`, each written with its closing tag
// where no `>` follows the cursor
EMBED_INLINE void complete_elements(const MavenCompleteCtx *args)
{
    const char *text = args->text;
    const char *token = args->token;
    unsigned long long token_size = args->token_size;
    CliCompletions *items = args->items;
    g_cli_guard->persistent_buf_release(s_xml.element, g_cli_xml_owner);
    *(&s_xml) = (CliXml){s_xml.text, 0ULL, NULL, 0u, 0u, NULL, NULL, 0u, 0u, NULL, 0u, 0u, NULL, 0u};
    EMBED_CALL(cli_maven_xml_text_internal.xml_parse, CliMavenXmlTextCtx, .text = text, .size = args->end,
               .xml_out = &s_xml);
    if (s_xml.opens == 0u)
    {
        EMBED_CALL(cli_builds_items.build_relative, CliBuildsItemsCfg, .items = items, .token = token,
                   .token_size = token_size, .value = "project", .value_size = 7u, .type = "keyword", .detail = "POM",
                   .detail_size = 3u);
        return;
    }
    const unsigned int open = s_xml.open[s_xml.opens - 1u];
    const CliXmlElement *const parent = &s_xml.element[open];
    const char *const name = text + parent->from;
    const unsigned long long name_size = parent->to - parent->from;
    const char *const *names = NULL;
    unsigned int count = 0u;
    static char s_detail[1u << 12u];
    VerbaLinea detail = {s_detail, sizeof(s_detail), 0ULL};
    if (EMBED_CALL(cli_maven_xml_name_internal.xml_by_name, CliMavenXmlNameCtx, .xml = &s_xml, .element = open,
                   .name = "properties"))
    {
        names = g_cli_maven_common_properties;
        count = 6u;
        mmgr_line_put(&detail, "property");
    }
    else if (EMBED_CALL(cli_maven_xml_at_internal.xml_inside_free, CliMavenXmlAtCtx, .xml = &s_xml, .at = open) ||
             EMBED_CALL(cli_maven_xml_name_internal.xml_by_name, CliMavenXmlNameCtx, .xml = &s_xml, .element = open,
                        .name = "configuration"))
    {
        return;
    }
    else
    {
        names = EMBED_CALL(cli_maven_size_name_internal.schema, CliMavenSizeNameCtx, .name = name, .size = name_size,
                           .count_pointer = &count);
        mmgr_line_put(&detail, "in <");
        mmgr_line_put_n(&detail, name, name_size);
        mmgr_line_ch(&detail, '>');
    }
    const unsigned long long base = EMBED_CALL(cli_builds_text_at.build_run_before, CliBuildsTextAtCfg, .text = token,
                                               .at = token_size, .of = cli_build_word_letter);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const unsigned int first = items->count;
        EMBED_CALL(cli_builds_items.build_relative, CliBuildsItemsCfg, .items = items, .token = token,
                   .token_size = token_size, .value = names[at],
                   .value_size = cellul.len(&(CatenaFinitaCfg){.src = names[at], .cap = CLI_POOL_BYTES}),
                   .type = "keyword", .detail = s_detail, .detail_size = detail.at);
        if ((items->count == first) || args->follows)
        {
            continue;
        }
        const CliCompletion *const item = &items->item[first];
        static char s_insert[1u << 13u];
        VerbaLinea insert = {s_insert, sizeof(s_insert), 0ULL};
        mmgr_line_put_n(&insert, items->text + item->insert_at, item->insert_size);
        mmgr_line_put(&insert, ">$0</");
        mmgr_line_put_n(&insert, token, base);
        mmgr_line_put_n(&insert, items->text + item->insert_at, item->insert_size);
        mmgr_line_ch(&insert, '>');
        const CliCompletion pivot = *item;
        items->count = first;
        (void)EMBED_CALL(cli_edits_items.completion_append, CliEditsItemsCfg, .items = items,
                         .label = items->text + pivot.label_at, .label_size = pivot.label_size, .type = pivot.type,
                         .detail = items->text + pivot.detail_at, .detail_size = pivot.detail_size, .insert = s_insert,
                         .insert_size = insert.at, .snippet = 1);
    }
}

// a folder of the local repository recorded by its name
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

// a folder of the repository recorded, its name after the base
static void complete_folder_visit(const CliEachNameIsFolderCfg *args)
{
    CliMavenFolders *const folders = args->context;
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES});
    if (!args->is_folder || (folders->count == CLI_MAVEN_FOLDERS) ||
        (folders->base_size + length + 1u > CLI_MAVEN_FOLDERS_TEXT - folders->used))
    {
        return;
    }
    char *const at = folders->text + folders->used;
    memor.cpy(&(MemoriaCfg){.dst = at, .src = folders->base, .bytes = (size_t)folders->base_size});
    memor.cpy(&(MemoriaCfg){.dst = at + folders->base_size, .src = args->name, .bytes = length});
    at[folders->base_size + length] = '\0';
    folders->at[folders->count] = folders->used;
    folders->size[folders->count] = (unsigned int)(folders->base_size + length);
    folders->count += 1u;
    folders->used += (unsigned int)(folders->base_size + length + 1u);
}

// two folders in the order of their names
static int complete_folder_order(const void *one, const void *other)
{
    return EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = s_folders.text + *(const unsigned int *)one,
                      .other_pointer = s_folders.text + *(const unsigned int *)other);
}

// the folders of `folder` in the local repository, each after `base`, put after `token`
EMBED_INLINE void complete_repository(const MavenCompleteCtx *args)
{
    const char *base = args->base;
    unsigned long long base_size = args->base_size;
    const char *token = args->token;
    unsigned long long token_size = args->token_size;
    CliCompletions *items = args->items;
    s_folders.count = 0u;
    s_folders.used = 0u;
    s_folders.base = base;
    s_folders.base_size = base_size;
    (void)EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = args->folder,
                     .each = complete_folder_visit, .context_bytess = &s_folders);
    qsort(s_folders.at, s_folders.count, sizeof(s_folders.at[0]), complete_folder_order);
    for (unsigned int at = 0u; at < s_folders.count; at += 1u)
    {
        const char *const value = s_folders.text + s_folders.at[at];
        EMBED_CALL(cli_builds_items.build_relative, CliBuildsItemsCfg, .items = items, .token = token,
                   .token_size = token_size, .value = value,
                   .value_size = cellul.len(&(CatenaFinitaCfg){.src = value, .cap = CLI_POOL_BYTES}), .type = "module",
                   .detail = "local repository", .detail_size = 16u);
    }
}

// `size` bytes of a group joined to `repository` with its dots as slashes into `out`
EMBED_INLINE void complete_group_path(const MavenCompleteCtx *args)
{
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea line = {path, sizeof(path), 0ULL};
    for (unsigned long long at = 0ULL; at < args->size; at += 1u)
    {
        mmgr_line_ch(&line, (args->group[at] == '.') ? '/' : args->group[at]);
    }
    (void)mmgr_line_finish(&line);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->repository, .relative = path, .joined = args->out);
}

// the groups, artifacts or versions of the local repository an element `name` of the POM takes, put after `token`
EMBED_INLINE void complete_coordinates(const MavenCompleteCtx *args)
{
    const char *text = args->text;
    const char *name = args->name;
    char repository[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_maven_out_internal.local_repository, CliMavenOutCtx, .out_capacity = repository))
    {
        return;
    }
    g_cli_guard->persistent_buf_release(s_xml.element, g_cli_xml_owner);
    *(&s_xml) = (CliXml){s_xml.text, 0ULL, NULL, 0u, 0u, NULL, NULL, 0u, 0u, NULL, 0u, 0u, NULL, 0u};
    EMBED_CALL(cli_maven_xml_text_internal.xml_parse, CliMavenXmlTextCtx, .text = text, .size = args->tag_end,
               .xml_out = &s_xml);
    const unsigned int open = (s_xml.opens > 0u) ? s_xml.open[s_xml.opens - 1u] : CLI_XML_NONE;
    static char s_group[1u << 12u];
    static char s_artifact[1u << 12u];
    VerbaLinea group = {s_group, sizeof(s_group), 0ULL};
    VerbaLinea artifact = {s_artifact, sizeof(s_artifact), 0ULL};
    const int has_group = EMBED_CALL(cli_maven_xml_out_internal.xml_text_get, CliMavenXmlOutCtx, .xml = &s_xml,
                                     .of = open, .name = "groupId", .out = &group) &&
                          mmgr_line_ok(&group);
    const int has_artifact = EMBED_CALL(cli_maven_xml_out_internal.xml_text_get, CliMavenXmlOutCtx, .xml = &s_xml,
                                        .of = open, .name = "artifactId", .out = &artifact) &&
                             mmgr_line_ok(&artifact);
    char folder[CLI_PATH_MAX_LEN];
    if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "groupId", .cap = CLI_POOL_BYTES}))
    {
        unsigned long long typed = args->token_size;
        typed = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->token, .at = typed,
                           .set = ".");
        EMBED_CALL(complete_group_path, MavenCompleteCtx, .repository = repository, .group = args->token, .size = typed,
                   .out = folder);
        EMBED_CALL(complete_repository, MavenCompleteCtx, .folder = folder, .base = args->token, .base_size = typed,
                   .token = args->token, .token_size = args->token_size, .items = args->items);
        return;
    }
    if (!has_group ||
        ((cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "version", .cap = CLI_POOL_BYTES})) && !has_artifact))
    {
        return;
    }
    EMBED_CALL(complete_group_path, MavenCompleteCtx, .repository = repository, .group = s_group, .size = group.at,
               .out = folder);
    if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "version", .cap = CLI_POOL_BYTES}))
    {
        char version_folder[CLI_PATH_MAX_LEN];
        (void)mmgr_line_finish(&artifact);
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = s_artifact,
                   .joined = version_folder);
        EMBED_CALL(complete_repository, MavenCompleteCtx, .folder = version_folder, .base = "", .base_size = 0ULL,
                   .token = args->token, .token_size = args->token_size, .items = args->items);
        return;
    }
    EMBED_CALL(complete_repository, MavenCompleteCtx, .folder = folder, .base = "", .base_size = 0ULL,
               .token = args->token, .token_size = args->token_size, .items = args->items);
}

// the values an element of `size` bytes of `name` takes, put after `token`: 1, or 0 where it takes none of these
EMBED_INLINE int complete_values(const MavenCompleteCtx *args)
{
    const char *name = args->name;
    const char *token = args->token;
    unsigned long long token_size = args->token_size;
    CliCompletions *items = args->items;
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
        if ((cellul.len(&(CatenaFinitaCfg){.src = s_lists[at].name, .cap = CLI_POOL_BYTES}) == args->size) &&
            (memor.cmp(&(MemoriaCfg){.src = s_lists[at].name, .other = name, .bytes = (size_t)args->size}) == 0))
        {
            list = s_lists[at].list;
            count = s_lists[at].count;
        }
        if ((cellul.len(&(CatenaFinitaCfg){.src = s_boolean_names[at], .cap = CLI_POOL_BYTES}) == args->size) &&
            (memor.cmp(&(MemoriaCfg){.src = s_boolean_names[at], .other = name, .bytes = (size_t)args->size}) == 0))
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
    VerbaLinea detail = {s_detail, sizeof(s_detail), 0ULL};
    mmgr_line_ch(&detail, '<');
    mmgr_line_put_n(&detail, name, args->size);
    mmgr_line_ch(&detail, '>');
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        EMBED_CALL(cli_builds_items.build_relative, CliBuildsItemsCfg, .items = items, .token = token,
                   .token_size = token_size, .value = list[at],
                   .value_size = cellul.len(&(CatenaFinitaCfg){.src = list[at], .cap = CLI_POOL_BYTES}),
                   .type = "value", .detail = s_detail, .detail_size = detail.at);
    }
    return 1;
}

void cli_maven_complete(const CliMavenTextSizeCtx *args)
{
    const unsigned long long token = EMBED_CALL(cli_builds_text_at.build_run_before, CliBuildsTextAtCfg,
                                                .text = args->text, .at = args->at_u64, .of = complete_name_letter);
    unsigned long long line_start = token;
    line_start = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->text,
                            .at = line_start, .set = "\n");
    const char *const lead = args->text + line_start;
    const unsigned long long comment_prefix_size = token - line_start;
    const char *const typed = args->text + token;
    const unsigned long long typed_size = args->at_u64 - token;
    if (EMBED_CALL(cli_text_size_suffix.ends, CliTextSizeSuffixCfg, .text = lead, .size = comment_prefix_size,
                   .suffix = "${"))
    {
        EMBED_CALL(complete_properties, MavenCompleteCtx, .path = args->path, .text = args->text, .size = args->size,
                   .token = typed, .token_size = typed_size, .items = args->items);
    }
    else if (EMBED_CALL(cli_text_size_suffix.ends, CliTextSizeSuffixCfg, .text = lead, .size = comment_prefix_size,
                        .suffix = "</"))
    {
        g_cli_guard->persistent_buf_release(s_xml.element, g_cli_xml_owner);
        *(&s_xml) = (CliXml){s_xml.text, 0ULL, NULL, 0u, 0u, NULL, NULL, 0u, 0u, NULL, 0u, 0u, NULL, 0u};
        EMBED_CALL(cli_maven_xml_text_internal.xml_parse, CliMavenXmlTextCtx, .text = args->text, .size = token - 2u,
                   .xml_out = &s_xml);
        if (s_xml.opens > 0u)
        {
            const CliXmlElement *const open = &s_xml.element[s_xml.open[s_xml.opens - 1u]];
            static char s_insert[1u << 12u];
            VerbaLinea insert = {s_insert, sizeof(s_insert), 0ULL};
            mmgr_line_put_n(&insert, args->text + open->from, open->to - open->from);
            mmgr_line_ch(&insert, '>');
            (void)EMBED_CALL(cli_edits_items.completion_append, CliEditsItemsCfg, .items = args->items,
                             .label = args->text + open->from, .label_size = open->to - open->from, .type = "keyword",
                             .detail = "close", .detail_size = 5u, .insert = s_insert, .insert_size = insert.at,
                             .snippet = 0);
        }
    }
    else if (EMBED_CALL(cli_text_size_suffix.ends, CliTextSizeSuffixCfg, .text = lead, .size = comment_prefix_size,
                        .suffix = "<"))
    {
        EMBED_CALL(complete_elements, MavenCompleteCtx, .text = args->text, .end = token - 1u, .token = typed,
                   .token_size = typed_size,
                   .follows = (args->at_u64 < args->size) && (args->text[args->at_u64] == '>'), .items = args->items);
    }
    else
    {
        unsigned long long tag = comment_prefix_size;
        tag =
            EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = lead, .at = tag, .set = "<");
        if ((tag > 0u) && (tag < comment_prefix_size) && (lead[tag] != '/') &&
            (memor.chr(&(MemoriaCfg){.src = lead + tag, .val = '>', .bytes = (size_t)(comment_prefix_size - tag)}) !=
             NULL))
        {
            unsigned long long name_end = tag;
            for (int lettered = 1; lettered && (name_end < comment_prefix_size);)
            {
                unsigned long long length = 0ULL;
                lettered =
                    EMBED_CALL(cli_maven_internal.xml_name_letter, CliMavenCtx,
                               .letter = EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = lead,
                                                    .size = comment_prefix_size, .at = name_end, .length = &length));
                name_end += lettered ? length : 0ULL;
            }
            static char s_name[1u << 12u];
            const unsigned long long name_size = (name_end - tag < sizeof(s_name)) ? name_end - tag : 0ULL;
            memor.cpy(&(MemoriaCfg){.dst = s_name, .src = lead + tag, .bytes = (size_t)name_size});
            s_name[name_size] = '\0';
            if (!EMBED_CALL(complete_values, MavenCompleteCtx, .name = s_name, .size = name_size, .token = typed,
                            .token_size = typed_size, .items = args->items) &&
                ((cellul.eq(&(CatenaFinitaCfg){.src = s_name, .other = "groupId", .cap = CLI_POOL_BYTES})) ||
                 (cellul.eq(&(CatenaFinitaCfg){.src = s_name, .other = "artifactId", .cap = CLI_POOL_BYTES})) ||
                 (cellul.eq(&(CatenaFinitaCfg){.src = s_name, .other = "version", .cap = CLI_POOL_BYTES}))))
            {
                EMBED_CALL(complete_coordinates, MavenCompleteCtx, .text = args->text, .tag_end = line_start + tag - 1u,
                           .name = s_name, .token = typed, .token_size = typed_size, .items = args->items);
            }
        }
    }
    g_cli_guard->persistent_buf_release(s_xml.element, g_cli_xml_owner);
    *(&s_xml) = (CliXml){s_xml.text, 0ULL, NULL, 0u, 0u, NULL, NULL, 0u, 0u, NULL, 0u, 0u, NULL, 0u};
}
