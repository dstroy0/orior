// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// maven_dependencies.c: the dependencies of a `<dependencies>` checked: each names its group and its artifact, is
// listed once by its coordinates, type and classifier, takes a scope Maven knows, and has a version or a management
// that gives it one, where the POM and its parents were all read and none of their managed dependencies imports a
// POM; a version that is a snapshot is noted with the fix that fetches its newest build. Each value has the properties
// it names given. More dependencies than CLI_MAVEN_LISTING in one list are not compared for being listed twice
#include "maven_internal.h"

// the most dependencies of one list compared for being listed twice, and the bytes they take
#define CLI_MAVEN_LISTING 4096u
#define CLI_MAVEN_LISTING_TEXT (1u << 20u)

static unsigned int s_seen_at[CLI_MAVEN_LISTING];
static unsigned int s_seen_size[CLI_MAVEN_LISTING];
static unsigned int s_seen;
static char s_seen_text[CLI_MAVEN_LISTING_TEXT];
static unsigned int s_seen_used;

typedef struct
{
    const CliMavenCheck *check;
    unsigned int of;
    const char *name;
    VerbaLinea *out;
    const char *whole;
    unsigned long long size;
    unsigned int dependency;
    const CliMavenModel *model;
    const char *key;
    int managing;
} MavenDependenciesCtx;
// the child `name` of `of`, with the properties it names given, into `out`: 1, or 0 where it has none
EMBED_INLINE int dependencies_get(const MavenDependenciesCtx *args)
{
    unsigned int of = args->of;
    const char *name = args->name;
    VerbaLinea *out = args->out;
    static char s_raw[1u << 14u];
    VerbaLinea raw = {s_raw, sizeof(s_raw), 0ULL};
    if (!EMBED_CALL(cli_maven_xml_out_internal.xml_text_get, CliMavenXmlOutCtx, .xml = args->check->xml, .of = of,
                    .name = name, .out = &raw) ||
        !mmgr_line_ok(&raw))
    {
        return 0;
    }
    EMBED_CALL(cli_maven_size_out_internal.resolve, CliMavenSizeOutCtx, .value = s_raw, .size = raw.at,
               .model_read = args->check->model, .out = out);
    return 1;
}

// whether `size` bytes of `whole` were listed above in this list, and recorded where they were not
EMBED_INLINE int dependencies_visit(const MavenDependenciesCtx *args)
{
    for (unsigned int at = 0u; at < s_seen; at += 1u)
    {
        if ((s_seen_size[at] == args->size) &&
            (memor.cmp(&(MemoriaCfg){
                 .src = s_seen_text + s_seen_at[at], .other = args->whole, .bytes = (size_t)args->size}) == 0))
        {
            return 1;
        }
    }
    if ((s_seen < CLI_MAVEN_LISTING) && (args->size <= CLI_MAVEN_LISTING_TEXT - s_seen_used))
    {
        memor.cpy(&(MemoriaCfg){.dst = s_seen_text + s_seen_used, .src = args->whole, .bytes = (size_t)args->size});
        s_seen_at[s_seen] = s_seen_used;
        s_seen_size[s_seen] = (unsigned int)args->size;
        s_seen += 1u;
        s_seen_used += (unsigned int)args->size;
    }
    return 0;
}

