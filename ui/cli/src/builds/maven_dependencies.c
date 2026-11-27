// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// maven_dependencies.c: the dependencies of a `<dependencies>` checked: each names its group and its artifact, is
// listed once by its coordinates, type and classifier, takes a scope Maven knows, and has a version or a management
// that gives it one, where the POM and its parents were all read and none of their managed dependencies imports a
// POM; a version that is a snapshot is noted with the fix that fetches its newest build. Each value has the properties
// it names given. The dependencies listed so far are held in cells of the call, grown as the list fills them
#include "maven_internal.h"

// A movable cell of the call and its bytes
typedef struct
{
    const CarcerMovableEntry *cell;
    unsigned long long bytes;
} CliMavenCell;

// Where one dependency's coordinates text is in the texts' cell, and its bytes
typedef struct
{
    unsigned long long at;
    unsigned long long size;
} CliMavenPlace;

// The dependencies listed above in one list, compared for being listed twice: their coordinates' texts in one cell
// and each one's place in another, both grown as the list fills them, the count of them and the texts' bytes used
typedef struct
{
    CliMavenCell texts;
    CliMavenCell places;
    unsigned int count;
    unsigned long long used;
} CliMavenSeen;

// the cells' first sizes, and the page a cell's records move by as it grows
#define CLI_MAVEN_SEEN_TEXTS_FIRST 4096ULL

#define CLI_MAVEN_SEEN_PLACES_FIRST (16ULL * sizeof(CliMavenPlace))

#define CLI_MAVEN_SEEN_PAGE 4096ULL

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
    CliMavenSeen *seen;
    CliMavenCell *store;
    unsigned long long needed;
    unsigned long long first;
} MavenDependenciesCtx;

// the cell of `store` grown to `needed` bytes where it is less, doubled from its size or begun at `first`, its bytes
// copied over a page at a time, the cell before given back: 1, or 0 where the cells cannot meet it
EMBED_INLINE int dependencies_cell_grown(const MavenDependenciesCtx *args)
{
    CliMavenCell *const store = args->store;
    if (args->needed <= store->bytes)
    {
        return 1;
    }
    const unsigned long long doubled = (store->bytes != 0ULL) ? (store->bytes * 2ULL) : args->first;
    const unsigned long long bytes = (doubled < args->needed) ? args->needed : doubled;
    const CarcerMovableEntry *const cell = g_platform_guard->movable_buf_alloc(bytes, g_cli_build_owner);
    if (cell == NULL)
    {
        return 0;
    }
    for (unsigned long long at = 0ULL; at < store->bytes; at += CLI_MAVEN_SEEN_PAGE)
    {
        char page[CLI_MAVEN_SEEN_PAGE];
        const unsigned long long piece =
            ((store->bytes - at) < CLI_MAVEN_SEEN_PAGE) ? (store->bytes - at) : CLI_MAVEN_SEEN_PAGE;
        (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = store->cell, .offset = (embed_word)at, .dst = page, .bytes = (embed_word)piece});
        (void)g_platform_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = cell, .offset = (embed_word)at, .src = page, .bytes = (embed_word)piece});
    }
    if (store->cell != NULL)
    {
        (void)g_platform_guard->movable_buf_release(store->cell, g_cli_build_owner);
    }
    store->cell = cell;
    store->bytes = bytes;
    return 1;
}
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

// whether `size` bytes of `whole` were listed above in this list, among `seen`, compared in the texts' cell; recorded
// where they were not, the cells grown to hold them
EMBED_INLINE int dependencies_visit(const MavenDependenciesCtx *args)
{
    CliMavenSeen *const seen = args->seen;
    for (unsigned int at = 0u; at < seen->count; at += 1u)
    {
        CliMavenPlace place;
        (void)g_platform_guard->movable_buf_read(&(CarcerMovableCfg){.cell = seen->places.cell,
                                                                     .offset = (embed_word)at * sizeof(CliMavenPlace),
                                                                     .dst = &place,
                                                                     .bytes = sizeof(place)});
        if ((place.size == args->size) &&
            (g_platform_guard->movable_buf_compare(&(CarcerMovableCfg){.cell = seen->texts.cell,
                                                                       .offset = (embed_word)place.at,
                                                                       .src = args->whole,
                                                                       .bytes = (embed_word)args->size}) == 0))
        {
            return 1;
        }
    }
    if (EMBED_CALL(dependencies_cell_grown, MavenDependenciesCtx, .store = &seen->places,
                   .needed = ((unsigned long long)seen->count + 1ULL) * sizeof(CliMavenPlace),
                   .first = CLI_MAVEN_SEEN_PLACES_FIRST) &&
        EMBED_CALL(dependencies_cell_grown, MavenDependenciesCtx, .store = &seen->texts,
                   .needed = seen->used + args->size, .first = CLI_MAVEN_SEEN_TEXTS_FIRST))
    {
        const CliMavenPlace place = {seen->used, args->size};
        (void)g_platform_guard->movable_buf_write(&(CarcerMovableCfg){.cell = seen->texts.cell,
                                                                      .offset = (embed_word)seen->used,
                                                                      .src = args->whole,
                                                                      .bytes = (embed_word)args->size});
        (void)g_platform_guard->movable_buf_write(&(CarcerMovableCfg){.cell = seen->places.cell,
                                                                      .offset = (embed_word)seen->count * sizeof(place),
                                                                      .src = &place,
                                                                      .bytes = sizeof(place)});
        seen->count += 1u;
        seen->used += args->size;
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
                                     .bytes = (embed_word)args->size}) == 0))
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
    if (EMBED_CALL(dependencies_visit, MavenDependenciesCtx, .seen = args->seen, .whole = s_coordinates,
                   .size = whole.at))
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
    // the dependencies listed so far, in cells of the call, given back as the list ends
    CliMavenSeen seen = {{NULL, 0ULL}, {NULL, 0ULL}, 0u, 0ULL};
    for (unsigned int one = xml->element[args->list].first; one != CLI_XML_NONE; one = xml->element[one].next)
    {
        if (EMBED_CALL(cli_maven_xml_name_internal.xml_by_name, CliMavenXmlNameCtx, .xml = xml, .element = one,
                       .name = "dependency"))
        {
            EMBED_CALL(dependencies_one, MavenDependenciesCtx, .check = args->check, .dependency = one,
                       .managing = managing, .seen = &seen);
        }
    }
    if (seen.texts.cell != NULL)
    {
        (void)g_platform_guard->movable_buf_release(seen.texts.cell, g_cli_build_owner);
    }
    if (seen.places.cell != NULL)
    {
        (void)g_platform_guard->movable_buf_release(seen.places.cell, g_cli_build_owner);
    }
}