// the scope of a dependency checked against those Maven knows
EMBED_INLINE void dependencies_scope(const MavenDependenciesCtx *args)
{
    const CliXml *const xml = args->check->xml;
    const unsigned int scope = EMBED_CALL(cli_maven_xml_out_internal.xml_child, CliMavenXmlOutCtx, .xml = xml,
                                          .of = args->dependency, .name = "scope");
    static char s_value[1u << 12u];
    VerbaLinea value = {s_value, sizeof(s_value), 0ULL};
    unsigned long long found = 0ULL;
    if ((scope == CLI_XML_NONE) ||
        !EMBED_CALL(cli_maven_xml_out_internal.xml_text, CliMavenXmlOutCtx, .xml = xml, .of = scope, .out = &value) ||
        !mmgr_line_ok(&value) ||
        EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = s_value, .length_value = value.at, .required = "${",
                   .size = 2u, .at_pointer = &found))
    {
        return;
    }
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = g_cli_maven_scopes[at], .cap = CLI_POOL_BYTES}) == value.at) &&
            (memor.cmp(&(MemoriaCfg){.src = g_cli_maven_scopes[at], .other = s_value, .bytes = value.at}) == 0))
        {
            return;
        }
    }
    const CliXmlElement *const element = &xml->element[scope];
    VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = args->check->found);
    mmgr_line_ch(&msg, '`');
    mmgr_line_put_n(&msg, s_value, value.at);
    mmgr_line_put(&msg, "` is no scope: a scope is ");
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        mmgr_line_put(&msg, (at > 0u) ? ", `" : "`");
        mmgr_line_put(&msg, g_cli_maven_scopes[at]);
        mmgr_line_ch(&msg, '`');
    }
    const unsigned int finding =
        EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = args->check->found, .message = &msg,
                   .lines = args->check->lines, .from = element->inner_from, .to = element->inner_to,
                   .severity = CLI_BUILD_ERROR, .code = "pom-scope");
    CliBuildAdjacent adjacent;
    EMBED_CALL(cli_builds_size.build_adjacent_begin, CliBuildsSizeCfg, .adjacent = &adjacent, .name = s_value,
               .size = value.at);
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        EMBED_CALL(cli_builds_size_adjacent.build_adjacent_offer, CliBuildsSizeAdjacentCfg, .adjacent = &adjacent,
                   .one = g_cli_maven_scopes[at],
                   .size = cellul.len(&(CatenaFinitaCfg){.src = g_cli_maven_scopes[at], .cap = CLI_POOL_BYTES}));
    }
    for (unsigned int at = 0u; at < adjacent.count; at += 1u)
    {
        VerbaLinea title = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = args->check->found);
        mmgr_line_put(&title, "Change to ");
        mmgr_line_put_n(&title, adjacent.adjacent[at], adjacent.adjacent_size[at]);
        EMBED_CALL(cli_builds_text.build_edit_fix, CliBuildsTextCfg, .found = args->check->found, .finding = finding,
                   .title = &title, .lines = args->check->lines, .from = element->inner_from, .to = element->inner_to,
                   .text = adjacent.adjacent[at], .size = adjacent.adjacent_size[at]);
    }
}

// whether the model's management gives `size` bytes of `key` a version
EMBED_INLINE int dependencies_managed(const MavenDependenciesCtx *args)
{
    for (unsigned int at = 0u; at < args->model->managed; at += 1u)
    {
        if ((args->model->managed_size[at] == args->size) &&
            (memor.cmp(&(MemoriaCfg){.src = args->model->text + args->model->managed_at[at],
                                     .other = args->key,
                                     .bytes = (size_t)args->size}) == 0))
        {
            return 1;
        }
    }
    return 0;
}

// one dependency of the list checked
EMBED_INLINE void dependencies_one(const MavenDependenciesCtx *args)
{
    const CliMavenCheck *check = args->check;
    const CliXml *const xml = check->xml;
    const CliXmlElement *const element = &xml->element[args->dependency];
    static char s_key[1u << 14u];
    static char s_coordinates[1u << 14u];
    static char s_version[1u << 14u];
    VerbaLinea key = {s_key, sizeof(s_key), 0ULL};
    if (!EMBED_CALL(dependencies_get, MavenDependenciesCtx, .check = check, .of = args->dependency, .name = "groupId",
                    .out = &key))
    {
        VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = check->found);
        mmgr_line_put(&msg, "A dependency names its `<groupId>` and its `<artifactId>`");
        (void)EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = check->found, .message = &msg,
                         .lines = check->lines, .from = element->from, .to = element->to, .severity = CLI_BUILD_ERROR,
                         .code = "pom-dependency");
        return;
    }
    mmgr_line_ch(&key, ':');
    if (!EMBED_CALL(dependencies_get, MavenDependenciesCtx, .check = check, .of = args->dependency,
                    .name = "artifactId", .out = &key))
    {
        VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = check->found);
        mmgr_line_put(&msg, "A dependency names its `<groupId>` and its `<artifactId>`");
        (void)EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = check->found, .message = &msg,
                         .lines = check->lines, .from = element->from, .to = element->to, .severity = CLI_BUILD_ERROR,
                         .code = "pom-dependency");
        return;
    }
    VerbaLinea whole = {s_coordinates, sizeof(s_coordinates), 0ULL};
    mmgr_line_put_n(&whole, s_key, key.at);
    mmgr_line_ch(&whole, ':');
    if (!EMBED_CALL(dependencies_get, MavenDependenciesCtx, .check = check, .of = args->dependency, .name = "type",
                    .out = &whole))
    {
        mmgr_line_put(&whole, "jar");
    }
    mmgr_line_ch(&whole, ':');
    (void)EMBED_CALL(dependencies_get, MavenDependenciesCtx, .check = check, .of = args->dependency,
                     .name = "classifier", .out = &whole);
    if (EMBED_CALL(dependencies_visit, MavenDependenciesCtx, .whole = s_coordinates, .size = whole.at))
    {
        VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = check->found);
        mmgr_line_ch(&msg, '`');
        mmgr_line_put_n(&msg, s_key, key.at);
        mmgr_line_put(&msg, "` is listed above it in these dependencies");
        (void)EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = check->found, .message = &msg,
                         .lines = check->lines, .from = element->from, .to = element->to, .severity = CLI_BUILD_WARNING,
                         .code = "pom-duplicate");
    }
    EMBED_CALL(dependencies_scope, MavenDependenciesCtx, .check = check, .dependency = args->dependency);
    VerbaLinea version = {s_version, sizeof(s_version), 0ULL};
    if (!EMBED_CALL(dependencies_get, MavenDependenciesCtx, .check = check, .of = args->dependency, .name = "version",
                    .out = &version))
    {
        const CliMavenModel *const model = check->model;
        if (!args->managing && model->whole && !model->imports &&
            !EMBED_CALL(dependencies_managed, MavenDependenciesCtx, .model = model, .key = s_key, .size = key.at))
        {
            VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = check->found);
            mmgr_line_ch(&msg, '`');
            mmgr_line_put_n(&msg, s_key, key.at);
            mmgr_line_put(&msg, "` has no version, and no `<dependencyManagement>` gives it one");
            (void)EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = check->found, .message = &msg,
                             .lines = check->lines, .from = element->from, .to = element->to,
                             .severity = CLI_BUILD_WARNING, .code = "pom-version");
        }
        return;
    }
    if (!EMBED_CALL(cli_builds_size_version.build_snapshot, CliBuildsSizeVersionCfg, .version = s_version,
                    .size = version.at))
    {
        return;
    }
    const unsigned int child = EMBED_CALL(cli_maven_xml_out_internal.xml_child, CliMavenXmlOutCtx, .xml = xml,
                                          .of = args->dependency, .name = "version");
    const CliXmlElement *const at = &xml->element[(child != CLI_XML_NONE) ? child : args->dependency];
    VerbaLinea coordinates = {s_coordinates, sizeof(s_coordinates), 0ULL};
    mmgr_line_put_n(&coordinates, s_key, key.at);
    mmgr_line_ch(&coordinates, ':');
    mmgr_line_put_n(&coordinates, s_version, version.at);
    VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = check->found);
    mmgr_line_ch(&msg, '`');
    mmgr_line_put_n(&msg, s_coordinates, coordinates.at);
    mmgr_line_put(&msg, "` is a snapshot, whose newest build Maven fetches with `-U`");
    const unsigned int finding = EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = check->found,
                                            .message = &msg, .lines = check->lines, .from = at->inner_from,
                                            .to = at->inner_to, .severity = CLI_BUILD_NOTE, .code = "snapshot");
    EMBED_CALL(cli_maven_size_check_internal.update, CliMavenSizeCheckCtx, .check = check, .finding = finding,
               .coordinates = s_coordinates, .size = coordinates.at);
}

void cli_maven_dependencies(const CliMavenCheckCtx *args)
{
    const CliXml *const xml = args->check->xml;
    const unsigned int parent = xml->element[args->list].parent;
    const int managing =
        (parent != CLI_XML_NONE) && EMBED_CALL(cli_maven_xml_name_internal.xml_by_name, CliMavenXmlNameCtx, .xml = xml,
                                               .element = parent, .name = "dependencyManagement");
    s_seen = 0u;
    s_seen_used = 0u;
    for (unsigned int one = xml->element[args->list].first; one != CLI_XML_NONE; one = xml->element[one].next)
    {
        if (EMBED_CALL(cli_maven_xml_name_internal.xml_by_name, CliMavenXmlNameCtx, .xml = xml, .element = one,
                       .name = "dependency"))
        {
            EMBED_CALL(dependencies_one, MavenDependenciesCtx, .check = args->check, .dependency = one,
                       .managing = managing);
        }
    }
}
